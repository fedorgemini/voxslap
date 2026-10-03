"""
Packs the frames rendered by tools/render_controls.py into sprite sheets in Resources/.

    python3 tools/pack_controls.py <render outdir>

Knobs: 120 frames (3 degrees apart, clockwise from 12 o'clock) in a 12 x 10 grid.
Toggle: [on, off] side by side. Key: [off, on, down] stacked vertically.
"""
import os, sys, glob
from PIL import Image

SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(__file__), "..", "Resources")
COLS = 12


def frames(name):
    return [Image.open(f).convert("RGBA") for f in sorted(glob.glob(os.path.join(SRC, name, "*.png")))]


def save(img, name):
    path = os.path.join(OUT, name)
    img.save(path, optimize=True)
    print(name, img.size, os.path.getsize(path) // 1024, "KB")


for name in ("alu", "black", "chicken"):
    fs = frames(name)
    w, h = fs[0].size
    rows = (len(fs) + COLS - 1) // COLS
    sheet = Image.new("RGBA", (COLS * w, rows * h), (0, 0, 0, 0))
    for i, f in enumerate(fs):
        sheet.paste(f, ((i % COLS) * w, (i // COLS) * h))
    save(sheet, f"knob_{name}.png")

fs = frames("toggle")
sheet = Image.new("RGBA", (fs[0].width * len(fs), fs[0].height))
for i, f in enumerate(fs):
    sheet.paste(f, (i * f.width, 0))
save(sheet, "toggle.png")

fs = frames("key")
sheet = Image.new("RGBA", (fs[0].width, fs[0].height * len(fs)))
for i, f in enumerate(fs):
    sheet.paste(f, (0, i * f.height))
save(sheet, "key.png")
