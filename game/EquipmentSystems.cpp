#include "game/EquipmentSystems.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "game/GameSystems.hpp"

namespace game {
using namespace rtype::common::components;

namespace {
bool valid(PickupType type) {
  return type >= PickupType::ExtraLife && type <= PickupType::Trophy;
}
} // namespace

engine::ecs::Entity spawnPickup(engine::ecs::Registry& registry, PickupType type, float x, float y) {
  if (!valid(type) || !std::isfinite(x) || !std::isfinite(y)) {
    throw std::invalid_argument("invalid pickup type or position");
  }
  const auto entity = registry.spawnEntity();
  registry.addComponent(entity, Position{x, y});
  if (type == PickupType::Trophy) {
    // The Cup waits where the final boss fell: the game is won when a player flies into it.
    registry.addComponent(entity, Velocity{0.0F, 0.0F});
    registry.addComponent(entity, Hitbox{TROPHY_SIZE, TROPHY_SIZE});
    registry.addComponent(entity, Pickup{type, std::numeric_limits<float>::infinity()});
    return entity;
  }
  registry.addComponent(entity, Velocity{-45.0F, 0.0F});
  registry.addComponent(entity, Hitbox{20.0F, 20.0F});
  registry.addComponent(entity, Pickup{type, 12.0F});
  return entity;
}

bool applyPickup(engine::ecs::Registry& registry, engine::ecs::Entity entity, PickupType type) {
  if (!valid(type)) {
    throw std::invalid_argument("invalid pickup type");
  }
  auto* player = registry.getComponent<Player>(entity);
  auto* health = registry.getComponent<Health>(entity);
  if (player == nullptr || health == nullptr || health->current <= 0 || player->lives <= 0) {
    return false;
  }
  if (registry.getComponent<Equipment>(entity) == nullptr) {
    registry.addComponent(entity, Equipment{});
  }
  auto* equipment = registry.getComponent<Equipment>(entity);
  switch (type) {
  case PickupType::ExtraLife:
    player->lives = std::min(MAX_PLAYER_LIVES, player->lives + 1);
    break;
  case PickupType::Heal:
    health->current = health->max;
    break;
  case PickupType::RapidFire:
    equipment->rapidFireSeconds = RAPID_FIRE_SECONDS;
    break;
  case PickupType::DoubleShot:
    equipment->weapon = WeaponType::Double;
    break;
  case PickupType::SpreadShot:
    equipment->weapon = WeaponType::Spread;
    break;
  case PickupType::HeavyShot:
    equipment->weapon = WeaponType::Heavy;
    break;
  case PickupType::Shield:
    equipment->shieldSeconds = SHIELD_SECONDS;
    break;
  case PickupType::Trophy:
    player->score += TROPHY_POINTS;
    break;
  }
  return true;
}

std::size_t collectPickups(engine::ecs::Registry& registry) {
  std::size_t collected = 0;
  for (const auto pickup : registry.view<Pickup, Position, Hitbox>()) {
    const auto position = *registry.getComponent<Position>(pickup);
    const auto box = *registry.getComponent<Hitbox>(pickup);
    for (const auto player : registry.view<Player, Position, Hitbox, Health>()) {
      const auto* ship = registry.getComponent<Position>(player);
      const auto* shipBox = registry.getComponent<Hitbox>(player);
      if (ship->x < position.x + box.width && ship->x + shipBox->width > position.x &&
          ship->y < position.y + box.height && ship->y + shipBox->height > position.y &&
          applyPickup(registry, player, registry.getComponent<Pickup>(pickup)->type)) {
        registry.killEntity(pickup);
        ++collected;
        break; // one item benefits exactly one player
      }
    }
  }
  return collected;
}

void updateEquipment(engine::ecs::Registry& registry, float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
    throw std::invalid_argument("equipment delta must be finite and non-negative");
  }
  for (const auto player : registry.view<Equipment>()) {
    auto* equipment = registry.getComponent<Equipment>(player);
    equipment->rapidFireSeconds = std::max(0.0F, equipment->rapidFireSeconds - deltaSeconds);
    equipment->shieldSeconds = std::max(0.0F, equipment->shieldSeconds - deltaSeconds);
    equipment->invulnerableSeconds =
        std::max(0.0F, equipment->invulnerableSeconds - deltaSeconds);
  }
  for (const auto entity : registry.view<Pickup>()) {
    auto* pickup = registry.getComponent<Pickup>(entity);
    pickup->remainingSeconds -= deltaSeconds;
    if (pickup->remainingSeconds <= 0.0F) {
      registry.killEntity(entity);
    }
  }
}

float weaponCooldownSeconds(const engine::ecs::Registry& registry, engine::ecs::Entity player) {
  const auto* equipment = registry.getComponent<Equipment>(player);
  const float base = equipment != nullptr && equipment->weapon == WeaponType::Heavy ? 0.5F : 0.25F;
  return equipment != nullptr && equipment->rapidFireSeconds > 0.0F ? base * 0.4F : base;
}

std::vector<engine::ecs::Entity> fireEquippedWeapon(engine::ecs::Registry& registry,
                                                  engine::ecs::Entity player, float speed) {
  const auto* ship = registry.getComponent<Player>(player);
  const auto* health = registry.getComponent<Health>(player);
  if (ship == nullptr || health == nullptr || health->current <= 0 || ship->lives <= 0 ||
      !std::isfinite(speed) || speed <= 0.0F) {
    throw std::invalid_argument("weapon requires a living player and positive finite speed");
  }
  const auto* equipment = registry.getComponent<Equipment>(player);
  const auto weapon = equipment != nullptr ? equipment->weapon : WeaponType::Standard;
  std::vector<engine::ecs::Entity> shots;
  if (weapon == WeaponType::Double) {
    for (const float offset : {-6.0F, 6.0F}) {
      const auto shot = firePlayerProjectile(registry, player, speed);
      registry.getComponent<Position>(shot)->y += offset;
      shots.push_back(shot);
    }
  } else if (weapon == WeaponType::Spread) {
    for (const float angle : {-0.25F, 0.0F, 0.25F}) {
      shots.push_back(fireProjectile(registry, player, true, static_cast<std::int32_t>(ship->id),
                                     speed * std::cos(angle), speed * std::sin(angle)));
    }
  } else {
    shots.push_back(firePlayerProjectile(registry, player, speed,
                                         weapon == WeaponType::Heavy ? 3 : 1));
  }
  return shots;
}

} // namespace game
