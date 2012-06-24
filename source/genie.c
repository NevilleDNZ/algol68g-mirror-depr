/*!
\file genie.c                                       
\brief routines executing primitive A68 actions
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2012 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

A68_HANDLE nil_handle = { 
  INIT_MASK, 
  NO_BYTE, 
  0, 
  NO_MOID, 
  NO_HANDLE, 
  NO_HANDLE 
};

A68_REF nil_ref = { 
(STATUS_MASK) (INIT_MASK | NIL_MASK), 
  0, 
  0, 
  NO_HANDLE 
};

#define IF_ROW(m)\
  (IS (m, FLEX_SYMBOL) || IS (m, ROW_SYMBOL) || m == MODE (STRING))

ADDR_T frame_pointer = 0, stack_pointer = 0, 
       heap_pointer = 0, handle_pointer = 0, global_pointer = 0, 
       frame_start, frame_end, 
       stack_start, stack_end;
BOOL_T do_confirm_exit = A68_TRUE;
BYTE_T *stack_segment = NO_BYTE, 
       *heap_segment = NO_BYTE, 
       *handle_segment = NO_BYTE;
NODE_T *last_unit = NO_NODE;
int global_level = 0, ret_code, ret_line_number, ret_char_number;
int max_lex_lvl = 0;
jmp_buf genie_exit_label;

int frame_stack_size, expr_stack_size, heap_size, handle_pool_size;
int stack_limit, frame_stack_limit, expr_stack_limit;
int storage_overhead;

A68_PROCEDURE on_gc_event;

static A68_REF genie_make_rowrow (NODE_T *, MOID_T *, int, ADDR_T);
static A68_REF genie_make_ref_row_of_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
static A68_REF genie_make_ref_row_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
static A68_REF genie_clone (NODE_T *, MOID_T *, A68_REF *, A68_REF *);
static A68_REF genie_store (NODE_T *, MOID_T *, A68_REF *, A68_REF *);
static void genie_clone_stack (NODE_T *, MOID_T *, A68_REF *, A68_REF *);

static void genie_serial_units_no_label (NODE_T *, int, NODE_T **);

/* Genie routines */

static PROP_T genie_and_function (NODE_T *);
static PROP_T genie_assertion (NODE_T *);
static PROP_T genie_assignation_constant (NODE_T *);
static PROP_T genie_assignation (NODE_T *);
static PROP_T genie_call (NODE_T *);
static PROP_T genie_cast (NODE_T *);
static PROP_T genie_closed (volatile NODE_T *);
static PROP_T genie_coercion (NODE_T *);
static PROP_T genie_collateral (NODE_T *);
static PROP_T genie_column_function (NODE_T *);
static PROP_T genie_conditional (volatile NODE_T *);
static PROP_T genie_constant (NODE_T *);
static PROP_T genie_denotation (NODE_T *);
static PROP_T genie_deproceduring (NODE_T *);
static PROP_T genie_dereference_frame_identifier (NODE_T *);
static PROP_T genie_dereference_generic_identifier (NODE_T *);
static PROP_T genie_dereference_selection_name_quick (NODE_T *);
static PROP_T genie_dereference_slice_name_quick (NODE_T *);
static PROP_T genie_dereferencing (NODE_T *);
static PROP_T genie_dereferencing_quick (NODE_T *);
static PROP_T genie_diagonal_function (NODE_T *);
static PROP_T genie_dyadic (NODE_T *);
static PROP_T genie_dyadic_quick (NODE_T *);
static PROP_T genie_enclosed (volatile NODE_T *);
static PROP_T genie_field_selection (NODE_T *);
static PROP_T genie_format_text (NODE_T *);
static PROP_T genie_formula (NODE_T *);
static PROP_T genie_generator (NODE_T *);
static PROP_T genie_identifier (NODE_T *);
static PROP_T genie_identifier_standenv (NODE_T *);
static PROP_T genie_identifier_standenv_proc (NODE_T *);
static PROP_T genie_identity_relation (NODE_T *);
static PROP_T genie_int_case (volatile NODE_T *);
static PROP_T genie_frame_identifier (NODE_T *);
static PROP_T genie_loop (volatile NODE_T *);
static PROP_T genie_monadic (NODE_T *);
static PROP_T genie_nihil (NODE_T *);
static PROP_T genie_or_function (NODE_T *);
static PROP_T genie_routine_text (NODE_T *);
static PROP_T genie_row_function (NODE_T *);
static PROP_T genie_rowing (NODE_T *);
static PROP_T genie_rowing_ref_row_of_row (NODE_T *);
static PROP_T genie_rowing_ref_row_row (NODE_T *);
static PROP_T genie_rowing_row_of_row (NODE_T *);
static PROP_T genie_rowing_row_row (NODE_T *);
static PROP_T genie_selection_name_quick (NODE_T *);
static PROP_T genie_selection (NODE_T *);
static PROP_T genie_selection_value_quick (NODE_T *);
static PROP_T genie_skip (NODE_T *);
static PROP_T genie_slice_name_quick (NODE_T *);
static PROP_T genie_slice (NODE_T *);
static PROP_T genie_transpose_function (NODE_T *);
static PROP_T genie_united_case (volatile NODE_T *);
static PROP_T genie_uniting (NODE_T *);
static PROP_T genie_unit (NODE_T *);
static PROP_T genie_voiding_assignation_constant (NODE_T *);
static PROP_T genie_voiding_assignation (NODE_T *);
static PROP_T genie_voiding (NODE_T *);
static PROP_T genie_widening_int_to_real (NODE_T *);
static PROP_T genie_widening (NODE_T *);
static PROP_T genie_assignation_quick (NODE_T * p);
static PROP_T genie_loop (volatile NODE_T *);

#if defined HAVE_PARALLEL_CLAUSE
static PROP_T genie_parallel (NODE_T *);
#endif

/*!
\brief nop for the genie, for instance '+' for INT or REAL
\param p position in tree
**/

void genie_idle (NODE_T * p)
{
  (void) p;
}

/*!
\brief unimplemented feature handler
\param p position in tree
**/

void genie_unimplemented (NODE_T * p)
{
  diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNIMPLEMENTED);
  exit_genie (p, A68_RUNTIME_ERROR);
}

/*!
\brief PROC system = (STRING) INT
\param p position in tree
**/

void genie_system (NODE_T * p)
{
  int sys_ret_code, size;
  A68_REF cmd;
  A68_REF ref_z;
  POP_REF (p, &cmd);
  CHECK_INIT (p, INITIALISED (&cmd), MODE (STRING));
  size = 1 + a68_string_size (p, cmd);
  ref_z = heap_generator (p, MODE (C_STRING), 1 + size);
  sys_ret_code = system (a_to_c_string (p, DEREF (char, &ref_z), cmd));
  PUSH_PRIMITIVE (p, sys_ret_code, A68_INT);
}

/*!
\brief set flags throughout tree
\param p position in tree
**/

void change_masks (NODE_T * p, unsigned mask, BOOL_T set)
{
  for (; p != NO_NODE; FORWARD (p)) {
    change_masks (SUB (p), mask, set);
    if (LINE_NUMBER (p) > 0) {
      if (set == A68_TRUE) {
        STATUS_SET (p, mask);
      } else {
        STATUS_CLEAR (p, mask);
      }
    }
  }
}

/*!
\brief leave interpretation
\param p position in tree
\param ret exit code
**/

void exit_genie (NODE_T * p, int ret)
{
#if defined HAVE_CURSES
  genie_curses_end (p);
#endif
  if (!in_execution) {
    return;
  }
  if (ret == A68_RUNTIME_ERROR && in_monitor) {
    return;
  } else if (ret == A68_RUNTIME_ERROR && OPTION_DEBUG (&program)) {
    diagnostics_to_terminal (TOP_LINE (&program), A68_RUNTIME_ERROR);
    single_step (p, (unsigned) BREAKPOINT_ERROR_MASK);
    in_execution = A68_FALSE;
    ret_line_number = LINE_NUMBER (p);
    ret_code = ret;
    longjmp (genie_exit_label, 1);
  } else {
    if (ret > A68_FORCE_QUIT) {
      ret -= A68_FORCE_QUIT;
    }
#if defined HAVE_PARALLEL_CLAUSE
    if (!is_main_thread ()) {
      genie_set_exit_from_threads (ret);
    } else {
      in_execution = A68_FALSE;
      ret_line_number = LINE_NUMBER (p);
      ret_code = ret;
      longjmp (genie_exit_label, 1);
    }
#else
    in_execution = A68_FALSE;
    ret_line_number = LINE_NUMBER (p);
    ret_code = ret;
    longjmp (genie_exit_label, 1);
#endif
  }
}

/*!
\brief genie init rng
**/

void genie_init_rng (void)
{
  time_t t;
  if (time (&t) != -1) {
    struct tm *u = localtime (&t);
    int seed = TM_SEC (u) + 60 * (TM_MIN (u) + 60 * TM_HOUR (u));
    init_rng ((long unsigned) seed);
  }
}

/*!
\brief tie label to the clause it is defined in
\param p position in tree
**/

void tie_label_to_serial (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, SERIAL_CLAUSE)) {
      BOOL_T valid_follow;
      if (NEXT (p) == NO_NODE) {
        valid_follow = A68_TRUE;
      } else if (IS (NEXT (p), CLOSE_SYMBOL)) {
        valid_follow = A68_TRUE;
      } else if (IS (NEXT (p), END_SYMBOL)) {
        valid_follow = A68_TRUE;
      } else if (IS (NEXT (p), EDOC_SYMBOL)) {
        valid_follow = A68_TRUE;
      } else if (IS (NEXT (p), OD_SYMBOL)) {
        valid_follow = A68_TRUE;
      } else {
        valid_follow = A68_FALSE;
      }
      if (valid_follow) {
        JUMP_TO (TABLE (SUB (p))) = NO_NODE;
      }
    }
    tie_label_to_serial (SUB (p));
  }
}

/*!
\brief tie label to the clause it is defined in
\param p position in tree
\param unit associated unit
**/

static void tie_label (NODE_T * p, NODE_T * unit)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, DEFINING_IDENTIFIER)) {
      UNIT (TAX (p)) = unit;
    }
    tie_label (SUB (p), unit);
  }
}

/*!
\brief tie label to the clause it is defined in
\param p position in tree
**/

void tie_label_to_unit (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, LABELED_UNIT)) {
      tie_label (SUB_SUB (p), NEXT_SUB (p));
    }
    tie_label_to_unit (SUB (p));
  }
}

/*!
\brief fast way to indicate a mode
\param p position in tree
**/

static int mode_attribute (MOID_T * p)
{
  if (IS (p, REF_SYMBOL)) {
    return (REF_SYMBOL);
  } else if (IS (p, PROC_SYMBOL)) {
    return (PROC_SYMBOL);
  } else if (IS (p, UNION_SYMBOL)) {
    return (UNION_SYMBOL);
  } else if (p == MODE (INT)) {
    return (MODE_INT);
  } else if (p == MODE (LONG_INT)) {
    return (MODE_LONG_INT);
  } else if (p == MODE (LONGLONG_INT)) {
    return (MODE_LONGLONG_INT);
  } else if (p == MODE (REAL)) {
    return (MODE_REAL);
  } else if (p == MODE (LONG_REAL)) {
    return (MODE_LONG_REAL);
  } else if (p == MODE (LONGLONG_REAL)) {
    return (MODE_LONGLONG_REAL);
  } else if (p == MODE (COMPLEX)) {
    return (MODE_COMPLEX);
  } else if (p == MODE (LONG_COMPLEX)) {
    return (MODE_LONG_COMPLEX);
  } else if (p == MODE (LONGLONG_COMPLEX)) {
    return (MODE_LONGLONG_COMPLEX);
  } else if (p == MODE (BOOL)) {
    return (MODE_BOOL);
  } else if (p == MODE (CHAR)) {
    return (MODE_CHAR);
  } else if (p == MODE (BITS)) {
    return (MODE_BITS);
  } else if (p == MODE (LONG_BITS)) {
    return (MODE_LONG_BITS);
  } else if (p == MODE (LONGLONG_BITS)) {
    return (MODE_LONGLONG_BITS);
  } else if (p == MODE (BYTES)) {
    return (MODE_BYTES);
  } else if (p == MODE (LONG_BYTES)) {
    return (MODE_LONG_BYTES);
  } else if (p == MODE (FILE)) {
    return (MODE_FILE);
  } else if (p == MODE (FORMAT)) {
    return (MODE_FORMAT);
  } else if (p == MODE (PIPE)) {
    return (MODE_PIPE);
  } else if (p == MODE (SOUND)) {
    return (MODE_SOUND);
  } else {
    return (MODE_NO_CHECK);
  }
}

/*!
\brief perform tasks before interpretation
\param p position in tree
\param max_lev maximum level found
**/

void genie_preprocess (NODE_T * p, int *max_lev, void *compile_lib)
{
#if defined HAVE_COMPILER
  static char * last_compile_name = NO_TEXT;
  static PROP_PROC * last_compile_unit = NO_PPROC;
#endif
  for (; p != NO_NODE; FORWARD (p)) {
    if (STATUS_TEST (p, BREAKPOINT_MASK)) {
      if (!(STATUS_TEST (p, INTERRUPTIBLE_MASK))) {
        STATUS_CLEAR (p, BREAKPOINT_MASK);
      }
    }
    if (GINFO (p) != NO_GINFO) {
      IS_COERCION (GINFO (p)) = is_coercion (p);
      IS_NEW_LEXICAL_LEVEL (GINFO (p)) = is_new_lexical_level (p);
/* The default */
      UNIT (&GPROP (p)) = genie_unit;
      SOURCE (&GPROP (p)) = p;
#if defined HAVE_COMPILER
      if (OPTION_OPTIMISE (&program) && COMPILE_NAME (GINFO (p)) != NO_TEXT && compile_lib != NULL) {
        if (COMPILE_NAME (GINFO (p)) == last_compile_name) { /* copy */
          UNIT (&GPROP (p)) = last_compile_unit;
        } else { /* look up */
/* 
Next line provokes a warning that cannot be suppressed, 
not even by this POSIX workaround. Tant pis.
*/
          * (void **) &(UNIT (&GPROP (p))) = dlsym (compile_lib, COMPILE_NAME (GINFO (p)));
          ABEND (UNIT (&GPROP (p)) == NULL, "compiler cannot resolve", dlerror ());
          last_compile_name = COMPILE_NAME (GINFO (p));
          last_compile_unit = UNIT (&GPROP (p));
        }
      }
#endif
    }
    if (MOID (p) != NO_MOID) {
      SIZE (MOID (p)) = moid_size (MOID (p));
      SHORT_ID (MOID (p)) = mode_attribute (MOID (p));
      if (GINFO (p) != NO_GINFO) {
        NEED_DNS (GINFO (p)) = A68_FALSE;
        if (IS (MOID (p), REF_SYMBOL)) {
          NEED_DNS (GINFO (p)) = A68_TRUE;
        } else if (IS (MOID (p), PROC_SYMBOL)) {
          NEED_DNS (GINFO (p)) = A68_TRUE;
        } else if (IS (MOID (p), FORMAT_SYMBOL)) {
          NEED_DNS (GINFO (p)) = A68_TRUE;
        }
      }
    }
    if (TABLE (p) != NO_TABLE) {
      if (LEX_LEVEL (p) > *max_lev) {
        *max_lev = LEX_LEVEL (p);
      }
    }
    if (IS (p, FORMAT_TEXT)) {
      TAG_T *q = TAX (p);
      if (q != NO_TAG && NODE (q) != NO_NODE) {
        NODE (q) = p;
      }
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *q = TAX (p);
      if (q != NO_TAG && NODE (q) != NO_NODE && TABLE (NODE (q)) != NO_TABLE) {
        LEVEL (GINFO (p)) = LEX_LEVEL (NODE (q));
      }
    } else if (IS (p, IDENTIFIER)) {
      TAG_T *q = TAX (p);
      if (q != NO_TAG && NODE (q) != NO_NODE && TABLE (NODE (q)) != NO_TABLE) {
        LEVEL (GINFO (p)) = LEX_LEVEL (NODE (q));
        OFFSET (GINFO (p)) = &stack_segment[FRAME_INFO_SIZE + OFFSET (q)];
      }
    } else if (IS (p, OPERATOR)) {
      TAG_T *q = TAX (p);
      if (q != NO_TAG && NODE (q) != NO_NODE && TABLE (NODE (q)) != NO_TABLE) {
        LEVEL (GINFO (p)) = LEX_LEVEL (NODE (q));
        OFFSET (GINFO (p)) = &stack_segment[FRAME_INFO_SIZE + OFFSET (q)];
      }
    }
    if (SUB (p) != NO_NODE) {
      if (GINFO (p) != NO_GINFO) {
        GPARENT (SUB (p)) = p;
      }
      genie_preprocess (SUB (p), max_lev, compile_lib);
    }
  }
}

/*!
\brief get outermost lexical level in the user program
\param p position in tree
**/

void get_global_level (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (LINE_NUMBER (p) != 0 && IS (p, UNIT)) {
      if (LEX_LEVEL (p) < global_level) {
        global_level = LEX_LEVEL (p);
      }
    }
    get_global_level (SUB (p));
  }
}

/*!
\brief free heap allocated by genie
\param p position in tree
**/

void free_genie_heap (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    free_genie_heap (SUB (p));
    if (GINFO (p) != NO_GINFO && CONSTANT (GINFO (p)) != NO_CONSTANT) {
      free (CONSTANT (GINFO (p)));
      CONSTANT (GINFO (p)) = NO_CONSTANT;
    }
  }
}

/*!
\brief driver for the interpreter
**/

void genie (void * compile_lib)
{
  MOID_T *m;
/* Fill in final info for modes */
  for (m = TOP_MOID (&program); m != NO_MOID; FORWARD (m)) {
    SIZE (m) = moid_size (m);
    SHORT_ID (m) = mode_attribute (m);
  }
/* Preprocessing */
  max_lex_lvl = 0;
/*  genie_lex_levels (TOP_NODE (&program), 1); */
  genie_preprocess (TOP_NODE (&program), &max_lex_lvl, compile_lib);
  change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
  watchpoint_expression = NO_TEXT;
  frame_stack_limit = frame_end - storage_overhead;
  expr_stack_limit = stack_end - storage_overhead;
  if (OPTION_REGRESSION_TEST (&program)) {
    init_rng (1);
  } else {
    genie_init_rng ();
  }
  io_close_tty_line ();
  if (OPTION_TRACE (&program)) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "genie: frame stack %dk, expression stack %dk, heap %dk, handles %dk\n", frame_stack_size / KILOBYTE, expr_stack_size / KILOBYTE, heap_size / KILOBYTE, handle_pool_size / KILOBYTE) >= 0);
    WRITE (STDOUT_FILENO, output_line);
  }
  install_signal_handlers ();
  set_default_event_procedure (&on_gc_event);
  do_confirm_exit = A68_TRUE;
/* Dive into the program */
  if (setjmp (genie_exit_label) == 0) {
    NODE_T *p = SUB (TOP_NODE (&program));
/* If we are to stop in the monitor, set a breakpoint on the first unit */
    if (OPTION_DEBUG (&program)) {
      change_masks (TOP_NODE (&program), BREAKPOINT_TEMPORARY_MASK, A68_TRUE);
      WRITE (STDOUT_FILENO, "Execution begins ...");
    }
    RESET_ERRNO;
    ret_code = 0;
    global_level = A68_MAX_INT;
    global_pointer = 0;
    get_global_level (p);
    frame_pointer = frame_start;
    stack_pointer = stack_start;
    FRAME_DYNAMIC_LINK (frame_pointer) = 0;
    FRAME_DNS (frame_pointer) = 0;
    FRAME_STATIC_LINK (frame_pointer) = 0;
    FRAME_NUMBER (frame_pointer) = 0;
    FRAME_TREE (frame_pointer) = (NODE_T *) p;
    FRAME_LEXICAL_LEVEL (frame_pointer) = LEX_LEVEL (p);
    FRAME_PARAMETER_LEVEL (frame_pointer) = LEX_LEVEL (p);
    FRAME_PARAMETERS (frame_pointer) = frame_pointer;
    initialise_frame (p);
    genie_init_heap (p);
    genie_init_transput (TOP_NODE (&program));
    cputime_0 = seconds ();
/* Here we go .. */
    in_execution = A68_TRUE;
    last_unit = TOP_NODE (&program);
#if ! defined HAVE_WIN32
    (void) alarm (1);
#endif /* ! defined HAVE_WIN32 */
    if (OPTION_TRACE (&program)) {
      WIS (TOP_NODE (&program));
    }
    (void) genie_enclosed (TOP_NODE (&program));
  } else {
/* Here we have jumped out of the interpreter. What happened? */
    if (OPTION_DEBUG (&program)) {
      WRITE (STDOUT_FILENO, "Execution discontinued");
    }
    if (ret_code == A68_RERUN) {
      diagnostics_to_terminal (TOP_LINE (&program), A68_RUNTIME_ERROR);
      genie (compile_lib);
    } else if (ret_code == A68_RUNTIME_ERROR) {
      if (OPTION_BACKTRACE (&program)) {
        int printed = 0;
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\nStack backtrace") >= 0);
        WRITE (STDOUT_FILENO, output_line);
        stack_dump (STDOUT_FILENO, frame_pointer, 16, &printed);
        WRITE (STDOUT_FILENO, NEWLINE_STRING);
      }
      if (FILE_LISTING_OPENED (&program)) {
        int printed = 0;
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\nStack backtrace") >= 0);
        WRITE (FILE_LISTING_FD (&program), output_line);
        stack_dump (FILE_LISTING_FD (&program), frame_pointer, 32, &printed);
      }
    }
  }
  in_execution = A68_FALSE;
}

/*
This file contains interpreter ("genie") routines related to executing primitive
A68 actions.

The genie is self-optimising as when it traverses the tree, it stores terminals
it ends up in at the root where traversing for that terminal started.
Such piece of information is called a PROP.
*/

/*!
\brief shows line where 'p' is at and draws a '-' beneath the position
\param f file number
\param p position in tree
**/

void where_in_source (FILE_T f, NODE_T * p)
{
  write_source_line (f, LINE (INFO (p)), p, A68_NO_DIAGNOSTICS);
}

/*
Since Algol 68 can pass procedures as parameters, we use static links rather
than a display.
*/

/*!
\brief initialise PROC and OP identities
\param p starting node of a declaration
\param seq chain to link nodes into
\param count number of constants initialised
**/

static void genie_init_proc_op (NODE_T * p, NODE_T ** seq, int *count)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case OP_SYMBOL:
    case PROC_SYMBOL:
    case OPERATOR_PLAN:
    case DECLARER:
      {
        break;
      }
    case DEFINING_IDENTIFIER:
    case DEFINING_OPERATOR:
      {
/* Store position so we need not search again */
        NODE_T *save = *seq;
        (*seq) = p;
        SEQUENCE (*seq) = save;
        (*count)++;
        return;
      }
    default:
      {
        genie_init_proc_op (SUB (p), seq, count);
        break;
      }
    }
  }
}

/*!
\brief initialise PROC and OP identity declarations
\param p position in tree
\param count number of constants initialised
**/

void genie_find_proc_op (NODE_T * p, int *count)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (GINFO (p) != NO_GINFO && IS_NEW_LEXICAL_LEVEL (GINFO (p))) {
/* Don't enter a new lexical level - it will have its own initialisation */
      return;
    } else if (IS (p, PROCEDURE_DECLARATION) || IS (p, BRIEF_OPERATOR_DECLARATION)) {
      genie_init_proc_op (SUB (p), &(SEQUENCE (TABLE (p))), count);
      return;
    } else {
      genie_find_proc_op (SUB (p), count);
    }
  }
}

void initialise_frame (NODE_T * p)
{
  if (INITIALISE_ANON (TABLE (p))) {
    TAG_T *_a_;
    INITIALISE_ANON (TABLE (p)) = A68_FALSE;
    for (_a_ = ANONYMOUS (TABLE (p)); _a_ != NO_TAG; FORWARD (_a_)) {
      if (PRIO (_a_) == ROUTINE_TEXT) {
        int youngest = YOUNGEST_ENVIRON (TAX (NODE (_a_)));
        A68_PROCEDURE *_z_ = (A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (_a_)));
        STATUS (_z_) = INIT_MASK;
        NODE (&(BODY (_z_))) = NODE (_a_);
        if (youngest > 0) {
          STATIC_LINK_FOR_FRAME (ENVIRON (_z_), 1 + youngest);
        } else {
          ENVIRON (_z_) = 0;
        }
        LOCALE (_z_) = NO_HANDLE;
        MOID (_z_) = MOID (_a_);
        INITIALISE_ANON (TABLE (p)) = A68_TRUE;
      } else if (PRIO (_a_) == FORMAT_TEXT) {
        int youngest = YOUNGEST_ENVIRON (TAX (NODE (_a_)));
        A68_FORMAT *_z_ = (A68_FORMAT *) (FRAME_OBJECT (OFFSET (_a_)));
        STATUS (_z_) = INIT_MASK;
        BODY (_z_) = NODE (_a_);
        if (youngest > 0) {
          STATIC_LINK_FOR_FRAME (ENVIRON (_z_), 1 + youngest);
        } else {
          ENVIRON (_z_) = 0;
        }
        INITIALISE_ANON (TABLE (p)) = A68_TRUE;
      }
    }
  }
  if (PROC_OPS (TABLE (p))) {
    NODE_T *_q_;
    ADDR_T pop_sp;
    if (SEQUENCE (TABLE (p)) == NO_NODE) {
      int count = 0;
      genie_find_proc_op (p, &count);
      PROC_OPS (TABLE (p)) = (BOOL_T) (count > 0);
    }
    pop_sp = stack_pointer;
    for (_q_ = SEQUENCE (TABLE (p)); _q_ != NO_NODE; _q_ = SEQUENCE (_q_)) {
      NODE_T *u = NEXT_NEXT (_q_);
      if (IS (u, ROUTINE_TEXT)) {
        NODE_T *src = SOURCE (&(GPROP (u)));
        *(A68_PROCEDURE *) FRAME_OBJECT (OFFSET (TAX (_q_))) = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (src))));
      } else if ((IS (u, UNIT) && IS (SUB (u), ROUTINE_TEXT))) {
        NODE_T *src = SOURCE (&(GPROP (SUB (u))));
        *(A68_PROCEDURE *) FRAME_OBJECT (OFFSET (TAX (_q_))) = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (src))));
      }
    }
  }
  INITIALISE_FRAME (TABLE (p)) = (BOOL_T) (INITIALISE_ANON (TABLE (p)) || PROC_OPS (TABLE (p)));
}

/*!
\brief whether item at "w" of mode "q" is initialised
\param p position in tree
\param w pointer to object
\param q mode of object
**/

