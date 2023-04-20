//! @file plugin-inline.c
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
//! Plugin compiler inlining routines.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-genie.h"
#include "a68g-listing.h"
#include "a68g-mp.h"
#include "a68g-optimiser.h"
#include "a68g-plugin.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

//! @brief Code an A68 mode.

char *inline_mode (MOID_T * m)
{
  if (m == M_INT) {
    return "A68_INT";
  } else if (m == M_REAL) {
    return "A68_REAL";
  } else if (m == M_BOOL) {
    return "A68_BOOL";
  } else if (m == M_CHAR) {
    return "A68_CHAR";
  } else if (m == M_BITS) {
    return "A68_BITS";
  } else if (m == M_COMPLEX) {
    return "A68_COMPLEX";
  } else if (IS (m, REF_SYMBOL)) {
    return "A68_REF";
  } else if (IS (m, ROW_SYMBOL)) {
    return "A68_ROW";
  } else if (IS (m, PROC_SYMBOL)) {
    return "A68_PROCEDURE";
  } else if (IS (m, STRUCT_SYMBOL)) {
    return "A68_STRUCT";
  } else {
    return "A68_ERROR";
  }
}

//! @brief Compile inline arguments.

void inline_arguments (NODE_T * p, FILE_T out, int phase, int *size)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT) && phase == L_PUSH) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "EXECUTE_UNIT_TRACE (_NODE_ (%d));\n", NUMBER (p)));
    inline_arguments (NEXT (p), out, L_PUSH, size);
  } else if (IS (p, UNIT)) {
    char arg[NAME_SIZE];
    (void) make_name (arg, ARG, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (MOID (p)), 1, arg);
      inline_unit (p, out, L_DECLARE);
    } else if (phase == L_INITIALISE) {
      inline_unit (p, out, L_EXECUTE);
    } else if (phase == L_EXECUTE) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) FRAME_OBJECT (%d);\n", arg, inline_mode (MOID (p)), *size));
      (*size) += SIZE (MOID (p));
    } else if (phase == L_YIELD && primitive_mode (MOID (p))) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_STATUS_ (%s) = INIT_MASK;\n", arg));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s) = ", arg));
      inline_unit (p, out, L_YIELD);
      undent (out, ";\n");
    } else if (phase == L_YIELD && basic_mode (MOID (p))) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MOVE ((void *) %s, (void *) ", arg));
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", %d);\n", SIZE (MOID (p))));
    }
  } else {
    inline_arguments (SUB (p), out, phase, size);
    inline_arguments (NEXT (p), out, phase, size);
  }
}

//! @brief Code denotation.

void inline_denotation (NODE_T * p, FILE_T out, int phase)
{
  if (phase == L_YIELD) {
    if (MOID (p) == M_INT) {
      A68_INT z;
      NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
      char *den = NSYMBOL (s);
      if (genie_string_to_value_internal (p, M_INT, den, (BYTE_T *) & z) == A68_FALSE) {
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, M_INT);
      }
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, A68_LD, VALUE (&z)));
    } else if (MOID (p) == M_REAL) {
      A68_REAL z;
      NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
      char *den = NSYMBOL (s);
      if (genie_string_to_value_internal (p, M_REAL, den, (BYTE_T *) & z) == A68_FALSE) {
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, M_REAL);
      }
      if (strchr (den, '.') == NO_TEXT && strchr (den, 'e') == NO_TEXT && strchr (den, 'E') == NO_TEXT) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(REAL_T) %s", den));
      } else {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", den));
      }
    } else if (MOID (p) == M_BOOL) {
      undent (out, "(BOOL_T) A68_");
      undent (out, NSYMBOL (p));
    } else if (MOID (p) == M_CHAR) {
      if (NSYMBOL (p)[0] == '\'') {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'\\''"));
      } else if (NSYMBOL (p)[0] == NULL_CHAR) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "NULL_CHAR"));
      } else if (NSYMBOL (p)[0] == '\\') {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'\\\\'"));
      } else {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'%c'", (NSYMBOL (p))[0]));
      }
    } else if (MOID (p) == M_BITS) {
      A68_BITS z;
      NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
      if (genie_string_to_value_internal (p, M_BITS, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, M_BITS);
      }
      ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "(UNSIGNED_T) 0x" A68_LX, VALUE (&z)) >= 0);
      undent (out, A68 (edit_line));
    }
  }
}

