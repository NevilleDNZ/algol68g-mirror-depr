//! @file plugin.c
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
//! Plugin compiler driver.

// The plugin compiler generates optimised C routines for many units in an Algol 68 source
// program. A68G 1.x contained some general optimised routines. These are
// decommissioned in A68G 2.x that dynamically generates routines depending
// on the source code. The generated routines are compiled on the fly into a 
// dynamic library that is linked by the running interpreter, like a plugin.

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
#include "a68g-plugin.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

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
    gen_basics (TOP_NODE (&A68_JOB), out);
  } else {
// Allow all optimisations.
    A68_OPT (OPTION_CODE_LEVEL) = 9;
    write_prelude (out);
    gen_units (TOP_NODE (&A68_JOB), out);
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

// Pretty printing stuff.

//! @brief Name formatting

char *moid_with_name (char *pre, MOID_T * m, char *post)
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

void indent (FILE_T out, char *str)
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

void undent (FILE_T out, char *str)
{
  if (out == 0) {
    return;
  }
  WRITE (out, str);
}

//! @brief Write indent text.

void indentf (FILE_T out, int ret)
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

void undentf (FILE_T out, int ret)
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

//! @brief Make name.

char *make_unic_name (char *buf, char *name, char *tag, char *ext)
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

char *signed_in_name (char *name)
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

void sign_in_name (char *name, int *action)
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

void sign_in (int action, int phase, char *idf, void *info, int number)
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

BOOK_T *signed_in (int action, int phase, char *idf)
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

char *make_name (char *buf, char *name, char *tag, int n)
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

BOOL_T same_tree (NODE_T * l, NODE_T * r)
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

//! @brief Whether stems from certain attribute.

NODE_T *stems_from (NODE_T * p, int att)
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

// Auxilliary routines for emitting C code.

//! @brief Whether frame needs initialisation.

BOOL_T need_initialise_frame (NODE_T * p)
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

void comment_tree (NODE_T * p, FILE_T out, int *want_space, int *max_print)
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

void comment_source (NODE_T * p, FILE_T out)
{
  int want_space = 0, max_print = 16, ld = -1;
  undentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "\n// %s: %d: ", FILENAME (LINE (INFO (p))), LINE_NUMBER (p)));
  comment_tree (p, out, &want_space, &max_print);
  tree_listing (out, p, 1, LINE (INFO (p)), &ld, A68_TRUE);
  undent (out, "\n");
}

//! @brief Inline comment source line.

void inline_comment_source (NODE_T * p, FILE_T out)
{
  int want_space = 0, max_print = 8;
  undent (out, " // ");
  comment_tree (p, out, &want_space, &max_print);
//  undent (out, " */");
}

//! @brief Write prelude.

void write_prelude (FILE_T out)
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

void init_static_frame (FILE_T out, NODE_T * p)
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

void gen_check_init (NODE_T * p, FILE_T out, char *idf)
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

void get_stack (NODE_T * p, FILE_T out, char *dst, char *cast)
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

void write_fun_prelude (NODE_T * p, FILE_T out, char *fn)
{
  (void) p;
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "\nPROP_T %s (NODE_T *p) {\n", fn));
  A68_OPT (indentation)++;
  indent (out, "PROP_T self;\n");
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "UNIT (&self) = %s;\n", fn));
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "SOURCE (&self) = _NODE_ (%d);\n", NUMBER (p)));
  A68_OPT (cse_pointer) = 0;
}

//! @brief Code function postlude.

void write_fun_postlude (NODE_T * p, FILE_T out, char *fn)
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

char *internal_mode (MOID_T * m)
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

//! @brief Compile denotation.

char *compile_denotation (NODE_T * p, FILE_T out)
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

char *compile_cast (NODE_T * p, FILE_T out)
{
  if (folder_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, moid_with_name ("", MOID (p), "_cast"), "", NUMBER (p));
    A68_OPT (root_idf) = NO_DEC;
    inline_unit (NEXT_SUB (p), out, L_DECLARE);
    print_declarations (out, A68_OPT (root_idf));
    inline_unit (NEXT_SUB (p), out, L_EXECUTE);
    gen_push (NEXT_SUB (p), out);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile identifier.

char *compile_identifier (NODE_T * p, FILE_T out)
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
      gen_push (p, out);
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile dereference identifier.

char *compile_dereference_identifier (NODE_T * p, FILE_T out)
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
      gen_push (p, out);
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile formula.

char *compile_formula (NODE_T * p, FILE_T out)
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
    gen_push (p, out);
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

char *compile_call (NODE_T * p, FILE_T out)
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
      gen_push (p, out);
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

