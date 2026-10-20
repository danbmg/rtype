# Comparative study: networking, storage and security

This study explains the technical choices behind the network layer, the server and its data,
the alternatives we considered, and why we did not take them. Numbers come from our own
measurements on a laptop (loopback interface) unless stated otherwise.

## 1. Networking library

| Option | Pros | Cons |
| --- | --- | --- |
| **Standalone Asio** (chosen) | Header-only, no Boost; same code on Linux and Windows; asynchronous model (`io_context`) that maps directly onto a dedicated network thread; mature and documented; basis of the C++ Networking TS | Verbose; its error codes and handlers need care (e.g. Windows reports an ICMP "port unreachable" as a receive error) |
| Boost.Asio | Same API | Pulls Boost into the build for nothing more |
| Raw BSD sockets / Winsock | No dependency, full control | Two APIs to maintain (`WSAStartup`, `errno` vs `WSAGetLastError`, non-blocking modes differ); everything Asio already tests would have to be rewritten |
| SFML Network | Already used by the client | The server would depend on SFML, which our architecture forbids; minimal UDP support, no asynchronous model |
| ENet / GameNetworkingSockets | Reliability, channels and fragmentation out of the box | Hide the protocol we must design ourselves; reliability is precisely our part 2 track |

Asio is used in one file only (`engine/network/UdpSocket.cpp`) and linked privately: the rest of
the project sees `UdpSocket` and `Endpoint`, so the library could be replaced without touching
the server, the client or the game.

## 2. Transport and format

**UDP rather than TCP.** TCP delivers bytes in order: one lost segment blocks everything behind
it until it is resent (head-of-line blocking), so a position from 200 ms ago can delay the one
from now. In an action game the newest state matters more than a complete history. UDP delivers
each datagram on its own or not at all; our protocol is built so that losing one does not matter
(section 4). The subject also requires UDP for all in-game traffic.

**Binary rather than text.** One entity in a snapshot takes 14 bytes. The same information as
JSON, `{"id":5,"kind":1,"player":0,"x":60.0,"y":111.5}`, takes about 50: three to four times the
bandwidth, plus parsing cost and ambiguities (number formats, escaping). A binary format with
fixed sizes is also easier to validate: each field has one size and one valid range.

## 3. Serialization

| Option | Verdict |
| --- | --- |
| `memcpy` of C++ structs | Rejected: padding, alignment and byte order depend on the compiler and the CPU, so two builds could disagree; and it trusts received bytes blindly |
| Protocol Buffers, FlatBuffers, Cap'n Proto | Schema evolution and generated code, but a code generator in the build, larger messages (Protocol Buffers) and a format we would not have designed |
| **Hand-written field-by-field encoding** (chosen) | `BinaryWriter` / `BinaryReader` write and read one field at a time in big-endian; small, explicit, and every byte is documented in the RFC |

The cost of writing each packet by hand is paid back by tests: every packet type is round-tripped,
truncated at every length, and 100 000 random or corrupted datagrams are decoded, with the rule
that anything accepted must re-encode to exactly the same bytes.

## 4. State synchronisation

| Option | Pros | Cons |
| --- | --- | --- |
| Events only ("entity 5 moved by 3 px") | Small | One lost datagram desynchronises the client forever |
| Lockstep (everyone exchanges inputs, everyone simulates) | Tiny bandwidth | The slowest player sets the pace; needs a perfectly deterministic simulation on every machine; the server would no longer be the only authority |
| **Full snapshots + events** (chosen) | Self-healing: the next snapshot fixes any loss; simple | Bandwidth grows with the number of entities |
| Delta snapshots against the last acknowledged one | Much smaller | Needs acknowledgements: part 2 |

Measured with 4 players: 74 datagrams per second and **12 KiB/s per client** including IPv4 and
UDP headers, for up to 26 entities per snapshot. The events (ENTITY_SPAWN, ENTITY_DESTROY) are
only there for sounds and effects; the snapshots carry the truth.

## 5. Concurrency

| Option | Verdict |
| --- | --- |
| One thread per client | Pointless with UDP (one socket for everyone), and 4+ threads touching the game state need locks everywhere |
| Everything on one thread | The game loop would block on the network or the network would wait for the game |
| **One network thread + one game loop thread + a queue** (chosen) | The game state belongs to one thread, so it needs no lock; only the queue is shared |

The queue is a `std::deque` behind a `std::mutex`, bounded to 4096 items. A lock-free ring buffer
would avoid the mutex, but the game loop takes the lock once per tick (`drain()`) and the
network thread once per datagram: contention is negligible, and a mutex is far easier to prove
correct. We measured the server absorbing about 100 000 datagrams per second without any client
losing its connection.

