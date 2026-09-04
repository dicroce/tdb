## tdb

tdb is a small, educational disk-based B+tree. It aims to keep the tree and
durability algorithms readable while still providing behavior that is useful in
real programs.

### Goals

A database uses fixed 4 KiB pages. Internal pages contain separator keys and
child page numbers; leaf pages contain key/value pairs and links to the next
leaf. The public API currently supports signed 64-bit keys and values.

`row_store` (declared in `tdb/row_store.h`) is an optional companion for opaque,
variable-sized rows. A row is
addressed by a 64-bit `row_id` containing its page, slot, and slot generation.
The slot directory grows from the front of a page while row bytes grow from the
back. Rows can therefore be compacted within a page without changing their IDs,
and the generation prevents a removed ID from aliasing a later row that reuses
the same slot. Rows currently cannot span pages.

`begin_read()` creates an explicit read transaction holding a shared lock.
Its `search()` method returns a positioned `b_tree_iterator`. An iterator can
be repositioned with `find()`, moved in key order with `next()` and `prev()`,
and inspected with `key()` and `value()`. The read transaction must outlive all
of its iterators. `get()` is a convenience point lookup.

`begin_write()` creates an exclusive write transaction. Multiple inserts and
removes can be buffered and made durable by one `commit()`. Destroying an
uncommitted write transaction discards its changes. The tree-level `insert()`
and `remove()` methods are one-operation convenience transactions.

Row-store operations take one of these transactions explicitly. This makes an
index update and its row update atomic without coupling the two structures:

```cpp
row_store rows;
auto write = tree.begin_write();
row_id id = rows.insert(write, bytes);
write.insert(key, static_cast<std::int64_t>(id));
write.commit();
```

Writes are serialized within the process. Readers take a shared lock, allowing
multiple simultaneous readers while ensuring that no reader observes a partial
write.

### Durability

Each committed write transaction uses redo logging:

1. Modified pages are buffered in memory.
2. Complete page after-images and a checksummed commit record are written to
   `<database>.wal`.
3. The WAL is flushed to durable storage.
4. The pages are applied to the database and the database is flushed.
5. The WAL is removed.

On open, a complete committed WAL is replayed idempotently. An incomplete or
corrupt WAL is discarded. Full-page logging is intentionally simple; it avoids
undo records and per-page log sequence numbers.

### Deliberate limitations

- Writer serialization is process-local, not cross-process.
- Removed pages are recycled through an on-disk free-page list. The physical
  file is not shrunk, and `vacuum()` is not implemented.
- A row must fit in one page; overflow rows and size-changing updates are not
  implemented.
