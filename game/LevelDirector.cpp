#include "game/LevelDirector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace game {
using namespace rtype::common::components;

LevelDirector::LevelDirector(std::uint32_t seed, WorldBounds bounds, LevelCatalog catalog,
                             std::uint32_t initialLevel)
    : _random(seed), _bounds(bounds), _catalog(std::move(catalog)) {
  if (!std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
      bounds.width <= 0.0F || bounds.height <= 24.0F) {
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
}

void LevelDirector::beginWave() {
  _progress.spawned = 0;
  _progress.remainingEnemies = 0;
  _progress.bossWave = _level.waves[_progress.wave - 1].boss;
  _progress.phase = LevelPhase::Spawning;
  _timer = 0.0F;
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

void LevelDirector::spawn(engine::ecs::Registry& registry) {
  const auto& wave = _level.waves[_progress.wave - 1];
  engine::ecs::Entity spawned;
  if (wave.boss) {
    const float width = std::min(96.0F, _bounds.width * 0.25F);
    const float height = std::min(72.0F, _bounds.height * 0.25F);
    const auto enemy = spawnEnemy(registry, std::max(0.0F, _bounds.width - width - 16.0F),
        (_bounds.height - height) / 2.0F, 0.0F, 65.0F, wave.enemyHealth);
    *registry.getComponent<Hitbox>(enemy) = {width, height};
    *registry.getComponent<Enemy>(enemy) = {1000, Enemy::BOSS_PATTERN};
    spawned = enemy;
  } else {
    const auto enemy = spawnRandomEnemy(registry, _random, _bounds);
    registry.getComponent<Velocity>(enemy)->vx *= wave.enemySpeedMultiplier;
    *registry.getComponent<Health>(enemy) = {wave.enemyHealth, wave.enemyHealth};
    spawned = enemy;
  }
  std::bernoulli_distribution drops(wave.pickupDropChance);
  if (wave.boss || drops(_random)) {
    std::uniform_int_distribution<int> kind(1, 6);
    registry.addComponent(spawned, LootDrop{static_cast<PickupType>(kind(_random))});
  }
  ++_progress.spawned;
}

void LevelDirector::update(engine::ecs::Registry& registry, float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0F) {
    throw std::invalid_argument("level step must be finite and positive");
  }
  _progress.remainingEnemies = static_cast<std::uint32_t>(registry.view<Enemy>().size());
  const auto& wave = _level.waves[_progress.wave - 1];
  if (_progress.phase == LevelPhase::Spawning) {
    _timer += deltaSeconds;
    while (_progress.spawned < wave.enemyCount && _timer >= wave.spawnIntervalSeconds) {
      _timer -= wave.spawnIntervalSeconds;
      spawn(registry);
    }
    if (_progress.spawned == wave.enemyCount) {
      _progress.phase = LevelPhase::Clearing;
      _timer = 0.0F;
    }
  } else if (_progress.phase == LevelPhase::Clearing && _progress.remainingEnemies == 0) {
    _progress.phase = _progress.wave == _progress.waveCount ? LevelPhase::LevelPause
                                                          : LevelPhase::WavePause;
    _timer = 0.0F;
    // Clear stray shots between encounters without changing the players.
    for (const auto projectile : registry.view<Projectile>()) {
      registry.killEntity(projectile);
    }
  }
  if (_progress.phase == LevelPhase::WavePause || _progress.phase == LevelPhase::LevelPause) {
    _timer += deltaSeconds;
    const bool nextLevel = _progress.phase == LevelPhase::LevelPause;
    const float delay = nextLevel ? _level.nextLevelDelaySeconds : _level.interWaveDelaySeconds;
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
  updateEnemies(registry, deltaSeconds);
  _progress.remainingEnemies = static_cast<std::uint32_t>(registry.view<Enemy>().size());
}

void LevelDirector::updateEnemies(engine::ecs::Registry& registry, float deltaSeconds) {
  const auto& wave = _level.waves[_progress.wave - 1];
  const auto players = registry.view<Player, Position>();
  for (const auto enemy : registry.view<Enemy, Position, Velocity, Hitbox>()) {
    const bool boss = registry.getComponent<Enemy>(enemy)->pattern == Enemy::BOSS_PATTERN;
    auto* position = registry.getComponent<Position>(enemy);
    auto* velocity = registry.getComponent<Velocity>(enemy);
    if (boss) {
      const float maxY = _bounds.height - registry.getComponent<Hitbox>(enemy)->height;
      position->y = std::clamp(position->y, 0.0F, maxY);
      if (position->y + velocity->vy * deltaSeconds > maxY) {
        velocity->vy = -std::abs(velocity->vy);
      } else if (position->y + velocity->vy * deltaSeconds < 0.0F) {
        velocity->vy = std::abs(velocity->vy);
      }
    }
    if (players.empty() || position->x >= _bounds.width) {
      continue;
    }
    std::bernoulli_distribution fires(std::min(1.0,
        static_cast<double>(wave.fireChancePerSecond) * deltaSeconds * (boss ? 4.0 : 1.0)));
    if (fires(_random)) {
      std::uniform_int_distribution<std::size_t> pickTarget(0, players.size() - 1);
      fireEnemyProjectile(registry, enemy, players[pickTarget(_random)], wave.shotSpeed);
    }
  }
}

} // namespace game
