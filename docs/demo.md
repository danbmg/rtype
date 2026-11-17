# Run a demo

Every terminal starts at the repository root.

## On the server machine

### Terminal 1 — rebuild from scratch, then start the server

```sh
rm -rf build r-type_server r-type_client
export CPM_SOURCE_CACHE=$HOME/.cache/CPM
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./r-type_server -p 4242
```

### Terminal 2 — start your client

```sh
./r-type_client -h 127.0.0.1 -p 4242
```

Join first: the first player chooses the mode and the difficulty, the others join that game.

Give the other players the server address:

```sh
hostname -I | awk '{print $1}'
```

## On each other player's machine

Same network as the server, same commit as the server. Replace `192.168.1.89` with the server
address:

```sh
git clone git@github.com:EpitechPGE3-2026/G-ING-500-NCE-5-1-rtype-2.git rtype
cd rtype
export CPM_SOURCE_CACHE=$HOME/.cache/CPM
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./r-type_client -h 192.168.1.89 -p 4242
```

Up to 4 players. A client on another commit is rejected (different protocol version).


To open the Mkdocs :

python3 -m venv .venv && .venv/bin/pip install -r docs/requirements.txt
.venv/bin/mkdocs serve
