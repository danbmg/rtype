#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game/GameSystems.hpp"

namespace {

using namespace rtype::common::components;

struct TestWorld {
  engine::ecs::Registry registry;

  TestWorld() { game::registerSystems(registry); }

  engine::ecs::Entity addPlayer(float x, float y, std::uint32_t id = 1, int health = 3) {
    const auto player = registry.spawnEntity();
    registry.addComponent(player, Position{x, y});
    registry.addComponent(player, Velocity{});
    registry.addComponent(player, Player{id, 3});
    registry.addComponent(player, Health{health, health});
    registry.addComponent(player, Hitbox{20.0F, 20.0F});
    return player;
  }
};

} // namespace

TEST_CASE("Player input is normalized and applied as velocity", "[game][input]") {
  TestWorld world;
  const auto player = world.addPlayer(0.0F, 0.0F);

  game::applyPlayerInput(world.registry, player, 1.0F, 1.0F, 100.0F);
  const auto* velocity = world.registry.getComponent<Velocity>(player);
  REQUIRE(velocity != nullptr);
  CHECK(std::hypot(velocity->vx, velocity->vy) == Catch::Approx(100.0F));

  game::applyPlayerInput(world.registry, player, 4.0F, -2.0F, 100.0F);
  CHECK(velocity->vx == Catch::Approx(100.0F / std::sqrt(2.0F)));
  CHECK(velocity->vy == Catch::Approx(-100.0F / std::sqrt(2.0F)));

  game::applyPlayerInput(world.registry, player, 0.0F, 0.0F, 100.0F);
  CHECK(velocity->vx == 0.0F);
  CHECK(velocity->vy == 0.0F);
}

TEST_CASE("Player input rejects invalid actors and values", "[game][input][invalid-input]") {
  TestWorld world;
  const auto player = world.addPlayer(0.0F, 0.0F);
  const auto nonPlayer = world.registry.spawnEntity();
  world.registry.addComponent(nonPlayer, Velocity{});

  CHECK_THROWS_AS(game::applyPlayerInput(world.registry, player,
                                         std::numeric_limits<float>::quiet_NaN(), 0.0F, 1.0F),
                  std::invalid_argument);
  CHECK_THROWS_AS(game::applyPlayerInput(world.registry, player, 0.0F, 0.0F, -1.0F),
                  std::invalid_argument);
  CHECK_THROWS_AS(game::applyPlayerInput(world.registry, nonPlayer, 0.0F, 0.0F, 1.0F),
                  std::invalid_argument);
}

TEST_CASE("Movement scales by delta time and rejects invalid deltas", "[game][movement]") {
  TestWorld world;
  const auto player = world.addPlayer(10.0F, 20.0F);
  world.registry.addComponent(player, Velocity{4.0F, -2.0F});

  game::updateMovement(world.registry, 0.5F);
  const auto* position = world.registry.getComponent<Position>(player);
  REQUIRE(position != nullptr);
  CHECK(position->x == 12.0F);
  CHECK(position->y == 19.0F);

  CHECK_THROWS_AS(game::updateMovement(world.registry, -0.1F), std::invalid_argument);
  CHECK_THROWS_AS(
      game::updatePlayerMovement(world.registry, std::numeric_limits<float>::infinity()),
      std::invalid_argument);
}

TEST_CASE("Screen bounds clamp players using their hitbox size", "[game][bounds]") {
  TestWorld world;
  const auto player = world.addPlayer(-5.0F, 100.0F);
  auto* position = world.registry.getComponent<Position>(player);
  REQUIRE(position != nullptr);

  game::keepEntitiesOnScreen(world.registry, game::WorldBounds{100.0F, 80.0F});
  CHECK(position->x == 0.0F);
  CHECK(position->y == 60.0F);

  position->x = 500.0F;
  position->y = -5.0F;
  game::keepEntitiesOnScreen(world.registry, game::WorldBounds{100.0F, 80.0F});
  CHECK(position->x == 80.0F);
  CHECK(position->y == 0.0F);
}

