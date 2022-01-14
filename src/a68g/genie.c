//! @file genie.c
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
#include "a68g-genie.h"
#include "a68g-frames.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

// Genie routines.

static A68_REF genie_clone (NODE_T *, MOID_T *, A68_REF *, A68_REF *);
static A68_REF genie_make_ref_row_of_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
static A68_REF genie_make_ref_row_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
static A68_REF genie_make_rowrow (NODE_T *, MOID_T *, int, ADDR_T);
static PROP_T genie_and_function (NODE_T *);
static PROP_T genie_assertion (NODE_T *);
static PROP_T genie_assignation_constant (NODE_T *);
static PROP_T genie_assignation (NODE_T *);
static PROP_T genie_assignation_quick (NODE_T * p);
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
static PROP_T genie_frame_identifier (NODE_T *);
static PROP_T genie_identifier (NODE_T *);
static PROP_T genie_identifier_standenv (NODE_T *);
static PROP_T genie_identifier_standenv_proc (NODE_T *);
static PROP_T genie_identity_relation (NODE_T *);
static PROP_T genie_int_case (volatile NODE_T *);
static PROP_T genie_loop (volatile NODE_T *);
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
static PROP_T genie_widen_int_to_real (NODE_T *);
static PROP_T genie_widen (NODE_T *);
static void genie_clone_stack (NODE_T *, MOID_T *, A68_REF *, A68_REF *);
static void genie_serial_units_no_label (NODE_T *, ADDR_T, NODE_T **);

//! @brief Nop for the genie, for instance '+' for INT or REAL.

void genie_idle (NODE_T * p)
{
  (void) p;
}

//! @brief Unimplemented feature handler.

void genie_unimplemented (NODE_T * p)
{
  diagnostic (A68_RUNTIME_ERROR, p, ERROR_UNIMPLEMENTED);
  exit_genie (p, A68_RUNTIME_ERROR);
}

//! @brief PROC sleep = (INT) INT

void genie_sleep (NODE_T * p)
{
  A68_INT secs;
  int wait;
  POP_OBJECT (p, &secs, A68_INT);
  wait = VALUE (&secs);
  PRELUDE_ERROR (wait < 0, p, ERROR_INVALID_ARGUMENT, M_INT);
  while (wait > 0) {
    wait = (int) sleep ((unsigned) wait);
  }
  PUSH_VALUE (p, (INT_T) 0, A68_INT);
}

//! @brief PROC system = (STRING) INT

void genie_system (NODE_T * p)
{
  int sys_ret_code, size;
  A68_REF cmd;
  A68_REF ref_z;
  POP_REF (p, &cmd);
  CHECK_INIT (p, INITIALISED (&cmd), M_STRING);
  size = 1 + a68_string_size (p, cmd);
  ref_z = heap_generator (p, M_C_STRING, 1 + size);
  sys_ret_code = system (a_to_c_string (p, DEREF (char, &ref_z), cmd));
  PUSH_VALUE (p, sys_ret_code, A68_INT);
}

//! @brief Set flags throughout tree.

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

//! @brief Leave interpretation.

void exit_genie (NODE_T * p, int ret)
{
#if defined (HAVE_CURSES)
  genie_curses_end (p);
#endif
  A68 (close_tty_on_exit) = A68_TRUE;
  if (!A68 (in_execution)) {
    return;
  }
  if (ret == A68_RUNTIME_ERROR && A68 (in_monitor)) {
    return;
  } else if (ret == A68_RUNTIME_ERROR && OPTION_DEBUG (&A68_JOB)) {
    diagnostics_to_terminal (TOP_LINE (&A68_JOB), A68_RUNTIME_ERROR);
    single_step (p, (unsigned) BREAKPOINT_ERROR_MASK);
    A68 (in_execution) = A68_FALSE;
    A68 (ret_line_number) = LINE_NUMBER (p);
    A68 (ret_code) = ret;
    longjmp (A68 (genie_exit_label), 1);
  } else {
    if ((ret & A68_FORCE_QUIT) != NULL_MASK) {
      ret &= ~A68_FORCE_QUIT;
    }
#if defined (BUILD_PARALLEL_CLAUSE)
    if (!is_main_thread ()) {
      genie_set_exit_from_threads (ret);
    } else {
      A68 (in_execution) = A68_FALSE;
      A68 (ret_line_number) = LINE_NUMBER (p);
      A68 (ret_code) = ret;
      longjmp (A68 (genie_exit_label), 1);
    }
#else
    A68 (in_execution) = A68_FALSE;
    A68 (ret_line_number) = LINE_NUMBER (p);
    A68 (ret_code) = ret;
    longjmp (A68 (genie_exit_label), 1);
#endif
  }
}

//! @brief Genie init rng.

void genie_init_rng (void)
{
  time_t t;
  if (time (&t) != -1) {
    init_rng ((unsigned) t);
  }
}

//! @brief Tie label to the clause it is defined in.

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

//! @brief Tie label to the clause it is defined in.

static void tie_label (NODE_T * p, NODE_T * unit)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, DEFINING_IDENTIFIER)) {
      UNIT (TAX (p)) = unit;
    }
    tie_label (SUB (p), unit);
  }
}

//! @brief Tie label to the clause it is defined in.

void tie_label_to_unit (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, LABELED_UNIT)) {
      tie_label (SUB_SUB (p), NEXT_SUB (p));
    }
    tie_label_to_unit (SUB (p));
  }
}

//! @brief Fast way to indicate a mode.

static int mode_attribute (MOID_T * p)
{
  if (IS_REF (p)) {
    return REF_SYMBOL;
  } else if (IS (p, PROC_SYMBOL)) {
    return PROC_SYMBOL;
  } else if (IS_UNION (p)) {
    return UNION_SYMBOL;
  } else if (p == M_INT) {
    return MODE_INT;
  } else if (p == M_LONG_INT) {
    return MODE_LONG_INT;
  } else if (p == M_LONG_LONG_INT) {
    return MODE_LONG_LONG_INT;
  } else if (p == M_REAL) {
    return MODE_REAL;
  } else if (p == M_LONG_REAL) {
    return MODE_LONG_REAL;
  } else if (p == M_LONG_LONG_REAL) {
    return MODE_LONG_LONG_REAL;
  } else if (p == M_COMPLEX) {
    return MODE_COMPLEX;
  } else if (p == M_LONG_COMPLEX) {
    return MODE_LONG_COMPLEX;
  } else if (p == M_LONG_LONG_COMPLEX) {
    return MODE_LONG_LONG_COMPLEX;
  } else if (p == M_BOOL) {
    return MODE_BOOL;
  } else if (p == M_CHAR) {
    return MODE_CHAR;
  } else if (p == M_BITS) {
    return MODE_BITS;
  } else if (p == M_LONG_BITS) {
    return MODE_LONG_BITS;
  } else if (p == M_LONG_LONG_BITS) {
    return MODE_LONG_LONG_BITS;
  } else if (p == M_BYTES) {
    return MODE_BYTES;
  } else if (p == M_LONG_BYTES) {
    return MODE_LONG_BYTES;
  } else if (p == M_FILE) {
    return MODE_FILE;
  } else if (p == M_FORMAT) {
    return MODE_FORMAT;
  } else if (p == M_PIPE) {
    return MODE_PIPE;
  } else if (p == M_SOUND) {
    return MODE_SOUND;
  } else {
    return MODE_NO_CHECK;
  }
}

//! @brief Perform tasks before interpretation.

void genie_preprocess (NODE_T * p, int *max_lev, void *compile_lib)
{
#if defined (BUILD_A68_COMPILER)
  static char *last_compile_name = NO_TEXT;
  static PROP_PROC *last_compile_unit = NO_PPROC;
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
// The default.
      UNIT (&GPROP (p)) = genie_unit;
      SOURCE (&GPROP (p)) = p;
#if defined (BUILD_A68_COMPILER)
      if (OPTION_OPT_LEVEL (&A68_JOB) > 0 && COMPILE_NAME (GINFO (p)) != NO_TEXT && compile_lib != NULL) {
        if (COMPILE_NAME (GINFO (p)) == last_compile_name) {
// We copy.
          UNIT (&GPROP (p)) = last_compile_unit;
        } else {
// We look up.
// Next line may provoke a warning even with this POSIX workaround. Tant pis.
          *(void **) &(UNIT (&GPROP (p))) = dlsym (compile_lib, COMPILE_NAME (GINFO (p)));
          ABEND (UNIT (&GPROP (p)) == NULL, ERROR_INTERNAL_CONSISTENCY, dlerror ());
          last_compile_name = COMPILE_NAME (GINFO (p));
          last_compile_unit = UNIT (&GPROP (p));
        }
      }
#endif
    }
    if (MOID (p) != NO_MOID) {
      SIZE (MOID (p)) = moid_size (MOID (p));
      DIGITS (MOID (p)) = moid_digits (MOID (p));
      SHORT_ID (MOID (p)) = mode_attribute (MOID (p));
      if (GINFO (p) != NO_GINFO) {
        NEED_DNS (GINFO (p)) = A68_FALSE;
        if (IS_REF (MOID (p))) {
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
        OFFSET (GINFO (p)) = &(A68_STACK[FRAME_INFO_SIZE + OFFSET (q)]);
      }
    } else if (IS (p, OPERATOR)) {
      TAG_T *q = TAX (p);
      if (q != NO_TAG && NODE (q) != NO_NODE && TABLE (NODE (q)) != NO_TABLE) {
        LEVEL (GINFO (p)) = LEX_LEVEL (NODE (q));
        OFFSET (GINFO (p)) = &(A68_STACK[FRAME_INFO_SIZE + OFFSET (q)]);
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

//! @brief Get outermost lexical level in the user program.

void get_global_level (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (LINE_NUMBER (p) != 0 && IS (p, UNIT)) {
      if (LEX_LEVEL (p) < A68 (global_level)) {
        A68 (global_level) = LEX_LEVEL (p);
      }
    }
    get_global_level (SUB (p));
  }
}

//! @brief Driver for the interpreter.

void genie (void *compile_lib)
{
  MOID_T *m;
// Fill in final info for modes.
  for (m = TOP_MOID (&A68_JOB); m != NO_MOID; FORWARD (m)) {
    SIZE (m) = moid_size (m);
    DIGITS (m) = moid_digits (m);
    SHORT_ID (m) = mode_attribute (m);
  }
// Preprocessing.
  A68 (max_lex_lvl) = 0;
//  genie_lex_levels (TOP_NODE (&A68_JOB), 1);.
  genie_preprocess (TOP_NODE (&A68_JOB), &A68 (max_lex_lvl), compile_lib);
  change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
  A68_MON (watchpoint_expression) = NO_TEXT;
  A68 (frame_stack_limit) = A68 (frame_end) - A68 (storage_overhead);
  A68 (expr_stack_limit) = A68 (stack_end) - A68 (storage_overhead);
  if (OPTION_REGRESSION_TEST (&A68_JOB)) {
    init_rng (1);
  } else {
    genie_init_rng ();
  }
  io_close_tty_line ();
  if (OPTION_TRACE (&A68_JOB)) {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "genie: frame stack " A68_LU "k, expression stack " A68_LU "k, heap " A68_LU "k, handles " A68_LU "k\n", A68 (frame_stack_size) / KILOBYTE, A68 (expr_stack_size) / KILOBYTE, A68 (heap_size) / KILOBYTE, A68 (handle_pool_size) / KILOBYTE) >= 0);
    WRITE (STDOUT_FILENO, A68 (output_line));
  }
  install_signal_handlers ();
  set_default_event_procedure (&A68 (on_gc_event));
  A68 (do_confirm_exit) = A68_TRUE;
#if defined (BUILD_PARALLEL_CLAUSE)
  ASSERT (pthread_mutex_init (&A68_PAR (unit_sema), NULL) == 0);
#endif
// Dive into the program.
  if (setjmp (A68 (genie_exit_label)) == 0) {
    NODE_T *p = SUB (TOP_NODE (&A68_JOB));
// If we are to stop in the monitor, set a breakpoint on the first unit.
    if (OPTION_DEBUG (&A68_JOB)) {
      change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_TEMPORARY_MASK, A68_TRUE);
      WRITE (STDOUT_FILENO, "Execution begins ...");
    }
    errno = 0;
    A68 (ret_code) = 0;
    A68 (global_level) = INT_MAX;
    A68_GLOBALS = 0;
    get_global_level (p);
    A68_FP = A68 (frame_start);
    A68_SP = A68 (stack_start);
    FRAME_DYNAMIC_LINK (A68_FP) = 0;
    FRAME_DNS (A68_FP) = 0;
    FRAME_STATIC_LINK (A68_FP) = 0;
    FRAME_NUMBER (A68_FP) = 0;
    FRAME_TREE (A68_FP) = (NODE_T *) p;
    FRAME_LEXICAL_LEVEL (A68_FP) = LEX_LEVEL (p);
    FRAME_PARAMETER_LEVEL (A68_FP) = LEX_LEVEL (p);
    FRAME_PARAMETERS (A68_FP) = A68_FP;
    initialise_frame (p);
    genie_init_heap (p);
    genie_init_transput (TOP_NODE (&A68_JOB));
    A68 (cputime_0) = seconds ();
// Here we go ...
    A68 (in_execution) = A68_TRUE;
    A68 (f_entry) = TOP_NODE (&A68_JOB);
#if defined (BUILD_UNIX)
    (void) alarm (1);
#endif
    if (OPTION_TRACE (&A68_JOB)) {
      WIS (TOP_NODE (&A68_JOB));
    }
    (void) genie_enclosed (TOP_NODE (&A68_JOB));
  } else {
// Here we have jumped out of the interpreter. What happened?.
    if (OPTION_DEBUG (&A68_JOB)) {
      WRITE (STDOUT_FILENO, "Execution discontinued");
    }
    if (A68 (ret_code) == A68_RERUN) {
      diagnostics_to_terminal (TOP_LINE (&A68_JOB), A68_RUNTIME_ERROR);
      genie (compile_lib);
    } else if (A68 (ret_code) == A68_RUNTIME_ERROR) {
      if (OPTION_BACKTRACE (&A68_JOB)) {
        int printed = 0;
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\nStack backtrace") >= 0);
        WRITE (STDOUT_FILENO, A68 (output_line));
        stack_dump (STDOUT_FILENO, A68_FP, 16, &printed);
        WRITE (STDOUT_FILENO, NEWLINE_STRING);
      }
      if (FILE_LISTING_OPENED (&A68_JOB)) {
        int printed = 0;
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\nStack backtrace") >= 0);
        WRITE (FILE_LISTING_FD (&A68_JOB), A68 (output_line));
        stack_dump (FILE_LISTING_FD (&A68_JOB), A68_FP, 32, &printed);
      }
    }
  }
  A68 (in_execution) = A68_FALSE;
}

// This file contains interpreter ("genie") routines related to executing primitive
// A68 actions.
// 
// The genie is self-optimising as when it traverses the tree, it stores terminals
// it ends up in at the root where traversing for that terminal started.
// Such piece of information is called a PROP.

//! @brief Shows line where 'p' is at and draws a '-' beneath the position.

void where_in_source (FILE_T f, NODE_T * p)
{
  write_source_line (f, LINE (INFO (p)), p, A68_NO_DIAGNOSTICS);
}

// Since Algol 68 can pass procedures as parameters, we use static links rather
// than a display.

//! @brief Initialise PROC and OP identities.

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
// Store position so we need not search again.
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

//! @brief Initialise PROC and OP identity declarations.

void genie_find_proc_op (NODE_T * p, int *count)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (GINFO (p) != NO_GINFO && IS_NEW_LEXICAL_LEVEL (GINFO (p))) {
// Don't enter a new lexical level - it will have its own initialisation.
      return;
    } else if (IS (p, PROCEDURE_DECLARATION) || IS (p, BRIEF_OPERATOR_DECLARATION)) {
      genie_init_proc_op (SUB (p), &(SEQUENCE (TABLE (p))), count);
      return;
    } else {
      genie_find_proc_op (SUB (p), count);
    }
  }
}

//! @brief Initialise stack frame.

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
    if (SEQUENCE (TABLE (p)) == NO_NODE) {
      int count = 0;
      genie_find_proc_op (p, &count);
      PROC_OPS (TABLE (p)) = (BOOL_T) (count > 0);
    }
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

//! @brief Whether item at "w" of mode "q" is initialised.

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
      A68_REAL *i = (A68_REAL *) (w + SIZE_ALIGNED (A68_REAL));
      CHECK_INIT (p, INITIALISED (r), q);
      CHECK_INIT (p, INITIALISED (i), q);
      return;
    }
