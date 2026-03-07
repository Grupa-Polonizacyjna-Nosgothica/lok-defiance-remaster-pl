#include <algorithm>

#include "LocaleHandler.hpp"
#include "Logger.hpp"

LocaleHandler::LocaleHandler(const uint8_t* data, const size_t fileSize)
    : m_data(data), m_fileSize(fileSize)
{
    auto check_block = [&](const Locale locale, const DataBlockInfo block_info)
    {
        if (block_info.end() > m_fileSize)
            return false;

        const auto header = GetHeader(locale);
        if(std::memcmp(m_data + block_info.offset(), header.data(), 8) != 0)
            return false;
        //check if last block byte is a 0 and if the next byte is not a 0 to confirm block boundary
        if (const auto end = block_info.end();
            m_data[end - 1] !=0 || (end < m_fileSize && m_data[end] ==0))
            return false;

        return true;
    };

    Log("Checking known locale blocks...");
    for (const auto& [locale, block_info] : KNOWN_BLOCKS) {
        if (check_block(locale, block_info)) {
            Log("Known block for locale " + std::to_string(static_cast<int>(locale)) + " is valid.");
            auto block_data = LoadBlock(block_info);
            m_loaded_blocks[locale] = std::move(block_data);
        }
        else
        {
            Log("Known block for locale " + std::to_string(static_cast<int>(locale)) + " is invalid.\n"
                "Searching for it manually...");
            if(const auto found_block_info = QuickSearch(locale); found_block_info.offset() != 0)
            {
                Log("Block for locale " + std::to_string(static_cast<int>(locale)) + " found at offset "
                    + std::to_string(found_block_info.offset()) + " with size " + std::to_string(found_block_info.size()));
                auto block_data = LoadBlock(found_block_info);
                m_loaded_blocks[locale] = std::move(block_data);
            }
             else
            {
                Log("Failed to find block for locale " + std::to_string(static_cast<int>(locale)) + ".", LogLevel::Error);
            }
        }
    }
}

std::array<uint8_t, 8> LocaleHandler::GetHeader(const Locale locale)
{
    std::array<uint8_t, 8> header{};
    const auto& locale_header = LOCALE_HEADERS.at(locale);
    std::copy_n(locale_header.begin(), 4, header.begin());
    std::copy_n(MAGIC_NUMBER, 4, header.begin() + 4);
    return header;
}

std::vector<uint8_t> LocaleHandler::LoadBlock(const DataBlockInfo& block_info) const
{
    if (block_info.end() > m_fileSize) {
        Log("Block end exceeds file size.", LogLevel::Error);
        return {};
    }

    return std::vector(m_data + block_info.offset(), m_data + block_info.end());
}


DataBlockInfo LocaleHandler::QuickSearch(const Locale locale) const
{
    size_t block_offset = 0;
    const size_t table_limit = std::min(MAX_SCAN_OFFSET, m_fileSize);

    for (size_t table_pos = 0; table_pos + 4 <= table_limit; table_pos += 4) {
        const uint32_t candidate_offset = read_u32_le(m_data + table_pos);

        if (candidate_offset >= m_fileSize - 8)
            continue;

        if (const uint8_t* header = m_data + candidate_offset;
            std::memcmp(header, LOCALE_HEADERS[locale].data(), 4) == 0 &&
            std::memcmp(header + 4, MAGIC_NUMBER, 4) == 0)
        {
            block_offset = candidate_offset;
            break;
        }
    }

    if (block_offset == 0) {
        Log("Quick search failed for locale " + std::to_string(static_cast<int>(locale)) +
            ". No valid block found.", LogLevel::Error);
        return DataBlockInfo(0, 0);
    }

    // Determine block size by finding the next block or end of file
    // Right after the 8 byte header, there are 4 empty bytes, and then the internal offset table starts
    // Take the first offset from this table and add it to the block start offset to get to the first string in the block
    const size_t first_string_offset = block_offset + read_u32_le(m_data + block_offset + 12);

    // Every string is null-terminated and then the next string starts immediately after
    // So when first two consecutive zero bytes are found, this means we reached the beginning of the block padding
    size_t pos = first_string_offset;
    while (pos + 1 < m_fileSize) {
        if (m_data[pos] == 0 && m_data[pos + 1] == 0) {
            // Found padding - now continue seeking until a non-zero byte is found
            pos += 2;
            while (pos < m_fileSize && m_data[pos] == 0) {
                pos++;
            }
            // pos now points to the first non-zero byte (end of block) or end of file
            break;
        }
        pos++;
    }

    const size_t block_size = pos > block_offset ? pos - block_offset : 0;
    return DataBlockInfo(block_offset, block_size);
}
