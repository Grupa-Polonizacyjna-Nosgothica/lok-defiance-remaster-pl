#!/usr/bin/env python3
"""Add the Polish letters ą ć ę ł ń ś ź ż (and capitals) to the game's Latin fonts.

Background (see docs/fonts.md): the game draws text from bitmap atlases `PCEnglish\\font<N>.raw`
(8 bit, stored bottom-up) described by `\\SR3\\image\\font<N>.txt` (one line per code point:
code,advance,x0,y0,x1,y1,u0,v0,u1,v1; x/y in em, u/v in atlas pixels from the top, half-pixel
inset). Fonts 1 (Main) and 2 (Header) are Latin; ó and Ó already exist.

Nothing of the game's own font data is stored in the repository. For every font the repository
holds only the *delta*, which the patcher applies to the user's own files:
    fonts/font<N>.overlay.png   atlas-sized image that is black except for the new glyphs (edit this)
    fonts/font<N>.overlay       the same as a tiny binary the patcher reads (made by `build`)
    fonts/font<N>.add.txt       glyph table lines of the new glyphs
    fonts/char_casing.add.txt   new `code,lower,upper` lines

    polish_glyphs.py generate <bigfile.x64.dat> <fonts_dir>   build the deltas from the game data
    polish_glyphs.py build    <fonts_dir>                     overlay PNG -> overlay binary
    polish_glyphs.py preview  <bigfile.x64.dat> <fonts_dir> "Zażółć gęślą jaźń" [--font N]
"""
import argparse, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "bigfile"))
import bigfile_index as bi  # noqa: E402

PX_PER_EM = 40.0
FONTS = (1, 2)
# (new, base, accent kind)
NEW_GLYPHS = [
    ("ą", "a", "ogonek"), ("Ą", "A", "ogonek"), ("ć", "c", "acute"), ("Ć", "C", "acute"),
    ("ę", "e", "ogonek"), ("Ę", "E", "ogonek"), ("ł", "l", "stroke"), ("Ł", "L", "stroke"),
    ("ń", "n", "acute"), ("Ń", "N", "acute"), ("ś", "s", "acute"), ("Ś", "S", "acute"),
    ("ź", "z", "acute"), ("Ź", "Z", "acute"), ("ż", "z", "dot"), ("Ż", "Z", "dot"),
]
CASING = [("ą", "Ą"), ("ć", "Ć"), ("ę", "Ę"), ("ł", "Ł"), ("ń", "Ń"), ("ś", "Ś"), ("ź", "Ź"), ("ż", "Ż")]
NOISE = 24  # residual below this is not part of an accent


class Glyph:
    def __init__(self, code, line):
        self.code = code
        p = line.split(",")
        self.adv, self.x0, self.y0, self.x1, self.y1, self.u0, self.v0, self.u1, self.v1 = map(float, p[1:])

    def box(self):  # inclusive pixel rectangle in the upright atlas
        return int(self.u0), int(self.v0), int(self.u1) + 1, int(self.v1) + 1

    def line(self):
        f = lambda v: repr(float(v)) if v != int(v) else str(int(v)) if abs(v) < 1e6 else repr(v)
        return ",".join([str(self.code)] + [f(v) for v in (self.adv, self.x0, self.y0, self.x1, self.y1,
                                                           self.u0, self.v0, self.u1, self.v1)])


def read_asset(path_in_archive, dat):
    with open(dat, "rb") as f:
        _, hashes, entries = bi.read_index(f)
        slots = [e for h, e in zip(hashes, entries) if h == bi.path_hash(path_in_archive)]
        if not slots:
            raise SystemExit(f"{path_in_archive} not found in {dat}")
        f.seek(slots[0][1])
        return f.read(slots[0][0])


def load_atlas(raw):
    hdr = raw[:128]
    w, h = struct.unpack_from("<2I", hdr, 20)
    pix = np.frombuffer(raw, dtype=np.uint8, count=w * h, offset=128).reshape(h, w)
    return hdr, pix[::-1].copy()  # upright


