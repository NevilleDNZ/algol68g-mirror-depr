/*!
\file engine.c
\brief routines executing primitive A68 actions
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "config.h"
#include "algol68g.h"
#include "interpreter.h"
#include "mp.h"

/* Driver for the interpreter */

#if ! defined ENABLE_WIN32
#include <sys/resource.h>
#endif

void genie_preprocess (NODE_T *, int *, void *);

A68_HANDLE nil_handle = { INITIALISED_MASK, NULL, 0, NULL, NULL, NULL };
A68_REF nil_ref = { (STATUS_MASK) (INITIALISED_MASK | NIL_MASK), 0, {NULL} };

ADDR_T frame_pointer = 0, stack_pointer = 0, heap_pointer = 0, handle_pointer = 0, global_pointer = 0, frame_start, frame_end, stack_start, stack_end;
BOOL_T do_confirm_exit = A68_TRUE;
BYTE_T *stack_segment = NULL, *heap_segment = NULL, *handle_segment = NULL;
NODE_T *last_unit = NULL;
int global_level = 0, ret_code, ret_line_number, ret_char_number;
int max_lex_lvl = 0;
jmp_buf genie_exit_label;

int frame_stack_size, expr_stack_size, heap_size, handle_pool_size;
int stack_limit, frame_stack_limit, expr_stack_limit;
int storage_overhead;

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
  sys_ret_code = system (a_to_c_string (p, (char *) ADDRESS (&ref_z), cmd));
  PUSH_PRIMITIVE (p, sys_ret_code, A68_INT);
}

/*!
\brief set flags throughout tree
\param p position in tree
**/

void change_masks (NODE_T * p, unsigned mask, BOOL_T set)
{
  for (; p != NULL; FORWARD (p)) {
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
#ifdef ENABLE_CURSES
  genie_curses_end (p);
#endif
  if (!in_execution) {
    return;
  }
  if (ret == A68_RUNTIME_ERROR && in_monitor) {
    return;
  } else if (ret == A68_RUNTIME_ERROR && program.options.debug) {
    diagnostics_to_terminal (program.top_line, A68_RUNTIME_ERROR);
    single_step (p, (unsigned) BREAKPOINT_ERROR_MASK);
    in_execution = A68_FALSE;
    ret_line_number = LINE_NUMBER (p);
    ret_code = ret;
    longjmp (genie_exit_label, 1);
  } else {
    if (ret > A68_FORCE_QUIT) {
      ret -= A68_FORCE_QUIT;
    }
#if defined ENABLE_PAR_CLAUSE
    if (!whether_main_thread ()) {
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
    int seed = u->tm_sec + 60 * (u->tm_min + 60 * u->tm_hour);
    init_rng ((long unsigned) seed);
  }
}

/*!
\brief tie label to the clause it is defined in
\param p position in tree
**/

void tie_label_to_serial (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, SERIAL_CLAUSE)) {
      BOOL_T valid_follow;
      if (NEXT (p) == NULL) {
        valid_follow = A68_TRUE;
      } else if (WHETHER (NEXT (p), CLOSE_SYMBOL)) {
        valid_follow = A68_TRUE;
      } else if (WHETHER (NEXT (p), END_SYMBOL)) {
        valid_follow = A68_TRUE;
      } else if (WHETHER (NEXT (p), EDOC_SYMBOL)) {
        valid_follow = A68_TRUE;
      } else if (WHETHER (NEXT (p), OD_SYMBOL)) {
        valid_follow = A68_TRUE;
      } else {
        valid_follow = A68_FALSE;
      }
      if (valid_follow) {
        SYMBOL_TABLE (SUB (p))->jump_to = NULL;
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
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAX (p)->unit = unit;
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
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, LABELED_UNIT)) {
      tie_label (SUB_SUB (p), NEXT_SUB (p));
    }
    tie_label_to_unit (SUB (p));
  }
}

/*!
\brief protect constructs from premature sweeping
\param p position in tree
**/

void protect_from_sweep (NODE_T * p)
{
/*
Insert annotations in the tree that prevent premature sweeping of temporary
names and rows. For instance, let x, y be PROC STRING, then x + y can crash by
the heap sweeper. Annotations are local hence when the block is exited they
become prone to the heap sweeper.
*/
  for (; p != NULL; FORWARD (p)) {
    protect_from_sweep (SUB (p));
    if (GENIE (p) != NULL) {
      GENIE (p)->protect_sweep = NULL;
    }
/*
Catch all constructs that give vulnerable intermediate results on the stack.
Units do not apply, casts work through their enclosed-clauses, denotations are
protected and identifiers protect themselves. 
*/
    switch (ATTRIBUTE (p)) {
    case FORMULA:
    case MONADIC_FORMULA:
    case GENERATOR:
/*  case ENCLOSED_CLAUSE: */
/*  case PARALLEL_CLAUSE: */
    case CLOSED_CLAUSE:
    case COLLATERAL_CLAUSE:
    case CONDITIONAL_CLAUSE:
    case INTEGER_CASE_CLAUSE:
    case UNITED_CASE_CLAUSE:
    case LOOP_CLAUSE:
    case CODE_CLAUSE:
    case CALL:
    case SLICE:
    case SELECTION:
    case FIELD_SELECTION:
    case DEPROCEDURING:
    case ROWING:
    case WIDENING:
      {
        MOID_T *m = MOID (p);
        if (m != NULL && (WHETHER (m, REF_SYMBOL) || WHETHER (DEFLEX (m), ROW_SYMBOL))) {
          TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, m, PROTECT_FROM_SWEEP);
          GENIE (p)->protect_sweep = z;
          HEAP (z) = HEAP_SYMBOL;
          USE (z) = A68_TRUE;
        }
        break;
      }
    }
  }
}

/*!
\brief fast way to indicate a mode
\param p position in tree
**/

static int mode_attribute (MOID_T * p)
{
  if (WHETHER (p, REF_SYMBOL)) {
    return (REF_SYMBOL);
  } else if (WHETHER (p, PROC_SYMBOL)) {
    return (PROC_SYMBOL);
  } else if (WHETHER (p, UNION_SYMBOL)) {
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
\brief whether a symbol table contains no user definition
\param t symbol table
\return TRUE or FALSE
**/

BOOL_T genie_no_user_symbols (SYMBOL_TABLE_T * t)
{
  return ((BOOL_T) (t->identifiers == NULL && t->operators == NULL && PRIO (t) == NULL && t->indicants == NULL && t->labels == NULL));
}

/*!
\brief whether a symbol table contains no (anonymous) definition
\param t symbol table
\return TRUE or FALSE
**/

static BOOL_T genie_empty_table (SYMBOL_TABLE_T * t)
{
  return ((BOOL_T) (t->identifiers == NULL && t->operators == NULL && PRIO (t) == NULL && t->indicants == NULL && t->labels == NULL));
}

/*!
\brief perform tasks before interpretation
\param p position in tree
\param max_lev maximum level found
**/

void genie_preprocess (NODE_T * p, int *max_lev, void *compile_lib)
{
  for (; p != NULL; FORWARD (p)) {
    if (STATUS_TEST (p, BREAKPOINT_MASK)) {
      if (!(STATUS_TEST (p, INTERRUPTIBLE_MASK))) {
        STATUS_CLEAR (p, BREAKPOINT_MASK);
      }
    }
    if (GENIE (p) != NULL) {
      GENIE (p)->whether_coercion = whether_coercion (p);
      GENIE (p)->whether_new_lexical_level = whether_new_lexical_level (p);
#if defined ENABLE_COMPILER
      if (program.options.optimise && GENIE (p)->compile_name != NULL && compile_lib != NULL) {
/* Writing (PROPAGATOR_T) dlsym(...) would seem more natural, but the C99 standard leaves
   casting from void * to a function pointer undefined. The assignment below is the 
   POSIX.1-2003 (Technical Corrigendum 1) workaround. */
        * (void **) &(PROPAGATOR (p).unit) = dlsym (compile_lib, GENIE (p)->compile_name);
        ABEND (PROPAGATOR (p).unit == NULL, "compiler cannot resolve", dlerror ());
      } else {
        PROPAGATOR (p).unit = genie_unit;
      }
#else
      PROPAGATOR (p).unit = genie_unit;
#endif
      PROPAGATOR (p).source = p;
    }
    if (MOID (p) != NULL) {
      MOID (p)->size = moid_size (MOID (p));
      MOID (p)->short_id = mode_attribute (MOID (p));
      if (GENIE (p) != NULL) {
        if (WHETHER (MOID (p), REF_SYMBOL)) {
          GENIE (p)->need_dns = A68_TRUE;
        } else if (WHETHER (MOID (p), PROC_SYMBOL)) {
          GENIE (p)->need_dns = A68_TRUE;
        } else if (WHETHER (MOID (p), UNION_SYMBOL)) {
          GENIE (p)->need_dns = A68_TRUE;
        } else if (WHETHER (MOID (p), FORMAT_SYMBOL)) {
          GENIE (p)->need_dns = A68_TRUE;
        }
      }
    }
    if (SYMBOL_TABLE (p) != NULL) {
      SYMBOL_TABLE (p)->empty_table = genie_empty_table (SYMBOL_TABLE (p));
      if (LEX_LEVEL (p) > *max_lev) {
        *max_lev = LEX_LEVEL (p);
      }
    }
    if (WHETHER (p, FORMAT_TEXT)) {
      TAG_T *q = TAX (p);
      if (q != NULL && NODE (q) != NULL) {
        NODE (q) = p;
      }
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *q = TAX (p);
      if (q != NULL && NODE (q) != NULL && SYMBOL_TABLE (NODE (q)) != NULL) {
        LEVEL (GENIE (p)) = LEX_LEVEL (NODE (q));
      }
    } else if (WHETHER (p, IDENTIFIER)) {
      TAG_T *q = TAX (p);
      if (q != NULL && NODE (q) != NULL && SYMBOL_TABLE (NODE (q)) != NULL) {
        LEVEL (GENIE (p)) = LEX_LEVEL (NODE (q));
        OFFSET (GENIE (p)) = &stack_segment[FRAME_INFO_SIZE + OFFSET (q)];
      }
    } else if (WHETHER (p, OPERATOR)) {
      TAG_T *q = TAX (p);
      if (q != NULL && NODE (q) != NULL && SYMBOL_TABLE (NODE (q)) != NULL) {
        LEVEL (GENIE (p)) = LEX_LEVEL (NODE (q));
        OFFSET (GENIE (p)) = &stack_segment[FRAME_INFO_SIZE + OFFSET (q)];
      }
    }
    if (SUB (p) != NULL) {
      if (GENIE (p) != NULL) {
        PARENT (SUB (p)) = p;
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
  for (; p != NULL; FORWARD (p)) {
    if (LINE_NUMBER (p) != 0 && WHETHER (p, UNIT)) {
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
  for (; p != NULL; FORWARD (p)) {
    free_genie_heap (SUB (p));
    if (GENIE (p) != NULL && GENIE (p)->constant != NULL) {
      free (GENIE (p)->constant);
      GENIE (p)->constant = NULL;
    }
  }
}

/*!
\brief driver for the interpreter
**/

void genie (void * compile_lib)
{
  MOID_LIST_T *ml;
/* Fill in final info for modes */
  for (ml = top_moid_list; ml != NULL; FORWARD (ml)) {
    MOID_T *mml = MOID (ml);
    mml->size = moid_size (mml);
    mml->short_id = mode_attribute (mml);
  }
/* Preprocessing */
  max_lex_lvl = 0;
/*  genie_lex_levels (program.top_node, 1); */
  genie_preprocess (program.top_node, &max_lex_lvl, compile_lib);
  change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
  watchpoint_expression = NULL;
  frame_stack_limit = frame_end - storage_overhead;
  expr_stack_limit = stack_end - storage_overhead;
  if (program.options.regression_test) {
    init_rng (1);
  } else {
    genie_init_rng ();
  }
  io_close_tty_line ();
  if (program.options.trace) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "genie: frame stack %dk, expression stack %dk, heap %dk, handles %dk\n", frame_stack_size / KILOBYTE, expr_stack_size / KILOBYTE, heap_size / KILOBYTE, handle_pool_size / KILOBYTE) >= 0);
    WRITE (STDOUT_FILENO, output_line);
  }
  install_signal_handlers ();
  do_confirm_exit = A68_TRUE;
/* Dive into the program. */
  if (setjmp (genie_exit_label) == 0) {
    NODE_T *p = SUB (program.top_node);
/* If we are to stop in the monitor, set a breakpoint on the first unit. */
    if (program.options.debug) {
      change_masks (program.top_node, BREAKPOINT_TEMPORARY_MASK, A68_TRUE);
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
    FRAME_DYNAMIC_SCOPE (frame_pointer) = 0;
    FRAME_STATIC_LINK (frame_pointer) = 0;
    FRAME_NUMBER (frame_pointer) = 0;
    FRAME_TREE (frame_pointer) = (NODE_T *) p;
    FRAME_LEXICAL_LEVEL (frame_pointer) = LEX_LEVEL (p);
    FRAME_PARAMETER_LEVEL (frame_pointer) = LEX_LEVEL (p);
    FRAME_PARAMETERS (frame_pointer) = frame_pointer;
    initialise_frame (p);
    genie_init_heap (p);
    genie_init_transput (program.top_node);
    cputime_0 = seconds ();
/* Here we go ... */
    in_execution = A68_TRUE;
    last_unit = program.top_node;
#if ! defined ENABLE_WIN32
    (void) alarm (1);
#endif
    if (program.options.trace) {
      where_in_source (STDOUT_FILENO, program.top_node);
    }
    (void) genie_enclosed (program.top_node);
  } else {
/* Here we have jumped out of the interpreter. What happened? */
    if (program.options.debug) {
      WRITE (STDOUT_FILENO, "Execution discontinued");
    }
    if (ret_code == A68_RERUN) {
      diagnostics_to_terminal (program.top_line, A68_RUNTIME_ERROR);
      genie (compile_lib);
    } else if (ret_code == A68_RUNTIME_ERROR) {
      if (program.options.backtrace) {
        int printed = 0;
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\nStack backtrace") >= 0);
        WRITE (STDOUT_FILENO, output_line);
        stack_dump (STDOUT_FILENO, frame_pointer, 16, &printed);
        WRITE (STDOUT_FILENO, "\n");
      }
      if (program.files.listing.opened) {
        int printed = 0;
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\nStack backtrace") >= 0);
        WRITE (program.files.listing.fd, output_line);
        stack_dump (program.files.listing.fd, frame_pointer, 32, &printed);
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
Such piece of information is called a PROPAGATOR.
*/

void genie_serial_units_no_label (NODE_T *, int, NODE_T **);

PROPAGATOR_T genie_assignation_quick (NODE_T * p);
PROPAGATOR_T genie_loop (volatile NODE_T *);
PROPAGATOR_T genie_voiding_assignation_constant (NODE_T * p);
PROPAGATOR_T genie_voiding_assignation (NODE_T * p);

/*!
\brief shows line where 'p' is at and draws a '-' beneath the position
\param f file number
\param p position in tree
**/

void where_in_source (FILE_T f, NODE_T * p)
{
  write_source_line (f, LINE (p), p, A68_NO_DIAGNOSTICS);
}

/*
Since Algol 68 can pass procedures as parameters, we use static links rather
than a display. Static link access to non-local variables is more elaborate than
display access, but you don't have to copy the display on every call, which is
expensive in terms of time and stack space. Early versions of Algol68G did use a
display, but speed improvement was negligible and the code was less transparent.
So it was reverted to static links.
*/

/*!
\brief initialise PROC and OP identities
\param p starting node of a declaration
\param seq chain to link nodes into
\param count number of constants initialised
**/

static void genie_init_proc_op (NODE_T * p, NODE_T ** seq, int *count)
{
  for (; p != NULL; FORWARD (p)) {
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
/* Store position so we need not search again. */
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
  for (; p != NULL; FORWARD (p)) {
    if (GENIE (p) != NULL && GENIE (p)->whether_new_lexical_level) {
/* Don't enter a new lexical level - it will have its own initialisation. */
      return;
    } else if (WHETHER (p, PROCEDURE_DECLARATION) || WHETHER (p, BRIEF_OPERATOR_DECLARATION)) {
      genie_init_proc_op (SUB (p), &(SEQUENCE (SYMBOL_TABLE (p))), count);
      return;
    } else {
      genie_find_proc_op (SUB (p), count);
    }
  }
}

void initialise_frame (NODE_T * p)
{
  if (SYMBOL_TABLE (p)->initialise_anon) {
    TAG_T *_a_;
    SYMBOL_TABLE (p)->initialise_anon = A68_FALSE;
    for (_a_ = SYMBOL_TABLE (p)->anonymous; _a_ != NULL; FORWARD (_a_)) {
      if (PRIO (_a_) == ROUTINE_TEXT) {
        int youngest = TAX (NODE (_a_))->youngest_environ;
        A68_PROCEDURE *_z_ = (A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (_a_)));
        STATUS (_z_) = INITIALISED_MASK;
        (BODY (_z_)).node = NODE (_a_);
        if (youngest > 0) {
          STATIC_LINK_FOR_FRAME (ENVIRON (_z_), 1 + youngest);
        } else {
          ENVIRON (_z_) = 0;
        }
        LOCALE (_z_) = NULL;
        MOID (_z_) = MOID (_a_);
        SYMBOL_TABLE (p)->initialise_anon = A68_TRUE;
      } else if (PRIO (_a_) == FORMAT_TEXT) {
        int youngest = TAX (NODE (_a_))->youngest_environ;
        A68_FORMAT *_z_ = (A68_FORMAT *) (FRAME_OBJECT (OFFSET (_a_)));
        STATUS (_z_) = INITIALISED_MASK;
        BODY (_z_) = NODE (_a_);
        if (youngest > 0) {
          STATIC_LINK_FOR_FRAME (ENVIRON (_z_), 1 + youngest);
        } else {
          ENVIRON (_z_) = 0;
        }
        SYMBOL_TABLE (p)->initialise_anon = A68_TRUE;
      }
    }
  }
  if (SYMBOL_TABLE (p)->proc_ops) {
    NODE_T *_q_;
    ADDR_T pop_sp;
    if (SEQUENCE (SYMBOL_TABLE (p)) == NULL) {
      int count = 0;
      genie_find_proc_op (p, &count);
      SYMBOL_TABLE (p)->proc_ops = (BOOL_T) (count > 0);
    }
    pop_sp = stack_pointer;
    for (_q_ = SEQUENCE (SYMBOL_TABLE (p)); _q_ != NULL; _q_ = SEQUENCE (_q_)) {
      NODE_T *u = NEXT_NEXT (_q_);
      if (WHETHER (u, ROUTINE_TEXT)) {
        NODE_T *src = (PROPAGATOR (u)).source;
        *(A68_PROCEDURE *) FRAME_OBJECT (OFFSET (TAX (_q_))) = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (src))));
      } else if ((WHETHER (u, UNIT) && WHETHER (SUB (u), ROUTINE_TEXT))) {
        NODE_T *src = (PROPAGATOR (SUB (u))).source;
        *(A68_PROCEDURE *) FRAME_OBJECT (OFFSET (TAX (_q_))) = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (src))));
      }
    }
  }
  SYMBOL_TABLE (p)->initialise_frame = (BOOL_T) (SYMBOL_TABLE (p)->initialise_anon || SYMBOL_TABLE (p)->proc_ops);
}

/*!
\brief dynamic scope check
\param p position in tree
\param m mode of object
\param w pointer to object
\param limit limiting frame pointer
\param info info for diagnostics
**/

void genie_dns_addr (NODE_T * p, MOID_T * m, BYTE_T * w, ADDR_T limit, char *info)
{
  if (m != NULL && w != NULL) {
    ADDR_T _limit_2 = (limit < global_pointer ? global_pointer : limit);
    if (WHETHER (m, REF_SYMBOL)) {
      SCOPE_CHECK (p, (GET_REF_SCOPE ((A68_REF *) w)), _limit_2, m, info);
    } else if (WHETHER (m, UNION_SYMBOL)) {
      genie_dns_addr (p, (MOID_T *) VALUE ((A68_UNION *) w), &(w[ALIGNED_SIZE_OF (A68_UNION)]), _limit_2, "united value");
    } else if (WHETHER (m, PROC_SYMBOL)) {
      A68_PROCEDURE *v = (A68_PROCEDURE *) w;
      SCOPE_CHECK (p, ENVIRON (v), _limit_2, m, info);
      if (LOCALE (v) != NULL) {
        BYTE_T *u = POINTER (LOCALE (v));
        PACK_T *s = PACK (MOID (v));
        for (; s != NULL; FORWARD (s)) {
          if (VALUE ((A68_BOOL *) & u[0]) == A68_TRUE) {
            genie_dns_addr (p, MOID (s), &u[ALIGNED_SIZE_OF (A68_BOOL)], _limit_2, "partial parameter value");
          }
          u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
        }
      }
    } else if (WHETHER (m, FORMAT_SYMBOL)) {
      SCOPE_CHECK (p, ((A68_FORMAT *) w)->environ, _limit_2, m, info);
    }
  }
}

#undef SCOPE_CHECK

/*!
\brief whether item at "w" of mode "q" is initialised
\param p position in tree
\param w pointer to object
\param q mode of object
**/

void genie_check_initialisation (NODE_T * p, BYTE_T * w, MOID_T * q)
{
  switch (q->short_id) {
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
      MP_DIGIT_T *z = (MP_DIGIT_T *) w;
      CHECK_INIT (p, (unsigned) z[0] & INITIALISED_MASK, q);
      return;
    }
  case MODE_LONG_COMPLEX:
    {
      MP_DIGIT_T *r = (MP_DIGIT_T *) w;
      MP_DIGIT_T *i = (MP_DIGIT_T *) (w + size_long_mp ());
      CHECK_INIT (p, (unsigned) r[0] & INITIALISED_MASK, q);
      CHECK_INIT (p, (unsigned) i[0] & INITIALISED_MASK, q);
      return;
    }
  case MODE_LONGLONG_COMPLEX:
    {
      MP_DIGIT_T *r = (MP_DIGIT_T *) w;
      MP_DIGIT_T *i = (MP_DIGIT_T *) (w + size_longlong_mp ());
      CHECK_INIT (p, (unsigned) r[0] & INITIALISED_MASK, q);
      CHECK_INIT (p, (unsigned) i[0] & INITIALISED_MASK, q);
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
      A68_REF *pipe_write = (A68_REF *) (w + ALIGNED_SIZE_OF (A68_REF));
      A68_INT *pid = (A68_INT *) (w + 2 * ALIGNED_SIZE_OF (A68_REF));
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

PROPAGATOR_T genie_constant (NODE_T * p)
{
  PUSH (p, GENIE (p)->constant, GENIE (p)->size);
  return (PROPAGATOR (p));
}

/*!
\brief unite value in the stack and push result
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_uniting (NODE_T * p)
{
  PROPAGATOR_T self;
  ADDR_T sp = stack_pointer;
  MOID_T *u = MOID (p), *v = MOID (SUB (p));
  int size = MOID_SIZE (u);
  if (ATTRIBUTE (v) != UNION_SYMBOL) {
    PUSH_UNION (p, (void *) unites_to (v, u));
    EXECUTE_UNIT (SUB (p));
  } else {
    A68_UNION *m = (A68_UNION *) STACK_TOP;
    EXECUTE_UNIT (SUB (p));
    VALUE (m) = (void *) unites_to ((MOID_T *) VALUE (m), u);
  }
  stack_pointer = sp + size;
  self.unit = genie_uniting;
  self.source = p;
  return (self);
}

/*!
\brief store widened constant as a constant
\param p position in tree
\param m mode of object
\param self propagator to set
**/

static void make_constant_widening (NODE_T * p, MOID_T * m, PROPAGATOR_T * self)
{
  if (SUB (p) != NULL && GENIE (SUB (p))->constant != NULL) {
    int size = MOID_SIZE (m);
    self->unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((unsigned) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, (void *) (STACK_OFFSET (-size)), size);
  }
}

/*!
\brief (optimised) push INT widened to REAL
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_widening_int_to_real (NODE_T * p)
{
  A68_INT *i = (A68_INT *) STACK_TOP;
  A68_REAL *z = (A68_REAL *) STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REAL) - ALIGNED_SIZE_OF (A68_INT));
  VALUE (z) = (double) VALUE (i);
  STATUS (z) = INITIALISED_MASK;
  return (PROPAGATOR (p));
}

/*!
\brief widen value in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_widening (NODE_T * p)
{
#define COERCE_FROM_TO(p, a, b) (MOID (p) == (b) && MOID (SUB (p)) == (a))
  PROPAGATOR_T self;
  self.unit = genie_widening;
  self.source = p;
/* INT widenings. */
  if (COERCE_FROM_TO (p, MODE (INT), MODE (REAL))) {
    (void) genie_widening_int_to_real (p);
    self.unit = genie_widening_int_to_real;
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
/* 1-1 mapping. */
    make_constant_widening (p, MODE (LONG_REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONGLONG_INT), MODE (LONGLONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
/* 1-1 mapping. */
    make_constant_widening (p, MODE (LONGLONG_REAL), &self);
  }
/* REAL widenings. */
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
    MP_DIGIT_T *z;
    int digits = get_mp_digits (MODE (LONG_REAL));
    EXECUTE_UNIT (SUB (p));
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
    make_constant_widening (p, MODE (LONG_COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX))) {
    MP_DIGIT_T *z;
    int digits = get_mp_digits (MODE (LONGLONG_REAL));
    EXECUTE_UNIT (SUB (p));
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
    make_constant_widening (p, MODE (LONGLONG_COMPLEX), &self);
  }
/* COMPLEX widenings. */
  else if (COERCE_FROM_TO (p, MODE (COMPLEX), MODE (LONG_COMPLEX))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_complex_to_long_complex (p);
    make_constant_widening (p, MODE (LONG_COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_COMPLEX), MODE (LONGLONG_COMPLEX))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_complex_to_longlong_complex (p);
    make_constant_widening (p, MODE (LONGLONG_COMPLEX), &self);
  }
/* BITS widenings. */
  else if (COERCE_FROM_TO (p, MODE (BITS), MODE (LONG_BITS))) {
    EXECUTE_UNIT (SUB (p));
/* Treat unsigned as int, but that's ok. */
    genie_lengthen_int_to_long_mp (p);
    make_constant_widening (p, MODE (LONG_BITS), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_BITS), MODE (LONGLONG_BITS))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_mp_to_longlong_mp (p);
    make_constant_widening (p, MODE (LONGLONG_BITS), &self);
  }
/* Miscellaneous widenings. */
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
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_BOOL), BITS_WIDTH * MOID_SIZE (MODE (BOOL)));
    DIM (&arr) = 1;
    MOID (&arr) = MODE (BOOL);
    arr.elem_size = MOID_SIZE (MODE (BOOL));
    arr.slice_offset = 0;
    arr.field_offset = 0;
    ARRAY (&arr) = row;
    LWB (&tup) = 1;
    UPB (&tup) = BITS_WIDTH;
    tup.shift = LWB (&tup);
    tup.span = 1;
    tup.k = 0;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = ADDRESS (&row) + MOID_SIZE (MODE (BOOL)) * (BITS_WIDTH - 1);
    bit = 1;
    for (k = BITS_WIDTH - 1; k >= 0; k--, base -= MOID_SIZE (MODE (BOOL)), bit <<= 1) {
      STATUS ((A68_BOOL *) base) = INITIALISED_MASK;
      VALUE ((A68_BOOL *) base) = (BOOL_T) ((VALUE (&x) & bit) != 0 ? A68_TRUE : A68_FALSE);
    }
    PUSH_REF (p, z);
    UNPROTECT_SWEEP_HANDLE (&z);
    PROTECT_FROM_SWEEP_STACK (p);
  } else if (COERCE_FROM_TO (p, MODE (LONG_BITS), MODE (ROW_BOOL)) || COERCE_FROM_TO (p, MODE (LONGLONG_BITS), MODE (ROW_BOOL))) {
    MOID_T *m = MOID (SUB (p));
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int size = get_mp_size (m), k, width = get_mp_bits_width (m), words = get_mp_bits_words (m);
    unsigned *bits;
    BYTE_T *base;
    MP_DIGIT_T *x;
    ADDR_T pop_sp = stack_pointer;
/* Calculate and convert BITS value. */
    EXECUTE_UNIT (SUB (p));
    x = (MP_DIGIT_T *) STACK_OFFSET (-size);
    bits = stack_mp_bits (p, x, m);
/* Make [] BOOL. */
    z = heap_generator (p, MODE (ROW_BOOL), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_BOOL), width * MOID_SIZE (MODE (BOOL)));
    DIM (&arr) = 1;
    MOID (&arr) = MODE (BOOL);
    arr.elem_size = MOID_SIZE (MODE (BOOL));
    arr.slice_offset = 0;
    arr.field_offset = 0;
    ARRAY (&arr) = row;
    LWB (&tup) = 1;
    UPB (&tup) = width;
    tup.shift = LWB (&tup);
    tup.span = 1;
    tup.k = 0;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = ADDRESS (&row) + (width - 1) * MOID_SIZE (MODE (BOOL));
    k = width;
    while (k > 0) {
      unsigned bit = 0x1;
      int j;
      for (j = 0; j < MP_BITS_BITS && k >= 0; j++) {
        STATUS ((A68_BOOL *) base) = INITIALISED_MASK;
        VALUE ((A68_BOOL *) base) = (BOOL_T) ((bits[words - 1] & bit) ? A68_TRUE : A68_FALSE);
        base -= MOID_SIZE (MODE (BOOL));
        bit <<= 1;
        k--;
      }
      words--;
    }
    if (GENIE (SUB (p))->constant != NULL) {
      self.unit = genie_constant;
      PROTECT_SWEEP_HANDLE (&z);
      GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_REF));
      GENIE (p)->size = ALIGNED_SIZE_OF (A68_REF);
      COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_REF));
    } else {
      UNPROTECT_SWEEP_HANDLE (&z);
    }
    stack_pointer = pop_sp;
    PUSH_REF (p, z);
    PROTECT_FROM_SWEEP_STACK (p);
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
  NODE_T *label = (WHETHER (q, GOTO_SYMBOL) ? NEXT (q) : q);
  STATUS (&z) = INITIALISED_MASK;
  (BODY (&z)).node = jump;
  STATIC_LINK_FOR_FRAME (ENVIRON (&z), 1 + TAG_LEX_LEVEL (TAX (label)));
  LOCALE (&z) = NULL;
  MOID (&z) = MODE (PROC_VOID);
  PUSH_PROCEDURE (p, z);
}

