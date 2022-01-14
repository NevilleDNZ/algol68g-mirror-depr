//! @file taxes.c
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

#include "a68g.h"
#include "a68g-postulates.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"

// Mode collection, equivalencing and derived modes.

// Mode service routines.

//! @brief Count bounds in declarer in tree.

static int count_bounds (NODE_T * p)
{
  if (p == NO_NODE) {
    return 0;
  } else {
    if (IS (p, BOUND)) {
      return 1 + count_bounds (NEXT (p));
    } else {
      return count_bounds (NEXT (p)) + count_bounds (SUB (p));
    }
  }
}

//! @brief Count number of SHORTs or LONGs.

static int count_sizety (NODE_T * p)
{
  if (p == NO_NODE) {
    return 0;
  } else if (IS (p, LONGETY)) {
    return count_sizety (SUB (p)) + count_sizety (NEXT (p));
  } else if (IS (p, SHORTETY)) {
    return count_sizety (SUB (p)) + count_sizety (NEXT (p));
  } else if (IS (p, LONG_SYMBOL)) {
    return 1;
  } else if (IS (p, SHORT_SYMBOL)) {
    return -1;
  } else {
    return 0;
  }
}

//! @brief Count moids in a pack.

int count_pack_members (PACK_T * u)
{
  int k = 0;
  for (; u != NO_PACK; FORWARD (u)) {
    k++;
  }
  return k;
}

//! @brief Replace a mode by its equivalent mode.

static void resolve_equivalent (MOID_T ** m)
{
  while ((*m) != NO_MOID && EQUIVALENT ((*m)) != NO_MOID && (*m) != EQUIVALENT (*m)) {
    (*m) = EQUIVALENT (*m);
  }
}

//! @brief Reset moid.

static void reset_moid_tree (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    MOID (p) = NO_MOID;
    reset_moid_tree (SUB (p));
  }
}

//! @brief Renumber moids.

void renumber_moids (MOID_T * p, int n)
{
  if (p != NO_MOID) {
    NUMBER (p) = n;
    renumber_moids (NEXT (p), n + 1);
  }
}

//! @brief Register mode in the global mode table, if mode is unique.

MOID_T *register_extra_mode (MOID_T ** z, MOID_T * u)
{
  MOID_T *head = TOP_MOID (&A68_JOB);
// If we already know this mode, return the existing entry; otherwise link it in.
  for (; head != NO_MOID; FORWARD (head)) {
    if (prove_moid_equivalence (head, u)) {
      return head;
    }
  }
// Link to chain and exit.
  NUMBER (u) = A68 (mode_count)++;
  NEXT (u) = (*z);
  return *z = u;
}

//! @brief Add mode "sub" to chain "z".

MOID_T *add_mode (MOID_T ** z, int att, int dim, NODE_T * node, MOID_T * sub, PACK_T * pack)
{
  MOID_T *new_mode = new_moid ();
  if (sub == NO_MOID) {
    ABEND (att == REF_SYMBOL, ERROR_INTERNAL_CONSISTENCY, __func__);
    ABEND (att == FLEX_SYMBOL, ERROR_INTERNAL_CONSISTENCY, __func__);
    ABEND (att == ROW_SYMBOL, ERROR_INTERNAL_CONSISTENCY, __func__);
  }
  USE (new_mode) = A68_FALSE;
  SIZE (new_mode) = 0;
  ATTRIBUTE (new_mode) = att;
  DIM (new_mode) = dim;
  NODE (new_mode) = node;
  HAS_ROWS (new_mode) = (BOOL_T) (att == ROW_SYMBOL);
  SUB (new_mode) = sub;
  PACK (new_mode) = pack;
  NEXT (new_mode) = NO_MOID;
  EQUIVALENT (new_mode) = NO_MOID;
  SLICE (new_mode) = NO_MOID;
  DEFLEXED (new_mode) = NO_MOID;
  NAME (new_mode) = NO_MOID;
  MULTIPLE (new_mode) = NO_MOID;
  ROWED (new_mode) = NO_MOID;
  return register_extra_mode (z, new_mode);
}

//! @brief Contract a UNION.

void contract_union (MOID_T * u)
{
  PACK_T *s = PACK (u);
  for (; s != NO_PACK; FORWARD (s)) {
    PACK_T *t = s;
    while (t != NO_PACK) {
      if (NEXT (t) != NO_PACK && MOID (NEXT (t)) == MOID (s)) {
        MOID (t) = MOID (t);
        NEXT (t) = NEXT_NEXT (t);
      } else {
        FORWARD (t);
      }
    }
  }
}

//! @brief Absorb UNION pack.

PACK_T *absorb_union_pack (PACK_T * u)
{
  BOOL_T go_on;
  PACK_T *t, *z;
  do {
    z = NO_PACK;
    go_on = A68_FALSE;
    for (t = u; t != NO_PACK; FORWARD (t)) {
      if (IS (MOID (t), UNION_SYMBOL)) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NO_PACK; FORWARD (s)) {
          (void) add_mode_to_pack (&z, MOID (s), NO_TEXT, NODE (s));
        }
      } else {
        (void) add_mode_to_pack (&z, MOID (t), NO_TEXT, NODE (t));
      }
    }
    u = z;
  } while (go_on);
  return z;
}

//! @brief Add row and its slices to chain, recursively.

static MOID_T *add_row (MOID_T ** p, int dim, MOID_T * sub, NODE_T * n, BOOL_T derivate)
{
  MOID_T *q = add_mode (p, ROW_SYMBOL, dim, n, sub, NO_PACK);
  DERIVATE (q) |= derivate;
  if (dim > 1) {
    SLICE (q) = add_row (&NEXT (q), dim - 1, sub, n, derivate);
  } else {
    SLICE (q) = sub;
  }
  return q;
}

//! @brief Add a moid to a pack, maybe with a (field) name.

void add_mode_to_pack (PACK_T ** p, MOID_T * m, char *text, NODE_T * node)
{
  PACK_T *z = new_pack ();
  MOID (z) = m;
  TEXT (z) = text;
  NODE (z) = node;
  NEXT (z) = *p;
  PREVIOUS (z) = NO_PACK;
  if (NEXT (z) != NO_PACK) {
    PREVIOUS (NEXT (z)) = z;
  }
// Link in chain.
  *p = z;
}

//! @brief Add a moid to a pack, maybe with a (field) name.

void add_mode_to_pack_end (PACK_T ** p, MOID_T * m, char *text, NODE_T * node)
{
  PACK_T *z = new_pack ();
  MOID (z) = m;
  TEXT (z) = text;
  NODE (z) = node;
  NEXT (z) = NO_PACK;
  if (NEXT (z) != NO_PACK) {
    PREVIOUS (NEXT (z)) = z;
  }
// Link in chain.
  while ((*p) != NO_PACK) {
    p = &(NEXT (*p));
  }
  PREVIOUS (z) = (*p);
  (*p) = z;
}

//! @brief Absorb UNION members.

static void absorb_unions (MOID_T * m)
{
// UNION (A, UNION (B, C)) = UNION (A, B, C) or
// UNION (A, UNION (A, B)) = UNION (A, B).
  for (; m != NO_MOID; FORWARD (m)) {
    if (IS (m, UNION_SYMBOL)) {
      PACK (m) = absorb_union_pack (PACK (m));
    }
  }
}

//! @brief Contract UNIONs .

static void contract_unions (MOID_T * m)
{
// UNION (A, B, A) -> UNION (A, B).
  for (; m != NO_MOID; FORWARD (m)) {
    if (IS (m, UNION_SYMBOL) && EQUIVALENT (m) == NO_MOID) {
      contract_union (m);
    }
  }
}

// Routines to collect MOIDs from the program text.

//! @brief Search standard mode in standard environ.

static MOID_T *search_standard_mode (int sizety, NODE_T * indicant)
{
  MOID_T *p = TOP_MOID (&A68_JOB);
// Search standard mode.
  for (; p != NO_MOID; FORWARD (p)) {
    if (IS (p, STANDARD) && DIM (p) == sizety && NSYMBOL (NODE (p)) == NSYMBOL (indicant)) {
      return p;
    }
  }
// Sanity check
//if (sizety == -2 || sizety == 2) {
//  return NO_MOID;
//}
// Map onto greater precision.
  if (sizety < 0) {
    return search_standard_mode (sizety + 1, indicant);
  } else if (sizety > 0) {
    return search_standard_mode (sizety - 1, indicant);
  } else {
    return NO_MOID;
  }
}

//! @brief Collect mode from STRUCT field.

static void get_mode_from_struct_field (NODE_T * p, PACK_T ** u)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTIFIER)) {
      ATTRIBUTE (p) = FIELD_IDENTIFIER;
      (void) add_mode_to_pack (u, NO_MOID, NSYMBOL (p), p);
    } else if (IS (p, DECLARER)) {
      MOID_T *new_one = get_mode_from_declarer (p);
      PACK_T *t;
      get_mode_from_struct_field (NEXT (p), u);
      for (t = *u; t && MOID (t) == NO_MOID; FORWARD (t)) {
        MOID (t) = new_one;
        MOID (NODE (t)) = new_one;
      }
    } else {
      get_mode_from_struct_field (NEXT (p), u);
      get_mode_from_struct_field (SUB (p), u);
    }
  }
}

//! @brief Collect MODE from formal pack.

static void get_mode_from_formal_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NO_NODE) {
    if (IS (p, DECLARER)) {
      MOID_T *z;
      get_mode_from_formal_pack (NEXT (p), u);
      z = get_mode_from_declarer (p);
      (void) add_mode_to_pack (u, z, NO_TEXT, p);
    } else {
      get_mode_from_formal_pack (NEXT (p), u);
      get_mode_from_formal_pack (SUB (p), u);
    }
  }
}

//! @brief Collect MODE or VOID from formal UNION pack.

