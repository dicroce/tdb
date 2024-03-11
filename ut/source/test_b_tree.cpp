
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

template<typename T>
bool has_all_keys(T& t, const vector<int64_t>& keys)
{
    for(auto k : keys)
    {
        uint64_t v;
        if(!t.search(k))
            return false;
    }
    return true;
}

template<typename T>
void insert_all(T& t, const vector<int64_t>& keys)
{
    for(size_t i = 0; i < keys.size(); ++i)
        t.insert(keys[i], keys[i]+100);
}

void test_b_tree::setup()
{
    b_tree::create_db_file("test.db");
    b_tree t("test.db", 4);

    std::iota(begin(test_keys), end(test_keys), 0);
    std::iota(begin(test_vals), end(test_vals), test_keys.size());

    auto rng = std::default_random_engine {};
    std::shuffle(begin(test_keys), end(test_keys), rng);
    std::shuffle(begin(test_vals), end(test_vals), rng);    

    insert_all(t, test_keys);
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

void test_b_tree::test_duplicate_key_insert()
{
    b_tree t("test.db", 4);

    RTF_ASSERT_THROWS(t.insert(99, 99), std::runtime_error);
}

void test_b_tree::test_dot_file()
{
    b_tree t("test.db", 4);

    t.write_dot_file("dotfile.txt");
}

void test_b_tree::test_basic_remove()
{
    b_tree::create_db_file("test_basic_remove.db");
    b_tree t("test_basic_remove.db", 4);

    insert_all(t, {10, 20, 30, 40, 50, 60, 70, 80, 90, 100});

    t.remove(30);

    RTF_ASSERT(!t.search(30));
    RTF_ASSERT(has_all_keys(t, {10, 20, 40, 50, 60, 70, 80, 90, 100}));

    unlink("test_basic_remove.db");
}

void test_b_tree::test_lots_of_inserts_and_removes()
{
    b_tree t("test.db", 4);

    auto keys = test_keys;

    while(!keys.empty())
    {
        RTF_ASSERT(has_all_keys(t, keys));

        auto k = keys.back();
        keys.pop_back();
        RTF_ASSERT(t.search(k));
        t.remove(k);
        RTF_ASSERT(!t.search(k));
    }
}
