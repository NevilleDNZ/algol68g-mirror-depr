//! @file moids-misc.c
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
//! Miscellaneous MOID routines.

#include "a68g.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"
#include "a68g-moids.h"

// MODE checker routines.

//! @brief Absorb nested series modes recursively.

void absorb_series_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NO_PACK, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NO_PACK; FORWARD (t)) {
      if (MOID (t) != NO_MOID && IS (MOID (t), SERIES_MODE)) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NO_PACK; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NO_TEXT, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NO_TEXT, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

//! @brief Make SERIES (u, v).

MOID_T *make_series_from_moids (MOID_T * u, MOID_T * v)
{
  MOID_T *x = new_moid ();
  ATTRIBUTE (x) = SERIES_MODE;
  add_mode_to_pack (&(PACK (x)), u, NO_TEXT, NODE (u));
  add_mode_to_pack (&(PACK (x)), v, NO_TEXT, NODE (v));
  absorb_series_pack (&x);
  DIM (x) = count_pack_members (PACK (x));
  (void) register_extra_mode (&TOP_MOID (&A68_JOB), x);
  if (DIM (x) == 1) {
    return MOID (PACK (x));
  } else {
    return x;
  }
}

//! @brief Absorb firmly related unions in mode.

MOID_T *absorb_related_subsets (MOID_T * m)
{
// For instance invalid UNION (PROC REF UNION (A, B), A, B) -> valid UNION (A, B),
// which is used in balancing conformity clauses.
  int mods;
  do {
    PACK_T *u = NO_PACK, *v;
    mods = 0;
    for (v = PACK (m); v != NO_PACK; FORWARD (v)) {
      MOID_T *n = depref_completely (MOID (v));
      if (IS (n, UNION_SYMBOL) && is_subset (n, m, SAFE_DEFLEXING)) {
// Unpack it.
        PACK_T *w;
        for (w = PACK (n); w != NO_PACK; FORWARD (w)) {
          add_mode_to_pack (&u, MOID (w), NO_TEXT, NODE (w));
        }
        mods++;
      } else {
        add_mode_to_pack (&u, MOID (v), NO_TEXT, NODE (v));
      }
    }
    PACK (m) = absorb_union_pack (u);
  } while (mods != 0);
  return m;
}

//! @brief Absorb nested series and united modes recursively.

void absorb_series_union_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NO_PACK, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NO_PACK; FORWARD (t)) {
      if (MOID (t) != NO_MOID && (IS (MOID (t), SERIES_MODE) || IS (MOID (t), UNION_SYMBOL))) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NO_PACK; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NO_TEXT, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NO_TEXT, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

//! @brief Make united mode, from mode that is a SERIES (..).

MOID_T *make_united_mode (MOID_T * m)
{
  MOID_T *u;
  PACK_T *w;
  int mods;
  if (m == NO_MOID) {
    return M_ERROR;
  } else if (ATTRIBUTE (m) != SERIES_MODE) {
    return m;
  }
// Do not unite a single UNION.
  if (DIM (m) == 1 && IS (MOID (PACK (m)), UNION_SYMBOL)) {
    return MOID (PACK (m));
  }
// Straighten the series.
  absorb_series_union_pack (&m);
// Copy the series into a UNION.
  u = new_moid ();
  ATTRIBUTE (u) = UNION_SYMBOL;
  PACK (u) = NO_PACK;
  w = PACK (m);
  for (w = PACK (m); w != NO_PACK; FORWARD (w)) {
    add_mode_to_pack (&(PACK (u)), MOID (w), NO_TEXT, NODE (m));
  }
// Absorb and contract the new UNION.
  do {
    mods = 0;
    absorb_series_union_pack (&u);
    DIM (u) = count_pack_members (PACK (u));
    PACK (u) = absorb_union_pack (PACK (u));
    contract_union (u);
    DIM (u) = count_pack_members (PACK (u));
  } while (mods != 0);
// A UNION of one mode is that mode itself.
  if (DIM (u) == 1) {
    return MOID (PACK (u));
  } else {
    return register_extra_mode (&TOP_MOID (&A68_JOB), u);
  }
}

//! @brief Make SOID data structure.

void make_soid (SOID_T * s, int sort, MOID_T * type, int attribute)
{
  ATTRIBUTE (s) = attribute;
  SORT (s) = sort;
  MOID (s) = type;
  CAST (s) = A68_FALSE;
}

//! @brief Whether mode is not well defined.

BOOL_T is_mode_isnt_well (MOID_T * p)
{
  if (p == NO_MOID) {
    return A68_TRUE;
  } else if (!IF_MODE_IS_WELL (p)) {
    return A68_TRUE;
  } else if (PACK (p) != NO_PACK) {
    PACK_T *q = PACK (p);
    for (; q != NO_PACK; FORWARD (q)) {
      if (!IF_MODE_IS_WELL (MOID (q))) {
        return A68_TRUE;
      }
    }
  }
  return A68_FALSE;
}

//! @brief Add SOID data to free chain.

void free_soid_list (SOID_T * root)
{
  if (root != NO_SOID) {
    SOID_T *q;
    for (q = root; NEXT (q) != NO_SOID; FORWARD (q)) {
      ;
    }
    NEXT (q) = A68 (top_soid_list);
    A68 (top_soid_list) = root;
  }
}

//! @brief Add SOID data structure to soid list.

void add_to_soid_list (SOID_T ** root, NODE_T * where, SOID_T * soid)
{
  if (*root != NO_SOID) {
    add_to_soid_list (&(NEXT (*root)), where, soid);
  } else {
    SOID_T *new_one;
    if (A68 (top_soid_list) == NO_SOID) {
      new_one = (SOID_T *) get_temp_heap_space ((size_t) SIZE_ALIGNED (SOID_T));
    } else {
      new_one = A68 (top_soid_list);
      FORWARD (A68 (top_soid_list));
    }
    make_soid (new_one, SORT (soid), MOID (soid), 0);
    NODE (new_one) = where;
    NEXT (new_one) = NO_SOID;
    *root = new_one;
  }
}

//! @brief Pack soids in moid, gather resulting moids from terminators in a clause.

MOID_T *pack_soids_in_moid (SOID_T * top_sl, int attribute)
{
  MOID_T *x = new_moid ();
  PACK_T *t, **p;
  ATTRIBUTE (x) = attribute;
  DIM (x) = 0;
  SUB (x) = NO_MOID;
  EQUIVALENT (x) = NO_MOID;
  SLICE (x) = NO_MOID;
  DEFLEXED (x) = NO_MOID;
  NAME (x) = NO_MOID;
  NEXT (x) = NO_MOID;
  PACK (x) = NO_PACK;
  p = &(PACK (x));
  for (; top_sl != NO_SOID; FORWARD (top_sl)) {
    t = new_pack ();
    MOID (t) = MOID (top_sl);
    TEXT (t) = NO_TEXT;
    NODE (t) = NODE (top_sl);
    NEXT (t) = NO_PACK;
    DIM (x)++;
    *p = t;
    p = &NEXT (t);
  }
  (void) register_extra_mode (&TOP_MOID (&A68_JOB), x);
  return x;
}

//! @brief Whether "p" is compatible with "q".

BOOL_T is_equal_modes (MOID_T * p, MOID_T * q, int deflex)
{
  if (deflex == FORCE_DEFLEXING) {
    return DEFLEX (p) == DEFLEX (q);
  } else if (deflex == ALIAS_DEFLEXING) {
    if (IS (p, REF_SYMBOL) && IS (q, REF_SYMBOL)) {
      return p == q || DEFLEX (p) == q;
    } else if (!IS (p, REF_SYMBOL) && !IS (q, REF_SYMBOL)) {
      return DEFLEX (p) == DEFLEX (q);
    }
  } else if (deflex == SAFE_DEFLEXING) {
    if (!IS (p, REF_SYMBOL) && !IS (q, REF_SYMBOL)) {
      return DEFLEX (p) == DEFLEX (q);
    }
  }
  return p == q;
}

//! @brief Whether mode is deprefable.

BOOL_T is_deprefable (MOID_T * p)
{
  if (IS_REF (p)) {
    return A68_TRUE;
  } else {
    return (BOOL_T) (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK);
  }
}

//! @brief Depref mode once.

MOID_T *depref_once (MOID_T * p)
{
  if (IS_REF_FLEX (p)) {
    return SUB_SUB (p);
  } else if (IS_REF (p)) {
    return SUB (p);
  } else if (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    return SUB (p);
  } else {
    return NO_MOID;
  }
}

//! @brief Depref mode completely.

MOID_T *depref_completely (MOID_T * p)
{
  while (is_deprefable (p)) {
    p = depref_once (p);
  }
  return p;
}

//! @brief Deproc_completely.

MOID_T *deproc_completely (MOID_T * p)
{
  while (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    p = depref_once (p);
  }
  return p;
}

//! @brief Depref rows.

MOID_T *depref_rows (MOID_T * p, MOID_T * q)
{
  if (q == M_ROWS) {
    while (is_deprefable (p)) {
      p = depref_once (p);
    }
    return p;
  } else {
    return q;
  }
}

//! @brief Derow mode, strip FLEX and BOUNDS.

MOID_T *derow (MOID_T * p)
{
  if (IS_ROW (p) || IS_FLEX (p)) {
    return derow (SUB (p));
  } else {
    return p;
  }
}

//! @brief Whether rows type.

BOOL_T is_rows_type (MOID_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ROW_SYMBOL:
  case FLEX_SYMBOL:
    {
      return A68_TRUE;
    }
  case UNION_SYMBOL:
    {
      PACK_T *t = PACK (p);
      BOOL_T go_on = A68_TRUE;
      while (t != NO_PACK && go_on) {
        go_on &= is_rows_type (MOID (t));
        FORWARD (t);
      }
      return go_on;
    }
  default:
    {
      return A68_FALSE;
    }
  }
}

