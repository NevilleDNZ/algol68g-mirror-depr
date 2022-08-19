//! @file a68g-genie.h
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

#if !defined (__A68G_GENIE_H__)
#define __A68G_GENIE_H__

//! @brief PROC VOID gc heap

// Prelude errors can also occur in the constant folder

#define CHECK_INT_SHORTEN(p, i)\
  PRELUDE_ERROR (((i) > INT_MAX || (i) < -INT_MAX), p, ERROR_MATH, M_INT)

#define CHECK_INT_ADDITION(p, i, j)\
  PRELUDE_ERROR (\
    ((j) > 0 && (i) > (A68_MAX_INT - (j))) || ((j) < 0 && (i) < (-A68_MAX_INT - (j))),\
    p, "M overflow", M_INT)

#define CHECK_INT_MULTIPLICATION(p, i, j)\
  PRELUDE_ERROR (\
    (j) != 0 && ABS (i) > A68_MAX_INT / ABS (j),\
    p, "M overflow", M_INT)

#define CHECK_BITS_ADDITION(p, i, j)\
  if (!MODULAR_MATH (p)) {\
    PRELUDE_ERROR (((i) > (A68_MAX_BITS - (j))), p, ERROR_MATH, M_BITS);\
  }

#define CHECK_BITS_SUBTRACTION(p, i, j)\
  if (!MODULAR_MATH (p)) {\
    PRELUDE_ERROR (((j) > (i)), p, ERROR_MATH, M_BITS);\
  }

#define CHECK_BITS_MULTIPLICATION(p, i, j)\
  if (!MODULAR_MATH (p)) {\
    PRELUDE_ERROR ((j) != 0 && (i) > A68_MAX_BITS / (j), p, ERROR_MATH, M_BITS);\
  }

#define CHECK_INT_DIVISION(p, i, j)\
  PRELUDE_ERROR ((j) == 0, p, ERROR_DIVISION_BY_ZERO, M_INT)

#define PRELUDE_ERROR(cond, p, txt, add)\
  if (cond) {\
    if (A68 (in_execution)) {\
      diagnostic (A68_RUNTIME_ERROR, p, txt, add);\
      exit_genie (p, A68_RUNTIME_ERROR);\
    } else {\
      diagnostic (A68_MATH_ERROR, p, txt, add);\
    }}

// Check on a NIL name

#define CHECK_REF(p, z, m)\
  if (! INITIALISED (&z)) {\
    diagnostic (A68_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE_FROM, (m));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  } else if (IS_NIL (z)) {\
    diagnostic (A68_RUNTIME_ERROR, (p), ERROR_ACCESSING_NIL, (m));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }

// Macros for row-handling

#define DESCRIPTOR_SIZE(n) (SIZE_ALIGNED (A68_ARRAY) + (n) * SIZE_ALIGNED (A68_TUPLE))

#define NEW_ROW_1D(des, row, arr, tup, row_m, mod, upb)\
  (des) = heap_generator (p, (row_m), DESCRIPTOR_SIZE (1));\
  (row) = heap_generator (p, (row_m), (upb) * SIZE (mod));\
  DIM (&(arr)) = 1;\
  MOID (&(arr)) = (mod);\
  ELEM_SIZE (&(arr)) = SIZE (mod);\
  SLICE_OFFSET (&(arr)) = 0;\
  FIELD_OFFSET (&(arr)) = 0;\
  ARRAY (&(arr)) = (row);\
  LWB (&(tup)) = 1;\
  UPB (&(tup)) = (upb);\
  SHIFT (&(tup)) = LWB (&(tup));\
  SPAN (&(tup)) = 1;\
  K (&(tup)) = 0;\
  PUT_DESCRIPTOR ((arr), (tup), &(des));

#define GET_DESCRIPTOR(a, t, p)\
  a = (A68_ARRAY *) ARRAY_ADDRESS (p);\
  t = (A68_TUPLE *) & (((BYTE_T *) (a)) [SIZE_ALIGNED (A68_ARRAY)]);

