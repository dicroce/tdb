
#include "framework.h"

class test_b_tree : public test_fixture
{
public:
    RTF_FIXTURE(test_b_tree);
      TEST(test_b_tree::test_basic);
      TEST(test_b_tree::test_dot_file);
      TEST(test_b_tree::test_basic_remove);
      TEST(test_b_tree::test_lots_of_inserts_and_removes);
    RTF_FIXTURE_END();

    virtual ~test_b_tree() throw() {}

    virtual void setup();
    virtual void teardown();

    void test_basic();
    void test_dot_file();
    void test_basic_remove();
    void test_lots_of_inserts_and_removes();
};
