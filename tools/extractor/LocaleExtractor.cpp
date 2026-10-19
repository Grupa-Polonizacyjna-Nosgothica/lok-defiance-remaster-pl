#include "CinematicExporter.hpp"
#include "LocaleHandler.hpp"
#include "MappedFile.hpp"
#include "Logger.hpp"

#include <filesystem>

auto FILENAME = "bigfile.x64.dat";
auto OUTPUT_ROOT = "output";

// Usage: LocaleExtractor [bigfile.x64.dat] [output_root]
// Writes <output_root>/strings/EN (locals.bin) and <output_root>/cinematic/EN (TextEntry and language blocks)
int main(const int argc, char* argv[]) {
    const char* filename = argc > 1 ? argv[1] : FILENAME;
    const std::filesystem::path root = argc > 2 ? argv[2] : OUTPUT_ROOT;

    MappedFile file;

    try {
        file.Open(filename);
    } catch (const std::exception& e) {
        Log(e.what(), LogLevel::Error);
        return -2;
    }

    try {
        const LocaleHandler locales(file.Data(), file.Size());
        locales.ExportBlock(Locale::EN, (root / "strings" / "EN").string());
        ExportCinematicText(file.Data(), file.Size(), BigfileIndex(file.Data(), file.Size()),
                            (root / "cinematic" / "EN").string());
    } catch (const std::exception& e) {
        Log(e.what(), LogLevel::Error);
        return -3;
    }

    return 0;
}
