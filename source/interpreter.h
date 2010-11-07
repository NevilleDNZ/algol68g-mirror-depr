/*!
\file interpreter.h
\brief contains interpreter related definitions
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

#if ! defined A68G_INTERPRETER_H
#define A68G_INTERPRETER_H

/* Inline macros. */

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

/* 
The void * cast in next macro is to stop warnings about dropping a volatile
qualifier to a pointer. This is safe here.
*/

#define GENIE_DNS_STACK(p, m, limit, info)\
  if (p != NULL && GENIE (p) != NULL && GENIE (p)->need_dns && limit != PRIMAL_SCOPE) {\
    genie_dns_addr ((void *)(p), (m), (STACK_OFFSET (-MOID_SIZE (m))), (limit), (info));\
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

/* Interpreter macros. */

#define INITIALISED(z) ((BOOL_T) ((z)->status & INITIALISED_MASK))

#define BITS_WIDTH ((int) (1 + ceil (log ((double) A68_MAX_INT) / log((double) 2))))
#define INT_WIDTH ((int) (1 + floor (log ((double) A68_MAX_INT) / log ((double) 10))))

#define CHAR_WIDTH (1 + (int) log10 ((double) SCHAR_MAX))
#define REAL_WIDTH (DBL_DIG)
#define EXP_WIDTH ((int) (1 + log10 ((double) DBL_MAX_10_EXP)))

#define HEAP_ADDRESS(n) ((BYTE_T *) & (heap_segment[n]))

#define LHS_MODE(p) (MOID (PACK (MOID (p))))
#define RHS_MODE(p) (MOID (NEXT (PACK (MOID (p)))))

/* Activation records in the frame stack. */

typedef struct ACTIVATION_RECORD ACTIVATION_RECORD;

struct ACTIVATION_RECORD
{
  ADDR_T static_link, dynamic_link, dynamic_scope, parameters;
  NODE_T *node;
  jmp_buf *jump_stat;
  BOOL_T proc_frame;
  int frame_no, frame_level, parameter_level;
#if defined ENABLE_PAR_CLAUSE
  pthread_t thread_id;
#endif
};

/* Stack manipulation. */

#define STACK_ADDRESS(n) ((BYTE_T *) &(stack_segment[(n)]))
#define STACK_OFFSET(n) (STACK_ADDRESS (stack_pointer + (int) (n)))
#define STACK_TOP (STACK_ADDRESS (stack_pointer))

/* External symbols. */

extern ADDR_T frame_pointer, stack_pointer, heap_pointer, handle_pointer, global_pointer, frame_start, frame_end, stack_start, stack_end, finish_frame_pointer;
extern A68_FORMAT nil_format;
extern A68_HANDLE nil_handle, *free_handles, *busy_handles;
extern A68_REF nil_ref;
extern BOOL_T in_monitor, do_confirm_exit;
extern BYTE_T *stack_segment, *heap_segment, *handle_segment;
extern MOID_T *top_expr_moid;
extern char *watchpoint_expression;
extern double cputime_0;
extern int global_level, max_lex_lvl, ret_code;
extern jmp_buf genie_exit_label, monitor_exit_label;

extern int frame_stack_size, expr_stack_size, heap_size, handle_pool_size;
extern int stack_limit, frame_stack_limit, expr_stack_limit;
extern int storage_overhead;

/* External symbols. */

#if defined ENABLE_PAR_CLAUSE
#endif

extern int free_handle_count, max_handle_count;
extern void genie_find_proc_op (NODE_T *, int *);
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
extern GENIE_PROCEDURE genie_arccosh_long_mp;
extern GENIE_PROCEDURE genie_arccos_real;
extern GENIE_PROCEDURE genie_arcsinh_long_mp;
extern GENIE_PROCEDURE genie_arcsin_real;
extern GENIE_PROCEDURE genie_arctanh_long_mp;
extern GENIE_PROCEDURE genie_arctan_real;
extern GENIE_PROCEDURE genie_arg_complex;
extern GENIE_PROCEDURE genie_arg_long_complex;
extern GENIE_PROCEDURE genie_asin_long_complex;
extern GENIE_PROCEDURE genie_asin_long_mp;
extern GENIE_PROCEDURE genie_atan2_long_mp;
extern GENIE_PROCEDURE genie_atan2_real;
extern GENIE_PROCEDURE genie_atan_long_complex;
extern GENIE_PROCEDURE genie_atan_long_mp;
extern GENIE_PROCEDURE genie_abs_bits;
extern GENIE_PROCEDURE genie_bin_int;
extern GENIE_PROCEDURE genie_bin_long_mp;
extern GENIE_PROCEDURE genie_bits_lengths;
extern GENIE_PROCEDURE genie_bits_pack;
extern GENIE_PROCEDURE genie_bits_shorths;
extern GENIE_PROCEDURE genie_bits_width;
extern GENIE_PROCEDURE genie_break;
extern GENIE_PROCEDURE genie_bytes_lengths;
extern GENIE_PROCEDURE genie_bytespack;
extern GENIE_PROCEDURE genie_bytes_shorths;
extern GENIE_PROCEDURE genie_bytes_width;
extern GENIE_PROCEDURE genie_cd;
extern GENIE_PROCEDURE genie_char_in_string;
extern GENIE_PROCEDURE genie_clear_bits;
extern GENIE_PROCEDURE genie_clear_long_bits;
extern GENIE_PROCEDURE genie_clear_longlong_bits;
extern GENIE_PROCEDURE genie_complex_lengths;
extern GENIE_PROCEDURE genie_complex_shorths;
extern GENIE_PROCEDURE genie_conj_complex;
extern GENIE_PROCEDURE genie_conj_long_complex;
extern GENIE_PROCEDURE genie_cosh_long_mp;
extern GENIE_PROCEDURE genie_cos_long_complex;
extern GENIE_PROCEDURE genie_cos_long_mp;
extern GENIE_PROCEDURE genie_cos_real;
extern GENIE_PROCEDURE genie_cputime;
extern GENIE_PROCEDURE genie_curt_long_mp;
extern GENIE_PROCEDURE genie_curt_real;
extern GENIE_PROCEDURE genie_debug;
extern GENIE_PROCEDURE genie_directory;
extern GENIE_PROCEDURE genie_divab_complex;
extern GENIE_PROCEDURE genie_divab_long_complex;
extern GENIE_PROCEDURE genie_divab_long_mp;
extern GENIE_PROCEDURE genie_divab_real;
extern GENIE_PROCEDURE genie_div_complex;
extern GENIE_PROCEDURE genie_div_int;
extern GENIE_PROCEDURE genie_div_long_complex;
extern GENIE_PROCEDURE genie_div_long_mp;
extern GENIE_PROCEDURE genie_div_real;
extern GENIE_PROCEDURE genie_dyad_elems;
extern GENIE_PROCEDURE genie_dyad_lwb;
extern GENIE_PROCEDURE genie_dyad_upb;
extern GENIE_PROCEDURE genie_elem_bits;
extern GENIE_PROCEDURE genie_elem_bytes;
extern GENIE_PROCEDURE genie_elem_long_bits;
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
extern GENIE_PROCEDURE genie_file_is_block_device;
extern GENIE_PROCEDURE genie_file_is_char_device;
extern GENIE_PROCEDURE genie_file_is_directory;
extern GENIE_PROCEDURE genie_file_is_regular;
extern GENIE_PROCEDURE genie_file_mode;
extern GENIE_PROCEDURE genie_first_random;
extern GENIE_PROCEDURE genie_garbage_collections;
extern GENIE_PROCEDURE genie_garbage_freed;
extern GENIE_PROCEDURE genie_garbage_seconds;
extern GENIE_PROCEDURE genie_ge_bits;
extern GENIE_PROCEDURE genie_ge_bytes;
extern GENIE_PROCEDURE genie_ge_char;
extern GENIE_PROCEDURE genie_ge_int;
extern GENIE_PROCEDURE genie_ge_long_bits;
extern GENIE_PROCEDURE genie_ge_long_bytes;
extern GENIE_PROCEDURE genie_ge_long_mp;
extern GENIE_PROCEDURE genie_ge_real;
extern GENIE_PROCEDURE genie_ge_string;
extern GENIE_PROCEDURE genie_get_sound;
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
extern GENIE_PROCEDURE genie_is_alnum;
extern GENIE_PROCEDURE genie_is_alpha;
extern GENIE_PROCEDURE genie_is_cntrl;
extern GENIE_PROCEDURE genie_is_digit;
extern GENIE_PROCEDURE genie_is_graph;
extern GENIE_PROCEDURE genie_is_lower;
extern GENIE_PROCEDURE genie_is_print;
extern GENIE_PROCEDURE genie_is_punct;
extern GENIE_PROCEDURE genie_is_space;
extern GENIE_PROCEDURE genie_is_upper;
extern GENIE_PROCEDURE genie_is_xdigit;
extern GENIE_PROCEDURE genie_last_char_in_string;
extern GENIE_PROCEDURE genie_le_bits;
extern GENIE_PROCEDURE genie_le_bytes;
extern GENIE_PROCEDURE genie_le_char;
extern GENIE_PROCEDURE genie_le_int;
extern GENIE_PROCEDURE genie_le_long_bits;
extern GENIE_PROCEDURE genie_le_long_bytes;
extern GENIE_PROCEDURE genie_le_long_mp;
extern GENIE_PROCEDURE genie_leng_bytes;
extern GENIE_PROCEDURE genie_lengthen_complex_to_long_complex;
extern GENIE_PROCEDURE genie_lengthen_int_to_long_mp;
extern GENIE_PROCEDURE genie_lengthen_long_complex_to_longlong_complex;
extern GENIE_PROCEDURE genie_lengthen_long_mp_to_longlong_mp;
extern GENIE_PROCEDURE genie_lengthen_real_to_long_mp;
extern GENIE_PROCEDURE genie_lengthen_unsigned_to_long_mp;
extern GENIE_PROCEDURE genie_le_real;
extern GENIE_PROCEDURE genie_le_string;
extern GENIE_PROCEDURE genie_lj_e_12_6;
extern GENIE_PROCEDURE genie_lj_f_12_6;
extern GENIE_PROCEDURE genie_ln_long_complex;
extern GENIE_PROCEDURE genie_ln_long_mp;
extern GENIE_PROCEDURE genie_ln_real;
extern GENIE_PROCEDURE genie_log_long_mp;
extern GENIE_PROCEDURE genie_log_real;
extern GENIE_PROCEDURE genie_long_bits_pack;
extern GENIE_PROCEDURE genie_long_bits_width;
extern GENIE_PROCEDURE genie_long_bytespack;
extern GENIE_PROCEDURE genie_long_bytes_width;
extern GENIE_PROCEDURE genie_long_exp_width;
extern GENIE_PROCEDURE genie_long_int_width;
extern GENIE_PROCEDURE genie_longlong_bits_width;
extern GENIE_PROCEDURE genie_longlong_exp_width;
extern GENIE_PROCEDURE genie_longlong_int_width;
extern GENIE_PROCEDURE genie_longlong_max_bits;
extern GENIE_PROCEDURE genie_longlong_max_int;
extern GENIE_PROCEDURE genie_longlong_max_real;
extern GENIE_PROCEDURE genie_longlong_min_real;
extern GENIE_PROCEDURE genie_longlong_real_width;
extern GENIE_PROCEDURE genie_longlong_small_real;
extern GENIE_PROCEDURE genie_long_max_bits;
extern GENIE_PROCEDURE genie_long_max_int;
extern GENIE_PROCEDURE genie_long_max_real;
extern GENIE_PROCEDURE genie_long_min_real;
extern GENIE_PROCEDURE genie_long_next_random;
extern GENIE_PROCEDURE genie_long_real_width;
extern GENIE_PROCEDURE genie_long_small_real;
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
extern GENIE_PROCEDURE genie_min_real;
extern GENIE_PROCEDURE genie_minusab_complex;
extern GENIE_PROCEDURE genie_minusab_int;
extern GENIE_PROCEDURE genie_minusab_long_complex;
extern GENIE_PROCEDURE genie_minusab_long_int;
extern GENIE_PROCEDURE genie_minusab_long_mp;
extern GENIE_PROCEDURE genie_minusab_real;
extern GENIE_PROCEDURE genie_minus_complex;
extern GENIE_PROCEDURE genie_minus_int;
extern GENIE_PROCEDURE genie_minus_long_complex;
extern GENIE_PROCEDURE genie_minus_long_int;
extern GENIE_PROCEDURE genie_minus_long_mp;
extern GENIE_PROCEDURE genie_minus_real;
extern GENIE_PROCEDURE genie_modab_int;
extern GENIE_PROCEDURE genie_modab_long_mp;
extern GENIE_PROCEDURE genie_mod_int;
extern GENIE_PROCEDURE genie_mod_long_mp;
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
extern GENIE_PROCEDURE genie_new_sound;
extern GENIE_PROCEDURE genie_next_random;
extern GENIE_PROCEDURE genie_not_bits;
extern GENIE_PROCEDURE genie_not_bool;
extern GENIE_PROCEDURE genie_not_long_mp;
extern GENIE_PROCEDURE genie_null_char;
extern GENIE_PROCEDURE genie_odd_int;
extern GENIE_PROCEDURE genie_odd_long_mp;
extern GENIE_PROCEDURE genie_or_bits;
extern GENIE_PROCEDURE genie_or_bool;
extern GENIE_PROCEDURE genie_or_long_mp;
extern GENIE_PROCEDURE genie_overab_int;
extern GENIE_PROCEDURE genie_overab_long_mp;
extern GENIE_PROCEDURE genie_over_int;
extern GENIE_PROCEDURE genie_over_long_mp;
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
extern GENIE_PROCEDURE genie_pwd;
extern GENIE_PROCEDURE genie_real_lengths;
extern GENIE_PROCEDURE genie_real_shorths;
extern GENIE_PROCEDURE genie_real_width;
extern GENIE_PROCEDURE genie_re_complex;
extern GENIE_PROCEDURE genie_re_long_complex;
extern GENIE_PROCEDURE genie_repr_char;
extern GENIE_PROCEDURE genie_round_long_mp;
extern GENIE_PROCEDURE genie_round_real;
extern GENIE_PROCEDURE genie_set_bits;
extern GENIE_PROCEDURE genie_set_long_bits;
extern GENIE_PROCEDURE genie_set_longlong_bits;
extern GENIE_PROCEDURE genie_set_sound;
extern GENIE_PROCEDURE genie_shl_bits;
extern GENIE_PROCEDURE genie_shl_long_mp;
extern GENIE_PROCEDURE genie_shorten_bytes;
extern GENIE_PROCEDURE genie_shorten_long_complex_to_complex;
extern GENIE_PROCEDURE genie_shorten_longlong_complex_to_long_complex;
extern GENIE_PROCEDURE genie_shorten_longlong_mp_to_long_mp;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_bits;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_int;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_real;
extern GENIE_PROCEDURE genie_shr_bits;
extern GENIE_PROCEDURE genie_shr_long_mp;
extern GENIE_PROCEDURE genie_sign_int;
extern GENIE_PROCEDURE genie_sign_long_mp;
extern GENIE_PROCEDURE genie_sign_real;
extern GENIE_PROCEDURE genie_sinh_long_mp;
extern GENIE_PROCEDURE genie_sin_long_complex;
extern GENIE_PROCEDURE genie_sin_long_mp;
extern GENIE_PROCEDURE genie_sin_real;
extern GENIE_PROCEDURE genie_small_real;
extern GENIE_PROCEDURE genie_sort_row_string;
extern GENIE_PROCEDURE genie_sound_channels;
extern GENIE_PROCEDURE genie_sound_rate;
extern GENIE_PROCEDURE genie_sound_resolution;
extern GENIE_PROCEDURE genie_sound_samples;
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
extern GENIE_PROCEDURE genie_tanh_long_mp;
extern GENIE_PROCEDURE genie_tan_long_complex;
extern GENIE_PROCEDURE genie_tan_long_mp;
extern GENIE_PROCEDURE genie_tan_real;
extern GENIE_PROCEDURE genie_term;
extern GENIE_PROCEDURE genie_timesab_complex;
extern GENIE_PROCEDURE genie_timesab_int;
extern GENIE_PROCEDURE genie_timesab_long_complex;
extern GENIE_PROCEDURE genie_timesab_long_int;
extern GENIE_PROCEDURE genie_timesab_long_mp;
extern GENIE_PROCEDURE genie_timesab_real;
extern GENIE_PROCEDURE genie_timesab_string;
extern GENIE_PROCEDURE genie_times_char_int;
extern GENIE_PROCEDURE genie_times_int_char;
extern GENIE_PROCEDURE genie_times_int_string;
extern GENIE_PROCEDURE genie_times_string_int;
extern GENIE_PROCEDURE genie_to_lower;
extern GENIE_PROCEDURE genie_to_upper;
extern GENIE_PROCEDURE genie_unimplemented;
extern GENIE_PROCEDURE genie_vector_times_scalar;
extern GENIE_PROCEDURE genie_xor_bits;
extern GENIE_PROCEDURE genie_xor_bool;
extern GENIE_PROCEDURE genie_xor_long_mp;

extern PROPAGATOR_T genie_and_function (NODE_T *p);
extern PROPAGATOR_T genie_assertion (NODE_T *p);
extern PROPAGATOR_T genie_assignation_constant (NODE_T *p);
extern PROPAGATOR_T genie_assignation (NODE_T *p);
extern PROPAGATOR_T genie_call (NODE_T *p);
extern PROPAGATOR_T genie_cast (NODE_T *p);
extern PROPAGATOR_T genie_closed (volatile NODE_T *p);
extern PROPAGATOR_T genie_coercion (NODE_T *p);
extern PROPAGATOR_T genie_collateral (NODE_T *p);
extern PROPAGATOR_T genie_column_function (NODE_T *p);
extern PROPAGATOR_T genie_conditional (volatile NODE_T *p);
extern PROPAGATOR_T genie_constant (NODE_T *p);
extern PROPAGATOR_T genie_denotation (NODE_T *p);
extern PROPAGATOR_T genie_deproceduring (NODE_T *p);
extern PROPAGATOR_T genie_dereference_frame_identifier (NODE_T *p);
extern PROPAGATOR_T genie_dereference_generic_identifier (NODE_T *p);
extern PROPAGATOR_T genie_dereference_selection_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_dereference_slice_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_dereferencing (NODE_T *p);
extern PROPAGATOR_T genie_dereferencing_quick (NODE_T *p);
extern PROPAGATOR_T genie_diagonal_function (NODE_T *p);
extern PROPAGATOR_T genie_dyadic (NODE_T *p);
extern PROPAGATOR_T genie_dyadic_quick (NODE_T *p);
extern PROPAGATOR_T genie_enclosed (volatile NODE_T *p);
extern PROPAGATOR_T genie_field_selection (NODE_T *p);
extern PROPAGATOR_T genie_format_text (NODE_T *p);
extern PROPAGATOR_T genie_formula (NODE_T *p);
extern PROPAGATOR_T genie_generator (NODE_T *p);
extern PROPAGATOR_T genie_identifier (NODE_T *p);
extern PROPAGATOR_T genie_identifier_standenv (NODE_T *p);
extern PROPAGATOR_T genie_identifier_standenv_proc (NODE_T *p);
extern PROPAGATOR_T genie_identity_relation (NODE_T *p);
extern PROPAGATOR_T genie_int_case (volatile NODE_T *p);
extern PROPAGATOR_T genie_frame_identifier (NODE_T *p);
extern PROPAGATOR_T genie_loop (volatile NODE_T *p);
extern PROPAGATOR_T genie_monadic (NODE_T *p);
extern PROPAGATOR_T genie_nihil (NODE_T *p);
extern PROPAGATOR_T genie_or_function (NODE_T *p);
extern PROPAGATOR_T genie_routine_text (NODE_T *p);
extern PROPAGATOR_T genie_row_function (NODE_T *p);
extern PROPAGATOR_T genie_rowing (NODE_T *p);
extern PROPAGATOR_T genie_rowing_ref_row_of_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_ref_row_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_row_of_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_row_row (NODE_T *p);
extern PROPAGATOR_T genie_selection_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_selection (NODE_T *p);
extern PROPAGATOR_T genie_selection_value_quick (NODE_T *p);
extern PROPAGATOR_T genie_skip (NODE_T *p);
extern PROPAGATOR_T genie_slice_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_slice (NODE_T *p);
extern PROPAGATOR_T genie_transpose_function (NODE_T *p);
extern PROPAGATOR_T genie_united_case (volatile NODE_T *p);
extern PROPAGATOR_T genie_uniting (NODE_T *p);
extern PROPAGATOR_T genie_unit (NODE_T *p);
extern PROPAGATOR_T genie_voiding_assignation_constant (NODE_T *p);
extern PROPAGATOR_T genie_voiding_assignation (NODE_T *p);
extern PROPAGATOR_T genie_voiding (NODE_T *p);
extern PROPAGATOR_T genie_widening_int_to_real (NODE_T *p);
extern PROPAGATOR_T genie_widening (NODE_T *p);

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
extern BOOL_T close_device (NODE_T *, A68_FILE *);
extern BOOL_T genie_int_case_unit (NODE_T *, int, int *);
extern BOOL_T genie_no_user_symbols (SYMBOL_TABLE_T *);
extern BOOL_T genie_string_to_value_internal (NODE_T *, MOID_T *, char *, BYTE_T *);
extern BOOL_T genie_united_case_unit (NODE_T *, MOID_T *);
extern char *a_to_c_string (NODE_T *, char *, A68_REF);
extern char *genie_standard_format (NODE_T *, MOID_T *, void *);
extern double inverf (double);
extern double inverfc (double);
extern double rng_53_bit (void);
extern int a68_string_size (NODE_T *, A68_REF);
extern int iabs (int);
extern int isign (int);
extern NODE_T *last_unit;
extern void change_breakpoints (NODE_T *, unsigned, int, BOOL_T *, char *);
extern void change_masks (NODE_T *, unsigned, BOOL_T);
extern void dump_frame (int);
extern void dump_stack (int);
extern void exit_genie (NODE_T *, int);
extern void free_genie_heap (NODE_T *);
extern void genie_call_operator (NODE_T *, ADDR_T);
extern void genie_call_procedure (NODE_T *, MOID_T *, MOID_T *, MOID_T *, A68_PROCEDURE *, ADDR_T, ADDR_T);
extern void genie_check_initialisation (NODE_T *, BYTE_T *, MOID_T *);
extern void genie_copy_sound (NODE_T *, BYTE_T *, BYTE_T *);
extern void genie_declaration (NODE_T *);
extern void genie_dump_frames (void);
extern void genie_enquiry_clause (NODE_T *);
extern void genie_f_and_becomes (NODE_T *, MOID_T *, GENIE_PROCEDURE *);
extern void genie_generator_bounds (NODE_T *);
extern void genie_generator_internal (NODE_T *, MOID_T *, TAG_T *, LEAP_T, ADDR_T);
extern void genie_init_lib (NODE_T *);
extern void genie (void *);
extern void genie_prepare_declarer (NODE_T *);
extern void genie_push_undefined (NODE_T *, MOID_T *);
extern void genie_serial_clause (NODE_T *, jmp_buf *);
extern void genie_serial_units (NODE_T *, NODE_T **, jmp_buf *, int);
extern void genie_subscript_linear (NODE_T *, ADDR_T *, int *);
extern void genie_subscript (NODE_T *, A68_TUPLE **, int *, NODE_T **);
extern void get_global_pointer (NODE_T *);
extern void initialise_frame (NODE_T *);
extern void init_rng (unsigned long);
extern void install_signal_handlers (void);
extern void show_data_item (FILE_T, NODE_T *, MOID_T *, BYTE_T *);
extern void show_frame (NODE_T *, FILE_T);
extern void single_step (NODE_T *, unsigned);
extern void stack_dump (FILE_T, ADDR_T, int, int *);
extern void stack_dump_light (ADDR_T);
extern void un_init_frame (NODE_T *);

extern void genie_argc (NODE_T *);
extern void genie_argv (NODE_T *);
extern void genie_create_pipe (NODE_T *);
extern void genie_utctime (NODE_T *);
extern void genie_localtime (NODE_T *);
extern void genie_errno (NODE_T *);
extern void genie_execve (NODE_T *);
extern void genie_execve_child (NODE_T *);
extern void genie_execve_child_pipe (NODE_T *);
extern void genie_execve_output (NODE_T *);
extern void genie_fork (NODE_T *);
extern void genie_getenv (NODE_T *);
extern void genie_reset_errno (NODE_T *);
extern void genie_strerror (NODE_T *);
extern void genie_waitpid (NODE_T *);

#ifdef __S_IFIFO
extern GENIE_PROCEDURE genie_file_is_fifo;
#endif

#ifdef __S_IFLNK
extern GENIE_PROCEDURE genie_file_is_link;
#endif

#if defined ENABLE_PAR_CLAUSE
extern pthread_t main_thread_id;

extern BOOL_T whether_main_thread (void);
extern GENIE_PROCEDURE genie_down_sema;
extern GENIE_PROCEDURE genie_level_int_sema;
extern GENIE_PROCEDURE genie_level_sema_int;
extern GENIE_PROCEDURE genie_up_sema;
extern PROPAGATOR_T genie_parallel (NODE_T *p);
extern void count_par_clauses (NODE_T *);
extern void genie_abend_all_threads (NODE_T *, jmp_buf *, NODE_T *);
extern void genie_set_exit_from_threads (int);
#endif

#if defined ENABLE_HTTP
extern void genie_http_content (NODE_T *);
extern void genie_tcp_request (NODE_T *);
#endif

#if defined ENABLE_REGEX
extern void genie_grep_in_string (NODE_T *);
extern void genie_sub_in_string (NODE_T *);
#endif

#if defined ENABLE_CURSES
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

#if defined ENABLE_POSTGRESQL
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

/* Library for code generator */

extern int a68g_mod_int (int, int);
extern int a68g_entier (double);
extern A68_REF * a68g_plusab_real (A68_REF *, double);
extern A68_REF * a68g_minusab_real (A68_REF *, double);
extern A68_REF * a68g_timesab_real (A68_REF *, double);
extern A68_REF * a68g_divab_real (A68_REF *, double);
extern A68_REF * a68g_plusab_int (A68_REF *, int);
extern A68_REF * a68g_minusab_int (A68_REF *, int);
extern A68_REF * a68g_timesab_int (A68_REF *, int);
extern A68_REF * a68g_overab_int (A68_REF *, int);
extern double a68g_pow_real_int (double, int);
extern double a68g_pow_real (double, double);
extern double a68g_re_complex (A68_REAL *);
extern double a68g_im_complex (A68_REAL *);
extern double a68g_abs_complex (A68_REAL *);
extern double a68g_arg_complex (A68_REAL *);
extern void a68g_i_complex (A68_REAL *, double, double);
extern void a68g_minus_complex (A68_REAL *, A68_REAL *);
extern void a68g_conj_complex (A68_REAL *, A68_REAL *);
extern void a68g_add_complex (A68_REAL *, A68_REAL *, A68_REAL *);
extern void a68g_sub_complex (A68_REAL *, A68_REAL *, A68_REAL *);
extern void a68g_mul_complex (A68_REAL *, A68_REAL *, A68_REAL *);
extern void a68g_div_complex (A68_REAL *, A68_REAL *, A68_REAL *);
extern void a68g_sqrt_complex (A68_REAL *, A68_REAL *);
extern void a68g_exp_complex (A68_REAL *, A68_REAL *);
extern void a68g_ln_complex (A68_REAL *, A68_REAL *);
extern void a68g_sin_complex (A68_REAL *, A68_REAL *);
extern void a68g_cos_complex (A68_REAL *, A68_REAL *);
extern BOOL_T a68g_eq_complex (A68_REAL *, A68_REAL *);
extern BOOL_T a68g_ne_complex (A68_REAL *, A68_REAL *);

/* Transput related definitions */

#define TRANSPUT_BUFFER_SIZE BUFFER_SIZE
#define ITEM_NOT_USED (-1)
#define EMBEDDED_FORMAT A68_TRUE
#define NOT_EMBEDDED_FORMAT A68_FALSE
#define WANT_PATTERN A68_TRUE
#define SKIP_PATTERN A68_FALSE

#define IS_NIL_FORMAT(f) ((BOOL_T) (BODY (f) == NULL && ENVIRON (f) == 0))
#define NON_TERM(p) (find_non_terminal (top_non_terminal, ATTRIBUTE (p)))

#undef DEBUG

#define DIGIT_NORMAL ((unsigned) 0x1)
#define DIGIT_BLANK ((unsigned) 0x2)

#define INSERTION_NORMAL ((unsigned) 0x10)
#define INSERTION_BLANK ((unsigned) 0x20)

/* Some OS's open only 64 files. */
#define MAX_TRANSPUT_BUFFER 64

enum
{ INPUT_BUFFER = 0, OUTPUT_BUFFER, EDIT_BUFFER, UNFORMATTED_BUFFER, 
  FORMATTED_BUFFER, DOMAIN_BUFFER, PATH_BUFFER, REQUEST_BUFFER, 
  CONTENT_BUFFER, STRING_BUFFER, PATTERN_BUFFER, REPLACE_BUFFER, 
  READLINE_BUFFER, FIXED_TRANSPUT_BUFFERS
};

extern A68_REF stand_in, stand_out, skip_file;
extern A68_CHANNEL stand_in_channel, stand_out_channel, stand_draw_channel, stand_back_channel, stand_error_channel, associate_channel, skip_channel;

extern GENIE_PROCEDURE genie_associate;
extern GENIE_PROCEDURE genie_backspace;
extern GENIE_PROCEDURE genie_bin_possible;
extern GENIE_PROCEDURE genie_blank_char;
extern GENIE_PROCEDURE genie_close;
extern GENIE_PROCEDURE genie_compressible;
extern GENIE_PROCEDURE genie_create;
extern GENIE_PROCEDURE genie_draw_possible;
extern GENIE_PROCEDURE genie_erase;
extern GENIE_PROCEDURE genie_real;
extern GENIE_PROCEDURE genie_eof;
extern GENIE_PROCEDURE genie_eoln;
extern GENIE_PROCEDURE genie_error_char;
extern GENIE_PROCEDURE genie_establish;
extern GENIE_PROCEDURE genie_exp_char;
extern GENIE_PROCEDURE genie_fixed;
extern GENIE_PROCEDURE genie_flip_char;
extern GENIE_PROCEDURE genie_float;
extern GENIE_PROCEDURE genie_flop_char;
extern GENIE_PROCEDURE genie_formfeed_char;
extern GENIE_PROCEDURE genie_get_possible;
extern GENIE_PROCEDURE genie_init_transput;
extern GENIE_PROCEDURE genie_lock;
extern GENIE_PROCEDURE genie_new_line;
extern GENIE_PROCEDURE genie_new_page;
extern GENIE_PROCEDURE genie_newline_char;
extern GENIE_PROCEDURE genie_on_file_end;
extern GENIE_PROCEDURE genie_on_format_end;
extern GENIE_PROCEDURE genie_on_format_error;
extern GENIE_PROCEDURE genie_on_line_end;
extern GENIE_PROCEDURE genie_on_open_error;
extern GENIE_PROCEDURE genie_on_page_end;
extern GENIE_PROCEDURE genie_on_transput_error;
extern GENIE_PROCEDURE genie_on_value_error;
extern GENIE_PROCEDURE genie_open;
extern GENIE_PROCEDURE genie_print_bits;
extern GENIE_PROCEDURE genie_print_bool;
extern GENIE_PROCEDURE genie_print_char;
extern GENIE_PROCEDURE genie_print_complex;
extern GENIE_PROCEDURE genie_print_int;
extern GENIE_PROCEDURE genie_print_long_bits;
extern GENIE_PROCEDURE genie_print_long_complex;
extern GENIE_PROCEDURE genie_print_long_int;
extern GENIE_PROCEDURE genie_print_long_real;
extern GENIE_PROCEDURE genie_print_longlong_bits;
extern GENIE_PROCEDURE genie_print_longlong_complex;
extern GENIE_PROCEDURE genie_print_longlong_int;
extern GENIE_PROCEDURE genie_print_longlong_real;
extern GENIE_PROCEDURE genie_print_real;
extern GENIE_PROCEDURE genie_print_string;
extern GENIE_PROCEDURE genie_put_possible;
extern GENIE_PROCEDURE genie_read;
extern GENIE_PROCEDURE genie_read_bin;
extern GENIE_PROCEDURE genie_read_bin_file;
extern GENIE_PROCEDURE genie_read_bits;
extern GENIE_PROCEDURE genie_read_bool;
extern GENIE_PROCEDURE genie_read_char;
extern GENIE_PROCEDURE genie_read_complex;
extern GENIE_PROCEDURE genie_read_file;
extern GENIE_PROCEDURE genie_read_file_format;
extern GENIE_PROCEDURE genie_read_format;
extern GENIE_PROCEDURE genie_read_int;
extern GENIE_PROCEDURE genie_read_long_bits;
extern GENIE_PROCEDURE genie_read_long_complex;
extern GENIE_PROCEDURE genie_read_long_int;
extern GENIE_PROCEDURE genie_read_long_real;
extern GENIE_PROCEDURE genie_read_longlong_bits;
extern GENIE_PROCEDURE genie_read_longlong_complex;
extern GENIE_PROCEDURE genie_read_longlong_int;
extern GENIE_PROCEDURE genie_read_longlong_real;
extern GENIE_PROCEDURE genie_read_real;
extern GENIE_PROCEDURE genie_read_string;
extern GENIE_PROCEDURE genie_reidf_possible;
extern GENIE_PROCEDURE genie_reset;
extern GENIE_PROCEDURE genie_reset_possible;
extern GENIE_PROCEDURE genie_set;
extern GENIE_PROCEDURE genie_set_possible;
extern GENIE_PROCEDURE genie_space;
extern GENIE_PROCEDURE genie_tab_char;
extern GENIE_PROCEDURE genie_whole;
extern GENIE_PROCEDURE genie_write;
extern GENIE_PROCEDURE genie_write_bin;
extern GENIE_PROCEDURE genie_write_bin_file;
extern GENIE_PROCEDURE genie_write_file;
extern GENIE_PROCEDURE genie_write_file_format;
extern GENIE_PROCEDURE genie_write_format;

#if defined ENABLE_GRAPHICS
extern GENIE_PROCEDURE genie_draw_aspect;
extern GENIE_PROCEDURE genie_draw_atom;
extern GENIE_PROCEDURE genie_draw_background_colour;
extern GENIE_PROCEDURE genie_draw_background_colour_name;
extern GENIE_PROCEDURE genie_draw_circle;
extern GENIE_PROCEDURE genie_draw_clear;
extern GENIE_PROCEDURE genie_draw_colour;
extern GENIE_PROCEDURE genie_draw_colour_name;
extern GENIE_PROCEDURE genie_draw_fillstyle;
extern GENIE_PROCEDURE genie_draw_fontname;
extern GENIE_PROCEDURE genie_draw_fontsize;
extern GENIE_PROCEDURE genie_draw_get_colour_name;
extern GENIE_PROCEDURE genie_draw_line;
extern GENIE_PROCEDURE genie_draw_linestyle;
extern GENIE_PROCEDURE genie_draw_linewidth;
extern GENIE_PROCEDURE genie_draw_move;
extern GENIE_PROCEDURE genie_draw_point;
extern GENIE_PROCEDURE genie_draw_rect;
extern GENIE_PROCEDURE genie_draw_show;
extern GENIE_PROCEDURE genie_draw_star;
extern GENIE_PROCEDURE genie_draw_text;
extern GENIE_PROCEDURE genie_draw_textangle;
extern GENIE_PROCEDURE genie_make_device;
#endif

extern FILE_T open_physical_file (NODE_T *, A68_REF, int, mode_t);
extern NODE_T *get_next_format_pattern (NODE_T *, A68_REF, BOOL_T);
extern char *error_chars (char *, int);
extern char *fixed (NODE_T * p);
extern char *fleet (NODE_T * p);
extern char *get_transput_buffer (int);
extern char *stack_string (NODE_T *, int);
extern char *sub_fixed (NODE_T *, double, int, int);
extern char *sub_whole (NODE_T *, int, int);
extern char *whole (NODE_T * p);
extern char get_flip_char (void);
extern char get_flop_char (void);
extern char pop_char_transput_buffer (int);
extern int char_scanner (A68_FILE *);
extern int end_of_format (NODE_T *, A68_REF);
extern int get_replicator_value (NODE_T *, BOOL_T);
extern int get_transput_buffer_index (int);
extern int get_transput_buffer_length (int);
extern int get_transput_buffer_size (int);
extern int get_unblocked_transput_buffer (NODE_T *);
extern void add_a_string_transput_buffer (NODE_T *, int, BYTE_T *);
extern void add_char_transput_buffer (NODE_T *, int, char);
extern void add_string_from_stack_transput_buffer (NODE_T *, int);
extern void add_string_transput_buffer (NODE_T *, int, char *);
extern void end_of_file_error (NODE_T * p, A68_REF ref_file);
extern void enlarge_transput_buffer (NODE_T *, int, int);
extern void format_error (NODE_T *, A68_REF, char *);
extern void genie_dns_addr (NODE_T *, MOID_T *, BYTE_T *, ADDR_T, char *);
extern void genie_read_standard (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_string_to_value (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_value_to_string (NODE_T *, MOID_T *, BYTE_T *, int);
extern void genie_write_standard (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_write_string_from_stack (NODE_T * p, A68_REF);
extern void on_event_handler (NODE_T *, A68_PROCEDURE, A68_REF);
extern void open_error (NODE_T *, A68_REF, char *);
extern void pattern_error (NODE_T *, MOID_T *, int);
extern void read_insertion (NODE_T *, A68_REF);
extern void reset_transput_buffer (int);
extern void set_default_mended_procedure (A68_PROCEDURE *);
extern void set_default_mended_procedures (A68_FILE *);
extern void set_transput_buffer_index (int, int);
extern void set_transput_buffer_length (int, int);
extern void set_transput_buffer_size (int, int);
extern void standardise (double *, int, int, int *);
extern void transput_error (NODE_T *, A68_REF, MOID_T *);
extern void unchar_scanner (NODE_T *, A68_FILE *, char);
extern void value_error (NODE_T *, MOID_T *, A68_REF);
extern void write_insertion (NODE_T *, A68_REF, unsigned);
extern void write_purge_buffer (NODE_T *, A68_REF, int);
extern void read_sound (NODE_T *, A68_REF, A68_SOUND *);
extern void write_sound (NODE_T *, A68_REF, A68_SOUND *);

#endif