/*!
\brief (optimised) dereference value of a unit
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereferencing_quick (NODE_T * p)
{
  A68_REF *z = (A68_REF *) STACK_TOP;
  ADDR_T pop_sp = stack_pointer;
  BYTE_T *stack_base = STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  stack_pointer = pop_sp;
  CHECK_REF (p, *z, MOID (SUB (p)));
  PUSH (p, ADDRESS (z), MOID_SIZE (MOID (p)));
  CHECK_INIT_GENERIC (p, stack_base, MOID (p));
  return (PROPAGATOR (p));
}

/*!
\brief dereference an identifier
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_frame_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB_MOID (p);
  BYTE_T *stack_base = STACK_TOP;
  FRAME_GET (z, A68_REF, p);
  PUSH (p, ADDRESS (z),  MOID_SIZE (deref));
  CHECK_INIT_GENERIC (p, stack_base, deref);
  return (PROPAGATOR (p));
}

/*!
\brief dereference an identifier
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_generic_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB_MOID (p);
  BYTE_T *stack_base = STACK_TOP;
  FRAME_GET (z, A68_REF, p);
  CHECK_REF (p, *z, MOID (SUB (p)));
  PUSH (p, ADDRESS (z), MOID_SIZE (deref));
  CHECK_INIT_GENERIC (p, stack_base, deref);
  return (PROPAGATOR (p));
}

/*!
\brief slice REF [] A to A
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *prim = SUB (p);
  A68_ARRAY *a;
  A68_TUPLE *t;
  A68_REF *z;
  MOID_T *ref_mode = MOID (p);
  MOID_T *deref_mode = SUB (ref_mode);
  int size = MOID_SIZE (deref_mode), row_index;
  ADDR_T pop_sp = stack_pointer;
  BYTE_T *stack_base = STACK_TOP;
/* Get REF []. */
  UP_SWEEP_SEMA;
  z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (prim);
  stack_pointer = pop_sp;
  CHECK_REF (p, *z, ref_mode);
  GET_DESCRIPTOR (a, t, (A68_ROW *) ADDRESS (z));
  for (row_index = 0, q = SEQUENCE (p); q != NULL; t++, q = SEQUENCE (q)) {
    A68_INT *j = (A68_INT *) STACK_TOP;
    int k;
    EXECUTE_UNIT (q);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    row_index += (t->span * k - t->shift);
    stack_pointer = pop_sp;
  }
/* Push element. */
  PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, row_index)]), size);
  CHECK_INIT_GENERIC (p, stack_base, deref_mode);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief dereference SELECTION from a name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  MOID_T *result_mode = SUB_MOID (selector);
  int size = MOID_SIZE (result_mode);
  A68_REF *z = (A68_REF *) STACK_TOP;
  ADDR_T pop_sp = stack_pointer;
  EXECUTE_UNIT (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  stack_pointer = pop_sp;
  PUSH (p, ADDRESS (z), size);
/*  PROTECT_FROM_SWEEP_STACK (p); */
  return (PROPAGATOR (p));
}

/*!
\brief dereference name in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereferencing (NODE_T * p)
{
  A68_REF z;
  PROPAGATOR_T self;
  EXECUTE_UNIT_2 (SUB (p), self);
  POP_REF (p, &z);
  CHECK_REF (p, z, MOID (SUB (p)));
  PUSH (p, ADDRESS (&z), MOID_SIZE (MOID (p)));
  CHECK_INIT_GENERIC (p, STACK_OFFSET (-MOID_SIZE (MOID (p))), MOID (p));
  if (self.unit == genie_frame_identifier) {
    if (IS_IN_FRAME (&z)) {
      self.unit = genie_dereference_frame_identifier;
    } else {
      self.unit = genie_dereference_generic_identifier;
    }
    GENIE (self.source)->propagator.unit = self.unit;
  } else if (self.unit == genie_slice_name_quick) {
    self.unit = genie_dereference_slice_name_quick;
    GENIE (self.source)->propagator.unit = self.unit;
  } else if (self.unit == genie_selection_name_quick) {
    self.unit = genie_dereference_selection_name_quick;
    GENIE (self.source)->propagator.unit = self.unit;
  } else {
    self.unit = genie_dereferencing_quick;
    self.source = p;
  }
  return (self);
}

/*!
\brief deprocedure PROC in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_deproceduring (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_PROCEDURE *z;
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  NODE_T *proc = SUB (p);
  MOID_T *proc_mode = MOID (proc);
  self.unit = genie_deproceduring;
  self.source = p;
/* Get procedure. */
  z = (A68_PROCEDURE *) STACK_TOP;
  EXECUTE_UNIT (proc);
  stack_pointer = pop_sp;
  CHECK_INIT_GENERIC (p, (BYTE_T *) z, proc_mode);
  genie_call_procedure (p, proc_mode, proc_mode, MODE (VOID), z, pop_sp, pop_fp);
  PROTECT_FROM_SWEEP_STACK (p);
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "deproceduring");
  return (self);
}

/*!
\brief voiden value in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_voiding (NODE_T * p)
{
  PROPAGATOR_T self, source;
  ADDR_T sp_for_voiding = stack_pointer;
  self.source = p;
  EXECUTE_UNIT_2 (SUB (p), source);
  stack_pointer = sp_for_voiding;
  if (source.unit == genie_assignation_quick) {
    self.unit = genie_voiding_assignation;
    self.source = source.source;
  } else if (source.unit == genie_assignation_constant) {
    self.unit = genie_voiding_assignation_constant;
    self.source = source.source;
  } else {
    self.unit = genie_voiding;
  }
  return (self);
}

/*!
\brief coerce value in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_coercion (NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = genie_coercion;
  self.source = p;
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
  PROPAGATOR (p) = self;
  return (self);
}

/*!
\brief push argument units
\param p position in tree
\param seq chain to link nodes into
**/

static void genie_argument (NODE_T * p, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      EXECUTE_UNIT (p);
      SEQUENCE (*seq) = p;
      (*seq) = p;
      return;
    } else if (WHETHER (p, TRIMMER)) {
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
/* Get locale for the new procedure descriptor. Copy is necessary. */
  if (LOCALE (&z) == NULL) {
    int size = 0;
    for (s = PACK (pr_mode); s != NULL; FORWARD (s)) {
      size += (ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s)));
    }
    ref = heap_generator (p, pr_mode, size);
    loc = REF_HANDLE (&ref);
  } else {
    int size = LOCALE (&z)->size;
    ref = heap_generator (p, pr_mode, size);
    loc = REF_HANDLE (&ref);
    COPY (POINTER (loc), POINTER (LOCALE (&z)), size);
  }
