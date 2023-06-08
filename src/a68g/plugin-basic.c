//! @file compiler.c
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
#include "a68g-prelude.h"
#include "a68g-genie.h"
#include "a68g-listing.h"
#include "a68g-mp.h"
#include "a68g-optimiser.h"
#include "a68g-compiler.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

// Whether stuff is sufficiently "basic" to be compiled.

//! @brief Whether primitive mode, with simple C equivalent.

BOOL_T primitive_mode (MOID_T * m)
{
  if (m == M_INT) {
    return A68_TRUE;
  } else if (m == M_REAL) {
    return A68_TRUE;
  } else if (m == M_BOOL) {
    return A68_TRUE;
  } else if (m == M_CHAR) {
    return A68_TRUE;
  } else if (m == M_BITS) {
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether basic mode, for which units are compiled.

BOOL_T basic_mode (MOID_T * m)
{
  if (primitive_mode (m)) {
    return A68_TRUE;
  } else if (IS (m, REF_SYMBOL)) {
    if (IS (SUB (m), REF_SYMBOL) || IS (SUB (m), PROC_SYMBOL)) {
      return A68_FALSE;
    } else {
      return basic_mode (SUB (m));
    }
  } else if (IS (m, ROW_SYMBOL)) {
    return A68_FALSE;
// Not (fully) implemented yet.
// TODO: code to convert stacked units into an array.
//  if (primitive_mode (SUB (m))) {
//    return A68_TRUE;
//  } else if (IS (SUB (m), STRUCT_SYMBOL)) {
//    return basic_mode (SUB (m));
//  } else {
//    return A68_FALSE;
//  }
  } else if (IS (m, STRUCT_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      if (!primitive_mode (MOID (p))) {
        return A68_FALSE;
      }
    }
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether basic mode, which is not a row.

BOOL_T basic_mode_non_row (MOID_T * m)
{
  if (primitive_mode (m)) {
    return A68_TRUE;
  } else if (IS (m, REF_SYMBOL)) {
    if (IS (SUB (m), REF_SYMBOL) || IS (SUB (m), PROC_SYMBOL)) {
      return A68_FALSE;
    } else {
      return basic_mode_non_row (SUB (m));
    }
  } else if (IS (m, STRUCT_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      if (!primitive_mode (MOID (p))) {
        return A68_FALSE;
      }
    }
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether basic collateral clause.

BOOL_T basic_collateral (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, UNIT)) {
    return (BOOL_T) (basic_mode (MOID (p)) && basic_unit (SUB (p)) && basic_collateral (NEXT (p)));
  } else {
    return (BOOL_T) (basic_collateral (SUB (p)) && basic_collateral (NEXT (p)));
  }
}

//! @brief Whether basic serial clause.

void count_basic_units (NODE_T * p, int *total, int *good)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      (*total)++;
      if (basic_unit (p)) {
        (*good)++;
      }
    } else if (IS (p, DECLARATION_LIST)) {
      (*total)++;
    } else {
      count_basic_units (SUB (p), total, good);
    }
  }
}

//! @brief Whether basic serial clause.

BOOL_T basic_serial (NODE_T * p, int want)
{
  int total = 0, good = 0;
  count_basic_units (p, &total, &good);
  if (want > 0) {
    return total == want && total == good;
  } else {
    return total == good;
  }
}

//! @brief Whether basic indexer.

BOOL_T basic_indexer (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, TRIMMER)) {
    return A68_FALSE;
  } else if (IS (p, UNIT)) {
    return basic_unit (p);
  } else {
    return (BOOL_T) (basic_indexer (SUB (p)) && basic_indexer (NEXT (p)));
  }
}

//! @brief Whether basic slice.

BOOL_T basic_slice (NODE_T * p)
{
  if (IS (p, SLICE)) {
    NODE_T *prim = SUB (p);
    NODE_T *idf = stems_from (prim, IDENTIFIER);
    if (idf != NO_NODE) {
      NODE_T *indx = NEXT (prim);
      return basic_indexer (indx);
    }
  }
  return A68_FALSE;
}

//! @brief Whether basic argument.

BOOL_T basic_argument (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, UNIT)) {
    return (BOOL_T) (basic_mode (MOID (p)) && basic_unit (p) && basic_argument (NEXT (p)));
  } else {
    return (BOOL_T) (basic_argument (SUB (p)) && basic_argument (NEXT (p)));
  }
}

//! @brief Whether basic call.

