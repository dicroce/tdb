#ifndef TDB_ROW_STORE_H
#define TDB_ROW_STORE_H

#include "tdb/b_tree.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

using row_id = std::uint64_t;

class row_store
{
public:
    row_id insert(b_tree_write_transaction& transaction,
                  std::span<const std::uint8_t> bytes) const;
    std::optional<std::vector<std::uint8_t>> get(
        b_tree_read_transaction& transaction, row_id id) const;
    std::optional<std::vector<std::uint8_t>> get(
        b_tree_write_transaction& transaction, row_id id) const;
    bool remove(b_tree_write_transaction& transaction, row_id id) const;
};

#endif
