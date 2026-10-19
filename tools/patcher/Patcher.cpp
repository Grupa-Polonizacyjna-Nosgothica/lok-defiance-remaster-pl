#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include "BinaryUtils.hpp"
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
    LocalesBin LoadPolishLocals(const TranslationSource& source, const LocalesBin& en)
    {
        LocalesBin pl;
        pl.localeId = LOCALE_IDS.at(Locale::RU);
        pl.idToString = en.idToString;
        for (size_t i = 0; i < en.strings.size(); i++) {
            char name[40];
            std::snprintf(name, sizeof(name), "strings/PL/%04zu.txt", i);
            pl.strings.push_back(source.Read(name));
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

    // ---- fonts: the translation only holds a delta that is applied to the user's own files ----

    uint32_t HeaderU32(const std::string& raw, const size_t index)
    {
        return read_u32_le(reinterpret_cast<const uint8_t*>(raw.data()) + index * 4);
    }

    // Sparse overlay of new glyph pixels: "PLGO", u32 width, u32 height, u32 count, then per rectangle
    // u16 x, y, w, h and w*h bytes (top-down). Applied to the atlas (stored bottom-up) with max().
    std::string ApplyOverlay(const std::string& atlas, const std::string& overlay, const std::string& what)
    {
        const auto* o = reinterpret_cast<const uint8_t*>(overlay.data());
        if (overlay.size() < 16 || std::memcmp(o, "PLGO", 4) != 0) throw std::runtime_error(what + ": bad overlay file.");
        if (atlas.size() < 128 || atlas.compare(0, 4, "!WAR") != 0 || HeaderU32(atlas, 8) != 8)
            throw std::runtime_error(what + ": the game's atlas is not an 8 bit !WAR texture.");
        const uint32_t w = HeaderU32(atlas, 5), h = HeaderU32(atlas, 6);
        if (read_u32_le(o + 4) != w || read_u32_le(o + 8) != h || atlas.size() < 128 + static_cast<size_t>(w) * h)
            throw std::runtime_error(what + ": the overlay does not match the size of the game's atlas.");

        std::string out = atlas;
        size_t pos = 16;
        for (uint32_t i = 0, n = read_u32_le(o + 12); i < n; i++) {
            if (pos + 8 > overlay.size()) throw std::runtime_error(what + ": overlay is truncated.");
            const unsigned x = o[pos] | o[pos + 1] << 8, y = o[pos + 2] | o[pos + 3] << 8;
            const unsigned rw = o[pos + 4] | o[pos + 5] << 8, rh = o[pos + 6] | o[pos + 7] << 8;
            pos += 8;
            if (pos + static_cast<size_t>(rw) * rh > overlay.size() || x + rw > w || y + rh > h)
                throw std::runtime_error(what + ": overlay rectangle is out of range.");
            for (unsigned row = 0; row < rh; row++) {
                uint8_t* dst = reinterpret_cast<uint8_t*>(&out[128 + static_cast<size_t>(h - 1 - (y + row)) * w + x]);
                for (unsigned col = 0; col < rw; col++) dst[col] = std::max(dst[col], o[pos + static_cast<size_t>(row) * rw + col]);
            }
            pos += static_cast<size_t>(rw) * rh;
        }
        return out;
    }

    std::vector<std::string> SplitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream in(text);
        for (std::string line; std::getline(in, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) lines.push_back(line);
        }
        return lines;
    }

    // Tables with a count on the first line and `code,...` lines (font glyphs, char_casing.txt):
    // adds the lines whose code is not in the table yet and returns it sorted by code, CR LF terminated.
    std::string MergeTable(const std::string& original, const std::string& additions)
    {
        auto lines = SplitLines(original);
        if (lines.empty()) throw std::runtime_error("Empty table in the game files.");
        lines.erase(lines.begin());
        std::set<long> codes;
        for (const auto& l : lines) codes.insert(std::stol(l));
        for (const auto& l : SplitLines(additions))
            if (codes.insert(std::stol(l)).second) lines.push_back(l);
        std::stable_sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) { return std::stol(a) < std::stol(b); });
        std::string out = std::to_string(lines.size()) + "\r\n";
        for (const auto& l : lines) out += l + "\r\n";
        return out;
    }

    std::string AssetFromArchive(const uint8_t* data, const size_t fileSize, const BigfileIndex& index,
                                 const std::string& archivePath, std::vector<size_t>& slots)
    {
        slots = index.FindSlots(archivePath);
        if (slots.empty()) throw std::runtime_error("Cannot find " + archivePath + " in the index.");
        const BigfileEntry e = CheckedEntry(index, slots.front(), fileSize);
        return std::string(reinterpret_cast<const char*>(data + e.offset), e.size);
    }

    void AddAppend(PatchPlan& plan, const BigfileIndex& index, const std::string& content, const std::vector<size_t>& slots)
    {
        plan.appends.push_back({std::vector<uint8_t>(content.begin(), content.end()), SlotsOf(index, slots)});
    }

    // Every language reads its fonts from PCEnglish\ (docs/fonts.md): add the Polish glyphs there
    size_t AddFonts(const uint8_t* data, const size_t fileSize, const BigfileIndex& index,
                    const TranslationSource& source, PatchPlan& plan)
    {
        size_t count = 0;
        std::vector<size_t> slots;
        for (const int n : {1, 2}) {
            const std::string base = "fonts/font" + std::to_string(n);
            if (!source.Exists(base + ".overlay")) continue;

            const std::string rawPath = "PCEnglish\\font" + std::to_string(n) + ".raw";
            AddAppend(plan, index, ApplyOverlay(AssetFromArchive(data, fileSize, index, rawPath, slots),
                                                source.Read(base + ".overlay"), rawPath), slots);

            const std::string txtPath = "\\SR3\\image\\font" + std::to_string(n) + ".txt";
            AddAppend(plan, index, MergeTable(AssetFromArchive(data, fileSize, index, txtPath, slots),
                                              source.Read(base + ".add.txt")), slots);
            count += 2;
        }
        if (source.Exists("fonts/char_casing.add.txt")) {
            const std::string path = "\\SR3\\image\\char_casing.txt";
            AddAppend(plan, index, MergeTable(AssetFromArchive(data, fileSize, index, path, slots),
                                              source.Read("fonts/char_casing.add.txt")), slots);
            count++;
        }
        if (count == 0)
            Log("The translation has no font data; Polish letters will be missing in the game.", LogLevel::Error);
        return count;
    }
}

