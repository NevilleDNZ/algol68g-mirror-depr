/*!
\file genie.h
\brief contains genie related definitions
**/

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

#if ! defined A68G_INLINE_H
#define A68G_INLINE_H

/* Macros. */

/* ADDRESS calculates the effective address of fat pointer z. */

#define ADDRESS(z) (&((IS_IN_HEAP (z) ? REF_POINTER (z) : stack_segment)[REF_OFFSET (z)]))
#define ARRAY_ADDRESS(z) (&(REF_POINTER (z)[REF_OFFSET (z)]))
#define DEREF(mode, expr) ((mode *) ADDRESS (expr))

/* Prelude errors can also occur in the constant folder */

#define PRELUDE_ERROR(cond, p, txt, add)\
  if (cond) {\
    errno = ERANGE;\
    if (in_execution) {\
      diagnostic_node (A68_RUNTIME_ERROR, p, txt, add);\
      exit_genie (p, A68_RUNTIME_ERROR);\
    } else {\
      diagnostic_node (A68_MATH_ERROR, p, txt, add);\
    }}

/* Check on a NIL name. */

#define CHECK_REF(p, z, m)\
  if (! INITIALISED (&z)) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE_FROM, (m));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  } else if (IS_NIL (z)) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_ACCESSING_NIL, (m));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }

/* Check whether the heap fills. */

#define PREEMPTIVE_SWEEP {\
  double f = (double) heap_pointer / (double) heap_size;\
  double h = (double) free_handle_count / (double) max_handle_count;\
  if (f > 0.8 || h < 0.2) {\
    sweep_heap ((NODE_T *) p, frame_pointer);\
  }}

extern int block_heap_compacter;
/* Operations on stack frames */

#define FRAME_ADDRESS(n) ((BYTE_T *) &(stack_segment[n]))
#define FRAME_CLEAR(m) FILL_ALIGNED ((BYTE_T *) FRAME_OFFSET (FRAME_INFO_SIZE), 0, (m))
#define FRAME_DYNAMIC_LINK(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->dynamic_link)
#define FRAME_DYNAMIC_SCOPE(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->dynamic_scope)
#define FRAME_INCREMENT(n) (SYMBOL_TABLE (FRAME_TREE(n))->ap_increment)
#define FRAME_INFO_SIZE (A68_ALIGN (ALIGNED_SIZE_OF (ACTIVATION_RECORD)))
#define FRAME_JUMP_STAT(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->jump_stat)
#define FRAME_LEXICAL_LEVEL(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->frame_level)
#define FRAME_LOCAL(n, m) (FRAME_ADDRESS ((n) + FRAME_INFO_SIZE + (m)))
#define FRAME_NUMBER(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->frame_no)
#define FRAME_OBJECT(n) (FRAME_OFFSET (FRAME_INFO_SIZE + (n)))
#define FRAME_OFFSET(n) (FRAME_ADDRESS (frame_pointer + (n)))
#define FRAME_OUTER(n) (SYMBOL_TABLE (FRAME_TREE(n))->outer)
#define FRAME_PARAMETER_LEVEL(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->parameter_level)
#define FRAME_PARAMETERS(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->parameters)
#define FRAME_PROC_FRAME(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->proc_frame)
#define FRAME_SIZE(fp) (FRAME_INFO_SIZE + FRAME_INCREMENT (fp))
#define FRAME_STATIC_LINK(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->static_link)
#define FRAME_TOP (FRAME_OFFSET (0))
#define FRAME_TREE(n) (NODE ((ACTIVATION_RECORD *) FRAME_ADDRESS(n)))

#if defined ENABLE_PAR_CLAUSE
#define FRAME_THREAD_ID(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->thread_id)
#endif

#define FOLLOW_STATIC_LINK(dest, l) {\
  if ((l) == global_level && global_pointer > 0) {\
    (dest) = global_pointer;\
  } else {\
    (dest) = frame_pointer;\
    if ((l) <= FRAME_PARAMETER_LEVEL ((dest))) {\
      (dest) = FRAME_PARAMETERS ((dest));\
    }\
    while ((l) != FRAME_LEXICAL_LEVEL ((dest))) {\
      (dest) = FRAME_STATIC_LINK ((dest));\
    }\
  }}

