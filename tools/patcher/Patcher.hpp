#ifndef PATCHER_HPP_
#define PATCHER_HPP_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "BigfileIndex.hpp"

// Everything a patch changes in bigfile.x64.dat, computed before anything is written.
struct PatchPlan
{
    // An index entry to rewrite: where it is stored in the file and its unchanged third field
    struct Slot
    {
        size_t entryPosition;
        uint64_t extra;
    };
    // A new blob appended at the end of the file; the listed index slots are pointed at it
    struct Append
    {
        std::vector<uint8_t> data;
        std::vector<Slot> slots;
    };
    // New bytes for a region of an existing record (length unchanged)
    struct InPlace
    {
        uint64_t offset;
        std::vector<uint8_t> data;
    };

    size_t indexSize = 0; // bytes at the start of the file that hold the index
    std::vector<Append> appends;
    std::vector<InPlace> inPlace;

    size_t localeStrings = 0;
    size_t fontFiles = 0;
    size_t textEntries = 0;
    size_t blocksPatched = 0;
    size_t blocksWithoutRussian = 0; // patched by reusing another language's slot
    size_t blocksSkipped = 0; // record or block not found
    size_t blocksTooLong = 0; // translation did not fit
};

// Reads the translation from <translationRoot>/strings/PL, cinematic/PL and the fonts in fonts/ and works
// out how to put it in the slots of the Russian language. Throws std::runtime_error on fatal problems.
PatchPlan BuildPlan(const uint8_t* data, size_t fileSize, const BigfileIndex& index,
                    const std::filesystem::path& translationRoot);

// Backs up, then writes the plan into the game file. Throws on failure.
void ApplyPlan(const std::filesystem::path& bigfile, const PatchPlan& plan);

// Re-reads the patched file and checks that the Russian slots now hold the translation
void VerifyPatch(const std::filesystem::path& bigfile, const std::filesystem::path& translationRoot);

#endif //PATCHER_HPP_
