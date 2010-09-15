/*!
\file coercion.c
\brief mode checker and coercion inserter
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program. If not, see <http://www.gnu.org/licenses/>.
*/

/*
This is the mode checker and coercion inserter. The syntax tree is traversed to
determine and check all modes. Next the tree is traversed again to insert
coercions.

Algol 68 contexts are SOFT, WEAK, MEEK, FIRM and STRONG.
These contexts are increasing in strength:

  SOFT: Deproceduring
  WEAK: Dereferencing to REF [] or REF STRUCT
  MEEK: Deproceduring and dereferencing
  FIRM: MEEK followed by uniting
  STRONG: FIRM followed by rowing, widening or voiding

Furthermore you will see in this file next switches:

(1) FORCE_DEFLEXING allows assignment compatibility between FLEX and non FLEX
rows. This can only be the case when there is no danger of altering bounds of a
non FLEX row.

(2) ALIAS_DEFLEXING prohibits aliasing a FLEX row to a non FLEX row (vice versa
is no problem) so that one cannot alter the bounds of a non FLEX row by
aliasing it to a FLEX row. This is particularly the case when passing names as
parameters to procedures:

   PROC x = (REF STRING s) VOID: ..., PROC y = (REF [] CHAR c) VOID: ...;

   x (LOC STRING);    # OK #

   x (LOC [10] CHAR); # Not OK, suppose x changes bounds of s! #
   y (LOC STRING);    # OK #
   y (LOC [10] CHAR); # OK #

(3) SAFE_DEFLEXING sets FLEX row apart from non FLEX row. This holds for names,
not for values, so common things are not rejected, for instance

   STRING x = read string;
   [] CHAR y = read string

(4) NO_DEFLEXING sets FLEX row apart from non FLEX row.
*/

#include "config.h"
#include "diagnostics.h"
#include "algol68g.h"

TAG_T *error_tag;

static SOID_LIST_T *top_soid_list = NULL;

static BOOL_T basic_coercions (MOID_T *, MOID_T *, int, int);
static BOOL_T whether_coercible (MOID_T *, MOID_T *, int, int);
static BOOL_T whether_nonproc (MOID_T *);

