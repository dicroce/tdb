

#include "ptree/ptree.h"
#include "cppkit/ck_exception.h"
#include "cppkit/ck_path.h"
#include "cppkit/os/ck_large_files.h"
#include "cppkit/ck_byte_ptr.h"
#include "cppkit/ck_socket.h"

using namespace tdb;
using namespace cppkit;
using namespace std;

const uint32_t HEADER_SIZE = 64;

const uint32_t CB_VERSION_OFS = 0;
const uint32_t CB_ROOT_OFS = 4;
const uint32_t CB_FREE_OFS = 8;
const uint32_t CB_DATA_FREE_OFS = 12;
const uint32_t CB_PAGE_SIZE = 4096;

const uint32_t NODE_SIZE = 32;

const uint32_t NODE_KEY_OFS = 0;         // int64_t
const uint32_t NODE_HEIGHT_OFS = 8;      // uint8_t
const uint32_t NODE_FLAGS_OFS = 9;       // uint8_t
const uint32_t NODE_LEFT_OFS = 12;       // uint32_t
const uint32_t NODE_RIGHT_OFS = 16;      // uint32_t
const uint32_t NODE_PARENT_OFS = 20;     // uint32_t
const uint32_t NODE_REC_OFS = 24;        // uint32_t
const uint32_t NODE_REC_SIZE_OFS = 28;   // uint32_t

const uint8_t NODE_FLAG_USED = 1;

// Node
//     int64_t key         0 - 7
//     uint8_t height          8
//     uint8_t flags           9
//     uint16_t reserved   10 - 11
//     uint32_t left       12 - 15
//     uint32_t right      16 - 19
//     uint32_t parent     20 - 23
//     uint32_t recordOffset  24 - 27
//     uint32_t size          28 - 31
//
// Journal Entry
//     uint32_t  ofs
//     uint32_t  size
//     uint8_t[] bytes

ptree::iterator::iterator() :
    _current( ptree::iterator::END ),
    _tree( NULL )
{
}

ptree::iterator::iterator( uint32_t node, ptree* tree ) :
    _current( node ),
    _tree( tree )
{
}

ptree::iterator::iterator( const ptree::iterator& obj ) :
    _current( obj._current ),
    _tree( obj._tree )
{
}

ptree::iterator::~iterator() throw()
{
}

ptree::iterator& ptree::iterator::operator = ( const ptree::iterator& rhs )
{
    _current = rhs._current;
    _tree = rhs._tree;
    return *this;
}

bool ptree::iterator::operator == ( const ptree::iterator& rhs )
{
    if( (_current == rhs._current) )
        return true;
    return false;
}

bool ptree::iterator::operator != ( const ptree::iterator& rhs )
{
    if( (_current != rhs._current) )
        return true;
    return false;
}

ptree::iterator& ptree::iterator::operator++()
{
    if( _current == ptree::iterator::END )
        CK_THROW(("Invalid iterator."));

    bool incremented = _next();
    if( !incremented )
        _current = ptree::iterator::END;
    return *this;
}

ptree::iterator ptree::iterator::operator++(int)
{
    if( _current == ptree::iterator::END )
        CK_THROW(("Invalid iterator."));

    ptree::iterator tmp( *this );
    this->operator++();
    return tmp;
}

ptree::iterator& ptree::iterator::operator--()
{
    if( _current == ptree::iterator::END )
        CK_THROW(("Invalid iterator."));

    _prev();
    // should we do something different here if we are at the beginning of the tree?
    return *this;
}

ptree::iterator ptree::iterator::operator--(int)
{
    if( _current == ptree::iterator::END )
        CK_THROW(("Invalid iterator."));

    ptree::iterator tmp( *this );
    this->operator--();
    return tmp;
}

int64_t ptree::iterator::key()
{
    if( _current == ptree::iterator::END )
        CK_THROW(("Invalid iterator."));

    return _tree->_read_dword( _current + NODE_KEY_OFS );
}

