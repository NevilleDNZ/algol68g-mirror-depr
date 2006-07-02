/*!
\file vector.c
\brief small vector library
*/

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


/*
This file is a small "layer" of vector routines that soften the slowness of the
interpreter. 
*/

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

#define TEST_LENGTH(a, b) {\
    if ((a) != (b)) {\
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);\
      exit_genie (p, A_RUNTIME_ERROR);\
    }\
  }

#define BASE(a, t) (((t)->lower_bound - (t)->shift) * (t)->span +\
  (a)->slice_offset) * (a)->elem_size + (a)->field_offset

/*!
\brief PROC ([] REAL, REAL) VOID vector set
\param p
**/

void genie_vector_set (NODE_T * p)
{
  A68_REF a;
  A68_REAL y;
  A68_ARRAY *arr_a;
  A68_TUPLE *tup_a;
  int l_a, k, inc_a, i;
  BYTE_T *b_a;
/* POP arguments. */
  POP (p, &y, SIZE_OF (A68_REAL));
  TEST_INIT (p, (y), MODE (REAL));
  POP (p, &a, SIZE_OF (A68_REF));
  TEST_INIT (p, a, MODE (REF_ROW_REAL));
  TEST_NIL (p, a, MODE (REF_ROW_REAL));
  a = *(A68_REF *) ADDRESS (&a);
  TEST_INIT (p, a, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_a, tup_a, &a);
  l_a = ROW_SIZE (tup_a);
/* Optimised loop for vector element access. */
  b_a = (BYTE_T *) ADDRESS (&arr_a->array);
  i = BASE (arr_a, tup_a);
  inc_a = tup_a->span * arr_a->elem_size;
  for (k = 0; k < l_a; k++) {
    A68_REAL *x = (A68_REAL *) (b_a + i);
    x->status = INITIALISED_MASK;
    x->value = y.value;
    i += inc_a;
  }
}

/*!
\brief PROC (REF [] REAL, [] REAL, REAL) VOID vector times scalar
\param p
**/

void genie_vector_times_scalar (NODE_T * p)
{
  A68_REF a, b;
  A68_REAL z;
  A68_ARRAY *arr_a, *arr_b;
  A68_TUPLE *tup_a, *tup_b;
  int l_a, l_b, m, inc_a, inc_b, i, j;
  BYTE_T *b_a, *b_b;
/* POP arguments. */
  POP (p, &z, SIZE_OF (A68_REAL));
  TEST_INIT (p, z, MODE (REAL));
  POP (p, &b, SIZE_OF (A68_REF));
  TEST_INIT (p, b, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_b, tup_b, &b);
  l_b = ROW_SIZE (tup_b);
  POP (p, &a, SIZE_OF (A68_REF));
  TEST_INIT (p, a, MODE (REF_ROW_REAL));
  TEST_NIL (p, a, MODE (REF_ROW_REAL));
  a = *(A68_REF *) ADDRESS (&a);
  TEST_INIT (p, a, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_a, tup_a, &a);
  l_a = ROW_SIZE (tup_a);
/* Test length. */
  TEST_LENGTH (l_a, l_b);
/* Optimised loop for vector element access. */
  b_b = (BYTE_T *) ADDRESS (&arr_b->array);
  b_a = (BYTE_T *) ADDRESS (&arr_a->array);
  j = BASE (arr_b, tup_b);
  i = BASE (arr_a, tup_a);
  inc_b = tup_b->span * arr_b->elem_size;
  inc_a = tup_a->span * arr_a->elem_size;
  for (m = 0; m < l_a; m++) {
    A68_REAL *y = (A68_REAL *) & (b_b[j]);
    A68_REAL *x = (A68_REAL *) & (b_a[i]);
    TEST_INIT (p, *y, MODE (REAL));
    x->status = INITIALISED_MASK;
    x->value = y->value * z.value;
    j += inc_b;
    i += inc_a;
  }
}

/*!
\brief PROC (REF [] REAL, [] REAL) VOID vector move
\param p
**/

