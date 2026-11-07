#pragma once

#include <array>
#include <cstddef>

#include "common/protocol/Packets.hpp"
#include "engine/input/IInput.hpp"

namespace client {

// Actions the player can bind to any key.
enum class Action : std::size_t { Up, Down, Left, Right, Fire, Count };
constexpr std::size_t ACTION_COUNT = static_cast<std::size_t>(Action::Count);
using KeyBindings = std::array<engine::input::Key, ACTION_COUNT>;

constexpr std::array<const char*, ACTION_COUNT> ACTION_NAMES{"UP", "DOWN", "LEFT", "RIGHT",
                                                             "FIRE"};

// Ready-made layouts; the player can also bind each action to any key (Custom).
struct Layout {
  const char* name;
  KeyBindings keys;
};
inline constexpr std::array<Layout, 3> LAYOUTS{{
    {"ARROWS",
     {engine::input::Key::Up, engine::input::Key::Down, engine::input::Key::Left,
      engine::input::Key::Right, engine::input::Key::Space}},
    {"WASD",
     {engine::input::Key::W, engine::input::Key::S, engine::input::Key::A, engine::input::Key::D,
      engine::input::Key::Space}},
    {"ZQSD",
     {engine::input::Key::Z, engine::input::Key::S, engine::input::Key::Q, engine::input::Key::D,
      engine::input::Key::Space}},
}};

// What the player chose in the menu. Presentation and input only: the server decides the game.
struct Settings {
  rtype::protocol::GameMode mode = rtype::protocol::GameMode::Story;
  rtype::protocol::Difficulty difficulty = rtype::protocol::Difficulty::Normal;
  KeyBindings keys = LAYOUTS[0].keys;
  bool autoFire = false;     // fire without holding a key (motor accessibility)
  int musicVolume = 40;      // 0 to 100
  int effectsVolume = 60;    // 0 to 100
  bool muted = false;
  bool largeText = false;
  bool captions = true;      // text captions for the important sounds
  bool screenShake = true;   // can be turned off (motion sensitivity)
};

// Name of the layout the bindings match, or "CUSTOM".
inline const char* layoutName(const KeyBindings& keys) {
  for (const auto& layout : LAYOUTS) {
    if (layout.keys == keys) {
      return layout.name;
    }
  }
  return "CUSTOM";
}

} // namespace client
