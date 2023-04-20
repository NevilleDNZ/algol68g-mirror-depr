//! @file single-fft.c
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
//! REAL, COMPLEX fast fourier transform.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"

#if defined (HAVE_GSL)

//! @brief Map GSL error handler onto a68g error handler.

void fft_error_handler (const char *reason, const char *file, int line, int gsl_errno)
{
  if (line != 0) {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s in line %d of file %s", reason, line, file) >= 0);
  } else {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s", reason) >= 0);
  }
  diagnostic (A68_RUNTIME_ERROR, A68 (f_entry), ERROR_FFT, A68 (edit_line), gsl_strerror (gsl_errno));
  exit_genie (A68 (f_entry), A68_RUNTIME_ERROR);
}

//! @brief Detect math errors.

void fft_test_error (int rc)
{
  if (rc != 0) {
    fft_error_handler ("math error", "", 0, rc);
  }
}

//! @brief Pop [] REAL on the stack as complex REAL_T [].

REAL_T *pop_array_real (NODE_T * p, int *len)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int inc, iindex, k;
  BYTE_T *base;
  REAL_T *v;
  A68 (f_entry) = p;
// Pop arguments.
  POP_REF (p, &desc);
  CHECK_REF (p, desc, M_ROW_REAL);
  GET_DESCRIPTOR (arr, tup, &desc);
  *len = ROW_SIZE (tup);
  if ((*len) <= 0) {
    return NO_REAL;
  }
  v = (REAL_T *) get_heap_space (2 * (size_t) (*len) * sizeof (REAL_T));
  fft_test_error (v == NO_REAL ? GSL_ENOMEM : GSL_SUCCESS);
  base = DEREF (BYTE_T, &ARRAY (arr));
  iindex = VECTOR_OFFSET (arr, tup);
  inc = SPAN (tup) * ELEM_SIZE (arr);
  for (k = 0; k < (*len); k++, iindex += inc) {
    A68_REAL *x = (A68_REAL *) (base + iindex);
    CHECK_INIT (p, INITIALISED (x), M_REAL);
    v[2 * k] = VALUE (x);
    v[2 * k + 1] = 0.0;
  }
  return v;
}

//! @brief Push REAL_T [] on the stack as [] REAL.

void push_array_real (NODE_T * p, REAL_T * v, int len)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int inc, iindex, k;
  BYTE_T *base;
  A68 (f_entry) = p;
  NEW_ROW_1D (desc, row, arr, tup, M_ROW_REAL, M_REAL, len);
  base = DEREF (BYTE_T, &ARRAY (&arr));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = SPAN (&tup) * ELEM_SIZE (&arr);
  for (k = 0; k < len; k++, iindex += inc) {
    A68_REAL *x = (A68_REAL *) (base + iindex);
    STATUS (x) = INIT_MASK;
    VALUE (x) = v[2 * k];
    CHECK_REAL (p, VALUE (x));
  }
  PUSH_REF (p, desc);
}

//! @brief Pop [] COMPLEX on the stack as REAL_T [].

REAL_T *pop_array_complex (NODE_T * p, int *len)
{
  A68_REF desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int inc, iindex, k;
  BYTE_T *base;
  REAL_T *v;
  A68 (f_entry) = p;
// Pop arguments.
  POP_REF (p, &desc);
  CHECK_REF (p, desc, M_ROW_COMPLEX);
  GET_DESCRIPTOR (arr, tup, &desc);
  *len = ROW_SIZE (tup);
  if ((*len) <= 0) {
    return NO_REAL;
  }
  v = (REAL_T *) get_heap_space (2 * (size_t) (*len) * sizeof (REAL_T));
  fft_test_error (v == NO_REAL ? GSL_ENOMEM : GSL_SUCCESS);
  base = DEREF (BYTE_T, &ARRAY (arr));
  iindex = VECTOR_OFFSET (arr, tup);
  inc = SPAN (tup) * ELEM_SIZE (arr);
  for (k = 0; k < (*len); k++, iindex += inc) {
    A68_REAL *re = (A68_REAL *) (base + iindex);
    A68_REAL *im = (A68_REAL *) (base + iindex + SIZE (M_REAL));
    CHECK_INIT (p, INITIALISED (re), M_COMPLEX);
    CHECK_INIT (p, INITIALISED (im), M_COMPLEX);
    v[2 * k] = VALUE (re);
    v[2 * k + 1] = VALUE (im);
  }
  return v;
}