//! @brief Code widening.

void inline_widening (NODE_T * p, FILE_T out, int phase)
{
  if (WIDEN_TO (p, INT, REAL)) {
    if (phase == L_DECLARE) {
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      undent (out, "(REAL_T) (");
      inline_unit (SUB (p), out, L_YIELD);
      undent (out, ")");
    }
  } else if (WIDEN_TO (p, REAL, COMPLEX)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (M_COMPLEX), 0, acc);
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "STATUS_RE (%s) = INIT_MASK;\n", acc));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "STATUS_IM (%s) = INIT_MASK;\n", acc));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "RE (%s) = (REAL_T) (", acc));
      inline_unit (SUB (p), out, L_YIELD);
      undent (out, ");\n");
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "IM (%s) = 0.0;\n", acc));
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(A68_REAL *) %s", acc));
    }
  }
}

//! @brief Code dereferencing of identifier.

void inline_dereference_identifier (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *q = stems_from (SUB (p), IDENTIFIER);
  ABEND (q == NO_NODE, ERROR_INTERNAL_CONSISTENCY, __func__);
  if (phase == L_DECLARE) {
    if (signed_in (BOOK_DEREF, L_DECLARE, NSYMBOL (q)) != NO_BOOK) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (p));
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (MOID (p)), 1, idf);
      sign_in (BOOK_DEREF, L_DECLARE, NSYMBOL (p), NULL, NUMBER (p));
      inline_unit (SUB (p), out, L_DECLARE);
    }
  } else if (phase == L_EXECUTE) {
    if (signed_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q)) != NO_BOOK) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (p));
      inline_unit (SUB (p), out, L_EXECUTE);
      if (BODY (TAX (q)) != NO_TAG) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) LOCAL_ADDRESS (", idf, inline_mode (MOID (p))));
        sign_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (p), NULL, NUMBER (p));
        inline_unit (SUB (p), out, L_YIELD);
        undent (out, ");\n");
      } else {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = DEREF (%s, ", idf, inline_mode (MOID (p))));
        sign_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (p), NULL, NUMBER (p));
        inline_unit (SUB (p), out, L_YIELD);
        undent (out, ");\n");
      }
      gen_check_init (p, out, idf);
    }
  } else if (phase == L_YIELD) {
    char idf[NAME_SIZE];
    if (signed_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q)) != NO_BOOK) {
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (signed_in (BOOK_DEREF, L_DECLARE, NSYMBOL (q))));
    } else {
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (p));
    }
    if (primitive_mode (MOID (p))) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s)", idf));
    } else if (MOID (p) == M_COMPLEX) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(A68_REAL *) (%s)", idf));
    } else if (basic_mode (MOID (p))) {
      undent (out, idf);
    }
  }
}

//! @brief Code identifier.

