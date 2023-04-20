//! @file single-torrix.c
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
//! REAL vector and matrix support.

#include "a68g.h"
#include "a68g-torrix.h"

//! @brief Push description for diagonal of square matrix.

PROP_T genie_diagonal_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  BOOL_T name = (BOOL_T) (IS_REF (MOID (p)));
  int k = 0;
  if (IS (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  MOID_T *m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  ADDR_T scope = PRIMAL_SCOPE;
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  A68_ROW row; A68_ARRAY *arr; A68_TUPLE *tup1, *tup2;
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
  A68_ROW new_row = heap_generator (p, m, DESCRIPTOR_SIZE (1));
  A68_ARRAY new_arr;
  DIM (&new_arr) = 1;
  MOID (&new_arr) = m;
  ELEM_SIZE (&new_arr) = ELEM_SIZE (arr);
  SLICE_OFFSET (&new_arr) = SLICE_OFFSET (arr);
  FIELD_OFFSET (&new_arr) = FIELD_OFFSET (arr);
  ARRAY (&new_arr) = ARRAY (arr);
  A68_TUPLE new_tup;
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
  PROP_T self;
  UNIT (&self) = genie_diagonal_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push description for transpose of matrix.

PROP_T genie_transpose_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  BOOL_T name = (BOOL_T) (IS_REF (MOID (p)));
  MOID_T *m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  EXECUTE_UNIT (NEXT (q));
  ADDR_T scope = PRIMAL_SCOPE;
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  A68_ROW row; A68_ARRAY *arr; A68_TUPLE *tup1, *tup2;
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  A68_ROW new_row = heap_generator (p, m, DESCRIPTOR_SIZE (2));
  A68_ARRAY new_arr = *arr;
  A68_TUPLE new_tup1 = *tup2, new_tup2 = *tup1;
  PUT_DESCRIPTOR2 (new_arr, new_tup1, new_tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), A68_REF_SIZE);
    *DEREF (A68_REF, &ref_new) = new_row;
    REF_SCOPE (&ref_new) = scope;
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  PROP_T self;
  UNIT (&self) = genie_transpose_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push description for row vector.

PROP_T genie_row_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  BOOL_T name = (BOOL_T) (IS_REF (MOID (p)));
  int k = 1;
  if (IS (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  MOID_T *m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  A68_ROW row; A68_ARRAY *arr; A68_TUPLE *tup;
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  if (DIM (arr) != 1) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_NO_VECTOR, m, PRIMARY);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB_MOID (p) : MOID (p));
  A68_ROW new_row = heap_generator (p, m, DESCRIPTOR_SIZE (2));
  A68_ARRAY new_arr;
  DIM (&new_arr) = 2;
  MOID (&new_arr) = m;
  ELEM_SIZE (&new_arr) = ELEM_SIZE (arr);
  SLICE_OFFSET (&new_arr) = SLICE_OFFSET (arr);
  FIELD_OFFSET (&new_arr) = FIELD_OFFSET (arr);
  ARRAY (&new_arr) = ARRAY (arr);
  A68_TUPLE tup1, tup2;
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
  PROP_T self;
  UNIT (&self) = genie_row_function;
  SOURCE (&self) = p;
  return self;
}

//! @brief Push description for column vector.

PROP_T genie_column_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  BOOL_T name = (BOOL_T) (IS_REF (MOID (p)));
  ADDR_T scope = PRIMAL_SCOPE;
  int k = 1;
  if (IS (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  MOID_T *m = (name ? SUB_MOID (NEXT (q)) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  A68_ROW row; A68_ARRAY *arr; A68_TUPLE *tup;
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  m = (name ? SUB_MOID (p) : MOID (p));
  A68_ROW new_row = heap_generator (p, m, DESCRIPTOR_SIZE (2));
  A68_ARRAY new_arr;
  DIM (&new_arr) = 2;
  MOID (&new_arr) = m;
  ELEM_SIZE (&new_arr) = ELEM_SIZE (arr);
  SLICE_OFFSET (&new_arr) = SLICE_OFFSET (arr);
  FIELD_OFFSET (&new_arr) = FIELD_OFFSET (arr);
  ARRAY (&new_arr) = ARRAY (arr);
  A68_TUPLE tup1, tup2;
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
  PROP_T self;
  UNIT (&self) = genie_column_function;
  SOURCE (&self) = p;
  return self;
}
