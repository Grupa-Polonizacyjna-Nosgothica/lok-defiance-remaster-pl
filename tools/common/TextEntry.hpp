#ifndef TEXT_ENTRY_HPP_
#define TEXT_ENTRY_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// "TextEntry" records hold subtitles as plain text, one line per language, lines separated by CR LF:
//   Version 1 / TextEntry / Line0 / lang_english <date> "\(1.8)text \(3.4)text ..." / lang_russian ...
// The body keeps its `\(seconds)` timing markers and literal `\n` sequences untouched.
namespace TextEntry
{
    [[nodiscard]] bool IsTextEntry(const uint8_t* data, size_t size);
    // Body between the quotes of the `lang_<language>` line
    [[nodiscard]] std::optional<std::string> GetBody(const std::vector<uint8_t>& record, const std::string& language);
    // Returns the record with that language's body replaced, or nullopt if the line is missing
    [[nodiscard]] std::optional<std::vector<uint8_t>> SetBody(const std::vector<uint8_t>& record,
                                                              const std::string& language,
                                                              const std::string& body);
}

#endif //TEXT_ENTRY_HPP_
