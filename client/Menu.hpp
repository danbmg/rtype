#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "client/Settings.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/input/IInput.hpp"
#include "engine/render/IRenderer.hpp"

namespace client {

// The start screen: game mode, difficulty, controls (with key remapping) and accessibility
// options. Up/Down select a line, Left/Right change it, Enter joins or rebinds the keys.
class Menu {
public:
  enum class Result { None, Join, Quit };
  enum class Item : std::size_t {
    Mode,
    Difficulty,
    Controls,
    AutoFire,
    Music,
    Effects,
    TextSize,
    Captions,
    Shake,
    Join,
    Count
  };

  Result update(const engine::input::IInput& input, Settings& settings,
                engine::audio::IAudio& audio);
  void draw(engine::render::IRenderer& renderer, const Settings& settings,
            const std::string& address, const std::string& status, float time) const;

  bool rebinding() const { return _rebinding.has_value(); }
  Item selected() const { return static_cast<Item>(_selected); }

private:
  void change(Settings& settings, int direction);

  std::size_t _selected = static_cast<std::size_t>(Item::Join);
  std::optional<std::size_t> _rebinding; // action waiting for its new key
};

std::string valueOf(Menu::Item item, const Settings& settings);

} // namespace client
