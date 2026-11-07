# Stack overview

Every technology in the project, what else we could have used, and why we did not. This page is
the summary; the three detailed studies give the full comparison for their area:

- [Engine, build and dependencies](architecture-study.md): ECS data layout, CMake, CPM;
- [Graphics and audio](client-study.md): SFML against SDL and raylib;
- [Networking, storage and security](network-study.md): Asio, UDP, serialization,
  synchronisation, threads, storage, security.

## The stack at a glance

| Need | Choice | Alternatives considered | Why this one | Details |
| --- | --- | --- | --- | --- |
| Language | **C++20** | Rust, Python, C# | C++ fits a real-time game: no garbage collector pausing a tick, direct control of memory layout (plain-data components, byte-exact packets), and SFML and Asio are native C++ libraries. Python is too slow for a 60 ticks/s loop; Rust would be a good fit, but the whole team works in C++. We use C++20 rather than an older standard for `std::span` (pass datagrams without copies), `std::bit_cast` (serialize floats without undefined behaviour) and `contains()` on maps | — |
| Build system | **CMake ≥ 3.21** | Meson, plain Make | Required to build on Linux and Windows; one target per module enforces the dependency rule; every IDE reads it | [study](architecture-study.md#build-system) |
| Package manager | **CPM.cmake** | Conan, vcpkg | A single CMake script, no extra tool to install; dependencies are built from source at exact versions; nothing vendored in the repository | [study](architecture-study.md#dependency-management) |
| Game architecture | **ECS with sparse arrays** | class hierarchy, archetype ECS | Ships, Bydos and missiles mix properties freely; plain data is easy to test and to send over the network; sparse arrays are small enough to explain line by line | [study](architecture-study.md#ecs-data-organization) |
| Graphics and input | **SFML 3** | SDL 3, raylib | C++ API for window, sprites, text and keyboard; small enough to hide behind our own `IRenderer` / `IInput` interfaces | [study](client-study.md) |
| Audio | **SFML Audio** | SDL 3 audio, raylib audio | Same library as graphics: no second dependency for two sound effects; hidden behind `IAudio` | [study](client-study.md#resource-ownership-and-drawing) |
| Networking library | **standalone Asio** | Boost.Asio, raw sockets, SFML Network, ENet | Same code on Linux and Windows, asynchronous model that maps onto a dedicated network thread, no Boost; ENet would hide the protocol we must design | [study](network-study.md#1-networking-library) |
| Transport | **UDP** | TCP | A lost TCP segment holds back every later position; in an action game the newest state matters more than the complete history | [study](network-study.md#2-transport-and-format) |
| Wire format | **hand-written binary** | JSON, Protocol Buffers, `memcpy` of structs | 14 bytes per entity instead of about 50 in JSON; every byte documented in the RFC and validated; no code generator; `memcpy` breaks on padding and byte order | [study](network-study.md#3-serialization) |
| State synchronisation | **full snapshots + events** | events only, lockstep, delta snapshots | Self-healing: the next snapshot repairs any loss, with no acknowledgement needed. About 12 KiB/s per client with 4 players | [study](network-study.md#4-state-synchronisation) |
| Concurrency | **one network thread + one game loop thread + a queue** | one thread per client, a single thread | The game state belongs to one thread, so it needs no lock; only the queue is shared | [study](network-study.md#5-concurrency) |
| Storage | **memory only** (SQLite planned for scores) | JSON files, PostgreSQL, Redis | The game has nothing to keep between games; SQLite is one file with transactions and no server | [study](network-study.md#6-storage) |
| Tests | **Catch2 3** | GoogleTest, doctest | Test names are plain sentences ("A flooding client is rate limited"), each test is registered in CTest automatically, fetched by CPM like the rest | — |
| Code style | **clang-format, clang-tidy** | none, a written style guide | One shared configuration at the root; formatting is never discussed in review | — |
| Continuous integration | **GitHub Actions** | GitLab CI, Jenkins | The code is on GitHub; hosted Linux and Windows machines; dependency cache keyed on the CMake files | — |
| Documentation | **MkDocs + Material** | Docusaurus, Sphinx, Doxygen, a wiki | Markdown files next to the code, reviewed in the same pull requests; search, outline, dark mode and Mermaid diagrams built in; a Python tool only. Doxygen alone is not enough for the subject; a wiki is not versioned with the code | — |

We use no game engine and no library with a too broad scope (Unity, Unreal, Godot…):
the ECS, the game loop, the network layer and the protocol are ours. Third-party code is limited
to Asio (sockets), SFML (window, drawing, sound) and Catch2 (tests), all pinned to exact versions.

## Algorithms and data structures

This section lists the existing algorithms and structures we use, and the ones we designed
ourselves, and why. We wrote the core ones ourselves because the game is built on our own
engine, and because each fits in a few dozen lines that we can explain and test.

| Problem | Solution | Existing or ours | Why | Limit |
| --- | --- | --- | --- | --- |
| store components | one sparse array per component type (`std::vector<std::optional<T>>`), indexed by entity id | ours | O(1) access by id, plain data, simple to test | ids are never reused, so the arrays grow with every entity created during a game; an archetype layout would be faster beyond thousands of entities |
| select entities for a system | registry views over several component types | ours | systems say what data they read | a view walks every live entity |
| run systems in a fixed order | `SystemScheduler`, a named ordered list | ours | the order (spawn → move → collide → die → clean) is a gameplay rule, so it must be explicit | — |
| step the simulation | fixed timestep with an accumulator, at most 5 catch-up ticks | classic pattern, our implementation | speed independent of CPU and frame rate; a stall cannot snowball | — |
| detect collisions | axis-aligned bounding boxes (AABB), every projectile against every target | classic | exact enough for rectangles of sprites, trivial to test | O(projectiles × targets): fine for tens of entities, a spatial grid would be next |
| random waves | `std::mt19937` with a seed | standard library | the same seed replays the same wave in tests | not for security |
| decouple modules | interfaces `IRenderer` / `IInput` / `IAudio` (dependency inversion); an event bus (mediator pattern) in `engine/core` | classic patterns | the client code never calls SFML directly, so SFML can be replaced behind the interfaces | the event bus is ready in the engine but R-Type does not use it yet |
| hand packets between threads | bounded queue: `std::deque` + `std::mutex`, drained once per tick | classic producer–consumer | easy to prove correct, constant memory under a flood | a lock-free ring buffer would avoid the lock, not needed at our rates |
| write and read packets | field by field, big-endian, bounds-checked reader; floats through `std::bit_cast` | ours | identical bytes on every compiler and CPU; never reads past the datagram | hand-written: each packet needs tests (they exist) |
| ignore old datagrams | sequence comparison that survives the wrap from 2³²−1 to 0: `int32_t(a − b) > 0` | classic (serial number arithmetic) | a delayed INPUT cannot overwrite a newer one | — |
| find spawned and destroyed entities | compare this tick's entity set with the last one in a hash map | ours | O(n) per tick; gameplay code never calls the network | — |
| player sessions | fixed array of 4 optional slots, slot index = player id | ours | no allocation, lowest free id for free | fixed maximum of 4 players (the game's) |

## How the choices are kept honest

- **Pinned versions:** Asio 1.36.0, SFML 3.0.2, Catch2 3.8.1, CPM 0.42.3, MkDocs 1.6.1,
  Material 9.6.14. A version bump is a pull request approved by the whole team.
- **Measured, not assumed:** bandwidth, flood resistance and memory figures in the network study
  come from runs on our machines, and the tests check the claims that can be automated.
- **Replaceable:** Asio is used in one file, SFML only behind three interfaces. Changing either
  does not touch the game, the server or the protocol.
