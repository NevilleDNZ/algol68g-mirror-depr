//! @file moids-size.c
//! @author J. Marcel van der Veer
//!
//! @section Copyright
//!
//! This file is part of Algol68G - an Algol 68 compiler-interpreter.
//! Copyright 2001-2023 J. Marcel van der Veer [algol68g@xs4all.nl].
//!
//! @section License
//!
//! This program is free software; you can redistribute it and/or modify it 
//! under the terms of the GNU General Public License as published by the 
//! Free Software Foundation; either version 3 of the License, or 
//! (at your option) any later version.
//!
//! This program is distributed in the hope that it will be useful, but 
//! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
//! or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
//! more details. You should have received a copy of the GNU General Public 
//! License along with this program. If not, see [http://www.gnu.org/licenses/].

//! @section Synopsis
//!
//! Memory footprint (size) of a mode.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-genie.h"
#include "a68g-postulates.h"
#include "a68g-parser.h"
#include "a68g-options.h"
#include "a68g-optimiser.h"
#include "a68g-listing.h"

// Next are routines to calculate the size of a mode.

//! @brief Max unitings to simplout.

void max_unitings_to_simplout (NODE_T * p, int *max)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNITING) && MOID (p) == M_SIMPLOUT) {
      MOID_T *q = MOID (SUB (p));
      if (q != M_SIMPLOUT) {
        int size = moid_size (q);
        MAXIMISE (*max, size);
      }
    }
    max_unitings_to_simplout (SUB (p), max);
  }
}

//! @brief Get max simplout size.

void get_max_simplout_size (NODE_T * p)
{
  A68 (max_simplout_size) = A68_REF_SIZE;       // For anonymous SKIP
  max_unitings_to_simplout (p, &A68 (max_simplout_size));
}

//! @brief Set moid sizes.

void set_moid_sizes (MOID_T * z)
{
  for (; z != NO_MOID; FORWARD (z)) {
    SIZE (z) = moid_size (z);
    DIGITS (z) = moid_digits (z);
  }
// Next is guaranteed.
#if (A68_LEVEL >= 3)
  SIZE (M_LONG_REAL) = moid_size (M_LONG_REAL);
  DIGITS (M_LONG_REAL) = 0;
#else
  SIZE (M_LONG_REAL) = moid_size (M_LONG_REAL);
  DIGITS (M_LONG_REAL) = moid_digits (M_LONG_REAL);
#endif
  SIZE (M_LONG_LONG_REAL) = moid_size (M_LONG_LONG_REAL);
  DIGITS (M_LONG_LONG_REAL) = moid_digits (M_LONG_LONG_REAL);
  SIZEC (M_LONG_COMPLEX) = SIZE (M_LONG_REAL);
  SIZEC (M_REF_LONG_COMPLEX) = SIZE (M_LONG_REAL);
  DIGITSC (M_LONG_COMPLEX) = DIGITS (M_LONG_REAL);
  DIGITSC (M_REF_LONG_COMPLEX) = DIGITS (M_LONG_REAL);
  SIZEC (M_LONG_LONG_COMPLEX) = SIZE (M_LONG_LONG_REAL);
  SIZEC (M_REF_LONG_LONG_COMPLEX) = SIZE (M_LONG_LONG_REAL);
  DIGITSC (M_LONG_LONG_COMPLEX) = DIGITS (M_LONG_LONG_REAL);
  DIGITSC (M_REF_LONG_LONG_COMPLEX) = DIGITS (M_LONG_LONG_REAL);
}

//! @brief Moid size 2.

