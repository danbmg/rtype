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
      protocol::SkipLevel{7},
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
      protocol::Connect{protocol::GameMode::Endless, protocol::Difficulty::Hard},
      protocol::Connect{protocol::GameMode::Story, protocol::Difficulty::Tutorial},
      protocol::GameStatus{protocol::GameMode::Story, protocol::Difficulty::Easy,
                           protocol::GamePhase::BossFight, 5, 10, 3, 3, 120, 400, 0},
      protocol::LevelInfo{5, 2, "Quarterfinal", "The Gatekeeper blocks the way.\nBreak through!"},
      protocol::LevelInfo{1, 0, "", ""},
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
  const std::vector<std::uint8_t> expected{0x52, 0x54, protocol::VERSION, 0x02, 0x00, 0x00,
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

  for (const int type : {0x00, 0x06, 0x0B, 0x17, 0x7F, 0xFF}) {
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
  CHECK(entity(protocol::LAST_ENTITY_KIND, protocol::NO_PLAYER, 1.0F) == DecodeError::None);
  CHECK(entity(protocol::LAST_ENTITY_KIND + 1, 0, 1.0F) == DecodeError::InvalidValue);
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

TEST_CASE("Boss entities survive the network codec", "[protocol][codec][boss]") {
  const auto result = protocol::decode(protocol::encode(
      1, protocol::EntitySpawn{{{42, protocol::EntityKind::Boss, protocol::NO_PLAYER,
                                   688.0F, 264.0F}}}));
  REQUIRE(result.packet);
  const auto& spawn = std::get<protocol::EntitySpawn>(result.packet->payload);
  REQUIRE(spawn.entities.size() == 1);
  CHECK(spawn.entities.front().kind == protocol::EntityKind::Boss);
}

TEST_CASE("Equipment status and every pickup type survive the codec", "[protocol][codec][equipment]") {
  const auto decoded = protocol::decode(protocol::encode(
      1, protocol::PlayerStatus{{{0, 4, 3, 1000, 3, 8000}}}));
  REQUIRE(decoded.packet);
  const auto& state = std::get<protocol::PlayerStatus>(decoded.packet->payload).players.front();
  CHECK(state.weapon == 3);
  CHECK(state.rapidFireMilliseconds == 8000);
  CHECK(state.lives == 4);
  auto malformed = protocol::encode(1, protocol::PlayerStatus{{{0, 3, 3, 0, 4, 0}}});
  CHECK(errorOf(malformed) == DecodeError::InvalidValue);
  for (std::uint8_t kind = 6; kind <= 13; ++kind) {
    const auto result = protocol::decode(protocol::encode(
        1, protocol::EntitySpawn{{{42, static_cast<protocol::EntityKind>(kind),
                                   protocol::NO_PLAYER, 200, 100}}}));
    REQUIRE(result.packet);
    CHECK(static_cast<std::uint8_t>(std::get<protocol::EntitySpawn>(result.packet->payload)
                                       .entities.front().kind) == kind);
  }
}

TEST_CASE("CONNECT carries the requested game mode and difficulty", "[protocol][codec][v3]") {
  const auto result = protocol::decode(
      protocol::encode(1, protocol::Connect{protocol::GameMode::Endless,
                                            protocol::Difficulty::Easy}));
  REQUIRE(result.packet);
  const auto& connect = std::get<protocol::Connect>(result.packet->payload);
  CHECK(connect.mode == protocol::GameMode::Endless);
  CHECK(connect.difficulty == protocol::Difficulty::Easy);
  CHECK(errorOf(withHeader(protocol::PacketType::Connect, {})) == DecodeError::SizeMismatch);
  CHECK(errorOf(withHeader(protocol::PacketType::Connect, {2, 1})) == DecodeError::InvalidValue);
  CHECK(errorOf(withHeader(protocol::PacketType::Connect, {0, 4})) == DecodeError::InvalidValue);
}

TEST_CASE("Tutorial is a valid difficulty in CONNECT and GAME_STATUS", "[protocol][codec][v4]") {
  const auto connect = protocol::decode(withHeader(protocol::PacketType::Connect, {0, 3}));
  REQUIRE(connect.packet);
  CHECK(std::get<protocol::Connect>(connect.packet->payload).difficulty ==
        protocol::Difficulty::Tutorial);
  const auto status = protocol::decode(
      withHeader(protocol::PacketType::GameStatus, {0, 3, 2, 0, 3, 0, 10, 1, 4, 0, 0, 0, 0, 0}));
  REQUIRE(status.packet);
  CHECK(std::get<protocol::GameStatus>(status.packet->payload).difficulty ==
        protocol::Difficulty::Tutorial);
}

TEST_CASE("SKIP_LEVEL carries a level from 1 and nothing else",
          "[protocol][codec][v4][malformed]") {
  const auto result = protocol::decode(protocol::encode(3, protocol::SkipLevel{513}));
  REQUIRE(result.packet);
  CHECK(result.packet->header.type == protocol::PacketType::SkipLevel);
  CHECK(std::get<protocol::SkipLevel>(result.packet->payload).level == 513);
  CHECK(errorOf(withHeader(protocol::PacketType::SkipLevel, {})) == DecodeError::SizeMismatch);
  CHECK(errorOf(withHeader(protocol::PacketType::SkipLevel, {0})) == DecodeError::SizeMismatch);
  CHECK(errorOf(withHeader(protocol::PacketType::SkipLevel, {0, 1, 0})) ==
        DecodeError::SizeMismatch);
  CHECK(errorOf(withHeader(protocol::PacketType::SkipLevel, {0, 0})) ==
        DecodeError::InvalidValue); // levels start at 1
  CHECK(errorOf(withHeader(protocol::PacketType::SkipLevel, {0xFF, 0xFF})) == DecodeError::None);
}

TEST_CASE("GAME_STATUS rejects inconsistent values", "[protocol][codec][v3][malformed]") {
  const auto status = [](std::vector<std::uint8_t> bytes) {
    return errorOf(withHeader(protocol::PacketType::GameStatus, bytes));
  };
  // mode, difficulty, phase, level(2), levelCount(2), wave, waves, boss hp(2), boss max(2),
  // countdown
  CHECK(status({0, 1, 2, 0, 3, 0, 10, 1, 4, 0, 0, 0, 0, 0}) == DecodeError::None);
  CHECK(status({0, 1, 2, 0, 3, 0, 10, 1, 4, 0, 0, 0, 0}) == DecodeError::SizeMismatch);
  CHECK(status({2, 1, 2, 0, 3, 0, 10, 1, 4, 0, 0, 0, 0, 0}) == DecodeError::InvalidValue);
  CHECK(status({0, 4, 2, 0, 3, 0, 10, 1, 4, 0, 0, 0, 0, 0}) == DecodeError::InvalidValue);
  CHECK(status({0, 1, 7, 0, 3, 0, 10, 1, 4, 0, 0, 0, 0, 0}) == DecodeError::InvalidValue);
  CHECK(status({0, 1, 2, 0, 11, 0, 10, 1, 4, 0, 0, 0, 0, 0}) == DecodeError::InvalidValue);
  CHECK(status({0, 1, 2, 0, 3, 0, 10, 5, 4, 0, 0, 0, 0, 0}) == DecodeError::InvalidValue);
  CHECK(status({0, 1, 3, 0, 5, 0, 10, 1, 1, 0, 9, 0, 8, 0}) == DecodeError::InvalidValue);
  // Endless mode: no level count, any level number.
  CHECK(status({1, 1, 2, 0, 99, 0, 0, 1, 5, 0, 0, 0, 0, 0}) == DecodeError::None);
}

TEST_CASE("LEVEL_INFO texts are bounded and printable", "[protocol][codec][v3][malformed]") {
  const auto info = [](std::vector<std::uint8_t> bytes) {
    return errorOf(withHeader(protocol::PacketType::LevelInfo, bytes));
  };
  // level(2), theme, title length, title, story length(2), story
  CHECK(info({0, 1, 0, 2, 'O', 'K', 0, 1, '!'}) == DecodeError::None);
  CHECK(info({0, 1, 0, 6, 'O', 'K', 0, 1, '!'}) == DecodeError::SizeMismatch);
  CHECK(info({0, 1, 0, 3, 'O', 'K', 0, 1, '!'}) == DecodeError::InvalidValue); // reads 'O' 'K' 0x00
  CHECK(info({0, 1, 0, 2, 'O', 'K', 0, 2, '!'}) == DecodeError::SizeMismatch);
  CHECK(info({0, 1, 0, 2, 'O', 'K', 0xFF, 0xFF, '!'}) == DecodeError::SizeMismatch);
  CHECK(info({0, 1, 0, 200, 'O', 'K', 0, 1, '!'}) == DecodeError::SizeMismatch);
  CHECK(info({0, 1, 0, 2, 'O', 0x07, 0, 1, '!'}) == DecodeError::InvalidValue);
  CHECK(info({0, 1, 0, 2, 'O', 'K', 0, 1, 0xC3}) == DecodeError::InvalidValue);
  CHECK(info({0, 1, protocol::THEME_COUNT, 0, 0, 0}) == DecodeError::InvalidValue);
  CHECK(info({0, 1}) == DecodeError::SizeMismatch);

  CHECK_THROWS_AS(
      protocol::encode(
          1, protocol::LevelInfo{1, 0, std::string(protocol::MAX_TITLE_LENGTH + 1, 'a'), ""}),
      std::length_error);
  CHECK_THROWS_AS(
      protocol::encode(
          1, protocol::LevelInfo{1, 0, "", std::string(protocol::MAX_STORY_LENGTH + 1, 'a')}),
      std::length_error);
  CHECK_THROWS_AS(protocol::encode(1, protocol::LevelInfo{1, 0, "caf\xC3\xA9", ""}),
                  std::invalid_argument);
  const auto longest = protocol::encode(
      1, protocol::LevelInfo{1, 4, std::string(protocol::MAX_TITLE_LENGTH, 'T'),
                             std::string(protocol::MAX_STORY_LENGTH, 'S')});
  CHECK(longest.size() <= protocol::MAX_DATAGRAM_SIZE);
  CHECK(errorOf(longest) == DecodeError::None);
}

TEST_CASE("Player flags only use the documented bits", "[protocol][codec][v3][malformed]") {
  const auto ok = protocol::decode(protocol::encode(
      1, protocol::PlayerStatus{{{0, 3, 3, 10, 0, 0, protocol::player::SHIELD}}}));
  REQUIRE(ok.packet);
  CHECK(std::get<protocol::PlayerStatus>(ok.packet->payload).players.front().flags ==
        protocol::player::SHIELD);
  CHECK(errorOf(protocol::encode(1, protocol::PlayerStatus{{{0, 3, 3, 10, 0, 0, 0x04}}})) ==
        DecodeError::InvalidValue);
}

TEST_CASE("The examples of the RFC are exact", "[protocol][codec][v4][rfc]") {
  using Bytes = std::vector<std::uint8_t>;
  CHECK(protocol::encode(0, protocol::Connect{}) ==
        Bytes{0x52, 0x54, 0x04, 0x01, 0, 0, 0, 0, 0x00, 0x02, 0x00, 0x01});
  CHECK(protocol::encode(
            0x2C, protocol::PlayerStatus{{{0, 3, 2, 1500, 0, 0, protocol::player::SHIELD}}}) ==
        Bytes{0x52, 0x54, 0x04, 0x13, 0,    0,    0,    0x2C, 0x00, 0x0C, 0x01,
              0x00, 0x03, 0x02, 0x00, 0x00, 0x05, 0xDC, 0x00, 0x00, 0x00, 0x01});
  CHECK(protocol::encode(
            0x2E, protocol::GameStatus{protocol::GameMode::Story, protocol::Difficulty::Normal,
                                       protocol::GamePhase::BossFight, 5, 10, 3, 3, 120, 150, 0}) ==
        Bytes{0x52, 0x54, 0x04, 0x15, 0,    0,    0,    0x2E, 0x00, 0x0E, 0x00, 0x01,
              0x03, 0x00, 0x05, 0x00, 0x0A, 0x03, 0x03, 0x00, 0x78, 0x00, 0x96, 0x00});
  CHECK(protocol::encode(0x2F, protocol::LevelInfo{1, 0, "Go!", "Hi"}) ==
        Bytes{0x52, 0x54, 0x04, 0x16, 0,    0,    0,    0x2F, 0x00, 0x0B, 0x00,
              0x01, 0x00, 0x03, 0x47, 0x6F, 0x21, 0x00, 0x02, 0x48, 0x69});
  CHECK(protocol::encode(0x30, protocol::SkipLevel{3}) ==
        Bytes{0x52, 0x54, 0x04, 0x05, 0, 0, 0, 0x30, 0x00, 0x02, 0x00, 0x03});
}
