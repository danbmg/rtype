#pragma once

#include <cstddef>

namespace rtype::common::components {

struct Player {
  std::size_t id = 0;
  int lives = 3;
};

} // namespace rtype::common::components

namespace common::components {
using Player = rtype::common::components::Player;
} // namespace common::components
