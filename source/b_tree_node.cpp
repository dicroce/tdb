
#include "tdb/b_tree_node.h"
#include <iostream>

using namespace std;

b_tree_node::b_tree_node(const pager& p, uint16_t min_degree, bool leaf) :
    _p(p),
    _ofs(_p.append_page()),
    _mm(_p.map_page_from(_ofs)),
    _min_degree(nullptr),
    _leaf(nullptr),
    _num_keys(nullptr),
    _keys(nullptr),
    _valid_keys(nullptr),
    _vals(nullptr),
    _child_ofs(nullptr)
{
    auto read_ptr = _mm.map().first;

    _min_degree = (uint16_t*)read_ptr;
    *(_min_degree) = min_degree;
    read_ptr += sizeof(uint16_t);

    _leaf = (uint16_t*)read_ptr;
    *(_leaf) = leaf ? 1 : 0;
    read_ptr += sizeof(uint16_t);

    _num_keys = (uint16_t*)read_ptr;
    *(_num_keys) = 0;
    read_ptr += sizeof(uint16_t);

    _keys = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree * 2) - 1);

    _valid_keys = (uint8_t*)read_ptr;
    read_ptr += sizeof(uint8_t) * ((*_min_degree * 2) - 1);

    _vals = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree * 2) - 1);

    _child_ofs = (int64_t*)read_ptr;
}

b_tree_node::b_tree_node(const pager& p, int64_t ofs) :
    _p(p),
    _ofs(ofs),
    _mm(_p.map_page_from(_ofs))
{
    if(ofs == 0)
        throw runtime_error("Unable to create b_tree_node from offset 0");

    auto read_ptr = _mm.map().first;

    _min_degree = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _leaf = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _num_keys = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _keys = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree * 2) - 1);

    _valid_keys = (uint8_t*)read_ptr;
    read_ptr += sizeof(uint8_t) * ((*_min_degree * 2) - 1);

    _vals = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree * 2) - 1);

    _child_ofs = (int64_t*)read_ptr;
}

void b_tree_node::insert_non_full(int64_t k, int64_t v)
{
    // Initialize index as index of rightmost element
    int i = num_keys()-1;
 
    // If this is a leaf node
    if (leaf())
    {
        // The following loop does two things
        // a) Finds the location of new key to be inserted
        // b) Moves all greater keys to one place ahead
        while (i >= 0 && _keys[i] > k)
        {
            _keys[i+1] = _keys[i];
            _valid_keys[i+1] = _valid_keys[i];
            _vals[i+1] = _vals[i];
            i--;
        }

        if(i >= 0 && (_keys[i] == k && _valid_keys[i] == 1))
            throw runtime_error("Duplicate key");
 
        // Insert the new key at found location
        _keys[i+1] = k;
        _valid_keys[i+1] = 1;
        _vals[i+1] = v;
        set_num_keys(num_keys() + 1);
    }
    else // If this node is not leaf
    {
        // Find the child which is going to have the new key
        while (i >= 0 && _keys[i] > k)
            i--;
 
        b_tree_node child(_p, child_ofs(i+1));
        // See if the found child is full
        if (child.num_keys() == 2*min_degree() - 1)
        {
            // If the child is full, then split it
            split_child(i+1, child_ofs(i+1));
 
            // After split, the middle key of _child_ofs[i] goes up and
            // _child_ofs[i] is splitted into two.  See which of the two
            // is going to have the new key
            if (_keys[i+1] < k)
                i++;
        }

        b_tree_node new_child(_p, _child_ofs[i+1]);
        new_child.insert_non_full(k, v);
    }
}

void b_tree_node::split_child(int i, int64_t ofs)
{
    b_tree_node y(_p, ofs);

    // Create a new node which is going to store (t-1) keys
    // of y
    b_tree_node z(_p, y.min_degree(), y.leaf());
    z.set_num_keys(min_degree() - 1);
 
    // Copy the last (min_degree-1) keys of y to z
    for (int j = 0; j < min_degree()-1; j++)
    {
        z._keys[j] = y._keys[j+min_degree()];
        z._valid_keys[j] = y._valid_keys[j+min_degree()];
        z._vals[j] = y._vals[j+min_degree()];
    }
 
    // Copy the last min_degree children of y to z
    if (y.leaf() == false)
    {
        for (int j = 0; j < min_degree(); j++)
            z._child_ofs[j] = y._child_ofs[j+min_degree()];
    }
 
    // Reduce the number of keys in y
    y.set_num_keys(min_degree() - 1);
 
    // Since this node is going to have a new child,
    // create space of new child
    for (int j = num_keys(); j >= i+1; j--)
        _child_ofs[j+1] = _child_ofs[j];
 
    // Link the new child to this node
    _child_ofs[i+1] = z.ofs();
 
    // A key of y will move to this node. Find the location of
    // new key and move all greater keys one space ahead
    for (int j = num_keys()-1; j >= i; j--)
    {
        _keys[j+1] = _keys[j];
        _valid_keys[j+1] = _valid_keys[j];
        _vals[j+1] = _vals[j];
    }
 
    // Copy the middle key of y to this node
    _keys[i] = y._keys[min_degree()-1];
    _valid_keys[i] = y._valid_keys[min_degree()-1];
    _vals[i] = y._vals[min_degree()-1];
 
    // Increment count of keys in this node
    set_num_keys(num_keys() + 1);
}

void b_tree_node::traverse()
{
    // There are n keys and n+1 children, traverse through n keys
    // and first n children
    int i;
    for (i = 0; i < num_keys(); i++)
    {
        // If this is not leaf, then before printing key[i],
        // traverse the subtree rooted with child C[i].
        if (leaf() == false)
        {
            b_tree_node child(_p, child_ofs(i));
            child.traverse();
        }
        if(_valid_keys[i] == 1)
            cout << " " << _keys[i];
    }
 
    // Print the subtree rooted with last child
    if (leaf() == false)
    {
        b_tree_node child(_p, child_ofs(i));
        child.traverse();
    }
}

optional<int64_t> b_tree_node::search(int64_t k)
{
    // Find the first key greater than or equal to k
    int i = 0;
    while (i < num_keys() && k > _keys[i])
        i++;
 
    optional<int64_t> result;
    // If the found key is equal to k, return this node
    if (_keys[i] == k)
    {
        if(_valid_keys[i] == 1)
            result = _vals[i];
        return result;
    }
 
    // If key is not found here and this is a leaf node
    if (leaf() == true)
        return result;
 
    // Go to the appropriate child
    b_tree_node child(_p, child_ofs(i));
    return child.search(k);
}

void b_tree_node::remove(int64_t k)
{
    int i = 0;
    while(i < num_keys() && _keys[i] < k)
        i++;

    if(i < num_keys() && _keys[i] == k)
    {
        _valid_keys[i] = 0;
        return;
    }

   if(leaf())
        return;

    b_tree_node child(_p, child_ofs(i));
    child.remove(k);
}