BOOL_T basic_call (NODE_T * p)
{
  if (IS (p, CALL)) {
    NODE_T *prim = SUB (p);
    NODE_T *idf = stems_from (prim, IDENTIFIER);
    if (idf == NO_NODE) {
      return A68_FALSE;
    } else if (SUB_MOID (idf) == MOID (p)) {    // Prevent partial parametrisation
      int k;
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          NODE_T *args = NEXT (prim);
          return basic_argument (args);
        }
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether basic monadic formula.

BOOL_T basic_monadic_formula (NODE_T * p)
{
  if (IS (p, MONADIC_FORMULA)) {
    NODE_T *op = SUB (p);
    int k;
    for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k++) {
      if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
        NODE_T *rhs = NEXT (op);
        return basic_unit (rhs);
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether basic dyadic formula.

BOOL_T basic_formula (NODE_T * p)
{
  if (IS (p, FORMULA)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    if (op == NO_NODE) {
      return basic_monadic_formula (lhs);
    } else {
      int k;
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          NODE_T *rhs = NEXT (op);
          return (BOOL_T) (basic_unit (lhs) && basic_unit (rhs));
        }
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether basic conditional clause.

BOOL_T basic_conditional (NODE_T * p)
{
  if (!(IS (p, IF_PART) || IS (p, OPEN_PART))) {
    return A68_FALSE;
  }
  if (!basic_serial (NEXT_SUB (p), 1)) {
    return A68_FALSE;
  }
  FORWARD (p);
  if (!(IS (p, THEN_PART) || IS (p, CHOICE))) {
    return A68_FALSE;
  }
  if (!basic_serial (NEXT_SUB (p), 1)) {
    return A68_FALSE;
  }
  FORWARD (p);
  if (IS (p, ELSE_PART) || IS (p, CHOICE)) {
    return basic_serial (NEXT_SUB (p), 1);
  } else if (IS (p, FI_SYMBOL)) {
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether basic unit.

BOOL_T basic_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_FALSE;
  } else if (IS (p, UNIT)) {
    return basic_unit (SUB (p));
  } else if (IS (p, TERTIARY)) {
    return basic_unit (SUB (p));
  } else if (IS (p, SECONDARY)) {
    return basic_unit (SUB (p));
  } else if (IS (p, PRIMARY)) {
    return basic_unit (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    return basic_unit (SUB (p));
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 3) {
    if (IS (p, CLOSED_CLAUSE)) {
      return basic_serial (NEXT_SUB (p), 1);
    } else if (IS (p, COLLATERAL_CLAUSE)) {
      return basic_mode (MOID (p)) && basic_collateral (NEXT_SUB (p));
    } else if (IS (p, CONDITIONAL_CLAUSE)) {
      return basic_mode (MOID (p)) && basic_conditional (SUB (p));
    }
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 2) {
    if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), IDENTIFIER) != NO_NODE) {
      NODE_T *dst = SUB_SUB (p);
      NODE_T *src = NEXT_NEXT (dst);
      return (BOOL_T) basic_unit (src) && basic_mode_non_row (MOID (src));
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), SLICE) != NO_NODE) {
      NODE_T *dst = SUB_SUB (p);
      NODE_T *src = NEXT_NEXT (dst);
      NODE_T *slice = stems_from (dst, SLICE);
      return (BOOL_T) (IS (MOID (slice), REF_SYMBOL) && basic_slice (slice) && basic_unit (src) && basic_mode_non_row (MOID (src)));
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), SELECTION) != NO_NODE) {
      NODE_T *dst = SUB_SUB (p);
      NODE_T *src = NEXT_NEXT (dst);
      return (BOOL_T) (stems_from (NEXT_SUB (stems_from (dst, SELECTION)), IDENTIFIER) != NO_NODE && basic_unit (src) && basic_mode_non_row (MOID (dst)));
    } else if (IS (p, VOIDING)) {
      return basic_unit (SUB (p));
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SLICE)) {
      NODE_T *slice = stems_from (SUB (p), SLICE);
      return (BOOL_T) (basic_mode (MOID (p)) && IS (MOID (SUB (slice)), REF_SYMBOL) && basic_slice (slice));
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SELECTION)) {
      return (BOOL_T) (primitive_mode (MOID (p)) && BASIC (SUB (p), SELECTION));
    } else if (IS (p, WIDENING)) {
      if (WIDEN_TO (p, INT, REAL)) {
        return basic_unit (SUB (p));
      } else if (WIDEN_TO (p, REAL, COMPLEX)) {
        return basic_unit (SUB (p));
      } else {
        return A68_FALSE;
      }
    } else if (IS (p, CAST)) {
      return (BOOL_T) (folder_mode (MOID (SUB (p))) && basic_unit (NEXT_SUB (p)));
    } else if (IS (p, SLICE)) {
      return (BOOL_T) (basic_mode (MOID (p)) && basic_slice (p));
    } else if (IS (p, SELECTION)) {
      NODE_T *sec = stems_from (NEXT_SUB (p), IDENTIFIER);
      if (sec == NO_NODE) {
        return A68_FALSE;
      } else {
        return basic_mode_non_row (MOID (sec));
      }
    } else if (IS (p, IDENTITY_RELATION)) {
#define GOOD(p) (stems_from (p, IDENTIFIER) != NO_NODE && IS (MOID (stems_from ((p), IDENTIFIER)), REF_SYMBOL))
      NODE_T *lhs = SUB (p);
      NODE_T *rhs = NEXT_NEXT (lhs);
      if (GOOD (lhs) && GOOD (rhs)) {
        return A68_TRUE;
      } else if (GOOD (lhs) && stems_from (rhs, NIHIL) != NO_NODE) {
        return A68_TRUE;
      } else {
        return A68_FALSE;
      }
#undef GOOD
    }
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 1) {
    if (IS (p, IDENTIFIER)) {
      if (A68_STANDENV_PROC (TAX (p))) {
        int k;
        for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k++) {
          if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
            return A68_TRUE;
          }
        }
        return A68_FALSE;
      } else {
        return basic_mode (MOID (p));
      }
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), IDENTIFIER)) {
      return (BOOL_T) (basic_mode (MOID (p)) && BASIC (SUB (p), IDENTIFIER));
    } else if (IS (p, DENOTATION)) {
      return primitive_mode (MOID (p));
    } else if (IS (p, MONADIC_FORMULA)) {
      return (BOOL_T) (basic_mode (MOID (p)) && basic_monadic_formula (p));
    } else if (IS (p, FORMULA)) {
      return (BOOL_T) (basic_mode (MOID (p)) && basic_formula (p));
    } else if (IS (p, CALL)) {
      return (BOOL_T) (basic_mode (MOID (p)) && basic_call (p));
    }
  }
  return A68_FALSE;
}

