#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace engine::core {

template<typename Registry>
class SystemScheduler {
public:
  using System = std::function<void(Registry&)>;

  void addSystem(std::string name, System system) {
    _systems.emplace_back(std::move(name), std::move(system));
  }

  template<typename Func>
  void addSystem(std::string name, Func&& system) {
    addSystem(std::move(name), System{std::forward<Func>(system)});
  }

  void run(Registry& registry) const {
    for (const auto& [name, system] : _systems) {
      (void)name;
      system(registry);
    }
  }

private:
  std::vector<std::pair<std::string, System>> _systems;
};

} // namespace engine::core
