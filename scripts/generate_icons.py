"""Generates Bytes's branding assets: bytes.png, bytes.ico, folder.ico.

Run once during development (or re-run to regenerate). Requires Pillow.
Output goes to ../assets relative to this script.
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.abspath(os.path.join(HERE, "..", "assets"))
os.makedirs(ASSETS, exist_ok=True)

NAVY_DARK = (10, 10, 12, 255)
NAVY_LIGHT = (30, 12, 14, 255)
ACCENT = (214, 40, 40, 255)    # Bytes red
WHITE = (245, 248, 255, 255)


def find_font(size, bold=True):
    candidates = [
        r"C:\Windows\Fonts\segoeuib.ttf" if bold else r"C:\Windows\Fonts\segoeui.ttf",
        r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf",
    ]
    for path in candidates:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def vertical_gradient(size, top, bottom):
    img = Image.new("RGBA", (size, size), top)
    draw = ImageDraw.Draw(img)
    for y in range(size):
        t = y / max(1, size - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(4))
        draw.line([(0, y), (size, y)], fill=color)
    return img


def rounded_mask(size, radius):
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return mask


def draw_binary_accent(draw, size):
    # A subtle row of "bits" along the bottom to read as tech/byte themed.
    bit_w = size * 0.045
    gap = size * 0.02
    total = 8 * bit_w + 7 * gap
    x = (size - total) / 2
    y = size * 0.78
    bits = [1, 0, 1, 1, 0, 1, 0, 1]
    for b in bits:
        h = size * (0.09 if b else 0.05)
        draw.rounded_rectangle([x, y - h, x + bit_w, y], radius=bit_w * 0.3,
                                fill=ACCENT if b else (255, 255, 255, 90))
        x += bit_w + gap


def make_master(size=1024):
    img = vertical_gradient(size, NAVY_LIGHT, NAVY_DARK)
    mask = rounded_mask(size, int(size * 0.22))
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(img, (0, 0), mask)

    draw = ImageDraw.Draw(canvas)

    # Accent ring
    ring_w = size * 0.02
    draw.rounded_rectangle([ring_w, ring_w, size - ring_w, size - ring_w],
                            radius=int(size * 0.22), outline=ACCENT, width=int(size * 0.012))

    # The "B" glyph
    font = find_font(int(size * 0.56))
    text = "B"
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (size - tw) / 2 - bbox[0]
    ty = (size - th) / 2 - bbox[1] - size * 0.03
    draw.text((tx, ty), text, font=font, fill=WHITE)

    draw_binary_accent(draw, size)
    return canvas


def make_folder_icon(size=1024, badge_image=None):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    body_top = size * 0.32
    body_color = (40, 20, 20, 255)   # dark charcoal-red body, matching the app theme
    tab_color = (58, 30, 30, 255)

    # Tab
    draw.rounded_rectangle([size * 0.08, size * 0.22, size * 0.42, body_top + size * 0.04],
                            radius=int(size * 0.05), fill=tab_color)
    # Body
    draw.rounded_rectangle([size * 0.08, body_top, size * 0.92, size * 0.86],
                            radius=int(size * 0.07), fill=body_color)
    # Highlight stripe
    draw.rounded_rectangle([size * 0.08, body_top, size * 0.92, body_top + size * 0.05],
                            radius=int(size * 0.02), fill=(255, 255, 255, 30))

    # Bytes badge (small circle) bottom-right, carrying the real Bytes logo
    badge_r = size * 0.19
    cx, cy = size * 0.78, size * 0.72
    draw.ellipse([cx - badge_r, cy - badge_r, cx + badge_r, cy + badge_r], fill=(18, 18, 18, 255),
                 outline=ACCENT, width=int(size * 0.012))

    if badge_image is not None:
        inner = int(badge_r * 1.5)
        logo = badge_image.convert("RGBA").resize((inner, inner), Image.LANCZOS)
        img.paste(logo, (int(cx - inner / 2), int(cy - inner / 2)), logo)
    else:
        font = find_font(int(badge_r * 1.15))
        text = "B"
        bbox = draw.textbbox((0, 0), text, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        draw.text((cx - tw / 2 - bbox[0], cy - th / 2 - bbox[1]), text, font=font, fill=WHITE)

    return img


def save_ico(img, path, sizes=(16, 24, 32, 48, 64, 128, 256)):
    img.save(path, format="ICO", sizes=[(s, s) for s in sizes])


if __name__ == "__main__":
    # If the user has supplied their own logo (Bytes/icon.ico, next to this
    # repo), use it as-is for bytes.ico/bytes.png and as the folder badge,
    # instead of the generated placeholder "B" mark.
    custom_logo_path = os.path.join(ASSETS, "..", "icon.ico")
    if os.path.exists(custom_logo_path):
        logo = Image.open(custom_logo_path).convert("RGBA")
        logo.resize((512, 512), Image.LANCZOS).save(os.path.join(ASSETS, "bytes.png"))
        # Re-save as .ico with a full size set so Windows always has a
        # crisp size to pick from (taskbar, tray, Explorer, shortcuts).
        biggest = logo.resize((256, 256), Image.LANCZOS)
        save_ico(biggest, os.path.join(ASSETS, "bytes.ico"))
        folder = make_folder_icon(1024, badge_image=logo)
    else:
        master = make_master(1024)
        master.save(os.path.join(ASSETS, "bytes.png"))
        save_ico(master, os.path.join(ASSETS, "bytes.ico"))
        folder = make_folder_icon(1024)

    save_ico(folder, os.path.join(ASSETS, "folder.ico"))

    print("Wrote:", os.path.join(ASSETS, "bytes.png"))
    print("Wrote:", os.path.join(ASSETS, "bytes.ico"))
    print("Wrote:", os.path.join(ASSETS, "folder.ico"))
