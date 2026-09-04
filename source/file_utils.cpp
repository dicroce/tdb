#include "tdb/file_utils.h"

#ifdef _WIN32
#include <io.h>
#include <share.h>
#else
#include <cerrno>
#endif

using namespace std;

r_file::r_file() : _f(nullptr) {}
r_file::r_file(r_file&& obj) noexcept : _f(obj._f) { obj._f = nullptr; }
r_file::~r_file() noexcept { if (_f) fclose(_f); }

r_file& r_file::operator=(r_file&& obj) noexcept
{
    if (this != &obj)
    {
        if (_f) fclose(_f);
        _f = obj._f;
        obj._f = nullptr;
    }
    return *this;
}

r_file r_file::open(const string& path, const string& mode)
{
    r_file obj;
#ifdef _WIN32
    obj._f = _fsopen(path.c_str(), mode.c_str(), _SH_DENYNO);
#else
    obj._f = fopen(path.c_str(), mode.c_str());
#endif
    if (!obj._f) throw runtime_error("Unable to open: " + path);
    return obj;
}

void r_file::close()
{
    if (_f) { fclose(_f); _f = nullptr; }
}

r_memory_map::r_memory_map() : _mem(nullptr), _length(0), _mapOffset(0)
#ifdef _WIN32
    , _fileHandle(INVALID_HANDLE_VALUE), _mapHandle(nullptr)
#endif
{}

r_memory_map::r_memory_map(r_memory_map&& obj) noexcept :
    _mem(obj._mem), _length(obj._length), _mapOffset(obj._mapOffset)
#ifdef _WIN32
    , _fileHandle(obj._fileHandle), _mapHandle(obj._mapHandle)
#endif
{
    obj._mem = nullptr; obj._length = 0; obj._mapOffset = 0;
#ifdef _WIN32
    obj._fileHandle = INVALID_HANDLE_VALUE; obj._mapHandle = nullptr;
#endif
}

r_memory_map::r_memory_map(int fd, uint64_t offset, uint64_t len,
                           uint32_t prot, uint32_t flags, uint64_t mapOffset) :
    _mem(nullptr), _length(len), _mapOffset(mapOffset)
#ifdef _WIN32
    , _fileHandle(INVALID_HANDLE_VALUE), _mapHandle(nullptr)
