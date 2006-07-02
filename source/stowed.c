/*!
\file stowed.c
\brief routines for handling stowed objects
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2006 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
An A68G row is a reference to a descriptor in the heap:

               ...
A68_REF row -> A68_ARRAY ----+   ARRAY: Description of row, ref to elements
               A68_TUPLE 1   |   TUPLE: Bounds, one for every dimension
               ...           |
               A68_TUPLE dim |
               ...           |
               ...           |
               Element 1 <---+   Element: Sequential row elements, in the heap
               ...                        Not always contiguous - trims!
               Element n
*/

#include "algol68g.h"
#include "genie.h"

static void genie_copy_union (NODE_T *, BYTE_T *, BYTE_T *, A68_REF);
static A68_REF genie_copy_row (A68_REF, NODE_T *, MOID_T *);

/*!
\brief return size of a row
\param tup
\param dimensions
\return
**/

int get_row_size (A68_TUPLE * tup, int dimensions)
{
  int span = 1, k;
  for (k = 0; k < dimensions; k++) {
    int stride = ROW_SIZE (&tup[k]);
    ABNORMAL_END ((stride > 0 && span > MAX_INT / stride), ERROR_INVALID_SIZE, "get_row_size");
    span *= stride;
  }
  return (span);
}

/*!
\brief initialise index for FORALL constructs
\param tup
\param dimensions
**/

void initialise_internal_index (A68_TUPLE * tup, int dimensions)
{
  int k;
  for (k = 0; k < dimensions; k++) {
    A68_TUPLE *ref = &tup[k];
    ref->k = ref->lower_bound;
  }
}

/*!
\brief calculate index
\param tup
\param dimensions
\return
**/

ADDR_T calculate_internal_index (A68_TUPLE * tup, int dimensions)
{
  ADDR_T index = 0;
  int k;
  for (k = 0; k < dimensions; k++) {
    A68_TUPLE *ref = &tup[k];
    index += ref->span * (ref->k - ref->shift);
  }
  return (index);
}

/*!
\brief increment index for FORALL constructs
\param tup
\param dimensions
\return
**/

BOOL_T increment_internal_index (A68_TUPLE * tup, int dimensions)
{
/* Returns whether maximum index + 1 is reached. */
  BOOL_T carry = A_TRUE;
  int k;
  for (k = dimensions - 1; k >= 0 && carry; k--) {
    A68_TUPLE *ref = &tup[k];
    if (ref->k < ref->upper_bound) {
      (ref->k)++;
      carry = A_FALSE;
    } else {
      ref->k = ref->lower_bound;
    }
  }
  return (carry);
}

/*!
\brief print index
\param tup
\param dimensions
\return
**/

void print_internal_index (FILE_T f, A68_TUPLE * tup, int dimensions)
{
  int k;
  for (k = 0; k < dimensions; k++) {
    A68_TUPLE *ref = &tup[k];
    char buf[BUFFER_SIZE];
    snprintf (buf, BUFFER_SIZE, "%d", ref->k);
    io_write_string (f, buf);
    if (k < dimensions - 1) {
      io_write_string (f, ", ");
    }
  }
}

/*!
\brief convert C string to A68 [] CHAR
\param p
\param str
\param width
\return
**/

A68_REF c_string_to_row_char (NODE_T * p, char *str, int width)
{
  A68_REF z, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int str_size, k;
  ADDR_T ref_h;
  str_size = strlen (str);
  z = heap_generator (p, MODE (ROW_CHAR), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&z);
  row = heap_generator (p, MODE (ROW_CHAR), width * SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&row);
  arr.dimensions = 1;
  arr.type = MODE (CHAR);
  arr.elem_size = SIZE_OF (A68_CHAR);
  arr.slice_offset = 0;
  arr.field_offset = 0;
  arr.array = row;
  tup.lower_bound = 1;
  tup.upper_bound = width;
  tup.shift = tup.lower_bound;
  tup.span = 1;
  PUT_DESCRIPTOR (arr, tup, &z);
  ref_h = row.offset + row.handle->offset;
  for (k = 0; k < width; k++) {
    A68_CHAR ch;
    ch.status = INITIALISED_MASK;
    ch.value = (k < str_size ? str[k] : NULL_CHAR);
    *(A68_CHAR *) HEAP_ADDRESS (ref_h) = ch;
    ref_h += SIZE_OF (A68_CHAR);
  }
  UNPROTECT_SWEEP_HANDLE (&z);
  UNPROTECT_SWEEP_HANDLE (&row);
  return (z);
}

/*!
\brief convert C string to A68 string
\param p
\param str
\return
**/

