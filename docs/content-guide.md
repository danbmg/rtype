# Content guide: add a level, an enemy or a boss

Levels, enemies and bosses are JSON files in `assets/levels/`. The server reads them when it
starts: changing the game needs **no C++ and no rebuild**, only a server restart.

```text
assets/levels/
├── enemies.json      every enemy type and every boss
├── story.json        the story: its name and its levels, in order
├── story/*.json      one file per story level
└── endless.json      what endless mode generates its levels from
```

The files accept `//` comments. Distances are in world units (the screen is 800 × 600, x to the
right, y downwards) and times in seconds.

## Check your work

```sh
./r-type_server --check-content
```

It loads every file, prints a summary and exits: `0` when everything is valid. Otherwise it
names the file and the field that is wrong, and the server refuses to start with that content:

```text
ERROR content error: 03-swiss-round-one.json: waves[2]: "enemy" names an enemy that is not in enemies.json: "chargr"
ERROR content error: enemies.json: enemies[1] "heavy-drone": unknown field "sped" (allowed: name, visual, ...)
ERROR content error: 02-asteroid-lanes.json: "title" is too long (61 characters, at most 48)
```

To test a level without playing the ones before it, start the server on it:

```sh
./r-type_server --level 5            # every game starts at level 5
./r-type_server --content my-levels  # another content directory
```

The automated tests (`ctest`) also load the shipped files: a broken level fails the build.

## Tutorial: add a level to the story

1. Create `assets/levels/story/11-victory-lap.json`:

    ```json
    // A level after the Cup is back. Waves run in order.
    {
      "title": "Victory Lap",
      "story": "The Cup is home, but the Bydos want a rematch.\nShow them who the champions are.",
      "theme": 2,
      "intro": 4,
      "waves": [
        { "enemy": "drone", "count": 10, "interval": 0.4, "formation": "v" },
        { "enemy": "charger", "count": 8, "interval": 0.5 },
        { "boss": "gatekeeper" }
      ]
    }
    ```

2. Add it to the list in `assets/levels/story.json`, where it should be played:

    ```json
    "levels": [ "...", "story/10-summoners-cup.json", "story/11-victory-lap.json" ]
    ```

3. Run `./r-type_server --check-content`, then play it with `./r-type_server --level 11`.

The last level of the list is the final one. The story is won when its last wave is cleared
and, if its boss carries the Cup (`"trophy": true`), when the Cup is caught.

### Level fields

| Field | Required | Default | Meaning |
| --- | --- | --- | --- |
| `title` | yes | | shown on the title card and in the footer, at most 48 characters |
| `story` | no | empty | text of the title card, at most 512 characters, `\n` for a new line |
| `theme` | no | 0 | background palette: 0 blue, 1 amber, 2 green, 3 red, 4 purple |
| `intro` | no | 4 | seconds of title card before the first wave (0 to 15) |
| `waveDelay` | no | 2.5 | seconds between two waves |
| `nextLevelDelay` | no | 4 | seconds of "stage clear" before the next level |
| `waves` | yes | | 1 to 30 waves, played in order |

Texts use plain ASCII: no accents and no emoji (the network protocol carries ASCII only).

### Wave fields

A wave is either regular enemies or a boss.

| Field | Default | Meaning |
| --- | --- | --- |
| `enemy` | | an enemy name from `enemies.json` |
| `count` | 5 | how many enemies, 1 to 50 |
| `interval` | 1.0 | seconds between two spawns, 0.1 to 10 |
| `formation` | `random` | `random`, `line` (same height), `v`, `stairs` (top to bottom) |
| `speed` | 1.0 | multiplies the enemy's speed, 0.2 to 5 |
| `drop` | the enemy's | chance that each enemy drops a power-up, 0 to 1 |
| `boss` | | a boss name from `enemies.json`: the wave is that boss alone |

A wave ends when all its enemies are destroyed or have left the screen; a boss stays until it
is destroyed.

## Tutorial: add an enemy

Add an entry to the `enemies` list of `enemies.json`:

```json
{ "name": "sniper", "visual": "turret", "health": 3, "points": 300,
  "movement": "sine", "speed": 50, "amplitude": 120, "frequency": 0.3,
  "fireRate": 0.8, "burst": 1, "shotSpeed": 400, "drop": 0.25 }
```

It can now be used in any wave (`{ "enemy": "sniper", "count": 4 }`) and in endless mode by
adding its name to the `enemies` list of `endless.json`.

