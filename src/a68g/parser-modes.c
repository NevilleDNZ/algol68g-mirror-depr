//! @file parser-modes.c
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
//! Mode table management.

#include "a68g.h"
#include "a68g-postulates.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"

// Mode collection, equivalencing and derived modes.

// Mode service routines.

//! @brief Count bounds in declarer in tree.

int count_bounds (NODE_T * p)
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

int count_sizety (NODE_T * p)
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

void resolve_equivalent (MOID_T ** m)
{
  while ((*m) != NO_MOID && EQUIVALENT ((*m)) != NO_MOID && (*m) != EQUIVALENT (*m)) {
    (*m) = EQUIVALENT (*m);
  }
}

//! @brief Reset moid.

void reset_moid_tree (NODE_T * p)
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

MOID_T *add_row (MOID_T ** p, int dim, MOID_T * sub, NODE_T * n, BOOL_T derivate)
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

void absorb_unions (MOID_T * m)
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

void contract_unions (MOID_T * m)
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

MOID_T *search_standard_mode (int sizety, NODE_T * indicant)
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

void get_mode_from_struct_field (NODE_T * p, PACK_T ** u)
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

void get_mode_from_formal_pack (NODE_T * p, PACK_T ** u)
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

void get_mode_from_union_pack (NODE_T * p, PACK_T ** u)
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

void get_mode_from_routine_pack (NODE_T * p, PACK_T ** u)
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

MOID_T *get_mode_from_routine_text (NODE_T * p)
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

MOID_T *get_mode_from_operator (NODE_T * p)
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

void get_mode_from_denotation (NODE_T * p, int sizety)
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

void get_modes_from_tree (NODE_T * p, int attribute)
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

void get_mode_from_proc_variables (NODE_T * p)
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

void get_mode_from_proc_var_declarations_tree (NODE_T * p)
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

BOOL_T is_well_formed (MOID_T * def, MOID_T * z, BOOL_T yin, BOOL_T yang, BOOL_T video)
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

void resolve_eq_members (MOID_T * q)
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

void resolve_eq_tags (TAG_T * z)
{
  for (; z != NO_TAG; FORWARD (z)) {
    if (MOID (z) != NO_MOID) {
      resolve_equivalent (&MOID (z));
    }
  }
}

//! @brief Bind modes in syntax tree.

void bind_modes (NODE_T * p)
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

void make_name_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p)
{
  if (src != NO_PACK) {
    MOID_T *z;
    make_name_pack (NEXT (src), dst, p);
    z = add_mode (p, REF_SYMBOL, 0, NO_NODE, MOID (src), NO_PACK);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

//! @brief Make flex multiple row pack.

void make_flex_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
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

MOID_T *make_name_struct (MOID_T * m, MOID_T ** p)
{
  PACK_T *u = NO_PACK;
  make_name_pack (PACK (m), &u, p);
  return add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u);
}

//! @brief Make name row.

MOID_T *make_name_row (MOID_T * m, MOID_T ** p)
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

void make_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NO_PACK) {
    make_multiple_row_pack (NEXT (src), dst, p, dim);
    (void) add_mode_to_pack (dst, add_row (p, dim, MOID (src), NO_NODE, A68_FALSE), TEXT (src), NODE (src));
  }
}

//! @brief Make flex multiple struct.

MOID_T *make_flex_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  PACK_T *u = NO_PACK;
  make_flex_multiple_row_pack (PACK (m), &u, p, dim);
  return add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u);
}

//! @brief Make multiple struct.

MOID_T *make_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  PACK_T *u = NO_PACK;
  make_multiple_row_pack (PACK (m), &u, p, dim);
  return add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u);
}

//! @brief Whether mode has row.

BOOL_T is_mode_has_row (MOID_T * m)
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

void compute_derived_modes (MODULE_T * mod)
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
