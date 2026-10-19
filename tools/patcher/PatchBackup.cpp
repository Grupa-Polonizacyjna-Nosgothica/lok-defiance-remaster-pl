#include <cstring>
#include <fstream>
#include <stdexcept>

#include "PatchBackup.hpp"

namespace
{
    constexpr char MAGIC[8] = {'L', 'O', 'K', 'P', 'L', 'B', 'K', '2'};

    void PutU64(std::ostream& out, const uint64_t v)
    {
        char b[8];
        for (int i = 0; i < 8; i++) b[i] = static_cast<char>(v >> (8 * i));
        out.write(b, 8);
    }

    uint64_t GetU64(std::istream& in)
    {
        unsigned char b[8];
        in.read(reinterpret_cast<char*>(b), 8);
        if (!in) throw std::runtime_error("Backup file is truncated.");
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(b[i]) << (8 * i);
        return v;
    }

    void GetBytes(std::istream& in, std::vector<uint8_t>& out, const uint64_t n)
    {
        out.resize(n);
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(n));
        if (!in) throw std::runtime_error("Backup file is truncated.");
    }
}

std::filesystem::path PatchBackup::PathFor(const std::filesystem::path& bigfile)
{
    return std::filesystem::path(bigfile).concat(".plbackup");
}

void PatchBackup::Save(const std::filesystem::path& file) const
{
    // Write to a temporary name first so an interrupted save never leaves a valid-looking backup
    const auto tmp = std::filesystem::path(file).concat(".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot create backup file " + tmp.string());
        out.write(MAGIC, sizeof(MAGIC));
        PutU64(out, originalSize);
        PutU64(out, patchedSize);
        PutU64(out, patchedIndexCrc);
        PutU64(out, originalIndex.size());
        out.write(reinterpret_cast<const char*>(originalIndex.data()), static_cast<std::streamsize>(originalIndex.size()));
        PutU64(out, regions.size());
        for (const auto& r : regions) {
            PutU64(out, r.offset);
            PutU64(out, r.original.size());
            out.write(reinterpret_cast<const char*>(r.original.data()), static_cast<std::streamsize>(r.original.size()));
        }
        out.flush();
        if (!out) throw std::runtime_error("Failed to write backup file " + tmp.string());
    }
    std::filesystem::rename(tmp, file);
}

PatchBackup PatchBackup::Load(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open backup file " + file.string());
    char magic[8];
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, MAGIC, sizeof(MAGIC)) != 0)
        throw std::runtime_error(file.string() + " is not a patch backup.");

    PatchBackup backup;
    backup.originalSize = GetU64(in);
    backup.patchedSize = GetU64(in);
    backup.patchedIndexCrc = static_cast<uint32_t>(GetU64(in));
    GetBytes(in, backup.originalIndex, GetU64(in));
    const uint64_t regionCount = GetU64(in);
    for (uint64_t i = 0; i < regionCount; i++) {
        Region r;
        r.offset = GetU64(in);
        GetBytes(in, r.original, GetU64(in));
        backup.regions.push_back(std::move(r));
    }
    return backup;
}

void PatchBackup::Restore(const std::filesystem::path& bigfile) const
{
    if (std::filesystem::file_size(bigfile) < originalSize)
        throw std::runtime_error("The game file is smaller than before patching; it was changed after patching. "
                                 "Verify the game files with your game client instead.");
    {
        std::fstream f(bigfile, std::ios::in | std::ios::out | std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open " + bigfile.string() + " for writing.");
        for (const auto& r : regions) {
            f.seekp(static_cast<std::streamoff>(r.offset));
            f.write(reinterpret_cast<const char*>(r.original.data()), static_cast<std::streamsize>(r.original.size()));
        }
        f.seekp(0);
        f.write(reinterpret_cast<const char*>(originalIndex.data()), static_cast<std::streamsize>(originalIndex.size()));
        f.flush();
        if (!f) throw std::runtime_error("Failed to write to " + bigfile.string());
    }
    std::filesystem::resize_file(bigfile, originalSize);
}
