#include "server/GameWorld.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "game/EquipmentSystems.hpp"

namespace server {

namespace protocol = rtype::protocol;
using namespace rtype::common::components;

namespace {

constexpr float PLAYER_SPEED = 250.0F; // world units per second
constexpr float MISSILE_SPEED = 600.0F;
constexpr float SHIP_X = 60.0F;
constexpr Hitbox SHIP_HITBOX{33.0F, 17.0F}; // size of a ship in the provided sprite sheet
constexpr int SHIP_HEALTH = 3;
// An entity removed while its top-left corner is this far inside the screen was visible, so
// it was destroyed in play (explosion); further out, it simply flew away.
constexpr float VISIBLE_MARGIN = 32.0F;

std::uint8_t toByte(int value) { return static_cast<std::uint8_t>(std::clamp(value, 0, 255)); }

std::uint16_t toU16(long long value) {
  return static_cast<std::uint16_t>(std::clamp(value, 0LL, 65535LL));
}

std::uint8_t secondsLeft(float seconds) {
  return static_cast<std::uint8_t>(std::clamp(std::ceil(seconds), 0.0F, 255.0F));
}

game::Difficulty toGame(protocol::Difficulty difficulty) {
  switch (difficulty) {
  case protocol::Difficulty::Easy:
  case protocol::Difficulty::Tutorial: return game::Difficulty::Easy;
  case protocol::Difficulty::Hard: return game::Difficulty::Hard;
  case protocol::Difficulty::Normal: break;
  }
  return game::Difficulty::Normal;
}

// LEVEL_INFO only carries printable ASCII within fixed bounds: content files are checked when
// they are loaded, this is the last line of defence for levels built in code.
std::string forWire(const std::string& text, std::size_t maximum) {
  std::string result = text.substr(0, maximum);
  for (auto& character : result) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte != '\n' && (byte < 0x20 || byte > 0x7E)) {
      character = '?';
    }
  }
  return result;
}

protocol::EntityKind kindOf(Visual visual) {
  switch (visual) {
  case Visual::Waver: return protocol::EntityKind::Waver;
  case Visual::Charger: return protocol::EntityKind::Charger;
  case Visual::Turret: return protocol::EntityKind::Turret;
  case Visual::Boss: return protocol::EntityKind::Boss;
  case Visual::FinalBoss: return protocol::EntityKind::FinalBoss;
  case Visual::BossShot: return protocol::EntityKind::BossMissile;
  case Visual::Bydos: break;
  }
  return protocol::EntityKind::Bydos;
}

protocol::EntityKind kindOf(PickupType type) {
  switch (type) {
  case PickupType::ExtraLife: return protocol::EntityKind::ExtraLife;
  case PickupType::Heal: return protocol::EntityKind::Heal;
  case PickupType::RapidFire: return protocol::EntityKind::RapidFire;
  case PickupType::DoubleShot: return protocol::EntityKind::DoubleShot;
  case PickupType::SpreadShot: return protocol::EntityKind::SpreadShot;
  case PickupType::HeavyShot: return protocol::EntityKind::HeavyShot;
  case PickupType::Shield: return protocol::EntityKind::Shield;
  case PickupType::Trophy: return protocol::EntityKind::Trophy;
  }
  return protocol::EntityKind::Heal;
}

} // namespace

GameWorld::GameWorld(std::uint8_t tickRate, std::uint32_t seed, game::WorldBounds bounds,
                     game::GameContent content, std::uint32_t initialLevel)
    : _bounds(bounds), _content(std::make_shared<const game::GameContent>(std::move(content))),
      _seed(seed), _initialLevel(initialLevel), _tickRate(tickRate) {
  if (tickRate == 0) {
    throw std::invalid_argument("tick rate must be positive");
  }
  if (initialLevel == 0) {
    throw std::invalid_argument("levels start at one");
  }
  _ships.fill(engine::ecs::INVALID_ENTITY);
  game::registerSystems(_registry);
  const float tickSeconds = 1.0F / static_cast<float>(tickRate);
  game::registerGameplaySystems(_scheduler, tickSeconds, _bounds);
  // A director exists from the start so the campaign can be inspected before anyone joins.
  _levels.emplace(_seed, _bounds, _content->story, _initialLevel, _content);
}

void GameWorld::requestGame(protocol::GameMode mode, protocol::Difficulty difficulty) {
  if (difficulty == protocol::Difficulty::Tutorial && mode != protocol::GameMode::Story) {
    difficulty = protocol::Difficulty::Normal;
  }
  _requestedMode = mode;
  _requestedDifficulty = difficulty;
}