static void mode_check_enclosed (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_unit (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_formula (NODE_T *, SOID_T *, SOID_T *);

static void coerce_enclosed (NODE_T *, SOID_T *);
static void coerce_operand (NODE_T *, SOID_T *);
static void coerce_formula (NODE_T *, SOID_T *);
static void coerce_unit (NODE_T *, SOID_T *);

#define DEPREF A68_TRUE
#define NO_DEPREF A68_FALSE

#define WHETHER_MODE_IS_WELL(n) (! ((n) == MODE (ERROR) || (n) == MODE (UNDEFINED)))
#define INSERT_COERCIONS(n, p, q) make_strong ((n), (p), MOID (q))

/*!
\brief give accurate error message
\param p mode 1
\param q mode 2
\param context context
\param deflex deflexing regime
\param depth depth of recursion
\return error text
**/

static char *mode_error_text (NODE_T * n, MOID_T * p, MOID_T * q, int context, int deflex, int depth)
{
#define TAIL(z) (&(z)[strlen (z)])
  static char txt[BUFFER_SIZE];
  if (depth == 1) {
    txt[0] = NULL_CHAR;
  }
  if (WHETHER (p, SERIES_MODE)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (MOID (u) != NULL) {
          if (WHETHER (MOID (u), SERIES_MODE)) {
            (void) mode_error_text (n, MOID (u), q, context, deflex, depth + 1);
          } else if (!whether_coercible (MOID (u), q, context, deflex)) {
            int len = (int) strlen (txt);
            if (len > BUFFER_SIZE / 2) {
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " etcetera") >= 0);
            } else {
              if (strlen (txt) > 0) {
                ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " and ") >= 0);
              }
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
            }
          }
        }
      }
    }
    if (depth == 1) {
      ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " cannot be coerced to %s", moid_to_string (q, MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && WHETHER (q, FLEX_SYMBOL)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (!whether_coercible (MOID (u), SLICE (SUB (q)), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (SUB (q)), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && WHETHER (q, ROW_SYMBOL)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (!whether_coercible (MOID (u), SLICE (q), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (q), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && (WHETHER (q, PROC_SYMBOL) || WHETHER (q, STRUCT_SYMBOL))) {
    PACK_T *u = PACK (p), *v = PACK (q);
    if (u == NULL) {
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL && v != NULL; FORWARD (u), FORWARD (v)) {
        if (!whether_coercible (MOID (u), MOID (v), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, "%s cannot be coerced to %s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n), moid_to_string (MOID (v), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
    }
  }
  return (txt);
#undef TAIL
}

/*!
\brief cannot coerce error
\param p position in tree
\param from mode 1
\param to mode 2
\param context context
\param deflex deflexing regime
\param att attribute of context
**/

static void cannot_coerce (NODE_T * p, MOID_T * from, MOID_T * to, int context, int deflex, int att)
{
  char *txt = mode_error_text (p, from, to, context, deflex, 1);
  if (att == NULL_ATTRIBUTE) {
    if (strlen (txt) == 0) {
      diagnostic_node (A68_ERROR, p, "M cannot be coerced to M in C context", from, to, context);
    } else {
      diagnostic_node (A68_ERROR, p, "Y in C context", txt, context);
    }
  } else {
    if (strlen (txt) == 0) {
      diagnostic_node (A68_ERROR, p, "M cannot be coerced to M in C-A", from, to, context, att);
    } else {
      diagnostic_node (A68_ERROR, p, "Y in C-A", txt, context, att);
    }
  }
}

/*!
\brief driver for mode checker
\param p position in tree
**/

void mode_checker (NODE_T * p)
{
  if (WHETHER (p, PARTICULAR_PROGRAM)) {
    SOID_T x, y;
    top_soid_list = NULL;
    make_soid (&x, STRONG, MODE (VOID), 0);
    mode_check_enclosed (SUB (p), &x, &y);
    MOID (p) = MOID (&y);
  }
}

/*!
\brief driver for coercion inserions
\param p position in tree
**/

void coercion_inserter (NODE_T * p)
{
  if (WHETHER (p, PARTICULAR_PROGRAM)) {
    SOID_T q;
    make_soid (&q, STRONG, MODE (VOID), 0);
    coerce_enclosed (SUB (p), &q);
  }
}

/*!
\brief whether mode is not well defined
\param p mode, may be NULL
\return same
**/

static BOOL_T whether_mode_isnt_well (MOID_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else if (!WHETHER_MODE_IS_WELL (p)) {
    return (A68_TRUE);
  } else if (PACK (p) != NULL) {
    PACK_T *q = PACK (p);
    for (; q != NULL; FORWARD (q)) {
      if (!WHETHER_MODE_IS_WELL (MOID (q))) {
        return (A68_TRUE);
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief make SOID data structure
\param s soid buffer
\param sort sort
\param type mode
\param attribute attribute
**/

void make_soid (SOID_T * s, int sort, MOID_T * type, int attribute)
{
  ATTRIBUTE (s) = attribute;
  SORT (s) = sort;
  MOID (s) = type;
  CAST (s) = A68_FALSE;
}

/*!
\brief add SOID data to free chain
\param root top soid list
**/

void free_soid_list (SOID_LIST_T *root) {
  if (root != NULL) {
    SOID_LIST_T *q;
    for (q = root; NEXT (q) != NULL; FORWARD (q)) {
      /* skip */;
    }
    NEXT (q) = top_soid_list;
    top_soid_list = root;
  }
}

/*!
\brief add SOID data structure to soid list
\param root top soid list
\param nwhere position in tree
\param soid entry to add
**/

static void add_to_soid_list (SOID_LIST_T ** root, NODE_T * nwhere, SOID_T * soid)
{
  if (*root != NULL) {
    add_to_soid_list (&(NEXT (*root)), nwhere, soid);
  } else {
    SOID_LIST_T *new_one;
    if (top_soid_list == NULL) {
      new_one = (SOID_LIST_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (SOID_LIST_T));
      new_one->yield = (SOID_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (SOID_T));
    } else {
      new_one = top_soid_list;
      FORWARD (top_soid_list);
    }
    new_one->where = nwhere;
    make_soid (new_one->yield, SORT (soid), MOID (soid), 0);
    NEXT (new_one) = NULL;
    *root = new_one;
  }
}

/*!
\brief absorb nested series modes recursively
\param p mode
**/

static void absorb_series_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NULL, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NULL; FORWARD (t)) {
      if (MOID (t) != NULL && WHETHER (MOID (t), SERIES_MODE)) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NULL; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NULL, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NULL, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

/*!
\brief absorb nested series and united modes recursively
\param p mode
**/

static void absorb_series_union_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NULL, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NULL; FORWARD (t)) {
      if (MOID (t) != NULL && (WHETHER (MOID (t), SERIES_MODE) || WHETHER (MOID (t), UNION_SYMBOL))) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NULL; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NULL, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NULL, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

/*!
\brief make SERIES (u, v)
\param u mode 1
\param v mode 2
\return SERIES (u, v)
**/

static MOID_T *make_series_from_moids (MOID_T * u, MOID_T * v)
{
  MOID_T *x = new_moid ();
  ATTRIBUTE (x) = SERIES_MODE;
  add_mode_to_pack (&(PACK (x)), u, NULL, NODE (u));
  add_mode_to_pack (&(PACK (x)), v, NULL, NODE (v));
  absorb_series_pack (&x);
  DIM (x) = count_pack_members (PACK (x));
  add_single_moid_to_list (&top_moid_list, x, NULL);
  if (DIM (x) == 1) {
    return (MOID (PACK (x)));
  } else {
    return (x);
  }
}

/*!
\brief absorb firmly related unions in mode
\param m united mode
\return absorbed "m"
**/

static MOID_T *absorb_related_subsets (MOID_T * m)
{
/*
For instance invalid UNION (PROC REF UNION (A, B), A, B) -> valid UNION (A, B),
which is used in balancing conformity clauses.
*/
  int mods;
  do {
    PACK_T *u = NULL, *v;
    mods = 0;
    for (v = PACK (m); v != NULL; FORWARD (v)) {
      MOID_T *n = depref_completely (MOID (v));
      if (WHETHER (n, UNION_SYMBOL) && whether_subset (n, m, SAFE_DEFLEXING)) {
/* Unpack it. */
        PACK_T *w;
        for (w = PACK (n); w != NULL; FORWARD (w)) {
          add_mode_to_pack (&u, MOID (w), NULL, NODE (w));
        }
        mods++;
      } else {
        add_mode_to_pack (&u, MOID (v), NULL, NODE (v));
      }
    }
    PACK (m) = absorb_union_pack (u, &mods);
  } while (mods != 0);
  return (m);
}

/*!
\brief register mode in the global mode table, if mode is unique
\param u mode to enter
\return mode table entry
**/

static MOID_T *register_extra_mode (MOID_T * u)
{
  MOID_LIST_T *z;
/* Check for equivalency. */
  for (z = top_moid_list; z != NULL; FORWARD (z)) {
    MOID_T *v = MOID (z);
    BOOL_T w;
    free_postulate_list (top_postulate, NULL);
    top_postulate = NULL;
    w = (BOOL_T) (EQUIVALENT (v) == NULL && whether_modes_equivalent (v, u));
    if (w) {
      return (v);
    }
  }
/* Mode u is unique - include in the global moid list. */
  z = (MOID_LIST_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (MOID_LIST_T));
  z->coming_from_level = NULL;
  MOID (z) = u;
  NEXT (z) = top_moid_list;
  ABEND (z == NULL, "NULL pointer", "register_extra_mode");
  top_moid_list = z;
  add_single_moid_to_list (&top_moid_list, u, NULL);
  return (u);
}

/*!
\brief make united mode, from mode that is a SERIES (..)
\param m series mode
\return mode table entry
**/

static MOID_T *make_united_mode (MOID_T * m)
{
  MOID_T *u;
  PACK_T *v, *w;
  int mods;
  if (m == NULL) {
    return (MODE (ERROR));
  } else if (ATTRIBUTE (m) != SERIES_MODE) {
    return (m);
  }
/* Do not unite a single UNION. */
  if (DIM (m) == 1 && WHETHER (MOID (PACK (m)), UNION_SYMBOL)) {
    return (MOID (PACK (m)));
  }
/* Straighten the series. */
  absorb_series_union_pack (&m);
/* Copy the series into a UNION. */
  u = new_moid ();
  ATTRIBUTE (u) = UNION_SYMBOL;
  PACK (u) = NULL;
  v = PACK (u);
  w = PACK (m);
  for (w = PACK (m); w != NULL; FORWARD (w)) {
    add_mode_to_pack (&(PACK (u)), MOID (w), NULL, NODE (m));
  }
/* Absorb and contract the new UNION. */
  do {
    mods = 0;
    absorb_series_union_pack (&u);
    DIM (u) = count_pack_members (PACK (u));
    PACK (u) = absorb_union_pack (PACK (u), &mods);
    contract_union (u, &mods);
  } while (mods != 0);
/* A UNION of one mode is that mode itself. */
  if (DIM (u) == 1) {
    return (MOID (PACK (u)));
  } else {
    return (register_extra_mode (u));
  }
}

/*!
\brief pack soids in moid, gather resulting moids from terminators in a clause
\param top_sl top soid list
\param attribute mode attribute
\return mode table entry
**/

static MOID_T *pack_soids_in_moid (SOID_LIST_T * top_sl, int attribute)
{
  MOID_T *x = new_moid ();
  PACK_T *t, **p;
  NUMBER (x) = mode_count++;
  ATTRIBUTE (x) = attribute;
  DIM (x) = 0;
  SUB (x) = NULL;
  EQUIVALENT (x) = NULL;
  SLICE (x) = NULL;
  DEFLEXED (x) = NULL;
  NAME (x) = NULL;
  NEXT (x) = NULL;
  PACK (x) = NULL;
  p = &(PACK (x));
  for (; top_sl != NULL; FORWARD (top_sl)) {
    t = new_pack ();
    MOID (t) = MOID (top_sl->yield);
    t->text = NULL;
    NODE (t) = top_sl->where;
    NEXT (t) = NULL;
    DIM (x)++;
    *p = t;
    p = &NEXT (t);
  }
  add_single_moid_to_list (&top_moid_list, x, NULL);
  return (x);
}

/*!
\brief whether mode is deprefable
\param p mode
\return same
**/

BOOL_T whether_deprefable (MOID_T * p)
{
  if (WHETHER (p, REF_SYMBOL)) {
    return (A68_TRUE);
  } else {
    return ((BOOL_T) (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL));
  }
}

/*!
\brief depref mode once
\param p mode
\return single-depreffed mode
**/

static MOID_T *depref_once (MOID_T * p)
{
  if (WHETHER (p, REF_SYMBOL)) {
    return (SUB (p));
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (SUB (p));
  } else {
    return (NULL);
  }
}

/*!
\brief depref mode completely
\param p mode
\return completely depreffed mode
**/

MOID_T *depref_completely (MOID_T * p)
{
  while (whether_deprefable (p)) {
    p = depref_once (p);
  }
  return (p);
}

/*!
\brief deproc_completely
\param p mode
\return completely deprocedured mode
**/

static MOID_T *deproc_completely (MOID_T * p)
{
  while (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    p = depref_once (p);
  }
  return (p);
}

/*!
\brief depref rows
\param p mode
\param q mode
\return possibly depreffed mode
**/

static MOID_T *depref_rows (MOID_T * p, MOID_T * q)
{
  if (q == MODE (ROWS)) {
    while (whether_deprefable (p)) {
      p = depref_once (p);
    }
    return (p);
  } else {
    return (q);
  }
}

/*!
\brief derow mode, strip FLEX and BOUNDS
\param p mode
\return same
**/

static MOID_T *derow (MOID_T * p)
{
  if (WHETHER (p, ROW_SYMBOL) || WHETHER (p, FLEX_SYMBOL)) {
    return (derow (SUB (p)));
  } else {
    return (p);
  }
}

/*!
\brief whether rows type
\param p mode
\return same
**/

static BOOL_T whether_rows_type (MOID_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ROW_SYMBOL:
  case FLEX_SYMBOL:
    {
      return (A68_TRUE);
    }
  case UNION_SYMBOL:
    {
      PACK_T *t = PACK (p);
      BOOL_T go_on = A68_TRUE;
      while (t != NULL && go_on) {
        go_on &= whether_rows_type (MOID (t));
        FORWARD (t);
      }
      return (go_on);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether mode is PROC (REF FILE) VOID or FORMAT
\param p mode
\return same
**/

static BOOL_T whether_proc_ref_file_void_or_format (MOID_T * p)
{
  if (p == MODE (PROC_REF_FILE_VOID)) {
    return (A68_TRUE);
  } else if (p == MODE (FORMAT)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether mode can be transput
\param p mode
\return same
**/

static BOOL_T whether_transput_mode (MOID_T * p, char rw)
{
  if (p == MODE (INT)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_INT)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_INT)) {
    return (A68_TRUE);
  } else if (p == MODE (REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (BOOL)) {
    return (A68_TRUE);
  } else if (p == MODE (CHAR)) {
    return (A68_TRUE);
  } else if (p == MODE (BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (ROW_CHAR)) {
    return (A68_TRUE);
  } else if (p == MODE (STRING)) {
    return (A68_TRUE);
  } else if (p == MODE (SOUND)) {
    return (A68_TRUE);
  } else if (WHETHER (p, UNION_SYMBOL) || WHETHER (p, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (p);
    BOOL_T k = A68_TRUE;
    for (; q != NULL && k; FORWARD (q)) {
      k = (BOOL_T) (k & (whether_transput_mode (MOID (q), rw) || whether_proc_ref_file_void_or_format (MOID (q))));
    }
    return (k);
  } else if (WHETHER (p, FLEX_SYMBOL)) {
    return ((BOOL_T) (rw == 'w' ? whether_transput_mode (SUB (p), rw) : A68_FALSE));
  } else if (WHETHER (p, ROW_SYMBOL)) {
    return ((BOOL_T) (whether_transput_mode (SUB (p), rw) || whether_proc_ref_file_void_or_format (SUB (p))));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether mode is printable
\param p mode
\return same
**/

static BOOL_T whether_printable_mode (MOID_T * p)
{
  if (whether_proc_ref_file_void_or_format (p)) {
    return (A68_TRUE);
  } else {
    return (whether_transput_mode (p, 'w'));
  }
}

/*!
\brief whether mode is readable
\param p mode
\return same
**/

static BOOL_T whether_readable_mode (MOID_T * p)
{
  if (whether_proc_ref_file_void_or_format (p)) {
    return (A68_TRUE);
  } else {
    return ((BOOL_T) (WHETHER (p, REF_SYMBOL) ? whether_transput_mode (SUB (p), 'r') : A68_FALSE));
  }
}

/*!
\brief whether name struct
\param p mode
\return same
**/

static BOOL_T whether_name_struct (MOID_T * p)
{
  return ((BOOL_T) (p->name != NULL ? WHETHER (DEFLEX (SUB (p)), STRUCT_SYMBOL) : A68_FALSE));
}

/*!
\brief whether mode can be coerced to another in a certain context
\param u mode 1
\param v mode 2
\param context
\return same
**/

BOOL_T whether_modes_equal (MOID_T * u, MOID_T * v, int deflex)
{
/*
printf("\nwme strong u: %s", moid_to_string (u, 180, NULL));
printf("\nwme strong v: %s", moid_to_string (v, 180, NULL));
fflush(stdout);
*/
  if (u == v) {
    return (A68_TRUE);
  } else {
    switch (deflex) {
    case SKIP_DEFLEXING:
    case FORCE_DEFLEXING:
      {
/* Allow any interchange between FLEX [] A and [] A. */
        return ((BOOL_T) (DEFLEX (u) == DEFLEX (v)));
      }
    case ALIAS_DEFLEXING:
/* Cannot alias [] A to FLEX [] A, but vice versa is ok. */
      {
        if (u->has_ref) {
          return ((BOOL_T) (DEFLEX (u) == v));
        } else {
          return (whether_modes_equal (u, v, SAFE_DEFLEXING));
        }
      }
    case SAFE_DEFLEXING:
      {
/* Cannot alias [] A to FLEX [] A but values are ok. */
        if (!u->has_ref && !v->has_ref) {
          return (whether_modes_equal (u, v, FORCE_DEFLEXING));
        } else {
          return (A68_FALSE);
        }
    case NO_DEFLEXING:
        return (A68_FALSE);
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief yield mode to unite to
\param m mode
\param u united mode
\return same
**/

MOID_T *unites_to (MOID_T * m, MOID_T * u)
{
/* Uniting m->u. */
  MOID_T *v = NULL;
  PACK_T *p;
  if (u == MODE (SIMPLIN) || u == MODE (SIMPLOUT)) {
    return (m);
  }
  for (p = PACK (u); p != NULL; FORWARD (p)) {
/* Prefer []->[] over []->FLEX []. */
    if (m == MOID (p)) {
      v = MOID (p);
    } else if (v == NULL && DEFLEX (m) == DEFLEX (MOID (p))) {
      v = MOID (p);
    }
  }
  return (v);
}

/*!
\brief whether moid in pack
\param u mode
\param v pack
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_moid_in_pack (MOID_T * u, PACK_T * v, int deflex)
{
  for (; v != NULL; FORWARD (v)) {
    if (whether_modes_equal (u, MOID (v), deflex)) {
      return (A68_TRUE);
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether "p" is a subset of "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

BOOL_T whether_subset (MOID_T * p, MOID_T * q, int deflex)
{
  PACK_T *u = PACK (p);
  BOOL_T j = A68_TRUE;
  for (; u != NULL && j; FORWARD (u)) {
    j = (BOOL_T) (j && whether_moid_in_pack (MOID (u), PACK (q), deflex));
  }
  return (j);
}

/*!
\brief whether "p" can be united to UNION "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

BOOL_T whether_unitable (MOID_T * p, MOID_T * q, int deflex)
{
  if (WHETHER (q, UNION_SYMBOL)) {
    if (WHETHER (p, UNION_SYMBOL)) {
      return (whether_subset (p, q, deflex));
    } else {
      return (whether_moid_in_pack (p, PACK (q), deflex));
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether all or some components of "u" can be firmly coerced to a component mode of "v".
\param u mode
\param v mode 
\param all all coercible
\param some some coercible
**/

static void investigate_firm_relations (PACK_T * u, PACK_T * v, BOOL_T * all, BOOL_T * some)
{
  *all = A68_TRUE;
  *some = A68_FALSE;
  for (; v != NULL; FORWARD (v)) {
    PACK_T *w;
    BOOL_T k = A68_FALSE;
    for (w = u; w != NULL; FORWARD (w)) {
      k |= whether_coercible (MOID (w), MOID (v), FIRM, FORCE_DEFLEXING);
    }
    *some |= k;
    *all &= k;
  }
}

/*!
\brief whether there is a soft path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_softly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (whether_softly_coercible (SUB (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a weak path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_weakly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_weakly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a meek path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_meekly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_meekly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a firm path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_firmly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    return (A68_TRUE);
  } else if (whether_unitable (p, q, deflex)) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_firmly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" widens to "q"
\param p mode
\param q mode
\return same
**/

static MOID_T *widens_to (MOID_T * p, MOID_T * q)
{
  if (p == MODE (INT)) {
    if (q == MODE (LONG_INT) || q == MODE (LONGLONG_INT) || q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_INT));
    } else if (q == MODE (REAL) || q == MODE (COMPLEX)) {
      return (MODE (REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_INT)) {
    if (q == MODE (LONGLONG_INT)) {
      return (MODE (LONGLONG_INT));
    } else if (q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_INT)) {
    if (q == MODE (LONGLONG_REAL) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (REAL)) {
    if (q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_REAL));
    } else if (q == MODE (COMPLEX)) {
      return (MODE (COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (COMPLEX)) {
    if (q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_REAL)) {
    if (q == MODE (LONGLONG_REAL) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_REAL));
    } else if (q == MODE (LONG_COMPLEX)) {
      return (MODE (LONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_COMPLEX)) {
    if (q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_REAL)) {
    if (q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (BITS)) {
    if (q == MODE (LONG_BITS) || q == MODE (LONGLONG_BITS)) {
      return (MODE (LONG_BITS));
    } else if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_BITS)) {
    if (q == MODE (LONGLONG_BITS)) {
      return (MODE (LONGLONG_BITS));
    } else if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_BITS)) {
    if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (BYTES) && q == MODE (ROW_CHAR)) {
    return (MODE (ROW_CHAR));
  } else if (p == MODE (LONG_BYTES) && q == MODE (ROW_CHAR)) {
    return (MODE (ROW_CHAR));
  } else {
    return (NULL);
  }
}

/*!
\brief whether "p" widens to "q"
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_widenable (MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  if (z != NULL) {
    return ((BOOL_T) (z == q ? A68_TRUE : whether_widenable (z, q)));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" is a REF ROW
\param p mode
\return same
**/

static BOOL_T whether_ref_row (MOID_T * p)
{
  return ((BOOL_T) (p->name != NULL ? WHETHER (DEFLEX (SUB (p)), ROW_SYMBOL) : A68_FALSE));
}

/*!
\brief whether strong name
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_strong_name (MOID_T * p, MOID_T * q)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_ref_row (q)) {
    return (whether_strong_name (p, q->name));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether strong slice
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_strong_slice (MOID_T * p, MOID_T * q)
{
  if (p == q || whether_widenable (p, q)) {
    return (A68_TRUE);
  } else if (SLICE (q) != NULL) {
    return (whether_strong_slice (p, SLICE (q)));
  } else if (WHETHER (q, FLEX_SYMBOL)) {
    return (whether_strong_slice (p, SUB (q)));
  } else if (whether_ref_row (q)) {
    return (whether_strong_name (p, q));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether strongly coercible
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_strongly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
/* Keep this sequence of statements. */
/*
printf("\nstrong p: %s", moid_to_string (p, 180, NULL));
printf("\nstrong q: %s", moid_to_string (q, 180, NULL));
fflush(stdout);
*/
  if (p == q) {
    return (A68_TRUE);
  } else if (q == MODE (VOID)) {
    return (A68_TRUE);
  } else if ((q == MODE (SIMPLIN) || q == MODE (ROW_SIMPLIN)) && whether_readable_mode (p)) {
    return (A68_TRUE);
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    return (A68_TRUE);
  } else if (whether_unitable (p, derow (q), deflex)) {
    return (A68_TRUE);
  }
  if (whether_ref_row (q) && whether_strong_name (p, q)) {
    return (A68_TRUE);
  } else if (SLICE (q) != NULL && whether_strong_slice (p, q)) {
    return (A68_TRUE);
  } else if (WHETHER (q, FLEX_SYMBOL) && whether_strong_slice (p, q)) {
    return (A68_TRUE);
  } else if (whether_widenable (p, q)) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_strongly_coercible (depref_once (p), q, deflex));
  } else if (q == MODE (SIMPLOUT) || q == MODE (ROW_SIMPLOUT)) {
    return (whether_printable_mode (p));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether firm
\param p mode
\param q mode
\return same
**/

BOOL_T whether_firm (MOID_T * p, MOID_T * q)
{
  return ((BOOL_T) (whether_firmly_coercible (p, q, SAFE_DEFLEXING) || whether_firmly_coercible (q, p, SAFE_DEFLEXING)));
}

/*!
\brief whether coercible stowed
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_stowed (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c == STRONG) {
    if (q == MODE (VOID)) {
      return (A68_TRUE);
    } else if (WHETHER (q, FLEX_SYMBOL)) {
      PACK_T *u = PACK (p);
      BOOL_T j = A68_TRUE;
      for (; u != NULL && j; FORWARD (u)) {
        j &= whether_coercible (MOID (u), SLICE (SUB (q)), c, deflex);
      }
      return (j);
    } else if (WHETHER (q, ROW_SYMBOL)) {
      PACK_T *u = PACK (p);
      BOOL_T j = A68_TRUE;
      for (; u != NULL && j; FORWARD (u)) {
        j &= whether_coercible (MOID (u), SLICE (q), c, deflex);
      }
      return (j);
    } else if (WHETHER (q, PROC_SYMBOL) || WHETHER (q, STRUCT_SYMBOL)) {
      PACK_T *u = PACK (p), *v = PACK (q);
      if (DIM (p) != DIM (q)) {
        return (A68_FALSE);
      } else {
        BOOL_T j = A68_TRUE;
        while (u != NULL && v != NULL && j) {
          j &= whether_coercible (MOID (u), MOID (v), c, deflex);
          FORWARD (u);
          FORWARD (v);
        }
        return (j);
      }
    } else {
      return (A68_FALSE);
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether coercible series
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_series (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c != STRONG) {
    return (A68_FALSE);
  } else if (p == NULL || q == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, SERIES_MODE) && PACK (p) == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (q, SERIES_MODE) && PACK (q) == NULL) {
    return (A68_FALSE);
  } else if (PACK (p) == NULL) {
    return (whether_coercible (p, q, c, deflex));
  } else {
    PACK_T *u = PACK (p);
    BOOL_T j = A68_TRUE;
    for (; u != NULL && j; FORWARD (u)) {
      if (MOID (u) != NULL) {
        j &= whether_coercible (MOID (u), q, c, deflex);
      }
    }
    return (j);
  }
}

/*!
\brief basic coercions
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T basic_coercions (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (c == NO_SORT) {
    return ((BOOL_T) (p == q));
  } else if (c == SOFT) {
    return (whether_softly_coercible (p, q, deflex));
  } else if (c == WEAK) {
    return (whether_weakly_coercible (p, q, deflex));
  } else if (c == MEEK) {
    return (whether_meekly_coercible (p, q, deflex));
  } else if (c == FIRM) {
    return (whether_firmly_coercible (p, q, deflex));
  } else if (c == STRONG) {
    return (whether_strongly_coercible (p, q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" can be coerced to "q" in a "c" context
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

BOOL_T whether_coercible (MOID_T * p, MOID_T * q, int c, int deflex)
{
/*
printf("\np: %s", moid_to_string (p, 180, NULL));
printf("\nq: %s", moid_to_string (q, 180, NULL));
fflush(stdout);
*/
  if (whether_mode_isnt_well (p) || whether_mode_isnt_well (q)) {
    return (A68_TRUE);
  } else if (p == q) {
    return (A68_TRUE);
  } else if (p == MODE (HIP)) {
    return (A68_TRUE);
  } else if (WHETHER (p, STOWED_MODE)) {
    return (whether_coercible_stowed (p, q, c, deflex));
  } else if (WHETHER (p, SERIES_MODE)) {
    return (whether_coercible_series (p, q, c, deflex));
  } else if (p == MODE (VACUUM) && WHETHER (DEFLEX (q), ROW_SYMBOL)) {
    return (A68_TRUE);
  } else if (basic_coercions (p, q, c, deflex)) {
    return (A68_TRUE);
  } else if (deflex == FORCE_DEFLEXING) {
/* Allow for any interchange between FLEX [] A and [] A. */
    return (basic_coercions (DEFLEX (p), DEFLEX (q), c, FORCE_DEFLEXING));
  } else if (deflex == ALIAS_DEFLEXING) {
/* No aliasing of REF [] and REF FLEX [], but vv is ok and values too. */
    if (p->has_ref) {
      return (basic_coercions (DEFLEX (p), q, c, ALIAS_DEFLEXING));
    } else {
      return (whether_coercible (p, q, c, SAFE_DEFLEXING));
    }
  } else if (deflex == SAFE_DEFLEXING) {
/* No aliasing of REF [] and REF FLEX [], but ok and values too. */
    if (!p->has_ref && !q->has_ref) {
      return (whether_coercible (p, q, c, FORCE_DEFLEXING));
    } else {
      return (basic_coercions (p, q, c, SAFE_DEFLEXING));
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether coercible in context
\param p soid
\param q soid
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_in_context (SOID_T * p, SOID_T * q, int deflex)
{
  if (SORT (p) != SORT (q)) {
    return (A68_FALSE);
  } else if (MOID (p) == MOID (q)) {
    return (A68_TRUE);
  } else {
    return (whether_coercible (MOID (p), MOID (q), SORT (q), deflex));
  }
}

/*!
\brief whether list "y" is balanced
\param n position in tree
\param y soid list
\param sort sort
\return same
**/

static BOOL_T whether_balanced (NODE_T * n, SOID_LIST_T * y, int sort)
{
  if (sort == STRONG) {
    return (A68_TRUE);
  } else {
    BOOL_T k = A68_FALSE;
    for (; y != NULL && !k; FORWARD (y)) {
      SOID_T *z = y->yield;
      k = (BOOL_T) (WHETHER_NOT (MOID (z), STOWED_MODE));
    }
    if (k == A68_FALSE) {
      diagnostic_node (A68_ERROR, n, ERROR_NO_UNIQUE_MODE);
    }
    return (k);
  }
}

/*!
\brief a moid from "m" to which all other members can be coerced
\param m mode
\param sort sort
\param return_depreffed whether to depref
\param deflex deflexing regime
\return same
**/

MOID_T *get_balanced_mode (MOID_T * m, int sort, BOOL_T return_depreffed, int deflex)
{
  MOID_T *common = NULL;
  if (m != NULL && !whether_mode_isnt_well (m) && WHETHER (m, UNION_SYMBOL)) {
    int depref_level;
    BOOL_T go_on = A68_TRUE;
/* Test for increasing depreffing. */
    for (depref_level = 0; go_on; depref_level++) {
      PACK_T *p;
      go_on = A68_FALSE;
/* Test the whole pack. */
      for (p = PACK (m); p != NULL; FORWARD (p)) {
/* HIPs are not eligible of course. */
        if (MOID (p) != MODE (HIP)) {
          MOID_T *candidate = MOID (p);
          int k;
/* Depref as far as allowed. */
          for (k = depref_level; k > 0 && whether_deprefable (candidate); k--) {
            candidate = depref_once (candidate);
          }
/* Only need testing if all allowed deprefs succeeded. */
          if (k == 0) {
            PACK_T *q;
            MOID_T *to = return_depreffed ? depref_completely (candidate) : candidate;
            BOOL_T all_coercible = A68_TRUE;
            go_on = A68_TRUE;
            for (q = PACK (m); q != NULL && all_coercible; FORWARD (q)) {
              MOID_T *from = MOID (q);
              if (p != q && from != to) {
                all_coercible &= whether_coercible (from, to, sort, deflex);
              }
            }
/* If the pack is coercible to the candidate, we mark the candidate.
   We continue searching for longest series of REF REF PROC REF .. */
            if (all_coercible) {
              MOID_T *mark = (return_depreffed ? MOID (p) : candidate);
              if (common == NULL) {
                common = mark;
              } else if (WHETHER (candidate, FLEX_SYMBOL) && DEFLEX (candidate) == common) {
/* We prefer FLEX. */
                common = mark;
              }
            }
          }
        }
      }                         /* for */
    }                           /* for */
  }
  return (common == NULL ? m : common);
}

/*!
\brief whether we can search a common mode from a clause or not
\param att attribute
\return same
**/

static BOOL_T clause_allows_balancing (int att)
{
  switch (att) {
  case CLOSED_CLAUSE:
  case CONDITIONAL_CLAUSE:
  case INTEGER_CASE_CLAUSE:
  case SERIAL_CLAUSE:
  case UNITED_CASE_CLAUSE:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief a unique mode from "z"
\param z soid
\param deflex deflexing regime
\return same
**/

static MOID_T *determine_unique_mode (SOID_T * z, int deflex)
{
  if (z == NULL) {
    return (NULL);
  } else {
    MOID_T *x = MOID (z);
    if (whether_mode_isnt_well (x)) {
      return (MODE (ERROR));
    }
    x = make_united_mode (x);
    if (clause_allows_balancing (ATTRIBUTE (z))) {
      return (get_balanced_mode (x, STRONG, NO_DEPREF, deflex));
    } else {
      return (x);
    }
  }
}

/*!
\brief give a warning when a value is silently discarded
\param p position in tree
\param x soid
\param y soid
\param c context
**/

static void warn_for_voiding (NODE_T * p, SOID_T * x, SOID_T * y, int c)
{
  (void) c;
  if (CAST (x) == A68_FALSE) {
    if (MOID (x) == MODE (VOID) && MOID (y) != MODE (ERROR) && !(MOID (y) == MODE (VOID) || !whether_nonproc (MOID (y)))) {
      if (WHETHER (p, FORMULA)) {
        diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_VOIDED, MOID (y));
      } else {
        diagnostic_node (A68_WARNING, p, WARNING_VOIDED, MOID (y));
      }
    }
  }
}

/*!
\brief warn for things that are likely unintended
\param p position in tree
\param m moid
\param c context
\param u attribute
**/

static void semantic_pitfall (NODE_T * p, MOID_T * m, int c, int u)
{
/*
semantic_pitfall: warn for things that are likely unintended, for instance
                  REF INT i := LOC INT := 0, which should probably be
                  REF INT i = LOC INT := 0.
*/
  if (WHETHER (p, u)) {
    diagnostic_node (A68_WARNING, p, WARNING_UNINTENDED, MOID (p), u, m, c);
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    semantic_pitfall (SUB (p), m, c, u);
  }
}

/*!
\brief insert coercion "a" in the tree
\param l position in tree
\param a attribute
\param m (coerced) moid
**/

static void make_coercion (NODE_T * l, int a, MOID_T * m)
{
  make_sub (l, l, a);
  MOID (l) = depref_rows (MOID (l), m);
}

/*!
\brief make widening coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_widening_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  make_coercion (n, WIDENING, z);
  if (z != q) {
    make_widening_coercion (n, z, q);
  }
}

/*!
\brief make ref rowing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_ref_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (whether_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (whether_ref_row (q)) {
      make_ref_rowing_coercion (n, p, q->name);
      make_coercion (n, ROWING, q);
    }
  }
}

/*!
\brief make rowing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (whether_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (SLICE (q) != NULL) {
      make_rowing_coercion (n, p, SLICE (q));
      make_coercion (n, ROWING, q);
    } else if (WHETHER (q, FLEX_SYMBOL)) {
      make_rowing_coercion (n, p, SUB (q));
    } else if (whether_ref_row (q)) {
      make_ref_rowing_coercion (n, p, q);
    }
  }
}

/*!
\brief make uniting coercion
\param n position in tree
\param q mode
**/

static void make_uniting_coercion (NODE_T * n, MOID_T * q)
{
  make_coercion (n, UNITING, derow (q));
  if (WHETHER (q, ROW_SYMBOL) || WHETHER (q, FLEX_SYMBOL)) {
    make_rowing_coercion (n, derow (q), q);
  }
}

/*!
\brief make depreffing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_depreffing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) == DEFLEX (q)) {
    return;
  } else if (q == MODE (SIMPLOUT) && whether_printable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == MODE (ROW_SIMPLOUT) && whether_printable_mode (p)) {
    make_coercion (n, UNITING, MODE (SIMPLOUT));
    make_coercion (n, ROWING, MODE (ROW_SIMPLOUT));
  } else if (q == MODE (SIMPLIN) && whether_readable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == MODE (ROW_SIMPLIN) && whether_readable_mode (p)) {
    make_coercion (n, UNITING, MODE (SIMPLIN));
    make_coercion (n, ROWING, MODE (ROW_SIMPLIN));
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    make_coercion (n, UNITING, MODE (ROWS));
    MOID (n) = MODE (ROWS);
  } else if (whether_widenable (p, q)) {
    make_widening_coercion (n, p, q);
  } else if (whether_unitable (p, derow (q), SAFE_DEFLEXING)) {
    make_uniting_coercion (n, q);
  } else if (whether_ref_row (q) && whether_strong_name (p, q)) {
    make_ref_rowing_coercion (n, p, q);
  } else if (SLICE (q) != NULL && whether_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (WHETHER (q, FLEX_SYMBOL) && whether_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (WHETHER (p, REF_SYMBOL)) {
    MOID_T *r = /* DEFLEX */ (SUB (p));
    make_coercion (n, DEREFERENCING, r);
    make_depreffing_coercion (n, r, q);
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    MOID_T *r = SUB (p);
    make_coercion (n, DEPROCEDURING, r);
    make_depreffing_coercion (n, r, q);
  } else if (p != q) {
    cannot_coerce (n, p, q, NO_SORT, SKIP_DEFLEXING, 0);
  }
}

/*!
\brief whether p is a nonproc mode (that is voided directly)
\param p mode
\return same
**/

static BOOL_T whether_nonproc (MOID_T * p)
{
  if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (whether_nonproc (SUB (p)));
  } else {
    return (A68_TRUE);
  }
}

/*!
\brief make_void: voiden in an appropriate way
\param p position in tree
\param q mode
**/

static void make_void (NODE_T * p, MOID_T * q)
{
  switch (ATTRIBUTE (p)) {
  case ASSIGNATION:
  case IDENTITY_RELATION:
  case GENERATOR:
  case CAST:
  case DENOTATION:
    {
      make_coercion (p, VOIDING, MODE (VOID));
      return;
    }
  }
/* MORFs are an involved case. */
  switch (ATTRIBUTE (p)) {
  case SELECTION:
  case SLICE:
  case ROUTINE_TEXT:
  case FORMULA:
  case CALL:
  case IDENTIFIER:
    {
/* A nonproc moid value is eliminated directly. */
      if (whether_nonproc (q)) {
        make_coercion (p, VOIDING, MODE (VOID));
        return;
      } else {
/* Descend the chain of e.g. REF PROC .. until a nonproc moid remains. */
        MOID_T *z = q;
        while (!whether_nonproc (z)) {
          if (WHETHER (z, REF_SYMBOL)) {
            make_coercion (p, DEREFERENCING, SUB (z));
          }
          if (WHETHER (z, PROC_SYMBOL) && NODE_PACK (p) == NULL) {
            make_coercion (p, DEPROCEDURING, SUB (z));
          }
          z = SUB (z);
        }
        if (z != MODE (VOID)) {
          make_coercion (p, VOIDING, MODE (VOID));
        }
        return;
      }
    }
  }
/* All other is voided straight away. */
  make_coercion (p, VOIDING, MODE (VOID));
}

/*!
\brief make strong coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_strong (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (q == MODE (VOID) && p != MODE (VOID)) {
    make_void (n, p);
  } else {
    make_depreffing_coercion (n, p, q);
  }
}

/*!
\brief mode check on bounds
\param p position in tree
**/

static void mode_check_bounds (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, MODE (INT), 0);
    mode_check_unit (p, &x, &y);
    if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&y), MODE (INT), MEEK, SAFE_DEFLEXING, UNIT);
    }
    mode_check_bounds (NEXT (p));
  } else {
    mode_check_bounds (SUB (p));
    mode_check_bounds (NEXT (p));
  }
}

/*!
\brief mode check declarer
\param p position in tree
**/

static void mode_check_declarer (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, BOUNDS)) {
    mode_check_bounds (SUB (p));
    mode_check_declarer (NEXT (p));
  } else {
    mode_check_declarer (SUB (p));
    mode_check_declarer (NEXT (p));
  }
}

/*!
\brief mode check identity declaration
\param p position in tree
**/

static void mode_check_identity_declaration (NODE_T * p)
{
  if (p != NULL) {
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
        if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
          cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
        } else if (MOID (&x) != MOID (&y)) {
/* Check for instance, REF INT i = LOC REF INT. */
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

/*!
\brief mode check variable declaration
\param p position in tree
**/

static void mode_check_variable_declaration (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        mode_check_declarer (SUB (p));
        mode_check_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
          SOID_T x, y;
          make_soid (&x, STRONG, SUB_MOID (p), 0);
          mode_check_unit (NEXT_NEXT (p), &x, &y);
          if (!whether_coercible_in_context (&y, &x, FORCE_DEFLEXING)) {
            cannot_coerce (p, MOID (&y), MOID (&x), STRONG, FORCE_DEFLEXING, UNIT);
          } else if (SUB_MOID (&x) != MOID (&y)) {
/* Check for instance, REF INT i = LOC REF INT. */
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

/*!
\brief mode check routine text
\param p position in tree
\param y resulting soid
**/

static void mode_check_routine_text (NODE_T * p, SOID_T * y)
{
  SOID_T w;
  if (WHETHER (p, PARAMETER_PACK)) {
    mode_check_declarer (SUB (p));
    FORWARD (p);
  }
  mode_check_declarer (SUB (p));
  make_soid (&w, STRONG, MOID (p), 0);
  mode_check_unit (NEXT_NEXT (p), &w, y);
  if (!whether_coercible_in_context (y, &w, FORCE_DEFLEXING)) {
    cannot_coerce (NEXT_NEXT (p), MOID (y), MOID (&w), STRONG, FORCE_DEFLEXING, UNIT);
  }
}

/*!
\brief mode check proc declaration
\param p position in tree
**/

static void mode_check_proc_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, NULL, 0);
    mode_check_routine_text (SUB (p), &y);
  } else {
    mode_check_proc_declaration (SUB (p));
    mode_check_proc_declaration (NEXT (p));
  }
}

/*!
\brief mode check brief op declaration
\param p position in tree
**/

static void mode_check_brief_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
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

/*!
\brief mode check op declaration
\param p position in tree
**/

static void mode_check_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    SOID_T y, x;
    make_soid (&x, STRONG, MOID (p), 0);
    mode_check_unit (NEXT_NEXT (p), &x, &y);
    if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
    }
  } else {
    mode_check_op_declaration (SUB (p));
    mode_check_op_declaration (NEXT (p));
  }
}

/*!
\brief mode check declaration list
\param p position in tree
**/

static void mode_check_declaration_list (NODE_T * p)
{
  if (p != NULL) {
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

/*!
\brief mode check serial clause
\param r resulting soids
\param p position in tree
\param x expected soid
\param k whether statement yields a value other than VOID
**/

static void mode_check_serial (SOID_LIST_T ** r, NODE_T * p, SOID_T * x, BOOL_T k)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, INITIALISER_SERIES)) {
    mode_check_serial (r, SUB (p), x, A68_FALSE);
    mode_check_serial (r, NEXT (p), x, k);
  } else if (WHETHER (p, DECLARATION_LIST)) {
    mode_check_declaration_list (SUB (p));
  } else if (whether_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, NULL_ATTRIBUTE)) {
    mode_check_serial (r, NEXT (p), x, k);
  } else if (whether_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, NULL_ATTRIBUTE)) {
    if (NEXT (p) != NULL) {
      if (WHETHER (NEXT (p), EXIT_SYMBOL) || WHETHER (NEXT (p), END_SYMBOL) || WHETHER (NEXT (p), CLOSE_SYMBOL)) {
        mode_check_serial (r, SUB (p), x, A68_TRUE);
      } else {
        mode_check_serial (r, SUB (p), x, A68_FALSE);
      }
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      mode_check_serial (r, SUB (p), x, A68_TRUE);
    }
  } else if (WHETHER (p, LABELED_UNIT)) {
    mode_check_serial (r, SUB (p), x, k);
  } else if (WHETHER (p, UNIT)) {
    SOID_T y;
    if (k) {
      mode_check_unit (p, x, &y);
    } else {
      SOID_T w;
      make_soid (&w, STRONG, MODE (VOID), 0);
      mode_check_unit (p, &w, &y);
    }
    if (NEXT (p) != NULL) {
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      if (k) {
        add_to_soid_list (r, p, &y);
      }
    }
  }
}

/*!
\brief mode check serial clause units
\param p position in tree
\param x expected soid
\param y resulting soid
\param att attribute (SERIAL or ENQUIRY)
**/

static void mode_check_serial_units (NODE_T * p, SOID_T * x, SOID_T * y, int att)
{
  SOID_LIST_T *top_sl = NULL;
  (void) att;
  mode_check_serial (&top_sl, SUB (p), x, A68_TRUE);
  if (whether_balanced (p, top_sl, SORT (x))) {
    MOID_T *result = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), result, SERIAL_CLAUSE);
  } else {
    make_soid (y, SORT (x), MOID (x) != NULL ? MOID (x) : MODE (ERROR), 0);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check unit list
\param r resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_unit_list (SOID_LIST_T ** r, NODE_T * p, SOID_T * x)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    mode_check_unit_list (r, SUB (p), x);
    mode_check_unit_list (r, NEXT (p), x);
  } else if (WHETHER (p, COMMA_SYMBOL)) {
    mode_check_unit_list (r, NEXT (p), x);
  } else if (WHETHER (p, UNIT)) {
    SOID_T y;
    mode_check_unit (p, x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_unit_list (r, NEXT (p), x);
  }
}

/*!
\brief mode check struct display
\param r resulting soids
\param p position in tree
\param fields pack
**/

static void mode_check_struct_display (SOID_LIST_T ** r, NODE_T * p, PACK_T ** fields)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    mode_check_struct_display (r, SUB (p), fields);
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (WHETHER (p, COMMA_SYMBOL)) {
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (WHETHER (p, UNIT)) {
    SOID_T x, y;
    if (*fields != NULL) {
      make_soid (&x, STRONG, MOID (*fields), 0);
      FORWARD (*fields);
    } else {
      make_soid (&x, STRONG, NULL, 0);
    }
    mode_check_unit (p, &x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_struct_display (r, NEXT (p), fields);
  }
}

/*!
\brief mode check get specified moids
\param p position in tree
\param u united mode to add to
**/

static void mode_check_get_specified_moids (NODE_T * p, MOID_T * u)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      mode_check_get_specified_moids (SUB (p), u);
    } else if (WHETHER (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      add_mode_to_pack (&(PACK (u)), m, NULL, NODE (m));
    }
  }
}

/*!
\brief mode check specified unit list
\param r resulting soids
\param p position in tree
\param x expected soid
\param u resulting united mode
**/

static void mode_check_specified_unit_list (SOID_LIST_T ** r, NODE_T * p, SOID_T * x, MOID_T * u)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      mode_check_specified_unit_list (r, SUB (p), x, u);
    } else if (WHETHER (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      if (u != NULL && !whether_unitable (m, u, SAFE_DEFLEXING)) {
        diagnostic_node (A68_ERROR, p, ERROR_NO_COMPONENT, m, u);
      }
    } else if (WHETHER (p, UNIT)) {
      SOID_T y;
      mode_check_unit (p, x, &y);
      add_to_soid_list (r, p, &y);
    }
  }
}

/*!
\brief mode check united case parts
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_united_case_parts (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  MOID_T *u = NULL, *v = NULL, *w = NULL;
/* Check the CASE part and deduce the united mode. */
  make_soid (&enq_expct, STRONG, NULL, 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
/* Deduce the united mode from the enquiry clause. */
  u = depref_completely (MOID (&enq_yield));
  u = make_united_mode (u);
  u = depref_completely (u);
/* Also deduce the united mode from the specifiers. */
  v = new_moid ();
  ATTRIBUTE (v) = SERIES_MODE;
  mode_check_get_specified_moids (NEXT_SUB (NEXT (p)), v);
  v = make_united_mode (v);
/* Determine a resulting union. */
  if (u == MODE (HIP)) {
    w = v;
  } else {
    if (WHETHER (u, UNION_SYMBOL)) {
      BOOL_T uv, vu, some;
      investigate_firm_relations (PACK (u), PACK (v), &uv, &some);
      investigate_firm_relations (PACK (v), PACK (u), &vu, &some);
      if (uv && vu) {
/* Every component has a specifier. */
        w = u;
      } else if (!uv && !vu) {
/* Hmmmm ... let the coercer sort it out. */
        w = u;
      } else {
/*  This is all the balancing we allow here for the moment. Firmly related
subsets are not valid so we absorb them. If this doesn't solve it then we
get a coercion-error later. */
        w = absorb_related_subsets (u);
      }
    } else {
      diagnostic_node (A68_ERROR, NEXT_SUB (p), ERROR_NO_UNION, u);
      return;
    }
  }
  MOID (SUB (p)) = w;
  FORWARD (p);
/* Check the IN part. */
  mode_check_specified_unit_list (ry, NEXT_SUB (p), x, w);
/* OUSE, OUT, ESAC. */
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, UNITED_OUSE_PART, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE)) {
      mode_check_united_case_parts (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check united case
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_united_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_united_case_parts (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), UNITED_CASE_CLAUSE);

    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, UNITED_CASE_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check unit list 2
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_unit_list_2 (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  if (MOID (x) != NULL) {
    if (WHETHER (MOID (x), FLEX_SYMBOL)) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (SUB_MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (WHETHER (MOID (x), ROW_SYMBOL)) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (WHETHER (MOID (x), STRUCT_SYMBOL)) {
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

/*!
\brief mode check closed
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_closed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, SERIAL_CLAUSE)) {
    mode_check_serial_units (p, x, y, SERIAL_CLAUSE);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
    mode_check_closed (NEXT (p), x, y);
  }
  MOID (p) = MOID (y);
}

/*!
\brief mode check collateral
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_collateral (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (whether (p, BEGIN_SYMBOL, END_SYMBOL, 0)
             || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0)) {
    if (SORT (x) == STRONG) {
      make_soid (y, STRONG, MODE (VACUUM), 0);
    } else {
      make_soid (y, STRONG, MODE (UNDEFINED), 0);
    }
  } else {
    if (WHETHER (p, UNIT_LIST)) {
      mode_check_unit_list_2 (p, x, y);
    } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
      mode_check_collateral (NEXT (p), x, y);
    }
    MOID (p) = MOID (y);
  }
}

/*!
\brief mode check conditional 2
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_conditional_2 (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, ELSE_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, ELIF_PART, BRIEF_ELIF_IF_PART, NULL_ATTRIBUTE)) {
      mode_check_conditional_2 (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check conditional
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_conditional (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_conditional_2 (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), CONDITIONAL_CLAUSE);
    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, CONDITIONAL_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check int case 2
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_int_case_2 (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, STRONG, MODE (INT), 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_unit_list (ry, NEXT_SUB (p), x);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, INTEGER_OUT_PART, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE)) {
      mode_check_int_case_2 (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check int case
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_int_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_int_case_2 (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), INTEGER_CASE_CLAUSE);
    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, INTEGER_CASE_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check loop 2
\param p position in tree
\param y resulting soid
**/

static void mode_check_loop_2 (NODE_T * p, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, FOR_PART)) {
    mode_check_loop_2 (NEXT (p), y);
  } else if (whether_one_of (p, FROM_PART, BY_PART, TO_PART, NULL_ATTRIBUTE)) {
    SOID_T ix, iy;
    make_soid (&ix, STRONG, MODE (INT), 0);
    mode_check_unit (NEXT_SUB (p), &ix, &iy);
    if (!whether_coercible_in_context (&iy, &ix, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_SUB (p), MOID (&iy), MODE (INT), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (WHETHER (p, WHILE_PART)) {
    SOID_T enq_expct, enq_yield;
    make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
    mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
    if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (whether_one_of (p, DO_PART, ALT_DO_PART, NULL_ATTRIBUTE)) {
    SOID_LIST_T *z = NULL;
    SOID_T ix;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&ix, STRONG, MODE (VOID), 0);
    if (WHETHER (do_p, SERIAL_CLAUSE)) {
      mode_check_serial (&z, do_p, &ix, A68_TRUE);
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
      SOID_T enq_expct, enq_yield;
      make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
      mode_check_serial_units (NEXT_SUB (un_p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
      if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
        cannot_coerce (un_p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
      }
    }
    free_soid_list (z);
  }
}

/*!
\brief mode check loop
\param p position in tree
\param y resulting soid
**/

static void mode_check_loop (NODE_T * p, SOID_T * y)
{
  SOID_T *z = NULL;
  mode_check_loop_2 (p, /* y. */ z);
  make_soid (y, STRONG, MODE (VOID), 0);
}

/*!
\brief mode check enclosed
\param p position in tree
\param x expected soid
\param y resulting soid
**/

void mode_check_enclosed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    mode_check_closed (SUB (p), x, y);
  } else if (WHETHER (p, PARALLEL_CLAUSE)) {
    mode_check_collateral (SUB (NEXT_SUB (p)), x, y);
    make_soid (y, STRONG, MODE (VOID), 0);
    MOID (NEXT_SUB (p)) = MODE (VOID);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    mode_check_collateral (SUB (p), x, y);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    mode_check_conditional (SUB (p), x, y);
  } else if (WHETHER (p, INTEGER_CASE_CLAUSE)) {
    mode_check_int_case (SUB (p), x, y);
  } else if (WHETHER (p, UNITED_CASE_CLAUSE)) {
    mode_check_united_case (SUB (p), x, y);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    mode_check_loop (SUB (p), y);
  }
  MOID (p) = MOID (y);
}

/*!
\brief search table for operator
\param t tag chain to search
\param n name of operator
\param x lhs mode
\param y rhs mode
\param deflex deflexing regime
\return tag entry or NULL
**/

static TAG_T *search_table_for_operator (TAG_T * t, char *n, MOID_T * x, MOID_T * y, int deflex)
{
  if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
  for (; t != NULL; FORWARD (t)) {
    if (SYMBOL (NODE (t)) == n) {
      PACK_T *p = PACK (MOID (t));
      if (whether_coercible (x, MOID (p), FIRM, deflex)) {
        FORWARD (p);
        if (p == NULL && y == NULL) {
/* Matched in case of a monad. */
          return (t);
        } else if (p != NULL && y != NULL && whether_coercible (y, MOID (p), FIRM, deflex)) {
/* Matched in case of a nomad. */
          return (t);
        }
      }
    }
  }
  return (NULL);
}

/*!
\brief search chain of symbol tables and return matching operator "x n y" or "n x"
\param s symbol table to start search
\param n name of token
\param x lhs mode
\param y rhs mode
\param deflex deflexing regime
\return tag entry or NULL
**/

static TAG_T *search_table_chain_for_operator (SYMBOL_TABLE_T * s, char *n, MOID_T * x, MOID_T * y, int deflex)
{
  if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
  while (s != NULL) {
    TAG_T *z = search_table_for_operator (s->operators, n, x, y, deflex);
    if (z != NULL) {
      return (z);
    }
    s = PREVIOUS (s);
  }
  return (NULL);
}

/*!
\brief return a matching operator "x n y"
\param s symbol table to start search
\param n name of token
\param x lhs mode
\param y rhs mode
\return tag entry or NULL
**/

static TAG_T *find_operator (SYMBOL_TABLE_T * s, char *n, MOID_T * x, MOID_T * y)
{
/* Coercions to operand modes are FIRM. */
  TAG_T *z;
  MOID_T *u, *v;
/* (A) Catch exceptions first. */
  if (x == NULL && y == NULL) {
    return (NULL);
  } else if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
/* (B) MONADs. */
  if (x != NULL && y == NULL) {
    z = search_table_chain_for_operator (s, n, x, NULL, SAFE_DEFLEXING);
    return (z);
  }
/* (C) NOMADs. */
  z = search_table_chain_for_operator (s, n, x, y, SAFE_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
/* (D) Vector and matrix "strong coercions" in standard environ. */
  u = depref_completely (x);
  v = depref_completely (y);
  if ((u == MODE (ROW_REAL) || u == MODE (ROWROW_REAL))
      || (v == MODE (ROW_REAL) || v == MODE (ROWROW_REAL))
      || (u == MODE (ROW_COMPLEX) || u == MODE (ROWROW_COMPLEX))
      || (v == MODE (ROW_COMPLEX) || v == MODE (ROWROW_COMPLEX))) {
    if (u == MODE (INT)) {
      z = search_table_for_operator (stand_env->operators, n, MODE (REAL), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
      z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (v == MODE (INT)) {
      z = search_table_for_operator (stand_env->operators, n, x, MODE (REAL), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
      z = search_table_for_operator (stand_env->operators, n, x, MODE (COMPLEX), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (u == MODE (REAL)) {
      z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (v == MODE (REAL)) {
      z = search_table_for_operator (stand_env->operators, n, x, MODE (COMPLEX), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    }
  }
/* (E) Look in standenv for an appropriate cross-term. */
  u = make_series_from_moids (x, y);
  u = make_united_mode (u);
  v = get_balanced_mode (u, STRONG, NO_DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (stand_env->operators, n, v, v, ALIAS_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
  if (whether_coercible_series (u, MODE (REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (REAL), MODE (REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONG_REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONG_REAL), MODE (LONG_REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONGLONG_REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), MODE (COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONGLONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
/* (F) Now allow for depreffing for REF REAL +:= INT and alike. */
  v = get_balanced_mode (u, STRONG, DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (stand_env->operators, n, v, v, ALIAS_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
  return (NULL);
}

/*!
\brief mode check monadic operator
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_monadic_operator (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p != NULL) {
    TAG_T *t;
    MOID_T *u;
    u = determine_unique_mode (y, SAFE_DEFLEXING);
    if (whether_mode_isnt_well (u)) {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (u == MODE (HIP)) {
      diagnostic_node (A68_ERROR, NEXT (p), ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else {
      if (a68g_strchr (NOMADS, *(SYMBOL (p))) != NULL) {
        t = NULL;
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      } else {
        t = find_operator (SYMBOL_TABLE (p), SYMBOL (p), u, NULL);
        if (t == NULL) {
          diagnostic_node (A68_ERROR, p, ERROR_NO_MONADIC, u);
          make_soid (y, SORT (x), MODE (ERROR), 0);
        }
      }
      if (t != NULL) {
        MOID (p) = MOID (t);
      }
      TAX (p) = t;
      if (t != NULL && t != error_tag) {
        MOID (p) = MOID (t);
        make_soid (y, SORT (x), SUB_MOID (t), 0);
      } else {
        MOID (p) = MODE (ERROR);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
    }
  }
}

/*!
\brief mode check monadic formula
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_monadic_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e;
  make_soid (&e, FIRM, NULL, 0);
  mode_check_formula (NEXT (p), &e, y);
  mode_check_monadic_operator (p, &e, y);
  make_soid (y, SORT (x), MOID (y), 0);
}

/*!
\brief mode check formula
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T ls, rs;
  TAG_T *op;
  MOID_T *u, *v;
  if (WHETHER (p, MONADIC_FORMULA)) {
    mode_check_monadic_formula (SUB (p), x, &ls);
  } else if (WHETHER (p, FORMULA)) {
    mode_check_formula (SUB (p), x, &ls);
  } else if (WHETHER (p, SECONDARY)) {
    SOID_T e;
    make_soid (&e, FIRM, NULL, 0);
    mode_check_unit (SUB (p), &e, &ls);
  }
  u = determine_unique_mode (&ls, SAFE_DEFLEXING);
  MOID (p) = u;
  if (NEXT (p) == NULL) {
    make_soid (y, SORT (x), u, 0);
  } else {
    NODE_T *q = NEXT_NEXT (p);
    if (WHETHER (q, MONADIC_FORMULA)) {
      mode_check_monadic_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (WHETHER (q, FORMULA)) {
      mode_check_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (WHETHER (q, SECONDARY)) {
      SOID_T e;
      make_soid (&e, FIRM, NULL, 0);
      mode_check_unit (SUB (q), &e, &rs);
    }
    v = determine_unique_mode (&rs, SAFE_DEFLEXING);
    MOID (q) = v;
    if (whether_mode_isnt_well (u) || whether_mode_isnt_well (v)) {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (u == MODE (HIP)) {
      diagnostic_node (A68_ERROR, p, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (v == MODE (HIP)) {
      diagnostic_node (A68_ERROR, q, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else {
      op = find_operator (SYMBOL_TABLE (NEXT (p)), SYMBOL (NEXT (p)), u, v);
      if (op == NULL) {
        diagnostic_node (A68_ERROR, NEXT (p), ERROR_NO_DYADIC, u, v);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
      if (op != NULL) {
        MOID (NEXT (p)) = MOID (op);
      }
      TAX (NEXT (p)) = op;
      if (op != NULL && op != error_tag) {
        make_soid (y, SORT (x), SUB_MOID (op), 0);
      } else {
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
    }
  }
}

/*!
\brief mode check assignation
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_assignation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T name, tmp, value;
  MOID_T *name_moid, *source_moid, *dest_moid, *ori;
/* Get destination mode. */
  make_soid (&name, SOFT, NULL, 0);
  mode_check_unit (SUB (p), &name, &tmp);
  dest_moid = MOID (&tmp);
/* SOFT coercion. */
  ori = determine_unique_mode (&tmp, SAFE_DEFLEXING);
  name_moid = deproc_completely (ori);
  if (ATTRIBUTE (name_moid) != REF_SYMBOL) {
    if (WHETHER_MODE_IS_WELL (name_moid)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_NAME, ori, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (p) = name_moid;
/* Get source mode. */
  make_soid (&name, STRONG, SUB (name_moid), 0);
  mode_check_unit (NEXT_NEXT (p), &name, &value);
/*
where (STDOUT_FILENO, p);
printf("\ndst: %s", moid_to_string (name_moid, 180, NULL));
printf("\nsrc: %s", moid_to_string (MOID (&value), 180, NULL));fflush (stdout);
fflush(stdout);
*/
  if (!whether_coercible_in_context (&value, &name, FORCE_DEFLEXING)) {
    source_moid = MOID (&value);
    cannot_coerce (p, MOID (&value), MOID (&name), STRONG, FORCE_DEFLEXING, UNIT);
    make_soid (y, SORT (x), MODE (ERROR), 0);
  } else {
    make_soid (y, SORT (x), name_moid, 0);
  }
}

/*!
\brief mode check identity relation
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_identity_relation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  MOID_T *lhs, *rhs, *oril, *orir;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, SOFT, NULL, 0);
  mode_check_unit (SUB (ln), &e, &l);
  mode_check_unit (SUB (rn), &e, &r);
/* SOFT coercion. */
  oril = determine_unique_mode (&l, SAFE_DEFLEXING);
  orir = determine_unique_mode (&r, SAFE_DEFLEXING);
  lhs = deproc_completely (oril);
  rhs = deproc_completely (orir);
  if (WHETHER_MODE_IS_WELL (lhs) && lhs != MODE (HIP) && ATTRIBUTE (lhs) != REF_SYMBOL) {
    diagnostic_node (A68_ERROR, ln, ERROR_NO_NAME, oril, ATTRIBUTE (SUB (ln)));
    lhs = MODE (ERROR);
  }
  if (WHETHER_MODE_IS_WELL (rhs) && rhs != MODE (HIP) && ATTRIBUTE (rhs) != REF_SYMBOL) {
    diagnostic_node (A68_ERROR, rn, ERROR_NO_NAME, orir, ATTRIBUTE (SUB (rn)));
    rhs = MODE (ERROR);
  }
  if (lhs == MODE (HIP) && rhs == MODE (HIP)) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_UNIQUE_MODE);
  }
  if (whether_coercible (lhs, rhs, STRONG, SAFE_DEFLEXING)) {
    lhs = rhs;
  } else if (whether_coercible (rhs, lhs, STRONG, SAFE_DEFLEXING)) {
    rhs = lhs;
  } else {
    cannot_coerce (NEXT (p), rhs, lhs, SOFT, SKIP_DEFLEXING, TERTIARY);
    lhs = rhs = MODE (ERROR);
  }
  MOID (ln) = lhs;
  MOID (rn) = rhs;
  make_soid (y, SORT (x), MODE (BOOL), 0);
}

/*!
\brief mode check bool functions ANDF and ORF
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_bool_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, STRONG, MODE (BOOL), 0);
  mode_check_unit (SUB (ln), &e, &l);
  if (!whether_coercible_in_context (&l, &e, SAFE_DEFLEXING)) {
    cannot_coerce (ln, MOID (&l), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  mode_check_unit (SUB (rn), &e, &r);
  if (!whether_coercible_in_context (&r, &e, SAFE_DEFLEXING)) {
    cannot_coerce (rn, MOID (&r), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  MOID (ln) = MODE (BOOL);
  MOID (rn) = MODE (BOOL);
  make_soid (y, SORT (x), MODE (BOOL), 0);
}

/*!
\brief mode check cast
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_cast (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w;
  mode_check_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  w.cast = A68_TRUE;
  mode_check_enclosed (SUB_NEXT (p), &w, y);
  if (!whether_coercible_in_context (y, &w, SAFE_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (y), MOID (&w), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
  }
  make_soid (y, SORT (x), MOID (p), 0);
}

/*!
\brief mode check assertion
\param p position in tree
**/

static void mode_check_assertion (NODE_T * p)
{
  SOID_T w, y;
  make_soid (&w, STRONG, MODE (BOOL), 0);
  mode_check_enclosed (SUB_NEXT (p), &w, &y);
  SORT (&y) = SORT (&w);        /* Patch. */
  if (!whether_coercible_in_context (&y, &w, NO_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (&y), MOID (&w), MEEK, NO_DEFLEXING, ENCLOSED_CLAUSE);
  }
}

/*!
\brief mode check argument list
\param r resulting soids
\param p position in tree
\param x proc argument pack
\param v partial locale pack
\param w partial proc pack
**/

static void mode_check_argument_list (SOID_LIST_T ** r, NODE_T * p, PACK_T ** x, PACK_T ** v, PACK_T ** w)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, GENERIC_ARGUMENT_LIST)) {
      ATTRIBUTE (p) = ARGUMENT_LIST;
    }
    if (WHETHER (p, ARGUMENT_LIST)) {
      mode_check_argument_list (r, SUB (p), x, v, w);
    } else if (WHETHER (p, UNIT)) {
      SOID_T y, z;
      if (*x != NULL) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MOID (*x), NULL, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NULL, 0);
      }
      mode_check_unit (p, &z, &y);
      add_to_soid_list (r, p, &y);
    } else if (WHETHER (p, TRIMMER)) {
      SOID_T z;
      if (SUB (p) != NULL) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, ARGUMENT);
        make_soid (&z, STRONG, MODE (ERROR), 0);
        add_mode_to_pack_end (v, MODE (VOID), NULL, p);
        add_mode_to_pack_end (w, MOID (*x), NULL, p);
        FORWARD (*x);
      } else if (*x != NULL) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MODE (VOID), NULL, p);
        add_mode_to_pack_end (w, MOID (*x), NULL, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NULL, 0);
      }
      add_to_soid_list (r, p, &z);
    } else if (WHETHER (p, SUB_SYMBOL) && !program.options.brackets) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, CALL);
    }
  }
}

/*!
\brief mode check argument list 2
\param p position in tree
\param x proc argument pack
\param y soid
\param v partial locale pack
\param w partial proc pack
**/

static void mode_check_argument_list_2 (NODE_T * p, PACK_T * x, SOID_T * y, PACK_T ** v, PACK_T ** w)
{
  SOID_LIST_T *top_sl = NULL;
  mode_check_argument_list (&top_sl, SUB (p), &x, v, w);
  make_soid (y, STRONG, pack_soids_in_moid (top_sl, STOWED_MODE), 0);
  free_soid_list (top_sl);
}

/*!
\brief mode check meek int
\param p position in tree
**/

static void mode_check_meek_int (NODE_T * p)
{
  SOID_T x, y;
  make_soid (&x, STRONG, MODE (INT), 0);
  mode_check_unit (p, &x, &y);
  if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&y), MOID (&x), MEEK, SAFE_DEFLEXING, 0);
  }
}

/*!
\brief mode check trimmer
\param p position in tree
**/

static void mode_check_trimmer (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, TRIMMER)) {
    mode_check_trimmer (SUB (p));
  } else if (WHETHER (p, UNIT)) {
    mode_check_meek_int (p);
    mode_check_trimmer (NEXT (p));
  } else {
    mode_check_trimmer (NEXT (p));
  }
}

/*!
\brief mode check indexer
\param p position in tree
\param subs subscript counter
\param trims trimmer counter
**/

static void mode_check_indexer (NODE_T * p, int *subs, int *trims)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, TRIMMER)) {
    (*trims)++;
    mode_check_trimmer (SUB (p));
  } else if (WHETHER (p, UNIT)) {
    (*subs)++;
    mode_check_meek_int (p);
  } else {
    mode_check_indexer (SUB (p), subs, trims);
    mode_check_indexer (NEXT (p), subs, trims);
  }
}

/*!
\brief mode check call
\param p position in tree
\param n mode
\param x expected soid
\param y resulting soid
**/

static void mode_check_call (NODE_T * p, MOID_T * n, SOID_T * x, SOID_T * y)
{
  SOID_T d;
  MOID (p) = n;
/* "partial_locale" is the mode of the locale. */
  GENIE (p)->partial_locale = new_moid ();
  ATTRIBUTE (GENIE (p)->partial_locale) = PROC_SYMBOL;
  PACK (GENIE (p)->partial_locale) = NULL;
  SUB (GENIE (p)->partial_locale) = SUB (n);
/* "partial_proc" is the mode of the resulting proc. */
  GENIE (p)->partial_proc = new_moid ();
  ATTRIBUTE (GENIE (p)->partial_proc) = PROC_SYMBOL;
  PACK (GENIE (p)->partial_proc) = NULL;
  SUB (GENIE (p)->partial_proc) = SUB (n);
/* Check arguments and construct modes. */
  mode_check_argument_list_2 (NEXT (p), PACK (n), &d, &PACK (GENIE (p)->partial_locale), &PACK (GENIE (p)->partial_proc));
  DIM (GENIE (p)->partial_proc) = count_pack_members (PACK (GENIE (p)->partial_proc));
  DIM (GENIE (p)->partial_locale) = count_pack_members (PACK (GENIE (p)->partial_locale));
  GENIE (p)->partial_proc = register_extra_mode (GENIE (p)->partial_proc);
  GENIE (p)->partial_locale = register_extra_mode (GENIE (p)->partial_locale);
  if (DIM (MOID (&d)) != DIM (n)) {
    diagnostic_node (A68_ERROR, p, ERROR_ARGUMENT_NUMBER, n);
    make_soid (y, SORT (x), SUB (n), 0);
/*  make_soid (y, SORT (x), MODE (ERROR), 0); */
  } else {
    if (!whether_coercible (MOID (&d), n, STRONG, ALIAS_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), n, STRONG, ALIAS_DEFLEXING, ARGUMENT);
    }
    if (DIM (GENIE (p)->partial_proc) == 0) {
      make_soid (y, SORT (x), SUB (n), 0);
    } else {
      if (program.options.portcheck) {
        diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, NEXT (p), WARNING_EXTENSION, NULL);
      }
      make_soid (y, SORT (x), GENIE (p)->partial_proc, 0);
    }
  }
}

/*!
\brief mode check slice
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL or a SLICE
**/

static void mode_check_slice (NODE_T * p, MOID_T * ori, SOID_T * x, SOID_T * y)
{
  BOOL_T whether_ref;
  int rowdim, subs, trims;
  MOID_T *m = depref_completely (ori), *n = ori;
/* WEAK coercion. */
  while ((WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) || (WHETHER (n, PROC_SYMBOL) && PACK (n) == NULL)) {
    n = depref_once (n);
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_ROW_OR_PROC, n, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
  }
  MOID (p) = n;
  subs = trims = 0;
  mode_check_indexer (SUB_NEXT (p), &subs, &trims);
  if ((whether_ref = whether_ref_row (n)) != 0) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if ((subs + trims) != rowdim) {
    diagnostic_node (A68_ERROR, p, ERROR_INDEXER_NUMBER, n);
    make_soid (y, SORT (x), MODE (ERROR), 0);
  } else {
    if (subs > 0 && trims == 0) {
      ANNOTATION (NEXT (p)) = SLICE;
      m = n;
    } else {
      ANNOTATION (NEXT (p)) = TRIMMER;
      m = n;
    }
    while (subs > 0) {
      if (whether_ref) {
        m = m->name;
      } else {
        if (WHETHER (m, FLEX_SYMBOL)) {
          m = SUB (m);
        }
        m = SLICE (m);
      }
      ABEND (m == NULL, "NULL mode in mode_check_slice", NULL);
      subs--;
    }
/* A trim cannot be but deflexed
    make_soid (y, SORT (x), ANNOTATION (NEXT (p)) == TRIMMER && m->deflexed_mode != NULL ? m->deflexed_mode : m,0);
*/
    make_soid (y, SORT (x), ANNOTATION (NEXT (p)) == TRIMMER && m->trim != NULL ? m->trim : m, 0);
  }
}

/*!
\brief mode check field selection
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL, SLICE or FIELD_SELECTION
**/

static void mode_check_field_identifiers (NODE_T * p, MOID_T ** m, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      if (MOID (p) != MODE (ERROR)) {
        ATTRIBUTE (p) = FIELD_IDENTIFIER;
      }
      NODE_PACK (p) = NODE_PACK (SUB (p));
      SEQUENCE (*seq) = p;
      (*seq) = p;
      SUB (p) = NULL;
    } else if (WHETHER (p, TERTIARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, SECONDARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, PRIMARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, IDENTIFIER)) {
      BOOL_T coerce;
      MOID_T *n, *str;
      PACK_T *t, *t_2;
      char *fs;
      n = (*m);
      coerce = A68_TRUE;
      while (coerce) {
        if (WHETHER (n, STRUCT_SYMBOL)) {
          coerce = A68_FALSE;
          t = PACK (n);
        } else if (WHETHER (n, REF_SYMBOL) && (WHETHER (SUB (n), ROW_SYMBOL) || WHETHER (SUB (n), FLEX_SYMBOL)) && n->multiple_mode != NULL) {
          coerce = A68_FALSE;
          t = PACK (n->multiple_mode);
        } else if ((WHETHER (n, ROW_SYMBOL) || WHETHER (n, FLEX_SYMBOL)) && n->multiple_mode != NULL) {
          coerce = A68_FALSE;
          t = PACK (n->multiple_mode);
        } else if (WHETHER (n, REF_SYMBOL) && whether_name_struct (n)) {
          coerce = A68_FALSE;
          t = PACK (n->name);
        } else if (whether_deprefable (n)) {
          coerce = A68_TRUE;
          n = SUB (n);
          t = NULL;
        } else {
          coerce = A68_FALSE;
          t = NULL;
        }
      }
      if (t == NULL) {
        if (WHETHER_MODE_IS_WELL (*m)) {
          diagnostic_node (A68_ERROR, p, ERROR_NO_STRUCT, (*m), CONSTRUCT);
        }
        (*m) = MODE (ERROR);
        return;
      }
      fs = SYMBOL (p);
      str = n;
      while (WHETHER (str, REF_SYMBOL)) {
        str = SUB (str);
      }
      if (WHETHER (str, FLEX_SYMBOL)) {
        str = SUB (str);
      }
      if (WHETHER (str, ROW_SYMBOL)) {
        str = SUB (str);
      }
      t_2 = PACK (str);
      while (t != NULL && t_2 != NULL) {
        if (t->text == fs) {
          (*m) = MOID (t);
          MOID (p) = (*m);
          NODE_PACK (p) = t_2;
          return;
        }
        FORWARD (t);
        FORWARD (t_2);
      }
      diagnostic_node (A68_ERROR, p, ERROR_NO_FIELD, str, fs);
      (*m) = MODE (ERROR);
    } else if (WHETHER (p, GENERIC_ARGUMENT) || WHETHER (p, GENERIC_ARGUMENT_LIST)) {
      mode_check_field_identifiers (SUB (p), m, seq);
    } else if (whether_one_of (p, COMMA_SYMBOL, OPEN_SYMBOL, CLOSE_SYMBOL, SUB_SYMBOL, BUS_SYMBOL, NULL)) {
      /* ok */;
    } else {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, FIELD_IDENTIFIER);
      (*m) = MODE (ERROR);
    }
  }
}

