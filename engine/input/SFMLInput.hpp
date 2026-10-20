#pragma once

#include <array>

#include "engine/input/IInput.hpp"

namespace engine::input {

class SFMLInput final : public IInput {
public:
  void update(bool focused) override;
  bool down(Key key) const override;
  bool pressed(Key key) const override;

private:
  static constexpr auto KEY_COUNT = static_cast<std::size_t>(Key::Count);
  std::array<bool, KEY_COUNT> _current{};
  std::array<bool, KEY_COUNT> _previous{};
};

} // namespace engine::input