def parse_table(text):
    lines = text.replace("\r\n", "\n").split("\n")
    glyphs = {}
    for l in lines[1:]:
        if l.strip():
            glyphs[int(l.split(",")[0])] = Glyph(int(l.split(",")[0]), l)
    return glyphs


def crop(atlas, g):
    u0, v0, u1, v1 = g.box()
    return atlas[v0:v1, u0:u1].astype(np.int16)


def tight(bm):
    ys, xs = np.where(bm > 8)
    return ys.min(), ys.max(), xs.min(), xs.max()


def residual(accented, plain, at_top):
    """accented minus plain, where plain sits at the bottom (accent above) or top (ogonek below)."""
    ha, hp = accented.shape[0], plain.shape[0]
    pad = np.zeros_like(accented)
    w = min(plain.shape[1], accented.shape[1])
    if at_top:
        pad[:hp, :w] = plain[:, :w]
    else:
        pad[ha - hp:, :w] = plain[:, :w]
    r = np.clip(accented - pad, 0, 255)
    r[r < NOISE] = 0
    return r


def compose(kind, base, donors, glyphs, atlas):
    """Returns (bitmap, extra_top_rows, extra_left, extra_right) for a new glyph from `base` bitmap."""
    bm = crop(atlas, base)
    hb, wb = bm.shape
    t, b_, l, r = tight(bm)
    cx = (l + r) / 2.0
    upper = chr(base.code).isupper()
    if kind == "acute":
        acc, plain = donors["acute_up" if upper else "acute_lo"]
        a_bm, p_bm = crop(atlas, acc), crop(atlas, plain)
        res = residual(a_bm, p_bm, at_top=False)
        e = a_bm.shape[0] - p_bm.shape[0]
        pt, pb, pl, pr = tight(p_bm)
        shift = int(round(cx - (pl + pr) / 2.0))
        mask = res
        top = e
        out_h = hb + e
        place = lambda canvas, lx: add(canvas, mask, 0, shift + lx)
        wm = mask.shape[1]
    elif kind == "dot":
        dot = crop(atlas, glyphs[ord(".")])
        dt, db, dl, dr = tight(dot)
        mask = dot[dt:db + 1, dl:dr + 1]
        e = 11
        top, out_h, wm = e, hb + e, mask.shape[1]
        shift = int(round(cx - wm / 2.0))
        place = lambda canvas, lx: add(canvas, mask, 4, shift + lx)
    elif kind == "ogonek":
        acc, plain = donors["cedilla_up" if upper else "cedilla_lo"]
        e_bottom = crop(atlas, acc).shape[0] - crop(atlas, plain).shape[0]
        top, out_h, wm, shift = 0, hb + e_bottom, 0, 0
        place = lambda canvas, lx: draw_ogonek(canvas, (t, b_, l, r), lx)
    elif kind == "stroke":
        top, out_h = 0, hb
        wm = 0
        place = None
    else:
        raise ValueError(kind)

    left_extra = max(0, -shift) if kind in ("acute", "dot", "ogonek") else 0
    right_extra = max(0, shift + wm - wb) if kind in ("acute", "dot", "ogonek") else 0
    if kind == "stroke":
        left_extra = right_extra = 2
    elif kind == "ogonek":
        left_extra, right_extra = 0, 3  # the hook curls to the right of the letter
    canvas = np.zeros((out_h, wb + left_extra + right_extra), dtype=np.int16)
    canvas[top:top + hb, left_extra:left_extra + wb] = bm
    if kind == "stroke":
        draw_stroke(canvas, tight(bm), top, left_extra, upper)
    else:
        place(canvas, left_extra)
    return np.clip(canvas, 0, 255), top, left_extra, right_extra, (out_h - hb - top if kind == "ogonek" else 0)


def add(canvas, mask, row, col):
    h, w = mask.shape
    r0, c0 = max(0, row), max(0, col)
    r1, c1 = min(canvas.shape[0], row + h), min(canvas.shape[1], col + w)
    if r1 > r0 and c1 > c0:
        sub = mask[r0 - row:r1 - row, c0 - col:c1 - col]
        canvas[r0:r1, c0:c1] = np.maximum(canvas[r0:r1, c0:c1], sub)


