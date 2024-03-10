
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
    // Constructor (Initializes tree as empty)
    b_tree(const std::string& file_name, uint16_t min_degree) :
        _p(file_name),
        _root(_p.root_ofs()),
        _min_degree(min_degree)
    {        
    }
 
    // function to traverse the tree
    void traverse()
    {
        if (_root != 0)
        {
            b_tree_node root(_p, _root);
            root.traverse();
        }
    }
 
    // function to search a key in this tree
    std::optional<int64_t> search(int64_t k)
    {
        std::optional<int64_t> result;
        if (_root != 0)
        {
            b_tree_node root(_p, _root);
            result = root.search(k);
        }
        return result;
    }
 
    // The main function that inserts a new key in this B-Tree
    void insert(int64_t k, int64_t v);

    // Function to write the B-tree to a Graphviz DOT file
    void write_dot_file(const std::string& file_name);

    static void create_db_file(const std::string& file_name)
    {
        pager::create(file_name);
    }

private:
    pager _p;
    int64_t _root; // Pointer to root node
    uint16_t _min_degree;  // Minimum degree
};

#endif
