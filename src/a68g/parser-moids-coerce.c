//! @file parser-moids-coerce.c
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
//! Mode coercion driver.

#include "a68g.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"
#include "a68g-moids.h"

//! @brief Coerce bounds.

void coerce_bounds (NODE_T * p)
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

void coerce_declarer (NODE_T * p)
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

void coerce_identity_declaration (NODE_T * p)
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

void coerce_variable_declaration (NODE_T * p)
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

void coerce_routine_text (NODE_T * p)
{
  SOID_T w;
  if (IS (p, PARAMETER_PACK)) {
    FORWARD (p);
  }
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

//! @brief Coerce proc declaration.

void coerce_proc_declaration (NODE_T * p)
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

void coerce_op_declaration (NODE_T * p)
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

void coerce_brief_op_declaration (NODE_T * p)
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

void coerce_declaration_list (NODE_T * p)
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

void coerce_serial (NODE_T * p, SOID_T * q, BOOL_T k)
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

void coerce_closed (NODE_T * p, SOID_T * q)
{
  if (IS (p, SERIAL_CLAUSE)) {
    coerce_serial (p, q, A68_TRUE);
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, STOP)) {
    coerce_closed (NEXT (p), q);
  }
}

//! @brief Coerce conditional.

void coerce_conditional (NODE_T * p, SOID_T * q)
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

void coerce_unit_list (NODE_T * p, SOID_T * q)
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

void coerce_int_case (NODE_T * p, SOID_T * q)
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

void coerce_spec_unit_list (NODE_T * p, SOID_T * q)
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

void coerce_united_case (NODE_T * p, SOID_T * q)
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

void coerce_loop (NODE_T * p)
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

void coerce_struct_display (PACK_T ** r, NODE_T * p)
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

void coerce_collateral (NODE_T * p, SOID_T * q)
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

MOID_T *get_monad_moid (NODE_T * p)
{
  if (TAX (p) != NO_TAG && TAX (p) != A68_PARSER (error_tag)) {
    MOID (p) = MOID (TAX (p));
    return MOID (PACK (MOID (p)));
  } else {
    return M_ERROR;
  }
}

//! @brief Coerce monad oper.

void coerce_monad_oper (NODE_T * p, SOID_T * q)
{
  if (p != NO_NODE) {
    SOID_T z;
    make_soid (&z, FIRM, MOID (PACK (MOID (TAX (p)))), 0);
    INSERT_COERCIONS (NEXT (p), MOID (q), &z);
  }
}

//! @brief Coerce monad formula.

void coerce_monad_formula (NODE_T * p)
{
  SOID_T e;
  make_soid (&e, STRONG, get_monad_moid (p), 0);
  coerce_operand (NEXT (p), &e);
  coerce_monad_oper (p, &e);
}

//! @brief Coerce operand.

void coerce_operand (NODE_T * p, SOID_T * q)
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

void coerce_formula (NODE_T * p, SOID_T * q)
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

void coerce_assignation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, SOFT, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, SUB_MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

//! @brief Coerce relation.

void coerce_relation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, MOID (NEXT_NEXT (p)), 0);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

//! @brief Coerce bool function.

void coerce_bool_function (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, M_BOOL, 0);
  coerce_unit (SUB (p), &w);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

//! @brief Coerce assertion.

void coerce_assertion (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, MEEK, M_BOOL, 0);
  coerce_enclosed (SUB_NEXT (p), &w);
}

//! @brief Coerce selection.

void coerce_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

//! @brief Coerce cast.

void coerce_cast (NODE_T * p)
{
  SOID_T w;
  coerce_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_enclosed (NEXT (p), &w);
}

//! @brief Coerce argument list.

void coerce_argument_list (PACK_T ** r, NODE_T * p)
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

void coerce_call (NODE_T * p)
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

void coerce_meek_int (NODE_T * p)
{
  SOID_T x;
  make_soid (&x, MEEK, M_INT, 0);
  coerce_unit (p, &x);
}

//! @brief Coerce trimmer.

void coerce_trimmer (NODE_T * p)
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

void coerce_indexer (NODE_T * p)
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

void coerce_slice (NODE_T * p)
{
  SOID_T w;
  MOID_T *row;
  row = MOID (p);
  make_soid (&w, STRONG, row, 0);
  coerce_unit (SUB (p), &w);
  coerce_indexer (SUB_NEXT (p));
}

//! @brief Mode coerce diagonal.

void coerce_diagonal (NODE_T * p)
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

void coerce_transpose (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

//! @brief Mode coerce row or column function.

void coerce_row_column_function (NODE_T * p)
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

void coerce_format_text (NODE_T * p)
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

void coerce_unit (NODE_T * p, SOID_T * q)
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

//! @brief Driver for coercion inserions.

void coercion_inserter (NODE_T * p)
{
  if (IS (p, PARTICULAR_PROGRAM)) {
    SOID_T q;
    make_soid (&q, STRONG, M_VOID, 0);
    coerce_enclosed (SUB (p), &q);
  }
}

