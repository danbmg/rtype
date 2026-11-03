#include <limits>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "game/LevelDirector.hpp"

namespace {
using namespace rtype::common::components;

game::LevelDefinition shortLevel(std::uint32_t number) {
  game::LevelDefinition level;
  level.number = number;
  level.interWaveDelaySeconds = 0.5F;
  level.nextLevelDelaySeconds = 0.5F;
  game::WaveDefinition wave;
  wave.enemyCount = 1;
  wave.spawnIntervalSeconds = 0.25F;
  wave.fireChancePerSecond = 0.0F;
  level.waves = {wave, wave};
  level.waves.back().boss = true;
  level.waves.back().enemyHealth = 30;
  return level;
}

void clearEnemies(engine::ecs::Registry& registry) {
  for (const auto enemy : registry.view<Enemy>()) {
    registry.killEntity(enemy);
  }
}
} // namespace

TEST_CASE("Catalog generates numbered levels and supports overrides", "[game][levels]") {
  game::LevelCatalog catalog;
  const auto first = catalog.get(1);
  const auto second = catalog.get(2);
  const auto hundred = catalog.get(100);
  REQUIRE(first.waves.size() == 5);
  CHECK(first.waves[0].enemyCount == 5);
  CHECK(second.waves[0].enemyCount > first.waves[0].enemyCount);
  CHECK(hundred.number == 100);
  CHECK(hundred.waves.back().enemyHealth > second.waves.back().enemyHealth);
  CHECK(hundred.waves.back().boss);
  CHECK(catalog.get(std::numeric_limits<std::uint32_t>::max()).waves.size() == 5);
  catalog.define(shortLevel(100));
  CHECK(catalog.get(100).waves.size() == 2);
  CHECK_THROWS_AS(catalog.get(0), std::invalid_argument);
  auto invalid = shortLevel(1);
  invalid.waves.front().spawnIntervalSeconds = 0.0F;
  CHECK_THROWS_AS(catalog.define(invalid), std::invalid_argument);
  invalid = shortLevel(1);
  invalid.waves.clear();
  CHECK_THROWS_AS(catalog.define(invalid), std::invalid_argument);
}

TEST_CASE("Director clears waves before progressing through a level and into the next", "[game][levels]") {
  engine::ecs::Registry registry;
  game::registerSystems(registry);
  game::LevelCatalog catalog;
  catalog.define(shortLevel(1));
  catalog.define(shortLevel(2));
  game::LevelDirector director(42, {}, catalog);
  director.update(registry, 0.25F);
  REQUIRE(registry.view<Enemy>().size() == 1);
  CHECK(director.progress().phase == game::LevelPhase::Clearing);
  for (int i = 0; i < 10; ++i) {
    director.update(registry, 0.25F);
  }
  CHECK(director.progress().wave == 1);
  clearEnemies(registry);
  director.update(registry, 0.25F);
  CHECK(director.progress().phase == game::LevelPhase::WavePause);
  director.update(registry, 0.25F);
  CHECK(director.progress().wave == 2);
  CHECK(director.progress().bossWave);
  director.update(registry, 0.25F);
  REQUIRE(registry.view<Enemy>().size() == 1);
  const auto boss = registry.view<Enemy>().front();
  CHECK(registry.getComponent<Enemy>(boss)->pattern == Enemy::BOSS_PATTERN);
  clearEnemies(registry);
  director.update(registry, 0.25F);
  CHECK(director.progress().phase == game::LevelPhase::LevelPause);
  director.update(registry, 0.25F);
  CHECK(director.progress().level == 2);
  CHECK(director.progress().wave == 1);
  CHECK(director.progress().spawned == 0);
}

TEST_CASE("Loading a level resets encounters but preserves player state", "[game][levels]") {
  engine::ecs::Registry registry;
  game::registerSystems(registry);
  game::LevelDirector director(42, {});
  const auto player = registry.spawnEntity();
  registry.addComponent(player, Player{0, 2, 1234});
  registry.addComponent(player, Health{2, 3});
  registry.addComponent(player, Position{60.0F, 100.0F});
  registry.addComponent(player, Hitbox{33.0F, 17.0F});
  game::spawnEnemy(registry, 700, 100, 0, 0);
  game::firePlayerProjectile(registry, player, 600);
  CHECK_THROWS_AS(director.loadLevel(registry, 0), std::invalid_argument);
  CHECK_FALSE(registry.view<Enemy>().empty());
  director.loadLevel(registry, 100);
  CHECK(director.progress().level == 100);
  CHECK(director.progress().wave == 1);
  CHECK(registry.view<Enemy>().empty());
  CHECK(registry.view<Projectile>().empty());
  CHECK(registry.getComponent<Player>(player)->score == 1234);
  CHECK(registry.getComponent<Player>(player)->lives == 2);
  CHECK(registry.getComponent<Health>(player)->current == 2);
}

TEST_CASE("Last representable level completes without wrapping to zero", "[game][levels]") {
  engine::ecs::Registry registry;
  game::registerSystems(registry);
  auto level = shortLevel(std::numeric_limits<std::uint32_t>::max());
  level.waves.resize(1);
  level.nextLevelDelaySeconds = 0;
  game::LevelCatalog catalog;
  catalog.define(level);
  game::LevelDirector director(42, {}, catalog, level.number);
  director.update(registry, 0.25F);
  clearEnemies(registry);
  director.update(registry, 0.25F);
  CHECK(director.progress().phase == game::LevelPhase::Complete);
  director.update(registry, 0.25F);
  CHECK(director.progress().level == level.number);
}
