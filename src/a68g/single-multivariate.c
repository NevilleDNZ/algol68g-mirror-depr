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

#include "a68g.h"

#if defined (HAVE_GSL)

#include "a68g-torrix.h"
#include "a68g-prelude-gsl.h"

//! @brief Compute pseudo_inverse := pseudo inverse (X) 

void compute_pseudo_inverse (NODE_T *p, gsl_matrix **pseudo_inverse, gsl_matrix *X, REAL_T lim)
{
//
// The pseudo inverse gives a least-square approximate solution for a 
// system of linear equations not having an exact solution.
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
// Diagonal elements < 'lim' * max (singular values) are set to zero.
// Very small eigenvalues wreak havoc on a pseudo inverse.
// Python NumPy default is 1e-15, but assumes a Hermitian matrix.
// SVD gives singular values sorted in descending order.
  REAL_T lwb = gsl_vector_get (Sv, 0) * (lim > 0 ? lim : 1e-9);
  gsl_matrix *S_inv = gsl_matrix_calloc (N, M); // calloc sets matrix to 0.
  for (unt i = 0; i < N; i++) {
    REAL_T x = gsl_vector_get (Sv, i);
    if (x > lwb) {
      gsl_matrix_set (S_inv, i, i, 1 / x);
    } else {
      gsl_matrix_set (S_inv, i, i, 0);
    }
  }
  gsl_vector_free (Sv);
// GSL SVD yields thin SVD - pad with zeros.
  gsl_matrix *Uf = gsl_matrix_calloc (M, M);
  for (unt i = 0; i < M; i++) {
    for (unt j = 0; j < N; j++) {
      gsl_matrix_set (Uf, i, j, gsl_matrix_get (U, i, j));
    }
  }
// Compute pseudo inverse A⁻¹ = VS⁻¹Uᵀ. 
  gsl_matrix *VS_inv = gsl_matrix_calloc (N, M);
  gsl_matrix *X_inv = gsl_matrix_calloc (N, M);
  ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 1, V, S_inv, 0, VS_inv));
  ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasTrans, 1, VS_inv, Uf, 0, X_inv));
// Compose result.
  if (transpose) {
    (*pseudo_inverse) = gsl_matrix_calloc (M, N);
    gsl_matrix_transpose_memcpy ((*pseudo_inverse), X_inv);
  } else {
    (*pseudo_inverse) = gsl_matrix_calloc (N, M);
    gsl_matrix_memcpy ((*pseudo_inverse), X_inv);
  }
// Clean up.
  gsl_matrix_free (S_inv);
  gsl_matrix_free (U);
  gsl_matrix_free (Uf);
  gsl_matrix_free (V);
  gsl_matrix_free (VS_inv);
  gsl_matrix_free (X_inv);
}

//! @brief PROC pseudo inv = ([, ] REAL, REAL) [, ] REAL