static void get_mode_from_union_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NO_NODE) {
    if (IS (p, DECLARER) || IS (p, VOID_SYMBOL)) {
      MOID_T *z;
      get_mode_from_union_pack (NEXT (p), u);
      z = get_mode_from_declarer (p);
      (void) add_mode_to_pack (u, z, NO_TEXT, p);
    } else {
      get_mode_from_union_pack (NEXT (p), u);
      get_mode_from_union_pack (SUB (p), u);
    }
  }
}

//! @brief Collect mode from PROC, OP pack.

static void get_mode_from_routine_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTIFIER)) {
      (void) add_mode_to_pack (u, NO_MOID, NO_TEXT, p);
    } else if (IS (p, DECLARER)) {
      MOID_T *z = get_mode_from_declarer (p);
      PACK_T *t = *u;
      for (; t != NO_PACK && MOID (t) == NO_MOID; FORWARD (t)) {
        MOID (t) = z;
        MOID (NODE (t)) = z;
      }
      (void) add_mode_to_pack (u, z, NO_TEXT, p);
    } else {
      get_mode_from_routine_pack (NEXT (p), u);
      get_mode_from_routine_pack (SUB (p), u);
    }
  }
}

//! @brief Collect MODE from DECLARER.

MOID_T *get_mode_from_declarer (NODE_T * p)
{
  if (p == NO_NODE) {
    return NO_MOID;
  } else {
    if (IS (p, DECLARER)) {
      if (MOID (p) != NO_MOID) {
        return MOID (p);
      } else {
        return MOID (p) = get_mode_from_declarer (SUB (p));
      }
    } else {
      if (IS (p, VOID_SYMBOL)) {
        MOID (p) = M_VOID;
        return MOID (p);
      } else if (IS (p, LONGETY)) {
        if (whether (p, LONGETY, INDICANT, STOP)) {
          int k = count_sizety (SUB (p));
          MOID (p) = search_standard_mode (k, NEXT (p));
          return MOID (p);
        } else {
          return NO_MOID;
        }
      } else if (IS (p, SHORTETY)) {
        if (whether (p, SHORTETY, INDICANT, STOP)) {
          int k = count_sizety (SUB (p));
          MOID (p) = search_standard_mode (k, NEXT (p));
          return MOID (p);
        } else {
          return NO_MOID;
        }
      } else if (IS (p, INDICANT)) {
        MOID_T *q = search_standard_mode (0, p);
        if (q != NO_MOID) {
          MOID (p) = q;
        } else {
// Position of definition tells indicants apart.
          TAG_T *y = find_tag_global (TABLE (p), INDICANT, NSYMBOL (p));
          if (y == NO_TAG) {
            diagnostic (A68_ERROR, p, ERROR_UNDECLARED_TAG_2, NSYMBOL (p));
          } else {
            MOID (p) = add_mode (&TOP_MOID (&A68_JOB), INDICANT, 0, NODE (y), NO_MOID, NO_PACK);
          }
        }
        return MOID (p);
      } else if (IS_REF (p)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, p, new_one, NO_PACK);
        return MOID (p);
      } else if (IS_FLEX (p)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (&TOP_MOID (&A68_JOB), FLEX_SYMBOL, 0, p, new_one, NO_PACK);
        SLICE (MOID (p)) = SLICE (new_one);
        return MOID (p);
      } else if (IS (p, FORMAL_BOUNDS)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_row (&TOP_MOID (&A68_JOB), 1 + count_formal_bounds (SUB (p)), new_one, p, A68_FALSE);
        return MOID (p);
      } else if (IS (p, BOUNDS)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_row (&TOP_MOID (&A68_JOB), count_bounds (SUB (p)), new_one, p, A68_FALSE);
        return MOID (p);
      } else if (IS (p, STRUCT_SYMBOL)) {
        PACK_T *u = NO_PACK;
        get_mode_from_struct_field (NEXT (p), &u);
        MOID (p) = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (u), p, NO_MOID, u);
        return MOID (p);
      } else if (IS (p, UNION_SYMBOL)) {
        PACK_T *u = NO_PACK;
        get_mode_from_union_pack (NEXT (p), &u);
        MOID (p) = add_mode (&TOP_MOID (&A68_JOB), UNION_SYMBOL, count_pack_members (u), p, NO_MOID, u);
        return MOID (p);
      } else if (IS (p, PROC_SYMBOL)) {
        NODE_T *save = p;
        PACK_T *u = NO_PACK;
        MOID_T *new_one;
        if (IS (NEXT (p), FORMAL_DECLARERS)) {
          get_mode_from_formal_pack (SUB_NEXT (p), &u);
          FORWARD (p);
        }
        new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, count_pack_members (u), save, new_one, u);
        MOID (save) = MOID (p);
        return MOID (p);
      } else {
        return NO_MOID;
      }
    }
  }
}

//! @brief Collect MODEs from a routine-text header.

static MOID_T *get_mode_from_routine_text (NODE_T * p)
{
  PACK_T *u = NO_PACK;
  MOID_T *n;
  NODE_T *q = p;
  if (IS (p, PARAMETER_PACK)) {
    get_mode_from_routine_pack (SUB (p), &u);
    FORWARD (p);
  }
  n = get_mode_from_declarer (p);
  return add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, count_pack_members (u), q, n, u);
}

//! @brief Collect modes from operator-plan.

static MOID_T *get_mode_from_operator (NODE_T * p)
{
  PACK_T *u = NO_PACK;
  MOID_T *new_one;
  NODE_T *save = p;
  if (IS (NEXT (p), FORMAL_DECLARERS)) {
    get_mode_from_formal_pack (SUB_NEXT (p), &u);
    FORWARD (p);
  }
  new_one = get_mode_from_declarer (NEXT (p));
  MOID (p) = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, count_pack_members (u), save, new_one, u);
  return MOID (p);
}

//! @brief Collect mode from denotation.

static void get_mode_from_denotation (NODE_T * p, int sizety)
{
  if (p != NO_NODE) {
    if (IS (p, ROW_CHAR_DENOTATION)) {
      if (strlen (NSYMBOL (p)) == 1) {
        MOID (p) = M_CHAR;
      } else {
        MOID (p) = M_ROW_CHAR;
      }
    } else if (IS (p, TRUE_SYMBOL) || IS (p, FALSE_SYMBOL)) {
      MOID (p) = M_BOOL;
    } else if (IS (p, INT_DENOTATION)) {
      if (sizety == 0) {
        MOID (p) = M_INT;
      } else if (sizety == 1) {
        MOID (p) = M_LONG_INT;
      } else if (sizety == 2) {
        MOID (p) = M_LONG_LONG_INT;
      } else {
        MOID (p) = (sizety > 0 ? M_LONG_LONG_INT : M_INT);
      }
    } else if (IS (p, REAL_DENOTATION)) {
      if (sizety == 0) {
        MOID (p) = M_REAL;
      } else if (sizety == 1) {
        MOID (p) = M_LONG_REAL;
      } else if (sizety == 2) {
        MOID (p) = M_LONG_LONG_REAL;
      } else {
        MOID (p) = (sizety > 0 ? M_LONG_LONG_REAL : M_REAL);
      }
    } else if (IS (p, BITS_DENOTATION)) {
      if (sizety == 0) {
        MOID (p) = M_BITS;
      } else if (sizety == 1) {
        MOID (p) = M_LONG_BITS;
      } else if (sizety == 2) {
        MOID (p) = M_LONG_LONG_BITS;
      } else {
        MOID (p) = (sizety > 0 ? M_LONG_LONG_BITS : M_BITS);
      }
    } else if (IS (p, LONGETY) || IS (p, SHORTETY)) {
      get_mode_from_denotation (NEXT (p), count_sizety (SUB (p)));
      MOID (p) = MOID (NEXT (p));
    } else if (IS (p, EMPTY_SYMBOL)) {
      MOID (p) = M_VOID;
    }
  }
}

//! @brief Collect modes from the syntax tree.

static void get_modes_from_tree (NODE_T * p, int attribute)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, VOID_SYMBOL)) {
      MOID (q) = M_VOID;
    } else if (IS (q, DECLARER)) {
      if (attribute == VARIABLE_DECLARATION) {
        MOID_T *new_one = get_mode_from_declarer (q);
        MOID (q) = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, new_one, NO_PACK);
      } else {
        MOID (q) = get_mode_from_declarer (q);
      }
    } else if (IS (q, ROUTINE_TEXT)) {
      MOID (q) = get_mode_from_routine_text (SUB (q));
    } else if (IS (q, OPERATOR_PLAN)) {
      MOID (q) = get_mode_from_operator (SUB (q));
    } else if (is_one_of (q, LOC_SYMBOL, HEAP_SYMBOL, NEW_SYMBOL, STOP)) {
      if (attribute == GENERATOR) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (q));
        MOID (NEXT (q)) = new_one;
        MOID (q) = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, new_one, NO_PACK);
      }
    } else {
      if (attribute == DENOTATION) {
        get_mode_from_denotation (q, 0);
      }
    }
  }
  if (attribute != DENOTATION) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      if (SUB (q) != NO_NODE) {
        get_modes_from_tree (SUB (q), ATTRIBUTE (q));
      }
    }
  }
}

//! @brief Collect modes from proc variables.

static void get_mode_from_proc_variables (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      get_mode_from_proc_variables (SUB (p));
      get_mode_from_proc_variables (NEXT (p));
    } else if (IS (p, QUALIFIER) || IS (p, PROC_SYMBOL) || IS (p, COMMA_SYMBOL)) {
      get_mode_from_proc_variables (NEXT (p));
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      MOID_T *new_one = MOID (NEXT_NEXT (p));
      MOID (p) = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, p, new_one, NO_PACK);
    }
  }
}

//! @brief Collect modes from proc variable declarations.

static void get_mode_from_proc_var_declarations_tree (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    get_mode_from_proc_var_declarations_tree (SUB (p));
    if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      get_mode_from_proc_variables (p);
    }
  }
}

// Various routines to test modes.

//! @brief Whether a mode declaration refers to self or relates to void.

