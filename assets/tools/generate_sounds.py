"""Regenerate the sound effects and music of the client, using only Python's standard library.

    python3 assets/tools/generate_sounds.py

Every sound is synthesised here (square, triangle and noise voices): they are original, and
the WAV files are committed, so neither building nor playing needs Python. The three music
tracks loop seamlessly: their length is a whole number of bars.
"""

import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
DESTINATION = Path(__file__).resolve().parent.parent


def save(name, samples, gain=0.9):
    peak = max(1e-9, max(abs(value) for value in samples))
    scale = gain / peak if peak > gain else 1.0
    data = bytearray()
    for value in samples:
        data.extend(struct.pack("<h", int(max(-1.0, min(1.0, value * scale)) * 32000)))
    with wave.open(str(DESTINATION / name), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(data)


def write(name, duration, sample):
    """The original one-shot effects: a sample function with a quadratic fade out."""
    count = int(RATE * duration)
    samples = []
    for index in range(count):
        envelope = (1.0 - index / count) ** 2
        samples.append(0.375 * envelope * sample(index / RATE))
    save(name, samples, gain=1.0)


def frequency(note):
    """MIDI note number to Hz (69 = A4 = 440 Hz)."""
    return 440.0 * 2 ** ((note - 69) / 12)


def square(phase, duty=0.5):
    return 1.0 if (phase % 1.0) < duty else -1.0


def triangle(phase):
    return 4.0 * abs((phase % 1.0) - 0.5) - 1.0


class Track:
    """A buffer that notes and drums are mixed into."""

    def __init__(self, seconds):
        self.samples = [0.0] * int(RATE * seconds)

    def add(self, start, duration, voice, volume, attack=0.005, release=0.05):
        first = int(start * RATE)
        count = int(duration * RATE)
        for index in range(count):
            position = first + index
            if position >= len(self.samples):
                break
            t = index / RATE
            envelope = min(1.0, t / attack) if attack > 0 else 1.0
            remaining = duration - t
            if remaining < release:
                envelope *= max(0.0, remaining / release)
            self.samples[position] += volume * envelope * voice(t)

    def note(self, start, duration, note, volume, shape="square", duty=0.5):
        hz = frequency(note)
        if shape == "triangle":
            self.add(start, duration, lambda t: triangle(hz * t), volume)
        else:
            self.add(start, duration, lambda t: square(hz * t, duty), volume)

    def kick(self, start, volume=0.9):
        def voice(t):
            return math.sin(2 * math.pi * (150 * t - 110 * t * t / 0.15)) * math.exp(-t * 18)
        self.add(start, 0.15, voice, volume, attack=0.0, release=0.01)

    def noise(self, start, duration, volume, decay, seed):
        rng = random.Random(seed)
        state = [0.0]

        def voice(t):
            state[0] = 0.6 * state[0] + 0.4 * rng.uniform(-1, 1)
            return state[0] * math.exp(-t * decay)
        self.add(start, duration, voice, volume, attack=0.0, release=0.01)


def song(name, bpm, bars, chords, melody, bass_shape, lead_duty, drums, lead_volume=0.22):
    """chords: one root note per bar. melody: 16 sixteenth-note steps per bar, None for rest."""
    step = 60.0 / bpm / 4
    track = Track(step * 16 * bars)
    for bar in range(bars):
        root = chords[bar % len(chords)]
        for eighth in range(8):
            start = (bar * 16 + eighth * 2) * step
            note = root - 24 + (12 if eighth % 2 else 0)
            track.note(start, step * 1.8, note, 0.32, shape=bass_shape)
        line = melody[bar % len(melody)]
        for index, note in enumerate(line):
            if note is not None:
                length = 1
                while index + length < 16 and line[index + length] == "-":
                    length += 1
                if note != "-":
                    track.note((bar * 16 + index) * step, step * length * 0.95, note, lead_volume,
                               duty=lead_duty)
        for index in range(16):
            start = (bar * 16 + index) * step
            if index in drums["kick"]:
                track.kick(start)
            if index in drums["snare"]:
                track.noise(start, 0.18, 0.45, 18, bar * 16 + index)
            if index in drums["hat"]:
                track.noise(start, 0.04, 0.18, 80, 1000 + bar * 16 + index)
    save(name, track.samples, gain=0.85)


def effect(name, seconds, build):
    track = Track(seconds)
    build(track)
    save(name, track.samples)


R = None   # rest
H = "-"    # hold the previous note

STAGE_MELODY = [
    [69, H, 72, H, 76, H, 74, 72, 74, H, H, 69, R, 72, 74, 76],
    [77, H, 76, H, 74, H, 72, H, 69, H, H, H, R, 65, 69, 72],
    [72, H, 76, H, 79, H, 77, 76, 74, H, 72, H, 76, H, H, H],
    [74, H, 71, H, 67, H, 71, 74, 79, H, H, H, 76, H, 74, H],
]
BOSS_MELODY = [
    [64, 64, R, 64, 67, R, 64, R, 71, H, 69, H, 67, H, 66, H],
    [64, 64, R, 64, 67, R, 72, R, 71, H, 67, H, 64, H, R, R],
    [66, 66, R, 66, 69, R, 66, R, 74, H, 72, H, 71, H, 69, H],
    [71, H, H, H, 70, H, H, H, 71, R, 71, R, 75, H, H, H],
]
MENU_MELODY = [
    [72, H, H, 76, H, H, 79, H, H, H, 76, H, 74, H, H, H],
    [72, H, H, 69, H, H, 72, H, H, H, 76, H, H, H, R, R],
    [77, H, H, 76, H, H, 72, H, H, H, 69, H, 72, H, H, H],
    [74, H, H, 71, H, H, 67, H, 71, H, 74, H, 79, H, H, H],
]

if __name__ == "__main__":
    # Original placeholder effects, unchanged.
    write("shot.wav", 0.12, lambda t: math.sin(2 * math.pi * (1400 * t - 4000 * t * t)))
    noise = random.Random(42)
    write("explosion.wav", 0.30,
          lambda t: 0.7 * noise.uniform(-1, 1) + 0.3 * math.sin(2 * math.pi * 80 * t))

    def big_explosion(track):
        track.noise(0.0, 1.4, 1.0, 2.5, 7)
        track.add(0.0, 1.4, lambda t: math.sin(2 * math.pi * (55 - 20 * t) * t) * math.exp(-t * 2), 0.8)
        track.noise(0.25, 0.8, 0.6, 4, 8)
    effect("big_explosion.wav", 1.5, big_explosion)

    def pickup(track):
        for index, note in enumerate((72, 76, 79, 84)):
            track.note(index * 0.05, 0.07, note, 0.5, duty=0.25)
    effect("pickup.wav", 0.3, pickup)

    def hit(track):
        track.add(0.0, 0.22, lambda t: square(110 * t - 120 * t * t) * math.exp(-t * 10), 0.6)
        track.noise(0.0, 0.15, 0.4, 20, 3)
    effect("hit.wav", 0.25, hit)

    def shield(track):
        track.add(0.0, 0.3, lambda t: math.sin(2 * math.pi * 1800 * t) * math.exp(-t * 12), 0.6)
        track.add(0.0, 0.3, lambda t: math.sin(2 * math.pi * 2700 * t) * math.exp(-t * 16), 0.3)
    effect("shield.wav", 0.32, shield)

    def warning(track):
        for index in range(4):
            note = 81 if index % 2 == 0 else 76
            track.note(index * 0.4, 0.38, note, 0.45, duty=0.5)
            track.note(index * 0.4, 0.38, note - 12, 0.25, shape="triangle")
    effect("warning.wav", 1.7, warning)

    def stage_clear(track):
        for index, note in enumerate((67, 72, 76, 79)):
            track.note(index * 0.11, 0.1, note, 0.4, duty=0.25)
        for note in (72, 76, 79, 84):
            track.note(0.45, 0.7, note, 0.18, duty=0.5)
    effect("stage_clear.wav", 1.25, stage_clear)

    def victory(track):
        fanfare = [(0.0, 0.15, 72), (0.16, 0.15, 72), (0.32, 0.15, 72), (0.48, 0.45, 76),
                   (0.95, 0.45, 74), (1.42, 0.3, 77), (1.74, 0.3, 76), (2.06, 1.0, 79)]
        for start, length, note in fanfare:
            track.note(start, length, note, 0.4, duty=0.5)
            track.note(start, length, note - 24, 0.25, shape="triangle")
        for note in (72, 76, 79):
            track.note(2.06, 1.0, note - 12, 0.15, duty=0.25)
        for beat in range(6):
            track.kick(beat * 0.48)
    effect("victory.wav", 3.2, victory)

    def select(track):
        track.note(0.0, 0.05, 84, 0.4, duty=0.25)
    effect("select.wav", 0.06, select)

    song("music_stage.wav", 140, 8, [57, 53, 48, 55], STAGE_MELODY, "triangle", 0.25,
         {"kick": {0, 8}, "snare": {4, 12}, "hat": {0, 2, 4, 6, 8, 10, 12, 14}})
    song("music_boss.wav", 168, 8, [52, 48, 50, 47], BOSS_MELODY, "square", 0.5,
         {"kick": {0, 4, 8, 12}, "snare": {4, 12}, "hat": set(range(16))}, lead_volume=0.2)
    song("music_menu.wav", 96, 4, [48, 45, 41, 43], MENU_MELODY, "triangle", 0.125,
         {"kick": {0}, "snare": set(), "hat": {4, 12}}, lead_volume=0.18)
