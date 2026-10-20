#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "common/protocol/Packets.hpp"

namespace rtype::protocol {

enum class DecodeError : std::uint8_t {
  None,
  TooShort,     // smaller than a header
  TooLong,      // larger than MAX_DATAGRAM_SIZE
  BadMagic,     // not an R-Type datagram
  BadVersion,   // another protocol version
  UnknownType,  // unknown packet type
  SizeMismatch, // declared size, list count, or payload length disagree
  InvalidValue, // a field is out of range (enum, flag, player id, non-finite float)
};

struct DecodeResult {
  std::optional<Packet> packet; // set only when error == DecodeError::None
  DecodeError error = DecodeError::None;
};

PacketType packetTypeOf(const Payload& payload);

// Builds one datagram: header + payload, field by field, big-endian.
// Throws std::length_error if a list is longer than its MAX_*_PER_PACKET constant (or a
// PlayerStatus holds more than MAX_PLAYERS entries): that is a bug in the sender, which must
// split long lists over several packets.
std::vector<std::uint8_t> encode(std::uint32_t sequence, const Payload& payload);

// Treats the datagram as hostile: every check (size, magic, version, type, declared size, list
// counts, field ranges) happens before the value is used, nothing is allocated from an unchecked
// count, and nothing is read outside the datagram. Never throws on bad input: it reports why.
DecodeResult decode(std::span<const std::uint8_t> datagram);

std::string_view toString(DecodeError error);

} // namespace rtype::protocol