int moid_size_2 (MOID_T * p)
{
  if (p == NO_MOID) {
    return 0;
  } else if (EQUIVALENT (p) != NO_MOID) {
    return moid_size_2 (EQUIVALENT (p));
  } else if (p == M_HIP) {
    return 0;
  } else if (p == M_VOID) {
    return 0;
  } else if (p == M_INT) {
    return SIZE_ALIGNED (A68_INT);
  } else if (p == M_LONG_LONG_INT) {
    return (int) size_long_mp ();
  } else if (p == M_REAL) {
    return SIZE_ALIGNED (A68_REAL);
  } else if (p == M_LONG_INT) {
#if (A68_LEVEL >= 3)
    return SIZE_ALIGNED (A68_LONG_INT);
#else
    return (int) size_mp ();
#endif
  } else if (p == M_LONG_REAL) {
#if (A68_LEVEL >= 3)
    return SIZE_ALIGNED (A68_LONG_REAL);
#else
    return (int) size_mp ();
#endif
  } else if (p == M_LONG_BITS) {
#if (A68_LEVEL >= 3)
    return SIZE_ALIGNED (A68_LONG_BITS);
#else
    return (int) size_mp ();
#endif
  } else if (p == M_LONG_LONG_REAL) {
    return (int) size_long_mp ();
  } else if (p == M_BOOL) {
    return SIZE_ALIGNED (A68_BOOL);
  } else if (p == M_CHAR) {
    return SIZE_ALIGNED (A68_CHAR);
  } else if (p == M_ROW_CHAR) {
    return A68_REF_SIZE;
  } else if (p == M_BITS) {
    return SIZE_ALIGNED (A68_BITS);
  } else if (p == M_LONG_LONG_BITS) {
    return (int) size_long_mp ();
  } else if (p == M_BYTES) {
    return SIZE_ALIGNED (A68_BYTES);
  } else if (p == M_LONG_BYTES) {
    return SIZE_ALIGNED (A68_LONG_BYTES);
  } else if (p == M_FILE) {
    return SIZE_ALIGNED (A68_FILE);
  } else if (p == M_CHANNEL) {
    return SIZE_ALIGNED (A68_CHANNEL);
  } else if (p == M_FORMAT) {
    return SIZE_ALIGNED (A68_FORMAT);
  } else if (p == M_SEMA) {
    return A68_REF_SIZE;
  } else if (p == M_SOUND) {
    return SIZE_ALIGNED (A68_SOUND);
  } else if (p == M_COLLITEM) {
    return SIZE_ALIGNED (A68_COLLITEM);
  } else if (p == M_HEX_NUMBER) {
    int k = 0;
    MAXIMISE (k, SIZE_ALIGNED (A68_BOOL));
    MAXIMISE (k, SIZE_ALIGNED (A68_CHAR));
    MAXIMISE (k, SIZE_ALIGNED (A68_INT));
    MAXIMISE (k, SIZE_ALIGNED (A68_REAL));
    MAXIMISE (k, SIZE_ALIGNED (A68_BITS));
#if (A68_LEVEL >= 3)
    MAXIMISE (k, SIZE_ALIGNED (A68_LONG_INT));
    MAXIMISE (k, SIZE_ALIGNED (A68_LONG_REAL));
    MAXIMISE (k, SIZE_ALIGNED (A68_LONG_BITS));
#endif
    return SIZE_ALIGNED (A68_UNION) + k;
  } else if (p == M_NUMBER) {
    int k = 0;
    MAXIMISE (k, A68_REF_SIZE);
    MAXIMISE (k, SIZE_ALIGNED (A68_INT));
    MAXIMISE (k, SIZE_ALIGNED (A68_REAL));
    MAXIMISE (k, (int) size_long_mp ());
#if (A68_LEVEL >= 3)
    MAXIMISE (k, SIZE_ALIGNED (A68_LONG_INT));
    MAXIMISE (k, SIZE_ALIGNED (A68_LONG_REAL));
#else
    MAXIMISE (k, (int) size_mp ());
#endif
    return SIZE_ALIGNED (A68_UNION) + k;
  } else if (p == M_SIMPLIN) {
    int k = 0;
    MAXIMISE (k, A68_REF_SIZE);
    MAXIMISE (k, SIZE_ALIGNED (A68_FORMAT));
    MAXIMISE (k, SIZE_ALIGNED (A68_PROCEDURE));
    MAXIMISE (k, SIZE_ALIGNED (A68_SOUND));
    return SIZE_ALIGNED (A68_UNION) + k;
  } else if (p == M_SIMPLOUT) {
    return SIZE_ALIGNED (A68_UNION) + A68 (max_simplout_size);
  } else if (IS_REF (p)) {
    return A68_REF_SIZE;
  } else if (IS (p, PROC_SYMBOL)) {
    return SIZE_ALIGNED (A68_PROCEDURE);
  } else if (IS_ROW (p) && p != M_ROWS) {
    return A68_REF_SIZE;
  } else if (p == M_ROWS) {
    return SIZE_ALIGNED (A68_UNION) + A68_REF_SIZE;
  } else if (IS_FLEX (p)) {
    return moid_size (SUB (p));
  } else if (IS_STRUCT (p)) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NO_PACK; FORWARD (z)) {
      size += moid_size (MOID (z));
    }
    return size;
  } else if (IS_UNION (p)) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NO_PACK; FORWARD (z)) {
      if (moid_size (MOID (z)) > size) {
        size = moid_size (MOID (z));
      }
    }
    return SIZE_ALIGNED (A68_UNION) + size;
  } else if (PACK (p) != NO_PACK) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NO_PACK; FORWARD (z)) {
      size += moid_size (MOID (z));
    }
    return size;
  } else {
// ?
    return 0;
  }
}

//! @brief Moid digits 2.

int moid_digits_2 (MOID_T * p)
{
  if (p == NO_MOID) {
    return 0;
  }
  if (EQUIVALENT (p) != NO_MOID) {
    return moid_digits_2 (EQUIVALENT (p));
  }
  if (p == M_LONG_INT) {
#if (A68_LEVEL >= 3)
    return 0;
#else
    return (int) mp_digits ();
#endif
  }
  if (p == M_LONG_LONG_INT) {
    return (int) long_mp_digits ();
  }
  if (p == M_LONG_REAL) {
    return (int) mp_digits ();
  }
  if (p == M_LONG_LONG_REAL) {
    return (int) long_mp_digits ();
  }
  if (p == M_LONG_BITS) {
#if (A68_LEVEL >= 3)
    return 0;
#else
    return (int) mp_digits ();
#endif
  }
  if (p == M_LONG_LONG_BITS) {
    return (int) long_mp_digits ();
  } else {
    return 0;
  }
}

//! @brief Moid size.

int moid_size (MOID_T * p)
{
  SIZE (p) = A68_ALIGN (moid_size_2 (p));
  return SIZE (p);
}

//! @brief Moid digits.

int moid_digits (MOID_T * p)
{
  DIGITS (p) = moid_digits_2 (p);
  return DIGITS (p);
}
