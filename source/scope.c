/*!
\file scope.c
\brief static scope checker
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2006 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#include "algol68g.h"
#include "genie.h"

/*
Static scope checker. 
*/

typedef struct TUPLE_T TUPLE_T;
typedef struct SCOPE_T SCOPE_T;

struct TUPLE_T {
  int level;
  BOOL_T transient;
};

struct SCOPE_T {
  NODE_T *where;
  TUPLE_T tuple;
  SCOPE_T *next;
};

#define NOT_TRANSIENT 0x0
#define TRANSIENT 0x1

static void gather_scopes_for_youngest (NODE_T *, SCOPE_T **);
static void scope_statement (NODE_T *, SCOPE_T **);
static void scope_enclosed_clause (NODE_T *, SCOPE_T **);
static void scope_formula (NODE_T *, SCOPE_T **);
static void scope_routine_text (NODE_T *, SCOPE_T **);

/*!
\brief scope_make_tuple
\param e
\param t
\return
**/

static TUPLE_T scope_make_tuple (int e, int t)
{
  TUPLE_T z;
  z.level = e;
  z.transient = t;
  return (z);
}

/*!
\brief link scope information into the list
\param sl
\param p
\param tup
**/

static void scope_add (SCOPE_T ** sl, NODE_T * p, TUPLE_T tup)
{
  if (sl != NULL) {
    SCOPE_T *ns = (SCOPE_T *) get_temp_heap_space ((unsigned) SIZE_OF (SCOPE_T));
    ns->where = p;
    ns->tuple = tup;
    NEXT (ns) = *sl;
    *sl = ns;
  }
}

/*!
\brief scope_check
\param top
\param mask
\param dest
\return
**/

static BOOL_T scope_check (SCOPE_T * top, int mask, int dest)
{
  SCOPE_T *s;
  int errors = 0;
/* Transient names cannot be stored. */
  if (mask & TRANSIENT) {
    for (s = top; s != NULL; s = NEXT (s)) {
      if (s->tuple.transient & TRANSIENT) {
        diagnostic_node (A_ERROR, s->where, ERROR_TRANSIENT_NAME);
        s->where->error = A_TRUE;
        errors++;
      }
    }
  }
  for (s = top; s != NULL; s = NEXT (s)) {
    if (dest < s->tuple.level && !s->where->error) {
/* Potential scope violations. */
      if (MOID (s->where) == NULL) {
        diagnostic_node (A_WARNING, s->where, WARNING_SCOPE_STATIC_1, ATTRIBUTE (s->where));
      } else {
        diagnostic_node (A_WARNING, s->where, WARNING_SCOPE_STATIC_2, MOID (s->where), ATTRIBUTE (s->where));
      }
      s->where->error = A_TRUE;
      errors++;
    }
  }
  return (errors == 0);
}

/*!
\brief scope_check_multiple
\param top
\param mask
\param dest
\return
**/

static BOOL_T scope_check_multiple (SCOPE_T * top, int mask, SCOPE_T * dest)
{
  BOOL_T no_err = A_TRUE;
  for (; dest != NULL; dest = NEXT (dest)) {
    no_err &= scope_check (top, mask, dest->tuple.level);
  }
  return (no_err);
}

/*!
\brief check_identifier_usage
\param t
\param p
**/

static void check_identifier_usage (TAG_T * t, NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, IDENTIFIER) && TAX (p) == t && ATTRIBUTE (MOID (t)) != PROC_SYMBOL) {
      diagnostic_node (A_WARNING, p, WARNING_UNINITIALISED);
    }
    check_identifier_usage (t, SUB (p));
  }
}

/*!
\brief scope_find_youngest_outside
\param s
\param treshold
\return
**/

static TUPLE_T scope_find_youngest_outside (SCOPE_T * s, int treshold)
{
  TUPLE_T z = scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT);
  for (; s != NULL; s = NEXT (s)) {
    if (s->tuple.level > z.level && s->tuple.level <= treshold) {
      z = s->tuple;
    }
  }
  return (z);
}

