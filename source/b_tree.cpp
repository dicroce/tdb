#include "tdb/b_tree.h"

using namespace std;

b_tree::b_tree(const string& file_name) :
    _file_name(file_name),
    _p(file_name),
    _degree(4)
{
}

void b_tree::create_db_file(const string& file_name)
{
    pager::create(file_name);
}

void b_tree::insert(int64_t k, uint64_t v)
{
    auto root_ofs = _p.root_ofs();
    if(root_ofs == 0)
    {
        auto new_root_ofs = _p.append_page();
        b_tree_node new_root_node(_p, new_root_ofs, _degree, true);
        new_root_node.set_key(0, k);
        new_root_node.set_val(0, v);
        new_root_node.set_num_keys(1);

        _p.set_root_ofs(new_root_ofs);
    }
    else
    {
        auto old_root = b_tree_node(_p, root_ofs);
        if(old_root.full())
        {
            auto new_root_ofs = _p.append_page();
            b_tree_node new_root_node(_p, new_root_ofs, _degree, false);
            new_root_node.set_child_ofs(0, root_ofs);
            new_root_node._split_child(0, root_ofs);

            // New root has two _children now.  Decide which of the
            // two _children is going to have new key
            int i = 0;
            if(new_root_node.key(i) < k)
                ++i;
            auto new_child = b_tree_node(_p, new_root_node.child_ofs(i));
            new_child._insert_non_full(k, v);

            // Change root
            _p.set_root_ofs(new_root_node.ofs());
        }
        else
        {
            old_root._insert_non_full(k, v);
        }
    }
}

void b_tree::remove(int64_t k)
{
    b_tree_node root(_p, _p.root_ofs());
    auto search_result = root._search(k, 0xFFFFFFFFFFFFFFFF);

    if(!search_result)
        return;

    auto found_node = std::get<0>(*search_result);
    auto idx = std::get<1>(*search_result);
    auto parent_ofs = std::get<2>(*search_result);

    auto new_ofs = _p.append_page();
    b_tree_node new_node(_p, new_ofs, found_node.degree(), found_node.leaf());

    // Copy the keys, values, and child offsets from the old node to the new node,
    // skipping the key to be deleted
    int n_copied = 0;
    for (int i = 0, j = 0; i < found_node.num_keys(); ++i) {
        if (i != idx) {
            new_node.set_key(j, found_node.key(i));
            new_node.set_val(j, found_node.val(i));
            ++j;
            ++n_copied;
        }
    }
    new_node.set_num_keys(n_copied);

    for (int i = 0, j = 0; i < found_node.num_keys()+1; ++i) {
        if (i != idx+1) {
            new_node.set_child_ofs(j, found_node.child_ofs(i));
            ++j;
        }
    }

    if(parent_ofs != 0xFFFFFFFFFFFFFFFF)
    {
        b_tree_node parent(_p, parent_ofs);
        // note: btree nodes have one more child than keys because they have both less and greater than keys for each child.
        for (int i = 0; i < parent.num_keys()+1; ++i) {
            if (parent.child_ofs(i) == found_node.ofs()) {
                parent.set_child_ofs(i, new_node.ofs());
                break;
            }
        }
    }
    else
    {
        _p.set_root_ofs(new_node.ofs());
    }
}

optional<uint64_t> b_tree::search(int64_t k)
{
    auto root_ofs = _p.root_ofs();
    if(root_ofs == 0)
        throw runtime_error("empty tree");
    b_tree_node root_node(_p, root_ofs);
    auto search_results = root_node._search(k, 0xFFFFFFFFFFFFFFFF);
    optional<uint64_t> result;
    if(search_results)
        result = std::get<0>(*search_results).val(std::get<1>(*search_results));

    return result;
}

void b_tree::render_to_dot_file(const string& file_name)
{
    auto root_ofs = _p.root_ofs();

    string tree;
    tree = "digraph G {\n";
    _build_dot_tree(root_ofs, tree);
    tree += "}\n";
    shared_ptr<FILE> fp(fopen(file_name.c_str(), "w"), fclose);
    fprintf(fp.get(), "%s", tree.c_str());
}

void b_tree::traverse()
{
    auto root_ofs = _p.root_ofs();
    if(root_ofs == 0)
        throw runtime_error("empty tree");
    b_tree_node root_node(_p, root_ofs);
    root_node._traverse();
}

void b_tree::_build_dot_tree(uint64_t root_ofs, string& tree, int nodeNum)
{
    auto root = b_tree_node(_p, root_ofs);

    // Print the root node
    tree += "node" + to_string(nodeNum) + " [label=\"";
    for(int i = 0; i < root.num_keys(); ++i)
        tree += to_string(root.key(i)) + " ";
    tree += "ofs=" + to_string(root_ofs) + " ";
    tree += "\"]\n";

    // Print the child nodes
    int childNum = 1;
    for(int i = 0; i < root.num_keys()+1; ++i)
    {
        if(root.child_ofs(i) != 0)
        {
            tree += "node" + to_string(nodeNum) + " -> node" + to_string(nodeNum*10+childNum) + "\n";
            _build_dot_tree(root.child_ofs(i), tree, nodeNum*10+childNum);
            ++childNum;
        }
    }
}
