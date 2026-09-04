#include "tdb/b_tree.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <functional>
#include <queue>
#include <stdexcept>
#include <vector>

using namespace std;

namespace
{
constexpr uint32_t node_magic = 0x45444f4e; // "NODE"
constexpr uint8_t leaf_kind = 1;
constexpr uint8_t internal_kind = 2;
constexpr size_t node_header_size = 16;
constexpr size_t leaf_capacity = (pager::page_size - node_header_size) / 16;
constexpr size_t internal_capacity = (pager::page_size - node_header_size - 8) / 16;

struct node
{
    bool leaf = true;
    uint64_t next_leaf = 0;
    vector<int64_t> keys;
    vector<int64_t> values;
    vector<uint64_t> children;
};

template<typename T>
T read_scalar(const pager::page& page, size_t& offset)
{
    T result{};
    memcpy(&result, page.data() + offset, sizeof(result));
    offset += sizeof(result);
    return result;
}

template<typename T>
void write_scalar(pager::page& page, size_t& offset, T value)
{
    memcpy(page.data() + offset, &value, sizeof(value));
    offset += sizeof(value);
}

node decode_node(const pager::page& page)
{
    size_t offset = 0;
    if (read_scalar<uint32_t>(page, offset) != node_magic)
        throw runtime_error("Invalid B+tree node page.");
    const uint8_t kind = read_scalar<uint8_t>(page, offset);
    (void)read_scalar<uint8_t>(page, offset);
    const uint16_t count = read_scalar<uint16_t>(page, offset);
    node result;
    result.leaf = kind == leaf_kind;
    if (!result.leaf && kind != internal_kind)
        throw runtime_error("Invalid B+tree node kind.");
    result.next_leaf = read_scalar<uint64_t>(page, offset);
    const size_t capacity = result.leaf ? leaf_capacity : internal_capacity;
    if (count > capacity) throw runtime_error("Corrupt B+tree node key count.");

    if (result.leaf)
    {
        result.keys.reserve(count);
        result.values.reserve(count);
        for (uint16_t i = 0; i < count; ++i)
        {
            result.keys.push_back(read_scalar<int64_t>(page, offset));
            result.values.push_back(read_scalar<int64_t>(page, offset));
        }
    }
    else
    {
        result.children.reserve(count + 1);
        result.keys.reserve(count);
        result.children.push_back(read_scalar<uint64_t>(page, offset));
        for (uint16_t i = 0; i < count; ++i)
        {
            result.keys.push_back(read_scalar<int64_t>(page, offset));
            result.children.push_back(read_scalar<uint64_t>(page, offset));
        }
        if (result.children.size() != result.keys.size() + 1)
            throw runtime_error("Corrupt B+tree internal node.");
    }
    return result;
}

void encode_node(pager::page& page, const node& value)
{
    if (value.keys.size() > (value.leaf ? leaf_capacity : internal_capacity))
        throw runtime_error("B+tree node exceeds physical page capacity.");
    if (value.leaf && value.values.size() != value.keys.size())
        throw runtime_error("Leaf key/value counts differ.");
    if (!value.leaf && value.children.size() != value.keys.size() + 1)
        throw runtime_error("Internal child count is invalid.");

    page.fill(0);
    size_t offset = 0;
    write_scalar<uint32_t>(page, offset, node_magic);
    write_scalar<uint8_t>(page, offset, value.leaf ? leaf_kind : internal_kind);
    write_scalar<uint8_t>(page, offset, 0);
    write_scalar<uint16_t>(page, offset, static_cast<uint16_t>(value.keys.size()));
    write_scalar<uint64_t>(page, offset, value.next_leaf);
    if (value.leaf)
    {
        for (size_t i = 0; i < value.keys.size(); ++i)
        {
            write_scalar<int64_t>(page, offset, value.keys[i]);
            write_scalar<int64_t>(page, offset, value.values[i]);
        }
    }
    else
    {
        write_scalar<uint64_t>(page, offset, value.children[0]);
        for (size_t i = 0; i < value.keys.size(); ++i)
        {
            write_scalar<int64_t>(page, offset, value.keys[i]);
            write_scalar<uint64_t>(page, offset, value.children[i + 1]);
        }
    }
}

struct split_result
{
    int64_t separator;
    uint64_t right_page;
};
}

shared_mutex b_tree::_tree_mutex;

b_tree::b_tree(const string& file_name, uint16_t min_degree) :
    _pager(file_name),
    _max_keys(0)
{
    if (min_degree < 2)
        throw invalid_argument("A B+tree min_degree must be at least 2.");
    const size_t configured_max_keys = 2 * static_cast<size_t>(min_degree) - 1;
    if (configured_max_keys > min(leaf_capacity, internal_capacity))
        throw invalid_argument("min_degree exceeds the 4 KiB page capacity.");
    _max_keys = static_cast<uint16_t>(configured_max_keys);
}