static void mode_check_field_selection (NODE_T * p, MOID_T * m, SOID_T * x, SOID_T * y)
{
  MOID_T *ori = m;
  NODE_T *seq = p;
  mode_check_field_identifiers (NEXT (p), &ori, &seq);
  MOID (p) = MOID (SUB (p));
  make_soid (y, SORT (x), ori, 0);
}

/*!
\brief mode check specification
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL, SLICE or FIELD_SELECTION
**/

static int mode_check_specification (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  MOID_T *m, *ori;
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (SUB (p), &w, &d);
  ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  m = depref_completely (ori);
  if (WHETHER (m, PROC_SYMBOL)) {
/* Assume CALL. */
    mode_check_call (p, m, x, y);
    return (CALL);
  } else if (WHETHER (m, ROW_SYMBOL) || WHETHER (m, FLEX_SYMBOL)) {
/* Assume SLICE. */
    mode_check_slice (p, ori, x, y);
    return (SLICE);
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
    mode_check_field_selection (p, ori, x, y);
    return (FIELD_SELECTION);
  } else {
    if (m != MODE (ERROR)) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_MODE_SPECIFICATION, m, NULL);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return (PRIMARY);
  }
}

/*!
\brief mode check selection
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_selection (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  BOOL_T coerce;
  MOID_T *n, *str, *ori;
  PACK_T *t, *t_2;
  char *fs;
  NODE_T *secondary = SUB_NEXT (p);
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (secondary, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  coerce = A68_TRUE;
  while (coerce) {
    if (WHETHER (n, STRUCT_SYMBOL)) {
      coerce = A68_FALSE;
      t = PACK (n);
    } else if (WHETHER (n, REF_SYMBOL) && (WHETHER (SUB (n), ROW_SYMBOL) || WHETHER (SUB (n), FLEX_SYMBOL)) && n->multiple_mode != NULL) {
      coerce = A68_FALSE;
      t = PACK (n->multiple_mode);
    } else if ((WHETHER (n, ROW_SYMBOL) || WHETHER (n, FLEX_SYMBOL)) && n->multiple_mode != NULL) {
      coerce = A68_FALSE;
      t = PACK (n->multiple_mode);
    } else if (WHETHER (n, REF_SYMBOL) && whether_name_struct (n)) {
      coerce = A68_FALSE;
      t = PACK (n->name);
    } else if (whether_deprefable (n)) {
      coerce = A68_TRUE;
      n = SUB (n);
      t = NULL;
    } else {
      coerce = A68_FALSE;
      t = NULL;
    }
  }
  if (t == NULL) {
    if (WHETHER_MODE_IS_WELL (MOID (&d))) {
      diagnostic_node (A68_ERROR, secondary, ERROR_NO_STRUCT, ori, ATTRIBUTE (secondary));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (NEXT (p)) = n;
  fs = SYMBOL (SUB (p));
  str = n;
  while (WHETHER (str, REF_SYMBOL)) {
    str = SUB (str);
  }
  if (WHETHER (str, FLEX_SYMBOL)) {
    str = SUB (str);
  }
  if (WHETHER (str, ROW_SYMBOL)) {
    str = SUB (str);
  }
  t_2 = PACK (str);
  while (t != NULL && t_2 != NULL) {
    if (t->text == fs) {
      make_soid (y, SORT (x), MOID (t), 0);
      MOID (p) = MOID (t);
      NODE_PACK (SUB (p)) = t_2;
      return;
    }
    FORWARD (t);
    FORWARD (t_2);
  }
  make_soid (&d, NO_SORT, n, 0);
  diagnostic_node (A68_ERROR, p, ERROR_NO_FIELD, str, fs);
  make_soid (y, SORT (x), MODE (ERROR), 0);
}

/*!
\brief mode check diagonal
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_diagonal (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, STRONG, MODE (INT), 0);
    mode_check_unit (p, &w, &d);
    if (!whether_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 2) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  if (whether_ref) {
    n = NAME (n);
  } else {
    n = SLICE (n);
  }
  ABEND (n == NULL, "NULL mode in mode_check_diagonal", NULL);
  make_soid (y, SORT (x), n, 0);
}

/*!
\brief mode check transpose
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_transpose (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert = NEXT (p);
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 2) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  ABEND (n == NULL, "NULL mode in mode_check_transpose", NULL);
  make_soid (y, SORT (x), n, 0);
}

/*!
\brief mode check row or column function
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_row_column_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, STRONG, MODE (INT), 0);
    mode_check_unit (p, &w, &d);
    if (!whether_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 1) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  ABEND (n == NULL, "NULL mode in mode_check_diagonal", NULL);
  make_soid (y, SORT (x), ROWED (n), 0);
}

/*!
\brief mode check format text
\param p position in tree
**/