void genie_matrix_pinv_lim (NODE_T * p)
{
// Compute the Moore-Penrose pseudo inverse of a MxN matrix.
// G. Strang. "Linear Algebra and its applications", 2nd edition.
// Academic Press [1980].
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REAL lim;
  POP_OBJECT (p, &lim, A68_REAL);
  gsl_matrix *X = pop_matrix (p, A68_TRUE), *pseudo_inverse = NO_REAL_MATRIX;
  compute_pseudo_inverse (p, &pseudo_inverse, X, VALUE (&lim));
  push_matrix (p, pseudo_inverse);
  gsl_matrix_free (pseudo_inverse);
  gsl_matrix_free (X);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief OP PINV = ([, ] REAL) [, ] REAL

void genie_matrix_pinv (NODE_T * p)
{
  PUSH_VALUE (p, 0, A68_REAL);
  genie_matrix_pinv_lim (p);
}

//! @brief column-centered data matrix.

gsl_matrix *column_mean (gsl_matrix *DATA)
{
// M samples, N features.
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

//! @brief take left columns from matrix.

void genie_left_columns (NODE_T *p)
{
  A68_INT k;
  POP_OBJECT (p, &k, A68_INT);
  gsl_matrix *X = pop_matrix (p, A68_TRUE); 
  unt M = SIZE1 (X), N = SIZE2 (X), cols = VALUE (&k);
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
  push_matrix (p, Y);
  gsl_matrix_free (X);
  gsl_matrix_free (Y);
}

//! @brief OP MEAN = ([, ] REAL) [, ] REAL

void genie_matrix_column_mean (NODE_T *p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
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
  gsl_matrix_free (A);
  gsl_matrix_free (Z);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief compute PCA of MxN matrix from covariance matrix
//! @param eigen_values vector with eigen values on output
//! @param X data matrix, samples in rows, features in columns

gsl_matrix *compute_pca_cv (NODE_T *p, gsl_vector **eigen_values, gsl_matrix *X)
{
//
// Forming the covariance matrix squares the condition number.
// Hence this routine might loose more significant digits than SVD.
// On the other hand, using PCA one looks for dominant eigenvectors.
//
  MATH_RTE (p, X == NO_REAL_MATRIX, M_ROW_ROW_REAL, "empty data matrix");
// M samples, N features.
  unt M = SIZE1 (X), N = SIZE2 (X);
// Keep X column-mean-centered.
  gsl_matrix *C = column_mean (X);
// Covariance matrix, M samples: Cov = XᵀX.
  M = MAX (M, N);
  gsl_matrix *CV = gsl_matrix_calloc (M, M);
  gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1, C, C, 0, CV);
// Compute and sort eigenvectors.
  gsl_vector *Ev = gsl_vector_calloc (M);
  gsl_matrix *eigen_vectors = gsl_matrix_calloc (M, M);
  gsl_eigen_symmv_workspace *W = gsl_eigen_symmv_alloc (M);
  ASSERT_GSL (gsl_eigen_symmv (CV, Ev, eigen_vectors, W));
  ASSERT_GSL (gsl_eigen_symmv_sort (Ev, eigen_vectors, GSL_EIGEN_SORT_ABS_DESC));
// Return eigen values, if required.
  if (eigen_values != NO_REF_VECTOR) {
    (*eigen_values) = gsl_vector_calloc (N);
    ASSERT_GSL (gsl_vector_memcpy ((*eigen_values), Ev));
  }
// Clean up.
  gsl_eigen_symmv_free (W);
  gsl_matrix_free (C);
  gsl_matrix_free (CV);
  gsl_vector_free (Ev);
  return eigen_vectors;
}

//! @brief PROC pca cv = ([, ] REAL) [, ] REAL

void genie_matrix_pca_cv (NODE_T * p)
{
// Principal component analysis of a MxN matrix.
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REF ref_row;
  POP_REF (p, &ref_row);
  gsl_matrix *X = pop_matrix (p, A68_TRUE);
  gsl_vector *Ev;
  gsl_matrix *eigen_vectors = compute_pca_cv (p, &Ev, X);
  *DEREF (A68_REF, &ref_row) = vector_to_row (p, Ev);
  push_matrix (p, eigen_vectors);
  gsl_matrix_free (eigen_vectors);
  gsl_matrix_free (X);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief compute PCA of MxN matrix by Jacobi SVD
//! @param singular_values vector with singular values on output
//! @param X data matrix, samples in rows, features in columns

gsl_matrix *compute_pca_svd (NODE_T *p, gsl_vector **singular_values, gsl_matrix *X)
{
//
// In PCA, SVD is closely related to eigenvector decomposition of the covariance matrix:
// If Cov = XᵀX = VLVᵀ and X = USVᵀ then XᵀX = VSUᵀUSVᵀ = VS²Vᵀ, hence L = S².
//
  MATH_RTE (p, X == NO_REAL_MATRIX, M_ROW_ROW_REAL, "empty data matrix");
// Keep X column-mean-centered.
  gsl_matrix *C = column_mean (X), *U;
// M samples, N features.
  unt M = SIZE1 (X), N = SIZE2 (X);
// GSL does thin SVD, only handles M >= N, overdetermined systems.    
  BOOL_T transpose = (M < N);
  if (transpose) {
// Xᵀ = VSUᵀ
    M = SIZE2 (X); N = SIZE1 (X);  
    U = gsl_matrix_calloc (M, N);
    gsl_matrix_transpose_memcpy (U, C);
  } else {
// X = USVᵀ
    U = gsl_matrix_calloc (M, N);
    gsl_matrix_memcpy (U, C);
  }
// X = USVᵀ by Jacobi, more precise than Golub-Reinsch.
// GSL routine yields V, not Vᵀ, U develops in place.
  gsl_matrix *V = gsl_matrix_calloc (N, N);
  gsl_vector *Sv = gsl_vector_calloc (N);
  ASSERT_GSL (gsl_linalg_SV_decomp_jacobi (U, V, Sv));
// Return singular values, if required.
  if (singular_values != NO_REF_VECTOR) {
    (*singular_values) = gsl_vector_calloc (N);
    ASSERT_GSL (gsl_vector_memcpy ((*singular_values), Sv));
  }
  gsl_matrix *eigen_vectors = gsl_matrix_calloc (N, M);
  if (transpose) {
    eigen_vectors = gsl_matrix_calloc (M, N);
    ASSERT_GSL (gsl_matrix_memcpy (eigen_vectors, U));
  } else {
    eigen_vectors = gsl_matrix_calloc (M, N);
    ASSERT_GSL (gsl_matrix_memcpy (eigen_vectors, V));
  }
// Clean up.
  gsl_matrix_free (C);
  gsl_matrix_free (U);
  gsl_matrix_free (V);
  gsl_vector_free (Sv);
// Done.
  return eigen_vectors;
}

//! @brief PROC pca svd = ([, ] REAL, REF [] REAL) [, ] REAL

void genie_matrix_pca_svd (NODE_T * p)
{
// Principal component analysis of a MxN matrix.
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
  A68_REF ref_row;
  POP_REF (p, &ref_row);
  gsl_matrix *X = pop_matrix (p, A68_TRUE);
  gsl_vector *Sv;
  gsl_matrix *eigen_vectors = compute_pca_svd (p, &Sv, X);
  if (!IS_NIL (ref_row)) {
    *DEREF (A68_REF, &ref_row) = vector_to_row (p, Sv);
  }
  push_matrix (p, eigen_vectors);
  gsl_matrix_free (eigen_vectors);
  gsl_matrix_free (X);
  (void) gsl_set_error_handler (save_handler);
}


static gsl_matrix *mat_before_ab (NODE_T *p, gsl_matrix *u, gsl_matrix *v)
{
// Form A := A BEFORE B. Deallocate A, otherwise PLS1 leaks memory.
  gsl_matrix *w = matrix_hcat (p, u, v);
  if (u != NO_REAL_MATRIX) {
    gsl_matrix_free (u);
  }
  return w;
}

static gsl_matrix *mat_over_ab (NODE_T *p, gsl_matrix *u, gsl_matrix *v)
{
// Form A := A OVER B. Deallocate A, otherwise PLS1 leaks memory.
  gsl_matrix *w = matrix_vcat (p, u, v);
  if (u != NO_REAL_MATRIX) {
    gsl_matrix_free (u);
  }
  return w;
}

//! @brief PROC pls1 = ([, ] REAL, [, ] REAL, INT, REF [] REAL) [, ] REAL

void genie_matrix_pls1 (NODE_T *p)
{
// PLS decomposes X and Y data concurrently as
//
// X = T Pᵀ + E
// Y = U Qᵀ + F
//
// PLS1 is a widely used algorithm appropriate for the vector Y case.
//
// PLS1 with SVD solver for beta.
// NIPALS algorithm with orthonormal scores and loadings.
// See Ulf Indahl, Journal of Chemometrics 28(3) 168-180 [2014].
//
// Note that E is an MxN matrix and f, beta are Nx1 column vectors.
// This for consistency with PLS2.
//
// Decompose Nc eigenvectors.
// 
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
// Pop arguments.
  A68_REF ref_eigenv;
  POP_REF (p, &ref_eigenv);
  A68_INT Nc;
  POP_OBJECT (p, &Nc, A68_INT); 
  gsl_matrix *F = pop_matrix (p, A68_TRUE);
  gsl_matrix *E = pop_matrix (p, A68_TRUE);
// Catch wrong calls.
  unt Me = SIZE1 (E), Ne = SIZE2 (E), Mf = SIZE1 (F), Nf = SIZE2 (F);
  MATH_RTE (p, Mf == 0 || Nf == 0, M_ROW_ROW_REAL, "invalid column vector F");
  MATH_RTE (p, Me == 0 || Ne == 0, M_ROW_ROW_REAL, "invalid data matrix E");
  MATH_RTE (p, Me != Mf, M_ROW_ROW_REAL, "rows in F must match columns in E");
  MATH_RTE (p, Nf != 1, M_ROW_ROW_REAL, "F must be a column vector");
  MATH_RTE (p, VALUE (&Nc) < 0, M_INT, "invalid number of components");
  CHECK_INT_SHORTEN(p, VALUE (&Nc));
// Sensible defaults.
  int Nk = VALUE (&Nc);
  if (Nk > Ne) {
    Nk = Ne;
  } else if (Nk == 0) { // All eigenvectors.
    Nk = Me;
  }
// Decompose E and F.
  gsl_matrix *EIGEN = NO_REAL_MATRIX, *nE = NO_REAL_MATRIX, *nF = NO_REAL_MATRIX; 
  gsl_matrix *eig = gsl_matrix_calloc (Ne, 1);
  gsl_matrix *lat = gsl_matrix_calloc (Me, 1);
  gsl_matrix *pl = gsl_matrix_calloc (Ne, 1);
  gsl_matrix *ql = gsl_matrix_calloc (Nf, 1); // 1x1 in PLS1
  gsl_vector *Ev = gsl_vector_calloc (Nk);
// Latent variable deflation.
  for (int k = 0; k < Nk; k ++) {
// E weight from E, F covariance.
// eig = Eᵀ * f / | Eᵀ * f |
    ASSERT_GSL (gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1.0, E, F, 0.0, eig));
    REAL_T norm = matrix_norm (eig);
    ASSERT_GSL (gsl_matrix_scale (eig, 1.0 / norm));
    gsl_vector_set (Ev, k, norm);
// Compute latent variable.
// lat = E * eig / | E * eig |
    ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 1.0, E, eig, 0.0, lat));
    norm = matrix_norm (lat);
    ASSERT_GSL (gsl_matrix_scale (lat, 1.0 / norm));
// Deflate E and F, remove latent variable from both.
// pl = Eᵀ * lat; E -= lat * plᵀ and ql = Fᵀ * lat; F -= lat * qlᵀ
    ASSERT_GSL (gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1.0, E, lat, 0.0, pl));
    ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasTrans, -1.0, lat, pl, 1.0, E));
    ASSERT_GSL (gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1.0, lat, F, 0.0, ql));
