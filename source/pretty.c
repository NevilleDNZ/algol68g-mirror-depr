/**
@file pretty.c
@author J. Marcel van der Veer.
@brief Pretty-printer for Algol 68 programs.

@section Copyright

This file is part of Algol68G - an Algol 68 compiler-interpreter.
Copyright 2001-2016 J. Marcel van der Veer <algol68g@xs4all.nl>.

@section License

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.

@section Description

Basic indenter for hopeless code.
It applies one style only.
**/

#include "a68g.h"

#define ONE_LINER (A68_TRUE)
#define KEYWORD (A68_TRUE)
#define BLANK {put_str (" ");}

#define IS_OPEN_SYMBOL(p) (IS (p, OPEN_SYMBOL) || IS (p, SUB_SYMBOL) || IS (p, ACCO_SYMBOL))
#define IS_CLOSE_SYMBOL(p) (IS (p, CLOSE_SYMBOL) || IS (p, BUS_SYMBOL) || IS (p, OCCA_SYMBOL))
#define IS_IDENTIFIER(p) (IS (p, IDENTIFIER) || IS (p, DEFINING_IDENTIFIER) || IS (p, FIELD_IDENTIFIER))

static char in_line[BUFFER_SIZE];

static FILE_T fd;

static int ind, col;
static int indentation = 0;
static BOOL_T use_folder;

static void in_declarer (NODE_T *);
static void in_serial (NODE_T *, BOOL_T, NODE_T **);
static void in_statement (NODE_T *);
static void in_format (NODE_T *);

/**
@brief Write newline and indent.
**/

static void put_nl (void)
{
  WRITE (fd, "\n");
  for (col = 1; col < ind % 72; col ++) {
    WRITE (fd, " ");
  }
}

/**
@brief Write a string.
@param txt
**/

static void put_str (char *txt)
{
  WRITE (fd, txt);
  col += (int) strlen (txt);
}

/**
@brief Write a character.
@param ch
**/

static void put_ch (char ch)
{
  char str[2];
  str[0] = ch;
  str[1] = NULL_CHAR;
  put_str (str);
}

/**
@brief Write pragment string.
@param p Node in syntax tree.
**/

static void put_pragment (NODE_T *p)
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

/**
@brief Write pragment string.
@param p Node in syntax tree.
@param keyw Whether keyword.
**/

static void pragment (NODE_T *p, BOOL_T keyw)
{
  if (NPRAGMENT (p) != NO_TEXT) {
    if (NPRAGMENT_TYPE (p) == BOLD_COMMENT_SYMBOL ||
        NPRAGMENT_TYPE (p) == BOLD_PRAGMAT_SYMBOL) {
      if (! keyw) {
        put_nl ();
      }
      put_pragment (p);
      put_nl ();
      put_nl ();
    } else {
      if (! keyw && (int) strlen (NPRAGMENT (p)) < 20) {
        if (col > ind) {
          BLANK;
        }
        put_pragment (p);
        BLANK;
      } else {
        if (col > ind) {
          put_nl ();
        }
        put_pragment (p);
        put_nl ();
      }
    }
  }}

/**
@brief Write with typographic display features.
@param p Node in syntax tree.
@param keyw Whether p is a keyword.
**/

static void put_sym (NODE_T *p, BOOL_T keyw)
{
  char *txt = NSYMBOL (p);
  char *sym = NCHAR_IN_LINE (p);
  int n = 0, size = (int) strlen (txt);
  pragment (p, keyw);
  if (txt[0] != sym[0] || (int) strlen (sym) - 1 <= size) {
/* Without features. */
    put_str (txt);
  } else {
/* With features. Preserves spaces in identifiers etcetera. */
    while (n < size) {
      put_ch (sym[0]);
      if (TO_LOWER (txt[0]) == TO_LOWER (sym[0])) {
        txt ++;
        n ++;
      }
      sym ++;
    }
  }
}

/**
@brief Count units and separators in a sub-tree.
@param p Node in syntax tree.
@param units
@param seps
**/

static void count (NODE_T *p, int *units, int *seps)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, UNIT)) {
      (*units) ++;
      count (SUB (p), units, seps);
    } else if (IS (p, SEMI_SYMBOL)) {
      (*seps) ++;
    } else if (IS (p, COMMA_SYMBOL)) {
      (*seps) ++;
    } else {
      count (SUB (p), units, seps);
    }
  }
}

/**
@brief Count units and separators in a sub-tree.
@param p Node in syntax tree.
@param units
@param seps
**/

