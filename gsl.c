/*!
\file gsl.c
\brief vector, matrix and FFT support through GSL
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2011 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#if (defined HAVE_GSL_GSL_BLAS_H && defined HAVE_LIBGSL)

#include "a68g.h"

#include <gsl/gsl_blas.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_fft_complex.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_sf.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_integration.h>

static NODE_T *error_node = NULL;

#define VECTOR_OFFSET(a, t)\
  ((LWB (t) * (t)->span - (t)->shift + (a)->slice_offset) * (a)->elem_size + (a)->field_offset)

#define MATRIX_OFFSET(a, t1, t2)\
  ((LWB (t1) * (t1)->span - (t1)->shift + LWB (t2) * (t2)->span - (t2)->shift + (a)->slice_offset ) * (a)->elem_size + (a)->field_offset)

/*!
\brief set permutation vector element - function fails in gsl
\param p permutation vector
\param i element iindex
\param j element value
**/

void gsl_permutation_set (const gsl_permutation * p, const size_t i, const size_t j)
{
  p->data[i] = j;
}

/*!
\brief map GSL error handler onto a68g error handler
\param reason error text
\param file gsl file where error occured
\param line line in above file
\param int gsl error number
**/

void torrix_error_handler (const char *reason, const char *file, int line, int gsl_errno)
{
  if (line != 0) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s in line %d of file %s", reason, line, file) >= 0);
  } else {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s", reason) >= 0);
  }
  diagnostic_node (A68_RUNTIME_ERROR, error_node, ERROR_TORRIX, edit_line, gsl_strerror (gsl_errno), NULL);
  exit_genie (error_node, A68_RUNTIME_ERROR);
}

/*!
\brief detect math errors, mainly in BLAS functions
\param rc return code from function
**/

static void torrix_test_error (int rc)
{
  if (rc != 0) {
    torrix_error_handler ("math error", "", 0, rc);
  }
}

/*!
\brief pop [] INT on the stack as gsl_permutation
\param p position in tree
\param get whether to get elements from row in the stack
\return gsl_permutation_complex
**/

static gsl_permutation *pop_permutation (NODE_T * p, BOOL_T get)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int len, inc, iindex, k;
  BYTE_T *base;
  gsl_permutation *v;
/* Pop arguments */
  POP_REF (p, &desc);
  CHECK_REF (p, desc, MODE (ROW_INT));
  GET_DESCRIPTOR (arr, tup, &desc);
  len = ROW_SIZE (tup);
  v = gsl_permutation_alloc ((size_t) len);
  if (get) {
    base = DEREF (BYTE_T, &(ARRAY (arr)));
    iindex = VECTOR_OFFSET (arr, tup);
    inc = tup->span * arr->elem_size;
    for (k = 0; k < len; k++, iindex += inc) {
      A68_INT *x = (A68_INT *) (base + iindex);
      CHECK_INIT (p, INITIALISED (x), MODE (INT));
      gsl_permutation_set (v, (size_t) k, (size_t) VALUE (x));
    }
  }
  return (v);
}

/*!
\brief push gsl_permutation on the stack as [] INT
\param p position in tree
**/

static void push_permutation (NODE_T * p, gsl_permutation * v)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int len, inc, iindex, k;
  BYTE_T *base;
  len = (int) (v->size);
  desc = heap_generator (p, MODE (ROW_INT), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  BLOCK_GC_HANDLE (&desc);
  row = heap_generator (p, MODE (ROW_INT), len * ALIGNED_SIZE_OF (A68_INT));
  BLOCK_GC_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (INT);
  arr.elem_size = ALIGNED_SIZE_OF (A68_INT);
  arr.slice_offset = arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = len;
  tup.span = 1;
  tup.shift = LWB (&tup);
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &desc);
  base = DEREF (BYTE_T, &(ARRAY (&arr)));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = tup.span * arr.elem_size;
  for (k = 0; k < len; k++, iindex += inc) {
    A68_INT *x = (A68_INT *) (base + iindex);
    STATUS (x) = INITIALISED_MASK;
    VALUE (x) = (int) gsl_permutation_get (v, (size_t) k);
  }
  UNBLOCK_GC_HANDLE (&desc);
  UNBLOCK_GC_HANDLE (&row);
  PUSH_REF (p, desc);
}

/*!
\brief pop [] REAL on the stack as gsl_vector
\param p position in tree
\param get whether to get elements from row in the stack
\return gsl_vector_complex
**/

static gsl_vector *pop_vector (NODE_T * p, BOOL_T get)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int len, inc, iindex, k;
  BYTE_T *base;
  gsl_vector *v;
/* Pop arguments */
  POP_REF (p, &desc);
  CHECK_REF (p, desc, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr, tup, &desc);
  len = ROW_SIZE (tup);
  v = gsl_vector_alloc ((size_t) len);
  if (get) {
    base = DEREF (BYTE_T, &(ARRAY (arr)));
    iindex = VECTOR_OFFSET (arr, tup);
    inc = tup->span * arr->elem_size;
    for (k = 0; k < len; k++, iindex += inc) {
      A68_REAL *x = (A68_REAL *) (base + iindex);
      CHECK_INIT (p, INITIALISED (x), MODE (REAL));
      gsl_vector_set (v, (size_t) k, VALUE (x));
    }
  }
  return (v);
}

/*!
\brief push gsl_vector on the stack as [] REAL
\param p position in tree
**/

static void push_vector (NODE_T * p, gsl_vector * v)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int len, inc, iindex, k;
  BYTE_T *base;
  len = (int) (v->size);
  desc = heap_generator (p, MODE (ROW_REAL), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  BLOCK_GC_HANDLE (&desc);
  row = heap_generator (p, MODE (ROW_REAL), len * ALIGNED_SIZE_OF (A68_REAL));
  BLOCK_GC_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (REAL);
  arr.elem_size = ALIGNED_SIZE_OF (A68_REAL);
  arr.slice_offset = arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = len;
  tup.span = 1;
  tup.shift = LWB (&tup);
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &desc);
  base = DEREF (BYTE_T, &(ARRAY (&arr)));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = tup.span * arr.elem_size;
  for (k = 0; k < len; k++, iindex += inc) {
    A68_REAL *x = (A68_REAL *) (base + iindex);
    STATUS (x) = INITIALISED_MASK;
    VALUE (x) = gsl_vector_get (v, (size_t) k);
    CHECK_REAL_REPRESENTATION (p, VALUE (x));
  }
  UNBLOCK_GC_HANDLE (&desc);
  UNBLOCK_GC_HANDLE (&row);
  PUSH_REF (p, desc);
}

/*!
\brief pop [,] REAL on the stack as gsl_matrix
\param p position in tree
\param get whether to get elements from row in the stack
\return gsl_matrix
**/

static gsl_matrix *pop_matrix (NODE_T * p, BOOL_T get)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup1, *tup2;
  int len1, len2, inc1, inc2, iindex1, iindex2, k1, k2;
  BYTE_T *base;
  gsl_matrix *a;
/* Pop arguments */
  POP_REF (p, &desc);
  CHECK_REF (p, desc, MODE (ROWROW_REAL));
  GET_DESCRIPTOR (arr, tup1, &desc);
  tup2 = &(tup1[1]);
  len1 = ROW_SIZE (tup1);
  len2 = ROW_SIZE (tup2);
  a = gsl_matrix_alloc ((size_t) len1, (size_t) len2);
  if (get) {
    base = DEREF (BYTE_T, &(ARRAY (arr)));
    iindex1 = MATRIX_OFFSET (arr, tup1, tup2);
    inc1 = tup1->span * arr->elem_size;
    inc2 = tup2->span * arr->elem_size;
    for (k1 = 0; k1 < len1; k1++, iindex1 += inc1) {
      for (k2 = 0, iindex2 = iindex1; k2 < len2; k2++, iindex2 += inc2) {
        A68_REAL *x = (A68_REAL *) (base + iindex2);
        CHECK_INIT (p, INITIALISED (x), MODE (REAL));
        gsl_matrix_set (a, (size_t) k1, (size_t) k2, VALUE (x));
      }
    }
  }
  return (a);
}

/*!
\brief push gsl_matrix on the stack as [,] REAL
\param p position in tree
**/

