
#ifndef __b_tree_h
#define __b_tree_h

#include "tdb/b_tree_node.h"
#include <string>
#include <stdexcept>
#include <memory>

class b_tree
{
public:
    b_tree(const std::string& file_name);
    void traverse();
    b_tree_node search(int64_t k);
    void insert(int64_t k, uint64_t v);
    void render_to_dot_file(const std::string& file_name);

private:
    void _build_dot_tree(uint64_t root_ofs, std::string& tree, int nodeNum=1);

    std::string _file_name;
    pager _p;
    int _degree;
};

#endif