/* Move arguments from stack to locale using pmap. */
  u = POINTER (loc);
  s = PACK (pr_mode);
  v = STACK_ADDRESS (pop_sp);
  t = PACK (pmap);
  for (; t != NULL && s != NULL; FORWARD (t)) {
/* Skip already initialised arguments. */
    while (u != NULL && VALUE ((A68_BOOL *) & u[0])) {
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      FORWARD (s);
    }
    if (u != NULL && MOID (t) == MODE (VOID)) {
/* Move to next field in locale. */
      voids++;
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      FORWARD (s);
    } else {
/* Move argument from stack to locale. */
      A68_BOOL w;
      STATUS (&w) = INITIALISED_MASK;
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
/* Closure is complete. Push locale onto the stack and call procedure body. */
    stack_pointer = pop_sp;
    u = POINTER (loc);
    v = STACK_ADDRESS (stack_pointer);
    s = PACK (pr_mode);
    for (; s != NULL; FORWARD (s)) {
      int size = MOID_SIZE (MOID (s));
      COPY (v, &u[ALIGNED_SIZE_OF (A68_BOOL)], size);
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + size]);
      v = &(v[MOID_SIZE (MOID (s))]);
      INCREMENT_STACK_POINTER (p, size);
    }
    genie_call_procedure (p, pr_mode, pproc, MODE (VOID), &z, pop_sp, pop_fp);
  } else {
/*  Closure is not complete. Return procedure body. */
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
    (void) ((*((BODY (z)).proc)) (p));
  } else if (STATUS (z) & SKIP_PROCEDURE_MASK) {
    stack_pointer = pop_sp;
    genie_push_undefined (p, SUB ((MOID (z))));
  } else {
    NODE_T *body = (BODY (z)).node;
    if (WHETHER (body, ROUTINE_TEXT)) {
      NODE_T *entry = SUB (body);
      PACK_T *args = PACK (pr_mode);
      ADDR_T fp0 = 0;
/* Copy arguments from stack to frame. */
      OPEN_PROC_FRAME (entry, ENVIRON (z));
      INIT_STATIC_FRAME (entry);
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
      for (; args != NULL; FORWARD (args)) {
        int size = MOID_SIZE (MOID (args));
        COPY ((FRAME_OBJECT (fp0)), STACK_ADDRESS (pop_sp + fp0), size);
        fp0 += size;
      }
      stack_pointer = pop_sp;
      GENIE (p)->argsize = fp0;
/* Interpret routine text. */
      if (DIM (pr_mode) > 0) {
/* With PARAMETERS. */
        entry = NEXT (NEXT_NEXT (entry));
      } else {
/* Without PARAMETERS. */
        entry = NEXT_NEXT (entry);
      }
      EXECUTE_UNIT (entry);
      if (frame_pointer == finish_frame_pointer) {
        change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
      }
      CLOSE_FRAME;
      GENIE_DNS_STACK (p, SUB (pr_mode), frame_pointer, "procedure");
    } else {
      OPEN_PROC_FRAME (body, ENVIRON (z));
      INIT_STATIC_FRAME (body);
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
      EXECUTE_UNIT (body);
      if (frame_pointer == finish_frame_pointer) {
        change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
      }
      CLOSE_FRAME;
      GENIE_DNS_STACK (p, SUB (pr_mode), frame_pointer, "procedure");
    }
  }
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_call_standenv_quick (NODE_T * p)
{
  NODE_T *pr = SUB (p), *q = SEQUENCE (p);
  TAG_T *proc = TAX (PROPAGATOR (pr).source);
/* Get arguments. */
  UP_SWEEP_SEMA;
  for (; q != NULL; q = SEQUENCE (q)) {
    EXECUTE_UNIT (q);
  }
  DOWN_SWEEP_SEMA;
  (void) ((*(proc->procedure)) (p));
  return (PROPAGATOR (p));
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_call_quick (NODE_T * p)
{
  A68_PROCEDURE z;
  NODE_T *proc = SUB (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
/* Get procedure. */
  EXECUTE_UNIT (proc);
  POP_OBJECT (proc, &z, A68_PROCEDURE);
  CHECK_INIT_GENERIC (p, (BYTE_T *) &z, MOID (proc));
/* Get arguments. */
  if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GENIE_INFO_T g;
    GENIE (&top_seq) = &g;
    genie_argument (NEXT (proc), &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
  } else {
    NODE_T *q = SEQUENCE (p);
    for (; q != NULL; q = SEQUENCE (q)) {
      EXECUTE_UNIT (q);
    }
  }
  genie_call_procedure (p, MOID (&z), GENIE (proc)->partial_proc, GENIE (proc)->partial_locale, &z, pop_sp, pop_fp);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_call (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_PROCEDURE z;
  NODE_T *proc = SUB (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  self.unit = genie_call_quick;
  self.source = p;
/* Get procedure. */
  EXECUTE_UNIT (proc);
  POP_OBJECT (proc, &z, A68_PROCEDURE);
  CHECK_INIT_GENERIC (p, (BYTE_T *) &z, MOID (proc));
/* Get arguments. */
  if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GENIE_INFO_T g;
    GENIE (&top_seq) = &g;
    genie_argument (NEXT (proc), &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
  } else {
    NODE_T *q = SEQUENCE (p);
    for (; q != NULL; q = SEQUENCE (q)) {
      EXECUTE_UNIT (q);
    }
  }
  genie_call_procedure (p, MOID (&z), GENIE (proc)->partial_proc, GENIE (proc)->partial_locale, &z, pop_sp, pop_fp);
  if (GENIE (proc)->partial_locale != MODE (VOID) && MOID (&z) != GENIE (proc)->partial_locale) {
    /* skip */ ;
  } else if ((STATUS (&z) & STANDENV_PROC_MASK) && (GENIE (p)->protect_sweep == NULL)) {
    if (PROPAGATOR (proc).unit == genie_identifier_standenv_proc) {
      self.unit = genie_call_standenv_quick;
    }
  }
  PROTECT_FROM_SWEEP_STACK (p);
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
  if (p != NULL) {
    if (WHETHER (p, UNIT)) {
      A68_INT k;
      A68_TUPLE *t;
      EXECUTE_UNIT (p);
      POP_OBJECT (p, &k, A68_INT);
      t = (A68_TUPLE *) * ref_old;
      CHECK_INDEX (p, &k, t);
      (*offset) += t->span * VALUE (&k) - t->shift;
      /*
       * (*ref_old) += ALIGNED_SIZE_OF (A68_TUPLE);
       */
      (*ref_old) += sizeof (A68_TUPLE);
    } else if (WHETHER (p, TRIMMER)) {
      A68_INT k;
      NODE_T *q;
      int L, U, D;
      A68_TUPLE *old_tup = (A68_TUPLE *) * ref_old;
      A68_TUPLE *new_tup = (A68_TUPLE *) * ref_new;
/* TRIMMER is (l:u@r) with all units optional or (empty). */
      q = SUB (p);
      if (q == NULL) {
        L = LWB (old_tup);
        U = UPB (old_tup);
        D = 0;
      } else {
        BOOL_T absent = A68_TRUE;
/* Lower index. */
        if (q != NULL && WHETHER (q, UNIT)) {
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
        if (q != NULL && (WHETHER (q, COLON_SYMBOL)
                          || WHETHER (q, DOTDOT_SYMBOL))) {
          FORWARD (q);
          absent = A68_FALSE;
        }
/* Upper index. */
        if (q != NULL && WHETHER (q, UNIT)) {
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
        if (q != NULL && WHETHER (q, AT_SYMBOL)) {
          FORWARD (q);
        }
/* Revised lower bound. */
        if (q != NULL && WHETHER (q, UNIT)) {
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
      new_tup->span = old_tup->span;
      new_tup->shift = old_tup->shift - D * new_tup->span;
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
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        A68_INT *k;
        A68_TUPLE *t = *tup;
        EXECUTE_UNIT (p);
        POP_ADDRESS (p, k, A68_INT);
        CHECK_INDEX (p, k, t);
        (*tup)++;
        (*sum) += (t->span * VALUE (k) - t->shift);
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

PROPAGATOR_T genie_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *pr = SUB (p);
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_ARRAY *a;
  ADDR_T pop_sp, scope;
  A68_TUPLE *t;
  int sindex;
/* Get row and save row from sweeper. */
  UP_SWEEP_SEMA;
  EXECUTE_UNIT (pr);
  CHECK_REF (p, *z, MOID (SUB (p)));
  GET_DESCRIPTOR (a, t, (A68_ROW *) ADDRESS (z));
  pop_sp = stack_pointer;
  for (sindex = 0, q = SEQUENCE (p); q != NULL; t++, q = SEQUENCE (q)) {
    A68_INT *j = (A68_INT *) STACK_TOP;
    int k;
    EXECUTE_UNIT (q);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    sindex += (t->span * k - t->shift);
    stack_pointer = pop_sp;
  }
  DOWN_SWEEP_SEMA;
/* Leave reference to element on the stack, preserving scope. */
  scope = GET_REF_SCOPE (z);
  *z = ARRAY (a);
  OFFSET (z) += ROW_ELEMENT (a, sindex);
  SET_REF_SCOPE (z, scope);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief push slice of a rowed object
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_slice (NODE_T * p)
{
  PROPAGATOR_T self, primary;
  ADDR_T pop_sp, scope = PRIMAL_SCOPE;
  BOOL_T slice_of_name = (BOOL_T) (WHETHER (MOID (SUB (p)), REF_SYMBOL));
  MOID_T *result_moid = slice_of_name ? SUB_MOID (p) : MOID (p);
  NODE_T *indexer = NEXT_SUB (p);
  self.unit = genie_slice;
  self.source = p;
  pop_sp = stack_pointer;
/* Get row. */
  UP_SWEEP_SEMA;
  EXECUTE_UNIT_2 (SUB (p), primary);
/* In case of slicing a REF [], we need the [] internally, so dereference. */
  if (slice_of_name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  if (ANNOTATION (indexer) == SLICE) {
/* SLICING subscripts one element from an array. */
    A68_REF z;
    A68_ARRAY *a;
    A68_TUPLE *t;
    int sindex;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    GET_DESCRIPTOR (a, t, &z);
    if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      GENIE_INFO_T g;
      GENIE (&top_seq) = &g;
      sindex = 0;
      genie_subscript (indexer, &t, &sindex, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      STATUS_SET (p, SEQUENCE_MASK);
    } else {
      NODE_T *q;
      for (sindex = 0, q = SEQUENCE (p); q != NULL; t++, q = SEQUENCE (q)) {
        A68_INT *j = (A68_INT *) STACK_TOP;
        int k;
        EXECUTE_UNIT (q);
        k = VALUE (j);
        if (k < LWB (t) || k > UPB (t)) {
          diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
          exit_genie (q, A68_RUNTIME_ERROR);
        }
        sindex += (t->span * k - t->shift);
      }
    }
/* Slice of a name yields a name. */
    stack_pointer = pop_sp;
    if (slice_of_name) {
      A68_REF name = ARRAY (a);
      name.offset += ROW_ELEMENT (a, sindex);
      SET_REF_SCOPE (&name, scope);
      PUSH_REF (p, name);
      if (STATUS_TEST (p, SEQUENCE_MASK)) {
        self.unit = genie_slice_name_quick;
        self.source = p;
      }
    } else {
      PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, sindex)]), MOID_SIZE (result_moid));
    }
    PROTECT_FROM_SWEEP_STACK (p);
    DOWN_SWEEP_SEMA;
    return (self);
  } else if (ANNOTATION (indexer) == TRIMMER) {
/* Trimming selects a subarray from an array. */
    int offset;
    A68_REF z, ref_desc_copy;
    A68_ARRAY *old_des, *new_des;
    BYTE_T *ref_new, *ref_old;
    ref_desc_copy = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_ARRAY) + DIM (DEFLEX (result_moid)) * ALIGNED_SIZE_OF (A68_TUPLE));
/* Get descriptor. */
    POP_REF (p, &z);
/* Get indexer. */
    CHECK_REF (p, z, MOID (SUB (p)));
    old_des = (A68_ARRAY *) ADDRESS (&z);
    new_des = (A68_ARRAY *) ADDRESS (&ref_desc_copy);
    ref_old = ADDRESS (&z) + ALIGNED_SIZE_OF (A68_ARRAY);
    ref_new = ADDRESS (&ref_desc_copy) + ALIGNED_SIZE_OF (A68_ARRAY);
    DIM (new_des) = DIM (DEFLEX (result_moid));
    MOID (new_des) = MOID (old_des);
    new_des->elem_size = old_des->elem_size;
    offset = old_des->slice_offset;
    genie_trimmer (indexer, &ref_new, &ref_old, &offset);
    new_des->slice_offset = offset;
    new_des->field_offset = old_des->field_offset;
    ARRAY (new_des) = ARRAY (old_des);
/* Trim of a name is a name. */
    if (slice_of_name) {
      A68_REF ref_new2 = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
      *(A68_REF *) ADDRESS (&ref_new2) = ref_desc_copy;
      SET_REF_SCOPE (&ref_new2, scope);
      PUSH_REF (p, ref_new2);
    } else {
      PUSH_REF (p, ref_desc_copy);
    }
    PROTECT_FROM_SWEEP_STACK (p);
    DOWN_SWEEP_SEMA;
    return (self);
  } else {
    ABEND (A68_TRUE, "impossible state in genie_slice", NULL);
    return (self);
  }
}

/*!
\brief push value of denotation
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_denotation (NODE_T * p)
{
  MOID_T *moid = MOID (p);
  PROPAGATOR_T self;
  self.unit = genie_denotation;
  self.source = p;
  if (moid == MODE (INT)) {
/* INT denotation. */
    A68_INT z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    self.unit = genie_constant;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | CONSTANT_MASK);
    GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_INT));
    GENIE (p)->size = ALIGNED_SIZE_OF (A68_INT);
    COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_INT));
    PUSH_PRIMITIVE (p, VALUE ((A68_INT *) (GENIE (p)->constant)), A68_INT);
  } else if (moid == MODE (REAL)) {
/* REAL denotation. */
    A68_REAL z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | CONSTANT_MASK);
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_REAL));
    GENIE (p)->size = ALIGNED_SIZE_OF (A68_REAL);
    COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_REAL));
    PUSH_PRIMITIVE (p, VALUE ((A68_REAL *) (GENIE (p)->constant)), A68_REAL);
  } else if (moid == MODE (LONG_INT) || moid == MODE (LONGLONG_INT)) {
/* [LONG] LONG INT denotation. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (WHETHER (SUB (p), SHORTETY) || WHETHER (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (double) (INITIALISED_MASK | CONSTANT_MASK);
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, z, size);
  } else if (moid == MODE (LONG_REAL) || moid == MODE (LONGLONG_REAL)) {
/* [LONG] LONG REAL denotation. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (WHETHER (SUB (p), SHORTETY) || WHETHER (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (MP_DIGIT_T) (INITIALISED_MASK | CONSTANT_MASK);
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, z, size);
  } else if (moid == MODE (BITS)) {
/* BITS denotation. */
    A68_BITS z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    self.unit = genie_constant;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | CONSTANT_MASK);
    GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_BITS));
    GENIE (p)->size = ALIGNED_SIZE_OF (A68_BITS);
    COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_BITS));
    PUSH_PRIMITIVE (p, VALUE ((A68_BITS *) (GENIE (p)->constant)), A68_BITS);
  } else if (moid == MODE (LONG_BITS) || moid == MODE (LONGLONG_BITS)) {
/* [LONG] LONG BITS denotation. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (WHETHER (SUB (p), SHORTETY) || WHETHER (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (MP_DIGIT_T) (INITIALISED_MASK | CONSTANT_MASK);
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, z, size);
  } else if (moid == MODE (BOOL)) {
/* BOOL denotation. */
    A68_BOOL z;
    ASSERT (genie_string_to_value_internal (p, MODE (BOOL), SYMBOL (p), (BYTE_T *) & z) == A68_TRUE);
    PUSH_PRIMITIVE (p, VALUE (&z), A68_BOOL);
  } else if (moid == MODE (CHAR)) {
/* CHAR denotation. */
    PUSH_PRIMITIVE (p, TO_UCHAR (SYMBOL (p)[0]), A68_CHAR);
  } else if (moid == MODE (ROW_CHAR)) {
/* [] CHAR denotation. */
/* Make a permanent string in the heap. */
    A68_REF z;
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    z = c_to_a_string (p, SYMBOL (p));
    GET_DESCRIPTOR (arr, tup, &z);
    PROTECT_SWEEP_HANDLE (&z);
    PROTECT_SWEEP_HANDLE (&(ARRAY (arr)));
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_REF));
    GENIE (p)->size = ALIGNED_SIZE_OF (A68_REF);
    COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_REF));
    PUSH_REF (p, *(A68_REF *) (GENIE (p)->constant));
  } else if (moid == MODE (VOID)) {
/* VOID denotation: EMPTY. */
    ;
  }
  return (self);
}

/*!
\brief push a local identifier
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_frame_identifier (NODE_T * p)
{
  BYTE_T *z;
  FRAME_GET (z, BYTE_T, p);
  PUSH (p, z, MOID_SIZE (MOID (p)));
  return (PROPAGATOR (p));
}

/*!
\brief push standard environ routine as PROC
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier_standenv_proc (NODE_T * p)
{
  A68_PROCEDURE z;
  TAG_T *q = TAX (p);
  STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | STANDENV_PROC_MASK);
  (BODY (&z)).proc = q->procedure;
  ENVIRON (&z) = 0;
  LOCALE (&z) = NULL;
  MOID (&z) = MOID (p);
  PUSH_PROCEDURE (p, z);
  return (PROPAGATOR (p));
}

/*!
\brief (optimised) push identifier from standard environ
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier_standenv (NODE_T * p)
{
  (void) ((*(TAX (p)->procedure)) (p));
  return (PROPAGATOR (p));
}

/*!
\brief push identifier onto the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier (NODE_T * p)
{
  static PROPAGATOR_T self;
  TAG_T *q = TAX (p);
  self.source = p;
  if (q->stand_env_proc) {
    if (WHETHER (MOID (q), PROC_SYMBOL)) {
      (void) genie_identifier_standenv_proc (p);
      self.unit = genie_identifier_standenv_proc;
    } else {
      (void) genie_identifier_standenv (p);
      self.unit = genie_identifier_standenv;
    }
  } else if (STATUS_TEST (q, CONSTANT_MASK)) {
    int size = MOID_SIZE (MOID (p));
    BYTE_T *sp_0 = STACK_TOP;
    (void) genie_frame_identifier (p);
    GENIE (p)->constant = (void *) get_heap_space ((size_t) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, (void *) sp_0, size);
    self.unit = genie_constant;
  } else {
    (void) genie_frame_identifier (p);
    self.unit = genie_frame_identifier;
  }
  return (self);
}

/*!
\brief push result of cast (coercions are deeper in the tree)
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_cast (NODE_T * p)
{
  PROPAGATOR_T self;
  EXECUTE_UNIT (NEXT_SUB (p));
  self.unit = genie_cast;
  self.source = p;
  return (self);
}

/*!
\brief execute assertion
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_assertion (NODE_T * p)
{
  PROPAGATOR_T self;
  if (STATUS_TEST (p, ASSERT_MASK)) {
    A68_BOOL z;
    EXECUTE_UNIT (NEXT_SUB (p));
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FALSE_ASSERTION);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  self.unit = genie_assertion;
  self.source = p;
  return (self);
}

/*!
\brief push format text
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_format_text (NODE_T * p)
{
  static PROPAGATOR_T self;
  A68_FORMAT z = *(A68_FORMAT *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_FORMAT (p, z);
  self.unit = genie_format_text;
  self.source = p;
  return (self);
}

/*!
\brief SELECTION from a value
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_selection_value_quick (NODE_T * p)
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
  }
  INCREMENT_STACK_POINTER (selector, size);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief SELECTION from a name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief push selection from secondary
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_selection (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  PROPAGATOR_T self;
  MOID_T *struct_mode = MOID (NEXT (selector)), *result_mode = MOID (selector);
  BOOL_T selection_of_name = (BOOL_T) (WHETHER (struct_mode, REF_SYMBOL));
  self.source = p;
  self.unit = genie_selection;
  EXECUTE_UNIT (NEXT (selector));
/* Multiple selections. */
  if (selection_of_name && (WHETHER (SUB (struct_mode), FLEX_SYMBOL) || WHETHER (SUB (struct_mode), ROW_SYMBOL))) {
    A68_REF *row1, row2, row3;
    int dims, desc_size;
    UP_SWEEP_SEMA;
    POP_ADDRESS (selector, row1, A68_REF);
    CHECK_REF (p, *row1, struct_mode);
    row1 = (A68_REF *) ADDRESS (row1);
    dims = DIM (DEFLEX (SUB (struct_mode)));
    desc_size = ALIGNED_SIZE_OF (A68_ARRAY) + dims * ALIGNED_SIZE_OF (A68_TUPLE);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), (BYTE_T *) ADDRESS (row1), (unsigned) desc_size);
    MOID (((A68_ARRAY *) ADDRESS (&row2))) = SUB_SUB (result_mode);
    ((A68_ARRAY *) ADDRESS (&row2))->field_offset += OFFSET (NODE_PACK (SUB (selector)));
    row3 = heap_generator (selector, result_mode, ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&row3) = row2;
    PUSH_REF (selector, row3);
    self.unit = genie_selection;
    DOWN_SWEEP_SEMA;
    PROTECT_FROM_SWEEP_STACK (p);
  } else if (struct_mode != NULL && (WHETHER (struct_mode, FLEX_SYMBOL) || WHETHER (struct_mode, ROW_SYMBOL))) {
    A68_REF *row1, row2;
    int dims, desc_size;
    UP_SWEEP_SEMA;
    POP_ADDRESS (selector, row1, A68_REF);
    dims = DIM (DEFLEX (struct_mode));
    desc_size = ALIGNED_SIZE_OF (A68_ARRAY) + dims * ALIGNED_SIZE_OF (A68_TUPLE);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), (BYTE_T *) ADDRESS (row1), (unsigned) desc_size);
    MOID (((A68_ARRAY *) ADDRESS (&row2))) = SUB (result_mode);
    ((A68_ARRAY *) ADDRESS (&row2))->field_offset += OFFSET (NODE_PACK (SUB (selector)));
    PUSH_REF (selector, row2);
    self.unit = genie_selection;
    DOWN_SWEEP_SEMA;
    PROTECT_FROM_SWEEP_STACK (p);
  }
/* Normal selections. */
  else if (selection_of_name && WHETHER (SUB (struct_mode), STRUCT_SYMBOL)) {
    A68_REF *z = (A68_REF *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REF)));
    CHECK_REF (selector, *z, struct_mode);
    OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
    self.unit = genie_selection_name_quick;
    PROTECT_FROM_SWEEP_STACK (p);
  } else if (WHETHER (struct_mode, STRUCT_SYMBOL)) {
    DECREMENT_STACK_POINTER (selector, MOID_SIZE (struct_mode));
    MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (SUB (selector)))), (unsigned) MOID_SIZE (result_mode));
    INCREMENT_STACK_POINTER (selector, MOID_SIZE (result_mode));
    self.unit = genie_selection_value_quick;
    PROTECT_FROM_SWEEP_STACK (p);
  }
  return (self);
}

/*!
\brief push selection from primary
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_field_selection (NODE_T * p)
{
  PROPAGATOR_T self;
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  NODE_T *entry = p;
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_PROCEDURE *w = (A68_PROCEDURE *) STACK_TOP;
  self.source = entry;
  self.unit = genie_field_selection;
  EXECUTE_UNIT (SUB (p));
  for (p = SEQUENCE (SUB (p)); p != NULL; p = SEQUENCE (p)) {
    BOOL_T coerce = A68_TRUE;
    MOID_T *m = MOID (p);
    MOID_T *result_mode = MOID (NODE_PACK (p));
    while (coerce) {
      if (WHETHER (m, REF_SYMBOL) && WHETHER_NOT (SUB (m), STRUCT_SYMBOL)) {
        int size = MOID_SIZE (SUB (m));
        stack_pointer = pop_sp;
        CHECK_REF (p, *z, m);
        PUSH (p, ADDRESS (z), size);
        CHECK_INIT_GENERIC (p, STACK_OFFSET (-size), MOID (p));
        m = SUB (m);
      } else if (WHETHER (m, PROC_SYMBOL)) {
        CHECK_INIT_GENERIC (p, (BYTE_T *) w, m);
        genie_call_procedure (p, m, m, MODE (VOID), w, pop_sp, pop_fp);
        GENIE_DNS_STACK (p, MOID (p), frame_pointer, "deproceduring");
        m = SUB (m);
      } else {
        coerce = A68_FALSE;
      }
    }
    if (WHETHER (m, REF_SYMBOL) && WHETHER (SUB (m), STRUCT_SYMBOL)) {
      CHECK_REF (p, *z, m);
      OFFSET (z) += OFFSET (NODE_PACK (p));
    } else if (WHETHER (m, STRUCT_SYMBOL)) {
      stack_pointer = pop_sp;
      MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (p))), (unsigned) MOID_SIZE (result_mode));
      INCREMENT_STACK_POINTER (p, MOID_SIZE (result_mode));
    }
  }
  PROTECT_FROM_SWEEP_STACK (entry);
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
}

/*!
\brief push result of monadic formula OP "u"
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_monadic (NODE_T * p)
{
  NODE_T *op = SUB (p);
  NODE_T *u = NEXT (op);
  PROPAGATOR_T self;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (u);
  if (TAX (op)->procedure != NULL) {
    (void) ((*(TAX (op)->procedure)) (op));
  } else {
    genie_call_operator (op, sp);
  }
  PROTECT_FROM_SWEEP_STACK (p);
  self.unit = genie_monadic;
  self.source = p;
  return (self);
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dyadic_quick (NODE_T * p)
{
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  NODE_T *v = NEXT (op);
  EXECUTE_UNIT (u);
  EXECUTE_UNIT (v);
  (void) ((*(TAX (op)->procedure)) (op));
  return (PROPAGATOR (p));
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dyadic (NODE_T * p)
{
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  NODE_T *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  EXECUTE_UNIT (u);
  EXECUTE_UNIT (v);
  if (TAX (op)->procedure != NULL) {
    (void) ((*(TAX (op)->procedure)) (op));
  } else {
    genie_call_operator (op, pop_sp);
  }
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula (NODE_T * p)
{
  PROPAGATOR_T self, lhs, rhs;
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  ADDR_T pop_sp = stack_pointer;
  self.unit = genie_formula;
  self.source = p;
  EXECUTE_UNIT_2 (u, lhs);
  if (op != NULL) {
    NODE_T *v = NEXT (op);
    GENIE_PROCEDURE *proc = TAX (op)->procedure;
    EXECUTE_UNIT_2 (v, rhs);
    self.unit = genie_dyadic;
    if (proc != NULL) {
      (void) ((*(proc)) (op));
      if (GENIE (p)->protect_sweep == NULL) {
        self.unit = genie_dyadic_quick;
      }
    } else {
      genie_call_operator (op, pop_sp);
    }
    PROTECT_FROM_SWEEP_STACK (p);
    return (self);
  } else if (lhs.unit == genie_monadic) {
    return (lhs);
  }
  return (self);
}

/*!
\brief push NIL
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_nihil (NODE_T * p)
{
  PROPAGATOR_T self;
  PUSH_REF (p, nil_ref);
  self.unit = genie_nihil;
  self.source = p;
  return (self);
}

/*!
\brief copies union with stowed components on top of the stack
\param p position in tree
**/

static void genie_pop_union (NODE_T * p)
{
  A68_UNION *u = (A68_UNION *) STACK_TOP;
  MOID_T *v = (MOID_T *) VALUE (u);
  if (v != NULL) {
    int v_size = MOID_SIZE (v);
    INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
    if (WHETHER (v, STRUCT_SYMBOL)) {
      A68_REF old, new_one;
      STATUS (&old) = (STATUS_MASK) (INITIALISED_MASK | IN_STACK_MASK);
      old.offset = stack_pointer;
      REF_HANDLE (&old) = &nil_handle;
      new_one = genie_copy_stowed (old, p, v);
      MOVE (STACK_TOP, ADDRESS (&old), v_size);
    } else if (WHETHER (v, ROW_SYMBOL) || WHETHER (v, FLEX_SYMBOL)) {
      A68_REF new_one, old = *(A68_REF *) STACK_TOP;
      new_one = genie_copy_stowed (old, p, v);
      MOVE (STACK_TOP, &new_one, ALIGNED_SIZE_OF (A68_REF));
    }
    DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  }
}

/*!
\brief copy a sound value, make new copy of sound data
\param p position in tree
\param dst destination
\param src source
**/

void genie_copy_sound (NODE_T * p, BYTE_T * dst, BYTE_T * src)
{
  A68_SOUND *w = (A68_SOUND *) dst;
  BYTE_T *wdata;
  int size = A68_SOUND_DATA_SIZE (w);
  COPY (dst, src, MOID_SIZE (MODE (SOUND)));
  wdata = ADDRESS (&(w->data));
  w->data = heap_generator (p, MODE (SOUND_DATA), size);
  COPY (wdata, ADDRESS (&(w->data)), size);
}