void genie_vector_move (NODE_T * p)
{
/* 
Copies a vector into the other. This is faster than a := b since there is no
need to make a copy - this routine is explicitly destructive when source and
destination overlap. 
*/
  A68_REF a, b;
  A68_ARRAY *arr_a, *arr_b;
  A68_TUPLE *tup_a, *tup_b;
  int l_a, l_b, k, inc_a, inc_b;
  ADDR_T i, j;
  BYTE_T *b_a, *b_b;
/* POP arguments. */
  POP (p, &b, SIZE_OF (A68_REF));
  TEST_INIT (p, b, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_b, tup_b, &b);
  l_b = ROW_SIZE (tup_b);
  POP (p, &a, SIZE_OF (A68_REF));
  TEST_INIT (p, a, MODE (REF_ROW_REAL));
  TEST_NIL (p, a, MODE (REF_ROW_REAL));
  a = *(A68_REF *) ADDRESS (&a);
  TEST_INIT (p, a, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_a, tup_a, &a);
  l_a = ROW_SIZE (tup_a);
  TEST_LENGTH (l_a, l_b);
/* Optimised loop for vector element access. */
  b_b = (BYTE_T *) ADDRESS (&arr_b->array);
  b_a = (BYTE_T *) ADDRESS (&arr_a->array);
  j = BASE (arr_b, tup_b);
  i = BASE (arr_a, tup_a);
  inc_b = tup_b->span * arr_b->elem_size;
  inc_a = tup_a->span * arr_a->elem_size;
  for (k = 0; k < l_a; k++) {
    A68_REAL *y = (A68_REAL *) & (b_b[j]);
    A68_REAL *x = (A68_REAL *) & (b_a[i]);
    TEST_INIT (p, *y, MODE (REAL));
    x->status = INITIALISED_MASK;
    x->value = y->value;
    j += inc_b;
    i += inc_a;
  }
}

/*!
\brief PROC (REF [] REAL, [] REAL, [] REAL) VOID vector add
\param p
**/

void genie_vector_add (NODE_T * p)
{
  A68_REF a, b, c;
  A68_ARRAY *arr_a, *arr_b, *arr_c;
  A68_TUPLE *tup_a, *tup_b, *tup_c;
  int l_a, l_b, l_c, m, inc_a, inc_b, inc_c, i, j, k;
  BYTE_T *b_a, *b_b, *b_c;
/* POP arguments. */
  POP (p, &c, SIZE_OF (A68_REF));
  TEST_INIT (p, c, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_c, tup_c, &c);
  l_c = ROW_SIZE (tup_c);
  POP (p, &b, SIZE_OF (A68_REF));
  TEST_INIT (p, b, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_b, tup_b, &b);
  l_b = ROW_SIZE (tup_b);
  POP (p, &a, SIZE_OF (A68_REF));
  TEST_INIT (p, a, MODE (REF_ROW_REAL));
  TEST_NIL (p, a, MODE (REF_ROW_REAL));
  a = *(A68_REF *) ADDRESS (&a);
  TEST_INIT (p, a, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_a, tup_a, &a);
  l_a = ROW_SIZE (tup_a);
/* Test length. */
  TEST_LENGTH (l_a, l_b);
  TEST_LENGTH (l_a, l_c);
/* Optimised loop for vector element access. */
  b_c = (BYTE_T *) ADDRESS (&arr_c->array);
  b_b = (BYTE_T *) ADDRESS (&arr_b->array);
  b_a = (BYTE_T *) ADDRESS (&arr_a->array);
  k = BASE (arr_c, tup_c);
  j = BASE (arr_b, tup_b);
  i = BASE (arr_a, tup_a);
  inc_c = tup_c->span * arr_c->elem_size;
  inc_b = tup_b->span * arr_b->elem_size;
  inc_a = tup_a->span * arr_a->elem_size;
  for (m = 0; m < l_a; m++) {
    A68_REAL *z = (A68_REAL *) & (b_c[k]);
    A68_REAL *y = (A68_REAL *) & (b_b[j]);
    A68_REAL *x = (A68_REAL *) & (b_a[i]);
    TEST_INIT (p, *z, MODE (REAL));
    TEST_INIT (p, *y, MODE (REAL));
    x->status = INITIALISED_MASK;
    x->value = y->value + z->value;
    k += inc_c;
    j += inc_b;
    i += inc_a;
  }
}

/*!
\brief PROC (REF [] REAL, [] REAL, [] REAL) VOID vector sub
\param p
**/

