# classlmdb
Object-oriented C interface for the LMDB database


Rationale
---------

LMDB is the fastest database for most read-biased workloads, offering zero-copy
access to data, full ACID semantics, and MVCC for non-blocking concurrent
read access.

However, the emphasis on performance and generality in LMDB's API can make
common and seemingly simple uses surprisingly difficult, requiring the user
to learn, hold in their head and reason about a fairly large number of
LMDB-specific details.

This project attempts to improve the situation, exposing a trivially simple-to-use
object-oriented API which surfaces the key functionality in an easy-to-explore
way. (Doubtless there are other central LMDB patterns of use not currently included;
we'll add these to the library as it becomes obvious they make sense.)

We're also specifically trying to address the case where you're programming in
a higher level language with a C FFI, and while you could spend days writing
an idiomatic interface wrapping the LMDB API in your own language, you'd
much rather just get a library off the shelf that does 95% or all of the
work for you. Just sprinkle a little bit of RAII and you're golden, etc.


Example
-------

```c
// #include <classlmdb.h>

// An 'environment' manages a file on disk
lmdbenv_t *env = lmdbenv_new ("testdb.db");
assert (env);

// A 'database' is a named collection of key/val pairs within an LMDB file;
// we access it through a 'database interface'
lmdbdbi_t *dbi = lmdbdbi_new (env, "pets_data");
assert (dbi);

int rc = 1;

// Put some data
{
    // Open a write trasaction
    lmdbtxn_t *txn = lmdbtxn_new_rdrw (env);
    assert (txn);

    // PUT a string->string key/val pair (including the terminating NULLs)
    rc = lmdbdbi_put_strstr (dbi, txn, "cat", "felix");
    assert (!rc);

    // PUT a uint32_t -> anything key/val pair
    double num = 999.999;
    rc = lmdbdbi_put_ui32 (dbi, txn, 222, &num, sizeof (num));
    assert (!rc);

    // Write changes to disk
    rc = lmdbtxn_commit (txn);
    assert (!rc);

    lmdbtxn_destroy (&txn);
}
   
// Read it back
{
    // We only need a read transaction this time
    lmdbtxn_t *txn = lmdbtxn_new_rdonly (env);
    assert (txn);

    // Get a value with the given string key (lmdbspans give zero copy
    // access to data currently stored in the DB)
    lmdbspan r1 = lmdbdbi_get_str (dbi, txn, "cat");
    assert (lmdbspan_valid (r1));
    // We know the value is a string, so use the lmdbspan string cast helper
    const char *r1_str = lmdbspan_asstr (r1);
    assert (streq (r1_str, "felix"));

    // Similar, but for uint32_t -> double
    lmdbspan r2 = lmdbdbi_get_ui32 (dbi, txn, 222);
    assert (lmdbspan_valid (r2));
    assert (lmdbspan_size (r2) == sizeof (double));
    assert (lmdbspan_asdouble (r2) == 999.999);

    lmdbtxn_destroy (&txn);
}

lmdbdbi_destroy (&dbi);
lmdbenv_destroy (&env);
```


Core classes
------------

__lmdbenv__ - an *LMDB Environment* manages an LMDB storage file on disk.
Everything starts by creating one of these.

__lmdbdbi__ - a *Database Interface* manages a named collecion of key/value
pairs inside the LMDB file/environment. This is what LMDB calls a *database* -
a database is *not* a file on disk.

__lmdbtxn__ - a *Transaction* allows you to interact with a database. They can
be read-only or read-write; only use the latter if you *must* change data,
as more write transactions can have a big impact on concurrent write performance.
Make sure also to close transactions as soon as possible, as very long running
transactions can cause file size bloat.
Note that values returned within a transaction are only valid up to its closing.

__lmdbcur__ - a *Cursor* lets you traverse subsets of data in a database
sequentially. You need this e.g. if you don't already know what's there.

__lmdbspan__ - an *LMDB Span* is a view into an array of immutable data curently
stored in the LMDB file, specifically the key or value of a stored pair.
Since instances of this class don't own the data they're always copied by value.
As above, note that spans are only valid until the transaction used to obtain
them is closed.


Installation and usage
----------------------

Currently we build and test on Linux, but it should be very easy to get
this library working on Windows, thanks to the use of zproject and czmq.

First install the following libraries:

- [czmq](https://github.com/zeromq/czmq/) which is very likely in your
  distro's package repositories, or can quite easily be installed from
  source

- [LMDB](https://github.com/LMDB/lmdb) which is also very likely in
  your distro's package repositores, or which can also be very easily
  installed from source

Then you can use either autotools or cmake with this project, e.g.
for the former:

```
./autogen.sh
./configure
make
sudo make install
```


Caveats
-------

__Alignment__: LMDB by default only guarantees that stored data - keys and values -
are aligned to two byte boundaries. If you want more, make the size of every
key and every value in a database a multiple of the alignment you want, and
you'll get it. But note that modern Intel processors don't impose a cost on
misaligned access.

__Concurrency__: Transactions are private to the thread that created them.
Call all lmdbenv_xxx methods from the thread that first opened the lmdbenv
they're working on. And don't open two lmdbenv's pointing at the same file
at the same time, though I don't know quite why you'd want to do that.


Ownership and license
---------------------

Copyright (c) 2017 Inkblot Software Limited.

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

(Note that the MPLv2 allows you to include this library in your own software
without changing the license, but modifications of the library itself are
bound to the same license.)


Full API
--------

__lmdbenv__

```c
//  Ctr. Accesses the LMDB file at the given path, creates if not present.
//  Assumes default limits of max file size = 1GB and max number of named
//  DBs in the file = 10.
//  Returns NULL on error.
CLASSLMDB_EXPORT lmdbenv_t *
    lmdbenv_new (const char *path);

//  As simple constructor, but lets you control the max filesize and max
//  named DB count limits.
//  Note that we round max_size up to the nearest multiple of 4096.
CLASSLMDB_EXPORT lmdbenv_t *
    lmdbenv_new_withlimits (const char *path, size_t max_size, size_t max_dbs);

//  Destroy the lmdbenv.
CLASSLMDB_EXPORT void
    lmdbenv_destroy (lmdbenv_t **self_p);

//  Return a pointer to the underlying MDB_env instance.
//  BEWARE: this is an escape hatch for people that *really* need it; if you
//  need more functionality then prefer to extend this library to contain it.
CLASSLMDB_EXPORT MDB_env *
    lmdbenv_handle (lmdbenv_t *self);
```

__lmdbdbi__

```c
//  Create a named database interface object, using the provided lmdbenv.
//  Note that a database is not a file, but a key/val collection inside one.
//  Creates the database if it does not already exist.
//  If you want the file to contain exactly one database, pass NULL for name.
//  Returns NULL on failure.
CLASSLMDB_EXPORT lmdbdbi_t *
    lmdbdbi_new (lmdbenv_t *env, const char *name);

//  As simple ctr, but the DB must only hold keys of unsigned int or size_t,
//  all keys having the same size, and with sequential traversal (e.g. with
//  a cursor) following their natural numeric order, not doing a per-byte
//  comparison like LMDB usually does.
CLASSLMDB_EXPORT lmdbdbi_t *
    lmdbdbi_new_intkeys (lmdbenv_t *env, const char *name);

//  Aborts the transaction if not already committed.
CLASSLMDB_EXPORT void
    lmdbdbi_destroy (lmdbdbi_t **self_p);

//  Fetch a the value from the DB with the given key.
//  Returns nullish lmdbspan (.data == NULL) if the key doesn't exist, or if an
//  error occurs (this will be becuase you supplied a duff dbi or txn).
CLASSLMDB_EXPORT lmdbspan
    lmdbdbi_get (lmdbdbi_t *self, lmdbtxn_t *txn,
                 const void *key, size_t key_size);

//  As get method, but takes a string as key.
//  NB counts the terminating NULL as part of the string.
CLASSLMDB_EXPORT lmdbspan
    lmdbdbi_get_str (lmdbdbi_t *self, lmdbtxn_t *txn, const char *key);

//  As get method, but takes an uint32_t as key.
CLASSLMDB_EXPORT lmdbspan
    lmdbdbi_get_ui32 (lmdbdbi_t *self, lmdbtxn_t *txn, uint32_t key);

//  As get method, but takes an int32_t as key.
CLASSLMDB_EXPORT lmdbspan
    lmdbdbi_get_i32 (lmdbdbi_t *self, lmdbtxn_t *txn, int32_t key);

//  Put a key/val pair to the DB.
//  Returns 0 on sucess, -1 on failure.
CLASSLMDB_EXPORT int
    lmdbdbi_put (lmdbdbi_t *self, lmdbtxn_t *txn,
                 const void *key, size_t key_size,
                 const void *value, size_t value_size);

//  As put method, but takes a string as the key.
//  NB treats the terminating NULL as part of the string.
CLASSLMDB_EXPORT int
    lmdbdbi_put_str (lmdbdbi_t *self, lmdbtxn_t *txn,
                     const char *key, const void *value,
                     size_t value_size);

//  As put method, but takes both key and val are strings.
//  NB for both strings counts the terminating NULL as part of the string.
CLASSLMDB_EXPORT int
    lmdbdbi_put_strstr (lmdbdbi_t *self, lmdbtxn_t *txn,
                        const char *key, const char *value);

//  As put method, but takes a uint32_t as key.
CLASSLMDB_EXPORT int
    lmdbdbi_put_ui32 (lmdbdbi_t *self, lmdbtxn_t *txn, uint32_t key,
                      const void *val, size_t val_size);

//  As put method, but takes an int32_t as key.
CLASSLMDB_EXPORT int
    lmdbdbi_put_i32 (lmdbdbi_t *self, lmdbtxn_t *txn, int32_t key,
                     const void *val, size_t val_size);

//  Returns true iff the instance was created as an intkeys dbi.
CLASSLMDB_EXPORT bool
    lmdbdbi_has_intkey (lmdbdbi_t *self);

//  Return a copy of the the underlying MDB_dbi.
//  BEWARE: this is an escape hatch for people that *really* need it; if you
//  need more functionality then prefer to extend this library to contain it.
CLASSLMDB_EXPORT MDB_dbi
    lmdbdbi_handle (lmdbdbi_t *self);
```

__lmdbtxn__

```c
//  Open a new read-only transaction for the provided lmdbenv.
//  Aborts (ends) the transaction on dtr call.
//  Returns NULL ir error.
CLASSLMDB_EXPORT lmdbtxn_t *
    lmdbtxn_new_rdonly (lmdbenv_t *env);

//  As new_rdonly, but opens a read-write transaction.
//  Aborts on dbr call if not already committed.
CLASSLMDB_EXPORT lmdbtxn_t *
    lmdbtxn_new_rdrw (lmdbenv_t *env);

//  Destroy the lmdbtxn.
CLASSLMDB_EXPORT void
    lmdbtxn_destroy (lmdbtxn_t **self_p);

//  Commit transaction. Only valid for rdrw lmdbtxn's.
//  Returns 0 on success, -1 on error.
CLASSLMDB_EXPORT int
    lmdbtxn_commit (lmdbtxn_t *self);

//  Is this a read-only transaction?
CLASSLMDB_EXPORT bool
    lmdbtxn_rdonly (lmdbtxn_t *self);

//  Return a pointer to the underlying MDB_txn.
//  BEWARE: this is an escape hatch for people that *really* need it; if you
//  need more functionality then prefer to extend this library to contain it.
CLASSLMDB_EXPORT MDB_txn *
    lmdbtxn_handle (lmdbtxn_t *self);
```

__lmdbcur__

```c
//  Creates a cursor that traverses all k/y pairs in the DB in ascending order.
//  Returns NULL on any error.
CLASSLMDB_EXPORT lmdbcur_t *
    lmdbcur_new_overall (lmdbdbi_t *dbi, lmdbtxn_t *txn);

//  Creates a cursor that traverses all k/y pairs in the DB in ascending order,
//  starting at the provided key. This key must exist.
//  Returns NULL on any error.
CLASSLMDB_EXPORT lmdbcur_t *
    lmdbcur_new_fromkey (lmdbdbi_t *dbi, lmdbtxn_t *txn, const void *key, size_t key_size);

//  As _fromkey ctr, but if the provided key does not exist starts iteration
//  at the next key above it ('greater than or equal to key').
//  Returns NULL on any error.
CLASSLMDB_EXPORT lmdbcur_t *
    lmdbcur_new_gekey (lmdbdbi_t *dbi, lmdbtxn_t *txn, const void *key, size_t key_size);

//  Destroy the lmdbcur.
CLASSLMDB_EXPORT void
    lmdbcur_destroy (lmdbcur_t **self_p);

//  Move the cursor to the next k/v pair in the db, in key sorted
//  ascending order.
//  Returns 0 on success, or -1 if no such key exists.
CLASSLMDB_EXPORT int
    lmdbcur_next (lmdbcur_t *self);

//  Returns the key the cursor is curently pointing to.
//  You need this when you call _next() and don't know what's there.
//  Returns nullish lmdbspan on error (!lmdspan_valid (s)), which isn't a real
//  value that can be returned from the DB.
CLASSLMDB_EXPORT lmdbspan
    lmdbcur_key (lmdbcur_t *self);

//  Like key(), but returns the value the cursor is curently pointing to.
CLASSLMDB_EXPORT lmdbspan
    lmdbcur_val (lmdbcur_t *self);

//  Return a pointer to the underlying MDB_cur we're managing.
//  BEWARE: this is an escape hatch for people that *really* need it; if you
//  need more functionality then prefer to extend this library to contain it.
CLASSLMDB_EXPORT MDB_cursor *
    lmdbcur_handle (lmdbcur_t *self);
```

__lmdbspan__

(Exposed as header-only functions)

```c
// Does the span point to valid data, rather than being a null span?
inline static bool
lmdbspan_valid (lmdbspan self);

// What is the size of the data the span is pointing to?
inline static size_t
lmdbspan_size (lmdbspan self);

// Get a pointer to the data the span is pointing to
inline static const void *
lmdbspan_data (lmdbspan self);

// Reinterpret the pointed-to data as a string.
// It's the caller's responsability to make sure they PUT null-terminated data.
inline static const char *
lmdbspan_asstr (lmdbspan self);

// Reinterpret the pointed-to data as a double, and return a copy.
// The caller must ensure the pointed-to data allows a valid conversion
// (though we have an assert that the size is right, depending on compile flags).
inline static double
lmdbspan_asdouble (lmdbspan self);

// Reinterpret the pointed-to data as a uint32_t, and return a copy.
// The caller must ensure the pointed-to data allows a valid conversion
// (though we have an assert that the size is right, depending on compile flags).
inline static uint32_t
lmdbspan_asui32 (lmdbspan self);
```