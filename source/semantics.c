/*!
\file semantics.c
\brief tags, modes, coercions, scope
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "config.h"
#include "algol68g.h"
#include "interpreter.h"
#include "mp.h"

/* This file contains routines that work with tags and symbol tables. */

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
    if (program.options.portcheck) {
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
        TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, SUB_MOID (SUB (p)), GENERATOR);
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
        ABEND (A68_TRUE, "standard environ error", NULL);
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
        ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "add tag");
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
        ABEND (A68_TRUE, "impossible state in find_tag_global", NULL);
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
      ABEND (A68_TRUE, "impossible state in find_tag_local", NULL);
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
        TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, SUB_MOID (p), GENERATOR);
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
      CODEX (entry) |= PROC_DECLARATION_MASK;
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
  NODE_T *pack = SUB_SUB (NEXT_NEXT (p));     /* That's where the parameter pack is. */
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
      NODE_T *u = SUB_SUB (p);
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

/*
Mode collection, equivalencing and derived modes
*/

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
          get_mode_from_formal_pack (SUB_NEXT (p), &u);
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
    get_mode_from_formal_pack (SUB_NEXT (p), &u);
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
  ABEND (A68_TRUE, "cannot decide in whether_modes_equivalent", NULL);
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
              NAME (m) = make_name_row (SUB_SUB (m), topmoid);
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
            if (SUB_SUB (q) == NULL) {
              (*mods)++;        /* as yet unresolved FLEX INDICANT. */
            } else {
              if (WHETHER (SUB_SUB (q), STRUCT_SYMBOL)) {
                z = A68_TRUE;
                (*mods)++;
                MULTIPLE (q) = make_flex_multiple_struct (SUB_SUB (q), top, DIM (SUB (q)));
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
        if (SUB_SUB (q) == NULL) {
          (*mods)++;            /* as yet unresolved FLEX INDICANT. */
        } else {
          if (WHETHER (SUB_SUB (q), STRUCT_SYMBOL)) {
            z = A68_TRUE;
            (*mods)++;
            MULTIPLE (q) = make_flex_multiple_struct (SUB_SUB (q), top, DIM (SUB (q)));
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
    ABEND (SUB (m) == NULL, "NULL mode while deflexing", NULL);
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
    ABEND (n == NULL, "NULL equivalent mode while deflexing", NULL);
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
          ABEND (whether_mode_has_flex (DEFLEXED (m)), "deflexing failed", moid_to_string (DEFLEXED (m), MOID_WIDTH, NULL));
        }
/* 'Light' deflexing needed for trims. */
        if (TRIM (m) == NULL && WHETHER (m, FLEX_SYMBOL)) {
          (*mods)++;
          TRIM (m) = SUB (m);
        } else if (TRIM (m) == NULL && WHETHER (m, REF_SYMBOL) && WHETHER (SUB (m), FLEX_SYMBOL)) {
          (*mods)++;
          (void) add_mode (top, REF_SYMBOL, DIM (m), NULL, SUB_SUB (m), NULL);
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
      MOID_T *z = add_row (top, DIM (SUB (m)) + 1, SUB_SUB (m), NODE (SUB (m)));
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
  if (program.error_count == 0) {
/* Check yin-yang of modes. */
    free_postulate_list (top_postulate, NULL);
    top_postulate = NULL;
    check_well_formedness_tree (top_node);
/* Construct the full moid list. */
    if (program.error_count == 0) {
      int cycle = 0;
      track_equivalent_standard_modes ();
      while (expand_contract_moids (top_node, cycle) > 0 || cycle < 16) {
        ABEND (cycle++ > 32, "apparently indefinite loop in set_up_mode_table", NULL);
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
    ASSERT (snprintf (str, (size_t) SMALL_BUFFER_SIZE, "\\%d", ATTRIBUTE (n)) >= 0);
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

/* Static scope checker */

/*
Static scope checker. 
Also a little preparation for the monitor:
- indicates UNITs that can be interrupted.
*/

typedef struct TUPLE_T TUPLE_T;
typedef struct SCOPE_T SCOPE_T;

struct TUPLE_T
{
  int level;
  BOOL_T transient;
};

struct SCOPE_T
{
  NODE_T *where;
  TUPLE_T tuple;
  SCOPE_T *next;
};

enum
{ NOT_TRANSIENT = 0, TRANSIENT };

static void gather_scopes_for_youngest (NODE_T *, SCOPE_T **);
static void scope_statement (NODE_T *, SCOPE_T **);
static void scope_enclosed_clause (NODE_T *, SCOPE_T **);
static void scope_formula (NODE_T *, SCOPE_T **);
static void scope_routine_text (NODE_T *, SCOPE_T **);

/*!
\brief scope_make_tuple
\param e level
\param t whether transient
\return tuple (e, t)
**/

static TUPLE_T scope_make_tuple (int e, int t)
{
  static TUPLE_T z;
  z.level = e;
  z.transient = (BOOL_T) t;
  return (z);
}

/*!
\brief link scope information into the list
\param sl chain to link into
\param p position in tree
\param tup tuple to link
**/

static void scope_add (SCOPE_T ** sl, NODE_T * p, TUPLE_T tup)
{
  if (sl != NULL) {
    SCOPE_T *ns = (SCOPE_T *) get_temp_heap_space ((unsigned) ALIGNED_SIZE_OF (SCOPE_T));
    ns->where = p;
    ns->tuple = tup;
    NEXT (ns) = *sl;
    *sl = ns;
  }
}

/*!
\brief scope_check
\param top top of scope chain
\param mask what to check
\param dest level to check against
\return whether errors were detected
**/

static BOOL_T scope_check (SCOPE_T * top, int mask, int dest)
{
  SCOPE_T *s;
  int errors = 0;
/* Transient names cannot be stored. */
  if (mask & TRANSIENT) {
    for (s = top; s != NULL; FORWARD (s)) {
      if (s->tuple.transient & TRANSIENT) {
        diagnostic_node (A68_ERROR, s->where, ERROR_TRANSIENT_NAME);
        STATUS_SET (s->where, SCOPE_ERROR_MASK);
        errors++;
      }
    }
  }
  for (s = top; s != NULL; FORWARD (s)) {
    if (dest < s->tuple.level && ! STATUS_TEST (s->where, SCOPE_ERROR_MASK)) {
/* Potential scope violations. */
      if (MOID (s->where) == NULL) {
        diagnostic_node (A68_WARNING, s->where, WARNING_SCOPE_STATIC_1, ATTRIBUTE (s->where));
      } else {
        diagnostic_node (A68_WARNING, s->where, WARNING_SCOPE_STATIC_2, MOID (s->where), ATTRIBUTE (s->where));
      }
      STATUS_SET (s->where, SCOPE_ERROR_MASK);
      errors++;
    }
  }
  return ((BOOL_T) (errors == 0));
}

/*!
\brief scope_check_multiple
\param top top of scope chain
\param mask what to check
\param dest level to check against
\return whether error
**/

static BOOL_T scope_check_multiple (SCOPE_T * top, int mask, SCOPE_T * dest)
{
  BOOL_T no_err = A68_TRUE;
  for (; dest != NULL; FORWARD (dest)) {
    no_err &= scope_check (top, mask, dest->tuple.level);
  }
  return (no_err);
}

/*!
\brief check_identifier_usage
\param t tag
\param p position in tree
**/

static void check_identifier_usage (TAG_T * t, NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, IDENTIFIER) && TAX (p) == t && ATTRIBUTE (MOID (t)) != PROC_SYMBOL) {
      diagnostic_node (A68_WARNING, p, WARNING_UNINITIALISED);
    }
    check_identifier_usage (t, SUB (p));
  }
}

/*!
\brief scope_find_youngest_outside
\param s chain to link into
\param treshold threshold level
\return youngest tuple outside
**/

static TUPLE_T scope_find_youngest_outside (SCOPE_T * s, int treshold)
{
  TUPLE_T z = scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT);
  for (; s != NULL; FORWARD (s)) {
    if (s->tuple.level > z.level && s->tuple.level <= treshold) {
      z = s->tuple;
    }
  }
  return (z);
}

/*!
\brief scope_find_youngest
\param s chain to link into
\return youngest tuple outside
**/

static TUPLE_T scope_find_youngest (SCOPE_T * s)
{
  return (scope_find_youngest_outside (s, A68_MAX_INT));
}

/* Routines for determining scope of ROUTINE TEXT or FORMAT TEXT. */

/*!
\brief get_declarer_elements
\param p position in tree
\param r chain to link into
\param no_ref whether no REF seen yet
**/

static void get_declarer_elements (NODE_T * p, SCOPE_T ** r, BOOL_T no_ref)
{
  if (p != NULL) {
    if (WHETHER (p, BOUNDS)) {
      gather_scopes_for_youngest (SUB (p), r);
    } else if (WHETHER (p, INDICANT)) {
      if (MOID (p) != NULL && TAX (p) != NULL && MOID (p)->has_rows && no_ref) {
        scope_add (r, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
      }
    } else if (WHETHER (p, REF_SYMBOL)) {
      get_declarer_elements (NEXT (p), r, A68_FALSE);
    } else if (whether_one_of (p, PROC_SYMBOL, UNION_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else {
      get_declarer_elements (SUB (p), r, no_ref);
      get_declarer_elements (NEXT (p), r, no_ref);
    }
  }
}

/*!
\brief gather_scopes_for_youngest
\param p position in tree
\param s chain to link into
**/

static void gather_scopes_for_youngest (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if ((whether_one_of (p, ROUTINE_TEXT, FORMAT_TEXT, NULL_ATTRIBUTE))
        && (TAX (p)->youngest_environ == PRIMAL_SCOPE)) {
      SCOPE_T *t = NULL;
      gather_scopes_for_youngest (SUB (p), &t);
      TAX (p)->youngest_environ = scope_find_youngest_outside (t, LEX_LEVEL (p)).level;
/* Direct link into list iso "gather_scopes_for_youngest (SUB (p), s);" */
      if (t != NULL) {
        SCOPE_T *u = t;
        while (NEXT (u) != NULL) {
          FORWARD (u);
        }
        NEXT (u) = *s;
        (*s) = t;
      }
    } else if (whether_one_of (p, IDENTIFIER, OPERATOR, NULL_ATTRIBUTE)) {
      if (TAX (p) != NULL && TAG_LEX_LEVEL (TAX (p)) != PRIMAL_SCOPE) {
        scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
      }
    } else if (WHETHER (p, DECLARER)) {
      get_declarer_elements (p, s, A68_TRUE);
    } else {
      gather_scopes_for_youngest (SUB (p), s);
    }
  }
}

/*!
\brief get_youngest_environs
\param p position in tree
**/

static void get_youngest_environs (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, ROUTINE_TEXT, FORMAT_TEXT, NULL_ATTRIBUTE)) {
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
\param p position in tree
**/

static void bind_scope_to_tag (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, DEFINING_IDENTIFIER) && MOID (p) == MODE (FORMAT)) {
      if (WHETHER (NEXT_NEXT (p), FORMAT_TEXT)) {
        TAX (p)->scope = TAX (NEXT_NEXT (p))->youngest_environ;
        TAX (p)->scope_assigned = A68_TRUE;
      }
      return;
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      if (WHETHER (NEXT_NEXT (p), ROUTINE_TEXT)) {
        TAX (p)->scope = TAX (NEXT_NEXT (p))->youngest_environ;
        TAX (p)->scope_assigned = A68_TRUE;
      }
      return;
    } else {
      bind_scope_to_tag (SUB (p));
    }
  }
}

/*!
\brief bind_scope_to_tags
\param p position in tree
**/

static void bind_scope_to_tags (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, PROCEDURE_DECLARATION, IDENTITY_DECLARATION, NULL_ATTRIBUTE)) {
      bind_scope_to_tag (SUB (p));
    } else {
      bind_scope_to_tags (SUB (p));
    }
  }
}

/*!
\brief scope_bounds
\param p position in tree
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
\param p position in tree
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
    } else if (whether_one_of (p, PROC_SYMBOL, UNION_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else {
      scope_declarer (SUB (p));
      scope_declarer (NEXT (p));
    }
  }
}

/*!
\brief scope_identity_declaration
\param p position in tree
**/

static void scope_identity_declaration (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    scope_identity_declaration (SUB (p));
    if (WHETHER (p, DEFINING_IDENTIFIER)) {
      NODE_T *unit = NEXT_NEXT (p);
      SCOPE_T *s = NULL;
      int z = PRIMAL_SCOPE;
      if (ATTRIBUTE (MOID (TAX (p))) != PROC_SYMBOL) {
        check_identifier_usage (TAX (p), unit);
      }
      scope_statement (unit, &s);
      (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
      z = scope_find_youngest (s).level;
      if (z < LEX_LEVEL (p)) {
        TAX (p)->scope = z;
        TAX (p)->scope_assigned = A68_TRUE;
      }
      STATUS_SET (unit, INTERRUPTIBLE_MASK);
      return;
    }
  }
}

/*!
\brief scope_variable_declaration
\param p position in tree
**/

static void scope_variable_declaration (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    scope_variable_declaration (SUB (p));
    if (WHETHER (p, DECLARER)) {
      scope_declarer (SUB (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
        NODE_T *unit = NEXT_NEXT (p);
        SCOPE_T *s = NULL;
        check_identifier_usage (TAX (p), unit);
        scope_statement (unit, &s);
        (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
        STATUS_SET (unit, INTERRUPTIBLE_MASK);
        return;
      }
    }
  }
}

/*!
\brief scope_procedure_declaration
\param p position in tree
**/

static void scope_procedure_declaration (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    scope_procedure_declaration (SUB (p));
    if (whether_one_of (p, DEFINING_IDENTIFIER, DEFINING_OPERATOR, NULL_ATTRIBUTE)) {
      NODE_T *unit = NEXT_NEXT (p);
      SCOPE_T *s = NULL;
      scope_statement (unit, &s);
      (void) scope_check (s, NOT_TRANSIENT, LEX_LEVEL (p));
      STATUS_SET (unit, INTERRUPTIBLE_MASK);
      return;
    }
  }
}

/*!
\brief scope_declaration_list
\param p position in tree
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
    } else if (whether_one_of (p, BRIEF_OPERATOR_DECLARATION, OPERATOR_DECLARATION, NULL_ATTRIBUTE)) {
      scope_procedure_declaration (SUB (p));
    } else {
      scope_declaration_list (SUB (p));
      scope_declaration_list (NEXT (p));
    }
  }
}

/*!
\brief scope_arguments
\param p position in tree
**/

static void scope_arguments (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      SCOPE_T *s = NULL;
      scope_statement (p, &s);
      (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
    } else {
      scope_arguments (SUB (p));
    }
  }
}

/*!
\brief whether_transient_row
\param m mode of row
\return same
**/

static BOOL_T whether_transient_row (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return ((BOOL_T) (WHETHER (SUB (m), FLEX_SYMBOL)));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether_coercion
\param p position in tree
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
        return (A68_TRUE);
      }
    default:
      {
        return (A68_FALSE);
      }
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief scope_coercion
\param p position in tree
\param s chain to link into
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
      NODE_T *q = SUB_SUB (p);
      if (WHETHER (q, GOTO_SYMBOL)) {
        FORWARD (q);
      }
      scope_add (s, q, scope_make_tuple (TAG_LEX_LEVEL (TAX (q)), NOT_TRANSIENT));
    } else {
      scope_coercion (SUB (p), s);
    }
  } else {
    scope_statement (p, s);
  }
}

/*!
\brief scope_format_text
\param p position in tree
\param s chain to link into
**/

static void scope_format_text (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, FORMAT_PATTERN)) {
      scope_enclosed_clause (SUB (NEXT_SUB (p)), s);
    } else if (WHETHER (p, FORMAT_ITEM_G) && NEXT (p) != NULL) {
      scope_enclosed_clause (SUB_NEXT (p), s);
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      scope_enclosed_clause (SUB (NEXT_SUB (p)), s);
    } else {
      scope_format_text (SUB (p), s);
    }
  }
}

/*!
\brief whether_transient_selection
\param m mode under test
\return same
**/

static BOOL_T whether_transient_selection (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (whether_transient_selection (SUB (m)));
  } else {
    return ((BOOL_T) (WHETHER (m, FLEX_SYMBOL)));
  }
}

/*!
\brief scope_operand
\param p position in tree
\param s chain to link into
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
\param p position in tree
\param s chain to link into
**/