static BOOL_T is_well_formed (MOID_T * def, MOID_T * z, BOOL_T yin, BOOL_T yang, BOOL_T video)
{
  if (z == NO_MOID) {
    return A68_FALSE;
  } else if (yin && yang) {
    return z == M_VOID ? video : A68_TRUE;
  } else if (z == M_VOID) {
    return video;
  } else if (IS (z, STANDARD)) {
    return A68_TRUE;
  } else if (IS (z, INDICANT)) {
    if (def == NO_MOID) {
// Check an applied indicant for relation to VOID.
      while (z != NO_MOID) {
        z = EQUIVALENT (z);
      }
      if (z == M_VOID) {
        return video;
      } else {
        return A68_TRUE;
      }
    } else {
      if (z == def || USE (z)) {
        return yin && yang;
      } else {
        BOOL_T wwf;
        USE (z) = A68_TRUE;
        wwf = is_well_formed (def, EQUIVALENT (z), yin, yang, video);
        USE (z) = A68_FALSE;
        return wwf;
      }
    }
  } else if (IS_REF (z)) {
    return is_well_formed (def, SUB (z), A68_TRUE, yang, A68_FALSE);
  } else if (IS (z, PROC_SYMBOL)) {
    return PACK (z) != NO_PACK ? A68_TRUE : is_well_formed (def, SUB (z), A68_TRUE, yang, A68_TRUE);
  } else if (IS_ROW (z)) {
    return is_well_formed (def, SUB (z), yin, yang, A68_FALSE);
  } else if (IS_FLEX (z)) {
    return is_well_formed (def, SUB (z), yin, yang, A68_FALSE);
  } else if (IS (z, STRUCT_SYMBOL)) {
    PACK_T *s = PACK (z);
    for (; s != NO_PACK; FORWARD (s)) {
      if (!is_well_formed (def, MOID (s), yin, A68_TRUE, A68_FALSE)) {
        return A68_FALSE;
      }
    }
    return A68_TRUE;
  } else if (IS (z, UNION_SYMBOL)) {
    PACK_T *s = PACK (z);
    for (; s != NO_PACK; FORWARD (s)) {
      if (!is_well_formed (def, MOID (s), yin, yang, A68_TRUE)) {
        return A68_FALSE;
      }
    }
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Replace a mode by its equivalent mode (walk chain).

static void resolve_eq_members (MOID_T * q)
{
  PACK_T *p;
  resolve_equivalent (&SUB (q));
  resolve_equivalent (&DEFLEXED (q));
  resolve_equivalent (&MULTIPLE (q));
  resolve_equivalent (&NAME (q));
  resolve_equivalent (&SLICE (q));
  resolve_equivalent (&TRIM (q));
  resolve_equivalent (&ROWED (q));
  for (p = PACK (q); p != NO_PACK; FORWARD (p)) {
    resolve_equivalent (&MOID (p));
  }
}

//! @brief Track equivalent tags.

static void resolve_eq_tags (TAG_T * z)
{
  for (; z != NO_TAG; FORWARD (z)) {
    if (MOID (z) != NO_MOID) {
      resolve_equivalent (&MOID (z));
    }
  }
}

//! @brief Bind modes in syntax tree.

static void bind_modes (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    resolve_equivalent (&MOID (p));
    if (SUB (p) != NO_NODE && is_new_lexical_level (p)) {
      TABLE_T *s = TABLE (SUB (p));
      TAG_T *z = INDICANTS (s);
      for (; z != NO_TAG; FORWARD (z)) {
        if (NODE (z) != NO_NODE) {
          resolve_equivalent (&MOID (NEXT_NEXT (NODE (z))));
          MOID (z) = MOID (NEXT_NEXT (NODE (z)));
          MOID (NODE (z)) = MOID (z);
        }
      }
    }
    bind_modes (SUB (p));
  }
}

// Routines for calculating subordinates for selections, for instance selection
// from REF STRUCT (A) yields REF A fields and selection from [] STRUCT (A) yields
// [] A fields.

//! @brief Make name pack.

static void make_name_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p)
{
  if (src != NO_PACK) {
    MOID_T *z;
    make_name_pack (NEXT (src), dst, p);
    z = add_mode (p, REF_SYMBOL, 0, NO_NODE, MOID (src), NO_PACK);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

//! @brief Make flex multiple row pack.

static void make_flex_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NO_PACK) {
    MOID_T *z;
    make_flex_multiple_row_pack (NEXT (src), dst, p, dim);
    z = add_row (p, dim, MOID (src), NO_NODE, A68_FALSE);
    z = add_mode (p, FLEX_SYMBOL, 0, NO_NODE, z, NO_PACK);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

//! @brief Make name struct.

static MOID_T *make_name_struct (MOID_T * m, MOID_T ** p)
{
  PACK_T *u = NO_PACK;
  make_name_pack (PACK (m), &u, p);
  return add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u);
}

//! @brief Make name row.

static MOID_T *make_name_row (MOID_T * m, MOID_T ** p)
{
  if (SLICE (m) != NO_MOID) {
    return add_mode (p, REF_SYMBOL, 0, NO_NODE, SLICE (m), NO_PACK);
  } else if (SUB (m) != NO_MOID) {
    return add_mode (p, REF_SYMBOL, 0, NO_NODE, SUB (m), NO_PACK);
  } else {
    return NO_MOID;             // weird, FLEX INT or so ...
  }
}

//! @brief Make multiple row pack.

static void make_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NO_PACK) {
    make_multiple_row_pack (NEXT (src), dst, p, dim);
    (void) add_mode_to_pack (dst, add_row (p, dim, MOID (src), NO_NODE, A68_FALSE), TEXT (src), NODE (src));
  }
}

//! @brief Make flex multiple struct.

static MOID_T *make_flex_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  PACK_T *u = NO_PACK;
  make_flex_multiple_row_pack (PACK (m), &u, p, dim);
  return add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u);
}

//! @brief Make multiple struct.

static MOID_T *make_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  PACK_T *u = NO_PACK;
  make_multiple_row_pack (PACK (m), &u, p, dim);
  return add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u);
}

//! @brief Whether mode has row.

static BOOL_T is_mode_has_row (MOID_T * m)
{
  if (IS (m, STRUCT_SYMBOL) || IS (m, UNION_SYMBOL)) {
    BOOL_T k = A68_FALSE;
    PACK_T *p = PACK (m);
    for (; p != NO_PACK && k == A68_FALSE; FORWARD (p)) {
      HAS_ROWS (MOID (p)) = is_mode_has_row (MOID (p));
      k |= (HAS_ROWS (MOID (p)));
    }
    return k;
  } else {
    return (BOOL_T) (HAS_ROWS (m) || IS_ROW (m) || IS_FLEX (m));
  }
}

//! @brief Compute derived modes.

static void compute_derived_modes (MODULE_T * mod)
{
  MOID_T *z;
  int k, len = 0, nlen = 1;
// UNION things.
  absorb_unions (TOP_MOID (mod));
  contract_unions (TOP_MOID (mod));
// The for-statement below prevents an endless loop.
  for (k = 1; k <= 10 && len != nlen; k++) {
// Make deflexed modes.
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (SUB (z) != NO_MOID) {
        if (IS_REF_FLEX (z) && DEFLEXED (SUB_SUB (z)) != NO_MOID) {
          DEFLEXED (z) = add_mode (&TOP_MOID (mod), REF_SYMBOL, 0, NODE (z), DEFLEXED (SUB_SUB (z)), NO_PACK);
        } else if (IS_REF (z) && DEFLEXED (SUB (z)) != NO_MOID) {
          DEFLEXED (z) = add_mode (&TOP_MOID (mod), REF_SYMBOL, 0, NODE (z), DEFLEXED (SUB (z)), NO_PACK);
        } else if (IS_ROW (z) && DEFLEXED (SUB (z)) != NO_MOID) {
          DEFLEXED (z) = add_mode (&TOP_MOID (mod), ROW_SYMBOL, DIM (z), NODE (z), DEFLEXED (SUB (z)), NO_PACK);
        } else if (IS_FLEX (z) && DEFLEXED (SUB (z)) != NO_MOID) {
          DEFLEXED (z) = DEFLEXED (SUB (z));
        } else if (IS_FLEX (z)) {
          DEFLEXED (z) = SUB (z);
        } else {
          DEFLEXED (z) = z;
        }
      }
    }
// Derived modes for stowed modes.
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (NAME (z) == NO_MOID && IS_REF (z)) {
        if (IS (SUB (z), STRUCT_SYMBOL)) {
          NAME (z) = make_name_struct (SUB (z), &TOP_MOID (mod));
        } else if (IS_ROW (SUB (z))) {
          NAME (z) = make_name_row (SUB (z), &TOP_MOID (mod));
        } else if (IS_FLEX (SUB (z)) && SUB_SUB (z) != NO_MOID) {
          NAME (z) = make_name_row (SUB_SUB (z), &TOP_MOID (mod));
        }
      }
      if (MULTIPLE (z) != NO_MOID) {
        ;
      } else if (IS_REF (z)) {
        if (MULTIPLE (SUB (z)) != NO_MOID) {
          MULTIPLE (z) = make_name_struct (MULTIPLE (SUB (z)), &TOP_MOID (mod));
        }
      } else if (IS_ROW (z)) {
        if (IS (SUB (z), STRUCT_SYMBOL)) {
          MULTIPLE (z) = make_multiple_struct (SUB (z), &TOP_MOID (mod), DIM (z));
        }
      }
    }
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (TRIM (z) == NO_MOID && IS_FLEX (z)) {
        TRIM (z) = SUB (z);
      }
      if (TRIM (z) == NO_MOID && IS_REF_FLEX (z)) {
        TRIM (z) = add_mode (&TOP_MOID (mod), REF_SYMBOL, 0, NODE (z), SUB_SUB (z), NO_PACK);
      }
    }
