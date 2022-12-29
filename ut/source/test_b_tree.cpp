
#include "test_b_tree.h"
#include "tdb/b_tree.h"
#include <numeric>

using namespace std;

REGISTER_TEST_FIXTURE(test_b_tree);

void test_b_tree::setup()
{
}

void test_b_tree::teardown()
{
}

void test_b_tree::test_basic()
{
    std::vector<int> test_vals(1000);
    std::iota(begin(test_vals), end(test_vals), 0);

    auto n_test_vals = test_vals.size();

    pager::create("test.db");
    b_tree t("test.db");
    for(int i = 0, n = n_test_vals; i < n; ++i)
    {
        auto range = n - i;
        auto randex = i + (rand() % range);
        std::swap(test_vals[i], test_vals[randex]);
        t.insert(test_vals[i], 0);
    }

    t.render_to_dot_file("dotfile.txt");
}
