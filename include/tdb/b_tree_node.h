
#ifndef __b_tree_node_h
#define __b_tree_node_h

#include "tdb/pager.h"
#include "tdb/file_utils.h"
#include <map>
#include <optional>
#include <iostream>

template<typename KEY_T>
class b_tree;

template<typename KEY_T>
class b_tree_node
{
    friend class b_tree<KEY_T>;

public:
    b_tree_node(const pager& p) :
        _p(p),
        _ofs(0),
        _mm(),
        _degree(nullptr),
        _leaf(nullptr),
        _num_keys(nullptr),
        _keys(nullptr),
        _vals(nullptr),
        _child_ofs(nullptr)
    {
    }

    b_tree_node(const pager& p, uint64_t ofs) :
        _p(p),
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
        _keys = (KEY_T*)read_ptr;
        read_ptr += sizeof(KEY_T) * (degree() - 1);

        // read the vals
        _vals = (uint64_t*)read_ptr;
        read_ptr += sizeof(uint64_t) * (degree() - 1);

        // read the child offsets
        _child_ofs = (uint64_t*)read_ptr;
    }

    b_tree_node(const pager& p, uint64_t ofs, int degree, bool leaf) :
        _p(p),
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
        memset(write_ptr, 0, sizeof(KEY_T) * (degree - 1));
        _keys = (KEY_T*)write_ptr;
        write_ptr += sizeof(KEY_T) * (degree - 1);

        // write the vals
        memset(write_ptr, 0, sizeof(uint64_t) * (degree - 1));
        _vals = (uint64_t*)write_ptr;
        write_ptr += sizeof(uint64_t) * (degree - 1);

        // write the child offsets
        _child_ofs = (uint64_t*)write_ptr;
        memset(write_ptr, 0, sizeof(uint64_t) * degree);
    }

    b_tree_node(const b_tree_node& obj) :
        _p(obj._p),
        _ofs(obj._ofs),
        _mm(),
        _degree(nullptr),
        _leaf(nullptr),
        _num_keys(nullptr),
        _keys(nullptr),
        _vals(nullptr),
        _child_ofs(nullptr)
    {
        _mm = _p.map_page_from(_ofs);

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
        _keys = (KEY_T*)read_ptr;
        read_ptr += sizeof(KEY_T) * (degree() - 1);

        // read the vals
        _vals = (uint64_t*)read_ptr;
        read_ptr += sizeof(uint64_t) * (degree() - 1);

        // read the child offsets
        _child_ofs = (uint64_t*)read_ptr;
    }

    b_tree_node& operator=(const b_tree_node& obj)
    {
        if(this != &obj)
        {
            // Note: we skip copying _p here because it is const and we should never be assiging
            // a node from a different pager.

            _ofs = obj._ofs;
            _mm = _p.map_page_from(_ofs);

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
            _keys = (KEY_T*)read_ptr;
            read_ptr += sizeof(KEY_T) * (degree() - 1);

            // read the vals
            _vals = (uint64_t*)read_ptr;
            read_ptr += sizeof(uint64_t) * (degree() - 1);

            // read the child offsets
            _child_ofs = (uint64_t*)read_ptr;
        }

        return *this;
    }

    inline uint16_t degree() const {return *_degree;}
    inline bool leaf() const {return (*_leaf) != 0;}
    inline void set_leaf(bool l) {*_leaf = l ? 1 : 0;}
    inline uint16_t num_keys() const {return (uint16_t)*_num_keys;}
    inline void set_num_keys(uint16_t n) {*_num_keys = n;}
    inline KEY_T key(uint16_t i) const {return _keys[i];}
    inline void set_key(uint16_t i, KEY_T k) {_keys[i] = k;}
    inline uint64_t val(uint16_t i) const {return _vals[i];}
    inline void set_val(uint16_t i, uint64_t v) {_vals[i] = v;}
    inline uint64_t child_ofs(uint16_t i) const {return _child_ofs[i];}
    inline void set_child_ofs(uint16_t i, uint64_t ofs) {_child_ofs[i] = ofs;}

