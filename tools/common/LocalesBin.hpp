#ifndef LOCALES_BIN_HPP_
#define LOCALES_BIN_HPP_

#include <cstdint>
#include <string>
#include <vector>

// PC<Language>\locals.bin: u32 locale id, u32 count, count u32 offsets (indexed by string ID,
// 0 = empty), then the NUL-terminated strings. Several IDs may share one string.
struct LocalesBin
{
    uint32_t localeId = 0;
    // Distinct strings in the order they are stored
    std::vector<std::string> strings;
    // For every string ID: index into `strings`, or -1 for an empty ID
    std::vector<int32_t> idToString;

    // Throws std::runtime_error on a malformed block
    static LocalesBin Parse(const uint8_t* data, size_t size);
    [[nodiscard]] std::vector<uint8_t> Build() const;
};

#endif //LOCALES_BIN_HPP_