void inline_identifier (NODE_T * p, FILE_T out, int phase)
{
// Possible constant folding.
  NODE_T *def = NODE (TAX (p));
  if (primitive_mode (MOID (p)) && def != NO_NODE && NEXT (def) != NO_NODE && IS (NEXT (def), EQUALS_SYMBOL)) {
    NODE_T *src = stems_from (NEXT_NEXT (def), DENOTATION);
    if (src != NO_NODE) {
      inline_denotation (src, out, phase);
      return;
    }
  }
// No folding - consider identifier.
  if (phase == L_DECLARE) {
    if (signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (p)) != NO_BOOK) {
      return;
    } else if (A68_STANDENV_PROC (TAX (p))) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (MOID (p)), 1, idf);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (p), NULL, NUMBER (p));
    }
  } else if (phase == L_EXECUTE) {
    if (signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p)) != NO_BOOK) {
      return;
    } else if (A68_STANDENV_PROC (TAX (p))) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      get_stack (p, out, idf, inline_mode (MOID (p)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), NULL, NUMBER (p));
      gen_check_init (p, out, idf);
    }
  } else if (phase == L_YIELD) {
    if (A68_STANDENV_PROC (TAX (p))) {
      int k;
      for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
          undent (out, CODE (&constants[k]));
          return;
        }
      }
    } else {
      char idf[NAME_SIZE];
      BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p));
      if (entry != NO_BOOK) {
        (void) make_name (idf, NSYMBOL (p), "", NUMBER (entry));
      } else {
        (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      }
      if (primitive_mode (MOID (p))) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s)", idf));
      } else if (MOID (p) == M_COMPLEX) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(A68_REAL *) (%s)", idf));
      } else if (basic_mode (MOID (p))) {
        undent (out, idf);
      }
    }
  }
}

//! @brief Code indexer.

void inline_indexer (NODE_T * p, FILE_T out, int phase, INT_T * k, char *tup)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    if (phase != L_YIELD) {
      inline_unit (p, out, phase);
    } else {
      if ((*k) == 0) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(SPAN (&%s[" A68_LD "]) * (", tup, (*k)));
      } else {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, " + (SPAN (&%s[" A68_LD "]) * (", tup, (*k)));
      }
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ") - SHIFT (&%s[" A68_LD "]))", tup, (*k)));
    }
    (*k)++;
  } else {
    inline_indexer (SUB (p), out, phase, k, tup);
    inline_indexer (NEXT (p), out, phase, k, tup);
  }
}

//! @brief Code dereferencing of slice.

void inline_dereference_slice (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *prim = SUB (p);
  NODE_T *indx = NEXT (prim);
  MOID_T *row_mode = DEFLEX (MOID (prim));
  MOID_T *mode = SUB_SUB (row_mode);
  char *symbol = NSYMBOL (SUB (prim));
  char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE];
  INT_T k;
  if (phase == L_DECLARE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 1, idf);
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 0, elm);
      (void) add_declaration (&A68_OPT (root_idf), "A68_ARRAY", 1, arr);
      (void) add_declaration (&A68_OPT (root_idf), "A68_TUPLE", 1, tup);
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (mode), 1, drf);
      sign_in (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 0, elm);
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (mode), 1, drf);
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NO_TEXT);
  } else if (phase == L_EXECUTE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    NODE_T *pidf = stems_from (prim, IDENTIFIER);
    if (entry == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      get_stack (pidf, out, idf, "A68_REF");
      if (IS (row_mode, REF_SYMBOL) && IS (SUB (row_mode), ROW_SYMBOL)) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, DEREF (A68_ROW, %s));\n", arr, tup, idf));
      } else {
        ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
      }
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (arr, ARR, "", NUMBER (entry));
      (void) make_name (tup, TUP, "", NUMBER (entry));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = ARRAY (%s);\n", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NO_TEXT);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ");\n"));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = DEREF (%s, & %s);\n", drf, inline_mode (mode), elm));
  } else if (phase == L_YIELD) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NO_BOOK && same_tree (indx, (NODE_T *) (INFO (entry))) == A68_TRUE) {
      (void) make_name (drf, DRF, "", NUMBER (entry));
    } else {
      (void) make_name (drf, DRF, "", NUMBER (prim));
    }
    if (primitive_mode (mode)) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s)", drf));
    } else if (mode == M_COMPLEX) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(A68_REAL *) (%s)", drf));
    } else if (basic_mode (mode)) {
      undent (out, drf);
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  }
}

//! @brief Code slice REF [] MODE -> REF MODE.