A68_REF c_to_a_string (NODE_T * p, char *str)
{
  if (str == NULL) {
    return (empty_string (p));
  } else {
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int str_size, k;
    ADDR_T ref_h;
    str_size = strlen (str);
    z = heap_generator (p, MODE (ROW_CHAR), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_CHAR), str_size * SIZE_OF (A68_CHAR));
    PROTECT_SWEEP_HANDLE (&row);
    arr.dimensions = 1;
    arr.type = MODE (CHAR);
    arr.elem_size = SIZE_OF (A68_CHAR);
    arr.slice_offset = arr.field_offset = 0;
    arr.array = row;
    tup.lower_bound = 1;
    tup.upper_bound = str_size;
    tup.shift = tup.lower_bound;
    tup.span = 1;
    PUT_DESCRIPTOR (arr, tup, &z);
    ref_h = row.offset + row.handle->offset;
    for (k = 0; k < str_size; k++) {
      A68_CHAR ch;
      ch.status = INITIALISED_MASK;
      ch.value = str[k];
      *(A68_CHAR *) HEAP_ADDRESS (ref_h) = ch;
      ref_h += SIZE_OF (A68_CHAR);
    }
    UNPROTECT_SWEEP_HANDLE (&z);
    UNPROTECT_SWEEP_HANDLE (&row);
    return (z);
  }
}

/*!
\brief yield the size of a string
\param p
\param row
\return
**/

int a68_string_size (NODE_T * p, A68_REF row)
{
  (void) p;
  if (row.status & INITIALISED_MASK) {
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    GET_DESCRIPTOR (arr, tup, &row);
    return (ROW_SIZE (tup));
  } else {
    return (0);
  }
}

/*!
\brief convert A68 string to C string
\param p
\param str
\param row
\return
**/

char *a_to_c_string (NODE_T * p, char *str, A68_REF row)
{
/* Assume "str" to be long enough - caller's responsibility! */
  (void) p;
  if (row.status & INITIALISED_MASK) {
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    int size, n = 0;
    GET_DESCRIPTOR (arr, tup, &row);
    size = ROW_SIZE (tup);
    if (size > 0) {
      int k;
      BYTE_T *base_address = ADDRESS (&arr->array);
      for (k = tup->lower_bound; k <= tup->upper_bound; k++) {
        int addr = INDEX_1_DIM (arr, tup, k);
        A68_CHAR *ch = (A68_CHAR *) & (base_address[addr]);
        TEST_INIT (p, *ch, MODE (CHAR));
        str[n++] = ch->value;
      }
    }
    str[n] = NULL_CHAR;
    return (str);
  } else {
    return (NULL);
  }
}

/*!
\brief return an empty row
\param p
\param u
\return
**/

A68_REF empty_row (NODE_T * p, MOID_T * u)
{
  A68_REF ref_desc;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int dim, k;
  if (WHETHER (u, FLEX_SYMBOL)) {
    u = SUB (u);
  }
  dim = DIMENSION (u);
  ref_desc = heap_generator (p, u, SIZE_OF (A68_ARRAY) + dim * SIZE_OF (A68_TUPLE));
  GET_DESCRIPTOR (arr, tup, &ref_desc);
  DIMENSION (arr) = dim;
  MOID (arr) = SLICE (u);
  arr->elem_size = moid_size (SLICE (u));
  arr->slice_offset = 0;
  arr->field_offset = 0;
  arr->array.status = INITIALISED_MASK;
  arr->array.segment = heap_segment;
  arr->array.offset = 0;
  arr->array.handle = &nil_handle;
  for (k = 0; k < dim; k++) {
    tup[k].lower_bound = 1;
    tup[k].upper_bound = 0;
    tup[k].span = 0;
    tup[k].shift = tup->lower_bound;
  }
  return (ref_desc);
}

/*!
\brief an empty string, FLEX [1 : 0] CHAR
\param p
\return
**/

A68_REF empty_string (NODE_T * p)
{
  return (empty_row (p, MODE (STRING)));
}

/*!
\brief make [,, ..] MODE  from [, ..] MODE
\param p
\param row_mode
\param elems_in_stack
\param sp
\return
**/

