#include "game/GameSystems.hpp"
#include "game/EquipmentSystems.hpp"
#include "game/LevelDirector.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace game {

namespace {

using Entity = engine::ecs::Entity;
using namespace rtype::common::components;

bool overlaps(const Position& lhsPosition, const Hitbox& lhsHitbox, const Position& rhsPosition,
              const Hitbox& rhsHitbox) {
  if (!std::isfinite(lhsPosition.x) || !std::isfinite(lhsPosition.y) ||
      !std::isfinite(rhsPosition.x) || !std::isfinite(rhsPosition.y) ||
      !std::isfinite(lhsHitbox.width) || !std::isfinite(lhsHitbox.height) ||
      !std::isfinite(rhsHitbox.width) || !std::isfinite(rhsHitbox.height) ||
      lhsHitbox.width <= 0.0F || lhsHitbox.height <= 0.0F || rhsHitbox.width <= 0.0F ||
      rhsHitbox.height <= 0.0F) {
    return false;
  }
  return lhsPosition.x < rhsPosition.x + rhsHitbox.width &&
         lhsPosition.x + lhsHitbox.width > rhsPosition.x &&
         lhsPosition.y < rhsPosition.y + rhsHitbox.height &&
         lhsPosition.y + lhsHitbox.height > rhsPosition.y;
}

// What protects a ship from one hit. Invulnerable (just respawned): the hit does not happen at
// all. Shield: the hit is absorbed and the shield is gone.
enum class Protection { None, Invulnerable, Shield };

Protection protectionOf(engine::ecs::Registry& registry, Entity ship) {
  const auto* equipment = registry.getComponent<Equipment>(ship);
  if (equipment == nullptr) {
    return Protection::None;
  }
  if (equipment->invulnerableSeconds > 0.0F) {
    return Protection::Invulnerable;
  }
  return equipment->shieldSeconds > 0.0F ? Protection::Shield : Protection::None;
}

void consumeShield(engine::ecs::Registry& registry, Entity ship) {
  if (auto* equipment = registry.getComponent<Equipment>(ship)) {
    equipment->shieldSeconds = 0.0F;
  }
}

bool overlaps(const engine::ecs::Registry& registry, Entity lhs, Entity rhs) {
  const auto* lhsPosition = registry.getComponent<Position>(lhs);
  const auto* lhsHitbox = registry.getComponent<Hitbox>(lhs);
  const auto* rhsPosition = registry.getComponent<Position>(rhs);
  const auto* rhsHitbox = registry.getComponent<Hitbox>(rhs);
  return lhsPosition != nullptr && lhsHitbox != nullptr && rhsPosition != nullptr &&
         rhsHitbox != nullptr && overlaps(*lhsPosition, *lhsHitbox, *rhsPosition, *rhsHitbox);
}

Player* findPlayerById(engine::ecs::Registry& registry, std::int32_t id) {
  if (id < 0) {
    return nullptr;
  }
  for (const auto entity : registry.view<Player>()) {
    auto* player = registry.getComponent<Player>(entity);
    if (player->id == static_cast<std::uint32_t>(id)) {
      return player;
    }
  }
  return nullptr;
}

void validateBounds(WorldBounds bounds) {
  if (!std::isfinite(bounds.width) || !std::isfinite(bounds.height) || bounds.width <= 0.0F ||
      bounds.height <= 0.0F) {
    throw std::invalid_argument("world bounds must be finite and positive");
  }
}

} // namespace

void registerSystems(engine::ecs::Registry& registry) {
  registry.registerComponent<Position>();
  registry.registerComponent<Velocity>();
  registry.registerComponent<Sprite>();
  registry.registerComponent<Health>();
  registry.registerComponent<Hitbox>();
  registry.registerComponent<Enemy>();
  registry.registerComponent<Projectile>();
  registry.registerComponent<Player>();
  registry.registerComponent<Equipment>();
  registry.registerComponent<Pickup>();
  registry.registerComponent<LootDrop>();
  registry.registerComponent<Appearance>();
  registry.registerComponent<EnemyBehavior>();
  registry.registerComponent<BossBehavior>();
}

