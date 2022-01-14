//! @file modes.c
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2022 J. Marcel van der Veer <algol68g@xs4all.nl>.
//
//! @section License
//
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the 
// Free Software Foundation; either version 3 of the License, or 
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
// more details. You should have received a copy of the GNU General Public 
// License along with this program. If not, see <http://www.gnu.org/licenses/>.

// Algol 68 contexts are SOFT, WEAK, MEEK, FIRM and STRONG.
// These contexts are increasing in strength:
// 
//   SOFT: Deproceduring
// 
//   WEAK: Dereferencing to REF [] or REF STRUCT
// 
//   MEEK: Deproceduring and dereferencing
// 
//   FIRM: MEEK followed by uniting
// 
//   STRONG: FIRM followed by rowing, widening or voiding
// 
// Furthermore you will see in this file next switches:
// 
// (1) FORCE_DEFLEXING allows assignment compatibility between FLEX and non FLEX
// rows. This can only be the case when there is no danger of altering bounds of a
// non FLEX row.
// 
// (2) ALIAS_DEFLEXING prohibits aliasing a FLEX row to a non FLEX row (vice versa
// is no problem) so that one cannot alter the bounds of a non FLEX row by
// aliasing it to a FLEX row. This is particularly the case when passing names as
// parameters to procedures:
// 
//    PROC x = (REF STRING s) VOID: ..., PROC y = (REF [] CHAR c) VOID: ...;
// 
//    x (LOC STRING);    # OK #
// 
//    x (LOC [10] CHAR); # Not OK, suppose x changes bounds of s! #
//  
//    y (LOC STRING);    # OK #
// 
//    y (LOC [10] CHAR); # OK #
// 
// (3) SAFE_DEFLEXING sets FLEX row apart from non FLEX row. This holds for names,
// not for values, so common things are not rejected, for instance
// 
//    STRING x = read string;
// 
//    [] CHAR y = read string
// 
// (4) NO_DEFLEXING sets FLEX row apart from non FLEX row.
// 
// Finally, a static scope checker inspects the source. Note that Algol 68 also 
// needs dynamic scope checking. This phase concludes the parser.

#include "a68g.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"

static BOOL_T basic_coercions (MOID_T *, MOID_T *, int, int);
static BOOL_T is_coercible (MOID_T *, MOID_T *, int, int);
static BOOL_T is_nonproc (MOID_T *);
static void mode_check_enclosed (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_unit (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_formula (NODE_T *, SOID_T *, SOID_T *);
static void coerce_enclosed (NODE_T *, SOID_T *);
static void coerce_operand (NODE_T *, SOID_T *);
static void coerce_formula (NODE_T *, SOID_T *);
static void coerce_unit (NODE_T *, SOID_T *);

#define DEPREF A68_TRUE
#define NO_DEPREF A68_FALSE

#define IF_MODE_IS_WELL(n) (! ((n) == M_ERROR || (n) == M_UNDEFINED))
#define INSERT_COERCIONS(n, p, q) make_strong ((n), (p), MOID (q))

// MODE checker and coercion inserter.

//! @brief Absorb nested series modes recursively.

static void absorb_series_pack (MOID_T ** p)
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

static MOID_T *make_series_from_moids (MOID_T * u, MOID_T * v)
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

static MOID_T *absorb_related_subsets (MOID_T * m)
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

static void absorb_series_union_pack (MOID_T ** p)
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

//! @brief Give accurate error message.

static char *mode_error_text (NODE_T * n, MOID_T * p, MOID_T * q, int context, int deflex, int depth)
{
#define TAIL(z) (&(z)[strlen (z)])
  static char txt[BUFFER_SIZE];
  if (depth == 1) {
    txt[0] = NULL_CHAR;
  }
  if (IS (p, SERIES_MODE)) {
    PACK_T *u = PACK (p);
    int N = 0;
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
      N++;
    } else {
      for (; u != NO_PACK; FORWARD (u)) {
        if (MOID (u) != NO_MOID) {
          if (IS (MOID (u), SERIES_MODE)) {
            (void) mode_error_text (n, MOID (u), q, context, deflex, depth + 1);
          } else if (!is_coercible (MOID (u), q, context, deflex)) {
            int len = (int) strlen (txt);
            if (len > BUFFER_SIZE / 2) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
              N++;
            } else {
              if (strlen (txt) > 0) {
                ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
                N++;
              }
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "%s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
              N++;
            }
          }
        }
      }
    }
    if (depth == 1) {
      if (N == 0) {
        ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "mode") >= 0);
      }
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (q, MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (IS (p, STOWED_MODE) && IS_FLEX (q)) {
    PACK_T *u = PACK (p);
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NO_PACK; FORWARD (u)) {
        if (!is_coercible (MOID (u), SLICE (SUB (q)), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "%s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (SUB (q)), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (IS (p, STOWED_MODE) && IS (q, ROW_SYMBOL)) {
    PACK_T *u = PACK (p);
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NO_PACK; FORWARD (u)) {
        if (!is_coercible (MOID (u), SLICE (q), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "%s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (q), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (IS (p, STOWED_MODE) && (IS (q, PROC_SYMBOL) || IS (q, STRUCT_SYMBOL))) {
    PACK_T *u = PACK (p), *v = PACK (q);
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NO_PACK && v != NO_PACK; FORWARD (u), FORWARD (v)) {
        if (!is_coercible (MOID (u), MOID (v), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "%s cannot be coerced to %s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n), moid_to_string (MOID (v), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
    }
  }
  return txt;
#undef TAIL
}

//! @brief Cannot coerce error.

static void cannot_coerce (NODE_T * p, MOID_T * from, MOID_T * to, int context, int deflex, int att)
{
  char *txt = mode_error_text (p, from, to, context, deflex, 1);
  if (att == STOP) {
    if (strlen (txt) == 0) {
      diagnostic (A68_ERROR, p, "M cannot be coerced to M in C context", from, to, context);
    } else {
      diagnostic (A68_ERROR, p, "Y in C context", txt, context);
    }
  } else {
    if (strlen (txt) == 0) {
      diagnostic (A68_ERROR, p, "M cannot be coerced to M in C-A", from, to, context, att);
    } else {
      diagnostic (A68_ERROR, p, "Y in C-A", txt, context, att);
    }
  }
}

//! @brief Make SOID data structure.

static void make_soid (SOID_T * s, int sort, MOID_T * type, int attribute)
{
  ATTRIBUTE (s) = attribute;
  SORT (s) = sort;
  MOID (s) = type;
  CAST (s) = A68_FALSE;
}

//! @brief Driver for mode checker.

void mode_checker (NODE_T * p)
{
  if (IS (p, PARTICULAR_PROGRAM)) {
    SOID_T x, y;
    A68 (top_soid_list) = NO_SOID;
    make_soid (&x, STRONG, M_VOID, 0);
    mode_check_enclosed (SUB (p), &x, &y);
    MOID (p) = MOID (&y);
  }
}

//! @brief Driver for coercion inserions.

void coercion_inserter (NODE_T * p)
{
  if (IS (p, PARTICULAR_PROGRAM)) {
    SOID_T q;
    make_soid (&q, STRONG, M_VOID, 0);
    coerce_enclosed (SUB (p), &q);
  }
}

//! @brief Whether mode is not well defined.

static BOOL_T is_mode_isnt_well (MOID_T * p)
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

static void add_to_soid_list (SOID_T ** root, NODE_T * where, SOID_T * soid)
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

static MOID_T *pack_soids_in_moid (SOID_T * top_sl, int attribute)
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

static BOOL_T is_equal_modes (MOID_T * p, MOID_T * q, int deflex)
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

static MOID_T *depref_once (MOID_T * p)
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

static MOID_T *deproc_completely (MOID_T * p)
{
  while (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    p = depref_once (p);
  }
  return p;
}

//! @brief Depref rows.

static MOID_T *depref_rows (MOID_T * p, MOID_T * q)
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

static MOID_T *derow (MOID_T * p)
{
  if (IS_ROW (p) || IS_FLEX (p)) {
    return derow (SUB (p));
  } else {
    return p;
  }
}

//! @brief Whether rows type.

static BOOL_T is_rows_type (MOID_T * p)
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

static BOOL_T is_proc_ref_file_void_or_format (MOID_T * p)
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

static BOOL_T is_transput_mode (MOID_T * p, char rw)
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

static BOOL_T is_printable_mode (MOID_T * p)
{
  if (is_proc_ref_file_void_or_format (p)) {
    return A68_TRUE;
  } else {
    return is_transput_mode (p, 'w');
  }
}

//! @brief Whether mode is readable.

static BOOL_T is_readable_mode (MOID_T * p)
{
  if (is_proc_ref_file_void_or_format (p)) {
    return A68_TRUE;
  } else {
    return (BOOL_T) (IS_REF (p) ? is_transput_mode (SUB (p), 'r') : A68_FALSE);
  }
}

//! @brief Whether name struct.

static BOOL_T is_name_struct (MOID_T * p)
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

static BOOL_T is_moid_in_pack (MOID_T * u, PACK_T * v, int deflex)
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

static void investigate_firm_relations (PACK_T * u, PACK_T * v, BOOL_T * all, BOOL_T * some)
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

static BOOL_T is_softly_coercible (MOID_T * p, MOID_T * q, int deflex)
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

static BOOL_T is_weakly_coercible (MOID_T * p, MOID_T * q, int deflex)
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

static BOOL_T is_meekly_coercible (MOID_T * p, MOID_T * q, int deflex)
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

static BOOL_T is_firmly_coercible (MOID_T * p, MOID_T * q, int deflex)
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

static MOID_T *widens_to (MOID_T * p, MOID_T * q)
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

static BOOL_T is_widenable (MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  if (z != NO_MOID) {
    return (BOOL_T) (z == q ? A68_TRUE : is_widenable (z, q));
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether "p" is a REF ROW.

static BOOL_T is_ref_row (MOID_T * p)
{
  return (BOOL_T) (NAME (p) != NO_MOID ? IS_ROW (DEFLEX (SUB (p))) : A68_FALSE);
}

//! @brief Whether strong name.

static BOOL_T is_strong_name (MOID_T * p, MOID_T * q)
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

static BOOL_T is_strong_slice (MOID_T * p, MOID_T * q)
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

static BOOL_T is_strongly_coercible (MOID_T * p, MOID_T * q, int deflex)
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

static BOOL_T basic_coercions (MOID_T * p, MOID_T * q, int c, int deflex)
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

static BOOL_T is_coercible_stowed (MOID_T * p, MOID_T * q, int c, int deflex)
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

static BOOL_T is_coercible_series (MOID_T * p, MOID_T * q, int c, int deflex)
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

static BOOL_T is_coercible_in_context (SOID_T * p, SOID_T * q, int deflex)
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

static BOOL_T is_balanced (NODE_T * n, SOID_T * y, int sort)
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

static BOOL_T clause_allows_balancing (int att)
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

static MOID_T *determine_unique_mode (SOID_T * z, int deflex)
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

//! @brief Give a warning when a value is silently discarded.

static void warn_for_voiding (NODE_T * p, SOID_T * x, SOID_T * y, int c)
{
  (void) c;
  if (CAST (x) == A68_FALSE) {
    if (MOID (x) == M_VOID && MOID (y) != M_ERROR && !(MOID (y) == M_VOID || !is_nonproc (MOID (y)))) {
      if (IS (p, FORMULA)) {
        diagnostic (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_VOIDED, MOID (y));
      } else {
        diagnostic (A68_WARNING, p, WARNING_VOIDED, MOID (y));
      }
    }
  }
}

//! @brief Warn for things that are likely unintended.

static void semantic_pitfall (NODE_T * p, MOID_T * m, int c, int u)
{
// semantic_pitfall: warn for things that are likely unintended, for instance
//                   REF INT i := LOC INT := 0, which should probably be
//                   REF INT i = LOC INT := 0.
  if (IS (p, u)) {
    diagnostic (A68_WARNING, p, WARNING_UNINTENDED, MOID (p), u, m, c);
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, STOP)) {
    semantic_pitfall (SUB (p), m, c, u);
  }
}

//! @brief Insert coercion "a" in the tree.

static void make_coercion (NODE_T * l, int a, MOID_T * m)
{
  make_sub (l, l, a);
  MOID (l) = depref_rows (MOID (l), m);
}

//! @brief Make widening coercion.

static void make_widening_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  make_coercion (n, WIDENING, z);
  if (z != q) {
    make_widening_coercion (n, z, q);
  }
}

//! @brief Make ref rowing coercion.

static void make_ref_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
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

static void make_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
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

static void make_uniting_coercion (NODE_T * n, MOID_T * q)
{
  make_coercion (n, UNITING, derow (q));
  if (IS_ROW (q) || IS_FLEX (q)) {
    make_rowing_coercion (n, derow (q), q);
  }
}

//! @brief Make depreffing coercion.

static void make_depreffing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
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

static BOOL_T is_nonproc (MOID_T * p)
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

static void make_void (NODE_T * p, MOID_T * q)
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

static void make_strong (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (q == M_VOID && p != M_VOID) {
    make_void (n, p);
  } else {
    make_depreffing_coercion (n, p, q);
  }
}

//! @brief Mode check on bounds.

static void mode_check_bounds (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, M_INT, 0);
    mode_check_unit (p, &x, &y);
    if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&y), M_INT, MEEK, SAFE_DEFLEXING, UNIT);
    }
    mode_check_bounds (NEXT (p));
  } else {
    mode_check_bounds (SUB (p));
    mode_check_bounds (NEXT (p));
  }
}

//! @brief Mode check declarer.

static void mode_check_declarer (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, BOUNDS)) {
    mode_check_bounds (SUB (p));
    mode_check_declarer (NEXT (p));
  } else {
    mode_check_declarer (SUB (p));
    mode_check_declarer (NEXT (p));
  }
}

