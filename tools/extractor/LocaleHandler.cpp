#include <cstring>
#include <filesystem>
#include <fstream>

#include "LocaleHandler.hpp"
#include "Logger.hpp"

LocaleHandler::LocaleHandler(const uint8_t* data, const size_t fileSize)
    : m_data(data), m_fileSize(fileSize)
{
    if (m_fileSize < 4) {
        Log("File is too small to contain a bigfile index.", LogLevel::Error);
        return;
    }
    m_entryCount = read_u32_le(m_data) & 0xFFFF;
    if (4 + static_cast<size_t>(m_entryCount) * (4 + BIGFILE_ENTRY_SIZE) > m_fileSize) {
        Log("Bigfile index exceeds file size.", LogLevel::Error);
        m_entryCount = 0;
        return;
    }
    Log("Bigfile index has " + std::to_string(m_entryCount) + " entries.");

    for (const auto& [locale, language] : LOCALE_LANGUAGES) {
        const std::string path = "PC" + language + "\\locals.bin";
        const auto block_info = FindFile(path);
        if (!block_info) {
            Log("Could not find " + path + " in the bigfile index.", LogLevel::Error);
            continue;
        }

        auto block_data = LoadBlock(*block_info);
        if (block_data.size() < 16 || read_u32_le(block_data.data()) != LOCALE_IDS.at(locale)) {
            Log("Block " + path + " has an unexpected header.", LogLevel::Error);
            continue;
        }

        Log(path + " found at offset " + std::to_string(block_info->offset())
            + " with size " + std::to_string(block_info->size()));
        m_loaded_blocks[locale] = std::move(block_data);
    }
}

void LocaleHandler::ExportBlock(Locale locale, const std::string& output_dir) const
{
    const auto it = m_loaded_blocks.find(locale);
    if (it == m_loaded_blocks.end()) {
        Log("No loaded block for locale " + std::to_string(static_cast<int>(locale)) + ".", LogLevel::Error);
        return;
    }

    if (const std::vector<std::string> strings = std::move(ExtractStrings(it->second)); !strings.empty())
    {
        //export every string into a separate text file (UTF-8 without BOM) directly in the output directory
        //with the filename being the string index in the vector (use 4 digits with leading zeros) and the extension .txt
        const auto output_path = std::filesystem::path(output_dir);
        for (size_t i = 0; i < strings.size(); i++)
        {
            const std::filesystem::path file_path = output_path / (std::to_string(i).insert(0, 4 - std::to_string(i).length(), '0') + ".txt");
            std::ofstream file(file_path, std::ios::binary);
            if (!file) {
                Log("Failed to create file: " + file_path.string(), LogLevel::Error);
                continue;
            }
            file.write(strings[i].data(), static_cast<std::streamsize>(strings[i].size()));
        }
    }

    Log("Exported block for locale " + std::to_string(static_cast<int>(locale)) + " to directory: " + output_dir);
}

void LocaleHandler::ExportBlocks(const std::string& output_dir) const
{
    for(auto &[locale, locale_str] : LOCALE_NAMES)
    {
        const std::filesystem::path locale_path = std::filesystem::path(output_dir) / locale_str;
        std::filesystem::create_directory(locale_path);
        ExportBlock(locale, locale_path.string());
    }
}

std::optional<DataBlockInfo> LocaleHandler::FindFile(const std::string& path) const
{
    const uint32_t hash = bigfile_hash(path);
    const uint8_t* hashes = m_data + 4;

    // Hashes are sorted; binary search for the first candidate
    size_t lo = 0, hi = m_entryCount;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (read_u32_le(hashes + mid * 4) < hash) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= m_entryCount || read_u32_le(hashes + lo * 4) != hash)
        return std::nullopt;

    // Entry: u64 size, u64 file offset, u64 unknown (always 0 so far)
    const uint8_t* entry = hashes + static_cast<size_t>(m_entryCount) * 4 + lo * BIGFILE_ENTRY_SIZE;
    const uint64_t size = read_u64_le(entry);
    const uint64_t offset = read_u64_le(entry + 8);
    if (size == 0 || offset == 0 || offset > m_fileSize || size > m_fileSize - offset)
        return std::nullopt;

    return DataBlockInfo(offset, size);
}

std::vector<uint8_t> LocaleHandler::LoadBlock(const DataBlockInfo& block_info) const
{
    if (block_info.end() > m_fileSize) {
        Log("Block end exceeds file size.", LogLevel::Error);
        return {};
    }

    return { m_data + block_info.offset(), m_data + block_info.end() };
}


std::vector<std::string> LocaleHandler::ExtractStrings(const std::vector<uint8_t>& block_data)
{
    std::vector<std::string> strings;

    //read first 4 bytes into an uint32 to get the offset to the first string, then add this offset to the block start offset to get the actual position of the first string in the block
    if (block_data.size() < 16) {
        Log("Block data is too small to contain valid header and offset table.", LogLevel::Error);
        return {};
    }
    size_t pos = read_u32_le(block_data.data() + 12);

    // Every string is null-terminated and then the next string starts immediately after
    // Push them into the strings vector until two consecutive zero bytes are found, which indicates the end of the block
    while (pos < block_data.size())
    {
        auto str_start = reinterpret_cast<const char*>(block_data.data() + pos);
        size_t str_len = std::strlen(str_start);
        if (str_len == 0) {
            // Check for two consecutive zero bytes to confirm end of block
            if (pos + 1 < block_data.size() && block_data[pos + 1] == 0) {
                break; // End of block reached
            }
            pos++; // Move past the single zero byte
            continue;
        }
        strings.emplace_back(str_start, str_len);
        pos += str_len + 1; // Move to the start of the next string
    }

    return strings;
}