static void scope_formula (NODE_T * p, SCOPE_T ** s)
{
  NODE_T *q = SUB (p);
  SCOPE_T *s2 = NULL;
  scope_operand (q, &s2);
  (void) scope_check (s2, TRANSIENT, LEX_LEVEL (p));
  if (NEXT (q) != NULL) {
    SCOPE_T *s3 = NULL;
    scope_operand (NEXT_NEXT (q), &s3);
    (void) scope_check (s3, TRANSIENT, LEX_LEVEL (p));
  }
  (void) s;
}

/*!
\brief scope_routine_text
\param p position in tree
\param s chain to link into
**/

static void scope_routine_text (NODE_T * p, SCOPE_T ** s)
{
  NODE_T *q = SUB (p), *routine = WHETHER (q, PARAMETER_PACK) ? NEXT (q) : q;
  SCOPE_T *x = NULL;
  TUPLE_T routine_tuple;
  scope_statement (NEXT_NEXT (routine), &x);
  (void) scope_check (x, TRANSIENT, LEX_LEVEL (p));
  routine_tuple = scope_make_tuple (TAX (p)->youngest_environ, NOT_TRANSIENT);
  scope_add (s, p, routine_tuple);
}

/*!
\brief scope_statement
\param p position in tree
\param s chain to link into
**/

static void scope_statement (NODE_T * p, SCOPE_T ** s)
{
  if (whether_coercion (p)) {
    scope_coercion (p, s);
  } else if (whether_one_of (p, PRIMARY, SECONDARY, TERTIARY, UNIT, NULL_ATTRIBUTE)) {
    scope_statement (SUB (p), s);
  } else if (whether_one_of (p, DENOTATION, NIHIL, NULL_ATTRIBUTE)) {
    scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
  } else if (WHETHER (p, IDENTIFIER)) {
    if (WHETHER (MOID (p), REF_SYMBOL)) {
      if (PRIO (TAX (p)) == PARAMETER_IDENTIFIER) {
        scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)) - 1, NOT_TRANSIENT));
      } else {
        if (HEAP (TAX (p)) == HEAP_SYMBOL) {
          scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
        } else if (TAX (p)->scope_assigned) {
          scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
        } else {
          scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
        }
      }
    } else if (ATTRIBUTE (MOID (p)) == PROC_SYMBOL && TAX (p)->scope_assigned == A68_TRUE) {
      scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
    } else if (MOID (p) == MODE (FORMAT) && TAX (p)->scope_assigned == A68_TRUE) {
      scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
    }
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    scope_enclosed_clause (SUB (p), s);
  } else if (WHETHER (p, CALL)) {
    SCOPE_T *x = NULL;
    scope_statement (SUB (p), &x);
    (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
    scope_arguments (NEXT_SUB (p));
  } else if (WHETHER (p, SLICE)) {
    SCOPE_T *x = NULL;
    MOID_T *m = MOID (SUB (p));
    if (WHETHER (m, REF_SYMBOL)) {
      if (ATTRIBUTE (SUB (p)) == PRIMARY && ATTRIBUTE (SUB_SUB (p)) == SLICE) {
        scope_statement (SUB (p), s);
      } else {
        scope_statement (SUB (p), &x);
        (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
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
    SCOPE_T *x = NULL;
    scope_enclosed_clause (SUB (NEXT_SUB (p)), &x);
    (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
    scope_add (s, p, scope_find_youngest (x));
  } else if (WHETHER (p, FIELD_SELECTION)) {
    SCOPE_T *ns = NULL;
    scope_statement (SUB (p), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (p));
    if (whether_transient_selection (MOID (SUB (p)))) {
      scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
    }
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, SELECTION)) {
    SCOPE_T *ns = NULL;
    scope_statement (NEXT_SUB (p), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (p));
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
  } else if (WHETHER (p, DIAGONAL_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NULL;
    if (WHETHER (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NULL;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, TRANSPOSE_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NULL;
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, ROW_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NULL;
    if (WHETHER (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NULL;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, COLUMN_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NULL;
    if (WHETHER (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NULL;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, FORMULA)) {
    scope_formula (p, s);
  } else if (WHETHER (p, ASSIGNATION)) {
    NODE_T *unit = NEXT (NEXT_SUB (p));
    SCOPE_T *ns = NULL, *nd = NULL;
    scope_statement (SUB_SUB (p), &nd);
    scope_statement (unit, &ns);
    (void) scope_check_multiple (ns, TRANSIENT, nd);
    scope_add (s, p, scope_make_tuple (scope_find_youngest (nd).level, NOT_TRANSIENT));
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    scope_routine_text (p, s);
  } else if (whether_one_of (p, IDENTITY_RELATION, AND_FUNCTION, OR_FUNCTION, NULL_ATTRIBUTE)) {
    SCOPE_T *n = NULL;
    scope_statement (SUB (p), &n);
    scope_statement (NEXT (NEXT_SUB (p)), &n);
    (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  } else if (WHETHER (p, ASSERTION)) {
    SCOPE_T *n = NULL;
    scope_enclosed_clause (SUB (NEXT_SUB (p)), &n);
    (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  } else if (whether_one_of (p, JUMP, SKIP, NULL_ATTRIBUTE)) {
    ;
  }
}

/*!
\brief scope_statement_list
\param p position in tree
\param s chain to link into
**/

static void scope_statement_list (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      STATUS_SET (p, INTERRUPTIBLE_MASK);
      scope_statement (p, s);
    } else {
      scope_statement_list (SUB (p), s);
    }
  }
}

/*!
\brief scope_serial_clause
\param p position in tree
\param s chain to link into
\param terminator whether unit terminates clause
**/

static void scope_serial_clause (NODE_T * p, SCOPE_T ** s, BOOL_T terminator)
{
  if (p != NULL) {
    if (WHETHER (p, INITIALISER_SERIES)) {
      scope_serial_clause (SUB (p), s, A68_FALSE);
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (WHETHER (p, DECLARATION_LIST)) {
      scope_declaration_list (SUB (p));
    } else if (whether_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, NULL_ATTRIBUTE)) {
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (whether_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, NULL_ATTRIBUTE)) {
      if (NEXT (p) != NULL) {
        int j = ATTRIBUTE (NEXT (p));
        if (j == EXIT_SYMBOL || j == END_SYMBOL || j == CLOSE_SYMBOL) {
          scope_serial_clause (SUB (p), s, A68_TRUE);
        } else {
          scope_serial_clause (SUB (p), s, A68_FALSE);
        }
      } else {
        scope_serial_clause (SUB (p), s, A68_TRUE);
      }
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (WHETHER (p, LABELED_UNIT)) {
      scope_serial_clause (SUB (p), s, terminator);
    } else if (WHETHER (p, UNIT)) {
      STATUS_SET (p, INTERRUPTIBLE_MASK);
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
\param p position in tree
\param s chain to link into
**/

static void scope_closed_clause (NODE_T * p, SCOPE_T ** s)
{
  if (p != NULL) {
    if (WHETHER (p, SERIAL_CLAUSE)) {
      scope_serial_clause (p, s, A68_TRUE);
    } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
      scope_closed_clause (NEXT (p), s);
    }
  }
}

/*!
\brief scope_collateral_clause
\param p position in tree
\param s chain to link into
**/

static void scope_collateral_clause (NODE_T * p, SCOPE_T ** s)
{
  if (p != NULL) {
    if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, 0)
          || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0))) {
      scope_statement_list (p, s);
    }
  }
}

/*!
\brief scope_conditional_clause
\param p position in tree
\param s chain to link into
**/

static void scope_conditional_clause (NODE_T * p, SCOPE_T ** s)
{
  scope_serial_clause (NEXT_SUB (p), NULL, A68_TRUE);
  FORWARD (p);
  scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, ELSE_PART, CHOICE, NULL_ATTRIBUTE)) {
      scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
    } else if (whether_one_of (p, ELIF_PART, BRIEF_ELIF_IF_PART, NULL_ATTRIBUTE)) {
      scope_conditional_clause (SUB (p), s);
    }
  }
}

/*!
\brief scope_case_clause
\param p position in tree
\param s chain to link into
**/

static void scope_case_clause (NODE_T * p, SCOPE_T ** s)
{
  SCOPE_T *n = NULL;
  scope_serial_clause (NEXT_SUB (p), &n, A68_TRUE);
  (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  FORWARD (p);
  scope_statement_list (NEXT_SUB (p), s);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
    } else if (whether_one_of (p, INTEGER_OUT_PART, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE)) {
      scope_case_clause (SUB (p), s);
    } else if (whether_one_of (p, UNITED_OUSE_PART, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE)) {
      scope_case_clause (SUB (p), s);
    }
  }
}

/*!
\brief scope_loop_clause
\param p position in tree
**/

static void scope_loop_clause (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, FOR_PART)) {
      scope_loop_clause (NEXT (p));
    } else if (whether_one_of (p, FROM_PART, BY_PART, TO_PART, NULL_ATTRIBUTE)) {
      scope_statement (NEXT_SUB (p), NULL);
      scope_loop_clause (NEXT (p));
    } else if (WHETHER (p, WHILE_PART)) {
      scope_serial_clause (NEXT_SUB (p), NULL, A68_TRUE);
      scope_loop_clause (NEXT (p));
    } else if (whether_one_of (p, DO_PART, ALT_DO_PART, NULL_ATTRIBUTE)) {
      NODE_T *do_p = NEXT_SUB (p), *un_p;
      if (WHETHER (do_p, SERIAL_CLAUSE)) {
        scope_serial_clause (do_p, NULL, A68_TRUE);
        un_p = NEXT (do_p);
      } else {
        un_p = do_p;
      }
      if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
        scope_serial_clause (NEXT_SUB (un_p), NULL, A68_TRUE);
      }
    }
  }
}

/*!
\brief scope_enclosed_clause
\param p position in tree
\param s chain to link into
**/

static void scope_enclosed_clause (NODE_T * p, SCOPE_T ** s)
{
  if (WHETHER (p, ENCLOSED_CLAUSE)) {
    scope_enclosed_clause (SUB (p), s);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    scope_closed_clause (SUB (p), s);
  } else if (whether_one_of (p, COLLATERAL_CLAUSE, PARALLEL_CLAUSE, NULL_ATTRIBUTE)) {
    scope_collateral_clause (SUB (p), s);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    scope_conditional_clause (SUB (p), s);
  } else if (whether_one_of (p, INTEGER_CASE_CLAUSE, UNITED_CASE_CLAUSE, NULL_ATTRIBUTE)) {
    scope_case_clause (SUB (p), s);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    scope_loop_clause (SUB (p));
  }
}

/*!
\brief scope_checker
\param p position in tree
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

/* Mode checker and coercion inserter */

/*
This is the mode checker and coercion inserter. The syntax tree is traversed to
determine and check all modes. Next the tree is traversed again to insert
coercions.

Algol 68 contexts are SOFT, WEAK, MEEK, FIRM and STRONG.
These contexts are increasing in strength:

  SOFT: Deproceduring
  WEAK: Dereferencing to REF [] or REF STRUCT
  MEEK: Deproceduring and dereferencing
  FIRM: MEEK followed by uniting
  STRONG: FIRM followed by rowing, widening or voiding

Furthermore you will see in this file next switches:

(1) FORCE_DEFLEXING allows assignment compatibility between FLEX and non FLEX
rows. This can only be the case when there is no danger of altering bounds of a
non FLEX row.

(2) ALIAS_DEFLEXING prohibits aliasing a FLEX row to a non FLEX row (vice versa
is no problem) so that one cannot alter the bounds of a non FLEX row by
aliasing it to a FLEX row. This is particularly the case when passing names as
parameters to procedures:

   PROC x = (REF STRING s) VOID: ..., PROC y = (REF [] CHAR c) VOID: ...;

   x (LOC STRING);    # OK #

   x (LOC [10] CHAR); # Not OK, suppose x changes bounds of s! #
   y (LOC STRING);    # OK #
   y (LOC [10] CHAR); # OK #

(3) SAFE_DEFLEXING sets FLEX row apart from non FLEX row. This holds for names,
not for values, so common things are not rejected, for instance

   STRING x = read string;
   [] CHAR y = read string

(4) NO_DEFLEXING sets FLEX row apart from non FLEX row.
*/

TAG_T *error_tag;

static SOID_LIST_T *top_soid_list = NULL;

static BOOL_T basic_coercions (MOID_T *, MOID_T *, int, int);
static BOOL_T whether_coercible (MOID_T *, MOID_T *, int, int);
static BOOL_T whether_nonproc (MOID_T *);

