#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace client {

struct Options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 4242;
  std::filesystem::path assets = "assets";
  bool mute = false;
  bool help = false;
};

// Arguments exclude argv[0]. Invalid arguments throw std::invalid_argument.
Options parseOptions(std::span<const std::string_view> arguments,
                     const std::filesystem::path& executable);

} // namespace client
