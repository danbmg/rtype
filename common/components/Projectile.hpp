#pragma once

namespace rtype::common::components {

struct Projectile {
  int damage = 1;
  bool friendly = true;
  int ownerId = 0;
};

} // namespace rtype::common::components

namespace common::components {
using Projectile = rtype::common::components::Projectile;
} // namespace common::components
