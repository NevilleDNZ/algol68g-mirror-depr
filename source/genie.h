/*!
\file genie.h
\brief contains genie related definitions
**/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2006 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef A68G_GENIE_H
#define A68G_GENIE_H

/* Macros. */

#define BITS_WIDTH ((int) (1 + ceil (log (MAX_INT) / log(2))))
#define INT_WIDTH ((int) (1 + floor (log (MAX_INT) / log (10))))

#define CHAR_WIDTH (1 + (int) log10 ((double) SCHAR_MAX))
#define REAL_WIDTH (DBL_DIG)
#define EXP_WIDTH ((int) (1 + log10 ((double) DBL_MAX_10_EXP)))

#define HEAP_ADDRESS(n) ((BYTE_T *) & (heap_segment[n]))
#define IS_NIL(p) ((p).offset == 0 && (p).handle == NULL)

#define LHS_MODE(p) (MOID (PACK (MOID (p))))
#define RHS_MODE(p) (MOID (NEXT (PACK (MOID (p)))))

/* ADDRESS calculates the effective address of fat pointer z. */

#define ADDRESS(z)\
 ((BYTE_T *) (\
   &((z)->segment[(z)->offset + ((z)->segment == heap_segment ? (z)->handle->offset : 0)])\
  ))

/* Check on a NIL name. */

#define TEST_NIL(p, z, m)\
  if (IS_NIL (z)) {\
    genie_check_initialisation (p, (BYTE_T *) &z, m);\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_ACCESSING_NIL, (m));\
    exit_genie ((p), A_RUNTIME_ERROR);\
  }

/* Initialisation check. */

#define TEST_INIT(p, z, m) {\
  if (! (z).status & INITIALISED_MASK) {\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE, (m));\
    exit_genie ((p), A_RUNTIME_ERROR);\
  }}

/* Activation records in the frame stack. */

typedef struct ACTIVATION_RECORD ACTIVATION_RECORD;

struct ACTIVATION_RECORD
{
  ADDR_T static_link, dynamic_link, dynamic_scope;
  NODE_T *node;
  jmp_buf *jump_stat;
  BOOL_T proc_frame;
};

#define FRAME_DYNAMIC_LINK(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->dynamic_link)
#define FRAME_DYNAMIC_SCOPE(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->dynamic_scope)
#define FRAME_PROC_FRAME(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->proc_frame)
#define FRAME_INCREMENT(n) (SYMBOL_TABLE (FRAME_TREE(n))->ap_increment)
#define FRAME_JUMP_STAT(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->jump_stat)
#define FRAME_LEXICAL_LEVEL(n) (SYMBOL_TABLE (FRAME_TREE(n))->level)
#define FRAME_STATIC_LINK(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->static_link)
#define FRAME_TREE(n) (NODE ((ACTIVATION_RECORD *) FRAME_ADDRESS(n)))
#define FRAME_INFO_SIZE (ALIGN (SIZE_OF (ACTIVATION_RECORD)))
#define FRAME_ADDRESS(n) ((BYTE_T *) &frame_segment[n])
#define FRAME_LOCAL(n, m) (FRAME_ADDRESS ((n) + FRAME_INFO_SIZE + (m)))
#define FRAME_OFFSET(n) (FRAME_ADDRESS (frame_pointer + (n)))
#define FRAME_TOP (FRAME_OFFSET (0))
#define FRAME_CLEAR(m) FILL ((BYTE_T *) FRAME_OFFSET (FRAME_INFO_SIZE), 0, (m))
#define FRAME_SIZE(fp) (FRAME_INFO_SIZE + FRAME_INCREMENT (fp))

/* 
STATIC_LINK_FOR_FRAME: determine static link for stack frame.
new_lex_lvl: lexical level of new stack frame.
returns: static link for stack frame at 'new_lex_lvl'. 
*/

#define STATIC_LINK_FOR_FRAME(dest, new_lex_lvl) {\
  int m_cur_lex_lvl = FRAME_LEXICAL_LEVEL (frame_pointer);\
  if (m_cur_lex_lvl == (new_lex_lvl)) {\
    (dest) = FRAME_STATIC_LINK (frame_pointer);\
  } else if (m_cur_lex_lvl > (new_lex_lvl)) {\
    ADDR_T m_static_link = frame_pointer;\
    while (FRAME_LEXICAL_LEVEL (m_static_link) >= (new_lex_lvl)) {\
      m_static_link = FRAME_STATIC_LINK (m_static_link);\
    }\
    (dest) = m_static_link;\
  } else {\
    (dest) = frame_pointer;\
  }}

/* DESCENT: descent static link to appropriate lexical level. */

#define DESCENT(dest, l) {\
  if ((l) == global_level) {\
    (dest) = global_pointer;\
  } else {\
    ADDR_T m_sl = frame_pointer;\
    while (l != FRAME_LEXICAL_LEVEL (m_sl)) {\
      m_sl = FRAME_STATIC_LINK (m_sl);\
    }\
    (dest) = m_sl;\
  }}

#define FRAME_GET(dest, cast, p) {\
  ADDR_T m_z;\
  DESCENT (m_z, (p)->genie.level);\
  (dest) = (cast *) & ((p)->genie.offset[m_z]);\
  }

/* Macros for row-handling. */

#define GET_DESCRIPTOR(a, t, p)\
  /* ABNORMAL_END (IS_NIL (*p), ERROR_NIL_DESCRIPTOR, NULL); */\
  a = (A68_ARRAY *) ADDRESS (p);\
  t = (A68_TUPLE *) & (((BYTE_T *) (a)) [SIZE_OF (A68_ARRAY)]);

#define PUT_DESCRIPTOR(a, t, p)\
  ABNORMAL_END (IS_NIL (*p), ERROR_NIL_DESCRIPTOR, NULL);\
  *(A68_ARRAY *) ADDRESS (p) = (a);\
  *(A68_TUPLE *) &(((BYTE_T *) (ADDRESS(p))) [SIZE_OF (A68_ARRAY)]) = (t);

#define ROW_SIZE(t) (((t)->upper_bound >= (t)->lower_bound) ? ((t)->upper_bound - (t)->lower_bound + 1) : 0)

#define ROW_ELEMENT(a, k) (((ADDR_T) k + (a)->slice_offset) * (a)->elem_size + (a)->field_offset)

#define INDEX_1_DIM(a, t, k) ROW_ELEMENT (a, ((t)->span * ((int) k - (t)->shift)))

/* Macros for execution. */

extern unsigned check_time_limit_count;

#define CHECK_TIME_LIMIT(p) {\
  double m_t = p->info->module->options.time_limit;\
  BOOL_T m_trace_mood = (MASK (p) & TRACE_MASK) != 0;\
  if (m_t > 0 && (((check_time_limit_count++) % 100) == 0)) {\
    if ((seconds () - cputime_0) > m_t) {\
      diagnostic_node (A_RUNTIME_ERROR, (NODE_T *) p, ERROR_TIME_LIMIT_EXCEEDED);\
      exit_genie ((NODE_T *) p, A_RUNTIME_ERROR);\
    }\
  } else if (sys_request_flag) {\
    single_step ((NODE_T *) p, A_TRUE, A_FALSE);\
  } else if (m_trace_mood) {\
    where (STDOUT_FILENO, (NODE_T *) p);\
  }}