static void mode_check_enclosed (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_unit (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_formula (NODE_T *, SOID_T *, SOID_T *);

static void coerce_enclosed (NODE_T *, SOID_T *);
static void coerce_operand (NODE_T *, SOID_T *);
static void coerce_formula (NODE_T *, SOID_T *);
static void coerce_unit (NODE_T *, SOID_T *);

#define DEPREF A68_TRUE
#define NO_DEPREF A68_FALSE

#define WHETHER_MODE_IS_WELL(n) (! ((n) == MODE (ERROR) || (n) == MODE (UNDEFINED)))
#define INSERT_COERCIONS(n, p, q) make_strong ((n), (p), MOID (q))

/*!
\brief give accurate error message
\param p mode 1
\param q mode 2
\param context context
\param deflex deflexing regime
\param depth depth of recursion
\return error text
**/

static char *mode_error_text (NODE_T * n, MOID_T * p, MOID_T * q, int context, int deflex, int depth)
{
#define TAIL(z) (&(z)[strlen (z)])
  static char txt[BUFFER_SIZE];
  if (depth == 1) {
    txt[0] = NULL_CHAR;
  }
  if (WHETHER (p, SERIES_MODE)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (MOID (u) != NULL) {
          if (WHETHER (MOID (u), SERIES_MODE)) {
            (void) mode_error_text (n, MOID (u), q, context, deflex, depth + 1);
          } else if (!whether_coercible (MOID (u), q, context, deflex)) {
            int len = (int) strlen (txt);
            if (len > BUFFER_SIZE / 2) {
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " etcetera") >= 0);
            } else {
              if (strlen (txt) > 0) {
                ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " and ") >= 0);
              }
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
            }
          }
        }
      }
    }
    if (depth == 1) {
      ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " cannot be coerced to %s", moid_to_string (q, MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && WHETHER (q, FLEX_SYMBOL)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (!whether_coercible (MOID (u), SLICE (SUB (q)), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (SUB (q)), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && WHETHER (q, ROW_SYMBOL)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (!whether_coercible (MOID (u), SLICE (q), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (q), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && (WHETHER (q, PROC_SYMBOL) || WHETHER (q, STRUCT_SYMBOL))) {
    PACK_T *u = PACK (p), *v = PACK (q);
    if (u == NULL) {
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL && v != NULL; FORWARD (u), FORWARD (v)) {
        if (!whether_coercible (MOID (u), MOID (v), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), (size_t) BUFFER_SIZE, "%s cannot be coerced to %s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n), moid_to_string (MOID (v), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
    }
  }
  return (txt);
#undef TAIL
}

/*!
\brief cannot coerce error
\param p position in tree
\param from mode 1
\param to mode 2
\param context context
\param deflex deflexing regime
\param att attribute of context
**/

static void cannot_coerce (NODE_T * p, MOID_T * from, MOID_T * to, int context, int deflex, int att)
{
  char *txt = mode_error_text (p, from, to, context, deflex, 1);
  if (att == NULL_ATTRIBUTE) {
    if (strlen (txt) == 0) {
      diagnostic_node (A68_ERROR, p, "M cannot be coerced to M in C context", from, to, context);
    } else {
      diagnostic_node (A68_ERROR, p, "Y in C context", txt, context);
    }
  } else {
    if (strlen (txt) == 0) {
      diagnostic_node (A68_ERROR, p, "M cannot be coerced to M in C-A", from, to, context, att);
    } else {
      diagnostic_node (A68_ERROR, p, "Y in C-A", txt, context, att);
    }
  }
}

/*!
\brief driver for mode checker
\param p position in tree
**/

void mode_checker (NODE_T * p)
{
  if (WHETHER (p, PARTICULAR_PROGRAM)) {
    SOID_T x, y;
    top_soid_list = NULL;
    make_soid (&x, STRONG, MODE (VOID), 0);
    mode_check_enclosed (SUB (p), &x, &y);
    MOID (p) = MOID (&y);
  }
}

/*!
\brief driver for coercion inserions
\param p position in tree
**/

void coercion_inserter (NODE_T * p)
{
  if (WHETHER (p, PARTICULAR_PROGRAM)) {
    SOID_T q;
    make_soid (&q, STRONG, MODE (VOID), 0);
    coerce_enclosed (SUB (p), &q);
  }
}

/*!
\brief whether mode is not well defined
\param p mode, may be NULL
\return same
**/

static BOOL_T whether_mode_isnt_well (MOID_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else if (!WHETHER_MODE_IS_WELL (p)) {
    return (A68_TRUE);
  } else if (PACK (p) != NULL) {
    PACK_T *q = PACK (p);
    for (; q != NULL; FORWARD (q)) {
      if (!WHETHER_MODE_IS_WELL (MOID (q))) {
        return (A68_TRUE);
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief make SOID data structure
\param s soid buffer
\param sort sort
\param type mode
\param attribute attribute
**/

void make_soid (SOID_T * s, int sort, MOID_T * type, int attribute)
{
  ATTRIBUTE (s) = attribute;
  SORT (s) = sort;
  MOID (s) = type;
  CAST (s) = A68_FALSE;
}

/*!
\brief add SOID data to free chain
\param root top soid list
**/

void free_soid_list (SOID_LIST_T *root) {
  if (root != NULL) {
    SOID_LIST_T *q;
    for (q = root; NEXT (q) != NULL; FORWARD (q)) {
      /* skip */;
    }
    NEXT (q) = top_soid_list;
    top_soid_list = root;
  }
}

/*!
\brief add SOID data structure to soid list
\param root top soid list
\param nwhere position in tree
\param soid entry to add
**/

static void add_to_soid_list (SOID_LIST_T ** root, NODE_T * nwhere, SOID_T * soid)
{
  if (*root != NULL) {
    add_to_soid_list (&(NEXT (*root)), nwhere, soid);
  } else {
    SOID_LIST_T *new_one;
    if (top_soid_list == NULL) {
      new_one = (SOID_LIST_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (SOID_LIST_T));
      new_one->yield = (SOID_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (SOID_T));
    } else {
      new_one = top_soid_list;
      FORWARD (top_soid_list);
    }
    new_one->where = nwhere;
    make_soid (new_one->yield, SORT (soid), MOID (soid), 0);
    NEXT (new_one) = NULL;
    *root = new_one;
  }
}

/*!
\brief absorb nested series modes recursively
\param p mode
**/

static void absorb_series_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NULL, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NULL; FORWARD (t)) {
      if (MOID (t) != NULL && WHETHER (MOID (t), SERIES_MODE)) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NULL; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NULL, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NULL, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

/*!
\brief absorb nested series and united modes recursively
\param p mode
**/

static void absorb_series_union_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NULL, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NULL; FORWARD (t)) {
      if (MOID (t) != NULL && (WHETHER (MOID (t), SERIES_MODE) || WHETHER (MOID (t), UNION_SYMBOL))) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NULL; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NULL, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NULL, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

/*!
\brief make SERIES (u, v)
\param u mode 1
\param v mode 2
\return SERIES (u, v)
**/

static MOID_T *make_series_from_moids (MOID_T * u, MOID_T * v)
{
  MOID_T *x = new_moid ();
  ATTRIBUTE (x) = SERIES_MODE;
  add_mode_to_pack (&(PACK (x)), u, NULL, NODE (u));
  add_mode_to_pack (&(PACK (x)), v, NULL, NODE (v));
  absorb_series_pack (&x);
  DIM (x) = count_pack_members (PACK (x));
  add_single_moid_to_list (&top_moid_list, x, NULL);
  if (DIM (x) == 1) {
    return (MOID (PACK (x)));
  } else {
    return (x);
  }
}

/*!
\brief absorb firmly related unions in mode
\param m united mode
\return absorbed "m"
**/

static MOID_T *absorb_related_subsets (MOID_T * m)
{
/*
For instance invalid UNION (PROC REF UNION (A, B), A, B) -> valid UNION (A, B),
which is used in balancing conformity clauses.
*/
  int mods;
  do {
    PACK_T *u = NULL, *v;
    mods = 0;
    for (v = PACK (m); v != NULL; FORWARD (v)) {
      MOID_T *n = depref_completely (MOID (v));
      if (WHETHER (n, UNION_SYMBOL) && whether_subset (n, m, SAFE_DEFLEXING)) {
/* Unpack it. */
        PACK_T *w;
        for (w = PACK (n); w != NULL; FORWARD (w)) {
          add_mode_to_pack (&u, MOID (w), NULL, NODE (w));
        }
        mods++;
      } else {
        add_mode_to_pack (&u, MOID (v), NULL, NODE (v));
      }
    }
    PACK (m) = absorb_union_pack (u, &mods);
  } while (mods != 0);
  return (m);
}

/*!
\brief register mode in the global mode table, if mode is unique
\param u mode to enter
\return mode table entry
**/

static MOID_T *register_extra_mode (MOID_T * u)
{
  MOID_LIST_T *z;
/* Check for equivalency. */
  for (z = top_moid_list; z != NULL; FORWARD (z)) {
    MOID_T *v = MOID (z);
    BOOL_T w;
    free_postulate_list (top_postulate, NULL);
    top_postulate = NULL;
    w = (BOOL_T) (EQUIVALENT (v) == NULL && whether_modes_equivalent (v, u));
    if (w) {
      return (v);
    }
  }
/* Mode u is unique - include in the global moid list. */
  z = (MOID_LIST_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (MOID_LIST_T));
  z->coming_from_level = NULL;
  MOID (z) = u;
  NEXT (z) = top_moid_list;
  ABEND (z == NULL, "NULL pointer", "register_extra_mode");
  top_moid_list = z;
  add_single_moid_to_list (&top_moid_list, u, NULL);
  return (u);
}

/*!
\brief make united mode, from mode that is a SERIES (..)
\param m series mode
\return mode table entry
**/

static MOID_T *make_united_mode (MOID_T * m)
{
  MOID_T *u;
  PACK_T *v, *w;
  int mods;
  if (m == NULL) {
    return (MODE (ERROR));
  } else if (ATTRIBUTE (m) != SERIES_MODE) {
    return (m);
  }
/* Do not unite a single UNION. */
  if (DIM (m) == 1 && WHETHER (MOID (PACK (m)), UNION_SYMBOL)) {
    return (MOID (PACK (m)));
  }
/* Straighten the series. */
  absorb_series_union_pack (&m);
/* Copy the series into a UNION. */
  u = new_moid ();
  ATTRIBUTE (u) = UNION_SYMBOL;
  PACK (u) = NULL;
  v = PACK (u);
  w = PACK (m);
  for (w = PACK (m); w != NULL; FORWARD (w)) {
    add_mode_to_pack (&(PACK (u)), MOID (w), NULL, NODE (m));
  }
/* Absorb and contract the new UNION. */
  do {
    mods = 0;
    absorb_series_union_pack (&u);
    DIM (u) = count_pack_members (PACK (u));
    PACK (u) = absorb_union_pack (PACK (u), &mods);
    contract_union (u, &mods);
  } while (mods != 0);
/* A UNION of one mode is that mode itself. */
  if (DIM (u) == 1) {
    return (MOID (PACK (u)));
  } else {
    return (register_extra_mode (u));
  }
}

/*!
\brief pack soids in moid, gather resulting moids from terminators in a clause
\param top_sl top soid list
\param attribute mode attribute
\return mode table entry
**/

static MOID_T *pack_soids_in_moid (SOID_LIST_T * top_sl, int attribute)
{
  MOID_T *x = new_moid ();
  PACK_T *t, **p;
  NUMBER (x) = mode_count++;
  ATTRIBUTE (x) = attribute;
  DIM (x) = 0;
  SUB (x) = NULL;
  EQUIVALENT (x) = NULL;
  SLICE (x) = NULL;
  DEFLEXED (x) = NULL;
  NAME (x) = NULL;
  NEXT (x) = NULL;
  PACK (x) = NULL;
  p = &(PACK (x));
  for (; top_sl != NULL; FORWARD (top_sl)) {
    t = new_pack ();
    MOID (t) = MOID (top_sl->yield);
    t->text = NULL;
    NODE (t) = top_sl->where;
    NEXT (t) = NULL;
    DIM (x)++;
    *p = t;
    p = &NEXT (t);
  }
  add_single_moid_to_list (&top_moid_list, x, NULL);
  return (x);
}

/*!
\brief whether mode is deprefable
\param p mode
\return same
**/

BOOL_T whether_deprefable (MOID_T * p)
{
  if (WHETHER (p, REF_SYMBOL)) {
    return (A68_TRUE);
  } else {
    return ((BOOL_T) (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL));
  }
}

/*!
\brief depref mode once
\param p mode
\return single-depreffed mode
**/

static MOID_T *depref_once (MOID_T * p)
{
  if (WHETHER (p, REF_SYMBOL)) {
    return (SUB (p));
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (SUB (p));
  } else {
    return (NULL);
  }
}

/*!
\brief depref mode completely
\param p mode
\return completely depreffed mode
**/

MOID_T *depref_completely (MOID_T * p)
{
  while (whether_deprefable (p)) {
    p = depref_once (p);
  }
  return (p);
}

/*!
\brief deproc_completely
\param p mode
\return completely deprocedured mode
**/

static MOID_T *deproc_completely (MOID_T * p)
{
  while (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    p = depref_once (p);
  }
  return (p);
}

/*!
\brief depref rows
\param p mode
\param q mode
\return possibly depreffed mode
**/

static MOID_T *depref_rows (MOID_T * p, MOID_T * q)
{
  if (q == MODE (ROWS)) {
    while (whether_deprefable (p)) {
      p = depref_once (p);
    }
    return (p);
  } else {
    return (q);
  }
}

/*!
\brief derow mode, strip FLEX and BOUNDS
\param p mode
\return same
**/

static MOID_T *derow (MOID_T * p)
{
  if (WHETHER (p, ROW_SYMBOL) || WHETHER (p, FLEX_SYMBOL)) {
    return (derow (SUB (p)));
  } else {
    return (p);
  }
}

/*!
\brief whether rows type
\param p mode
\return same
**/

static BOOL_T whether_rows_type (MOID_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ROW_SYMBOL:
  case FLEX_SYMBOL:
    {
      return (A68_TRUE);
    }
  case UNION_SYMBOL:
    {
      PACK_T *t = PACK (p);
      BOOL_T go_on = A68_TRUE;
      while (t != NULL && go_on) {
        go_on &= whether_rows_type (MOID (t));
        FORWARD (t);
      }
      return (go_on);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether mode is PROC (REF FILE) VOID or FORMAT
\param p mode
\return same
**/

static BOOL_T whether_proc_ref_file_void_or_format (MOID_T * p)
{
  if (p == MODE (PROC_REF_FILE_VOID)) {
    return (A68_TRUE);
  } else if (p == MODE (FORMAT)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether mode can be transput
\param p mode
\return same
**/

static BOOL_T whether_transput_mode (MOID_T * p, char rw)
{
  if (p == MODE (INT)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_INT)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_INT)) {
    return (A68_TRUE);
  } else if (p == MODE (REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (BOOL)) {
    return (A68_TRUE);
  } else if (p == MODE (CHAR)) {
    return (A68_TRUE);
  } else if (p == MODE (BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (ROW_CHAR)) {
    return (A68_TRUE);
  } else if (p == MODE (STRING)) {
    return (A68_TRUE);
  } else if (p == MODE (SOUND)) {
    return (A68_TRUE);
  } else if (WHETHER (p, UNION_SYMBOL) || WHETHER (p, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (p);
    BOOL_T k = A68_TRUE;
    for (; q != NULL && k; FORWARD (q)) {
      k = (BOOL_T) (k & (whether_transput_mode (MOID (q), rw) || whether_proc_ref_file_void_or_format (MOID (q))));
    }
    return (k);
  } else if (WHETHER (p, FLEX_SYMBOL)) {
    return ((BOOL_T) (rw == 'w' ? whether_transput_mode (SUB (p), rw) : A68_FALSE));
  } else if (WHETHER (p, ROW_SYMBOL)) {
    return ((BOOL_T) (whether_transput_mode (SUB (p), rw) || whether_proc_ref_file_void_or_format (SUB (p))));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether mode is printable
\param p mode
\return same
**/

static BOOL_T whether_printable_mode (MOID_T * p)
{
  if (whether_proc_ref_file_void_or_format (p)) {
    return (A68_TRUE);
  } else {
    return (whether_transput_mode (p, 'w'));
  }
}

/*!
\brief whether mode is readable
\param p mode
\return same
**/

static BOOL_T whether_readable_mode (MOID_T * p)
{
  if (whether_proc_ref_file_void_or_format (p)) {
    return (A68_TRUE);
  } else {
    return ((BOOL_T) (WHETHER (p, REF_SYMBOL) ? whether_transput_mode (SUB (p), 'r') : A68_FALSE));
  }
}

/*!
\brief whether name struct
\param p mode
\return same
**/

static BOOL_T whether_name_struct (MOID_T * p)
{
  return ((BOOL_T) (p->name != NULL ? WHETHER (DEFLEX (SUB (p)), STRUCT_SYMBOL) : A68_FALSE));
}

/*!
\brief whether mode can be coerced to another in a certain context
\param u mode 1
\param v mode 2
\param context
\return same
**/

BOOL_T whether_modes_equal (MOID_T * u, MOID_T * v, int deflex)
{
/*
printf("\nwme strong u: %s", moid_to_string (u, 180, NULL));
printf("\nwme strong v: %s", moid_to_string (v, 180, NULL));
fflush(stdout);
*/
  if (u == v) {
    return (A68_TRUE);
  } else {
    switch (deflex) {
    case SKIP_DEFLEXING:
    case FORCE_DEFLEXING:
      {
/* Allow any interchange between FLEX [] A and [] A. */
        return ((BOOL_T) (DEFLEX (u) == DEFLEX (v)));
      }
    case ALIAS_DEFLEXING:
/* Cannot alias [] A to FLEX [] A, but vice versa is ok. */
      {
        if (u->has_ref) {
          return ((BOOL_T) (DEFLEX (u) == v));
        } else {
          return (whether_modes_equal (u, v, SAFE_DEFLEXING));
        }
      }
    case SAFE_DEFLEXING:
      {
/* Cannot alias [] A to FLEX [] A but values are ok. */
        if (!u->has_ref && !v->has_ref) {
          return (whether_modes_equal (u, v, FORCE_DEFLEXING));
        } else {
          return (A68_FALSE);
        }
    case NO_DEFLEXING:
        return (A68_FALSE);
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief yield mode to unite to
\param m mode
\param u united mode
\return same
**/

MOID_T *unites_to (MOID_T * m, MOID_T * u)
{
/* Uniting m->u. */
  MOID_T *v = NULL;
  PACK_T *p;
  if (u == MODE (SIMPLIN) || u == MODE (SIMPLOUT)) {
    return (m);
  }
  for (p = PACK (u); p != NULL; FORWARD (p)) {
/* Prefer []->[] over []->FLEX []. */
    if (m == MOID (p)) {
      v = MOID (p);
    } else if (v == NULL && DEFLEX (m) == DEFLEX (MOID (p))) {
      v = MOID (p);
    }
  }
  return (v);
}

/*!
\brief whether moid in pack
\param u mode
\param v pack
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_moid_in_pack (MOID_T * u, PACK_T * v, int deflex)
{
  for (; v != NULL; FORWARD (v)) {
    if (whether_modes_equal (u, MOID (v), deflex)) {
      return (A68_TRUE);
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether "p" is a subset of "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

BOOL_T whether_subset (MOID_T * p, MOID_T * q, int deflex)
{
  PACK_T *u = PACK (p);
  BOOL_T j = A68_TRUE;
  for (; u != NULL && j; FORWARD (u)) {
    j = (BOOL_T) (j && whether_moid_in_pack (MOID (u), PACK (q), deflex));
  }
  return (j);
}

/*!
\brief whether "p" can be united to UNION "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

BOOL_T whether_unitable (MOID_T * p, MOID_T * q, int deflex)
{
  if (WHETHER (q, UNION_SYMBOL)) {
    if (WHETHER (p, UNION_SYMBOL)) {
      return (whether_subset (p, q, deflex));
    } else {
      return (whether_moid_in_pack (p, PACK (q), deflex));
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether all or some components of "u" can be firmly coerced to a component mode of "v".
\param u mode
\param v mode 
\param all all coercible
\param some some coercible
**/

static void investigate_firm_relations (PACK_T * u, PACK_T * v, BOOL_T * all, BOOL_T * some)
{
  *all = A68_TRUE;
  *some = A68_FALSE;
  for (; v != NULL; FORWARD (v)) {
    PACK_T *w;
    BOOL_T k = A68_FALSE;
    for (w = u; w != NULL; FORWARD (w)) {
      k |= whether_coercible (MOID (w), MOID (v), FIRM, FORCE_DEFLEXING);
    }
    *some |= k;
    *all &= k;
  }
}

/*!
\brief whether there is a soft path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_softly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (whether_softly_coercible (SUB (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a weak path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_weakly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_weakly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a meek path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_meekly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_meekly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a firm path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_firmly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    return (A68_TRUE);
  } else if (whether_unitable (p, q, deflex)) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_firmly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" widens to "q"
\param p mode
\param q mode
\return same
**/

static MOID_T *widens_to (MOID_T * p, MOID_T * q)
{
  if (p == MODE (INT)) {
    if (q == MODE (LONG_INT) || q == MODE (LONGLONG_INT) || q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_INT));
    } else if (q == MODE (REAL) || q == MODE (COMPLEX)) {
      return (MODE (REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_INT)) {
    if (q == MODE (LONGLONG_INT)) {
      return (MODE (LONGLONG_INT));
    } else if (q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_INT)) {
    if (q == MODE (LONGLONG_REAL) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (REAL)) {
    if (q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_REAL));
    } else if (q == MODE (COMPLEX)) {
      return (MODE (COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (COMPLEX)) {
    if (q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_REAL)) {
    if (q == MODE (LONGLONG_REAL) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_REAL));
    } else if (q == MODE (LONG_COMPLEX)) {
      return (MODE (LONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_COMPLEX)) {
    if (q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_REAL)) {
    if (q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (BITS)) {
    if (q == MODE (LONG_BITS) || q == MODE (LONGLONG_BITS)) {
      return (MODE (LONG_BITS));
    } else if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_BITS)) {
    if (q == MODE (LONGLONG_BITS)) {
      return (MODE (LONGLONG_BITS));
    } else if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_BITS)) {
    if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (BYTES) && q == MODE (ROW_CHAR)) {
    return (MODE (ROW_CHAR));
  } else if (p == MODE (LONG_BYTES) && q == MODE (ROW_CHAR)) {
    return (MODE (ROW_CHAR));
  } else {
    return (NULL);
  }
}

/*!
\brief whether "p" widens to "q"
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_widenable (MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  if (z != NULL) {
    return ((BOOL_T) (z == q ? A68_TRUE : whether_widenable (z, q)));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" is a REF ROW
\param p mode
\return same
**/

static BOOL_T whether_ref_row (MOID_T * p)
{
  return ((BOOL_T) (p->name != NULL ? WHETHER (DEFLEX (SUB (p)), ROW_SYMBOL) : A68_FALSE));
}

/*!
\brief whether strong name
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_strong_name (MOID_T * p, MOID_T * q)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_ref_row (q)) {
    return (whether_strong_name (p, q->name));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether strong slice
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_strong_slice (MOID_T * p, MOID_T * q)
{
  if (p == q || whether_widenable (p, q)) {
    return (A68_TRUE);
  } else if (SLICE (q) != NULL) {
    return (whether_strong_slice (p, SLICE (q)));
  } else if (WHETHER (q, FLEX_SYMBOL)) {
    return (whether_strong_slice (p, SUB (q)));
  } else if (whether_ref_row (q)) {
    return (whether_strong_name (p, q));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether strongly coercible
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_strongly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
/* Keep this sequence of statements. */
/*
printf("\nstrong p: %s", moid_to_string (p, 180, NULL));
printf("\nstrong q: %s", moid_to_string (q, 180, NULL));
fflush(stdout);
*/
  if (p == q) {
    return (A68_TRUE);
  } else if (q == MODE (VOID)) {
    return (A68_TRUE);
  } else if ((q == MODE (SIMPLIN) || q == MODE (ROW_SIMPLIN)) && whether_readable_mode (p)) {
    return (A68_TRUE);
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    return (A68_TRUE);
  } else if (whether_unitable (p, derow (q), deflex)) {
    return (A68_TRUE);
  }
  if (whether_ref_row (q) && whether_strong_name (p, q)) {
    return (A68_TRUE);
  } else if (SLICE (q) != NULL && whether_strong_slice (p, q)) {
    return (A68_TRUE);
  } else if (WHETHER (q, FLEX_SYMBOL) && whether_strong_slice (p, q)) {
    return (A68_TRUE);
  } else if (whether_widenable (p, q)) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_strongly_coercible (depref_once (p), q, deflex));
  } else if (q == MODE (SIMPLOUT) || q == MODE (ROW_SIMPLOUT)) {
    return (whether_printable_mode (p));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether firm
\param p mode
\param q mode
\return same
**/

BOOL_T whether_firm (MOID_T * p, MOID_T * q)
{
  return ((BOOL_T) (whether_firmly_coercible (p, q, SAFE_DEFLEXING) || whether_firmly_coercible (q, p, SAFE_DEFLEXING)));
}

/*!
\brief whether coercible stowed
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_stowed (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c == STRONG) {
    if (q == MODE (VOID)) {
      return (A68_TRUE);
    } else if (WHETHER (q, FLEX_SYMBOL)) {
      PACK_T *u = PACK (p);
      BOOL_T j = A68_TRUE;
      for (; u != NULL && j; FORWARD (u)) {
        j &= whether_coercible (MOID (u), SLICE (SUB (q)), c, deflex);
      }
      return (j);
    } else if (WHETHER (q, ROW_SYMBOL)) {
      PACK_T *u = PACK (p);
      BOOL_T j = A68_TRUE;
      for (; u != NULL && j; FORWARD (u)) {
        j &= whether_coercible (MOID (u), SLICE (q), c, deflex);
      }
      return (j);
    } else if (WHETHER (q, PROC_SYMBOL) || WHETHER (q, STRUCT_SYMBOL)) {
      PACK_T *u = PACK (p), *v = PACK (q);
      if (DIM (p) != DIM (q)) {
        return (A68_FALSE);
      } else {
        BOOL_T j = A68_TRUE;
        while (u != NULL && v != NULL && j) {
          j &= whether_coercible (MOID (u), MOID (v), c, deflex);
          FORWARD (u);
          FORWARD (v);
        }
        return (j);
      }
    } else {
      return (A68_FALSE);
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether coercible series
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_series (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c != STRONG) {
    return (A68_FALSE);
  } else if (p == NULL || q == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, SERIES_MODE) && PACK (p) == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (q, SERIES_MODE) && PACK (q) == NULL) {
    return (A68_FALSE);
  } else if (PACK (p) == NULL) {
    return (whether_coercible (p, q, c, deflex));
  } else {
    PACK_T *u = PACK (p);
    BOOL_T j = A68_TRUE;
    for (; u != NULL && j; FORWARD (u)) {
      if (MOID (u) != NULL) {
        j &= whether_coercible (MOID (u), q, c, deflex);
      }
    }
    return (j);
  }
}

/*!
\brief basic coercions
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T basic_coercions (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (c == NO_SORT) {
    return ((BOOL_T) (p == q));
  } else if (c == SOFT) {
    return (whether_softly_coercible (p, q, deflex));
  } else if (c == WEAK) {
    return (whether_weakly_coercible (p, q, deflex));
  } else if (c == MEEK) {
    return (whether_meekly_coercible (p, q, deflex));
  } else if (c == FIRM) {
    return (whether_firmly_coercible (p, q, deflex));
  } else if (c == STRONG) {
    return (whether_strongly_coercible (p, q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" can be coerced to "q" in a "c" context
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

BOOL_T whether_coercible (MOID_T * p, MOID_T * q, int c, int deflex)
{
/*
printf("\np: %s", moid_to_string (p, 180, NULL));
printf("\nq: %s", moid_to_string (q, 180, NULL));
fflush(stdout);
*/
  if (whether_mode_isnt_well (p) || whether_mode_isnt_well (q)) {
    return (A68_TRUE);
  } else if (p == q) {
    return (A68_TRUE);
  } else if (p == MODE (HIP)) {
    return (A68_TRUE);
  } else if (WHETHER (p, STOWED_MODE)) {
    return (whether_coercible_stowed (p, q, c, deflex));
  } else if (WHETHER (p, SERIES_MODE)) {
    return (whether_coercible_series (p, q, c, deflex));
  } else if (p == MODE (VACUUM) && WHETHER (DEFLEX (q), ROW_SYMBOL)) {
    return (A68_TRUE);
  } else if (basic_coercions (p, q, c, deflex)) {
    return (A68_TRUE);
  } else if (deflex == FORCE_DEFLEXING) {
/* Allow for any interchange between FLEX [] A and [] A. */
    return (basic_coercions (DEFLEX (p), DEFLEX (q), c, FORCE_DEFLEXING));
  } else if (deflex == ALIAS_DEFLEXING) {
/* No aliasing of REF [] and REF FLEX [], but vv is ok and values too. */
    if (p->has_ref) {
      return (basic_coercions (DEFLEX (p), q, c, ALIAS_DEFLEXING));
    } else {
      return (whether_coercible (p, q, c, SAFE_DEFLEXING));
    }
  } else if (deflex == SAFE_DEFLEXING) {
/* No aliasing of REF [] and REF FLEX [], but ok and values too. */
    if (!p->has_ref && !q->has_ref) {
      return (whether_coercible (p, q, c, FORCE_DEFLEXING));
    } else {
      return (basic_coercions (p, q, c, SAFE_DEFLEXING));
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether coercible in context
\param p soid
\param q soid
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_in_context (SOID_T * p, SOID_T * q, int deflex)
{
  if (SORT (p) != SORT (q)) {
    return (A68_FALSE);
  } else if (MOID (p) == MOID (q)) {
    return (A68_TRUE);
  } else {
    return (whether_coercible (MOID (p), MOID (q), SORT (q), deflex));
  }
}

/*!
\brief whether list "y" is balanced
\param n position in tree
\param y soid list
\param sort sort
\return same
**/

static BOOL_T whether_balanced (NODE_T * n, SOID_LIST_T * y, int sort)
{
  if (sort == STRONG) {
    return (A68_TRUE);
  } else {
    BOOL_T k = A68_FALSE;
    for (; y != NULL && !k; FORWARD (y)) {
      SOID_T *z = y->yield;
      k = (BOOL_T) (WHETHER_NOT (MOID (z), STOWED_MODE));
    }
    if (k == A68_FALSE) {
      diagnostic_node (A68_ERROR, n, ERROR_NO_UNIQUE_MODE);
    }
    return (k);
  }
}

/*!
\brief a moid from "m" to which all other members can be coerced
\param m mode
\param sort sort
\param return_depreffed whether to depref
\param deflex deflexing regime
\return same
**/

MOID_T *get_balanced_mode (MOID_T * m, int sort, BOOL_T return_depreffed, int deflex)
{
  MOID_T *common = NULL;
  if (m != NULL && !whether_mode_isnt_well (m) && WHETHER (m, UNION_SYMBOL)) {
    int depref_level;
    BOOL_T go_on = A68_TRUE;
/* Test for increasing depreffing. */
    for (depref_level = 0; go_on; depref_level++) {
      PACK_T *p;
      go_on = A68_FALSE;
/* Test the whole pack. */
      for (p = PACK (m); p != NULL; FORWARD (p)) {
/* HIPs are not eligible of course. */
        if (MOID (p) != MODE (HIP)) {
          MOID_T *candidate = MOID (p);
          int k;
/* Depref as far as allowed. */
          for (k = depref_level; k > 0 && whether_deprefable (candidate); k--) {
            candidate = depref_once (candidate);
          }
/* Only need testing if all allowed deprefs succeeded. */
          if (k == 0) {
            PACK_T *q;
            MOID_T *to = return_depreffed ? depref_completely (candidate) : candidate;
            BOOL_T all_coercible = A68_TRUE;
            go_on = A68_TRUE;
            for (q = PACK (m); q != NULL && all_coercible; FORWARD (q)) {
              MOID_T *from = MOID (q);
              if (p != q && from != to) {
                all_coercible &= whether_coercible (from, to, sort, deflex);
              }
            }
/* If the pack is coercible to the candidate, we mark the candidate.
   We continue searching for longest series of REF REF PROC REF .. */
            if (all_coercible) {
              MOID_T *mark = (return_depreffed ? MOID (p) : candidate);
              if (common == NULL) {
                common = mark;
              } else if (WHETHER (candidate, FLEX_SYMBOL) && DEFLEX (candidate) == common) {
/* We prefer FLEX. */
                common = mark;
              }
            }
          }
        }
      }                         /* for */
    }                           /* for */
  }
  return (common == NULL ? m : common);
}

/*!
\brief whether we can search a common mode from a clause or not
\param att attribute
\return same
**/

static BOOL_T clause_allows_balancing (int att)
{
  switch (att) {
  case CLOSED_CLAUSE:
  case CONDITIONAL_CLAUSE:
  case INTEGER_CASE_CLAUSE:
  case SERIAL_CLAUSE:
  case UNITED_CASE_CLAUSE:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief a unique mode from "z"
\param z soid
\param deflex deflexing regime
\return same
**/

static MOID_T *determine_unique_mode (SOID_T * z, int deflex)
{
  if (z == NULL) {
    return (NULL);
  } else {
    MOID_T *x = MOID (z);
    if (whether_mode_isnt_well (x)) {
      return (MODE (ERROR));
    }
    x = make_united_mode (x);
    if (clause_allows_balancing (ATTRIBUTE (z))) {
      return (get_balanced_mode (x, STRONG, NO_DEPREF, deflex));
    } else {
      return (x);
    }
  }
}

/*!
\brief give a warning when a value is silently discarded
\param p position in tree
\param x soid
\param y soid
\param c context
**/

static void warn_for_voiding (NODE_T * p, SOID_T * x, SOID_T * y, int c)
{
  (void) c;
  if (CAST (x) == A68_FALSE) {
    if (MOID (x) == MODE (VOID) && MOID (y) != MODE (ERROR) && !(MOID (y) == MODE (VOID) || !whether_nonproc (MOID (y)))) {
      if (WHETHER (p, FORMULA)) {
        diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_VOIDED, MOID (y));
      } else {
        diagnostic_node (A68_WARNING, p, WARNING_VOIDED, MOID (y));
      }
    }
  }
}

/*!
\brief warn for things that are likely unintended
\param p position in tree
\param m moid
\param c context
\param u attribute
**/

static void semantic_pitfall (NODE_T * p, MOID_T * m, int c, int u)
{
/*
semantic_pitfall: warn for things that are likely unintended, for instance
                  REF INT i := LOC INT := 0, which should probably be
                  REF INT i = LOC INT := 0.
*/
  if (WHETHER (p, u)) {
    diagnostic_node (A68_WARNING, p, WARNING_UNINTENDED, MOID (p), u, m, c);
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    semantic_pitfall (SUB (p), m, c, u);
  }
}

/*!
\brief insert coercion "a" in the tree
\param l position in tree
\param a attribute
\param m (coerced) moid
**/

static void make_coercion (NODE_T * l, int a, MOID_T * m)
{
  make_sub (l, l, a);
  MOID (l) = depref_rows (MOID (l), m);
}

/*!
\brief make widening coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_widening_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  make_coercion (n, WIDENING, z);
  if (z != q) {
    make_widening_coercion (n, z, q);
  }
}

/*!
\brief make ref rowing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_ref_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (whether_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (whether_ref_row (q)) {
      make_ref_rowing_coercion (n, p, q->name);
      make_coercion (n, ROWING, q);
    }
  }
}

/*!
\brief make rowing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (whether_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (SLICE (q) != NULL) {
      make_rowing_coercion (n, p, SLICE (q));
      make_coercion (n, ROWING, q);
    } else if (WHETHER (q, FLEX_SYMBOL)) {
      make_rowing_coercion (n, p, SUB (q));
    } else if (whether_ref_row (q)) {
      make_ref_rowing_coercion (n, p, q);
    }
  }
}

/*!
\brief make uniting coercion
\param n position in tree
\param q mode
**/

static void make_uniting_coercion (NODE_T * n, MOID_T * q)
{
  make_coercion (n, UNITING, derow (q));
  if (WHETHER (q, ROW_SYMBOL) || WHETHER (q, FLEX_SYMBOL)) {
    make_rowing_coercion (n, derow (q), q);
  }
}

/*!
\brief make depreffing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_depreffing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) == DEFLEX (q)) {
    return;
  } else if (q == MODE (SIMPLOUT) && whether_printable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == MODE (ROW_SIMPLOUT) && whether_printable_mode (p)) {
    make_coercion (n, UNITING, MODE (SIMPLOUT));
    make_coercion (n, ROWING, MODE (ROW_SIMPLOUT));
  } else if (q == MODE (SIMPLIN) && whether_readable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == MODE (ROW_SIMPLIN) && whether_readable_mode (p)) {
    make_coercion (n, UNITING, MODE (SIMPLIN));
    make_coercion (n, ROWING, MODE (ROW_SIMPLIN));
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    make_coercion (n, UNITING, MODE (ROWS));
    MOID (n) = MODE (ROWS);
  } else if (whether_widenable (p, q)) {
    make_widening_coercion (n, p, q);
  } else if (whether_unitable (p, derow (q), SAFE_DEFLEXING)) {
    make_uniting_coercion (n, q);
  } else if (whether_ref_row (q) && whether_strong_name (p, q)) {
    make_ref_rowing_coercion (n, p, q);
  } else if (SLICE (q) != NULL && whether_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (WHETHER (q, FLEX_SYMBOL) && whether_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (WHETHER (p, REF_SYMBOL)) {
    MOID_T *r = /* DEFLEX */ (SUB (p));
    make_coercion (n, DEREFERENCING, r);
    make_depreffing_coercion (n, r, q);
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    MOID_T *r = SUB (p);
    make_coercion (n, DEPROCEDURING, r);
    make_depreffing_coercion (n, r, q);
  } else if (p != q) {
    cannot_coerce (n, p, q, NO_SORT, SKIP_DEFLEXING, 0);
  }
}

/*!
\brief whether p is a nonproc mode (that is voided directly)
\param p mode
\return same
**/

static BOOL_T whether_nonproc (MOID_T * p)
{
  if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (whether_nonproc (SUB (p)));
  } else {
    return (A68_TRUE);
  }
}

/*!
\brief make_void: voiden in an appropriate way
\param p position in tree
\param q mode
**/

static void make_void (NODE_T * p, MOID_T * q)
{
  switch (ATTRIBUTE (p)) {
  case ASSIGNATION:
  case IDENTITY_RELATION:
  case GENERATOR:
  case CAST:
  case DENOTATION:
    {
      make_coercion (p, VOIDING, MODE (VOID));
      return;
    }
  }
/* MORFs are an involved case. */
  switch (ATTRIBUTE (p)) {
  case SELECTION:
  case SLICE:
  case ROUTINE_TEXT:
  case FORMULA:
  case CALL:
  case IDENTIFIER:
    {
/* A nonproc moid value is eliminated directly. */
      if (whether_nonproc (q)) {
        make_coercion (p, VOIDING, MODE (VOID));
        return;
      } else {
/* Descend the chain of e.g. REF PROC .. until a nonproc moid remains. */
        MOID_T *z = q;
        while (!whether_nonproc (z)) {
          if (WHETHER (z, REF_SYMBOL)) {
            make_coercion (p, DEREFERENCING, SUB (z));
          }
          if (WHETHER (z, PROC_SYMBOL) && NODE_PACK (p) == NULL) {
            make_coercion (p, DEPROCEDURING, SUB (z));
          }
          z = SUB (z);
        }
        if (z != MODE (VOID)) {
          make_coercion (p, VOIDING, MODE (VOID));
        }
        return;
      }
    }
  }
/* All other is voided straight away. */
  make_coercion (p, VOIDING, MODE (VOID));
}

/*!
\brief make strong coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_strong (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (q == MODE (VOID) && p != MODE (VOID)) {
    make_void (n, p);
  } else {
    make_depreffing_coercion (n, p, q);
  }
}

/*!
\brief mode check on bounds
\param p position in tree
**/

static void mode_check_bounds (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, MODE (INT), 0);
    mode_check_unit (p, &x, &y);
    if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&y), MODE (INT), MEEK, SAFE_DEFLEXING, UNIT);
    }
    mode_check_bounds (NEXT (p));
  } else {
    mode_check_bounds (SUB (p));
    mode_check_bounds (NEXT (p));
  }
}

/*!
\brief mode check declarer
\param p position in tree
**/

static void mode_check_declarer (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, BOUNDS)) {
    mode_check_bounds (SUB (p));
    mode_check_declarer (NEXT (p));
  } else {
    mode_check_declarer (SUB (p));
    mode_check_declarer (NEXT (p));
  }
}

/*!
\brief mode check identity declaration
\param p position in tree
**/

static void mode_check_identity_declaration (NODE_T * p)
{
  if (p != NULL) {
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
        if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
          cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
        } else if (MOID (&x) != MOID (&y)) {
/* Check for instance, REF INT i = LOC REF INT. */
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

/*!
\brief mode check variable declaration
\param p position in tree
**/

static void mode_check_variable_declaration (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        mode_check_declarer (SUB (p));
        mode_check_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
          SOID_T x, y;
          make_soid (&x, STRONG, SUB_MOID (p), 0);
          mode_check_unit (NEXT_NEXT (p), &x, &y);
          if (!whether_coercible_in_context (&y, &x, FORCE_DEFLEXING)) {
            cannot_coerce (p, MOID (&y), MOID (&x), STRONG, FORCE_DEFLEXING, UNIT);
          } else if (SUB_MOID (&x) != MOID (&y)) {
/* Check for instance, REF INT i = LOC REF INT. */
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

/*!
\brief mode check routine text
\param p position in tree
\param y resulting soid
**/

static void mode_check_routine_text (NODE_T * p, SOID_T * y)
{
  SOID_T w;
  if (WHETHER (p, PARAMETER_PACK)) {
    mode_check_declarer (SUB (p));
    FORWARD (p);
  }
  mode_check_declarer (SUB (p));
  make_soid (&w, STRONG, MOID (p), 0);
  mode_check_unit (NEXT_NEXT (p), &w, y);
  if (!whether_coercible_in_context (y, &w, FORCE_DEFLEXING)) {
    cannot_coerce (NEXT_NEXT (p), MOID (y), MOID (&w), STRONG, FORCE_DEFLEXING, UNIT);
  }
}

/*!
\brief mode check proc declaration
\param p position in tree
**/

static void mode_check_proc_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, NULL, 0);
    mode_check_routine_text (SUB (p), &y);
  } else {
    mode_check_proc_declaration (SUB (p));
    mode_check_proc_declaration (NEXT (p));
  }
}

/*!
\brief mode check brief op declaration
\param p position in tree
**/

static void mode_check_brief_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
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

/*!
\brief mode check op declaration
\param p position in tree
**/

static void mode_check_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    SOID_T y, x;
    make_soid (&x, STRONG, MOID (p), 0);
    mode_check_unit (NEXT_NEXT (p), &x, &y);
    if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
    }
  } else {
    mode_check_op_declaration (SUB (p));
    mode_check_op_declaration (NEXT (p));
  }
}

/*!
\brief mode check declaration list
\param p position in tree
**/

static void mode_check_declaration_list (NODE_T * p)
{
  if (p != NULL) {
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

/*!
\brief mode check serial clause
\param r resulting soids
\param p position in tree
\param x expected soid
\param k whether statement yields a value other than VOID
**/

static void mode_check_serial (SOID_LIST_T ** r, NODE_T * p, SOID_T * x, BOOL_T k)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, INITIALISER_SERIES)) {
    mode_check_serial (r, SUB (p), x, A68_FALSE);
    mode_check_serial (r, NEXT (p), x, k);
  } else if (WHETHER (p, DECLARATION_LIST)) {
    mode_check_declaration_list (SUB (p));
  } else if (whether_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, NULL_ATTRIBUTE)) {
    mode_check_serial (r, NEXT (p), x, k);
  } else if (whether_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, NULL_ATTRIBUTE)) {
    if (NEXT (p) != NULL) {
      if (WHETHER (NEXT (p), EXIT_SYMBOL) || WHETHER (NEXT (p), END_SYMBOL) || WHETHER (NEXT (p), CLOSE_SYMBOL)) {
        mode_check_serial (r, SUB (p), x, A68_TRUE);
      } else {
        mode_check_serial (r, SUB (p), x, A68_FALSE);
      }
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      mode_check_serial (r, SUB (p), x, A68_TRUE);
    }
  } else if (WHETHER (p, LABELED_UNIT)) {
    mode_check_serial (r, SUB (p), x, k);
  } else if (WHETHER (p, UNIT)) {
    SOID_T y;
    if (k) {
      mode_check_unit (p, x, &y);
    } else {
      SOID_T w;
      make_soid (&w, STRONG, MODE (VOID), 0);
      mode_check_unit (p, &w, &y);
    }
    if (NEXT (p) != NULL) {
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      if (k) {
        add_to_soid_list (r, p, &y);
      }
    }
  }
}

/*!
\brief mode check serial clause units
\param p position in tree
\param x expected soid
\param y resulting soid
\param att attribute (SERIAL or ENQUIRY)
**/

static void mode_check_serial_units (NODE_T * p, SOID_T * x, SOID_T * y, int att)
{
  SOID_LIST_T *top_sl = NULL;
  (void) att;
  mode_check_serial (&top_sl, SUB (p), x, A68_TRUE);
  if (whether_balanced (p, top_sl, SORT (x))) {
    MOID_T *result = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), result, SERIAL_CLAUSE);
  } else {
    make_soid (y, SORT (x), MOID (x) != NULL ? MOID (x) : MODE (ERROR), 0);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check unit list
\param r resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_unit_list (SOID_LIST_T ** r, NODE_T * p, SOID_T * x)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    mode_check_unit_list (r, SUB (p), x);
    mode_check_unit_list (r, NEXT (p), x);
  } else if (WHETHER (p, COMMA_SYMBOL)) {
    mode_check_unit_list (r, NEXT (p), x);
  } else if (WHETHER (p, UNIT)) {
    SOID_T y;
    mode_check_unit (p, x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_unit_list (r, NEXT (p), x);
  }
}

/*!
\brief mode check struct display
\param r resulting soids
\param p position in tree
\param fields pack
**/

static void mode_check_struct_display (SOID_LIST_T ** r, NODE_T * p, PACK_T ** fields)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    mode_check_struct_display (r, SUB (p), fields);
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (WHETHER (p, COMMA_SYMBOL)) {
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (WHETHER (p, UNIT)) {
    SOID_T x, y;
    if (*fields != NULL) {
      make_soid (&x, STRONG, MOID (*fields), 0);
      FORWARD (*fields);
    } else {
      make_soid (&x, STRONG, NULL, 0);
    }
    mode_check_unit (p, &x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_struct_display (r, NEXT (p), fields);
  }
}

/*!
\brief mode check get specified moids
\param p position in tree
\param u united mode to add to
**/

static void mode_check_get_specified_moids (NODE_T * p, MOID_T * u)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      mode_check_get_specified_moids (SUB (p), u);
    } else if (WHETHER (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      add_mode_to_pack (&(PACK (u)), m, NULL, NODE (m));
    }
  }
}

/*!
\brief mode check specified unit list
\param r resulting soids
\param p position in tree
\param x expected soid
\param u resulting united mode
**/

static void mode_check_specified_unit_list (SOID_LIST_T ** r, NODE_T * p, SOID_T * x, MOID_T * u)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      mode_check_specified_unit_list (r, SUB (p), x, u);
    } else if (WHETHER (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      if (u != NULL && !whether_unitable (m, u, SAFE_DEFLEXING)) {
        diagnostic_node (A68_ERROR, p, ERROR_NO_COMPONENT, m, u);
      }
    } else if (WHETHER (p, UNIT)) {
      SOID_T y;
      mode_check_unit (p, x, &y);
      add_to_soid_list (r, p, &y);
    }
  }
}

/*!
\brief mode check united case parts
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_united_case_parts (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  MOID_T *u = NULL, *v = NULL, *w = NULL;
/* Check the CASE part and deduce the united mode. */
  make_soid (&enq_expct, STRONG, NULL, 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
/* Deduce the united mode from the enquiry clause. */
  u = depref_completely (MOID (&enq_yield));
  u = make_united_mode (u);
  u = depref_completely (u);
/* Also deduce the united mode from the specifiers. */
  v = new_moid ();
  ATTRIBUTE (v) = SERIES_MODE;
  mode_check_get_specified_moids (NEXT_SUB (NEXT (p)), v);
  v = make_united_mode (v);
/* Determine a resulting union. */
  if (u == MODE (HIP)) {
    w = v;
  } else {
    if (WHETHER (u, UNION_SYMBOL)) {
      BOOL_T uv, vu, some;
      investigate_firm_relations (PACK (u), PACK (v), &uv, &some);
      investigate_firm_relations (PACK (v), PACK (u), &vu, &some);
      if (uv && vu) {
/* Every component has a specifier. */
        w = u;
      } else if (!uv && !vu) {
/* Hmmmm ... let the coercer sort it out. */
        w = u;
      } else {
/*  This is all the balancing we allow here for the moment. Firmly related
subsets are not valid so we absorb them. If this doesn't solve it then we
get a coercion-error later. */
        w = absorb_related_subsets (u);
      }
    } else {
      diagnostic_node (A68_ERROR, NEXT_SUB (p), ERROR_NO_UNION, u);
      return;
    }
  }
  MOID (SUB (p)) = w;
  FORWARD (p);
/* Check the IN part. */
  mode_check_specified_unit_list (ry, NEXT_SUB (p), x, w);
/* OUSE, OUT, ESAC. */
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, UNITED_OUSE_PART, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE)) {
      mode_check_united_case_parts (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check united case
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_united_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_united_case_parts (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), UNITED_CASE_CLAUSE);

    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, UNITED_CASE_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check unit list 2
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_unit_list_2 (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  if (MOID (x) != NULL) {
    if (WHETHER (MOID (x), FLEX_SYMBOL)) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (SUB_MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (WHETHER (MOID (x), ROW_SYMBOL)) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (WHETHER (MOID (x), STRUCT_SYMBOL)) {
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

/*!
\brief mode check closed
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_closed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, SERIAL_CLAUSE)) {
    mode_check_serial_units (p, x, y, SERIAL_CLAUSE);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
    mode_check_closed (NEXT (p), x, y);
  }
  MOID (p) = MOID (y);
}

/*!
\brief mode check collateral
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_collateral (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (whether (p, BEGIN_SYMBOL, END_SYMBOL, 0)
             || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0)) {
    if (SORT (x) == STRONG) {
      make_soid (y, STRONG, MODE (VACUUM), 0);
    } else {
      make_soid (y, STRONG, MODE (UNDEFINED), 0);
    }
  } else {
    if (WHETHER (p, UNIT_LIST)) {
      mode_check_unit_list_2 (p, x, y);
    } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
      mode_check_collateral (NEXT (p), x, y);
    }
    MOID (p) = MOID (y);
  }
}

/*!
\brief mode check conditional 2
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_conditional_2 (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, ELSE_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, ELIF_PART, BRIEF_ELIF_IF_PART, NULL_ATTRIBUTE)) {
      mode_check_conditional_2 (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check conditional
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_conditional (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_conditional_2 (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), CONDITIONAL_CLAUSE);
    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, CONDITIONAL_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check int case 2
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_int_case_2 (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, STRONG, MODE (INT), 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_unit_list (ry, NEXT_SUB (p), x);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, INTEGER_OUT_PART, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE)) {
      mode_check_int_case_2 (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check int case
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_int_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_int_case_2 (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), INTEGER_CASE_CLAUSE);
    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, INTEGER_CASE_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check loop 2
\param p position in tree
\param y resulting soid
**/

static void mode_check_loop_2 (NODE_T * p, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, FOR_PART)) {
    mode_check_loop_2 (NEXT (p), y);
  } else if (whether_one_of (p, FROM_PART, BY_PART, TO_PART, NULL_ATTRIBUTE)) {
    SOID_T ix, iy;
    make_soid (&ix, STRONG, MODE (INT), 0);
    mode_check_unit (NEXT_SUB (p), &ix, &iy);
    if (!whether_coercible_in_context (&iy, &ix, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_SUB (p), MOID (&iy), MODE (INT), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (WHETHER (p, WHILE_PART)) {
    SOID_T enq_expct, enq_yield;
    make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
    mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
    if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (whether_one_of (p, DO_PART, ALT_DO_PART, NULL_ATTRIBUTE)) {
    SOID_LIST_T *z = NULL;
    SOID_T ix;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&ix, STRONG, MODE (VOID), 0);
    if (WHETHER (do_p, SERIAL_CLAUSE)) {
      mode_check_serial (&z, do_p, &ix, A68_TRUE);
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
      SOID_T enq_expct, enq_yield;
      make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
      mode_check_serial_units (NEXT_SUB (un_p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
      if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
        cannot_coerce (un_p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
      }
    }
    free_soid_list (z);
  }
}

/*!
\brief mode check loop
\param p position in tree
\param y resulting soid
**/

static void mode_check_loop (NODE_T * p, SOID_T * y)
{
  SOID_T *z = NULL;
  mode_check_loop_2 (p, /* y. */ z);
  make_soid (y, STRONG, MODE (VOID), 0);
}

/*!
\brief mode check enclosed
\param p position in tree
\param x expected soid
\param y resulting soid
**/

void mode_check_enclosed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    mode_check_closed (SUB (p), x, y);
  } else if (WHETHER (p, PARALLEL_CLAUSE)) {
    mode_check_collateral (SUB (NEXT_SUB (p)), x, y);
    make_soid (y, STRONG, MODE (VOID), 0);
    MOID (NEXT_SUB (p)) = MODE (VOID);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    mode_check_collateral (SUB (p), x, y);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    mode_check_conditional (SUB (p), x, y);
  } else if (WHETHER (p, INTEGER_CASE_CLAUSE)) {
    mode_check_int_case (SUB (p), x, y);
  } else if (WHETHER (p, UNITED_CASE_CLAUSE)) {
    mode_check_united_case (SUB (p), x, y);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    mode_check_loop (SUB (p), y);
  }
  MOID (p) = MOID (y);
}

/*!
\brief search table for operator
\param t tag chain to search
\param n name of operator
\param x lhs mode
\param y rhs mode
\param deflex deflexing regime
\return tag entry or NULL
**/

static TAG_T *search_table_for_operator (TAG_T * t, char *n, MOID_T * x, MOID_T * y, int deflex)
{
  if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
  for (; t != NULL; FORWARD (t)) {
    if (SYMBOL (NODE (t)) == n) {
      PACK_T *p = PACK (MOID (t));
      if (whether_coercible (x, MOID (p), FIRM, deflex)) {
        FORWARD (p);
        if (p == NULL && y == NULL) {
/* Matched in case of a monad. */
          return (t);
        } else if (p != NULL && y != NULL && whether_coercible (y, MOID (p), FIRM, deflex)) {
/* Matched in case of a nomad. */
          return (t);
        }
      }
    }
  }
  return (NULL);
}

/*!
\brief search chain of symbol tables and return matching operator "x n y" or "n x"
\param s symbol table to start search
\param n name of token
\param x lhs mode
\param y rhs mode
\param deflex deflexing regime
\return tag entry or NULL
**/

static TAG_T *search_table_chain_for_operator (SYMBOL_TABLE_T * s, char *n, MOID_T * x, MOID_T * y, int deflex)
{
  if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
  while (s != NULL) {
    TAG_T *z = search_table_for_operator (s->operators, n, x, y, deflex);
    if (z != NULL) {
      return (z);
    }
    s = PREVIOUS (s);
  }
  return (NULL);
}

/*!
\brief return a matching operator "x n y"
\param s symbol table to start search
\param n name of token
\param x lhs mode
\param y rhs mode
\return tag entry or NULL
**/

static TAG_T *find_operator (SYMBOL_TABLE_T * s, char *n, MOID_T * x, MOID_T * y)
{
/* Coercions to operand modes are FIRM. */
  TAG_T *z;
  MOID_T *u, *v;
/* (A) Catch exceptions first. */
  if (x == NULL && y == NULL) {
    return (NULL);
  } else if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
/* (B) MONADs. */
  if (x != NULL && y == NULL) {
    z = search_table_chain_for_operator (s, n, x, NULL, SAFE_DEFLEXING);
    return (z);
  }
/* (C) NOMADs. */
  z = search_table_chain_for_operator (s, n, x, y, SAFE_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
/* (D) Vector and matrix "strong coercions" in standard environ. */
  u = depref_completely (x);
  v = depref_completely (y);
  if ((u == MODE (ROW_REAL) || u == MODE (ROWROW_REAL))
      || (v == MODE (ROW_REAL) || v == MODE (ROWROW_REAL))
      || (u == MODE (ROW_COMPLEX) || u == MODE (ROWROW_COMPLEX))
      || (v == MODE (ROW_COMPLEX) || v == MODE (ROWROW_COMPLEX))) {
    if (u == MODE (INT)) {
      z = search_table_for_operator (stand_env->operators, n, MODE (REAL), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
      z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (v == MODE (INT)) {
      z = search_table_for_operator (stand_env->operators, n, x, MODE (REAL), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
      z = search_table_for_operator (stand_env->operators, n, x, MODE (COMPLEX), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (u == MODE (REAL)) {
      z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (v == MODE (REAL)) {
      z = search_table_for_operator (stand_env->operators, n, x, MODE (COMPLEX), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    }
  }
/* (E) Look in standenv for an appropriate cross-term. */
  u = make_series_from_moids (x, y);
  u = make_united_mode (u);
  v = get_balanced_mode (u, STRONG, NO_DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (stand_env->operators, n, v, v, ALIAS_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
  if (whether_coercible_series (u, MODE (REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (REAL), MODE (REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONG_REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONG_REAL), MODE (LONG_REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONGLONG_REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), MODE (COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONGLONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
/* (F) Now allow for depreffing for REF REAL +:= INT and alike. */
  v = get_balanced_mode (u, STRONG, DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (stand_env->operators, n, v, v, ALIAS_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
  return (NULL);
}

/*!
\brief mode check monadic operator
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_monadic_operator (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p != NULL) {
    TAG_T *t;
    MOID_T *u;
    u = determine_unique_mode (y, SAFE_DEFLEXING);
    if (whether_mode_isnt_well (u)) {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (u == MODE (HIP)) {
      diagnostic_node (A68_ERROR, NEXT (p), ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else {
      if (a68g_strchr (NOMADS, *(SYMBOL (p))) != NULL) {
        t = NULL;
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      } else {
        t = find_operator (SYMBOL_TABLE (p), SYMBOL (p), u, NULL);
        if (t == NULL) {
          diagnostic_node (A68_ERROR, p, ERROR_NO_MONADIC, u);
          make_soid (y, SORT (x), MODE (ERROR), 0);
        }
      }
      if (t != NULL) {
        MOID (p) = MOID (t);
      }
      TAX (p) = t;
      if (t != NULL && t != error_tag) {
        MOID (p) = MOID (t);
        make_soid (y, SORT (x), SUB_MOID (t), 0);
      } else {
        MOID (p) = MODE (ERROR);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
    }
  }
}

/*!
\brief mode check monadic formula
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_monadic_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e;
  make_soid (&e, FIRM, NULL, 0);
  mode_check_formula (NEXT (p), &e, y);
  mode_check_monadic_operator (p, &e, y);
  make_soid (y, SORT (x), MOID (y), 0);
}

/*!
\brief mode check formula
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T ls, rs;
  TAG_T *op;
  MOID_T *u, *v;
  if (WHETHER (p, MONADIC_FORMULA)) {
    mode_check_monadic_formula (SUB (p), x, &ls);
  } else if (WHETHER (p, FORMULA)) {
    mode_check_formula (SUB (p), x, &ls);
  } else if (WHETHER (p, SECONDARY)) {
    SOID_T e;
    make_soid (&e, FIRM, NULL, 0);
    mode_check_unit (SUB (p), &e, &ls);
  }
  u = determine_unique_mode (&ls, SAFE_DEFLEXING);
  MOID (p) = u;
  if (NEXT (p) == NULL) {
    make_soid (y, SORT (x), u, 0);
  } else {
    NODE_T *q = NEXT_NEXT (p);
    if (WHETHER (q, MONADIC_FORMULA)) {
      mode_check_monadic_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (WHETHER (q, FORMULA)) {
      mode_check_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (WHETHER (q, SECONDARY)) {
      SOID_T e;
      make_soid (&e, FIRM, NULL, 0);
      mode_check_unit (SUB (q), &e, &rs);
    }
    v = determine_unique_mode (&rs, SAFE_DEFLEXING);
    MOID (q) = v;
    if (whether_mode_isnt_well (u) || whether_mode_isnt_well (v)) {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (u == MODE (HIP)) {
      diagnostic_node (A68_ERROR, p, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (v == MODE (HIP)) {
      diagnostic_node (A68_ERROR, q, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else {
      op = find_operator (SYMBOL_TABLE (NEXT (p)), SYMBOL (NEXT (p)), u, v);
      if (op == NULL) {
        diagnostic_node (A68_ERROR, NEXT (p), ERROR_NO_DYADIC, u, v);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
      if (op != NULL) {
        MOID (NEXT (p)) = MOID (op);
      }
      TAX (NEXT (p)) = op;
      if (op != NULL && op != error_tag) {
        make_soid (y, SORT (x), SUB_MOID (op), 0);
      } else {
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
    }
  }
}

/*!
\brief mode check assignation
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_assignation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T name, tmp, value;
  MOID_T *name_moid, *source_moid, *dest_moid, *ori;
/* Get destination mode. */
  make_soid (&name, SOFT, NULL, 0);
  mode_check_unit (SUB (p), &name, &tmp);
  dest_moid = MOID (&tmp);
/* SOFT coercion. */
  ori = determine_unique_mode (&tmp, SAFE_DEFLEXING);
  name_moid = deproc_completely (ori);
  if (ATTRIBUTE (name_moid) != REF_SYMBOL) {
    if (WHETHER_MODE_IS_WELL (name_moid)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_NAME, ori, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (p) = name_moid;
/* Get source mode. */
  make_soid (&name, STRONG, SUB (name_moid), 0);
  mode_check_unit (NEXT_NEXT (p), &name, &value);
/*
where (STDOUT_FILENO, p);
printf("\ndst: %s", moid_to_string (name_moid, 180, NULL));
printf("\nsrc: %s", moid_to_string (MOID (&value), 180, NULL));fflush (stdout);
fflush(stdout);
*/
  if (!whether_coercible_in_context (&value, &name, FORCE_DEFLEXING)) {
    source_moid = MOID (&value);
    cannot_coerce (p, MOID (&value), MOID (&name), STRONG, FORCE_DEFLEXING, UNIT);
    make_soid (y, SORT (x), MODE (ERROR), 0);
  } else {
    make_soid (y, SORT (x), name_moid, 0);
  }
}

/*!
\brief mode check identity relation
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_identity_relation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  MOID_T *lhs, *rhs, *oril, *orir;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, SOFT, NULL, 0);
  mode_check_unit (SUB (ln), &e, &l);
  mode_check_unit (SUB (rn), &e, &r);
/* SOFT coercion. */
  oril = determine_unique_mode (&l, SAFE_DEFLEXING);
  orir = determine_unique_mode (&r, SAFE_DEFLEXING);
  lhs = deproc_completely (oril);
  rhs = deproc_completely (orir);
  if (WHETHER_MODE_IS_WELL (lhs) && lhs != MODE (HIP) && ATTRIBUTE (lhs) != REF_SYMBOL) {
    diagnostic_node (A68_ERROR, ln, ERROR_NO_NAME, oril, ATTRIBUTE (SUB (ln)));
    lhs = MODE (ERROR);
  }
  if (WHETHER_MODE_IS_WELL (rhs) && rhs != MODE (HIP) && ATTRIBUTE (rhs) != REF_SYMBOL) {
    diagnostic_node (A68_ERROR, rn, ERROR_NO_NAME, orir, ATTRIBUTE (SUB (rn)));
    rhs = MODE (ERROR);
  }
  if (lhs == MODE (HIP) && rhs == MODE (HIP)) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_UNIQUE_MODE);
  }
  if (whether_coercible (lhs, rhs, STRONG, SAFE_DEFLEXING)) {
    lhs = rhs;
  } else if (whether_coercible (rhs, lhs, STRONG, SAFE_DEFLEXING)) {
    rhs = lhs;
  } else {
    cannot_coerce (NEXT (p), rhs, lhs, SOFT, SKIP_DEFLEXING, TERTIARY);
    lhs = rhs = MODE (ERROR);
  }
  MOID (ln) = lhs;
  MOID (rn) = rhs;
  make_soid (y, SORT (x), MODE (BOOL), 0);
}

/*!
\brief mode check bool functions ANDF and ORF
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_bool_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, STRONG, MODE (BOOL), 0);
  mode_check_unit (SUB (ln), &e, &l);
  if (!whether_coercible_in_context (&l, &e, SAFE_DEFLEXING)) {
    cannot_coerce (ln, MOID (&l), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  mode_check_unit (SUB (rn), &e, &r);
  if (!whether_coercible_in_context (&r, &e, SAFE_DEFLEXING)) {
    cannot_coerce (rn, MOID (&r), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  MOID (ln) = MODE (BOOL);
  MOID (rn) = MODE (BOOL);
  make_soid (y, SORT (x), MODE (BOOL), 0);
}

/*!
\brief mode check cast
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_cast (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w;
  mode_check_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  w.cast = A68_TRUE;
  mode_check_enclosed (SUB_NEXT (p), &w, y);
  if (!whether_coercible_in_context (y, &w, SAFE_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (y), MOID (&w), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
  }
  make_soid (y, SORT (x), MOID (p), 0);
}

/*!
\brief mode check assertion
\param p position in tree
**/

static void mode_check_assertion (NODE_T * p)
{
  SOID_T w, y;
  make_soid (&w, STRONG, MODE (BOOL), 0);
  mode_check_enclosed (SUB_NEXT (p), &w, &y);
  SORT (&y) = SORT (&w);        /* Patch. */
  if (!whether_coercible_in_context (&y, &w, NO_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (&y), MOID (&w), MEEK, NO_DEFLEXING, ENCLOSED_CLAUSE);
  }
}

/*!
\brief mode check argument list
\param r resulting soids
\param p position in tree
\param x proc argument pack
\param v partial locale pack
\param w partial proc pack
**/

static void mode_check_argument_list (SOID_LIST_T ** r, NODE_T * p, PACK_T ** x, PACK_T ** v, PACK_T ** w)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, GENERIC_ARGUMENT_LIST)) {
      ATTRIBUTE (p) = ARGUMENT_LIST;
    }
    if (WHETHER (p, ARGUMENT_LIST)) {
      mode_check_argument_list (r, SUB (p), x, v, w);
    } else if (WHETHER (p, UNIT)) {
      SOID_T y, z;
      if (*x != NULL) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MOID (*x), NULL, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NULL, 0);
      }
      mode_check_unit (p, &z, &y);
      add_to_soid_list (r, p, &y);
    } else if (WHETHER (p, TRIMMER)) {
      SOID_T z;
      if (SUB (p) != NULL) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, ARGUMENT);
        make_soid (&z, STRONG, MODE (ERROR), 0);
        add_mode_to_pack_end (v, MODE (VOID), NULL, p);
        add_mode_to_pack_end (w, MOID (*x), NULL, p);
        FORWARD (*x);
      } else if (*x != NULL) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MODE (VOID), NULL, p);
        add_mode_to_pack_end (w, MOID (*x), NULL, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NULL, 0);
      }
      add_to_soid_list (r, p, &z);
    } else if (WHETHER (p, SUB_SYMBOL) && !program.options.brackets) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, CALL);
    }
  }
}

/*!
\brief mode check argument list 2
\param p position in tree
\param x proc argument pack
\param y soid
\param v partial locale pack
\param w partial proc pack
**/

static void mode_check_argument_list_2 (NODE_T * p, PACK_T * x, SOID_T * y, PACK_T ** v, PACK_T ** w)
{
  SOID_LIST_T *top_sl = NULL;
  mode_check_argument_list (&top_sl, SUB (p), &x, v, w);
  make_soid (y, STRONG, pack_soids_in_moid (top_sl, STOWED_MODE), 0);
  free_soid_list (top_sl);
}

/*!
\brief mode check meek int
\param p position in tree
**/

static void mode_check_meek_int (NODE_T * p)
{
  SOID_T x, y;
  make_soid (&x, STRONG, MODE (INT), 0);
  mode_check_unit (p, &x, &y);
  if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&y), MOID (&x), MEEK, SAFE_DEFLEXING, 0);
  }
}

/*!
\brief mode check trimmer
\param p position in tree
**/

static void mode_check_trimmer (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, TRIMMER)) {
    mode_check_trimmer (SUB (p));
  } else if (WHETHER (p, UNIT)) {
    mode_check_meek_int (p);
    mode_check_trimmer (NEXT (p));
  } else {
    mode_check_trimmer (NEXT (p));
  }
}

/*!
\brief mode check indexer
\param p position in tree
\param subs subscript counter
\param trims trimmer counter
**/

static void mode_check_indexer (NODE_T * p, int *subs, int *trims)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, TRIMMER)) {
    (*trims)++;
    mode_check_trimmer (SUB (p));
  } else if (WHETHER (p, UNIT)) {
    (*subs)++;
    mode_check_meek_int (p);
  } else {
    mode_check_indexer (SUB (p), subs, trims);
    mode_check_indexer (NEXT (p), subs, trims);
  }
}

/*!
\brief mode check call
\param p position in tree
\param n mode
\param x expected soid
\param y resulting soid
**/

static void mode_check_call (NODE_T * p, MOID_T * n, SOID_T * x, SOID_T * y)
{
  SOID_T d;
  MOID (p) = n;
/* "partial_locale" is the mode of the locale. */
  GENIE (p)->partial_locale = new_moid ();
  ATTRIBUTE (GENIE (p)->partial_locale) = PROC_SYMBOL;
  PACK (GENIE (p)->partial_locale) = NULL;
  SUB (GENIE (p)->partial_locale) = SUB (n);
/* "partial_proc" is the mode of the resulting proc. */
  GENIE (p)->partial_proc = new_moid ();
  ATTRIBUTE (GENIE (p)->partial_proc) = PROC_SYMBOL;
  PACK (GENIE (p)->partial_proc) = NULL;
  SUB (GENIE (p)->partial_proc) = SUB (n);
/* Check arguments and construct modes. */
  mode_check_argument_list_2 (NEXT (p), PACK (n), &d, &PACK (GENIE (p)->partial_locale), &PACK (GENIE (p)->partial_proc));
  DIM (GENIE (p)->partial_proc) = count_pack_members (PACK (GENIE (p)->partial_proc));
  DIM (GENIE (p)->partial_locale) = count_pack_members (PACK (GENIE (p)->partial_locale));
  GENIE (p)->partial_proc = register_extra_mode (GENIE (p)->partial_proc);
  GENIE (p)->partial_locale = register_extra_mode (GENIE (p)->partial_locale);
  if (DIM (MOID (&d)) != DIM (n)) {
    diagnostic_node (A68_ERROR, p, ERROR_ARGUMENT_NUMBER, n);
    make_soid (y, SORT (x), SUB (n), 0);
/*  make_soid (y, SORT (x), MODE (ERROR), 0); */
  } else {
    if (!whether_coercible (MOID (&d), n, STRONG, ALIAS_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), n, STRONG, ALIAS_DEFLEXING, ARGUMENT);
    }
    if (DIM (GENIE (p)->partial_proc) == 0) {
      make_soid (y, SORT (x), SUB (n), 0);
    } else {
      if (program.options.portcheck) {
        diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, NEXT (p), WARNING_EXTENSION, NULL);
      }
      make_soid (y, SORT (x), GENIE (p)->partial_proc, 0);
    }
  }
}

/*!
\brief mode check slice
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL or a SLICE
**/

static void mode_check_slice (NODE_T * p, MOID_T * ori, SOID_T * x, SOID_T * y)
{
  BOOL_T whether_ref;
  int rowdim, subs, trims;
  MOID_T *m = depref_completely (ori), *n = ori;
/* WEAK coercion. */
  while ((WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) || (WHETHER (n, PROC_SYMBOL) && PACK (n) == NULL)) {
    n = depref_once (n);
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_ROW_OR_PROC, n, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
  }
  MOID (p) = n;
  subs = trims = 0;
  mode_check_indexer (SUB_NEXT (p), &subs, &trims);
  if ((whether_ref = whether_ref_row (n)) != 0) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if ((subs + trims) != rowdim) {
    diagnostic_node (A68_ERROR, p, ERROR_INDEXER_NUMBER, n);
    make_soid (y, SORT (x), MODE (ERROR), 0);
  } else {
    if (subs > 0 && trims == 0) {
      ANNOTATION (NEXT (p)) = SLICE;
      m = n;
    } else {
      ANNOTATION (NEXT (p)) = TRIMMER;
      m = n;
    }
    while (subs > 0) {
      if (whether_ref) {
        m = m->name;
      } else {
        if (WHETHER (m, FLEX_SYMBOL)) {
          m = SUB (m);
        }
        m = SLICE (m);
      }
      ABEND (m == NULL, "NULL mode in mode_check_slice", NULL);
      subs--;
    }
/* A trim cannot be but deflexed
    make_soid (y, SORT (x), ANNOTATION (NEXT (p)) == TRIMMER && m->deflexed_mode != NULL ? m->deflexed_mode : m,0);
*/
    make_soid (y, SORT (x), ANNOTATION (NEXT (p)) == TRIMMER && m->trim != NULL ? m->trim : m, 0);
  }
}

/*!
\brief mode check field selection
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL, SLICE or FIELD_SELECTION
**/

static void mode_check_field_identifiers (NODE_T * p, MOID_T ** m, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      if (MOID (p) != MODE (ERROR)) {
        ATTRIBUTE (p) = FIELD_IDENTIFIER;
      }
      NODE_PACK (p) = NODE_PACK (SUB (p));
      SEQUENCE (*seq) = p;
      (*seq) = p;
      SUB (p) = NULL;
    } else if (WHETHER (p, TERTIARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, SECONDARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, PRIMARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, IDENTIFIER)) {
      BOOL_T coerce;
      MOID_T *n, *str;
      PACK_T *t, *t_2;
      char *fs;
      n = (*m);
      coerce = A68_TRUE;
      while (coerce) {
        if (WHETHER (n, STRUCT_SYMBOL)) {
          coerce = A68_FALSE;
          t = PACK (n);
        } else if (WHETHER (n, REF_SYMBOL) && (WHETHER (SUB (n), ROW_SYMBOL) || WHETHER (SUB (n), FLEX_SYMBOL)) && n->multiple_mode != NULL) {
          coerce = A68_FALSE;
          t = PACK (n->multiple_mode);
        } else if ((WHETHER (n, ROW_SYMBOL) || WHETHER (n, FLEX_SYMBOL)) && n->multiple_mode != NULL) {
          coerce = A68_FALSE;
          t = PACK (n->multiple_mode);
        } else if (WHETHER (n, REF_SYMBOL) && whether_name_struct (n)) {
          coerce = A68_FALSE;
          t = PACK (n->name);
        } else if (whether_deprefable (n)) {
          coerce = A68_TRUE;
          n = SUB (n);
          t = NULL;
        } else {
          coerce = A68_FALSE;
          t = NULL;
        }
      }
      if (t == NULL) {
        if (WHETHER_MODE_IS_WELL (*m)) {
          diagnostic_node (A68_ERROR, p, ERROR_NO_STRUCT, (*m), CONSTRUCT);
        }
        (*m) = MODE (ERROR);
        return;
      }
      fs = SYMBOL (p);
      str = n;
      while (WHETHER (str, REF_SYMBOL)) {
        str = SUB (str);
      }
      if (WHETHER (str, FLEX_SYMBOL)) {
        str = SUB (str);
      }
      if (WHETHER (str, ROW_SYMBOL)) {
        str = SUB (str);
      }
      t_2 = PACK (str);
      while (t != NULL && t_2 != NULL) {
        if (t->text == fs) {
          (*m) = MOID (t);
          MOID (p) = (*m);
          NODE_PACK (p) = t_2;
          return;
        }
        FORWARD (t);
        FORWARD (t_2);
      }
      diagnostic_node (A68_ERROR, p, ERROR_NO_FIELD, str, fs);
      (*m) = MODE (ERROR);
    } else if (WHETHER (p, GENERIC_ARGUMENT) || WHETHER (p, GENERIC_ARGUMENT_LIST)) {
      mode_check_field_identifiers (SUB (p), m, seq);
    } else if (whether_one_of (p, COMMA_SYMBOL, OPEN_SYMBOL, CLOSE_SYMBOL, SUB_SYMBOL, BUS_SYMBOL, NULL)) {
      /* ok */;
    } else {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, FIELD_IDENTIFIER);
      (*m) = MODE (ERROR);
    }
  }
}

