//! @file top-down.c
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

// Top-down parser, elaborates the control structure.

//! @brief Substitute brackets.

void substitute_brackets (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    substitute_brackets (SUB (p));
    switch (ATTRIBUTE (p)) {
    case ACCO_SYMBOL:
      {
        ATTRIBUTE (p) = OPEN_SYMBOL;
        break;
      }
    case OCCA_SYMBOL:
      {
        ATTRIBUTE (p) = CLOSE_SYMBOL;
        break;
      }
    case SUB_SYMBOL:
      {
        ATTRIBUTE (p) = OPEN_SYMBOL;
        break;
      }
    case BUS_SYMBOL:
      {
        ATTRIBUTE (p) = CLOSE_SYMBOL;
        break;
      }
    }
  }
}

//! @brief Intelligible diagnostic from syntax tree branch.

char *phrase_to_text (NODE_T * p, NODE_T ** w)
{
#define MAX_TERMINALS 8
  int count = 0, line = -1;
  static char buffer[BUFFER_SIZE];
  for (buffer[0] = NULL_CHAR; p != NO_NODE && count < MAX_TERMINALS; FORWARD (p)) {
    if (LINE_NUMBER (p) > 0) {
      int gatt = get_good_attribute (p);
      char *z = non_terminal_string (A68 (input_line), gatt);
// Where to put the error message? Bob Uzgalis noted that actual content of a 
// diagnostic is not as important as accurately indicating *were* the problem is! 
      if (w != NO_VAR) {
        if (count == 0 || (*w) == NO_NODE) {
          *w = p;
        } else if (dont_mark_here (*w)) {
          *w = p;
        }
      }
// Add initiation.
      if (count == 0) {
        if (w != NO_VAR) {
          bufcat (buffer, "construct beginning with", BUFFER_SIZE);
        }
      } else if (count == 1) {
        bufcat (buffer, " followed by", BUFFER_SIZE);
      } else if (count == 2) {
        bufcat (buffer, " and then", BUFFER_SIZE);
      } else if (count >= 3) {
        bufcat (buffer, " and", BUFFER_SIZE);
      }
// Attribute or symbol.
      if (z != NO_TEXT && SUB (p) != NO_NODE) {
        if (gatt == IDENTIFIER || gatt == OPERATOR || gatt == DENOTATION) {
          ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
          bufcat (buffer, A68 (edit_line), BUFFER_SIZE);
        } else {
          if (strchr ("aeio", z[0]) != NO_TEXT) {
            bufcat (buffer, " an", BUFFER_SIZE);
          } else {
            bufcat (buffer, " a", BUFFER_SIZE);
          }
          ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, " %s", z) >= 0);
          bufcat (buffer, A68 (edit_line), BUFFER_SIZE);
        }
      } else if (z != NO_TEXT && SUB (p) == NO_NODE) {
        ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
        bufcat (buffer, A68 (edit_line), BUFFER_SIZE);
      } else if (NSYMBOL (p) != NO_TEXT) {
        ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
        bufcat (buffer, A68 (edit_line), BUFFER_SIZE);
      }
// Add "starting in line nn".
      if (z != NO_TEXT && line != LINE_NUMBER (p)) {
        line = LINE_NUMBER (p);
        if (gatt == SERIAL_CLAUSE || gatt == ENQUIRY_CLAUSE || gatt == INITIALISER_SERIES) {
          bufcat (buffer, " starting", BUFFER_SIZE);
        }
        ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, " in line %d", line) >= 0);
        bufcat (buffer, A68 (edit_line), BUFFER_SIZE);
      }
      count++;
    }
  }
  if (p != NO_NODE && count == MAX_TERMINALS) {
    bufcat (buffer, " etcetera", BUFFER_SIZE);
  }
  return buffer;
}

// Next is a top-down parser that branches out the basic blocks.
// After this we can assign symbol tables to basic blocks.
// This renders the two-level grammar LALR.

//! @brief Give diagnose from top-down parser.

void top_down_diagnose (NODE_T * start, NODE_T * posit, int clause, int expected)
{
  NODE_T *issue = (posit != NO_NODE ? posit : start);
  if (expected != 0) {
    diagnostic (A68_SYNTAX_ERROR, issue, ERROR_EXPECTED_NEAR, expected, clause, NSYMBOL (start), LINE (INFO (start)));
  } else {
    diagnostic (A68_SYNTAX_ERROR, issue, ERROR_UNBALANCED_KEYWORD, clause, NSYMBOL (start), LINE (INFO (start)));
  }
}

