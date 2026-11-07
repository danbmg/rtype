# Graphics and audio choices

The client needs a window, 2D sprites, text, keyboard input and short sound effects.
It does not need a scene editor, a 3D renderer or a physics library.

| Candidate | Relevant properties | Decision for this project |
| --- | --- | --- |
| SFML 3 | C++ APIs for window/input, sprites/text and sound buffers | Already pinned by the team; a small implementation fits our C++ interfaces |
| SDL 3 | C API usable from C++; low-level graphics, input and audio, with image/font/mixer satellite libraries | Suitable, but requires a different wrapper and dependency selection |
| raylib | C library with drawing, fonts, audio and many examples | Suitable for a prototype; switching brings no demonstrated benefit to the existing SFML build |

Sources: [SFML 3 tutorials](https://www.sfml-dev.org/tutorials/3.0/),
[SDL 3 documentation](https://wiki.libsdl.org/SDL3/FrontPage),
[raylib features](https://www.raylib.com/).
This is a comparison of fit and integration cost, not a measured performance benchmark.

## Resource ownership and drawing

Textures are loaded once into an id-keyed map owned by the renderer. Per-frame sprites reference
those textures, which outlive each draw call. Short-lived sprites keep the implementation easy
to inspect. Batching or cached drawable objects could be considered after profiling.

GIF sheets from the supplied archive are loaded directly. Their top-left background colour is
masked on load, and individual frames are selected with rectangles. This avoids adding an image
conversion step to the build. The font and WAV files are shipped with the application.

Sound buffers are cached by id. Each playback owns a sound instance referencing its buffer.
Stopped instances are discarded, and simultaneous playback is capped at 16 effects. Events
trigger playback once; frame-by-frame rendering does not trigger it repeatedly. Two small
original synthesized WAV effects cover shooting and destruction. Music streaming is unnecessary
for this prototype.

## Separation and constraints

Public renderer, input and audio interfaces use standard C++ types; SFML-specific calls live in
their implementations. The application constructs those implementations and uses the interfaces.
The server does not link these targets. The client scene translates network entities into draw
commands without running gameplay rules.

Colour is supplemented by player labels, audio by visible effects, and keyboard control by
three layout presets. Resizing, larger HUD text, mute and volume controls provide a small,
explainable accessibility baseline. Settings are session-local; there is no claim of exhaustive
accessibility coverage.
