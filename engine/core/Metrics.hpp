#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace engine::core {

struct MetricsSnapshot {
  double framesPerSecond = 0.0;
  double frameTimeMilliseconds = 0.0;
  double cpuUsagePercent = 0.0;
  std::uint64_t memoryBytes = 0;
  std::size_t entityCount = 0;
  std::uint64_t frameCount = 0;
};

class Metrics {
public:
  void recordFrame(double frameTimeMilliseconds, std::size_t entityCount) {
    if (!std::isfinite(frameTimeMilliseconds) || frameTimeMilliseconds < 0.0) {
      throw std::invalid_argument("frame time must be finite and non-negative");
    }
    ++_snapshot.frameCount;
    _snapshot.frameTimeMilliseconds = frameTimeMilliseconds;
    _snapshot.framesPerSecond =
        frameTimeMilliseconds > 0.0 ? 1000.0 / frameTimeMilliseconds : 0.0;
    _snapshot.entityCount = entityCount;
  }

  void setCpuUsagePercent(double cpuUsagePercent) {
    if (!std::isfinite(cpuUsagePercent) || cpuUsagePercent < 0.0 || cpuUsagePercent > 100.0) {
      throw std::invalid_argument("CPU usage must be finite and between 0 and 100");
    }
    _snapshot.cpuUsagePercent = cpuUsagePercent;
  }

  void setMemoryBytes(std::uint64_t memoryBytes) { _snapshot.memoryBytes = memoryBytes; }

  const MetricsSnapshot& snapshot() const { return _snapshot; }

private:
  MetricsSnapshot _snapshot;
};

} // namespace engine::core
