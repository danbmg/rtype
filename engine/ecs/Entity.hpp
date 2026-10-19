#pragma once

#include <cstdint>
#include <limits>

namespace engine::ecs {

using Entity = std::uint32_t;

constexpr Entity INVALID_ENTITY = std::numeric_limits<Entity>::max();

} // namespace engine::ecs

namespace rtype::ecs {
using Entity = ::engine::ecs::Entity;
constexpr Entity INVALID_ENTITY = ::engine::ecs::INVALID_ENTITY;
} // namespace rtype::ecs
