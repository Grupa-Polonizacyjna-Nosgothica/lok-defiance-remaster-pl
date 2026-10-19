#include <stdexcept>
#include "MappedFile.hpp"

void MappedFile::Open(const char* filename) {
#ifdef _WIN32
    hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        throw std::runtime_error("CreateFile failed");

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    size = static_cast<size_t>(fileSize.QuadPart);

    hMapping = CreateFileMappingA(hFile, nullptr, PAGE_READONLY,
                                  0, 0, nullptr);
    if (!hMapping)
        throw std::runtime_error("CreateFileMapping failed");

    data = static_cast<uint8_t*>(
        MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0));

    if (!data)
        throw std::runtime_error("MapViewOfFile failed");
#else
    fd = ::open(filename, O_RDONLY);
    if (fd < 0)
        throw std::runtime_error("open failed");

    struct stat st;
    fstat(fd, &st);
    size = st.st_size;

    data = static_cast<uint8_t*>(
        mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));

    if (data == MAP_FAILED)
        throw std::runtime_error("mmap failed");
#endif
}

void MappedFile::Close() const {
#ifdef _WIN32
    if (data) UnmapViewOfFile(data);
    if (hMapping) CloseHandle(hMapping);
    if (hFile && hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
#else
    if (data && data != MAP_FAILED) munmap(data, size);
    if (fd >= 0) ::close(fd);
#endif
}


