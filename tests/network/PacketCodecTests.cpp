#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "common/protocol/PacketCodec.hpp"

namespace protocol = rtype::protocol;
using protocol::DecodeError;

namespace {

protocol::EntityState ship(std::uint32_t id, float x) {
  return {id, protocol::EntityKind::Player, 2, x, 300.5F};
}

DecodeError errorOf(const std::vector<std::uint8_t>& datagram) {
  return protocol::decode(datagram).error;
}

// A datagram with a valid header around an arbitrary payload.
std::vector<std::uint8_t> withHeader(protocol::PacketType type,
                                     const std::vector<std::uint8_t>& payload) {
  std::vector<std::uint8_t> datagram{0x52,
                                     0x54,
                                     protocol::VERSION,
                                     static_cast<std::uint8_t>(type),
                                     0,
                                     0,
                                     0,
                                     1,
                                     static_cast<std::uint8_t>(payload.size() >> 8U),
                                     static_cast<std::uint8_t>(payload.size() & 0xFFU)};
  datagram.insert(datagram.end(), payload.begin(), payload.end());
  return datagram;
}

std::vector<protocol::Payload> samplePayloads() {
  return {
      protocol::Connect{},
      protocol::Input{protocol::input::UP | protocol::input::FIRE},
      protocol::Ping{0xDEADBEEF},
      protocol::Disconnect{},
      protocol::Accept{3, 800, 600, 60},
      protocol::Reject{protocol::RejectReason::VersionMismatch},
      protocol::Pong{42},
      protocol::EntitySpawn{
          {ship(1, 10.0F), {2, protocol::EntityKind::Bydos, protocol::NO_PLAYER, 800.0F, -12.25F}}},
      protocol::EntityUpdate{123456, {ship(1, 11.0F)}},
      protocol::EntityDestroy{
          {{9, protocol::DestroyReason::Killed}, {10, protocol::DestroyReason::LeftScreen}}},
      protocol::PlayerStatus{{{0, 3, 2, 1500}, {3, 0, 0, 4294967295U}}},
      protocol::PlayerLeft{1, protocol::LeaveReason::TimedOut},
  };
}

} // namespace

TEST_CASE("Every packet type survives an encode/decode round trip", "[protocol][codec]") {
  std::uint32_t sequence = 100;
  for (const auto& payload : samplePayloads()) {
    const auto datagram = protocol::encode(sequence, payload);
    const auto result = protocol::decode(datagram);
    REQUIRE(result.error == DecodeError::None);
    REQUIRE(result.packet.has_value());
    CHECK(result.packet->header.sequence == sequence);
    CHECK(result.packet->header.type == protocol::packetTypeOf(payload));
    CHECK(result.packet->header.payloadSize == datagram.size() - protocol::HEADER_SIZE);
    CHECK(result.packet->payload.index() == payload.index());
    // Decoding is exact: re-encoding gives the same bytes.
    CHECK(protocol::encode(sequence, result.packet->payload) == datagram);
    ++sequence;
  }
}

TEST_CASE("Decoded values are the ones that were sent", "[protocol][codec]") {
  const auto result = protocol::decode(protocol::encode(
      7, protocol::EntityUpdate{
             99,
             {ship(5, -3.5F),
              {6, protocol::EntityKind::EnemyMissile, protocol::NO_PLAYER, 1.0F, 2.0F}}}));
  REQUIRE(result.packet);
  const auto& update = std::get<protocol::EntityUpdate>(result.packet->payload);
  CHECK(update.tick == 99);
  REQUIRE(update.entities.size() == 2);
  CHECK(update.entities[0].id == 5);
  CHECK(update.entities[0].kind == protocol::EntityKind::Player);
  CHECK(update.entities[0].playerId == 2);
  CHECK(update.entities[0].x == -3.5F);
  CHECK(update.entities[0].y == 300.5F);
  CHECK(update.entities[1].kind == protocol::EntityKind::EnemyMissile);
}

TEST_CASE("The header layout matches the RFC byte for byte", "[protocol][codec]") {
  const auto datagram =
      protocol::encode(7, protocol::Input{protocol::input::UP | protocol::input::FIRE});
  const std::vector<std::uint8_t> expected{0x52, 0x54, 0x01, 0x02, 0x00, 0x00,
                                           0x00, 0x07, 0x00, 0x01, 0x11};
  CHECK(datagram == expected);
}

