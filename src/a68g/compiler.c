//! @file compiler.c
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

// This file generates optimised C routines for many units in an Algol 68 source
// program. A68G 1.x contained some general optimised routines. These are
// decommissioned in A68G 2.x that dynamically generates routines depending
// on the source code. The generated routines are compiled on the fly into a 
// dynamic library that is linked by the running interpreter.

// To invoke this code generator specify option --optimise.
// Currently the optimiser only considers units that operate on basic modes that are
// contained in a single C struct, for instance primitive modes
// 
//   INT, REAL, BOOL, CHAR and BITS
// 
// and simple structures of these basic modes, such as
// 
//   COMPLEX
// 
// and also (single) references, rows and procedures
// 
//   REF MODE, [] MODE, PROC PARAMSETY MODE
// 
// The code generator employs a few simple optimisations like constant folding
// and common subexpression elimination when DEREFERENCING or SLICING is
// performed; for instance
// 
//   x[i + 1] := x[i + 1] + 1
// 
// translates into
// 
//   tmp = x[i + 1]; tmp := tmp + 1
// 
// We don't do stuff that is easily recognised by a back end compiler,
// for instance symbolic simplification.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-genie.h"
#include "a68g-listing.h"
#include "a68g-mp.h"
#include "a68g-optimiser.h"
#include "a68g-compiler.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

typedef union UFU UFU;

union UFU
{
  UNSIGNED_T u;
  REAL_T f;
};

#define BASIC(p, n) (basic_unit (stems_from ((p), (n))))

#define CON "const"
#define ELM "elem"
#define TMP "tmp"
#define ARG "arg"
#define ARR "array"
#define DEC "declarer"
#define DRF "deref"
#define DSP "display"
#define FUN "function"
#define PUP "pop"
#define REF "ref"
#define SEL "field"
#define TUP "tuple"

#define A68_MAKE_NOTHING 0
#define A68_MAKE_OTHERS 1
#define A68_MAKE_FUNCTION 2

#define OFFSET_OFF(s) (OFFSET (NODE_PACK (SUB (s))))
#define WIDEN_TO(p, a, b) (MOID (p) == MODE (b) && MOID (SUB (p)) == MODE (a))

#define NEEDS_DNS(m) (m != NO_MOID && (IS (m, REF_SYMBOL) || IS (m, PROC_SYMBOL) || IS (m, UNION_SYMBOL) || IS (m, FORMAT_SYMBOL)))

#define CODE_EXECUTE(p) {\
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "EXECUTE_UNIT_TRACE (_NODE_ (%d));", NUMBER (p)));\
  }

#define NAME_SIZE 200

static BOOL_T basic_unit (NODE_T *);
static char *compile_cast (NODE_T *, FILE_T);
static char *compile_denotation (NODE_T *, FILE_T);
static char *compile_dereference_identifier (NODE_T *, FILE_T);
static char *compile_identifier (NODE_T *, FILE_T);
static char *optimise_basic (NODE_T *, FILE_T);
static char *optimise_unit (NODE_T *, FILE_T, BOOL_T);
static void indentf (FILE_T, int);
static void indent (FILE_T, char *);
static void inline_unit (NODE_T *, FILE_T, int);
static void optimise_units (NODE_T *, FILE_T);

// The phases we go through.

enum
{ L_NONE = 0, L_DECLARE = 1, L_INITIALISE, L_EXECUTE, L_EXECUTE_2, L_YIELD, L_PUSH };

//! @brief Compiler optimisation option string

char *optimisation_option (void)
{
  switch (OPTION_OPT_LEVEL (&A68_JOB)) {
  case OPTIMISE_0:{
      return "-Og";
    }
  case OPTIMISE_1:{
      return "-O1";
    }
  case OPTIMISE_2:{
      return "-O2";
    }
  case OPTIMISE_3:{
      return "-O3";
    }
  case OPTIMISE_FAST:{
      return "-Ofast";
    }
  default:{
      return "-Og";
    }
  }
}

// Pretty printing stuff.

//! @brief Name formatting

static char *moid_with_name (char *pre, MOID_T * m, char *post)
{
  static char buf[NAME_SIZE];
  char *mode = "MODE", *ref = NO_TEXT;
  if (m != NO_MOID && IS (m, REF_SYMBOL)) {
    ref = "REF";
    m = SUB (m);
  }
  if (m == M_INT) {
    mode = "INT";
  } else if (m == M_REAL) {
    mode = "REAL";
  } else if (m == M_BOOL) {
    mode = "BOOL";
  } else if (m == M_CHAR) {
    mode = "CHAR";
  } else if (m == M_BITS) {
    mode = "BITS";
  } else if (m == M_VOID) {
    mode = "VOID";
  }
  if (ref == NO_TEXT) {
    snprintf (buf, NAME_SIZE, "%s%s%s", pre, mode, post);
  } else {
    snprintf (buf, NAME_SIZE, "%sREF_%s%s", pre, mode, post);
  }
  return buf;
}

//! @brief Write indented text.

static void indent (FILE_T out, char *str)
{
  int j = A68_OPT (indentation);
  if (out == 0) {
    return;
  }
  while (j-- > 0) {
    WRITE (out, "  ");
  }
  WRITE (out, str);
}

//! @brief Write unindented text.

static void undent (FILE_T out, char *str)
{
  if (out == 0) {
    return;
  }
  WRITE (out, str);
}

//! @brief Write indent text.

static void indentf (FILE_T out, int ret)
{
  if (out == 0) {
    return;
  }
  if (ret >= 0) {
    indent (out, A68 (edit_line));
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, error_specification ());
  }
}

//! @brief Write unindent text.

static void undentf (FILE_T out, int ret)
{
  if (out == 0) {
    return;
  }
  if (ret >= 0) {
    WRITE (out, A68 (edit_line));
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, error_specification ());
  }
}

// Administration of C declarations .
// Pretty printing of C declarations.

//! @brief Add declaration to a tree.

//! @brief Add declaration to a tree.

DEC_T *add_identifier (DEC_T ** p, int level, char *idf)
{
  char *z = new_temp_string (idf);
  while (*p != NO_DEC) {
    int k = strcmp (z, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, z);
      return *p;
    }
  }
  *p = (DEC_T *) get_temp_heap_space ((size_t) SIZE_ALIGNED (DEC_T));
  TEXT (*p) = z;
  LEVEL (*p) = level;
  SUB (*p) = LESS (*p) = MORE (*p) = NO_DEC;
  return *p;
}

//! @brief Add declaration to a tree.

DEC_T *add_declaration (DEC_T ** p, char *mode, int level, char *idf)
{
  char *z = new_temp_string (mode);
  while (*p != NO_DEC) {
    int k = strcmp (z, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      (void) add_identifier (&SUB (*p), level, idf);
      return *p;
    }
  }
  *p = (DEC_T *) get_temp_heap_space ((size_t) SIZE_ALIGNED (DEC_T));
  TEXT (*p) = z;
  LEVEL (*p) = -1;
  SUB (*p) = LESS (*p) = MORE (*p) = NO_DEC;
  (void) add_identifier (&SUB (*p), level, idf);
  return *p;
}

//! @brief Print identifiers (following mode).

void print_identifiers (FILE_T out, DEC_T * p)
{
  if (p != NO_DEC) {
    print_identifiers (out, LESS (p));
    if (A68_OPT (put_idf_comma)) {
      WRITE (out, ", ");
    } else {
      A68_OPT (put_idf_comma) = A68_TRUE;
    }
    if (LEVEL (p) > 0) {
      int k = LEVEL (p);
      while (k--) {
        WRITE (out, "*");
      }
      WRITE (out, " ");
    }
    WRITE (out, TEXT (p));
    print_identifiers (out, MORE (p));
  }
}

//! @brief Print declarations.

void print_declarations (FILE_T out, DEC_T * p)
{
  if (p != NO_DEC) {
    print_declarations (out, LESS (p));
    indent (out, TEXT (p));
    WRITE (out, " ");
    A68_OPT (put_idf_comma) = A68_FALSE;
    print_identifiers (out, SUB (p));
    WRITE (out, ";\n");
    print_declarations (out, MORE (p));
  }
}

// Administration for common functions.
// Otherwise we generate many routines that push 0 or 1 or TRUE etc.

#define UNIC_NAME(k) (A68_OPT (unic_functions)[k].fun)

enum
{ UNIC_EXISTS, UNIC_MAKE_NEW, UNIC_MAKE_ALT };

//! @brief Make name.

static char *make_unic_name (char *buf, char *name, char *tag, char *ext)
{
  if (strlen (tag) > 0) {
    ASSERT (snprintf (buf, NAME_SIZE, "genie_%s_%s_%s", name, tag, ext) >= 0);
  } else {
    ASSERT (snprintf (buf, NAME_SIZE, "genie_%s_%s", name, ext) >= 0);
  }
  ABEND (strlen (buf) >= NAME_SIZE, ERROR_ACTION, __func__);
  return buf;
}

//! @brief Look up a name in the list.

static char *signed_in_name (char *name)
{
  int k;
  for (k = 0; k < A68_OPT (unic_pointer); k++) {
    if (strcmp (UNIC_NAME (k), name) == 0) {
      return UNIC_NAME (k);
    }
  }
  return NO_TEXT;
}

//! @brief Enter new name in list, if there is space.

static void sign_in_name (char *name, int *action)
{
  if (signed_in_name (name)) {
    *action = UNIC_EXISTS;
  } else if (A68_OPT (unic_pointer) < MAX_UNIC) {
    UNIC_NAME (A68_OPT (unic_pointer)) = new_string (name, NO_TEXT);
    A68_OPT (unic_pointer)++;
    *action = UNIC_MAKE_NEW;
  } else {
    *action = UNIC_MAKE_ALT;
  }
}

//! @brief Book identifier to keep track of it for CSE.

static void sign_in (int action, int phase, char *idf, void *info, int number)
{
  if (A68_OPT (cse_pointer) < MAX_BOOK) {
    ACTION (&A68_OPT (cse_book)[A68_OPT (cse_pointer)]) = action;
    PHASE (&A68_OPT (cse_book)[A68_OPT (cse_pointer)]) = phase;
    IDF (&A68_OPT (cse_book)[A68_OPT (cse_pointer)]) = idf;
    INFO (&A68_OPT (cse_book)[A68_OPT (cse_pointer)]) = info;
    NUMBER (&A68_OPT (cse_book)[A68_OPT (cse_pointer)]) = number;
    A68_OPT (cse_pointer)++;
  }
}

//! @brief Whether identifier is signed_in.

static BOOK_T *signed_in (int action, int phase, char *idf)
{
  int k;
  for (k = 0; k < A68_OPT (cse_pointer); k++) {
    if (IDF (&A68_OPT (cse_book)[k]) == idf && ACTION (&A68_OPT (cse_book)[k]) == action && PHASE (&A68_OPT (cse_book)[k]) >= phase) {
      return &(A68_OPT (cse_book)[k]);
    }
  }
  return NO_BOOK;
}

//! @brief Make name.

static char *make_name (char *buf, char *name, char *tag, int n)
{
  if (strlen (tag) > 0) {
    ASSERT (snprintf (buf, NAME_SIZE, "genie_%s_%s_%d", name, tag, n) >= 0);
  } else {
    ASSERT (snprintf (buf, NAME_SIZE, "genie_%s_%d", name, n) >= 0);
  }
  ABEND (strlen (buf) >= NAME_SIZE, ERROR_ACTION, __func__);
  return buf;
}

//! @brief Whether two sub-trees are the same Algol 68 construct.

static BOOL_T same_tree (NODE_T * l, NODE_T * r)
{
  if (l == NO_NODE) {
    return (BOOL_T) (r == NO_NODE);
  } else if (r == NO_NODE) {
    return (BOOL_T) (l == NO_NODE);
  } else if (ATTRIBUTE (l) == ATTRIBUTE (r) && NSYMBOL (l) == NSYMBOL (r)) {
    return (BOOL_T) (same_tree (SUB (l), SUB (r)) && same_tree (NEXT (l), NEXT (r)));
  } else {
    return A68_FALSE;
  }
}

// Basic mode check.

//! @brief Whether primitive mode, with simple C equivalent.

