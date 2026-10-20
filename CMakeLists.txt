cmake_minimum_required(VERSION 3.21)
project(rtype VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RTYPE_BUILD_TESTS "Build the unit tests (Catch2)" ON)
option(RTYPE_BUILD_CLIENT "Build the graphical client and fetch SFML" ON)
option(RTYPE_BUILD_EXAMPLES "Build the engine examples" OFF)
option(RTYPE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------------------------
# Package manager: CPM.cmake, downloaded once into CPM_SOURCE_CACHE and checked against its
# SHA256 so a tampered file is rejected. Every third-party library goes through CPMAddPackage
# below, with a pinned version; a version bump is a PR approved by the whole team.
# ---------------------------------------------------------------------------------------------
set(CPM_DOWNLOAD_VERSION 0.42.3)
set(CPM_HASH_SUM "a609e875fd532b067174250f6abbc3dac22fe2d64869783fb1e80bda1625c844")

if(CPM_SOURCE_CACHE)
  set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
  set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

file(DOWNLOAD
     https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
     ${CPM_DOWNLOAD_LOCATION} EXPECTED_HASH SHA256=${CPM_HASH_SUM})
include(${CPM_DOWNLOAD_LOCATION})

# ---------------------------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------------------------
find_package(Threads REQUIRED)

# Asio — standalone, header-only (engine/network only)
CPMAddPackage(NAME asio GITHUB_REPOSITORY chriskohlhoff/asio GIT_TAG asio-1-36-0 DOWNLOAD_ONLY YES)
if(NOT TARGET asio)
  add_library(asio INTERFACE)
  target_include_directories(asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
  target_compile_definitions(asio INTERFACE ASIO_STANDALONE
                                            $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0A00>)
  target_link_libraries(asio INTERFACE Threads::Threads $<$<PLATFORM_ID:Windows>:ws2_32 wsock32>)
endif()

# SFML 3 — client side only.
# SFML_USE_SYSTEM_DEPS OFF is mandatory: on Linux SFML otherwise links the system
# freetype/vorbis/flac/ogg, which the subject forbids (and breaks on clean machines).
if(RTYPE_BUILD_CLIENT)
  CPMAddPackage(NAME SFML GITHUB_REPOSITORY SFML/SFML GIT_TAG 3.0.2
    OPTIONS "SFML_BUILD_NETWORK OFF" "SFML_USE_SYSTEM_DEPS OFF" "BUILD_SHARED_LIBS OFF")
endif()

if(RTYPE_BUILD_TESTS)
  CPMAddPackage("gh:catchorg/Catch2@3.8.1")
  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
  enable_testing()
endif()

# ---------------------------------------------------------------------------------------------
# Helpers available to every module's CMakeLists.txt
# ---------------------------------------------------------------------------------------------

# Project-wide warning flags for a target we compile (not for dependencies).
function(rtype_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(RTYPE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    if(RTYPE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# Writes the executable at the repository root, as the subject requires for r-type_server and
# r-type_client. The $<1:...> generator expression stops multi-config generators (Visual Studio)
# from appending a Debug/ or Release/ subdirectory.
function(rtype_output_to_root target)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY $<1:${PROJECT_SOURCE_DIR}>)
endfunction()

# ---------------------------------------------------------------------------------------------
# Modules
# ---------------------------------------------------------------------------------------------

# Each module owns its CMakeLists.txt. A module is added as soon as its CMakeLists.txt exists,
# so members can merge their part independently. Order matters: dependencies come first.
# client/net is added here, not by client/CMakeLists.txt, so it builds without SFML.
set(RTYPE_MODULES engine common/components common/protocol game server client/net)
if(RTYPE_BUILD_CLIENT)
  list(APPEND RTYPE_MODULES client)
endif()
if(RTYPE_BUILD_TESTS)
  list(APPEND RTYPE_MODULES tests)
endif()

foreach(module IN LISTS RTYPE_MODULES)
  if(EXISTS "${PROJECT_SOURCE_DIR}/${module}/CMakeLists.txt")
    add_subdirectory(${module})
  endif()
endforeach()
