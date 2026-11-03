#include "game/Level.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace game {

void LevelCatalog::define(LevelDefinition level) {
  if (level.number == 0 || level.waves.empty() ||
      !std::isfinite(level.interWaveDelaySeconds) || level.interWaveDelaySeconds < 0.0F ||
      !std::isfinite(level.nextLevelDelaySeconds) || level.nextLevelDelaySeconds < 0.0F) {
    throw std::invalid_argument("level needs a positive number, waves, and valid delays");
  }
  for (const auto& wave : level.waves) {
    if (wave.enemyCount == 0 || wave.enemyHealth <= 0 ||
        !std::isfinite(wave.pickupDropChance) || wave.pickupDropChance < 0.0F ||
        wave.pickupDropChance > 1.0F ||
        !std::isfinite(wave.spawnIntervalSeconds) || wave.spawnIntervalSeconds <= 0.0F ||
        !std::isfinite(wave.enemySpeedMultiplier) || wave.enemySpeedMultiplier <= 0.0F ||
        !std::isfinite(wave.fireChancePerSecond) || wave.fireChancePerSecond < 0.0F ||
        !std::isfinite(wave.shotSpeed) || wave.shotSpeed <= 0.0F ||
        (wave.boss && wave.enemyCount != 1)) {
      throw std::invalid_argument("invalid wave definition");
    }
  }
  _levels.insert_or_assign(level.number, std::move(level));
}

LevelDefinition LevelCatalog::get(std::uint32_t number) const {
  if (number == 0) {
    throw std::invalid_argument("level numbers start at one");
  }
  if (const auto found = _levels.find(number); found != _levels.end()) {
    return found->second;
  }
  LevelDefinition level;
  level.number = number;
  const auto difficulty = std::min(number - 1, 99U);
  for (std::uint32_t wave = 1; wave <= 5; ++wave) {
    WaveDefinition definition;
    definition.boss = wave % 5 == 0;
    definition.enemyCount = definition.boss ? 1 : std::min(50U, 5 + (wave - 1) * 2 + difficulty);
    definition.enemyHealth = definition.boss ? 30 + static_cast<int>(difficulty) * 10
                                           : 1 + static_cast<int>(difficulty / 10);
    definition.spawnIntervalSeconds = std::max(0.3F, 1.2F - static_cast<float>(difficulty) * 0.02F);
    definition.enemySpeedMultiplier = 1.0F + static_cast<float>(difficulty) * 0.02F;
    definition.fireChancePerSecond = 0.25F + static_cast<float>(difficulty) * 0.02F;
    definition.shotSpeed = 220.0F + static_cast<float>(difficulty) * 2.0F;
    level.waves.push_back(definition);
  }
  return level;
}

} // namespace game