uint32_t ptree::iterator::data_size()
{
    if( _current == ptree::iterator::END )
        CK_THROW(("Invalid iterator."));

    return _tree->_read_word( _current + NODE_REC_SIZE_OFS );
}

uint8_t* ptree::iterator::data_ptr()
{
    if( _current == ptree::iterator::END )
        CK_THROW(("Invalid iterator."));

    return (uint8_t*)_tree->_memoryMap->map().get_ptr() + _tree->_read_word( _current + NODE_REC_OFS );
}

// walking a tree without recursion is surprisingly complicated... :)
bool ptree::iterator::_prev()
{
    if( _tree->_read_word( _current + NODE_LEFT_OFS ) != 0 )
    {
        _current = _tree->_read_word( _current + NODE_LEFT_OFS );
        while( _tree->_read_word( _current + NODE_RIGHT_OFS ) != 0 )
            _current = _tree->_read_word( _current + NODE_RIGHT_OFS );
        return true;
    }
    else
    {
        uint32_t p = _tree->_read_word( _current + NODE_PARENT_OFS );
        while( p != 0 && _current == _tree->_read_word( p + NODE_LEFT_OFS ) )
        {
            _current = p;
            p = _tree->_read_word( _current + NODE_PARENT_OFS );
        }

        _current = p;

        if( _current )
            return true;
        return false;
    }
}

// walking a tree without recursion is surprisingly complicated... :)
bool ptree::iterator::_next()
{
    if( _tree->_read_word( _current + NODE_RIGHT_OFS ) != 0 )
    {
        _current = _tree->_read_word( _current + NODE_RIGHT_OFS );
        while( _tree->_read_word( _current + NODE_LEFT_OFS ) != 0 )
            _current = _tree->_read_word( _current + NODE_LEFT_OFS );
        return true;
    }
    else
    {
        uint32_t p = _tree->_read_word( _current + NODE_PARENT_OFS );
        while( p != 0 && _current == _tree->_read_word( p + NODE_RIGHT_OFS ) )
        {
            _current = p;
            p = _tree->_read_word( _current + NODE_PARENT_OFS );
        }

        _current = p;

        if( _current )
            return true;
        return false;
    }
}

ptree::ptree( const ck_string& indexPath, size_t indexAllocSize ) :
    _indexPath( indexPath ),
    _indexFile( NULL ),
    _indexFileSize( 0 ),
    _memoryMap(),
    _journalFile( NULL ),
    _completeJournalRecovery( true ),
    _dirtyPages()
{
    bool exists = ck_path::exists( _indexPath );

    if( !exists )
    {
        _indexFile = fopen( _indexPath.c_str(), "w+b" );
        if( !_indexFile )
            CK_THROW(("Unable to open file."));

        if( ck_fallocate( _indexFile, indexAllocSize ) < 0 )
            CK_THROW(("Unable to pre allocate file."));

        // Note: windows required preallocated files to be closed and reopened to see the new size
        fclose( _indexFile );
    }

    ck_file_info fileInfo;
    if( ck_stat( _indexPath.c_str(), &fileInfo ) < 0 )
        CK_THROW(("Unable to stat file."));

    _indexFileSize = fileInfo.file_size;

    _indexFile = fopen( _indexPath.c_str(), "r+b" );
    if( !_indexFile )
        CK_THROW(("Unable to open file."));

    _memoryMap = make_shared<ck_memory_map>( fileno( _indexFile ),
                                             0,
                                             (uint32_t)_indexFileSize,
                                             ck_memory_map::MM_PROT_READ | ck_memory_map::MM_PROT_WRITE,
                                             ck_memory_map::MM_TYPE_FILE | ck_memory_map::MM_SHARED );

    if( !exists )
        _initialize_new_file();
}

ptree::~ptree() throw()
{
    _memoryMap.reset();

    fclose( _indexFile );
}

