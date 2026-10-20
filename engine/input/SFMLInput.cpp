#include "engine/input/SFMLInput.hpp"

#include <SFML/Window/Keyboard.hpp>

namespace engine::input {

void SFMLInput::update(bool focused) {
  using K = sf::Keyboard::Key;
  static constexpr std::array KEYS{K::Up,     K::Down, K::Left, K::Right, K::Space,  K::W,
                                   K::A,      K::S,    K::D,    K::Z,     K::Q,      K::Enter,
                                   K::Escape, K::Tab,  K::M,    K::Equal, K::Hyphen, K::F1};
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

} // namespace engine::input
