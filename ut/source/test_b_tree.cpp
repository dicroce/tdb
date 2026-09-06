
#include "test_b_tree.h"
#include "tdb/b_tree.h"
#include "tdb/row_store.h"
#include <algorithm>
#include <numeric>
#include <random>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <cstdint>
#include <atomic>
#include <filesystem>

using namespace std;

REGISTER_TEST_FIXTURE(test_b_tree);

std::vector<int64_t> test_keys(100);
std::vector<uint64_t> test_vals(100);

template<typename T>
bool has_all_keys(T& t, const vector<int64_t>& keys)
{
    for(auto k : keys)
    {
        if(!t.get(k))
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

    auto write = t.begin_write();
    insert_all(write, test_keys);
    write.commit();
}

void test_b_tree::teardown()
{
    remove_file("test.db");
    remove_file("dotfile.txt");
    remove_file("test_basic_remove.db");
    remove_file("test_insert_ascending_order.db");
    remove_file("test_insert_descending_order.db");
    remove_file("test_remove_random_order.db");
    remove_file("test_large_number_of_keys.db");
    remove_file("test_concurrent_inserts.db");
    remove_file("big_dotfile.txt");
    remove_file("test_remove_reuses_pages.db");
    remove_file("test_remove_reuses_pages.db.wal");
    remove_file("test_explicit_write_transaction.db");
    remove_file("test_explicit_write_transaction.db.wal");
    remove_file("test_row_store.db");
    remove_file("test_row_store.db.wal");
    remove_file("test_atomic_row_and_index.db");
    remove_file("test_atomic_row_and_index.db.wal");
    remove_file("test_row_store_overflow.db");
    remove_file("test_row_store_overflow.db.wal");
    remove_file("test_row_store_overflow_reuse.db");
    remove_file("test_row_store_overflow_reuse.db.wal");
    remove_file("test_row_store_overflow_atomic.db");
    remove_file("test_row_store_overflow_atomic.db.wal");
}

void test_b_tree::test_CAS()
{
    std::atomic<uint64_t> counter = 41;

    // Make sure CAS update on new value (where compval==*ptr) works
    uint64_t expected = 41;
    auto result = counter.compare_exchange_strong(expected, 42);
    RTF_ASSERT(result);

    // Make sure CAS update on same value (where compval==*ptr) works
    expected = 42;
    result = counter.compare_exchange_strong(expected, 42);
    RTF_ASSERT(result);

    // Make sure CAS update on different value (where compval!=*ptr) works
    expected = 43;
    result = counter.compare_exchange_strong(expected, 44);
    RTF_ASSERT(!result);
}

void test_b_tree::test_basic()
{
    b_tree t("test.db", 4);

    RTF_ASSERT(t.get(47) == 147);
    RTF_ASSERT(t.get(0) == 100);
    RTF_ASSERT(t.get(99) == 199);
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

    RTF_ASSERT(!t.get(30));
    RTF_ASSERT(has_all_keys(t, {10, 20, 40, 50, 60, 70, 80, 90, 100}));

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
        RTF_ASSERT(t.get(k));
        t.remove(k);
        RTF_ASSERT(!t.get(k));
    }
}

void test_b_tree::test_insert_ascending_order()
{
    b_tree::create_db_file("test_insert_ascending_order.db");
    b_tree t("test_insert_ascending_order.db", 4);

    std::vector<int64_t> keys(100);
    std::iota(begin(keys), end(keys), 1);

    {
        auto write = t.begin_write();
        insert_all(write, keys);
        write.commit();
    }

    RTF_ASSERT(has_all_keys(t, keys));

}

void test_b_tree::test_insert_descending_order()
{
    b_tree::create_db_file("test_insert_descending_order.db");
    b_tree t("test_insert_descending_order.db", 4);

    std::vector<int64_t> keys(100);
    std::iota(rbegin(keys), rend(keys), 1);

    {
        auto write = t.begin_write();
        insert_all(write, keys);
        write.commit();
    }

    RTF_ASSERT(has_all_keys(t, keys));

}

void test_b_tree::test_remove_random_order()
{
    b_tree::create_db_file("test_remove_random_order.db");
    b_tree t("test_remove_random_order.db", 4);

    std::vector<int64_t> keys(100);
    std::iota(begin(keys), end(keys), 1);
    std::shuffle(begin(keys), end(keys), std::default_random_engine{});

    {
        auto write = t.begin_write();
        insert_all(write, keys);
        write.commit();
    }

    std::shuffle(begin(keys), end(keys), std::default_random_engine{});

    {
        auto write = t.begin_write();
        for (auto k : keys)
            write.remove(k);
        write.commit();
    }

    auto read = t.begin_read();
    for (auto k : keys)
        RTF_ASSERT(!read.get(k));

}

