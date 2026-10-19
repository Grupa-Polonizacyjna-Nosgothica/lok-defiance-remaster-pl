# LoK: Defiance Remastered PL
[![Ko-fi](ko-fi_btn.png)](https://ko-fi.com/veritaware)

Nieoficjalne spolszczenie napisów i tekstów w grze `Legacy of Kain: Defiance Remastered` autorstwa Grupy Polonizacyjnej Nosgothica.
Jest to nasze **autorskie** tłumaczenie, niepowiązane z popularnym polskim tłumaczeniem Mrocznej Kroniki LoK: Defiance autorstwa _LoK:Archives_.

Podobnie jak przy naszym spolszczeniu do remastera Soul Reaver 1&2, skupiliśmy się przede wszystkim na tym, by było ono jak najbardziej przystępne
dla polskiego odbiorcy, było możliwie dokładne, ale także zachowywało brzmienie godne dzieła z gatunku Dark Fantasy, unikając anachronizmów,
nowoczesnych kolokwializmów czy kalek z języka angielskiego, które po polsku brzmią niezbyt naturalnie.

## Status prac: **w toku, bez daty premiery**

Remaster trzyma wszystkie dane w jednym wielkim pliku (`bigfile.x64.dat`), więc nie da się go spolszczyć podmianą plików, jak w przypadku Soul Reaver 1&2.
Przez długi czas blokowało nas to całkowicie. Dziś **narzędzia są gotowe, ale nie zostały jeszcze sprawdzone w samej grze**,
a tłumaczenie tekstów remastera trzeba jeszcze przenieść i dokończyć.

### Tłumaczenie

| Część | Stan |
|---|---|
| Dialogi i napisy z oryginalnej gry z 2003 roku (`defiance-orig/raw-cine-text-pl`) | ![100%](https://progress-bar.xyz/100?width=128) |
| Przeniesienie dialogów do tekstów remastera (`cinematic/PL`, napisy w `strings/PL`) | _do zrobienia_ - pliki `PL` są na razie kopiami angielskich |
| Interfejs, menu, opisy, galeria (`strings/PL`) | _do zrobienia_ |
| Bonusy i Mroczna Kronika | _TBA_ |

Postęp zadań możesz sprawdzić na [Roadmapie](https://github.com/orgs/Grupa-Polonizacyjna-Nosgothica/projects/2). Stopień przetłumaczenia plików `PL` pokaże `python3 tools/validate_translation.py .`.

### Narzędzia i wydanie

| Część | Stan |
|---|---|
| Wyciąganie tekstów z `bigfile.x64.dat` (`LocaleExtractor`) | gotowe |
| Wgrywanie tłumaczenia na komputerze użytkownika (`PolishPatcher`) - polskie teksty trafiają w miejsce rosyjskich, z kopią zapasową i przywracaniem | gotowe, sprawdzone na kopii pliku gry; **nie wypróbowane w grze** |
| Polskie litery w czcionkach (ą ć ę ł ń ś ź ż) | pierwsza wersja glifów, **do sprawdzenia w grze** i ewentualnego poprawienia graficznego |
| Instalator (Inno Setup) i automatyczne budowanie (GitHub Actions) | napisane; **jeszcze nie skompilowane ani uruchomione** (wersja na Windows) |
| Wydanie | brak terminu - najpierw musimy sprawdzić, czy spolszczenie działa w grze |

Instalator nie będzie zawierał plików gry: znajdzie grę (Steam/GOG), a potem zmodyfikuje na komputerze użytkownika jego własny plik `bigfile.x64.dat`.
W launcherze trzeba będzie wybrać język **rosyjski** - w jego miejsce gra pokaże polskie teksty. Szczegóły: [docs/patching.md](docs/patching.md).

## Dla tłumaczy i programistów

- `strings/EN`, `strings/PL` - teksty interfejsu i długie napisy (3908 plików, jeden tekst na plik). `cinematic/EN`, `cinematic/PL` - krótkie napisy i kwestie.
- Edytujemy tylko pliki `PL`; numery plików muszą pozostać bez zmian.
- Zasady (linie z czasami `60 0`, znaczniki `\(1.8)`, ciągi `\n`, nazwy techniczne typu `Sarafan_battle_01`, limit miejsca dla krótkich kwestii) opisuje
  [docs/cinematic-text-vs-strings.md](docs/cinematic-text-vs-strings.md) i [docs/locals-and-text-records.md](docs/locals-and-text-records.md). Błędy wychwytuje `python3 tools/validate_translation.py .` (działa też w CI).
- Budowanie narzędzi: `cmake -S tools -B build && cmake --build build`, testy: `ctest --test-dir build`.
- Dokumentacja techniczna: [pliki gry i formaty tekstów](docs/locals-and-text-records.md), [czcionki](docs/fonts.md), [wgrywanie tłumaczenia, instalator, testy i CI](docs/patching.md),
  [format indeksu archiwum](tools/bigfile/FORMAT.md). Wskazówki dla asystentów AI: [CLAUDE.md](CLAUDE.md).

## Wspieraj nasze projekty
Jeśli chcesz nas wesprzeć finansowo, [postaw nam kawę na Ko-fi](https://ko-fi.com/veritaware).

Aby dyskutować nad postępami prac i wprowadzanymi decyzjami, oraz śledzić nasze przyszłe projekty, dołącz do nas na naszym [serwerze Discord](https://discord.gg/bJKCtJcaDg).

---

# LoK: Defiance Remastered PL (English)
[![Ko-fi](ko-fi_btn_en.png)](https://ko-fi.com/veritaware)

An unofficial Polish translation of the subtitles and texts of *Legacy of Kain: Defiance Remastered* by Grupa Polonizacyjna Nosgothica (the Nosgothica Polonization Group).
It is **our own** translation, unrelated to the well-known Polish translation of the Dark Chronicle of LoK: Defiance by _LoK:Archives_.

As with our *Soul Reaver 1&2 Remastered* translation, we aim for natural, accurate Polish that keeps the tone of a Dark Fantasy work, avoiding anachronisms, modern colloquialisms and calques from English.

## Status: **in progress, no release date**

The remaster keeps all its data in one huge file (`bigfile.x64.dat`), so it cannot be translated by swapping files as in *Soul Reaver 1&2*. For a long time this blocked us completely.
**The tools are now written but have not been tried in the game yet**, and the translation of the remaster's texts still has to be ported and finished.

### Translation

| Part | State |
|---|---|
| Dialogue and subtitles of the original 2003 game (`defiance-orig/raw-cine-text-pl`) | 100% |
| Porting the dialogue into the remaster's texts (`cinematic/PL`, subtitles in `strings/PL`) | to do - the `PL` files are copies of the English ones for now |
| Interface, menus, descriptions, gallery (`strings/PL`) | to do |
| Bonuses and the Dark Chronicle | TBA |

Task progress is on the [Roadmap](https://github.com/orgs/Grupa-Polonizacyjna-Nosgothica/projects/2); `python3 tools/validate_translation.py .` reports how much of the `PL` files is translated.

### Tools and release

| Part | State |
|---|---|
| Extracting texts from `bigfile.x64.dat` (`LocaleExtractor`) | done |
| Applying the translation on the user's machine (`PolishPatcher`): Polish text goes into the Russian slot, with a backup and restore | done, tested on a copy of the game file; **not tried in the game** |
| Polish letters in the fonts (ą ć ę ł ń ś ź ż) | first version of the glyphs, **to be checked in the game** and possibly redrawn |
| Installer (Inno Setup) and automatic builds (GitHub Actions) | written; **not compiled or run yet** (Windows build) |
| Release | no date - we first have to confirm that it works in the game |

The installer will not contain game files: it finds the game (Steam/GOG) and modifies the user's own `bigfile.x64.dat` on their machine. Players will select **Russian** in the game launcher,
and the game will show the Polish texts in its place. Details: [docs/patching.md](docs/patching.md).

## For translators and developers

- `strings/EN`, `strings/PL` hold interface texts and long subtitles (3908 files, one text per file); `cinematic/EN`, `cinematic/PL` hold short subtitles and lines. Only edit the `PL` files and keep the file numbers unchanged.
- The rules (timing lines like `60 0`, `\(1.8)` markers, `\n` sequences, technical names such as `Sarafan_battle_01`, the space limit for short lines) are in [docs/cinematic-text-vs-strings.md](docs/cinematic-text-vs-strings.md)
  and [docs/locals-and-text-records.md](docs/locals-and-text-records.md). `python3 tools/validate_translation.py .` catches mistakes (it also runs in CI).
- Build the tools: `cmake -S tools -B build && cmake --build build`, run the tests: `ctest --test-dir build`.
- Technical docs: [game files and text formats](docs/locals-and-text-records.md), [fonts](docs/fonts.md), [applying the translation, installer, tests and CI](docs/patching.md), [archive index format](tools/bigfile/FORMAT.md).
  Notes for AI assistants: [CLAUDE.md](CLAUDE.md).

## Support our projects
If you would like to support us financially, [buy us a coffee on Ko-fi](https://ko-fi.com/veritaware). To discuss progress and decisions and to follow our future projects, join our [Discord server](https://discord.gg/bJKCtJcaDg).
