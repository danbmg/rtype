# Client assets

Keep this directory beside `r-type_client` and `r-type_server`, or pass `--assets` (client) and
`--content` (server). Both executables also find it in the current directory.

## Supplied and third-party files

- `r-typesheet1.gif`, `r-typesheet3.gif`, `r-typesheet5.gif`, `r-typesheet7.gif`,
  `r-typesheet8.gif`, `r-typesheet11.gif`, `r-typesheet42.gif`: unmodified files from the
  `rtypes-sprites.zip` supplied with the R-Type project (ships, missiles, explosions, Bydos,
  wavers, chargers, turrets). They are original game artwork used in this non-commercial project;
  no new ownership or general redistribution license is asserted.
- `source/T1.png`: the T1 esports team logo. `source/world_cup.png`: the League of Legends
  World Championship 2026 logo. Both are trademarks of their owners (T1, Riot Games), used here
  for a non-commercial project only; they are not covered by the project's MIT license, and they are
  kept out of the public documentation site.
- `DejaVuSans.ttf`: DejaVu Sans, with the upstream notices in `DejaVu-LICENSE.txt`.

## Generated files

Everything below is made by the scripts of `tools/` and committed, so playing needs no Python.

| Files | Made by | From |
| --- | --- | --- |
| `ships.png` | `generate_sprites.py` (Pillow) | `r-typesheet42.gif` ships in red, blue, green and a yellow recolour, with the T1 emblem in the same colour |
| `t1.png`, `trophy.png`, `trophy_big.png` | `generate_sprites.py` | the two logos of `source/` |
| `boss_gatekeeper.png`, `boss_final.png`, `boss_shot.png`, `pickups.png`, `shield.png`, `planets.png` | `generate_sprites.py` | original drawings |
| `shot.wav`, `explosion.wav`, `big_explosion.wav`, `pickup.wav`, `hit.wav`, `shield.wav`, `warning.wav`, `stage_clear.wav`, `victory.wav`, `select.wav` | `generate_sounds.py` (standard library) | synthesised |
| `music_menu.wav`, `music_stage.wav`, `music_boss.wav` | `generate_sounds.py` | original compositions, seamless loops |

```sh
python3 assets/tools/generate_sprites.py   # needs: python3 -m pip install pillow
python3 assets/tools/generate_sounds.py
```

## Game content

`levels/` holds the levels, enemies and bosses read by the server: see the
[content guide](../docs/content-guide.md).

The renderer masks the top-left colour of each supplied sheet and uses nearest-neighbour texture
filtering; generated PNG files carry their own transparency.