static void push_matrix (NODE_T * p, gsl_matrix * a)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup1, tup2;
  int len1, len2, inc1, inc2, iindex1, iindex2, k1, k2;
  BYTE_T *base;
  len1 = (int) (a->size1);
  len2 = (int) (a->size2);
  desc = heap_generator (p, MODE (ROWROW_REAL), ALIGNED_SIZE_OF (A68_ARRAY) + 2 * ALIGNED_SIZE_OF (A68_TUPLE));
  BLOCK_GC_HANDLE (&desc);
  row = heap_generator (p, MODE (ROWROW_REAL), len1 * len2 * ALIGNED_SIZE_OF (A68_REAL));
  BLOCK_GC_HANDLE (&row);
  DIM (&arr) = 2;
  MOID (&arr) = MODE (REAL);
  arr.elem_size = ALIGNED_SIZE_OF (A68_REAL);
  arr.slice_offset = arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup1) = 1;
  UPB (&tup1) = len1;
  tup1.span = 1;
  tup1.shift = LWB (&tup1);
  tup1.k = 0;
  LWB (&tup2) = 1;
  UPB (&tup2) = len2;
  tup2.span = ROW_SIZE (&tup1);
  tup2.shift = LWB (&tup2) * tup2.span;
  tup2.k = 0;
  PUT_DESCRIPTOR2 (arr, tup1, tup2, &desc);
  base = DEREF (BYTE_T, &(ARRAY (&arr)));
  iindex1 = MATRIX_OFFSET (&arr, &tup1, &tup2);
  inc1 = tup1.span * arr.elem_size;
  inc2 = tup2.span * arr.elem_size;
  for (k1 = 0; k1 < len1; k1++, iindex1 += inc1) {
    for (k2 = 0, iindex2 = iindex1; k2 < len2; k2++, iindex2 += inc2) {
      A68_REAL *x = (A68_REAL *) (base + iindex2);
      STATUS (x) = INITIALISED_MASK;
      VALUE (x) = gsl_matrix_get (a, (size_t) k1, (size_t) k2);
      CHECK_REAL_REPRESENTATION (p, VALUE (x));
    }
  }
  UNBLOCK_GC_HANDLE (&desc);
  UNBLOCK_GC_HANDLE (&row);
  PUSH_REF (p, desc);
}

/*!
\brief pop [] COMPLEX on the stack as gsl_vector_complex
\param p position in tree
\param get whether to get elements from row in the stack
\return gsl_vector_complex
**/

static gsl_vector_complex *pop_vector_complex (NODE_T * p, BOOL_T get)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int len, inc, iindex, k;
  BYTE_T *base;
  gsl_vector_complex *v;
/* Pop arguments */
  POP_REF (p, &desc);
  CHECK_REF (p, desc, MODE (ROW_COMPLEX));
  GET_DESCRIPTOR (arr, tup, &desc);
  len = ROW_SIZE (tup);
  v = gsl_vector_complex_alloc ((size_t) len);
  if (get) {
    base = DEREF (BYTE_T, &(ARRAY (arr)));
    iindex = VECTOR_OFFSET (arr, tup);
    inc = tup->span * arr->elem_size;
    for (k = 0; k < len; k++, iindex += inc) {
      A68_REAL *re = (A68_REAL *) (base + iindex);
      A68_REAL *im = (A68_REAL *) (base + iindex + ALIGNED_SIZE_OF (A68_REAL));
      gsl_complex z;
      CHECK_INIT (p, INITIALISED (re), MODE (COMPLEX));
      CHECK_INIT (p, INITIALISED (im), MODE (COMPLEX));
      GSL_SET_COMPLEX (&z, VALUE (re), VALUE (im));
      gsl_vector_complex_set (v, (size_t) k, z);
    }
  }
  return (v);
}

/*!
\brief push gsl_vector_complex on the stack as [] COMPLEX
\param p position in tree
**/

static void push_vector_complex (NODE_T * p, gsl_vector_complex * v)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int len, inc, iindex, k;
  BYTE_T *base;
  len = (int) (v->size);
  desc = heap_generator (p, MODE (ROW_COMPLEX), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  BLOCK_GC_HANDLE (&desc);
  row = heap_generator (p, MODE (ROW_COMPLEX), len * 2 * ALIGNED_SIZE_OF (A68_REAL));
  BLOCK_GC_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (COMPLEX);
  arr.elem_size = 2 * ALIGNED_SIZE_OF (A68_REAL);
  arr.slice_offset = arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = len;
  tup.span = 1;
  tup.shift = LWB (&tup);
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &desc);
  base = DEREF (BYTE_T, &(ARRAY (&arr)));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = tup.span * arr.elem_size;
  for (k = 0; k < len; k++, iindex += inc) {
    A68_REAL *re = (A68_REAL *) (base + iindex);
    A68_REAL *im = (A68_REAL *) (base + iindex + ALIGNED_SIZE_OF (A68_REAL));
    gsl_complex z = gsl_vector_complex_get (v, (size_t) k);
    STATUS (re) = INITIALISED_MASK;
    VALUE (re) = GSL_REAL (z);
    STATUS (im) = INITIALISED_MASK;
    VALUE (im) = GSL_IMAG (z);
    CHECK_COMPLEX_REPRESENTATION (p, VALUE (re), VALUE (im));
  }
  UNBLOCK_GC_HANDLE (&desc);
  UNBLOCK_GC_HANDLE (&row);
  PUSH_REF (p, desc);
}

/*!
\brief pop [,] COMPLEX on the stack as gsl_matrix_complex
\param p position in tree
\param get whether to get elements from row in the stack
\return gsl_matrix_complex
**/

static gsl_matrix_complex *pop_matrix_complex (NODE_T * p, BOOL_T get)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup1, *tup2;
  int len1, len2;
  gsl_matrix_complex *a;
/* Pop arguments */
  POP_REF (p, &desc);
  CHECK_REF (p, desc, MODE (ROWROW_COMPLEX));
  GET_DESCRIPTOR (arr, tup1, &desc);
  tup2 = &(tup1[1]);
  len1 = ROW_SIZE (tup1);
  len2 = ROW_SIZE (tup2);
  a = gsl_matrix_complex_alloc ((size_t) len1, (size_t) len2);
  if (get) {
    BYTE_T *base = DEREF (BYTE_T, &(ARRAY (arr)));
    int iindex1 = MATRIX_OFFSET (arr, tup1, tup2);
    int inc1 = tup1->span * arr->elem_size, inc2 = tup2->span * arr->elem_size, k1;
    for (k1 = 0; k1 < len1; k1++, iindex1 += inc1) {
      int iindex2, k2;
      for (k2 = 0, iindex2 = iindex1; k2 < len2; k2++, iindex2 += inc2) {
        A68_REAL *re = (A68_REAL *) (base + iindex2);
        A68_REAL *im = (A68_REAL *) (base + iindex2 + ALIGNED_SIZE_OF (A68_REAL));
        gsl_complex z;
        CHECK_INIT (p, INITIALISED (re), MODE (COMPLEX));
        CHECK_INIT (p, INITIALISED (im), MODE (COMPLEX));
        GSL_SET_COMPLEX (&z, VALUE (re), VALUE (im));
        gsl_matrix_complex_set (a, (size_t) k1, (size_t) k2, z);
      }
    }
  }
  return (a);
}

/*!
\brief push gsl_matrix_complex on the stack as [,] COMPLEX
\param p position in tree
**/

static void push_matrix_complex (NODE_T * p, gsl_matrix_complex * a)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup1, tup2;
  int len1, len2, inc1, inc2, iindex1, iindex2, k1, k2;
  BYTE_T *base;
  len1 = (int) (a->size1);
  len2 = (int) (a->size2);
  desc = heap_generator (p, MODE (ROWROW_COMPLEX), ALIGNED_SIZE_OF (A68_ARRAY) + 2 * ALIGNED_SIZE_OF (A68_TUPLE));
  BLOCK_GC_HANDLE (&desc);
  row = heap_generator (p, MODE (ROWROW_COMPLEX), len1 * len2 * 2 * ALIGNED_SIZE_OF (A68_REAL));
  BLOCK_GC_HANDLE (&row);
  DIM (&arr) = 2;
  MOID (&arr) = MODE (COMPLEX);
  arr.elem_size = 2 * ALIGNED_SIZE_OF (A68_REAL);
  arr.slice_offset = arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup1) = 1;
  UPB (&tup1) = len1;
  tup1.span = 1;
  tup1.shift = LWB (&tup1);
  tup1.k = 0;
  LWB (&tup2) = 1;
  UPB (&tup2) = len2;
  tup2.span = ROW_SIZE (&tup1);
  tup2.shift = LWB (&tup2) * tup2.span;
  tup2.k = 0;
  PUT_DESCRIPTOR2 (arr, tup1, tup2, &desc);
  base = DEREF (BYTE_T, &(ARRAY (&arr)));
  iindex1 = MATRIX_OFFSET (&arr, &tup1, &tup2);
  inc1 = tup1.span * arr.elem_size;
  inc2 = tup2.span * arr.elem_size;
  for (k1 = 0; k1 < len1; k1++, iindex1 += inc1) {
    for (k2 = 0, iindex2 = iindex1; k2 < len2; k2++, iindex2 += inc2) {
      A68_REAL *re = (A68_REAL *) (base + iindex2);
      A68_REAL *im = (A68_REAL *) (base + iindex2 + ALIGNED_SIZE_OF (A68_REAL));
      gsl_complex z = gsl_matrix_complex_get (a, (size_t) k1, (size_t) k2);
      STATUS (re) = INITIALISED_MASK;
      VALUE (re) = GSL_REAL (z);
      STATUS (im) = INITIALISED_MASK;
      VALUE (im) = GSL_IMAG (z);
      CHECK_COMPLEX_REPRESENTATION (p, VALUE (re), VALUE (im));
    }
  }
  UNBLOCK_GC_HANDLE (&desc);
  UNBLOCK_GC_HANDLE (&row);
  PUSH_REF (p, desc);
}

