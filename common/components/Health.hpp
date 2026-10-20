#pragma once

namespace rtype::common::components {

struct Health {
  int current = 0;
  int max = 0;
};

} // namespace rtype::common::components

namespace common::components {
using Health = rtype::common::components::Health;
} // namespace common::components
