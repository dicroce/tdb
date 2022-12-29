
#ifndef __pager_h
#define __pager_h

#include "tdb/file_utils.h"
#include <string>

class pager final
{
public:
    pager(const std::string& fileName);
    pager(const pager&) = delete;
    pager(pager&&) = delete;
    ~pager() noexcept;
    pager& operator=(const pager&) = delete;
    pager& operator=(pager&&) = delete;

    static size_t block_size();

    static void create(const std::string& fileName);

    uint64_t block_start_from(uint64_t ofs) const;

    r_memory_map map_page_from(uint64_t ofs) const;

    uint64_t append_page() const;

    uint64_t root_ofs() const;
    void set_root_ofs(uint64_t ofs) const;

    void sync() const;

private:
    uint32_t _read_nblocks() const;

    bool _update_nblocks(uint32_t lastVal, uint32_t newVal) const;

    uint64_t _read_root_ofs() const;

    bool _update_root_ofs(uint64_t lastVal, uint64_t newVal) const;

    std::string _fileName;
    r_file _f;
};

#endif