//! @brief Mode check identity declaration.

static void mode_check_identity_declaration (NODE_T * p)
{
  if (p != NO_NODE) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        mode_check_declarer (SUB (p));
        mode_check_identity_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        SOID_T x, y;
        make_soid (&x, STRONG, MOID (p), 0);
        mode_check_unit (NEXT_NEXT (p), &x, &y);
        if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
          cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
        } else if (MOID (&x) != MOID (&y)) {
// Check for instance, REF INT i = LOC REF INT.
          semantic_pitfall (NEXT_NEXT (p), MOID (&x), IDENTITY_DECLARATION, GENERATOR);
        }
        break;
      }
    default:
      {
        mode_check_identity_declaration (SUB (p));
        mode_check_identity_declaration (NEXT (p));
        break;
      }
    }
  }
}

//! @brief Mode check variable declaration.

static void mode_check_variable_declaration (NODE_T * p)
{
  if (p != NO_NODE) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        mode_check_declarer (SUB (p));
        mode_check_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP)) {
          SOID_T x, y;
          make_soid (&x, STRONG, SUB_MOID (p), 0);
          mode_check_unit (NEXT_NEXT (p), &x, &y);
          if (!is_coercible_in_context (&y, &x, FORCE_DEFLEXING)) {
            cannot_coerce (p, MOID (&y), MOID (&x), STRONG, FORCE_DEFLEXING, UNIT);
          } else if (SUB_MOID (&x) != MOID (&y)) {
// Check for instance, REF INT i = LOC REF INT.
            semantic_pitfall (NEXT_NEXT (p), MOID (&x), VARIABLE_DECLARATION, GENERATOR);
          }
        }
        break;
      }
    default:
      {
        mode_check_variable_declaration (SUB (p));
        mode_check_variable_declaration (NEXT (p));
        break;
      }
    }
  }
}

//! @brief Mode check routine text.

static void mode_check_routine_text (NODE_T * p, SOID_T * y)
{
  SOID_T w;
  if (IS (p, PARAMETER_PACK)) {
    mode_check_declarer (SUB (p));
    FORWARD (p);
  }
  mode_check_declarer (SUB (p));
  make_soid (&w, STRONG, MOID (p), 0);
  mode_check_unit (NEXT_NEXT (p), &w, y);
  if (!is_coercible_in_context (y, &w, FORCE_DEFLEXING)) {
    cannot_coerce (NEXT_NEXT (p), MOID (y), MOID (&w), STRONG, FORCE_DEFLEXING, UNIT);
  }
}

//! @brief Mode check proc declaration.

static void mode_check_proc_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, ROUTINE_TEXT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, NO_MOID, 0);
    mode_check_routine_text (SUB (p), &y);
  } else {
    mode_check_proc_declaration (SUB (p));
    mode_check_proc_declaration (NEXT (p));
  }
}

//! @brief Mode check brief op declaration.

static void mode_check_brief_op_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, DEFINING_OPERATOR)) {
    SOID_T y;
    if (MOID (p) != MOID (NEXT_NEXT (p))) {
      SOID_T y2, x;
      make_soid (&y2, NO_SORT, MOID (NEXT_NEXT (p)), 0);
      make_soid (&x, NO_SORT, MOID (p), 0);
      cannot_coerce (NEXT_NEXT (p), MOID (&y2), MOID (&x), STRONG, SKIP_DEFLEXING, ROUTINE_TEXT);
    }
    mode_check_routine_text (SUB (NEXT_NEXT (p)), &y);
  } else {
    mode_check_brief_op_declaration (SUB (p));
    mode_check_brief_op_declaration (NEXT (p));
  }
}

//! @brief Mode check op declaration.

static void mode_check_op_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, DEFINING_OPERATOR)) {
    SOID_T y, x;
    make_soid (&x, STRONG, MOID (p), 0);
    mode_check_unit (NEXT_NEXT (p), &x, &y);
    if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
    }
  } else {
    mode_check_op_declaration (SUB (p));
    mode_check_op_declaration (NEXT (p));
  }
}

//! @brief Mode check declaration list.

static void mode_check_declaration_list (NODE_T * p)
{
  if (p != NO_NODE) {
    switch (ATTRIBUTE (p)) {
    case IDENTITY_DECLARATION:
      {
        mode_check_identity_declaration (SUB (p));
        break;
      }
    case VARIABLE_DECLARATION:
      {
        mode_check_variable_declaration (SUB (p));
        break;
      }
    case MODE_DECLARATION:
      {
        mode_check_declarer (SUB (p));
        break;
      }
    case PROCEDURE_DECLARATION:
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        mode_check_proc_declaration (SUB (p));
        break;
      }
    case BRIEF_OPERATOR_DECLARATION:
      {
        mode_check_brief_op_declaration (SUB (p));
        break;
      }
    case OPERATOR_DECLARATION:
      {
        mode_check_op_declaration (SUB (p));
        break;
      }
    default:
      {
        mode_check_declaration_list (SUB (p));
        mode_check_declaration_list (NEXT (p));
        break;
      }
    }
  }
}

//! @brief Mode check serial clause.

static void mode_check_serial (SOID_T ** r, NODE_T * p, SOID_T * x, BOOL_T k)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, INITIALISER_SERIES)) {
    mode_check_serial (r, SUB (p), x, A68_FALSE);
    mode_check_serial (r, NEXT (p), x, k);
  } else if (IS (p, DECLARATION_LIST)) {
    mode_check_declaration_list (SUB (p));
  } else if (is_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, STOP)) {
    mode_check_serial (r, NEXT (p), x, k);
  } else if (is_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, STOP)) {
    if (NEXT (p) != NO_NODE) {
      if (IS (NEXT (p), EXIT_SYMBOL) || IS (NEXT (p), END_SYMBOL) || IS (NEXT (p), CLOSE_SYMBOL)) {
        mode_check_serial (r, SUB (p), x, A68_TRUE);
      } else {
        mode_check_serial (r, SUB (p), x, A68_FALSE);
      }
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      mode_check_serial (r, SUB (p), x, A68_TRUE);
    }
  } else if (IS (p, LABELED_UNIT)) {
    mode_check_serial (r, SUB (p), x, k);
  } else if (IS (p, UNIT)) {
    SOID_T y;
    if (k) {
      mode_check_unit (p, x, &y);
    } else {
      SOID_T w;
      make_soid (&w, STRONG, M_VOID, 0);
      mode_check_unit (p, &w, &y);
    }
    if (NEXT (p) != NO_NODE) {
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      if (k) {
        add_to_soid_list (r, p, &y);
      }
    }
  }
}

//! @brief Mode check serial clause units.

static void mode_check_serial_units (NODE_T * p, SOID_T * x, SOID_T * y, int att)
{
  SOID_T *top_sl = NO_SOID;
  (void) att;
  mode_check_serial (&top_sl, SUB (p), x, A68_TRUE);
  if (is_balanced (p, top_sl, SORT (x))) {
    MOID_T *result = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), result, SERIAL_CLAUSE);
  } else {
    make_soid (y, SORT (x), (MOID (x) != NO_MOID ? MOID (x) : M_ERROR), 0);
  }
  free_soid_list (top_sl);
}

