
#include "framework.h"

class test_b_tree : public test_fixture
{
public:
    RTF_FIXTURE(test_b_tree);
      TEST(test_b_tree::test_basic);
      TEST(test_b_tree::test_dot_file);
      TEST(test_b_tree::test_has_all_keys);
      TEST(test_b_tree::test_missing_keys);
      TEST(test_b_tree::test_keys_inserted_in_order);
    RTF_FIXTURE_END();

    virtual ~test_b_tree() throw() {}

    virtual void setup();
    virtual void teardown();

    void test_basic();
    void test_dot_file();
    void test_has_all_keys();
    void test_missing_keys();
    void test_keys_inserted_in_order();
};
