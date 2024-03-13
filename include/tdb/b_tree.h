
#ifndef __b_tree_h
#define __b_tree_h

// TODO
// - Write more tests.
// - Shrink the pager page size to be closer / equal to node size.
// - More atomic removes
//   - Since inserts are append only I think we can delete a key with the following steps:
//      - Read of the ofs of the root node.
//      - Traverse the tree to find the key to delete.
//      - Update valid flag of the key to delete.
//      - CAS update of root node. If we fail, we need to traverse the tree again and try to delete the key again. *NOTE* This relies on __sync_bool_compare_and_swap() returning true when the cmpval and exchval are the same.

#include "tdb/b_tree_node.h"
#include <string>
#include <stdexcept>
#include <memory>
#include <optional>

class b_tree
{
public:
    b_tree(const std::string& file_name, uint16_t min_degree);
 
    void insert(int64_t key, int64_t value);
    std::optional<int64_t> search(int64_t k);
    void remove(int64_t k);
    void write_dot_file(const std::string& file_name);

    static void create_db_file(const std::string& file_name);
    static void vacuum(const std::string& file_name);

private:
    b_tree_node _copy_arm(int64_t key, int64_t node_ofs);
    void _insert_atomic_recursive(int64_t key, int64_t value, int64_t node_ofs);

    pager _p;
    uint16_t _min_degree;
};

#endif
