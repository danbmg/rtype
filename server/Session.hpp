#pragma once

#include <chrono>
#include <cstdint>

#include "engine/ecs/Entity.hpp"
#include "engine/network/Endpoint.hpp"

namespace server {

// One connected client. UDP has no connection: a session is just "the endpoint that sent an
// accepted CONNECT", kept alive by the datagrams it keeps sending.
struct Session {
  engine::network::Endpoint endpoint;
  std::uint8_t playerId = 0;
  engine::ecs::Entity entity = engine::ecs::INVALID_ENTITY; // its ship; dead ships stay invalid
  std::uint8_t inputFlags = 0;                              // last INPUT received
  std::uint32_t lastInputSequence = 0;
  bool hasInput = false;
  std::uint32_t outgoingSequence = 0; // sequence of the next datagram sent to this client
  std::uint32_t packetsThisTick = 0;
  std::chrono::steady_clock::time_point lastSeen;
};

} // namespace server