void registerGameplaySystems(engine::core::SystemScheduler<engine::ecs::Registry>& scheduler,
                             float deltaSeconds, WorldBounds bounds) {
  scheduler.addSystem("equipment-timers", [deltaSeconds](engine::ecs::Registry& registry) {
    updateEquipment(registry, deltaSeconds);
  });
  scheduler.addSystem("movement", [deltaSeconds](engine::ecs::Registry& registry) {
    updateMovement(registry, deltaSeconds);
  });
  scheduler.addSystem("player-bounds", [bounds](engine::ecs::Registry& registry) {
    keepEntitiesOnScreen(registry, bounds);
  });
  scheduler.addSystem("offscreen-cleanup", [bounds](engine::ecs::Registry& registry) {
    removeOffscreenEntities(registry, bounds);
  });
  scheduler.addSystem("projectile-collisions", [](engine::ecs::Registry& registry) {
    resolveProjectileCollisions(registry);
  });
  scheduler.addSystem("ship-collisions",
                      [](engine::ecs::Registry& registry) { resolveShipCollisions(registry); });
  scheduler.addSystem("pickup-collisions", [](engine::ecs::Registry& registry) {
    collectPickups(registry);
  });
  scheduler.addSystem("player-deaths",
                      [](engine::ecs::Registry& registry) { handlePlayerDeaths(registry); });
  scheduler.addSystem("dead-entity-cleanup",
                      [](engine::ecs::Registry& registry) { removeDeadEntities(registry); });
}

void registerEnemySystems(engine::core::SystemScheduler<engine::ecs::Registry>& scheduler,
                          float deltaSeconds, std::uint32_t seed, WorldBounds bounds,
                          EnemyWaveSettings settings) {
  validateBounds(bounds);
  if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0F ||
      !std::isfinite(settings.spawnIntervalSeconds) || settings.spawnIntervalSeconds <= 0.0F ||
      !std::isfinite(settings.fireChancePerSecond) || settings.fireChancePerSecond < 0.0F ||
      !std::isfinite(settings.shotSpeed) || settings.shotSpeed <= 0.0F ||
      !std::isfinite(settings.interWaveDelaySeconds) || settings.interWaveDelaySeconds < 0.0F ||
      settings.enemiesPerWave == 0 || settings.bossEveryWaves == 0) {
    throw std::invalid_argument("enemy wave timing, fire chance, and shot speed must be valid");
  }

  LevelCatalog catalog;
  LevelDefinition level;
  for (std::uint32_t index = 0; index < 5; ++index) {
    WaveDefinition wave;
    wave.boss = (index + 1) % settings.bossEveryWaves == 0;
    wave.enemyCount = wave.boss ? 1U : static_cast<std::uint32_t>(
        std::min<std::uint64_t>(50, settings.enemiesPerWave +
            static_cast<std::uint64_t>(index) * settings.extraEnemiesPerWave));
    wave.enemyHealth = wave.boss ? 30 : 1;
    wave.spawnIntervalSeconds = settings.spawnIntervalSeconds;
    wave.fireChancePerSecond = settings.fireChancePerSecond;
    wave.shotSpeed = settings.shotSpeed;
    level.waves.push_back(wave);
  }
  level.interWaveDelaySeconds = settings.interWaveDelaySeconds;
  catalog.define(std::move(level));
  auto director = std::make_shared<LevelDirector>(seed, bounds, std::move(catalog));
  scheduler.addSystem("level-director", [director, deltaSeconds](engine::ecs::Registry& registry) {
    director->update(registry, deltaSeconds);
  });
}

