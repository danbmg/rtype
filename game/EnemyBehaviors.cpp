#include "game/EnemyBehaviors.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace game {

namespace {

using Entity = engine::ecs::Entity;
using namespace rtype::common::components;

constexpr float TWO_PI = 2.0F * std::numbers::pi_v<float>;
constexpr float BOSS_ENTRY_SPEED = 120.0F;
constexpr float BOSS_MARGIN = 24.0F;
constexpr float BOSS_SHOT_SIZE = 12.0F;
constexpr std::size_t MAX_ENEMIES_FOR_SUMMON = 24;
constexpr std::size_t MAX_PROJECTILES = 400;

struct Target {
  float x;
  float y;
};

std::vector<Target> playerCentres(const engine::ecs::Registry& registry) {
  std::vector<Target> targets;
  for (const auto player : registry.view<Player, Position>()) {
    const auto* position = registry.getComponent<Position>(player);
    const auto* hitbox = registry.getComponent<Hitbox>(player);
    const float width = hitbox != nullptr ? hitbox->width : 0.0F;
    const float height = hitbox != nullptr ? hitbox->height : 0.0F;
    targets.push_back({position->x + width / 2.0F, position->y + height / 2.0F});
  }
  return targets;
}

Target centreOf(const engine::ecs::Registry& registry, Entity entity) {
  const auto* position = registry.getComponent<Position>(entity);
  const auto* hitbox = registry.getComponent<Hitbox>(entity);
  return {position->x + (hitbox != nullptr ? hitbox->width / 2.0F : 0.0F),
          position->y + (hitbox != nullptr ? hitbox->height / 2.0F : 0.0F)};
}

bool tooManyProjectiles(const engine::ecs::Registry& registry) {
  return registry.view<Projectile>().size() >= MAX_PROJECTILES;
}

// One enemy shot leaving the shooter's centre at `angle` (0 = right, pi = left).
Entity fireAt(engine::ecs::Registry& registry, Entity shooter, float angle, float speed,
              bool bossShot) {
  const auto shot = fireProjectile(registry, shooter, false, Projectile::NO_OWNER,
                                   std::cos(angle) * speed, std::sin(angle) * speed);
  if (bossShot) {
    registry.addComponent(shot, Appearance{Visual::BossShot});
    *registry.getComponent<Hitbox>(shot) = {BOSS_SHOT_SIZE, BOSS_SHOT_SIZE};
    auto* position = registry.getComponent<Position>(shot);
    position->x -= BOSS_SHOT_SIZE / 2.0F;
    position->y -= BOSS_SHOT_SIZE / 2.0F;
  }
  return shot;
}

float angleTo(Target from, Target to) {
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  return dx == 0.0F && dy == 0.0F ? std::numbers::pi_v<float> : std::atan2(dy, dx);
}

void fireVolley(engine::ecs::Registry& registry, Entity shooter, const std::vector<Target>& targets,
                std::uint8_t count, float speed, bool bossShot, std::mt19937& random) {
  if (targets.empty() || count == 0) {
    return;
  }
  std::uniform_int_distribution<std::size_t> pick(0, targets.size() - 1);
  const float aim = angleTo(centreOf(registry, shooter), targets[pick(random)]);
  const float middle = static_cast<float>(count - 1) / 2.0F;
  for (std::uint8_t index = 0; index < count; ++index) {
    fireAt(registry, shooter, aim + (static_cast<float>(index) - middle) * 0.12F, speed, bossShot);
  }
}

void steer(EnemyBehavior& behavior, Velocity& velocity, const Position& position,
           const Hitbox& hitbox, WorldBounds bounds, const std::vector<Target>& targets) {
  velocity.vx = -behavior.speed;
  velocity.vy = 0.0F;
  const float maxY = std::max(0.0F, bounds.height - hitbox.height);
  switch (behavior.movement) {
  case MovementPattern::Straight:
  case MovementPattern::Turret: break;
  case MovementPattern::Sine: {
    // y(t) = baseY + A sin(2 pi f t): the velocity is its derivative, so the movement system
    // integrates the curve whatever the tick rate.
    const float omega = TWO_PI * behavior.frequency;
    velocity.vy = behavior.amplitude * omega * std::cos(omega * behavior.age);
    break;
  }
  case MovementPattern::Zigzag: {
    const auto leg = static_cast<long long>(behavior.age * std::max(behavior.frequency, 0.1F));
    const float direction = leg % 2 == 0 ? 1.0F : -1.0F;
    velocity.vy = direction * behavior.speed * 0.8F;
    if ((position.y <= 0.0F && velocity.vy < 0.0F) || (position.y >= maxY && velocity.vy > 0.0F)) {
      velocity.vy = -velocity.vy;
    }
    break;
  }
  case MovementPattern::Chase: {
    if (targets.empty()) {
      break;
    }
    const float centre = position.y + hitbox.height / 2.0F;
    const auto closest = std::ranges::min_element(targets, {}, [centre, &position](Target target) {
      return std::abs(target.y - centre) + std::abs(target.x - position.x) * 0.25F;
    });
    velocity.vy = std::clamp((closest->y - centre) * 3.0F, -behavior.speed, behavior.speed);
    break;
  }
  }
  // Never steer out of the screen vertically.
  if ((position.y <= 0.0F && velocity.vy < 0.0F) || (position.y >= maxY && velocity.vy > 0.0F)) {
    velocity.vy = 0.0F;
  }
}

void attack(engine::ecs::Registry& registry, Entity boss, const BossPhase& phase, BossAttack kind,
            const std::vector<Target>& targets, const GameContent& content,
            const DifficultySettings& difficulty, WorldBounds bounds, float age,
            std::mt19937& random) {
  const float speed = phase.shotSpeed * difficulty.shotSpeed;
  const auto bullets = phase.bullets;
  const float pi = std::numbers::pi_v<float>;
  switch (kind) {
  case BossAttack::Aimed:
    fireVolley(registry, boss, targets, std::min<std::uint8_t>(bullets, 7), speed, true, random);
    break;
  case BossAttack::Spread: {
    const float arc = 1.2F;
    for (std::uint8_t index = 0; index < bullets; ++index) {
      const float step =
          bullets > 1 ? static_cast<float>(index) / static_cast<float>(bullets - 1) : 0.5F;
      fireAt(registry, boss, pi - arc / 2.0F + step * arc, speed, true);
    }
    break;
  }
  case BossAttack::Ring:
    for (std::uint8_t index = 0; index < bullets; ++index) {
      const float angle =
          age * 0.9F + TWO_PI * static_cast<float>(index) / static_cast<float>(bullets);
      fireAt(registry, boss, angle, speed * 0.8F, true);
    }
    break;
  case BossAttack::Wall: {
    // A column of shots across the screen with a gap of three slots somewhere: fly through it.
    constexpr float SLOT = 40.0F;
    const auto slots = static_cast<int>(bounds.height / SLOT);
    if (slots < 4) {
      break;
    }
    std::uniform_int_distribution<int> gapStart(0, slots - 3);
    const int gap = gapStart(random);
    for (int slot = 0; slot < slots; ++slot) {
      if (slot >= gap && slot < gap + 3) {
        continue;
      }
      const auto shot = fireAt(registry, boss, pi, speed * 0.6F, true);
      registry.getComponent<Position>(shot)->y = static_cast<float>(slot) * SLOT + SLOT / 2.0F;
    }
    break;
  }
  case BossAttack::Summon: {
    const auto* type = content.findEnemy(phase.summon);
    if (type == nullptr || registry.view<Enemy>().size() >= MAX_ENEMIES_FOR_SUMMON) {
      break;
    }
    const auto* position = registry.getComponent<Position>(boss);
    const auto* hitbox = registry.getComponent<Hitbox>(boss);
    for (const float side : {-1.0F, 1.0F}) {
      const float offset = side * (hitbox->height / 2.0F + 20.0F);
      const float y = std::clamp(position->y + hitbox->height / 2.0F + offset, 0.0F,
                                 std::max(0.0F, bounds.height - type->height));
      spawnEnemyOfType(registry, *type, position->x - type->width, y, difficulty, 1.0F, random);
    }
    break;
  }
  }
}

} // namespace

