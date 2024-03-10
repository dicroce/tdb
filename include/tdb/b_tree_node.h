
#ifndef __b_tree_node_h
#define __b_tree_node_h

#include "tdb/pager.h"
#include "tdb/file_utils.h"
#include <map>
#include <optional>
#include <tuple>

class b_tree_node
{
friend class b_tree;
public:
    b_tree_node(const pager& p, uint16_t min_degree, bool leaf);
    b_tree_node(const pager& p, int64_t ofs);

    void insert_non_full(int64_t k, int64_t v);

    void split_child(int i, int64_t ofs);

    void traverse();

    std::optional<int64_t> search(int64_t k);

    uint16_t min_degree() const {return *_min_degree;}
    bool leaf() const {return (*_leaf) != 0;}
    uint16_t num_keys() const {return *_num_keys;}
    void set_num_keys(uint16_t n) {*_num_keys = n;}
    int64_t key(uint16_t i) const {return _keys[i];}
    void set_key(uint16_t i, int64_t k) {_keys[i] = k;}
    int64_t val(uint16_t i) const {return _vals[i];}
    void set_val(uint16_t i, int64_t v) {_vals[i] = v;}
    int64_t child_ofs(uint16_t i) const {return _child_ofs[i];}
    void set_child_ofs(uint16_t i, int64_t ofs) {_child_ofs[i] = ofs;}
    bool full() const {return num_keys() == 2*min_degree() - 1;}

    int64_t ofs() const {return _ofs;}

private:
    const pager& _p;
    int64_t _ofs;
    r_memory_map _mm;
    uint16_t* _min_degree;
    uint16_t* _leaf;
    uint16_t* _num_keys;
    int64_t* _keys;
    int64_t* _vals;
    int64_t* _child_ofs;
};

#endif
