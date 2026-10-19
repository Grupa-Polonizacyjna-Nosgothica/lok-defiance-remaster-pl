#include <algorithm>
#include <cstring>

#include "CrBlocks.hpp"

namespace
{
    bool IsDigit(const uint8_t c) { return c >= '0' && c <= '9'; }

    bool IsValidUtf8(const uint8_t* s, const size_t n)
    {
        size_t i = 0;
        while (i < n) {
            const uint8_t c = s[i];
            size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
            if (len == 0 || i + len > n) return false;
            for (size_t k = 1; k < len; k++)
                if ((s[i + k] & 0xC0) != 0x80) return false;
            i += len;
        }
        return true;
    }

    // Tries to read one block starting at `start`
    bool ParseAt(const uint8_t* data, const size_t size, const size_t start, CrBlocks::Block& block)
    {
        block = {};
        block.begin = start;
        size_t p = start;
        unsigned seen = 0; // bit mask of the languages read so far
        while (p + 1 < size && IsDigit(data[p]) && data[p + 1] == '\r') {
            // Blocks can follow each other directly; a language that repeats starts the next block
            if (seen & (1u << (data[p] - '0'))) break;
            seen |= 1u << (data[p] - '0');
            const size_t textBegin = p + 2;
            size_t q = textBegin;
            while (q < size && data[q] >= 0x20 && q - textBegin <= 1000) q++; // no control bytes in real texts
            if (q >= size || data[q] != '\r' || q == textBegin || q - textBegin > 1000) return false;
            if (!IsValidUtf8(data + textBegin, q - textBegin)) return false;
            block.entries.push_back({data[p] - '0', textBegin, q});
            p = q + 1;
        }
        block.end = p;
        if (block.entries.size() < 3) return false;

        bool hasEnglish = false;
        std::vector<int> langs;
        for (const auto& e : block.entries) {
            langs.push_back(e.lang);
            if (e.lang == CrBlocks::kEnglish) {
                hasEnglish = true;
                int letters = 0;
                for (size_t i = e.textBegin; i < e.textEnd; i++)
                    if ((data[i] | 0x20) >= 'a' && (data[i] | 0x20) <= 'z') letters++;
                if (letters < 2) return false;
            }
        }
        std::sort(langs.begin(), langs.end());
        return hasEnglish && std::unique(langs.begin(), langs.end()) - langs.begin() >= 3;
    }
}

std::vector<CrBlocks::Block> CrBlocks::Find(const uint8_t* data, const size_t size)
{
    std::vector<Block> blocks;
    size_t i = 0;
    while (i + 4 < size) {
        // Jump to the next CR; a block starts one byte before it
        const void* cr = std::memchr(data + i + 1, '\r', size - i - 1);
        if (!cr) break;
        const size_t crPos = static_cast<const uint8_t*>(cr) - data;
        if (IsDigit(data[crPos - 1])) {
            Block block;
            if (ParseAt(data, size, crPos - 1, block)) {
                blocks.push_back(block);
                i = block.end - 1;
                continue;
            }
        }
        i = crPos;
    }
    return blocks;
}

bool CrBlocks::SetRussian(uint8_t* data, const Block& block, const std::string& text, size_t& shortBy, bool* usedDonor)
{
    if (usedDonor) *usedDonor = false;
    const size_t length = block.end - block.begin;
    size_t overhead = 0;
    for (size_t k = 0; k < block.entries.size(); k++) overhead += 3; // digit, CR, CR
    const size_t available = length - overhead;

    std::vector<size_t> others; // indexes of entries that are neither English nor Russian
    size_t englishLen = 0, russianIdx = block.entries.size();
    for (size_t k = 0; k < block.entries.size(); k++) {
        const auto& e = block.entries[k];
        if (e.lang == kEnglish) englishLen = e.textEnd - e.textBegin;
        else if (e.lang == kRussian) russianIdx = k;
        else others.push_back(k);
    }
    if (russianIdx == block.entries.size()) {
        // Not every block has a Russian entry; reuse the largest other language for it
        if (others.empty()) { shortBy = 0; return false; }
        const auto donor = std::max_element(others.begin(), others.end(), [&](size_t a, size_t b) {
            return block.entries[a].textEnd - block.entries[a].textBegin < block.entries[b].textEnd - block.entries[b].textBegin;
        });
        russianIdx = *donor;
        others.erase(donor);
        if (usedDonor) *usedDonor = true;
    }

    // Keep other languages as they are while they fit, shortest first; every entry needs one byte at least
    std::sort(others.begin(), others.end(), [&](size_t a, size_t b) {
        return block.entries[a].textEnd - block.entries[a].textBegin < block.entries[b].textEnd - block.entries[b].textBegin;
    });
    const size_t fixed = englishLen + text.size();
    if (fixed + others.size() > available) {
        shortBy = fixed + others.size() - available;
        return false;
    }
    size_t budget = available - fixed;
    std::vector<size_t> otherLen(block.entries.size(), 0);
    size_t remaining = others.size();
    for (const size_t k : others) {
        remaining--;
        const size_t orig = block.entries[k].textEnd - block.entries[k].textBegin;
        // Reserve one byte for each entry still to be placed
        const size_t room = budget - remaining;
        otherLen[k] = std::min(orig, room);
        if (otherLen[k] < orig) otherLen[k] = 1;
        budget -= otherLen[k];
    }

    // Rebuild the block; the Russian text is padded with spaces to use up leftover bytes
    std::vector<uint8_t> out;
    out.reserve(length);
    for (size_t k = 0; k < block.entries.size(); k++) {
        const auto& e = block.entries[k];
        out.push_back(static_cast<uint8_t>('0' + (k == russianIdx ? kRussian : e.lang)));
        out.push_back('\r');
        if (k == russianIdx) {
            out.insert(out.end(), text.begin(), text.end());
            out.insert(out.end(), budget, ' ');
        } else if (e.lang == kEnglish || otherLen[k] == e.textEnd - e.textBegin) {
            out.insert(out.end(), data + e.textBegin, data + e.textEnd);
        } else {
            out.push_back(' ');
        }
        out.push_back('\r');
    }
    if (out.size() != length) { shortBy = 0; return false; }
    std::memcpy(data + block.begin, out.data(), length);
    return true;
}
