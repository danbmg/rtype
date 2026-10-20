#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::network {

static_assert(std::numeric_limits<float>::is_iec559,
              "the wire format stores floats as IEEE 754 binary32");

// Serializes values one field at a time in network byte order (big-endian), whatever the byte
// order of the machine. Whole structs are never copied: their padding and layout depend on the
// compiler, so two builds of the game could disagree on the bytes.
class BinaryWriter {
public:
  BinaryWriter() = default;
  explicit BinaryWriter(std::size_t reserve) { _buffer.reserve(reserve); }

  void writeU8(std::uint8_t value) { _buffer.push_back(value); }

  void writeU16(std::uint16_t value) {
    _buffer.push_back(static_cast<std::uint8_t>(value >> 8U));
    _buffer.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  }

  void writeU32(std::uint32_t value) {
    writeU16(static_cast<std::uint16_t>(value >> 16U));
    writeU16(static_cast<std::uint16_t>(value & 0xFFFFU));
  }

  void writeF32(float value) { writeU32(std::bit_cast<std::uint32_t>(value)); }

  // Overwrites a 16-bit field written earlier, e.g. a size only known once the body is written.
  void patchU16(std::size_t offset, std::uint16_t value) {
    if (offset > _buffer.size() || _buffer.size() - offset < 2) {
      throw std::out_of_range("BinaryWriter::patchU16 outside the written data");
    }
    _buffer[offset] = static_cast<std::uint8_t>(value >> 8U);
    _buffer[offset + 1] = static_cast<std::uint8_t>(value & 0xFFU);
  }

  std::size_t size() const { return _buffer.size(); }
  const std::vector<std::uint8_t>& data() const { return _buffer; }
  std::vector<std::uint8_t> release() { return std::exchange(_buffer, {}); }

private:
  std::vector<std::uint8_t> _buffer;
};

} // namespace engine::network