#if (A68_LEVEL >= 3)
  case MODE_LONG_INT:
  case MODE_LONG_REAL:
  case MODE_LONG_BITS:
    {
      A68_DOUBLE *z = (A68_DOUBLE *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_LONG_COMPLEX:
    {
      A68_LONG_REAL *r = (A68_LONG_REAL *) w;
      A68_LONG_REAL *i = (A68_LONG_REAL *) (w + SIZE_ALIGNED (A68_LONG_REAL));
      CHECK_INIT (p, INITIALISED (r), q);
      CHECK_INIT (p, INITIALISED (i), q);
      return;
    }
  case MODE_LONG_LONG_INT:
  case MODE_LONG_LONG_REAL:
  case MODE_LONG_LONG_BITS:
    {
      MP_T *z = (MP_T *) w;
      CHECK_INIT (p, (unsigned) MP_STATUS (z) & INIT_MASK, q);
      return;
    }
#else
  case MODE_LONG_INT:
  case MODE_LONG_LONG_INT:
  case MODE_LONG_REAL:
  case MODE_LONG_LONG_REAL:
  case MODE_LONG_BITS:
  case MODE_LONG_LONG_BITS:
    {
      MP_T *z = (MP_T *) w;
      CHECK_INIT (p, (unsigned) MP_STATUS (z) & INIT_MASK, q);
      return;
    }
  case MODE_LONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_mp ());
      CHECK_INIT (p, (unsigned) r[0] & INIT_MASK, q);
      CHECK_INIT (p, (unsigned) i[0] & INIT_MASK, q);
      return;
    }
#endif
  case MODE_LONG_LONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_long_mp ());
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

//! @brief Push constant stored in the tree.

static PROP_T genie_constant (NODE_T * p)
{
  PUSH (p, CONSTANT (GINFO (p)), SIZE (GINFO (p)));
  return GPROP (p);
}

//! @brief Unite value in the stack and push result.

static PROP_T genie_uniting (NODE_T * p)
{
  PROP_T self;
  ADDR_T sp = A68_SP;
  MOID_T *u = MOID (p), *v = MOID (SUB (p));
  int size = SIZE (u);
  if (ATTRIBUTE (v) != UNION_SYMBOL) {
    MOID_T *w = unites_to (v, u);
    PUSH_UNION (p, (void *) w);
    EXECUTE_UNIT (SUB (p));
    STACK_DNS (p, SUB (v), A68_FP);
  } else {
    A68_UNION *m = (A68_UNION *) STACK_TOP;
    EXECUTE_UNIT (SUB (p));
    STACK_DNS (p, SUB (v), A68_FP);
    VALUE (m) = (void *) unites_to ((MOID_T *) VALUE (m), u);
  }
  A68_SP = sp + size;
  UNIT (&self) = genie_uniting;
  SOURCE (&self) = p;
  return self;
}

//! @brief Store widened constant as a constant.

static void make_constant_widening (NODE_T * p, MOID_T * m, PROP_T * self)
{
  if (SUB (p) != NO_NODE && CONSTANT (GINFO (SUB (p))) != NO_CONSTANT) {
    int size = SIZE (m);
    UNIT (self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), (void *) (STACK_OFFSET (-size)), size);
  }
}

//! @brief (optimised) push INT widened to REAL

static PROP_T genie_widen_int_to_real (NODE_T * p)
{
  A68_INT *i = (A68_INT *) STACK_TOP;
  A68_REAL *z = (A68_REAL *) STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  INCREMENT_STACK_POINTER (p, SIZE_ALIGNED (A68_REAL) - SIZE (M_INT));
  VALUE (z) = (REAL_T) VALUE (i);
  STATUS (z) = INIT_MASK;
  return GPROP (p);
}

//! @brief Widen value in the stack.

static PROP_T genie_widen (NODE_T * p)
{
#define COERCE_FROM_TO(p, a, b) (MOID (p) == (b) && MOID (SUB (p)) == (a))
  PROP_T self;
  UNIT (&self) = genie_widen;
  SOURCE (&self) = p;
// INT widenings.
  if (COERCE_FROM_TO (p, M_INT, M_REAL)) {
    (void) genie_widen_int_to_real (p);
    UNIT (&self) = genie_widen_int_to_real;
    make_constant_widening (p, M_REAL, &self);
  } else if (COERCE_FROM_TO (p, M_INT, M_LONG_INT)) {
    EXECUTE_UNIT (SUB (p));
#if (A68_LEVEL >= 3)
    genie_lengthen_int_to_int_16 (p);
#else
    genie_lengthen_int_to_mp (p);
#endif
    make_constant_widening (p, M_LONG_INT, &self);
  } else if (COERCE_FROM_TO (p, M_LONG_INT, M_LONG_LONG_INT)) {
    EXECUTE_UNIT (SUB (p));
#if (A68_LEVEL >= 3)
    genie_lengthen_int_16_to_mp (p);
#else
    genie_lengthen_mp_to_long_mp (p);
#endif
    make_constant_widening (p, M_LONG_LONG_INT, &self);
  } else if (COERCE_FROM_TO (p, M_LONG_INT, M_LONG_REAL)) {
#if (A68_LEVEL >= 3)
    (void) genie_widen_int_16_to_real_16 (p);
#else
// 1-1 mapping.
    EXECUTE_UNIT (SUB (p));
#endif
    make_constant_widening (p, M_LONG_REAL, &self);
  } else if (COERCE_FROM_TO (p, M_LONG_LONG_INT, M_LONG_LONG_REAL)) {
    EXECUTE_UNIT (SUB (p));
// 1-1 mapping.
    make_constant_widening (p, M_LONG_LONG_REAL, &self);
  }
// REAL widenings.
  else if (COERCE_FROM_TO (p, M_REAL, M_LONG_REAL)) {
    EXECUTE_UNIT (SUB (p));
#if (A68_LEVEL >= 3)
    genie_lengthen_real_to_real_16 (p);
#else
    genie_lengthen_real_to_mp (p);
#endif
    make_constant_widening (p, M_LONG_REAL, &self);
  } else if (COERCE_FROM_TO (p, M_LONG_REAL, M_LONG_LONG_REAL)) {
    EXECUTE_UNIT (SUB (p));
#if (A68_LEVEL >= 3)
    genie_lengthen_real_16_to_mp (p);
#else
    genie_lengthen_mp_to_long_mp (p);
#endif
    make_constant_widening (p, M_LONG_LONG_REAL, &self);
  } else if (COERCE_FROM_TO (p, M_REAL, M_COMPLEX)) {
    EXECUTE_UNIT (SUB (p));
    PUSH_VALUE (p, 0.0, A68_REAL);
    make_constant_widening (p, M_COMPLEX, &self);
  } else if (COERCE_FROM_TO (p, M_LONG_REAL, M_LONG_COMPLEX)) {
#if (A68_LEVEL >= 3)
    QUAD_WORD_T z;
    z.f = 0.0q;
    EXECUTE_UNIT (SUB (p));
    PUSH_VALUE (p, z, A68_LONG_REAL);
#else
    EXECUTE_UNIT (SUB (p));
    (void) nil_mp (p, DIGITS (M_LONG_REAL));
    make_constant_widening (p, M_LONG_COMPLEX, &self);
#endif
  } else if (COERCE_FROM_TO (p, M_LONG_LONG_REAL, M_LONG_LONG_COMPLEX)) {
    EXECUTE_UNIT (SUB (p));
    (void) nil_mp (p, DIGITS (M_LONG_LONG_REAL));
    make_constant_widening (p, M_LONG_LONG_COMPLEX, &self);
  } else if (COERCE_FROM_TO (p, M_COMPLEX, M_LONG_COMPLEX)) {
// COMPLEX widenings.
    EXECUTE_UNIT (SUB (p));
#if (A68_LEVEL >= 3)
    genie_lengthen_complex_to_complex_32 (p);
#else
    genie_lengthen_complex_to_mp_complex (p);
#endif
    make_constant_widening (p, M_LONG_COMPLEX, &self);
  } else if (COERCE_FROM_TO (p, M_LONG_COMPLEX, M_LONG_LONG_COMPLEX)) {
    EXECUTE_UNIT (SUB (p));
#if (A68_LEVEL >= 3)
    genie_lengthen_complex_32_to_long_mp_complex (p);
#else
    genie_lengthen_mp_complex_to_long_mp_complex (p);
#endif
    make_constant_widening (p, M_LONG_LONG_COMPLEX, &self);
  } else if (COERCE_FROM_TO (p, M_BITS, M_LONG_BITS)) {
// BITS widenings.
    EXECUTE_UNIT (SUB (p));
#if (A68_LEVEL >= 3)
    genie_lengthen_bits_to_double_bits (p);
#else
    genie_lengthen_int_to_mp (p);
#endif
    make_constant_widening (p, M_LONG_BITS, &self);
  } else if (COERCE_FROM_TO (p, M_LONG_BITS, M_LONG_LONG_BITS)) {
#if (A68_LEVEL >= 3)
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
#else
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_mp_to_long_mp (p);
    make_constant_widening (p, M_LONG_LONG_BITS, &self);
#endif
  } else if (COERCE_FROM_TO (p, M_BITS, M_ROW_BOOL) || COERCE_FROM_TO (p, M_BITS, M_FLEX_ROW_BOOL)) {
    A68_BITS x;
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int k;
    UNSIGNED_T bit;
    BYTE_T *base;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &x, A68_BITS);
    NEW_ROW_1D (z, row, arr, tup, M_ROW_BOOL, M_BOOL, BITS_WIDTH);
    base = ADDRESS (&row) + SIZE (M_BOOL) * (BITS_WIDTH - 1);
    bit = 1;
    for (k = BITS_WIDTH - 1; k >= 0; k--, base -= SIZE (M_BOOL), bit <<= 1) {
      STATUS ((A68_BOOL *) base) = INIT_MASK;
      VALUE ((A68_BOOL *) base) = (BOOL_T) ((VALUE (&x) & bit) != 0 ? A68_TRUE : A68_FALSE);
    }
    PUSH_REF (p, z);
  } else if (COERCE_FROM_TO (p, M_LONG_BITS, M_ROW_BOOL) || COERCE_FROM_TO (p, M_LONG_BITS, M_FLEX_ROW_BOOL)) {
#if (A68_LEVEL >= 3)
    A68_LONG_BITS x;
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int k;
    UNSIGNED_T bit;
    BYTE_T *base;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &x, A68_LONG_BITS);
    NEW_ROW_1D (z, row, arr, tup, M_ROW_BOOL, M_BOOL, LONG_BITS_WIDTH);
    base = ADDRESS (&row) + SIZE (M_BOOL) * (LONG_BITS_WIDTH - 1);
    bit = 1;
    for (k = BITS_WIDTH - 1; k >= 0; k--, base -= SIZE (M_BOOL), bit <<= 1) {
      STATUS ((A68_BOOL *) base) = INIT_MASK;
      VALUE ((A68_BOOL *) base) = (BOOL_T) ((LW (VALUE (&x)) & bit) != 0 ? A68_TRUE : A68_FALSE);
    }
    bit = 1;
    for (k = BITS_WIDTH - 1; k >= 0; k--, base -= SIZE (M_BOOL), bit <<= 1) {
      STATUS ((A68_BOOL *) base) = INIT_MASK;
      VALUE ((A68_BOOL *) base) = (BOOL_T) ((HW (VALUE (&x)) & bit) != 0 ? A68_TRUE : A68_FALSE);
    }
    PUSH_REF (p, z);
#else
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_bits_to_row_bool (p);
#endif
  } else if (COERCE_FROM_TO (p, M_LONG_LONG_BITS, M_ROW_BOOL) || COERCE_FROM_TO (p, M_LONG_LONG_BITS, M_FLEX_ROW_BOOL)) {
#if (A68_LEVEL <= 2)
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_bits_to_row_bool (p);
#endif
  } else if (COERCE_FROM_TO (p, M_BYTES, M_ROW_CHAR) || COERCE_FROM_TO (p, M_BYTES, M_FLEX_ROW_CHAR)) {
    A68_BYTES z;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &z, A68_BYTES);
    PUSH_REF (p, c_string_to_row_char (p, VALUE (&z), BYTES_WIDTH));
  } else if (COERCE_FROM_TO (p, M_LONG_BYTES, M_ROW_CHAR) || COERCE_FROM_TO (p, M_LONG_BYTES, M_FLEX_ROW_CHAR)) {
    A68_LONG_BYTES z;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &z, A68_LONG_BYTES);
    PUSH_REF (p, c_string_to_row_char (p, VALUE (&z), LONG_BYTES_WIDTH));
  } else {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_CANNOT_WIDEN, MOID (SUB (p)), MOID (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  return self;
#undef COERCE_FROM_TO
}

//! @brief Cast a jump to a PROC VOID without executing the jump.

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
  MOID (&z) = M_PROC_VOID;
  PUSH_PROCEDURE (p, z);
}

//! @brief (optimised) dereference value of a unit

static PROP_T genie_dereferencing_quick (NODE_T * p)
{
  A68_REF *z = (A68_REF *) STACK_TOP;
  ADDR_T pop_sp = A68_SP;
  BYTE_T *stack_top = STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  A68_SP = pop_sp;
  CHECK_REF (p, *z, MOID (SUB (p)));
  PUSH (p, ADDRESS (z), SIZE (MOID (p)));
  genie_check_initialisation (p, stack_top, MOID (p));
  return GPROP (p);
}

//! @brief Dereference an identifier.

static PROP_T genie_dereference_frame_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB_MOID (p);
  BYTE_T *stack_top = STACK_TOP;
  FRAME_GET (z, A68_REF, p);
  PUSH (p, ADDRESS (z), SIZE (deref));
  genie_check_initialisation (p, stack_top, deref);
  return GPROP (p);
}

//! @brief Dereference an identifier.

static PROP_T genie_dereference_generic_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB_MOID (p);
  BYTE_T *stack_top = STACK_TOP;
  FRAME_GET (z, A68_REF, p);
  CHECK_REF (p, *z, MOID (SUB (p)));
  PUSH (p, ADDRESS (z), SIZE (deref));
  genie_check_initialisation (p, stack_top, deref);
  return GPROP (p);
}

//! @brief Slice REF [] A to A.

static PROP_T genie_dereference_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *prim = SUB (p);
  A68_ARRAY *a;
  A68_TUPLE *t;
  A68_REF *z;
  MOID_T *ref_mode = MOID (p);
  MOID_T *deref_mode = SUB (ref_mode);
  int size = SIZE (deref_mode), row_index;
  ADDR_T pop_sp = A68_SP;
  BYTE_T *stack_top = STACK_TOP;
// Get REF [].
  z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (prim);
  A68_SP = pop_sp;
  CHECK_REF (p, *z, ref_mode);
  GET_DESCRIPTOR (a, t, DEREF (A68_ROW, z));
  for (row_index = 0, q = SEQUENCE (p); q != NO_NODE; t++, q = SEQUENCE (q)) {
    A68_INT *j = (A68_INT *) STACK_TOP;
    int k;
    EXECUTE_UNIT (q);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    row_index += (SPAN (t) * k - SHIFT (t));
    A68_SP = pop_sp;
  }
// Push element.
  PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, row_index)]), size);
  genie_check_initialisation (p, stack_top, deref_mode);
  return GPROP (p);
}

//! @brief Dereference SELECTION from a name.

static PROP_T genie_dereference_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  MOID_T *result_mode = SUB_MOID (selector);
  int size = SIZE (result_mode);
  A68_REF *z = (A68_REF *) STACK_TOP;
  ADDR_T pop_sp = A68_SP;
  BYTE_T *stack_top;
  EXECUTE_UNIT (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  A68_SP = pop_sp;
  stack_top = STACK_TOP;
  PUSH (p, ADDRESS (z), size);
  genie_check_initialisation (p, stack_top, result_mode);
  return GPROP (p);
}

//! @brief Dereference name in the stack.

static PROP_T genie_dereferencing (NODE_T * p)
{
  A68_REF z;
  PROP_T self;
  EXECUTE_UNIT_2 (SUB (p), self);
  POP_REF (p, &z);
  CHECK_REF (p, z, MOID (SUB (p)));
  PUSH (p, ADDRESS (&z), SIZE (MOID (p)));
  genie_check_initialisation (p, STACK_OFFSET (-SIZE (MOID (p))), MOID (p));
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
  return self;
}

//! @brief Deprocedure PROC in the stack.

static PROP_T genie_deproceduring (NODE_T * p)
{
  PROP_T self;
  A68_PROCEDURE *z;
  ADDR_T pop_sp = A68_SP, pop_fp = A68_FP;
  NODE_T *proc = SUB (p);
  MOID_T *proc_mode = MOID (proc);
  UNIT (&self) = genie_deproceduring;
  SOURCE (&self) = p;
// Get procedure.
  z = (A68_PROCEDURE *) STACK_TOP;
  EXECUTE_UNIT (proc);
  A68_SP = pop_sp;
  genie_check_initialisation (p, (BYTE_T *) z, proc_mode);
  genie_call_procedure (p, proc_mode, proc_mode, M_VOID, z, pop_sp, pop_fp);
  STACK_DNS (p, MOID (p), A68_FP);
  return self;
}

//! @brief Voiden value in the stack.