void genie_vector_sub (NODE_T * p)
{
  A68_REF a, b, c;
  A68_ARRAY *arr_a, *arr_b, *arr_c;
  A68_TUPLE *tup_a, *tup_b, *tup_c;
  int l_a, l_b, l_c, m, inc_a, inc_b, inc_c, i, j, k;
  BYTE_T *b_a, *b_b, *b_c;
/* POP arguments. */
  POP (p, &c, SIZE_OF (A68_REF));
  TEST_INIT (p, c, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_c, tup_c, &c);
  l_c = ROW_SIZE (tup_c);
  POP (p, &b, SIZE_OF (A68_REF));
  TEST_INIT (p, b, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_b, tup_b, &b);
  l_b = ROW_SIZE (tup_b);
  POP (p, &a, SIZE_OF (A68_REF));
  TEST_INIT (p, a, MODE (REF_ROW_REAL));
  TEST_NIL (p, a, MODE (REF_ROW_REAL));
  a = *(A68_REF *) ADDRESS (&a);
  TEST_INIT (p, a, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_a, tup_a, &a);
  l_a = ROW_SIZE (tup_a);
/* Test length. */
  TEST_LENGTH (l_a, l_b);
  TEST_LENGTH (l_a, l_c);
/* Optimised loop for vector element access. */
  b_c = (BYTE_T *) ADDRESS (&arr_c->array);
  b_b = (BYTE_T *) ADDRESS (&arr_b->array);
  b_a = (BYTE_T *) ADDRESS (&arr_a->array);
  k = BASE (arr_c, tup_c);
  j = BASE (arr_b, tup_b);
  i = BASE (arr_a, tup_a);
  inc_c = tup_c->span * arr_c->elem_size;
  inc_b = tup_b->span * arr_b->elem_size;
  inc_a = tup_a->span * arr_a->elem_size;
  for (m = 0; m < l_a; m++) {
    A68_REAL *z = (A68_REAL *) & (b_c[k]);
    A68_REAL *y = (A68_REAL *) & (b_b[j]);
    A68_REAL *x = (A68_REAL *) & (b_a[i]);
    TEST_INIT (p, *z, MODE (REAL));
    TEST_INIT (p, *y, MODE (REAL));
    x->status = INITIALISED_MASK;
    x->value = y->value - z->value;
    k += inc_c;
    j += inc_b;
    i += inc_a;
  }
}

/*!
\brief PROC (REF [] REAL, [] REAL, [] REAL) VOID vector mul
\param p
**/

void genie_vector_mul (NODE_T * p)
{
  A68_REF a, b, c;
  A68_ARRAY *arr_a, *arr_b, *arr_c;
  A68_TUPLE *tup_a, *tup_b, *tup_c;
  int l_a, l_b, l_c, m, inc_a, inc_b, inc_c, i, j, k;
  BYTE_T *b_a, *b_b, *b_c;
/* POP arguments. */
  POP (p, &c, SIZE_OF (A68_REF));
  TEST_INIT (p, c, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_c, tup_c, &c);
  l_c = ROW_SIZE (tup_c);
  POP (p, &b, SIZE_OF (A68_REF));
  TEST_INIT (p, b, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_b, tup_b, &b);
  l_b = ROW_SIZE (tup_b);
  POP (p, &a, SIZE_OF (A68_REF));
  TEST_INIT (p, a, MODE (REF_ROW_REAL));
  TEST_NIL (p, a, MODE (REF_ROW_REAL));
  a = *(A68_REF *) ADDRESS (&a);
  TEST_INIT (p, a, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_a, tup_a, &a);
  l_a = ROW_SIZE (tup_a);
/* Test length. */
  TEST_LENGTH (l_a, l_b);
  TEST_LENGTH (l_a, l_c);
/* Optimised loop for vector element access. */
  b_c = (BYTE_T *) ADDRESS (&arr_c->array);
  b_b = (BYTE_T *) ADDRESS (&arr_b->array);
  b_a = (BYTE_T *) ADDRESS (&arr_a->array);
  k = BASE (arr_c, tup_c);
  j = BASE (arr_b, tup_b);
  i = BASE (arr_a, tup_a);
  inc_c = tup_c->span * arr_c->elem_size;
  inc_b = tup_b->span * arr_b->elem_size;
  inc_a = tup_a->span * arr_a->elem_size;
  for (m = 0; m < l_a; m++) {
    A68_REAL *z = (A68_REAL *) & (b_c[k]);
    A68_REAL *y = (A68_REAL *) & (b_b[j]);
    A68_REAL *x = (A68_REAL *) & (b_a[i]);
    TEST_INIT (p, *z, MODE (REAL));
    TEST_INIT (p, *y, MODE (REAL));
    x->status = INITIALISED_MASK;
    x->value = y->value * z->value;
    k += inc_c;
    j += inc_b;
    i += inc_a;
  }
}

/*!
\brief PROC (REF [] REAL, [] REAL, [] REAL) VOID vector div
\param p
**/