//! @brief Check for premature exhaustion of tokens.

void tokens_exhausted (NODE_T * p, NODE_T * q)
{
  if (p == NO_NODE) {
    diagnostic (A68_SYNTAX_ERROR, q, ERROR_KEYWORD);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
  }
}

// This part specifically branches out loop clauses.

//! @brief Whether in cast or formula with loop clause.

int is_loop_cast_formula (NODE_T * p)
{
// Accept declarers that can appear in such casts but not much more.
  if (IS (p, VOID_SYMBOL)) {
    return 1;
  } else if (IS (p, INT_SYMBOL)) {
    return 1;
  } else if (IS_REF (p)) {
    return 1;
  } else if (is_one_of (p, OPERATOR, BOLD_TAG, STOP)) {
    return 1;
  } else if (whether (p, UNION_SYMBOL, OPEN_SYMBOL, STOP)) {
    return 2;
  } else if (is_one_of (p, OPEN_SYMBOL, SUB_SYMBOL, STOP)) {
    int k;
    for (k = 0; p != NO_NODE && (is_one_of (p, OPEN_SYMBOL, SUB_SYMBOL, STOP)); FORWARD (p), k++) {
      ;
    }
    return p != NO_NODE && (whether (p, UNION_SYMBOL, OPEN_SYMBOL, STOP) ? k : 0);
  }
  return 0;
}

//! @brief Skip a unit in a loop clause (FROM u BY u TO u).