A68_REF genie_concatenate_rows (NODE_T * p, MOID_T * row_mode, int elems_in_stack, ADDR_T sp)
{
  MOID_T *new_mode = WHETHER (row_mode, FLEX_SYMBOL) ? SUB (row_mode) : row_mode;
  MOID_T *elem_mode = SUB (new_mode);
  A68_REF new_row, old_row;
  A68_ARRAY *new_arr, *old_arr;
  A68_TUPLE *new_tup, *old_tup;
  int span, old_dim = DIMENSION (new_mode) - 1;
/* Make the new descriptor. */
  UP_SWEEP_SEMA;
  new_row = heap_generator (p, row_mode, SIZE_OF (A68_ARRAY) + DIMENSION (new_mode) * SIZE_OF (A68_TUPLE));
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIMENSION (new_arr) = DIMENSION (new_mode);
  MOID (new_arr) = elem_mode;
  new_arr->elem_size = MOID_SIZE (elem_mode);
  new_arr->slice_offset = 0;
  new_arr->field_offset = 0;
  if (elems_in_stack == 0) {
/* There is a vacuum on the stack. */
    int k;
    elems_in_stack = 1;
    span = 0;
    for (k = 0; k < old_dim; k++) {
      new_tup[k + 1].lower_bound = 1;
      new_tup[k + 1].upper_bound = 0;
      new_tup[k + 1].shift = new_tup[k + 1].lower_bound;
      new_tup[k + 1].span = 1;
    }
  } else {
    int k;
    A68_ARRAY *dummy = NULL;
    if (elems_in_stack > 1) {
/* All arrays in the stack must have the same bounds with respect to (arbitrary) first one. */
      int i;
      for (i = 1; i < elems_in_stack; i++) {
        A68_REF run_row, ref_row;
        A68_TUPLE *run_tup, *ref_tup;
        int j;
        ref_row = *(A68_REF *) STACK_ADDRESS (sp);
        run_row = *(A68_REF *) STACK_ADDRESS (sp + i * SIZE_OF (A68_REF));
        GET_DESCRIPTOR (dummy, ref_tup, &ref_row);
        GET_DESCRIPTOR (dummy, run_tup, &run_row);
        for (j = 0; j < old_dim; j++) {
          if ((ref_tup->upper_bound != run_tup->upper_bound) || (ref_tup->lower_bound != run_tup->lower_bound)) {
            diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
            exit_genie (p, A_RUNTIME_ERROR);
          }
          ref_tup++;
          run_tup++;
        }
      }
    }
/* Ok, same size. Fill descriptor of new row with info from (arbitrary) first one. */
    old_row = *(A68_REF *) STACK_ADDRESS (sp);
    GET_DESCRIPTOR (dummy, old_tup, &old_row);
    span = 1;
    for (k = 0; k < old_dim; k++) {
      A68_TUPLE *new_t = &new_tup[k + 1], *old_t = &old_tup[k];
      new_t->lower_bound = old_t->lower_bound;
      new_t->upper_bound = old_t->upper_bound;
      new_t->shift = new_t->lower_bound;
      new_t->span = span;
      span *= ROW_SIZE (new_t);
    }
  }
  new_tup->lower_bound = 1;
  new_tup->upper_bound = elems_in_stack;
  new_tup->shift = new_tup->lower_bound;
  new_tup->span = span;
/* Allocate space for the big new row. */
  new_arr->array = heap_generator (p, row_mode, elems_in_stack * span * new_arr->elem_size);
  if (span > 0) {
/* Copy 'elems_in_stack' rows into the new one. */
    int j;
    BYTE_T *new_elem = ADDRESS (&(new_arr->array));
    for (j = 0; j < elems_in_stack; j++) {
/* new [j, , ] := old [, ] */
      BYTE_T *old_elem;
      BOOL_T done;
      GET_DESCRIPTOR (old_arr, old_tup, (A68_REF *) STACK_ADDRESS (sp + j * SIZE_OF (A68_REF)));
      old_elem = ADDRESS (&(old_arr->array));
      initialise_internal_index (old_tup, old_dim);
      initialise_internal_index (&new_tup[1], old_dim);
      done = A_FALSE;
      while (!done) {
        ADDR_T old_index, new_index, old_addr, new_addr;
        old_index = calculate_internal_index (old_tup, old_dim);
        new_index = j * new_tup->span + calculate_internal_index (&new_tup[1], old_dim);
        old_addr = ROW_ELEMENT (old_arr, old_index);
        new_addr = ROW_ELEMENT (new_arr, new_index);
        MOVE (&new_elem[new_addr], &old_elem[old_addr], (unsigned) new_arr->elem_size);
        done = increment_internal_index (old_tup, old_dim) | increment_internal_index (&new_tup[1], old_dim);
      }
    }
  }
  DOWN_SWEEP_SEMA;
  return (new_row);
}

/*!
\brief make a row of 'elems_in_stack' objects that are in the stack
\param p
\param elem_mode
\param elems_in_stack
\param sp
\return
**/

A68_REF genie_make_row (NODE_T * p, MOID_T * elem_mode, int elems_in_stack, ADDR_T sp)
{
  A68_REF new_row, new_arr;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int k;
  new_row = heap_generator (p, MOID (p), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  new_arr = heap_generator (p, MOID (p), elems_in_stack * MOID_SIZE (elem_mode));
  PROTECT_SWEEP_HANDLE (&new_arr);
  GET_DESCRIPTOR (arr, tup, &new_row);
  DIMENSION (arr) = 1;
  MOID (arr) = elem_mode;
  arr->elem_size = MOID_SIZE (elem_mode);
  arr->slice_offset = 0;
  arr->field_offset = 0;
  arr->array = new_arr;
  tup->lower_bound = 1;
  tup->upper_bound = elems_in_stack;
  tup->shift = tup->lower_bound;
  tup->span = 1;
  for (k = 0; k < elems_in_stack; k++) {
    ADDR_T offset = k * arr->elem_size;
    BYTE_T *src_a, *dst_a;
    A68_REF dst = new_arr, src;
    dst.offset += offset;
    src.status = INITIALISED_MASK;
    src.segment = stack_segment;
    src.offset = sp + offset;
    src.handle = &nil_handle;
    dst_a = ADDRESS (&dst);
    src_a = ADDRESS (&src);
    if (elem_mode->has_rows) {
      if (WHETHER (elem_mode, STRUCT_SYMBOL)) {
        A68_REF new_one = genie_copy_stowed (src, p, elem_mode);
        MOVE (dst_a, ADDRESS (&new_one), MOID_SIZE (elem_mode));
      } else if (WHETHER (elem_mode, FLEX_SYMBOL) || elem_mode == MODE (STRING)) {
        *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, DEFLEX (elem_mode));
      } else if (WHETHER (elem_mode, ROW_SYMBOL)) {
        *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, elem_mode);
      } else if (WHETHER (elem_mode, UNION_SYMBOL)) {
        genie_copy_union (p, dst_a, src_a, src);
      } else {
        ABNORMAL_END (A_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_make_row");
      }
    } else {
      MOVE (dst_a, src_a, (unsigned) arr->elem_size);
    }
  }
  UNPROTECT_SWEEP_HANDLE (&new_row);
  UNPROTECT_SWEEP_HANDLE (&new_arr);
  return (new_row);
}

