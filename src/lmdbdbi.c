/*  =========================================================================
    lmdbdbi - Manager for a named LMDB database interface, within an lmdbenv object

    Copyright (c) 2017 Inkblot Software Limited.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    lmdbdbi - Manager for a named LMDB database interface, within an lmdbenv object
@discuss
@end
*/

#include "classlmdb_classes.h"

#include "logging.h"

//  Structure of our class

struct _lmdbdbi_t {
    MDB_dbi handle;
    bool    is_intkeys;  // Was opened with intkeys?
};


//  --------------------------------------------------------------------------
//  Create a new lmdbdbi

static lmdbdbi_t *
s_makedbi_withflags (lmdbenv_t *env, const char *name, unsigned int flags)
{
    assert (env);

    lmdbdbi_t *self = (lmdbdbi_t *) zmalloc (sizeof (lmdbdbi_t));
    assert (self);

    // We need a txn to create the db, but can close it after
    lmdbtxn_t *txn = lmdbtxn_new_rdrw (env);
    if (!txn)
        goto die;

    int rc = 1;

    rc = mdb_dbi_open (lmdbtxn_handle (txn), name, flags, &self->handle);
    if (rc)
        goto die;

    rc = lmdbtxn_commit (txn);
    if (rc)
        goto die;

    goto cleanup_ret;

 die:
    lmdbdbi_destroy (&self);

 cleanup_ret:
    lmdbtxn_destroy (&txn);
    return self;
}

lmdbdbi_t *
lmdbdbi_new (lmdbenv_t *env, const char *name)
{
    assert (env);
    return s_makedbi_withflags (env, name, MDB_CREATE);
}

