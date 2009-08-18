/*!
\file moids.c
\brief routines for mode collection, equivalencing and derived modes
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

static MOID_T *get_mode_from_declarer (NODE_T *);
BOOL_T check_yin_yang (NODE_T *, MOID_T *, BOOL_T, BOOL_T);
static void moid_to_string_2 (char *, MOID_T *, int *, NODE_T *);
static MOID_T *make_deflexed (MOID_T *, MOID_T **);

MOID_LIST_T *top_moid_list, *old_moid_list = NULL;
static int max_simplout_size;
static POSTULATE_T *postulates;

/*!
\brief add mode "sub" to chain "z"
\param z chain to insert into
\param att attribute
\param dim dimension
\param node node
\param sub subordinate mode
\param pack pack
\return new entry
**/

MOID_T *add_mode (MOID_T ** z, int att, int dim, NODE_T * node, MOID_T * sub, PACK_T * pack)
{
  MOID_T *new_mode = new_moid ();
  new_mode->in_standard_environ = (BOOL_T) (z == &(stand_env->moids));
  USE (new_mode) = A68_FALSE;
  SIZE (new_mode) = 0;
  NUMBER (new_mode) = mode_count++;
  ATTRIBUTE (new_mode) = att;
  DIM (new_mode) = dim;
  NODE (new_mode) = node;
  new_mode->well_formed = A68_TRUE;
  new_mode->has_rows = (BOOL_T) (att == ROW_SYMBOL);
  SUB (new_mode) = sub;
  PACK (new_mode) = pack;
  NEXT (new_mode) = *z;
  EQUIVALENT (new_mode) = NULL;
  SLICE (new_mode) = NULL;
  DEFLEXED (new_mode) = NULL;
  NAME (new_mode) = NULL;
  MULTIPLE (new_mode) = NULL;
  TRIM (new_mode) = NULL;
  ROWED (new_mode) = NULL;
/* Link to chain and exit. */
  *z = new_mode;
  return (new_mode);
}

/*!
\brief add row and its slices to chain, recursively
\param p chain to insert into
\param dim dimension
\param sub mode of slice
\param n position in tree
\return pointer to entry
**/

static MOID_T *add_row (MOID_T ** p, int dim, MOID_T * sub, NODE_T * n)
{
  (void) add_mode (p, ROW_SYMBOL, dim, n, sub, NULL);
  if (dim > 1) {
    SLICE (*p) = add_row (&NEXT (*p), dim - 1, sub, n);
  } else {
    SLICE (*p) = sub;
  }
  return (*p);
}

/*!
\brief initialise moid list
**/

void init_moid_list (void)
{
  top_moid_list = NULL;
  old_moid_list = NULL;
}

/*!
\brief reset moid list
**/

void reset_moid_list (void)
{
  old_moid_list = top_moid_list;
  top_moid_list = NULL;
}

/*!
\brief add single moid to list
\param p moid list to insert to
\param q moid to insert
\param c symbol table to link to
**/

void add_single_moid_to_list (MOID_LIST_T ** p, MOID_T * q, SYMBOL_TABLE_T * c)
{
  MOID_LIST_T *m;
  if (old_moid_list == NULL) {
    m = (MOID_LIST_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (MOID_LIST_T));
  } else {
    m = old_moid_list;
    old_moid_list = NEXT (old_moid_list);
  }
  m->coming_from_level = c;
  MOID (m) = q;
  NEXT (m) = *p;
  *p = m;
}

/*!
\brief add moid list
\param p chain to insert into
\param c symbol table from which to insert moids
**/

void add_moids_from_table (MOID_LIST_T ** p, SYMBOL_TABLE_T * c)
{
  if (c != NULL) {
    MOID_T *q;
    for (q = c->moids; q != NULL; FORWARD (q)) {
      add_single_moid_to_list (p, q, c);
    }
  }
}

/*!
\brief add moids from symbol tables to moid list
\param p position in tree
\param q chain to insert to
**/

void add_moids_from_table_tree (NODE_T * p, MOID_LIST_T ** q)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL) {
      add_moids_from_table_tree (SUB (p), q);
      if (whether_new_lexical_level (p)) {
        add_moids_from_table (q, SYMBOL_TABLE (SUB (p)));
      }
    }
  }
}

/*!
\brief count moids in a pack
\param u pack
\return same
**/

int count_pack_members (PACK_T * u)
{
  int k = 0;
  for (; u != NULL; FORWARD (u)) {
    k++;
  }
  return (k);
}

/*!
\brief add a moid to a pack, maybe with a (field) name
\param p pack
\param m moid to add
\param text field name
\param node position in tree
**/

void add_mode_to_pack (PACK_T ** p, MOID_T * m, char *text, NODE_T * node)
{
  PACK_T *z = new_pack ();
  MOID (z) = m;
  TEXT (z) = text;
  NODE (z) = node;
  NEXT (z) = *p;
  PREVIOUS (z) = NULL;
  if (NEXT (z) != NULL) {
    PREVIOUS (NEXT (z)) = z;
  }
/* Link in chain. */
  *p = z;
}

/*!
\brief add a moid to a pack, maybe with a (field) name
\param p pack
\param m moid to add
\param text field name
\param node position in tree
**/

void add_mode_to_pack_end (PACK_T ** p, MOID_T * m, char *text, NODE_T * node)
{
  PACK_T *z = new_pack ();
  MOID (z) = m;
  TEXT (z) = text;
  NODE (z) = node;
  NEXT (z) = NULL;
  if (NEXT (z) != NULL) {
    PREVIOUS (NEXT (z)) = z;
  }
/* Link in chain. */
  while ((*p) != NULL) {
    p = &(NEXT (*p));
  }
  PREVIOUS (z) = (*p);
  (*p) = z;
}

/*!
\brief count formal bounds in declarer in tree
\param p position in tree
\return same
**/

static int count_formal_bounds (NODE_T * p)
{
  if (p == NULL) {
    return (0);
  } else {
    if (WHETHER (p, COMMA_SYMBOL)) {
      return (1);
    } else {
      return (count_formal_bounds (NEXT (p)) + count_formal_bounds (SUB (p)));
    }
  }
}

/*!
\brief count bounds in declarer in tree
\param p position in tree
\return same
**/

static int count_bounds (NODE_T * p)
{
  if (p == NULL) {
    return (0);
  } else {
    if (WHETHER (p, BOUND)) {
      return (1 + count_bounds (NEXT (p)));
    } else {
      return (count_bounds (NEXT (p)) + count_bounds (SUB (p)));
    }
  }
}

/*!
\brief count number of SHORTs or LONGs
\param p position in tree
\return same
**/

static int count_sizety (NODE_T * p)
{
  if (p == NULL) {
    return (0);
  } else {
    switch (ATTRIBUTE (p)) {
    case LONGETY:
      {
        return (count_sizety (SUB (p)) + count_sizety (NEXT (p)));
      }
    case SHORTETY:
      {
        return (count_sizety (SUB (p)) + count_sizety (NEXT (p)));
      }
    case LONG_SYMBOL:
      {
        return (1);
      }
    case SHORT_SYMBOL:
      {
        return (-1);
      }
    default:
      {
        return (0);
      }
    }
  }
}

/* Routines to collect MOIDs from the program text. */

/*!
\brief collect standard mode
\param sizety sizety
\param indicant position in tree
\return moid entry in standard environ
**/

static MOID_T *get_mode_from_standard_moid (int sizety, NODE_T * indicant)
{
  MOID_T *p = stand_env->moids;
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, STANDARD) && DIM (p) == sizety && SYMBOL (NODE (p)) == SYMBOL (indicant)) {
      return (p);
    }
  }
  if (sizety < 0) {
    return (get_mode_from_standard_moid (sizety + 1, indicant));
  } else if (sizety > 0) {
    return (get_mode_from_standard_moid (sizety - 1, indicant));
  } else {
    return (NULL);
  }
}

/*!
\brief collect mode from STRUCT field
\param p position in tree
\param u pack to insert to
**/

static void get_mode_from_struct_field (NODE_T * p, PACK_T ** u)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case IDENTIFIER:
      {
        ATTRIBUTE (p) = FIELD_IDENTIFIER;
        (void) add_mode_to_pack (u, NULL, SYMBOL (p), p);
        break;
      }
    case DECLARER:
      {
        MOID_T *new_one = get_mode_from_declarer (p);
        PACK_T *t;
        get_mode_from_struct_field (NEXT (p), u);
        for (t = *u; t && MOID (t) == NULL; FORWARD (t)) {
          MOID (t) = new_one;
          MOID (NODE (t)) = new_one;
        }
        break;
      }
    default:
      {
        get_mode_from_struct_field (NEXT (p), u);
        get_mode_from_struct_field (SUB (p), u);
        break;
      }
    }
  }
}

/*!
\brief collect MODE from formal pack
\param p position in tree
\param u pack to insert to
**/

static void get_mode_from_formal_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        MOID_T *z;
        get_mode_from_formal_pack (NEXT (p), u);
        z = get_mode_from_declarer (p);
        (void) add_mode_to_pack (u, z, NULL, p);
        break;
      }
    default:
      {
        get_mode_from_formal_pack (NEXT (p), u);
        get_mode_from_formal_pack (SUB (p), u);
        break;
      }
    }
  }
}

/*!
\brief collect MODE or VOID from formal UNION pack
\param p position in tree
\param u pack to insert to
**/

static void get_mode_from_union_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
    case VOID_SYMBOL:
      {
        MOID_T *z;
        get_mode_from_union_pack (NEXT (p), u);
        z = get_mode_from_declarer (p);
        (void) add_mode_to_pack (u, z, NULL, p);
        break;
      }
    default:
      {
        get_mode_from_union_pack (NEXT (p), u);
        get_mode_from_union_pack (SUB (p), u);
        break;
      }
    }
  }
}

/*!
\brief collect mode from PROC, OP pack
\param p position in tree
\param u pack to insert to
**/

static void get_mode_from_routine_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case IDENTIFIER:
      {
        (void) add_mode_to_pack (u, NULL, NULL, p);
        break;
      }
    case DECLARER:
      {
        MOID_T *z = get_mode_from_declarer (p);
        PACK_T *t = *u;
        for (; t != NULL && MOID (t) == NULL; FORWARD (t)) {
          MOID (t) = z;
          MOID (NODE (t)) = z;
        }
        (void) add_mode_to_pack (u, z, NULL, p);
        break;
      }
    default:
      {
        get_mode_from_routine_pack (NEXT (p), u);
        get_mode_from_routine_pack (SUB (p), u);
        break;
      }
    }
  }
}

