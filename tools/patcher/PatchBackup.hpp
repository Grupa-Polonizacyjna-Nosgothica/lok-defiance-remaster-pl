#ifndef PATCH_BACKUP_HPP_
#define PATCH_BACKUP_HPP_

#include <cstdint>
#include <filesystem>
#include <vector>

// Sidecar file next to bigfile.x64.dat that holds everything needed to undo a patch:
// the original file size, the original index and the original bytes of every region patched in place.
// Patches only append data and rewrite the index and these regions, so restoring is exact.
class PatchBackup
{
public:
    struct Region
    {
        uint64_t offset;
        std::vector<uint8_t> original;
    };

    static std::filesystem::path PathFor(const std::filesystem::path& bigfile);

    uint64_t originalSize = 0;
    std::vector<uint8_t> originalIndex; // bytes [0, indexSize) of the original file
    std::vector<Region> regions;

    void Save(const std::filesystem::path& file) const;
    static PatchBackup Load(const std::filesystem::path& file);
    // Writes the original bytes back and cuts off appended data
    void Restore(const std::filesystem::path& bigfile) const;
};

#endif //PATCH_BACKUP_HPP_
