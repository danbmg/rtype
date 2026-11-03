#pragma once

#include <cstdint>

namespace rtype::common::components {

enum class WeaponType : std::uint8_t { Standard = 0, Double = 1, Spread = 2, Heavy = 3 };
enum class PickupType : std::uint8_t {
  ExtraLife = 1, Heal = 2, RapidFire = 3, DoubleShot = 4, SpreadShot = 5, HeavyShot = 6
};

struct Equipment {
  WeaponType weapon = WeaponType::Standard;
  float rapidFireSeconds = 0.0F;
};

struct Pickup {
  PickupType type = PickupType::Heal;
  float remainingSeconds = 12.0F;
};

// Assigned when an enemy spawns, emitted only when it dies (never when it escapes).
struct LootDrop {
  PickupType type = PickupType::Heal;
};

} // namespace rtype::common::components
