#pragma once

namespace rtype::common::components {

struct Position {
  float x = 0.0F;
  float y = 0.0F;
};

} // namespace rtype::common::components

namespace common::components {
using Position = rtype::common::components::Position;
} // namespace common::components