TEST_CASE("Truncated datagrams are rejected at every length", "[protocol][codec][malformed]") {
  for (const auto& payload : samplePayloads()) {
    const auto datagram = protocol::encode(1, payload);
    for (std::size_t length = 0; length < datagram.size(); ++length) {
      const std::vector<std::uint8_t> truncated(
          datagram.begin(), datagram.begin() + static_cast<std::ptrdiff_t>(length));
      const auto error = errorOf(truncated);
      CHECK((error == DecodeError::TooShort || error == DecodeError::SizeMismatch));
    }
  }
}

TEST_CASE("Oversized datagrams and extra bytes are rejected", "[protocol][codec][malformed]") {
  std::vector<std::uint8_t> huge(protocol::MAX_DATAGRAM_SIZE + 1, 0);
  huge[0] = 0x52;
  huge[1] = 0x54;
  CHECK(errorOf(huge) == DecodeError::TooLong);

  auto padded = protocol::encode(1, protocol::Ping{1});
  padded.push_back(0);
  CHECK(errorOf(padded) == DecodeError::SizeMismatch);

  // Declared size bigger, then smaller, than the payload actually received.
  auto lying = protocol::encode(1, protocol::Ping{1});
  lying[9] = 5;
  CHECK(errorOf(lying) == DecodeError::SizeMismatch);
  lying[9] = 3;
  CHECK(errorOf(lying) == DecodeError::SizeMismatch);
}

TEST_CASE("Foreign or unknown datagrams are rejected", "[protocol][codec][malformed]") {
  auto datagram = protocol::encode(1, protocol::Connect{});

  auto badMagic = datagram;
  badMagic[0] = 'H';
  CHECK(errorOf(badMagic) == DecodeError::BadMagic);

  auto badVersion = datagram;
  badVersion[2] = protocol::VERSION + 1;
  CHECK(errorOf(badVersion) == DecodeError::BadVersion);

  for (const int type : {0x00, 0x05, 0x0B, 0x15, 0x7F, 0xFF}) {
    auto unknown = datagram;
    unknown[3] = static_cast<std::uint8_t>(type);
    CHECK(errorOf(unknown) == DecodeError::UnknownType);
  }
}

TEST_CASE("List counts must match the payload length", "[protocol][codec][malformed]") {
  // The count announces 65535 entities in a 2-byte payload: rejected before any allocation.
  CHECK(errorOf(withHeader(protocol::PacketType::EntitySpawn, {0xFF, 0xFF})) ==
        DecodeError::SizeMismatch);
  // Count 2, one entity present.
  auto oneEntity = protocol::encode(1, protocol::EntitySpawn{{ship(1, 0.0F)}});
  oneEntity[protocol::HEADER_SIZE + 1] = 2;
  CHECK(errorOf(oneEntity) == DecodeError::SizeMismatch);
  // More players than a game can hold.
  std::vector<std::uint8_t> fivePlayers{5};
  fivePlayers.resize(1 + 5 * protocol::PLAYER_STATE_SIZE, 0);
  CHECK(errorOf(withHeader(protocol::PacketType::PlayerStatus, fivePlayers)) ==
        DecodeError::SizeMismatch);
  // Session packets with a payload they should not have.
  CHECK(errorOf(withHeader(protocol::PacketType::Connect, {0})) == DecodeError::SizeMismatch);
  CHECK(errorOf(withHeader(protocol::PacketType::Input, {})) == DecodeError::SizeMismatch);
}

