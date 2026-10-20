// r-type_bot: headless test clients. Each bot connects through NetworkClient, flies at random,
// fires, and reports what it sees once per second. Used to load the server without a window.
#include <charconv>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#include "client/net/NetworkClient.hpp"
#include "engine/core/Logger.hpp"

namespace {

using engine::core::Logger;
using namespace std::chrono_literals;

constexpr int EXIT_ERROR = 84;
constexpr int MAX_BOTS = 8;

// Written by the signal handler: a global sig_atomic_t is the only portable choice.
volatile std::sig_atomic_t stopRequested = 0;

extern "C" void onSignal(int /*signal*/) { stopRequested = 1; }

struct Options {
  std::string host = "127.0.0.1";
  std::uint16_t port = rtype::protocol::DEFAULT_PORT;
  int bots = 1;
  int seconds = 10; // 0: until Ctrl+C
};

struct Bot {
  std::unique_ptr<client::net::NetworkClient> network;
  client::net::InputState input;
  int number = 0;
  int kills = 0;
  int spawns = 0;
};

void printUsage(std::ostream& output) {
  output << "USAGE: ./r-type_bot [-h host] [-p port] [-n bots] [-d seconds]\n"
            "  -h host     server address (default 127.0.0.1)\n"
            "  -p port     server UDP port (default 4242)\n"
            "  -n bots     number of bots, 1 to 8 (default 1; more than 4 tests REJECT)\n"
            "  -d seconds  run time, 0 for until Ctrl+C (default 10)\n";
}

std::optional<int> parseInt(std::string_view text, int min, int max) {
  int value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value < min || value > max) {
    return std::nullopt;
  }
  return value;
}

std::optional<Options> parseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view flag = argv[index];
    if (index + 1 >= argc) {
      return std::nullopt;
    }
    const std::string_view value = argv[++index];
    if (flag == "-h") {
      options.host = value;
    } else if (flag == "-p") {
      const auto port = parseInt(value, 1, 65535);
      if (!port) {
        return std::nullopt;
      }
      options.port = static_cast<std::uint16_t>(*port);
    } else if (flag == "-n") {
      const auto bots = parseInt(value, 1, MAX_BOTS);
      if (!bots) {
        return std::nullopt;
      }
      options.bots = *bots;
    } else if (flag == "-d") {
      const auto seconds = parseInt(value, 0, 24 * 3600);
      if (!seconds) {
        return std::nullopt;
      }
      options.seconds = *seconds;
    } else {
      return std::nullopt;
    }
  }
  return options;
}

void handleEvents(Bot& bot) {
  for (const auto& event : bot.network->pollEvents()) {
    std::visit(
        [&bot](const auto& value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, client::net::EntitySpawned>) {
            ++bot.spawns;
          } else if constexpr (std::is_same_v<T, client::net::EntityDestroyed>) {
            bot.kills += value.reason == rtype::protocol::DestroyReason::Killed ? 1 : 0;
          } else if constexpr (std::is_same_v<T, client::net::PlayerLeft>) {
            Logger::info(
                "bot " + std::to_string(bot.number) + ": player " +
                std::to_string(value.playerId + 1) +
                (value.reason == rtype::protocol::LeaveReason::Quit ? " quit" : " timed out"));
          } else {
            Logger::warning("bot " + std::to_string(bot.number) + ": connection lost");
          }
        },
        event);
  }
}

void report(const Bot& bot) {
  const auto& network = *bot.network;
  std::string line = "bot " + std::to_string(bot.number) + " (player " +
                     std::to_string(network.playerId() + 1) +
                     "): " + std::to_string(network.entities().size()) + " entities, ping " +
                     std::to_string(network.ping().count()) + " ms, " + std::to_string(bot.spawns) +
                     " spawned, " + std::to_string(bot.kills) + " destroyed";
  for (const auto& player : network.players()) {
    if (player.playerId == network.playerId()) {
      line += ", lives " + std::to_string(player.lives) + ", score " + std::to_string(player.score);
    }
  }
  Logger::info(line);
}

} // namespace

int main(int argc, char** argv) {
  const auto options = parseOptions(argc, argv);
  if (!options) {
    printUsage(std::cerr);
    return EXIT_ERROR;
  }
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);

  std::vector<Bot> bots;
  for (int number = 1; number <= options->bots; ++number) {
    Bot bot{std::make_unique<client::net::NetworkClient>(), {}, number};
    if (bot.network->connectBlocking(options->host, options->port)) {
      Logger::info("bot " + std::to_string(number) + " connected as player " +
                   std::to_string(bot.network->playerId() + 1));
      bots.push_back(std::move(bot));
    } else {
      Logger::warning("bot " + std::to_string(number) + ": " + bot.network->error());
    }
  }
  if (bots.empty()) {
    return EXIT_ERROR;
  }

  std::mt19937 random(std::random_device{}());
  std::bernoulli_distribution coin(0.5);
  const auto start = std::chrono::steady_clock::now();
  auto nextReport = start + 1s;
  auto nextTurn = start;
  while (stopRequested == 0 &&
         (options->seconds == 0 ||
          std::chrono::steady_clock::now() - start < std::chrono::seconds(options->seconds))) {
    const auto now = std::chrono::steady_clock::now();
    const bool turn = now >= nextTurn;
    for (auto& bot : bots) {
      bot.network->update();
      if (turn) {
        bot.input = {coin(random), coin(random), coin(random), coin(random), coin(random)};
      }
      bot.network->sendInput(bot.input);
      handleEvents(bot);
    }
    if (turn) {
      nextTurn = now + 500ms;
    }
    if (now >= nextReport) {
      nextReport = now + 1s;
      for (const auto& bot : bots) {
        if (bot.network->isConnected()) {
          report(bot);
        }
      }
    }
    std::this_thread::sleep_for(16ms);
  }

  int lost = 0;
  for (auto& bot : bots) {
    lost += bot.network->isConnected() ? 0 : 1;
    bot.network->disconnect();
  }
  return lost == 0 ? 0 : EXIT_ERROR;
}
