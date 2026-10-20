#include "engine/network/UdpSocket.hpp"

#include <asio.hpp>

#include <atomic>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "engine/core/Logger.hpp"

namespace engine::network {

namespace {

asio::ip::udp::endpoint toAsio(const Endpoint& endpoint) {
  return {asio::ip::address_v4(endpoint.address), endpoint.port};
}

Endpoint fromAsio(const asio::ip::udp::endpoint& endpoint) {
  return {endpoint.address().to_v4().to_uint(), endpoint.port()};
}

} // namespace

struct UdpSocket::Impl {
  explicit Impl(std::size_t maxSize) : maxDatagramSize(maxSize), buffer(maxSize + 1) {}

  void receiveNext() {
    socket.async_receive_from(asio::buffer(buffer), sender,
                              [this](const asio::error_code& error, std::size_t bytes) {
                                if (error == asio::error::operation_aborted || !socket.is_open()) {
                                  return;
                                }
                                // The buffer is one byte larger than the limit: a datagram that
                                // fills it is too big. Errors are counted and the loop goes on.
                                // Windows, for instance, reports an ICMP "port unreachable" from a
                                // vanished client as an error on the next receive: a dead client
                                // must never stop the server from listening.
                                if (error || bytes > maxDatagramSize) {
                                  ++dropped;
                                } else {
                                  deliver(bytes);
                                }
                                receiveNext();
                              });
  }

  void deliver(std::size_t bytes) {
    try {
      handler(fromAsio(sender), std::span<const std::uint8_t>(buffer.data(), bytes));
    } catch (const std::exception& exception) {
      ++dropped;
      core::Logger::error(std::string("UdpSocket: receive handler threw: ") + exception.what());
    } catch (...) {
      ++dropped;
      core::Logger::error("UdpSocket: receive handler threw an unknown exception");
    }
  }

  asio::io_context io;
  asio::ip::udp::socket socket{io};
  std::size_t maxDatagramSize;
  std::vector<std::uint8_t> buffer;
  asio::ip::udp::endpoint sender;
  ReceiveHandler handler;
  std::thread thread;
  std::atomic<std::uint64_t> dropped{0};
};

UdpSocket::UdpSocket(std::size_t maxDatagramSize) : _impl(std::make_unique<Impl>(maxDatagramSize)) {
  if (maxDatagramSize == 0) {
    throw std::invalid_argument("UdpSocket: maximum datagram size must be positive");
  }
}

UdpSocket::~UdpSocket() { stop(); }

void UdpSocket::open(std::uint16_t port) {
  asio::error_code error;
  _impl->socket.open(asio::ip::udp::v4(), error);
  if (!error) {
    _impl->socket.bind({asio::ip::udp::v4(), port}, error);
  }
  if (error) {
    asio::error_code ignored;
    _impl->socket.close(ignored);
    throw std::runtime_error("cannot bind UDP port " + std::to_string(port) + ": " +
                             error.message());
  }
}

void UdpSocket::start(ReceiveHandler handler) {
  if (!_impl->socket.is_open()) {
    throw std::logic_error("UdpSocket: open() the socket before start()");
  }
  if (_impl->thread.joinable()) {
    throw std::logic_error("UdpSocket: already started");
  }
  if (!handler) {
    throw std::invalid_argument("UdpSocket: the receive handler must be callable");
  }
  _impl->handler = std::move(handler);
  _impl->receiveNext();
  _impl->thread = std::thread([impl = _impl.get()] {
    // No exception may escape the network thread: log it and serve again.
    // run() only returns normally once stop() has closed the socket.
    while (!impl->io.stopped()) {
      try {
        impl->io.run();
      } catch (const std::exception& exception) {
        core::Logger::error(std::string("UdpSocket: network thread: ") + exception.what());
      } catch (...) {
        core::Logger::error("UdpSocket: network thread: unknown exception");
      }
    }
  });
}

void UdpSocket::send(const Endpoint& to, std::vector<std::uint8_t> datagram) {
  // asio::post is the outgoing queue: the lambda runs later on the network thread, the only
  // thread that touches the socket (an Asio socket must not be used by two threads at once).
  // A UDP send hands the datagram to the kernel and returns; an error (peer gone, buffer
  // full) only loses this datagram, which UDP allows anyway.
  asio::post(_impl->io, [impl = _impl.get(), to, data = std::move(datagram)] {
    asio::error_code ignored;
    impl->socket.send_to(asio::buffer(data), toAsio(to), 0, ignored);
  });
}

void UdpSocket::stop() {
  if (!_impl) {
    return;
  }
  if (_impl->thread.joinable()) {
    // Close from the network thread, after the datagrams already queued by send(): a
    // DISCONNECT sent just before stop() still leaves. Closing cancels the pending receive,
    // so run() runs out of work and the thread ends.
    asio::post(_impl->io, [impl = _impl.get()] {
      asio::error_code ignored;
      impl->socket.close(ignored);
    });
    _impl->thread.join();
  }
  asio::error_code ignored;
  _impl->socket.close(ignored);
}

std::uint16_t UdpSocket::localPort() const {
  asio::error_code error;
  const auto endpoint = _impl->socket.local_endpoint(error);
  return error ? 0 : endpoint.port();
}

std::uint64_t UdpSocket::droppedDatagrams() const { return _impl->dropped.load(); }

std::optional<Endpoint> UdpSocket::resolve(const std::string& host, std::uint16_t port) {
  asio::io_context io;
  asio::ip::udp::resolver resolver(io);
  asio::error_code error;
  const auto results = resolver.resolve(asio::ip::udp::v4(), host, std::to_string(port), error);
  if (error || results.empty()) {
    return std::nullopt;
  }
  return fromAsio(results.begin()->endpoint());
}

} // namespace engine::network
