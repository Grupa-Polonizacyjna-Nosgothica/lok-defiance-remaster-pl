#!/usr/bin/env python3
"""List and export the "!WAR" textures (*.raw) stored in bigfile.x64.dat / bigfilehd.dat.

Header (128 bytes, little endian u32 fields, derived from defiance.exe and the data):
  [0] "!WAR"  [1] header size (0x80)  [2] data size (all mip levels)  [3] format
  [5] width  [6] height  [7] row pitch in bytes  [8] bits per pixel
  [13..16] A/R/G/B masks for the 32 bit format  [17..20] log2 size / mip hints
Format 17 = 8 bit single channel, 16 = 32 bit RGBA (byte order R,G,B,A). The first
width*height*bpp/8 bytes after the header are the top mip level, further levels follow.

  war_textures.py list <dat> [--csv out.csv]       summary of every texture
  war_textures.py export <dat> <out_dir> [--only HASH,HASH] [--path "PCEnglish\\font1.raw"]
"""
import argparse, collections, csv, os, struct, sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "bigfile"))
import bigfile_index as bi  # noqa: E402

MAGIC = b"!WAR"


def read_header(f, offset, size):
    if size < 128:
        return None
    f.seek(offset)
    head = f.read(128)
    if head[:4] != MAGIC:
        return None
    u = struct.unpack("<32I", head)
    return {"header": u[1], "data": u[2], "format": u[3], "width": u[5], "height": u[6],
            "pitch": u[7], "bpp": u[8], "masks": u[13:17], "raw": head}


def to_image(f, offset, size, hdr):
    from PIL import Image
    w, h, bpp = hdr["width"], hdr["height"], hdr["bpp"]
    n = w * h * bpp // 8
    f.seek(offset + hdr["header"])
    data = f.read(n)
    if len(data) < n:
        return None
    if bpp == 8:
        return Image.frombytes("L", (w, h), data)
    if bpp == 32:
        return Image.frombytes("RGBA", (w, h), data)
    return None


def cmd_list(args):
    with open(args.dat, "rb") as f:
        _, hashes, entries = bi.read_index(f)
        rows = []
        for i, (hh, (size, off, _)) in enumerate(zip(hashes, entries)):
            hdr = read_header(f, off, size)
            if hdr:
                rows.append((i, hh, size, off, hdr))
    kinds = collections.Counter((r[4]["format"], r[4]["bpp"]) for r in rows)
    print(f"{len(rows)} textures of {len(hashes)} entries; (format, bpp): count -> {dict(kinds)}", file=sys.stderr)
    out = open(args.csv, "w", newline="") if args.csv else sys.stdout
    w = csv.writer(out)
    w.writerow(["slot", "hash", "size", "offset", "width", "height", "format", "bpp", "data_size"])
    for i, hh, size, off, hdr in rows:
        w.writerow([i, f"{hh:08x}", size, f"0x{off:x}", hdr["width"], hdr["height"], hdr["format"], hdr["bpp"], hdr["data"]])


def cmd_export(args):
    os.makedirs(args.out_dir, exist_ok=True)
    only = {int(x, 16) for x in args.only.split(",")} if args.only else None
    if args.path:
        only = (only or set()) | {bi.path_hash(p) for p in args.path}
    done = set()
    with open(args.dat, "rb") as f:
        _, hashes, entries = bi.read_index(f)
        for hh, (size, off, _) in zip(hashes, entries):
            if (only is not None and hh not in only) or (hh, off) in done:
                continue
            hdr = read_header(f, off, size)
            if not hdr:
                continue
            img = to_image(f, off, size, hdr)
            if img:
                done.add((hh, off))
                img.save(os.path.join(args.out_dir, f"{hh:08x}.png"))
    print(f"exported {len(done)} textures to {args.out_dir}", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("list"); p.add_argument("dat"); p.add_argument("--csv"); p.set_defaults(fn=cmd_list)
    p = sub.add_parser("export"); p.add_argument("dat"); p.add_argument("out_dir")
    p.add_argument("--only"); p.add_argument("--path", action="append"); p.set_defaults(fn=cmd_export)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
