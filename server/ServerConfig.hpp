#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "common/protocol/Packets.hpp"

namespace server {

struct ServerConfig {
  std::uint16_t port = rtype::protocol::DEFAULT_PORT; // 0 lets the system pick one (tests)
  std::uint8_t maxPlayers = rtype::protocol::MAX_PLAYERS;
  std::uint8_t tickRate = 60;         // simulation steps per second
  std::uint32_t ticksPerSnapshot = 2; // 60 / 2 = 30 world snapshots per second
  std::chrono::milliseconds clientTimeout{rtype::protocol::TIMEOUT_MS};
  std::size_t inboundCapacity = 4096;           // queued datagrams before new ones are dropped
  std::uint32_t maxPacketsPerClientPerTick = 8; // more is a flood: the rest is dropped
  std::uint32_t seed = 0;                       // enemy waves
};

} // namespace server
