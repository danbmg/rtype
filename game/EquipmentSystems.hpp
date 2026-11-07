#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/components/Equipment.hpp"
#include "engine/ecs/Registry.hpp"

namespace game {

constexpr float RAPID_FIRE_SECONDS = 8.0F;
constexpr float SHIELD_SECONDS = 12.0F;
constexpr int MAX_PLAYER_LIVES = 9;
constexpr float TROPHY_SIZE = 40.0F;
constexpr std::uint32_t TROPHY_POINTS = 10000;

engine::ecs::Entity spawnPickup(engine::ecs::Registry& registry,
                                rtype::common::components::PickupType type, float x, float y);
bool applyPickup(engine::ecs::Registry& registry, engine::ecs::Entity player,
                  rtype::common::components::PickupType type);
std::size_t collectPickups(engine::ecs::Registry& registry);
void updateEquipment(engine::ecs::Registry& registry, float deltaSeconds);
float weaponCooldownSeconds(const engine::ecs::Registry& registry, engine::ecs::Entity player);
std::vector<engine::ecs::Entity> fireEquippedWeapon(engine::ecs::Registry& registry,
                                                  engine::ecs::Entity player, float speed);

} // namespace game