void applyPlayerInput(engine::ecs::Registry& registry, engine::ecs::Entity player, float horizontal,
                      float vertical, float speed) {
  auto* velocity = registry.getComponent<Velocity>(player);
  if (registry.getComponent<Player>(player) == nullptr || velocity == nullptr ||
      !std::isfinite(horizontal) || !std::isfinite(vertical) || !std::isfinite(speed) ||
      speed < 0.0F) {
    throw std::invalid_argument(
        "player input and speed must be finite and the entity must be a player");
  }

  horizontal = std::clamp(horizontal, -1.0F, 1.0F);
  vertical = std::clamp(vertical, -1.0F, 1.0F);
  const float magnitude = std::hypot(horizontal, vertical);
  if (magnitude > 1.0F) {
    horizontal /= magnitude;
    vertical /= magnitude;
  }

  velocity->vx = horizontal * speed;
  velocity->vy = vertical * speed;
}

void updateMovement(engine::ecs::Registry& registry, float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
    throw std::invalid_argument("movement delta must be finite and non-negative");
  }

  const auto movingEntities = registry.view<Position, Velocity>();
  for (const auto entity : movingEntities) {
    auto* position = registry.getComponent<Position>(entity);
    const auto* velocity = registry.getComponent<Velocity>(entity);
    if (position != nullptr && velocity != nullptr) {
      position->x += velocity->vx * deltaSeconds;
      position->y += velocity->vy * deltaSeconds;
    }
  }
}

void updatePlayerMovement(engine::ecs::Registry& registry, float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
    throw std::invalid_argument("movement delta must be finite and non-negative");
  }

  const auto players = registry.view<Player, Position, Velocity>();
  for (const auto entity : players) {
    auto* position = registry.getComponent<Position>(entity);
    const auto* velocity = registry.getComponent<Velocity>(entity);
    if (position != nullptr && velocity != nullptr) {
      position->x += velocity->vx * deltaSeconds;
      position->y += velocity->vy * deltaSeconds;
    }
  }
}

void keepEntitiesOnScreen(engine::ecs::Registry& registry, WorldBounds bounds) {
  if (!std::isfinite(bounds.width) || !std::isfinite(bounds.height) || bounds.width < 0.0F ||
      bounds.height < 0.0F) {
    throw std::invalid_argument("world bounds must be finite and non-negative");
  }

  const auto players = registry.view<Player, Position>();
  for (const auto entity : players) {
    auto* position = registry.getComponent<Position>(entity);
    const auto* hitbox = registry.getComponent<Hitbox>(entity);
    if (position == nullptr || !std::isfinite(position->x) || !std::isfinite(position->y)) {
      continue;
    }

    const float width = hitbox != nullptr && std::isfinite(hitbox->width) && hitbox->width > 0.0F
                            ? hitbox->width
                            : 0.0F;
    const float height = hitbox != nullptr && std::isfinite(hitbox->height) && hitbox->height > 0.0F
                             ? hitbox->height
                             : 0.0F;
    const float maxX = std::max(0.0F, bounds.width - width);
    const float maxY = std::max(0.0F, bounds.height - height);
    position->x = std::clamp(position->x, 0.0F, maxX);
    position->y = std::clamp(position->y, 0.0F, maxY);
  }
}

engine::ecs::Entity spawnEnemy(engine::ecs::Registry& registry, float x, float y, float velocityX,
                               float velocityY, int health) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(velocityX) ||
      !std::isfinite(velocityY) || health <= 0) {
    throw std::invalid_argument("enemy position, velocity, and health must be valid");
  }

  const auto enemy = registry.spawnEntity();
  registry.addComponent(enemy, Position{x, y});
  registry.addComponent(enemy, Velocity{velocityX, velocityY});
  registry.addComponent(enemy, Enemy{100, 0});
  registry.addComponent(enemy, Health{health, health});
  registry.addComponent(enemy, Hitbox{32.0F, 24.0F});
  return enemy;
}

engine::ecs::Entity spawnRandomEnemy(engine::ecs::Registry& registry, std::mt19937& random,
                                     WorldBounds bounds) {
  if (!std::isfinite(bounds.width) || !std::isfinite(bounds.height) || bounds.width <= 0.0F ||
      bounds.height <= 24.0F) {
    throw std::invalid_argument("enemy spawn bounds are too small or not finite");
  }

  std::uniform_real_distribution<float> yDistribution(0.0F, bounds.height - 24.0F);
  std::uniform_real_distribution<float> speedDistribution(40.0F, 100.0F);
  return spawnEnemy(registry, bounds.width, yDistribution(random), -speedDistribution(random),
                    0.0F);
}