static BOOL_T primitive_mode (MOID_T * m)
{
  if (m == M_INT) {
    return A68_TRUE;
  } else if (m == M_REAL) {
    return A68_TRUE;
  } else if (m == M_BOOL) {
    return A68_TRUE;
  } else if (m == M_CHAR) {
    return A68_TRUE;
  } else if (m == M_BITS) {
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether mode is handled by the constant folder.

BOOL_T folder_mode (MOID_T * m)
{
  if (primitive_mode (m)) {
    return A68_TRUE;
  } else if (m == M_COMPLEX) {
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether basic mode, for which units are compiled.

static BOOL_T basic_mode (MOID_T * m)
{
  if (primitive_mode (m)) {
    return A68_TRUE;
  } else if (IS (m, REF_SYMBOL)) {
    if (IS (SUB (m), REF_SYMBOL) || IS (SUB (m), PROC_SYMBOL)) {
      return A68_FALSE;
    } else {
      return basic_mode (SUB (m));
    }
  } else if (IS (m, ROW_SYMBOL)) {
    return A68_FALSE;
// Not (fully) implemented yet.
// TODO: code to convert stacked units into an array.
//  if (primitive_mode (SUB (m))) {
//    return A68_TRUE;
//  } else if (IS (SUB (m), STRUCT_SYMBOL)) {
//    return basic_mode (SUB (m));
//  } else {
//    return A68_FALSE;
//  }
  } else if (IS (m, STRUCT_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      if (!primitive_mode (MOID (p))) {
        return A68_FALSE;
      }
    }
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether basic mode, which is not a row.

static BOOL_T basic_mode_non_row (MOID_T * m)
{
  if (primitive_mode (m)) {
    return A68_TRUE;
  } else if (IS (m, REF_SYMBOL)) {
    if (IS (SUB (m), REF_SYMBOL) || IS (SUB (m), PROC_SYMBOL)) {
      return A68_FALSE;
    } else {
      return basic_mode_non_row (SUB (m));
    }
  } else if (IS (m, STRUCT_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      if (!primitive_mode (MOID (p))) {
        return A68_FALSE;
      }
    }
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether stems from certain attribute.

static NODE_T *stems_from (NODE_T * p, int att)
{
  if (IS (p, VOIDING)) {
    return stems_from (SUB (p), att);
  } else if (IS (p, UNIT)) {
    return stems_from (SUB (p), att);
  } else if (IS (p, TERTIARY)) {
    return stems_from (SUB (p), att);
  } else if (IS (p, SECONDARY)) {
    return stems_from (SUB (p), att);
  } else if (IS (p, PRIMARY)) {
    return stems_from (SUB (p), att);
  } else if (IS (p, att)) {
    return p;
  } else {
    return NO_NODE;
  }
}

// Basic unit check                                      .
// Whether a unit is sufficiently "basic" to be compiled.

//! @brief Whether basic collateral clause.

static BOOL_T basic_collateral (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, UNIT)) {
    return (BOOL_T) (basic_mode (MOID (p)) && basic_unit (SUB (p)) && basic_collateral (NEXT (p)));
  } else {
    return (BOOL_T) (basic_collateral (SUB (p)) && basic_collateral (NEXT (p)));
  }
}

//! @brief Whether basic serial clause.

static void count_basic_units (NODE_T * p, int *total, int *good)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      (*total)++;
      if (basic_unit (p)) {
        (*good)++;
      }
    } else if (IS (p, DECLARATION_LIST)) {
      (*total)++;
    } else {
      count_basic_units (SUB (p), total, good);
    }
  }
}

//! @brief Whether basic serial clause.

static BOOL_T basic_serial (NODE_T * p, int want)
{
  int total = 0, good = 0;
  count_basic_units (p, &total, &good);
  if (want > 0) {
    return total == want && total == good;
  } else {
    return total == good;
  }
}

//! @brief Whether basic indexer.

static BOOL_T basic_indexer (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, TRIMMER)) {
    return A68_FALSE;
  } else if (IS (p, UNIT)) {
    return basic_unit (p);
  } else {
    return (BOOL_T) (basic_indexer (SUB (p)) && basic_indexer (NEXT (p)));
  }
}

//! @brief Whether basic slice.

static BOOL_T basic_slice (NODE_T * p)
{
  if (IS (p, SLICE)) {
    NODE_T *prim = SUB (p);
    NODE_T *idf = stems_from (prim, IDENTIFIER);
    if (idf != NO_NODE) {
      NODE_T *indx = NEXT (prim);
      return basic_indexer (indx);
    }
  }
  return A68_FALSE;
}

//! @brief Whether basic argument.

static BOOL_T basic_argument (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, UNIT)) {
    return (BOOL_T) (basic_mode (MOID (p)) && basic_unit (p) && basic_argument (NEXT (p)));
  } else {
    return (BOOL_T) (basic_argument (SUB (p)) && basic_argument (NEXT (p)));
  }
}

//! @brief Whether basic call.

static BOOL_T basic_call (NODE_T * p)
{
  if (IS (p, CALL)) {
    NODE_T *prim = SUB (p);
    NODE_T *idf = stems_from (prim, IDENTIFIER);
    if (idf == NO_NODE) {
      return A68_FALSE;
    } else if (SUB_MOID (idf) == MOID (p)) {    // Prevent partial parametrisation
      int k;
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          NODE_T *args = NEXT (prim);
          return basic_argument (args);
        }
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether basic monadic formula.

static BOOL_T basic_monadic_formula (NODE_T * p)
{
  if (IS (p, MONADIC_FORMULA)) {
    NODE_T *op = SUB (p);
    int k;
    for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k++) {
      if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
        NODE_T *rhs = NEXT (op);
        return basic_unit (rhs);
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether basic dyadic formula.

static BOOL_T basic_formula (NODE_T * p)
{
  if (IS (p, FORMULA)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    if (op == NO_NODE) {
      return basic_monadic_formula (lhs);
    } else {
      int k;
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          NODE_T *rhs = NEXT (op);
          return (BOOL_T) (basic_unit (lhs) && basic_unit (rhs));
        }
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether basic conditional clause.

static BOOL_T basic_conditional (NODE_T * p)
{
  if (!(IS (p, IF_PART) || IS (p, OPEN_PART))) {
    return A68_FALSE;
  }
  if (!basic_serial (NEXT_SUB (p), 1)) {
    return A68_FALSE;
  }
  FORWARD (p);
  if (!(IS (p, THEN_PART) || IS (p, CHOICE))) {
    return A68_FALSE;
  }
  if (!basic_serial (NEXT_SUB (p), 1)) {
    return A68_FALSE;
  }
  FORWARD (p);
  if (IS (p, ELSE_PART) || IS (p, CHOICE)) {
    return basic_serial (NEXT_SUB (p), 1);
  } else if (IS (p, FI_SYMBOL)) {
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Whether basic unit.

static BOOL_T basic_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_FALSE;
  } else if (IS (p, UNIT)) {
    return basic_unit (SUB (p));
  } else if (IS (p, TERTIARY)) {
    return basic_unit (SUB (p));
  } else if (IS (p, SECONDARY)) {
    return basic_unit (SUB (p));
  } else if (IS (p, PRIMARY)) {
    return basic_unit (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    return basic_unit (SUB (p));
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 3) {
    if (IS (p, CLOSED_CLAUSE)) {
      return basic_serial (NEXT_SUB (p), 1);
    } else if (IS (p, COLLATERAL_CLAUSE)) {
      return basic_mode (MOID (p)) && basic_collateral (NEXT_SUB (p));
    } else if (IS (p, CONDITIONAL_CLAUSE)) {
      return basic_mode (MOID (p)) && basic_conditional (SUB (p));
    }
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 2) {
    if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), IDENTIFIER) != NO_NODE) {
      NODE_T *dst = SUB_SUB (p);
      NODE_T *src = NEXT_NEXT (dst);
      return (BOOL_T) basic_unit (src) && basic_mode_non_row (MOID (src));
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), SLICE) != NO_NODE) {
      NODE_T *dst = SUB_SUB (p);
      NODE_T *src = NEXT_NEXT (dst);
      NODE_T *slice = stems_from (dst, SLICE);
      return (BOOL_T) (IS (MOID (slice), REF_SYMBOL) && basic_slice (slice) && basic_unit (src) && basic_mode_non_row (MOID (src)));
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), SELECTION) != NO_NODE) {
      NODE_T *dst = SUB_SUB (p);
      NODE_T *src = NEXT_NEXT (dst);
      return (BOOL_T) (stems_from (NEXT_SUB (stems_from (dst, SELECTION)), IDENTIFIER) != NO_NODE && basic_unit (src) && basic_mode_non_row (MOID (dst)));
    } else if (IS (p, VOIDING)) {
      return basic_unit (SUB (p));
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SLICE)) {
      NODE_T *slice = stems_from (SUB (p), SLICE);
      return (BOOL_T) (basic_mode (MOID (p)) && IS (MOID (SUB (slice)), REF_SYMBOL) && basic_slice (slice));
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SELECTION)) {
      return (BOOL_T) (primitive_mode (MOID (p)) && BASIC (SUB (p), SELECTION));
    } else if (IS (p, WIDENING)) {
      if (WIDEN_TO (p, INT, REAL)) {
        return basic_unit (SUB (p));
      } else if (WIDEN_TO (p, REAL, COMPLEX)) {
        return basic_unit (SUB (p));
      } else {
        return A68_FALSE;
      }
    } else if (IS (p, CAST)) {
      return (BOOL_T) (folder_mode (MOID (SUB (p))) && basic_unit (NEXT_SUB (p)));
    } else if (IS (p, SLICE)) {
      return (BOOL_T) (basic_mode (MOID (p)) && basic_slice (p));
    } else if (IS (p, SELECTION)) {
      NODE_T *sec = stems_from (NEXT_SUB (p), IDENTIFIER);
      if (sec == NO_NODE) {
        return A68_FALSE;
      } else {
        return basic_mode_non_row (MOID (sec));
      }
    } else if (IS (p, IDENTITY_RELATION)) {
#define GOOD(p) (stems_from (p, IDENTIFIER) != NO_NODE && IS (MOID (stems_from ((p), IDENTIFIER)), REF_SYMBOL))
      NODE_T *lhs = SUB (p);
      NODE_T *rhs = NEXT_NEXT (lhs);
      if (GOOD (lhs) && GOOD (rhs)) {
        return A68_TRUE;
      } else if (GOOD (lhs) && stems_from (rhs, NIHIL) != NO_NODE) {
        return A68_TRUE;
      } else {
        return A68_FALSE;
      }
#undef GOOD
    }
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 1) {
    if (IS (p, IDENTIFIER)) {
      if (A68_STANDENV_PROC (TAX (p))) {
        int k;
        for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k++) {
          if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
            return A68_TRUE;
          }
        }
        return A68_FALSE;
      } else {
        return basic_mode (MOID (p));
      }
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), IDENTIFIER)) {
      return (BOOL_T) (basic_mode (MOID (p)) && BASIC (SUB (p), IDENTIFIER));
    } else if (IS (p, DENOTATION)) {
      return primitive_mode (MOID (p));
    } else if (IS (p, MONADIC_FORMULA)) {
      return (BOOL_T) (basic_mode (MOID (p)) && basic_monadic_formula (p));
    } else if (IS (p, FORMULA)) {
      return (BOOL_T) (basic_mode (MOID (p)) && basic_formula (p));
    } else if (IS (p, CALL)) {
      return (BOOL_T) (basic_mode (MOID (p)) && basic_call (p));
    }
  }
  return A68_FALSE;
}

// Constant folder                                                .
// Uses interpreter routines to calculate compile-time expressions.

// Constant unit check.

//! @brief Whether constant collateral clause.

static BOOL_T constant_collateral (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, UNIT)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_unit (SUB (p)) && constant_collateral (NEXT (p)));
  } else {
    return (BOOL_T) (constant_collateral (SUB (p)) && constant_collateral (NEXT (p)));
  }
}

//! @brief Whether constant serial clause.

static void count_constant_units (NODE_T * p, int *total, int *good)
{
  if (p != NO_NODE) {
    if (IS (p, UNIT)) {
      (*total)++;
      if (constant_unit (p)) {
        (*good)++;
      }
      count_constant_units (NEXT (p), total, good);
    } else {
      count_constant_units (SUB (p), total, good);
      count_constant_units (NEXT (p), total, good);
    }
  }
}

//! @brief Whether constant serial clause.

static BOOL_T constant_serial (NODE_T * p, int want)
{
  int total = 0, good = 0;
  count_constant_units (p, &total, &good);
  if (want > 0) {
    return total == want && total == good;
  } else {
    return total == good;
  }
}

//! @brief Whether constant argument.

static BOOL_T constant_argument (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
  } else if (IS (p, UNIT)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_unit (p) && constant_argument (NEXT (p)));
  } else {
    return (BOOL_T) (constant_argument (SUB (p)) && constant_argument (NEXT (p)));
  }
}

//! @brief Whether constant call.

static BOOL_T constant_call (NODE_T * p)
{
  if (IS (p, CALL)) {
    NODE_T *prim = SUB (p);
    NODE_T *idf = stems_from (prim, IDENTIFIER);
    if (idf != NO_NODE) {
      int k;
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          NODE_T *args = NEXT (prim);
          return constant_argument (args);
        }
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether constant monadic formula.

static BOOL_T constant_monadic_formula (NODE_T * p)
{
  if (IS (p, MONADIC_FORMULA)) {
    NODE_T *op = SUB (p);
    int k;
    for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k++) {
      if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
        NODE_T *rhs = NEXT (op);
        return constant_unit (rhs);
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether constant dyadic formula.

static BOOL_T constant_formula (NODE_T * p)
{
  if (IS (p, FORMULA)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    if (op == NO_NODE) {
      return constant_monadic_formula (lhs);
    } else {
      int k;
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          NODE_T *rhs = NEXT (op);
          return (BOOL_T) (constant_unit (lhs) && constant_unit (rhs));
        }
      }
    }
  }
  return A68_FALSE;
}

//! @brief Whether constant unit.

BOOL_T constant_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_FALSE;
  } else if (IS (p, UNIT)) {
    return constant_unit (SUB (p));
  } else if (IS (p, TERTIARY)) {
    return constant_unit (SUB (p));
  } else if (IS (p, SECONDARY)) {
    return constant_unit (SUB (p));
  } else if (IS (p, PRIMARY)) {
    return constant_unit (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    return constant_unit (SUB (p));
  } else if (IS (p, CLOSED_CLAUSE)) {
    return constant_serial (NEXT_SUB (p), 1);
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    return folder_mode (MOID (p)) && constant_collateral (NEXT_SUB (p));
  } else if (IS (p, WIDENING)) {
    if (WIDEN_TO (p, INT, REAL)) {
      return constant_unit (SUB (p));
    } else if (WIDEN_TO (p, REAL, COMPLEX)) {
      return constant_unit (SUB (p));
    } else {
      return A68_FALSE;
    }
  } else if (IS (p, IDENTIFIER)) {
    if (A68_STANDENV_PROC (TAX (p))) {
      int k;
      for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
          return A68_TRUE;
        }
      }
      return A68_FALSE;
    } else {
// Possible constant folding.
      NODE_T *def = NODE (TAX (p));
      BOOL_T ret = A68_FALSE;
      if (STATUS (p) & COOKIE_MASK) {
        diagnostic (A68_WARNING, p, WARNING_UNINITIALISED);
      } else {
        STATUS (p) |= COOKIE_MASK;
        if (folder_mode (MOID (p)) && def != NO_NODE && NEXT (def) != NO_NODE && IS (NEXT (def), EQUALS_SYMBOL)) {
          ret = constant_unit (NEXT_NEXT (def));
        }
      }
      STATUS (p) &= !(COOKIE_MASK);
      return ret;
    }
  } else if (IS (p, DENOTATION)) {
    return primitive_mode (MOID (p));
  } else if (IS (p, MONADIC_FORMULA)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_monadic_formula (p));
  } else if (IS (p, FORMULA)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_formula (p));
  } else if (IS (p, CALL)) {
    return (BOOL_T) (folder_mode (MOID (p)) && constant_call (p));
  } else if (IS (p, CAST)) {
    return (BOOL_T) (folder_mode (MOID (SUB (p))) && constant_unit (NEXT_SUB (p)));
  } else {
    return A68_FALSE;
  }
}