static void count_stowed (NODE_T *p, int *units, int *seps)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, UNIT)) {
      MOID_T *v = MOID (p);
      BOOL_T stowed = (BOOL_T) (IS (v, FLEX_SYMBOL) || 
                                IS (v, ROW_SYMBOL) || 
                                IS (v, STRUCT_SYMBOL));
      if (stowed) {
        (*units) ++;
      }
    } else if (IS (p, SEMI_SYMBOL)) {
      (*seps) ++;
    } else if (IS (p, COMMA_SYMBOL)) {
      (*seps) ++;
    } else {
      count_stowed (SUB (p), units, seps);
    }
  }
}

/**
@brief Count enclosed_clauses in a sub-tree.
@param p Node in syntax tree.
@param enclos
@param seps
**/

static void count_enclos (NODE_T *p, int *enclos, int *seps)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, ENCLOSED_CLAUSE)) {
      (*enclos) ++;
    } else if (IS (p, SEMI_SYMBOL)) {
      (*seps) ++;
    } else if (IS (p, COMMA_SYMBOL)) {
      (*seps) ++;
    } else {
      count_enclos (SUB (p), enclos, seps);
    }
  }
}

/**
@brief Indent sizety.
@param p Node in syntax tree.
**/

static void in_sizety (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, LONGETY) || IS (p, SHORTETY)) {
      in_sizety (SUB (p));
    } else if (IS (p, LONG_SYMBOL) || IS (p, SHORT_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    }
  }
}

/**
@brief Indent generic list.
@param p Node in syntax tree.
@param what Pointer to node that will explain list type.
@param one_liner Whether construct is one-liner.
**/

static void in_generic_list (NODE_T * p, NODE_T ** what, BOOL_T one_liner)
{
  for (; p != NULL; FORWARD (p)) {
    if (IS_OPEN_SYMBOL (p)) {
      put_sym (p, KEYWORD);
      ind = col;
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
      in_statement (SUB (p));
    } else if (IS (p, SPECIFIER)) {
      NODE_T *q = SUB (p);
      put_sym (q, KEYWORD);
      FORWARD (q);
      in_declarer (q);
      FORWARD (q);
      if (IS_IDENTIFIER (q)) {
        BLANK;
        put_sym (q, !KEYWORD);
        FORWARD (q);
      }
      put_sym (q, !KEYWORD);
      FORWARD (q);
      put_sym (NEXT (p), !KEYWORD); /* : */
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
      in_generic_list (SUB (p), what, one_liner);
    }
  }
}

/**
@brief Indent declarer pack.
@param p Node in syntax tree.
**/

static void in_pack (NODE_T *p)
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
      in_declarer (p);
      if (NEXT (p) != NO_NODE && IS_IDENTIFIER (NEXT (p))) {
        BLANK;
      }
    } else if (IS_IDENTIFIER (p)) {
      put_sym (p, !KEYWORD);
    } else {
      in_pack (SUB (p));
    }
  }
}

/**
@brief Indent declarer.
@param p Node in syntax tree.
**/

static void in_declarer (NODE_T *p)
{
  if (IS (p, DECLARER)) {
    in_declarer (SUB (p));
  } else if (IS (p, LONGETY) || IS (p, SHORTETY)) {
    in_sizety (SUB (p));
    in_declarer (NEXT (p));
  } else if (IS (p, VOID_SYMBOL)) {
    put_sym (p, !KEYWORD);
  } else if (IS (p, REF_SYMBOL)) {
    put_sym (p, !KEYWORD);
    BLANK;
    in_declarer (NEXT (p));
  } else if (IS (p, FLEX_SYMBOL)) {
    put_sym (p, !KEYWORD);
    BLANK;
    in_declarer (NEXT (p));
  } else if (IS (p, BOUNDS) || IS (p, FORMAL_BOUNDS)) {
    NODE_T *what = NO_NODE;
    int pop_ind = ind;
    in_generic_list (SUB (p), &what, ONE_LINER);
    ind = pop_ind;
    BLANK;
    in_declarer (NEXT (p));
  } else if (IS (p, STRUCT_SYMBOL) || IS (p, UNION_SYMBOL)) {
    NODE_T *pack = NEXT (p);
    put_sym (p, !KEYWORD);
    BLANK;
    in_pack (pack);
  } else if (IS (p, PROC_SYMBOL)) {
    NODE_T *q = NEXT (p);
    put_sym (p, KEYWORD);
    BLANK;
    if (IS (q, FORMAL_DECLARERS)) {
      in_pack (SUB (q));
      BLANK;
      FORWARD (q);
    }
    in_declarer (q);
    return;
  } else if (IS (p, OP_SYMBOL)) { /* Operator plan */
    NODE_T *q = NEXT (p);
    put_sym (p, KEYWORD);
    BLANK;
    if (IS (q, FORMAL_DECLARERS)) {
      in_pack (SUB (q));
      BLANK;
      FORWARD (q);
    }
    in_declarer (q);
    return;
  } else if (IS (p, INDICANT)) {
    put_sym (p, !KEYWORD);
  }
}