//! @brief Push REAL_T [] on the stack as [] COMPLEX.

void push_array_complex (NODE_T * p, REAL_T * v, int len)
{
  A68_REF desc, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int inc, iindex, k;
  BYTE_T *base;
  A68 (f_entry) = p;
  NEW_ROW_1D (desc, row, arr, tup, M_ROW_COMPLEX, M_COMPLEX, len);
  base = DEREF (BYTE_T, &ARRAY (&arr));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = SPAN (&tup) * ELEM_SIZE (&arr);
  for (k = 0; k < len; k++, iindex += inc) {
    A68_REAL *re = (A68_REAL *) (base + iindex);
    A68_REAL *im = (A68_REAL *) (base + iindex + SIZE (M_REAL));
    STATUS (re) = INIT_MASK;
    VALUE (re) = v[2 * k];
    STATUS (im) = INIT_MASK;
    VALUE (im) = v[2 * k + 1];
    CHECK_COMPLEX (p, VALUE (re), VALUE (im));
  }
  PUSH_REF (p, desc);
}

//! @brief Push prime factorisation on the stack as [] INT.

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
  A68 (f_entry) = p;
  POP_OBJECT (p, &n, A68_INT);
  CHECK_INIT (p, INITIALISED (&n), M_INT);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) (VALUE (&n)));
  len = (int) (NF (wt));
  NEW_ROW_1D (desc, row, arr, tup, M_ROW_INT, M_INT, len);
  base = DEREF (BYTE_T, &ARRAY (&arr));
  iindex = VECTOR_OFFSET (&arr, &tup);
  inc = SPAN (&tup) * ELEM_SIZE (&arr);
  for (k = 0; k < len; k++, iindex += inc) {
    A68_INT *x = (A68_INT *) (base + iindex);
    STATUS (x) = INIT_MASK;
    VALUE (x) = (int) ((FACTOR (wt))[k]);
  }
  gsl_fft_complex_wavetable_free (wt);
  PUSH_REF (p, desc);
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC ([] COMPLEX) [] COMPLEX fft complex forward

void genie_fft_complex_forward (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  REAL_T *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  A68 (f_entry) = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_forward (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_complex (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NO_REAL) {
    a68_free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC ([] COMPLEX) [] COMPLEX fft complex backward

void genie_fft_complex_backward (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  REAL_T *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  A68 (f_entry) = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_backward (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_complex (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NO_REAL) {
    a68_free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC ([] COMPLEX) [] COMPLEX fft complex inverse

void genie_fft_complex_inverse (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  REAL_T *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  A68 (f_entry) = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_inverse (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_complex (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NO_REAL) {
    a68_free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC ([] REAL) [] COMPLEX fft forward

void genie_fft_forward (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  REAL_T *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  A68 (f_entry) = p;
  data = pop_array_real (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_forward (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_complex (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NO_REAL) {
    a68_free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC ([] COMPLEX) [] REAL fft backward

void genie_fft_backward (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  REAL_T *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  A68 (f_entry) = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_backward (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_real (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NO_REAL) {
    a68_free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

//! @brief PROC ([] COMPLEX) [] REAL fft inverse

void genie_fft_inverse (NODE_T * p)
{
  gsl_error_handler_t *save_handler = gsl_set_error_handler (fft_error_handler);
  int len, rc;
  REAL_T *data;
  gsl_fft_complex_wavetable *wt;
  gsl_fft_complex_workspace *ws;
  A68 (f_entry) = p;
  data = pop_array_complex (p, &len);
  fft_test_error (len == 0 ? GSL_EDOM : GSL_SUCCESS);
  wt = gsl_fft_complex_wavetable_alloc ((size_t) len);
  ws = gsl_fft_complex_workspace_alloc ((size_t) len);
  rc = gsl_fft_complex_inverse (data, 1, (size_t) len, wt, ws);
  fft_test_error (rc);
  push_array_real (p, data, len);
  gsl_fft_complex_wavetable_free (wt);
  gsl_fft_complex_workspace_free (ws);
  if (data != NO_REAL) {
    a68_free (data);
  }
  (void) gsl_set_error_handler (save_handler);
}

#endif
