#ifndef LOCALE_HANDLER_HPP_
#define LOCALE_HANDLER_HPP_

#include <cstdint>
#include <map>
#include <string>

#include "BigfileIndex.hpp"
#include "Locales.hpp"
#include "LocalesBin.hpp"

class LocaleHandler
{
public:
    LocaleHandler(const uint8_t* data, size_t fileSize);
    // Writes strings as NNNN.txt (position in the block) into output_dir, creating it if needed
    void ExportBlock(Locale locale, const std::string& output_dir) const;
    void ExportBlocks(const std::string& output_dir) const;
protected:
    BigfileIndex m_index;
    const uint8_t* m_data;
    std::map<Locale, LocalesBin> m_blocks;
};

#endif //LOCALE_HANDLER_HPP_