/**
@brief Indent conditional.
@param p Node in syntax tree.
**/

static void in_conditional (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, IF_PART) || IS (p, ELIF_IF_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_serial (NEXT_SUB (p), !ONE_LINER, &what);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, THEN_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_serial (NEXT_SUB (p), !ONE_LINER, &what);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, ELSE_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_serial (NEXT_SUB (p), !ONE_LINER, &what);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, ELIF_PART)) {
      in_conditional (SUB (p));
    } else if (IS (p, FI_SYMBOL)) {
      put_sym (p, KEYWORD);
    } else if (IS (p, OPEN_PART)) {
      NODE_T *what = NO_NODE;
      put_sym (SUB (p), KEYWORD);
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, ELSE_OPEN_PART)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, CHOICE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, BRIEF_ELIF_PART)) {
      in_conditional (SUB (p));
    } else if (IS_CLOSE_SYMBOL (p)) {
      put_sym (p, KEYWORD);
    }
  }
}

/**
@brief Indent integer case clause.
@param p Node in syntax tree.
**/

static void in_case (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, CASE_PART) || IS (p, OUSE_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_serial (NEXT_SUB (p), !ONE_LINER, &what);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, CASE_IN_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_generic_list (NEXT_SUB (p), &what, ONE_LINER);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, OUT_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_serial (NEXT_SUB (p), !ONE_LINER, &what);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, CASE_OUSE_PART)) {
      in_case (SUB (p));
    } else if (IS (p, ESAC_SYMBOL)) {
      put_sym (p, KEYWORD);
    } else if (IS (p, OPEN_PART)) {
      NODE_T *what = NO_NODE;
      put_sym (SUB (p), KEYWORD);
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, ELSE_OPEN_PART)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, CASE_CHOICE_CLAUSE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_generic_list (NEXT_SUB (p), &what, ONE_LINER);
    } else if (IS (p, CHOICE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, BRIEF_OUSE_PART)) {
      in_case (SUB (p));
    } else if (IS_CLOSE_SYMBOL (p)) {
      put_sym (p, KEYWORD);
    }
  }
}

/**
@brief Indent conformity clause.
@param p Node in syntax tree.
**/

static void in_conformity (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, CASE_PART) || IS (p, OUSE_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_serial (NEXT_SUB (p), !ONE_LINER, &what);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, CONFORMITY_IN_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_generic_list (NEXT_SUB (p), &what, ONE_LINER);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, OUT_PART)) {
      NODE_T *what = NO_NODE;
      int pop_ind = ind;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_serial (NEXT_SUB (p), !ONE_LINER, &what);
      ind = pop_ind;
      put_nl ();
    } else if (IS (p, CONFORMITY_OUSE_PART)) {
      in_conformity (SUB (p));
    } else if (IS (p, ESAC_SYMBOL)) {
      put_sym (p, KEYWORD);
    } else if (IS (p, OPEN_PART)) {
      NODE_T *what = NO_NODE;
      put_sym (SUB (p), KEYWORD);
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, ELSE_OPEN_PART)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, CONFORMITY_CHOICE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_generic_list (NEXT_SUB (p), &what, ONE_LINER);
    } else if (IS (p, CHOICE)) {
      NODE_T *what = NO_NODE;
      BLANK;
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_serial (NEXT_SUB (p), ONE_LINER, &what);
    } else if (IS (p, BRIEF_CONFORMITY_OUSE_PART)) {
      in_conformity (SUB (p));
    } else if (IS_CLOSE_SYMBOL (p)) {
      put_sym (p, KEYWORD);
    }
  }
}

/**
@brief Indent loop.
@param p Node in syntax tree.
**/