//! @brief Mode check unit list.

static void mode_check_unit_list (SOID_T ** r, NODE_T * p, SOID_T * x)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT_LIST)) {
    mode_check_unit_list (r, SUB (p), x);
    mode_check_unit_list (r, NEXT (p), x);
  } else if (IS (p, COMMA_SYMBOL)) {
    mode_check_unit_list (r, NEXT (p), x);
  } else if (IS (p, UNIT)) {
    SOID_T y;
    mode_check_unit (p, x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_unit_list (r, NEXT (p), x);
  }
}

//! @brief Mode check struct display.

static void mode_check_struct_display (SOID_T ** r, NODE_T * p, PACK_T ** fields)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT_LIST)) {
    mode_check_struct_display (r, SUB (p), fields);
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (IS (p, COMMA_SYMBOL)) {
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (IS (p, UNIT)) {
    SOID_T x, y;
    if (*fields != NO_PACK) {
      make_soid (&x, STRONG, MOID (*fields), 0);
      FORWARD (*fields);
    } else {
      make_soid (&x, STRONG, NO_MOID, 0);
    }
    mode_check_unit (p, &x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_struct_display (r, NEXT (p), fields);
  }
}

//! @brief Mode check get specified moids.

static void mode_check_get_specified_moids (NODE_T * p, MOID_T * u)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (is_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, STOP)) {
      mode_check_get_specified_moids (SUB (p), u);
    } else if (IS (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      add_mode_to_pack (&(PACK (u)), m, NO_TEXT, NODE (m));
    }
  }
}

//! @brief Mode check specified unit list.

static void mode_check_specified_unit_list (SOID_T ** r, NODE_T * p, SOID_T * x, MOID_T * u)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (is_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, STOP)) {
      mode_check_specified_unit_list (r, SUB (p), x, u);
    } else if (IS (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      if (u != NO_MOID && !is_unitable (m, u, SAFE_DEFLEXING)) {
        diagnostic (A68_ERROR, p, ERROR_NO_COMPONENT, m, u);
      }
    } else if (IS (p, UNIT)) {
      SOID_T y;
      mode_check_unit (p, x, &y);
      add_to_soid_list (r, p, &y);
    }
  }
}

//! @brief Mode check united case parts.

static void mode_check_united_case_parts (SOID_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  MOID_T *u = NO_MOID, *v = NO_MOID, *w = NO_MOID;
// Check the CASE part and deduce the united mode.
  make_soid (&enq_expct, MEEK, NO_MOID, 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
// Deduce the united mode from the enquiry clause.
  u = depref_completely (MOID (&enq_yield));
  u = make_united_mode (u);
  u = depref_completely (u);
// Also deduce the united mode from the specifiers.
  v = new_moid ();
  ATTRIBUTE (v) = SERIES_MODE;
  mode_check_get_specified_moids (NEXT_SUB (NEXT (p)), v);
  v = make_united_mode (v);
// Determine a resulting union.
  if (u == M_HIP) {
    w = v;
  } else {
    if (IS (u, UNION_SYMBOL)) {
      BOOL_T uv, vu, some;
      investigate_firm_relations (PACK (u), PACK (v), &uv, &some);
      investigate_firm_relations (PACK (v), PACK (u), &vu, &some);
      if (uv && vu) {
// Every component has a specifier.
        w = u;
      } else if (!uv && !vu) {
// Hmmmm ... let the coercer sort it out.
        w = u;
      } else {
// This is all the balancing we allow here for the moment. Firmly related
// subsets are not valid so we absorb them. If this doesn't solve it then we
// get a coercion-error later.
        w = absorb_related_subsets (u);
      }
    } else {
      diagnostic (A68_ERROR, NEXT_SUB (p), ERROR_NO_UNION, u);
      return;
    }
  }
  MOID (SUB (p)) = w;
  FORWARD (p);
// Check the IN part.
  mode_check_specified_unit_list (ry, NEXT_SUB (p), x, w);
// OUSE, OUT, ESAC.
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (is_one_of (p, CONFORMITY_OUSE_PART, BRIEF_CONFORMITY_OUSE_PART, STOP)) {
      mode_check_united_case_parts (ry, SUB (p), x);
    }
  }
}

//! @brief Mode check united case.

static void mode_check_united_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T *top_sl = NO_SOID;
  MOID_T *z;
  mode_check_united_case_parts (&top_sl, p, x);
  if (!is_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NO_MOID) {
      make_soid (y, SORT (x), MOID (x), CONFORMITY_CLAUSE);

    } else {
      make_soid (y, SORT (x), M_ERROR, 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, CONFORMITY_CLAUSE);
  }
  free_soid_list (top_sl);
}

//! @brief Mode check unit list 2.

static void mode_check_unit_list_2 (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T *top_sl = NO_SOID;
  if (MOID (x) != NO_MOID) {
    if (IS_FLEX (MOID (x))) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (SUB_MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (IS_ROW (MOID (x))) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (IS (MOID (x), STRUCT_SYMBOL)) {
      PACK_T *y2 = PACK (MOID (x));
      mode_check_struct_display (&top_sl, SUB (p), &y2);
    } else {
      mode_check_unit_list (&top_sl, SUB (p), x);
    }
  } else {
    mode_check_unit_list (&top_sl, SUB (p), x);
  }
  make_soid (y, STRONG, pack_soids_in_moid (top_sl, STOWED_MODE), 0);
  free_soid_list (top_sl);
}

//! @brief Mode check closed.

static void mode_check_closed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, SERIAL_CLAUSE)) {
    mode_check_serial_units (p, x, y, SERIAL_CLAUSE);
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, STOP)) {
    mode_check_closed (NEXT (p), x, y);
  }
  MOID (p) = MOID (y);
}

//! @brief Mode check collateral.

static void mode_check_collateral (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (whether (p, BEGIN_SYMBOL, END_SYMBOL, STOP) || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, STOP)) {
    if (SORT (x) == STRONG) {
      if (MOID (x) == NO_MOID) {
        diagnostic (A68_ERROR, p, ERROR_VACUUM, "REF MODE");
      } else {
        make_soid (y, STRONG, M_VACUUM, 0);
      }
    } else {
      make_soid (y, STRONG, M_UNDEFINED, 0);
    }
  } else {
    if (IS (p, UNIT_LIST)) {
      mode_check_unit_list_2 (p, x, y);
    } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, STOP)) {
      mode_check_collateral (NEXT (p), x, y);
    }
    MOID (p) = MOID (y);
  }
}

//! @brief Mode check conditional 2.

static void mode_check_conditional_2 (SOID_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, MEEK, M_BOOL, 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!is_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, ELSE_PART, CHOICE, STOP)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (is_one_of (p, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      mode_check_conditional_2 (ry, SUB (p), x);
    }
  }
}

//! @brief Mode check conditional.

static void mode_check_conditional (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T *top_sl = NO_SOID;
  MOID_T *z;
  mode_check_conditional_2 (&top_sl, p, x);
  if (!is_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NO_MOID) {
      make_soid (y, SORT (x), MOID (x), CONDITIONAL_CLAUSE);
    } else {
      make_soid (y, SORT (x), M_ERROR, 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, CONDITIONAL_CLAUSE);
  }
  free_soid_list (top_sl);
}

//! @brief Mode check int case 2.

static void mode_check_int_case_2 (SOID_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, MEEK, M_INT, 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!is_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_unit_list (ry, NEXT_SUB (p), x);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (is_one_of (p, CASE_OUSE_PART, BRIEF_OUSE_PART, STOP)) {
      mode_check_int_case_2 (ry, SUB (p), x);
    }
  }
}

//! @brief Mode check int case.

static void mode_check_int_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T *top_sl = NO_SOID;
  MOID_T *z;
  mode_check_int_case_2 (&top_sl, p, x);
  if (!is_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NO_MOID) {
      make_soid (y, SORT (x), MOID (x), CASE_CLAUSE);
    } else {
      make_soid (y, SORT (x), M_ERROR, 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, CASE_CLAUSE);
  }
  free_soid_list (top_sl);
}

//! @brief Mode check loop 2.

