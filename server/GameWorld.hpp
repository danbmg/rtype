#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "common/protocol/Packets.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/ecs/Registry.hpp"
#include "game/Content.hpp"
#include "game/GameSystems.hpp"
#include "game/LevelDirector.hpp"

namespace server {

// The authoritative game: the ECS registry and the gameplay systems from game/, plus what the
// network needs from them (player ships, inputs, and the world translated into protocol terms).
// Runs on the game loop thread only.
//
// Life of a game:
//   Lobby (nobody connected) --first player joins, with its mode and difficulty--> Running
//   Running --every connected player out of lives--> GameOver --countdown--> Running (restart)
//   Running --story completed (Cup picked up)--> Victory --countdown--> Running (restart)
//   Running --tutorial: SKIP_LEVEL on the final level--> Victory
//   any state --last player leaves--> Lobby
class GameWorld {
public:
  struct Changes {
    std::vector<rtype::protocol::EntityState> spawned;
    std::vector<rtype::protocol::EntityRemoval> destroyed;
  };

  static constexpr float GAME_OVER_SECONDS = 6.0F;
  static constexpr float VICTORY_SECONDS = 20.0F;

  GameWorld(std::uint8_t tickRate, std::uint32_t seed, game::WorldBounds bounds = {},
            game::GameContent content = {}, std::uint32_t initialLevel = 1);

  // What the next game will be. Only taken into account while nobody plays (lobby): players who
  // join a running game play whatever is running. Tutorial is for the story only: an endless
  // game asked in tutorial is played in normal.
  void requestGame(rtype::protocol::GameMode mode, rtype::protocol::Difficulty difficulty);

  // Spawns the ship of a new player. The first player of an empty world starts the game.
  engine::ecs::Entity spawnPlayer(std::uint8_t playerId);
  // Removes a player and its ship. The world goes back to the lobby when nobody is left.
  void removePlayer(std::uint8_t playerId, engine::ecs::Entity ship);

  // Movement and fire for one tick. Fire is limited by a cooldown, and a dead ship ignores
  // its inputs. applyPlayerInput finds the ship of a player, which changes when a game restarts.
  void applyInput(engine::ecs::Entity ship, std::uint8_t flags);
  void applyPlayerInput(std::uint8_t playerId, std::uint8_t flags);
  engine::ecs::Entity ship(std::uint8_t playerId) const;

  // One fixed step: level director (waves, behaviours), then movement, collisions, deaths,
  // cleanup, then the game state (game over, victory, restart).
  void step();

  // Entities that appeared and disappeared since the previous call. Call it once per tick,
  // after step(): snapshot() reports the world as of this call.
  Changes collectChanges();
  std::vector<rtype::protocol::EntityState> snapshot() const;
  std::vector<rtype::protocol::PlayerState> playerStates() const;
  rtype::protocol::GameStatus gameStatus() const;
  rtype::protocol::LevelInfo levelInfo() const;
  // Changes every time a level starts (and when a game starts): time to send LEVEL_INFO.
  std::uint32_t levelRevision() const { return _levelRevision; }

  // Server API for campaign control.
  const game::LevelProgress& levelProgress() const { return _levels->progress(); }
  const game::LevelDefinition& levelDefinition() const { return _levels->definition(); }
  void loadLevel(std::uint32_t number);
  // A player asks to skip the level it sees. Honoured only in a running tutorial game while
  // that level is still the current one: the next level starts, or the victory after the
  // final one. Returns whether the level was skipped.
  bool skipLevel(std::uint32_t level);
  bool isTutorial() const;
  rtype::protocol::GamePhase phase() const { return gameStatus().phase; }

  std::uint32_t tick() const { return _tick; }
  std::uint8_t tickRate() const { return _tickRate; }
  const game::WorldBounds& bounds() const { return _bounds; }
  engine::ecs::Registry& registry() { return _registry; }

private:
  enum class State { Lobby, Running, GameOver, Victory };

  std::optional<rtype::protocol::EntityState> describe(engine::ecs::Entity entity) const;
  void refreshPlayerStates();
  void startGame();
  void enterLobby();
  void clearNonPlayers();
  engine::ecs::Entity createShip(std::uint8_t playerId);
  bool everyoneIsOut() const;
  void refundLives();
  void trackLevel();

  engine::ecs::Registry _registry;
  engine::core::SystemScheduler<engine::ecs::Registry> _scheduler;
  game::WorldBounds _bounds;
  std::shared_ptr<const game::GameContent> _content;
  std::uint32_t _seed;
  std::uint32_t _initialLevel;
  std::optional<game::LevelDirector> _levels;
  std::uint8_t _tickRate;
  std::uint32_t _tick = 0;
  State _state = State::Lobby;
  float _stateSeconds = 0.0F; // countdown of GameOver and Victory
  rtype::protocol::GameMode _mode = rtype::protocol::GameMode::Story;
  rtype::protocol::Difficulty _difficulty = rtype::protocol::Difficulty::Normal;
  rtype::protocol::GameMode _requestedMode = rtype::protocol::GameMode::Story;
  rtype::protocol::Difficulty _requestedDifficulty = rtype::protocol::Difficulty::Normal;
  std::uint32_t _games = 0;
  std::uint32_t _levelRevision = 0;
  std::uint32_t _trackedLevel = 0;
  std::unordered_map<engine::ecs::Entity, std::uint32_t> _fireCooldowns;
  std::unordered_map<engine::ecs::Entity, rtype::protocol::EntityState> _known;
  std::array<engine::ecs::Entity, rtype::protocol::MAX_PLAYERS> _ships{};
  std::array<std::optional<rtype::protocol::PlayerState>, rtype::protocol::MAX_PLAYERS> _players;
};

} // namespace server