static void mode_check_field_selection (NODE_T * p, MOID_T * m, SOID_T * x, SOID_T * y)
{
  MOID_T *ori = m;
  NODE_T *seq = p;
  mode_check_field_identifiers (NEXT (p), &ori, &seq);
  MOID (p) = MOID (SUB (p));
  make_soid (y, SORT (x), ori, 0);
}

/*!
\brief mode check specification
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL, SLICE or FIELD_SELECTION
**/

static int mode_check_specification (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  MOID_T *m, *ori;
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (SUB (p), &w, &d);
  ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  m = depref_completely (ori);
  if (WHETHER (m, PROC_SYMBOL)) {
/* Assume CALL. */
    mode_check_call (p, m, x, y);
    return (CALL);
  } else if (WHETHER (m, ROW_SYMBOL) || WHETHER (m, FLEX_SYMBOL)) {
/* Assume SLICE. */
    mode_check_slice (p, ori, x, y);
    return (SLICE);
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
    mode_check_field_selection (p, ori, x, y);
    return (FIELD_SELECTION);
  } else {
    if (m != MODE (ERROR)) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_MODE_SPECIFICATION, m, NULL);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return (PRIMARY);
  }
}

/*!
\brief mode check selection
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_selection (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  BOOL_T coerce;
  MOID_T *n, *str, *ori;
  PACK_T *t, *t_2;
  char *fs;
  NODE_T *secondary = SUB_NEXT (p);
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (secondary, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  coerce = A68_TRUE;
  while (coerce) {
    if (WHETHER (n, STRUCT_SYMBOL)) {
      coerce = A68_FALSE;
      t = PACK (n);
    } else if (WHETHER (n, REF_SYMBOL) && (WHETHER (SUB (n), ROW_SYMBOL) || WHETHER (SUB (n), FLEX_SYMBOL)) && n->multiple_mode != NULL) {
      coerce = A68_FALSE;
      t = PACK (n->multiple_mode);
    } else if ((WHETHER (n, ROW_SYMBOL) || WHETHER (n, FLEX_SYMBOL)) && n->multiple_mode != NULL) {
      coerce = A68_FALSE;
      t = PACK (n->multiple_mode);
    } else if (WHETHER (n, REF_SYMBOL) && whether_name_struct (n)) {
      coerce = A68_FALSE;
      t = PACK (n->name);
    } else if (whether_deprefable (n)) {
      coerce = A68_TRUE;
      n = SUB (n);
      t = NULL;
    } else {
      coerce = A68_FALSE;
      t = NULL;
    }
  }
  if (t == NULL) {
    if (WHETHER_MODE_IS_WELL (MOID (&d))) {
      diagnostic_node (A68_ERROR, secondary, ERROR_NO_STRUCT, ori, ATTRIBUTE (secondary));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (NEXT (p)) = n;
  fs = SYMBOL (SUB (p));
  str = n;
  while (WHETHER (str, REF_SYMBOL)) {
    str = SUB (str);
  }
  if (WHETHER (str, FLEX_SYMBOL)) {
    str = SUB (str);
  }
  if (WHETHER (str, ROW_SYMBOL)) {
    str = SUB (str);
  }
  t_2 = PACK (str);
  while (t != NULL && t_2 != NULL) {
    if (t->text == fs) {
      make_soid (y, SORT (x), MOID (t), 0);
      MOID (p) = MOID (t);
      NODE_PACK (SUB (p)) = t_2;
      return;
    }
    FORWARD (t);
    FORWARD (t_2);
  }
  make_soid (&d, NO_SORT, n, 0);
  diagnostic_node (A68_ERROR, p, ERROR_NO_FIELD, str, fs);
  make_soid (y, SORT (x), MODE (ERROR), 0);
}

/*!
\brief mode check diagonal
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_diagonal (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, STRONG, MODE (INT), 0);
    mode_check_unit (p, &w, &d);
    if (!whether_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 2) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  if (whether_ref) {
    n = NAME (n);
  } else {
    n = SLICE (n);
  }
  ABEND (n == NULL, "NULL mode in mode_check_diagonal", NULL);
  make_soid (y, SORT (x), n, 0);
}

/*!
\brief mode check transpose
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_transpose (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert = NEXT (p);
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 2) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  ABEND (n == NULL, "NULL mode in mode_check_transpose", NULL);
  make_soid (y, SORT (x), n, 0);
}

/*!
\brief mode check row or column function
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_row_column_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, STRONG, MODE (INT), 0);
    mode_check_unit (p, &w, &d);
    if (!whether_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 1) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  ABEND (n == NULL, "NULL mode in mode_check_diagonal", NULL);
  make_soid (y, SORT (x), ROWED (n), 0);
}

/*!
\brief mode check format text
\param p position in tree
**/

