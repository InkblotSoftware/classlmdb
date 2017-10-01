/*  =========================================================================
    lmdbcur - Manager for an LMDB database-traversal cursor.

    Copyright (c) 2017 Inkblot Software Limited.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    lmdbcur - Manager for an LMDB database-traversal cursor.
@discuss
@end
*/

#include "classlmdb_classes.h"

#include "logging.h"

//  Structure of our class

struct _lmdbcur_t {
    MDB_cursor *handle;

    // Changed on cursor move
    MDB_val mkey;
    MDB_val mval;

    // Was this cur created in a _fromkey() ctr?
    bool is_fromkey;

    // When we fetched the first k/v pair during construction, did we find one?
    // We need this to check whether the _fromkey() ctr matched.
    bool did_first_exist;
};


//  --------------------------------------------------------------------------
//  Create a new lmdbcur


lmdbcur_t *
s_new_withcop (lmdbdbi_t *dbi, lmdbtxn_t *txn,
               const void *key, size_t key_size,
               MDB_cursor_op cop)
{
    assert (dbi);
    assert (txn);

    lmdbcur_t *self = (lmdbcur_t *) zmalloc (sizeof (lmdbcur_t));
    assert (self);

    // We are temporarily pointing to data the caller owns; fixed below.
    // Note that LMDB requires casting away const, but doesn't mutate.
    self->mkey.mv_data = (void*)key;
    self->mkey.mv_size = key_size;

    int err = 1;
    
    err = mdb_cursor_open (lmdbtxn_handle (txn), lmdbdbi_handle (dbi),
                           &self->handle);
    if (err)
        goto fail;

    // We die on any error except not finding a first item
    err = mdb_cursor_get (self->handle, &self->mkey, &self->mval, cop);
    if (err == MDB_NOTFOUND)
        goto no_match;
    else
    if (err)
        goto fail;
    
    // The caller may have supplied a cop that gets the value as well as
    // positioning the cursor, but we don't know they have, so call this to be sure.
    err = mdb_cursor_get (self->handle, &self->mkey, &self->mval, MDB_GET_CURRENT);
    if (err == MDB_NOTFOUND)
        goto no_match;
    else
    if (err)
        goto fail;

    self->did_first_exist = true;
    return self;
    
 no_match:
    // Failed to find any k/v pairs matching the key req
    self->mkey = (MDB_val) {0};
    self->mval = (MDB_val) {0};
    self->did_first_exist = false;
    return self;
    
 fail:
    // An error prevented us constructing the lmdbcur at all
    lmdbcur_destroy (&self);
    return NULL;
}

lmdbcur_t *
lmdbcur_new_overall (lmdbdbi_t *dbi, lmdbtxn_t *txn)
{
    return s_new_withcop (dbi, txn, NULL, 0, MDB_FIRST);
}

lmdbcur_t *
lmdbcur_new_fromkey (lmdbdbi_t *dbi, lmdbtxn_t *txn,
                     const void *key, size_t key_size)
{
    lmdbcur_t *res = s_new_withcop (dbi, txn, key, key_size, MDB_SET);
    if (res)
        res->is_fromkey = true;
    return res;
}

lmdbcur_t *
lmdbcur_new_gekey (lmdbdbi_t *dbi, lmdbtxn_t *txn,
                   const void *key, size_t key_size)
{
    return s_new_withcop (dbi, txn, key, key_size, MDB_SET_RANGE);
}


//  --------------------------------------------------------------------------
//  Check a valid key was found

bool
lmdbcur_matched (lmdbcur_t *self)
{
    assert (self);
    assert (self->is_fromkey);// || self->is_gekey);
    return self->did_first_exist;
}


//  --------------------------------------------------------------------------
//  Destroy the lmdbcur

