#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace engine::input {

// Keys the engine reports. The first ones keep their historical order; any key below can be
// bound to an action by the game (key remapping).
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
  B,
  C,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  N,
  O,
  P,
  R,
  T,
  U,
  V,
  X,
  Y,
  Num0,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,
  LShift,
  RShift,
  LControl,
  RControl,
  LAlt,
  RAlt,
  Backspace,
  F2,
  F3,
  F4,
  F5,
  Count
};

constexpr std::size_t KEY_COUNT = static_cast<std::size_t>(Key::Count);

// Label of a key for menus and the on-screen controls reminder.
constexpr std::string_view keyName(Key key) {
  constexpr std::array<std::string_view, KEY_COUNT> NAMES{
      "UP",    "DOWN",   "LEFT",   "RIGHT",  "SPACE",  "W",     "A",         "S",  "D",  "Z",
      "Q",     "ENTER",  "ESCAPE", "TAB",    "M",      "+",     "-",         "F1", "B",  "C",
      "E",     "F",      "G",      "H",      "I",      "J",     "K",         "L",  "N",  "O",
      "P",     "R",      "T",      "U",      "V",      "X",     "Y",         "0",  "1",  "2",
      "3",     "4",      "5",      "6",      "7",      "8",     "9",         "LSHIFT", "RSHIFT",
      "LCTRL", "RCTRL",  "LALT",   "RALT",   "BACKSPACE", "F2", "F3",        "F4", "F5"};
  const auto index = static_cast<std::size_t>(key);
  return index < KEY_COUNT ? NAMES[index] : "?";
}

class IInput {
public:
  virtual ~IInput() = default;
  virtual void update(bool focused) = 0;
  virtual bool down(Key key) const = 0;
  virtual bool pressed(Key key) const = 0;
  // A key pressed since the previous update, if any: used to bind a key to an action.
  virtual std::optional<Key> anyPressed() const = 0;
};

} // namespace engine::input
