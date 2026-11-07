#include "game/LevelDirector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "game/EnemyBehaviors.hpp"

namespace game {
using namespace rtype::common::components;

namespace {

constexpr int FIRST_RANDOM_PICKUP = static_cast<int>(PickupType::ExtraLife);
constexpr int LAST_RANDOM_PICKUP = static_cast<int>(PickupType::Shield);

std::shared_ptr<const GameContent> orEmpty(std::shared_ptr<const GameContent> content) {
  return content ? std::move(content) : std::make_shared<const GameContent>();
}

bool trophyInPlay(const engine::ecs::Registry& registry) {
  const auto pickups = registry.view<Pickup>();
  return std::ranges::any_of(pickups, [&registry](engine::ecs::Entity entity) {
    return registry.getComponent<Pickup>(entity)->type == PickupType::Trophy;
  });
}

} // namespace

LevelDirector::LevelDirector(std::uint32_t seed, WorldBounds bounds, LevelCatalog catalog,
                             std::uint32_t initialLevel, std::shared_ptr<const GameContent> content,
                             Difficulty difficulty)
    : _random(seed), _bounds(bounds), _catalog(std::move(catalog)),
      _content(orEmpty(std::move(content))), _difficulty(difficulty),
      _settings(difficultySettings(difficulty)) {
  if (!std::isfinite(bounds.width) || !std::isfinite(bounds.height) || bounds.width <= 0.0F ||
      bounds.height <= 24.0F) {
    throw std::invalid_argument("level bounds must fit enemies");
  }
  beginLevel(initialLevel);
}

void LevelDirector::beginLevel(std::uint32_t number) {
  _level = _catalog.get(number);
  _progress = {};
  _progress.level = number;
  _progress.waveCount = static_cast<std::uint32_t>(_level.waves.size());
  beginWave();
  if (_level.introSeconds > 0.0F) {
    _progress.phase = LevelPhase::Intro;
    _progress.phaseRemainingSeconds = _level.introSeconds;
  }
}

void LevelDirector::beginWave() {
  _progress.spawned = 0;
  _progress.remainingEnemies = 0;
  _progress.bossWave = _level.waves[_progress.wave - 1].boss;
  _progress.phase = LevelPhase::Spawning;
  _progress.phaseRemainingSeconds = 0.0F;
  _timer = 0.0F;
  std::uniform_real_distribution<float> line(_bounds.height * 0.15F, _bounds.height * 0.75F);
  _lineY = line(_random);
}

void LevelDirector::loadLevel(engine::ecs::Registry& registry, std::uint32_t number) {
  // Resolve first: an invalid request must not clear the running world.
  const auto next = _catalog.get(number);
  (void)next;
  for (const auto enemy : registry.view<Enemy>()) {
    registry.killEntity(enemy);
  }
  for (const auto projectile : registry.view<Projectile>()) {
    registry.killEntity(projectile);
  }
  for (const auto pickup : registry.view<Pickup>()) {
    registry.killEntity(pickup);
  }
  beginLevel(number);
}

float LevelDirector::formationY(const WaveDefinition& wave, float height) {
  const float maxY = std::max(0.0F, _bounds.height - height);
  const auto index = static_cast<float>(_progress.spawned);
  switch (wave.formation) {
  case Formation::Line: return std::clamp(_lineY, 0.0F, maxY);
  case Formation::V: {
    // 0, -1, +1, -2, +2... rows around the centre.
    const float row = std::ceil(index / 2.0F) * (_progress.spawned % 2 == 0 ? 1.0F : -1.0F);
    return std::clamp(maxY / 2.0F + row * (height + 12.0F), 0.0F, maxY);
  }
  case Formation::Stairs: {
    const float steps = static_cast<float>(std::max(1U, wave.enemyCount - 1));
    return maxY * 0.1F + (index / steps) * maxY * 0.8F;
  }
  case Formation::Random: break;
  }
  std::uniform_real_distribution<float> anywhere(0.0F, maxY);
  return anywhere(_random);
}

void LevelDirector::spawn(engine::ecs::Registry& registry) {
  const auto& wave = _level.waves[_progress.wave - 1];
  engine::ecs::Entity spawned = engine::ecs::INVALID_ENTITY;
  bool dropsTrophy = false;
  if (wave.boss) {
    const auto index = _content->findBoss(wave.bossType).value_or(GENERIC_BOSS);
    spawned = spawnBoss(registry, *_content, index, wave.enemyHealth, _settings, _bounds);
    // The Cup only ends a finite campaign; in endless mode its boss drops a regular item.
    dropsTrophy = _content->boss(index).trophy && _catalog.finalLevel().has_value();
  } else if (const auto* type = _content->findEnemy(wave.enemy); type != nullptr) {
    const float y = formationY(wave, type->height);
    spawned = spawnEnemyOfType(registry, *type, _bounds.width, y, _settings,
                               wave.enemySpeedMultiplier, _random);
  } else {
    // Generic Bydos: same random placement as before content files existed.
    spawned = spawnRandomEnemy(registry, _random, _bounds);
    auto* velocity = registry.getComponent<Velocity>(spawned);
    velocity->vx *= wave.enemySpeedMultiplier * _settings.enemySpeed;
    const float scaled = static_cast<float>(wave.enemyHealth) * _settings.enemyHealth;
    const int health = std::max(1, static_cast<int>(std::lround(scaled)));
    *registry.getComponent<Health>(spawned) = {health, health};
    EnemyBehavior behavior;
    behavior.speed = -velocity->vx;
    behavior.fireRate = wave.fireChancePerSecond * _settings.enemyFireRate;
    behavior.shotSpeed = wave.shotSpeed * _settings.shotSpeed;
    registry.addComponent(spawned, behavior);
    registry.addComponent(spawned, Appearance{Visual::Bydos});
  }
  if (dropsTrophy) {
    registry.addComponent(spawned, LootDrop{PickupType::Trophy});
  } else {
    std::bernoulli_distribution drops(
        std::clamp(wave.pickupDropChance * _settings.dropChance, 0.0F, 1.0F));
    if (wave.boss || drops(_random)) {
      std::uniform_int_distribution<int> kind(FIRST_RANDOM_PICKUP, LAST_RANDOM_PICKUP);
      registry.addComponent(spawned, LootDrop{static_cast<PickupType>(kind(_random))});
    }
  }
  ++_progress.spawned;
}

void LevelDirector::finishWave(engine::ecs::Registry& registry) {
  const bool lastWave = _progress.wave == _progress.waveCount;
  const auto finalLevel = _catalog.finalLevel();
  if (lastWave && finalLevel && _progress.level >= *finalLevel) {
    _progress.phase = LevelPhase::Complete;
  } else {
    _progress.phase = lastWave ? LevelPhase::LevelPause : LevelPhase::WavePause;
  }
  _timer = 0.0F;
  // Clear stray shots between encounters without changing the players.
  for (const auto projectile : registry.view<Projectile>()) {
    registry.killEntity(projectile);
  }
}

void LevelDirector::update(engine::ecs::Registry& registry, float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0F) {
    throw std::invalid_argument("level step must be finite and positive");
  }
  refreshCounters(registry);
  const auto& wave = _level.waves[_progress.wave - 1];
  if (_progress.phase == LevelPhase::Intro) {
    _progress.phaseRemainingSeconds -= deltaSeconds;
    if (_progress.phaseRemainingSeconds <= 0.0F) {
      _progress.phase = LevelPhase::Spawning;
      _progress.phaseRemainingSeconds = 0.0F;
      _timer = 0.0F;
    }
  } else if (_progress.phase == LevelPhase::Spawning) {
    _timer += deltaSeconds;
    while (_progress.spawned < wave.enemyCount && _timer >= wave.spawnIntervalSeconds) {
      _timer -= wave.spawnIntervalSeconds;
      spawn(registry);
    }
    if (_progress.spawned == wave.enemyCount) {
      _progress.phase = LevelPhase::Clearing;
      _timer = 0.0F;
    }
  } else if (_progress.phase == LevelPhase::Clearing && _progress.remainingEnemies == 0 &&
             !trophyInPlay(registry)) {
    finishWave(registry);
  }
  if (_progress.phase == LevelPhase::WavePause || _progress.phase == LevelPhase::LevelPause) {
    _timer += deltaSeconds;
    const bool nextLevel = _progress.phase == LevelPhase::LevelPause;
    const float delay = nextLevel ? _level.nextLevelDelaySeconds : _level.interWaveDelaySeconds;
    _progress.phaseRemainingSeconds = std::max(0.0F, delay - _timer);
    if (_timer >= delay) {
      if (nextLevel) {
        if (_progress.level == std::numeric_limits<std::uint32_t>::max()) {
          _progress.phase = LevelPhase::Complete;
        } else {
          beginLevel(_progress.level + 1);
        }
      } else {
        ++_progress.wave;
        beginWave();
      }
    }
  }
  updateEnemyBehaviors(registry, deltaSeconds, _bounds, _random);
  updateBosses(registry, deltaSeconds, _bounds, *_content, _settings, _random);
  refreshCounters(registry);
}

void LevelDirector::refreshCounters(const engine::ecs::Registry& registry) {
  _progress.remainingEnemies = static_cast<std::uint32_t>(registry.view<Enemy>().size());
  _progress.bossHealth = 0;
  _progress.bossMaxHealth = 0;
  for (const auto boss : registry.view<BossBehavior, Health>()) {
    const auto* health = registry.getComponent<Health>(boss);
    _progress.bossHealth += std::max(0, health->current);
    _progress.bossMaxHealth += std::max(0, health->max);
  }
}

} // namespace game