engine::ecs::Entity spawnEnemyOfType(engine::ecs::Registry& registry, const EnemyType& type,
                                     float x, float y, const DifficultySettings& difficulty,
                                     float speedMultiplier, std::mt19937& random) {
  const int health = std::max(
      1, static_cast<int>(std::lround(static_cast<float>(type.health) * difficulty.enemyHealth)));
  const float speed = type.speed * speedMultiplier * difficulty.enemySpeed;
  const auto enemy = spawnEnemy(registry, x, y, -speed, 0.0F, health);
  *registry.getComponent<Hitbox>(enemy) = {type.width, type.height};
  *registry.getComponent<Enemy>(enemy) = {type.points, 0};
  registry.addComponent(enemy, Appearance{type.visual});

  EnemyBehavior behavior;
  behavior.movement = type.movement;
  behavior.speed = speed;
  behavior.amplitude = type.amplitude;
  behavior.frequency = type.frequency;
  behavior.fireRate = type.fireRate * difficulty.enemyFireRate;
  behavior.shotSpeed = type.shotSpeed * difficulty.shotSpeed;
  behavior.burst = type.burst;
  if (behavior.fireRate > 0.0F) {
    std::uniform_real_distribution<float> phase(0.0F, 1.0F / behavior.fireRate);
    behavior.fireTimer = phase(random);
  }
  registry.addComponent(enemy, behavior);
  return enemy;
}

