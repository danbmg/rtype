#include <algorithm>
#include <map>

#include <catch2/catch_test_macros.hpp>

#include "server/GameWorld.hpp"

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
