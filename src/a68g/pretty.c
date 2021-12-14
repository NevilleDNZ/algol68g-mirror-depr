//! @file pretty.c
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2021 J. Marcel van der Veer <algol68g@xs4all.nl>.
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

// Basic indenter for hopeless code.
// It applies one style only.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"
#include "a68g-optimiser.h"

#define ONE_LINER (A68_TRUE)
#define KEYWORD (A68_TRUE)
#define BLANK {put_str (" ");}

#define IS_OPEN_SYMBOL(p) (IS (p, OPEN_SYMBOL) || IS (p, SUB_SYMBOL) || IS (p, ACCO_SYMBOL))
#define IS_CLOSE_SYMBOL(p) (IS (p, CLOSE_SYMBOL) || IS (p, BUS_SYMBOL) || IS (p, OCCA_SYMBOL))
#define IS_IDENTIFIER(p) (IS (p, IDENTIFIER) || IS (p, DEFINING_IDENTIFIER) || IS (p, FIELD_IDENTIFIER))

static void indent_declarer (NODE_T *);
static void indent_serial (NODE_T *, BOOL_T, NODE_T **);
static void indent_statement (NODE_T *);
static void indent_format (NODE_T *);

//! @brief Write newline and indent.

static void put_nl (void)
{
  WRITE (A68_INDENT (fd), "\n");
  for (A68_INDENT (col) = 1; A68_INDENT (col) < A68_INDENT (ind); A68_INDENT (col)++) {
    WRITE (A68_INDENT (fd), " ");
  }
}

//! @brief Write a string.

static void put_str (char *txt)
{
  WRITE (A68_INDENT (fd), txt);
  A68_INDENT (col) += (int) strlen (txt);
}

//! @brief Write a character.

static void put_ch (char ch)
{
  char str[2];
  str[0] = ch;
  str[1] = NULL_CHAR;
  put_str (str);
}

//! @brief Write pragment string.

static void put_pragment (NODE_T * p)
{
  char *txt = NPRAGMENT (p);
  for (; txt != NO_TEXT && txt[0] != NULL_CHAR; txt++) {
    if (txt[0] == NEWLINE_CHAR) {
      put_nl ();
    } else {
      put_ch (txt[0]);
    }
  }
}

//! @brief Write pragment string.

static void pragment (NODE_T * p, BOOL_T keyw)
{
  if (NPRAGMENT (p) != NO_TEXT) {
    if (NPRAGMENT_TYPE (p) == BOLD_COMMENT_SYMBOL || NPRAGMENT_TYPE (p) == BOLD_PRAGMAT_SYMBOL) {
      if (!keyw) {
        put_nl ();
      }
      put_pragment (p);
      put_nl ();
      put_nl ();
    } else {
      if (!keyw && (int) strlen (NPRAGMENT (p)) < 20) {
        if (A68_INDENT (col) > A68_INDENT (ind)) {
          BLANK;
        }
        put_pragment (p);
        BLANK;
      } else {
        if (A68_INDENT (col) > A68_INDENT (ind)) {
          put_nl ();
        }
        put_pragment (p);
        put_nl ();
      }
    }
  }
}

//! @brief Write with typographic display features.

static void put_sym (NODE_T * p, BOOL_T keyw)
{
  char *txt = NSYMBOL (p);
  char *sym = NCHAR_IN_LINE (p);
  int n = 0, size = (int) strlen (txt);
  pragment (p, keyw);
  if (txt[0] != sym[0] || (int) strlen (sym) - 1 <= size) {
// Without features..
    put_str (txt);
  } else {
// With features. Preserves spaces in identifiers etcetera..
    while (n < size) {
      put_ch (sym[0]);
      if (TO_LOWER (txt[0]) == TO_LOWER (sym[0])) {
        txt++;
        n++;
      }
      sym++;
    }
  }
}

//! @brief Count units and separators in a sub-tree.

static void count (NODE_T * p, int *units, int *seps)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      (*units)++;
      count (SUB (p), units, seps);
    } else if (IS (p, SEMI_SYMBOL)) {
      (*seps)++;
    } else if (IS (p, COMMA_SYMBOL)) {
      (*seps)++;
    } else if (IS (p, CLOSED_CLAUSE)) {
      (*units)--;
    } else if (IS (p, COLLATERAL_CLAUSE)) {
      (*units)--;
      count (SUB (p), units, seps);
    } else {
      count (SUB (p), units, seps);
    }
  }
}

//! @brief Count units and separators in a sub-tree.

