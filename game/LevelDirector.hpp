#pragma once

#include <random>

#include "game/GameSystems.hpp"
#include "game/Level.hpp"

namespace game {

// Server-owned campaign state. Gameplay/ECS remain independent of rendering and networking.
class LevelDirector {
public:
  LevelDirector(std::uint32_t seed, WorldBounds bounds, LevelCatalog catalog = {},
                std::uint32_t initialLevel = 1);
  void update(engine::ecs::Registry& registry, float deltaSeconds);
  // Loading a level clears enemies and projectiles, preserving players, lives and scores.
  void loadLevel(engine::ecs::Registry& registry, std::uint32_t number);
  const LevelProgress& progress() const { return _progress; }
  const LevelDefinition& definition() const { return _level; }

private:
  void beginLevel(std::uint32_t number);
  void beginWave();
  void spawn(engine::ecs::Registry& registry);
  void updateEnemies(engine::ecs::Registry& registry, float deltaSeconds);

  std::mt19937 _random;
  WorldBounds _bounds;
  LevelCatalog _catalog;
  LevelDefinition _level;
  LevelProgress _progress;
  float _timer = 0.0F;
};

} // namespace game
