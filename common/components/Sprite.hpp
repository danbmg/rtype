#pragma once

#include <string>

namespace rtype::common::components {

struct Sprite {
  std::string textureId;
  int layer = 0;
  bool flipped = false;
};

} // namespace rtype::common::components

namespace common::components {
using Sprite = rtype::common::components::Sprite;
} // namespace common::components
