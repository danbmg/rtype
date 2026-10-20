#pragma once

#include <algorithm>
#include <any>
#include <functional>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "engine/ecs/Entity.hpp"
#include "engine/ecs/SparseArray.hpp"

namespace engine::ecs {

class Registry {
public:
  using Entity = engine::ecs::Entity;

  Entity spawnEntity() {
    const auto entity = static_cast<Entity>(_nextEntity++);
    _aliveEntities.push_back(entity);
    return entity;
  }

  bool containsEntity(Entity entity) const {
    return std::find(_aliveEntities.begin(), _aliveEntities.end(), entity) != _aliveEntities.end();
  }

  void killEntity(Entity entity) {
    const auto it = std::find(_aliveEntities.begin(), _aliveEntities.end(), entity);
    if (it == _aliveEntities.end()) {
      return;
    }
    _aliveEntities.erase(it);
    for (const auto& [type, removeComponent] : _componentRemovers) {
      (void)type;
      removeComponent(_components.at(type), entity);
    }
  }

  void clear() {
    _aliveEntities.clear();
    _components.clear();
    _componentRemovers.clear();
    _nextEntity = 0;
  }

  template<typename Component>
  SparseArray<Component>& registerComponent() {
    const auto type = std::type_index(typeid(Component));
    auto& components = _components[type];
    if (!components.has_value()) {
      components = SparseArray<Component>{};
      _componentRemovers.emplace(type, [](std::any& storage, Entity entity) {
        std::any_cast<SparseArray<Component>&>(storage).erase(static_cast<std::size_t>(entity));
      });
    }
    return std::any_cast<SparseArray<Component>&>(components);
  }

  template<typename Component>
  void addComponent(Entity entity, Component component) {
    if (!containsEntity(entity)) {
      throw std::invalid_argument("cannot add a component to an entity that is not alive");
    }
    auto& sparse = registerComponent<Component>();
    sparse.insert(static_cast<std::size_t>(entity), std::move(component));
  }

  template<typename Component>
  void removeComponent(Entity entity) {
    auto& sparse = registerComponent<Component>();
    sparse.erase(static_cast<std::size_t>(entity));
  }

  template<typename Component>
  SparseArray<Component>& getComponents() {
    return registerComponent<Component>();
  }

  template<typename Component>
  const SparseArray<Component>& getComponents() const {
    const auto type = std::type_index(typeid(Component));
    return std::any_cast<const SparseArray<Component>&>(_components.at(type));
  }

  template<typename Component>
  Component* getComponent(Entity entity) {
    const auto it = _components.find(std::type_index(typeid(Component)));
    if (it == _components.end()) {
      return nullptr;
    }
    auto* sparse = std::any_cast<SparseArray<Component>>(&it->second);
    if (sparse == nullptr) {
      return nullptr;
    }
    if (static_cast<std::size_t>(entity) >= sparse->size()) {
      return nullptr;
    }
    auto& optionalValue = (*sparse)[static_cast<std::size_t>(entity)];
    return optionalValue.has_value() ? &optionalValue.value() : nullptr;
  }

  template<typename Component>
  const Component* getComponent(Entity entity) const {
    const auto it = _components.find(std::type_index(typeid(Component)));
    if (it == _components.end()) {
      return nullptr;
    }
    const auto* sparse = std::any_cast<SparseArray<Component>>(&it->second);
    if (sparse == nullptr) {
      return nullptr;
    }
    if (static_cast<std::size_t>(entity) >= sparse->size()) {
      return nullptr;
    }
    const auto& optionalValue = (*sparse)[static_cast<std::size_t>(entity)];
    return optionalValue.has_value() ? &optionalValue.value() : nullptr;
  }

  template<typename... Components>
  std::vector<Entity> view() const {
    std::vector<Entity> entities;
    for (const auto entity : _aliveEntities) {
      bool matches = true;
      ((matches = matches && (getComponent<Components>(entity) != nullptr)), ...);
      if (matches) {
        entities.push_back(entity);
      }
    }
    return entities;
  }

  template<typename Func>
  void runSystem(Func&& func) {
    for (const auto entity : _aliveEntities) {
      func(entity, *this);
    }
  }

private:
  std::unordered_map<std::type_index, std::any> _components;
  std::unordered_map<std::type_index, std::function<void(std::any&, Entity)>> _componentRemovers;
  std::vector<Entity> _aliveEntities;
  Entity _nextEntity = 0;
};

} // namespace engine::ecs