// Fill out stuff for rows, f.i. inverse relations.
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (IS_ROW (z) && DIM (z) > 0 && SUB (z) != NO_MOID && !DERIVATE (z)) {
        (void) add_row (&TOP_MOID (mod), DIM (z) + 1, SUB (z), NODE (z), A68_TRUE);
      } else if (IS_REF (z) && IS (SUB (z), ROW_SYMBOL) && !DERIVATE (SUB (z))) {
        MOID_T *x = add_row (&TOP_MOID (mod), DIM (SUB (z)) + 1, SUB_SUB (z), NODE (SUB (z)), A68_TRUE);
        MOID_T *y = add_mode (&TOP_MOID (mod), REF_SYMBOL, 0, NODE (z), x, NO_PACK);
        NAME (y) = z;
      }
    }
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (IS_ROW (z) && SLICE (z) != NO_MOID) {
        ROWED (SLICE (z)) = z;
      }
      if (IS_REF (z)) {
        MOID_T *y = SUB (z);
        if (SLICE (y) != NO_MOID && IS_ROW (SLICE (y)) && NAME (z) != NO_MOID) {
          ROWED (NAME (z)) = z;
        }
      }
    }
    bind_modes (TOP_NODE (mod));
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (IS (z, INDICANT) && NODE (z) != NO_NODE) {
        EQUIVALENT (z) = MOID (NODE (z));
      }
    }
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      resolve_eq_members (z);
    }
    resolve_eq_tags (INDICANTS (A68_STANDENV));
    resolve_eq_tags (IDENTIFIERS (A68_STANDENV));
    resolve_eq_tags (OPERATORS (A68_STANDENV));
    resolve_equivalent (&M_STRING);
    resolve_equivalent (&M_COMPLEX);
    resolve_equivalent (&M_COMPL);
    resolve_equivalent (&M_LONG_COMPLEX);
    resolve_equivalent (&M_LONG_COMPL);
    resolve_equivalent (&M_LONG_LONG_COMPLEX);
    resolve_equivalent (&M_LONG_LONG_COMPL);
    resolve_equivalent (&M_SEMA);
    resolve_equivalent (&M_PIPE);
// UNION members could be resolved.
    absorb_unions (TOP_MOID (mod));
    contract_unions (TOP_MOID (mod));
// FLEX INDICANT could be resolved.
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (IS_FLEX (z) && SUB (z) != NO_MOID) {
        if (SUB_SUB (z) != NO_MOID && IS (SUB_SUB (z), STRUCT_SYMBOL)) {
          MULTIPLE (z) = make_flex_multiple_struct (SUB_SUB (z), &TOP_MOID (mod), DIM (SUB (z)));
        }
      }
    }
// See what new known modes we have generated by resolving..
    for (z = TOP_MOID (mod); z != STANDENV_MOID (&A68_JOB); FORWARD (z)) {
      MOID_T *v;
      for (v = NEXT (z); v != NO_MOID; FORWARD (v)) {
        if (prove_moid_equivalence (z, v)) {
          EQUIVALENT (z) = v;
          EQUIVALENT (v) = NO_MOID;
        }
      }
    }
// Count the modes to check self consistency.
    len = nlen;
    for (nlen = 0, z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      nlen++;
    }
  }
  ABEND (M_STRING != M_FLEX_ROW_CHAR, ERROR_INTERNAL_CONSISTENCY, __func__);
// Find out what modes contain rows.
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    HAS_ROWS (z) = is_mode_has_row (z);
  }
// Check flexible modes.
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS_FLEX (z) && !IS (SUB (z), ROW_SYMBOL)) {
      diagnostic (A68_ERROR, NODE (z), ERROR_NOT_WELL_FORMED, z);
    }
  }
// Check on fields in structured modes f.i. STRUCT (REAL x, INT n, REAL x) is wrong.
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, STRUCT_SYMBOL) && EQUIVALENT (z) == NO_MOID) {
      PACK_T *s = PACK (z);
      for (; s != NO_PACK; FORWARD (s)) {
        PACK_T *t = NEXT (s);
        BOOL_T x = A68_TRUE;
        for (t = NEXT (s); t != NO_PACK && x; FORWARD (t)) {
          if (TEXT (s) == TEXT (t)) {
            diagnostic (A68_ERROR, NODE (z), ERROR_MULTIPLE_FIELD);
            while (NEXT (s) != NO_PACK && TEXT (NEXT (s)) == TEXT (t)) {
              FORWARD (s);
            }
            x = A68_FALSE;
          }
        }
      }
    }
  }
// Various union test.
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, UNION_SYMBOL) && EQUIVALENT (z) == NO_MOID) {
      PACK_T *s = PACK (z);
// Discard unions with one member.
      if (count_pack_members (s) == 1) {
        diagnostic (A68_ERROR, NODE (z), ERROR_COMPONENT_NUMBER, z);
      }
// Discard incestuous unions with firmly related modes.
      for (; s != NO_PACK; FORWARD (s)) {
        PACK_T *t;
        for (t = NEXT (s); t != NO_PACK; FORWARD (t)) {
          if (MOID (t) != MOID (s)) {
            if (is_firm (MOID (s), MOID (t))) {
              diagnostic (A68_ERROR, NODE (z), ERROR_COMPONENT_RELATED, z);
            }
          }
        }
      }
// Discard incestuous unions with firmly related subsets.
      for (s = PACK (z); s != NO_PACK; FORWARD (s)) {
        MOID_T *n = depref_completely (MOID (s));
        if (IS (n, UNION_SYMBOL) && is_subset (n, z, NO_DEFLEXING)) {
          diagnostic (A68_ERROR, NODE (z), ERROR_SUBSET_RELATED, z, n);
        }
      }
    }
  }
// Wrap up and exit.
  free_postulate_list (A68 (top_postulate), NO_POSTULATE);
  A68 (top_postulate) = NO_POSTULATE;
}

//! @brief Make list of all modes in the program.

void make_moid_list (MODULE_T * mod)
{
  MOID_T *z;
  BOOL_T cont = A68_TRUE;
// Collect modes from the syntax tree.
  reset_moid_tree (TOP_NODE (mod));
  get_modes_from_tree (TOP_NODE (mod), STOP);
  get_mode_from_proc_var_declarations_tree (TOP_NODE (mod));
// Connect indicants to their declarers.
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, INDICANT)) {
      NODE_T *u = NODE (z);
      ABEND (NEXT (u) == NO_NODE, ERROR_INTERNAL_CONSISTENCY, __func__);
      ABEND (NEXT_NEXT (u) == NO_NODE, ERROR_INTERNAL_CONSISTENCY, __func__);
      ABEND (MOID (NEXT_NEXT (u)) == NO_MOID, ERROR_INTERNAL_CONSISTENCY, __func__);
      EQUIVALENT (z) = MOID (NEXT_NEXT (u));
    }
  }
// Checks on wrong declarations.
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    USE (z) = A68_FALSE;
  }
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, INDICANT) && EQUIVALENT (z) != NO_MOID) {
      if (!is_well_formed (z, EQUIVALENT (z), A68_FALSE, A68_FALSE, A68_TRUE)) {
        diagnostic (A68_ERROR, NODE (z), ERROR_NOT_WELL_FORMED, z);
        cont = A68_FALSE;
      }
    }
  }
  for (z = TOP_MOID (mod); cont && z != NO_MOID; FORWARD (z)) {
    if (IS (z, INDICANT) && EQUIVALENT (z) != NO_MOID) {
      ;
    } else if (NODE (z) != NO_NODE) {
      if (!is_well_formed (NO_MOID, z, A68_FALSE, A68_FALSE, A68_TRUE)) {
        diagnostic (A68_ERROR, NODE (z), ERROR_NOT_WELL_FORMED, z);
      }
    }
  }
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    ABEND (USE (z), ERROR_INTERNAL_CONSISTENCY, __func__);
  }
  if (ERROR_COUNT (mod) != 0) {
    return;
  }
  compute_derived_modes (mod);
  init_postulates ();
}

// Symbol table handling, managing TAGS.

//! @brief Set level for procedures.

void set_proc_level (NODE_T * p, int n)
{
  for (; p != NO_NODE; FORWARD (p)) {
    PROCEDURE_LEVEL (INFO (p)) = n;
    if (IS (p, ROUTINE_TEXT)) {
      set_proc_level (SUB (p), n + 1);
    } else {
      set_proc_level (SUB (p), n);
    }
  }
}

//! @brief Set nests for diagnostics.

void set_nest (NODE_T * p, NODE_T * s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    NEST (p) = s;
    if (IS (p, PARTICULAR_PROGRAM)) {
      set_nest (SUB (p), p);
    } else if (IS (p, CLOSED_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, COLLATERAL_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, CONDITIONAL_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, CASE_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, CONFORMITY_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, LOOP_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else {
      set_nest (SUB (p), s);
    }
  }
}

// Routines that work with tags and symbol tables.

static void tax_tags (NODE_T *);
static void tax_specifier_list (NODE_T *);
static void tax_parameter_list (NODE_T *);
static void tax_format_texts (NODE_T *);

//! @brief Find a tag, searching symbol tables towards the root.

int first_tag_global (TABLE_T * table, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s = NO_TAG;
    for (s = IDENTIFIERS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return IDENTIFIER;
      }
    }
    for (s = INDICANTS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return INDICANT;
      }
    }
    for (s = LABELS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return LABEL;
      }
    }
    for (s = OPERATORS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return OP_SYMBOL;
      }
    }
    for (s = PRIO (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return PRIO_SYMBOL;
      }
    }
    return first_tag_global (PREVIOUS (table), name);
  } else {
    return STOP;
  }
}

#define PORTCHECK_TAX(p, q) {\
  if (q == A68_FALSE) {\
    diagnostic (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_TAG_NOT_PORTABLE);\
  }}

//! @brief Check portability of sub tree.