engine::ecs::Entity fireProjectile(engine::ecs::Registry& registry, engine::ecs::Entity shooter,
                                   bool friendly, std::int32_t ownerId, float velocityX,
                                   float velocityY, int damage) {
  const auto* position = registry.getComponent<Position>(shooter);
  if (position == nullptr || !std::isfinite(velocityX) || !std::isfinite(velocityY) ||
      damage <= 0) {
    throw std::invalid_argument("projectile shooter, velocity, and damage must be valid");
  }

  if (!std::isfinite(position->x) || !std::isfinite(position->y) || (friendly && ownerId < 0)) {
    throw std::invalid_argument("projectile shooter position or owner id is invalid");
  }

  float spawnX = position->x;
  float spawnY = position->y;
  if (const auto* hitbox = registry.getComponent<Hitbox>(shooter); hitbox != nullptr) {
    if (!std::isfinite(hitbox->width) || !std::isfinite(hitbox->height) || hitbox->width < 0.0F ||
        hitbox->height < 0.0F) {
      throw std::invalid_argument("projectile shooter hitbox must be finite and non-negative");
    }
    spawnX += hitbox->width / 2.0F;
    spawnY += hitbox->height / 2.0F;
  }

  const auto projectile = registry.spawnEntity();
  registry.addComponent(projectile, Position{spawnX, spawnY});
  registry.addComponent(projectile, Velocity{velocityX, velocityY});
  registry.addComponent(projectile, Projectile{damage, friendly, ownerId});
  registry.addComponent(projectile, Hitbox{8.0F, 4.0F});
  return projectile;
}

engine::ecs::Entity fireEnemyProjectile(engine::ecs::Registry& registry, engine::ecs::Entity enemy,
                                        engine::ecs::Entity target, float speed, int damage) {
  const auto* enemyPosition = registry.getComponent<Position>(enemy);
  const auto* targetPosition = registry.getComponent<Position>(target);
  if (registry.getComponent<Enemy>(enemy) == nullptr ||
      registry.getComponent<Player>(target) == nullptr || enemyPosition == nullptr ||
      targetPosition == nullptr || !std::isfinite(speed) || speed <= 0.0F || damage <= 0) {
    throw std::invalid_argument(
        "enemy fire requires an enemy, player, and positive speed and damage");
  }

  float startX = enemyPosition->x;
  float startY = enemyPosition->y;
  float targetX = targetPosition->x;
  float targetY = targetPosition->y;
  if (const auto* hitbox = registry.getComponent<Hitbox>(enemy); hitbox != nullptr) {
    startX += hitbox->width / 2.0F;
    startY += hitbox->height / 2.0F;
  }
  if (const auto* hitbox = registry.getComponent<Hitbox>(target); hitbox != nullptr) {
    targetX += hitbox->width / 2.0F;
    targetY += hitbox->height / 2.0F;
  }
  if (!std::isfinite(startX) || !std::isfinite(startY) || !std::isfinite(targetX) ||
      !std::isfinite(targetY)) {
    throw std::invalid_argument("enemy or target position must be finite");
  }

  float directionX = targetX - startX;
  float directionY = targetY - startY;
  const float distance = std::hypot(directionX, directionY);
  if (distance == 0.0F) {
    directionX = -1.0F;
    directionY = 0.0F;
  } else {
    directionX /= distance;
    directionY /= distance;
  }
  return fireProjectile(registry, enemy, false, Projectile::NO_OWNER, directionX * speed,
                        directionY * speed, damage);
}

