// r-type_bot: headless test clients. Each bot connects through NetworkClient, flies at random,
// fires, and reports what it sees once per second. Used to load the server without a window.
#include <array>
#include <charconv>
#include <cmath>
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
  rtype::protocol::GameMode mode = rtype::protocol::GameMode::Story;
  bool smart = false; // aim at enemies, dodge shots and go for the Cup instead of flying at random
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
            "  -d seconds  run time, 0 for until Ctrl+C (default 10)\n"
            "  -m mode     story or endless, if the bots start the game (default story)\n"
            "  -b style    random (default) or smart: aims, dodges and catches the Cup\n";
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
    } else if (flag == "-b" && (value == "random" || value == "smart")) {
      options.smart = value == "smart";
    } else if (flag == "-m" && (value == "story" || value == "endless")) {
      options.mode = value == "story" ? rtype::protocol::GameMode::Story
                                      : rtype::protocol::GameMode::Endless;
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

// Smart style: line up with the closest enemy and fire, step aside from incoming shots, and fly
// into the Summoner's Cup when it appears. Uses only what any client sees: entity positions.
client::net::InputState pilot(const client::net::NetworkClient& network) {
  using rtype::protocol::EntityKind;
  client::net::InputState input;
  input.fire = true;
  const client::net::RemoteEntity* ship = nullptr;
  for (const auto& [id, entity] : network.entities()) {
    if (entity.kind == EntityKind::Player && entity.playerId == network.playerId()) {
      ship = &entity;
    }
  }
  if (ship == nullptr) {
    return input;
  }
  const float shipX = ship->x + 16.0F;
  const float shipY = ship->y + 8.0F;
  float targetX = 120.0F + static_cast<float>(network.playerId()) * 40.0F;
  float targetY = shipY;
  float bestDistance = 1e9F;
  float threat = 0.0F;
  for (const auto& [id, entity] : network.entities()) {
    const bool enemy = entity.kind == EntityKind::Bydos || entity.kind == EntityKind::Waver ||
                       entity.kind == EntityKind::Charger || entity.kind == EntityKind::Turret ||
                       entity.kind == EntityKind::Boss || entity.kind == EntityKind::FinalBoss;
    const bool big = entity.kind == EntityKind::Boss || entity.kind == EntityKind::FinalBoss;
    if (entity.kind == EntityKind::Trophy) {
      targetX = entity.x + 20.0F;
      targetY = entity.y + 20.0F;
      bestDistance = -1.0F; // nothing matters more
    } else if (enemy && bestDistance >= 0.0F && entity.x < 790.0F) {
      const float centreY = entity.y + (big ? 48.0F : 12.0F);
      const float distance = std::abs(centreY - shipY) + (entity.x - shipX) * 0.2F;
      if (distance < bestDistance) {
        bestDistance = distance;
        targetY = centreY;
      }
    } else if ((entity.kind == EntityKind::EnemyMissile ||
                entity.kind == EntityKind::BossMissile) &&
               entity.x > shipX - 20.0F && entity.x < shipX + 140.0F &&
               std::abs(entity.y - shipY) < 26.0F) {
      threat = entity.y < shipY ? 1.0F : -1.0F; // step away from the shot
    }
  }
  if (threat != 0.0F && bestDistance >= 0.0F) {
    targetY = shipY + threat * 60.0F;
  }
  input.up = targetY < shipY - 6.0F;
  input.down = targetY > shipY + 6.0F;
  input.left = targetX < shipX - 8.0F;
  input.right = targetX > shipX + 8.0F;
  return input;
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
  constexpr std::array PHASES{"lobby", "intro", "playing", "boss fight", "stage clear",
                              "victory", "game over"};
  const auto& status = network.gameStatus();
  line += " | level " + std::to_string(status.level) + " wave " + std::to_string(status.wave) +
          "/" + std::to_string(status.waveCount) + ", " +
          PHASES.at(static_cast<std::size_t>(status.phase));
  if (status.bossMaxHealth > 0) {
    line += ", boss " + std::to_string(status.bossHealth) + "/" +
            std::to_string(status.bossMaxHealth);
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
    if (bot.network->connectBlocking(options->host, options->port, 3s, options->mode)) {
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
      if (options->smart) {
        bot.input = pilot(*bot.network);
      } else if (turn) {
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