void portcheck (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    portcheck (SUB (p));
    if (OPTION_PORTCHECK (&A68_JOB)) {
      if (IS (p, INDICANT) && MOID (p) != NO_MOID) {
        PORTCHECK_TAX (p, PORTABLE (MOID (p)));
        PORTABLE (MOID (p)) = A68_TRUE;
      } else if (IS (p, IDENTIFIER)) {
        PORTCHECK_TAX (p, PORTABLE (TAX (p)));
        PORTABLE (TAX (p)) = A68_TRUE;
      } else if (IS (p, OPERATOR)) {
        PORTCHECK_TAX (p, PORTABLE (TAX (p)));
        PORTABLE (TAX (p)) = A68_TRUE;
      } else if (IS (p, ASSERTION)) {
        diagnostic (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_TAG_NOT_PORTABLE);
      }
    }
  }
}

//! @brief Whether routine can be "lengthety-mapped".

static BOOL_T is_mappable_routine (char *z)
{
#define ACCEPT(u, v) {\
  if (strlen (u) >= strlen (v)) {\
    if (strcmp (&u[strlen (u) - strlen (v)], v) == 0) {\
      return A68_TRUE;\
  }}}
// Math routines.
  ACCEPT (z, "arccos");
  ACCEPT (z, "arccosdg");
  ACCEPT (z, "arccot");
  ACCEPT (z, "arccotdg");
  ACCEPT (z, "arcsin");
  ACCEPT (z, "arcsindg");
  ACCEPT (z, "arctan");
  ACCEPT (z, "arctandg");
  ACCEPT (z, "beta");
  ACCEPT (z, "betainc");
  ACCEPT (z, "cbrt");
  ACCEPT (z, "cos");
  ACCEPT (z, "cosdg");
  ACCEPT (z, "cospi");
  ACCEPT (z, "cot");
  ACCEPT (z, "cot");
  ACCEPT (z, "cotdg");
  ACCEPT (z, "cotpi");
  ACCEPT (z, "curt");
  ACCEPT (z, "erf");
  ACCEPT (z, "erfc");
  ACCEPT (z, "exp");
  ACCEPT (z, "gamma");
  ACCEPT (z, "gammainc");
  ACCEPT (z, "gammaincg");
  ACCEPT (z, "gammaincgf");
  ACCEPT (z, "ln");
  ACCEPT (z, "log");
  ACCEPT (z, "pi");
  ACCEPT (z, "sin");
  ACCEPT (z, "sindg");
  ACCEPT (z, "sinpi");
  ACCEPT (z, "sqrt");
  ACCEPT (z, "tan");
  ACCEPT (z, "tandg");
  ACCEPT (z, "tanpi");
// Random generator.
  ACCEPT (z, "nextrandom");
  ACCEPT (z, "random");
// BITS.
  ACCEPT (z, "bitspack");
// Enquiries.
  ACCEPT (z, "maxint");
  ACCEPT (z, "intwidth");
  ACCEPT (z, "maxreal");
  ACCEPT (z, "realwidth");
  ACCEPT (z, "expwidth");
  ACCEPT (z, "maxbits");
  ACCEPT (z, "bitswidth");
  ACCEPT (z, "byteswidth");
  ACCEPT (z, "smallreal");
  return A68_FALSE;
#undef ACCEPT
}

//! @brief Map "short sqrt" onto "sqrt" etcetera.

static TAG_T *bind_lengthety_identifier (char *u)
{
#define CAR(u, v) (strncmp (u, v, strlen(v)) == 0)
// We can only map routines blessed by "is_mappable_routine", so there is no
// "short print" or "long char in string".
  if (CAR (u, "short")) {
    do {
      char *v;
      TAG_T *w;
      u = &u[strlen ("short")];
      v = TEXT (add_token (&A68 (top_token), u));
      w = find_tag_local (A68_STANDENV, IDENTIFIER, v);
      if (w != NO_TAG && is_mappable_routine (v)) {
        return w;
      }
    } while (CAR (u, "short"));
  } else if (CAR (u, "long")) {
    do {
      char *v;
      TAG_T *w;
      u = &u[strlen ("long")];
      v = TEXT (add_token (&A68 (top_token), u));
      w = find_tag_local (A68_STANDENV, IDENTIFIER, v);
      if (w != NO_TAG && is_mappable_routine (v)) {
        return w;
      }
    } while (CAR (u, "long"));
  }
  return NO_TAG;
#undef CAR
}

//! @brief Bind identifier tags to the symbol table.

static void bind_identifier_tag_to_symbol_table (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    bind_identifier_tag_to_symbol_table (SUB (p));
    if (is_one_of (p, IDENTIFIER, DEFINING_IDENTIFIER, STOP)) {
      int att = first_tag_global (TABLE (p), NSYMBOL (p));
      TAG_T *z;
      if (att == STOP) {
        if ((z = bind_lengthety_identifier (NSYMBOL (p))) != NO_TAG) {
          MOID (p) = MOID (z);
        }
        TAX (p) = z;
      } else {
        z = find_tag_global (TABLE (p), att, NSYMBOL (p));
        if (att == IDENTIFIER && z != NO_TAG) {
          MOID (p) = MOID (z);
        } else if (att == LABEL && z != NO_TAG) {
          ;
        } else if ((z = bind_lengthety_identifier (NSYMBOL (p))) != NO_TAG) {
          MOID (p) = MOID (z);
        } else {
          diagnostic (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          z = add_tag (TABLE (p), IDENTIFIER, p, M_ERROR, NORMAL_IDENTIFIER);
          MOID (p) = M_ERROR;
        }
        TAX (p) = z;
        if (IS (p, DEFINING_IDENTIFIER)) {
          NODE (z) = p;
        }
      }
    }
  }
}

//! @brief Bind indicant tags to the symbol table.

static void bind_indicant_tag_to_symbol_table (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    bind_indicant_tag_to_symbol_table (SUB (p));
    if (is_one_of (p, INDICANT, DEFINING_INDICANT, STOP)) {
      TAG_T *z = find_tag_global (TABLE (p), INDICANT, NSYMBOL (p));
      if (z != NO_TAG) {
        MOID (p) = MOID (z);
        TAX (p) = z;
        if (IS (p, DEFINING_INDICANT)) {
          NODE (z) = p;
        }
      }
    }
  }
}

//! @brief Enter specifier identifiers in the symbol table.

static void tax_specifiers (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_specifiers (SUB (p));
    if (SUB (p) != NO_NODE && IS (p, SPECIFIER)) {
      tax_specifier_list (SUB (p));
    }
  }
}

//! @brief Enter specifier identifiers in the symbol table.

static void tax_specifier_list (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, OPEN_SYMBOL)) {
      tax_specifier_list (NEXT (p));
    } else if (is_one_of (p, CLOSE_SYMBOL, VOID_SYMBOL, STOP)) {
      ;
    } else if (IS (p, IDENTIFIER)) {
      TAG_T *z = add_tag (TABLE (p), IDENTIFIER, p, NO_MOID, SPECIFIER_IDENTIFIER);
      HEAP (z) = LOC_SYMBOL;
    } else if (IS (p, DECLARER)) {
      tax_specifiers (SUB (p));
      tax_specifier_list (NEXT (p));
// last identifier entry is identifier with this declarer.
      if (IDENTIFIERS (TABLE (p)) != NO_TAG && PRIO (IDENTIFIERS (TABLE (p))) == SPECIFIER_IDENTIFIER)
        MOID (IDENTIFIERS (TABLE (p))) = MOID (p);
    }
  }
}

//! @brief Enter parameter identifiers in the symbol table.

static void tax_parameters (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE) {
      tax_parameters (SUB (p));
      if (IS (p, PARAMETER_PACK)) {
        tax_parameter_list (SUB (p));
      }
    }
  }
}

//! @brief Enter parameter identifiers in the symbol table.

static void tax_parameter_list (NODE_T * p)
{
  if (p != NO_NODE) {
    if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_parameter_list (NEXT (p));
    } else if (IS (p, CLOSE_SYMBOL)) {
      ;
    } else if (is_one_of (p, PARAMETER_LIST, PARAMETER, STOP)) {
      tax_parameter_list (NEXT (p));
      tax_parameter_list (SUB (p));
    } else if (IS (p, IDENTIFIER)) {
// parameters are always local.
      HEAP (add_tag (TABLE (p), IDENTIFIER, p, NO_MOID, PARAMETER_IDENTIFIER)) = LOC_SYMBOL;
    } else if (IS (p, DECLARER)) {
      TAG_T *s;
      tax_parameter_list (NEXT (p));
// last identifier entries are identifiers with this declarer.
      for (s = IDENTIFIERS (TABLE (p)); s != NO_TAG && MOID (s) == NO_MOID; FORWARD (s)) {
        MOID (s) = MOID (p);
      }
      tax_parameters (SUB (p));
    }
  }
}

//! @brief Enter FOR identifiers in the symbol table.

static void tax_for_identifiers (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_for_identifiers (SUB (p));
    if (IS (p, FOR_SYMBOL)) {
      if ((FORWARD (p)) != NO_NODE) {
        (void) add_tag (TABLE (p), IDENTIFIER, p, M_INT, LOOP_IDENTIFIER);
      }
    }
  }
}

//! @brief Enter routine texts in the symbol table.

static void tax_routine_texts (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_routine_texts (SUB (p));
    if (IS (p, ROUTINE_TEXT)) {
      TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, MOID (p), ROUTINE_TEXT);
      TAX (p) = z;
      HEAP (z) = LOC_SYMBOL;
      USE (z) = A68_TRUE;
    }
  }
}

//! @brief Enter format texts in the symbol table.

static void tax_format_texts (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_format_texts (SUB (p));
    if (IS (p, FORMAT_TEXT)) {
      TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, M_FORMAT, FORMAT_TEXT);
      TAX (p) = z;
      USE (z) = A68_TRUE;
    } else if (IS (p, FORMAT_DELIMITER_SYMBOL) && NEXT (p) != NO_NODE) {
      TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, M_FORMAT, FORMAT_IDENTIFIER);
      TAX (p) = z;
      USE (z) = A68_TRUE;
    }
  }
}

//! @brief Enter FORMAT pictures in the symbol table.

