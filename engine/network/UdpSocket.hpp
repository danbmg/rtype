#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "engine/network/Endpoint.hpp"

namespace engine::network {

// A UDP socket served by its own thread, the "network thread".
// - Received datagrams are handed to a callback, on the network thread.
// - send() may be called from any thread: it only queues the datagram for the network thread,
//   so the caller (e.g. the game loop) never blocks on the network.
// Asio stays inside UdpSocket.cpp (pimpl): including this header does not pull Asio in.
class UdpSocket {
public:
  using ReceiveHandler =
      std::function<void(const Endpoint& from, std::span<const std::uint8_t> datagram)>;

  // Datagrams bigger than maxDatagramSize are dropped before reaching the callback.
  explicit UdpSocket(std::size_t maxDatagramSize);
  ~UdpSocket();
  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;
  UdpSocket(UdpSocket&&) = delete;
  UdpSocket& operator=(UdpSocket&&) = delete;

  // Binds on every IPv4 interface. Port 0 lets the system pick a free port (clients, tests).
  // Throws std::runtime_error if the port cannot be bound (e.g. already in use).
  void open(std::uint16_t port = 0);

  // Starts the network thread. The handler must not block: it runs between two receives.
  void start(ReceiveHandler handler);

  // Thread-safe, never blocks. Silently does nothing once the socket is stopped.
  void send(const Endpoint& to, std::vector<std::uint8_t> datagram);

  // Stops and joins the network thread, then closes the socket. Idempotent.
  // Must not be called from the receive handler.
  void stop();

  std::uint16_t localPort() const;

  // Datagrams dropped by the socket itself: too large, receive errors, or a throwing handler.
  std::uint64_t droppedDatagrams() const;

  // Resolves a host name or a dotted IPv4 address. Blocking; std::nullopt on failure.
  static std::optional<Endpoint> resolve(const std::string& host, std::uint16_t port);

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

} // namespace engine::network