void inline_slice_ref_to_ref (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *prim = SUB (p);
  NODE_T *indx = NEXT (prim);
  MOID_T *mode = SUB_MOID (p);
  MOID_T *row_mode = DEFLEX (MOID (prim));
  char *symbol = NSYMBOL (SUB (prim));
  char idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE], drf[NAME_SIZE];
  INT_T k;
  if (phase == L_DECLARE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 1, idf);
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 0, elm);
      (void) add_declaration (&A68_OPT (root_idf), "A68_ARRAY", 1, arr);
      (void) add_declaration (&A68_OPT (root_idf), "A68_TUPLE", 1, tup);
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (mode), 1, drf);
      sign_in (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 0, elm);
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (mode), 1, drf);
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NO_TEXT);
  } else if (phase == L_EXECUTE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry == NO_BOOK) {
      NODE_T *pidf = stems_from (prim, IDENTIFIER);
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      get_stack (pidf, out, idf, "A68_REF");
      if (IS (row_mode, REF_SYMBOL) && IS (SUB (row_mode), ROW_SYMBOL)) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, DEREF (A68_ROW, %s));\n", arr, tup, idf));
      } else {
        ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
      }
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (arr, ARR, "", NUMBER (entry));
      (void) make_name (tup, TUP, "", NUMBER (entry));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = ARRAY (%s);\n", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NO_TEXT);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ");\n"));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = DEREF (%s, & %s);\n", drf, inline_mode (mode), elm));
  } else if (phase == L_YIELD) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NO_BOOK && same_tree (indx, (NODE_T *) (INFO (entry))) == A68_TRUE) {
      (void) make_name (elm, ELM, "", NUMBER (entry));
    } else {
      (void) make_name (elm, ELM, "", NUMBER (prim));
    }
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(&%s)", elm));
  }
}

//! @brief Code slice [] MODE -> MODE.

void inline_slice (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *prim = SUB (p);
  NODE_T *indx = NEXT (prim);
  MOID_T *mode = MOID (p);
  MOID_T *row_mode = DEFLEX (MOID (prim));
  char *symbol = NSYMBOL (SUB (prim));
  char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE];
  INT_T k;
  if (phase == L_DECLARE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, inline_mode (mode), drf, arr, tup));
      sign_in (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_REF %s; %s * %s;\n", elm, inline_mode (mode), drf));
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NO_TEXT);
  } else if (phase == L_EXECUTE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry == NO_BOOK) {
      NODE_T *pidf = stems_from (prim, IDENTIFIER);
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      get_stack (pidf, out, idf, "A68_REF");
      if (IS (row_mode, REF_SYMBOL)) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, DEREF (A68_ROW, %s));\n", arr, tup, idf));
      } else {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, (A68_ROW *) %s);\n", arr, tup, idf));
      }
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (arr, ARR, "", NUMBER (entry));
      (void) make_name (tup, TUP, "", NUMBER (entry));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = ARRAY (%s);\n", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NO_TEXT);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ");\n"));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = DEREF (%s, & %s);\n", drf, inline_mode (mode), elm));
  } else if (phase == L_YIELD) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NO_BOOK && same_tree (indx, (NODE_T *) (INFO (entry))) == A68_TRUE) {
      (void) make_name (drf, DRF, "", NUMBER (entry));
    } else {
      (void) make_name (drf, DRF, "", NUMBER (prim));
    }
    if (primitive_mode (mode)) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s)", drf));
    } else if (mode == M_COMPLEX) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(A68_REAL *) (%s)", drf));
    } else if (basic_mode (mode)) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", drf));
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  }
}

//! @brief Code monadic formula.

