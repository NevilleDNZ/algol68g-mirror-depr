//! @file single-decomposition.c
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
//! REAL GSL LU, QR and Choleski decomposition.

#include "a68g.h"
#include "a68g-torrix.h"

#if defined (HAVE_GSL)

//! @brief PROC lu decomp = ([, ] REAL, REF [] INT, REF INT) [, ] REAL

void genie_matrix_lu (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REF ref_signum;
  POP_REF (p, &ref_signum);
  CHECK_REF (p, ref_signum, M_REF_INT);
  A68_REF ref_q;
  POP_REF (p, &ref_q);
  CHECK_REF (p, ref_q, M_REF_ROW_INT);
  PUSH_REF (p, *DEREF (A68_ROW, &ref_q));
  gsl_permutation *q = pop_permutation (p, A68_FALSE);
  gsl_matrix *u = pop_matrix (p, A68_TRUE);
  int sign;
  ASSERT_GSL (gsl_linalg_LU_decomp (u, q, &sign));
  A68_INT signum;
  VALUE (&signum) = sign;
  STATUS (&signum) = INIT_MASK;
  *DEREF (A68_INT, &ref_signum) = signum;
  push_permutation (p, q);
  POP_REF (p, DEREF (A68_ROW, &ref_q));
  push_matrix (p, u);
  gsl_matrix_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC lu det = ([, ] REAL, INT) REAL

void genie_matrix_lu_det (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_INT signum;
  POP_OBJECT (p, &signum, A68_INT);
  gsl_matrix *lu = pop_matrix (p, A68_TRUE);
  PUSH_VALUE (p, gsl_linalg_LU_det (lu, VALUE (&signum)), A68_REAL);
  gsl_matrix_free (lu);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC lu inv = ([, ] REAL, [] INT) [, ] REAL

void genie_matrix_lu_inv (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q = pop_permutation (p, A68_TRUE);
  gsl_matrix *lu = pop_matrix (p, A68_TRUE);
  gsl_matrix *inv = gsl_matrix_calloc (SIZE1 (lu), SIZE2 (lu));
  ASSERT_GSL (gsl_linalg_LU_invert (lu, q, inv));
  push_matrix (p, inv);
  gsl_matrix_free (lu);
  gsl_matrix_free (inv);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC lu solve ([, ] REAL, [, ] REAL, [] INT, [] REAL) [] REAL

void genie_matrix_lu_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *b = pop_vector (p, A68_TRUE);
  gsl_permutation *q = pop_permutation (p, A68_TRUE);
  gsl_matrix *lu = pop_matrix (p, A68_TRUE);
  gsl_matrix *a = pop_matrix (p, A68_TRUE);
  gsl_vector *x = gsl_vector_calloc (SIZE (b));
  gsl_vector *r = gsl_vector_calloc (SIZE (b));
  ASSERT_GSL (gsl_linalg_LU_solve (lu, q, b, x));
  ASSERT_GSL (gsl_linalg_LU_refine (a, lu, q, b, x, r));
  push_vector (p, x);
  gsl_matrix_free (a);
  gsl_matrix_free (lu);
  gsl_vector_free (b);
  gsl_vector_free (r);
  gsl_vector_free (x);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC complex lu decomp = ([, ] COMPLEX, REF [] INT, REF INT) [, ] COMPLEX

void genie_matrix_complex_lu (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REF ref_signum, ref_q;
  gsl_permutation *q;
  gsl_matrix_complex *u;
  int sign;
  A68_INT signum;
  POP_REF (p, &ref_signum);
  CHECK_REF (p, ref_signum, M_REF_INT);
  POP_REF (p, &ref_q);
  CHECK_REF (p, ref_q, M_REF_ROW_INT);
  PUSH_REF (p, *DEREF (A68_ROW, &ref_q));
  q = pop_permutation (p, A68_FALSE);
  u = pop_matrix_complex (p, A68_TRUE);
  ASSERT_GSL (gsl_linalg_complex_LU_decomp (u, q, &sign));
  VALUE (&signum) = sign;
  STATUS (&signum) = INIT_MASK;
  *DEREF (A68_INT, &ref_signum) = signum;
  push_permutation (p, q);
  POP_REF (p, DEREF (A68_ROW, &ref_q));
  push_matrix_complex (p, u);
  gsl_matrix_complex_free (u);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC complex lu det = ([, ] COMPLEX, INT) COMPLEX

void genie_matrix_complex_lu_det (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix_complex *lu;
  A68_INT signum;
  gsl_complex det;
  POP_OBJECT (p, &signum, A68_INT);
  lu = pop_matrix_complex (p, A68_TRUE);
  det = gsl_linalg_complex_LU_det (lu, VALUE (&signum));
  PUSH_VALUE (p, GSL_REAL (det), A68_REAL);
  PUSH_VALUE (p, GSL_IMAG (det), A68_REAL);
  gsl_matrix_complex_free (lu);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC complex lu inv = ([, ] COMPLEX, [] INT) [, ] COMPLEX

void genie_matrix_complex_lu_inv (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_permutation *q;
  gsl_matrix_complex *lu, *inv;
  q = pop_permutation (p, A68_TRUE);
  lu = pop_matrix_complex (p, A68_TRUE);
  inv = gsl_matrix_complex_calloc (SIZE1 (lu), SIZE2 (lu));
  ASSERT_GSL (gsl_linalg_complex_LU_invert (lu, q, inv));
  push_matrix_complex (p, inv);
  gsl_matrix_complex_free (lu);
  gsl_matrix_complex_free (inv);
  gsl_permutation_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC complex lu solve ([, ] COMPLEX, [, ] COMPLEX, [] INT, [] COMPLEX) [] COMPLEX

void genie_matrix_complex_lu_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector_complex *b = pop_vector_complex (p, A68_TRUE);
  gsl_permutation *q = pop_permutation (p, A68_TRUE);
  gsl_matrix_complex *lu = pop_matrix_complex (p, A68_TRUE);
  gsl_matrix_complex *a = pop_matrix_complex (p, A68_TRUE);
  gsl_vector_complex *x = gsl_vector_complex_calloc (SIZE (b));
  gsl_vector_complex *r = gsl_vector_complex_calloc (SIZE (b));
  ASSERT_GSL (gsl_linalg_complex_LU_solve (lu, q, b, x));
  ASSERT_GSL (gsl_linalg_complex_LU_refine (a, lu, q, b, x, r));
  gsl_matrix_complex_free (a);
  gsl_matrix_complex_free (lu);
  gsl_permutation_free (q);
  gsl_vector_complex_free (b);
  gsl_vector_complex_free (r);
  push_vector_complex (p, x);
  gsl_vector_complex_free (x);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC qr decomp = ([, ] REAL, [] REAL) [, ] REAL

void genie_matrix_qr (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REF ref_t;
  POP_REF (p, &ref_t);
  CHECK_REF (p, ref_t, M_REF_ROW_REAL);
  PUSH_REF (p, *DEREF (A68_ROW, &ref_t));
  gsl_vector *t = pop_vector (p, A68_FALSE);
  gsl_matrix *a = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_linalg_QR_decomp (a, t));
  push_vector (p, t);
  gsl_vector_free (t);
  POP_REF (p, DEREF (A68_ROW, &ref_t));
  push_matrix (p, a);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC qr solve = ([, ] REAL, [] REAL, [] REAL) [] REAL

void genie_matrix_qr_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *b = pop_vector (p, A68_TRUE);
  gsl_vector *t = pop_vector (p, A68_TRUE);
  gsl_matrix *q = pop_matrix (p, A68_TRUE);
  gsl_vector *x = gsl_vector_calloc (SIZE (b));
  ASSERT_GSL (gsl_linalg_QR_solve (q, t, b, x));
  push_vector (p, x);
  gsl_vector_free (x);
  gsl_vector_free (b);
  gsl_vector_free (t);
  gsl_matrix_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC qr ls solve = ([, ] REAL, [] REAL, [] REAL) [] REAL

void genie_matrix_qr_ls_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_vector *b = pop_vector (p, A68_TRUE);
  gsl_vector *t = pop_vector (p, A68_TRUE);
  gsl_matrix *q = pop_matrix (p, A68_TRUE);
  gsl_vector *r = gsl_vector_calloc (SIZE (b));
  gsl_vector *x = gsl_vector_calloc (SIZE (b));
  ASSERT_GSL (gsl_linalg_QR_lssolve (q, t, b, x, r));
  push_vector (p, x);
  gsl_vector_free (x);
  gsl_vector_free (r);
  gsl_vector_free (b);
  gsl_vector_free (t);
  gsl_matrix_free (q);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC cholesky decomp = ([, ] REAL) [, ] REAL

void genie_matrix_ch (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *a = pop_matrix (p, A68_TRUE);
  ASSERT_GSL (gsl_linalg_cholesky_decomp (a));
  push_matrix (p, a);
  gsl_matrix_free (a);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC cholesky solve = ([, ] REAL, [] REAL) [] REAL

void genie_matrix_ch_solve (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *c;
  gsl_vector *b, *x;
  b = pop_vector (p, A68_TRUE);
  c = pop_matrix (p, A68_TRUE);
  x = gsl_vector_calloc (SIZE (b));
  ASSERT_GSL (gsl_linalg_cholesky_solve (c, b, x));
  push_vector (p, x);
  gsl_vector_free (x);
  gsl_vector_free (b);
  gsl_matrix_free (c);
  (void) gsl_set_error_handler (save_handler);
}

#endif
