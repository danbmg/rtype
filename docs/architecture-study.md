# Architecture and tooling study

## ECS data organization

This project uses a sparse array per component type. Each entity identifier indexes its optional
component value, while registry views select live entities that have every requested component.
This keeps component data plain and makes system inputs explicit.

An object-oriented hierarchy would place behavior and state together, but gameplay entities do
not share one stable inheritance tree: players, enemies, and projectiles need different mixtures
of properties. An archetype/chunk ECS can improve cache locality at scale, but adds structural
change and query complexity that is not justified for this project scope. Sparse arrays provide
a small, inspectable implementation suited to the expected entity count and the team's C++ goals.

The trade-off is that a view currently walks live entities and checks each requested component.
That is simple and correct for this prototype, but a large-scale engine could instead maintain
component intersections or archetype indexes and benchmark them against representative scenes.

## Build system

CMake is used because it supports the required Linux and Windows toolchains, generates IDE/native
build files, and provides target-based module composition. The engine's `engine-core` and
`engine-ecs` targets can be selected independently; `engine::engine` is a convenient aggregate.
Meson is a reasonable alternative with concise configuration and fast Ninja integration, but the
team's CMake requirements and ecosystem support make CMake the lower-risk choice here.

## Dependency management

CPM.cmake is a small CMake-based dependency fetcher, so pinned dependencies can be declared with
their targets alongside the build. It does not require copying dependency sources into the
repository. Conan and vcpkg provide broader package/version and binary-cache features; they add
separate package-manager configuration and tool installation to this project. For the limited,
source-built dependency set, CPM keeps setup small. CI should cache its source directory and all
versions must remain pinned.

## Measurements and future scaling

The design favors ease of review and deterministic tests rather than claiming benchmark superiority.
Before replacing sparse-array views or changing dependency tooling, profile a representative
multi-player session and record build times, memory, and system costs. The engine metrics collector
keeps measurements supplied by the host application separate from platform-specific sampling.
