/*!
\file taxes.c
\brief routines for TAXes and symbol tables
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

/* This file contains routines that work with TAXes and symbol tables. */

#include "algol68g.h"

static void tax_tags (NODE_T *);
static void tax_specifier_list (NODE_T *);
static void tax_parameter_list (NODE_T *);
static void tax_format_texts (NODE_T *);

/*!
\brief find a tag, searching symbol tables towards the root
\param table symbol table to search
\param a attribute of tag
\param name name of tag
\return type of tag, identifier or label or ...
**/

int first_tag_global (SYMBOL_TABLE_T * table, char *name)
{
  if (table != NULL) {
    TAG_T *s = NULL;
    for (s = table->identifiers; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (IDENTIFIER);
      }
    }
    for (s = table->indicants; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (INDICANT);
      }
    }
    for (s = table->labels; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (LABEL);
      }
    }
    for (s = table->operators; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (OP_SYMBOL);
      }
    }
    for (s = PRIO (table); s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (PRIO_SYMBOL);
      }
    }
    return (first_tag_global (PREVIOUS (table), name));
  } else {
    return (NULL_ATTRIBUTE);
  }
}

#define PORTCHECK_TAX(p, q) {\
  if (q == A68_FALSE) {\
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_TAG_NOT_PORTABLE, NULL);\
  }}

/*!
\brief check portability of sub tree
\param p position in tree
**/

void portcheck (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    portcheck (SUB (p));
    if (MODULE (INFO (p))->options.portcheck) {
      if (WHETHER (p, INDICANT) && MOID (p) != NULL) {
        PORTCHECK_TAX (p, MOID (p)->portable);
        MOID (p)->portable = A68_TRUE;
      } else if (WHETHER (p, IDENTIFIER)) {
        PORTCHECK_TAX (p, TAX (p)->portable);
        TAX (p)->portable = A68_TRUE;
      } else if (WHETHER (p, OPERATOR)) {
        PORTCHECK_TAX (p, TAX (p)->portable);
        TAX (p)->portable = A68_TRUE;
      }
    }
  }
}

/*!
\brief whether routine can be "lengthety-mapped"
\param z name of routine
\return same
**/

static BOOL_T whether_mappable_routine (char *z)
{
#define ACCEPT(u, v) {\
  if (strlen (u) >= strlen (v)) {\
    if (strcmp (&u[strlen (u) - strlen (v)], v) == 0) {\
      return (A68_TRUE);\
  }}}
/* Math routines. */
  ACCEPT (z, "arccos");
  ACCEPT (z, "arcsin");
  ACCEPT (z, "arctan");
  ACCEPT (z, "cbrt");
  ACCEPT (z, "cos");
  ACCEPT (z, "curt");
  ACCEPT (z, "exp");
  ACCEPT (z, "ln");
  ACCEPT (z, "log");
  ACCEPT (z, "pi");
  ACCEPT (z, "sin");
  ACCEPT (z, "sqrt");
  ACCEPT (z, "tan");
/* Random generator. */
  ACCEPT (z, "nextrandom");
  ACCEPT (z, "random");
/* BITS. */
  ACCEPT (z, "bitspack");
/* Enquiries. */
  ACCEPT (z, "maxint");
  ACCEPT (z, "intwidth");
  ACCEPT (z, "maxreal");
  ACCEPT (z, "realwidth");
  ACCEPT (z, "expwidth");
  ACCEPT (z, "maxbits");
  ACCEPT (z, "bitswidth");
  ACCEPT (z, "byteswidth");
  ACCEPT (z, "smallreal");
  return (A68_FALSE);
#undef ACCEPT
}

/*!
\brief map "short sqrt" onto "sqrt" etcetera
\param u name of routine
\return tag to map onto
**/

static TAG_T *bind_lengthety_identifier (char *u)
{
#define CAR(u, v) (strncmp (u, v, strlen(v)) == 0)
/*
We can only map routines blessed by "whether_mappable_routine", so there is no
"short print" or "long char in string".
*/
  if (CAR (u, "short")) {
    do {
      char *v;
      TAG_T *w;
      u = &u[strlen ("short")];
      v = add_token (&top_token, u)->text;
      w = find_tag_local (stand_env, IDENTIFIER, v);
      if (w != NULL && whether_mappable_routine (v)) {
        return (w);
      }
    } while (CAR (u, "short"));
  } else if (CAR (u, "long")) {
    do {
      char *v;
      TAG_T *w;
      u = &u[strlen ("long")];
      v = add_token (&top_token, u)->text;
      w = find_tag_local (stand_env, IDENTIFIER, v);
      if (w != NULL && whether_mappable_routine (v)) {
        return (w);
      }
    } while (CAR (u, "long"));
  }
  return (NULL);
#undef CAR
}

/*!
\brief bind identifier tags to the symbol table
\param p position in tree
**/

static void bind_identifier_tag_to_symbol_table (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    bind_identifier_tag_to_symbol_table (SUB (p));
    if (whether_one_of (p, IDENTIFIER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      int att = first_tag_global (SYMBOL_TABLE (p), SYMBOL (p));
      if (att != NULL_ATTRIBUTE) {
        TAG_T *z = find_tag_global (SYMBOL_TABLE (p), att, SYMBOL (p));
        if (att == IDENTIFIER && z != NULL) {
          MOID (p) = MOID (z);
        } else if (att == LABEL && z != NULL) {
          ;
        } else if ((z = bind_lengthety_identifier (SYMBOL (p))) != NULL) {
          MOID (p) = MOID (z);
        } else {
          diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          z = add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
          MOID (p) = MODE (ERROR);
        }
        TAX (p) = z;
        if (WHETHER (p, DEFINING_IDENTIFIER)) {
          NODE (z) = p;
        }
      }
    }
  }
}

