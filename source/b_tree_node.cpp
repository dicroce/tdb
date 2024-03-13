
#include "tdb/b_tree_node.h"
#include <iostream>

using namespace std;

// Object copy constructor, non deep copy
b_tree_node::b_tree_node(const b_tree_node& obj) :
    _p(obj._p),
    _ofs_field(obj._ofs_field),
    _mm(_p.map_page_from(_ofs_field)),
    _min_degree_field(nullptr),
    _leaf_field(nullptr),
    _num_keys_field(nullptr),
    _keys_field(nullptr),
    _valid_keys_field(nullptr),
    _vals_field(nullptr),
    _child_ofs_field(nullptr)
{
    auto read_ptr = _mm.map().first;

    _min_degree_field = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _leaf_field = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _num_keys_field = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _keys_field = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree_field * 2) - 1);

    _valid_keys_field = (uint8_t*)read_ptr;
    read_ptr += sizeof(uint8_t) * ((*_min_degree_field * 2) - 1);

    _vals_field = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree_field * 2) - 1);

    _child_ofs_field = (int64_t*)read_ptr;
}

b_tree_node::b_tree_node(const pager& p, uint16_t min_degree, bool leaf) :
    _p(p),
    _ofs_field(_p.append_page()),
    _mm(_p.map_page_from(_ofs_field)),
    _min_degree_field(nullptr),
    _leaf_field(nullptr),
    _num_keys_field(nullptr),
    _keys_field(nullptr),
    _valid_keys_field(nullptr),
    _vals_field(nullptr),
    _child_ofs_field(nullptr)
{
    auto read_ptr = _mm.map().first;

    _min_degree_field = (uint16_t*)read_ptr;
    *(_min_degree_field) = min_degree;
    read_ptr += sizeof(uint16_t);

    _leaf_field = (uint16_t*)read_ptr;
    *(_leaf_field) = leaf ? 1 : 0;
    read_ptr += sizeof(uint16_t);

    _num_keys_field = (uint16_t*)read_ptr;
    *(_num_keys_field) = 0;
    read_ptr += sizeof(uint16_t);

    _keys_field = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree_field * 2) - 1);

    _valid_keys_field = (uint8_t*)read_ptr;
    read_ptr += sizeof(uint8_t) * ((*_min_degree_field * 2) - 1);

    _vals_field = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree_field * 2) - 1);

    _child_ofs_field = (int64_t*)read_ptr;
}

b_tree_node::b_tree_node(const pager& p, int64_t ofs) :
    _p(p),
    _ofs_field(ofs),
    _mm(_p.map_page_from(_ofs_field))
{
    if(ofs == 0)
        throw runtime_error("Unable to create b_tree_node from offset 0");

    auto read_ptr = _mm.map().first;

    _min_degree_field = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _leaf_field = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _num_keys_field = (uint16_t*)read_ptr;
    read_ptr += sizeof(uint16_t);

    _keys_field = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree_field * 2) - 1);

    _valid_keys_field = (uint8_t*)read_ptr;
    read_ptr += sizeof(uint8_t) * ((*_min_degree_field * 2) - 1);

    _vals_field = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree_field * 2) - 1);

    _child_ofs_field = (int64_t*)read_ptr;
}

b_tree_node& b_tree_node::operator=(const b_tree_node& obj)
{
    if (this != &obj)
    {
        _ofs_field = obj._ofs_field;
        _mm = _p.map_page_from(_ofs_field);

        auto read_ptr = _mm.map().first;

        _min_degree_field = (uint16_t*)read_ptr;
        read_ptr += sizeof(uint16_t);

        _leaf_field = (uint16_t*)read_ptr;
        read_ptr += sizeof(uint16_t);

        _num_keys_field = (uint16_t*)read_ptr;
        read_ptr += sizeof(uint16_t);

        _keys_field = (int64_t*)read_ptr;
        read_ptr += sizeof(int64_t) * ((*_min_degree_field * 2) - 1);

        _valid_keys_field = (uint8_t*)read_ptr;
        read_ptr += sizeof(uint8_t) * ((*_min_degree_field * 2) - 1);

        _vals_field = (int64_t*)read_ptr;
        read_ptr += sizeof(int64_t) * ((*_min_degree_field * 2) - 1);

        _child_ofs_field = (int64_t*)read_ptr;
    }
    return *this;
}