static void mode_check_format_text (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    mode_check_format_text (SUB (p));
    if (WHETHER (p, FORMAT_PATTERN)) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (FORMAT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (ROW_INT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (INT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    }
  }
}

/*!
\brief mode check unit
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_unit (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    mode_check_unit (SUB (p), x, y);
/* Ex primary. */
  } else if (WHETHER (p, SPECIFICATION)) {
    ATTRIBUTE (p) = mode_check_specification (SUB (p), x, y);
    if (WHETHER (p, FIELD_SELECTION) && program.options.portcheck) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_EXTENSION, NULL);
    } else if (WHETHER (p, FIELD_SELECTION)) {
      diagnostic_node (A68_WARNING, p, WARNING_EXTENSION, NULL);
    }
    warn_for_voiding (p, x, y, ATTRIBUTE (p));
  } else if (WHETHER (p, CAST)) {
    mode_check_cast (SUB (p), x, y);
    warn_for_voiding (p, x, y, CAST);
  } else if (WHETHER (p, DENOTATION)) {
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, DENOTATION);
  } else if (WHETHER (p, IDENTIFIER)) {
    if ((TAX (p) == NULL) && (MOID (p) == NULL)) {
      int att = first_tag_global (SYMBOL_TABLE (p), SYMBOL (p));
      if (att == NULL_ATTRIBUTE) {
        (void) add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
        diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
        MOID (p) = MODE (ERROR);
      } else {
        TAG_T *z = find_tag_global (SYMBOL_TABLE (p), att, SYMBOL (p));
        if (att == IDENTIFIER && z != NULL) {
          MOID (p) = MOID (z);
        } else {
          (void) add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
          diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          MOID (p) = MODE (ERROR);
        }
      }
    }
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, IDENTIFIER);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (WHETHER (p, FORMAT_TEXT)) {
    mode_check_format_text (p);
    make_soid (y, SORT (x), MODE (FORMAT), 0);
    warn_for_voiding (p, x, y, FORMAT_TEXT);
/* Ex secondary. */
  } else if (WHETHER (p, GENERATOR)) {
    mode_check_declarer (SUB (p));
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, GENERATOR);
  } else if (WHETHER (p, SELECTION)) {
    mode_check_selection (SUB (p), x, y);
    warn_for_voiding (p, x, y, SELECTION);
/* Ex tertiary. */
  } else if (WHETHER (p, NIHIL)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  } else if (WHETHER (p, FORMULA)) {
    mode_check_formula (p, x, y);
    if (WHETHER_NOT (MOID (y), REF_SYMBOL)) {
      warn_for_voiding (p, x, y, FORMULA);
    }
  } else if (WHETHER (p, DIAGONAL_FUNCTION)) {
    mode_check_diagonal (SUB (p), x, y);
    warn_for_voiding (p, x, y, DIAGONAL_FUNCTION);
  } else if (WHETHER (p, TRANSPOSE_FUNCTION)) {
    mode_check_transpose (SUB (p), x, y);
    warn_for_voiding (p, x, y, TRANSPOSE_FUNCTION);
  } else if (WHETHER (p, ROW_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, ROW_FUNCTION);
  } else if (WHETHER (p, COLUMN_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, COLUMN_FUNCTION);
/* Ex unit. */
  } else if (whether_one_of (p, JUMP, SKIP, NULL_ATTRIBUTE)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  } else if (WHETHER (p, ASSIGNATION)) {
    mode_check_assignation (SUB (p), x, y);
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    mode_check_identity_relation (SUB (p), x, y);
    warn_for_voiding (p, x, y, IDENTITY_RELATION);
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    mode_check_routine_text (SUB (p), y);
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, ROUTINE_TEXT);
  } else if (WHETHER (p, ASSERTION)) {
    mode_check_assertion (SUB (p));
    make_soid (y, STRONG, MODE (VOID), 0);
  } else if (WHETHER (p, AND_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, AND_FUNCTION);
  } else if (WHETHER (p, OR_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, OR_FUNCTION);
  }
  MOID (p) = MOID (y);
}

