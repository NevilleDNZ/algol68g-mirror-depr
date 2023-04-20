//! @file single-torrix-gsl.c
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
//! REAL vector and matrix support using GSL.

#include "a68g.h"
#include "a68g-torrix.h"

#if defined (HAVE_GSL)

NODE_T *torrix_error_node = NO_NODE;

//! @brief Set permutation vector element - function fails in gsl.

void gsl_permutation_set (const gsl_permutation * p, const size_t i, const size_t j)
{
  DATA (p)[i] = j;
}

//! @brief Map GSL error handler onto a68g error handler.

void torrix_error_handler (const char *reason, const char *file, int line, int gsl_errno)
{
  if (line != 0) {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s in line %d of file %s", reason, line, file) >= 0);
  } else {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", reason) >= 0);
  }
  diagnostic (A68_RUNTIME_ERROR, torrix_error_node, ERROR_TORRIX, A68 (edit_line), gsl_strerror (gsl_errno));
  exit_genie (torrix_error_node, A68_RUNTIME_ERROR);
}

//! @brief Pop [] INT on the stack as gsl_permutation.

gsl_permutation *pop_permutation (NODE_T * p, BOOL_T get)
{
  A68_REF desc; A68_ARRAY *arr; A68_TUPLE *tup;
  POP_REF (p, &desc);
  CHECK_REF (p, desc, M_ROW_INT);
  GET_DESCRIPTOR (arr, tup, &desc);
  int len = ROW_SIZE (tup);
  gsl_permutation *v = gsl_permutation_calloc ((size_t) len);
  if (get && len > 0) {
    BYTE_T *base = DEREF (BYTE_T, &ARRAY (arr));
    int idx = VECTOR_OFFSET (arr, tup);
    int inc = SPAN (tup) * ELEM_SIZE (arr);
    for (int k = 0; k < len; k++, idx += inc) {
      A68_INT *x = (A68_INT *) (base + idx);
      CHECK_INIT (p, INITIALISED (x), M_INT);
      gsl_permutation_set (v, (size_t) k, (size_t) VALUE (x));
    }
  }
  return v;
}

//! @brief Push gsl_permutation on the stack as [] INT.

void push_permutation (NODE_T * p, gsl_permutation * v)
{
  int len = (int) (SIZE (v));
  A68_REF desc, row; A68_ARRAY arr; A68_TUPLE tup;
  NEW_ROW_1D (desc, row, arr, tup, M_ROW_INT, M_INT, len);
  BYTE_T *base = DEREF (BYTE_T, &ARRAY (&arr));
  int idx = VECTOR_OFFSET (&arr, &tup);
  int inc = SPAN (&tup) * ELEM_SIZE (&arr);
  for (int k = 0; k < len; k++, idx += inc) {
    A68_INT *x = (A68_INT *) (base + idx);
    STATUS (x) = INIT_MASK;
    VALUE (x) = (int) gsl_permutation_get (v, (size_t) k);
  }
  PUSH_REF (p, desc);
}

//! @brief Pop [] REAL on the stack as gsl_vector.

gsl_vector *pop_vector (NODE_T * p, BOOL_T get)
{
  A68_REF desc; A68_ARRAY *arr; A68_TUPLE *tup;
  POP_REF (p, &desc);
  CHECK_REF (p, desc, M_ROW_REAL);
  GET_DESCRIPTOR (arr, tup, &desc);
  int len = ROW_SIZE (tup);
  gsl_vector *v = gsl_vector_calloc ((size_t) len);
  if (get && len > 0) {
    BYTE_T *base = DEREF (BYTE_T, &ARRAY (arr));
    int idx = VECTOR_OFFSET (arr, tup);
    int inc = SPAN (tup) * ELEM_SIZE (arr);
    for (int k = 0; k < len; k++, idx += inc) {
      A68_REAL *x = (A68_REAL *) (base + idx);
      CHECK_INIT (p, INITIALISED (x), M_REAL);
      gsl_vector_set (v, (size_t) k, VALUE (x));
    }
  }
  return v;
}

//! @brief Push gsl_vector on the stack as [] REAL.

void push_vector (NODE_T * p, gsl_vector * v)
{
  PUSH_REF (p, vector_to_row (p, v));
}

//! @brief Pop [, ] REAL on the stack as gsl_matrix.

