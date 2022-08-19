//! @file extract.c
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
#include "a68g-parser.h"

// This is part of the bottom-up parser.
//
// Here is a set of routines that gather definitions from phrases.
// This way we can apply tags before defining them.
// These routines do not look very elegant as they have to scan through all
// kind of symbols to find a pattern that they recognise.

//! @brief Insert alt equals symbol.

void insert_alt_equals (NODE_T * p)
{
  NODE_T *q = new_node ();
  *q = *p;
  INFO (q) = new_node_info ();
  *INFO (q) = *INFO (p);
  GINFO (q) = new_genie_info ();
  *GINFO (q) = *GINFO (p);
  ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
  NSYMBOL (q) = TEXT (add_token (&A68 (top_token), "="));
  NEXT (p) = q;
  PREVIOUS (q) = p;
  if (NEXT (q) != NO_NODE) {
    PREVIOUS (NEXT (q)) = q;
  }
}

//! @brief Detect redefined keyword.

void detect_redefined_keyword (NODE_T * p, int construct)
{
  if (p != NO_NODE && whether (p, KEYWORD, EQUALS_SYMBOL, STOP)) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_REDEFINED_KEYWORD, NSYMBOL (p), construct);
  }
}

//! @brief Skip anything until a comma, semicolon or EXIT is found.

NODE_T *skip_unit (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, COMMA_SYMBOL)) {
      return p;
    } else if (IS (p, SEMI_SYMBOL)) {
      return p;
    } else if (IS (p, EXIT_SYMBOL)) {
      return p;
    }
  }
  return NO_NODE;
}

//! @brief Attribute of entry in symbol table.

int find_tag_definition (TABLE_T * table, char *name)
{
  if (table != NO_TABLE) {
    int ret = 0;
    TAG_T *s;
    BOOL_T found;
    found = A68_FALSE;
    for (s = INDICANTS (table); s != NO_TAG && !found; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        ret += INDICANT;
        found = A68_TRUE;
      }
    }
    found = A68_FALSE;
    for (s = OPERATORS (table); s != NO_TAG && !found; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        ret += OPERATOR;
        found = A68_TRUE;
      }
    }
    if (ret == 0) {
      return find_tag_definition (PREVIOUS (table), name);
    } else {
      return ret;
    }
  } else {
    return 0;
  }
}

//! @brief Fill in whether bold tag is operator or indicant.

void elaborate_bold_tags (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, BOLD_TAG)) {
      switch (find_tag_definition (TABLE (q), NSYMBOL (q))) {
      case 0:
        {
          diagnostic (A68_SYNTAX_ERROR, q, ERROR_UNDECLARED_TAG);
          break;
        }
      case INDICANT:
        {
          ATTRIBUTE (q) = INDICANT;
          break;
        }
      case OPERATOR:
        {
          ATTRIBUTE (q) = OPERATOR;
          break;
        }
      }
    }
  }
}

//! @brief Skip declarer, or argument pack and declarer.

NODE_T *skip_pack_declarer (NODE_T * p)
{
// Skip () REF [] REF FLEX [] [] ...
  while (p != NO_NODE && (is_one_of (p, SUB_SYMBOL, OPEN_SYMBOL, REF_SYMBOL, FLEX_SYMBOL, SHORT_SYMBOL, LONG_SYMBOL, STOP))) {
    FORWARD (p);
  }
// Skip STRUCT (), UNION () or PROC [()].
  if (p != NO_NODE && (is_one_of (p, STRUCT_SYMBOL, UNION_SYMBOL, STOP))) {
    return NEXT (p);
  } else if (p != NO_NODE && IS (p, PROC_SYMBOL)) {
    return skip_pack_declarer (NEXT (p));
  } else {
    return p;
  }
}

//! @brief Search MODE A = .., B = .. and store indicants.