TEST_CASE("Enemy spawning is reproducible and invalid inputs are rejected", "[game][spawn]") {
  TestWorld firstWorld;
  TestWorld secondWorld;
  std::mt19937 firstRandom(1234U);
  std::mt19937 secondRandom(1234U);

  const auto firstEnemy =
      game::spawnRandomEnemy(firstWorld.registry, firstRandom, game::WorldBounds{800.0F, 600.0F});
  const auto secondEnemy =
      game::spawnRandomEnemy(secondWorld.registry, secondRandom, game::WorldBounds{800.0F, 600.0F});
  REQUIRE(firstWorld.registry.containsEntity(firstEnemy));
  REQUIRE(secondWorld.registry.containsEntity(secondEnemy));
  CHECK(firstWorld.registry.getComponent<Position>(firstEnemy)->y ==
        secondWorld.registry.getComponent<Position>(secondEnemy)->y);
  CHECK(firstWorld.registry.getComponent<Velocity>(firstEnemy)->vx ==
        secondWorld.registry.getComponent<Velocity>(secondEnemy)->vx);
  CHECK(firstWorld.registry.getComponent<Position>(firstEnemy)->x == 800.0F);
  CHECK(firstWorld.registry.getComponent<Velocity>(firstEnemy)->vx < 0.0F);

  CHECK_THROWS_AS(game::spawnEnemy(firstWorld.registry, 0.0F, 0.0F, 0.0F, 0.0F, 0),
                  std::invalid_argument);
  CHECK_THROWS_AS(
      game::spawnRandomEnemy(firstWorld.registry, firstRandom, game::WorldBounds{0.0F, 20.0F}),
      std::invalid_argument);
}

TEST_CASE("AABB collisions require positive overlapping hitbox area", "[game][collision]") {
  TestWorld world;
  const auto player = world.addPlayer(0.0F, 0.0F);
  const auto enemy = game::spawnEnemy(world.registry, 18.0F, 0.0F, 0.0F, 0.0F, 2);
  const auto touchingShot = game::fireProjectile(world.registry, player, true, 1, 0.0F, 0.0F);

  CHECK(game::resolveProjectileCollisions(world.registry) == 0);
  CHECK(world.registry.containsEntity(touchingShot));

  world.registry.getComponent<Position>(enemy)->x = 17.0F;
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(enemy)->current == 1);
  CHECK_FALSE(world.registry.containsEntity(touchingShot));
}

TEST_CASE("Projectiles cannot hit their owner or their own side", "[game][collision]") {
  TestWorld world;
  const auto owner = world.addPlayer(0.0F, 0.0F, 1);
  const auto teammate = world.addPlayer(0.0F, 0.0F, 2);

  const auto friendly = game::fireProjectile(world.registry, owner, true, 1, 0.0F, 0.0F);
  CHECK(game::resolveProjectileCollisions(world.registry) == 0);
  CHECK(world.registry.containsEntity(friendly));
  CHECK(world.registry.getComponent<Health>(owner)->current == 3);
  CHECK(world.registry.getComponent<Health>(teammate)->current == 3);

  const auto enemy = game::spawnEnemy(world.registry, 0.0F, 0.0F, 0.0F, 0.0F);
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(enemy)->current == 0);
}

TEST_CASE("Projectile damage and dead entity cleanup are separate operations", "[game][damage]") {
  TestWorld world;
  const auto player = world.addPlayer(0.0F, 0.0F);
  const auto enemy = game::spawnEnemy(world.registry, 0.0F, 0.0F, 0.0F, 0.0F, 2);

  const auto firstShot = game::fireProjectile(world.registry, player, true, 1, 0.0F, 0.0F);
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(enemy)->current == 1);
  CHECK(game::removeDeadEntities(world.registry) == 0);

  const auto secondShot = game::fireProjectile(world.registry, player, true, 1, 0.0F, 0.0F);
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(enemy)->current == 0);
  CHECK(game::removeDeadEntities(world.registry) == 1);
  CHECK_FALSE(world.registry.containsEntity(enemy));
  CHECK_FALSE(world.registry.containsEntity(firstShot));
  CHECK_FALSE(world.registry.containsEntity(secondShot));
}

TEST_CASE("Enemy projectiles aim at players and damage them", "[game][enemy-fire]") {
  TestWorld world;
  const auto player = world.addPlayer(20.0F, 20.0F);
  const auto enemy = game::spawnEnemy(world.registry, 0.0F, 0.0F, 0.0F, 0.0F);

  const auto shot = game::fireEnemyProjectile(world.registry, enemy, player, 100.0F);
  const auto* velocity = world.registry.getComponent<Velocity>(shot);
  REQUIRE(velocity != nullptr);
  CHECK(velocity->vx > 0.0F);
  CHECK(velocity->vy > 0.0F);
  CHECK(std::hypot(velocity->vx, velocity->vy) == Catch::Approx(100.0F));
  game::updateMovement(world.registry, 0.2F);
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(player)->current == 2);

  CHECK_THROWS_AS(game::fireEnemyProjectile(world.registry, player, enemy, 100.0F),
                  std::invalid_argument);
}

