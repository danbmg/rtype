#include <cmath>
#include <limits>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game/EquipmentSystems.hpp"
#include "game/GameSystems.hpp"
#include "game/LevelDirector.hpp"

namespace {
using namespace rtype::common::components;
struct World {
  engine::ecs::Registry registry;
  World() { game::registerSystems(registry); }
  engine::ecs::Entity player(std::uint32_t id = 0) {
    const auto entity = registry.spawnEntity();
    registry.addComponent(entity, Player{id, 3, 0});
    registry.addComponent(entity, Health{2, 3});
    registry.addComponent(entity, Position{60.0F, 100.0F});
    registry.addComponent(entity, Hitbox{33.0F, 17.0F});
    registry.addComponent(entity, Equipment{});
    return entity;
  }
};
} // namespace

TEST_CASE("Weapons create distinct shots with the player's ownership", "[game][equipment]") {
  World world;
  const auto player = world.player(2);
  auto shots = game::fireEquippedWeapon(world.registry, player, 600);
  REQUIRE(shots.size() == 1);
  CHECK(world.registry.getComponent<Projectile>(shots.front())->damage == 1);
  game::applyPickup(world.registry, player, PickupType::DoubleShot);
  shots = game::fireEquippedWeapon(world.registry, player, 600);
  REQUIRE(shots.size() == 2);
  CHECK(world.registry.getComponent<Position>(shots[0])->y !=
        world.registry.getComponent<Position>(shots[1])->y);
  game::applyPickup(world.registry, player, PickupType::SpreadShot);
  shots = game::fireEquippedWeapon(world.registry, player, 600);
  REQUIRE(shots.size() == 3);
  CHECK(world.registry.getComponent<Velocity>(shots[0])->vy < 0);
  CHECK(world.registry.getComponent<Velocity>(shots[1])->vy == 0);
  CHECK(world.registry.getComponent<Velocity>(shots[2])->vy > 0);
  for (const auto shot : shots) {
    const auto* projectile = world.registry.getComponent<Projectile>(shot);
    CHECK(projectile->ownerId == 2);
    CHECK(projectile->friendly);
    const auto* velocity = world.registry.getComponent<Velocity>(shot);
    CHECK(std::hypot(velocity->vx, velocity->vy) == Catch::Approx(600.0F));
  }
  game::applyPickup(world.registry, player, PickupType::HeavyShot);
  shots = game::fireEquippedWeapon(world.registry, player, 600);
  REQUIRE(shots.size() == 1);
  CHECK(world.registry.getComponent<Projectile>(shots.front())->damage == 3);
  CHECK(game::weaponCooldownSeconds(world.registry, player) == Catch::Approx(0.5F));
}

TEST_CASE("Pickups heal, cap lives, and refresh rather than stack rapid fire", "[game][equipment]") {
  World world;
  const auto player = world.player();
  CHECK(game::applyPickup(world.registry, player, PickupType::Heal));
  CHECK(world.registry.getComponent<Health>(player)->current == 3);
  game::applyPickup(world.registry, player, PickupType::ExtraLife);
  CHECK(world.registry.getComponent<Player>(player)->lives == 4);
  for (int i = 0; i < 20; ++i) {
    game::applyPickup(world.registry, player, PickupType::ExtraLife);
  }
  CHECK(world.registry.getComponent<Player>(player)->lives == game::MAX_PLAYER_LIVES);
  game::applyPickup(world.registry, player, PickupType::RapidFire);
  CHECK(game::weaponCooldownSeconds(world.registry, player) == Catch::Approx(0.1F));
  game::updateEquipment(world.registry, 3.0F);
  CHECK(world.registry.getComponent<Equipment>(player)->rapidFireSeconds == 5.0F);
  game::applyPickup(world.registry, player, PickupType::RapidFire);
  CHECK(world.registry.getComponent<Equipment>(player)->rapidFireSeconds == 8.0F);
  game::updateEquipment(world.registry, 9.0F);
  CHECK(world.registry.getComponent<Equipment>(player)->rapidFireSeconds == 0.0F);
  CHECK(game::weaponCooldownSeconds(world.registry, player) == Catch::Approx(0.25F));
}

