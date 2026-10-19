# Engine and gameplay systems

The server-side game loop owns the `engine::ecs::Registry`. Components are plain data, and
systems process entities selected by registry views. Gameplay code depends on the generic engine
and shared components; the engine itself does not depend on the game, client, or server.

## Running the matraphlu checks

From the repository root:

```sh
cmake -S . -B build -G Ninja -DRTYPE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

For a manual game loop, call `FixedTimestep::update()` each loop and run one simulation tick when
it returns `true`. If the loop needs to process accumulated catch-up ticks, call
`FixedTimestep::consumeStep()` repeatedly while it returns `true`.

Catch2 is fetched by CPM when tests are enabled. The `engine_tests` executable has separate test
cases for the ECS registry, fixed timestep, event bus, scheduler, player input, movement, bounds,
enemy spawning, projectile collisions, damage, and cleanup. Catch2 assertions remain active in
Release builds. These tests do not open a graphics window or start a network server.

To run optimized tests as well, configure a separate Release build directory:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DRTYPE_BUILD_TESTS=ON
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

## Gameplay system order

Call `registerGameplaySystems` with the scheduler and fixed-tick duration to register movement,
player bounds, projectile collisions, and dead-entity cleanup in that order. The server should
apply player input and create scheduled enemy spawns/shots as appropriate around each scheduler
tick. The systems are deliberately independent so the server can own input/network integration.

- `updateMovement(registry, deltaSeconds)` updates every entity with `Position` and `Velocity`.
- `applyPlayerInput` converts normalized horizontal/vertical input to velocity and prevents faster
  diagonal movement.
- `updatePlayerMovement(registry, deltaSeconds)` updates only players.
- `keepEntitiesOnScreen(registry, bounds)` clamps players, accounting for an optional hitbox.
- `spawnEnemy` creates an enemy with position, velocity, health, hitbox, and score value.
- `spawnRandomEnemy` uses a caller-owned seeded `std::mt19937`, keeping tests reproducible.
- `fireProjectile` creates a projectile at the shooter's hitbox center when available.
- `fireEnemyProjectile` aims a hostile projectile at a player target.
- `resolveProjectileCollisions` applies opposing-side projectile damage using top-left AABBs and
  removes each projectile after its first hit.
- `removeDeadEntities` removes entities whose health is zero or below.

Invalid deltas, world bounds, spawn data, or projectile inputs throw `std::invalid_argument`;
callers at the game-loop boundary should handle invalid input without allowing an exception to
escape the loop.

`Projectile::ownerId` is compared with `Player::id` to prevent a friendly projectile from hitting
its owner. Hostile projectiles target players; friendly projectiles target enemies.

## Adding a component or system

Components belong in `common/components/` and should remain plain data. Register each component
in `game::registerSystems`, then implement system logic in `game/` using a registry view rather
than walking unrelated component arrays. Add a deterministic test to `tests/engine/` for behavior
that can be exercised without graphics or networking. Changes to the frozen shared component
contract require the team's contract-approval process.

## Generic core utilities

- `ResourceManager` accepts typed loader callbacks, caches loaded resources by id, supports
  preloading, retrieval, unloading, and cache clearing. It does not depend on any graphics or
  audio library; the owning module supplies its resource type and loader.
- `DeveloperConsole` provides a small command dispatcher and typed boolean, integer, floating
  point, and string CVars. Applications can attach a UI or terminal front end around `execute`.
- `Metrics` collects frame time/FPS, entity count, and host-supplied CPU and memory readings.
  The renderer-owned graphical metrics overlay can display this snapshot; platform-specific
  process sampling and network stats remain the responsibility of the application modules.
