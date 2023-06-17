//! @file single-python.c
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
//! REAL vector and matrix routines, Python look-a-likes.

#include "a68g.h"

#if defined (HAVE_GSL)

#include "a68g-double.h"
#include "a68g-genie.h"
#include "a68g-prelude-gsl.h"
#include "a68g-prelude.h"
#include "a68g-torrix.h"

//! Print REAL vector and matrix using GSL.

#define FMT "%12.4g"

void print_vector (gsl_vector *A, unt w)
{
  unt N = SIZE (A);
  fprintf (stdout, "[%d]\n", N);
  if (w <= 0 || N <= 2 * w) {
    for (unt i = 0; i < N; i++) {
      fprintf (stdout, FMT, gsl_vector_get (A, i));
    }
  } else {
    for (unt i = 0; i < w; i++) {
      fprintf (stdout, FMT, gsl_vector_get (A, i));
    }
    fprintf (stdout, " ... ");
    for (unt i = N - w; i < N; i++) {
      fprintf (stdout, FMT, gsl_vector_get (A, i));
    }
  }
  fprintf (stdout, "\n");
}

void print_row (gsl_matrix *A, unt m, unt w)
{
  unt N = SIZE2 (A);
  if (w <= 0 || N <= 2 * w) {
    for (unt i = 0; i < N; i++) {
      fprintf (stdout, FMT, gsl_matrix_get (A, m, i));
    }
  } else {
    for (unt i = 0; i < w; i++) {
      fprintf (stdout, FMT, gsl_matrix_get (A, m, i));
    }
    fprintf (stdout, " ... ");
    for (unt i = N - w; i < N; i++) {
      fprintf (stdout, FMT, gsl_matrix_get (A, m, i));
    }
  }
  fprintf (stdout, "\n");
}

void print_matrix (gsl_matrix *A, unt w)
{
  unt M = SIZE1 (A), N = SIZE2 (A);
  fprintf (stdout, "[%d, %d]\n", M, N);
  if (w <= 0 || M <= 2 * w) {
    for (unt i = 0; i < M; i++) {
      print_row (A, i, w);
    }
  } else {
    for (unt i = 0; i < w; i++) {
      print_row (A, i, w);
    }
    fprintf (stdout, " ...\n");
    for (unt i = M - w; i < M; i++) {
      print_row (A, i, w);
    }
  }
  fprintf (stdout, "\n");
}

//! @brief PROC print vector = ([] REAL v, INT width) VOID

void genie_print_vector (NODE_T *p)
{
  A68_INT width;
  POP_OBJECT (p, &width, A68_INT);
  gsl_vector *V = pop_vector (p, A68_TRUE);
  print_vector (V, VALUE (&width));
  gsl_vector_free (V);
}

//! @brief PROC print matrix = ([, ] REAL v, INT width) VOID

void genie_print_matrix (NODE_T *p)
{
  A68_INT width;
  POP_OBJECT (p, &width, A68_INT);
  gsl_matrix *M = pop_matrix (p, A68_TRUE);
  print_matrix (M, VALUE (&width));
  gsl_matrix_free (M);
}

//! @brief Convert VECTOR to [] REAL.

A68_ROW vector_to_row (NODE_T * p, gsl_vector * v)
{
  unt len = (unt) (SIZE (v));
  A68_ROW desc, row; A68_ARRAY arr; A68_TUPLE tup;
  NEW_ROW_1D (desc, row, arr, tup, M_ROW_REAL, M_REAL, len);
  BYTE_T *base = DEREF (BYTE_T, &ARRAY (&arr));
  int idx = VECTOR_OFFSET (&arr, &tup);
  unt inc = SPAN (&tup) * ELEM_SIZE (&arr);
  for (unt k = 0; k < len; k++, idx += inc) {
    A68_REAL *x = (A68_REAL *) (base + idx);
    STATUS (x) = INIT_MASK;
    VALUE (x) = gsl_vector_get (v, (size_t) k);
    CHECK_REAL (p, VALUE (x));
  }
  return desc;
}

//! @brief Convert MATRIX to [, ] REAL.

A68_ROW matrix_to_row (NODE_T * p, gsl_matrix * a)
{
  int len1 = (int) (SIZE1 (a)), len2 = (int) (SIZE2 (a));
  A68_REF desc; A68_ROW row; A68_ARRAY arr; A68_TUPLE tup1, tup2;
  desc = heap_generator (p, M_ROW_ROW_REAL, DESCRIPTOR_SIZE (2));
  row = heap_generator (p, M_ROW_ROW_REAL, len1 * len2 * SIZE (M_REAL));
  DIM (&arr) = 2;
  MOID (&arr) = M_REAL;
  ELEM_SIZE (&arr) = SIZE (M_REAL);
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
      A68_REAL *x = (A68_REAL *) (base + idx2);
      STATUS (x) = INIT_MASK;
      VALUE (x) = gsl_matrix_get (a, (size_t) k1, (size_t) k2);
      CHECK_REAL (p, VALUE (x));
    }
  }
  return desc;
}