void inline_monadic_formula (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *op = SUB (p);
  NODE_T *rhs = NEXT (op);
  if (IS (p, MONADIC_FORMULA) && MOID (p) == M_COMPLEX) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (M_COMPLEX), 0, acc);
      inline_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_unit (rhs, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
          indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s (%s, ", CODE (&monadics[k]), acc));
          inline_unit (rhs, out, L_YIELD);
          undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", acc));
    }
  } else if (IS (p, MONADIC_FORMULA) && basic_mode (MOID (p))) {
    if (phase != L_YIELD) {
      inline_unit (rhs, out, phase);
    } else {
      int k;
      for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
          if (IS_ALNUM ((CODE (&monadics[k]))[0])) {
            undent (out, CODE (&monadics[k]));
            undent (out, "(");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          } else {
            undent (out, CODE (&monadics[k]));
            undent (out, "(");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          }
        }
      }
    }
  }
}

//! @brief Code dyadic formula.

void inline_formula (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *lhs = SUB (p), *rhs;
  NODE_T *op = NEXT (lhs);
  if (IS (p, FORMULA) && op == NO_NODE) {
    inline_monadic_formula (lhs, out, phase);
    return;
  }
  rhs = NEXT (op);
  if (IS (p, FORMULA) && MOID (p) == M_COMPLEX) {
    if (op == NO_NODE) {
      inline_monadic_formula (lhs, out, phase);
    } else if (phase == L_DECLARE) {
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (MOID (p)), 0, acc);
      inline_unit (lhs, out, L_DECLARE);
      inline_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      char acc[NAME_SIZE];
      int k;
      (void) make_name (acc, TMP, "", NUMBER (p));
      inline_unit (lhs, out, L_EXECUTE);
      inline_unit (rhs, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          if (MOID (p) == M_COMPLEX) {
            indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s (%s, ", CODE (&dyadics[k]), acc));
          } else {
            indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s (& %s, ", CODE (&dyadics[k]), acc));
          }
          inline_unit (lhs, out, L_YIELD);
          undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", "));
          inline_unit (rhs, out, L_YIELD);
          undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      if (MOID (p) == M_COMPLEX) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", acc));
      } else {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (& %s)", acc));
      }
    }
  } else if (IS (p, FORMULA) && basic_mode (MOID (p))) {
    if (phase != L_YIELD) {
      inline_unit (lhs, out, phase);
      inline_unit (rhs, out, phase);
    } else {
      int k;
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          if (IS_ALNUM ((CODE (&dyadics[k]))[0])) {
            undent (out, CODE (&dyadics[k]));
            undent (out, "(");
            inline_unit (lhs, out, L_YIELD);
            undent (out, ", ");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          } else {
            undent (out, "(");
            inline_unit (lhs, out, L_YIELD);
            undent (out, " ");
            undent (out, CODE (&dyadics[k]));
            undent (out, " ");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          }
        }
      }
    }
  }
}

//! @brief Code argument.

void inline_single_argument (NODE_T * p, FILE_T out, int phase)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ARGUMENT_LIST) || IS (p, ARGUMENT)) {
      inline_single_argument (SUB (p), out, phase);
    } else if (IS (p, GENERIC_ARGUMENT_LIST) || IS (p, GENERIC_ARGUMENT)) {
      inline_single_argument (SUB (p), out, phase);
    } else if (IS (p, UNIT)) {
      inline_unit (p, out, phase);
    }
  }
}

//! @brief Code call.

void inline_call (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *prim = SUB (p);
  NODE_T *args = NEXT (prim);
  NODE_T *idf = stems_from (prim, IDENTIFIER);
  if (MOID (p) == M_COMPLEX) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (M_COMPLEX), 0, acc);
      inline_single_argument (args, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_single_argument (args, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s (%s, ", CODE (&functions[k]), acc));
          inline_single_argument (args, out, L_YIELD);
          undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", acc));
    }
  } else if (basic_mode (MOID (p))) {
    if (phase != L_YIELD) {
      inline_single_argument (args, out, phase);
    } else {
      int k;
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          undent (out, CODE (&functions[k]));
          undent (out, " (");
          inline_single_argument (args, out, L_YIELD);
          undent (out, ")");
        }
      }
    }
  }
}

//! @brief Code collateral units.

