#include <charconv>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>

#include "engine/core/Logger.hpp"
#include "server/Server.hpp"

namespace {

constexpr int EXIT_ERROR = 84;

// Written by the signal handler, so it has to be a global sig_atomic_t: that is the only kind of
// variable a signal handler may portably touch.
volatile std::sig_atomic_t stopRequested = 0;

extern "C" void onSignal(int /*signal*/) { stopRequested = 1; }

void printUsage(std::ostream& output) {
  output << "USAGE: ./r-type_server [-p port]\n"
            "  -p port   UDP port to listen on (default "
         << rtype::protocol::DEFAULT_PORT << ")\n"
         << "  -h        show this help\n";
}

std::optional<std::uint16_t> parsePort(std::string_view text) {
  unsigned int port = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), port);
  if (error != std::errc{} || end != text.data() + text.size() || port == 0 || port > 65535) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(port);
}

} // namespace

int main(int argc, char** argv) {
  server::ServerConfig config;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "-h" || argument == "--help") {
      printUsage(std::cout);
      return 0;
    }
    if (argument == "-p" && index + 1 < argc) {
      const auto port = parsePort(argv[++index]);
      if (!port) {
        engine::core::Logger::error("invalid port: " + std::string(argv[index]));
        return EXIT_ERROR;
      }
      config.port = *port;
      continue;
    }
    printUsage(std::cerr);
    return EXIT_ERROR;
  }
  config.seed = std::random_device{}();

  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
  try {
    server::Server server(config);
    server.start();
    while (stopRequested == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    engine::core::Logger::info("shutting down");
    server.stop();
  } catch (const std::exception& exception) {
    engine::core::Logger::error(exception.what());
    return EXIT_ERROR;
  }
  return 0;
}