#define FRAME_GET(dest, cast, p) {\
  ADDR_T _m_z;\
  FOLLOW_STATIC_LINK (_m_z, GENIE (p)->level);\
  (dest) = (cast *) & ((GENIE (p)->offset)[_m_z]);\
  }

/* Macros for row-handling. */

#define GET_DESCRIPTOR(a, t, p)\
  a = (A68_ARRAY *) ARRAY_ADDRESS (p);\
  t = (A68_TUPLE *) & (((BYTE_T *) (a)) [ALIGNED_SIZE_OF (A68_ARRAY)]);

#define GET_DESCRIPTOR2(a, t1, t2, p)\
  a = (A68_ARRAY *) ARRAY_ADDRESS (p);\
  t1 = (A68_TUPLE *) & (((BYTE_T *) (a)) [ALIGNED_SIZE_OF (A68_ARRAY)]);\
  t2 = (A68_TUPLE *) & (((BYTE_T *) (a)) [ALIGNED_SIZE_OF (A68_ARRAY) + sizeof (A68_TUPLE)]);

#define PUT_DESCRIPTOR(a, t1, p) {\
  BYTE_T *a_p = ARRAY_ADDRESS (p);\
  *(A68_ARRAY *) a_p = (a);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [ALIGNED_SIZE_OF (A68_ARRAY)]) = (t1);\
  }

#define PUT_DESCRIPTOR2(a, t1, t2, p) {\
  /* ABEND (IS_NIL (*p), ERROR_NIL_DESCRIPTOR, NULL); */\
  BYTE_T *a_p = ARRAY_ADDRESS (p);\
  *(A68_ARRAY *) a_p = (a);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [ALIGNED_SIZE_OF (A68_ARRAY)]) = (t1);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [ALIGNED_SIZE_OF (A68_ARRAY) + sizeof (A68_TUPLE)]) = (t2);\
  }

#define ROW_SIZE(t) ((UPB (t) >= LWB (t)) ? (UPB (t) - LWB (t) + 1) : 0)

#define ROW_ELEMENT(a, k) (((ADDR_T) k + (a)->slice_offset) * (a)->elem_size + (a)->field_offset)
#define ROW_ELEM(a, k, s) (((ADDR_T) k + (a)->slice_offset) * (s) + (a)->field_offset)

#define INDEX_1_DIM(a, t, k) ROW_ELEMENT (a, ((t)->span * (int) k - (t)->shift))

/* Macros for execution. */

#define EXECUTE_UNIT_2(p, dest) {\
  PROPAGATOR_T *_prop_ = &PROPAGATOR (p);\
  last_unit = p;\
  dest = (*(_prop_->unit)) (_prop_->source);\
  }

#define EXECUTE_UNIT(p) {\
  PROPAGATOR_T *_prop_ = &PROPAGATOR (p);\
  last_unit = p;\
  (void) (*(_prop_->unit)) (_prop_->source);\
  }

#define EXECUTE_UNIT_TRACE(p) {\
  PROPAGATOR_T *_prop_ = &PROPAGATOR (p);\
  if (STATUS_TEST (p, (BREAKPOINT_MASK | BREAKPOINT_TEMPORARY_MASK | \
      BREAKPOINT_INTERRUPT_MASK | BREAKPOINT_WATCH_MASK | BREAKPOINT_TRACE_MASK))) {\
    single_step ((p), STATUS (p));\
  }\
  errno = 0;\
  last_unit = p;\
  (void) (*(_prop_->unit)) (_prop_->source);\
  if (errno != 0) {\
    diagnostic_node (A68_RUNTIME_ERROR, (NODE_T *) p, ERROR_RUNTIME_ERROR);\
  }}

/* Macro's for the garbage collector. */

/* Store intermediate REF to save it from the GC. */

#define PROTECT_FROM_SWEEP(p, z)\
  if ((p)->protect_sweep != NULL) {\
    *(A68_REF *) FRAME_LOCAL (frame_pointer, (p)->protect_sweep->offset) = *(A68_REF *) (z);\
  }

#define PROTECT_FROM_SWEEP_STACK(p)\
  if (GENIE (p)->protect_sweep != NULL) {\
    *(A68_REF *) FRAME_LOCAL (frame_pointer, GENIE (p)->protect_sweep->offset) =\
    *(A68_REF *) (STACK_OFFSET (- ALIGNED_SIZE_OF (A68_REF)));\
  }