#define EXECUTE_UNIT(p) (last_unit = p, (p)->genie.propagator.unit ((p)->genie.propagator.source))

#define EXECUTE_UNIT_TRACE(p) genie_unit_trace (p);

/* Stack manipulation. */

#define STACK_ADDRESS(n) ((BYTE_T *) &(stack_segment[(n)]))
#define STACK_OFFSET(n) (STACK_ADDRESS (stack_pointer + (n)))
#define STACK_TOP (STACK_ADDRESS (stack_pointer))

#define INCREMENT_STACK_POINTER(err, i) {stack_pointer += ALIGN (i); (void) (err);}

#define DECREMENT_STACK_POINTER(err, i) {\
  stack_pointer -= ALIGN (i);\
  (void) (err);\
}

#define PUSH(p, addr, size) {\
  BYTE_T *sp = STACK_TOP;\
  INCREMENT_STACK_POINTER ((p), (size));\
  COPY (sp, (BYTE_T *) (addr), (unsigned) (size));\
}

#define POP(p, addr, size) {\
  DECREMENT_STACK_POINTER((p), (size));\
  COPY ((BYTE_T *) (addr), STACK_TOP, (unsigned) (size));\
}

#define POP_ADDRESS(p, addr, type) {\
  DECREMENT_STACK_POINTER((p), SIZE_OF (type));\
  (addr) = (type *) STACK_TOP;\
}

#define PUSH_INT(p, k) {\
  A68_INT *_z_ = (A68_INT *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  _z_->value = (k);\
  INCREMENT_STACK_POINTER((p), SIZE_OF (A68_INT));\
}

#define POP_INT(p, k) {\
  DECREMENT_STACK_POINTER((p), SIZE_OF (A68_INT));\
  (*(k)) = *((A68_INT *) STACK_TOP);\
}

#define PUSH_REAL(p, x) {\
  A68_REAL *_z_ = (A68_REAL *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  _z_->value = (x);\
  INCREMENT_STACK_POINTER((p), MOID_SIZE (MODE (REAL)));\
}

#define POP_REAL(p, x) {\
  DECREMENT_STACK_POINTER((p), MOID_SIZE (MODE (REAL)));\
  (*(x)) = *((A68_REAL *) STACK_TOP);\
}

#define PUSH_BOOL(p, k) {\
  A68_BOOL *_z_ = (A68_BOOL *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  _z_->value = (k);\
  INCREMENT_STACK_POINTER((p), SIZE_OF (A68_BOOL));\
}

#define POP_BOOL(p, k) {\
  DECREMENT_STACK_POINTER((p), SIZE_OF (A68_BOOL));\
  (*(k)) = *((A68_BOOL *) STACK_TOP);\
}

#define PUSH_CHAR(p, k) {\
  A68_CHAR *_z_ = (A68_CHAR *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  _z_->value = (k);\
  INCREMENT_STACK_POINTER((p), SIZE_OF (A68_CHAR));\
}

#define POP_CHAR(p, k) {\
  DECREMENT_STACK_POINTER((p), SIZE_OF (A68_CHAR));\
  (*(k)) = *((A68_CHAR *) STACK_TOP);\
}

#define PUSH_BITS(p, k) {\
  A68_BITS *_z_ = (A68_BITS *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  _z_->value = (k);\
  INCREMENT_STACK_POINTER((p), SIZE_OF (A68_BITS));\
}

#define POP_BITS(p, k) {\
  DECREMENT_STACK_POINTER((p), SIZE_OF (A68_BITS));\
  (*(k)) = *((A68_BITS *) STACK_TOP);\
}

#define PUSH_BYTES(p, k) {\
  A68_BYTES *_z_ = (A68_BYTES *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  strncpy (_z_->value, k, BYTES_WIDTH);\
  INCREMENT_STACK_POINTER((p), SIZE_OF (A68_BYTES));\
}

#define POP_BYTES(p, k) {\
  DECREMENT_STACK_POINTER((p), SIZE_OF (A68_BYTES));\
  (*(k)) = *((A68_BYTES *) STACK_TOP);\
}

#define PUSH_LONG_BYTES(p, k) {\
  A68_LONG_BYTES *_z_ = (A68_LONG_BYTES *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  strncpy (_z_->value, k, LONG_BYTES_WIDTH);\
  INCREMENT_STACK_POINTER((p), SIZE_OF (A68_LONG_BYTES));\
}

#define POP_LONG_BYTES(p, k) {\
  DECREMENT_STACK_POINTER((p), SIZE_OF (A68_LONG_BYTES));\
  (*(k)) = *((A68_LONG_BYTES *) STACK_TOP);\
}

#define PUSH_COMPLEX(p, re, im) {\
  PUSH_REAL (p, re);\
  PUSH_REAL (p, im);\
}

#define POP_COMPLEX(p, re, im) {\
  POP_REAL (p, im);\
  POP_REAL (p, re);\
}

#define PUSH_REF(p, z) {\
  * (A68_REF *) STACK_TOP = (z);\
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_REF));\
}

#define PUSH_REF_FILE(p, z) PUSH_REF (p, z)

#define PUSH_CHANNEL(p, z) {\
  * (A68_CHANNEL *) STACK_TOP = (z);\
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_CHANNEL));\
}

#define PUSH_POINTER(p, z) {\
  A68_POINTER *_w_ = (A68_POINTER *) STACK_TOP;\
  _w_->status = INITIALISED_MASK;\
  _w_->value = z;\
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));\
}

#define POP_REF(p, z) {\
  DECREMENT_STACK_POINTER((p), SIZE_OF (A68_REF));\
  (*(z)) = *((A68_REF *) STACK_TOP);\
}

#define POP_OPERAND_ADDRESS(p, i, type) {\
  (void) (p);\
  (i) = (type *) (STACK_OFFSET (-SIZE_OF (type)));\
}

#define POP_OPERAND_ADDRESSES(p, i, j, type) {\
  DECREMENT_STACK_POINTER ((p), SIZE_OF (type));\
  (j) = (type *) STACK_TOP;\
  (i) = (type *) (STACK_OFFSET (-SIZE_OF (type)));\
}

/* Macro's for the garbage collector. */

/* Store intermediate REF to save it from the GC. */

#define PROTECT_FROM_SWEEP(p)\
  if ((p)->protect_sweep != NULL) {\
    *(A68_REF *) FRAME_LOCAL (frame_pointer, (p)->protect_sweep->offset) =\
    *(A68_REF *) (STACK_OFFSET (- SIZE_OF (A68_REF)));\
  }

#define PREEMPTIVE_SWEEP {\
  double f = (double) heap_pointer / (double) heap_size;\
  double h = (double) free_handle_count / (double) max_handle_count;\
  if (f > 0.5 || h < 0.1) {\
    sweep_heap ((NODE_T *) p, frame_pointer);\
  }}

extern int block_heap_compacter;

#define UP_SWEEP_SEMA {block_heap_compacter++;}
#define DOWN_SWEEP_SEMA {block_heap_compacter--;}

#define PROTECT_SWEEP_HANDLE(z) {((z)->handle)->status |= NO_SWEEP_MASK;}
#define UNPROTECT_SWEEP_HANDLE(z) {((z)->handle)->status &= ~NO_SWEEP_MASK;}