static void mode_check_format_text (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    mode_check_format_text (SUB (p));
    if (WHETHER (p, FORMAT_PATTERN)) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (FORMAT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (ROW_INT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (INT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    }
  }
}

/*!
\brief mode check unit
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_unit (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    mode_check_unit (SUB (p), x, y);
/* Ex primary. */
  } else if (WHETHER (p, SPECIFICATION)) {
    ATTRIBUTE (p) = mode_check_specification (SUB (p), x, y);
    if (WHETHER (p, FIELD_SELECTION) && program.options.portcheck) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_EXTENSION, NULL);
    } else if (WHETHER (p, FIELD_SELECTION)) {
      diagnostic_node (A68_WARNING, p, WARNING_EXTENSION, NULL);
    }
    warn_for_voiding (p, x, y, ATTRIBUTE (p));
  } else if (WHETHER (p, CAST)) {
    mode_check_cast (SUB (p), x, y);
    warn_for_voiding (p, x, y, CAST);
  } else if (WHETHER (p, DENOTATION)) {
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, DENOTATION);
  } else if (WHETHER (p, IDENTIFIER)) {
    if ((TAX (p) == NULL) && (MOID (p) == NULL)) {
      int att = first_tag_global (SYMBOL_TABLE (p), SYMBOL (p));
      if (att == NULL_ATTRIBUTE) {
        (void) add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
        diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
        MOID (p) = MODE (ERROR);
      } else {
        TAG_T *z = find_tag_global (SYMBOL_TABLE (p), att, SYMBOL (p));
        if (att == IDENTIFIER && z != NULL) {
          MOID (p) = MOID (z);
        } else {
          (void) add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
          diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          MOID (p) = MODE (ERROR);
        }
      }
    }
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, IDENTIFIER);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (WHETHER (p, FORMAT_TEXT)) {
    mode_check_format_text (p);
    make_soid (y, SORT (x), MODE (FORMAT), 0);
    warn_for_voiding (p, x, y, FORMAT_TEXT);
/* Ex secondary. */
  } else if (WHETHER (p, GENERATOR)) {
    mode_check_declarer (SUB (p));
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, GENERATOR);
  } else if (WHETHER (p, SELECTION)) {
    mode_check_selection (SUB (p), x, y);
    warn_for_voiding (p, x, y, SELECTION);
/* Ex tertiary. */
  } else if (WHETHER (p, NIHIL)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  } else if (WHETHER (p, FORMULA)) {
    mode_check_formula (p, x, y);
    if (WHETHER_NOT (MOID (y), REF_SYMBOL)) {
      warn_for_voiding (p, x, y, FORMULA);
    }
  } else if (WHETHER (p, DIAGONAL_FUNCTION)) {
    mode_check_diagonal (SUB (p), x, y);
    warn_for_voiding (p, x, y, DIAGONAL_FUNCTION);
  } else if (WHETHER (p, TRANSPOSE_FUNCTION)) {
    mode_check_transpose (SUB (p), x, y);
    warn_for_voiding (p, x, y, TRANSPOSE_FUNCTION);
  } else if (WHETHER (p, ROW_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, ROW_FUNCTION);
  } else if (WHETHER (p, COLUMN_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, COLUMN_FUNCTION);
/* Ex unit. */
  } else if (whether_one_of (p, JUMP, SKIP, NULL_ATTRIBUTE)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  } else if (WHETHER (p, ASSIGNATION)) {
    mode_check_assignation (SUB (p), x, y);
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    mode_check_identity_relation (SUB (p), x, y);
    warn_for_voiding (p, x, y, IDENTITY_RELATION);
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    mode_check_routine_text (SUB (p), y);
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, ROUTINE_TEXT);
  } else if (WHETHER (p, ASSERTION)) {
    mode_check_assertion (SUB (p));
    make_soid (y, STRONG, MODE (VOID), 0);
  } else if (WHETHER (p, AND_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, AND_FUNCTION);
  } else if (WHETHER (p, OR_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, OR_FUNCTION);
  }
  MOID (p) = MOID (y);
}

