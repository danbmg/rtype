#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/network/BinaryReader.hpp"
#include "engine/network/BinaryWriter.hpp"

using engine::network::BinaryReader;
using engine::network::BinaryWriter;

TEST_CASE("BinaryWriter writes big-endian whatever the host byte order", "[network][serializer]") {
  BinaryWriter writer;
  writer.writeU8(0xAB);
  writer.writeU16(0x1234);
  writer.writeU32(0xA1B2C3D4);
  writer.writeF32(1.0F); // IEEE 754: 0x3F800000

  const std::vector<std::uint8_t> expected{0xAB, 0x12, 0x34, 0xA1, 0xB2, 0xC3,
                                           0xD4, 0x3F, 0x80, 0x00, 0x00};
  CHECK(writer.data() == expected);
}

TEST_CASE("BinaryReader reads back what BinaryWriter wrote", "[network][serializer]") {
  BinaryWriter writer;
  writer.writeU8(7);
  writer.writeU16(65535);
  writer.writeU32(4000000000U);
  writer.writeF32(-12.5F);
  writer.writeF32(std::numeric_limits<float>::denorm_min());
  const auto bytes = writer.release();

  BinaryReader reader(bytes);
  CHECK(reader.readU8() == 7);
  CHECK(reader.readU16() == 65535);
  CHECK(reader.readU32() == 4000000000U);
  CHECK(reader.readF32() == -12.5F);
  CHECK(reader.readF32() == std::numeric_limits<float>::denorm_min());
  CHECK(reader.ok());
  CHECK(reader.remaining() == 0);
}

TEST_CASE("BinaryReader never reads past the end and stays failed", "[network][serializer]") {
  const std::vector<std::uint8_t> bytes{0x01, 0x02, 0x03};
  BinaryReader reader(bytes);

  CHECK(reader.readU32() == 0);
  CHECK_FALSE(reader.ok());
  CHECK(reader.remaining() == 3);
  // The failure is sticky: the bytes that do remain are not read any more.
  CHECK(reader.readU8() == 0);
  CHECK_FALSE(reader.ok());

  BinaryReader empty(std::span<const std::uint8_t>{});
  CHECK(empty.readU8() == 0);
  CHECK_FALSE(empty.ok());
}

TEST_CASE("BinaryWriter patches a field in place and rejects patches outside the data",
          "[network][serializer]") {
  BinaryWriter writer;
  writer.writeU16(0);
  writer.writeU8(9);
  writer.patchU16(0, 0xBEEF);
  CHECK(writer.data() == std::vector<std::uint8_t>{0xBE, 0xEF, 0x09});

  CHECK_THROWS_AS(writer.patchU16(2, 1), std::out_of_range);
  CHECK_THROWS_AS(writer.patchU16(100, 1), std::out_of_range);
}