engine::ecs::Entity spawnBoss(engine::ecs::Registry& registry, const GameContent& content,
                              std::uint16_t type, int health, const DifficultySettings& difficulty,
                              WorldBounds bounds) {
  const auto& definition = content.boss(type);
  const bool generic = type >= content.bosses.size();
  const float width = std::min(definition.width, bounds.width * 0.4F);
  const float height = std::min(definition.height, bounds.height * 0.5F);
  const int baseHealth = generic ? health : definition.health;
  const int scaledHealth = std::max(
      1, static_cast<int>(std::lround(static_cast<float>(baseHealth) * difficulty.enemyHealth)));
  const float y = (bounds.height - height) / 2.0F;
  const auto boss =
      spawnEnemy(registry, bounds.width + 8.0F, y, -BOSS_ENTRY_SPEED, 0.0F, scaledHealth);
  *registry.getComponent<Hitbox>(boss) = {width, height};
  *registry.getComponent<Enemy>(boss) = {definition.points, Enemy::BOSS_PATTERN};
  registry.addComponent(boss, Appearance{definition.visual});
  BossBehavior behavior;
  behavior.type = generic ? GENERIC_BOSS : type;
  behavior.attackTimer = 2.0F; // a breath while it flies in
  behavior.anchorX = std::max(0.0F, bounds.width - width - BOSS_MARGIN);
  behavior.baseY = y;
  registry.addComponent(boss, behavior);
  return boss;
}

void updateEnemyBehaviors(engine::ecs::Registry& registry, float deltaSeconds, WorldBounds bounds,
                          std::mt19937& random) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
    throw std::invalid_argument("behaviour delta must be finite and non-negative");
  }
  const auto targets = playerCentres(registry);
  for (const auto enemy : registry.view<EnemyBehavior, Position, Velocity, Hitbox>()) {
    auto& behavior = *registry.getComponent<EnemyBehavior>(enemy);
    const auto position = *registry.getComponent<Position>(enemy);
    const auto hitbox = *registry.getComponent<Hitbox>(enemy);
    behavior.age += deltaSeconds;
    behavior.fireTimer += deltaSeconds;
    steer(behavior, *registry.getComponent<Velocity>(enemy), position, hitbox, bounds, targets);

    if (behavior.fireRate <= 0.0F) {
      continue;
    }
    const float interval = 1.0F / behavior.fireRate;
    const bool onScreen = position.x + hitbox.width > 0.0F && position.x < bounds.width;
    if (!onScreen || targets.empty() || tooManyProjectiles(registry)) {
      // Ready to shoot as soon as it can: never stores more than one volley.
      behavior.fireTimer = std::min(behavior.fireTimer, interval);
      continue;
    }
    if (behavior.fireTimer >= interval) {
      behavior.fireTimer = std::min(behavior.fireTimer - interval, interval);
      fireVolley(registry, enemy, targets, behavior.burst, behavior.shotSpeed, false, random);
    }
  }
}

