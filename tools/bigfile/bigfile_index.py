#!/usr/bin/env python3
"""Dump the file index of bigfile.x64.dat / bigfilehd.dat as a CSV offset map.

Format derived statically from defiance.exe (see docs/bigfile-format.md).
Entries are keyed by a CRC-32 of the lowercased path, so names are only
recovered for paths passed via --names (one per line) or the built-in guesses.
"""
import argparse, struct, sys

KNOWN_PATHS = [
    "tex.map", "misc/noise3d.bin", "PCEnglish\\locals.bin",
    "\\SR3\\image\\char_casing.txt", "\\SR3\\image\\font.ini",
    "\\sr3\\object\\artifact\\artifacts.ini",
]
KIND_BY_EXT = {
    ".bin": "binary blob", ".txt": "text", ".ini": "config", ".map": "texture map",
    ".ogg": "video/audio", ".dat": "data",
}

def crc32_msb(s: bytes) -> int:
    """CRC-32/BZIP2 (poly 0x04C11DB7, MSB-first), as FUN_1400024e0."""
    crc = 0xFFFFFFFF
    for b in s:
        crc ^= b << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
    return ~crc & 0xFFFFFFFF

def path_hash(path: str) -> int:
    return crc32_msb(path.lower().encode("utf-8"))

def read_index(f):
    head = struct.unpack("<I", f.read(4))[0]
    count, b2, b3 = head & 0xFFFF, (head >> 16) & 0xFF, head >> 24
    blob = f.read(count * 0x1C)
    hashes = struct.unpack_from(f"<{count}I", blob, 0)
    entries = [struct.unpack_from("<QQQ", blob, count * 4 + i * 0x18) for i in range(count)]
    return (count, b2, b3), hashes, entries

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bigfile")
    ap.add_argument("--names", help="text file with candidate paths, one per line")
    a = ap.parse_args()
    names = list(KNOWN_PATHS)
    if a.names:
        names += [l.strip() for l in open(a.names, encoding="utf-8") if l.strip()]
    lookup = {path_hash(n): n for n in names}
    with open(a.bigfile, "rb") as f:
        hdr, hashes, entries = read_index(f)
    print(f"# header: count={hdr[0]} byte2={hdr[1]} byte3={hdr[2]}", file=sys.stderr)
    print("hash,size,offset,field2,name,kind")
    for h, (size, off, extra) in zip(hashes, entries):
        n = lookup.get(h, "")
        ext = n[n.rfind("."):].lower() if "." in n else ""
        print(f"{h:08x},{size},0x{off:x},{extra},{n},{KIND_BY_EXT.get(ext, '')}")

if __name__ == "__main__":
    main()
