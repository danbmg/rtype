#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "client/net/NetworkTypes.hpp"
#include "common/protocol/Packets.hpp"
#include "engine/network/Endpoint.hpp"
#include "engine/network/ThreadSafeQueue.hpp"
#include "engine/network/UdpSocket.hpp"

namespace client::net {

// The client side of the protocol. Usage from the render loop:
//
//   client::net::NetworkClient network;
//   network.connect("127.0.0.1", 4242);              // from the connection menu
//   while (window.isOpen()) {
//     network.update();                              // once per frame, never blocks
//     network.sendInput({up, down, left, right, fire});
//     for (auto& event : network.pollEvents()) { ... }  // sounds, explosions
//     for (auto& [id, entity] : network.entities()) { ... }  // draw
//   }
//
// Datagrams are received on a network thread and queued; everything else, including every
// accessor, runs on the thread that calls update(). The client never decides anything about
// the game: it sends inputs and displays what the server reports.
class NetworkClient {
public:
  NetworkClient();
  ~NetworkClient();
  NetworkClient(const NetworkClient&) = delete;
  NetworkClient& operator=(const NetworkClient&) = delete;
  NetworkClient(NetworkClient&&) = delete;
  NetworkClient& operator=(NetworkClient&&) = delete;

  // Starts connecting without blocking; update() then moves state() to Connected, Rejected or
  // Failed. Returns false at once if the host cannot be resolved or no socket can be opened.
  bool connect(const std::string& host, std::uint16_t port);

  // connect(), then update() until connected or the timeout. For tools and tests.
  bool connectBlocking(const std::string& host, std::uint16_t port,
                       std::chrono::milliseconds timeout = std::chrono::seconds(3));

  // Tells the server we leave (best effort) and closes the socket.
  void disconnect();

  // Applies what the server sent, retries CONNECT, sends heartbeats, detects a dead server.
  void update();

  // Sent when it changes, and repeated every 33 ms otherwise so a lost datagram costs little.
  void sendInput(const InputState& input);

  ConnectionState state() const { return _state; }
  bool isConnected() const { return _state == ConnectionState::Connected; }
  const std::string& error() const { return _error; }

  std::uint8_t playerId() const { return _playerId; } // valid once connected
  std::uint16_t worldWidth() const { return _worldWidth; }
  std::uint16_t worldHeight() const { return _worldHeight; }
  std::chrono::milliseconds ping() const { return _ping; } // last round-trip time

  const std::unordered_map<std::uint32_t, RemoteEntity>& entities() const { return _entities; }
  const std::vector<rtype::protocol::PlayerState>& players() const { return _players; }
  std::vector<NetworkEvent> pollEvents();

  std::uint64_t droppedDatagrams() const;

private:
  using Clock = std::chrono::steady_clock;

  void onDatagram(const engine::network::Endpoint& from, std::span<const std::uint8_t> datagram);
  void handlePacket(const rtype::protocol::Packet& packet, Clock::time_point now);
  void applyEntity(const rtype::protocol::EntityState& state, Clock::time_point now);
  void removeStaleEntities(Clock::time_point now);
  void send(const rtype::protocol::Payload& payload);
  void fail(ConnectionState state, std::string error);
  void pushEvent(NetworkEvent event);
  void closeSocket();

  std::unique_ptr<engine::network::UdpSocket> _socket;
  engine::network::Endpoint _server;
  engine::network::ThreadSafeQueue<rtype::protocol::Packet> _inbound;
  std::atomic<std::uint64_t> _dropped{0};
  std::atomic<bool> _versionMismatch{false};

  ConnectionState _state = ConnectionState::Disconnected;
  std::string _error;
  std::uint32_t _sequence = 0;
  std::uint8_t _playerId = rtype::protocol::NO_PLAYER;
  std::uint16_t _worldWidth = 0;
  std::uint16_t _worldHeight = 0;
  std::chrono::milliseconds _ping{0};

  Clock::time_point _epoch = Clock::now();
  Clock::time_point _connectStarted;
  Clock::time_point _lastConnectSent;
  Clock::time_point _lastServerPacket;
  Clock::time_point _lastPingSent;
  Clock::time_point _lastInputSent;
  std::uint8_t _lastInputFlags = 0;
  std::uint32_t _lastSnapshotTick = 0;
  bool _hasSnapshot = false;

  std::unordered_map<std::uint32_t, RemoteEntity> _entities;
  std::unordered_map<std::uint32_t, Clock::time_point> _lastSeen;
  std::vector<rtype::protocol::PlayerState> _players;
  std::deque<NetworkEvent> _events;
};

} // namespace client::net