engine::ecs::Entity firePlayerProjectile(engine::ecs::Registry& registry,
                                         engine::ecs::Entity player, float speed, int damage) {
  const auto* shooter = registry.getComponent<Player>(player);
  if (shooter == nullptr || !std::isfinite(speed) || speed <= 0.0F) {
    throw std::invalid_argument("player fire requires a player and a positive speed");
  }
  return fireProjectile(registry, player, true, static_cast<std::int32_t>(shooter->id), speed, 0.0F,
                        damage);
}

std::size_t resolveProjectileCollisions(engine::ecs::Registry& registry) {
  const auto projectiles = registry.view<Position, Hitbox, Projectile>();
  const auto targets = registry.view<Position, Hitbox, Health>();
  std::vector<Entity> projectilesToRemove;
  std::size_t collisions = 0;

  for (const auto projectileEntity : projectiles) {
    const auto* projectile = registry.getComponent<Projectile>(projectileEntity);
    const auto* projectilePosition = registry.getComponent<Position>(projectileEntity);
    const auto* projectileHitbox = registry.getComponent<Hitbox>(projectileEntity);
    if (projectile == nullptr || projectilePosition == nullptr || projectileHitbox == nullptr) {
      continue;
    }

    for (const auto targetEntity : targets) {
      if (targetEntity == projectileEntity) {
        continue;
      }

      const bool isEnemy = registry.getComponent<Enemy>(targetEntity) != nullptr;
      const bool isPlayer = registry.getComponent<Player>(targetEntity) != nullptr;
      if ((projectile->friendly && !isEnemy) || (!projectile->friendly && !isPlayer)) {
        continue;
      }

      if (projectile->friendly) {
        const auto* owner = registry.getComponent<Player>(targetEntity);
        if (owner != nullptr && projectile->ownerId >= 0 &&
            owner->id == static_cast<std::uint32_t>(projectile->ownerId)) {
          continue;
        }
      }

      const auto* targetPosition = registry.getComponent<Position>(targetEntity);
      const auto* targetHitbox = registry.getComponent<Hitbox>(targetEntity);
      auto* targetHealth = registry.getComponent<Health>(targetEntity);
      if (targetPosition == nullptr || targetHitbox == nullptr || targetHealth == nullptr ||
          targetHealth->current <= 0 ||
          !overlaps(*projectilePosition, *projectileHitbox, *targetPosition, *targetHitbox)) {
        continue;
      }

      if (isPlayer) {
        const auto protection = protectionOf(registry, targetEntity);
        if (protection == Protection::Invulnerable) {
          continue; // the shot flies through a ship that just respawned
        }
        if (protection == Protection::Shield) {
          consumeShield(registry, targetEntity);
          projectilesToRemove.push_back(projectileEntity);
          ++collisions;
          break;
        }
      }
      targetHealth->current = std::max(0, targetHealth->current - projectile->damage);
      if (projectile->friendly && targetHealth->current == 0) {
        auto* shooter = findPlayerById(registry, projectile->ownerId);
        if (shooter != nullptr) {
          shooter->score += static_cast<std::uint32_t>(
              std::max(0, registry.getComponent<Enemy>(targetEntity)->points));
        }
      }
      projectilesToRemove.push_back(projectileEntity);
      ++collisions;
      break;
    }
  }

  std::sort(projectilesToRemove.begin(), projectilesToRemove.end());
  projectilesToRemove.erase(std::unique(projectilesToRemove.begin(), projectilesToRemove.end()),
                            projectilesToRemove.end());
  for (const auto projectile : projectilesToRemove) {
    registry.killEntity(projectile);
  }
  return collisions;
}