// Build matrices.
    EIGEN = mat_before_ab (p, EIGEN, eig);
    nE = mat_before_ab (p, nE, pl); // P
    nF = mat_over_ab (p, nF, ql);   // Qᵀ
  }
// Intermediate garbage collector.
  gsl_matrix_free (E);
  gsl_matrix_free (F);
  gsl_matrix_free (eig);
  gsl_matrix_free (lat);
  gsl_matrix_free (pl);
  gsl_matrix_free (ql);
// Projection of original data = Eᵀ * EIGEN
  gsl_matrix *Pr = gsl_matrix_calloc (SIZE2 (nE), SIZE2 (EIGEN));
  ASSERT_GSL (gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1.0, nE, EIGEN, 0.0, Pr));
// 
// Now beta = EIGEN * Pr⁻¹ * nF
// Compute with SVD to avoid matrix inversion.
// Matrix inversion is a source of numerical noise!
// 
  gsl_matrix *u, *v; gsl_vector *s, *w;
  unt M = SIZE1 (Pr), N = SIZE2 (Pr);
// GSL computes thin SVD, M >= N only, returning V, not Vᵀ.
  if (M >= N) {
// X = USVᵀ
    s = gsl_vector_calloc (N);
    u = gsl_matrix_calloc (M, N);
    v = gsl_matrix_calloc (N, N);
    w = gsl_vector_calloc (N);
    gsl_matrix_memcpy (u, Pr);
    ASSERT_GSL (gsl_linalg_SV_decomp (u, v, s, w));
  } else {
// Xᵀ = VSUᵀ
    s = gsl_vector_calloc (M);
    u = gsl_matrix_calloc (N, M);
    v = gsl_matrix_calloc (M, M);
    w = gsl_vector_calloc (M);
    gsl_matrix_transpose_memcpy (u, Pr);
    ASSERT_GSL (gsl_linalg_SV_decomp (u, v, s, w));
  }
