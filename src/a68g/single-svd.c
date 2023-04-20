//! @file single-svd.c
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
//! REAL matrix SVD decomposition using GSL.

#include "a68g.h"
#include "a68g-torrix.h"

#if defined (HAVE_GSL)

//! @brief PROC svd decomp = ([, ] REAL, REF [, ] REAL, REF [] REAL, REF [, ] REAL) VOID

void genie_matrix_svd (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
//
  A68_REF ref_u, ref_s, ref_v;
  POP_REF (p, &ref_v);
  POP_REF (p, &ref_s);
  POP_REF (p, &ref_u);
//
  gsl_matrix *a = pop_matrix (p, A68_TRUE), *u, *v; gsl_vector *s, *w;
  unt M = SIZE1 (a), N = SIZE2 (a);
// GSL computes thin SVD, only handles M >= N. GSL returns V, not Vᵀ.
  if (M >= N) {
// X = USVᵀ
    s = gsl_vector_calloc (N);
    u = gsl_matrix_calloc (M, N);
    v = gsl_matrix_calloc (N, N);
    w = gsl_vector_calloc (N);
    gsl_matrix_memcpy (u, a);
    ASSERT_GSL (gsl_linalg_SV_decomp (u, v, s, w));
    if (!IS_NIL (ref_u)) {
      *DEREF (A68_REF, &ref_u) = matrix_to_row (p, u);
    }
    if (!IS_NIL (ref_s)) {
      *DEREF (A68_REF, &ref_s) = vector_to_row (p, s);
    }
    if (!IS_NIL (ref_v)) {
      *DEREF (A68_REF, &ref_v) = matrix_to_row (p, v);
    }
  } else {
// Xᵀ = VSUᵀ
    s = gsl_vector_calloc (M);
    u = gsl_matrix_calloc (N, M);
    v = gsl_matrix_calloc (M, M);
    w = gsl_vector_calloc (M);
    gsl_matrix_transpose_memcpy (u, a);
    ASSERT_GSL (gsl_linalg_SV_decomp (u, v, s, w));
    if (!IS_NIL (ref_u)) {
      *DEREF (A68_REF, &ref_u) = matrix_to_row (p, v);
    }
    if (!IS_NIL (ref_s)) {
      *DEREF (A68_REF, &ref_s) = vector_to_row (p, s);
    }
    if (!IS_NIL (ref_v)) {
      *DEREF (A68_REF, &ref_v) = matrix_to_row (p, u);
    }
  }
  gsl_matrix_free (a);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  gsl_vector_free (s);
  gsl_vector_free (w);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC svd solve = ([, ] REAL, [] REAL, [,] REAL, [] REAL) [] REAL

void genie_matrix_svd_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  gsl_vector *b = pop_vector (p, A68_TRUE);
  gsl_matrix *v = pop_matrix (p, A68_TRUE);
  gsl_vector *s = pop_vector (p, A68_TRUE);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  gsl_vector *x = gsl_vector_calloc (SIZE (b));
  ASSERT_GSL (gsl_linalg_SV_solve (u, v, s, b, x));
  push_vector (p, x);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  gsl_vector_free (b);
  gsl_vector_free (s);
  gsl_vector_free (x);
  (void) gsl_set_error_handler (save_handler);
}

#endif
