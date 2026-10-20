#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine/network/ThreadSafeQueue.hpp"

using engine::network::ThreadSafeQueue;

TEST_CASE("ThreadSafeQueue is FIFO and refuses items when full", "[network][queue]") {
  ThreadSafeQueue<int> queue(3);
  CHECK(queue.tryPush(1));
  CHECK(queue.tryPush(2));
  CHECK(queue.tryPush(3));
  CHECK_FALSE(queue.tryPush(4));
  CHECK(queue.size() == 3);

  CHECK(queue.tryPop() == 1);
  CHECK(queue.drain() == std::vector<int>{2, 3});
  CHECK_FALSE(queue.tryPop().has_value());
  CHECK(queue.drain().empty());

  CHECK_THROWS_AS(ThreadSafeQueue<int>(0), std::invalid_argument);
}

TEST_CASE("ThreadSafeQueue hands every item from several producers to one consumer",
          "[network][queue]") {
  constexpr int PRODUCERS = 4;
  constexpr int ITEMS = 20000;
  ThreadSafeQueue<int> queue(256);
  std::atomic<long long> refused{0};

  std::vector<std::thread> producers;
  for (int producer = 0; producer < PRODUCERS; ++producer) {
    producers.emplace_back([&queue, &refused] {
      for (int item = 1; item <= ITEMS; ++item) {
        while (!queue.tryPush(item)) {
          ++refused; // full: the consumer is behind, try again
          std::this_thread::yield();
        }
      }
    });
  }

  long long sum = 0;
  int received = 0;
  while (received < PRODUCERS * ITEMS) {
    for (const int item : queue.drain()) {
      sum += item;
      ++received;
    }
  }
  for (auto& producer : producers) {
    producer.join();
  }

  CHECK(received == PRODUCERS * ITEMS);
  CHECK(sum == static_cast<long long>(PRODUCERS) * ITEMS * (ITEMS + 1) / 2);
  CHECK(queue.size() == 0);
}