/* Macros for checking representation of values. */

#define TEST_INT_ADDITION(p, i, j)\
  if (((i ^ j) & MIN_INT) == 0 && ABS (i) > INT_MAX - ABS (j)) {\
    errno = ERANGE;\
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_MATH, MODE (INT), NULL);\
    exit_genie (p, A_RUNTIME_ERROR);\
    }

#ifdef HAVE_IEEE_754
#define TEST_REAL_REPRESENTATION(p, u)\
  if (isnan (u) || isinf (u)) {\
    errno = ERANGE;\
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_MATH, MODE (REAL), NULL);\
    exit_genie (p, A_RUNTIME_ERROR);\
  }
#else
#define TEST_REAL_REPRESENTATION(p, u) {;}
#endif

#ifdef HAVE_IEEE_754
#define TEST_COMPLEX_REPRESENTATION(p, u, v)\
  if (isnan (u) || isinf (u) || isnan (v) || isinf (v)) {\
    errno = ERANGE;\
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_MATH, MODE (COMPLEX), NULL);\
    exit_genie (p, A_RUNTIME_ERROR);\
  }
#else
#define TEST_COMPLEX_REPRESENTATION(p, u, v) {;}
#endif

/* Now we assume that double can exactly represent MAX_INT: */

#define TEST_TIMES_OVERFLOW_INT(p, u, v)\
  if ((double) ABS (u) * (double) ABS (v) > (double) MAX_INT) {\
    errno = ERANGE;\
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_MATH, MODE (INT), NULL);\
    exit_genie (p, A_RUNTIME_ERROR);\
    }\

#ifdef HAVE_IEEE_754
#define TEST_TIMES_OVERFLOW_REAL(p, u, v) {;}
#else
#define TEST_TIMES_OVERFLOW_REAL(p, u, v)\
  if (v != 0.0) {\
    if ((u >= 0 ? u : -u) > DBL_MAX / (v >= 0 ? v : -v)) {\
      errno = ERANGE;\
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_MATH, MODE (REAL), NULL);\
      exit_genie (p, A_RUNTIME_ERROR);\
      }\
  }
#endif

/*
Macro's for stack checking. Since the stacks grow by small amounts at a time
(A68 rows are in the heap), we check the stacks only at certain points: where
A68 recursion may set in, or in the garbage collector. We check whether there
still is sufficient overhead to make it to the next check.
*/

#define TOO_COMPLEX "program too complex"

#if defined HAVE_SYSTEM_STACK_CHECK && defined HAVE_POSIX_THREADS
#define SYSTEM_STACK_USED (ABS ((int) system_stack_offset - (int) &stack_offset))
#define LOW_STACK_ALERT(p) {\
  BYTE_T stack_offset;\
  if (stack_size > 0 && SYSTEM_STACK_USED > stack_limit) {\
    errno = 0;\
    if ((p) == NULL) {\
      ABNORMAL_END (A_TRUE, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
    } else {\
      diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
      exit_genie ((p), A_RUNTIME_ERROR);\
    }\
  }\
  if ((p) != NULL && stack_pointer > expr_stack_limit) {\
    errno = 0;\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
    exit_genie ((p), A_RUNTIME_ERROR);\
  }\
  if ((p) != NULL && frame_pointer > frame_stack_limit) { \
    errno = 0;\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
    exit_genie ((p), A_RUNTIME_ERROR);\
  }}
#elif defined HAVE_SYSTEM_STACK_CHECK
#define LOW_STACK_ALERT(p) {\
  BYTE_T stack_offset;\
  if (stack_size > 0 && ABS ((int) system_stack_offset - (int) &stack_offset) > stack_limit) {\
    errno = 0;\
    if ((p) == NULL) {\
      ABNORMAL_END (A_TRUE, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
    } else {\
      diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
      exit_genie ((p), A_RUNTIME_ERROR);\
    }\
  }\
  if ((p) != NULL && stack_pointer > expr_stack_limit) {\
    errno = 0;\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
    exit_genie ((p), A_RUNTIME_ERROR);\
  }\
  if ((p) != NULL && frame_pointer > frame_stack_limit) { \
    errno = 0;\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
    exit_genie ((p), A_RUNTIME_ERROR);\
  }}
#else
#define LOW_STACK_ALERT(p) {\
  (void) (p);\
  errno = 0;\
  ABNORMAL_END (stack_pointer > expr_stack_limit, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
  ABNORMAL_END (frame_pointer > frame_stack_limit, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
  }
#endif

/* Opening of stack frames is in-line. */

#define OPEN_STATIC_FRAME(p) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act;\
  LOW_STACK_ALERT (p);\
  STATIC_LINK_FOR_FRAME (static_link, LEX_LEVEL (p));\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A_FALSE;\
  FRAME_CLEAR (SYMBOL_TABLE (p)->ap_increment);\
  if (SYMBOL_TABLE (p)->initialise_frame) {\
    if (global_pointer == 0 && LEX_LEVEL (p) == global_level) {\
      global_pointer = frame_pointer;\
    }\
    initialise_frame (p);\
  }}

#define OPEN_PROC_FRAME(p, environ) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act;\
  LOW_STACK_ALERT (p);\
  static_link = (environ > 0 ? environ : frame_pointer);\
  if (frame_pointer < static_link) {\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_SCOPE_DYNAMIC_0);\
    exit_genie (p, A_RUNTIME_ERROR);\
  }\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A_TRUE;\
  FRAME_CLEAR (SYMBOL_TABLE (p)->ap_increment);\
  if (SYMBOL_TABLE (p)->initialise_frame) {\
    if (global_pointer == 0 && LEX_LEVEL (p) == global_level) {\
      global_pointer = frame_pointer;\
    }\
    initialise_frame (p);\
  }}

#define CLOSE_FRAME {\
  frame_pointer = FRAME_DYNAMIC_LINK (frame_pointer);\
  }

/* Macros for check on initialisation of values. */

#define CHECK_INIT(p, c, q)\
  if (!(c)) {\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE_FROM, (q));\
    exit_genie ((p), A_RUNTIME_ERROR);\
  }

#define GENIE_CHECK_INITIALISATION(p, w, q) {\
  switch ((q)->short_id) {\
  case MODE_INT: {\
      A68_INT *_z_ = (A68_INT *) (w);\
      CHECK_INIT ((p), _z_->status & INITIALISED_MASK, (q));\
      break;\
    }\
  case MODE_REAL: {\
      A68_REAL *_z_ = (A68_REAL *) (w);\
      CHECK_INIT ((p), _z_->status & INITIALISED_MASK, (q));\
      break;\
    }\
  case MODE_BOOL: {\
      A68_BOOL *_z_ = (A68_BOOL *) (w);\
      CHECK_INIT ((p), _z_->status & INITIALISED_MASK, (q));\
      break;\
    }\
  case MODE_CHAR: {\
      A68_CHAR *_z_ = (A68_CHAR *) (w);\
      CHECK_INIT ((p), _z_->status & INITIALISED_MASK, (q));\
      break;\
    }\
  case MODE_BITS: {\
      A68_BITS *_z_ = (A68_BITS *) (w);\
      CHECK_INIT ((p), _z_->status & INITIALISED_MASK, (q));\
      break;\
    }\
  case MODE_COMPLEX: {\
      A68_REAL *_r_ = (A68_REAL *) (w);\
      A68_REAL *_i_ = (A68_REAL *) ((w) + SIZE_OF (A68_REAL));\
      CHECK_INIT ((p), _r_->status & INITIALISED_MASK, (q));\
      CHECK_INIT ((p), _i_->status & INITIALISED_MASK, (q));\
      break;\
    }\
  case REF_SYMBOL: {\
      A68_REF *_z_ = (A68_REF *) (w);\
      CHECK_INIT ((p), _z_->status & INITIALISED_MASK, (q));\
      break;\
    }\
  case PROC_SYMBOL: {\
      A68_PROCEDURE *_z_ = (A68_PROCEDURE *) (w);\
      CHECK_INIT ((p), _z_->status & INITIALISED_MASK, (q));\
      break;\
    }\
  default: {\
      genie_check_initialisation ((p), (w), (q));\
      break;\
    }\
  }\
}

