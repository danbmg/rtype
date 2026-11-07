#include "game/Level.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace game {

void LevelCatalog::define(LevelDefinition level) {
  if (level.number == 0 || level.waves.empty() || !std::isfinite(level.interWaveDelaySeconds) ||
      level.interWaveDelaySeconds < 0.0F || !std::isfinite(level.nextLevelDelaySeconds) ||
      level.nextLevelDelaySeconds < 0.0F || !std::isfinite(level.introSeconds) ||
      level.introSeconds < 0.0F) {
    throw std::invalid_argument("level needs a positive number, waves, and valid delays");
  }
  for (const auto& wave : level.waves) {
    if (wave.enemyCount == 0 || wave.enemyHealth <= 0 || !std::isfinite(wave.pickupDropChance) ||
        wave.pickupDropChance < 0.0F || wave.pickupDropChance > 1.0F ||
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

void LevelCatalog::setFinalLevel(std::uint32_t number) {
  if (number == 0) {
    throw std::invalid_argument("the final level must be positive");
  }
  _finalLevel = number;
}

void LevelCatalog::setGenerator(LevelGenerator generator) {
  if (generator.bossEvery == 0 || generator.wavesPerLevel == 0 || generator.wavesPerLevel > 20 ||
      !std::isfinite(generator.introSeconds) || generator.introSeconds < 0.0F) {
    throw std::invalid_argument("invalid level generator");
  }
  _generator = std::move(generator);
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
  level.introSeconds = _generator.introSeconds;
  level.title = _generator.titlePrefix + " " + std::to_string(number);
  level.theme = static_cast<std::uint8_t>((number - 1) % 5);
  const auto difficulty = std::min(number - 1, 99U);
  const auto waves = _generator.wavesPerLevel;
  const auto& pool = _generator.enemies;
  for (std::uint32_t wave = 1; wave <= waves; ++wave) {
    WaveDefinition definition;
    definition.boss = wave == waves;
    definition.enemyCount =
        definition.boss ? 1 : std::min(50U, 5 + (wave - 1) * 2 + difficulty);
    definition.enemyHealth = definition.boss ? 30 + static_cast<int>(difficulty) * 10
                                             : 1 + static_cast<int>(difficulty / 10);
    definition.spawnIntervalSeconds =
        std::max(0.3F, 1.2F - static_cast<float>(difficulty) * 0.02F);
    definition.enemySpeedMultiplier = 1.0F + static_cast<float>(difficulty) * 0.02F;
    definition.fireChancePerSecond = 0.25F + static_cast<float>(difficulty) * 0.02F;
    definition.shotSpeed = 220.0F + static_cast<float>(difficulty) * 2.0F;
    if (!definition.boss && !pool.empty()) {
      // New kinds join as levels go up; each wave picks among those already unlocked.
      const auto unlocked = std::min<std::size_t>(pool.size(), 1 + (number - 1) / 2 + wave / 3);
      definition.enemy = pool[(number + wave * 3) % unlocked];
      definition.formation = static_cast<Formation>((number + wave) % 4);
    }
    if (definition.boss && !_generator.bosses.empty() && number % _generator.bossEvery == 0) {
      const auto turn = (number / _generator.bossEvery - 1) % _generator.bosses.size();
      definition.bossType = _generator.bosses[turn];
    }
    level.waves.push_back(definition);
  }
  return level;
}

} // namespace game
