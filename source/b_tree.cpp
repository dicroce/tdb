#include "tdb/b_tree.h"
#include <iostream>
#include <fstream>
#include <queue>

using namespace std;

void b_tree::insert(int64_t k, int64_t v)
{
    // If tree is empty
    if (_root == 0)
    {
        // Allocate memory for root
        b_tree_node root(_p, _min_degree, true);
        _root = root.ofs();
        _p.set_root_ofs(_root);

        root.set_num_keys(1);  // Update number of keys in root
        root.set_key(0, k);  // Insert key
        root.set_val(0, v);  // Insert value
    }
    else // If tree is not empty
    {
        // If root is full, then tree grows in height
        b_tree_node root(_p, _root);
        if(root.full())
        {            
            b_tree_node s(_p, _min_degree, false);
            s.set_child_ofs(0, _root);
            s.split_child(0, _root);

            int i = 0;
            if(s.key(i) < k)
                i++;
            b_tree_node new_child(_p, s.child_ofs(i));
            new_child.insert_non_full(k, v);
            _root = s.ofs();
            _p.set_root_ofs(_root);
        }
        else root.insert_non_full(k, v);
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
    q.push({_root, node_id++});

    while (!q.empty())
    {
        b_tree_node node(_p, q.front().first);
        int parent_id = q.front().second;
        q.pop();

        int current_id = node_id++;

        file << "  node" << current_id << " [label=\"";
        for (int i = 0; i < node.num_keys(); i++)
        {
            if (i > 0)
                file << "|";
            file << "<f" << i << "> " << node.key(i) << " (" << node.val(i) << ")";
        }
        file << "\"];\n";

        if (parent_id != -1)
        {
            file << "  node" << parent_id << " -> node" << current_id << ";\n";
        }

        if (!node.leaf())
        {
            for (int i = 0; i <= node.num_keys(); i++)
            {
                q.push({node.child_ofs(i), current_id});
            }
        }
    }

    file << "}\n";
    file.close();
}