void genie_vector_div (NODE_T * p)
{
  A68_REF a, b, c;
  A68_ARRAY *arr_a, *arr_b, *arr_c;
  A68_TUPLE *tup_a, *tup_b, *tup_c;
  int l_a, l_b, l_c, m, inc_a, inc_b, inc_c, i, j, k;
  BYTE_T *b_a, *b_b, *b_c;
/* POP arguments. */
  POP (p, &c, SIZE_OF (A68_REF));
  TEST_INIT (p, c, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_c, tup_c, &c);
  l_c = ROW_SIZE (tup_c);
  POP (p, &b, SIZE_OF (A68_REF));
  TEST_INIT (p, b, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_b, tup_b, &b);
  l_b = ROW_SIZE (tup_b);
  POP (p, &a, SIZE_OF (A68_REF));
  TEST_INIT (p, a, MODE (REF_ROW_REAL));
  TEST_NIL (p, a, MODE (REF_ROW_REAL));
  a = *(A68_REF *) ADDRESS (&a);
  TEST_INIT (p, a, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_a, tup_a, &a);
  l_a = ROW_SIZE (tup_a);
/* Test length. */
  TEST_LENGTH (l_a, l_b);
  TEST_LENGTH (l_a, l_c);
/* Optimised loop for vector element access. */
  b_c = (BYTE_T *) ADDRESS (&arr_c->array);
  b_b = (BYTE_T *) ADDRESS (&arr_b->array);
  b_a = (BYTE_T *) ADDRESS (&arr_a->array);
  k = BASE (arr_c, tup_c);
  j = BASE (arr_b, tup_b);
  i = BASE (arr_a, tup_a);
  inc_c = tup_c->span * arr_c->elem_size;
  inc_b = tup_b->span * arr_b->elem_size;
  inc_a = tup_a->span * arr_a->elem_size;
  for (m = 0; m < l_a; m++) {
    A68_REAL *z = (A68_REAL *) & (b_c[k]);
    A68_REAL *y = (A68_REAL *) & (b_b[j]);
    A68_REAL *x = (A68_REAL *) & (b_a[i]);
    TEST_INIT (p, *z, MODE (REAL));
    TEST_INIT (p, *y, MODE (REAL));
    if (z->value == 0.0) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (REAL));
      exit_genie (p, A_RUNTIME_ERROR);
    }
    x->status = INITIALISED_MASK;
    x->value = y->value / z->value;
    k += inc_c;
    j += inc_b;
    i += inc_a;
  }
}

/*!
\brief PROC ([] REAL, [] REAL) REAL vector inproduct
\param p
**/

void genie_vector_inner_product (NODE_T * p)
{
  A68_REF a, b;
  int digits = long_mp_digits ();
  ADDR_T pop_sp;
  MP_DIGIT_T *dsum, *dfac;
  A68_ARRAY *arr_a, *arr_b;
  A68_TUPLE *tup_a, *tup_b;
  int l_a, l_b, k, inc_a, inc_b, i, j;
  BYTE_T *b_a, *b_b;
/* POP arguments. */
  POP (p, &b, SIZE_OF (A68_REF));
  TEST_INIT (p, b, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_b, tup_b, &b);
  l_b = ROW_SIZE (tup_b);
  POP (p, &a, SIZE_OF (A68_REF));
  TEST_INIT (p, a, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr_a, tup_a, &a);
  l_a = ROW_SIZE (tup_a);
  TEST_LENGTH (l_a, l_b);
/* Optimised loop for vector element access. */
  b_b = (BYTE_T *) ADDRESS (&arr_b->array);
  b_a = (BYTE_T *) ADDRESS (&arr_a->array);
  j = BASE (arr_b, tup_b);
  i = BASE (arr_a, tup_a);
  inc_b = tup_b->span * arr_b->elem_size;
  inc_a = tup_a->span * arr_a->elem_size;
  pop_sp = stack_pointer;
  STACK_MP (dsum, p, digits);
  SET_MP_ZERO (dsum, digits);
  STACK_MP (dfac, p, digits);
  for (k = 0; k < l_a; k++) {
    A68_REAL *y = (A68_REAL *) & (b_b[j]);
    A68_REAL *x = (A68_REAL *) & (b_a[i]);
    TEST_INIT (p, *y, MODE (REAL));
    TEST_INIT (p, *x, MODE (REAL));
    real_to_mp (p, dfac, x->value * y->value, digits);
    add_mp (p, dsum, dsum, dfac, digits);
    j += inc_b;
    i += inc_a;
  }
  stack_pointer = pop_sp;
  PUSH_REAL (p, mp_to_real (p, dsum, digits));
}
