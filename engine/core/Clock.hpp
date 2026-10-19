#pragma once

#include <chrono>
#include <stdexcept>

namespace engine::core {

class Clock {
public:
  using clock = std::chrono::steady_clock;

  Clock() : _epoch(clock::now()) {}

  void reset() { _epoch = clock::now(); }

  template<typename Duration = std::chrono::milliseconds>
  Duration elapsed() const {
    return std::chrono::duration_cast<Duration>(clock::now() - _epoch);
  }

  double seconds() const {
    return std::chrono::duration<double>(clock::now() - _epoch).count();
  }

private:
  clock::time_point _epoch;
};

class FixedTimestep {
public:
  explicit FixedTimestep(std::chrono::nanoseconds step = std::chrono::milliseconds(16))
      : _step(step), _accumulator(std::chrono::nanoseconds::zero()),
        _lastTick(std::chrono::steady_clock::now()) {
    if (_step <= std::chrono::nanoseconds::zero()) {
      throw std::invalid_argument("fixed timestep duration must be positive");
    }
  }

  void reset() {
    _accumulator = std::chrono::nanoseconds::zero();
    _lastTick = std::chrono::steady_clock::now();
  }

  bool update() {
    const auto now = std::chrono::steady_clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(now - _lastTick);
    _lastTick = now;
    _accumulator += delta;

    return consumeStep();
  }

  void advance(std::chrono::nanoseconds delta) {
    if (delta < std::chrono::nanoseconds::zero()) {
      throw std::invalid_argument("fixed timestep delta must be non-negative");
    }
    _accumulator += delta;
  }

  bool consumeStep() {
    if (!ready()) {
      return false;
    }
    _accumulator -= _step;
    return true;
  }

  bool ready() const { return _accumulator >= _step; }

  std::chrono::nanoseconds stepDuration() const { return _step; }

private:
  std::chrono::nanoseconds _step;
  std::chrono::nanoseconds _accumulator;
  std::chrono::steady_clock::time_point _lastTick;
};

} // namespace engine::core
