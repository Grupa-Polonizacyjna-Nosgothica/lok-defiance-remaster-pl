#include <algorithm>
#include <cstring>
#include <string_view>

#include "TextEntry.hpp"

namespace
{
    struct BodyRange { size_t begin; size_t end; };

    std::optional<BodyRange> FindBody(const std::vector<uint8_t>& record, const std::string& language)
    {
        const std::string key = "lang_" + language + " ";
        const std::string_view text(reinterpret_cast<const char*>(record.data()), record.size());
        size_t pos = 0;
        while (pos < text.size()) {
            const size_t lineEnd = std::min(text.find_first_of("\r\n", pos), text.size());
            if (text.compare(pos, key.size(), key) == 0) {
                const size_t open = text.find('"', pos);
                const size_t close = text.rfind('"', lineEnd == 0 ? 0 : lineEnd - 1);
                if (open == std::string_view::npos || close == std::string_view::npos || close <= open || close >= lineEnd)
                    return std::nullopt;
                return BodyRange{open + 1, close};
            }
            pos = lineEnd + 1;
        }
        return std::nullopt;
    }
}

bool TextEntry::IsTextEntry(const uint8_t* data, const size_t size)
{
    static constexpr char kHeader[] = "Version 1\r\nTextEntry\r\n";
    return size > sizeof(kHeader) && std::memcmp(data, kHeader, sizeof(kHeader) - 1) == 0;
}

std::optional<std::string> TextEntry::GetBody(const std::vector<uint8_t>& record, const std::string& language)
{
    const auto range = FindBody(record, language);
    if (!range) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(record.data()) + range->begin, range->end - range->begin);
}

std::optional<std::vector<uint8_t>> TextEntry::SetBody(const std::vector<uint8_t>& record,
                                                       const std::string& language,
                                                       const std::string& body)
{
    const auto range = FindBody(record, language);
    if (!range) return std::nullopt;
    std::vector<uint8_t> out(record.begin(), record.begin() + range->begin);
    out.insert(out.end(), body.begin(), body.end());
    out.insert(out.end(), record.begin() + range->end, record.end());
    return out;
}