/*!
\brief coerce bounds
\param p position in tree
**/

static void coerce_bounds (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      SOID_T q;
      make_soid (&q, MEEK, MODE (INT), 0);
      coerce_unit (p, &q);
    } else {
      coerce_bounds (SUB (p));
    }
  }
}

/*!
\brief coerce declarer
\param p position in tree
**/

static void coerce_declarer (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOUNDS)) {
      coerce_bounds (SUB (p));
    } else {
      coerce_declarer (SUB (p));
    }
  }
}

/*!
\brief coerce identity declaration
\param p position in tree
**/

static void coerce_identity_declaration (NODE_T * p)
{
  if (p != NULL) {
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

/*!
\brief coerce variable declaration
\param p position in tree
**/

static void coerce_variable_declaration (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        coerce_declarer (SUB (p));
        coerce_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
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

/*!
\brief coerce routine text
\param p position in tree
**/

static void coerce_routine_text (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, PARAMETER_PACK)) {
    FORWARD (p);
  }
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

/*!
\brief coerce proc declaration
\param p position in tree
**/

static void coerce_proc_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
  } else {
    coerce_proc_declaration (SUB (p));
    coerce_proc_declaration (NEXT (p));
  }
}

/*!
\brief coerce_op_declaration
\param p position in tree
**/