extern int block_heap_compacter;

#define UP_SWEEP_SEMA {block_heap_compacter++;}
#define DOWN_SWEEP_SEMA {block_heap_compacter--;}

#define PROTECT_SWEEP_HANDLE(z) { if (IS_IN_HEAP (z)) {STATUS_SET (REF_HANDLE(z), NO_SWEEP_MASK);} }
#define UNPROTECT_SWEEP_HANDLE(z) { if (IS_IN_HEAP (z)) {STATUS_CLEAR (REF_HANDLE (z), NO_SWEEP_MASK);} }

/* Tests for objects of mode INT. */

#if defined ENABLE_IEEE_754
#define CHECK_INT_ADDITION(p, i, j)\
  PRELUDE_ERROR (ABS ((double) (i) + (double) (j)) > (double) INT_MAX, p, ERROR_MATH, MODE (INT))
#define CHECK_INT_SUBTRACTION(p, i, j)\
  PRELUDE_ERROR (ABS ((double) (i) - (double) (j)) > (double) INT_MAX, p, ERROR_MATH, MODE (INT))
#define CHECK_INT_MULTIPLICATION(p, i, j)\
  PRELUDE_ERROR (ABS ((double) (i) * (double) (j)) > (double) INT_MAX, p, ERROR_MATH, MODE (INT))
#define CHECK_INT_DIVISION(p, i, j)\
  PRELUDE_ERROR ((j) == 0, p, ERROR_DIVISION_BY_ZERO, MODE (INT))
#else
#define CHECK_INT_ADDITION(p, i, j) {;}
#define CHECK_INT_SUBTRACTION(p, i, j) {;}
#define CHECK_INT_MULTIPLICATION(p, i, j) {;}
#define CHECK_INT_DIVISION(p, i, j) {;}
#endif

#define CHECK_INDEX(p, k, t) {\
  if (VALUE (k) < LWB (t) || VALUE (k) > UPB (t)) {\
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }}

/* Tests for objects of mode REAL. */

#if defined ENABLE_IEEE_754

#if defined ENABLE_WIN32
extern int finite (double);
#endif

#define NOT_A_REAL(x) (!finite (x))
#define CHECK_REAL_REPRESENTATION(p, u)\
  PRELUDE_ERROR (NOT_A_REAL (u), p, ERROR_MATH, MODE (REAL))
#define CHECK_REAL_ADDITION(p, u, v) CHECK_REAL_REPRESENTATION (p, (u) + (v))
#define CHECK_REAL_SUBTRACTION(p, u, v) CHECK_REAL_REPRESENTATION (p, (u) - (v))
#define CHECK_REAL_MULTIPLICATION(p, u, v) CHECK_REAL_REPRESENTATION (p, (u) * (v))
#define CHECK_REAL_DIVISION(p, u, v)\
  PRELUDE_ERROR ((v) == 0, p, ERROR_DIVISION_BY_ZERO, MODE (REAL))
#define CHECK_COMPLEX_REPRESENTATION(p, u, v)\
  PRELUDE_ERROR (NOT_A_REAL (u) || NOT_A_REAL (v), p, ERROR_MATH, MODE (COMPLEX))
#else
#define CHECK_REAL_REPRESENTATION(p, u) {;}
#define CHECK_REAL_ADDITION(p, u, v) {;}
#define CHECK_REAL_SUBTRACTION(p, u, v) {;}
#define CHECK_REAL_MULTIPLICATION(p, u, v) {;}
#define CHECK_REAL_DIVISION(p, u, v) {;}
#define CHECK_COMPLEX_REPRESENTATION(p, u, v) {;}
#endif

#define math_rte(p, z, m, t)\
  PRELUDE_ERROR (z, (p), ERROR_MATH, (m))

/*
Macro's for stack checking. Since the stacks grow by small amounts at a time
(A68 rows are in the heap), we check the stacks only at certain points: where
A68 recursion may set in, or in the garbage collector. We check whether there
still is sufficient overhead to make it to the next check.
*/

#define TOO_COMPLEX "program too complex"