/*!
\brief make REF [1 : 1] [] MODE from REF [] MODE
\param p
\param dst_mode
\param src_mode
\param sp
\return
**/

A68_REF genie_make_ref_row_of_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  A68_REF new_row, name, array;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  ADDR_T scope;
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  array = *(A68_REF *) STACK_ADDRESS (sp);
/* ROWING NIL yields NIL. */
  if (IS_NIL (array)) {
    return (nil_ref);
  }
/* The new row will be as young as the widened value. */
  scope = array.scope;
  new_row = heap_generator (p, SUB (dst_mode), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  name = heap_generator (p, dst_mode, SIZE_OF (A68_REF));
  GET_DESCRIPTOR (arr, tup, &new_row);
  DIMENSION (arr) = 1;
  MOID (arr) = src_mode;
  arr->elem_size = MOID_SIZE (src_mode);
  arr->slice_offset = 0;
  arr->field_offset = 0;
  arr->array = array;
  tup->lower_bound = 1;
  tup->upper_bound = 1;
  tup->shift = tup->lower_bound;
  tup->span = 1;
  *(A68_REF *) ADDRESS (&name) = new_row;
  UNPROTECT_SWEEP_HANDLE (&new_row);
  name.scope = scope;
  return (name);
}

/*!
\brief make REF [1 : 1, ..] MODE from REF [..] MODE
\param p
\param dst_mode
\param src_mode
\param sp
\return
**/

A68_REF genie_make_ref_row_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  A68_REF name, new_row, old_row;
  A68_ARRAY *new_arr, *old_arr;
  A68_TUPLE *new_tup, *old_tup;
  ADDR_T scope;
  int k;
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  name = *(A68_REF *) STACK_ADDRESS (sp);
/* ROWING NIL yields NIL. */
  if (IS_NIL (name)) {
    return (nil_ref);
  }
/* The new row will be as young as the widened value. */
  scope = name.scope;
  old_row = *(A68_REF *) ADDRESS (&name);
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
/* Make new descriptor. */
  new_row = heap_generator (p, dst_mode, SIZE_OF (A68_ARRAY) + DIMENSION (SUB (dst_mode)) * SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  name = heap_generator (p, dst_mode, SIZE_OF (A68_REF));
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIMENSION (new_arr) = DIMENSION (SUB (dst_mode));
  MOID (new_arr) = MOID (old_arr);
  new_arr->elem_size = old_arr->elem_size;
  new_arr->slice_offset = 0;
  new_arr->field_offset = 0;
  new_arr->array = old_arr->array;
/* Fill out the descriptor. */
  new_tup[0].lower_bound = 1;
  new_tup[0].upper_bound = 1;
  new_tup[0].shift = new_tup[0].lower_bound;
  new_tup[0].span = 1;
  for (k = 0; k < DIMENSION (SUB (src_mode)); k++) {
    new_tup[k + 1] = old_tup[k];
  }
/* Yield the new name. */
  *(A68_REF *) ADDRESS (&name) = new_row;
  UNPROTECT_SWEEP_HANDLE (&new_row);
  name.scope = scope;
  return (name);
}

/*!
\brief coercion to [1 : 1, ] MODE
\param p
\return
**/

PROPAGATOR_T genie_rowing_row_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  row = genie_concatenate_rows (p, MOID (p), 1, sp);
  stack_pointer = sp;
  PUSH_REF (p, row);
  PROTECT_FROM_SWEEP (p);
  return (p->genie.propagator);
}

/*!
\brief coercion to [1 : 1] [] MODE
\param p
\return
**/

PROPAGATOR_T genie_rowing_row_of_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  row = genie_make_row (p, SLICE (MOID (p)), 1, sp);
  stack_pointer = sp;
  PUSH_REF (p, row);
  PROTECT_FROM_SWEEP (p);
  return (p->genie.propagator);
}

/*!
\brief coercion to REF [1 : 1, ..] MODE
\param p
\return
**/

PROPAGATOR_T genie_rowing_ref_row_row (NODE_T * p)
{
  A68_REF name;
  ADDR_T sp = stack_pointer;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  stack_pointer = sp;
  name = genie_make_ref_row_row (p, dst, src, sp);
  PUSH_REF (p, name);
  PROTECT_FROM_SWEEP (p);
  return (p->genie.propagator);
}

