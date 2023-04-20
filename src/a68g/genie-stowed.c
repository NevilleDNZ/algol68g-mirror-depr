//! @file genie-stowed.c
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
//! Interpreter routines for STOWED values.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-frames.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

// Routines for handling stowed objects.
// 
// An A68G row is a reference to a descriptor in the heap:
// 
//                ...
// A68_REF row -> A68_ARRAY ----+   ARRAY: Description of row, ref to elements
//                A68_TUPLE 1   |   TUPLE: Bounds, one for every dimension
//                ...           |
//                A68_TUPLE dim |
//                ...           |
//                ...           |
//                Element 1 <---+   Element: Sequential row elements, in the heap
//                ...                        Not always contiguous - trims!
//                Element n

//! @brief Size of a row.

int get_row_size (A68_TUPLE * tup, int dim)
{
  int span = 1;
  for (int k = 0; k < dim; k++) {
    int stride = ROW_SIZE (&tup[k]);
    ABEND ((stride > 0 && span > A68_MAX_INT / stride), ERROR_INVALID_SIZE, __func__);
    span *= stride;
  }
  return span;
}

//! @brief Initialise index for FORALL constructs.

void initialise_internal_index (A68_TUPLE * tup, int dim)
{
  for (int k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    K (ref) = LWB (ref);
  }
}

//! @brief Calculate index.

ADDR_T calculate_internal_index (A68_TUPLE * tup, int dim)
{
  ADDR_T idx = 0;
  for (int k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
// Only consider non-empty rows.
    if (ROW_SIZE (ref) > 0) {
      idx += (SPAN (ref) * K (ref) - SHIFT (ref));
    }
  }
  return idx;
}

//! @brief Increment index for FORALL constructs.

BOOL_T increment_internal_index (A68_TUPLE * tup, int dim)
{
  BOOL_T carry = A68_TRUE;
  for (int k = dim - 1; k >= 0 && carry; k--) {
    A68_TUPLE *ref = &tup[k];
    if (K (ref) < UPB (ref)) {
      (K (ref))++;
      carry = A68_FALSE;
    } else {
      K (ref) = LWB (ref);
    }
  }
  return carry;
}

//! @brief Print index.

void print_internal_index (FILE_T f, A68_TUPLE * tup, int dim)
{
  for (int k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    BUFFER buf;
    ASSERT (snprintf (buf, SNPRINTF_SIZE, A68_LD, K (ref)) >= 0);
    WRITE (f, buf);
    if (k < dim - 1) {
      WRITE (f, ", ");
    }
  }
}

//! @brief Convert C string to A68 [] CHAR.

A68_REF c_string_to_row_char (NODE_T * p, char *str, int width)
{
  A68_REF z, row; A68_ARRAY arr; A68_TUPLE tup;
  NEW_ROW_1D (z, row, arr, tup, M_ROW_CHAR, M_CHAR, width);
  BYTE_T *base = ADDRESS (&row);
  int len = strlen (str);
  for (int k = 0; k < width; k++) {
    A68_CHAR *ch = (A68_CHAR *) & (base[k * SIZE_ALIGNED (A68_CHAR)]);
    STATUS (ch) = INIT_MASK;
    VALUE (ch) = (k < len ? TO_UCHAR (str[k]) : NULL_CHAR);
  }
  return z;
}

//! @brief Convert C string to A68 string.

A68_REF c_to_a_string (NODE_T * p, char *str, int width)
{
  if (str == NO_TEXT) {
    return empty_string (p);
  } else {
    if (width == DEFAULT_WIDTH) {
      return c_string_to_row_char (p, str, (int) strlen (str));
    } else {
      return c_string_to_row_char (p, str, (int) width);
    }
  }
}

//! @brief Size of a string.

int a68_string_size (NODE_T * p, A68_REF row)
{
  (void) p;
  if (INITIALISED (&row)) {
    A68_ARRAY *arr; A68_TUPLE *tup;
    GET_DESCRIPTOR (arr, tup, &row);
    return ROW_SIZE (tup);
  } else {
    return 0;
  }
}

//! @brief Convert A68 string to C string.

char *a_to_c_string (NODE_T * p, char *str, A68_REF row)
{
// Assume "str" to be long enough - caller's responsibility!.
  (void) p;
  if (INITIALISED (&row)) {
    A68_ARRAY *arr; A68_TUPLE *tup;
    GET_DESCRIPTOR (arr, tup, &row);
    int size = ROW_SIZE (tup), n = 0;
    if (size > 0) {
      BYTE_T *base_address = ADDRESS (&ARRAY (arr));
      for (int k = LWB (tup); k <= UPB (tup); k++) {
        int addr = INDEX_1_DIM (arr, tup, k);
        A68_CHAR *ch = (A68_CHAR *) & (base_address[addr]);
        CHECK_INIT (p, INITIALISED (ch), M_CHAR);
        str[n++] = (char) VALUE (ch);
      }
    }
    str[n] = NULL_CHAR;
    return str;
  } else {
    return NO_TEXT;
  }
}

//! @brief Return an empty row.

A68_REF empty_row (NODE_T * p, MOID_T * u)
{
  if (IS_FLEX (u)) {
    u = SUB (u);
  }
  MOID_T *v = SUB (u);
  int dim = DIM (u);
  A68_REF dsc; A68_ARRAY *arr; A68_TUPLE *tup;
  dsc = heap_generator (p, u, DESCRIPTOR_SIZE (dim));
  GET_DESCRIPTOR (arr, tup, &dsc);
  DIM (arr) = dim;
  MOID (arr) = SLICE (u);
  ELEM_SIZE (arr) = moid_size (SLICE (u));
  SLICE_OFFSET (arr) = 0;
  FIELD_OFFSET (arr) = 0;
  if (IS_ROW (v) || IS_FLEX (v)) {
// [] AMODE or FLEX [] AMODE 
    ARRAY (arr) = heap_generator (p, v, A68_REF_SIZE);
    *DEREF (A68_REF, &ARRAY (arr)) = empty_row (p, v);
  } else {
    ARRAY (arr) = nil_ref;
  }
  STATUS (&ARRAY (arr)) = (STATUS_MASK_T) (INIT_MASK | IN_HEAP_MASK);
  for (int k = 0; k < dim; k++) {
    LWB (&tup[k]) = 1;
    UPB (&tup[k]) = 0;
    SPAN (&tup[k]) = 1;
    SHIFT (&tup[k]) = LWB (tup);
  }
  return dsc;
}