// Evaluate compile-time expressions using interpreter routines.

//! @brief Push denotation.

static void push_denotation (NODE_T * p)
{
#define PUSH_DENOTATION(mode, decl) {\
  decl z;\
  NODE_T *s = (IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p));\
  if (genie_string_to_value_internal (p, MODE (mode), NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {\
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (mode));\
  }\
  PUSH_VALUE (p, VALUE (&z), decl);}

  if (MOID (p) == M_INT) {
    PUSH_DENOTATION (INT, A68_INT);
  } else if (MOID (p) == M_REAL) {
    PUSH_DENOTATION (REAL, A68_REAL);
  } else if (MOID (p) == M_BOOL) {
    PUSH_DENOTATION (BOOL, A68_BOOL);
  } else if (MOID (p) == M_CHAR) {
    if ((NSYMBOL (p))[0] == NULL_CHAR) {
      PUSH_VALUE (p, NULL_CHAR, A68_CHAR);
    } else {
      PUSH_VALUE (p, (NSYMBOL (p))[0], A68_CHAR);
    }
  } else if (MOID (p) == M_BITS) {
    PUSH_DENOTATION (BITS, A68_BITS);
  }
#undef PUSH_DENOTATION
}

//! @brief Push widening.

static void push_widening (NODE_T * p)
{
  push_unit (SUB (p));
  if (WIDEN_TO (p, INT, REAL)) {
    A68_INT k;
    POP_OBJECT (p, &k, A68_INT);
    PUSH_VALUE (p, (REAL_T) VALUE (&k), A68_REAL);
  } else if (WIDEN_TO (p, REAL, COMPLEX)) {
    PUSH_VALUE (p, 0.0, A68_REAL);
  }
}

//! @brief Code collateral units.

static void push_collateral_units (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    push_unit (p);
  } else {
    push_collateral_units (SUB (p));
    push_collateral_units (NEXT (p));
  }
}

//! @brief Code argument.

static void push_argument (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      push_unit (p);
    } else {
      push_argument (SUB (p));
    }
  }
}

//! @brief Push unit.

void push_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  }
  if (IS (p, UNIT)) {
    push_unit (SUB (p));
  } else if (IS (p, TERTIARY)) {
    push_unit (SUB (p));
  } else if (IS (p, SECONDARY)) {
    push_unit (SUB (p));
  } else if (IS (p, PRIMARY)) {
    push_unit (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    push_unit (SUB (p));
  } else if (IS (p, CLOSED_CLAUSE)) {
    push_unit (SUB (NEXT_SUB (p)));
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    push_collateral_units (NEXT_SUB (p));
  } else if (IS (p, WIDENING)) {
    push_widening (p);
  } else if (IS (p, IDENTIFIER)) {
    if (A68_STANDENV_PROC (TAX (p))) {
      (void) (*(PROCEDURE (TAX (p)))) (p);
    } else {
// Possible constant folding 
      NODE_T *def = NODE (TAX (p));
      push_unit (NEXT_NEXT (def));
    }
  } else if (IS (p, DENOTATION)) {
    push_denotation (p);
  } else if (IS (p, MONADIC_FORMULA)) {
    NODE_T *op = SUB (p);
    NODE_T *rhs = NEXT (op);
    push_unit (rhs);
    (*(PROCEDURE (TAX (op)))) (op);
  } else if (IS (p, FORMULA)) {
    NODE_T *lhs = SUB (p);
    NODE_T *op = NEXT (lhs);
    if (op == NO_NODE) {
      push_unit (lhs);
    } else {
      NODE_T *rhs = NEXT (op);
      push_unit (lhs);
      push_unit (rhs);
      (*(PROCEDURE (TAX (op)))) (op);
    }
  } else if (IS (p, CALL)) {
    NODE_T *prim = SUB (p);
    NODE_T *args = NEXT (prim);
    NODE_T *idf = stems_from (prim, IDENTIFIER);
    push_argument (args);
    (void) (*(PROCEDURE (TAX (idf)))) (p);
  } else if (IS (p, CAST)) {
    push_unit (NEXT_SUB (p));
  }
}

//! @brief Code constant folding.

static void constant_folder (NODE_T * p, FILE_T out, int phase)
{
  if (phase == L_DECLARE) {
    if (MOID (p) == M_COMPLEX) {
      char acc[NAME_SIZE];
      A68_REAL re, im;
      (void) make_name (acc, CON, "", NUMBER (p));
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &im, A68_REAL);
      POP_OBJECT (p, &re, A68_REAL);
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_COMPLEX %s = {", acc));
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "{INIT_MASK, %.*g}", REAL_WIDTH + 2, VALUE (&re)));
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", {INIT_MASK, %.*g}", REAL_WIDTH + 2, VALUE (&im)));
      undent (out, "};\n");
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
  } else if (phase == L_EXECUTE) {
    if (MOID (p) == M_COMPLEX) {
// Done at declaration stage 
    }
  } else if (phase == L_YIELD) {
    if (MOID (p) == M_INT) {
      A68_INT k;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &k, A68_INT);
      ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, A68_LD, VALUE (&k)) >= 0);
      undent (out, A68 (edit_line));
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_REAL) {
      A68_REAL x;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &x, A68_REAL);
// Mind overflowing or underflowing values.
      if (!finite (VALUE (&x))) {
        A68_OPT (code_errors)++;
        VALUE (&x) = 0.0;
      }
      if (VALUE (&x) == REAL_MAX) {
        undent (out, "REAL_MAX");
      } else if (VALUE (&x) == -REAL_MAX) {
        undent (out, "(-REAL_MAX)");
      } else {
        ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%.*g", REAL_WIDTH + 2, VALUE (&x)) >= 0);
        undent (out, A68 (edit_line));
      }
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_BOOL) {
      A68_BOOL b;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &b, A68_BOOL);
      ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", (VALUE (&b) ? "A68_TRUE" : "A68_FALSE")) >= 0);
      undent (out, A68 (edit_line));
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_CHAR) {
      A68_CHAR c;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &c, A68_CHAR);
      if (VALUE (&c) == '\'') {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'\\\''"));
      } else if (VALUE (&c) == '\\') {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'\\\\'"));
      } else if (VALUE (&c) == NULL_CHAR) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "NULL_CHAR"));
      } else if (IS_PRINT (VALUE (&c))) {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "'%c'", (CHAR_T) VALUE (&c)));
      } else {
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(CHAR_T) %d", VALUE (&c)));
      }
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_BITS) {
      A68_BITS b;
      A68_SP = 0;
      push_unit (p);
      POP_OBJECT (p, &b, A68_BITS);
      ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "(UNSIGNED_T) 0x" A68_LX, VALUE (&b)) >= 0);
      undent (out, A68 (edit_line));
      ABEND (A68_SP > 0, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else if (MOID (p) == M_COMPLEX) {
      char acc[NAME_SIZE];
      (void) make_name (acc, CON, "", NUMBER (p));
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "(A68_REAL *) %s", acc));
    }
  }
}

// Auxilliary routines for emitting C code.

//! @brief Whether frame needs initialisation.

static BOOL_T need_initialise_frame (NODE_T * p)
{
  TAG_T *tag;
  int count;
  for (tag = ANONYMOUS (TABLE (p)); tag != NO_TAG; FORWARD (tag)) {
    if (PRIO (tag) == ROUTINE_TEXT) {
      return A68_TRUE;
    } else if (PRIO (tag) == FORMAT_TEXT) {
      return A68_TRUE;
    }
  }
  count = 0;
  genie_find_proc_op (p, &count);
  if (count > 0) {
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Comment source line.

static void comment_tree (NODE_T * p, FILE_T out, int *want_space, int *max_print)
{
// Take care not to generate nested comments.
#define UNDENT(out, p) {\
  char * q;\
  for (q = p; q[0] != NULL_CHAR; q ++) {\
    if (q[0] == '*' && q[1] == '/') {\
      undent (out, "\\*\\/");\
      q ++;\
    } else if (q[0] == '/' && q[1] == '*') {\
      undent (out, "\\/\\*");\
      q ++;\
    } else {\
      char w[2];\
      w[0] = q[0];\
      w[1] = NULL_CHAR;\
      undent (out, w);\
    }\
  }}

  for (; p != NO_NODE && (*max_print) >= 0; FORWARD (p)) {
    if (IS (p, ROW_CHAR_DENOTATION)) {
      if (*want_space != 0) {
        UNDENT (out, " ");
      }
      UNDENT (out, "\"");
      UNDENT (out, NSYMBOL (p));
      UNDENT (out, "\"");
      *want_space = 2;
    } else if (SUB (p) != NO_NODE) {
      comment_tree (SUB (p), out, want_space, max_print);
    } else if (NSYMBOL (p)[0] == '(' || NSYMBOL (p)[0] == '[' || NSYMBOL (p)[0] == '{') {
      if (*want_space == 2) {
        UNDENT (out, " ");
      }
      UNDENT (out, NSYMBOL (p));
      *want_space = 0;
    } else if (NSYMBOL (p)[0] == ')' || NSYMBOL (p)[0] == ']' || NSYMBOL (p)[0] == '}') {
      UNDENT (out, NSYMBOL (p));
      *want_space = 1;
    } else if (NSYMBOL (p)[0] == ';' || NSYMBOL (p)[0] == ',') {
      UNDENT (out, NSYMBOL (p));
      *want_space = 2;
    } else if (strlen (NSYMBOL (p)) == 1 && (NSYMBOL (p)[0] == '.' || NSYMBOL (p)[0] == ':')) {
      UNDENT (out, NSYMBOL (p));
      *want_space = 2;
    } else {
      if (*want_space != 0) {
        UNDENT (out, " ");
      }
      if ((*max_print) > 0) {
        UNDENT (out, NSYMBOL (p));
      } else if ((*max_print) == 0) {
        if (*want_space == 0) {
          UNDENT (out, " ");
        }
        UNDENT (out, "...");
      }
      (*max_print)--;
      if (IS_UPPER (NSYMBOL (p)[0])) {
        *want_space = 2;
      } else if (!IS_ALNUM (NSYMBOL (p)[0])) {
        *want_space = 2;
      } else {
        *want_space = 1;
      }
    }
  }
#undef UNDENT
}

//! @brief Comment source line.

static void comment_source (NODE_T * p, FILE_T out)
{
  int want_space = 0, max_print = 16, ld = -1;
  undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "\n// %s: %d: ", FILENAME (LINE (INFO (p))), LINE_NUMBER (p)));
  comment_tree (p, out, &want_space, &max_print);
  tree_listing (out, p, 1, LINE (INFO (p)), &ld, A68_TRUE);
  undent (out, "\n");
}

//! @brief Inline comment source line.

static void inline_comment_source (NODE_T * p, FILE_T out)
{
  int want_space = 0, max_print = 8;
  undent (out, " // ");
  comment_tree (p, out, &want_space, &max_print);
//  undent (out, " */");
}

//! @brief Write prelude.

static void write_prelude (FILE_T out)
{
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "// \"%s\" %s\n", FILE_OBJECT_NAME (&A68_JOB), PACKAGE_STRING));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "// optimiser_level=%d code_level=%d\n", OPTION_OPT_LEVEL (&A68_JOB), A68_OPT (OPTION_CODE_LEVEL)));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "// %s %s\n", __DATE__, __TIME__));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "\n#include <%s/a68g-config.h>\n", PACKAGE));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "#include <%s/a68g.h>\n", PACKAGE));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "#include <%s/a68g-genie.h>\n", PACKAGE));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "#include <%s/a68g-prelude.h>\n", PACKAGE));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "#include <%s/a68g-environ.h>\n", PACKAGE));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "#include <%s/a68g-lib.h>\n", PACKAGE));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "#include <%s/a68g-optimiser.h>\n", PACKAGE));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "#include <%s/a68g-frames.h>\n", PACKAGE));
  indent (out, "\n#define _NODE_(n) (A68 (node_register)[n])\n");
  indent (out, "#define _STATUS_(z) (STATUS (z))\n");
  indent (out, "#define _VALUE_(z) (VALUE (z))\n");
}

//! @brief Write initialisation of frame.

static void init_static_frame (FILE_T out, NODE_T * p)
{
  if (AP_INCREMENT (TABLE (p)) > 0) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "FRAME_CLEAR (" A68_LU ");\n", AP_INCREMENT (TABLE (p))));
  }
  if (LEX_LEVEL (p) == A68 (global_level)) {
    indent (out, "A68_GLOBALS = A68_FP;\n");
  }
  if (need_initialise_frame (p)) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "initialise_frame (_NODE_ (%d));\n", NUMBER (p)));
  }
}