gsl_matrix *pop_matrix (NODE_T * p, BOOL_T get)
{
  A68_REF desc; A68_ARRAY *arr; A68_TUPLE *tup1, *tup2;
  POP_REF (p, &desc);
  CHECK_REF (p, desc, M_ROW_ROW_REAL);
  GET_DESCRIPTOR (arr, tup1, &desc);
  tup2 = &(tup1[1]);
  int len1 = ROW_SIZE (tup1), len2 = ROW_SIZE (tup2);
  gsl_matrix *a = gsl_matrix_calloc ((size_t) len1, (size_t) len2);
  if (get && (len1 * len2 > 0)) {
    BYTE_T *base = DEREF (BYTE_T, &ARRAY (arr));
    int idx1 = MATRIX_OFFSET (arr, tup1, tup2);
    int inc1 = SPAN (tup1) * ELEM_SIZE (arr), inc2 = SPAN (tup2) * ELEM_SIZE (arr);
    for (int k1 = 0; k1 < len1; k1++, idx1 += inc1) {
      for (int k2 = 0, idx2 = idx1; k2 < len2; k2++, idx2 += inc2) {
        A68_REAL *x = (A68_REAL *) (base + idx2);
        CHECK_INIT (p, INITIALISED (x), M_REAL);
        gsl_matrix_set (a, (size_t) k1, (size_t) k2, VALUE (x));
      }
    }
  }
  return a;
}

//! @brief Push gsl_matrix on the stack as [, ] REAL.

void push_matrix (NODE_T * p, gsl_matrix * a)
{
  PUSH_REF (p, matrix_to_row (p, a));
}

//! @brief Pop [] COMPLEX on the stack as gsl_vector_complex.

gsl_vector_complex *pop_vector_complex (NODE_T * p, BOOL_T get)
{
  A68_REF desc; A68_ARRAY *arr; A68_TUPLE *tup;
  POP_REF (p, &desc);
  CHECK_REF (p, desc, M_ROW_COMPLEX);
  GET_DESCRIPTOR (arr, tup, &desc);
  int len = ROW_SIZE (tup);
  gsl_vector_complex *v = gsl_vector_complex_calloc ((size_t) len);
  if (get && len > 0) {
    BYTE_T *base = DEREF (BYTE_T, &ARRAY (arr));
    int idx = VECTOR_OFFSET (arr, tup);
    int inc = SPAN (tup) * ELEM_SIZE (arr);
    for (int k = 0; k < len; k++, idx += inc) {
      A68_REAL *re = (A68_REAL *) (base + idx);
      A68_REAL *im = (A68_REAL *) (base + idx + SIZE (M_REAL));
      gsl_complex z;
      CHECK_INIT (p, INITIALISED (re), M_COMPLEX);
      CHECK_INIT (p, INITIALISED (im), M_COMPLEX);
      GSL_SET_COMPLEX (&z, VALUE (re), VALUE (im));
      gsl_vector_complex_set (v, (size_t) k, z);
    }
  }
  return v;
}

//! @brief Push gsl_vector_complex on the stack as [] COMPLEX.

void push_vector_complex (NODE_T * p, gsl_vector_complex * v)
{
  int len = (int) (SIZE (v));
  A68_REF desc, row; A68_ARRAY arr; A68_TUPLE tup;
  NEW_ROW_1D (desc, row, arr, tup, M_ROW_COMPLEX, M_COMPLEX, len);
  BYTE_T *base = DEREF (BYTE_T, &ARRAY (&arr));
  int idx = VECTOR_OFFSET (&arr, &tup);
  int inc = SPAN (&tup) * ELEM_SIZE (&arr);
  for (int k = 0; k < len; k++, idx += inc) {
    A68_REAL *re = (A68_REAL *) (base + idx);
    A68_REAL *im = (A68_REAL *) (base + idx + SIZE (M_REAL));
    gsl_complex z = gsl_vector_complex_get (v, (size_t) k);
    STATUS (re) = INIT_MASK;
    VALUE (re) = GSL_REAL (z);
    STATUS (im) = INIT_MASK;
    VALUE (im) = GSL_IMAG (z);
    CHECK_COMPLEX (p, VALUE (re), VALUE (im));
  }
  PUSH_REF (p, desc);
}

//! @brief Pop [, ] COMPLEX on the stack as gsl_matrix_complex.

gsl_matrix_complex *pop_matrix_complex (NODE_T * p, BOOL_T get)
{
  A68_REF desc; A68_ARRAY *arr; A68_TUPLE *tup1, *tup2;
  POP_REF (p, &desc);
  CHECK_REF (p, desc, M_ROW_ROW_COMPLEX);
  GET_DESCRIPTOR (arr, tup1, &desc);
  tup2 = &(tup1[1]);
  int len1 = ROW_SIZE (tup1), len2 = ROW_SIZE (tup2);
  gsl_matrix_complex *a = gsl_matrix_complex_calloc ((size_t) len1, (size_t) len2);
  if (get && (len1 * len2 > 0)) {
    BYTE_T *base = DEREF (BYTE_T, &ARRAY (arr));
    int idx1 = MATRIX_OFFSET (arr, tup1, tup2);
    int inc1 = SPAN (tup1) * ELEM_SIZE (arr), inc2 = SPAN (tup2) * ELEM_SIZE (arr);
    for (int k1 = 0; k1 < len1; k1++, idx1 += inc1) {
      for (int k2 = 0, idx2 = idx1; k2 < len2; k2++, idx2 += inc2) {
        A68_REAL *re = (A68_REAL *) (base + idx2);
        A68_REAL *im = (A68_REAL *) (base + idx2 + SIZE (M_REAL));
        CHECK_INIT (p, INITIALISED (re), M_COMPLEX);
        CHECK_INIT (p, INITIALISED (im), M_COMPLEX);
        gsl_complex z;
        GSL_SET_COMPLEX (&z, VALUE (re), VALUE (im));
        gsl_matrix_complex_set (a, (size_t) k1, (size_t) k2, z);
      }
    }
  }
  return a;
}

