#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "common/protocol/Packets.hpp"

namespace client {

struct Options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 4242;
  std::filesystem::path assets = "assets";
  bool mute = false;
  bool help = false;
  bool join = false; // connect at once instead of showing the menu
  rtype::protocol::GameMode mode = rtype::protocol::GameMode::Story;
  rtype::protocol::Difficulty difficulty = rtype::protocol::Difficulty::Normal;
};

// Arguments exclude argv[0]. Invalid arguments throw std::invalid_argument.
Options parseOptions(std::span<const std::string_view> arguments,
                     const std::filesystem::path& executable);

} // namespace client