TEST_CASE("Gameplay scheduler executes movement then player bounds", "[game][scheduler]") {
  TestWorld world;
  const auto player = world.addPlayer(95.0F, 0.0F);
  world.registry.addComponent(player, Velocity{20.0F, 0.0F});
  engine::core::SystemScheduler<engine::ecs::Registry> scheduler;
  game::registerGameplaySystems(scheduler, 1.0F, game::WorldBounds{100.0F, 80.0F});

  scheduler.run(world.registry);

  CHECK(world.registry.getComponent<Position>(player)->x == 80.0F);
}

TEST_CASE("Entities that leave the screen are destroyed, players are kept", "[game][offscreen]") {
  TestWorld world;
  const auto player = world.addPlayer(100.0F, 300.0F);
  engine::core::SystemScheduler<engine::ecs::Registry> scheduler;
  game::registerGameplaySystems(scheduler, 1.0F / 60.0F);

  // One minute of shooting into empty space at 10 shots per second.
  for (int tick = 0; tick < 60 * 60; ++tick) {
    if (tick % 6 == 0) {
      game::firePlayerProjectile(world.registry, player, 500.0F);
    }
    scheduler.run(world.registry);
  }
  // Without cleanup all 600 shots would still be alive. Only those still crossing the screen
  // remain: about (800 + margin - 110) px / 500 px/s = 1.5 s of fire, so at most 16 shots.
  CHECK(world.registry.view<Projectile>().size() <= 16);
  CHECK(world.registry.containsEntity(player));

  const auto leaving =
      game::spawnEnemy(world.registry, -game::OFFSCREEN_MARGIN - 1.0F, 0.0F, -10.0F, 0.0F);
  const auto entering = game::spawnEnemy(world.registry, 800.0F, 0.0F, -10.0F, 0.0F);
  const auto broken = game::spawnEnemy(world.registry, 0.0F, 0.0F, 0.0F, 0.0F);
  world.registry.getComponent<Position>(broken)->y = std::numeric_limits<float>::quiet_NaN();
  CHECK(game::removeOffscreenEntities(world.registry) == 2);
  CHECK_FALSE(world.registry.containsEntity(leaving));
  CHECK_FALSE(world.registry.containsEntity(broken));
  CHECK(world.registry.containsEntity(entering));

  CHECK_THROWS_AS(game::removeOffscreenEntities(world.registry, game::WorldBounds{}, -1.0F),
                  std::invalid_argument);
}

TEST_CASE("Player shots are friendly, owned by the shooter and fly right", "[game][player-fire]") {
  TestWorld world;
  const auto player = world.addPlayer(0.0F, 0.0F, 3);
  const auto shot = game::firePlayerProjectile(world.registry, player, 300.0F);

  const auto* projectile = world.registry.getComponent<Projectile>(shot);
  REQUIRE(projectile != nullptr);
  CHECK(projectile->friendly);
  CHECK(projectile->ownerId == 3);
  CHECK(world.registry.getComponent<Velocity>(shot)->vx == 300.0F);

  const auto enemy = game::spawnEnemy(world.registry, 0.0F, 0.0F, 0.0F, 0.0F);
  CHECK_THROWS_AS(game::firePlayerProjectile(world.registry, enemy, 300.0F), std::invalid_argument);
  CHECK_THROWS_AS(game::firePlayerProjectile(world.registry, player, 0.0F), std::invalid_argument);
}

TEST_CASE("Killing a Bydos awards its points to the shooter only", "[game][score]") {
  TestWorld world;
  const auto shooter = world.addPlayer(0.0F, 0.0F, 1);
  const auto other = world.addPlayer(500.0F, 500.0F, 2);
  const auto enemy = game::spawnEnemy(world.registry, 0.0F, 0.0F, 0.0F, 0.0F, 2);

  game::firePlayerProjectile(world.registry, shooter, 1.0F);
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Player>(shooter)->score == 0);

  game::firePlayerProjectile(world.registry, shooter, 1.0F);
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(enemy)->current == 0);
  CHECK(world.registry.getComponent<Player>(shooter)->score ==
        static_cast<std::uint32_t>(world.registry.getComponent<Enemy>(enemy)->points));
  CHECK(world.registry.getComponent<Player>(other)->score == 0);
}

