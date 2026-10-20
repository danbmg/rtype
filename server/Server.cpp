#include "server/Server.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/protocol/PacketCodec.hpp"
#include "engine/core/Clock.hpp"
#include "engine/core/Logger.hpp"

namespace server {

namespace protocol = rtype::protocol;
using engine::core::Logger;
using namespace std::chrono_literals;

namespace {

// After a stall (debugger, suspended VM) the loop catches up at most this many ticks at once,
// then drops the rest instead of freezing to replay them.
constexpr int MAX_CATCH_UP_TICKS = 5;
constexpr std::uint32_t DROP_REPORT_SECONDS = 5;

// Sequence numbers wrap around: compare them with serial number arithmetic (RFC 1982).
bool isNewer(std::uint32_t sequence, std::uint32_t reference) {
  return static_cast<std::int32_t>(sequence - reference) > 0;
}

bool isClientToServer(protocol::PacketType type) {
  return type == protocol::PacketType::Connect || type == protocol::PacketType::Input ||
         type == protocol::PacketType::Ping || type == protocol::PacketType::Disconnect;
}

template<typename State>
std::vector<std::vector<State>> chunk(const std::vector<State>& items, std::size_t size) {
  std::vector<std::vector<State>> chunks;
  for (std::size_t first = 0; first < items.size(); first += size) {
    const auto last = std::min(items.size(), first + size);
    chunks.emplace_back(items.begin() + static_cast<std::ptrdiff_t>(first),
                        items.begin() + static_cast<std::ptrdiff_t>(last));
  }
  return chunks;
}

const ServerConfig& validated(const ServerConfig& config) {
  if (config.tickRate == 0 || config.ticksPerSnapshot == 0 || config.clientTimeout <= 0ms ||
      config.inboundCapacity == 0 || config.maxPacketsPerClientPerTick == 0) {
    throw std::invalid_argument("invalid server configuration");
  }
  return config;
}

} // namespace

Server::Server(ServerConfig config)
    : _config(validated(config)), _socket(protocol::MAX_DATAGRAM_SIZE),
      _inbound(config.inboundCapacity), _sessions(config.maxPlayers),
      _world(config.tickRate, config.seed) {}

Server::~Server() { stop(); }

void Server::start() {
  _socket.open(_config.port);
  _socket.start([this](const engine::network::Endpoint& from,
                       std::span<const std::uint8_t> datagram) { onDatagram(from, datagram); });
  _running = true;
  _gameThread = std::thread([this] { gameLoop(); });
  Logger::info("server listening on UDP port " + std::to_string(port()) + ", " +
               std::to_string(_config.tickRate) + " ticks per second");
}

void Server::stop() {
  _running = false;
  if (_gameThread.joinable()) {
    _gameThread.join();
  }
  _socket.stop();
}

ServerStats Server::stats() const {
  return {_received.load(), _invalid.load(), _queueFull.load(), _ignored.load(),
          _socket.droppedDatagrams()};
}

// ---- Network thread ---------------------------------------------------------------------
// Runs between two receives: decode, validate, queue. No game state is touched here.

void Server::onDatagram(const engine::network::Endpoint& from,
                        std::span<const std::uint8_t> datagram) {
  // Le thread réseau valide et met les paquets en file sans toucher au monde de jeu.
  ++_received;
  auto result = protocol::decode(datagram);
  if (!result.packet) {
    ++_invalid;
    // Byte 3 is the packet type in every protocol version: a CONNECT from another version
    // gets a clear refusal instead of silence.
    if (result.error == protocol::DecodeError::BadVersion &&
        datagram[3] == static_cast<std::uint8_t>(protocol::PacketType::Connect)) {
      _socket.send(from,
                   protocol::encode(0, protocol::Reject{protocol::RejectReason::VersionMismatch}));
    }
    return;
  }
  if (!isClientToServer(result.packet->header.type)) {
    ++_invalid; // a client never sends world packets
    return;
  }
  if (!_inbound.tryPush({from, std::move(*result.packet)})) {
    ++_queueFull;
  }
}

// ---- Game loop thread -------------------------------------------------------------------

void Server::gameLoop() {
  engine::core::FixedTimestep timestep(std::chrono::nanoseconds(1s) / _config.tickRate);
  while (_running) {
    if (timestep.update()) {
      runTick();
      int catchUp = 0;
      while (catchUp < MAX_CATCH_UP_TICKS && timestep.consumeStep()) {
        runTick();
        ++catchUp;
      }
      if (timestep.ready()) {
        Logger::warning("server: game loop is late, skipping ticks");
        timestep.reset();
      }
    }
    std::this_thread::sleep_for(1ms);
  }
}

void Server::runTick() {
  // No exception may escape the game loop: one bad tick must not stop the server.
  try {
    tick(Clock::now());
  } catch (const std::exception& exception) {
    Logger::error(std::string("server: tick failed: ") + exception.what());
  } catch (...) {
    Logger::error("server: tick failed with an unknown exception");
  }
}

void Server::tick(Clock::time_point now) {
  // Chaque tick traite les entrées, simule le monde, puis diffuse changements et états.
  _sessions.forEach([](Session& session) { session.packetsThisTick = 0; });
  for (const auto& incoming : _inbound.drain()) {
    handlePacket(incoming, now);
  }

  for (const auto& session : _sessions.removeTimedOut(now, _config.clientTimeout)) {
    closeSession(session, protocol::LeaveReason::TimedOut);
  }

  _sessions.forEach(
      [this](Session& session) { _world.applyInput(session.entity, session.inputFlags); });
  _world.step();
  broadcastChanges(_world.collectChanges());
  if (_world.tick() % _config.ticksPerSnapshot == 0) {
    broadcastSnapshot();
  }
  if (_world.tick() % (DROP_REPORT_SECONDS * _config.tickRate) == 0) {
    logDrops();
  }
}

void Server::handlePacket(const Incoming& incoming, Clock::time_point now) {
  const auto& [from, packet] = incoming;
  auto* session = _sessions.find(from);
  if (session != nullptr && ++session->packetsThisTick > _config.maxPacketsPerClientPerTick) {
    ++_ignored; // flood: a real client sends a couple of packets per tick at most
    return;
  }
  if (packet.header.type == protocol::PacketType::Connect) {
    handleConnect(from, now);
    return;
  }
  if (session == nullptr) {
    ++_ignored; // only CONNECT may come from an unknown endpoint
    return;
  }
  session->lastSeen = now;

  if (const auto* input = std::get_if<protocol::Input>(&packet.payload)) {
    // Le serveur reste autoritaire et ignore les commandes reçues dans le désordre.
    // UDP can reorder: an older INPUT must not overwrite a newer one.
    if (!session->hasInput || isNewer(packet.header.sequence, session->lastInputSequence)) {
      session->inputFlags = input->flags;
      session->lastInputSequence = packet.header.sequence;
      session->hasInput = true;
    }
  } else if (const auto* ping = std::get_if<protocol::Ping>(&packet.payload)) {
    sendTo(*session, protocol::Pong{ping->token});
  } else if (std::holds_alternative<protocol::Disconnect>(packet.payload)) {
    if (auto removed = _sessions.remove(from)) {
      closeSession(*removed, protocol::LeaveReason::Quit);
    }
  }
}

void Server::handleConnect(const engine::network::Endpoint& from, Clock::time_point now) {
  // L'adresse UDP identifie la session ; un CONNECT répété ne crée pas un second joueur.
  const bool known = _sessions.find(from) != nullptr;
  auto* session = _sessions.join(from, now);
  if (session == nullptr) {
    _socket.send(from, protocol::encode(0, protocol::Reject{protocol::RejectReason::ServerFull}));
    return;
  }
  session->lastSeen = now;
  if (!known) {
    session->entity = _world.spawnPlayer(session->playerId);
    Logger::info("player " + std::to_string(session->playerId + 1) + " joined from " +
                 from.toString());
  }
  // Sent again on a repeated CONNECT: the first ACCEPT may have been lost.
  sendTo(*session,
         protocol::Accept{session->playerId, static_cast<std::uint16_t>(_world.bounds().width),
                          static_cast<std::uint16_t>(_world.bounds().height), _config.tickRate});
}

void Server::closeSession(const Session& session, protocol::LeaveReason reason) {
  _world.removePlayer(session.playerId, session.entity);
  broadcast(protocol::PlayerLeft{session.playerId, reason});
  Logger::info("player " + std::to_string(session.playerId + 1) +
               (reason == protocol::LeaveReason::Quit ? " left" : " timed out (crashed?)"));
}

void Server::broadcastChanges(const GameWorld::Changes& changes) {
  for (auto& entities : chunk(changes.spawned, protocol::MAX_SPAWNS_PER_PACKET)) {
    broadcast(protocol::EntitySpawn{std::move(entities)});
  }
  for (auto& entities : chunk(changes.destroyed, protocol::MAX_REMOVALS_PER_PACKET)) {
    broadcast(protocol::EntityDestroy{std::move(entities)});
  }
}

void Server::broadcastSnapshot() {
  // Les snapshots périodiques resynchronisent les clients après les pertes UDP.
  for (auto& entities : chunk(_world.snapshot(), protocol::MAX_UPDATES_PER_PACKET)) {
    broadcast(protocol::EntityUpdate{_world.tick(), std::move(entities)});
  }
  broadcast(protocol::PlayerStatus{_world.playerStates()});
}

void Server::sendTo(Session& session, const protocol::Payload& payload) {
  _socket.send(session.endpoint, protocol::encode(session.outgoingSequence++, payload));
}

void Server::broadcast(const protocol::Payload& payload) {
  _sessions.forEach([this, &payload](Session& session) { sendTo(session, payload); });
}

void Server::logDrops() {
  const auto current = stats();
  const auto drops = current.invalid + current.queueFull + current.ignored + current.socketDropped;
  if (drops != _reportedDrops) {
    Logger::warning("server: dropped " + std::to_string(drops - _reportedDrops) +
                    " datagrams (invalid " + std::to_string(current.invalid) + ", queue full " +
                    std::to_string(current.queueFull) + ", ignored " +
                    std::to_string(current.ignored) + ", socket " +
                    std::to_string(current.socketDropped) + " in total)");
    _reportedDrops = drops;
  }
}

} // namespace server
