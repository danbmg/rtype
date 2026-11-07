#include "engine/input/SFMLInput.hpp"

#include <SFML/Window/Keyboard.hpp>

namespace engine::input {

void SFMLInput::update(bool focused) {
  using K = sf::Keyboard::Key;
  // Same order as engine::input::Key.
  static constexpr std::array<K, KEY_COUNT> KEYS{
      K::Up,     K::Down,    K::Left,     K::Right,    K::Space,  K::W,      K::A,
      K::S,      K::D,       K::Z,        K::Q,        K::Enter,  K::Escape, K::Tab,
      K::M,      K::Equal,   K::Hyphen,   K::F1,       K::B,      K::C,      K::E,
      K::F,      K::G,       K::H,        K::I,        K::J,      K::K,      K::L,
      K::N,      K::O,       K::P,        K::R,        K::T,      K::U,      K::V,
      K::X,      K::Y,       K::Num0,     K::Num1,     K::Num2,   K::Num3,   K::Num4,
      K::Num5,   K::Num6,    K::Num7,     K::Num8,     K::Num9,   K::LShift, K::RShift,
      K::LControl, K::RControl, K::LAlt,  K::RAlt,     K::Backspace, K::F2,  K::F3,
      K::F4,     K::F5};
  _previous = _current;
  for (std::size_t index = 0; index < KEYS.size(); ++index) {
    _current[index] = focused && sf::Keyboard::isKeyPressed(KEYS[index]);
  }
  _current[static_cast<std::size_t>(Key::Add)] |= focused && sf::Keyboard::isKeyPressed(K::Add);
  _current[static_cast<std::size_t>(Key::Subtract)] |=
      focused && sf::Keyboard::isKeyPressed(K::Subtract);
}

bool SFMLInput::down(Key key) const { return _current.at(static_cast<std::size_t>(key)); }
bool SFMLInput::pressed(Key key) const {
  const auto index = static_cast<std::size_t>(key);
  return _current.at(index) && !_previous.at(index);
}

std::optional<Key> SFMLInput::anyPressed() const {
  for (std::size_t index = 0; index < KEY_COUNT; ++index) {
    if (_current[index] && !_previous[index]) {
      return static_cast<Key>(index);
    }
  }
  return std::nullopt;
}

} // namespace engine::input
