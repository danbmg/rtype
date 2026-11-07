#pragma once

#include <memory>
#include <random>

#include "game/Content.hpp"
#include "game/GameSystems.hpp"
#include "game/Level.hpp"

namespace game {

// Server-owned campaign state: which level and wave is running, what spawns next, and how the
// enemies on screen behave. Gameplay and ECS stay independent of rendering and networking.
//
// Phases of a level: Intro (title card, no enemy) -> for each wave: Spawning -> Clearing
// (every enemy dead or gone) -> WavePause; after the last wave: LevelPause -> next level, or
// Complete when the catalog's final level is cleared (and its Summoner's Cup picked up).
class LevelDirector {
public:
  LevelDirector(std::uint32_t seed, WorldBounds bounds, LevelCatalog catalog = {},
                std::uint32_t initialLevel = 1,
                std::shared_ptr<const GameContent> content = nullptr,
                Difficulty difficulty = Difficulty::Normal);
  void update(engine::ecs::Registry& registry, float deltaSeconds);
  // Loading a level clears enemies and projectiles, preserving players, lives and scores.
  void loadLevel(engine::ecs::Registry& registry, std::uint32_t number);
  const LevelProgress& progress() const { return _progress; }
  const LevelDefinition& definition() const { return _level; }
  const LevelCatalog& catalog() const { return _catalog; }
  Difficulty difficulty() const { return _difficulty; }

private:
  void beginLevel(std::uint32_t number);
  void beginWave();
  void spawn(engine::ecs::Registry& registry);
  float formationY(const WaveDefinition& wave, float height);
  void finishWave(engine::ecs::Registry& registry);
  void refreshCounters(const engine::ecs::Registry& registry);

  std::mt19937 _random;
  WorldBounds _bounds;
  LevelCatalog _catalog;
  std::shared_ptr<const GameContent> _content;
  Difficulty _difficulty;
  DifficultySettings _settings;
  LevelDefinition _level;
  LevelProgress _progress;
  float _timer = 0.0F;
  float _lineY = 0.0F; // height of the current Line formation
};

} // namespace game
