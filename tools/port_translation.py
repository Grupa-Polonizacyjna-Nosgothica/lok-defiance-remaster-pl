#!/usr/bin/env python3
"""Port the existing Polish translation of the original 2003 game into the remaster's text files.

The original game's dialogue is in defiance-orig/raw-cine-text-{en,pl} (one subtitle line per line,
the PL file mirrors the EN file line by line). The remaster's texts are strings/EN, cinematic/EN
(and their PL copies). Where a remaster text is made of lines that also occur, unchanged, in the
original, the matching Polish lines are written into the PL file of the remaster text.

  port_translation.py [repo_root] [--dry-run] [--strict] [--force] [--report-dir DIR] [--min-ratio R]

Match levels, strongest first:
  exact       the same lines, only whitespace differs         -> applied
  normalized  the same lines apart from case and punctuation  -> applied (not with --strict)
  context     the line occurs in several originals with different translations, and the other
              lines of the same game record decide which one is meant -> applied
Everything else is not applied but listed for a manual check:
  close       similar text (edited in the remaster), several different translations (ambiguous),
              or the Polish original is not line-aligned with the English one (misaligned)
  unmatched   nothing similar in the original game

PL files that already differ from their EN file are never overwritten unless --force is given.
Reports are written to the report directory (default: port-report/).
"""
import argparse, collections, difflib, math, os, re, sys

