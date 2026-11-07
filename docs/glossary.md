# Glossary

Terms used across this site, in alphabetical order, with where they show up in R-Type.

| Term | Meaning | In R-Type |
| --- | --- | --- |
| **AABB** | axis-aligned bounding box: a rectangle that never rotates; two of them overlap when they overlap on both axes | every collision test in `game/` |
| **Authoritative server** | the only program allowed to decide what happens in the game; clients only send requests | `r-type_server` decides movement, hits, score and lives |
| **Big-endian** | byte order with the most significant byte first, the usual order on networks | every multi-byte field of the protocol |
| **Bydos** | the alien enemies of R-Type | spawned at random on the right by the server |
| **CI** | continuous integration: build and test every change automatically | GitHub Actions, Linux and Windows |
| **Component** | a piece of plain data attached to an entity, with no behaviour | `Position`, `Velocity`, `Health`… in `common/components` |
| **CPM** | CPM.cmake, the package manager that downloads and pins our dependencies from CMake | Asio, SFML, Catch2 |
| **Datagram** | one UDP message, delivered whole or not at all | at most 1200 bytes in our protocol |
| **ECS** | entity–component–system: game objects are ids, their data are components, the logic is systems | `engine/ecs`, used by the server |
| **Entity** | a game object reduced to an id | a ship, a Bydos or a missile; ids are never reused |
| **Fixed timestep** | the simulation advances by equal steps of time, whatever the machine speed | 60 ticks per second on the server |
| **Head-of-line blocking** | with TCP, one lost segment holds back all the data behind it until it is resent | the main reason we use UDP |
| **Heartbeat** | a small message sent regularly to prove a peer is still alive | PING every second; silence for 3 s means a timeout |
| **Interpolation** | drawing an entity between two received positions to smooth its movement | not done yet |
| **Magic number** | fixed bytes at the start of every packet, to reject data that is not ours | `0x5254` ("RT") |
| **Mermaid** | a text format for diagrams, drawn by this site and by GitHub | every diagram of this site |
| **MkDocs** | the tool that turns the Markdown files of `docs/` into this website | with the Material theme |
| **Mutex** | a lock that lets only one thread at a time touch shared data | inside `ThreadSafeQueue` |
| **netem** | a Linux tool to add loss, delay or bandwidth limits to a network interface | to test the game on a bad network |
| **Payload** | the part of a packet after the header | e.g. 1 byte of key flags for INPUT |
| **Prediction** | moving the local ship at once, before the server confirms, then correcting | not done yet |
| **RFC** | "request for comments": the classic format of internet protocol specifications | our [Protocol RFC](protocol/rfc.md) |
| **Sequence number** | a counter in each packet, incremented by its sender | lets the server ignore an INPUT older than the last one |
| **Snapshot** | the state of every entity at one tick | ENTITY_UPDATE, 30 times per second |
| **Sparse array** | an array indexed by entity id where most slots may be empty | one per component type in the registry |
| **System** | a function that runs every tick on the entities having some components | `movement`, `projectile-collisions`… |
| **Thread** | an independent flow of execution inside a program | the server has a network thread and a game loop thread |
| **Tick** | one step of the server simulation | 1/60 s |
| **Timeout** | the delay after which a silent peer is considered gone | 3 seconds, both ways |
| **UDP / TCP** | the two internet transports: UDP sends independent datagrams, TCP a reliable ordered stream | all game traffic uses UDP |
| **View** | in the ECS, the list of entities that have a given set of components | `registry.view<Position, Velocity>()` |
