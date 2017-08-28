#ifndef CLASSLMDB_LMDBSPAN_H_INCLUDED
#define CLASSLMDB_LMDBSPAN_H_INCLUDED

/*  =========================================================================
    lmdbspan - Immutable view onto an array of data stored in a file on disk

    Copyright (c) 2017 Inkblot Software Limited.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/


//  --------------------------------------------------------------------------
//  Body

typedef struct _lmdbspan {
    const void *data;
    const size_t size;
} lmdbspan;


//  --------------------------------------------------------------------------
//  Ctrs

// Private
inline static lmdbspan
lmdbspan_makenull ()
{
    return (lmdbspan){ .data = NULL, .size = 0};
}


//  --------------------------------------------------------------------------
//  Accessors

// Does the span point to valid data, rather than being a null span?
inline static bool
lmdbspan_valid (lmdbspan self)
{
    return self.data != NULL;
}

// What is the size of the data the span is pointing to?
inline static size_t
lmdbspan_size (lmdbspan self)
{
    return self.size;
}

// Get a pointer to the data the span is pointing to
inline static const void *
lmdbspan_data (lmdbspan self)
{
    return self.data;
}


//  --------------------------------------------------------------------------
//  Type conversions
//    UB if the span doesn't actually contain suitable data

// Reinterpret the pointed-to data as a string.
// It's the caller's responsability to make sure they PUT null-terminated data.
inline static const char *
lmdbspan_asstr (lmdbspan self)
{
    assert (self.data);
    assert (self.size >= 1);
    return (const char *) self.data;
}

// Reinterpret the pointed-to data as a double, and return a copy.
// The caller must ensure the pointed-to data allows a valid conversion
// (though we have an assert that the size is right, depending on compile flags).
inline static double
lmdbspan_asdouble (lmdbspan self)
{
    assert (self.data);
    assert (self.size == sizeof (double));
    return *((const double *) self.data);
}

// Reinterpret the pointed-to data as a uint32_t, and return a copy.
// The caller must ensure the pointed-to data allows a valid conversion
// (though we have an assert that the size is right, depending on compile flags).
inline static uint32_t
lmdbspan_asui32 (lmdbspan self)
{
    assert (self.data);
    assert (self.size == sizeof (uint32_t));
    return *((uint32_t *) self.data);
}


#endif
