#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace engine::network {

// An IPv4 address and a UDP port, stored as plain integers so that code outside engine/network
// never sees an Asio type. Comparable and hashable: the server uses it as a session key.
struct Endpoint {
  std::uint32_t address = 0; // host byte order: 127.0.0.1 is 0x7F000001
  std::uint16_t port = 0;

  bool operator==(const Endpoint&) const = default;

  std::string toString() const {
    return std::to_string((address >> 24U) & 0xFFU) + "." +
           std::to_string((address >> 16U) & 0xFFU) + "." +
           std::to_string((address >> 8U) & 0xFFU) + "." + std::to_string(address & 0xFFU) + ":" +
           std::to_string(port);
  }
};

} // namespace engine::network

template<>
struct std::hash<engine::network::Endpoint> {
  std::size_t operator()(const engine::network::Endpoint& endpoint) const noexcept {
    return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(endpoint.address) << 16U) |
                                      endpoint.port);
  }
};
