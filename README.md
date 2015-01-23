# tdb
A playground for learning about databases...

tdb is a super simple database, suitable for learning about how databases work (primarily because it is so short).
It supports atomic commit (via a journal file), and transactions (with rollback). tdb is implemented with a disk
based AVL tree primarily because I consider it to be the simplest to implement but still balanced tree type. A real
database should use B+trees or log structured merge trees.
