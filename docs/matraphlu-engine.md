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

Catch2 is fetched by CPM when tests are enabled. `engine_tests` covers the ECS registry, fixed
timestep, event bus, scheduler, logger, and core tools; `game_tests` covers player input and fire,
movement, bounds, enemy waves, collisions, damage, score, lives, and cleanup. Catch2 assertions
remain active in Release builds. These tests do not open a graphics window or start a network
server.

To run optimized tests as well, configure a separate Release build directory:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DRTYPE_BUILD_TESTS=ON
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

## Gameplay system order

Each server tick runs, in this order:

1. player input, applied by the server with `applyPlayerInput` and `firePlayerProjectile`;
2. the systems from `registerEnemySystems`: `enemy-spawner`, then `enemy-fire`;
3. the systems from `registerGameplaySystems`: `movement`, `player-bounds`, `offscreen-cleanup`,
   `projectile-collisions`, `ship-collisions`, `player-deaths`, `dead-entity-cleanup`.

Register the enemy systems before the gameplay systems on the same scheduler. Both take the
fixed-tick duration; `registerEnemySystems` also takes a seed, so a wave can be replayed exactly.

- `updateMovement(registry, deltaSeconds)` updates every entity with `Position` and `Velocity`.
- `applyPlayerInput` converts normalized horizontal/vertical input to velocity and prevents faster
  diagonal movement.
- `updatePlayerMovement(registry, deltaSeconds)` updates only players.
- `keepEntitiesOnScreen(registry, bounds)` clamps players, accounting for an optional hitbox.
- `spawnEnemy` creates an enemy with position, velocity, health, hitbox, and score value.
- `spawnRandomEnemy` uses a caller-owned seeded `std::mt19937`, keeping tests reproducible.
- `registerEnemySystems` spawns a Bydos every `EnemyWaveSettings::spawnIntervalSeconds` and gives
  each on-screen Bydos a `fireChancePerSecond` chance per second to shoot at a random player.
- `fireProjectile` creates a projectile at the shooter's hitbox center when available.
- `firePlayerProjectile` fires a friendly projectile to the right, owned by the player's id.
- `fireEnemyProjectile` aims a hostile projectile at a player target.
- `resolveProjectileCollisions` applies opposing-side projectile damage using top-left AABBs and
  removes each projectile after its first hit.
- A Bydos killed by a friendly projectile adds its `Enemy::points` to the shooter's
  `Player::score`.
- `resolveShipCollisions` handles a ship touching a Bydos: the Bydos is destroyed and the player
  loses one hit point, without scoring.
- `handlePlayerDeaths` takes one life from each player at zero health. With lives left, health is
  refilled and the ship keeps flying; on the last life it stays at zero.
- `removeOffscreenEntities` destroys every non-player entity more than `OFFSCREEN_MARGIN` pixels
  outside the screen (missed shots, Bydos that flew past), so the server never keeps or sends
  entities nobody can see.
- `removeDeadEntities` removes entities whose health is zero or below.

Invalid deltas, world bounds, spawn data, or projectile inputs throw `std::invalid_argument`;
callers at the game-loop boundary should handle invalid input without allowing an exception to
escape the loop.

`Projectile::ownerId` is compared with `Player::id` to prevent a friendly projectile from hitting
its owner; enemy shots use `Projectile::NO_OWNER`. Hostile projectiles target players; friendly
projectiles target enemies. `Player::id`, `Player::score`, and `Projectile::ownerId` are
fixed-width integers because the server serializes them.

Entity ids are never reused. That keeps a late network packet about a destroyed entity from being
applied to a new entity that got the same id.

## Adding a component or system

Components belong in `common/components/` and should remain plain data. Register each component
in `game::registerSystems`, then implement system logic in `game/` using a registry view rather
than walking unrelated component arrays. Add a deterministic test to `tests/game/` (or `tests/engine/` for engine code) for behavior
that can be exercised without graphics or networking. Changes to the frozen shared component
contract require the team's contract-approval process.

## Generic core utilities

- `ResourceManager` accepts typed loader callbacks, caches loaded resources by id, supports
  preloading, retrieval, unloading, and cache clearing. It does not depend on any graphics or
  audio library; the owning module supplies its resource type and loader.
- `DeveloperConsole` provides a small command dispatcher and typed boolean, integer, floating
  point, and string CVars. Applications can attach a UI or terminal front end around `execute`.
- `Logger` can be called from several threads at once (network thread and game loop): the level
  is atomic and each line is written under a lock.
- `Metrics` collects frame time/FPS, entity count, and host-supplied CPU and memory readings.
  The renderer-owned graphical metrics overlay can display this snapshot; platform-specific
  process sampling and network stats remain the responsibility of the application modules.
