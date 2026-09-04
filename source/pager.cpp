#include "tdb/pager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <io.h>
#endif

using namespace std;

namespace
{
constexpr uint64_t database_magic = 0x31424454534c5053ULL;
constexpr uint32_t database_version = 3;
constexpr uint64_t wal_magic = 0x314c415742445454ULL;
constexpr uint64_t wal_commit_magic = 0x54494d4d4f434254ULL;
constexpr uint64_t free_page_magic = 0x3145455246424454ULL;

struct alignas(8) database_header
{
    uint64_t magic;
    uint32_t version;
    uint32_t page_size;
    uint64_t root_page;
    uint64_t page_count;
    uint64_t free_page;
};

struct wal_header
{
    uint64_t magic;
    uint32_t version;
    uint32_t page_size;
    uint64_t record_count;
};

struct wal_footer
{
    uint64_t commit_magic;
    uint64_t checksum;
};

uint64_t checksum_bytes(uint64_t hash, const void* data, size_t size)
{
    constexpr uint64_t fnv_prime = 1099511628211ULL;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= fnv_prime;
    }
    return hash;
}

void seek_file(FILE* file, uint64_t offset)
{
#ifdef _WIN32
    if (_fseeki64(file, static_cast<int64_t>(offset), SEEK_SET) != 0)
#else
    if (fseeko(file, static_cast<off_t>(offset), SEEK_SET) != 0)
#endif
        throw runtime_error("Unable to seek database file.");
}

database_header decode_header(const pager::page& page)
{
    database_header result{};
    memcpy(&result, page.data(), sizeof(result));
    if (result.magic != database_magic || result.version != database_version ||
        result.page_size != pager::page_size || result.page_count == 0 ||
        result.free_page >= result.page_count)
        throw runtime_error("Unsupported or corrupt tdb database header.");
    return result;
}

void encode_header(pager::page& page, uint64_t root_page, uint64_t page_count,
                   uint64_t free_page)
{
    database_header header{database_magic, database_version,
                           static_cast<uint32_t>(pager::page_size),
                           root_page, page_count, free_page};
    page.fill(0);
    memcpy(page.data(), &header, sizeof(header));
}
}

pager::pager(const string& file_name) :
    _file_name(file_name), _wal_name(file_name + ".wal"),
    _file(r_file::open(file_name, "r+b"))
{
    recover();
    (void)decode_header(read(0));
}

void pager::create(const string& file_name)
{
    auto file = r_file::open(file_name, "w+b");
    page header_page{};
    encode_header(header_page, 0, 1, 0);
    block_write_file(header_page.data(), header_page.size(), file);
    sync_file(file);
    remove_file(file_name + ".wal");
}

pager::page pager::read(uint64_t page_number) const
{
    lock_guard lock(_io_mutex);
    page result{};
    seek_file(_file, page_number * page_size);
    block_read_file(result.data(), result.size(), _file);
    return result;
}

uint64_t pager::root_page() const
{
    return decode_header(read(0)).root_page;
}

pager::transaction pager::begin_transaction()
{
    return transaction(*this);
}

void pager::write_page(uint64_t page_number, const page& contents)
{
    seek_file(_file, page_number * page_size);
    block_write_file(contents.data(), contents.size(), _file);
}

void pager::commit(const map<uint64_t, page>& pages)
{
    if (pages.empty()) return;

    {
        auto wal = r_file::open(_wal_name, "w+b");
        wal_header header{wal_magic, database_version,
                          static_cast<uint32_t>(page_size), pages.size()};
        uint64_t checksum = 14695981039346656037ULL;
        checksum = checksum_bytes(checksum, &header, sizeof(header));
        block_write_file(&header, sizeof(header), wal);

        for (const auto& [page_number, contents] : pages)
        {
            checksum = checksum_bytes(checksum, &page_number, sizeof(page_number));
            checksum = checksum_bytes(checksum, contents.data(), contents.size());
            block_write_file(&page_number, sizeof(page_number), wal);
            block_write_file(contents.data(), contents.size(), wal);
        }

        wal_footer footer{wal_commit_magic, checksum};
        block_write_file(&footer, sizeof(footer), wal);
        sync_file(wal);
    }

    {
        lock_guard lock(_io_mutex);
        for (const auto& [page_number, contents] : pages)
            write_page(page_number, contents);
        sync_file(_file);
    }
    remove_file(_wal_name);
}

