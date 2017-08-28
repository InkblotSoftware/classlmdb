/*  =========================================================================
    lmdbenv - Manager for an LMDB Environment, the in-memory interface to an LMDB file on disk

    Copyright (c) 2017 Inkblot Software Limited.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    lmdbenv - Manager for an LMDB Environment, the in-memory interface to an LMDB file on disk
@discuss
@end
*/

#include "classlmdb_classes.h"

#include "logging.h"

//  Structure of our class

struct _lmdbenv_t {
    MDB_env *handle;
};


//  --------------------------------------------------------------------------
//  Constants used in mdb_ functions

static unsigned int
s_default_open_flags = MDB_NOSUBDIR;

static mdb_mode_t
s_default_open_mode = 0664;

static size_t
s_default_mapsize = 1UL * 1024UL * 1024UL * 1024UL; // 1GB

static size_t  // max number of named dbis in the db
s_default_max_dbs = 10;


//  --------------------------------------------------------------------------
//  Create a new lmdbenv

lmdbenv_t *
lmdbenv_new (const char *path)
{
    assert (path);
    return lmdbenv_new_withlimits (path, s_default_mapsize, s_default_max_dbs);
}

lmdbenv_t *
lmdbenv_new_withlimits (const char *path, size_t max_size, size_t max_dbs)
{
    lmdbenv_t *self = (lmdbenv_t *) zmalloc (sizeof (lmdbenv_t));
    assert (self);
    int err = 0;

    err = mdb_env_create (&self->handle);
    if (err)
        goto die;

    size_t round_max_size = max_size + (4096 - (max_size % 4096)) - 4096;
    err = mdb_env_set_mapsize (self->handle, round_max_size);
    if (err)
        goto die;

    err = mdb_env_set_maxdbs (self->handle, max_dbs);
    if (err)
        goto die;

    err = mdb_env_open (self->handle, path,
                        s_default_open_flags, s_default_open_mode);
    if (err)
        goto die;

    goto cleanup_ret;

 die:
    lmdbenv_destroy (&self);
    
 cleanup_ret:
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the lmdbenv

void
lmdbenv_destroy (lmdbenv_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        lmdbenv_t *self = *self_p;

        mdb_env_close (self->handle);

        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Accessors

MDB_env *
lmdbenv_handle (lmdbenv_t *self)
{
    assert (self);
    return self->handle;
}


//  --------------------------------------------------------------------------
//  Self test of this class

void
lmdbenv_test (bool verbose)
{
    printf (" * lmdbenv: ");

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

    char *test_db_path = zsys_sprintf ("%s/%s", SELFTEST_DIR_RW, "LMDBENV_TEST_DB.db");
    if (zsys_file_exists (test_db_path))
        zsys_file_delete (test_db_path);

    lmdbenv_t *env = lmdbenv_new (test_db_path);
    assert (env);

    lmdbenv_destroy (&env);
    zstr_free (&test_db_path);

    //  @end
    printf ("OK\n");
}