static void tax_pictures (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_pictures (SUB (p));
    if (IS (p, PICTURE)) {
      TAX (p) = add_tag (TABLE (p), ANONYMOUS, p, M_COLLITEM, FORMAT_IDENTIFIER);
    }
  }
}

//! @brief Enter generators in the symbol table.

static void tax_generators (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_generators (SUB (p));
    if (IS (p, GENERATOR)) {
      if (IS (SUB (p), LOC_SYMBOL)) {
        TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, SUB_MOID (SUB (p)), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        TAX (p) = z;
      }
    }
  }
}

//! @brief Find a firmly related operator for operands.

static TAG_T *find_firmly_related_op (TABLE_T * c, char *n, MOID_T * l, MOID_T * r, TAG_T * self)
{
  if (c != NO_TABLE) {
    TAG_T *s = OPERATORS (c);
    for (; s != NO_TAG; FORWARD (s)) {
      if (s != self && NSYMBOL (NODE (s)) == n) {
        PACK_T *t = PACK (MOID (s));
        if (t != NO_PACK && is_firm (MOID (t), l)) {
// catch monadic operator.
          if ((FORWARD (t)) == NO_PACK) {
            if (r == NO_MOID) {
              return s;
            }
          } else {
// catch dyadic operator.
            if (r != NO_MOID && is_firm (MOID (t), r)) {
              return s;
            }
          }
        }
      }
    }
  }
  return NO_TAG;
}

//! @brief Check for firmly related operators in this range.

static void test_firmly_related_ops_local (NODE_T * p, TAG_T * s)
{
  if (s != NO_TAG) {
    PACK_T *u = PACK (MOID (s));
    if (u != NO_PACK) {
      MOID_T *l = MOID (u);
      MOID_T *r = (NEXT (u) != NO_PACK ? MOID (NEXT (u)) : NO_MOID);
      TAG_T *t = find_firmly_related_op (TAG_TABLE (s), NSYMBOL (NODE (s)), l, r, s);
      if (t != NO_TAG) {
        if (TAG_TABLE (t) == A68_STANDENV) {
          diagnostic (A68_ERROR, p, ERROR_OPERATOR_RELATED, MOID (s), NSYMBOL (NODE (s)), MOID (t), NSYMBOL (NODE (t)));
          ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
        } else {
          diagnostic (A68_ERROR, p, ERROR_OPERATOR_RELATED, MOID (s), NSYMBOL (NODE (s)), MOID (t), NSYMBOL (NODE (t)));
        }
      }
    }
    if (NEXT (s) != NO_TAG) {
      test_firmly_related_ops_local ((p == NO_NODE ? NO_NODE : NODE (NEXT (s))), NEXT (s));
    }
  }
}

//! @brief Find firmly related operators in this program.

static void test_firmly_related_ops (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE && is_new_lexical_level (p)) {
      TAG_T *oops = OPERATORS (TABLE (SUB (p)));
      if (oops != NO_TAG) {
        test_firmly_related_ops_local (NODE (oops), oops);
      }
    }
    test_firmly_related_ops (SUB (p));
  }
}

//! @brief Driver for the processing of TAXes.

void collect_taxes (NODE_T * p)
{
  tax_tags (p);
  tax_specifiers (p);
  tax_parameters (p);
  tax_for_identifiers (p);
  tax_routine_texts (p);
  tax_pictures (p);
  tax_format_texts (p);
  tax_generators (p);
  bind_identifier_tag_to_symbol_table (p);
  bind_indicant_tag_to_symbol_table (p);
  test_firmly_related_ops (p);
  test_firmly_related_ops_local (NO_NODE, OPERATORS (A68_STANDENV));
}

//! @brief Whether tag has already been declared in this range.

static void already_declared (NODE_T * n, int a)
{
  if (find_tag_local (TABLE (n), a, NSYMBOL (n)) != NO_TAG) {
    diagnostic (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
}

//! @brief Whether tag has already been declared in this range.

static void already_declared_hidden (NODE_T * n, int a)
{
  TAG_T *s;
  if (find_tag_local (TABLE (n), a, NSYMBOL (n)) != NO_TAG) {
    diagnostic (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
  if ((s = find_tag_global (PREVIOUS (TABLE (n)), a, NSYMBOL (n))) != NO_TAG) {
    if (TAG_TABLE (s) == A68_STANDENV) {
      diagnostic (A68_WARNING, n, WARNING_HIDES_PRELUDE, MOID (s), NSYMBOL (n));
    } else {
      diagnostic (A68_WARNING, n, WARNING_HIDES, NSYMBOL (n));
    }
  }
}

//! @brief Add tag to local symbol table.

TAG_T *add_tag (TABLE_T * s, int a, NODE_T * n, MOID_T * m, int p)
{
#define INSERT_TAG(l, n) {NEXT (n) = *(l); *(l) = (n);}
  if (s != NO_TABLE) {
    TAG_T *z = new_tag ();
    TAG_TABLE (z) = s;
    PRIO (z) = p;
    MOID (z) = m;
    NODE (z) = n;
//    TAX(n) = z;.
    switch (a) {
    case IDENTIFIER:{
        already_declared_hidden (n, IDENTIFIER);
        already_declared_hidden (n, LABEL);
        INSERT_TAG (&IDENTIFIERS (s), z);
        break;
      }
    case INDICANT:{
        already_declared_hidden (n, INDICANT);
        already_declared (n, OP_SYMBOL);
        already_declared (n, PRIO_SYMBOL);
        INSERT_TAG (&INDICANTS (s), z);
        break;
      }
    case LABEL:{
        already_declared_hidden (n, LABEL);
        already_declared_hidden (n, IDENTIFIER);
        INSERT_TAG (&LABELS (s), z);
        break;
      }
    case OP_SYMBOL:{
        already_declared (n, INDICANT);
        INSERT_TAG (&OPERATORS (s), z);
        break;
      }
    case PRIO_SYMBOL:{
        already_declared (n, PRIO_SYMBOL);
        already_declared (n, INDICANT);
        INSERT_TAG (&PRIO (s), z);
        break;
      }
    case ANONYMOUS:{
        INSERT_TAG (&ANONYMOUS (s), z);
        break;
      }
    default:{
        ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
      }
    }
    return z;
  } else {
    return NO_TAG;
  }
}

//! @brief Find a tag, searching symbol tables towards the root.

TAG_T *find_tag_global (TABLE_T * table, int a, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s = NO_TAG;
    switch (a) {
    case IDENTIFIER:{
        s = IDENTIFIERS (table);
        break;
      }
    case INDICANT:{
        s = INDICANTS (table);
        break;
      }
    case LABEL:{
        s = LABELS (table);
        break;
      }
    case OP_SYMBOL:{
        s = OPERATORS (table);
        break;
      }
    case PRIO_SYMBOL:{
        s = PRIO (table);
        break;
      }
    default:{
        ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
        break;
      }
    }
    for (; s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return s;
      }
    }
    return find_tag_global (PREVIOUS (table), a, name);
  } else {
    return NO_TAG;
  }
}

//! @brief Whether identifier or label global.

int is_identifier_or_label_global (TABLE_T * table, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s;
    for (s = IDENTIFIERS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return IDENTIFIER;
      }
    }
    for (s = LABELS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return LABEL;
      }
    }
    return is_identifier_or_label_global (PREVIOUS (table), name);
  } else {
    return 0;
  }
}

//! @brief Find a tag, searching only local symbol table.

TAG_T *find_tag_local (TABLE_T * table, int a, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s = NO_TAG;
    if (a == OP_SYMBOL) {
      s = OPERATORS (table);
    } else if (a == PRIO_SYMBOL) {
      s = PRIO (table);
    } else if (a == IDENTIFIER) {
      s = IDENTIFIERS (table);
    } else if (a == INDICANT) {
      s = INDICANTS (table);
    } else if (a == LABEL) {
      s = LABELS (table);
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
    for (; s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return s;
      }
    }
  }
  return NO_TAG;
}

//! @brief Whether context specifies HEAP or LOC for an identifier.

static int tab_qualifier (NODE_T * p)
{
  if (p != NO_NODE) {
    if (is_one_of (p, UNIT, ASSIGNATION, TERTIARY, SECONDARY, GENERATOR, STOP)) {
      return tab_qualifier (SUB (p));
    } else if (is_one_of (p, LOC_SYMBOL, HEAP_SYMBOL, NEW_SYMBOL, STOP)) {
      return ATTRIBUTE (p) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL;
    } else {
      return LOC_SYMBOL;
    }
  } else {
    return LOC_SYMBOL;
  }
}

//! @brief Enter identity declarations in the symbol table.

static void tax_identity_dec (NODE_T * p, MOID_T ** m)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTITY_DECLARATION)) {
      tax_identity_dec (SUB (p), m);
      tax_identity_dec (NEXT (p), m);
    } else if (IS (p, DECLARER)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_identity_dec (NEXT (p), m);
    } else if (IS (p, COMMA_SYMBOL)) {
      tax_identity_dec (NEXT (p), m);
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (TABLE (p), IDENTIFIER, NSYMBOL (p));
      MOID (p) = *m;
      HEAP (entry) = LOC_SYMBOL;
      TAX (p) = entry;
      MOID (entry) = *m;
      if (ATTRIBUTE (*m) == REF_SYMBOL) {
        HEAP (entry) = tab_qualifier (NEXT_NEXT (p));
      }
      tax_identity_dec (NEXT_NEXT (p), m);
    } else {
      tax_tags (p);
    }
  }
}

//! @brief Enter variable declarations in the symbol table.

