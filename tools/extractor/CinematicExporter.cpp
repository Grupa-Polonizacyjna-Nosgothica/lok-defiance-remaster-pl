#include <filesystem>
#include <fstream>
#include <vector>

#include "CinematicExporter.hpp"
#include "CinematicFiles.hpp"
#include "CrBlocks.hpp"
#include "Logger.hpp"
#include "TextEntry.hpp"

namespace
{
    // The largest record known to hold such a block is about 39 MB; skip anything above this
    constexpr uint64_t MAX_SCANNED_SIZE = 256ull * 1024 * 1024;

    void WriteFile(const std::filesystem::path& path, const std::string& content)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            Log("Failed to create file: " + path.string(), LogLevel::Error);
            return;
        }
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
}

void ExportCinematicText(const uint8_t* data, const size_t fileSize, const BigfileIndex& index, const std::string& output_dir)
{
    std::filesystem::create_directories(output_dir);
    const std::filesystem::path out(output_dir);
    size_t textEntries = 0, crRecords = 0, crBlocks = 0;

    for (size_t slot = 0; slot < index.Count(); slot++) {
        // Slots with the same hash are copies of one record; read it once
        if (slot > 0 && index.Hash(slot) == index.Hash(slot - 1))
            continue;
        if (slot % 2000 == 0)
            Log("Scanning records... " + std::to_string(slot) + "/" + std::to_string(index.Count()));

        const BigfileEntry entry = index.Entry(slot);
        if (entry.size < 16 || entry.size > MAX_SCANNED_SIZE || entry.offset > fileSize || entry.size > fileSize - entry.offset)
            continue;
        const uint8_t* record = data + entry.offset;
        const uint32_t hash = index.Hash(slot);

        if (TextEntry::IsTextEntry(record, entry.size)) {
            const std::vector<uint8_t> bytes(record, record + entry.size);
            if (const auto body = TextEntry::GetBody(bytes, "english")) {
                WriteFile(out / CinematicFiles::TextEntryName(hash), *body);
                textEntries++;
            }
            continue;
        }

        const auto blocks = CrBlocks::Find(record, entry.size);
        if (blocks.empty()) continue;
        crRecords++;
        for (size_t k = 0; k < blocks.size(); k++) {
            for (const auto& e : blocks[k].entries) {
                if (e.lang != CrBlocks::kEnglish) continue;
                WriteFile(out / CinematicFiles::CrBlockName(hash, k),
                          std::string(reinterpret_cast<const char*>(record) + e.textBegin, e.textEnd - e.textBegin));
                crBlocks++;
                break;
            }
        }
    }

    Log("Exported " + std::to_string(textEntries) + " TextEntry records and " + std::to_string(crBlocks)
        + " language blocks from " + std::to_string(crRecords) + " records to directory: " + output_dir);
}