/*!
\brief REF [1 : 1] [] MODE from [] MODE
\param p
\return
**/

PROPAGATOR_T genie_rowing_ref_row_of_row (NODE_T * p)
{
  A68_REF name;
  ADDR_T sp = stack_pointer;
  MOID_T *dst = MOID (p), *src = MOID (SUB (p));
  EXECUTE_UNIT (SUB (p));
  stack_pointer = sp;
  name = genie_make_ref_row_of_row (p, dst, src, sp);
  PUSH_REF (p, name);
  PROTECT_FROM_SWEEP (p);
  return (p->genie.propagator);
}

/*!
\brief rowing coercion
\param p
\return
**/

PROPAGATOR_T genie_rowing (NODE_T * p)
{
  PROPAGATOR_T self;
  if (WHETHER (MOID (p), REF_SYMBOL)) {
/* REF ROW, decide whether we want A->[] A or [] A->[,] A. */
    MOID_T *mode = SUB (MOID (p));
    if (DIMENSION (DEFLEX (mode)) >= 2) {
      genie_rowing_ref_row_row (p);
      self.unit = genie_rowing_ref_row_row;
      self.source = p;
    } else {
      genie_rowing_ref_row_of_row (p);
      self.unit = genie_rowing_ref_row_of_row;
      self.source = p;
    }
  } else {
/* ROW, decide whether we want A->[] A or [] A->[,] A. */
    if (DIMENSION (DEFLEX (MOID (p))) >= 2) {
      genie_rowing_row_row (p);
      self.unit = genie_rowing_row_row;
      self.source = p;
    } else {
      genie_rowing_row_of_row (p);
      self.unit = genie_rowing_row_of_row;
      self.source = p;
    }
  }
  return (self);
}

/*!
\brief revise lower bound
\param p
\param src
\param dst
**/

void genie_revise_lower_bound (NODE_T * p, A68_REF src, A68_REF dst)
{
  A68_ARRAY *src_arr, *dst_arr;
  A68_TUPLE *src_tup, *dst_tup;
  int src_stride, dst_stride;
  GET_DESCRIPTOR (src_arr, src_tup, &src);
  GET_DESCRIPTOR (dst_arr, dst_tup, &dst);
  src_stride = ROW_SIZE (&src_tup[0]);
  dst_stride = ROW_SIZE (&dst_tup[0]);
  if (src_stride != dst_stride) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  dst_tup->lower_bound = src_tup->lower_bound;
  dst_tup->upper_bound = src_tup->upper_bound;
  dst_tup->shift = src_tup->shift;
}

/*!
\brief copy a stowed united object
\param p
\param dst_a
\param src_a
\param struct_field
**/

static void genie_copy_union (NODE_T * p, BYTE_T * dst_a, BYTE_T * src_a, A68_REF struct_field)
{
  BYTE_T *dst_u = &(dst_a[UNION_OFFSET]), *src_u = &(src_a[UNION_OFFSET]);
  A68_UNION *u = (A68_UNION *) src_a;
  MOID_T *um = (MOID_T *) u->value;
  if (um != NULL) {
    *(A68_POINTER *) dst_a = *u;
    if (WHETHER (um, STRUCT_SYMBOL)) {
/* UNION (STRUCT ..) */
      A68_REF w = struct_field, src;
      w.offset += UNION_OFFSET;
      src = genie_copy_stowed (w, p, um);
      MOVE (dst_u, ADDRESS (&src), MOID_SIZE (um));
    } else if (WHETHER (um, FLEX_SYMBOL) || um == MODE (STRING)) {
/* UNION (FLEX [] A ..). Bounds are irrelevant: copy and assign. */
      *(A68_REF *) dst_u = genie_copy_row (*(A68_REF *) src_u, p, DEFLEX (um));
    } else if (WHETHER (um, ROW_SYMBOL)) {
/* UNION ([] A ..). Bounds are irrelevant: copy and assign. */
      *(A68_REF *) dst_u = genie_copy_row (*(A68_REF *) src_u, p, um);
    } else {
/* UNION (..). Non-stowed mode. */
      MOVE (dst_u, src_u, MOID_SIZE (um));
    }
  }
}

/*!
\brief make copy from array of mode 'm' from 'old'
\param old_row
\param p
\param m
\return
**/

A68_REF genie_copy_row (A68_REF old_row, NODE_T * p, MOID_T * m)
{
/* We need this complex routine since arrays are not always contiguous (trims). */
  A68_REF new_row;
  A68_ARRAY *old_arr, *new_arr;
  A68_TUPLE *old_tup, *new_tup, *old_p, *new_p;
  int k, span;
  UP_SWEEP_SEMA;
  if (IS_NIL (old_row)) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE, m);
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Cut FLEX from the mode. That is not important in this routine. */
  if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
    m = SUB (m);
  }
