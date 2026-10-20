# Client assets

- `r-typesheet1.gif`, `r-typesheet3.gif`, `r-typesheet5.gif`: unmodified files from the
  `rtypes-sprites.zip` supplied with the R-Type project. They are original game artwork used for
  the educational project; no new ownership or general redistribution license is asserted.
- `DejaVuSans.ttf`: DejaVu Sans, with the upstream notices in `DejaVu-LICENSE.txt`.
- `shot.wav`, `explosion.wav`: original synthesized placeholder effects for this client.
  Regenerate with `python3 assets/generate_sounds.py`; Python is not needed to play.

Keep this directory beside `r-type_client`, or pass `--assets /path/to/assets`.
The renderer masks the top-left colour of each sheet and uses nearest-neighbour texture filtering.