static void tax_variable_dec (NODE_T * p, int *q, MOID_T ** m)
{
  if (p != NO_NODE) {
    if (IS (p, VARIABLE_DECLARATION)) {
      tax_variable_dec (SUB (p), q, m);
      tax_variable_dec (NEXT (p), q, m);
    } else if (IS (p, DECLARER)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_variable_dec (NEXT (p), q, m);
    } else if (IS (p, QUALIFIER)) {
      *q = ATTRIBUTE (SUB (p));
      tax_variable_dec (NEXT (p), q, m);
    } else if (IS (p, COMMA_SYMBOL)) {
      tax_variable_dec (NEXT (p), q, m);
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (TABLE (p), IDENTIFIER, NSYMBOL (p));
      MOID (p) = *m;
      TAX (p) = entry;
      HEAP (entry) = *q;
      if (*q == LOC_SYMBOL) {
        TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, SUB (*m), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        BODY (entry) = z;
      } else {
        BODY (entry) = NO_TAG;
      }
      MOID (entry) = *m;
      tax_variable_dec (NEXT (p), q, m);
    } else {
      tax_tags (p);
    }
  }
}

//! @brief Enter procedure variable declarations in the symbol table.

static void tax_proc_variable_dec (NODE_T * p, int *q)
{
  if (p != NO_NODE) {
    if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      tax_proc_variable_dec (SUB (p), q);
      tax_proc_variable_dec (NEXT (p), q);
    } else if (IS (p, QUALIFIER)) {
      *q = ATTRIBUTE (SUB (p));
      tax_proc_variable_dec (NEXT (p), q);
    } else if (is_one_of (p, PROC_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_proc_variable_dec (NEXT (p), q);
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (TABLE (p), IDENTIFIER, NSYMBOL (p));
      TAX (p) = entry;
      HEAP (entry) = *q;
      MOID (entry) = MOID (p);
      if (*q == LOC_SYMBOL) {
        TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, SUB_MOID (p), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        BODY (entry) = z;
      } else {
        BODY (entry) = NO_TAG;
      }
      tax_proc_variable_dec (NEXT (p), q);
    } else {
      tax_tags (p);
    }
  }
}

//! @brief Enter procedure declarations in the symbol table.

static void tax_proc_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, PROCEDURE_DECLARATION)) {
      tax_proc_dec (SUB (p));
      tax_proc_dec (NEXT (p));
    } else if (is_one_of (p, PROC_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_proc_dec (NEXT (p));
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (TABLE (p), IDENTIFIER, NSYMBOL (p));
      MOID_T *m = MOID (NEXT_NEXT (p));
      MOID (p) = m;
      TAX (p) = entry;
      CODEX (entry) |= PROC_DECLARATION_MASK;
      HEAP (entry) = LOC_SYMBOL;
      MOID (entry) = m;
      tax_proc_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

//! @brief Check validity of operator declaration.

static void check_operator_dec (NODE_T * p, MOID_T * u)
{
  int k = 0;
  if (u == NO_MOID) {
    NODE_T *pack = SUB_SUB (NEXT_NEXT (p));     // Where the parameter pack is
    if (ATTRIBUTE (NEXT_NEXT (p)) != ROUTINE_TEXT) {
      pack = SUB (pack);
    }
    k = 1 + count_operands (pack);
  } else {
    k = count_pack_members (PACK (u));
  }
  if (k < 1 || k > 2) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_OPERAND_NUMBER);
    k = 0;
  }
  if (k == 1 && strchr (NOMADS, NSYMBOL (p)[0]) != NO_TEXT) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
  } else if (k == 2 && !find_tag_global (TABLE (p), PRIO_SYMBOL, NSYMBOL (p))) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_DYADIC_PRIORITY);
  }
}

//! @brief Enter operator declarations in the symbol table.

static void tax_op_dec (NODE_T * p, MOID_T ** m)
{
  if (p != NO_NODE) {
    if (IS (p, OPERATOR_DECLARATION)) {
      tax_op_dec (SUB (p), m);
      tax_op_dec (NEXT (p), m);
    } else if (IS (p, OPERATOR_PLAN)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_op_dec (NEXT (p), m);
    } else if (IS (p, OP_SYMBOL)) {
      tax_op_dec (NEXT (p), m);
    } else if (IS (p, COMMA_SYMBOL)) {
      tax_op_dec (NEXT (p), m);
    } else if (IS (p, DEFINING_OPERATOR)) {
      TAG_T *entry = OPERATORS (TABLE (p));
      check_operator_dec (p, *m);
      while (entry != NO_TAG && NODE (entry) != p) {
        FORWARD (entry);
      }
      MOID (p) = *m;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      MOID (entry) = *m;
      tax_op_dec (NEXT (p), m);
    } else {
      tax_tags (p);
    }
  }
}

//! @brief Enter brief operator declarations in the symbol table.

static void tax_brief_op_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, BRIEF_OPERATOR_DECLARATION)) {
      tax_brief_op_dec (SUB (p));
      tax_brief_op_dec (NEXT (p));
    } else if (is_one_of (p, OP_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_brief_op_dec (NEXT (p));
    } else if (IS (p, DEFINING_OPERATOR)) {
      TAG_T *entry = OPERATORS (TABLE (p));
      MOID_T *m = MOID (NEXT_NEXT (p));
      check_operator_dec (p, NO_MOID);
      while (entry != NO_TAG && NODE (entry) != p) {
        FORWARD (entry);
      }
      MOID (p) = m;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      MOID (entry) = m;
      tax_brief_op_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

//! @brief Enter priority declarations in the symbol table.

static void tax_prio_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, PRIORITY_DECLARATION)) {
      tax_prio_dec (SUB (p));
      tax_prio_dec (NEXT (p));
    } else if (is_one_of (p, PRIO_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_prio_dec (NEXT (p));
    } else if (IS (p, DEFINING_OPERATOR)) {
      TAG_T *entry = PRIO (TABLE (p));
      while (entry != NO_TAG && NODE (entry) != p) {
        FORWARD (entry);
      }
      MOID (p) = NO_MOID;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      tax_prio_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

//! @brief Enter TAXes in the symbol table.

static void tax_tags (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    int heap = LOC_SYMBOL;
    MOID_T *m = NO_MOID;
    if (IS (p, IDENTITY_DECLARATION)) {
      tax_identity_dec (p, &m);
    } else if (IS (p, VARIABLE_DECLARATION)) {
      tax_variable_dec (p, &heap, &m);
    } else if (IS (p, PROCEDURE_DECLARATION)) {
      tax_proc_dec (p);
    } else if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      tax_proc_variable_dec (p, &heap);
    } else if (IS (p, OPERATOR_DECLARATION)) {
      tax_op_dec (p, &m);
    } else if (IS (p, BRIEF_OPERATOR_DECLARATION)) {
      tax_brief_op_dec (p);
    } else if (IS (p, PRIORITY_DECLARATION)) {
      tax_prio_dec (p);
    } else {
      tax_tags (SUB (p));
    }
  }
}

//! @brief Reset symbol table nest count.

void reset_symbol_table_nest_count (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE && is_new_lexical_level (p)) {
      NEST (TABLE (SUB (p))) = A68 (symbol_table_count)++;
    }
    reset_symbol_table_nest_count (SUB (p));
  }
}

//! @brief Bind routines in symbol table to the tree.

void bind_routine_tags_to_tree (NODE_T * p)
{
// By inserting coercions etc. some may have shifted.
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ROUTINE_TEXT) && TAX (p) != NO_TAG) {
      NODE (TAX (p)) = p;
    }
    bind_routine_tags_to_tree (SUB (p));
  }
}

//! @brief Bind formats in symbol table to tree.

void bind_format_tags_to_tree (NODE_T * p)
{
// By inserting coercions etc. some may have shifted.
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FORMAT_TEXT) && TAX (p) != NO_TAG) {
      NODE (TAX (p)) = p;
    } else if (IS (p, FORMAT_DELIMITER_SYMBOL) && NEXT (p) != NO_NODE && TAX (p) != NO_TAG) {
      NODE (TAX (p)) = p;
    }
    bind_format_tags_to_tree (SUB (p));
  }
}

//! @brief Fill outer level of symbol table.

void fill_symbol_table_outer (NODE_T * p, TABLE_T * s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (TABLE (p) != NO_TABLE) {
      OUTER (TABLE (p)) = s;
    }
    if (SUB (p) != NO_NODE && IS (p, ROUTINE_TEXT)) {
      fill_symbol_table_outer (SUB (p), TABLE (SUB (p)));
    } else if (SUB (p) != NO_NODE && IS (p, FORMAT_TEXT)) {
      fill_symbol_table_outer (SUB (p), TABLE (SUB (p)));
    } else {
      fill_symbol_table_outer (SUB (p), s);
    }
  }
}

//! @brief Flood branch in tree with local symbol table "s".

static void flood_with_symbol_table_restricted (NODE_T * p, TABLE_T * s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    TABLE (p) = s;
    if (ATTRIBUTE (p) != ROUTINE_TEXT && ATTRIBUTE (p) != SPECIFIED_UNIT) {
      if (is_new_lexical_level (p)) {
        PREVIOUS (TABLE (SUB (p))) = s;
      } else {
        flood_with_symbol_table_restricted (SUB (p), s);
      }
    }
  }
}

//! @brief Final structure of symbol table after parsing.

void finalise_symbol_table_setup (NODE_T * p, int l)
{
  TABLE_T *s = TABLE (p);
  NODE_T *q = p;
  while (q != NO_NODE) {
// routine texts are ranges.
    if (IS (q, ROUTINE_TEXT)) {
      flood_with_symbol_table_restricted (SUB (q), new_symbol_table (s));
    }
// specifiers are ranges.
    else if (IS (q, SPECIFIED_UNIT)) {
      flood_with_symbol_table_restricted (SUB (q), new_symbol_table (s));
    }
// level count and recursion.
    if (SUB (q) != NO_NODE) {
      if (is_new_lexical_level (q)) {
        LEX_LEVEL (SUB (q)) = l + 1;
        PREVIOUS (TABLE (SUB (q))) = s;
        finalise_symbol_table_setup (SUB (q), l + 1);
        if (IS (q, WHILE_PART)) {
// This was a bug that went unnoticed for 15 years!.
          TABLE_T *s2 = TABLE (SUB (q));
          if ((FORWARD (q)) == NO_NODE) {
            return;
          }
          if (IS (q, ALT_DO_PART)) {
            PREVIOUS (TABLE (SUB (q))) = s2;
            LEX_LEVEL (SUB (q)) = l + 2;
            finalise_symbol_table_setup (SUB (q), l + 2);
          }
        }
      } else {
        TABLE (SUB (q)) = s;
        finalise_symbol_table_setup (SUB (q), l);
      }
    }
    TABLE (q) = s;
    if (IS (q, FOR_SYMBOL)) {
      FORWARD (q);
    }
    FORWARD (q);
  }
// FOR identifiers are in the DO ... OD range.
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, FOR_SYMBOL)) {
      TABLE (NEXT (q)) = TABLE (SEQUENCE (NEXT (q)));
    }
  }
}

