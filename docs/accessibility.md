# Accessibility

For each kind of disability, this page lists what the game and this documentation provide
today, what they do not, and what is planned. Nothing below is claimed unless it is in the code.

## Summary

| Need | In the game | In this documentation |
| --- | --- | --- |
| Physical and motor | free key remapping, auto-fire, hold to fire, easy difficulty, no timed menus | full keyboard navigation, search shortcuts |
| Visual | players and power-ups told apart without colour, larger text, scalable window, high contrast | light and dark themes, browser zoom, every diagram explained in text |
| Hearing | every sound has a visual equivalent, optional sound captions, separate music and effect volumes | no audio or video content |
| Mental and cognitive | difficulty levels, title cards that explain each level, controls always on screen, plain error messages | plain English, glossary, step-by-step guides |
| Motion sensitivity | screen shake can be turned off | |

Every option is in the start menu (UP/DOWN to choose a line, LEFT/RIGHT to change it, ENTER to
confirm). They are client settings: they change nothing for the other players.

## Physical and motor

**In the game**

- **Free key remapping.** In the menu, CONTROLS > ENTER asks for a key for each action (up,
  down, left, right, fire), one after the other: any key of the keyboard can be used, letters,
  digits, arrows, Shift, Ctrl or Alt, so the game can be played with one hand, on either side of
  the keyboard, or with keys that suit a particular hand position. ESCAPE stops the remapping
  and keeps what was already bound. LEFT/RIGHT on the same line pick a ready-made layout instead:
  arrows, WASD or ZQSD.
- **Auto-fire.** The ship can fire on its own (menu, or F2 at any time in game): playing then
  needs only the movement keys. Without it, holding the fire key fires continuously; no rapid
  tapping is ever needed.
- **Easy difficulty**: 5 lives instead of 3, slower and fewer enemy shots, weaker enemies. It
  gives more time to react for players with slower motor responses.
- **Invulnerability after a hit.** A ship that loses a life cannot be hit again for 2 seconds.
- **No timed interaction outside the game itself.** The menu, game over and victory screens wait
  or loop; reconnecting is always possible.
- **No stuck keys.** When the window loses focus, every key is released.

**Not available yet:** gamepads (and with them adaptive controllers), a slower game speed.

**In this documentation:** every page is reachable with the keyboard alone (Tab, Enter); the
theme provides a "skip to content" link; `s` or `/` opens the search, `n` and `p` go to the next
and previous page.

## Visual

**In the game**

- **Players do not depend on colour.** The four ships are red, blue, green and yellow with a T1
  emblem of the same colour, and each also carries a `P1`–`P4` label above it; the HUD marks your
  own slot with `YOU`. Red and green, the pair that colour-blind players confuse most, are
  always told apart by their label.
- **Power-ups do not depend on colour:** each capsule shows a letter (`1`, `+`, `R`, `D`, `S`,
  `H`, `O`), and the HUD writes the weapon in full ("SPREAD", "SHIELD").
- **Larger text:** in the menu (TEXT SIZE) or F1 in game: the menu, the HUD, the title cards and
  the footer grow.
- **Scalable window:** the whole game scales with the window, keeping its proportions, and pixel
  art stays sharp (no blur when enlarged).
- **Contrast:** light text on dark panels for every message, the HUD and the menu; the boss
  health bar flashes white, not only red, when hit.

**Not available yet:** a colour-blind palette selector, menus read aloud by a screen reader.

**In this documentation:** light and dark themes follow the system setting and can be switched
by hand; text grows with the browser zoom and the layout adapts to narrow screens; every diagram
is described by the text around it, so nothing is only in a picture; tables have header rows; no
information is given by colour alone.

## Hearing

**In the game**

- **Every sound has a visual equivalent:** a shot is a missile on screen, a destruction is an
  explosion, a boss arrival is a flashing "WARNING" banner and a health bar, a hit makes the ship
  flash red.
- **Sound captions** (on by default, SOUND CAPTIONS in the menu): the important sounds are also
  written at the top right of the screen: "[Alarm: a boss is coming]", "[Ship hit]", "[Shield
  down]", "[Power-up collected]", "[Fanfare: stage clear]", "[Victory fanfare]".
- **Events are written:** "Player 2 disconnected (timeout)", "Connection lost", "the server is
  full" appear as text.
- **Separate volumes** for the music and the sound effects (menu, 0 to 100). M mutes everything,
  + and − change both by steps of 10, `--mute` starts the client silent.

The game has no speech, so there is nothing else to subtitle.

**In this documentation:** there is no audio or video content.

## Mental and cognitive

**In the game**

- **Difficulty levels** (easy, normal, hard), chosen in the menu by the player who starts the
  game.
- **Each level explains itself:** a title card shows the level number, its name and a short
  story that announces what is new ("chargers dive straight at you: keep moving").
- **Controls always visible:** the menu explains the selected line, and the footer shows the
  mode, the level, the wave, the fire key and how to leave, at all times.
- **Simple rules:** move, shoot, avoid, catch capsules. One menu screen.
- **Errors in plain words**, on screen: "no answer from the server", "the server speaks another
  protocol version", "connection lost: the server stopped answering".
- **Same keys everywhere:** Enter joins, continues or confirms, Escape leaves, in every situation.

**Limits:** notices disappear after 4 seconds; settings are not saved between two launches.

**In this documentation:** short sentences in plain English; every page starts with what it
covers; the [home page](index.md#where-to-start) sends each reader to the right page for their
goal; [Getting started](getting-started.md) goes step by step; technical terms are defined in the
[Glossary](glossary.md).

## Motion sensitivity

Boss explosions and hits shake the screen. SCREEN SHAKE in the menu turns it off; nothing else
moves the whole screen.

## Planned

Settings kept between launches, gamepad support, a slower game speed option and a colour-blind
palette selector.

These ideas come from games known for their accessibility: Celeste's Assist Mode (slower game
speed, extra help, chosen by the player), The Last of Us Part II (full remapping, high contrast,
dozens of options) and the colour-blind modes found in most online shooters.
