//! @file plugin-folder.c
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
//! Plugin compiler constant folder.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-genie.h"
#include "a68g-listing.h"
#include "a68g-mp.h"
#include "a68g-optimiser.h"
#include "a68g-plugin.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

// Constant folder                                                .
// Uses interpreter routines to calculate compile-time expressions.

//! @brief Whether mode is handled by the constant folder.

BOOL_T folder_mode (MOID_T * m)
{
  if (primitive_mode (m)) {
    return A68_TRUE;
  } else if (m == M_COMPLEX) {
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

// Constant unit check.

//! @brief Whether constant collateral clause.

BOOL_T constant_collateral (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, UNIT)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_unit (SUB (p)) && constant_collateral (NEXT (p)));
  } else {
    return (BOOL_T) (constant_collateral (SUB (p)) && constant_collateral (NEXT (p)));
  }
}

//! @brief Whether constant serial clause.

void count_constant_units (NODE_T * p, int *total, int *good)
{
  if (p != NO_NODE) {
    if (IS (p, UNIT)) {
      (*total)++;
      if (constant_unit (p)) {
        (*good)++;
      }
      count_constant_units (NEXT (p), total, good);
    } else {
      count_constant_units (SUB (p), total, good);
      count_constant_units (NEXT (p), total, good);
    }
  }
}

//! @brief Whether constant serial clause.

BOOL_T constant_serial (NODE_T * p, int want)
{
  int total = 0, good = 0;
  count_constant_units (p, &total, &good);
  if (want > 0) {
    return total == want && total == good;
  } else {
    return total == good;
  }
}

//! @brief Whether constant argument.

BOOL_T constant_argument (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, UNIT)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_unit (p) && constant_argument (NEXT (p)));
  } else {
    return (BOOL_T) (constant_argument (SUB (p)) && constant_argument (NEXT (p)));
  }
}

//! @brief Whether constant call.

BOOL_T constant_call (NODE_T * p)
{
  if (IS (p, CALL)) {
    NODE_T *prim = SUB (p);
    NODE_T *idf = stems_from (prim, IDENTIFIER);
    if (idf != NO_NODE) {
      int k;
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          NODE_T *args = NEXT (prim);
          return constant_argument (args);
        }
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether constant monadic formula.

BOOL_T constant_monadic_formula (NODE_T * p)
{
  if (IS (p, MONADIC_FORMULA)) {
    NODE_T *op = SUB (p);
    int k;
    for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k++) {
      if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
        NODE_T *rhs = NEXT (op);
        return constant_unit (rhs);
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether constant dyadic formula.

BOOL_T constant_formula (NODE_T * p)
{
  if (IS (p, FORMULA)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    if (op == NO_NODE) {
      return constant_monadic_formula (lhs);
    } else {
      int k;
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          NODE_T *rhs = NEXT (op);
          return (BOOL_T) (constant_unit (lhs) && constant_unit (rhs));
        }
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether constant unit.

BOOL_T constant_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_FALSE;
  } else if (IS (p, UNIT)) {
    return constant_unit (SUB (p));
  } else if (IS (p, TERTIARY)) {
    return constant_unit (SUB (p));
  } else if (IS (p, SECONDARY)) {
    return constant_unit (SUB (p));
  } else if (IS (p, PRIMARY)) {
    return constant_unit (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    return constant_unit (SUB (p));
  } else if (IS (p, CLOSED_CLAUSE)) {
    return constant_serial (NEXT_SUB (p), 1);
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    return folder_mode (MOID (p)) && constant_collateral (NEXT_SUB (p));
  } else if (IS (p, WIDENING)) {
    if (WIDEN_TO (p, INT, REAL)) {
      return constant_unit (SUB (p));
    } else if (WIDEN_TO (p, REAL, COMPLEX)) {
      return constant_unit (SUB (p));
    } else {
      return A68_FALSE;
    }
  } else if (IS (p, IDENTIFIER)) {
    if (A68_STANDENV_PROC (TAX (p))) {
      int k;
      for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
          return A68_TRUE;
        }
      }
      return A68_FALSE;
    } else {
// Possible constant folding.
      NODE_T *def = NODE (TAX (p));
      BOOL_T ret = A68_FALSE;
      if (STATUS (p) & COOKIE_MASK) {
        diagnostic (A68_WARNING, p, WARNING_UNINITIALISED);
      } else {
        STATUS (p) |= COOKIE_MASK;
        if (folder_mode (MOID (p)) && def != NO_NODE && NEXT (def) != NO_NODE && IS (NEXT (def), EQUALS_SYMBOL)) {
          ret = constant_unit (NEXT_NEXT (def));
        }
      }
      STATUS (p) &= !(COOKIE_MASK);
      return ret;
    }
  } else if (IS (p, DENOTATION)) {
    return primitive_mode (MOID (p));
  } else if (IS (p, MONADIC_FORMULA)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_monadic_formula (p));
  } else if (IS (p, FORMULA)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_formula (p));
  } else if (IS (p, CALL)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_call (p));
  } else if (IS (p, CAST)) {
    return (BOOL_T) (folder_mode (MOID (SUB (p))) && constant_unit (NEXT_SUB (p)));
  } else {
    return A68_FALSE;
  }
}

