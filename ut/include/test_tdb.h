
#include "framework.h"

class test_tdb : public test_fixture
{
public:

    RTF_FIXTURE(test_tdb);
      TEST(test_tdb::test_basic);
    RTF_FIXTURE_END();

    virtual ~test_tdb() throw() {}

    virtual void setup();
    virtual void teardown();

    void test_basic();
};
