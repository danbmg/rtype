#include "server/GameWorld.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace server {

namespace protocol = rtype::protocol;
using namespace rtype::common::components;

namespace {

constexpr float PLAYER_SPEED = 250.0F; // world units per second
constexpr float MISSILE_SPEED = 600.0F;
constexpr float FIRE_COOLDOWN_SECONDS = 0.25F;
constexpr float SHIP_X = 60.0F;
constexpr Hitbox SHIP_HITBOX{33.0F, 17.0F}; // size of a ship in the provided sprite sheet
constexpr int SHIP_LIVES = 3;
constexpr int SHIP_HEALTH = 3;
// An entity removed while its top-left corner is this far inside the screen was visible, so
// it was destroyed in play (explosion); further out, it simply flew away.
constexpr float VISIBLE_MARGIN = 32.0F;

std::uint8_t toByte(int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); }

} // namespace

GameWorld::GameWorld(std::uint8_t tickRate, std::uint32_t seed, game::WorldBounds bounds)
    : _bounds(bounds), _tickRate(tickRate),
      _fireCooldownTicks(static_cast<std::uint32_t>(
          std::lround(FIRE_COOLDOWN_SECONDS * static_cast<float>(tickRate)))) {
  if (tickRate == 0) {
    throw std::invalid_argument("tick rate must be positive");
  }
  _ships.fill(engine::ecs::INVALID_ENTITY);
  game::registerSystems(_registry);
  const float tickSeconds = 1.0F / static_cast<float>(tickRate);
  game::registerEnemySystems(_scheduler, tickSeconds, seed, _bounds);
  game::registerGameplaySystems(_scheduler, tickSeconds, _bounds);
}

engine::ecs::Entity GameWorld::spawnPlayer(std::uint8_t playerId) {
  if (playerId >= protocol::MAX_PLAYERS) {
    throw std::invalid_argument("player id out of range");
  }
  // Ships start on the left, spread over the height of the screen.
  const float laneY = _bounds.height * static_cast<float>(playerId + 1) /
                      static_cast<float>(protocol::MAX_PLAYERS + 1);
  const auto ship = _registry.spawnEntity();
  _registry.addComponent(ship, Position{SHIP_X, laneY - SHIP_HITBOX.height / 2.0F});
  _registry.addComponent(ship, Velocity{});
  _registry.addComponent(ship, Player{playerId, SHIP_LIVES, 0});
  _registry.addComponent(ship, Health{SHIP_HEALTH, SHIP_HEALTH});
  _registry.addComponent(ship, SHIP_HITBOX);
  _ships[playerId] = ship;
  _players[playerId] = protocol::PlayerState{playerId, SHIP_LIVES, SHIP_HEALTH, 0};
  return ship;
}

void GameWorld::removePlayer(std::uint8_t playerId, engine::ecs::Entity ship) {
  if (_registry.containsEntity(ship)) {
    _registry.killEntity(ship);
  }
  _fireCooldowns.erase(ship);
  if (playerId < protocol::MAX_PLAYERS) {
    _ships[playerId] = engine::ecs::INVALID_ENTITY;
    _players[playerId].reset();
  }
}

void GameWorld::applyInput(engine::ecs::Entity ship, std::uint8_t flags) {
  if (!_registry.containsEntity(ship)) {
    return;
  }
  const auto pressed = [flags](std::uint8_t bit) { return (flags & bit) != 0 ? 1.0F : 0.0F; };
  // Screen coordinates: y grows downwards.
  const float horizontal = pressed(protocol::input::RIGHT) - pressed(protocol::input::LEFT);
  const float vertical = pressed(protocol::input::DOWN) - pressed(protocol::input::UP);
  game::applyPlayerInput(_registry, ship, horizontal, vertical, PLAYER_SPEED);

  auto& cooldown = _fireCooldowns[ship];
  if ((flags & protocol::input::FIRE) != 0 && cooldown == 0) {
    game::firePlayerProjectile(_registry, ship, MISSILE_SPEED);
    cooldown = _fireCooldownTicks;
  }
}

