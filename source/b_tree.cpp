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

void b_tree::insert(int64_t k, int64_t v)
{
    auto root_ofs = _p.root_ofs();

    if (root_ofs == 0)
    {
        // Allocate memory for root
        b_tree_node root(_p, _min_degree, true);
        root._set_num_keys(1);  // Update number of keys in root
        root._set_key(0, k);  // Insert key
        root._set_valid_key(0, true);  // Mark key as valid
        root._set_val(0, v);  // Insert value

        _p.set_root_ofs(root._ofs());
    }
    else _insert(k, v, root_ofs);
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

void b_tree::_insert(int64_t k, int64_t v, int64_t root_ofs)
{
    b_tree_node root(_p, root_ofs);
    if(root._full())
    {
        // If the root is full then:
        //     Make a new node.
        //     Set our old root ofs as its child 0 and call _split_child on it. Our new node will then have two children.
        //     Scan our new node and if the key we are inserting is less than the key at index i, then call _insert_non_full() on the node at child i.

        b_tree_node new_root(_p, _min_degree, false);
        new_root._set_child_ofs(0, root_ofs);
        new_root._split_child(0, root_ofs);

        int i = 0;
        if(new_root._key(i) < k)
            i++;

        b_tree_node new_child(_p, new_root._child_ofs(i));
        new_child._insert_non_full(k, v);

        _p.set_root_ofs(new_root._ofs());
    }
    else root._insert_non_full(k, v);
}