TEST_CASE("A collectible benefits one living overlapping player and expires when unclaimed", "[game][equipment]") {
  World world;
  const auto first = world.player(0);
  const auto second = world.player(1);
  const auto item = game::spawnPickup(world.registry, PickupType::ExtraLife, 60, 100);
  CHECK(game::collectPickups(world.registry) == 1);
  CHECK_FALSE(world.registry.containsEntity(item));
  CHECK(world.registry.getComponent<Player>(first)->lives +
        world.registry.getComponent<Player>(second)->lives == 7);
  CHECK(game::collectPickups(world.registry) == 0);
  const auto distant = game::spawnPickup(world.registry, PickupType::Heal, 500, 100);
  CHECK(game::collectPickups(world.registry) == 0);
  game::updateEquipment(world.registry, 12.0F);
  CHECK_FALSE(world.registry.containsEntity(distant));
  world.registry.getComponent<Health>(first)->current = 0;
  CHECK_FALSE(game::applyPickup(world.registry, first, PickupType::ExtraLife));
  CHECK_THROWS_AS(game::spawnPickup(world.registry, static_cast<PickupType>(0), 0, 0),
                  std::invalid_argument);
  CHECK_THROWS_AS(game::updateEquipment(world.registry, -1), std::invalid_argument);
  CHECK_THROWS_AS(game::fireEquippedWeapon(world.registry, first, 600), std::invalid_argument);
}

TEST_CASE("Losing a life resets equipment", "[game][equipment]") {
  World world;
  const auto player = world.player();
  game::applyPickup(world.registry, player, PickupType::SpreadShot);
  game::applyPickup(world.registry, player, PickupType::RapidFire);
  world.registry.getComponent<Health>(player)->current = 0;
  CHECK(game::handlePlayerDeaths(world.registry) == 1);
  CHECK(world.registry.getComponent<Equipment>(player)->weapon == WeaponType::Standard);
  CHECK(world.registry.getComponent<Equipment>(player)->rapidFireSeconds == 0);
  CHECK(world.registry.getComponent<Player>(player)->lives == 2);
}

TEST_CASE("Loot is dropped once on death, never when an enemy escapes", "[game][equipment]") {
  World world;
  const auto enemy = game::spawnEnemy(world.registry, 500, 100, 0, 0);
  world.registry.addComponent(enemy, LootDrop{PickupType::HeavyShot});
  world.registry.getComponent<Health>(enemy)->current = 0;
  CHECK(game::removeDeadEntities(world.registry) == 1);
  REQUIRE(world.registry.view<Pickup>().size() == 1);
  CHECK(world.registry.getComponent<Pickup>(world.registry.view<Pickup>().front())->type ==
        PickupType::HeavyShot);
  game::removeDeadEntities(world.registry);
  CHECK(world.registry.view<Pickup>().size() == 1);
  const auto escaped = game::spawnEnemy(world.registry, -100, 100, 0, 0);
  world.registry.addComponent(escaped, LootDrop{PickupType::ExtraLife});
  game::removeOffscreenEntities(world.registry);
  CHECK_FALSE(world.registry.containsEntity(escaped));
  CHECK(world.registry.view<Pickup>().size() == 1);
}

TEST_CASE("Wave drop probability is configurable and bosses guarantee loot", "[game][equipment]") {
  for (const bool boss : {false, true}) {
    World world;
    game::LevelCatalog catalog;
    game::LevelDefinition level;
    game::WaveDefinition wave;
    wave.enemyCount = 1;
    wave.spawnIntervalSeconds = 0.1F;
    wave.pickupDropChance = 0.0F;
    wave.boss = boss;
    level.waves.push_back(wave);
    catalog.define(level);
    game::LevelDirector director(42, {}, catalog);
    director.update(world.registry, 0.1F);
    REQUIRE(world.registry.view<Enemy>().size() == 1);
    CHECK((world.registry.getComponent<LootDrop>(world.registry.view<Enemy>().front()) != nullptr) == boss);
    auto invalid = level;
    invalid.waves.front().pickupDropChance = 1.1F;
    CHECK_THROWS_AS(catalog.define(invalid), std::invalid_argument);
  }
}
