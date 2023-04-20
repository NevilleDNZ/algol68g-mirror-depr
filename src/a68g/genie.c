//! @file genie.c
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
//! Interpreter driver.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-frames.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

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
    wait = (int) sleep ((unt) wait);
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

void change_masks (NODE_T * p, unt mask, BOOL_T set)
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
    single_step (p, (unt) BREAKPOINT_ERROR_MASK);
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
    init_rng ((unt) t);
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

void tie_label (NODE_T * p, NODE_T * unit)
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

int mode_attribute (MOID_T * p)
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

void genie_preprocess (NODE_T * p, int *max_lev, void *compile_plugin)
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
      if (OPTION_OPT_LEVEL (&A68_JOB) > 0 && COMPILE_NAME (GINFO (p)) != NO_TEXT && compile_plugin != NULL) {
        if (COMPILE_NAME (GINFO (p)) == last_compile_name) {
// We copy.
          UNIT (&GPROP (p)) = last_compile_unit;
        } else {
// We look up.
// Next line may provoke a warning even with this POSIX workaround. Tant pis.
          *(void **) &(UNIT (&GPROP (p))) = dlsym (compile_plugin, COMPILE_NAME (GINFO (p)));
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
      genie_preprocess (SUB (p), max_lev, compile_plugin);
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

void genie (void *compile_plugin)
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
  genie_preprocess (TOP_NODE (&A68_JOB), &A68 (max_lex_lvl), compile_plugin);
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
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "genie: frame stack %uk, expression stack %uk, heap %uk, handles %uk\n", A68 (frame_stack_size) / KILOBYTE, A68 (expr_stack_size) / KILOBYTE, A68 (heap_size) / KILOBYTE, A68 (handle_pool_size) / KILOBYTE) >= 0);
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
      genie (compile_plugin);
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

void genie_init_proc_op (NODE_T * p, NODE_T ** seq, int *count)
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
      CHECK_INIT (p, (unt) MP_STATUS (z) & INIT_MASK, q);
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
      CHECK_INIT (p, (unt) MP_STATUS (z) & INIT_MASK, q);
      return;
    }
  case MODE_LONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_mp ());
      CHECK_INIT (p, (unt) r[0] & INIT_MASK, q);
      CHECK_INIT (p, (unt) i[0] & INIT_MASK, q);
      return;
    }
#endif
  case MODE_LONG_LONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_long_mp ());
      CHECK_INIT (p, (unt) r[0] & INIT_MASK, q);
      CHECK_INIT (p, (unt) i[0] & INIT_MASK, q);
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

PROP_T genie_constant (NODE_T * p)
{
  PUSH (p, CONSTANT (GINFO (p)), SIZE (GINFO (p)));
  return GPROP (p);
}

//! @brief Push argument units.

void genie_argument (NODE_T * p, NODE_T ** seq)
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

PROP_T genie_call_standenv_quick (NODE_T * p)
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

PROP_T genie_call_quick (NODE_T * p)
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

PROP_T genie_call (NODE_T * p)
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

//! @brief Push value of denotation.

PROP_T genie_denotation (NODE_T * p)
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

PROP_T genie_frame_identifier (NODE_T * p)
{
  BYTE_T *z;
  FRAME_GET (z, BYTE_T, p);
  PUSH (p, z, SIZE (MOID (p)));
  return GPROP (p);
}

//! @brief Push standard environ routine as PROC.

PROP_T genie_identifier_standenv_proc (NODE_T * p)
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

PROP_T genie_identifier_standenv (NODE_T * p)
{
  (void) ((*(PROCEDURE (TAX (p)))) (p));
  return GPROP (p);
}

//! @brief Push identifier onto the stack.

PROP_T genie_identifier (NODE_T * p)
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

PROP_T genie_cast (NODE_T * p)
{
  PROP_T self;
  EXECUTE_UNIT (NEXT_SUB (p));
  UNIT (&self) = genie_cast;
  SOURCE (&self) = p;
  return self;
}

//! @brief Execute assertion.

PROP_T genie_assertion (NODE_T * p)
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

