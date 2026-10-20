#include <chrono>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "server/SessionManager.hpp"

using engine::network::Endpoint;
using server::SessionManager;
using namespace std::chrono_literals;

namespace {

Endpoint client(std::uint16_t port) { return {0x7F000001, port}; }

} // namespace

TEST_CASE("Sessions get the lowest free player id, up to the player limit", "[server][sessions]") {
  SessionManager sessions(4);
  const auto now = SessionManager::Clock::now();

  for (std::uint16_t index = 0; index < 4; ++index) {
    auto* session = sessions.join(client(static_cast<std::uint16_t>(5000 + index)), now);
    REQUIRE(session != nullptr);
    CHECK(session->playerId == index);
  }
  CHECK(sessions.full());
  CHECK(sessions.join(client(6000), now) == nullptr);

  REQUIRE(sessions.remove(client(5001)).has_value());
  auto* reused = sessions.join(client(6000), now);
  REQUIRE(reused != nullptr);
  CHECK(reused->playerId == 1);
  CHECK_FALSE(sessions.remove(client(5001)).has_value());
}

TEST_CASE("A repeated CONNECT from the same endpoint keeps its session", "[server][sessions]") {
  SessionManager sessions(2);
  const auto now = SessionManager::Clock::now();
  auto* first = sessions.join(client(5000), now);
  auto* again = sessions.join(client(5000), now);
  CHECK(first == again);
  CHECK(sessions.size() == 1);
}

TEST_CASE("Silent sessions time out, active ones stay", "[server][sessions]") {
  SessionManager sessions(4);
  const auto start = SessionManager::Clock::now();
  sessions.join(client(5000), start);
  sessions.join(client(5001), start)->lastSeen = start + 2s;

  CHECK(sessions.removeTimedOut(start + 2s, 3s).empty());
  const auto removed = sessions.removeTimedOut(start + 3500ms, 3s);
  REQUIRE(removed.size() == 1);
  CHECK(removed[0].endpoint == client(5000));
  CHECK(sessions.size() == 1);
  CHECK(sessions.find(client(5001)) != nullptr);
}

TEST_CASE("The player limit must fit the protocol", "[server][sessions]") {
  CHECK_THROWS_AS(SessionManager(0), std::invalid_argument);
  CHECK_THROWS_AS(SessionManager(5), std::invalid_argument);
}
