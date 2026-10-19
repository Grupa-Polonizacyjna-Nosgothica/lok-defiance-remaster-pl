# Original cinematic text vs. remaster strings

How `defiance-orig/raw-cine-text-en/` (text from the 2003 game's `.mul` streams) relates to `strings/EN/` (the string table extracted from the remaster's `bigfile.x64.dat`). Numbers below come from comparing the previous `strings/EN` (3855 files, from an older game build) with the 327 raw files. `strings/` now holds the current build's 3908 strings, so indices quoted below (e.g. 428–433) refer to the old numbering and the counts may differ slightly.

## Raw cinematic text (`defiance-orig/raw-cine-text-en/`)

- 327 files, one per cutscene or subtitle stream (`.mul`, see `docs/unLoKable-wiki/File Format: MUL.md`), grouped by chapter folder: `00_Intro` … `13_Avernus_Catacombs`, plus `Inventory` (50) and `Miscellaneous` (45). File names are the original stream names (`kvm_str_02`, `rvm_cem_04`, …). `raw-cine-text-pl/` mirrors the tree with Polish text.
- Plain UTF-8, no BOM, LF line endings, with a trailing newline (324 of 327 files).
- No timing and no markup. Each line is one on-screen subtitle line, i.e. the line breaks are the original subtitle breaks.
- A line starting with `-- ` marks a speaker change inside a conversation (77 files). Only `00_Intro/intro.txt` has blank lines (paragraph breaks) and `–` en dashes in dialogue.

## Remaster strings (`strings/EN/NNNN.txt`)

One string per file; `NNNN` is its position in the `locals.bin` block (not a game string ID, see `CLAUDE.md`). UTF-8, no BOM, LF. Usually no trailing newline (46 strings do end with one).

### Timed subtitle strings (597 strings, indices 419–3287)

The remaster stores subtitles as text lines alternating with a timing line:

```
Moebius's attempt at stopping me \n
60 0\n
was not one of his better efforts. \n
150 0\n
When I found him \n
196 0\n
...
to offer a few surprises.
```

- Every text line except the last has a **trailing space**, followed by a line `<start> <0>`. The timing line comes *after* the text line it introduces. Observed: it is monotonic within a string.
- The first number is a start time in an unknown unit (largest value seen: 5244). The second number is `0` in all 2111 timing lines.
- 55 strings begin with an empty text line followed by the first timing line (`\n60 0\n…`), e.g. the intro.
- 2 strings have a wrapped continuation line with no timing line of its own (e.g. in the intro string, 2931).
- Stripping the timing lines and trailing spaces gives text equal to the raw file's text.

### Other strings

UI and menu text, setup dialog (index 0), gallery and credits captions, single voice lines, inventory item names and so on. Indices are not strictly grouped by type; for example single-line inventory strings are spread over 1888–3286.

Multi-paragraph strings (such as bonus and Dark Chronicle texts, e.g. 214, 2397) contain inline markup using Unicode private-use characters. Their exact meaning is inferred from context and not verified in the game:

| Characters | Likely meaning |
|---|---|
| `U+F8F4`–`U+F8FB` (e.g. `F8F4` before a heading, `F8F5` before emphasised text, `F8FB` before a quotation) | Start of a text style |
| `U+F8FF` | End of the current style (closes the markers above) |
| `U+E0xx` (e.g. `E045`, `E073`, `E06D`) | Inline controller or keyboard button glyphs ("Press  to cancel") |
| `%s` | Runtime substitution (key name) |

These must be preserved byte for byte in translations. `strings/PL` copies them.

## How raw files map to strings

Of the 327 raw files, after removing timing lines and normalising punctuation/case:

| Result | Files | Notes |
|---|---|---|
| Exact text match with one string | 265 | Includes all 95 `Inventory` and `Miscellaneous` single-string files, which map 1:1 |
| Match after normalising dashes, quotes, case, whitespace | 20 | Remaster uses `–`/`“”`/`’`, different capitalisation (`Spectral Realm`) |
| Near match (ratio ≥ 0.75, small edits) | 13 | Remaster text was edited: `Moebius'` → `Moebius's`, `Wheel of Fate!`, intro re-wrapped |
| No single string, but every line found | 29 | The raw file is **split across several consecutive strings** in the remaster, one per voice line or speaker (e.g. `shold2_1_2.txt` = strings 428–433) |

Consequences:

- The mapping is by text only; there is no shared identifier. Nothing in the raw file name appears in the strings.
- The relation is not 1:1 for conversations: one raw file may be several strings, and the same text can occur at several indices (33 raw files match more than one string, e.g. `sf2_strc_01.txt` at 418 and 439).
- The `-- ` speaker prefix is kept in strings (386 start with it) and the Polish raw text can be reused, but each timed string needs its timing lines re-inserted around the translated lines, one translated line per original line, since the timing line count must stay the same.
- Strings at indices below about 417 (setup, gallery, credits) and the many UI strings have no counterpart in the raw files.