static void coerce_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    SOID_T q;
    make_soid (&q, STRONG, MOID (p), 0);
    coerce_unit (NEXT_NEXT (p), &q);
  } else {
    coerce_op_declaration (SUB (p));
    coerce_op_declaration (NEXT (p));
  }
}

/*!
\brief coerce brief op declaration
\param p position in tree
**/

static void coerce_brief_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    coerce_routine_text (SUB (NEXT_NEXT (p)));
  } else {
    coerce_brief_op_declaration (SUB (p));
    coerce_brief_op_declaration (NEXT (p));
  }
}

/*!
\brief coerce declaration list
\param p position in tree
**/

static void coerce_declaration_list (NODE_T * p)
{
  if (p != NULL) {
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

/*!
\brief coerce serial
\param p position in tree
\param q soid
\param k whether k yields value other than VOID
**/

static void coerce_serial (NODE_T * p, SOID_T * q, BOOL_T k)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, INITIALISER_SERIES)) {
    coerce_serial (SUB (p), q, A68_FALSE);
    coerce_serial (NEXT (p), q, k);
  } else if (WHETHER (p, DECLARATION_LIST)) {
    coerce_declaration_list (SUB (p));
  } else if (whether_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_serial (NEXT (p), q, k);
  } else if (whether_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, NULL_ATTRIBUTE)) {
    NODE_T *z = NEXT (p);
    if (z != NULL) {
      if (WHETHER (z, EXIT_SYMBOL) || WHETHER (z, END_SYMBOL) || WHETHER (z, CLOSE_SYMBOL) || WHETHER (z, OCCA_SYMBOL)) {
        coerce_serial (SUB (p), q, A68_TRUE);
      } else {
        coerce_serial (SUB (p), q, A68_FALSE);
      }
    } else {
      coerce_serial (SUB (p), q, A68_TRUE);
    }
    coerce_serial (NEXT (p), q, k);
  } else if (WHETHER (p, LABELED_UNIT)) {
    coerce_serial (SUB (p), q, k);
  } else if (WHETHER (p, UNIT)) {
    if (k) {
      coerce_unit (p, q);
    } else {
      SOID_T strongvoid;
      make_soid (&strongvoid, STRONG, MODE (VOID), 0);
      coerce_unit (p, &strongvoid);
    }
  }
}