bool GameWorld::isTutorial() const {
  return _state != State::Lobby && _mode == protocol::GameMode::Story &&
         _difficulty == protocol::Difficulty::Tutorial;
}

void GameWorld::startGame() {
  _mode = _requestedMode;
  _difficulty = _requestedDifficulty;
  const auto& catalog = _mode == protocol::GameMode::Story ? _content->story : _content->endless;
  auto level = _initialLevel;
  if (const auto last = catalog.finalLevel()) {
    level = std::min(level, *last);
  }
  ++_games;
  clearNonPlayers();
  _levels.emplace(_seed + _games, _bounds, catalog, level, _content, toGame(_difficulty));
  // Everyone connected starts again with a fresh ship, full lives and no score.
  for (std::uint8_t playerId = 0; playerId < protocol::MAX_PLAYERS; ++playerId) {
    if (_players[playerId]) {
      if (_registry.containsEntity(_ships[playerId])) {
        _fireCooldowns.erase(_ships[playerId]);
        _registry.killEntity(_ships[playerId]);
      }
      createShip(playerId);
    }
  }
  _state = State::Running;
  _stateSeconds = 0.0F;
  _trackedLevel = 0;
  trackLevel();
}

void GameWorld::enterLobby() {
  clearNonPlayers();
  _state = State::Lobby;
  _stateSeconds = 0.0F;
  // The next game is whatever the next first player asks for.
  _requestedMode = protocol::GameMode::Story;
  _requestedDifficulty = protocol::Difficulty::Normal;
}

void GameWorld::clearNonPlayers() {
  for (const auto entity : _registry.view<Position>()) {
    if (_registry.getComponent<Player>(entity) == nullptr) {
      _registry.killEntity(entity);
    }
  }
}

engine::ecs::Entity GameWorld::createShip(std::uint8_t playerId) {
  const int lives = game::difficultySettings(toGame(_difficulty)).playerLives;
  // Ships start on the left, spread over the height of the screen.
  const float laneY = _bounds.height * static_cast<float>(playerId + 1) /
                      static_cast<float>(protocol::MAX_PLAYERS + 1);
  const auto ship = _registry.spawnEntity();
  _registry.addComponent(ship, Position{SHIP_X, laneY - SHIP_HITBOX.height / 2.0F});
  _registry.addComponent(ship, Velocity{});
  _registry.addComponent(ship, Player{playerId, lives, 0});
  _registry.addComponent(ship, Equipment{});
  _registry.addComponent(ship, Health{SHIP_HEALTH, SHIP_HEALTH});
  _registry.addComponent(ship, SHIP_HITBOX);
  _ships[playerId] = ship;
  _players[playerId] = protocol::PlayerState{playerId, toByte(lives), SHIP_HEALTH, 0};
  return ship;
}

engine::ecs::Entity GameWorld::spawnPlayer(std::uint8_t playerId) {
  if (playerId >= protocol::MAX_PLAYERS) {
    throw std::invalid_argument("player id out of range");
  }
  if (_state == State::Lobby) {
    _players[playerId] = protocol::PlayerState{playerId};
    startGame(); // creates the ship of every connected player, this one included
    return _ships[playerId];
  }
  return createShip(playerId);
}

void GameWorld::removePlayer(std::uint8_t playerId, engine::ecs::Entity ship) {
  for (const auto entity : {ship, playerId < protocol::MAX_PLAYERS ? _ships[playerId]
                                                                   : engine::ecs::INVALID_ENTITY}) {
    if (_registry.containsEntity(entity) && _registry.getComponent<Player>(entity) != nullptr) {
      _registry.killEntity(entity);
    }
    _fireCooldowns.erase(entity);
  }
  if (playerId < protocol::MAX_PLAYERS) {
    _ships[playerId] = engine::ecs::INVALID_ENTITY;
    _players[playerId].reset();
  }
  if (std::ranges::none_of(_players, [](const auto& state) { return state.has_value(); })) {
    enterLobby();
  }
}

engine::ecs::Entity GameWorld::ship(std::uint8_t playerId) const {
  return playerId < protocol::MAX_PLAYERS ? _ships[playerId] : engine::ecs::INVALID_ENTITY;
}

void GameWorld::applyPlayerInput(std::uint8_t playerId, std::uint8_t flags) {
  applyInput(ship(playerId), flags);
}