#define GET_DESCRIPTOR2(a, t1, t2, p)\
  a = (A68_ARRAY *) ARRAY_ADDRESS (p);\
  t1 = (A68_TUPLE *) & (((BYTE_T *) (a)) [SIZE_ALIGNED (A68_ARRAY)]);\
  t2 = (A68_TUPLE *) & (((BYTE_T *) (a)) [SIZE_ALIGNED (A68_ARRAY) + sizeof (A68_TUPLE)]);

#define PUT_DESCRIPTOR(a, t1, p) {\
  BYTE_T *a_p = ARRAY_ADDRESS (p);\
  *(A68_ARRAY *) a_p = (a);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [SIZE_ALIGNED (A68_ARRAY)]) = (t1);\
  }

#define PUT_DESCRIPTOR2(a, t1, t2, p) {\
  BYTE_T *a_p = ARRAY_ADDRESS (p);\
  *(A68_ARRAY *) a_p = (a);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [SIZE_ALIGNED (A68_ARRAY)]) = (t1);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [SIZE_ALIGNED (A68_ARRAY) + sizeof (A68_TUPLE)]) = (t2);\
  }

#define ROW_SIZE(t) ((UPB (t) >= LWB (t)) ? (UPB (t) - LWB (t) + 1) : 0)
#define ROW_ELEMENT(a, k) (((ADDR_T) k + SLICE_OFFSET (a)) * ELEM_SIZE (a) + FIELD_OFFSET (a))
#define INDEX_1_DIM(a, t, k) ROW_ELEMENT (a, (SPAN (t) * (int) (k) - SHIFT (t)))

#define VECTOR_OFFSET(a, t)\
  ((LWB (t) * SPAN (t) - SHIFT (t) + SLICE_OFFSET (a)) * ELEM_SIZE (a) + FIELD_OFFSET (a))

#define MATRIX_OFFSET(a, t1, t2)\
  ((LWB (t1) * SPAN (t1) - SHIFT (t1) + LWB (t2) * SPAN (t2) - SHIFT (t2) + SLICE_OFFSET (a)) * ELEM_SIZE (a) + FIELD_OFFSET (a))

// Execution

#define EXECUTE_UNIT_2(p, dest) {\
  PROP_T *_prop_ = &GPROP (p);\
  A68 (f_entry) = p;\
  dest = (*(UNIT (_prop_))) (SOURCE (_prop_));}

#define EXECUTE_UNIT(p) {\
  PROP_T *_prop_ = &GPROP (p);\
  A68 (f_entry) = p;\
  (void) (*(UNIT (_prop_))) (SOURCE (_prop_));}

#define EXECUTE_UNIT_TRACE(p) {\
  if (STATUS_TEST (p, (BREAKPOINT_MASK | BREAKPOINT_TEMPORARY_MASK | \
      BREAKPOINT_INTERRUPT_MASK | BREAKPOINT_WATCH_MASK | BREAKPOINT_TRACE_MASK))) {\
    single_step ((p), STATUS (p));\
  }\
  EXECUTE_UNIT (p);}

// Stuff for the garbage collector

// Check whether the heap fills

#define DEFAULT_PREEMPTIVE 0.8

// Save a handle from the GC

#define BLOCK_GC_HANDLE(z) {\
  if (IS_IN_HEAP (z)) {\
    STATUS_SET (REF_HANDLE(z), BLOCK_GC_MASK);\
  }}

#define UNBLOCK_GC_HANDLE(z) {\
  if (IS_IN_HEAP (z)) {\
    STATUS_CLEAR (REF_HANDLE (z), BLOCK_GC_MASK);\
  }}

// Tests for objects of mode INT

#define CHECK_INDEX(p, k, t) {\
  if (VALUE (k) < LWB (t) || VALUE (k) > UPB (t)) {\
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }}

// Tests for objects of mode REAL

#if defined (HAVE_IEEE_754)
#define CHECK_REAL(p, u) PRELUDE_ERROR (!finite (u), p, ERROR_INFINITE, M_REAL)
#define CHECK_COMPLEX(p, u, v) PRELUDE_ERROR (!finite (u) || !finite (v), p, ERROR_INFINITE, M_COMPLEX)
#else
#define CHECK_REAL(p, u) {;}
#define CHECK_COMPLEX(p, u, v) {;}
#endif