static void count_stowed (NODE_T * p, int *units, int *seps)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      MOID_T *v = MOID (p);
      BOOL_T stowed = (BOOL_T) (IS_FLEX (v) || IS_ROW (v) || IS_STRUCT (v));
      if (stowed) {
        (*units)++;
      }
    } else if (IS (p, SEMI_SYMBOL)) {
      (*seps)++;
    } else if (IS (p, COMMA_SYMBOL)) {
      (*seps)++;
    } else {
      count_stowed (SUB (p), units, seps);
    }
  }
}

//! @brief Count enclosed_clauses in a sub-tree.

static void count_enclos (NODE_T * p, int *enclos, int *seps)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ENCLOSED_CLAUSE)) {
      (*enclos)++;
    } else if (IS (p, SEMI_SYMBOL)) {
      (*seps)++;
    } else if (IS (p, COMMA_SYMBOL)) {
      (*seps)++;
    } else {
      count_enclos (SUB (p), enclos, seps);
    }
  }
}

//! @brief Indent sizety.

static void indent_sizety (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, LONGETY) || IS (p, SHORTETY)) {
      indent_sizety (SUB (p));
    } else if (IS (p, LONG_SYMBOL) || IS (p, SHORT_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    }
  }
}

//! @brief Indent generic list.

static void indent_generic_list (NODE_T * p, NODE_T ** what, BOOL_T one_liner)
{
  for (; p != NULL; FORWARD (p)) {
    if (IS_OPEN_SYMBOL (p)) {
      put_sym (p, KEYWORD);
      A68_INDENT (ind) = A68_INDENT (col);
    } else if (IS_CLOSE_SYMBOL (p)) {
      put_sym (p, KEYWORD);
    } else if (IS (p, BEGIN_SYMBOL)) {
      put_sym (p, KEYWORD);
      BLANK;
    } else if (IS (p, END_SYMBOL)) {
      BLANK;
      put_sym (p, KEYWORD);
    } else if (IS (p, AT_SYMBOL)) {
      if (NSYMBOL (p)[0] == '@') {
        put_sym (p, !KEYWORD);
      } else {
        BLANK;
        put_sym (p, !KEYWORD);
        BLANK;
      }
    } else if (IS (p, COLON_SYMBOL)) {
      BLANK;
      put_sym (p, !KEYWORD);
      BLANK;
    } else if (IS (p, DOTDOT_SYMBOL)) {
      BLANK;
      put_sym (p, !KEYWORD);
      BLANK;
    } else if (IS (p, UNIT)) {
      *what = p;
      indent_statement (SUB (p));
    } else if (IS (p, SPECIFIER)) {
      NODE_T *q = SUB (p);
      put_sym (q, KEYWORD);
      FORWARD (q);
      indent_declarer (q);
      FORWARD (q);
      if (IS_IDENTIFIER (q)) {
        BLANK;
        put_sym (q, !KEYWORD);
        FORWARD (q);
      }
      put_sym (q, !KEYWORD);
      FORWARD (q);
      put_sym (NEXT (p), !KEYWORD);     // : 
      BLANK;
      FORWARD (p);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      if (one_liner) {
        BLANK;
      } else {
        put_nl ();
      }
    } else {
      indent_generic_list (SUB (p), what, one_liner);
    }
  }
}

//! @brief Indent declarer pack.

static void indent_pack (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS_OPEN_SYMBOL (p) || IS_CLOSE_SYMBOL (p)) {
      put_sym (p, KEYWORD);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    } else if (IS (p, VOID_SYMBOL)) {
      put_sym (p, !KEYWORD);
    } else if (IS (p, DECLARER)) {
      indent_declarer (p);
      if (NEXT (p) != NO_NODE && IS_IDENTIFIER (NEXT (p))) {
        BLANK;
      }
    } else if (IS_IDENTIFIER (p)) {
      put_sym (p, !KEYWORD);
    } else {
      indent_pack (SUB (p));
    }
  }
}

//! @brief Indent declarer.