PROP_T genie_format_text (NODE_T * p)
{
  static PROP_T self;
  A68_FORMAT z = *(A68_FORMAT *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_FORMAT (p, z);
  UNIT (&self) = genie_format_text;
  SOURCE (&self) = p;
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

PROP_T genie_monadic (NODE_T * p)
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

PROP_T genie_dyadic_quick (NODE_T * p)
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

PROP_T genie_dyadic (NODE_T * p)
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

PROP_T genie_formula (NODE_T * p)
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

PROP_T genie_nihil (NODE_T * p)
{
  PROP_T self;
  PUSH_REF (p, nil_ref);
  UNIT (&self) = genie_nihil;
  SOURCE (&self) = p;
  return self;
}

//! @brief Assign a value to a name and voiden.

PROP_T genie_voiding_assignation_constant (NODE_T * p)
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

PROP_T genie_voiding_assignation (NODE_T * p)
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

PROP_T genie_assignation_constant (NODE_T * p)
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

PROP_T genie_assignation_quick (NODE_T * p)
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

PROP_T genie_assignation (NODE_T * p)
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

PROP_T genie_identity_relation (NODE_T * p)
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

PROP_T genie_and_function (NODE_T * p)
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

PROP_T genie_or_function (NODE_T * p)
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

PROP_T genie_routine_text (NODE_T * p)
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
    PUSH_VALUE (p, (a68_unif_rand ()), A68_REAL);
  } else if (u == M_BOOL) {
    PUSH_VALUE (p, (BOOL_T) (a68_unif_rand () < 0.5), A68_BOOL);
  } else if (u == M_CHAR) {
    PUSH_VALUE (p, (char) (32 + 96 * a68_unif_rand ()), A68_CHAR);
  } else if (u == M_BITS) {
    PUSH_VALUE (p, (UNSIGNED_T) (a68_unif_rand () * A68_MAX_BITS), A68_BITS);
  } else if (u == M_COMPLEX) {
    PUSH_COMPLEX (p, a68_unif_rand (), a68_unif_rand ());
  } else if (u == M_BYTES) {
    PUSH_BYTES (p, "SKIP");
  } else if (u == M_LONG_BYTES) {
    PUSH_LONG_BYTES (p, "SKIP");
  } else if (u == M_STRING) {
    PUSH_REF (p, empty_string (p));
  } else if (u == M_LONG_INT) {
#if (A68_LEVEL >= 3)
    DOUBLE_NUM_T w;
    set_lw (w, 1);
    PUSH_VALUE (p, w, A68_LONG_INT);    // Because users write [~] INT !
#else
    (void) nil_mp (p, DIGITS (u));
#endif
  } else if (u == M_LONG_REAL) {
#if (A68_LEVEL >= 3)
    genie_next_random_double_real (p);
#else
    (void) nil_mp (p, DIGITS (u));
#endif
  } else if (u == M_LONG_BITS) {
#if (A68_LEVEL >= 3)
    DOUBLE_NUM_T w;
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
    genie_next_random_double_real (p);
    genie_next_random_double_real (p);
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

PROP_T genie_skip (NODE_T * p)
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

void genie_jump (NODE_T * p)
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

PROP_T genie_unit (NODE_T * p)
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

void genie_serial_units_no_label (NODE_T * p, ADDR_T pop_sp, NODE_T ** seq)
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

void genie_collateral_units (NODE_T * p, int *count)
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

PROP_T genie_collateral (NODE_T * p)
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

BOOL_T genie_united_case_unit (NODE_T * p, MOID_T * m)
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
      unt size = (unt) SIZE (src_mode);
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
            MOVE (ADDRESS (z), STACK_TOP, (unt) SIZE (src_mode));
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
          MOVE (ADDRESS (z), STACK_TOP, (unt) SIZE (src_mode));
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

#define LABEL_FREE(_p_) {\
  NODE_T *_m_q; ADDR_T pop_sp_lf = A68_SP;\
  for (_m_q = SEQUENCE (_p_); _m_q != NO_NODE; _m_q = SEQUENCE (_m_q)) {\
    if (IS (_m_q, UNIT) || IS (_m_q, DECLARATION_LIST)) {\
      EXECUTE_UNIT_TRACE (_m_q);\
    }\
    if (SEQUENCE (_m_q) != NO_NODE) {\
      A68_SP = pop_sp_lf;\
      _m_q = SEQUENCE (_m_q);\
    }\
  }}

#define SERIAL_CLAUSE(_p_)\
  genie_preemptive_gc_heap ((NODE_T *) (_p_));\
  if (STATUS_TEST ((_p_), OPTIMAL_MASK)) {\
    EXECUTE_UNIT_TRACE (SEQUENCE (_p_));\
  } else if (STATUS_TEST ((_p_), SERIAL_MASK)) {\
    LABEL_FREE (_p_);\
  } else {\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) (_p_), (jmp_buf *) exit_buf);\
  }}

#define ENQUIRY_CLAUSE(_p_)\
  genie_preemptive_gc_heap ((NODE_T *) (_p_));\
  if (STATUS_TEST ((_p_), OPTIMAL_MASK)) {\
    EXECUTE_UNIT (SEQUENCE (_p_));\
  } else if (STATUS_TEST ((_p_), SERIAL_MASK)) {\
    LABEL_FREE (_p_);\
  } else {\
    genie_enquiry_clause ((NODE_T *) (_p_));\
  }

//! @brief Execute integral-case-clause.

PROP_T genie_int_case (volatile NODE_T * p)
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

PROP_T genie_united_case (volatile NODE_T * p)
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

PROP_T genie_conditional (volatile NODE_T * p)
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

PROP_T genie_loop (volatile NODE_T * p)
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
          SERIAL_CLAUSE (do_part);
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
      SERIAL_CLAUSE (q);
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

PROP_T genie_closed (volatile NODE_T * p)
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

PROP_T genie_enclosed (volatile NODE_T * p)
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