/* Make new array. */
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  new_row = heap_generator (p, m, SIZE_OF (A68_ARRAY) + DIMENSION (old_arr) * SIZE_OF (A68_TUPLE));
/* Get descriptor again in case the heap sweeper moved data (but switched off now) */
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIMENSION (new_arr) = DIMENSION (old_arr);
  MOID (new_arr) = MOID (old_arr);
  new_arr->elem_size = old_arr->elem_size;
  new_arr->slice_offset = 0;
  new_arr->field_offset = 0;
/* Get size and copy bounds; no checks since this routine just makes a copy. */
  span = 1;
  for (k = 0; k < DIMENSION (old_arr); k++) {
    old_p = &old_tup[k];
    new_p = &new_tup[k];
    new_p->lower_bound = old_p->lower_bound;
    new_p->upper_bound = old_p->upper_bound;
    new_p->span = span;
    new_p->shift = new_p->lower_bound;
    span *= ROW_SIZE (new_p);
  }
  new_arr->array = heap_generator (p, MOID (p), span * new_arr->elem_size);
/* The n-dimensional copier. */
  if (span > 0) {
    unsigned elem_size = (unsigned) moid_size (MOID (old_arr));
    BYTE_T *old_elem = ADDRESS (&(old_arr->array));
    BYTE_T *new_elem = ADDRESS (&(new_arr->array));
    BOOL_T done = A_FALSE;
    initialise_internal_index (old_tup, DIMENSION (old_arr));
    initialise_internal_index (new_tup, DIMENSION (new_arr));
    while (!done) {
      ADDR_T old_index, new_index, old_addr, new_addr;
      old_index = calculate_internal_index (old_tup, DIMENSION (old_arr));
      new_index = calculate_internal_index (new_tup, DIMENSION (new_arr));
      old_addr = ROW_ELEMENT (old_arr, old_index);
      new_addr = ROW_ELEMENT (new_arr, new_index);
      if (SUB (m)->has_rows) {
/* Recursion. */
        A68_REF new_old = old_arr->array, new_dst = new_arr->array;
        BYTE_T *src_a, *dst_a;
        new_old.offset += old_addr;
        new_dst.offset += new_addr;
        src_a = ADDRESS (&new_old);
        dst_a = ADDRESS (&new_dst);
        if (WHETHER (SUB (m), STRUCT_SYMBOL)) {
          A68_REF str_src = genie_copy_stowed (new_old, p, SUB (m));
          MOVE (dst_a, ADDRESS (&str_src), MOID_SIZE (SUB (m)));
        } else if (WHETHER (SUB (m), FLEX_SYMBOL) || SUB (m) == MODE (STRING)) {
          *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, DEFLEX (SUB (m)));
        } else if (WHETHER (SUB (m), ROW_SYMBOL)) {
          *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, SUB (m));
        } else if (WHETHER (SUB (m), UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, new_old);
        } else {
          ABNORMAL_END (A_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_row");
        }
      } else {
        MOVE (&new_elem[new_addr], &old_elem[old_addr], elem_size);
      }
/* Increase pointers. */
      done = increment_internal_index (old_tup, DIMENSION (old_arr)) | increment_internal_index (new_tup, DIMENSION (new_arr));
    }
  }
  DOWN_SWEEP_SEMA;
  return (new_row);
}

/*!
\brief assign array of MODE 'm' from 'old_row' to 'dst'
\param old_row
\param dst
\param p
\param m
\return
**/

