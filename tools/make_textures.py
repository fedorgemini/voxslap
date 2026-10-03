"""
Bakes the UI textures in Resources/ from CC0 photo-scanned materials by ambientCG (https://ambientcg.com):

    PaintedMetal004, Metal011, Wood066, SurfaceImperfections003, Fingerprints002  (1K-JPG versions)

Usage:
    python3 tools/make_textures.py <folder with the unzipped ambientCG downloads>

Requires Pillow and numpy. Output sizes are 2x the editor's logical layout (for Retina screens).
"""
import sys, os
import numpy as np
from PIL import Image

SRC = sys.argv[1]
OUT = os.path.join(os.path.dirname(__file__), "..", "Resources")
os.makedirs(OUT, exist_ok=True)

EDITOR_W, CHEEK_W, TOP_H, BOTTOM_H = 1000, 26, 540, 196
S = 2  # retina scale
UNIT_W = (EDITOR_W - 2 * CHEEK_W) * S


def load(asset, kind, mode="L"):
    path = os.path.join(SRC, asset, f"{asset}_1K-JPG_{kind}.jpg")
    return np.asarray(Image.open(path).convert(mode), dtype=np.float32) / 255.0


def cover(a, w, h, offset=(0, 0), scale=1.0):
    """Scale one seamless tile up to cover the whole area, so no pattern repeats."""
    side = int(max(w, h) * scale)
    img = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).resize((side, side), Image.LANCZOS)
    big = np.asarray(img, dtype=np.float32) / 255.0
    big = np.roll(big, offset, axis=(0, 1))
    return big[:h, :w]


def tile(a, w, h, offset=(0, 0)):
    a = np.roll(a, offset, axis=(0, 1))
    reps = (h // a.shape[0] + 1, w // a.shape[1] + 1) + ((1,) if a.ndim == 3 else ())
    return np.tile(a, reps)[:h, :w]


def save(arr, name, quality=90):
    img = Image.fromarray(np.clip(arr * 255.0, 0, 255).astype(np.uint8))
    img.save(os.path.join(OUT, name), quality=quality, optimize=True)
    print(name, img.size, os.path.getsize(os.path.join(OUT, name)) // 1024, "KB")


def edge_weight(w, h, falloff):
    """1 at the panel border fading to 0 inside: wear concentrates on edges, as on real gear."""
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.minimum(np.minimum(x, w - 1 - x), np.minimum(y, h - 1 - y))
    return np.exp(-d / falloff)


chips_src = load("PaintedMetal004", "Metalness")
rough_src = load("PaintedMetal004", "Roughness")
grime_src = load("SurfaceImperfections003", "Opacity")


def painted_panel(w, h, base_rgb, chip_strength, offset, seed):
    rng = np.random.default_rng(seed)
    base = np.array(base_rgb, np.float32) / 255.0

    rough = cover(rough_src, w, h, offset)
    rough = (rough - rough.mean()) / (rough.std() + 1e-6)
    grime = cover(grime_src, w, h, (offset[1], offset[0]), 1.2)
    grime = (grime - grime.min()) / (np.ptp(grime) + 1e-6)

    chips = cover(chips_src, w, h, offset)
    wear = 0.05 + 0.95 * edge_weight(w, h, 38.0 * S)
    chip_alpha = np.clip(chips * wear * chip_strength, 0, 1)

    shade = 1.0 + 0.03 * rough - 0.16 * grime + 0.015 * rng.standard_normal((h, w)).astype(np.float32)
    paint = base[None, None, :] * shade[..., None]

    steel = np.array([0.56, 0.55, 0.53], np.float32)[None, None, :] * (0.85 + 0.08 * rough[..., None])
    out = paint * (1 - chip_alpha[..., None]) + steel * chip_alpha[..., None]
    return out


save(painted_panel(UNIT_W, TOP_H * S, (79, 31, 29), 0.85, (0, 0), 1), "panel_burgundy.jpg")
save(painted_panel(UNIT_W, BOTTOM_H * S, (28, 27, 26), 0.32, (311, 517), 2), "panel_black.jpg")

# Brushed aluminium name plate (grain runs horizontally)
alu = load("Metal011", "Color")
plate_w, plate_h = UNIT_W - 36 * S, 60 * S
strip = tile(alu, plate_w, plate_h, (300, 0))
strip = strip / strip.mean()
save(np.clip(strip[..., None] * np.array([0.56, 0.556, 0.54], np.float32), 0, 1), "plate_aluminium.jpg")

# Walnut cheeks: rotate so the grain runs vertically, take two strips (left/right cheek)
wood = np.rot90(load("Wood066", "Color", "RGB"))
cheek_w, cheek_h = CHEEK_W * S, (TOP_H + BOTTOM_H) * S
left = tile(wood[:, 180:180 + cheek_w], cheek_w, cheek_h)
right = tile(wood[:, 640:640 + cheek_w], cheek_w, cheek_h, (400, 0))
save(np.concatenate([left, right], axis=1) * 0.92, "wood_cheeks.jpg")

# Fingerprints/smudges for glass (used as a faint white alpha mask)
prints = load("Fingerprints002", "Opacity")
img = Image.fromarray((prints * 255).astype(np.uint8)).resize((512, 512), Image.LANCZOS)
img.save(os.path.join(OUT, "glass_smudges.jpg"), quality=85)
print("glass_smudges.jpg", img.size)

# Wear mask for silkscreen ink: mostly opaque, with small rubbed-off spots and hairline scratches.
chips = Image.fromarray((chips_src * 255).astype(np.uint8)).resize((512, 512), Image.LANCZOS)
c = np.asarray(chips, dtype=np.float32) / 255.0
ink = np.clip(1.0 - 0.85 * c, 0.0, 1.0) * (0.9 + 0.1 * load("PaintedMetal004", "Roughness")[:512, :512])
Image.fromarray((np.clip(ink, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, "ink_wear.png"), optimize=True)
print("ink_wear.png", (512, 512))