//! @brief An empty string, FLEX [1 : 0] CHAR.

A68_REF empty_string (NODE_T * p)
{
  return empty_row (p, M_STRING);
}

//! @brief Make [,, ..] MODE  from [, ..] MODE.

A68_REF genie_make_rowrow (NODE_T * p, MOID_T * rmod, int len, ADDR_T sp)
{
  MOID_T *nmod = IS_FLEX (rmod) ? SUB (rmod) : rmod;
  MOID_T *emod = SUB (nmod);
  int odim = DIM (nmod) - 1;
// Make the new descriptor.
  A68_REF nrow; A68_ARRAY *new_arr; A68_TUPLE *new_tup;
  nrow = heap_generator (p, rmod, DESCRIPTOR_SIZE (DIM (nmod)));
  GET_DESCRIPTOR (new_arr, new_tup, &nrow);
  DIM (new_arr) = DIM (nmod);
  MOID (new_arr) = emod;
  ELEM_SIZE (new_arr) = SIZE (emod);
  SLICE_OFFSET (new_arr) = 0;
  FIELD_OFFSET (new_arr) = 0;
  if (len == 0) {
// There is a vacuum on the stack.
    for (int k = 0; k < odim; k++) {
      LWB (&new_tup[k + 1]) = 1;
      UPB (&new_tup[k + 1]) = 0;
      SPAN (&new_tup[k + 1]) = 1;
      SHIFT (&new_tup[k + 1]) = LWB (&new_tup[k + 1]);
    }
    LWB (new_tup) = 1;
    UPB (new_tup) = 0;
    SPAN (new_tup) = 0;
    SHIFT (new_tup) = 0;
    ARRAY (new_arr) = nil_ref;
    return nrow;
  } else if (len > 0) {
    A68_ARRAY *x = NO_ARRAY;
// Arrays in the stack must have equal bounds.
    for (int j = 1; j < len; j++) {
      A68_REF rrow = *(A68_REF *) STACK_ADDRESS (sp);
      A68_REF vrow = *(A68_REF *) STACK_ADDRESS (sp + j * A68_REF_SIZE);
      A68_TUPLE *vtup, *rtup;
      GET_DESCRIPTOR (x, rtup, &rrow);
      GET_DESCRIPTOR (x, vtup, &vrow);
      for (int k = 0; k < odim; k++, rtup++, vtup++) {
        if ((UPB (rtup) != UPB (vtup)) || (LWB (rtup) != LWB (vtup))) {
          diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
    }
// Fill descriptor of new row with info from (arbitrary) first one.
    A68_REF orow; A68_ARRAY *old_arr; A68_TUPLE *old_tup;
    orow = *(A68_REF *) STACK_ADDRESS (sp);
    GET_DESCRIPTOR (x, old_tup, &orow);
    int span = 1;
    for (int k = 0; k < odim; k++) {
      A68_TUPLE *nt = &new_tup[k + 1], *ot = &old_tup[k];
      LWB (nt) = LWB (ot);
      UPB (nt) = UPB (ot);
      SPAN (nt) = span;
      SHIFT (nt) = LWB (nt) * SPAN (nt);
      span *= ROW_SIZE (nt);
    }
    LWB (new_tup) = 1;
    UPB (new_tup) = len;
    SPAN (new_tup) = span;
    SHIFT (new_tup) = LWB (new_tup) * SPAN (new_tup);
    ARRAY (new_arr) = heap_generator (p, rmod, len * span * ELEM_SIZE (new_arr));
    for (int j = 0; j < len; j++) {
// new[j,, ] := old[, ].
      GET_DESCRIPTOR (old_arr, old_tup, (A68_REF *) STACK_ADDRESS (sp + j * A68_REF_SIZE));
      if (LWB (old_tup) > UPB (old_tup)) {
        A68_REF dst = ARRAY (new_arr);
        ADDR_T new_k = j * SPAN (new_tup) + calculate_internal_index (&new_tup[1], odim);
        OFFSET (&dst) += ROW_ELEMENT (new_arr, new_k);
        A68_REF none = empty_row (p, SLICE (rmod));
        MOVE (ADDRESS (&dst), ADDRESS (&none), SIZE (emod));
      } else {
        initialise_internal_index (old_tup, odim);
        initialise_internal_index (&new_tup[1], odim);
        BOOL_T done = A68_FALSE;
        while (!done) {
          A68_REF src = ARRAY (old_arr), dst = ARRAY (new_arr);
          ADDR_T old_k = calculate_internal_index (old_tup, odim);
          ADDR_T new_k = j * SPAN (new_tup) + calculate_internal_index (&new_tup[1], odim);
          OFFSET (&src) += ROW_ELEMENT (old_arr, old_k);
          OFFSET (&dst) += ROW_ELEMENT (new_arr, new_k);
          if (HAS_ROWS (emod)) {
            A68_REF none = genie_clone (p, emod, (A68_REF *) & nil_ref, &src);
            MOVE (ADDRESS (&dst), ADDRESS (&none), SIZE (emod));
          } else {
            MOVE (ADDRESS (&dst), ADDRESS (&src), SIZE (emod));
          }
          done = increment_internal_index (old_tup, odim) | increment_internal_index (&new_tup[1], odim);
        }
      }
    }
  }
  return nrow;
}

//! @brief Make a row of 'len' objects that are in the stack.

A68_REF genie_make_row (NODE_T * p, MOID_T * elem_mode, int len, ADDR_T sp)
{
  A68_REF new_row, new_arr; A68_ARRAY arr; A68_TUPLE tup;
  NEW_ROW_1D (new_row, new_arr, arr, tup, MOID (p), elem_mode, len);
  for (int k = 0; k < len * ELEM_SIZE (&arr); k += ELEM_SIZE (&arr)) {
    A68_REF dst = new_arr, src;
    OFFSET (&dst) += k;
    STATUS (&src) = (STATUS_MASK_T) (INIT_MASK | IN_STACK_MASK);
    OFFSET (&src) = sp + k;
    REF_HANDLE (&src) = (A68_HANDLE *) & nil_handle;
    if (HAS_ROWS (elem_mode)) {
      A68_REF new_one = genie_clone (p, elem_mode, (A68_REF *) & nil_ref, &src);
      MOVE (ADDRESS (&dst), ADDRESS (&new_one), SIZE (elem_mode));
    } else {
      MOVE (ADDRESS (&dst), ADDRESS (&src), SIZE (elem_mode));
    }
  }
  return new_row;
}

//! @brief Make REF [1 : 1] [] MODE from REF [] MODE.

A68_REF genie_make_ref_row_of_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  A68_REF array = *(A68_REF *) STACK_ADDRESS (sp);
// ROWING NIL yields NIL.
  if (IS_NIL (array)) {
    return nil_ref;
  } else {
    A68_REF new_row = heap_generator (p, SUB (dst_mode), DESCRIPTOR_SIZE (1));
    A68_REF name = heap_generator (p, dst_mode, A68_REF_SIZE);
    A68_ARRAY *arr; A68_TUPLE *tup;
    GET_DESCRIPTOR (arr, tup, &new_row);
    DIM (arr) = 1;
    MOID (arr) = src_mode;
    ELEM_SIZE (arr) = SIZE (src_mode);
    SLICE_OFFSET (arr) = 0;
    FIELD_OFFSET (arr) = 0;
    ARRAY (arr) = array;
    LWB (tup) = 1;
    UPB (tup) = 1;
    SPAN (tup) = 1;
    SHIFT (tup) = LWB (tup);
    *DEREF (A68_REF, &name) = new_row;
    return name;
  }
}

//! @brief Make REF [1 : 1, ..] MODE from REF [..] MODE.

A68_REF genie_make_ref_row_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  A68_REF name = *(A68_REF *) STACK_ADDRESS (sp);
// ROWING NIL yields NIL.
  if (IS_NIL (name)) {
    return nil_ref;
  }
  A68_REF old_row = *DEREF (A68_REF, &name); A68_TUPLE *new_tup, *old_tup;
  A68_ARRAY *old_arr;
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
// Make new descriptor.
  A68_REF new_row = heap_generator (p, dst_mode, DESCRIPTOR_SIZE (DIM (SUB (dst_mode))));
  A68_ARRAY *new_arr;
  name = heap_generator (p, dst_mode, A68_REF_SIZE);
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (SUB (dst_mode));
  MOID (new_arr) = MOID (old_arr);
  ELEM_SIZE (new_arr) = ELEM_SIZE (old_arr);
  SLICE_OFFSET (new_arr) = 0;
  FIELD_OFFSET (new_arr) = 0;
  ARRAY (new_arr) = ARRAY (old_arr);
// Fill out the descriptor.
  LWB (&(new_tup[0])) = 1;
  UPB (&(new_tup[0])) = 1;
  SPAN (&(new_tup[0])) = 1;
  SHIFT (&(new_tup[0])) = LWB (&(new_tup[0]));
  for (int k = 0; k < DIM (SUB (src_mode)); k++) {
    new_tup[k + 1] = old_tup[k];
  }
// Yield the new name.
  *DEREF (A68_REF, &name) = new_row;
  return name;
}

