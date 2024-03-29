#include "tdb/b_tree.h"
#include <iostream>
#include <fstream>
#include <queue>

using namespace std;

b_tree::b_tree(const string& file_name, uint16_t min_degree) :
    _p(file_name),
    _min_degree(min_degree)
{
}

void b_tree::insert(int64_t key, int64_t value) {
    bool inserted = false;
    int attempt = 0;
    while (!inserted) {
        int64_t old_root_ofs = _p.root_ofs();
        if (old_root_ofs == 0) {
            // If the tree is empty, create a new root node and insert the key-value pair
            b_tree_node root(_p, _min_degree, true);
            root._set_num_keys(1);
            root._set_key(0, key);
            root._set_valid_key(0, true);
            root._set_val(0, value);
            if(_p.set_root_ofs(0, root._ofs()))
                inserted = true;
            else { printf("Failed to set root ofs 1 %d\n",attempt); ++attempt; }
        } else {
            // Copy the arm of the tree from the root to the leaf node
            printf("Copied arm ofs: ");
            b_tree_node copied_root = _copy_arm(key, old_root_ofs);
            printf("\n");

            // Traverse down the copied arm and insert the key-value pair
            if (copied_root._num_keys() == 2 * _min_degree - 1) {
                printf("Root is full\n");
                // If the root node is full, split it preemptively
                b_tree_node new_root(_p, _min_degree, false);
                new_root._set_child_ofs(0, copied_root._ofs());
                new_root._split_child(0, copied_root._ofs());
                printf("insert ofs s: ");
                _insert_atomic_recursive(key, value, new_root._ofs());
                printf("\n");

                if(_p.set_root_ofs(old_root_ofs, new_root._ofs()))
                    inserted = true;
                else { printf("Failed to set root ofs 2 %d\n",attempt); ++attempt; }
            }
            else
            {
                printf("insert ofs: ");
                _insert_atomic_recursive(key, value, copied_root._ofs());
                printf("\n");
                if(_p.set_root_ofs(old_root_ofs, copied_root._ofs()))
                    inserted = true;
                else { printf("Failed to set root ofs 3 %d\n",attempt); ++attempt; }
            }
        }
    }
}

optional<int64_t> b_tree::search(int64_t k)
{
    auto root_ofs = _p.root_ofs();
    std::optional<int64_t> result;
    if (root_ofs != 0)
    {
        b_tree_node root(_p, root_ofs);
        result = root._search(k);
    }
    return result;
}

void b_tree::remove(int64_t k)
{
    auto root_ofs = _p.root_ofs();
    if (root_ofs != 0)
    {
        b_tree_node root(_p, root_ofs);
        root._remove(k);
    }
}

void b_tree::write_dot_file(const string& file_name)
{
    ofstream file(file_name);
    if (!file.is_open())
    {
        cout << "Unable to open file: " << file_name << endl;
        return;
    }

    file << "digraph BTree {\n";
    file << "  node [shape=record];\n";

    queue<pair<int64_t, int>> q;
    int node_id = 0;
    auto root_ofs = _p.root_ofs();
    q.push({root_ofs, node_id++});

    while (!q.empty())
    {
        b_tree_node node(_p, q.front().first);
        int parent_id = q.front().second;
        q.pop();

        int current_id = node_id++;

        file << "  node" << current_id << " [label=\"";
        for (int i = 0; i < node._num_keys(); i++)
        {
            if (i > 0)
                file << "|";
            if(node._valid_key(i))
                file << "<f" << i << "> " << node._key(i) << " (" << node._val(i) << ")";
        }
        file << "\"];\n";

        if (parent_id != -1)
        {
            file << "  node" << parent_id << " -> node" << current_id << ";\n";
        }

        if (!node._leaf())
        {
            for (int i = 0; i <= node._num_keys(); i++)
            {
                q.push({node._child_ofs(i), current_id});
            }
        }
    }

    file << "}\n";
    file.close();
}

void b_tree::create_db_file(const std::string& file_name)
{
    pager::create(file_name);
}

void b_tree::vacuum(const std::string& file_name)
{
    // create temp file name
    // call create_db_file with temp file name
    // pager open file_name
    // read root node and traverse tree writing each element to temp file
    // rename temp file to file_name
}

b_tree_node b_tree::_copy_arm(int64_t key, int64_t node_ofs)
{
    b_tree_node current_node(_p, node_ofs);
    b_tree_node new_node(_p, current_node._min_degree(), current_node._leaf());

    printf("%ld ", new_node._ofs());

    // Copy keys, values, and valid flags from the current node to the new node
    memcpy(new_node._mm.map().first, current_node._mm.map().first, _p.block_size());

    int i = 0;
    while (i < current_node._num_keys() && key > current_node._key(i))
        i++;

    // If the current node is a leaf, return the new node
    if (current_node._leaf())
        return new_node;

    // If the current node is an internal node, recursively copy the child arm
    b_tree_node child_node = _copy_arm(key, current_node._child_ofs(i));

    // Update the child offset in the new node to point to the copied child arm
    new_node._set_child_ofs(i, child_node._ofs());

    return new_node;
}

void b_tree::_insert_atomic_recursive(int64_t key, int64_t value, int64_t node_ofs)
{
    b_tree_node node(_p, node_ofs);
    printf("%ld ", node._ofs());

    // If the node is a leaf, insert the key-value pair
    if (node._leaf())
        node._insert_non_full(key, value);
    else
    {
        // If the node is an internal node, find the appropriate child to descend into
        int i = 0;
        while (i < node._num_keys() && key > node._key(i))
            i++;

        b_tree_node child(_p, node._child_ofs(i));
        if (child._num_keys() == 2 * _min_degree - 1) {
            // If the child node is full, split it before descending
            printf("split child ofs\n");
            node._split_child(i, child._ofs());
            if (key > node._key(i))
                i++;
        }

        // Recursively insert the key-value pair into the appropriate child
        _insert_atomic_recursive(key, value, node._child_ofs(i));
    }
}