/*!
\brief bind indicant tags to the symbol table
\param p position in tree
**/

static void bind_indicant_tag_to_symbol_table (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    bind_indicant_tag_to_symbol_table (SUB (p));
    if (whether_one_of (p, INDICANT, DEFINING_INDICANT, NULL_ATTRIBUTE)) {
      TAG_T *z = find_tag_global (SYMBOL_TABLE (p), INDICANT, SYMBOL (p));
      if (z != NULL) {
        MOID (p) = MOID (z);
        TAX (p) = z;
        if (WHETHER (p, DEFINING_INDICANT)) {
          NODE (z) = p;
        }
      }
    }
  }
}

/*!
\brief enter specifier identifiers in the symbol table
\param p position in tree
**/

static void tax_specifiers (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_specifiers (SUB (p));
    if (SUB (p) != NULL && WHETHER (p, SPECIFIER)) {
      tax_specifier_list (SUB (p));
    }
  }
}

/*!
\brief enter specifier identifiers in the symbol table
\param p position in tree
**/

static void tax_specifier_list (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, OPEN_SYMBOL)) {
      tax_specifier_list (NEXT (p));
    } else if (whether_one_of (p, CLOSE_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else if (WHETHER (p, IDENTIFIER)) {
      TAG_T *z = add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, NULL, SPECIFIER_IDENTIFIER);
      HEAP (z) = LOC_SYMBOL;
    } else if (WHETHER (p, DECLARER)) {
      tax_specifiers (SUB (p));
      tax_specifier_list (NEXT (p));
/* last identifier entry is identifier with this declarer. */
      if (SYMBOL_TABLE (p)->identifiers != NULL && PRIO (SYMBOL_TABLE (p)->identifiers) == SPECIFIER_IDENTIFIER)
        MOID (SYMBOL_TABLE (p)->identifiers) = MOID (p);
    }
  }
}

/*!
\brief enter parameter identifiers in the symbol table
\param p position in tree
**/

static void tax_parameters (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL) {
      tax_parameters (SUB (p));
      if (WHETHER (p, PARAMETER_PACK)) {
        tax_parameter_list (SUB (p));
      }
    }
  }
}

/*!
\brief enter parameter identifiers in the symbol table
\param p position in tree
**/

static void tax_parameter_list (NODE_T * p)
{
  if (p != NULL) {
    if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_parameter_list (NEXT (p));
    } else if (WHETHER (p, CLOSE_SYMBOL)) {
      ;
    } else if (whether_one_of (p, PARAMETER_LIST, PARAMETER, NULL_ATTRIBUTE)) {
      tax_parameter_list (NEXT (p));
      tax_parameter_list (SUB (p));
    } else if (WHETHER (p, IDENTIFIER)) {
/* parameters are always local. */
      HEAP (add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, NULL, PARAMETER_IDENTIFIER)) = LOC_SYMBOL;
    } else if (WHETHER (p, DECLARER)) {
      TAG_T *s;
      tax_parameter_list (NEXT (p));
/* last identifier entries are identifiers with this declarer. */
      for (s = SYMBOL_TABLE (p)->identifiers; s != NULL && MOID (s) == NULL; FORWARD (s)) {
        MOID (s) = MOID (p);
      }
      tax_parameters (SUB (p));
    }
  }
}

/*!
\brief enter FOR identifiers in the symbol table
\param p position in tree
**/

static void tax_for_identifiers (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_for_identifiers (SUB (p));
    if (WHETHER (p, FOR_SYMBOL)) {
      if ((FORWARD (p)) != NULL) {
        (void) add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (INT), LOOP_IDENTIFIER);
      }
    }
  }
}

/*!
\brief enter routine texts in the symbol table
\param p position in tree
**/

static void tax_routine_texts (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_routine_texts (SUB (p));
    if (WHETHER (p, ROUTINE_TEXT)) {
      TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, MOID (p), ROUTINE_TEXT);
      TAX (p) = z;
      HEAP (z) = LOC_SYMBOL;
      USE (z) = A68_TRUE;
    }
  }
}

/*!
\brief enter format texts in the symbol table
\param p position in tree
**/

static void tax_format_texts (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_format_texts (SUB (p));
    if (WHETHER (p, FORMAT_TEXT)) {
      TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, MODE (FORMAT), FORMAT_TEXT);
      TAX (p) = z;
      USE (z) = A68_TRUE;
    } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL) && NEXT (p) != NULL) {
      TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, MODE (FORMAT), FORMAT_IDENTIFIER);
      TAX (p) = z;
      USE (z) = A68_TRUE;
    }
  }
}

/*!
\brief enter FORMAT pictures in the symbol table
\param p position in tree
**/

static void tax_pictures (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_pictures (SUB (p));
    if (WHETHER (p, PICTURE)) {
      TAX (p) = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, MODE (COLLITEM), FORMAT_IDENTIFIER);
    }
  }
}

/*!
\brief enter generators in the symbol table
\param p position in tree
**/

static void tax_generators (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_generators (SUB (p));
    if (WHETHER (p, GENERATOR)) {
      if (WHETHER (SUB (p), LOC_SYMBOL)) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, SUB (MOID (SUB (p))), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        TAX (p) = z;
      }
    }
  }
}

/*!
\brief consistency check on fields in structured modes
\param p position in tree
**/