#define MATH_RTE(p, z, m, t) PRELUDE_ERROR (z, (p), (t == NO_TEXT ? ERROR_MATH : t), (m))

// Macros.

#define C_FUNCTION(p, f)\
  A68 (f_entry) = p;\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  errno = 0;\
  VALUE (x) = f (VALUE (x));\
  MATH_RTE (p, errno != 0, M_REAL, NO_TEXT);

#define OWN_FUNCTION(p, f)\
  A68 (f_entry) = p;\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  errno = 0;\
  VALUE (x) = f (p, VALUE (x));\
  MATH_RTE (p, errno != 0, M_REAL, NO_TEXT);

// Macro's for standard environ

#define A68_ENV_INT(n, k) void n (NODE_T *p) {PUSH_PRIMAL (p, (k), INT);}
#define A68_ENV_REAL(n, z) void n (NODE_T *p) {PUSH_PRIMAL (p, (z), REAL);}

// Macros for the evaluation stack

#define INCREMENT_STACK_POINTER(err, i)\
  {A68_SP += (ADDR_T) A68_ALIGN (i); (void) (err);}

#define DECREMENT_STACK_POINTER(err, i)\
  {A68_SP -= A68_ALIGN (i); (void) (err);}

#define PUSH(p, addr, size) {\
  BYTE_T *_sp_ = STACK_TOP;\
  INCREMENT_STACK_POINTER ((p), (int) (size));\
  COPY (_sp_, (BYTE_T *) (addr), (int) (size));\
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
  DECREMENT_STACK_POINTER((p), (int) SIZE_ALIGNED (type));\
  (addr) = (type *) STACK_TOP;\
  }

#define POP_OPERAND_ADDRESS(p, i, type) {\
  (void) (p);\
  (i) = (type *) (STACK_OFFSET (-SIZE_ALIGNED (type)));\
  }

#define POP_OPERAND_ADDRESSES(p, i, j, type) {\
  DECREMENT_STACK_POINTER ((p), (int) SIZE_ALIGNED (type));\
  (j) = (type *) STACK_TOP;\
  (i) = (type *) (STACK_OFFSET (-SIZE_ALIGNED (type)));\
  }

#define POP_3_OPERAND_ADDRESSES(p, i, j, k, type) {\
  DECREMENT_STACK_POINTER ((p), (int) (2 * SIZE_ALIGNED (type)));\
  (k) = (type *) (STACK_OFFSET (SIZE_ALIGNED (type)));\
  (j) = (type *) STACK_TOP;\
  (i) = (type *) (STACK_OFFSET (-SIZE_ALIGNED (type)));\
  }

#define PUSH_VALUE(p, z, mode) {\
  mode *_x_ = (mode *) STACK_TOP;\
  STATUS (_x_) = INIT_MASK;\
  VALUE (_x_) = (z);\
  INCREMENT_STACK_POINTER ((p), SIZE_ALIGNED (mode));\
  }

#define PUSH_PRIMAL(p, z, m) {\
  A68_##m *_x_ = (A68_##m *) STACK_TOP;\
  int _size_ = SIZE_ALIGNED (A68_##m);\
  STATUS (_x_) = INIT_MASK;\
  VALUE (_x_) = (z);\
  INCREMENT_STACK_POINTER ((p), _size_);\
  }

#define PUSH_OBJECT(p, z, mode) {\
  *(mode *) STACK_TOP = (z);\
  INCREMENT_STACK_POINTER (p, SIZE_ALIGNED (mode));\
  }

#define POP_OBJECT(p, z, mode) {\
  DECREMENT_STACK_POINTER((p), SIZE_ALIGNED (mode));\
  (*(z)) = *((mode *) STACK_TOP);\
  }

#define PUSH_COMPLEX(p, re, im) {\
  PUSH_PRIMAL (p, re, REAL);\
  PUSH_PRIMAL (p, im, REAL);\
  }

#define POP_COMPLEX(p, re, im) {\
  POP_OBJECT (p, im, A68_REAL);\
  POP_OBJECT (p, re, A68_REAL);\
  }

