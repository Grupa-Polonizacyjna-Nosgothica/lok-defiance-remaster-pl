#include <algorithm>
#include <cstring>
#include <map>
#include <stdexcept>

#include "BinaryUtils.hpp"
#include "LocalesBin.hpp"

LocalesBin LocalesBin::Parse(const uint8_t* data, const size_t size)
{
    if (size < 8)
        throw std::runtime_error("locals.bin block is too small.");

    LocalesBin result;
    result.localeId = read_u32_le(data);
    const uint32_t count = read_u32_le(data + 4);
    if (8 + static_cast<size_t>(count) * 4 > size)
        throw std::runtime_error("locals.bin offset table exceeds the block.");

    std::vector<uint32_t> offsets(count);
    for (uint32_t i = 0; i < count; i++)
        offsets[i] = read_u32_le(data + 8 + i * 4);

    // Strings are stored in order of their offsets; give each distinct offset one string
    std::vector<uint32_t> sorted;
    for (const uint32_t off : offsets)
        if (off != 0) sorted.push_back(off);
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    std::map<uint32_t, int32_t> stringOfOffset;
    for (const uint32_t off : sorted) {
        if (off >= size)
            throw std::runtime_error("locals.bin string offset is out of range.");
        const void* end = std::memchr(data + off, 0, size - off);
        if (!end)
            throw std::runtime_error("locals.bin string is not NUL-terminated.");
        stringOfOffset[off] = static_cast<int32_t>(result.strings.size());
        result.strings.emplace_back(reinterpret_cast<const char*>(data + off),
                                    static_cast<const uint8_t*>(end) - (data + off));
    }

    result.idToString.reserve(count);
    for (const uint32_t off : offsets)
        result.idToString.push_back(off == 0 ? -1 : stringOfOffset.at(off));
    return result;
}

std::vector<uint8_t> LocalesBin::Build() const
{
    const size_t count = idToString.size();
    std::vector<uint8_t> out(8 + count * 4);
    auto put_u32 = [&out](const size_t pos, const uint32_t v) {
        for (int i = 0; i < 4; i++) out[pos + i] = static_cast<uint8_t>(v >> (8 * i));
    };
    put_u32(0, localeId);
    put_u32(4, static_cast<uint32_t>(count));

    std::vector<uint32_t> stringOffsets;
    for (const auto& s : strings) {
        stringOffsets.push_back(static_cast<uint32_t>(out.size()));
        out.insert(out.end(), s.begin(), s.end());
        out.push_back(0);
    }
    for (size_t id = 0; id < count; id++)
        put_u32(8 + id * 4, idToString[id] < 0 ? 0 : stringOffsets[idToString[id]]);
    return out;
}
