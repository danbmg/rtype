#pragma once

#include <cstddef>

namespace engine::input {

enum class Key : std::size_t {
  Up,
  Down,
  Left,
  Right,
  Space,
  W,
  A,
  S,
  D,
  Z,
  Q,
  Enter,
  Escape,
  Tab,
  M,
  Add,
  Subtract,
  F1,
  Count
};

class IInput {
public:
  virtual ~IInput() = default;
  virtual void update(bool focused) = 0;
  virtual bool down(Key key) const = 0;
  virtual bool pressed(Key key) const = 0;
};

} // namespace engine::input
