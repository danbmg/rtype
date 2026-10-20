#include "common/protocol/PacketCodec.hpp"

#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "engine/network/BinaryReader.hpp"
#include "engine/network/BinaryWriter.hpp"

namespace rtype::protocol {

namespace {

using engine::network::BinaryReader;
using engine::network::BinaryWriter;

constexpr std::size_t PAYLOAD_SIZE_OFFSET = 8;

bool isKnownType(std::uint8_t type) {
  switch (static_cast<PacketType>(type)) {
  case PacketType::Connect:
  case PacketType::Input:
  case PacketType::Ping:
  case PacketType::Disconnect:
  case PacketType::Accept:
  case PacketType::Reject:
  case PacketType::Pong:
  case PacketType::EntitySpawn:
  case PacketType::EntityUpdate:
  case PacketType::EntityDestroy:
  case PacketType::PlayerStatus:
  case PacketType::PlayerLeft: return true;
  }
  return false;
}

bool isValidPlayerId(std::uint8_t playerId) { return playerId < MAX_PLAYERS; }

template<typename Enum>
std::uint8_t toByte(Enum value) {
  return static_cast<std::uint8_t>(value);
}

void requireCount(std::size_t count, std::size_t maximum) {
  if (count > maximum) {
    throw std::length_error("protocol: list too long for one datagram, split it");
  }
}

// ---- Encoding ---------------------------------------------------------------------------

void writeEntityState(BinaryWriter& writer, const EntityState& state) {
  writer.writeU32(state.id);
  writer.writeU8(toByte(state.kind));
  writer.writeU8(state.playerId);
  writer.writeF32(state.x);
  writer.writeF32(state.y);
}

struct PayloadWriter {
  BinaryWriter& writer;

  void operator()(const Connect&) const {}
  void operator()(const Disconnect&) const {}
  void operator()(const Input& input) const { writer.writeU8(input.flags); }
  void operator()(const Ping& ping) const { writer.writeU32(ping.token); }
  void operator()(const Pong& pong) const { writer.writeU32(pong.token); }
  void operator()(const Reject& reject) const { writer.writeU8(toByte(reject.reason)); }

  void operator()(const Accept& accept) const {
    writer.writeU8(accept.playerId);
    writer.writeU16(accept.worldWidth);
    writer.writeU16(accept.worldHeight);
    writer.writeU8(accept.tickRate);
  }

  void operator()(const EntitySpawn& spawn) const {
    requireCount(spawn.entities.size(), MAX_SPAWNS_PER_PACKET);
    writer.writeU16(static_cast<std::uint16_t>(spawn.entities.size()));
    for (const auto& state : spawn.entities) {
      writeEntityState(writer, state);
    }
  }

  void operator()(const EntityUpdate& update) const {
    requireCount(update.entities.size(), MAX_UPDATES_PER_PACKET);
    writer.writeU32(update.tick);
    writer.writeU16(static_cast<std::uint16_t>(update.entities.size()));
    for (const auto& state : update.entities) {
      writeEntityState(writer, state);
    }
  }

  void operator()(const EntityDestroy& destroy) const {
    requireCount(destroy.entities.size(), MAX_REMOVALS_PER_PACKET);
    writer.writeU16(static_cast<std::uint16_t>(destroy.entities.size()));
    for (const auto& removal : destroy.entities) {
      writer.writeU32(removal.id);
      writer.writeU8(toByte(removal.reason));
    }
  }

  void operator()(const PlayerStatus& status) const {
    requireCount(status.players.size(), MAX_PLAYERS);
    writer.writeU8(static_cast<std::uint8_t>(status.players.size()));
    for (const auto& player : status.players) {
      writer.writeU8(player.playerId);
      writer.writeU8(player.lives);
      writer.writeU8(player.health);
      writer.writeU32(player.score);
    }
  }

