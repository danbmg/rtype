#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace engine::core {

class DeveloperConsole {
public:
  using CVar = std::variant<bool, std::int64_t, double, std::string>;
  using Command = std::function<std::string(const std::vector<std::string>&)>;

  void registerCommand(std::string name, Command command) {
    if (name.empty() || !command) {
      throw std::invalid_argument("command name and callback must be valid");
    }
    if (!_commands.emplace(std::move(name), std::move(command)).second) {
      throw std::invalid_argument("command is already registered");
    }
  }

  void defineVariable(std::string name, CVar value) {
    if (name.empty()) {
      throw std::invalid_argument("CVar name must not be empty");
    }
    if (!_variables.emplace(std::move(name), std::move(value)).second) {
      throw std::invalid_argument("CVar is already defined");
    }
  }

  const CVar* getVariable(std::string_view name) const {
    const auto variable = _variables.find(std::string(name));
    return variable == _variables.end() ? nullptr : &variable->second;
  }

  void setVariable(std::string_view name, std::string_view value) {
    const auto variable = _variables.find(std::string(name));
    if (variable == _variables.end()) {
      throw std::out_of_range("unknown CVar: " + std::string(name));
    }
    variable->second = parseLike(variable->second, value);
  }

  std::string execute(std::string_view line) {
    const auto arguments = tokenize(line);
    if (arguments.empty()) {
      return {};
    }

    if (arguments.front() == "get") {
      if (arguments.size() != 2) {
        throw std::invalid_argument("usage: get <name>");
      }
      const auto* variable = getVariable(arguments[1]);
      if (variable == nullptr) {
        throw std::out_of_range("unknown CVar: " + arguments[1]);
      }
      return format(*variable);
    }
    if (arguments.front() == "set") {
      if (arguments.size() != 3) {
        throw std::invalid_argument("usage: set <name> <value>");
      }
      setVariable(arguments[1], arguments[2]);
      return format(*getVariable(arguments[1]));
    }

    const auto command = _commands.find(arguments.front());
    if (command == _commands.end()) {
      throw std::out_of_range("unknown command: " + arguments.front());
    }
    return command->second(arguments);
  }

  static std::vector<std::string> tokenize(std::string_view line) {
    std::istringstream stream{std::string(line)};
    std::vector<std::string> tokens;
    for (std::string token; stream >> token;) {
      tokens.push_back(std::move(token));
    }
    return tokens;
  }

private:
  static CVar parseLike(const CVar& original, std::string_view value) {
    if (std::holds_alternative<bool>(original)) {
      if (value == "true" || value == "1") {
        return true;
      }
      if (value == "false" || value == "0") {
        return false;
      }
      throw std::invalid_argument("boolean CVar value must be true, false, 1, or 0");
    }
    if (std::holds_alternative<std::int64_t>(original)) {
      std::int64_t parsed = 0;
      const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
      if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument("invalid integer CVar value");
      }
      return parsed;
    }
    if (std::holds_alternative<double>(original)) {
      std::istringstream stream{std::string(value)};
      double parsed = 0.0;
      stream >> parsed;
      if (!stream || stream.peek() != std::char_traits<char>::eof() || !std::isfinite(parsed)) {
        throw std::invalid_argument("invalid floating-point CVar value");
      }
      return parsed;
    }
    return std::string(value);
  }

  static std::string format(const CVar& value) {
    return std::visit(
        [](const auto& item) {
          std::ostringstream stream;
          if constexpr (std::is_same_v<std::decay_t<decltype(item)>, bool>) {
            stream << (item ? "true" : "false");
          } else {
            stream << item;
          }
          return stream.str();
        },
        value);
  }

  std::map<std::string, Command, std::less<>> _commands;
  std::map<std::string, CVar, std::less<>> _variables;
};

} // namespace engine::core
