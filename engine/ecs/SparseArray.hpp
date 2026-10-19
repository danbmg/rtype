#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace engine::ecs {

template<typename Component>
class SparseArray {
public:
  using value_type = std::optional<Component>;
  using reference_type = value_type&;
  using const_reference_type = const value_type&;
  using container_type = std::vector<value_type>;
  using size_type = typename container_type::size_type;
  using iterator = typename container_type::iterator;
  using const_iterator = typename container_type::const_iterator;

  SparseArray() = default;
  SparseArray(const SparseArray&) = default;
  SparseArray(SparseArray&&) noexcept = default;
  SparseArray& operator=(const SparseArray&) = default;
  SparseArray& operator=(SparseArray&&) noexcept = default;

  reference_type operator[](std::size_t index) { return _data[index]; }
  const_reference_type operator[](std::size_t index) const { return _data[index]; }

  std::size_t size() const { return _data.size(); }

  iterator begin() { return _data.begin(); }
  const_iterator begin() const { return _data.begin(); }
  iterator end() { return _data.end(); }
  const_iterator end() const { return _data.end(); }

  reference_type insert(std::size_t index, const Component& component) {
    reserve(index);
    _data[index] = component;
    return _data[index];
  }

  reference_type insert(std::size_t index, Component&& component) {
    reserve(index);
    _data[index] = std::move(component);
    return _data[index];
  }

  void erase(std::size_t index) {
    if (index < _data.size()) {
      _data[index].reset();
    }
  }

  reference_type at(std::size_t index) {
    reserve(index);
    return _data[index];
  }

  const_reference_type at(std::size_t index) const {
    if (index >= _data.size()) {
      static const value_type empty = std::nullopt;
      return empty;
    }
    return _data[index];
  }

  void clear() { _data.clear(); }

private:
  void reserve(std::size_t index) {
    if (index >= _data.size()) {
      _data.resize(index + 1);
    }
  }

  container_type _data;
};

} // namespace engine::ecs