void ptree::insert( int64_t key, uint8_t* src, uint32_t size  )
{
    if( !_completeJournalRecovery )
        CK_THROW(("Incomeple previous journal recovery detected!"));

    bool localTransaction = !_journalFile;

    if( localTransaction )
        begin_transaction();

    uint32_t root = _read_word( CB_ROOT_OFS );

    _write_word( CB_ROOT_OFS, _insert( root, 0, key, src, size ) );

    if( localTransaction )
        commit_transaction();
}

void ptree::remove( int64_t key )
{
    if( !_completeJournalRecovery )
        CK_THROW(("Incomeple previous journal recovery detected!"));

    bool localTransaction = !_journalFile;

    if( localTransaction )
        begin_transaction();

    uint32_t root = _read_word( CB_ROOT_OFS );

    _write_word( CB_ROOT_OFS, _remove( root, key ) );

    if( localTransaction )
        commit_transaction();
}

void ptree::begin_transaction()
{
    if( _journalFile )
        CK_THROW(("Journal is already open!"));

    if( ck_path::exists( "journal" ) )
    {
        CK_LOG_NOTICE(("Journal exists, attempting rollback..."));

        roll_back_transaction();

        if( !_completeJournalRecovery )
            CK_THROW(("Journal recovery failed! Very Bad!"));
    }

    _journalFile = fopen( "journal", "a+b" );

    _dirtyPages.clear();
}

void ptree::commit_transaction()
{
    if( !_journalFile )
        CK_THROW(("Cant commit unopened journal!"));

    ck_filecommit( _indexFile );

    fclose( _journalFile );

    _journalFile = NULL;

    ::remove( "journal" );
}

void ptree::roll_back_transaction()
{
    FILE* j = fopen( "journal", "rb" );
    if( !j )
        CK_THROW(("Unable to open journal for recovery!"));

    uint8_t* map = (uint8_t*)_memoryMap->map().get_ptr();

    _completeJournalRecovery = false;

    try
    {
        while( !feof( j ) )
        {
            uint32_t word;

            if( fread( &word, 1, sizeof( uint32_t ), j ) != sizeof( uint32_t ) )
            {
                if( feof( j ) )
                    continue;
                CK_THROW(("Incomplete journal read (ofs)."));
            }

            uint32_t ofs = ntohl( word );

            if( fread( &word, 1, sizeof( uint32_t ), j ) != sizeof( uint32_t ) )
            {
                if( feof( j ) )
                    continue;
                CK_THROW(("Incomplete journal read (size)."));
            }

            uint32_t size = ntohl( word );

            uint8_t* writeHead = map + ofs;

            if( fread( writeHead, 1, size, j ) != size )
            {
                if( feof( j ) )
                    continue;
                CK_THROW(("Incomplete journal read (data)."));
            }
        }

        _completeJournalRecovery = true;
    }
    catch(std::exception& ex)
    {
        CK_LOG_ERROR("%s",ex.what());
    }
    catch(...)
    {
        CK_LOG_ERROR("Unable to complete journal recovery.");
    }

    ck_filecommit( _indexFile );

    fclose( j );

    if( !_completeJournalRecovery )
        CK_THROW(("Was unable to succeed!"));

    ::remove( "journal" );
}

ptree::iterator ptree::begin()
{
    if( !_completeJournalRecovery )
        CK_THROW(("Incomeple previous journal recovery detected!"));

    return _begin( _read_word( CB_ROOT_OFS ) );
}

ptree::iterator ptree::end()
{
    if( !_completeJournalRecovery )
        CK_THROW(("Incomeple previous journal recovery detected!"));

    return ptree::iterator( ptree::iterator::END, this );
}

ptree::iterator ptree::find( int64_t key )
{
    return _find( _read_word( CB_ROOT_OFS ), key );
}

void ptree::_initialize_new_file()
{
    begin_transaction();

    _write_word( CB_VERSION_OFS, 1 );
    _write_word( CB_ROOT_OFS, 0 );
    _write_word( CB_FREE_OFS, HEADER_SIZE );
    _write_word( CB_DATA_FREE_OFS, _indexFileSize );

    commit_transaction();
}