//! @brief Coercion to [1 : 1, ] MODE.

PROP_T genie_rowing_row_row (NODE_T * p)
{
  ADDR_T sp = A68_SP;
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), A68_FP);
  A68_REF row = genie_make_rowrow (p, MOID (p), 1, sp);
  A68_SP = sp;
  PUSH_REF (p, row);
  return GPROP (p);
}

//! @brief Coercion to [1 : 1] [] MODE.

PROP_T genie_rowing_row_of_row (NODE_T * p)
{
  ADDR_T sp = A68_SP;
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), A68_FP);
  A68_REF row = genie_make_row (p, SLICE (MOID (p)), 1, sp);
  A68_SP = sp;
  PUSH_REF (p, row);
  return GPROP (p);
}

//! @brief Coercion to REF [1 : 1, ..] MODE.

PROP_T genie_rowing_ref_row_row (NODE_T * p)
{
  ADDR_T sp = A68_SP;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), A68_FP);
  A68_SP = sp;
  A68_REF name = genie_make_ref_row_row (p, dst, src, sp);
  PUSH_REF (p, name);
  return GPROP (p);
}

//! @brief REF [1 : 1] [] MODE from [] MODE

PROP_T genie_rowing_ref_row_of_row (NODE_T * p)
{
  ADDR_T sp = A68_SP;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  STACK_DNS (p, MOID (SUB (p)), A68_FP);
  A68_SP = sp;
  A68_REF name = genie_make_ref_row_of_row (p, dst, src, sp);
  PUSH_REF (p, name);
  return GPROP (p);
}

//! @brief Rowing coercion.