void genie_check_initialisation (NODE_T * p, BYTE_T * w, MOID_T * q)
{
  switch (SHORT_ID (q)) {
  case REF_SYMBOL:
    {
      A68_REF *z = (A68_REF *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case PROC_SYMBOL:
    {
      A68_PROCEDURE *z = (A68_PROCEDURE *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_INT:
    {
      A68_INT *z = (A68_INT *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_REAL:
    {
      A68_REAL *z = (A68_REAL *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_COMPLEX:
    {
      A68_REAL *r = (A68_REAL *) w;
      A68_REAL *i = (A68_REAL *) (w + ALIGNED_SIZE_OF (A68_REAL));
      CHECK_INIT (p, INITIALISED (r), q);
      CHECK_INIT (p, INITIALISED (i), q);
      return;
    }
  case MODE_LONG_INT:
  case MODE_LONGLONG_INT:
  case MODE_LONG_REAL:
  case MODE_LONGLONG_REAL:
  case MODE_LONG_BITS:
  case MODE_LONGLONG_BITS:
    {
      MP_T *z = (MP_T *) w;
      CHECK_INIT (p, (unsigned) z[0] & INIT_MASK, q);
      return;
    }
  case MODE_LONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_long_mp ());
      CHECK_INIT (p, (unsigned) r[0] & INIT_MASK, q);
      CHECK_INIT (p, (unsigned) i[0] & INIT_MASK, q);
      return;
    }
  case MODE_LONGLONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_longlong_mp ());
      CHECK_INIT (p, (unsigned) r[0] & INIT_MASK, q);
      CHECK_INIT (p, (unsigned) i[0] & INIT_MASK, q);
      return;
    }
  case MODE_BOOL:
    {
      A68_BOOL *z = (A68_BOOL *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_CHAR:
    {
      A68_CHAR *z = (A68_CHAR *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_BITS:
    {
      A68_BITS *z = (A68_BITS *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_BYTES:
    {
      A68_BYTES *z = (A68_BYTES *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_LONG_BYTES:
    {
      A68_LONG_BYTES *z = (A68_LONG_BYTES *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_FILE:
    {
      A68_FILE *z = (A68_FILE *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_FORMAT:
    {
      A68_FORMAT *z = (A68_FORMAT *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_PIPE:
    {
      A68_REF *pipe_read = (A68_REF *) w;
      A68_REF *pipe_write = (A68_REF *) (w + A68_REF_SIZE);
      A68_INT *pid = (A68_INT *) (w + 2 * A68_REF_SIZE);
      CHECK_INIT (p, INITIALISED (pipe_read), q);
      CHECK_INIT (p, INITIALISED (pipe_write), q);
      CHECK_INIT (p, INITIALISED (pid), q);
      return;
    }
  case MODE_SOUND:
    {
      A68_SOUND *z = (A68_SOUND *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  }
}

/*!
\brief push constant stored in the tree
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_constant (NODE_T * p)
{
  PUSH (p, CONSTANT (GINFO (p)), SIZE (GINFO (p)));
  return (GPROP (p));
}

/*!
\brief unite value in the stack and push result
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_uniting (NODE_T * p)
{
  PROP_T self;
  ADDR_T sp = stack_pointer;
  MOID_T *u = MOID (p), *v = MOID (SUB (p));
  int size = MOID_SIZE (u);
  if (ATTRIBUTE (v) != UNION_SYMBOL) {
    PUSH_UNION (p, (void *) unites_to (v, u));
    EXECUTE_UNIT (SUB (p));
    STACK_DNS (p, SUB (v), frame_pointer);
  } else {
    A68_UNION *m = (A68_UNION *) STACK_TOP;
    EXECUTE_UNIT (SUB (p));
    STACK_DNS (p, SUB (v), frame_pointer);
    VALUE (m) = (void *) unites_to ((MOID_T *) VALUE (m), u);
  }
  stack_pointer = sp + size;
  UNIT (&self) = genie_uniting;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief store widened constant as a constant
\param p position in tree
\param m mode of object
\param self propagator to set
**/

static void make_constant_widening (NODE_T * p, MOID_T * m, PROP_T * self)
{
  if (SUB (p) != NO_NODE && CONSTANT (GINFO (SUB (p))) != NO_CONSTANT) {
    int size = MOID_SIZE (m);
    UNIT (self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((unsigned) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), (void *) (STACK_OFFSET (-size)), size);
  }
}

/*!
\brief (optimised) push INT widened to REAL
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_widening_int_to_real (NODE_T * p)
{
  A68_INT *i = (A68_INT *) STACK_TOP;
  A68_REAL *z = (A68_REAL *) STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REAL) - ALIGNED_SIZE_OF (A68_INT));
  VALUE (z) = (double) VALUE (i);
  STATUS (z) = INIT_MASK;
  return (GPROP (p));
}

/*!
\brief widen value in the stack
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_widening (NODE_T * p)
{
#define COERCE_FROM_TO(p, a, b) (MOID (p) == (b) && MOID (SUB (p)) == (a))
  PROP_T self;
  UNIT (&self) = genie_widening;
  SOURCE (&self) = p;
/* INT widenings */
  if (COERCE_FROM_TO (p, MODE (INT), MODE (REAL))) {
    (void) genie_widening_int_to_real (p);
    UNIT (&self) = genie_widening_int_to_real;
    make_constant_widening (p, MODE (REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (INT), MODE (LONG_INT))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_int_to_long_mp (p);
    make_constant_widening (p, MODE (LONG_INT), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_INT), MODE (LONGLONG_INT))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_mp_to_longlong_mp (p);
    make_constant_widening (p, MODE (LONGLONG_INT), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_INT), MODE (LONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
/* 1-1 mapping */
    make_constant_widening (p, MODE (LONG_REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONGLONG_INT), MODE (LONGLONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
/* 1-1 mapping */
    make_constant_widening (p, MODE (LONGLONG_REAL), &self);
  }
/* REAL widenings */
  else if (COERCE_FROM_TO (p, MODE (REAL), MODE (LONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_real_to_long_mp (p);
    make_constant_widening (p, MODE (LONG_REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_REAL), MODE (LONGLONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_mp_to_longlong_mp (p);
    make_constant_widening (p, MODE (LONGLONG_REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (REAL), MODE (COMPLEX))) {
    EXECUTE_UNIT (SUB (p));
    PUSH_PRIMITIVE (p, 0.0, A68_REAL);
    make_constant_widening (p, MODE (COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_REAL), MODE (LONG_COMPLEX))) {
    MP_T *z;
    int digits = get_mp_digits (MODE (LONG_REAL));
    EXECUTE_UNIT (SUB (p));
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_T) INIT_MASK;
    make_constant_widening (p, MODE (LONG_COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX))) {
    MP_T *z;
    int digits = get_mp_digits (MODE (LONGLONG_REAL));
    EXECUTE_UNIT (SUB (p));
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_T) INIT_MASK;
    make_constant_widening (p, MODE (LONGLONG_COMPLEX), &self);
  }
/* COMPLEX widenings */
  else if (COERCE_FROM_TO (p, MODE (COMPLEX), MODE (LONG_COMPLEX))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_complex_to_long_complex (p);
    make_constant_widening (p, MODE (LONG_COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_COMPLEX), MODE (LONGLONG_COMPLEX))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_complex_to_longlong_complex (p);
    make_constant_widening (p, MODE (LONGLONG_COMPLEX), &self);
  }
/* BITS widenings */
  else if (COERCE_FROM_TO (p, MODE (BITS), MODE (LONG_BITS))) {
    EXECUTE_UNIT (SUB (p));
/* Treat unsigned as int, but that's ok */
    genie_lengthen_int_to_long_mp (p);
    make_constant_widening (p, MODE (LONG_BITS), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_BITS), MODE (LONGLONG_BITS))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_mp_to_longlong_mp (p);
    make_constant_widening (p, MODE (LONGLONG_BITS), &self);
  }
/* Miscellaneous widenings */
  else if (COERCE_FROM_TO (p, MODE (BYTES), MODE (ROW_CHAR))) {
    A68_BYTES z;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &z, A68_BYTES);
    PUSH_REF (p, c_string_to_row_char (p, VALUE (&z), BYTES_WIDTH));
  } else if (COERCE_FROM_TO (p, MODE (LONG_BYTES), MODE (ROW_CHAR))) {
    A68_LONG_BYTES z;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &z, A68_LONG_BYTES);
    PUSH_REF (p, c_string_to_row_char (p, VALUE (&z), LONG_BYTES_WIDTH));
  } else if (COERCE_FROM_TO (p, MODE (BITS), MODE (ROW_BOOL))) {
    A68_BITS x;
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int k;
    unsigned bit;
    BYTE_T *base;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &x, A68_BITS);
    z = heap_generator (p, MODE (ROW_BOOL), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    row = heap_generator (p, MODE (ROW_BOOL), BITS_WIDTH * MOID_SIZE (MODE (BOOL)));
    DIM (&arr) = 1;
    MOID (&arr) = MODE (BOOL);
    ELEM_SIZE (&arr) = MOID_SIZE (MODE (BOOL));
    SLICE_OFFSET (&arr) = 0;
    FIELD_OFFSET (&arr) = 0;
    ARRAY (&arr) = row;
    LWB (&tup) = 1;
    UPB (&tup) = BITS_WIDTH;
    SHIFT (&tup) = LWB (&tup);
    SPAN (&tup) = 1;
    K (&tup) = 0;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = ADDRESS (&row) + MOID_SIZE (MODE (BOOL)) * (BITS_WIDTH - 1);
    bit = 1;
    for (k = BITS_WIDTH - 1; k >= 0; k--, base -= MOID_SIZE (MODE (BOOL)), bit <<= 1) {
      STATUS ((A68_BOOL *) base) = INIT_MASK;
      VALUE ((A68_BOOL *) base) = (BOOL_T) ((VALUE (&x) & bit) != 0 ? A68_TRUE : A68_FALSE);
    }
    PUSH_REF (p, z);
  } else if (COERCE_FROM_TO (p, MODE (LONG_BITS), MODE (ROW_BOOL)) || COERCE_FROM_TO (p, MODE (LONGLONG_BITS), MODE (ROW_BOOL))) {
    MOID_T *m = MOID (SUB (p));
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int size = get_mp_size (m), k, width = get_mp_bits_width (m), words = get_mp_bits_words (m);
    unsigned *bits;
    BYTE_T *base;
    MP_T *x;
    ADDR_T pop_sp = stack_pointer;
/* Calculate and convert BITS value */
    EXECUTE_UNIT (SUB (p));
    x = (MP_T *) STACK_OFFSET (-size);
    bits = stack_mp_bits (p, x, m);
/* Make [] BOOL */
    z = heap_generator (p, MODE (ROW_BOOL), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    row = heap_generator (p, MODE (ROW_BOOL), width * MOID_SIZE (MODE (BOOL)));
    DIM (&arr) = 1;
    MOID (&arr) = MODE (BOOL);
    ELEM_SIZE (&arr) = MOID_SIZE (MODE (BOOL));
    SLICE_OFFSET (&arr) = 0;
    FIELD_OFFSET (&arr) = 0;
    ARRAY (&arr) = row;
    LWB (&tup) = 1;
    UPB (&tup) = width;
    SHIFT (&tup) = LWB (&tup);
    SPAN (&tup) = 1;
    K (&tup) = 0;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = ADDRESS (&row) + (width - 1) * MOID_SIZE (MODE (BOOL));
    k = width;
    while (k > 0) {
      unsigned bit = 0x1;
      int j;
      for (j = 0; j < MP_BITS_BITS && k >= 0; j++) {
        STATUS ((A68_BOOL *) base) = INIT_MASK;
        VALUE ((A68_BOOL *) base) = (BOOL_T) ((bits[words - 1] & bit) ? A68_TRUE : A68_FALSE);
        base -= MOID_SIZE (MODE (BOOL));
        bit <<= 1;
        k--;
      }
      words--;
    }
    if (CONSTANT (GINFO (SUB (p))) != NO_CONSTANT) {
      UNIT (&self) = genie_constant;
      BLOCK_GC_HANDLE (&z);
      CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) A68_REF_SIZE);
      SIZE (GINFO (p)) = A68_REF_SIZE;
      COPY (CONSTANT (GINFO (p)), &z, A68_REF_SIZE);
    }
    stack_pointer = pop_sp;
    PUSH_REF (p, z);
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CANNOT_WIDEN, MOID (SUB (p)), MOID (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  return (self);
#undef COERCE_FROM_TO
}

/*!
\brief cast a jump to a PROC VOID without executing the jump
\param p position in tree
**/

static void genie_proceduring (NODE_T * p)
{
  A68_PROCEDURE z;
  NODE_T *jump = SUB (p);
  NODE_T *q = SUB (jump);
  NODE_T *label = (IS (q, GOTO_SYMBOL) ? NEXT (q) : q);
  STATUS (&z) = INIT_MASK;
  NODE (&(BODY (&z))) = jump;
  STATIC_LINK_FOR_FRAME (ENVIRON (&z), 1 + TAG_LEX_LEVEL (TAX (label)));
  LOCALE (&z) = NO_HANDLE;
  MOID (&z) = MODE (PROC_VOID);
  PUSH_PROCEDURE (p, z);
}

/*!
\brief (optimised) dereference value of a unit
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_dereferencing_quick (NODE_T * p)
{
  A68_REF *z = (A68_REF *) STACK_TOP;
  ADDR_T pop_sp = stack_pointer;
  BYTE_T *stack_top = STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  stack_pointer = pop_sp;
  CHECK_REF (p, *z, MOID (SUB (p)));
  PUSH (p, ADDRESS (z), MOID_SIZE (MOID (p)));
  genie_check_initialisation (p, stack_top, MOID (p));
  return (GPROP (p));
}

/*!
\brief dereference an identifier
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_dereference_frame_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB_MOID (p);
  BYTE_T *stack_top = STACK_TOP;
  FRAME_GET (z, A68_REF, p);
  PUSH (p, ADDRESS (z),  MOID_SIZE (deref));
  genie_check_initialisation (p, stack_top, deref);
  return (GPROP (p));
}

/*!
\brief dereference an identifier
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_dereference_generic_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB_MOID (p);
  BYTE_T *stack_top = STACK_TOP;
  FRAME_GET (z, A68_REF, p);
  CHECK_REF (p, *z, MOID (SUB (p)));
  PUSH (p, ADDRESS (z), MOID_SIZE (deref));
  genie_check_initialisation (p, stack_top, deref);
  return (GPROP (p));
}

/*!
\brief slice REF [] A to A
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_dereference_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *prim = SUB (p);
  A68_ARRAY *a;
  A68_TUPLE *t;
  A68_REF *z;
  MOID_T *ref_mode = MOID (p);
  MOID_T *deref_mode = SUB (ref_mode);
  int size = MOID_SIZE (deref_mode), row_index;
  ADDR_T pop_sp = stack_pointer;
  BYTE_T *stack_top = STACK_TOP;
/* Get REF [] */
  z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (prim);
  stack_pointer = pop_sp;
  CHECK_REF (p, *z, ref_mode);
  GET_DESCRIPTOR (a, t, DEREF (A68_ROW, z));
  for (row_index = 0, q = SEQUENCE (p); q != NO_NODE; t++, q = SEQUENCE (q)) {
    A68_INT *j = (A68_INT *) STACK_TOP;
    int k;
    EXECUTE_UNIT (q);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    row_index += (SPAN (t) * k - SHIFT (t));
    stack_pointer = pop_sp;
  }
/* Push element */
  PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, row_index)]), size);
  genie_check_initialisation (p, stack_top, deref_mode);
  return (GPROP (p));
}

/*!
\brief dereference SELECTION from a name
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_dereference_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  MOID_T *result_mode = SUB_MOID (selector);
  int size = MOID_SIZE (result_mode);
  A68_REF *z = (A68_REF *) STACK_TOP;
  ADDR_T pop_sp = stack_pointer;
  BYTE_T *stack_top;
  EXECUTE_UNIT (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  stack_pointer = pop_sp;
  stack_top = STACK_TOP;
  PUSH (p, ADDRESS (z), size);
  genie_check_initialisation (p, stack_top, result_mode);
  return (GPROP (p));
}

/*!
\brief dereference name in the stack
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_dereferencing (NODE_T * p)
{
  A68_REF z;
  PROP_T self;
  EXECUTE_UNIT_2 (SUB (p), self);
  POP_REF (p, &z);
  CHECK_REF (p, z, MOID (SUB (p)));
  PUSH (p, ADDRESS (&z), MOID_SIZE (MOID (p)));
  genie_check_initialisation (p, STACK_OFFSET (-MOID_SIZE (MOID (p))), MOID (p));
  if (UNIT (&self) == genie_frame_identifier) {
    if (IS_IN_FRAME (&z)) {
      UNIT (&self) = genie_dereference_frame_identifier;
    } else {
      UNIT (&self) = genie_dereference_generic_identifier;
    }
    UNIT (&PROP (GINFO (SOURCE (&self)))) = UNIT (&self);
  } else if (UNIT (&self) == genie_slice_name_quick) {
    UNIT (&self) = genie_dereference_slice_name_quick;
    UNIT (&PROP (GINFO (SOURCE (&self)))) = UNIT (&self);
  } else if (UNIT (&self) == genie_selection_name_quick) {
    UNIT (&self) = genie_dereference_selection_name_quick;
    UNIT (&PROP (GINFO (SOURCE (&self)))) = UNIT (&self);
  } else {
    UNIT (&self) = genie_dereferencing_quick;
    SOURCE (&self) = p;
  }
  return (self);
}

/*!
\brief deprocedure PROC in the stack
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_deproceduring (NODE_T * p)
{
  PROP_T self;
  A68_PROCEDURE *z;
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  NODE_T *proc = SUB (p);
  MOID_T *proc_mode = MOID (proc);
  UNIT (&self) = genie_deproceduring;
  SOURCE (&self) = p;
/* Get procedure */
  z = (A68_PROCEDURE *) STACK_TOP;
  EXECUTE_UNIT (proc);
  stack_pointer = pop_sp;
  genie_check_initialisation (p, (BYTE_T *) z, proc_mode);
  genie_call_procedure (p, proc_mode, proc_mode, MODE (VOID), z, pop_sp, pop_fp);
  STACK_DNS (p, MOID (p), frame_pointer);
  return (self);
}

/*!
\brief voiden value in the stack
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_voiding (NODE_T * p)
{
  PROP_T self, source;
  ADDR_T sp_for_voiding = stack_pointer;
  SOURCE (&self) = p;
  EXECUTE_UNIT_2 (SUB (p), source);
  stack_pointer = sp_for_voiding;
  if (UNIT (&source) == genie_assignation_quick) {
    UNIT (&self) = genie_voiding_assignation;
    SOURCE (&self) = SOURCE (&source);
  } else if (UNIT (&source) == genie_assignation_constant) {
    UNIT (&self) = genie_voiding_assignation_constant;
    SOURCE (&self) = SOURCE (&source);
  } else {
    UNIT (&self) = genie_voiding;
  }
  return (self);
}

/*!
\brief coerce value in the stack
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_coercion (NODE_T * p)
{
  PROP_T self;
  UNIT (&self) = genie_coercion;
  SOURCE (&self) = p;
  switch (ATTRIBUTE (p)) {
  case VOIDING:
    {
      self = genie_voiding (p);
      break;
    }
  case UNITING:
    {
      self = genie_uniting (p);
      break;
    }
  case WIDENING:
    {
      self = genie_widening (p);
      break;
    }
  case ROWING:
    {
      self = genie_rowing (p);
      break;
    }
  case DEREFERENCING:
    {
      self = genie_dereferencing (p);
      break;
    }
  case DEPROCEDURING:
    {
      self = genie_deproceduring (p);
      break;
    }
  case PROCEDURING:
    {
      genie_proceduring (p);
      break;
    }
  }
  GPROP (p) = self;
  return (self);
}

/*!
\brief push argument units
\param p position in tree
\param seq chain to link nodes into
**/

static void genie_argument (NODE_T * p, NODE_T ** seq)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      EXECUTE_UNIT (p);
      STACK_DNS (p, MOID (p), frame_pointer);
      SEQUENCE (*seq) = p;
      (*seq) = p;
      return;
    } else if (IS (p, TRIMMER)) {
      return;
    } else {
      genie_argument (SUB (p), seq);
    }
  }
}

/*!
\brief evaluate partial call
\param p position in tree
\param pr_mode full mode of procedure object
\param pproc mode of resulting proc
\param pmap mode of the locale
\param z procedure object to call
\param pop_sp stack pointer value to restore
\param pop_fp frame pointer value to restore
**/

void genie_partial_call (NODE_T * p, MOID_T * pr_mode, MOID_T * pproc, MOID_T * pmap, A68_PROCEDURE z, ADDR_T pop_sp, ADDR_T pop_fp)
{
  int voids = 0;
  BYTE_T *u, *v;
  PACK_T *s, *t;
  A68_REF ref;
  A68_HANDLE *loc;
/* Get locale for the new procedure descriptor. Copy is necessary */
  if (LOCALE (&z) == NO_HANDLE) {
    int size = 0;
    for (s = PACK (pr_mode); s != NO_PACK; FORWARD (s)) {
      size += (ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s)));
    }
    ref = heap_generator (p, pr_mode, size);
    loc = REF_HANDLE (&ref);
  } else {
    int size = SIZE (LOCALE (&z));
    ref = heap_generator (p, pr_mode, size);
    loc = REF_HANDLE (&ref);
    COPY (POINTER (loc), POINTER (LOCALE (&z)), size);
  }
/* Move arguments from stack to locale using pmap */
  u = POINTER (loc);
  s = PACK (pr_mode);
  v = STACK_ADDRESS (pop_sp);
  t = PACK (pmap);
  for (; t != NO_PACK && s != NO_PACK; FORWARD (t)) {
/* Skip already initialised arguments */
    while (u != NULL && VALUE ((A68_BOOL *) & u[0])) {
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      FORWARD (s);
    }
    if (u != NULL && MOID (t) == MODE (VOID)) {
/* Move to next field in locale */
      voids++;
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      FORWARD (s);
    } else {
/* Move argument from stack to locale */
      A68_BOOL w;
      STATUS (&w) = INIT_MASK;
      VALUE (&w) = A68_TRUE;
      *(A68_BOOL *) & u[0] = w;
      COPY (&(u[ALIGNED_SIZE_OF (A68_BOOL)]), v, MOID_SIZE (MOID (t)));
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      v = &(v[MOID_SIZE (MOID (t))]);
      FORWARD (s);
    }
  }
  stack_pointer = pop_sp;
  LOCALE (&z) = loc;
/* Is closure complete? */
  if (voids == 0) {
/* Closure is complete. Push locale onto the stack and call procedure body */
    stack_pointer = pop_sp;
    u = POINTER (loc);
    v = STACK_ADDRESS (stack_pointer);
    s = PACK (pr_mode);
    for (; s != NO_PACK; FORWARD (s)) {
      int size = MOID_SIZE (MOID (s));
      COPY (v, &u[ALIGNED_SIZE_OF (A68_BOOL)], size);
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + size]);
      v = &(v[MOID_SIZE (MOID (s))]);
      INCREMENT_STACK_POINTER (p, size);
    }
    genie_call_procedure (p, pr_mode, pproc, MODE (VOID), &z, pop_sp, pop_fp);
  } else {
/*  Closure is not complete. Return procedure body */
    PUSH_PROCEDURE (p, z);
  }
}

/*!
\brief closure and deproceduring of routines with PARAMSETY
\param p position in tree
\param pr_mode full mode of procedure object
\param pproc mode of resulting proc
\param pmap mode of the locale
\param z procedure object to call
\param pop_sp stack pointer value to restore
\param pop_fp frame pointer value to restore
**/

void genie_call_procedure (NODE_T * p, MOID_T * pr_mode, MOID_T * pproc, MOID_T * pmap, A68_PROCEDURE * z, ADDR_T pop_sp, ADDR_T pop_fp)
{
  if (pmap != MODE (VOID) && pr_mode != pmap) {
    genie_partial_call (p, pr_mode, pproc, pmap, *z, pop_sp, pop_fp);
  } else if (STATUS (z) & STANDENV_PROC_MASK) {
    (void) ((*(PROCEDURE (&(BODY (z))))) (p));
  } else if (STATUS (z) & SKIP_PROCEDURE_MASK) {
    stack_pointer = pop_sp;
    genie_push_undefined (p, SUB ((MOID (z))));
  } else {
    NODE_T *body = NODE (&(BODY (z)));
    if (IS (body, ROUTINE_TEXT)) {
      NODE_T *entry = SUB (body);
      PACK_T *args = PACK (pr_mode);
      ADDR_T fp0 = 0;
/* Copy arguments from stack to frame */
      OPEN_PROC_FRAME (entry, ENVIRON (z));
      INIT_STATIC_FRAME (entry);
      FRAME_DNS (frame_pointer) = pop_fp;
      for (; args != NO_PACK; FORWARD (args)) {
        int size = MOID_SIZE (MOID (args));
        COPY ((FRAME_OBJECT (fp0)), STACK_ADDRESS (pop_sp + fp0), size);
        fp0 += size;
      }
      stack_pointer = pop_sp;
      ARGSIZE (GINFO (p)) = fp0;
/* Interpret routine text */
      if (DIM (pr_mode) > 0) {
/* With PARAMETERS */
        entry = NEXT (NEXT_NEXT (entry));
      } else {
/* Without PARAMETERS */
        entry = NEXT_NEXT (entry);
      }
      EXECUTE_UNIT_TRACE (entry);
      if (frame_pointer == finish_frame_pointer) {
        change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
      }
      CLOSE_FRAME;
      STACK_DNS (p, SUB (pr_mode), frame_pointer);
    } else {
      OPEN_PROC_FRAME (body, ENVIRON (z));
      INIT_STATIC_FRAME (body);
      FRAME_DNS (frame_pointer) = pop_fp;
      EXECUTE_UNIT_TRACE (body);
      if (frame_pointer == finish_frame_pointer) {
        change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
      }
      CLOSE_FRAME;
      STACK_DNS (p, SUB (pr_mode), frame_pointer);
    }
  }
}

/*!
\brief call event routine
\param p position in tree
\return a propagator for this action
**/

void genie_call_event_routine (NODE_T *p, MOID_T *m, A68_PROCEDURE *proc, ADDR_T pop_sp, ADDR_T pop_fp)
{
  if (NODE (&(BODY (proc))) != NO_NODE) {
    A68_PROCEDURE save = *proc;
    set_default_event_procedure (proc);
    genie_call_procedure (p, MOID (&save), m, m, &save, pop_sp, pop_fp);
    (*proc) = save;
  }
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_call_standenv_quick (NODE_T * p)
{
  NODE_T *pr = SUB (p), *q = SEQUENCE (p);
  TAG_T *proc = TAX (SOURCE (&GPROP (pr)));
/* Get arguments */
  for (; q != NO_NODE; q = SEQUENCE (q)) {
    EXECUTE_UNIT (q);
    STACK_DNS (p, MOID (q), frame_pointer);
  }
  (void) ((*(PROCEDURE (proc))) (p));
  return (GPROP (p));
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_call_quick (NODE_T * p)
{
  A68_PROCEDURE z;
  NODE_T *proc = SUB (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
/* Get procedure */
  EXECUTE_UNIT (proc);
  POP_OBJECT (proc, &z, A68_PROCEDURE);
  genie_check_initialisation (p, (BYTE_T *) &z, MOID (proc));
/* Get arguments */
  if (SEQUENCE (p) == NO_NODE && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GINFO_T g;
    GINFO (&top_seq) = &g;
    genie_argument (NEXT (proc), &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
  } else {
    NODE_T *q = SEQUENCE (p);
    for (; q != NO_NODE; q = SEQUENCE (q)) {
      EXECUTE_UNIT (q);
      STACK_DNS (p, MOID (q), frame_pointer);
    }
  }
  genie_call_procedure (p, MOID (&z), PARTIAL_PROC (GINFO (proc)), PARTIAL_LOCALE (GINFO (proc)), &z, pop_sp, pop_fp);
  return (GPROP (p));
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_call (NODE_T * p)
{
  PROP_T self;
  A68_PROCEDURE z;
  NODE_T *proc = SUB (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  UNIT (&self) = genie_call_quick;
  SOURCE (&self) = p;
/* Get procedure */
  EXECUTE_UNIT (proc);
  POP_OBJECT (proc, &z, A68_PROCEDURE);
  genie_check_initialisation (p, (BYTE_T *) &z, MOID (proc));
/* Get arguments */
  if (SEQUENCE (p) == NO_NODE && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GINFO_T g;
    GINFO (&top_seq) = &g;
    genie_argument (NEXT (proc), &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
  } else {
    NODE_T *q = SEQUENCE (p);
    for (; q != NO_NODE; q = SEQUENCE (q)) {
      EXECUTE_UNIT (q);
    }
  }
  genie_call_procedure (p, MOID (&z), PARTIAL_PROC (GINFO (proc)), PARTIAL_LOCALE (GINFO (proc)), &z, pop_sp, pop_fp);
  if (PARTIAL_LOCALE (GINFO (proc)) != MODE (VOID) && MOID (&z) != PARTIAL_LOCALE (GINFO (proc))) {
    /* skip */ ;
  } else if (STATUS (&z) & STANDENV_PROC_MASK) {
    if (UNIT (&GPROP (proc)) == genie_identifier_standenv_proc) {
      UNIT (&self) = genie_call_standenv_quick;
    }
  }
  return (self);
}

/*!
\brief construct a descriptor "ref_new" for a trim of "ref_old"
\param p position in tree
\param ref_new new descriptor
\param ref_old old descriptor
\param offset calculates the offset of the trim
**/

static void genie_trimmer (NODE_T * p, BYTE_T * *ref_new, BYTE_T * *ref_old, int *offset)
{
  if (p != NO_NODE) {
    if (IS (p, UNIT)) {
      A68_INT k;
      A68_TUPLE *t;
      EXECUTE_UNIT (p);
      POP_OBJECT (p, &k, A68_INT);
      t = (A68_TUPLE *) * ref_old;
      CHECK_INDEX (p, &k, t);
      (*offset) += SPAN (t) * VALUE (&k) - SHIFT (t);
      /*
       * (*ref_old) += ALIGNED_SIZE_OF (A68_TUPLE);
       */
      (*ref_old) += sizeof (A68_TUPLE);
    } else if (IS (p, TRIMMER)) {
      A68_INT k;
      NODE_T *q;
      int L, U, D;
      A68_TUPLE *old_tup = (A68_TUPLE *) * ref_old;
      A68_TUPLE *new_tup = (A68_TUPLE *) * ref_new;
/* TRIMMER is (l:u@r) with all units optional or (empty) */
      q = SUB (p);
      if (q == NO_NODE) {
        L = LWB (old_tup);
        U = UPB (old_tup);
        D = 0;
      } else {
        BOOL_T absent = A68_TRUE;
/* Lower index */
        if (q != NO_NODE && IS (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_OBJECT (p, &k, A68_INT);
          if (VALUE (&k) < LWB (old_tup)) {
            diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
          }
          L = VALUE (&k);
          FORWARD (q);
          absent = A68_FALSE;
        } else {
          L = LWB (old_tup);
        }
        if (q != NO_NODE && (IS (q, COLON_SYMBOL)
                          || IS (q, DOTDOT_SYMBOL))) {
          FORWARD (q);
          absent = A68_FALSE;
        }
/* Upper index */
        if (q != NO_NODE && IS (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_OBJECT (p, &k, A68_INT);
          if (VALUE (&k) > UPB (old_tup)) {
            diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
          }
          U = VALUE (&k);
          FORWARD (q);
          absent = A68_FALSE;
        } else {
          U = UPB (old_tup);
        }
        if (q != NO_NODE && IS (q, AT_SYMBOL)) {
          FORWARD (q);
        }
/* Revised lower bound */
        if (q != NO_NODE && IS (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_OBJECT (p, &k, A68_INT);
          D = L - VALUE (&k);
          FORWARD (q);
        } else {
          D = (absent ? 0 : L - 1);
        }
      }
      LWB (new_tup) = L - D;
      UPB (new_tup) = U - D;    /* (L - D) + (U - L) */
      SPAN (new_tup) = SPAN (old_tup);
      SHIFT (new_tup) = SHIFT (old_tup) - D * SPAN (new_tup);
      (*ref_old) += sizeof (A68_TUPLE);
      (*ref_new) += sizeof (A68_TUPLE);
    } else {
      genie_trimmer (SUB (p), ref_new, ref_old, offset);
      genie_trimmer (NEXT (p), ref_new, ref_old, offset);
    }
  }
}

/*!
\brief calculation of subscript
\param p position in tree
\param ref_heap
\param sum calculates the index of the subscript
\param seq chain to link nodes into
**/

void genie_subscript (NODE_T * p, A68_TUPLE ** tup, int *sum, NODE_T ** seq)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        A68_INT *k;
        A68_TUPLE *t = *tup;
        EXECUTE_UNIT (p);
        POP_ADDRESS (p, k, A68_INT);
        CHECK_INDEX (p, k, t);
        (*tup)++;
        (*sum) += (SPAN (t) * VALUE (k) - SHIFT (t));
        SEQUENCE (*seq) = p;
        (*seq) = p;
        return;
      }
    case GENERIC_ARGUMENT:
    case GENERIC_ARGUMENT_LIST:
      {
        genie_subscript (SUB (p), tup, sum, seq);
      }
    }
  }
}

/*!
\brief slice REF [] A to REF A
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *pr = SUB (p);
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_ARRAY *a;
  ADDR_T pop_sp, scope;
  A68_TUPLE *t;
  int sindex;
/* Get row and save row from garbage collector */
  EXECUTE_UNIT (pr);
  CHECK_REF (p, *z, MOID (SUB (p)));
  GET_DESCRIPTOR (a, t, DEREF (A68_ROW, z));
  pop_sp = stack_pointer;
  for (sindex = 0, q = SEQUENCE (p); q != NO_NODE; t++, q = SEQUENCE (q)) {
    A68_INT *j = (A68_INT *) STACK_TOP;
    int k;
    EXECUTE_UNIT (q);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    sindex += (SPAN (t) * k - SHIFT (t));
    stack_pointer = pop_sp;
  }
/* Leave reference to element on the stack, preserving scope */
  scope = REF_SCOPE (z);
  *z = ARRAY (a);
  OFFSET (z) += ROW_ELEMENT (a, sindex);
  REF_SCOPE (z) = scope;
  return (GPROP (p));
}

/*!
\brief push slice of a rowed object
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_slice (NODE_T * p)
{
  PROP_T self, primary;
  ADDR_T pop_sp, scope = PRIMAL_SCOPE;
  BOOL_T slice_of_name = (BOOL_T) (IS (MOID (SUB (p)), REF_SYMBOL));
  MOID_T *result_mode = slice_of_name ? SUB_MOID (p) : MOID (p);
  NODE_T *indexer = NEXT_SUB (p);
  UNIT (&self) = genie_slice;
  SOURCE (&self) = p;
  pop_sp = stack_pointer;
/* Get row */
  EXECUTE_UNIT_2 (SUB (p), primary);
/* In case of slicing a REF [], we need the [] internally, so dereference */
  if (slice_of_name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, * DEREF (A68_REF, &z));
  }
  if (ANNOTATION (indexer) == SLICE) {
/* SLICING subscripts one element from an array */
    A68_REF z;
    A68_ARRAY *a;
    A68_TUPLE *t;
    int sindex;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    GET_DESCRIPTOR (a, t, &z);
    if (SEQUENCE (p) == NO_NODE && ! STATUS_TEST (p, SEQUENCE_MASK)) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      GINFO_T g;
      GINFO (&top_seq) = &g;
      sindex = 0;
      genie_subscript (indexer, &t, &sindex, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      STATUS_SET (p, SEQUENCE_MASK);
    } else {
      NODE_T *q;
      for (sindex = 0, q = SEQUENCE (p); q != NO_NODE; t++, q = SEQUENCE (q)) {
        A68_INT *j = (A68_INT *) STACK_TOP;
        int k;
        EXECUTE_UNIT (q);
        k = VALUE (j);
        if (k < LWB (t) || k > UPB (t)) {
          diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
          exit_genie (q, A68_RUNTIME_ERROR);
        }
        sindex += (SPAN (t) * k - SHIFT (t));
      }
    }
/* Slice of a name yields a name */
    stack_pointer = pop_sp;
    if (slice_of_name) {
      A68_REF name = ARRAY (a);
      OFFSET (&name) += ROW_ELEMENT (a, sindex);
      REF_SCOPE (&name) = scope;
      PUSH_REF (p, name);
      if (STATUS_TEST (p, SEQUENCE_MASK)) {
        UNIT (&self) = genie_slice_name_quick;
        SOURCE (&self) = p;
      }
    } else {
      BYTE_T *stack_top = STACK_TOP;
      PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, sindex)]), MOID_SIZE (result_mode));
      genie_check_initialisation (p, stack_top, result_mode);
    }
    return (self);
  } else if (ANNOTATION (indexer) == TRIMMER) {
/* Trimming selects a subarray from an array */
    int offset;
    A68_REF z, ref_desc_copy;
    A68_ARRAY *old_des, *new_des;
    BYTE_T *ref_new, *ref_old;
    ref_desc_copy = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_ARRAY) + DIM (DEFLEX (result_mode)) * ALIGNED_SIZE_OF (A68_TUPLE));
/* Get descriptor */
    POP_REF (p, &z);
/* Get indexer */
    CHECK_REF (p, z, MOID (SUB (p)));
    old_des = DEREF (A68_ARRAY, &z);
    new_des = DEREF (A68_ARRAY, &ref_desc_copy);
    ref_old = ADDRESS (&z) + ALIGNED_SIZE_OF (A68_ARRAY);
    ref_new = ADDRESS (&ref_desc_copy) + ALIGNED_SIZE_OF (A68_ARRAY);
    DIM (new_des) = DIM (DEFLEX (result_mode));
    MOID (new_des) = MOID (old_des);
    ELEM_SIZE (new_des) = ELEM_SIZE (old_des);
    offset = SLICE_OFFSET (old_des);
    genie_trimmer (indexer, &ref_new, &ref_old, &offset);
    SLICE_OFFSET (new_des) = offset;
    FIELD_OFFSET (new_des) = FIELD_OFFSET (old_des);
    ARRAY (new_des) = ARRAY (old_des);
/* Trim of a name is a name */
    if (slice_of_name) {
      A68_REF ref_new2 = heap_generator (p, MOID (p), A68_REF_SIZE);
      * DEREF (A68_REF, &ref_new2) = ref_desc_copy;
      REF_SCOPE (&ref_new2) = scope;
      PUSH_REF (p, ref_new2);
    } else {
      PUSH_REF (p, ref_desc_copy);
    }
    return (self);
  } else {
    ABEND (A68_TRUE, "impossible state in genie_slice", NO_TEXT);
    return (self);
  }
}

/*!
\brief push value of denotation
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_denotation (NODE_T * p)
{
  MOID_T *moid = MOID (p);
  PROP_T self;
  UNIT (&self) = genie_denotation;
  SOURCE (&self) = p;
  if (moid == MODE (INT)) {
/* INT denotation */
    A68_INT z;
    NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    UNIT (&self) = genie_constant;
    STATUS (&z) = INIT_MASK;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_INT));
    SIZE (GINFO (p)) = ALIGNED_SIZE_OF (A68_INT);
    COPY (CONSTANT (GINFO (p)), &z, ALIGNED_SIZE_OF (A68_INT));
    PUSH_PRIMITIVE (p, VALUE ((A68_INT *) (CONSTANT (GINFO (p)))), A68_INT);
  } else if (moid == MODE (REAL)) {
/* REAL denotation */
    A68_REAL z;
    NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    STATUS (&z) = INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_REAL));
    SIZE (GINFO (p)) = ALIGNED_SIZE_OF (A68_REAL);
    COPY (CONSTANT (GINFO (p)), &z, ALIGNED_SIZE_OF (A68_REAL));
    PUSH_PRIMITIVE (p, VALUE ((A68_REAL *) (CONSTANT (GINFO (p)))), A68_REAL);
  } else if (moid == MODE (LONG_INT) || moid == MODE (LONGLONG_INT)) {
/* [LONG] LONG INT denotation */
    int digits = get_mp_digits (moid);
    MP_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (IS (SUB (p), SHORTETY) || IS (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (MP_T) INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), z, size);
  } else if (moid == MODE (LONG_REAL) || moid == MODE (LONGLONG_REAL)) {
/* [LONG] LONG REAL denotation */
    int digits = get_mp_digits (moid);
    MP_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (IS (SUB (p), SHORTETY) || IS (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (MP_T) INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), z, size);
  } else if (moid == MODE (BITS)) {
/* BITS denotation */
    A68_BITS z;
    NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    UNIT (&self) = genie_constant;
    STATUS (&z) = INIT_MASK;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_BITS));
    SIZE (GINFO (p)) = ALIGNED_SIZE_OF (A68_BITS);
    COPY (CONSTANT (GINFO (p)), &z, ALIGNED_SIZE_OF (A68_BITS));
    PUSH_PRIMITIVE (p, VALUE ((A68_BITS *) (CONSTANT (GINFO (p)))), A68_BITS);
  } else if (moid == MODE (LONG_BITS) || moid == MODE (LONGLONG_BITS)) {
/* [LONG] LONG BITS denotation */
    int digits = get_mp_digits (moid);
    MP_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (IS (SUB (p), SHORTETY) || IS (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (MP_T) INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), z, size);
  } else if (moid == MODE (BOOL)) {
/* BOOL denotation */
    A68_BOOL z;
    ASSERT (genie_string_to_value_internal (p, MODE (BOOL), NSYMBOL (p), (BYTE_T *) & z) == A68_TRUE);
    PUSH_PRIMITIVE (p, VALUE (&z), A68_BOOL);
  } else if (moid == MODE (CHAR)) {
/* CHAR denotation */
    PUSH_PRIMITIVE (p, TO_UCHAR (NSYMBOL (p)[0]), A68_CHAR);
  } else if (moid == MODE (ROW_CHAR)) {
/* [] CHAR denotation - permanent string in the heap */
    A68_REF z;
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    z = c_to_a_string (p, NSYMBOL (p), DEFAULT_WIDTH);
    GET_DESCRIPTOR (arr, tup, &z);
    BLOCK_GC_HANDLE (&z);
    BLOCK_GC_HANDLE (&(ARRAY (arr)));
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) A68_REF_SIZE);
    SIZE (GINFO (p)) = A68_REF_SIZE;
    COPY (CONSTANT (GINFO (p)), &z, A68_REF_SIZE);
    PUSH_REF (p, *(A68_REF *) (CONSTANT (GINFO (p))));
  } else if (moid == MODE (VOID)) {
/* VOID denotation: EMPTY */
    ;
  }
  return (self);
}