void pager::recover()
{
    if (!filesystem::exists(_wal_name)) return;

    bool committed = false;
    map<uint64_t, page> pages;
    try
    {
        auto wal = r_file::open(_wal_name, "rb");
        wal_header header{};
        block_read_file(&header, sizeof(header), wal);
        if (header.magic != wal_magic || header.version != database_version ||
            header.page_size != page_size)
            throw runtime_error("Invalid WAL header.");

        uint64_t checksum = 14695981039346656037ULL;
        checksum = checksum_bytes(checksum, &header, sizeof(header));
        for (uint64_t i = 0; i < header.record_count; ++i)
        {
            uint64_t page_number = 0;
            page contents{};
            block_read_file(&page_number, sizeof(page_number), wal);
            block_read_file(contents.data(), contents.size(), wal);
            checksum = checksum_bytes(checksum, &page_number, sizeof(page_number));
            checksum = checksum_bytes(checksum, contents.data(), contents.size());
            pages[page_number] = contents;
        }
        wal_footer footer{};
        block_read_file(&footer, sizeof(footer), wal);
        committed = footer.commit_magic == wal_commit_magic && footer.checksum == checksum;
    }
    catch (const exception&)
    {
        committed = false;
    }

    if (committed)
    {
        lock_guard lock(_io_mutex);
        for (const auto& [page_number, contents] : pages)
            write_page(page_number, contents);
        sync_file(_file);
    }
    remove_file(_wal_name);
}

pager::transaction::transaction(pager& owner) :
    _owner(owner), _root_page(0), _page_count(0), _free_page(0), _committed(false)
{
    auto header = decode_header(_owner.read(0));
    _root_page = header.root_page;
    _page_count = header.page_count;
    _free_page = header.free_page;
}

const pager::page& pager::transaction::read(uint64_t page_number)
{
    auto found = _pages.find(page_number);
    if (found == _pages.end())
        found = _pages.emplace(page_number, _owner.read(page_number)).first;
    return found->second;
}

pager::page& pager::transaction::write(uint64_t page_number)
{
    (void)read(page_number);
    _dirty_pages.insert(page_number);
    return _pages.at(page_number);
}

uint64_t pager::transaction::allocate()
{
    if (_free_page != 0)
    {
        const uint64_t result = _free_page;
        const page& free_page = read(result);
        uint64_t magic = 0;
        memcpy(&magic, free_page.data(), sizeof(magic));
        if (magic != free_page_magic)
            throw runtime_error("Corrupt free-page list.");
        memcpy(&_free_page, free_page.data() + sizeof(magic), sizeof(_free_page));
        write(result).fill(0);
        return result;
    }
    const uint64_t result = _page_count++;
    _pages[result] = page{};
    _dirty_pages.insert(result);
    return result;
}

void pager::transaction::release(uint64_t page_number)
{
    if (page_number == 0 || page_number >= _page_count)
        throw invalid_argument("Invalid page release.");
    page& released = write(page_number);
    released.fill(0);
    memcpy(released.data(), &free_page_magic, sizeof(free_page_magic));
    memcpy(released.data() + sizeof(free_page_magic), &_free_page, sizeof(_free_page));
    _free_page = page_number;
}

uint64_t pager::transaction::root_page() const { return _root_page; }
void pager::transaction::set_root_page(uint64_t page_number) { _root_page = page_number; }

void pager::transaction::commit()
{
    if (_committed) throw logic_error("Transaction has already committed.");
    encode_header(write(0), _root_page, _page_count, _free_page);
    map<uint64_t, page> dirty;
    for (uint64_t page_number : _dirty_pages)
        dirty.emplace(page_number, _pages.at(page_number));
    _owner.commit(dirty);
    _committed = true;
}