// COMPILATION OF PARTIAL UNITS.

static void optimise_check_init (NODE_T * p, FILE_T out, char *idf)
{
  if (OPTION_COMPILE_CHECK (&A68_JOB) && folder_mode (MOID (p))) {
    if (MOID (p) == M_COMPLEX) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "if (!(INITIALISED (&(*%s)[0]) && INITIALISED (&(*%s)[1]))) {\n", idf, idf));
      A68_OPT (indentation)++;
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "diagnostic (A68_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE_FROM, M_COMPLEX);\n"));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "exit_genie ((p), A68_RUNTIME_ERROR);\n"));
      A68_OPT (indentation)--;
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "}\n"));
    } else {
      char *M = "M_ERROR";
      if (MOID (p) == M_INT) {
        M = "M_INT";
      } else if (MOID (p) == M_REAL) {
        M = "M_REAL";
      } else if (MOID (p) == M_BOOL) {
        M = "M_BOOL";
      } else if (MOID (p) == M_CHAR) {
        M = "M_CHAR";
      }
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "if (!INITIALISED(%s)) {\n", idf));
      A68_OPT (indentation)++;
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "diagnostic (A68_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE_FROM, %s);\n", M));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "exit_genie ((p), A68_RUNTIME_ERROR);\n"));
      A68_OPT (indentation)--;
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "}\n"));
    }
  }
}

//! @brief Code getting objects from the stack.

static void get_stack (NODE_T * p, FILE_T out, char *dst, char *cast)
{
  if (A68_OPT (OPTION_CODE_LEVEL) >= 4) {
    if (LEVEL (GINFO (p)) == A68 (global_level)) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "GET_GLOBAL (%s, %s, " A68_LU ");\n", dst, cast, OFFSET (TAX (p))));
    } else {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "GET_FRAME (%s, %s, %d, " A68_LU ");\n", dst, cast, LEVEL (GINFO (p)), OFFSET (TAX (p))));
    }
  } else {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "GET_FRAME (%s, %s, %d, " A68_LU ");\n", dst, cast, LEVEL (GINFO (p)), OFFSET (TAX (p))));
  }
}

//! @brief Code function prelude.

static void write_fun_prelude (NODE_T * p, FILE_T out, char *fn)
{
  (void) p;
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "\nPROP_T %s (NODE_T *p) {\n", fn));
  A68_OPT (indentation)++;
  indent (out, "PROP_T self;\n");
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "UNIT (&self) = %s;\n", fn));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "SOURCE (&self) = _NODE_ (%d);\n", NUMBER (p)));
  indent (out, "A68 (f_entry) = p;\n");
  A68_OPT (cse_pointer) = 0;
}

//! @brief Code function postlude.

static void write_fun_postlude (NODE_T * p, FILE_T out, char *fn)
{
  (void) fn;
  (void) p;
  indent (out, "return (self);\n");
  A68_OPT (indentation)--;
  A68_OPT (procedures)++;
  indent (out, "}\n");
  A68_OPT (cse_pointer) = 0;
}

//! @brief Code internal a68g mode.

static char *internal_mode (MOID_T * m)
{
  if (m == M_INT) {
    return "M_INT";
  } else if (m == M_REAL) {
    return "M_REAL";
  } else if (m == M_BOOL) {
    return "M_BOOL";
  } else if (m == M_CHAR) {
    return "M_CHAR";
  } else if (m == M_BITS) {
    return "M_BITS";
  } else {
    return "M_ERROR";
  }
}

//! @brief Code an A68 mode.

static char *inline_mode (MOID_T * m)
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

//! @brief Code denotation.

static void inline_denotation (NODE_T * p, FILE_T out, int phase)
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

static void inline_widening (NODE_T * p, FILE_T out, int phase)
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

static void inline_dereference_identifier (NODE_T * p, FILE_T out, int phase)
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
      optimise_check_init (p, out, idf);
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

static void inline_identifier (NODE_T * p, FILE_T out, int phase)
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
      optimise_check_init (p, out, idf);
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

static void inline_indexer (NODE_T * p, FILE_T out, int phase, INT_T * k, char *tup)
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

static void inline_dereference_slice (NODE_T * p, FILE_T out, int phase)
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

static void inline_slice_ref_to_ref (NODE_T * p, FILE_T out, int phase)
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

static void inline_slice (NODE_T * p, FILE_T out, int phase)
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

static void inline_monadic_formula (NODE_T * p, FILE_T out, int phase)
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

static void inline_formula (NODE_T * p, FILE_T out, int phase)
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

static void inline_single_argument (NODE_T * p, FILE_T out, int phase)
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

static void inline_call (NODE_T * p, FILE_T out, int phase)
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

static void inline_collateral_units (NODE_T * p, FILE_T out, int phase)
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

static void inline_collateral (NODE_T * p, FILE_T out, int phase)
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

static void inline_closed (NODE_T * p, FILE_T out, int phase)
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

static void inline_conditional (NODE_T * p, FILE_T out, int phase)
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

static void inline_dereference_selection (NODE_T * p, FILE_T out, int phase)
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

static void inline_selection (NODE_T * p, FILE_T out, int phase)
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

static void inline_selection_ref_to_ref (NODE_T * p, FILE_T out, int phase)
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

static void inline_ref_identifier (NODE_T * p, FILE_T out, int phase)
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

static void inline_identity_relation (NODE_T * p, FILE_T out, int phase)
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

static void inline_unit (NODE_T * p, FILE_T out, int phase)
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

// COMPILATION OF COMPLETE UNITS.

//! @brief Compile code clause.

static void embed_code_clause (NODE_T * p, FILE_T out)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ROW_CHAR_DENOTATION)) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s\n", NSYMBOL (p)));
    }
    embed_code_clause (SUB (p), out);
  }
}

//! @brief Compile push.

static void optimise_push (NODE_T * p, FILE_T out)
{
  if (primitive_mode (MOID (p))) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "PUSH_VALUE (p, "));
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", %s);\n", inline_mode (MOID (p))));
  } else if (basic_mode (MOID (p))) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MOVE ((void *) STACK_TOP, (void *) "));
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", %d);\n", SIZE (MOID (p))));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP += %d;\n", SIZE (MOID (p))));
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, moid_to_string (MOID (p), 80, NO_NODE));
  }
}

//! @brief Compile assign (C source to C destination).

static void optimise_assign (NODE_T * p, FILE_T out, char *dst)
{
  if (primitive_mode (MOID (p))) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_STATUS_ (%s) = INIT_MASK;\n", dst));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s) = ", dst));
    inline_unit (p, out, L_YIELD);
    undent (out, ";\n");
  } else if (basic_mode (MOID (p))) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MOVE ((void *) %s, (void *) ", dst));
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", %d);\n", SIZE (MOID (p))));
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, moid_to_string (MOID (p), 80, NO_NODE));
  }
}

//! @brief Compile denotation.

static char *optimise_denotation (NODE_T * p, FILE_T out, int compose_fun)
{
  if (primitive_mode (MOID (p))) {
    if (compose_fun == A68_MAKE_FUNCTION) {
      return compile_denotation (p, out);
    } else {
      static char fn[NAME_SIZE];
      comment_source (p, out);
      (void) make_name (fn, moid_with_name ("", MOID (p), "_denotation"), "", NUMBER (p));
      A68_OPT (root_idf) = NO_DEC;
      inline_unit (p, out, L_DECLARE);
      print_declarations (out, A68_OPT (root_idf));
      inline_unit (p, out, L_EXECUTE);
      if (primitive_mode (MOID (p))) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "PUSH_VALUE (p, "));
        inline_unit (p, out, L_YIELD);
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", %s);\n", inline_mode (MOID (p))));
      } else {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "PUSH (p, "));
        inline_unit (p, out, L_YIELD);
        undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", %d);\n", SIZE (MOID (p))));
      }
      return fn;
    }
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile cast.

static char *optimise_cast (NODE_T * p, FILE_T out, int compose_fun)
{
  if (compose_fun == A68_MAKE_FUNCTION) {
    return compile_cast (p, out);
  } else if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_cast"), "", NUMBER (p));
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (NEXT_SUB (p), out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (NEXT_SUB (p), out, L_EXECUTE);
    optimise_push (NEXT_SUB (p), out);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile identifier.

static char *optimise_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  if (compose_fun == A68_MAKE_FUNCTION) {
    return compile_identifier (p, out);
  } else if (basic_mode (MOID (p))) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, moid_with_name ("deref_REF_", MOID (p), "_identifier"), "", NUMBER (p));
    comment_source (p, out);
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (p, out, L_EXECUTE);
    optimise_push (p, out);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile dereference identifier.

static char *optimise_dereference_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  if (compose_fun == A68_MAKE_FUNCTION) {
    return compile_dereference_identifier (p, out);
  } else if (basic_mode (MOID (p))) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("deref_REF_", MOID (p), "_identifier"), "", NUMBER (p));
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (p, out, L_EXECUTE);
    optimise_push (p, out);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile slice.

static char *optimise_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_slice"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (p, out, L_EXECUTE);
    optimise_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile slice.

static char *optimise_dereference_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("deref_REF_", MOID (p), "_slice"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (p, out, L_EXECUTE);
    optimise_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile selection.

static char *optimise_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_select"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (p, out, L_EXECUTE);
    optimise_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile selection.

static char *optimise_dereference_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("deref_REF_", MOID (p), "_select"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (p, out, L_EXECUTE);
    optimise_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile formula.

static char *optimise_formula (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_formula"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    if (OPTION_COMPILE_CHECK (&A68_JOB) && !constant_unit (p)) {
      if (MOID (p) == M_REAL || MOID (p) == M_COMPLEX) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "errno = 0;\n"));
      }
    }
    inline_unit (p, out, L_EXECUTE);
    optimise_push (p, out);
    if (OPTION_COMPILE_CHECK (&A68_JOB) && !constant_unit (p)) {
      if (MOID (p) == M_REAL) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MATH_RTE (p, errno != 0, M_REAL, NO_TEXT);\n"));
      }
      if (MOID (p) == M_COMPLEX) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MATH_RTE (p, errno != 0, M_COMPLEX, NO_TEXT);\n"));
      }
    }
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile voiding formula.

static char *optimise_voiding_formula (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    char pop[NAME_SIZE];
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("void_", MOID (p), "_formula"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
    inline_unit (p, out, L_EXECUTE);
    indent (out, "(void) (");
    inline_unit (p, out, L_YIELD);
    undent (out, ");\n");
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile uniting.

static char *optimise_uniting (NODE_T * p, FILE_T out, int compose_fun)
{
  MOID_T *u = MOID (p), *v = MOID (SUB (p));
  NODE_T *q = SUB (p);
  if (basic_unit (q) && ATTRIBUTE (v) != UNION_SYMBOL && primitive_mode (v)) {
    static char fn[NAME_SIZE];
    char pop0[NAME_SIZE];
    (void) make_name (pop0, PUP, "0", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_unite"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop0);
    inline_unit (q, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop0));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "PUSH_UNION (_NODE_ (%d), %s);\n", NUMBER (p), internal_mode (v)));
    inline_unit (q, out, L_EXECUTE);
    optimise_push (q, out);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s + %d;\n", pop0, SIZE (u)));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile inline arguments.

static void inline_arguments (NODE_T * p, FILE_T out, int phase, int *size)
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

//! @brief Compile deproceduring.

static char *optimise_deproceduring (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *idf = stems_from (SUB (p), IDENTIFIER);
  if (idf == NO_NODE) {
    return NO_TEXT;
  } else if (!(SUB_MOID (idf) == M_VOID || basic_mode (SUB_MOID (idf)))) {
    return NO_TEXT;
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return NO_TEXT;
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE];
    (void) make_name (fun, FUN, "", NUMBER (idf));
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_deproc"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
// Declare.
    A68_OPT (root_idf) = NO_DEC;
    (void) add_declaration (&A68_OPT (root_idf), "A68_PROCEDURE", 1, fun);
    (void) add_declaration (&A68_OPT (root_idf), "NODE_T", 1, "body");
    print_declarations (out, A68_OPT (root_idf));
// Initialise.
    get_stack (idf, out, fun, "A68_PROCEDURE");
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", fun));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
// Execute procedure.
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT (body));\n");
    indent (out, "if (A68_FP == A68_MON (finish_frame_pointer)) {\n");
    A68_OPT (indentation)++;
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    A68_OPT (indentation)--;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  }
}

//! @brief Compile deproceduring.

static char *optimise_voiding_deproceduring (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *idf = stems_from (SUB_SUB (p), IDENTIFIER);
  if (idf == NO_NODE) {
    return NO_TEXT;
  } else if (!(SUB_MOID (idf) == M_VOID || basic_mode (SUB_MOID (idf)))) {
    return NO_TEXT;
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return NO_TEXT;
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE], pop[NAME_SIZE];
    (void) make_name (fun, FUN, "", NUMBER (idf));
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("void_", MOID (p), "_deproc"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
// Declare.
    A68_OPT (root_idf) = NO_DEC;
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    (void) add_declaration (&A68_OPT (root_idf), "A68_PROCEDURE", 1, fun);
    (void) add_declaration (&A68_OPT (root_idf), "NODE_T", 1, "body");
    print_declarations (out, A68_OPT (root_idf));
// Initialise.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
    if (compose_fun != A68_MAKE_NOTHING) {
    }
    get_stack (idf, out, fun, "A68_PROCEDURE");
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", fun));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
// Execute procedure.
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT (body));\n");
    indent (out, "if (A68_FP == A68_MON (finish_frame_pointer)) {\n");
    A68_OPT (indentation)++;
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    A68_OPT (indentation)--;
    indent (out, "}\n");
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    indent (out, "CLOSE_FRAME;\n");
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  }
}