static void structure_fields_test (NODE_T * p)
{
/* STRUCT (REAL x, INT n, REAL x) is wrong. */
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m = SYMBOL_TABLE (SUB (p))->moids;
      for (; m != NULL; FORWARD (m)) {
        if (WHETHER (m, STRUCT_SYMBOL) && m->equivalent_mode == NULL) {
/* check on identically named fields. */
          PACK_T *s = PACK (m);
          for (; s != NULL; FORWARD (s)) {
            PACK_T *t = NEXT (s);
            BOOL_T k = A68_TRUE;
            for (t = NEXT (s); t != NULL && k; FORWARD (t)) {
              if (s->text == t->text) {
                diagnostic_node (A68_ERROR, p, ERROR_MULTIPLE_FIELD);
                while (NEXT (s) != NULL && NEXT (s)->text == t->text) {
                  FORWARD (s);
                }
                k = A68_FALSE;
              }
            }
          }
        }
      }
    }
    structure_fields_test (SUB (p));
  }
}

/*!
\brief incestuous union test
\param p position in tree
**/

static void incestuous_union_test (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *symbol_table = SYMBOL_TABLE (SUB (p));
      MOID_T *m = symbol_table->moids;
      for (; m != NULL; FORWARD (m)) {
        if (WHETHER (m, UNION_SYMBOL) && m->equivalent_mode == NULL) {
          PACK_T *s = PACK (m);
          BOOL_T x = A68_TRUE;
/* Discard unions with one member. */
          if (count_pack_members (s) == 1) {
            SOID_T a;
            make_soid (&a, NO_SORT, m, 0);
            diagnostic_node (A68_ERROR, NODE (m), ERROR_COMPONENT_NUMBER, m);
            x = A68_FALSE;
          }
/* Discard unions with firmly related modes. */
          for (; s != NULL && x; FORWARD (s)) {
            PACK_T *t = NEXT (s);
            for (; t != NULL; FORWARD (t)) {
              if (MOID (t) != MOID (s)) {
                if (whether_firm (MOID (s), MOID (t))) {
                  diagnostic_node (A68_ERROR, p, ERROR_COMPONENT_RELATED, m);
                }
              }
            }
          }
/* Discard unions with firmly related subsets. */
          for (s = PACK (m); s != NULL && x; FORWARD (s)) {
            MOID_T *n = depref_completely (MOID (s));
            if (WHETHER (n, UNION_SYMBOL)
                && whether_subset (n, m, NO_DEFLEXING)) {
              SOID_T z;
              make_soid (&z, NO_SORT, n, 0);
              diagnostic_node (A68_ERROR, p, ERROR_SUBSET_RELATED, m, n);
            }
          }
        }
      }
    }
    incestuous_union_test (SUB (p));
  }
}

/*!
\brief find a firmly related operator for operands
\param c symbol table
\param n name of operator
\param l left operand mode
\param r right operand mode
\param self own tag of "n", as to not relate to itself
\return pointer to entry in table
**/

static TAG_T *find_firmly_related_op (SYMBOL_TABLE_T * c, char *n, MOID_T * l, MOID_T * r, TAG_T * self)
{
  if (c != NULL) {
    TAG_T *s = c->operators;
    for (; s != NULL; FORWARD (s)) {
      if (s != self && SYMBOL (NODE (s)) == n) {
        PACK_T *t = PACK (MOID (s));
        if (t != NULL && whether_firm (MOID (t), l)) {
/* catch monadic operator. */
          if ((FORWARD (t)) == NULL) {
            if (r == NULL) {
              return (s);
            }
          } else {
/* catch dyadic operator. */
            if (r != NULL && whether_firm (MOID (t), r)) {
              return (s);
            }
          }
        }
      }
    }
  }
  return (NULL);
}

/*!
\brief check for firmly related operators in this range
\param p position in tree
\param s operator tag to start from
**/

static void test_firmly_related_ops_local (NODE_T * p, TAG_T * s)
{
  if (s != NULL) {
    PACK_T *u = PACK (MOID (s));
    MOID_T *l = MOID (u);
    MOID_T *r = NEXT (u) != NULL ? MOID (NEXT (u)) : NULL;
    TAG_T *t = find_firmly_related_op (TAG_TABLE (s), SYMBOL (NODE (s)), l, r, s);
    if (t != NULL) {
      if (TAG_TABLE (t) == stand_env) {
        diagnostic_node (A68_ERROR, p, ERROR_OPERATOR_RELATED, MOID (s), SYMBOL (NODE (s)), MOID (t), SYMBOL (NODE (t)), NULL);
        ABNORMAL_END (A68_TRUE, "standard environ error", NULL);
      } else {
        diagnostic_node (A68_ERROR, p, ERROR_OPERATOR_RELATED, MOID (s), SYMBOL (NODE (s)), MOID (t), SYMBOL (NODE (t)), NULL);
      }
    }
    if (NEXT (s) != NULL) {
      test_firmly_related_ops_local (p == NULL ? NULL : NODE (NEXT (s)), NEXT (s));
    }
  }
}

/*!
\brief find firmly related operators in this program
\param p position in tree
**/

static void test_firmly_related_ops (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      TAG_T *oops = SYMBOL_TABLE (SUB (p))->operators;
      if (oops != NULL) {
        test_firmly_related_ops_local (NODE (oops), oops);
      }
    }
    test_firmly_related_ops (SUB (p));
  }
}

/*!
\brief driver for the processing of TAXes
\param p position in tree
**/

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
  structure_fields_test (p);
  incestuous_union_test (p);
  test_firmly_related_ops (p);
  test_firmly_related_ops_local (NULL, stand_env->operators);
}

/*!
\brief whether tag has already been declared in this range
\param n name of tag
\param a attribute of tag
**/

