#pragma once

#include <any>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine::core {

class EventBus {
public:
  template<typename Event>
  using Handler = std::function<void(const Event&)>;

  template<typename Event>
  void subscribe(Handler<Event> handler) {
    _subscribers[std::type_index(typeid(Event))].emplace_back(
        [handler](const std::any& event) { handler(std::any_cast<const Event&>(event)); });
  }

  template<typename Event>
  void emit(const Event& event) const {
    const auto it = _subscribers.find(std::type_index(typeid(Event)));
    if (it == _subscribers.end()) {
      return;
    }

    for (const auto& subscriber : it->second) {
      subscriber(event);
    }
  }

private:
  using Subscriber = std::function<void(const std::any&)>;
  mutable std::unordered_map<std::type_index, std::vector<Subscriber>> _subscribers;
};

using Mediator = EventBus;

} // namespace engine::core

namespace engine {
using EventBus = core::EventBus;
using Mediator = core::Mediator;
} // namespace engine
