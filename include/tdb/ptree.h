
#ifndef __tdb_ptree_h
#define __tdb_ptree_h

#include "cppkit/ck_string.h"
#include "cppkit/ck_memory_map.h"
#include "cppkit/ck_types.h"
#include <stdlib.h>
#include <memory>
#include <map>

namespace tdb
{

// ptree is a super tiny key value database. ptree is very much a work in progress at the moment and should not be
// used for any mission critical task. I do my best to keep it defect free, but I make no guarantees.
//
// The main purpose of ptree is to illustrate some of the concepts required to implement a real database. I consider
// it largely educational.
//
// Features:
//     - Short (ptree.cpp is 490 lines long at this moment).
//     - Atomic (journal based).
//     - Supports transactions (implicit or explicit).
//     - Simplicity (ptree is based on AVL trees, which IMHO are the simplest form of balanced tree... so it should
//       be pretty easy to work on).
//     - Not Thread Safe, so no speed penalty or complexity from locking.
//
// Negatives:
//     - AVL trees will always lose to B+Trees for jobs like this, so if you need the utmost performance consider
//       BerkelyDB or Tokyo Cabinet. I chose AVL trees because they were a good comprimise between ease of
//       implementation and performance.
//     - Not Thread Safe, so you'll have to do your own locking.
//     - No server... but if you need a server, this is not the DB for you... :)
//
// Future Goals:
//     - b+tree
//     - > 4gb databases
//     - more intelligent memory mapping.
//

class ptree
{
public:
    class iterator
    {
        friend class ptree;
        enum iterator_enum { END };

    public:
        iterator();
        iterator( uint32_t node, ptree* tree );
        iterator( const iterator& obj );
        virtual ~iterator() throw();

        iterator& operator = ( const iterator& rhs );

        bool operator == ( const iterator& rhs );
        bool operator != ( const iterator& rhs );

        iterator& operator++();
        iterator operator++(int);

        iterator& operator--();
        iterator operator--(int);

        int64_t key();
        uint32_t data_size();
        uint8_t* data_ptr();

    private:
        bool _prev();
        bool _next();

        uint32_t _current;
        ptree* _tree;
    };

    friend class iterator;

    ptree( const cppkit::ck_string& path, size_t indexAllocSize );

    virtual ~ptree() throw();

    // insertion and removal...
    void insert( int64_t key, uint8_t* src, uint32_t size );
    void remove( int64_t key );

    // transactions...
    void begin_transaction();
    void commit_transaction();
    void roll_back_transaction();

    // iterators...
    iterator begin();
    iterator end();
    iterator find( int64_t key );

private:
    void _initialize_new_file();

    void _write_word( uint32_t ofs, uint32_t val );
    void _write_dword( uint32_t ofs, int64_t val );
    void _write_byte( uint32_t ofs, uint8_t val );
    void _write_bytes( uint32_t ofs, uint8_t* src, uint32_t size );

    uint32_t _read_word( uint32_t ofs );
    int64_t _read_dword( uint32_t ofs );
    uint8_t _read_byte( uint32_t ofs );

    uint8_t _height( uint32_t ofs );
    int _balance_factor( uint32_t ofs );
    void _fix_height( uint32_t ofs );
    uint32_t _rotate_right( uint32_t p );
    uint32_t _rotate_left( uint32_t q );
    uint32_t _balance( uint32_t p );

    uint32_t _create_node( int64_t key, uint32_t parent, uint8_t* src, uint32_t size );

    uint32_t _write_data_record( uint8_t* src, uint32_t size );

    uint32_t _insert( uint32_t p, uint32_t parent, int64_t key, uint8_t* src, uint32_t size  );

    uint32_t _find_min( uint32_t p );
    uint32_t _remove_min( uint32_t p );

    uint32_t _remove( uint32_t p, int64_t k );

    void _write_journal( uint32_t ofs, uint32_t size );

    iterator _begin( uint32_t ofs );

    iterator _find( uint32_t ofs, int64_t key );

    cppkit::ck_string _path;
    FILE* _file;
    size_t _fileSize;
    std::shared_ptr<cppkit::ck_memory_map> _memoryMap;
    FILE* _journalFile;
    bool _completeJournalRecovery;
    std::map<uint32_t,bool> _dirtyPages;
};

};

#endif