//! @brief Compile call.

static char *optimise_call (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *proc = SUB (p);
  NODE_T *args = NEXT (proc);
  NODE_T *idf = stems_from (proc, IDENTIFIER);
  if (idf == NO_NODE) {
    return NO_TEXT;
  } else if (!(SUB_MOID (proc) == M_VOID || basic_mode (SUB_MOID (proc)))) {
    return NO_TEXT;
  } else if (DIM (MOID (proc)) == 0) {
    return NO_TEXT;
  } else if (A68_STANDENV_PROC (TAX (idf))) {
    if (basic_call (p)) {
      static char fun[NAME_SIZE];
      comment_source (p, out);
      (void) make_name (fun, moid_with_name ("", SUB_MOID (proc), "_call"), "", NUMBER (p));
      if (compose_fun == A68_MAKE_FUNCTION) {
        write_fun_prelude (p, out, fun);
      }
      A68_OPT (root_idf) = NO_DEC;
      inline_unit (p, out, L_DECLARE);
      print_declarations (out, A68_OPT (root_idf));
      inline_unit (p, out, L_EXECUTE);
      optimise_push (p, out);
      if (compose_fun == A68_MAKE_FUNCTION) {
        write_fun_postlude (p, out, fun);
      }
      return fun;
    } else {
      return NO_TEXT;
    }
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return NO_TEXT;
  } else if (DIM (PARTIAL_PROC (GINFO (proc))) != 0) {
    return NO_TEXT;
  } else if (!basic_argument (args)) {
    return NO_TEXT;
  } else {
    static char fun[NAME_SIZE];
    char body[NAME_SIZE], pop[NAME_SIZE];
    int size;
// Declare.
    (void) make_name (body, FUN, "", NUMBER (proc));
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fun, moid_with_name ("", SUB_MOID (proc), "_call"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fun);
    }
// Compute arguments.
    size = 0;
    A68_OPT (root_idf) = NO_DEC;
    inline_arguments (args, out, L_DECLARE, &size);
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    (void) add_declaration (&A68_OPT (root_idf), "A68_PROCEDURE", 1, body);
    (void) add_declaration (&A68_OPT (root_idf), "NODE_T", 1, "body");
    print_declarations (out, A68_OPT (root_idf));
// Initialise.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
    inline_arguments (args, out, L_INITIALISE, &size);
    get_stack (idf, out, body, "A68_PROCEDURE");
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", body));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", body));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
    size = 0;
    inline_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    inline_arguments (args, out, L_YIELD, &size);
// Execute procedure.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT_NEXT (body));\n");
    indent (out, "if (A68_FP == A68_MON (finish_frame_pointer)) {\n");
    A68_OPT (indentation)++;
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    A68_OPT (indentation)--;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fun);
    }
    return fun;
  }
}

//! @brief Compile call.

static char *optimise_voiding_call (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *proc = SUB (stems_from (p, CALL));
  NODE_T *args = NEXT (proc);
  NODE_T *idf = stems_from (proc, IDENTIFIER);
  if (idf == NO_NODE) {
    return NO_TEXT;
  } else if (!(SUB_MOID (proc) == M_VOID || basic_mode (SUB_MOID (proc)))) {
    return NO_TEXT;
  } else if (DIM (MOID (proc)) == 0) {
    return NO_TEXT;
  } else if (A68_STANDENV_PROC (TAX (idf))) {
    return NO_TEXT;
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return NO_TEXT;
  } else if (DIM (PARTIAL_PROC (GINFO (proc))) != 0) {
    return NO_TEXT;
  } else if (!basic_argument (args)) {
    return NO_TEXT;
  } else {
    static char fun[NAME_SIZE];
    char body[NAME_SIZE], pop[NAME_SIZE];
    int size;
// Declare.
    (void) make_name (body, FUN, "", NUMBER (proc));
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fun, moid_with_name ("void_", SUB_MOID (proc), "_call"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fun);
    }
// Compute arguments.
    size = 0;
    A68_OPT (root_idf) = NO_DEC;
    inline_arguments (args, out, L_DECLARE, &size);
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    (void) add_declaration (&A68_OPT (root_idf), "A68_PROCEDURE", 1, body);
    (void) add_declaration (&A68_OPT (root_idf), "NODE_T", 1, "body");
    print_declarations (out, A68_OPT (root_idf));
// Initialise.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
    inline_arguments (args, out, L_INITIALISE, &size);
    get_stack (idf, out, body, "A68_PROCEDURE");
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", body));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", body));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
    size = 0;
    inline_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    inline_arguments (args, out, L_YIELD, &size);
// Execute procedure.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT_NEXT (body));\n");
    indent (out, "if (A68_FP == A68_MON (finish_frame_pointer)) {\n");
    A68_OPT (indentation)++;
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    A68_OPT (indentation)--;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fun);
    }
    return fun;
  }
}

//! @brief Compile voiding assignation.

static char *optimise_voiding_assignation_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *dst = SUB (stems_from (p, ASSIGNATION));
  NODE_T *src = NEXT_NEXT (dst);
  if (BASIC (dst, SELECTION) && basic_unit (src) && basic_mode_non_row (MOID (dst))) {
    NODE_T *field = SUB (stems_from (dst, SELECTION));
    NODE_T *sec = NEXT (field);
    NODE_T *idf = stems_from (sec, IDENTIFIER);
    char sel[NAME_SIZE], ref[NAME_SIZE], pop[NAME_SIZE];
    char *field_idf = NSYMBOL (SUB (field));
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (pop, PUP, "", NUMBER (p));
    (void) make_name (fn, moid_with_name ("void_", MOID (SUB (p)), "_assign"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
// Declare.
    A68_OPT (root_idf) = NO_DEC;
    if (signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf)) == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_REF * %s; /* %s */\n", ref, NSYMBOL (idf)));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s * %s;\n", inline_mode (SUB_MOID (field)), sel));
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else {
      int n = NUMBER (signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf)));
      (void) make_name (ref, NSYMBOL (idf), "", n);
      (void) make_name (sel, SEL, "", n);
    }
    inline_unit (src, out, L_DECLARE);
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    print_declarations (out, A68_OPT (root_idf));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
// Initialise.
    if (signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf)) == NO_BOOK) {
      get_stack (idf, out, ref, "A68_REF");
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) & (ADDRESS (%s)[" A68_LU "]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (src, out, L_EXECUTE);
// Generate.
    optimise_assign (src, out, sel);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile voiding assignation.

static char *optimise_voiding_assignation_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *dst = SUB (stems_from (p, ASSIGNATION));
  NODE_T *src = NEXT_NEXT (dst);
  NODE_T *slice = stems_from (SUB (dst), SLICE);
  NODE_T *prim = SUB (slice);
  MOID_T *mode = SUB_MOID (dst);
  MOID_T *row_mode = DEFLEX (MOID (prim));
  if (IS (row_mode, REF_SYMBOL) && basic_slice (slice) && basic_unit (src) && basic_mode_non_row (MOID (src))) {
    NODE_T *indx = NEXT (prim);
    char *symbol = NSYMBOL (SUB (prim));
    char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE], pop[NAME_SIZE];
    static char fn[NAME_SIZE];
    INT_T k;
    comment_source (p, out);
    (void) make_name (pop, PUP, "", NUMBER (p));
    (void) make_name (fn, moid_with_name ("void_", MOID (SUB (p)), "_assign"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
// Declare.
    A68_OPT (root_idf) = NO_DEC;
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    if (signed_in (BOOK_DECL, L_DECLARE, symbol) == NO_BOOK) {
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
    } else {
      int n = NUMBER (signed_in (BOOK_DECL, L_EXECUTE, symbol));
      (void) make_name (idf, symbol, "", n);
      (void) make_name (arr, ARR, "", n);
      (void) make_name (tup, TUP, "", n);
      (void) make_name (elm, ELM, "", n);
      (void) make_name (drf, DRF, "", n);
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NO_TEXT);
    inline_unit (src, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
// Initialise.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
    if (signed_in (BOOK_DECL, L_EXECUTE, symbol) == NO_BOOK) {
      NODE_T *pidf = stems_from (prim, IDENTIFIER);
      get_stack (pidf, out, idf, "A68_REF");
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, DEREF (A68_ROW, %s));\n", arr, tup, idf));
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = ARRAY (%s);\n", elm, arr));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), (void *) indx, NUMBER (prim));
    }
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NO_TEXT);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ");\n"));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = DEREF (%s, & %s);\n", drf, inline_mode (mode), elm));
    inline_unit (src, out, L_EXECUTE);
// Generate.
    optimise_assign (src, out, drf);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile voiding assignation.

static char *optimise_voiding_assignation_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *dst = SUB (stems_from (p, ASSIGNATION));
  NODE_T *src = NEXT_NEXT (dst);
  if (BASIC (dst, IDENTIFIER) && basic_unit (src) && basic_mode_non_row (MOID (src))) {
    static char fn[NAME_SIZE];
    char idf[NAME_SIZE], pop[NAME_SIZE];
    NODE_T *q = stems_from (dst, IDENTIFIER);
// Declare.
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("void_", MOID (SUB (p)), "_assign"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    if (signed_in (BOOK_DEREF, L_DECLARE, NSYMBOL (q)) == NO_BOOK) {
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (p));
      (void) add_declaration (&A68_OPT (root_idf), inline_mode (SUB_MOID (dst)), 1, idf);
      sign_in (BOOK_DEREF, L_DECLARE, NSYMBOL (q), NULL, NUMBER (p));
    } else {
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (signed_in (BOOK_DEREF, L_DECLARE, NSYMBOL (p))));
    }
    inline_unit (dst, out, L_DECLARE);
    inline_unit (src, out, L_DECLARE);
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    print_declarations (out, A68_OPT (root_idf));
// Initialise.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
    inline_unit (dst, out, L_EXECUTE);
    if (signed_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q)) == NO_BOOK) {
      if (BODY (TAX (q)) != NO_TAG) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (%s *) LOCAL_ADDRESS (", idf, inline_mode (SUB_MOID (dst))));
        inline_unit (dst, out, L_YIELD);
        undent (out, ");\n");
        sign_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q), NULL, NUMBER (p));
      } else {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = DEREF (%s, ", idf, inline_mode (SUB_MOID (dst))));
        inline_unit (dst, out, L_YIELD);
        undent (out, ");\n");
        sign_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q), NULL, NUMBER (p));
      }
    }
    inline_unit (src, out, L_EXECUTE);
    optimise_assign (src, out, idf);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile identity-relation.

static char *optimise_identity_relation (NODE_T * p, FILE_T out, int compose_fun)
{
#define GOOD(p) (stems_from (p, IDENTIFIER) != NO_NODE && IS (MOID (stems_from ((p), IDENTIFIER)), REF_SYMBOL))
  NODE_T *lhs = SUB (p);
  NODE_T *op = NEXT (lhs);
  NODE_T *rhs = NEXT (op);
  if (GOOD (lhs) && GOOD (rhs)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_identity"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_identity_relation (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_identity_relation (p, out, L_EXECUTE);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "PUSH_VALUE (p, "));
    inline_identity_relation (p, out, L_YIELD);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", A68_BOOL);\n"));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else if (GOOD (lhs) && stems_from (rhs, NIHIL) != NO_NODE) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_identity"), "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_identity_relation (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_identity_relation (p, out, L_EXECUTE);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "PUSH_VALUE (p, "));
    inline_identity_relation (p, out, L_YIELD);
    undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", A68_BOOL);\n"));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
#undef GOOD
}

//! @brief Compile closed clause.

static void optimise_declaration_list (NODE_T * p, FILE_T out, int *decs, char *pop)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case MODE_DECLARATION:
    case PROCEDURE_DECLARATION:
    case BRIEF_OPERATOR_DECLARATION:
    case PRIORITY_DECLARATION:
      {
// No action needed.
        (*decs)++;
        return;
      }
    case OPERATOR_DECLARATION:
      {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "genie_operator_dec (_NODE_ (%d));", NUMBER (SUB (p))));
        inline_comment_source (p, out);
        undent (out, NEWLINE_STRING);
        (*decs)++;
        break;
      }
    case IDENTITY_DECLARATION:
      {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "genie_identity_dec (_NODE_ (%d));", NUMBER (SUB (p))));
        inline_comment_source (p, out);
        undent (out, NEWLINE_STRING);
        (*decs)++;
        break;
      }
    case VARIABLE_DECLARATION:
      {
        char declarer[NAME_SIZE];
        (void) make_name (declarer, DEC, "", NUMBER (SUB (p)));
        indent (out, "{");
        inline_comment_source (p, out);
        undent (out, NEWLINE_STRING);
        A68_OPT (indentation)++;
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "NODE_T *%s = NO_NODE;\n", declarer));
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "genie_variable_dec (_NODE_ (%d), &%s, A68_SP);\n", NUMBER (SUB (p)), declarer));
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
        A68_OPT (indentation)--;
        indent (out, "}\n");
        (*decs)++;
        break;
      }
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "genie_proc_variable_dec (_NODE_ (%d));", NUMBER (SUB (p))));
        inline_comment_source (p, out);
        undent (out, NEWLINE_STRING);
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
        (*decs)++;
        break;
      }
    default:
      {
        optimise_declaration_list (SUB (p), out, decs, pop);
        break;
      }
    }
  }
}

//! @brief Compile closed clause.

