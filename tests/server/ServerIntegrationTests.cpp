// End-to-end tests over real UDP on the loopback interface: a Server on a free port, real
// NetworkClient instances, and raw sockets playing crashed, outdated or hostile clients.
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "client/net/NetworkClient.hpp"
#include "common/protocol/PacketCodec.hpp"
#include "engine/network/UdpSocket.hpp"
#include "server/Server.hpp"

namespace protocol = rtype::protocol;
using client::net::NetworkClient;
using engine::network::Endpoint;
using namespace std::chrono_literals;

namespace {

constexpr std::uint32_t LOCALHOST = 0x7F000001;

server::ServerConfig testConfig() {
  server::ServerConfig config;
  config.port = 0;
  // Shorter than the real 3 s to keep the tests fast, but longer than the 1 s heartbeat of an
  // idle client: below that, healthy clients would time out too.
  config.clientTimeout = 1500ms;
  config.seed = 1;
  return config;
}

// Runs every client's update() until the condition holds or the timeout expires.
bool pump(const std::vector<NetworkClient*>& clients, const std::function<bool()>& condition,
          std::chrono::milliseconds timeout = 3s) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    for (auto* client : clients) {
      client->update();
    }
    if (condition()) {
      return true;
    }
    std::this_thread::sleep_for(5ms);
  }
  return false;
}

std::size_t shipsSeenBy(const NetworkClient& client) {
  return static_cast<std::size_t>(std::ranges::count_if(client.entities(), [](const auto& entry) {
    return entry.second.kind == protocol::EntityKind::Player;
  }));
}

const client::net::RemoteEntity* shipOf(const NetworkClient& viewer, std::uint8_t playerId) {
  for (const auto& [id, entity] : viewer.entities()) {
    if (entity.kind == protocol::EntityKind::Player && entity.playerId == playerId) {
      return &entity;
    }
  }
  return nullptr;
}

// Collects events across several pollEvents() calls.
struct EventLog {
  std::vector<client::net::NetworkEvent> events;

  void collect(NetworkClient& client) {
    for (auto& event : client.pollEvents()) {
      events.push_back(std::move(event));
    }
  }

  bool playerLeft(std::uint8_t playerId, protocol::LeaveReason reason) const {
    return std::ranges::any_of(events, [&](const auto& event) {
      const auto* left = std::get_if<client::net::PlayerLeft>(&event);
      return left != nullptr && left->playerId == playerId && left->reason == reason;
    });
  }

  bool connectionLost() const {
    return std::ranges::any_of(events, [](const auto& event) {
      return std::holds_alternative<client::net::ConnectionLost>(event);
    });
  }
};

// A hand-driven client: sends any bytes, records the valid packets it gets back.
class RawClient {
public:
  RawClient() {
    _socket.open(0);
    _socket.start([this](const Endpoint&, std::span<const std::uint8_t> datagram) {
      auto result = protocol::decode(datagram);
      if (result.packet) {
        const std::scoped_lock lock(_mutex);
        _packets.push_back(std::move(*result.packet));
      }
    });
  }

  void send(std::uint16_t port, std::vector<std::uint8_t> datagram) {
    _socket.send({LOCALHOST, port}, std::move(datagram));
  }

  template<typename Payload>
  bool waitFor(std::chrono::milliseconds timeout = 2s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      {
        const std::scoped_lock lock(_mutex);
        if (std::ranges::any_of(_packets, [](const protocol::Packet& packet) {
              return std::holds_alternative<Payload>(packet.payload);
            })) {
          return true;
        }
      }
      std::this_thread::sleep_for(5ms);
    }
    return false;
  }

  template<typename Payload>
  std::vector<Payload> received() {
    const std::scoped_lock lock(_mutex);
    std::vector<Payload> found;
    for (const auto& packet : _packets) {
      if (const auto* payload = std::get_if<Payload>(&packet.payload)) {
        found.push_back(*payload);
      }
    }
    return found;
  }

private:
  engine::network::UdpSocket _socket{protocol::MAX_DATAGRAM_SIZE};
  std::mutex _mutex;
  std::vector<protocol::Packet> _packets;
};

} // namespace

TEST_CASE("Four players see each other and a fifth one is turned away", "[server][integration]") {
  server::Server server(testConfig());
  server.start();

  std::array<NetworkClient, 4> players;
  std::vector<NetworkClient*> all;
  std::set<std::uint8_t> ids;
  for (auto& player : players) {
    REQUIRE(player.connectBlocking("127.0.0.1", server.port()));
    ids.insert(player.playerId());
    all.push_back(&player);
  }
  CHECK(ids == std::set<std::uint8_t>{0, 1, 2, 3});
  CHECK(players[0].worldWidth() == 800);
  CHECK(players[0].worldHeight() == 600);

  CHECK(pump(all, [&players] {
    return std::ranges::all_of(players, [](const NetworkClient& player) {
      return shipsSeenBy(player) == 4 && player.players().size() == 4;
    });
  }));

  NetworkClient fifth;
  CHECK_FALSE(fifth.connectBlocking("127.0.0.1", server.port()));
  CHECK(fifth.state() == client::net::ConnectionState::Rejected);
  CHECK(fifth.error() == "the server is full");
}