/* Macro's for standard environ. */

#define A68_ENV_INT(n, k) void n (NODE_T *p) {PUSH_INT (p, (k));}
#define A68_ENV_REAL(n, z) void n (NODE_T *p) {PUSH_REAL (p, (z));}

/* External symbols. */

extern ADDR_T frame_pointer, stack_pointer, heap_pointer, handle_pointer, global_pointer;
extern A68_FORMAT nil_format;
extern A68_HANDLE nil_handle, *free_handles, *busy_handles;
extern A68_POINTER nil_pointer;
extern A68_REF nil_ref;
extern A68_REF stand_in, stand_out;
extern BOOL_T in_monitor;
extern BYTE_T *frame_segment, *stack_segment, *heap_segment, *handle_segment;
extern MOID_T *top_expr_moid;
extern double cputime_0;
extern int global_level, max_lex_lvl, ret_code;
extern jmp_buf genie_exit_label, monitor_exit_label;

extern int frame_stack_size, expr_stack_size, heap_size, handle_pool_size;
extern int stack_limit, frame_stack_limit, expr_stack_limit;
extern int storage_overhead;

/* External symbols. */

#ifdef HAVE_POSIX_THREADS
extern int parallel_clauses;
extern pthread_t main_thread_id;
extern BOOL_T in_par_clause;
extern BOOL_T is_main_thread (void);
extern void count_parallel_clauses (NODE_T *);
extern void genie_abend_thread (void);
extern void zap_all_threads (NODE_T *, jmp_buf *, NODE_T *);
#endif

extern int free_handle_count, max_handle_count;
extern void sweep_heap (NODE_T *, ADDR_T);