//! @brief Push gsl_matrix_complex on the stack as [, ] COMPLEX.

void push_matrix_complex (NODE_T * p, gsl_matrix_complex * a)
{
  int len1 = (int) (SIZE1 (a)), len2 = (int) (SIZE2 (a));
  A68_REF desc, row; A68_ARRAY arr; A68_TUPLE tup1, tup2;
  desc = heap_generator (p, M_ROW_ROW_COMPLEX, DESCRIPTOR_SIZE (2));
  row = heap_generator (p, M_ROW_ROW_COMPLEX, len1 * len2 * 2 * SIZE (M_REAL));
  DIM (&arr) = 2;
  MOID (&arr) = M_COMPLEX;
  ELEM_SIZE (&arr) = 2 * SIZE (M_REAL);
  SLICE_OFFSET (&arr) = FIELD_OFFSET (&arr) = 0;
  ARRAY (&arr) = row;
  LWB (&tup1) = 1; UPB (&tup1) = len1; SPAN (&tup1) = 1;
  SHIFT (&tup1) = LWB (&tup1); K (&tup1) = 0;
  LWB (&tup2) = 1; UPB (&tup2) = len2; SPAN (&tup2) = ROW_SIZE (&tup1);
  SHIFT (&tup2) = LWB (&tup2) * SPAN (&tup2); K (&tup2) = 0;
  PUT_DESCRIPTOR2 (arr, tup1, tup2, &desc);
  BYTE_T *base = DEREF (BYTE_T, &ARRAY (&arr));
  int idx1 = MATRIX_OFFSET (&arr, &tup1, &tup2);
  int inc1 = SPAN (&tup1) * ELEM_SIZE (&arr), inc2 = SPAN (&tup2) * ELEM_SIZE (&arr);
  for (int k1 = 0; k1 < len1; k1++, idx1 += inc1) {
    for (int k2 = 0, idx2 = idx1; k2 < len2; k2++, idx2 += inc2) {
      A68_REAL *re = (A68_REAL *) (base + idx2);
      A68_REAL *im = (A68_REAL *) (base + idx2 + SIZE (M_REAL));
      gsl_complex z = gsl_matrix_complex_get (a, (size_t) k1, (size_t) k2);
      STATUS (re) = INIT_MASK;
      VALUE (re) = GSL_REAL (z);
      STATUS (im) = INIT_MASK;
      VALUE (im) = GSL_IMAG (z);
      CHECK_COMPLEX (p, VALUE (re), VALUE (im));
    }
  }
  PUSH_REF (p, desc);
}

//! @brief Generically perform operation and assign result (+:=, -:=, ...) .

void op_ab_torrix (NODE_T * p, MOID_T * m, MOID_T * n, GPROC * op)
{
  ADDR_T parm_size = SIZE (m) + SIZE (n);
  A68_REF dst, src, *save = (A68_REF *) STACK_OFFSET (-parm_size);
  torrix_error_node = p;
  dst = *save;
  CHECK_REF (p, dst, m);
  *save = *DEREF (A68_ROW, &dst);
  STATUS (&src) = (STATUS_MASK_T) (INIT_MASK | IN_STACK_MASK);
  OFFSET (&src) = A68_SP - parm_size;
  (*op) (p);
  if (IS_REF (m)) {
    genie_store (p, SUB (m), &dst, &src);
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
  }
  *save = dst;
}

//! @brief PROC vector echo = ([] REAL) [] REAL

