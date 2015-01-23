#tdb
A playground for learning about databases...

tdb is a super simple database, suitable for learning about how databases work (primarily because it is so short).
It supports atomic commit (via a journal file), and transactions (with rollback). tdb is implemented with a disk
based AVL tree primarily because I consider it to be the simplest to implement but still balanced tree type. A real
database should use B+trees or log structured merge trees.

###Inserting elements
```
#include "tdb/ptree.h"

using namespace tdb;
using namespace cppkit;

int main( int argc, char* argv[] )
{
    uint8_t dumbRecord[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    
    ptree t( "data.db", 1024*1024*5 );
    
    t.insert( 100, dumbRecord, 10 );

    return 0;
}
```

###Removing elements
```
    ptree t( "data.db", 1024*1024*5 );

    t.remove( 100 );
```

###Finding an element
```
    ptree t( "data.db", 1024*1024*5 );
    
    ptree::iterator i = t.find( 100 );
```

###Range queries
```
    ptree t( "data.db", 1024*1024*5 );
    
    ptree::iterator i = t.find( 100 );
    
    for( i = t.find( 100 ); i.key() < 500; i++ )
    {
        int64_t key = i.key();
        uint32_t dataSize = i.data_size();
        uint8_t* data = i.data_ptr();
    }
```

###Transactions
By default insert() and remove() operations are their own transactions, but since transactions control when disk
sync's occur MUCH greater performance can be achieved by batching your changes up in transactions.
```
    uint8_t dumbRecord[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    ptree t( "data.db", 1024*1024*5 );
    
    t.begin_transaction();
    for( int i = 0; i < 1000; i++ )
        t.insert( i, dumbRecord, 10 );
    t.commit_transaction();
```
Transactions can also be rolled back with:
```
    t.roll_back_transaction();
```
Automatic transaction rollback will occur at the beginning of a transaction if a journal file is present in the same directory as the database file.