| Field | Default | Meaning |
| --- | --- | --- |
| `name` | | unique, lowercase letters, digits and `-` |
| `visual` | `bydos` | the sprite: `bydos`, `waver`, `charger` or `turret` |
| `health` | 1 | hits to destroy it (before difficulty), 1 to 1000 |
| `points` | 100 | score for the player who destroys it |
| `size` | `[32, 24]` | hitbox `[width, height]`; keep the size of its visual (below) |
| `movement` | `straight` | `straight`, `sine`, `zigzag`, `chase` or `turret` |
| `speed` | 80 | leftward speed, 10 to 600 |
| `amplitude` | 60 | height of the sine wave |
| `frequency` | 0.5 | sine cycles, or zigzag turns, per second |
| `fireRate` | 0 | volleys per second; 0 never shoots |
| `burst` | 1 | aimed shots per volley, 1 to 8 |
| `shotSpeed` | 220 | speed of its shots |
| `drop` | 0.15 | chance of dropping a power-up when destroyed |

| Visual | Drawn size |
| --- | --- |
| `bydos` | 32 × 24 |
| `waver` | 30 × 30 |
| `charger` | 28 × 28 |
| `turret` | 32 × 32 |

| Movement | Behaviour |
| --- | --- |
| `straight` | flies right to left |
| `sine` | right to left on a wave of `amplitude` around its spawn height |
| `zigzag` | right to left, changing vertical direction `frequency` times per second |
| `chase` | right to left, steering towards the closest ship |
| `turret` | drifts slowly; give it a `fireRate` and a `burst` |

## Tutorial: add a boss

Add an entry to the `bosses` list of `enemies.json`, then a wave `{ "boss": "<name>" }`:

```json
{ "name": "rift-warden", "visual": "boss", "health": 220, "points": 8000,
  "phases": [
    { "below": 1.0, "attacks": ["wall", "aimed"], "interval": 1.4, "bullets": 5 },
    { "below": 0.5, "attacks": ["ring", "summon"], "interval": 1.0, "bullets": 14,
      "summon": "zigzag", "moveSpeed": 120 }
  ] }
```

| Field | Default | Meaning |
| --- | --- | --- |
| `name` | | unique, lowercase letters, digits and `-` |
| `visual` | `boss` | `boss` (120 × 96) or `final-boss` (150 × 120) |
| `health` | 60 | before difficulty, 1 to 20000 |
| `points` | 5000 | for the player who destroys it |
| `size` | the visual's | hitbox `[width, height]` |
| `trophy` | `false` | drops the Summoner's Cup: catching it ends the story |
| `phases` | | 1 to 8 phases, the first with `"below": 1.0`, then decreasing |

A phase is active while the boss health ratio is at or below its `below` value. The boss cycles
through the phase's attacks, one every `interval` seconds.

| Phase field | Default | Meaning |
| --- | --- | --- |
| `below` | 1.0 | health ratio where the phase starts |
| `attacks` | | 1 to 8 of `aimed`, `spread`, `ring`, `wall`, `summon` |
| `interval` | 1.5 | seconds between two attacks |
| `bullets` | 5 | shots per attack (`aimed` uses at most 7) |
| `shotSpeed` | 240 | speed of its shots |
| `moveSpeed` | 60 | speed of its vertical sweep |
| `summon` | | the enemy that `summon` brings, two at a time |

| Attack | Pattern |
| --- | --- |
| `aimed` | a tight volley at one of the ships |
| `spread` | a fan of shots to the left |
| `ring` | shots in every direction, rotating over time |
| `wall` | a column of shots across the screen with a gap to fly through |
| `summon` | two enemies of type `summon` beside the boss (at most 24 enemies on screen) |

## Endless mode

`endless.json` tells the generator what to use:

| Field | Default | Meaning |
| --- | --- | --- |
| `title` | `Endless - Round` | levels are called "`title` N" |
| `enemies` | | enemy names, introduced in this order as the rounds go up |
| `bosses` | `[]` | named bosses, used in turn |
| `bossEvery` | 2 | a named boss closes every `bossEvery` rounds; the others close with a generic boss |
| `wavesPerLevel` | 5 | waves per round, the last one being the boss |
| `intro` | 3 | seconds of title card per round |

Enemy counts, health, speed and fire rate grow with the round number, up to round 100.

## What needs code

Content files cover numbers, combinations and texts. These still need C++ (and a new protocol
entity kind for anything new on screen):

- a new **visual** (sprite) for an enemy or a boss: `Visual` in
  `common/components/Behavior.hpp`, the entity kind in `common/protocol/Packets.hpp` and the RFC,
  and the drawing in `client/Scene.cpp`;
- a new **movement pattern** or **boss attack**: `game/EnemyBehaviors.cpp`, then the name in
  `game/Content.cpp`;
- a new **power-up** effect: `game/EquipmentSystems.cpp`.
