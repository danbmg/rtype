"""Regenerate the derived sprites of the client from the source art.

Needs Pillow (python3 -m pip install pillow), only to regenerate: the PNG files it writes are
committed, so neither building nor playing needs Python.

    python3 assets/tools/generate_sprites.py

Inputs (in assets/): r-typesheet42.gif (supplied R-Type ships), source/T1.png (team emblem),
source/world_cup.png (Worlds trophy logo).
Outputs (in assets/): ships.png, t1.png, trophy.png, trophy_big.png, pickups.png,
boss_gatekeeper.png, boss_final.png, boss_shot.png, shield.png, planets.png.
The two bosses, the pickups and the planets are original art drawn by this script.
"""

import colorsys
import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ASSETS = Path(__file__).resolve().parent.parent

# Player colours, in player id order: red, blue, green, yellow.
PLAYER_COLOURS = [(235, 45, 60), (60, 140, 255), (60, 220, 100), (255, 215, 40)]
SHIP_W, SHIP_H = 33, 17


def transparent_black(image):
    image = image.convert("RGBA")
    pixels = image.load()
    for y in range(image.size[1]):
        for x in range(image.size[0]):
            if pixels[x, y][:3] == (0, 0, 0):
                pixels[x, y] = (0, 0, 0, 0)
    return image


def recolour_canopy(frame, hue):
    """The supplied sheet has no yellow ship: turn the cyan canopy yellow."""
    frame = frame.copy()
    pixels = frame.load()
    for y in range(frame.size[1]):
        for x in range(frame.size[0]):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            if s > 0.35 and 0.40 < h < 0.75:
                r2, g2, b2 = colorsys.hsv_to_rgb(hue, min(1.0, s * 1.05), min(1.0, v * 1.1))
                pixels[x, y] = (int(r2 * 255), int(g2 * 255), int(b2 * 255), a)
    return frame


def t1_mask():
    """Alpha mask of the T1 logo (red on white), cropped to the logo."""
    logo = Image.open(ASSETS / "source" / "T1.png").convert("RGB")
    mask = Image.new("L", logo.size)
    source, target = logo.load(), mask.load()
    for y in range(logo.size[1]):
        for x in range(logo.size[0]):
            r, g, b = source[x, y]
            target[x, y] = max(0, min(255, int((255 - min(r, g, b)) * 255 / 223)))
    return mask.crop(mask.getbbox())


def tinted(mask, colour):
    image = Image.new("RGBA", mask.size, colour + (0,))
    image.putalpha(mask)
    return image


def outlined_emblem(mask, width, colour):
    """The emblem in a player colour with a dark outline, readable on the grey hull."""
    height = max(1, round(width * mask.size[1] / mask.size[0]))
    small = mask.resize((width, height), Image.LANCZOS)
    result = Image.new("RGBA", (width + 2, height + 2), (0, 0, 0, 0))
    dark = tinted(small, (10, 10, 20))
    for dx in range(3):
        for dy in range(3):
            result.alpha_composite(dark, (dx, dy))
    result.alpha_composite(tinted(small, colour), (1, 1))
    return result


def make_ships(mask):
    sheet = transparent_black(Image.open(ASSETS / "r-typesheet42.gif"))

    def row(index):
        return [sheet.crop((c * 33, index * 17, c * 33 + SHIP_W, index * 17 + SHIP_H))
                for c in range(5)]

    # Rows of the supplied sheet: 0 cyan, 1 pink, 2 green, 3 red, 4 blue.
    rows = [row(3), row(4), row(2), [recolour_canopy(frame, 0.14) for frame in row(0)]]
    out = Image.new("RGBA", (SHIP_W * 5, SHIP_H * 4), (0, 0, 0, 0))
    for player, frames in enumerate(rows):
        emblem = outlined_emblem(mask, 14, PLAYER_COLOURS[player])
        for column, frame in enumerate(frames):
            ship = frame.copy()
            ship.alpha_composite(emblem, (1, 5))
            out.alpha_composite(ship, (column * SHIP_W, player * SHIP_H))
    out.save(ASSETS / "ships.png")