void genie_vector_echo (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *u = pop_vector (p, A68_TRUE);
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC matrix echo = ([, ] REAL) [, ] REAL

void genie_matrix_echo (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *a = pop_matrix (p, A68_TRUE);
  push_matrix (p, a);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC complex vector echo = ([] COMPLEX) [] COMPLEX

void genie_vector_complex_echo (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC complex matrix echo = ([, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_echo (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *a = pop_matrix_complex (p, A68_TRUE);
  push_matrix_complex (p, a);
  gsl_matrix_complex_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP ROW = ([] REAL) [, ] REAL

void genie_vector_row (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *u = pop_vector (p, A68_TRUE);
  gsl_matrix *v = gsl_matrix_calloc (1, SIZE (u));
  ASSERT_GSL (gsl_matrix_set_row (v, 0, u));
  push_matrix (p, v);
  gsl_vector_free (u);
  gsl_matrix_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP COL = ([] REAL) [, ] REAL

void genie_vector_col (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *u = pop_vector (p, A68_TRUE);
  gsl_matrix *v = gsl_matrix_calloc (SIZE (u), 1);
  ASSERT_GSL (gsl_matrix_set_col (v, 0, u));
  push_matrix (p, v);
  gsl_vector_free (u);
  gsl_matrix_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP - = ([] REAL) [] REAL

void genie_vector_minus (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *u = pop_vector (p, A68_TRUE);
  ASSERT_GSL (gsl_vector_scale (u, -1));
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP - = ([, ] REAL) [, ] REAL

void genie_matrix_minus (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix * a = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_scale (a, -1));
  push_matrix (p, a);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP T = ([, ] REAL) [, ] REAL

void genie_matrix_transpose (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *a = pop_matrix (p, A68_TRUE);
  gsl_matrix *t = gsl_matrix_calloc (SIZE2(a), SIZE1(a));
  ASSERT_GSL (gsl_matrix_transpose_memcpy (t, a));
  push_matrix (p, t);
  gsl_matrix_free (a);
  gsl_matrix_free (t);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP T = ([, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_transpose (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *a = pop_matrix_complex (p, A68_TRUE);
  gsl_matrix_complex *t = gsl_matrix_complex_calloc (SIZE2(a), SIZE1(a));
  ASSERT_GSL ( gsl_matrix_complex_transpose_memcpy (t, a));
  push_matrix_complex (p, a);
  gsl_matrix_complex_free (a);
  gsl_matrix_complex_free (t);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP INV = ([, ] REAL) [, ] REAL

void genie_matrix_inv (NODE_T * p)
{
// Avoid direct use of the inverse whenever possible; linear solver functions
// can obtain the same result more efficiently and reliably.
// Consult any introductory textbook on numerical linear algebra for details.
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  unt M = SIZE1 (u), N =SIZE2 (u);
  MATH_RTE (p, M != N, M_ROW_ROW_REAL, "matrix is not square");
  gsl_matrix *inv = NO_REAL_MATRIX;
  compute_pseudo_inverse (p, &inv, u, 0); // Pseudo inverse equals inverse for a square matrix.
  push_matrix (p, inv);
  gsl_matrix_free (inv);
  gsl_matrix_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP INV = ([, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_inv (NODE_T * p)
{
// Avoid direct use of the inverse whenever possible; linear solver functions
// can obtain the same result more efficiently and reliably.
// Consult any introductory textbook on numerical linear algebra for details.
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  unt M = SIZE1 (u), N =SIZE2 (u);
  MATH_RTE (p, M != N, M_ROW_ROW_COMPLEX, "matrix is not square");
  gsl_permutation *q = gsl_permutation_calloc (SIZE1 (u));
  int sign;
  ASSERT_GSL (gsl_linalg_complex_LU_decomp (u, q, &sign));
  gsl_matrix_complex *inv = gsl_matrix_complex_calloc (SIZE1 (u), SIZE2 (u));
  ASSERT_GSL (gsl_linalg_complex_LU_invert (u, q, inv));
  push_matrix_complex (p, inv);
  gsl_matrix_complex_free (inv);
  gsl_matrix_complex_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP DET = ([, ] REAL) REAL

void genie_matrix_det (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  gsl_permutation *q = gsl_permutation_calloc (SIZE1 (u));
  int sign;
  ASSERT_GSL (gsl_linalg_LU_decomp (u, q, &sign));
  PUSH_VALUE (p, gsl_linalg_LU_det (u, sign), A68_REAL);
  gsl_matrix_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP DET = ([, ] COMPLEX) COMPLEX

void genie_matrix_complex_det (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  gsl_permutation *q = gsl_permutation_calloc (SIZE1 (u));
  int sign;
  ASSERT_GSL (gsl_linalg_complex_LU_decomp (u, q, &sign));
  gsl_complex det = gsl_linalg_complex_LU_det (u, sign);
  PUSH_VALUE (p, GSL_REAL (det), A68_REAL);
  PUSH_VALUE (p, GSL_IMAG (det), A68_REAL);
  gsl_matrix_complex_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP TRACE = ([, ] REAL) REAL

void genie_matrix_trace (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *a = pop_matrix (p, A68_TRUE);
  int len1 = (int) (SIZE1 (a)), len2 = (int) (SIZE2 (a));
  if (len1 != len2) {
    torrix_error_handler ("cannot calculate trace", __FILE__, __LINE__, GSL_ENOTSQR);
  }
  REAL_T sum = 0.0;
  for (int k = 0; k < len1; k++) {
    sum += gsl_matrix_get (a, (size_t) k, (size_t) k);
  }
  PUSH_VALUE (p, sum, A68_REAL);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP TRACE = ([, ] COMPLEX) COMPLEX

void genie_matrix_complex_trace (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *a = pop_matrix_complex (p, A68_TRUE);
  int len1 = (int) (SIZE1 (a)), len2 = (int) (SIZE2 (a));
  if (len1 != len2) {
    torrix_error_handler ("cannot calculate trace", __FILE__, __LINE__, GSL_ENOTSQR);
  }
  gsl_complex sum;
  GSL_SET_COMPLEX (&sum, 0.0, 0.0);
  for (int k = 0; k < len1; k++) {
    sum = gsl_complex_add (sum, gsl_matrix_complex_get (a, (size_t) k, (size_t) k));
  }
  PUSH_VALUE (p, GSL_REAL (sum), A68_REAL);
  PUSH_VALUE (p, GSL_IMAG (sum), A68_REAL);
  gsl_matrix_complex_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP - = ([] COMPLEX) [] COMPLEX

void genie_vector_complex_minus (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  gsl_blas_zdscal (-1, u);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP - = ([, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_minus (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_complex one;
  GSL_SET_COMPLEX (&one, -1.0, 0.0);
  gsl_matrix_complex *a = pop_matrix_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_complex_scale (a, one));
  push_matrix_complex (p, a);
  gsl_matrix_complex_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP + = ([] REAL, [] REAL) [] REAL

void genie_vector_add (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *v = pop_vector (p, A68_TRUE);
  gsl_vector *u = pop_vector (p, A68_TRUE);
  ASSERT_GSL (gsl_vector_add (u, v));
  push_vector (p, u);
  gsl_vector_free (u);
  gsl_vector_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP - = ([] REAL, [] REAL) [] REAL

void genie_vector_sub (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *v = pop_vector (p, A68_TRUE);
  gsl_vector *u = pop_vector (p, A68_TRUE);
  ASSERT_GSL (gsl_vector_sub (u, v));
  push_vector (p, u);
  gsl_vector_free (u);
  gsl_vector_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP = = ([] REAL, [] REAL) BOOL

void genie_vector_eq (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *v = pop_vector (p, A68_TRUE);
  gsl_vector *u = pop_vector (p, A68_TRUE);
  ASSERT_GSL (gsl_vector_sub (u, v));
  PUSH_VALUE (p, (BOOL_T) (gsl_vector_isnull (u) ? A68_TRUE : A68_FALSE), A68_BOOL);
  gsl_vector_free (u);
  gsl_vector_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP /= = ([] REAL, [] REAL) BOOL

void genie_vector_ne (NODE_T * p)
{
  genie_vector_eq (p);
  genie_not_bool (p);
}

//! @brief OP +:= = (REF [] REAL, [] REAL) REF [] REAL

void genie_vector_plusab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_REAL, M_ROW_REAL, genie_vector_add);
}

//! @brief OP -:= = (REF [] REAL, [] REAL) REF [] REAL

void genie_vector_minusab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_REAL, M_ROW_REAL, genie_vector_sub);
}

//! @brief OP + = ([, ] REAL, [, ] REAL) [, ] REAL

void genie_matrix_add (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *v = pop_matrix (p, A68_TRUE);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_add (u, v));
  push_matrix (p, u);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP - = ([, ] REAL, [, ] REAL) [, ] REAL

void genie_matrix_sub (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *v = pop_matrix (p, A68_TRUE);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_sub (u, v));
  push_matrix (p, u);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP = = ([, ] REAL, [, ] REAL) [, ] BOOL

void genie_matrix_eq (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *v = pop_matrix (p, A68_TRUE);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_sub (u, v));
  PUSH_VALUE (p, (BOOL_T) (gsl_matrix_isnull (u) ? A68_TRUE : A68_FALSE), A68_BOOL);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP /= = ([, ] REAL, [, ] REAL) [, ] BOOL

void genie_matrix_ne (NODE_T * p)
{
  genie_matrix_eq (p);
  genie_not_bool (p);
}

//! @brief OP +:= = (REF [, ] REAL, [, ] REAL) [, ] REAL

void genie_matrix_plusab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_ROW_REAL, M_ROW_ROW_REAL, genie_matrix_add);
}

//! @brief OP -:= = (REF [, ] REAL, [, ] REAL) [, ] REAL

void genie_matrix_minusab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_ROW_REAL, M_ROW_ROW_REAL, genie_matrix_sub);
}

//! @brief OP + = ([] COMPLEX, [] COMPLEX) [] COMPLEX

void genie_vector_complex_add (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_complex one;
  GSL_SET_COMPLEX (&one, 1.0, 0.0);
  gsl_vector_complex *v = pop_vector_complex (p, A68_TRUE);
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_blas_zaxpy (one, u, v));
  push_vector_complex (p, v);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP - = ([] COMPLEX, [] COMPLEX) [] COMPLEX

void genie_vector_complex_sub (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_complex one;
  GSL_SET_COMPLEX (&one, -1.0, 0.0);
  gsl_vector_complex *v = pop_vector_complex (p, A68_TRUE);
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_blas_zaxpy (one, v, u));
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP = = ([] COMPLEX, [] COMPLEX) BOOL

void genie_vector_complex_eq (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_complex one;
  GSL_SET_COMPLEX (&one, -1.0, 0.0);
  gsl_vector_complex *v = pop_vector_complex (p, A68_TRUE);
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_blas_zaxpy (one, v, u));
  PUSH_VALUE (p, (BOOL_T) (gsl_vector_complex_isnull (u) ? A68_TRUE : A68_FALSE), A68_BOOL);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP /= = ([] COMPLEX, [] COMPLEX) BOOL

void genie_vector_complex_ne (NODE_T * p)
{
  genie_vector_complex_eq (p);
  genie_not_bool (p);
}

//! @brief OP +:= = (REF [] COMPLEX, [] COMPLEX) [] COMPLEX

void genie_vector_complex_plusab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_COMPLEX, M_ROW_COMPLEX, genie_vector_complex_add);
}

//! @brief OP -:= = (REF [] COMPLEX, [] COMPLEX) [] COMPLEX

void genie_vector_complex_minusab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_COMPLEX, M_ROW_COMPLEX, genie_vector_complex_sub);
}

//! @brief OP + = ([, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_add (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *v = pop_matrix_complex (p, A68_TRUE);
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_complex_add (u, v));
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  gsl_matrix_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP - = ([, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_sub (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *v = pop_matrix_complex (p, A68_TRUE);
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_complex_sub (u, v));
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  gsl_matrix_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP = = ([, ] COMPLEX, [, ] COMPLEX) BOOL

void genie_matrix_complex_eq (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *v = pop_matrix_complex (p, A68_TRUE);
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_complex_sub (u, v));
  PUSH_VALUE (p, (BOOL_T) (gsl_matrix_complex_isnull (u) ? A68_TRUE : A68_FALSE), A68_BOOL);
  gsl_matrix_complex_free (u);
  gsl_matrix_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP /= = ([, ] COMPLEX, [, ] COMPLEX) BOOL

void genie_matrix_complex_ne (NODE_T * p)
{
  genie_matrix_complex_eq (p);
  genie_not_bool (p);
}

//! @brief OP +:= = (REF [, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_plusab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, genie_matrix_complex_add);
}

//! @brief OP -:= = (REF [, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_minusab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, genie_matrix_complex_sub);
}

//! @brief OP * = ([] REAL, REAL) [] REAL

void genie_vector_scale_real (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REAL v;
  POP_OBJECT (p, &v, A68_REAL);
  gsl_vector *u = pop_vector (p, A68_TRUE);
  ASSERT_GSL (gsl_vector_scale (u, VALUE (&v)));
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = (REAL, [] REAL) [] REAL

void genie_real_scale_vector (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *u = pop_vector (p, A68_TRUE);
  A68_REAL v;
  POP_OBJECT (p, &v, A68_REAL);
  ASSERT_GSL (gsl_vector_scale (u, VALUE (&v)));
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([, ] REAL, REAL) [, ] REAL

void genie_matrix_scale_real (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REAL v;
  POP_OBJECT (p, &v, A68_REAL);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_scale (u, VALUE (&v)));
  push_matrix (p, u);
  gsl_matrix_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = (REAL, [, ] REAL) [, ] REAL

void genie_real_scale_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  A68_REAL v;
  POP_OBJECT (p, &v, A68_REAL);
  ASSERT_GSL (gsl_matrix_scale (u, VALUE (&v)));
  push_matrix (p, u);
  gsl_matrix_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([] COMPLEX, COMPLEX) [] COMPLEX

void genie_vector_complex_scale_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REAL re, im; gsl_complex v;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  gsl_blas_zscal (v, u);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = (COMPLEX, [] COMPLEX) [] COMPLEX

void genie_complex_scale_vector_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  A68_REAL re, im; gsl_complex v;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  gsl_blas_zscal (v, u);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([, ] COMPLEX, COMPLEX) [, ] COMPLEX

void genie_matrix_complex_scale_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REAL re, im; gsl_complex v;
  torrix_error_node = p;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_complex_scale (u, v));
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = (COMPLEX, [, ] COMPLEX) [, ] COMPLEX

void genie_complex_scale_matrix_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  A68_REAL re, im; gsl_complex v;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  ASSERT_GSL (gsl_matrix_complex_scale (u, v));
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP *:= (REF [] REAL, REAL) REF [] REAL

void genie_vector_scale_real_ab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_REAL, M_REAL, genie_vector_scale_real);
}

//! @brief OP *:= (REF [, ] REAL, REAL) REF [, ] REAL

void genie_matrix_scale_real_ab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_ROW_REAL, M_REAL, genie_matrix_scale_real);
}

//! @brief OP *:= (REF [] COMPLEX, COMPLEX) REF [] COMPLEX

void genie_vector_complex_scale_complex_ab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_COMPLEX, M_COMPLEX, genie_vector_complex_scale_complex);
}

//! @brief OP *:= (REF [, ] COMPLEX, COMPLEX) REF [, ] COMPLEX

void genie_matrix_complex_scale_complex_ab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_ROW_COMPLEX, M_COMPLEX, genie_matrix_complex_scale_complex);
}

//! @brief OP / = ([] REAL, REAL) [] REAL

void genie_vector_div_real (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REAL v;
  POP_OBJECT (p, &v, A68_REAL);
  if (VALUE (&v) == 0.0) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, M_ROW_REAL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  gsl_vector *u = pop_vector (p, A68_TRUE);
  ASSERT_GSL (gsl_vector_scale (u, 1.0 / VALUE (&v)));
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP / = ([, ] REAL, REAL) [, ] REAL

void genie_matrix_div_real (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REAL v;
  POP_OBJECT (p, &v, A68_REAL);
  if (VALUE (&v) == 0.0) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, M_ROW_ROW_REAL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_scale (u, 1.0 / VALUE (&v)));
  push_matrix (p, u);
  gsl_matrix_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP / = ([] COMPLEX, COMPLEX) [] COMPLEX

void genie_vector_complex_div_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REAL re, im; gsl_complex v;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  if (VALUE (&re) == 0.0 && VALUE (&im) == 0.0) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, M_ROW_COMPLEX);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  v = gsl_complex_inverse (v);
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  gsl_blas_zscal (v, u);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP / = ([, ] COMPLEX, COMPLEX) [, ] COMPLEX

void genie_matrix_complex_div_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REAL re, im; gsl_complex v;
  torrix_error_node = p;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  if (VALUE (&re) == 0.0 && VALUE (&im) == 0.0) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, M_ROW_ROW_COMPLEX);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  v = gsl_complex_inverse (v);
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_complex_scale (u, v));
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP /:= (REF [] REAL, REAL) REF [] REAL

void genie_vector_div_real_ab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_REAL, M_REAL, genie_vector_div_real);
}

//! @brief OP /:= (REF [, ] REAL, REAL) REF [, ] REAL

void genie_matrix_div_real_ab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_ROW_REAL, M_REAL, genie_matrix_div_real);
}

//! @brief OP /:= (REF [] COMPLEX, COMPLEX) REF [] COMPLEX

void genie_vector_complex_div_complex_ab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_COMPLEX, M_COMPLEX, genie_vector_complex_div_complex);
}

