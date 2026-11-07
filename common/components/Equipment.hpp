#pragma once

#include <cstdint>

namespace rtype::common::components {

enum class WeaponType : std::uint8_t { Standard = 0, Double = 1, Spread = 2, Heavy = 3 };
// Trophy is the Summoner's Cup dropped by the final boss of the story: picking it up wins the game.
enum class PickupType : std::uint8_t {
  ExtraLife = 1,
  Heal = 2,
  RapidFire = 3,
  DoubleShot = 4,
  SpreadShot = 5,
  HeavyShot = 6,
  Shield = 7,
  Trophy = 8
};

struct Equipment {
  WeaponType weapon = WeaponType::Standard;
  float rapidFireSeconds = 0.0F;
  float shieldSeconds = 0.0F;       // > 0: the next hit is absorbed
  float invulnerableSeconds = 0.0F; // > 0: just respawned, enemy fire and contact do nothing
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