static PROP_T genie_voiding (NODE_T * p)
{
  PROP_T self, source;
  ADDR_T sp_for_voiding = A68_SP;
  SOURCE (&self) = p;
  EXECUTE_UNIT_2 (SUB (p), source);
  A68_SP = sp_for_voiding;
  if (UNIT (&source) == genie_assignation_quick) {
    UNIT (&self) = genie_voiding_assignation;
    SOURCE (&self) = SOURCE (&source);
  } else if (UNIT (&source) == genie_assignation_constant) {
    UNIT (&self) = genie_voiding_assignation_constant;
    SOURCE (&self) = SOURCE (&source);
  } else {
    UNIT (&self) = genie_voiding;
  }
  return self;
}

//! @brief Coerce value in the stack.

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
      self = genie_widen (p);
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
  return self;
}

//! @brief Push argument units.

static void genie_argument (NODE_T * p, NODE_T ** seq)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      EXECUTE_UNIT (p);
      STACK_DNS (p, MOID (p), A68_FP);
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

//! @brief Evaluate partial call.

void genie_partial_call (NODE_T * p, MOID_T * pr_mode, MOID_T * pproc, MOID_T * pmap, A68_PROCEDURE z, ADDR_T pop_sp, ADDR_T pop_fp)
{
  int voids = 0;
  BYTE_T *u, *v;
  PACK_T *s, *t;
  A68_REF ref;
  A68_HANDLE *loc;
// Get locale for the new procedure descriptor. Copy is necessary.
  if (LOCALE (&z) == NO_HANDLE) {
    int size = 0;
    for (s = PACK (pr_mode); s != NO_PACK; FORWARD (s)) {
      size += (SIZE (M_BOOL) + SIZE (MOID (s)));
    }
    ref = heap_generator (p, pr_mode, size);
    loc = REF_HANDLE (&ref);
  } else {
    int size = SIZE (LOCALE (&z));
    ref = heap_generator (p, pr_mode, size);
    loc = REF_HANDLE (&ref);
    COPY (POINTER (loc), POINTER (LOCALE (&z)), size);
  }
// Move arguments from stack to locale using pmap.
  u = POINTER (loc);
  s = PACK (pr_mode);
  v = STACK_ADDRESS (pop_sp);
  t = PACK (pmap);
  for (; t != NO_PACK && s != NO_PACK; FORWARD (t)) {
// Skip already initialised arguments.
    while (u != NULL && VALUE ((A68_BOOL *) & u[0])) {
      u = &(u[SIZE (M_BOOL) + SIZE (MOID (s))]);
      FORWARD (s);
    }
    if (u != NULL && MOID (t) == M_VOID) {
// Move to next field in locale.
      voids++;
      u = &(u[SIZE (M_BOOL) + SIZE (MOID (s))]);
      FORWARD (s);
    } else {
// Move argument from stack to locale.
      A68_BOOL w;
      STATUS (&w) = INIT_MASK;
      VALUE (&w) = A68_TRUE;
      *(A68_BOOL *) & u[0] = w;
      COPY (&(u[SIZE (M_BOOL)]), v, SIZE (MOID (t)));
      u = &(u[SIZE (M_BOOL) + SIZE (MOID (s))]);
      v = &(v[SIZE (MOID (t))]);
      FORWARD (s);
    }
  }
  A68_SP = pop_sp;
  LOCALE (&z) = loc;
// Is closure complete?.
  if (voids == 0) {
// Closure is complete. Push locale onto the stack and call procedure body.
    A68_SP = pop_sp;
    u = POINTER (loc);
    v = STACK_ADDRESS (A68_SP);
    s = PACK (pr_mode);
    for (; s != NO_PACK; FORWARD (s)) {
      int size = SIZE (MOID (s));
      COPY (v, &u[SIZE (M_BOOL)], size);
      u = &(u[SIZE (M_BOOL) + size]);
      v = &(v[SIZE (MOID (s))]);
      INCREMENT_STACK_POINTER (p, size);
    }
    genie_call_procedure (p, pr_mode, pproc, M_VOID, &z, pop_sp, pop_fp);
  } else {
//  Closure is not complete. Return procedure body.
    PUSH_PROCEDURE (p, z);
  }
}

//! @brief Closure and deproceduring of routines with PARAMSETY.

void genie_call_procedure (NODE_T * p, MOID_T * pr_mode, MOID_T * pproc, MOID_T * pmap, A68_PROCEDURE * z, ADDR_T pop_sp, ADDR_T pop_fp)
{
  if (pmap != M_VOID && pr_mode != pmap) {
    genie_partial_call (p, pr_mode, pproc, pmap, *z, pop_sp, pop_fp);
  } else if (STATUS (z) & STANDENV_PROC_MASK) {
    (void) ((*(PROCEDURE (&(BODY (z))))) (p));
  } else if (STATUS (z) & SKIP_PROCEDURE_MASK) {
    A68_SP = pop_sp;
    genie_push_undefined (p, SUB ((MOID (z))));
  } else {
    NODE_T *body = NODE (&(BODY (z)));
    if (IS (body, ROUTINE_TEXT)) {
      NODE_T *entry = SUB (body);
      PACK_T *args = PACK (pr_mode);
      ADDR_T fp0 = 0;
// Copy arguments from stack to frame.
      OPEN_PROC_FRAME (entry, ENVIRON (z));
      INIT_STATIC_FRAME (entry);
      FRAME_DNS (A68_FP) = pop_fp;
      for (; args != NO_PACK; FORWARD (args)) {
        int size = SIZE (MOID (args));
        COPY ((FRAME_OBJECT (fp0)), STACK_ADDRESS (pop_sp + fp0), size);
        fp0 += size;
      }
      A68_SP = pop_sp;
      ARGSIZE (GINFO (p)) = fp0;
// Interpret routine text.
      if (DIM (pr_mode) > 0) {
// With PARAMETERS.
        entry = NEXT (NEXT_NEXT (entry));
      } else {
// Without PARAMETERS.
        entry = NEXT_NEXT (entry);
      }
      EXECUTE_UNIT_TRACE (entry);
      if (A68_FP == A68_MON (finish_frame_pointer)) {
        change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
      }
      CLOSE_FRAME;
      STACK_DNS (p, SUB (pr_mode), A68_FP);
    } else {
      OPEN_PROC_FRAME (body, ENVIRON (z));
      INIT_STATIC_FRAME (body);
      FRAME_DNS (A68_FP) = pop_fp;
      EXECUTE_UNIT_TRACE (body);
      if (A68_FP == A68_MON (finish_frame_pointer)) {
        change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
      }
      CLOSE_FRAME;
      STACK_DNS (p, SUB (pr_mode), A68_FP);
    }
  }
}

//! @brief Call event routine.

void genie_call_event_routine (NODE_T * p, MOID_T * m, A68_PROCEDURE * proc, ADDR_T pop_sp, ADDR_T pop_fp)
{
  if (NODE (&(BODY (proc))) != NO_NODE) {
    A68_PROCEDURE save = *proc;
    set_default_event_procedure (proc);
    genie_call_procedure (p, MOID (&save), m, m, &save, pop_sp, pop_fp);
    (*proc) = save;
  }
}

//! @brief Call PROC with arguments and push result.

static PROP_T genie_call_standenv_quick (NODE_T * p)
{
  NODE_T *pr = SUB (p), *q = SEQUENCE (p);
  TAG_T *proc = TAX (SOURCE (&GPROP (pr)));
// Get arguments.
  for (; q != NO_NODE; q = SEQUENCE (q)) {
    EXECUTE_UNIT (q);
    STACK_DNS (p, MOID (q), A68_FP);
  }
  (void) ((*(PROCEDURE (proc))) (p));
  return GPROP (p);
}

//! @brief Call PROC with arguments and push result.

static PROP_T genie_call_quick (NODE_T * p)
{
  A68_PROCEDURE z;
  NODE_T *proc = SUB (p);
  ADDR_T pop_sp = A68_SP, pop_fp = A68_FP;
// Get procedure.
  EXECUTE_UNIT (proc);
  POP_OBJECT (proc, &z, A68_PROCEDURE);
  genie_check_initialisation (p, (BYTE_T *) & z, MOID (proc));
// Get arguments.
  if (SEQUENCE (p) == NO_NODE && !STATUS_TEST (p, SEQUENCE_MASK)) {
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
      STACK_DNS (p, MOID (q), A68_FP);
    }
  }
  genie_call_procedure (p, MOID (&z), PARTIAL_PROC (GINFO (proc)), PARTIAL_LOCALE (GINFO (proc)), &z, pop_sp, pop_fp);
  return GPROP (p);
}

//! @brief Call PROC with arguments and push result.

static PROP_T genie_call (NODE_T * p)
{
  PROP_T self;
  A68_PROCEDURE z;
  NODE_T *proc = SUB (p);
  ADDR_T pop_sp = A68_SP, pop_fp = A68_FP;
  UNIT (&self) = genie_call_quick;
  SOURCE (&self) = p;
// Get procedure.
  EXECUTE_UNIT (proc);
  POP_OBJECT (proc, &z, A68_PROCEDURE);
  genie_check_initialisation (p, (BYTE_T *) & z, MOID (proc));
// Get arguments.
  if (SEQUENCE (p) == NO_NODE && !STATUS_TEST (p, SEQUENCE_MASK)) {
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
  if (PARTIAL_LOCALE (GINFO (proc)) != M_VOID && MOID (&z) != PARTIAL_LOCALE (GINFO (proc))) {
    ;
  } else if (STATUS (&z) & STANDENV_PROC_MASK) {
    if (UNIT (&GPROP (proc)) == genie_identifier_standenv_proc) {
      UNIT (&self) = genie_call_standenv_quick;
    }
  }
  return self;
}

//! @brief Construct a descriptor "ref_new" for a trim of "ref_old".

static void genie_trimmer (NODE_T * p, BYTE_T * *ref_new, BYTE_T * *ref_old, INT_T * offset)
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
      (*ref_old) += sizeof (A68_TUPLE);
    } else if (IS (p, TRIMMER)) {
      A68_INT k;
      NODE_T *q;
      INT_T L, U, D;
      A68_TUPLE *old_tup = (A68_TUPLE *) * ref_old;
      A68_TUPLE *new_tup = (A68_TUPLE *) * ref_new;
// TRIMMER is (l:u@r) with all units optional or (empty).
      q = SUB (p);
      if (q == NO_NODE) {
        L = LWB (old_tup);
        U = UPB (old_tup);
        D = 0;
      } else {
        BOOL_T absent = A68_TRUE;
// Lower index.
        if (q != NO_NODE && IS (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_OBJECT (p, &k, A68_INT);
          if (VALUE (&k) < LWB (old_tup)) {
            diagnostic (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
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
// Upper index.
        if (q != NO_NODE && IS (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_OBJECT (p, &k, A68_INT);
          if (VALUE (&k) > UPB (old_tup)) {
            diagnostic (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
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
// Revised lower bound.
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
      UPB (new_tup) = U - D;    // (L - D) + (U - L)
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

//! @brief Calculation of subscript.

void genie_subscript (NODE_T * p, A68_TUPLE ** tup, INT_T * sum, NODE_T ** seq)
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

//! @brief Slice REF [] A to REF A.

static PROP_T genie_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *pr = SUB (p);
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_ARRAY *a;
  ADDR_T pop_sp, scope;
  A68_TUPLE *t;
  INT_T sindex;
// Get row and save row from garbage collector.
  EXECUTE_UNIT (pr);
  CHECK_REF (p, *z, MOID (SUB (p)));
  GET_DESCRIPTOR (a, t, DEREF (A68_ROW, z));
  pop_sp = A68_SP;
  for (sindex = 0, q = SEQUENCE (p); q != NO_NODE; t++, q = SEQUENCE (q)) {
    A68_INT *j = (A68_INT *) STACK_TOP;
    INT_T k;
    EXECUTE_UNIT (q);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    sindex += (SPAN (t) * k - SHIFT (t));
    A68_SP = pop_sp;
  }
// Leave reference to element on the stack, preserving scope.
  scope = REF_SCOPE (z);
  *z = ARRAY (a);
  OFFSET (z) += ROW_ELEMENT (a, sindex);
  REF_SCOPE (z) = scope;
  return GPROP (p);
}

//! @brief Push slice of a rowed object.

static PROP_T genie_slice (NODE_T * p)
{
  PROP_T self, primary;
  ADDR_T pop_sp, scope = PRIMAL_SCOPE;
  BOOL_T slice_of_name = (BOOL_T) (IS_REF (MOID (SUB (p))));
  MOID_T *result_mode = slice_of_name ? SUB_MOID (p) : MOID (p);
  NODE_T *indexer = NEXT_SUB (p);
  UNIT (&self) = genie_slice;
  SOURCE (&self) = p;
  pop_sp = A68_SP;
// Get row.
  EXECUTE_UNIT_2 (SUB (p), primary);
// In case of slicing a REF [], we need the [] internally, so dereference.
  if (slice_of_name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  if (ANNOTATION (indexer) == SLICE) {
// SLICING subscripts one element from an array.
    A68_REF z;
    A68_ARRAY *a;
    A68_TUPLE *t;
    INT_T sindex;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    GET_DESCRIPTOR (a, t, &z);
    if (SEQUENCE (p) == NO_NODE && !STATUS_TEST (p, SEQUENCE_MASK)) {
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
        INT_T k;
        EXECUTE_UNIT (q);
        k = VALUE (j);
        if (k < LWB (t) || k > UPB (t)) {
          diagnostic (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
          exit_genie (q, A68_RUNTIME_ERROR);
        }
        sindex += (SPAN (t) * k - SHIFT (t));
      }
    }
// Slice of a name yields a name.
    A68_SP = pop_sp;
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
      PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, sindex)]), SIZE (result_mode));
      genie_check_initialisation (p, stack_top, result_mode);
    }
    return self;
  } else if (ANNOTATION (indexer) == TRIMMER) {
// Trimming selects a subarray from an array.
    INT_T offset;
    int dim;
    A68_REF z, ref_desc_copy;
    A68_ARRAY *old_des, *new_des;
    BYTE_T *ref_new, *ref_old;
    dim = DIM (DEFLEX (result_mode));
    ref_desc_copy = heap_generator (p, MOID (p), DESCRIPTOR_SIZE (dim));
// Get descriptor.
    POP_REF (p, &z);
// Get indexer.
    CHECK_REF (p, z, MOID (SUB (p)));
    old_des = DEREF (A68_ARRAY, &z);
    new_des = DEREF (A68_ARRAY, &ref_desc_copy);
    ref_old = ADDRESS (&z) + SIZE_ALIGNED (A68_ARRAY);
    ref_new = ADDRESS (&ref_desc_copy) + SIZE_ALIGNED (A68_ARRAY);
    DIM (new_des) = dim;
    MOID (new_des) = MOID (old_des);
    ELEM_SIZE (new_des) = ELEM_SIZE (old_des);
    offset = SLICE_OFFSET (old_des);
    genie_trimmer (indexer, &ref_new, &ref_old, &offset);
    SLICE_OFFSET (new_des) = offset;
    FIELD_OFFSET (new_des) = FIELD_OFFSET (old_des);
    ARRAY (new_des) = ARRAY (old_des);
// Trim of a name is a name.
    if (slice_of_name) {
      A68_REF ref_new2 = heap_generator (p, MOID (p), A68_REF_SIZE);
      *DEREF (A68_REF, &ref_new2) = ref_desc_copy;
      REF_SCOPE (&ref_new2) = scope;
      PUSH_REF (p, ref_new2);
    } else {
      PUSH_REF (p, ref_desc_copy);
    }
    return self;
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    return self;
  }
  (void) primary;
}

//! @brief Push value of denotation.

static PROP_T genie_denotation (NODE_T * p)
{
  MOID_T *moid = MOID (p);
  PROP_T self;
  UNIT (&self) = genie_denotation;
  SOURCE (&self) = p;
  if (moid == M_INT) {
// INT denotation.
    A68_INT z;
    NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    UNIT (&self) = genie_constant;
    STATUS (&z) = INIT_MASK;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) SIZE (M_INT));
    SIZE (GINFO (p)) = SIZE (M_INT);
    COPY (CONSTANT (GINFO (p)), &z, SIZE (M_INT));
    PUSH_VALUE (p, VALUE ((A68_INT *) (CONSTANT (GINFO (p)))), A68_INT);
    return self;
  }
  if (moid == M_REAL) {
// REAL denotation.
    A68_REAL z;
    NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    STATUS (&z) = INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) SIZE_ALIGNED (A68_REAL));
    SIZE (GINFO (p)) = SIZE_ALIGNED (A68_REAL);
    COPY (CONSTANT (GINFO (p)), &z, SIZE_ALIGNED (A68_REAL));
    PUSH_VALUE (p, VALUE ((A68_REAL *) (CONSTANT (GINFO (p)))), A68_REAL);
    return self;
  }
#if (A68_LEVEL >= 3)
  if (moid == M_LONG_INT) {
// LONG INT denotation.
    A68_LONG_INT z;
    size_t len = (size_t) SIZE_ALIGNED (A68_LONG_INT);
    NODE_T *s = IS (SUB (p), LONGETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    UNIT (&self) = genie_constant;
    STATUS (&z) = INIT_MASK;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) len);
    SIZE (GINFO (p)) = len;
    COPY (CONSTANT (GINFO (p)), &z, len);
    PUSH_VALUE (p, VALUE ((A68_LONG_INT *) (CONSTANT (GINFO (p)))), A68_LONG_INT);
    return self;
  }
  if (moid == M_LONG_REAL) {
// LONG REAL denotation.
    A68_LONG_REAL z;
    NODE_T *s = IS (SUB (p), LONGETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    STATUS (&z) = INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) SIZE_ALIGNED (A68_LONG_REAL));
    SIZE (GINFO (p)) = SIZE_ALIGNED (A68_LONG_REAL);
    COPY (CONSTANT (GINFO (p)), &z, SIZE_ALIGNED (A68_LONG_REAL));
    PUSH_VALUE (p, VALUE ((A68_LONG_REAL *) (CONSTANT (GINFO (p)))), A68_LONG_REAL);
    return self;
  }
