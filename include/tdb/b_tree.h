
#ifndef __b_tree_h
#define __b_tree_h

// TODO
// - Write more tests.
// - Shrink the pager page size to be closer / equal to node size.
// - More atomic inserts
//   - Add a new atomic insert path
//     - this will require an atomic _insert_non_full() and _split_child() functions.
//     - The atomic versions of these functions will limit its modifications to the existing
//       tree to CAS updates of child_ofs.
//       - This will work by duplicating existing nodes at the end of the file (with append_page())
//         and updating the child_ofs of the parent node to point to the new node with a CAS.
//         BUT if the cas fails, then the whole insert needs to be retried.
//     - At this point, insert() should be thread safe requiring no locking.
// - remove()
//   - modify remove to use cas to publish the changed node.
//   - modify b_tree_node to have "deleted" flag
//   - add vacuum method to pager + b_tree

#include "tdb/b_tree_node.h"
#include <string>
#include <stdexcept>
#include <memory>
#include <optional>

class b_tree
{
public:
    b_tree() = delete;
    b_tree(const std::string& file_name);
    b_tree(const b_tree&) = delete;
    b_tree(b_tree&&) = delete;

    b_tree& operator=(const b_tree&) = delete;
    b_tree& operator=(b_tree&&) = delete;

    static void create_db_file(const std::string& file_name);

    void insert(int64_t k, uint64_t v);
    void remove(int64_t k);
    std::optional<uint64_t> search(int64_t k);
    void render_to_dot_file(const std::string& file_name);
    void traverse();

private:
    void _build_dot_tree(uint64_t root_ofs, std::string& tree, int nodeNum=1);

    std::string _file_name;
    pager _p;
    int _degree;
};

#endif