static A68_REF genie_assign_row (A68_REF old_row, A68_REF * dst, NODE_T * p, MOID_T * m)
{
  A68_REF new_row;
  A68_ARRAY *old_arr, *new_arr;
  A68_TUPLE *old_tup, *new_tup;
  int k, span = 0;
/* Get row desriptors. */
  new_row = *dst;
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
/* In case of non FLEX rows, the arrays are now equally large so we can overwrite.
   Not so with FLEX rows, so we make a new descriptor. */
    m = SUB (m);
    span = 1;
    for (k = 0; k < DIMENSION (old_arr); k++) {
      A68_TUPLE *old_p = &old_tup[k], *new_p = &new_tup[k];
      new_p->lower_bound = old_p->lower_bound;
      new_p->upper_bound = old_p->upper_bound;
      new_p->span = span;
      new_p->shift = new_p->lower_bound;
      span *= ROW_SIZE (new_p);
    }
    UP_SWEEP_SEMA;
    new_arr->array = heap_generator (p, m, span * old_arr->elem_size);
    DOWN_SWEEP_SEMA;
  } else if (WHETHER (m, ROW_SYMBOL)) {
    for (k = 0; k < DIMENSION (old_arr); k++) {
      A68_TUPLE *old_p = &old_tup[k], *new_p = &new_tup[k];
      if ((new_p->upper_bound != old_p->upper_bound) || (new_p->lower_bound != old_p->lower_bound)) {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
        exit_genie (p, A_RUNTIME_ERROR);
      }
    }
    span = 1;
    for (k = 0; k < DIMENSION (old_arr); k++) {
      A68_TUPLE *old_p = &old_tup[k];
      span *= ROW_SIZE (old_p);
    }
  } else {
    ABNORMAL_END (A_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_row");
  }
/* The n-dimensional copier. */
  initialise_internal_index (old_tup, DIMENSION (old_arr));
  initialise_internal_index (new_tup, DIMENSION (new_arr));
  if (span > 0) {
    unsigned elem_size = (unsigned) moid_size (MOID (old_arr));
    BOOL_T done;
    BYTE_T *old_elem, *new_elem;
    old_elem = ADDRESS (&(old_arr->array));
    new_elem = ADDRESS (&(new_arr->array));
    done = A_FALSE;
    while (!done) {
      ADDR_T old_index, new_index, old_addr, new_addr;
      old_index = calculate_internal_index (old_tup, DIMENSION (old_arr));
      new_index = calculate_internal_index (new_tup, DIMENSION (new_arr));
      old_addr = ROW_ELEMENT (old_arr, old_index);
      new_addr = ROW_ELEMENT (new_arr, new_index);
      if (SUB (m)->has_rows) {
/* Recursion. */
        A68_REF new_old = old_arr->array, new_dst = new_arr->array;
        BYTE_T *src_a, *dst_a;
        new_old.offset += old_addr;
        new_dst.offset += new_addr;
        src_a = ADDRESS (&new_old);
        dst_a = ADDRESS (&new_dst);
        if (WHETHER (SUB (m), STRUCT_SYMBOL)) {
          genie_assign_stowed (new_old, &new_dst, p, SUB (m));
        } else if (ATTRIBUTE (SUB (m)) == FLEX_SYMBOL || SUB (m) == MODE (STRING)) {
          A68_REF dst_addr = *(A68_REF *) dst_a;
/* Algol68G does not know ghost elements. NIL means an initially empty row. */
          if (IS_NIL (dst_addr)) {
            *(A68_REF *) dst_a = *(A68_REF *) src_a;
          } else {
            genie_assign_stowed (*(A68_REF *) src_a, &dst_addr, p, SUB (m));
          }
        } else if (ATTRIBUTE (SUB (m)) == ROW_SYMBOL) {
          A68_REF dst_addr = *(A68_REF *) dst_a;
/* Algol68G does not know ghost elements. NIL means an initially empty row. */
          if (IS_NIL (dst_addr)) {
            *(A68_REF *) dst_a = *(A68_REF *) src_a;
          } else {
            genie_assign_stowed (*(A68_REF *) src_a, &dst_addr, p, SUB (m));
          }
        } else if (WHETHER (SUB (m), UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, new_old);
        } else {
          ABNORMAL_END (A_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_row");
        }
      } else {
        MOVE (&new_elem[new_addr], &old_elem[old_addr], elem_size);
      }
/* Increase pointers. */
      done = increment_internal_index (old_tup, DIMENSION (old_arr)) | increment_internal_index (new_tup, DIMENSION (new_arr));
    }
  }
  return (new_row);
}

/*!
\brief assign multiple value of mode 'm' from 'old' to 'dst'
\param old
\param dst
\param p
\param m
\return
**/

A68_REF genie_assign_stowed (A68_REF old, A68_REF * dst, NODE_T * p, MOID_T * m)
{
  if (WHETHER (m, STRUCT_SYMBOL)) {
    PACK_T *fields;
    A68_REF new_struct;
    UP_SWEEP_SEMA;
    new_struct = *dst;
    for (fields = PACK (m); fields != NULL; fields = NEXT (fields)) {
      A68_REF old_field = old, new_field = new_struct;
      BYTE_T *src_a, *dst_a;
      old_field.offset += fields->offset;
      new_field.offset += fields->offset;
      src_a = ADDRESS (&old_field);
      dst_a = ADDRESS (&new_field);
      if (MOID (fields)->has_rows) {
        if (WHETHER (MOID (fields), STRUCT_SYMBOL)) {
/* STRUCT (STRUCT (..) ..) */
          genie_assign_stowed (old_field, &new_field, p, MOID (fields));
        } else if (ATTRIBUTE (MOID (fields)) == FLEX_SYMBOL || MOID (fields) == MODE (STRING)) {
/* STRUCT (FLEX [] A ..) */
          *(A68_REF *) dst_a = *(A68_REF *) src_a;
        } else if (ATTRIBUTE (MOID (fields)) == ROW_SYMBOL) {
/* STRUCT (FLEX [] A ..) */
          A68_REF arr_src = *(A68_REF *) src_a;
          A68_REF arr_dst = *(A68_REF *) dst_a;
          genie_assign_row (arr_src, &arr_dst, p, MOID (fields));
        } else if (WHETHER (MOID (fields), UNION_SYMBOL)) {
/* UNION (..) */
          genie_copy_union (p, dst_a, src_a, old_field);
        } else {
          ABNORMAL_END (A_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_stowed");
        }
      } else {
        MOVE (dst_a, src_a, MOID_SIZE (fields));
      }
    }
    DOWN_SWEEP_SEMA;
    return (new_struct);
  } else if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
    A68_REF new_row;
    UP_SWEEP_SEMA;
    new_row = genie_assign_row (old, dst, p, m);
    DOWN_SWEEP_SEMA;
    return (new_row);
  } else if (WHETHER (m, ROW_SYMBOL)) {
    A68_REF new_row;
    UP_SWEEP_SEMA;
    new_row = genie_assign_row (old, dst, p, m);
    DOWN_SWEEP_SEMA;
    return (new_row);
  } else {
    ABNORMAL_END (A_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_stowed");
    return (nil_ref);
  }
}

/*!
\brief make copy of stowed value at 'old'
\param old
\param p
\param m
\return
**/

A68_REF genie_copy_stowed (A68_REF old, NODE_T * p, MOID_T * m)
{
  if (WHETHER (m, STRUCT_SYMBOL)) {
    PACK_T *fields;
    A68_REF new_struct;
    UP_SWEEP_SEMA;
    new_struct = heap_generator (p, m, MOID_SIZE (m));
    for (fields = PACK (m); fields != NULL; fields = NEXT (fields)) {
      A68_REF old_field = old, new_field = new_struct;
      BYTE_T *src_a, *dst_a;
      old_field.offset += fields->offset;
      new_field.offset += fields->offset;
      src_a = ADDRESS (&old_field);
      dst_a = ADDRESS (&new_field);
      if (MOID (fields)->has_rows) {
        if (WHETHER (MOID (fields), STRUCT_SYMBOL)) {
          A68_REF str_src = genie_copy_stowed (old_field, p, MOID (fields));
          MOVE (dst_a, ADDRESS (&str_src), MOID_SIZE (MOID (fields)));
        } else if (WHETHER (MOID (fields), FLEX_SYMBOL) || MOID (fields) == MODE (STRING)) {
          *(A68_REF *) dst_a = genie_copy_row (*(A68_REF *) src_a, p, MOID (fields));
        } else if (WHETHER (MOID (fields), ROW_SYMBOL)) {
          *(A68_REF *) dst_a = genie_copy_row (*(A68_REF *) src_a, p, MOID (fields));
        } else if (WHETHER (MOID (fields), UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, old_field);
        } else {
          ABNORMAL_END (A_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_stowed");
        }
      } else {
        MOVE (dst_a, src_a, MOID_SIZE (fields));
      }
    }
    DOWN_SWEEP_SEMA;
    return (new_struct);
  } else if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
    A68_REF new_row;
    UP_SWEEP_SEMA;
    new_row = genie_copy_row (old, p, DEFLEX (m));
    DOWN_SWEEP_SEMA;
    return (new_row);
  } else if (WHETHER (m, ROW_SYMBOL)) {
    A68_REF new_row;
    UP_SWEEP_SEMA;
    new_row = genie_copy_row (old, p, DEFLEX (m));
    DOWN_SWEEP_SEMA;
    return (new_row);
  } else {
    ABNORMAL_END (A_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_stowed");
    return (nil_ref);
  }
}

/* Operators for ROWS. */

/*!
\brief OP ELEMS = (ROWS) INT
\param p position in syntax tree, should not be NULL
**/

void genie_monad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));
  TEST_INIT (p, z, MODE (ROWS));
  TEST_NIL (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_INT (p, get_row_size (t, x->dimensions));
}

/*!
\brief OP LWB = (ROWS) INT
\param p position in syntax tree, should not be NULL
**/

void genie_monad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));
  TEST_INIT (p, z, MODE (ROWS));
  TEST_NIL (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_INT (p, t->lower_bound);
}

