#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "common/protocol/Packets.hpp"
#include "engine/network/Endpoint.hpp"
#include "server/Session.hpp"

namespace server {

// The connected clients, one slot per player id (0 to maxPlayers - 1). Used by the game loop
// thread only, so it needs no lock. Pointers it returns stay valid until that session is removed.
class SessionManager {
public:
  using Clock = std::chrono::steady_clock;

  explicit SessionManager(std::uint8_t maxPlayers);

  // Creates a session in the lowest free slot, or returns nullptr when the server is full.
  Session* join(const engine::network::Endpoint& endpoint, Clock::time_point now);
  Session* find(const engine::network::Endpoint& endpoint);

  std::optional<Session> remove(const engine::network::Endpoint& endpoint);
  // Removes and returns the sessions silent for longer than timeout: crashed clients.
  std::vector<Session> removeTimedOut(Clock::time_point now, std::chrono::milliseconds timeout);

  template<typename Func>
  void forEach(Func&& func) {
    for (auto& slot : _slots) {
      if (slot) {
        func(*slot);
      }
    }
  }

  std::size_t size() const;
  bool full() const { return size() >= _maxPlayers; }

private:
  std::array<std::optional<Session>, rtype::protocol::MAX_PLAYERS> _slots;
  std::uint8_t _maxPlayers;
};

} // namespace server