/*!
\brief collect MODE from DECLARER
\param p position in tree
\param put_where insert in symbol table from "p" or in its parent
\return mode table entry or NULL on error
**/

static MOID_T *get_mode_from_declarer (NODE_T * p)
{
  if (p == NULL) {
    return (NULL);
  } else {
    if (WHETHER (p, DECLARER)) {
      if (MOID (p) != NULL) {
        return (MOID (p));
      } else {
        return (MOID (p) = get_mode_from_declarer (SUB (p)));
      }
    } else {
      MOID_T **m = &(SYMBOL_TABLE (p)->moids);
      if (WHETHER (p, VOID_SYMBOL)) {
        MOID (p) = MODE (VOID);
        return (MOID (p));
      } else if (WHETHER (p, LONGETY)) {
        if (whether (p, LONGETY, INDICANT, 0)) {
          int k = count_sizety (SUB (p));
          MOID (p) = get_mode_from_standard_moid (k, NEXT (p));
          return (MOID (p));
        } else {
          return (NULL);
        }
      } else if (WHETHER (p, SHORTETY)) {
        if (whether (p, SHORTETY, INDICANT, 0)) {
          int k = count_sizety (SUB (p));
          MOID (p) = get_mode_from_standard_moid (k, NEXT (p));
          return (MOID (p));
        } else {
          return (NULL);
        }
      } else if (WHETHER (p, INDICANT)) {
        MOID_T *q = get_mode_from_standard_moid (0, p);
        if (q != NULL) {
          MOID (p) = q;
        } else {
          MOID (p) = add_mode (m, INDICANT, 0, p, NULL, NULL);
        }
        return (MOID (p));
      } else if (WHETHER (p, REF_SYMBOL)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (m, REF_SYMBOL, 0, p, new_one, NULL);
        return (MOID (p));
      } else if (WHETHER (p, FLEX_SYMBOL)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (m, FLEX_SYMBOL, 0, p, new_one, NULL);
        SLICE (MOID (p)) = SLICE (new_one);
        return (MOID (p));
      } else if (WHETHER (p, FORMAL_BOUNDS)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_row (m, 1 + count_formal_bounds (SUB (p)), new_one, p);
        return (MOID (p));
      } else if (WHETHER (p, BOUNDS)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_row (m, count_bounds (SUB (p)), new_one, p);
        return (MOID (p));
      } else if (WHETHER (p, STRUCT_SYMBOL)) {
        PACK_T *u = NULL;
        get_mode_from_struct_field (NEXT (p), &u);
        MOID (p) = add_mode (m, STRUCT_SYMBOL, count_pack_members (u), p, NULL, u);
        return (MOID (p));
      } else if (WHETHER (p, UNION_SYMBOL)) {
        PACK_T *u = NULL;
        get_mode_from_union_pack (NEXT (p), &u);
        MOID (p) = add_mode (m, UNION_SYMBOL, count_pack_members (u), p, NULL, u);
        return (MOID (p));
      } else if (WHETHER (p, PROC_SYMBOL)) {
        NODE_T *save = p;
        PACK_T *u = NULL;
        MOID_T *new_one;
        if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
          get_mode_from_formal_pack (SUB (NEXT (p)), &u);
          FORWARD (p);
        }
        new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (m, PROC_SYMBOL, count_pack_members (u), save, new_one, u);
        MOID (save) = MOID (p);
        return (MOID (p));
      } else {
        return (NULL);
      }
    }
  }
}

/*!
\brief collect MODEs from a routine-text header
\param p position in tree
\return mode table entry
**/

static MOID_T *get_mode_from_routine_text (NODE_T * p)
{
  PACK_T *u = NULL;
  MOID_T **m, *n;
  NODE_T *q = p;
  m = &(PREVIOUS (SYMBOL_TABLE (p))->moids);
  if (WHETHER (p, PARAMETER_PACK)) {
    get_mode_from_routine_pack (SUB (p), &u);
    FORWARD (p);
  }
  n = get_mode_from_declarer (p);
  return (add_mode (m, PROC_SYMBOL, count_pack_members (u), q, n, u));
}

/*!
\brief collect modes from operator-plan
\param p position in tree
\return mode table entry
**/

static MOID_T *get_mode_from_operator (NODE_T * p)
{
  PACK_T *u = NULL;
  MOID_T **m = &(SYMBOL_TABLE (p)->moids), *new_one;
  NODE_T *save = p;
  if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
    get_mode_from_formal_pack (SUB (NEXT (p)), &u);
    FORWARD (p);
  }
  new_one = get_mode_from_declarer (NEXT (p));
  MOID (p) = add_mode (m, PROC_SYMBOL, count_pack_members (u), save, new_one, u);
  return (MOID (p));
}

/*!
\brief collect mode from denotation
\param p position in tree
\return mode table entry
**/

static void get_mode_from_denotation (NODE_T * p, int sizety)
{
  if (p != NULL) {
    if (WHETHER (p, ROW_CHAR_DENOTATION)) {
      if (strlen (SYMBOL (p)) == 1) {
        MOID (p) = MODE (CHAR);
      } else {
        MOID (p) = MODE (ROW_CHAR);
      }
    } else if (WHETHER (p, TRUE_SYMBOL) || WHETHER (p, FALSE_SYMBOL)) {
      MOID (p) = MODE (BOOL);
    } else if (WHETHER (p, INT_DENOTATION)) {
      switch (sizety) {
      case 0:
        {
          MOID (p) = MODE (INT);
          break;
        }
      case 1:
        {
          MOID (p) = MODE (LONG_INT);
          break;
        }
      case 2:
        {
          MOID (p) = MODE (LONGLONG_INT);
          break;
        }
      default:
        {
          MOID (p) = (sizety > 0 ? MODE (LONGLONG_INT) : MODE (INT));
          break;
        }
      }
    } else if (WHETHER (p, REAL_DENOTATION)) {
      switch (sizety) {
      case 0:
        {
          MOID (p) = MODE (REAL);
          break;
        }
      case 1:
        {
          MOID (p) = MODE (LONG_REAL);
          break;
        }
      case 2:
        {
          MOID (p) = MODE (LONGLONG_REAL);
          break;
        }
      default:
        {
          MOID (p) = (sizety > 0 ? MODE (LONGLONG_REAL) : MODE (REAL));
          break;
        }
      }
    } else if (WHETHER (p, BITS_DENOTATION)) {
      switch (sizety) {
      case 0:
        {
          MOID (p) = MODE (BITS);
          break;
        }
      case 1:
        {
          MOID (p) = MODE (LONG_BITS);
          break;
        }
      case 2:
        {
          MOID (p) = MODE (LONGLONG_BITS);
          break;
        }
      default:
        {
          MOID (p) = MODE (BITS);
          break;
        }
      }
    } else if (WHETHER (p, LONGETY) || WHETHER (p, SHORTETY)) {
      get_mode_from_denotation (NEXT (p), count_sizety (SUB (p)));
      MOID (p) = MOID (NEXT (p));
    } else if (WHETHER (p, EMPTY_SYMBOL)) {
      MOID (p) = MODE (VOID);
    }
  }
}

/*!
\brief collect modes from the syntax tree
\param p position in tree
\param attribute
**/

static void get_modes_from_tree (NODE_T * p, int attribute)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, VOID_SYMBOL)) {
      MOID (q) = MODE (VOID);
    } else if (WHETHER (q, DECLARER)) {
      if (attribute == VARIABLE_DECLARATION) {
        MOID_T **m = &(SYMBOL_TABLE (q)->moids);
        MOID_T *new_one = get_mode_from_declarer (q);
        MOID (q) = add_mode (m, REF_SYMBOL, 0, NULL, new_one, NULL);
      } else {
        MOID (q) = get_mode_from_declarer (q);
      }
    } else if (WHETHER (q, ROUTINE_TEXT)) {
      MOID (q) = get_mode_from_routine_text (SUB (q));
    } else if (WHETHER (q, OPERATOR_PLAN)) {
      MOID (q) = get_mode_from_operator (SUB (q));
    } else if (whether_one_of (q, LOC_SYMBOL, HEAP_SYMBOL, NEW_SYMBOL, NULL)) {
      if (attribute == GENERATOR) {
        MOID_T **m = &(SYMBOL_TABLE (q)->moids);
        MOID_T *new_one = get_mode_from_declarer (NEXT (q));
        MOID (NEXT (q)) = new_one;
        MOID (q) = add_mode (m, REF_SYMBOL, 0, NULL, new_one, NULL);
      }
    } else {
      if (attribute == DENOTATION) {
        get_mode_from_denotation (q, 0);
      }
    }
  }
  if (attribute != DENOTATION) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (SUB (q) != NULL) {
        get_modes_from_tree (SUB (q), ATTRIBUTE (q));
      }
    }
  }
}

/*!
\brief collect modes from proc variables
\param p position in tree
**/

static void get_mode_from_proc_variables (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      get_mode_from_proc_variables (SUB (p));
      get_mode_from_proc_variables (NEXT (p));
    } else if (WHETHER (p, QUALIFIER) || WHETHER (p, PROC_SYMBOL) || WHETHER (p, COMMA_SYMBOL)) {
      get_mode_from_proc_variables (NEXT (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      MOID_T **m = &(SYMBOL_TABLE (p)->moids), *new_one = MOID (NEXT_NEXT (p));
      MOID (p) = add_mode (m, REF_SYMBOL, 0, p, new_one, NULL);
    }
  }
}

/*!
\brief collect modes from proc variable declarations
\param p position in tree
**/

static void get_mode_from_proc_var_declarations_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    get_mode_from_proc_var_declarations_tree (SUB (p));
    if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      get_mode_from_proc_variables (p);
    }
  }
}

/* Various routines to test modes. */

/*!
\brief test whether a MODE shows VOID
\param m moid under test
\return same
**/

