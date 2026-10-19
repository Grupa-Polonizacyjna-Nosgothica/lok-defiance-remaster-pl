# Patching the translation into the game

`PolishPatcher` (built from `tools/patcher`) writes the Polish text over the **Russian** language of `bigfile.x64.dat`, so the player selects Russian in the game launcher. `bigfilehd.dat` is not touched.

## Usage

```
cmake -S tools -B build && cmake --build build
./build/PolishPatcher patch   <game>/bigfile.x64.dat <repo root | polish.pak>
./build/PolishPatcher restore <game>/bigfile.x64.dat
./build/PolishPatcher check   <game>/bigfile.x64.dat      # reports state, build, running game, write access; changes nothing
./build/PolishPatcher pack    <repo root> polish.pak
```

`patch` first undoes an earlier patch, so it can be run repeatedly. It prints how many strings, subtitle records and language blocks it wrote, lists problems (first 20) and re-reads the file to verify `locals.bin`.

## What it does

| Text | How it is patched |
|---|---|
| `PCRussian\locals.bin` | A new block (English string IDs, `strings/PL` texts, Russian locale id) is appended at the end of the file, aligned to 0x800, and the index entry's size and offset are changed. No size limit. |
| `TextEntry` records | The `lang_russian` body is replaced, the record appended, and every index slot with that hash re-pointed. No size limit. |
| Fonts | The font deltas of `fonts/` are applied to the user's own `PCEnglish\font1.raw`/`font2.raw`, `\SR3\image\font1.txt`/`font2.txt` and `char_casing.txt`, and the results appended (all index slots). See `docs/fonts.md`. |
| Language blocks | Written in place into the Russian entry, **keeping the block length**. If the text is longer than the Russian slot, space is taken from other languages (never English), which get cut down to a single space. A block without a Russian entry reuses the largest other language and its digit becomes 8. |

Trailing newlines in `cinematic/PL` files are dropped, and real line breaks become the literal `\n`. Internal strings (see `docs/locals-and-text-records.md`) are always kept as in English. Untranslated files (identical to EN) simply put English into the Russian slot.

## The translation package

`patch` reads either a directory laid out like the repository (`strings/PL`, `cinematic/PL`, `fonts`) or one `.pak` file. `pack` builds that file: it holds only our texts and the font deltas (27,172 files, about 2.4 MB), never any game data, and every entry has a CRC-32 that is checked when it is read. This is what the installer ships. Format: `"LOKPLPK1"`, u32 count, then per file u16 name length, name, u64 size, u32 CRC-32, bytes. A directory source and a package of it give byte-identical patched files (tested).

## Checks before patching

`patch` refuses to touch the file unless these pass (the exit code tells an installer which one failed; `--force` skips the first three and ignores a changed-file backup):

| Check | Exit code |
|---|---|
| **Game build.** `bigfile.x64.dat` must have 18,802 index entries and an English `locals.bin` of 465,568 bytes with CRC-32 `0x51E16C91` (the Steam build we analysed). The patch never changes the English texts, so a patched file passes as well. | 3 |
| **Game running.** No process named `defiance.exe` (Windows, or Proton/Wine on Linux). | 4 |
| **Disk space.** Appended data, backup and a 16 MB margin must fit on the game's drive. | 5 |
| **Changed file** (see below). | 6 |
| **Write access** to the game file. | 7 |

Other failures exit with 2, bad usage with 1.

## Backup

Before the first write, `bigfile.x64.dat.plbackup` is saved next to the game file: original size, original index and the original bytes of every region patched in place. When the patch is complete, the file size and a CRC-32 of the patched index are added to it. The backup is now checked against the file on every run:

| State of the file | Meaning | `patch` / `restore` |
|---|---|---|
| matches the patched state | normal | restores, then patches again |
| matches the original state | the game client already restored it (e.g. file verification) | drops the stale backup |
| backup has no patched state | a patch was interrupted | restores the original |
| anything else | the file was changed since (game update, another tool) | refuses (exit 6); `--force` drops the backup |

`restore` writes the original bytes back and cuts the file to its original size. Tested on a copy of the real file: restore gave a byte-identical file. After a verification or reinstall through Steam/GOG the stale backup is recognised and dropped automatically, so nothing has to be deleted by hand.

## Tests and CI

- `ctest --test-dir build` runs `PatcherTests`: it builds a small synthetic archive (index, English and Russian `locals.bin`, a text entry stored in two slots, a binary record with two language blocks, a font atlas and tables) and checks patching, in-place block edits, the font delta, restore (byte-identical), re-patching, the package (same result as a directory, corruption detected) and the backup states. It needs no game data, so it also runs on Windows in CI.
- `python3 tools/validate_translation.py` checks `strings/PL` and `cinematic/PL` against the EN originals: same files, unchanged timing lines and `\(seconds)` markers, same format placeholders, internal names untouched, valid UTF-8 without BOM, no real line breaks in language blocks. It also prints how much is translated.
- `.github/workflows/build.yml` runs the validator, then builds, tests and smoke-tests `PolishPatcher` on Windows (MSVC, static runtime, no redistributable needed) and Linux, builds `polish.pak` and uploads everything as artifacts. Pushing a tag `v*` also publishes them as a GitHub release.
- The CI has not run yet (nothing pushed); the Windows build in particular has never been compiled.

## Limits and untested points

- **Not tested in the game.** The patched file was checked structurally only (index, parsing, in-place diffs, restore). The idea that the game accepts relocated records comes from the third-party tool.
- Blocks whose translation is longer than the whole block (all languages but English cut down) are not patched and stay Russian; the patcher reports them.
- Other languages' blocks lose their text when space is needed, which does not matter while the game runs in Russian.
- Polish letters come from our modified Latin fonts (`docs/fonts.md`); the generated glyphs are a first version and nothing has been seen in the game yet.
- Files in `strings/PL` must match the count of `strings/EN`. 
- The Windows build, including the process check and the handling of the 11 GB file, is exercised only by CI, which has not run yet.
- The free-disk-space check is written but could not be exercised on the test machine; the other checks were tested with real failures (wrong build, running process, read-only file, altered index, interrupted patch).
- The installer text (`installer/INSTALL.txt`) still describes copying files and has to be updated for this flow.
