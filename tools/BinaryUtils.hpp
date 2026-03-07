#ifndef BINARY_UTILS_HPP_
#define BINARY_UTILS_HPP_

#include <cstdint>

inline uint32_t read_u32_le(const uint8_t* ptr) {
    return static_cast<uint32_t>(ptr[0])       |
           static_cast<uint32_t>(ptr[1]) << 8  |
           static_cast<uint32_t>(ptr[2]) << 16 |
           static_cast<uint32_t>(ptr[3]) << 24;
}

class DataBlockInfo
{
public:
    DataBlockInfo(const size_t offset, const size_t size)
        : m_offset(offset), m_size(size) {}

    size_t offset() const { return m_offset; }
    size_t size() const { return m_size; }
    size_t end() const { return m_offset + m_size; }
private:
    size_t m_offset;
    size_t m_size;
};

#endif //BINARY_UTILS_HPP_