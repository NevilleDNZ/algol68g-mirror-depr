//! @file plugin-gen.c
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
//! Plugin compiler generator routines.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-genie.h"
#include "a68g-listing.h"
#include "a68g-mp.h"
#include "a68g-optimiser.h"
#include "a68g-plugin.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

//! @brief Compile code clause.

void embed_code_clause (NODE_T * p, FILE_T out)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ROW_CHAR_DENOTATION)) {
      indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s\n", NSYMBOL (p)));
    }
    embed_code_clause (SUB (p), out);
  }
}

//! @brief Compile push.

void gen_push (NODE_T * p, FILE_T out)
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

void gen_assign (NODE_T * p, FILE_T out, char *dst)
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

char *gen_denotation (NODE_T * p, FILE_T out, int compose_fun)
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

char *gen_cast (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (NEXT_SUB (p), out);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile identifier.

char *gen_identifier (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (p, out);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile dereference identifier.

char *gen_dereference_identifier (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (p, out);
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile slice.

char *gen_slice (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile slice.

char *gen_dereference_slice (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile selection.

char *gen_selection (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile selection.

char *gen_dereference_selection (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile formula.

char *gen_formula (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (p, out);
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

char *gen_voiding_formula (NODE_T * p, FILE_T out, int compose_fun)
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

char *gen_uniting (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_push (q, out);
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "A68_SP = %s + %d;\n", pop0, SIZE (u)));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return fn;
  } else {
    return NO_TEXT;
  }
}

//! @brief Compile deproceduring.

char *gen_deproceduring (NODE_T * p, FILE_T out, int compose_fun)
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

char *gen_voiding_deproceduring (NODE_T * p, FILE_T out, int compose_fun)
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

char *gen_call (NODE_T * p, FILE_T out, int compose_fun)
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
      gen_push (p, out);
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

char *gen_voiding_call (NODE_T * p, FILE_T out, int compose_fun)
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

char *gen_voiding_assignation_selection (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_assign (src, out, sel);
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

char *gen_voiding_assignation_slice (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_assign (src, out, drf);
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

char *gen_voiding_assignation_identifier (NODE_T * p, FILE_T out, int compose_fun)
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
    gen_assign (src, out, idf);
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

char *gen_identity_relation (NODE_T * p, FILE_T out, int compose_fun)
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

void gen_declaration_list (NODE_T * p, FILE_T out, int *decs, char *pop)
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
        gen_declaration_list (SUB (p), out, decs, pop);
        break;
      }
    }
  }
}

//! @brief Compile closed clause.

void gen_serial_clause (NODE_T * p, FILE_T out, NODE_T ** last, int *units, int *decs, char *pop, int compose_fun)
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
        if (gen_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
          if (IS (p, UNIT) && IS (SUB (p), TERTIARY)) {
            gen_units (SUB_SUB (p), out);
          } else {
            gen_units (SUB (p), out);
          }
        } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
          COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
          a68_free (COMPILE_NAME (GINFO (p)));
          COMPILE_NAME (GINFO (p)) = new_string (COMPILE_NAME (GINFO (SUB (p))), NO_TEXT);
        }
        return;
      } else {
        gen_serial_clause (SUB (p), out, last, units, decs, pop, compose_fun);
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
          gen_declaration_list (SUB (p), out, decs, pop);
          break;
        }
      default:
        {
          gen_serial_clause (SUB (p), out, last, units, decs, pop, compose_fun);
          break;
        }
      }
  }
}

//! @brief Embed serial clause.

void embed_serial_clause (NODE_T * p, FILE_T out, char *pop)
{
  NODE_T *last = NO_NODE;
  int units = 0, decs = 0;
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "OPEN_STATIC_FRAME (_NODE_ (%d));\n", NUMBER (p)));
  init_static_frame (out, p);
  gen_serial_clause (p, out, &last, &units, &decs, pop, A68_MAKE_FUNCTION);
  indent (out, "CLOSE_FRAME;\n");
}