void GameWorld::step() {
  for (auto& [ship, cooldown] : _fireCooldowns) {
    cooldown = cooldown > 0 ? cooldown - 1 : 0;
  }
  _scheduler.run(_registry);
  ++_tick;
  refreshPlayerStates();
}

void GameWorld::refreshPlayerStates() {
  for (std::uint8_t playerId = 0; playerId < protocol::MAX_PLAYERS; ++playerId) {
    auto& state = _players[playerId];
    if (!state) {
      continue;
    }
    const auto ship = _ships[playerId];
    const auto* player = _registry.getComponent<Player>(ship);
    const auto* health = _registry.getComponent<Health>(ship);
    if (!_registry.containsEntity(ship) || player == nullptr || health == nullptr) {
      // Out of lives: the ship was removed. Keep the score for the HUD.
      state->lives = 0;
      state->health = 0;
      _fireCooldowns.erase(ship);
      continue;
    }
    state->lives = toByte(player->lives);
    state->health = toByte(health->current);
    state->score = player->score;
  }
}

GameWorld::Changes GameWorld::collectChanges() {
  Changes changes;
  std::unordered_map<engine::ecs::Entity, protocol::EntityState> current;
  for (const auto entity : _registry.view<Position>()) {
    if (const auto state = describe(entity)) {
      current.emplace(entity, *state);
      if (!_known.contains(entity)) {
        changes.spawned.push_back(*state);
      }
    }
  }
  for (const auto& [entity, state] : _known) {
    if (current.contains(entity)) {
      continue;
    }
    const bool visible = state.x > -VISIBLE_MARGIN && state.x < _bounds.width &&
                         state.y > -VISIBLE_MARGIN && state.y < _bounds.height;
    changes.destroyed.push_back({state.id, visible ? protocol::DestroyReason::Killed
                                                   : protocol::DestroyReason::LeftScreen});
  }
  _known = std::move(current);
  return changes;
}

std::vector<protocol::EntityState> GameWorld::snapshot() const {
  std::vector<protocol::EntityState> states;
  states.reserve(_known.size());
  for (const auto& [entity, state] : _known) {
    states.push_back(state);
  }
  std::ranges::sort(states, {}, &protocol::EntityState::id);
  return states;
}

std::vector<protocol::PlayerState> GameWorld::playerStates() const {
  std::vector<protocol::PlayerState> states;
  for (const auto& state : _players) {
    if (state) {
      states.push_back(*state);
    }
  }
  return states;
}

std::optional<protocol::EntityState> GameWorld::describe(engine::ecs::Entity entity) const {
  const auto* position = _registry.getComponent<Position>(entity);
  // A non-finite coordinate would make clients reject the whole datagram: never send it.
  if (position == nullptr || !std::isfinite(position->x) || !std::isfinite(position->y)) {
    return std::nullopt;
  }
  protocol::EntityState state;
  state.id = entity;
  state.x = position->x;
  state.y = position->y;

  if (const auto* player = _registry.getComponent<Player>(entity)) {
    state.kind = protocol::EntityKind::Player;
    state.playerId = static_cast<std::uint8_t>(player->id);
  } else if (_registry.getComponent<Enemy>(entity) != nullptr) {
    state.kind = protocol::EntityKind::Bydos;
  } else if (const auto* projectile = _registry.getComponent<Projectile>(entity)) {
    state.kind = projectile->friendly ? protocol::EntityKind::PlayerMissile
                                      : protocol::EntityKind::EnemyMissile;
    if (projectile->friendly && projectile->ownerId >= 0 &&
        projectile->ownerId < protocol::MAX_PLAYERS) {
      state.playerId = static_cast<std::uint8_t>(projectile->ownerId);
    }
  } else {
    return std::nullopt; // not something clients know how to draw
  }
  return state;
}

} // namespace server
