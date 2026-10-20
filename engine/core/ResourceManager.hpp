#pragma once

#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace engine::core {

class ResourceManager {
public:
  template<typename Resource>
  using Loader = std::function<std::shared_ptr<Resource>(const std::string&)>;

  template<typename Resource>
  void registerLoader(Loader<Resource> loader) {
    if (!loader) {
      throw std::invalid_argument("resource loader must be callable");
    }
    _loaders.insert_or_assign(std::type_index(typeid(Resource)), std::move(loader));
  }

  template<typename Resource>
  std::shared_ptr<Resource> load(const std::string& id) {
    if (id.empty()) {
      throw std::invalid_argument("resource id must not be empty");
    }

    const auto type = std::type_index(typeid(Resource));
    const auto loader = _loaders.find(type);
    if (loader == _loaders.end()) {
      throw std::logic_error("no loader registered for requested resource type");
    }

    auto storage = _resources.find(type);
    if (storage == _resources.end()) {
      storage = _resources.emplace(type, std::unordered_map<std::string, std::shared_ptr<Resource>>{})
                    .first;
    }
    auto& resources = std::any_cast<std::unordered_map<std::string, std::shared_ptr<Resource>>&>(
        storage->second);
    if (const auto cached = resources.find(id); cached != resources.end()) {
      return cached->second;
    }

    const auto& typedLoader = std::any_cast<const Loader<Resource>&>(loader->second);
    auto resource = typedLoader(id);
    if (!resource) {
      throw std::runtime_error("resource loader returned an empty resource for id: " + id);
    }
    resources.emplace(id, resource);
    return resource;
  }

  template<typename Resource>
  std::vector<std::shared_ptr<Resource>> preload(const std::vector<std::string>& ids) {
    std::vector<std::shared_ptr<Resource>> loaded;
    loaded.reserve(ids.size());
    for (const auto& id : ids) {
      loaded.push_back(load<Resource>(id));
    }
    return loaded;
  }

  template<typename Resource>
  std::shared_ptr<Resource> get(const std::string& id) const {
    const auto storage = _resources.find(std::type_index(typeid(Resource)));
    if (storage == _resources.end()) {
      return nullptr;
    }
    const auto& resources = std::any_cast<
        const std::unordered_map<std::string, std::shared_ptr<Resource>>&>(storage->second);
    const auto resource = resources.find(id);
    return resource == resources.end() ? nullptr : resource->second;
  }

  template<typename Resource>
  bool unload(const std::string& id) {
    const auto storage = _resources.find(std::type_index(typeid(Resource)));
    if (storage == _resources.end()) {
      return false;
    }
    auto& resources = std::any_cast<std::unordered_map<std::string, std::shared_ptr<Resource>>&>(
        storage->second);
    return resources.erase(id) != 0;
  }

  void clear() { _resources.clear(); }

private:
  std::unordered_map<std::type_index, std::any> _loaders;
  std::unordered_map<std::type_index, std::any> _resources;
};

} // namespace engine::core