/*!
\brief push a local identifier
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_frame_identifier (NODE_T * p)
{
  BYTE_T *z;
  FRAME_GET (z, BYTE_T, p);
  PUSH (p, z, MOID_SIZE (MOID (p)));
  return (GPROP (p));
}

/*!
\brief push standard environ routine as PROC
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_identifier_standenv_proc (NODE_T * p)
{
  A68_PROCEDURE z;
  TAG_T *q = TAX (p);
  STATUS (&z) = (STATUS_MASK) (INIT_MASK | STANDENV_PROC_MASK);
  PROCEDURE (&(BODY (&z))) = PROCEDURE (q);
  ENVIRON (&z) = 0;
  LOCALE (&z) = NO_HANDLE;
  MOID (&z) = MOID (p);
  PUSH_PROCEDURE (p, z);
  return (GPROP (p));
}

/*!
\brief (optimised) push identifier from standard environ
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_identifier_standenv (NODE_T * p)
{ 
  (void) ((*(PROCEDURE (TAX (p)))) (p));
  return (GPROP (p));
}

/*!
\brief push identifier onto the stack
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_identifier (NODE_T * p)
{
  static PROP_T self;
  TAG_T *q = TAX (p);
  SOURCE (&self) = p;
  if (A68G_STANDENV_PROC (q)) {
    if (IS (MOID (q), PROC_SYMBOL)) {
      (void) genie_identifier_standenv_proc (p);
      UNIT (&self) = genie_identifier_standenv_proc;
    } else {
      (void) genie_identifier_standenv (p);
      UNIT (&self) = genie_identifier_standenv;
    }
  } else if (STATUS_TEST (q, CONSTANT_MASK)) {
    int size = MOID_SIZE (MOID (p));
    BYTE_T *sp_0 = STACK_TOP;
    (void) genie_frame_identifier (p);
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), (void *) sp_0, size);
    UNIT (&self) = genie_constant;
  } else {
    (void) genie_frame_identifier (p);
    UNIT (&self) = genie_frame_identifier;
  }
  return (self);
}

/*!
\brief push result of cast (coercions are deeper in the tree)
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_cast (NODE_T * p)
{
  PROP_T self;
  EXECUTE_UNIT (NEXT_SUB (p));
  UNIT (&self) = genie_cast;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief execute assertion
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_assertion (NODE_T * p)
{
  PROP_T self;
  if (STATUS_TEST (p, ASSERT_MASK)) {
    A68_BOOL z;
    EXECUTE_UNIT (NEXT_SUB (p));
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FALSE_ASSERTION);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  UNIT (&self) = genie_assertion;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push format text
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_format_text (NODE_T * p)
{
  static PROP_T self;
  A68_FORMAT z = *(A68_FORMAT *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_FORMAT (p, z);
  UNIT (&self) = genie_format_text;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief SELECTION from a value
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_selection_value_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *result_mode = MOID (selector);
  ADDR_T old_stack_pointer = stack_pointer;
  int size = MOID_SIZE (result_mode);
  int offset = OFFSET (NODE_PACK (SUB (selector)));
  EXECUTE_UNIT (NEXT (selector));
  stack_pointer = old_stack_pointer;
  if (offset > 0) {
    MOVE (STACK_TOP, STACK_OFFSET (offset), (unsigned) size);
    genie_check_initialisation (p, STACK_TOP, result_mode);
  }
  INCREMENT_STACK_POINTER (selector, size);
  return (GPROP (p));
}

/*!
\brief SELECTION from a name
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  return (GPROP (p));
}

/*!
\brief push selection from secondary
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_selection (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  PROP_T self;
  MOID_T *struct_mode = MOID (NEXT (selector)), *result_mode = MOID (selector);
  BOOL_T selection_of_name = (BOOL_T) (IS (struct_mode, REF_SYMBOL));
  SOURCE (&self) = p;
  UNIT (&self) = genie_selection;
  EXECUTE_UNIT (NEXT (selector));
/* Multiple selections */
  if (selection_of_name && (IS (SUB (struct_mode), FLEX_SYMBOL) || IS (SUB (struct_mode), ROW_SYMBOL))) {
    A68_REF *row1, row2, row3;
    int dims, desc_size;
    POP_ADDRESS (selector, row1, A68_REF);
    CHECK_REF (p, *row1, struct_mode);
    row1 = DEREF (A68_REF, row1);
    dims = DIM (DEFLEX (SUB (struct_mode)));
    desc_size = ALIGNED_SIZE_OF (A68_ARRAY) + dims * ALIGNED_SIZE_OF (A68_TUPLE);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), DEREF (BYTE_T, row1), (unsigned) desc_size);
    MOID ((DEREF (A68_ARRAY, &row2))) = SUB_SUB (result_mode);
    FIELD_OFFSET (DEREF (A68_ARRAY, &row2)) += OFFSET (NODE_PACK (SUB (selector)));
    row3 = heap_generator (selector, result_mode, A68_REF_SIZE);
    * DEREF (A68_REF, &row3) = row2;
    PUSH_REF (selector, row3);
    UNIT (&self) = genie_selection;
  } else if (struct_mode != NO_MOID && (IS (struct_mode, FLEX_SYMBOL) || IS (struct_mode, ROW_SYMBOL))) {
    A68_REF *row1, row2;
    int dims, desc_size;
    POP_ADDRESS (selector, row1, A68_REF);
    dims = DIM (DEFLEX (struct_mode));
    desc_size = ALIGNED_SIZE_OF (A68_ARRAY) + dims * ALIGNED_SIZE_OF (A68_TUPLE);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), DEREF (BYTE_T, row1), (unsigned) desc_size);
    MOID ((DEREF (A68_ARRAY, &row2))) = SUB (result_mode);
    FIELD_OFFSET (DEREF (A68_ARRAY, &row2)) += OFFSET (NODE_PACK (SUB (selector)));
    PUSH_REF (selector, row2);
    UNIT (&self) = genie_selection;
  }
/* Normal selections */
  else if (selection_of_name && IS (SUB (struct_mode), STRUCT_SYMBOL)) {
    A68_REF *z = (A68_REF *) (STACK_OFFSET (-A68_REF_SIZE));
    CHECK_REF (selector, *z, struct_mode);
    OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
    UNIT (&self) = genie_selection_name_quick;
  } else if (IS (struct_mode, STRUCT_SYMBOL)) {
    DECREMENT_STACK_POINTER (selector, MOID_SIZE (struct_mode));
    MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (SUB (selector)))), (unsigned) MOID_SIZE (result_mode));
    genie_check_initialisation (p, STACK_TOP, result_mode);
    INCREMENT_STACK_POINTER (selector, MOID_SIZE (result_mode));
    UNIT (&self) = genie_selection_value_quick;
  }
  return (self);
}

/*!
\brief push selection from primary
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_field_selection (NODE_T * p)
{
  PROP_T self;
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  NODE_T *entry = p;
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_PROCEDURE *w = (A68_PROCEDURE *) STACK_TOP;
  SOURCE (&self) = entry;
  UNIT (&self) = genie_field_selection;
  EXECUTE_UNIT (SUB (p));
  for (p = SEQUENCE (SUB (p)); p != NO_NODE; p = SEQUENCE (p)) {
    BOOL_T coerce = A68_TRUE;
    MOID_T *m = MOID (p);
    MOID_T *result_mode = MOID (NODE_PACK (p));
    while (coerce) {
      if (IS (m, REF_SYMBOL) && ISNT (SUB (m), STRUCT_SYMBOL)) {
        int size = MOID_SIZE (SUB (m));
        stack_pointer = pop_sp;
        CHECK_REF (p, *z, m);
        PUSH (p, ADDRESS (z), size);
        genie_check_initialisation (p, STACK_OFFSET (-size), MOID (p));
        m = SUB (m);
      } else if (IS (m, PROC_SYMBOL)) {
        genie_check_initialisation (p, (BYTE_T *) w, m);
        genie_call_procedure (p, m, m, MODE (VOID), w, pop_sp, pop_fp);
        STACK_DNS (p, MOID (p), frame_pointer);
        m = SUB (m);
      } else {
        coerce = A68_FALSE;
      }
    }
    if (IS (m, REF_SYMBOL) && IS (SUB (m), STRUCT_SYMBOL)) {
      CHECK_REF (p, *z, m);
      OFFSET (z) += OFFSET (NODE_PACK (p));
    } else if (IS (m, STRUCT_SYMBOL)) {
      stack_pointer = pop_sp;
      MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (p))), (unsigned) MOID_SIZE (result_mode));
      INCREMENT_STACK_POINTER (p, MOID_SIZE (result_mode));
    }
  }
  return (self);
}

/*!
\brief call operator
\param p position in tree
\param pop_sp stack pointer value to restore
**/

void genie_call_operator (NODE_T * p, ADDR_T pop_sp)
{
  A68_PROCEDURE *z;
  ADDR_T pop_fp = frame_pointer;
  MOID_T *pr_mode = MOID (TAX (p));
  FRAME_GET (z, A68_PROCEDURE, p);
  genie_call_procedure (p, pr_mode, MOID (z), pr_mode, z, pop_sp, pop_fp);
  STACK_DNS (p, SUB (pr_mode), frame_pointer);
}

