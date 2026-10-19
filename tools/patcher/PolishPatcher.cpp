#include <filesystem>
#include <iostream>
#include <string>

#include "Logger.hpp"
#include "MappedFile.hpp"
#include "PatchBackup.hpp"
#include "Patcher.hpp"

namespace fs = std::filesystem;

namespace
{
    void Usage()
    {
        std::cout << "Usage:\n"
                     "  PolishPatcher patch   [bigfile.x64.dat] [translation_root]\n"
                     "  PolishPatcher restore [bigfile.x64.dat]\n\n"
                     "patch writes the translation (translation_root/strings/PL, cinematic/PL and fonts) over the\n"
                     "Russian texts of the game and keeps a backup next to the file; restore undoes it.\n";
    }

    // Returns true if there was a patch to undo
    bool Restore(const fs::path& bigfile)
    {
        const fs::path backupPath = PatchBackup::PathFor(bigfile);
        if (!fs::exists(backupPath)) return false;
        PatchBackup::Load(backupPath).Restore(bigfile);
        fs::remove(backupPath);
        return true;
    }
}

int main(const int argc, char* argv[])
{
    if (argc < 2) { Usage(); return 1; }
    const std::string command = argv[1];
    const fs::path bigfile = argc > 2 ? argv[2] : "bigfile.x64.dat";

    try {
        if (!fs::exists(bigfile)) throw std::runtime_error(bigfile.string() + " not found.");

        if (command == "restore") {
            if (Restore(bigfile)) Log("Original game file restored.");
            else Log("No patch backup found next to " + bigfile.string() + "; nothing to restore.");
            return 0;
        }
        if (command != "patch") { Usage(); return 1; }

        const fs::path root = argc > 3 ? argv[3] : ".";
        if (Restore(bigfile)) Log("Removed the previous patch first.");

        PatchPlan plan;
        {
            MappedFile file;
            file.Open(bigfile.string().c_str());
            plan = BuildPlan(file.Data(), file.Size(), BigfileIndex(file.Data(), file.Size()), root);
        }
        Log("Patching: " + std::to_string(plan.localeStrings) + " strings, " + std::to_string(plan.fontFiles) + " font files, "
            + std::to_string(plan.textEntries) + " subtitle records, "
            + std::to_string(plan.blocksPatched) + " language blocks ("
            + std::to_string(plan.blocksWithoutRussian) + " of them had no Russian entry; "
            + std::to_string(plan.blocksTooLong) + " too long, " + std::to_string(plan.blocksSkipped) + " skipped).");
        ApplyPlan(bigfile, plan);
        VerifyPatch(bigfile, root);
        Log("Done. Select Russian in the game launcher to play the Polish translation.");
    } catch (const std::exception& e) {
        Log(e.what(), LogLevel::Error);
        return 2;
    }
    return 0;
}
