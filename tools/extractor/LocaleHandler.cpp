#include <filesystem>
#include <fstream>
#include <cstdio>

#include "LocaleHandler.hpp"
#include "Logger.hpp"

LocaleHandler::LocaleHandler(const uint8_t* data, const size_t fileSize)
    : m_index(data, fileSize), m_data(data)
{
    Log("Bigfile index has " + std::to_string(m_index.Count()) + " entries.");

    for (const auto& [locale, language] : LOCALE_LANGUAGES) {
        const std::string path = LocalesPath(locale);
        const auto slots = m_index.FindSlots(path);
        if (slots.empty()) {
            Log("Could not find " + path + " in the bigfile index.", LogLevel::Error);
            continue;
        }

        const BigfileEntry entry = m_index.Entry(slots.front());
        if (entry.size == 0 || entry.offset > fileSize || entry.size > fileSize - entry.offset) {
            Log("Entry of " + path + " is out of range.", LogLevel::Error);
            continue;
        }

        try {
            LocalesBin block = LocalesBin::Parse(data + entry.offset, entry.size);
            if (block.localeId != LOCALE_IDS.at(locale)) {
                Log("Block " + path + " has an unexpected locale id.", LogLevel::Error);
                continue;
            }
            Log(path + " found at offset " + std::to_string(entry.offset) + " with size " + std::to_string(entry.size));
            m_blocks[locale] = std::move(block);
        } catch (const std::exception& e) {
            Log(path + ": " + e.what(), LogLevel::Error);
        }
    }
}

void LocaleHandler::ExportBlock(const Locale locale, const std::string& output_dir) const
{
    const auto it = m_blocks.find(locale);
    if (it == m_blocks.end()) {
        Log("No loaded block for locale " + LOCALE_NAMES.at(locale) + ".", LogLevel::Error);
        return;
    }

    // One UTF-8 file (no BOM) per string, named by its position: 0000.txt, 0001.txt, ...
    std::filesystem::create_directories(output_dir);
    const auto& strings = it->second.strings;
    for (size_t i = 0; i < strings.size(); i++) {
        char name[16];
        std::snprintf(name, sizeof(name), "%04zu.txt", i);
        const std::filesystem::path file_path = std::filesystem::path(output_dir) / name;
        std::ofstream file(file_path, std::ios::binary);
        if (!file) {
            Log("Failed to create file: " + file_path.string(), LogLevel::Error);
            continue;
        }
        file.write(strings[i].data(), static_cast<std::streamsize>(strings[i].size()));
    }

    Log("Exported " + std::to_string(strings.size()) + " strings of locale " + LOCALE_NAMES.at(locale)
        + " to directory: " + output_dir);
}

void LocaleHandler::ExportBlocks(const std::string& output_dir) const
{
    for (const auto& [locale, locale_str] : LOCALE_NAMES)
        ExportBlock(locale, (std::filesystem::path(output_dir) / locale_str).string());
}