// Kill short eigenvectors cf. Python numpy.
  for (int k = 1; k < SIZE (s); k++) {
    if (gsl_vector_get (s, k) / gsl_vector_get (s, 0) < 1e-15) {
      gsl_vector_set (s, k, 0.0);
    }
  }
// Solve for beta.
  gsl_vector *nFv = gsl_vector_calloc (Nk), *x = gsl_vector_calloc (Nk);
  gsl_matrix_get_col (nFv, nF, 0);
  if (M >= N) {
    ASSERT_GSL (gsl_linalg_SV_solve (u, v, s, nFv, x));
  } else {
    ASSERT_GSL (gsl_linalg_SV_solve (v, u, s, nFv, x));
  }
  gsl_matrix *beta = gsl_matrix_calloc (Ne, 1), *xmat = gsl_matrix_calloc (Nk, 1);
  ASSERT_GSL (gsl_matrix_set_col (xmat, 0, x));
  ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 1.0, EIGEN, xmat, 0.0, beta));
// Yield results.
  if (!IS_NIL (ref_eigenv)) {
    *DEREF (A68_REF, &ref_eigenv) = vector_to_row (p, Ev);
  }
  push_matrix (p, beta);
// Garbage collector.
  gsl_matrix_free (beta);
  gsl_matrix_free (EIGEN);
  gsl_matrix_free (nE);
  gsl_matrix_free (nF);
  gsl_matrix_free (Pr);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  gsl_matrix_free (xmat);
  gsl_vector_free (Ev);
  gsl_vector_free (nFv);
  gsl_vector_free (s);
  gsl_vector_free (w);
  gsl_vector_free (x);
