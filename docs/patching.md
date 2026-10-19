# Patching the translation into the game

`PolishPatcher` (built from `tools/patcher`) writes the Polish text over the **Russian** language of `bigfile.x64.dat`, so the player selects Russian in the game launcher. `bigfilehd.dat` is not touched.

## Usage

```
cmake -S tools -B build && cmake --build build
./build/PolishPatcher patch   <game>/bigfile.x64.dat <repo root | polish.pak>
./build/PolishPatcher restore <game>/bigfile.x64.dat
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

## Backup

Before the first write, `bigfile.x64.dat.plbackup` is saved next to the game file: original size, original index and the original bytes of every region patched in place. `restore` writes them back and cuts the file to its original size. Tested on a copy of the real file: restore gave a byte-identical file. If the game files are verified or reinstalled through Steam/GOG, delete the stale `.plbackup`.

## Limits and untested points

- **Not tested in the game.** The patched file was checked structurally only (index, parsing, in-place diffs, restore). The idea that the game accepts relocated records comes from the third-party tool.
- Blocks whose translation is longer than the whole block (all languages but English cut down) are not patched and stay Russian; the patcher reports them.
- Other languages' blocks lose their text when space is needed, which does not matter while the game runs in Russian.
- Polish letters come from our modified Latin fonts (`docs/fonts.md`); the generated glyphs are a first version and nothing has been seen in the game yet.
- Files in `strings/PL` must match the count of `strings/EN`. 
- The installer text (`installer/INSTALL.txt`) still describes copying files and has to be updated for this flow.