// LONG BITS denotation.
  if (moid == M_LONG_BITS) {
    A68_LONG_BITS z;
    NODE_T *s = IS (SUB (p), LONGETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    UNIT (&self) = genie_constant;
    STATUS (&z) = INIT_MASK;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) SIZE_ALIGNED (A68_LONG_BITS));
    SIZE (GINFO (p)) = SIZE_ALIGNED (A68_LONG_BITS);
    COPY (CONSTANT (GINFO (p)), &z, SIZE_ALIGNED (A68_LONG_BITS));
    PUSH_VALUE (p, VALUE ((A68_LONG_BITS *) (CONSTANT (GINFO (p)))), A68_LONG_BITS);
    return self;
  }
#endif
  if (moid == M_LONG_INT || moid == M_LONG_LONG_INT) {
// [LONG] LONG INT denotation.
    int digits = DIGITS (moid);
    int size = SIZE (moid);
    NODE_T *number;
    if (IS (SUB (p), SHORTETY) || IS (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    MP_T *z = nil_mp (p, digits);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    MP_STATUS (z) = (MP_T) INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), z, size);
    return self;
  }
  if (moid == M_LONG_REAL || moid == M_LONG_LONG_REAL) {
// [LONG] LONG REAL denotation.
    int digits = DIGITS (moid);
    int size = SIZE (moid);
    NODE_T *number;
    if (IS (SUB (p), SHORTETY) || IS (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    MP_T *z = nil_mp (p, digits);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    MP_STATUS (z) = (MP_T) INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), z, size);
    return self;
  }
  if (moid == M_BITS) {
// BITS denotation.
    A68_BITS z;
    NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    UNIT (&self) = genie_constant;
    STATUS (&z) = INIT_MASK;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) SIZE_ALIGNED (A68_BITS));
    SIZE (GINFO (p)) = SIZE_ALIGNED (A68_BITS);
    COPY (CONSTANT (GINFO (p)), &z, SIZE_ALIGNED (A68_BITS));
    PUSH_VALUE (p, VALUE ((A68_BITS *) (CONSTANT (GINFO (p)))), A68_BITS);
  }
  if (moid == M_LONG_BITS || moid == M_LONG_LONG_BITS) {
// [LONG] LONG BITS denotation.
    int digits = DIGITS (moid);
    int size = SIZE (moid);
    NODE_T *number;
    if (IS (SUB (p), SHORTETY) || IS (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    MP_T *z = nil_mp (p, digits);
    if (genie_string_to_value_internal (p, moid, NSYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    MP_STATUS (z) = (MP_T) INIT_MASK;
    UNIT (&self) = genie_constant;
    CONSTANT (GINFO (p)) = (void *) get_heap_space ((size_t) size);
    SIZE (GINFO (p)) = size;
    COPY (CONSTANT (GINFO (p)), z, size);
    return self;
  }
  if (moid == M_BOOL) {
// BOOL denotation.
    A68_BOOL z;
    ASSERT (genie_string_to_value_internal (p, M_BOOL, NSYMBOL (p), (BYTE_T *) & z) == A68_TRUE);
    PUSH_VALUE (p, VALUE (&z), A68_BOOL);
    return self;
  } else if (moid == M_CHAR) {
// CHAR denotation.
    PUSH_VALUE (p, TO_UCHAR (NSYMBOL (p)[0]), A68_CHAR);
    return self;
  } else if (moid == M_ROW_CHAR) {
// [] CHAR denotation - permanent string in the heap.
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
    (void) tup;
    return self;
  }
  if (moid == M_VOID) {
// VOID denotation: EMPTY.
    return self;
  }
// ?.
  return self;
}

//! @brief Push a local identifier.

static PROP_T genie_frame_identifier (NODE_T * p)
{
  BYTE_T *z;
  FRAME_GET (z, BYTE_T, p);
  PUSH (p, z, SIZE (MOID (p)));
  return GPROP (p);
}

//! @brief Push standard environ routine as PROC.

static PROP_T genie_identifier_standenv_proc (NODE_T * p)
{
  A68_PROCEDURE z;
  TAG_T *q = TAX (p);
  STATUS (&z) = (STATUS_MASK_T) (INIT_MASK | STANDENV_PROC_MASK);
  PROCEDURE (&(BODY (&z))) = PROCEDURE (q);
  ENVIRON (&z) = 0;
  LOCALE (&z) = NO_HANDLE;
  MOID (&z) = MOID (p);
  PUSH_PROCEDURE (p, z);
  return GPROP (p);
}

//! @brief (optimised) push identifier from standard environ

static PROP_T genie_identifier_standenv (NODE_T * p)
{
  (void) ((*(PROCEDURE (TAX (p)))) (p));
  return GPROP (p);
}

//! @brief Push identifier onto the stack.

static PROP_T genie_identifier (NODE_T * p)
{
  static PROP_T self;
  TAG_T *q = TAX (p);
  SOURCE (&self) = p;
  if (A68_STANDENV_PROC (q)) {
    if (IS (MOID (q), PROC_SYMBOL)) {
      (void) genie_identifier_standenv_proc (p);
      UNIT (&self) = genie_identifier_standenv_proc;
    } else {
      (void) genie_identifier_standenv (p);
      UNIT (&self) = genie_identifier_standenv;
    }
  } else if (STATUS_TEST (q, CONSTANT_MASK)) {
    int size = SIZE (MOID (p));
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
  return self;
}

//! @brief Push result of cast (coercions are deeper in the tree).

static PROP_T genie_cast (NODE_T * p)
{
  PROP_T self;
  EXECUTE_UNIT (NEXT_SUB (p));
  UNIT (&self) = genie_cast;
  SOURCE (&self) = p;
  return self;
}

//! @brief Execute assertion.

static PROP_T genie_assertion (NODE_T * p)
{
  PROP_T self;
  if (STATUS_TEST (p, ASSERT_MASK)) {
    A68_BOOL z;
    EXECUTE_UNIT (NEXT_SUB (p));
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_FALSE_ASSERTION);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  UNIT (&self) = genie_assertion;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push format text.

static PROP_T genie_format_text (NODE_T * p)
{
  static PROP_T self;
  A68_FORMAT z = *(A68_FORMAT *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_FORMAT (p, z);
  UNIT (&self) = genie_format_text;
  SOURCE (&self) = p;
  return self;
}

//! @brief SELECTION from a value

static PROP_T genie_selection_value_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *result_mode = MOID (selector);
  ADDR_T pop_sp = A68_SP;
  int size = SIZE (result_mode);
  INT_T offset = OFFSET (NODE_PACK (SUB (selector)));
  EXECUTE_UNIT (NEXT (selector));
  A68_SP = pop_sp;
  if (offset > 0) {
    MOVE (STACK_TOP, STACK_OFFSET (offset), (unsigned) size);
    genie_check_initialisation (p, STACK_TOP, result_mode);
  }
  INCREMENT_STACK_POINTER (selector, size);
  return GPROP (p);
}

//! @brief SELECTION from a name

static PROP_T genie_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  return GPROP (p);
}

//! @brief Push selection from secondary.

static PROP_T genie_selection (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  PROP_T self;
  MOID_T *struct_mode = MOID (NEXT (selector)), *result_mode = MOID (selector);
  BOOL_T selection_of_name = (BOOL_T) (IS_REF (struct_mode));
  SOURCE (&self) = p;
  UNIT (&self) = genie_selection;
  EXECUTE_UNIT (NEXT (selector));
// Multiple selections.
  if (selection_of_name && (IS_FLEX (SUB (struct_mode)) || IS_ROW (SUB (struct_mode)))) {
    A68_REF *row1, row2, row3;
    int dims, desc_size;
    POP_ADDRESS (selector, row1, A68_REF);
    CHECK_REF (p, *row1, struct_mode);
    row1 = DEREF (A68_REF, row1);
    dims = DIM (DEFLEX (SUB (struct_mode)));
    desc_size = DESCRIPTOR_SIZE (dims);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), DEREF (BYTE_T, row1), (unsigned) desc_size);
    MOID ((DEREF (A68_ARRAY, &row2))) = SUB_SUB (result_mode);
    FIELD_OFFSET (DEREF (A68_ARRAY, &row2)) += OFFSET (NODE_PACK (SUB (selector)));
    row3 = heap_generator (selector, result_mode, A68_REF_SIZE);
    *DEREF (A68_REF, &row3) = row2;
    PUSH_REF (selector, row3);
    UNIT (&self) = genie_selection;
  } else if (struct_mode != NO_MOID && (IS_FLEX (struct_mode) || IS_ROW (struct_mode))) {
    A68_REF *row1, row2;
    int dims, desc_size;
    POP_ADDRESS (selector, row1, A68_REF);
    dims = DIM (DEFLEX (struct_mode));
    desc_size = DESCRIPTOR_SIZE (dims);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), DEREF (BYTE_T, row1), (unsigned) desc_size);
    MOID ((DEREF (A68_ARRAY, &row2))) = SUB (result_mode);
    FIELD_OFFSET (DEREF (A68_ARRAY, &row2)) += OFFSET (NODE_PACK (SUB (selector)));
    PUSH_REF (selector, row2);
    UNIT (&self) = genie_selection;
  }
// Normal selections.
  else if (selection_of_name && IS_STRUCT (SUB (struct_mode))) {
    A68_REF *z = (A68_REF *) (STACK_OFFSET (-A68_REF_SIZE));
    CHECK_REF (selector, *z, struct_mode);
    OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
    UNIT (&self) = genie_selection_name_quick;
  } else if (IS_STRUCT (struct_mode)) {
    DECREMENT_STACK_POINTER (selector, SIZE (struct_mode));
    MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (SUB (selector)))), (unsigned) SIZE (result_mode));
    genie_check_initialisation (p, STACK_TOP, result_mode);
    INCREMENT_STACK_POINTER (selector, SIZE (result_mode));
    UNIT (&self) = genie_selection_value_quick;
  }
  return self;
}

//! @brief Push selection from primary.

static PROP_T genie_field_selection (NODE_T * p)
{
  PROP_T self;
  ADDR_T pop_sp = A68_SP, pop_fp = A68_FP;
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
      if (IS_REF (m) && ISNT (SUB (m), STRUCT_SYMBOL)) {
        int size = SIZE (SUB (m));
        A68_SP = pop_sp;
        CHECK_REF (p, *z, m);
        PUSH (p, ADDRESS (z), size);
        genie_check_initialisation (p, STACK_OFFSET (-size), MOID (p));
        m = SUB (m);
      } else if (IS (m, PROC_SYMBOL)) {
        genie_check_initialisation (p, (BYTE_T *) w, m);
        genie_call_procedure (p, m, m, M_VOID, w, pop_sp, pop_fp);
        STACK_DNS (p, MOID (p), A68_FP);
        m = SUB (m);
      } else {
        coerce = A68_FALSE;
      }
    }
    if (IS_REF (m) && IS (SUB (m), STRUCT_SYMBOL)) {
      CHECK_REF (p, *z, m);
      OFFSET (z) += OFFSET (NODE_PACK (p));
    } else if (IS_STRUCT (m)) {
      A68_SP = pop_sp;
      MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (p))), (unsigned) SIZE (result_mode));
      INCREMENT_STACK_POINTER (p, SIZE (result_mode));
    }
  }
  return self;
}

//! @brief Call operator.

void genie_call_operator (NODE_T * p, ADDR_T pop_sp)
{
  A68_PROCEDURE *z;
  ADDR_T pop_fp = A68_FP;
  MOID_T *pr_mode = MOID (TAX (p));
  FRAME_GET (z, A68_PROCEDURE, p);
  genie_call_procedure (p, pr_mode, MOID (z), pr_mode, z, pop_sp, pop_fp);
  STACK_DNS (p, SUB (pr_mode), A68_FP);
}

//! @brief Push result of monadic formula OP "u".

static PROP_T genie_monadic (NODE_T * p)
{
  NODE_T *op = SUB (p);
  NODE_T *u = NEXT (op);
  PROP_T self;
  ADDR_T sp = A68_SP;
  EXECUTE_UNIT (u);
  STACK_DNS (u, MOID (u), A68_FP);
  if (PROCEDURE (TAX (op)) != NO_GPROC) {
    (void) ((*(PROCEDURE (TAX (op)))) (op));
  } else {
    genie_call_operator (op, sp);
  }
  UNIT (&self) = genie_monadic;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push result of formula.

static PROP_T genie_dyadic_quick (NODE_T * p)
{
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  NODE_T *v = NEXT (op);
  EXECUTE_UNIT (u);
  STACK_DNS (u, MOID (u), A68_FP);
  EXECUTE_UNIT (v);
  STACK_DNS (v, MOID (v), A68_FP);
  (void) ((*(PROCEDURE (TAX (op)))) (op));
  return GPROP (p);
}

//! @brief Push result of formula.

static PROP_T genie_dyadic (NODE_T * p)
{
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  NODE_T *v = NEXT (op);
  ADDR_T pop_sp = A68_SP;
  EXECUTE_UNIT (u);
  STACK_DNS (u, MOID (u), A68_FP);
  EXECUTE_UNIT (v);
  STACK_DNS (v, MOID (v), A68_FP);
  if (PROCEDURE (TAX (op)) != NO_GPROC) {
    (void) ((*(PROCEDURE (TAX (op)))) (op));
  } else {
    genie_call_operator (op, pop_sp);
  }
  return GPROP (p);
}

//! @brief Push result of formula.

static PROP_T genie_formula (NODE_T * p)
{
  PROP_T self, lhs, rhs;
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  ADDR_T pop_sp = A68_SP;
  UNIT (&self) = genie_formula;
  SOURCE (&self) = p;
  EXECUTE_UNIT_2 (u, lhs);
  STACK_DNS (u, MOID (u), A68_FP);
  if (op != NO_NODE) {
    NODE_T *v = NEXT (op);
    GPROC *proc = PROCEDURE (TAX (op));
    EXECUTE_UNIT_2 (v, rhs);
    STACK_DNS (v, MOID (v), A68_FP);
    UNIT (&self) = genie_dyadic;
    if (proc != NO_GPROC) {
      (void) ((*(proc)) (op));
      UNIT (&self) = genie_dyadic_quick;
    } else {
      genie_call_operator (op, pop_sp);
    }
    return self;
  } else if (UNIT (&lhs) == genie_monadic) {
    return lhs;
  }
  (void) rhs;
  return self;
}

//! @brief Push NIL.

static PROP_T genie_nihil (NODE_T * p)
{
  PROP_T self;
  PUSH_REF (p, nil_ref);
  UNIT (&self) = genie_nihil;
  SOURCE (&self) = p;
  return self;
}

//! @brief Assign a value to a name and voiden.

static PROP_T genie_voiding_assignation_constant (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = SOURCE (&PROP (GINFO (NEXT_NEXT (dst))));
  ADDR_T pop_sp = A68_SP;
  A68_REF *z = (A68_REF *) STACK_TOP;
  PROP_T self;
  UNIT (&self) = genie_voiding_assignation_constant;
  SOURCE (&self) = p;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  COPY (ADDRESS (z), CONSTANT (GINFO (src)), SIZE (GINFO (src)));
  A68_SP = pop_sp;
  return self;
}

//! @brief Assign a value to a name and voiden.

static PROP_T genie_voiding_assignation (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (dst);
  ADDR_T pop_sp = A68_SP, pop_fp = FRAME_DNS (A68_FP);
  A68_REF z;
  PROP_T self;
  UNIT (&self) = genie_voiding_assignation;
  SOURCE (&self) = p;
  EXECUTE_UNIT (dst);
  POP_OBJECT (p, &z, A68_REF);
  CHECK_REF (p, z, MOID (p));
  FRAME_DNS (A68_FP) = REF_SCOPE (&z);
  EXECUTE_UNIT (src);
  STACK_DNS (src, src_mode, REF_SCOPE (&z));
  FRAME_DNS (A68_FP) = pop_fp;
  A68_SP = pop_sp;
  if (HAS_ROWS (src_mode)) {
    genie_clone_stack (p, src_mode, &z, &z);
  } else {
    COPY_ALIGNED (ADDRESS (&z), STACK_TOP, SIZE (src_mode));
  }
  return self;
}

//! @brief Assign a value to a name and push the name.

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
  return self;
}

