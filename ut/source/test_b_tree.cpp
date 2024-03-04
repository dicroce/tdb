
#include "test_b_tree.h"
#include "tdb/b_tree.h"
#include <numeric>
#include <unistd.h>

using namespace std;

REGISTER_TEST_FIXTURE(test_b_tree);

void test_b_tree::setup()
{
    b_tree::create_db_file("test.db");

    b_tree t("test.db");

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
        t.insert(test_keys[i], test_vals[i]);
    }
}

void test_b_tree::teardown()
{
    unlink("test.db");
    //unlink("dotfile.txt");
}

void test_b_tree::test_basic()
{
    b_tree t("test.db");

    RTF_ASSERT(t.search(747) == 1747);
    RTF_ASSERT(t.search(0) == 1000);
    RTF_ASSERT(t.search(999) == 1999);
}

void test_b_tree::test_dot_file()
{
    b_tree t("test.db");

    t.render_to_dot_file("dotfile.txt");
}

void test_b_tree::test_basic_remove()
{
    b_tree::create_db_file("test_basic_remove.db");
    b_tree t("test_basic_remove.db");

    t.insert(10, 10);
    t.insert(20, 20);
    t.insert(30, 30);
    t.insert(40, 40);

    t.render_to_dot_file("dotfile_basic_remove_pre.txt");

    t.remove(30);

    t.render_to_dot_file("dotfile_basic_remove.txt");

}