static void mode_check_loop_2 (NODE_T * p, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, FOR_PART)) {
    mode_check_loop_2 (NEXT (p), y);
  } else if (is_one_of (p, FROM_PART, BY_PART, TO_PART, STOP)) {
    SOID_T ix, iy;
    make_soid (&ix, STRONG, M_INT, 0);
    mode_check_unit (NEXT_SUB (p), &ix, &iy);
    if (!is_coercible_in_context (&iy, &ix, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_SUB (p), MOID (&iy), M_INT, MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (IS (p, WHILE_PART)) {
    SOID_T enq_expct, enq_yield;
    make_soid (&enq_expct, MEEK, M_BOOL, 0);
    mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
    if (!is_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (is_one_of (p, DO_PART, ALT_DO_PART, STOP)) {
    SOID_T *z = NO_SOID;
    SOID_T ix;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&ix, STRONG, M_VOID, 0);
    if (IS (do_p, SERIAL_CLAUSE)) {
      mode_check_serial (&z, do_p, &ix, A68_TRUE);
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NO_NODE && IS (un_p, UNTIL_PART)) {
      SOID_T enq_expct, enq_yield;
      make_soid (&enq_expct, STRONG, M_BOOL, 0);
      mode_check_serial_units (NEXT_SUB (un_p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
      if (!is_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
        cannot_coerce (un_p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
      }
    }
    free_soid_list (z);
  }
}

//! @brief Mode check loop.

static void mode_check_loop (NODE_T * p, SOID_T * y)
{
  SOID_T *z = NO_SOID;
  mode_check_loop_2 (p, z);
  make_soid (y, STRONG, M_VOID, 0);
}

//! @brief Mode check enclosed.

void mode_check_enclosed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (IS (p, CLOSED_CLAUSE)) {
    mode_check_closed (SUB (p), x, y);
  } else if (IS (p, PARALLEL_CLAUSE)) {
    mode_check_collateral (SUB (NEXT_SUB (p)), x, y);
    make_soid (y, STRONG, M_VOID, 0);
    MOID (NEXT_SUB (p)) = M_VOID;
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    mode_check_collateral (SUB (p), x, y);
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    mode_check_conditional (SUB (p), x, y);
  } else if (IS (p, CASE_CLAUSE)) {
    mode_check_int_case (SUB (p), x, y);
  } else if (IS (p, CONFORMITY_CLAUSE)) {
    mode_check_united_case (SUB (p), x, y);
  } else if (IS (p, LOOP_CLAUSE)) {
    mode_check_loop (SUB (p), y);
  }
  MOID (p) = MOID (y);
}

//! @brief Search table for operator.

static TAG_T *search_table_for_operator (TAG_T * t, char *n, MOID_T * x, MOID_T * y)
{
  if (is_mode_isnt_well (x)) {
    return A68_PARSER (error_tag);
  } else if (y != NO_MOID && is_mode_isnt_well (y)) {
    return A68_PARSER (error_tag);
  }
  for (; t != NO_TAG; FORWARD (t)) {
    if (NSYMBOL (NODE (t)) == n) {
      PACK_T *p = PACK (MOID (t));
      if (is_coercible (x, MOID (p), FIRM, ALIAS_DEFLEXING)) {
        FORWARD (p);
        if (p == NO_PACK && y == NO_MOID) {
// Matched in case of a monadic.
          return t;
        } else if (p != NO_PACK && y != NO_MOID && is_coercible (y, MOID (p), FIRM, ALIAS_DEFLEXING)) {
// Matched in case of a dyadic.
          return t;
        }
      }
    }
  }
  return NO_TAG;
}

//! @brief Search chain of symbol tables and return matching operator "x n y" or "n x".

static TAG_T *search_table_chain_for_operator (TABLE_T * s, char *n, MOID_T * x, MOID_T * y)
{
  if (is_mode_isnt_well (x)) {
    return A68_PARSER (error_tag);
  } else if (y != NO_MOID && is_mode_isnt_well (y)) {
    return A68_PARSER (error_tag);
  }
  while (s != NO_TABLE) {
    TAG_T *z = search_table_for_operator (OPERATORS (s), n, x, y);
    if (z != NO_TAG) {
      return z;
    }
    BACKWARD (s);
  }
  return NO_TAG;
}

//! @brief Return a matching operator "x n y".

static TAG_T *find_operator (TABLE_T * s, char *n, MOID_T * x, MOID_T * y)
{
// Coercions to operand modes are FIRM.
  TAG_T *z;
  MOID_T *u, *v;
// (A) Catch exceptions first.
  if (x == NO_MOID && y == NO_MOID) {
    return NO_TAG;
  } else if (is_mode_isnt_well (x)) {
    return A68_PARSER (error_tag);
  } else if (y != NO_MOID && is_mode_isnt_well (y)) {
    return A68_PARSER (error_tag);
  }
// (B) MONADs.
  if (x != NO_MOID && y == NO_MOID) {
    z = search_table_chain_for_operator (s, n, x, NO_MOID);
    if (z != NO_TAG) {
      return z;
    } else {
// (B.2) A little trick to allow - (0, 1) or ABS (1, long pi).
      if (is_coercible (x, M_COMPLEX, STRONG, SAFE_DEFLEXING)) {
        z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_COMPLEX, NO_MOID);
        if (z != NO_TAG) {
          return z;
        }
      }
      if (is_coercible (x, M_LONG_COMPLEX, STRONG, SAFE_DEFLEXING)) {
        z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_LONG_COMPLEX, NO_MOID);
        if (z != NO_TAG) {
          return z;
        }
      }
      if (is_coercible (x, M_LONG_LONG_COMPLEX, STRONG, SAFE_DEFLEXING)) {
        z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_LONG_LONG_COMPLEX, NO_MOID);
      }
    }
    return NO_TAG;
  }
// (C) DYADs.
  z = search_table_chain_for_operator (s, n, x, y);
  if (z != NO_TAG) {
    return z;
  }
// (C.2) Vector and matrix "strong coercions" in standard environ.
  u = depref_completely (x);
  v = depref_completely (y);
  if ((u == M_ROW_REAL || u == M_ROW_ROW_REAL)
      || (v == M_ROW_REAL || v == M_ROW_ROW_REAL)
      || (u == M_ROW_COMPLEX || u == M_ROW_ROW_COMPLEX)
      || (v == M_ROW_COMPLEX || v == M_ROW_ROW_COMPLEX)) {
    if (u == M_INT) {
      z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_REAL, y);
      if (z != NO_TAG) {
        return z;
      }
      z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_COMPLEX, y);
      if (z != NO_TAG) {
        return z;
      }
    } else if (v == M_INT) {
      z = search_table_for_operator (OPERATORS (A68_STANDENV), n, x, M_REAL);
      if (z != NO_TAG) {
        return z;
      }
      z = search_table_for_operator (OPERATORS (A68_STANDENV), n, x, M_COMPLEX);
      if (z != NO_TAG) {
        return z;
      }
    } else if (u == M_REAL) {
      z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_COMPLEX, y);
      if (z != NO_TAG) {
        return z;
      }
    } else if (v == M_REAL) {
      z = search_table_for_operator (OPERATORS (A68_STANDENV), n, x, M_COMPLEX);
      if (z != NO_TAG) {
        return z;
      }
    }
  }
// (C.3) Look in standenv for an appropriate cross-term.
  u = make_series_from_moids (x, y);
  u = make_united_mode (u);
  v = get_balanced_mode (u, STRONG, NO_DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (OPERATORS (A68_STANDENV), n, v, v);
  if (z != NO_TAG) {
    return z;
  }
  if (is_coercible_series (u, M_REAL, STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_REAL, M_REAL);
    if (z != NO_TAG) {
      return z;
    }
  }
  if (is_coercible_series (u, M_LONG_REAL, STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_LONG_REAL, M_LONG_REAL);
    if (z != NO_TAG) {
      return z;
    }
  }
  if (is_coercible_series (u, M_LONG_LONG_REAL, STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_LONG_LONG_REAL, M_LONG_LONG_REAL);
    if (z != NO_TAG) {
      return z;
    }
  }
  if (is_coercible_series (u, M_COMPLEX, STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_COMPLEX, M_COMPLEX);
    if (z != NO_TAG) {
      return z;
    }
  }
  if (is_coercible_series (u, M_LONG_COMPLEX, STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_LONG_COMPLEX, M_LONG_COMPLEX);
    if (z != NO_TAG) {
      return z;
    }
  }
  if (is_coercible_series (u, M_LONG_LONG_COMPLEX, STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (A68_STANDENV), n, M_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX);
    if (z != NO_TAG) {
      return z;
    }
  }
// (C.4) Now allow for depreffing for REF REAL +:= INT and alike.
  v = get_balanced_mode (u, STRONG, DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (OPERATORS (A68_STANDENV), n, v, v);
  if (z != NO_TAG) {
    return z;
  }
  return NO_TAG;
}

//! @brief Mode check monadic operator.

static void mode_check_monadic_operator (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p != NO_NODE) {
    TAG_T *t;
    MOID_T *u;
    u = determine_unique_mode (y, SAFE_DEFLEXING);
    if (is_mode_isnt_well (u)) {
      make_soid (y, SORT (x), M_ERROR, 0);
    } else if (u == M_HIP) {
      diagnostic (A68_ERROR, NEXT (p), ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), M_ERROR, 0);
    } else {
      if (strchr (NOMADS, *(NSYMBOL (p))) != NO_TEXT) {
        t = NO_TAG;
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
        make_soid (y, SORT (x), M_ERROR, 0);
      } else {
        t = find_operator (TABLE (p), NSYMBOL (p), u, NO_MOID);
        if (t == NO_TAG) {
          diagnostic (A68_ERROR, p, ERROR_NO_MONADIC, u);
          make_soid (y, SORT (x), M_ERROR, 0);
        }
      }
      if (t != NO_TAG) {
        MOID (p) = MOID (t);
      }
      TAX (p) = t;
      if (t != NO_TAG && t != A68_PARSER (error_tag)) {
        MOID (p) = MOID (t);
        make_soid (y, SORT (x), SUB_MOID (t), 0);
      } else {
        MOID (p) = M_ERROR;
        make_soid (y, SORT (x), M_ERROR, 0);
      }
    }
  }
}

//! @brief Mode check monadic formula.

static void mode_check_monadic_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e;
  make_soid (&e, FIRM, NO_MOID, 0);
  mode_check_formula (NEXT (p), &e, y);
  mode_check_monadic_operator (p, &e, y);
  make_soid (y, SORT (x), MOID (y), 0);
}

//! @brief Mode check formula.

static void mode_check_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T ls, rs;
  TAG_T *op;
  MOID_T *u, *v;
  if (IS (p, MONADIC_FORMULA)) {
    mode_check_monadic_formula (SUB (p), x, &ls);
  } else if (IS (p, FORMULA)) {
    mode_check_formula (SUB (p), x, &ls);
  } else if (IS (p, SECONDARY)) {
    SOID_T e;
    make_soid (&e, FIRM, NO_MOID, 0);
    mode_check_unit (SUB (p), &e, &ls);
  }
  u = determine_unique_mode (&ls, SAFE_DEFLEXING);
  MOID (p) = u;
  if (NEXT (p) == NO_NODE) {
    make_soid (y, SORT (x), u, 0);
  } else {
    NODE_T *q = NEXT_NEXT (p);
    if (IS (q, MONADIC_FORMULA)) {
      mode_check_monadic_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (IS (q, FORMULA)) {
      mode_check_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (IS (q, SECONDARY)) {
      SOID_T e;
      make_soid (&e, FIRM, NO_MOID, 0);
      mode_check_unit (SUB (q), &e, &rs);
    }
    v = determine_unique_mode (&rs, SAFE_DEFLEXING);
    MOID (q) = v;
    if (is_mode_isnt_well (u) || is_mode_isnt_well (v)) {
      make_soid (y, SORT (x), M_ERROR, 0);
    } else if (u == M_HIP) {
      diagnostic (A68_ERROR, p, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), M_ERROR, 0);
    } else if (v == M_HIP) {
      diagnostic (A68_ERROR, q, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), M_ERROR, 0);
    } else {
      op = find_operator (TABLE (NEXT (p)), NSYMBOL (NEXT (p)), u, v);
      if (op == NO_TAG) {
        diagnostic (A68_ERROR, NEXT (p), ERROR_NO_DYADIC, u, v);
        make_soid (y, SORT (x), M_ERROR, 0);
      }
      if (op != NO_TAG) {
        MOID (NEXT (p)) = MOID (op);
      }
      TAX (NEXT (p)) = op;
      if (op != NO_TAG && op != A68_PARSER (error_tag)) {
        make_soid (y, SORT (x), SUB_MOID (op), 0);
      } else {
        make_soid (y, SORT (x), M_ERROR, 0);
      }
    }
  }
}