static void in_loop (NODE_T * p)
{
  int parts = 0, pop_ind = col;
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FOR_PART)) {
      put_sym (SUB (p), KEYWORD);
      BLANK;
      put_sym (NEXT_SUB (p), !KEYWORD);
      BLANK;
      parts ++;
    } else if (is_one_of (p, FROM_PART, BY_PART, TO_PART, STOP)) {
      put_sym (SUB (p), KEYWORD);
      BLANK;
      in_statement (NEXT_SUB (p));
      BLANK;
      parts ++;
    } else if (IS (p, WHILE_PART)) {
      NODE_T *what = NO_NODE;
      ind = pop_ind;
      if (parts > 0) {
        put_nl ();
      }
      put_sym (SUB (p), KEYWORD);
      BLANK;
      ind = col;
      in_serial (NEXT_SUB (p), !ONE_LINER, &what);
      ind = pop_ind;
      parts ++;
    } else if (is_one_of (p, DO_PART, ALT_DO_PART, STOP)) {
      NODE_T *q = SUB (p);
      NODE_T *what = NO_NODE;
      ind = pop_ind;
      if (parts > 0) {
        put_nl ();
      }
      put_sym (q, KEYWORD); /* DO */
      BLANK;
      ind = col;
      FORWARD (q);
      parts = 0;
      if (IS (q, SERIAL_CLAUSE)) {
        in_serial (SUB (q), !ONE_LINER, &what);
        FORWARD (q);
        parts ++;
      }
      if (IS (q, UNTIL_PART)) {
        int pop_ind2 = ind;
        if (parts > 0) {
          put_nl ();
        }
        put_sym (SUB (q), KEYWORD);
        BLANK;
        ind = col;
        in_serial (NEXT_SUB (q), !ONE_LINER, &what);
        ind = pop_ind2;
        FORWARD (q);
      }
      ind = pop_ind;
      put_nl ();
      put_sym (q, KEYWORD); /* OD */
      parts ++;
    }
  }
}

/**
@brief Indent closed clause.
@param p Node in syntax tree.
**/

static void in_closed (NODE_T *p)
{
  int units = 0, seps = 0;
  count (SUB_NEXT (p), &units, &seps);
  if (units == 1 && seps == 0) {
    put_sym (p, KEYWORD);
    if (IS (p, BEGIN_SYMBOL)) {
      NODE_T *what = NO_NODE;
      BLANK;
      in_serial (SUB_NEXT (p), ONE_LINER, &what);
      BLANK;
    } else {
      NODE_T *what = NO_NODE;
      in_serial (SUB_NEXT (p), ONE_LINER, &what);
    }
    put_sym (NEXT_NEXT (p), KEYWORD);
  } else if (units <= 3 && seps == (units - 1) && IS_OPEN_SYMBOL (p)) {
      NODE_T *what = NO_NODE;
    put_sym (p, KEYWORD);
    in_serial (SUB_NEXT (p), ONE_LINER, &what);
    put_sym (NEXT_NEXT (p), KEYWORD);
  } else {
    NODE_T *what = NO_NODE;
    int pop_ind = ind;
    put_sym (p, KEYWORD);
    if (IS (p, BEGIN_SYMBOL)) {
      BLANK;
    }
    ind = col;
    in_serial (SUB_NEXT (p), !ONE_LINER, &what);
    ind = pop_ind;
    if (IS (NEXT_NEXT (p), END_SYMBOL)) {
      put_nl ();
    }
    put_sym (NEXT_NEXT (p), KEYWORD);
  }
}

/**
@brief Indent collateral clause.
@param p Node in syntax tree.
**/

static void in_collateral (NODE_T *p)
{
  int units = 0, seps = 0;
  NODE_T *what = NO_NODE;
  int pop_ind = ind;
  count_stowed (p, &units, &seps);
  if (units <= 3) {
    in_generic_list (p, &what, ONE_LINER);
  } else {
    in_generic_list (p, &what, !ONE_LINER);
  }
  ind = pop_ind;
}

/**
@brief Indent enclosed clause.
@param p Node in syntax tree.
**/

static void in_enclosed (NODE_T *p)
{
  if (IS (p, ENCLOSED_CLAUSE)) {
    in_enclosed (SUB (p));
  } else if (IS (p, CLOSED_CLAUSE)) {
    in_closed (SUB (p));
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    in_collateral (SUB (p));
  } else if (IS (p, PARALLEL_CLAUSE)) {
    put_sym (SUB (p), KEYWORD);
    in_enclosed (NEXT_SUB (p));
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    in_conditional (SUB (p));
  } else if (IS (p, CASE_CLAUSE)) {
    in_case (SUB (p));
  } else if (IS (p, CONFORMITY_CLAUSE)) {
    in_conformity (SUB (p));
  } else if (IS (p, LOOP_CLAUSE)) {
    in_loop (SUB (p));
  }
}