void extract_indicants (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (IS (q, MODE_SYMBOL)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        detect_redefined_keyword (q, MODE_DECLARATION);
        if (whether (q, BOLD_TAG, EQUALS_SYMBOL, STOP)) {
// Store in the symbol table, but also in the moid list.
// Position of definition (q) connects to this lexical level! 
          ASSERT (add_tag (TABLE (p), INDICANT, q, NO_MOID, STOP) != NO_TAG);
          ASSERT (add_mode (&TOP_MOID (&A68_JOB), INDICANT, 0, q, NO_MOID, NO_PACK) != NO_MOID);
          ATTRIBUTE (q) = DEFINING_INDICANT;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          q = skip_pack_declarer (NEXT (q));
          FORWARD (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

#define GET_PRIORITY(q, k)\
  errno=0;\
  (k) = atoi (NSYMBOL (q));\
  if (errno != 0) {\
    diagnostic (A68_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY);\
    (k) = MAX_PRIORITY;\
  } else if ((k) < 1 || (k) > MAX_PRIORITY) {\
    diagnostic (A68_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY);\
    (k) = MAX_PRIORITY;\
  }

//! @brief Search PRIO X = .., Y = .. and store priorities.

void extract_priorities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (IS (q, PRIO_SYMBOL)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        detect_redefined_keyword (q, PRIORITY_DECLARATION);
// An operator tag like ++ or && gives strange errors so we catch it here.
        if (whether (q, OPERATOR, OPERATOR, STOP)) {
          int k;
          NODE_T *y = q;
          diagnostic (A68_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG);
          ATTRIBUTE (q) = DEFINING_OPERATOR;
// Remove one superfluous operator, and hope it was only one.           .
          NEXT (q) = NEXT_NEXT (q);
          PREVIOUS (NEXT (q)) = q;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (TABLE (p), PRIO_SYMBOL, y, NO_MOID, k) != NO_TAG);
          FORWARD (q);
        } else if (whether (q, OPERATOR, EQUALS_SYMBOL, INT_DENOTATION, STOP) || whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, INT_DENOTATION, STOP)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (TABLE (p), PRIO_SYMBOL, y, NO_MOID, k) != NO_TAG);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, IDENTIFIER, STOP)) {
          siga = A68_FALSE;
        } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, INT_DENOTATION, STOP)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (TABLE (p), PRIO_SYMBOL, y, NO_MOID, k) != NO_TAG);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, INT_DENOTATION, STOP) || whether (q, OPERATOR, INT_DENOTATION, STOP) || whether (q, EQUALS_SYMBOL, INT_DENOTATION, STOP)) {
// The scanner cannot separate operator and "=" sign so we do this here.
          int len = (int) strlen (NSYMBOL (q));
          if (len > 1 && NSYMBOL (q)[len - 1] == '=') {
            int k;
            NODE_T *y = q;
            char *sym = (char *) get_temp_heap_space ((size_t) (len + 1));
            bufcpy (sym, NSYMBOL (q), len + 1);
            sym[len - 1] = NULL_CHAR;
            NSYMBOL (q) = TEXT (add_token (&A68 (top_token), sym));
            if (len > 2 && NSYMBOL (q)[len - 2] == ':' && NSYMBOL (q)[len - 3] != '=') {
              diagnostic (A68_SYNTAX_ERROR, q, ERROR_OPERATOR_INVALID_END);
            }
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            insert_alt_equals (q);
            q = NEXT_NEXT (q);
            GET_PRIORITY (q, k);
            ATTRIBUTE (q) = PRIORITY;
            ASSERT (add_tag (TABLE (p), PRIO_SYMBOL, y, NO_MOID, k) != NO_TAG);
            FORWARD (q);
          } else {
            siga = A68_FALSE;
          }
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

//! @brief Search OP [( .. ) ..] X = .., Y = .. and store operators.

void extract_operators (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (!IS (q, OP_SYMBOL)) {
      FORWARD (q);
    } else {
      BOOL_T siga = A68_TRUE;
// Skip operator plan.
      if (NEXT (q) != NO_NODE && IS (NEXT (q), OPEN_SYMBOL)) {
        q = skip_pack_declarer (NEXT (q));
      }
// Sample operators.
      if (q != NO_NODE) {
        do {
          FORWARD (q);
          detect_redefined_keyword (q, OPERATOR_DECLARATION);
// Unacceptable operator tags like ++ or && could give strange errors.
          if (whether (q, OPERATOR, OPERATOR, STOP)) {
            diagnostic (A68_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG);
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (TABLE (p), OP_SYMBOL, q, NO_MOID, STOP) != NO_TAG);
            NEXT (q) = NEXT_NEXT (q);   // Remove one superfluous operator, and hope it was only one
            PREVIOUS (NEXT (q)) = q;
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, OPERATOR, EQUALS_SYMBOL, STOP) || whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, STOP)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (TABLE (p), OP_SYMBOL, q, NO_MOID, STOP) != NO_TAG);
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, BOLD_TAG, IDENTIFIER, STOP)) {
            siga = A68_FALSE;
          } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, STOP)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (TABLE (p), OP_SYMBOL, q, NO_MOID, STOP) != NO_TAG);
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (q != NO_NODE && (is_one_of (q, OPERATOR, BOLD_TAG, EQUALS_SYMBOL, STOP))) {
// The scanner cannot separate operator and "=" sign so we do this here.
            int len = (int) strlen (NSYMBOL (q));
            if (len > 1 && NSYMBOL (q)[len - 1] == '=') {
              char *sym = (char *) get_temp_heap_space ((size_t) (len + 1));
              bufcpy (sym, NSYMBOL (q), len + 1);
              sym[len - 1] = NULL_CHAR;
              NSYMBOL (q) = TEXT (add_token (&A68 (top_token), sym));
              if (len > 2 && NSYMBOL (q)[len - 2] == ':' && NSYMBOL (q)[len - 3] != '=') {
                diagnostic (A68_SYNTAX_ERROR, q, ERROR_OPERATOR_INVALID_END);
              }
              ATTRIBUTE (q) = DEFINING_OPERATOR;
              insert_alt_equals (q);
              ASSERT (add_tag (TABLE (p), OP_SYMBOL, q, NO_MOID, STOP) != NO_TAG);
              FORWARD (q);
              q = skip_unit (q);
            } else {
              siga = A68_FALSE;
            }
          } else {
            siga = A68_FALSE;
          }
        } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
      }
    }
  }
}