//! @brief OP NORM = ([, ] REAL) REAL

REAL_T matrix_norm (gsl_matrix *a)
{
#if (A68_LEVEL >= 3)
  DOUBLE_T sum = 0.0;
  unt M = SIZE1 (a), N = SIZE2 (a);
  for (unt i = 0; i < M; i++) {
    for (unt j = 0; j < N; j++) {
      DOUBLE_T z = gsl_matrix_get (a, i, j);
      sum += z * z;
    }
  }
  return ((REAL_T) sqrt_double (sum));
#else
  REAL_T sum = 0.0;
  unt M = SIZE1 (a), N = SIZE2 (a);
  for (unt i = 0; i < M; i++) {
    for (unt j = 0; j < N; j++) {
      REAL_T z = gsl_matrix_get (a, i, j);
      sum += z * z;
    }
  }
  return (sqrt (sum));
#endif
}

void genie_matrix_norm (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a = pop_matrix (p, A68_TRUE);
  PUSH_VALUE (p, matrix_norm (a), A68_REAL);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP HCAT = ([, ] REAL, [, ] REAL) [, ] REAL

gsl_matrix *matrix_hcat (NODE_T *p, gsl_matrix *u, gsl_matrix *v)
{
  unt Mv = SIZE1 (v), Nv = SIZE2 (v);
  unt Mu, Nu;
  if (u == NO_REAL_MATRIX) {
    Mu = Nu = 0;
  } else {
    Mu = SIZE1 (u), Nu = SIZE2 (u);
  }
  if (Mu == 0 && Nu == 0) {
    Mu = Mv;
  } else {
    MATH_RTE (p, Mu != Mv, M_INT, "number of rows does not match");
  }
  gsl_matrix *w = gsl_matrix_calloc (Mu, Nu + Nv);
  for (unt i = 0; i < Mu; i++) {
    unt k = 0;
    for (unt j = 0; j < Nu; j++) {
      gsl_matrix_set (w, i, k++, gsl_matrix_get (u, i, j));
    }
    for (unt j = 0; j < Nv; j++) {
      gsl_matrix_set (w, i, k++, gsl_matrix_get (v, i, j));
    }
  }
  return (w);
}

void genie_matrix_hcat (NODE_T *p)
{
// Yield [u v].
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *v = pop_matrix (p, A68_TRUE);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  gsl_matrix *w = matrix_hcat (p, u, v);
  push_matrix (p, w);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP VCAT = ([, ] REAL, [, ] REAL) [, ] REAL

gsl_matrix *matrix_vcat (NODE_T *p, gsl_matrix *u, gsl_matrix *v)
{
  unt Mv = SIZE1 (v), Nv = SIZE2 (v);
  unt Mu, Nu;
  if (u == NO_REAL_MATRIX) {
    Mu = Nu = 0;
  } else {
    Mu = SIZE1 (u), Nu = SIZE2 (u);
  }
  if (Mu == 0 && Nu == 0) {
    Nu = Nv;
  } else {  
    MATH_RTE (p, Nu != Nv, M_INT, "number of columns does not match");
  }
  gsl_matrix *w = gsl_matrix_calloc (Mu + Mv, Nu);
  for (unt j = 0; j < Nu; j++) {
    unt k = 0;
    for (unt i = 0; i < Mu; i++) {
      gsl_matrix_set (w, k++, j, gsl_matrix_get (u, i, j));
    }
    for (unt i = 0; i < Mv; i++) {
      gsl_matrix_set (w, k++, j, gsl_matrix_get (v, i, j));
    }
  }
  return (w);
}

void genie_matrix_vcat (NODE_T *p)
{
// Yield [u; v].
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *v = pop_matrix (p, A68_TRUE);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  gsl_matrix *w = matrix_vcat (p, u, v);
  push_matrix (p, w);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  gsl_matrix_free (w);
  (void) gsl_set_error_handler (save_handler);
}

gsl_matrix *mat_before_ab (NODE_T *p, gsl_matrix *u, gsl_matrix *v)
{
// Form A := A BEFORE B.
  gsl_matrix *w = matrix_hcat (p, u, v);
  if (u != NO_REAL_MATRIX) {
    a68_matrix_free (u); // Deallocate A, otherwise caller leaks memory.
  }
  return w;
}

gsl_matrix *mat_over_ab (NODE_T *p, gsl_matrix *u, gsl_matrix *v)
{
// Form A := A OVER B.
  gsl_matrix *w = matrix_vcat (p, u, v);
  if (u != NO_REAL_MATRIX) {
    a68_matrix_free (u); // Deallocate A, otherwise caller leaks memory.
  }
  return w;
}

#endif