static void already_declared (NODE_T * n, int a)
{
  if (find_tag_local (SYMBOL_TABLE (n), a, SYMBOL (n)) != NULL) {
    diagnostic_node (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
}

/*!
\brief whether tag has already been declared in this range
\param n name of tag
\param a attribute of tag
**/

static void already_declared_hidden (NODE_T * n, int a)
{
  TAG_T *s;
  if (find_tag_local (SYMBOL_TABLE (n), a, SYMBOL (n)) != NULL) {
    diagnostic_node (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
  if ((s = find_tag_global (PREVIOUS (SYMBOL_TABLE (n)), a, SYMBOL (n))) != NULL) {
    if (TAG_TABLE (s) == stand_env) {
      diagnostic_node (A68_WARNING, n, WARNING_HIDES_PRELUDE, MOID (s), SYMBOL (n));
    } else {
      diagnostic_node (A68_WARNING, n, WARNING_HIDES, SYMBOL (n));
    }
  }
}

/*!
\brief add tag to local symbol table
\param s table where to insert
\param a attribute
\param n name of tag
\param m mode of tag
\param p position in tree
\return entry in symbol table
**/

TAG_T *add_tag (SYMBOL_TABLE_T * s, int a, NODE_T * n, MOID_T * m, int p)
{
#define INSERT_TAG(l, n) {NEXT (n) = *(l); *(l) = (n);}
  if (s != NULL) {
    TAG_T *z = new_tag ();
    TAG_TABLE (z) = s;
    PRIO (z) = p;
    MOID (z) = m;
    NODE (z) = n;
/*    TAX(n) = z; */
    switch (a) {
    case IDENTIFIER:{
        already_declared_hidden (n, IDENTIFIER);
        already_declared_hidden (n, LABEL);
        INSERT_TAG (&s->identifiers, z);
        break;
      }
    case INDICANT:{
        already_declared_hidden (n, INDICANT);
        already_declared (n, OP_SYMBOL);
        already_declared (n, PRIO_SYMBOL);
        INSERT_TAG (&s->indicants, z);
        break;
      }
    case LABEL:{
        already_declared_hidden (n, LABEL);
        already_declared_hidden (n, IDENTIFIER);
        INSERT_TAG (&s->labels, z);
        break;
      }
    case OP_SYMBOL:{
        already_declared (n, INDICANT);
        INSERT_TAG (&s->operators, z);
        break;
      }
    case PRIO_SYMBOL:{
        already_declared (n, PRIO_SYMBOL);
        already_declared (n, INDICANT);
        INSERT_TAG (&PRIO (s), z);
        break;
      }
    case ANONYMOUS:{
        INSERT_TAG (&s->anonymous, z);
        break;
      }
    default:{
        ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "add tag");
      }
    }
    return (z);
  } else {
    return (NULL);
  }
#undef INSERT_TAG
}

/*!
\brief find a tag, searching symbol tables towards the root
\param table symbol table to search
\param a attribute of tag
\param name name of tag
\return entry in symbol table
**/

TAG_T *find_tag_global (SYMBOL_TABLE_T * table, int a, char *name)
{
  if (table != NULL) {
    TAG_T *s = NULL;
    switch (a) {
    case IDENTIFIER:{
        s = table->identifiers;
        break;
      }
    case INDICANT:{
        s = table->indicants;
        break;
      }
    case LABEL:{
        s = table->labels;
        break;
      }
    case OP_SYMBOL:{
        s = table->operators;
        break;
      }
    case PRIO_SYMBOL:{
        s = PRIO (table);
        break;
      }
    default:{
        ABNORMAL_END (A68_TRUE, "impossible state in find_tag_global", NULL);
        break;
      }
    }
    for (; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (s);
      }
    }
    return (find_tag_global (PREVIOUS (table), a, name));
  } else {
    return (NULL);
  }
}

/*!
\brief whether identifier or label global
\param table symbol table to search
\param name name of tag
\return attribute of tag
**/

int whether_identifier_or_label_global (SYMBOL_TABLE_T * table, char *name)
{
  if (table != NULL) {
    TAG_T *s;
    for (s = table->identifiers; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (IDENTIFIER);
      }
    }
    for (s = table->labels; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (LABEL);
      }
    }
    return (whether_identifier_or_label_global (PREVIOUS (table), name));
  } else {
    return (0);
  }
}

/*!
\brief find a tag, searching only local symbol table
\param table symbol table to search
\param a attribute of tag
\param name name of tag
\return entry in symbol table
**/

TAG_T *find_tag_local (SYMBOL_TABLE_T * table, int a, char *name)
{
  if (table != NULL) {
    TAG_T *s = NULL;
    if (a == OP_SYMBOL) {
      s = table->operators;
    } else if (a == PRIO_SYMBOL) {
      s = PRIO (table);
    } else if (a == IDENTIFIER) {
      s = table->identifiers;
    } else if (a == INDICANT) {
      s = table->indicants;
    } else if (a == LABEL) {
      s = table->labels;
    } else {
      ABNORMAL_END (A68_TRUE, "impossible state in find_tag_local", NULL);
    }
    for (; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (s);
      }
    }
  }
  return (NULL);
}

/*!
\brief whether context specifies HEAP or LOC for an identifier
\param p position in tree
\return attribute of generator
**/