void GameWorld::applyInput(engine::ecs::Entity ship, std::uint8_t flags) {
  // Les commandes réseau sont appliquées ici au monde simulé côté serveur.
  if (!_registry.containsEntity(ship) || _registry.getComponent<Player>(ship) == nullptr) {
    return;
  }
  const auto pressed = [flags](std::uint8_t bit) { return (flags & bit) != 0 ? 1.0F : 0.0F; };
  // Screen coordinates: y grows downwards.
  const float horizontal = pressed(protocol::input::RIGHT) - pressed(protocol::input::LEFT);
  const float vertical = pressed(protocol::input::DOWN) - pressed(protocol::input::UP);
  game::applyPlayerInput(_registry, ship, horizontal, vertical, PLAYER_SPEED);

  auto& cooldown = _fireCooldowns[ship];
  if ((flags & protocol::input::FIRE) != 0 && cooldown == 0) {
    game::fireEquippedWeapon(_registry, ship, MISSILE_SPEED);
    cooldown = std::max(1U, static_cast<std::uint32_t>(std::lround(
                                game::weaponCooldownSeconds(_registry, ship) *
                                static_cast<float>(_tickRate))));
  }
}

bool GameWorld::everyoneIsOut() const {
  bool anyone = false;
  for (std::uint8_t playerId = 0; playerId < protocol::MAX_PLAYERS; ++playerId) {
    if (!_players[playerId]) {
      continue;
    }
    anyone = true;
    const auto* player = _registry.getComponent<Player>(_ships[playerId]);
    if (_registry.containsEntity(_ships[playerId]) && player != nullptr && player->lives > 0) {
      return false;
    }
  }
  return anyone;
}

void GameWorld::trackLevel() {
  const auto level = _levels->progress().level;
  if (level != _trackedLevel) {
    _trackedLevel = level;
    ++_levelRevision;
  }
}

void GameWorld::step() {
  const float tickSeconds = 1.0F / static_cast<float>(_tickRate);
  for (auto& [ship, cooldown] : _fireCooldowns) {
    cooldown = cooldown > 0 ? cooldown - 1 : 0;
  }
  if (_state == State::Running) {
    _levels->update(_registry, tickSeconds);
  }
  _scheduler.run(_registry);
  if (isTutorial()) {
    refundLives();
  }
  ++_tick;
  refreshPlayerStates();

  switch (_state) {
  case State::Lobby: break;
  case State::Running:
    trackLevel();
    if (everyoneIsOut()) {
      _state = State::GameOver;
      _stateSeconds = GAME_OVER_SECONDS;
    } else if (_levels->progress().phase == game::LevelPhase::Complete) {
      _state = State::Victory;
      _stateSeconds = VICTORY_SECONDS;
    }
    break;
  case State::GameOver:
  case State::Victory:
    _stateSeconds -= tickSeconds;
    if (_stateSeconds <= 0.0F) {
      startGame();
    }
    break;
  }
}

void GameWorld::loadLevel(std::uint32_t number) {
  _levels->loadLevel(_registry, number);
  trackLevel();
}

bool GameWorld::skipLevel(std::uint32_t level) {
  if (_state != State::Running || !isTutorial()) {
    return false;
  }
  const auto& progress = _levels->progress();
  if (progress.level != level || progress.phase == game::LevelPhase::Complete) {
    return false; // stale or duplicated request: that level is already behind
  }
  const auto last = _levels->catalog().finalLevel();
  if (last && level >= *last) {
    clearNonPlayers();
    _state = State::Victory;
    _stateSeconds = VICTORY_SECONDS;
    return true;
  }
  loadLevel(level + 1);
  return true;
}

// Tutorial: lives never drop below their starting value, a lost one comes back on the same
// tick. The death system only removes a ship on its last life, so it always respawns.
void GameWorld::refundLives() {
  const int lives = game::difficultySettings(toGame(_difficulty)).playerLives;
  for (const auto ship : _ships) {
    auto* player = _registry.containsEntity(ship) ? _registry.getComponent<Player>(ship) : nullptr;
    if (player != nullptr && player->lives < lives) {
      player->lives = lives;
    }
  }
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
      state->weapon = 0;
      state->rapidFireMilliseconds = 0;
      state->flags = 0;
      _fireCooldowns.erase(ship);
      continue;
    }
    state->lives = toByte(player->lives);
    state->health = toByte(health->current);
    state->score = player->score;
    state->flags = 0;
    if (const auto* equipment = _registry.getComponent<Equipment>(ship)) {
      state->weapon = static_cast<std::uint8_t>(equipment->weapon);
      state->rapidFireMilliseconds = static_cast<std::uint16_t>(
          std::clamp(std::lround(equipment->rapidFireSeconds * 1000.0F), 0L, 65535L));
      if (equipment->shieldSeconds > 0.0F) {
        state->flags |= protocol::player::SHIELD;
      }
      if (equipment->invulnerableSeconds > 0.0F) {
        state->flags |= protocol::player::INVULNERABLE;
      }
    }
  }
}