void inline_collateral_units (NODE_T * p, FILE_T out, int phase)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    if (phase == L_DECLARE) {
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "PUSH_VALUE (p, "));
      inline_unit (SUB (p), out, L_YIELD);
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", %s);\n", inline_mode (MOID (p))));
    }
  } else {
    inline_collateral_units (SUB (p), out, phase);
    inline_collateral_units (NEXT (p), out, phase);
  }
}

//! @brief Code collateral units.

void inline_collateral (NODE_T * p, FILE_T out, int phase)
{
  char dsp[NAME_SIZE];
  (void) make_name (dsp, DSP, "", NUMBER (p));
  if (p == NO_NODE) {
    return;
  } else if (phase == L_DECLARE) {
    if (MOID (p) == M_COMPLEX) {
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (M_REAL), 1, dsp);
    } else {
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (MOID (p)), 1, dsp);
    }
    inline_collateral_units (NEXT_SUB (p), out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    if (MOID (p) == M_COMPLEX) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) STACK_TOP;\n", dsp, inline_mode (M_REAL)));
    } else {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) STACK_TOP;\n", dsp, inline_mode (MOID (p))));
    }
    inline_collateral_units (NEXT_SUB (p), out, L_EXECUTE);
    inline_collateral_units (NEXT_SUB (p), out, L_YIELD);
  } else if (phase == L_YIELD) {
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", dsp));
  }
}

//! @brief Code basic closed clause.

void inline_closed (NODE_T * p, FILE_T out, int phase)
{
  if (p == NO_NODE) {
    return;
  } else if (phase != L_YIELD) {
    inline_unit (SUB (NEXT_SUB (p)), out, phase);
  } else {
    undent (out, "(");
    inline_unit (SUB (NEXT_SUB (p)), out, L_YIELD);
    undent (out, ")");
  }
}

//! @brief Code basic closed clause.

void inline_conditional (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *if_part = NO_NODE, *then_part = NO_NODE, *else_part = NO_NODE;
  p = SUB (p);
  if (IS (p, IF_PART) || IS (p, OPEN_PART)) {
    if_part = p;
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
  }
  FORWARD (p);
  if (IS (p, THEN_PART) || IS (p, CHOICE)) {
    then_part = p;
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
  }
  FORWARD (p);
  if (IS (p, ELSE_PART) || IS (p, CHOICE)) {
    else_part = p;
  } else {
    else_part = NO_NODE;
  }
  if (phase == L_DECLARE) {
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_DECLARE);
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_DECLARE);
    inline_unit (SUB (NEXT_SUB (else_part)), out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_EXECUTE);
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_EXECUTE);
    inline_unit (SUB (NEXT_SUB (else_part)), out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    undent (out, "(");
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_YIELD);
    undent (out, " ? ");
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_YIELD);
    undent (out, " : ");
    if (else_part != NO_NODE) {
      inline_unit (SUB (NEXT_SUB (else_part)), out, L_YIELD);
    } else {
// This is not an ideal solution although RR permits it;
// an omitted else-part means SKIP: yield some value of the
// mode required.
      inline_unit (SUB (NEXT_SUB (then_part)), out, L_YIELD);
    }
    undent (out, ")");
  }
}

//! @brief Code dereferencing of selection.

void inline_dereference_selection (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *field = SUB (p);
  NODE_T *sec = NEXT (field);
  NODE_T *idf = stems_from (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char *field_idf = NSYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 1, ref);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), NULL, NUMBER (field));
    }
    if (entry == NO_BOOK || field_idf != (char *) (INFO (entry))) {
      (void) make_name (sel, SEL, "", NUMBER (field));
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (SUB_MOID (field)), 1, sel);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      get_stack (idf, out, ref, "A68_REF");
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), NULL, NUMBER (field));
    }
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) & (ADDRESS (%s)[" A68_LU "]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else if (field_idf != (char *) (INFO (entry))) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) & (ADDRESS (%s)[" A68_LU "]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry != NO_BOOK && (char *) (INFO (entry)) == field_idf) {
      (void) make_name (sel, SEL, "", NUMBER (entry));
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (SUB_MOID (p))) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s)", sel));
    } else if (SUB_MOID (p) == M_COMPLEX) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(A68_REAL *) (%s)", sel));
    } else if (basic_mode (SUB_MOID (p))) {
      undent (out, sel);
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  }
}

