#ifndef BIGFILE_INDEX_HPP_
#define BIGFILE_INDEX_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Index at the start of bigfile.x64.dat / bigfilehd.dat (see tools/bigfile/FORMAT.md):
// u32 header (low 16 bits = entry count N), N sorted u32 path hashes, N entries of 0x18 bytes
constexpr size_t BIGFILE_ENTRY_SIZE = 0x18;
constexpr uint64_t BIGFILE_ALIGN = 0x800;

struct BigfileEntry
{
    uint64_t size;
    uint64_t offset;
    uint64_t extra; // always 0 so far
};

class BigfileIndex
{
public:
    // Throws std::runtime_error if the index does not fit in the file
    BigfileIndex(const uint8_t* data, size_t fileSize);

    [[nodiscard]] uint32_t Count() const { return m_count; }
    [[nodiscard]] uint32_t Hash(size_t slot) const;
    [[nodiscard]] BigfileEntry Entry(size_t slot) const;
    // Slots can share a hash (the same record listed once per language)
    [[nodiscard]] std::vector<size_t> FindSlots(uint32_t hash) const;
    [[nodiscard]] std::vector<size_t> FindSlots(const std::string& path) const;
    // File position of a slot's 0x18 byte entry
    [[nodiscard]] size_t EntryPosition(size_t slot) const;
    // Size of header + hashes + entries, i.e. everything a patch may rewrite in the index
    [[nodiscard]] size_t IndexSize() const { return EntryPosition(m_count); }
    // Bytes of the entry as stored: size, offset, extra
    static void EncodeEntry(const BigfileEntry& entry, uint8_t* out);

private:
    const uint8_t* m_data;
    uint32_t m_count = 0;
};

#endif //BIGFILE_INDEX_HPP_