/*!
\brief scope_find_youngest
\param s
\return
**/

static TUPLE_T scope_find_youngest (SCOPE_T * s)
{
  return (scope_find_youngest_outside (s, MAX_INT));
}

/* Routines for determining scope of ROUTINE TEXT or FORMAT TEXT. */

/*!
\brief get_declarer_elements
\param p
\param r
\param no_ref
**/

static void get_declarer_elements (NODE_T * p, SCOPE_T ** r, BOOL_T no_ref)
{
  if (p != NULL) {
    if (WHETHER (p, BOUNDS)) {
      gather_scopes_for_youngest (SUB (p), r);
    } else if (WHETHER (p, INDICANT)) {
      if (MOID (p) != NULL && TAX (p) != NULL && MOID (p)->has_rows && no_ref) {
        scope_add (r, p, scope_make_tuple (LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
      }
    } else if (WHETHER (p, REF_SYMBOL)) {
      get_declarer_elements (NEXT (p), r, A_FALSE);
    } else if (WHETHER (p, PROC_SYMBOL) || WHETHER (p, UNION_SYMBOL)) {
      ;
    } else {
      get_declarer_elements (SUB (p), r, no_ref);
      get_declarer_elements (NEXT (p), r, no_ref);
    }
  }
}

/*!
\brief gather_scopes_for_youngest
\param p
\param s
**/

static void gather_scopes_for_youngest (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if ((WHETHER (p, ROUTINE_TEXT) || WHETHER (p, FORMAT_TEXT)) && (TAX (p)->youngest_environ == PRIMAL_SCOPE)) {
      SCOPE_T *t = NULL;
      gather_scopes_for_youngest (SUB (p), &t);
      TAX (p)->youngest_environ = scope_find_youngest_outside (t, LEX_LEVEL (p)).level;
/* Direct link into list iso "gather_scopes_for_youngest (SUB (p), s);" */
      if (t != NULL) {
        SCOPE_T *u = t;
        while (NEXT (u) != NULL) {
          u = NEXT (u);
        }
        NEXT (u) = *s;
        (*s) = t;
      }
    } else if (WHETHER (p, IDENTIFIER) || WHETHER (p, OPERATOR)) {
      if (TAX (p) != NULL && LEX_LEVEL (TAX (p)) != PRIMAL_SCOPE) {
        scope_add (s, p, scope_make_tuple (LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
      }
    } else if (WHETHER (p, DECLARER)) {
      get_declarer_elements (p, s, A_TRUE);
    } else {
      gather_scopes_for_youngest (SUB (p), s);
    }
  }
}

/*!
\brief get_youngest_environs
\param p
**/

static void get_youngest_environs (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ROUTINE_TEXT) || WHETHER (p, FORMAT_TEXT)) {
      SCOPE_T *s = NULL;
      gather_scopes_for_youngest (SUB (p), &s);
      TAX (p)->youngest_environ = scope_find_youngest_outside (s, LEX_LEVEL (p)).level;
    } else {
      get_youngest_environs (SUB (p));
    }
  }
}

/*!
\brief bind_scope_to_tag
\param p
**/

static void bind_scope_to_tag (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, DEFINING_IDENTIFIER) && MOID (p) == MODE (FORMAT)) {
      if (WHETHER (NEXT (NEXT (p)), FORMAT_TEXT)) {
        TAX (p)->scope = TAX (NEXT (NEXT (p)))->youngest_environ;
        TAX (p)->scope_assigned = A_TRUE;
      }
      return;
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      if (WHETHER (NEXT (NEXT (p)), ROUTINE_TEXT)) {
        TAX (p)->scope = TAX (NEXT (NEXT (p)))->youngest_environ;
        TAX (p)->scope_assigned = A_TRUE;
      }
      return;
    } else {
      bind_scope_to_tag (SUB (p));
    }
  }
}

/*!
\brief bind_scope_to_tags
\param p
**/

