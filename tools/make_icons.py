"""Derives the Windows .ico (installer + app) from Resources/icon.png rendered by tools/render_icon.py."""
import os
from PIL import Image

here = os.path.dirname(os.path.abspath(__file__))
src = Image.open(os.path.join(here, "..", "Resources", "icon.png")).convert("RGBA")
out = os.path.join(here, "..", "installer", "windows", "icon.ico")
src.save(out, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
print("Saved", out)
