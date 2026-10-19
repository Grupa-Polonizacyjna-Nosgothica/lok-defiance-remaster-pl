#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Logger.hpp"
#include "MappedFile.hpp"
#include "PatchBackup.hpp"
#include "Patcher.hpp"
#include "Preflight.hpp"

namespace fs = std::filesystem;

namespace
{
    void Usage()
    {
        std::cout << "Usage:\n"
                     "  PolishPatcher patch   [bigfile.x64.dat] [translation] [--force]\n"
                     "  PolishPatcher restore [bigfile.x64.dat] [--force]\n"
                     "  PolishPatcher check   [bigfile.x64.dat]\n"
                     "  PolishPatcher pack    <translation_dir> <out.pak>\n\n"
                     "patch writes the translation (a directory laid out like the repository, or a .pak file)\n"
                     "over the Russian texts of the game and keeps a backup next to the file; restore undoes it;\n"
                     "check reports the state of the game file without changing anything;\n"
                     "pack makes the single-file package that is shipped to players.\n"
                     "--force skips the game build check, the running-game check and the changed-file refusal.\n\n"
                     "Exit codes: 0 ok, 1 usage, 2 error, 3 unsupported game build, 4 game running,\n"
                     "5 not enough disk space, 6 game file changed since patching, 7 file not writable.\n";
    }

    const char* StateName(const FileState state)
    {
        switch (state) {
            case FileState::Original: return "original (a patch backup exists, but the file was already restored)";
            case FileState::Patched: return "patched";
            case FileState::Interrupted: return "a previous patch was interrupted";
            case FileState::Changed: return "changed by something else since it was patched";
        }
        return "";
    }

    // Undoes an earlier patch if there is one. Returns true if the file was restored.
    bool UndoEarlierPatch(const fs::path& bigfile, const bool force)
    {
        const fs::path backupPath = PatchBackup::PathFor(bigfile);
        if (!fs::exists(backupPath)) return false;

        const PatchBackup backup = PatchBackup::Load(backupPath);
        switch (ClassifyFile(bigfile, backup)) {
            case FileState::Original:
                fs::remove(backupPath);
                return false;
            case FileState::Patched:
            case FileState::Interrupted:
                backup.Restore(bigfile);
                fs::remove(backupPath);
                return true;
            case FileState::Changed:
                if (!force)
                    throw PreflightFailure(ExitCode::FileChanged,
                                           "The game file was changed after it was patched (for example by a game update "
                                           "or a file verification), so the old backup can no longer be used. Verify the game "
                                           "files in your game client, delete " + backupPath.string() + " and try again, "
                                           "or use --force to ignore the backup.");
                Log("Ignoring an outdated backup (--force).", LogLevel::Error);
                fs::remove(backupPath);
                return false;
        }
        return false;
    }

    void CheckNotRunning(const bool force)
    {
        if (!force && IsGameRunning())
            throw PreflightFailure(ExitCode::GameRunning, "The game is running. Close it (and its launcher) and try again.");
    }

    // Reports everything and returns the exit code of the first problem found (0 if none), so that an
    // installer can show a fitting message without parsing the text.
    int Check(const fs::path& bigfile)
    {
        ExitCode result = ExitCode::Ok;
        auto problem = [&result](const ExitCode code, const std::string& message) {
            Log(message, LogLevel::Error);
            if (result == ExitCode::Ok) result = code;
        };

        const fs::path backupPath = PatchBackup::PathFor(bigfile);
        if (fs::exists(backupPath))
            Log(std::string("Game file state: ") + StateName(ClassifyFile(bigfile, PatchBackup::Load(backupPath))));
        else
            Log("Game file state: no patch applied by this tool");

        try {
            MappedFile file;
            file.Open(bigfile.string().c_str());
            CheckGameVersion(file.Data(), file.Size(), BigfileIndex(file.Data(), file.Size()));
            Log("Game build: supported");
        } catch (const PreflightFailure& e) {
            problem(e.Code(), e.what());
        } catch (const std::exception& e) {
            problem(ExitCode::Error, e.what());
        }
        if (IsGameRunning()) problem(ExitCode::GameRunning, "The game is running.");
        try { CheckWritable(bigfile); Log("Game file: writable"); }
        catch (const PreflightFailure& e) { problem(e.Code(), e.what()); }
        return static_cast<int>(result);
    }
}

int main(const int argc, char* argv[])
{
    std::vector<std::string> args;
    bool force = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--force") == 0) force = true;
        else args.push_back(argv[i]);
    }
    if (args.empty()) { Usage(); return static_cast<int>(ExitCode::Usage); }
    const std::string command = args[0];

    try {
        if (command == "pack") {
            if (args.size() < 3) { Usage(); return static_cast<int>(ExitCode::Usage); }
            const auto source = OpenTranslationSource(args[1]);
            Log("Packed " + std::to_string(WritePackage(*source, args[2])) + " files into " + args[2]);
            return 0;
        }
        if (command != "patch" && command != "restore" && command != "check") { Usage(); return static_cast<int>(ExitCode::Usage); }

        const fs::path bigfile = args.size() > 1 ? args[1] : "bigfile.x64.dat";
        if (!fs::exists(bigfile)) throw std::runtime_error(bigfile.string() + " not found.");

        if (command == "check") return Check(bigfile);

        if (command == "restore") {
            CheckNotRunning(force);
            if (UndoEarlierPatch(bigfile, force)) Log("Original game file restored.");
            else Log("Nothing to restore: no patch backup, or the file is already original.");
            return 0;
        }

        // patch
        const auto source = OpenTranslationSource(args.size() > 2 ? args[2] : ".");
        CheckNotRunning(force);
        CheckWritable(bigfile);
        if (UndoEarlierPatch(bigfile, force)) Log("Removed the previous patch first.");

        PatchPlan plan;
        {
            MappedFile file;
            file.Open(bigfile.string().c_str());
            const BigfileIndex index(file.Data(), file.Size());
            if (force) Log("Skipping the game build check (--force).", LogLevel::Error);
            else CheckGameVersion(file.Data(), file.Size(), index);
            plan = BuildPlan(file.Data(), file.Size(), index, *source);
        }
        CheckFreeSpace(bigfile, plan);

        Log("Patching from " + source->Describe() + ": " + std::to_string(plan.localeStrings) + " strings, "
            + std::to_string(plan.fontFiles) + " font files, "
            + std::to_string(plan.textEntries) + " subtitle records, "
            + std::to_string(plan.blocksPatched) + " language blocks ("
            + std::to_string(plan.blocksWithoutRussian) + " of them had no Russian entry; "
            + std::to_string(plan.blocksTooLong) + " too long, " + std::to_string(plan.blocksSkipped) + " skipped).");
        ApplyPlan(bigfile, plan);
        VerifyPatch(bigfile, *source);
        Log("Done. Select Russian in the game launcher to play the Polish translation.");
    } catch (const PreflightFailure& e) {
        Log(e.what(), LogLevel::Error);
        return static_cast<int>(e.Code());
    } catch (const std::exception& e) {
        Log(e.what(), LogLevel::Error);
        return static_cast<int>(ExitCode::Error);
    }
    return 0;
}
