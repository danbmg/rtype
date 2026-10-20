cmake_minimum_required(VERSION 3.21)
project(rtype_engine VERSION 1.0.0 LANGUAGES CXX)

# The engine also configures on its own: cmake -S engine -B build-engine
if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  option(RTYPE_BUILD_EXAMPLES "Build the engine examples" OFF)
endif()

# core and ecs are header-only. Consumers include "engine/<module>/<File>.hpp", so the include
# root is the directory that contains engine/.
set(ENGINE_INCLUDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_library(engine-core INTERFACE)
add_library(engine::core ALIAS engine-core)
target_include_directories(engine-core INTERFACE "$<BUILD_INTERFACE:${ENGINE_INCLUDE_ROOT}>")
target_compile_features(engine-core INTERFACE cxx_std_20)

add_library(engine-ecs INTERFACE)
add_library(engine::ecs ALIAS engine-ecs)
target_include_directories(engine-ecs INTERFACE "$<BUILD_INTERFACE:${ENGINE_INCLUDE_ROOT}>")
target_compile_features(engine-ecs INTERFACE cxx_std_20)

add_library(engine INTERFACE)
add_library(engine::engine ALIAS engine)
target_link_libraries(engine INTERFACE engine-core engine-ecs)

# Optional modules owned by other members (network: danbmg; render, audio, input: theodsh).
# Each one defines its own engine-<module> target and is added once its CMakeLists.txt exists.
foreach(module network render audio input)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module}/CMakeLists.txt")
    add_subdirectory(${module})
  endif()
endforeach()

if(RTYPE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