//! @brief First structure of symbol table for parsing.

void preliminary_symbol_table_setup (NODE_T * p)
{
  NODE_T *q;
  TABLE_T *s = TABLE (p);
  BOOL_T not_a_for_range = A68_FALSE;
// let the tree point to the current symbol table.
  for (q = p; q != NO_NODE; FORWARD (q)) {
    TABLE (q) = s;
  }
// insert new tables when required.
  for (q = p; q != NO_NODE && !not_a_for_range; FORWARD (q)) {
    if (SUB (q) != NO_NODE) {
// BEGIN ... END, CODE ... EDOC, DEF ... FED, DO ... OD, $ ... $, { ... } are ranges.
      if (is_one_of (q, BEGIN_SYMBOL, DO_SYMBOL, ALT_DO_SYMBOL, FORMAT_DELIMITER_SYMBOL, ACCO_SYMBOL, STOP)) {
        TABLE (SUB (q)) = new_symbol_table (s);
        preliminary_symbol_table_setup (SUB (q));
      }
// ( ... ) is a range.
      else if (IS (q, OPEN_SYMBOL)) {
        if (whether (q, OPEN_SYMBOL, THEN_BAR_SYMBOL, STOP)) {
          TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NO_NODE) {
            not_a_for_range = A68_TRUE;
          } else {
            if (IS (q, THEN_BAR_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (IS (q, OPEN_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
// don't worry about STRUCT (...), UNION (...), PROC (...) yet.
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
// IF ... THEN ... ELSE ... FI are ranges.
      else if (IS (q, IF_SYMBOL)) {
        if (whether (q, IF_SYMBOL, THEN_SYMBOL, STOP)) {
          TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NO_NODE) {
            not_a_for_range = A68_TRUE;
          } else {
            if (IS (q, ELSE_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (IS (q, IF_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
// CASE ... IN ... OUT ... ESAC are ranges.
      else if (IS (q, CASE_SYMBOL)) {
        if (whether (q, CASE_SYMBOL, IN_SYMBOL, STOP)) {
          TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NO_NODE) {
            not_a_for_range = A68_TRUE;
          } else {
            if (IS (q, OUT_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (IS (q, CASE_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
// UNTIL ... OD is a range.
      else if (IS (q, UNTIL_SYMBOL) && SUB (q) != NO_NODE) {
        TABLE (SUB (q)) = new_symbol_table (s);
        preliminary_symbol_table_setup (SUB (q));
// WHILE ... DO ... OD are ranges.
      } else if (IS (q, WHILE_SYMBOL)) {
        TABLE_T *u = new_symbol_table (s);
        TABLE (SUB (q)) = u;
        preliminary_symbol_table_setup (SUB (q));
        if ((FORWARD (q)) == NO_NODE) {
          not_a_for_range = A68_TRUE;
        } else if (IS (q, ALT_DO_SYMBOL)) {
          TABLE (SUB (q)) = new_symbol_table (u);
          preliminary_symbol_table_setup (SUB (q));
        }
      } else {
        TABLE (SUB (q)) = s;
        preliminary_symbol_table_setup (SUB (q));
      }
    }
  }
// FOR identifiers will go to the DO ... OD range.
  if (!not_a_for_range) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      if (IS (q, FOR_SYMBOL)) {
        NODE_T *r = q;
        TABLE (NEXT (q)) = NO_TABLE;
        for (; r != NO_NODE && TABLE (NEXT (q)) == NO_TABLE; FORWARD (r)) {
          if ((is_one_of (r, WHILE_SYMBOL, ALT_DO_SYMBOL, STOP)) && (NEXT (q) != NO_NODE && SUB (r) != NO_NODE)) {
            TABLE (NEXT (q)) = TABLE (SUB (r));
            SEQUENCE (NEXT (q)) = SUB (r);
          }
        }
      }
    }
  }
}

//! @brief Mark a mode as in use.

static void mark_mode (MOID_T * m)
{
  if (m != NO_MOID && USE (m) == A68_FALSE) {
    PACK_T *p = PACK (m);
    USE (m) = A68_TRUE;
    for (; p != NO_PACK; FORWARD (p)) {
      mark_mode (MOID (p));
      mark_mode (SUB (m));
      mark_mode (SLICE (m));
    }
  }
}

//! @brief Traverse tree and mark modes as used.

void mark_moids (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    mark_moids (SUB (p));
    if (MOID (p) != NO_MOID) {
      mark_mode (MOID (p));
    }
  }
}

//! @brief Mark various tags as used.

void mark_auxilliary (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE) {
// You get no warnings on unused PROC parameters. That is ok since A68 has some
// parameters that you may not use at all - think of PROC (REF FILE) BOOL event
// routines in transput.
      mark_auxilliary (SUB (p));
    } else if (IS (p, OPERATOR)) {
      TAG_T *z;
      if (TAX (p) != NO_TAG) {
        USE (TAX (p)) = A68_TRUE;
      }
      if ((z = find_tag_global (TABLE (p), PRIO_SYMBOL, NSYMBOL (p))) != NO_TAG) {
        USE (z) = A68_TRUE;
      }
    } else if (IS (p, INDICANT)) {
      TAG_T *z = find_tag_global (TABLE (p), INDICANT, NSYMBOL (p));
      if (z != NO_TAG) {
        TAX (p) = z;
        USE (z) = A68_TRUE;
      }
    } else if (IS (p, IDENTIFIER)) {
      if (TAX (p) != NO_TAG) {
        USE (TAX (p)) = A68_TRUE;
      }
    }
  }
}

//! @brief Check a single tag.

static void unused (TAG_T * s)
{
  for (; s != NO_TAG; FORWARD (s)) {
    if (LINE_NUMBER (NODE (s)) > 0 && !USE (s)) {
      diagnostic (A68_WARNING, NODE (s), WARNING_TAG_UNUSED, NODE (s));
    }
  }
}

//! @brief Driver for traversing tree and warn for unused tags.

void warn_for_unused_tags (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE) {
      if (is_new_lexical_level (p) && ATTRIBUTE (TABLE (SUB (p))) != ENVIRON_SYMBOL) {
        unused (OPERATORS (TABLE (SUB (p))));
        unused (PRIO (TABLE (SUB (p))));
        unused (IDENTIFIERS (TABLE (SUB (p))));
        unused (LABELS (TABLE (SUB (p))));
        unused (INDICANTS (TABLE (SUB (p))));
      }
    }
    warn_for_unused_tags (SUB (p));
  }
}

//! @brief Mark jumps and procedured jumps.

void jumps_from_procs (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PROCEDURING)) {
      NODE_T *u = SUB_SUB (p);
      if (IS (u, GOTO_SYMBOL)) {
        FORWARD (u);
      }
      USE (TAX (u)) = A68_TRUE;
    } else if (IS (p, JUMP)) {
      NODE_T *u = SUB (p);
      if (IS (u, GOTO_SYMBOL)) {
        FORWARD (u);
      }
      if ((TAX (u) == NO_TAG) && (MOID (u) == NO_MOID) && (find_tag_global (TABLE (u), LABEL, NSYMBOL (u)) == NO_TAG)) {
        (void) add_tag (TABLE (u), LABEL, u, NO_MOID, LOCAL_LABEL);
        diagnostic (A68_ERROR, u, ERROR_UNDECLARED_TAG);
      } else {
        USE (TAX (u)) = A68_TRUE;
      }
    } else {
      jumps_from_procs (SUB (p));
    }
  }
}

//! @brief Assign offset tags.

static ADDR_T assign_offset_tags (TAG_T * t, ADDR_T base)
{
  ADDR_T sum = base;
  for (; t != NO_TAG; FORWARD (t)) {
    ABEND (MOID (t) == NO_MOID, ERROR_INTERNAL_CONSISTENCY, NSYMBOL (NODE (t)));
    SIZE (t) = moid_size (MOID (t));
    if (VALUE (t) == NO_TEXT) {
      OFFSET (t) = sum;
      sum += SIZE (t);
    }
  }
  return sum;
}

//! @brief Assign offsets table.

void assign_offsets_table (TABLE_T * c)
{
  AP_INCREMENT (c) = assign_offset_tags (IDENTIFIERS (c), 0);
  AP_INCREMENT (c) = assign_offset_tags (OPERATORS (c), AP_INCREMENT (c));
  AP_INCREMENT (c) = assign_offset_tags (ANONYMOUS (c), AP_INCREMENT (c));
  AP_INCREMENT (c) = A68_ALIGN (AP_INCREMENT (c));
}

//! @brief Assign offsets.

void assign_offsets (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE && is_new_lexical_level (p)) {
      assign_offsets_table (TABLE (SUB (p)));
    }
    assign_offsets (SUB (p));
  }
}

//! @brief Assign offsets packs in moid list.

void assign_offsets_packs (MOID_T * q)
{
  for (; q != NO_MOID; FORWARD (q)) {
    if (EQUIVALENT (q) == NO_MOID && IS (q, STRUCT_SYMBOL)) {
      PACK_T *p = PACK (q);
      ADDR_T offset = 0;
      for (; p != NO_PACK; FORWARD (p)) {
        SIZE (p) = moid_size (MOID (p));
        OFFSET (p) = offset;
        offset += SIZE (p);
      }
    }
  }
}