static void indent_declarer (NODE_T * p)
{
  if (IS (p, DECLARER)) {
    indent_declarer (SUB (p));
  } else if (IS (p, LONGETY) || IS (p, SHORTETY)) {
    indent_sizety (SUB (p));
    indent_declarer (NEXT (p));
  } else if (IS (p, VOID_SYMBOL)) {
    put_sym (p, !KEYWORD);
  } else if (IS (p, REF_SYMBOL)) {
    put_sym (p, !KEYWORD);
    BLANK;
    indent_declarer (NEXT (p));
  } else if (IS_FLEX (p)) {
    put_sym (p, !KEYWORD);
    BLANK;
    indent_declarer (NEXT (p));
  } else if (IS (p, BOUNDS) || IS (p, FORMAL_BOUNDS)) {
    NODE_T *what = NO_NODE;
    int pop_ind = A68_INDENT (ind);
    indent_generic_list (SUB (p), &what, ONE_LINER);
    A68_INDENT (ind) = pop_ind;
    BLANK;
    indent_declarer (NEXT (p));
  } else if (IS_STRUCT (p) || IS_UNION (p)) {
    NODE_T *pack = NEXT (p);
    put_sym (p, !KEYWORD);
    BLANK;
    indent_pack (pack);
  } else if (IS (p, PROC_SYMBOL)) {
    NODE_T *q = NEXT (p);
    put_sym (p, KEYWORD);
    BLANK;
    if (IS (q, FORMAL_DECLARERS)) {
      indent_pack (SUB (q));
      BLANK;
      FORWARD (q);
    }
    indent_declarer (q);
    return;
  } else if (IS (p, OP_SYMBOL)) {       // Operator plan
    NODE_T *q = NEXT (p);
    put_sym (p, KEYWORD);
    BLANK;
    if (IS (q, FORMAL_DECLARERS)) {
      indent_pack (SUB (q));
      BLANK;
      FORWARD (q);
    }
    indent_declarer (q);
    return;
  } else if (IS (p, INDICANT)) {
    put_sym (p, !KEYWORD);
  }
}

//! @brief Indent conditional.

static void indent_conditional (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, IF_PART) || IS (p, ELIF_IF_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_serial (NEXT_SUB (p), !ONE_LINER, &what);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, THEN_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_serial (NEXT_SUB (p), !ONE_LINER, &what);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, ELSE_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_serial (NEXT_SUB (p), !ONE_LINER, &what);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, ELIF_PART)) {
      indent_conditional (SUB (p));
    } else if (IS (p, FI_SYMBOL)) {
      put_sym (p, KEYWORD);
    } else if (IS (p, OPEN_PART)) {
      NODE_T *what = NO_NODE;
      put_sym (SUB (p), KEYWORD);
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, ELSE_OPEN_PART)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, CHOICE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, BRIEF_ELIF_PART)) {
      indent_conditional (SUB (p));
    } else if (IS_CLOSE_SYMBOL (p)) {
      put_sym (p, KEYWORD);
    }
  }
}

//! @brief Indent integer case clause.

static void indent_case (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, CASE_PART) || IS (p, OUSE_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_serial (NEXT_SUB (p), !ONE_LINER, &what);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, CASE_IN_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_generic_list (NEXT_SUB (p), &what, ONE_LINER);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, OUT_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_serial (NEXT_SUB (p), !ONE_LINER, &what);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, CASE_OUSE_PART)) {
      indent_case (SUB (p));
    } else if (IS (p, ESAC_SYMBOL)) {
      put_sym (p, KEYWORD);
    } else if (IS (p, OPEN_PART)) {
      NODE_T *what = NO_NODE;
      put_sym (SUB (p), KEYWORD);
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, ELSE_OPEN_PART)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, CASE_CHOICE_CLAUSE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_generic_list (NEXT_SUB (p), &what, ONE_LINER);
    } else if (IS (p, CHOICE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, BRIEF_OUSE_PART)) {
      indent_case (SUB (p));
    } else if (IS_CLOSE_SYMBOL (p)) {
      put_sym (p, KEYWORD);
    }
  }
}

//! @brief Indent conformity clause.

static void indent_conformity (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, CASE_PART) || IS (p, OUSE_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_serial (NEXT_SUB (p), !ONE_LINER, &what);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, CONFORMITY_IN_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_generic_list (NEXT_SUB (p), &what, ONE_LINER);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, OUT_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = A68_INDENT (col);
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_serial (NEXT_SUB (p), !ONE_LINER, &what);
      A68_INDENT (ind) = pop_ind;
      put_nl ();
    } else if (IS (p, CONFORMITY_OUSE_PART)) {
      indent_conformity (SUB (p));
    } else if (IS (p, ESAC_SYMBOL)) {
      put_sym (p, KEYWORD);
    } else if (IS (p, OPEN_PART)) {
      NODE_T *what = NO_NODE;
      put_sym (SUB (p), KEYWORD);
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, ELSE_OPEN_PART)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, CONFORMITY_CHOICE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_generic_list (NEXT_SUB (p), &what, ONE_LINER);
    } else if (IS (p, CHOICE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, BRIEF_CONFORMITY_OUSE_PART)) {
      indent_conformity (SUB (p));
    } else if (IS_CLOSE_SYMBOL (p)) {
      put_sym (p, KEYWORD);
    }
  }
}

//! @brief Indent loop.

