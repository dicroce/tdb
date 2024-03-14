
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
    b_tree_node(const b_tree_node& obj); // Object copy constructor, non deep copy
    b_tree_node(const pager& p, uint16_t min_degree, bool leaf);
    b_tree_node(const pager& p, int64_t ofs);

private:
    int64_t _ofs() const {return _ofs_field;}
    uint16_t _min_degree() const {return *_min_degree_field;}
    bool _leaf() const {return (*_leaf_field) != 0;}
    uint16_t _num_keys() const {return *_num_keys_field;}
    void _set_num_keys(uint16_t n) {*_num_keys_field = n;}
    int64_t _key(uint16_t i) const {return _keys_field[i];}
    void _set_key(uint16_t i, int64_t k) {_keys_field[i] = k;}
    bool _valid_key(uint16_t i) const {return (_valid_keys_field[i] == 0)?false:true;}
    void _set_valid_key(uint16_t i, bool v) {_valid_keys_field[i] = (v)?1:0;}
    int64_t _val(uint16_t i) const {return _vals_field[i];}
    void _set_val(uint16_t i, int64_t v) {_vals_field[i] = v;}
    int64_t _child_ofs(uint16_t i) const {return _child_ofs_field[i];}
    void _set_child_ofs(uint16_t i, int64_t ofs) {_child_ofs_field[i] = ofs;}
    bool _full() const {return _num_keys() == 2*_min_degree() - 1;}

    void _insert_non_full(int64_t k, int64_t v);
    void _split_child(int i, int64_t ofs);
    std::optional<int64_t> _search(int64_t k);
    void _remove(int64_t k);

    const pager& _p;
    int64_t _ofs_field;
    r_memory_map _mm;
    uint16_t* _min_degree_field;
    uint16_t* _leaf_field;
    uint16_t* _num_keys_field;
    int64_t* _keys_field;
    uint8_t* _valid_keys_field;
    int64_t* _vals_field;
    int64_t* _child_ofs_field;
};

#endif