def read_file(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


TIMING = re.compile(r"^\d+ \d+$")
MARKER = re.compile(r"\\\([0-9.]+\)")
PUA = re.compile("[-]")


# ------------------------------------------------------------------ normalisation

def norm0(s):
    return re.sub(r"\s+", " ", s).strip()


def norm1(s):
    s = s.replace("’", "'").replace("‘", "'").replace("“", '"').replace("”", '"')
    s = s.replace("…", "...")
    s = re.sub(r"[–—-]+", "-", s)   # – — - -- are all the same dash
    s = re.sub(r"\s*-\s*", " - ", s) if "-" in s else s
    return norm0(s).lower()


NORMS = (norm0, norm1)
LEVEL_NAMES = ("exact", "normalized")


# ------------------------------------------------------------------ the original game

def align_lines(en, pl):
    """Lines of a Polish original that was re-wrapped (a different number of lines than the English
    one): pair the lines by length, allowing one English line for two Polish ones and the reverse.
    Returns groups (en_from, en_to, pl_from, pl_to)."""
    n, m = len(en), len(pl)
    INF = 1e18
    dash = lambda l: l.strip().startswith("-")

    def length_cost(a, b, penalty):
        la = sum(len(x.strip()) for x in a) + 4
        lb = sum(len(x.strip()) for x in b) + 4
        d = math.log(lb / la) - math.log(1.1)
        return d * d * 4 + penalty

    cost = [[INF] * (m + 1) for _ in range(n + 1)]
    back = [[None] * (m + 1) for _ in range(n + 1)]
    cost[0][0] = 0
    for i in range(n + 1):
        for j in range(m + 1):
            if cost[i][j] >= INF:
                continue
            for di, dj in ((1, 1), (1, 2), (2, 1), (1, 0), (0, 1)):
                ni, nj = i + di, j + dj
                if ni > n or nj > m:
                    continue
                if (di, dj) == (1, 1):
                    add = length_cost(en[i:ni], pl[j:nj], 3.0 if dash(en[i]) != dash(pl[j]) else 0)
                elif di == 0 or dj == 0:
                    add = 6.0
                else:
                    add = length_cost(en[i:ni], pl[j:nj], 2.5)
                if cost[i][j] + add < cost[ni][nj]:
                    cost[ni][nj] = cost[i][j] + add
                    back[ni][nj] = (i, j)
    groups, i, j = [], n, m
    while (i, j) != (0, 0):
        pi, pj = back[i][j]
        groups.append((pi, i, pj, j))
        i, j = pi, pj
    return groups[::-1]


class Original:
    """One file of the original game. pl_for[i] is the Polish text of English line i, or None when it
    cannot be told (the translator merged it with a neighbour, or the file cannot be aligned)."""

    def __init__(self, name, en, pl):
        self.name = name
        self.en, self.pl = en, pl
        self.aligned = len(en) == len(pl)
        self.by_length = set()      # lines paired by length instead of by position
        if self.aligned:
            self.pl_for = list(pl)
        else:
            self.pl_for = [None] * len(en)
            groups = align_lines(en, pl) if abs(len(en) - len(pl)) <= 2 and en and pl else []
            odd = [g for g in groups if not (g[1] - g[0] == 1 and g[3] - g[2] == 1)]
            if groups and len(odd) <= 2:
                for a, b, c, d in groups:
                    if b - a == 1 and d - c == 1:
                        self.pl_for[a] = pl[c]
                        if a != c:                      # shifted by an earlier merge or split
                            self.by_length.add(a)
                    elif b - a == 1 and d - c == 2:     # one English line, two Polish lines
                        self.pl_for[a] = pl[c].rstrip() + " " + pl[c + 1].lstrip()
                        self.by_length.add(a)
                # the line before the odd group is paired by length too
                for a, b, c, d in odd:
                    if a > 0 and self.pl_for[a - 1] is not None:
                        self.by_length.add(a - 1)
        # An empty Polish line next to a non-empty English one means the translator dropped it
        for k, line in enumerate(self.pl_for):
            if line is not None and not line.strip() and en[k].strip():
                self.pl_for[k] = None
        self.keys = [[n(l) for l in en] for n in NORMS]


def split_lines(text):
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    return lines


def load_originals(root):
    base = os.path.join(root, "defiance-orig")
    out = []
    for dirpath, _, files in os.walk(os.path.join(base, "raw-cine-text-en")):
        for f in sorted(files):
            rel = os.path.relpath(os.path.join(dirpath, f), os.path.join(base, "raw-cine-text-en"))
            en = split_lines(read_file(os.path.join(dirpath, f)))
            pl_path = os.path.join(base, "raw-cine-text-pl", rel)
            pl = split_lines(read_file(pl_path)) if os.path.exists(pl_path) else []
            out.append(Original(rel.replace(os.sep, "/"), en, pl))
    return sorted(out, key=lambda o: o.name)


class Index:
    """Finds the places in the originals where a sequence of lines occurs."""

    def __init__(self, originals):
        self.originals = originals
        self.first = [collections.defaultdict(list) for _ in NORMS]   # level -> first line -> [(orig, line)]
        for o in originals:
            for lvl in range(len(NORMS)):
                for i, k in enumerate(o.keys[lvl]):
                    self.first[lvl][k].append((o, i))

    def find(self, lines, lvl):
        keys = [NORMS[lvl](l) for l in lines]
        hits = []
        for o, i in self.first[lvl].get(keys[0], []):
            if o.keys[lvl][i:i + len(keys)] == keys:
                hits.append((o, i))
        return hits


# ------------------------------------------------------------------ the remaster's texts

class Target:
    def __init__(self, kind, name, en):
        self.kind, self.name, self.en = kind, name, en    # kind: strings / crblock / textentry
        self.timed = any(TIMING.match(l) for l in en.split("\n"))
        self.lines = self.text_lines()

    def text_lines(self):
        if self.kind == "textentry":
            return []
        if self.kind == "crblock":
            return [self.en]
        # in a timed text the empty lines are only spacing around the timing lines
        return [l for l in self.en.split("\n") if not TIMING.match(l) and not (self.timed and not l.strip())]

    def comparable(self):
        return [l for l in self.lines]

    def render(self, pl_lines):
        """The PL text: same structure as the EN text (timing lines, spacing), new words."""
        if self.kind == "crblock":
            return pl_lines[0].strip()
        out, k = [], 0
        for l in self.en.split("\n"):
            if TIMING.match(l) or (self.timed and not l.strip()):
                out.append(l)
            else:
                lead = l[:len(l) - len(l.lstrip())]
                trail = l[len(l.rstrip()):]
                out.append(lead + pl_lines[k].strip() + trail if l.strip() else l)
                k += 1
        return "\n".join(out)


def load_targets(root):
    targets = []
    for kind, folder in (("strings", "strings"), ("crblock", "cinematic"), ("textentry", "cinematic")):
        d = os.path.join(root, folder, "EN")
        for name in sorted(os.listdir(d)):
            if kind == "strings" or name.startswith(kind):
                targets.append(Target(kind, f"{folder}/{{L}}/{name}", read_file(os.path.join(d, name))))
    return targets


# ------------------------------------------------------------------ matching

class Result:
    def __init__(self, target):
        self.target = target
        self.status = "unmatched"   # applied / close / unmatched
        self.level = None           # exact / normalized / context
        self.reason = ""
        self.source = None          # (original, first line)
        self.pl_lines = None
        self.candidates = []        # [(original, line, ratio)] for the report


def pl_window(o, i, n):
    window = o.pl_for[i:i + n]
    return tuple(window) if None not in window else None


def decide(target, hits):
    """Choose from the places where the lines occur. Returns (status, hit, reason)."""
    n = len(target.lines)
    usable = [(o, i) for o, i in hits if pl_window(o, i, n) is not None]
    if not usable:
        return "close", hits[0], "the Polish original merges these lines with a neighbouring line (or cannot be aligned with the English one)"
    plans = {pl_window(o, i, n) for o, i in usable}
    if len(plans) == 1:
        return "ok", usable[0], ""
    return "ambiguous", None, f"{len(plans)} different translations in {len({o.name for o, _ in usable})} original files"


def match_all(targets, index):
    results = {t.name: Result(t) for t in targets}
    pending = []
    for t in targets:
        r = results[t.name]
        if t.kind == "textentry" or not t.lines or not any(l.strip() for l in t.lines):
            continue
        for lvl in (0, 1):
            hits = index.find(t.lines, lvl)
            deduced = False
            if hits and t.kind == "strings" and not t.timed:
                # An interface text ("Kain", "Close") may also be a line inside some dialogue. A whole
                # original file is a safe match; a line inside a longer text is applied too, but logged
                # as deduced so that it gets reviewed
                whole = [(o, i) for o, i in hits if len(o.en) == len(t.lines)]
                if whole:
                    hits = whole
                else:
                    deduced = True
            if hits:
                status, hit, reason = decide(t, hits)
                r.level = LEVEL_NAMES[lvl]
                r.candidates = [(o, i, 1.0) for o, i in hits]
                if status == "ok":
                    r.status, r.source = "applied", hit
                    r.pl_lines = list(pl_window(hit[0], hit[1], len(t.lines)))
                    if deduced:
                        r.level = "deduced"
                    elif any(k in hit[0].by_length for k in range(hit[1], hit[1] + len(t.lines))):
                        r.level = "length-aligned"
                else:
                    r.status, r.reason = "close", reason
                    if status == "ambiguous":
                        pending.append((r, lvl, hits))
                break
    resolve_by_context(results, index, pending)
    return results


def resolve_by_context(results, index, pending):
    """A line like "-- Yes..." occurs in many originals. The other blocks of the same game record
    (crblock_<hash>_<k>) are the neighbouring lines of one scene; use them to find which original."""
    by_record = collections.defaultdict(list)
    for r in results.values():
        if r.target.kind == "crblock":
            m = re.match(r"cinematic/\{L\}/crblock_([0-9a-f]+)_(\d+)\.txt", r.target.name)
            by_record[m.group(1)].append((int(m.group(2)), r))
    for r, lvl, hits in pending:
        if r.target.kind != "crblock":
            continue
        record = sorted(by_record[re.match(r"cinematic/\{L\}/crblock_([0-9a-f]+)_", r.target.name).group(1)], key=lambda x: x[0])
        votes = collections.Counter()
        for _, other in record:
            for name in {o.name for o, _, _ in other.candidates}:
                votes[name] += 1
        if not votes:
            continue
        ranked = votes.most_common()
        if len(ranked) > 1 and ranked[0][1] == ranked[1][1]:
            continue
        best = ranked[0][0]
        mine = [(o, i) for o, i in hits if o.name == best and pl_window(o, i, 1) is not None]
        if mine and len({pl_window(o, i, 1) for o, i in mine}) == 1:
            o, i = mine[0]
            r.status, r.level, r.source = "applied", "context", (o, i)
            r.pl_lines = list(pl_window(o, i, 1))
            r.reason = f"chosen by the other lines of the record (original {best})"


def words(s):
    return set(re.findall(r"\w+", norm1(s)))


def fuzzy(results, originals, min_ratio):
    """Similar text for what has no (usable) match: slide windows of the original over the text."""
    wordmap = collections.defaultdict(set)
    for oi, o in enumerate(originals):
        for i, l in enumerate(o.en):
            for w in words(l):
                wordmap[w].add((oi, i))
    cache = {}

    def search(lines):
        text = norm1(" ".join(lines))
        key = (text, len(lines))
        if key in cache:
            return cache[key]
        tw = words(text)
        found = []
        if len(tw) >= 2:
            seeds = collections.Counter()
            for w in tw:
                hits = wordmap.get(w, ())
                if len(hits) <= 400:               # skip very common words
                    for oi, i in hits:
                        seeds[(oi, i)] += 1
            n = len(lines)
            tried = set()
            for (oi, i), c in seeds.most_common(25):
                if c * 2 < len(tw):
                    break
                o = originals[oi]
                for start in range(max(0, i - n), i + 1):
                    for length in {max(1, n - 1), n, n + 1}:
                        if (oi, start, length) in tried:
                            continue
                        tried.add((oi, start, length))
                        window = o.en[start:start + length]
                        if not window:
                            continue
                        sm = difflib.SequenceMatcher(None, text, norm1(" ".join(window)), autojunk=False)
                        if sm.real_quick_ratio() >= min_ratio and sm.quick_ratio() >= min_ratio:
                            ratio = sm.ratio()
                            if ratio >= min_ratio:
                                found.append((ratio, oi, start, length))
            found.sort(reverse=True)
        cache[key] = [(originals[oi], start, length, ratio) for ratio, oi, start, length in found[:3]]
        return cache[key]

    for r in results.values():
        t = r.target
        if r.status != "unmatched" or t.kind == "textentry" or not t.lines:
            continue
        cands = search(t.lines)
        if cands:
            r.status, r.level = "close", "similar"
            r.reason = f"similar text, best ratio {cands[0][3]:.2f}"
            r.candidates = cands
            join_rewrapped(r)


def join_rewrapped(r):
    """The remaster has one line where the original wrapped the same words over several. When every
    original with those words (at least two) has the same Polish text, take it as one line."""
    if r.target.kind == "textentry" or len(r.target.lines) != 1:
        return
    exact = [c for c in r.candidates if len(c) == 4 and c[3] >= 0.9999]
    if len({c[0].name for c in exact}) < 2:
        return
    texts = set()
    for o, start, length, _ in exact:
        window = o.pl_for[start:start + length]
        if None in window:
            return
        texts.add(" ".join(l.strip() for l in window))
    if len(texts) == 1:
        r.status, r.level = "applied", "joined"
        r.source = (exact[0][0], exact[0][1])
        r.pl_lines = [texts.pop()]
        r.reason = ""


def fuzzy_textentries(results, originals, min_ratio):
    for r in results.values():
        t = r.target
        if t.kind != "textentry":
            continue
        text = norm1(MARKER.sub(" ", t.en))
        best = (0, None)
        for o in originals:
            ratio = difflib.SequenceMatcher(None, text, norm1(" ".join(o.en)), autojunk=False).ratio()
            if ratio > best[0]:
                best = (ratio, o)
        if best[0] >= min_ratio:
            r.status, r.level = "close", "similar"
            r.reason = f"subtitle record with timing markers; same text as {best[1].name} (ratio {best[0]:.2f}), timings need manual work"
            r.candidates = [(best[1], 0, len(best[1].en), best[0])]


# ------------------------------------------------------------------ applying and reporting

def apply_results(root, results, strict, force, dry):
    applied = skipped = 0
    for r in results.values():
        if r.status != "applied":
            continue
        if strict and r.level != "exact":
            r.status, r.reason = "close", "differs from the original in case or punctuation (--strict)"
            continue
        en_path = os.path.join(root, r.target.name.replace("{L}", "EN"))
        pl_path = os.path.join(root, r.target.name.replace("{L}", "PL"))
        current = read_file(pl_path) if os.path.exists(pl_path) else None
        if current is not None and current != r.target.en and not force:
            r.status, r.reason = "skipped", "the PL file is already translated"
            skipped += 1
            continue
        new = r.target.render(r.pl_lines)
        if not dry:
            with open(pl_path, "w", encoding="utf-8", newline="") as f:
                f.write(new)
        applied += 1
    return applied, skipped


def skip_translated(root, results, force):
    """Move close/unmatched texts whose PL file already differs from EN out of those reports."""
    if force:
        return
    for r in results.values():
        if r.status not in ("close", "unmatched"):
            continue
        pl_path = os.path.join(root, r.target.name.replace("{L}", "PL"))
        if os.path.exists(pl_path) and read_file(pl_path) != r.target.en:
            r.status, r.reason = "skipped", "the PL file is already translated"


def short(text, n=110):
    text = text.replace("\n", " | ")
    return text if len(text) <= n else text[:n - 3] + "..."


def write_reports(out, results, originals, applied_total):
    os.makedirs(out, exist_ok=True)
    by = collections.defaultdict(list)
    for r in results.values():
        by[(r.status, r.level)].append(r)

    def kinds(rs):
        c = collections.Counter(r.target.kind for r in rs)
        return ", ".join(f"{v} {k}" for k, v in sorted(c.items())) or "-"

    with open(os.path.join(out, "summary.md"), "w", encoding="utf-8") as f:
        f.write("# Porting the original translation: summary\n\n")
        f.write(f"{len(results)} remaster texts examined.\n\n| Result | Count | Of which |\n|---|---|---|\n")
        for status, level, label in (("applied", "exact", "applied, exact match"), ("applied", "normalized", "applied, case/punctuation differences only"),
                                     ("applied", "context", "applied, chosen by the neighbouring lines"),
                                     ("applied", "deduced", "applied, deduced from a longer original text (please review)"),
                                     ("applied", "joined", "applied, original wrapped over several lines (joined)"), ("skipped", "exact", "skipped (already translated)"),
                                     ("close", None, "listed in close.md"), ("unmatched", None, "listed in unmatched.md")):
            rs = [r for (s, l), v in by.items() if s == status and (level is None or l == level) for r in v] if level is None \
                else by.get((status, level), [])
            if status == "skipped":
                rs = [r for r in results.values() if r.status == "skipped"]
            f.write(f"| {label} | {len(rs)} | {kinds(rs)} |\n")
        f.write("\n## Applied, deduced from a longer original text (please review)\n\n")
        for r in sorted((r for r in results.values() if r.status == "applied" and r.level == "deduced"), key=lambda r: r.target.name):
            o, i = r.source
            f.write(f"- `{r.target.name.replace('{L}', 'PL')}`: {short(' / '.join(r.target.lines), 60)}  ->  {short(' / '.join(r.pl_lines), 60)}  (`{o.name}` line {i + 1} of {len(o.en)})\n")
        f.write("\n## Applied with case/punctuation differences only (please skim)\n\n")
        norm = sorted((r for r in results.values() if r.status == "applied" and r.level == "normalized"), key=lambda r: r.target.name)
        for r in norm[:300]:
            o, i = r.source
            f.write(f"- `{r.target.name.replace('{L}', 'EN')}`: {short(' / '.join(r.target.lines), 70)}  <-  `{o.name}` line {i + 1}: {short(' / '.join(o.en[i:i + len(r.target.lines)]), 70)}\n")
        if len(norm) > 300:
            f.write(f"- ... and {len(norm) - 300} more (see applied.tsv)\n")

    with open(os.path.join(out, "applied.tsv"), "w", encoding="utf-8") as f:
        f.write("target\tlevel\toriginal file\tfirst line\n")
        for r in sorted(results.values(), key=lambda r: r.target.name):
            if r.status == "applied":
                f.write(f"{r.target.name.replace('{L}', 'PL')}\t{r.level}\t{r.source[0].name}\t{r.source[1] + 1}\n")

    with open(os.path.join(out, "close.md"), "w", encoding="utf-8") as f:
        f.write("# Close or uncertain matches: check these by hand\n\n")
        f.write("Nothing here was applied. For each text: the remaster's English, the closest original English and the original Polish.\n\n")
        close = sorted((r for r in results.values() if r.status == "close"), key=lambda r: (r.reason.split(" ")[0], r.target.name))
        for r in close:
            f.write(f"### `{r.target.name.replace('{L}', 'PL')}`\n\n{r.reason}\n\n")
            f.write(f"- EN (remaster): {short(r.target.en, 300)}\n")
            for cand in r.candidates[:3]:
                o, start = cand[0], cand[1]
                length = cand[2] if len(cand) == 4 else len(r.target.lines)
                ratio = f" ratio {cand[3]:.2f}" if len(cand) == 4 else ""
                f.write(f"- original `{o.name}` lines {start + 1}-{start + length}{ratio}\n")
                f.write(f"  - EN: {short(' / '.join(o.en[start:start + length]), 300)}\n")
                if o.pl:
                    f.write(f"  - PL: {short(' / '.join(o.pl[start:start + length]), 300)}\n")
            f.write("\n")

    with open(os.path.join(out, "unmatched.md"), "w", encoding="utf-8") as f:
        f.write("# No match in the original game: translate from scratch\n\n")
        un = [r for r in results.values() if r.status == "unmatched"]
        for kind in ("strings", "crblock", "textentry"):
            rs = sorted((r for r in un if r.target.kind == kind), key=lambda r: r.target.name)
            f.write(f"## {kind} ({len(rs)})\n\n")
            for r in rs:
                f.write(f"- `{r.target.name.replace('{L}', 'EN').split('/')[-1]}`: {short(r.target.en, 120)}\n")
            f.write("\n")

    used = collections.defaultdict(set)
    for r in results.values():
        if r.status == "applied":
            o, i = r.source
            used[o.name].update(range(i, i + len(r.target.lines)))
        for cand in r.candidates:
            o, start = cand[0], cand[1]
            used[o.name].update(range(start, start + (cand[2] if len(cand) == 4 else len(r.target.lines))))
    with open(os.path.join(out, "unused-originals.md"), "w", encoding="utf-8") as f:
        f.write("# Original (2003) files whose lines are not used by any remaster text\n\n"
                "Their Polish translation exists but nothing in the remaster matched it yet.\n\n")
        for o in originals:
            left = [i for i in range(len(o.en)) if i not in used[o.name]]
            if left:
                f.write(f"- `{o.name}`: {len(left)} of {len(o.en)} lines unused" + (f", e.g. {short(o.en[left[0]], 80)}" if o.en[left[0]].strip() else "") + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("root", nargs="?", default=os.path.join(os.path.dirname(__file__), ".."))
    ap.add_argument("--dry-run", action="store_true", help="report only, change no file")
    ap.add_argument("--strict", action="store_true", help="apply exact matches only")
    ap.add_argument("--force", action="store_true", help="also overwrite PL files that are already translated")
    ap.add_argument("--report-dir", default=None)
    ap.add_argument("--min-ratio", type=float, default=0.75, help="similarity needed to list a close match")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    report = args.report_dir or os.path.join(root, "port-report")

    originals = load_originals(root)
    targets = load_targets(root)
    print(f"{len(originals)} original files, {len(targets)} remaster texts")
    results = match_all(targets, Index(originals))
    fuzzy(results, originals, args.min_ratio)
    fuzzy_textentries(results, originals, args.min_ratio)
    apply_results(root, results, args.strict, args.force, args.dry_run)
    skip_translated(root, results, args.force)
    write_reports(report, results, originals, 0)

    counts = collections.Counter((r.status, r.level) for r in results.values())
    for key, n in sorted(counts.items(), key=lambda kv: (kv[0][0], str(kv[0][1]))):
        print(f"{key[0]:10} {str(key[1]):11} {n}")
    print(f"reports in {report}" + (" (dry run, no PL file changed)" if args.dry_run else ""))


if __name__ == "__main__":
    main()