def supersampled(canvas, draw_fn, ss=8):
    img = Image.new("L", (canvas.shape[1] * ss, canvas.shape[0] * ss), 0)
    draw_fn(ImageDraw.Draw(img), ss)
    small = np.array(img.resize((canvas.shape[1], canvas.shape[0]), Image.LANCZOS), dtype=np.int16)
    np.maximum(canvas, small, out=canvas)


def draw_stroke(canvas, tb, top, left, upper):
    """Short slanted bar across the stem (the stem is the left 9 pixels of the letter)."""
    t, b_, l, r = tb
    stem_cx = left + l + min(r - l, 9) / 2.0
    mid = top + (t + b_) / 2.0 + (1 if not upper else 0)
    half = 5.5

    def draw(d, ss):
        d.line([((stem_cx - half) * ss, (mid + 3.5) * ss), ((stem_cx + half) * ss, (mid - 3.5) * ss)],
               fill=215, width=int(2.0 * ss))
    supersampled(canvas, draw)


def draw_ogonek(canvas, tb, left):
    """Small hook hanging from the right foot of the letter."""
    t, b_, l, r = tb
    x = left + r - 3.0

    def draw(d, ss):
        pts = [(x, b_ - 1.0), (x - 0.5, b_ + 2.5), (x + 0.8, b_ + 5.0), (x + 3.0, b_ + 6.5), (x + 5.5, b_ + 6.0)]
        d.line([(px * ss, py * ss) for px, py in pts], fill=215, width=int(2.8 * ss), joint="curve")
    supersampled(canvas, draw)


def generate_font(n, dat, out):
    raw = read_asset(f"PCEnglish\\font{n}.raw", dat)
    table = read_asset(f"\\SR3\\image\\font{n}.txt", dat).decode("utf-8")
    hdr, atlas = load_atlas(raw)
    glyphs = parse_table(table)
    H, W = atlas.shape
    overlay = np.zeros_like(atlas)
    donors = {"acute_lo": (glyphs[ord("ó")], glyphs[ord("o")]), "acute_up": (glyphs[ord("Ó")], glyphs[ord("O")]),
              "cedilla_lo": (glyphs[ord("ç")], glyphs[ord("c")]), "cedilla_up": (glyphs[ord("Ç")], glyphs[ord("C")])}
    # skip glyphs the font already has
    todo = [(c, b, k) for c, b, k in NEW_GLYPHS if ord(c) not in glyphs]
    used_rows = max(g.box()[3] for g in glyphs.values() if g.u1 > g.u0)  # the space has a dummy box
    x, y, row_h = 0, used_rows + 2, 0
    new_glyphs = []
    for ch, base_ch, kind in todo:
        base = glyphs[ord(base_ch)]
        bm, top, lx, rx, bot = compose(kind, base, donors, glyphs, atlas)
        h, w = bm.shape
        if x + w > W:
            x, y, row_h = 0, y + row_h + 2, 0
        if y + h > H:
            raise SystemExit(f"font{n}: no room left in the atlas")
        overlay[y:y + h, x:x + w] = bm.astype(np.uint8)
        g = Glyph(ord(ch), ",".join(["0"] * 10))
        g.adv = base.adv
        g.x0, g.x1 = base.x0 - lx / PX_PER_EM, base.x1 + rx / PX_PER_EM
        g.y0 = base.y0 - top / PX_PER_EM
        g.y1 = base.y1 + bot / PX_PER_EM
        g.u0, g.v0, g.u1, g.v1 = x + 0.5, y + 0.5, x + w - 0.5, y + h - 0.5
        new_glyphs.append(g)
        x += w + 2
        row_h = max(row_h, h)
    write_delta(n, out, overlay, new_glyphs)
    print(f"font{n}: added {len(new_glyphs)} glyphs; atlas rows used up to {y + row_h} of {H}")


def write_delta(n, out, overlay, new_glyphs):
    os.makedirs(out, exist_ok=True)
    Image.fromarray(overlay, "L").save(os.path.join(out, f"font{n}.overlay.png"))
    with open(os.path.join(out, f"font{n}.add.txt"), "wb") as f:
        f.write(("\r\n".join(g.line() for g in sorted(new_glyphs, key=lambda g: g.code)) + "\r\n").encode())
    write_overlay_bin(n, out)