static BOOL_T whether_mode_has_void (MOID_T * m)
{
  if (m == MODE (VOID)) {
    return (A68_TRUE);
  } else if (whether_postulated_pair (top_postulate, m, NULL)) {
    return (A68_FALSE);
  } else {
    int z = ATTRIBUTE (m);
    make_postulate (&top_postulate, m, NULL);
    if (z == REF_SYMBOL || z == FLEX_SYMBOL || z == ROW_SYMBOL) {
      return whether_mode_has_void (SUB (m));
    } else if (z == STRUCT_SYMBOL) {
      PACK_T *p;
      for (p = PACK (m); p != NULL; FORWARD (p)) {
        if (whether_mode_has_void (MOID (p))) {
          return (A68_TRUE);
        }
      }
      return (A68_FALSE);
    } else if (z == UNION_SYMBOL) {
      PACK_T *p;
      for (p = PACK (m); p != NULL; FORWARD (p)) {
        if (MOID (p) != MODE (VOID) && whether_mode_has_void (MOID (p))) {
          return (A68_TRUE);
        }
      }
      return (A68_FALSE);
    } else if (z == PROC_SYMBOL) {
      PACK_T *p;
      for (p = PACK (m); p != NULL; FORWARD (p)) {
        if (whether_mode_has_void (MOID (p))) {
          return (A68_TRUE);
        }
      }
      if (SUB (m) == MODE (VOID)) {
        return (A68_FALSE);
      } else {
        return (whether_mode_has_void (SUB (m)));
      }
    } else {
      return (A68_FALSE);
    }
  }
}

/*!
\brief check for modes that are related to VOID
\param p position in tree
**/

static void check_relation_to_void_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m;
      for (m = SYMBOL_TABLE (SUB (p))->moids; m != NULL; FORWARD (m)) {
        free_postulate_list (top_postulate, NULL);
        top_postulate = NULL;
        if (NODE (m) != NULL && whether_mode_has_void (m)) {
          diagnostic_node (A68_ERROR, NODE (m), ERROR_RELATED_MODES, m, MODE (VOID));
        }
      }
    }
    check_relation_to_void_tree (SUB (p));
  }
}

/*!
\brief absorb UNION pack
\param t pack
\param mods modification count 
\return absorbed pack
**/

PACK_T *absorb_union_pack (PACK_T * t, int *mods)
{
  PACK_T *z = NULL;
  for (; t != NULL; FORWARD (t)) {
    if (WHETHER (MOID (t), UNION_SYMBOL)) {
      PACK_T *s;
      (*mods)++;
      for (s = PACK (MOID (t)); s != NULL; FORWARD (s)) {
        (void) add_mode_to_pack (&z, MOID (s), NULL, NODE (s));
      }
    } else {
      (void) add_mode_to_pack (&z, MOID (t), NULL, NODE (t));
    }
  }
  return (z);
}

/*!
\brief absorb UNION members troughout symbol tables
\param p position in tree
\param mods modification count 
**/

static void absorb_unions_tree (NODE_T * p, int *mods)
{
/*
UNION (A, UNION (B, C)) = UNION (A, B, C) or
UNION (A, UNION (A, B)) = UNION (A, B).
*/
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m;
      for (m = SYMBOL_TABLE (SUB (p))->moids; m != NULL; FORWARD (m)) {
        if (WHETHER (m, UNION_SYMBOL)) {
          PACK (m) = absorb_union_pack (PACK (m), mods);
        }
      }
    }
    absorb_unions_tree (SUB (p), mods);
  }
}

/*!
\brief contract a UNION
\param u united mode
\param mods modification count 
**/

void contract_union (MOID_T * u, int *mods)
{
  PACK_T *s = PACK (u);
  for (; s != NULL; FORWARD (s)) {
    PACK_T *t = s;
    while (t != NULL) {
      if (NEXT (t) != NULL && MOID (NEXT (t)) == MOID (s)) {
        (*mods)++;
        MOID (t) = MOID (t);
        NEXT (t) = NEXT_NEXT (t);
      } else {
        FORWARD (t);
      }
    }
  }
}

/*!
\brief contract UNIONs troughout symbol tables
\param p position in tree
\param mods modification count 
**/

static void contract_unions_tree (NODE_T * p, int *mods)
{
/* UNION (A, B, A) -> UNION (A, B). */
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m;
      for (m = SYMBOL_TABLE (SUB (p))->moids; m != NULL; FORWARD (m)) {
        if (WHETHER (m, UNION_SYMBOL) && EQUIVALENT (m) == NULL) {
          contract_union (m, mods);
        }
      }
    }
    contract_unions_tree (SUB (p), mods);
  }
}

/*!
\brief bind indicants in symbol tables to tags in syntax tree
\param p position in tree
**/

static void bind_indicants_to_tags_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *s = SYMBOL_TABLE (SUB (p));
      TAG_T *z;
      for (z = s->indicants; z != NULL; FORWARD (z)) {
        TAG_T *y = find_tag_global (s, INDICANT, SYMBOL (NODE (z)));
        if (y != NULL && NODE (y) != NULL) {
          MOID (z) = MOID (NEXT_NEXT (NODE (y)));
        }
      }
    }
    bind_indicants_to_tags_tree (SUB (p));
  }
}

/*!
\brief bind indicants in symbol tables to tags in syntax tree
\param p position in tree
**/

static void bind_indicants_to_modes_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *s = SYMBOL_TABLE (SUB (p));
      MOID_T *z;
      for (z = s->moids; z != NULL; FORWARD (z)) {
        if (WHETHER (z, INDICANT)) {
          TAG_T *y = find_tag_global (s, INDICANT, SYMBOL (NODE (z)));
          if (y != NULL && NODE (y) != NULL) {
            EQUIVALENT (z) = MOID (NEXT_NEXT (NODE (y)));
          } else {
            diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG_2, SYMBOL (NODE (z)));
          }
        }
      }
    }
    bind_indicants_to_modes_tree (SUB (p));
  }
}

/*!
\brief whether a mode declaration refers to self
\param table first tag in chain
\param p mode under test
\return same
**/

static BOOL_T cyclic_declaration (TAG_T * table, MOID_T * p)
{
  if (WHETHER (p, VOID_SYMBOL)) {
    return (A68_TRUE);
  } else if (WHETHER (p, INDICANT)) {
    if (whether_postulated (top_postulate, p)) {
      return (A68_TRUE);
    } else {
      TAG_T *z;
      for (z = table; z != NULL; FORWARD (z)) {
        if (SYMBOL (NODE (z)) == SYMBOL (NODE (p))) {
          make_postulate (&top_postulate, p, NULL);
          return (cyclic_declaration (table, MOID (z)));
        }
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief check for cyclic mode chains like MODE A = B, B = C, C = A
\param p position in tree
**/

static void check_cyclic_modes_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      TAG_T *table = SYMBOL_TABLE (SUB (p))->indicants, *z;
      for (z = table; z != NULL; FORWARD (z)) {
        free_postulate_list (top_postulate, NULL);
        top_postulate = NULL;
        if (cyclic_declaration (table, MOID (z))) {
          diagnostic_node (A68_ERROR, NODE (z), ERROR_CYCLIC_MODE, MOID (z));
        }
      }
    }
    check_cyclic_modes_tree (SUB (p));
  }
}

/*!
\brief check flex mode chains like MODE A = FLEX B, B = C, C = INT
\param p position in tree
**/

static void check_flex_modes_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *z;
      for (z = SYMBOL_TABLE (SUB (p))->moids; z != NULL; FORWARD (z)) {
        if (WHETHER (z, FLEX_SYMBOL)) {
          NODE_T *err = NODE (z);
          MOID_T *sub = SUB (z);
          while (WHETHER (sub, INDICANT)) {
            sub = EQUIVALENT (sub);
          }
          if (WHETHER_NOT (sub, ROW_SYMBOL)) {
            diagnostic_node (A68_ERROR, (err == NULL ? p : err), ERROR_FLEX_ROW);
          }
        }
      }
    }
    check_flex_modes_tree (SUB (p));
  }
}

/*!
\brief whether pack is well-formed
\param p position in tree
\param s pack
\param yin yin
\param yang yang
\return same
**/

static BOOL_T check_yin_yang_pack (NODE_T * p, PACK_T * s, BOOL_T yin, BOOL_T yang)
{
  for (; s != NULL; FORWARD (s)) {
    if (! check_yin_yang (p, MOID (s), yin, yang)) {
      return (A68_FALSE);
    }
  }
  return (A68_TRUE);
}

/*!
\brief whether mode is well-formed
\param def position in tree of definition
\param dec moid of declarer
\param yin yin
\param yang yang
\return same
**/

BOOL_T check_yin_yang (NODE_T * def, MOID_T * dec, BOOL_T yin, BOOL_T yang)
{
  if (dec->well_formed == A68_FALSE) {
    return (A68_TRUE);
  } else {
    if (WHETHER (dec, VOID_SYMBOL)) {
      return (A68_TRUE);
    } else if (WHETHER (dec, STANDARD)) {
      return (A68_TRUE);
    } else if (WHETHER (dec, INDICANT)) {
      if (SYMBOL (def) == SYMBOL (NODE (dec))) {
        return ((BOOL_T) (yin && yang));
      } else {
        TAG_T *s = SYMBOL_TABLE (def)->indicants;
        BOOL_T z = A68_TRUE;
        while (s != NULL && z) {
          if (SYMBOL (NODE (s)) == SYMBOL (NODE (dec))) {
            z = A68_FALSE;
          } else {
            FORWARD (s);
          }
        }
        return ((BOOL_T) (s == NULL ? A68_TRUE : check_yin_yang (def, MOID (s), yin, yang)));
      }
    } else if (WHETHER (dec, REF_SYMBOL)) {
      return ((BOOL_T) (yang ? A68_TRUE : check_yin_yang (def, SUB (dec), A68_TRUE, yang)));
    } else if (WHETHER (dec, FLEX_SYMBOL) || WHETHER (dec, ROW_SYMBOL)) {
      return ((BOOL_T) (check_yin_yang (def, SUB (dec), yin, yang)));
    } else if (WHETHER (dec, ROW_SYMBOL)) {
      return ((BOOL_T) (check_yin_yang (def, SUB (dec), yin, yang)));
    } else if (WHETHER (dec, STRUCT_SYMBOL)) {
      return ((BOOL_T) (yin ? A68_TRUE : check_yin_yang_pack (def, PACK (dec), yin, A68_TRUE)));
    } else if (WHETHER (dec, UNION_SYMBOL)) {
      return ((BOOL_T) check_yin_yang_pack (def, PACK (dec), yin, yang));
    } else if (WHETHER (dec, PROC_SYMBOL)) {
      if (PACK (dec) != NULL) {
        return (A68_TRUE);
      } else {
        return ((BOOL_T) (yang ? A68_TRUE : check_yin_yang (def, SUB (dec), A68_TRUE, yang)));
      }
    } else {
      return (A68_FALSE);
    }
  }
}