//! @brief Assign a value to a name and push the name.

static PROP_T genie_assignation_quick (NODE_T * p)
{
  PROP_T self;
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (dst);
  int size = SIZE (src_mode);
  ADDR_T pop_fp = FRAME_DNS (A68_FP);
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DNS (A68_FP) = REF_SCOPE (z);
  EXECUTE_UNIT (src);
  STACK_DNS (src, src_mode, REF_SCOPE (z));
  FRAME_DNS (A68_FP) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (HAS_ROWS (src_mode)) {
    genie_clone_stack (p, src_mode, z, z);
  } else {
    COPY (ADDRESS (z), STACK_TOP, size);
  }
  UNIT (&self) = genie_assignation_quick;
  SOURCE (&self) = p;
  return self;
}

//! @brief Assign a value to a name and push the name.

static PROP_T genie_assignation (NODE_T * p)
{
  PROP_T self, srp;
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (dst);
  int size = SIZE (src_mode);
  ADDR_T pop_fp = FRAME_DNS (A68_FP);
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DNS (A68_FP) = REF_SCOPE (z);
  EXECUTE_UNIT_2 (src, srp);
  STACK_DNS (src, src_mode, REF_SCOPE (z));
  FRAME_DNS (A68_FP) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (HAS_ROWS (src_mode)) {
    genie_clone_stack (p, src_mode, z, z);
    UNIT (&self) = genie_assignation;
  } else {
    COPY (ADDRESS (z), STACK_TOP, size);
    if (UNIT (&srp) == genie_constant) {
      UNIT (&self) = genie_assignation_constant;
    } else {
      UNIT (&self) = genie_assignation_quick;
    }
  }
  SOURCE (&self) = p;
  return self;
}

//! @brief Push equality of two REFs.

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
    PUSH_VALUE (p, (BOOL_T) (ADDRESS (&x) == ADDRESS (&y)), A68_BOOL);
  } else {
    PUSH_VALUE (p, (BOOL_T) (ADDRESS (&x) != ADDRESS (&y)), A68_BOOL);
  }
  return self;
}

//! @brief Push result of ANDF.

static PROP_T genie_and_function (NODE_T * p)
{
  PROP_T self;
  A68_BOOL x;
  EXECUTE_UNIT (SUB (p));
  POP_OBJECT (p, &x, A68_BOOL);
  if (VALUE (&x) == A68_TRUE) {
    EXECUTE_UNIT (NEXT_NEXT (SUB (p)));
  } else {
    PUSH_VALUE (p, A68_FALSE, A68_BOOL);
  }
  UNIT (&self) = genie_and_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push result of ORF.

static PROP_T genie_or_function (NODE_T * p)
{
  PROP_T self;
  A68_BOOL x;
  EXECUTE_UNIT (SUB (p));
  POP_OBJECT (p, &x, A68_BOOL);
  if (VALUE (&x) == A68_FALSE) {
    EXECUTE_UNIT (NEXT_NEXT (SUB (p)));
  } else {
    PUSH_VALUE (p, A68_TRUE, A68_BOOL);
  }
  UNIT (&self) = genie_or_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push routine text.

static PROP_T genie_routine_text (NODE_T * p)
{
  static PROP_T self;
  A68_PROCEDURE z = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_PROCEDURE (p, z);
  UNIT (&self) = genie_routine_text;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push an undefined value of the required mode.

void genie_push_undefined (NODE_T * p, MOID_T * u)
{
// For primitive modes we push an initialised value.
  if (u == M_VOID) {
    ;
  } else if (u == M_INT) {
    PUSH_VALUE (p, 1, A68_INT); // Because users write [~] INT !
  } else if (u == M_REAL) {
    PUSH_VALUE (p, (unif_rand ()), A68_REAL);
  } else if (u == M_BOOL) {
    PUSH_VALUE (p, (BOOL_T) (unif_rand () < 0.5), A68_BOOL);
  } else if (u == M_CHAR) {
    PUSH_VALUE (p, (char) (32 + 96 * unif_rand ()), A68_CHAR);
  } else if (u == M_BITS) {
    PUSH_VALUE (p, (UNSIGNED_T) (unif_rand () * A68_MAX_BITS), A68_BITS);
  } else if (u == M_COMPLEX) {
    PUSH_COMPLEX (p, unif_rand (), unif_rand ());
  } else if (u == M_BYTES) {
    PUSH_BYTES (p, "SKIP");
  } else if (u == M_LONG_BYTES) {
    PUSH_LONG_BYTES (p, "SKIP");
  } else if (u == M_STRING) {
    PUSH_REF (p, empty_string (p));
  } else if (u == M_LONG_INT) {
#if (A68_LEVEL >= 3)
    QUAD_WORD_T w;
    set_lw (w, 1);
    PUSH_VALUE (p, w, A68_LONG_INT);    // Because users write [~] INT !
#else
    (void) nil_mp (p, DIGITS (u));
#endif
  } else if (u == M_LONG_REAL) {
#if (A68_LEVEL >= 3)
    genie_next_random_real_16 (p);
#else
    (void) nil_mp (p, DIGITS (u));
#endif
  } else if (u == M_LONG_BITS) {
#if (A68_LEVEL >= 3)
    QUAD_WORD_T w;
    set_lw (w, 1);
    PUSH_VALUE (p, w, A68_LONG_BITS);   // Because users write [~] INT !
#else
    (void) nil_mp (p, DIGITS (u));
#endif
  } else if (u == M_LONG_LONG_INT) {
    (void) nil_mp (p, DIGITS (u));
  } else if (u == M_LONG_LONG_REAL) {
    (void) nil_mp (p, DIGITS (u));
  } else if (u == M_LONG_LONG_BITS) {
    (void) nil_mp (p, DIGITS (u));
  } else if (u == M_LONG_COMPLEX) {
#if (A68_LEVEL >= 3)
    genie_next_random_real_16 (p);
    genie_next_random_real_16 (p);
#else
    (void) nil_mp (p, DIGITSC (u));
    (void) nil_mp (p, DIGITSC (u));
#endif
  } else if (u == M_LONG_LONG_COMPLEX) {
    (void) nil_mp (p, DIGITSC (u));
    (void) nil_mp (p, DIGITSC (u));
  } else if (IS_REF (u)) {
// All REFs are NIL.
    PUSH_REF (p, nil_ref);
  } else if (IS_ROW (u) || IS_FLEX (u)) {
// [] AMODE or FLEX [] AMODE.
    A68_REF er = empty_row (p, u);
    STATUS (&er) |= SKIP_ROW_MASK;
    PUSH_REF (p, er);
  } else if (IS_STRUCT (u)) {
// STRUCT.
    PACK_T *v;
    for (v = PACK (u); v != NO_PACK; FORWARD (v)) {
      genie_push_undefined (p, MOID (v));
    }
  } else if (IS_UNION (u)) {
// UNION.
    ADDR_T sp = A68_SP;
    PUSH_UNION (p, MOID (PACK (u)));
    genie_push_undefined (p, MOID (PACK (u)));
    A68_SP = sp + SIZE (u);
  } else if (IS (u, PROC_SYMBOL)) {
// PROC.
    A68_PROCEDURE z;
    STATUS (&z) = (STATUS_MASK_T) (INIT_MASK | SKIP_PROCEDURE_MASK);
    (NODE (&BODY (&z))) = NO_NODE;
    ENVIRON (&z) = 0;
    LOCALE (&z) = NO_HANDLE;
    MOID (&z) = u;
    PUSH_PROCEDURE (p, z);
  } else if (u == M_FORMAT) {
// FORMAT etc. - what arbitrary FORMAT could mean anything at all?.
    A68_FORMAT z;
    STATUS (&z) = (STATUS_MASK_T) (INIT_MASK | SKIP_FORMAT_MASK);
    BODY (&z) = NO_NODE;
    ENVIRON (&z) = 0;
    PUSH_FORMAT (p, z);
  } else if (u == M_SIMPLOUT) {
    ADDR_T sp = A68_SP;
    PUSH_UNION (p, M_STRING);
    PUSH_REF (p, c_to_a_string (p, "SKIP", DEFAULT_WIDTH));
    A68_SP = sp + SIZE (u);
  } else if (u == M_SIMPLIN) {
    ADDR_T sp = A68_SP;
    PUSH_UNION (p, M_REF_STRING);
    genie_push_undefined (p, M_REF_STRING);
    A68_SP = sp + SIZE (u);
  } else if (u == M_REF_FILE) {
    PUSH_REF (p, A68 (skip_file));
  } else if (u == M_FILE) {
    A68_REF *z = (A68_REF *) STACK_TOP;
    int size = SIZE (M_FILE);
    ADDR_T pop_sp = A68_SP;
    PUSH_REF (p, A68 (skip_file));
    A68_SP = pop_sp;
    PUSH (p, ADDRESS (z), size);
  } else if (u == M_CHANNEL) {
    PUSH_OBJECT (p, A68 (skip_channel), A68_CHANNEL);
  } else if (u == M_PIPE) {
    genie_push_undefined (p, M_REF_FILE);
    genie_push_undefined (p, M_REF_FILE);
    genie_push_undefined (p, M_INT);
  } else if (u == M_SOUND) {
    A68_SOUND *z = (A68_SOUND *) STACK_TOP;
    int size = SIZE (M_SOUND);
    INCREMENT_STACK_POINTER (p, size);
    FILL (z, 0, size);
    STATUS (z) = INIT_MASK;
  } else {
    BYTE_T *_sp_ = STACK_TOP;
    int size = SIZE_ALIGNED (u);
    INCREMENT_STACK_POINTER (p, size);
    FILL (_sp_, 0, size);
  }
}

//! @brief Push an undefined value of the required mode.

static PROP_T genie_skip (NODE_T * p)
{
  PROP_T self;
  if (MOID (p) != M_VOID) {
    genie_push_undefined (p, MOID (p));
  }
  UNIT (&self) = genie_skip;
  SOURCE (&self) = p;
  return self;
}

//! @brief Jump to the serial clause where the label is at.

static void genie_jump (NODE_T * p)
{
// Stack pointer and frame pointer were saved at target serial clause.
  NODE_T *jump = SUB (p);
  NODE_T *label = (IS (jump, GOTO_SYMBOL)) ? NEXT (jump) : jump;
  ADDR_T target_frame_pointer = A68_FP;
  jmp_buf *jump_stat = NO_JMP_BUF;
// Find the stack frame this jump points to.
  BOOL_T found = A68_FALSE;
  while (target_frame_pointer > 0 && !found) {
    found = (BOOL_T) ((TAG_TABLE (TAX (label)) == TABLE (FRAME_TREE (target_frame_pointer))) && FRAME_JUMP_STAT (target_frame_pointer) != NO_JMP_BUF);
    if (!found) {
      target_frame_pointer = FRAME_STATIC_LINK (target_frame_pointer);
    }
  }
// Beam us up, Scotty!.
#if defined (BUILD_PARALLEL_CLAUSE)
  {
    pthread_t target_id = FRAME_THREAD_ID (target_frame_pointer);
    if (SAME_THREAD (target_id, pthread_self ())) {
      jump_stat = FRAME_JUMP_STAT (target_frame_pointer);
      JUMP_TO (TAG_TABLE (TAX (label))) = UNIT (TAX (label));
      longjmp (*(jump_stat), 1);
    } else if (SAME_THREAD (target_id, A68_PAR (main_thread_id))) {
// A jump out of all parallel clauses back into the main program.
      genie_abend_all_threads (p, FRAME_JUMP_STAT (target_frame_pointer), label);
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    } else {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_JUMP);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
#else
  jump_stat = FRAME_JUMP_STAT (target_frame_pointer);
  JUMP_TO (TAG_TABLE (TAX (label))) = UNIT (TAX (label));
  longjmp (*(jump_stat), 1);
#endif
}

//! @brief Execute a unit, tertiary, secondary or primary.

static PROP_T genie_unit (NODE_T * p)
{
  if (IS_COERCION (GINFO (p))) {
    GLOBAL_PROP (&A68_JOB) = genie_coercion (p);
  } else {
    switch (ATTRIBUTE (p)) {
    case DECLARATION_LIST:
      {
        genie_declaration (SUB (p));
        UNIT (&GLOBAL_PROP (&A68_JOB)) = genie_unit;
        SOURCE (&GLOBAL_PROP (&A68_JOB)) = p;
        break;
      }
    case UNIT:
      {
        EXECUTE_UNIT_2 (SUB (p), GLOBAL_PROP (&A68_JOB));
        break;
      }
    case TERTIARY:
    case SECONDARY:
    case PRIMARY:
      {
        GLOBAL_PROP (&A68_JOB) = genie_unit (SUB (p));
        break;
      }
// Ex primary.
    case ENCLOSED_CLAUSE:
      {
        GLOBAL_PROP (&A68_JOB) = genie_enclosed ((volatile NODE_T *) p);
        break;
      }
    case IDENTIFIER:
      {
        GLOBAL_PROP (&A68_JOB) = genie_identifier (p);
        break;
      }
    case CALL:
      {
        GLOBAL_PROP (&A68_JOB) = genie_call (p);
        break;
      }
    case SLICE:
      {
        GLOBAL_PROP (&A68_JOB) = genie_slice (p);
        break;
      }
    case DENOTATION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_denotation (p);
        break;
      }
    case CAST:
      {
        GLOBAL_PROP (&A68_JOB) = genie_cast (p);
        break;
      }
    case FORMAT_TEXT:
      {
        GLOBAL_PROP (&A68_JOB) = genie_format_text (p);
        break;
      }
// Ex secondary.
    case GENERATOR:
      {
        GLOBAL_PROP (&A68_JOB) = genie_generator (p);
        break;
      }
    case SELECTION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_selection (p);
        break;
      }
// Ex tertiary.
    case FORMULA:
      {
        GLOBAL_PROP (&A68_JOB) = genie_formula (p);
        break;
      }
    case MONADIC_FORMULA:
      {
        GLOBAL_PROP (&A68_JOB) = genie_monadic (p);
        break;
      }
    case NIHIL:
      {
        GLOBAL_PROP (&A68_JOB) = genie_nihil (p);
        break;
      }
    case DIAGONAL_FUNCTION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_diagonal_function (p);
        break;
      }
    case TRANSPOSE_FUNCTION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_transpose_function (p);
        break;
      }
    case ROW_FUNCTION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_row_function (p);
        break;
      }
    case COLUMN_FUNCTION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_column_function (p);
        break;
      }
// Ex unit.
    case ASSIGNATION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_assignation (p);
        break;
      }
    case IDENTITY_RELATION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_identity_relation (p);
        break;
      }
    case ROUTINE_TEXT:
      {
        GLOBAL_PROP (&A68_JOB) = genie_routine_text (p);
        break;
      }
    case SKIP:
      {
        GLOBAL_PROP (&A68_JOB) = genie_skip (p);
        break;
      }
    case JUMP:
      {
        UNIT (&GLOBAL_PROP (&A68_JOB)) = genie_unit;
        SOURCE (&GLOBAL_PROP (&A68_JOB)) = p;
        genie_jump (p);
        break;
      }
    case AND_FUNCTION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_and_function (p);
        break;
      }
    case OR_FUNCTION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_or_function (p);
        break;
      }
    case ASSERTION:
      {
        GLOBAL_PROP (&A68_JOB) = genie_assertion (p);
        break;
      }
    case CODE_CLAUSE:
      {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_CODE);
        exit_genie (p, A68_RUNTIME_ERROR);
        break;
      }
    }
  }
  return GPROP (p) = GLOBAL_PROP (&A68_JOB);
}

//! @brief Execution of serial clause without labels.

static void genie_serial_units_no_label (NODE_T * p, ADDR_T pop_sp, NODE_T ** seq)
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
// Voiden the expression stack.
        A68_SP = pop_sp;
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

//! @brief Execution of serial clause with labels.

void genie_serial_units (NODE_T * p, NODE_T ** jump_to, jmp_buf * exit_buf, ADDR_T pop_sp)
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
// If we dropped in this clause from a jump then this unit is the target.
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
// Voiden the expression stack.
          A68_SP = pop_sp;
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

//! @brief Execute serial clause.