PROP_T genie_rowing (NODE_T * p)
{
  PROP_T self;
  if (IS_REF (MOID (p))) {
// REF ROW, decide whether we want A->[] A or [] A->[,] A.
    MOID_T *mode = SUB_MOID (p);
    if (DIM (DEFLEX (mode)) >= 2) {
      (void) genie_rowing_ref_row_row (p);
      UNIT (&self) = genie_rowing_ref_row_row;
      SOURCE (&self) = p;
    } else {
      (void) genie_rowing_ref_row_of_row (p);
      UNIT (&self) = genie_rowing_ref_row_of_row;
      SOURCE (&self) = p;
    }
  } else {
// ROW, decide whether we want A->[] A or [] A->[,] A.
    if (DIM (DEFLEX (MOID (p))) >= 2) {
      (void) genie_rowing_row_row (p);
      UNIT (&self) = genie_rowing_row_row;
      SOURCE (&self) = p;
    } else {
      (void) genie_rowing_row_of_row (p);
      UNIT (&self) = genie_rowing_row_of_row;
      SOURCE (&self) = p;
    }
  }
  return self;
}

//! @brief Clone a compounded value referred to by 'old'.

A68_REF genie_clone (NODE_T * p, MOID_T * m, A68_REF * tmp, A68_REF * old)
{
// This complex routine is needed as arrays are not always contiguous.
// The routine takes a REF to the value and returns a REF to the clone.
  if (m == M_SOUND) {
// REF SOUND.
    A68_REF nsound = heap_generator (p, m, SIZE (m));
    A68_SOUND *w = DEREF (A68_SOUND, &nsound);
    int size = A68_SOUND_DATA_SIZE (w);
    COPY ((BYTE_T *) w, ADDRESS (old), SIZE (M_SOUND));
    BYTE_T *owd = ADDRESS (&(DATA (w)));
    DATA (w) = heap_generator (p, M_SOUND_DATA, size);
    COPY (ADDRESS (&(DATA (w))), owd, size);
    return nsound;
  } else if (IS_STRUCT (m)) {
// REF STRUCT.
    A68_REF nstruct = heap_generator (p, m, SIZE (m));
    for (PACK_T *fds = PACK (m); fds != NO_PACK; FORWARD (fds)) {
      MOID_T *fm = MOID (fds);
      A68_REF of = *old, nf = nstruct, tf = *tmp;
      OFFSET (&of) += OFFSET (fds);
      OFFSET (&nf) += OFFSET (fds);
      if (!IS_NIL (tf)) {
        OFFSET (&tf) += OFFSET (fds);
      }
      if (HAS_ROWS (fm)) {
        A68_REF a68_clone = genie_clone (p, fm, &tf, &of);
        MOVE (ADDRESS (&nf), ADDRESS (&a68_clone), SIZE (fm));
      } else {
        MOVE (ADDRESS (&nf), ADDRESS (&of), SIZE (fm));
      }
    }
    return nstruct;
  } else if (IS_UNION (m)) {
// REF UNION.
    A68_REF nunion = heap_generator (p, m, SIZE (m));
    A68_REF src = *old;
    A68_UNION *u = DEREF (A68_UNION, &src);
    MOID_T *um = (MOID_T *) VALUE (u);
    OFFSET (&src) += UNION_OFFSET;
    A68_REF dst = nunion;
    *DEREF (A68_UNION, &dst) = *u;
    OFFSET (&dst) += UNION_OFFSET;
// A union has formal members, so tmp is irrelevant.
    A68_REF tmpu = nil_ref;
    if (um != NO_MOID && HAS_ROWS (um)) {
      A68_REF a68_clone = genie_clone (p, um, &tmpu, &src);
      MOVE (ADDRESS (&dst), ADDRESS (&a68_clone), SIZE (um));
    } else if (um != NO_MOID) {
      MOVE (ADDRESS (&dst), ADDRESS (&src), SIZE (um));
    }
    return nunion;
  } else if (IF_ROW (m)) {
// REF [FLEX] [].
    MOID_T *em = SUB (IS_FLEX (m) ? SUB (m) : m);
// Make new array.
    A68_ARRAY *old_arr; A68_TUPLE *old_tup;
    GET_DESCRIPTOR (old_arr, old_tup, DEREF (A68_REF, old));
    A68_ARRAY *new_arr; A68_TUPLE *new_tup;
    A68_REF nrow = heap_generator (p, m, DESCRIPTOR_SIZE (DIM (old_arr)));
    GET_DESCRIPTOR (new_arr, new_tup, &nrow);
    DIM (new_arr) = DIM (old_arr);
    MOID (new_arr) = MOID (old_arr);
    ELEM_SIZE (new_arr) = ELEM_SIZE (old_arr);
    SLICE_OFFSET (new_arr) = 0;
    FIELD_OFFSET (new_arr) = 0;
// Get size and copy bounds; check in case of a row.
// This is just song and dance to comply with the RR.
    BOOL_T check_bounds = A68_FALSE;
    A68_REF ntmp; A68_ARRAY *tarr; A68_TUPLE *ttup = NO_TUPLE;
    if (IS_NIL (*tmp)) {
      ntmp = nil_ref;
    } else {
      A68_REF *z = DEREF (A68_REF, tmp);
      if (!IS_NIL (*z)) {
        GET_DESCRIPTOR (tarr, ttup, z);
        ntmp = ARRAY (tarr);
        check_bounds = IS_ROW (m);
      }
    }
    int span = 1;
    for (int k = 0; k < DIM (old_arr); k++) {
      A68_TUPLE *op = &old_tup[k], *np = &new_tup[k];
      if (check_bounds) {
        A68_TUPLE *tp = &ttup[k];
        if (UPB (tp) >= LWB (tp) && UPB (op) >= LWB (op)) {
          if (UPB (tp) != UPB (op) || LWB (tp) != LWB (op)) {
            diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
          }
        }
      }
      LWB (np) = LWB (op);
      UPB (np) = UPB (op);
      SPAN (np) = span;
      SHIFT (np) = LWB (np) * SPAN (np);
      span *= ROW_SIZE (np);
    }
// Make a new array with at least a ghost element.
    if (span == 0) {
      ARRAY (new_arr) = heap_generator (p, em, ELEM_SIZE (new_arr));
    } else {
      ARRAY (new_arr) = heap_generator (p, em, span * ELEM_SIZE (new_arr));
    }
// Copy the ghost element if there are no elements.
    if (span == 0) {
      if (IS_UNION (em)) {
// UNION has formal members.
      } else if (HAS_ROWS (em)) {
        A68_REF old_ref, dst_ref, a68_clone;
        old_ref = ARRAY (old_arr);
        OFFSET (&old_ref) += ROW_ELEMENT (old_arr, 0);
        dst_ref = ARRAY (new_arr);
        OFFSET (&dst_ref) += ROW_ELEMENT (new_arr, 0);
        a68_clone = genie_clone (p, em, &ntmp, &old_ref);
        MOVE (ADDRESS (&dst_ref), ADDRESS (&a68_clone), SIZE (em));
      }
    } else if (span > 0) {
// The n-dimensional copier.
      initialise_internal_index (old_tup, DIM (old_arr));
      initialise_internal_index (new_tup, DIM (new_arr));
      BOOL_T done = A68_FALSE;
      while (!done) {
        A68_REF old_ref = ARRAY (old_arr), dst_ref = ARRAY (new_arr);
        ADDR_T old_k = calculate_internal_index (old_tup, DIM (old_arr));
        ADDR_T new_k = calculate_internal_index (new_tup, DIM (new_arr));
        OFFSET (&old_ref) += ROW_ELEMENT (old_arr, old_k);
        OFFSET (&dst_ref) += ROW_ELEMENT (new_arr, new_k);
        if (HAS_ROWS (em)) {
          A68_REF a68_clone;
          a68_clone = genie_clone (p, em, &ntmp, &old_ref);
          MOVE (ADDRESS (&dst_ref), ADDRESS (&a68_clone), SIZE (em));
        } else {
          MOVE (ADDRESS (&dst_ref), ADDRESS (&old_ref), SIZE (em));
        }
// Increase pointers.
        done = increment_internal_index (old_tup, DIM (old_arr)) | increment_internal_index (new_tup, DIM (new_arr));
      }
    }
    A68_REF heap = heap_generator (p, m, A68_REF_SIZE);
    *DEREF (A68_REF, &heap) = nrow;
    return heap;
  }
  return nil_ref;
}

