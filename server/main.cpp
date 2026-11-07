#include <charconv>
#include <chrono>
#include <csignal>
#include <exception>
#include <filesystem>
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
  output << "USAGE: ./r-type_server [-p port] [--level number] [--content directory] "
            "[--check-content]\n"
            "  -p port              UDP port to listen on (default "
         << rtype::protocol::DEFAULT_PORT << ")\n"
         << "  --level number       level every game starts at (default 1)\n"
         << "  --content directory  levels, enemies and bosses (default assets/levels)\n"
         << "  --check-content      validate the content files, print a summary and exit\n"
         << "  -h                   show this help\n";
}

// assets/levels from the working directory, else beside the executable (like the client's
// assets), else nothing: the server then plays generated levels.
std::optional<std::filesystem::path> findContent(const char* executable) {
  const std::filesystem::path local = "assets/levels";
  if (std::filesystem::is_directory(local)) {
    return local;
  }
  std::error_code error;
  const auto beside =
      std::filesystem::absolute(executable, error).parent_path() / "assets" / "levels";
  if (!error && std::filesystem::is_directory(beside)) {
    return beside;
  }
  return std::nullopt;
}

void printSummary(const game::GameContent& content) {
  std::cout << "Campaign \"" << content.campaignName << "\": "
            << content.story.finalLevel().value_or(0) << " levels\n";
  for (std::uint32_t number = 1; number <= content.story.finalLevel().value_or(0); ++number) {
    const auto level = content.story.get(number);
    std::cout << "  " << number << ". " << level.title << " (" << level.waves.size()
              << " waves";
    for (const auto& wave : level.waves) {
      if (wave.boss) {
        std::cout << ", boss " << wave.bossType;
      }
    }
    std::cout << ")\n";
  }
  std::cout << content.enemies.size() << " enemies:";
  for (const auto& enemy : content.enemies) {
    std::cout << ' ' << enemy.name;
  }
  std::cout << '\n' << content.bosses.size() << " bosses:";
  for (const auto& boss : content.bosses) {
    std::cout << ' ' << boss.name;
  }
  std::cout << "\nEndless mode: " << content.endless.generator().enemies.size()
            << " enemies, " << content.endless.generator().bosses.size() << " bosses\n";
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
  std::optional<std::filesystem::path> contentDirectory;
  bool checkOnly = false;
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
    if (argument == "--level" && index + 1 < argc) {
      const std::string_view text = argv[++index];
      std::uint32_t level = 0;
      const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), level);
      if (error != std::errc{} || end != text.data() + text.size() || level == 0) {
        engine::core::Logger::error("invalid level: " + std::string(text));
        return EXIT_ERROR;
      }
      config.initialLevel = level;
      continue;
    }
    if (argument == "--content" && index + 1 < argc) {
      contentDirectory = argv[++index];
      continue;
    }
    if (argument == "--check-content") {
      checkOnly = true;
      continue;
    }
    printUsage(std::cerr);
    return EXIT_ERROR;
  }
  config.seed = std::random_device{}();

  // A broken content file stops the server with the reason: better than a game that silently
  // differs from what the designer wrote.
  if (!contentDirectory) {
    contentDirectory = findContent(argv[0]);
  }
  if (contentDirectory) {
    try {
      config.content = game::loadContent(*contentDirectory);
      engine::core::Logger::info("content loaded from " + contentDirectory->string() + ": \"" +
                                 config.content.campaignName + "\", " +
                                 std::to_string(config.content.story.finalLevel().value_or(0)) +
                                 " story levels");
    } catch (const game::ContentError& error) {
      engine::core::Logger::error(std::string("content error: ") + error.what());
      return EXIT_ERROR;
    }
  } else {
    engine::core::Logger::warning("no assets/levels directory: playing generated levels");
  }
  if (checkOnly) {
    if (!contentDirectory) {
      engine::core::Logger::error("no content directory to check (use --content directory)");
      return EXIT_ERROR;
    }
    printSummary(config.content);
    return 0;
  }

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