TEST_CASE("Out-of-range field values are rejected", "[protocol][codec][malformed]") {
  CHECK(errorOf(withHeader(protocol::PacketType::Input, {0x20})) == DecodeError::InvalidValue);
  CHECK(errorOf(withHeader(protocol::PacketType::Reject, {0})) == DecodeError::InvalidValue);
  CHECK(errorOf(withHeader(protocol::PacketType::Reject, {3})) == DecodeError::InvalidValue);
  CHECK(errorOf(withHeader(protocol::PacketType::PlayerLeft, {4, 1})) == DecodeError::InvalidValue);
  CHECK(errorOf(withHeader(protocol::PacketType::PlayerLeft, {0, 9})) == DecodeError::InvalidValue);
  CHECK(errorOf(withHeader(protocol::PacketType::Accept, {0, 3, 32, 2, 88, 0})) ==
        DecodeError::InvalidValue); // tick rate 0
  CHECK(errorOf(withHeader(protocol::PacketType::EntityDestroy, {0, 1, 0, 0, 0, 1, 7})) ==
        DecodeError::InvalidValue);

  const auto entity = [](std::uint8_t kind, std::uint8_t playerId, float x) {
    auto datagram = protocol::encode(1, protocol::EntitySpawn{{ship(1, 0.0F)}});
    datagram[protocol::HEADER_SIZE + 6] = kind;
    datagram[protocol::HEADER_SIZE + 7] = playerId;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &x, sizeof(bits));
    for (std::size_t index = 0; index < 4; ++index) {
      datagram[protocol::HEADER_SIZE + 8 + index] =
          static_cast<std::uint8_t>(bits >> (24U - 8U * index));
    }
    return errorOf(datagram);
  };
  CHECK(entity(1, 0, 1.0F) == DecodeError::None);
  CHECK(entity(1, protocol::NO_PLAYER, 1.0F) == DecodeError::None);
  CHECK(entity(0, 0, 1.0F) == DecodeError::InvalidValue);
  CHECK(entity(5, 0, 1.0F) == DecodeError::InvalidValue);
  CHECK(entity(1, 4, 1.0F) == DecodeError::InvalidValue);
  CHECK(entity(1, 0, std::numeric_limits<float>::quiet_NaN()) == DecodeError::InvalidValue);
  CHECK(entity(1, 0, std::numeric_limits<float>::infinity()) == DecodeError::InvalidValue);
}

TEST_CASE("Encoding refuses payloads that do not fit in one datagram", "[protocol][codec]") {
  protocol::EntitySpawn spawn;
  spawn.entities.assign(protocol::MAX_SPAWNS_PER_PACKET, ship(1, 0.0F));
  CHECK(protocol::encode(1, spawn).size() <= protocol::MAX_DATAGRAM_SIZE);
  spawn.entities.push_back(ship(2, 0.0F));
  CHECK_THROWS_AS(protocol::encode(1, spawn), std::length_error);

  protocol::EntityUpdate update{1, {}};
  update.entities.assign(protocol::MAX_UPDATES_PER_PACKET + 1, ship(1, 0.0F));
  CHECK_THROWS_AS(protocol::encode(1, update), std::length_error);

  protocol::PlayerStatus status;
  status.players.assign(protocol::MAX_PLAYERS + 1, {});
  CHECK_THROWS_AS(protocol::encode(1, status), std::length_error);
}

TEST_CASE("Random and corrupted datagrams never crash the decoder", "[protocol][codec][fuzz]") {
  std::mt19937 random(20261020U);
  std::uniform_int_distribution<int> byte(0, 255);
  std::uniform_int_distribution<std::size_t> length(0, protocol::MAX_DATAGRAM_SIZE + 100);
  const auto valid = samplePayloads();
  std::uniform_int_distribution<std::size_t> pick(0, valid.size() - 1);
  std::size_t accepted = 0;

  for (int round = 0; round < 100000; ++round) {
    std::vector<std::uint8_t> datagram;
    if (round % 2 == 0) {
      datagram.resize(length(random));
      for (auto& value : datagram) {
        value = static_cast<std::uint8_t>(byte(random));
      }
    } else {
      // A valid datagram with a few bytes changed: gets past the header checks more often.
      datagram = protocol::encode(static_cast<std::uint32_t>(round), valid[pick(random)]);
      std::uniform_int_distribution<std::size_t> position(0, datagram.size() - 1);
      for (int flips = 0; flips < 3; ++flips) {
        datagram[position(random)] = static_cast<std::uint8_t>(byte(random));
      }
    }
    const auto result = protocol::decode(datagram);
    if (result.packet) {
      ++accepted;
      // Whatever is accepted is well formed: it re-encodes to the exact same bytes.
      CHECK(protocol::encode(result.packet->header.sequence, result.packet->payload) == datagram);
    }
  }
  CHECK(accepted > 0);
}