//
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC pls1 lim = ([, ] REAL, [, ] REAL, REAL, REF [] REAL) [, ] REAL

void genie_matrix_pls1_lim (NODE_T *p)
{
//
// PLS1 with SVD solver for beta.
// NIPALS algorithm with orthonormal scores and loadings.
// See Ulf Indahl, Journal of Chemometrics 28(3) 168-180 [2014].
//
// Note that E is an MxN matrix and f, beta are Nx1 column vectors.
// This for consistency with PLS2.
//
// Decompose eigenvectors until relative size is too short.
//
  gsl_error_handler_t *save_handler = gsl_set_error_handler (torrix_error_handler);
  torrix_error_node = p;
// Pop arguments.
  A68_REF ref_eigenv;
  POP_REF (p, &ref_eigenv);
// 'lim' is minimum relative length to first eigenvector.
  A68_REAL lim;
  POP_OBJECT (p, &lim, A68_REAL); 
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
  int Nk = MIN (Ne, Mf);
// Decompose E and F.
  gsl_matrix *EIGEN = NO_REAL_MATRIX, *nE = NO_REAL_MATRIX, *nF = NO_REAL_MATRIX; 
  gsl_matrix *eig = gsl_matrix_calloc (Ne, 1);
  gsl_matrix *lat = gsl_matrix_calloc (Me, 1);
  gsl_matrix *pl = gsl_matrix_calloc (Ne, 1);
  gsl_matrix *ql = gsl_matrix_calloc (Nf, 1); // 1x1 in PLS1
  gsl_vector *Ev = gsl_vector_calloc (Nk);
// Latent variable deflation.
  int go_on = A68_TRUE;
  for (int k = 0; k < Nk && go_on; k ++) {
// E weight from E, F covariance.
// eig = Eᵀ * f / | Eᵀ * f |
    ASSERT_GSL (gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1.0, E, F, 0.0, eig));
    REAL_T norm = matrix_norm (eig);
    ASSERT_GSL (gsl_matrix_scale (eig, 1.0 / norm));
    gsl_vector_set (Ev, k, norm);
// Compute latent variable.
// lat = E * eig / | E * eig |
    ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 1.0, E, eig, 0.0, lat));
    norm = matrix_norm (lat);
    ASSERT_GSL (gsl_matrix_scale (lat, 1.0 / norm));
// Deflate E and F, remove latent variable from both.
// pl = Eᵀ * lat; E -= lat * plᵀ and ql = Fᵀ * lat; F -= lat * qlᵀ
    ASSERT_GSL (gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1.0, E, lat, 0.0, pl));
    ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasTrans, -1.0, lat, pl, 1.0, E));
    ASSERT_GSL (gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1.0, lat, F, 0.0, ql));