/*!
\brief internal workings of an assignment of stowed objects
\param p position in tree
\param z fat A68 pointer
\param src_mode
**/

static void genie_assign_internal (NODE_T * p, A68_REF * z, MOID_T * src_mode)
{
  if (WHETHER (src_mode, FLEX_SYMBOL) || src_mode == MODE (STRING)) {
/* Assign to FLEX [] AMODE. */
    A68_REF old_one = *(A68_REF *) STACK_TOP;
    *(A68_REF *) ADDRESS (z) = genie_copy_stowed (old_one, p, src_mode);
  } else if (WHETHER (src_mode, ROW_SYMBOL)) {
/* Assign to [] AMODE. */
    A68_REF old_one, dst_one;
    A68_ARRAY *dst_arr, *old_arr;
    A68_TUPLE *dst_tup, *old_tup;
    old_one = *(A68_REF *) STACK_TOP;
    dst_one = *(A68_REF *) ADDRESS (z);
    GET_DESCRIPTOR (dst_arr, dst_tup, &dst_one);
    GET_DESCRIPTOR (old_arr, old_tup, &old_one);
    if (ADDRESS (&(ARRAY (dst_arr))) != ADDRESS (&(ARRAY (old_arr))) && !(src_mode->slice->has_rows)) {
      (void) genie_assign_stowed (old_one, &dst_one, p, src_mode);
    } else {
      A68_REF new_one = genie_copy_stowed (old_one, p, src_mode);
      (void) genie_assign_stowed (new_one, &dst_one, p, src_mode);
    }
  } else if (WHETHER (src_mode, STRUCT_SYMBOL)) {
/* STRUCT with row. */
    A68_REF old_one, new_one;
    STATUS (&old_one) = (STATUS_MASK) (INITIALISED_MASK | IN_STACK_MASK);
    old_one.offset = stack_pointer;
    REF_HANDLE (&old_one) = &nil_handle;
    new_one = genie_copy_stowed (old_one, p, src_mode);
    (void) genie_assign_stowed (new_one, z, p, src_mode);
  } else if (WHETHER (src_mode, UNION_SYMBOL)) {
/* UNION with stowed. */
    genie_pop_union (p);
    COPY (ADDRESS (z), STACK_TOP, MOID_SIZE (src_mode));
  } else if (src_mode == MODE (SOUND)) {
    genie_copy_sound (p, ADDRESS (z), STACK_TOP);
  }
}

/*!
\brief assign a value to a name and voiden
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_voiding_assignation_constant (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = (GENIE (NEXT_NEXT (dst))->propagator).source;
  ADDR_T pop_sp = stack_pointer;
  A68_REF *z = (A68_REF *) STACK_TOP;
  PROPAGATOR_T self;
  self.unit = genie_voiding_assignation_constant;
  self.source = p;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  COPY (ADDRESS (z), GENIE (src)->constant, GENIE (src)->size);
  stack_pointer = pop_sp;
  return (self);
}

/*!
\brief assign a value to a name and voiden
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_voiding_assignation (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = FRAME_DYNAMIC_SCOPE (frame_pointer);
  A68_REF z;
  PROPAGATOR_T self;
  self.unit = genie_voiding_assignation;
  self.source = p;
  UP_SWEEP_SEMA;
  EXECUTE_UNIT (dst);
  POP_OBJECT (p, &z, A68_REF);
  CHECK_REF (p, z, MOID (p));
  FRAME_DYNAMIC_SCOPE (frame_pointer) = GET_REF_SCOPE (&z);
  EXECUTE_UNIT (src);
  GENIE_DNS_STACK (src, src_mode, GET_REF_SCOPE (&z), "assignation");
  FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
  stack_pointer = pop_sp;
  if (src_mode->has_rows) {
    genie_assign_internal (p, &z, src_mode);
  } else {
    COPY_ALIGNED (ADDRESS (&z), STACK_TOP, MOID_SIZE (src_mode));
  }
  DOWN_SWEEP_SEMA;
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_assignation_constant (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = (GENIE (NEXT_NEXT (dst))->propagator).source;
  A68_REF *z = (A68_REF *) STACK_TOP;
  PROPAGATOR_T self;
  self.unit = genie_assignation_constant;
  self.source = p;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  COPY (ADDRESS (z), GENIE (src)->constant, GENIE (src)->size);
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_assignation_quick (NODE_T * p)
{
  PROPAGATOR_T self;
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (p);
  int size = MOID_SIZE (src_mode);
  ADDR_T pop_fp = FRAME_DYNAMIC_SCOPE (frame_pointer);
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DYNAMIC_SCOPE (frame_pointer) = GET_REF_SCOPE (z);
  EXECUTE_UNIT (src);
  GENIE_DNS_STACK (src, src_mode, GET_REF_SCOPE (z), "assignation");
  FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (src_mode->has_rows) {
    genie_assign_internal (p, z, src_mode);
  } else {
    COPY (ADDRESS (z), STACK_TOP, size);
  }
  self.unit = genie_assignation_quick;
  self.source = p;
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_assignation (NODE_T * p)
{
  PROPAGATOR_T self, srp;
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *src_mode = SUB_MOID (p);
  int size = MOID_SIZE (src_mode);
  ADDR_T pop_fp = FRAME_DYNAMIC_SCOPE (frame_pointer);
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DYNAMIC_SCOPE (frame_pointer) = GET_REF_SCOPE (z);
  EXECUTE_UNIT_2 (src, srp);
  GENIE_DNS_STACK (src, src_mode, GET_REF_SCOPE (z), "assignation");
  FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (src_mode->has_rows) {
    genie_assign_internal (p, z, src_mode);
  } else {
    COPY (ADDRESS (z), STACK_TOP, size);
  }
  if (srp.unit == genie_constant) {
    self.unit = genie_assignation_constant;
  } else {
    self.unit = genie_assignation_quick;
  }
  self.source = p;
  return (self);
}

/*!
\brief push equality of two REFs
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identity_relation (NODE_T * p)
{
  PROPAGATOR_T self;
  NODE_T *lhs = SUB (p), *rhs = NEXT_NEXT (lhs);
  A68_REF x, y;
  self.unit = genie_identity_relation;
  self.source = p;
  EXECUTE_UNIT (lhs);
  POP_REF (p, &y);
  EXECUTE_UNIT (rhs);
  POP_REF (p, &x);
  if (WHETHER (NEXT_SUB (p), IS_SYMBOL)) {
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

PROPAGATOR_T genie_and_function (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_BOOL x;
  EXECUTE_UNIT (SUB (p));
  POP_OBJECT (p, &x, A68_BOOL);
  if (VALUE (&x) == A68_TRUE) {
    EXECUTE_UNIT (NEXT_NEXT (SUB (p)));
  } else {
    PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
  }
  self.unit = genie_and_function;
  self.source = p;
  return (self);
}

/*!
\brief push result of ORF
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_or_function (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_BOOL x;
  EXECUTE_UNIT (SUB (p));
  POP_OBJECT (p, &x, A68_BOOL);
  if (VALUE (&x) == A68_FALSE) {
    EXECUTE_UNIT (NEXT_NEXT (SUB (p)));
  } else {
    PUSH_PRIMITIVE (p, A68_TRUE, A68_BOOL);
  }
  self.unit = genie_or_function;
  self.source = p;
  return (self);
}

/*!
\brief push routine text
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_routine_text (NODE_T * p)
{
  static PROPAGATOR_T self;
  A68_PROCEDURE z = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_PROCEDURE (p, z);
  self.unit = genie_routine_text;
  self.source = p;
  return (self);
}

/*!
\brief push an undefined value of the required mode
\param p position in tree
\param u mode of object to push
**/

void genie_push_undefined (NODE_T * p, MOID_T * u)
{
/* For primitive modes we push an initialised value. */
  if (u == MODE (VOID)) {
    /* skip */ ;
  } else if (u == MODE (INT)) {
    PUSH_PRIMITIVE (p, (int) (rng_53_bit () * A68_MAX_INT), A68_INT);
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
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
  } else if (u == MODE (LONG_REAL) || u == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
  } else if (u == MODE (LONG_BITS) || u == MODE (LONGLONG_BITS)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
  } else if (u == MODE (LONG_COMPLEX) || u == MODE (LONGLONG_COMPLEX)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
  } else if (WHETHER (u, REF_SYMBOL)) {
/* All REFs are NIL. */
    PUSH_REF (p, nil_ref);
  } else if (WHETHER (u, ROW_SYMBOL) || WHETHER (u, FLEX_SYMBOL)) {
/* [] AMODE or FLEX [] AMODE. */
    PUSH_REF (p, empty_row (p, u));
  } else if (WHETHER (u, STRUCT_SYMBOL)) {
/* STRUCT. */
    PACK_T *v;
    for (v = PACK (u); v != NULL; FORWARD (v)) {
      genie_push_undefined (p, MOID (v));
    }
  } else if (WHETHER (u, UNION_SYMBOL)) {
/* UNION. */
    ADDR_T sp = stack_pointer;
    PUSH_UNION (p, MOID (PACK (u)));
    genie_push_undefined (p, MOID (PACK (u)));
    stack_pointer = sp + MOID_SIZE (u);
  } else if (WHETHER (u, PROC_SYMBOL)) {
/* PROC. */
    A68_PROCEDURE z;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | SKIP_PROCEDURE_MASK);
    (BODY (&z)).node = NULL;
    ENVIRON (&z) = 0;
    LOCALE (&z) = NULL;
    MOID (&z) = u;
    PUSH_PROCEDURE (p, z);
  } else if (u == MODE (FORMAT)) {
/* FORMAT etc. - what arbitrary FORMAT could mean anything at all? */
    A68_FORMAT z;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | SKIP_FORMAT_MASK);
    BODY (&z) = NULL;
    ENVIRON (&z) = 0;
    PUSH_FORMAT (p, z);
  } else if (u == MODE (SIMPLOUT)) {
    ADDR_T sp = stack_pointer;
    PUSH_UNION (p, MODE (STRING));
    PUSH_REF (p, c_to_a_string (p, "SKIP"));
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
    STATUS (z) = INITIALISED_MASK;
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

PROPAGATOR_T genie_skip (NODE_T * p)
{
  PROPAGATOR_T self;
  if (MOID (p) != MODE (VOID)) {
    genie_push_undefined (p, MOID (p));
  }
  self.unit = genie_skip;
  self.source = p;
  return (self);
}

/*!
\brief jump to the serial clause where the label is at
\param p position in tree
**/

static void genie_jump (NODE_T * p)
{
/* Stack pointer and frame pointer were saved at target serial clause. */
  NODE_T *jump = SUB (p);
  NODE_T *label = (WHETHER (jump, GOTO_SYMBOL)) ? NEXT (jump) : jump;
  ADDR_T target_frame_pointer = frame_pointer;
  jmp_buf *jump_stat = NULL;
/* Find the stack frame this jump points to. */
  BOOL_T found = A68_FALSE;
  while (target_frame_pointer > 0 && !found) {
    found = (BOOL_T) ((TAG_TABLE (TAX (label)) == SYMBOL_TABLE (FRAME_TREE (target_frame_pointer))) && FRAME_JUMP_STAT (target_frame_pointer) != NULL);
    if (!found) {
      target_frame_pointer = FRAME_STATIC_LINK (target_frame_pointer);
    }
  }
/* Beam us up, Scotty! */
#if defined ENABLE_PAR_CLAUSE
  {
    int curlev = PAR_LEVEL (p), tarlev = PAR_LEVEL (NODE (TAX (label)));
    if (curlev == tarlev) {
/* A jump within the same thread */
      jump_stat = FRAME_JUMP_STAT (target_frame_pointer);
      SYMBOL_TABLE (TAX (label))->jump_to = TAX (label)->unit;
      longjmp (*(jump_stat), 1);
    } else if (curlev > 0 && tarlev == 0) {
/* A jump out of all parallel clauses back into the main program */
      genie_abend_all_threads (p, FRAME_JUMP_STAT (target_frame_pointer), label);
      ABEND (A68_TRUE, "should not return from genie_abend_all_threads", NULL);
    } else {
/* A jump between threads is forbidden in Algol68G */
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_LABEL_IN_PAR_CLAUSE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
#else
  jump_stat = FRAME_JUMP_STAT (target_frame_pointer);
  TAG_TABLE (TAX (label))->jump_to = TAX (label)->unit;
  longjmp (*(jump_stat), 1);
#endif
}

/*!
\brief execute a unit, tertiary, secondary or primary
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_unit (NODE_T * p)
{
  if (GENIE (p)->whether_coercion) {
    program.global_prop = genie_coercion (p);
  } else {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        EXECUTE_UNIT_2 (SUB (p), program.global_prop);
        break;
      }
    case TERTIARY:
    case SECONDARY:
    case PRIMARY:
      {
        program.global_prop = genie_unit (SUB (p));
        break;
      }
/* Ex primary. */
    case ENCLOSED_CLAUSE:
      {
        program.global_prop = genie_enclosed ((volatile NODE_T *) p);
        break;
      }
    case IDENTIFIER:
      {
        program.global_prop = genie_identifier (p);
        break;
      }
    case CALL:
      {
        program.global_prop = genie_call (p);
        break;
      }
    case SLICE:
      {
        program.global_prop = genie_slice (p);
        break;
      }
    case FIELD_SELECTION:
      {
        program.global_prop = genie_field_selection (p);
        break;
      }
    case DENOTATION:
      {
        program.global_prop = genie_denotation (p);
        break;
      }
    case CAST:
      {
        program.global_prop = genie_cast (p);
        break;
      }
    case FORMAT_TEXT:
      {
        program.global_prop = genie_format_text (p);
        break;
      }
/* Ex secondary. */
    case GENERATOR:
      {
        program.global_prop = genie_generator (p);
        break;
      }
    case SELECTION:
      {
        program.global_prop = genie_selection (p);
        break;
      }
/* Ex tertiary. */
    case FORMULA:
      {
        program.global_prop = genie_formula (p);
        break;
      }
    case MONADIC_FORMULA:
      {
        program.global_prop = genie_monadic (p);
        break;
      }
    case NIHIL:
      {
        program.global_prop = genie_nihil (p);
        break;
      }
    case DIAGONAL_FUNCTION:
      {
        program.global_prop = genie_diagonal_function (p);
        break;
      }
    case TRANSPOSE_FUNCTION:
      {
        program.global_prop = genie_transpose_function (p);
        break;
      }
    case ROW_FUNCTION:
      {
        program.global_prop = genie_row_function (p);
        break;
      }
    case COLUMN_FUNCTION:
      {
        program.global_prop = genie_column_function (p);
        break;
      }
/* Ex unit. */
    case ASSIGNATION:
      {
        program.global_prop = genie_assignation (p);
        break;
      }
    case IDENTITY_RELATION:
      {
        program.global_prop = genie_identity_relation (p);
        break;
      }
    case ROUTINE_TEXT:
      {
        program.global_prop = genie_routine_text (p);
        break;
      }
    case SKIP:
      {
        program.global_prop = genie_skip (p);
        break;
      }
    case JUMP:
      {
        program.global_prop.unit = genie_unit;
        program.global_prop.source = p;
        genie_jump (p);
        break;
      }
    case AND_FUNCTION:
      {
        program.global_prop = genie_and_function (p);
        break;
      }
    case OR_FUNCTION:
      {
        program.global_prop = genie_or_function (p);
        break;
      }
    case ASSERTION:
      {
        program.global_prop = genie_assertion (p);
        break;
      }
    }
  }
  return (PROPAGATOR (p) = program.global_prop);
}

/*!
\brief execution of serial clause without labels
\param p position in tree
\param p position in treeop_sp
\param seq chain to link nodes into
**/

