#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace game {

struct WaveDefinition {
  std::uint32_t enemyCount = 5;
  float spawnIntervalSeconds = 1.2F;
  int enemyHealth = 1;
  float enemySpeedMultiplier = 1.0F;
  float fireChancePerSecond = 0.25F;
  float shotSpeed = 220.0F;
  bool boss = false;
  float pickupDropChance = 0.2F; // bosses always drop an item
};

struct LevelDefinition {
  std::uint32_t number = 1;
  std::vector<WaveDefinition> waves;
  float interWaveDelaySeconds = 3.0F;
  float nextLevelDelaySeconds = 5.0F;
};

// Explicit levels override the generated campaign. Any positive number, including 100,
// is available without creating one source file per level.
class LevelCatalog {
public:
  void define(LevelDefinition level);
  LevelDefinition get(std::uint32_t number) const;

private:
  std::unordered_map<std::uint32_t, LevelDefinition> _levels;
};

enum class LevelPhase { Spawning, Clearing, WavePause, LevelPause, Complete };

struct LevelProgress {
  std::uint32_t level = 1;
  std::uint32_t wave = 1; // one-based within the level
  std::uint32_t waveCount = 0;
  std::uint32_t spawned = 0;
  std::uint32_t remainingEnemies = 0;
  bool bossWave = false;
  LevelPhase phase = LevelPhase::Spawning;
};

} // namespace game
