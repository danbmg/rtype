#include "client/net/NetworkClient.hpp"

#include <algorithm>
#include <exception>
#include <thread>
#include <type_traits>
#include <utility>

#include "common/protocol/PacketCodec.hpp"

namespace client::net {

namespace protocol = rtype::protocol;
using namespace std::chrono_literals;

namespace {

constexpr auto CONNECT_RETRY_INTERVAL = 250ms;
constexpr auto CONNECT_TIMEOUT = 3s;
constexpr auto INPUT_RESEND_INTERVAL = 33ms;
constexpr auto HEARTBEAT_INTERVAL = std::chrono::milliseconds(protocol::HEARTBEAT_INTERVAL_MS);
constexpr auto SERVER_TIMEOUT = std::chrono::milliseconds(protocol::TIMEOUT_MS);
// Snapshots arrive 30 times per second. An entity missing from them for this long was
// destroyed and the ENTITY_DESTROY datagram was lost.
constexpr auto STALE_ENTITY_DELAY = 500ms;
constexpr std::size_t INBOUND_CAPACITY = 1024;
constexpr std::size_t MAX_PENDING_EVENTS = 1024;

std::uint8_t toFlags(const InputState& input) {
  std::uint8_t flags = 0;
  flags |= input.up ? protocol::input::UP : 0U;
  flags |= input.down ? protocol::input::DOWN : 0U;
  flags |= input.left ? protocol::input::LEFT : 0U;
  flags |= input.right ? protocol::input::RIGHT : 0U;
  flags |= input.fire ? protocol::input::FIRE : 0U;
  return flags;
}

} // namespace

NetworkClient::NetworkClient() : _inbound(INBOUND_CAPACITY) {}

NetworkClient::~NetworkClient() { disconnect(); }

bool NetworkClient::connect(const std::string& host, std::uint16_t port,
                            protocol::GameMode mode, protocol::Difficulty difficulty) {
  disconnect();
  _connectRequest = protocol::Connect{mode, difficulty};
  _state = ConnectionState::Disconnected;
  _error.clear();
  _versionMismatch = false;
  _hasSnapshot = false;
  _lastInputFlags = 0;

  const auto server = engine::network::UdpSocket::resolve(host, port);
  if (!server) {
    fail(ConnectionState::Failed, "cannot resolve host " + host);
    return false;
  }
  _server = *server;

  try {
    _socket = std::make_unique<engine::network::UdpSocket>(protocol::MAX_DATAGRAM_SIZE);
    _socket->open(0);
    _socket->start([this](const engine::network::Endpoint& from,
                          std::span<const std::uint8_t> datagram) { onDatagram(from, datagram); });
  } catch (const std::exception& exception) {
    _socket.reset();
    fail(ConnectionState::Failed, exception.what());
    return false;
  }