static void optimise_serial_clause (NODE_T * p, FILE_T out, NODE_T ** last, int *units, int *decs, char *pop, int compose_fun)
{
  for (; p != NO_NODE && A68_OPT (code_errors) == 0; FORWARD (p)) {
    if (compose_fun == A68_MAKE_OTHERS) {
      if (IS (p, UNIT)) {
        (*units)++;
      }
      if (IS (p, DECLARATION_LIST)) {
        (*decs)++;
      }
      if (IS (p, UNIT) || IS (p, DECLARATION_LIST)) {
        if (optimise_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
          if (IS (p, UNIT) && IS (SUB (p), TERTIARY)) {
            optimise_units (SUB_SUB (p), out);
          } else {
            optimise_units (SUB (p), out);
          }
        } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
          COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
          a68_free (COMPILE_NAME (GINFO (p)));
          COMPILE_NAME (GINFO (p)) = new_string (COMPILE_NAME (GINFO (SUB (p))), NO_TEXT);
        }
        return;
      } else {
        optimise_serial_clause (SUB (p), out, last, units, decs, pop, compose_fun);
      }
    } else
      switch (ATTRIBUTE (p)) {
      case UNIT:
        {
          (*last) = p;
          CODE_EXECUTE (p);
          inline_comment_source (p, out);
          undent (out, NEWLINE_STRING);
          (*units)++;
          return;
        }
      case SEMI_SYMBOL:
        {
          if (IS (*last, UNIT) && MOID (*last) == M_VOID) {
            break;
          } else if (IS (*last, DECLARATION_LIST)) {
            break;
          } else {
            indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
          }
          break;
        }
      case DECLARATION_LIST:
        {
          (*last) = p;
          optimise_declaration_list (SUB (p), out, decs, pop);
          break;
        }
      default:
        {
          optimise_serial_clause (SUB (p), out, last, units, decs, pop, compose_fun);
          break;
        }
      }
  }
}

//! @brief Embed serial clause.

static void embed_serial_clause (NODE_T * p, FILE_T out, char *pop)
{
  NODE_T *last = NO_NODE;
  int units = 0, decs = 0;
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_STATIC_FRAME (_NODE_ (%d));\n", NUMBER (p)));
  init_static_frame (out, p);
  optimise_serial_clause (p, out, &last, &units, &decs, pop, A68_MAKE_FUNCTION);
  indent (out, "CLOSE_FRAME;\n");
}

//! @brief Compile code clause.

static char *optimise_code_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  comment_source (p, out);
  (void) make_name (fn, "code", "", NUMBER (p));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  embed_code_clause (SUB (p), out);
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "code", "", NUMBER (p));
    write_fun_postlude (p, out, fn);
  }
  return fn;
}

//! @brief Compile closed clause.

static char *optimise_closed_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *sc = NEXT_SUB (p);
  if (MOID (p) == M_VOID && LABELS (TABLE (sc)) == NO_TAG) {
    static char fn[NAME_SIZE];
    char pop[NAME_SIZE];
    int units = 0, decs = 0;
    NODE_T *last = NO_NODE;
    optimise_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, "closed", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    print_declarations (out, A68_OPT (root_idf));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
    embed_serial_clause (sc, out, pop);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "closed", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile collateral clause.

static char *optimise_collateral_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p) && IS (MOID (p), STRUCT_SYMBOL)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "collateral", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_collateral_units (NEXT_SUB (p), out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_collateral_units (NEXT_SUB (p), out, L_EXECUTE);
    inline_collateral_units (NEXT_SUB (p), out, L_YIELD);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "collateral", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile conditional clause.

static char *optimise_basic_conditional (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  NODE_T *q = SUB (p);
  if (!(basic_mode (MOID (p)) || MOID (p) == M_VOID)) {
    return NO_TEXT;
  }
  p = q;
  if (!basic_conditional (p)) {
    return NO_TEXT;
  }
  comment_source (p, out);
  (void) make_name (fn, "conditional", "", NUMBER (q));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (q, out, fn);
  }
// Collect declarations.
  if (IS (p, IF_PART) || IS (p, OPEN_PART)) {
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (SUB (NEXT_SUB (p)), out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (SUB (NEXT_SUB (p)), out, L_EXECUTE);
    indent (out, "if (");
    inline_unit (SUB (NEXT_SUB (p)), out, L_YIELD);
    undent (out, ") {\n");
    A68_OPT (indentation)++;
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
  }
  FORWARD (p);
  if (IS (p, THEN_PART) || IS (p, CHOICE)) {
    int pop = A68_OPT (cse_pointer);
    (void) optimise_unit (SUB (NEXT_SUB (p)), out, A68_MAKE_NOTHING);
    A68_OPT (indentation)--;
    A68_OPT (cse_pointer) = pop;
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
  }
  FORWARD (p);
  if (IS (p, ELSE_PART) || IS (p, CHOICE)) {
    int pop = A68_OPT (cse_pointer);
    indent (out, "} else {\n");
    A68_OPT (indentation)++;
    (void) optimise_unit (SUB (NEXT_SUB (p)), out, A68_MAKE_NOTHING);
    A68_OPT (indentation)--;
    A68_OPT (cse_pointer) = pop;
  }
// Done.
  indent (out, "}\n");
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "conditional", "", NUMBER (q));
    write_fun_postlude (q, out, fn);
  }
  return fn;
}

//! @brief Compile conditional clause.

static char *optimise_conditional_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  char pop[NAME_SIZE];
  int units = 0, decs = 0;
  NODE_T *q, *last;
// We only compile IF basic unit or ELIF basic unit, so we save on opening frames.
// Check worthiness of the clause.
  if (MOID (p) != M_VOID) {
    return NO_TEXT;
  }
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    if (!basic_serial (NEXT_SUB (q), 1)) {
      return NO_TEXT;
    }
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      if (LABELS (TABLE (NEXT_SUB (q))) != NO_TAG) {
        return NO_TEXT;
      }
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL, STOP)) {
      FORWARD (q);
    }
  }
// Generate embedded units.
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      last = NO_NODE;
      units = decs = 0;
      optimise_serial_clause (NEXT_SUB (q), out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL, STOP)) {
      FORWARD (q);
    }
  }
// Prep and Dec.
  (void) make_name (fn, "conditional", "", NUMBER (p));
  (void) make_name (pop, PUP, "", NUMBER (p));
  comment_source (p, out);
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  A68_OPT (root_idf) = NO_DEC;
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    inline_unit (SUB (NEXT_SUB (q)), out, L_DECLARE);
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL, STOP)) {
      FORWARD (q);
    }
  }
  (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
  print_declarations (out, A68_OPT (root_idf));
// Generate the function body.
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    inline_unit (SUB (NEXT_SUB (q)), out, L_EXECUTE);
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL, STOP)) {
      FORWARD (q);
    }
  }
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    BOOL_T else_part = A68_FALSE;
    if (is_one_of (q, IF_PART, OPEN_PART, STOP)) {
      indent (out, "if (");
    } else {
      indent (out, "} else if (");
    }
    inline_unit (SUB (NEXT_SUB (q)), out, L_YIELD);
    undent (out, ") {\n");
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      if (else_part) {
        indent (out, "} else {\n");
      }
      A68_OPT (indentation)++;
      embed_serial_clause (NEXT_SUB (q), out, pop);
      A68_OPT (indentation)--;
      else_part = A68_TRUE;
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL, STOP)) {
      FORWARD (q);
    }
  }
  indent (out, "}\n");
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "conditional", "", NUMBER (p));
    write_fun_postlude (p, out, fn);
  }
  return fn;
}

//! @brief Compile unit from integral-case in-part.

BOOL_T optimise_int_case_units (NODE_T * p, FILE_T out, NODE_T * sym, int k, int *count, int compose_fun)
{
  if (p == NO_NODE) {
    return A68_FALSE;
  } else {
    if (IS (p, UNIT)) {
      if (k == *count) {
        if (compose_fun == A68_MAKE_FUNCTION) {
          indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "case %d: {\n", k));
          A68_OPT (indentation)++;
          indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_STATIC_FRAME (_NODE_ (%d));\n", NUMBER (sym)));
          CODE_EXECUTE (p);
          inline_comment_source (p, out);
          undent (out, NEWLINE_STRING);
          indent (out, "CLOSE_FRAME;\n");
          indent (out, "break;\n");
          A68_OPT (indentation)--;
          indent (out, "}\n");
        } else if (compose_fun == A68_MAKE_OTHERS) {
          if (optimise_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
            if (IS (p, UNIT) && IS (SUB (p), TERTIARY)) {
              optimise_units (SUB_SUB (p), out);
            } else {
              optimise_units (SUB (p), out);
            }
          } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
            COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
            a68_free (COMPILE_NAME (GINFO (p)));
            COMPILE_NAME (GINFO (p)) = new_string (COMPILE_NAME (GINFO (SUB (p))), NO_TEXT);
          }
        }
        return A68_TRUE;
      } else {
        (*count)++;
        return A68_FALSE;
      }
    } else {
      if (optimise_int_case_units (SUB (p), out, sym, k, count, compose_fun)) {
        return A68_TRUE;
      } else {
        return optimise_int_case_units (NEXT (p), out, sym, k, count, compose_fun);
      }
    }
  }
}

//! @brief Compile integral-case-clause.

static char *optimise_int_case_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  char pop[NAME_SIZE];
  int units = 0, decs = 0, k = 0, count = 0;
  NODE_T *q, *last;
// We only compile CASE basic unit.
// Check worthiness of the clause.
  if (MOID (p) != M_VOID) {
    return NO_TEXT;
  }
  q = SUB (p);
  if (q != NO_NODE && is_one_of (q, CASE_PART, OPEN_PART, STOP)) {
    if (!basic_serial (NEXT_SUB (q), 1)) {
      return NO_TEXT;
    }
    FORWARD (q);
  } else {
    return NO_TEXT;
  }
  while (q != NO_NODE && is_one_of (q, CASE_IN_PART, OUT_PART, CHOICE, STOP)) {
    if (LABELS (TABLE (NEXT_SUB (q))) != NO_TAG) {
      return NO_TEXT;
    }
    FORWARD (q);
  }
  if (q != NO_NODE && is_one_of (q, ESAC_SYMBOL, CLOSE_SYMBOL, STOP)) {
    FORWARD (q);
  } else {
    return NO_TEXT;
  }
// Generate embedded units.
  q = SUB (p);
  if (q != NO_NODE && is_one_of (q, CASE_PART, OPEN_PART, STOP)) {
    FORWARD (q);
    if (q != NO_NODE && is_one_of (q, CASE_IN_PART, CHOICE, STOP)) {
      last = NO_NODE;
      units = decs = 0;
      k = 0;
      do {
        count = 1;
        k++;
      } while (optimise_int_case_units (NEXT_SUB (q), out, NO_NODE, k, &count, A68_MAKE_OTHERS));
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, OUT_PART, CHOICE, STOP)) {
      last = NO_NODE;
      units = decs = 0;
      optimise_serial_clause (NEXT_SUB (q), out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
      FORWARD (q);
    }
  }
// Prep and Dec.
  (void) make_name (pop, PUP, "", NUMBER (p));
  comment_source (p, out);
  (void) make_name (fn, "case", "", NUMBER (p));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  A68_OPT (root_idf) = NO_DEC;
  q = SUB (p);
  inline_unit (SUB (NEXT_SUB (q)), out, L_DECLARE);
  (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
  print_declarations (out, A68_OPT (root_idf));
// Generate the function body.
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
  q = SUB (p);
  inline_unit (SUB (NEXT_SUB (q)), out, L_EXECUTE);
  indent (out, "switch (");
  inline_unit (SUB (NEXT_SUB (q)), out, L_YIELD);
  undent (out, ") {\n");
  A68_OPT (indentation)++;
  FORWARD (q);
  k = 0;
  do {
    count = 1;
    k++;
  } while (optimise_int_case_units (NEXT_SUB (q), out, SUB (q), k, &count, A68_MAKE_FUNCTION));
  FORWARD (q);
  if (q != NO_NODE && is_one_of (q, OUT_PART, CHOICE, STOP)) {
    indent (out, "default: {\n");
    A68_OPT (indentation)++;
    embed_serial_clause (NEXT_SUB (q), out, pop);
    indent (out, "break;\n");
    A68_OPT (indentation)--;
    indent (out, "}\n");
  }
  A68_OPT (indentation)--;
  indent (out, "}\n");
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "case", "", NUMBER (p));
    write_fun_postlude (p, out, fn);
  }
  return fn;
}

//! @brief Compile loop clause.

static char *optimise_loop_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *for_part = NO_NODE, *from_part = NO_NODE, *by_part = NO_NODE, *to_part = NO_NODE, *downto_part = NO_NODE, *while_part = NO_NODE, *sc;
  static char fn[NAME_SIZE];
  char idf[NAME_SIZE], z[NAME_SIZE], pop[NAME_SIZE];
  NODE_T *q = SUB (p), *last = NO_NODE;
  int units, decs;
  BOOL_T gc, need_reinit;
// FOR identifier.
  if (IS (q, FOR_PART)) {
    for_part = NEXT_SUB (q);
    FORWARD (q);
  }
// FROM unit.
  if (IS (p, FROM_PART)) {
    from_part = NEXT_SUB (q);
    if (!basic_unit (from_part)) {
      return NO_TEXT;
    }
    FORWARD (q);
  }
// BY unit.
  if (IS (q, BY_PART)) {
    by_part = NEXT_SUB (q);
    if (!basic_unit (by_part)) {
      return NO_TEXT;
    }
    FORWARD (q);
  }
// TO unit, DOWNTO unit.
  if (IS (q, TO_PART)) {
    if (IS (SUB (q), TO_SYMBOL)) {
      to_part = NEXT_SUB (q);
      if (!basic_unit (to_part)) {
        return NO_TEXT;
      }
    } else if (IS (SUB (q), DOWNTO_SYMBOL)) {
      downto_part = NEXT_SUB (q);
      if (!basic_unit (downto_part)) {
        return NO_TEXT;
      }
    }
    FORWARD (q);
  }