static void bind_scope_to_tags (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, PROCEDURE_DECLARATION) || WHETHER (p, IDENTITY_DECLARATION)) {
      bind_scope_to_tag (SUB (p));
    } else {
      bind_scope_to_tags (SUB (p));
    }
  }
}

/*!
\brief scope_bounds
\param p
**/

static void scope_bounds (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      scope_statement (p, NULL);
    } else {
      scope_bounds (SUB (p));
    }
  }
}

/*!
\brief scope_declarer
\param p
**/

static void scope_declarer (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, BOUNDS)) {
      scope_bounds (SUB (p));
    } else if (WHETHER (p, INDICANT)) {
      ;
    } else if (WHETHER (p, REF_SYMBOL)) {
      scope_declarer (NEXT (p));
    } else if (WHETHER (p, PROC_SYMBOL) || WHETHER (p, UNION_SYMBOL)) {
      ;
    } else {
      scope_declarer (SUB (p));
      scope_declarer (NEXT (p));
    }
  }
}

/*!
\brief scope_identity_declaration
\param p
**/

static void scope_identity_declaration (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      ;
    } else if (WHETHER (p, DECLARER)) {
      scope_identity_declaration (NEXT (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      NODE_T *unit = NEXT (NEXT (p));
      SCOPE_T *s = NULL;
      int z = PRIMAL_SCOPE;
      if (ATTRIBUTE (MOID (TAX (p))) != PROC_SYMBOL) {
        check_identifier_usage (TAX (p), unit);
      }
      scope_statement (unit, &s);
      scope_check (s, TRANSIENT, LEX_LEVEL (p));
      z = scope_find_youngest (s).level;
      if (z < LEX_LEVEL (p)) {
        TAX (p)->scope = z;
        TAX (p)->scope_assigned = A_TRUE;
      }
    } else {
      scope_identity_declaration (SUB (p));
      scope_identity_declaration (NEXT (p));
    }
  }
}

/*!
\brief scope_variable_declaration
\param p
**/

static void scope_variable_declaration (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, VARIABLE_DECLARATION)) {
      ;
    } else if (WHETHER (p, DECLARER)) {
      scope_declarer (SUB (p));
      scope_variable_declaration (NEXT (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
        NODE_T *unit = NEXT (NEXT (p));
        SCOPE_T *s = NULL;
        check_identifier_usage (TAX (p), unit);
        scope_statement (unit, &s);
        scope_check (s, TRANSIENT, LEX_LEVEL (p));
      }
    } else {
      scope_variable_declaration (SUB (p));
      scope_variable_declaration (NEXT (p));
    }
  }
}

/*!
\brief scope_procedure_declaration
\param p
**/

static void scope_procedure_declaration (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, PROCEDURE_DECLARATION) || WHETHER (p, PROCEDURE_VARIABLE_DECLARATION) || WHETHER (p, BRIEF_OPERATOR_DECLARATION) || WHETHER (p, OPERATOR_DECLARATION)) {
      ;
    } else if (WHETHER (p, DEFINING_IDENTIFIER) || WHETHER (p, DEFINING_OPERATOR)) {
      SCOPE_T *s = NULL;
/*
	  scope_routine_text (NEXT (NEXT (p)), &s);
*/
      scope_statement (NEXT (NEXT (p)), &s);
      scope_check (s, NOT_TRANSIENT, LEX_LEVEL (p));
    } else {
      scope_procedure_declaration (SUB (p));
      scope_procedure_declaration (NEXT (p));
    }
  }
}

/*!
\brief scope_declaration_list
\param p
**/

static void scope_declaration_list (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      scope_identity_declaration (SUB (p));
    } else if (WHETHER (p, VARIABLE_DECLARATION)) {
      scope_variable_declaration (SUB (p));
    } else if (WHETHER (p, MODE_DECLARATION)) {
      scope_declarer (SUB (p));
    } else if (WHETHER (p, PRIORITY_DECLARATION)) {
      ;
    } else if (WHETHER (p, PROCEDURE_DECLARATION)) {
      scope_procedure_declaration (SUB (p));
    } else if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      scope_procedure_declaration (SUB (p));
    } else if (WHETHER (p, BRIEF_OPERATOR_DECLARATION) || WHETHER (p, OPERATOR_DECLARATION)) {
      scope_procedure_declaration (SUB (p));
    } else {
      scope_declaration_list (SUB (p));
      scope_declaration_list (NEXT (p));
    }
  }
}

