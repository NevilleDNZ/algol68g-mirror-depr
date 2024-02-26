//! @file single-multivariate.c
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
//! REAL multivariate regression.

// This code implements least square regression methods:
//
//   OLS - Ordinary Least Squares
//   PCR - Principle Component Regression, OLS after dimension reduction
//
//   TLS - Total Least Squares
//   PLS - Partial Least Squares, TLS after dimension reduction

#include "a68g.h"

#if defined (HAVE_GSL)

#include "a68g-torrix.h"
#include "a68g-prelude-gsl.h"

#define SMALL_EIGEN 1e-9

//! @brief column-centered data matrix.

gsl_matrix *column_mean (gsl_matrix *DATA)
{
// A is MxN; M samples x N features.
  unt M = SIZE1 (DATA), N = SIZE2 (DATA);
  gsl_matrix *C = gsl_matrix_calloc (M, N);
  for (unt i = 0; i < N; i++) {
    DOUBLE_T sum = 0;
    for (unt j = 0; j < M; j++) {
      sum += gsl_matrix_get (DATA, j, i);
    }
    REAL_T mean = sum / M;
    for (unt j = 0; j < M; j++) {
      gsl_matrix_set (C, j, i, gsl_matrix_get (DATA, j, i) - mean);
    }
  }
  return C;
}

//! @brief left columns from matrix.

static gsl_matrix *left_columns (NODE_T *p, gsl_matrix *X, int cols)
{
  unt M = SIZE1 (X), N = SIZE2 (X);
  MATH_RTE (p, cols < 0, M_INT, "invalid number of columns");
  if (cols == 0 || cols > N) {
    cols = N;
  }
  gsl_matrix *Y = gsl_matrix_calloc (M, cols);
  for (unt i = 0; i < M; i++) {
    for (unt j = 0; j < cols; j++) {
      gsl_matrix_set (Y, i, j, gsl_matrix_get (X, i, j));
    }
  }
  return Y;
}

//! @brief Compute Moore-Penrose pseudo inverse.

void compute_pseudo_inverse (NODE_T *p, gsl_matrix **mpinv, gsl_matrix *X, REAL_T lim)
{
// The Moore-Penrose pseudo inverse gives a least-square approximate solution
// for a system of linear equations not having an exact solution.
// Multivariate statistics is a well known application.
//
  MATH_RTE (p, X == NO_REAL_MATRIX, M_ROW_ROW_REAL, "empty data matrix");
  MATH_RTE (p, lim < 0, M_REAL, "invalid limit");
  unt M = SIZE1 (X), N = SIZE2 (X);  
// GSL only handles M >= N, transpose commutes with pseudo inverse.
  gsl_matrix *U;
  BOOL_T transpose = (M < N);
  if (transpose) {
    M = SIZE2 (X); N = SIZE1 (X);  
    U = gsl_matrix_calloc (M, N);
    gsl_matrix_transpose_memcpy (U, X);
  } else {
    U = gsl_matrix_calloc (M, N);
    gsl_matrix_memcpy (U, X);
  }
// A = USVᵀ by Jacobi, more precise than Golub-Reinsch.
// The GSL routine yields V, not Vᵀ. U is decomposed in place.
  gsl_matrix *V = gsl_matrix_calloc (N, N);
  gsl_vector *Sv = gsl_vector_calloc (N);
  ASSERT_GSL (gsl_linalg_SV_decomp_jacobi (U, V, Sv));
// Compute S⁻¹. 
// Very small eigenvalues wreak havoc on a pseudo inverse.
// So diagonal elements < 'lim * largest element' are set to zero.
// Python NumPy default is 1e-15, but assumes a Hermitian matrix.
// SVD gives singular values sorted in descending order.
  REAL_T x0 = gsl_vector_get (Sv, 0);
  MATH_RTE (p, x0 == 0, M_ROW_ROW_REAL, "zero eigenvalue or singular value");
  gsl_matrix *S_inv = gsl_matrix_calloc (N, M);
  gsl_matrix_set (S_inv, 0, 0, 1 / x0);
  for (unt i = 1; i < N; i++) {
    REAL_T x = gsl_vector_get (Sv, i);
    if ((x / x0) > lim) {
      gsl_matrix_set (S_inv, i, i, 1 / x);
    } else {
      gsl_matrix_set (S_inv, i, i, 0);
    }
  }
  a68_vector_free (Sv);
// GSL SVD yields thin SVD - pad with zeros.
  gsl_matrix *Uf = gsl_matrix_calloc (M, M);
  for (unt i = 0; i < M; i++) {
    for (unt j = 0; j < N; j++) {
      gsl_matrix_set (Uf, i, j, gsl_matrix_get (U, i, j));
    }
  }
// Compute pseudo inverse A⁻¹ = VS⁻¹Uᵀ. 
  gsl_matrix *VS_inv = NO_REAL_MATRIX, *X_inv = NO_REAL_MATRIX;
  a68_dgemm (I, I, 1, V, S_inv, 0, &VS_inv);
  a68_dgemm (I, T, 1, VS_inv, Uf, 0, &X_inv);
// Compose result.
  if (transpose) {
    (*mpinv) = gsl_matrix_calloc (M, N);
    gsl_matrix_transpose_memcpy ((*mpinv), X_inv);
  } else {
    (*mpinv) = gsl_matrix_calloc (N, M);
    gsl_matrix_memcpy ((*mpinv), X_inv);
  }
// Clean up.
  a68_matrix_free (S_inv);
  a68_matrix_free (U);
  a68_matrix_free (Uf);
  a68_matrix_free (V);
  a68_matrix_free (VS_inv);
  a68_matrix_free (X_inv);
}