void test_b_tree::test_search_non_existent_keys()
{
    b_tree t("test.db", 4);

    std::vector<int64_t> non_existent_keys = {-10, -5, 200, 500};

    for (auto k : non_existent_keys) {
        RTF_ASSERT(!t.get(k));
    }
}

void test_b_tree::test_large_number_of_keys()
{
    b_tree::create_db_file("test_large_number_of_keys.db");
    b_tree t("test_large_number_of_keys.db", 4);

    std::vector<int64_t> keys(10000);
    std::iota(begin(keys), end(keys), 1);
    std::shuffle(begin(keys), end(keys), std::default_random_engine{});

    {
        auto write = t.begin_write();
        insert_all(write, keys);
        write.commit();
    }

    {
        auto read = t.begin_read();
        RTF_ASSERT(has_all_keys(read, keys));
    }

    std::shuffle(begin(keys), end(keys), std::default_random_engine{});

    {
        auto write = t.begin_write();
        for (auto k : keys)
            write.remove(k);
        write.commit();
    }

    {
        auto read = t.begin_read();
        for (auto k : keys)
            RTF_ASSERT(!read.get(k));
    }

}

void test_b_tree::test_concurrent_inserts()
{
    b_tree::create_db_file("test_concurrent_inserts.db");
    b_tree t("test_concurrent_inserts.db", 4);

    const int num_threads = 4;
    const int num_inserts_per_thread = 1000;

    std::vector<std::thread> threads;
    std::vector<std::vector<int64_t>> thread_keys(num_threads);

    // Launch multiple threads to perform concurrent inserts
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            auto write = t.begin_write();
            for (int j = 0; j < num_inserts_per_thread; ++j) {
                int64_t key = i * num_inserts_per_thread + j;
                thread_keys[i].push_back(key);
                write.insert(key, key + 100);
            }
            write.commit();
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify that all inserted keys are present in the B-tree
    {
        auto read = t.begin_read();
        for (const auto& keys : thread_keys) {
            RTF_ASSERT(has_all_keys(read, keys));
        }
    }

    t.write_dot_file("big_dotfile.txt");

}

void test_b_tree::test_delete_and_reinsert()
{
    b_tree t("test.db", 4);
    t.remove(47);
    RTF_ASSERT(!t.get(47));
    t.insert(47, 947);
    RTF_ASSERT(t.get(47) == 947);
}

void test_b_tree::test_concurrent_readers()
{
    b_tree t("test.db", 4);
    std::atomic<bool> all_found = true;
    std::vector<std::thread> readers;
    for (int reader = 0; reader < 8; ++reader)
    {
        readers.emplace_back([&]() {
            auto read = t.begin_read();
            for (int pass = 0; pass < 20; ++pass)
                for (int64_t key = 0; key < 100; ++key)
                    if (read.get(key) != key + 100)
                        all_found = false;
        });
    }
    for (auto& reader : readers) reader.join();
    RTF_ASSERT(all_found);
}

void test_b_tree::test_remove_reuses_pages()
{
    const std::string path = "test_remove_reuses_pages.db";
    b_tree::create_db_file(path);
    std::uintmax_t high_water_size = 0;
    {
        b_tree t(path, 2);
        {
            auto write = t.begin_write();
            for (int64_t key = 0; key < 500; ++key)
                write.insert(key, key + 1000);
            write.commit();
        }
        high_water_size = std::filesystem::file_size(path);

        {
            auto write = t.begin_write();
            for (int64_t key = 0; key < 500; ++key)
                write.remove(key);
            write.commit();
        }
        auto read = t.begin_read();
        for (int64_t key = 0; key < 500; ++key)
            RTF_ASSERT(!read.get(key));
    }
    {
        b_tree t(path, 2);
        {
            auto write = t.begin_write();
            for (int64_t key = 500; key < 1000; ++key)
                write.insert(key, key + 1000);
            write.commit();
        }
        auto read = t.begin_read();
        for (int64_t key = 500; key < 1000; ++key)
            RTF_ASSERT(read.get(key) == key + 1000);
    }
    RTF_ASSERT(std::filesystem::file_size(path) == high_water_size);
}