#if defined ENABLE_SYS_STACK_CHECK
#define SYSTEM_STACK_USED (ABS ((int) system_stack_offset - (int) &stack_offset))
#define LOW_STACK_ALERT(p) {\
  BYTE_T stack_offset;\
  if (stack_size > 0 && SYSTEM_STACK_USED >= stack_limit) {\
    errno = 0;\
    if ((p) == NULL) {\
      ABEND (A68_TRUE, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
    } else {\
      diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
      exit_genie ((p), A68_RUNTIME_ERROR);\
    }\
  }\
  if ((p) != NULL && (frame_pointer >= frame_stack_limit || stack_pointer >= expr_stack_limit)) { \
    errno = 0;\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }}
#else
#define LOW_STACK_ALERT(p) {\
  (void) (p);\
  errno = 0;\
  ABEND (stack_pointer >= expr_stack_limit, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
  ABEND (frame_pointer >= frame_stack_limit, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
  }
#endif

/* Opening of stack frames is in-line. */

/* 
STATIC_LINK_FOR_FRAME: determine static link for stack frame.
new_lex_lvl: lexical level of new stack frame.
returns: static link for stack frame at 'new_lex_lvl'. 
*/

#define STATIC_LINK_FOR_FRAME(dest, new_lex_lvl) {\
  int _m_cur_lex_lvl = FRAME_LEXICAL_LEVEL (frame_pointer);\
  if (_m_cur_lex_lvl == (new_lex_lvl)) {\
    (dest) = FRAME_STATIC_LINK (frame_pointer);\
  } else if (_m_cur_lex_lvl > (new_lex_lvl)) {\
    ADDR_T _m_static_link = frame_pointer;\
    while (FRAME_LEXICAL_LEVEL (_m_static_link) >= (new_lex_lvl)) {\
      _m_static_link = FRAME_STATIC_LINK (_m_static_link);\
    }\
    (dest) = _m_static_link;\
  } else {\
    (dest) = frame_pointer;\
  }}

#if defined ENABLE_PAR_CLAUSE
#define OPEN_STATIC_FRAME(p) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act, *pre;\
  STATIC_LINK_FOR_FRAME (static_link, LEX_LEVEL (p));\
  pre = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->frame_no = pre->frame_no + 1;\
  act->frame_level = LEX_LEVEL (p);\
  act->parameter_level = pre->parameter_level;\
  act->parameters = pre->parameters;\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A68_FALSE;\
  act->thread_id = pthread_self ();\
  FRAME_CLEAR (SYMBOL_TABLE (p)->ap_increment);\
  if (LEX_LEVEL (p) == global_level) {\
    global_pointer = frame_pointer;\
  }\
  if (SYMBOL_TABLE (p)->initialise_frame) {\
    initialise_frame (p);\
  }}
#else
#define OPEN_STATIC_FRAME(p) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act, *pre;\
  STATIC_LINK_FOR_FRAME (static_link, LEX_LEVEL (p));\
  pre = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->frame_no = pre->frame_no + 1;\
  act->frame_level = LEX_LEVEL (p);\
  act->parameter_level = pre->parameter_level;\
  act->parameters = pre->parameters;\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A68_FALSE;\
  FRAME_CLEAR (SYMBOL_TABLE (p)->ap_increment);\
  if (LEX_LEVEL (p) == global_level) {\
    global_pointer = frame_pointer;\
  }\
  if (SYMBOL_TABLE (p)->initialise_frame) {\
    initialise_frame (p);\
  }}
#endif

#if defined ENABLE_PAR_CLAUSE
#define OPEN_PROC_FRAME(p, environ) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act;\
  PREEMPTIVE_SWEEP;\
  LOW_STACK_ALERT (p);\
  static_link = (environ > 0 ? environ : frame_pointer);\
  if (frame_pointer < static_link) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_SCOPE_DYNAMIC_0);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->frame_no = FRAME_NUMBER (dynamic_link) + 1;\
  act->frame_level = LEX_LEVEL (p);\
  act->parameter_level = LEX_LEVEL (p);\
  act->parameters = frame_pointer;\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A68_TRUE;\
  act->thread_id = pthread_self ();\
  FRAME_CLEAR (SYMBOL_TABLE (p)->ap_increment);\
  if (SYMBOL_TABLE (p)->initialise_frame) {\
    initialise_frame (p);\
  }}
