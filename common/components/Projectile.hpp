#pragma once

#include <cstdint>

namespace rtype::common::components {

struct Projectile {
  int damage = 1;
  bool friendly = true;
  // Player::id of the shooter, or NO_OWNER for an enemy shot.
  std::int32_t ownerId = 0;

  static constexpr std::int32_t NO_OWNER = -1;
};

} // namespace rtype::common::components

namespace common::components {
using Projectile = rtype::common::components::Projectile;
} // namespace common::components
