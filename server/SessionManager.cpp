#include "server/SessionManager.hpp"

#include <stdexcept>

namespace server {

SessionManager::SessionManager(std::uint8_t maxPlayers) : _maxPlayers(maxPlayers) {
  if (maxPlayers == 0 || maxPlayers > rtype::protocol::MAX_PLAYERS) {
    throw std::invalid_argument("maxPlayers must be between 1 and protocol::MAX_PLAYERS");
  }
}

Session* SessionManager::join(const engine::network::Endpoint& endpoint, Clock::time_point now) {
  if (auto* existing = find(endpoint)) {
    return existing;
  }
  for (std::uint8_t slot = 0; slot < _maxPlayers; ++slot) {
    if (!_slots[slot]) {
      Session session;
      session.endpoint = endpoint;
      session.playerId = slot;
      session.lastSeen = now;
      return &_slots[slot].emplace(session);
    }
  }
  return nullptr;
}

Session* SessionManager::find(const engine::network::Endpoint& endpoint) {
  for (auto& slot : _slots) {
    if (slot && slot->endpoint == endpoint) {
      return &*slot;
    }
  }
  return nullptr;
}

std::optional<Session> SessionManager::remove(const engine::network::Endpoint& endpoint) {
  for (auto& slot : _slots) {
    if (slot && slot->endpoint == endpoint) {
      auto removed = std::move(slot);
      slot.reset();
      return removed;
    }
  }
  return std::nullopt;
}

std::vector<Session> SessionManager::removeTimedOut(Clock::time_point now,
                                                    std::chrono::milliseconds timeout) {
  std::vector<Session> removed;
  for (auto& slot : _slots) {
    if (slot && now - slot->lastSeen > timeout) {
      removed.push_back(*slot);
      slot.reset();
    }
  }
  return removed;
}

std::size_t SessionManager::size() const {
  std::size_t count = 0;
  for (const auto& slot : _slots) {
    count += slot ? 1 : 0;
  }
  return count;
}

} // namespace server
