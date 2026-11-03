#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

// Version 2 extends the contracts-v1 baseline with equipment state and pickup entity kinds.
// Keep docs/protocol/rfc.md synchronized with wire layout changes. Byte layouts are given in the RFC; every
// multi-byte field is big-endian and floats are IEEE 754 binary32.
namespace rtype::protocol {

constexpr std::uint16_t MAGIC = 0x5254; // "RT"
constexpr std::uint8_t VERSION = 2;
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
  Player = 1, Bydos = 2, PlayerMissile = 3, EnemyMissile = 4, Boss = 5,
  ExtraLife = 6, Heal = 7, RapidFire = 8, DoubleShot = 9, SpreadShot = 10, HeavyShot = 11
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

// ---- Client -> server -------------------------------------------------------------------
struct Connect {}; // empty payload
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
struct PlayerState { // 10 bytes
  std::uint8_t playerId = 0;
  std::uint8_t lives = 0; // 0: game over for this player
  std::uint8_t health = 0;
  std::uint32_t score = 0;
  std::uint8_t weapon = 0; // 0 standard, 1 double, 2 spread, 3 heavy
  std::uint16_t rapidFireMilliseconds = 0;
};
struct PlayerStatus { // u8 count + count * PlayerState, count <= MAX_PLAYERS
  std::vector<PlayerState> players;
};
struct PlayerLeft { // 2 bytes
  std::uint8_t playerId = 0;
  LeaveReason reason = LeaveReason::Quit;
};

constexpr std::size_t ENTITY_STATE_SIZE = 14;
constexpr std::size_t ENTITY_REMOVAL_SIZE = 5;
constexpr std::size_t PLAYER_STATE_SIZE = 10;
constexpr std::size_t MAX_SPAWNS_PER_PACKET = (MAX_PAYLOAD_SIZE - 2) / ENTITY_STATE_SIZE;
constexpr std::size_t MAX_UPDATES_PER_PACKET = (MAX_PAYLOAD_SIZE - 6) / ENTITY_STATE_SIZE;
constexpr std::size_t MAX_REMOVALS_PER_PACKET = (MAX_PAYLOAD_SIZE - 2) / ENTITY_REMOVAL_SIZE;

using Payload = std::variant<Connect, Input, Ping, Disconnect, Accept, Reject, Pong, EntitySpawn,
                             EntityUpdate, EntityDestroy, PlayerStatus, PlayerLeft>;

struct Packet {
  Header header;
  Payload payload;
};

} // namespace rtype::protocol
