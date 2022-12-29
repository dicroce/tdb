
#include "tdb/b_tree_node.h"
#include <iostream>
using namespace std;

b_tree_node::b_tree_node(const pager& p) :
    _p(p),
    _is_empty(true),
    _ofs(0),
    _mm(),
    _degree(nullptr),
    _leaf(nullptr),
    _num_keys(nullptr),
    _keys(nullptr),
    _child_ofs(nullptr)
{
}

b_tree_node::b_tree_node(const pager& p, uint64_t ofs) : 
    _p(p),
    _is_empty(false),
    _ofs(ofs),
    _mm(_p.map_page_from(_ofs))
{
    if(ofs == 0)
        throw std::runtime_error("Unable to create b_tree_node from offset 0");

    // map the block
    auto read_ptr = _mm.map().first;

    // read the degree
    _degree = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    // read the leaf flag
    _leaf = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    // read the number of keys
    _num_keys = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    // read the keys
    _keys = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((degree() * 2) - 1);

    // read the child offsets
    _child_ofs = (uint64_t*)read_ptr;
}

b_tree_node::b_tree_node(const pager& p, uint64_t ofs, int degree, bool leaf) : 
    _p(p),
    _is_empty(false),
    _ofs(ofs),
    _mm(_p.map_page_from(_ofs))
{
    // map the block
    auto write_ptr = _mm.map().first;

    // write the degree
    *(uint16_t*)write_ptr = degree;
    _degree = (uint16_t*)write_ptr;
    write_ptr += sizeof(uint16_t);

    // write the leaf flag
    *(uint16_t*)write_ptr = leaf ? 1 : 0;
    _leaf = (uint16_t*)write_ptr;
    write_ptr += sizeof(uint16_t);

    // write the number of keys
    *(uint16_t*)write_ptr = 0;
    _num_keys = (uint16_t*)write_ptr;
    write_ptr += sizeof(uint16_t);
    
    // write the keys
    memset(write_ptr, 0, sizeof(int64_t) * ((degree * 2) - 1));
    _keys = (int64_t*)write_ptr;
    write_ptr += sizeof(int64_t) * ((degree * 2) - 1);

    // write the child offsets
    _child_ofs = (uint64_t*)write_ptr;
    memset(write_ptr, 0, sizeof(uint64_t) * (degree * 2));
}

void b_tree_node::_insert_non_full(int64_t k)
{
    // Initialize index as index of rightmost element
    int i = num_keys() - 1;

    // If this is a leaf
    if(leaf())
    {
        // The following loop does two things
        // a) Finds the location of new key to be inserted
        // b) Moves all greater keys to one place ahead
        while(i >= 0 && _keys[i] > k)
        {
            set_key(i+1, key(i));
            --i;
        }

        // Insert the new key at found location
        set_key(i+1, k);
        set_num_keys(num_keys() + 1);
    }
    else // If this node is not leaf
    {
        // Find the child which is going to have the new key
        while(i >= 0 && key(i) > k)
            --i;

        auto found_child = b_tree_node(_p, child_ofs(i+1));

        // See if the found child is full
        if(found_child.num_keys() == 2*found_child.degree()-1)
        {
            // If the child is full, then split it
            _split_child(i+1, child_ofs(i+1));

            // After split, the middle key of C[i] goes up and
            // C[i] is splitted into two.  See which of the two
            // is going to have the new key
            if(key(i+1) < k)
                ++i;
        }

        auto found_child2 = b_tree_node(_p, child_ofs(i+1));

        found_child2._insert_non_full(k);
    }
}

void b_tree_node::_split_child(int i, uint64_t ofs)
{
    // Get the node to be split
    auto y = b_tree_node(_p, ofs);

    // Create a new node which is going to store (t-1) keys
    // of y
    auto z = b_tree_node(_p, _p.append_page(), y.degree(), y.leaf());

    z.set_num_keys(y.degree()-1);

    // Copy the last (t-1) keys of y to z
    for(int j = 0; j < y.degree()-1; ++j)
        z.set_key(j, y.key(j+y.degree()));

    // Copy the last t children of y to z
    if(!y.leaf())
    {
        for (int j = 0; j < y.degree(); ++j)
            z.set_child_ofs(j, y.child_ofs(j+y.degree()));
    }

    // Reduce the number of keys in y
    y.set_num_keys(y.degree()-1);

    // Since this node is going to have a new child,
    // create space of new child
    for(int j = num_keys(); j >= i+1; --j)
        set_child_ofs(j+1, child_ofs(j));

    // Link the new child to this node
    set_child_ofs(i+1, z._ofs);

    // A key of y will move to this node. Find location of
    // new key and move all greater keys one space ahead
    for(int j = num_keys()-1; j >= i; --j)
        set_key(j+1, key(j));

    // Copy the middle key of y to this node
    set_key(i, y.key(y.degree()-1));

    // Increment count of keys in this node
    set_num_keys(num_keys() + 1);
}

void b_tree_node::_traverse()
{
    // There are n keys and n+1 children, travers through n
    // keys and first n children
    int i;
    for(i = 0; i < num_keys(); ++i)
    {
        // If this is not leaf, then before printing key[i],
        // traverse the subtree rooted with child C[i].
        if(!leaf())
        {
            auto child = b_tree_node(_p, child_ofs(i));
            child._traverse();
        }
        std::cout << " " << _keys[i];
    }

    // Print the subtree rooted with last child
    if(!leaf())
    {
        auto child = b_tree_node(_p, child_ofs(i));
        child._traverse();
    }
}

b_tree_node b_tree_node::_search(int k)   // returns NULL if k is not present.
{
    // Find the first key greater than or equal to k
    int i = 0;
    while(i < num_keys() && k > key(i))
        ++i;

    // If the found key is equal to k, return this node
    if(_keys[i] == k)
        return b_tree_node(_p, child_ofs(i));

    // If key is not found here and this is a leaf node
    if(leaf())
        return b_tree_node(_p);

    // Go to the appropriate child
    auto child = b_tree_node(_p, child_ofs(i));
    return child._search(k);
}