void genie_serial_units_no_label (NODE_T * p, int pop_sp, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        EXECUTE_UNIT_TRACE (p);
        SEQUENCE (*seq) = p;
        (*seq) = p;
        return;
      }
    case SEMI_SYMBOL:
      {
/* Voiden the expression stack. */
        stack_pointer = pop_sp;
        SEQUENCE (*seq) = p;
        (*seq) = p;
        break;
      }
    case DECLARATION_LIST:
      {
        genie_declaration (SUB (p));
        SEQUENCE (*seq) = p;
        (*seq) = p;
        return;
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
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        if (*jump_to == NULL) {
          EXECUTE_UNIT_TRACE (p);
        } else if (p == *jump_to) {
/* If we dropped in this clause from a jump then this unit is the target. */
          *jump_to = NULL;
          EXECUTE_UNIT_TRACE (p);
        }
        return;
      }
    case EXIT_SYMBOL:
      {
        if (*jump_to == NULL) {
          longjmp (*exit_buf, 1);
        }
        break;
      }
    case SEMI_SYMBOL:
      {
        if (*jump_to == NULL) {
/* Voiden the expression stack. */
          stack_pointer = pop_sp;
        }
        break;
      }
    default:
      {
        if (WHETHER (p, DECLARATION_LIST) && *jump_to == NULL) {
          genie_declaration (SUB (p));
          return;
        } else {
          genie_serial_units (SUB (p), jump_to, exit_buf, pop_sp);
        }
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
  if (SYMBOL_TABLE (p)->labels == NULL) {
/* No labels in this clause. */
    if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      GENIE_INFO_T g;
      GENIE (&top_seq) = &g;
      genie_serial_units_no_label (SUB (p), stack_pointer, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      STATUS_SET (p, SEQUENCE_MASK);
      STATUS_SET (p, SERIAL_MASK);
      if (SEQUENCE (p) != NULL && SEQUENCE (SEQUENCE (p)) == NULL) {
        STATUS_SET (p, OPTIMAL_MASK);
      }
    } else {
/* A linear list without labels. */
      NODE_T *q;
      ADDR_T pop_sp = stack_pointer;
      STATUS_SET (p, SERIAL_CLAUSE);
      for (q = SEQUENCE (p); q != NULL; q = SEQUENCE (q)) {
        switch (ATTRIBUTE (q)) {
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
        case DECLARATION_LIST:
          {
            genie_declaration (SUB (q));
            break;
          }
        }
      }
    }
  } else {
/* Labels in this clause. */
    jmp_buf jump_stat;
    ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
    ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
    FRAME_JUMP_STAT (frame_pointer) = &jump_stat;
    if (!setjmp (jump_stat)) {
      NODE_T *jump_to = NULL;
      genie_serial_units (SUB (p), &jump_to, exit_buf, stack_pointer);
    } else {
/* HIjol! Restore state and look for indicated unit. */
      NODE_T *jump_to = SYMBOL_TABLE (p)->jump_to;
      stack_pointer = pop_sp;
      frame_pointer = pop_fp;
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
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
  if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GENIE_INFO_T g;
    GENIE (&top_seq) = &g;
    genie_serial_units_no_label (SUB (p), stack_pointer, &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
    if (SEQUENCE (p) != NULL && SEQUENCE (SEQUENCE (p)) == NULL) {
      STATUS_SET (p, OPTIMAL_MASK);
    }
  } else {
/* A linear list without labels (of course, it's an enquiry clause). */
    NODE_T *q;
    ADDR_T pop_sp = stack_pointer;
    STATUS_SET (p, SERIAL_MASK);
    for (q = SEQUENCE (p); q != NULL; q = SEQUENCE (q)) {
      switch (ATTRIBUTE (q)) {
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
      case DECLARATION_LIST:
        {
          genie_declaration (SUB (q));
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
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      EXECUTE_UNIT_TRACE (p);
      GENIE_DNS_STACK (p, MOID (p), FRAME_DYNAMIC_SCOPE (frame_pointer), "collateral units");
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

PROPAGATOR_T genie_collateral (NODE_T * p)
{
  PROPAGATOR_T self;
/* VOID clause and STRUCT display. */
  if (MOID (p) == MODE (VOID) || WHETHER (MOID (p), STRUCT_SYMBOL)) {
    int count = 0;
    genie_collateral_units (SUB (p), &count);
  } else {
/* Row display. */
    A68_REF new_display;
    int count = 0;
    ADDR_T sp = stack_pointer;
    MOID_T *m = MOID (p);
    genie_collateral_units (SUB (p), &count);
    if (DIM (DEFLEX (m)) == 1) {
/* [] AMODE display. */
      new_display = genie_make_row (p, DEFLEX (m)->slice, count, sp);
      stack_pointer = sp;
      INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REF));
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    } else {
/* [,,] AMODE display, we concatenate 1 + (n-1) to n dimensions. */
      new_display = genie_concatenate_rows (p, m, count, sp);
      stack_pointer = sp;
      INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REF));
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    }
  }
  self.unit = genie_collateral;
  self.source = p;
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
  if (p == NULL) {
    return (A68_FALSE);
  } else {
    if (WHETHER (p, UNIT)) {
      if (k == *count) {
        EXECUTE_UNIT_TRACE (p);
        return (A68_TRUE);
      } else {
        (*count)++;
        return (A68_FALSE);
      }
    } else {
      if (genie_int_case_unit (SUB (p), k, count) != 0) {
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

BOOL_T genie_united_case_unit (NODE_T * p, MOID_T * m)
{
  if (p == NULL) {
    return (A68_FALSE);
  } else {
    if (WHETHER (p, SPECIFIER)) {
      MOID_T *spec_moid = MOID (NEXT_SUB (p));
      BOOL_T equal_modes;
      if (m != NULL) {
        if (WHETHER (spec_moid, UNION_SYMBOL)) {
          equal_modes = whether_unitable (m, spec_moid, SAFE_DEFLEXING);
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
        if (WHETHER (q, IDENTIFIER)) {
          if (WHETHER (spec_moid, UNION_SYMBOL)) {
            COPY ((FRAME_OBJECT (OFFSET (TAX (q)))), STACK_TOP, MOID_SIZE (spec_moid));
          } else {
            COPY ((FRAME_OBJECT (OFFSET (TAX (q)))), STACK_OFFSET (ALIGNED_SIZE_OF (A68_UNION)), MOID_SIZE (spec_moid));
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

static void genie_identity_dec (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_IDENTIFIER:
      {
        NODE_T *src = NEXT_NEXT (p);
        MOID_T *src_mode = MOID (p);
        unsigned size = (unsigned) MOID_SIZE (src_mode);
        BYTE_T *z = (FRAME_OBJECT (OFFSET (TAX (p))));
        BYTE_T *stack_base = STACK_TOP;
        ADDR_T pop_sp = stack_pointer;
        ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
        FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
        EXECUTE_UNIT_TRACE (src);
        CHECK_INIT_GENERIC (src, stack_base, src_mode);
        GENIE_DNS_STACK (src, src_mode, frame_pointer, "identity-declaration");
        FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
        if (src_mode->has_rows) {
          stack_pointer = pop_sp;
          if (WHETHER (src_mode, STRUCT_SYMBOL)) {
/* STRUCT with row. */
            A68_REF w, src2;
            STATUS (&w) = (STATUS_MASK) (INITIALISED_MASK | IN_STACK_MASK);
            w.offset = stack_pointer;
            REF_HANDLE (&w) = &nil_handle;
            src2 = genie_copy_stowed (w, p, MOID (p));
            COPY (z, ADDRESS (&src2), (int) size);
          } else if (WHETHER (MOID (p), UNION_SYMBOL)) {
/* UNION with row. */
            genie_pop_union (p);
            COPY (z, STACK_TOP, (int) size);
          } else if (WHETHER (MOID (p), ROW_SYMBOL) || WHETHER (MOID (p), FLEX_SYMBOL)) {
/* (FLEX) ROW. */
            *(A68_REF *) z = genie_copy_stowed (*(A68_REF *) STACK_TOP, p, MOID (p));
          } else if (MOID (p) == MODE (SOUND)) {
            COPY (z, STACK_TOP, (int) size);
          }
        } else if (PROPAGATOR (src).unit == genie_constant) {
          STATUS_SET (TAX (p), CONSTANT_MASK);
          POP_ALIGNED (p, z, size);
        } else {
          POP_ALIGNED (p, z, size);
        }
        return;
      }
    default:
      {
        genie_identity_dec (SUB (p));
        break;
      }
    }
  }
}

/*!
\brief execute variable declaration
\param p position in tree
\param declarer pointer to the declarer
**/

static void genie_variable_dec (NODE_T * p, NODE_T ** declarer, ADDR_T sp)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, VARIABLE_DECLARATION)) {
      genie_variable_dec (SUB (p), declarer, sp);
    } else {
      if (WHETHER (p, DECLARER)) {
        (*declarer) = SUB (p);
        genie_generator_bounds (*declarer);
        FORWARD (p);
      }
      if (WHETHER (p, DEFINING_IDENTIFIER)) {
        MOID_T *ref_mode = MOID (p);
        TAG_T *tag = TAX (p);
        int leap = (HEAP (tag) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
        A68_REF *z = (A68_REF *) (FRAME_OBJECT (OFFSET (TAX (p))));
        genie_generator_internal (*declarer, ref_mode, BODY (tag), leap, sp);
        POP_REF (p, z);
        if (NEXT (p) != NULL && WHETHER (NEXT (p), ASSIGN_SYMBOL)) {
          NODE_T *src = NEXT_NEXT (p);
          MOID_T *src_mode = SUB_MOID (p);
          ADDR_T pop_sp = stack_pointer;
          ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
          FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
          EXECUTE_UNIT_TRACE (src);
          GENIE_DNS_STACK (src, src_mode, frame_pointer, "variable-declaration");
          FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
          stack_pointer = pop_sp;
          if (src_mode->has_rows) {
            genie_assign_internal (p, z, src_mode);
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

static void genie_proc_variable_dec (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
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
        if (NEXT (p) != NULL && WHETHER (NEXT (p), ASSIGN_SYMBOL)) {
          MOID_T *src_mode = SUB_MOID (p);
          ADDR_T pop_sp = stack_pointer;
          ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
          FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
          EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
          GENIE_DNS_STACK (p, SUB (ref_mode), frame_pointer, "procedure-variable-declaration");
          FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
          stack_pointer = pop_sp;
          MOVE (ADDRESS (z), STACK_TOP, (unsigned) MOID_SIZE (src_mode));
        }
        stack_pointer = sp_for_voiding; /* Voiding. */
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

static void genie_operator_dec (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_OPERATOR:
      {
        A68_PROCEDURE *z = (A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (p))));
        ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
        FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
        EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
        GENIE_DNS_STACK (p, MOID (p), frame_pointer, "operator-declaration");
        FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
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
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case MODE_DECLARATION:
    case PROCEDURE_DECLARATION:
    case BRIEF_OPERATOR_DECLARATION:
    case PRIORITY_DECLARATION:
      {
/* Already resolved. */
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
        NODE_T *declarer = NULL;
        ADDR_T pop_sp = stack_pointer;
        genie_variable_dec (SUB (p), &declarer, stack_pointer);
/* Voiding to remove garbage from declarers. */
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
  for (_m_q = SEQUENCE (p); _m_q != NULL; _m_q = SEQUENCE (_m_q)) {\
    switch (ATTRIBUTE (_m_q)) {\
    case UNIT: {\
      EXECUTE_UNIT_TRACE (_m_q);\
      break;\
    }\
    case SEMI_SYMBOL: {\
      stack_pointer = pop_sp;\
      break;\
    }\
    case DECLARATION_LIST: {\
      genie_declaration (SUB (_m_q));\
      break;\
    }\
    }\
  }}
*/

#define LABEL_FREE(p) {\
  NODE_T *_m_q; ADDR_T pop_sp_lf = stack_pointer;\
  for (_m_q = SEQUENCE (p); _m_q != NULL; _m_q = SEQUENCE (_m_q)) {\
    if (WHETHER (_m_q, UNIT)) {\
      EXECUTE_UNIT_TRACE (_m_q);\
    } else if (WHETHER (_m_q, DECLARATION_LIST)) {\
      genie_declaration (SUB (_m_q));\
    }\
    if (SEQUENCE (_m_q) != NULL) {\
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

PROPAGATOR_T genie_int_case (volatile NODE_T * p)
{
  volatile int unit_count;
  volatile BOOL_T found_unit;
  jmp_buf exit_buf;
  A68_INT k;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* CASE or OUSE. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  POP_OBJECT (q, &k, A68_INT);
/* IN. */
  FORWARD (q);
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  unit_count = 1;
  found_unit = genie_int_case_unit (NEXT_SUB ((NODE_T *) q), (int) VALUE (&k), (int *) &unit_count);
  CLOSE_FRAME;
/* OUT. */
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
        genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
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
/* ESAC. */
  CLOSE_FRAME;
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "integer-case-clause");
  PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
  return (PROPAGATOR (p));
}

/*!
\brief execute united-case-clause
\param p position in tree
**/

PROPAGATOR_T genie_united_case (volatile NODE_T * p)
{
  volatile BOOL_T found_unit = A68_FALSE;
  volatile MOID_T *um;
  volatile ADDR_T pop_sp;
  jmp_buf exit_buf;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* CASE or OUSE. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  pop_sp = stack_pointer;
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  stack_pointer = pop_sp;
  um = (volatile MOID_T *) VALUE ((A68_UNION *) STACK_TOP);
/* IN. */
  FORWARD (q);
  if (um != NULL) {
    OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
    INIT_STATIC_FRAME ((NODE_T *) SUB (q));
    found_unit = genie_united_case_unit (NEXT_SUB ((NODE_T *) q), (MOID_T *) um);
    CLOSE_FRAME;
  } else {
    found_unit = A68_FALSE;
  }
/* OUT. */
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
        genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
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
/* ESAC. */
  CLOSE_FRAME;
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "united-case-clause");
  PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
  return (PROPAGATOR (p));
}

/*!
\brief execute conditional-clause
\param p position in tree
**/

PROPAGATOR_T genie_conditional (volatile NODE_T * p)
{
  volatile int pop_sp = stack_pointer;
  jmp_buf exit_buf;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* IF or ELIF. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  INIT_GLOBAL_POINTER ((NODE_T *) SUB (q));
  INIT_STATIC_FRAME ((NODE_T *) SUB (q));
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  stack_pointer = pop_sp;
  FORWARD (q);
  if (VALUE ((A68_BOOL *) STACK_TOP) == A68_TRUE) {
/* THEN. */
    OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
    INIT_STATIC_FRAME ((NODE_T *) SUB (q));
    SERIAL_CLAUSE (NEXT_SUB (q));
    CLOSE_FRAME;
  } else {
/* ELSE. */
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
        genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
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
/* FI. */
  CLOSE_FRAME;
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "conditional-clause");
  PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
  return (PROPAGATOR (p));
}

/*
INCREMENT_COUNTER procures that the counter only increments if there is
a for-part or a to-part. Otherwise an infinite loop would trigger overflow
when the anonymous counter reaches max int, which is strange behaviour.
*/

#define INCREMENT_COUNTER\
  if (!(for_part == NULL && to_part == NULL)) {\
    CHECK_INT_ADDITION ((NODE_T *) p, counter, by);\
    counter += by;\
  }

/*!
\brief execute loop-clause
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_loop (volatile NODE_T * p)
{
  volatile ADDR_T pop_sp = stack_pointer;
  volatile int from, by, to, counter;
  volatile BOOL_T siga, conditional;
  volatile NODE_T *for_part = NULL, *to_part = NULL, *q;
  jmp_buf exit_buf;
/* FOR  identifier. */
  if (WHETHER (p, FOR_PART)) {
    for_part = NEXT_SUB (p);
    FORWARD (p);
  }
/* FROM unit. */
  if (WHETHER (p, FROM_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    from = VALUE ((A68_INT *) STACK_TOP);
    FORWARD (p);
  } else {
    from = 1;
  }
/* BY unit. */
  if (WHETHER (p, BY_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    by = VALUE ((A68_INT *) STACK_TOP);
    FORWARD (p);
  } else {
    by = 1;
  }
/* TO unit, DOWNTO unit. */
  if (WHETHER (p, TO_PART)) {
    if (WHETHER (SUB (p), DOWNTO_SYMBOL)) {
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
   We open the frame only once and reinitialise if necessary. */
  OPEN_STATIC_FRAME ((NODE_T *) q);
  INIT_GLOBAL_POINTER ((NODE_T *) q);
  INIT_STATIC_FRAME ((NODE_T *) q);
  counter = from;
/* Does the loop contain conditionals? */
  if (WHETHER (p, WHILE_PART)) {
    conditional = A68_TRUE;
  } else if (WHETHER (p, DO_PART) || WHETHER (p, ALT_DO_PART)) {
    NODE_T *un_p = NEXT_SUB (p);
    if (WHETHER (un_p, SERIAL_CLAUSE)) {
      un_p = NEXT (un_p);
    }
    conditional = (BOOL_T) (un_p != NULL && WHETHER (un_p, UNTIL_PART));
  } else {
    conditional = A68_FALSE;
  }
  if (conditional) {
/* [FOR ...] [WHILE ...] DO [...] [UNTIL ...] OD. */
    siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
    while (siga) {
      if (for_part != NULL) {
        A68_INT *z = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (for_part))));
        STATUS (z) = INITIALISED_MASK;
        VALUE (z) = counter;
      }
      stack_pointer = pop_sp;
      if (WHETHER (p, WHILE_PART)) {
        ENQUIRY_CLAUSE (q);
        stack_pointer = pop_sp;
        siga = (BOOL_T) (VALUE ((A68_BOOL *) STACK_TOP) != A68_FALSE);
      }
      if (siga) {
        volatile NODE_T *do_p = p, *un_p;
        if (WHETHER (p, WHILE_PART)) {
          do_p = NEXT_SUB (NEXT (p));
          OPEN_STATIC_FRAME ((NODE_T *) do_p);
          INIT_STATIC_FRAME ((NODE_T *) do_p);
        } else {
          do_p = NEXT_SUB (p);
        }
        if (WHETHER (do_p, SERIAL_CLAUSE)) {
          PREEMPTIVE_SWEEP;
          SERIAL_CLAUSE_TRACE (do_p);
          un_p = NEXT (do_p);
        } else {
          un_p = do_p;
        }
/* UNTIL part. */
        if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
          NODE_T *v = NEXT_SUB (un_p);
          OPEN_STATIC_FRAME ((NODE_T *) v);
          INIT_STATIC_FRAME ((NODE_T *) v);
          stack_pointer = pop_sp;
          ENQUIRY_CLAUSE (v);
          stack_pointer = pop_sp;
          siga = (BOOL_T) (VALUE ((A68_BOOL *) STACK_TOP) == A68_FALSE);
          CLOSE_FRAME;
        }
        if (WHETHER (p, WHILE_PART)) {
          CLOSE_FRAME;
        }
/* Increment counter. */
        if (siga) {
          INCREMENT_COUNTER;
          siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
        }
/* The genie cannot take things to next iteration: re-initialise stack frame. */
        if (siga) {
          FRAME_CLEAR (SYMBOL_TABLE (q)->ap_increment);
          if (SYMBOL_TABLE (q)->initialise_frame) {
            initialise_frame ((NODE_T *) q);
          }
        }
      }
    }
  } else {
/* [FOR ...] DO ... OD. */
    siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
    while (siga) {
      if (for_part != NULL) {
        A68_INT *z = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (for_part))));
        STATUS (z) = INITIALISED_MASK;
        VALUE (z) = counter;
      }
      stack_pointer = pop_sp;
      PREEMPTIVE_SWEEP;
      SERIAL_CLAUSE_TRACE (q);
      INCREMENT_COUNTER;
      siga = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
/* The genie cannot take things to next iteration: re-initialise stack frame. */
      if (siga) {
        FRAME_CLEAR (SYMBOL_TABLE (q)->ap_increment);
        if (SYMBOL_TABLE (q)->initialise_frame) {
          initialise_frame ((NODE_T *) q);
        }
      }
    }
  }
/* OD. */
  CLOSE_FRAME;
  stack_pointer = pop_sp;
  return (PROPAGATOR (p));
}

#undef INCREMENT_COUNTER
#undef LOOP_OVERFLOW

/*!
\brief execute closed clause
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_closed (volatile NODE_T * p)
{
  jmp_buf exit_buf;
  volatile NODE_T *q = NEXT_SUB (p);
  OPEN_STATIC_FRAME ((NODE_T *) q);
  INIT_GLOBAL_POINTER ((NODE_T *) q);
  INIT_STATIC_FRAME ((NODE_T *) q);
  SERIAL_CLAUSE (q);
  CLOSE_FRAME;
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "closed-clause");
  PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
  return (PROPAGATOR (p));
}

/*!
\brief execute enclosed clause
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_enclosed (volatile NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = (PROPAGATOR_PROCEDURE *) genie_enclosed;
  self.source = (NODE_T *) p;
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
      if (self.unit == genie_unit) {
        self.unit = (PROPAGATOR_PROCEDURE *) genie_closed;
        self.source = (NODE_T *) p;
      }
      break;
    }
#if defined ENABLE_PAR_CLAUSE
  case PARALLEL_CLAUSE:
    {
      (void) genie_parallel ((NODE_T *) NEXT_SUB (p));
      GENIE_DNS_STACK (p, MOID (p), frame_pointer, "parallel-clause");
      PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
      break;
    }
#endif
  case COLLATERAL_CLAUSE:
    {
      (void) genie_collateral ((NODE_T *) p);
      GENIE_DNS_STACK (p, MOID (p), frame_pointer, "collateral-clause");
      PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
      break;
    }
  case CONDITIONAL_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_conditional (p);
      self.unit = (PROPAGATOR_PROCEDURE *) genie_conditional;
      self.source = (NODE_T *) p;
      break;
    }
  case INTEGER_CASE_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_int_case (p);
      self.unit = (PROPAGATOR_PROCEDURE *) genie_int_case;
      self.source = (NODE_T *) p;
      break;
    }
  case UNITED_CASE_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_united_case (p);
      self.unit = (PROPAGATOR_PROCEDURE *) genie_united_case;
      self.source = (NODE_T *) p;
      break;
    }
  case LOOP_CLAUSE:
    {
      (void) genie_loop (SUB ((NODE_T *) p));
      self.unit = (PROPAGATOR_PROCEDURE *) genie_loop;
      self.source = SUB ((NODE_T *) p);
      break;
    }
  }
  PROPAGATOR (p) = self;
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

static void genie_copy_union (NODE_T *, BYTE_T *, BYTE_T *, A68_REF);
static A68_REF genie_copy_row (A68_REF, NODE_T *, MOID_T *);

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
    ref->k = LWB (ref);
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
    iindex += (ref->span * ref->k - ref->shift);
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
    if (ref->k < UPB (ref)) {
      (ref->k)++;
      carry = A68_FALSE;
    } else {
      ref->k = LWB (ref);
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
    ASSERT (snprintf (buf, (size_t) BUFFER_SIZE, "%d", ref->k) >= 0);
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
  PROTECT_SWEEP_HANDLE (&z);
  row = heap_generator (p, MODE (ROW_CHAR), width * ALIGNED_SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (CHAR);
  arr.elem_size = ALIGNED_SIZE_OF (A68_CHAR);
  arr.slice_offset = 0;
  arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = width;
  tup.span = 1;
  tup.shift = LWB (&tup);
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &z);
  base = ADDRESS (&row);
  for (k = 0; k < width; k++) {
    A68_CHAR *ch = (A68_CHAR *) & (base[k * ALIGNED_SIZE_OF (A68_CHAR)]);
    STATUS (ch) = INITIALISED_MASK;
    VALUE (ch) = TO_UCHAR (str[k]);
  }
  UNPROTECT_SWEEP_HANDLE (&z);
  UNPROTECT_SWEEP_HANDLE (&row);
  return (z);
}

/*!
\brief convert C string to A68 string
\param p position in tree
\param str string to convert
\return STRING
**/

A68_REF c_to_a_string (NODE_T * p, char *str)
{
  if (str == NULL) {
    return (empty_string (p));
  } else {
    return (c_string_to_row_char (p, str, (int) strlen (str)));
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
\return str on success or NULL on failure
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
    return (NULL);
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
  A68_REF ref_desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int dim, k;
  if (WHETHER (u, FLEX_SYMBOL)) {
    u = SUB (u);
  }
  dim = DIM (u);
  ref_desc = heap_generator (p, u, ALIGNED_SIZE_OF (A68_ARRAY) + dim * ALIGNED_SIZE_OF (A68_TUPLE));
  GET_DESCRIPTOR (arr, tup, &ref_desc);
  DIM (arr) = dim;
  MOID (arr) = SLICE (u);
  arr->elem_size = moid_size (SLICE (u));
  arr->slice_offset = 0;
  arr->field_offset = 0;
  STATUS (&ARRAY (arr)) = (STATUS_MASK) (INITIALISED_MASK | IN_HEAP_MASK);
  ARRAY (arr).offset = 0;
  REF_HANDLE (&(ARRAY (arr))) = &nil_handle;
  for (k = 0; k < dim; k++) {
    tup[k].lower_bound = 1;
    tup[k].upper_bound = 0;
    tup[k].span = 1;
    tup[k].shift = LWB (tup);
  }
  return (ref_desc);
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
\param row_mode row of mode
\param elems_in_stack number of elements
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_concatenate_rows (NODE_T * p, MOID_T * row_mode, int elems_in_stack, ADDR_T sp)
{
  MOID_T *new_mode = WHETHER (row_mode, FLEX_SYMBOL) ? SUB (row_mode) : row_mode;
  MOID_T *elem_mode = SUB (new_mode);
  A68_REF new_row, old_row;
  A68_ARRAY *new_arr, *old_arr;
  A68_TUPLE *new_tup, *old_tup;
  int span, old_dim = DIM (new_mode) - 1;
/* Make the new descriptor. */
  UP_SWEEP_SEMA;
  new_row = heap_generator (p, row_mode, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (new_mode) * ALIGNED_SIZE_OF (A68_TUPLE));
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (new_mode);
  MOID (new_arr) = elem_mode;
  new_arr->elem_size = MOID_SIZE (elem_mode);
  new_arr->slice_offset = 0;
  new_arr->field_offset = 0;
  if (elems_in_stack == 0) {
/* There is a vacuum on the stack. */
    int k;
    elems_in_stack = 1;
    span = 0;
    for (k = 0; k < old_dim; k++) {
      new_tup[k + 1].lower_bound = 1;
      new_tup[k + 1].upper_bound = 0;
      new_tup[k + 1].span = 1;
      new_tup[k + 1].shift = new_tup[k + 1].lower_bound;
    }
  } else {
    int k;
    A68_ARRAY *dummy = NULL;
    if (elems_in_stack > 1) {
/* AARRAY(&ll)s in the stack must have the same bounds with respect to (arbitrary) first one. */
      int i;
      for (i = 1; i < elems_in_stack; i++) {
        A68_REF run_row, ref_row;
        A68_TUPLE *run_tup, *ref_tup;
        int j;
        ref_row = *(A68_REF *) STACK_ADDRESS (sp);
        run_row = *(A68_REF *) STACK_ADDRESS (sp + i * ALIGNED_SIZE_OF (A68_REF));
        GET_DESCRIPTOR (dummy, ref_tup, &ref_row);
        GET_DESCRIPTOR (dummy, run_tup, &run_row);
        for (j = 0; j < old_dim; j++) {
          if ((UPB (ref_tup) != UPB (run_tup)) || (LWB (ref_tup) != LWB (run_tup))) {
            diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
          }
          ref_tup++;
          run_tup++;
        }
      }
    }
/* Ok, same size. Fill descriptor of new row with info from (arbitrary) first one. */
    old_row = *(A68_REF *) STACK_ADDRESS (sp);
    GET_DESCRIPTOR (dummy, old_tup, &old_row);
    span = 1;
    for (k = 0; k < old_dim; k++) {
      A68_TUPLE *new_t = &new_tup[k + 1], *old_t = &old_tup[k];
      LWB (new_t) = LWB (old_t);
      UPB (new_t) = UPB (old_t);
      new_t->span = span;
      new_t->shift = LWB (new_t) * new_t->span;
      span *= ROW_SIZE (new_t);
    }
  }
  LWB (new_tup) = 1;
  UPB (new_tup) = elems_in_stack;
  new_tup->span = span;
  new_tup->shift = LWB (new_tup) * new_tup->span;
/* Allocate space for the big new row. */
  ARRAY (new_arr) = heap_generator (p, row_mode, elems_in_stack * span * new_arr->elem_size);
  if (span > 0) {
/* Copy 'elems_in_stack' rows into the new one. */
    int j;
    BYTE_T *new_elem = ADDRESS (&(ARRAY (new_arr)));
    for (j = 0; j < elems_in_stack; j++) {
/* new [j, , ] := old [, ] */
      BYTE_T *old_elem;
      BOOL_T done;
      GET_DESCRIPTOR (old_arr, old_tup, (A68_REF *) STACK_ADDRESS (sp + j * ALIGNED_SIZE_OF (A68_REF)));
      old_elem = ADDRESS (&(ARRAY (old_arr)));
      initialise_internal_index (old_tup, old_dim);
      initialise_internal_index (&new_tup[1], old_dim);
      done = A68_FALSE;
      while (!done) {
        ADDR_T old_index, new_index, old_addr, new_addr;
        old_index = calculate_internal_index (old_tup, old_dim);
        new_index = j * new_tup->span + calculate_internal_index (&new_tup[1], old_dim);
        old_addr = ROW_ELEMENT (old_arr, old_index);
        new_addr = ROW_ELEMENT (new_arr, new_index);
        MOVE (&new_elem[new_addr], &old_elem[old_addr], (unsigned) new_arr->elem_size);
        done = increment_internal_index (old_tup, old_dim) | increment_internal_index (&new_tup[1], old_dim);
      }
    }
  }
  DOWN_SWEEP_SEMA;
  return (new_row);
}

/*!
\brief make a row of 'elems_in_stack' objects that are in the stack
\param p position in tree
\param elem_mode mode of element
\param elems_in_stack number of elements in the stack
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_make_row (NODE_T * p, MOID_T * elem_mode, int elems_in_stack, ADDR_T sp)
{
  A68_REF new_row, new_arr;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int k;
  new_row = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  new_arr = heap_generator (p, MOID (p), elems_in_stack * MOID_SIZE (elem_mode));
  PROTECT_SWEEP_HANDLE (&new_arr);
  GET_DESCRIPTOR (arr, tup, &new_row);
  DIM (arr) = 1;
  MOID (arr) = elem_mode;
  arr->elem_size = MOID_SIZE (elem_mode);
  arr->slice_offset = 0;
  arr->field_offset = 0;
  ARRAY (arr) = new_arr;
  LWB (tup) = 1;
  UPB (tup) = elems_in_stack;
  tup->span = 1;
  tup->shift = LWB (tup);
  for (k = 0; k < elems_in_stack; k++) {
    ADDR_T offset = k * arr->elem_size;
    BYTE_T *src_a, *dst_a;
    A68_REF dst = new_arr, src;
    dst.offset += offset;
    STATUS (&src) = (STATUS_MASK) (INITIALISED_MASK | IN_STACK_MASK);
    src.offset = sp + offset;
    REF_HANDLE (&src) = &nil_handle;
    dst_a = ADDRESS (&dst);
    src_a = ADDRESS (&src);
    if (elem_mode->has_rows) {
      if (WHETHER (elem_mode, STRUCT_SYMBOL)) {
        A68_REF new_one = genie_copy_stowed (src, p, elem_mode);
        MOVE (dst_a, ADDRESS (&new_one), MOID_SIZE (elem_mode));
      } else if (WHETHER (elem_mode, FLEX_SYMBOL) || elem_mode == MODE (STRING)) {
        *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, DEFLEX (elem_mode));
      } else if (WHETHER (elem_mode, ROW_SYMBOL)) {
        *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, elem_mode);
      } else if (WHETHER (elem_mode, UNION_SYMBOL)) {
        genie_copy_union (p, dst_a, src_a, src);
      } else if (elem_mode == MODE (SOUND)) {
        genie_copy_sound (p, dst_a, src_a);
      } else {
        ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_make_row");
      }
    } else {
      MOVE (dst_a, src_a, (unsigned) arr->elem_size);
    }
  }
  UNPROTECT_SWEEP_HANDLE (&new_row);
  UNPROTECT_SWEEP_HANDLE (&new_arr);
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
/* ROWING NIL yields NIL. */
  if (IS_NIL (array)) {
    return (nil_ref);
  }
  new_row = heap_generator (p, SUB (dst_mode), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  name = heap_generator (p, dst_mode, ALIGNED_SIZE_OF (A68_REF));
  GET_DESCRIPTOR (arr, tup, &new_row);
  DIM (arr) = 1;
  MOID (arr) = src_mode;
  arr->elem_size = MOID_SIZE (src_mode);
  arr->slice_offset = 0;
  arr->field_offset = 0;
  ARRAY (arr) = array;
  LWB (tup) = 1;
  UPB (tup) = 1;
  tup->span = 1;
  tup->shift = LWB (tup);
  *(A68_REF *) ADDRESS (&name) = new_row;
  UNPROTECT_SWEEP_HANDLE (&new_row);
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
/* ROWING NIL yields NIL. */
  if (IS_NIL (name)) {
    return (nil_ref);
  }
  old_row = *(A68_REF *) ADDRESS (&name);
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
/* Make new descriptor. */
  new_row = heap_generator (p, dst_mode, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (SUB (dst_mode)) * ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  name = heap_generator (p, dst_mode, ALIGNED_SIZE_OF (A68_REF));
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (SUB (dst_mode));
  MOID (new_arr) = MOID (old_arr);
  new_arr->elem_size = old_arr->elem_size;
  new_arr->slice_offset = 0;
  new_arr->field_offset = 0;
  ARRAY (new_arr) = ARRAY (old_arr);
/* Fill out the descriptor. */
  new_tup[0].lower_bound = 1;
  new_tup[0].upper_bound = 1;
  new_tup[0].span = 1;
  new_tup[0].shift = new_tup[0].lower_bound;
  for (k = 0; k < DIM (SUB (src_mode)); k++) {
    new_tup[k + 1] = old_tup[k];
  }
/* Yield the new name. */
  *(A68_REF *) ADDRESS (&name) = new_row;
  UNPROTECT_SWEEP_HANDLE (&new_row);
  return (name);
}

/*!
\brief coercion to [1 : 1, ] MODE
\param p position in tree
\return propagator for this action
**/

PROPAGATOR_T genie_rowing_row_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  row = genie_concatenate_rows (p, MOID (p), 1, sp);
  stack_pointer = sp;
  PUSH_REF (p, row);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief coercion to [1 : 1] [] MODE
\param p position in tree
\return propagator for this action
**/

PROPAGATOR_T genie_rowing_row_of_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  row = genie_make_row (p, SLICE (MOID (p)), 1, sp);
  stack_pointer = sp;
  PUSH_REF (p, row);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief coercion to REF [1 : 1, ..] MODE
\param p position in tree
\return propagator for this action
**/

PROPAGATOR_T genie_rowing_ref_row_row (NODE_T * p)
{
  A68_REF name;
  ADDR_T sp = stack_pointer;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  stack_pointer = sp;
  name = genie_make_ref_row_row (p, dst, src, sp);
  PUSH_REF (p, name);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief REF [1 : 1] [] MODE from [] MODE
\param p position in tree
\return propagator for this action
**/

PROPAGATOR_T genie_rowing_ref_row_of_row (NODE_T * p)
{
  A68_REF name;
  ADDR_T sp = stack_pointer;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  stack_pointer = sp;
  name = genie_make_ref_row_of_row (p, dst, src, sp);
  PUSH_REF (p, name);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief rowing coercion
\param p position in tree
\return propagator for this action
**/

PROPAGATOR_T genie_rowing (NODE_T * p)
{
  PROPAGATOR_T self;
  if (WHETHER (MOID (p), REF_SYMBOL)) {
/* REF ROW, decide whether we want A->[] A or [] A->[,] A. */
    MOID_T *mode = SUB_MOID (p);
    if (DIM (DEFLEX (mode)) >= 2) {
      (void) genie_rowing_ref_row_row (p);
      self.unit = genie_rowing_ref_row_row;
      self.source = p;
    } else {
      (void) genie_rowing_ref_row_of_row (p);
      self.unit = genie_rowing_ref_row_of_row;
      self.source = p;
    }
  } else {
/* ROW, decide whether we want A->[] A or [] A->[,] A. */
    if (DIM (DEFLEX (MOID (p))) >= 2) {
      (void) genie_rowing_row_row (p);
      self.unit = genie_rowing_row_row;
      self.source = p;
    } else {
      (void) genie_rowing_row_of_row (p);
      self.unit = genie_rowing_row_of_row;
      self.source = p;
    }
  }
  return (self);
}

/*!
\brief copy a united object holding stowed
\param p position in tree
\param dst_a destination
\param src_a source
\param struct_field pointer to structured field
**/

static void genie_copy_union (NODE_T * p, BYTE_T * dst_a, BYTE_T * src_a, A68_REF struct_field)
{
  BYTE_T *dst_u = &(dst_a[UNION_OFFSET]), *src_u = &(src_a[UNION_OFFSET]);
  A68_UNION *u = (A68_UNION *) src_a;
  MOID_T *um = (MOID_T *) VALUE (u);
  if (um != NULL) {
    *(A68_UNION *) dst_a = *u;
    if (WHETHER (um, STRUCT_SYMBOL)) {
/* UNION (STRUCT ..) */
      A68_REF w = struct_field, src;
      w.offset += UNION_OFFSET;
      src = genie_copy_stowed (w, p, um);
      MOVE (dst_u, ADDRESS (&src), MOID_SIZE (um));
    } else if (WHETHER (um, FLEX_SYMBOL) || um == MODE (STRING)) {
/* UNION (FLEX [] A ..). Bounds are irrelevant: copy and assign. */
      *(A68_REF *) dst_u = genie_copy_row (*(A68_REF *) src_u, p, DEFLEX (um));
    } else if (WHETHER (um, ROW_SYMBOL)) {
/* UNION ([] A ..). Bounds are irrelevant: copy and assign. */
      *(A68_REF *) dst_u = genie_copy_row (*(A68_REF *) src_u, p, um);
    } else {
/* UNION (..). Non-stowed mode. */
      MOVE (dst_u, src_u, MOID_SIZE (um));
    }
  }
}

/*!
\brief make copy array of mode 'm' from 'old'
\param old_row fat pointer to descriptor of old row
\param p position in tree
\param m mode of row
\return fat pointer to descriptor
**/

A68_REF genie_copy_row (A68_REF old_row, NODE_T * p, MOID_T * m)
{
/* We need this complex routine since arrays are not always contiguous (trims). */
  A68_REF new_row;
  A68_ARRAY *old_arr, *new_arr;
  A68_TUPLE *old_tup, *new_tup, *old_p, *new_p;
  int k, span;
  UP_SWEEP_SEMA;
  if (IS_NIL (old_row)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Cut FLEX from the mode. That is not interesting in this routine. */
  if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
    m = SUB (m);
  }
/* Make ARRAY(&new). */
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (old_arr) * ALIGNED_SIZE_OF (A68_TUPLE));
/* Get descriptor again in case the heap sweeper moved data (but switched off now) */
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (old_arr);
  MOID (new_arr) = MOID (old_arr);
  new_arr->elem_size = old_arr->elem_size;
  new_arr->slice_offset = 0;
  new_arr->field_offset = 0;
/* Get size and copy bounds; no checks since this routine just makes a copy. */
  span = 1;
  for (k = 0; k < DIM (old_arr); k++) {
    old_p = &old_tup[k];
    new_p = &new_tup[k];
    LWB (new_p) = LWB (old_p);
    UPB (new_p) = UPB (old_p);
    new_p->span = span;
    new_p->shift = LWB (new_p) * new_p->span;
    span *= ROW_SIZE (new_p);
  }
  ARRAY (new_arr) = heap_generator (p, MOID (p), span * new_arr->elem_size);
/* The n-dimensional copier. */
  if (span > 0) {
    unsigned elem_size = (unsigned) moid_size (MOID (old_arr));
    MOID_T *elem_mode = SUB (m);
    BYTE_T *old_elem = ADDRESS (&(ARRAY (old_arr)));
    BYTE_T *new_elem = ADDRESS (&(ARRAY (new_arr)));
    BOOL_T done = A68_FALSE;
    initialise_internal_index (old_tup, DIM (old_arr));
    initialise_internal_index (new_tup, DIM (new_arr));
    while (!done) {
      ADDR_T old_index, new_index, old_addr, new_addr;
      old_index = calculate_internal_index (old_tup, DIM (old_arr));
      new_index = calculate_internal_index (new_tup, DIM (new_arr));
      old_addr = ROW_ELEMENT (old_arr, old_index);
      new_addr = ROW_ELEMENT (new_arr, new_index);
      if (elem_mode->has_rows) {
/* Recursion. */
        A68_REF new_old = ARRAY (old_arr), new_dst = ARRAY (new_arr);
        BYTE_T *src_a, *dst_a;
        new_old.offset += old_addr;
        new_dst.offset += new_addr;
        src_a = ADDRESS (&new_old);
        dst_a = ADDRESS (&new_dst);
        if (WHETHER (elem_mode, STRUCT_SYMBOL)) {
          A68_REF str_src = genie_copy_stowed (new_old, p, elem_mode);
          MOVE (dst_a, ADDRESS (&str_src), MOID_SIZE (elem_mode));
        } else if (WHETHER (elem_mode, FLEX_SYMBOL)
                   || elem_mode == MODE (STRING)) {
          *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, DEFLEX (elem_mode));
        } else if (WHETHER (elem_mode, ROW_SYMBOL)) {
          *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, elem_mode);
        } else if (WHETHER (elem_mode, UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, new_old);
        } else if (elem_mode == MODE (SOUND)) {
          genie_copy_sound (p, dst_a, src_a);
        } else {
          ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_row");
        }
      } else {
        MOVE (&new_elem[new_addr], &old_elem[old_addr], elem_size);
      }
/* Increase pointers. */
      done = increment_internal_index (old_tup, DIM (old_arr)) | increment_internal_index (new_tup, DIM (new_arr));
    }
  }
  DOWN_SWEEP_SEMA;
  return (new_row);
}

/*!
\brief make copy of stowed value at 'old'
\param old pointer to old object
\param p position in tree
\param m mode of object
\return fat pointer to descriptor or structured value
**/

A68_REF genie_copy_stowed (A68_REF old, NODE_T * p, MOID_T * m)
{
  if (WHETHER (m, STRUCT_SYMBOL)) {
    PACK_T *fields;
    A68_REF new_struct;
    UP_SWEEP_SEMA;
    new_struct = heap_generator (p, m, MOID_SIZE (m));
    for (fields = PACK (m); fields != NULL; fields = NEXT (fields)) {
      A68_REF old_field = old, new_field = new_struct;
      BYTE_T *src_a, *dst_a;
      old_field.offset += fields->offset;
      new_field.offset += fields->offset;
      src_a = ADDRESS (&old_field);
      dst_a = ADDRESS (&new_field);
      if (MOID (fields)->has_rows) {
        if (WHETHER (MOID (fields), STRUCT_SYMBOL)) {
          A68_REF str_src = genie_copy_stowed (old_field, p, MOID (fields));
          MOVE (dst_a, ADDRESS (&str_src), MOID_SIZE (MOID (fields)));
        } else if (WHETHER (MOID (fields), FLEX_SYMBOL)
                   || MOID (fields) == MODE (STRING)) {
          *(A68_REF *) dst_a = genie_copy_row (*(A68_REF *) src_a, p, MOID (fields));
        } else if (WHETHER (MOID (fields), ROW_SYMBOL)) {
          *(A68_REF *) dst_a = genie_copy_row (*(A68_REF *) src_a, p, MOID (fields));
        } else if (WHETHER (MOID (fields), UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, old_field);
        } else if (MOID (fields) == MODE (SOUND)) {
          genie_copy_sound (p, dst_a, src_a);
        } else {
          ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_stowed");
        }
      } else {
        MOVE (dst_a, src_a, MOID_SIZE (fields));
      }
    }
    DOWN_SWEEP_SEMA;
    return (new_struct);
  } else if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
    A68_REF new_row;
    UP_SWEEP_SEMA;
    new_row = genie_copy_row (old, p, DEFLEX (m));
    DOWN_SWEEP_SEMA;
    return (new_row);
  } else if (WHETHER (m, ROW_SYMBOL)) {
    A68_REF new_row;
    UP_SWEEP_SEMA;
    new_row = genie_copy_row (old, p, DEFLEX (m));
    DOWN_SWEEP_SEMA;
    return (new_row);
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_stowed");
    return (nil_ref);
  }
}

/*!
\brief assign array of MODE 'm' from 'old_row' to 'dst'
\param old_row pointer to old object
\param dst destination, may be overwritten
\param p position in tree
\param m mode of row
\return fat pointer to descriptor
**/

static A68_REF genie_assign_row (A68_REF old_row, A68_REF * dst, NODE_T * p, MOID_T * m)
{
  A68_REF new_row;
  A68_ARRAY *old_arr, *new_arr = NULL;
  A68_TUPLE *old_tup, *new_tup = NULL, *old_p, *new_p;
  int k, span = 0;
  STATUS (&new_row) = INITIALISED_MASK;
  new_row.offset = 0;
/* Get row desriptors. Switch off GC so data is not moved. */
  UP_SWEEP_SEMA;
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
/* In case of FLEX rows we make a new descriptor. */
    m = SUB (m);
    new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (old_arr) * ALIGNED_SIZE_OF (A68_TUPLE));
    GET_DESCRIPTOR (new_arr, new_tup, &new_row);
    DIM (new_arr) = DIM (old_arr);
    MOID (new_arr) = MOID (old_arr);
    new_arr->elem_size = old_arr->elem_size;
    new_arr->slice_offset = 0;
    new_arr->field_offset = 0;
    for (k = 0, span = 1; k < DIM (old_arr); k++) {
      old_p = &old_tup[k];
      new_p = &new_tup[k];
      LWB (new_p) = LWB (old_p);
      UPB (new_p) = UPB (old_p);
      new_p->span = span;
      new_p->shift = LWB (new_p) * new_p->span;
      span *= ROW_SIZE (new_p);
    }
    ARRAY (new_arr) = heap_generator (p, m, span * new_arr->elem_size);
  } else if (WHETHER (m, ROW_SYMBOL)) {
/* In case of non-FLEX rows we check on equal length. */
    new_row = *dst;
    GET_DESCRIPTOR (new_arr, new_tup, &new_row);
    for (k = 0, span = 1; k < DIM (old_arr); k++) {
      old_p = &old_tup[k];
      new_p = &new_tup[k];
      if ((UPB (new_p) != UPB (old_p)) || (LWB (new_p) != LWB (old_p))) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      span *= ROW_SIZE (old_p);
    }
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_row");
  }
/* The n-dimensional copier. */
  initialise_internal_index (old_tup, DIM (old_arr));
  initialise_internal_index (new_tup, DIM (new_arr));
  if (span > 0) {
    unsigned elem_size = (unsigned) moid_size (MOID (old_arr));
    MOID_T *elem_mode = SUB (m);
    BYTE_T *old_elem = ADDRESS (&(ARRAY (old_arr))), *new_elem = ADDRESS (&(ARRAY (new_arr)));
    BOOL_T done = A68_FALSE;
    while (!done) {
      ADDR_T old_index, new_index, old_addr, new_addr;
      old_index = calculate_internal_index (old_tup, DIM (old_arr));
      new_index = calculate_internal_index (new_tup, DIM (new_arr));
      old_addr = ROW_ELEMENT (old_arr, old_index);
      new_addr = ROW_ELEMENT (new_arr, new_index);
      if (elem_mode->has_rows) {
/* Recursion. */
        A68_REF new_old = ARRAY (old_arr), new_dst = ARRAY (new_arr);
        BYTE_T *src_a, *dst_a;
        new_old.offset += old_addr;
        new_dst.offset += new_addr;
        src_a = ADDRESS (&new_old);
        dst_a = ADDRESS (&new_dst);
        if (WHETHER (elem_mode, STRUCT_SYMBOL)) {
          (void) genie_assign_stowed (new_old, &new_dst, p, elem_mode);
        } else if (WHETHER (elem_mode, FLEX_SYMBOL) || elem_mode == MODE (STRING)) {
/* Algol68G does not know ghost elements. NIL means an initially empty row. */
          A68_REF dst_addr = *(A68_REF *) dst_a;
          if (IS_NIL (dst_addr)) {
            *(A68_REF *) dst_a = *(A68_REF *) src_a;
          } else {
            *(A68_REF *) dst_a = genie_assign_stowed (*(A68_REF *) src_a, &dst_addr, p, elem_mode);
          }
        } else if (WHETHER (elem_mode, ROW_SYMBOL)) {
/* Algol68G does not know ghost elements. NIL means an initially empty row. */
          A68_REF dst_addr = *(A68_REF *) dst_a;
          if (IS_NIL (dst_addr)) {
            *(A68_REF *) dst_a = *(A68_REF *) src_a;
          } else {
            (void) genie_assign_stowed (*(A68_REF *) src_a, &dst_addr, p, elem_mode);
          }
        } else if (WHETHER (elem_mode, UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, new_old);
        } else if (elem_mode == MODE (SOUND)) {
          genie_copy_sound (p, dst_a, src_a);
        } else {
          ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_row");
        }
      } else {
        MOVE (&new_elem[new_addr], &old_elem[old_addr], elem_size);
      }
/* Increase pointers. */
      done = increment_internal_index (old_tup, DIM (old_arr)) | increment_internal_index (new_tup, DIM (new_arr));
    }
  }
  DOWN_SWEEP_SEMA;
  return (new_row);
}

/*!
\brief assign multiple value of mode 'm' from 'old' to 'dst'
\param old pointer to old value
\param dst pointer to destination
\param p position in tree
\param m mode of object
\return fat pointer to descriptor or structured value
**/

A68_REF genie_assign_stowed (A68_REF old, A68_REF * dst, NODE_T * p, MOID_T * m)
{
  if (WHETHER (m, STRUCT_SYMBOL)) {
    PACK_T *fields;
    A68_REF new_struct;
    UP_SWEEP_SEMA;
    new_struct = *dst;
    for (fields = PACK (m); fields != NULL; fields = NEXT (fields)) {
      A68_REF old_field = old, new_field = new_struct;
      BYTE_T *src_a, *dst_a;
      old_field.offset += fields->offset;
      new_field.offset += fields->offset;
      src_a = ADDRESS (&old_field);
      dst_a = ADDRESS (&new_field);
      if (MOID (fields)->has_rows) {
        if (WHETHER (MOID (fields), STRUCT_SYMBOL)) {
/* STRUCT (STRUCT (..) ..) */
          (void) genie_assign_stowed (old_field, &new_field, p, MOID (fields));
        } else if (WHETHER (MOID (fields), FLEX_SYMBOL) || MOID (fields) == MODE (STRING)) {
/* STRUCT (FLEX [] A ..) */
          *(A68_REF *) dst_a = genie_copy_row (*(A68_REF *) src_a, p, MOID (fields));
        } else if (WHETHER (MOID (fields), ROW_SYMBOL)) {
/* STRUCT ([] A ..) */
          A68_REF arr_src = *(A68_REF *) src_a;
          A68_REF arr_dst = *(A68_REF *) dst_a;
          (void) genie_assign_row (arr_src, &arr_dst, p, MOID (fields));
        } else if (WHETHER (MOID (fields), UNION_SYMBOL)) {
/* UNION (..) */
          genie_copy_union (p, dst_a, src_a, old_field);
        } else if (MOID (fields) == MODE (SOUND)) {
          genie_copy_sound (p, dst_a, src_a);
        } else {
          ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_stowed");
        }
      } else {
        MOVE (dst_a, src_a, MOID_SIZE (fields));
      }
    }
    DOWN_SWEEP_SEMA;
    return (new_struct);
  } else if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
    A68_REF new_row;
    UP_SWEEP_SEMA;
    new_row = genie_assign_row (old, dst, p, m);
    DOWN_SWEEP_SEMA;
    return (new_row);
  } else if (WHETHER (m, ROW_SYMBOL)) {
    A68_REF new_row;
    UP_SWEEP_SEMA;
    new_row = genie_assign_row (old, dst, p, m);
    DOWN_SWEEP_SEMA;
    return (new_row);
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_stowed");
    return (nil_ref);
  }
}

/*!
\brief dump a stowed object for debugging purposes
\param f file to dump to
\param w pointer to struct, row or union
\param m mode of object that p points to
\param level recursion level
**/

#define INDENT(n) {\
  int j;\
  WRITE (f, "\n");\
  for (j = 0; j < n; j++) {\
    WRITE (f, " ");\
  }}

void dump_stowed (NODE_T * p, FILE_T f, void *w, MOID_T * m, int level)
{
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BOOL_T done = A68_FALSE;
  int span, k;
  char buf[BUFFER_SIZE];
  INDENT (level);
  ASSERT (snprintf (buf, (size_t) BUFFER_SIZE, "%s at %p pointing at %p", moid_to_string (m, 80, NULL), w, (void *) ADDRESS ((A68_ROW *) w)) >= 0);
  WRITE (f, buf);
  if (IS_NIL (*(A68_REF *) w)) {
    INDENT (level);
    ASSERT (snprintf (buf, (size_t) BUFFER_SIZE, "NIL - returning") >= 0);
    WRITE (f, buf);
    return;
  }
  if (WHETHER (m, STRUCT_SYMBOL)) {
    PACK_T *fields;
    for (fields = PACK (m); fields != NULL; fields = NEXT (fields)) {
      if (MOID (fields)->has_rows) {
        dump_stowed (p, f, &((BYTE_T *) w)[fields->offset], MOID (fields), level + 1);
      } else {
        INDENT (level);
        ASSERT (snprintf (buf, (size_t) BUFFER_SIZE, "%s %s at %p", moid_to_string (MOID (fields), 80, NULL), fields->text, &((BYTE_T *) w)[fields->offset]) >= 0);
        WRITE (f, buf);
      }
    }
  } else if (WHETHER (m, UNION_SYMBOL)) {
    A68_UNION *u = (A68_UNION *) w;
    MOID_T *um = (MOID_T *) VALUE (u);
    if (um != NULL) {
      if (um->has_rows) {
        dump_stowed (p, f, &((BYTE_T *) w)[UNION_OFFSET], um, level + 1);
      } else {
        ASSERT (snprintf (buf, (size_t) BUFFER_SIZE, " holds %s at %p", moid_to_string (um, 80, NULL), &((BYTE_T *) w)[UNION_OFFSET]) >= 0);
        WRITE (f, buf);
      }
    }
  } else {
    if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
      m = SUB (m);
    }
    GET_DESCRIPTOR (arr, tup, (A68_ROW *) w);
    for (k = 0, span = 1; k < DIM (arr); k++) {
      A68_TUPLE *z = &tup[k];
      INDENT (level);
      ASSERT (snprintf (buf, (size_t) BUFFER_SIZE, "tuple %d has lwb=%d and upb=%d", k, LWB (z), UPB (z)) >= 0);
      WRITE (f, buf);
      span *= ROW_SIZE (z);
    }
    INDENT (level);
    ASSERT (snprintf (buf, (size_t) BUFFER_SIZE, "elems=%d, elem size=%d, slice_offset=%d, field_offset=%d", span, arr->elem_size, arr->slice_offset, arr->field_offset) >= 0);
    WRITE (f, buf);
    if (span > 0) {
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        A68_REF elem = ARRAY (arr);
        BYTE_T *elem_p;
        MOID_T *elem_mode = SUB (m);
        ADDR_T iindex = calculate_internal_index (tup, DIM (arr));
        ADDR_T addr = ROW_ELEMENT (arr, iindex);
        elem.offset += addr;
        elem_p = ADDRESS (&elem);
        if (elem_mode->has_rows) {
          dump_stowed (p, f, elem_p, elem_mode, level + 3);
        } else {
          INDENT (level);
          ASSERT (snprintf (buf, (size_t) BUFFER_SIZE, "%s [%d] at %p", moid_to_string (elem_mode, 80, NULL), iindex, elem_p) >= 0);
          WRITE (f, buf);
/*        print_item (p, f, elem_p, elem_mode); */
        }
/* Increase pointers. */
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
}

#undef INDENT

/* Operators for ROWS. */

/*!
\brief OP ELEMS = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, get_row_size (t, DIM (x)), A68_INT);
}

/*!
\brief OP LWB = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, LWB (t), A68_INT);
}

/*!
\brief OP UPB = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, UPB (t), A68_INT);
}

/*!
\brief OP ELEMS = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t, *u;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  u = &(t[VALUE (&k) - 1]);
  PUSH_PRIMITIVE (p, ROW_SIZE (u), A68_INT);
}

/*!
\brief OP LWB = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, t[VALUE (&k) - 1].lower_bound, A68_INT);
}

/*!
\brief OP UPB = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, t[VALUE (&k) - 1].upper_bound, A68_INT);
}

/*!
\brief push description for diagonal of square matrix
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_diagonal_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROPAGATOR_T self;
  A68_ROW row, new_row;
  int k = 0;
  BOOL_T name = (BOOL_T) (WHETHER (MOID (p), REF_SYMBOL));
  A68_ARRAY *arr, new_arr;
  A68_TUPLE *tup1, *tup2, new_tup;
  MOID_T *m;
  UP_SWEEP_SEMA;
  if (WHETHER (q, TERTIARY)) {
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
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  if (ROW_SIZE (tup1) != ROW_SIZE (tup2)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_NO_SQUARE_MATRIX, m, NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (ABS (k) >= ROW_SIZE (tup1)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS, NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 1;
  MOID (&new_arr) = m;
  new_arr.elem_size = arr->elem_size;
  new_arr.slice_offset = arr->slice_offset;
  new_arr.field_offset = arr->field_offset;
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&new_tup) = 1;
  UPB (&new_tup) = ROW_SIZE (tup1) - ABS (k);
  new_tup.shift = tup1->shift + tup2->shift - k * tup2->span;
  if (k < 0) {
    new_tup.shift -= (-k) * (tup1->span + tup2->span);
  }
  new_tup.span = tup1->span + tup2->span;
  new_tup.k = 0;
  PUT_DESCRIPTOR (new_arr, new_tup, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&ref_new) = new_row;
    SET_REF_SCOPE (&ref_new, scope);
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  DOWN_SWEEP_SEMA;
  self.unit = genie_diagonal_function;
  self.source = p;
  return (self);
}

/*!
\brief push description for transpose of matrix
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_transpose_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROPAGATOR_T self;
  A68_ROW row, new_row;
  BOOL_T name = (BOOL_T) (WHETHER (MOID (p), REF_SYMBOL));
  A68_ARRAY *arr, new_arr;
  A68_TUPLE *tup1, *tup2, new_tup1, new_tup2;
  MOID_T *m;
  UP_SWEEP_SEMA;
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + 2 * ALIGNED_SIZE_OF (A68_TUPLE));
  new_arr = *arr;
  new_tup1 = *tup2;
  new_tup2 = *tup1;
  PUT_DESCRIPTOR2 (new_arr, new_tup1, new_tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&ref_new) = new_row;
    SET_REF_SCOPE (&ref_new, scope);
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  DOWN_SWEEP_SEMA;
  self.unit = genie_transpose_function;
  self.source = p;
  return (self);
}

/*!
\brief push description for row vector
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_row_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROPAGATOR_T self;
  A68_ROW row, new_row;
  int k = 1;
  BOOL_T name = (BOOL_T) (WHETHER (MOID (p), REF_SYMBOL));
  A68_ARRAY *arr, new_arr;
  A68_TUPLE tup1, tup2, *tup;
  MOID_T *m;
  UP_SWEEP_SEMA;
  if (WHETHER (q, TERTIARY)) {
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
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  if (DIM (arr) != 1) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_NO_VECTOR, m, PRIMARY, NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 2;
  MOID (&new_arr) = m;
  new_arr.elem_size = arr->elem_size;
  new_arr.slice_offset = arr->slice_offset;
  new_arr.field_offset = arr->field_offset;
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&tup1) = k;
  UPB (&tup1) = k;
  tup1.span = 1;
  tup1.shift = k * tup1.span;
  tup1.k = 0;
  LWB (&tup2) = 1;
  UPB (&tup2) = ROW_SIZE (tup);
  tup2.span = tup->span;
  tup2.shift = tup->span;
  tup2.k = 0;
  PUT_DESCRIPTOR2 (new_arr, tup1, tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&ref_new) = new_row;
    SET_REF_SCOPE (&ref_new, scope);
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  DOWN_SWEEP_SEMA;
  self.unit = genie_row_function;
  self.source = p;
  return (self);
}

/*!
\brief push description for column vector
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_column_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROPAGATOR_T self;
  A68_ROW row, new_row;
  int k = 1;
  BOOL_T name = (BOOL_T) (WHETHER (MOID (p), REF_SYMBOL));
  A68_ARRAY *arr, new_arr;
  A68_TUPLE tup1, tup2, *tup;
  MOID_T *m;
  UP_SWEEP_SEMA;
  if (WHETHER (q, TERTIARY)) {
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
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  m = (name ? SUB_MOID (p) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 2;
  MOID (&new_arr) = m;
  new_arr.elem_size = arr->elem_size;
  new_arr.slice_offset = arr->slice_offset;
  new_arr.field_offset = arr->field_offset;
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&tup1) = 1;
  UPB (&tup1) = ROW_SIZE (tup);
  tup1.span = tup->span;
  tup1.shift = tup->span;
  tup1.k = 0;
  LWB (&tup2) = k;
  UPB (&tup2) = k;
  tup2.span = 1;
  tup2.shift = k * tup2.span;
  tup2.k = 0;
  PUT_DESCRIPTOR2 (new_arr, tup1, tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&ref_new) = new_row;
    SET_REF_SCOPE (&ref_new, scope);
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  DOWN_SWEEP_SEMA;
  self.unit = genie_column_function;
  self.source = p;
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
    if (ptrs == NULL) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* Copy C-strings into the stack and sort. */
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
      ASSERT (a_to_c_string (p, (char *) STACK_TOP, ref) != NULL);
      INCREMENT_STACK_POINTER (p, len);
    }
    qsort (ptrs, (size_t) size, sizeof (char *), qstrcmp);
/* Construct an array of sorted strings. */
    z = heap_generator (p, MODE (ROW_STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_STRING), size * MOID_SIZE (MODE (STRING)));
    DIM (&arrn) = 1;
    MOID (&arrn) = MODE (STRING);
    arrn.elem_size = MOID_SIZE (MODE (STRING));
    arrn.slice_offset = 0;
    arrn.field_offset = 0;
    ARRAY (&arrn) = row;
    LWB (&tupn) = 1;
    UPB (&tupn) = size;
    tupn.shift = LWB (&tupn);
    tupn.span = 1;
    tupn.k = 0;
    PUT_DESCRIPTOR (arrn, tupn, &z);
    base_ref = (A68_REF *) ADDRESS (&row);
    for (k = 0; k < size; k++) {
      base_ref[k] = c_to_a_string (p, ptrs[k]);
    }
    free (ptrs);
    stack_pointer = pop_sp;
    PUSH_REF (p, z);
  } else {
/* This is how we sort an empty row of strings ... */
    stack_pointer = pop_sp;
    PUSH_REF (p, empty_row (p, MODE (ROW_STRING)));
  }
}

/*
Generator and garbage collector routines.

The generator allocates space in stack or heap and initialises dynamically sized objects.

A mark-and-sweep garbage collector defragments the heap. When called, it walks
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
heap clogs, we try to keep heap occupation low by sweeping the heap
occasionally, before it fills up completely. If this automatic mechanism does
not help, one can always invoke the garbage collector by calling "sweep heap"
from Algol 68 source text.

Mark-and-sweep is simple but since it walks recursive structures, it could
exhaust the C-stack (segment violation). A rough check is in place.
*/

void colour_object (BYTE_T *, MOID_T *);
void sweep_heap (NODE_T *, ADDR_T);
int garbage_collects, garbage_bytes_freed;
int free_handle_count, max_handle_count;
int block_heap_compacter;
A68_HANDLE *free_handles, *busy_handles;
double garbage_seconds;

#define DEF(p) (NEXT_NEXT (NODE (TAX (p))))
#define MAX(u, v) ((u) = ((u) > (v) ? (u) : (v)))

A68_REF genie_allocate_declarer (NODE_T *, ADDR_T *, A68_REF, BOOL_T);
void genie_generator_stowed (NODE_T *, BYTE_T *, NODE_T **, ADDR_T *, ADDR_T *);

/* Total freed is kept in a LONG INT. */
MP_DIGIT_T garbage_total_freed[LONG_MP_DIGITS + 2];
static MP_DIGIT_T garbage_freed[LONG_MP_DIGITS + 2];

extern int block_heap_compacter;
/*!
\brief PROC VOID sweep heap
\param p position in tree
**/

void genie_sweep_heap (NODE_T * p)
{
  sweep_heap (p, frame_pointer);
}

/*!
\brief PROC VOID preemptive sweep heap
\param p position in tree
**/

void genie_preemptive_sweep_heap (NODE_T * p)
{
  PREEMPTIVE_SWEEP;
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
/* Note that this timing is a rough cut. */
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
  if (heap_segment == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, program.top_node, ERROR_OUT_OF_CORE);
    exit_genie (program.top_node, A68_RUNTIME_ERROR);
  }
  if (handle_segment == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, program.top_node, ERROR_OUT_OF_CORE);
    exit_genie (program.top_node, A68_RUNTIME_ERROR);
  }
  block_heap_compacter = 0;
  garbage_seconds = 0;
  SET_MP_ZERO (garbage_total_freed, LONG_MP_DIGITS);
  garbage_collects = 0;
  ABEND (fixed_heap_pointer >= heap_size, ERROR_OUT_OF_CORE, NULL);
  heap_pointer = fixed_heap_pointer;
  get_fixed_heap_allowed = A68_FALSE;
/* Assign handle space. */
  z = (A68_HANDLE *) handle_segment;
  free_handles = z;
  busy_handles = NULL;
  max = (int) handle_pool_size / (int) sizeof (A68_HANDLE);
  free_handle_count = max;
  max_handle_count = max;
  for (k = 0; k < max; k++) {
    STATUS (&(z[k])) = NULL_MASK;
    POINTER (&(z[k])) = NULL;
    SIZE (&(z[k])) = 0;
    NEXT (&z[k]) = (k == max - 1 ? NULL : &z[k + 1]);
    PREVIOUS (&z[k]) = (k == 0 ? NULL : &z[k - 1]);
  }
}

/*!
\brief whether mode must be coloured
\param m moid to colour
\return same
**/

static BOOL_T moid_needs_colouring (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (A68_TRUE);
  } else if (WHETHER (m, PROC_SYMBOL)) {
    return (A68_TRUE);
  } else if (WHETHER (m, FLEX_SYMBOL) || WHETHER (m, ROW_SYMBOL)) {
    return (A68_TRUE);
  } else if (WHETHER (m, STRUCT_SYMBOL) || WHETHER (m, UNION_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NULL; FORWARD (p)) {
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
/* Empty rows are trivial since we don't recognise ghost elements. */
  if (get_row_size (tup, DIM (arr)) > 0) {
/* The multi-dimensional sweeper. */
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
  if (item == NULL || m == NULL) {
    return;
  }
/* Deeply recursive objects might exhaust the stack. */
  LOW_STACK_ALERT (NULL);
  if (WHETHER (m, REF_SYMBOL)) {
/* REF AMODE colour pointer and object to which it refers. */
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
  } else if (WHETHER (m, FLEX_SYMBOL) || WHETHER (m, ROW_SYMBOL) || m == MODE (STRING)) {
/* Claim the descriptor and the row itself. */
    A68_REF *z = (A68_REF *) item;
    if (INITIALISED (z) && IS_IN_HEAP (z)) {
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      if (STATUS_TEST (REF_HANDLE (z), COOKIE_MASK)) {
        return;
      }
/* An array is ALWAYS in the heap. */
      STATUS_SET (REF_HANDLE (z), (COOKIE_MASK | COLOUR_MASK));
      GET_DESCRIPTOR (arr, tup, z);
      if (REF_HANDLE (&(ARRAY (arr))) != NULL) {
/* Assume its initialisation. */
        MOID_T *n = DEFLEX (m);
        STATUS_SET (REF_HANDLE (&(ARRAY (arr))), COLOUR_MASK);
        if (moid_needs_colouring (SUB (n))) {
          colour_row_elements (z, n);
        }
      }
      STATUS_CLEAR (REF_HANDLE (z), COOKIE_MASK);
    }
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
/* STRUCTures - colour fields. */
    PACK_T *p = PACK (m);
    for (; p != NULL; FORWARD (p)) {
      colour_object (&item[OFFSET (p)], MOID (p));
    }
  } else if (WHETHER (m, UNION_SYMBOL)) {
/* UNIONs - a united object may contain a value that needs colouring. */
    A68_UNION *z = (A68_UNION *) item;
    if (INITIALISED (z)) {
      MOID_T *united_moid = (MOID_T *) VALUE (z);
      colour_object (&item[ALIGNED_SIZE_OF (A68_UNION)], united_moid);
    }
  } else if (WHETHER (m, PROC_SYMBOL)) {
/* PROCs - save a locale and the objects it points to. */
    A68_PROCEDURE *z = (A68_PROCEDURE *) item;
    if (INITIALISED (z) && LOCALE (z) != NULL && !(STATUS_TEST (LOCALE (z), COOKIE_MASK))) {
      BYTE_T *u = POINTER (LOCALE (z));
      PACK_T *s = PACK (MOID (z));
      STATUS_SET (LOCALE (z), (COOKIE_MASK | COLOUR_MASK));
      for (; s != NULL; FORWARD (s)) {
        if (VALUE ((A68_BOOL *) & u[0]) == A68_TRUE) {
          colour_object (&u[ALIGNED_SIZE_OF (A68_BOOL)], MOID (s));
        }
        u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      }
      STATUS_CLEAR (LOCALE (z), COOKIE_MASK);
    }
  } else if (m == MODE (SOUND)) {
/* Claim the data of a SOUND object, that is in the heap. */
    A68_SOUND *w = (A68_SOUND *) item;
    if (INITIALISED (w)) {
      STATUS_SET (REF_HANDLE (&(w->data)), (COOKIE_MASK | COLOUR_MASK));
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
    SYMBOL_TABLE_T *q = SYMBOL_TABLE (p);
    if (q != NULL) {
      TAG_T *i;
      for (i = q->identifiers; i != NULL; FORWARD (i)) {
        colour_object (FRAME_LOCAL (fp, OFFSET (i)), MOID (i));
      }
      for (i = q->anonymous; i != NULL; FORWARD (i)) {
        if (PRIO (i) == GENERATOR || PRIO (i) == PROTECT_FROM_SWEEP) {
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
/* Free handles. */
  z = busy_handles;
  while (z != NULL) {
    if (!(STATUS_TEST (z, COLOUR_MASK)) && !(STATUS_TEST (z, NO_SWEEP_MASK))) {
      A68_HANDLE *y = NEXT (z);
      if (PREVIOUS (z) == NULL) {
        busy_handles = NEXT (z);
      } else {
        NEXT (PREVIOUS (z)) = NEXT (z);
      }
      if (NEXT (z) != NULL) {
        PREVIOUS (NEXT (z)) = PREVIOUS (z);
      }
      NEXT (z) = free_handles;
      PREVIOUS (z) = NULL;
      if (NEXT (z) != NULL) {
        PREVIOUS (NEXT (z)) = z;
      }
      free_handles = z;
      STATUS_CLEAR (z, ALLOCATED_MASK);
      garbage_bytes_freed += z->size;
      free_handle_count++;
      z = y;
    } else {
      FORWARD (z);
    }
  }
/* There can be no uncoloured allocated handle. */
  for (z = busy_handles; z != NULL; FORWARD (z)) {
    ABEND (!(STATUS_TEST (z, COLOUR_MASK)) && !(STATUS_TEST (z, NO_SWEEP_MASK)), "bad GC consistency", NULL);
  }
/* Defragment the heap. */
  heap_pointer = fixed_heap_pointer;
  for (z = busy_handles; z != NULL && NEXT (z) != NULL; FORWARD (z)) {
    ;
  }
  for (; z != NULL; z = PREVIOUS (z)) {
    BYTE_T *dst = HEAP_ADDRESS (heap_pointer);
    if (dst != POINTER (z)) {
      MOVE (dst, POINTER (z), (unsigned) z->size);
    }
    STATUS_CLEAR (z, (COLOUR_MASK | COOKIE_MASK));
    POINTER (z) = dst;
    heap_pointer += (z->size);
    ABEND (heap_pointer % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
  }
}

/*!
\brief clean up garbage and defragment the heap
\param p position in tree
\param fp running frame pointer
**/

void sweep_heap (NODE_T * p, ADDR_T fp)
{
/* Must start with fp = current frame_pointer. */
  A68_HANDLE *z;
  double t0, t1;
  t0 = seconds ();
  if (block_heap_compacter == 0) {
/* Unfree handles are subject to inspection. */
    for (z = busy_handles; z != NULL; FORWARD (z)) {
      STATUS_CLEAR (z, (COLOUR_MASK | COOKIE_MASK));
    }
/* Pour paint into the heap to reveal active objects. */
    colour_heap (fp);
/* Start freeing and compacting. */
    garbage_bytes_freed = 0;
    defragment_heap ();
/* Stats and logging. */
    garbage_collects++;
    (void) int_to_mp (p, garbage_freed, (int) garbage_bytes_freed, LONG_MP_DIGITS);
    (void) add_mp (p, garbage_total_freed, garbage_total_freed, garbage_freed, LONG_MP_DIGITS);
  }
  t1 = seconds ();
/* C optimiser can make last digit differ, so next condition is 
   needed to determine a positive time difference. */
  if ((t1 - t0) > ((double) clock_res / 2.0)) {
    garbage_seconds += (t1 - t0);
  } else {
    garbage_seconds += ((double) clock_res / 2.0);
  }
}

/*!
\brief yield a handle that will point to a block in the heap
\param p position in tree
\param a68m mode of object
\return handle that points to object
**/

static A68_HANDLE *give_handle (NODE_T * p, MOID_T * a68m)
{
  if (free_handles != NULL) {
    A68_HANDLE *x = free_handles;
    free_handles = NEXT (x);
    if (free_handles != NULL) {
      PREVIOUS (free_handles) = NULL;
    }
    STATUS (x) = ALLOCATED_MASK;
    POINTER (x) = NULL;
    SIZE (x) = 0;
    MOID (x) = a68m;
    NEXT (x) = busy_handles;
    PREVIOUS (x) = NULL;
    if (NEXT (x) != NULL) {
      PREVIOUS (NEXT (x)) = x;
    }
    busy_handles = x;
    free_handle_count--;
    return (x);
  } else {
    sweep_heap (p, frame_pointer);
    if (free_handles != NULL) {
      return (give_handle (p, a68m));
    } else {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  return (NULL);
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
/* Align. */
  ABEND (size < 0, ERROR_INVALID_SIZE, NULL);
  size = A68_ALIGN (size);
/* Now give it. */
  if (heap_available () >= size) {
    A68_HANDLE *x;
    A68_REF z;
    PREEMPTIVE_SWEEP;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | IN_HEAP_MASK);
    OFFSET (&z) = 0;
    x = give_handle (p, mode);
    SIZE (x) = size;
    POINTER (x) = HEAP_ADDRESS (heap_pointer);
    FILL (x->pointer, 0, size);
    SET_REF_SCOPE (&z, PRIMAL_SCOPE);
    REF_HANDLE (&z) = x;
    ABEND (((long) ADDRESS (&z)) % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
    heap_pointer += size;
    return (z);
  } else {
/* No heap space. First sweep the heap. */
    sweep_heap (p, frame_pointer);
    if (heap_available () > size) {
      return (heap_generator (p, mode, size));
    } else {
/* Still no heap space. We must abend. */
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
      return (nil_ref);
    }
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

static BOOL_T needs_allocation (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (A68_FALSE);
  }
  if (WHETHER (m, PROC_SYMBOL)) {
    return (A68_FALSE);
  }
  if (WHETHER (m, UNION_SYMBOL)) {
    return (A68_FALSE);
  }
  if (m == MODE (VOID)) {
    return (A68_FALSE);
  }
  return (A68_TRUE);
}

/*!
\brief prepare bounds
\param p position in tree
**/

static void genie_prepare_bounds (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOUNDS_LIST)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, BOUND)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, UNIT)) {
      if (NEXT (p) != NULL && (whether_one_of (NEXT (p), COLON_SYMBOL, DOTDOT_SYMBOL, NULL_ATTRIBUTE))) {
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
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOUNDS)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, INDICANT)) {
      if (TAX (p) != NULL && MOID (TAX (p))->has_rows) {
        /* Continue from definition at MODE A = .. */
        genie_generator_bounds (DEF (p));
      }
    } else if (WHETHER (p, DECLARER) && needs_allocation (MOID (p)) == A68_FALSE) {
      return;
    } else {
      genie_generator_bounds (SUB (p));
    }
  }
}

/*!
\brief allocate a structure
\param p declarer in the syntax tree
**/

void genie_generator_field (NODE_T * p, BYTE_T ** q, NODE_T ** declarer, ADDR_T * sp, ADDR_T * max_sp)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, STRUCTURED_FIELD)) {
      genie_generator_field (SUB (p), q, declarer, sp, max_sp);
    }
    if (WHETHER (p, DECLARER)) {
      (*declarer) = SUB (p);
      FORWARD (p);
    }
    if (WHETHER (p, FIELD_IDENTIFIER)) {
      MOID_T *field_mode = MOID (*declarer);
      ADDR_T pop_sp = *sp;
      if (field_mode->has_rows && WHETHER_NOT (field_mode, UNION_SYMBOL)) {
        genie_generator_stowed (*declarer, *q, NULL, sp, max_sp);
      } else {
        MAX (*max_sp, *sp);
      }
      (*sp) = pop_sp;
      (*q) += MOID_SIZE (field_mode);
    }
  }
}

/*!
\brief allocate a structure
\param p declarer in the syntax tree
**/

void genie_generator_struct (NODE_T * p, BYTE_T ** q, ADDR_T * sp, ADDR_T * max_sp)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, STRUCTURED_FIELD_LIST)) {
      genie_generator_struct (SUB (p), q, sp, max_sp);
    } else if (WHETHER (p, STRUCTURED_FIELD)) {
      NODE_T *declarer = NULL;
      ADDR_T bla = *max_sp;
      genie_generator_field (SUB (p), q, &declarer, sp, &bla);
      *max_sp = bla;
      *sp = *max_sp;
    }
  }
}

/*!
\brief allocate a stowed object
\param p declarer in the syntax tree
**/

void genie_generator_stowed (NODE_T * p, BYTE_T * q, NODE_T ** declarer, ADDR_T * sp, ADDR_T * max_sp)
{
  if (p == NULL) {
    return;
  }
  if (WHETHER (p, INDICANT)) {
    if (MOID (p) == MODE (STRING)) {
      *((A68_REF *) q) = empty_string (p);
    } else if (TAX (p) != NULL) {
/* Continue from definition at MODE A = .. */
      genie_generator_stowed (DEF (p), q, declarer, sp, max_sp);
    }
    return;
  }
  if (WHETHER (p, DECLARER) && needs_allocation (MOID (p))) {
    genie_generator_stowed (SUB (p), q, declarer, sp, max_sp);
    return;
  }
  if (WHETHER (p, STRUCT_SYMBOL)) {
    BYTE_T *r = q;
    genie_generator_struct (SUB_NEXT (p), &r, sp, max_sp);
    return;
  }
  if (WHETHER (p, FLEX_SYMBOL)) {
    FORWARD (p);
  }
  if (WHETHER (p, BOUNDS)) {
    ADDR_T bla = *max_sp;
    A68_REF desc;
    MOID_T *slice_mode = MOID (NEXT (p));
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    BYTE_T *bounds = STACK_ADDRESS (*sp);
    int k, dim = DIM (DEFLEX (MOID (p)));
    int elem_size = MOID_SIZE (slice_mode), row_size = 1;
    UP_SWEEP_SEMA;
    desc = heap_generator (p, MOID (p), dim * ALIGNED_SIZE_OF (A68_TUPLE) + ALIGNED_SIZE_OF (A68_ARRAY));
    GET_DESCRIPTOR (arr, tup, &desc);
    for (k = 0; k < dim; k++) {
      tup[k].lower_bound = VALUE ((A68_INT *) bounds);
      bounds += ALIGNED_SIZE_OF (A68_INT);
      tup[k].upper_bound = VALUE ((A68_INT *) bounds);
      bounds += ALIGNED_SIZE_OF (A68_INT);
      tup[k].span = row_size;
      tup[k].shift = tup[k].lower_bound * tup[k].span;
      row_size *= ROW_SIZE (&tup[k]);
    }
    DIM (arr) = dim;
    MOID (arr) = slice_mode;
    arr->elem_size = elem_size;
    arr->slice_offset = 0;
    arr->field_offset = 0;
    ARRAY (arr) = heap_generator (p, MOID (p), row_size * elem_size);
    (*sp) += (dim * 2 * ALIGNED_SIZE_OF (A68_INT));
    MAX (bla, *sp);
    if (slice_mode->has_rows && needs_allocation (slice_mode)) {
      BYTE_T *elem = ADDRESS (&(ARRAY (arr)));
      for (k = 0; k < row_size; k++) {
        ADDR_T pop_sp = *sp;
        bla = *max_sp;
        genie_generator_stowed (NEXT (p), &(elem[k * elem_size]), NULL, sp, &bla);
        *sp = pop_sp;
      }
    }
    *max_sp = bla;
    *sp = *max_sp;
    *(A68_REF *) q = desc;
    DOWN_SWEEP_SEMA;
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
  A68_REF name;
  UP_SWEEP_SEMA;
/* Set up a REF MODE object, either in the stack or in the heap. */
  if (leap == LOC_SYMBOL) {
    STATUS (&name) = (STATUS_MASK) (INITIALISED_MASK | IN_FRAME_MASK);
    REF_HANDLE (&name) = &nil_handle;
    name.offset = frame_pointer + FRAME_INFO_SIZE + OFFSET (tag);
    SET_REF_SCOPE (&name, frame_pointer);
  } else {
    name = heap_generator (p, mode, MOID_SIZE (mode));
    SET_REF_SCOPE (&name, PRIMAL_SCOPE);
  }
  if (mode->has_rows) {
    ADDR_T cur_sp = sp, max_sp = sp;
    genie_generator_stowed (p, ADDRESS (&name), NULL, &cur_sp, &max_sp);
  }
  PUSH_REF (p, name);
  DOWN_SWEEP_SEMA;
}

/*!
\brief push a name refering to allocated space
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_generator (NODE_T * p)
{
  PROPAGATOR_T self;
  ADDR_T pop_sp = stack_pointer;
  A68_REF z;
  if (NEXT_SUB (p) != NULL) {
    genie_generator_bounds (NEXT_SUB (p));
  }
  genie_generator_internal (NEXT_SUB (p), MOID (p), TAX (p), ATTRIBUTE (SUB (p)), pop_sp);
  POP_REF (p, &z);
  stack_pointer = pop_sp;
  PUSH_REF (p, z);
  PROTECT_FROM_SWEEP_STACK (p);
  self.unit = genie_generator;
  self.source = p;
  return (self);
}

/*!
Implements a parallel clause.

This code implements a parallel clause for Algol68G. This parallel clause has
been included for educational purposes, and this implementation just emulates
a multi-processor machine. It cannot make use of actual multiple processors.

POSIX threads are used to have separate registers and stack for each concurrent 
unit. Algol68G parallel units behave as POSIX threads - they have private stacks.
Hence an assignation to an object in another thread, does not change that object
in that other thread. Also jumps between threads are forbidden.
*/

#if defined ENABLE_PAR_CLAUSE

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
    if (!((s != NULL) && (s->bytes > 0) && (size <= s->bytes))) {\
      if (s->swap != NULL) {\
        free (s->swap);\
      }\
      s->swap = (BYTE_T *) malloc ((size_t) size);\
      ABEND (s->swap == NULL, ERROR_OUT_OF_CORE, NULL);\
    }\
    s->start = start;\
    s->bytes = size;\
    COPY (s->swap, start, size);\
  } else {\
    s->start = start;\
    s->bytes = 0;\
    if (s->swap != NULL) {\
      free (s->swap);\
    }\
    s->swap = NULL;\
  }}

#define RESTORE_STACK(stk) {\
  A68_STACK_DESCRIPTOR *s = (stk);\
  if (s != NULL && s->bytes > 0) {\
    COPY (s->start, s->swap, s->bytes);\
  }}