void genie_serial_clause (NODE_T * p, jmp_buf * exit_buf)
{
  if (LABELS (TABLE (p)) == NO_TAG) {
// No labels in this clause.
    if (SEQUENCE (p) == NO_NODE && !STATUS_TEST (p, SEQUENCE_MASK)) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      GINFO_T g;
      GINFO (&top_seq) = &g;
      genie_serial_units_no_label (SUB (p), A68_SP, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      STATUS_SET (p, SEQUENCE_MASK);
      STATUS_SET (p, SERIAL_MASK);
      if (SEQUENCE (p) != NO_NODE && SEQUENCE (SEQUENCE (p)) == NO_NODE) {
        STATUS_SET (p, OPTIMAL_MASK);
      }
    } else {
// A linear list without labels.
      NODE_T *q;
      ADDR_T pop_sp = A68_SP;
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
            A68_SP = pop_sp;
            break;
          }
        }
      }
    }
  } else {
// Labels in this clause.
    jmp_buf jump_stat;
    ADDR_T pop_sp = A68_SP, pop_fp = A68_FP;
    ADDR_T pop_dns = FRAME_DNS (A68_FP);
    FRAME_JUMP_STAT (A68_FP) = &jump_stat;
    if (!setjmp (jump_stat)) {
      NODE_T *jump_to = NO_NODE;
      genie_serial_units (SUB (p), &jump_to, exit_buf, A68_SP);
    } else {
// HIjol! Restore state and look for indicated unit.
      NODE_T *jump_to = JUMP_TO (TABLE (p));
      A68_SP = pop_sp;
      A68_FP = pop_fp;
      FRAME_DNS (A68_FP) = pop_dns;
      genie_serial_units (SUB (p), &jump_to, exit_buf, A68_SP);
    }
  }
}

//! @brief Execute enquiry clause.

void genie_enquiry_clause (NODE_T * p)
{
  if (SEQUENCE (p) == NO_NODE && !STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GINFO_T g;
    GINFO (&top_seq) = &g;
    genie_serial_units_no_label (SUB (p), A68_SP, &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
    if (SEQUENCE (p) != NO_NODE && SEQUENCE (SEQUENCE (p)) == NO_NODE) {
      STATUS_SET (p, OPTIMAL_MASK);
    }
  } else {
// A linear list without labels (of course, it's an enquiry clause).
    NODE_T *q;
    ADDR_T pop_sp = A68_SP;
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
          A68_SP = pop_sp;
          break;
        }
      }
    }
  }
}

//! @brief Execute collateral units.

static void genie_collateral_units (NODE_T * p, int *count)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      EXECUTE_UNIT_TRACE (p);
      STACK_DNS (p, MOID (p), FRAME_DNS (A68_FP));
      (*count)++;
      return;
    } else {
      genie_collateral_units (SUB (p), count);
    }
  }
}

//! @brief Execute collateral clause.

static PROP_T genie_collateral (NODE_T * p)
{
  PROP_T self;
// VOID clause and STRUCT display.
  if (MOID (p) == M_VOID || IS_STRUCT (MOID (p))) {
    int count = 0;
    genie_collateral_units (SUB (p), &count);
  } else {
// Row display.
    A68_REF new_display;
    int count = 0;
    ADDR_T sp = A68_SP;
    MOID_T *m = MOID (p);
    genie_collateral_units (SUB (p), &count);
// [] AMODE vacuum.
    if (count == 0) {
      A68_SP = sp;
      INCREMENT_STACK_POINTER (p, A68_REF_SIZE);
      *(A68_REF *) STACK_ADDRESS (sp) = empty_row (p, m);
    } else if (DIM (DEFLEX (m)) == 1) {
// [] AMODE display.
      new_display = genie_make_row (p, SLICE (DEFLEX (m)), count, sp);
      A68_SP = sp;
      INCREMENT_STACK_POINTER (p, A68_REF_SIZE);
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    } else {
// [,,] AMODE display, we concatenate 1 + (n-1) to n dimensions.
      new_display = genie_make_rowrow (p, m, count, sp);
      A68_SP = sp;
      INCREMENT_STACK_POINTER (p, A68_REF_SIZE);
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    }
  }
  UNIT (&self) = genie_collateral;
  SOURCE (&self) = p;
  return self;
}

//! @brief Execute unit from integral-case in-part.

BOOL_T genie_int_case_unit (NODE_T * p, int k, int *count)
{
  if (p == NO_NODE) {
    return A68_FALSE;
  } else {
    if (IS (p, UNIT)) {
      if (k == *count) {
        EXECUTE_UNIT_TRACE (p);
        return A68_TRUE;
      } else {
        (*count)++;
        return A68_FALSE;
      }
    } else {
      if (genie_int_case_unit (SUB (p), k, count)) {
        return A68_TRUE;
      } else {
        return genie_int_case_unit (NEXT (p), k, count);
      }
    }
  }
}

//! @brief Execute unit from united-case in-part.

static BOOL_T genie_united_case_unit (NODE_T * p, MOID_T * m)
{
  if (p == NO_NODE) {
    return A68_FALSE;
  } else {
    if (IS (p, SPECIFIER)) {
      MOID_T *spec_moid = MOID (NEXT_SUB (p));
      BOOL_T equal_modes;
      if (m != NO_MOID) {
        if (IS_UNION (spec_moid)) {
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
          if (IS_UNION (spec_moid)) {
            COPY ((FRAME_OBJECT (OFFSET (TAX (q)))), STACK_TOP, SIZE (spec_moid));
          } else {
            COPY ((FRAME_OBJECT (OFFSET (TAX (q)))), STACK_OFFSET (A68_UNION_SIZE), SIZE (spec_moid));
          }
        }
        EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
        CLOSE_FRAME;
        return A68_TRUE;
      } else {
        return A68_FALSE;
      }
    } else {
      if (genie_united_case_unit (SUB (p), m)) {
        return A68_TRUE;
      } else {
        return genie_united_case_unit (NEXT (p), m);
      }
    }
  }
}

//! @brief Execute identity declaration.

void genie_identity_dec (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (ISNT (p, DEFINING_IDENTIFIER)) {
      genie_identity_dec (SUB (p));
    } else {
      A68_REF loc;
      NODE_T *src = NEXT_NEXT (p);
      MOID_T *src_mode = MOID (p);
      unsigned size = (unsigned) SIZE (src_mode);
      BYTE_T *stack_top = STACK_TOP;
      ADDR_T pop_sp = A68_SP;
      ADDR_T pop_dns = FRAME_DNS (A68_FP);
      FRAME_DNS (A68_FP) = A68_FP;
      EXECUTE_UNIT_TRACE (src);
      genie_check_initialisation (src, stack_top, src_mode);
      STACK_DNS (src, src_mode, A68_FP);
      FRAME_DNS (A68_FP) = pop_dns;
// Make a temporary REF to the object in the frame.
      STATUS (&loc) = (STATUS_MASK_T) (INIT_MASK | IN_FRAME_MASK);
      REF_HANDLE (&loc) = (A68_HANDLE *) & nil_handle;
      OFFSET (&loc) = A68_FP + FRAME_INFO_SIZE + OFFSET (TAX (p));
      REF_SCOPE (&loc) = A68_FP;
      ABEND (ADDRESS (&loc) != FRAME_OBJECT (OFFSET (TAX (p))), ERROR_INTERNAL_CONSISTENCY, __func__);
// Initialise the tag, value is in the stack.
      if (HAS_ROWS (src_mode)) {
        A68_SP = pop_sp;
        genie_clone_stack (p, src_mode, &loc, (A68_REF *) & nil_ref);
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

//! @brief Execute variable declaration.

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
        LEAP_T leap = (HEAP (tag) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
        A68_REF *z;
        MOID_T *src_mode = SUB_MOID (p);
        z = (A68_REF *) (FRAME_OBJECT (OFFSET (TAX (p))));
        genie_generator_internal (*declarer, ref_mode, BODY (tag), leap, sp);
        POP_REF (p, z);
        if (NEXT (p) != NO_NODE && IS (NEXT (p), ASSIGN_SYMBOL)) {
          NODE_T *src = NEXT_NEXT (p);
          ADDR_T pop_sp = A68_SP;
          ADDR_T pop_dns = FRAME_DNS (A68_FP);
          FRAME_DNS (A68_FP) = A68_FP;
          EXECUTE_UNIT_TRACE (src);
          STACK_DNS (src, src_mode, A68_FP);
          FRAME_DNS (A68_FP) = pop_dns;
          A68_SP = pop_sp;
          if (HAS_ROWS (src_mode)) {
            genie_clone_stack (p, src_mode, z, z);
          } else {
            MOVE (ADDRESS (z), STACK_TOP, (unsigned) SIZE (src_mode));
          }
        }
      }
    }
  }
}

//! @brief Execute PROC variable declaration.

void genie_proc_variable_dec (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_IDENTIFIER:
      {
        ADDR_T sp_for_voiding = A68_SP;
        MOID_T *ref_mode = MOID (p);
        TAG_T *tag = TAX (p);
        LEAP_T leap = (HEAP (tag) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
        A68_REF *z = (A68_REF *) (FRAME_OBJECT (OFFSET (TAX (p))));
        genie_generator_internal (p, ref_mode, BODY (tag), leap, A68_SP);
        POP_REF (p, z);
        if (NEXT (p) != NO_NODE && IS (NEXT (p), ASSIGN_SYMBOL)) {
          MOID_T *src_mode = SUB_MOID (p);
          ADDR_T pop_sp = A68_SP;
          ADDR_T pop_dns = FRAME_DNS (A68_FP);
          FRAME_DNS (A68_FP) = A68_FP;
          EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
          STACK_DNS (p, SUB (ref_mode), A68_FP);
          FRAME_DNS (A68_FP) = pop_dns;
          A68_SP = pop_sp;
          MOVE (ADDRESS (z), STACK_TOP, (unsigned) SIZE (src_mode));
        }
        A68_SP = sp_for_voiding;        // Voiding
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

//! @brief Execute operator declaration.

void genie_operator_dec (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_OPERATOR:
      {
        A68_PROCEDURE *z = (A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (p))));
        ADDR_T pop_dns = FRAME_DNS (A68_FP);
        FRAME_DNS (A68_FP) = A68_FP;
        EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
        STACK_DNS (p, MOID (p), A68_FP);
        FRAME_DNS (A68_FP) = pop_dns;
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

//! @brief Execute declaration.

void genie_declaration (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case MODE_DECLARATION:
    case PROCEDURE_DECLARATION:
    case BRIEF_OPERATOR_DECLARATION:
    case PRIORITY_DECLARATION:
      {
// Already resolved.
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
        ADDR_T pop_sp = A68_SP;
        genie_variable_dec (SUB (p), &declarer, A68_SP);
// Voiding to remove garbage from declarers.
        A68_SP = pop_sp;
        break;
      }
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        ADDR_T pop_sp = A68_SP;
        genie_proc_variable_dec (SUB (p));
        A68_SP = pop_sp;
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

#define LABEL_FREE(p) {\
  NODE_T *_m_q; ADDR_T pop_sp_lf = A68_SP;\
  for (_m_q = SEQUENCE (p); _m_q != NO_NODE; _m_q = SEQUENCE (_m_q)) {\
    if (IS (_m_q, UNIT) || IS (_m_q, DECLARATION_LIST)) {\
      EXECUTE_UNIT_TRACE (_m_q);\
    }\
    if (SEQUENCE (_m_q) != NO_NODE) {\
      A68_SP = pop_sp_lf;\
      _m_q = SEQUENCE (_m_q);\
    }\
  }}

#define SERIAL_CLAUSE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    PREEMPTIVE_GC (DEFAULT_PREEMPTIVE);\
    LABEL_FREE (p);\
  } else {\
    PREEMPTIVE_GC (DEFAULT_PREEMPTIVE);\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) p, (jmp_buf *) exit_buf);\
  }}

#define SERIAL_CLAUSE_TRACE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT_TRACE (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    PREEMPTIVE_GC (DEFAULT_PREEMPTIVE);\
    LABEL_FREE (p);\
  } else {\
    PREEMPTIVE_GC (DEFAULT_PREEMPTIVE);\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) p, (jmp_buf *) exit_buf);\
  }}

#define ENQUIRY_CLAUSE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    PREEMPTIVE_GC (DEFAULT_PREEMPTIVE);\
    LABEL_FREE (p);\
  } else {\
    PREEMPTIVE_GC (DEFAULT_PREEMPTIVE);\
    genie_enquiry_clause ((NODE_T *) p);\
  }

//! @brief Execute integral-case-clause.

static PROP_T genie_int_case (volatile NODE_T * p)
{
  volatile int unit_count;
  volatile BOOL_T found_unit;
  jmp_buf exit_buf;
  A68_INT k;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
// CASE or OUSE.
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  POP_OBJECT (q, &k, A68_INT);
// IN.
  FORWARD (q);
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  unit_count = 1;
  found_unit = genie_int_case_unit (NEXT_SUB ((NODE_T *) q), (int) VALUE (&k), (int *) &unit_count);
  CLOSE_FRAME;
// OUT.
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
        if (yield != M_VOID) {
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
// ESAC.
  CLOSE_FRAME;
  return GPROP (p);
}

//! @brief Execute united-case-clause.

static PROP_T genie_united_case (volatile NODE_T * p)
{
  volatile BOOL_T found_unit = A68_FALSE;
  volatile MOID_T *um;
  volatile ADDR_T pop_sp;
  jmp_buf exit_buf;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
// CASE or OUSE.
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  pop_sp = A68_SP;
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  A68_SP = pop_sp;
  um = (volatile MOID_T *) VALUE ((A68_UNION *) STACK_TOP);
// IN.
  FORWARD (q);
  if (um != NO_MOID) {
    OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
    INIT_STATIC_FRAME ((NODE_T *) SUB (q));
    found_unit = genie_united_case_unit (NEXT_SUB ((NODE_T *) q), (MOID_T *) um);
    CLOSE_FRAME;
  } else {
    found_unit = A68_FALSE;
  }
// OUT.
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
        if (yield != M_VOID) {
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
// ESAC.
  CLOSE_FRAME;
  return GPROP (p);
}

//! @brief Execute conditional-clause.

static PROP_T genie_conditional (volatile NODE_T * p)
{
  volatile ADDR_T pop_sp = A68_SP;
  jmp_buf exit_buf;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
// IF or ELIF.
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  A68_SP = pop_sp;
  FORWARD (q);
  if (VALUE ((A68_BOOL *) STACK_TOP) == A68_TRUE) {
// THEN.
    OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
    INIT_STATIC_FRAME ((NODE_T *) SUB (q));
    SERIAL_CLAUSE (NEXT_SUB (q));
    CLOSE_FRAME;
  } else {
// ELSE.
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
        if (yield != M_VOID) {
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
// FI.
  CLOSE_FRAME;
  return GPROP (p);
}

// INCREMENT_COUNTER procures that the counter only increments if there is
// a for-part or a to-part. Otherwise an infinite loop would trigger overflow
// when the anonymous counter reaches max int, which is strange behaviour.
// This is less relevant using 64-bit integers.

#define INCREMENT_COUNTER\
  if (!(for_part == NO_NODE && to_part == NO_NODE)) {\
    CHECK_INT_ADDITION ((NODE_T *) p, counter, by);\
    counter += by;\
  }

//! @brief Execute loop-clause.

static PROP_T genie_loop (volatile NODE_T * p)
{
  volatile ADDR_T pop_sp = A68_SP;
  volatile INT_T from, by, to, counter;
  volatile BOOL_T siga, conditional;
  volatile NODE_T *for_part = NO_NODE, *to_part = NO_NODE, *q = NO_NODE;
  jmp_buf exit_buf;
// FOR  identifier.
  if (IS (p, FOR_PART)) {
    for_part = NEXT_SUB (p);
    FORWARD (p);
  }
// FROM unit.
  if (IS (p, FROM_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    A68_SP = pop_sp;
    from = VALUE ((A68_INT *) STACK_TOP);
    FORWARD (p);
  } else {
    from = 1;
  }
// BY unit.
  if (IS (p, BY_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    A68_SP = pop_sp;
    by = VALUE ((A68_INT *) STACK_TOP);
    FORWARD (p);
  } else {
    by = 1;
  }
// TO unit, DOWNTO unit.
  if (IS (p, TO_PART)) {
    if (IS (SUB (p), DOWNTO_SYMBOL)) {
      by = -by;
    }
    EXECUTE_UNIT (NEXT_SUB (p));
    A68_SP = pop_sp;
    to = VALUE ((A68_INT *) STACK_TOP);
    to_part = p;
    FORWARD (p);
  } else {
    to = (by >= 0 ? A68_MAX_INT : -A68_MAX_INT);
  }
  q = NEXT_SUB (p);
// Here the loop part starts.
// We open the frame only once and reinitialise if necessary
  OPEN_STATIC_FRAME ((NODE_T *) q);
  INIT_GLOBAL_POINTER ((NODE_T *) q);
  INIT_STATIC_FRAME ((NODE_T *) q);
  counter = from;
// Does the loop contain conditionals?.
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
// [FOR ...] [WHILE ...] DO [...] [UNTIL ...] OD.
    siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
    while (siga) {
      if (for_part != NO_NODE) {
        A68_INT *z = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (for_part))));
        STATUS (z) = INIT_MASK;
        VALUE (z) = counter;
      }
      A68_SP = pop_sp;
      if (IS (p, WHILE_PART)) {
        ENQUIRY_CLAUSE (q);
        A68_SP = pop_sp;
        siga = (BOOL_T) (VALUE ((A68_BOOL *) STACK_TOP) != A68_FALSE);
      }
      if (siga) {
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
// UNTIL part.
        if (until_part != NO_NODE && IS (until_part, UNTIL_PART)) {
          NODE_T *v = NEXT_SUB (until_part);
          OPEN_STATIC_FRAME ((NODE_T *) v);
          INIT_STATIC_FRAME ((NODE_T *) v);
          A68_SP = pop_sp;
          ENQUIRY_CLAUSE (v);
          A68_SP = pop_sp;
          siga = (BOOL_T) (VALUE ((A68_BOOL *) STACK_TOP) == A68_FALSE);
          CLOSE_FRAME;
        }
        if (IS (p, WHILE_PART)) {
          CLOSE_FRAME;
        }
// Increment counter.
        if (siga) {
          INCREMENT_COUNTER;
          siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
        }
// The genie cannot take things to next iteration: re-initialise stack frame.
        if (siga) {
          FRAME_CLEAR (AP_INCREMENT (TABLE (q)));
          if (INITIALISE_FRAME (TABLE (q))) {
            initialise_frame ((NODE_T *) q);
          }
        }
      }
    }
  } else {
// [FOR ...] DO ... OD.
    siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
    while (siga) {
      if (for_part != NO_NODE) {
        A68_INT *z = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (for_part))));
        STATUS (z) = INIT_MASK;
        VALUE (z) = counter;
      }
      A68_SP = pop_sp;
      SERIAL_CLAUSE_TRACE (q);
      INCREMENT_COUNTER;
      siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
// The genie cannot take things to next iteration: re-initialise stack frame.
      if (siga) {
        FRAME_CLEAR (AP_INCREMENT (TABLE (q)));
        if (INITIALISE_FRAME (TABLE (q))) {
          initialise_frame ((NODE_T *) q);
        }
      }
    }
  }
