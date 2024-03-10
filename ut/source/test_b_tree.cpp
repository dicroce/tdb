
#include "test_b_tree.h"
#include "tdb/b_tree.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <queue>
#include <cstdint>

using namespace std;

REGISTER_TEST_FIXTURE(test_b_tree);

std::vector<int64_t> test_keys(100);
std::vector<uint64_t> test_vals(100);

#if 0
class btree_node2
{
friend class btree2;
public:
    btree_node2(const pager& p, uint16_t min_degree, bool leaf);
    btree_node2(const pager& p, int64_t ofs);

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

btree_node2::btree_node2(const pager& p, uint16_t min_degree, bool leaf) :
    _p(p),
    _ofs(_p.append_page()),
    _mm(_p.map_page_from(_ofs)),
    _min_degree(nullptr),
    _leaf(nullptr),
    _num_keys(nullptr),
    _keys(nullptr),
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
    read_ptr += sizeof(uint16_t);

    _keys = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree * 2) - 1);

    _vals = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree * 2) - 1);

    _child_ofs = (int64_t*)read_ptr;
}

btree_node2::btree_node2(const pager& p, int64_t ofs) :
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

    _vals = (int64_t*)read_ptr;
    read_ptr += sizeof(int64_t) * ((*_min_degree * 2) - 1);

    _child_ofs = (int64_t*)read_ptr;
}

void btree_node2::insert_non_full(int64_t k, int64_t v)
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
            _vals[i+1] = _vals[i];
            i--;
        }
 
        // Insert the new key at found location
        _keys[i+1] = k;
        _vals[i+1] = v;
        set_num_keys(num_keys() + 1);
    }
    else // If this node is not leaf
    {
        // Find the child which is going to have the new key
        while (i >= 0 && _keys[i] > k)
            i--;
 
        btree_node2 child(_p, child_ofs(i+1));
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

        btree_node2 new_child(_p, _child_ofs[i+1]);
        new_child.insert_non_full(k, v);
    }
}

void btree_node2::split_child(int i, int64_t ofs)
{
    btree_node2 y(_p, ofs);

    // Create a new node which is going to store (t-1) keys
    // of y
    btree_node2 z(_p, y.min_degree(), y.leaf());
    z.set_num_keys(min_degree() - 1);
 
    // Copy the last (min_degree-1) keys of y to z
    for (int j = 0; j < min_degree()-1; j++)
    {
        z._keys[j] = y._keys[j+min_degree()];
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
        _vals[j+1] = _vals[j];
    }
 
    // Copy the middle key of y to this node
    _keys[i] = y._keys[min_degree()-1];
    _vals[i] = y._vals[min_degree()-1];
 
    // Increment count of keys in this node
    set_num_keys(num_keys() + 1);
}

void btree_node2::traverse()
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
            btree_node2 child(_p, child_ofs(i));
            child.traverse();
        }
        cout << " " << _keys[i];
    }
 
    // Print the subtree rooted with last child
    if (leaf() == false)
    {
        btree_node2 child(_p, child_ofs(i));
        child.traverse();
    }
}

optional<int64_t> btree_node2::search(int64_t k)
{
    // Find the first key greater than or equal to k
    int i = 0;
    while (i < num_keys() && k > _keys[i])
        i++;
 
    optional<int64_t> result;
    // If the found key is equal to k, return this node
    if (_keys[i] == k)
    {
        result = _vals[i];
        return result;
    }
 
    // If key is not found here and this is a leaf node
    if (leaf() == true)
        return result;
 
    // Go to the appropriate child
    btree_node2 child(_p, child_ofs(i));
    return child.search(k);
}

class btree2
{
public:
    // Constructor (Initializes tree as empty)
    btree2(const string& file_name, uint16_t min_degree) :
        _p(file_name),
        _root(_p.root_ofs()),
        _min_degree(min_degree)
    {        
    }
 
    // function to traverse the tree
    void traverse()
    {
        if (_root != 0)
        {
            btree_node2 root(_p, _root);
            root.traverse();
        }
    }
 
    // function to search a key in this tree
    std::optional<int64_t> search(int64_t k)
    {
        std::optional<int64_t> result;
        if (_root != 0)
        {
            btree_node2 root(_p, _root);
            result = root.search(k);
        }
        return result;
    }
 
    // The main function that inserts a new key in this B-Tree
    void insert(int64_t k, int64_t v);

    // Function to write the B-tree to a Graphviz DOT file
    void write_dot_file(const string& file_name);

private:
    pager _p;
    int64_t _root; // Pointer to root node
    uint16_t _min_degree;  // Minimum degree
};

void btree2::insert(int64_t k, int64_t v)
{
    // If tree is empty
    if (_root == 0)
    {
        // Allocate memory for root
        btree_node2 root(_p, _min_degree, true);
        _root = root.ofs();
        _p.set_root_ofs(_root);

        root.set_num_keys(1);  // Update number of keys in root
        root.set_key(0, k);  // Insert key
        root.set_val(0, v);  // Insert value
    }
    else // If tree is not empty
    {
        // If root is full, then tree grows in height
        btree_node2 root(_p, _root);
        if(root.full())
        {            
            btree_node2 s(_p, _min_degree, false);
            s.set_child_ofs(0, _root);
            s.split_child(0, _root);

            int i = 0;
            if(s.key(i) < k)
                i++;
            btree_node2 new_child(_p, s.child_ofs(i));
            new_child.insert_non_full(k, v);
            _root = s.ofs();
            _p.set_root_ofs(_root);
        }
        else root.insert_non_full(k, v);
    }
}

