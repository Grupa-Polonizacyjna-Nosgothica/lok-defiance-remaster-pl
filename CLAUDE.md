# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

An unofficial Polish translation (by Grupa Polonizacyjna Nosgothica) of *Legacy of Kain: Defiance Remastered*. It is mostly translation text plus a small C++ tool that extracts the game's string tables. There is no test suite or linter. The README documents project status: extraction is done, but patching `bigfile.x64.dat` back with Polish text on the user's side is still unsolved. Docs are written in Polish (and English in `README.md`).

## Layout

- `strings/EN/NNNN.txt`, `strings/PL/NNNN.txt`: UI/menu/game strings, one string per file, 3908 each, re-extracted from the current game build (`strings/PL` started as an exact copy of `strings/EN`). The 4-digit number is the string's index in the game's locale block, so EN and PL must stay index-aligned. These files are raw UTF-8 without BOM and usually have no trailing newline (the extractor writes bytes exactly as stored). Keep that when editing.
- `defiance-orig/raw-cine-text-{en,pl}/<chapter>/<scene>.txt`: cinematic/dialogue text, grouped by chapter (`00_Intro` … `13_Avernus_Catacombs`, plus `Inventory` and `Miscellaneous`). The `en` and `pl` trees mirror each other file for file. Keep paths identical when adding or renaming.
- `tools/extractor/`: the `LocaleExtractor` C++17 tool. `tools/bigfile/` holds the archive index notes and a Python index dumper.
- `tools/deps/DDStractor`: git submodule (`Nidrax/DDStractor`). Run `git submodule update --init` to fetch it. It is not referenced by `tools/extractor/CMakeLists.txt` yet.
- `installer/`: installer assets and `INSTALL.txt` (end-user instructions). The game's language is set to Russian because the translation replaces the RU locale.
- `GameData/` is gitignored. It is the local work dir for extracted game files.

## Commands

Build the extractor (CMake output dir `build` is gitignored):

```
cmake -S tools/extractor -B build && cmake --build build
./build/LocaleExtractor [path/to/bigfile.x64.dat]   # defaults to ./bigfile.x64.dat
```

The output goes to an `output/` directory relative to the current directory. It must already exist, because `ExportBlock` does not create it. Currently `main()` only exports the EN locale (`locales.ExportBlock(Locale::EN, "output")`). `ExportBlocks(dir)` exports all locales into `dir/<LOCALE>/` and creates the subdirectories itself, but `dir` must exist.

## Extractor architecture (`tools/extractor/`)

`bigfile.x64.dat` is memory-mapped (`MappedFile`). The archive starts with its own index (format in `tools/bigfile/FORMAT.md`, reverse-engineered from `defiance.exe`): a header with the entry count, sorted CRC hashes of lowercased paths, then 0x18-byte entries holding size and absolute offset. `LocaleHandler` hashes `PC<Language>\locals.bin` for each of the 9 languages (names in `LOCALE_LANGUAGES`), binary-searches the hash table (`FindFile`) and copies the block out. It checks that the first u32 equals the locale id (`LOCALE_IDS`; CN is 9 and RU is 10, 7 and 8 are unused).

`ExtractStrings` takes the u32 at block offset 12 as the offset of the first string and reads consecutive NUL-terminated strings, writing each to its own `NNNN.txt`. The block also has an offset table (4230 u32s from offset 12, with duplicates) that maps string IDs to strings; the extractor ignores it and numbers strings by position.

`tools/bigfile/bigfile_index.py <dat>` dumps the whole index as CSV. Little-endian reads and the hash live in `BinaryUtils.hpp`.

How the old cinematic text maps to these strings (timing lines, markup characters) is described in `docs/cinematic-text-vs-strings.md`.

The game is not expected to receive further updates, so string indices should stay stable.
