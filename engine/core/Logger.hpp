#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string_view>

namespace engine::core {

enum class LogLevel : std::uint8_t { Debug, Info, Warning, Error };

// Safe to call from several threads at once (the server logs from its network thread and its
// game loop): the level is atomic, the time is converted with the reentrant localtime_r/s, and
// each line is written under a lock so lines never interleave.
class Logger {
public:
  Logger() = delete;

  static void setLevel(LogLevel level) { _level.store(level, std::memory_order_relaxed); }

  static void log(LogLevel level, std::string_view message) {
    if (level < _level.load(std::memory_order_relaxed)) {
      return;
    }

    const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    const std::scoped_lock lock(_outputMutex);
    std::clog << "[" << std::put_time(&local, "%H:%M:%S") << "] " << toString(level) << " "
              << message << '\n';
  }

  static void debug(std::string_view message) { log(LogLevel::Debug, message); }
  static void info(std::string_view message) { log(LogLevel::Info, message); }
  static void warning(std::string_view message) { log(LogLevel::Warning, message); }
  static void error(std::string_view message) { log(LogLevel::Error, message); }

  static std::string_view toString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warning: return "WARNING";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO";
  }

private:
  static std::atomic<LogLevel> _level;
  static std::mutex _outputMutex;
};

inline std::atomic<LogLevel> Logger::_level{LogLevel::Info};
inline std::mutex Logger::_outputMutex;

} // namespace engine::core
