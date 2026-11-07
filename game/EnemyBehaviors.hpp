#pragma once

#include <cstdint>
#include <random>

#include "engine/ecs/Registry.hpp"
#include "game/Content.hpp"
#include "game/GameSystems.hpp"

namespace game {

// Spawns an enemy of a content type at (x, y), its stats scaled by the difficulty and by the
// wave's speed multiplier. Its fire timer starts at a random point so a wave does not shoot in
// sync.
engine::ecs::Entity spawnEnemyOfType(engine::ecs::Registry& registry, const EnemyType& type,
                                     float x, float y, const DifficultySettings& difficulty,
                                     float speedMultiplier, std::mt19937& random);

// Spawns a boss just off the right edge; it flies in, then sweeps up and down. `health` is used
// for the generic boss (index GENERIC_BOSS), whose health comes from the wave.
engine::ecs::Entity spawnBoss(engine::ecs::Registry& registry, const GameContent& content,
                              std::uint16_t type, int health, const DifficultySettings& difficulty,
                              WorldBounds bounds);

// Steers every enemy that has an EnemyBehavior (velocity only: the movement system moves it)
// and fires its volleys at the players.
void updateEnemyBehaviors(engine::ecs::Registry& registry, float deltaSeconds, WorldBounds bounds,
                          std::mt19937& random);

// Moves every boss, picks its phase from its health and runs the attacks of that phase.
void updateBosses(engine::ecs::Registry& registry, float deltaSeconds, WorldBounds bounds,
                  const GameContent& content, const DifficultySettings& difficulty,
                  std::mt19937& random);

// Index of the boss phase active at this health ratio (1.0 = full health).
std::size_t bossPhaseFor(const BossType& boss, float healthRatio);

} // namespace game
