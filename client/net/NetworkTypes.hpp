#pragma once

#include <cstdint>
#include <variant>

#include "common/protocol/Packets.hpp"

namespace client::net {

// What the local player is pressing this frame.
struct InputState {
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
  bool fire = false;
};

// An entity as last reported by the server. Coordinates are world units, top-left corner:
// scale them by window size / NetworkClient::worldWidth() and worldHeight().
struct RemoteEntity {
  std::uint32_t id = 0;
  rtype::protocol::EntityKind kind = rtype::protocol::EntityKind::Player;
  std::uint8_t playerId = rtype::protocol::NO_PLAYER; // ship owner (0-3): pick its colour
  float x = 0.0F;
  float y = 0.0F;
};

// One-shot events for sounds and effects. NetworkClient::entities() already reflects them.
struct EntitySpawned { // a Bydos appeared, someone fired...
  RemoteEntity entity;
};
struct EntityDestroyed { // reason Killed: play an explosion at entity.x, entity.y
  RemoteEntity entity;
  rtype::protocol::DestroyReason reason = rtype::protocol::DestroyReason::Killed;
};
struct PlayerLeft { // another player quit or crashed (TimedOut)
  std::uint8_t playerId = 0;
  rtype::protocol::LeaveReason reason = rtype::protocol::LeaveReason::Quit;
};
struct ConnectionLost {}; // the server stopped answering

using NetworkEvent = std::variant<EntitySpawned, EntityDestroyed, PlayerLeft, ConnectionLost>;

enum class ConnectionState {
  Disconnected,
  Connecting, // connect() called, waiting for the server
  Connected,
  Rejected, // server full or other protocol version: see NetworkClient::error()
  Failed,   // host not found, no answer, or connection lost: see NetworkClient::error()
};

} // namespace client::net