extern GENIE_PROCEDURE genie_abs_bool;
extern GENIE_PROCEDURE genie_abs_char;
extern GENIE_PROCEDURE genie_abs_complex;
extern GENIE_PROCEDURE genie_abs_int;
extern GENIE_PROCEDURE genie_abs_long_complex;
extern GENIE_PROCEDURE genie_abs_long_mp;
extern GENIE_PROCEDURE genie_abs_real;
extern GENIE_PROCEDURE genie_acos_long_complex;
extern GENIE_PROCEDURE genie_acos_long_mp;
extern GENIE_PROCEDURE genie_acronym;
extern GENIE_PROCEDURE genie_add_bytes;
extern GENIE_PROCEDURE genie_add_char;
extern GENIE_PROCEDURE genie_add_complex;
extern GENIE_PROCEDURE genie_add_int;
extern GENIE_PROCEDURE genie_add_long_bytes;
extern GENIE_PROCEDURE genie_add_long_complex;
extern GENIE_PROCEDURE genie_add_long_int;
extern GENIE_PROCEDURE genie_add_long_mp;
extern GENIE_PROCEDURE genie_add_real;
extern GENIE_PROCEDURE genie_add_string;
extern GENIE_PROCEDURE genie_and_bits;
extern GENIE_PROCEDURE genie_and_bool;
extern GENIE_PROCEDURE genie_and_long_mp;
extern GENIE_PROCEDURE genie_arccos_real;
extern GENIE_PROCEDURE genie_arccosh_long_mp;
extern GENIE_PROCEDURE genie_arcsin_real;
extern GENIE_PROCEDURE genie_arcsinh_long_mp;
extern GENIE_PROCEDURE genie_arctan_real;
extern GENIE_PROCEDURE genie_arctanh_long_mp;
extern GENIE_PROCEDURE genie_arg_complex;
extern GENIE_PROCEDURE genie_arg_long_complex;
extern GENIE_PROCEDURE genie_asin_long_complex;
extern GENIE_PROCEDURE genie_asin_long_mp;
extern GENIE_PROCEDURE genie_atan2_real;
extern GENIE_PROCEDURE genie_atan_long_complex;
extern GENIE_PROCEDURE genie_atan_long_mp;
extern GENIE_PROCEDURE genie_bin_int;
extern GENIE_PROCEDURE genie_bin_long_mp;
extern GENIE_PROCEDURE genie_bits_lengths;
extern GENIE_PROCEDURE genie_bits_pack;
extern GENIE_PROCEDURE genie_bits_shorths;
extern GENIE_PROCEDURE genie_bits_width;
extern GENIE_PROCEDURE genie_break;
extern GENIE_PROCEDURE genie_debug;
extern GENIE_PROCEDURE genie_bytes_lengths;
extern GENIE_PROCEDURE genie_bytes_shorths;
extern GENIE_PROCEDURE genie_bytes_width;
extern GENIE_PROCEDURE genie_bytespack;
extern GENIE_PROCEDURE genie_char_in_string;
extern GENIE_PROCEDURE genie_complex_lengths;
extern GENIE_PROCEDURE genie_complex_shorths;
extern GENIE_PROCEDURE genie_conj_complex;
extern GENIE_PROCEDURE genie_conj_long_complex;
extern GENIE_PROCEDURE genie_cos_long_complex;
extern GENIE_PROCEDURE genie_cos_long_mp;
extern GENIE_PROCEDURE genie_cos_real;
extern GENIE_PROCEDURE genie_cosh_long_mp;
extern GENIE_PROCEDURE genie_cputime;
extern GENIE_PROCEDURE genie_curt_long_mp;
extern GENIE_PROCEDURE genie_curt_real;
extern GENIE_PROCEDURE genie_div_complex;
extern GENIE_PROCEDURE genie_div_int;
extern GENIE_PROCEDURE genie_div_long_complex;
extern GENIE_PROCEDURE genie_div_long_mp;
extern GENIE_PROCEDURE genie_div_real;
extern GENIE_PROCEDURE genie_divab_complex;
extern GENIE_PROCEDURE genie_divab_long_complex;
extern GENIE_PROCEDURE genie_divab_long_mp;
extern GENIE_PROCEDURE genie_down_sema;
extern GENIE_PROCEDURE genie_dyad_elems;
extern GENIE_PROCEDURE genie_dyad_lwb;
extern GENIE_PROCEDURE genie_dyad_upb;
extern GENIE_PROCEDURE genie_elem_bits;
extern GENIE_PROCEDURE genie_elem_bytes;
extern GENIE_PROCEDURE genie_elem_long_bits;
extern GENIE_PROCEDURE genie_elem_long_bytes;
extern GENIE_PROCEDURE genie_elem_longlong_bits;
extern GENIE_PROCEDURE genie_elem_string;
extern GENIE_PROCEDURE genie_entier_long_mp;
extern GENIE_PROCEDURE genie_entier_real;
extern GENIE_PROCEDURE genie_eq_bits;
extern GENIE_PROCEDURE genie_eq_bool;
extern GENIE_PROCEDURE genie_eq_bytes;
extern GENIE_PROCEDURE genie_eq_char;
extern GENIE_PROCEDURE genie_eq_complex;
extern GENIE_PROCEDURE genie_eq_int;
extern GENIE_PROCEDURE genie_eq_long_bytes;
extern GENIE_PROCEDURE genie_eq_long_complex;
extern GENIE_PROCEDURE genie_eq_long_mp;
extern GENIE_PROCEDURE genie_eq_real;
extern GENIE_PROCEDURE genie_eq_string;
extern GENIE_PROCEDURE genie_evaluate;
extern GENIE_PROCEDURE genie_exp_long_complex;
extern GENIE_PROCEDURE genie_exp_long_mp;
extern GENIE_PROCEDURE genie_exp_real;
extern GENIE_PROCEDURE genie_exp_width;
extern GENIE_PROCEDURE genie_first_random;
extern GENIE_PROCEDURE genie_garbage_collections;
extern GENIE_PROCEDURE genie_garbage_freed;
extern GENIE_PROCEDURE genie_garbage_seconds;
extern GENIE_PROCEDURE genie_ge_bits;
extern GENIE_PROCEDURE genie_ge_bytes;
extern GENIE_PROCEDURE genie_ge_char;
extern GENIE_PROCEDURE genie_ge_int;
extern GENIE_PROCEDURE genie_ge_long_bytes;
extern GENIE_PROCEDURE genie_ge_long_mp;
extern GENIE_PROCEDURE genie_ge_real;
extern GENIE_PROCEDURE genie_ge_string;
extern GENIE_PROCEDURE genie_gt_bits;
extern GENIE_PROCEDURE genie_gt_bytes;
extern GENIE_PROCEDURE genie_gt_char;
extern GENIE_PROCEDURE genie_gt_int;
extern GENIE_PROCEDURE genie_gt_long_bytes;
extern GENIE_PROCEDURE genie_gt_long_mp;
extern GENIE_PROCEDURE genie_gt_real;
extern GENIE_PROCEDURE genie_gt_string;
extern GENIE_PROCEDURE genie_icomplex;
extern GENIE_PROCEDURE genie_idf;
extern GENIE_PROCEDURE genie_idle;
extern GENIE_PROCEDURE genie_iint_complex;
extern GENIE_PROCEDURE genie_im_complex;
extern GENIE_PROCEDURE genie_im_long_complex;
extern GENIE_PROCEDURE genie_int_lengths;
extern GENIE_PROCEDURE genie_int_shorths;
extern GENIE_PROCEDURE genie_int_width;
extern GENIE_PROCEDURE genie_last_char_in_string;
extern GENIE_PROCEDURE genie_le_bits;
extern GENIE_PROCEDURE genie_le_bytes;
extern GENIE_PROCEDURE genie_le_char;
extern GENIE_PROCEDURE genie_le_int;
extern GENIE_PROCEDURE genie_le_long_bytes;
extern GENIE_PROCEDURE genie_le_long_mp;
extern GENIE_PROCEDURE genie_le_real;
extern GENIE_PROCEDURE genie_le_string;
extern GENIE_PROCEDURE genie_leng_bytes;
extern GENIE_PROCEDURE genie_lengthen_complex_to_long_complex;
extern GENIE_PROCEDURE genie_lengthen_int_to_long_mp;
extern GENIE_PROCEDURE genie_lengthen_long_complex_to_longlong_complex;
extern GENIE_PROCEDURE genie_lengthen_long_mp_to_longlong_mp;
extern GENIE_PROCEDURE genie_lengthen_real_to_long_mp;
extern GENIE_PROCEDURE genie_lengthen_unsigned_to_long_mp;
extern GENIE_PROCEDURE genie_level_int_sema;
extern GENIE_PROCEDURE genie_level_sema_int;
extern GENIE_PROCEDURE genie_ln_long_complex;
extern GENIE_PROCEDURE genie_ln_long_mp;
extern GENIE_PROCEDURE genie_ln_real;
extern GENIE_PROCEDURE genie_log_long_mp;
extern GENIE_PROCEDURE genie_log_real;
extern GENIE_PROCEDURE genie_long_bits_pack;
extern GENIE_PROCEDURE genie_long_bits_width;
extern GENIE_PROCEDURE genie_long_bytes_width;
extern GENIE_PROCEDURE genie_long_bytespack;
extern GENIE_PROCEDURE genie_long_exp_width;
extern GENIE_PROCEDURE genie_long_int_width;
extern GENIE_PROCEDURE genie_long_max_bits;
extern GENIE_PROCEDURE genie_long_max_int;
extern GENIE_PROCEDURE genie_long_max_real;
extern GENIE_PROCEDURE genie_long_next_random;
extern GENIE_PROCEDURE genie_long_real_width;
extern GENIE_PROCEDURE genie_long_small_real;
extern GENIE_PROCEDURE genie_longlong_bits_width;
extern GENIE_PROCEDURE genie_longlong_exp_width;
extern GENIE_PROCEDURE genie_longlong_int_width;
extern GENIE_PROCEDURE genie_longlong_max_bits;
extern GENIE_PROCEDURE genie_longlong_max_int;
extern GENIE_PROCEDURE genie_longlong_max_real;
extern GENIE_PROCEDURE genie_longlong_real_width;
extern GENIE_PROCEDURE genie_longlong_small_real;
extern GENIE_PROCEDURE genie_lt_bits;
extern GENIE_PROCEDURE genie_lt_bytes;
extern GENIE_PROCEDURE genie_lt_char;
extern GENIE_PROCEDURE genie_lt_int;
extern GENIE_PROCEDURE genie_lt_long_bytes;
extern GENIE_PROCEDURE genie_lt_long_mp;
extern GENIE_PROCEDURE genie_lt_real;
extern GENIE_PROCEDURE genie_lt_string;
extern GENIE_PROCEDURE genie_make_term;
extern GENIE_PROCEDURE genie_max_abs_char;
extern GENIE_PROCEDURE genie_max_bits;
extern GENIE_PROCEDURE genie_max_int;
extern GENIE_PROCEDURE genie_max_real;
extern GENIE_PROCEDURE genie_minus_complex;
extern GENIE_PROCEDURE genie_minus_int;
extern GENIE_PROCEDURE genie_minus_long_complex;
extern GENIE_PROCEDURE genie_minus_long_int;
extern GENIE_PROCEDURE genie_minus_long_mp;
extern GENIE_PROCEDURE genie_minus_real;
extern GENIE_PROCEDURE genie_minusab_complex;
extern GENIE_PROCEDURE genie_minusab_int;
extern GENIE_PROCEDURE genie_minusab_long_complex;
extern GENIE_PROCEDURE genie_minusab_long_int;
extern GENIE_PROCEDURE genie_minusab_long_mp;
extern GENIE_PROCEDURE genie_minusab_real;
extern GENIE_PROCEDURE genie_mod_int;
extern GENIE_PROCEDURE genie_mod_long_mp;
extern GENIE_PROCEDURE genie_modab_int;
extern GENIE_PROCEDURE genie_modab_long_mp;
extern GENIE_PROCEDURE genie_monad_elems;
extern GENIE_PROCEDURE genie_monad_lwb;
extern GENIE_PROCEDURE genie_monad_upb;
extern GENIE_PROCEDURE genie_mul_complex;
extern GENIE_PROCEDURE genie_mul_int;
extern GENIE_PROCEDURE genie_mul_long_complex;
extern GENIE_PROCEDURE genie_mul_long_int;
extern GENIE_PROCEDURE genie_mul_long_mp;
extern GENIE_PROCEDURE genie_mul_real;
extern GENIE_PROCEDURE genie_ne_bits;
extern GENIE_PROCEDURE genie_ne_bool;
extern GENIE_PROCEDURE genie_ne_bytes;
extern GENIE_PROCEDURE genie_ne_char;
extern GENIE_PROCEDURE genie_ne_complex;
extern GENIE_PROCEDURE genie_ne_int;
extern GENIE_PROCEDURE genie_ne_long_bytes;
extern GENIE_PROCEDURE genie_ne_long_complex;
extern GENIE_PROCEDURE genie_ne_long_mp;
extern GENIE_PROCEDURE genie_ne_real;
extern GENIE_PROCEDURE genie_ne_string;
extern GENIE_PROCEDURE genie_next_random;
extern GENIE_PROCEDURE genie_nint_real;
extern GENIE_PROCEDURE genie_not_bits;
extern GENIE_PROCEDURE genie_not_bool;
extern GENIE_PROCEDURE genie_not_long_mp;
extern GENIE_PROCEDURE genie_null_char;
extern GENIE_PROCEDURE genie_odd_int;
extern GENIE_PROCEDURE genie_odd_long_mp;
extern GENIE_PROCEDURE genie_or_bits;
extern GENIE_PROCEDURE genie_or_bool;
extern GENIE_PROCEDURE genie_or_long_mp;
extern GENIE_PROCEDURE genie_over_int;
extern GENIE_PROCEDURE genie_over_long_mp;
extern GENIE_PROCEDURE genie_overab_int;
extern GENIE_PROCEDURE genie_overab_long_mp;
extern GENIE_PROCEDURE genie_divab_real;
extern GENIE_PROCEDURE genie_pi;
extern GENIE_PROCEDURE genie_pi_long_mp;
extern GENIE_PROCEDURE genie_plusab_bytes;
extern GENIE_PROCEDURE genie_plusab_complex;
extern GENIE_PROCEDURE genie_plusab_int;
extern GENIE_PROCEDURE genie_plusab_long_bytes;
extern GENIE_PROCEDURE genie_plusab_long_complex;
extern GENIE_PROCEDURE genie_plusab_long_int;
extern GENIE_PROCEDURE genie_plusab_long_mp;
extern GENIE_PROCEDURE genie_plusab_real;
extern GENIE_PROCEDURE genie_plusab_string;
extern GENIE_PROCEDURE genie_plusto_bytes;
extern GENIE_PROCEDURE genie_plusto_long_bytes;
extern GENIE_PROCEDURE genie_plusto_string;
extern GENIE_PROCEDURE genie_pow_complex_int;
extern GENIE_PROCEDURE genie_pow_int;
extern GENIE_PROCEDURE genie_pow_long_complex_int;
extern GENIE_PROCEDURE genie_pow_long_mp;
extern GENIE_PROCEDURE genie_pow_long_mp_int;
extern GENIE_PROCEDURE genie_pow_long_mp_int_int;
extern GENIE_PROCEDURE genie_pow_real;
extern GENIE_PROCEDURE genie_pow_real_int;
extern GENIE_PROCEDURE genie_preemptive_sweep_heap;
extern GENIE_PROCEDURE genie_program_idf;
extern GENIE_PROCEDURE genie_re_complex;
extern GENIE_PROCEDURE genie_re_long_complex;
extern GENIE_PROCEDURE genie_real_lengths;
extern GENIE_PROCEDURE genie_real_shorths;
extern GENIE_PROCEDURE genie_real_width;
extern GENIE_PROCEDURE genie_repr_char;
extern GENIE_PROCEDURE genie_round_long_mp;
extern GENIE_PROCEDURE genie_round_real;
extern GENIE_PROCEDURE genie_seconds;
extern GENIE_PROCEDURE genie_shl_bits;
extern GENIE_PROCEDURE genie_shl_long_mp;
extern GENIE_PROCEDURE genie_shorten_bytes;
extern GENIE_PROCEDURE genie_shorten_long_complex_to_complex;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_bits;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_int;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_real;
extern GENIE_PROCEDURE genie_shorten_longlong_complex_to_long_complex;
extern GENIE_PROCEDURE genie_shorten_longlong_mp_to_long_mp;
extern GENIE_PROCEDURE genie_shr_bits;
extern GENIE_PROCEDURE genie_shr_long_mp;
extern GENIE_PROCEDURE genie_sign_int;
extern GENIE_PROCEDURE genie_sign_long_mp;
extern GENIE_PROCEDURE genie_sign_real;
extern GENIE_PROCEDURE genie_sin_long_complex;
extern GENIE_PROCEDURE genie_sin_long_mp;
extern GENIE_PROCEDURE genie_sin_real;
extern GENIE_PROCEDURE genie_sinh_long_mp;
extern GENIE_PROCEDURE genie_small_real;
extern GENIE_PROCEDURE genie_sqrt_long_complex;
extern GENIE_PROCEDURE genie_sqrt_long_mp;
extern GENIE_PROCEDURE genie_sqrt_real;
extern GENIE_PROCEDURE genie_stack_pointer;
extern GENIE_PROCEDURE genie_stand_back;
extern GENIE_PROCEDURE genie_stand_back_channel;
extern GENIE_PROCEDURE genie_stand_draw_channel;
extern GENIE_PROCEDURE genie_stand_error;
extern GENIE_PROCEDURE genie_stand_error_channel;
extern GENIE_PROCEDURE genie_stand_in;
extern GENIE_PROCEDURE genie_stand_in_channel;
extern GENIE_PROCEDURE genie_stand_out;
extern GENIE_PROCEDURE genie_stand_out_channel;
extern GENIE_PROCEDURE genie_string_in_string;
extern GENIE_PROCEDURE genie_sub_complex;
extern GENIE_PROCEDURE genie_sub_int;
extern GENIE_PROCEDURE genie_sub_long_complex;
extern GENIE_PROCEDURE genie_sub_long_mp;
extern GENIE_PROCEDURE genie_sub_real;
extern GENIE_PROCEDURE genie_sweep_heap;
extern GENIE_PROCEDURE genie_system;
extern GENIE_PROCEDURE genie_system_stack_pointer;
extern GENIE_PROCEDURE genie_system_stack_size;
extern GENIE_PROCEDURE genie_tan_long_complex;
extern GENIE_PROCEDURE genie_tan_long_mp;
extern GENIE_PROCEDURE genie_tan_real;
extern GENIE_PROCEDURE genie_tanh_long_mp;
extern GENIE_PROCEDURE genie_term;
extern GENIE_PROCEDURE genie_times_char_int;
extern GENIE_PROCEDURE genie_times_int_char;
extern GENIE_PROCEDURE genie_times_int_string;
extern GENIE_PROCEDURE genie_times_string_int;
extern GENIE_PROCEDURE genie_timesab_complex;
extern GENIE_PROCEDURE genie_timesab_int;
extern GENIE_PROCEDURE genie_timesab_long_complex;
extern GENIE_PROCEDURE genie_timesab_long_int;
extern GENIE_PROCEDURE genie_timesab_long_mp;
extern GENIE_PROCEDURE genie_timesab_real;
extern GENIE_PROCEDURE genie_timesab_string;
extern GENIE_PROCEDURE genie_up_sema;
extern GENIE_PROCEDURE genie_vector_add;
extern GENIE_PROCEDURE genie_vector_div;
extern GENIE_PROCEDURE genie_vector_inner_product;
extern GENIE_PROCEDURE genie_vector_move;
extern GENIE_PROCEDURE genie_vector_mul;
extern GENIE_PROCEDURE genie_vector_set;
extern GENIE_PROCEDURE genie_vector_sub;
extern GENIE_PROCEDURE genie_vector_times_scalar;
extern GENIE_PROCEDURE genie_xor_bits;
extern GENIE_PROCEDURE genie_xor_bool;
extern GENIE_PROCEDURE genie_xor_long_mp;