//! @brief Mode check assignation.

static void mode_check_assignation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T name, tmp, value;
  MOID_T *name_moid, *ori;
// Get destination mode.
  make_soid (&name, SOFT, NO_MOID, 0);
  mode_check_unit (SUB (p), &name, &tmp);
// SOFT coercion.
  ori = determine_unique_mode (&tmp, SAFE_DEFLEXING);
  name_moid = deproc_completely (ori);
  if (ATTRIBUTE (name_moid) != REF_SYMBOL) {
    if (IF_MODE_IS_WELL (name_moid)) {
      diagnostic (A68_ERROR, p, ERROR_NO_NAME, ori, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  MOID (p) = name_moid;
// Get source mode.
  make_soid (&name, STRONG, SUB (name_moid), 0);
  mode_check_unit (NEXT_NEXT (p), &name, &value);
  if (!is_coercible_in_context (&value, &name, FORCE_DEFLEXING)) {
    cannot_coerce (p, MOID (&value), MOID (&name), STRONG, FORCE_DEFLEXING, UNIT);
    make_soid (y, SORT (x), M_ERROR, 0);
  } else {
    make_soid (y, SORT (x), name_moid, 0);
  }
}

//! @brief Mode check identity relation.

static void mode_check_identity_relation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  MOID_T *lhs, *rhs, *oril, *orir;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, SOFT, NO_MOID, 0);
  mode_check_unit (SUB (ln), &e, &l);
  mode_check_unit (SUB (rn), &e, &r);
// SOFT coercion.
  oril = determine_unique_mode (&l, SAFE_DEFLEXING);
  orir = determine_unique_mode (&r, SAFE_DEFLEXING);
  lhs = deproc_completely (oril);
  rhs = deproc_completely (orir);
  if (IF_MODE_IS_WELL (lhs) && lhs != M_HIP && ATTRIBUTE (lhs) != REF_SYMBOL) {
    diagnostic (A68_ERROR, ln, ERROR_NO_NAME, oril, ATTRIBUTE (SUB (ln)));
    lhs = M_ERROR;
  }
  if (IF_MODE_IS_WELL (rhs) && rhs != M_HIP && ATTRIBUTE (rhs) != REF_SYMBOL) {
    diagnostic (A68_ERROR, rn, ERROR_NO_NAME, orir, ATTRIBUTE (SUB (rn)));
    rhs = M_ERROR;
  }
  if (lhs == M_HIP && rhs == M_HIP) {
    diagnostic (A68_ERROR, p, ERROR_NO_UNIQUE_MODE);
  }
  if (is_coercible (lhs, rhs, STRONG, SAFE_DEFLEXING)) {
    lhs = rhs;
  } else if (is_coercible (rhs, lhs, STRONG, SAFE_DEFLEXING)) {
    rhs = lhs;
  } else {
    cannot_coerce (NEXT (p), rhs, lhs, SOFT, SKIP_DEFLEXING, TERTIARY);
    lhs = rhs = M_ERROR;
  }
  MOID (ln) = lhs;
  MOID (rn) = rhs;
  make_soid (y, SORT (x), M_BOOL, 0);
}

//! @brief Mode check bool functions ANDF and ORF.

static void mode_check_bool_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, STRONG, M_BOOL, 0);
  mode_check_unit (SUB (ln), &e, &l);
  if (!is_coercible_in_context (&l, &e, SAFE_DEFLEXING)) {
    cannot_coerce (ln, MOID (&l), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  mode_check_unit (SUB (rn), &e, &r);
  if (!is_coercible_in_context (&r, &e, SAFE_DEFLEXING)) {
    cannot_coerce (rn, MOID (&r), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  MOID (ln) = M_BOOL;
  MOID (rn) = M_BOOL;
  make_soid (y, SORT (x), M_BOOL, 0);
}

//! @brief Mode check cast.

static void mode_check_cast (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w;
  mode_check_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  CAST (&w) = A68_TRUE;
  mode_check_enclosed (SUB_NEXT (p), &w, y);
  if (!is_coercible_in_context (y, &w, SAFE_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (y), MOID (&w), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
  }
  make_soid (y, SORT (x), MOID (p), 0);
}

//! @brief Mode check assertion.

static void mode_check_assertion (NODE_T * p)
{
  SOID_T w, y;
  make_soid (&w, STRONG, M_BOOL, 0);
  mode_check_enclosed (SUB_NEXT (p), &w, &y);
  SORT (&y) = SORT (&w);
  if (!is_coercible_in_context (&y, &w, NO_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (&y), MOID (&w), MEEK, NO_DEFLEXING, ENCLOSED_CLAUSE);
  }
}

//! @brief Mode check argument list.

static void mode_check_argument_list (SOID_T ** r, NODE_T * p, PACK_T ** x, PACK_T ** v, PACK_T ** w)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, GENERIC_ARGUMENT_LIST)) {
      ATTRIBUTE (p) = ARGUMENT_LIST;
    }
    if (IS (p, ARGUMENT_LIST)) {
      mode_check_argument_list (r, SUB (p), x, v, w);
    } else if (IS (p, UNIT)) {
      SOID_T y, z;
      if (*x != NO_PACK) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MOID (*x), NO_TEXT, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NO_MOID, 0);
      }
      mode_check_unit (p, &z, &y);
      add_to_soid_list (r, p, &y);
    } else if (IS (p, TRIMMER)) {
      SOID_T z;
      if (SUB (p) != NO_NODE) {
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, ARGUMENT);
        make_soid (&z, STRONG, M_ERROR, 0);
        add_mode_to_pack_end (v, M_VOID, NO_TEXT, p);
        add_mode_to_pack_end (w, MOID (*x), NO_TEXT, p);
        FORWARD (*x);
      } else if (*x != NO_PACK) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, M_VOID, NO_TEXT, p);
        add_mode_to_pack_end (w, MOID (*x), NO_TEXT, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NO_MOID, 0);
      }
      add_to_soid_list (r, p, &z);
    } else if (IS (p, SUB_SYMBOL) && !OPTION_BRACKETS (&A68_JOB)) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, CALL);
    }
  }
}

//! @brief Mode check argument list 2.

static void mode_check_argument_list_2 (NODE_T * p, PACK_T * x, SOID_T * y, PACK_T ** v, PACK_T ** w)
{
  SOID_T *top_sl = NO_SOID;
  mode_check_argument_list (&top_sl, SUB (p), &x, v, w);
  make_soid (y, STRONG, pack_soids_in_moid (top_sl, STOWED_MODE), 0);
  free_soid_list (top_sl);
}

//! @brief Mode check meek int.

static void mode_check_meek_int (NODE_T * p)
{
  SOID_T x, y;
  make_soid (&x, MEEK, M_INT, 0);
  mode_check_unit (p, &x, &y);
  if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&y), MOID (&x), MEEK, SAFE_DEFLEXING, 0);
  }
}

//! @brief Mode check trimmer.

static void mode_check_trimmer (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, TRIMMER)) {
    mode_check_trimmer (SUB (p));
  } else if (IS (p, UNIT)) {
    mode_check_meek_int (p);
    mode_check_trimmer (NEXT (p));
  } else {
    mode_check_trimmer (NEXT (p));
  }
}

//! @brief Mode check indexer.

static void mode_check_indexer (NODE_T * p, int *subs, int *trims)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, TRIMMER)) {
    (*trims)++;
    mode_check_trimmer (SUB (p));
  } else if (IS (p, UNIT)) {
    (*subs)++;
    mode_check_meek_int (p);
  } else {
    mode_check_indexer (SUB (p), subs, trims);
    mode_check_indexer (NEXT (p), subs, trims);
  }
}

//! @brief Mode check call.

static void mode_check_call (NODE_T * p, MOID_T * n, SOID_T * x, SOID_T * y)
{
  SOID_T d;
  MOID (p) = n;
// "partial_locale" is the mode of the locale.
  PARTIAL_LOCALE (GINFO (p)) = new_moid ();
  ATTRIBUTE (PARTIAL_LOCALE (GINFO (p))) = PROC_SYMBOL;
  PACK (PARTIAL_LOCALE (GINFO (p))) = NO_PACK;
  SUB (PARTIAL_LOCALE (GINFO (p))) = SUB (n);
// "partial_proc" is the mode of the resulting proc.
  PARTIAL_PROC (GINFO (p)) = new_moid ();
  ATTRIBUTE (PARTIAL_PROC (GINFO (p))) = PROC_SYMBOL;
  PACK (PARTIAL_PROC (GINFO (p))) = NO_PACK;
  SUB (PARTIAL_PROC (GINFO (p))) = SUB (n);
// Check arguments and construct modes.
  mode_check_argument_list_2 (NEXT (p), PACK (n), &d, &PACK (PARTIAL_LOCALE (GINFO (p))), &PACK (PARTIAL_PROC (GINFO (p))));
  DIM (PARTIAL_PROC (GINFO (p))) = count_pack_members (PACK (PARTIAL_PROC (GINFO (p))));
  DIM (PARTIAL_LOCALE (GINFO (p))) = count_pack_members (PACK (PARTIAL_LOCALE (GINFO (p))));
  PARTIAL_PROC (GINFO (p)) = register_extra_mode (&TOP_MOID (&A68_JOB), PARTIAL_PROC (GINFO (p)));
  PARTIAL_LOCALE (GINFO (p)) = register_extra_mode (&TOP_MOID (&A68_JOB), PARTIAL_LOCALE (GINFO (p)));
  if (DIM (MOID (&d)) != DIM (n)) {
    diagnostic (A68_ERROR, p, ERROR_ARGUMENT_NUMBER, n);
    make_soid (y, SORT (x), SUB (n), 0);
//  make_soid (y, SORT (x), M_ERROR, 0);.
  } else {
    if (!is_coercible (MOID (&d), n, STRONG, ALIAS_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), n, STRONG, ALIAS_DEFLEXING, ARGUMENT);
    }
    if (DIM (PARTIAL_PROC (GINFO (p))) == 0) {
      make_soid (y, SORT (x), SUB (n), 0);
    } else {
      if (OPTION_PORTCHECK (&A68_JOB)) {
        diagnostic (A68_WARNING | A68_FORCE_DIAGNOSTICS, NEXT (p), WARNING_EXTENSION);
      }
      make_soid (y, SORT (x), PARTIAL_PROC (GINFO (p)), 0);
    }
  }
}