TEST_CASE("A player's inputs move its ship on the other players' screens",
          "[server][integration]") {
  server::Server server(testConfig());
  server.start();
  NetworkClient pilot;
  NetworkClient watcher;
  REQUIRE(pilot.connectBlocking("127.0.0.1", server.port()));
  REQUIRE(watcher.connectBlocking("127.0.0.1", server.port()));
  const std::vector<NetworkClient*> both{&pilot, &watcher};
  REQUIRE(pump(both, [&] { return shipOf(watcher, pilot.playerId()) != nullptr; }));
  const float startX = shipOf(watcher, pilot.playerId())->x;

  CHECK(pump(both, [&] {
    pilot.sendInput({.right = true});
    const auto* ship = shipOf(watcher, pilot.playerId());
    return ship != nullptr && ship->x > startX + 50.0F;
  }));
}

TEST_CASE("A crashed client is detected and the other players are told", "[server][integration]") {
  server::Server server(testConfig());
  server.start();
  NetworkClient survivor;
  REQUIRE(survivor.connectBlocking("127.0.0.1", server.port()));

  // Joins, then never sends anything again, like a client killed with SIGKILL.
  RawClient crashed;
  crashed.send(server.port(), protocol::encode(0, protocol::Connect{}));
  REQUIRE(crashed.waitFor<protocol::Accept>());
  const auto crashedId = crashed.received<protocol::Accept>().front().playerId;

  EventLog log;
  CHECK(pump({&survivor}, [&] {
    log.collect(survivor);
    return log.playerLeft(crashedId, protocol::LeaveReason::TimedOut);
  }));
  CHECK(survivor.isConnected());
  CHECK(pump({&survivor}, [&] { return shipsSeenBy(survivor) == 1; }));
}

TEST_CASE("A player who quits is announced as having quit", "[server][integration]") {
  server::Server server(testConfig());
  server.start();
  NetworkClient stays;
  NetworkClient leaves;
  REQUIRE(stays.connectBlocking("127.0.0.1", server.port()));
  REQUIRE(leaves.connectBlocking("127.0.0.1", server.port()));
  const auto leaverId = leaves.playerId();

  leaves.disconnect();
  EventLog log;
  CHECK(pump({&stays}, [&] {
    log.collect(stays);
    return log.playerLeft(leaverId, protocol::LeaveReason::Quit);
  }));
}

TEST_CASE("Garbage datagrams are dropped and the game goes on", "[server][integration]") {
  server::Server server(testConfig());
  server.start();
  NetworkClient player;
  REQUIRE(player.connectBlocking("127.0.0.1", server.port()));

  RawClient attacker;
  std::mt19937 random(42U);
  std::uniform_int_distribution<std::size_t> length(0, 1500);
  std::uniform_int_distribution<int> byte(0, 255);
  constexpr int GARBAGE = 1000;
  for (int index = 0; index < GARBAGE; ++index) {
    std::vector<std::uint8_t> datagram(length(random));
    for (auto& value : datagram) {
      value = static_cast<std::uint8_t>(byte(random));
    }
    attacker.send(server.port(), std::move(datagram));
    if (index % 50 == 0) {
      std::this_thread::sleep_for(1ms); // stay below the kernel's receive buffer
    }
  }
  // Packets a client must never send, and packets from someone who never connected.
  attacker.send(server.port(), protocol::encode(0, protocol::EntityDestroy{}));
  attacker.send(server.port(), protocol::encode(0, protocol::Input{protocol::input::FIRE}));

  CHECK(pump({&player}, [&server] {
    const auto stats = server.stats();
    return stats.invalid + stats.socketDropped >= GARBAGE * 9 / 10 && stats.ignored >= 1;
  }));
  CHECK(player.isConnected());
  CHECK(pump({&player}, [&player] { return shipsSeenBy(player) == 1; }));
}

TEST_CASE("A client with another protocol version is refused cleanly", "[server][integration]") {
  server::Server server(testConfig());
  server.start();

  RawClient outdated;
  auto connect = protocol::encode(0, protocol::Connect{});
  connect[2] = protocol::VERSION + 1;
  outdated.send(server.port(), connect);
  REQUIRE(outdated.waitFor<protocol::Reject>());
  CHECK(outdated.received<protocol::Reject>().front().reason ==
        protocol::RejectReason::VersionMismatch);
}

TEST_CASE("A flooding client is rate limited", "[server][integration]") {
  server::Server server(testConfig());
  server.start();
  NetworkClient player;
  REQUIRE(player.connectBlocking("127.0.0.1", server.port()));

  RawClient flooder;
  flooder.send(server.port(), protocol::encode(0, protocol::Connect{}));
  REQUIRE(flooder.waitFor<protocol::Accept>());
  for (std::uint32_t sequence = 1; sequence <= 500; ++sequence) {
    flooder.send(server.port(), protocol::encode(sequence, protocol::Ping{sequence}));
  }

  CHECK(pump({&player}, [&server] { return server.stats().ignored > 0; }));
  CHECK(flooder.received<protocol::Pong>().size() < 500);
  CHECK(player.isConnected());
}

TEST_CASE("A client notices when the server stops answering", "[server][integration]") {
  server::Server server(testConfig());
  server.start();
  NetworkClient player;
  REQUIRE(player.connectBlocking("127.0.0.1", server.port()));
  server.stop();

  EventLog log;
  CHECK(pump(
      {&player},
      [&] {
        log.collect(player);
        return log.connectionLost();
      },
      std::chrono::milliseconds(protocol::TIMEOUT_MS) + 2s));
  CHECK(player.state() == client::net::ConnectionState::Failed);
}
