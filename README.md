# tdb
A playground for learning about databases...

tdb is a super simple database, suitable for learning about how databases work (primarily because it is so short).
It supports atomic commit (via a journal file), and transactions (with rollback). tdb is implemented with a disk
based AVL tree primarily because I consider it to be the simplest to implement but still balanced tree type. A real
database should use B+trees or log structured merge trees.

#include "tdb/ptree.h"

using namespace tdb;
using namespace cppkit;

int main( int agrc, char* argv[] )
{
    uint8_t dumbRecord[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    
    ptree t( "data.db", 1024*1024*5 );
    
    t.begin_transaction();
    t.insert( 100, dumbRecord, 10 );
    t.end_transaction();
}
