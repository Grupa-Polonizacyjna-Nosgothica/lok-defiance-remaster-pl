#include "LocaleHandler.hpp"
#include "MappedFile.hpp"
#include "Logger.hpp"

auto FILENAME = "bigfile.x64.dat";

int main(const int argc, char* argv[]) {
    const char* filename = argc > 1 ? argv[1] : FILENAME;

    MappedFile file;

    try {
        file.Open(filename);
    } catch (const std::exception& e) {
        Log(e.what(), LogLevel::Error);
        return -2;
    }

    const LocaleHandler locales(file.Data(), file.Size());
    locales.ExportBlock(Locale::EN, "output");

    return 0;
}