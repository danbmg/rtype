#include <algorithm>
#include <map>

#include <catch2/catch_test_macros.hpp>

#include "server/GameWorld.hpp"
#include "game/EquipmentSystems.hpp"

namespace protocol = rtype::protocol;
using namespace rtype::common::components;
using server::GameWorld;

namespace {

constexpr std::uint8_t TICK_RATE = 60;

const protocol::EntityState* findState(const std::vector<protocol::EntityState>& states,
                                       std::uint32_t id) {
  const auto found = std::ranges::find(states, id, &protocol::EntityState::id);
  return found == states.end() ? nullptr : &*found;
}

std::size_t countKind(const std::vector<protocol::EntityState>& states, protocol::EntityKind kind) {
  return static_cast<std::size_t>(std::ranges::count(states, kind, &protocol::EntityState::kind));
}

void runTicks(GameWorld& world, int ticks) {
  for (int tick = 0; tick < ticks; ++tick) {
    world.step();
    world.collectChanges();
  }
}

} // namespace

TEST_CASE("A new ship is announced once, then appears in every snapshot", "[server][world]") {
  GameWorld world(TICK_RATE, 1U);
  const auto ship = world.spawnPlayer(2);

  world.step();
  const auto changes = world.collectChanges();
  const auto* spawned = findState(changes.spawned, ship);
  REQUIRE(spawned != nullptr);
  CHECK(spawned->kind == protocol::EntityKind::Player);
  CHECK(spawned->playerId == 2);

  world.step();
  CHECK(findState(world.collectChanges().spawned, ship) == nullptr);
  CHECK(findState(world.snapshot(), ship) != nullptr);
}

TEST_CASE("Inputs move the ship in screen coordinates", "[server][world]") {
  GameWorld world(TICK_RATE, 1U);
  const auto ship = world.spawnPlayer(0);
  const auto start = *world.registry().getComponent<Position>(ship);

  world.applyInput(ship, protocol::input::RIGHT | protocol::input::UP);
  world.step();
  const auto* moved = world.registry().getComponent<Position>(ship);
  CHECK(moved->x > start.x);
  CHECK(moved->y < start.y); // y grows downwards

  world.applyInput(ship, 0);
  const auto stopped = *moved;
  world.step();
  CHECK(moved->x == stopped.x);
}

TEST_CASE("Holding fire shoots four missiles per second", "[server][world]") {
  GameWorld world(TICK_RATE, 1U);
  const auto ship = world.spawnPlayer(3);
  std::size_t missiles = 0;
  for (int tick = 0; tick < TICK_RATE; ++tick) {
    world.applyInput(ship, protocol::input::FIRE);
    world.step();
    for (const auto& state : world.collectChanges().spawned) {
      if (state.kind == protocol::EntityKind::PlayerMissile) {
        CHECK(state.playerId == 3);
        ++missiles;
      }
    }
  }
  CHECK(missiles == 4);
}

TEST_CASE("Removals say whether the entity died on screen or flew away", "[server][world]") {
  GameWorld world(TICK_RATE, 1U);
  auto& registry = world.registry();
  const auto ship = world.spawnPlayer(0);
  const auto missile = game::firePlayerProjectile(registry, ship, 600.0F);
  const auto bydos = game::spawnEnemy(registry, 400.0F, 300.0F, 0.0F, 0.0F);
  registry.getComponent<Position>(missile)->x = 780.0F; // about to leave on the right
  world.collectChanges();
  registry.getComponent<Health>(bydos)->current = 0; // killed where it stands

  std::map<std::uint32_t, protocol::DestroyReason> reasons;
  for (int tick = 0; tick < 30 && !(reasons.contains(missile) && reasons.contains(bydos)); ++tick) {
    world.step();
    for (const auto& removal : world.collectChanges().destroyed) {
      reasons[removal.id] = removal.reason;
    }
  }
  REQUIRE(reasons.contains(bydos));
  REQUIRE(reasons.contains(missile));
  CHECK(reasons.at(bydos) == protocol::DestroyReason::Killed);
  CHECK(reasons.at(missile) == protocol::DestroyReason::LeftScreen);
}

TEST_CASE("Player status follows lives and score, and survives the ship", "[server][world]") {
  GameWorld world(TICK_RATE, 1U);
  const auto ship = world.spawnPlayer(1);
  world.registry().getComponent<Player>(ship)->score = 300;
  world.step();

  auto states = world.playerStates();
  REQUIRE(states.size() == 1);
  CHECK(states[0].playerId == 1);
  CHECK(states[0].lives == 3);
  CHECK(states[0].score == 300);

  // Last life lost: the ship disappears, the status stays with zero lives.
  world.registry().getComponent<Player>(ship)->lives = 1;
  world.registry().getComponent<Health>(ship)->current = 0;
  world.step();
  CHECK_FALSE(world.registry().containsEntity(ship));
  states = world.playerStates();
  REQUIRE(states.size() == 1);
  CHECK(states[0].lives == 0);
  CHECK(states[0].score == 300);

  world.removePlayer(1, ship);
  CHECK(world.playerStates().empty());
}

