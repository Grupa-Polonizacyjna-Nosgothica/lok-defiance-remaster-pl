#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

#include "BinaryUtils.hpp"
#include "TranslationSource.hpp"

namespace fs = std::filesystem;

namespace
{
    constexpr char MAGIC[8] = {'L', 'O', 'K', 'P', 'L', 'P', 'K', '1'};

    uint32_t Crc32(const std::string& data)
    {
        return crc32_ieee(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }

    std::string ReadWholeFile(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot read " + path.string());
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    class DirectorySource : public TranslationSource
    {
    public:
        explicit DirectorySource(fs::path root) : m_root(std::move(root)) {}

        bool Exists(const std::string& path) const override { return fs::is_regular_file(m_root / path); }
        std::string Read(const std::string& path) const override { return ReadWholeFile(m_root / path); }

        std::vector<std::string> List(const std::string& dir) const override
        {
            std::vector<std::string> names;
            if (!fs::is_directory(m_root / dir)) return names;
            for (const auto& e : fs::directory_iterator(m_root / dir))
                if (e.is_regular_file()) names.push_back(e.path().filename().string());
            return names;
        }

        std::string Describe() const override { return "directory " + m_root.string(); }

    private:
        fs::path m_root;
    };

    class PackageSource : public TranslationSource
    {
    public:
        explicit PackageSource(const fs::path& path) : m_path(path), m_data(ReadWholeFile(path))
        {
            size_t pos = 0;
            auto need = [&](const size_t n) {
                if (pos + n > m_data.size()) throw std::runtime_error(m_path.string() + " is truncated.");
            };
            auto get = [&](const size_t n) {
                uint64_t v = 0;
                need(n);
                for (size_t i = 0; i < n; i++) v |= static_cast<uint64_t>(static_cast<uint8_t>(m_data[pos + i])) << (8 * i);
                pos += n;
                return v;
            };
            need(sizeof(MAGIC));
            if (m_data.compare(0, sizeof(MAGIC), MAGIC, sizeof(MAGIC)) != 0)
                throw std::runtime_error(m_path.string() + " is not a translation package.");
            pos = sizeof(MAGIC);
            const uint64_t count = get(4);
            for (uint64_t i = 0; i < count; i++) {
                const size_t nameLen = get(2);
                need(nameLen);
                std::string name = m_data.substr(pos, nameLen);
                pos += nameLen;
                const uint64_t size = get(8);
                const uint32_t crc = static_cast<uint32_t>(get(4));
                need(size);
                m_entries[name] = {pos, size, crc};
                pos += size;
            }
        }

        bool Exists(const std::string& path) const override { return m_entries.count(path) != 0; }

        std::string Read(const std::string& path) const override
        {
            const auto it = m_entries.find(path);
            if (it == m_entries.end()) throw std::runtime_error("The package has no " + path);
            std::string content = m_data.substr(it->second.offset, it->second.size);
            if (Crc32(content) != it->second.crc)
                throw std::runtime_error("The package is damaged (checksum of " + path + ").");
            return content;
        }

        std::vector<std::string> List(const std::string& dir) const override
        {
            std::vector<std::string> names;
            const std::string prefix = dir + "/";
            for (const auto& [name, entry] : m_entries)
                if (name.compare(0, prefix.size(), prefix) == 0 && name.find('/', prefix.size()) == std::string::npos)
                    names.push_back(name.substr(prefix.size()));
            return names;
        }

        std::string Describe() const override { return "package " + m_path.string(); }

    private:
        struct Entry { size_t offset; uint64_t size; uint32_t crc; };
        fs::path m_path;
        std::string m_data;
        std::map<std::string, Entry> m_entries;
    };
}

std::unique_ptr<TranslationSource> OpenTranslationSource(const fs::path& path)
{
    if (fs::is_directory(path)) return std::make_unique<DirectorySource>(path);
    if (fs::is_regular_file(path)) return std::make_unique<PackageSource>(path);
    throw std::runtime_error("Translation source " + path.string() + " not found.");
}

std::vector<std::string> ListTranslationFiles(const TranslationSource& source)
{
    std::vector<std::string> files;
    for (const std::string dir : {"strings/PL", "cinematic/PL"})
        for (const auto& name : source.List(dir)) files.push_back(dir + "/" + name);
    // Only the deltas of the fonts, never full game files
    for (const auto& name : source.List("fonts"))
        if (name.size() > 8 && (name.compare(name.size() - 8, 8, ".add.txt") == 0 || name.compare(name.size() - 8, 8, ".overlay") == 0))
            files.push_back("fonts/" + name);
    return files;
}

size_t WritePackage(const TranslationSource& source, const fs::path& out)
{
    std::vector<std::string> files = ListTranslationFiles(source);
    std::sort(files.begin(), files.end());
    std::ofstream f(out, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("Cannot create " + out.string());
    auto put = [&f](const uint64_t v, const int n) {
        for (int i = 0; i < n; i++) f.put(static_cast<char>(v >> (8 * i)));
    };
    f.write(MAGIC, sizeof(MAGIC));
    put(files.size(), 4);
    for (const auto& name : files) {
        const std::string content = source.Read(name);
        put(name.size(), 2);
        f.write(name.data(), static_cast<std::streamsize>(name.size()));
        put(content.size(), 8);
        put(Crc32(content), 4);
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    f.flush();
    if (!f) throw std::runtime_error("Failed to write " + out.string());
    return files.size();
}
