#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

// Contract shared by the client and the server. Version 2 added equipment and pickups; version 3
// adds game modes and difficulty (CONNECT), the game state (GAME_STATUS), level titles and story
// (LEVEL_INFO), new enemy kinds and player flags. Any change is a dedicated PR approved by the
// whole team and updates docs/protocol/rfc.md in the same PR. Byte layouts are given in the RFC;
// every multi-byte field is big-endian and floats are IEEE 754 binary32.
namespace rtype::protocol {

constexpr std::uint16_t MAGIC = 0x5254; // "RT"
constexpr std::uint8_t VERSION = 3;
constexpr std::uint16_t DEFAULT_PORT = 4242;

// Tous les échanges suivent ce même en-tête, puis un payload propre au type de paquet.
constexpr std::size_t HEADER_SIZE = 10;
// Fits the minimum IPv6 MTU (1280 bytes) with IP and UDP headers: never fragmented.
constexpr std::size_t MAX_DATAGRAM_SIZE = 1200;
constexpr std::size_t MAX_PAYLOAD_SIZE = MAX_DATAGRAM_SIZE - HEADER_SIZE;

constexpr std::uint8_t MAX_PLAYERS = 4;
constexpr std::uint8_t NO_PLAYER = 0xFF;

// A client sends at least one packet per heartbeat interval (inputs or PING).
// A peer that stays silent for TIMEOUT_MS is considered gone (crashed or disconnected).
constexpr std::uint32_t HEARTBEAT_INTERVAL_MS = 1000;
constexpr std::uint32_t TIMEOUT_MS = 3000;

enum class PacketType : std::uint8_t {
  // Session, client -> server
  Connect = 0x01,
  Input = 0x02,
  Ping = 0x03,
  Disconnect = 0x04,
  // Session, server -> client
  Accept = 0x08,
  Reject = 0x09,
  Pong = 0x0A,
  // World state, server -> client
  EntitySpawn = 0x10,
  EntityUpdate = 0x11,
  EntityDestroy = 0x12,
  PlayerStatus = 0x13,
  PlayerLeft = 0x14,
  GameStatus = 0x15,
  LevelInfo = 0x16,
};

struct Header {
  std::uint16_t magic = MAGIC;
  std::uint8_t version = VERSION;
  PacketType type = PacketType::Connect;
  std::uint32_t sequence = 0; // Numéro par expéditeur pour repérer les entrées UDP reçues dans le désordre.
  std::uint16_t payloadSize = 0;
};

enum class RejectReason : std::uint8_t { ServerFull = 1, VersionMismatch = 2 };
enum class EntityKind : std::uint8_t {
  Player = 1,
  Bydos = 2,
  PlayerMissile = 3,
  EnemyMissile = 4,
  Boss = 5, // first boss (level 5 of the story)
  // Pickups
  ExtraLife = 6,
  Heal = 7,
  RapidFire = 8,
  DoubleShot = 9,
  SpreadShot = 10,
  HeavyShot = 11,
  Shield = 12,
  Trophy = 13, // the Summoner's Cup, dropped by the final boss
  // Enemies added in version 3
  Waver = 14,     // sine-wave flyer
  Charger = 15,   // homing kamikaze
  Turret = 16,    // slow gun platform
  FinalBoss = 17, // last boss of the story
  BossMissile = 18,
};
constexpr std::uint8_t LAST_ENTITY_KIND = static_cast<std::uint8_t>(EntityKind::BossMissile);

enum class GameMode : std::uint8_t { Story = 0, Endless = 1 };
enum class Difficulty : std::uint8_t { Easy = 0, Normal = 1, Hard = 2 };
// Lobby: nobody plays yet. Intro: level title card, no enemy. StageClear: last wave of a level
// cleared, next level soon. Victory and GameOver end the game; it restarts after the countdown.
enum class GamePhase : std::uint8_t {
  Lobby = 0,
  Intro = 1,
  Playing = 2,
  BossFight = 3,
  StageClear = 4,
  Victory = 5,
  GameOver = 6,
};
// Killed: destroyed on screen (show an explosion). LeftScreen: flew out of view.
enum class DestroyReason : std::uint8_t { Killed = 1, LeftScreen = 2 };
enum class LeaveReason : std::uint8_t { Quit = 1, TimedOut = 2 };

// Bits of Input::flags; the other bits must be zero.
namespace input {
constexpr std::uint8_t UP = 0x01;
constexpr std::uint8_t DOWN = 0x02;
constexpr std::uint8_t LEFT = 0x04;
constexpr std::uint8_t RIGHT = 0x08;
constexpr std::uint8_t FIRE = 0x10;
constexpr std::uint8_t ALL = UP | DOWN | LEFT | RIGHT | FIRE;
} // namespace input

// Bits of PlayerState::flags; the other bits must be zero.
namespace player {
constexpr std::uint8_t SHIELD = 0x01;       // the next hit is absorbed
constexpr std::uint8_t INVULNERABLE = 0x02; // just respawned: enemy fire passes through
constexpr std::uint8_t ALL = SHIELD | INVULNERABLE;
} // namespace player

// Bounds of the LEVEL_INFO texts: printable ASCII (0x20 to 0x7E) and line feeds only.
constexpr std::size_t MAX_TITLE_LENGTH = 48;
constexpr std::size_t MAX_STORY_LENGTH = 512;
constexpr std::uint8_t THEME_COUNT = 5;

// ---- Client -> server -------------------------------------------------------------------
struct Connect { // 2 bytes
  // Applied only when this player starts a new game (nobody else connected): the others join
  // the game in progress, whatever they asked for.
  GameMode mode = GameMode::Story;
  Difficulty difficulty = Difficulty::Normal;
};
struct Input {     // 1 byte
  std::uint8_t flags = 0;
};
struct Ping {              // 4 bytes
  std::uint32_t token = 0; // echoed back in Pong
};
struct Disconnect {}; // empty payload

// ---- Server -> client -------------------------------------------------------------------
struct Accept {                 // 6 bytes
  std::uint8_t playerId = 0;    // 0 to MAX_PLAYERS - 1
  std::uint16_t worldWidth = 0; // world units; the client scales them to its window
  std::uint16_t worldHeight = 0;
  std::uint8_t tickRate = 0; // server simulation steps per second
};
struct Reject { // 1 byte
  RejectReason reason = RejectReason::ServerFull;
};
struct Pong { // 4 bytes
  std::uint32_t token = 0;
};

struct EntityState {    // 14 bytes
  std::uint32_t id = 0; // never reused by the server
  EntityKind kind = EntityKind::Player;
  std::uint8_t playerId = NO_PLAYER; // ship owner, or shooter of a player missile
  float x = 0.0F;                    // top-left corner, world units
  float y = 0.0F;
};
struct EntitySpawn { // u16 count + count * EntityState
  std::vector<EntityState> entities;
};
struct EntityUpdate {     // u32 tick + u16 count + count * EntityState
  std::uint32_t tick = 0; // server tick of the snapshot; older ones are ignored
  std::vector<EntityState> entities;
};
struct EntityRemoval { // 5 bytes
  std::uint32_t id = 0;
  DestroyReason reason = DestroyReason::Killed;
};
struct EntityDestroy { // u16 count + count * EntityRemoval
  std::vector<EntityRemoval> entities;
};
struct PlayerState { // 11 bytes
  std::uint8_t playerId = 0;
  std::uint8_t lives = 0; // 0: game over for this player
  std::uint8_t health = 0;
  std::uint32_t score = 0;
  std::uint8_t weapon = 0; // 0 standard, 1 double, 2 spread, 3 heavy
  std::uint16_t rapidFireMilliseconds = 0;
  std::uint8_t flags = 0; // player::SHIELD, player::INVULNERABLE
};
struct PlayerStatus { // u8 count + count * PlayerState, count <= MAX_PLAYERS
  std::vector<PlayerState> players;
};
struct PlayerLeft { // 2 bytes
  std::uint8_t playerId = 0;
  LeaveReason reason = LeaveReason::Quit;
};
struct GameStatus { // 14 bytes, sent with every snapshot
  GameMode mode = GameMode::Story;
  Difficulty difficulty = Difficulty::Normal;
  GamePhase phase = GamePhase::Lobby;
  std::uint16_t level = 0;      // 1-based, 0 in the lobby
  std::uint16_t levelCount = 0; // levels of the story; 0 in endless mode
  std::uint8_t wave = 0;        // 1-based within the level
  std::uint8_t waveCount = 0;
  std::uint16_t bossHealth = 0;
  std::uint16_t bossMaxHealth = 0; // 0: no boss on screen
  std::uint8_t countdown = 0;      // seconds left in Intro, StageClear, Victory or GameOver
};
struct LevelInfo { // 6 + title + story bytes, sent when a level starts and every second
  std::uint16_t level = 0;
  std::uint8_t theme = 0; // background palette, 0 to THEME_COUNT - 1
  std::string title;      // at most MAX_TITLE_LENGTH bytes
  std::string story;      // at most MAX_STORY_LENGTH bytes
};

constexpr std::size_t ENTITY_STATE_SIZE = 14;
constexpr std::size_t ENTITY_REMOVAL_SIZE = 5;
constexpr std::size_t PLAYER_STATE_SIZE = 11;
constexpr std::size_t GAME_STATUS_SIZE = 14;
constexpr std::size_t MAX_SPAWNS_PER_PACKET = (MAX_PAYLOAD_SIZE - 2) / ENTITY_STATE_SIZE;
constexpr std::size_t MAX_UPDATES_PER_PACKET = (MAX_PAYLOAD_SIZE - 6) / ENTITY_STATE_SIZE;
constexpr std::size_t MAX_REMOVALS_PER_PACKET = (MAX_PAYLOAD_SIZE - 2) / ENTITY_REMOVAL_SIZE;

using Payload = std::variant<Connect, Input, Ping, Disconnect, Accept, Reject, Pong, EntitySpawn,
                             EntityUpdate, EntityDestroy, PlayerStatus, PlayerLeft, GameStatus,
                             LevelInfo>;

struct Packet {
  Header header;
  Payload payload;
};

} // namespace rtype::protocol
