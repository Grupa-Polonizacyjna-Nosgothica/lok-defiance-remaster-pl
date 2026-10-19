#ifndef CR_BLOCKS_HPP_
#define CR_BLOCKS_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Cinematic and gameplay records embed short multi-language texts as CR-delimited blocks:
//   <lang digit> CR <text> CR   (repeated, languages in any order)
// e.g. `7\r<Chinese>\r0\rSurround him!\r1\rCernez-le !\r...\r8\r<Russian>\r`. Whatever follows
// (often a NUL, sometimes other binary data) is not part of the block.
// The block sits inside a binary record, so its total length cannot change.
namespace CrBlocks
{
    // Language digits used inside the blocks (differs from the locals.bin header ids)
    constexpr int kEnglish = 0;
    constexpr int kRussian = 8;

    struct Entry
    {
        int lang;
        size_t textBegin; // offset in the scanned buffer
        size_t textEnd;   // exclusive, points at the CR after the text
    };

    struct Block
    {
        size_t begin; // first byte of the first entry
        size_t end;   // exclusive, after the last CR
        std::vector<Entry> entries;
    };

    // Finds all well-formed blocks, in the order they occur
    [[nodiscard]] std::vector<Block> Find(const uint8_t* data, size_t size);

    // Rewrites `block` in `data` (same length) so that the Russian entry holds `text`.
    // Space is taken from other languages (never English) when the Russian slot is too small;
    // those entries are cut down to a single space. Blocks without a Russian entry reuse the
    // largest other language: its digit becomes 8 (`*usedDonor` is set). Returns false (data
    // untouched) if the text does not fit, with the missing byte count in `shortBy` (0 if the
    // block has nothing to reuse).
    bool SetRussian(uint8_t* data, const Block& block, const std::string& text, size_t& shortBy, bool* usedDonor = nullptr);
}

#endif //CR_BLOCKS_HPP_
