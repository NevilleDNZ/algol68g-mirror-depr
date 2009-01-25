/*!
\file genie.c
\brief driver for the interpreter
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "algol68g.h"
#include "genie.h"
#include "inline.h"
#include "transput.h"
#include "mp.h"

#if ! defined ENABLE_WIN32
#include <sys/resource.h>
#endif

void genie_preprocess (NODE_T *, int *);

A68_HANDLE nil_handle = { INITIALISED_MASK, 0, 0, NULL, NULL, NULL };
A68_REF nil_ref = { INITIALISED_MASK | NIL_MASK, 0, {NULL} };

ADDR_T frame_pointer = 0, stack_pointer = 0, heap_pointer = 0, handle_pointer = 0, global_pointer = 0, frame_start, frame_end, stack_start, stack_end;
BOOL_T do_confirm_exit = A68_TRUE;
BYTE_T *stack_segment = NULL, *heap_segment = NULL, *handle_segment = NULL;
NODE_T *last_unit = NULL;
int global_level = 0, ret_code, ret_line_number, ret_char_number;
int max_lex_lvl = 0;
jmp_buf genie_exit_label;
unsigned check_time_limit_count = 0;

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
  int ret_code, size;
  A68_REF cmd;
  A68_REF ref_z;
  POP_REF (p, &cmd);
  CHECK_INIT (p, INITIALISED (&cmd), MODE (STRING));
  size = 1 + a68_string_size (p, cmd);
  ref_z = heap_generator (p, MODE (C_STRING), 1 + size);
  ret_code = system (a_to_c_string (p, (char *) ADDRESS (&ref_z), cmd));
  PUSH_PRIMITIVE (p, ret_code, A68_INT);
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
        MASK (p) |= mask;
      } else {
        MASK (p) &= ~mask;
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
  if (ret == A68_RUNTIME_ERROR && in_monitor) {
    return;
  } else if (ret == A68_RUNTIME_ERROR && MODULE (INFO (p))->options.debug) {
    diagnostics_to_terminal (MODULE (INFO (p))->top_line, A68_RUNTIME_ERROR);
    single_step (p, BREAKPOINT_ERROR_MASK);
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
      ret_line_number = LINE_NUMBER (p);
      ret_code = ret;
      longjmp (genie_exit_label, 1);
    }
#else
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
    init_rng (seed);
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
      tie_label (SUB (SUB (p)), NEXT (SUB (p)));
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
    p->protect_sweep = NULL;
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
    case PARALLEL_CLAUSE:
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
    case DEPROCEDURING:
    case ROWING:
    case WIDENING:
      {
        MOID_T *m = MOID (p);
        if (m != NULL && (WHETHER (m, REF_SYMBOL) || WHETHER (DEFLEX (m), ROW_SYMBOL))) {
          TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, m, PROTECT_FROM_SWEEP);
          p->protect_sweep = z;
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
  return (t->identifiers == NULL && t->operators == NULL && PRIO (t) == NULL && t->indicants == NULL && t->labels == NULL);
}

/*!
\brief whether a symbol table contains no (anonymous) definition
\param t symbol table
\return TRUE or FALSE
**/

static BOOL_T genie_empty_table (SYMBOL_TABLE_T * t)
{
  return (t->identifiers == NULL && t->operators == NULL && PRIO (t) == NULL && t->indicants == NULL && t->labels == NULL);
}

/*!
\brief perform tasks before interpretation
\param p position in tree
\param max_lev maximum level found
**/

