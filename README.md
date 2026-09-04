## tdb

tdb is a small, educational disk-based B+tree. It aims to keep the tree and
durability algorithms readable while still providing behavior that is useful in
real programs.

### Goals

A database uses fixed 4 KiB pages. Internal pages contain separator keys and
child page numbers; leaf pages contain key/value pairs and links to the next
leaf. The public API currently supports signed 64-bit keys and values.

Writes are serialized within the process. Readers take a shared lock, allowing
multiple simultaneous readers while ensuring that no reader observes a partial
write.

### Durability

Each mutation is a small redo transaction:

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
- Each public mutation is its own durable transaction; batching is future work.
