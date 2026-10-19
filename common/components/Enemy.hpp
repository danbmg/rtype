#pragma once

namespace rtype::common::components {

struct Enemy {
  int points = 0;
  int pattern = 0;
};

} // namespace rtype::common::components

namespace common::components {
using Enemy = rtype::common::components::Enemy;
} // namespace common::components
