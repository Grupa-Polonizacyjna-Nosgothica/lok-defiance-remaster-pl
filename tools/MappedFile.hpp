#ifndef MAPPED_FILE_HPP_
#define MAPPED_FILE_HPP_

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

class MappedFile {
public:
    ~MappedFile() { Close(); }
    void Open(const char* filename);
    void Close() const;
    [[nodiscard]] uint8_t* Data() const { return data; }
    [[nodiscard]] size_t Size() const { return size; }

protected:
    uint8_t* data = nullptr;
    size_t size = 0;
#ifdef _WIN32
    HANDLE hFile = nullptr;
    HANDLE hMapping = nullptr;
#else
    int fd = -1;
#endif
};

#endif //MAPPED_FILE_HPP_