void b_tree_node::_insert_non_full(int64_t k, int64_t v)
{
    // Initialize index as index of rightmost element
    int i = _num_keys()-1;
 
    // If this is a leaf node
    if (_leaf())
    {
        // The following loop does two things
        // a) Finds the location of new key to be inserted
        // b) Moves all greater keys to one place ahead
        while (i >= 0 && _keys_field[i] > k)
        {
            _keys_field[i+1] = _keys_field[i];
            _valid_keys_field[i+1] = _valid_keys_field[i];
            _vals_field[i+1] = _vals_field[i];
            i--;
        }

        if(i >= 0 && (_keys_field[i] == k && _valid_keys_field[i] == 1))
            throw runtime_error("Duplicate key");
 
        // Insert the new key at found location
        _keys_field[i+1] = k;
        _valid_keys_field[i+1] = 1;
        _vals_field[i+1] = v;
        _set_num_keys(_num_keys() + 1);
    }
    else // If this node is not leaf
    {
        // Find the child which is going to have the new key
        while (i >= 0 && _keys_field[i] > k)
            i--;
 
        b_tree_node child(_p, _child_ofs(i+1));
        // See if the found child is full
        if (child._num_keys() == 2*_min_degree() - 1)
        {
            // If the child is full, then split it
            _split_child(i+1, _child_ofs(i+1));
 
            // After split, the middle key of _child_ofs[i] goes up and
            // _child_ofs[i] is splitted into two.  See which of the two
            // is going to have the new key
            if (_keys_field[i+1] < k)
                i++;
        }

        b_tree_node new_child(_p, _child_ofs_field[i+1]);
        new_child._insert_non_full(k, v);
    }
}

void b_tree_node::_split_child(int i, int64_t ofs)
{
    b_tree_node y(_p, ofs);

    // Create a new node which is going to store (t-1) keys
    // of y
    b_tree_node z(_p, y._min_degree(), y._leaf());
    z._set_num_keys(_min_degree() - 1);
 
    // Copy the last (min_degree-1) keys of y to z
    for (int j = 0; j < _min_degree()-1; j++)
    {
        z._keys_field[j] = y._keys_field[j+_min_degree()];
        z._valid_keys_field[j] = y._valid_keys_field[j+_min_degree()];
        z._vals_field[j] = y._vals_field[j+_min_degree()];
    }
 
    // Copy the last min_degree children of y to z
    if (y._leaf() == false)
    {
        for (int j = 0; j < _min_degree(); j++)
            z._child_ofs_field[j] = y._child_ofs_field[j+_min_degree()];
    }
 
    // Reduce the number of keys in y
    y._set_num_keys(_min_degree() - 1);
 
    // Since this node is going to have a new child,
    // create space of new child
    for (int j = _num_keys(); j >= i+1; j--)
        _child_ofs_field[j+1] = _child_ofs_field[j];
 
    // Link the new child to this node
    _child_ofs_field[i+1] = z._ofs();
 
    // A key of y will move to this node. Find the location of
    // new key and move all greater keys one space ahead
    for (int j = _num_keys()-1; j >= i; j--)
    {
        _keys_field[j+1] = _keys_field[j];
        _valid_keys_field[j+1] = _valid_keys_field[j];
        _vals_field[j+1] = _vals_field[j];
    }
 
    // Copy the middle key of y to this node
    _keys_field[i] = y._keys_field[_min_degree()-1];
    _valid_keys_field[i] = y._valid_keys_field[_min_degree()-1];
    _vals_field[i] = y._vals_field[_min_degree()-1];
 
    // Increment count of keys in this node
    _set_num_keys(_num_keys() + 1);
}

optional<int64_t> b_tree_node::_search(int64_t k)
{
    // Find the first key greater than or equal to k
    int i = 0;
    while (i < _num_keys() && k > _keys_field[i])
        i++;
 
    optional<int64_t> result;
    // If the found key is equal to k, return this node
    if (_keys_field[i] == k)
    {
        if(_valid_keys_field[i] == 1)
            result = _vals_field[i];
        return result;
    }
 
    // If key is not found here and this is a leaf node
    if (_leaf() == true)
        return result;
 
    // Go to the appropriate child
    b_tree_node child(_p, _child_ofs(i));
    return child._search(k);
}

void b_tree_node::_remove(int64_t k)
{
    int i = 0;
    while(i < _num_keys() && _keys_field[i] < k)
        i++;

    if(i < _num_keys() && _keys_field[i] == k)
    {
        _valid_keys_field[i] = 0;
        return;
    }

   if(_leaf())
        return;

    b_tree_node child(_p, _child_ofs(i));
    child._remove(k);
}