/*!
\brief scope_arguments
\param p
**/

static void scope_arguments (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      SCOPE_T *s = NULL;
      scope_statement (p, &s);
      scope_check (s, TRANSIENT, LEX_LEVEL (p));
    } else {
      scope_arguments (SUB (p));
    }
  }
}

/*!
\brief whether_transient_row
\param m
**/

static BOOL_T whether_transient_row (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (WHETHER (SUB (m), FLEX_SYMBOL));
  } else {
    return (A_FALSE);
  }
}

/*!
\brief whether_coercion
\param p
**/

BOOL_T whether_coercion (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DEPROCEDURING:
    case DEREFERENCING:
    case UNITING:
    case ROWING:
    case WIDENING:
    case VOIDING:
    case PROCEDURING:
      {
        return (A_TRUE);
      }
    default:
      {
        return (A_FALSE);
      }
    }
  } else {
    return (A_FALSE);
  }
}

/*!
\brief scope_coercion
\param p
\param s
**/

static void scope_coercion (NODE_T * p, SCOPE_T ** s)
{
  if (whether_coercion (p)) {
    if (WHETHER (p, VOIDING)) {
      scope_coercion (SUB (p), NULL);
    } else if (WHETHER (p, DEREFERENCING)) {
/* Leave this to the dynamic scope checker. */
      scope_coercion (SUB (p), NULL);
    } else if (WHETHER (p, DEPROCEDURING)) {
      scope_coercion (SUB (p), NULL);
    } else if (WHETHER (p, ROWING)) {
      scope_coercion (SUB (p), s);
      if (whether_transient_row (MOID (SUB (p)))) {
        scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
      }
    } else if (WHETHER (p, PROCEDURING)) {
/* Can only be a JUMP. */
      NODE_T *q = SUB (SUB (p));
      if (WHETHER (q, GOTO_SYMBOL)) {
        q = NEXT (q);
      }
      scope_add (s, q, scope_make_tuple (LEX_LEVEL (TAX (q)), NOT_TRANSIENT));
    } else {
      scope_coercion (SUB (p), s);
    }
  } else {
    scope_statement (p, s);
  }
}

/*!
\brief scope_format_text
\param p
\param s
**/

static void scope_format_text (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, FORMAT_PATTERN)) {
      scope_enclosed_clause (SUB (NEXT_SUB (p)), s);
    } else if (WHETHER (p, FORMAT_ITEM_G) && NEXT (p) != NULL) {
      scope_enclosed_clause (SUB (NEXT (p)), s);
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      scope_enclosed_clause (SUB (NEXT_SUB (p)), s);
    } else {
      scope_format_text (SUB (p), s);
    }
  }
}

/*!
\brief whether_transient_selection
\param m
**/

static BOOL_T whether_transient_selection (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (whether_transient_selection (SUB (m)));
  } else {
    return (WHETHER (m, FLEX_SYMBOL));
  }
}

/*!
\brief scope_operand
\param p
\param s
**/

static void scope_operand (NODE_T * p, SCOPE_T ** s)
{
  if (WHETHER (p, MONADIC_FORMULA)) {
    scope_operand (NEXT_SUB (p), s);
  } else if (WHETHER (p, FORMULA)) {
    scope_formula (p, s);
  } else if (WHETHER (p, SECONDARY)) {
    scope_statement (SUB (p), s);
  }
}

/*!
\brief scope_formula
\param p
\param s
**/

static void scope_formula (NODE_T * p, SCOPE_T ** s)
{
  NODE_T *q = SUB (p);
  SCOPE_T *s2 = NULL;
  scope_operand (q, &s2);
  scope_check (s2, TRANSIENT, LEX_LEVEL (p));
  if (NEXT (q) != NULL) {
    SCOPE_T *s3 = NULL;
    scope_operand (NEXT (NEXT (q)), &s3);
    scope_check (s3, TRANSIENT, LEX_LEVEL (p));
  }
  (void) s;
}