#define GET_THREAD_INDEX(z, ptid) {\
  int _k_;\
  pthread_t _tid_ = (ptid);\
  (z) = -1;\
  for (_k_ = 0; _k_ < context_index && (z) == -1; _k_++) {\
    if (pthread_equal (_tid_, context[_k_].id)) {\
      (z) = _k_;\
    }\
  }\
  ABEND ((z) == -1, "thread id not registered", NULL);\
  }

#define ERROR_THREAD_FAULT "thread fault"

#define LOCK_THREAD {\
  ABEND (pthread_mutex_lock (&unit_sema) != 0, ERROR_THREAD_FAULT, NULL);\
  }

#define UNLOCK_THREAD {\
  ABEND (pthread_mutex_unlock (&unit_sema) != 0, ERROR_THREAD_FAULT, NULL);\
  }

/*!
\brief does system stack grow up or down?
\param lwb BYTE in the stack to calibrate direction
\return 1 if stackpointer increases, -1 if stackpointer decreases, 0 in case of error
**/

static int stack_direction (BYTE_T * lwb)
{
  BYTE_T upb;
  if ((int) &upb > (int) lwb) {
    return (1);
  } else if ((int) &upb < (int) lwb) {
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
  for (; p != NULL; p = NEXT (p)) {
    if (WHETHER (p, PARALLEL_CLAUSE)) {
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

BOOL_T whether_main_thread (void)
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
  context[k].active = A68_FALSE;
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
  if (!whether_main_thread ()) {
    genie_abend_thread ();
  }
}

/*!
\brief save this thread and try to start another
\param p position in tree
**/

static void try_change_thread (NODE_T * p)
{
  if (whether_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
/* Release the unit_sema so another thread can take it up ... */
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
/* Store stack pointers. */
  context[k].frame.cur_ptr = frame_pointer;
  context[k].stack.cur_ptr = stack_pointer;
/* Swap out evaluation stack. */
  p = stack_pointer;
  q = context[k].stack.ini_ptr;
  SAVE_STACK (&(context[k].stack), STACK_ADDRESS (q), p - q);
/* Swap out frame stack. */
  p = frame_pointer;
  q = context[k].frame.ini_ptr;
  u = p + FRAME_SIZE (p);
  v = q + FRAME_SIZE (q);
/* Consider the embedding thread. */
  SAVE_STACK (&(context[k].frame), FRAME_ADDRESS (v), u - v);
}

/*!
\brief restore stacks of thread
\param t thread number
**/

static void restore_stacks (pthread_t t)
{
  if (program.error_count > 0 || abend_all_threads) {
    genie_abend_thread ();
  } else {
    int k;
    GET_THREAD_INDEX (k, t);
/* Restore stack pointers. */
    get_stack_size ();
    system_stack_offset = context[k].thread_stack_offset;
    frame_pointer = context[k].frame.cur_ptr;
    stack_pointer = context[k].stack.cur_ptr;
/* Restore stacks. */
    RESTORE_STACK (&(context[k].stack));
    RESTORE_STACK (&(context[k].frame));
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
    if (parent == context[k].parent) {
      (*active) |= context[k].active;
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
  context[k].thread_stack_offset = (BYTE_T *) ((int) &stack_offset - stack_direction (&stack_offset) * context[k].stack_used);
  restore_stacks (t);
  p = (NODE_T *) (context[k].unit);
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
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      pthread_t new_id;
      pthread_attr_t new_at;
      size_t ss;
      BYTE_T stack_offset;
      A68_THREAD_CONTEXT *u;
/* Set up a thread for this unit. */
      if (context_index >= THREAD_MAX) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OVERFLOW);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
/* Fill out a context for this thread. */
      u = &(context[context_index]);
      u->unit = p;
      u->stack_used = SYSTEM_STACK_USED;
      u->thread_stack_offset = NULL;
      u->stack.cur_ptr = stack_pointer;
      u->frame.cur_ptr = frame_pointer;
      u->stack.ini_ptr = sp0;
      u->frame.ini_ptr = fp0;
      u->stack.swap = NULL;
      u->frame.swap = NULL;
      u->stack.start = NULL;
      u->frame.start = NULL;
      u->stack.bytes = 0;
      u->frame.bytes = 0;
      u->active = A68_TRUE;
/* Create the thread. */
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
      ABEND ((size_t) ss != (size_t) stack_size, "cannot set thread stack size", NULL);
      if (pthread_create (&new_id, &new_at, start_unit, NULL) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      u->parent = parent;
      u->id = new_id;
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
  UP_SWEEP_SEMA;
  LOCK_THREAD;
  t = pthread_self ();
  GET_THREAD_INDEX (k, t);
  context[k].thread_stack_offset = (BYTE_T *) ((int) &stack_offset - stack_direction (&stack_offset) * context[k].stack_used);
  restore_stacks (t);
  p = (NODE_T *) (context[k].unit);
/* This is the thread spawned by the main thread, we spawn parallel units and await their completion. */
  start_parallel_units (SUB (p), t);
  do {
    units_active = A68_FALSE;
    check_parallel_units (&units_active, pthread_self ());
    if (units_active) {
      try_change_thread (p);
    }
  } while (units_active);
  DOWN_SWEEP_SEMA;
  genie_abend_thread ();
  return ((void *) NULL);
}

/*!
\brief execute parallel clause
\param p position in tree
\return propagator for this routine
**/

PROPAGATOR_T genie_parallel (NODE_T * p)
{
  int j;
  ADDR_T stack_s = 0, frame_s = 0;
  BYTE_T *system_stack_offset_s = NULL;
  if (whether_main_thread ()) {
/* Here we are not in the main thread, spawn first thread and await its completion. */
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
/* Set up a thread for this unit. */
    u = &(context[context_index]);
    u->unit = p;
    u->stack_used = SYSTEM_STACK_USED;
    u->thread_stack_offset = NULL;
    u->stack.cur_ptr = stack_pointer;
    u->frame.cur_ptr = frame_pointer;
    u->stack.ini_ptr = sp0;
    u->frame.ini_ptr = fp0;
    u->stack.swap = NULL;
    u->frame.swap = NULL;
    u->stack.start = NULL;
    u->frame.start = NULL;
    u->stack.bytes = 0;
    u->frame.bytes = 0;
    u->active = A68_TRUE;
/* Spawn the first thread and join it to await its completion. */
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
    ABEND ((size_t) ss != (size_t) stack_size, "cannot set thread stack size", NULL);
    if (pthread_create (&parent_thread_id, &new_at, start_genie_parallel, NULL) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (errno != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    u->parent = main_thread_id;
    u->id = parent_thread_id;
    context_index++;
    save_stacks (parent_thread_id);
    UNLOCK_THREAD;
    if (pthread_join (parent_thread_id, NULL) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* The first spawned thread has completed, now clean up. */
    for (j = 0; j < context_index; j++) {
      if (context[j].active && context[j].id != main_thread_id && context[j].id != parent_thread_id) {
/* If threads are being zapped it is possible that some are still active at this point! */
        if (pthread_join (context[j].id, NULL) != 0) {
          diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
      if (context[j].stack.swap != NULL) {
        free (context[j].stack.swap);
        context[j].stack.swap = NULL;
      }
      if (context[j].frame.swap != NULL) {
        free (context[j].frame.swap);
        context[j].frame.swap = NULL;
      }
    }
/* Now every thread should have ended. */
    context_index = 0;
    stack_pointer = stack_s;
    frame_pointer = frame_s;
    get_stack_size ();
    system_stack_offset = system_stack_offset_s;
/* See if we ended execution in parallel clause. */
    if (whether_main_thread () && exit_from_threads) {
      exit_genie (p, par_return_code);
    }
    if (whether_main_thread () && program.error_count > 0) {
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* See if we jumped out of the parallel clause(s). */
    if (whether_main_thread () && abend_all_threads) {
      SYMBOL_TABLE (TAX (jump_label))->jump_to = TAX (jump_label)->unit;
      longjmp (*(jump_buffer), 1);
    }
  } else {
/* Here we are not in the main thread, we spawn parallel units and await their completion. */
    BOOL_T units_active;
    pthread_t t = pthread_self ();
/* Spawn parallel units. */
    start_parallel_units (SUB (p), t);
    do {
      units_active = A68_FALSE;
      check_parallel_units (&units_active, t);
      if (units_active) {
        try_change_thread (p);
      }
    } while (units_active);
  }
  return (PROPAGATOR (p));
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
  *(A68_INT *) ADDRESS (&s) = k;
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
  PUSH_PRIMITIVE (p, VALUE ((A68_INT *) ADDRESS (&s)), A68_INT);
}

/*!
\brief OP UP = (SEMA) VOID
\param p position in tree
**/

void genie_up_sema (NODE_T * p)
{
  A68_REF s;
  if (whether_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), MODE (SEMA));
  VALUE ((A68_INT *) ADDRESS (&s))++;
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
  if (whether_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), MODE (SEMA));
  while (cont) {
    k = (A68_INT *) ADDRESS (&s);
    if (VALUE (k) <= 0) {
      save_stacks (pthread_self ());
      while (VALUE (k) <= 0) {
        if (program.error_count > 0 || abend_all_threads) {
          genie_abend_thread ();
        }
        UNLOCK_THREAD;
/* Waiting a bit relaxes overhead. */
        ASSERT (usleep (10) == 0);
        LOCK_THREAD;
/* Garbage may be collected, so recalculate 'k'. */
        k = (A68_INT *) ADDRESS (&s);
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