/**
@brief Indent a literal.
@param txt
**/

static void in_literal (char *txt)
{
  put_str ("\"");
  while (txt[0] != NULL_CHAR) {
    if (txt[0] == '\"') {
      put_str ("\"\"");
    } else {
      put_ch (txt[0]);
    }
    txt ++;
  }
  put_str ("\"");
}

/**
@brief Indent denotation.
@param p Node in syntax tree.
**/

static void in_denotation (NODE_T *p)
{
  if (IS (p, ROW_CHAR_DENOTATION)) {
    in_literal (NSYMBOL (p));
  } else if (IS (p, LONGETY) || IS (p, SHORTETY)) {
    in_sizety (SUB (p));
    in_denotation (NEXT (p));
  } else {
    put_sym (p, !KEYWORD);
  }
}

/**
@brief Indent label.
@param p Node in syntax tree.
**/

static void in_label (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NULL) {
      in_label (SUB (p));
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      put_sym (p, !KEYWORD);
      put_sym (NEXT (p), KEYWORD);
    }
  }
}

/**
@brief Indent literal list.
@param p Node in syntax tree.
**/

static void in_collection (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, FORMAT_OPEN_SYMBOL) || IS (p, FORMAT_CLOSE_SYMBOL)) {
      put_sym (p, !KEYWORD);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    } else {
      in_format (SUB (p));
    }
  }
}

/**
@brief Indent format text.
@param p Node in syntax tree.
**/

static void in_format (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FORMAT_DELIMITER_SYMBOL)) {
      put_sym (p, !KEYWORD);
    } else if (IS (p, COLLECTION)) {
      in_collection (SUB (p));
    } else if (IS (p, ENCLOSED_CLAUSE)) {
      in_enclosed (SUB (p));
    } else if (IS (p, LITERAL)) {
      in_literal (NSYMBOL (p));
    } else if (IS (p, STATIC_REPLICATOR)) {
      in_denotation (p);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    } else {
       if (SUB (p) != NO_NODE) {
         in_format (SUB (p));
       } else {
         switch (ATTRIBUTE (p)) {
           case FORMAT_ITEM_A: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_B: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_C: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_D: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_E: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_ESCAPE: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_F: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_G: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_H: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_I: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_J: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_K: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_L: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_M: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_MINUS: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_N: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_O: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_P: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_PLUS: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_POINT: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_Q: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_R: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_S: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_T: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_U: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_V: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_W: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_X: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_Y: put_sym (p, !KEYWORD); break;
           case FORMAT_ITEM_Z: put_sym (p, !KEYWORD); break;
         }
       }
    }
  }
}

/**
@brief Constant folder - replace constant statement with value.
@param p Node in syntax tree.
**/

static BOOL_T in_folder (NODE_T *p)
{
  if (MOID (p) == MODE (INT)) {
    A68_INT k;
    stack_pointer = 0;
    push_unit (p);
    POP_OBJECT (p, &k, A68_INT);
    if (ERROR_COUNT (&program) == 0) {
      ASSERT (snprintf (in_line, SNPRINTF_SIZE, "%d", VALUE (&k)) >= 0);
      put_str (in_line);
      return (A68_TRUE);
    } else {
      return (A68_FALSE);
    }
  } else if (MOID (p) == MODE (REAL)) {
    A68_REAL x;
    double conv;
    stack_pointer = 0;
    push_unit (p);
    POP_OBJECT (p, &x, A68_REAL);
/* Mind overflowing or underflowing values */
    if (ERROR_COUNT (&program) != 0) {
      return (A68_FALSE);
    } else if (VALUE (&x) == DBL_MAX) {
      return (A68_FALSE);
    } else if (VALUE (&x) == -DBL_MAX) {
      return (A68_FALSE);
    } else {
      ASSERT (snprintf (in_line, SNPRINTF_SIZE, "%.*g", REAL_WIDTH, VALUE (&x)) >= 0);
      errno = 0;
      conv = strtod (in_line, NO_VAR);
      if (errno == ERANGE && conv == 0.0) {
        put_str ("0.0");
        return (A68_TRUE);
      } else if (errno == ERANGE) {
        return (A68_FALSE);
      } else {
        if (strchr (in_line, '.') == NO_TEXT && 
            strchr (in_line, 'e') == NO_TEXT && 
            strchr (in_line, 'E') == NO_TEXT) {
          strncat (in_line, ".0", BUFFER_SIZE);
        }
        put_str (in_line);
        return (A68_TRUE);
      }
    }
  } else if (MOID (p) == MODE (BOOL)) {
    A68_BOOL b;
    stack_pointer = 0;
    push_unit (p);
    POP_OBJECT (p, &b, A68_BOOL);
    if (ERROR_COUNT (&program) != 0) {
      return (A68_FALSE);
    } else {
      ASSERT (snprintf (in_line, SNPRINTF_SIZE, "%s", (VALUE (&b) ? "TRUE" : "FALSE")) >= 0);
      put_str (in_line);
      return (A68_TRUE);
    }
  } else if (MOID (p) == MODE (CHAR)) {
    A68_CHAR c;
    stack_pointer = 0;
    push_unit (p);
    POP_OBJECT (p, &c, A68_CHAR);
    if (ERROR_COUNT (&program) == 0) {
      return (A68_FALSE);
    } else if (VALUE (&c) == '\"') {
      put_str ("\"\"\"\"");
      return (A68_TRUE);
    } else {
      ASSERT (snprintf (in_line, SNPRINTF_SIZE, "\"%c\"", VALUE (&c)) >= 0);
      return (A68_TRUE);
    }
  }
  return (A68_FALSE);
}

