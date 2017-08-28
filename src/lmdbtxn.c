/*  =========================================================================
    lmdbtxn - Manager for an LMDB transaction

    Copyright (c) 2017 Inkblot Software Limited.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    lmdbtxn - Manager for an LMDB transaction
@discuss
@end
*/

#include "classlmdb_classes.h"

#include "logging.h"

//  Structure of our class

struct _lmdbtxn_t {
    // We NULL this out on commit or abort
    MDB_txn *handle;
    bool is_rdonly;
};


//  --------------------------------------------------------------------------
//  Create a new lmdbtxn

// helper
static lmdbtxn_t *
s_lmdbtxn_new_withflags (lmdbenv_t *env, unsigned int flags)
{
    assert (env);
    
    lmdbtxn_t *self = (lmdbtxn_t *) zmalloc (sizeof (lmdbtxn_t));
    assert (self);

    int err = mdb_txn_begin (lmdbenv_handle (env), NULL, flags, &self->handle);
    if (err)
        lmdbtxn_destroy (&self);
    
    return self;
}

lmdbtxn_t *
lmdbtxn_new_rdonly (lmdbenv_t *env)
{
    assert (env);
    lmdbtxn_t *self = s_lmdbtxn_new_withflags (env, MDB_RDONLY);

    if (self)
        self->is_rdonly = true;

    return self;
}

lmdbtxn_t *
lmdbtxn_new_rdrw (lmdbenv_t *env)
{
    assert (env);
    lmdbtxn_t *self = s_lmdbtxn_new_withflags (env, 0);

    if (self)
        self->is_rdonly = false;

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the lmdbtxn

void
lmdbtxn_destroy (lmdbtxn_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        lmdbtxn_t *self = *self_p;

        if (self->handle) {
            mdb_txn_abort (self->handle);
            self->handle = NULL;
        }

        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Commit

int
lmdbtxn_commit (lmdbtxn_t *self)
{
    assert (self);
    if (!self->handle)
        return -1;
    
    int err = mdb_txn_commit (self->handle);
    self->handle = NULL;
    return err;
}


//  --------------------------------------------------------------------------
//  Accessors

MDB_txn *
lmdbtxn_handle (lmdbtxn_t *self)
{
    assert (self);
    return self->handle;
}

bool
lmdbtxn_rdonly (lmdbtxn_t *self)
{
    assert (self);
    return self->is_rdonly;
}


//  --------------------------------------------------------------------------
//  Self test of this class

void
lmdbtxn_test (bool verbose)
{
    printf (" * lmdbtxn: ");

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


    char *test_db_path = zsys_sprintf ("%s/%s", SELFTEST_DIR_RW, "LMDBTXN_TEST_DB.db");
    if (zsys_file_exists (test_db_path))
        zsys_file_delete (test_db_path);

    lmdbenv_t *env = lmdbenv_new (test_db_path);
    assert (env);

    {  // rw
        lmdbtxn_t *txn = lmdbtxn_new_rdrw (env);
        assert (txn);
        assert (lmdbtxn_handle (txn));
        assert (! lmdbtxn_rdonly (txn));
        
        int err = 0;
        
        err = lmdbtxn_commit (txn);
        assert (!err);
        assert (!lmdbtxn_handle (txn));
        
        err = lmdbtxn_commit (txn);
        assert (err);

        lmdbtxn_destroy (&txn);
    }
    if (verbose)
        log ("rdonly txn tests passed");

    {  // ro
        lmdbtxn_t *txn = lmdbtxn_new_rdonly (env);
        assert (txn);
        assert (lmdbtxn_handle (txn));
        assert (lmdbtxn_rdonly (txn));

        lmdbtxn_destroy (&txn);
    }
    if (verbose)
        log ("rdrw txn tests passed");
        
    lmdbenv_destroy (&env);
    
    zstr_free (&test_db_path);    

    
    //  @end
    printf ("OK\n");
}