// Evaluate compile-time expressions using interpreter routines.

//! @brief Push denotation.

void push_denotation (NODE_T * p)
{
#define PUSH_DENOTATION(mode, decl) {\
  decl z;\
  NODE_T *s = (IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p));\
  if (genie_string_to_value_internal (p, MODE (mode), NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {\
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (mode));\
  }\
  PUSH_VALUE (p, VALUE (&z), decl);}

  if (MOID (p) == M_INT) {
    PUSH_DENOTATION (INT, A68_INT);
  } else if (MOID (p) == M_REAL) {
    PUSH_DENOTATION (REAL, A68_REAL);
  } else if (MOID (p) == M_BOOL) {
    PUSH_DENOTATION (BOOL, A68_BOOL);
  } else if (MOID (p) == M_CHAR) {
    if ((NSYMBOL (p))[0] == NULL_CHAR) {
      PUSH_VALUE (p, NULL_CHAR, A68_CHAR);
    } else {
      PUSH_VALUE (p, (NSYMBOL (p))[0], A68_CHAR);
    }
  } else if (MOID (p) == M_BITS) {
    PUSH_DENOTATION (BITS, A68_BITS);
  }
#undef PUSH_DENOTATION
}

//! @brief Push widening.

void push_widening (NODE_T * p)
{
  push_unit (SUB (p));
  if (WIDEN_TO (p, INT, REAL)) {
    A68_INT k;
    POP_OBJECT (p, &k, A68_INT);
    PUSH_VALUE (p, (REAL_T) VALUE (&k), A68_REAL);
  } else if (WIDEN_TO (p, REAL, COMPLEX)) {
    PUSH_VALUE (p, 0.0, A68_REAL);
  }
}

//! @brief Code collateral units.

void push_collateral_units (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    push_unit (p);
  } else {
    push_collateral_units (SUB (p));
    push_collateral_units (NEXT (p));
  }
}

//! @brief Code argument.

void push_argument (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      push_unit (p);
    } else {
      push_argument (SUB (p));
    }
  }
}

//! @brief Push unit.

