#pragma once

#include <cstddef>
#include <random>

#include "common/components/Components.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/ecs/Registry.hpp"

namespace game {

struct WorldBounds {
  float width = 800.0F;
  float height = 600.0F;
};

void registerSystems(engine::ecs::Registry& registry);
void registerGameplaySystems(
    engine::core::SystemScheduler<engine::ecs::Registry>& scheduler, float deltaSeconds,
    WorldBounds bounds = {});
void applyPlayerInput(engine::ecs::Registry& registry, engine::ecs::Entity player, float horizontal,
                      float vertical, float speed);
void updateMovement(engine::ecs::Registry& registry, float deltaSeconds);
void updatePlayerMovement(engine::ecs::Registry& registry, float deltaSeconds = 1.0F);
void keepEntitiesOnScreen(engine::ecs::Registry& registry, WorldBounds bounds = {});

engine::ecs::Entity spawnEnemy(engine::ecs::Registry& registry, float x, float y,
                               float velocityX, float velocityY, int health = 1);
engine::ecs::Entity spawnRandomEnemy(engine::ecs::Registry& registry, std::mt19937& random,
                                     WorldBounds bounds = {});
engine::ecs::Entity fireProjectile(engine::ecs::Registry& registry, engine::ecs::Entity shooter,
                                   bool friendly, int ownerId, float velocityX, float velocityY,
                                   int damage = 1);
engine::ecs::Entity fireEnemyProjectile(engine::ecs::Registry& registry, engine::ecs::Entity enemy,
                                        engine::ecs::Entity target, float speed, int damage = 1);
std::size_t resolveProjectileCollisions(engine::ecs::Registry& registry);
std::size_t removeDeadEntities(engine::ecs::Registry& registry);

} // namespace game