static void indent_loop (NODE_T * p)
{
  int parts = 0, pop_ind = A68_INDENT (col);
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FOR_PART)) {
      put_sym (SUB (p), KEYWORD);
      BLANK;
      put_sym (NEXT_SUB (p), !KEYWORD);
      BLANK;
      parts++;
    } else if (is_one_of (p, FROM_PART, BY_PART, TO_PART, STOP)) {
      put_sym (SUB (p), KEYWORD);
      BLANK;
      indent_statement (NEXT_SUB (p));
      BLANK;
      parts++;
    } else if (IS (p, WHILE_PART)) {
      NODE_T *what = NO_NODE;
      A68_INDENT (ind) = pop_ind;
      if (parts > 0) {
        put_nl ();
      }
      put_sym (SUB (p), KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      indent_serial (NEXT_SUB (p), !ONE_LINER, &what);
      A68_INDENT (ind) = pop_ind;
      parts++;
    } else if (is_one_of (p, DO_PART, ALT_DO_PART, STOP)) {
      NODE_T *q = SUB (p);
      NODE_T *what = NO_NODE;
      A68_INDENT (ind) = pop_ind;
      if (parts > 0) {
        put_nl ();
      }
      put_sym (q, KEYWORD);     // DO
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
      FORWARD (q);
      parts = 0;
      if (IS (q, SERIAL_CLAUSE)) {
        indent_serial (SUB (q), !ONE_LINER, &what);
        FORWARD (q);
        parts++;
      }
      if (IS (q, UNTIL_PART)) {
        int pop_ind2 = A68_INDENT (ind);
        if (parts > 0) {
          put_nl ();
        }
        put_sym (SUB (q), KEYWORD);
        BLANK;
        A68_INDENT (ind) = A68_INDENT (col);
        indent_serial (NEXT_SUB (q), !ONE_LINER, &what);
        A68_INDENT (ind) = pop_ind2;
        FORWARD (q);
      }
      A68_INDENT (ind) = pop_ind;
      put_nl ();
      put_sym (q, KEYWORD);     // OD
      parts++;
    }
  }
}

//! @brief Indent closed clause.

static void indent_closed (NODE_T * p)
{
  int units = 0, seps = 0;
  count (SUB_NEXT (p), &units, &seps);
  if (units <= 3 && seps == (units - 1)) {
    put_sym (p, KEYWORD);
    if (IS (p, BEGIN_SYMBOL)) {
      NODE_T *what = NO_NODE;
      BLANK;
      indent_serial (SUB_NEXT (p), ONE_LINER, &what);
      BLANK;
    } else {
      NODE_T *what = NO_NODE;
      indent_serial (SUB_NEXT (p), ONE_LINER, &what);
    }
    put_sym (NEXT_NEXT (p), KEYWORD);
  } else if (units <= 3 && seps == (units - 1) && IS_OPEN_SYMBOL (p)) {
    NODE_T *what = NO_NODE;
    put_sym (p, KEYWORD);
    indent_serial (SUB_NEXT (p), ONE_LINER, &what);
    put_sym (NEXT_NEXT (p), KEYWORD);
  } else {
    NODE_T *what = NO_NODE;
    int pop_ind = A68_INDENT (col);
    put_sym (p, KEYWORD);
    if (IS (p, BEGIN_SYMBOL)) {
      BLANK;
    }
    A68_INDENT (ind) = A68_INDENT (col);
    indent_serial (SUB_NEXT (p), !ONE_LINER, &what);
    A68_INDENT (ind) = pop_ind;
    if (IS (NEXT_NEXT (p), END_SYMBOL)) {
      put_nl ();
    }
    put_sym (NEXT_NEXT (p), KEYWORD);
  }
}

//! @brief Indent collateral clause.

static void indent_collateral (NODE_T * p)
{
  int units = 0, seps = 0;
  NODE_T *what = NO_NODE;
  int pop_ind = A68_INDENT (col);
  count_stowed (p, &units, &seps);
  if (units <= 3) {
    indent_generic_list (p, &what, ONE_LINER);
  } else {
    indent_generic_list (p, &what, !ONE_LINER);
  }
  A68_INDENT (ind) = pop_ind;
}

//! @brief Indent enclosed clause.

static void indent_enclosed (NODE_T * p)
{
  if (IS (p, ENCLOSED_CLAUSE)) {
    indent_enclosed (SUB (p));
  } else if (IS (p, CLOSED_CLAUSE)) {
    indent_closed (SUB (p));
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    indent_collateral (SUB (p));
  } else if (IS (p, PARALLEL_CLAUSE)) {
    put_sym (SUB (p), KEYWORD);
    indent_enclosed (NEXT_SUB (p));
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    indent_conditional (SUB (p));
  } else if (IS (p, CASE_CLAUSE)) {
    indent_case (SUB (p));
  } else if (IS (p, CONFORMITY_CLAUSE)) {
    indent_conformity (SUB (p));
  } else if (IS (p, LOOP_CLAUSE)) {
    indent_loop (SUB (p));
  }
}