// Build matrices.
    EIGEN = mat_before_ab (p, EIGEN, eig);
    nE = mat_before_ab (p, nE, pl); // P
    nF = mat_over_ab (p, nF, ql);   // Qᵀ
// Another iteration? 
   if (k > 0 && gsl_vector_get (Ev, k) / gsl_vector_get (Ev, 0) < VALUE (&lim)) {
      Nk = k + 1;
      go_on = A68_FALSE;
    }
  }
// Intermediate garbage collector.
  gsl_matrix_free (E);
  gsl_matrix_free (F);
  gsl_matrix_free (eig);
  gsl_matrix_free (lat);
  gsl_matrix_free (pl);
  gsl_matrix_free (ql);
// Projection of original data = Eᵀ * EIGEN
  gsl_matrix *Pr = gsl_matrix_calloc (SIZE2 (nE), SIZE2 (EIGEN));
  ASSERT_GSL (gsl_blas_dgemm (CblasTrans, CblasNoTrans, 1.0, nE, EIGEN, 0.0, Pr));
// 
// Now beta = EIGEN * Pr⁻¹ * nF
// Compute with SVD to avoid matrix inversion.
// Matrix inversion is a source of numerical noise!
// 
  gsl_matrix *u, *v; gsl_vector *s, *w;
  unt M = SIZE1 (Pr), N = SIZE2 (Pr);
// GSL computes thin SVD, M >= N only, returning V, not Vᵀ.
  if (M >= N) {
// X = USVᵀ
    s = gsl_vector_calloc (N);
    u = gsl_matrix_calloc (M, N);
    v = gsl_matrix_calloc (N, N);
    w = gsl_vector_calloc (N);
    gsl_matrix_memcpy (u, Pr);
    ASSERT_GSL (gsl_linalg_SV_decomp (u, v, s, w));
  } else {
// Xᵀ = VSUᵀ
    s = gsl_vector_calloc (M);
    u = gsl_matrix_calloc (N, M);
    v = gsl_matrix_calloc (M, M);
    w = gsl_vector_calloc (M);
    gsl_matrix_transpose_memcpy (u, Pr);
    ASSERT_GSL (gsl_linalg_SV_decomp (u, v, s, w));
  }
// Kill short eigenvectors cf. Python numpy.
  for (int k = 1; k < SIZE (s); k++) {
    if (gsl_vector_get (s, k) / gsl_vector_get (s, 0) < 1e-15) {
      gsl_vector_set (s, k, 0.0);
    }
  }
// Solve for beta.
  gsl_vector *nFv = gsl_vector_calloc (Nk), *x = gsl_vector_calloc (Nk);
  gsl_matrix_get_col (nFv, nF, 0);
  if (M >= N) {
    ASSERT_GSL (gsl_linalg_SV_solve (u, v, s, nFv, x));
  } else {
    ASSERT_GSL (gsl_linalg_SV_solve (v, u, s, nFv, x));
  }
  gsl_matrix *beta = gsl_matrix_calloc (Ne, 1), *xmat = gsl_matrix_calloc (Nk, 1);
  ASSERT_GSL (gsl_matrix_set_col (xmat, 0, x));
  ASSERT_GSL (gsl_blas_dgemm (CblasNoTrans, CblasNoTrans, 1.0, EIGEN, xmat, 0.0, beta));
// Yield results.
  if (!IS_NIL (ref_eigenv)) {
    gsl_vector *Evl = gsl_vector_calloc (Nk);
    for (unt k = 0; k < Nk; k++) {
      gsl_vector_set (Evl, k, gsl_vector_get (Evl, k));
    }
    *DEREF (A68_REF, &ref_eigenv) = vector_to_row (p, Evl);
    gsl_vector_free (Evl);
  }
  push_matrix (p, beta);
// Garbage collector.
  gsl_matrix_free (beta);
  gsl_matrix_free (EIGEN);
  gsl_matrix_free (nE);
  gsl_matrix_free (nF);
  gsl_matrix_free (Pr);
  gsl_matrix_free (u);
  gsl_matrix_free (v);
  gsl_matrix_free (xmat);
  gsl_vector_free (Ev);
  gsl_vector_free (nFv);
  gsl_vector_free (s);
  gsl_vector_free (w);
  gsl_vector_free (x);
//
  (void) gsl_set_error_handler (save_handler);
}

#endif
