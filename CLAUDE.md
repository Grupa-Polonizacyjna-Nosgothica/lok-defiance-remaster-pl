# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

An unofficial Polish translation (by Grupa Polonizacyjna Nosgothica) of *Legacy of Kain: Defiance Remastered*. It is mostly translation text plus C++ tools that extract the game's texts from `bigfile.x64.dat` and patch the translation back over the Russian language. There is no test suite or linter. The README's status note about patching being unsolved is out of date; patching is implemented but not yet tried in the game. Docs are written in Polish (and English in `README.md`).

## Layout

- `strings/EN/NNNN.txt`, `strings/PL/NNNN.txt`: UI/menu/game strings, one string per file, 3908 each, re-extracted from the current game build (`strings/PL` started as an exact copy of `strings/EN`). The 4-digit number is the string's index in the game's locale block, so EN and PL must stay index-aligned. These files are raw UTF-8 without BOM and usually have no trailing newline (the extractor writes bytes exactly as stored). Keep that when editing.
- `defiance-orig/raw-cine-text-{en,pl}/<chapter>/<scene>.txt`: cinematic/dialogue text, grouped by chapter (`00_Intro` … `13_Avernus_Catacombs`, plus `Inventory` and `Miscellaneous`). The `en` and `pl` trees mirror each other file for file. Keep paths identical when adding or renaming.
- `cinematic/EN`, `cinematic/PL`: texts of TextEntry records (`textentry_<hash>.txt`) and CR language blocks (`crblock_<hash>_<k>.txt`) extracted from `bigfile.x64.dat`; `PL` starts as a copy of `EN`. Raw text: `\n` stays two characters, files have no trailing newline, and each block's translation must fit the original byte space (see `docs/patching.md`).
- `fonts/`: only the *delta* of the Latin fonts (new glyph pixels and table lines for the Polish letters), applied by the patcher to the user's own files so no game data is stored here (see `docs/fonts.md`). Edit `font<N>.overlay.png`, then `python3 tools/fonts/polish_glyphs.py build fonts`.
- `tools/`: C++17 tools built by `tools/CMakeLists.txt`: `common/` (index, locals.bin, TextEntry and language block code), `extractor/` (`LocaleExtractor`) and `patcher/` (`PolishPatcher`). `tools/bigfile/` holds the archive index notes and a Python index dumper, `tools/textures/` lists/exports the game's `!WAR` textures and `tools/fonts/` generates the Polish glyphs (both Python with Pillow and numpy).
- `tools/deps/DDStractor`: git submodule (`Nidrax/DDStractor`). Run `git submodule update --init` to fetch it. It is not referenced by `tools/CMakeLists.txt` yet.
- `installer/`: installer assets and `INSTALL.txt` (end-user instructions). The game's language is set to Russian because the translation replaces the RU locale.
- `GameData/` is gitignored. It is the local work dir for extracted game files.

## Commands

Build both tools (CMake output dir `build` is gitignored):

```
cmake -S tools -B build && cmake --build build
./build/LocaleExtractor [bigfile.x64.dat] [output_root]            # writes <root>/strings/EN and <root>/cinematic/EN
./build/PolishPatcher patch|restore [bigfile.x64.dat] [repo_root|polish.pak]   # see docs/patching.md
./build/PolishPatcher check [bigfile.x64.dat]                       # state/build/running-game report, changes nothing
./build/PolishPatcher pack <repo_root> polish.pak                   # the package shipped to players
```

The extractor takes about 6 s on the real 11 GB file. Do not run `patch` on the only copy of your game files without knowing `restore` exists (a `.plbackup` is written next to the file).

## Tools architecture (`tools/`)

`bigfile.x64.dat` is memory-mapped (`MappedFile`). The archive starts with its own index (format in `tools/bigfile/FORMAT.md`, reverse-engineered from `defiance.exe`; `BigfileIndex`): a header with the entry count, sorted CRC hashes of lowercased paths, then 0x18-byte entries holding size and absolute offset. `LocaleHandler` hashes `PC<Language>\locals.bin`, looks it up and parses it with `LocalesBin`. `CinematicExporter` walks every record for `TextEntry` and `CrBlocks`. The patcher reuses the same classes to write text back (`docs/patching.md`); `docs/locals-and-text-records.md` describes the three text formats.

`locals.bin` layout: u32 locale id, u32 `count` (4231), then `count` u32 offsets (from byte 8) indexed by string ID, then the NUL-terminated strings. Offset 0 means an empty string (323 IDs) and several IDs share one string. `LocalesBin::Parse` keeps the distinct strings in stored order and the ID-to-string mapping; the extractor writes each string to its own `NNNN.txt`, so file numbers are positions in that order, not IDs. See `docs/locals-and-text-records.md` for the ID/position mapping and the strings that must not be translated.

`tools/bigfile/bigfile_index.py <dat>` dumps the whole index as CSV. Little-endian reads and the path hash live in `common/BinaryUtils.hpp`.

How the old cinematic text maps to these strings (timing lines, markup characters) is described in `docs/cinematic-text-vs-strings.md`.

The game is not expected to receive further updates, so string indices should stay stable.

## Commit rules

Keep the commit message brief and to the point. ***Use `Assisted-By` instead of `Co-Authored-By` when marking AI-assisted commits***, (e.g. `Assisted-By: Claude Sonnet 5.5 <noreply@anthropic.com>`).
