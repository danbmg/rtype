#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace engine::network {

// Reads big-endian fields from a received buffer, the counterpart of BinaryWriter.
// It never reads past the end: a read that does not fit returns 0 and puts the reader in a
// failed state that stays set, so a parser can read a whole record and check ok() once.
class BinaryReader {
public:
  explicit BinaryReader(std::span<const std::uint8_t> data) : _data(data) {}

  std::uint8_t readU8() {
    if (!canRead(1)) {
      return 0;
    }
    return _data[_offset++];
  }

  std::uint16_t readU16() {
    if (!canRead(2)) {
      return 0;
    }
    const auto value = static_cast<std::uint16_t>((_data[_offset] << 8U) | _data[_offset + 1]);
    _offset += 2;
    return value;
  }

  std::uint32_t readU32() {
    if (!canRead(4)) {
      return 0;
    }
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index) {
      value = (value << 8U) | _data[_offset + index];
    }
    _offset += 4;
    return value;
  }

  float readF32() { return std::bit_cast<float>(readU32()); }

  bool ok() const { return !_failed; }
  std::size_t remaining() const { return _data.size() - _offset; }

private:
  bool canRead(std::size_t count) {
    if (_failed || count > remaining()) {
      _failed = true;
      return false;
    }
    return true;
  }

  std::span<const std::uint8_t> _data;
  std::size_t _offset = 0;
  bool _failed = false;
};

} // namespace engine::network
