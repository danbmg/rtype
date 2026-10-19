#include "game/GameSystems.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
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
}

void registerGameplaySystems(
    engine::core::SystemScheduler<engine::ecs::Registry>& scheduler, float deltaSeconds,
    WorldBounds bounds) {
  scheduler.addSystem("movement", [deltaSeconds](engine::ecs::Registry& registry) {
    updateMovement(registry, deltaSeconds);
  });
  scheduler.addSystem("player-bounds", [bounds](engine::ecs::Registry& registry) {
    keepEntitiesOnScreen(registry, bounds);
  });
  scheduler.addSystem("projectile-collisions", [](engine::ecs::Registry& registry) {
    resolveProjectileCollisions(registry);
  });
  scheduler.addSystem("dead-entity-cleanup", [](engine::ecs::Registry& registry) {
    removeDeadEntities(registry);
  });
}

void applyPlayerInput(engine::ecs::Registry& registry, engine::ecs::Entity player, float horizontal,
                      float vertical, float speed) {
  auto* velocity = registry.getComponent<Velocity>(player);
  if (registry.getComponent<Player>(player) == nullptr || velocity == nullptr ||
      !std::isfinite(horizontal) || !std::isfinite(vertical) || !std::isfinite(speed) || speed < 0.0F) {
    throw std::invalid_argument("player input and speed must be finite and the entity must be a player");
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
  return spawnEnemy(registry, bounds.width, yDistribution(random), -speedDistribution(random), 0.0F);
}

engine::ecs::Entity fireProjectile(engine::ecs::Registry& registry, engine::ecs::Entity shooter,
                                   bool friendly, int ownerId, float velocityX, float velocityY,
                                   int damage) {
  const auto* position = registry.getComponent<Position>(shooter);
  if (position == nullptr || !std::isfinite(velocityX) || !std::isfinite(velocityY) || damage <= 0) {
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
  if (registry.getComponent<Enemy>(enemy) == nullptr || registry.getComponent<Player>(target) == nullptr ||
      enemyPosition == nullptr || targetPosition == nullptr || !std::isfinite(speed) || speed <= 0.0F ||
      damage <= 0) {
    throw std::invalid_argument("enemy fire requires an enemy, player, and positive speed and damage");
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
  return fireProjectile(registry, enemy, false, -1, directionX * speed, directionY * speed, damage);
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
            owner->id == static_cast<std::size_t>(projectile->ownerId)) {
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

      targetHealth->current = std::max(0, targetHealth->current - projectile->damage);
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
    registry.killEntity(entity);
  }
  return deadEntities.size();
}

} // namespace game