//! @brief Compile code clause.

char *gen_code_clause (NODE_T * p, FILE_T out, int compose_fun)
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

char *gen_closed_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *sc = NEXT_SUB (p);
  if (MOID (p) == M_VOID && LABELS (TABLE (sc)) == NO_TAG) {
    static char fn[NAME_SIZE];
    char pop[NAME_SIZE];
    int units = 0, decs = 0;
    NODE_T *last = NO_NODE;
    gen_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
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

char *gen_collateral_clause (NODE_T * p, FILE_T out, int compose_fun)
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

char *gen_basic_conditional (NODE_T * p, FILE_T out, int compose_fun)
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
    (void) gen_unit (SUB (NEXT_SUB (p)), out, A68_MAKE_NOTHING);
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
    (void) gen_unit (SUB (NEXT_SUB (p)), out, A68_MAKE_NOTHING);
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

char *gen_conditional_clause (NODE_T * p, FILE_T out, int compose_fun)
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
      gen_serial_clause (NEXT_SUB (q), out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
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

BOOL_T gen_int_case_units (NODE_T * p, FILE_T out, NODE_T * sym, int k, int *count, int compose_fun)
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
          if (gen_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
            if (IS (p, UNIT) && IS (SUB (p), TERTIARY)) {
              gen_units (SUB_SUB (p), out);
            } else {
              gen_units (SUB (p), out);
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
      if (gen_int_case_units (SUB (p), out, sym, k, count, compose_fun)) {
        return A68_TRUE;
      } else {
        return gen_int_case_units (NEXT (p), out, sym, k, count, compose_fun);
      }
    }
  }
}

//! @brief Compile integral-case-clause.

char *gen_int_case_clause (NODE_T * p, FILE_T out, int compose_fun)
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
      } while (gen_int_case_units (NEXT_SUB (q), out, NO_NODE, k, &count, A68_MAKE_OTHERS));
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, OUT_PART, CHOICE, STOP)) {
      last = NO_NODE;
      units = decs = 0;
      gen_serial_clause (NEXT_SUB (q), out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
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
  } while (gen_int_case_units (NEXT_SUB (q), out, SUB (q), k, &count, A68_MAKE_FUNCTION));
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

char *gen_loop_clause (NODE_T * p, FILE_T out, int compose_fun)
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
  gen_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
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
    indent (out, "// genie_preemptive_gc_heap (p);\n");
  }
  if (for_part != NO_NODE) {
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_STATUS_ (%s) = INIT_MASK;\n", z));
    indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "_VALUE_ (%s) = %s;\n", z, idf));
  }
  units = decs = 0;
  gen_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_FUNCTION);
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

//! @brief Optimise units.

