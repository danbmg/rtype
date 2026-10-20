#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "common/protocol/Packets.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/ecs/Registry.hpp"
#include "game/GameSystems.hpp"

namespace server {

// The authoritative game: the ECS registry and the gameplay systems from game/, plus what the
// network needs from them (player ships, inputs, and the world translated into protocol terms).
// Runs on the game loop thread only.
class GameWorld {
public:
  struct Changes {
    std::vector<rtype::protocol::EntityState> spawned;
    std::vector<rtype::protocol::EntityRemoval> destroyed;
  };

  GameWorld(std::uint8_t tickRate, std::uint32_t seed, game::WorldBounds bounds = {});

  engine::ecs::Entity spawnPlayer(std::uint8_t playerId);
  void removePlayer(std::uint8_t playerId, engine::ecs::Entity ship);

  // Movement and fire for one tick. Fire is limited by a cooldown, and a dead ship ignores
  // its inputs.
  void applyInput(engine::ecs::Entity ship, std::uint8_t flags);

  // One fixed step: enemy waves, then movement, collisions, deaths, cleanup.
  void step();

  // Entities that appeared and disappeared since the previous call. Call it once per tick,
  // after step(): snapshot() reports the world as of this call.
  Changes collectChanges();
  std::vector<rtype::protocol::EntityState> snapshot() const;
  std::vector<rtype::protocol::PlayerState> playerStates() const;

  std::uint32_t tick() const { return _tick; }
  std::uint8_t tickRate() const { return _tickRate; }
  const game::WorldBounds& bounds() const { return _bounds; }
  engine::ecs::Registry& registry() { return _registry; }

private:
  std::optional<rtype::protocol::EntityState> describe(engine::ecs::Entity entity) const;
  void refreshPlayerStates();

  engine::ecs::Registry _registry;
  engine::core::SystemScheduler<engine::ecs::Registry> _scheduler;
  game::WorldBounds _bounds;
  std::uint8_t _tickRate;
  std::uint32_t _tick = 0;
  std::uint32_t _fireCooldownTicks;
  std::unordered_map<engine::ecs::Entity, std::uint32_t> _fireCooldowns;
  std::unordered_map<engine::ecs::Entity, rtype::protocol::EntityState> _known;
  std::array<engine::ecs::Entity, rtype::protocol::MAX_PLAYERS> _ships{};
  std::array<std::optional<rtype::protocol::PlayerState>, rtype::protocol::MAX_PLAYERS> _players;
};

} // namespace server
