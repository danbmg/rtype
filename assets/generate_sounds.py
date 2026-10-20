"""Regenerate the two original placeholder effects using only Python's standard library."""

import math
from pathlib import Path
import random
import struct
import wave

RATE = 22050
DESTINATION = Path(__file__).parent


def write(name, duration, sample):
    count = int(RATE * duration)
    data = bytearray()
    for index in range(count):
        t = index / RATE
        envelope = (1.0 - index / count) ** 2
        value = int(12000 * envelope * sample(t))
        data.extend(struct.pack("<h", value))
    with wave.open(str(DESTINATION / name), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(data)


if __name__ == "__main__":
    write("shot.wav", 0.12, lambda t: math.sin(2 * math.pi * (1400 * t - 4000 * t * t)))
    noise = random.Random(42)
    write("explosion.wav", 0.30,
          lambda t: 0.7 * noise.uniform(-1, 1) + 0.3 * math.sin(2 * math.pi * 80 * t))
