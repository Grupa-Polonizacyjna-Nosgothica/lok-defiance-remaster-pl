#ifndef BINARY_UTILS_HPP_
#define BINARY_UTILS_HPP_

#include <cstddef>
#include <cstdint>
#include <string>

inline uint32_t read_u32_le(const uint8_t* ptr) {
    return static_cast<uint32_t>(ptr[0])       |
           static_cast<uint32_t>(ptr[1]) << 8  |
           static_cast<uint32_t>(ptr[2]) << 16 |
           static_cast<uint32_t>(ptr[3]) << 24;
}

inline uint64_t read_u64_le(const uint8_t* ptr) {
    return static_cast<uint64_t>(read_u32_le(ptr)) |
           static_cast<uint64_t>(read_u32_le(ptr + 4)) << 32;
}

// Name hash used by the game's bigfile index: CRC-32 (poly 0x04C11DB7, MSB-first,
// init 0xFFFFFFFF, final NOT) over the ASCII-lowercased path.
inline uint32_t bigfile_hash(const std::string& path) {
    uint32_t crc = 0xFFFFFFFF;
    for (const char c : path) {
        const auto b = static_cast<uint8_t>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
        crc ^= static_cast<uint32_t>(b) << 24;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x80000000) ? (crc << 1) ^ 0x04C11DB7 : (crc << 1);
    }
    return ~crc;
}

// Standard CRC-32 (IEEE 802.3, as in zip); `crc` continues a previous result
inline uint32_t crc32_ieee(const uint8_t* data, const size_t size, uint32_t crc = 0) {
    static const auto table = [] {
        struct Table { uint32_t v[256]; } t{};
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t.v[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < size; i++) crc = table.v[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

#endif //BINARY_UTILS_HPP_
