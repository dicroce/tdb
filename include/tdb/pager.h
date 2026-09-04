#ifndef TDB_PAGER_H
#define TDB_PAGER_H

#include "tdb/file_utils.h"

#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>

class pager final
{
public:
    static constexpr std::size_t page_size = 4096;
    using page = std::array<std::uint8_t, page_size>;

    class transaction final
    {
    public:
        explicit transaction(pager& owner);

        const page& read(std::uint64_t page_number);
        page& write(std::uint64_t page_number);
        std::uint64_t allocate();
        void release(std::uint64_t page_number);
        std::uint64_t root_page() const;
        void set_root_page(std::uint64_t page_number);
        void commit();

    private:
        pager& _owner;
        std::map<std::uint64_t, page> _pages;
        std::set<std::uint64_t> _dirty_pages;
        std::uint64_t _root_page;
        std::uint64_t _page_count;
        std::uint64_t _free_page;
        bool _committed;
    };

    explicit pager(const std::string& file_name);
    pager(const pager&) = delete;
    pager& operator=(const pager&) = delete;

    static void create(const std::string& file_name);

    page read(std::uint64_t page_number) const;
    std::uint64_t root_page() const;
    transaction begin_transaction();

private:
    friend class transaction;

    void recover();
    void commit(const std::map<std::uint64_t, page>& pages);
    void write_page(std::uint64_t page_number, const page& contents);

    std::string _file_name;
    std::string _wal_name;
    r_file _file;
    mutable std::mutex _io_mutex;
};

#endif