std::size_t resolveShipCollisions(engine::ecs::Registry& registry) {
  std::size_t collisions = 0;
  for (const auto player : registry.view<Player, Position, Hitbox, Health>()) {
    auto* playerHealth = registry.getComponent<Health>(player);
    for (const auto enemy : registry.view<Enemy, Position, Hitbox, Health>()) {
      auto* enemyHealth = registry.getComponent<Health>(enemy);
      if (playerHealth->current <= 0 || enemyHealth->current <= 0 ||
          !overlaps(registry, player, enemy)) {
        continue;
      }
      // Ramming destroys the Bydos and costs the player one hit point, without awarding points.
      // A shield takes the hit instead; a ship that just respawned takes nothing.
      const auto protection = protectionOf(registry, player);
      if (protection == Protection::Shield) {
        consumeShield(registry, player);
      } else if (protection == Protection::None) {
        playerHealth->current -= 1;
      }
      if (registry.getComponent<Enemy>(enemy)->pattern == Enemy::BOSS_PATTERN) {
        enemyHealth->current -= 1;
        // Push the ship away so contact cannot consume every life in consecutive ticks.
        registry.getComponent<Position>(player)->x = std::max(
            0.0F, registry.getComponent<Position>(enemy)->x -
                      registry.getComponent<Hitbox>(player)->width - 16.0F);
      } else {
        enemyHealth->current = 0;
      }
      ++collisions;
    }
  }
  return collisions;
}

std::size_t handlePlayerDeaths(engine::ecs::Registry& registry) {
  std::size_t livesLost = 0;
  for (const auto entity : registry.view<Player, Health>()) {
    auto* player = registry.getComponent<Player>(entity);
    auto* health = registry.getComponent<Health>(entity);
    if (health->current > 0 || player->lives <= 0) {
      continue;
    }
    auto* equipment = registry.getComponent<Equipment>(entity);
    if (equipment != nullptr) {
      *equipment = Equipment{};
    }
    --player->lives;
    ++livesLost;
    // With lives left the ship respawns in place, briefly invulnerable so that the shots
    // already on their way cannot take a second life; on its last life health stays at zero and
    // removeDeadEntities destroys it.
    if (player->lives > 0) {
      health->current = health->max;
      if (equipment != nullptr) {
        equipment->invulnerableSeconds = RESPAWN_INVULNERABLE_SECONDS;
      }
    }
  }
  return livesLost;
}

std::size_t removeOffscreenEntities(engine::ecs::Registry& registry, WorldBounds bounds,
                                    float margin) {
  validateBounds(bounds);
  if (!std::isfinite(margin) || margin < 0.0F) {
    throw std::invalid_argument("offscreen margin must be finite and non-negative");
  }

  std::vector<Entity> offscreen;
  for (const auto entity : registry.view<Position>()) {
    if (registry.getComponent<Player>(entity) != nullptr) {
      continue;
    }
    const auto* position = registry.getComponent<Position>(entity);
    // A non-finite position can never come back on screen either.
    if (!std::isfinite(position->x) || !std::isfinite(position->y) || position->x < -margin ||
        position->x > bounds.width + margin || position->y < -margin ||
        position->y > bounds.height + margin) {
      offscreen.push_back(entity);
    }
  }
  for (const auto entity : offscreen) {
    registry.killEntity(entity);
  }
  return offscreen.size();
}

std::size_t removeDeadEntities(engine::ecs::Registry& registry) {
  const auto entities = registry.view<Health>();
  std::vector<Entity> deadEntities;
  for (const auto entity : entities) {
    const auto* health = registry.getComponent<Health>(entity);
    if (health != nullptr && health->current <= 0) {
      deadEntities.push_back(entity);
    }
  }
  for (const auto entity : deadEntities) {
    if (registry.getComponent<Enemy>(entity) != nullptr) {
      const auto* drop = registry.getComponent<LootDrop>(entity);
      const auto* position = registry.getComponent<Position>(entity);
      if (drop != nullptr && position != nullptr) {
        // Copy before spawning: adding components may reallocate their storage.
        const auto type = drop->type;
        auto location = *position;
        if (type == PickupType::Trophy) {
          // The Cup appears in the middle of the boss wreck.
          if (const auto* hitbox = registry.getComponent<Hitbox>(entity)) {
            location.x += (hitbox->width - TROPHY_SIZE) / 2.0F;
            location.y += (hitbox->height - TROPHY_SIZE) / 2.0F;
          }
        }
        spawnPickup(registry, type, location.x, location.y);
      }
    }
    registry.killEntity(entity);
  }
  return deadEntities.size();
}

} // namespace game