/*!
\brief push result of monadic formula OP "u"
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_monadic (NODE_T * p)
{
  NODE_T *op = SUB (p);
  NODE_T *u = NEXT (op);
  PROP_T self;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (u);
  STACK_DNS (u, MOID (u), frame_pointer);
  if (PROCEDURE (TAX (op)) != NO_GPROC) {
    (void) ((*(PROCEDURE (TAX (op)))) (op));
  } else {
    genie_call_operator (op, sp);
  }
  UNIT (&self) = genie_monadic;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_dyadic_quick (NODE_T * p)
{
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  NODE_T *v = NEXT (op);
  EXECUTE_UNIT (u);
  STACK_DNS (u, MOID (u), frame_pointer);
  EXECUTE_UNIT (v);
  STACK_DNS (v, MOID (v), frame_pointer);
  (void) ((*(PROCEDURE (TAX (op)))) (op));
  return (GPROP (p));
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_dyadic (NODE_T * p)
{
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  NODE_T *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  EXECUTE_UNIT (u);
  STACK_DNS (u, MOID (u), frame_pointer);
  EXECUTE_UNIT (v);
  STACK_DNS (v, MOID (v), frame_pointer);
  if (PROCEDURE (TAX (op)) != NO_GPROC) {
    (void) ((*(PROCEDURE (TAX (op)))) (op));
  } else {
    genie_call_operator (op, pop_sp);
  }
  return (GPROP (p));
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_formula (NODE_T * p)
{
  PROP_T self, lhs, rhs;
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  ADDR_T pop_sp = stack_pointer;
  UNIT (&self) = genie_formula;
  SOURCE (&self) = p;
  EXECUTE_UNIT_2 (u, lhs);
  STACK_DNS (u, MOID (u), frame_pointer);
  if (op != NO_NODE) {
    NODE_T *v = NEXT (op);
    GPROC *proc = PROCEDURE (TAX (op));
    EXECUTE_UNIT_2 (v, rhs);
    STACK_DNS (v, MOID (v), frame_pointer);
    UNIT (&self) = genie_dyadic;
    if (proc != NO_GPROC) {
      (void) ((*(proc)) (op));
      UNIT (&self) = genie_dyadic_quick;
    } else {
      genie_call_operator (op, pop_sp);
    }
    return (self);
  } else if (UNIT (&lhs) == genie_monadic) {
    return (lhs);
  }
  return (self);
}

/*!
\brief push NIL
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_nihil (NODE_T * p)
{
  PROP_T self;
  PUSH_REF (p, nil_ref);
  UNIT (&self) = genie_nihil;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief assign a value to a name and voiden
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_voiding_assignation_constant (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = SOURCE (&PROP (GINFO (NEXT_NEXT (dst))));
  ADDR_T pop_sp = stack_pointer;
  A68_REF *z = (A68_REF *) STACK_TOP;
  PROP_T self;
  UNIT (&self) = genie_voiding_assignation_constant;
  SOURCE (&self) = p;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  COPY (ADDRESS (z), CONSTANT (GINFO (src)), SIZE (GINFO (src)));
  stack_pointer = pop_sp;
  return (self);
}

/*!
\brief assign a value to a name and voiden
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_voiding_assignation (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = FRAME_DNS (frame_pointer);
  A68_REF z;
  BOOL_T caution;
  PROP_T self;
  UNIT (&self) = genie_voiding_assignation;
  SOURCE (&self) = p;
  EXECUTE_UNIT (dst);
  POP_OBJECT (p, &z, A68_REF);
  caution = (BOOL_T) IS_IN_HEAP (&z);
  if (caution) {
  }
  CHECK_REF (p, z, MOID (p));
  FRAME_DNS (frame_pointer) = REF_SCOPE (&z);
  EXECUTE_UNIT (src);
  STACK_DNS (src, src_mode, REF_SCOPE (&z));
  FRAME_DNS (frame_pointer) = pop_fp;
  stack_pointer = pop_sp;
  if (HAS_ROWS (src_mode)) {
    genie_clone_stack (p, src_mode, &z, &z);
  } else {
    COPY_ALIGNED (ADDRESS (&z), STACK_TOP, MOID_SIZE (src_mode));
  }
  if (caution) {
  }
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_assignation_constant (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = SOURCE (&PROP (GINFO (NEXT_NEXT (dst))));
  A68_REF *z = (A68_REF *) STACK_TOP;
  PROP_T self;
  UNIT (&self) = genie_assignation_constant;
  SOURCE (&self) = p;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  COPY (ADDRESS (z), CONSTANT (GINFO (src)), SIZE (GINFO (src)));
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_assignation_quick (NODE_T * p)
{
  PROP_T self;
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (p);
  int size = MOID_SIZE (src_mode);
  ADDR_T pop_fp = FRAME_DNS (frame_pointer);
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DNS (frame_pointer) = REF_SCOPE (z);
  EXECUTE_UNIT (src);
  STACK_DNS (src, src_mode, REF_SCOPE (z));
  FRAME_DNS (frame_pointer) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (HAS_ROWS (src_mode)) {
    genie_clone_stack (p, src_mode, z, z);
  } else {
    COPY (ADDRESS (z), STACK_TOP, size);
  }
  UNIT (&self) = genie_assignation_quick;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_assignation (NODE_T * p)
{
  PROP_T self, srp;
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (p);
  int size = MOID_SIZE (src_mode);
  ADDR_T pop_fp = FRAME_DNS (frame_pointer);
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DNS (frame_pointer) = REF_SCOPE (z);
  EXECUTE_UNIT_2 (src, srp);
  STACK_DNS (src, src_mode, REF_SCOPE (z));
  FRAME_DNS (frame_pointer) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (HAS_ROWS (src_mode)) {
    genie_clone_stack (p, src_mode, z, z);
  } else {
    COPY (ADDRESS (z), STACK_TOP, size);
  }
  if (UNIT (&srp) == genie_constant) {
    UNIT (&self) = genie_assignation_constant;
  } else {
    UNIT (&self) = genie_assignation_quick;
  }
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push equality of two REFs
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_identity_relation (NODE_T * p)
{
  PROP_T self;
  NODE_T *lhs = SUB (p), *rhs = NEXT_NEXT (lhs);
  A68_REF x, y;
  UNIT (&self) = genie_identity_relation;
  SOURCE (&self) = p;
  EXECUTE_UNIT (lhs);
  POP_REF (p, &y);
  EXECUTE_UNIT (rhs);
  POP_REF (p, &x);
  if (IS (NEXT_SUB (p), IS_SYMBOL)) {
    PUSH_PRIMITIVE (p, (BOOL_T) (ADDRESS (&x) == ADDRESS (&y)), A68_BOOL);
  } else {
    PUSH_PRIMITIVE (p, (BOOL_T) (ADDRESS (&x) != ADDRESS (&y)), A68_BOOL);
  }
  return (self);
}

/*!
\brief push result of ANDF
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_and_function (NODE_T * p)
{
  PROP_T self;
  A68_BOOL x;
  EXECUTE_UNIT (SUB (p));
  POP_OBJECT (p, &x, A68_BOOL);
  if (VALUE (&x) == A68_TRUE) {
    EXECUTE_UNIT (NEXT_NEXT (SUB (p)));
  } else {
    PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
  }
  UNIT (&self) = genie_and_function;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push result of ORF
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_or_function (NODE_T * p)
{
  PROP_T self;
  A68_BOOL x;
  EXECUTE_UNIT (SUB (p));
  POP_OBJECT (p, &x, A68_BOOL);
  if (VALUE (&x) == A68_FALSE) {
    EXECUTE_UNIT (NEXT_NEXT (SUB (p)));
  } else {
    PUSH_PRIMITIVE (p, A68_TRUE, A68_BOOL);
  }
  UNIT (&self) = genie_or_function;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push routine text
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_routine_text (NODE_T * p)
{
  static PROP_T self;
  A68_PROCEDURE z = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_PROCEDURE (p, z);
  UNIT (&self) = genie_routine_text;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push an undefined value of the required mode
\param p position in tree
\param u mode of object to push
**/

void genie_push_undefined (NODE_T * p, MOID_T * u)
{
/* For primitive modes we push an initialised value */
  if (u == MODE (VOID)) {
    /* skip */ ;
  } else if (u == MODE (INT)) {
    PUSH_PRIMITIVE (p, 1, A68_INT); /* Because users write [~] INT ! */
  } else if (u == MODE (REAL)) {
    PUSH_PRIMITIVE (p, (rng_53_bit ()), A68_REAL);
  } else if (u == MODE (BOOL)) {
    PUSH_PRIMITIVE (p, (BOOL_T) (rng_53_bit () < 0.5), A68_BOOL);
  } else if (u == MODE (CHAR)) {
    PUSH_PRIMITIVE (p, (char) (32 + 96 * rng_53_bit ()), A68_CHAR);
  } else if (u == MODE (BITS)) {
    PUSH_PRIMITIVE (p, (unsigned) (rng_53_bit () * A68_MAX_UNT), A68_BITS);
  } else if (u == MODE (COMPLEX)) {
    PUSH_COMPLEX (p, rng_53_bit (), rng_53_bit ());
  } else if (u == MODE (BYTES)) {
    PUSH_BYTES (p, "SKIP");
  } else if (u == MODE (LONG_BYTES)) {
    PUSH_LONG_BYTES (p, "SKIP");
  } else if (u == MODE (STRING)) {
    PUSH_REF (p, empty_string (p));
  } else if (u == MODE (LONG_INT) || u == MODE (LONGLONG_INT)) {
    int digits = get_mp_digits (u);
    MP_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_T) INIT_MASK;
  } else if (u == MODE (LONG_REAL) || u == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (u);
    MP_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_T) INIT_MASK;
  } else if (u == MODE (LONG_BITS) || u == MODE (LONGLONG_BITS)) {
    int digits = get_mp_digits (u);
    MP_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_T) INIT_MASK;
  } else if (u == MODE (LONG_COMPLEX) || u == MODE (LONGLONG_COMPLEX)) {
    int digits = get_mp_digits (u);
    MP_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_T) INIT_MASK;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_T) INIT_MASK;
  } else if (IS (u, REF_SYMBOL)) {
/* All REFs are NIL */
    PUSH_REF (p, nil_ref);
  } else if (IS (u, ROW_SYMBOL) || IS (u, FLEX_SYMBOL)) {
/* [] AMODE or FLEX [] AMODE */
    A68_REF er = empty_row (p, u);
    STATUS (&er) |= SKIP_ROW_MASK;
    PUSH_REF (p, er);
  } else if (IS (u, STRUCT_SYMBOL)) {
/* STRUCT */
    PACK_T *v;
    for (v = PACK (u); v != NO_PACK; FORWARD (v)) {
      genie_push_undefined (p, MOID (v));
    }
  } else if (IS (u, UNION_SYMBOL)) {
/* UNION */
    ADDR_T sp = stack_pointer;
    PUSH_UNION (p, MOID (PACK (u)));
    genie_push_undefined (p, MOID (PACK (u)));
    stack_pointer = sp + MOID_SIZE (u);
  } else if (IS (u, PROC_SYMBOL)) {
/* PROC */
    A68_PROCEDURE z;
    STATUS (&z) = (STATUS_MASK) (INIT_MASK | SKIP_PROCEDURE_MASK);
    (NODE (&BODY (&z))) = NO_NODE;
    ENVIRON (&z) = 0;
    LOCALE (&z) = NO_HANDLE;
    MOID (&z) = u;
    PUSH_PROCEDURE (p, z);
  } else if (u == MODE (FORMAT)) {
/* FORMAT etc. - what arbitrary FORMAT could mean anything at all? */
    A68_FORMAT z;
    STATUS (&z) = (STATUS_MASK) (INIT_MASK | SKIP_FORMAT_MASK);
    BODY (&z) = NO_NODE;
    ENVIRON (&z) = 0;
    PUSH_FORMAT (p, z);
  } else if (u == MODE (SIMPLOUT)) {
    ADDR_T sp = stack_pointer;
    PUSH_UNION (p, MODE (STRING));
    PUSH_REF (p, c_to_a_string (p, "SKIP", DEFAULT_WIDTH));
    stack_pointer = sp + MOID_SIZE (u);
  } else if (u == MODE (SIMPLIN)) {
    ADDR_T sp = stack_pointer;
    PUSH_UNION (p, MODE (REF_STRING));
    genie_push_undefined (p, MODE (REF_STRING));
    stack_pointer = sp + MOID_SIZE (u);
  } else if (u == MODE (REF_FILE)) {
    PUSH_REF (p, skip_file);
  } else if (u == MODE (FILE)) {
    A68_REF *z = (A68_REF *) STACK_TOP;
    int size = MOID_SIZE (MODE (FILE));
    ADDR_T pop_sp = stack_pointer;
    PUSH_REF (p, skip_file);
    stack_pointer = pop_sp;
    PUSH (p, ADDRESS (z), size);
  } else if (u == MODE (CHANNEL)) {
    PUSH_OBJECT (p, skip_channel, A68_CHANNEL);
  } else if (u == MODE (PIPE)) {
    genie_push_undefined (p, MODE (REF_FILE));
    genie_push_undefined (p, MODE (REF_FILE));
    genie_push_undefined (p, MODE (INT));
  } else if (u == MODE (SOUND)) {
    A68_SOUND *z = (A68_SOUND *) STACK_TOP;
    int size = MOID_SIZE (MODE (SOUND));
    INCREMENT_STACK_POINTER (p, size);
    FILL (z, 0, size);
    STATUS (z) = INIT_MASK;
  } else {
    BYTE_T *_sp_ = STACK_TOP;
    int size = ALIGNED_SIZE_OF (u);
    INCREMENT_STACK_POINTER (p, size);
    FILL (_sp_, 0, size);
  }
}

/*!
\brief push an undefined value of the required mode
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_skip (NODE_T * p)
{
  PROP_T self;
  if (MOID (p) != MODE (VOID)) {
    genie_push_undefined (p, MOID (p));
  }
  UNIT (&self) = genie_skip;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief jump to the serial clause where the label is at
\param p position in tree
**/

static void genie_jump (NODE_T * p)
{
/* Stack pointer and frame pointer were saved at target serial clause */
  NODE_T *jump = SUB (p);
  NODE_T *label = (IS (jump, GOTO_SYMBOL)) ? NEXT (jump) : jump;
  ADDR_T target_frame_pointer = frame_pointer;
  jmp_buf *jump_stat = NO_JMP_BUF;
/* Find the stack frame this jump points to */
  BOOL_T found = A68_FALSE;
  while (target_frame_pointer > 0 && !found) {
    found = (BOOL_T) ((TAG_TABLE (TAX (label)) == TABLE (FRAME_TREE (target_frame_pointer))) && FRAME_JUMP_STAT (target_frame_pointer) != NO_JMP_BUF);
    if (!found) {
      target_frame_pointer = FRAME_STATIC_LINK (target_frame_pointer);
    }
  }
/* Beam us up, Scotty! */
#if defined HAVE_PARALLEL_CLAUSE
  {
    int curlev = running_par_level, tarlev = PAR_LEVEL (NODE (TAX (label)));
    if (curlev == tarlev) {
/* A jump within the same thread */
      jump_stat = FRAME_JUMP_STAT (target_frame_pointer);
      JUMP_TO (TABLE (TAX (label))) = UNIT (TAX (label));
      longjmp (*(jump_stat), 1);
    } else if (curlev > 0 && tarlev == 0) {
/* A jump out of all parallel clauses back into the main program */
      genie_abend_all_threads (p, FRAME_JUMP_STAT (target_frame_pointer), label);
      ABEND (A68_TRUE, "should not return from genie_abend_all_threads", NO_TEXT);
    } else {
/* A jump between threads is forbidden in Algol68G */
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_LABEL_IN_PAR_CLAUSE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
#else
  jump_stat = FRAME_JUMP_STAT (target_frame_pointer);
  JUMP_TO (TAG_TABLE (TAX (label))) = UNIT (TAX (label));
  longjmp (*(jump_stat), 1);
#endif
}

/*!
\brief execute a unit, tertiary, secondary or primary
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_unit (NODE_T * p)
{
  if (IS_COERCION (GINFO (p))) {
    GLOBAL_PROP (&program) = genie_coercion (p);
  } else {
    switch (ATTRIBUTE (p)) {
    case DECLARATION_LIST:
      {
        genie_declaration (SUB (p));
        UNIT (&GLOBAL_PROP (&program)) = genie_unit;
        SOURCE (&GLOBAL_PROP (&program)) = p;
        break;
      }
    case UNIT:
      {
        EXECUTE_UNIT_2 (SUB (p), GLOBAL_PROP (&program));
        break;
      }
    case TERTIARY:
    case SECONDARY:
    case PRIMARY:
      {
        GLOBAL_PROP (&program) = genie_unit (SUB (p));
        break;
      }
/* Ex primary */
    case ENCLOSED_CLAUSE:
      {
        GLOBAL_PROP (&program) = genie_enclosed ((volatile NODE_T *) p);
        break;
      }
    case IDENTIFIER:
      {
        GLOBAL_PROP (&program) = genie_identifier (p);
        break;
      }
    case CALL:
      {
        GLOBAL_PROP (&program) = genie_call (p);
        break;
      }
    case SLICE:
      {
        GLOBAL_PROP (&program) = genie_slice (p);
        break;
      }
    case DENOTATION:
      {
        GLOBAL_PROP (&program) = genie_denotation (p);
        break;
      }
    case CAST:
      {
        GLOBAL_PROP (&program) = genie_cast (p);
        break;
      }
    case FORMAT_TEXT:
      {
        GLOBAL_PROP (&program) = genie_format_text (p);
        break;
      }
/* Ex secondary */
    case GENERATOR:
      {
        GLOBAL_PROP (&program) = genie_generator (p);
        break;
      }
    case SELECTION:
      {
        GLOBAL_PROP (&program) = genie_selection (p);
        break;
      }
/* Ex tertiary */
    case FORMULA:
      {
        GLOBAL_PROP (&program) = genie_formula (p);
        break;
      }
    case MONADIC_FORMULA:
      {
        GLOBAL_PROP (&program) = genie_monadic (p);
        break;
      }
    case NIHIL:
      {
        GLOBAL_PROP (&program) = genie_nihil (p);
        break;
      }
    case DIAGONAL_FUNCTION:
      {
        GLOBAL_PROP (&program) = genie_diagonal_function (p);
        break;
      }
    case TRANSPOSE_FUNCTION:
      {
        GLOBAL_PROP (&program) = genie_transpose_function (p);
        break;
      }
    case ROW_FUNCTION:
      {
        GLOBAL_PROP (&program) = genie_row_function (p);
        break;
      }
    case COLUMN_FUNCTION:
      {
        GLOBAL_PROP (&program) = genie_column_function (p);
        break;
      }
/* Ex unit */
    case ASSIGNATION:
      {
        GLOBAL_PROP (&program) = genie_assignation (p);
        break;
      }
    case IDENTITY_RELATION:
      {
        GLOBAL_PROP (&program) = genie_identity_relation (p);
        break;
      }
    case ROUTINE_TEXT:
      {
        GLOBAL_PROP (&program) = genie_routine_text (p);
        break;
      }
    case SKIP:
      {
        GLOBAL_PROP (&program) = genie_skip (p);
        break;
      }
    case JUMP:
      {
        UNIT (&GLOBAL_PROP (&program)) = genie_unit;
        SOURCE (&GLOBAL_PROP (&program)) = p;
        genie_jump (p);
        break;
      }
    case AND_FUNCTION:
      {
        GLOBAL_PROP (&program) = genie_and_function (p);
        break;
      }
    case OR_FUNCTION:
      {
        GLOBAL_PROP (&program) = genie_or_function (p);
        break;
      }
    case ASSERTION:
      {
        GLOBAL_PROP (&program) = genie_assertion (p);
        break;
      }
    case CODE_CLAUSE:
      {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CODE);
        exit_genie (p, A68_RUNTIME_ERROR);
        break;
      }
    }
  }
  return (GPROP (p) = GLOBAL_PROP (&program));
}

/*!
\brief execution of serial clause without labels
\param p position in tree
\param p position in treeop_sp
\param seq chain to link nodes into
**/

void genie_serial_units_no_label (NODE_T * p, int pop_sp, NODE_T ** seq)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DECLARATION_LIST:
    case UNIT:
      {
        EXECUTE_UNIT_TRACE (p);
        SEQUENCE (*seq) = p;
        (*seq) = p;
        return;
      }
    case SEMI_SYMBOL:
      {
/* Voiden the expression stack */
        stack_pointer = pop_sp;
        SEQUENCE (*seq) = p;
        (*seq) = p;
        break;
      }
    default:
      {
        genie_serial_units_no_label (SUB (p), pop_sp, seq);
        break;
      }
    }
  }
}

/*!
\brief execution of serial clause with labels
\param p position in tree
\param jump_to indicates node to jump to after jump
\param exit_buf jump buffer for EXITs
\param p position in treeop_sp
**/

void genie_serial_units (NODE_T * p, NODE_T ** jump_to, jmp_buf * exit_buf, int pop_sp)
{
  LOW_STACK_ALERT (p);
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DECLARATION_LIST:
    case UNIT:
      {
        if (*jump_to == NO_NODE) {
          EXECUTE_UNIT_TRACE (p);
        } else if (p == *jump_to) {
/* If we dropped in this clause from a jump then this unit is the target */
          *jump_to = NO_NODE;
          EXECUTE_UNIT_TRACE (p);
        }
        return;
      }
    case EXIT_SYMBOL:
      {
        if (*jump_to == NO_NODE) {
          longjmp (*exit_buf, 1);
        }
        break;
      }
    case SEMI_SYMBOL:
      {
        if (*jump_to == NO_NODE) {
/* Voiden the expression stack */
          stack_pointer = pop_sp;
        }
        break;
      }
    default:
      {
        genie_serial_units (SUB (p), jump_to, exit_buf, pop_sp);
        break;
      }
    }
  }
}

/*!
\brief execute serial clause
\param p position in tree
\param exit_buf jump buffer for EXITs
**/

void genie_serial_clause (NODE_T * p, jmp_buf * exit_buf)
{
  if (LABELS (TABLE (p)) == NO_TAG) {
/* No labels in this clause */
    if (SEQUENCE (p) == NO_NODE && ! STATUS_TEST (p, SEQUENCE_MASK)) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      GINFO_T g;
      GINFO (&top_seq) = &g;
      genie_serial_units_no_label (SUB (p), stack_pointer, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      STATUS_SET (p, SEQUENCE_MASK);
      STATUS_SET (p, SERIAL_MASK);
      if (SEQUENCE (p) != NO_NODE && SEQUENCE (SEQUENCE (p)) == NO_NODE) {
        STATUS_SET (p, OPTIMAL_MASK);
      }
    } else {
/* A linear list without labels */
      NODE_T *q;
      ADDR_T pop_sp = stack_pointer;
      STATUS_SET (p, SERIAL_CLAUSE);
      for (q = SEQUENCE (p); q != NO_NODE; q = SEQUENCE (q)) {
        switch (ATTRIBUTE (q)) {
        case DECLARATION_LIST:
        case UNIT:
          {
            EXECUTE_UNIT_TRACE (q);
            break;
          }
        case SEMI_SYMBOL:
          {
            stack_pointer = pop_sp;
            break;
          }
        }
      }
    }
  } else {
/* Labels in this clause */
    jmp_buf jump_stat;
    ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
    ADDR_T pop_dns = FRAME_DNS (frame_pointer);
    FRAME_JUMP_STAT (frame_pointer) = &jump_stat;
    if (!setjmp (jump_stat)) {
      NODE_T *jump_to = NO_NODE;
      genie_serial_units (SUB (p), &jump_to, exit_buf, stack_pointer);
    } else {
/* HIjol! Restore state and look for indicated unit */
      NODE_T *jump_to = JUMP_TO (TABLE (p));
      stack_pointer = pop_sp;
      frame_pointer = pop_fp;
      FRAME_DNS (frame_pointer) = pop_dns;
      genie_serial_units (SUB (p), &jump_to, exit_buf, stack_pointer);
    }
  }
}

/*!
\brief execute enquiry clause
\param p position in tree
**/

void genie_enquiry_clause (NODE_T * p)
{
  if (SEQUENCE (p) == NO_NODE && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GINFO_T g;
    GINFO (&top_seq) = &g;
    genie_serial_units_no_label (SUB (p), stack_pointer, &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
    if (SEQUENCE (p) != NO_NODE && SEQUENCE (SEQUENCE (p)) == NO_NODE) {
      STATUS_SET (p, OPTIMAL_MASK);
    }
  } else {
/* A linear list without labels (of course, it's an enquiry clause) */
    NODE_T *q;
    ADDR_T pop_sp = stack_pointer;
    STATUS_SET (p, SERIAL_MASK);
    for (q = SEQUENCE (p); q != NO_NODE; q = SEQUENCE (q)) {
      switch (ATTRIBUTE (q)) {
      case DECLARATION_LIST:
      case UNIT:
        {
          EXECUTE_UNIT_TRACE (q);
          break;
        }
      case SEMI_SYMBOL:
        {
          stack_pointer = pop_sp;
          break;
        }
      }
    }
  }
}

/*!
\brief execute collateral units
\param p position in tree
\param count counts collateral units
**/

static void genie_collateral_units (NODE_T * p, int *count)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      EXECUTE_UNIT_TRACE (p);
      STACK_DNS (p, MOID (p), FRAME_DNS (frame_pointer));
      (*count)++;
      return;
    } else {
      genie_collateral_units (SUB (p), count);
    }
  }
}

/*!
\brief execute collateral clause
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_collateral (NODE_T * p)
{
  PROP_T self;
/* VOID clause and STRUCT display */
  if (MOID (p) == MODE (VOID) || IS (MOID (p), STRUCT_SYMBOL)) {
    int count = 0;
    genie_collateral_units (SUB (p), &count);
  } else {
/* Row display */
    A68_REF new_display;
    int count = 0;
    ADDR_T sp = stack_pointer;
    MOID_T *m = MOID (p);
    genie_collateral_units (SUB (p), &count);
    if (DIM (DEFLEX (m)) == 1) {
/* [] AMODE display */
      new_display = genie_make_row (p, SLICE (DEFLEX (m)), count, sp);
      stack_pointer = sp;
      INCREMENT_STACK_POINTER (p, A68_REF_SIZE);
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    } else {
/* [,,] AMODE display, we concatenate 1 + (n-1) to n dimensions */
      new_display = genie_make_rowrow (p, m, count, sp);
      stack_pointer = sp;
      INCREMENT_STACK_POINTER (p, A68_REF_SIZE);
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    }
  }
  UNIT (&self) = genie_collateral;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief execute unit from integral-case in-part
\param p position in tree
\param k value of enquiry clause
\param count unit counter
\return whether a unit was executed
**/

BOOL_T genie_int_case_unit (NODE_T * p, int k, int *count)
{
  if (p == NO_NODE) {
    return (A68_FALSE);
  } else {
    if (IS (p, UNIT)) {
      if (k == *count) {
        EXECUTE_UNIT_TRACE (p);
        return (A68_TRUE);
      } else {
        (*count)++;
        return (A68_FALSE);
      }
    } else {
      if (genie_int_case_unit (SUB (p), k, count)) {
        return (A68_TRUE);
      } else {
        return (genie_int_case_unit (NEXT (p), k, count));
      }
    }
  }
}

/*!
\brief execute unit from united-case in-part
\param p position in tree
\param m mode of enquiry clause
\return whether a unit was executed
**/

static BOOL_T genie_united_case_unit (NODE_T * p, MOID_T * m)
{
  if (p == NO_NODE) {
    return (A68_FALSE);
  } else {
    if (IS (p, SPECIFIER)) {
      MOID_T *spec_moid = MOID (NEXT_SUB (p));
      BOOL_T equal_modes;
      if (m != NO_MOID) {
        if (IS (spec_moid, UNION_SYMBOL)) {
          equal_modes = is_unitable (m, spec_moid, SAFE_DEFLEXING);
        } else {
          equal_modes = (BOOL_T) (m == spec_moid);
        }
      } else {
        equal_modes = A68_FALSE;
      }
      if (equal_modes) {
        NODE_T *q = NEXT_NEXT (SUB (p));
        OPEN_STATIC_FRAME (p);
        INIT_STATIC_FRAME (p);
        if (IS (q, IDENTIFIER)) {
          if (IS (spec_moid, UNION_SYMBOL)) {
            COPY ((FRAME_OBJECT (OFFSET (TAX (q)))), STACK_TOP, MOID_SIZE (spec_moid));
          } else {
            COPY ((FRAME_OBJECT (OFFSET (TAX (q)))), STACK_OFFSET (A68_UNION_SIZE), MOID_SIZE (spec_moid));
          }
        }
        EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
        CLOSE_FRAME;
        return (A68_TRUE);
      } else {
        return (A68_FALSE);
      }
    } else {
      if (genie_united_case_unit (SUB (p), m)) {
        return (A68_TRUE);
      } else {
        return (genie_united_case_unit (NEXT (p), m));
      }
    }
  }
}

/*!
\brief execute identity declaration
\param p position in tree
**/

void genie_identity_dec (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (ISNT (p, DEFINING_IDENTIFIER)) {
      genie_identity_dec (SUB (p));
    } else {
        A68_REF loc;
        NODE_T *src = NEXT_NEXT (p);
        MOID_T *src_mode = MOID (p);
        unsigned size = (unsigned) MOID_SIZE (src_mode);
        BYTE_T *stack_top = STACK_TOP;
        ADDR_T pop_sp = stack_pointer;
        ADDR_T pop_dns = FRAME_DNS (frame_pointer);
        FRAME_DNS (frame_pointer) = frame_pointer;
        EXECUTE_UNIT_TRACE (src);
        genie_check_initialisation (src, stack_top, src_mode);
        STACK_DNS (src, src_mode, frame_pointer);
        FRAME_DNS (frame_pointer) = pop_dns;
/* Make a temporary REF to the object in the frame */
        STATUS (&loc) = (STATUS_MASK) (INIT_MASK | IN_FRAME_MASK);
        REF_HANDLE (&loc) = &nil_handle;
        OFFSET (&loc) = frame_pointer + FRAME_INFO_SIZE + OFFSET (TAX (p));
        REF_SCOPE (&loc) = frame_pointer;
        ABEND (ADDRESS (&loc) != FRAME_OBJECT (OFFSET (TAX (p))), ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
/* Initialise the tag, value is in the stack */
        if (HAS_ROWS (src_mode)) {
          stack_pointer = pop_sp;
          genie_clone_stack (p, src_mode, &loc, &nil_ref);
        } else if (UNIT (&GPROP (src)) == genie_constant) {
          STATUS_SET (TAX (p), CONSTANT_MASK);
          POP_ALIGNED (p, ADDRESS (&loc), size);
        } else {
          POP_ALIGNED (p, ADDRESS (&loc), size);
        }
        return;
    }
  }
}