#define PUSH_BYTES(p, k) {\
  A68_BYTES *_z_ = (A68_BYTES *) STACK_TOP;\
  STATUS (_z_) = INIT_MASK;\
  a68_memmove (VALUE (_z_), k, BYTES_WIDTH);\
  INCREMENT_STACK_POINTER((p), SIZE_ALIGNED (A68_BYTES));\
  }

#define PUSH_LONG_BYTES(p, k) {\
  A68_LONG_BYTES *_z_ = (A68_LONG_BYTES *) STACK_TOP;\
  STATUS (_z_) = INIT_MASK;\
  a68_memmove (VALUE (_z_), k, LONG_BYTES_WIDTH);\
  INCREMENT_STACK_POINTER((p), SIZE_ALIGNED (A68_LONG_BYTES));\
  }

#define PUSH_REF(p, z) PUSH_OBJECT (p, z, A68_REF)
#define PUSH_PROCEDURE(p, z) PUSH_OBJECT (p, z, A68_PROCEDURE)
#define PUSH_FORMAT(p, z) PUSH_OBJECT (p, z, A68_FORMAT)

#define POP_REF(p, z) POP_OBJECT (p, z, A68_REF)
#define POP_PROCEDURE(p, z) POP_OBJECT (p, z, A68_PROCEDURE)

#define PUSH_UNION(p, z) {\
  A68_UNION *_x_ = (A68_UNION *) STACK_TOP;\
  STATUS (_x_) = INIT_MASK;\
  VALUE (_x_) = (z);\
  INCREMENT_STACK_POINTER ((p), SIZE_ALIGNED (A68_UNION));\
  }

// Interpreter macros

#define INITIALISED(z)  ((BOOL_T) (STATUS (z) & INIT_MASK))
#define MODULAR_MATH(z) ((BOOL_T) (STATUS (z) & MODULAR_MASK))
#define LHS_MODE(p) (MOID (PACK (MOID (p))))
#define RHS_MODE(p) (MOID (NEXT (PACK (MOID (p)))))

// Transput related macros

#define IS_NIL_FORMAT(f) ((BOOL_T) (BODY (f) == NO_NODE && ENVIRON (f) == 0))

// Macros for check on initialisation of values

#define CHECK_INIT(p, c, q)\
  if (!(c)) {\
    diagnostic (A68_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE_FROM, (q));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }

#define CHECK_DNS2(p, scope, limit, mode)\
  if (scope > limit) {\
    char txt[BUFFER_SIZE];\
    ASSERT (snprintf (txt, SNPRINTF_SIZE, ERROR_SCOPE_DYNAMIC_1) >= 0);\
    diagnostic (A68_RUNTIME_ERROR, p, txt, mode);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }

#define CHECK_DNS(p, m, w, limit)\
  if (NEED_DNS (GINFO (p))) {\
    ADDR_T _lim = ((limit) < A68_GLOBALS ? A68_GLOBALS : (limit));\
    if (IS ((m), REF_SYMBOL)) {\
      CHECK_DNS2 (p, (REF_SCOPE ((A68_REF *) (w))), _lim, (m));\
    } else if (IS ((m), PROC_SYMBOL)) {\
      CHECK_DNS2 (p, ENVIRON ((A68_PROCEDURE *) (w)), _lim, (m));\
    } else if (IS ((m), FORMAT_SYMBOL)) {\
      CHECK_DNS2 (p, ENVIRON ((A68_FORMAT *) w), _lim, (m));\
  }}

// 
// The void * cast in next macro is to stop warnings about dropping a volatile
// qualifier to a pointer. This is safe here.

#define STACK_DNS(p, m, limit)\
  if (p != NO_NODE && GINFO (p) != NO_GINFO) {\
    CHECK_DNS ((NODE_T *)(void *)(p), (m),\
               (STACK_OFFSET (-SIZE (m))), (limit));\
  }

// Genie routines.

extern PROP_T genie_column_function (NODE_T *);
extern PROP_T genie_diagonal_function (NODE_T *);
extern PROP_T genie_row_function (NODE_T *);
extern PROP_T genie_transpose_function (NODE_T *);