//! @brief Store into a row, fi. trimmed destinations.

A68_REF genie_store (NODE_T * p, MOID_T * m, A68_REF * dst, A68_REF * old)
{
// This complex routine is needed as arrays are not always contiguous.
// The routine takes a REF to the value and returns a REF to the clone.
  if (IF_ROW (m)) {
// REF [FLEX] [].
    A68_TUPLE *old_tup, *new_tup, *old_p, *new_p;
    MOID_T *em = SUB (IS_FLEX (m) ? SUB (m) : m);
    BOOL_T done = A68_FALSE;
    A68_ARRAY *old_arr, *new_arr;
    GET_DESCRIPTOR (old_arr, old_tup, DEREF (A68_REF, old));
    GET_DESCRIPTOR (new_arr, new_tup, DEREF (A68_REF, dst));
// Get size and check bounds.
// This is just song and dance to comply with the RR.
    int span = 1;
    for (int k = 0; k < DIM (old_arr); k++) {
      old_p = &old_tup[k];
      new_p = &new_tup[k];
      if ((UPB (new_p) >= LWB (new_p) && UPB (old_p) >= LWB (old_p))) {
        if ((UPB (new_p) != UPB (old_p) || LWB (new_p) != LWB (old_p))) {
          diagnostic (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
      span *= ROW_SIZE (new_p);
    }
// Destination is an empty row, inspect if the source has elements.
    if (span == 0) {
      span = 1;
      for (int k = 0; k < DIM (old_arr); k++) {
        span *= ROW_SIZE (old_p);
      }
      if (span > 0) {
        span = 1;
        for (int k = 0; k < DIM (old_arr); k++) {
          new_tup[k] = old_tup[k];
        }
        ARRAY (new_arr) = heap_generator (p, em, span * ELEM_SIZE (new_arr));
      }
    } 
    if (span > 0) {
      initialise_internal_index (old_tup, DIM (old_arr));
      initialise_internal_index (new_tup, DIM (new_arr));
      while (!done) {
        A68_REF new_old = ARRAY (old_arr), new_dst = ARRAY (new_arr);
        ADDR_T old_index = calculate_internal_index (old_tup, DIM (old_arr));
        ADDR_T new_index = calculate_internal_index (new_tup, DIM (new_arr));
        OFFSET (&new_old) += ROW_ELEMENT (old_arr, old_index);
        OFFSET (&new_dst) += ROW_ELEMENT (new_arr, new_index);
        MOVE (ADDRESS (&new_dst), ADDRESS (&new_old), SIZE (em));
        done = increment_internal_index (old_tup, DIM (old_arr)) | increment_internal_index (new_tup, DIM (new_arr));
      }
    }
    return *dst;
  }
  return nil_ref;
}

//! @brief Assignment of complex objects in the stack.

void genie_clone_stack (NODE_T * p, MOID_T * srcm, A68_REF * dst, A68_REF * tmp)
{
// STRUCT, UNION, [FLEX] [] or SOUND.
  A68_REF stack;
  STATUS (&stack) = (STATUS_MASK_T) (INIT_MASK | IN_STACK_MASK);
  OFFSET (&stack) = A68_SP;
  REF_HANDLE (&stack) = (A68_HANDLE *) & nil_handle;
  A68_REF *src = DEREF (A68_REF, &stack);
  if (IS_ROW (srcm) && !IS_NIL (*tmp)) {
    if (STATUS (src) & SKIP_ROW_MASK) {
      return;
    }
    A68_REF a68_clone = genie_clone (p, srcm, tmp, &stack);
    (void) genie_store (p, srcm, dst, &a68_clone);
  } else {
    A68_REF a68_clone = genie_clone (p, srcm, tmp, &stack);
    MOVE (ADDRESS (dst), ADDRESS (&a68_clone), SIZE (srcm));
  }
}

//! @brief Strcmp for qsort.

int qstrcmp (const void *a, const void *b)
{
  return strcmp (*(char *const *) a, *(char *const *) b);
}

//! @brief Sort row of string.

void genie_sort_row_string (NODE_T * p)
{
  A68_REF z; A68_ARRAY *arr; A68_TUPLE *tup;
  POP_REF (p, &z);
  ADDR_T pop_sp = A68_SP;
  CHECK_REF (p, z, M_ROW_STRING);
  GET_DESCRIPTOR (arr, tup, &z);
  int size = ROW_SIZE (tup);
  if (size > 0) {
    BYTE_T *base = ADDRESS (&ARRAY (arr));
    char **ptrs = (char **) a68_alloc ((size_t) (size * (int) sizeof (char *)), __func__, __LINE__);
    if (ptrs == NO_VAR) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
// Copy C-strings into the stack and sort.
    for (int j = 0, k = LWB (tup); k <= UPB (tup); j++, k++) {
      int addr = INDEX_1_DIM (arr, tup, k);
      A68_REF ref = *(A68_REF *) & (base[addr]);
      CHECK_REF (p, ref, M_STRING);
      int len = A68_ALIGN (a68_string_size (p, ref) + 1);
      if (A68_SP + len > A68 (expr_stack_limit)) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      ptrs[j] = (char *) STACK_TOP;
      ASSERT (a_to_c_string (p, (char *) STACK_TOP, ref) != NO_TEXT);
      INCREMENT_STACK_POINTER (p, len);
    }
    qsort (ptrs, (size_t) size, sizeof (char *), qstrcmp);
// Construct an array of sorted strings.
    A68_REF row; A68_ARRAY arrn; A68_TUPLE tupn;
    NEW_ROW_1D (z, row, arrn, tupn, M_ROW_STRING, M_STRING, size);
    A68_REF *base_ref = DEREF (A68_REF, &row);
    for (int k = 0; k < size; k++) {
      base_ref[k] = c_to_a_string (p, ptrs[k], DEFAULT_WIDTH);
    }
    a68_free (ptrs);
    A68_SP = pop_sp;
    PUSH_REF (p, z);
  } else {
// This is how we sort an empty row of strings ...
    A68_SP = pop_sp;
    PUSH_REF (p, empty_row (p, M_ROW_STRING));
  }
}

//! @brief Construct a descriptor "ref_new" for a trim of "ref_old".

void genie_trimmer (NODE_T * p, BYTE_T * *ref_new, BYTE_T * *ref_old, INT_T * offset)
{
  if (p != NO_NODE) {
    if (IS (p, UNIT)) {
      EXECUTE_UNIT (p);
      A68_INT k;
      POP_OBJECT (p, &k, A68_INT);
      A68_TUPLE *t = (A68_TUPLE *) * ref_old;
      CHECK_INDEX (p, &k, t);
      (*offset) += SPAN (t) * VALUE (&k) - SHIFT (t);
      (*ref_old) += sizeof (A68_TUPLE);
    } else if (IS (p, TRIMMER)) {
      A68_TUPLE *old_tup = (A68_TUPLE *) * ref_old;
      A68_TUPLE *new_tup = (A68_TUPLE *) * ref_new;
// TRIMMER is (l:u@r) with all units optional or (empty).
      INT_T L, U, D;
      NODE_T *q = SUB (p);
      if (q == NO_NODE) {
        L = LWB (old_tup);
        U = UPB (old_tup);
        D = 0;
      } else {
        BOOL_T absent = A68_TRUE;
// Lower index.
        if (q != NO_NODE && IS (q, UNIT)) {
          EXECUTE_UNIT (q);
          A68_INT k;
          POP_OBJECT (p, &k, A68_INT);
          if (VALUE (&k) < LWB (old_tup)) {
            diagnostic (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
          }
          L = VALUE (&k);
          FORWARD (q);
          absent = A68_FALSE;
        } else {
          L = LWB (old_tup);
        }
        if (q != NO_NODE && (IS (q, COLON_SYMBOL)
                             || IS (q, DOTDOT_SYMBOL))) {
          FORWARD (q);
          absent = A68_FALSE;
        }
// Upper index.
        if (q != NO_NODE && IS (q, UNIT)) {
          EXECUTE_UNIT (q);
          A68_INT k;
          POP_OBJECT (p, &k, A68_INT);
          if (VALUE (&k) > UPB (old_tup)) {
            diagnostic (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
          }
          U = VALUE (&k);
          FORWARD (q);
          absent = A68_FALSE;
        } else {
          U = UPB (old_tup);
        }
        if (q != NO_NODE && IS (q, AT_SYMBOL)) {
          FORWARD (q);
        }
// Revised lower bound.
        if (q != NO_NODE && IS (q, UNIT)) {
          EXECUTE_UNIT (q);
          A68_INT k;
          POP_OBJECT (p, &k, A68_INT);
          D = L - VALUE (&k);
          FORWARD (q);
        } else {
          D = (absent ? 0 : L - 1);
        }
      }
      LWB (new_tup) = L - D;
      UPB (new_tup) = U - D;    // (L - D) + (U - L)
      SPAN (new_tup) = SPAN (old_tup);
      SHIFT (new_tup) = SHIFT (old_tup) - D * SPAN (new_tup);
      (*ref_old) += sizeof (A68_TUPLE);
      (*ref_new) += sizeof (A68_TUPLE);
    } else {
      genie_trimmer (SUB (p), ref_new, ref_old, offset);
      genie_trimmer (NEXT (p), ref_new, ref_old, offset);
    }
  }
}

//! @brief Calculation of subscript.

void genie_subscript (NODE_T * p, A68_TUPLE ** tup, INT_T * sum, NODE_T ** seq)
{
  for (; p != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        EXECUTE_UNIT (p);
        A68_INT *k;
        POP_ADDRESS (p, k, A68_INT);
        A68_TUPLE *t = *tup;
        CHECK_INDEX (p, k, t);
        (*tup)++;
        (*sum) += (SPAN (t) * VALUE (k) - SHIFT (t));
        SEQUENCE (*seq) = p;
        (*seq) = p;
        return;
      }
    case GENERIC_ARGUMENT:
    case GENERIC_ARGUMENT_LIST:
      {
        genie_subscript (SUB (p), tup, sum, seq);
      }
    }
  }
}

//! @brief Slice REF [] A to REF A.

PROP_T genie_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *pr = SUB (p);
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_ARRAY *a; A68_TUPLE *t;
// Get row and save row from garbage collector.
  EXECUTE_UNIT (pr);
  CHECK_REF (p, *z, MOID (SUB (p)));
  GET_DESCRIPTOR (a, t, DEREF (A68_ROW, z));
  ADDR_T pop_sp = A68_SP;
  INT_T sindex = 0;
  for (q = SEQUENCE (p); q != NO_NODE; t++, q = SEQUENCE (q)) {
    A68_INT *j = (A68_INT *) STACK_TOP;
    INT_T k;
    EXECUTE_UNIT (q);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    sindex += (SPAN (t) * k - SHIFT (t));
    A68_SP = pop_sp;
  }
// Leave reference to element on the stack, preserving scope.
  ADDR_T scope = REF_SCOPE (z);
  *z = ARRAY (a);
  OFFSET (z) += ROW_ELEMENT (a, sindex);
  REF_SCOPE (z) = scope;
  return GPROP (p);
}

//! @brief Push slice of a rowed object.

PROP_T genie_slice (NODE_T * p)
{
  ADDR_T pop_sp, scope = PRIMAL_SCOPE;
  BOOL_T slice_of_name = (BOOL_T) (IS_REF (MOID (SUB (p))));
  MOID_T *result_mode = slice_of_name ? SUB_MOID (p) : MOID (p);
  NODE_T *indexer = NEXT_SUB (p);
  PROP_T self;
  UNIT (&self) = genie_slice;
  SOURCE (&self) = p;
  pop_sp = A68_SP;
// Get row.
  PROP_T primary;
  EXECUTE_UNIT_2 (SUB (p), primary);
// In case of slicing a REF [], we need the [] internally, so dereference.
  if (slice_of_name) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = REF_SCOPE (&z);
    PUSH_REF (p, *DEREF (A68_REF, &z));
  }
  if (ANNOTATION (indexer) == SLICE) {
// SLICING subscripts one element from an array.
    A68_REF z; A68_ARRAY *a; A68_TUPLE *t;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    GET_DESCRIPTOR (a, t, &z);
    INT_T sindex;
    if (SEQUENCE (p) == NO_NODE && !STATUS_TEST (p, SEQUENCE_MASK)) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      GINFO_T g;
      GINFO (&top_seq) = &g;
      sindex = 0;
      genie_subscript (indexer, &t, &sindex, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      STATUS_SET (p, SEQUENCE_MASK);
    } else {
      NODE_T *q;
      for (sindex = 0, q = SEQUENCE (p); q != NO_NODE; t++, q = SEQUENCE (q)) {
        A68_INT *j = (A68_INT *) STACK_TOP;
        INT_T k;
        EXECUTE_UNIT (q);
        k = VALUE (j);
        if (k < LWB (t) || k > UPB (t)) {
          diagnostic (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
          exit_genie (q, A68_RUNTIME_ERROR);
        }
        sindex += (SPAN (t) * k - SHIFT (t));
      }
    }
// Slice of a name yields a name.
    A68_SP = pop_sp;
    if (slice_of_name) {
      A68_REF name = ARRAY (a);
      OFFSET (&name) += ROW_ELEMENT (a, sindex);
      REF_SCOPE (&name) = scope;
      PUSH_REF (p, name);
      if (STATUS_TEST (p, SEQUENCE_MASK)) {
        UNIT (&self) = genie_slice_name_quick;
        SOURCE (&self) = p;
      }
    } else {
      BYTE_T *stack_top = STACK_TOP;
      PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, sindex)]), SIZE (result_mode));
      genie_check_initialisation (p, stack_top, result_mode);
    }
    return self;
  } else if (ANNOTATION (indexer) == TRIMMER) {
// Trimming selects a subarray from an array.
    int dim = DIM (DEFLEX (result_mode));
    A68_REF ref_desc_copy = heap_generator (p, MOID (p), DESCRIPTOR_SIZE (dim));
// Get descriptor.
    A68_REF z;
    POP_REF (p, &z);
// Get indexer.
    CHECK_REF (p, z, MOID (SUB (p)));
    A68_ARRAY *old_des = DEREF (A68_ARRAY, &z), *new_des = DEREF (A68_ARRAY, &ref_desc_copy);
    BYTE_T *ref_old = ADDRESS (&z) + SIZE_ALIGNED (A68_ARRAY);
    BYTE_T *ref_new = ADDRESS (&ref_desc_copy) + SIZE_ALIGNED (A68_ARRAY);
    DIM (new_des) = dim;
    MOID (new_des) = MOID (old_des);
    ELEM_SIZE (new_des) = ELEM_SIZE (old_des);
    INT_T offset = SLICE_OFFSET (old_des);
    genie_trimmer (indexer, &ref_new, &ref_old, &offset);
    SLICE_OFFSET (new_des) = offset;
    FIELD_OFFSET (new_des) = FIELD_OFFSET (old_des);
    ARRAY (new_des) = ARRAY (old_des);
// Trim of a name is a name.
    if (slice_of_name) {
      A68_REF ref_new2 = heap_generator (p, MOID (p), A68_REF_SIZE);
      *DEREF (A68_REF, &ref_new2) = ref_desc_copy;
      REF_SCOPE (&ref_new2) = scope;
      PUSH_REF (p, ref_new2);
    } else {
      PUSH_REF (p, ref_desc_copy);
    }
    return self;
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    return self;
  }
  (void) primary;
}