extern PROPAGATOR_T genie_and_function (NODE_T *);
extern PROPAGATOR_T genie_assertion (NODE_T *);
extern PROPAGATOR_T genie_assignation (NODE_T *);
extern PROPAGATOR_T genie_assignation_quick (NODE_T *);
extern PROPAGATOR_T genie_call (NODE_T *);
extern PROPAGATOR_T genie_call_quick (NODE_T *);
extern PROPAGATOR_T genie_call_standenv_quick (NODE_T *);
extern PROPAGATOR_T genie_cast (NODE_T *);
extern PROPAGATOR_T genie_coercion (NODE_T *);
extern PROPAGATOR_T genie_collateral (NODE_T *);
extern PROPAGATOR_T genie_constant (NODE_T *);
extern PROPAGATOR_T genie_constant_int (NODE_T *);
extern PROPAGATOR_T genie_constant_real (NODE_T *);
extern PROPAGATOR_T genie_denoter (NODE_T *);
extern PROPAGATOR_T genie_deproceduring (NODE_T *);
extern PROPAGATOR_T genie_dereference_loc_identifier (NODE_T *);
extern PROPAGATOR_T genie_dereference_loc_identifier_int (NODE_T *);
extern PROPAGATOR_T genie_dereference_loc_identifier_real (NODE_T *);
extern PROPAGATOR_T genie_dereference_slice_loc_name_quick (NODE_T *);
extern PROPAGATOR_T genie_dereference_slice_loc_name_quick_int (NODE_T *);
extern PROPAGATOR_T genie_dereference_slice_loc_name_quick_real (NODE_T *);
extern PROPAGATOR_T genie_dereference_slice_name_quick (NODE_T *);
extern PROPAGATOR_T genie_dereferencing (NODE_T *);
extern PROPAGATOR_T genie_dereferencing_quick (NODE_T *);
extern PROPAGATOR_T genie_enclosed (volatile NODE_T *);
extern PROPAGATOR_T genie_format_text (NODE_T *);
extern PROPAGATOR_T genie_formula (NODE_T *);
extern PROPAGATOR_T genie_formula_quick (NODE_T *);
extern PROPAGATOR_T genie_formula_standenv_lhs_triad_quick (NODE_T *);
extern PROPAGATOR_T genie_formula_standenv_quick (NODE_T *);
extern PROPAGATOR_T genie_formula_standenv_rhs_triad_quick (NODE_T *);
extern PROPAGATOR_T genie_generator (NODE_T *);
extern PROPAGATOR_T genie_identifier (NODE_T *);
extern PROPAGATOR_T genie_identifier_standenv (NODE_T *);
extern PROPAGATOR_T genie_identifier_standenv_proc (NODE_T *);
extern PROPAGATOR_T genie_identity_relation (NODE_T *);
extern PROPAGATOR_T genie_loc_assignation (NODE_T *);
extern PROPAGATOR_T genie_loc_assignation_int (NODE_T *);
extern PROPAGATOR_T genie_loc_assignation_real (NODE_T *);
extern PROPAGATOR_T genie_loc_constant_assignation (NODE_T *);
extern PROPAGATOR_T genie_loc_constant_assignation_int (NODE_T *);
extern PROPAGATOR_T genie_loc_constant_assignation_real (NODE_T *);
extern PROPAGATOR_T genie_loc_identifier (NODE_T *);
extern PROPAGATOR_T genie_loc_identifier_int (NODE_T *);
extern PROPAGATOR_T genie_loc_identifier_real (NODE_T *);
extern PROPAGATOR_T genie_loc_identifier_ref (NODE_T *);
extern PROPAGATOR_T genie_monadic (NODE_T *);
extern PROPAGATOR_T genie_nihil (NODE_T *);
extern PROPAGATOR_T genie_or_function (NODE_T *);
extern PROPAGATOR_T genie_parallel (NODE_T *);
extern PROPAGATOR_T genie_primary (NODE_T *);
extern PROPAGATOR_T genie_routine_text (NODE_T *);
extern PROPAGATOR_T genie_rowing (NODE_T *);
extern PROPAGATOR_T genie_rowing_ref_row_of_row (NODE_T *);
extern PROPAGATOR_T genie_rowing_ref_row_row (NODE_T *);
extern PROPAGATOR_T genie_rowing_row_of_row (NODE_T *);
extern PROPAGATOR_T genie_rowing_row_row (NODE_T *);
extern PROPAGATOR_T genie_secondary (NODE_T *);
extern PROPAGATOR_T genie_selection (NODE_T *);
extern PROPAGATOR_T genie_selection_name (NODE_T *);
extern PROPAGATOR_T genie_selection_value (NODE_T *);
extern PROPAGATOR_T genie_skip (NODE_T *);
extern PROPAGATOR_T genie_slice (NODE_T *);
extern PROPAGATOR_T genie_slice_loc_name_quick (NODE_T *);
extern PROPAGATOR_T genie_slice_name_quick (NODE_T *);
extern PROPAGATOR_T genie_tertiary (NODE_T *);
extern PROPAGATOR_T genie_unit (NODE_T *);
extern PROPAGATOR_T genie_uniting (NODE_T *);
extern PROPAGATOR_T genie_voiding (NODE_T *);
extern PROPAGATOR_T genie_voiding_assignation_quick (NODE_T *);
extern PROPAGATOR_T genie_voiding_loc_assignation (NODE_T *);
extern PROPAGATOR_T genie_voiding_loc_assignation_int (NODE_T *);
extern PROPAGATOR_T genie_voiding_loc_assignation_real (NODE_T *);
extern PROPAGATOR_T genie_voiding_loc_constant_assignation (NODE_T *);
extern PROPAGATOR_T genie_voiding_loc_constant_assignation_int (NODE_T *);
extern PROPAGATOR_T genie_voiding_loc_constant_assignation_real (NODE_T *);
extern PROPAGATOR_T genie_widening (NODE_T *);
extern PROPAGATOR_T genie_widening_int_to_real (NODE_T *);