/*!
\brief coerce bounds
\param p position in tree
**/

static void coerce_bounds (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      SOID_T q;
      make_soid (&q, MEEK, MODE (INT), 0);
      coerce_unit (p, &q);
    } else {
      coerce_bounds (SUB (p));
    }
  }
}

/*!
\brief coerce declarer
\param p position in tree
**/

static void coerce_declarer (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOUNDS)) {
      coerce_bounds (SUB (p));
    } else {
      coerce_declarer (SUB (p));
    }
  }
}

/*!
\brief coerce identity declaration
\param p position in tree
**/

static void coerce_identity_declaration (NODE_T * p)
{
  if (p != NULL) {
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

/*!
\brief coerce variable declaration
\param p position in tree
**/

static void coerce_variable_declaration (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        coerce_declarer (SUB (p));
        coerce_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
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

/*!
\brief coerce routine text
\param p position in tree
**/

static void coerce_routine_text (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, PARAMETER_PACK)) {
    FORWARD (p);
  }
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

/*!
\brief coerce proc declaration
\param p position in tree
**/

static void coerce_proc_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
  } else {
    coerce_proc_declaration (SUB (p));
    coerce_proc_declaration (NEXT (p));
  }
}

/*!
\brief coerce_op_declaration
\param p position in tree
**/

