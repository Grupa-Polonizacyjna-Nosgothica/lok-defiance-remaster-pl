#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include "CinematicFiles.hpp"
#include "CrBlocks.hpp"
#include "Locales.hpp"
#include "Logger.hpp"
#include "LocalesBin.hpp"
#include "MappedFile.hpp"
#include "PatchBackup.hpp"
#include "Patcher.hpp"
#include "TextEntry.hpp"

namespace fs = std::filesystem;

namespace
{
    constexpr size_t MAX_REPORTED = 20;

    std::string ReadFile(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot read " + path.string());
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // Cinematic texts store line breaks as the two characters `\n`; editors add a final newline
    std::string ToInlineText(std::string text)
    {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        std::string out;
        for (const char c : text) {
            if (c == '\r') continue;
            if (c == '\n') out += "\\n";
            else out += c;
        }
        return out;
    }

    BigfileEntry CheckedEntry(const BigfileIndex& index, const size_t slot, const size_t fileSize)
    {
        const BigfileEntry e = index.Entry(slot);
        if (e.size == 0 || e.offset > fileSize || e.size > fileSize - e.offset)
            throw std::runtime_error("Index entry " + std::to_string(slot) + " is out of range.");
        return e;
    }

    std::vector<PatchPlan::Slot> SlotsOf(const BigfileIndex& index, const std::vector<size_t>& slots)
    {
        std::vector<PatchPlan::Slot> out;
        for (const size_t s : slots) out.push_back({index.EntryPosition(s), index.Entry(s).extra});
        return out;
    }

    // Parses the original English block, which defines the string IDs
    LocalesBin LoadEnglish(const uint8_t* data, const size_t fileSize, const BigfileIndex& index)
    {
        const auto slots = index.FindSlots(LocalesPath(Locale::EN));
        if (slots.empty()) throw std::runtime_error("Cannot find " + LocalesPath(Locale::EN) + " in the index.");
        const BigfileEntry e = CheckedEntry(index, slots.front(), fileSize);
        LocalesBin en = LocalesBin::Parse(data + e.offset, e.size);
        if (en.localeId != LOCALE_IDS.at(Locale::EN))
            throw std::runtime_error("English locals.bin has an unexpected locale id.");
        return en;
    }

    // The translated locals.bin: English layout, Polish strings, Russian locale id
    LocalesBin LoadPolishLocals(const fs::path& stringsDir, const LocalesBin& en)
    {
        LocalesBin pl;
        pl.localeId = LOCALE_IDS.at(Locale::RU);
        pl.idToString = en.idToString;
        for (size_t i = 0; i < en.strings.size(); i++) {
            char name[16];
            std::snprintf(name, sizeof(name), "%04zu.txt", i);
            pl.strings.push_back(ReadFile(stringsDir / name));
        }

        // Internal names (music tracks, keys) have to stay as they are
        size_t restored = 0;
        for (size_t id = 0; id < en.idToString.size(); id++) {
            const int32_t s = en.idToString[id];
            if (s >= 0 && pl.strings[s] != en.strings[s] && IsProtectedString(id, en.strings[s])) {
                pl.strings[s] = en.strings[s];
                restored++;
            }
        }
        if (restored)
            Log("Kept " + std::to_string(restored) + " internal strings (music names, keys) as in the original; "
                "they had been changed in strings/PL.", LogLevel::Error);
        return pl;
    }
}

namespace
{
    // Game files replaced by the files of the fonts/ directory (name -> path in the archive)
    std::vector<std::pair<std::string, std::string>> FontTargets()
    {
        std::vector<std::pair<std::string, std::string>> targets;
        // Every language reads its font atlases from PCEnglish\ (see docs/fonts.md)
        for (const int n : {1, 2}) {
            targets.push_back({"font" + std::to_string(n) + ".raw", "PCEnglish\\font" + std::to_string(n) + ".raw"});
            targets.push_back({"font" + std::to_string(n) + ".txt", "\\SR3\\image\\font" + std::to_string(n) + ".txt"});
        }
        targets.push_back({"char_casing.txt", "\\SR3\\image\\char_casing.txt"});
        return targets;
    }

