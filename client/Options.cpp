#include "client/Options.hpp"

#include <charconv>
#include <stdexcept>

namespace client {

Options parseOptions(std::span<const std::string_view> arguments,
                     const std::filesystem::path& executable) {
  Options options;
  const auto besideExecutable = std::filesystem::absolute(executable).parent_path() / "assets";
  if (std::filesystem::is_directory(besideExecutable)) {
    options.assets = besideExecutable;
  }
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const auto flag = arguments[index];
    if (flag == "--help") {
      options.help = true;
    } else if (flag == "--mute") {
      options.mute = true;
    } else if (flag == "-h" || flag == "-p" || flag == "--assets") {
      if (++index == arguments.size() || arguments[index].empty()) {
        throw std::invalid_argument("Missing value for " + std::string(flag));
      }
      const auto value = arguments[index];
      if (flag == "-h") {
        options.host = value;
      } else if (flag == "--assets") {
        options.assets = value;
      } else {
        unsigned int port = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), port);
        if (error != std::errc{} || end != value.data() + value.size() || port == 0 ||
            port > 65535) {
          throw std::invalid_argument("Port must be an integer between 1 and 65535");
        }
        options.port = static_cast<std::uint16_t>(port);
      }
    } else {
      throw std::invalid_argument("Unknown option: " + std::string(flag));
    }
  }
  return options;
}

} // namespace client
