#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

// Where the enemies of a wave appear on the right edge.
// Random: anywhere. Line: all at the same height. V: alternately above and below the centre,
// further out each time. Stairs: from top to bottom, evenly spaced.
enum class Formation : std::uint8_t { Random, Line, V, Stairs };

struct WaveDefinition {
  std::uint32_t enemyCount = 5;
  float spawnIntervalSeconds = 1.2F;
  // Used when `enemy` is empty: a generic Bydos with these stats.
  int enemyHealth = 1;
  float enemySpeedMultiplier = 1.0F;
  float fireChancePerSecond = 0.25F;
  float shotSpeed = 220.0F;
  bool boss = false;
  float pickupDropChance = 0.2F; // bosses always drop an item
  // Data-driven content: names from enemies.json. Empty: the generic enemy or boss above.
  std::string enemy;
  std::string bossType;
  Formation formation = Formation::Random;
};

struct LevelDefinition {
  std::uint32_t number = 1;
  std::vector<WaveDefinition> waves;
  float interWaveDelaySeconds = 3.0F;
  float nextLevelDelaySeconds = 5.0F;
  // Presentation sent to the clients (title card, story text, background palette).
  std::string title;
  std::string story;
  std::uint8_t theme = 0;
  float introSeconds = 0.0F; // title card before the first wave
};

// What generated levels are made of. Empty pools: generic Bydos and bosses.
struct LevelGenerator {
  std::vector<std::string> enemies; // used in order of appearance, harder ones later
  std::vector<std::string> bosses;  // one boss every `bossEvery` levels, in turn
  std::uint32_t bossEvery = 1;      // level interval between named bosses (others: generic)
  std::uint32_t wavesPerLevel = 5;
  float introSeconds = 0.0F;
  std::string titlePrefix = "Level";
};

// Explicit levels override the generated ones. Any positive number, including 100, is available
// without creating one source file per level. A finite campaign (the story) sets a final level:
// clearing it completes the game.
class LevelCatalog {
public:
  void define(LevelDefinition level);
  LevelDefinition get(std::uint32_t number) const;

  void setFinalLevel(std::uint32_t number);
  std::optional<std::uint32_t> finalLevel() const { return _finalLevel; }
  void setGenerator(LevelGenerator generator);
  const LevelGenerator& generator() const { return _generator; }
  std::size_t definedLevels() const { return _levels.size(); }

private:
  std::unordered_map<std::uint32_t, LevelDefinition> _levels;
  std::optional<std::uint32_t> _finalLevel;
  LevelGenerator _generator;
};

enum class LevelPhase { Intro, Spawning, Clearing, WavePause, LevelPause, Complete };

struct LevelProgress {
  std::uint32_t level = 1;
  std::uint32_t wave = 1; // one-based within the level
  std::uint32_t waveCount = 0;
  std::uint32_t spawned = 0;
  std::uint32_t remainingEnemies = 0;
  bool bossWave = false;
  int bossHealth = 0;
  int bossMaxHealth = 0; // 0: no boss alive
  LevelPhase phase = LevelPhase::Spawning;
  float phaseRemainingSeconds = 0.0F; // Intro, WavePause and LevelPause countdowns
};

} // namespace game