    size_t AddFontFiles(const uint8_t* data, const size_t fileSize, const BigfileIndex& index,
                        const fs::path& fontDir, PatchPlan& plan)
    {
        size_t count = 0;
        if (!fs::is_directory(fontDir)) {
            Log("No " + fontDir.string() + " directory; fonts are left as they are (Polish letters will be missing).", LogLevel::Error);
            return 0;
        }
        for (const auto& [name, archivePath] : FontTargets()) {
            const fs::path file = fontDir / name;
            if (!fs::exists(file)) continue;
            const auto slots = index.FindSlots(archivePath);
            if (slots.empty()) throw std::runtime_error("Cannot find " + archivePath + " in the index.");
            std::string content = ReadFile(file);
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".raw") == 0) {
                // A texture must keep the layout of the original: same header and same size
                const BigfileEntry e = CheckedEntry(index, slots.front(), fileSize);
                if (content.size() != e.size || content.compare(0, 128, std::string(reinterpret_cast<const char*>(data + e.offset), 128)) != 0)
                    throw std::runtime_error(file.string() + " does not have the header or size of the original " + archivePath);
            }
            plan.appends.push_back({std::vector<uint8_t>(content.begin(), content.end()), SlotsOf(index, slots)});
            count++;
        }
        return count;
    }
}

PatchPlan BuildPlan(const uint8_t* data, const size_t fileSize, const BigfileIndex& index, const fs::path& translationRoot)
{
    PatchPlan plan;
    plan.indexSize = index.IndexSize();

    // 1. locals.bin
    const fs::path stringsDir = translationRoot / "strings" / "PL";
    const LocalesBin en = LoadEnglish(data, fileSize, index);
    const LocalesBin pl = LoadPolishLocals(stringsDir, en);
    const auto ruSlots = index.FindSlots(LocalesPath(Locale::RU));
    if (ruSlots.empty()) throw std::runtime_error("Cannot find " + LocalesPath(Locale::RU) + " in the index.");
    plan.appends.push_back({pl.Build(), SlotsOf(index, ruSlots)});
    plan.localeStrings = pl.strings.size();

    // 2. fonts: replacement atlases and glyph tables that add the Polish letters
    plan.fontFiles = AddFontFiles(data, fileSize, index, translationRoot / "fonts", plan);

    // 3. cinematic texts
    const fs::path cineDir = translationRoot / "cinematic" / "PL";
    if (!fs::is_directory(cineDir)) {
        Log("No " + cineDir.string() + " directory; cinematic texts are left as they are.", LogLevel::Error);
        return plan;
    }

    std::map<uint32_t, std::string> textEntries;
    std::map<uint32_t, std::map<size_t, std::string>> crBlocks;
    for (const auto& dirEntry : fs::directory_iterator(cineDir)) {
        const std::string name = dirEntry.path().filename().string();
        if (const auto hash = CinematicFiles::ParseTextEntryName(name))
            textEntries[*hash] = ReadFile(dirEntry.path());
        else if (const auto ref = CinematicFiles::ParseCrBlockName(name))
            crBlocks[ref->hash][ref->k] = ReadFile(dirEntry.path());
    }

    size_t reported = 0;
    auto warn = [&reported](const std::string& message) {
        if (reported++ < MAX_REPORTED) Log(message, LogLevel::Error);
    };

    // TextEntry records are plain text, so the record is rebuilt and appended like locals.bin
    for (const auto& [hash, body] : textEntries) {
        const auto slots = index.FindSlots(hash);
        if (slots.empty()) { warn("No record " + CinematicFiles::TextEntryName(hash)); continue; }
        const BigfileEntry e = CheckedEntry(index, slots.front(), fileSize);
        if (!TextEntry::IsTextEntry(data + e.offset, e.size)) { warn("Not a TextEntry record: " + CinematicFiles::TextEntryName(hash)); continue; }
        const std::vector<uint8_t> record(data + e.offset, data + e.offset + e.size);
        auto rebuilt = TextEntry::SetBody(record, "russian", ToInlineText(body));
        if (!rebuilt) { warn("TextEntry without a Russian line: " + CinematicFiles::TextEntryName(hash)); continue; }
        plan.appends.push_back({std::move(*rebuilt), SlotsOf(index, slots)});
        plan.textEntries++;
    }

    // Language blocks sit inside binary records, so they are patched in place and keep their length
    for (const auto& [hash, texts] : crBlocks) {
        const auto slots = index.FindSlots(hash);
        if (slots.empty()) { warn("No record with hash " + CinematicFiles::CrBlockName(hash, 0)); plan.blocksSkipped += texts.size(); continue; }

        std::set<std::pair<uint64_t, uint64_t>> seen; // copies of a record may share their data
        for (const size_t slot : slots) {
            const BigfileEntry e = CheckedEntry(index, slot, fileSize);
            if (!seen.insert({e.offset, e.size}).second) continue;
            const auto blocks = CrBlocks::Find(data + e.offset, e.size);

            for (const auto& [k, text] : texts) {
                const std::string name = CinematicFiles::CrBlockName(hash, k);
                if (k >= blocks.size()) { warn("Record has no block " + name); plan.blocksSkipped++; continue; }

                const auto& block = blocks[k];
                std::vector<uint8_t> bytes(data + e.offset + block.begin, data + e.offset + block.end);
                CrBlocks::Block local = block; // same block, positions relative to `bytes`
                local.begin = 0;
                local.end = bytes.size();
                for (auto& entry : local.entries) { entry.textBegin -= block.begin; entry.textEnd -= block.begin; }

                size_t shortBy = 0;
                bool usedDonor = false;
                if (!CrBlocks::SetRussian(bytes.data(), local, ToInlineText(text), shortBy, &usedDonor)) {
                    if (shortBy) { warn(name + " is " + std::to_string(shortBy) + " bytes too long and was not patched"); plan.blocksTooLong++; }
                    else { warn(name + " has no Russian entry"); plan.blocksSkipped++; }
                    continue;
                }
                plan.inPlace.push_back({e.offset + block.begin, std::move(bytes)});
                plan.blocksPatched++;
                if (usedDonor) plan.blocksWithoutRussian++;
            }
        }
    }
    if (reported > MAX_REPORTED)
        Log("... and " + std::to_string(reported - MAX_REPORTED) + " more problems.", LogLevel::Error);
    return plan;
}