//! @brief Mode check slice.

static void mode_check_slice (NODE_T * p, MOID_T * ori, SOID_T * x, SOID_T * y)
{
  BOOL_T is_ref;
  int rowdim, subs, trims;
  MOID_T *m = depref_completely (ori), *n = ori;
// WEAK coercion.
  while ((IS_REF (n) && !is_ref_row (n)) || (IS (n, PROC_SYMBOL) && PACK (n) == NO_PACK)) {
    n = depref_once (n);
  }
  if (n == NO_MOID || !(SLICE (DEFLEX (n)) != NO_MOID || is_ref_row (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic (A68_ERROR, p, ERROR_NO_ROW_OR_PROC, n, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), M_ERROR, 0);
  }

  MOID (p) = n;
  subs = trims = 0;
  mode_check_indexer (SUB_NEXT (p), &subs, &trims);
  if ((is_ref = is_ref_row (n)) != 0) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if ((subs + trims) != rowdim) {
    diagnostic (A68_ERROR, p, ERROR_INDEXER_NUMBER, n);
    make_soid (y, SORT (x), M_ERROR, 0);
  } else {
    if (subs > 0 && trims == 0) {
      ANNOTATION (NEXT (p)) = SLICE;
      m = n;
    } else {
      ANNOTATION (NEXT (p)) = TRIMMER;
      m = n;
    }
    while (subs > 0) {
      if (is_ref) {
        m = NAME (m);
      } else {
        if (IS_FLEX (m)) {
          m = SUB (m);
        }
        m = SLICE (m);
      }
      ABEND (m == NO_MOID, ERROR_INTERNAL_CONSISTENCY, __func__);
      subs--;
    }
// A trim cannot be but deflexed.
    if (ANNOTATION (NEXT (p)) == TRIMMER && TRIM (m) != NO_MOID) {
      ABEND (TRIM (m) == NO_MOID, ERROR_INTERNAL_CONSISTENCY, __func__);
      make_soid (y, SORT (x), TRIM (m), 0);
    } else {
      make_soid (y, SORT (x), m, 0);
    }
  }
}

//! @brief Mode check specification.

static int mode_check_specification (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  MOID_T *m, *ori;
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (SUB (p), &w, &d);
  ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  m = depref_completely (ori);
  if (IS (m, PROC_SYMBOL)) {
// Assume CALL.
    mode_check_call (p, m, x, y);
    return CALL;
  } else if (IS_ROW (m) || IS_FLEX (m)) {
// Assume SLICE.
    mode_check_slice (p, ori, x, y);
    return SLICE;
  } else {
    if (m != M_ERROR) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_MODE_SPECIFICATION, m);
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return PRIMARY;
  }
}

//! @brief Mode check selection.

