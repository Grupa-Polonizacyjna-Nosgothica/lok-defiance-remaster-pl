#include <cstdio>
#include <fstream>
#include <vector>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
    #include <tlhelp32.h>
#else
    #include <dirent.h>
#endif

#include "BinaryUtils.hpp"
#include "Locales.hpp"
#include "Preflight.hpp"

namespace fs = std::filesystem;

void CheckGameVersion(const uint8_t* data, const size_t fileSize, const BigfileIndex& index)
{
    const std::string hint = " This patch is made for the Steam build of the game. "
                             "Verify the game files in your game client and try again.";
    if (index.Count() != EXPECTED_ENTRY_COUNT)
        throw PreflightFailure(ExitCode::UnsupportedVersion,
                               "bigfile.x64.dat has " + std::to_string(index.Count()) + " entries, expected "
                               + std::to_string(EXPECTED_ENTRY_COUNT) + "." + hint);

    const auto slots = index.FindSlots(LocalesPath(Locale::EN));
    if (slots.empty())
        throw PreflightFailure(ExitCode::UnsupportedVersion, "PCEnglish\\locals.bin is missing." + hint);
    const BigfileEntry e = index.Entry(slots.front());
    if (e.size != EXPECTED_ENGLISH_LOCALS_SIZE || e.offset > fileSize || e.size > fileSize - e.offset
        || crc32_ieee(data + e.offset, e.size) != EXPECTED_ENGLISH_LOCALS_CRC)
        throw PreflightFailure(ExitCode::UnsupportedVersion, "The English texts of the game differ from the expected build." + hint);
}

bool IsGameRunning()
{
#ifdef _WIN32
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    bool running = false;
    for (BOOL ok = Process32First(snapshot, &entry); ok && !running; ok = Process32Next(snapshot, &entry))
        running = _stricmp(entry.szExeFile, "defiance.exe") == 0;
    CloseHandle(snapshot);
    return running;
#else
    // Also covers the game running through Proton/Wine, where the process keeps the .exe name
    DIR* proc = opendir("/proc");
    if (!proc) return false;
    bool running = false;
    while (const dirent* d = readdir(proc)) {
        if (d->d_name[0] < '0' || d->d_name[0] > '9') continue;
        std::ifstream comm(std::string("/proc/") + d->d_name + "/comm");
        std::string name;
        if (comm && std::getline(comm, name) && name == "defiance.exe") {
            running = true;
            break;
        }
    }
    closedir(proc);
    return running;
#endif
}

void CheckWritable(const fs::path& bigfile)
{
    std::fstream f(bigfile, std::ios::in | std::ios::out | std::ios::binary);
    if (!f)
        throw PreflightFailure(ExitCode::NotWritable,
                               "Cannot open " + bigfile.string() + " for writing. Close the game and its launcher, "
                               "and run the patcher as administrator if the game is in a protected folder.");
}

void CheckFreeSpace(const fs::path& bigfile, const PatchPlan& plan)
{
    uint64_t needed = plan.indexSize;
    for (const auto& a : plan.appends) needed += a.data.size() + BIGFILE_ALIGN;
    for (const auto& op : plan.inPlace) needed += 2 * op.data.size();
    needed += 16ull * 1024 * 1024; // margin

    std::error_code ec;
    const auto space = fs::space(bigfile.parent_path().empty() ? fs::path(".") : bigfile.parent_path(), ec);
    if (!ec && space.available < needed)
        throw PreflightFailure(ExitCode::NotEnoughSpace,
                               "Not enough free disk space: " + std::to_string(needed / (1024 * 1024)) + " MB needed, "
                               + std::to_string(space.available / (1024 * 1024)) + " MB available.");
}

FileState ClassifyFile(const fs::path& bigfile, const PatchBackup& backup)
{
    const uint64_t size = fs::file_size(bigfile);
    std::vector<uint8_t> index(backup.originalIndex.size());
    {
        std::ifstream f(bigfile, std::ios::binary);
        f.read(reinterpret_cast<char*>(index.data()), static_cast<std::streamsize>(index.size()));
        if (!f) return FileState::Changed;
    }

    if (backup.patchedSize == 0) // the patch never reached its last step
        return size >= backup.originalSize ? FileState::Interrupted : FileState::Changed;
    if (size == backup.originalSize && index == backup.originalIndex) return FileState::Original;
    if (size == backup.patchedSize && crc32_ieee(index.data(), index.size()) == backup.patchedIndexCrc)
        return FileState::Patched;
    return FileState::Changed;
}
