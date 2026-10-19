# Fonts and the Polish letters

The game's Russian text works because the fonts contain Cyrillic glyphs. Polish letters (ą ć ę ł ń ś ź ż and capitals) are missing; ó and Ó already exist. This is how fonts work and how we add the letters.

## How the game draws text (from `defiance.exe`, checked against the data)

- `\SR3\image\font.ini` says `Main = 01345` and `Header = 02345`. Each digit is a font number; for every character the game uses **the first font in that list that has the code point**. Fonts are not chosen by language: Russian text uses Latin letters from font 1 (or 2 for headers) and Cyrillic from font 3.
- Font numbers: 0 = button icons (private-use code points, RGBA), **1 = Latin (Main), 2 = Latin (Header)**, 3 = Cyrillic, 4 = Chinese, 5 = Japanese.
- Each font is two files in `bigfile.x64.dat`:
  - `\SR3\image\font<N>.txt`: first line is the glyph count, then one CR LF terminated line per glyph: `code,advance,x0,y0,x1,y1,u0,v0,u1,v1`. `x/y` are in em (40 px per em) relative to the pen and baseline, `u/v` are atlas pixels counted from the top with a half-pixel inset. The game sorts the table itself, so the order does not matter. The file is stored once and referenced by 9 index slots.
  - `PCEnglish\font<N>.raw`: the atlas texture.
- **Every language loads its fonts from `PCEnglish\`.** The loader (`FUN_14019c910`) builds the path as `<prefix>font<N>.raw`, and the prefix variable is set once at start-up to the literal `PCEnglish\`. The coordinates in `font<N>.txt` also only fit the `PCEnglish` atlases (512×512 for font 1; the `PCRussian\`, `PCFrench\`, … atlases are larger and unused for fonts). This conclusion comes from static analysis only; it has not been checked by running the game.
- The game also looks for an HD replacement `image_hd/font<N>.dds` in `bigfilehd.dat`; there is none for the fonts.
- `\SR3\image\char_casing.txt`: first line is the count, then `code,lower,upper` lines used for case conversion. Polish pairs had to be added there as well.

## The `!WAR` texture format (`*.raw`)

128 byte header (little-endian u32): `"!WAR"`, header size 0x80, data size, format, 0, width, height, pitch, bits per pixel, …; then the pixel data. Format 17 = 8 bit, one channel (the font atlases); format 16 = 32 bit RGBA. Rows are stored **bottom-up**. Some textures carry a full mip chain (data size = 4/3 of the top level). `bigfile.x64.dat` has 4252 such textures; `bigfilehd.dat` has none and holds 7121 DDS files plus other data (`tex.map` lists the names of the textures that have an HD version).

## Tools

- `tools/textures/war_textures.py list <dat> [--csv f]` lists every `!WAR` texture; `export <dat> <dir> [--path "PCEnglish\font1.raw"]` writes upright PNGs named by path hash. Use it to look at any texture of the game.
- `tools/fonts/polish_glyphs.py generate <dat> fonts` builds the 16 missing glyphs for fonts 1 and 2 out of existing ones (acute from ó/Ó, dot from the full stop, a drawn bar for ł/Ł and a drawn hook for the ogonek) and writes the atlas PNG, the glyph table and the RAW. `build fonts` converts edited PNGs back to RAW; `preview` renders a test line.

The results are committed in `fonts/`: `font1.png` and `font2.png` are the files to retouch by hand (draw the glyphs inside the cells listed in `font<N>.txt`), `*.raw` and `*.txt` are what the patcher writes into the game, `*.hdr` is the original 128 byte header, `preview*.png` shows the current look. The generated glyphs are a first version: the acutes and dots look right, the ogonek and the bar of ł are simple drawings worth redrawing.

## Remaining work

- Check in the game that the Polish glyphs show up (and that Cyrillic and other languages are unaffected).
- Font 3 (Cyrillic) is not touched; Russian text is not needed any more.
- Buttons and other graphics that contain text (menus drawn as textures) may still need work; they would be found with `war_textures.py`.
