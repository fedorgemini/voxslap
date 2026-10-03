"""
Packs the frames rendered by tools/render_controls.py into sprite sheets in Resources/.

    python3 tools/pack_controls.py <render outdir>

Knobs: 120 frames (3 degrees apart, clockwise from 12 o'clock) in a 12 x 10 grid.
Toggles: 5 frames side by side, from on (frame 0) to off (frame 4).
"""
import os, sys, glob
from PIL import Image

SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(__file__), "..", "Resources")
COLS = 12
# Frame size in the plugin: sharp at 150% on a Retina screen without wasting memory.
KNOB_FRAME = {"alu": 256, "black": 150, "chicken": 224}


def soften_edges(img):
    """The shadow catcher's shadow reaches the frame edge; fade it out radially so no square edge shows."""
    import numpy as np
    a = np.asarray(img, dtype=np.float32)
    h, w = a.shape[:2]
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot(x - (w - 1) / 2, y - (h - 1) / 2) / (min(w, h) / 2)
    fade = np.clip((1.0 - r) / 0.22, 0.0, 1.0)
    a[..., 3] *= fade
    return Image.fromarray(a.astype(np.uint8))


def frames(name):
    return [soften_edges(Image.open(f).convert("RGBA")) for f in sorted(glob.glob(os.path.join(SRC, name, "*.png")))]


def save(img, name):
    path = os.path.join(OUT, name)
    img.save(path, optimize=True)
    print(name, img.size, os.path.getsize(path) // 1024, "KB")


for name in ("alu", "black", "chicken"):
    fs = [f.resize((KNOB_FRAME[name], KNOB_FRAME[name]), Image.LANCZOS) for f in frames(name)]
    w, h = fs[0].size
    rows = (len(fs) + COLS - 1) // COLS
    sheet = Image.new("RGBA", (COLS * w, rows * h), (0, 0, 0, 0))
    for i, f in enumerate(fs):
        sheet.paste(f, ((i % COLS) * w, (i // COLS) * h))
    save(sheet, f"knob_{name}.png")

for name in ("toggle", "power"):
    fs = frames(name)
    sheet = Image.new("RGBA", (fs[0].width * len(fs), fs[0].height))
    for i, f in enumerate(fs):
        sheet.paste(f, (i * f.width, 0))
    save(sheet, f"{name}.png")