//! @brief Code selection.

void inline_selection (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *field = SUB (p);
  NODE_T *sec = NEXT (field);
  NODE_T *idf = stems_from (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char *field_idf = NSYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) add_declaration (&A68_OPT (root_idf), "A68_STRUCT", 0, ref);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), NULL, NUMBER (field));
    }
    if (entry == NO_BOOK || field_idf != (char *) (INFO (entry))) {
      (void) make_name (sel, SEL, "", NUMBER (field));
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (MOID (field)), 1, sel);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      get_stack (idf, out, ref, "BYTE_T");
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) & (%s[" A68_LU "]);\n", sel, inline_mode (MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else if (field_idf != (char *) (INFO (entry))) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) & (%s[" A68_LU "]);\n", sel, inline_mode (MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry != NO_BOOK && (char *) (INFO (entry)) == field_idf) {
      (void) make_name (sel, SEL, "", NUMBER (entry));
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (MOID (p))) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s)", sel));
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  }
}

//! @brief Code selection.

void inline_selection_ref_to_ref (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *field = SUB (p);
  NODE_T *sec = NEXT (field);
  NODE_T *idf = stems_from (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char *field_idf = NSYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 1, ref);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), NULL, NUMBER (field));
    }
    if (entry == NO_BOOK || field_idf != (char *) (INFO (entry))) {
      (void) make_name (sel, SEL, "", NUMBER (field));
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 0, sel);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE_2, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      get_stack (idf, out, ref, "A68_REF");
      (void) make_name (sel, SEL, "", NUMBER (field));
      sign_in (BOOK_DECL, L_EXECUTE_2, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else if (field_idf != (char *) (INFO (entry))) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      sign_in (BOOK_DECL, L_EXECUTE_2, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = *%s;\n", sel, ref));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OFFSET (&%s) += " A68_LU ";\n", sel, OFFSET_OFF (field)));
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry != NO_BOOK && (char *) (INFO (entry)) == field_idf) {
      (void) make_name (sel, SEL, "", NUMBER (entry));
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (SUB_MOID (p))) {
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(&%s)", sel));
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  }
}

//! @brief Code identifier.

void inline_ref_identifier (NODE_T * p, FILE_T out, int phase)
{
// No folding - consider identifier.
  if (phase == L_DECLARE) {
    if (signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (p)) != NO_BOOK) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      (void) add_declaration (&A68_OPT (root_idf), "A68_REF", 1, idf);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (p), NULL, NUMBER (p));
    }
  } else if (phase == L_EXECUTE) {
    if (signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p)) != NO_BOOK) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      get_stack (p, out, idf, "A68_REF");
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), NULL, NUMBER (p));
    }
  } else if (phase == L_YIELD) {
    char idf[NAME_SIZE];
    BOOK_T *entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p));
    if (entry != NO_BOOK) {
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (entry));
    } else {
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
    }
    undent (out, idf);
  }
}

//! @brief Code identity-relation.