//! @brief Indent a literal.

static void indent_literal (char *txt)
{
  put_str ("\"");
  while (txt[0] != NULL_CHAR) {
    if (txt[0] == '\"') {
      put_str ("\"\"");
    } else {
      put_ch (txt[0]);
    }
    txt++;
  }
  put_str ("\"");
}

//! @brief Indent denotation.

static void indent_denotation (NODE_T * p)
{
  if (IS (p, ROW_CHAR_DENOTATION)) {
    indent_literal (NSYMBOL (p));
  } else if (IS (p, LONGETY) || IS (p, SHORTETY)) {
    indent_sizety (SUB (p));
    indent_denotation (NEXT (p));
  } else {
    put_sym (p, !KEYWORD);
  }
}

//! @brief Indent label.

static void indent_label (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NULL) {
      indent_label (SUB (p));
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      put_sym (p, !KEYWORD);
      put_sym (NEXT (p), KEYWORD);
    }
  }
}

//! @brief Indent literal list.

static void indent_collection (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FORMAT_OPEN_SYMBOL) || IS (p, FORMAT_CLOSE_SYMBOL)) {
      put_sym (p, !KEYWORD);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    } else {
      indent_format (SUB (p));
    }
  }
}

//! @brief Indent format text.

static void indent_format (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FORMAT_DELIMITER_SYMBOL)) {
      put_sym (p, !KEYWORD);
    } else if (IS (p, COLLECTION)) {
      indent_collection (SUB (p));
    } else if (IS (p, ENCLOSED_CLAUSE)) {
      indent_enclosed (SUB (p));
    } else if (IS (p, LITERAL)) {
      indent_literal (NSYMBOL (p));
    } else if (IS (p, STATIC_REPLICATOR)) {
      indent_denotation (p);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    } else {
      if (SUB (p) != NO_NODE) {
        indent_format (SUB (p));
      } else {
        switch (ATTRIBUTE (p)) {
        case FORMAT_ITEM_A:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_B:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_C:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_D:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_E:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_ESCAPE:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_F:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_G:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_H:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_I:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_J:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_K:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_L:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_M:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_MINUS:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_N:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_O:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_P:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_PLUS:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_POINT:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_Q:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_R:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_S:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_T:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_U:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_V:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_W:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_X:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_Y:
          put_sym (p, !KEYWORD);
          break;
        case FORMAT_ITEM_Z:
          put_sym (p, !KEYWORD);
          break;
        }
      }
    }
  }
}

//! @brief Constant folder - replace constant statement with value.

static BOOL_T indent_folder (NODE_T * p)
{
  if (MOID (p) == M_INT) {
    A68_INT k;
    A68_SP = 0;
    push_unit (p);
    POP_OBJECT (p, &k, A68_INT);
    if (ERROR_COUNT (&(A68 (job))) == 0) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, A68_LD, VALUE (&k)) >= 0);
      put_str (A68 (output_line));
      return A68_TRUE;
    } else {
      return A68_FALSE;
    }
  } else if (MOID (p) == M_REAL) {
    A68_REAL x;
    REAL_T conv;
    A68_SP = 0;
    push_unit (p);
    POP_OBJECT (p, &x, A68_REAL);
// Mind overflowing or underflowing values.
    if (ERROR_COUNT (&(A68 (job))) != 0) {
      return A68_FALSE;
    } else if (VALUE (&x) == REAL_MAX) {
      return A68_FALSE;
    } else if (VALUE (&x) == -REAL_MAX) {
      return A68_FALSE;
    } else {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%.*g", REAL_WIDTH, VALUE (&x)) >= 0);
      errno = 0;
      conv = strtod (A68 (output_line), NO_VAR);
      if (errno == ERANGE && conv == 0.0) {
        put_str ("0.0");
        return A68_TRUE;
      } else if (errno == ERANGE) {
        return A68_FALSE;
      } else {
        if (strchr (A68 (output_line), '.') == NO_TEXT && strchr (A68 (output_line), 'e') == NO_TEXT && strchr (A68 (output_line), 'E') == NO_TEXT) {
          strncat (A68 (output_line), ".0", BUFFER_SIZE - 1);
        }
        put_str (A68 (output_line));
        return A68_TRUE;
      }
    }
  } else if (MOID (p) == M_BOOL) {
    A68_BOOL b;
    A68_SP = 0;
    push_unit (p);
    POP_OBJECT (p, &b, A68_BOOL);
    if (ERROR_COUNT (&(A68 (job))) != 0) {
      return A68_FALSE;
    } else {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s", (VALUE (&b) ? "TRUE" : "FALSE")) >= 0);
      put_str (A68 (output_line));
      return A68_TRUE;
    }
  } else if (MOID (p) == M_CHAR) {
    A68_CHAR c;
    A68_SP = 0;
    push_unit (p);
    POP_OBJECT (p, &c, A68_CHAR);
    if (ERROR_COUNT (&(A68 (job))) == 0) {
      return A68_FALSE;
    } else if (VALUE (&c) == '\"') {
      put_str ("\"\"\"\"");
      return A68_TRUE;
    } else {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\"%c\"", (int) VALUE (&c)) >= 0);
      return A68_TRUE;
    }
  }
  return A68_FALSE;
}