/*!
\brief pop REF [...] M and derefence to [...] M
\param p position in tree
\param m mode of REF [...] M
\param par_size size of parameters in the stack
\return the undereferenced REF
**/

static A68_REF dereference_ref_row (NODE_T * p, MOID_T * m, ADDR_T par_size)
{
  A68_REF *u, v;
  u = (A68_REF *) STACK_OFFSET (-par_size);
  v = *u;
  CHECK_REF (p, v, m);
  *u = *DEREF (A68_ROW, &v);
  return (v);
}

/*!
\brief generically perform operation and assign result (+:=, -:=, ...) 
\param p position in tree
\param m mode of REF [...] M
\param n mode of right operand M
\param op operation to be performed
**/

static void op_ab (NODE_T * p, MOID_T * m, MOID_T * n, GENIE_PROC * op)
{
  ADDR_T par_size = MOID_SIZE (m) + MOID_SIZE (n);
  A68_REF u, *v;
  error_node = p;
  u = dereference_ref_row (p, m, par_size);
  v = (A68_REF *) STACK_OFFSET (-par_size);
  (*op) (p);
  *DEREF (A68_ROW, &u) = *v;
  *v = u;
}

/*!
\brief PROC vector echo = ([] REAL) [] REAL
\param p position in tree
**/