extern A68_REF c_string_to_row_char (NODE_T *, char *, int);
extern A68_REF c_to_a_string (NODE_T *, char *);
extern A68_REF empty_row (NODE_T *, MOID_T *);
extern A68_REF empty_string (NODE_T *);
extern A68_REF genie_allocate_declarer (NODE_T *, ADDR_T *, A68_REF, BOOL_T);
extern A68_REF genie_assign_stowed (A68_REF, A68_REF *, NODE_T *, MOID_T *);
extern A68_REF genie_concatenate_rows (NODE_T *, MOID_T *, int, ADDR_T);
extern A68_REF genie_copy_stowed (A68_REF, NODE_T *, MOID_T *);
extern A68_REF genie_make_ref_row_of_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
extern A68_REF genie_make_ref_row_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
extern A68_REF genie_make_row (NODE_T *, MOID_T *, int, ADDR_T);
extern BOOL_T breakpoint_expression (NODE_T *);
extern BOOL_T close_device (NODE_T *, A68_FILE *);
extern BOOL_T genie_int_case_unit (NODE_T *, int, int *);
extern BOOL_T genie_no_user_symbols (SYMBOL_TABLE_T *);
extern BOOL_T genie_string_to_value_internal (NODE_T *, MOID_T *, char *, BYTE_T *);
extern BOOL_T genie_united_case_unit (NODE_T *, MOID_T *);
extern char *a_to_c_string (NODE_T *, char *, A68_REF);
extern char *genie_standard_format (NODE_T *, MOID_T *, void *);
extern double dabs (double);
extern double dmod (double, double);
extern double dsign (double);
extern double rng_53_bit (void);
extern int a68_string_size (NODE_T *, A68_REF);
extern int iabs (int);
extern int isign (int);
extern NODE_T *last_unit;
extern void dump_frame (int);
extern void dump_stack (int);
extern void exit_genie (NODE_T *, int);
extern void genie (MODULE_T *);
extern void genie_call_operator (NODE_T *, ADDR_T);
extern void genie_call_procedure (NODE_T *, MOID_T *, MOID_T *, MOID_T *, A68_PROCEDURE *, ADDR_T, ADDR_T);
extern void genie_check_initialisation (NODE_T *, BYTE_T *, MOID_T *);
extern void genie_declaration (NODE_T *);
extern void genie_dump_frames ();
extern void genie_enquiry_clause (NODE_T *);
extern void genie_f_and_becomes (NODE_T *, MOID_T *, GENIE_PROCEDURE *);
extern void genie_generator_bounds (NODE_T *);
extern void genie_generator_internal (NODE_T *, MOID_T *, TAG_T *, LEAP_T, ADDR_T);
extern void genie_init_lib (NODE_T *);
extern void genie_prepare_declarer (NODE_T *);
extern void genie_push_undefined (NODE_T *, MOID_T *);
extern void genie_revise_lower_bound (NODE_T *, A68_REF, A68_REF);
extern void genie_serial_clause (NODE_T *, jmp_buf *);
extern void genie_serial_units (NODE_T *, NODE_T **, jmp_buf *, int);
extern void genie_subscript (NODE_T *, ADDR_T *, int *, NODE_T **);
extern void genie_subscript_linear (NODE_T *, ADDR_T *, int *);
extern void genie_unit_trace (NODE_T *);
extern void init_rng (unsigned long);
extern void initialise_frame (NODE_T *);
extern void install_signal_handlers (void);
extern void show_data_item (FILE_T, NODE_T *, MOID_T *, BYTE_T *);
extern void show_frame (NODE_T *, FILE_T);
extern void show_stack_frame (FILE_T, NODE_T *, ADDR_T);
extern void single_step (NODE_T *, BOOL_T, BOOL_T);
extern void stack_dump (FILE_T, ADDR_T, int);
extern void stack_dump_light (ADDR_T);
extern void un_init_frame (NODE_T *);
extern void where (FILE_T, NODE_T *);

