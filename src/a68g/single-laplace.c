//! @file single-laplace.c
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
//! REAL laplace routines.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"

#if defined (HAVE_GSL)

//! @brief Map GSL error handler onto a68g error handler.

void laplace_error_handler (const char *reason, const char *file, int line, int gsl_errno)
{
  if (line != 0) {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s in line %d of file %s", reason, line, file) >= 0);
  } else {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", reason) >= 0);
  }
  diagnostic (A68_RUNTIME_ERROR, A68 (f_entry), ERROR_LAPLACE, A68 (edit_line), gsl_strerror (gsl_errno));
  exit_genie (A68 (f_entry), A68_RUNTIME_ERROR);
}

//! @brief Detect math errors.

void laplace_test_error (int rc)
{
  if (rc != 0) {
    laplace_error_handler ("math error", "", 0, rc);
  }
}

//! @brief PROC (PROC (REAL) REAL, REAL, REF REAL) REAL laplace

#define LAPLACE_DIVISIONS 1024

typedef struct A68_LAPLACE A68_LAPLACE;

struct A68_LAPLACE
{
  NODE_T *p;
  A68_PROCEDURE f;
  REAL_T s;
};

//! @brief Evaluate function for Laplace transform.

REAL_T laplace_f (REAL_T t, void *z)
{
  A68_LAPLACE *l = (A68_LAPLACE *) z;
  ADDR_T pop_sp = A68_SP, pop_fp = A68_FP;
  MOID_T *u = M_PROC_REAL_REAL;
  A68_REAL *ft = (A68_REAL *) STACK_TOP;
  PUSH_VALUE (P (l), t, A68_REAL);
  genie_call_procedure (P (l), MOID (&(F (l))), u, u, &(F (l)), pop_sp, pop_fp);
  A68_SP = pop_sp;
  return VALUE (ft) * a68_exp (-(S (l)) * t);
}

//! @brief Calculate Laplace transform.

void genie_laplace (NODE_T * p)
{
  A68_REF ref_error;
  A68_REAL s, *error;
  A68_PROCEDURE f;
  A68_LAPLACE l;
  gsl_function g;
  gsl_integration_workspace *w;
  REAL_T result, estimated_error;
  int rc;
  gsl_error_handler_t *save_handler = gsl_set_error_handler (laplace_error_handler);
  POP_REF (p, &ref_error);
  CHECK_REF (p, ref_error, M_REF_REAL);
  error = (A68_REAL *) ADDRESS (&ref_error);
  POP_OBJECT (p, &s, A68_REAL);
  POP_PROCEDURE (p, &f);
  P (&l) = p;
  F (&l) = f;
  S (&l) = VALUE (&s);
  FUNCTION (&g) = &laplace_f;
  GSL_PARAMS (&g) = &l;
  w = gsl_integration_workspace_alloc (LAPLACE_DIVISIONS);
  if (VALUE (error) >= 0.0) {
    rc = gsl_integration_qagiu (&g, 0.0, VALUE (error), 0.0, LAPLACE_DIVISIONS, w, &result, &estimated_error);
  } else {
    rc = gsl_integration_qagiu (&g, 0.0, 0.0, -VALUE (error), LAPLACE_DIVISIONS, w, &result, &estimated_error);
  }
  laplace_test_error (rc);
  VALUE (error) = estimated_error;
  PUSH_VALUE (p, result, A68_REAL);
  gsl_integration_workspace_free (w);
  (void) gsl_set_error_handler (save_handler);
}

#endif
