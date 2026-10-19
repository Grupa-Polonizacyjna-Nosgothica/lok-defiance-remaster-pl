#include <stdexcept>

#include "BigfileIndex.hpp"
#include "BinaryUtils.hpp"

BigfileIndex::BigfileIndex(const uint8_t* data, const size_t fileSize)
    : m_data(data)
{
    if (fileSize < 4)
        throw std::runtime_error("File is too small to contain a bigfile index.");
    m_count = read_u32_le(data) & 0xFFFF;
    if (EntryPosition(m_count) > fileSize)
        throw std::runtime_error("Bigfile index exceeds file size.");
}

uint32_t BigfileIndex::Hash(const size_t slot) const
{
    return read_u32_le(m_data + 4 + slot * 4);
}

size_t BigfileIndex::EntryPosition(const size_t slot) const
{
    return 4 + static_cast<size_t>(m_count) * 4 + slot * BIGFILE_ENTRY_SIZE;
}

BigfileEntry BigfileIndex::Entry(const size_t slot) const
{
    const uint8_t* p = m_data + EntryPosition(slot);
    return { read_u64_le(p), read_u64_le(p + 8), read_u64_le(p + 16) };
}

std::vector<size_t> BigfileIndex::FindSlots(const uint32_t hash) const
{
    // Hashes are sorted; binary search for the first candidate
    size_t lo = 0, hi = m_count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (Hash(mid) < hash) lo = mid + 1;
        else hi = mid;
    }
    std::vector<size_t> slots;
    for (; lo < m_count && Hash(lo) == hash; lo++)
        slots.push_back(lo);
    return slots;
}

std::vector<size_t> BigfileIndex::FindSlots(const std::string& path) const
{
    return FindSlots(bigfile_hash(path));
}

void BigfileIndex::EncodeEntry(const BigfileEntry& entry, uint8_t* out)
{
    for (int i = 0; i < 8; i++) {
        out[i] = static_cast<uint8_t>(entry.size >> (8 * i));
        out[8 + i] = static_cast<uint8_t>(entry.offset >> (8 * i));
        out[16 + i] = static_cast<uint8_t>(entry.extra >> (8 * i));
    }
}
