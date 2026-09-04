#ifndef TDB_B_TREE_H
#define TDB_B_TREE_H

#include "tdb/pager.h"

#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>

class b_tree
{
public:
    b_tree(const std::string& file_name, std::uint16_t min_degree);

    void insert(std::int64_t key, std::int64_t value);
    std::optional<std::int64_t> search(std::int64_t key);
    void remove(std::int64_t key);
    void write_dot_file(const std::string& file_name);

    static void create_db_file(const std::string& file_name);
    static void vacuum(const std::string& file_name);

private:
    pager _pager;
    std::uint16_t _max_keys;

    static std::shared_mutex _tree_mutex;
};

#endif