static int tab_qualifier (NODE_T * p)
{
  if (p != NULL) {
    if (whether_one_of (p, UNIT, ASSIGNATION, TERTIARY, SECONDARY, GENERATOR, NULL_ATTRIBUTE)) {
      return (tab_qualifier (SUB (p)));
    } else if (whether_one_of (p, LOC_SYMBOL, HEAP_SYMBOL, NEW_SYMBOL, NULL_ATTRIBUTE)) {
      return (ATTRIBUTE (p) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
    } else {
      return (LOC_SYMBOL);
    }
  } else {
    return (LOC_SYMBOL);
  }
}

/*!
\brief enter identity declarations in the symbol table
\param p position in tree
\param m mode of identifiers to enter (picked from the left-most one in fi. INT i = 1, j = 2)
**/

static void tax_identity_dec (NODE_T * p, MOID_T ** m)
{
  if (p != NULL) {
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      tax_identity_dec (SUB (p), m);
      tax_identity_dec (NEXT (p), m);
    } else if (WHETHER (p, DECLARER)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_identity_dec (NEXT (p), m);
    } else if (WHETHER (p, COMMA_SYMBOL)) {
      tax_identity_dec (NEXT (p), m);
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (SYMBOL_TABLE (p), IDENTIFIER, SYMBOL (p));
      MOID (p) = *m;
      HEAP (entry) = LOC_SYMBOL;
      TAX (p) = entry;
      MOID (entry) = *m;
      if ((*m)->attribute == REF_SYMBOL) {
        HEAP (entry) = tab_qualifier (NEXT_NEXT (p));
      }
      tax_identity_dec (NEXT_NEXT (p), m);
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter variable declarations in the symbol table
\param p position in tree
\param q qualifier of generator (HEAP, LOC) picked from left-most identifier
\param m mode of identifiers to enter (picked from the left-most one in fi. INT i, j, k)
**/

static void tax_variable_dec (NODE_T * p, int *q, MOID_T ** m)
{
  if (p != NULL) {
    if (WHETHER (p, VARIABLE_DECLARATION)) {
      tax_variable_dec (SUB (p), q, m);
      tax_variable_dec (NEXT (p), q, m);
    } else if (WHETHER (p, DECLARER)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_variable_dec (NEXT (p), q, m);
    } else if (WHETHER (p, QUALIFIER)) {
      *q = ATTRIBUTE (SUB (p));
      tax_variable_dec (NEXT (p), q, m);
    } else if (WHETHER (p, COMMA_SYMBOL)) {
      tax_variable_dec (NEXT (p), q, m);
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (SYMBOL_TABLE (p), IDENTIFIER, SYMBOL (p));
      MOID (p) = *m;
      TAX (p) = entry;
      HEAP (entry) = *q;
      if (*q == LOC_SYMBOL) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, SUB (*m), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        BODY (entry) = z;
      } else {
        BODY (entry) = NULL;
      }
      MOID (entry) = *m;
      tax_variable_dec (NEXT (p), q, m);
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter procedure variable declarations in the symbol table
\param p position in tree
\param q qualifier of generator (HEAP, LOC) picked from left-most identifier
**/

static void tax_proc_variable_dec (NODE_T * p, int *q)
{
  if (p != NULL) {
    if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      tax_proc_variable_dec (SUB (p), q);
      tax_proc_variable_dec (NEXT (p), q);
    } else if (WHETHER (p, QUALIFIER)) {
      *q = ATTRIBUTE (SUB (p));
      tax_proc_variable_dec (NEXT (p), q);
    } else if (whether_one_of (p, PROC_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_proc_variable_dec (NEXT (p), q);
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (SYMBOL_TABLE (p), IDENTIFIER, SYMBOL (p));
      TAX (p) = entry;
      HEAP (entry) = *q;
      MOID (entry) = MOID (p);
      if (*q == LOC_SYMBOL) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, SUB (MOID (p)),
                            GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        BODY (entry) = z;
      } else {
        BODY (entry) = NULL;
      }
      tax_proc_variable_dec (NEXT (p), q);
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter procedure declarations in the symbol table
\param p position in tree
**/

static void tax_proc_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, PROCEDURE_DECLARATION)) {
      tax_proc_dec (SUB (p));
      tax_proc_dec (NEXT (p));
    } else if (whether_one_of (p, PROC_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_proc_dec (NEXT (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (SYMBOL_TABLE (p), IDENTIFIER, SYMBOL (p));
      MOID_T *m = MOID (NEXT_NEXT (p));
      MOID (p) = m;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      MOID (entry) = m;
      tax_proc_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief count number of operands in operator parameter list
\param p position in tree
\return same
**/

static int count_operands (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, DECLARER)) {
      return (count_operands (NEXT (p)));
    } else if (WHETHER (p, COMMA_SYMBOL)) {
      return (1 + count_operands (NEXT (p)));
    } else {
      return (count_operands (NEXT (p)) + count_operands (SUB (p)));
    }
  } else {
    return (0);
  }
}

/*!
\brief check validity of operator declaration
\param p position in tree
**/

static void check_operator_dec (NODE_T * p)
{
  int k = 0;
  NODE_T *pack = SUB (SUB (NEXT_NEXT (p)));     /* That's where the parameter pack is. */
  if (ATTRIBUTE (NEXT_NEXT (p)) != ROUTINE_TEXT) {
    pack = SUB (pack);
  }
  k = 1 + count_operands (pack);
  if (k < 1 && k > 2) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERAND_NUMBER);
    k = 0;
  }
  if (k == 1 && a68g_strchr (NOMADS, SYMBOL (p)[0]) != NULL) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
  } else if (k == 2 && !find_tag_global (SYMBOL_TABLE (p), PRIO_SYMBOL, SYMBOL (p))) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_DYADIC_PRIORITY);
  }
}

/*!
\brief enter operator declarations in the symbol table
\param p position in tree
\param m mode of operators to enter (picked from the left-most one)
**/

static void tax_op_dec (NODE_T * p, MOID_T ** m)
{
  if (p != NULL) {
    if (WHETHER (p, OPERATOR_DECLARATION)) {
      tax_op_dec (SUB (p), m);
      tax_op_dec (NEXT (p), m);
    } else if (WHETHER (p, OPERATOR_PLAN)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_op_dec (NEXT (p), m);
    } else if (WHETHER (p, OP_SYMBOL)) {
      tax_op_dec (NEXT (p), m);
    } else if (WHETHER (p, COMMA_SYMBOL)) {
      tax_op_dec (NEXT (p), m);
    } else if (WHETHER (p, DEFINING_OPERATOR)) {
      TAG_T *entry = SYMBOL_TABLE (p)->operators;
      check_operator_dec (p);
      while (entry != NULL && NODE (entry) != p) {
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

/*!
\brief enter brief operator declarations in the symbol table
\param p position in tree
**/

static void tax_brief_op_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, BRIEF_OPERATOR_DECLARATION)) {
      tax_brief_op_dec (SUB (p));
      tax_brief_op_dec (NEXT (p));
    } else if (whether_one_of (p, OP_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_brief_op_dec (NEXT (p));
    } else if (WHETHER (p, DEFINING_OPERATOR)) {
      TAG_T *entry = SYMBOL_TABLE (p)->operators;
      MOID_T *m = MOID (NEXT_NEXT (p));
      check_operator_dec (p);
      while (entry != NULL && NODE (entry) != p) {
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

/*!
\brief enter priority declarations in the symbol table
\param p position in tree
**/

static void tax_prio_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, PRIORITY_DECLARATION)) {
      tax_prio_dec (SUB (p));
      tax_prio_dec (NEXT (p));
    } else if (whether_one_of (p, PRIO_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_prio_dec (NEXT (p));
    } else if (WHETHER (p, DEFINING_OPERATOR)) {
      TAG_T *entry = PRIO (SYMBOL_TABLE (p));
      while (entry != NULL && NODE (entry) != p) {
        FORWARD (entry);
      }
      MOID (p) = NULL;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      tax_prio_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter TAXes in the symbol table
\param p position in tree
**/

static void tax_tags (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    int heap = LOC_SYMBOL;
    MOID_T *m = NULL;
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      tax_identity_dec (p, &m);
    } else if (WHETHER (p, VARIABLE_DECLARATION)) {
      tax_variable_dec (p, &heap, &m);
    } else if (WHETHER (p, PROCEDURE_DECLARATION)) {
      tax_proc_dec (p);
    } else if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      tax_proc_variable_dec (p, &heap);
    } else if (WHETHER (p, OPERATOR_DECLARATION)) {
      tax_op_dec (p, &m);
    } else if (WHETHER (p, BRIEF_OPERATOR_DECLARATION)) {
      tax_brief_op_dec (p);
    } else if (WHETHER (p, PRIORITY_DECLARATION)) {
      tax_prio_dec (p);
    } else {
      tax_tags (SUB (p));
    }
  }
}

/*!
\brief reset symbol table nest count
\param p position in tree
**/

void reset_symbol_table_nest_count (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE (SUB (p))->nest = symbol_table_count++;
    }
    reset_symbol_table_nest_count (SUB (p));
  }
}

/*!
\brief bind routines in symbol table to the tree
\param p position in tree
**/

void bind_routine_tags_to_tree (NODE_T * p)
{
/* By inserting coercions etc. some may have shifted. */
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ROUTINE_TEXT) && TAX (p) != NULL) {
      NODE (TAX (p)) = p;
    }
    bind_routine_tags_to_tree (SUB (p));
  }
}

