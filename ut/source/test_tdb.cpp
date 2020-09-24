
#include "test_tdb.h"
#include "tdb/ptree.h"
#include <vector>
#include <unistd.h>
#include <string.h>

using namespace std;
using namespace cppkit;
using namespace tdb;

REGISTER_TEST_FIXTURE(test_tdb);

void test_tdb::setup()
{
}

void test_tdb::teardown()
{
}

void test_tdb::test_basic()
{
    uint8_t dumbRecord[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    
    {
        ptree t( "data.db", 1024*1024*5 );
    
        t.insert( 100, dumbRecord, 10 );
    }

    {
        ptree t( "data.db", 1024*1024*5 );
    
        ptree::iterator i = t.find( 100 );

        vector<uint8_t> val(i.data_size());
        memcpy(&val[0], i.data_ptr(), i.data_size());

        RTF_ASSERT(memcmp(dumbRecord, &val[0], 10) == 0);
    }
}