#else
#define OPEN_PROC_FRAME(p, environ) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act;\
  PREEMPTIVE_SWEEP;\
  LOW_STACK_ALERT (p);\
  static_link = (environ > 0 ? environ : frame_pointer);\
  if (frame_pointer < static_link) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_SCOPE_DYNAMIC_0);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->frame_no = FRAME_NUMBER (dynamic_link) + 1;\
  act->frame_level = LEX_LEVEL (p);\
  act->parameter_level = LEX_LEVEL (p);\
  act->parameters = frame_pointer;\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A68_TRUE;\
  FRAME_CLEAR (SYMBOL_TABLE (p)->ap_increment);\
  if (SYMBOL_TABLE (p)->initialise_frame) {\
    initialise_frame (p);\
  }}
#endif

#define CLOSE_FRAME {\
  frame_pointer = FRAME_DYNAMIC_LINK (frame_pointer);\
  }

/* Macros for check on initialisation of values. */

#define CHECK_INIT(p, c, q)\
  if (!(c)) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE_FROM, (q));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }

#define CHECK_INIT_GENERIC(p, w, q) {\
  switch ((q)->short_id) {\
  case MODE_INT: {\
      CHECK_INIT ((p), INITIALISED ((A68_INT *) (w)), (q));\
      break;\
    }\
  case MODE_REAL: {\
      CHECK_INIT ((p), INITIALISED ((A68_REAL *) (w)), (q));\
      break;\
    }\
  case MODE_BOOL: {\
      CHECK_INIT ((p), INITIALISED ((A68_BOOL *) (w)), (q));\
      break;\
    }\
  case MODE_CHAR: {\
      CHECK_INIT ((p), INITIALISED ((A68_CHAR *) (w)), (q));\
      break;\
    }\
  case MODE_BITS: {\
      CHECK_INIT ((p), INITIALISED ((A68_BITS *) (w)), (q));\
      break;\
    }\
  case MODE_COMPLEX: {\
      A68_REAL *_r_ = (A68_REAL *) (w);\
      A68_REAL *_i_ = (A68_REAL *) ((w) + ALIGNED_SIZE_OF (A68_REAL));\
      CHECK_INIT ((p), INITIALISED (_r_), (q));\
      CHECK_INIT ((p), INITIALISED (_i_), (q));\
      break;\
    }\
  case ROW_SYMBOL:\
  case REF_SYMBOL: {\
      CHECK_INIT ((p), INITIALISED ((A68_REF *) (w)), (q));\
      break;\
    }\
  case PROC_SYMBOL: {\
      CHECK_INIT ((p), INITIALISED ((A68_PROCEDURE *) (w)), (q));\
      break;\
    }\
  default: {\
      genie_check_initialisation ((p), (BYTE_T *) (w), (q));\
      break;\
    }\
  }\
}

#define SCOPE_CHECK(p, scope, limit, mode, info)\
  if (scope > limit) {\
    char txt[BUFFER_SIZE];\
    if (info == NULL) {\
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, ERROR_SCOPE_DYNAMIC_1) >= 0);\
    } else {\
      ASSERT (snprintf (txt, (size_t) BUFFER_SIZE, ERROR_SCOPE_DYNAMIC_2, info) >= 0);\
    }\
    diagnostic_node (A68_RUNTIME_ERROR, p, txt, mode);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }

#define INCREMENT_STACK_POINTER(err, i) {stack_pointer += (ADDR_T) A68_ALIGN (i); (void) (err);}

#define DECREMENT_STACK_POINTER(err, i) {\
  stack_pointer -= A68_ALIGN (i);\
  (void) (err);\
  }

#define PUSH(p, addr, size) {\
  BYTE_T *_sp_ = STACK_TOP;\
  INCREMENT_STACK_POINTER ((p), (int) (size));\
  COPY (_sp_, (BYTE_T *) (addr), (int) (size));\
  }

#define PUSH_ALIGNED(p, addr, size) {\
  BYTE_T *_sp_ = STACK_TOP;\
  INCREMENT_STACK_POINTER ((p), (int) (size));\
  COPY_ALIGNED (_sp_, (BYTE_T *) (addr), (int) (size));\
  }

