# Porting the existing translation into the remaster texts

`defiance-orig/raw-cine-text-pl` holds the finished Polish translation of the 2003 game's dialogue. Much of that text is unchanged in the remaster, whose texts are in `strings/` and `cinematic/` (see `docs/cinematic-text-vs-strings.md`). `tools/port_translation.py` finds the unchanged text and copies the Polish lines into the `PL` files.

```
python3 tools/port_translation.py . --dry-run      # reports only, changes nothing
python3 tools/port_translation.py .                # writes the PL files, reports in port-report/
  --strict          apply exact matches only (not the case/punctuation ones)
  --force           also overwrite PL files that already differ from their EN file
  --min-ratio 0.75  similarity needed to list a "close" match
```

Run it again after editing: files whose PL differs from EN are skipped, so hand work is never overwritten. About 6 s. Tests: `python3 -m unittest discover -s tools`.

## How texts are matched

The unit is the **line**. A remaster text becomes a list of lines (a timed subtitle string: its text lines without the `60 0` timing lines; a `crblock` file: one line; a UI string: its lines). The tool looks for the same lines, in the same order, in the English originals and takes the Polish lines at the same positions; the remaster's structure (timing lines, trailing spaces) is kept. Original Polish files mirror the English ones line by line.

| Result | Meaning | Applied? |
|---|---|---|
| exact | same lines, only whitespace differs | yes |
| normalized | same lines apart from case, quotes and dashes (`Citadel` / `citadel`) | yes (not with `--strict`) |
| context | a short line (`-- Yes...`) occurs in many originals with different translations; the other lines of the same game record (`crblock_<hash>_<k>`) show which scene is meant | yes |
| length-aligned | the Polish original was re-wrapped (a different number of lines than the English), so lines after the change are paired by length | yes, **skim these** |
| close | similar text (the remaster edited the wording), several different translations, or the Polish merges the line with its neighbour | no, listed |
| unmatched | nothing similar in the original game | no, listed |

Rules that prevent wrong matches:

- An interface text that is not a subtitle (no timing lines), e.g. `Kain` or `Close`, is matched against whole original files (item names and the like) first. When the same words occur only as a line inside a longer original, the translation of that line is still applied, at level `deduced`, and listed in `summary.md` under "deduced from a longer original text" for review. Several different translations stay in `close.md`.
- An empty Polish line next to a non-empty English one counts as missing, not as a translation.
- Files whose line counts differ are aligned by character length (one English line for two Polish ones and the reverse); only up to two such changes per file, otherwise the file is left for manual work (`10_Avernus/rvm_missing_01.txt`).
- Subtitle records with `\(seconds)` markers (`textentry_*`) are never applied automatically: their timing has to be set by hand. The report names the original they resemble.

## Reports (`port-report/`, not committed)

| File | Content |
|---|---|
| `summary.md` | counts per result and kind, and the list of case/punctuation matches to skim |
| `applied.tsv` | every applied text, its level and the original file and line it came from |
| `close.md` | texts to check by hand: the remaster English, the closest original English and the original Polish |
| `unmatched.md` | texts with no counterpart: translate from scratch |
| `unused-originals.md` | original files with lines that no remaster text used |

## Things to know

- The Polish lines are used as the original game showed them. Its line breaks are not always the English ones, so a short English line can receive a longer or shorter Polish line (`Kain` / `Kain został`). That is faithful to the original subtitle slots.
- The same English line translated differently in two originals (typos included, e.g. `pozwólcier`) is reported as ambiguous with both versions; pick one.
- Results with the current data (dry run): about 22,600 of 27,167 remaster texts get a translation (21,052 exact, 224 normalized, 468 by context, 870 length-aligned); about 1,000 are listed as close; about 3,500 have no counterpart and need translating (mostly interface and gallery texts).
- A translation that is longer than the original English may not fit the space of a `crblock` in the game (see `docs/patching.md`).