/*!
\brief execute variable declaration
\param p position in tree
\param declarer pointer to the declarer
**/

void genie_variable_dec (NODE_T * p, NODE_T ** declarer, ADDR_T sp)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, VARIABLE_DECLARATION)) {
      genie_variable_dec (SUB (p), declarer, sp);
    } else {
      if (IS (p, DECLARER)) {
        (*declarer) = SUB (p);
        genie_generator_bounds (*declarer);
        FORWARD (p);
      }
      if (IS (p, DEFINING_IDENTIFIER)) {
        MOID_T *ref_mode = MOID (p);
        TAG_T *tag = TAX (p);
        int leap = (HEAP (tag) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
        A68_REF *z;
        PREEMPTIVE_GC;
        z = (A68_REF *) (FRAME_OBJECT (OFFSET (TAX (p))));
        genie_generator_internal (*declarer, ref_mode, BODY (tag), leap, sp);
        POP_REF (p, z);
        if (NEXT (p) != NO_NODE && IS (NEXT (p), ASSIGN_SYMBOL)) {
          NODE_T *src = NEXT_NEXT (p);
          MOID_T *src_mode = SUB_MOID (p);
          ADDR_T pop_sp = stack_pointer;
          ADDR_T pop_dns = FRAME_DNS (frame_pointer);
          FRAME_DNS (frame_pointer) = frame_pointer;
          EXECUTE_UNIT_TRACE (src);
          STACK_DNS (src, src_mode, frame_pointer);
          FRAME_DNS (frame_pointer) = pop_dns;
          stack_pointer = pop_sp;
          if (HAS_ROWS (src_mode)) {
            genie_clone_stack (p, src_mode, z, z);
          } else {
            MOVE (ADDRESS (z), STACK_TOP, (unsigned) MOID_SIZE (src_mode));
          }
        }
      }
    }
  }
}

/*!
\brief execute PROC variable declaration
\param p position in tree
**/

void genie_proc_variable_dec (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_IDENTIFIER:
      {
        ADDR_T sp_for_voiding = stack_pointer;
        MOID_T *ref_mode = MOID (p);
        TAG_T *tag = TAX (p);
        int leap = (HEAP (tag) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
        A68_REF *z = (A68_REF *) (FRAME_OBJECT (OFFSET (TAX (p))));
        genie_generator_internal (p, ref_mode, BODY (tag), leap, stack_pointer);
        POP_REF (p, z);
        if (NEXT (p) != NO_NODE && IS (NEXT (p), ASSIGN_SYMBOL)) {
          MOID_T *src_mode = SUB_MOID (p);
          ADDR_T pop_sp = stack_pointer;
          ADDR_T pop_dns = FRAME_DNS (frame_pointer);
          FRAME_DNS (frame_pointer) = frame_pointer;
          EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
          STACK_DNS (p, SUB (ref_mode), frame_pointer);
          FRAME_DNS (frame_pointer) = pop_dns;
          stack_pointer = pop_sp;
          MOVE (ADDRESS (z), STACK_TOP, (unsigned) MOID_SIZE (src_mode));
        }
        stack_pointer = sp_for_voiding; /* Voiding */
        return;
      }
    default:
      {
        genie_proc_variable_dec (SUB (p));
        break;
      }
    }
  }
}

/*!
\brief execute operator declaration
\param p position in tree
**/

void genie_operator_dec (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_OPERATOR:
      {
        A68_PROCEDURE *z = (A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (p))));
        ADDR_T pop_dns = FRAME_DNS (frame_pointer);
        FRAME_DNS (frame_pointer) = frame_pointer;
        EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
        STACK_DNS (p, MOID (p), frame_pointer);
        FRAME_DNS (frame_pointer) = pop_dns;
        POP_PROCEDURE (p, z);
        return;
      }
    default:
      {
        genie_operator_dec (SUB (p));
        break;
      }
    }
  }
}

/*!
\brief execute declaration
\param p position in tree
**/

void genie_declaration (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case MODE_DECLARATION:
    case PROCEDURE_DECLARATION:
    case BRIEF_OPERATOR_DECLARATION:
    case PRIORITY_DECLARATION:
      {
/* Already resolved */
        return;
      }
    case IDENTITY_DECLARATION:
      {
        genie_identity_dec (SUB (p));
        break;
      }
    case OPERATOR_DECLARATION:
      {
        genie_operator_dec (SUB (p));
        break;
      }
    case VARIABLE_DECLARATION:
      {
        NODE_T *declarer = NO_NODE;
        ADDR_T pop_sp = stack_pointer;
        genie_variable_dec (SUB (p), &declarer, stack_pointer);
/* Voiding to remove garbage from declarers */
        stack_pointer = pop_sp;
        break;
      }
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        ADDR_T pop_sp = stack_pointer;
        genie_proc_variable_dec (SUB (p));
        stack_pointer = pop_sp;
        break;
      }
    default:
      {
        genie_declaration (SUB (p));
        break;
      }
    }
  }
}

/*
#define LABEL_FREE(p) {\
  NODE_T *_m_q; ADDR_T pop_sp = stack_pointer;\
  for (_m_q = SEQUENCE (p); _m_q != NO_NODE; _m_q = SEQUENCE (_m_q)) {\
    switch (ATTRIBUTE (_m_q)) {\
      case DECLARATION_LIST:
      case UNIT: {\
        EXECUTE_UNIT_TRACE (_m_q);\
        break;\
      }\
      case SEMI_SYMBOL: {\
        stack_pointer = pop_sp;\
        break;\
  }}}}
*/

#define LABEL_FREE(p) {\
  NODE_T *_m_q; ADDR_T pop_sp_lf = stack_pointer;\
  for (_m_q = SEQUENCE (p); _m_q != NO_NODE; _m_q = SEQUENCE (_m_q)) {\
    if (IS (_m_q, UNIT) || IS (_m_q, DECLARATION_LIST)) {\
      EXECUTE_UNIT_TRACE (_m_q);\
    }\
    if (SEQUENCE (_m_q) != NO_NODE) {\
      stack_pointer = pop_sp_lf;\
      _m_q = SEQUENCE (_m_q);\
    }\
  }}

#define SERIAL_CLAUSE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    LABEL_FREE (p);\
  } else {\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) p, (jmp_buf *) exit_buf);\
  }}

#define SERIAL_CLAUSE_TRACE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT_TRACE (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    LABEL_FREE (p);\
  } else {\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) p, (jmp_buf *) exit_buf);\
  }}

#define ENQUIRY_CLAUSE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    LABEL_FREE (p);\
  } else {\
    genie_enquiry_clause ((NODE_T *) p);\
  }

/*!
\brief execute integral-case-clause
\param p position in tree
**/

static PROP_T genie_int_case (volatile NODE_T * p)
{
  volatile int unit_count;
  volatile BOOL_T found_unit;
  jmp_buf exit_buf;
  A68_INT k;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* CASE or OUSE */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  POP_OBJECT (q, &k, A68_INT);
/* IN */
  FORWARD (q);
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  unit_count = 1;
  found_unit = genie_int_case_unit (NEXT_SUB ((NODE_T *) q), (int) VALUE (&k), (int *) &unit_count);
  CLOSE_FRAME;
/* OUT */
  if (!found_unit) {
    FORWARD (q);
    switch (ATTRIBUTE (q)) {
    case CHOICE:
    case OUT_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
        INIT_STATIC_FRAME ((NODE_T *) SUB (q));
        SERIAL_CLAUSE (NEXT_SUB (q));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case ESAC_SYMBOL:
      {
        if (yield != MODE (VOID)) {
          genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
        }
        break;
      }
    default:
      {
        MOID (SUB ((NODE_T *) q)) = (MOID_T *) yield;
        (void) genie_int_case (q);
        break;
      }
    }
  }
/* ESAC */
  CLOSE_FRAME;
  return (GPROP (p));
}

/*!
\brief execute united-case-clause
\param p position in tree
**/

static PROP_T genie_united_case (volatile NODE_T * p)
{
  volatile BOOL_T found_unit = A68_FALSE;
  volatile MOID_T *um;
  volatile ADDR_T pop_sp;
  jmp_buf exit_buf;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* CASE or OUSE */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  pop_sp = stack_pointer;
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  stack_pointer = pop_sp;
  um = (volatile MOID_T *) VALUE ((A68_UNION *) STACK_TOP);
/* IN */
  FORWARD (q);
  if (um != NO_MOID) {
    OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
    INIT_STATIC_FRAME ((NODE_T *) SUB (q));
    found_unit = genie_united_case_unit (NEXT_SUB ((NODE_T *) q), (MOID_T *) um);
    CLOSE_FRAME;
  } else {
    found_unit = A68_FALSE;
  }
/* OUT */
  if (!found_unit) {
    FORWARD (q);
    switch (ATTRIBUTE (q)) {
    case CHOICE:
    case OUT_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
        INIT_STATIC_FRAME ((NODE_T *) SUB (q));
        SERIAL_CLAUSE (NEXT_SUB (q));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case ESAC_SYMBOL:
      {
        if (yield != MODE (VOID)) {
          genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
        }
        break;
      }
    default:
      {
        MOID (SUB ((NODE_T *) q)) = (MOID_T *) yield;
        (void) genie_united_case (q);
        break;
      }
    }
  }
/* ESAC */
  CLOSE_FRAME;
  return (GPROP (p));
}

/*!
\brief execute conditional-clause
\param p position in tree
**/

static PROP_T genie_conditional (volatile NODE_T * p)
{
  volatile int pop_sp = stack_pointer;
  jmp_buf exit_buf;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* IF or ELIF */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  stack_pointer = pop_sp;
  FORWARD (q);
  if (VALUE ((A68_BOOL *) STACK_TOP) == A68_TRUE) {
/* THEN */
    OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
    INIT_STATIC_FRAME ((NODE_T *) SUB (q));
    SERIAL_CLAUSE (NEXT_SUB (q));
    CLOSE_FRAME;
  } else {
/* ELSE */
    FORWARD (q);
    switch (ATTRIBUTE (q)) {
    case CHOICE:
    case ELSE_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
        INIT_STATIC_FRAME ((NODE_T *) SUB (q));
        SERIAL_CLAUSE (NEXT_SUB (q));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case FI_SYMBOL:
      {
        if (yield != MODE (VOID)) {
          genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
        }
        break;
      }
    default:
      {
        MOID (SUB ((NODE_T *) q)) = (MOID_T *) yield;
        (void) genie_conditional (q);
        break;
      }
    }
  }
/* FI */
  CLOSE_FRAME;
  return (GPROP (p));
}

/*
INCREMENT_COUNTER procures that the counter only increments if there is
a for-part or a to-part. Otherwise an infinite loop would trigger overflow
when the anonymous counter reaches max int, which is strange behaviour.
*/

#define INCREMENT_COUNTER\
  if (!(for_part == NO_NODE && to_part == NO_NODE)) {\
    CHECK_INT_ADDITION ((NODE_T *) p, counter, by);\
    counter += by;\
  }

/*!
\brief execute loop-clause
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_loop (volatile NODE_T * p)
{
  volatile ADDR_T pop_sp = stack_pointer;
  volatile int from, by, to, counter;
  volatile BOOL_T siga, conditional;
/*
Next line provokes inevitably:
  warning: variable 'for_part' might be clobbered by 'longjmp' or 'vfork'
  warning: variable 'to_part' might be clobbered by 'longjmp' or 'vfork'
This warning can be safely ignored.
*/
  volatile NODE_T *for_part = NO_NODE, *to_part = NO_NODE, *q = NO_NODE;
  jmp_buf exit_buf;
/* FOR  identifier */
  if (IS (p, FOR_PART)) {
    for_part = NEXT_SUB (p);
    FORWARD (p);
  }
/* FROM unit */
  if (IS (p, FROM_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    from = VALUE ((A68_INT *) STACK_TOP);
    FORWARD (p);
  } else {
    from = 1;
  }
/* BY unit */
  if (IS (p, BY_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    by = VALUE ((A68_INT *) STACK_TOP);
    FORWARD (p);
  } else {
    by = 1;
  }
/* TO unit, DOWNTO unit */
  if (IS (p, TO_PART)) {
    if (IS (SUB (p), DOWNTO_SYMBOL)) {
      by = -by;
    }
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    to = VALUE ((A68_INT *) STACK_TOP);
    to_part = p;
    FORWARD (p);
  } else {
    to = (by >= 0 ? A68_MAX_INT : -A68_MAX_INT);
  }
  q = NEXT_SUB (p);
/* Here the loop part starts.
   We open the frame only once and reinitialise if necessary */
  OPEN_STATIC_FRAME ((NODE_T *) q);
  INIT_GLOBAL_POINTER ((NODE_T *) q);
  INIT_STATIC_FRAME ((NODE_T *) q);
  counter = from;
/* Does the loop contain conditionals? */
  if (IS (p, WHILE_PART)) {
    conditional = A68_TRUE;
  } else if (IS (p, DO_PART) || IS (p, ALT_DO_PART)) {
    NODE_T *until_part = NEXT_SUB (p);
    if (IS (until_part, SERIAL_CLAUSE)) {
      until_part = NEXT (until_part);
    }
    conditional = (BOOL_T) (until_part != NO_NODE && IS (until_part, UNTIL_PART));
  } else {
    conditional = A68_FALSE;
  }
  if (conditional) {
/* [FOR ...] [WHILE ...] DO [...] [UNTIL ...] OD */
    siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
    while (siga) {
      if (for_part != NO_NODE) {
        A68_INT *z = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (for_part))));
        STATUS (z) = INIT_MASK;
        VALUE (z) = counter;
      }
      stack_pointer = pop_sp;
      if (IS (p, WHILE_PART)) {
        ENQUIRY_CLAUSE (q);
        stack_pointer = pop_sp;
        siga = (BOOL_T) (VALUE ((A68_BOOL *) STACK_TOP) != A68_FALSE);
      }
      if (siga) {
/*
Next line provokes inevitably:
  warning: variable 'do_p' might be clobbered by 'longjmp' or 'vfork'
This warning can be safely ignored.
*/
        volatile NODE_T *do_part = p, *until_part;
        if (IS (p, WHILE_PART)) {
          do_part = NEXT_SUB (NEXT (p));
          OPEN_STATIC_FRAME ((NODE_T *) do_part);
          INIT_STATIC_FRAME ((NODE_T *) do_part);
        } else {
          do_part = NEXT_SUB (p);
        }
        if (IS (do_part, SERIAL_CLAUSE)) {
          SERIAL_CLAUSE_TRACE (do_part);
          until_part = NEXT (do_part);
        } else {
          until_part = do_part;
        }
/* UNTIL part */
        if (until_part != NO_NODE && IS (until_part, UNTIL_PART)) {
          NODE_T *v = NEXT_SUB (until_part);
          OPEN_STATIC_FRAME ((NODE_T *) v);
          INIT_STATIC_FRAME ((NODE_T *) v);
          stack_pointer = pop_sp;
          ENQUIRY_CLAUSE (v);
          stack_pointer = pop_sp;
          siga = (BOOL_T) (VALUE ((A68_BOOL *) STACK_TOP) == A68_FALSE);
          CLOSE_FRAME;
        }
        if (IS (p, WHILE_PART)) {
          CLOSE_FRAME;
        }
/* Increment counter */
        if (siga) {
          INCREMENT_COUNTER;
          siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
        }
/* The genie cannot take things to next iteration: re-initialise stack frame */
        if (siga) {
          FRAME_CLEAR (AP_INCREMENT (TABLE (q)));
          if (INITIALISE_FRAME (TABLE (q))) {
            initialise_frame ((NODE_T *) q);
          }
        }
      }
    }
  } else {
/* [FOR ...] DO ... OD */
    siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
    while (siga) {
      if (for_part != NO_NODE) {
        A68_INT *z = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (for_part))));
        STATUS (z) = INIT_MASK;
        VALUE (z) = counter;
      }
      stack_pointer = pop_sp;
      SERIAL_CLAUSE_TRACE (q);
      INCREMENT_COUNTER;
      siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
/* The genie cannot take things to next iteration: re-initialise stack frame */
      if (siga) {
        FRAME_CLEAR (AP_INCREMENT (TABLE (q)));
        if (INITIALISE_FRAME (TABLE (q))) {
          initialise_frame ((NODE_T *) q);
        }
      }
    }
  }
/* OD */
  CLOSE_FRAME;
  stack_pointer = pop_sp;
  return (GPROP (p));
}

#undef INCREMENT_COUNTER
#undef LOOP_OVERFLOW

/*!
\brief execute closed clause
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_closed (volatile NODE_T * p)
{
  jmp_buf exit_buf;
  volatile NODE_T *q = NEXT_SUB (p);
  OPEN_STATIC_FRAME ((NODE_T *) q);
  INIT_GLOBAL_POINTER ((NODE_T *) q);
  INIT_STATIC_FRAME ((NODE_T *) q);
  SERIAL_CLAUSE (q);
  CLOSE_FRAME;
  return (GPROP (p));
}

/*!
\brief execute enclosed clause
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_enclosed (volatile NODE_T * p)
{
  PROP_T self;
  UNIT (&self) = (PROP_PROC *) genie_enclosed;
  SOURCE (&self) = (NODE_T *) p;
  switch (ATTRIBUTE (p)) {
  case PARTICULAR_PROGRAM:
    {
      self = genie_enclosed (SUB (p));
      break;
    }
  case ENCLOSED_CLAUSE:
    {
      self = genie_enclosed (SUB (p));
      break;
    }
  case CLOSED_CLAUSE:
    {
      self = genie_closed ((NODE_T *) p);
      if (UNIT (&self) == genie_unit) {
        UNIT (&self) = (PROP_PROC *) genie_closed;
        SOURCE (&self) = (NODE_T *) p;
      }
      break;
    }
#if defined HAVE_PARALLEL_CLAUSE
  case PARALLEL_CLAUSE:
    {
      (void) genie_parallel ((NODE_T *) NEXT_SUB (p));
      break;
    }
#endif
  case COLLATERAL_CLAUSE:
    {
      (void) genie_collateral ((NODE_T *) p);
      break;
    }
  case CONDITIONAL_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_conditional (p);
      UNIT (&self) = (PROP_PROC *) genie_conditional;
      SOURCE (&self) = (NODE_T *) p;
      break;
    }
  case CASE_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_int_case (p);
      UNIT (&self) = (PROP_PROC *) genie_int_case;
      SOURCE (&self) = (NODE_T *) p;
      break;
    }
  case CONFORMITY_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_united_case (p);
      UNIT (&self) = (PROP_PROC *) genie_united_case;
      SOURCE (&self) = (NODE_T *) p;
      break;
    }
  case LOOP_CLAUSE:
    {
      (void) genie_loop (SUB ((NODE_T *) p));
      UNIT (&self) = (PROP_PROC *) genie_loop;
      SOURCE (&self) = SUB ((NODE_T *) p);
      break;
    }
  }
  GPROP (p) = self;
  return (self);
}

/*
Routines for handling stowed objects.

An A68G row is a reference to a descriptor in the heap:

               ...
A68_REF row -> A68_ARRAY ----+   ARRAY: Description of row, ref to elements
               A68_TUPLE 1   |   TUPLE: Bounds, one for every dimension
               ...           |
               A68_TUPLE dim |
               ...           |
               ...           |
               Element 1 <---+   Element: Sequential row elements, in the heap
               ...                        Not always contiguous - trims!
               Element n
*/

/*!
\brief size of a row
\param tup first tuple
\param tup dimension of row
\return same
**/

int get_row_size (A68_TUPLE * tup, int dim)
{
  int span = 1, k;
  for (k = 0; k < dim; k++) {
    int stride = ROW_SIZE (&tup[k]);
    ABEND ((stride > 0 && span > A68_MAX_INT / stride), ERROR_INVALID_SIZE, "get_row_size");
    span *= stride;
  }
  return (span);
}

/*!
\brief initialise index for FORALL constructs
\param tup first tuple
\param tup dimension of row
**/

void initialise_internal_index (A68_TUPLE * tup, int dim)
{
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    K (ref) = LWB (ref);
  }
}

/*!
\brief calculate index
\param tup first tuple
\param tup dimension of row
\return same
**/

ADDR_T calculate_internal_index (A68_TUPLE * tup, int dim)
{
  ADDR_T iindex = 0;
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    iindex += (SPAN (ref) * K (ref) - SHIFT (ref));
  }
  return (iindex);
}

/*!
\brief increment index for FORALL constructs
\param tup first tuple
\param tup dimension of row
\return whether maximum (index + 1) is reached
**/

BOOL_T increment_internal_index (A68_TUPLE * tup, int dim)
{
  BOOL_T carry = A68_TRUE;
  int k;
  for (k = dim - 1; k >= 0 && carry; k--) {
    A68_TUPLE *ref = &tup[k];
    if (K (ref) < UPB (ref)) {
      (K (ref))++;
      carry = A68_FALSE;
    } else {
      K (ref) = LWB (ref);
    }
  }
  return (carry);
}

/*!
\brief print index
\param tup first tuple
\param tup dimension of row
**/

void print_internal_index (FILE_T f, A68_TUPLE * tup, int dim)
{
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    char buf[BUFFER_SIZE];
    ASSERT (snprintf (buf, SNPRINTF_SIZE, "%d", K (ref)) >= 0);
    WRITE (f, buf);
    if (k < dim - 1) {
      WRITE (f, ", ");
    }
  }
}

/*!
\brief convert C string to A68 [] CHAR
\param p position in tree
\param str string to convert
\param width width of string
\return pointer to [] CHAR
**/