#endif
{
    if (fd < 0) throw runtime_error("Attempting to memory map a bad file descriptor.");
    if (len == 0) throw runtime_error("Attempting to memory map 0 bytes is invalid.");
    if (!(flags & MM_TYPE_FILE) && !(flags & MM_TYPE_ANON))
        throw runtime_error("A mapping must be file-backed or anonymous.");
    if (flags & MM_FIXED) throw runtime_error("Fixed mappings are not supported.");

#ifdef _WIN32
    const DWORD pageProtection = (prot & MM_PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
    const DWORD viewAccess = (prot & MM_PROT_WRITE) ? FILE_MAP_WRITE : FILE_MAP_READ;
    HANDLE source = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (source == INVALID_HANDLE_VALUE)
        throw runtime_error("Unable to obtain Windows file handle.");
    if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(),
                         &_fileHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
        throw runtime_error("Unable to duplicate Windows file handle.");
    _mapHandle = CreateFileMappingA(_fileHandle, nullptr, pageProtection, 0, 0, nullptr);
    if (!_mapHandle) { _close(); throw runtime_error("Unable to create Windows file mapping."); }
    _mem = MapViewOfFile(_mapHandle, viewAccess, static_cast<DWORD>(offset >> 32),
                         static_cast<DWORD>(offset & 0xffffffffULL), len);
    if (!_mem) { _close(); throw runtime_error("Unable to map Windows file view."); }
#else
    _mem = mmap(nullptr, _length, _get_posix_prot_flags(prot),
                _get_posix_access_flags(flags), fd, static_cast<off_t>(offset));
    if (_mem == MAP_FAILED)
    {
        _mem = nullptr;
        throw runtime_error("Unable to complete file mapping.");
    }
#endif
}

r_memory_map::~r_memory_map() noexcept { _close(); }

r_memory_map& r_memory_map::operator=(r_memory_map&& obj) noexcept
{
    if (this != &obj)
    {
        _close();
        _mem = obj._mem; _length = obj._length; _mapOffset = obj._mapOffset;
#ifdef _WIN32
        _fileHandle = obj._fileHandle; _mapHandle = obj._mapHandle;
        obj._fileHandle = INVALID_HANDLE_VALUE; obj._mapHandle = nullptr;
#endif
        obj._mem = nullptr; obj._length = 0; obj._mapOffset = 0;
    }
    return *this;
}

void r_memory_map::advise(void* addr, size_t length, int advice) const
{
#ifndef _WIN32
    if (madvise(addr, length, _get_posix_advice(advice)) != 0)
        throw runtime_error("Unable to apply memory mapping advice.");
#else
    (void)addr; (void)length; (void)advice;
#endif
}

void r_memory_map::flush(bool synchronous) const
{
    if (!_mem) return;
#ifdef _WIN32
    if (!FlushViewOfFile(_mem, _length)) throw runtime_error("Unable to flush mapped view.");
    if (synchronous && !FlushFileBuffers(_fileHandle)) throw runtime_error("Unable to flush mapped file.");
#else
    if (msync(_mem, _length, synchronous ? MS_SYNC : MS_ASYNC) != 0)
        throw runtime_error("Unable to flush mapped view.");
#endif
}

uint64_t r_memory_map::allocation_granularity()
{
#ifdef _WIN32
    SYSTEM_INFO info{}; GetSystemInfo(&info); return info.dwAllocationGranularity;
#else
    long value = sysconf(_SC_PAGE_SIZE);
    if (value <= 0) throw runtime_error("Unable to determine OS page size.");
    return static_cast<uint64_t>(value);
#endif
}

void r_memory_map::_close() noexcept
{
#ifdef _WIN32
    if (_mem) { UnmapViewOfFile(_mem); _mem = nullptr; }
    if (_mapHandle) { CloseHandle(_mapHandle); _mapHandle = nullptr; }
    if (_fileHandle != INVALID_HANDLE_VALUE) { CloseHandle(_fileHandle); _fileHandle = INVALID_HANDLE_VALUE; }
#else
    if (_mem) { munmap(_mem, _length); _mem = nullptr; }
#endif
}

int r_memory_map::_get_posix_prot_flags(int prot) const
{
#ifdef _WIN32
    (void)prot; return 0;
#else
    int result = 0;
    if (prot & MM_PROT_READ) result |= PROT_READ;
    if (prot & MM_PROT_WRITE) result |= PROT_WRITE;
    if (prot & MM_PROT_EXEC) result |= PROT_EXEC;
    return result;
#endif
}

int r_memory_map::_get_posix_access_flags(int flags) const
{
#ifdef _WIN32
    (void)flags; return 0;
#else
    int result = 0;
    if (flags & MM_TYPE_ANON) result |= MAP_ANONYMOUS;
    if (flags & MM_SHARED) result |= MAP_SHARED;
    if (flags & MM_PRIVATE) result |= MAP_PRIVATE;
    return result;
#endif
}

int r_memory_map::_get_posix_advice(int advice) const
{
#ifdef _WIN32
    (void)advice; return 0;
#else
    if (advice & MM_ADVICE_RANDOM) return MADV_RANDOM;
    if (advice & MM_ADVICE_SEQUENTIAL) return MADV_SEQUENTIAL;
    if (advice & MM_ADVICE_WILLNEED) return MADV_WILLNEED;
    if (advice & MM_ADVICE_DONTNEED) return MADV_DONTNEED;
    return MADV_NORMAL;
#endif
}

int file_number(FILE* f)
{
#ifdef _WIN32
    return _fileno(f);
#else
    return fileno(f);
#endif
}

void resize_file(FILE* f, uint64_t size)
{
    if (fflush(f) != 0) throw runtime_error("Unable to flush file stream.");
#ifdef _WIN32
    if (_chsize_s(file_number(f), size) != 0) throw runtime_error("Unable to resize file.");
#else
    if (ftruncate(file_number(f), static_cast<off_t>(size)) != 0) throw runtime_error("Unable to resize file.");
#endif
}

void sync_file(FILE* f)
{
    if (fflush(f) != 0) throw runtime_error("Unable to flush file stream.");
#ifdef _WIN32
    HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(file_number(f)));
    if (handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(handle)) throw runtime_error("Unable to sync file.");
#else
    if (fsync(file_number(f)) != 0) throw runtime_error("Unable to sync file.");
#endif
}

void remove_file(const string& path)
{
#ifdef _WIN32
    if (!DeleteFileA(path.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
        throw runtime_error("Unable to remove file: " + path);
#else
    if (unlink(path.c_str()) != 0 && errno != ENOENT)
        throw runtime_error("Unable to remove file: " + path);
#endif
}