  void operator()(const PlayerLeft& left) const {
    writer.writeU8(left.playerId);
    writer.writeU8(toByte(left.reason));
  }
};

// ---- Decoding ---------------------------------------------------------------------------
// Each reader first checks that the remaining length is exactly what the packet needs, then
// reads, then validates the values. A failure returns SizeMismatch or InvalidValue.

DecodeError readEntityState(BinaryReader& reader, EntityState& state) {
  state.id = reader.readU32();
  const auto kind = reader.readU8();
  state.playerId = reader.readU8();
  state.x = reader.readF32();
  state.y = reader.readF32();
  if (!reader.ok()) {
    return DecodeError::SizeMismatch;
  }
  if (kind < toByte(EntityKind::Player) || kind > toByte(EntityKind::EnemyMissile) ||
      (state.playerId != NO_PLAYER && !isValidPlayerId(state.playerId)) ||
      !std::isfinite(state.x) || !std::isfinite(state.y)) {
    return DecodeError::InvalidValue;
  }
  state.kind = static_cast<EntityKind>(kind);
  return DecodeError::None;
}

DecodeError readEntityStates(BinaryReader& reader, std::size_t maximum,
                             std::vector<EntityState>& states) {
  const std::size_t count = reader.readU16();
  if (!reader.ok() || count > maximum || reader.remaining() != count * ENTITY_STATE_SIZE) {
    return DecodeError::SizeMismatch;
  }
  states.resize(count);
  for (auto& state : states) {
    if (const auto error = readEntityState(reader, state); error != DecodeError::None) {
      return error;
    }
  }
  return DecodeError::None;
}

DecodeError decodePayload(PacketType type, BinaryReader& reader, Payload& payload) {
  const auto expect = [&reader](std::size_t size) { return reader.remaining() == size; };

  switch (type) {
  case PacketType::Connect:
    payload = Connect{};
    return expect(0) ? DecodeError::None : DecodeError::SizeMismatch;

  case PacketType::Disconnect:
    payload = Disconnect{};
    return expect(0) ? DecodeError::None : DecodeError::SizeMismatch;

  case PacketType::Input: {
    if (!expect(1)) {
      return DecodeError::SizeMismatch;
    }
    const Input input{reader.readU8()};
    payload = input;
    return (input.flags & ~input::ALL) == 0 ? DecodeError::None : DecodeError::InvalidValue;
  }

  case PacketType::Ping:
  case PacketType::Pong: {
    if (!expect(4)) {
      return DecodeError::SizeMismatch;
    }
    const auto token = reader.readU32();
    payload = type == PacketType::Ping ? Payload{Ping{token}} : Payload{Pong{token}};
    return DecodeError::None;
  }

  case PacketType::Accept: {
    if (!expect(6)) {
      return DecodeError::SizeMismatch;
    }
    Accept accept;
    accept.playerId = reader.readU8();
    accept.worldWidth = reader.readU16();
    accept.worldHeight = reader.readU16();
    accept.tickRate = reader.readU8();
    payload = accept;
    return isValidPlayerId(accept.playerId) && accept.worldWidth > 0 && accept.worldHeight > 0 &&
                   accept.tickRate > 0
               ? DecodeError::None
               : DecodeError::InvalidValue;
  }

  case PacketType::Reject: {
    if (!expect(1)) {
      return DecodeError::SizeMismatch;
    }
    const auto reason = reader.readU8();
    if (reason < toByte(RejectReason::ServerFull) ||
        reason > toByte(RejectReason::VersionMismatch)) {
      return DecodeError::InvalidValue;
    }
    payload = Reject{static_cast<RejectReason>(reason)};
    return DecodeError::None;
  }

  case PacketType::EntitySpawn: {
    EntitySpawn spawn;
    const auto error = readEntityStates(reader, MAX_SPAWNS_PER_PACKET, spawn.entities);
    payload = std::move(spawn);
    return error;
  }

  case PacketType::EntityUpdate: {
    EntityUpdate update;
    update.tick = reader.readU32();
    const auto error = readEntityStates(reader, MAX_UPDATES_PER_PACKET, update.entities);
    payload = std::move(update);
    return error;
  }

  case PacketType::EntityDestroy: {
    const std::size_t count = reader.readU16();
    if (!reader.ok() || count > MAX_REMOVALS_PER_PACKET ||
        reader.remaining() != count * ENTITY_REMOVAL_SIZE) {
      return DecodeError::SizeMismatch;
    }
    EntityDestroy destroy;
    destroy.entities.resize(count);
    for (auto& removal : destroy.entities) {
      removal.id = reader.readU32();
      const auto reason = reader.readU8();
      if (reason < toByte(DestroyReason::Killed) || reason > toByte(DestroyReason::LeftScreen)) {
        return DecodeError::InvalidValue;
      }
      removal.reason = static_cast<DestroyReason>(reason);
    }
    payload = std::move(destroy);
    return DecodeError::None;
  }

  case PacketType::PlayerStatus: {
    const std::size_t count = reader.readU8();
    if (!reader.ok() || count > MAX_PLAYERS || reader.remaining() != count * PLAYER_STATE_SIZE) {
      return DecodeError::SizeMismatch;
    }
    PlayerStatus status;
    status.players.resize(count);
    for (auto& player : status.players) {
      player.playerId = reader.readU8();
      player.lives = reader.readU8();
      player.health = reader.readU8();
      player.score = reader.readU32();
      if (!isValidPlayerId(player.playerId)) {
        return DecodeError::InvalidValue;
      }
    }
    payload = std::move(status);
    return DecodeError::None;
  }

  case PacketType::PlayerLeft: {
    if (!expect(2)) {
      return DecodeError::SizeMismatch;
    }
    const auto playerId = reader.readU8();
    const auto reason = reader.readU8();
    if (!isValidPlayerId(playerId) || reason < toByte(LeaveReason::Quit) ||
        reason > toByte(LeaveReason::TimedOut)) {
      return DecodeError::InvalidValue;
    }
    payload = PlayerLeft{playerId, static_cast<LeaveReason>(reason)};
    return DecodeError::None;
  }
  }
  return DecodeError::UnknownType;
}

DecodeResult fail(DecodeError error) { return {std::nullopt, error}; }

} // namespace

PacketType packetTypeOf(const Payload& payload) {
  return std::visit(
      [](const auto& value) -> PacketType {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Connect>) {
          return PacketType::Connect;
        } else if constexpr (std::is_same_v<T, Input>) {
          return PacketType::Input;
        } else if constexpr (std::is_same_v<T, Ping>) {
          return PacketType::Ping;
        } else if constexpr (std::is_same_v<T, Disconnect>) {
          return PacketType::Disconnect;
        } else if constexpr (std::is_same_v<T, Accept>) {
          return PacketType::Accept;
        } else if constexpr (std::is_same_v<T, Reject>) {
          return PacketType::Reject;
        } else if constexpr (std::is_same_v<T, Pong>) {
          return PacketType::Pong;
        } else if constexpr (std::is_same_v<T, EntitySpawn>) {
          return PacketType::EntitySpawn;
        } else if constexpr (std::is_same_v<T, EntityUpdate>) {
          return PacketType::EntityUpdate;
        } else if constexpr (std::is_same_v<T, EntityDestroy>) {
          return PacketType::EntityDestroy;
        } else if constexpr (std::is_same_v<T, PlayerStatus>) {
          return PacketType::PlayerStatus;
        } else {
          static_assert(std::is_same_v<T, PlayerLeft>, "every payload needs a packet type");
          return PacketType::PlayerLeft;
        }
      },
      payload);
}