void genie_vector_echo (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u;
  error_node = p;
  u = pop_vector (p, A68_TRUE);
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC matrix echo = ([,] REAL) [,] REAL
\param p position in tree
**/

void genie_matrix_echo (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a;
  error_node = p;
  a = pop_matrix (p, A68_TRUE);
  push_matrix (p, a);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC complex vector echo = ([] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_echo (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u;
  error_node = p;
  u = pop_vector_complex (p, A68_TRUE);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC complex matrix echo = ([,] COMPLEX) [,] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_echo (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *a;
  error_node = p;
  a = pop_matrix_complex (p, A68_TRUE);
  push_matrix_complex (p, a);
  gsl_matrix_complex_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP - = ([] REAL) [] REAL
\param p position in tree
**/

void genie_vector_minus (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u;
  int rc;
  error_node = p;
  u = pop_vector (p, A68_TRUE);
  rc = gsl_vector_scale (u, -1);
  torrix_test_error (rc);
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP - = ([,] REAL) [,] REAL
\param p position in tree
**/

void genie_matrix_minus (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a;
  int rc;
  error_node = p;
  a = pop_matrix (p, A68_TRUE);
  rc = gsl_matrix_scale (a, -1);
  torrix_test_error (rc);
  push_matrix (p, a);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP T = ([,] REAL) [,] REAL
\param p position in tree
**/

void genie_matrix_transpose (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a;
  int rc;
  error_node = p;
  a = pop_matrix (p, A68_TRUE);
  rc = gsl_matrix_transpose (a);
  torrix_test_error (rc);
  push_matrix (p, a);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP T = ([,] COMPLEX) [,] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_transpose (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *a;
  int rc;
  error_node = p;
  a = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_matrix_complex_transpose (a);
  torrix_test_error (rc);
  push_matrix_complex (p, a);
  gsl_matrix_complex_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP INV = ([,] REAL) [,] REAL
\param p position in tree
**/

void genie_matrix_inv (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix *u, *inv;
  int rc, signum;
  error_node = p;
  u = pop_matrix (p, A68_TRUE);
  q = gsl_permutation_alloc (u->size1);
  rc = gsl_linalg_LU_decomp (u, q, &signum);
  torrix_test_error (rc);
  inv = gsl_matrix_alloc (u->size1, u->size2);
  rc = gsl_linalg_LU_invert (u, q, inv);
  torrix_test_error (rc);
  push_matrix (p, inv);
  gsl_matrix_free (inv);
  gsl_matrix_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP INV = ([,] COMPLEX) [,] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_inv (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix_complex *u, *inv;
  int rc, signum;
  error_node = p;
  u = pop_matrix_complex (p, A68_TRUE);
  q = gsl_permutation_alloc (u->size1);
  rc = gsl_linalg_complex_LU_decomp (u, q, &signum);
  torrix_test_error (rc);
  inv = gsl_matrix_complex_alloc (u->size1, u->size2);
  rc = gsl_linalg_complex_LU_invert (u, q, inv);
  torrix_test_error (rc);
  push_matrix_complex (p, inv);
  gsl_matrix_complex_free (inv);
  gsl_matrix_complex_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP DET = ([,] REAL) REAL
\param p position in tree
**/

void genie_matrix_det (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix *u;
  int rc, signum;
  error_node = p;
  u = pop_matrix (p, A68_TRUE);
  q = gsl_permutation_alloc (u->size1);
  rc = gsl_linalg_LU_decomp (u, q, &signum);
  torrix_test_error (rc);
  PUSH_PRIMITIVE (p, gsl_linalg_LU_det (u, signum), A68_REAL);
  gsl_matrix_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP DET = ([,] COMPLEX) COMPLEX
\param p position in tree
**/

void genie_matrix_complex_det (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix_complex *u;
  int rc, signum;
  gsl_complex det;
  error_node = p;
  u = pop_matrix_complex (p, A68_TRUE);
  q = gsl_permutation_alloc (u->size1);
  rc = gsl_linalg_complex_LU_decomp (u, q, &signum);
  torrix_test_error (rc);
  det = gsl_linalg_complex_LU_det (u, signum);
  PUSH_PRIMITIVE (p, GSL_REAL (det), A68_REAL);
  PUSH_PRIMITIVE (p, GSL_IMAG (det), A68_REAL);
  gsl_matrix_complex_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP TRACE = ([,] REAL) REAL
\param p position in tree
**/

void genie_matrix_trace (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a;
  double sum;
  int len1, len2, k;
  error_node = p;
  a = pop_matrix (p, A68_TRUE);
  len1 = (int) (a->size1);
  len2 = (int) (a->size2);
  if (len1 != len2) {
    torrix_error_handler ("cannot calculate trace", __FILE__, __LINE__, GSL_ENOTSQR);
  }
  sum = 0.0;
  for (k = 0; k < len1; k++) {
    sum += gsl_matrix_get (a, (size_t) k, (size_t) k);
  }
  PUSH_PRIMITIVE (p, sum, A68_REAL);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP TRACE = ([,] COMPLEX) COMPLEX
\param p position in tree
**/

void genie_matrix_complex_trace (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *a;
  gsl_complex sum;
  int len1, len2, k;
  error_node = p;
  a = pop_matrix_complex (p, A68_TRUE);
  len1 = (int) (a->size1);
  len2 = (int) (a->size2);
  if (len1 != len2) {
    torrix_error_handler ("cannot calculate trace", __FILE__, __LINE__, GSL_ENOTSQR);
  }
  GSL_SET_COMPLEX (&sum, 0.0, 0.0);
  for (k = 0; k < len1; k++) {
    sum = gsl_complex_add (sum, gsl_matrix_complex_get (a, (size_t) k, (size_t) k));
  }
  PUSH_PRIMITIVE (p, GSL_REAL (sum), A68_REAL);
  PUSH_PRIMITIVE (p, GSL_IMAG (sum), A68_REAL);
  gsl_matrix_complex_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP - = ([] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_minus (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u;
  error_node = p;
  u = pop_vector_complex (p, A68_TRUE);
  gsl_blas_zdscal (-1, u);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP - = ([,] COMPLEX) [,] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_minus (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *a;
  gsl_complex one;
  int rc;
  error_node = p;
  GSL_SET_COMPLEX (&one, -1.0, 0.0);
  a = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_matrix_complex_scale (a, one);
  torrix_test_error (rc);
  push_matrix_complex (p, a);
  gsl_matrix_complex_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP + = ([] REAL, [] REAL) [] REAL
\param p position in tree
**/

void genie_vector_add (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u, *v;
  int rc;
  error_node = p;
  v = pop_vector (p, A68_TRUE);
  u = pop_vector (p, A68_TRUE);
  rc = gsl_vector_add (u, v);
  torrix_test_error (rc);
  push_vector (p, u);
  gsl_vector_free (u);
  gsl_vector_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP - = ([] REAL, [] REAL) [] REAL
\param p position in tree
**/

void genie_vector_sub (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u, *v;
  int rc;
  error_node = p;
  v = pop_vector (p, A68_TRUE);
  u = pop_vector (p, A68_TRUE);
  rc = gsl_vector_sub (u, v);
  torrix_test_error (rc);
  push_vector (p, u);
  gsl_vector_free (u);
  gsl_vector_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP = = ([] REAL, [] REAL) BOOL
\param p position in tree
**/

void genie_vector_eq (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u, *v;
  int rc;
  error_node = p;
  v = pop_vector (p, A68_TRUE);
  u = pop_vector (p, A68_TRUE);
  rc = gsl_vector_sub (u, v);
  torrix_test_error (rc);
  PUSH_PRIMITIVE (p, (BOOL_T) (gsl_vector_isnull (u) ? A68_TRUE : A68_FALSE), A68_BOOL);
  gsl_vector_free (u);
  gsl_vector_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP /= = ([] REAL, [] REAL) BOOL
\param p position in tree
**/

void genie_vector_ne (NODE_T * p)
{
  genie_vector_eq (p);
  genie_not_bool (p);
}

/*!
\brief OP +:= = (REF [] REAL, [] REAL) REF [] REAL
\param p position in tree
**/

void genie_vector_plusab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROW_REAL), MODE (ROW_REAL), genie_vector_add);
}

/*!
\brief OP -:= = (REF [] REAL, [] REAL) REF [] REAL
\param p position in tree
**/

void genie_vector_minusab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROW_REAL), MODE (ROW_REAL), genie_vector_sub);
}

/*!
\brief OP + = ([, ] REAL, [, ] REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_add (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *u, *v;
  int rc;
  error_node = p;
  v = pop_matrix (p, A68_TRUE);
  u = pop_matrix (p, A68_TRUE);
  rc = gsl_matrix_add (u, v);
  torrix_test_error (rc);
  push_matrix (p, u);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP - = ([, ] REAL, [, ] REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_sub (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *u, *v;
  int rc;
  error_node = p;
  v = pop_matrix (p, A68_TRUE);
  u = pop_matrix (p, A68_TRUE);
  rc = gsl_matrix_sub (u, v);
  torrix_test_error (rc);
  push_matrix (p, u);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP = = ([, ] REAL, [, ] REAL) [, ] BOOL
\param p position in tree
**/

void genie_matrix_eq (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *u, *v;
  int rc;
  error_node = p;
  v = pop_matrix (p, A68_TRUE);
  u = pop_matrix (p, A68_TRUE);
  rc = gsl_matrix_sub (u, v);
  torrix_test_error (rc);
  PUSH_PRIMITIVE (p, (BOOL_T) (gsl_matrix_isnull (u) ? A68_TRUE : A68_FALSE), A68_BOOL);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP /= = ([, ] REAL, [, ] REAL) [, ] BOOL
\param p position in tree
**/

void genie_matrix_ne (NODE_T * p)
{
  genie_matrix_eq (p);
  genie_not_bool (p);
}

/*!
\brief OP +:= = (REF [, ] REAL, [, ] REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_plusab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROWROW_REAL), MODE (ROWROW_REAL), genie_matrix_add);
}

/*!
\brief OP -:= = (REF [, ] REAL, [, ] REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_minusab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROWROW_REAL), MODE (ROWROW_REAL), genie_matrix_sub);
}

/*!
\brief OP + = ([] COMPLEX, [] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_add (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u, *v;
  gsl_complex one;
  int rc;
  error_node = p;
  GSL_SET_COMPLEX (&one, 1.0, 0.0);
  v = pop_vector_complex (p, A68_TRUE);
  u = pop_vector_complex (p, A68_TRUE);
  rc = gsl_blas_zaxpy (one, u, v);
  torrix_test_error (rc);
  push_vector_complex (p, v);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP - = ([] COMPLEX, [] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_sub (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u, *v;
  gsl_complex one;
  int rc;
  error_node = p;
  GSL_SET_COMPLEX (&one, -1.0, 0.0);
  v = pop_vector_complex (p, A68_TRUE);
  u = pop_vector_complex (p, A68_TRUE);
  rc = gsl_blas_zaxpy (one, v, u);
  torrix_test_error (rc);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP = = ([] COMPLEX, [] COMPLEX) BOOL
\param p position in tree
**/

void genie_vector_complex_eq (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u, *v;
  gsl_complex one;
  int rc;
  error_node = p;
  GSL_SET_COMPLEX (&one, -1.0, 0.0);
  v = pop_vector_complex (p, A68_TRUE);
  u = pop_vector_complex (p, A68_TRUE);
  rc = gsl_blas_zaxpy (one, v, u);
  torrix_test_error (rc);
  PUSH_PRIMITIVE (p, (BOOL_T) (gsl_vector_complex_isnull (u) ? A68_TRUE : A68_FALSE), A68_BOOL);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP /= = ([] COMPLEX, [] COMPLEX) BOOL
\param p position in tree
**/

void genie_vector_complex_ne (NODE_T * p)
{
  genie_vector_complex_eq (p);
  genie_not_bool (p);
}

/*!
\brief OP +:= = (REF [] COMPLEX, [] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_plusab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROW_COMPLEX), MODE (ROW_COMPLEX), genie_vector_complex_add);
}

/*!
\brief OP -:= = (REF [] COMPLEX, [] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_minusab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROW_COMPLEX), MODE (ROW_COMPLEX), genie_vector_complex_sub);
}

/*!
\brief OP + = ([, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_add (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *u, *v;
  int rc;
  error_node = p;
  v = pop_matrix_complex (p, A68_TRUE);
  u = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_matrix_complex_add (u, v);
  torrix_test_error (rc);
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  gsl_matrix_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP - = ([, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_sub (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *u, *v;
  int rc;
  error_node = p;
  v = pop_matrix_complex (p, A68_TRUE);
  u = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_matrix_complex_sub (u, v);
  torrix_test_error (rc);
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  gsl_matrix_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP = = ([, ] COMPLEX, [, ] COMPLEX) BOOL
\param p position in tree
**/

void genie_matrix_complex_eq (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *u, *v;
  int rc;
  error_node = p;
  v = pop_matrix_complex (p, A68_TRUE);
  u = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_matrix_complex_sub (u, v);
  torrix_test_error (rc);
  PUSH_PRIMITIVE (p, (BOOL_T) (gsl_matrix_complex_isnull (u) ? A68_TRUE : A68_FALSE), A68_BOOL);
  gsl_matrix_complex_free (u);
  gsl_matrix_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP /= = ([, ] COMPLEX, [, ] COMPLEX) BOOL
\param p position in tree
**/

void genie_matrix_complex_ne (NODE_T * p)
{
  genie_matrix_complex_eq (p);
  genie_not_bool (p);
}

/*!
\brief OP +:= = (REF [, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_plusab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), genie_matrix_complex_add);
}

/*!
\brief OP -:= = (REF [, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_minusab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), genie_matrix_complex_sub);
}

/*!
\brief OP * = ([] REAL, REAL) [] REAL
\param p position in tree
**/

void genie_vector_scale_real (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u;
  A68_REAL v;
  int rc;
  error_node = p;
  POP_OBJECT (p, &v, A68_REAL);
  u = pop_vector (p, A68_TRUE);
  rc = gsl_vector_scale (u, VALUE (&v));
  torrix_test_error (rc);
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = (REAL, [] REAL) [] REAL
\param p position in tree
**/

void genie_real_scale_vector (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u;
  A68_REAL v;
  int rc;
  error_node = p;
  u = pop_vector (p, A68_TRUE);
  POP_OBJECT (p, &v, A68_REAL);
  rc = gsl_vector_scale (u, VALUE (&v));
  torrix_test_error (rc);
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([, ] REAL, REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_scale_real (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *u;
  A68_REAL v;
  int rc;
  error_node = p;
  POP_OBJECT (p, &v, A68_REAL);
  u = pop_matrix (p, A68_TRUE);
  rc = gsl_matrix_scale (u, VALUE (&v));
  torrix_test_error (rc);
  push_matrix (p, u);
  gsl_matrix_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = (REAL, [, ] REAL) [, ] REAL
\param p position in tree
**/

void genie_real_scale_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *u;
  A68_REAL v;
  int rc;
  error_node = p;
  u = pop_matrix (p, A68_TRUE);
  POP_OBJECT (p, &v, A68_REAL);
  rc = gsl_matrix_scale (u, VALUE (&v));
  torrix_test_error (rc);
  push_matrix (p, u);
  gsl_matrix_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([] COMPLEX, COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_scale_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u;
  A68_REAL re, im;
  gsl_complex v;
  error_node = p;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  u = pop_vector_complex (p, A68_TRUE);
  gsl_blas_zscal (v, u);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = (COMPLEX, [] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_complex_scale_vector_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u;
  A68_REAL re, im;
  gsl_complex v;
  error_node = p;
  u = pop_vector_complex (p, A68_TRUE);
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  gsl_blas_zscal (v, u);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([, ] COMPLEX, COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_scale_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *u;
  A68_REAL re, im;
  gsl_complex v;
  int rc;
  error_node = p;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  u = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_matrix_complex_scale (u, v);
  torrix_test_error (rc);
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = (COMPLEX, [, ] COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_complex_scale_matrix_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *u;
  A68_REAL re, im;
  gsl_complex v;
  int rc;
  error_node = p;
  u = pop_matrix_complex (p, A68_TRUE);
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  rc = gsl_matrix_complex_scale (u, v);
  torrix_test_error (rc);
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP *:= (REF [] REAL, REAL) REF [] REAL
\param p position in tree
**/

void genie_vector_scale_real_ab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROW_REAL), MODE (REAL), genie_vector_scale_real);
}

/*!
\brief OP *:= (REF [, ] REAL, REAL) REF [, ] REAL
\param p position in tree
**/

void genie_matrix_scale_real_ab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROWROW_REAL), MODE (REAL), genie_matrix_scale_real);
}

/*!
\brief OP *:= (REF [] COMPLEX, COMPLEX) REF [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_scale_complex_ab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROW_COMPLEX), MODE (COMPLEX), genie_vector_complex_scale_complex);
}

/*!
\brief OP *:= (REF [, ] COMPLEX, COMPLEX) REF [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_scale_complex_ab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROWROW_COMPLEX), MODE (COMPLEX), genie_matrix_complex_scale_complex);
}

/*!
\brief OP / = ([] REAL, REAL) [] REAL
\param p position in tree
**/

void genie_vector_div_real (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u;
  A68_REAL v;
  int rc;
  error_node = p;
  POP_OBJECT (p, &v, A68_REAL);
  if (VALUE (&v) == 0.0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (ROW_REAL));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  u = pop_vector (p, A68_TRUE);
  rc = gsl_vector_scale (u, 1.0 / VALUE (&v));
  torrix_test_error (rc);
  push_vector (p, u);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP / = ([, ] REAL, REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_div_real (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *u;
  A68_REAL v;
  int rc;
  error_node = p;
  POP_OBJECT (p, &v, A68_REAL);
  if (VALUE (&v) == 0.0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (ROWROW_REAL));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  u = pop_matrix (p, A68_TRUE);
  rc = gsl_matrix_scale (u, 1.0 / VALUE (&v));
  torrix_test_error (rc);
  push_matrix (p, u);
  gsl_matrix_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP / = ([] COMPLEX, COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_div_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u;
  A68_REAL re, im;
  gsl_complex v;
  error_node = p;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  if (VALUE (&re) == 0.0 && VALUE (&im) == 0.0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (ROW_COMPLEX));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  u = pop_vector_complex (p, A68_TRUE);
  v = gsl_complex_inverse (v);
  gsl_blas_zscal (v, u);
  push_vector_complex (p, u);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP / = ([, ] COMPLEX, COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_div_complex (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *u;
  A68_REAL re, im;
  gsl_complex v;
  int rc;
  error_node = p;
  POP_OBJECT (p, &im, A68_REAL);
  POP_OBJECT (p, &re, A68_REAL);
  if (VALUE (&re) == 0.0 && VALUE (&im) == 0.0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (ROWROW_COMPLEX));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  GSL_SET_COMPLEX (&v, VALUE (&re), VALUE (&im));
  v = gsl_complex_inverse (v);
  u = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_matrix_complex_scale (u, v);
  torrix_test_error (rc);
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP /:= (REF [] REAL, REAL) REF [] REAL
\param p position in tree
**/

void genie_vector_div_real_ab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROW_REAL), MODE (REAL), genie_vector_div_real);
}

/*!
\brief OP /:= (REF [, ] REAL, REAL) REF [, ] REAL
\param p position in tree
**/

void genie_matrix_div_real_ab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROWROW_REAL), MODE (REAL), genie_matrix_div_real);
}

/*!
\brief OP /:= (REF [] COMPLEX, COMPLEX) REF [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_div_complex_ab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROW_COMPLEX), MODE (COMPLEX), genie_vector_complex_div_complex);
}

/*!
\brief OP /:= (REF [, ] COMPLEX, COMPLEX) REF [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_div_complex_ab (NODE_T * p)
{
  op_ab (p, MODE (REF_ROWROW_COMPLEX), MODE (COMPLEX), genie_matrix_complex_div_complex);
}

/*!
\brief OP * = ([] REAL, [] REAL) REAL
\param p position in tree
**/

void genie_vector_dot (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u, *v;
  double w;
  int rc;
  error_node = p;
  v = pop_vector (p, A68_TRUE);
  u = pop_vector (p, A68_TRUE);
  rc = gsl_blas_ddot (u, v, &w);
  torrix_test_error (rc);
  PUSH_PRIMITIVE (p, w, A68_REAL);
  gsl_vector_free (u);
  gsl_vector_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([] COMPLEX, [] COMPLEX) COMPLEX
\param p position in tree
**/

void genie_vector_complex_dot (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u, *v;
  gsl_complex w;
  int rc;
  error_node = p;
  v = pop_vector_complex (p, A68_TRUE);
  u = pop_vector_complex (p, A68_TRUE);
  rc = gsl_blas_zdotc (u, v, &w);
  torrix_test_error (rc);
  PUSH_PRIMITIVE (p, GSL_REAL (w), A68_REAL);
  PUSH_PRIMITIVE (p, GSL_IMAG (w), A68_REAL);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP NORM = ([] REAL) REAL
\param p position in tree
**/

void genie_vector_norm (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u;
  error_node = p;
  u = pop_vector (p, A68_TRUE);
  PUSH_PRIMITIVE (p, gsl_blas_dnrm2 (u), A68_REAL);
  gsl_vector_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP NORM = ([] COMPLEX) COMPLEX
\param p position in tree
**/

void genie_vector_complex_norm (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u;
  error_node = p;
  u = pop_vector_complex (p, A68_TRUE);
  PUSH_PRIMITIVE (p, gsl_blas_dznrm2 (u), A68_REAL);
  gsl_vector_complex_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP DYAD = ([] REAL, [] REAL) [, ] REAL
\param p position in tree
**/

void genie_vector_dyad (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *u, *v;
  gsl_matrix *w;
  int len1, len2, j, k;
  error_node = p;
  v = pop_vector (p, A68_TRUE);
  u = pop_vector (p, A68_TRUE);
  len1 = (int) (u->size);
  len2 = (int) (v->size);
  w = gsl_matrix_alloc ((size_t) len1, (size_t) len2);
  for (j = 0; j < len1; j++) {
    double uj = gsl_vector_get (u, (size_t) j);
    for (k = 0; k < len2; k++) {
      double vk = gsl_vector_get (v, (size_t) k);
      gsl_matrix_set (w, (size_t) j, (size_t) k, uj * vk);
    }
  }
  push_matrix (p, w);
  gsl_vector_free (u);
  gsl_vector_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP DYAD = ([] COMPLEX, [] COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_vector_complex_dyad (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *u, *v;
  gsl_matrix_complex *w;
  int len1, len2, j, k;
  error_node = p;
  v = pop_vector_complex (p, A68_TRUE);
  u = pop_vector_complex (p, A68_TRUE);
  len1 = (int) (u->size);
  len2 = (int) (v->size);
  w = gsl_matrix_complex_alloc ((size_t) len1, (size_t) len2);
  for (j = 0; j < len1; j++) {
    gsl_complex uj = gsl_vector_complex_get (u, (size_t) j);
    for (k = 0; k < len2; k++) {
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

/*!
\brief OP * = ([, ] REAL, [] REAL) [] REAL
\param p position in tree
**/

void genie_matrix_times_vector (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  int len;
  int rc;
  gsl_vector *u, *v;
  gsl_matrix *w;
  error_node = p;
  u = pop_vector (p, A68_TRUE);
  w = pop_matrix (p, A68_TRUE);
  len = (int) (u->size);
  v = gsl_vector_alloc ((size_t) len);
  gsl_vector_set_zero (v);
  rc = gsl_blas_dgemv (CblasNoTrans, 1.0, w, u, 0.0, v);
  torrix_test_error (rc);
  push_vector (p, v);
  gsl_vector_free (u);
  gsl_vector_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([] REAL, [, ] REAL) [] REAL
\param p position in tree
**/

void genie_vector_times_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  int len;
  int rc;
  gsl_vector *u, *v;
  gsl_matrix *w;
  error_node = p;
  w = pop_matrix (p, A68_TRUE);
  rc = gsl_matrix_transpose (w);
  torrix_test_error (rc);
  u = pop_vector (p, A68_TRUE);
  len = (int) (u->size);
  v = gsl_vector_alloc ((size_t) len);
  gsl_vector_set_zero (v);
  rc = gsl_blas_dgemv (CblasNoTrans, 1.0, w, u, 0.0, v);
  torrix_test_error (rc);
  push_vector (p, v);
  gsl_vector_free (u);
  gsl_vector_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([, ] REAL, [, ] REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_times_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  int len1, len2;
  int rc;
  gsl_matrix *u, *v, *w;
  error_node = p;
  v = pop_matrix (p, A68_TRUE);
  u = pop_matrix (p, A68_TRUE);
  len2 = (int) (v->size2);
  len1 = (int) (u->size1);
  w = gsl_matrix_alloc ((size_t) len1, (size_t) len2);
  gsl_matrix_set_zero (w);
  rc = gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 1.0, u, v, 0.0, w);
  torrix_test_error (rc);
  push_matrix (p, w);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([, ] COMPLEX, [] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_times_vector (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  int len, rc;
  gsl_vector_complex *u, *v;
  gsl_matrix_complex *w;
  gsl_complex zero, one;
  error_node = p;
  GSL_SET_COMPLEX (&zero, 0.0, 0.0);
  GSL_SET_COMPLEX (&one, 1.0, 0.0);
  u = pop_vector_complex (p, A68_TRUE);
  w = pop_matrix_complex (p, A68_TRUE);
  len = (int) (u->size);
  v = gsl_vector_complex_alloc ((size_t) len);
  gsl_vector_complex_set_zero (v);
  rc = gsl_blas_zgemv (CblasNoTrans, one, w, u, zero, v);
  torrix_test_error (rc);
  push_vector_complex (p, v);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  gsl_matrix_complex_free (w);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([] COMPLEX, [, ] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_vector_complex_times_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  int len, rc;
  gsl_vector_complex *u, *v;
  gsl_matrix_complex *w;
  gsl_complex zero, one;
  error_node = p;
  GSL_SET_COMPLEX (&zero, 0.0, 0.0);
  GSL_SET_COMPLEX (&one, 1.0, 0.0);
  w = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_matrix_complex_transpose (w);
  torrix_test_error (rc);
  u = pop_vector_complex (p, A68_TRUE);
  len = (int) (u->size);
  v = gsl_vector_complex_alloc ((size_t) len);
  gsl_vector_complex_set_zero (v);
  rc = gsl_blas_zgemv (CblasNoTrans, one, w, u, zero, v);
  torrix_test_error (rc);
  push_vector_complex (p, v);
  gsl_vector_complex_free (u);
  gsl_vector_complex_free (v);
  gsl_matrix_complex_free (w);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief OP * = ([, ] COMPLEX, [, ] COMPLEX) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_times_matrix (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  int len1, len2, rc;
  gsl_matrix_complex *u, *v, *w;
  gsl_complex zero, one;
  error_node = p;
  GSL_SET_COMPLEX (&zero, 0.0, 0.0);
  GSL_SET_COMPLEX (&one, 1.0, 0.0);
  v = pop_matrix_complex (p, A68_TRUE);
  u = pop_matrix_complex (p, A68_TRUE);
  len1 = (int) (v->size2);
  len2 = (int) (u->size1);
  w = gsl_matrix_complex_alloc ((size_t) len1, (size_t) len2);
  gsl_matrix_complex_set_zero (w);
  rc = gsl_blas_zgemm (CblasNoTrans, CblasNoTrans, one, u, v, zero, w);
  torrix_test_error (rc);
  push_matrix_complex (p, w);
  gsl_matrix_complex_free (u);
  gsl_matrix_complex_free (v);
  gsl_matrix_complex_free (w);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC lu decomp = ([, ] REAL, REF [] INT, REF INT) [, ] REAL
\param p position in tree
**/

void genie_matrix_lu (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REF ref_signum, ref_q;
  gsl_permutation *q;
  gsl_matrix *u;
  int rc;
  A68_INT signum;
  error_node = p;
  POP_REF (p, &ref_signum);
  CHECK_REF (p, ref_signum, MODE (REF_INT));
  POP_REF (p, &ref_q);
  CHECK_REF (p, ref_q, MODE (REF_ROW_INT));
  PUSH_REF (p, *DEREF (A68_ROW, &ref_q));
  q = pop_permutation (p, A68_FALSE);
  u = pop_matrix (p, A68_TRUE);
  rc = gsl_linalg_LU_decomp (u, q, &(VALUE (&signum)));
  torrix_test_error (rc);
  STATUS (&signum) = INITIALISED_MASK;
  *DEREF (A68_INT, &ref_signum) = signum;
  push_permutation (p, q);
  POP_REF (p, DEREF (A68_ROW, &ref_q));
  push_matrix (p, u);
  gsl_matrix_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC lu det = ([, ] REAL, INT) REAL
\param p position in tree
**/

void genie_matrix_lu_det (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *lu;
  A68_INT signum;
  error_node = p;
  POP_OBJECT (p, &signum, A68_INT);
  lu = pop_matrix (p, A68_TRUE);
  PUSH_PRIMITIVE (p, gsl_linalg_LU_det (lu, VALUE (&signum)), A68_REAL);
  gsl_matrix_free (lu);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC lu inv = ([, ] REAL, [] INT) [, ] REAL
\param p position in tree
**/

void genie_matrix_lu_inv (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix *lu, *inv;
  int rc;
  error_node = p;
  q = pop_permutation (p, A68_TRUE);
  lu = pop_matrix (p, A68_TRUE);
  inv = gsl_matrix_alloc (lu->size1, lu->size2);
  rc = gsl_linalg_LU_invert (lu, q, inv);
  torrix_test_error (rc);
  push_matrix (p, inv);
  gsl_matrix_free (lu);
  gsl_matrix_free (inv);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC lu solve ([, ] REAL, [, ] REAL, [] INT, [] REAL) [] REAL
\param p position in tree
**/

void genie_matrix_lu_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix *a, *lu;
  gsl_vector *b, *x, *r;
  int rc;
  error_node = p;
  b = pop_vector (p, A68_TRUE);
  q = pop_permutation (p, A68_TRUE);
  lu = pop_matrix (p, A68_TRUE);
  a = pop_matrix (p, A68_TRUE);
  x = gsl_vector_alloc (b->size);
  r = gsl_vector_alloc (b->size);
  rc = gsl_linalg_LU_solve (lu, q, b, x);
  torrix_test_error (rc);
  rc = gsl_linalg_LU_refine (a, lu, q, b, x, r);
  torrix_test_error (rc);
  push_vector (p, x);
  gsl_matrix_free (a);
  gsl_matrix_free (lu);
  gsl_vector_free (b);
  gsl_vector_free (r);
  gsl_vector_free (x);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC complex lu decomp = ([, ] COMPLEX, REF [] INT, REF INT) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_lu (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REF ref_signum, ref_q;
  gsl_permutation *q;
  gsl_matrix_complex *u;
  int rc;
  A68_INT signum;
  error_node = p;
  POP_REF (p, &ref_signum);
  CHECK_REF (p, ref_signum, MODE (REF_INT));
  POP_REF (p, &ref_q);
  CHECK_REF (p, ref_q, MODE (REF_ROW_INT));
  PUSH_REF (p, *DEREF (A68_ROW, &ref_q));
  q = pop_permutation (p, A68_FALSE);
  u = pop_matrix_complex (p, A68_TRUE);
  rc = gsl_linalg_complex_LU_decomp (u, q, &(VALUE (&signum)));
  torrix_test_error (rc);
  STATUS (&signum) = INITIALISED_MASK;
  *DEREF (A68_INT, &ref_signum) = signum;
  push_permutation (p, q);
  POP_REF (p, DEREF (A68_ROW, &ref_q));
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC complex lu det = ([, ] COMPLEX, INT) COMPLEX
\param p position in tree
**/

void genie_matrix_complex_lu_det (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *lu;
  A68_INT signum;
  gsl_complex det;
  error_node = p;
  POP_OBJECT (p, &signum, A68_INT);
  lu = pop_matrix_complex (p, A68_TRUE);
  det = gsl_linalg_complex_LU_det (lu, VALUE (&signum));
  PUSH_PRIMITIVE (p, GSL_REAL (det), A68_REAL);
  PUSH_PRIMITIVE (p, GSL_IMAG (det), A68_REAL);
  gsl_matrix_complex_free (lu);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC complex lu inv = ([, ] COMPLEX, [] INT) [, ] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_lu_inv (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix_complex *lu, *inv;
  int rc;
  error_node = p;
  q = pop_permutation (p, A68_TRUE);
  lu = pop_matrix_complex (p, A68_TRUE);
  inv = gsl_matrix_complex_alloc (lu->size1, lu->size2);
  rc = gsl_linalg_complex_LU_invert (lu, q, inv);
  torrix_test_error (rc);
  push_matrix_complex (p, inv);
  gsl_matrix_complex_free (lu);
  gsl_matrix_complex_free (inv);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC complex lu solve ([, ] COMPLEX, [, ] COMPLEX, [] INT, [] COMPLEX) [] COMPLEX
\param p position in tree
**/

void genie_matrix_complex_lu_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix_complex *a, *lu;
  gsl_vector_complex *b, *x, *r;
  int rc;
  error_node = p;
  b = pop_vector_complex (p, A68_TRUE);
  q = pop_permutation (p, A68_TRUE);
  lu = pop_matrix_complex (p, A68_TRUE);
  a = pop_matrix_complex (p, A68_TRUE);
  x = gsl_vector_complex_alloc (b->size);
  r = gsl_vector_complex_alloc (b->size);
  rc = gsl_linalg_complex_LU_solve (lu, q, b, x);
  torrix_test_error (rc);
  rc = gsl_linalg_complex_LU_refine (a, lu, q, b, x, r);
  torrix_test_error (rc);
  push_vector_complex (p, x);
  gsl_matrix_complex_free (a);
  gsl_matrix_complex_free (lu);
  gsl_vector_complex_free (b);
  gsl_vector_complex_free (r);
  gsl_vector_complex_free (x);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC svd decomp = ([, ] REAL, REF [, ] REAL, REF [] REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_svd (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a, *v;
  gsl_vector *s, *w;
  A68_REF ref_s, ref_v;
  int rc;
  error_node = p;
  POP_REF (p, &ref_s);
  CHECK_REF (p, ref_s, MODE (REF_ROW_REAL));
  PUSH_REF (p, *DEREF (A68_ROW, &ref_s));
  s = pop_vector (p, A68_FALSE);
  POP_REF (p, &ref_v);
  CHECK_REF (p, ref_v, MODE (REF_ROWROW_REAL));
  PUSH_REF (p, *DEREF (A68_ROW, &ref_v));
  v = pop_matrix (p, A68_FALSE);
  a = pop_matrix (p, A68_TRUE);
  w = gsl_vector_alloc (v->size2);
  rc = gsl_linalg_SV_decomp (a, v, s, w);
  torrix_test_error (rc);
  push_vector (p, s);
  POP_REF (p, DEREF (A68_ROW, &ref_s));
  push_matrix (p, v);
  POP_REF (p, DEREF (A68_ROW, &ref_v));
  push_matrix (p, a);
  gsl_matrix_free (a);
  gsl_matrix_free (v);
  gsl_vector_free (s);
  gsl_vector_free (w);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC svd solve = ([, ] REAL, [, ] REAL, [] REAL, [] REAL) [] REAL
\param p position in tree
**/

void genie_matrix_svd_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *u, *v;
  gsl_vector *s, *b, *x;
  int rc;
  error_node = p;
  b = pop_vector (p, A68_TRUE);
  s = pop_vector (p, A68_TRUE);
  v = pop_matrix (p, A68_TRUE);
  u = pop_matrix (p, A68_TRUE);
  x = gsl_vector_alloc (b->size);
  rc = gsl_linalg_SV_solve (u, v, s, b, x);
  push_vector (p, x);
  gsl_vector_free (x);
  gsl_vector_free (b);
  gsl_vector_free (s);
  gsl_matrix_free (v);
  gsl_matrix_free (u);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC qr decomp = ([, ] REAL, [] REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_qr (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a;
  gsl_vector *t;
  A68_REF ref_t;
  int rc;
  error_node = p;
  POP_REF (p, &ref_t);
  CHECK_REF (p, ref_t, MODE (REF_ROW_REAL));
  PUSH_REF (p, *DEREF (A68_ROW, &ref_t));
  t = pop_vector (p, A68_FALSE);
  a = pop_matrix (p, A68_TRUE);
  rc = gsl_linalg_QR_decomp (a, t);
  torrix_test_error (rc);
  push_vector (p, t);
  POP_REF (p, DEREF (A68_ROW, &ref_t));
  push_matrix (p, a);
  gsl_matrix_free (a);
  gsl_vector_free (t);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC qr solve = ([, ] REAL, [] REAL, [] REAL) [] REAL
\param p position in tree
**/

void genie_matrix_qr_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *q;
  gsl_vector *t, *b, *x;
  int rc;
  error_node = p;
  b = pop_vector (p, A68_TRUE);
  t = pop_vector (p, A68_TRUE);
  q = pop_matrix (p, A68_TRUE);
  x = gsl_vector_alloc (b->size);
  rc = gsl_linalg_QR_solve (q, t, b, x);
  push_vector (p, x);
  gsl_vector_free (x);
  gsl_vector_free (b);
  gsl_vector_free (t);
  gsl_matrix_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC qr ls solve = ([, ] REAL, [] REAL, [] REAL) [] REAL
\param p position in tree
**/

void genie_matrix_qr_ls_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *q;
  gsl_vector *t, *b, *x, *r;
  int rc;
  error_node = p;
  b = pop_vector (p, A68_TRUE);
  t = pop_vector (p, A68_TRUE);
  q = pop_matrix (p, A68_TRUE);
  r = gsl_vector_alloc (b->size);
  x = gsl_vector_alloc (b->size);
  rc = gsl_linalg_QR_lssolve (q, t, b, x, r);
  push_vector (p, x);
  gsl_vector_free (x);
  gsl_vector_free (r);
  gsl_vector_free (b);
  gsl_vector_free (t);
  gsl_matrix_free (q);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC cholesky decomp = ([, ] REAL) [, ] REAL
\param p position in tree
**/

void genie_matrix_ch (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a;
  int rc;
  error_node = p;
  a = pop_matrix (p, A68_TRUE);
  rc = gsl_linalg_cholesky_decomp (a);
  torrix_test_error (rc);
  push_matrix (p, a);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC cholesky solve = ([, ] REAL, [] REAL) [] REAL
\param p position in tree
**/

void genie_matrix_ch_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *c;
  gsl_vector *b, *x;
  int rc;
  error_node = p;
  b = pop_vector (p, A68_TRUE);
  c = pop_matrix (p, A68_TRUE);
  x = gsl_vector_alloc (b->size);
  rc = gsl_linalg_cholesky_solve (c, b, x);
  push_vector (p, x);
  gsl_vector_free (x);
  gsl_vector_free (b);
  gsl_matrix_free (c);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief map GSL error handler onto a68g error handler
\param reason error text
\param file gsl file where error occured
\param line line in above file
\param int gsl error number
**/

void fft_error_handler (const char *reason, const char *file, int line, int gsl_errno)
{
  if (line != 0) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s in line %d of file %s", reason, line, file) >= 0);
  } else {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s", reason) >= 0);
  }
  diagnostic_node (A68_RUNTIME_ERROR, error_node, ERROR_FFT, edit_line, gsl_strerror (gsl_errno), NULL);
  exit_genie (error_node, A68_RUNTIME_ERROR);
}

/*!
\brief detect math errors
\param rc return code from function
**/

static void fft_test_error (int rc)
{
  if (rc != 0) {
    fft_error_handler ("math error", "", 0, rc);
  }
}

/*!
\brief pop [] REAL on the stack as complex double []
\param p position in tree
\param get whether to get elements from row in the stack
\return double []
**/

static double *pop_array_real (NODE_T * p, int *len)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int inc, iindex, k;
  BYTE_T *base;
  double *v;
  error_node = p;
/* Pop arguments */
  POP_REF (p, &desc);
  CHECK_REF (p, desc, MODE (ROW_REAL));
  GET_DESCRIPTOR (arr, tup, &desc);
  *len = ROW_SIZE (tup);
  if ((*len) == 0) {
    return (NULL);
  }
  v = malloc (2 * (size_t) (*len) * sizeof (double));
  fft_test_error (v == NULL ? GSL_ENOMEM : GSL_SUCCESS);
  base = DEREF (BYTE_T, &(ARRAY (arr)));
  iindex = VECTOR_OFFSET (arr, tup);
  inc = tup->span * arr->elem_size;
  for (k = 0; k < (*len); k++, iindex += inc) {
    A68_REAL *x = (A68_REAL *) (base + iindex);
    CHECK_INIT (p, INITIALISED (x), MODE (REAL));
    v[2 * k] = VALUE (x);
    v[2 * k + 1] = 0.0;
  }
  return (v);
}

/*!
\brief push double [] on the stack as [] REAL
\param p position in tree
**/

static void push_array_real (NODE_T * p, double *v, int len)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int inc, iindex, k;
  BYTE_T *base;
  error_node = p;
  desc = heap_generator (p, MODE (ROW_REAL), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  BLOCK_GC_HANDLE (&desc);
  row = heap_generator (p, MODE (ROW_REAL), len * ALIGNED_SIZE_OF (A68_REAL));
  BLOCK_GC_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (REAL);
  arr.elem_size = ALIGNED_SIZE_OF (A68_REAL);
  arr.slice_offset = arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = len;
  tup.shift = LWB (&tup);
  tup.span = 1;
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &desc);
  base = DEREF (BYTE_T, &(ARRAY (&arr)));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = tup.span * arr.elem_size;
  for (k = 0; k < len; k++, iindex += inc) {
    A68_REAL *x = (A68_REAL *) (base + iindex);
    STATUS (x) = INITIALISED_MASK;
    VALUE (x) = v[2 * k];
    CHECK_REAL_REPRESENTATION (p, VALUE (x));
  }
  UNBLOCK_GC_HANDLE (&desc);
  UNBLOCK_GC_HANDLE (&row);
  PUSH_REF (p, desc);
}

/*!
\brief pop [] COMPLEX on the stack as double []
\param p position in tree
\param get whether to get elements from row in the stack
\return double []
**/

static double *pop_array_complex (NODE_T * p, int *len)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int inc, iindex, k;
  BYTE_T *base;
  double *v;
  error_node = p;
/* Pop arguments */
  POP_REF (p, &desc);
  CHECK_REF (p, desc, MODE (ROW_COMPLEX));
  GET_DESCRIPTOR (arr, tup, &desc);
  *len = ROW_SIZE (tup);
  if ((*len) == 0) {
    return (NULL);
  }
  v = malloc (2 * (size_t) (*len) * sizeof (double));
  fft_test_error (v == NULL ? GSL_ENOMEM : GSL_SUCCESS);
  base = DEREF (BYTE_T, &(ARRAY (arr)));
  iindex = VECTOR_OFFSET (arr, tup);
  inc = tup->span * arr->elem_size;
  for (k = 0; k < (*len); k++, iindex += inc) {
    A68_REAL *re = (A68_REAL *) (base + iindex);
    A68_REAL *im = (A68_REAL *) (base + iindex + ALIGNED_SIZE_OF (A68_REAL));
    CHECK_INIT (p, INITIALISED (re), MODE (COMPLEX));
    CHECK_INIT (p, INITIALISED (im), MODE (COMPLEX));
    v[2 * k] = VALUE (re);
    v[2 * k + 1] = VALUE (im);
  }
  return (v);
}

/*!
\brief push double [] on the stack as [] COMPLEX
\param p position in tree
**/

static void push_array_complex (NODE_T * p, double *v, int len)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int inc, iindex, k;
  BYTE_T *base;
  error_node = p;
  desc = heap_generator (p, MODE (ROW_COMPLEX), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  BLOCK_GC_HANDLE (&desc);
  row = heap_generator (p, MODE (ROW_COMPLEX), len * 2 * ALIGNED_SIZE_OF (A68_REAL));
  BLOCK_GC_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (COMPLEX);
  arr.elem_size = 2 * ALIGNED_SIZE_OF (A68_REAL);
  arr.slice_offset = arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = len;
  tup.shift = LWB (&tup);
  tup.span = 1;
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &desc);
  base = DEREF (BYTE_T, &(ARRAY (&arr)));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = tup.span * arr.elem_size;
  for (k = 0; k < len; k++, iindex += inc) {
    A68_REAL *re = (A68_REAL *) (base + iindex);
    A68_REAL *im = (A68_REAL *) (base + iindex + ALIGNED_SIZE_OF (A68_REAL));
    STATUS (re) = INITIALISED_MASK;
    VALUE (re) = v[2 * k];
    STATUS (im) = INITIALISED_MASK;
    VALUE (im) = v[2 * k + 1];
    CHECK_COMPLEX_REPRESENTATION (p, VALUE (re), VALUE (im));
  }
  UNBLOCK_GC_HANDLE (&desc);
  UNBLOCK_GC_HANDLE (&row);
  PUSH_REF (p, desc);
}

/*!
\brief push prime factorisation on the stack as [] INT
\param p position in tree
**/

void genie_prime_factors (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  A68_INT n;
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int len, inc, iindex, k;
  BYTE_T *base;
  gsl_fft_complex_wavetable *wt;
  error_node = p;
  POP_OBJECT (p, &n, A68_INT);
  CHECK_INIT (p, INITIALISED (&n), MODE (INT));
  wt = gsl_fft_complex_wavetable_alloc ((size_t) (VALUE (&n)));
  len = (int) (wt->nf);
  desc = heap_generator (p, MODE (ROW_INT), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  BLOCK_GC_HANDLE (&desc);
  row = heap_generator (p, MODE (ROW_INT), len * ALIGNED_SIZE_OF (A68_INT));
  BLOCK_GC_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (INT);
  arr.elem_size = ALIGNED_SIZE_OF (A68_INT);
  arr.slice_offset = arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = len;
  tup.shift = LWB (&tup);
  tup.span = 1;
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &desc);
  base = DEREF (BYTE_T, &(ARRAY (&arr)));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = tup.span * arr.elem_size;
  for (k = 0; k < len; k++, iindex += inc) {
    A68_INT *x = (A68_INT *) (base + iindex);
    STATUS (x) = INITIALISED_MASK;
    VALUE (x) = (int) ((wt->factor)[k]);
  }
  gsl_fft_complex_wavetable_free (wt);
  UNBLOCK_GC_HANDLE (&desc);
  UNBLOCK_GC_HANDLE (&row);
  PUSH_REF (p, desc);
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC ([] COMPLEX) [] COMPLEX fft complex forward
\param p position in tree
**/

void genie_fft_complex_forward (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  double *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  error_node = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_forward (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_complex (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NULL) {
    free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC ([] COMPLEX) [] COMPLEX fft complex backward
\param p position in tree
**/

void genie_fft_complex_backward (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  double *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  error_node = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_backward (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_complex (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NULL) {
    free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC ([] COMPLEX) [] COMPLEX fft complex inverse
\param p position in tree
**/

void genie_fft_complex_inverse (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  double *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  error_node = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_inverse (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_complex (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NULL) {
    free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC ([] REAL) [] COMPLEX fft forward
\param p position in tree
**/

void genie_fft_forward (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  double *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  error_node = p;
  data = pop_array_real (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_forward (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_complex (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NULL) {
    free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC ([] COMPLEX) [] REAL fft backward
\param p position in tree
**/

void genie_fft_backward (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  double *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  error_node = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_backward (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_real (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NULL) {
    free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief PROC ([] COMPLEX) [] REAL fft inverse
\param p position in tree
**/

void genie_fft_inverse (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  double *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  error_node = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_inverse (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_real (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NULL) {
    free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

/*!
\brief map GSL error handler onto a68g error handler
\param reason error text
\param file gsl file where error occured
\param line line in above file
\param int gsl error number
**/

void laplace_error_handler (const char *reason, const char *file, int line, int gsl_errno)
{
  if (line != 0) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s in line %d of file %s", reason, line, file) >= 0);
  } else {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s", reason) >= 0);
  }
  diagnostic_node (A68_RUNTIME_ERROR, error_node, ERROR_LAPLACE, edit_line, gsl_strerror (gsl_errno), NULL);
  exit_genie (error_node, A68_RUNTIME_ERROR);
}

/*!
\brief detect math errors
\param rc return code from function
**/

static void laplace_test_error (int rc)
{
  if (rc != 0) {
    laplace_error_handler ("math error", "", 0, rc);
  }
}

/*!
\brief PROC (PROC (REAL) REAL, REAL, REF REAL) REAL laplace
\param p position in tree
**/

#define LAPLACE_DIVISIONS 1024

typedef struct A68_LAPLACE A68_LAPLACE;

struct A68_LAPLACE
{
  NODE_T *p;
  A68_PROCEDURE f;
  double s;
};

double laplace_f (double t, void *z)
{
  A68_LAPLACE *l = (A68_LAPLACE *) z;
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  MOID_T *u = MODE (PROC_REAL_REAL);
  A68_REAL *ft = (A68_REAL *) STACK_TOP;
  PUSH_PRIMITIVE (l->p, t, A68_REAL);
  genie_call_procedure (l->p, MOID (&(l->f)), u, u, &(l->f), pop_sp, pop_fp);
  stack_pointer = pop_sp;
  return (VALUE (ft) * a68g_exp (-(l->s) * t));
}

void genie_laplace (NODE_T * p)
{
  A68_REF ref_error;
  A68_REAL s, *error;
  A68_PROCEDURE f;
  A68_LAPLACE l;
  gsl_function g;
  gsl_integration_workspace *w;
  double result, estimated_error;
  int rc;
  gsl_error_handler_t *save_handler = gsl_set_error_handler (laplace_error_handler);
  POP_REF (p, &ref_error);
  CHECK_REF (p, ref_error, MODE (REF_REAL));
  error = (A68_REAL *) ADDRESS (&ref_error);
  POP_OBJECT (p, &s, A68_REAL);
  POP_PROCEDURE (p, &f);
  l.p = p;
  l.f = f;
  l.s = VALUE (&s);
  g.function = &laplace_f;
  g.params = &l;
  w = gsl_integration_workspace_alloc (LAPLACE_DIVISIONS);
  if (VALUE (error) >= 0.0) {
    rc = gsl_integration_qagiu (&g, 0.0, VALUE (error), 0.0, LAPLACE_DIVISIONS, w, &result, &estimated_error);
  } else {
    rc = gsl_integration_qagiu (&g, 0.0, 0.0, -VALUE (error), LAPLACE_DIVISIONS, w, &result, &estimated_error);
  }
  laplace_test_error (rc);
  VALUE (error) = estimated_error;
  PUSH_PRIMITIVE (p, result, A68_REAL);
  gsl_integration_workspace_free (w);
  (void) gsl_set_error_handler (save_handler);
}

#endif /* HAVE_GSL_GSL_BLAS_H && defined HAVE_LIBGSL */