TEST_CASE("Ramming a Bydos destroys it and costs the player one hit point", "[game][collision]") {
  TestWorld world;
  const auto player = world.addPlayer(0.0F, 0.0F);
  const auto enemy = game::spawnEnemy(world.registry, 10.0F, 10.0F, 0.0F, 0.0F, 5);
  const auto farEnemy = game::spawnEnemy(world.registry, 400.0F, 400.0F, 0.0F, 0.0F);

  CHECK(game::resolveShipCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(player)->current == 2);
  CHECK(world.registry.getComponent<Health>(enemy)->current == 0);
  CHECK(world.registry.getComponent<Health>(farEnemy)->current == 1);
  CHECK(world.registry.getComponent<Player>(player)->score == 0);

  CHECK(game::resolveShipCollisions(world.registry) == 0);
}

TEST_CASE("A player loses a life per death and is removed after the last one", "[game][lives]") {
  TestWorld world;
  const auto player = world.addPlayer(0.0F, 0.0F);
  auto* health = world.registry.getComponent<Health>(player);
  auto* stats = world.registry.getComponent<Player>(player);
  REQUIRE(stats->lives == 3);

  CHECK(game::handlePlayerDeaths(world.registry) == 0);

  health->current = 0;
  CHECK(game::handlePlayerDeaths(world.registry) == 1);
  CHECK(stats->lives == 2);
  CHECK(health->current == health->max);
  CHECK(game::removeDeadEntities(world.registry) == 0);

  health->current = 0;
  game::handlePlayerDeaths(world.registry);
  health->current = 0;
  CHECK(game::handlePlayerDeaths(world.registry) == 1);
  CHECK(stats->lives == 0);
  CHECK(health->current == 0);
  CHECK(game::removeDeadEntities(world.registry) == 1);
  CHECK_FALSE(world.registry.containsEntity(player));
}

TEST_CASE("Enemy systems spawn Bydos on a timer and only shoot at players", "[game][spawn]") {
  TestWorld world;
  engine::core::SystemScheduler<engine::ecs::Registry> scheduler;
  game::EnemyWaveSettings settings;
  settings.spawnIntervalSeconds = 0.5F;
  settings.fireChancePerSecond = 1000.0F;
  game::registerEnemySystems(scheduler, 0.25F, 42U, game::WorldBounds{800.0F, 600.0F}, settings);

  for (int tick = 0; tick < 8; ++tick) {
    scheduler.run(world.registry);
  }
  // Two seconds at one Bydos every half second; nobody to shoot at yet.
  CHECK(world.registry.view<Enemy>().size() == 4);
  CHECK(world.registry.view<Projectile>().empty());

  for (const auto enemy : world.registry.view<Enemy>()) {
    world.registry.getComponent<Position>(enemy)->x = 700.0F;
  }
  world.addPlayer(100.0F, 300.0F);
  scheduler.run(world.registry);
  const auto shots = world.registry.view<Projectile>();
  REQUIRE_FALSE(shots.empty());
  for (const auto shot : shots) {
    CHECK_FALSE(world.registry.getComponent<Projectile>(shot)->friendly);
  }

  game::EnemyWaveSettings invalid;
  invalid.spawnIntervalSeconds = 0.0F;
  CHECK_THROWS_AS(game::registerEnemySystems(scheduler, 0.25F, 1U, {}, invalid),
                  std::invalid_argument);
  CHECK_THROWS_AS(game::registerEnemySystems(scheduler, 0.0F, 1U), std::invalid_argument);
}

TEST_CASE("Enemy waves are reproducible with the same seed", "[game][spawn]") {
  TestWorld first;
  TestWorld second;
  engine::core::SystemScheduler<engine::ecs::Registry> firstScheduler;
  engine::core::SystemScheduler<engine::ecs::Registry> secondScheduler;
  game::registerEnemySystems(firstScheduler, 0.5F, 7U);
  game::registerEnemySystems(secondScheduler, 0.5F, 7U);
  for (int tick = 0; tick < 10; ++tick) {
    firstScheduler.run(first.registry);
    secondScheduler.run(second.registry);
  }

  const auto firstEnemies = first.registry.view<Enemy>();
  const auto secondEnemies = second.registry.view<Enemy>();
  REQUIRE(firstEnemies.size() == secondEnemies.size());
  REQUIRE_FALSE(firstEnemies.empty());
  for (std::size_t index = 0; index < firstEnemies.size(); ++index) {
    CHECK(first.registry.getComponent<Position>(firstEnemies[index])->y ==
          second.registry.getComponent<Position>(secondEnemies[index])->y);
  }
}
