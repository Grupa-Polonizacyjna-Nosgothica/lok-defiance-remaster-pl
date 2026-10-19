#!/usr/bin/env python3
"""Check that strings/PL and cinematic/PL are consistent with their EN originals.

Needs no game data, so it runs in CI. Errors (exit code 1) are things that would break the game or
the patcher; warnings are things a translator should look at.

  validate_translation.py [repo_root] [--quiet]
"""
import os, re, sys, collections

PUA = re.compile("[-]")
PLACEHOLDER = re.compile(r"%[-+ #0]*\d*(?:\.\d+)?[a-zA-Z]")
TIMING = re.compile(r"^\d+ \d+$")
IDENT = re.compile(r"^[A-Za-z0-9_]+$")
MARKER = re.compile(r"\\\(([0-9.]+)\)")


class Report:
    def __init__(self):
        self.errors, self.warnings = [], []

    def error(self, where, msg):
        self.errors.append(f"{where}: {msg}")

    def warn(self, where, msg):
        self.warnings.append(f"{where}: {msg}")


def read(path, rep, where):
    raw = open(path, "rb").read()
    if raw.startswith(b"\xef\xbb\xbf"):
        rep.error(where, "has a UTF-8 byte order mark")
        raw = raw[3:]
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as e:
        rep.error(where, f"is not valid UTF-8 ({e})")
        return None
    if "\r" in text:
        rep.error(where, "contains a carriage return")
    if "\0" in text:
        rep.error(where, "contains a NUL character")
    return text


def compare_dirs(en_dir, pl_dir, rep, what):
    en = set(os.listdir(en_dir)) if os.path.isdir(en_dir) else set()
    pl = set(os.listdir(pl_dir)) if os.path.isdir(pl_dir) else set()
    for name in sorted(en - pl):
        rep.error(f"{what}/{name}", "missing in PL")
    for name in sorted(pl - en):
        rep.error(f"{what}/{name}", "has no EN original")
    return sorted(en & pl)


def check_string(name, en, pl, rep):
    where = f"strings/{name}"
    en_t = [l for l in en.split("\n") if TIMING.match(l)]
    pl_t = [l for l in pl.split("\n") if TIMING.match(l)]
    if en_t != pl_t:
        rep.error(where, "timing lines (`<start> 0`) differ from EN; they must stay exactly as they are")
    if collections.Counter(PUA.findall(en)) != collections.Counter(PUA.findall(pl)):
        rep.warn(where, "special markup characters (private-use glyphs) differ from EN")
    if collections.Counter(PLACEHOLDER.findall(en)) != collections.Counter(PLACEHOLDER.findall(pl)):
        rep.error(where, "format placeholders (%s, %d, ...) differ from EN")
    if "_" in en and IDENT.match(en) and pl != en:
        rep.error(where, f"internal name {en!r} must not be translated")
    if en.strip() and not pl.strip():
        rep.error(where, "is empty but the EN original is not")


def check_cinematic(name, en, pl, rep):
    where = f"cinematic/{name}"
    if name.startswith("textentry_"):
        if MARKER.findall(en) != MARKER.findall(pl):
            rep.error(where, "timing markers \\(seconds) differ from EN")
    else:
        if re.search(r"[\x00-\x09\x0b-\x1f]", pl) or "\n" in pl.rstrip("\n"):
            rep.error(where, "contains a real line break or control character; write line breaks as the two characters \\n")
        if len(pl.encode("utf-8")) > 4 * max(len(en.encode("utf-8")), 8) + 64:
            rep.warn(where, "is much longer than the original and may not fit in the game's space")
    if not pl.strip():
        rep.error(where, "is empty")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    quiet = "--quiet" in sys.argv
    root = args[0] if args else os.path.join(os.path.dirname(__file__), "..")
    rep = Report()
    stats = {}
    for kind, check in (("strings", check_string), ("cinematic", check_cinematic)):
        en_dir, pl_dir = os.path.join(root, kind, "EN"), os.path.join(root, kind, "PL")
        same = changed = 0
        for name in compare_dirs(en_dir, pl_dir, rep, kind):
            en = read(os.path.join(en_dir, name), rep, f"{kind}/EN/{name}")
            pl = read(os.path.join(pl_dir, name), rep, f"{kind}/PL/{name}")
            if en is None or pl is None:
                continue
            check(name, en, pl, rep)
            if en == pl:
                same += 1
            else:
                changed += 1
        stats[kind] = (changed, same + changed)

    if not quiet:
        for w in rep.warnings[:50]:
            print("warning:", w)
        if len(rep.warnings) > 50:
            print(f"... and {len(rep.warnings) - 50} more warnings")
    for e in rep.errors[:100]:
        print("error:", e)
    if len(rep.errors) > 100:
        print(f"... and {len(rep.errors) - 100} more errors")
    for kind, (c, total) in stats.items():
        print(f"{kind}: {c} of {total} files differ from EN ({100 * c / total:.0f}% translated)" if total else f"{kind}: no files")
    print(f"{len(rep.errors)} errors, {len(rep.warnings)} warnings")
    return 1 if rep.errors else 0


if __name__ == "__main__":
    sys.exit(main())