void b_tree::insert(int64_t key, int64_t value)
{
    unique_lock lock(_tree_mutex);
    auto tx = _pager.begin_transaction();

    if (tx.root_page() == 0)
    {
        const uint64_t root_page = tx.allocate();
        node root;
        root.keys.push_back(key);
        root.values.push_back(value);
        encode_node(tx.write(root_page), root);
        tx.set_root_page(root_page);
        tx.commit();
        return;
    }

    function<optional<split_result>(uint64_t)> insert_recursive;
    insert_recursive = [&](uint64_t page_number) -> optional<split_result>
    {
        node current = decode_node(tx.read(page_number));
        if (current.leaf)
        {
            auto position = lower_bound(current.keys.begin(), current.keys.end(), key);
            const size_t index = static_cast<size_t>(position - current.keys.begin());
            if (position != current.keys.end() && *position == key)
                throw runtime_error("Duplicate key");
            current.keys.insert(position, key);
            current.values.insert(current.values.begin() + index, value);
            if (current.keys.size() <= _max_keys)
            {
                encode_node(tx.write(page_number), current);
                return nullopt;
            }

            const size_t middle = current.keys.size() / 2;
            node right;
            right.leaf = true;
            right.next_leaf = current.next_leaf;
            right.keys.assign(current.keys.begin() + middle, current.keys.end());
            right.values.assign(current.values.begin() + middle, current.values.end());
            current.keys.erase(current.keys.begin() + middle, current.keys.end());
            current.values.erase(current.values.begin() + middle, current.values.end());
            const uint64_t right_page = tx.allocate();
            current.next_leaf = right_page;
            encode_node(tx.write(page_number), current);
            encode_node(tx.write(right_page), right);
            return split_result{right.keys.front(), right_page};
        }

        const size_t child_index = upper_bound(current.keys.begin(), current.keys.end(), key)
            - current.keys.begin();
        auto child_split = insert_recursive(current.children[child_index]);
        if (!child_split) return nullopt;
        current.keys.insert(current.keys.begin() + child_index, child_split->separator);
        current.children.insert(current.children.begin() + child_index + 1,
                                child_split->right_page);
        if (current.keys.size() <= _max_keys)
        {
            encode_node(tx.write(page_number), current);
            return nullopt;
        }

        const size_t middle = current.keys.size() / 2;
        const int64_t separator = current.keys[middle];
        node right;
        right.leaf = false;
        right.keys.assign(current.keys.begin() + middle + 1, current.keys.end());
        right.children.assign(current.children.begin() + middle + 1, current.children.end());
        current.keys.erase(current.keys.begin() + middle, current.keys.end());
        current.children.erase(current.children.begin() + middle + 1, current.children.end());
        const uint64_t right_page = tx.allocate();
        encode_node(tx.write(page_number), current);
        encode_node(tx.write(right_page), right);
        return split_result{separator, right_page};
    };

    const uint64_t old_root = tx.root_page();
    auto root_split = insert_recursive(old_root);
    if (root_split)
    {
        node root;
        root.leaf = false;
        root.keys.push_back(root_split->separator);
        root.children = {old_root, root_split->right_page};
        const uint64_t new_root = tx.allocate();
        encode_node(tx.write(new_root), root);
        tx.set_root_page(new_root);
    }
    tx.commit();
}

optional<int64_t> b_tree::search(int64_t key)
{
    shared_lock lock(_tree_mutex);
    uint64_t page_number = _pager.root_page();
    while (page_number != 0)
    {
        node current = decode_node(_pager.read(page_number));
        if (current.leaf)
        {
            auto position = lower_bound(current.keys.begin(), current.keys.end(), key);
            if (position == current.keys.end() || *position != key) return nullopt;
            return current.values[static_cast<size_t>(position - current.keys.begin())];
        }
        const size_t child_index = upper_bound(current.keys.begin(), current.keys.end(), key)
            - current.keys.begin();
        page_number = current.children[child_index];
    }
    return nullopt;
}

void b_tree::remove(int64_t key)
{
    unique_lock lock(_tree_mutex);
    auto tx = _pager.begin_transaction();
    uint64_t page_number = tx.root_page();
    if (page_number == 0) return;

    while (true)
    {
        node current = decode_node(tx.read(page_number));
        if (current.leaf)
        {
            auto position = lower_bound(current.keys.begin(), current.keys.end(), key);
            if (position == current.keys.end() || *position != key) return;
            const size_t index = static_cast<size_t>(position - current.keys.begin());
            current.keys.erase(position);
            current.values.erase(current.values.begin() + index);
            encode_node(tx.write(page_number), current);
            tx.commit();
            return;
        }
        const size_t child_index = upper_bound(current.keys.begin(), current.keys.end(), key)
            - current.keys.begin();
        page_number = current.children[child_index];
    }
}

void b_tree::write_dot_file(const string& file_name)
{
    shared_lock lock(_tree_mutex);
    ofstream file(file_name);
    if (!file) throw runtime_error("Unable to open DOT output file.");
    file << "digraph BPlusTree {\n  node [shape=record];\n";
    const uint64_t root = _pager.root_page();
    if (root == 0) { file << "}\n"; return; }

    queue<uint64_t> pending;
    pending.push(root);
    while (!pending.empty())
    {
        const uint64_t page_number = pending.front();
        pending.pop();
        node current = decode_node(_pager.read(page_number));
        file << "  node" << page_number << " [label=\"";
        for (size_t i = 0; i < current.keys.size(); ++i)
        {
            if (i) file << '|';
            file << current.keys[i];
            if (current.leaf) file << " (" << current.values[i] << ')';
        }
        file << "\"];\n";
        if (!current.leaf)
        {
            for (uint64_t child : current.children)
            {
                file << "  node" << page_number << " -> node" << child << ";\n";
                pending.push(child);
            }
        }
        else if (current.next_leaf != 0)
            file << "  node" << page_number << " -> node" << current.next_leaf
                 << " [style=dashed,color=gray];\n";
    }
    file << "}\n";
}

void b_tree::create_db_file(const string& file_name) { pager::create(file_name); }
void b_tree::vacuum(const string&) { throw logic_error("vacuum is not implemented"); }
