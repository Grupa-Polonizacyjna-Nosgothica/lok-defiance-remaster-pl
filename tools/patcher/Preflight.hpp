#ifndef PREFLIGHT_HPP_
#define PREFLIGHT_HPP_

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "BigfileIndex.hpp"
#include "PatchBackup.hpp"
#include "Patcher.hpp"

// Exit codes of PolishPatcher, so that an installer can tell the reasons apart
enum class ExitCode : int
{
    Ok = 0,
    Usage = 1,
    Error = 2,
    UnsupportedVersion = 3,
    GameRunning = 4,
    NotEnoughSpace = 5,
    FileChanged = 6,
    NotWritable = 7,
};

class PreflightFailure : public std::runtime_error
{
public:
    PreflightFailure(const ExitCode code, const std::string& message) : std::runtime_error(message), m_code(code) {}
    [[nodiscard]] ExitCode Code() const { return m_code; }
private:
    ExitCode m_code;
};

// The build of bigfile.x64.dat the patch was made for. The English locals.bin is never changed by
// the patch, so a patched file passes this check as well.
constexpr uint32_t EXPECTED_ENTRY_COUNT = 18802;
constexpr uint64_t EXPECTED_ENGLISH_LOCALS_SIZE = 465568;
constexpr uint32_t EXPECTED_ENGLISH_LOCALS_CRC = 0x51E16C91;

// Throws PreflightFailure(UnsupportedVersion) if the file is not that build
void CheckGameVersion(const uint8_t* data, size_t fileSize, const BigfileIndex& index);

[[nodiscard]] bool IsGameRunning();

// Throws PreflightFailure(NotWritable)
void CheckWritable(const std::filesystem::path& bigfile);

// The patch appends data and writes a backup of the same order of size; needs that much plus a margin
void CheckFreeSpace(const std::filesystem::path& bigfile, const PatchPlan& plan);

enum class FileState
{
    Original,     // matches the backup's original state (e.g. restored by the game client)
    Patched,      // still exactly as left by our patch
    Interrupted,  // a patch was started but never finished
    Changed,      // neither: the file was modified by something else since
};

// How the game file relates to a backup
[[nodiscard]] FileState ClassifyFile(const std::filesystem::path& bigfile, const PatchBackup& backup);

#endif //PREFLIGHT_HPP_
