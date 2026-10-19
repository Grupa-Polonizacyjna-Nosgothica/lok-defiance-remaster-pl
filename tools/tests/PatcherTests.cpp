// Tests of the patcher on a small synthetic archive, so they run anywhere without game data.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>

#include "BigfileIndex.hpp"
#include "BinaryUtils.hpp"
#include "CinematicFiles.hpp"
#include "CrBlocks.hpp"
#include "Locales.hpp"
#include "LocalesBin.hpp"
#include "MappedFile.hpp"
#include "PatchBackup.hpp"
#include "Patcher.hpp"
#include "Preflight.hpp"
#include "TextEntry.hpp"
#include "TranslationSource.hpp"

namespace fs = std::filesystem;

namespace
{
    int g_failures = 0;
    int g_checks = 0;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; } } while (0)

    std::string ReadAll(const fs::path& p)
    {
        std::ifstream in(p, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    void WriteAll(const fs::path& p, const std::string& content)
    {
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    void PutU32(std::string& s, const uint32_t v) { for (int i = 0; i < 4; i++) s += static_cast<char>(v >> (8 * i)); }
    void PutU16(std::string& s, const unsigned v) { s += static_cast<char>(v); s += static_cast<char>(v >> 8); }

    // ---- synthetic game archive ----
    constexpr uint32_t kTextEntryHash = 0x35DB199E;
    constexpr uint32_t kCrHash = 0x00C09A5B;
    constexpr unsigned kAtlas = 64;

    std::string Bytes(const std::vector<uint8_t>& v) { return std::string(v.begin(), v.end()); }

    std::string Locals(const uint32_t id, const std::vector<std::string>& strings)
    {
        LocalesBin b;
        b.localeId = id;
        b.strings = strings;
        b.idToString = {-1, 0, 1, 2, 3, 0}; // ID 5 shares the first string, ID 0 is empty
        return Bytes(b.Build());
    }

    std::string FontAtlas()
    {
        std::string hdr(128, '\0');
        std::memcpy(&hdr[0], "!WAR", 4);
        auto set = [&hdr](const size_t i, const uint32_t v) { for (int k = 0; k < 4; k++) hdr[i * 4 + k] = static_cast<char>(v >> (8 * k)); };
        set(1, 128); set(2, kAtlas * kAtlas); set(3, 17); set(5, kAtlas); set(6, kAtlas); set(7, kAtlas); set(8, 8);
        std::string pixels(kAtlas * kAtlas, '\0');
        pixels[0] = 77; // an original pixel (bottom-left in the upright image)
        return hdr + pixels;
    }

    std::string TextEntryRecord()
    {
        return "Version 1\r\nTextEntry\r\nLine0\r\n"
               "lang_english 2003-10-20 15:57:01:00 \"\\(1.8)Now, at last,\"\r\n"
               "lang_russian 2026-02-06 13:21:18:97 \"\\(1.8)Privet\"\r\n";
    }

    std::string CrRecord(size_t& blockOffset)
    {
        std::string r(16, '\x01');
        r += std::string(4, '\0');
        blockOffset = r.size();
        r += "7\r<cn>\r0\rHello there\r1\rBonjour\r8\rPrivet vsem\r"; // block 0, with a Russian entry
        r += "0\rAbc def\r1\rFrench text\r2\rGerman text!\r";       // block 1, directly after it, no Russian entry
        r += std::string(8, '\0');
        return r;
    }

    struct Archive
    {
        fs::path file;
        std::string original;
        size_t crBlockOffset = 0;
    };

    // Index with sorted hashes, 24 byte entries, data aligned to 0x800. The text entry is listed twice.
    Archive BuildArchive(const fs::path& dir)
    {
        Archive a;
        std::vector<std::pair<uint32_t, std::string>> records = {
            {bigfile_hash(LocalesPath(Locale::EN)), Locals(0, {"Hello", "Setup", "Music_01", "Bye"})},
            {bigfile_hash(LocalesPath(Locale::RU)), Locals(10, {"Privet", "Nastroyka", "Music_01", "Poka"})},
            {kTextEntryHash, TextEntryRecord()},
            {kCrHash, CrRecord(a.crBlockOffset)},
            {bigfile_hash("PCEnglish\\font1.raw"), FontAtlas()},
            {bigfile_hash("\\SR3\\image\\font1.txt"), "2\r\n65,0.5,0,-0.8,0.5,0.1,4.5,4.5,20.5,20.5\r\n97,0.5,0,-0.8,0.5,0.1,24.5,4.5,40.5,20.5\r\n"},
            {bigfile_hash("\\SR3\\image\\char_casing.txt"), "1\r\n65,97,0\r\n"},
        };
        records.push_back(records[2]); // a second slot with the same hash

        std::sort(records.begin(), records.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
        const uint32_t n = static_cast<uint32_t>(records.size());
        std::string out;
        PutU32(out, n);
        for (const auto& r : records) PutU32(out, r.first);

        std::string table, data;
        const uint64_t dataStart = (out.size() + n * BIGFILE_ENTRY_SIZE + BIGFILE_ALIGN - 1) / BIGFILE_ALIGN * BIGFILE_ALIGN;
        std::map<std::string, uint64_t> placed; // identical data is stored once
        for (const auto& r : records) {
            auto it = placed.find(r.second);
            if (it == placed.end()) {
                while ((dataStart + data.size()) % BIGFILE_ALIGN) data += '\0';
                it = placed.emplace(r.second, dataStart + data.size()).first;
                data += r.second;
            }
            for (const uint64_t v : {static_cast<uint64_t>(r.second.size()), it->second, uint64_t(0)})
                for (int k = 0; k < 8; k++) table += static_cast<char>(v >> (8 * k));
        }
        out += table;
        out.resize(dataStart, '\0');
        out += data;

        a.original = out;
        a.file = dir / "bigfile.x64.dat";
        WriteAll(a.file, out);
        return a;
    }

    // ---- synthetic translation ----
    void BuildTranslation(const fs::path& root)
    {
        const std::vector<std::string> pl = {"Czesc", "Ustawienia", "Zmieniona_muzyka", "Pa"}; // the music name must not survive
        for (size_t i = 0; i < pl.size(); i++) {
            char name[32];
            std::snprintf(name, sizeof(name), "%04zu.txt", i);
            WriteAll(root / "strings" / "PL" / name, pl[i]);
        }
        WriteAll(root / "cinematic" / "PL" / CinematicFiles::TextEntryName(kTextEntryHash), "\\(1.8)Teraz, wreszcie,\n");
        WriteAll(root / "cinematic" / "PL" / CinematicFiles::CrBlockName(kCrHash, 0), "Witaj tam\n");
        WriteAll(root / "cinematic" / "PL" / CinematicFiles::CrBlockName(kCrHash, 1), "Tekst bez rosyjskiego");

        std::string overlay = "PLGO";
        PutU32(overlay, kAtlas); PutU32(overlay, kAtlas); PutU32(overlay, 1);
        PutU16(overlay, 30); PutU16(overlay, 30); PutU16(overlay, 4); PutU16(overlay, 4);
        overlay += std::string(16, '\x90');
        WriteAll(root / "fonts" / "font1.overlay", overlay);
        WriteAll(root / "fonts" / "font1.add.txt", "261,0.5,0,-0.8,0.5,0.1,30.5,30.5,33.5,33.5\r\n");
        WriteAll(root / "fonts" / "char_casing.add.txt", "261,0,260\r\n260,261,0\r\n");
    }

    PatchPlan Plan(const fs::path& file, const TranslationSource& source)
    {
        MappedFile m;
        m.Open(file.string().c_str());
        return BuildPlan(m.Data(), m.Size(), BigfileIndex(m.Data(), m.Size()), source);
    }

    std::string Record(const fs::path& file, const uint32_t hash, const size_t which = 0)
    {
        MappedFile m;
        m.Open(file.string().c_str());
        const BigfileIndex index(m.Data(), m.Size());
        const BigfileEntry e = index.Entry(index.FindSlots(hash).at(which));
        return std::string(reinterpret_cast<const char*>(m.Data()) + e.offset, e.size);
    }

    void TestHashes()
    {
        CHECK(bigfile_hash("PCEnglish\\locals.bin") == 0xDFE4292C); // the value the game itself computes
        CHECK(crc32_ieee(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xCBF43926);
    }

    void TestFormats()
    {
        const LocalesBin parsed = LocalesBin::Parse(reinterpret_cast<const uint8_t*>(Locals(0, {"a", "b", "c", "d"}).data()),
                                                    Locals(0, {"a", "b", "c", "d"}).size());
        CHECK(parsed.strings.size() == 4 && parsed.idToString.size() == 6 && parsed.idToString[5] == 0 && parsed.idToString[0] == -1);

        const std::string cr = CrRecord(*std::make_unique<size_t>());
        const auto blocks = CrBlocks::Find(reinterpret_cast<const uint8_t*>(cr.data()), cr.size());
        CHECK(blocks.size() == 2);

        const std::string te = TextEntryRecord();
        const std::vector<uint8_t> teBytes(te.begin(), te.end());
        CHECK(TextEntry::IsTextEntry(teBytes.data(), teBytes.size()));
        CHECK(TextEntry::GetBody(teBytes, "russian") == std::string("\\(1.8)Privet"));
        const auto changed = TextEntry::SetBody(teBytes, "russian", "X");
        CHECK(changed && TextEntry::GetBody(*changed, "russian") == std::string("X") && TextEntry::GetBody(*changed, "english"));

        // a text that does not fit keeps the block untouched
        std::vector<uint8_t> crBytes(cr.begin(), cr.end());
        size_t shortBy = 0;
        CrBlocks::Block local = blocks[0];
        CHECK(!CrBlocks::SetRussian(crBytes.data(), local, std::string(500, 'x'), shortBy) && shortBy > 0);
    }

    void TestPatchAndRestore(const fs::path& dir)
    {
        const Archive a = BuildArchive(dir);
        BuildTranslation(dir / "tr");
        const auto source = OpenTranslationSource(dir / "tr");

        // the synthetic file is not the real game build
        {
            MappedFile m;
            m.Open(a.file.string().c_str());
            bool refused = false;
            try { CheckGameVersion(m.Data(), m.Size(), BigfileIndex(m.Data(), m.Size())); }
            catch (const PreflightFailure& e) { refused = e.Code() == ExitCode::UnsupportedVersion; }
            CHECK(refused);
        }

        const PatchPlan plan = Plan(a.file, *source);
        CHECK(plan.localeStrings == 4 && plan.textEntries == 1 && plan.blocksPatched == 2 && plan.blocksTooLong == 0);
        CHECK(plan.blocksWithoutRussian == 1); // block 1 has no Russian entry and reuses another language
        CHECK(plan.fontFiles == 3);            // atlas and glyph table of font 1, and the casing table
        ApplyPlan(a.file, plan);
        VerifyPatch(a.file, *source);

        const PatchBackup backup = PatchBackup::Load(PatchBackup::PathFor(a.file));
        CHECK(ClassifyFile(a.file, backup) == FileState::Patched);

        {
            MappedFile m;
            m.Open(a.file.string().c_str());
            const BigfileIndex index(m.Data(), m.Size());
            CHECK(m.Size() > a.original.size());

            // locals: Polish text in the Russian slot, music name kept, English untouched
            const BigfileEntry ru = index.Entry(index.FindSlots(LocalesPath(Locale::RU)).at(0));
            const LocalesBin ruBlock = LocalesBin::Parse(m.Data() + ru.offset, ru.size);
            CHECK(ruBlock.localeId == 10 && ruBlock.strings[0] == "Czesc" && ruBlock.strings[2] == "Music_01");
            CHECK(Record(a.file, bigfile_hash(LocalesPath(Locale::EN))) == Locals(0, {"Hello", "Setup", "Music_01", "Bye"}));

            // both slots of the text entry point to the new data
            CHECK(index.FindSlots(kTextEntryHash).size() == 2);
            for (size_t s = 0; s < 2; s++) {
                const std::string r = Record(a.file, kTextEntryHash, s);
                const std::vector<uint8_t> bytes(r.begin(), r.end());
                CHECK(TextEntry::GetBody(bytes, "russian") == std::string("\\(1.8)Teraz, wreszcie,"));
            }

            // language blocks: same record size, Polish in the Russian slot, English unchanged
            const std::string cr = Record(a.file, kCrHash);
            CHECK(cr.size() == CrRecord(*std::make_unique<size_t>()).size());
            const auto blocks = CrBlocks::Find(reinterpret_cast<const uint8_t*>(cr.data()), cr.size());
            CHECK(blocks.size() == 2);
            auto textOf = [&cr](const CrBlocks::Block& b, const int lang) {
                for (const auto& e : b.entries) if (e.lang == lang) return cr.substr(e.textBegin, e.textEnd - e.textBegin);
                return std::string("<none>");
            };
            CHECK(textOf(blocks[0], CrBlocks::kRussian).rfind("Witaj tam", 0) == 0);
            CHECK(textOf(blocks[0], CrBlocks::kEnglish) == "Hello there");
            CHECK(textOf(blocks[1], CrBlocks::kRussian).rfind("Tekst bez rosyjskiego", 0) == 0);
            CHECK(textOf(blocks[1], CrBlocks::kEnglish) == "Abc def");

            // fonts: overlay pixel added, original pixel kept, glyph and casing lines merged
            const std::string atlas = Record(a.file, bigfile_hash("PCEnglish\\font1.raw"));
            CHECK(static_cast<uint8_t>(atlas[128]) == 77);
            CHECK(static_cast<uint8_t>(atlas[128 + (kAtlas - 1 - 31) * kAtlas + 31]) == 0x90); // upright (31,31), file is bottom-up
            CHECK(Record(a.file, bigfile_hash("\\SR3\\image\\font1.txt")).rfind("3\r\n65,", 0) == 0);
            CHECK(Record(a.file, bigfile_hash("\\SR3\\image\\char_casing.txt")) == "3\r\n65,97,0\r\n260,261,0\r\n261,0,260\r\n");
        }

        // patching again starts from the original and gives the same file
        const std::string patched = ReadAll(a.file);
        backup.Restore(a.file);
        fs::remove(PatchBackup::PathFor(a.file));
        CHECK(ReadAll(a.file) == a.original); // restore is exact
        {
            const PatchPlan again = Plan(a.file, *source);
            ApplyPlan(a.file, again);
            CHECK(ReadAll(a.file) == patched);
        }

        // a package gives the same result as the directory
        WritePackage(*source, dir / "polish.pak");
        PatchBackup::Load(PatchBackup::PathFor(a.file)).Restore(a.file);
        fs::remove(PatchBackup::PathFor(a.file));
        const auto pak = OpenTranslationSource(dir / "polish.pak");
        ApplyPlan(a.file, Plan(a.file, *pak));
        CHECK(ReadAll(a.file) == patched);

        // states of the backup
        PatchBackup b = PatchBackup::Load(PatchBackup::PathFor(a.file));
        CHECK(ClassifyFile(a.file, b) == FileState::Patched);
        PatchBackup interrupted = b;
        interrupted.patchedSize = 0;
        CHECK(ClassifyFile(a.file, interrupted) == FileState::Interrupted);
        b.Restore(a.file);
        CHECK(ClassifyFile(a.file, b) == FileState::Original);
        CHECK(ReadAll(a.file) == a.original);
        std::string damaged = a.original;
        damaged[200] ^= 1; // somewhere in the index
        WriteAll(a.file, damaged);
        CHECK(ClassifyFile(a.file, b) == FileState::Changed);
    }

    void TestPackageIntegrity(const fs::path& dir)
    {
        BuildTranslation(dir / "tr2");
        const auto source = OpenTranslationSource(dir / "tr2");
        WritePackage(*source, dir / "p.pak");
        std::string pak = ReadAll(dir / "p.pak");
        pak[pak.size() - 3] ^= 0x55; // inside the last file
        WriteAll(dir / "bad.pak", pak);
        bool detected = false;
        try {
            const auto bad = OpenTranslationSource(dir / "bad.pak");
            for (const auto& f : ListTranslationFiles(*bad)) (void)bad->Read(f);
        } catch (const std::exception&) { detected = true; }
        CHECK(detected);

        const auto good = OpenTranslationSource(dir / "p.pak");
        CHECK(good->Read("strings/PL/0000.txt") == "Czesc");
        CHECK(good->List("strings/PL").size() == 4);
        CHECK(!good->Exists("fonts/font1.raw")); // full game files are never packed
    }
}

int main()
{
    const fs::path dir = fs::temp_directory_path() / ("lokpl_tests_" + std::to_string(std::rand()));
    fs::create_directories(dir);
    try {
        TestHashes();
        TestFormats();
        TestPatchAndRestore(dir);
        TestPackageIntegrity(dir);
    } catch (const std::exception& e) {
        std::cerr << "Test aborted: " << e.what() << "\n";
        g_failures++;
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::cout << g_checks << " checks, " << g_failures << " failures\n";
    return g_failures == 0 ? 0 : 1;
}