#define POP(p, addr, size) {\
  DECREMENT_STACK_POINTER((p), (int) (size));\
  COPY ((BYTE_T *) (addr), STACK_TOP, (int) (size));\
  }

#define POP_ALIGNED(p, addr, size) {\
  DECREMENT_STACK_POINTER((p), (int) (size));\
  COPY_ALIGNED ((BYTE_T *) (addr), STACK_TOP, (int) (size));\
  }

#define POP_ADDRESS(p, addr, type) {\
  DECREMENT_STACK_POINTER((p), (int) ALIGNED_SIZE_OF (type));\
  (addr) = (type *) STACK_TOP;\
  }

#define POP_OPERAND_ADDRESS(p, i, type) {\
  (void) (p);\
  (i) = (type *) (STACK_OFFSET (-ALIGNED_SIZE_OF (type)));\
  }

#define POP_OPERAND_ADDRESSES(p, i, j, type) {\
  DECREMENT_STACK_POINTER ((p), (int) ALIGNED_SIZE_OF (type));\
  (j) = (type *) STACK_TOP;\
  (i) = (type *) (STACK_OFFSET (-ALIGNED_SIZE_OF (type)));\
  }

#define POP_3_OPERAND_ADDRESSES(p, i, j, k, type) {\
  DECREMENT_STACK_POINTER ((p), (int) (2 * ALIGNED_SIZE_OF (type)));\
  (k) = (type *) (STACK_OFFSET (ALIGNED_SIZE_OF (type)));\
  (j) = (type *) STACK_TOP;\
  (i) = (type *) (STACK_OFFSET (-ALIGNED_SIZE_OF (type)));\
  }

#define PUSH_PRIMITIVE(p, z, mode) {\
  mode *_x_ = (mode *) STACK_TOP;\
  _x_->status = INITIALISED_MASK;\
  _x_->value = (z);\
  INCREMENT_STACK_POINTER((p), ALIGNED_SIZE_OF (mode));\
  }

#define PUSH_OBJECT(p, z, mode) {\
  *(mode *) STACK_TOP = (z);\
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (mode));\
  }

#define POP_OBJECT(p, z, mode) {\
  DECREMENT_STACK_POINTER((p), ALIGNED_SIZE_OF (mode));\
  (*(z)) = *((mode *) STACK_TOP);\
  }

#define PUSH_COMPLEX(p, re, im) {\
  PUSH_PRIMITIVE (p, re, A68_REAL);\
  PUSH_PRIMITIVE (p, im, A68_REAL);\
  }

#define POP_COMPLEX(p, re, im) {\
  POP_OBJECT (p, im, A68_REAL);\
  POP_OBJECT (p, re, A68_REAL);\
  }

#define PUSH_BYTES(p, k) {\
  A68_BYTES *_z_ = (A68_BYTES *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  strncpy (_z_->value, k, BYTES_WIDTH);\
  INCREMENT_STACK_POINTER((p), ALIGNED_SIZE_OF (A68_BYTES));\
  }

#define PUSH_LONG_BYTES(p, k) {\
  A68_LONG_BYTES *_z_ = (A68_LONG_BYTES *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  strncpy (_z_->value, k, LONG_BYTES_WIDTH);\
  INCREMENT_STACK_POINTER((p), ALIGNED_SIZE_OF (A68_LONG_BYTES));\
  }

#define PUSH_REF(p, z) PUSH_OBJECT (p, z, A68_REF)
#define PUSH_PROCEDURE(p, z) PUSH_OBJECT (p, z, A68_PROCEDURE)
#define PUSH_FORMAT(p, z) PUSH_OBJECT (p, z, A68_FORMAT)

#define POP_REF(p, z) POP_OBJECT (p, z, A68_REF)
#define POP_PROCEDURE(p, z) POP_OBJECT (p, z, A68_PROCEDURE)
#define POP_FORMAT(p, z) POP_OBJECT (p, z, A68_FORMAT)

#define PUSH_UNION(p, z) PUSH_PRIMITIVE (p, z, A68_UNION)

/* Macro's for standard environ. */

#define A68_ENV_INT(n, k) void n (NODE_T *p) {PUSH_PRIMITIVE (p, (k), A68_INT);}
#define A68_ENV_REAL(n, z) void n (NODE_T *p) {PUSH_PRIMITIVE (p, (z), A68_REAL);}

#endif