A68_REF c_string_to_row_char (NODE_T * p, char *str, int width)
{
  A68_REF z, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  BYTE_T *base;
  int str_size, k;
  str_size = (int) strlen (str);
  z = heap_generator (p, MODE (ROW_CHAR), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  row = heap_generator (p, MODE (ROW_CHAR), width * ALIGNED_SIZE_OF (A68_CHAR));
  DIM (&arr) = 1;
  MOID (&arr) = MODE (CHAR);
  ELEM_SIZE (&arr) = ALIGNED_SIZE_OF (A68_CHAR);
  SLICE_OFFSET (&arr) = 0;
  FIELD_OFFSET (&arr) = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = width;
  SPAN (&tup) = 1;
  SHIFT (&tup) = LWB (&tup);
  K (&tup) = 0;
  PUT_DESCRIPTOR (arr, tup, &z);
  base = ADDRESS (&row);
  for (k = 0; k < width; k++) {
    A68_CHAR *ch = (A68_CHAR *) & (base[k * ALIGNED_SIZE_OF (A68_CHAR)]);
    STATUS (ch) = INIT_MASK;
    VALUE (ch) = TO_UCHAR (str[k]);
  }
  return (z);
}

/*!
\brief convert C string to A68 string
\param p position in tree
\param str string to convert
\return STRING
**/

A68_REF c_to_a_string (NODE_T * p, char *str, int width)
{
  if (str == NO_TEXT) {
    return (empty_string (p));
  } else {
    if (width == DEFAULT_WIDTH) {
      return (c_string_to_row_char (p, str, (int) strlen (str)));
    } else {
      return (c_string_to_row_char (p, str, (int) width));
    }
  }
}

/*!
\brief size of a string
\param p position in tree
\param row row, pointer to descriptor
\return same
**/

int a68_string_size (NODE_T * p, A68_REF row)
{
  (void) p;
  if (INITIALISED (&row)) {
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    GET_DESCRIPTOR (arr, tup, &row);
    return (ROW_SIZE (tup));
  } else {
    return (0);
  }
}

/*!
\brief convert A68 string to C string
\param p position in tree
\param str string to store result
\param row STRING to convert
\return str
**/

char *a_to_c_string (NODE_T * p, char *str, A68_REF row)
{
/* Assume "str" to be long enough - caller's responsibility! */
  (void) p;
  if (INITIALISED (&row)) {
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    int size, n = 0;
    GET_DESCRIPTOR (arr, tup, &row);
    size = ROW_SIZE (tup);
    if (size > 0) {
      int k;
      BYTE_T *base_address = ADDRESS (&ARRAY (arr));
      for (k = LWB (tup); k <= UPB (tup); k++) {
        int addr = INDEX_1_DIM (arr, tup, k);
        A68_CHAR *ch = (A68_CHAR *) & (base_address[addr]);
        CHECK_INIT (p, INITIALISED (ch), MODE (CHAR));
        str[n++] = (char) VALUE (ch);
      }
    }
    str[n] = NULL_CHAR;
    return (str);
  } else {
    return (NO_TEXT);
  }
}

/*!
\brief return an empty row
\param p position in tree
\param u mode of row
\return fat pointer to descriptor
**/

A68_REF empty_row (NODE_T * p, MOID_T * u)
{
  A68_REF dsc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  MOID_T *v;
  int dim, k;
  if (IS (u, FLEX_SYMBOL)) {
    u = SUB (u);
  }
  v = SUB (u);
  dim = DIM (u);
  dsc = heap_generator (p, u, ALIGNED_SIZE_OF (A68_ARRAY) + dim * ALIGNED_SIZE_OF (A68_TUPLE));
  GET_DESCRIPTOR (arr, tup, &dsc);
  DIM (arr) = dim;
  MOID (arr) = SLICE (u);
  ELEM_SIZE (arr) = moid_size (SLICE (u));
  SLICE_OFFSET (arr) = 0;
  FIELD_OFFSET (arr) = 0;
  if (IS (v, ROW_SYMBOL) || IS (v, FLEX_SYMBOL)) {
    /* [] AMODE or FLEX [] AMODE */
    ARRAY (arr) = heap_generator (p, v, A68_REF_SIZE);
    * DEREF (A68_REF,&ARRAY (arr)) = empty_row (p, v);
  } else {
    ARRAY (arr) = nil_ref;
  }
  STATUS (&ARRAY (arr)) = (STATUS_MASK) (INIT_MASK | IN_HEAP_MASK);
  for (k = 0; k < dim; k++) {
    LWB (&tup[k]) = 1;
    UPB (&tup[k]) = 0;
    SPAN (&tup[k]) = 1;
    SHIFT (&tup[k]) = LWB (tup);
  }
  return (dsc);
}

/*!
\brief an empty string, FLEX [1 : 0] CHAR
\param p position in tree
\return fat pointer to descriptor
**/

A68_REF empty_string (NODE_T * p)
{
  return (empty_row (p, MODE (STRING)));
}

/*!
\brief make [,, ..] MODE  from [, ..] MODE
\param p position in tree
\param rmod row of mode
\param len number of elements
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_make_rowrow (NODE_T * p, MOID_T * rmod, int len, ADDR_T sp)
{
  MOID_T *nmod = IS (rmod, FLEX_SYMBOL) ? SUB (rmod) : rmod;
  MOID_T *emod = SUB (nmod);
  A68_REF nrow, orow;
  A68_ARRAY *narr, *oarr;
  A68_TUPLE *ntup, *otup;
  int j, k, span, odim = DIM (nmod) - 1;
/* Make the new descriptor */
  nrow = heap_generator (p, rmod, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (nmod) * ALIGNED_SIZE_OF (A68_TUPLE));
  GET_DESCRIPTOR (narr, ntup, &nrow);
  DIM (narr) = DIM (nmod);
  MOID (narr) = emod;
  ELEM_SIZE (narr) = MOID_SIZE (emod);
  SLICE_OFFSET (narr) = 0;
  FIELD_OFFSET (narr) = 0;
  if (len == 0) {
/* There is a vacuum on the stack */
    for (k = 0; k < odim; k++) {
      LWB (&ntup[k + 1]) = 1;
      UPB (&ntup[k + 1]) = 0;
      SPAN (&ntup[k + 1]) = 1;
      SHIFT (&ntup[k + 1]) = LWB (&ntup[k + 1]);
    }
    LWB (ntup) = 1;
    UPB (ntup) = 0;
    SPAN (ntup) = 0;
    SHIFT (ntup) = 0;
    ARRAY (narr) = nil_ref;
    return (nrow);
  } else if (len > 0) {
    A68_ARRAY *x = NO_ARRAY;
/* Arrays in the stack must have equal bounds */
    for (j = 1; j < len; j++) {
      A68_REF vrow, rrow;
      A68_TUPLE *vtup, *rtup;
      rrow = *(A68_REF *) STACK_ADDRESS (sp);
      vrow = *(A68_REF *) STACK_ADDRESS (sp + j * A68_REF_SIZE);
      GET_DESCRIPTOR (x, rtup, &rrow);
      GET_DESCRIPTOR (x, vtup, &vrow);
      for (k = 0; k < odim; k++, rtup++, vtup++) {
        if ((UPB (rtup) != UPB (vtup)) || (LWB (rtup) != LWB (vtup))) {
          diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
    }
/* Fill descriptor of new row with info from (arbitrary) first one */
    orow = *(A68_REF *) STACK_ADDRESS (sp);
    GET_DESCRIPTOR (x, otup, &orow);
    for (span = 1, k = 0; k < odim; k++) {
      A68_TUPLE *nt = &ntup[k + 1], *ot = &otup[k];
      LWB (nt) = LWB (ot);
      UPB (nt) = UPB (ot);
      SPAN (nt) = span;
      SHIFT (nt) = LWB (nt) * SPAN (nt);
      span *= ROW_SIZE (nt);
    }
    LWB (ntup) = 1;
    UPB (ntup) = len;
    SPAN (ntup) = span;
    SHIFT (ntup) = LWB (ntup) * SPAN (ntup);
    ARRAY (narr) = heap_generator (p, rmod, len * span * ELEM_SIZE (narr));
    for (j = 0; j < len; j++) {
/* new[j,, ] := old[, ] */
      BOOL_T done;
      GET_DESCRIPTOR (oarr, otup, (A68_REF *) STACK_ADDRESS (sp + j * A68_REF_SIZE));
      initialise_internal_index (otup, odim);
      initialise_internal_index (&ntup[1], odim);
      done = A68_FALSE;
      while (!done) {
        A68_REF src = ARRAY (oarr), dst = ARRAY (narr);
        ADDR_T oindex, nindex;
        oindex = calculate_internal_index (otup, odim);
        nindex = j * SPAN (ntup) + calculate_internal_index (&ntup[1], odim);
        OFFSET (&src) += ROW_ELEMENT (oarr, oindex);
        OFFSET (&dst) += ROW_ELEMENT (narr, nindex);
        if (HAS_ROWS (emod)) {
          A68_REF none = genie_clone (p, emod, &nil_ref, &src);
          MOVE (ADDRESS (&dst), ADDRESS (&none), MOID_SIZE (emod));
        } else {
          MOVE (ADDRESS (&dst), ADDRESS (&src), MOID_SIZE (emod));
        }
        done = increment_internal_index (otup, odim) | increment_internal_index (&ntup[1], odim);
      }
    }
  }
  return (nrow);
}

/*!
\brief make a row of 'len' objects that are in the stack
\param p position in tree
\param elem_mode mode of element
\param len number of elements in the stack
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_make_row (NODE_T * p, MOID_T * elem_mode, int len, ADDR_T sp)
{
  A68_REF new_row, new_arr;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int k;
  new_row = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  new_arr = heap_generator (p, MOID (p), len * MOID_SIZE (elem_mode));
  GET_DESCRIPTOR (arr, tup, &new_row);
  DIM (arr) = 1;
  MOID (arr) = elem_mode;
  ELEM_SIZE (arr) = MOID_SIZE (elem_mode);
  SLICE_OFFSET (arr) = 0;
  FIELD_OFFSET (arr) = 0;
  ARRAY (arr) = new_arr;
  LWB (tup) = 1;
  UPB (tup) = len;
  SPAN (tup)= 1;
  SHIFT (tup) = LWB (tup);
  for (k = 0; k < len * ELEM_SIZE (arr); k += ELEM_SIZE (arr)) {
    A68_REF dst = new_arr, src;
    OFFSET (&dst) += k;
    STATUS (&src) = (STATUS_MASK) (INIT_MASK | IN_STACK_MASK);
    OFFSET (&src) = sp + k;
    REF_HANDLE (&src) = &nil_handle;
    if (HAS_ROWS (elem_mode)) {
      A68_REF new_one = genie_clone (p, elem_mode, &nil_ref, &src);
      MOVE (ADDRESS (&dst), ADDRESS (&new_one), MOID_SIZE (elem_mode));
    } else {
      MOVE (ADDRESS (&dst), ADDRESS (&src), MOID_SIZE (elem_mode));
    }
  }
  return (new_row);
}

/*!
\brief make REF [1 : 1] [] MODE from REF [] MODE
\param p position in tree
\param dst_mode destination mode
\param src_mode source mode
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_make_ref_row_of_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  A68_REF new_row, name, array;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  array = *(A68_REF *) STACK_ADDRESS (sp);
/* ROWING NIL yields NIL */
  if (IS_NIL (array)) {
    return (nil_ref);
  }
  new_row = heap_generator (p, SUB (dst_mode), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  name = heap_generator (p, dst_mode, A68_REF_SIZE);
  GET_DESCRIPTOR (arr, tup, &new_row);
  DIM (arr) = 1;
  MOID (arr) = src_mode;
  ELEM_SIZE (arr) = MOID_SIZE (src_mode);
  SLICE_OFFSET (arr) = 0;
  FIELD_OFFSET (arr) = 0;
  ARRAY (arr) = array;
  LWB (tup) = 1;
  UPB (tup) = 1;
  SPAN (tup) = 1;
  SHIFT (tup) = LWB (tup);
  * DEREF (A68_REF, &name) = new_row;
  return (name);
}

/*!
\brief make REF [1 : 1, ..] MODE from REF [..] MODE
\param p position in tree
\param dst_mode destination mode
\param src_mode source mode
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_make_ref_row_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  A68_REF name, new_row, old_row;
  A68_ARRAY *new_arr, *old_arr;
  A68_TUPLE *new_tup, *old_tup;
  int k;
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  name = *(A68_REF *) STACK_ADDRESS (sp);
/* ROWING NIL yields NIL */
  if (IS_NIL (name)) {
    return (nil_ref);
  }
  old_row = * DEREF (A68_REF, &name);
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
/* Make new descriptor */
  new_row = heap_generator (p, dst_mode, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (SUB (dst_mode)) * ALIGNED_SIZE_OF (A68_TUPLE));
  name = heap_generator (p, dst_mode, A68_REF_SIZE);
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (SUB (dst_mode));
  MOID (new_arr) = MOID (old_arr);
  ELEM_SIZE (new_arr) = ELEM_SIZE (old_arr);
  SLICE_OFFSET (new_arr) = 0;
  FIELD_OFFSET (new_arr) = 0;
  ARRAY (new_arr) = ARRAY (old_arr);
/* Fill out the descriptor */
  LWB (&(new_tup[0])) = 1;
  UPB (&(new_tup[0])) = 1;
  SPAN (&(new_tup[0])) = 1;
  SHIFT (&(new_tup[0])) = LWB (&(new_tup[0]));
  for (k = 0; k < DIM (SUB (src_mode)); k++) {
    new_tup[k + 1] = old_tup[k];
  }
/* Yield the new name */
  * DEREF (A68_REF, &name) = new_row;
  return (name);
}

/*!
\brief coercion to [1 : 1, ] MODE
\param p position in tree
\return propagator for this action
**/

static PROP_T genie_rowing_row_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), frame_pointer);
  row = genie_make_rowrow (p, MOID (p), 1, sp);
  stack_pointer = sp;
  PUSH_REF (p, row);
  return (GPROP (p));
}

/*!
\brief coercion to [1 : 1] [] MODE
\param p position in tree
\return propagator for this action
**/

static PROP_T genie_rowing_row_of_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), frame_pointer);
  row = genie_make_row (p, SLICE (MOID (p)), 1, sp);
  stack_pointer = sp;
  PUSH_REF (p, row);
  return (GPROP (p));
}

/*!
\brief coercion to REF [1 : 1, ..] MODE
\param p position in tree
\return propagator for this action
**/

static PROP_T genie_rowing_ref_row_row (NODE_T * p)
{
  A68_REF name;
  ADDR_T sp = stack_pointer;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), frame_pointer);
  stack_pointer = sp;
  name = genie_make_ref_row_row (p, dst, src, sp);
  PUSH_REF (p, name);
  return (GPROP (p));
}

/*!
\brief REF [1 : 1] [] MODE from [] MODE
\param p position in tree
\return propagator for this action
**/

static PROP_T genie_rowing_ref_row_of_row (NODE_T * p)
{
  A68_REF name;
  ADDR_T sp = stack_pointer;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), frame_pointer);
  stack_pointer = sp;
  name = genie_make_ref_row_of_row (p, dst, src, sp);
  PUSH_REF (p, name);
  return (GPROP (p));
}

/*!
\brief rowing coercion
\param p position in tree
\return propagator for this action
**/

static PROP_T genie_rowing (NODE_T * p)
{
  PROP_T self;
  if (IS (MOID (p), REF_SYMBOL)) {
/* REF ROW, decide whether we want A->[] A or [] A->[,] A */
    MOID_T *mode = SUB_MOID (p);
    if (DIM (DEFLEX (mode)) >= 2) {
      (void) genie_rowing_ref_row_row (p);
      UNIT (&self) = genie_rowing_ref_row_row;
      SOURCE (&self) = p;
    } else {
      (void) genie_rowing_ref_row_of_row (p);
      UNIT (&self) = genie_rowing_ref_row_of_row;
      SOURCE (&self) = p;
    }
  } else {
/* ROW, decide whether we want A->[] A or [] A->[,] A */
    if (DIM (DEFLEX (MOID (p))) >= 2) {
      (void) genie_rowing_row_row (p);
      UNIT (&self) = genie_rowing_row_row;
      SOURCE (&self) = p;
    } else {
      (void) genie_rowing_row_of_row (p);
      UNIT (&self) = genie_rowing_row_of_row;
      SOURCE (&self) = p;
    }
  }
  return (self);
}

/*!
\brief clone a compounded value refered to by 'old'
\param p position in tree
\param m mode of object
\param old fat pointer to old object
\param template used for bound checks, NIL if irrelevant
\return fat pointer to descriptor or structured value
**/