//! @brief Compute Principal Component Analysis by SVD.

gsl_matrix *pca_svd (gsl_matrix *X, gsl_vector ** Sv)
{
// In PCA, SVD is closely related to eigenvector decomposition of the covariance matrix:
// If Cov = XᵀX = VLVᵀ and X = USVᵀ then XᵀX = VSUᵀUSVᵀ = VS²Vᵀ, hence L = S².
//
// M samples, N features.
  unt M = SIZE1 (X), N = SIZE2 (X);
// GSL does thin SVD, only handles M >= N, overdetermined systems.    
  BOOL_T transpose = (M < N);
  gsl_matrix *U = NO_REAL_MATRIX;
  if (transpose) {
// Xᵀ = VSUᵀ
    M = SIZE2 (X); N = SIZE1 (X);  
    U = gsl_matrix_calloc (M, N);
    gsl_matrix_transpose_memcpy (U, X);
  } else {
// X = USVᵀ
    U = gsl_matrix_calloc (M, N);
    gsl_matrix_memcpy (U, X);
  }
// X = USVᵀ by Jacobi, more precise than Golub-Reinsch.
// GSL routine yields V, not Vᵀ, U develops in place.
  gsl_matrix *V = gsl_matrix_calloc (N, N);
  (*Sv) = gsl_vector_calloc (N);
  ASSERT_GSL (gsl_linalg_SV_decomp_jacobi (U, V, (*Sv)));
// Return singular values, if required.
  gsl_matrix *eigens = gsl_matrix_calloc (M, N);
  if (transpose) {
    ASSERT_GSL (gsl_matrix_memcpy (eigens, U));
  } else {
    ASSERT_GSL (gsl_matrix_memcpy (eigens, V));
  }
  a68_matrix_free (U);
  a68_matrix_free (V);
  return eigens;
}

//! @brief PROC pseudo inv = ([, ] REAL, REAL) [, ] REAL