void ApplyPlan(const fs::path& bigfile, const PatchPlan& plan)
{
    const uint64_t originalSize = fs::file_size(bigfile);
    std::fstream f(bigfile, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open " + bigfile.string() + " for writing.");

    auto readAt = [&f](const uint64_t offset, const size_t length) {
        std::vector<uint8_t> bytes(length);
        f.seekg(static_cast<std::streamoff>(offset));
        f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
        if (!f) throw std::runtime_error("Failed to read the game file.");
        return bytes;
    };

    // Everything is backed up before the first byte is written
    PatchBackup backup;
    backup.originalSize = originalSize;
    backup.originalIndex = readAt(0, plan.indexSize);
    for (const auto& op : plan.inPlace)
        backup.regions.push_back({op.offset, readAt(op.offset, op.data.size())});
    backup.Save(PatchBackup::PathFor(bigfile));
    f.clear();

    for (const auto& op : plan.inPlace) {
        f.seekp(static_cast<std::streamoff>(op.offset));
        f.write(reinterpret_cast<const char*>(op.data.data()), static_cast<std::streamsize>(op.data.size()));
    }

    uint64_t end = originalSize;
    for (const auto& append : plan.appends) {
        const uint64_t offset = (end + BIGFILE_ALIGN - 1) / BIGFILE_ALIGN * BIGFILE_ALIGN;
        const std::vector<char> padding(offset - end, 0);
        f.seekp(static_cast<std::streamoff>(end));
        f.write(padding.data(), static_cast<std::streamsize>(padding.size()));
        f.write(reinterpret_cast<const char*>(append.data.data()), static_cast<std::streamsize>(append.data.size()));
        end = offset + append.data.size();

        for (const auto& slot : append.slots) {
            uint8_t entry[BIGFILE_ENTRY_SIZE];
            BigfileIndex::EncodeEntry({append.data.size(), offset, slot.extra}, entry);
            f.seekp(static_cast<std::streamoff>(slot.entryPosition));
            f.write(reinterpret_cast<const char*>(entry), sizeof(entry));
        }
    }
    f.flush();
    if (!f) throw std::runtime_error("Failed to write to " + bigfile.string() + ". Run the restore command to undo.");
}

void VerifyPatch(const fs::path& bigfile, const fs::path& translationRoot)
{
    MappedFile file;
    file.Open(bigfile.string().c_str());
    const BigfileIndex index(file.Data(), file.Size());

    const LocalesBin en = LoadEnglish(file.Data(), file.Size(), index);
    const LocalesBin expected = LoadPolishLocals(translationRoot / "strings" / "PL", en);

    const auto slots = index.FindSlots(LocalesPath(Locale::RU));
    if (slots.empty()) throw std::runtime_error("Verification failed: no Russian locals.bin.");
    for (const size_t slot : slots) {
        const BigfileEntry e = CheckedEntry(index, slot, file.Size());
        const LocalesBin actual = LocalesBin::Parse(file.Data() + e.offset, e.size);
        if (actual.localeId != expected.localeId || actual.idToString != expected.idToString || actual.strings != expected.strings)
            throw std::runtime_error("Verification failed: Russian locals.bin does not match strings/PL.");
    }
    Log("Verified: the Russian locals.bin holds " + std::to_string(expected.strings.size()) + " translated strings.");
}
