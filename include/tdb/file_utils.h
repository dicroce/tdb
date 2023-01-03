
#ifndef __file_utils_h
#define __file_utils_h

#include <map>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>

template<typename T, typename F>
void block_write_file(const T* p, size_t size, const F& f, size_t blockSize = 4096)
{
    size_t blocks = (size >= blockSize)?size / blockSize:0;
    size_t remainder = (size >= blockSize)?size % blockSize:size;
    const uint8_t* writeHead = (const uint8_t*)p;

    while(blocks > 0)
    {
        auto blocksWritten = fwrite((void*)writeHead, blockSize, blocks, f);
        if(blocksWritten == 0)
            throw std::runtime_error("Unable to write to file.");
        blocks -= blocksWritten;
        writeHead += (blocksWritten * blockSize);
    }

    while(remainder > 0)
    {
        auto bytesWritten = fwrite((void*)writeHead, 1, remainder, f);
        if(bytesWritten == 0)
            throw std::runtime_error("Could not write to file.");
        remainder -= bytesWritten;
        writeHead += bytesWritten;
    }
}

// GLIBC fread() loops until it has read the requested number of bytes OR the lower level __read
// returns 0. If the lower level __read returns 0 the EOF flag is set on the stream.
template<typename T, typename F>
void block_read_file(T* p, size_t size, const F& f, size_t blockSize = 4096)
{
    size_t blocks = (size >= blockSize)?size / blockSize:0;
    size_t remainder = (size >= blockSize)?size % blockSize:size;
    uint8_t* readHead = (uint8_t*)p;

    if(blocks > 0)
    {
        auto blocksRead = fread((void*)readHead, blockSize, blocks, f);
        if(blocksRead != blocks)
            throw std::runtime_error("Short fread(). Implies feof().");
        readHead += (blocksRead * blockSize);
    }

    if(remainder > 0)
    {
        auto bytesRead = fread((void*)readHead, 1, remainder, f);
        if(bytesRead != remainder)
            throw std::runtime_error("Short fread(). Indicates feof()");
    }
}

class r_file final
{
public:
    r_file();
    r_file(const r_file&) = delete;
    r_file(r_file&& obj) noexcept;
    ~r_file() noexcept;
    r_file& operator = (r_file&) = delete;
    r_file& operator = (r_file&& obj) noexcept;

    inline operator FILE*() const { return _f; }

    static r_file open(const std::string& path, const std::string& mode);
    void close();

private:
    FILE* _f;
};

class r_memory_map
{
public:
    enum flags
    {
        MM_TYPE_FILE = 0x01,
        MM_TYPE_ANON = 0x02,
        MM_SHARED = 0x04,
        MM_PRIVATE = 0x08,
        MM_FIXED = 0x10
    };

    enum protection
    {
        MM_PROT_NONE = 0x00,
        MM_PROT_READ = 0x01,
        MM_PROT_WRITE = 0x02,
        MM_PROT_EXEC = 0x04
    };

    enum advice
    {
        MM_ADVICE_NORMAL = 0x00,
        MM_ADVICE_RANDOM = 0x01,
        MM_ADVICE_SEQUENTIAL = 0x02,
        MM_ADVICE_WILLNEED = 0x04,
        MM_ADVICE_DONTNEED = 0x08
    };

    r_memory_map();
    r_memory_map(const r_memory_map& ob ) = delete;
    r_memory_map(r_memory_map&& obj) noexcept;
    r_memory_map(int fd, uint64_t offset, uint64_t len, uint32_t prot, uint32_t flags, uint64_t mapOffset=0);
    virtual ~r_memory_map() noexcept;

    r_memory_map& operator = (const r_memory_map&) = delete;
    r_memory_map& operator = (r_memory_map&& obj) noexcept;

    inline std::pair<uint8_t*, uint8_t*> map() const
    {
        if(_mem == nullptr)
            throw std::runtime_error("Unable to map default constructed memory map.");
        return std::make_pair((uint8_t*)_mem, ((uint8_t*)_mem) + _mapOffset);
    }

    inline uint64_t size() const
    {
        return _length;
    }

    void advise(void* addr, size_t length, int advice) const;

private:
    void _close() noexcept;
    int _get_posix_prot_flags(int prot) const;
    int _get_posix_access_flags(int flags) const;
    int _get_posix_advice(int advice) const;
    void* _mem;
    uint64_t _length;
    uint64_t _mapOffset;
};

#endif
