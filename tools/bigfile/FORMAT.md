# bigfile.x64.dat / bigfilehd.dat index format

Derived statically from `GameData/defiance.exe` (sha256 `fc6a406f…daddc`, Ghidra 12.1.2). Addresses are image VAs. Checked against the real `bigfile.x64.dat` (18802 entries) and `bigfilehd.dat` (7960 entries): hashes are sorted, no entry runs past end of file, every offset is a multiple of 0x800, and the hashes of known paths resolve to sensible entries.

## Key finding

The exe contains no table of offsets. Both archives carry their own index at the start of the file, and the exe reads it at startup and looks entries up by path hash. The offset map therefore has to be read from the `.dat` files (`bigfile_index.py`), not from the exe.

## Loading sequence (`FUN_14019dae0`, the main entry, 0x14019dae0)

1. Open `bigfilehd.dat` (`FUN_1400025f0`). If it exists, read `tex.map` from it. If it is missing, `tex.map` is read as a loose file instead.
2. Open `bigfile.x64.dat` (`FUN_1400025f0`). If that fails: `Can't load big file named "%s"`.
3. Choose a language and look up `PC<Language>\locals.bin` in `bigfile.x64.dat`. If the chosen language is absent, try each of the 9 languages in the table at 0x1404d06a0. Failing all of them: `Could Not Load Localization Table`.
4. All later asset loads go through `FUN_140085410` (load queue of 0x80 slots). A missing path gives `Can't read from bigfile: %s`.

## Index layout (`FUN_1400025f0`)

| Offset | Size | Meaning |
|---|---|---|
| 0x00 | u32 | Header: bits 0-15 = entry count `N`, bits 16-23 and 24-31 are two extra bytes that the exe stores but did not show using in the code read so far |
| 0x04 | `N × u32` | Sorted CRC hashes of path names |
| 0x04 + 4N | `N × 0x18` | Entries, in the same order as the hashes |

Entry (`FUN_140002890` returns `entries + idx*0x18`):

| Field | Type | Meaning |
|---|---|---|
| +0x00 | u64 | Size in bytes (`fread` length for `tex.map`) |
| +0x08 | u64 | Absolute file offset (the seek position for `tex.map`) |
| +0x10 | u64 | Copied into the load request; meaning unknown (flags? uncompressed size?) |

`FUN_140085410` rejects entries whose size or offset is 0.

## Name hash (`FUN_1400024e0`)

1. Copy the path to a 264-byte buffer and lowercase it with `_strlwr` (ASCII only, no slash normalisation: `\` and `/` hash differently).
2. CRC-32/BZIP2: polynomial `0x04C11DB7`, init `0xFFFFFFFF`, MSB-first, final NOT. For `"123456789"` this gives `0xFC891918`.
3. Lookup is a binary search over the sorted hash array.

## Resource names seen in the exe

Literal or format-string paths, with a label based on their extension or directory:

| Path | Kind |
|---|---|
| `tex.map` (in `bigfilehd.dat`) | Texture map table (entry stride 0x30 in memory) |
| `PC%s\locals.bin` | Localization string tables (`%s` = language name) |
| `misc/noise3d.bin` | 3D noise texture/blob |
| `video/%s.ogg` | Video |
| `\SR3\image\font%X.txt`, `font%d.txt`, `char_casing.txt`, `font.ini` | Font and glyph configuration |
| `\SR3\%s\navigation.bin` | Level navigation data |
| `\SR3\%s\objectlist.txt`, `objlist.dat` | Level object lists |
| `\SR3\%s\worldmap.ini`, `artifact_modal.ini` | Level or UI configuration |
| `\sr3\object\artifact\artifacts.ini` | Artifact definitions |

Saves (`chapter_save\chapterNN.dat`, `save%03d.dat`, `autosave.dat`, `settings.dat`, `profile.dat`, key/pad bindings) are not in the archive.

Most game assets (meshes, textures, audio, level files) are loaded by paths built at runtime, so they will not appear as literals. Names can only be recovered by hashing candidate paths, so `bigfile_index.py --names` takes a candidate list.

## Observed in the real files

- `PCEnglish\locals.bin` is at 0x489800 in `bigfile.x64.dat` (465568 bytes); `tex.map` is at 0x2492EC000 (269956 bytes) and `misc/noise3d.bin` at 0x24922C000 (786432 bytes) in `bigfilehd.dat`.
- Entry field +0x10 and both extra header bytes are 0 in every entry.
- 24 hashes in `bigfile.x64.dat` occur 9 times each with identical size and offset (e.g. `artifacts.ini`, `font.ini`, `char_casing.txt`). The count of 9 matches the number of locales, so the index probably has one copy per language. That is an inference. `bigfilehd.dat` has no duplicates.
- The English `locals.bin` starts `00 00 00 00 87 10 00 00`: locale id 0 followed by `0x1087`. The older extractor expected `0B 10 00 00` there and used hardcoded block offsets that no longer match this build, so it was probably written for an older build. `tools/extractor` now locates the blocks through the index instead.

## Not yet done

- Names for the remaining ~26,700 entries (only 9 distinct paths resolved).
- Meaning of header bytes 2 and 3 and of entry field +0x10 (always 0 here).
- Whether entries are stored compressed.
- The language-name table at 0x1404d06a8.