std::vector<std::uint8_t> encode(std::uint32_t sequence, const Payload& payload) {
  // Convertit les types C++ en octets selon le contrat réseau commun.
  BinaryWriter writer(HEADER_SIZE + 64);
  writer.writeU16(MAGIC);
  writer.writeU8(VERSION);
  writer.writeU8(toByte(packetTypeOf(payload)));
  writer.writeU32(sequence);
  writer.writeU16(0); // payload size, patched once the payload is written
  std::visit(PayloadWriter{writer}, payload);

  const auto payloadSize = writer.size() - HEADER_SIZE;
  if (payloadSize > MAX_PAYLOAD_SIZE) {
    throw std::length_error("protocol: payload does not fit in one datagram");
  }
  writer.patchU16(PAYLOAD_SIZE_OFFSET, static_cast<std::uint16_t>(payloadSize));
  return writer.release();
}

DecodeResult decode(std::span<const std::uint8_t> datagram) {
  // Valide le datagram avant de construire un payload utilisable par le client ou le serveur.
  if (datagram.size() < HEADER_SIZE) {
    return fail(DecodeError::TooShort);
  }
  if (datagram.size() > MAX_DATAGRAM_SIZE) {
    return fail(DecodeError::TooLong);
  }

  BinaryReader reader(datagram);
  Header header;
  header.magic = reader.readU16();
  if (header.magic != MAGIC) {
    return fail(DecodeError::BadMagic);
  }
  header.version = reader.readU8();
  if (header.version != VERSION) {
    return fail(DecodeError::BadVersion);
  }
  const auto type = reader.readU8();
  if (!isKnownType(type)) {
    return fail(DecodeError::UnknownType);
  }
  header.type = static_cast<PacketType>(type);
  header.sequence = reader.readU32();
  header.payloadSize = reader.readU16();
  if (header.payloadSize != reader.remaining()) {
    return fail(DecodeError::SizeMismatch);
  }

  Payload payload;
  if (const auto error = decodePayload(header.type, reader, payload); error != DecodeError::None) {
    return fail(error);
  }
  if (!reader.ok() || reader.remaining() != 0) {
    return fail(DecodeError::SizeMismatch);
  }
  return {Packet{header, std::move(payload)}, DecodeError::None};
}

std::string_view toString(DecodeError error) {
  switch (error) {
  case DecodeError::None: return "none";
  case DecodeError::TooShort: return "too short";
  case DecodeError::TooLong: return "too long";
  case DecodeError::BadMagic: return "bad magic";
  case DecodeError::BadVersion: return "bad version";
  case DecodeError::UnknownType: return "unknown type";
  case DecodeError::SizeMismatch: return "size mismatch";
  case DecodeError::InvalidValue: return "invalid value";
  }
  return "unknown";
}

} // namespace rtype::protocol