// OD.
  CLOSE_FRAME;
  A68_SP = pop_sp;
  return GPROP (p);
}

#undef INCREMENT_COUNTER
#undef LOOP_OVERFLOW

//! @brief Execute closed clause.

static PROP_T genie_closed (volatile NODE_T * p)
{
  jmp_buf exit_buf;
  volatile NODE_T *q = NEXT_SUB (p);
  OPEN_STATIC_FRAME ((NODE_T *) q);
  INIT_GLOBAL_POINTER ((NODE_T *) q);
  INIT_STATIC_FRAME ((NODE_T *) q);
  SERIAL_CLAUSE (q);
  CLOSE_FRAME;
  return GPROP (p);
}

//! @brief Execute enclosed clause.

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
#if defined (BUILD_PARALLEL_CLAUSE)
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
  return self;
}

// Routines for handling stowed objects.
// 
// An A68G row is a reference to a descriptor in the heap:
// 
//                ...
// A68_REF row -> A68_ARRAY ----+   ARRAY: Description of row, ref to elements
//                A68_TUPLE 1   |   TUPLE: Bounds, one for every dimension
//                ...           |
//                A68_TUPLE dim |
//                ...           |
//                ...           |
//                Element 1 <---+   Element: Sequential row elements, in the heap
//                ...                        Not always contiguous - trims!
//                Element n

//! @brief Size of a row.

int get_row_size (A68_TUPLE * tup, int dim)
{
  int span = 1, k;
  for (k = 0; k < dim; k++) {
    int stride = ROW_SIZE (&tup[k]);
    ABEND ((stride > 0 && span > A68_MAX_INT / stride), ERROR_INVALID_SIZE, __func__);
    span *= stride;
  }
  return span;
}

//! @brief Initialise index for FORALL constructs.

void initialise_internal_index (A68_TUPLE * tup, int dim)
{
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    K (ref) = LWB (ref);
  }
}

//! @brief Calculate index.

ADDR_T calculate_internal_index (A68_TUPLE * tup, int dim)
{
  ADDR_T iindex = 0;
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    iindex += (SPAN (ref) * K (ref) - SHIFT (ref));
  }
  return iindex;
}

//! @brief Increment index for FORALL constructs.

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
  return carry;
}

//! @brief Print index.

void print_internal_index (FILE_T f, A68_TUPLE * tup, int dim)
{
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    char buf[BUFFER_SIZE];
    ASSERT (snprintf (buf, SNPRINTF_SIZE, A68_LD, K (ref)) >= 0);
    WRITE (f, buf);
    if (k < dim - 1) {
      WRITE (f, ", ");
    }
  }
}

//! @brief Convert C string to A68 [] CHAR.

A68_REF c_string_to_row_char (NODE_T * p, char *str, int width)
{
  A68_REF z, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  BYTE_T *base;
  int k;
  NEW_ROW_1D (z, row, arr, tup, M_ROW_CHAR, M_CHAR, width);
  base = ADDRESS (&row);
  int len = strlen (str);
  for (k = 0; k < width; k++) {
    A68_CHAR *ch = (A68_CHAR *) & (base[k * SIZE_ALIGNED (A68_CHAR)]);
    STATUS (ch) = INIT_MASK;
    VALUE (ch) = (k < len ? TO_UCHAR (str[k]) : NULL_CHAR);
  }
  return z;
}

//! @brief Convert C string to A68 string.

A68_REF c_to_a_string (NODE_T * p, char *str, int width)
{
  if (str == NO_TEXT) {
    return empty_string (p);
  } else {
    if (width == DEFAULT_WIDTH) {
      return c_string_to_row_char (p, str, (int) strlen (str));
    } else {
      return c_string_to_row_char (p, str, (int) width);
    }
  }
}

//! @brief Size of a string.

int a68_string_size (NODE_T * p, A68_REF row)
{
  (void) p;
  if (INITIALISED (&row)) {
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    GET_DESCRIPTOR (arr, tup, &row);
    return ROW_SIZE (tup);
  } else {
    return 0;
  }
}

//! @brief Convert A68 string to C string.

char *a_to_c_string (NODE_T * p, char *str, A68_REF row)
{
// Assume "str" to be long enough - caller's responsibility!.
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
        CHECK_INIT (p, INITIALISED (ch), M_CHAR);
        str[n++] = (char) VALUE (ch);
      }
    }
    str[n] = NULL_CHAR;
    return str;
  } else {
    return NO_TEXT;
  }
}

//! @brief Return an empty row.

A68_REF empty_row (NODE_T * p, MOID_T * u)
{
  A68_REF dsc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  MOID_T *v;
  int dim, k;
  if (IS_FLEX (u)) {
    u = SUB (u);
  }
  v = SUB (u);
  dim = DIM (u);
  dsc = heap_generator (p, u, DESCRIPTOR_SIZE (dim));
  GET_DESCRIPTOR (arr, tup, &dsc);
  DIM (arr) = dim;
  MOID (arr) = SLICE (u);
  ELEM_SIZE (arr) = moid_size (SLICE (u));
  SLICE_OFFSET (arr) = 0;
  FIELD_OFFSET (arr) = 0;
  if (IS_ROW (v) || IS_FLEX (v)) {
// [] AMODE or FLEX [] AMODE 
    ARRAY (arr) = heap_generator (p, v, A68_REF_SIZE);
    *DEREF (A68_REF, &ARRAY (arr)) = empty_row (p, v);
  } else {
    ARRAY (arr) = nil_ref;
  }
  STATUS (&ARRAY (arr)) = (STATUS_MASK_T) (INIT_MASK | IN_HEAP_MASK);
  for (k = 0; k < dim; k++) {
    LWB (&tup[k]) = 1;
    UPB (&tup[k]) = 0;
    SPAN (&tup[k]) = 1;
    SHIFT (&tup[k]) = LWB (tup);
  }
  return dsc;
}

//! @brief An empty string, FLEX [1 : 0] CHAR.

A68_REF empty_string (NODE_T * p)
{
  return empty_row (p, M_STRING);
}

//! @brief Make [,, ..] MODE  from [, ..] MODE.

A68_REF genie_make_rowrow (NODE_T * p, MOID_T * rmod, int len, ADDR_T sp)
{
  MOID_T *nmod = IS_FLEX (rmod) ? SUB (rmod) : rmod;
  MOID_T *emod = SUB (nmod);
  A68_REF nrow, orow;
  A68_ARRAY *narr, *oarr;
  A68_TUPLE *ntup, *otup;
  int j, k, span, odim = DIM (nmod) - 1;
// Make the new descriptor.
  nrow = heap_generator (p, rmod, DESCRIPTOR_SIZE (DIM (nmod)));
  GET_DESCRIPTOR (narr, ntup, &nrow);
  DIM (narr) = DIM (nmod);
  MOID (narr) = emod;
  ELEM_SIZE (narr) = SIZE (emod);
  SLICE_OFFSET (narr) = 0;
  FIELD_OFFSET (narr) = 0;
  if (len == 0) {
// There is a vacuum on the stack.
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
    return nrow;
  } else if (len > 0) {
    A68_ARRAY *x = NO_ARRAY;
// Arrays in the stack must have equal bounds.
    for (j = 1; j < len; j++) {
      A68_REF vrow, rrow;
      A68_TUPLE *vtup, *rtup;
      rrow = *(A68_REF *) STACK_ADDRESS (sp);
      vrow = *(A68_REF *) STACK_ADDRESS (sp + j * A68_REF_SIZE);
      GET_DESCRIPTOR (x, rtup, &rrow);
      GET_DESCRIPTOR (x, vtup, &vrow);
      for (k = 0; k < odim; k++, rtup++, vtup++) {
        if ((UPB (rtup) != UPB (vtup)) || (LWB (rtup) != LWB (vtup))) {
          diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
    }
// Fill descriptor of new row with info from (arbitrary) first one.
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
// new[j,, ] := old[, ].
      BOOL_T done;
      GET_DESCRIPTOR (oarr, otup, (A68_REF *) STACK_ADDRESS (sp + j * A68_REF_SIZE));
      if (LWB (otup) > UPB (otup)) {
        A68_REF dst = ARRAY (narr);
        ADDR_T nindex = j * SPAN (ntup) + calculate_internal_index (&ntup[1], odim);
        OFFSET (&dst) += ROW_ELEMENT (narr, nindex);
        A68_REF none = empty_row (p, SLICE (rmod));
        MOVE (ADDRESS (&dst), ADDRESS (&none), SIZE (emod));
      } else {
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
            A68_REF none = genie_clone (p, emod, (A68_REF *) & nil_ref, &src);
            MOVE (ADDRESS (&dst), ADDRESS (&none), SIZE (emod));
          } else {
            MOVE (ADDRESS (&dst), ADDRESS (&src), SIZE (emod));
          }
          done = increment_internal_index (otup, odim) | increment_internal_index (&ntup[1], odim);
        }
      }
    }
  }
  return nrow;
}

//! @brief Make a row of 'len' objects that are in the stack.

A68_REF genie_make_row (NODE_T * p, MOID_T * elem_mode, int len, ADDR_T sp)
{
  A68_REF new_row, new_arr;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int k;
  NEW_ROW_1D (new_row, new_arr, arr, tup, MOID (p), elem_mode, len);
  for (k = 0; k < len * ELEM_SIZE (&arr); k += ELEM_SIZE (&arr)) {
    A68_REF dst = new_arr, src;
    OFFSET (&dst) += k;
    STATUS (&src) = (STATUS_MASK_T) (INIT_MASK | IN_STACK_MASK);
    OFFSET (&src) = sp + k;
    REF_HANDLE (&src) = (A68_HANDLE *) & nil_handle;
    if (HAS_ROWS (elem_mode)) {
      A68_REF new_one = genie_clone (p, elem_mode, (A68_REF *) & nil_ref, &src);
      MOVE (ADDRESS (&dst), ADDRESS (&new_one), SIZE (elem_mode));
    } else {
      MOVE (ADDRESS (&dst), ADDRESS (&src), SIZE (elem_mode));
    }
  }
  return new_row;
}

//! @brief Make REF [1 : 1] [] MODE from REF [] MODE.

A68_REF genie_make_ref_row_of_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  A68_REF new_row, name, array;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  array = *(A68_REF *) STACK_ADDRESS (sp);
// ROWING NIL yields NIL.
  if (IS_NIL (array)) {
    return nil_ref;
  } else {
    new_row = heap_generator (p, SUB (dst_mode), DESCRIPTOR_SIZE (1));
    name = heap_generator (p, dst_mode, A68_REF_SIZE);
    GET_DESCRIPTOR (arr, tup, &new_row);
    DIM (arr) = 1;
    MOID (arr) = src_mode;
    ELEM_SIZE (arr) = SIZE (src_mode);
    SLICE_OFFSET (arr) = 0;
    FIELD_OFFSET (arr) = 0;
    ARRAY (arr) = array;
    LWB (tup) = 1;
    UPB (tup) = 1;
    SPAN (tup) = 1;
    SHIFT (tup) = LWB (tup);
    *DEREF (A68_REF, &name) = new_row;
    return name;
  }
}

//! @brief Make REF [1 : 1, ..] MODE from REF [..] MODE.

A68_REF genie_make_ref_row_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  A68_REF name, new_row, old_row;
  A68_ARRAY *new_arr, *old_arr;
  A68_TUPLE *new_tup, *old_tup;
  int k;
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  name = *(A68_REF *) STACK_ADDRESS (sp);
// ROWING NIL yields NIL.
  if (IS_NIL (name)) {
    return nil_ref;
  }
  old_row = *DEREF (A68_REF, &name);
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
// Make new descriptor.
  new_row = heap_generator (p, dst_mode, DESCRIPTOR_SIZE (DIM (SUB (dst_mode))));
  name = heap_generator (p, dst_mode, A68_REF_SIZE);
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (SUB (dst_mode));
  MOID (new_arr) = MOID (old_arr);
  ELEM_SIZE (new_arr) = ELEM_SIZE (old_arr);
  SLICE_OFFSET (new_arr) = 0;
  FIELD_OFFSET (new_arr) = 0;
  ARRAY (new_arr) = ARRAY (old_arr);
// Fill out the descriptor.
  LWB (&(new_tup[0])) = 1;
  UPB (&(new_tup[0])) = 1;
  SPAN (&(new_tup[0])) = 1;
  SHIFT (&(new_tup[0])) = LWB (&(new_tup[0]));
  for (k = 0; k < DIM (SUB (src_mode)); k++) {
    new_tup[k + 1] = old_tup[k];
  }
// Yield the new name.
  *DEREF (A68_REF, &name) = new_row;
  return name;
}

//! @brief Coercion to [1 : 1, ] MODE.

static PROP_T genie_rowing_row_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = A68_SP;
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), A68_FP);
  row = genie_make_rowrow (p, MOID (p), 1, sp);
  A68_SP = sp;
  PUSH_REF (p, row);
  return GPROP (p);
}

//! @brief Coercion to [1 : 1] [] MODE.

static PROP_T genie_rowing_row_of_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = A68_SP;
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), A68_FP);
  row = genie_make_row (p, SLICE (MOID (p)), 1, sp);
  A68_SP = sp;
  PUSH_REF (p, row);
  return GPROP (p);
}

//! @brief Coercion to REF [1 : 1, ..] MODE.

static PROP_T genie_rowing_ref_row_row (NODE_T * p)
{
  A68_REF name;
  ADDR_T sp = A68_SP;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), A68_FP);
  A68_SP = sp;
  name = genie_make_ref_row_row (p, dst, src, sp);
  PUSH_REF (p, name);
  return GPROP (p);
}

//! @brief REF [1 : 1] [] MODE from [] MODE

static PROP_T genie_rowing_ref_row_of_row (NODE_T * p)
{
  A68_REF name;
  ADDR_T sp = A68_SP;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), A68_FP);
  A68_SP = sp;
  name = genie_make_ref_row_of_row (p, dst, src, sp);
  PUSH_REF (p, name);
  return GPROP (p);
}

//! @brief Rowing coercion.