static void mode_check_selection (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  BOOL_T coerce;
  MOID_T *n, *str, *ori;
  PACK_T *t, *t_2;
  char *fs;
  BOOL_T deflex = A68_FALSE;
  NODE_T *secondary = SUB_NEXT (p);
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (secondary, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  coerce = A68_TRUE;
  while (coerce) {
    if (IS (n, STRUCT_SYMBOL)) {
      coerce = A68_FALSE;
      t = PACK (n);
    } else if (IS_REF (n) && (IS_ROW (SUB (n)) || IS_FLEX (SUB (n))) && MULTIPLE (n) != NO_MOID) {
      coerce = A68_FALSE;
      deflex = A68_TRUE;
      t = PACK (MULTIPLE (n));
    } else if ((IS_ROW (n) || IS_FLEX (n)) && MULTIPLE (n) != NO_MOID) {
      coerce = A68_FALSE;
      deflex = A68_TRUE;
      t = PACK (MULTIPLE (n));
    } else if (IS_REF (n) && is_name_struct (n)) {
      coerce = A68_FALSE;
      t = PACK (NAME (n));
    } else if (is_deprefable (n)) {
      coerce = A68_TRUE;
      n = SUB (n);
      t = NO_PACK;
    } else {
      coerce = A68_FALSE;
      t = NO_PACK;
    }
  }
  if (t == NO_PACK) {
    if (IF_MODE_IS_WELL (MOID (&d))) {
      diagnostic (A68_ERROR, secondary, ERROR_NO_STRUCT, ori, ATTRIBUTE (secondary));
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  MOID (NEXT (p)) = n;
  fs = NSYMBOL (SUB (p));
  str = n;
  while (IS_REF (str)) {
    str = SUB (str);
  }
  if (IS_FLEX (str)) {
    str = SUB (str);
  }
  if (IS_ROW (str)) {
    str = SUB (str);
  }
  t_2 = PACK (str);
  while (t != NO_PACK && t_2 != NO_PACK) {
    if (TEXT (t) == fs) {
      MOID_T *ret = MOID (t);
      if (deflex && TRIM (ret) != NO_MOID) {
        ret = TRIM (ret);
      }
      make_soid (y, SORT (x), ret, 0);
      MOID (p) = ret;
      NODE_PACK (SUB (p)) = t_2;
      return;
    }
    FORWARD (t);
    FORWARD (t_2);
  }
  make_soid (&d, NO_SORT, n, 0);
  diagnostic (A68_ERROR, p, ERROR_NO_FIELD, str, fs);
  make_soid (y, SORT (x), M_ERROR, 0);
}

//! @brief Mode check diagonal.

static void mode_check_diagonal (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T is_ref;
  if (IS (p, TERTIARY)) {
    make_soid (&w, STRONG, M_INT, 0);
    mode_check_unit (p, &w, &d);
    if (!is_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (IS_REF (n) && !is_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NO_MOID && (IS_FLEX (n) || IS_REF_FLEX (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  if (n == NO_MOID || !(SLICE (DEFLEX (n)) != NO_MOID || is_ref_row (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  if ((is_ref = is_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 2) {
    diagnostic (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  MOID (tert) = n;
  if (is_ref) {
    n = NAME (n);
    ABEND (!IS_REF (n), ERROR_INTERNAL_CONSISTENCY, PM (n));
  } else {
    n = SLICE (n);
  }
  ABEND (n == NO_MOID, ERROR_INTERNAL_CONSISTENCY, __func__);
  make_soid (y, SORT (x), n, 0);
}

//! @brief Mode check transpose.

static void mode_check_transpose (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert = NEXT (p);
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T is_ref;
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (IS_REF (n) && !is_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NO_MOID && (IS_FLEX (n) || IS_REF_FLEX (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  if (n == NO_MOID || !(SLICE (DEFLEX (n)) != NO_MOID || is_ref_row (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  if ((is_ref = is_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 2) {
    diagnostic (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  MOID (tert) = n;
  ABEND (n == NO_MOID, ERROR_INTERNAL_CONSISTENCY, __func__);
  make_soid (y, SORT (x), n, 0);
}

//! @brief Mode check row or column function.

static void mode_check_row_column_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T is_ref;
  if (IS (p, TERTIARY)) {
    make_soid (&w, STRONG, M_INT, 0);
    mode_check_unit (p, &w, &d);
    if (!is_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (IS_REF (n) && !is_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NO_MOID && (IS_FLEX (n) || IS_REF_FLEX (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  if (n == NO_MOID || !(SLICE (DEFLEX (n)) != NO_MOID || is_ref_row (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    }
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  if ((is_ref = is_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 1) {
    diagnostic (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    make_soid (y, SORT (x), M_ERROR, 0);
    return;
  }
  MOID (tert) = n;
  ABEND (n == NO_MOID, ERROR_INTERNAL_CONSISTENCY, __func__);
  make_soid (y, SORT (x), ROWED (n), 0);
}

//! @brief Mode check format text.

static void mode_check_format_text (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    mode_check_format_text (SUB (p));
    if (IS (p, FORMAT_PATTERN)) {
      SOID_T x, y;
      make_soid (&x, STRONG, M_FORMAT, 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
      SOID_T x, y;
      make_soid (&x, STRONG, M_ROW_INT, 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (IS (p, DYNAMIC_REPLICATOR)) {
      SOID_T x, y;
      make_soid (&x, STRONG, M_INT, 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    }
  }
}

//! @brief Mode check unit.

static void mode_check_unit (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, STOP)) {
    mode_check_unit (SUB (p), x, y);
// Ex primary.
  } else if (IS (p, SPECIFICATION)) {
    ATTRIBUTE (p) = mode_check_specification (SUB (p), x, y);
    warn_for_voiding (p, x, y, ATTRIBUTE (p));
  } else if (IS (p, CAST)) {
    mode_check_cast (SUB (p), x, y);
    warn_for_voiding (p, x, y, CAST);
  } else if (IS (p, DENOTATION)) {
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, DENOTATION);
  } else if (IS (p, IDENTIFIER)) {
    if ((TAX (p) == NO_TAG) && (MOID (p) == NO_MOID)) {
      int att = first_tag_global (TABLE (p), NSYMBOL (p));
      if (att == STOP) {
        (void) add_tag (TABLE (p), IDENTIFIER, p, M_ERROR, NORMAL_IDENTIFIER);
        diagnostic (A68_ERROR, p, ERROR_UNDECLARED_TAG);
        MOID (p) = M_ERROR;
      } else {
        TAG_T *z = find_tag_global (TABLE (p), att, NSYMBOL (p));
        if (att == IDENTIFIER && z != NO_TAG) {
          MOID (p) = MOID (z);
        } else {
          (void) add_tag (TABLE (p), IDENTIFIER, p, M_ERROR, NORMAL_IDENTIFIER);
          diagnostic (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          MOID (p) = M_ERROR;
        }
      }
    }
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, IDENTIFIER);
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (IS (p, FORMAT_TEXT)) {
    mode_check_format_text (p);
    make_soid (y, SORT (x), M_FORMAT, 0);
    warn_for_voiding (p, x, y, FORMAT_TEXT);
// Ex secondary.
  } else if (IS (p, GENERATOR)) {
    mode_check_declarer (SUB (p));
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, GENERATOR);
  } else if (IS (p, SELECTION)) {
    mode_check_selection (SUB (p), x, y);
    warn_for_voiding (p, x, y, SELECTION);
// Ex tertiary.
  } else if (IS (p, NIHIL)) {
    make_soid (y, STRONG, M_HIP, 0);
  } else if (IS (p, FORMULA)) {
    mode_check_formula (p, x, y);
    if (!IS_REF (MOID (y))) {
      warn_for_voiding (p, x, y, FORMULA);
    }
  } else if (IS (p, DIAGONAL_FUNCTION)) {
    mode_check_diagonal (SUB (p), x, y);
    warn_for_voiding (p, x, y, DIAGONAL_FUNCTION);
  } else if (IS (p, TRANSPOSE_FUNCTION)) {
    mode_check_transpose (SUB (p), x, y);
    warn_for_voiding (p, x, y, TRANSPOSE_FUNCTION);
  } else if (IS (p, ROW_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, ROW_FUNCTION);
  } else if (IS (p, COLUMN_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, COLUMN_FUNCTION);
// Ex unit.
  } else if (is_one_of (p, JUMP, SKIP, STOP)) {
    if (SORT (x) != STRONG) {
      diagnostic (A68_WARNING, p, WARNING_HIP, SORT (x));
    }
//  make_soid (y, STRONG, M_HIP, 0);
    make_soid (y, SORT (x), M_HIP, 0);
  } else if (IS (p, ASSIGNATION)) {
    mode_check_assignation (SUB (p), x, y);
  } else if (IS (p, IDENTITY_RELATION)) {
    mode_check_identity_relation (SUB (p), x, y);
    warn_for_voiding (p, x, y, IDENTITY_RELATION);
  } else if (IS (p, ROUTINE_TEXT)) {
    mode_check_routine_text (SUB (p), y);
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, ROUTINE_TEXT);
  } else if (IS (p, ASSERTION)) {
    mode_check_assertion (SUB (p));
    make_soid (y, STRONG, M_VOID, 0);
  } else if (IS (p, AND_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, AND_FUNCTION);
  } else if (IS (p, OR_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, OR_FUNCTION);
  } else if (IS (p, CODE_CLAUSE)) {
    make_soid (y, STRONG, M_HIP, 0);
  }
  MOID (p) = MOID (y);
}

//! @brief Coerce bounds.

static void coerce_bounds (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      SOID_T q;
      make_soid (&q, MEEK, M_INT, 0);
      coerce_unit (p, &q);
    } else {
      coerce_bounds (SUB (p));
    }
  }
}

//! @brief Coerce declarer.

static void coerce_declarer (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, BOUNDS)) {
      coerce_bounds (SUB (p));
    } else {
      coerce_declarer (SUB (p));
    }
  }
}

//! @brief Coerce identity declaration.

static void coerce_identity_declaration (NODE_T * p)
{
  if (p != NO_NODE) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        coerce_declarer (SUB (p));
        coerce_identity_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        SOID_T q;
        make_soid (&q, STRONG, MOID (p), 0);
        coerce_unit (NEXT_NEXT (p), &q);
        break;
      }
    default:
      {
        coerce_identity_declaration (SUB (p));
        coerce_identity_declaration (NEXT (p));
        break;
      }
    }
  }
}

//! @brief Coerce variable declaration.

static void coerce_variable_declaration (NODE_T * p)
{
  if (p != NO_NODE) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        coerce_declarer (SUB (p));
        coerce_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP)) {
          SOID_T q;
          make_soid (&q, STRONG, SUB_MOID (p), 0);
          coerce_unit (NEXT_NEXT (p), &q);
          break;
        }
      }
    default:
      {
        coerce_variable_declaration (SUB (p));
        coerce_variable_declaration (NEXT (p));
        break;
      }
    }
  }
}

//! @brief Coerce routine text.

static void coerce_routine_text (NODE_T * p)
{
  SOID_T w;
  if (IS (p, PARAMETER_PACK)) {
    FORWARD (p);
  }
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

//! @brief Coerce proc declaration.

static void coerce_proc_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
  } else {
    coerce_proc_declaration (SUB (p));
    coerce_proc_declaration (NEXT (p));
  }
}

//! @brief Coerce_op_declaration.

static void coerce_op_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, DEFINING_OPERATOR)) {
    SOID_T q;
    make_soid (&q, STRONG, MOID (p), 0);
    coerce_unit (NEXT_NEXT (p), &q);
  } else {
    coerce_op_declaration (SUB (p));
    coerce_op_declaration (NEXT (p));
  }
}

//! @brief Coerce brief op declaration.

static void coerce_brief_op_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, DEFINING_OPERATOR)) {
    coerce_routine_text (SUB (NEXT_NEXT (p)));
  } else {
    coerce_brief_op_declaration (SUB (p));
    coerce_brief_op_declaration (NEXT (p));
  }
}

//! @brief Coerce declaration list.

static void coerce_declaration_list (NODE_T * p)
{
  if (p != NO_NODE) {
    switch (ATTRIBUTE (p)) {
    case IDENTITY_DECLARATION:
      {
        coerce_identity_declaration (SUB (p));
        break;
      }
    case VARIABLE_DECLARATION:
      {
        coerce_variable_declaration (SUB (p));
        break;
      }
    case MODE_DECLARATION:
      {
        coerce_declarer (SUB (p));
        break;
      }
    case PROCEDURE_DECLARATION:
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        coerce_proc_declaration (SUB (p));
        break;
      }
    case BRIEF_OPERATOR_DECLARATION:
      {
        coerce_brief_op_declaration (SUB (p));
        break;
      }
    case OPERATOR_DECLARATION:
      {
        coerce_op_declaration (SUB (p));
        break;
      }
    default:
      {
        coerce_declaration_list (SUB (p));
        coerce_declaration_list (NEXT (p));
        break;
      }
    }
  }
}

//! @brief Coerce serial.

static void coerce_serial (NODE_T * p, SOID_T * q, BOOL_T k)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, INITIALISER_SERIES)) {
    coerce_serial (SUB (p), q, A68_FALSE);
    coerce_serial (NEXT (p), q, k);
  } else if (IS (p, DECLARATION_LIST)) {
    coerce_declaration_list (SUB (p));
  } else if (is_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, STOP)) {
    coerce_serial (NEXT (p), q, k);
  } else if (is_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, STOP)) {
    NODE_T *z = NEXT (p);
    if (z != NO_NODE) {
      if (IS (z, EXIT_SYMBOL) || IS (z, END_SYMBOL) || IS (z, CLOSE_SYMBOL) || IS (z, OCCA_SYMBOL)) {
        coerce_serial (SUB (p), q, A68_TRUE);
      } else {
        coerce_serial (SUB (p), q, A68_FALSE);
      }
    } else {
      coerce_serial (SUB (p), q, A68_TRUE);
    }
    coerce_serial (NEXT (p), q, k);
  } else if (IS (p, LABELED_UNIT)) {
    coerce_serial (SUB (p), q, k);
  } else if (IS (p, UNIT)) {
    if (k) {
      coerce_unit (p, q);
    } else {
      SOID_T strongvoid;
      make_soid (&strongvoid, STRONG, M_VOID, 0);
      coerce_unit (p, &strongvoid);
    }
  }
}

//! @brief Coerce closed.

static void coerce_closed (NODE_T * p, SOID_T * q)
{
  if (IS (p, SERIAL_CLAUSE)) {
    coerce_serial (p, q, A68_TRUE);
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, STOP)) {
    coerce_closed (NEXT (p), q);
  }
}

//! @brief Coerce conditional.

static void coerce_conditional (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, M_BOOL, 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_serial (NEXT_SUB (p), q, A68_TRUE);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, ELSE_PART, CHOICE, STOP)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (is_one_of (p, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      coerce_conditional (SUB (p), q);
    }
  }
}

//! @brief Coerce unit list.

static void coerce_unit_list (NODE_T * p, SOID_T * q)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT_LIST)) {
    coerce_unit_list (SUB (p), q);
    coerce_unit_list (NEXT (p), q);
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, STOP)) {
    coerce_unit_list (NEXT (p), q);
  } else if (IS (p, UNIT)) {
    coerce_unit (p, q);
    coerce_unit_list (NEXT (p), q);
  }
}

//! @brief Coerce int case.

static void coerce_int_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, M_INT, 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (is_one_of (p, CASE_OUSE_PART, BRIEF_OUSE_PART, STOP)) {
      coerce_int_case (SUB (p), q);
    }
  }
}

//! @brief Coerce spec unit list.

static void coerce_spec_unit_list (NODE_T * p, SOID_T * q)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (is_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, STOP)) {
      coerce_spec_unit_list (SUB (p), q);
    } else if (IS (p, UNIT)) {
      coerce_unit (p, q);
    }
  }
}

//! @brief Coerce united case.

static void coerce_united_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MOID (SUB (p)), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_spec_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (is_one_of (p, CONFORMITY_OUSE_PART, BRIEF_CONFORMITY_OUSE_PART, STOP)) {
      coerce_united_case (SUB (p), q);
    }
  }
}

//! @brief Coerce loop.

static void coerce_loop (NODE_T * p)
{
  if (IS (p, FOR_PART)) {
    coerce_loop (NEXT (p));
  } else if (is_one_of (p, FROM_PART, BY_PART, TO_PART, STOP)) {
    SOID_T w;
    make_soid (&w, MEEK, M_INT, 0);
    coerce_unit (NEXT_SUB (p), &w);
    coerce_loop (NEXT (p));
  } else if (IS (p, WHILE_PART)) {
    SOID_T w;
    make_soid (&w, MEEK, M_BOOL, 0);
    coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
    coerce_loop (NEXT (p));
  } else if (is_one_of (p, DO_PART, ALT_DO_PART, STOP)) {
    SOID_T w;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&w, STRONG, M_VOID, 0);
    coerce_serial (do_p, &w, A68_TRUE);
    if (IS (do_p, SERIAL_CLAUSE)) {
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NO_NODE && IS (un_p, UNTIL_PART)) {
      SOID_T sw;
      make_soid (&sw, MEEK, M_BOOL, 0);
      coerce_serial (NEXT_SUB (un_p), &sw, A68_TRUE);
    }
  }
}

//! @brief Coerce struct display.

static void coerce_struct_display (PACK_T ** r, NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT_LIST)) {
    coerce_struct_display (r, SUB (p));
    coerce_struct_display (r, NEXT (p));
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, STOP)) {
    coerce_struct_display (r, NEXT (p));
  } else if (IS (p, UNIT)) {
    SOID_T s;
    make_soid (&s, STRONG, MOID (*r), 0);
    coerce_unit (p, &s);
    FORWARD (*r);
    coerce_struct_display (r, NEXT (p));
  }
}

