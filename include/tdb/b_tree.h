
#ifndef __b_tree_h
#define __b_tree_h

// TODO
// - Write more tests.
// - Shrink the pager page size to be closer / equal to node size.
// - More atomic inserts
//   - Two pass insert
//     - First pass, traverse tree and build duplicate arm of tree from and including root.
//       - Copy the keys and values of each node we visit to the passed in new node.
//       - For each child of the node we are on in the tree, allocate a new node and update the child_ofs of the passed in new node.
//       - Recurse into each child.
//     - Insert into duplicated arm.
//     - CAS update of root node to duplicated arm root node.
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
 
    void insert(int64_t k, int64_t v);
    std::optional<int64_t> search(int64_t k);
    void remove(int64_t k);
    void write_dot_file(const std::string& file_name);

    static void create_db_file(const std::string& file_name);
    static void vacuum(const std::string& file_name);

private:
    void _insert(int64_t k, int64_t v, int64_t root_ofs);

    pager _p;
    uint16_t _min_degree;
};

#endif