A68_REF genie_clone (NODE_T * p, MOID_T *m, A68_REF *tmp, A68_REF *old)
{
/*
This complex routine is needed as arrays are not always contiguous.
The routine takes a REF to the value and returns a REF to the clone.
*/
  if (m == MODE (SOUND)) {
/* REF SOUND */
    A68_REF nsound;
    A68_SOUND *w;
    int size;
    BYTE_T *owd;
    nsound = heap_generator (p, m, MOID_SIZE (m));
    w = DEREF (A68_SOUND, &nsound);
    size = A68_SOUND_DATA_SIZE (w);
    COPY ((BYTE_T *) w, ADDRESS (old), MOID_SIZE (MODE (SOUND)));
    owd = ADDRESS (&(DATA (w)));
    DATA (w) = heap_generator (p, MODE (SOUND_DATA), size);
    COPY (ADDRESS (&(DATA (w))), owd, size);
    return (nsound);
  } else if (IS (m, STRUCT_SYMBOL)) {
/* REF STRUCT */
    PACK_T *fds;
    A68_REF nstruct;
    nstruct = heap_generator (p, m, MOID_SIZE (m));
    for (fds = PACK (m); fds != NO_PACK; FORWARD (fds)) {
      MOID_T *fm = MOID (fds);
      A68_REF of = *old, nf = nstruct, tf = *tmp;
      OFFSET (&of) += OFFSET (fds); 
      OFFSET (&nf) += OFFSET (fds);
      if (! IS_NIL (tf)) {
        OFFSET (&tf) += OFFSET (fds);
      }
      if (HAS_ROWS (fm)) {
        A68_REF a68_clone = genie_clone (p, fm, &tf, &of);
        MOVE (ADDRESS (&nf), ADDRESS (&a68_clone), MOID_SIZE (fm));
      } else {
        MOVE (ADDRESS (&nf), ADDRESS (&of), MOID_SIZE (fm));
      }
    }
    return (nstruct);
  } else if (IS (m, UNION_SYMBOL)) {
/* REF UNION */
    A68_REF nunion, src, dst, tmpu;
    A68_UNION *u;
    MOID_T *um;
    nunion = heap_generator (p, m, MOID_SIZE (m));
    src = *old;
    u = DEREF (A68_UNION, &src);
    um = (MOID_T *) VALUE (u);
    OFFSET (&src) += UNION_OFFSET;
    dst = nunion;
    * DEREF (A68_UNION, &dst) = *u;
    OFFSET (&dst) += UNION_OFFSET;
/* A union has formal members, so tmp is irrelevant */
    tmpu = nil_ref;
    if (um != NO_MOID && HAS_ROWS (um)) {
      A68_REF a68_clone = genie_clone (p, um, &tmpu, &src);
      MOVE (ADDRESS (&dst), ADDRESS (&a68_clone), MOID_SIZE (um));
    } else if (um != NO_MOID) {
      MOVE (ADDRESS (&dst), ADDRESS (&src), MOID_SIZE (um));
    }
    return (nunion);
  } else if (IF_ROW (m)) {
/* REF [FLEX] [] */
    A68_REF nrow, ntmp, heap;
    A68_ARRAY *oarr, *narr, *tarr;
    A68_TUPLE *otup, *ntup, *ttup = NO_TUPLE, *op, *np, *tp;
    MOID_T *em = SUB (IS (m, FLEX_SYMBOL) ? SUB (m) : m);
    int k, span;
    BOOL_T check_bounds;
/* Make new array */
    GET_DESCRIPTOR (oarr, otup, DEREF (A68_REF, old));
    nrow = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (oarr) * ALIGNED_SIZE_OF (A68_TUPLE));
/* Now fill the new descriptor */
    GET_DESCRIPTOR (narr, ntup, &nrow);
    DIM (narr) = DIM (oarr);
    MOID (narr) = MOID (oarr);
    ELEM_SIZE (narr) = ELEM_SIZE (oarr);
    SLICE_OFFSET (narr) = 0;
    FIELD_OFFSET (narr) = 0;
/* 
Get size and copy bounds; check in case of a row.
This is just song and dance to comply with the RR.
*/
    check_bounds = A68_FALSE;
    if (IS_NIL (*tmp)) {
      ntmp = nil_ref;
    } else {
      A68_REF *z = DEREF (A68_REF, tmp);
      if (!IS_NIL (*z)) {
        GET_DESCRIPTOR (tarr, ttup, z);
        ntmp = ARRAY (tarr);
        check_bounds = IS (m, ROW_SYMBOL);
      }
    }
    for (span = 1, k = 0; k < DIM (oarr); k++) {
      op = &otup[k];
      np = &ntup[k];
      if (check_bounds) {
        tp = &ttup[k];
        if (UPB (tp) != UPB (op) || LWB (tp) != LWB (op)) {
          diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
      LWB (np) = LWB (op);
      UPB (np) = UPB (op);
      SPAN (np) = span;
      SHIFT (np) = LWB (np) * SPAN (np);
      span *= ROW_SIZE (np);
    }
/* Make a new array with at least a ghost element */
    if (span == 0) {
      ARRAY (narr) = heap_generator (p, em, ELEM_SIZE (narr));
    } else {
      ARRAY (narr) = heap_generator (p, em, span * ELEM_SIZE (narr));
    }
/* Copy the ghost element if there are no elements */
    if (span == 0 && HAS_ROWS (em)) {
      A68_REF nold, ndst, a68_clone;
      nold = ARRAY (oarr); 
      OFFSET (&nold) += ROW_ELEMENT (oarr, 0); 
      ndst = ARRAY (narr); 
      OFFSET (&ndst) += ROW_ELEMENT (narr, 0);
      a68_clone = genie_clone (p, em, &ntmp, &nold);
      MOVE (ADDRESS (&ndst), ADDRESS (&a68_clone), MOID_SIZE (em));
    } else if (span > 0) {
/* The n-dimensional copier */
      BOOL_T done = A68_FALSE;
      initialise_internal_index (otup, DIM (oarr));
      initialise_internal_index (ntup, DIM (narr));
      while (!done) {
        A68_REF nold = ARRAY (oarr);
        A68_REF ndst = ARRAY (narr);
        ADDR_T oindex = calculate_internal_index (otup, DIM (oarr));
        ADDR_T nindex = calculate_internal_index (ntup, DIM (narr));
        OFFSET (&nold) += ROW_ELEMENT (oarr, oindex);
        OFFSET (&ndst) += ROW_ELEMENT (narr, nindex);
        if (HAS_ROWS (em)) {
          A68_REF a68_clone;
          a68_clone = genie_clone (p, em, &ntmp, &nold);
          MOVE (ADDRESS (&ndst), ADDRESS (&a68_clone), MOID_SIZE (em));
        } else {
          MOVE (ADDRESS (&ndst), ADDRESS (&nold), MOID_SIZE (em));
        }
/* Increase pointers */
        done = increment_internal_index (otup, DIM (oarr))
             | increment_internal_index (ntup, DIM (narr));
      }
    }
    heap = heap_generator (p, m, A68_REF_SIZE);
    * DEREF (A68_REF, &heap) = nrow;
    return (heap);
  }
  return (nil_ref);
}

/*!
\brief store into a row, fi. trimmed destinations 
\param p position in tree
\param m mode of object
\param dst REF destination
\param old REF old object
\return dst
**/

A68_REF genie_store (NODE_T * p, MOID_T *m, A68_REF *dst, A68_REF *old)
{
/*
This complex routine is needed as arrays are not always contiguous.
The routine takes a REF to the value and returns a REF to the clone.
*/
  if (IF_ROW (m)) {
/* REF [FLEX] [] */
    A68_ARRAY *old_arr, *new_arr;
    A68_TUPLE *old_tup, *new_tup, *old_p, *new_p;
    MOID_T *em = SUB (IS (m, FLEX_SYMBOL) ? SUB (m) : m);
    int k, span;
    BOOL_T done = A68_FALSE;
    GET_DESCRIPTOR (old_arr, old_tup, DEREF (A68_REF, old));
    GET_DESCRIPTOR (new_arr, new_tup, DEREF (A68_REF, dst));
/* 
Get size and check bounds.
This is just song and dance to comply with the RR.
*/
    for (span = 1, k = 0; k < DIM (old_arr); k++) {
      old_p = &old_tup[k];
      new_p = &new_tup[k];
      if ((UPB (new_p) != UPB (old_p) || LWB (new_p) != LWB (old_p))) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      span *= ROW_SIZE (new_p);
    }
    if (span > 0) {
      initialise_internal_index (old_tup, DIM (old_arr));
      initialise_internal_index (new_tup, DIM (new_arr));
      while (!done) {
        A68_REF new_old = ARRAY (old_arr);
        A68_REF new_dst = ARRAY (new_arr);
        ADDR_T old_index = calculate_internal_index (old_tup, DIM (old_arr));
        ADDR_T new_index = calculate_internal_index (new_tup, DIM (new_arr));
        OFFSET (&new_old) += ROW_ELEMENT (old_arr, old_index);
        OFFSET (&new_dst) += ROW_ELEMENT (new_arr, new_index);
        MOVE (ADDRESS (&new_dst), ADDRESS (&new_old), MOID_SIZE (em));
        done = increment_internal_index (old_tup, DIM (old_arr))
             | increment_internal_index (new_tup, DIM (new_arr));
      }
    }
    return (*dst);
  }
  return (nil_ref);
}

/*!
\brief assignment of complex objects in the stack
\param p position in tree
\param dst REF to destination
\param tmp REF to template for bounds checks
\param srcm mode of source
**/

static void genie_clone_stack (NODE_T *p, MOID_T *srcm, A68_REF *dst, A68_REF *tmp)
{
/* STRUCT, UNION, [FLEX] [] or SOUND */
  A68_REF stack, *src, a68_clone;
  STATUS (&stack) = (STATUS_MASK) (INIT_MASK | IN_STACK_MASK);
  OFFSET (&stack) = stack_pointer;
  REF_HANDLE (&stack) = &nil_handle;
  src = DEREF (A68_REF, &stack);
  if (IS (srcm, ROW_SYMBOL) && !IS_NIL (*tmp)) {
    if (STATUS (src) & SKIP_ROW_MASK) {
      return;
    }
    a68_clone = genie_clone (p, srcm, tmp, &stack);
    (void) genie_store (p, srcm, dst, &a68_clone);
  } else {
    a68_clone = genie_clone (p, srcm, tmp, &stack);
    MOVE (ADDRESS (dst), ADDRESS (&a68_clone), MOID_SIZE (srcm));
  }
}

/*!
\brief push description for diagonal of square matrix
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_diagonal_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROP_T self;
  A68_ROW row, new_row;
  int k = 0;
  BOOL_T name = (BOOL_T) (IS (MOID (p), REF_SYMBOL));
  A68_ARRAY *arr, new_arr;
  A68_TUPLE *tup1, *tup2, new_tup;
  MOID_T *m;
  if (IS (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, * DEREF (A68_REF, &z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  if (ROW_SIZE (tup1) != ROW_SIZE (tup2)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_NO_SQUARE_MATRIX, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (ABS (k) >= ROW_SIZE (tup1)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 1;
  MOID (&new_arr) = m;
  ELEM_SIZE (&new_arr) = ELEM_SIZE (arr);
  SLICE_OFFSET (&new_arr) = SLICE_OFFSET (arr);
  FIELD_OFFSET (&new_arr) = FIELD_OFFSET (arr);
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&new_tup) = 1;
  UPB (&new_tup) = ROW_SIZE (tup1) - ABS (k);
  SHIFT (&new_tup) = SHIFT (tup1) + SHIFT (tup2) - k * SPAN (tup2);
  if (k < 0) {
    SHIFT (&new_tup) -= (-k) * (SPAN (tup1) + SPAN (tup2));
  }
  SPAN (&new_tup) = SPAN (tup1) + SPAN (tup2);
  K (&new_tup) = 0;
  PUT_DESCRIPTOR (new_arr, new_tup, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), A68_REF_SIZE);
    * DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  UNIT (&self) = genie_diagonal_function;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push description for transpose of matrix
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_transpose_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROP_T self;
  A68_ROW row, new_row;
  BOOL_T name = (BOOL_T) (IS (MOID (p), REF_SYMBOL));
  A68_ARRAY *arr, new_arr;
  A68_TUPLE *tup1, *tup2, new_tup1, new_tup2;
  MOID_T *m;
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, * DEREF (A68_REF, &z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + 2 * ALIGNED_SIZE_OF (A68_TUPLE));
  new_arr = *arr;
  new_tup1 = *tup2;
  new_tup2 = *tup1;
  PUT_DESCRIPTOR2 (new_arr, new_tup1, new_tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), A68_REF_SIZE);
    * DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  UNIT (&self) = genie_transpose_function;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push description for row vector
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_row_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROP_T self;
  A68_ROW row, new_row;
  int k = 1;
  BOOL_T name = (BOOL_T) (IS (MOID (p), REF_SYMBOL));
  A68_ARRAY *arr, new_arr;
  A68_TUPLE tup1, tup2, *tup;
  MOID_T *m;
  if (IS (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, * DEREF (A68_REF, &z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  if (DIM (arr) != 1) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_NO_VECTOR, m, PRIMARY);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 2;
  MOID (&new_arr) = m;
  ELEM_SIZE (&new_arr) = ELEM_SIZE (arr);
  SLICE_OFFSET (&new_arr) = SLICE_OFFSET (arr);
  FIELD_OFFSET (&new_arr) = FIELD_OFFSET (arr);
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&tup1) = k;
  UPB (&tup1) = k;
  SPAN (&tup1) = 1;
  SHIFT (&tup1) = k * SPAN (&tup1);
  K (&tup1) = 0;
  LWB (&tup2) = 1;
  UPB (&tup2) = ROW_SIZE (tup);
  SPAN (&tup2) = SPAN (tup);
  SHIFT (&tup2) = SPAN (tup);
  K (&tup2) = 0;
  PUT_DESCRIPTOR2 (new_arr, tup1, tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), A68_REF_SIZE);
    * DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  UNIT (&self) = genie_row_function;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief push description for column vector
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_column_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROP_T self;
  A68_ROW row, new_row;
  int k = 1;
  BOOL_T name = (BOOL_T) (IS (MOID (p), REF_SYMBOL));
  A68_ARRAY *arr, new_arr;
  A68_TUPLE tup1, tup2, *tup;
  MOID_T *m;
  if (IS (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, * DEREF (A68_REF, &z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 2;
  MOID (&new_arr) = m;
  ELEM_SIZE (&new_arr) = ELEM_SIZE (arr);
  SLICE_OFFSET (&new_arr) = SLICE_OFFSET (arr);
  FIELD_OFFSET (&new_arr) = FIELD_OFFSET (arr);
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&tup1) = 1;
  UPB (&tup1) = ROW_SIZE (tup);
  SPAN (&tup1) = SPAN (tup);
  SHIFT (&tup1) = SPAN (tup);
  K (&tup1) = 0;
  LWB (&tup2) = k;
  UPB (&tup2) = k;
  SPAN (&tup2) = 1;
  SHIFT (&tup2) = k * SPAN (&tup2);
  K (&tup2) = 0;
  PUT_DESCRIPTOR2 (new_arr, tup1, tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), A68_REF_SIZE);
    * DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  UNIT (&self) = genie_column_function;
  SOURCE (&self) = p;
  return (self);
}

/*!
\brief strcmp for qsort
\param a string
\param b string
\return a - b
**/

int qstrcmp (const void *a, const void *b)
{
  return (strcmp (*(char *const *) a, *(char *const *) b));
}

/*!
\brief sort row of string
\param p position in tree
**/

void genie_sort_row_string (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  ADDR_T pop_sp;
  int size;
  POP_REF (p, &z);
  pop_sp = stack_pointer;
  CHECK_REF (p, z, MODE (ROW_STRING));
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  if (size > 0) {
    A68_REF row, *base_ref;
    A68_ARRAY arrn;
    A68_TUPLE tupn;
    int j, k;
    BYTE_T *base = ADDRESS (&ARRAY (arr));
    char **ptrs = (char **) malloc ((size_t) (size * (int) sizeof (char *)));
    if (ptrs == NO_VAR) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* Copy C-strings into the stack and sort */
    for (j = 0, k = LWB (tup); k <= UPB (tup); j++, k++) {
      int addr = INDEX_1_DIM (arr, tup, k);
      A68_REF ref = *(A68_REF *) & (base[addr]);
      int len;
      CHECK_REF (p, ref, MODE (STRING));
      len = A68_ALIGN (a68_string_size (p, ref) + 1);
      if (stack_pointer + len > expr_stack_limit) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      ptrs[j] = (char *) STACK_TOP;
      ASSERT (a_to_c_string (p, (char *) STACK_TOP, ref) != NO_TEXT);
      INCREMENT_STACK_POINTER (p, len);
    }
    qsort (ptrs, (size_t) size, sizeof (char *), qstrcmp);
/* Construct an array of sorted strings */
    z = heap_generator (p, MODE (ROW_STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    row = heap_generator (p, MODE (ROW_STRING), size * MOID_SIZE (MODE (STRING)));
    DIM (&arrn) = 1;
    MOID (&arrn) = MODE (STRING);
    ELEM_SIZE (&arrn) = MOID_SIZE (MODE (STRING));
    SLICE_OFFSET (&arrn) = 0;
    FIELD_OFFSET (&arrn) = 0;
    ARRAY (&arrn) = row;
    LWB (&tupn) = 1;
    UPB (&tupn) = size;
    SHIFT (&tupn) = LWB (&tupn);
    SPAN (&tupn) = 1;
    K (&tupn) = 0;
    PUT_DESCRIPTOR (arrn, tupn, &z);
    base_ref = DEREF (A68_REF, &row);
    for (k = 0; k < size; k++) {
      base_ref[k] = c_to_a_string (p, ptrs[k], DEFAULT_WIDTH);
    }
    free (ptrs);
    stack_pointer = pop_sp;
    PUSH_REF (p, z);
  } else {
/* This is how we sort an empty row of strings .. */
    stack_pointer = pop_sp;
    PUSH_REF (p, empty_row (p, MODE (ROW_STRING)));
  }
}

/*
Generator and garbage collector routines.

The generator allocates space in stack or heap and initialises dynamically sized objects.

A mark-and-gc garbage collector defragments the heap. When called, it walks
the stack frames and marks the heap space that is still active. This marking
process is called "colouring" here since we "pour paint" into the heap.
The active blocks are then joined, the non-active blocks are forgotten.

When colouring the heap, "cookies" are placed in objects as to find circular
references.

Algol68G introduces several anonymous tags in the symbol tables that save
temporary REF or ROW results, so that they do not get prematurely swept.

The genie is not smart enough to handle every heap clog, e.g. when copying
STOWED objects. This seems not very elegant, but garbage collectors in general
cannot solve all core management problems. To avoid many of the "unforeseen"
heap clogs, we try to keep heap occupation low by garbage collecting 
occasionally, before it fills up completely. If this automatic mechanism does
not help, one can always invoke the garbage collector by calling "gc heap"
from Algol 68 source text.

Mark-and-gc is simple but since it walks recursive structures, it could
exhaust the C-stack (segment violation). A rough check is in place.
*/

void colour_object (BYTE_T *, MOID_T *);
void gc_heap (NODE_T *, ADDR_T);
int garbage_collects, garbage_bytes_freed;
int free_handle_count, max_handle_count;
A68_HANDLE *free_handles, *busy_handles;
double garbage_seconds;

#define DEF_NODE(p) (NEXT_NEXT (NODE (TAX (p))))
#define MAX(u, v) ((u) = ((u) > (v) ? (u) : (v)))

void genie_generator_stowed (NODE_T *, BYTE_T *, NODE_T **, ADDR_T *);

/* Total freed is kept in a LONG INT */

MP_T garbage_total_freed[LONG_MP_DIGITS + 2];
static MP_T garbage_freed[LONG_MP_DIGITS + 2];

/*!
\brief PROC VOID gc heap
\param p position in tree
**/

void genie_gc_heap (NODE_T * p)
{
  gc_heap (p, frame_pointer);
}

/*!
\brief PROC VOID preemptive gc heap
\param p position in tree
**/

void genie_preemptive_gc_heap (NODE_T * p)
{
  PREEMPTIVE_GC;
}

/*!
\brief INT blocks
\param p position in tree
**/

void genie_block (NODE_T * p)
{
  PUSH_PRIMITIVE (p, 0, A68_INT);
}

/*!
\brief INT collections
\param p position in tree
**/

void genie_garbage_collections (NODE_T * p)
{
  PUSH_PRIMITIVE (p, garbage_collects, A68_INT);
}

/*!
\brief LONG INT garbage
\param p position in tree
**/

void genie_garbage_freed (NODE_T * p)
{
  PUSH (p, garbage_total_freed, moid_size (MODE (LONG_INT)));
}

/*!
\brief REAL collect seconds
\param p position in tree
**/

void genie_garbage_seconds (NODE_T * p)
{
/* Note that this timing is a rough cut */
  PUSH_PRIMITIVE (p, garbage_seconds, A68_REAL);
}

/*!
\brief size available for an object in the heap
\return size available in bytes
**/

int heap_available (void)
{
  return (heap_size - heap_pointer);
}

/*!
\brief initialise heap management
\param p position in tree
**/

void genie_init_heap (NODE_T * p)
{
  A68_HANDLE *z;
  int k, max;
  (void) p;
  if (heap_segment == NO_BYTE) {
    diagnostic_node (A68_RUNTIME_ERROR, TOP_NODE (&program), ERROR_OUT_OF_CORE);
    exit_genie (TOP_NODE (&program), A68_RUNTIME_ERROR);
  }
  if (handle_segment == NO_BYTE) {
    diagnostic_node (A68_RUNTIME_ERROR, TOP_NODE (&program), ERROR_OUT_OF_CORE);
    exit_genie (TOP_NODE (&program), A68_RUNTIME_ERROR);
  }
  garbage_seconds = 0;
  SET_MP_ZERO (garbage_total_freed, LONG_MP_DIGITS);
  garbage_collects = 0;
  ABEND (fixed_heap_pointer >= (heap_size - MIN_MEM_SIZE), ERROR_OUT_OF_CORE, NO_TEXT);
  heap_pointer = fixed_heap_pointer;
  heap_is_fluid = A68_FALSE;
/* Assign handle space */
  z = (A68_HANDLE *) handle_segment;
  free_handles = z;
  busy_handles = NO_HANDLE;
  max = (int) handle_pool_size / (int) sizeof (A68_HANDLE);
  free_handle_count = max;
  max_handle_count = max;
  for (k = 0; k < max; k++) {
    STATUS (&(z[k])) = NULL_MASK;
    POINTER (&(z[k])) = NO_BYTE;
    SIZE (&(z[k])) = 0;
    NEXT (&z[k]) = (k == max - 1 ? NO_HANDLE : &z[k + 1]);
    PREVIOUS (&z[k]) = (k == 0 ? NO_HANDLE : &z[k - 1]);
  }
}

/*!
\brief whether mode must be coloured
\param m moid to colour
\return same
**/

static BOOL_T moid_needs_colouring (MOID_T * m)
{
  if (IS (m, REF_SYMBOL)) {
    return (A68_TRUE);
  } else if (IS (m, PROC_SYMBOL)) {
    return (A68_TRUE);
  } else if (IS (m, FLEX_SYMBOL) || IS (m, ROW_SYMBOL)) {
    return (A68_TRUE);
  } else if (IS (m, STRUCT_SYMBOL) || IS (m, UNION_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      if (moid_needs_colouring (MOID (p))) {
        return (A68_TRUE);
      }
    }
    return (A68_FALSE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief colour all elements of a row
\param z fat pointer to descriptor
\param m mode of row
**/

static void colour_row_elements (A68_REF * z, MOID_T * m)
{
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  GET_DESCRIPTOR (arr, tup, z);
  if (get_row_size (tup, DIM (arr)) == 0) {
/* Empty rows have a ghost elements */
    BYTE_T *elem = ADDRESS (&ARRAY (arr));
    colour_object (&elem[0], SUB (m));
  } else {
/* The multi-dimensional garbage collector */
    BYTE_T *elem = ADDRESS (&ARRAY (arr));
    BOOL_T done = A68_FALSE;
    initialise_internal_index (tup, DIM (arr));
    while (!done) {
      ADDR_T iindex = calculate_internal_index (tup, DIM (arr));
      ADDR_T addr = ROW_ELEMENT (arr, iindex);
      colour_object (&elem[addr], SUB (m));
      done = increment_internal_index (tup, DIM (arr));
    }
  }
}

/*!
\brief colour an (active) object
\param item pointer to item to colour
\param m mode of item
**/

void colour_object (BYTE_T * item, MOID_T * m)
{
  if (item == NO_BYTE || m == NO_MOID) {
    return;
  }
  if (!moid_needs_colouring (m)) {
    return;
  }
/* Deeply recursive objects might exhaust the stack */
  LOW_STACK_ALERT (NO_NODE);
  if (IS (m, REF_SYMBOL)) {
/* REF AMODE colour pointer and object to which it refers */
    A68_REF *z = (A68_REF *) item;
    if (INITIALISED (z) && IS_IN_HEAP (z)) {
      if (STATUS_TEST (REF_HANDLE (z), COOKIE_MASK)) {
        return;
      }
      STATUS_SET (REF_HANDLE (z), (COOKIE_MASK | COLOUR_MASK));
      if (!IS_NIL (*z)) {
        colour_object (ADDRESS (z), SUB (m));
      }
      STATUS_CLEAR (REF_HANDLE (z), COOKIE_MASK);
    }
  } else if (IS (m, FLEX_SYMBOL) || IS (m, ROW_SYMBOL) || m == MODE (STRING)) {
/* Claim the descriptor and the row itself */
    A68_REF *z = (A68_REF *) item;
    if (INITIALISED (z) && IS_IN_HEAP (z)) {
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      if (STATUS_TEST (REF_HANDLE (z), COOKIE_MASK)) {
        return;
      }
/* An array is ALWAYS in the heap */
      STATUS_SET (REF_HANDLE (z), (COOKIE_MASK | COLOUR_MASK));
      GET_DESCRIPTOR (arr, tup, z);
      if (REF_HANDLE (&(ARRAY (arr))) != NO_HANDLE) {
/* Assume its initialisation */
        MOID_T *n = DEFLEX (m);
        STATUS_SET (REF_HANDLE (&(ARRAY (arr))), COLOUR_MASK);
        if (moid_needs_colouring (SUB (n))) {
          colour_row_elements (z, n);
        }
      }
/*      STATUS_CLEAR (REF_HANDLE (z), COOKIE_MASK); */
    }
  } else if (IS (m, STRUCT_SYMBOL)) {
/* STRUCTures - colour fields */
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      colour_object (&item[OFFSET (p)], MOID (p));
    }
  } else if (IS (m, UNION_SYMBOL)) {
/* UNIONs - a united object may contain a value that needs colouring */
    A68_UNION *z = (A68_UNION *) item;
    if (INITIALISED (z)) {
      MOID_T *united_moid = (MOID_T *) VALUE (z);
      colour_object (&item[A68_UNION_SIZE], united_moid);
    }
  } else if (IS (m, PROC_SYMBOL)) {
/* PROCs - save a locale and the objects it points to */
    A68_PROCEDURE *z = (A68_PROCEDURE *) item;
    if (INITIALISED (z) && LOCALE (z) != NO_HANDLE && !(STATUS_TEST (LOCALE (z), COOKIE_MASK))) {
      BYTE_T *u = POINTER (LOCALE (z));
      PACK_T *s = PACK (MOID (z));
      STATUS_SET (LOCALE (z), (COOKIE_MASK | COLOUR_MASK));
      for (; s != NO_PACK; FORWARD (s)) {
        if (VALUE ((A68_BOOL *) & u[0]) == A68_TRUE) {
          colour_object (&u[ALIGNED_SIZE_OF (A68_BOOL)], MOID (s));
        }
        u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      }
      STATUS_CLEAR (LOCALE (z), COOKIE_MASK);
    }
  } else if (m == MODE (SOUND)) {
/* Claim the data of a SOUND object, that is in the heap */
    A68_SOUND *w = (A68_SOUND *) item;
    if (INITIALISED (w)) {
      STATUS_SET (REF_HANDLE (&(DATA (w))), (COOKIE_MASK | COLOUR_MASK));
    }
  }
}

/*!
\brief colour active objects in the heap
\param fp running frame pointer
**/

static void colour_heap (ADDR_T fp)
{
  while (fp != 0) {
    NODE_T *p = FRAME_TREE (fp);
    TABLE_T *q = TABLE (p);
    if (q != NO_TABLE) {
      TAG_T *i;
      for (i = IDENTIFIERS (q); i != NO_TAG; FORWARD (i)) {
        colour_object (FRAME_LOCAL (fp, OFFSET (i)), MOID (i));
      }
      for (i = ANONYMOUS (q); i != NO_TAG; FORWARD (i)) {
        if (PRIO (i) == GENERATOR) {
          colour_object (FRAME_LOCAL (fp, OFFSET (i)), MOID (i));
        }
      }
    }
    fp = FRAME_DYNAMIC_LINK (fp);
  }
}

/*!
\brief join all active blocks in the heap
**/

static void defragment_heap (void)
{
  A68_HANDLE *z;
/* Free handles */
  z = busy_handles;
  while (z != NO_HANDLE) {
    if (!(STATUS_TEST (z, COLOUR_MASK)) && !(STATUS_TEST (z, BLOCK_GC_MASK))) {
      A68_HANDLE *y = NEXT (z);
      if (PREVIOUS (z) == NO_HANDLE) {
        busy_handles = NEXT (z);
      } else {
        NEXT (PREVIOUS (z)) = NEXT (z);
      }
      if (NEXT (z) != NO_HANDLE) {
        PREVIOUS (NEXT (z)) = PREVIOUS (z);
      }
      NEXT (z) = free_handles;
      PREVIOUS (z) = NO_HANDLE;
      if (NEXT (z) != NO_HANDLE) {
        PREVIOUS (NEXT (z)) = z;
      }
      free_handles = z;
      STATUS_CLEAR (z, ALLOCATED_MASK);
      garbage_bytes_freed += SIZE (z);
      free_handle_count++;
      z = y;
    } else {
      FORWARD (z);
    }
  }
/* There can be no uncoloured allocated handle */
  for (z = busy_handles; z != NO_HANDLE; FORWARD (z)) {
    ABEND (!(STATUS_TEST (z, COLOUR_MASK)) && !(STATUS_TEST (z, BLOCK_GC_MASK)), "bad GC consistency", NO_TEXT);
  }
/* Defragment the heap */
  heap_pointer = fixed_heap_pointer;
  for (z = busy_handles; z != NO_HANDLE && NEXT (z) != NO_HANDLE; FORWARD (z)) {
    ;
  }
  for (; z != NO_HANDLE; BACKWARD (z)) {
    BYTE_T *dst = HEAP_ADDRESS (heap_pointer);
    if (dst != POINTER (z)) {
      MOVE (dst, POINTER (z), (unsigned) SIZE (z));
    }
    STATUS_CLEAR (z, (COLOUR_MASK | COOKIE_MASK));
    POINTER (z) = dst;
    heap_pointer += (SIZE (z));
    ABEND (heap_pointer % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NO_TEXT);
  }
}

/*!
\brief clean up garbage and defragment the heap
\param p position in tree
\param fp running frame pointer
**/

void gc_heap (NODE_T * p, ADDR_T fp)
{
/* Must start with fp = current frame_pointer */
  A68_HANDLE *z;
  double t0, t1;
#if defined HAVE_PARALLEL_CLAUSE
  if (pthread_equal (FRAME_THREAD_ID (frame_pointer), main_thread_id) ==0) {
    return;
  }
#endif
  t0 = seconds ();
/* Unfree handles are subject to inspection */
  for (z = busy_handles; z != NO_HANDLE; FORWARD (z)) {
    STATUS_CLEAR (z, (COLOUR_MASK | COOKIE_MASK));
  }
/* Pour paint into the heap to reveal active objects */
  colour_heap (fp);
/* Start freeing and compacting */
  garbage_bytes_freed = 0;
  defragment_heap ();
/* Stats and logging */
  (void) int_to_mp (p, garbage_freed, (int) garbage_bytes_freed, LONG_MP_DIGITS);
  (void) add_mp (p, garbage_total_freed, garbage_total_freed, garbage_freed, LONG_MP_DIGITS);
  garbage_collects++;
  t1 = seconds ();
/* C optimiser can make last digit differ, so next condition is 
   needed to determine a positive time difference */
  if ((t1 - t0) > ((double) clock_res / 2.0)) {
    garbage_seconds += (t1 - t0);
  } else {
    garbage_seconds += ((double) clock_res / 2.0);
  }
/* Call the event handler */
  genie_call_event_routine (p, MODE (PROC_VOID), &on_gc_event, stack_pointer, frame_pointer);
}

/*!
\brief yield a handle that will point to a block in the heap
\param p position in tree
\param a68m mode of object
\return handle that points to object
**/

static A68_HANDLE *give_handle (NODE_T * p, MOID_T * a68m)
{
  if (free_handles != NO_HANDLE) {
    A68_HANDLE *x = free_handles;
    free_handles = NEXT (x);
    if (free_handles != NO_HANDLE) {
      PREVIOUS (free_handles) = NO_HANDLE;
    }
    STATUS (x) = ALLOCATED_MASK;
    POINTER (x) = NO_BYTE;
    SIZE (x) = 0;
    MOID (x) = a68m;
    NEXT (x) = busy_handles;
    PREVIOUS (x) = NO_HANDLE;
    if (NEXT (x) != NO_HANDLE) {
      PREVIOUS (NEXT (x)) = x;
    }
    busy_handles = x;
    free_handle_count--;
    return (x);
  } else {
/* Do not auto-GC! */
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  return (NO_HANDLE);
}

/*!
\brief give a block of heap for an object of indicated mode
\param p position in tree
\param mode mode of object
\param size size in bytes to allocate
\return fat pointer to object in the heap
**/

A68_REF heap_generator (NODE_T * p, MOID_T * mode, int size)
{
/* Align */
  ABEND (size < 0, ERROR_INVALID_SIZE, NO_TEXT);
  size = A68_ALIGN (size);
/* Now give it */
  if (heap_available () >= size) {
    A68_HANDLE *x;
    A68_REF z;
    STATUS (&z) = (STATUS_MASK) (INIT_MASK | IN_HEAP_MASK);
    OFFSET (&z) = 0;
    x = give_handle (p, mode);
    SIZE (x) = size;
    POINTER (x) = HEAP_ADDRESS (heap_pointer);
    FILL (POINTER (x), 0, size);
    REF_SCOPE (&z) = PRIMAL_SCOPE;
    REF_HANDLE (&z) = x;
    ABEND (((long) ADDRESS (&z)) % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NO_TEXT);
    heap_pointer += size;
    return (z);
  } else {
/* Do not auto-GC! */
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
    return (nil_ref);
  }
}

/*
Following implements the generator.

For dynamically sized objects, first bounds are evaluated (right first, then down).
The object is generated keeping track of the bound-count.

	...
	[#1]
	STRUCT
	(
	[#2]
	STRUCT
	(
	[#3] A a, b, ...
	)
	,			Advance bound-count here, max is #3
	[#4] B a, b, ...
	)
	,			Advance bound-count here, max is #4
	[#5] C a, b, ...
	...

Bound-count is maximised when generator_stowed is entered recursively. 
Bound-count is advanced when completing a STRUCTURED_FIELD.
*/

/*!
\brief whether a moid needs work in allocation
\param m moid under test
**/

static BOOL_T mode_needs_allocation (MOID_T * m)
{
  if (IS (m, UNION_SYMBOL)) {
    return (A68_FALSE);
  } else {
    return (HAS_ROWS (m));
  }
}

/*!
\brief prepare bounds
\param p position in tree
**/

static void genie_compute_bounds (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, BOUNDS_LIST)) {
      genie_compute_bounds (SUB (p));
    } else if (IS (p, BOUND)) {
      genie_compute_bounds (SUB (p));
    } else if (IS (p, UNIT)) {
      if (NEXT (p) != NO_NODE && (is_one_of (NEXT (p), COLON_SYMBOL, DOTDOT_SYMBOL, STOP))) {
        EXECUTE_UNIT (p);
        p = NEXT_NEXT (p);
      } else {
/* Default lower bound */
        PUSH_PRIMITIVE (p, 1, A68_INT);
      }
      EXECUTE_UNIT (p);
    }
  }
}

/*!
\brief prepare bounds for a row
\param p position in tree
**/

void genie_generator_bounds (NODE_T * p)
{
  LOW_STACK_ALERT (p);
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, BOUNDS)) {
      genie_compute_bounds (SUB (p));
    } else if (IS (p, INDICANT) && IS_LITERALLY (p, "STRING")) {
      return;
    } else if (IS (p, INDICANT)) {
      if (TAX (p) != NO_TAG && HAS_ROWS (MOID (TAX (p)))) {
/* Continue from definition at MODE A = ... */
        genie_generator_bounds (DEF_NODE (p));
      }
    } else if (IS (p, DECLARER) && !mode_needs_allocation (MOID (p))) {
      return;
    } else {
      genie_generator_bounds (SUB (p));
    }
  }
}

/*!
\brief allocate a structure
\param p decl in the syntax tree
**/

void genie_generator_field (NODE_T * p, BYTE_T ** faddr, NODE_T ** decl, ADDR_T * cur_sp, ADDR_T *top_sp)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, STRUCTURED_FIELD)) {
      genie_generator_field (SUB (p), faddr, decl, cur_sp, top_sp);
    }
    if (IS (p, DECLARER)) {
      (*decl) = SUB (p);
      FORWARD (p);
    }
    if (IS (p, FIELD_IDENTIFIER)) {
      MOID_T *fmoid = MOID (*decl);
      if (HAS_ROWS (fmoid) && ISNT (fmoid, UNION_SYMBOL)) {
        ADDR_T pop_sp = *cur_sp;
        genie_generator_stowed (*decl, *faddr, NO_VAR, cur_sp);
        *top_sp = *cur_sp;
        *cur_sp = pop_sp;
      }
      (*faddr) += MOID_SIZE (fmoid);
    }
  }
}

/*!
\brief allocate a structure
\param p decl in the syntax tree
**/

void genie_generator_struct (NODE_T * p, BYTE_T ** faddr, ADDR_T * cur_sp)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, STRUCTURED_FIELD_LIST)) {
      genie_generator_struct (SUB (p), faddr, cur_sp);
    } else if (IS (p, STRUCTURED_FIELD)) {
      NODE_T *decl = NO_NODE;
      ADDR_T top_sp = *cur_sp;
      genie_generator_field (SUB (p), faddr, &decl, cur_sp, &top_sp);
      *cur_sp = top_sp;
    }
  }
}

/*!
\brief allocate a stowed object
\param p decl in the syntax tree
**/

void genie_generator_stowed (NODE_T * p, BYTE_T * addr, NODE_T ** decl, ADDR_T * cur_sp)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, INDICANT) && IS_LITERALLY (p, "STRING")) {
/* The standard prelude definition is hard coded here */
    *((A68_REF *) addr) = empty_string (p);
    return;
  } else if (IS (p, INDICANT) && TAX (p) != NO_TAG) {
/* Continue from definition at MODE A = . */
    genie_generator_stowed (DEF_NODE (p), addr, decl, cur_sp);
    return;
  } else if (IS (p, DECLARER) && mode_needs_allocation (MOID (p))) {
    genie_generator_stowed (SUB (p), addr, decl, cur_sp);
    return;
  } else if (IS (p, STRUCT_SYMBOL)) {
    BYTE_T *faddr = addr;
    genie_generator_struct (SUB_NEXT (p), &faddr, cur_sp);
    return;
  }
/* Row c.s. */
  if (IS (p, FLEX_SYMBOL)) {
    FORWARD (p);
  }
  if (IS (p, BOUNDS)) {
    A68_REF desc;
    MOID_T *rmod = MOID (p), *smod = MOID (NEXT (p));
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    BYTE_T *bounds = STACK_ADDRESS (*cur_sp);
    int k, dim = DIM (DEFLEX (rmod));
    int esiz = MOID_SIZE (smod), rsiz = 1;
    BOOL_T alloc_sub, alloc_str;
    NODE_T *in = SUB_NEXT (p); 
    if (IS (in, INDICANT) && IS_LITERALLY (in, "STRING")) {
      alloc_str = A68_TRUE;
      alloc_sub = A68_FALSE;
    } else {
      alloc_sub = mode_needs_allocation (smod);
      alloc_str = A68_FALSE;
    }
    desc = heap_generator (p, rmod, dim * ALIGNED_SIZE_OF (A68_TUPLE) + ALIGNED_SIZE_OF (A68_ARRAY));
    GET_DESCRIPTOR (arr, tup, &desc);
    for (k = 0; k < dim; k++) {
      CHECK_INIT (p, INITIALISED ((A68_INT *) bounds), MODE (INT));
      LWB (&tup[k]) = VALUE ((A68_INT *) bounds);
      bounds += ALIGNED_SIZE_OF (A68_INT);
      CHECK_INIT (p, INITIALISED ((A68_INT *) bounds), MODE (INT));
      UPB (&tup[k]) = VALUE ((A68_INT *) bounds);
      bounds += ALIGNED_SIZE_OF (A68_INT);
      SPAN (&tup[k]) = rsiz;
      SHIFT (&tup[k]) = LWB (&tup[k]) * SPAN (&tup[k]);
      rsiz *= ROW_SIZE (&tup[k]);
    }
    DIM (arr) = dim;
    MOID (arr) = smod;
    ELEM_SIZE (arr) = esiz;
    SLICE_OFFSET (arr) = 0;
    FIELD_OFFSET (arr) = 0;
    (*cur_sp) += (dim * 2 * ALIGNED_SIZE_OF (A68_INT));
/* 
Generate a new row. Note that STRING is handled explicitly since
it has implicit bounds 
*/
    if (rsiz == 0) {
/* Generate a ghost element */
      ADDR_T top_sp = *cur_sp;
      BYTE_T *elem;
      ARRAY (arr) = heap_generator (p, rmod, esiz);
      elem = ADDRESS (&(ARRAY (arr)));
      if (alloc_sub) {
        genie_generator_stowed (NEXT (p), &(elem[0]), NO_VAR, cur_sp);
        top_sp = *cur_sp;
      } else if (alloc_str) {
        * (A68_REF *) elem = empty_string (p);
      }
      (*cur_sp) = top_sp;
    } else {
      ADDR_T pop_sp = *cur_sp, top_sp = *cur_sp;
      BYTE_T *elem;
      ARRAY (arr) = heap_generator (p, rmod, rsiz * esiz);
      elem = ADDRESS (&(ARRAY (arr)));
      for (k = 0; k < rsiz; k++) {
        if (alloc_sub) {
          (*cur_sp) = pop_sp;
          genie_generator_stowed (NEXT (p), &(elem[k * esiz]), NO_VAR, cur_sp);
          top_sp = *cur_sp;
        } else if (alloc_str) {
          * (A68_REF *) (&(elem[k * esiz])) = empty_string (p);
        }
      }
      (*cur_sp) = top_sp;
    }
    *(A68_REF *) addr = desc;
    return;
  }
}

