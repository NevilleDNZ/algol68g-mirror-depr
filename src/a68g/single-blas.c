//! @file single-blas.c
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
//! REAL GSL BLAS support.

#include "a68g.h"

#if defined (HAVE_GSL)

#include "a68g-torrix.h"
#include "a68g-prelude-gsl.h"

void a68_vector_free (gsl_vector *V)
{
  if (V != NO_REAL_VECTOR) {
    gsl_vector_free (V);
  }
}

void a68_matrix_free (gsl_matrix *M)
{
  if (M != NO_REAL_MATRIX) {
    gsl_matrix_free (M);
  }
}

void a68_dgemm (CBLAS_TRANSPOSE_t TransA, CBLAS_TRANSPOSE_t TransB,
                double alpha, gsl_matrix *A, gsl_matrix *B,
                double beta, gsl_matrix **C)
{
// Wrapper for gsl_blas_dgemm, allocates result matrix C if needed.
//
// GEMM from BLAS computes C := alpha * TransA (A) * TransB (B) + beta * C
//
  if ((*C) == NO_REAL_MATRIX) {
    unt N = (TransA == I ? SIZE1 (A) : SIZE2 (A));
    unt M = (TransB == I ? SIZE2 (B) : SIZE1 (B));
    (*C) = gsl_matrix_calloc (N, M); // NxM * MxP gives NxP.
  }
  ASSERT_GSL (gsl_blas_dgemm (TransA, TransB, alpha, A, B, beta, (*C)))
}

#endif