//! @brief Indent statement.

static void indent_statement (NODE_T * p)
{
  if (IS (p, LABEL)) {
    int enclos = 0, seps = 0;
    indent_label (SUB (p));
    FORWARD (p);
    count_enclos (SUB (p), &enclos, &seps);
    if (enclos == 0) {
      BLANK;
    } else {
      put_nl ();
    }
  }
  if (A68_INDENT (use_folder) && folder_mode (MOID (p)) && constant_unit (p)) {
    if (indent_folder (p)) {
      return;
    };
  }
  if (is_coercion (p)) {
    indent_statement (SUB (p));
  } else if (is_one_of (p, PRIMARY, SECONDARY, TERTIARY, UNIT, LABELED_UNIT, STOP)) {
    indent_statement (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    indent_enclosed (SUB (p));
  } else if (IS (p, DENOTATION)) {
    indent_denotation (SUB (p));
  } else if (IS (p, FORMAT_TEXT)) {
    indent_format (SUB (p));
  } else if (IS (p, IDENTIFIER)) {
    put_sym (p, !KEYWORD);
  } else if (IS (p, CAST)) {
    NODE_T *decl = SUB (p);
    NODE_T *rhs = NEXT (decl);
    indent_declarer (decl);
    BLANK;
    indent_enclosed (rhs);
  } else if (IS (p, CALL)) {
    NODE_T *primary = SUB (p);
    NODE_T *arguments = NEXT (primary);
    NODE_T *what = NO_NODE;
    int pop_ind = A68_INDENT (col);
    indent_statement (primary);
    BLANK;
    indent_generic_list (arguments, &what, ONE_LINER);
    A68_INDENT (ind) = pop_ind;
  } else if (IS (p, SLICE)) {
    NODE_T *primary = SUB (p);
    NODE_T *indexer = NEXT (primary);
    NODE_T *what = NO_NODE;
    int pop_ind = A68_INDENT (col);
    indent_statement (primary);
    indent_generic_list (indexer, &what, ONE_LINER);
    A68_INDENT (ind) = pop_ind;
  } else if (IS (p, SELECTION)) {
    NODE_T *selector = SUB (p);
    NODE_T *secondary = NEXT (selector);
    indent_statement (selector);
    indent_statement (secondary);
  } else if (IS (p, SELECTOR)) {
    NODE_T *identifier = SUB (p);
    put_sym (identifier, !KEYWORD);
    BLANK;
    put_sym (NEXT (identifier), !KEYWORD);      // OF
    BLANK;
  } else if (IS (p, GENERATOR)) {
    NODE_T *q = SUB (p);
    put_sym (q, !KEYWORD);
    BLANK;
    indent_declarer (NEXT (q));
  } else if (IS (p, FORMULA)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    indent_statement (lhs);
    if (op != NO_NODE) {
      NODE_T *rhs = NEXT (op);
      BLANK;
      put_sym (op, !KEYWORD);
      BLANK;
      indent_statement (rhs);
    }
  } else if (IS (p, MONADIC_FORMULA)) {
    NODE_T *op = SUB (p);
    NODE_T *rhs = NEXT (op);
    put_sym (op, !KEYWORD);
    if (strchr (MONADS, (NSYMBOL (op))[0]) == NO_TEXT) {
      BLANK;
    }
    indent_statement (rhs);
  } else if (IS (p, NIHIL)) {
    put_sym (p, !KEYWORD);
  } else if (IS (p, AND_FUNCTION) || IS (p, OR_FUNCTION)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    NODE_T *rhs = NEXT (op);
    indent_statement (lhs);
    BLANK;
    put_sym (op, !KEYWORD);
    BLANK;
    indent_statement (rhs);
  } else if (IS (p, TRANSPOSE_FUNCTION) || IS (p, DIAGONAL_FUNCTION) || IS (p, ROW_FUNCTION) || IS (p, COLUMN_FUNCTION)) {
    NODE_T *q = SUB (p);
    if (IS (p, TERTIARY)) {
      indent_statement (q);
      BLANK;
      FORWARD (q);
    }
    put_sym (q, !KEYWORD);
    BLANK;
    indent_statement (NEXT (q));
  } else if (IS (p, ASSIGNATION)) {
    NODE_T *dst = SUB (p);
    NODE_T *bec = NEXT (dst);
    NODE_T *src = NEXT (bec);
    indent_statement (dst);
    BLANK;
    put_sym (bec, !KEYWORD);
    BLANK;
    indent_statement (src);
  } else if (IS (p, ROUTINE_TEXT)) {
    NODE_T *q = SUB (p);
    int units, seps;
    if (IS (q, PARAMETER_PACK)) {
      indent_pack (SUB (q));
      BLANK;
      FORWARD (q);
    }
    indent_declarer (q);
    FORWARD (q);
    put_sym (q, !KEYWORD);      // :
    FORWARD (q);
    units = 0;
    seps = 0;
    count (q, &units, &seps);
    if (units <= 1) {
      BLANK;
      indent_statement (q);
    } else {
      put_nl ();
      indent_statement (q);
    }
  } else if (IS (p, IDENTITY_RELATION)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    NODE_T *rhs = NEXT (op);
    indent_statement (lhs);
    BLANK;
    put_sym (op, !KEYWORD);
    BLANK;
    indent_statement (rhs);
  } else if (IS (p, JUMP)) {
    NODE_T *q = SUB (p);
    if (IS (q, GOTO_SYMBOL)) {
      put_sym (q, !KEYWORD);
      BLANK;
      FORWARD (q);
    }
    put_sym (q, !KEYWORD);
  } else if (IS (p, SKIP)) {
    put_sym (p, !KEYWORD);
  } else if (IS (p, ASSERTION)) {
    NODE_T *q = SUB (p);
    put_sym (q, KEYWORD);
    BLANK;
    indent_enclosed (NEXT (q));
  } else if (IS (p, CODE_CLAUSE)) {
    NODE_T *q = SUB (p);
    put_sym (q, KEYWORD);
    BLANK;
    FORWARD (q);
    indent_collection (SUB (q));
    FORWARD (q);
    put_sym (q, KEYWORD);
  }
}

//! @brief Indent identifier declarations.

static void indent_iddecl (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, IDENTITY_DECLARATION) || IS (p, VARIABLE_DECLARATION)) {
      indent_iddecl (SUB (p));
    } else if (IS (p, QUALIFIER)) {
      put_sym (SUB (p), !KEYWORD);
      BLANK;
    } else if (IS (p, DECLARER)) {
      indent_declarer (SUB (p));
      BLANK;
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      NODE_T *q = p;
      int pop_ind = A68_INDENT (ind);
      put_sym (q, !KEYWORD);
      FORWARD (q);
      if (q != NO_NODE) {       // := unit
        BLANK;
        put_sym (q, !KEYWORD);
        BLANK;
        FORWARD (q);
        indent_statement (q);
      }
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    }
  }
}