inline void ptree::_write_word( uint32_t ofs, uint32_t val )
{
    _write_journal( ofs, sizeof(val) );

    ck_byte_ptr p = _memoryMap->map();
    p += ofs;
    uint32_t word = htonl( val );
    p.write<uint32_t>( word );
}

inline void ptree::_write_dword( uint32_t ofs, int64_t val )
{
    _write_journal( ofs, sizeof(val) );

    ck_byte_ptr p = _memoryMap->map();
    p += ofs;
    int64_t dword = x_htonll( val );
    p.write<int64_t>( dword );
}

inline void ptree::_write_byte( uint32_t ofs, uint8_t val )
{
    _write_journal( ofs, sizeof(val) );

    ck_byte_ptr p = _memoryMap->map();
    p += ofs;
    p.write<uint8_t>( val );
}

inline void ptree::_write_bytes( uint32_t ofs, uint8_t* src, uint32_t size )
{
    _write_journal( ofs, size );

    ck_byte_ptr p = _memoryMap->map();
    p += ofs;
    p.bulk_write( src, size );
}

inline uint32_t ptree::_read_word( uint32_t ofs )
{
    ck_byte_ptr p = _memoryMap->map();
    p += ofs;
    uint32_t word = p.consume<uint32_t>();
    return ntohl( word );
}

inline int64_t ptree::_read_dword( uint32_t ofs )
{
    ck_byte_ptr p = _memoryMap->map();
    p += ofs;
    int64_t dword = p.consume<int64_t>();
    return x_ntohll( dword );
}

inline uint8_t ptree::_read_byte( uint32_t ofs )
{
    ck_byte_ptr p = _memoryMap->map();
    p += ofs;
    return p.consume<uint8_t>();
}

uint8_t ptree::_height( uint32_t ofs )
{
    return (ofs > 0) ? _read_byte( ofs + NODE_HEIGHT_OFS ) : 0;
}

int ptree::_balance_factor( uint32_t ofs )
{
    int fac = _height( _read_word( ofs + NODE_RIGHT_OFS ) ) - _height( _read_word( ofs + NODE_LEFT_OFS ) );
    return fac;
}

void ptree::_fix_height( uint32_t ofs )
{
    uint8_t hl = _height( _read_word( ofs + NODE_LEFT_OFS ) );
    uint8_t hr = _height( _read_word( ofs + NODE_RIGHT_OFS ) );
    _write_byte( ofs + NODE_HEIGHT_OFS, (hl>hr ? hl : hr) + 1 );
}

uint32_t ptree::_rotate_right( uint32_t p )
{
    // first get the parent of the node we're going to rotate around...
    uint32_t p_parent = _read_word( p + NODE_PARENT_OFS );

    // This is a righ rotation, so get our left child...
    uint32_t q = _read_word( p + NODE_LEFT_OFS );
    // Our left childs parent points to our rotation points parent...
    _write_word( q + NODE_PARENT_OFS, p_parent );

    // If our new parent has any right children, tell them who their new parent is...
    if( _read_word( q + NODE_RIGHT_OFS ) > 0 )
        _write_word( _read_word( q + NODE_RIGHT_OFS ) + NODE_PARENT_OFS, p );

    // Our old root gets our new roots right children assigned to its left
    _write_word( p + NODE_LEFT_OFS, _read_word( q + NODE_RIGHT_OFS ) );
    // Our old root gets a new parent (our new root)
    _write_word( p + NODE_PARENT_OFS, q );
    // Our new roots right is assigned our old root
    _write_word( q + NODE_RIGHT_OFS, p );
    _fix_height( p );
    _fix_height( q );
    return q;
}

uint32_t ptree::_rotate_left( uint32_t q )
{
    uint32_t q_parent = _read_word( q + NODE_PARENT_OFS );

    uint32_t p = _read_word( q + NODE_RIGHT_OFS );
    _write_word( p + NODE_PARENT_OFS, q_parent );

    if( _read_word( p + NODE_LEFT_OFS ) > 0 )
        _write_word( _read_word( p + NODE_LEFT_OFS ) + NODE_PARENT_OFS, q );

    _write_word( q + NODE_RIGHT_OFS, _read_word( p + NODE_LEFT_OFS ) );
    _write_word( q + NODE_PARENT_OFS, p );
    _write_word( p + NODE_LEFT_OFS, q );
    _fix_height( q );
    _fix_height( p );
    return p;
}

