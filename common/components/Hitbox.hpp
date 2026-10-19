#pragma once

namespace rtype::common::components {

struct Hitbox {
  float width = 0.0F;
  float height = 0.0F;
};

} // namespace rtype::common::components

namespace common::components {
using Hitbox = rtype::common::components::Hitbox;
} // namespace common::components