/*!
\brief check well-formedness of modes in the program
\param p position in tree
**/

static void check_well_formedness_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    check_well_formedness_tree (SUB (p));
    if (WHETHER (p, DEFINING_INDICANT)) {
      MOID_T *z = NULL;
      if (NEXT (p) != NULL && NEXT_NEXT (p) != NULL) {
        z = MOID (NEXT_NEXT (p));
      }
      if (!check_yin_yang (p, z, A68_FALSE, A68_FALSE)) {
        diagnostic_node (A68_ERROR, p, ERROR_NOT_WELL_FORMED);
        z->well_formed = A68_FALSE;
      }
    }
  }
}

/*
After the initial version of the mode equivalencer was made to work (1993), I
found: Algol Bulletin 30.3.3 C.H.A. Koster: On infinite modes, 86-89 [1969],
which essentially concurs with the algorithm on mode equivalence I wrote (and
which is still here). It is basic logic anyway: prove equivalence of things
postulating their equivalence.
*/

/*!
\brief whether packs s and t are equivalent
\param s pack 1
\param t pack 2
\return same
**/

static BOOL_T whether_packs_equivalent (PACK_T * s, PACK_T * t)
{
  for (; s != NULL && t != NULL; FORWARD (s), FORWARD (t)) {
    if (!whether_modes_equivalent (MOID (s), MOID (t))) {
      return (A68_FALSE);
    }
    if (TEXT (s) != TEXT (t)) {
      return (A68_FALSE);
    }
  }
  return ((BOOL_T) (s == NULL && t == NULL));
}

/*!
\brief whether packs contain each others' modes
\param s united pack 1
\param t united pack 2
\return same
**/

static BOOL_T whether_united_packs_equivalent (PACK_T * s, PACK_T * t)
{
  PACK_T *p, *q; BOOL_T f;
/* s is a subset of t ... */
  for (p = s; p != NULL; FORWARD (p)) {
    for (f = A68_FALSE, q = t; q != NULL && !f; FORWARD (q)) {
      f = whether_modes_equivalent (MOID (p), MOID (q));
    }
    if (!f) {
      return (A68_FALSE);
    }
  }
/* ... and t is a subset of s ... */
  for (p = t; p != NULL; FORWARD (p)) {
    for (f = A68_FALSE, q = s; q != NULL && !f; FORWARD (q)) {
      f = whether_modes_equivalent (MOID (p), MOID (q));
    }
    if (!f) {
      return (A68_FALSE);
    }
  }
  return (A68_TRUE);
}

/*!
\brief whether moids a and b are structurally equivalent
\param a moid
\param b moid
\return same
**/

BOOL_T whether_modes_equivalent (MOID_T * a, MOID_T * b)
{
  if (a == b) {
    return (A68_TRUE);
  } else if (ATTRIBUTE (a) != ATTRIBUTE (b)) {
    return (A68_FALSE);
  } else if (WHETHER (a, STANDARD)) {
    return ((BOOL_T) (a == b));
  } else if (EQUIVALENT (a) == b || EQUIVALENT (b) == a) {
    return (A68_TRUE);
  } else if (whether_postulated_pair (top_postulate, a, b) || whether_postulated_pair (top_postulate, b, a)) {
    return (A68_TRUE);
  } else if (WHETHER (a, INDICANT)) {
    return ((BOOL_T) whether_modes_equivalent (EQUIVALENT (a), EQUIVALENT (b)));
  } else if (WHETHER (a, REF_SYMBOL)) {
    return ((BOOL_T) whether_modes_equivalent (SUB (a), SUB (b)));
  } else if (WHETHER (a, FLEX_SYMBOL)) {
    return ((BOOL_T) whether_modes_equivalent (SUB (a), SUB (b)));
  } else if (WHETHER (a, ROW_SYMBOL)) {
    return ((BOOL_T) (DIM (a) == DIM (b) && whether_modes_equivalent (SUB (a), SUB (b))));
  } else if (WHETHER (a, PROC_SYMBOL) && DIM (a) == 0) {
    if (DIM (b) == 0) {
      return ((BOOL_T) whether_modes_equivalent (SUB (a), SUB (b)));
    } else {
      return (A68_FALSE);
    }
  } else if (WHETHER (a, STRUCT_SYMBOL)) {
    POSTULATE_T *save; BOOL_T z;
    if (DIM (a) != DIM (b)) {
      return (A68_FALSE);
    }
    save = top_postulate;
    make_postulate (&top_postulate, a, b);
    z = whether_packs_equivalent (PACK (a), PACK (b));
    free_postulate_list (top_postulate, save);
    top_postulate = save;
    return (z);
  } else if (WHETHER (a, UNION_SYMBOL)) {
    return ((BOOL_T) whether_united_packs_equivalent (PACK (a), PACK (b)));
  } else if (WHETHER (a, PROC_SYMBOL) && DIM (a) > 0) {
    POSTULATE_T *save; BOOL_T z;
    if (DIM (a) != DIM (b)) {
      return (A68_FALSE);
    }
    if (ATTRIBUTE (SUB (a)) != ATTRIBUTE (SUB (b))) {
      return (A68_FALSE);
    }
    if (WHETHER (SUB (a), STANDARD) && SUB (a) != SUB (b)) {
      return (A68_FALSE);
    }
    save = top_postulate;
    make_postulate (&top_postulate, a, b);
    z = whether_modes_equivalent (SUB (a), SUB (b));
    if (z) {
      z = whether_packs_equivalent (PACK (a), PACK (b));
    }
    free_postulate_list (top_postulate, save);
    top_postulate = save;
    return (z);
  } else if (WHETHER (a, SERIES_MODE) || WHETHER (a, STOWED_MODE)) {
     return ((BOOL_T) (DIM (a) == DIM (b) && whether_packs_equivalent (PACK (a), PACK (b))));
  }
  ABNORMAL_END (A68_TRUE, "cannot decide in whether_modes_equivalent", NULL);
  return (A68_FALSE);
}

/*!
\brief whether modes 1 and 2 are structurally equivalent
\param p mode 1
\param q mode 2
\return same
**/

static BOOL_T prove_moid_equivalence (MOID_T * p, MOID_T * q)
{
/* Prove that two modes are equivalent under assumption that they are. */
  POSTULATE_T *save = top_postulate;
  BOOL_T z = whether_modes_equivalent (p, q);
/* If modes are equivalent, mark this depending on which one is in standard environ. */
  if (z) {
    if (q->in_standard_environ) {
      EQUIVALENT (p) = q;
    } else {
      EQUIVALENT (q) = p;
    }
  }
  free_postulate_list (top_postulate, save);
  top_postulate = save;
  return (z);
}

/*!
\brief find equivalent modes in program
\param start moid in moid list where to start
\param stop moid in moid list where to stop
**/

static void find_equivalent_moids (MOID_LIST_T * start, MOID_LIST_T * stop)
{
  for (; start != stop; FORWARD (start)) {
    MOID_LIST_T *p;
    MOID_T *master = MOID (start);
    for (p = NEXT (start); (p != NULL) && (EQUIVALENT (master) == NULL); FORWARD (p)) {
      MOID_T *slave = MOID (p);
      if ((EQUIVALENT (slave) == NULL) && (ATTRIBUTE (master) == ATTRIBUTE (slave)) && (DIM (master) == DIM (slave))) {
        (void) prove_moid_equivalence (slave, master);
      }
    }
  }
}

/*!
\brief replace a mode by its equivalent mode
\param m mode to replace
**/

static void track_equivalent_modes (MOID_T ** m)
{
  while ((*m) != NULL && EQUIVALENT ((*m)) != NULL) {
    (*m) = EQUIVALENT (*m);
  }
}

/*!
\brief replace a mode by its equivalent mode (walk chain)
\param q mode to track
**/

static void track_equivalent_one_moid (MOID_T * q)
{
  PACK_T *p;
  track_equivalent_modes (&SUB (q));
  track_equivalent_modes (&DEFLEXED (q));
  track_equivalent_modes (&MULTIPLE (q));
  track_equivalent_modes (&NAME (q));
  track_equivalent_modes (&SLICE (q));
  track_equivalent_modes (&TRIM (q));
  track_equivalent_modes (&ROWED (q));
  for (p = PACK (q); p != NULL; FORWARD (p)) {
    track_equivalent_modes (&MOID (p));
  }
}

/*!
\brief moid list track equivalent
\param q mode to track
**/

static void moid_list_track_equivalent (MOID_T * q)
{
  for (; q != NULL; FORWARD (q)) {
    track_equivalent_one_moid (q);
  }
}

/*!
\brief track equivalent tags
\param z tag to track
**/

static void track_equivalent_tags (TAG_T * z)
{
  for (; z != NULL; FORWARD (z)) {
    while (EQUIVALENT (MOID (z)) != NULL) {
      MOID (z) = EQUIVALENT (MOID (z));
    }
  }
}

/*!
\brief track equivalent tree
\param p position in tree
**/

static void track_equivalent_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (MOID (p) != NULL) {
      while (EQUIVALENT (MOID (p)) != NULL) {
        MOID (p) = EQUIVALENT (MOID (p));
      }
    }
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      if (SYMBOL_TABLE (SUB (p)) != NULL) {
        track_equivalent_tags (SYMBOL_TABLE (SUB (p))->indicants);
        moid_list_track_equivalent (SYMBOL_TABLE (SUB (p))->moids);
      }
    }
    track_equivalent_tree (SUB (p));
  }
}

/*!
\brief track equivalent standard modes
**/