/*!
\brief OP UPB = (ROWS) INT
\param p position in syntax tree, should not be NULL
**/

void genie_monad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));
  TEST_INIT (p, z, MODE (ROWS));
  TEST_NIL (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_INT (p, t->upper_bound);
}

/*!
\brief OP ELEMS = (INT, ROWS) INT
\param p position in syntax tree, should not be NULL
**/

void genie_dyad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t, *u;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));
  TEST_INIT (p, z, MODE (ROWS));
  TEST_NIL (p, z, MODE (ROWS));
  POP_INT (p, &k);
  GET_DESCRIPTOR (x, t, &z);
  if (k.value < 1 || k.value > x->dimensions) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) k.value);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  u = &(t[k.value - 1]);
  PUSH_INT (p, ROW_SIZE (u));
}

/*!
\brief OP LWB = (INT, ROWS) INT
\param p position in syntax tree, should not be NULL
**/

void genie_dyad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));
  TEST_INIT (p, z, MODE (ROWS));
  TEST_NIL (p, z, MODE (ROWS));
  POP_INT (p, &k);
  GET_DESCRIPTOR (x, t, &z);
  if (k.value < 1 || k.value > x->dimensions) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) k.value);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  PUSH_INT (p, t[k.value - 1].lower_bound);
}

/*!
\brief OP UPB = (INT, ROWS) INT
\param p position in syntax tree, should not be NULL
**/

void genie_dyad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));
  TEST_INIT (p, z, MODE (ROWS));
  TEST_NIL (p, z, MODE (ROWS));
  POP_INT (p, &k);
  GET_DESCRIPTOR (x, t, &z);
  if (k.value < 1 || k.value > x->dimensions) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) k.value);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  PUSH_INT (p, t[k.value - 1].upper_bound);
}