void test_b_tree::test_iterator()
{
    b_tree t("test.db", 4);

    auto read = t.begin_read();
    auto iterator = read.search(0);
    RTF_ASSERT(iterator);
    for (int64_t key = 0; key < 100; ++key)
    {
        RTF_ASSERT(iterator.key() == key);
        RTF_ASSERT(iterator.value() == key + 100);
        if (key < 99) RTF_ASSERT(iterator.next());
    }
    RTF_ASSERT(!iterator.next());
    RTF_ASSERT(!iterator);

    RTF_ASSERT(iterator.find(99));
    for (int64_t key = 99; key >= 0; --key)
    {
        RTF_ASSERT(iterator.key() == key);
        if (key > 0) RTF_ASSERT(iterator.prev());
    }
    RTF_ASSERT(!iterator.prev());

    RTF_ASSERT(iterator.find(50));
    RTF_ASSERT(iterator.next());
    RTF_ASSERT(iterator.key() == 51);
    RTF_ASSERT(iterator.prev());
    RTF_ASSERT(iterator.key() == 50);

    RTF_ASSERT(!iterator.find(500));
    RTF_ASSERT(!iterator.next());
    RTF_ASSERT(!iterator.prev());
    RTF_ASSERT_THROWS(iterator.key(), std::logic_error);
    RTF_ASSERT_THROWS(iterator.value(), std::logic_error);
}

void test_b_tree::test_explicit_write_transaction()
{
    const std::string path = "test_explicit_write_transaction.db";
    b_tree::create_db_file(path);
    b_tree t(path, 2);
    {
        auto write = t.begin_write();
        for (int64_t key = 0; key < 500; ++key)
            write.insert(key, key + 1000);
        write.commit();
    }
    for (int64_t key = 0; key < 500; ++key)
        RTF_ASSERT(t.get(key) == key + 1000);

    {
        auto write = t.begin_write();
        for (int64_t key = 0; key < 400; ++key)
            write.remove(key);
        write.commit();
    }
    for (int64_t key = 0; key < 400; ++key)
        RTF_ASSERT(!t.get(key));
    for (int64_t key = 400; key < 500; ++key)
        RTF_ASSERT(t.get(key) == key + 1000);

    {
        auto write = t.begin_write();
        write.insert(2000, 3000);
    }
    RTF_ASSERT(!t.get(2000));
}

void test_b_tree::test_row_store()
{
    const string path = "test_row_store.db";
    b_tree::create_db_file(path);
    b_tree database(path, 4);
    row_store rows;
    vector<uint8_t> first{0, 1, 2, 0, 255};
    vector<uint8_t> empty;
    vector<uint8_t> large(1000);
    iota(large.begin(), large.end(), uint8_t{0});

    row_id first_id = 0;
    row_id empty_id = 0;
    vector<row_id> large_ids;
    {
        auto write = database.begin_write();
        first_id = rows.insert(write, first);
        empty_id = rows.insert(write, empty);
        for (int i = 0; i < 12; ++i)
            large_ids.push_back(rows.insert(write, large));
        write.commit();
    }
    {
        auto read = database.begin_read();
        RTF_ASSERT(rows.get(read, first_id) == first);
        RTF_ASSERT(rows.get(read, empty_id) == empty);
        for (row_id id : large_ids) RTF_ASSERT(rows.get(read, id) == large);
    }

    row_id replacement_id = 0;
    {
        auto write = database.begin_write();
        RTF_ASSERT(rows.remove(write, first_id));
        RTF_ASSERT(!rows.remove(write, first_id));
        replacement_id = rows.insert(write, vector<uint8_t>{9, 8, 7});
        RTF_ASSERT(replacement_id != first_id);
        write.commit();
    }
    {
        auto read = database.begin_read();
        RTF_ASSERT(!rows.get(read, first_id));
        RTF_ASSERT(rows.get(read, replacement_id) == vector<uint8_t>({9, 8, 7}));
    }
}

static vector<uint8_t> pattern_bytes(size_t length)
{
    vector<uint8_t> result(length);
    for (size_t i = 0; i < length; ++i)
        result[i] = static_cast<uint8_t>((i * 31 + 7) & 0xff);
    return result;
}