extern void genie_argc (NODE_T *);
extern void genie_argv (NODE_T *);
extern void genie_create_pipe (NODE_T *);
extern void genie_utctime (NODE_T *);
extern void genie_localtime (NODE_T *);
extern void genie_errno (NODE_T *);
extern void genie_execve (NODE_T *);
extern void genie_execve_child (NODE_T *);
extern void genie_execve_child_pipe (NODE_T *);
extern void genie_fork (NODE_T *);
extern void genie_getenv (NODE_T *);
extern void genie_reset_errno (NODE_T *);
extern void genie_strerror (NODE_T *);
extern void genie_waitpid (NODE_T *);

#ifdef HAVE_HTTP
extern void genie_http_content (NODE_T *);
extern void genie_tcp_request (NODE_T *);
#endif

#ifdef HAVE_REGEX
extern void genie_grep_in_string (NODE_T *);
extern void genie_sub_in_string (NODE_T *);
#endif
#endif

#ifdef HAVE_CURSES
extern BOOL_T curses_active;
extern void genie_curses_clear (NODE_T *);
extern void genie_curses_columns (NODE_T *);
extern void genie_curses_end (NODE_T *);
extern void genie_curses_getchar (NODE_T *);
extern void genie_curses_lines (NODE_T *);
extern void genie_curses_move (NODE_T *);
extern void genie_curses_putchar (NODE_T *);
extern void genie_curses_refresh (NODE_T *);
extern void genie_curses_start (NODE_T *);
#endif

#ifdef HAVE_POSTGRESQL
extern void genie_pq_backendpid (NODE_T *);
extern void genie_pq_cmdstatus (NODE_T *);
extern void genie_pq_cmdtuples (NODE_T *);
extern void genie_pq_connectdb (NODE_T *);
extern void genie_pq_db (NODE_T *);
extern void genie_pq_errormessage (NODE_T *);
extern void genie_pq_exec (NODE_T *);
extern void genie_pq_fformat (NODE_T *);
extern void genie_pq_finish (NODE_T *);
extern void genie_pq_fname (NODE_T *);
extern void genie_pq_fnumber (NODE_T *);
extern void genie_pq_getisnull (NODE_T *);
extern void genie_pq_getvalue (NODE_T *);
extern void genie_pq_host (NODE_T *);
extern void genie_pq_nfields (NODE_T *);
extern void genie_pq_ntuples (NODE_T *);
extern void genie_pq_options (NODE_T *);
extern void genie_pq_parameterstatus (NODE_T *);
extern void genie_pq_pass (NODE_T *);
extern void genie_pq_port (NODE_T *);
extern void genie_pq_protocolversion (NODE_T *);
extern void genie_pq_reset (NODE_T *);
extern void genie_pq_resulterrormessage (NODE_T *);
extern void genie_pq_serverversion (NODE_T *);
extern void genie_pq_socket (NODE_T *);
extern void genie_pq_tty (NODE_T *);
extern void genie_pq_user (NODE_T *);
#endif

#ifdef HAVE_IEEE_754
#define NAN_STRING "NOT_A_NUMBER"
#define INF_STRING "INFINITY"
#endif