static void track_equivalent_standard_modes (void)
{
  track_equivalent_modes (&MODE (BITS));
  track_equivalent_modes (&MODE (BOOL));
  track_equivalent_modes (&MODE (BYTES));
  track_equivalent_modes (&MODE (CHANNEL));
  track_equivalent_modes (&MODE (CHAR));
  track_equivalent_modes (&MODE (COLLITEM));
  track_equivalent_modes (&MODE (COMPL));
  track_equivalent_modes (&MODE (COMPLEX));
  track_equivalent_modes (&MODE (C_STRING));
  track_equivalent_modes (&MODE (ERROR));
  track_equivalent_modes (&MODE (FILE));
  track_equivalent_modes (&MODE (FORMAT));
  track_equivalent_modes (&MODE (HIP));
  track_equivalent_modes (&MODE (INT));
  track_equivalent_modes (&MODE (LONG_BITS));
  track_equivalent_modes (&MODE (LONG_BYTES));
  track_equivalent_modes (&MODE (LONG_COMPL));
  track_equivalent_modes (&MODE (LONG_COMPLEX));
  track_equivalent_modes (&MODE (LONG_INT));
  track_equivalent_modes (&MODE (LONGLONG_BITS));
  track_equivalent_modes (&MODE (LONGLONG_COMPL));
  track_equivalent_modes (&MODE (LONGLONG_COMPLEX));
  track_equivalent_modes (&MODE (LONGLONG_INT));
  track_equivalent_modes (&MODE (LONGLONG_REAL));
  track_equivalent_modes (&MODE (LONG_REAL));
  track_equivalent_modes (&MODE (NUMBER));
  track_equivalent_modes (&MODE (PIPE));
  track_equivalent_modes (&MODE (PROC_REF_FILE_BOOL));
  track_equivalent_modes (&MODE (PROC_REF_FILE_VOID));
  track_equivalent_modes (&MODE (PROC_ROW_CHAR));
  track_equivalent_modes (&MODE (PROC_STRING));
  track_equivalent_modes (&MODE (PROC_VOID));
  track_equivalent_modes (&MODE (REAL));
  track_equivalent_modes (&MODE (REF_BITS));
  track_equivalent_modes (&MODE (REF_BOOL));
  track_equivalent_modes (&MODE (REF_BYTES));
  track_equivalent_modes (&MODE (REF_CHAR));
  track_equivalent_modes (&MODE (REF_COMPL));
  track_equivalent_modes (&MODE (REF_COMPLEX));
  track_equivalent_modes (&MODE (REF_FILE));
  track_equivalent_modes (&MODE (REF_FORMAT));
  track_equivalent_modes (&MODE (REF_INT));
  track_equivalent_modes (&MODE (REF_LONG_BITS));
  track_equivalent_modes (&MODE (REF_LONG_BYTES));
  track_equivalent_modes (&MODE (REF_LONG_COMPL));
  track_equivalent_modes (&MODE (REF_LONG_COMPLEX));
  track_equivalent_modes (&MODE (REF_LONG_INT));
  track_equivalent_modes (&MODE (REF_LONGLONG_BITS));
  track_equivalent_modes (&MODE (REF_LONGLONG_COMPL));
  track_equivalent_modes (&MODE (REF_LONGLONG_COMPLEX));
  track_equivalent_modes (&MODE (REF_LONGLONG_INT));
  track_equivalent_modes (&MODE (REF_LONGLONG_REAL));
  track_equivalent_modes (&MODE (REF_LONG_REAL));
  track_equivalent_modes (&MODE (REF_PIPE));
  track_equivalent_modes (&MODE (REF_REAL));
  track_equivalent_modes (&MODE (REF_REF_FILE));
  track_equivalent_modes (&MODE (REF_ROW_CHAR));
  track_equivalent_modes (&MODE (REF_ROW_COMPLEX));
  track_equivalent_modes (&MODE (REF_ROW_INT));
  track_equivalent_modes (&MODE (REF_ROW_REAL));
  track_equivalent_modes (&MODE (REF_ROWROW_COMPLEX));
  track_equivalent_modes (&MODE (REF_ROWROW_REAL));
  track_equivalent_modes (&MODE (REF_SOUND));
  track_equivalent_modes (&MODE (REF_STRING));
  track_equivalent_modes (&MODE (ROW_BITS));
  track_equivalent_modes (&MODE (ROW_BOOL));
  track_equivalent_modes (&MODE (ROW_CHAR));
  track_equivalent_modes (&MODE (ROW_COMPLEX));
  track_equivalent_modes (&MODE (ROW_INT));
  track_equivalent_modes (&MODE (ROW_LONG_BITS));
  track_equivalent_modes (&MODE (ROW_LONGLONG_BITS));
  track_equivalent_modes (&MODE (ROW_REAL));
  track_equivalent_modes (&MODE (ROW_ROW_CHAR));
  track_equivalent_modes (&MODE (ROWROW_COMPLEX));
  track_equivalent_modes (&MODE (ROWROW_REAL));
  track_equivalent_modes (&MODE (ROWS));
  track_equivalent_modes (&MODE (ROW_SIMPLIN));
  track_equivalent_modes (&MODE (ROW_SIMPLOUT));
  track_equivalent_modes (&MODE (ROW_STRING));
  track_equivalent_modes (&MODE (SEMA));
  track_equivalent_modes (&MODE (SIMPLIN));
  track_equivalent_modes (&MODE (SIMPLOUT));
  track_equivalent_modes (&MODE (SOUND));
  track_equivalent_modes (&MODE (SOUND_DATA));
  track_equivalent_modes (&MODE (STRING));
  track_equivalent_modes (&MODE (UNDEFINED));
  track_equivalent_modes (&MODE (VACUUM));
  track_equivalent_modes (&MODE (VOID));
}

/*
Routines for calculating subordinates for selections, for instance selection
from REF STRUCT (A) yields REF A fields and selection from [] STRUCT (A) yields
[] A fields.
*/

/*!
\brief make name pack
\param src source pack
\param dst destination pack with REF modes
\param p chain to insert new modes into
**/

static void make_name_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p)
{
  if (src != NULL) {
    MOID_T *z;
    make_name_pack (NEXT (src), dst, p);
    z = add_mode (p, REF_SYMBOL, 0, NULL, MOID (src), NULL);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

/*!
\brief make name struct
\param m structured mode
\param p chain to insert new modes into
\return mode table entry
**/

static MOID_T *make_name_struct (MOID_T * m, MOID_T ** p)
{
  MOID_T *save;
  PACK_T *u = NULL;
  (void) add_mode (p, STRUCT_SYMBOL, DIM (m), NULL, NULL, NULL);
  save = *p;
  make_name_pack (PACK (m), &u, p);
  PACK (save) = u;
  return (save);
}

/*!
\brief make name row
\param m rowed mode
\param p chain to insert new modes into
\return mode table entry
**/

static MOID_T *make_name_row (MOID_T * m, MOID_T ** p)
{
  if (SLICE (m) != NULL) {
    return (add_mode (p, REF_SYMBOL, 0, NULL, SLICE (m), NULL));
  } else {
    return (add_mode (p, REF_SYMBOL, 0, NULL, SUB (m), NULL));
  }
}

/*!
\brief make structured names
\param p position in tree
\param mods modification count
**/

static void make_stowed_names_tree (NODE_T * p, int *mods)
{
  for (; p != NULL; FORWARD (p)) {
/* Dive into lexical levels. */
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *symbol_table = SYMBOL_TABLE (SUB (p));
      MOID_T **topmoid = &(symbol_table->moids);
      BOOL_T k = A68_TRUE;
      while (k) {
        MOID_T *m = symbol_table->moids;
        k = A68_FALSE;
        for (; m != NULL; FORWARD (m)) {
          if (NAME (m) == NULL && WHETHER (m, REF_SYMBOL)) {
            if (WHETHER (SUB (m), STRUCT_SYMBOL)) {
              k = A68_TRUE;
              (*mods)++;
              NAME (m) = make_name_struct (SUB (m), topmoid);
            } else if (WHETHER (SUB (m), ROW_SYMBOL)) {
              k = A68_TRUE;
              (*mods)++;
              NAME (m) = make_name_row (SUB (m), topmoid);
            } else if (WHETHER (SUB (m), FLEX_SYMBOL)) {
              k = A68_TRUE;
              (*mods)++;
              NAME (m) = make_name_row (SUB (SUB (m)), topmoid);
            }
          }
        }
      }
    }
    make_stowed_names_tree (SUB (p), mods);
  }
}

/*!
\brief make multiple row pack
\param src source pack
\param dst destination pack with REF modes
\param p chain to insert new modes into
\param dim dimension
**/

static void make_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NULL) {
    make_multiple_row_pack (NEXT (src), dst, p, dim);
    (void) add_mode_to_pack (dst, add_row (p, dim, MOID (src), NULL), TEXT (src), NODE (src));
  }
}

/*!
\brief make multiple struct
\param m structured mode
\param p chain to insert new modes into
\param dim dimension
\return mode table entry
**/

static MOID_T *make_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  MOID_T *save;
  PACK_T *u = NULL;
  (void) add_mode (p, STRUCT_SYMBOL, DIM (m), NULL, NULL, NULL);
  save = *p;
  make_multiple_row_pack (PACK (m), &u, p, dim);
  PACK (save) = u;
  return (save);
}

/*!
\brief make flex multiple row pack
\param src source pack
\param dst destination pack with REF modes
\param p chain to insert new modes into
\param dim dimension
**/

static void make_flex_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NULL) {
    MOID_T *z;
    make_flex_multiple_row_pack (NEXT (src), dst, p, dim);
    z = add_row (p, dim, MOID (src), NULL);
    z = add_mode (p, FLEX_SYMBOL, 0, NULL, z, NULL);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

/*!
\brief make flex multiple struct
\param m structured mode
\param p chain to insert new modes into
\param dim dimension
\return mode table entry
**/

static MOID_T *make_flex_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  MOID_T *x;
  PACK_T *u = NULL;
  (void) add_mode (p, STRUCT_SYMBOL, DIM (m), NULL, NULL, NULL);
  x = *p;
  make_flex_multiple_row_pack (PACK (m), &u, p, dim);
  PACK (x) = u;
  return (x);
}

/*!
\brief make multiple modes
\param p position in tree
\param mods modification count
**/

