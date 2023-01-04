
#include "test_b_tree.h"
#include "tdb/b_tree.h"
#include <numeric>
#include <unistd.h>

using namespace std;

REGISTER_TEST_FIXTURE(test_b_tree);

// Return -1, 1 or 0 if a < b, a > b or a == b respectively
int cmp(int64_t a, int64_t b)
{
    return (a<b)?-1:((a>b)?1:0);
}

void test_b_tree::setup()
{
    {
        b_tree<int64_t>::create_db_file("test.db");

        b_tree<int64_t> t("test.db");

        std::vector<int> test_keys(1000);
        std::iota(begin(test_keys), end(test_keys), 0);

        std::vector<int> test_vals(1000);
        std::iota(begin(test_vals), end(test_vals), 1000);

        auto n_test_keys = test_keys.size();

        for(int i = 0, n = n_test_keys; i < n; ++i)
        {
            auto range = n - i;
            auto randex = i + (rand() % range);
            std::swap(test_vals[i], test_vals[randex]);
            std::swap(test_keys[i], test_keys[randex]);
            t.insert(
                test_keys[i],
                test_vals[i],
                cmp
            );
        }
    }
    {
        b_tree<int64_t>::create_db_file("test_inserted_in_order.db");

        b_tree<int64_t> t("test_inserted_in_order.db");

        std::vector<int> test_keys(1000);
        std::iota(begin(test_keys), end(test_keys), 0);

        std::vector<int> test_vals(1000);
        std::iota(begin(test_vals), end(test_vals), 1000);

        auto n_test_keys = test_keys.size();

        for(int i = 0, n = n_test_keys; i < n; ++i)
            t.insert(
                test_keys[i],
                test_vals[i],
                cmp
            );
    }
}

void test_b_tree::teardown()
{
    unlink("test.db");
    unlink("test_inserted_in_order.db");
    unlink("dotfile.txt");
}

void test_b_tree::test_basic()
{
    b_tree<int64_t> t("test.db");

    RTF_ASSERT(t.search(747, cmp) == 1747);
    RTF_ASSERT(t.search(0, cmp) == 1000);
    RTF_ASSERT(t.search(999, cmp) == 1999);
}

void test_b_tree::test_dot_file()
{
    b_tree<int64_t> t("test.db");

    t.render_to_dot_file("dotfile.txt");
}

void test_b_tree::test_has_all_keys()
{
    std::vector<int> test_keys(1000);
    std::iota(begin(test_keys), end(test_keys), 0);

    b_tree<int64_t> t("test.db");
    for(auto k : test_keys)
    {
        RTF_ASSERT(t.search(k, cmp) == k + 1000);
    }
}

void test_b_tree::test_missing_keys()
{
    std::vector<int> test_keys(1000);
    std::iota(begin(test_keys), end(test_keys), 1000);

    b_tree<int64_t> t("test.db");
    for(auto k : test_keys)
    {
        RTF_ASSERT(t.search(k, cmp).has_value() == false);
    }
}

void test_b_tree::test_keys_inserted_in_order()
{
    std::vector<int> test_keys(1000);
    std::iota(begin(test_keys), end(test_keys), 0);

    b_tree<int64_t> t("test_inserted_in_order.db");
    for(auto k : test_keys)
    {
        RTF_ASSERT(t.search(k, cmp) == k + 1000);
    }
}