// WHILE DO OD is not yet supported.
  if (IS (q, WHILE_PART)) {
    return NO_TEXT;
  }
// DO UNTIL OD is not yet supported.
  if (IS (q, DO_PART) || IS (q, ALT_DO_PART)) {
    sc = q = NEXT_SUB (q);
    if (IS (q, SERIAL_CLAUSE)) {
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, UNTIL_PART)) {
      return NO_TEXT;
    }
  } else {
    return NO_TEXT;
  }
  if (LABELS (TABLE (sc)) != NO_TAG) {
    return NO_TEXT;
  }
// Loop clause is compiled.
  units = decs = 0;
  optimise_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
  gc = (decs > 0);
  comment_source (p, out);
  (void) make_name (fn, "loop", "", NUMBER (p));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  A68_OPT (root_idf) = NO_DEC;
  (void) make_name (idf, "k", "", NUMBER (p));
  (void) add_declaration (&A68_OPT (root_idf), "INT_T", 0, idf);
  if (for_part != NO_NODE) {
    (void) make_name (z, "z", "", NUMBER (p));
    (void) add_declaration (&A68_OPT (root_idf), "A68_INT", 1, z);
  }
  if (from_part != NO_NODE) {
    inline_unit (from_part, out, L_DECLARE);
  }
  if (by_part != NO_NODE) {
    inline_unit (by_part, out, L_DECLARE);
  }
  if (to_part != NO_NODE) {
    inline_unit (to_part, out, L_DECLARE);
  }
  if (downto_part != NO_NODE) {
    inline_unit (downto_part, out, L_DECLARE);
  }
  if (while_part != NO_NODE) {
    inline_unit (SUB (NEXT_SUB (while_part)), out, L_DECLARE);
  }
  (void) make_name (pop, PUP, "", NUMBER (p));
  (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
  print_declarations (out, A68_OPT (root_idf));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
  if (from_part != NO_NODE) {
    inline_unit (from_part, out, L_EXECUTE);
  }
  if (by_part != NO_NODE) {
    inline_unit (by_part, out, L_EXECUTE);
  }
  if (to_part != NO_NODE) {
    inline_unit (to_part, out, L_EXECUTE);
  }
  if (downto_part != NO_NODE) {
    inline_unit (downto_part, out, L_EXECUTE);
  }
  if (while_part != NO_NODE) {
    inline_unit (SUB (NEXT_SUB (while_part)), out, L_EXECUTE);
  }
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_STATIC_FRAME (_NODE_ (%d));\n", NUMBER (sc)));
  init_static_frame (out, sc);
  if (for_part != NO_NODE) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (_NODE_ (%d)))));\n", z, NUMBER (for_part)));
  }
// The loop in C.
// Initialisation.
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "for (%s = ", idf));
  if (from_part == NO_NODE) {
    undent (out, "1");
  } else {
    inline_unit (from_part, out, L_YIELD);
  }
  undent (out, "; ");
// Condition.
  if (to_part == NO_NODE && downto_part == NO_NODE && while_part == NO_NODE) {
    undent (out, "A68_TRUE");
  } else {
    undent (out, idf);
    if (to_part != NO_NODE) {
      undent (out, " <= ");
    } else if (downto_part != NO_NODE) {
      undent (out, " >= ");
    }
    inline_unit (to_part, out, L_YIELD);
  }
  undent (out, "; ");
// Increment.
  if (by_part == NO_NODE) {
    undent (out, idf);
    if (to_part != NO_NODE) {
      undent (out, " ++");
    } else if (downto_part != NO_NODE) {
      undent (out, " --");
    } else {
      undent (out, " ++");
    }
  } else {
    undent (out, idf);
    if (to_part != NO_NODE) {
      undent (out, " += ");
    } else if (downto_part != NO_NODE) {
      undent (out, " -= ");
    } else {
      undent (out, " += ");
    }
    inline_unit (by_part, out, L_YIELD);
  }
  undent (out, ") {\n");
  A68_OPT (indentation)++;
  if (gc) {
    indent (out, "// PREEMPTIVE_GC (DEFAULT_PREEMPTIVE);\n");
  }
  if (for_part != NO_NODE) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_STATUS_ (%s) = INIT_MASK;\n", z));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s) = %s;\n", z, idf));
  }
  units = decs = 0;
  optimise_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_FUNCTION);
// Re-initialise if necessary.
  need_reinit = (BOOL_T) (AP_INCREMENT (TABLE (sc)) > 0 || need_initialise_frame (sc));
  if (need_reinit) {
    indent (out, "if (");
    if (to_part == NO_NODE && downto_part == NO_NODE) {
      undent (out, "A68_TRUE");
    } else {
      undent (out, idf);
      if (to_part != NO_NODE) {
        undent (out, " < ");
      } else if (downto_part != NO_NODE) {
        undent (out, " > ");
      }
      inline_unit (to_part, out, L_YIELD);
    }
    undent (out, ") {\n");
    A68_OPT (indentation)++;
    if (AP_INCREMENT (TABLE (sc)) > 0) {
#if (A68_LEVEL >= 3)
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "FRAME_CLEAR (%llu);\n", AP_INCREMENT (TABLE (sc))));
#else
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "FRAME_CLEAR (%u);\n", AP_INCREMENT (TABLE (sc))));
#endif
    }
    if (need_initialise_frame (sc)) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "initialise_frame (_NODE_ (%d));\n", NUMBER (sc)));
    }
    A68_OPT (indentation)--;
    indent (out, "}\n");
  }
// End of loop.
  A68_OPT (indentation)--;
  indent (out, "}\n");
  indent (out, "CLOSE_FRAME;\n");
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "loop", "", NUMBER (p));
    write_fun_postlude (p, out, fn);
  }
  return fn;
}

//
// Non-optimising
//

//! @brief Compile denotation.

static char *compile_denotation (NODE_T * p, FILE_T out)
{
  if (primitive_mode (MOID (p))) {
    static char fn[NAME_SIZE], N[NAME_SIZE];
    int action = UNIC_MAKE_ALT;
    comment_source (p, out);
    fn[0] = '\0';
    if (MOID (p) == M_INT) {
      char *end;
      UNSIGNED_T z = (UNSIGNED_T) a68_strtoi (NSYMBOL (p), &end, 10);
      ASSERT (snprintf (N, NAME_SIZE, A68_LX "_", z) >= 0);
      (void) make_unic_name (fn, moid_with_name ("", MOID (p), "_denotation"), "", N);
    } else if (MOID (p) == M_REAL) {
      char *V;
      char W[NAME_SIZE];
      int k;
      A68_SP = 0;
      PUSH_UNION (p, M_REAL);
      push_unit (p);
      INCREMENT_STACK_POINTER (p, SIZE (M_NUMBER) - (A68_UNION_SIZE + SIZE (M_REAL)));
      PUSH_VALUE (p, REAL_WIDTH + EXP_WIDTH + 5, A68_INT);
      PUSH_VALUE (p, REAL_WIDTH, A68_INT);
      PUSH_VALUE (p, EXP_WIDTH + 1, A68_INT);
      PUSH_VALUE (p, 3, A68_INT);
      V = real (p);
      for (k = 0; V[0] != '\0'; V++) {
        if (IS_ALNUM (V[0])) {
          W[k++] = TO_LOWER (V[0]);
          W[k] = '\0';
        }
        if (V[0] == '.' || V[0] == '-') {
          W[k++] = '_';
          W[k] = '\0';
        }
      }
      (void) make_unic_name (fn, moid_with_name ("", MOID (p), "_denotation"), "", W);
    } else if (MOID (p) == M_BOOL) {
      (void) make_unic_name (fn, moid_with_name ("", MOID (p), "_denotation"), "", NSYMBOL (SUB (p)));
    } else if (MOID (p) == M_CHAR) {
      ASSERT (snprintf (N, NAME_SIZE, "%02x_", NSYMBOL (SUB (p))[0]) >= 0);
      (void) make_unic_name (fn, moid_with_name ("", MOID (p), "_denotation"), "", N);
    }
    if (fn[0] != '\0') {
      sign_in_name (fn, &action);
      if (action == UNIC_EXISTS) {
        return fn;
      }
    }
    if (action == UNIC_MAKE_NEW || action == UNIC_MAKE_ALT) {
      if (action == UNIC_MAKE_ALT) {
        (void) make_name (fn, moid_with_name ("", MOID (p), "_denotation_alt"), "", NUMBER (p));
      }
      write_fun_prelude (p, out, fn);
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "PUSH_VALUE (p, "));
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, ", %s);\n", inline_mode (MOID (p))));
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

static char *compile_cast (NODE_T * p, FILE_T out)
{
  if (folder_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_cast"), "", NUMBER (p));
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (NEXT_SUB (p), out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (NEXT_SUB (p), out, L_EXECUTE);
    optimise_push (NEXT_SUB (p), out);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile identifier.

static char *compile_identifier (NODE_T * p, FILE_T out)
{
  if (folder_mode (MOID (p))) {
    static char fn[NAME_SIZE];
    int action = UNIC_MAKE_ALT;
    char N[NAME_SIZE];
// Some identifiers in standenv cannot be pushed.
// Examples are cputime, or clock that are procedures in disguise.
    if (A68_STANDENV_PROC (TAX (p))) {
      int k;
      BOOL_T ok = A68_FALSE;
      for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k++) {
        if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
          ok = A68_TRUE;
        }
      }
      if (!ok) {
        return NO_TEXT;
      }
    }
// Push the identifier.
    ASSERT (snprintf (N, NAME_SIZE, "%d_%d_" A68_LU, NUM (TABLE (TAX (p))), LEVEL (GINFO (p)), OFFSET (TAX (p))) >= 0);
    comment_source (p, out);
    fn[0] = '\0';
    (void) make_unic_name (fn, moid_with_name ("", MOID (p), "_identifier"), "", N);
    sign_in_name (fn, &action);
    if (action == UNIC_EXISTS) {
      return fn;
    }
    if (action == UNIC_MAKE_NEW || action == UNIC_MAKE_ALT) {
      if (action == UNIC_MAKE_ALT) {
        (void) make_name (fn, moid_with_name ("", MOID (p), "_identifier_alt"), "", NUMBER (p));
      }
      write_fun_prelude (p, out, fn);
      A68_OPT (root_idf) = NO_DEC;
      inline_unit (p, out, L_DECLARE);
      print_declarations (out, A68_OPT (root_idf));
      inline_unit (p, out, L_EXECUTE);
      optimise_push (p, out);
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile dereference identifier.

static char *compile_dereference_identifier (NODE_T * p, FILE_T out)
{
  if (folder_mode (MOID (p))) {
    static char fn[NAME_SIZE];
    int action = UNIC_MAKE_ALT;
    char N[NAME_SIZE];
    NODE_T *q = SUB (p);
    ASSERT (snprintf (N, NAME_SIZE, "%d_%d_" A68_LU, NUM (TABLE (TAX (q))), LEVEL (GINFO (q)), OFFSET (TAX (q))) >= 0);
    comment_source (p, out);
    fn[0] = '\0';
    (void) make_unic_name (fn, moid_with_name ("deref_REF_", MOID (p), "_identifier"), "", N);
    sign_in_name (fn, &action);
    if (action == UNIC_EXISTS) {
      return fn;
    }
    if (action == UNIC_MAKE_NEW || action == UNIC_MAKE_ALT) {
      if (action == UNIC_MAKE_ALT) {
        (void) make_name (fn, moid_with_name ("deref_REF_", MOID (p), "_identifier_alt"), "", NUMBER (p));
      }
      write_fun_prelude (p, out, fn);
      A68_OPT (root_idf) = NO_DEC;
      inline_unit (p, out, L_DECLARE);
      print_declarations (out, A68_OPT (root_idf));
      inline_unit (p, out, L_EXECUTE);
      optimise_push (p, out);
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile formula.

static char *compile_formula (NODE_T * p, FILE_T out)
{
  if (folder_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_formula"), "", NUMBER (p));
    write_fun_prelude (p, out, fn);
    if (OPTION_COMPILE_CHECK (&A68_JOB) && !constant_unit (p)) {
      if (MOID (p) == M_REAL || MOID (p) == M_COMPLEX) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_REAL * _st_ = (A68_REAL *) STACK_TOP;\n"));
      }
    }
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    if (OPTION_COMPILE_CHECK (&A68_JOB) && !constant_unit (p)) {
      if (folder_mode (MOID (p))) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "errno = 0;\n"));
      }
    }
    inline_unit (p, out, L_EXECUTE);
    optimise_push (p, out);
    if (OPTION_COMPILE_CHECK (&A68_JOB) && !constant_unit (p)) {
      if (MOID (p) == M_INT) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MATH_RTE (p, errno != 0, M_INT, NO_TEXT);\n"));
      }
      if (MOID (p) == M_REAL) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MATH_RTE (p, errno != 0, M_REAL, NO_TEXT);\n"));
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "CHECK_REAL (p, _VALUE_ (_st_));\n"));
      }
      if (MOID (p) == M_BITS) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MATH_RTE (p, errno != 0, M_BITS, NO_TEXT);\n"));
      }
      if (MOID (p) == M_COMPLEX) {
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "MATH_RTE (p, errno != 0, M_COMPLEX, NO_TEXT);\n"));
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "CHECK_REAL (p, _VALUE_ (&(_st_[0])));\n"));
        indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "CHECK_REAL (p, _VALUE_ (&(_st_[1])));\n"));
      }
    }
    write_fun_postlude (p, out, fn);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile call.

