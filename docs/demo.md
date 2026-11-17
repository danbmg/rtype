# Run a demo

Both terminals start at the repository root.

## Terminal 1 — rebuild from scratch, then start the server

```sh
rm -rf build r-type_server r-type_client
export CPM_SOURCE_CACHE=$HOME/.cache/CPM
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./r-type_server -p 4242
```

## Terminal 2 — start the client

```sh
./r-type_client -h 127.0.0.1 -p 4242
```