std::size_t bossPhaseFor(const BossType& boss, float healthRatio) {
  std::size_t phase = 0;
  for (std::size_t index = 0; index < boss.phases.size(); ++index) {
    if (healthRatio <= boss.phases[index].below) {
      phase = index;
    }
  }
  return phase;
}

void updateBosses(engine::ecs::Registry& registry, float deltaSeconds, WorldBounds bounds,
                  const GameContent& content, const DifficultySettings& difficulty,
                  std::mt19937& random) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
    throw std::invalid_argument("boss delta must be finite and non-negative");
  }
  const auto targets = playerCentres(registry);
  for (const auto boss : registry.view<BossBehavior, Position, Velocity, Hitbox, Health>()) {
    auto& behavior = *registry.getComponent<BossBehavior>(boss);
    auto& position = *registry.getComponent<Position>(boss);
    auto& velocity = *registry.getComponent<Velocity>(boss);
    const auto hitbox = *registry.getComponent<Hitbox>(boss);
    const auto health = *registry.getComponent<Health>(boss);
    const auto& type = content.boss(behavior.type);
    behavior.age += deltaSeconds;

    const float ratio =
        health.max > 0 ? static_cast<float>(health.current) / static_cast<float>(health.max) : 0.0F;
    const auto phaseIndex = static_cast<std::uint8_t>(bossPhaseFor(type, ratio));
    if (phaseIndex != behavior.phase) {
      behavior.phase = phaseIndex;
      behavior.nextAttack = 0;
      behavior.attackTimer = std::min(behavior.attackTimer, 0.8F);
    }
    const auto& phase = type.phases[behavior.phase];

    // Fly in from the right, then hold the anchor and sweep vertically.
    const float maxY = std::max(0.0F, bounds.height - hitbox.height);
    position.y = std::clamp(position.y, 0.0F, maxY);
    const bool entering = position.x > behavior.anchorX + 0.5F;
    if (entering) {
      velocity.vx = -BOSS_ENTRY_SPEED;
      velocity.vy = 0.0F;
    } else {
      position.x = std::min(position.x, behavior.anchorX);
      // The final boss also drifts forwards and back, which makes it harder to stay in front of.
      velocity.vx = type.visual == Visual::FinalBoss
                        ? 60.0F * std::sin(behavior.age * 0.8F) - (position.x - behavior.anchorX)
                        : behavior.anchorX - position.x;
      const float sweep = phase.moveSpeed * difficulty.enemySpeed;
      velocity.vy = velocity.vy < 0.0F ? -sweep : sweep;
      if (position.y + velocity.vy * deltaSeconds > maxY) {
        velocity.vy = -sweep;
      } else if (position.y + velocity.vy * deltaSeconds < 0.0F) {
        velocity.vy = sweep;
      }
      position.x = std::max(position.x, bounds.width * 0.4F);
    }

    behavior.attackTimer -= deltaSeconds;
    if (entering || behavior.attackTimer > 0.0F || tooManyProjectiles(registry)) {
      continue;
    }
    behavior.attackTimer = phase.interval / std::max(0.1F, difficulty.enemyFireRate);
    const auto kind = phase.attacks[behavior.nextAttack % phase.attacks.size()];
    behavior.nextAttack =
        static_cast<std::uint8_t>((behavior.nextAttack + 1) % phase.attacks.size());
    attack(registry, boss, phase, kind, targets, content, difficulty, bounds, behavior.age, random);
  }
}

} // namespace game