protocol::GameStatus GameWorld::gameStatus() const {
  protocol::GameStatus status;
  status.mode = _mode;
  status.difficulty = _difficulty;
  if (_state == State::Lobby) {
    status.mode = _requestedMode;
    status.difficulty = _requestedDifficulty;
    status.phase = protocol::GamePhase::Lobby;
    return status;
  }
  const auto& progress = _levels->progress();
  status.level = toU16(progress.level);
  if (const auto last = _levels->catalog().finalLevel()) {
    status.levelCount = toU16(std::max(*last, progress.level));
  }
  status.waveCount = toByte(static_cast<int>(std::min(progress.waveCount, 255U)));
  status.wave = std::min(toByte(static_cast<int>(std::min(progress.wave, 255U))), status.waveCount);
  status.bossMaxHealth = toU16(progress.bossMaxHealth);
  status.bossHealth = std::min(toU16(progress.bossHealth), status.bossMaxHealth);

  switch (_state) {
  case State::GameOver:
    status.phase = protocol::GamePhase::GameOver;
    status.countdown = secondsLeft(_stateSeconds);
    return status;
  case State::Victory:
    status.phase = protocol::GamePhase::Victory;
    status.countdown = secondsLeft(_stateSeconds);
    return status;
  case State::Lobby:
  case State::Running: break;
  }
  switch (progress.phase) {
  case game::LevelPhase::Intro:
    status.phase = protocol::GamePhase::Intro;
    status.countdown = secondsLeft(progress.phaseRemainingSeconds);
    break;
  case game::LevelPhase::LevelPause:
    status.phase = protocol::GamePhase::StageClear;
    status.countdown = secondsLeft(progress.phaseRemainingSeconds);
    break;
  case game::LevelPhase::Complete: status.phase = protocol::GamePhase::Victory; break;
  case game::LevelPhase::Spawning:
  case game::LevelPhase::Clearing:
  case game::LevelPhase::WavePause:
    status.phase = progress.bossMaxHealth > 0 ? protocol::GamePhase::BossFight
                                              : protocol::GamePhase::Playing;
    break;
  }
  return status;
}

protocol::LevelInfo GameWorld::levelInfo() const {
  protocol::LevelInfo info;
  const auto& level = _levels->definition();
  info.level = toU16(_levels->progress().level);
  info.theme = static_cast<std::uint8_t>(level.theme % protocol::THEME_COUNT);
  info.title = forWire(level.title, protocol::MAX_TITLE_LENGTH);
  info.story = forWire(level.story, protocol::MAX_STORY_LENGTH);
  return info;
}

GameWorld::Changes GameWorld::collectChanges() {
  // Compare les entités au tick précédent pour détecter créations et destructions à diffuser.
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
  const auto* appearance = _registry.getComponent<Appearance>(entity);

  if (const auto* player = _registry.getComponent<Player>(entity)) {
    state.kind = protocol::EntityKind::Player;
    state.playerId = static_cast<std::uint8_t>(player->id);
  } else if (const auto* enemy = _registry.getComponent<Enemy>(entity)) {
    if (appearance != nullptr) {
      state.kind = kindOf(appearance->visual);
    } else {
      state.kind = enemy->pattern == Enemy::BOSS_PATTERN ? protocol::EntityKind::Boss
                                                         : protocol::EntityKind::Bydos;
    }
  } else if (const auto* projectile = _registry.getComponent<Projectile>(entity)) {
    state.kind = projectile->friendly ? protocol::EntityKind::PlayerMissile
                                      : protocol::EntityKind::EnemyMissile;
    if (!projectile->friendly && appearance != nullptr && appearance->visual == Visual::BossShot) {
      state.kind = protocol::EntityKind::BossMissile;
    }
    if (projectile->friendly && projectile->ownerId >= 0 &&
        projectile->ownerId < protocol::MAX_PLAYERS) {
      state.playerId = static_cast<std::uint8_t>(projectile->ownerId);
    }
  } else if (const auto* pickup = _registry.getComponent<Pickup>(entity)) {
    state.kind = kindOf(pickup->type);
  } else {
    return std::nullopt; // not something clients know how to draw
  }
  return state;
}

} // namespace server