/**
@brief Indent statement.
@param p Node in syntax tree.
**/

static void in_statement (NODE_T *p)
{
  if (IS (p, LABEL)) {
    int enclos = 0, seps = 0;
    in_label (SUB (p));
    FORWARD (p);
    count_enclos (SUB (p), &enclos, &seps);
    if (enclos == 0) {
      BLANK;
    } else {
      put_nl ();
    }
  }
  if (use_folder && folder_mode (MOID (p)) && constant_unit (p)) {
    if (in_folder (p)) {
      return;
    };
  }
  if (is_coercion (p)) {
    in_statement (SUB (p));
  } else if (is_one_of (p, PRIMARY, SECONDARY, TERTIARY, UNIT, LABELED_UNIT, STOP)) {
    in_statement (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    in_enclosed (SUB (p));
  } else if (IS (p, DENOTATION)) {
    in_denotation (SUB (p));
  } else if (IS (p, FORMAT_TEXT)) {
    in_format (SUB (p));
  } else if (IS (p, IDENTIFIER)) {
    put_sym (p, !KEYWORD);
  } else if (IS (p, CAST)) {
    NODE_T *decl = SUB (p);
    NODE_T *rhs = NEXT (decl);
    in_declarer (decl);
    BLANK;
    in_enclosed (rhs); 
  } else if (IS (p, CALL)) {
    NODE_T *primary = SUB (p);
    NODE_T *arguments = NEXT (primary);
    NODE_T *what = NO_NODE;
    int pop_ind = ind;
    in_statement (primary);
    BLANK;
    in_generic_list (arguments, &what, ONE_LINER);
    ind = pop_ind;
  } else if (IS (p, SLICE)) {
    NODE_T *primary = SUB (p);
    NODE_T *indexer = NEXT (primary);
    NODE_T *what = NO_NODE;
    int pop_ind = ind;
    in_statement (primary);
    in_generic_list (indexer, &what, ONE_LINER);
    ind = pop_ind;
  } else if (IS (p, SELECTION)) {
    NODE_T *selector = SUB (p);
    NODE_T *secondary = NEXT (selector);
    in_statement (selector);
    in_statement (secondary);
  } else if (IS (p, SELECTOR)) {
    NODE_T *identifier = SUB (p);
    put_sym (identifier, !KEYWORD);
    BLANK;
    put_sym (NEXT (identifier), !KEYWORD); /* OF */
    BLANK;
  } else if (IS (p, GENERATOR)) {
    NODE_T *q = SUB (p);
    put_sym (q, !KEYWORD);
    BLANK;
    in_declarer (NEXT (q));
  } else if (IS (p, FORMULA)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    in_statement (lhs);
    if (op != NO_NODE) {
      NODE_T *rhs = NEXT (op);
      BLANK;
      put_sym (op, !KEYWORD);
      BLANK;
      in_statement (rhs);
    }
  } else if (IS (p, MONADIC_FORMULA)) {
    NODE_T *op = SUB (p);
    NODE_T *rhs = NEXT (op);
    put_sym (op, !KEYWORD);
    if (a68g_strchr (MONADS, (NSYMBOL (op))[0]) == NO_TEXT) {
      BLANK;
    }
    in_statement (rhs);
  } else if (IS (p, NIHIL)) {
    put_sym (p, !KEYWORD);
  } else if (IS (p, AND_FUNCTION) || IS (p, OR_FUNCTION)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    NODE_T *rhs = NEXT (op);
    in_statement (lhs);
    BLANK;
    put_sym (op, !KEYWORD);
    BLANK;
    in_statement (rhs);
  } else if (IS (p, TRANSPOSE_FUNCTION) ||
             IS (p, DIAGONAL_FUNCTION) ||
             IS (p, ROW_FUNCTION) ||
             IS (p, COLUMN_FUNCTION)) {
    NODE_T *q = SUB (p);
    if (IS (p, TERTIARY)) {
      in_statement (q);
      BLANK;
      FORWARD (q);
    }
    put_sym (q, !KEYWORD);
    BLANK;
    in_statement (NEXT (q));
  } else if (IS (p, ASSIGNATION)) {
    NODE_T *dst = SUB (p);
    NODE_T *bec = NEXT (dst);
    NODE_T *src = NEXT (bec);
    in_statement (dst);
    BLANK;
    put_sym (bec, !KEYWORD);
    BLANK;
    in_statement (src); 
  } else if (IS (p, ROUTINE_TEXT)) {
    NODE_T *q = SUB (p);
    int units, seps;
    if (IS (q, PARAMETER_PACK)) {
      in_pack (SUB (q));
      BLANK;
      FORWARD (q);
    }
    in_declarer (q);
    FORWARD (q);
    put_sym (q, !KEYWORD); /* : */
    FORWARD (q);
    units = 0;
    seps = 0;
    count (q, &units, &seps);
    if (units <= 1 && seps == 0) {
      BLANK;
      in_statement (q);
    } else {
      put_nl ();
      in_statement (q);
    }
  } else if (IS (p, IDENTITY_RELATION)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    NODE_T *rhs = NEXT (op);
    in_statement (lhs);
    BLANK;
    put_sym (op, !KEYWORD);
    BLANK;
    in_statement (rhs);
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
    in_enclosed (NEXT (q));
  } else if (IS (p, CODE_CLAUSE)) {
    NODE_T *q = SUB (p);
    put_sym (q, KEYWORD);
    BLANK;
    FORWARD (q);
    in_collection(SUB (q));
    FORWARD (q);
    put_sym (q, KEYWORD);
  }
}

/**
@brief Indent identifier declarations.
@param p Node in syntax tree.
**/

static void in_iddecl (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, IDENTITY_DECLARATION) || IS (p, VARIABLE_DECLARATION)) {
      in_iddecl (SUB (p));
    } else if (IS (p, QUALIFIER)) {
      put_sym (SUB (p), !KEYWORD);
      BLANK;
    } else if (IS (p, DECLARER)) {
      in_declarer (SUB (p));
      BLANK;
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      NODE_T *q = p;
      put_sym (q, !KEYWORD);
      FORWARD (q);
      if (q != NO_NODE) { /* := unit */
        BLANK;
        put_sym (q, !KEYWORD);
        BLANK;
        FORWARD (q);
        in_statement (q);
      }
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      BLANK;
    }
  }
}

