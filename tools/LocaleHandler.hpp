#ifndef LOCALE_HANDLER_HPP_
#define LOCALE_HANDLER_HPP_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "BinaryUtils.hpp"

enum class Locale { EN, FR, DE, IT, ES, JP, PT, CN, RU };
inline std::map<Locale, std::string> LOCALE_NAMES = {
    {Locale::EN, "EN"},
    {Locale::FR, "FR"},
    {Locale::DE, "DE"},
    {Locale::IT, "IT"},
    {Locale::ES, "ES"},
    {Locale::JP, "JP"},
    {Locale::PT, "PT"},
    {Locale::CN, "CN"},
    {Locale::RU, "RU"}
};

// Language names as used by the game to build the "PC<Language>\locals.bin" path
inline std::map<Locale, std::string> LOCALE_LANGUAGES = {
    {Locale::EN, "English"},
    {Locale::FR, "French"},
    {Locale::DE, "German"},
    {Locale::IT, "Italian"},
    {Locale::ES, "Spanish"},
    {Locale::JP, "Japanese"},
    {Locale::PT, "Portuguese"},
    {Locale::CN, "Chinese"},
    {Locale::RU, "Russian"}
};

// First u32 of a locals.bin block
inline std::map<Locale, uint32_t> LOCALE_IDS = {
    {Locale::EN, 0},
    {Locale::FR, 1},
    {Locale::DE, 2},
    {Locale::IT, 3},
    {Locale::ES, 4},
    {Locale::JP, 5},
    {Locale::PT, 6},
    {Locale::CN, 9},
    {Locale::RU, 10}
};

// Layout of the bigfile index (see tools/bigfile/FORMAT.md):
// u32 header (low 16 bits = entry count N), N sorted u32 name hashes, N entries of 0x18 bytes
constexpr size_t BIGFILE_ENTRY_SIZE = 0x18;

class LocaleHandler
{
public:
    LocaleHandler(const uint8_t* data, size_t fileSize);
    void ExportBlock(Locale locale, const std::string& output_dir) const;
    void ExportBlocks(const std::string& output_dir) const;
protected:
    const uint8_t* m_data;
    size_t m_fileSize;
    uint32_t m_entryCount = 0;
    std::map<Locale, std::vector<uint8_t>> m_loaded_blocks;

    [[nodiscard]] std::optional<DataBlockInfo> FindFile(const std::string& path) const;
    [[nodiscard]] std::vector<uint8_t> LoadBlock(const DataBlockInfo& block_info) const;
    static std::vector<std::string> ExtractStrings(const std::vector<uint8_t>& block_data);
};


#endif //LOCALE_HANDLER_HPP_