private:
    template<typename CMP>
    void _insert_non_full(KEY_T k, uint64_t v, CMP cmp)
    {
        // Initialize index as index of rightmost element
        int i = num_keys() - 1;

        // If this is a leaf
        if(leaf())
        {
            // The following loop does two things
            // a) Finds the location of new key to be inserted
            // b) Moves all greater keys to one place ahead
            while(i >= 0 && cmp(key(i), k) > 0)
            {
                set_key(i+1, key(i));
                set_val(i+1, val(i));
                --i;
            }

            // Insert the new key at found location
            set_key(i+1, k);
            set_num_keys(num_keys() + 1);
            set_val(i+1, v);
        }
        else // If this node is not leaf
        {
            // Find the child which is going to have the new key
            while(i >= 0 && cmp(key(i), k) > 0)
                --i;

            auto found_child = b_tree_node(_p, child_ofs(i+1));

            // See if the found child is full
            if(found_child.num_keys() == found_child.degree()-1)
            {
                // If the child is full, then split it
                _split_child(i+1, child_ofs(i+1));

                // After split, the middle key of C[i] goes up and
                // C[i] is splitted into two.  See which of the two
                // is going to have the new key
                if(cmp(key(i+1), k) < 0)
                    ++i;
            }

            auto found_child2 = b_tree_node(_p, child_ofs(i+1));

            found_child2._insert_non_full(k, v, cmp);
        }
    }

    void _split_child(int i, uint64_t ofs)
    {
        // Get the node to be split
        auto y = b_tree_node(_p, ofs);

        // Since we are splitting we need to move half of our keys to the new node.

        // Create a new node which is going to store (y.degree()/2)-1 keys of y
        auto z = b_tree_node(_p, _p.append_page(), y.degree(), y.leaf());

        z.set_num_keys((y.degree()/2)-1);

        // Copy the last (y.degree()/2)-1 keys of y to z
        for(int j = 0; j < (y.degree()/2)-1; ++j)
        {
            z.set_key(j, y.key(j+(y.degree()/2)));
            z.set_val(j, y.val(j+(y.degree()/2)));
        }

        // Copy the last y.degree()/2 children of y to z
        if(!y.leaf())
        {
            for (int j = 0; j < (y.degree()/2); ++j)
                z.set_child_ofs(j, y.child_ofs(j+(y.degree()/2)));
        }

        // Reduce the number of keys in y
        y.set_num_keys((y.degree()/2)-1);

        // Since this node is going to have a new child,
        // create space of new child
        for(int j = num_keys(); j >= i+1; --j)
            set_child_ofs(j+1, child_ofs(j));

        // Link the new child to this node
        set_child_ofs(i+1, z._ofs);

        // A key of y will move to this node. Find location of
        // new key and move all greater keys one space ahead
        for(int j = num_keys()-1; j >= i; --j)
        {
            set_key(j+1, key(j));
            set_val(j+1, val(j));
        }

        // Copy the middle key of y to this node
        set_key(i, y.key((y.degree()/2)-1));
        set_val(i, y.val((y.degree()/2)-1));

        // Increment count of keys in this node
        set_num_keys(num_keys() + 1);
    }

    void _traverse()
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
            std::cout << " " << key(i);
        }

        // Print the subtree rooted with last child
        if(!leaf())
        {
            auto child = b_tree_node(_p, child_ofs(i));
            child._traverse();
        }
    }

    template<typename CMP>
    std::optional<std::pair<b_tree_node, uint16_t>> _search(KEY_T k, CMP cmp) // returns NULL if k is not present.
    {
        // Find the first key greater than or equal to k
        int i = 0;
        while(i < num_keys() && cmp(k, key(i)) == 1)
            ++i;

        // If the found key is equal to k, return this node
        if(cmp(key(i), k) == 0)
            return std::make_pair(*this, i);
    
        // If key is not found here and this is a leaf node
        if(leaf())
            return std::optional<std::pair<b_tree_node, uint16_t>>();

        // Go to the appropriate child
        return b_tree_node(_p, child_ofs(i))._search(k, cmp);
    }

    const pager& _p;
    uint64_t _ofs;
    r_memory_map _mm;
    uint16_t* _degree;
    uint16_t* _leaf;
    uint16_t* _num_keys;
    KEY_T* _keys;
    uint64_t* _vals;
    uint64_t* _child_ofs;
};

#endif