/*!
\brief scope_routine_text
\param p
\param s
**/

static void scope_routine_text (NODE_T * p, SCOPE_T ** s)
{
  NODE_T *q = SUB (p), *routine = WHETHER (q, PARAMETER_PACK) ? NEXT (q) : q;
  SCOPE_T *x = NULL;
  TUPLE_T routine_tuple;
  scope_statement (NEXT (NEXT (routine)), &x);
  scope_check (x, TRANSIENT, LEX_LEVEL (p));
  routine_tuple = scope_make_tuple (TAX (p)->youngest_environ, NOT_TRANSIENT);
  scope_add (s, p, routine_tuple);
}

/*!
\brief scope_statement
\param p
\param s
**/

static void scope_statement (NODE_T * p, SCOPE_T ** s)
{
  if (whether_coercion (p)) {
    scope_coercion (p, s);
  } else if (WHETHER (p, PRIMARY) || WHETHER (p, SECONDARY) || WHETHER (p, TERTIARY) || WHETHER (p, UNIT)) {
    scope_statement (SUB (p), s);
  } else if (WHETHER (p, DENOTER) || WHETHER (p, NIHIL)) {
    scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
  } else if (WHETHER (p, IDENTIFIER)) {
    if (WHETHER (MOID (p), REF_SYMBOL)) {
      if (PRIO (TAX (p)) == PARAMETER_IDENTIFIER) {
        scope_add (s, p, scope_make_tuple (LEX_LEVEL (TAX (p)) - 1, NOT_TRANSIENT));
      } else {
        if (HEAP (TAX (p)) == HEAP_SYMBOL) {
          scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
        } else if (TAX (p)->scope_assigned) {
          scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
        } else {
          scope_add (s, p, scope_make_tuple (LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
        }
      }
    } else if (ATTRIBUTE (MOID (p)) == PROC_SYMBOL && TAX (p)->scope_assigned == A_TRUE) {
      scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
    } else if (MOID (p) == MODE (FORMAT) && TAX (p)->scope_assigned == A_TRUE) {
      scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
    }
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    scope_enclosed_clause (SUB (p), s);
  } else if (WHETHER (p, CALL)) {
    SCOPE_T *x = NULL;
    scope_statement (SUB (p), &x);
    scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
    scope_arguments (NEXT_SUB (p));
  } else if (WHETHER (p, SLICE)) {
    SCOPE_T *x = NULL;
    MOID_T *m = MOID (SUB (p));
    if (WHETHER (m, REF_SYMBOL)) {
      if (ATTRIBUTE (SUB (p)) == PRIMARY && ATTRIBUTE (SUB (SUB (p))) == SLICE) {
        scope_statement (SUB (p), s);
      } else {
        scope_statement (SUB (p), &x);
        scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
      }
      if (WHETHER (SUB (m), FLEX_SYMBOL)) {
        scope_add (s, SUB (p), scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
      }
      scope_bounds (SUB (NEXT_SUB (p)));
    }
    if (WHETHER (MOID (p), REF_SYMBOL)) {
      scope_add (s, p, scope_find_youngest (x));
    }
  } else if (WHETHER (p, FORMAT_TEXT)) {
    SCOPE_T *x = NULL;
    scope_format_text (SUB (p), &x);
    scope_add (s, p, scope_find_youngest (x));
  } else if (WHETHER (p, CAST)) {
/*    scope_enclosed_clause (SUB (NEXT_SUB (p)), &s); */
    SCOPE_T *x = NULL;
    scope_enclosed_clause (SUB (NEXT_SUB (p)), &x);
    scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
    scope_add (s, p, scope_find_youngest (x));
  } else if (WHETHER (p, SELECTION)) {
    SCOPE_T *ns = NULL;
    scope_statement (NEXT_SUB (p), &ns);
    scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (p));
    if (whether_transient_selection (MOID (NEXT_SUB (p)))) {
      scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
    }
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, GENERATOR)) {
    if (WHETHER (SUB (p), LOC_SYMBOL)) {
      scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), NOT_TRANSIENT));
    } else {
      scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
    }
    scope_declarer (SUB (NEXT_SUB (p)));
  } else if (WHETHER (p, FORMULA)) {
    scope_formula (p, s);
  } else if (WHETHER (p, ASSIGNATION)) {
    NODE_T *unit = NEXT (NEXT_SUB (p));
    SCOPE_T *ns = NULL, *nd = NULL;
    scope_statement (SUB (SUB (p)), &nd);
    scope_statement (unit, &ns);
    scope_check_multiple (ns, TRANSIENT, nd);
    scope_add (s, p, scope_make_tuple (scope_find_youngest (nd).level, NOT_TRANSIENT));
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    scope_routine_text (p, s);
  } else if (WHETHER (p, IDENTITY_RELATION) || WHETHER (p, AND_FUNCTION) || WHETHER (p, OR_FUNCTION)) {
    SCOPE_T *n = NULL;
    scope_statement (SUB (p), &n);
    scope_statement (NEXT (NEXT_SUB (p)), &n);
    scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  } else if (WHETHER (p, ASSERTION)) {
    SCOPE_T *n = NULL;
    scope_enclosed_clause (SUB (NEXT_SUB (p)), &n);
    scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  } else if (WHETHER (p, JUMP) || WHETHER (p, SKIP)) {
    ;
  }
}