void
lmdbcur_destroy (lmdbcur_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        lmdbcur_t *self = *self_p;
        //  free class properties here
        mdb_cursor_close (self->handle);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  --------------------------------------------------------------------------
//  Moving the cursor

int
lmdbcur_next (lmdbcur_t *self)
{
    assert (self);
    if (self->is_fromkey)
        assert (lmdbcur_matched (self));

    int err = 1;
    err = mdb_cursor_get (self->handle, &self->mkey, &self->mval, MDB_NEXT);
    if (err)
        goto die;
    err = mdb_cursor_get (self->handle, &self->mkey, &self->mval, MDB_GET_CURRENT);
    if (err)
        goto die;

    return 0;

 die:
    self->mkey.mv_data = NULL;
    self->mkey.mv_size = 0;
    self->mval.mv_data = NULL;
    self->mval.mv_size = 0;
    return -1;
}


//  --------------------------------------------------------------------------
//  Accessing keys and values the cursor is pointing to

lmdbspan
lmdbcur_key (lmdbcur_t *self)
{
    assert (self);
    if (self->is_fromkey)
        assert (lmdbcur_matched (self));
                
    return (lmdbspan){ .data = self->mkey.mv_data, .size = self->mkey.mv_size };
}

lmdbspan
lmdbcur_val (lmdbcur_t *self)
{
    assert (self);
    if (self->is_fromkey)
        assert (lmdbcur_matched (self));
    
    return (lmdbspan){ .data = self->mval.mv_data, .size = self->mval.mv_size };
}


//  --------------------------------------------------------------------------
//  Self test of this class

// Does a span contain exactly the following string?
static bool
s_span_is_str (lmdbspan sp, const char *str)
{
    return sp.size == strlen (str) + 1
           && streq (sp.data, str);
}

void
lmdbcur_test (bool verbose)
{
    printf (" * lmdbcur: ");

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


    char *test_db_path = zsys_sprintf ("%s/%s", SELFTEST_DIR_RW, "LMDBCUR_TEST_DB.db");
    if (zsys_file_exists (test_db_path))
        zsys_file_delete (test_db_path);
    lmdbenv_t *env = lmdbenv_new (test_db_path);
    assert (env);
    zstr_free (&test_db_path);

    lmdbdbi_t *dbi = lmdbdbi_new (env, "some_great_db");
    assert (dbi);

    // -- Write some test data to the db
    {
        lmdbtxn_t *txn = lmdbtxn_new_rdrw (env);
        assert (txn);

        int err = 1;
        err = lmdbdbi_put_strstr (dbi, txn, "cat", "felix");
        assert (!err);
        err = lmdbdbi_put_strstr (dbi, txn, "dog", "rover");
        assert (!err);
        err = lmdbdbi_put_strstr (dbi, txn, "00", "zeroes!");
        assert (!err);

        err = lmdbtxn_commit (txn);
        assert (!err);

        lmdbtxn_destroy (&txn);
    }
    if (verbose)
        log ("Wrote data to db");

    // -- Traverse whole db
    {
        lmdbtxn_t *txn = lmdbtxn_new_rdonly (env);
        assert (txn);
        lmdbcur_t *cur = lmdbcur_new_overall (dbi, txn);
        assert (cur);
        int rc = 1;

        // Test pair the cursor starts at is right (lexical sorting)
        assert (s_span_is_str (lmdbcur_key (cur), "00"));
        assert (s_span_is_str (lmdbcur_val (cur), "zeroes!"));

        // And the next one
        rc = lmdbcur_next (cur);
        assert (!rc);
        assert (s_span_is_str (lmdbcur_key (cur), "cat"));
        assert (s_span_is_str (lmdbcur_val (cur), "felix"));

        // And the last one
        rc = lmdbcur_next (cur);
        assert (!rc);
        assert (s_span_is_str (lmdbcur_key (cur), "dog"));
        assert (s_span_is_str (lmdbcur_val (cur), "rover"));

        // Now check we run off the end
        rc = lmdbcur_next (cur);
        assert (rc);

        lmdbcur_destroy (&cur);
        lmdbtxn_destroy (&txn);
    }
    if (verbose)
        log ("Full-db traversal succeeded");

    // -- Now check fromkey behaviour on same data
    {
        lmdbtxn_t *txn = lmdbtxn_new_rdonly (env);
        assert (txn);
        lmdbcur_t *cur = lmdbcur_new_fromkey (dbi, txn, "cat", strlen ("cat") + 1);
        assert (cur);
        assert (lmdbcur_matched (cur));
        int rc = 1;

        // Test pair the cursor starts at is right
        assert (s_span_is_str (lmdbcur_key (cur), "cat"));
        assert (s_span_is_str (lmdbcur_val (cur), "felix"));

        // And the next one
        rc = lmdbcur_next (cur);
        assert (!rc);
        assert (s_span_is_str (lmdbcur_key (cur), "dog"));
        assert (s_span_is_str (lmdbcur_val (cur), "rover"));

        // Now check we run off the end
        rc = lmdbcur_next (cur);
        assert (rc);

        // Finally check that making a cur with a bad key fails
        lmdbcur_t *cur_badkey = lmdbcur_new_fromkey (dbi, txn, "XXX", strlen ("XXX") + 1);
        assert (cur_badkey);
        assert (! lmdbcur_matched (cur_badkey));
        lmdbcur_destroy (&cur_badkey);

        // Cleanup
        lmdbcur_destroy (&cur);
        lmdbtxn_destroy (&txn);
    }
    if (verbose)
        log ("Fromkey cursor traversed correct set");

    // -- And gekey behaviour
    {
        lmdbtxn_t *txn = lmdbtxn_new_rdonly (env);
        assert (txn);
        lmdbcur_t *cur = lmdbcur_new_gekey (dbi, txn, "1", strlen ("1") + 1);
        assert (cur);
        int rc = 1;

        // Test pair the cursor starts at is right
        assert (s_span_is_str (lmdbcur_key (cur), "cat"));
        assert (s_span_is_str (lmdbcur_val (cur), "felix"));

        // And the next one
        rc = lmdbcur_next (cur);
        assert (!rc);
        assert (s_span_is_str (lmdbcur_key (cur), "dog"));
        assert (s_span_is_str (lmdbcur_val (cur), "rover"));

        // Now check we run off the end
        rc = lmdbcur_next (cur);
        assert (rc);

        // Finally check that making a cur with nothing above gives empty set
        char *key = "dox";
        lmdbcur_t *cur_badkey = lmdbcur_new_gekey (dbi, txn, key, strlen (key) + 1);
        assert (cur_badkey);
        assert (! lmdbspan_valid (lmdbcur_key (cur_badkey)));
        assert (! lmdbspan_valid (lmdbcur_val (cur_badkey)));
        rc = lmdbcur_next (cur_badkey);
        assert (rc == -1);
        assert (! lmdbspan_valid (lmdbcur_key (cur_badkey)));
        assert (! lmdbspan_valid (lmdbcur_val (cur_badkey)));
        lmdbcur_destroy (&cur_badkey);

        lmdbcur_destroy (&cur);
        lmdbtxn_destroy (&txn);
    }
    if (verbose)
        log ("GEKey cursor traversed correct set");

    // -- Also check ordering works for intkey data
    {
        //lmdbdbi_t *dbiik = lmdbenv_makedbi_intkeys (env, "ik_db");
        lmdbdbi_t *dbiik = lmdbdbi_new_intkeys (env, "ik_db");
        assert (dbiik);
        lmdbtxn_t *txn = lmdbtxn_new_rdrw (env);
        assert (txn);

        // On lexical sorting num1 is first, on numeric num2 is first.
        uint32_t key1 = 111;
        uint32_t key2 = 78;

        double val1 = 33.33;
        double val2 = 44.44;

        int rc = 1;

        rc = lmdbdbi_put_ui32 (dbiik, txn, key1, &val1, sizeof (val1));
        assert (!rc);
        rc = lmdbdbi_put_ui32 (dbiik, txn, key2, &val2,  sizeof (val2));
        assert (!rc);

        lmdbcur_t *cur = lmdbcur_new_overall (dbiik, txn);
        assert (lmdbspan_asui32 (lmdbcur_key (cur))
                == key2);

        rc = lmdbcur_next (cur);
        assert (!rc);
        assert (lmdbspan_asui32 (lmdbcur_key (cur))
                == key1);

        rc = lmdbcur_next (cur);
        assert (rc);

        lmdbcur_destroy (&cur);
        lmdbtxn_destroy (&txn);
        lmdbdbi_destroy (&dbiik);
    }
    if (verbose)
        log ("Intkey key ordering was correct")

    // Ends

    lmdbdbi_destroy (&dbi);
    lmdbenv_destroy (&env);


    //  @end
    printf ("OK\n");
}