static void make_multiple_modes_tree (NODE_T * p, int *mods)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *symbol_table = SYMBOL_TABLE (SUB (p));
      MOID_T **top = &symbol_table->moids;
      BOOL_T z = A68_TRUE;
      while (z) {
        MOID_T *q = symbol_table->moids;
        z = A68_FALSE;
        for (; q != NULL; FORWARD (q)) {
          if (MULTIPLE (q) != NULL) {
            ;
          } else if (WHETHER (q, REF_SYMBOL)) {
            if (MULTIPLE (SUB (q)) != NULL) {
              (*mods)++;
              MULTIPLE (q) = make_name_struct (MULTIPLE (SUB (q)), top);
            }
          } else if (WHETHER (q, ROW_SYMBOL)) {
            if (WHETHER (SUB (q), STRUCT_SYMBOL)) {
              z = A68_TRUE;
              (*mods)++;
              MULTIPLE (q) = make_multiple_struct (SUB (q), top, DIM (q));
            }
          } else if (WHETHER (q, FLEX_SYMBOL)) {
            if (SUB (SUB (q)) == NULL) {
              (*mods)++;        /* as yet unresolved FLEX INDICANT. */
            } else {
              if (WHETHER (SUB (SUB (q)), STRUCT_SYMBOL)) {
                z = A68_TRUE;
                (*mods)++;
                MULTIPLE (q) = make_flex_multiple_struct (SUB (SUB (q)), top, DIM (SUB (q)));
              }
            }
          }
        }
      }
    }
    make_multiple_modes_tree (SUB (p), mods);
  }
}

/*!
\brief make multiple modes in standard environ
\param mods modification count
**/

static void make_multiple_modes_standenv (int *mods)
{
  MOID_T **top = &stand_env->moids;
  BOOL_T z = A68_TRUE;
  while (z) {
    MOID_T *q = stand_env->moids;
    z = A68_FALSE;
    for (; q != NULL; FORWARD (q)) {
      if (MULTIPLE (q) != NULL) {
        ;
      } else if (WHETHER (q, REF_SYMBOL)) {
        if (MULTIPLE (SUB (q)) != NULL) {
          (*mods)++;
          MULTIPLE (q) = make_name_struct (MULTIPLE (SUB (q)), top);
        }
      } else if (WHETHER (q, ROW_SYMBOL)) {
        if (WHETHER (SUB (q), STRUCT_SYMBOL)) {
          z = A68_TRUE;
          (*mods)++;
          MULTIPLE (q) = make_multiple_struct (SUB (q), top, DIM (q));
        }
      } else if (WHETHER (q, FLEX_SYMBOL)) {
        if (SUB (SUB (q)) == NULL) {
          (*mods)++;            /* as yet unresolved FLEX INDICANT. */
        } else {
          if (WHETHER (SUB (SUB (q)), STRUCT_SYMBOL)) {
            z = A68_TRUE;
            (*mods)++;
            MULTIPLE (q) = make_flex_multiple_struct (SUB (SUB (q)), top, DIM (SUB (q)));
          }
        }
      }
    }
  }
}

/*
Deflexing removes all FLEX from a mode,
for instance REF STRING -> REF [] CHAR.
*/

/*!
\brief whether mode has flex 2
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_flex_2 (MOID_T * m)
{
  if (whether_postulated (top_postulate, m)) {
    return (A68_FALSE);
  } else {
    make_postulate (&top_postulate, m, NULL);
    if (WHETHER (m, FLEX_SYMBOL)) {
      return (A68_TRUE);
    } else if (WHETHER (m, REF_SYMBOL)) {
      return (whether_mode_has_flex_2 (SUB (m)));
    } else if (WHETHER (m, PROC_SYMBOL)) {
      return (whether_mode_has_flex_2 (SUB (m)));
    } else if (WHETHER (m, ROW_SYMBOL)) {
      return (whether_mode_has_flex_2 (SUB (m)));
    } else if (WHETHER (m, STRUCT_SYMBOL)) {
      PACK_T *t = PACK (m);
      BOOL_T z = A68_FALSE;
      for (; t != NULL && !z; FORWARD (t)) {
        z |= whether_mode_has_flex_2 (MOID (t));
      }
      return (z);
    } else {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether mode has flex
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_flex (MOID_T * m)
{
  free_postulate_list (top_postulate, NULL);
  top_postulate = NULL;
  return (whether_mode_has_flex_2 (m));
}

/*!
\brief make deflexed pack
\param src source pack
\param dst destination pack with REF modes
\param p chain to insert new modes into
**/

static void make_deflexed_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p)
{
  if (src != NULL) {
    make_deflexed_pack (NEXT (src), dst, p);
    (void) add_mode_to_pack (dst, make_deflexed (MOID (src), p), TEXT (src), NODE (src));
  }
}

/*!
\brief make deflexed
\param m structured mode
\param p chain to insert new modes into
\return mode table entry
**/

static MOID_T *make_deflexed (MOID_T * m, MOID_T ** p)
{
  if (DEFLEXED (m) != NULL) {   /* Keep this condition on top. */
    return (DEFLEXED (m));
  }
  if (WHETHER (m, REF_SYMBOL)) {
    MOID_T *new_one = make_deflexed (SUB (m), p);
    (void) add_mode (p, REF_SYMBOL, DIM (m), NULL, new_one, NULL);
    SUB (*p) = new_one;
    return (DEFLEXED (m) = *p);
  } else if (WHETHER (m, PROC_SYMBOL)) {
    MOID_T *save, *new_one;
    (void) add_mode (p, PROC_SYMBOL, DIM (m), NULL, NULL, PACK (m));
    save = *p;
/* Mark to prevent eventual cyclic references. */
    DEFLEXED (m) = save;
    new_one = make_deflexed (SUB (m), p);
    SUB (save) = new_one;
    return (save);
  } else if (WHETHER (m, FLEX_SYMBOL)) {
    ABNORMAL_END (SUB (m) == NULL, "NULL mode while deflexing", NULL);
    DEFLEXED (m) = make_deflexed (SUB (m), p);
    return (DEFLEXED (m));
  } else if (WHETHER (m, ROW_SYMBOL)) {
    MOID_T *new_sub, *new_slice;
    if (DIM (m) > 1) {
      new_slice = make_deflexed (SLICE (m), p);
      (void) add_mode (p, ROW_SYMBOL, DIM (m) - 1, NULL, new_slice, NULL);
      new_sub = make_deflexed (SUB (m), p);
    } else {
      new_sub = make_deflexed (SUB (m), p);
      new_slice = new_sub;
    }
    (void) add_mode (p, ROW_SYMBOL, DIM (m), NULL, new_sub, NULL);
    SLICE (*p) = new_slice;
    return (DEFLEXED (m) = *p);
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
    MOID_T *save;
    PACK_T *u = NULL;
    (void) add_mode (p, STRUCT_SYMBOL, DIM (m), NULL, NULL, NULL);
    save = *p;
/* Mark to prevent eventual cyclic references. */
    DEFLEXED (m) = save;
    make_deflexed_pack (PACK (m), &u, p);
    PACK (save) = u;
    return (save);
  } else if (WHETHER (m, INDICANT)) {
    MOID_T *n = EQUIVALENT (m);
    ABNORMAL_END (n == NULL, "NULL equivalent mode while deflexing", NULL);
    return (DEFLEXED (m) = make_deflexed (n, p));
  } else if (WHETHER (m, STANDARD)) {
    MOID_T *n = (DEFLEXED (m) != NULL ? DEFLEXED (m) : m);
    return (n);
  } else {
    return (m);
  }
}

/*!
\brief make deflexed modes
\param p position in tree
\param mods modification count
**/

static void make_deflexed_modes_tree (NODE_T * p, int *mods)
{
  for (; p != NULL; FORWARD (p)) {
/* Dive into lexical levels. */
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *s = SYMBOL_TABLE (SUB (p));
      MOID_T *m, **top = &s->moids;
      for (m = s->moids; m != NULL; FORWARD (m)) {
/* 'Complete' deflexing. */
        if (!m->has_flex) {
          m->has_flex = whether_mode_has_flex (m);
        }
        if (m->has_flex && DEFLEXED (m) == NULL) {
          (*mods)++;
          DEFLEXED (m) = make_deflexed (m, top);
          ABNORMAL_END (whether_mode_has_flex (DEFLEXED (m)), "deflexing failed", moid_to_string (DEFLEXED (m), MOID_WIDTH, NULL));
        }
/* 'Light' deflexing needed for trims. */
        if (TRIM (m) == NULL && WHETHER (m, FLEX_SYMBOL)) {
          (*mods)++;
          TRIM (m) = SUB (m);
        } else if (TRIM (m) == NULL && WHETHER (m, REF_SYMBOL) && WHETHER (SUB (m), FLEX_SYMBOL)) {
          (*mods)++;
          (void) add_mode (top, REF_SYMBOL, DIM (m), NULL, SUB (SUB (m)), NULL);
          TRIM (m) = *top;
        }
      }
    }
    make_deflexed_modes_tree (SUB (p), mods);
  }
}

/*!
\brief make extra rows local, rows with one extra dimension
\param s symbol table
**/

static void make_extra_rows_local (SYMBOL_TABLE_T * s)
{
  MOID_T *m, **top = &(s->moids);
  for (m = s->moids; m != NULL; FORWARD (m)) {
    if (WHETHER (m, ROW_SYMBOL) && DIM (m) > 0 && SUB (m) != NULL) {
      (void) add_row (top, DIM (m) + 1, SUB (m), NODE (m));
    } else if (WHETHER (m, REF_SYMBOL) && WHETHER (SUB (m), ROW_SYMBOL)) {
      MOID_T *z = add_row (top, DIM (SUB (m)) + 1, SUB (SUB (m)), NODE (SUB (m)));
      MOID_T *y = add_mode (top, REF_SYMBOL, 0, NODE (m), z, NULL);
      NAME (y) = m;
    }
  }
}

/*!
\brief make extra rows
\param p position in tree
**/

static void make_extra_rows_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
/* Dive into lexical levels. */
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      make_extra_rows_local (SYMBOL_TABLE (SUB (p)));
    }
    make_extra_rows_tree (SUB (p));
  }
}

/*!
\brief whether mode has ref 2
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_ref_2 (MOID_T * m)
{
  if (whether_postulated (top_postulate, m)) {
    return (A68_FALSE);
  } else {
    make_postulate (&top_postulate, m, NULL);
    if (WHETHER (m, FLEX_SYMBOL)) {
      return (whether_mode_has_ref_2 (SUB (m)));
    } else if (WHETHER (m, REF_SYMBOL)) {
      return (A68_TRUE);
    } else if (WHETHER (m, ROW_SYMBOL)) {
      return (whether_mode_has_ref_2 (SUB (m)));
    } else if (WHETHER (m, STRUCT_SYMBOL)) {
      PACK_T *t = PACK (m);
      BOOL_T z = A68_FALSE;
      for (; t != NULL && !z; FORWARD (t)) {
        z |= whether_mode_has_ref_2 (MOID (t));
      }
      return (z);
    } else {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether mode has ref
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_ref (MOID_T * m)
{
  free_postulate_list (top_postulate, NULL);
  top_postulate = NULL;
  return (whether_mode_has_ref_2 (m));
}

/* Routines setting properties of modes. */

