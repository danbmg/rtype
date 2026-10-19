#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "engine/ecs/Registry.hpp"
#include "engine/ecs/SparseArray.hpp"

namespace {

struct Position {
  float x;
  float y;
};

struct Velocity {
  float x;
  float y;
};

struct Unregistered {};

} // namespace

TEST_CASE("SparseArray grows to indexed inserts and supports erase", "[ecs][sparse-array]") {
  engine::ecs::SparseArray<int> components;
  components.insert(3, 42);

  REQUIRE(components.size() == 4);
  CHECK_FALSE(components[0].has_value());
  CHECK(components[3] == 42);

  components.erase(3);
  CHECK_FALSE(components[3].has_value());
  CHECK_NOTHROW(components.erase(99));
}

TEST_CASE("Registry creates live entities and stores components", "[ecs][registry]") {
  engine::ecs::Registry registry;

  const auto entity = registry.spawnEntity();
  REQUIRE(registry.containsEntity(entity));
  registry.addComponent(entity, Position{1.0F, 2.0F});

  const auto* position = registry.getComponent<Position>(entity);
  REQUIRE(position != nullptr);
  CHECK(position->x == 1.0F);
  CHECK(position->y == 2.0F);
}

TEST_CASE("Registry views match all requested components", "[ecs][registry][view]") {
  engine::ecs::Registry registry;
  const auto complete = registry.spawnEntity();
  const auto partial = registry.spawnEntity();
  registry.addComponent(complete, Position{});
  registry.addComponent(complete, Velocity{});
  registry.addComponent(partial, Position{});

  const auto matching = registry.view<Position, Velocity>();
  REQUIRE(matching.size() == 1);
  CHECK(matching.front() == complete);
  CHECK(registry.view<Unregistered>().empty());
}

TEST_CASE("Removing and killing entities clear their components", "[ecs][registry]") {
  engine::ecs::Registry registry;
  const auto entity = registry.spawnEntity();
  registry.addComponent(entity, Position{});
  registry.addComponent(entity, Velocity{});

  registry.removeComponent<Velocity>(entity);
  CHECK(registry.getComponent<Velocity>(entity) == nullptr);
  CHECK(registry.getComponent<Position>(entity) != nullptr);

  registry.killEntity(entity);
  CHECK_FALSE(registry.containsEntity(entity));
  CHECK(registry.getComponent<Position>(entity) == nullptr);
  CHECK(registry.view<Position>().empty());

  registry.killEntity(entity);
  CHECK_FALSE(registry.containsEntity(entity));
}

TEST_CASE("Registry rejects adding a component to a dead or unknown entity",
          "[ecs][registry][invalid-input]") {
  engine::ecs::Registry registry;

  CHECK_THROWS_AS(registry.addComponent(42, Position{}), std::invalid_argument);

  const auto entity = registry.spawnEntity();
  registry.killEntity(entity);
  CHECK_THROWS_AS(registry.addComponent(entity, Position{}), std::invalid_argument);
}

TEST_CASE("Clearing a registry resets entities and component storage", "[ecs][registry]") {
  engine::ecs::Registry registry;
  const auto entity = registry.spawnEntity();
  registry.addComponent(entity, Position{});

  registry.clear();

  CHECK(registry.view<Position>().empty());
  CHECK_FALSE(registry.containsEntity(entity));
  const auto nextEntity = registry.spawnEntity();
  CHECK(nextEntity == entity);
}