//! @brief SELECTION from a value

PROP_T genie_selection_value_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *result_mode = MOID (selector);
  ADDR_T pop_sp = A68_SP;
  int size = SIZE (result_mode);
  INT_T offset = OFFSET (NODE_PACK (SUB (selector)));
  EXECUTE_UNIT (NEXT (selector));
  A68_SP = pop_sp;
  if (offset > 0) {
    MOVE (STACK_TOP, STACK_OFFSET (offset), (unt) size);
    genie_check_initialisation (p, STACK_TOP, result_mode);
  }
  INCREMENT_STACK_POINTER (selector, size);
  return GPROP (p);
}

//! @brief SELECTION from a name

PROP_T genie_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  return GPROP (p);
}

//! @brief Push selection from secondary.

PROP_T genie_selection (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector)), *result_mode = MOID (selector);
  BOOL_T selection_of_name = (BOOL_T) (IS_REF (struct_mode));
  PROP_T self;
  SOURCE (&self) = p;
  UNIT (&self) = genie_selection;
  EXECUTE_UNIT (NEXT (selector));
// Multiple selections.
  if (selection_of_name && (IS_FLEX (SUB (struct_mode)) || IS_ROW (SUB (struct_mode)))) {
    A68_REF *row1;
    POP_ADDRESS (selector, row1, A68_REF);
    CHECK_REF (p, *row1, struct_mode);
    row1 = DEREF (A68_REF, row1);
    int dims = DIM (DEFLEX (SUB (struct_mode)));
    int desc_size = DESCRIPTOR_SIZE (dims);
    A68_REF row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), DEREF (BYTE_T, row1), (unt) desc_size);
    MOID ((DEREF (A68_ARRAY, &row2))) = SUB_SUB (result_mode);
    FIELD_OFFSET (DEREF (A68_ARRAY, &row2)) += OFFSET (NODE_PACK (SUB (selector)));
    A68_REF row3 = heap_generator (selector, result_mode, A68_REF_SIZE);
    *DEREF (A68_REF, &row3) = row2;
    PUSH_REF (selector, row3);
    UNIT (&self) = genie_selection;
  } else if (struct_mode != NO_MOID && (IS_FLEX (struct_mode) || IS_ROW (struct_mode))) {
    A68_REF *row1;
    POP_ADDRESS (selector, row1, A68_REF);
    int dims = DIM (DEFLEX (struct_mode));
    int desc_size = DESCRIPTOR_SIZE (dims);
    A68_REF row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), DEREF (BYTE_T, row1), (unt) desc_size);
    MOID ((DEREF (A68_ARRAY, &row2))) = SUB (result_mode);
    FIELD_OFFSET (DEREF (A68_ARRAY, &row2)) += OFFSET (NODE_PACK (SUB (selector)));
    PUSH_REF (selector, row2);
    UNIT (&self) = genie_selection;
  }
