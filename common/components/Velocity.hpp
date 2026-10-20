#pragma once

namespace rtype::common::components {

struct Velocity {
  float vx = 0.0F;
  float vy = 0.0F;
};

} // namespace rtype::common::components

namespace common::components {
using Velocity = rtype::common::components::Velocity;
} // namespace common::components
