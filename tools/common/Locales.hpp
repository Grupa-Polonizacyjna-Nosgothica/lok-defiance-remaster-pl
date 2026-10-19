#ifndef LOCALES_HPP_
#define LOCALES_HPP_

#include <cctype>
#include <cstdint>
#include <map>
#include <string>

enum class Locale { EN, FR, DE, IT, ES, JP, PT, CN, RU };

inline const std::map<Locale, std::string> LOCALE_NAMES = {
    {Locale::EN, "EN"}, {Locale::FR, "FR"}, {Locale::DE, "DE"},
    {Locale::IT, "IT"}, {Locale::ES, "ES"}, {Locale::JP, "JP"},
    {Locale::PT, "PT"}, {Locale::CN, "CN"}, {Locale::RU, "RU"}
};

// Language names as used by the game to build the "PC<Language>\locals.bin" path
inline const std::map<Locale, std::string> LOCALE_LANGUAGES = {
    {Locale::EN, "English"}, {Locale::FR, "French"}, {Locale::DE, "German"},
    {Locale::IT, "Italian"}, {Locale::ES, "Spanish"}, {Locale::JP, "Japanese"},
    {Locale::PT, "Portuguese"}, {Locale::CN, "Chinese"}, {Locale::RU, "Russian"}
};

// First u32 of a locals.bin block
inline const std::map<Locale, uint32_t> LOCALE_IDS = {
    {Locale::EN, 0}, {Locale::FR, 1}, {Locale::DE, 2},
    {Locale::IT, 3}, {Locale::ES, 4}, {Locale::JP, 5},
    {Locale::PT, 6}, {Locale::CN, 9}, {Locale::RU, 10}
};

inline std::string LocalesPath(const Locale locale)
{
    return "PC" + LOCALE_LANGUAGES.at(locale) + "\\locals.bin";
}

// Some locals.bin strings are internal names, not text: music and ambience tracks (Sarafan_battle_01,
// Wind, ...) and keys such as no_savefile. Translating them makes the game fail, e.g. while loading
// music, so they have to stay as in the original. `english` is the English string of that ID.
inline bool IsProtectedString(const size_t id, const std::string& english)
{
    // Music lists: 3114-3283 and 4103-4106 (the latter contains single words like "Dark")
    if ((id >= 3114 && id <= 3283) || (id >= 4103 && id <= 4106)) return true;
    // Anything that looks like an identifier: only letters, digits and underscores, at least one underscore
    if (english.find('_') == std::string::npos) return false;
    for (const char c : english)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    return true;
}

#endif //LOCALES_HPP_