/*!
\brief bind formats in symbol table to tree
\param p position in tree
**/

void bind_format_tags_to_tree (NODE_T * p)
{
/* By inserting coercions etc. some may have shifted. */
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, FORMAT_TEXT) && TAX (p) != NULL) {
      NODE (TAX (p)) = p;
    } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL) && NEXT (p) != NULL && TAX (p) != NULL) {
      NODE (TAX (p)) = p;
    }
    bind_format_tags_to_tree (SUB (p));
  }
}

/*!
\brief fill outer level of symbol table
\param p position in tree
\param s parent symbol table
**/

void fill_symbol_table_outer (NODE_T * p, SYMBOL_TABLE_T * s)
{
  for (; p != NULL; FORWARD (p)) {
    if (SYMBOL_TABLE (p) != NULL) {
      OUTER (SYMBOL_TABLE (p)) = s;
    }
    if (SUB (p) != NULL && ATTRIBUTE (p) == ROUTINE_TEXT) {
      fill_symbol_table_outer (SUB (p), SYMBOL_TABLE (SUB (p)));
    } else if (SUB (p) != NULL && ATTRIBUTE (p) == FORMAT_TEXT) {
      fill_symbol_table_outer (SUB (p), SYMBOL_TABLE (SUB (p)));
    } else {
      fill_symbol_table_outer (SUB (p), s);
    }
  }
}

/*!
\brief flood branch in tree with local symbol table "s"
\param p position in tree
\param s parent symbol table
**/

static void flood_with_symbol_table_restricted (NODE_T * p, SYMBOL_TABLE_T * s)
{
  for (; p != NULL; FORWARD (p)) {
    SYMBOL_TABLE (p) = s;
    if (ATTRIBUTE (p) != ROUTINE_TEXT && ATTRIBUTE (p) != SPECIFIED_UNIT) {
      if (whether_new_lexical_level (p)) {
        PREVIOUS (SYMBOL_TABLE (SUB (p))) = s;
      } else {
        flood_with_symbol_table_restricted (SUB (p), s);
      }
    }
  }
}

/*!
\brief final structure of symbol table after parsing
\param p position in tree
\param l current lexical level
**/