/**
@brief Indent procedure declarations.
@param p Node in syntax tree.
**/

static void in_procdecl (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, PROCEDURE_DECLARATION) || 
        IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      in_procdecl (SUB (p));
    } else if (IS (p, PROC_SYMBOL)) {
      put_sym (p, KEYWORD);
      BLANK;
      ind = col;
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      NODE_T *q = p;
      put_sym (q, !KEYWORD);
      FORWARD (q);
      BLANK;
      put_sym (q, !KEYWORD);
      BLANK;
      FORWARD (q);
      in_statement (q);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      put_nl ();
      BLANK;
    }
  }
}

/**
@brief Indent operator declarations.
@param p Node in syntax tree.
**/

static void in_opdecl (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, OPERATOR_DECLARATION) || 
        IS (p, BRIEF_OPERATOR_DECLARATION)) {
      in_opdecl (SUB (p));
    } else if (IS (p, OP_SYMBOL)) {
      put_sym (p, KEYWORD);
      BLANK;
      ind = col;
    } else if (IS (p, OPERATOR_PLAN)) {
      in_declarer (SUB (p));
      BLANK;
      ind = col;
    } else if (IS (p, DEFINING_OPERATOR)) {
      NODE_T *q = p;
      put_sym (q, !KEYWORD);
      FORWARD (q);
      BLANK;
      put_sym (q, !KEYWORD);
      BLANK;
      FORWARD (q);
      in_statement (q);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      put_nl ();
      BLANK;
    }
  }
}