void test_b_tree::test_row_store_overflow()
{
    const string path = "test_row_store_overflow.db";
    b_tree::create_db_file(path);
    b_tree database(path, 4);
    row_store rows;

    // 4068 is the largest inline row; 4080 is one full overflow page.
    const vector<size_t> sizes{0, 1, 4067, 4068, 4069, 4080, 4081, 8160, 8161, 1000000};
    vector<vector<uint8_t>> payloads;
    vector<row_id> ids;
    {
        auto write = database.begin_write();
        for (size_t size : sizes)
        {
            payloads.push_back(pattern_bytes(size));
            ids.push_back(rows.insert(write, payloads.back()));
            // Interleave a small row so the slotted pages stay in play.
            rows.insert(write, vector<uint8_t>{1, 2, 3});
        }
        write.commit();
    }
    {
        auto read = database.begin_read();
        for (size_t i = 0; i < ids.size(); ++i)
            RTF_ASSERT(rows.get(read, ids[i]) == payloads[i]);
    }

    // Survives a reopen, so the chain is genuinely on disk.
    {
        b_tree reopened(path, 4);
        auto read = reopened.begin_read();
        for (size_t i = 0; i < ids.size(); ++i)
            RTF_ASSERT(rows.get(read, ids[i]) == payloads[i]);
    }

    // Removing a spilled row invalidates it without disturbing its neighbours.
    {
        auto write = database.begin_write();
        RTF_ASSERT(rows.remove(write, ids.back()));
        RTF_ASSERT(!rows.remove(write, ids.back()));
        write.commit();
    }
    {
        auto read = database.begin_read();
        RTF_ASSERT(!rows.get(read, ids.back()));
        for (size_t i = 0; i + 1 < ids.size(); ++i)
            RTF_ASSERT(rows.get(read, ids[i]) == payloads[i]);
    }
}

void test_b_tree::test_row_store_overflow_reuses_pages()
{
    const string path = "test_row_store_overflow_reuse.db";
    b_tree::create_db_file(path);
    b_tree database(path, 4);
    row_store rows;
    const vector<uint8_t> payload = pattern_bytes(1000000);

    row_id first_id = 0;
    {
        auto write = database.begin_write();
        first_id = rows.insert(write, payload);
        write.commit();
    }
    const uintmax_t high_water_size = filesystem::file_size(path);

    {
        auto write = database.begin_write();
        RTF_ASSERT(rows.remove(write, first_id));
        write.commit();
    }

    row_id second_id = 0;
    {
        auto write = database.begin_write();
        second_id = rows.insert(write, payload);
        write.commit();
    }
    {
        auto read = database.begin_read();
        RTF_ASSERT(!rows.get(read, first_id));
        RTF_ASSERT(rows.get(read, second_id) == payload);
    }
    // The whole chain came back through the pager free list.
    RTF_ASSERT(filesystem::file_size(path) == high_water_size);
}

void test_b_tree::test_row_store_overflow_is_atomic()
{
    const string path = "test_row_store_overflow_atomic.db";
    b_tree::create_db_file(path);
    const uintmax_t empty_size = filesystem::file_size(path);
    const vector<uint8_t> payload = pattern_bytes(1000000);
    row_store rows;

    {
        b_tree database(path, 4);
        // Dropped without committing: a 1 MB chain must leave nothing on disk.
        auto write = database.begin_write();
        const row_id id = rows.insert(write, payload);
        write.insert(1, static_cast<int64_t>(id));
    }
    RTF_ASSERT(filesystem::file_size(path) == empty_size);
    {
        b_tree database(path, 4);
        auto read = database.begin_read();
        RTF_ASSERT(!read.get(1));
    }

    // The same spill, committed, is intact after a reopen.
    row_id committed_id = 0;
    {
        b_tree database(path, 4);
        auto write = database.begin_write();
        committed_id = rows.insert(write, payload);
        write.insert(1, static_cast<int64_t>(committed_id));
        write.commit();
    }
    {
        b_tree database(path, 4);
        auto read = database.begin_read();
        RTF_ASSERT(read.get(1) == static_cast<int64_t>(committed_id));
        RTF_ASSERT(rows.get(read, committed_id) == payload);
    }
}

void test_b_tree::test_atomic_row_and_index_transaction()
{
    const string path = "test_atomic_row_and_index.db";
    b_tree::create_db_file(path);
    b_tree database(path, 4);
    row_store rows;
    {
        auto write = database.begin_write();
        const row_id id = rows.insert(write, vector<uint8_t>{1, 2, 3});
        write.insert(10, static_cast<int64_t>(id));
    }
    RTF_ASSERT(!database.get(10));

    row_id committed_id = 0;
    {
        auto write = database.begin_write();
        committed_id = rows.insert(write, vector<uint8_t>{4, 5, 6});
        write.insert(10, static_cast<int64_t>(committed_id));
        write.commit();
    }
    {
        auto read = database.begin_read();
        auto indexed_id = read.get(10);
        RTF_ASSERT(indexed_id == static_cast<int64_t>(committed_id));
        RTF_ASSERT(rows.get(read, static_cast<row_id>(*indexed_id)) ==
                   vector<uint8_t>({4, 5, 6}));
    }
}
