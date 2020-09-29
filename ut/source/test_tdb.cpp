
#include "test_tdb.h"
#include "tdb/ptree.h"
#include "tdb/graphics.h"
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
        t.insert( 22, dumbRecord, 10 );
        t.insert( 72, dumbRecord, 10 );
        t.insert( 35, dumbRecord, 10 );
        t.insert( 88, dumbRecord, 10 );
        t.insert( 0, dumbRecord, 10 );
        t.insert( 34, dumbRecord, 10 );
        t.insert( 99, dumbRecord, 10 );
        t.insert( 89, dumbRecord, 10 );



    }

    {
        ptree t( "data.db", 1024*1024*5 );
    
        ptree::iterator i = t.find( 100 );

        vector<uint8_t> val(i.data_size());
        memcpy(&val[0], i.data_ptr(), i.data_size());

        RTF_ASSERT(memcmp(dumbRecord, &val[0], 10) == 0);
    }

    {
        ptree t( "data.db", 1024*1024*5 );

        auto img = make_shared<vector<uint8_t>>(1920*4*1920);    

        t.draw_tree(img, 1920, 1920);

        argb_to_ppm_file("out.ppm",
                        img,
                        1920,
                        1920);

    }

#if 0
    uint32_t w, h, color_max;
    auto buffer = ppm_buffer_to_argb(__0_ppm, __0_ppm_len, w, h, color_max);

    auto img = make_shared<vector<uint8_t>>(1280*4*720);

    memset(&(*img)[0], 0, img->size());

    render_text(img,
                     1280,
                     720, 
                     "test", 
                     100, 
                     300);

    draw_line(img,
              1280,
              720,
              100,
              100,
              1100,
              600,
              255,
              0,
              0);

    draw_line_rect(img,
                   1280,
                   720,
                   610,
                   100,
                   150,
                   190,
                   0,
                   255,
                   0);

    argb_to_ppm_file("out.ppm",
                     img,
                     1280,
                     720);
#endif
}
