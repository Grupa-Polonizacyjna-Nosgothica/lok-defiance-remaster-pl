#include "LocaleHandler.hpp"
#include "MappedFile.hpp"
#include "Logger.hpp"

auto FILENAME = "bigfile.x64.dat";

int main(const int argc, char* argv[]) {
    if (argc < 2) {
        Log("Usage: " + std::string(argv[0]) + " <output_directory> [input_file]", LogLevel::Error);
        return -1;
    }

    const char* output_dir = argv[1];
    const char* filename = argc > 2 ? argv[2] : FILENAME;

    MappedFile file;

    try {
        file.Open(filename);
    } catch (const std::exception& e) {
        Log(e.what(), LogLevel::Error);
        return -2;
    }

    const LocaleHandler locales(file.Data(), file.Size());
    locales.ExportBlocks(output_dir);

    return 0;
}