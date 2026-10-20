#pragma once

#include <cstddef>
#include <deque>
#include <iterator>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::network {

// Bounded FIFO shared by two threads: typically the network thread pushes received packets and
// the game loop pops them. Nothing ever waits: tryPop/drain return at once when the queue is
// empty, and tryPush refuses the item when the queue is full, so a packet flood cannot make
// memory grow without limit.
template<typename T>
class ThreadSafeQueue {
public:
  explicit ThreadSafeQueue(std::size_t capacity) : _capacity(capacity) {
    if (capacity == 0) {
      throw std::invalid_argument("ThreadSafeQueue capacity must be positive");
    }
  }

  // Returns false, and drops the item, when the queue is full.
  bool tryPush(T value) {
    const std::scoped_lock lock(_mutex);
    if (_items.size() >= _capacity) {
      return false;
    }
    _items.push_back(std::move(value));
    return true;
  }

  std::optional<T> tryPop() {
    const std::scoped_lock lock(_mutex);
    if (_items.empty()) {
      return std::nullopt;
    }
    T value = std::move(_items.front());
    _items.pop_front();
    return value;
  }

  // Takes every queued item at once: one lock per game tick instead of one per packet.
  std::vector<T> drain() {
    const std::scoped_lock lock(_mutex);
    std::vector<T> items(std::make_move_iterator(_items.begin()),
                         std::make_move_iterator(_items.end()));
    _items.clear();
    return items;
  }

  std::size_t size() const {
    const std::scoped_lock lock(_mutex);
    return _items.size();
  }

  std::size_t capacity() const { return _capacity; }

private:
  mutable std::mutex _mutex;
  std::deque<T> _items;
  std::size_t _capacity;
};

} // namespace engine::network