TEST_CASE("Bydos keep coming even with nobody connected", "[server][world]") {
  GameWorld world(TICK_RATE, 7U);
  runTicks(world, 3 * TICK_RATE);
  CHECK(countKind(world.snapshot(), protocol::EntityKind::Bydos) >= 2);
  CHECK(world.tick() == 3U * TICK_RATE);
}

TEST_CASE("GameWorld exposes and loads server levels without resetting players", "[server][world][levels]") {
  GameWorld world(TICK_RATE, 1U, {}, {}, 100);
  CHECK(world.levelProgress().level == 100);
  const auto ship = world.spawnPlayer(0);
  world.step();
  world.loadLevel(2);
  CHECK(world.levelProgress().level == 2);
  CHECK(world.levelProgress().wave == 1);
  CHECK(world.registry().containsEntity(ship));
  CHECK(world.playerStates().front().lives == 3);
  CHECK(world.levelDefinition().number == 2);
  CHECK_THROWS_AS(world.loadLevel(0), std::invalid_argument);
  CHECK(world.levelProgress().level == 2);
}

TEST_CASE("GameWorld advances custom levels and replicates bosses", "[server][world][levels]") {
  game::LevelCatalog catalog;
  game::LevelDefinition level;
  level.number = 1;
  level.nextLevelDelaySeconds = 0.0F;
  game::WaveDefinition bossWave;
  bossWave.boss = true;
  bossWave.enemyCount = 1;
  bossWave.enemyHealth = 30;
  bossWave.spawnIntervalSeconds = 0.01F;
  bossWave.fireChancePerSecond = 0.0F;
  level.waves.push_back(bossWave);
  catalog.define(level);
  GameWorld world(TICK_RATE, 42U, {}, catalog);
  const auto ship = world.spawnPlayer(0);
  world.registry().getComponent<Player>(ship)->score = 1234;
  world.step();
  const auto changes = world.collectChanges();
  const auto bossState = std::ranges::find(changes.spawned, protocol::EntityKind::Boss,
                                          &protocol::EntityState::kind);
  REQUIRE(bossState != changes.spawned.end());
  world.registry().getComponent<Health>(bossState->id)->current = 0;
  world.step(); // gameplay cleanup removes the boss
  world.step(); // director observes the completed encounter
  CHECK(world.levelProgress().level == 2);
  CHECK(world.levelProgress().wave == 1);
  CHECK(world.registry().getComponent<Player>(ship)->score == 1234);
  const auto removals = world.collectChanges();
  CHECK(std::ranges::find(removals.destroyed, bossState->id,
                         &protocol::EntityRemoval::id) != removals.destroyed.end());
}

TEST_CASE("Server enforces weapon cooldowns and rapid fire", "[server][world][equipment]") {
  for (const bool rapid : {false, true}) {
    GameWorld world(TICK_RATE, 42U);
    const auto ship = world.spawnPlayer(0);
    game::applyPickup(world.registry(), ship, PickupType::DoubleShot);
    if (rapid) {
      game::applyPickup(world.registry(), ship, PickupType::RapidFire);
    }
    for (int tick = 0; tick < 30; ++tick) {
      world.applyInput(ship, protocol::input::FIRE);
      world.applyInput(ship, protocol::input::FIRE); // duplicate cannot bypass cooldown
      world.step();
    }
    CHECK(world.registry().view<Projectile>().size() == (rapid ? 10 : 4));
    REQUIRE(world.playerStates().size() == 1);
    CHECK(world.playerStates().front().weapon == 1);
    CHECK(world.playerStates().front().rapidFireMilliseconds == (rapid ? 7500 : 0));
  }
}

TEST_CASE("Server replicates bonus types, collection and equipment state", "[server][world][equipment]") {
  GameWorld world(TICK_RATE, 42U);
  const auto ship = world.spawnPlayer(0);
  const auto item = game::spawnPickup(world.registry(), PickupType::SpreadShot, 300, 100);
  world.step();
  auto changes = world.collectChanges();
  const auto state = std::ranges::find(changes.spawned, item, &protocol::EntityState::id);
  REQUIRE(state != changes.spawned.end());
  CHECK(state->kind == protocol::EntityKind::SpreadShot);
  *world.registry().getComponent<Position>(item) = *world.registry().getComponent<Position>(ship);
  world.step();
  CHECK(world.playerStates().front().weapon == 2);
  changes = world.collectChanges();
  CHECK(std::ranges::find(changes.destroyed, item, &protocol::EntityRemoval::id) != changes.destroyed.end());
  game::applyPickup(world.registry(), ship, PickupType::RapidFire);
  game::spawnPickup(world.registry(), PickupType::Heal, 400, 200);
  world.loadLevel(100);
  CHECK(world.registry().view<Pickup>().empty());
  CHECK(world.registry().getComponent<Equipment>(ship)->weapon == WeaponType::Spread);
  CHECK(world.registry().getComponent<Equipment>(ship)->rapidFireSeconds == 8.0F);
}