/*!
\brief coerce closed
\param p position in tree
\param q soid
**/

static void coerce_closed (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, SERIAL_CLAUSE)) {
    coerce_serial (p, q, A68_TRUE);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_closed (NEXT (p), q);
  }
}

/*!
\brief coerce conditional
\param p position in tree
\param q soid
**/

static void coerce_conditional (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (BOOL), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_serial (NEXT_SUB (p), q, A68_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, ELSE_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, ELIF_PART, BRIEF_ELIF_IF_PART, NULL_ATTRIBUTE)) {
      coerce_conditional (SUB (p), q);
    }
  }
}

/*!
\brief coerce unit list
\param p position in tree
\param q soid
**/

static void coerce_unit_list (NODE_T * p, SOID_T * q)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    coerce_unit_list (SUB (p), q);
    coerce_unit_list (NEXT (p), q);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_unit_list (NEXT (p), q);
  } else if (WHETHER (p, UNIT)) {
    coerce_unit (p, q);
    coerce_unit_list (NEXT (p), q);
  }
}

/*!
\brief coerce int case
\param p position in tree
\param q soid
**/

static void coerce_int_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (INT), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, INTEGER_OUT_PART, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE)) {
      coerce_int_case (SUB (p), q);
    }
  }
}

/*!
\brief coerce spec unit list
\param p position in tree
\param q soid
**/