NODE_T *top_down_skip_loop_unit (NODE_T * p)
{
// Unit may start with, or consist of, a loop.
  if (is_loop_keyword (p)) {
    p = top_down_loop (p);
  }
// Skip rest of unit.
  while (p != NO_NODE) {
    int k = is_loop_cast_formula (p);
    if (k != 0) {
// operator-cast series ...
      while (p != NO_NODE && k != 0) {
        while (k != 0) {
          FORWARD (p);
          k--;
        }
        k = is_loop_cast_formula (p);
      }
// ... may be followed by a loop clause.
      if (is_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (is_loop_keyword (p) || IS (p, OD_SYMBOL)) {
// new loop or end-of-loop.
      return p;
    } else if (IS (p, COLON_SYMBOL)) {
      FORWARD (p);
// skip routine header: loop clause.
      if (p != NO_NODE && is_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (is_one_of (p, SEMI_SYMBOL, COMMA_SYMBOL, STOP) || IS (p, EXIT_SYMBOL)) {
// Statement separators.
      return p;
    } else {
      FORWARD (p);
    }
  }
  return NO_NODE;
}

//! @brief Skip a loop clause.

NODE_T *top_down_skip_loop_series (NODE_T * p)
{
  BOOL_T siga;
  do {
    p = top_down_skip_loop_unit (p);
    siga = (BOOL_T) (p != NO_NODE && (is_one_of (p, SEMI_SYMBOL, EXIT_SYMBOL, COMMA_SYMBOL, COLON_SYMBOL, STOP)));
    if (siga) {
      FORWARD (p);
    }
  } while (!(p == NO_NODE || !siga));
  return p;
}

//! @brief Make branch of loop parts.

NODE_T *top_down_loop (NODE_T * p)
{
  NODE_T *start = p, *q = p, *save;
  if (IS (q, FOR_SYMBOL)) {
    tokens_exhausted (FORWARD (q), start);
    if (IS (q, IDENTIFIER)) {
      ATTRIBUTE (q) = DEFINING_IDENTIFIER;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, IDENTIFIER);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
    tokens_exhausted (FORWARD (q), start);
    if (is_one_of (q, FROM_SYMBOL, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, STOP)) {
      ;
    } else if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, STOP);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
  }
  if (IS (q, FROM_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_unit (NEXT (q));
    tokens_exhausted (q, start);
    if (is_one_of (q, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, STOP)) {
      ;
    } else if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, STOP);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
    make_sub (start, PREVIOUS (q), FROM_SYMBOL);
  }
  if (IS (q, BY_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (is_one_of (q, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, STOP)) {
      ;
    } else if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, STOP);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
    make_sub (start, PREVIOUS (q), BY_SYMBOL);
  }
  if (is_one_of (q, TO_SYMBOL, DOWNTO_SYMBOL, STOP)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (IS (q, WHILE_SYMBOL)) {
      ;
    } else if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, STOP);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
    make_sub (start, PREVIOUS (q), TO_SYMBOL);
  }
  if (IS (q, WHILE_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, DO_SYMBOL);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
    make_sub (start, PREVIOUS (q), WHILE_SYMBOL);
  }
  if (is_one_of (q, DO_SYMBOL, ALT_DO_SYMBOL, STOP)) {
    int k = ATTRIBUTE (q);
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (!IS (q, OD_SYMBOL)) {
      top_down_diagnose (start, q, LOOP_CLAUSE, OD_SYMBOL);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
    make_sub (start, q, k);
  }
  save = NEXT (start);
  make_sub (p, start, LOOP_CLAUSE);
  return save;
}

//! @brief Driver for making branches of loop parts.

void top_down_loops (NODE_T * p)
{
  NODE_T *q = p;
  for (; q != NO_NODE; FORWARD (q)) {
    if (SUB (q) != NO_NODE) {
      top_down_loops (SUB (q));
    }
  }
  q = p;
  while (q != NO_NODE) {
    if (is_loop_keyword (q) != STOP) {
      q = top_down_loop (q);
    } else {
      FORWARD (q);
    }
  }
}

//! @brief Driver for making branches of until parts.

void top_down_untils (NODE_T * p)
{
  NODE_T *q = p;
  for (; q != NO_NODE; FORWARD (q)) {
    if (SUB (q) != NO_NODE) {
      top_down_untils (SUB (q));
    }
  }
  q = p;
  while (q != NO_NODE) {
    if (IS (q, UNTIL_SYMBOL)) {
      NODE_T *u = q;
      while (NEXT (u) != NO_NODE) {
        FORWARD (u);
      }
      make_sub (q, PREVIOUS (u), UNTIL_SYMBOL);
      return;
    } else {
      FORWARD (q);
    }
  }
}

// Branch anything except parts of a loop.

//! @brief Skip serial/enquiry clause (unit series).

NODE_T *top_down_series (NODE_T * p)
{
  BOOL_T siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    p = top_down_skip_unit (p);
    if (p != NO_NODE) {
      if (is_one_of (p, SEMI_SYMBOL, EXIT_SYMBOL, COMMA_SYMBOL, STOP)) {
        siga = A68_TRUE;
        FORWARD (p);
      }
    }
  }
  return p;
}

//! @brief Make branch of BEGIN .. END.

NODE_T *top_down_begin (NODE_T * begin_p)
{
  NODE_T *end_p = top_down_series (NEXT (begin_p));
  if (end_p == NO_NODE || !IS (end_p, END_SYMBOL)) {
    top_down_diagnose (begin_p, end_p, ENCLOSED_CLAUSE, END_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
    return NO_NODE;
  } else {
    make_sub (begin_p, end_p, BEGIN_SYMBOL);
    return NEXT (begin_p);
  }
}

//! @brief Make branch of CODE .. EDOC.

NODE_T *top_down_code (NODE_T * code_p)
{
  NODE_T *edoc_p = top_down_series (NEXT (code_p));
  if (edoc_p == NO_NODE || !IS (edoc_p, EDOC_SYMBOL)) {
    diagnostic (A68_SYNTAX_ERROR, code_p, ERROR_KEYWORD);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
    return NO_NODE;
  } else {
    make_sub (code_p, edoc_p, CODE_SYMBOL);
    return NEXT (code_p);
  }
}

//! @brief Make branch of ( .. ).

NODE_T *top_down_open (NODE_T * open_p)
{
  NODE_T *then_bar_p = top_down_series (NEXT (open_p)), *elif_bar_p;
  if (then_bar_p != NO_NODE && IS (then_bar_p, CLOSE_SYMBOL)) {
    make_sub (open_p, then_bar_p, OPEN_SYMBOL);
    return NEXT (open_p);
  }
  if (then_bar_p == NO_NODE || !IS (then_bar_p, THEN_BAR_SYMBOL)) {
    top_down_diagnose (open_p, then_bar_p, ENCLOSED_CLAUSE, STOP);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
  }
  make_sub (open_p, PREVIOUS (then_bar_p), OPEN_SYMBOL);
  elif_bar_p = top_down_series (NEXT (then_bar_p));
  if (elif_bar_p != NO_NODE && IS (elif_bar_p, CLOSE_SYMBOL)) {
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (open_p, elif_bar_p, OPEN_SYMBOL);
    return NEXT (open_p);
  }
  if (elif_bar_p != NO_NODE && IS (elif_bar_p, THEN_BAR_SYMBOL)) {
    NODE_T *close_p = top_down_series (NEXT (elif_bar_p));
    if (close_p == NO_NODE || !IS (close_p, CLOSE_SYMBOL)) {
      top_down_diagnose (open_p, elif_bar_p, ENCLOSED_CLAUSE, CLOSE_SYMBOL);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (elif_bar_p, PREVIOUS (close_p), THEN_BAR_SYMBOL);
    make_sub (open_p, close_p, OPEN_SYMBOL);
    return NEXT (open_p);
  }
  if (elif_bar_p != NO_NODE && IS (elif_bar_p, ELSE_BAR_SYMBOL)) {
    NODE_T *close_p = top_down_open (elif_bar_p);
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (open_p, elif_bar_p, OPEN_SYMBOL);
    return close_p;
  } else {
    top_down_diagnose (open_p, elif_bar_p, ENCLOSED_CLAUSE, CLOSE_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
    return NO_NODE;
  }
}

//! @brief Make branch of [ .. ].

NODE_T *top_down_sub (NODE_T * sub_p)
{
  NODE_T *bus_p = top_down_series (NEXT (sub_p));
  if (bus_p != NO_NODE && IS (bus_p, BUS_SYMBOL)) {
    make_sub (sub_p, bus_p, SUB_SYMBOL);
    return NEXT (sub_p);
  } else {
    top_down_diagnose (sub_p, bus_p, 0, BUS_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
    return NO_NODE;
  }
}

//! @brief Make branch of { .. }.

NODE_T *top_down_acco (NODE_T * acco_p)
{
  NODE_T *occa_p = top_down_series (NEXT (acco_p));
  if (occa_p != NO_NODE && IS (occa_p, OCCA_SYMBOL)) {
    make_sub (acco_p, occa_p, ACCO_SYMBOL);
    return NEXT (acco_p);
  } else {
    top_down_diagnose (acco_p, occa_p, ENCLOSED_CLAUSE, OCCA_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
    return NO_NODE;
  }
}

//! @brief Make branch of IF .. THEN .. ELSE .. FI.

NODE_T *top_down_if (NODE_T * if_p)
{
  NODE_T *then_p = top_down_series (NEXT (if_p)), *elif_p;
  if (then_p == NO_NODE || !IS (then_p, THEN_SYMBOL)) {
    top_down_diagnose (if_p, then_p, CONDITIONAL_CLAUSE, THEN_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
  }
  make_sub (if_p, PREVIOUS (then_p), IF_SYMBOL);
  elif_p = top_down_series (NEXT (then_p));
  if (elif_p != NO_NODE && IS (elif_p, FI_SYMBOL)) {
    make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
    make_sub (if_p, elif_p, IF_SYMBOL);
    return NEXT (if_p);
  }
  if (elif_p != NO_NODE && IS (elif_p, ELSE_SYMBOL)) {
    NODE_T *fi_p = top_down_series (NEXT (elif_p));
    if (fi_p == NO_NODE || !IS (fi_p, FI_SYMBOL)) {
      top_down_diagnose (if_p, fi_p, CONDITIONAL_CLAUSE, FI_SYMBOL);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    } else {
      make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
      make_sub (elif_p, PREVIOUS (fi_p), ELSE_SYMBOL);
      make_sub (if_p, fi_p, IF_SYMBOL);
      return NEXT (if_p);
    }
  }
  if (elif_p != NO_NODE && IS (elif_p, ELIF_SYMBOL)) {
    NODE_T *fi_p = top_down_if (elif_p);
    make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
    make_sub (if_p, elif_p, IF_SYMBOL);
    return fi_p;
  } else {
    top_down_diagnose (if_p, elif_p, CONDITIONAL_CLAUSE, FI_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
    return NO_NODE;
  }
}

//! @brief Make branch of CASE .. IN .. OUT .. ESAC.

NODE_T *top_down_case (NODE_T * case_p)
{
  NODE_T *in_p = top_down_series (NEXT (case_p)), *ouse_p;
  if (in_p == NO_NODE || !IS (in_p, IN_SYMBOL)) {
    top_down_diagnose (case_p, in_p, ENCLOSED_CLAUSE, IN_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
  }
  make_sub (case_p, PREVIOUS (in_p), CASE_SYMBOL);
  ouse_p = top_down_series (NEXT (in_p));
  if (ouse_p != NO_NODE && IS (ouse_p, ESAC_SYMBOL)) {
    make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
    make_sub (case_p, ouse_p, CASE_SYMBOL);
    return NEXT (case_p);
  }
  if (ouse_p != NO_NODE && IS (ouse_p, OUT_SYMBOL)) {
    NODE_T *esac_p = top_down_series (NEXT (ouse_p));
    if (esac_p == NO_NODE || !IS (esac_p, ESAC_SYMBOL)) {
      top_down_diagnose (case_p, esac_p, ENCLOSED_CLAUSE, ESAC_SYMBOL);
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    } else {
      make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
      make_sub (ouse_p, PREVIOUS (esac_p), OUT_SYMBOL);
      make_sub (case_p, esac_p, CASE_SYMBOL);
      return NEXT (case_p);
    }
  }
  if (ouse_p != NO_NODE && IS (ouse_p, OUSE_SYMBOL)) {
    NODE_T *esac_p = top_down_case (ouse_p);
    make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
    make_sub (case_p, ouse_p, CASE_SYMBOL);
    return esac_p;
  } else {
    top_down_diagnose (case_p, ouse_p, ENCLOSED_CLAUSE, ESAC_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
    return NO_NODE;
  }
}

//! @brief Skip a unit.

NODE_T *top_down_skip_unit (NODE_T * p)
{
  while (p != NO_NODE && !is_unit_terminator (p)) {
    if (IS (p, BEGIN_SYMBOL)) {
      p = top_down_begin (p);
    } else if (IS (p, SUB_SYMBOL)) {
      p = top_down_sub (p);
    } else if (IS (p, OPEN_SYMBOL)) {
      p = top_down_open (p);
    } else if (IS (p, IF_SYMBOL)) {
      p = top_down_if (p);
    } else if (IS (p, CASE_SYMBOL)) {
      p = top_down_case (p);
    } else if (IS (p, CODE_SYMBOL)) {
      p = top_down_code (p);
    } else if (IS (p, ACCO_SYMBOL)) {
      p = top_down_acco (p);
    } else {
      FORWARD (p);
    }
  }
  return p;
}

NODE_T *top_down_skip_format (NODE_T *);

//! @brief Make branch of ( .. ) in a format.

NODE_T *top_down_format_open (NODE_T * open_p)
{
  NODE_T *close_p = top_down_skip_format (NEXT (open_p));
  if (close_p != NO_NODE && IS (close_p, FORMAT_CLOSE_SYMBOL)) {
    make_sub (open_p, close_p, FORMAT_OPEN_SYMBOL);
    return NEXT (open_p);
  } else {
    top_down_diagnose (open_p, close_p, 0, FORMAT_CLOSE_SYMBOL);
    longjmp (A68_PARSER (top_down_crash_exit), 1);
    return NO_NODE;
  }
}

//! @brief Skip a format text.

NODE_T *top_down_skip_format (NODE_T * p)
{
  while (p != NO_NODE) {
    if (IS (p, FORMAT_OPEN_SYMBOL)) {
      p = top_down_format_open (p);
    } else if (is_one_of (p, FORMAT_CLOSE_SYMBOL, FORMAT_DELIMITER_SYMBOL, STOP)) {
      return p;
    } else {
      FORWARD (p);
    }
  }
  return NO_NODE;
}

//! @brief Make branch of $ .. $.

void top_down_formats (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (SUB (q) != NO_NODE) {
      top_down_formats (SUB (q));
    }
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, FORMAT_DELIMITER_SYMBOL)) {
      NODE_T *f = NEXT (q);
      while (f != NO_NODE && !IS (f, FORMAT_DELIMITER_SYMBOL)) {
        if (IS (f, FORMAT_OPEN_SYMBOL)) {
          f = top_down_format_open (f);
        } else {
          f = NEXT (f);
        }
      }
      if (f == NO_NODE) {
        top_down_diagnose (p, f, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL);
        longjmp (A68_PARSER (top_down_crash_exit), 1);
      } else {
        make_sub (q, f, FORMAT_DELIMITER_SYMBOL);
      }
    }
  }
}

//! @brief Make branches of phrases for the bottom-up parser.

void top_down_parser (NODE_T * p)
{
  if (p != NO_NODE) {
    if (!setjmp (A68_PARSER (top_down_crash_exit))) {
      (void) top_down_series (p);
      top_down_loops (p);
      top_down_untils (p);
      top_down_formats (p);
    }
  }
}
