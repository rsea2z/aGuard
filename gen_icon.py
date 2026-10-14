"""Generate app.ico / preview.png — original artwork (Win11 Fluent-style
muted slate shield with a white "pulse" line, i.e. keep-alive). Drawn from
scratch; no third-party icon assets are used. Requires Pillow.
"""
from PIL import Image, ImageDraw

SIZES = (16, 20, 24, 32, 40, 48, 64, 256)
SS = 8  # supersampling factor

TOP = (104, 120, 140)     # gradient top (slate)
BOTTOM = (58, 70, 86)     # gradient bottom

# pulse polyline in unit coordinates (0..1)
PULSE = [(0.24, 0.50), (0.40, 0.50), (0.47, 0.32), (0.56, 0.68),
         (0.62, 0.50), (0.76, 0.50)]


def bezier(p0, p1, p2, steps=48):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1])
            for t in (i / steps for i in range(steps + 1))]


def shield(n, inset=0.0):
    """Shield outline: flat top with rounded corners, curved sides to a point."""
    l, r, t, b = 0.12 + inset, 0.88 - inset, 0.06 + inset, 0.95 - inset
    c, mid = 0.10, 0.5
    pts = []
    pts += bezier((l, t + c), (l, t), (l + c, t))            # top-left corner
    pts += bezier((r - c, t), (r, t), (r, t + c))            # top-right corner
    pts += bezier((r, 0.52), (r, 0.80), (mid, b))[:-1]       # right side -> tip
    pts += bezier((mid, b), (l, 0.80), (l, 0.52))            # tip -> left side
    # straight side segments are implied by polygon edges between curves
    return [(x * n, y * n) for x, y in pts]


def render(size):
    n = size * SS
    mask = Image.new("L", (n, n), 0)
    ImageDraw.Draw(mask).polygon(shield(n), fill=255)

    grad = Image.new("RGBA", (n, n))
    gd = ImageDraw.Draw(grad)
    for y in range(n):
        k = y / (n - 1)
        gd.line((0, y, n, y), fill=tuple(round(a + (b - a) * k)
                                         for a, b in zip(TOP, BOTTOM)) + (255,))
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    img.paste(grad, (0, 0), mask)

    d = ImageDraw.Draw(img)
    if size >= 32:  # Fluent-style subtle inner highlight
        d.line(shield(n, 0.022) + [shield(n, 0.022)[0]],
               fill=(255, 255, 255, 60), width=max(1, round(n * 0.012)))
    width = round(n * (0.11 if size <= 24 else 0.075))
    pts = [(x * n, y * n) for x, y in PULSE]
    d.line(pts, fill="white", width=width, joint="curve")
    r = width / 2
    for x, y in (pts[0], pts[-1]):
        d.ellipse((x - r, y - r, x + r, y + r), fill="white")
    return img.resize((size, size), Image.LANCZOS)


def main():
    imgs = [render(s) for s in SIZES]
    imgs[-1].save("app.ico", sizes=[(s, s) for s in SIZES],
                  append_images=imgs[:-1])
    imgs[-1].save("preview.png")


if __name__ == "__main__":
    main()