/*!
\brief reset moid
\param p position in tree
**/

static void reset_moid_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    MOID (p) = NULL;
    reset_moid_tree (SUB (p));
  }
}

/*!
\brief renumber moids
\param p moid list
\return moids renumbered
**/

static int renumber_moids (MOID_LIST_T * p)
{
  if (p == NULL) {
    return (1);
  } else {
    return (1 + (NUMBER (MOID (p)) = renumber_moids (NEXT (p))));
  }
}

/*!
\brief whether mode has row
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_row (MOID_T * m)
{
  if (WHETHER (m, STRUCT_SYMBOL) || WHETHER (m, UNION_SYMBOL)) {
    BOOL_T k = A68_FALSE;
    PACK_T *p = PACK (m);
    for (; p != NULL && k == A68_FALSE; FORWARD (p)) {
      MOID (p)->has_rows = whether_mode_has_row (MOID (p));
      k |= (MOID (p)->has_rows);
    }
    return (k);
  } else {
    return ((BOOL_T) (m->has_rows || WHETHER (m, ROW_SYMBOL) || WHETHER (m, FLEX_SYMBOL)));
  }
}

/*!
\brief mark row modes
\param p position in tree
**/

static void mark_row_modes_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
/* Dive into lexical levels. */
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m;
      for (m = SYMBOL_TABLE (SUB (p))->moids; m != NULL; FORWARD (m)) {
        m->has_rows = whether_mode_has_row (m);
      }
    }
    mark_row_modes_tree (SUB (p));
  }
}

/*!
\brief set moid attributes
\param q moid in list where to start
**/

static void set_moid_attributes (MOID_LIST_T * q)
{
  for (; q != NULL; FORWARD (q)) {
    MOID_T *z = MOID (q);
    if (z->has_ref == A68_FALSE) {
      z->has_ref = whether_mode_has_ref (z);
    }
    if (z->has_flex == A68_FALSE) {
      z->has_flex = whether_mode_has_flex (z);
    }
    if (WHETHER (z, ROW_SYMBOL) && SLICE (z) != NULL) {
      ROWED (SLICE (z)) = z;
      track_equivalent_modes (&(ROWED (SLICE (z))));
    }
    if (WHETHER (z, REF_SYMBOL)) {
      MOID_T *y = SUB (z);
      if (SLICE (y) != NULL && WHETHER (SLICE (y), ROW_SYMBOL) && NAME (z) != NULL) {
        ROWED (NAME (z)) = z;
        track_equivalent_modes (&(ROWED (NAME (z))));
      }
    }
  }
}

/*!
\brief get moid list
\param top_moid_list top of moid list
\param top_node top node in tree
**/

void get_moid_list (MOID_LIST_T ** loc_top_moid_list, NODE_T * top_node)
{
  reset_moid_list ();
  add_moids_from_table (loc_top_moid_list, stand_env);
  add_moids_from_table_tree (top_node, loc_top_moid_list);
}

/*!
\brief construct moid list by expansion and contraction
\param top_node top node in tree
\param cycle_no cycle number
\return number of modifications
**/

static int expand_contract_moids (NODE_T * top_node, int cycle_no)
{
  int mods = 0;
  free_postulate_list (top_postulate, NULL);
  top_postulate = NULL;
  if (cycle_no >= 0) {          /* Experimental */
/* Calculate derived modes. */
    make_multiple_modes_standenv (&mods);
    absorb_unions_tree (top_node, &mods);
    contract_unions_tree (top_node, &mods);
    make_multiple_modes_tree (top_node, &mods);
    make_stowed_names_tree (top_node, &mods);
    make_deflexed_modes_tree (top_node, &mods);
  }
/* Calculate equivalent modes. */
  get_moid_list (&top_moid_list, top_node);
  bind_indicants_to_modes_tree (top_node);
  free_postulate_list (top_postulate, NULL);
  top_postulate = NULL;
  find_equivalent_moids (top_moid_list, NULL);
  track_equivalent_tree (top_node);
  track_equivalent_tags (stand_env->indicants);
  track_equivalent_tags (stand_env->identifiers);
  track_equivalent_tags (stand_env->operators);
  moid_list_track_equivalent (stand_env->moids);
  contract_unions_tree (top_node, &mods);
  set_moid_attributes (top_moid_list);
  track_equivalent_tree (top_node);
  track_equivalent_tags (stand_env->indicants);
  track_equivalent_tags (stand_env->identifiers);
  track_equivalent_tags (stand_env->operators);
  set_moid_sizes (top_moid_list);
  return (mods);
}

/*!
\brief maintain mode table
\param p position in tree
**/

void maintain_mode_table (NODE_T * p)
{
  (void) p;
  (void) renumber_moids (top_moid_list);
}

/*!
\brief make list of all modes in the program
\param top_node top node in tree
**/

void set_up_mode_table (NODE_T * top_node)
{
  reset_moid_tree (top_node);
  get_modes_from_tree (top_node, NULL_ATTRIBUTE);
  get_mode_from_proc_var_declarations_tree (top_node);
  make_extra_rows_local (stand_env);
  make_extra_rows_tree (top_node);
/* Tie MODE declarations to their respective a68_modes ... */
  bind_indicants_to_tags_tree (top_node);
  bind_indicants_to_modes_tree (top_node);
/* ... and check for cyclic definitions as MODE A = B, B = C, C = A. */
  check_cyclic_modes_tree (top_node);
  check_flex_modes_tree (top_node);
  if (a68_prog.error_count == 0) {
/* Check yin-yang of modes. */
    free_postulate_list (top_postulate, NULL);
    top_postulate = NULL;
    check_well_formedness_tree (top_node);
/* Construct the full moid list. */
    if (a68_prog.error_count == 0) {
      int cycle = 0;
      track_equivalent_standard_modes ();
      while (expand_contract_moids (top_node, cycle) > 0 || cycle < 16) {
        ABNORMAL_END (cycle++ > 32, "apparently indefinite loop in set_up_mode_table", NULL);
      }
/* Set standard modes. */
      track_equivalent_standard_modes ();
/* Postlude. */
      check_relation_to_void_tree (top_node);
      mark_row_modes_tree (top_node);
    }
  }
  init_postulates ();
}

/* Next are routines to calculate the size of a mode. */

/*!
\brief reset max simplout size
**/

void reset_max_simplout_size (void)
{
  max_simplout_size = 0;
}

/*!
\brief max unitings to simplout
\param p position in tree
\param max maximum calculated moid size
**/

static void max_unitings_to_simplout (NODE_T * p, int *max)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNITING) && MOID (p) == MODE (SIMPLOUT)) {
      MOID_T *q = MOID (SUB (p));
      if (q != MODE (SIMPLOUT)) {
        int size = moid_size (q);
        if (size > *max) {
          *max = size;
        }
      }
    }
    max_unitings_to_simplout (SUB (p), max);
  }
}

/*!
\brief get max simplout size
\param p position in tree
**/

void get_max_simplout_size (NODE_T * p)
{
  max_simplout_size = 0;
  max_unitings_to_simplout (p, &max_simplout_size);
}

/*!
\brief set moid sizes
\param start moid to start from
**/

void set_moid_sizes (MOID_LIST_T * start)
{
  for (; start != NULL; FORWARD (start)) {
    SIZE (MOID (start)) = moid_size (MOID (start));
  }
}

/*!
\brief moid size 2
\param p moid to calculate
\return moid size
**/

static int moid_size_2 (MOID_T * p)
{
  if (p == NULL) {
    return (0);
  } else if (EQUIVALENT (p) != NULL) {
    return (moid_size_2 (EQUIVALENT (p)));
  } else if (p == MODE (HIP)) {
    return (0);
  } else if (p == MODE (VOID)) {
    return (0);
  } else if (p == MODE (INT)) {
    return (ALIGNED_SIZE_OF (A68_INT));
  } else if (p == MODE (LONG_INT)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_INT)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (REAL)) {
    return (ALIGNED_SIZE_OF (A68_REAL));
  } else if (p == MODE (LONG_REAL)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_REAL)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (BOOL)) {
    return (ALIGNED_SIZE_OF (A68_BOOL));
  } else if (p == MODE (CHAR)) {
    return (ALIGNED_SIZE_OF (A68_CHAR));
  } else if (p == MODE (ROW_CHAR)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (BITS)) {
    return (ALIGNED_SIZE_OF (A68_BITS));
  } else if (p == MODE (LONG_BITS)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_BITS)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (BYTES)) {
    return (ALIGNED_SIZE_OF (A68_BYTES));
  } else if (p == MODE (LONG_BYTES)) {
    return (ALIGNED_SIZE_OF (A68_LONG_BYTES));
  } else if (p == MODE (FILE)) {
    return (ALIGNED_SIZE_OF (A68_FILE));
  } else if (p == MODE (CHANNEL)) {
    return (ALIGNED_SIZE_OF (A68_CHANNEL));
  } else if (p == MODE (FORMAT)) {
    return (ALIGNED_SIZE_OF (A68_FORMAT));
  } else if (p == MODE (SEMA)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (SOUND)) {
    return (ALIGNED_SIZE_OF (A68_SOUND));
  } else if (p == MODE (COLLITEM)) {
    return (ALIGNED_SIZE_OF (A68_COLLITEM));
  } else if (p == MODE (NUMBER)) {
    int k = 0;
    if (ALIGNED_SIZE_OF (A68_INT) > k) {
      k = ALIGNED_SIZE_OF (A68_INT);
    }
    if ((int) size_long_mp () > k) {
      k = (int) size_long_mp ();
    }
    if ((int) size_longlong_mp () > k) {
      k = (int) size_longlong_mp ();
    }
    if (ALIGNED_SIZE_OF (A68_REAL) > k) {
      k = ALIGNED_SIZE_OF (A68_REAL);
    }
    if ((int) size_long_mp () > k) {
      k = (int) size_long_mp ();
    }
    if ((int) size_longlong_mp () > k) {
      k = (int) size_longlong_mp ();
    }
    if (ALIGNED_SIZE_OF (A68_REF) > k) {
      k = ALIGNED_SIZE_OF (A68_REF);
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + k);
  } else if (p == MODE (SIMPLIN)) {
    int k = 0;
    if (ALIGNED_SIZE_OF (A68_REF) > k) {
      k = ALIGNED_SIZE_OF (A68_REF);
    }
    if (ALIGNED_SIZE_OF (A68_FORMAT) > k) {
      k = ALIGNED_SIZE_OF (A68_FORMAT);
    }
    if (ALIGNED_SIZE_OF (A68_PROCEDURE) > k) {
      k = ALIGNED_SIZE_OF (A68_PROCEDURE);
    }
    if (ALIGNED_SIZE_OF (A68_SOUND) > k) {
      k = ALIGNED_SIZE_OF (A68_SOUND);
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + k);
  } else if (p == MODE (SIMPLOUT)) {
    return (ALIGNED_SIZE_OF (A68_UNION) + max_simplout_size);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (WHETHER (p, PROC_SYMBOL)) {
    return (ALIGNED_SIZE_OF (A68_PROCEDURE));
  } else if (WHETHER (p, ROW_SYMBOL) && p != MODE (ROWS)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (ROWS)) {
    return (ALIGNED_SIZE_OF (A68_UNION) + ALIGNED_SIZE_OF (A68_REF));
  } else if (WHETHER (p, FLEX_SYMBOL)) {
    return moid_size (SUB (p));
  } else if (WHETHER (p, STRUCT_SYMBOL)) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NULL; FORWARD (z)) {
      size += moid_size (MOID (z));
    }
    return (size);
  } else if (WHETHER (p, UNION_SYMBOL)) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NULL; FORWARD (z)) {
      if (moid_size (MOID (z)) > size) {
        size = moid_size (MOID (z));
      }
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + size);
  } else if (PACK (p) != NULL) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NULL; FORWARD (z)) {
      size += moid_size (MOID (z));
    }
    return (size);
  } else {
/* ? */
    return (0);
  }
}