  // CONNECT lance la négociation UDP sans bloquer la boucle graphique.
  _state = ConnectionState::Connecting;
  _connectStarted = Clock::now();
  _lastConnectSent = _connectStarted;
  send(_connectRequest);
  return true;
}

bool NetworkClient::connectBlocking(const std::string& host, std::uint16_t port,
                                    std::chrono::milliseconds timeout, protocol::GameMode mode,
                                    protocol::Difficulty difficulty) {
  if (!connect(host, port, mode, difficulty)) {
    return false;
  }
  const auto deadline = Clock::now() + timeout;
  while (_state == ConnectionState::Connecting && Clock::now() < deadline) {
    std::this_thread::sleep_for(5ms);
    update();
  }
  if (_state == ConnectionState::Connecting) {
    fail(ConnectionState::Failed, "no answer from the server");
  }
  return isConnected();
}

void NetworkClient::disconnect() {
  if (_socket && _state == ConnectionState::Connected) {
    send(protocol::Disconnect{});
  }
  closeSocket();
  if (_state == ConnectionState::Connected || _state == ConnectionState::Connecting) {
    _state = ConnectionState::Disconnected;
  }
}

void NetworkClient::closeSocket() {
  if (_socket) {
    _socket->stop(); // datagrams already queued, such as DISCONNECT, are sent first
    _socket.reset();
  }
  _inbound.drain();
  _entities.clear();
  _lastSeen.clear();
  _players.clear();
  _gameStatus = {};
  _levelInfo = {};
}

void NetworkClient::update() {
  if (!_socket) {
    return;
  }
  const auto now = Clock::now();
  for (const auto& packet : _inbound.drain()) {
    handlePacket(packet, now);
    if (!_socket) {
      return; // the packet ended the session (REJECT)
    }
  }

  if (_state == ConnectionState::Connecting) {
    if (_versionMismatch) {
      fail(ConnectionState::Rejected, "the server speaks another protocol version");
    } else if (now - _connectStarted > CONNECT_TIMEOUT) {
      fail(ConnectionState::Failed, "no answer from the server");
    } else if (now - _lastConnectSent >= CONNECT_RETRY_INTERVAL) {
      _lastConnectSent = now; // CONNECT or ACCEPT may have been lost: ask again
      send(_connectRequest);
    }
    return;
  }

  if (_state == ConnectionState::Connected) {
    if (now - _lastServerPacket > SERVER_TIMEOUT) {
      pushEvent(ConnectionLost{});
      fail(ConnectionState::Failed, "connection lost: the server stopped answering");
      return;
    }
    if (now - _lastPingSent >= HEARTBEAT_INTERVAL) {
      _lastPingSent = now;
      const auto token = std::chrono::duration_cast<std::chrono::milliseconds>(now - _epoch);
      send(protocol::Ping{static_cast<std::uint32_t>(token.count())});
    }
    removeStaleEntities(now);
  }
}

void NetworkClient::sendInput(const InputState& input) {
  if (_state != ConnectionState::Connected) {
    return;
  }
  const auto flags = toFlags(input);
  const auto now = Clock::now();
  if (flags == _lastInputFlags && now - _lastInputSent < INPUT_RESEND_INTERVAL) {
    return;
  }
  _lastInputFlags = flags;
  _lastInputSent = now;
  send(protocol::Input{flags});
}

std::vector<NetworkEvent> NetworkClient::pollEvents() {
  std::vector<NetworkEvent> events(std::make_move_iterator(_events.begin()),
                                   std::make_move_iterator(_events.end()));
  _events.clear();
  return events;
}

std::uint64_t NetworkClient::droppedDatagrams() const {
  return _dropped.load() + (_socket ? _socket->droppedDatagrams() : 0);
}

// Network thread: validate, then hand over to update() through the queue.
void NetworkClient::onDatagram(const engine::network::Endpoint& from,
                               std::span<const std::uint8_t> datagram) {
  // Le thread UDP filtre source et format, puis passe le paquet au thread de jeu via la file.
  if (from != _server) {
    ++_dropped; // only the server may talk to us
    return;
  }
  auto result = protocol::decode(datagram);
  if (!result.packet) {
    if (result.error == protocol::DecodeError::BadVersion) {
      _versionMismatch = true;
    }
    ++_dropped;
    return;
  }
  if (!_inbound.tryPush(std::move(*result.packet))) {
    ++_dropped;
  }
}

void NetworkClient::handlePacket(const protocol::Packet& packet, Clock::time_point now) {
  if (_state == ConnectionState::Connecting) {
    if (const auto* accept = std::get_if<protocol::Accept>(&packet.payload)) {
      _playerId = accept->playerId;
      _worldWidth = accept->worldWidth;
      _worldHeight = accept->worldHeight;
      _state = ConnectionState::Connected;
      _lastServerPacket = now;
      _lastPingSent = now;
    } else if (const auto* reject = std::get_if<protocol::Reject>(&packet.payload)) {
      fail(ConnectionState::Rejected, reject->reason == protocol::RejectReason::ServerFull
                                          ? "the server is full"
                                          : "the server speaks another protocol version");
    }
    return; // world packets before ACCEPT belong to nobody yet
  }
  if (_state != ConnectionState::Connected) {
    return;
  }
  _lastServerPacket = now;

  std::visit(
      [this, now](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, protocol::Pong>) {
          const auto sent = _epoch + std::chrono::milliseconds(payload.token);
          _ping = std::chrono::duration_cast<std::chrono::milliseconds>(now - sent);
        } else if constexpr (std::is_same_v<T, protocol::EntitySpawn>) {
          for (const auto& state : payload.entities) {
            applyEntity(state, now);
          }
        } else if constexpr (std::is_same_v<T, protocol::EntityUpdate>) {
          // UDP may deliver an old snapshot after a newer one: never go back in time.
          if (_hasSnapshot && payload.tick < _lastSnapshotTick) {
            return;
          }
          _hasSnapshot = true;
          _lastSnapshotTick = payload.tick;
          for (const auto& state : payload.entities) {
            applyEntity(state, now);
          }
        } else if constexpr (std::is_same_v<T, protocol::EntityDestroy>) {
          for (const auto& removal : payload.entities) {
            const auto entity = _entities.find(removal.id);
            if (entity != _entities.end()) {
              pushEvent(EntityDestroyed{entity->second, removal.reason});
              _entities.erase(entity);
              _lastSeen.erase(removal.id);
            }
          }
        } else if constexpr (std::is_same_v<T, protocol::PlayerStatus>) {
          _players = payload.players;
        } else if constexpr (std::is_same_v<T, protocol::GameStatus>) {
          _gameStatus = payload;
        } else if constexpr (std::is_same_v<T, protocol::LevelInfo>) {
          _levelInfo = payload;
        } else if constexpr (std::is_same_v<T, protocol::PlayerLeft>) {
          std::erase_if(_players, [&payload](const protocol::PlayerState& player) {
            return player.playerId == payload.playerId;
          });
          pushEvent(PlayerLeft{payload.playerId, payload.reason});
        }
        // Other packet types are client -> server or session packets: nothing to apply.
      },
      packet.payload);
}

void NetworkClient::applyEntity(const protocol::EntityState& state, Clock::time_point now) {
  const RemoteEntity entity{state.id, state.kind, state.playerId, state.x, state.y};
  // A snapshot can show an entity whose ENTITY_SPAWN was lost: it appears all the same.
  const auto [position, inserted] = _entities.insert_or_assign(state.id, entity);
  if (inserted) {
    pushEvent(EntitySpawned{position->second});
  }
  _lastSeen[state.id] = now;
}

void NetworkClient::removeStaleEntities(Clock::time_point now) {
  for (auto entity = _entities.begin(); entity != _entities.end();) {
    if (now - _lastSeen[entity->first] > STALE_ENTITY_DELAY) {
      pushEvent(EntityDestroyed{entity->second, protocol::DestroyReason::LeftScreen});
      _lastSeen.erase(entity->first);
      entity = _entities.erase(entity);
    } else {
      ++entity;
    }
  }
}

void NetworkClient::send(const protocol::Payload& payload) {
  if (_socket) {
    _socket->send(_server, protocol::encode(_sequence++, payload));
  }
}

void NetworkClient::fail(ConnectionState state, std::string error) {
  closeSocket();
  _state = state;
  _error = std::move(error);
}

void NetworkClient::pushEvent(NetworkEvent event) {
  if (_events.size() >= MAX_PENDING_EVENTS) {
    _events.pop_front(); // nobody polls: keep the latest events only
  }
  _events.push_back(std::move(event));
}

} // namespace client::net