/*!
\brief scope_statement_list
\param p
\param s
**/

static void scope_statement_list (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      scope_statement (p, s);
    } else {
      scope_statement_list (SUB (p), s);
    }
  }
}

/*!
\brief scope_serial_clause
\param p
\param s
\param terminator
**/

static void scope_serial_clause (NODE_T * p, SCOPE_T ** s, BOOL_T terminator)
{
  if (p != NULL) {
    if (WHETHER (p, INITIALISER_SERIES)) {
      scope_serial_clause (SUB (p), s, A_FALSE);
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (WHETHER (p, DECLARATION_LIST)) {
      scope_declaration_list (SUB (p));
    } else if (WHETHER (p, LABEL) || WHETHER (p, SEMI_SYMBOL) || WHETHER (p, EXIT_SYMBOL)) {
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (WHETHER (p, SERIAL_CLAUSE) || WHETHER (p, ENQUIRY_CLAUSE)) {
      if (NEXT (p) != NULL) {
        int j = ATTRIBUTE (NEXT (p));
        if (j == EXIT_SYMBOL || j == END_SYMBOL || j == CLOSE_SYMBOL) {
          scope_serial_clause (SUB (p), s, A_TRUE);
        } else {
          scope_serial_clause (SUB (p), s, A_FALSE);
        }
      } else {
        scope_serial_clause (SUB (p), s, A_TRUE);
      }
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (WHETHER (p, LABELED_UNIT)) {
      scope_serial_clause (SUB (p), s, terminator);
    } else if (WHETHER (p, UNIT)) {
      if (terminator) {
        scope_statement (p, s);
      } else {
        scope_statement (p, NULL);
      }
    }
  }
}

/*!
\brief scope_closed_clause
\param p
\param s
**/

static void scope_closed_clause (NODE_T * p, SCOPE_T ** s)
{
  if (p != NULL) {
    if (WHETHER (p, SERIAL_CLAUSE)) {
      scope_serial_clause (p, s, A_TRUE);
    } else if (WHETHER (p, OPEN_SYMBOL) || WHETHER (p, BEGIN_SYMBOL)) {
      scope_closed_clause (NEXT (p), s);
    }
  }
}

/*!
\brief scope_collateral_clause
\param p
\param s
**/

static void scope_collateral_clause (NODE_T * p, SCOPE_T ** s)
{
  if (p != NULL) {
    if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, 0) || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0))) {
      scope_statement_list (p, s);
    }
  }
}