// Normal selections.
  else if (selection_of_name && IS_STRUCT (SUB (struct_mode))) {
    A68_REF *z = (A68_REF *) (STACK_OFFSET (-A68_REF_SIZE));
    CHECK_REF (selector, *z, struct_mode);
    OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
    UNIT (&self) = genie_selection_name_quick;
  } else if (IS_STRUCT (struct_mode)) {
    DECREMENT_STACK_POINTER (selector, SIZE (struct_mode));
    MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (SUB (selector)))), (unt) SIZE (result_mode));
    genie_check_initialisation (p, STACK_TOP, result_mode);
    INCREMENT_STACK_POINTER (selector, SIZE (result_mode));
    UNIT (&self) = genie_selection_value_quick;
  }
  return self;
}

//! @brief Push selection from primary.

PROP_T genie_field_selection (NODE_T * p)
{
  ADDR_T pop_sp = A68_SP, pop_fp = A68_FP;
  NODE_T *entry = p;
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_PROCEDURE *w = (A68_PROCEDURE *) STACK_TOP;
  PROP_T self;
  SOURCE (&self) = entry;
  UNIT (&self) = genie_field_selection;
  EXECUTE_UNIT (SUB (p));
  for (p = SEQUENCE (SUB (p)); p != NO_NODE; p = SEQUENCE (p)) {
    MOID_T *m = MOID (p);
    MOID_T *result_mode = MOID (NODE_PACK (p));
    BOOL_T coerce = A68_TRUE;
    while (coerce) {
      if (IS_REF (m) && ISNT (SUB (m), STRUCT_SYMBOL)) {
        int size = SIZE (SUB (m));
        A68_SP = pop_sp;
        CHECK_REF (p, *z, m);
        PUSH (p, ADDRESS (z), size);
        genie_check_initialisation (p, STACK_OFFSET (-size), MOID (p));
        m = SUB (m);
      } else if (IS (m, PROC_SYMBOL)) {
        genie_check_initialisation (p, (BYTE_T *) w, m);
        genie_call_procedure (p, m, m, M_VOID, w, pop_sp, pop_fp);
        STACK_DNS (p, MOID (p), A68_FP);
        m = SUB (m);
      } else {
        coerce = A68_FALSE;
      }
    }
    if (IS_REF (m) && IS (SUB (m), STRUCT_SYMBOL)) {
      CHECK_REF (p, *z, m);
      OFFSET (z) += OFFSET (NODE_PACK (p));
    } else if (IS_STRUCT (m)) {
      A68_SP = pop_sp;
      MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (p))), (unt) SIZE (result_mode));
      INCREMENT_STACK_POINTER (p, SIZE (result_mode));
    }
  }
  return self;
}

