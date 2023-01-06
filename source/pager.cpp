
#include "tdb/pager.h"
#include "tdb/file_utils.h"
#include <string>
#include <vector>

using namespace std;

pager::pager(const std::string& fileName) :
    _fileName(fileName),
    _f(r_file::open(fileName, "r+"))
{
}

pager::~pager() noexcept
{
}

size_t pager::block_size()
{
    return 4096;
}

void pager::create(const std::string& fileName)
{
    auto f = r_file::open(fileName, "w+");

    vector<uint8_t> block(pager::block_size());

    memset(&block[0], 0, pager::block_size());
    *(uint32_t*)&block[0] = 1;
    *(uint64_t*)&block[4] = 0;

    block_write_file(&block[0], pager::block_size(), f);
}

uint64_t pager::block_start_from(uint64_t ofs) const
{
    return (ofs / pager::block_size()) * pager::block_size();
}

r_memory_map pager::map_page_from(uint64_t ofs) const
{
    auto blockStart = block_start_from(ofs);
    size_t blockOfs = ofs - blockStart;

    return std::move(r_memory_map(fileno(_f),
                                blockStart,
                                pager::block_size(),
                                r_memory_map::MM_PROT_READ | r_memory_map::MM_PROT_WRITE,
                                r_memory_map::MM_TYPE_FILE | r_memory_map::MM_SHARED,
                                blockOfs));
}

uint64_t pager::append_page() const
{
    uint32_t lastNBlocks;

    // The idea here is that we want to append space for 1 block to the end of the file and update our
    // nblocks field in the special block at the beginning of the file. We'd also like to return the file
    // offset of the new blocks.
    //
    // To keep this lock free I'm calling _update_nblocks() (which uses the gcc compiler intrinsic for
    // compare and swap).

    do {
        lastNBlocks = _read_nblocks();
        auto err = ftruncate(fileno(_f), ((lastNBlocks+1)*pager::block_size()));
        if(err != 0)
            throw std::runtime_error("ftruncate failed");
    } while(!_update_nblocks(lastNBlocks, lastNBlocks+1));
    
    //return pager::block_size() + (lastNBlocks * pager::block_size());
    return lastNBlocks * pager::block_size();
}

void pager::copy_page(uint64_t srcOfs, uint64_t dstOfs) const
{
    auto src_map = map_page_from(srcOfs);
    auto dst_map = map_page_from(dstOfs);
    memcpy(dst_map.map().first, src_map.map().first, pager::block_size());
}

uint64_t pager::root_ofs() const
{
    return _read_root_ofs();
}

void pager::set_root_ofs(uint64_t ofs) const
{
    uint64_t lastRootOfs;

    do {
        lastRootOfs = _read_root_ofs();
    } while(!_update_root_ofs(lastRootOfs, ofs));
}

void pager::sync() const
{
    fsync(fileno(_f));
}

uint32_t pager::_read_nblocks() const
{
    auto mm = map_page_from(0);
    auto mp = mm.map();
    return *((uint32_t*)mp.first);
}

bool pager::_update_nblocks(uint32_t lastVal, uint32_t newVal) const
{
    uint32_t out = __sync_val_compare_and_swap((uint32_t*)map_page_from(0).map().first, lastVal, newVal);
    return out == lastVal;
}

uint64_t pager::_read_root_ofs() const
{
    auto mm = map_page_from(0);
    auto mp = mm.map();
    return *((uint64_t*)(mp.first + 4));
}

bool pager::_update_root_ofs(uint64_t lastVal, uint64_t newVal) const
{
    uint64_t out = __sync_val_compare_and_swap((uint64_t*)(map_page_from(0).map().first + 4), lastVal, newVal);
    return out == lastVal;
}
