#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <span>
#include <thread>

#include "common/protocol/Packets.hpp"
#include "engine/network/Endpoint.hpp"
#include "engine/network/ThreadSafeQueue.hpp"
#include "engine/network/UdpSocket.hpp"
#include "server/GameWorld.hpp"
#include "server/ServerConfig.hpp"
#include "server/SessionManager.hpp"

namespace server {

struct ServerStats {
  std::uint64_t received = 0;      // datagrams that reached the server
  std::uint64_t invalid = 0;       // rejected by the decoder, or a server -> client type
  std::uint64_t queueFull = 0;     // valid, but the inbound queue was full
  std::uint64_t ignored = 0;       // unknown sender, or over the per-client rate limit
  std::uint64_t socketDropped = 0; // too large or receive error
};

// The multithreaded server:
//
//   network thread (UdpSocket)        game loop thread (fixed tick)
//   receive -> decode + validate  ->  inbound queue -> sessions, inputs -> GameWorld::step()
//   send to the socket            <-  asio::post    <- spawn/destroy events, snapshots
//
// The game loop never waits for the network: it drains whatever has arrived and moves on.
// Invalid datagrams never reach it.
class Server {
public:
  explicit Server(ServerConfig config);
  ~Server();
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(Server&&) = delete;

  // Binds the port (throws std::runtime_error if it is taken) and starts both threads.
  void start();
  // Stops the game loop, then the network thread. Idempotent.
  void stop();

  std::uint16_t port() const { return _socket.localPort(); }
  ServerStats stats() const;

private:
  using Clock = std::chrono::steady_clock;

  struct Incoming {
    engine::network::Endpoint from;
    rtype::protocol::Packet packet;
  };

  void onDatagram(const engine::network::Endpoint& from, std::span<const std::uint8_t> datagram);
  void gameLoop();
  void runTick();
  void tick(Clock::time_point now);
  void handlePacket(const Incoming& incoming, Clock::time_point now);
  void handleConnect(const engine::network::Endpoint& from,
                     const rtype::protocol::Connect& connect, Clock::time_point now);
  void closeSession(const Session& session, rtype::protocol::LeaveReason reason);
  void broadcastChanges(const GameWorld::Changes& changes);
  void broadcastSnapshot();
  void broadcastLevelInfo();
  void sendTo(Session& session, const rtype::protocol::Payload& payload);
  void broadcast(const rtype::protocol::Payload& payload);
  void logDrops();

  ServerConfig _config;
  engine::network::UdpSocket _socket;
  engine::network::ThreadSafeQueue<Incoming> _inbound;
  SessionManager _sessions;
  GameWorld _world;
  std::thread _gameThread;
  std::atomic<bool> _running{false};

  std::atomic<std::uint64_t> _received{0};
  std::atomic<std::uint64_t> _invalid{0};
  std::atomic<std::uint64_t> _queueFull{0};
  std::atomic<std::uint64_t> _ignored{0};
  std::uint64_t _reportedDrops = 0;
  std::uint32_t _sentLevelRevision = 0;
};

} // namespace server
