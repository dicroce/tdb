
#ifndef __b_tree_node_h
#define __b_tree_node_h

#include "tdb/pager.h"
#include "tdb/file_utils.h"

class b_tree_node
{
    friend class b_tree;

public:
    b_tree_node(const pager& p);
    b_tree_node(const pager& p, uint64_t ofs);
    b_tree_node(const pager& p, uint64_t ofs, int degree, bool leaf);

    inline bool is_empty() const {return _is_empty;}
    inline uint16_t degree() const {return *_degree;}
    inline bool leaf() const {return (*_leaf) != 0;}
    inline void set_leaf(bool l) {*_leaf = l ? 1 : 0;}
    inline int num_keys() const {return (int)*_num_keys;}
    inline void set_num_keys(uint16_t n) {*_num_keys = n;}
    inline int64_t key(uint16_t i) const {return _keys[i];}
    inline void set_key(uint16_t i, int64_t k) {_keys[i] = k;}
    inline uint64_t child_ofs(uint16_t i) const {return _child_ofs[i];}
    inline void set_child_ofs(uint16_t i, uint64_t ofs) {_child_ofs[i] = ofs;}

private:
    void _insert_non_full(int64_t k);
    void _split_child(int i, uint64_t ofs);    
    void _traverse();
    b_tree_node _search(int k);   // returns NULL if k is not present.

    const pager& _p;
    bool _is_empty;
    uint64_t _ofs;
    r_memory_map _mm;
    uint16_t* _degree;
    uint16_t* _leaf;
    uint16_t* _num_keys;
    int64_t* _keys;
    uint64_t* _child_ofs;
};

#endif