/**
@brief Indent priority declarations.
@param p Node in syntax tree.
**/

static void in_priodecl (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, PRIORITY_DECLARATION)) {
      in_priodecl (SUB (p));
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

/** 
@brief Indent mode declarations.
@param p Node in syntax tree.
**/

static void in_modedecl (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p))
  {
    if (IS (p, MODE_DECLARATION)) {
      in_modedecl (SUB (p));
    } else if (IS (p, MODE_SYMBOL)) {
      put_sym (p, KEYWORD);
      BLANK;
      ind = col;
    } else if (IS (p, DEFINING_INDICANT)) {
      NODE_T *q = p;
      put_sym (q, !KEYWORD);
      FORWARD (q);
      BLANK;
      put_sym (q, !KEYWORD);
      BLANK;
      FORWARD (q);
      in_declarer (q);
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      put_nl ();
      BLANK;
    }
  }
}

/**
@brief Indent declaration list.
@param p Node in syntax tree.
@param one_liner Whether construct is one-liner.
**/

static void in_declist (NODE_T *p, BOOL_T one_liner)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, IDENTITY_DECLARATION) || IS (p, VARIABLE_DECLARATION)) {
      int pop_ind = ind;
      in_iddecl (p);
      ind = pop_ind;
    } else if (IS (p, PROCEDURE_DECLARATION) || 
               IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      int pop_ind = ind;
      in_procdecl (p);
      ind = pop_ind;
    } else if (IS (p, OPERATOR_DECLARATION) || 
               IS (p, BRIEF_OPERATOR_DECLARATION)) {
      int pop_ind = ind;
      in_opdecl (p);
      ind = pop_ind;
    } else if (IS (p, PRIORITY_DECLARATION)) {
      int pop_ind = ind;
      in_priodecl (p);
      ind = pop_ind;
    } else if (IS (p, MODE_DECLARATION)) {
      int pop_ind = ind;
      in_modedecl (p);
      ind = pop_ind;
    } else if (IS (p, COMMA_SYMBOL)) {
      put_sym (p, !KEYWORD);
      if (one_liner) {
        BLANK;
      } else {
        put_nl ();
      }
    } else {
      in_declist (SUB (p), one_liner);
    }
  }
}

/**
@brief Indent serial clause.
@param p Node in syntax tree.
@param one_liner Whether construct is one-liner.
@param what Pointer telling type of construct.
**/

static void in_serial (NODE_T *p, BOOL_T one_liner, NODE_T **what)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT) || IS (p, LABELED_UNIT)) {
      int pop_ind = ind;
      (*what) = p;
      in_statement (p);
      ind = pop_ind;
    } else if (IS (p, DECLARATION_LIST)) {
      (*what) = p;
      in_declist (p, one_liner);
    } else if (IS (p, SEMI_SYMBOL)) {
      put_sym (p, !KEYWORD);
      if (!one_liner) {
        put_nl ();
        if ((*what) != NO_NODE && IS ((*what), DECLARATION_LIST)) {
          put_nl ();
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
      in_serial (SUB (p), one_liner, what);
    }
  }
}

/**
@brief Do not pretty-print the environ.
@param p Node in syntax tree.
**/

static void skip_environ (NODE_T *p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (LINE_NUMBER (p) == 0) {
      pragment (p, ! KEYWORD);
      skip_environ (SUB (p));
    } else {
      NODE_T *what = NO_NODE;
      in_serial (p, !ONE_LINER, &what);
    }
  }
}

/**
@brief Indenter driver.
@param q Module to indent.
**/

void indenter (MODULE_T *q)
{
  ind = 1;
  col = 1;
  indentation = OPTION_INDENT (q);
  use_folder = OPTION_FOLD (q);
  FILE_PRETTY_FD (q) = open (FILE_PRETTY_NAME (q), O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (FILE_PRETTY_FD (q) == -1, "cannot open listing file", NO_TEXT);
  FILE_PRETTY_OPENED (q) = A68_TRUE;
  fd = FILE_PRETTY_FD (q);
  skip_environ (TOP_NODE (q));
  ASSERT (close (fd) == 0);
  FILE_PRETTY_OPENED (q) = A68_FALSE;
  return;
}