//! @brief Indent procedure declarations.

static void indent_procdecl (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PROCEDURE_DECLARATION) || IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      indent_procdecl (SUB (p));
    } else if (IS (p, PROC_SYMBOL)) {
      put_sym (p, KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      NODE_T *q = p;
      int pop_ind = A68_INDENT (ind);
      put_sym (q, !KEYWORD);
      FORWARD (q);
      BLANK;
      put_sym (q, !KEYWORD);
      BLANK;
      FORWARD (q);
      indent_statement (q);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      put_nl ();
    }
  }
}

//! @brief Indent operator declarations.

static void indent_opdecl (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, OPERATOR_DECLARATION) || IS (p, BRIEF_OPERATOR_DECLARATION)) {
      indent_opdecl (SUB (p));
    } else if (IS (p, OP_SYMBOL)) {
      put_sym (p, KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
    } else if (IS (p, OPERATOR_PLAN)) {
      indent_declarer (SUB (p));
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
    } else if (IS (p, DEFINING_OPERATOR)) {
      NODE_T *q = p;
      int pop_ind = A68_INDENT (ind);
      put_sym (q, !KEYWORD);
      FORWARD (q);
      BLANK;
      put_sym (q, !KEYWORD);
      BLANK;
      FORWARD (q);
      indent_statement (q);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      put_nl ();
    }
  }
}

