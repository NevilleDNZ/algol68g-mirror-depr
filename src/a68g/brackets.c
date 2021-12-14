//! @file brackets.c
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

#include "a68g.h"
#include "a68g-parser.h"

// Recursive-descent parenthesis checker.
//
// After this checker, we know that at least brackets are matched. 
// This stabilises later parser phases.
// Top-down parsing is done to place error diagnostics near offending lines.

//! @brief Intelligible diagnostics for the bracket checker.

void bracket_check_error (char *txt, int n, char *bra, char *ket)
{
  if (n != 0) {
    char b[BUFFER_SIZE];
    ASSERT (snprintf (b, SNPRINTF_SIZE, "\"%s\" without matching \"%s\"", (n > 0 ? bra : ket), (n > 0 ? ket : bra)) >= 0);
    if (strlen (txt) > 0) {
      bufcat (txt, " and ", BUFFER_SIZE);
    }
    bufcat (txt, b, BUFFER_SIZE);
  }
}

//! @brief Diagnose brackets in local branch of the tree.

char *bracket_check_diagnose (NODE_T * p)
{
  int begins = 0, opens = 0, format_delims = 0, format_opens = 0, subs = 0, ifs = 0, cases = 0, dos = 0, accos = 0;
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case BEGIN_SYMBOL:
      {
        begins++;
        break;
      }
    case END_SYMBOL:
      {
        begins--;
        break;
      }
    case OPEN_SYMBOL:
      {
        opens++;
        break;
      }
    case CLOSE_SYMBOL:
      {
        opens--;
        break;
      }
    case ACCO_SYMBOL:
      {
        accos++;
        break;
      }
    case OCCA_SYMBOL:
      {
        accos--;
        break;
      }
    case FORMAT_DELIMITER_SYMBOL:
      {
        if (format_delims == 0) {
          format_delims = 1;
        } else {
          format_delims = 0;
        }
        break;
      }
    case FORMAT_OPEN_SYMBOL:
      {
        format_opens++;
        break;
      }
    case FORMAT_CLOSE_SYMBOL:
      {
        format_opens--;
        break;
      }
    case SUB_SYMBOL:
      {
        subs++;
        break;
      }
    case BUS_SYMBOL:
      {
        subs--;
        break;
      }
    case IF_SYMBOL:
      {
        ifs++;
        break;
      }
    case FI_SYMBOL:
      {
        ifs--;
        break;
      }
    case CASE_SYMBOL:
      {
        cases++;
        break;
      }
    case ESAC_SYMBOL:
      {
        cases--;
        break;
      }
    case DO_SYMBOL:
      {
        dos++;
        break;
      }
    case OD_SYMBOL:
      {
        dos--;
        break;
      }
    }
  }
  A68 (edit_line)[0] = NULL_CHAR;
  bracket_check_error (A68 (edit_line), begins, "BEGIN", "END");
  bracket_check_error (A68 (edit_line), opens, "(", ")");
  bracket_check_error (A68 (edit_line), format_opens, "(", ")");
  bracket_check_error (A68 (edit_line), format_delims, "$", "$");
  bracket_check_error (A68 (edit_line), accos, "{", "}");
  bracket_check_error (A68 (edit_line), subs, "[", "]");
  bracket_check_error (A68 (edit_line), ifs, "IF", "FI");
  bracket_check_error (A68 (edit_line), cases, "CASE", "ESAC");
  bracket_check_error (A68 (edit_line), dos, "DO", "OD");
  return A68 (edit_line);
}

//! @brief Driver for locally diagnosing non-matching tokens.

NODE_T *bracket_check_parse (NODE_T * top, NODE_T * p)
{
  BOOL_T ignore_token;
  for (; p != NO_NODE; FORWARD (p)) {
    int ket = STOP;
    NODE_T *q = NO_NODE;
    ignore_token = A68_FALSE;
    switch (ATTRIBUTE (p)) {
    case BEGIN_SYMBOL:
      {
        ket = END_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case OPEN_SYMBOL:
      {
        ket = CLOSE_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case ACCO_SYMBOL:
      {
        ket = OCCA_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case FORMAT_OPEN_SYMBOL:
      {
        ket = FORMAT_CLOSE_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case SUB_SYMBOL:
      {
        ket = BUS_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case IF_SYMBOL:
      {
        ket = FI_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case CASE_SYMBOL:
      {
        ket = ESAC_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case DO_SYMBOL:
      {
        ket = OD_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case END_SYMBOL:
    case OCCA_SYMBOL:
    case CLOSE_SYMBOL:
    case FORMAT_CLOSE_SYMBOL:
    case BUS_SYMBOL:
    case FI_SYMBOL:
    case ESAC_SYMBOL:
    case OD_SYMBOL:
      {
        return p;
      }
    default:
      {
        ignore_token = A68_TRUE;
      }
    }
    if (ignore_token) {
      ;
    } else if (q != NO_NODE && IS (q, ket)) {
      p = q;
    } else if (q == NO_NODE) {
      char *diag = bracket_check_diagnose (top);
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_PARENTHESIS, (strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS));
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    } else {
      char *diag = bracket_check_diagnose (top);
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_PARENTHESIS_2, ATTRIBUTE (q), LINE (INFO (q)), ket, (strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS));
      longjmp (A68_PARSER (top_down_crash_exit), 1);
    }
  }
  return NO_NODE;
}

//! @brief Driver for globally diagnosing non-matching tokens.

void check_parenthesis (NODE_T * top)
{
  if (!setjmp (A68_PARSER (top_down_crash_exit))) {
    if (bracket_check_parse (top, top) != NO_NODE) {
      diagnostic (A68_SYNTAX_ERROR, top, ERROR_PARENTHESIS, INFO_MISSING_KEYWORDS);
    }
  }
}
