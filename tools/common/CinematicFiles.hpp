#ifndef CINEMATIC_FILES_HPP_
#define CINEMATIC_FILES_HPP_

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

// File names of the extracted cinematic texts (cinematic/<LANG>/):
//   textentry_<hash>.txt      body of the English line of a TextEntry record
//   crblock_<hash>_<k>.txt    English text of the k-th CR block (0-based) of a record
// <hash> is the record's 8 digit hex path hash from the bigfile index.
namespace CinematicFiles
{
    inline std::string TextEntryName(const uint32_t hash)
    {
        char name[40];
        std::snprintf(name, sizeof(name), "textentry_%08x.txt", hash);
        return name;
    }

    inline std::string CrBlockName(const uint32_t hash, const size_t k)
    {
        char name[40];
        std::snprintf(name, sizeof(name), "crblock_%08x_%02zu.txt", hash, k);
        return name;
    }

    inline std::optional<uint32_t> ParseTextEntryName(const std::string& name)
    {
        unsigned hash = 0;
        char tail[8] = {};
        if (name.size() == 22 && std::sscanf(name.c_str(), "textentry_%8x%7s", &hash, tail) == 2 && std::string(tail) == ".txt")
            return hash;
        return std::nullopt;
    }

    struct CrBlockRef { uint32_t hash; size_t k; };

    inline std::optional<CrBlockRef> ParseCrBlockName(const std::string& name)
    {
        unsigned hash = 0;
        size_t k = 0;
        int consumed = 0;
        if (std::sscanf(name.c_str(), "crblock_%8x_%zu.txt%n", &hash, &k, &consumed) == 2
            && static_cast<size_t>(consumed) == name.size())
            return CrBlockRef{hash, k};
        return std::nullopt;
    }
}

#endif //CINEMATIC_FILES_HPP_