## 6. Storage

**Part 1 stores nothing on disk.** Everything the server needs lives in memory for the duration
of a game, and the data structures were picked for their bounds:

| Data | Structure | Why |
| --- | --- | --- |
| Sessions | fixed array of 4 optional slots, slot index = player id | at most 4 players: no allocation, the lowest free id comes for free, pointers stay valid |
| Received packets | bounded `std::deque` (4096) | constant memory under flood |
| Entity states known by the clients | `std::unordered_map` from entity id | spawn and destroy detection in O(1) per entity |
| ECS components | sparse arrays indexed by entity id (see the engine study) | |

When persistent data arrives (high scores, server configuration, part 2 lobbies):

| Option | Fits | Does not fit |
| --- | --- | --- |
| JSON or TOML file | configuration, level data: human-editable, versioned in git | concurrent writes, queries |
| **SQLite** (recommended for scores) | a single file, transactions, no server to run, through CPM | many servers writing at once |
| PostgreSQL / Redis | shared state between several server machines | a separate service to deploy and secure, far beyond our needs |

## 7. Security

### Threat model

The server is reachable by anyone on the network and receives datagrams from strangers. Clients
are untrusted: a player may run a modified client. The network may lose, duplicate, reorder or
(on a hostile path) forge datagrams.

### What we do

| Risk | Measure |
| --- | --- |
| Crash or memory corruption from malformed input | every field is validated before use, nothing is read outside the datagram, nothing is allocated from an unchecked count (RFC section 9); fuzzed in the tests |
| Memory exhaustion (flood) | 1200-byte datagrams, 4096 queued packets, 8 packets per client per tick, 4 sessions: memory stayed at 4.5 MB under 494 000 random datagrams |
| One bad packet or bug stopping the server | exceptions caught in the network thread and around every tick; a crashed client is dropped after 3 s |
| Cheating clients | the server is the only authority: a client sends key states, never positions, hits or scores, so a modified client can only press keys faster than a human (and fire is limited by a server-side cooldown) |
| Supply chain | dependencies pinned to exact versions through CPM; the CPM script itself is checked against its SHA256 |

### What we do not do yet

| Gap | Consequence | Fix |
| --- | --- | --- |
| No authentication | anyone can join | accounts or a lobby token (part 2) |
| No encryption | traffic can be read and forged on the path | DTLS, or libsodium authenticated encryption per session |
| Source addresses can be spoofed | a third party could send INPUT or DISCONNECT for a player, or make the server stream to a victim for 3 s | a random session token in ACCEPT, required in every later packet |
| Git tags can be moved by a dependency's authors | a pinned tag could change content | pin dependencies by commit hash and check archive hashes |

These gaps are acceptable for a LAN prototype and are listed in the RFC's security section so
that a public deployment does not inherit them unknowingly.

### Weak points of each technology

| Technology | Typical weakness | How we handle it |
| --- | --- | --- |
| C++ | no memory safety: reading past a buffer, integer overflow on a size, use after free | `BinaryReader` checks every read against the datagram end; sizes are checked before any allocation; buffers are owned by RAII types, no raw `new` |
| UDP | no connection, so source addresses can be forged; no confidentiality; can be used to amplify traffic towards a victim | only CONNECT is accepted from an unknown address, and a stranger only ever gets ACCEPT or REJECT back; a forged CONNECT can still make the server stream to a victim for 3 s, and there is no encryption: both are listed above as known gaps |
| Asio | the library is safe, the danger is in the handlers we write: buffer lifetime, errors ignored, two threads using one socket | one fixed receive buffer owned by the socket, every error code checked, the socket used by the network thread only |
| SFML | image, font and sound parsers can be attacked with crafted files | the client only loads the files shipped in `assets/`; the server does not link SFML |
| Dependencies in general | a vulnerable or tampered version | exact versions through CPM, the CPM script checked by SHA256 |

### Monitoring over time

Security is not only what the code does on day one; it is also noticing problems later.

| What we watch | How | Status |
| --- | --- | --- |
| hostile or broken traffic on a running server | counters of invalid, ignored (flood) and queued-out datagrams; the server logs new drops every 5 seconds | in place |
| crashed or vanished players | every join, departure and timeout is logged with the player number | in place |
| regressions in validation | the codec tests (every truncation, 100 000 random datagrams) run on every pull request; Linux builds treat warnings as errors | in place |
| vulnerabilities in Asio, SFML, Catch2 | follow the upstream releases and security advisories, bump through a reviewed pull request; GitHub's automatic dependency alerts do not read CPM declarations, so this check is manual | process |
| network health in play | expose loss, latency and bandwidth (`NetStats`) to the metrics overlay | part 2 |