uint32_t ptree::_balance( uint32_t p )
{
    _fix_height( p );
    if( _balance_factor( p ) == 2 )
    {
        if( _balance_factor( _read_word( p + NODE_RIGHT_OFS ) ) < 0 )
            _write_word( p + NODE_RIGHT_OFS, _rotate_right( _read_word( p + NODE_RIGHT_OFS ) ) );
        return _rotate_left( p );
    }
    if( _balance_factor( p ) == -2 )
    {
        if( _balance_factor( _read_word( p + NODE_LEFT_OFS ) ) > 0 )
            _write_word( p + NODE_LEFT_OFS, _rotate_left( _read_word( p + NODE_LEFT_OFS ) ) );
        return _rotate_right( p );
    }
    return p;
}

uint32_t ptree::_create_node( int64_t key, uint32_t parent, uint8_t* src, uint32_t size )
{
    uint32_t dataFreeOfs = _read_word( CB_DATA_FREE_OFS );

    uint32_t newNode = _read_word( CB_FREE_OFS );

    if( newNode + NODE_SIZE >= dataFreeOfs )
        CK_THROW(("Database Full!"));

    _write_word( CB_FREE_OFS, newNode + NODE_SIZE );

    uint8_t tempNode[NODE_SIZE];
    ck_byte_ptr p( tempNode, NODE_SIZE );

    int64_t dword = x_htonll( key );
    p.write<int64_t>( dword );             // key
    p.write<uint8_t>( 1 );                 // height
    p.write<uint8_t>( NODE_FLAG_USED );    // flags
    p.write<uint8_t>( 0 );                 // reserved
    p.write<uint8_t>( 0 );                 // reserved
    p.write<uint32_t>( 0 );                // left
    p.write<uint32_t>( 0 );                // right
    uint32_t word = htonl( parent );
    p.write<uint32_t>( word );             // parent
    uint32_t dataRecOfs = _write_data_record( src, size );
    word = htonl( dataRecOfs );
    p.write<uint32_t>( word );                // node rec ofs
    word = htonl( size );
    p.write<uint32_t>( word );                // node rec ofs size

    _write_bytes( newNode, tempNode, NODE_SIZE );

    return newNode;
}

uint32_t ptree::_write_data_record( uint8_t* src, uint32_t size )
{
    uint32_t freeOfs = _read_word( CB_FREE_OFS );

    uint32_t dataFreeOfs = _read_word( CB_DATA_FREE_OFS );

    if( (dataFreeOfs - size) <= freeOfs )
        CK_THROW(("Database Full!"));

    dataFreeOfs -= size;
    _write_word( CB_DATA_FREE_OFS, dataFreeOfs );

    _write_bytes( dataFreeOfs, src, size );

    return dataFreeOfs;
}

uint32_t ptree::_insert( uint32_t p, uint32_t parent, int64_t key, uint8_t* src, uint32_t size  )
{
    if( p == 0 )
        return _create_node( key, parent, src, size );
    if( key < _read_dword( p + NODE_KEY_OFS ) )
    {
        uint32_t insertRet = _insert( _read_word( p + NODE_LEFT_OFS ), p, key, src, size );
        _write_word( p + NODE_LEFT_OFS, insertRet );
    }
    else if( key > _read_dword( p + NODE_KEY_OFS ) )
    {
        uint32_t insertRet = _insert( _read_word( p + NODE_RIGHT_OFS ), p, key, src, size );
        _write_word( p + NODE_RIGHT_OFS, insertRet );
    }
    else CK_THROW(("Duplicate Key!"));

    return _balance( p );
}