static void coerce_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    SOID_T q;
    make_soid (&q, STRONG, MOID (p), 0);
    coerce_unit (NEXT_NEXT (p), &q);
  } else {
    coerce_op_declaration (SUB (p));
    coerce_op_declaration (NEXT (p));
  }
}

/*!
\brief coerce brief op declaration
\param p position in tree
**/

static void coerce_brief_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    coerce_routine_text (SUB (NEXT_NEXT (p)));
  } else {
    coerce_brief_op_declaration (SUB (p));
    coerce_brief_op_declaration (NEXT (p));
  }
}

/*!
\brief coerce declaration list
\param p position in tree
**/

static void coerce_declaration_list (NODE_T * p)
{
  if (p != NULL) {
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

/*!
\brief coerce serial
\param p position in tree
\param q soid
\param k whether k yields value other than VOID
**/

static void coerce_serial (NODE_T * p, SOID_T * q, BOOL_T k)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, INITIALISER_SERIES)) {
    coerce_serial (SUB (p), q, A68_FALSE);
    coerce_serial (NEXT (p), q, k);
  } else if (WHETHER (p, DECLARATION_LIST)) {
    coerce_declaration_list (SUB (p));
  } else if (whether_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_serial (NEXT (p), q, k);
  } else if (whether_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, NULL_ATTRIBUTE)) {
    NODE_T *z = NEXT (p);
    if (z != NULL) {
      if (WHETHER (z, EXIT_SYMBOL) || WHETHER (z, END_SYMBOL) || WHETHER (z, CLOSE_SYMBOL) || WHETHER (z, OCCA_SYMBOL)) {
        coerce_serial (SUB (p), q, A68_TRUE);
      } else {
        coerce_serial (SUB (p), q, A68_FALSE);
      }
    } else {
      coerce_serial (SUB (p), q, A68_TRUE);
    }
    coerce_serial (NEXT (p), q, k);
  } else if (WHETHER (p, LABELED_UNIT)) {
    coerce_serial (SUB (p), q, k);
  } else if (WHETHER (p, UNIT)) {
    if (k) {
      coerce_unit (p, q);
    } else {
      SOID_T strongvoid;
      make_soid (&strongvoid, STRONG, MODE (VOID), 0);
      coerce_unit (p, &strongvoid);
    }
  }
}