/*!
\brief scope_conditional_clause
\param p
\param s
**/

static void scope_conditional_clause (NODE_T * p, SCOPE_T ** s)
{
  scope_serial_clause (NEXT_SUB (p), NULL, A_TRUE);
  FORWARD (p);
  scope_serial_clause (NEXT_SUB (p), s, A_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (WHETHER (p, ELSE_PART) || WHETHER (p, CHOICE)) {
      scope_serial_clause (NEXT_SUB (p), s, A_TRUE);
    } else if (WHETHER (p, ELIF_PART) || WHETHER (p, BRIEF_ELIF_IF_PART)) {
      scope_conditional_clause (SUB (p), s);
    }
  }
}

/*!
\brief scope_case_clause
\param p
\param s
**/

static void scope_case_clause (NODE_T * p, SCOPE_T ** s)
{
  SCOPE_T *n = NULL;
  scope_serial_clause (NEXT_SUB (p), &n, A_TRUE);
  scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  FORWARD (p);
  scope_statement_list (NEXT_SUB (p), s);
  if ((FORWARD (p)) != NULL) {
    if (WHETHER (p, OUT_PART) || WHETHER (p, CHOICE)) {
      scope_serial_clause (NEXT_SUB (p), s, A_TRUE);
    } else if (WHETHER (p, INTEGER_OUT_PART) || WHETHER (p, BRIEF_INTEGER_OUSE_PART)) {
      scope_case_clause (SUB (p), s);
    } else if (WHETHER (p, UNITED_OUSE_PART) || WHETHER (p, BRIEF_UNITED_OUSE_PART)) {
      scope_case_clause (SUB (p), s);
    }
  }
}

/*!
\brief scope_loop_clause
\param p
**/

static void scope_loop_clause (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, FOR_PART)) {
      scope_loop_clause (NEXT (p));
    } else if (WHETHER (p, FROM_PART) || WHETHER (p, BY_PART) || WHETHER (p, TO_PART)) {
      scope_statement (NEXT_SUB (p), NULL);
      scope_loop_clause (NEXT (p));
    } else if (WHETHER (p, WHILE_PART)) {
      scope_serial_clause (NEXT_SUB (p), NULL, A_TRUE);
      scope_loop_clause (NEXT (p));
    } else if (WHETHER (p, DO_PART) || WHETHER (p, ALT_DO_PART)) {
      NODE_T *do_p = NEXT_SUB (p), *un_p;
      if (WHETHER (do_p, SERIAL_CLAUSE)) {
        scope_serial_clause (do_p, NULL, A_TRUE);
        un_p = NEXT (do_p);
      } else {
        un_p = do_p;
      }
      if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
        scope_serial_clause (NEXT_SUB (un_p), NULL, A_TRUE);
      }
    }
  }
}

/*!
\brief scope_enclosed_clause
\param p
\param s
**/

static void scope_enclosed_clause (NODE_T * p, SCOPE_T ** s)
{
  if (WHETHER (p, ENCLOSED_CLAUSE)) {
    scope_enclosed_clause (SUB (p), s);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    scope_closed_clause (SUB (p), s);
  } else if (WHETHER (p, COLLATERAL_CLAUSE) || WHETHER (p, PARALLEL_CLAUSE)) {
    scope_collateral_clause (SUB (p), s);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    scope_conditional_clause (SUB (p), s);
  } else if (WHETHER (p, INTEGER_CASE_CLAUSE) || WHETHER (p, UNITED_CASE_CLAUSE)) {
    scope_case_clause (SUB (p), s);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    scope_loop_clause (SUB (p));
  }
}

/*!
\brief scope_checker
\param p
**/

void scope_checker (NODE_T * p)
{
/* First establish scopes of routine texts and format texts. */
  get_youngest_environs (p);
/* PROC and FORMAT identities can now be assigned a scope. */
  bind_scope_to_tags (p);
/* Now check evertyhing else. */
  scope_enclosed_clause (SUB (p), NULL);
}