//! @brief Coerce collateral.

static void coerce_collateral (NODE_T * p, SOID_T * q)
{
  if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, STOP) || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, STOP))) {
    if (IS (MOID (q), STRUCT_SYMBOL)) {
      PACK_T *t = PACK (MOID (q));
      coerce_struct_display (&t, p);
    } else if (IS_FLEX (MOID (q))) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (SUB_MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else if (IS_ROW (MOID (q))) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else {
// if (MOID (q) != M_VOID).
      coerce_unit_list (p, q);
    }
  }
}

//! @brief Coerce_enclosed.

void coerce_enclosed (NODE_T * p, SOID_T * q)
{
  if (IS (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (SUB (p), q);
  } else if (IS (p, CLOSED_CLAUSE)) {
    coerce_closed (SUB (p), q);
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    coerce_collateral (SUB (p), q);
  } else if (IS (p, PARALLEL_CLAUSE)) {
    coerce_collateral (SUB (NEXT_SUB (p)), q);
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    coerce_conditional (SUB (p), q);
  } else if (IS (p, CASE_CLAUSE)) {
    coerce_int_case (SUB (p), q);
  } else if (IS (p, CONFORMITY_CLAUSE)) {
    coerce_united_case (SUB (p), q);
  } else if (IS (p, LOOP_CLAUSE)) {
    coerce_loop (SUB (p));
  }
  MOID (p) = depref_rows (MOID (p), MOID (q));
}

//! @brief Get monad moid.

static MOID_T *get_monad_moid (NODE_T * p)
{
  if (TAX (p) != NO_TAG && TAX (p) != A68_PARSER (error_tag)) {
    MOID (p) = MOID (TAX (p));
    return MOID (PACK (MOID (p)));
  } else {
    return M_ERROR;
  }
}

//! @brief Coerce monad oper.

static void coerce_monad_oper (NODE_T * p, SOID_T * q)
{
  if (p != NO_NODE) {
    SOID_T z;
    make_soid (&z, FIRM, MOID (PACK (MOID (TAX (p)))), 0);
    INSERT_COERCIONS (NEXT (p), MOID (q), &z);
  }
}

//! @brief Coerce monad formula.

static void coerce_monad_formula (NODE_T * p)
{
  SOID_T e;
  make_soid (&e, STRONG, get_monad_moid (p), 0);
  coerce_operand (NEXT (p), &e);
  coerce_monad_oper (p, &e);
}

//! @brief Coerce operand.

static void coerce_operand (NODE_T * p, SOID_T * q)
{
  if (IS (p, MONADIC_FORMULA)) {
    coerce_monad_formula (SUB (p));
    if (MOID (p) != MOID (q)) {
      make_sub (p, p, FORMULA);
      INSERT_COERCIONS (p, MOID (p), q);
      make_sub (p, p, TERTIARY);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, SECONDARY)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
  }
}

//! @brief Coerce formula.

static void coerce_formula (NODE_T * p, SOID_T * q)
{
  (void) q;
  if (IS (p, MONADIC_FORMULA) && NEXT (p) == NO_NODE) {
    coerce_monad_formula (SUB (p));
  } else {
    if (TAX (NEXT (p)) != NO_TAG && TAX (NEXT (p)) != A68_PARSER (error_tag)) {
      SOID_T s;
      NODE_T *op = NEXT (p), *nq = NEXT_NEXT (p);
      MOID_T *w = MOID (op);
      MOID_T *u = MOID (PACK (w)), *v = MOID (NEXT (PACK (w)));
      make_soid (&s, STRONG, u, 0);
      coerce_operand (p, &s);
      make_soid (&s, STRONG, v, 0);
      coerce_operand (nq, &s);
    }
  }
}

//! @brief Coerce assignation.

static void coerce_assignation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, SOFT, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, SUB_MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

//! @brief Coerce relation.

static void coerce_relation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, MOID (NEXT_NEXT (p)), 0);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

//! @brief Coerce bool function.

static void coerce_bool_function (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, M_BOOL, 0);
  coerce_unit (SUB (p), &w);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

//! @brief Coerce assertion.

static void coerce_assertion (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, MEEK, M_BOOL, 0);
  coerce_enclosed (SUB_NEXT (p), &w);
}

//! @brief Coerce selection.

static void coerce_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

//! @brief Coerce cast.

static void coerce_cast (NODE_T * p)
{
  SOID_T w;
  coerce_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_enclosed (NEXT (p), &w);
}

//! @brief Coerce argument list.

static void coerce_argument_list (PACK_T ** r, NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ARGUMENT_LIST)) {
      coerce_argument_list (r, SUB (p));
    } else if (IS (p, UNIT)) {
      SOID_T s;
      make_soid (&s, STRONG, MOID (*r), 0);
      coerce_unit (p, &s);
      FORWARD (*r);
    } else if (IS (p, TRIMMER)) {
      FORWARD (*r);
    }
  }
}

//! @brief Coerce call.

static void coerce_call (NODE_T * p)
{
  MOID_T *proc = MOID (p);
  SOID_T w;
  PACK_T *t;
  make_soid (&w, MEEK, proc, 0);
  coerce_unit (SUB (p), &w);
  FORWARD (p);
  t = PACK (proc);
  coerce_argument_list (&t, SUB (p));
}

//! @brief Coerce meek int.

static void coerce_meek_int (NODE_T * p)
{
  SOID_T x;
  make_soid (&x, MEEK, M_INT, 0);
  coerce_unit (p, &x);
}

//! @brief Coerce trimmer.

static void coerce_trimmer (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, UNIT)) {
      coerce_meek_int (p);
      coerce_trimmer (NEXT (p));
    } else {
      coerce_trimmer (NEXT (p));
    }
  }
}

//! @brief Coerce indexer.

static void coerce_indexer (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, TRIMMER)) {
      coerce_trimmer (SUB (p));
    } else if (IS (p, UNIT)) {
      coerce_meek_int (p);
    } else {
      coerce_indexer (SUB (p));
      coerce_indexer (NEXT (p));
    }
  }
}

//! @brief Coerce_slice.

static void coerce_slice (NODE_T * p)
{
  SOID_T w;
  MOID_T *row;
  row = MOID (p);
  make_soid (&w, STRONG, row, 0);
  coerce_unit (SUB (p), &w);
  coerce_indexer (SUB_NEXT (p));
}

//! @brief Mode coerce diagonal.

static void coerce_diagonal (NODE_T * p)
{
  SOID_T w;
  if (IS (p, TERTIARY)) {
    make_soid (&w, MEEK, M_INT, 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

//! @brief Mode coerce transpose.

static void coerce_transpose (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

//! @brief Mode coerce row or column function.

static void coerce_row_column_function (NODE_T * p)
{
  SOID_T w;
  if (IS (p, TERTIARY)) {
    make_soid (&w, MEEK, M_INT, 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

//! @brief Coerce format text.

static void coerce_format_text (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    coerce_format_text (SUB (p));
    if (IS (p, FORMAT_PATTERN)) {
      SOID_T x;
      make_soid (&x, STRONG, M_FORMAT, 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
      SOID_T x;
      make_soid (&x, STRONG, M_ROW_INT, 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (IS (p, DYNAMIC_REPLICATOR)) {
      SOID_T x;
      make_soid (&x, STRONG, M_INT, 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    }
  }
}

//! @brief Coerce unit.

static void coerce_unit (NODE_T * p, SOID_T * q)
{
  if (p == NO_NODE) {
    return;
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, STOP)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
// Ex primary.
  } else if (IS (p, CALL)) {
    coerce_call (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, SLICE)) {
    coerce_slice (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, CAST)) {
    coerce_cast (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (is_one_of (p, DENOTATION, IDENTIFIER, STOP)) {
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, FORMAT_TEXT)) {
    coerce_format_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (p, q);
// Ex secondary.
  } else if (IS (p, SELECTION)) {
    coerce_selection (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, GENERATOR)) {
    coerce_declarer (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
// Ex tertiary.
  } else if (IS (p, NIHIL)) {
    if (ATTRIBUTE (MOID (q)) != REF_SYMBOL && MOID (q) != M_VOID) {
      diagnostic (A68_ERROR, p, ERROR_NO_NAME_REQUIRED);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, DIAGONAL_FUNCTION)) {
    coerce_diagonal (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, TRANSPOSE_FUNCTION)) {
    coerce_transpose (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, ROW_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, COLUMN_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
// Ex unit.
  } else if (IS (p, JUMP)) {
    if (MOID (q) == M_PROC_VOID) {
      make_sub (p, p, PROCEDURING);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, SKIP)) {
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, ASSIGNATION)) {
    coerce_assignation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, IDENTITY_RELATION)) {
    coerce_relation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (is_one_of (p, AND_FUNCTION, OR_FUNCTION, STOP)) {
    coerce_bool_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, ASSERTION)) {
    coerce_assertion (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  }
}

//! @brief Widen denotation depending on mode required, this is an extension to A68.

void widen_denotation (NODE_T * p)
{
#define WIDEN {\
  *q = *(SUB (q));\
  ATTRIBUTE (q) = DENOTATION;\
  MOID (q) = lm;\
  STATUS_SET (q, OPTIMAL_MASK);\
  }
#define WARN_WIDENING\
  if (OPTION_PORTCHECK (&A68_JOB) && !(STATUS_TEST (SUB (q), OPTIMAL_MASK))) {\
    diagnostic (A68_WARNING | A68_FORCE_DIAGNOSTICS, q, WARNING_WIDENING_NOT_PORTABLE);\
  }
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    widen_denotation (SUB (q));
    if (IS (q, WIDENING) && IS (SUB (q), DENOTATION)) {
      MOID_T *lm = MOID (q), *m = MOID (SUB (q));
      if (lm == M_LONG_LONG_INT && m == M_LONG_INT) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == M_LONG_INT && m == M_INT) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == M_LONG_LONG_REAL && m == M_LONG_REAL) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == M_LONG_REAL && m == M_REAL) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == M_LONG_REAL && m == M_LONG_INT) {
        WIDEN;
      }
      if (lm == M_REAL && m == M_INT) {
        WIDEN;
      }
      if (lm == M_LONG_LONG_BITS && m == M_LONG_BITS) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == M_LONG_BITS && m == M_BITS) {
        WARN_WIDENING;
        WIDEN;
      }
      return;
    }
  }
#undef WIDEN
#undef WARN_WIDENING
}
