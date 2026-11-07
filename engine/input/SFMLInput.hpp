#pragma once

#include <array>

#include "engine/input/IInput.hpp"

namespace engine::input {

class SFMLInput final : public IInput {
public:
  void update(bool focused) override;
  bool down(Key key) const override;
  bool pressed(Key key) const override;
  std::optional<Key> anyPressed() const override;

private:
  std::array<bool, KEY_COUNT> _current{};
  std::array<bool, KEY_COUNT> _previous{};
};

} // namespace engine::input