PatchPlan BuildPlan(const uint8_t* data, const size_t fileSize, const BigfileIndex& index, const TranslationSource& source)
{
    PatchPlan plan;
    plan.indexSize = index.IndexSize();

    // 1. locals.bin
    const LocalesBin en = LoadEnglish(data, fileSize, index);
    const LocalesBin pl = LoadPolishLocals(source, en);
    const auto ruSlots = index.FindSlots(LocalesPath(Locale::RU));
    if (ruSlots.empty()) throw std::runtime_error("Cannot find " + LocalesPath(Locale::RU) + " in the index.");
    plan.appends.push_back({pl.Build(), SlotsOf(index, ruSlots)});
    plan.localeStrings = pl.strings.size();

    // 2. fonts
    plan.fontFiles = AddFonts(data, fileSize, index, source, plan);

    // 3. cinematic texts
    std::map<uint32_t, std::string> textEntries;
    std::map<uint32_t, std::map<size_t, std::string>> crBlocks;
    for (const auto& name : source.List("cinematic/PL")) {
        if (const auto hash = CinematicFiles::ParseTextEntryName(name))
            textEntries[*hash] = source.Read("cinematic/PL/" + name);
        else if (const auto ref = CinematicFiles::ParseCrBlockName(name))
            crBlocks[ref->hash][ref->k] = source.Read("cinematic/PL/" + name);
    }
    if (textEntries.empty() && crBlocks.empty()) {
        Log("The translation has no cinematic texts; they are left as they are.", LogLevel::Error);
        return plan;
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

void VerifyPatch(const fs::path& bigfile, const TranslationSource& source)
{
    MappedFile file;
    file.Open(bigfile.string().c_str());
    const BigfileIndex index(file.Data(), file.Size());

    const LocalesBin en = LoadEnglish(file.Data(), file.Size(), index);
    const LocalesBin expected = LoadPolishLocals(source, en);

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