uint32_t ptree::_find_min( uint32_t p )
{
    uint32_t left = _read_word( p + NODE_LEFT_OFS );
    return (left > 0) ? _find_min( left ) : p;
}

uint32_t ptree::_remove_min( uint32_t p )
{
    if( _read_word( p + NODE_LEFT_OFS ) == 0 )
        return _read_word( p + NODE_RIGHT_OFS );
    _write_word( p + NODE_LEFT_OFS, _remove_min( _read_word( p + NODE_LEFT_OFS ) ) );
    return _balance( p );
}

uint32_t ptree::_remove( uint32_t p, int64_t k )
{
    if( p == 0 ) return 0;
    if( k < _read_dword( p + NODE_KEY_OFS ) )
        _write_word( p + NODE_LEFT_OFS, _remove( _read_word( p + NODE_LEFT_OFS ), k ) );
    else if( k > _read_dword( p + NODE_KEY_OFS ) )
        _write_word( p + NODE_RIGHT_OFS, _remove( _read_word( p + NODE_RIGHT_OFS ), k ) );
    else
    {
        uint32_t q = _read_word( p + NODE_LEFT_OFS );
        uint32_t r = _read_word( p + NODE_RIGHT_OFS );
        uint32_t targetParent = _read_word( p + NODE_PARENT_OFS );

        // eventually, if flags actually has multiple fields we cant do this this way!
        _write_byte( p + NODE_FLAGS_OFS, 0 );

        if( q != 0 )
            _write_word( q + NODE_PARENT_OFS, targetParent );     // **********

        // if our target node has no right subtree, return our left subtree...
        if( r == 0 )
            return q;

        // else find the smallest key in our right subtree...
        uint32_t min = _find_min( r );

        uint32_t removedMin = _remove_min( r );
        _write_word( removedMin + NODE_PARENT_OFS, min );     // **********

        _write_word( min + NODE_RIGHT_OFS, removedMin );

        _write_word( q + NODE_PARENT_OFS, min );              // **********

        _write_word( min + NODE_LEFT_OFS, q );
        return _balance( min );
    }
    return _balance( p );
}

void ptree::_write_journal( uint32_t ofs, uint32_t size )
{
    if( !_journalFile )
        CK_THROW(("No journal!"));

    uint32_t pageIndex = ofs / CB_PAGE_SIZE;

    if( _dirtyPages.find( pageIndex ) == _dirtyPages.end() )
    {
        uint32_t pageOffset = pageIndex * CB_PAGE_SIZE;

        // Journal Entry
        //     uint32_t  ofs
        //     uint32_t  size
        //     uint8_t[] bytes

        uint32_t word = htonl( pageOffset );
        fwrite( &word, 1, sizeof( uint32_t ), _journalFile );
        word = htonl( CB_PAGE_SIZE );
        fwrite( &word, 1, sizeof( uint32_t ), _journalFile );

        uint8_t* src = (uint8_t*)_memoryMap->map();
        src += pageIndex * CB_PAGE_SIZE;

        fwrite( src, 1, CB_PAGE_SIZE, _journalFile );
        printf("dirty page = %u\n",pageIndex);
        ck_filecommit( _journalFile );

        _dirtyPages[pageIndex] = true;
    }
}

ptree::iterator ptree::_begin( uint32_t ofs )
{
    if( ofs == 0 )
        return ptree::iterator( ptree::iterator::END, this );

    uint32_t left = _read_word( ofs + NODE_LEFT_OFS );
    if( left > 0 )
        return _begin( left );
    else return ptree::iterator( ofs, this );
}

ptree::iterator ptree::_find( uint32_t ofs, int64_t key )
{
    if( ofs == 0 )
        return ptree::iterator( ptree::iterator::END, this );

    int64_t nodeKey = _read_dword( ofs + NODE_KEY_OFS );

    if( key < nodeKey )
        return _find( _read_word( ofs + NODE_LEFT_OFS ), key );
    else if( key > nodeKey )
        return _find( _read_word( ofs + NODE_RIGHT_OFS ), key );
    else return iterator( ofs, this );
}
