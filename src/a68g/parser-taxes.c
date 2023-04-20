//! @file parser-taxes.c
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
//! Symbol table management.

#include "a68g.h"
#include "a68g-postulates.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"

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

void tax_tags (NODE_T *);
void tax_specifier_list (NODE_T *);
void tax_parameter_list (NODE_T *);
void tax_format_texts (NODE_T *);

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

BOOL_T is_mappable_routine (char *z)
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

TAG_T *bind_lengthety_identifier (char *u)
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

void bind_identifier_tag_to_symbol_table (NODE_T * p)
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

void bind_indicant_tag_to_symbol_table (NODE_T * p)
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

void tax_specifiers (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_specifiers (SUB (p));
    if (SUB (p) != NO_NODE && IS (p, SPECIFIER)) {
      tax_specifier_list (SUB (p));
    }
  }
}

//! @brief Enter specifier identifiers in the symbol table.

void tax_specifier_list (NODE_T * p)
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

void tax_parameters (NODE_T * p)
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

void tax_parameter_list (NODE_T * p)
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

void tax_for_identifiers (NODE_T * p)
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

void tax_routine_texts (NODE_T * p)
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

void tax_format_texts (NODE_T * p)
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

void tax_pictures (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_pictures (SUB (p));
    if (IS (p, PICTURE)) {
      TAX (p) = add_tag (TABLE (p), ANONYMOUS, p, M_COLLITEM, FORMAT_IDENTIFIER);
    }
  }
}

//! @brief Enter generators in the symbol table.

void tax_generators (NODE_T * p)
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

TAG_T *find_firmly_related_op (TABLE_T * c, char *n, MOID_T * l, MOID_T * r, TAG_T * self)
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

void test_firmly_related_ops_local (NODE_T * p, TAG_T * s)
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

void test_firmly_related_ops (NODE_T * p)
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

void already_declared (NODE_T * n, int a)
{
  if (find_tag_local (TABLE (n), a, NSYMBOL (n)) != NO_TAG) {
    diagnostic (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
}

//! @brief Whether tag has already been declared in this range.

void already_declared_hidden (NODE_T * n, int a)
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

int tab_qualifier (NODE_T * p)
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

void tax_identity_dec (NODE_T * p, MOID_T ** m)
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

void tax_variable_dec (NODE_T * p, int *q, MOID_T ** m)
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

void tax_proc_variable_dec (NODE_T * p, int *q)
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

void tax_proc_dec (NODE_T * p)
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

void check_operator_dec (NODE_T * p, MOID_T * u)
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

void tax_op_dec (NODE_T * p, MOID_T ** m)
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

void tax_brief_op_dec (NODE_T * p)
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

void tax_prio_dec (NODE_T * p)
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

void tax_tags (NODE_T * p)
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

void flood_with_symbol_table_restricted (NODE_T * p, TABLE_T * s)
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

void mark_mode (MOID_T * m)
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

void unused (TAG_T * s)
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

ADDR_T assign_offset_tags (TAG_T * t, ADDR_T base)
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
