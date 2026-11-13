"""UBMaster icon: 'Loudness Halo' — radial ring of fine rising/falling spectrum bars in Tiffany blue with soft bloom,
a bright target marker top-right and the white UB logo with a light chrome shimmer, on a deep-black rounded square.
usage: python3 tools/make_icon.py [logo.png] [outdir]"""
import math, random, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

S = 2048
LOGO = sys.argv[1] if len(sys.argv) > 1 else "plugin/resources/ub-logo-full.png"
OUT = sys.argv[2] if len(sys.argv) > 2 else "branding"
TIF = (129, 216, 208)
WHITE = (255, 255, 255)

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

def halo_layer(size, R):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2
    rnd = random.Random(7)
    n = 168
    base, ln = R * 0.80, R * 0.36
    # vinyl grooves
    for k in range(6):
        rr = base * (0.70 + 0.05 * k)
        d.ellipse([c - rr, c - rr, c + rr, c + rr], outline=(255, 255, 255, 14 + 6 * (k % 2)), width=max(1, size // 900))
    # spectrum bars: smooth "music" envelope + jitter, mirrored left/right
    for i in range(n):
        t = i / n
        m = t * 2 if t < 0.5 else (1 - t) * 2
        env = 0.55 + 0.30 * math.sin(m * 7.5 + 0.6) * math.exp(-m * 0.6) + 0.18 * math.sin(m * 23 + 1.1)
        v = max(0.12, min(1.0, env + rnd.uniform(-0.12, 0.12)))
        a = t * math.tau
        x0, y0 = c + math.sin(a) * base, c - math.cos(a) * base
        x1, y1 = c + math.sin(a) * (base + 6 + ln * v), c - math.cos(a) * (base + 6 + ln * v)
        col = lerp(TIF, WHITE, max(0.0, (v - 0.72) * 2.2))
        d.line([x0, y0, x1, y1], fill=col + (int(130 + 125 * v),), width=max(2, int(size / 340)))
    # gauge arc (thin) from -135° to the marker at +50°
    rr = R * 1.24
    box = [c - rr, c - rr, c + rr, c + rr]
    d.arc(box, -90 - 135, -90 + 135, fill=(255, 255, 255, 34), width=max(2, size // 400))
    d.arc(box, -90 - 135, -90 + 50, fill=TIF + (235,), width=max(3, size // 260))
    return img

def marker_layer(size, R):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = size / 2
    a = math.radians(50)
    rr = R * 1.24
    x, y = c + math.sin(a) * rr, c - math.cos(a) * rr
    for i, al in ((7, 40), (5, 70), (3.4, 120)):
        r = size / 1024 * 6 * i
        d.ellipse([x - r, y - r, x + r, y + r], fill=TIF + (al,))
    r = size / 1024 * 13
    d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 255, 255))
    return img

def chrome_logo(size):
    logo = Image.open(LOGO).convert("RGBA")
    lw = int(size * 0.36)
    logo = logo.resize((lw, int(logo.height * lw / logo.width)), Image.LANCZOS)
    a = np.array(logo).astype(float)
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    # chrome: white with a soft diagonal silver band + top highlight
    band = 0.5 + 0.5 * np.cos((xx / w * 1.2 + yy / h * 0.9) * math.pi * 2.0)
    shade = 0.80 + 0.20 * band - 0.10 * (yy / h)
    for ch in range(3): a[..., ch] = np.clip(255 * shade, 0, 255)
    return Image.fromarray(a.astype(np.uint8))

def make(full_bleed):
    base = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    if full_bleed:
        box, rad = (0, 0, S, S), int(S * 0.2)
    else:
        m = int(S * 100 / 1024); box, rad = (m, m, S - m, S - m), int(S * 0.2)
        sh = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(sh).rounded_rectangle((box[0], box[1] + S * 0.012, box[2], box[3] + S * 0.012), rad, fill=(0, 0, 0, 150))
        base = Image.alpha_composite(base, sh.filter(ImageFilter.GaussianBlur(S * 0.018)))
    mask = Image.new("L", (S, S), 0); ImageDraw.Draw(mask).rounded_rectangle(box, rad, fill=255)
    yy, xx = np.mgrid[0:S, 0:S]
    dist = np.sqrt((xx - S / 2) ** 2 + (yy - S / 2) ** 2) / (S / 2)
    g = np.clip(1 - dist, 0, 1) ** 1.8
    arr = np.zeros((S, S, 4), np.uint8)
    arr[..., 0] = (3 + 10 * g).astype(np.uint8); arr[..., 1] = (4 + 22 * g).astype(np.uint8); arr[..., 2] = (5 + 21 * g).astype(np.uint8); arr[..., 3] = 255
    body = Image.fromarray(arr); body.putalpha(mask)
    base = Image.alpha_composite(base, body)

    R = S * 0.27
    ring = halo_layer(S, R)
    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    for blur, k in ((S * 0.03, 2.2), (S * 0.009, 1.4)):
        gl = np.array(ring.filter(ImageFilter.GaussianBlur(blur))).astype(float); gl[..., 3] = np.clip(gl[..., 3] * k, 0, 255)
        layer = Image.alpha_composite(layer, Image.fromarray(gl.astype(np.uint8)))
    layer = Image.alpha_composite(layer, ring)
    mk = marker_layer(S, R)
    layer = Image.alpha_composite(layer, mk.filter(ImageFilter.GaussianBlur(S * 0.01)))
    layer = Image.alpha_composite(layer, mk)
    clip = Image.new("RGBA", (S, S), (0, 0, 0, 0)); clip.paste(layer, (0, 0), mask)
    base = Image.alpha_composite(base, clip)

    logo = chrome_logo(S)
    L = Image.new("RGBA", (S, S), (0, 0, 0, 0)); L.paste(logo, ((S - logo.width) // 2, (S - logo.height) // 2 + int(S * 0.004)), logo)
    gl = np.array(L).astype(float); gl[..., 0], gl[..., 1], gl[..., 2] = TIF; gl[..., 3] *= 0.55
    base = Image.alpha_composite(base, Image.fromarray(gl.astype(np.uint8)).filter(ImageFilter.GaussianBlur(S * 0.018)))
    base = Image.alpha_composite(base, L)
    rim = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(rim).rounded_rectangle(box, rad, outline=(255, 255, 255, 30), width=max(2, S // 512))
    return Image.alpha_composite(base, rim)

mac = make(False); win = make(True)
for sz in (1024, 512, 256, 128, 64, 48, 32, 24, 16):
    mac.resize((sz, sz), Image.LANCZOS).save(f"{OUT}/icon-{sz}.png", optimize=True)
win.resize((1024, 1024), Image.LANCZOS).save(f"{OUT}/icon-win-1024.png", optimize=True)
w256 = win.resize((256, 256), Image.LANCZOS)
w256.save(f"{OUT}/UBMaster.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
mac.resize((1024, 1024), Image.LANCZOS).save(f"{OUT}/UBMaster.icns")
print("icons written")