extern PROP_T genie_and_function (NODE_T *);
extern PROP_T genie_assertion (NODE_T *);
extern PROP_T genie_assignation_constant (NODE_T *);
extern PROP_T genie_assignation (NODE_T *);
extern PROP_T genie_assignation_quick (NODE_T * p);
extern PROP_T genie_call (NODE_T *);
extern PROP_T genie_cast (NODE_T *);
extern PROP_T genie_closed (volatile NODE_T *);
extern PROP_T genie_coercion (NODE_T *);
extern PROP_T genie_collateral (NODE_T *);
extern PROP_T genie_conditional (volatile NODE_T *);
extern PROP_T genie_constant (NODE_T *);
extern PROP_T genie_denotation (NODE_T *);
extern PROP_T genie_deproceduring (NODE_T *);
extern PROP_T genie_dereference_frame_identifier (NODE_T *);
extern PROP_T genie_dereference_generic_identifier (NODE_T *);
extern PROP_T genie_dereference_selection_name_quick (NODE_T *);
extern PROP_T genie_dereference_slice_name_quick (NODE_T *);
extern PROP_T genie_dereferencing (NODE_T *);
extern PROP_T genie_dereferencing_quick (NODE_T *);
extern PROP_T genie_dyadic (NODE_T *);
extern PROP_T genie_dyadic_quick (NODE_T *);
extern PROP_T genie_enclosed (volatile NODE_T *);
extern PROP_T genie_field_selection (NODE_T *);
extern PROP_T genie_format_text (NODE_T *);
extern PROP_T genie_formula (NODE_T *);
extern PROP_T genie_frame_identifier (NODE_T *);
extern PROP_T genie_identifier (NODE_T *);
extern PROP_T genie_identifier_standenv (NODE_T *);
extern PROP_T genie_identifier_standenv_proc (NODE_T *);
extern PROP_T genie_identity_relation (NODE_T *);
extern PROP_T genie_int_case (volatile NODE_T *);
extern PROP_T genie_loop (volatile NODE_T *);
extern PROP_T genie_loop (volatile NODE_T *);
extern PROP_T genie_monadic (NODE_T *);
extern PROP_T genie_nihil (NODE_T *);
extern PROP_T genie_or_function (NODE_T *);
extern PROP_T genie_routine_text (NODE_T *);
extern PROP_T genie_rowing (NODE_T *);
extern PROP_T genie_rowing_ref_row_of_row (NODE_T *);
extern PROP_T genie_rowing_ref_row_row (NODE_T *);
extern PROP_T genie_rowing_row_of_row (NODE_T *);
extern PROP_T genie_rowing_row_row (NODE_T *);
extern PROP_T genie_selection_name_quick (NODE_T *);
extern PROP_T genie_selection (NODE_T *);
extern PROP_T genie_selection_value_quick (NODE_T *);
extern PROP_T genie_skip (NODE_T *);
extern PROP_T genie_slice_name_quick (NODE_T *);
extern PROP_T genie_slice (NODE_T *);
extern PROP_T genie_united_case (volatile NODE_T *);
extern PROP_T genie_uniting (NODE_T *);
extern PROP_T genie_unit (NODE_T *);
extern PROP_T genie_voiding_assignation_constant (NODE_T *);
extern PROP_T genie_voiding_assignation (NODE_T *);
extern PROP_T genie_voiding (NODE_T *);
extern PROP_T genie_widen_int_to_real (NODE_T *);
extern PROP_T genie_widen (NODE_T *);

extern A68_REF genie_clone (NODE_T *, MOID_T *, A68_REF *, A68_REF *);
extern A68_REF genie_make_ref_row_of_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
extern A68_REF genie_make_ref_row_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
extern A68_REF genie_make_rowrow (NODE_T *, MOID_T *, int, ADDR_T);

extern void genie_clone_stack (NODE_T *, MOID_T *, A68_REF *, A68_REF *);
extern void genie_serial_units_no_label (NODE_T *, ADDR_T, NODE_T **);

#endif
