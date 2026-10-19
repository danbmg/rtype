#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace engine::core {

enum class LogLevel : std::uint8_t {
  Debug,
  Info,
  Warning,
  Error
};

class Logger {
public:
  Logger() = delete;

  static void setLevel(LogLevel level) { _level = level; }

  static void log(LogLevel level, std::string_view message) {
    if (level < _level) {
      return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::clog << "[" << std::put_time(std::localtime(&time), "%H:%M:%S") << "] "
              << toString(level) << " " << message << '\n';
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
  static LogLevel _level;
};

inline LogLevel Logger::_level = LogLevel::Info;

} // namespace engine::core