/*!
\brief coerce closed
\param p position in tree
\param q soid
**/

static void coerce_closed (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, SERIAL_CLAUSE)) {
    coerce_serial (p, q, A68_TRUE);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_closed (NEXT (p), q);
  }
}

/*!
\brief coerce conditional
\param p position in tree
\param q soid
**/

static void coerce_conditional (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (BOOL), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_serial (NEXT_SUB (p), q, A68_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, ELSE_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, ELIF_PART, BRIEF_ELIF_IF_PART, NULL_ATTRIBUTE)) {
      coerce_conditional (SUB (p), q);
    }
  }
}

/*!
\brief coerce unit list
\param p position in tree
\param q soid
**/

static void coerce_unit_list (NODE_T * p, SOID_T * q)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    coerce_unit_list (SUB (p), q);
    coerce_unit_list (NEXT (p), q);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_unit_list (NEXT (p), q);
  } else if (WHETHER (p, UNIT)) {
    coerce_unit (p, q);
    coerce_unit_list (NEXT (p), q);
  }
}

/*!
\brief coerce int case
\param p position in tree
\param q soid
**/

static void coerce_int_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (INT), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, INTEGER_OUT_PART, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE)) {
      coerce_int_case (SUB (p), q);
    }
  }
}

/*!
\brief coerce spec unit list
\param p position in tree
\param q soid
**/

static void coerce_spec_unit_list (NODE_T * p, SOID_T * q)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      coerce_spec_unit_list (SUB (p), q);
    } else if (WHETHER (p, UNIT)) {
      coerce_unit (p, q);
    }
  }
}

/*!
\brief coerce united case
\param p position in tree
\param q soid
**/

static void coerce_united_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MOID (SUB (p)), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_spec_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, UNITED_OUSE_PART, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE)) {
      coerce_united_case (SUB (p), q);
    }
  }
}

/*!
\brief coerce loop
\param p position in tree
**/

static void coerce_loop (NODE_T * p)
{
  if (WHETHER (p, FOR_PART)) {
    coerce_loop (NEXT (p));
  } else if (whether_one_of (p, FROM_PART, BY_PART, TO_PART, NULL_ATTRIBUTE)) {
    SOID_T w;
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (NEXT_SUB (p), &w);
    coerce_loop (NEXT (p));
  } else if (WHETHER (p, WHILE_PART)) {
    SOID_T w;
    make_soid (&w, MEEK, MODE (BOOL), 0);
    coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
    coerce_loop (NEXT (p));
  } else if (whether_one_of (p, DO_PART, ALT_DO_PART, NULL_ATTRIBUTE)) {
    SOID_T w;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&w, STRONG, MODE (VOID), 0);
    coerce_serial (do_p, &w, A68_TRUE);
    if (WHETHER (do_p, SERIAL_CLAUSE)) {
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
      SOID_T sw;
      make_soid (&sw, MEEK, MODE (BOOL), 0);
      coerce_serial (NEXT_SUB (un_p), &sw, A68_TRUE);
    }
  }
}

/*!
\brief coerce struct display
\param r pack
\param p position in tree
**/

static void coerce_struct_display (PACK_T ** r, NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    coerce_struct_display (r, SUB (p));
    coerce_struct_display (r, NEXT (p));
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_struct_display (r, NEXT (p));
  } else if (WHETHER (p, UNIT)) {
    SOID_T s;
    make_soid (&s, STRONG, MOID (*r), 0);
    coerce_unit (p, &s);
    FORWARD (*r);
    coerce_struct_display (r, NEXT (p));
  }
}

/*!
\brief coerce collateral
\param p position in tree
\param q soid
**/

static void coerce_collateral (NODE_T * p, SOID_T * q)
{
  if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, 0) || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0))) {
    if (WHETHER (MOID (q), STRUCT_SYMBOL)) {
      PACK_T *t = PACK (MOID (q));
      coerce_struct_display (&t, p);
    } else if (WHETHER (MOID (q), FLEX_SYMBOL)) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (SUB_MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else if (WHETHER (MOID (q), ROW_SYMBOL)) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else {
/* if (MOID (q) != MODE (VOID)) */
      coerce_unit_list (p, q);
    }
  }
}

/*!
\brief coerce_enclosed
\param p position in tree
\param q soid
**/

void coerce_enclosed (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (SUB (p), q);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    coerce_closed (SUB (p), q);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    coerce_collateral (SUB (p), q);
  } else if (WHETHER (p, PARALLEL_CLAUSE)) {
    coerce_collateral (SUB (NEXT_SUB (p)), q);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    coerce_conditional (SUB (p), q);
  } else if (WHETHER (p, INTEGER_CASE_CLAUSE)) {
    coerce_int_case (SUB (p), q);
  } else if (WHETHER (p, UNITED_CASE_CLAUSE)) {
    coerce_united_case (SUB (p), q);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    coerce_loop (SUB (p));
  }
  MOID (p) = depref_rows (MOID (p), MOID (q));
}

/*!
\brief get monad moid
\param p position in tree
**/

static MOID_T *get_monad_moid (NODE_T * p)
{
  if (TAX (p) != NULL && TAX (p) != error_tag) {
    MOID (p) = MOID (TAX (p));
    return (MOID (PACK (MOID (p))));
  } else {
    return (MODE (ERROR));
  }
}

/*!
\brief coerce monad oper
\param p position in tree
\param q soid
**/

static void coerce_monad_oper (NODE_T * p, SOID_T * q)
{
  if (p != NULL) {
    SOID_T z;
    make_soid (&z, FIRM, MOID (PACK (MOID (TAX (p)))), 0);
    INSERT_COERCIONS (NEXT (p), MOID (q), &z);
  }
}

/*!
\brief coerce monad formula
\param p position in tree
**/

static void coerce_monad_formula (NODE_T * p)
{
  SOID_T e;
  make_soid (&e, STRONG, get_monad_moid (p), 0);
  coerce_operand (NEXT (p), &e);
  coerce_monad_oper (p, &e);
}

/*!
\brief coerce operand
\param p position in tree
\param q soid
**/

static void coerce_operand (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, MONADIC_FORMULA)) {
    coerce_monad_formula (SUB (p));
    if (MOID (p) != MOID (q)) {
      make_sub (p, p, FORMULA);
      INSERT_COERCIONS (p, MOID (p), q);
      make_sub (p, p, TERTIARY);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, SECONDARY)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
  }
}

/*!
\brief coerce formula
\param p position in tree
\param q soid
**/

static void coerce_formula (NODE_T * p, SOID_T * q)
{
  (void) q;
  if (WHETHER (p, MONADIC_FORMULA) && NEXT (p) == NULL) {
    coerce_monad_formula (SUB (p));
  } else {
    if (TAX (NEXT (p)) != NULL && TAX (NEXT (p)) != error_tag) {
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

/*!
\brief coerce assignation
\param p position in tree
**/

static void coerce_assignation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, SOFT, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, SUB_MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

/*!
\brief coerce relation
\param p position in tree
**/

static void coerce_relation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, MOID (NEXT_NEXT (p)), 0);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

/*!
\brief coerce bool function
\param p position in tree
**/

static void coerce_bool_function (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MODE (BOOL), 0);
  coerce_unit (SUB (p), &w);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

/*!
\brief coerce assertion
\param p position in tree
**/

static void coerce_assertion (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (BOOL), 0);
  coerce_enclosed (SUB_NEXT (p), &w);
}

/*!
\brief coerce field-selection
\param p position in tree
**/

static void coerce_field_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK. */ STRONG, MOID (p), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce selection
\param p position in tree
**/

static void coerce_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK. */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce cast
\param p position in tree
**/

static void coerce_cast (NODE_T * p)
{
  SOID_T w;
  coerce_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_enclosed (NEXT (p), &w);
}

/*!
\brief coerce argument list
\param r pack
\param p position in tree
**/

static void coerce_argument_list (PACK_T ** r, NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ARGUMENT_LIST)) {
      coerce_argument_list (r, SUB (p));
    } else if (WHETHER (p, UNIT)) {
      SOID_T s;
      make_soid (&s, STRONG, MOID (*r), 0);
      coerce_unit (p, &s);
      FORWARD (*r);
    } else if (WHETHER (p, TRIMMER)) {
      FORWARD (*r);
    }
  }
}

/*!
\brief coerce call
\param p position in tree
**/

static void coerce_call (NODE_T * p)
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

/*!
\brief coerce meek int
\param p position in tree
**/

static void coerce_meek_int (NODE_T * p)
{
  SOID_T x;
  make_soid (&x, MEEK, MODE (INT), 0);
  coerce_unit (p, &x);
}

/*!
\brief coerce trimmer
\param p position in tree
**/

static void coerce_trimmer (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, UNIT)) {
      coerce_meek_int (p);
      coerce_trimmer (NEXT (p));
    } else {
      coerce_trimmer (NEXT (p));
    }
  }
}

/*!
\brief coerce indexer
\param p position in tree
**/

static void coerce_indexer (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, TRIMMER)) {
      coerce_trimmer (SUB (p));
    } else if (WHETHER (p, UNIT)) {
      coerce_meek_int (p);
    } else {
      coerce_indexer (SUB (p));
      coerce_indexer (NEXT (p));
    }
  }
}

/*!
\brief coerce_slice
\param p position in tree
**/

static void coerce_slice (NODE_T * p)
{
  SOID_T w;
  MOID_T *row;
  row = MOID (p);
  make_soid (&w, /* WEAK. */ STRONG, row, 0);
  coerce_unit (SUB (p), &w);
  coerce_indexer (SUB_NEXT (p));
}

/*!
\brief mode coerce diagonal
\param p position in tree
**/

static void coerce_diagonal (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, /* WEAK. */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief mode coerce transpose
\param p position in tree
**/

static void coerce_transpose (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK. */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief mode coerce row or column function
\param p position in tree
**/

static void coerce_row_column_function (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, /* WEAK. */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce format text
\param p position in tree
**/

static void coerce_format_text (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    coerce_format_text (SUB (p));
    if (WHETHER (p, FORMAT_PATTERN)) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (FORMAT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (ROW_INT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (INT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    }
  }
}

/*!
\brief coerce unit
\param p position in tree
\param q soid
**/

static void coerce_unit (NODE_T * p, SOID_T * q)
{
  if (p == NULL) {
    return;
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
/* Ex primary. */
  } else if (WHETHER (p, CALL)) {
    coerce_call (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, SLICE)) {
    coerce_slice (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, FIELD_SELECTION)) {
    coerce_field_selection (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, CAST)) {
    coerce_cast (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (whether_one_of (p, DENOTATION, IDENTIFIER, NULL_ATTRIBUTE)) {
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, FORMAT_TEXT)) {
    coerce_format_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (p, q);
/* Ex secondary. */
  } else if (WHETHER (p, SELECTION)) {
    coerce_selection (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, GENERATOR)) {
    coerce_declarer (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
/* Ex tertiary. */
  } else if (WHETHER (p, NIHIL)) {
    if (ATTRIBUTE (MOID (q)) != REF_SYMBOL && MOID (q) != MODE (VOID)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_NAME_REQUIRED);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, DIAGONAL_FUNCTION)) {
    coerce_diagonal (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, TRANSPOSE_FUNCTION)) {
    coerce_transpose (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ROW_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, COLUMN_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
/* Ex unit. */
  } else if (WHETHER (p, JUMP)) {
    if (MOID (q) == MODE (PROC_VOID)) {
      make_sub (p, p, PROCEDURING);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, SKIP)) {
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, ASSIGNATION)) {
    coerce_assignation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    coerce_relation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (whether_one_of (p, AND_FUNCTION, OR_FUNCTION, NULL_ATTRIBUTE)) {
    coerce_bool_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ASSERTION)) {
    coerce_assertion (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  }
}

/*!
\brief widen_denotation
\param p position in tree
**/

void widen_denotation (NODE_T * p)
{
#define WIDEN {\
  *q = *(SUB (q));\
  ATTRIBUTE (q) = DENOTATION;\
  MOID (q) = lm;\
  STATUS_SET (q, OPTIMAL_MASK);\
  }
#define WARN_WIDENING\
  if (program.options.portcheck && !(STATUS_TEST (SUB (q), OPTIMAL_MASK))) {\
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, q, WARNING_WIDENING_NOT_PORTABLE);\
  }
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    widen_denotation (SUB (q));
    if (WHETHER (q, WIDENING) && WHETHER (SUB (q), DENOTATION)) {
      MOID_T *lm = MOID (q), *m = MOID (SUB (q));
      if (lm == MODE (LONGLONG_INT) && m == MODE (LONG_INT)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_INT) && m == MODE (INT)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONGLONG_REAL) && m == MODE (LONG_REAL)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_REAL) && m == MODE (REAL)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_REAL) && m == MODE (LONG_INT)) {
        WIDEN;
      }
      if (lm == MODE (REAL) && m == MODE (INT)) {
        WIDEN;
      }
      if (lm == MODE (LONGLONG_BITS) && m == MODE (LONG_BITS)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_BITS) && m == MODE (BITS)) {
        WARN_WIDENING;
        WIDEN;
      }
      return;
    }
  }
#undef WIDEN
#undef WARN_WIDENING
}
