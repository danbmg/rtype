#pragma once

#include <cstddef>
#include <cstdint>
#include <random>

#include "common/components/Components.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/ecs/Registry.hpp"

namespace game {

struct WorldBounds {
  float width = 800.0F;
  float height = 600.0F;
};

// Finite waves: clear all enemies before the inter-wave pause starts.
struct EnemyWaveSettings {
  float spawnIntervalSeconds = 1.2F;
  float fireChancePerSecond = 0.25F;
  float shotSpeed = 220.0F;
  float interWaveDelaySeconds = 3.0F;
  std::uint32_t enemiesPerWave = 5;
  std::uint32_t extraEnemiesPerWave = 2;
  std::uint32_t bossEveryWaves = 5;
};

// Distance an entity may travel past the screen edge before it is destroyed.
constexpr float OFFSCREEN_MARGIN = 64.0F;
// A ship that loses a life ignores enemy fire and contact for this long.
constexpr float RESPAWN_INVULNERABLE_SECONDS = 2.0F;

void registerSystems(engine::ecs::Registry& registry);
void registerGameplaySystems(engine::core::SystemScheduler<engine::ecs::Registry>& scheduler,
                             float deltaSeconds, WorldBounds bounds = {});
void registerEnemySystems(engine::core::SystemScheduler<engine::ecs::Registry>& scheduler,
                          float deltaSeconds, std::uint32_t seed, WorldBounds bounds = {},
                          EnemyWaveSettings settings = {});
void applyPlayerInput(engine::ecs::Registry& registry, engine::ecs::Entity player, float horizontal,
                      float vertical, float speed);
void updateMovement(engine::ecs::Registry& registry, float deltaSeconds);
void updatePlayerMovement(engine::ecs::Registry& registry, float deltaSeconds = 1.0F);
void keepEntitiesOnScreen(engine::ecs::Registry& registry, WorldBounds bounds = {});

engine::ecs::Entity spawnEnemy(engine::ecs::Registry& registry, float x, float y, float velocityX,
                               float velocityY, int health = 1);
engine::ecs::Entity spawnRandomEnemy(engine::ecs::Registry& registry, std::mt19937& random,
                                     WorldBounds bounds = {});
engine::ecs::Entity fireProjectile(engine::ecs::Registry& registry, engine::ecs::Entity shooter,
                                   bool friendly, std::int32_t ownerId, float velocityX,
                                   float velocityY, int damage = 1);
engine::ecs::Entity firePlayerProjectile(engine::ecs::Registry& registry,
                                         engine::ecs::Entity player, float speed, int damage = 1);
engine::ecs::Entity fireEnemyProjectile(engine::ecs::Registry& registry, engine::ecs::Entity enemy,
                                        engine::ecs::Entity target, float speed, int damage = 1);
std::size_t resolveProjectileCollisions(engine::ecs::Registry& registry);
std::size_t resolveShipCollisions(engine::ecs::Registry& registry);
std::size_t handlePlayerDeaths(engine::ecs::Registry& registry);
std::size_t removeOffscreenEntities(engine::ecs::Registry& registry, WorldBounds bounds = {},
                                    float margin = OFFSCREEN_MARGIN);
std::size_t removeDeadEntities(engine::ecs::Registry& registry);

} // namespace game
