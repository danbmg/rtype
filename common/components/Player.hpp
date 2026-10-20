#pragma once

#include <cstdint>

namespace rtype::common::components {

// Fixed-width fields: the same values are serialized over the network, so their size must not
// depend on the platform (std::size_t is 4 bytes on some targets and 8 on others).
struct Player {
  std::uint32_t id = 0;
  int lives = 3;
  std::uint32_t score = 0;
};

} // namespace rtype::common::components

namespace common::components {
using Player = rtype::common::components::Player;
} // namespace common::components