static PROP_T genie_rowing (NODE_T * p)
{
  PROP_T self;
  if (IS_REF (MOID (p))) {
// REF ROW, decide whether we want A->[] A or [] A->[,] A.
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
// ROW, decide whether we want A->[] A or [] A->[,] A.
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
  return self;
}

//! @brief Clone a compounded value refered to by 'old'.

A68_REF genie_clone (NODE_T * p, MOID_T * m, A68_REF * tmp, A68_REF * old)
{
// This complex routine is needed as arrays are not always contiguous.
// The routine takes a REF to the value and returns a REF to the clone.
  if (m == M_SOUND) {
// REF SOUND.
    A68_REF nsound;
    A68_SOUND *w;
    int size;
    BYTE_T *owd;
    nsound = heap_generator (p, m, SIZE (m));
    w = DEREF (A68_SOUND, &nsound);
    size = A68_SOUND_DATA_SIZE (w);
    COPY ((BYTE_T *) w, ADDRESS (old), SIZE (M_SOUND));
    owd = ADDRESS (&(DATA (w)));
    DATA (w) = heap_generator (p, M_SOUND_DATA, size);
    COPY (ADDRESS (&(DATA (w))), owd, size);
    return nsound;
  } else if (IS_STRUCT (m)) {
// REF STRUCT.
    PACK_T *fds;
    A68_REF nstruct;
    nstruct = heap_generator (p, m, SIZE (m));
    for (fds = PACK (m); fds != NO_PACK; FORWARD (fds)) {
      MOID_T *fm = MOID (fds);
      A68_REF of = *old, nf = nstruct, tf = *tmp;
      OFFSET (&of) += OFFSET (fds);
      OFFSET (&nf) += OFFSET (fds);
      if (!IS_NIL (tf)) {
        OFFSET (&tf) += OFFSET (fds);
      }
      if (HAS_ROWS (fm)) {
        A68_REF a68_clone = genie_clone (p, fm, &tf, &of);
        MOVE (ADDRESS (&nf), ADDRESS (&a68_clone), SIZE (fm));
      } else {
        MOVE (ADDRESS (&nf), ADDRESS (&of), SIZE (fm));
      }
    }
    return nstruct;
  } else if (IS_UNION (m)) {
// REF UNION.
    A68_REF nunion, src, dst, tmpu;
    A68_UNION *u;
    MOID_T *um;
    nunion = heap_generator (p, m, SIZE (m));
    src = *old;
    u = DEREF (A68_UNION, &src);
    um = (MOID_T *) VALUE (u);
    OFFSET (&src) += UNION_OFFSET;
    dst = nunion;
    *DEREF (A68_UNION, &dst) = *u;
    OFFSET (&dst) += UNION_OFFSET;
// A union has formal members, so tmp is irrelevant.
    tmpu = nil_ref;
    if (um != NO_MOID && HAS_ROWS (um)) {
      A68_REF a68_clone = genie_clone (p, um, &tmpu, &src);
      MOVE (ADDRESS (&dst), ADDRESS (&a68_clone), SIZE (um));
    } else if (um != NO_MOID) {
      MOVE (ADDRESS (&dst), ADDRESS (&src), SIZE (um));
    }
    return nunion;
  } else if (IF_ROW (m)) {
// REF [FLEX] [].
    A68_REF nrow, ntmp, heap;
    A68_ARRAY *oarr, *narr, *tarr;
    A68_TUPLE *otup, *ntup, *ttup = NO_TUPLE, *op, *np, *tp;
    MOID_T *em = SUB (IS_FLEX (m) ? SUB (m) : m);
    int k, span;
    BOOL_T check_bounds;
// Make new array.
    GET_DESCRIPTOR (oarr, otup, DEREF (A68_REF, old));
    nrow = heap_generator (p, m, DESCRIPTOR_SIZE (DIM (oarr)));
    GET_DESCRIPTOR (narr, ntup, &nrow);
    DIM (narr) = DIM (oarr);
    MOID (narr) = MOID (oarr);
    ELEM_SIZE (narr) = ELEM_SIZE (oarr);
    SLICE_OFFSET (narr) = 0;
    FIELD_OFFSET (narr) = 0;
// Get size and copy bounds; check in case of a row.
// This is just song and dance to comply with the RR.
    check_bounds = A68_FALSE;
    if (IS_NIL (*tmp)) {
      ntmp = nil_ref;
    } else {
      A68_REF *z = DEREF (A68_REF, tmp);
      if (!IS_NIL (*z)) {
        GET_DESCRIPTOR (tarr, ttup, z);
        ntmp = ARRAY (tarr);
        check_bounds = IS_ROW (m);
      }
    }
    for (span = 1, k = 0; k < DIM (oarr); k++) {
      op = &otup[k];
      np = &ntup[k];
      if (check_bounds) {
        tp = &ttup[k];
        if (UPB (tp) != UPB (op) || LWB (tp) != LWB (op)) {
          diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
      LWB (np) = LWB (op);
      UPB (np) = UPB (op);
      SPAN (np) = span;
      SHIFT (np) = LWB (np) * SPAN (np);
      span *= ROW_SIZE (np);
    }
// Make a new array with at least a ghost element.
    if (span == 0) {
      ARRAY (narr) = heap_generator (p, em, ELEM_SIZE (narr));
    } else {
      ARRAY (narr) = heap_generator (p, em, span * ELEM_SIZE (narr));
    }
// Copy the ghost element if there are no elements.
    if (span == 0 && HAS_ROWS (em)) {
      A68_REF nold, ndst, a68_clone;
      nold = ARRAY (oarr);
      OFFSET (&nold) += ROW_ELEMENT (oarr, 0);
      ndst = ARRAY (narr);
      OFFSET (&ndst) += ROW_ELEMENT (narr, 0);
      a68_clone = genie_clone (p, em, &ntmp, &nold);
      MOVE (ADDRESS (&ndst), ADDRESS (&a68_clone), SIZE (em));
    } else if (span > 0) {
// The n-dimensional copier.
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
          MOVE (ADDRESS (&ndst), ADDRESS (&a68_clone), SIZE (em));
        } else {
          MOVE (ADDRESS (&ndst), ADDRESS (&nold), SIZE (em));
        }
// Increase pointers.
        done = increment_internal_index (otup, DIM (oarr))
          | increment_internal_index (ntup, DIM (narr));
      }
    }
    heap = heap_generator (p, m, A68_REF_SIZE);
    *DEREF (A68_REF, &heap) = nrow;
    return heap;
  }
  return nil_ref;
}

//! @brief Store into a row, fi. trimmed destinations .

A68_REF genie_store (NODE_T * p, MOID_T * m, A68_REF * dst, A68_REF * old)
{
// This complex routine is needed as arrays are not always contiguous.
// The routine takes a REF to the value and returns a REF to the clone.
  if (IF_ROW (m)) {
// REF [FLEX] [].
    A68_ARRAY *old_arr, *new_arr;
    A68_TUPLE *old_tup, *new_tup, *old_p, *new_p;
    MOID_T *em = SUB (IS_FLEX (m) ? SUB (m) : m);
    int k, span;
    BOOL_T done = A68_FALSE;
    GET_DESCRIPTOR (old_arr, old_tup, DEREF (A68_REF, old));
    GET_DESCRIPTOR (new_arr, new_tup, DEREF (A68_REF, dst));
// Get size and check bounds.
// This is just song and dance to comply with the RR.
    for (span = 1, k = 0; k < DIM (old_arr); k++) {
      old_p = &old_tup[k];
      new_p = &new_tup[k];
      if ((UPB (new_p) != UPB (old_p) || LWB (new_p) != LWB (old_p))) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
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
        MOVE (ADDRESS (&new_dst), ADDRESS (&new_old), SIZE (em));
        done = increment_internal_index (old_tup, DIM (old_arr))
          | increment_internal_index (new_tup, DIM (new_arr));
      }
    }
    return *dst;
  }
  return nil_ref;
}

//! @brief Assignment of complex objects in the stack.

static void genie_clone_stack (NODE_T * p, MOID_T * srcm, A68_REF * dst, A68_REF * tmp)
{
// STRUCT, UNION, [FLEX] [] or SOUND.
  A68_REF stack, *src, a68_clone;
  STATUS (&stack) = (STATUS_MASK_T) (INIT_MASK | IN_STACK_MASK);
  OFFSET (&stack) = A68_SP;
  REF_HANDLE (&stack) = (A68_HANDLE *) & nil_handle;
  src = DEREF (A68_REF, &stack);
  if (IS_ROW (srcm) && !IS_NIL (*tmp)) {
    if (STATUS (src) & SKIP_ROW_MASK) {
      return;
    }
    a68_clone = genie_clone (p, srcm, tmp, &stack);
    (void) genie_store (p, srcm, dst, &a68_clone);
  } else {
    a68_clone = genie_clone (p, srcm, tmp, &stack);
    MOVE (ADDRESS (dst), ADDRESS (&a68_clone), SIZE (srcm));
  }
}

//! @brief Push description for diagonal of square matrix.

static PROP_T genie_diagonal_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROP_T self;
  A68_ROW row, new_row;
  int k = 0;
  BOOL_T name = (BOOL_T) (IS_REF (MOID (p)));
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
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  if (ROW_SIZE (tup1) != ROW_SIZE (tup2)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_NO_SQUARE_MATRIX, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (ABS (k) >= ROW_SIZE (tup1)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, DESCRIPTOR_SIZE (1));
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
    *DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  UNIT (&self) = genie_diagonal_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push description for transpose of matrix.

static PROP_T genie_transpose_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROP_T self;
  A68_ROW row, new_row;
  BOOL_T name = (BOOL_T) (IS_REF (MOID (p)));
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
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  new_row = heap_generator (p, m, DESCRIPTOR_SIZE (2));
  new_arr = *arr;
  new_tup1 = *tup2;
  new_tup2 = *tup1;
  PUT_DESCRIPTOR2 (new_arr, new_tup1, new_tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), A68_REF_SIZE);
    *DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  UNIT (&self) = genie_transpose_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push description for row vector.

static PROP_T genie_row_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROP_T self;
  A68_ROW row, new_row;
  int k = 1;
  BOOL_T name = (BOOL_T) (IS_REF (MOID (p)));
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
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  if (DIM (arr) != 1) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_NO_VECTOR, m, PRIMARY);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, DESCRIPTOR_SIZE (2));
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
    *DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  UNIT (&self) = genie_row_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push description for column vector.

static PROP_T genie_column_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROP_T self;
  A68_ROW row, new_row;
  int k = 1;
  BOOL_T name = (BOOL_T) (IS_REF (MOID (p)));
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
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, DESCRIPTOR_SIZE (2));
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
    *DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  UNIT (&self) = genie_column_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Strcmp for qsort.

int qstrcmp (const void *a, const void *b)
{
  return strcmp (*(char *const *) a, *(char *const *) b);
}

//! @brief Sort row of string.

void genie_sort_row_string (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  ADDR_T pop_sp;
  int size;
  POP_REF (p, &z);
  pop_sp = A68_SP;
  CHECK_REF (p, z, M_ROW_STRING);
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  if (size > 0) {
    A68_REF row, *base_ref;
    A68_ARRAY arrn;
    A68_TUPLE tupn;
    int j, k;
    BYTE_T *base = ADDRESS (&ARRAY (arr));
    char **ptrs = (char **) a68_alloc ((size_t) (size * (int) sizeof (char *)), __func__, __LINE__);
    if (ptrs == NO_VAR) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
// Copy C-strings into the stack and sort.
    for (j = 0, k = LWB (tup); k <= UPB (tup); j++, k++) {
      int addr = INDEX_1_DIM (arr, tup, k);
      A68_REF ref = *(A68_REF *) & (base[addr]);
      int len;
      CHECK_REF (p, ref, M_STRING);
      len = A68_ALIGN (a68_string_size (p, ref) + 1);
      if (A68_SP + len > A68 (expr_stack_limit)) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      ptrs[j] = (char *) STACK_TOP;
      ASSERT (a_to_c_string (p, (char *) STACK_TOP, ref) != NO_TEXT);
      INCREMENT_STACK_POINTER (p, len);
    }
    qsort (ptrs, (size_t) size, sizeof (char *), qstrcmp);
// Construct an array of sorted strings.
    NEW_ROW_1D (z, row, arrn, tupn, M_ROW_STRING, M_STRING, size);
    base_ref = DEREF (A68_REF, &row);
    for (k = 0; k < size; k++) {
      base_ref[k] = c_to_a_string (p, ptrs[k], DEFAULT_WIDTH);
    }
    a68_free (ptrs);
    A68_SP = pop_sp;
    PUSH_REF (p, z);
  } else {
// This is how we sort an empty row of strings ...
    A68_SP = pop_sp;
    PUSH_REF (p, empty_row (p, M_ROW_STRING));
  }
}

//! @brief Propagator_name.

char *propagator_name (PROP_PROC * p)
{
  if (p == genie_and_function) {
    return "genie_and_function";
  }
  if (p == genie_assertion) {
    return "genie_assertion";
  }
  if (p == genie_assignation) {
    return "genie_assignation";
  }
  if (p == genie_assignation_constant) {
    return "genie_assignation_constant";
  }
  if (p == genie_call) {
    return "genie_call";
  }
  if (p == genie_cast) {
    return "genie_cast";
  }
  if (p == (PROP_PROC *) genie_closed) {
    return "genie_closed";
  }
  if (p == genie_coercion) {
    return "genie_coercion";
  }
  if (p == genie_collateral) {
    return "genie_collateral";
  }
  if (p == genie_column_function) {
    return "genie_column_function";
  }
  if (p == (PROP_PROC *) genie_conditional) {
    return "genie_conditional";
  }
  if (p == genie_constant) {
    return "genie_constant";
  }
  if (p == genie_denotation) {
    return "genie_denotation";
  }
  if (p == genie_deproceduring) {
    return "genie_deproceduring";
  }
  if (p == genie_dereference_frame_identifier) {
    return "genie_dereference_frame_identifier";
  }
  if (p == genie_dereference_selection_name_quick) {
    return "genie_dereference_selection_name_quick";
  }
  if (p == genie_dereference_slice_name_quick) {
    return "genie_dereference_slice_name_quick";
  }
  if (p == genie_dereferencing) {
    return "genie_dereferencing";
  }
  if (p == genie_dereferencing_quick) {
    return "genie_dereferencing_quick";
  }
  if (p == genie_diagonal_function) {
    return "genie_diagonal_function";
  }
  if (p == genie_dyadic) {
    return "genie_dyadic";
  }
  if (p == genie_dyadic_quick) {
    return "genie_dyadic_quick";
  }
  if (p == (PROP_PROC *) genie_enclosed) {
    return "genie_enclosed";
  }
  if (p == genie_format_text) {
    return "genie_format_text";
  }
  if (p == genie_formula) {
    return "genie_formula";
  }
  if (p == genie_generator) {
    return "genie_generator";
  }
  if (p == genie_identifier) {
    return "genie_identifier";
  }
  if (p == genie_identifier_standenv) {
    return "genie_identifier_standenv";
  }
  if (p == genie_identifier_standenv_proc) {
    return "genie_identifier_standenv_proc";
  }
  if (p == genie_identity_relation) {
    return "genie_identity_relation";
  }
  if (p == (PROP_PROC *) genie_int_case) {
    return "genie_int_case";
  }
  if (p == genie_field_selection) {
    return "genie_field_selection";
  }
  if (p == genie_frame_identifier) {
    return "genie_frame_identifier";
  }
  if (p == (PROP_PROC *) genie_loop) {
    return "genie_loop";
  }
  if (p == genie_monadic) {
    return "genie_monadic";
  }
  if (p == genie_nihil) {
    return "genie_nihil";
  }
  if (p == genie_or_function) {
    return "genie_or_function";
  }
#if defined (BUILD_PARALLEL_CLAUSE)
  if (p == genie_parallel) {
    return "genie_parallel";
  }
#endif
  if (p == genie_routine_text) {
    return "genie_routine_text";
  }
  if (p == genie_row_function) {
    return "genie_row_function";
  }
  if (p == genie_rowing) {
    return "genie_rowing";
  }
  if (p == genie_rowing_ref_row_of_row) {
    return "genie_rowing_ref_row_of_row";
  }
  if (p == genie_rowing_ref_row_row) {
    return "genie_rowing_ref_row_row";
  }
  if (p == genie_rowing_row_of_row) {
    return "genie_rowing_row_of_row";
  }
  if (p == genie_rowing_row_row) {
    return "genie_rowing_row_row";
  }
  if (p == genie_selection) {
    return "genie_selection";
  }
  if (p == genie_selection_name_quick) {
    return "genie_selection_name_quick";
  }
  if (p == genie_selection_value_quick) {
    return "genie_selection_value_quick";
  }
  if (p == genie_skip) {
    return "genie_skip";
  }
  if (p == genie_slice) {
    return "genie_slice";
  }
  if (p == genie_slice_name_quick) {
    return "genie_slice_name_quick";
  }
  if (p == genie_transpose_function) {
    return "genie_transpose_function";
  }
  if (p == genie_unit) {
    return "genie_unit";
  }
  if (p == (PROP_PROC *) genie_united_case) {
    return "genie_united_case";
  }
  if (p == genie_uniting) {
    return "genie_uniting";
  }
  if (p == genie_voiding) {
    return "genie_voiding";
  }
  if (p == genie_voiding_assignation) {
    return "genie_voiding_assignation";
  }
  if (p == genie_voiding_assignation_constant) {
    return "genie_voiding_assignation_constant";
  }
  if (p == genie_widen) {
    return "genie_widen";
  }
  if (p == genie_widen_int_to_real) {
    return "genie_widen_int_to_real";
  }
  return NO_TEXT;
}