static void coerce_spec_unit_list (NODE_T * p, SOID_T * q)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      coerce_spec_unit_list (SUB (p), q);
    } else if (WHETHER (p, UNIT)) {
      coerce_unit (p, q);
    }
  }
}

/*!
\brief coerce united case
\param p position in tree
\param q soid
**/

static void coerce_united_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MOID (SUB (p)), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_spec_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, UNITED_OUSE_PART, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE)) {
      coerce_united_case (SUB (p), q);
    }
  }
}

/*!
\brief coerce loop
\param p position in tree
**/

static void coerce_loop (NODE_T * p)
{
  if (WHETHER (p, FOR_PART)) {
    coerce_loop (NEXT (p));
  } else if (whether_one_of (p, FROM_PART, BY_PART, TO_PART, NULL_ATTRIBUTE)) {
    SOID_T w;
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (NEXT_SUB (p), &w);
    coerce_loop (NEXT (p));
  } else if (WHETHER (p, WHILE_PART)) {
    SOID_T w;
    make_soid (&w, MEEK, MODE (BOOL), 0);
    coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
    coerce_loop (NEXT (p));
  } else if (whether_one_of (p, DO_PART, ALT_DO_PART, NULL_ATTRIBUTE)) {
    SOID_T w;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&w, STRONG, MODE (VOID), 0);
    coerce_serial (do_p, &w, A68_TRUE);
    if (WHETHER (do_p, SERIAL_CLAUSE)) {
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
      SOID_T sw;
      make_soid (&sw, MEEK, MODE (BOOL), 0);
      coerce_serial (NEXT_SUB (un_p), &sw, A68_TRUE);
    }
  }
}

/*!
\brief coerce struct display
\param r pack
\param p position in tree
**/

static void coerce_struct_display (PACK_T ** r, NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    coerce_struct_display (r, SUB (p));
    coerce_struct_display (r, NEXT (p));
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_struct_display (r, NEXT (p));
  } else if (WHETHER (p, UNIT)) {
    SOID_T s;
    make_soid (&s, STRONG, MOID (*r), 0);
    coerce_unit (p, &s);
    FORWARD (*r);
    coerce_struct_display (r, NEXT (p));
  }
}

/*!
\brief coerce collateral
\param p position in tree
\param q soid
**/

static void coerce_collateral (NODE_T * p, SOID_T * q)
{
  if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, 0) || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0))) {
    if (WHETHER (MOID (q), STRUCT_SYMBOL)) {
      PACK_T *t = PACK (MOID (q));
      coerce_struct_display (&t, p);
    } else if (WHETHER (MOID (q), FLEX_SYMBOL)) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (SUB_MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else if (WHETHER (MOID (q), ROW_SYMBOL)) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else {
/* if (MOID (q) != MODE (VOID)) */
      coerce_unit_list (p, q);
    }
  }
}

/*!
\brief coerce_enclosed
\param p position in tree
\param q soid
**/

void coerce_enclosed (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (SUB (p), q);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    coerce_closed (SUB (p), q);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    coerce_collateral (SUB (p), q);
  } else if (WHETHER (p, PARALLEL_CLAUSE)) {
    coerce_collateral (SUB (NEXT_SUB (p)), q);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    coerce_conditional (SUB (p), q);
  } else if (WHETHER (p, INTEGER_CASE_CLAUSE)) {
    coerce_int_case (SUB (p), q);
  } else if (WHETHER (p, UNITED_CASE_CLAUSE)) {
    coerce_united_case (SUB (p), q);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    coerce_loop (SUB (p));
  }
  MOID (p) = depref_rows (MOID (p), MOID (q));
}

/*!
\brief get monad moid
\param p position in tree
**/

static MOID_T *get_monad_moid (NODE_T * p)
{
  if (TAX (p) != NULL && TAX (p) != error_tag) {
    MOID (p) = MOID (TAX (p));
    return (MOID (PACK (MOID (p))));
  } else {
    return (MODE (ERROR));
  }
}

/*!
\brief coerce monad oper
\param p position in tree
\param q soid
**/

static void coerce_monad_oper (NODE_T * p, SOID_T * q)
{
  if (p != NULL) {
    SOID_T z;
    make_soid (&z, FIRM, MOID (PACK (MOID (TAX (p)))), 0);
    INSERT_COERCIONS (NEXT (p), MOID (q), &z);
  }
}

/*!
\brief coerce monad formula
\param p position in tree
**/

static void coerce_monad_formula (NODE_T * p)
{
  SOID_T e;
  make_soid (&e, STRONG, get_monad_moid (p), 0);
  coerce_operand (NEXT (p), &e);
  coerce_monad_oper (p, &e);
}

/*!
\brief coerce operand
\param p position in tree
\param q soid
**/

static void coerce_operand (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, MONADIC_FORMULA)) {
    coerce_monad_formula (SUB (p));
    if (MOID (p) != MOID (q)) {
      make_sub (p, p, FORMULA);
      INSERT_COERCIONS (p, MOID (p), q);
      make_sub (p, p, TERTIARY);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, SECONDARY)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
  }
}

/*!
\brief coerce formula
\param p position in tree
\param q soid
**/

static void coerce_formula (NODE_T * p, SOID_T * q)
{
  (void) q;
  if (WHETHER (p, MONADIC_FORMULA) && NEXT (p) == NULL) {
    coerce_monad_formula (SUB (p));
  } else {
    if (TAX (NEXT (p)) != NULL && TAX (NEXT (p)) != error_tag) {
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

/*!
\brief coerce assignation
\param p position in tree
**/

static void coerce_assignation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, SOFT, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, SUB_MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

/*!
\brief coerce relation
\param p position in tree
**/

static void coerce_relation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, MOID (NEXT_NEXT (p)), 0);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

/*!
\brief coerce bool function
\param p position in tree
**/

static void coerce_bool_function (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MODE (BOOL), 0);
  coerce_unit (SUB (p), &w);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

/*!
\brief coerce assertion
\param p position in tree
**/

static void coerce_assertion (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (BOOL), 0);
  coerce_enclosed (SUB_NEXT (p), &w);
}

/*!
\brief coerce field-selection
\param p position in tree
**/

static void coerce_field_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK. */ STRONG, MOID (p), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce selection
\param p position in tree
**/

static void coerce_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK. */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce cast
\param p position in tree
**/

static void coerce_cast (NODE_T * p)
{
  SOID_T w;
  coerce_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_enclosed (NEXT (p), &w);
}

/*!
\brief coerce argument list
\param r pack
\param p position in tree
**/

static void coerce_argument_list (PACK_T ** r, NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ARGUMENT_LIST)) {
      coerce_argument_list (r, SUB (p));
    } else if (WHETHER (p, UNIT)) {
      SOID_T s;
      make_soid (&s, STRONG, MOID (*r), 0);
      coerce_unit (p, &s);
      FORWARD (*r);
    } else if (WHETHER (p, TRIMMER)) {
      FORWARD (*r);
    }
  }
}

/*!
\brief coerce call
\param p position in tree
**/

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

/*!
\brief coerce meek int
\param p position in tree
**/

static void coerce_meek_int (NODE_T * p)
{
  SOID_T x;
  make_soid (&x, MEEK, MODE (INT), 0);
  coerce_unit (p, &x);
}

/*!
\brief coerce trimmer
\param p position in tree
**/

static void coerce_trimmer (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, UNIT)) {
      coerce_meek_int (p);
      coerce_trimmer (NEXT (p));
    } else {
      coerce_trimmer (NEXT (p));
    }
  }
}

/*!
\brief coerce indexer
\param p position in tree
**/

static void coerce_indexer (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, TRIMMER)) {
      coerce_trimmer (SUB (p));
    } else if (WHETHER (p, UNIT)) {
      coerce_meek_int (p);
    } else {
      coerce_indexer (SUB (p));
      coerce_indexer (NEXT (p));
    }
  }
}

/*!
\brief coerce_slice
\param p position in tree
**/

static void coerce_slice (NODE_T * p)
{
  SOID_T w;
  MOID_T *row;
  row = MOID (p);
  make_soid (&w, /* WEAK. */ STRONG, row, 0);
  coerce_unit (SUB (p), &w);
  coerce_indexer (SUB_NEXT (p));
}

/*!
\brief mode coerce diagonal
\param p position in tree
**/

static void coerce_diagonal (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, /* WEAK. */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief mode coerce transpose
\param p position in tree
**/

static void coerce_transpose (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK. */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief mode coerce row or column function
\param p position in tree
**/

static void coerce_row_column_function (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, /* WEAK. */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce format text
\param p position in tree
**/

static void coerce_format_text (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    coerce_format_text (SUB (p));
    if (WHETHER (p, FORMAT_PATTERN)) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (FORMAT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (ROW_INT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (INT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    }
  }
}

/*!
\brief coerce unit
\param p position in tree
\param q soid
**/

static void coerce_unit (NODE_T * p, SOID_T * q)
{
  if (p == NULL) {
    return;
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
/* Ex primary. */
  } else if (WHETHER (p, CALL)) {
    coerce_call (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, SLICE)) {
    coerce_slice (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, FIELD_SELECTION)) {
    coerce_field_selection (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, CAST)) {
    coerce_cast (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (whether_one_of (p, DENOTATION, IDENTIFIER, NULL_ATTRIBUTE)) {
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, FORMAT_TEXT)) {
    coerce_format_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (p, q);
/* Ex secondary. */
  } else if (WHETHER (p, SELECTION)) {
    coerce_selection (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, GENERATOR)) {
    coerce_declarer (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
/* Ex tertiary. */
  } else if (WHETHER (p, NIHIL)) {
    if (ATTRIBUTE (MOID (q)) != REF_SYMBOL && MOID (q) != MODE (VOID)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_NAME_REQUIRED);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, DIAGONAL_FUNCTION)) {
    coerce_diagonal (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, TRANSPOSE_FUNCTION)) {
    coerce_transpose (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ROW_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, COLUMN_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
/* Ex unit. */
  } else if (WHETHER (p, JUMP)) {
    if (MOID (q) == MODE (PROC_VOID)) {
      make_sub (p, p, PROCEDURING);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, SKIP)) {
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, ASSIGNATION)) {
    coerce_assignation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    coerce_relation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (whether_one_of (p, AND_FUNCTION, OR_FUNCTION, NULL_ATTRIBUTE)) {
    coerce_bool_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ASSERTION)) {
    coerce_assertion (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  }
}

/*!
\brief widen_denotation
\param p position in tree
**/

void widen_denotation (NODE_T * p)
{
#define WIDEN {\
  *q = *(SUB (q));\
  ATTRIBUTE (q) = DENOTATION;\
  MOID (q) = lm;\
  STATUS_SET (q, OPTIMAL_MASK);\
  }
#define WARN_WIDENING\
  if (program.options.portcheck && !(STATUS_TEST (SUB (q), OPTIMAL_MASK))) {\
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, q, WARNING_WIDENING_NOT_PORTABLE);\
  }
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    widen_denotation (SUB (q));
    if (WHETHER (q, WIDENING) && WHETHER (SUB (q), DENOTATION)) {
      MOID_T *lm = MOID (q), *m = MOID (SUB (q));
      if (lm == MODE (LONGLONG_INT) && m == MODE (LONG_INT)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_INT) && m == MODE (INT)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONGLONG_REAL) && m == MODE (LONG_REAL)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_REAL) && m == MODE (REAL)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_REAL) && m == MODE (LONG_INT)) {
        WIDEN;
      }
      if (lm == MODE (REAL) && m == MODE (INT)) {
        WIDEN;
      }
      if (lm == MODE (LONGLONG_BITS) && m == MODE (LONG_BITS)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_BITS) && m == MODE (BITS)) {
        WARN_WIDENING;
        WIDEN;
      }
      return;
    }
  }
#undef WIDEN
#undef WARN_WIDENING
}