char *gen_unit (NODE_T * p, FILE_T out, BOOL_T compose_fun)
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
    COMPILE (SUB (p), out, gen_unit, compose_fun);
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 3) {
// Control structure.
    if (IS (p, CLOSED_CLAUSE)) {
      COMPILE (p, out, gen_closed_clause, compose_fun);
    } else if (IS (p, COLLATERAL_CLAUSE)) {
      COMPILE (p, out, gen_collateral_clause, compose_fun);
    } else if (IS (p, CONDITIONAL_CLAUSE)) {
      char *fn2 = gen_basic_conditional (p, out, compose_fun);
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
        COMPILE (p, out, gen_conditional_clause, compose_fun);
      }
    } else if (IS (p, CASE_CLAUSE)) {
      COMPILE (p, out, gen_int_case_clause, compose_fun);
    } else if (IS (p, LOOP_CLAUSE)) {
      COMPILE (p, out, gen_loop_clause, compose_fun);
    }
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 2) {
// Simple constructions.
    if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), IDENTIFIER) != NO_NODE) {
      COMPILE (p, out, gen_voiding_assignation_identifier, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), SLICE) != NO_NODE) {
      COMPILE (p, out, gen_voiding_assignation_slice, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && stems_from (SUB_SUB (p), SELECTION) != NO_NODE) {
      COMPILE (p, out, gen_voiding_assignation_selection, compose_fun);
    } else if (IS (p, SLICE)) {
      COMPILE (p, out, gen_slice, compose_fun);
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SLICE) != NO_NODE) {
      COMPILE (p, out, gen_dereference_slice, compose_fun);
    } else if (IS (p, SELECTION)) {
      COMPILE (p, out, gen_selection, compose_fun);
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), SELECTION) != NO_NODE) {
      COMPILE (p, out, gen_dereference_selection, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), FORMULA)) {
      COMPILE (SUB (p), out, gen_voiding_formula, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), MONADIC_FORMULA)) {
      COMPILE (SUB (p), out, gen_voiding_formula, compose_fun);
    } else if (IS (p, DEPROCEDURING)) {
      COMPILE (p, out, gen_deproceduring, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), DEPROCEDURING)) {
      COMPILE (p, out, gen_voiding_deproceduring, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), CALL)) {
      COMPILE (p, out, gen_voiding_call, compose_fun);
    } else if (IS (p, IDENTITY_RELATION)) {
      COMPILE (p, out, gen_identity_relation, compose_fun);
    } else if (IS (p, UNITING)) {
      COMPILE (p, out, gen_uniting, compose_fun);
    }
  }
  if (A68_OPT (OPTION_CODE_LEVEL) >= 1) {
// Most basic stuff.
    if (IS (p, VOIDING)) {
      COMPILE (SUB (p), out, gen_unit, compose_fun);
    } else if (IS (p, DENOTATION)) {
      COMPILE (p, out, gen_denotation, compose_fun);
    } else if (IS (p, CAST)) {
      COMPILE (p, out, gen_cast, compose_fun);
    } else if (IS (p, IDENTIFIER)) {
      COMPILE (p, out, gen_identifier, compose_fun);
    } else if (IS (p, DEREFERENCING) && stems_from (SUB (p), IDENTIFIER) != NO_NODE) {
      COMPILE (p, out, gen_dereference_identifier, compose_fun);
    } else if (IS (p, MONADIC_FORMULA)) {
      COMPILE (p, out, gen_formula, compose_fun);
    } else if (IS (p, FORMULA)) {
      COMPILE (p, out, gen_formula, compose_fun);
    } else if (IS (p, CALL)) {
      COMPILE (p, out, gen_call, compose_fun);
    }
  }
  if (IS (p, CODE_CLAUSE)) {
    COMPILE (p, out, gen_code_clause, compose_fun);
  }
  return NO_TEXT;
#undef COMPILE
}

//! @brief Compile unit.

char *gen_basic (NODE_T * p, FILE_T out)
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
    COMPILE (SUB (p), out, gen_basic);
  }
// Most basic stuff.
  if (IS (p, VOIDING)) {
    COMPILE (SUB (p), out, gen_basic);
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

void gen_units (NODE_T * p, FILE_T out)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT) || IS (p, CODE_CLAUSE)) {
      if (gen_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
        gen_units (SUB (p), out);
      } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
        COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
        COMPILE_NAME (GINFO (p)) = new_string (COMPILE_NAME (GINFO (SUB (p))), NO_TEXT);
      }
    } else {
      gen_units (SUB (p), out);
    }
  }
}

//! @brief Compile units.

void gen_basics (NODE_T * p, FILE_T out)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT) || IS (p, CODE_CLAUSE)) {
      if (gen_basic (p, out) == NO_TEXT) {
        gen_basics (SUB (p), out);
      } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
        COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
        COMPILE_NAME (GINFO (p)) = new_string (COMPILE_NAME (GINFO (SUB (p))), NO_TEXT);
      }
    } else {
      gen_basics (SUB (p), out);
    }
  }
}