void genie_matrix_pinv_lim (NODE_T * p)
{
// Compute the Moore-Penrose pseudo inverse of a MxN matrix.
// G. Strang. "Linear Algebra and its applications", 2nd edition.
// Academic Press [1980].
//
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REAL lim;
  POP_OBJECT (p, &lim, A68_REAL);
  gsl_matrix *X = pop_matrix (p, A68_TRUE), *mpinv = NO_REAL_MATRIX;
  compute_pseudo_inverse (p, &mpinv, X, VALUE (&lim));
  push_matrix (p, mpinv);
  a68_matrix_free (mpinv);
  a68_matrix_free (X);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP PINV = ([, ] REAL) [, ] REAL

void genie_matrix_pinv (NODE_T * p)
{
  PUSH_VALUE (p, SMALL_EIGEN, A68_REAL);
  genie_matrix_pinv_lim (p);
}

//! @brief OP MEAN = ([, ] REAL) [, ] REAL

void genie_matrix_column_mean (NODE_T *p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  gsl_matrix *Z = pop_matrix (p, A68_TRUE); 
  unt M = SIZE1 (Z), N = SIZE2 (Z);
  gsl_matrix *A = gsl_matrix_calloc (M, N);
  for (unt i = 0; i < N; i++) {
    DOUBLE_T sum = 0;
    for (unt j = 0; j < M; j++) {
      sum += gsl_matrix_get (Z, j, i);
    }
    DOUBLE_T mean = sum / M;
    for (unt j = 0; j < M; j++) {
      gsl_matrix_set (A, j, i, mean);
    }
  }
  push_matrix (p, A);
  a68_matrix_free (A);
  a68_matrix_free (Z);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC left columns = ([, ] REAL, INT) [, ] REAL

void genie_left_columns (NODE_T *p)
{
  A68_INT k;
  POP_OBJECT (p, &k, A68_INT);
  gsl_matrix *X = pop_matrix (p, A68_TRUE); 
  gsl_matrix *Y = left_columns (p, X, VALUE (&k));
  push_matrix (p, Y);
  a68_matrix_free (X);
  a68_matrix_free (Y);
}

//! @brief PROC pca cv = ([, ] REAL, REF [] REAL) [, ] REAL

void genie_matrix_pca_cv (NODE_T * p)
{
// Principal component analysis of a MxN matrix from a covariance matrix.
// Forming the covariance matrix squares the condition number.
// Hence this routine might loose more significant digits than SVD.
// On the other hand, using PCA one looks for dominant eigenvectors.
//
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REF singulars;
  POP_REF (p, &singulars);
  gsl_matrix *X = pop_matrix (p, A68_TRUE);
  MATH_RTE (p, X == NO_REAL_MATRIX, M_ROW_ROW_REAL, "empty data matrix");
// M samples, N features.
  unt M = SIZE1 (X), N = SIZE2 (X);
// Keep X column-mean-centered.
  gsl_matrix *C = column_mean (X);
// Covariance matrix, M samples: Cov = XᵀX.
  M = MAX (M, N);
  gsl_matrix *CV = NO_REAL_MATRIX;
  a68_dgemm (T, I, 1, C, C, 0, &CV);
// Compute and sort eigenvectors.
  gsl_vector *Sv = gsl_vector_calloc (M);
  gsl_matrix *eigens = gsl_matrix_calloc (M, M);
  gsl_eigen_symmv_workspace *W = gsl_eigen_symmv_alloc (M);
  ASSERT_GSL (gsl_eigen_symmv (CV, Sv, eigens, W));
  ASSERT_GSL (gsl_eigen_symmv_sort (Sv, eigens, GSL_EIGEN_SORT_ABS_DESC));
// Yield results.
  if (!IS_NIL (singulars)) {
    *DEREF (A68_REF, &singulars) = vector_to_row (p, Sv);
  }
  push_matrix (p, eigens);
  (void) gsl_set_error_handler (save_handler);
// Garbage collector
  a68_matrix_free (eigens);
  a68_matrix_free (X);
  gsl_eigen_symmv_free (W);
  a68_matrix_free (C);
  a68_matrix_free (CV);
  a68_vector_free (Sv);
}

//! @brief PROC pca svd = ([, ] REAL, REF [] REAL) [, ] REAL

void genie_matrix_pca_svd (NODE_T * p)
{
// Principal component analysis of a MxN matrix by Jacobi SVD.
//
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  A68_REF singulars;
  POP_REF (p, &singulars);
// X data matrix, samples in rows, features in columns
  gsl_matrix *X = pop_matrix (p, A68_TRUE);
  MATH_RTE (p, X == NO_REAL_MATRIX, M_ROW_ROW_REAL, "empty data matrix");
// Keep X column-mean-centered.
  gsl_matrix *C = column_mean (X);
  gsl_vector *Sv = NO_REAL_VECTOR;
  gsl_matrix *eigens = pca_svd (C, &Sv);
// Yield results.
  if (!IS_NIL (singulars)) {
    *DEREF (A68_REF, &singulars) = vector_to_row (p, Sv);
  }
  push_matrix (p, eigens);
  (void) gsl_set_error_handler (save_handler);
// Clean up.
  a68_matrix_free (eigens);
  a68_matrix_free (X);
  a68_matrix_free (C);
  if (Sv != NO_REAL_VECTOR) {
    a68_vector_free (Sv);
  }
}

//! @brief PROC pcr = ([, ] REAL, [, ] REAL, REF [] REAL, INT, REAL) [, ] REAL

void genie_matrix_pcr (NODE_T * p)
{
// Principal Component Regression of a MxN matrix by Jacobi SVD.
//
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
// Pop arguments.
// 'lim' is minimum relative length to first eigenvector.
  A68_REAL lim;
  POP_OBJECT (p, &lim, A68_REAL); 
// 'Nc' is maximum number of eigenvectors.
  A68_INT Nc;
  POP_OBJECT (p, &Nc, A68_INT); 
//
  A68_REF singulars;
  POP_REF (p, &singulars);
// Y data vector, features in a single column
  gsl_matrix *Y = pop_matrix (p, A68_TRUE);
  MATH_RTE (p, Y == NO_REAL_MATRIX, M_ROW_ROW_REAL, "empty data matrix");
  unt My = SIZE1 (Y), Ny = SIZE2 (Y);
// X data matrix, samples in rows, features in columns
  gsl_matrix *X = pop_matrix (p, A68_TRUE);
  MATH_RTE (p, X == NO_REAL_MATRIX, M_ROW_ROW_REAL, "empty data matrix");
  unt Mx = SIZE1 (X), Nx = SIZE2 (X);
// Catch wrong calls.
  MATH_RTE (p, My == 0 || Ny == 0, M_ROW_ROW_REAL, "invalid column vector F");
  MATH_RTE (p, Mx == 0 || Nx == 0, M_ROW_ROW_REAL, "invalid data matrix E");
  MATH_RTE (p, Mx != My, M_ROW_ROW_REAL, "rows in F must match columns in E");
  MATH_RTE (p, Ny != 1, M_ROW_ROW_REAL, "F must be a column vector");
  MATH_RTE (p, VALUE (&lim) < 0 || VALUE (&lim) > 1.0, M_REAL, "invalid relative length");
// Keep X column-mean-centered.
  gsl_matrix *C = column_mean (X);
  gsl_vector *Sv = NO_REAL_VECTOR;
  gsl_matrix *eigens = pca_svd (C, &Sv);
// Dimension reduction.
  int Nk = (VALUE (&Nc) == 0 ? SIZE (Sv) : MIN (SIZE (Sv), VALUE (&Nc)));
  if (VALUE (&lim) <= 0) {
    VALUE (&lim) = SMALL_EIGEN;
  }
// Too short eigenvectors wreak havoc.
  REAL_T Sv0 = gsl_vector_get (Sv, 0);
  for (unt k = 1; k < SIZE (Sv); k++) {
    if (gsl_vector_get (Sv, k) / Sv0 < VALUE (&lim)) {
      if (k + 1 < Nk) {
        Nk = k + 1;
      }
    }
  }
  gsl_matrix *reduced = left_columns (p, eigens, Nk);
// Compute projected set = X * reduced.
  gsl_matrix *proj = NO_REAL_MATRIX;
  a68_dgemm (I, I, 1.0, X, reduced, 0.0, &proj);
// Compute β = reduced * P⁻¹ * Y.
  gsl_matrix *mpinv = NO_REAL_MATRIX;
  compute_pseudo_inverse (p, &mpinv, proj, SMALL_EIGEN);
  gsl_matrix *z = NO_REAL_MATRIX, *beta = NO_REAL_MATRIX;
  a68_dgemm (I, I, 1.0, reduced, mpinv, 0.0, &z);
  a68_dgemm (I, I, 1.0, z, Y, 0.0, &beta);
// Yield results.
  if (!IS_NIL (singulars)) {
    *DEREF (A68_REF, &singulars) = vector_to_row (p, Sv);
  }
  push_matrix (p, beta);
  (void) gsl_set_error_handler (save_handler);
// Clean up.
  a68_matrix_free (eigens);
  a68_matrix_free (reduced);
  a68_matrix_free (X);
  a68_matrix_free (Y);
  a68_matrix_free (C);
  a68_vector_free (Sv);
  a68_matrix_free (z);
  a68_matrix_free (mpinv);
  a68_matrix_free (proj);
  a68_matrix_free (beta);
}

//! @brief PROC ols = ([, ] REAL, [, ] REAL) [, ] REAL

void genie_matrix_ols (NODE_T *p)
{
// TLS relates to PLS as OLS relates to PCR.
// Note that X is an MxN matrix and Y, β are Nx1 column vectors.
// OLS can implemented (inefficiently) as PCR with maximum number of singular values:
//
//  PUSH_VALUE (p, 0.0, A68_REAL);
//  PUSH_VALUE (p, 0, A68_INT);
//  genie_matrix_pcr (p);
//
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
// X and Y matrices.
  gsl_matrix *Y = pop_matrix (p, A68_TRUE);
  gsl_matrix *X = pop_matrix (p, A68_TRUE);
// Compute β = X⁻¹ * Y.
  gsl_matrix *mpinv = NO_REAL_MATRIX, *beta = NO_REAL_MATRIX;
  compute_pseudo_inverse (p, &mpinv, X, SMALL_EIGEN);
  a68_dgemm (I, I, 1.0, X, Y, 0.0, &beta);
// Yield results.
  push_matrix (p, beta);
  (void) gsl_set_error_handler (save_handler);
//
  a68_matrix_free (beta);
  a68_matrix_free (mpinv);
  a68_matrix_free (X);
  a68_matrix_free (Y);
}

//! @brief PROC pls1 = ([, ] REAL, [, ] REAL, REF [] REAL, INT, REAL) [, ] REAL

void genie_matrix_pls1 (NODE_T *p)
{
// PLS decomposes X and Y data concurrently as
//
// X = T Pᵀ + E
// Y = U Qᵀ + F
//
// PLS1 is a widely used algorithm appropriate for the vector Y case.
//
// This procedure computes PLS1 with SVD solver for β,
// by a NIPALS algorithm with orthonormal scores and loadings.
// See Ulf Indahl, Journal of Chemometrics 28(3) 168-180 [2014].
//
// Note that E is an MxN matrix and F, β are Nx1 column vectors.
// This for consistency with PLS2.
//
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
// Pop arguments.
// 'lim' is minimum relative length to first eigenvector.
  A68_REAL lim;
  POP_OBJECT (p, &lim, A68_REAL); 
// 'Nc' is maximum number of eigenvectors.
  A68_INT Nc;
  POP_OBJECT (p, &Nc, A68_INT); 
//
  A68_REF singulars;
  POP_REF (p, &singulars);
// X and Y matrices.
  gsl_matrix *F = pop_matrix (p, A68_TRUE);
  gsl_matrix *E = pop_matrix (p, A68_TRUE);
// Catch wrong calls.
  unt Me = SIZE1 (E), Ne = SIZE2 (E), Mf = SIZE1 (F), Nf = SIZE2 (F);
  MATH_RTE (p, Mf == 0 || Nf == 0, M_ROW_ROW_REAL, "invalid column vector F");
  MATH_RTE (p, Me == 0 || Ne == 0, M_ROW_ROW_REAL, "invalid data matrix E");
  MATH_RTE (p, Me != Mf, M_ROW_ROW_REAL, "rows in F must match columns in E");
  MATH_RTE (p, Nf != 1, M_ROW_ROW_REAL, "F must be a column vector");
  MATH_RTE (p, VALUE (&lim) < 0 || VALUE (&lim) > 1.0, M_REAL, "invalid relative length");
// Sensible defaults.
  int Nk = (VALUE (&Nc) == 0 ? MIN (Ne, Mf) : MIN (Mf, VALUE (&Nc)));
  if (VALUE (&lim) <= 0) {
    VALUE (&lim) = SMALL_EIGEN;
  }
// Decompose E and F.
  gsl_matrix *EIGEN = NO_REAL_MATRIX, *nE = NO_REAL_MATRIX, *nF = NO_REAL_MATRIX; 
  gsl_vector *Sv = gsl_vector_calloc (Nk);
  BOOL_T go_on = A68_TRUE;
  gsl_matrix *eigens = NO_REAL_MATRIX, *lat = NO_REAL_MATRIX, *pl = NO_REAL_MATRIX, *ql = NO_REAL_MATRIX;
  for (int k = 0; k < Nk && go_on; k ++) {
// E weight from E, F covariance.
// eigens = Eᵀ * f / | Eᵀ * f |
    a68_dgemm (T, I, 1.0, E, F, 0.0, &eigens);
    REAL_T norm = matrix_norm (eigens);
    if (k > 0 && (norm / gsl_vector_get (Sv, 0)) < VALUE (&lim)) {
      Nk = k;
      go_on = A68_FALSE;
    } else {
      ASSERT_GSL (gsl_matrix_scale (eigens, 1.0 / norm));
      gsl_vector_set (Sv, k, norm);
// Compute latent variable.
// lat = E * eigens / | E * eigens |
      a68_dgemm (I, I, 1.0, E, eigens, 0.0, &lat);
      norm = matrix_norm (lat);
      ASSERT_GSL (gsl_matrix_scale (lat, 1.0 / norm));
// Deflate E and F, remove latent variable from both.
// pl = Eᵀ * lat; E -= lat * plᵀ and ql = Fᵀ * lat; F -= lat * qlᵀ
      a68_dgemm (T, I, 1.0, E, lat, 0.0, &pl);
      a68_dgemm (I, T, -1.0, lat, pl, 1.0, &E);
      a68_dgemm (T, I, 1.0, F, lat, 0.0, &ql);
      a68_dgemm (I, T, -1.0, lat, ql, 1.0, &F);
// Build matrices.
      EIGEN = mat_before_ab (p, EIGEN, eigens);
      nE = mat_before_ab (p, nE, pl); // P
      nF = mat_over_ab (p, nF, ql);   // Qᵀ
    }
  }
// Projection of original data = Eᵀ * EIGEN
  gsl_matrix *nP = NO_REAL_MATRIX;
  a68_dgemm (T, I, 1.0, nE, EIGEN, 0.0, &nP);
// Compute β = EIGEN * nP⁻¹ * nF
  gsl_matrix *mpinv = NO_REAL_MATRIX, *z = NO_REAL_MATRIX, *beta = NO_REAL_MATRIX;
  compute_pseudo_inverse (p, &mpinv, nP, SMALL_EIGEN);
  a68_dgemm (I, I, 1.0, EIGEN, mpinv, 0.0, &z);
  a68_dgemm (I, I, 1.0, z, nF, 0.0, &beta);
// Yield results.
  if (!IS_NIL (singulars)) {
    gsl_vector *Svl = gsl_vector_calloc (Nk);
    for (unt k = 0; k < Nk; k++) {
      gsl_vector_set (Svl, k, gsl_vector_get (Sv, k));
    }
    *DEREF (A68_REF, &singulars) = vector_to_row (p, Svl);
    a68_vector_free (Svl);
  }
  push_matrix (p, beta);
  (void) gsl_set_error_handler (save_handler);
// Garbage collector.
  a68_matrix_free (E);
  a68_matrix_free (F);
  a68_matrix_free (eigens);
  a68_matrix_free (lat);
  a68_matrix_free (pl);
  a68_matrix_free (ql);
  a68_matrix_free (beta);
  a68_matrix_free (EIGEN);
  a68_matrix_free (nE);
  a68_matrix_free (nF);
  a68_matrix_free (nP);
  a68_vector_free (Sv);
}

//! @brief PROC pls2 = ([, ] REAL, [, ] REAL, REF [] REAL, INT, REAL) [, ] REAL

void genie_matrix_pls2 (NODE_T *p)
{
// PLS decomposes X and Y data concurrently as
//
// X = T Pᵀ + E
// Y = U Qᵀ + F
//
// Generic orthodox NIPALS, following Hervé Abdi, University of Texas.
// "Partial Least Squares Regression and Projection on Latent Structure Regression",
// Computational Statistics [2010].
//
#define PLS_MAX_ITER 100
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
// Pop arguments.
// 'lim' is minimum relative length to first eigenvector.
  A68_REAL lim;
  POP_OBJECT (p, &lim, A68_REAL); 
// 'Nc' is maximum number of eigenvectors.
  A68_INT Nc;
  POP_OBJECT (p, &Nc, A68_INT); 
//
  A68_REF singulars;
  POP_REF (p, &singulars);
// X and Y matrices.
  gsl_matrix *F = pop_matrix (p, A68_TRUE);
  gsl_matrix *E = pop_matrix (p, A68_TRUE);
// Catch wrong calls.
  unt Me = SIZE1 (E), Ne = SIZE2 (E), Mf = SIZE1 (F), Nf = SIZE2 (F);
  MATH_RTE (p, Mf == 0 || Nf == 0, M_ROW_ROW_REAL, "invalid column vector F");
  MATH_RTE (p, Me == 0 || Ne == 0, M_ROW_ROW_REAL, "invalid data matrix E");
  MATH_RTE (p, Me != Mf, M_ROW_ROW_REAL, "rows in F must match columns in E");
  CHECK_INT_SHORTEN(p, VALUE (&Nc));
// Sensible defaults.
  int Nk = (VALUE (&Nc) == 0 ? MIN (Ne, Mf) : MIN (Mf, VALUE (&Nc)));
  if (VALUE (&lim) <= 0) {
    VALUE (&lim) = SMALL_EIGEN;
  }
// Initial vector u.
  gsl_matrix *u = gsl_matrix_calloc (Mf, 1);
  if (Nf == 1) { // Column vector, PLS1
    for (int k = 0; k < Mf; k ++) {
      gsl_matrix_set (u, k, 0, gsl_matrix_get (F, k, 0));
    }
  } else {
    for (int k = 0; k < Mf; k ++) {
      gsl_matrix_set (u, k, 0, a68_gauss_rand ());
    }
  }
// Decompose E and F jointly.
  gsl_vector *Sv = gsl_vector_calloc (Nk), *dD = gsl_vector_calloc (Nk);
  gsl_matrix *u0 = gsl_matrix_calloc (Mf, 1), *diff = gsl_matrix_calloc (Mf, 1);
  gsl_matrix *eigens = NO_REAL_MATRIX;
  gsl_matrix *t = NO_REAL_MATRIX, *b = NO_REAL_MATRIX, *c = NO_REAL_MATRIX;
  gsl_matrix *pl = NO_REAL_MATRIX, *ql = NO_REAL_MATRIX, *nP = NO_REAL_MATRIX, *nC = NO_REAL_MATRIX;
  BOOL_T go_on = A68_TRUE;
  for (int k = 0; k < Nk && go_on; k ++) {
    REAL_T norm_e, norm;
    for (int j = 0; j < PLS_MAX_ITER; j ++) {
      gsl_matrix_memcpy (u0, u);
// Compute X weight.  Note that Eᵀ * u is of form [Indahl]
// Eᵀ * F * Fᵀ * E * w / |Eᵀ * F * Fᵀ * w| = (1 / lambda) (Fᵀ * E)ᵀ * (Fᵀ * E) * w,
// that is, w is an eigenvector of covariance matrix Fᵀ * E and 
// longest eigenvector of symmetric matrix Eᵀ * F * Fᵀ * E.
      a68_dgemm (T, I, 1.0, E, u, 0.0, &eigens);
      norm_e = matrix_norm (eigens);
      ASSERT_GSL (gsl_matrix_scale (eigens, 1.0 / norm_e));
// X factor score.
      a68_dgemm (I, I, 1.0, E, eigens, 0.0, &t);
      norm = matrix_norm (t);
      ASSERT_GSL (gsl_matrix_scale (t, 1.0 / norm));
// Y weight.
      a68_dgemm (T, I, 1.0, F, t, 0.0, &c);
      norm = matrix_norm (c);
      ASSERT_GSL (gsl_matrix_scale (c, 1.0 / norm));
// Y score.
      a68_dgemm (I, I, 1.0, F, c, 0.0, &u);
      gsl_matrix_memcpy (diff, u);
      gsl_matrix_sub (diff, u0);
      norm = matrix_norm (diff);
      if (norm < SMALL_EIGEN) {
        j = PLS_MAX_ITER;
      }   
    }
    if (k > 0 && (norm_e / gsl_vector_get (Sv, 0)) < VALUE (&lim)) {
      Nk = k;
      go_on = A68_FALSE;
    } else {
      gsl_vector_set (Sv, k, norm_e);
// X factor loading and deflation.
      a68_dgemm (T, I, 1.0, E, t, 0.0, &pl);
      norm = matrix_norm (t);
      ASSERT_GSL (gsl_matrix_scale (pl, 1.0 / (norm * norm)));
      a68_dgemm (I, T, -1.0, t, pl, 1.0, &E);
// Y factor loading and deflation.
      a68_dgemm (T, I, 1.0, F, u, 0.0, &ql);
      norm = matrix_norm (u);
      ASSERT_GSL (gsl_matrix_scale (ql, 1.0 / (norm * norm)));
      a68_dgemm (T, I, 1.0, t, u, 0.0, &b);
      a68_dgemm (I, T, -gsl_matrix_get (b, 0, 0), t, ql, 1.0, &F);
// Build vector and matrices.
      gsl_vector_set (dD, k, gsl_matrix_get(b, 0, 0));
      nP = mat_before_ab (p, nP, pl); // P
      nC = mat_before_ab (p, nC, c);  // C
    }
  }
// Compute β = (Pᵀ)⁻¹ * D * Cᵀ
// Python diag function.
  gsl_matrix *D = gsl_matrix_calloc (Nk, Nk);
  for (int k = 0; k < Nk; k++) {
    gsl_matrix_set (D, k, k, gsl_vector_get (dD, k));
  }
//
  gsl_matrix *mpinv = NO_REAL_MATRIX;
  compute_pseudo_inverse (p, &mpinv, nP, SMALL_EIGEN);
  gsl_matrix *z = NO_REAL_MATRIX, *beta = NO_REAL_MATRIX;
  a68_dgemm (T, I, 1.0, mpinv, D, 0.0, &z);
  a68_dgemm (I, T, 1.0, z, nC, 0.0, &beta);
// Yield results.
  if (!IS_NIL (singulars)) {
    gsl_vector *Svl = gsl_vector_calloc (Nk);
    for (unt k = 0; k < Nk; k++) {
      gsl_vector_set (Svl, k, gsl_vector_get (Sv, k));
    }
    *DEREF (A68_REF, &singulars) = vector_to_row (p, Svl);
    a68_vector_free (Svl);
  }
  push_matrix (p, beta);
  (void) gsl_set_error_handler (save_handler);
// Garbage collector.
  a68_vector_free (dD);
  a68_vector_free (Sv);
  a68_matrix_free (b);
  a68_matrix_free (beta);
  a68_matrix_free (c);
  a68_matrix_free (D);
  a68_matrix_free (diff);
  a68_matrix_free (eigens);
  a68_matrix_free (nC);
  a68_matrix_free (nP);
  a68_matrix_free (mpinv);
  a68_matrix_free (pl);
  a68_matrix_free (ql);
  a68_matrix_free (t);
  a68_matrix_free (u);
  a68_matrix_free (u0);
  a68_matrix_free (z);
#undef PLS_MAX_ITER
}

//! @brief PROC tls = ([, ] REAL, [, ] REAL, REF [] REAL) [, ] REAL

void genie_matrix_tls (NODE_T *p)
{
// TLS relates to PLS as OLS relates to PCR.
// TLS is implemented as PLS1 with maximum number of singular values.
  PUSH_VALUE (p, 0.0, A68_REAL);
  PUSH_VALUE (p, 0, A68_INT);
  genie_matrix_pls1 (p);
}

#endif
