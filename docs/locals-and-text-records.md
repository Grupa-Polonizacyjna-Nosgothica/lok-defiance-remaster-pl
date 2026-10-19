# Where the game's text lives

All text of the remaster sits in `bigfile.x64.dat`, in three kinds of records. The knowledge below comes from reading the third-party [LoKDR_Text_Tool](https://github.com/N-o-o-B-HU/LoKDR_Text_Tool) and re-checking it against the real archive with our own code (`tools/common`).

## 1. `locals.bin` (UI, menus, long subtitles)

Path `PC<Language>\locals.bin` (hash: see `tools/bigfile/FORMAT.md`). Layout: `u32 locale id`, `u32 count` (4231), `count × u32` string offsets starting at byte 8, then the strings. The table is indexed by **string ID**; 323 IDs have offset 0 (empty) and some IDs share a string. The extractor numbers the distinct strings by their position (3908 files in `strings/`), which is not the ID. Timed subtitle strings are described in `docs/cinematic-text-vs-strings.md`.

**Internal strings must not be translated.** IDs 3114–3283 and 4103–4106 are music and ambience names (`Sarafan_battle_01`, `Dark`, `Wind`, …), and strings made only of letters, digits and underscores (`no_savefile`, `Characters_Kain`, `dark_SR2`, …) are keys. Translating them breaks the game (e.g. "Cannot load music files for the '…' music"). The third-party tool only protected 3120–3283, which misses about 40 of them. `IsProtectedString` in `tools/common/Locales.hpp` has the full rule, and the patcher keeps those strings as in English.

## 2. `TextEntry` records (opening cinematic subtitles)

Three records (each listed 9 times in the index, once per language copy), plain text with CR LF line breaks:

```
Version 1
TextEntry
Line0
lang_english 2003-10-20 15:57:01:00 "\(1.8)Now, at last, \(3.4)the masks had fallen away. ..."
lang_russian 2026-02-06 13:21:18:97 "\(1.8)..."
...
```

One `lang_<language>` line per language, the body quoted, `\(seconds)` timing markers, literal `\n` for line breaks. The English line is dated 2003, so it is the original game's text. They can be rebuilt and appended freely.

## 3. Language blocks inside binary records (short lines)

3114 binary records (cutscene and gameplay data) contain CR-delimited multi-language blocks, 23,256 in total:

```
7\r<Chinese>\r0\rSurround him!\r1\rCernez-le !\r2\r...\r8\r<Russian>\r4\rRodeadlo!\r
```

Digit = language (**0 English, 1 French, 2 German, 3 Italian, 4 Spanish, 5 Japanese, 6 Portuguese, 7 Chinese, 8 Russian**), any order. Facts verified on the data:

- Blocks sit inside other binary data, so their **total length cannot change**. The patcher rewrites them in place.
- The block ends where the digit/CR/text pattern stops. A NUL often follows, but not always, and the next block can follow directly. A repeated language digit starts the next block.
- Blocks are not always complete: 6–9 languages are common, and about 250 of 23,256 have no Russian entry (and ~0.6% no English entry; those are ignored).
- The set of 3114 records found by our structural scan is identical to the hash list in the third-party tool.
- The same records appear in the original 2003 game as `.mul` text; `defiance-orig/raw-cine-text-*` holds those.

Files in `cinematic/EN`: `textentry_<hash>.txt` and `crblock_<hash>_<k>.txt` (k = block number within the record). The text is raw, with `\n` as two characters.

## Third-party tool: conclusion

`LoKDR_Text_Tool` was useful as a reference and has been removed from the repository. It keys translations by English text, targets Hungarian, has no licence, and rewrites English slots. Everything it taught us is now implemented in `tools/` (see `docs/patching.md`).
