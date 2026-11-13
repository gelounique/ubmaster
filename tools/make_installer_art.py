"""Inno Setup wizard images (UBMaster look): black, Tiffany halo, UB icon."""
from PIL import Image, ImageDraw, ImageFilter
import math
icon = Image.open("branding/icon-win-1024.png").convert("RGBA")
def large(w, h):
    im = Image.new("RGB", (w, h), (0, 0, 0))
    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0)); d = ImageDraw.Draw(glow)
    cx, cy, r = w / 2, h * 0.36, w * 0.62
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(129, 216, 208, 46))
    im.paste(glow.filter(ImageFilter.GaussianBlur(w * 0.18)), (0, 0), glow.filter(ImageFilter.GaussianBlur(w * 0.18)))
    s = int(w * 0.78); ic = icon.resize((s, s), Image.LANCZOS)
    im.paste(ic, (int(cx - s / 2), int(cy - s / 2)), ic)
    d = ImageDraw.Draw(im)
    # thin tiffany line + bars at the bottom (vinyl spectrum hint)
    n = 46
    for i in range(n):
        v = 0.35 + 0.35 * math.sin(i * 0.7) * math.sin(i * 0.23 + 1) + 0.25
        x = w * 0.12 + (w * 0.76) * i / (n - 1); y0 = h * 0.86
        d.line([x, y0, x, y0 - h * 0.07 * max(0.1, v)], fill=(129, 216, 208), width=max(1, w // 160))
    return im
large(328, 628).save("installer/windows/wizard-large.bmp"); large(164, 314).save("installer/windows/wizard-large-100.bmp")
for s, f in ((110, "installer/windows/wizard-small.bmp"), (55, "installer/windows/wizard-small-100.bmp")):
    im = Image.new("RGB", (s, s), (255, 255, 255)); ic = icon.resize((s, s), Image.LANCZOS); im.paste(ic, (0, 0), ic); im.save(f)
print("installer art written")
