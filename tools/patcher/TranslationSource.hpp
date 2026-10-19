#ifndef TRANSLATION_SOURCE_HPP_
#define TRANSLATION_SOURCE_HPP_

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Where the patcher reads the translation from: a directory laid out like the repository
// (strings/PL, cinematic/PL, fonts) or a single .pak file made by `PolishPatcher pack`.
// Paths are relative with '/' separators, e.g. "strings/PL/0001.txt".
class TranslationSource
{
public:
    virtual ~TranslationSource() = default;
    [[nodiscard]] virtual bool Exists(const std::string& path) const = 0;
    // Throws std::runtime_error if the file is missing or damaged
    [[nodiscard]] virtual std::string Read(const std::string& path) const = 0;
    // Names (not paths) of the files directly inside `dir`
    [[nodiscard]] virtual std::vector<std::string> List(const std::string& dir) const = 0;
    [[nodiscard]] virtual std::string Describe() const = 0;
};

// A directory or, if `path` is a regular file, a package
std::unique_ptr<TranslationSource> OpenTranslationSource(const std::filesystem::path& path);

// Files that make up the translation, relative to a translation directory
std::vector<std::string> ListTranslationFiles(const TranslationSource& source);

// Writes the package: magic "LOKPLPK1", u32 count, then per file u16 name length, name,
// u64 size, u32 CRC-32 and the bytes. Returns the number of files.
size_t WritePackage(const TranslationSource& source, const std::filesystem::path& out);

#endif //TRANSLATION_SOURCE_HPP_