/*!
\brief generate space and push a REF
\param p declarer in the syntax tree
\param ref_mode REF mode to be generated
\param tag associated internal LOC for this generator
\param leap where to generate space
\param sp stack pointer to locate bounds
**/

void genie_generator_internal (NODE_T * p, MOID_T * ref_mode, TAG_T * tag, LEAP_T leap, ADDR_T sp)
{
  MOID_T *mode = SUB (ref_mode);
  A68_REF name = nil_ref;
/* 
Set up a REF MODE object, either in the stack or in the heap.
Note that A68G will not extend stack frames.
Thus only 'static' LOC generators are in the stack,
and 'dynamic' LOC generators go into the heap.
These local REFs in the heap get local scope however, and A68Gs
approach differs from the CDC ALGOL 68 approach that put all
generators in the heap.
*/
  if (leap == LOC_SYMBOL) {
    STATUS (&name) = (STATUS_MASK) (INIT_MASK | IN_FRAME_MASK);
    REF_HANDLE (&name) = &nil_handle;
    OFFSET (&name) = frame_pointer + FRAME_INFO_SIZE + OFFSET (tag);
    REF_SCOPE (&name) = frame_pointer;
  } else if (leap == -LOC_SYMBOL && NON_LOCAL (p) != NO_TABLE) {
    ADDR_T lev;
    name = heap_generator (p, mode, MOID_SIZE (mode));
    FOLLOW_SL (lev, LEVEL (NON_LOCAL (p)));
    REF_SCOPE (&name) = lev;
  } else if (leap == -LOC_SYMBOL) {
    name = heap_generator (p, mode, MOID_SIZE (mode));
    REF_SCOPE (&name) = frame_pointer;
  } else if (leap == HEAP_SYMBOL || leap == -HEAP_SYMBOL) {
    name = heap_generator (p, mode, MOID_SIZE (mode));
    REF_SCOPE (&name) = PRIMAL_SCOPE;
  } else if (leap == NEW_SYMBOL || leap == -NEW_SYMBOL) {
    name = heap_generator (p, mode, MOID_SIZE (mode));
    REF_SCOPE (&name) = PRIMAL_SCOPE;
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
  }
  if (HAS_ROWS (mode)) {
    ADDR_T cur_sp = sp;
    genie_generator_stowed (p, ADDRESS (&name), NO_VAR, &cur_sp);
  }
  PUSH_REF (p, name);
}

/*!
\brief push a name refering to allocated space
\param p position in tree
\return a propagator for this action
**/

static PROP_T genie_generator (NODE_T * p)
{
  PROP_T self;
  ADDR_T pop_sp = stack_pointer;
  A68_REF z;
  if (NEXT_SUB (p) != NO_NODE) {
    genie_generator_bounds (NEXT_SUB (p));
  }
  genie_generator_internal (NEXT_SUB (p), MOID (p), TAX (p), -ATTRIBUTE (SUB (p)), pop_sp);
  POP_REF (p, &z);
  stack_pointer = pop_sp;
  PUSH_REF (p, z);
  UNIT (&self) = genie_generator;
  SOURCE (&self) = p;
  return (self);
}

/*
This code implements a parallel clause for Algol68G. This parallel clause has
been included for educational purposes, and this implementation just emulates
a multi-processor machine. It cannot make use of actual multiple processors.

POSIX threads are used to have separate registers and stack for each concurrent
unit. Algol68G parallel units behave as POSIX threads - they have private 
stacks. Hence an assignation to an object in another thread, does not change 
that object in that other thread. Also jumps between threads are forbidden.
*/

/*!
\brief propagator_name
\param p propagator procedure
\return function name of "p"
**/

char *propagator_name (PROP_PROC * p)
{
  if (p == genie_and_function) {
    return ("genie_and_function");
  }
  if (p == genie_assertion) {
    return ("genie_assertion");
  }
  if (p == genie_assignation) {
    return ("genie_assignation");
  }
  if (p == genie_assignation_constant) {
    return ("genie_assignation_constant");
  }
  if (p == genie_call) {
    return ("genie_call");
  }
  if (p == genie_cast) {
    return ("genie_cast");
  }
  if (p == (PROP_PROC *) genie_closed) {
    return ("genie_closed");
  }
  if (p == genie_coercion) {
    return ("genie_coercion");
  }
  if (p == genie_collateral) {
    return ("genie_collateral");
  }
  if (p == genie_column_function) {
    return ("genie_column_function");
  }
  if (p == (PROP_PROC *) genie_conditional) {
    return ("genie_conditional");
  }
  if (p == genie_constant) {
    return ("genie_constant");
  }
  if (p == genie_denotation) {
    return ("genie_denotation");
  }
  if (p == genie_deproceduring) {
    return ("genie_deproceduring");
  }
  if (p == genie_dereference_frame_identifier) {
    return ("genie_dereference_frame_identifier");
  }
  if (p == genie_dereference_selection_name_quick) {
    return ("genie_dereference_selection_name_quick");
  }
  if (p == genie_dereference_slice_name_quick) {
    return ("genie_dereference_slice_name_quick");
  }
  if (p == genie_dereferencing) {
    return ("genie_dereferencing");
  }
  if (p == genie_dereferencing_quick) {
    return ("genie_dereferencing_quick");
  }
  if (p == genie_diagonal_function) {
    return ("genie_diagonal_function");
  }
  if (p == genie_dyadic) {
    return ("genie_dyadic");
  }
  if (p == genie_dyadic_quick) {
    return ("genie_dyadic_quick");
  }
  if (p == (PROP_PROC *) genie_enclosed) {
    return ("genie_enclosed");
  }
  if (p == genie_format_text) {
    return ("genie_format_text");
  }
  if (p == genie_formula) {
    return ("genie_formula");
  }
  if (p == genie_generator) {
    return ("genie_generator");
  }
  if (p == genie_identifier) {
    return ("genie_identifier");
  }
  if (p == genie_identifier_standenv) {
    return ("genie_identifier_standenv");
  }
  if (p == genie_identifier_standenv_proc) {
    return ("genie_identifier_standenv_proc");
  }
  if (p == genie_identity_relation) {
    return ("genie_identity_relation");
  }
  if (p == (PROP_PROC *) genie_int_case) {
    return ("genie_int_case");
  }
  if (p == genie_field_selection) {
    return ("genie_field_selection");
  }
  if (p == genie_frame_identifier) {
    return ("genie_frame_identifier");
  }
  if (p == (PROP_PROC *) genie_loop) {
    return ("genie_loop");
  }
  if (p == genie_monadic) {
    return ("genie_monadic");
  }
  if (p == genie_nihil) {
    return ("genie_nihil");
  }
  if (p == genie_or_function) {
    return ("genie_or_function");
  }
#if defined HAVE_PARALLEL_CLAUSE
  if (p == genie_parallel) {
    return ("genie_parallel");
  }
#endif
  if (p == genie_routine_text) {
    return ("genie_routine_text");
  }
  if (p == genie_row_function) {
    return ("genie_row_function");
  }
  if (p == genie_rowing) {
    return ("genie_rowing");
  }
  if (p == genie_rowing_ref_row_of_row) {
    return ("genie_rowing_ref_row_of_row");
  }
  if (p == genie_rowing_ref_row_row) {
    return ("genie_rowing_ref_row_row");
  }
  if (p == genie_rowing_row_of_row) {
    return ("genie_rowing_row_of_row");
  }
  if (p == genie_rowing_row_row) {
    return ("genie_rowing_row_row");
  }
  if (p == genie_selection) {
    return ("genie_selection");
  }
  if (p == genie_selection_name_quick) {
    return ("genie_selection_name_quick");
  }
  if (p == genie_selection_value_quick) {
    return ("genie_selection_value_quick");
  }
  if (p == genie_skip) {
    return ("genie_skip");
  }
  if (p == genie_slice) {
    return ("genie_slice");
  }
  if (p == genie_slice_name_quick) {
    return ("genie_slice_name_quick");
  }
  if (p == genie_transpose_function) {
    return ("genie_transpose_function");
  }
  if (p == genie_unit) {
    return ("genie_unit");
  }
  if (p == (PROP_PROC *) genie_united_case) {
    return ("genie_united_case");
  }
  if (p == genie_uniting) {
    return ("genie_uniting");
  }
  if (p == genie_voiding) {
    return ("genie_voiding");
  }
  if (p == genie_voiding_assignation) {
    return ("genie_voiding_assignation");
  }
  if (p == genie_voiding_assignation_constant) {
    return ("genie_voiding_assignation_constant");
  }
  if (p == genie_widening) {
    return ("genie_widening");
  }
  if (p == genie_widening_int_to_real) {
    return ("genie_widening_int_to_real");
  }
  return (NO_TEXT);
}

#if defined HAVE_PARALLEL_CLAUSE

typedef struct A68_STACK_DESCRIPTOR A68_STACK_DESCRIPTOR;
typedef struct A68_THREAD_CONTEXT A68_THREAD_CONTEXT;

struct A68_STACK_DESCRIPTOR
{
  ADDR_T cur_ptr, ini_ptr;
  BYTE_T *swap, *start;
  int bytes;
};

struct A68_THREAD_CONTEXT
{
  pthread_t parent, id;
  A68_STACK_DESCRIPTOR stack, frame;
  NODE_T *unit;
  int stack_used;
  BYTE_T *thread_stack_offset;
  BOOL_T active;
};

/*
Set an upper limit for number of threads.
Don't copy POSIX_THREAD_THREADS_MAX since it may be ULONG_MAX.
*/

#define THREAD_LIMIT   256

#if ! defined _POSIX_THREAD_THREADS_MAX
#define _POSIX_THREAD_THREADS_MAX	(THREAD_LIMIT)
#endif

#if (_POSIX_THREAD_THREADS_MAX < THREAD_LIMIT)
#define THREAD_MAX     (_POSIX_THREAD_THREADS_MAX)
#else
#define THREAD_MAX     (THREAD_LIMIT)
#endif

pthread_t main_thread_id = 0;
int running_par_level = 0;
static A68_THREAD_CONTEXT context[THREAD_MAX];
static ADDR_T fp0, sp0;
static BOOL_T abend_all_threads = A68_FALSE, exit_from_threads = A68_FALSE;
static int context_index = 0;
static int par_return_code = 0;
static jmp_buf *jump_buffer;
static NODE_T *jump_label;
static pthread_mutex_t unit_sema = PTHREAD_MUTEX_INITIALIZER;
static pthread_t parent_thread_id = 0;

static void save_stacks (pthread_t);
static void restore_stacks (pthread_t);

#define SAVE_STACK(stk, st, si) {\
  A68_STACK_DESCRIPTOR *s = (stk);\
  BYTE_T *start = (st);\
  int size = (si);\
  if (size > 0) {\
    if (!((s != NULL) && (BYTES (s) > 0) && (size <= BYTES (s)))) {\
      if (SWAP (s) != NO_BYTE) {\
        free (SWAP (s));\
      }\
      SWAP (s) = (BYTE_T *) malloc ((size_t) size);\
      ABEND (SWAP (s) == NULL, ERROR_OUT_OF_CORE, NO_TEXT);\
    }\
    START (s) = start;\
    BYTES (s) = size;\
    COPY (SWAP (s), start, size);\
  } else {\
    START (s) = start;\
    BYTES (s) = 0;\
    if (SWAP (s) != NO_BYTE) {\
      free (SWAP (s));\
    }\
    SWAP (s) = NO_BYTE;\
  }}

#define RESTORE_STACK(stk) {\
  A68_STACK_DESCRIPTOR *s = (stk);\
  if (s != NULL && BYTES (s) > 0) {\
    COPY (START (s), SWAP (s), BYTES (s));\
  }}

#define GET_THREAD_INDEX(z, ptid) {\
  int _k_;\
  pthread_t _tid_ = (ptid);\
  (z) = -1;\
  for (_k_ = 0; _k_ < context_index && (z) == -1; _k_++) {\
    if (pthread_equal (_tid_, ID (&context[_k_]))) {\
      (z) = _k_;\
    }\
  }\
  ABEND ((z) == -1, "thread id not registered", NO_TEXT);\
  }

#define ERROR_THREAD_FAULT "thread fault"

#define LOCK_THREAD {\
  ABEND (pthread_mutex_lock (&unit_sema) != 0, ERROR_THREAD_FAULT, NO_TEXT);\
  }

#define UNLOCK_THREAD {\
  ABEND (pthread_mutex_unlock (&unit_sema) != 0, ERROR_THREAD_FAULT, NO_TEXT);\
  }

/*!
\brief does system stack grow up or down?
\param lwb BYTE in the stack to calibrate direction
\return 1 if stackpointer increases, -1 if stackpointer decreases, 0 in case of error
**/

static int stack_direction (BYTE_T * lwb)
{
  BYTE_T upb;
  if ((int) (&upb - lwb) > 0) {
    return (1);
  } else if ((int) (&upb - lwb) < 0) {
    return (-1);
  } else {
    return (0);
  }
}

/*!
\brief fill in tree what level of parallel clause we are in
\param p position in tree
\param n level counter
**/

void set_par_level (NODE_T * p, int n)
{
  for (; p != NO_NODE; p = NEXT (p)) {
    if (IS (p, PARALLEL_CLAUSE)) {
      PAR_LEVEL (p) = n + 1;
    } else {
      PAR_LEVEL (p) = n;
    }
    set_par_level (SUB (p), PAR_LEVEL (p));
  }
}

/*!
\brief whether we are in the main thread
\return same
**/

BOOL_T is_main_thread (void)
{
  return ((BOOL_T) (main_thread_id == pthread_self ()));
}

/*!
\brief end a thread, beit normally or not
**/

void genie_abend_thread (void)
{
  int k;
  GET_THREAD_INDEX (k, pthread_self ());
  ACTIVE (&context[k]) = A68_FALSE;
  UNLOCK_THREAD;
  pthread_exit (NULL);
}

/*!
\brief when we end execution in a parallel clause we zap all threads
**/

void genie_set_exit_from_threads (int ret)
{
  abend_all_threads = A68_TRUE;
  exit_from_threads = A68_TRUE;
  par_return_code = ret;
  genie_abend_thread ();
}

/*!
\brief when we jump out of a parallel clause we zap all threads
\param p position in tree
\param jump_stat jump buffer
\param label node where label is at
**/

void genie_abend_all_threads (NODE_T * p, jmp_buf * jump_stat, NODE_T * label)
{
  (void) p;
  abend_all_threads = A68_TRUE;
  exit_from_threads = A68_FALSE;
  jump_buffer = jump_stat;
  jump_label = label;
  if (!is_main_thread ()) {
    genie_abend_thread ();
  }
}

/*!
\brief save this thread and try to start another
\param p position in tree
**/

static void try_change_thread (NODE_T * p)
{
  if (is_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
/* Release the unit_sema so another thread can take it up .. */
    save_stacks (pthread_self ());
    UNLOCK_THREAD;
/* ... and take it up again! */
    LOCK_THREAD;
    restore_stacks (pthread_self ());
  }
}

/*!
\brief store the stacks of threads
\param t thread number
**/

static void save_stacks (pthread_t t)
{
  ADDR_T p, q, u, v;
  int k;
  GET_THREAD_INDEX (k, t);
/* Store stack pointers */
  CUR_PTR (&FRAME (&context[k])) = frame_pointer;
  CUR_PTR (&STACK (&context[k])) = stack_pointer;
/* Swap out evaluation stack */
  p = stack_pointer;
  q = INI_PTR (&STACK (&context[k]));
  SAVE_STACK (&(STACK (&context[k])), STACK_ADDRESS (q), p - q);
/* Swap out frame stack */
  p = frame_pointer;
  q = INI_PTR (&FRAME (&context[k]));
  u = p + FRAME_SIZE (p);
  v = q + FRAME_SIZE (q);
/* Consider the embedding thread */
  SAVE_STACK (&(FRAME (&context[k])), FRAME_ADDRESS (v), u - v);
}

/*!
\brief restore stacks of thread
\param t thread number
**/

static void restore_stacks (pthread_t t)
{
  if (ERROR_COUNT (&program) > 0 || abend_all_threads) {
    genie_abend_thread ();
  } else {
    int k;
    GET_THREAD_INDEX (k, t);
/* Restore stack pointers */
    get_stack_size ();
    system_stack_offset = THREAD_STACK_OFFSET (&context[k]);
    frame_pointer = CUR_PTR (&FRAME (&context[k]));
    stack_pointer = CUR_PTR (&STACK (&context[k]));
/* Restore stacks */
    RESTORE_STACK (&(STACK (&context[k])));
    RESTORE_STACK (&(FRAME (&context[k])));
  }
}

/*!
\brief check whether parallel units have terminated
\param active checks whether there are still active threads
\param parent parent thread number
**/

static void check_parallel_units (BOOL_T * active, pthread_t parent)
{
  int k;
  for (k = 0; k < context_index; k++) {
    if (parent == PARENT (&context[k])) {
      (*active) |= ACTIVE (&context[k]);
    }
  }
}

/*!
\brief execute one unit from a PAR clause
\param arg dummy argument
**/

static void *start_unit (void *arg)
{
  pthread_t t;
  int k;
  BYTE_T stack_offset;
  NODE_T *p;
  (void) arg;
  LOCK_THREAD;
  t = pthread_self ();
  GET_THREAD_INDEX (k, t);
  THREAD_STACK_OFFSET (&context[k]) = (BYTE_T *) (&stack_offset - stack_direction (&stack_offset) * STACK_USED (&context[k]));
  restore_stacks (t);
  p = (NODE_T *) (UNIT (&context[k]));
  EXECUTE_UNIT_TRACE (p);
  genie_abend_thread ();
  return ((void *) NULL);
}

/*!
\brief execute parallel units
\param p position in tree
\param parent parent thread number
**/

static void start_parallel_units (NODE_T * p, pthread_t parent)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      pthread_t new_id;
      pthread_attr_t new_at;
      size_t ss;
      BYTE_T stack_offset;
      A68_THREAD_CONTEXT *u;
/* Set up a thread for this unit */
      if (context_index >= THREAD_MAX) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OVERFLOW);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
/* Fill out a context for this thread */
      u = &(context[context_index]);
      UNIT (u) = p;
      STACK_USED (u) = SYSTEM_STACK_USED;
      THREAD_STACK_OFFSET (u) = NO_BYTE;
      CUR_PTR (&STACK (u)) = stack_pointer;
      CUR_PTR (&FRAME (u)) = frame_pointer;
      INI_PTR (&STACK (u)) = sp0;
      INI_PTR (&FRAME (u)) = fp0;
      SWAP (&STACK (u)) = NO_BYTE;
      SWAP (&FRAME (u)) = NO_BYTE;
      START (&STACK (u)) = NO_BYTE;
      START (&FRAME (u)) = NO_BYTE;
      BYTES (&STACK (u)) = 0;
      BYTES (&FRAME (u)) = 0;
      ACTIVE (u) = A68_TRUE;
/* Create the thread */
      RESET_ERRNO;
      if (pthread_attr_init (&new_at) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (pthread_attr_setstacksize (&new_at, (size_t) stack_size) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (pthread_attr_getstacksize (&new_at, &ss) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      ABEND ((size_t) ss != (size_t) stack_size, "cannot set thread stack size", NO_TEXT);
      if (pthread_create (&new_id, &new_at, start_unit, NULL) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      PARENT (u) = parent;
      ID (u) = new_id;
      context_index++;
      save_stacks (new_id);
    } else {
      start_parallel_units (SUB (p), parent);
    }
  }
}

/*!
\brief execute one unit from a PAR clause
\param arg dummy argument
**/

static void *start_genie_parallel (void *arg)
{
  pthread_t t;
  int k;
  BYTE_T stack_offset;
  NODE_T *p;
  BOOL_T units_active;
  (void) arg;
  LOCK_THREAD;
  t = pthread_self ();
  GET_THREAD_INDEX (k, t);
  THREAD_STACK_OFFSET (&context[k]) = (BYTE_T *) (&stack_offset - stack_direction (&stack_offset) * STACK_USED (&context[k]));
  restore_stacks (t);
  p = (NODE_T *) (UNIT (&context[k]));
/* This is the thread spawned by the main thread, we spawn parallel units and await their completion */
  start_parallel_units (SUB (p), t);
  do {
    units_active = A68_FALSE;
    check_parallel_units (&units_active, pthread_self ());
    if (units_active) {
      try_change_thread (p);
    }
  } while (units_active);
  genie_abend_thread ();
  return ((void *) NULL);
}

/*!
\brief execute parallel clause
\param p position in tree
\return propagator for this routine
**/

static PROP_T genie_parallel (NODE_T * p)
{
  int j;
  ADDR_T stack_s = 0, frame_s = 0;
  BYTE_T *system_stack_offset_s = NO_BYTE;
  int save_par_level = running_par_level;
  running_par_level = PAR_LEVEL (p);
  if (is_main_thread ()) {
/* Spawn first thread and await its completion */
    pthread_attr_t new_at;
    size_t ss;
    BYTE_T stack_offset;
    A68_THREAD_CONTEXT *u;
    LOCK_THREAD;
    abend_all_threads = A68_FALSE;
    exit_from_threads = A68_FALSE;
    par_return_code = 0;
    sp0 = stack_s = stack_pointer;
    fp0 = frame_s = frame_pointer;
    system_stack_offset_s = system_stack_offset;
    context_index = 0;
/* Set up a thread for this unit */
    u = &(context[context_index]);
    UNIT (u) = p;
    STACK_USED (u) = SYSTEM_STACK_USED;
    THREAD_STACK_OFFSET (u) = NO_BYTE;
    CUR_PTR (&STACK (u)) = stack_pointer;
    CUR_PTR (&FRAME (u)) = frame_pointer;
    INI_PTR (&STACK (u)) = sp0;
    INI_PTR (&FRAME (u)) = fp0;
    SWAP (&STACK (u)) = NO_BYTE;
    SWAP (&FRAME (u)) = NO_BYTE;
    START (&STACK (u)) = NO_BYTE;
    START (&FRAME (u)) = NO_BYTE;
    BYTES (&STACK (u)) = 0;
    BYTES (&FRAME (u)) = 0;
    ACTIVE (u) = A68_TRUE;
/* Spawn the first thread and join it to await its completion */
    RESET_ERRNO;
    if (pthread_attr_init (&new_at) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      running_par_level = save_par_level;
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (pthread_attr_setstacksize (&new_at, (size_t) stack_size) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      running_par_level = save_par_level;
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (pthread_attr_getstacksize (&new_at, &ss) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      running_par_level = save_par_level;
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    ABEND ((size_t) ss != (size_t) stack_size, "cannot set thread stack size", NO_TEXT);
    if (pthread_create (&parent_thread_id, &new_at, start_genie_parallel, NULL) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
      running_par_level = save_par_level;
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (errno != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      running_par_level = save_par_level;
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PARENT (u) = main_thread_id;
    ID (u) = parent_thread_id;
    context_index++;
    save_stacks (parent_thread_id);
    UNLOCK_THREAD;
    if (pthread_join (parent_thread_id, NULL) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      running_par_level = save_par_level;
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* The first spawned thread has completed, now clean up */
    for (j = 0; j < context_index; j++) {
      if (ACTIVE (&context[j]) && ID (&context[j]) != main_thread_id && ID (&context[j]) != parent_thread_id) {
/* If threads are zapped it is possible that some are active at this point! */
        if (pthread_join (ID (&context[j]), NULL) != 0) {
          diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
          running_par_level = save_par_level;
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
      if (SWAP (&STACK (&context[j])) != NO_BYTE) {
        free (SWAP (&STACK (&context[j])));
        SWAP (&STACK (&context[j])) = NO_BYTE;
      }
      if (SWAP (&STACK (&context[j])) != NO_BYTE) {
        free (SWAP (&STACK (&context[j])));
        SWAP (&STACK (&context[j])) = NO_BYTE;
      }
    }
/* Now every thread should have ended */
    running_par_level = save_par_level;
    context_index = 0;
    stack_pointer = stack_s;
    frame_pointer = frame_s;
    get_stack_size ();
    system_stack_offset = system_stack_offset_s;
/* See if we ended execution in parallel clause */
    if (is_main_thread () && exit_from_threads) {
      exit_genie (p, par_return_code);
    }
    if (is_main_thread () && ERROR_COUNT (&program) > 0) {
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* See if we jumped out of the parallel clause(s) */
    if (is_main_thread () && abend_all_threads) {
      JUMP_TO (TABLE (TAX (jump_label))) = UNIT (TAX (jump_label));
      longjmp (*(jump_buffer), 1);
    }
  } else {
/* Not in the main thread, spawn parallel units and await completion */
    BOOL_T units_active;
    pthread_t t = pthread_self ();
/* Spawn parallel units */
    start_parallel_units (SUB (p), t);
    do {
      units_active = A68_FALSE;
      check_parallel_units (&units_active, t);
      if (units_active) {
        try_change_thread (p);
      }
    } while (units_active);
    running_par_level = save_par_level;
  }
  return (GPROP (p));
}

/*!
\brief OP LEVEL = (INT) SEMA
\param p position in tree
**/

void genie_level_sema_int (NODE_T * p)
{
  A68_INT k;
  A68_REF s;
  POP_OBJECT (p, &k, A68_INT);
  s = heap_generator (p, MODE (INT), ALIGNED_SIZE_OF (A68_INT));
  * DEREF (A68_INT, &s) = k;
  PUSH_REF (p, s);
}

/*!
\brief OP LEVEL = (SEMA) INT
\param p position in tree
**/

void genie_level_int_sema (NODE_T * p)
{
  A68_REF s;
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), MODE (SEMA));
  PUSH_PRIMITIVE (p, VALUE (DEREF (A68_INT, &s)), A68_INT);
}

/*!
\brief OP UP = (SEMA) VOID
\param p position in tree
**/

void genie_up_sema (NODE_T * p)
{
  A68_REF s;
  if (is_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), MODE (SEMA));
  VALUE (DEREF (A68_INT, &s))++;
}

/*!
\brief OP DOWN = (SEMA) VOID
\param p position in tree
**/

void genie_down_sema (NODE_T * p)
{
  A68_REF s;
  A68_INT *k;
  BOOL_T cont = A68_TRUE;
  if (is_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), MODE (SEMA));
  while (cont) {
    k = DEREF (A68_INT, &s);
    if (VALUE (k) <= 0) {
      save_stacks (pthread_self ());
      while (VALUE (k) <= 0) {
        if (ERROR_COUNT (&program) > 0 || abend_all_threads) {
          genie_abend_thread ();
        }
        UNLOCK_THREAD;
/* Waiting a bit relaxes overhead */
        ASSERT (usleep (10) == 0);
        LOCK_THREAD;
/* Garbage may be collected, so recalculate 'k' */
        k = DEREF (A68_INT, &s);
      }
      restore_stacks (pthread_self ());
      cont = A68_TRUE;
    } else {
      VALUE (k)--;
      cont = A68_FALSE;
    }
  }
}

#endif