def make_t1(mask):
    width = 120
    height = round(width * mask.size[1] / mask.size[0])
    tinted(mask.resize((width, height), Image.LANCZOS), (255, 255, 255)).save(ASSETS / "t1.png")


def make_trophy():
    """The Worlds logo is white lines on black: the black outside becomes transparent, the
    shield stays dark navy, the lines become gold."""
    logo = Image.open(ASSETS / "source" / "world_cup.png").convert("RGB")
    size = logo.size
    marker = (255, 0, 255)
    flood = logo.copy()
    for corner in [(0, 0), (size[0] - 1, 0), (0, size[1] - 1), (size[0] - 1, size[1] - 1)]:
        if flood.getpixel(corner) != marker:
            ImageDraw.floodfill(flood, corner, marker, thresh=60)
    result = Image.new("RGBA", size, (0, 0, 0, 0))
    source, background, target = logo.load(), flood.load(), result.load()
    for y in range(size[1]):
        for x in range(size[0]):
            if background[x, y] == marker:
                continue
            light = sum(source[x, y]) / (3 * 255)
            shine = 0.75 + 0.25 * (1 - y / size[1])
            gold = (int(255 * shine), int(200 * shine), int(60 * shine))
            navy = (14, 20, 48)
            target[x, y] = tuple(int(navy[i] + (gold[i] - navy[i]) * light) for i in range(3)) + (255,)
    box = result.getbbox()
    result = result.crop(box)
    side = max(result.size)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.alpha_composite(result, ((side - result.size[0]) // 2, (side - result.size[1]) // 2))
    square.resize((192, 192), Image.LANCZOS).save(ASSETS / "trophy_big.png")
    square.resize((48, 48), Image.LANCZOS).save(ASSETS / "trophy.png")


def pickup_symbol(draw, kind):
    """White symbol of a bonus, drawn on a 96 x 96 canvas."""
    if kind == "life":  # a heart
        draw.ellipse((22, 26, 50, 54), fill=255)
        draw.ellipse((46, 26, 74, 54), fill=255)
        draw.polygon([(24, 46), (72, 46), (48, 74)], fill=255)
    elif kind == "heal":  # a medical cross
        draw.rectangle((39, 22, 57, 74), fill=255)
        draw.rectangle((22, 39, 74, 57), fill=255)
    elif kind == "rapid":  # a lightning bolt
        draw.polygon([(56, 16), (28, 54), (46, 54), (38, 80), (68, 40), (50, 40), (60, 16)],
                     fill=255)
    elif kind == "double":  # two parallel shots
        for y in (30, 54):
            draw.rounded_rectangle((20, y, 76, y + 12), radius=6, fill=255)
    elif kind == "spread":  # three separate shots in a fan
        for line in ((40, 36, 70, 20), (42, 48, 76, 48), (40, 60, 70, 76)):
            draw.line(line, fill=255, width=11)
        draw.ellipse((18, 42, 30, 54), fill=255)
    elif kind == "heavy":  # one big round shot with speed lines
        draw.ellipse((38, 26, 78, 66), fill=255)
        draw.rectangle((16, 36, 32, 42), fill=255)
        draw.rectangle((16, 50, 32, 56), fill=255)
    elif kind == "shield":  # a shield
        draw.polygon([(48, 18), (74, 28), (70, 54), (48, 78), (26, 54), (22, 28)], fill=255)


def make_pickups():
    """Capsules with a symbol: the bonus is told by its shape, not only by its colour."""
    icons = [  # in PickupType order
        ("life", (255, 110, 180)), ("heal", (70, 210, 90)), ("rapid", (255, 150, 40)),
        ("double", (40, 200, 230)), ("spread", (170, 100, 255)), ("heavy", (230, 60, 60)),
        ("shield", (90, 160, 255)),
    ]
    size, big = 24, 96  # drawn 4 times larger, then scaled down for smooth edges
    sheet = Image.new("RGBA", (size * len(icons), size), (0, 0, 0, 0))
    for index, (kind, colour) in enumerate(icons):
        icon = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        draw = ImageDraw.Draw(icon)
        dark = tuple(max(0, c // 3) for c in colour)
        draw.ellipse((0, 0, big - 1, big - 1), fill=dark + (255,))
        draw.ellipse((8, 8, big - 9, big - 9), fill=colour + (255,))
        shine = Image.new("RGBA", (big, big), (0, 0, 0, 0))
        ImageDraw.Draw(shine).ellipse((24, 12, 56, 32), fill=(255, 255, 255, 120))
        icon.alpha_composite(shine)
        symbol = Image.new("L", (big, big))
        pickup_symbol(ImageDraw.Draw(symbol), kind)
        icon.paste(dark + (255,), mask=symbol.filter(ImageFilter.MaxFilter(9)))
        icon.paste((255, 255, 255, 255), mask=symbol)
        sheet.alpha_composite(icon.resize((size, size), Image.LANCZOS), (index * size, 0))
    sheet.save(ASSETS / "pickups.png")


def shade(colour, factor):
    return tuple(max(0, min(255, int(c * factor))) for c in colour[:3]) + (255,)


def dragon_emblem(size, frame):
    """Round badge with a dragon head facing the players; it breathes fire on frame 1."""
    big = 100
    badge = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(badge)
    draw.ellipse((0, 0, big - 1, big - 1), fill=(25, 20, 30, 255))
    # One bat wing raised behind the head, its bones running from the shoulder to each tip.
    shoulder, tips = (60, 46), [(50, 10), (74, 6), (94, 26)]
    draw.polygon([shoulder, tips[0], (62, 22), tips[1], (80, 26), tips[2], (86, 46)],
                 fill=(120, 60, 150, 255))
    for tip in tips:
        draw.line(shoulder + tip, fill=(190, 140, 230, 255), width=3)
    # Neck curving down behind the head, with spikes along its back.
    draw.polygon([(46, 52), (64, 44), (90, 70), (72, 88)], fill=(185, 40, 45, 255))
    for t in (0.3, 0.55, 0.8):
        x, y = 64 + 26 * t, 44 + 26 * t
        draw.polygon([(x - 4, y - 4), (x + 6, y - 10), (x + 4, y + 4)], fill=(240, 225, 185, 255))
    # Long snout, lit top, open jaw.
    draw.polygon([(6, 46), (26, 38), (44, 34), (58, 40), (62, 52), (52, 62), (34, 60), (14, 56)],
                 fill=(220, 50, 50, 255))
    draw.polygon([(6, 46), (26, 38), (44, 34), (40, 42), (10, 48)], fill=(250, 110, 90, 255))
    draw.polygon([(6, 49), (36, 51), (14, 56)], fill=(50, 10, 15, 255))
    # Two horns swept back, a glowing eye, fire in the jaw on frame 1.
    for horn in ([(40, 36), (60, 12), (52, 39)], [(50, 39), (78, 30), (58, 45)]):
        draw.polygon(horn, fill=(240, 225, 185, 255))
    eye = (255, 245, 140) if frame else (255, 170, 50)
    draw.polygon([(26, 42), (38, 38), (35, 45)], fill=eye + (255,))
    if frame:
        draw.polygon([(10, 50), (34, 51), (14, 54)], fill=(255, 190, 60, 255))
        draw.polygon([(8, 50), (3, 44), (5, 51), (3, 58)], fill=(255, 140, 40, 255))
    draw.ellipse((0, 0, big - 1, big - 1), outline=(190, 150, 60, 255), width=5)
    return badge.resize((size, size), Image.LANCZOS)


def make_gatekeeper():
    """Boss 1: an armoured gate guardian with a dragon emblem and two cannons."""
    width, height = 120, 96
    frames = []
    for frame in range(2):
        image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        hull = (70, 92, 120)
        # Rear armour and fins.
        draw.polygon([(60, 4), (118, 14), (118, 82), (60, 92)], fill=shade(hull, 0.6))
        draw.polygon([(88, 0), (118, 6), (110, 20), (84, 16)], fill=shade(hull, 0.8))
        draw.polygon([(88, 96), (118, 90), (110, 76), (84, 80)], fill=shade(hull, 0.8))
        # Main body, lit from the top.
        draw.polygon([(22, 20), (78, 8), (100, 30), (100, 66), (78, 88), (22, 76), (10, 48)],
                     fill=shade(hull, 1.0))
        draw.polygon([(22, 20), (78, 8), (100, 30), (60, 34), (16, 40)], fill=shade(hull, 1.35))
        draw.polygon([(22, 76), (78, 88), (100, 66), (60, 62), (16, 56)], fill=shade(hull, 0.75))
        # Cannons pointing at the players.
        for y in (22, 66):
            draw.rectangle((0, y, 30, y + 8), fill=shade((150, 150, 160), 0.9))
            draw.rectangle((0, y, 30, y + 2), fill=shade((200, 200, 210), 1.0))
            draw.rectangle((0, y, 4, y + 8), fill=(40, 40, 50, 255))
        # Plates and rivets.
        for x in range(30, 96, 12):
            draw.line((x, 14 + (x - 30) // 6, x + 4, 82 - (x - 30) // 6), fill=shade(hull, 0.55))
            draw.point((x + 2, 26), fill=(210, 220, 235, 255))
            draw.point((x + 2, 70), fill=(210, 220, 235, 255))
        # Teal vents that glow on the second frame.
        vent = (60, 255, 220) if frame else (30, 140, 130)
        for y in (38, 46, 54):
            draw.rectangle((80, y, 96, y + 3), fill=vent + (255,))
        # The dragon emblem, where the eye used to be.
        image.alpha_composite(dragon_emblem(40, frame), (24, 28))
        frames.append(image)
    sheet = Image.new("RGBA", (width * 2, height), (0, 0, 0, 0))
    for index, image in enumerate(frames):
        sheet.alpha_composite(image, (index * width, 0))
    sheet.save(ASSETS / "boss_gatekeeper.png")


def make_final_boss(trophy):
    """Boss 2: the Cup thief, a crab-like Bydos lord holding the Summoner's Cup in its claws."""
    width, height = 150, 120
    cup = trophy.resize((34, 34), Image.LANCZOS)
    frames = []
    for frame in range(2):
        image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        draw = ImageDraw.Draw(image)
        shell = (120, 50, 150)
        # Legs behind the body.
        for i, y in enumerate((20, 44, 72, 96)):
            swing = (3 if frame else -3) * (1 if i % 2 else -1)
            draw.line((100, 60, 140, y + swing), fill=shade(shell, 0.5), width=6)
            draw.line((140, y + swing, 148, y + 10 + swing), fill=shade(shell, 0.5), width=4)
        # Carapace with highlight and dark belly.
        draw.ellipse((40, 18, 136, 102), fill=shade(shell, 0.9))
        draw.ellipse((48, 22, 128, 64), fill=shade(shell, 1.25))
        draw.chord((40, 18, 136, 102), 20, 160, fill=shade(shell, 0.6))
        for x in range(56, 128, 14):
            draw.arc((x - 10, 26, x + 10, 50), 200, 340, fill=shade(shell, 1.6), width=2)
        # Three eyes, brighter on the second frame.
        glow = (120, 255, 120) if frame else (60, 200, 80)
        for ex, ey in ((52, 50), (60, 38), (60, 62)):
            draw.ellipse((ex - 6, ey - 6, ex + 6, ey + 6), fill=(20, 10, 25, 255))
            draw.ellipse((ex - 4, ey - 4, ex + 4, ey + 4), fill=glow + (255,))
        # Arms reaching forwards to the claws.
        open_by = 4 if frame else 0
        draw.line((52, 34, 24, 26), fill=shade(shell, 0.8), width=8)
        draw.line((52, 86, 24, 94), fill=shade(shell, 0.8), width=8)
        # Claws around the Cup.
        draw.pieslice((2, 8 - open_by, 38, 44 - open_by), 120, 360, fill=shade((200, 70, 90), 1.0))
        draw.pieslice((2, 76 + open_by, 38, 112 + open_by), 0, 240, fill=shade((200, 70, 90), 1.0))
        image.alpha_composite(cup, (6, 43))
        draw.pieslice((2, 8 - open_by, 38, 44 - open_by), 150, 210, fill=shade((255, 120, 140), 1.0))
        frames.append(image)
    sheet = Image.new("RGBA", (width * 2, height), (0, 0, 0, 0))
    for index, image in enumerate(frames):
        sheet.alpha_composite(image, (index * width, 0))
    sheet.save(ASSETS / "boss_final.png")


def make_boss_shot():
    size = 12
    sheet = Image.new("RGBA", (size * 2, size), (0, 0, 0, 0))
    for frame, colours in enumerate([((255, 80, 200), (255, 220, 250)), ((255, 140, 60), (255, 240, 200))]):
        orb = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        draw = ImageDraw.Draw(orb)
        draw.ellipse((0, 0, size - 1, size - 1), fill=colours[0] + (200,))
        draw.ellipse((3, 3, size - 4, size - 4), fill=colours[1] + (255,))
        sheet.alpha_composite(orb, (frame * size, 0))
    sheet.save(ASSETS / "boss_shot.png")


def make_shield():
    """A translucent bubble drawn around a shielded ship."""
    width, height = 52, 34
    bubble = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(bubble)
    draw.ellipse((0, 0, width - 1, height - 1), fill=(90, 200, 255, 50), outline=(160, 230, 255, 220), width=2)
    draw.arc((6, 3, width - 18, height - 12), 200, 280, fill=(255, 255, 255, 200), width=2)
    bubble.save(ASSETS / "shield.png")


def make_planets():
    """Light planets: the client tints them with the colours of each level's theme."""
    rng = random.Random(7)
    sizes = [96, 64, 40]
    sheet = Image.new("RGBA", (sum(sizes) + 2 * len(sizes), max(sizes)), (0, 0, 0, 0))
    x = 0
    for index, size in enumerate(sizes):
        planet = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        pixels = planet.load()
        radius = size / 2 - 1
        for py in range(size):
            for px in range(size):
                dx, dy = px - size / 2 + 0.5, py - size / 2 + 0.5
                distance = math.hypot(dx, dy)
                if distance > radius:
                    continue
                light = max(0.15, 1 - math.hypot(dx + radius * 0.4, dy + radius * 0.4) / (radius * 1.9))
                band = 0.85 + 0.15 * math.sin(py * (0.35 + index * 0.1) + math.sin(px * 0.08) * 2)
                value = int(255 * light * band)
                pixels[px, py] = (value, value, value, 255)
        craters = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        for _ in range(size // 8):
            cx, cy, r = rng.randint(0, size), rng.randint(0, size), rng.randint(2, max(3, size // 10))
            ImageDraw.Draw(craters).ellipse((cx - r, cy - r, cx + r, cy + r), fill=(0, 0, 0, 40))
        planet.alpha_composite(craters)
        mask = Image.new("L", (size, size), 0)
        ImageDraw.Draw(mask).ellipse((1, 1, size - 2, size - 2), fill=255)
        planet.putalpha(mask.filter(ImageFilter.GaussianBlur(0.6)))
        sheet.alpha_composite(planet, (x, 0))
        x += size + 2
    sheet.save(ASSETS / "planets.png")


if __name__ == "__main__":
    emblem = t1_mask()
    make_ships(emblem)
    make_t1(emblem)
    make_trophy()
    make_pickups()
    make_gatekeeper()
    make_final_boss(Image.open(ASSETS / "trophy_big.png"))
    make_boss_shot()
    make_shield()
    make_planets()