def write_overlay_bin(n, out):
    """Binary the patcher reads: only the pixel rectangles of the glyphs listed in font<N>.add.txt.
    "PLGO", u32 atlas width, u32 atlas height, u32 count, then per rectangle u16 x, y, w, h and w*h
    bytes (top-down rows). Applied with max() onto the original atlas."""
    img = Image.open(os.path.join(out, f"font{n}.overlay.png")).convert("L")
    arr = np.array(img)
    w, h = img.size
    lines = open(os.path.join(out, f"font{n}.add.txt"), "rb").read().decode().split("\r\n")
    rects = []
    for l in lines:
        if l.strip():
            u0, v0, u1, v1 = Glyph(int(l.split(",")[0]), l).box()
            rects.append((u0, v0, u1 - u0, v1 - v0))
    covered = np.zeros_like(arr, dtype=bool)
    body = b""
    for x, y, rw, rh in rects:
        covered[y:y + rh, x:x + rw] = True
        body += struct.pack("<4H", x, y, rw, rh) + arr[y:y + rh, x:x + rw].tobytes()
    if (arr[~covered] != 0).any():
        raise SystemExit(f"font{n}.overlay.png has pixels outside the glyph cells of font{n}.add.txt")
    with open(os.path.join(out, f"font{n}.overlay"), "wb") as f:
        f.write(b"PLGO" + struct.pack("<3I", w, h, len(rects)) + body)


def cmd_generate(args):
    for n in FONTS:
        generate_font(n, args.dat, args.out)
    text = read_asset("\\SR3\\image\\char_casing.txt", args.dat).decode()
    have = {int(l.split(",")[0]) for l in text.replace("\r\n", "\n").split("\n")[1:] if l.strip()}
    add = []
    for lo, up in CASING:
        if ord(lo) not in have:
            add += [f"{ord(lo)},0,{ord(up)}", f"{ord(up)},{ord(lo)},0"]
    with open(os.path.join(args.out, "char_casing.add.txt"), "wb") as f:
        f.write(("\r\n".join(add) + "\r\n").encode())


def cmd_build(args):
    for n in FONTS:
        write_overlay_bin(n, args.out)
        print(f"font{n}.overlay rebuilt from font{n}.overlay.png")


def cmd_preview(args):
    n = args.font
    _, atlas = load_atlas(read_asset(f"PCEnglish\\font{n}.raw", args.dat))
    overlay = np.array(Image.open(os.path.join(args.fonts, f"font{n}.overlay.png")).convert("L"))
    atlas = np.maximum(atlas, overlay)
    table = read_asset(f"\\SR3\\image\\font{n}.txt", args.dat).decode()
    table += "\r\n" + open(os.path.join(args.fonts, f"font{n}.add.txt"), "rb").read().decode()
    glyphs = parse_table(table)
    img = Image.new("L", (1400, 120), 20)
    px, base_y = 20.0, 80
    for ch in args.text:
        g = glyphs.get(ord(ch))
        if not g:
            px += 18
            continue
        u0, v0, u1, v1 = g.box()
        bm = Image.fromarray(atlas[v0:v1, u0:u1], "L")
        x, y = int(px + g.x0 * PX_PER_EM), int(base_y + g.y0 * PX_PER_EM)
        region = np.maximum(np.array(img.crop((x, y, x + bm.width, y + bm.height))), np.array(bm))
        img.paste(Image.fromarray(region, "L"), (x, y))
        px += g.adv * PX_PER_EM
    img.save(args.out)
    print("wrote", args.out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("generate"); p.add_argument("dat"); p.add_argument("out"); p.set_defaults(fn=cmd_generate)
    p = sub.add_parser("build"); p.add_argument("out"); p.set_defaults(fn=cmd_build)
    p = sub.add_parser("preview"); p.add_argument("dat"); p.add_argument("fonts"); p.add_argument("text")
    p.add_argument("--font", type=int, default=1); p.add_argument("--out", default="preview.png"); p.set_defaults(fn=cmd_preview)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