void inline_identity_relation (NODE_T * p, FILE_T out, int phase)
{
#define GOOD(p) (stems_from (p, IDENTIFIER) != NO_NODE && IS (MOID (stems_from ((p), IDENTIFIER)), REF_SYMBOL))
  NODE_T *lhs = SUB (p);
  NODE_T *op = NEXT (lhs);
  NODE_T *rhs = NEXT (op);
  if (GOOD (lhs) && GOOD (rhs)) {
    if (phase == L_DECLARE) {
      NODE_T *lidf = stems_from (lhs, IDENTIFIER);
      NODE_T *ridf = stems_from (rhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_DECLARE);
      inline_ref_identifier (ridf, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T *lidf = stems_from (lhs, IDENTIFIER);
      NODE_T *ridf = stems_from (rhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_EXECUTE);
      inline_ref_identifier (ridf, out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      NODE_T *lidf = stems_from (lhs, IDENTIFIER);
      NODE_T *ridf = stems_from (rhs, IDENTIFIER);
      if (IS (op, IS_SYMBOL)) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "ADDRESS ("));
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ") == ADDRESS ("));
        inline_ref_identifier (ridf, out, L_YIELD);
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ")"));
      } else {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "ADDRESS ("));
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ") != ADDRESS ("));
        inline_ref_identifier (ridf, out, L_YIELD);
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ")"));
      }
    }
  } else if (GOOD (lhs) && stems_from (rhs, NIHIL) != NO_NODE) {
    if (phase == L_DECLARE) {
      NODE_T *lidf = stems_from (lhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T *lidf = stems_from (lhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      NODE_T *lidf = stems_from (lhs, IDENTIFIER);
      if (IS (op, IS_SYMBOL)) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "IS_NIL (*"));
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ")"));
      } else {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "!IS_NIL (*"));
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ")"));
      }
    }
  }
#undef GOOD
}

//! @brief Code unit.

void inline_unit (NODE_T * p, FILE_T out, int phase)
{
  if (p == NO_NODE) {
    return;
  } else if (constant_unit (p) && stems_from (p, DENOTATION) == NO_NODE) {
    constant_folder (p, out, phase);
  } else if (IS (p, UNIT)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, TERTIARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, SECONDARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, PRIMARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, CLOSED_CLAUSE)) {
    inline_closed (p, out, phase);
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    inline_collateral (p, out, phase);
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    inline_conditional (p, out, phase);
  } else if (IS (p, WIDENING)) {
    inline_widening (p, out, phase);
  } else if (IS (p, IDENTIFIER)) {
    inline_identifier (p, out, phase);
  } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), IDENTIFIER) != NO_NODE) {
    inline_dereference_identifier (p, out, phase);
  } else if (IS (p, SLICE)) {
    NODE_T *prim = SUB (p);
    MOID_T *mode = MOID (p);
    MOID_T *row_mode = DEFLEX (MOID (prim));
    if (mode == SUB (row_mode)) {
      inline_slice (p, out, phase);
    } else if (IS (mode, REF_SYMBOL) && IS (row_mode, REF_SYMBOL) && SUB (mode) == SUB_SUB (row_mode)) {
      inline_slice_ref_to_ref (p, out, phase);
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SLICE) != NO_NODE) {
    inline_dereference_slice (SUB (p), out, phase);
  } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SELECTION) != NO_NODE) {
    inline_dereference_selection (SUB (p), out, phase);
  } else if (IS (p, SELECTION)) {
    NODE_T *sec = NEXT_SUB (p);
    MOID_T *mode = MOID (p);
    MOID_T *struct_mode = MOID (sec);
    if (IS (struct_mode, REF_SYMBOL) && IS (mode, REF_SYMBOL)) {
      inline_selection_ref_to_ref (p, out, phase);
    } else if (IS (struct_mode, STRUCT_SYMBOL) && primitive_mode (mode)) {
      inline_selection (p, out, phase);
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  } else if (IS (p, DENOTATION)) {
    inline_denotation (p, out, phase);
  } else if (IS (p, MONADIC_FORMULA)) {
    inline_monadic_formula (p, out, phase);
  } else if (IS (p, FORMULA)) {
    inline_formula (p, out, phase);
  } else if (IS (p, CALL)) {
    inline_call (p, out, phase);
  } else if (IS (p, CAST)) {
    inline_unit (NEXT_SUB (p), out, phase);
  } else if (IS (p, IDENTITY_RELATION)) {
    inline_identity_relation (p, out, phase);
  }
}