lmdbdbi_t *
lmdbdbi_new_intkeys (lmdbenv_t *env, const char *name)
{
    assert (env);
    lmdbdbi_t *self = s_makedbi_withflags (env, name, MDB_CREATE | MDB_INTEGERKEY);
    if (self)
        self->is_intkeys = true;
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the lmdbdbi

void
lmdbdbi_destroy (lmdbdbi_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        lmdbdbi_t *self = *self_p;

        // No need to close handle

        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  GET functions

lmdbspan
lmdbdbi_get_str (lmdbdbi_t *self, lmdbtxn_t *txn, const char *key)
{
    assert (self);
    assert (txn);
    assert (key);

    size_t key_size = strlen (key) + 1;
    return lmdbdbi_get (self, txn, key, key_size);
}

lmdbspan
lmdbdbi_get_ui32 (lmdbdbi_t *self, lmdbtxn_t *txn, uint32_t key)
{
    assert (self);
    assert (txn);

    return lmdbdbi_get (self, txn, &key, sizeof (key));
}

lmdbspan
lmdbdbi_get (lmdbdbi_t *self, lmdbtxn_t *txn, const void *key, size_t key_size)
{
    assert (self);
    assert (txn);
    assert (key);

    MDB_val mkey, mval;
    // LMDB api requires us to cast away const here, but doesn't mutate
    mkey.mv_data = (void *) key;
    mkey.mv_size = key_size;

    int err = mdb_get (lmdbtxn_handle (txn), self->handle, &mkey, &mval);
    
    assert (err == 0 || err == MDB_NOTFOUND);
    if (err)
        return lmdbspan_makenull ();
    else
        return (lmdbspan){ .data = mval.mv_data, .size = mval.mv_size };
}


//  --------------------------------------------------------------------------
//  PUT functions

int
lmdbdbi_put_str (lmdbdbi_t *self, lmdbtxn_t *txn,
                 const char *key,
                 const void *val, size_t val_size)
{
    assert (self);
    assert (txn);
    assert (key);
    assert (val);
    
    size_t key_size = strlen (key) + 1;  // include null term
    return lmdbdbi_put (self, txn, key, key_size, val, val_size);
}

int
lmdbdbi_put_strstr (lmdbdbi_t *self, lmdbtxn_t *txn,
                    const char *key, const char *val)
{
    assert (self);
    assert (txn);
    assert (key);
    assert (val);
    
    size_t key_size = strlen (key) + 1;  // inc null term
    size_t val_size = strlen (val) + 1;  // inc null term
    return lmdbdbi_put (self, txn, key, key_size, val, val_size);
}

int
lmdbdbi_put_ui32 (lmdbdbi_t *self, lmdbtxn_t *txn,
                  uint32_t key,
                  const void *val, size_t val_size)
{
    assert (self);
    assert (txn);
    return lmdbdbi_put (self, txn, &key, sizeof (key), val, val_size);
}


int
lmdbdbi_put (lmdbdbi_t *self, lmdbtxn_t *txn,
             const void *key, size_t key_size,
             const void *val, size_t val_size)
{
    assert (self);
    assert (txn);
    assert (key);
    assert (val);

    // LMDB api reqs casting away const, but doesn't mutate
    MDB_val mkey = {.mv_data = (void *) key, .mv_size = key_size};
    MDB_val mval = {.mv_data = (void *) val, .mv_size = val_size};

    int err = mdb_put (lmdbtxn_handle (txn), self->handle,
                       &mkey, &mval, 0);  // 0 is flags
    if (err)
        return -1;
    else
        return 0;
}


//  --------------------------------------------------------------------------
//  Accessors

bool
lmdbdbi_has_intkeys (lmdbdbi_t *self)
{
    assert (self);
    return self->is_intkeys;
}

MDB_dbi
lmdbdbi_handle (lmdbdbi_t *self)
{
    assert (self);
    return self->handle;
}


//  --------------------------------------------------------------------------
//  Self test of this class

void
lmdbdbi_test (bool verbose)
{
    printf (" * lmdbdbi: ");

    //  @selftest
    //  Simple create/destroy test

    // Note: If your selftest reads SCMed fixture data, please keep it in
    // src/selftest-ro; if your test creates filesystem objects, please
    // do so under src/selftest-rw. They are defined below along with a
    // usecase for the variables (assert) to make compilers happy.
    const char *SELFTEST_DIR_RO = "src/selftest-ro";
    const char *SELFTEST_DIR_RW = "src/selftest-rw";
    assert (SELFTEST_DIR_RO);
    assert (SELFTEST_DIR_RW);
    // The following pattern is suggested for C selftest code:
    //    char *filename = NULL;
    //    filename = zsys_sprintf ("%s/%s", SELFTEST_DIR_RO, "mytemplate.file");
    //    assert (filename);
    //    ... use the filename for I/O ...
    //    zstr_free (&filename);
    // This way the same filename variable can be reused for many subtests.
    //
    // Uncomment these to use C++ strings in C++ selftest code:
    //std::string str_SELFTEST_DIR_RO = std::string(SELFTEST_DIR_RO);
    //std::string str_SELFTEST_DIR_RW = std::string(SELFTEST_DIR_RW);
    //assert ( (str_SELFTEST_DIR_RO != "") );
    //assert ( (str_SELFTEST_DIR_RW != "") );
    // NOTE that for "char*" context you need (str_SELFTEST_DIR_RO + "/myfilename").c_str()

    zsys_init ();
    int rc = 1;

    char *test_db_path = zsys_sprintf ("%s/%s", SELFTEST_DIR_RW, "LMDBDBI_TEST_DB.db");
    if (zsys_file_exists (test_db_path))
        zsys_file_delete (test_db_path);

    lmdbenv_t *env = lmdbenv_new (test_db_path);
    assert (env);
    zstr_free (&test_db_path);

    // Simple db
    //lmdbdbi_t *dbisim = lmdbenv_makedbi (env, "simple_db");
    lmdbdbi_t *dbisim = lmdbdbi_new (env, "simple_db");
    assert (dbisim);
    assert (lmdbdbi_has_intkeys (dbisim) == false);

    // Intkey db
    //lmdbdbi_t *dbiik = lmdbenv_makedbi_intkeys (env, "intkey_db");
    lmdbdbi_t *dbiik = lmdbdbi_new_intkeys (env, "intkey_db");
    assert (dbiik);
    assert (lmdbdbi_has_intkeys (dbiik) == true);

    lmdbtxn_t *txn = lmdbtxn_new_rdrw (env);
    assert (txn);

    // We use this in various tests
    double dubkey = 11.11;


    // -- Test the simple db

    rc = lmdbdbi_put_strstr (dbisim, txn, "cat", "felix");
    assert (!rc);

    lmdbspan s1 = lmdbdbi_get_str (dbisim, txn, "cat");
    assert (streq (lmdbspan_asstr (s1), "felix"));

    rc = lmdbdbi_put_ui32 (dbisim, txn, 123, &dubkey, sizeof (dubkey));
    assert (!rc);

    lmdbspan r2 = lmdbdbi_get_ui32 (dbisim, txn, 123);
    assert (lmdbspan_asdouble (r2) == dubkey);

    if (verbose)
        log ("Simple db tests passed");


    // -- And the intkeys db

    rc = lmdbdbi_put_ui32 (dbiik, txn, 88, &dubkey, sizeof (dubkey));
    assert (!rc);

    lmdbspan r3 = lmdbdbi_get_ui32 (dbiik, txn, 88);
    assert (lmdbspan_asdouble (r3) == dubkey);

    if (verbose)
        log ("Intkey db tests passed");


    // -- Ends

    lmdbtxn_destroy (&txn);
    lmdbdbi_destroy (&dbisim);
    lmdbdbi_destroy (&dbiik);
    lmdbenv_destroy (&env);

    
    //  @end
    printf ("OK\n");
}
