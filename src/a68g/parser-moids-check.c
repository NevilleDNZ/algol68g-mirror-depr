//! @file parser-moids-check.c
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
//! Mode checker routines.

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
#include "a68g-moids.h"

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

//! @brief Mode check on bounds.

void mode_check_bounds (NODE_T * p)
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

void mode_check_declarer (NODE_T * p)
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

void mode_check_identity_declaration (NODE_T * p)
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

void mode_check_variable_declaration (NODE_T * p)
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

void mode_check_routine_text (NODE_T * p, SOID_T * y)
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

void mode_check_proc_declaration (NODE_T * p)
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

void mode_check_brief_op_declaration (NODE_T * p)
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

void mode_check_op_declaration (NODE_T * p)
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

void mode_check_declaration_list (NODE_T * p)
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

void mode_check_serial (SOID_T ** r, NODE_T * p, SOID_T * x, BOOL_T k)
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

void mode_check_serial_units (NODE_T * p, SOID_T * x, SOID_T * y, int att)
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

void mode_check_unit_list (SOID_T ** r, NODE_T * p, SOID_T * x)
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

void mode_check_struct_display (SOID_T ** r, NODE_T * p, PACK_T ** fields)
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

void mode_check_get_specified_moids (NODE_T * p, MOID_T * u)
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

void mode_check_specified_unit_list (SOID_T ** r, NODE_T * p, SOID_T * x, MOID_T * u)
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

void mode_check_united_case_parts (SOID_T ** ry, NODE_T * p, SOID_T * x)
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

void mode_check_united_case (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_unit_list_2 (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_closed (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_collateral (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_conditional_2 (SOID_T ** ry, NODE_T * p, SOID_T * x)
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

void mode_check_conditional (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_int_case_2 (SOID_T ** ry, NODE_T * p, SOID_T * x)
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

void mode_check_int_case (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_loop_2 (NODE_T * p, SOID_T * y)
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

void mode_check_loop (NODE_T * p, SOID_T * y)
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

TAG_T *search_table_for_operator (TAG_T * t, char *n, MOID_T * x, MOID_T * y)
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

TAG_T *search_table_chain_for_operator (TABLE_T * s, char *n, MOID_T * x, MOID_T * y)
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

TAG_T *find_operator (TABLE_T * s, char *n, MOID_T * x, MOID_T * y)
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

void mode_check_monadic_operator (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_monadic_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e;
  make_soid (&e, FIRM, NO_MOID, 0);
  mode_check_formula (NEXT (p), &e, y);
  mode_check_monadic_operator (p, &e, y);
  make_soid (y, SORT (x), MOID (y), 0);
}

//! @brief Mode check formula.

void mode_check_formula (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_assignation (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_identity_relation (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_bool_function (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_cast (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_assertion (NODE_T * p)
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

void mode_check_argument_list (SOID_T ** r, NODE_T * p, PACK_T ** x, PACK_T ** v, PACK_T ** w)
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

void mode_check_argument_list_2 (NODE_T * p, PACK_T * x, SOID_T * y, PACK_T ** v, PACK_T ** w)
{
  SOID_T *top_sl = NO_SOID;
  mode_check_argument_list (&top_sl, SUB (p), &x, v, w);
  make_soid (y, STRONG, pack_soids_in_moid (top_sl, STOWED_MODE), 0);
  free_soid_list (top_sl);
}

//! @brief Mode check meek int.

void mode_check_meek_int (NODE_T * p)
{
  SOID_T x, y;
  make_soid (&x, MEEK, M_INT, 0);
  mode_check_unit (p, &x, &y);
  if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&y), MOID (&x), MEEK, SAFE_DEFLEXING, 0);
  }
}

//! @brief Mode check trimmer.

void mode_check_trimmer (NODE_T * p)
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

void mode_check_indexer (NODE_T * p, int *subs, int *trims)
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

void mode_check_call (NODE_T * p, MOID_T * n, SOID_T * x, SOID_T * y)
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

void mode_check_slice (NODE_T * p, MOID_T * ori, SOID_T * x, SOID_T * y)
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

int mode_check_specification (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_selection (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_diagonal (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_transpose (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_row_column_function (NODE_T * p, SOID_T * x, SOID_T * y)
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

void mode_check_format_text (NODE_T * p)
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

void mode_check_unit (NODE_T * p, SOID_T * x, SOID_T * y)
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

