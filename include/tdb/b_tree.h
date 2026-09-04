#ifndef TDB_B_TREE_H
#define TDB_B_TREE_H

#include "tdb/pager.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>

class b_tree;
class b_tree_read_transaction;
class b_tree_write_transaction;
class row_store;

class b_tree_iterator
{
public:
    b_tree_iterator(const b_tree_iterator&) = delete;
    b_tree_iterator& operator=(const b_tree_iterator&) = delete;
    b_tree_iterator(b_tree_iterator&&) = delete;
    b_tree_iterator& operator=(b_tree_iterator&&) = delete;

    bool find(std::int64_t key);
    bool next();
    bool prev();

    bool valid() const noexcept;
    explicit operator bool() const noexcept;
    std::int64_t key() const;
    std::int64_t value() const;

private:
    friend class b_tree_read_transaction;
    explicit b_tree_iterator(b_tree_read_transaction& transaction);
    b_tree_iterator(b_tree_read_transaction& transaction, std::int64_t key);
    void invalidate() noexcept;

    b_tree_read_transaction* _transaction;
    std::uint64_t _leaf_page;
    std::size_t _index;
    std::int64_t _key;
    std::int64_t _value;
    bool _valid;
};

class b_tree_read_transaction
{
public:
    b_tree_read_transaction(const b_tree_read_transaction&) = delete;
    b_tree_read_transaction& operator=(const b_tree_read_transaction&) = delete;
    b_tree_read_transaction(b_tree_read_transaction&&) = delete;
    b_tree_read_transaction& operator=(b_tree_read_transaction&&) = delete;

    std::optional<std::int64_t> get(std::int64_t key);
    b_tree_iterator search(std::int64_t key);
    b_tree_iterator iterator();

private:
    friend class b_tree;
    friend class b_tree_iterator;
    friend class row_store;
    explicit b_tree_read_transaction(b_tree& tree);

    b_tree* _tree;
    std::shared_lock<std::shared_mutex> _lock;
};

class b_tree_write_transaction
{
public:
    b_tree_write_transaction(const b_tree_write_transaction&) = delete;
    b_tree_write_transaction& operator=(const b_tree_write_transaction&) = delete;
    b_tree_write_transaction(b_tree_write_transaction&&) = delete;
    b_tree_write_transaction& operator=(b_tree_write_transaction&&) = delete;

    void insert(std::int64_t key, std::int64_t value);
    void remove(std::int64_t key);
    void commit();

private:
    friend class b_tree;
    friend class row_store;
    explicit b_tree_write_transaction(b_tree& tree);
    void require_active() const;

    b_tree* _tree;
    std::unique_lock<std::shared_mutex> _lock;
    pager::transaction _transaction;
    bool _committed;
};

class b_tree
{
public:
    b_tree(const std::string& file_name, std::uint16_t min_degree);

    b_tree_read_transaction begin_read();
    b_tree_write_transaction begin_write();

    std::optional<std::int64_t> get(std::int64_t key);
    void insert(std::int64_t key, std::int64_t value);
    void remove(std::int64_t key);
    void write_dot_file(const std::string& file_name);

    static void create_db_file(const std::string& file_name);
    static void vacuum(const std::string& file_name);

private:
    friend class b_tree_iterator;
    friend class b_tree_read_transaction;
    friend class b_tree_write_transaction;
    friend class row_store;

    struct iterator_position
    {
        std::uint64_t leaf_page;
        std::size_t index;
        std::int64_t key;
        std::int64_t value;
    };

    enum class seek_mode { exact, less };
    std::optional<iterator_position> seek_unlocked(std::int64_t key, seek_mode mode);
    void insert_unlocked(pager::transaction& transaction,
                         std::int64_t key, std::int64_t value);
    void remove_unlocked(pager::transaction& transaction, std::int64_t key);

    pager _pager;
    std::uint16_t _max_keys;

    static std::shared_mutex _tree_mutex;
};

#endif