static char *compile_call (NODE_T * p, FILE_T out)
{
  NODE_T *proc = SUB (p);
  NODE_T *args = NEXT (proc);
  NODE_T *idf = stems_from (proc, IDENTIFIER);
  if (idf == NO_NODE) {
    return NO_TEXT;
  } else if (!(SUB_MOID (proc) == M_VOID || basic_mode (SUB_MOID (proc)))) {
    return NO_TEXT;
  } else if (DIM (MOID (proc)) == 0) {
    return NO_TEXT;
  } else if (A68_STANDENV_PROC (TAX (idf))) {
    if (basic_call (p)) {
      static char fun[NAME_SIZE];
      comment_source (p, out);
      (void) make_name (fun, moid_with_name ("", SUB_MOID (proc), "_call"), "", NUMBER (p));
      write_fun_prelude (p, out, fun);
      A68_OPT (root_idf) = NO_DEC;
      inline_unit (p, out, L_DECLARE);
      print_declarations (out, A68_OPT (root_idf));
      inline_unit (p, out, L_EXECUTE);
      optimise_push (p, out);
      write_fun_postlude (p, out, fun);
      return fun;
    } else {
      return NO_TEXT;
    }
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return NO_TEXT;
  } else if (DIM (PARTIAL_PROC (GINFO (proc))) != 0) {
    return NO_TEXT;
  } else if (!basic_argument (args)) {
    return NO_TEXT;
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE], pop[NAME_SIZE];
    int size;
// Declare.
    (void) make_name (fun, FUN, "", NUMBER (proc));
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", SUB_MOID (proc), "_call"), "", NUMBER (p));
    write_fun_prelude (p, out, fn);
// Compute arguments.
    size = 0;
    A68_OPT (root_idf) = NO_DEC;
    inline_arguments (args, out, L_DECLARE, &size);
    (void) add_declaration (&A68_OPT (root_idf), "ADDR_T", 0, pop);
    (void) add_declaration (&A68_OPT (root_idf), "A68_PROCEDURE", 1, fun);
    (void) add_declaration (&A68_OPT (root_idf), "NODE_T", 1, "body");
    print_declarations (out, A68_OPT (root_idf));
// Initialise.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s = A68_SP;\n", pop));
    inline_arguments (args, out, L_INITIALISE, &size);
    get_stack (idf, out, fun, "A68_PROCEDURE");
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", fun));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
    size = 0;
    inline_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    inline_arguments (args, out, L_YIELD, &size);
// Execute procedure.
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s;\n", pop));
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT_NEXT (body));\n");
    indent (out, "if (A68_FP == A68_MON (finish_frame_pointer)) {\n");
    A68_OPT (indentation)++;
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    A68_OPT (indentation)--;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    write_fun_postlude (p, out, fn);
    return fn;
  }
}

//
// Driver routines
//

//! @brief Optimise units.

static char *optimise_unit (NODE_T * p, FILE_T out, BOOL_T compose_fun)
{
#define COMPILE(p, out, fun, compose_fun) {\
  char * fn = (fun) (p, out, compose_fun);\
  if (compose_fun == A68_MAKE_FUNCTION && fn != NO_TEXT) {\
    ABEND (strlen (fn) >= NAME_SIZE, ERROR_INTERNAL_CONSISTENCY, __func__);\
    COMPILE_NAME (GINFO (p)) = new_string (fn, NO_TEXT);\
    if (SUB (p) != NO_NODE && COMPILE_NODE (GINFO (SUB (p))) > 0) {\
      COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));\
    } else {\
      COMPILE_NODE (GINFO (p)) = NUMBER (p);\
    }\
    return COMPILE_NAME (GINFO (p));\
  } else {\
    COMPILE_NAME (GINFO (p)) = NO_TEXT;\
    COMPILE_NODE (GINFO (p)) = 0;\
    return NO_TEXT;\
  }}

  LOW_SYSTEM_STACK_ALERT (p);
  if (p == NO_NODE) {
    return NO_TEXT;
  } else if (COMPILE_NAME (GINFO (p)) != NO_TEXT) {
    return NO_TEXT;
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, ENCLOSED_CLAUSE, STOP)) {
    COMPILE (SUB (p), out, optimise_unit, compose_fun);
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 3) {
// Control structure.
    if (IS (p, CLOSED_CLAUSE)) {
      COMPILE (p, out, optimise_closed_clause, compose_fun);
    } else if (IS (p, COLLATERAL_CLAUSE)) {
      COMPILE (p, out, optimise_collateral_clause, compose_fun);
    } else if (IS (p, CONDITIONAL_CLAUSE)) {
      char *fn2 = optimise_basic_conditional (p, out, compose_fun);
      if (compose_fun == A68_MAKE_FUNCTION && fn2 != NO_TEXT) {
        ABEND (strlen (fn2) >= NAME_SIZE, ERROR_INTERNAL_CONSISTENCY, __func__);
        COMPILE_NAME (GINFO (p)) = new_string (fn2, NO_TEXT);
        if (SUB (p) != NO_NODE && COMPILE_NODE (GINFO (SUB (p))) > 0) {
          COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
        } else {
          COMPILE_NODE (GINFO (p)) = NUMBER (p);
        }
        return COMPILE_NAME (GINFO (p));
      } else {
        COMPILE (p, out, optimise_conditional_clause, compose_fun);
      }
    } else if (IS (p, CASE_CLAUSE)) {
      COMPILE (p, out, optimise_int_case_clause, compose_fun);
    } else if (IS (p, LOOP_CLAUSE)) {
      COMPILE (p, out, optimise_loop_clause, compose_fun);
    }
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 2) {
// Simple constructions.
    if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), IDENTIFIER) != NO_NODE) {
      COMPILE (p, out, optimise_voiding_assignation_identifier, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), SLICE) != NO_NODE) {
      COMPILE (p, out, optimise_voiding_assignation_slice, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), SELECTION) != NO_NODE) {
      COMPILE (p, out, optimise_voiding_assignation_selection, compose_fun);
    } else if (IS (p, SLICE)) {
      COMPILE (p, out, optimise_slice, compose_fun);
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SLICE) != NO_NODE) {
      COMPILE (p, out, optimise_dereference_slice, compose_fun);
    } else if (IS (p, SELECTION)) {
      COMPILE (p, out, optimise_selection, compose_fun);
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SELECTION) != NO_NODE) {
      COMPILE (p, out, optimise_dereference_selection, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), FORMULA)) {
      COMPILE (SUB (p), out, optimise_voiding_formula, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), MONADIC_FORMULA)) {
      COMPILE (SUB (p), out, optimise_voiding_formula, compose_fun);
    } else if (IS (p, DEPROCEDURING)) {
      COMPILE (p, out, optimise_deproceduring, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), DEPROCEDURING)) {
      COMPILE (p, out, optimise_voiding_deproceduring, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), CALL)) {
      COMPILE (p, out, optimise_voiding_call, compose_fun);
    } else if (IS (p, IDENTITY_RELATION)) {
      COMPILE (p, out, optimise_identity_relation, compose_fun);
    } else if (IS (p, UNITING)) {
      COMPILE (p, out, optimise_uniting, compose_fun);
    }
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 1) {
// Most basic stuff.
    if (IS (p, VOIDING)) {
      COMPILE (SUB (p), out, optimise_unit, compose_fun);
    } else if (IS (p, DENOTATION)) {
      COMPILE (p, out, optimise_denotation, compose_fun);
    } else if (IS (p, CAST)) {
      COMPILE (p, out, optimise_cast, compose_fun);
    } else if (IS (p, IDENTIFIER)) {
      COMPILE (p, out, optimise_identifier, compose_fun);
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), IDENTIFIER) != NO_NODE) {
      COMPILE (p, out, optimise_dereference_identifier, compose_fun);
    } else if (IS (p, MONADIC_FORMULA)) {
      COMPILE (p, out, optimise_formula, compose_fun);
    } else if (IS (p, FORMULA)) {
      COMPILE (p, out, optimise_formula, compose_fun);
    } else if (IS (p, CALL)) {
      COMPILE (p, out, optimise_call, compose_fun);
    }
  }
  if (IS (p, CODE_CLAUSE)) {
    COMPILE (p, out, optimise_code_clause, compose_fun);
  }
  return NO_TEXT;
#undef COMPILE
}

//! @brief Compile unit.

static char *optimise_basic (NODE_T * p, FILE_T out)
{
#define COMPILE(p, out, fun) {\
  char * fn = (fun) (p, out);\
  if (fn != NO_TEXT) {\
    ABEND (strlen (fn) >= NAME_SIZE, ERROR_INTERNAL_CONSISTENCY, __func__);\
    COMPILE_NAME (GINFO (p)) = new_string (fn, NO_TEXT);\
    if (SUB (p) != NO_NODE && COMPILE_NODE (GINFO (SUB (p))) > 0) {\
      COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));\
    } else {\
      COMPILE_NODE (GINFO (p)) = NUMBER (p);\
    }\
    return COMPILE_NAME (GINFO (p));\
  } else {\
    COMPILE_NAME (GINFO (p)) = NO_TEXT;\
    COMPILE_NODE (GINFO (p)) = 0;\
    return NO_TEXT;\
  }}

  LOW_SYSTEM_STACK_ALERT (p);
  if (p == NO_NODE) {
    return NO_TEXT;
  } else if (COMPILE_NAME (GINFO (p)) != NO_TEXT) {
    return NO_TEXT;
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, ENCLOSED_CLAUSE, STOP)) {
    COMPILE (SUB (p), out, optimise_basic);
  }
// Most basic stuff.
  if (IS (p, VOIDING)) {
    COMPILE (SUB (p), out, optimise_basic);
  } else if (IS (p, DENOTATION)) {
    COMPILE (p, out, compile_denotation);
  } else if (IS (p, CAST)) {
    COMPILE (p, out, compile_cast);
  } else if (IS (p, IDENTIFIER)) {
    COMPILE (p, out, compile_identifier);
  } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), IDENTIFIER) != NO_NODE) {
    COMPILE (p, out, compile_dereference_identifier);
  } else if (IS (p, FORMULA)) {
    COMPILE (p, out, compile_formula);
  } else if (IS (p, CALL)) {
    COMPILE (p, out, compile_call);
  }
  return NO_TEXT;
#undef COMPILE
}

//! @brief Optimise units.

void optimise_units (NODE_T * p, FILE_T out)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT) || IS (p, CODE_CLAUSE)) {
      if (optimise_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
        optimise_units (SUB (p), out);
      } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
        COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
        COMPILE_NAME (GINFO (p)) = new_string (COMPILE_NAME (GINFO (SUB (p))), NO_TEXT);
      }
    } else {
      optimise_units (SUB (p), out);
    }
  }
}

//! @brief Compile units.

void optimise_basics (NODE_T * p, FILE_T out)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT) || IS (p, CODE_CLAUSE)) {
      if (optimise_basic (p, out) == NO_TEXT) {
        optimise_basics (SUB (p), out);
      } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
        COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
        COMPILE_NAME (GINFO (p)) = new_string (COMPILE_NAME (GINFO (SUB (p))), NO_TEXT);
      }
    } else {
      optimise_basics (SUB (p), out);
    }
  }
}

//! @brief Compiler driver.

void compiler (FILE_T out)
{
  ADDR_T pop_temp_heap_pointer = A68 (temp_heap_pointer);
  if (OPTION_OPT_LEVEL (&A68_JOB) == NO_OPTIMISE) {
    return;
  }
  A68_OPT (indentation) = 0;
  A68_OPT (code_errors) = 0;
  A68_OPT (procedures) = 0;
  A68_OPT (cse_pointer) = 0;
  A68_OPT (unic_pointer) = 0;
  A68_OPT (root_idf) = NO_DEC;
  A68 (global_level) = INT_MAX;
  A68_GLOBALS = 0;
  get_global_level (SUB (TOP_NODE (&A68_JOB)));
  A68 (max_lex_lvl) = 0;
  genie_preprocess (TOP_NODE (&A68_JOB), &A68 (max_lex_lvl), NULL);
  get_global_level (TOP_NODE (&A68_JOB));
  A68_SP = A68 (stack_start);
  A68 (expr_stack_limit) = A68 (stack_end) - A68 (storage_overhead);
  if (OPTION_COMPILE_CHECK (&A68_JOB)) {
    monadics = monadics_check;
    dyadics = dyadics_check;
    functions = functions_check;
  } else {
    monadics = monadics_nocheck;
    dyadics = dyadics_nocheck;
    functions = functions_nocheck;
  }
  if (OPTION_OPT_LEVEL (&A68_JOB) == OPTIMISE_0) {
// Allow basic optimisation only.
    A68_OPT (OPTION_CODE_LEVEL) = 1;
    write_prelude (out);
    optimise_basics (TOP_NODE (&A68_JOB), out);
  } else {
// Allow all optimisations.
    A68_OPT (OPTION_CODE_LEVEL) = 9;
    write_prelude (out);
    optimise_units (TOP_NODE (&A68_JOB), out);
  }
  ABEND (A68_OPT (indentation) != 0, ERROR_INTERNAL_CONSISTENCY, __func__);
// At the end we discard temporary declarations.
  A68 (temp_heap_pointer) = pop_temp_heap_pointer;
  if (OPTION_VERBOSE (&A68_JOB)) {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s: A68_OPT (procedures)=%d unique-names=%d", A68 (a68_cmd_name), A68_OPT (procedures), A68_OPT (unic_pointer)) >= 0);
    io_close_tty_line ();
    WRITE (STDOUT_FILENO, A68 (output_line));
  }
//
  int k;
  for (k = 0; k < A68_OPT (unic_pointer); k++) {
    a68_free (UNIC_NAME (k));
  }
}
