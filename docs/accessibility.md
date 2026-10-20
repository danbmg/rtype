# Accessibility

The subject asks, for each kind of disability, what we provide, both in the game and in this
documentation. This page lists what exists today, what does not, and what is planned. Nothing
below is claimed unless it is in the code.

## Summary

| Need | In the game | In this documentation |
| --- | --- | --- |
| Physical and motor | hold to fire, 5 keys only, 3 keyboard layouts, no timed menus | full keyboard navigation, search shortcuts |
| Visual | players told apart without colour, larger text, scalable window, high contrast | light and dark themes, browser zoom, every diagram explained in text |
| Hearing | every sound has a visual equivalent, mute and volume | no audio or video content |
| Mental and cognitive | controls always on screen, plain error messages, simple rules | plain English, glossary, "where to start" by goal, step-by-step guides |

## Physical and motor

**In the game**

- **Hold to fire.** Keeping Space down fires continuously; the server applies the 0.25 s
  cooldown. No rapid tapping is needed, a common source of strain in shoot'em ups.
- **Five keys in total**: four directions and fire. No key combination, no double tap.
- **Three keyboard layouts**, switched with Tab: arrows (right hand), WASD and ZQSD (left hand,
  QWERTY or AZERTY). The current layout is shown in the footer.
- **No timed interaction outside the game itself.** The start and game over screens wait for
  Enter; reconnecting is always possible.
- **No stuck keys.** When the window loses focus, every key is released.

**Not available yet:** free key rebinding, gamepads (and with them adaptive controllers), a
toggle mode for fire, a slower game speed.

**In this documentation:** every page is reachable with the keyboard alone (Tab, Enter); the
theme provides a "skip to content" link; `s` or `/` opens the search, `n` and `p` go to the next
and previous page.

## Visual

**In the game**

- **Players do not depend on colour.** Each ship carries a `P1`–`P4` label above it and a
  coloured bar below it; the HUD marks your own slot with `YOU`. Colour is an extra cue, never the
  only one.
- **Larger text:** F1 enlarges the HUD, the notices and the controls footer.
- **Scalable window:** the whole game scales with the window, keeping its proportions, and pixel
  art stays sharp (no blur when enlarged).
- **Contrast:** light text on dark panels for every message and the HUD.

**Not available yet:** a colour-blind palette selector, a high-contrast sprite mode, menus read
aloud by a screen reader.

**In this documentation:** light and dark themes follow the system setting and can be switched
by hand; text grows with the browser zoom and the layout adapts to narrow screens; every diagram
is described by the text around it, so nothing is only in a picture; tables have header rows; no
information is given by colour alone.

## Hearing

**In the game**

- **Every sound has a visual equivalent:** a shot is a missile on screen, a destruction is an
  explosion animation. No gameplay information is sound-only.
- **Events are written:** "Player 2 disconnected (timeout)", "Connection lost", "the server is
  full" appear as text.
- **Sound control:** M mutes, + and − change the volume by steps of 10 %, `--mute` starts the
  client silent.

**Not available yet:** separate music and effects volumes (there is no music yet). The game has
no speech, so there is nothing to subtitle.

**In this documentation:** there is no audio or video content.

## Mental and cognitive

**In the game**

- **Controls always visible:** the start screen lists them, and the footer shows the current
  layout, fire key, volume, text size and how to leave, at all times.
- **Simple rules:** move, shoot, avoid. One screen, no menu tree.
- **Errors in plain words**, on screen: "no answer from the server", "the server speaks another
  protocol version", "connection lost: the server stopped answering".
- **Same keys everywhere:** Enter joins or retries, Escape leaves, in every situation.

**Limits:** notices disappear after 4 seconds; difficulty cannot be changed; settings are not
saved between two launches.

**In this documentation:** short sentences in plain English; every page starts with what it
covers; the [home page](index.md#where-to-start) sends each reader to the right page for their
goal; [Getting started](getting-started.md) goes step by step; technical terms are defined in the
[Glossary](glossary.md).

## Planned

The graphical client roadmap adds, in priority order: free key rebinding, a colour-blind mode,
separate volumes, settings kept between launches, then difficulty levels with the part 2 game
modes.

These ideas come from games known for their accessibility: Celeste's Assist Mode (slower game
speed, extra help, chosen by the player), The Last of Us Part II (full remapping, high contrast,
dozens of options) and the colour-blind modes found in most online shooters.