//! @brief OP /:= (REF [, ] COMPLEX, COMPLEX) REF [, ] COMPLEX

void genie_matrix_complex_div_complex_ab (NODE_T * p)
{
  op_ab_torrix (p, M_REF_ROW_ROW_COMPLEX, M_COMPLEX, genie_matrix_complex_div_complex);
}

//! @brief OP * = ([] REAL, [] REAL) REAL

void genie_vector_dot (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *v = pop_vector (p, A68_TRUE);
  gsl_vector *u = pop_vector (p, A68_TRUE);
  REAL_T w;
  ASSERT_GSL (gsl_blas_ddot (u, v, &w));
  PUSH_VALUE (p, w, A68_REAL);
  gsl_vector_free (u);
  gsl_vector_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([] COMPLEX, [] COMPLEX) COMPLEX

void genie_vector_complex_dot (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector_complex *v = pop_vector_complex (p, A68_TRUE);
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  gsl_complex w;
  ASSERT_GSL (gsl_blas_zdotc (u, v, &w));
  PUSH_VALUE (p, GSL_REAL (w), A68_REAL);
  PUSH_VALUE (p, GSL_IMAG (w), A68_REAL);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP NORM = ([] REAL) REAL

void genie_vector_norm (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *u = pop_vector (p, A68_TRUE);
  PUSH_VALUE (p, gsl_blas_dnrm2 (u), A68_REAL);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP NORM = ([] COMPLEX) COMPLEX

void genie_vector_complex_norm (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  PUSH_VALUE (p, gsl_blas_dznrm2 (u), A68_REAL);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP DYAD = ([] REAL, [] REAL) [, ] REAL

void genie_vector_dyad (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *v = pop_vector (p, A68_TRUE);
  gsl_vector *u = pop_vector (p, A68_TRUE);
  int len1 = (int) (SIZE (u)), len2 = (int) (SIZE (v));
  gsl_matrix *w = gsl_matrix_calloc ((size_t) len1, (size_t) len2);
  for (int j = 0; j < len1; j++) {
    REAL_T uj = gsl_vector_get (u, (size_t) j);
    for (int k = 0; k < len2; k++) {
      REAL_T vk = gsl_vector_get (v, (size_t) k);
      gsl_matrix_set (w, (size_t) j, (size_t) k, uj * vk);
    }
  }
  push_matrix (p, w);
  gsl_vector_free (u);
  gsl_vector_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP DYAD = ([] COMPLEX, [] COMPLEX) [, ] COMPLEX

void genie_vector_complex_dyad (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector_complex *v = pop_vector_complex (p, A68_TRUE);
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  int len1 = (int) (SIZE (u)), len2 = (int) (SIZE (v));
  gsl_matrix_complex *w = gsl_matrix_complex_calloc ((size_t) len1, (size_t) len2);
  for (int j = 0; j < len1; j++) {
    gsl_complex uj = gsl_vector_complex_get (u, (size_t) j);
    for (int k = 0; k < len2; k++) {
      gsl_complex vk = gsl_vector_complex_get (u, (size_t) k);
      gsl_matrix_complex_set (w, (size_t) j, (size_t) k, gsl_complex_mul (uj, vk));
    }
  }
  push_matrix_complex (p, w);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  gsl_matrix_complex_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([, ] REAL, [] REAL) [] REAL

void genie_matrix_times_vector (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *u = pop_vector (p, A68_TRUE);
  gsl_matrix *w = pop_matrix (p, A68_TRUE);
  int len = (int) (SIZE (u));
  gsl_vector *v = gsl_vector_calloc ((size_t) len);
  gsl_vector_set_zero (v);
  ASSERT_GSL (gsl_blas_dgemv (CblasNoTrans, 1.0, w, u, 0.0, v));
  push_vector (p, v);
  gsl_vector_free (u);
  gsl_vector_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([] REAL, [, ] REAL) [] REAL

void genie_vector_times_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *w = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_transpose (w));
  gsl_vector *u = pop_vector (p, A68_TRUE);
  int len = (int) (SIZE (u));
  gsl_vector *v = gsl_vector_calloc ((size_t) len);
  gsl_vector_set_zero (v);
  ASSERT_GSL (gsl_blas_dgemv (CblasNoTrans, 1.0, w, u, 0.0, v));
  push_vector (p, v);
  gsl_vector_free (u);
  gsl_vector_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([, ] REAL, [, ] REAL) [, ] REAL

void genie_matrix_times_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_matrix *v = pop_matrix (p, A68_TRUE);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  int len2 = (int) (SIZE2 (v)), len1 = (int) (SIZE1 (u));
  gsl_matrix *w = gsl_matrix_calloc ((size_t) len1, (size_t) len2);
  ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 1.0, u, v, 0.0, w));
  push_matrix (p, w);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([, ] COMPLEX, [] COMPLEX) [] COMPLEX

void genie_matrix_complex_times_vector (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_complex zero, one;
  GSL_SET_COMPLEX (&zero, 0.0, 0.0);
  GSL_SET_COMPLEX (&one, 1.0, 0.0);
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  gsl_matrix_complex *w = pop_matrix_complex (p, A68_TRUE);
  int len = (int) (SIZE (u));
  gsl_vector_complex *v = gsl_vector_complex_calloc ((size_t) len);
  ASSERT_GSL (gsl_blas_zgemv (CblasNoTrans, one, w, u, zero, v));
  push_vector_complex (p, v);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  gsl_matrix_complex_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([] COMPLEX, [, ] COMPLEX) [] COMPLEX

void genie_vector_complex_times_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_complex zero, one;
  GSL_SET_COMPLEX (&zero, 0.0, 0.0);
  GSL_SET_COMPLEX (&one, 1.0, 0.0);
  gsl_matrix_complex *w = pop_matrix_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_matrix_complex_transpose (w));
  gsl_vector_complex *u = pop_vector_complex (p, A68_TRUE);
  int len = (int) (SIZE (u));
  gsl_vector_complex *v = gsl_vector_complex_calloc ((size_t) len);
  ASSERT_GSL (gsl_blas_zgemv (CblasNoTrans, one, w, u, zero, v));
  push_vector_complex (p, v);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  gsl_matrix_complex_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP * = ([, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX

void genie_matrix_complex_times_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_complex zero, one;
  GSL_SET_COMPLEX (&zero, 0.0, 0.0);
  GSL_SET_COMPLEX (&one, 1.0, 0.0);
  gsl_matrix_complex *v = pop_matrix_complex (p, A68_TRUE);
  gsl_matrix_complex *u = pop_matrix_complex (p, A68_TRUE);
  int len1 = (int) (SIZE2 (v)), len2 = (int) (SIZE1 (u));
  gsl_matrix_complex *w = gsl_matrix_complex_calloc ((size_t) len1, (size_t) len2);
  ASSERT_GSL (gsl_blas_zgemm (CblasNoTrans, CblasNoTrans, one, u, v, zero, w));
  gsl_matrix_complex_free (u);
  gsl_matrix_complex_free (v);
  push_matrix_complex (p, w);
  gsl_matrix_complex_free (w);
  (void) gsl_set_error_handler (save_handler);
}

#endif