void btree2::write_dot_file(const string& file_name)
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
        btree_node2 node(_p, q.front().first);
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
#endif








void test_b_tree::setup()
{
    b_tree::create_db_file("test.db");
    b_tree t("test.db", 4);

    std::iota(begin(test_keys), end(test_keys), 0);
    std::iota(begin(test_vals), end(test_vals), test_keys.size());

//    auto rng = std::default_random_engine {};
//    std::shuffle(begin(test_keys), end(test_keys), rng);
//    std::shuffle(begin(test_vals), end(test_vals), rng);    

    for(auto k : test_keys)
    {
        printf("t.insert(%ld, %ld)\n", k, k + 100);
        fflush(stdout);
        t.insert(k, k + 100);
    }
}

void test_b_tree::teardown()
{
    unlink("test.db");
    unlink("dotfile.txt");
}

void test_b_tree::test_basic()
{
    b_tree t("test.db", 4);

    RTF_ASSERT(t.search(47) == 147);
    RTF_ASSERT(t.search(0) == 100);
    RTF_ASSERT(t.search(99) == 199);
}

void test_b_tree::test_dot_file()
{
    b_tree t("test.db", 4);

    t.write_dot_file("dotfile.txt");
}

template<typename T>
bool has_all_keys(T& t, const vector<int64_t>& keys)
{
    for(auto k : keys)
    {
        uint64_t v;
        if(!t.search(k))
        {
            printf("missing key %ld\n", k);
            t.write_dot_file("missing_key.dot");
            return false;
        }
    }
    return true;
}

template<typename T>
void insert_all(T& t, const vector<int64_t>& keys)
{
    for(size_t i = 0; i < keys.size(); ++i)
        t.insert(keys[i], keys[i]);
}

void test_b_tree::test_basic_remove()
{
#if 0
    b_tree::create_db_file("test_basic_remove.db");
    b_tree t("test_basic_remove.db");

    insert_all(t, {10, 20, 30, 40, 50, 60, 70, 80, 90, 100});

    t.remove(30);

    RTF_ASSERT(!t.search(30));
    RTF_ASSERT(has_all_keys(t, {10, 20, 40, 50, 60, 70, 80, 90, 100}));

    unlink("test_basic_remove.db");
#endif
}

void test_b_tree::test_lots_of_inserts_and_removes()
{
#if 0
    b_tree::create_db_file("tmp.db");
    b_tree t("tmp.db");

    insert_all(t, {10, 20, 30, 40});
    t.render_to_dot_file("0.dot");

    t.insert(35, 35);
    t.render_to_dot_file("1.dot");

    t.insert(36, 36);
    t.render_to_dot_file("2.dot");

    t.insert(37, 37);
    t.render_to_dot_file("3.dot");

    t.insert(38, 38);
    t.render_to_dot_file("4.dot");

    t.insert(39, 39);
    t.render_to_dot_file("5.dot");

    t.insert(40, 40);
    t.render_to_dot_file("6.dot");
#endif
#if 0
    b_tree t("test.db");

    auto keys = test_keys;

    insert_all(t, keys);

    RTF_ASSERT(has_all_keys(t, keys));

    while(!keys.empty())
    {
        auto k = keys.back();
        keys.pop_back();
        RTF_ASSERT(t.search(k));
        if(k==99)
            t.render_to_dot_file("before_remove.dot");
        t.remove(k);
        if(k==99)
            t.render_to_dot_file("after_remove.dot");
        printf("k=%ld\n", k);
        RTF_ASSERT(!t.search(k));
        printf("removed %ld\n", k);
        fflush(stdout);
        RTF_ASSERT(has_all_keys(t, keys));
    }
#endif

    pager::create("test.db");
    b_tree t("test.db", 3);

    auto keys = test_keys;

    insert_all(t, keys);

    RTF_ASSERT(has_all_keys(t, keys));

    t.insert(100,100);

    t.write_dot_file("before_remove.dot");



#if 0
    b_tree t(2);

    auto keys = test_keys;

    insert_all(t, keys);

    RTF_ASSERT(has_all_keys(t, keys));

    t.insert(100,100);

    t.write_dot_file("before_remove.dot");
#endif
#if 0
    while(!keys.empty())
    {
        auto k = keys.back();
        keys.pop_back();
        uint64_t v;
        RTF_ASSERT(t.find(k,v));
        if(k==99)
            t.render_to_dot_file("before_remove.dot");
        t.remove(k);
        if(k==99)
            t.render_to_dot_file("after_remove.dot");
        printf("k=%ld\n", k);
        RTF_ASSERT(!t.search(k));
        printf("removed %ld\n", k);
        fflush(stdout);
        RTF_ASSERT(has_all_keys(t, keys));
    }
#endif
}