void push_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  }
  if (IS (p, UNIT)) {
    push_unit (SUB (p));
  } else if (IS (p, TERTIARY)) {
    push_unit (SUB (p));
  } else if (IS (p, SECONDARY)) {
    push_unit (SUB (p));
  } else if (IS (p, PRIMARY)) {
    push_unit (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    push_unit (SUB (p));
  } else if (IS (p, CLOSED_CLAUSE)) {
    push_unit (SUB (NEXT_SUB (p)));
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    push_collateral_units (NEXT_SUB (p));
  } else if (IS (p, WIDENING)) {
    push_widening (p);
  } else if (IS (p, IDENTIFIER)) {
    if (A68_STANDENV_PROC (TAX (p))) {
      (void) (*(PROCEDURE (TAX (p)))) (p);
    } else {
// Possible constant folding 
      NODE_T *def = NODE (TAX (p));
      push_unit (NEXT_NEXT (def));
    }
  } else if (IS (p, DENOTATION)) {
    push_denotation (p);
  } else if (IS (p, MONADIC_FORMULA)) {
    NODE_T *op = SUB (p);
    NODE_T *rhs = NEXT (op);
    push_unit (rhs);
    (*(PROCEDURE (TAX (op)))) (op);
  } else if (IS (p, FORMULA)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    if (op == NO_NODE) {
      push_unit (lhs);
    } else {
      NODE_T *rhs = NEXT (op);
      push_unit (lhs);
      push_unit (rhs);
      (*(PROCEDURE (TAX (op)))) (op);
    }
  } else if (IS (p, CALL)) {
    NODE_T *prim = SUB (p);
    NODE_T *args = NEXT (prim);
    NODE_T *idf = stems_from (prim, IDENTIFIER);
    push_argument (args);
    (void) (*(PROCEDURE (TAX (idf)))) (p);
  } else if (IS (p, CAST)) {
    push_unit (NEXT_SUB (p));
  }
}

//! @brief Code constant folding.

void constant_folder (NODE_T * p, FILE_T out, int phase)
{
  if (phase == L_DECLARE) {
    if (MOID (p) == M_COMPLEX) {
      char acc[NAME_SIZE];
      A68_REAL re, im;
      (void) make_name (acc, CON, "", NUMBER (p));
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &im, A68_REAL);
      POP_OBJECT (p, &re, A68_REAL);
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_COMPLEX %s = {", acc));
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "{INIT_MASK, %.*g}", REAL_WIDTH + 2, VALUE (&re)));
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", {INIT_MASK, %.*g}", REAL_WIDTH + 2, VALUE (&im)));
      undent (out, "};\n");
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  } else if (phase == L_EXECUTE) {
    if (MOID (p) == M_COMPLEX) {
// Done at declaration stage 
    }
  } else if (phase == L_YIELD) {
    if (MOID (p) == M_INT) {
      A68_INT k;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &k, A68_INT);
      ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, A68_LD, VALUE (&k)) >= 0);
      undent (out, A68 (edit_line));
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_REAL) {
      A68_REAL x;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &x, A68_REAL);
// Mind overflowing or underflowing values.
      if (!finite (VALUE (&x))) {
        A68_OPT (code_errors)++;
        VALUE (&x) = 0.0;
      }
      if (VALUE (&x) == REAL_MAX) {
        undent (out, "REAL_MAX");
      } else if (VALUE (&x) == -REAL_MAX) {
        undent (out, "(-REAL_MAX)");
      } else {
        ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%.*g", REAL_WIDTH + 2, VALUE (&x)) >= 0);
        undent (out, A68 (edit_line));
      }
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_BOOL) {
      A68_BOOL b;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &b, A68_BOOL);
      ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", (VALUE (&b) ? "A68_TRUE" : "A68_FALSE")) >= 0);
      undent (out, A68 (edit_line));
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_CHAR) {
      A68_CHAR c;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &c, A68_CHAR);
      if (VALUE (&c) == '\'') {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'\\\''"));
      } else if (VALUE (&c) == '\\') {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'\\\\'"));
      } else if (VALUE (&c) == NULL_CHAR) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "NULL_CHAR"));
      } else if (IS_PRINT (VALUE (&c))) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'%c'", (CHAR_T) VALUE (&c)));
      } else {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(CHAR_T) %d", VALUE (&c)));
      }
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_BITS) {
      A68_BITS b;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &b, A68_BITS);
      ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "(UNSIGNED_T) 0x" A68_LX, VALUE (&b)) >= 0);
      undent (out, A68 (edit_line));
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_COMPLEX) {
      char acc[NAME_SIZE];
      (void) make_name (acc, CON, "", NUMBER (p));
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(A68_REAL *) %s", acc));
    }
  }
}
