#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/network/UdpSocket.hpp"

using engine::network::Endpoint;
using engine::network::UdpSocket;
using namespace std::chrono_literals;

namespace {

constexpr std::size_t MAX_SIZE = 64;
constexpr std::uint32_t LOCALHOST = 0x7F000001;

bool waitUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = 2s) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!condition()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    std::this_thread::sleep_for(1ms);
  }
  return true;
}

// Collects what a socket receives, from its network thread.
struct Inbox {
  std::mutex mutex;
  std::vector<std::vector<std::uint8_t>> datagrams;
  Endpoint lastSender;

  void add(const Endpoint& from, std::span<const std::uint8_t> datagram) {
    const std::scoped_lock lock(mutex);
    datagrams.emplace_back(datagram.begin(), datagram.end());
    lastSender = from;
  }

  std::size_t count() {
    const std::scoped_lock lock(mutex);
    return datagrams.size();
  }
};

} // namespace

TEST_CASE("UdpSocket delivers datagrams and the sender's address", "[network][udp]") {
  Inbox inbox;
  UdpSocket receiver(MAX_SIZE);
  receiver.open(0);
  receiver.start([&inbox](const Endpoint& from, std::span<const std::uint8_t> datagram) {
    inbox.add(from, datagram);
  });
  UdpSocket sender(MAX_SIZE);
  sender.open(0);
  sender.start([](const Endpoint&, std::span<const std::uint8_t>) {});

  sender.send({LOCALHOST, receiver.localPort()}, {1, 2, 3});
  REQUIRE(waitUntil([&inbox] { return inbox.count() == 1; }));
  const std::scoped_lock lock(inbox.mutex);
  CHECK(inbox.datagrams[0] == std::vector<std::uint8_t>{1, 2, 3});
  CHECK(inbox.lastSender == Endpoint{LOCALHOST, sender.localPort()});
}

TEST_CASE("UdpSocket drops oversized datagrams and keeps listening", "[network][udp]") {
  Inbox inbox;
  UdpSocket receiver(MAX_SIZE);
  receiver.open(0);
  receiver.start([&inbox](const Endpoint& from, std::span<const std::uint8_t> datagram) {
    inbox.add(from, datagram);
  });
  UdpSocket sender(1024);
  sender.open(0);
  sender.start([](const Endpoint&, std::span<const std::uint8_t>) {});
  const Endpoint target{LOCALHOST, receiver.localPort()};

  sender.send(target, std::vector<std::uint8_t>(MAX_SIZE + 1, 0xAA));
  sender.send(target, std::vector<std::uint8_t>(MAX_SIZE, 0xBB));
  REQUIRE(waitUntil([&inbox] { return inbox.count() == 1; }));
  CHECK(waitUntil([&receiver] { return receiver.droppedDatagrams() == 1; }));
  const std::scoped_lock lock(inbox.mutex);
  CHECK(inbox.datagrams[0].size() == MAX_SIZE);
}

TEST_CASE("A throwing receive handler does not stop the network thread", "[network][udp]") {
  std::atomic<int> calls{0};
  UdpSocket receiver(MAX_SIZE);
  receiver.open(0);
  receiver.start([&calls](const Endpoint&, std::span<const std::uint8_t>) {
    if (++calls == 1) {
      throw std::runtime_error("bad datagram");
    }
  });
  UdpSocket sender(MAX_SIZE);
  sender.open(0);
  sender.start([](const Endpoint&, std::span<const std::uint8_t>) {});

  sender.send({LOCALHOST, receiver.localPort()}, {1});
  sender.send({LOCALHOST, receiver.localPort()}, {2});
  CHECK(waitUntil([&calls] { return calls == 2; }));
  CHECK(receiver.droppedDatagrams() == 1);
}

TEST_CASE("stop() still sends the datagrams queued before it", "[network][udp]") {
  Inbox inbox;
  UdpSocket receiver(MAX_SIZE);
  receiver.open(0);
  receiver.start([&inbox](const Endpoint& from, std::span<const std::uint8_t> datagram) {
    inbox.add(from, datagram);
  });
  {
    UdpSocket sender(MAX_SIZE);
    sender.open(0);
    sender.start([](const Endpoint&, std::span<const std::uint8_t>) {});
    for (std::uint8_t index = 0; index < 10; ++index) {
      sender.send({LOCALHOST, receiver.localPort()}, {index});
    }
    sender.stop();
    sender.stop(); // idempotent
  }
  CHECK(waitUntil([&inbox] { return inbox.count() == 10; }));
}

TEST_CASE("UdpSocket reports a port that is already taken", "[network][udp]") {
  UdpSocket first(MAX_SIZE);
  first.open(0);
  UdpSocket second(MAX_SIZE);
  CHECK_THROWS_AS(second.open(first.localPort()), std::runtime_error);
  CHECK_THROWS_AS(second.start([](const Endpoint&, std::span<const std::uint8_t>) {}),
                  std::logic_error);
}

TEST_CASE("UdpSocket resolves dotted addresses and formats endpoints", "[network][udp]") {
  const auto endpoint = UdpSocket::resolve("127.0.0.1", 4242);
  REQUIRE(endpoint.has_value());
  CHECK(*endpoint == Endpoint{LOCALHOST, 4242});
  CHECK(endpoint->toString() == "127.0.0.1:4242");
}
