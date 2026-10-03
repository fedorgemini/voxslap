"""
Draws the printed VU scale card (Resources/vu_card.png) used as a texture by tools/render_vu.py.

The geometry is the same as VUMeter in Source/PluginEditor.cpp (pivot, radius, angle mapping), so the
live needle lines up with the printed marks. Sizes are in logical px and rendered at SCALE.
"""
import math, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "Resources")
SCALE = 3
W, H = 184, 166                          # face = 214x196 well minus 15 px on each side
FONT = os.path.join(RES, "fonts", "Michroma-Regular.ttf")
INK, RED, PAPER = (30, 23, 16), (196, 38, 26), (242, 228, 190)

img = Image.new("RGB", (W * SCALE, H * SCALE), PAPER)
d = ImageDraw.Draw(img)

arc_top = 44.0
pivot = (W / 2, H + H * 0.42)
radius = pivot[1] - arc_top
max_angle = math.asin(min(0.95, (W / 2 - 18) / radius))


def angle_for(pos): return -max_angle + 2 * max_angle * pos
def pos_for_db(db): return 10 ** (db / 20) / 1.4125
def polar(r, a): return (pivot[0] + r * math.sin(a), pivot[1] - r * math.cos(a))
def S(p): return (p[0] * SCALE, p[1] * SCALE)


def arc(r, a0, a1, width, colour, steps=200):
    pts = [S(polar(r, a0 + (a1 - a0) * i / steps)) for i in range(steps + 1)]
    d.line(pts, fill=colour, width=int(round(width * SCALE)), joint="curve")


def line(r0, r1, a, width, colour):
    d.line([S(polar(r0, a)), S(polar(r1, a))], fill=colour, width=max(1, int(round(width * SCALE))))


def font(px): return ImageFont.truetype(FONT, int(px * SCALE))


def text(t, centre, px, colour, spacing=0.0, anchor="mm"):
    f = font(px)
    if spacing <= 0:
        d.text(S(centre), t, font=f, fill=colour, anchor=anchor)
        return
    widths = [d.textlength(ch, font=f) for ch in t]
    total = sum(widths) + spacing * SCALE * (len(t) - 1)
    x = centre[0] * SCALE - (total if anchor == "rm" else total / 2)
    for ch, w in zip(t, widths):
        d.text((x, centre[1] * SCALE), ch, font=f, fill=colour, anchor="lm")
        x += w + spacing * SCALE


# Main arc, red zone band
arc(radius, angle_for(pos_for_db(-20)), angle_for(1.0), 1.4, INK)
arc(radius - 4, angle_for(pos_for_db(0)), angle_for(1.0), 5.0, RED)

for db in (-20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3):
    a = angle_for(pos_for_db(db))
    major = db in (-20, -10, -7, -5, -3, 0, 3)
    col = RED if db > 0 else INK
    line(radius, radius + (10 if major else 6), a, 1.5 if major else 1.1, col)
    if db in (-20, -10, -5, -3, 0, 3):
        text(("+" if db > 0 else "") + str(db), polar(radius + 19, a), 7.2, col)

for pct in range(0, 101, 10):
    a = angle_for(pct / 100 / 1.4125)
    lab = pct % 50 == 0
    line(radius - 9, radius - (15 if lab else 12), a, 0.9, INK)
    if lab:
        text(str(pct), polar(radius - 22, a), 5.2, INK)

text("VU", (W / 2, H - 61), 16.0, INK, spacing=2.0)
text("PEAK", (W - 28, 16), 4.8, INK, spacing=0.8, anchor="rm")

img.save(os.path.join(RES, "vu_card.png"), optimize=True)
print("Saved vu_card.png", img.size)
