# Run a demo

Every command below runs from the repository root, in its own terminal.

## 1. Build (once)

Linux:

```sh
export CPM_SOURCE_CACHE=$HOME/.cache/CPM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Windows (Visual Studio):

```sh
cmake -S . -B build
cmake --build build --config Release
```

This writes `r-type_server` and `r-type_client` at the repository root. The first build
downloads the dependencies; the next ones reuse them. Linux system packages are listed in the
[README](https://github.com/EpitechPGE3-2026/G-ING-500-NCE-5-1-rtype-2#build-requirements).

## 2. Play

```sh
./r-type_server
./r-type_client
```

In the client menu, choose **GAME MODE** (story or endless) and **DIFFICULTY**, then
**JOIN THE GAME** with Enter. Start more clients to play with up to 4 players: the first one
chooses the game, the others join it.

## 3. Show a specific moment

| To show | Server | Client |
| --- | --- | --- |
| The story from the start | `./r-type_server` | `./r-type_client --join` |
| Every level in a few minutes | `./r-type_server` | `./r-type_client --join --difficulty tutorial` |
| The first boss (level 5) | `./r-type_server --level 5` | `./r-type_client --join` |
| The final boss and the Cup (level 10) | `./r-type_server --level 10` | `./r-type_client --join --difficulty easy` |
| Endless mode | `./r-type_server` | `./r-type_client --join --mode endless` |
| A hard game | `./r-type_server` | `./r-type_client --join --difficulty hard` |

`--join` skips the menu. The client that joins first decides the mode and the difficulty, so
start it before the bots.

The **tutorial** difficulty (story only) is made for a tour of the features: nobody can lose, and
**F3** skips the current level for every player. On the final level, F3 goes to the victory
screen. The server alone decides: F3 does nothing in the other difficulties.

## 4. Fill the game with bots

Bots are headless players. With `-b smart` they aim, dodge and catch the Summoner's Cup:

```sh
./build/bin/r-type_bot -n 3 -d 600 -b smart
```

| Option | Meaning |
| --- | --- |
| `-n 3` | number of bots (more than 4 shows a refused connection) |
| `-d 600` | seconds before they leave (`0`: until Ctrl+C) |
| `-b smart` | play well; without it they fly at random |
| `-m endless` | the mode, if the bots start the game |

On Windows the bot is `build\bin\Release\r-type_bot.exe`.

A full demo, the final level with three teammates:

```sh
./r-type_server --level 10
./r-type_client --join --difficulty easy
./build/bin/r-type_bot -n 3 -d 600 -b smart
```

## 5. Play over the network

On the server machine:

```sh
./r-type_server -p 4242
```

On each player machine, with the server's address:

```sh
./r-type_client -h 192.168.1.20 -p 4242
```

## 6. Keys

| Key | In the menu | In game |
| --- | --- | --- |
| Arrows | choose a line (up/down), change it (left/right) | move |
| Space | | fire (hold) |
| Enter | join, or rebind the keys on the CONTROLS line | continue when out of lives |
| Escape | quit | back to the menu |
| F2 | | auto-fire on/off |
| F3 | | skip the level (tutorial only) |
| F1 | | larger text |
| M | | mute |
| + / − | | volume |

Every movement and fire key can be changed in the menu: CONTROLS, then Enter, then press the
new key for each action.

## 7. Change the levels

```sh
./r-type_server --check-content
```

checks the files of `assets/levels/` and lists the levels, enemies and bosses. To change them,
see [Add a level, an enemy or a boss](content-guide.md).

## 8. Run the tests

```sh
ctest --test-dir build --output-on-failure
```
