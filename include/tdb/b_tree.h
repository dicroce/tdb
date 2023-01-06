
#ifndef __b_tree_h
#define __b_tree_h

// TODO
// - More atomic inserts
//   - Add a new atomic insert path
//     - this will require an atomic _insert_non_full() and _split_child() functions.
//     - The atomic versions of these functions will limit its modifications to the existing
//       tree to CAS updates of child_ofs.
//       - This will work by duplicating existing nodes at the end of the file (with append_page())
//         and updating the child_ofs of the parent node to point to the new node with a CAS.
//         BUT if the cas fails, then the whole insert needs to be retried.
//     - At this point, insert() should be thread safe requiring no locking.
// - delete
//   - Find the node containing the value you want to delete.
//     - pager allocate a new node at the end of the file
//     - copy the keys, values and child_ofs from the node you want to delete to the new node skipping the index of the matching key (skip the key, value and child_ofs at the index)
//     - set the child_ofs of the parent node to the new node using a CAS
//       - if the node has no parent, then the new node is the new root
//     - This simplified version of delete will not merge nodes
//       - This will result in a slightly unbalanced tree until the next vacuum
//       - The advantage is that it is much simpler to implement and will be faster

#include "tdb/b_tree_node.h"
#include <string>
#include <stdexcept>
#include <memory>
#include <optional>

template<typename KEY_T>
class b_tree
{
public:
    b_tree() = delete;
    b_tree(const std::string& file_name, int degree = 4) :
        _file_name(file_name),
        _p(file_name),
        _degree(degree)
    {
    }
    b_tree(const b_tree&) = delete;
    b_tree(b_tree&&) = delete;

    b_tree& operator=(const b_tree&) = delete;
    b_tree& operator=(b_tree&&) = delete;

    static void create_db_file(const std::string& file_name)
    {
        pager::create(file_name);
    }

    template<typename CMP>
    void insert(KEY_T k, uint64_t v, CMP cmp)
    {
        auto root_ofs = _p.root_ofs();
        if(root_ofs == 0)
        {
            auto new_root_ofs = _p.append_page();
            b_tree_node<KEY_T> new_root_node(_p, new_root_ofs, _degree, true);
            new_root_node.set_key(0, k);
            new_root_node.set_val(0, v);
            new_root_node.set_num_keys(1);

            _p.set_root_ofs(new_root_ofs);
        }
        else
        {
            auto old_root = b_tree_node<KEY_T>(_p, root_ofs);
            if(old_root.num_keys() == _degree-1)
            {
                // Handle the full root node case...
                
                auto new_root_ofs = _p.append_page();
                b_tree_node<KEY_T> new_root_node(_p, new_root_ofs, _degree, false);
                new_root_node.set_child_ofs(0, root_ofs);
                new_root_node._split_child(0, root_ofs);

                // New root has two _children now.  Decide which of the
                // two _children is going to have new key
                int i = 0;
                if(new_root_node.key(i) < k)
                    ++i;
                auto new_child = b_tree_node<KEY_T>(_p, new_root_node.child_ofs(i));
                new_child._insert_non_full(k, v, cmp);

                // Change root
                _p.set_root_ofs(new_root_ofs);
            }
            else
            {
                old_root._insert_non_full(k, v, cmp);
            }
        }
    }

    template<typename CMP>
    std::optional<uint64_t> search(KEY_T k, CMP cmp)
    {
        auto root_ofs = _p.root_ofs();
        if(root_ofs == 0)
            throw std::runtime_error("empty tree");
        b_tree_node<KEY_T> root_node(_p, root_ofs);
        auto search_results = root_node._search(k, cmp);
        std::optional<uint64_t> result;
        if(search_results)
            result = search_results->first.val(search_results->second);
        return result;
    }

    void render_to_dot_file(const std::string& file_name)
    {
        auto root_ofs = _p.root_ofs();

        std::string tree;
        tree = "digraph G {\n";
        _build_dot_tree(root_ofs, tree);
        tree += "}\n";
        std::shared_ptr<FILE> fp(fopen(file_name.c_str(), "w"), fclose);
        fprintf(fp.get(), "%s", tree.c_str());
    }

    void traverse()
    {
        auto root_ofs = _p.root_ofs();
        if(root_ofs == 0)
            throw std::runtime_error("empty tree");
        b_tree_node<KEY_T> root_node(_p, root_ofs);
        root_node._traverse();
    }

private:
    void _build_dot_tree(uint64_t root_ofs, std::string& tree, int nodeNum=1)
    {
        auto root = b_tree_node<KEY_T>(_p, root_ofs);

        // Print the root node
        tree += "node" + std::to_string(nodeNum) + " [label=\"";
        for(int i = 0; i < root.num_keys(); ++i)
            tree += std::to_string(root.key(i)) + " ";
        tree += "\"]\n";

        auto n_keys = root.num_keys();
        // Print the child nodes
        int childNum = 1;
        for(int i = 0; i < n_keys+1; ++i)
        {
            if(root.child_ofs(i) != 0)
            {
                tree += "node" + std::to_string(nodeNum) + " -> node" + std::to_string(nodeNum*10+childNum) + "\n";
                _build_dot_tree(root.child_ofs(i), tree, nodeNum*10+childNum);
                ++childNum;
            }
        }
    }

    std::string _file_name;
    pager _p;
    int _degree;
};

#endif