//! @brief Whether mode is PROC (REF FILE) VOID or FORMAT.

BOOL_T is_proc_ref_file_void_or_format (MOID_T * p)
{
  if (p == M_PROC_REF_FILE_VOID) {
    return A68_TRUE;
  } else if (p == M_FORMAT) {
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether mode can be transput.

BOOL_T is_transput_mode (MOID_T * p, char rw)
{
  if (p == M_INT) {
    return A68_TRUE;
  } else if (p == M_LONG_INT) {
    return A68_TRUE;
  } else if (p == M_LONG_LONG_INT) {
    return A68_TRUE;
  } else if (p == M_REAL) {
    return A68_TRUE;
  } else if (p == M_LONG_REAL) {
    return A68_TRUE;
  } else if (p == M_LONG_LONG_REAL) {
    return A68_TRUE;
  } else if (p == M_BOOL) {
    return A68_TRUE;
  } else if (p == M_CHAR) {
    return A68_TRUE;
  } else if (p == M_BITS) {
    return A68_TRUE;
  } else if (p == M_LONG_BITS) {
    return A68_TRUE;
  } else if (p == M_LONG_LONG_BITS) {
    return A68_TRUE;
  } else if (p == M_COMPLEX) {
    return A68_TRUE;
  } else if (p == M_LONG_COMPLEX) {
    return A68_TRUE;
  } else if (p == M_LONG_LONG_COMPLEX) {
    return A68_TRUE;
  } else if (p == M_ROW_CHAR) {
    return A68_TRUE;
  } else if (p == M_STRING) {
    return A68_TRUE;
  } else if (p == M_SOUND) {
    return A68_TRUE;
  } else if (IS (p, UNION_SYMBOL) || IS (p, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (p);
    BOOL_T k = A68_TRUE;
    for (; q != NO_PACK && k; FORWARD (q)) {
      k = (BOOL_T) (k & (is_transput_mode (MOID (q), rw) || is_proc_ref_file_void_or_format (MOID (q))));
    }
    return k;
  } else if (IS_FLEX (p)) {
    if (SUB (p) == M_ROW_CHAR) {
      return A68_TRUE;
    } else {
      return (BOOL_T) (rw == 'w' ? is_transput_mode (SUB (p), rw) : A68_FALSE);
    }
  } else if (IS_ROW (p)) {
    return (BOOL_T) (is_transput_mode (SUB (p), rw) || is_proc_ref_file_void_or_format (SUB (p)));
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether mode is printable.

BOOL_T is_printable_mode (MOID_T * p)
{
  if (is_proc_ref_file_void_or_format (p)) {
    return A68_TRUE;
  } else {
    return is_transput_mode (p, 'w');
  }
}

//! @brief Whether mode is readable.

BOOL_T is_readable_mode (MOID_T * p)
{
  if (is_proc_ref_file_void_or_format (p)) {
    return A68_TRUE;
  } else {
    return (BOOL_T) (IS_REF (p) ? is_transput_mode (SUB (p), 'r') : A68_FALSE);
  }
}

//! @brief Whether name struct.

BOOL_T is_name_struct (MOID_T * p)
{
  return (BOOL_T) (NAME (p) != NO_MOID ? IS (DEFLEX (SUB (p)), STRUCT_SYMBOL) : A68_FALSE);
}

//! @brief Yield mode to unite to.

MOID_T *unites_to (MOID_T * m, MOID_T * u)
{
// Uniting U (m).
  MOID_T *v = NO_MOID;
  PACK_T *p;
  if (u == M_SIMPLIN || u == M_SIMPLOUT) {
    return m;
  }
  for (p = PACK (u); p != NO_PACK; FORWARD (p)) {
// Prefer []->[] over []->FLEX [].
    if (m == MOID (p)) {
      v = MOID (p);
    } else if (v == NO_MOID && DEFLEX (m) == DEFLEX (MOID (p))) {
      v = MOID (p);
    }
  }
  return v;
}

//! @brief Whether moid in pack.

BOOL_T is_moid_in_pack (MOID_T * u, PACK_T * v, int deflex)
{
  for (; v != NO_PACK; FORWARD (v)) {
    if (is_equal_modes (u, MOID (v), deflex)) {
      return A68_TRUE;
    }
  }
  return A68_FALSE;
}

//! @brief Whether "p" is a subset of "q".

BOOL_T is_subset (MOID_T * p, MOID_T * q, int deflex)
{
  PACK_T *u = PACK (p);
  BOOL_T j = A68_TRUE;
  for (; u != NO_PACK && j; FORWARD (u)) {
    j = (BOOL_T) (j && is_moid_in_pack (MOID (u), PACK (q), deflex));
  }
  return j;
}

//! @brief Whether "p" can be united to UNION "q".

BOOL_T is_unitable (MOID_T * p, MOID_T * q, int deflex)
{
  if (IS (q, UNION_SYMBOL)) {
    if (IS (p, UNION_SYMBOL)) {
      return is_subset (p, q, deflex);
    } else {
      return is_moid_in_pack (p, PACK (q), deflex);
    }
  }
  return A68_FALSE;
}

//! @brief Whether all or some components of "u" can be firmly coerced to a component mode of "v"..

void investigate_firm_relations (PACK_T * u, PACK_T * v, BOOL_T * all, BOOL_T * some)
{
  *all = A68_TRUE;
  *some = A68_FALSE;
  for (; v != NO_PACK; FORWARD (v)) {
    PACK_T *w;
    BOOL_T k = A68_FALSE;
    for (w = u; w != NO_PACK; FORWARD (w)) {
      k |= is_coercible (MOID (w), MOID (v), FIRM, FORCE_DEFLEXING);
    }
    *some |= k;
    *all &= k;
  }
}

//! @brief Whether there is a soft path from "p" to "q".

BOOL_T is_softly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return A68_TRUE;
  } else if (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    return is_softly_coercible (SUB (p), q, deflex);
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether there is a weak path from "p" to "q".

BOOL_T is_weakly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return A68_TRUE;
  } else if (is_deprefable (p)) {
    return is_weakly_coercible (depref_once (p), q, deflex);
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether there is a meek path from "p" to "q".

BOOL_T is_meekly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return A68_TRUE;
  } else if (is_deprefable (p)) {
    return is_meekly_coercible (depref_once (p), q, deflex);
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether there is a firm path from "p" to "q".

BOOL_T is_firmly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return A68_TRUE;
  } else if (q == M_ROWS && is_rows_type (p)) {
    return A68_TRUE;
  } else if (is_unitable (p, q, deflex)) {
    return A68_TRUE;
  } else if (is_deprefable (p)) {
    return is_firmly_coercible (depref_once (p), q, deflex);
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether firm.

BOOL_T is_firm (MOID_T * p, MOID_T * q)
{
  return (BOOL_T) (is_firmly_coercible (p, q, SAFE_DEFLEXING) || is_firmly_coercible (q, p, SAFE_DEFLEXING));
}

//! @brief Whether "p" widens to "q".

MOID_T *widens_to (MOID_T * p, MOID_T * q)
{
  if (p == M_INT) {
    if (q == M_LONG_INT || q == M_LONG_LONG_INT || q == M_LONG_REAL || q == M_LONG_LONG_REAL || q == M_LONG_COMPLEX || q == M_LONG_LONG_COMPLEX) {
      return M_LONG_INT;
    } else if (q == M_REAL || q == M_COMPLEX) {
      return M_REAL;
    } else {
      return NO_MOID;
    }
  } else if (p == M_LONG_INT) {
    if (q == M_LONG_LONG_INT) {
      return M_LONG_LONG_INT;
    } else if (q == M_LONG_REAL || q == M_LONG_LONG_REAL || q == M_LONG_COMPLEX || q == M_LONG_LONG_COMPLEX) {
      return M_LONG_REAL;
    } else {
      return NO_MOID;
    }
  } else if (p == M_LONG_LONG_INT) {
    if (q == M_LONG_LONG_REAL || q == M_LONG_LONG_COMPLEX) {
      return M_LONG_LONG_REAL;
    } else {
      return NO_MOID;
    }
  } else if (p == M_REAL) {
    if (q == M_LONG_REAL || q == M_LONG_LONG_REAL || q == M_LONG_COMPLEX || q == M_LONG_LONG_COMPLEX) {
      return M_LONG_REAL;
    } else if (q == M_COMPLEX) {
      return M_COMPLEX;
    } else {
      return NO_MOID;
    }
  } else if (p == M_COMPLEX) {
    if (q == M_LONG_COMPLEX || q == M_LONG_LONG_COMPLEX) {
      return M_LONG_COMPLEX;
    } else {
      return NO_MOID;
    }
  } else if (p == M_LONG_REAL) {
    if (q == M_LONG_LONG_REAL || q == M_LONG_LONG_COMPLEX) {
      return M_LONG_LONG_REAL;
    } else if (q == M_LONG_COMPLEX) {
      return M_LONG_COMPLEX;
    } else {
      return NO_MOID;
    }
  } else if (p == M_LONG_COMPLEX) {
    if (q == M_LONG_LONG_COMPLEX) {
      return M_LONG_LONG_COMPLEX;
    } else {
      return NO_MOID;
    }
  } else if (p == M_LONG_LONG_REAL) {
    if (q == M_LONG_LONG_COMPLEX) {
      return M_LONG_LONG_COMPLEX;
    } else {
      return NO_MOID;
    }
  } else if (p == M_BITS) {
    if (q == M_LONG_BITS || q == M_LONG_LONG_BITS) {
      return M_LONG_BITS;
    } else if (q == M_ROW_BOOL) {
      return M_ROW_BOOL;
    } else if (q == M_FLEX_ROW_BOOL) {
      return M_FLEX_ROW_BOOL;
    } else {
      return NO_MOID;
    }
  } else if (p == M_LONG_BITS) {
    if (q == M_LONG_LONG_BITS) {
      return M_LONG_LONG_BITS;
    } else if (q == M_ROW_BOOL) {
      return M_ROW_BOOL;
    } else if (q == M_FLEX_ROW_BOOL) {
      return M_FLEX_ROW_BOOL;
    } else {
      return NO_MOID;
    }
  } else if (p == M_LONG_LONG_BITS) {
    if (q == M_ROW_BOOL) {
      return M_ROW_BOOL;
    } else if (q == M_FLEX_ROW_BOOL) {
      return M_FLEX_ROW_BOOL;
    } else {
      return NO_MOID;
    }
  } else if (p == M_BYTES && q == M_ROW_CHAR) {
    return M_ROW_CHAR;
  } else if (p == M_LONG_BYTES && q == M_ROW_CHAR) {
    return M_ROW_CHAR;
  } else if (p == M_BYTES && q == M_FLEX_ROW_CHAR) {
    return M_FLEX_ROW_CHAR;
  } else if (p == M_LONG_BYTES && q == M_FLEX_ROW_CHAR) {
    return M_FLEX_ROW_CHAR;
  } else {
    return NO_MOID;
  }
}

//! @brief Whether "p" widens to "q".

BOOL_T is_widenable (MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  if (z != NO_MOID) {
    return (BOOL_T) (z == q ? A68_TRUE : is_widenable (z, q));
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether "p" is a REF ROW.

BOOL_T is_ref_row (MOID_T * p)
{
  return (BOOL_T) (NAME (p) != NO_MOID ? IS_ROW (DEFLEX (SUB (p))) : A68_FALSE);
}

//! @brief Whether strong name.

BOOL_T is_strong_name (MOID_T * p, MOID_T * q)
{
  if (p == q) {
    return A68_TRUE;
  } else if (is_ref_row (q)) {
    return is_strong_name (p, NAME (q));
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether strong slice.

BOOL_T is_strong_slice (MOID_T * p, MOID_T * q)
{
  if (p == q || is_widenable (p, q)) {
    return A68_TRUE;
  } else if (SLICE (q) != NO_MOID) {
    return is_strong_slice (p, SLICE (q));
  } else if (IS_FLEX (q)) {
    return is_strong_slice (p, SUB (q));
  } else if (is_ref_row (q)) {
    return is_strong_name (p, q);
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether strongly coercible.

BOOL_T is_strongly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
// Keep this sequence of statements.
  if (is_equal_modes (p, q, deflex)) {
    return A68_TRUE;
  } else if (q == M_VOID) {
    return A68_TRUE;
  } else if ((q == M_SIMPLIN || q == M_ROW_SIMPLIN) && is_readable_mode (p)) {
    return A68_TRUE;
  } else if (q == M_ROWS && is_rows_type (p)) {
    return A68_TRUE;
  } else if (is_unitable (p, derow (q), deflex)) {
    return A68_TRUE;
  }
  if (is_ref_row (q) && is_strong_name (p, q)) {
    return A68_TRUE;
  } else if (SLICE (q) != NO_MOID && is_strong_slice (p, q)) {
    return A68_TRUE;
  } else if (IS_FLEX (q) && is_strong_slice (p, q)) {
    return A68_TRUE;
  } else if (is_widenable (p, q)) {
    return A68_TRUE;
  } else if (is_deprefable (p)) {
    return is_strongly_coercible (depref_once (p), q, deflex);
  } else if (q == M_SIMPLOUT || q == M_ROW_SIMPLOUT) {
    return is_printable_mode (p);
  } else {
    return A68_FALSE;
  }
}

//! @brief Basic coercions.

BOOL_T basic_coercions (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return A68_TRUE;
  } else if (c == NO_SORT) {
    return (BOOL_T) (p == q);
  } else if (c == SOFT) {
    return is_softly_coercible (p, q, deflex);
  } else if (c == WEAK) {
    return is_weakly_coercible (p, q, deflex);
  } else if (c == MEEK) {
    return is_meekly_coercible (p, q, deflex);
  } else if (c == FIRM) {
    return is_firmly_coercible (p, q, deflex);
  } else if (c == STRONG) {
    return is_strongly_coercible (p, q, deflex);
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether coercible stowed.

BOOL_T is_coercible_stowed (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c != STRONG) {
// Such construct is always in a strong position, is it not?
    return A68_FALSE;
  } else if (q == M_VOID) {
    return A68_TRUE;
  } else if (IS_FLEX (q)) {
    PACK_T *u = PACK (p);
    BOOL_T j = A68_TRUE;
    for (; u != NO_PACK && j; FORWARD (u)) {
      j &= is_coercible (MOID (u), SLICE (SUB (q)), c, deflex);
    }
    return j;
  } else if (IS_ROW (q)) {
    PACK_T *u = PACK (p);
    BOOL_T j = A68_TRUE;
    for (; u != NO_PACK && j; FORWARD (u)) {
      j &= is_coercible (MOID (u), SLICE (q), c, deflex);
    }
    return j;
  } else if (IS (q, PROC_SYMBOL) || IS (q, STRUCT_SYMBOL)) {
    PACK_T *u = PACK (p), *v = PACK (q);
    if (DIM (p) != DIM (q)) {
      return A68_FALSE;
    } else {
      BOOL_T j = A68_TRUE;
      while (u != NO_PACK && v != NO_PACK && j) {
        j &= is_coercible (MOID (u), MOID (v), c, deflex);
        FORWARD (u);
        FORWARD (v);
        }
      return j;
    }
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether coercible series.

BOOL_T is_coercible_series (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c == NO_SORT) {
    return A68_FALSE;
  } else if (p == NO_MOID || q == NO_MOID) {
    return A68_FALSE;
  } else if (IS (p, SERIES_MODE) && PACK (p) == NO_PACK) {
    return A68_FALSE;
  } else if (IS (q, SERIES_MODE) && PACK (q) == NO_PACK) {
    return A68_FALSE;
  } else if (PACK (p) == NO_PACK) {
    return is_coercible (p, q, c, deflex);
  } else {
    PACK_T *u = PACK (p);
    BOOL_T j = A68_TRUE;
    for (; u != NO_PACK && j; FORWARD (u)) {
      if (MOID (u) != NO_MOID) {
        j &= is_coercible (MOID (u), q, c, deflex);
      }
    }
    return j;
  }
}

//! @brief Whether "p" can be coerced to "q" in a "c" context.

BOOL_T is_coercible (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (is_mode_isnt_well (p) || is_mode_isnt_well (q)) {
    return A68_TRUE;
  } else if (is_equal_modes (p, q, deflex)) {
    return A68_TRUE;
  } else if (p == M_HIP) {
    return A68_TRUE;
  } else if (IS (p, STOWED_MODE)) {
    return is_coercible_stowed (p, q, c, deflex);
  } else if (IS (p, SERIES_MODE)) {
    return is_coercible_series (p, q, c, deflex);
  } else if (p == M_VACUUM && IS_ROW (DEFLEX (q))) {
    return A68_TRUE;
  } else {
    return basic_coercions (p, q, c, deflex);
  }
}

//! @brief Whether coercible in context.

BOOL_T is_coercible_in_context (SOID_T * p, SOID_T * q, int deflex)
{
  if (SORT (p) != SORT (q)) {
    return A68_FALSE;
  } else if (MOID (p) == MOID (q)) {
    return A68_TRUE;
  } else {
    return is_coercible (MOID (p), MOID (q), SORT (q), deflex);
  }
}

//! @brief Whether list "y" is balanced.

BOOL_T is_balanced (NODE_T * n, SOID_T * y, int sort)
{
  if (sort == STRONG) {
    return A68_TRUE;
  } else {
    BOOL_T k = A68_FALSE;
    for (; y != NO_SOID && !k; FORWARD (y)) {
      k = (BOOL_T) (!IS (MOID (y), STOWED_MODE));
    }
    if (k == A68_FALSE) {
      diagnostic (A68_ERROR, n, ERROR_NO_UNIQUE_MODE);
    }
    return k;
  }
}

//! @brief A moid from "m" to which all other members can be coerced.

MOID_T *get_balanced_mode (MOID_T * m, int sort, BOOL_T return_depreffed, int deflex)
{
  MOID_T *common_moid = NO_MOID;
  if (m != NO_MOID && !is_mode_isnt_well (m) && IS (m, UNION_SYMBOL)) {
    int depref_level;
    BOOL_T go_on = A68_TRUE;
// Test for increasing depreffing.
    for (depref_level = 0; go_on; depref_level++) {
      PACK_T *p;
      go_on = A68_FALSE;
// Test the whole pack.
      for (p = PACK (m); p != NO_PACK; FORWARD (p)) {
// HIPs are not eligible of course.
        if (MOID (p) != M_HIP) {
          MOID_T *candidate = MOID (p);
          int k;
// Depref as far as allowed.
          for (k = depref_level; k > 0 && is_deprefable (candidate); k--) {
            candidate = depref_once (candidate);
          }
// Only need testing if all allowed deprefs succeeded.
          if (k == 0) {
            PACK_T *q;
            MOID_T *to = (return_depreffed ? depref_completely (candidate) : candidate);
            BOOL_T all_coercible = A68_TRUE;
            go_on = A68_TRUE;
            for (q = PACK (m); q != NO_PACK && all_coercible; FORWARD (q)) {
              MOID_T *from = MOID (q);
              if (p != q && from != to) {
                all_coercible &= is_coercible (from, to, sort, deflex);
              }
            }
// If the pack is coercible to the candidate, we mark the candidate.
// We continue searching for longest series of REF REF PROC REF.
            if (all_coercible) {
              MOID_T *mark = (return_depreffed ? MOID (p) : candidate);
              if (common_moid == NO_MOID) {
                common_moid = mark;
              } else if (IS_FLEX (candidate) && DEFLEX (candidate) == common_moid) {
// We prefer FLEX.
                common_moid = mark;
              }
            }
          }
        }
      }                         // for
    }                           // for
  }
  return common_moid == NO_MOID ? m : common_moid;
}

//! @brief Whether we can search a common mode from a clause or not.

BOOL_T clause_allows_balancing (int att)
{
  switch (att) {
  case CLOSED_CLAUSE:
  case CONDITIONAL_CLAUSE:
  case CASE_CLAUSE:
  case SERIAL_CLAUSE:
  case CONFORMITY_CLAUSE:
    {
      return A68_TRUE;
    }
  }
  return A68_FALSE;
}

//! @brief A unique mode from "z".

MOID_T *determine_unique_mode (SOID_T * z, int deflex)
{
  if (z == NO_SOID) {
    return NO_MOID;
  } else {
    MOID_T *x = MOID (z);
    if (is_mode_isnt_well (x)) {
      return M_ERROR;
    }
    x = make_united_mode (x);
    if (clause_allows_balancing (ATTRIBUTE (z))) {
      return get_balanced_mode (x, STRONG, NO_DEPREF, deflex);
    } else {
      return x;
    }
  }
}

//! @brief Insert coercion "a" in the tree.

void make_coercion (NODE_T * l, int a, MOID_T * m)
{
  make_sub (l, l, a);
  MOID (l) = depref_rows (MOID (l), m);
}

//! @brief Make widening coercion.

void make_widening_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  make_coercion (n, WIDENING, z);
  if (z != q) {
    make_widening_coercion (n, z, q);
  }
}

//! @brief Make ref rowing coercion.

void make_ref_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (is_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (is_ref_row (q)) {
      make_ref_rowing_coercion (n, p, NAME (q));
      make_coercion (n, ROWING, q);
    }
  }
}

//! @brief Make rowing coercion.

void make_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (is_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (SLICE (q) != NO_MOID) {
      make_rowing_coercion (n, p, SLICE (q));
      make_coercion (n, ROWING, q);
    } else if (IS_FLEX (q)) {
      make_rowing_coercion (n, p, SUB (q));
    } else if (is_ref_row (q)) {
      make_ref_rowing_coercion (n, p, q);
    }
  }
}

//! @brief Make uniting coercion.

void make_uniting_coercion (NODE_T * n, MOID_T * q)
{
  make_coercion (n, UNITING, derow (q));
  if (IS_ROW (q) || IS_FLEX (q)) {
    make_rowing_coercion (n, derow (q), q);
  }
}

//! @brief Make depreffing coercion.

void make_depreffing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) == DEFLEX (q)) {
    return;
  } else if (q == M_SIMPLOUT && is_printable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == M_ROW_SIMPLOUT && is_printable_mode (p)) {
    make_coercion (n, UNITING, M_SIMPLOUT);
    make_coercion (n, ROWING, M_ROW_SIMPLOUT);
  } else if (q == M_SIMPLIN && is_readable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == M_ROW_SIMPLIN && is_readable_mode (p)) {
    make_coercion (n, UNITING, M_SIMPLIN);
    make_coercion (n, ROWING, M_ROW_SIMPLIN);
  } else if (q == M_ROWS && is_rows_type (p)) {
    make_coercion (n, UNITING, M_ROWS);
    MOID (n) = M_ROWS;
  } else if (is_widenable (p, q)) {
    make_widening_coercion (n, p, q);
  } else if (is_unitable (p, derow (q), SAFE_DEFLEXING)) {
    make_uniting_coercion (n, q);
  } else if (is_ref_row (q) && is_strong_name (p, q)) {
    make_ref_rowing_coercion (n, p, q);
  } else if (SLICE (q) != NO_MOID && is_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (IS_FLEX (q) && is_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (IS_REF (p)) {
    MOID_T *r = depref_once (p);
    make_coercion (n, DEREFERENCING, r);
    make_depreffing_coercion (n, r, q);
  } else if (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    MOID_T *r = SUB (p);
    make_coercion (n, DEPROCEDURING, r);
    make_depreffing_coercion (n, r, q);
  } else if (p != q) {
    cannot_coerce (n, p, q, NO_SORT, SKIP_DEFLEXING, 0);
  }
}

//! @brief Whether p is a nonproc mode (that is voided directly).

BOOL_T is_nonproc (MOID_T * p)
{
  if (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    return A68_FALSE;
  } else if (IS_REF (p)) {
    return is_nonproc (SUB (p));
  } else {
    return A68_TRUE;
  }
}

//! @brief Make_void: voiden in an appropriate way.

void make_void (NODE_T * p, MOID_T * q)
{
  switch (ATTRIBUTE (p)) {
  case ASSIGNATION:
  case IDENTITY_RELATION:
  case GENERATOR:
  case CAST:
  case DENOTATION:
    {
      make_coercion (p, VOIDING, M_VOID);
      return;
    }
  }
// MORFs are an involved case.
  switch (ATTRIBUTE (p)) {
  case SELECTION:
  case SLICE:
  case ROUTINE_TEXT:
  case FORMULA:
  case CALL:
  case IDENTIFIER:
    {
// A nonproc moid value is eliminated directly.
      if (is_nonproc (q)) {
        make_coercion (p, VOIDING, M_VOID);
        return;
      } else {
// Descend the chain of e.g. REF PROC .. until a nonproc moid remains.
        MOID_T *z = q;
        while (!is_nonproc (z)) {
          if (IS_REF (z)) {
            make_coercion (p, DEREFERENCING, SUB (z));
          }
          if (IS (z, PROC_SYMBOL) && NODE_PACK (p) == NO_PACK) {
            make_coercion (p, DEPROCEDURING, SUB (z));
          }
          z = SUB (z);
        }
        if (z != M_VOID) {
          make_coercion (p, VOIDING, M_VOID);
        }
        return;
      }
    }
  }
// All other is voided straight away.
  make_coercion (p, VOIDING, M_VOID);
}

//! @brief Make strong coercion.

void make_strong (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (q == M_VOID && p != M_VOID) {
    make_void (n, p);
  } else {
    make_depreffing_coercion (n, p, q);
  }
}