void genie_preprocess (NODE_T * p, int *max_lev)
{
  for (; p != NULL; FORWARD (p)) {
    if (MASK (p) & BREAKPOINT_MASK) {
      if (!(MASK (p) & INTERRUPTIBLE_MASK)) {
        MASK (p) &= ~BREAKPOINT_MASK;
      }
    }
    p->genie.whether_coercion = whether_coercion (p);
    p->genie.whether_new_lexical_level = whether_new_lexical_level (p);
    PROPAGATOR (p).unit = genie_unit;
    PROPAGATOR (p).source = p;
    if (MOID (p) != NULL) {
      MOID (p)->size = moid_size (MOID (p));
      MOID (p)->short_id = mode_attribute (MOID (p));
      if (WHETHER (MOID (p), REF_SYMBOL)) {
        p->need_dns = A68_TRUE;
      } else if (WHETHER (MOID (p), PROC_SYMBOL)) {
        p->need_dns = A68_TRUE;
      } else if (WHETHER (MOID (p), UNION_SYMBOL)) {
        p->need_dns = A68_TRUE;
      } else if (WHETHER (MOID (p), FORMAT_SYMBOL)) {
        p->need_dns = A68_TRUE;
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
        p->genie.level = LEX_LEVEL (NODE (q));
      }
    } else if (WHETHER (p, IDENTIFIER)) {
      TAG_T *q = TAX (p);
      if (q != NULL && NODE (q) != NULL && SYMBOL_TABLE (NODE (q)) != NULL) {
        p->genie.level = LEX_LEVEL (NODE (q));
        p->genie.offset = &stack_segment[FRAME_INFO_SIZE + q->offset];
      }
    } else if (WHETHER (p, OPERATOR)) {
      TAG_T *q = TAX (p);
      if (q != NULL && NODE (q) != NULL && SYMBOL_TABLE (NODE (q)) != NULL) {
        p->genie.level = LEX_LEVEL (NODE (q));
        p->genie.offset = &stack_segment[FRAME_INFO_SIZE + q->offset];
      }
    }
    if (SUB (p) != NULL) {
      PARENT (SUB (p)) = p;
      genie_preprocess (SUB (p), max_lev);
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
    if (p->genie.constant != NULL) {
      free (p->genie.constant);
      p->genie.constant = NULL;
    }
  }
}

/*!
\brief driver for the interpreter
\param module current module
**/

void genie (MODULE_T * module)
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
/*  genie_lex_levels (module->top_node, 1); */
  genie_preprocess (module->top_node, &max_lex_lvl);
  change_masks (module->top_node, BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
  watchpoint_expression = NULL;
  frame_stack_limit = frame_end - storage_overhead;
  expr_stack_limit = stack_end - storage_overhead;
  if (module->options.regression_test) {
    init_rng (1);
  } else {
    genie_init_rng ();
  }
  io_close_tty_line ();
  if (module->options.trace) {
    snprintf (output_line, BUFFER_SIZE, "genie: frame stack %uk, expression stack %uk, heap %uk, handles %uk\n", frame_stack_size / 1024, expr_stack_size / 1024, heap_size / 1024, handle_pool_size / 1024);
    WRITE (STDOUT_FILENO, output_line);
  }
  install_signal_handlers ();
  do_confirm_exit = A68_TRUE;
/* Dive into the program. */
  if (setjmp (genie_exit_label) == 0) {
    NODE_T *p = SUB (module->top_node);
/* If we are to stop in the monitor, set a breakpoint on the first unit. */
    if (module->options.debug) {
      change_masks (module->top_node, BREAKPOINT_TEMPORARY_MASK, A68_TRUE);
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
    genie_init_heap (p, module);
    genie_init_transput (module->top_node);
    cputime_0 = seconds ();
/* Here we go ... */
    (void) genie_enclosed (module->top_node);
  } else {
/* Here we have jumped out of the interpreter. What happened? */
    if (module->options.debug) {
      WRITE (STDOUT_FILENO, "Execution discontinued");
    }
    if (ret_code == A68_RERUN) {
      diagnostics_to_terminal (module->top_line, A68_RUNTIME_ERROR);
      genie (module);
    } else if (ret_code == A68_RUNTIME_ERROR) {
      if (module->options.backtrace) {
        int printed = 0;
        snprintf (output_line, BUFFER_SIZE, "\nStack backtrace");
        WRITE (STDOUT_FILENO, output_line);
        stack_dump (STDOUT_FILENO, frame_pointer, 16, &printed);
        WRITE (STDOUT_FILENO, "\n");
      }
      if (module->files.listing.opened) {
        int printed = 0;
        snprintf (output_line, BUFFER_SIZE, "\nStack backtrace");
        WRITE (module->files.listing.fd, output_line);
        stack_dump (module->files.listing.fd, frame_pointer, 32, &printed);
      }
    }
  }
}