//! @brief Search and store labels.

void extract_labels (NODE_T * p, int expect)
{
  NODE_T *q;
// Only handle candidate phrases as not to search indexers!.
  if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      if (whether (q, IDENTIFIER, COLON_SYMBOL, STOP)) {
        TAG_T *z = add_tag (TABLE (p), LABEL, q, NO_MOID, LOCAL_LABEL);
        ATTRIBUTE (q) = DEFINING_IDENTIFIER;
        UNIT (z) = NO_NODE;
      }
    }
  }
}

//! @brief Search MOID x = .., y = .. and store identifiers.

void extract_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (whether (q, DECLARER, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
      BOOL_T siga = A68_TRUE;
      do {
        if (whether ((FORWARD (q)), IDENTIFIER, EQUALS_SYMBOL, STOP)) {
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, STOP)) {
// Handle common error in ALGOL 68 programs.
          diagnostic (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

//! @brief Search MOID x [:= ..], y [:= ..] and store identifiers.

void extract_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (whether (q, DECLARER, IDENTIFIER, STOP)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, STOP)) {
          if (whether (q, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
// Handle common error in ALGOL 68 programs.
            diagnostic (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
            ATTRIBUTE (NEXT (q)) = ASSIGN_SYMBOL;
          }
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

//! @brief Search PROC x = .., y = .. and stores identifiers.

void extract_proc_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
          TAG_T *t = add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER);
          IN_PROC (t) = A68_TRUE;
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, STOP)) {
// Handle common error in ALGOL 68 programs.
          diagnostic (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

//! @brief Search PROC x [:= ..], y [:= ..]; store identifiers.

void extract_proc_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, STOP)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, STOP)) {
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (FORWARD (q));
        } else if (whether (q, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
// Handle common error in ALGOL 68 programs.
          diagnostic (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ASSIGN_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

//! @brief Schedule gathering of definitions in a phrase.

void extract_declarations (NODE_T * p)
{
  NODE_T *q;
// Get definitions so we know what is defined in this range.
  extract_identities (p);
  extract_variables (p);
  extract_proc_identities (p);
  extract_proc_variables (p);
// By now we know whether "=" is an operator or not.
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = OPERATOR;
    } else if (IS (q, ALT_EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = EQUALS_SYMBOL;
    }
  }
// Get qualifiers.
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (whether (q, LOC_SYMBOL, DECLARER, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, DECLARER, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, NEW_SYMBOL, DECLARER, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, LOC_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, NEW_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
  }
// Give priorities to operators.
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, OPERATOR)) {
      if (find_tag_global (TABLE (q), OP_SYMBOL, NSYMBOL (q))) {
        TAG_T *s = find_tag_global (TABLE (q), PRIO_SYMBOL, NSYMBOL (q));
        if (s != NO_TAG) {
          PRIO (INFO (q)) = PRIO (s);
        } else {
          PRIO (INFO (q)) = 0;
        }
      } else {
        diagnostic (A68_SYNTAX_ERROR, q, ERROR_UNDECLARED_TAG);
        PRIO (INFO (q)) = 1;
      }
    }
  }
}
