
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
#include <thread>
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

void test_b_tree::test_CAS()
{
    uint64_t counter = 41;

    // Make sure CAS update on new value (where compval==*ptr) works
    auto result = __sync_bool_compare_and_swap(&counter, 41, 42);
    RTF_ASSERT(result);

    // Make sure CAS update on same value (where compval==*ptr) works
    result = __sync_bool_compare_and_swap(&counter, 42, 42);
    RTF_ASSERT(result);

    // Make sure CAS update on different value (where compval!=*ptr) works
    result = __sync_bool_compare_and_swap(&counter, 43, 44);
    RTF_ASSERT(!result);
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

void test_b_tree::test_insert_ascending_order()
{
    b_tree::create_db_file("test_insert_ascending_order.db");
    b_tree t("test_insert_ascending_order.db", 4);

    std::vector<int64_t> keys(100);
    std::iota(begin(keys), end(keys), 1);

    insert_all(t, keys);

    RTF_ASSERT(has_all_keys(t, keys));

    unlink("test_insert_ascending_order.db");
}

void test_b_tree::test_insert_descending_order()
{
    b_tree::create_db_file("test_insert_descending_order.db");
    b_tree t("test_insert_descending_order.db", 4);

    std::vector<int64_t> keys(100);
    std::iota(rbegin(keys), rend(keys), 1);

    insert_all(t, keys);

    RTF_ASSERT(has_all_keys(t, keys));

    unlink("test_insert_descending_order.db");
}

void test_b_tree::test_remove_random_order()
{
    b_tree::create_db_file("test_remove_random_order.db");
    b_tree t("test_remove_random_order.db", 4);

    std::vector<int64_t> keys(100);
    std::iota(begin(keys), end(keys), 1);
    std::shuffle(begin(keys), end(keys), std::default_random_engine{});

    insert_all(t, keys);

    std::shuffle(begin(keys), end(keys), std::default_random_engine{});

    for (auto k : keys) {
        RTF_ASSERT(t.search(k));
        t.remove(k);
        RTF_ASSERT(!t.search(k));
    }

    unlink("test_remove_random_order.db");
}

void test_b_tree::test_search_non_existent_keys()
{
    b_tree t("test.db", 4);

    std::vector<int64_t> non_existent_keys = {-10, -5, 200, 500};

    for (auto k : non_existent_keys) {
        RTF_ASSERT(!t.search(k));
    }
}

void test_b_tree::test_large_number_of_keys()
{
    b_tree::create_db_file("test_large_number_of_keys.db");
    b_tree t("test_large_number_of_keys.db", 4);

    std::vector<int64_t> keys(10000);
    std::iota(begin(keys), end(keys), 1);
    std::shuffle(begin(keys), end(keys), std::default_random_engine{});

    insert_all(t, keys);

    RTF_ASSERT(has_all_keys(t, keys));

    std::shuffle(begin(keys), end(keys), std::default_random_engine{});

    for (auto k : keys) {
        t.remove(k);
    }

    for (auto k : keys) {
        RTF_ASSERT(!t.search(k));
    }

    unlink("test_large_number_of_keys.db");
}

void test_b_tree::test_concurrent_inserts()
{
    b_tree::create_db_file("test_concurrent_inserts.db");
    b_tree t("test_concurrent_inserts.db", 4);

    const int num_threads = 1;
    const int num_inserts_per_thread = 1000;

    std::vector<std::thread> threads;
    std::vector<std::vector<int64_t>> thread_keys(num_threads);

    // Launch multiple threads to perform concurrent inserts
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < num_inserts_per_thread; ++j) {
                int64_t key = i * num_inserts_per_thread + j;
                thread_keys[i].push_back(key);
                t.insert(key, key + 100);
            }
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify that all inserted keys are present in the B-tree
    for (const auto& keys : thread_keys) {
        RTF_ASSERT(has_all_keys(t, keys));
    }

    t.write_dot_file("big_dotfile.txt");

    unlink("test_concurrent_inserts.db");
}
