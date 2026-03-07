#ifndef LOCALE_HANDLER_HPP_
#define LOCALE_HANDLER_HPP_

#include <array>
#include <cstdint>
#include <map>
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

inline std::map<Locale, DataBlockInfo> KNOWN_BLOCKS = {
    {Locale::RU, DataBlockInfo(0x08B800, 0xA3800)},
    {Locale::PT, DataBlockInfo(0x12F000, 0x72000)},
    {Locale::JP, DataBlockInfo(0x1A1000, 0x79800)},
    {Locale::CN, DataBlockInfo(0x21A800, 0x62800)},
    {Locale::ES, DataBlockInfo(0x27D000, 0x75800)},
    {Locale::DE, DataBlockInfo(0x2F2800, 0x76800)},
    {Locale::IT, DataBlockInfo(0x369000, 0x73800)},
    {Locale::FR, DataBlockInfo(0x3DC800, 0x78804)},
    {Locale::EN, DataBlockInfo(0x455000, 0x6D000)},
};

constexpr size_t MAX_SCAN_OFFSET = 0x7BFFF;
constexpr uint8_t MAGIC_NUMBER[4] = {0x0B, 0x10, 0x00, 0x00};
inline std::map<Locale, std::array<uint8_t, 4>> LOCALE_HEADERS = {
    {Locale::EN, {0x00, 0x00, 0x00, 0x00}},
    {Locale::FR, {0x01, 0x00, 0x00, 0x00}},
    {Locale::DE, {0x02, 0x00, 0x00, 0x00}},
    {Locale::IT, {0x03, 0x00, 0x00, 0x00}},
    {Locale::ES, {0x04, 0x00, 0x00, 0x00}},
    {Locale::JP, {0x05, 0x00, 0x00, 0x00}},
    {Locale::PT, {0x06, 0x00, 0x00, 0x00}},
    {Locale::CN, {0x09, 0x00, 0x00, 0x00}},
    {Locale::RU, {0x0A, 0x00, 0x00, 0x00}}
};

class LocaleHandler
{
public:
    LocaleHandler(const uint8_t* data, size_t fileSize);
    void ExportBlock(Locale locale, const std::string& output_dir) const;
    void ExportBlocks(const std::string& output_dir) const;
protected:
    const uint8_t* m_data;
    size_t m_fileSize;
    std::map<Locale, std::vector<uint8_t>> m_loaded_blocks;

    static std::array<uint8_t, 8> GetHeader(Locale locale);
    [[nodiscard]] std::vector<uint8_t> LoadBlock(const DataBlockInfo& block_info) const;
    [[nodiscard]] DataBlockInfo QuickSearch(Locale locale) const;
    static std::vector<std::string> ExtractStrings(const std::vector<uint8_t>& block_data);
};


#endif //LOCALE_HANDLER_HPP_
