# Building the headless Pong example

This tutorial shows how to build a small second simulation using only the standalone engine ECS.
It deliberately has no rendering, audio, client, server, or R-Type component dependency.

## Build and run

From the repository root:

```sh
cmake -S . -B build -G Ninja -DRTYPE_BUILD_EXAMPLES=ON -DRTYPE_BUILD_TESTS=OFF
cmake --build build
./build/bin/engine_pong
```

For a standalone engine configure, the example option is also available:

```sh
cmake -S engine -B build-engine -G Ninja -DRTYPE_BUILD_EXAMPLES=ON
cmake --build build-engine
./build-engine/bin/engine_pong
```

The example simulates a bounded match between two paddles that track the ball and prints the
result and simulated frame rate. It is a command-line simulation, not a graphical game.

## ECS mapping

The example defines its own `Position`, `Velocity`, `Paddle`, `Ball`, and `Score` data types. It
creates each object as an entity and attaches only the relevant components. The simulation reads
and updates those values through the registry. This demonstrates that the engine does not know
about R-Type's shared components or gameplay rules.

## Adding another system

Keep data in components and place behavior in a function that receives a registry. For larger
examples, register those functions with `engine::core::SystemScheduler` in the order the
simulation needs. Test system rules with deterministic inputs before adding a rendering front end.