//! @brief Indent priority declarations.

static void indent_priodecl (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PRIORITY_DECLARATION)) {
      indent_priodecl (SUB (p));
    } else if (IS (p, PRIO_SYMBOL)) {
      put_sym (p, KEYWORD);
      BLANK;
    } else if (IS (p, DEFINING_OPERATOR)) {
      NODE_T *q = p;
      put_sym (q, !KEYWORD);
      FORWARD (q);
      BLANK;
      put_sym (q, !KEYWORD);
      BLANK;
      FORWARD (q);
      put_sym (q, !KEYWORD);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    }
  }
}

//! @brief Indent mode declarations.

static void indent_modedecl (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, MODE_DECLARATION)) {
      indent_modedecl (SUB (p));
    } else if (IS (p, MODE_SYMBOL)) {
      put_sym (p, KEYWORD);
      BLANK;
      A68_INDENT (ind) = A68_INDENT (col);
    } else if (IS (p, DEFINING_INDICANT)) {
      NODE_T *q = p;
      int pop_ind = A68_INDENT (ind);
      put_sym (q, !KEYWORD);
      FORWARD (q);
      BLANK;
      put_sym (q, !KEYWORD);
      BLANK;
      FORWARD (q);
      indent_declarer (q);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      put_nl ();
    }
  }
}

//! @brief Indent declaration list.

static void indent_declist (NODE_T * p, BOOL_T one_liner)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, IDENTITY_DECLARATION) || IS (p, VARIABLE_DECLARATION)) {
      int pop_ind = A68_INDENT (ind);
      indent_iddecl (p);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, PROCEDURE_DECLARATION) || IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      int pop_ind = A68_INDENT (ind);
      indent_procdecl (p);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, OPERATOR_DECLARATION) || IS (p, BRIEF_OPERATOR_DECLARATION)) {
      int pop_ind = A68_INDENT (ind);
      indent_opdecl (p);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, PRIORITY_DECLARATION)) {
      int pop_ind = A68_INDENT (ind);
      indent_priodecl (p);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, MODE_DECLARATION)) {
      int pop_ind = A68_INDENT (ind);
      indent_modedecl (p);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      if (one_liner) {
        BLANK;
      } else {
        put_nl ();
      }
    } else {
      indent_declist (SUB (p), one_liner);
    }
  }
}

//! @brief Indent serial clause.

static void indent_serial (NODE_T * p, BOOL_T one_liner, NODE_T ** what)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT) || IS (p, LABELED_UNIT)) {
      int pop_ind = A68_INDENT (col);
      (*what) = p;
      indent_statement (p);
      A68_INDENT (ind) = pop_ind;
    } else if (IS (p, DECLARATION_LIST)) {
      (*what) = p;
      indent_declist (p, one_liner);
    } else if (IS (p, SEMI_SYMBOL)) {
      put_sym (p, !KEYWORD);
      if (!one_liner) {
        put_nl ();
        if ((*what) != NO_NODE && IS ((*what), DECLARATION_LIST)) {
//        put_nl ();
        }
      } else {
        BLANK;
      }
    } else if (IS (p, EXIT_SYMBOL)) {
      if (NPRAGMENT (p) == NO_TEXT) {
        BLANK;
      }
      put_sym (p, !KEYWORD);
      if (!one_liner) {
        put_nl ();
      } else {
        BLANK;
      }
    } else {
      indent_serial (SUB (p), one_liner, what);
    }
  }
}

//! @brief Do not pretty-print the environ.

static void skip_environ (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (LINE_NUMBER (p) == 0) {
      pragment (p, !KEYWORD);
      skip_environ (SUB (p));
    } else {
      NODE_T *what = NO_NODE;
      indent_serial (p, !ONE_LINER, &what);
    }
  }
}

//! @brief Indenter driver.

void indenter (MODULE_T * q)
{
  A68_INDENT (ind) = 1;
  A68_INDENT (col) = 1;
  A68_INDENT (indentation) = OPTION_INDENT (q);
  A68_INDENT (use_folder) = OPTION_FOLD (q);
  FILE_PRETTY_FD (q) = open (FILE_PRETTY_NAME (q), O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (FILE_PRETTY_FD (q) == -1, ERROR_ACTION, __func__);
  FILE_PRETTY_OPENED (q) = A68_TRUE;
  A68_INDENT (fd) = FILE_PRETTY_FD (q);
  skip_environ (TOP_NODE (q));
  ASSERT (close (A68_INDENT (fd)) == 0);
  FILE_PRETTY_OPENED (q) = A68_FALSE;
  return;
}
