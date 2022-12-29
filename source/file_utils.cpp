
#include "tdb/file_utils.h"

#include <map>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
    
r_file::r_file() :
    _f(nullptr) 
{
}

r_file::r_file(r_file&& obj) noexcept :
    _f(std::move(obj._f))
{
    obj._f = nullptr;
}

r_file::~r_file() noexcept
{
    if(_f)
        fclose(_f);
}

r_file& r_file::operator = (r_file&& obj) noexcept
{
    if(_f)
        fclose(_f);

    _f = std::move(obj._f);
    obj._f = nullptr;
    return *this;
}

r_file r_file::open(const std::string& path, const std::string& mode)
{
    r_file obj;
    obj._f = fopen(path.c_str(), mode.c_str());
    if(!obj._f)
        throw std::runtime_error("Unable to open: " + path);
    return obj;
}

void r_file::close()
{
    fclose(_f);
    _f = nullptr;
}

r_memory_map::r_memory_map() :
    _mem(nullptr),
    _length(0),
    _mapOffset(0)
{
}

r_memory_map::r_memory_map(r_memory_map&& obj) noexcept :
    _mem(std::move(obj._mem)),
    _length(std::move(obj._length)),
    _mapOffset(std::move(obj._mapOffset))
{
    obj._mem = NULL;
    obj._length = 0;
    obj._mapOffset = 0;
}

r_memory_map::r_memory_map(int fd, uint64_t offset, uint64_t len, uint32_t prot, uint32_t flags, uint64_t mapOffset) :
    _mem(NULL),
    _length(len),
    _mapOffset(mapOffset)
{
    if(fd <= 0)
        throw std::runtime_error("Attempting to memory map a bad file descriptor.");

    if(len == 0)
        throw std::runtime_error("Attempting to memory map 0 bytes is invalid.");

    if(!(flags & MM_TYPE_FILE) && !(flags & MM_TYPE_ANON))
        throw std::runtime_error("A mapping must be either a file mapping, or an anonymous mapping (neither was specified).");

    if(flags & MM_FIXED)
        throw std::runtime_error("r_memory_map does not support fixed mappings.");

    _mem = mmap64(NULL, _length, _get_posix_prot_flags(prot), _get_posix_access_flags(flags), fd, offset);
}

r_memory_map::~r_memory_map() noexcept
{
    _close();
}

r_memory_map& r_memory_map::operator = (r_memory_map&& obj) noexcept
{
    _close();

    _mem = std::move(obj._mem);
    obj._mem = NULL;

    _length = std::move(obj._length);
    obj._length = 0;

    _mapOffset = std::move(obj._mapOffset);
    obj._mapOffset = 0;

    return *this;
}

void r_memory_map::advise(void* addr, size_t length, int advice) const
{
    int posixAdvice = _get_posix_advice(advice);

    int err = madvise(addr, length, posixAdvice);

    if(err != 0)
        throw std::runtime_error("Unable to apply memory mapping advice.");
}

void r_memory_map::_close() noexcept
{
    if(_mem)
    {
        munmap(_mem, _length);
        _mem = nullptr;
    }
}

int r_memory_map::_get_posix_prot_flags(int prot) const
{
    int osProtFlags = 0;

    if(prot & MM_PROT_READ)
        osProtFlags |= PROT_READ;
    if(prot & MM_PROT_WRITE)
        osProtFlags |= PROT_WRITE;
    if(prot & MM_PROT_EXEC)
        osProtFlags |= PROT_EXEC;

    return osProtFlags;
}

int r_memory_map::_get_posix_access_flags(int flags) const
{
    int osFlags = 0;

    if(flags & MM_TYPE_FILE)
        osFlags |= MAP_FILE;
    if(flags & MM_TYPE_ANON)
    {
        osFlags |= MAP_ANONYMOUS;
    }
    if(flags & MM_SHARED)
        osFlags |= MAP_SHARED;
    if(flags & MM_PRIVATE)
        osFlags |= MAP_PRIVATE;
    if(flags & MM_FIXED)
        osFlags |= MAP_FIXED;

    return osFlags;
}

int r_memory_map::_get_posix_advice(int advice) const
{
    int posixAdvice = 0;

    if(advice & MM_ADVICE_RANDOM)
        posixAdvice |= MADV_RANDOM;
    if(advice & MM_ADVICE_SEQUENTIAL)
        posixAdvice |= MADV_SEQUENTIAL;
    if(advice & MM_ADVICE_WILLNEED)
        posixAdvice |= MADV_WILLNEED;
    if(advice & MM_ADVICE_DONTNEED)
        posixAdvice |= MADV_DONTNEED;

    return posixAdvice;
}