void finalise_symbol_table_setup (NODE_T * p, int l)
{
  SYMBOL_TABLE_T *s = SYMBOL_TABLE (p);
  NODE_T *q = p;
  while (q != NULL) {
/* routine texts are ranges. */
    if (WHETHER (q, ROUTINE_TEXT)) {
      flood_with_symbol_table_restricted (SUB (q), new_symbol_table (s));
    }
/* specifiers are ranges. */
    else if (WHETHER (q, SPECIFIED_UNIT)) {
      flood_with_symbol_table_restricted (SUB (q), new_symbol_table (s));
    }
/* level count and recursion. */
    if (SUB (q) != NULL) {
      if (whether_new_lexical_level (q)) {
        LEX_LEVEL (SUB (q)) = l + 1;
        PREVIOUS (SYMBOL_TABLE (SUB (q))) = s;
        finalise_symbol_table_setup (SUB (q), l + 1);
        if (WHETHER (q, WHILE_PART)) {
/* This was a bug that went 15 years unnoticed ! */
          SYMBOL_TABLE_T *s2 = SYMBOL_TABLE (SUB (q));
          if ((FORWARD (q)) == NULL) {
            return;
          }
          if (WHETHER (q, ALT_DO_PART)) {
            PREVIOUS (SYMBOL_TABLE (SUB (q))) = s2;
            LEX_LEVEL (SUB (q)) = l + 2;
            finalise_symbol_table_setup (SUB (q), l + 2);
          }
        }
      } else {
        SYMBOL_TABLE (SUB (q)) = s;
        finalise_symbol_table_setup (SUB (q), l);
      }
    }
    SYMBOL_TABLE (q) = s;
    if (WHETHER (q, FOR_SYMBOL)) {
      FORWARD (q);
    }
    FORWARD (q);
  }
/* FOR identifiers are in the DO ... OD range. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, FOR_SYMBOL)) {
      SYMBOL_TABLE (NEXT (q)) = SYMBOL_TABLE (NEXT (q)->sequence);
    }
  }
}

/*!
\brief first structure of symbol table for parsing
\param p position in tree
**/

void preliminary_symbol_table_setup (NODE_T * p)
{
  NODE_T *q;
  SYMBOL_TABLE_T *s = SYMBOL_TABLE (p);
  BOOL_T not_a_for_range = A68_FALSE;
/* let the tree point to the current symbol table. */
  for (q = p; q != NULL; FORWARD (q)) {
    SYMBOL_TABLE (q) = s;
  }
/* insert new tables when required. */
  for (q = p; q != NULL && !not_a_for_range; FORWARD (q)) {
    if (SUB (q) != NULL) {
/* BEGIN ... END, CODE ... EDOC, DEF ... FED, DO ... OD, $ ... $, { ... } are ranges. */
      if (whether_one_of (q, BEGIN_SYMBOL, DO_SYMBOL, ALT_DO_SYMBOL, FORMAT_DELIMITER_SYMBOL, ACCO_SYMBOL, NULL_ATTRIBUTE)) {
        SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
        preliminary_symbol_table_setup (SUB (q));
      }
/* ( ... ) is a range. */
      else if (WHETHER (q, OPEN_SYMBOL)) {
        if (whether (q, OPEN_SYMBOL, THEN_BAR_SYMBOL, 0)) {
          SYMBOL_TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NULL) {
            not_a_for_range = A68_TRUE;
          } else {
            if (WHETHER (q, THEN_BAR_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (WHETHER (q, OPEN_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
/* don't worry about STRUCT (...), UNION (...), PROC (...) yet. */
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* IF ... THEN ... ELSE ... FI are ranges. */
      else if (WHETHER (q, IF_SYMBOL)) {
        if (whether (q, IF_SYMBOL, THEN_SYMBOL, 0)) {
          SYMBOL_TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NULL) {
            not_a_for_range = A68_TRUE;
          } else {
            if (WHETHER (q, ELSE_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (WHETHER (q, IF_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* CASE ... IN ... OUT ... ESAC are ranges. */
      else if (WHETHER (q, CASE_SYMBOL)) {
        if (whether (q, CASE_SYMBOL, IN_SYMBOL, 0)) {
          SYMBOL_TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NULL) {
            not_a_for_range = A68_TRUE;
          } else {
            if (WHETHER (q, OUT_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (WHETHER (q, CASE_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* UNTIL ... OD is a range. */
      else if (WHETHER (q, UNTIL_SYMBOL) && SUB (q) != NULL) {
        SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
        preliminary_symbol_table_setup (SUB (q));
/* WHILE ... DO ... OD are ranges. */
      } else if (WHETHER (q, WHILE_SYMBOL)) {
        SYMBOL_TABLE_T *u = new_symbol_table (s);
        SYMBOL_TABLE (SUB (q)) = u;
        preliminary_symbol_table_setup (SUB (q));
        if ((FORWARD (q)) == NULL) {
          not_a_for_range = A68_TRUE;
        } else if (WHETHER (q, ALT_DO_SYMBOL)) {
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (u);
          preliminary_symbol_table_setup (SUB (q));
        }
      } else {
        SYMBOL_TABLE (SUB (q)) = s;
        preliminary_symbol_table_setup (SUB (q));
      }
    }
  }
/* FOR identifiers will go to the DO ... OD range. */
  if (!not_a_for_range) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (WHETHER (q, FOR_SYMBOL)) {
        NODE_T *r = q;
        SYMBOL_TABLE (NEXT (q)) = NULL;
        for (; r != NULL && SYMBOL_TABLE (NEXT (q)) == NULL; FORWARD (r)) {
          if ((whether_one_of (r, WHILE_SYMBOL, ALT_DO_SYMBOL, NULL_ATTRIBUTE)) && (NEXT (q) != NULL && SUB (r) != NULL)) {
            SYMBOL_TABLE (NEXT (q)) = SYMBOL_TABLE (SUB (r));
            NEXT (q)->sequence = SUB (r);
          }
        }
      }
    }
  }
}

/*!
\brief mark a mode as in use
\param m mode to mark
**/

static void mark_mode (MOID_T * m)
{
  if (m != NULL && USE (m) == A68_FALSE) {
    PACK_T *p = PACK (m);
    USE (m) = A68_TRUE;
    for (; p != NULL; FORWARD (p)) {
      mark_mode (MOID (p));
      mark_mode (SUB (m));
      mark_mode (SLICE (m));
    }
  }
}

/*!
\brief traverse tree and mark modes as used
\param p position in tree
**/

void mark_moids (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    mark_moids (SUB (p));
    if (MOID (p) != NULL) {
      mark_mode (MOID (p));
    }
  }
}

/*!
\brief mark various tags as used
\param p position in tree
**/

void mark_auxilliary (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL) {
/*
You get no warnings on unused PROC parameters. That is ok since A68 has some
parameters that you may not use at all - think of PROC (REF FILE) BOOL event
routines in transput.
*/
      mark_auxilliary (SUB (p));
    } else if (WHETHER (p, OPERATOR)) {
      TAG_T *z;
      if (TAX (p) != NULL) {
        USE (TAX (p)) = A68_TRUE;
      }
      if ((z = find_tag_global (SYMBOL_TABLE (p), PRIO_SYMBOL, SYMBOL (p))) != NULL) {
        USE (z) = A68_TRUE;
      }
    } else if (WHETHER (p, INDICANT)) {
      TAG_T *z = find_tag_global (SYMBOL_TABLE (p), INDICANT, SYMBOL (p));
      if (z != NULL) {
        TAX (p) = z;
        USE (z) = A68_TRUE;
      }
    } else if (WHETHER (p, IDENTIFIER)) {
      if (TAX (p) != NULL) {
        USE (TAX (p)) = A68_TRUE;
      }
    }
  }
}

/*!
\brief check a single tag
\param s tag to check
**/

static void unused (TAG_T * s)
{
  for (; s != NULL; FORWARD (s)) {
    if (!USE (s)) {
      diagnostic_node (A68_WARNING, NODE (s), WARNING_TAG_UNUSED, NODE (s));
    }
  }
}

/*!
\brief driver for traversing tree and warn for unused tags
\param p position in tree
**/

void warn_for_unused_tags (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && LINE_NUMBER (p) != 0) {
      if (whether_new_lexical_level (p) && ATTRIBUTE (SYMBOL_TABLE (SUB (p))) != ENVIRON_SYMBOL) {
        unused (SYMBOL_TABLE (SUB (p))->operators);
        unused (PRIO (SYMBOL_TABLE (SUB (p))));
        unused (SYMBOL_TABLE (SUB (p))->identifiers);
        unused (SYMBOL_TABLE (SUB (p))->indicants);
      }
    }
    warn_for_unused_tags (SUB (p));
  }
}

/*!
\brief warn if tags are used between threads
\param p position in tree
**/

void warn_tags_threads (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    warn_tags_threads (SUB (p));
    if (whether_one_of (p, IDENTIFIER, OPERATOR, NULL_ATTRIBUTE)) {
      if (TAX (p) != NULL) {
        int plev_def = PAR_LEVEL (NODE (TAX (p))), plev_app = PAR_LEVEL (p);
        if (plev_def != 0 && plev_def != plev_app) {
          diagnostic_node (A68_WARNING, p, WARNING_DEFINED_IN_OTHER_THREAD);
        }
      }
    }
  }
}

/*!
\brief mark jumps and procedured jumps
\param p position in tree
**/

void jumps_from_procs (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, PROCEDURING)) {
      NODE_T *u = SUB (SUB (p));
      if (WHETHER (u, GOTO_SYMBOL)) {
        FORWARD (u);
      }
      USE (TAX (u)) = A68_TRUE;
    } else if (WHETHER (p, JUMP)) {
      NODE_T *u = SUB (p);
      if (WHETHER (u, GOTO_SYMBOL)) {
        FORWARD (u);
      }
      if ((TAX (u) == NULL) && (MOID (u) == NULL) && (find_tag_global (SYMBOL_TABLE (u), LABEL, SYMBOL (u)) == NULL)) {
        (void) add_tag (SYMBOL_TABLE (u), LABEL, u, NULL, LOCAL_LABEL);
        diagnostic_node (A68_ERROR, u, ERROR_UNDECLARED_TAG);
      } else {
         USE (TAX (u)) = A68_TRUE;
      }
    } else {
      jumps_from_procs (SUB (p));
    }
  }
}

/*!
\brief assign offset tags
\param t tag to start from
\param base first (base) address
\return end address
**/

static ADDR_T assign_offset_tags (TAG_T * t, ADDR_T base)
{
  ADDR_T sum = base;
  for (; t != NULL; FORWARD (t)) {
    SIZE (t) = moid_size (MOID (t));
    if (VALUE (t) == NULL) {
      OFFSET (t) = sum;
      sum += SIZE (t);
    }
  }
  return (sum);
}

/*!
\brief assign offsets table
\param c symbol table 
**/

void assign_offsets_table (SYMBOL_TABLE_T * c)
{
  c->ap_increment = assign_offset_tags (c->identifiers, 0);
  c->ap_increment = assign_offset_tags (c->operators, c->ap_increment);
  c->ap_increment = assign_offset_tags (c->anonymous, c->ap_increment);
  c->ap_increment = A68_ALIGN (c->ap_increment);
}

/*!
\brief assign offsets
\param p position in tree
**/

void assign_offsets (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      assign_offsets_table (SYMBOL_TABLE (SUB (p)));
    }
    assign_offsets (SUB (p));
  }
}

/*!
\brief assign offsets packs in moid list
\param moid_list moid list entry to start from
**/

void assign_offsets_packs (MOID_LIST_T * moid_list)
{
  MOID_LIST_T *q;
  for (q = moid_list; q != NULL; FORWARD (q)) {
    if (EQUIVALENT (MOID (q)) == NULL && WHETHER (MOID (q), STRUCT_SYMBOL)) {
      PACK_T *p = PACK (MOID (q));
      ADDR_T offset = 0;
      for (; p != NULL; FORWARD (p)) {
        SIZE (p) = moid_size (MOID (p));
        OFFSET (p) = offset;
        offset += SIZE (p);
      }
    }
  }
}