/*!
\brief moid size
\param p moid to set size
\return moid size
**/

int moid_size (MOID_T * p)
{
  SIZE (p) = moid_size_2 (p);
  return (SIZE (p));
}

/* A pretty printer for moids. */

/*!
\brief moid to string 3
\param dst text buffer
\param str string to concatenate
\param w estimated width
\param idf print indicants if one exists in this range
**/

static void add_to_moid_text (char *dst, char *str, int *w)
{
  bufcat (dst, str, BUFFER_SIZE);
  (*w) -= (int) strlen (str);
}

/*!
\brief find a tag, searching symbol tables towards the root
\param table symbol table to search
\param mode mode of the tag
\return entry in symbol table
**/

TAG_T *find_indicant_global (SYMBOL_TABLE_T * table, MOID_T * mode)
{
  if (table != NULL) {
    TAG_T *s;
    for (s = table->indicants; s != NULL; FORWARD (s)) {
      if (MOID (s) == mode) {
        return (s);
      }
    }
    return (find_indicant_global (PREVIOUS (table), mode));
  } else {
    return (NULL);
  }
}

/*!
\brief pack to string
\param b text buffer
\param p pack
\param w estimated width
\param text include field names
\param idf print indicants if one exists in this range
**/

static void pack_to_string (char *b, PACK_T * p, int *w, BOOL_T text, NODE_T * idf)
{
  for (; p != NULL; FORWARD (p)) {
    moid_to_string_2 (b, MOID (p), w, idf);
    if (text) {
      if (TEXT (p) != NULL) {
        add_to_moid_text (b, " ", w);
        add_to_moid_text (b, TEXT (p), w);
      }
    }
    if (p != NULL && NEXT (p) != NULL) {
      add_to_moid_text (b, ", ", w);
    }
  }
}

/*!
\brief moid to string 2
\param b text buffer
\param n moid
\param w estimated width
\param idf print indicants if one exists in this range
**/

static void moid_to_string_2 (char *b, MOID_T * n, int *w, NODE_T * idf)
{
/* Oops. Should not happen */
  if (n == NULL) {
    add_to_moid_text (b, "NULL", w);;
    return;
  }
/* Reference to self through REF or PROC */
  if (whether_postulated (postulates, n)) {
    add_to_moid_text (b, "SELF", w);
    return;
  }
/* If declared by a mode-declaration, present the indicant */
  if (idf != NULL) {
    TAG_T *indy = find_indicant_global (SYMBOL_TABLE (idf), n);
    if (indy != NULL) {
      add_to_moid_text (b, SYMBOL (NODE (indy)), w);
      return;
    }
  }
/* Write the standard modes */
  if (n == MODE (HIP)) {
    add_to_moid_text (b, "HIP", w);
  } else if (n == MODE (ERROR)) {
    add_to_moid_text (b, "ERROR", w);
  } else if (n == MODE (UNDEFINED)) {
    add_to_moid_text (b, "unresolved", w);
  } else if (n == MODE (C_STRING)) {
    add_to_moid_text (b, "C-STRING", w);
  } else if (n == MODE (COMPLEX) || n == MODE (COMPL)) {
    add_to_moid_text (b, "COMPLEX", w);
  } else if (n == MODE (LONG_COMPLEX) || n == MODE (LONG_COMPL)) {
    add_to_moid_text (b, "LONG COMPLEX", w);
  } else if (n == MODE (LONGLONG_COMPLEX) || n == MODE (LONGLONG_COMPL)) {
    add_to_moid_text (b, "LONG LONG COMPLEX", w);
  } else if (n == MODE (STRING)) {
    add_to_moid_text (b, "STRING", w);
  } else if (n == MODE (PIPE)) {
    add_to_moid_text (b, "PIPE", w);
  } else if (n == MODE (SOUND)) {
    add_to_moid_text (b, "SOUND", w);
  } else if (n == MODE (COLLITEM)) {
    add_to_moid_text (b, "COLLITEM", w);
  } else if (WHETHER (n, IN_TYPE_MODE)) {
    add_to_moid_text (b, "\"SIMPLIN\"", w);
  } else if (WHETHER (n, OUT_TYPE_MODE)) {
    add_to_moid_text (b, "\"SIMPLOUT\"", w);
  } else if (WHETHER (n, ROWS_SYMBOL)) {
    add_to_moid_text (b, "\"ROWS\"", w);
  } else if (n == MODE (VACUUM)) {
    add_to_moid_text (b, "\"VACUUM\"", w);
  } else if (WHETHER (n, VOID_SYMBOL) || WHETHER (n, STANDARD) || WHETHER (n, INDICANT)) {
    if (DIM (n) > 0) {
      int k = DIM (n);
      if ((*w) >= k * (int) strlen ("LONG ") + (int) strlen (SYMBOL (NODE (n)))) {
        while (k --) {
          add_to_moid_text (b, "LONG ", w);
        }
        add_to_moid_text (b, SYMBOL (NODE (n)), w);
      } else {
        add_to_moid_text (b, "..", w);
      }
    } else if (DIM (n) < 0) {
      int k = -DIM (n);
      if ((*w) >= k * (int) strlen ("LONG ") + (int) strlen (SYMBOL (NODE (n)))) {
        while (k --) {
          add_to_moid_text (b, "LONG ", w);
        }
        add_to_moid_text (b, SYMBOL (NODE (n)), w);
      } else {
        add_to_moid_text (b, "..", w);
      }
    } else if (DIM (n) == 0) {
      add_to_moid_text (b, SYMBOL (NODE (n)), w);
    }
/* Write compounded modes */
  } else if (WHETHER (n, REF_SYMBOL)) {
    if ((*w) >= (int) strlen ("REF ..")) {
      add_to_moid_text (b, "REF ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "REF ..", w);
    }
  } else if (WHETHER (n, FLEX_SYMBOL)) {
    if ((*w) >= (int) strlen ("FLEX ..")) {
      add_to_moid_text (b, "FLEX ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "FLEX ..", w);
    }
  } else if (WHETHER (n, ROW_SYMBOL)) {
    int j = (int) strlen ("[] ..") + (DIM (n) - 1) * (int) strlen (",");
    if ((*w) >= j) {
      int k = DIM (n) - 1;
      add_to_moid_text (b, "[", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, "] ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else if (DIM (n) == 1) {
      add_to_moid_text (b, "[] ..", w);
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "[", w);
      while (k--) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, "] ..", w);
    }
  } else if (WHETHER (n, STRUCT_SYMBOL)) {
    int j = (int) strlen ("STRUCT ()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NULL);
      add_to_moid_text (b, "STRUCT (", w);
      pack_to_string (b, PACK (n), w, A68_TRUE, idf);
      add_to_moid_text (b, ")", w);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "STRUCT (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else if (WHETHER (n, UNION_SYMBOL)) {
    int j = (int) strlen ("UNION ()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NULL);
      add_to_moid_text (b, "UNION (", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ")", w);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "UNION (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else if (WHETHER (n, PROC_SYMBOL) && DIM (n) == 0) {
    if ((*w) >= (int) strlen ("PROC ..")) {
      add_to_moid_text (b, "PROC ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "PROC ..", w);
    }
  } else if (WHETHER (n, PROC_SYMBOL) && DIM (n) > 0) {
    int j = (int) strlen ("PROC () ..") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NULL);
      add_to_moid_text (b, "PROC (", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ") ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "PROC (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ") ..", w);
    }
  } else if (WHETHER (n, SERIES_MODE) || WHETHER (n, STOWED_MODE)) {
    int j = (int) strlen ("()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      add_to_moid_text (b, "(", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ")", w);
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "(", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else {
    char str[SMALL_BUFFER_SIZE];
    CHECK_RETVAL (snprintf (str, (size_t) SMALL_BUFFER_SIZE, "\\%d", ATTRIBUTE (n)) >= 0);
    add_to_moid_text (b, str, w);
  }
}

/*!
\brief pretty-formatted mode "n"; "w" is a measure of width
\param n moid
\param w estimated width; if w is exceeded, modes are abbreviated
\return text buffer
**/

char *moid_to_string (MOID_T * n, int w, NODE_T * idf)
{
  char a[BUFFER_SIZE];
  a[0] = NULL_CHAR;
  if (w >= BUFFER_SIZE) {
    w = BUFFER_SIZE - 1;
  }
  postulates = NULL;
  if (n != NULL) {
    moid_to_string_2 (a, n, &w, idf);
  } else {
    bufcat (a, "NULL", BUFFER_SIZE);
  }
  return (new_string (a));
}
