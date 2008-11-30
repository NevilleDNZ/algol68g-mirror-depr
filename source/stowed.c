/*!
\file stowed.c
\brief routines for handling stowed objects
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2008 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program. If not, see <http://www.gnu.org/licenses/>.
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
#include "inline.h"

static void genie_copy_union (NODE_T *, BYTE_T *, BYTE_T *, A68_REF);
static A68_REF genie_copy_row (A68_REF, NODE_T *, MOID_T *);

/*!
\brief size of a row
\param tup first tuple
\param tup dimension of row
\return same
**/

int get_row_size (A68_TUPLE * tup, int dim)
{
  int span = 1, k;
  for (k = 0; k < dim; k++) {
    int stride = ROW_SIZE (&tup[k]);
    ABNORMAL_END ((stride > 0 && span > A68_MAX_INT / stride), ERROR_INVALID_SIZE, "get_row_size");
    span *= stride;
  }
  return (span);
}

/*!
\brief initialise index for FORALL constructs
\param tup first tuple
\param tup dimension of row
**/

void initialise_internal_index (A68_TUPLE * tup, int dim)
{
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    ref->k = LWB (ref);
  }
}

/*!
\brief calculate index
\param tup first tuple
\param tup dimension of row
\return same
**/

ADDR_T calculate_internal_index (A68_TUPLE * tup, int dim)
{
  ADDR_T index = 0;
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    index += (ref->span * ref->k - ref->shift);
  }
  return (index);
}

/*!
\brief increment index for FORALL constructs
\param tup first tuple
\param tup dimension of row
\return whetehr maximum (index + 1) is reached
**/

BOOL_T increment_internal_index (A68_TUPLE * tup, int dim)
{
  BOOL_T carry = A68_TRUE;
  int k;
  for (k = dim - 1; k >= 0 && carry; k--) {
    A68_TUPLE *ref = &tup[k];
    if (ref->k < UPB (ref)) {
      (ref->k)++;
      carry = A68_FALSE;
    } else {
      ref->k = LWB (ref);
    }
  }
  return (carry);
}

/*!
\brief print index
\param tup first tuple
\param tup dimension of row
**/

void print_internal_index (FILE_T f, A68_TUPLE * tup, int dim)
{
  int k;
  for (k = 0; k < dim; k++) {
    A68_TUPLE *ref = &tup[k];
    char buf[BUFFER_SIZE];
    snprintf (buf, BUFFER_SIZE, "%d", ref->k);
    WRITE (f, buf);
    if (k < dim - 1) {
      WRITE (f, ", ");
    }
  }
}

/*!
\brief convert C string to A68 [] CHAR
\param p position in tree
\param str string to convert
\param width width of string
\return pointer to [] CHAR
**/

A68_REF c_string_to_row_char (NODE_T * p, char *str, int width)
{
  A68_REF z, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  BYTE_T *base;
  int str_size, k;
  str_size = strlen (str);
  z = heap_generator (p, MODE (ROW_CHAR), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&z);
  row = heap_generator (p, MODE (ROW_CHAR), width * ALIGNED_SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (CHAR);
  arr.elem_size = ALIGNED_SIZE_OF (A68_CHAR);
  arr.slice_offset = 0;
  arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = width;
  tup.span = 1;
  tup.shift = LWB (&tup);
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &z);
  base = ADDRESS (&row);
  for (k = 0; k < width; k++) {
    A68_CHAR *ch = (A68_CHAR *) & (base[k * ALIGNED_SIZE_OF (A68_CHAR)]);
    STATUS (ch) = INITIALISED_MASK;
    VALUE (ch) = str[k];
  }
  UNPROTECT_SWEEP_HANDLE (&z);
  UNPROTECT_SWEEP_HANDLE (&row);
  return (z);
}

/*!
\brief convert C string to A68 string
\param p position in tree
\param str string to convert
\return STRING
**/

A68_REF c_to_a_string (NODE_T * p, char *str)
{
  if (str == NULL) {
    return (empty_string (p));
  } else {
    return (c_string_to_row_char (p, str, strlen (str)));
  }
}

/*!
\brief size of a string
\param p position in tree
\param row row, pointer to descriptor
\return same
**/

int a68_string_size (NODE_T * p, A68_REF row)
{
  (void) p;
  if (INITIALISED (&row)) {
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
\param p position in tree
\param str string to store result
\param row STRING to convert
\return str on success or NULL on failure
**/

char *a_to_c_string (NODE_T * p, char *str, A68_REF row)
{
/* Assume "str" to be long enough - caller's responsibility! */
  (void) p;
  if (INITIALISED (&row)) {
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    int size, n = 0;
    GET_DESCRIPTOR (arr, tup, &row);
    size = ROW_SIZE (tup);
    if (size > 0) {
      int k;
      BYTE_T *base_address = ADDRESS (&ARRAY (arr));
      for (k = LWB (tup); k <= UPB (tup); k++) {
        int addr = INDEX_1_DIM (arr, tup, k);
        A68_CHAR *ch = (A68_CHAR *) & (base_address[addr]);
        CHECK_INIT (p, INITIALISED (ch), MODE (CHAR));
        str[n++] = VALUE (ch);
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
\param p position in tree
\param u mode of row
\return fat pointer to descriptor
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
  dim = DIM (u);
  ref_desc = heap_generator (p, u, ALIGNED_SIZE_OF (A68_ARRAY) + dim * ALIGNED_SIZE_OF (A68_TUPLE));
  GET_DESCRIPTOR (arr, tup, &ref_desc);
  DIM (arr) = dim;
  MOID (arr) = SLICE (u);
  arr->elem_size = moid_size (SLICE (u));
  arr->slice_offset = 0;
  arr->field_offset = 0;
  STATUS (&ARRAY (arr)) = INITIALISED_MASK | IN_HEAP_MASK;
  ARRAY (arr).offset = 0;
  REF_HANDLE (&(ARRAY (arr))) = &nil_handle;
  for (k = 0; k < dim; k++) {
    tup[k].lower_bound = 1;
    tup[k].upper_bound = 0;
    tup[k].span = 1;
    tup[k].shift = LWB (tup);
  }
  return (ref_desc);
}

/*!
\brief an empty string, FLEX [1 : 0] CHAR
\param p position in tree
\return fat pointer to descriptor
**/

A68_REF empty_string (NODE_T * p)
{
  return (empty_row (p, MODE (STRING)));
}

/*!
\brief make [,, ..] MODE  from [, ..] MODE
\param p position in tree
\param row_mode row of mode
\param elems_in_stack number of elements
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_concatenate_rows (NODE_T * p, MOID_T * row_mode, int elems_in_stack, ADDR_T sp)
{
  MOID_T *new_mode = WHETHER (row_mode, FLEX_SYMBOL) ? SUB (row_mode) : row_mode;
  MOID_T *elem_mode = SUB (new_mode);
  A68_REF new_row, old_row;
  A68_ARRAY *new_arr, *old_arr;
  A68_TUPLE *new_tup, *old_tup;
  int span, old_dim = DIM (new_mode) - 1;
/* Make the new descriptor. */
  UP_SWEEP_SEMA;
  new_row = heap_generator (p, row_mode, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (new_mode) * ALIGNED_SIZE_OF (A68_TUPLE));
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (new_mode);
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
      new_tup[k + 1].span = 1;
      new_tup[k + 1].shift = new_tup[k + 1].lower_bound;
    }
  } else {
    int k;
    A68_ARRAY *dummy = NULL;
    if (elems_in_stack > 1) {
/* AARRAY(&ll)s in the stack must have the same bounds with respect to (arbitrary) first one. */
      int i;
      for (i = 1; i < elems_in_stack; i++) {
        A68_REF run_row, ref_row;
        A68_TUPLE *run_tup, *ref_tup;
        int j;
        ref_row = *(A68_REF *) STACK_ADDRESS (sp);
        run_row = *(A68_REF *) STACK_ADDRESS (sp + i * ALIGNED_SIZE_OF (A68_REF));
        GET_DESCRIPTOR (dummy, ref_tup, &ref_row);
        GET_DESCRIPTOR (dummy, run_tup, &run_row);
        for (j = 0; j < old_dim; j++) {
          if ((UPB (ref_tup) != UPB (run_tup)) || (LWB (ref_tup) != LWB (run_tup))) {
            diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
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
      LWB (new_t) = LWB (old_t);
      UPB (new_t) = UPB (old_t);
      new_t->span = span;
      new_t->shift = LWB (new_t) * new_t->span;
      span *= ROW_SIZE (new_t);
    }
  }
  LWB (new_tup) = 1;
  UPB (new_tup) = elems_in_stack;
  new_tup->span = span;
  new_tup->shift = LWB (new_tup) * new_tup->span;
/* Allocate space for the big new row. */
  ARRAY (new_arr) = heap_generator (p, row_mode, elems_in_stack * span * new_arr->elem_size);
  if (span > 0) {
/* Copy 'elems_in_stack' rows into the new one. */
    int j;
    BYTE_T *new_elem = ADDRESS (&(ARRAY (new_arr)));
    for (j = 0; j < elems_in_stack; j++) {
/* new [j, , ] := old [, ] */
      BYTE_T *old_elem;
      BOOL_T done;
      GET_DESCRIPTOR (old_arr, old_tup, (A68_REF *) STACK_ADDRESS (sp + j * ALIGNED_SIZE_OF (A68_REF)));
      old_elem = ADDRESS (&(ARRAY (old_arr)));
      initialise_internal_index (old_tup, old_dim);
      initialise_internal_index (&new_tup[1], old_dim);
      done = A68_FALSE;
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
\param p position in tree
\param elem_mode mode of element
\param elems_in_stack number of elements in the stack
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_make_row (NODE_T * p, MOID_T * elem_mode, int elems_in_stack, ADDR_T sp)
{
  A68_REF new_row, new_arr;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int k;
  new_row = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  new_arr = heap_generator (p, MOID (p), elems_in_stack * MOID_SIZE (elem_mode));
  PROTECT_SWEEP_HANDLE (&new_arr);
  GET_DESCRIPTOR (arr, tup, &new_row);
  DIM (arr) = 1;
  MOID (arr) = elem_mode;
  arr->elem_size = MOID_SIZE (elem_mode);
  arr->slice_offset = 0;
  arr->field_offset = 0;
  ARRAY (arr) = new_arr;
  LWB (tup) = 1;
  UPB (tup) = elems_in_stack;
  tup->span = 1;
  tup->shift = LWB (tup);
  for (k = 0; k < elems_in_stack; k++) {
    ADDR_T offset = k * arr->elem_size;
    BYTE_T *src_a, *dst_a;
    A68_REF dst = new_arr, src;
    dst.offset += offset;
    STATUS (&src) = INITIALISED_MASK | IN_STACK_MASK;
    src.offset = sp + offset;
    REF_HANDLE (&src) = &nil_handle;
    dst_a = ADDRESS (&dst);
    src_a = ADDRESS (&src);
    if (elem_mode->has_rows) {
      if (WHETHER (elem_mode, STRUCT_SYMBOL)) {
        A68_REF new_one = genie_copy_stowed (src, p, elem_mode);
        MOVE (dst_a, ADDRESS (&new_one), MOID_SIZE (elem_mode));
      } else if (WHETHER (elem_mode, FLEX_SYMBOL)
                 || elem_mode == MODE (STRING)) {
        *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, DEFLEX (elem_mode));
      } else if (WHETHER (elem_mode, ROW_SYMBOL)) {
        *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, elem_mode);
      } else if (WHETHER (elem_mode, UNION_SYMBOL)) {
        genie_copy_union (p, dst_a, src_a, src);
      } else if (elem_mode == MODE (SOUND)) {
        genie_copy_sound (p, dst_a, src_a);
      } else {
        ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_make_row");
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
\param p position in tree
\param dst_mode destination mode
\param src_mode source mode
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_make_ref_row_of_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  A68_REF new_row, name, array;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  array = *(A68_REF *) STACK_ADDRESS (sp);
/* ROWING NIL yields NIL. */
  if (IS_NIL (array)) {
    return (nil_ref);
  }
  new_row = heap_generator (p, SUB (dst_mode), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  name = heap_generator (p, dst_mode, ALIGNED_SIZE_OF (A68_REF));
  GET_DESCRIPTOR (arr, tup, &new_row);
  DIM (arr) = 1;
  MOID (arr) = src_mode;
  arr->elem_size = MOID_SIZE (src_mode);
  arr->slice_offset = 0;
  arr->field_offset = 0;
  ARRAY (arr) = array;
  LWB (tup) = 1;
  UPB (tup) = 1;
  tup->span = 1;
  tup->shift = LWB (tup);
  *(A68_REF *) ADDRESS (&name) = new_row;
  UNPROTECT_SWEEP_HANDLE (&new_row);
  return (name);
}

/*!
\brief make REF [1 : 1, ..] MODE from REF [..] MODE
\param p position in tree
\param dst_mode destination mode
\param src_mode source mode
\param sp stack pointer
\return fat pointer to descriptor
**/

A68_REF genie_make_ref_row_row (NODE_T * p, MOID_T * dst_mode, MOID_T * src_mode, ADDR_T sp)
{
  A68_REF name, new_row, old_row;
  A68_ARRAY *new_arr, *old_arr;
  A68_TUPLE *new_tup, *old_tup;
  int k;
  dst_mode = DEFLEX (dst_mode);
  src_mode = DEFLEX (src_mode);
  name = *(A68_REF *) STACK_ADDRESS (sp);
/* ROWING NIL yields NIL. */
  if (IS_NIL (name)) {
    return (nil_ref);
  }
  old_row = *(A68_REF *) ADDRESS (&name);
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
/* Make new descriptor. */
  new_row = heap_generator (p, dst_mode, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (SUB (dst_mode)) * ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&new_row);
  name = heap_generator (p, dst_mode, ALIGNED_SIZE_OF (A68_REF));
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (SUB (dst_mode));
  MOID (new_arr) = MOID (old_arr);
  new_arr->elem_size = old_arr->elem_size;
  new_arr->slice_offset = 0;
  new_arr->field_offset = 0;
  ARRAY (new_arr) = ARRAY (old_arr);
/* Fill out the descriptor. */
  new_tup[0].lower_bound = 1;
  new_tup[0].upper_bound = 1;
  new_tup[0].span = 1;
  new_tup[0].shift = new_tup[0].lower_bound;
  for (k = 0; k < DIM (SUB (src_mode)); k++) {
    new_tup[k + 1] = old_tup[k];
  }
/* Yield the new name. */
  *(A68_REF *) ADDRESS (&name) = new_row;
  UNPROTECT_SWEEP_HANDLE (&new_row);
  return (name);
}

/*!
\brief coercion to [1 : 1, ] MODE
\param p position in tree
\return propagator for this action
**/

PROPAGATOR_T genie_rowing_row_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  row = genie_concatenate_rows (p, MOID (p), 1, sp);
  stack_pointer = sp;
  PUSH_REF (p, row);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief coercion to [1 : 1] [] MODE
\param p position in tree
\return propagator for this action
**/

PROPAGATOR_T genie_rowing_row_of_row (NODE_T * p)
{
  A68_REF row;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  row = genie_make_row (p, SLICE (MOID (p)), 1, sp);
  stack_pointer = sp;
  PUSH_REF (p, row);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief coercion to REF [1 : 1, ..] MODE
\param p position in tree
\return propagator for this action
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
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief REF [1 : 1] [] MODE from [] MODE
\param p position in tree
\return propagator for this action
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
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief rowing coercion
\param p position in tree
\return propagator for this action
**/

PROPAGATOR_T genie_rowing (NODE_T * p)
{
  PROPAGATOR_T self;
  if (WHETHER (MOID (p), REF_SYMBOL)) {
/* REF ROW, decide whether we want A->[] A or [] A->[,] A. */
    MOID_T *mode = SUB (MOID (p));
    if (DIM (DEFLEX (mode)) >= 2) {
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
    if (DIM (DEFLEX (MOID (p))) >= 2) {
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
\brief copy a united object holding stowed
\param p position in tree
\param dst_a destination
\param src_a source
\param struct_field pointer to structured field
**/

static void genie_copy_union (NODE_T * p, BYTE_T * dst_a, BYTE_T * src_a, A68_REF struct_field)
{
  BYTE_T *dst_u = &(dst_a[UNION_OFFSET]), *src_u = &(src_a[UNION_OFFSET]);
  A68_UNION *u = (A68_UNION *) src_a;
  MOID_T *um = (MOID_T *) VALUE (u);
  if (um != NULL) {
    *(A68_UNION *) dst_a = *u;
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
\brief make copy array of mode 'm' from 'old'
\param old_row fat pointer to descriptor of old row
\param p position in tree
\param m mode of row
\return fat pointer to descriptor
**/

A68_REF genie_copy_row (A68_REF old_row, NODE_T * p, MOID_T * m)
{
/* We need this complex routine ARRAY(&since)s are not always contiguous (trims). */
  A68_REF new_row;
  A68_ARRAY *old_arr, *new_arr;
  A68_TUPLE *old_tup, *new_tup, *old_p, *new_p;
  int k, span;
  UP_SWEEP_SEMA;
  if (IS_NIL (old_row)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Cut FLEX from the mode. That is not interesting in this routine. */
  if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
    m = SUB (m);
  }
/* Make ARRAY(&new). */
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (old_arr) * ALIGNED_SIZE_OF (A68_TUPLE));
/* Get descriptor again in case the heap sweeper moved data (but switched off now) */
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  GET_DESCRIPTOR (new_arr, new_tup, &new_row);
  DIM (new_arr) = DIM (old_arr);
  MOID (new_arr) = MOID (old_arr);
  new_arr->elem_size = old_arr->elem_size;
  new_arr->slice_offset = 0;
  new_arr->field_offset = 0;
/* Get size and copy bounds; no checks since this routine just makes a copy. */
  span = 1;
  for (k = 0; k < DIM (old_arr); k++) {
    old_p = &old_tup[k];
    new_p = &new_tup[k];
    LWB (new_p) = LWB (old_p);
    UPB (new_p) = UPB (old_p);
    new_p->span = span;
    new_p->shift = LWB (new_p) * new_p->span;
    span *= ROW_SIZE (new_p);
  }
  ARRAY (new_arr) = heap_generator (p, MOID (p), span * new_arr->elem_size);
/* The n-dimensional copier. */
  if (span > 0) {
    unsigned elem_size = (unsigned) moid_size (MOID (old_arr));
    MOID_T *elem_mode = SUB (m);
    BYTE_T *old_elem = ADDRESS (&(ARRAY (old_arr)));
    BYTE_T *new_elem = ADDRESS (&(ARRAY (new_arr)));
    BOOL_T done = A68_FALSE;
    initialise_internal_index (old_tup, DIM (old_arr));
    initialise_internal_index (new_tup, DIM (new_arr));
    while (!done) {
      ADDR_T old_index, new_index, old_addr, new_addr;
      old_index = calculate_internal_index (old_tup, DIM (old_arr));
      new_index = calculate_internal_index (new_tup, DIM (new_arr));
      old_addr = ROW_ELEMENT (old_arr, old_index);
      new_addr = ROW_ELEMENT (new_arr, new_index);
      if (elem_mode->has_rows) {
/* Recursion. */
        A68_REF new_old = ARRAY (old_arr), new_dst = ARRAY (new_arr);
        BYTE_T *src_a, *dst_a;
        new_old.offset += old_addr;
        new_dst.offset += new_addr;
        src_a = ADDRESS (&new_old);
        dst_a = ADDRESS (&new_dst);
        if (WHETHER (elem_mode, STRUCT_SYMBOL)) {
          A68_REF str_src = genie_copy_stowed (new_old, p, elem_mode);
          MOVE (dst_a, ADDRESS (&str_src), MOID_SIZE (elem_mode));
        } else if (WHETHER (elem_mode, FLEX_SYMBOL)
                   || elem_mode == MODE (STRING)) {
          *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, DEFLEX (elem_mode));
        } else if (WHETHER (elem_mode, ROW_SYMBOL)) {
          *(A68_REF *) dst_a = genie_copy_stowed (*(A68_REF *) src_a, p, elem_mode);
        } else if (WHETHER (elem_mode, UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, new_old);
        } else if (elem_mode == MODE (SOUND)) {
          genie_copy_sound (p, dst_a, src_a);
        } else {
          ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_row");
        }
      } else {
        MOVE (&new_elem[new_addr], &old_elem[old_addr], elem_size);
      }
/* Increase pointers. */
      done = increment_internal_index (old_tup, DIM (old_arr)) | increment_internal_index (new_tup, DIM (new_arr));
    }
  }
  DOWN_SWEEP_SEMA;
  return (new_row);
}

/*!
\brief make copy of stowed value at 'old'
\param old pointer to old object
\param p position in tree
\param m mode of object
\return fat pointer to descriptor or structured value
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
        } else if (WHETHER (MOID (fields), FLEX_SYMBOL)
                   || MOID (fields) == MODE (STRING)) {
          *(A68_REF *) dst_a = genie_copy_row (*(A68_REF *) src_a, p, MOID (fields));
        } else if (WHETHER (MOID (fields), ROW_SYMBOL)) {
          *(A68_REF *) dst_a = genie_copy_row (*(A68_REF *) src_a, p, MOID (fields));
        } else if (WHETHER (MOID (fields), UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, old_field);
        } else if (MOID (fields) == MODE (SOUND)) {
          genie_copy_sound (p, dst_a, src_a);
        } else {
          ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_stowed");
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
    ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_copy_stowed");
    return (nil_ref);
  }
}

/*!
\brief assign array of MODE 'm' from 'old_row' to 'dst'
\param old_row pointer to old object
\param dst destination, may be overwritten
\param p position in tree
\param m mode of row
\return fat pointer to descriptor
**/

static A68_REF genie_assign_row (A68_REF old_row, A68_REF * dst, NODE_T * p, MOID_T * m)
{
  A68_REF new_row;
  A68_ARRAY *old_arr, *new_arr = NULL;
  A68_TUPLE *old_tup, *new_tup = NULL, *old_p, *new_p;
  int k, span = 0;
  STATUS (&new_row) = INITIALISED_MASK;
  new_row.offset = 0;
/* Get row desriptors. Switch off GC so data is not moved. */
  UP_SWEEP_SEMA;
  GET_DESCRIPTOR (old_arr, old_tup, &old_row);
  if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
/* In case of FLEX rows we make a new descriptor. */
    m = SUB (m);
    new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + DIM (old_arr) * ALIGNED_SIZE_OF (A68_TUPLE));
    GET_DESCRIPTOR (new_arr, new_tup, &new_row);
    DIM (new_arr) = DIM (old_arr);
    MOID (new_arr) = MOID (old_arr);
    new_arr->elem_size = old_arr->elem_size;
    new_arr->slice_offset = 0;
    new_arr->field_offset = 0;
    for (k = 0, span = 1; k < DIM (old_arr); k++) {
      old_p = &old_tup[k];
      new_p = &new_tup[k];
      LWB (new_p) = LWB (old_p);
      UPB (new_p) = UPB (old_p);
      new_p->span = span;
      new_p->shift = LWB (new_p) * new_p->span;
      span *= ROW_SIZE (new_p);
    }
    ARRAY (new_arr) = heap_generator (p, m, span * new_arr->elem_size);
  } else if (WHETHER (m, ROW_SYMBOL)) {
/* In case of non-FLEX rows we check on equal length. */
    new_row = *dst;
    GET_DESCRIPTOR (new_arr, new_tup, &new_row);
    for (k = 0, span = 1; k < DIM (old_arr); k++) {
      old_p = &old_tup[k];
      new_p = &new_tup[k];
      if ((UPB (new_p) != UPB (old_p)) || (LWB (new_p) != LWB (old_p))) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIFFERENT_BOUNDS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      span *= ROW_SIZE (old_p);
    }
  } else {
    ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_row");
  }
/* The n-dimensional copier. */
  initialise_internal_index (old_tup, DIM (old_arr));
  initialise_internal_index (new_tup, DIM (new_arr));
  if (span > 0) {
    unsigned elem_size = (unsigned) moid_size (MOID (old_arr));
    MOID_T *elem_mode = SUB (m);
    BYTE_T *old_elem = ADDRESS (&(ARRAY (old_arr))), *new_elem = ADDRESS (&(ARRAY (new_arr)));
    BOOL_T done = A68_FALSE;
    while (!done) {
      ADDR_T old_index, new_index, old_addr, new_addr;
      old_index = calculate_internal_index (old_tup, DIM (old_arr));
      new_index = calculate_internal_index (new_tup, DIM (new_arr));
      old_addr = ROW_ELEMENT (old_arr, old_index);
      new_addr = ROW_ELEMENT (new_arr, new_index);
      if (elem_mode->has_rows) {
/* Recursion. */
        A68_REF new_old = ARRAY (old_arr), new_dst = ARRAY (new_arr);
        BYTE_T *src_a, *dst_a;
        new_old.offset += old_addr;
        new_dst.offset += new_addr;
        src_a = ADDRESS (&new_old);
        dst_a = ADDRESS (&new_dst);
        if (WHETHER (elem_mode, STRUCT_SYMBOL)) {
          genie_assign_stowed (new_old, &new_dst, p, elem_mode);
        } else if (WHETHER (elem_mode, FLEX_SYMBOL) || elem_mode == MODE (STRING)) {
/* Algol68G does not know ghost elements. NIL means an initially empty row. */
          A68_REF dst_addr = *(A68_REF *) dst_a;
          if (IS_NIL (dst_addr)) {
            *(A68_REF *) dst_a = *(A68_REF *) src_a;
          } else {
            *(A68_REF *) dst_a = genie_assign_stowed (*(A68_REF *) src_a, &dst_addr, p, elem_mode);
          }
        } else if (WHETHER (elem_mode, ROW_SYMBOL)) {
/* Algol68G does not know ghost elements. NIL means an initially empty row. */
          A68_REF dst_addr = *(A68_REF *) dst_a;
          if (IS_NIL (dst_addr)) {
            *(A68_REF *) dst_a = *(A68_REF *) src_a;
          } else {
            genie_assign_stowed (*(A68_REF *) src_a, &dst_addr, p, elem_mode);
          }
        } else if (WHETHER (elem_mode, UNION_SYMBOL)) {
          genie_copy_union (p, dst_a, src_a, new_old);
        } else if (elem_mode == MODE (SOUND)) {
          genie_copy_sound (p, dst_a, src_a);
        } else {
          ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_row");
        }
      } else {
        MOVE (&new_elem[new_addr], &old_elem[old_addr], elem_size);
      }
/* Increase pointers. */
      done = increment_internal_index (old_tup, DIM (old_arr)) | increment_internal_index (new_tup, DIM (new_arr));
    }
  }
  DOWN_SWEEP_SEMA;
  return (new_row);
}

/*!
\brief assign multiple value of mode 'm' from 'old' to 'dst'
\param old pointer to old value
\param dst pointer to destination
\param p position in tree
\param m mode of object
\return fat pointer to descriptor or structured value
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
        } else if (WHETHER (MOID (fields), FLEX_SYMBOL)
                   || MOID (fields) == MODE (STRING)) {
/* STRUCT (FLEX [] A ..) */
          *(A68_REF *) dst_a = genie_copy_row (*(A68_REF *) src_a, p, MOID (fields));
        } else if (WHETHER (MOID (fields), ROW_SYMBOL)) {
/* STRUCT ([] A ..) */
          A68_REF arr_src = *(A68_REF *) src_a;
          A68_REF arr_dst = *(A68_REF *) dst_a;
          genie_assign_row (arr_src, &arr_dst, p, MOID (fields));
        } else if (WHETHER (MOID (fields), UNION_SYMBOL)) {
/* UNION (..) */
          genie_copy_union (p, dst_a, src_a, old_field);
        } else if (MOID (fields) == MODE (SOUND)) {
          genie_copy_sound (p, dst_a, src_a);
        } else {
          ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_stowed");
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
    ABNORMAL_END (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "genie_assign_stowed");
    return (nil_ref);
  }
}

/*!
\brief dump a stowed object for debugging purposes
\param f file to dump to
\param w pointer to struct, row or union
\param m mode of object that p points to
\param level recursion level
**/

#define INDENT(n) {\
  int j;\
  WRITE (f, "\n");\
  for (j = 0; j < n; j++) {\
    WRITE (f, " ");\
  }}

void dump_stowed (NODE_T * p, FILE_T f, void *w, MOID_T * m, int level)
{
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BOOL_T done = A68_FALSE;
  int span, k;
  char buf[BUFFER_SIZE];
  INDENT (level);
  snprintf (buf, BUFFER_SIZE, "%s at %p pointing at %p", moid_to_string (m, 80, NULL), w, (void *) ADDRESS ((A68_ROW *) w));
  WRITE (f, buf);
  if (IS_NIL (*(A68_REF *) w)) {
    INDENT (level);
    snprintf (buf, BUFFER_SIZE, "NIL - returning");
    WRITE (f, buf);
    return;
  }
  if (WHETHER (m, STRUCT_SYMBOL)) {
    PACK_T *fields;
    for (fields = PACK (m); fields != NULL; fields = NEXT (fields)) {
      if (MOID (fields)->has_rows) {
        dump_stowed (p, f, &((BYTE_T *) w)[fields->offset], MOID (fields), level + 1);
      } else {
        INDENT (level);
        snprintf (buf, BUFFER_SIZE, "%s %s at %p", moid_to_string (MOID (fields), 80, NULL), fields->text, &((BYTE_T *) w)[fields->offset]);
        WRITE (f, buf);
      }
    }
  } else if (WHETHER (m, UNION_SYMBOL)) {
    A68_UNION *u = (A68_UNION *) w;
    MOID_T *um = (MOID_T *) VALUE (u);
    if (um != NULL) {
      if (um->has_rows) {
        dump_stowed (p, f, &((BYTE_T *) w)[UNION_OFFSET], um, level + 1);
      } else {
        (void) snprintf (buf, BUFFER_SIZE, " holds %s at %p", moid_to_string (um, 80, NULL), &((BYTE_T *) w)[UNION_OFFSET]);
        WRITE (f, buf);
      }
    }
  } else {
    if (WHETHER (m, FLEX_SYMBOL) || m == MODE (STRING)) {
      m = SUB (m);
    }
    GET_DESCRIPTOR (arr, tup, (A68_ROW *) w);
    for (k = 0, span = 1; k < DIM (arr); k++) {
      A68_TUPLE *z = &tup[k];
      INDENT (level);
      snprintf (buf, BUFFER_SIZE, "tuple %d has lwb=%d and upb=%d", k, LWB (z), UPB (z));
      WRITE (f, buf);
      span *= ROW_SIZE (z);
    }
    INDENT (level);
    snprintf (buf, BUFFER_SIZE, "elems=%d, elem size=%d, slice_offset=%d, field_offset=%d", span, arr->elem_size, arr->slice_offset, arr->field_offset);
    WRITE (f, buf);
    if (span > 0) {
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        A68_REF elem = ARRAY (arr);
        BYTE_T *elem_p;
        MOID_T *elem_mode = SUB (m);
        ADDR_T index = calculate_internal_index (tup, DIM (arr));
        ADDR_T addr = ROW_ELEMENT (arr, index);
        elem.offset += addr;
        elem_p = ADDRESS (&elem);
        if (elem_mode->has_rows) {
          dump_stowed (p, f, elem_p, elem_mode, level + 3);
        } else {
          INDENT (level);
          snprintf (buf, BUFFER_SIZE, "%s [%d] at %p", moid_to_string (elem_mode, 80, NULL), index, elem_p);
          WRITE (f, buf);
          print_item (p, f, elem_p, elem_mode);
        }
/* Increase pointers. */
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
}

#undef INDENT

/* Operators for ROWS. */

/*!
\brief OP ELEMS = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, get_row_size (t, DIM (x)), A68_INT);
}

/*!
\brief OP LWB = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, LWB (t), A68_INT);
}

/*!
\brief OP UPB = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, UPB (t), A68_INT);
}

/*!
\brief OP ELEMS = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t, *u;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  u = &(t[VALUE (&k) - 1]);
  PUSH_PRIMITIVE (p, ROW_SIZE (u), A68_INT);
}

/*!
\brief OP LWB = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, t[VALUE (&k) - 1].lower_bound, A68_INT);
}

/*!
\brief OP UPB = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack. */
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, t[VALUE (&k) - 1].upper_bound, A68_INT);
}

/*!
\brief push description for diagonal of square matrix
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_diagonal_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROPAGATOR_T self;
  A68_ROW row, new_row;
  int k = 0;
  BOOL_T name = WHETHER (MOID (p), REF_SYMBOL);
  A68_ARRAY *arr, new_arr;
  A68_TUPLE *tup1, *tup2, new_tup;
  MOID_T *m;
  UP_SWEEP_SEMA;
  if (WHETHER (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB (MOID (NEXT (q))) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    A68_PRINT_REF ("implicit deference", &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  if (ROW_SIZE (tup1) != ROW_SIZE (tup2)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_NO_SQUARE_MATRIX, m, NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (ABS (k) >= ROW_SIZE (tup1)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS, NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB (MOID (p)) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 1;
  MOID (&new_arr) = m;
  new_arr.elem_size = arr->elem_size;
  new_arr.slice_offset = arr->slice_offset;
  new_arr.field_offset = arr->field_offset;
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&new_tup) = 1;
  UPB (&new_tup) = ROW_SIZE (tup1) - ABS (k);
  new_tup.shift = tup1->shift + tup2->shift - k * tup2->span;
  if (k < 0) {
    new_tup.shift -= (-k) * (tup1->span + tup2->span);
  }
  new_tup.span = tup1->span + tup2->span;
  new_tup.k = 0;
  PUT_DESCRIPTOR (new_arr, new_tup, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&ref_new) = new_row;
    SET_REF_SCOPE (&ref_new, scope);
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  DOWN_SWEEP_SEMA;
  self.unit = genie_diagonal_function;
  self.source = p;
  return (self);
}

/*!
\brief push description for transpose of square matrix
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_transpose_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROPAGATOR_T self;
  A68_ROW row, new_row;
  BOOL_T name = WHETHER (MOID (p), REF_SYMBOL);
  A68_ARRAY *arr, new_arr;
  A68_TUPLE *tup1, *tup2, new_tup1, new_tup2;
  MOID_T *m;
  UP_SWEEP_SEMA;
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB (MOID (NEXT (q))) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    A68_PRINT_REF ("implicit deference", &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR2 (arr, tup1, tup2, &row);
  if (ROW_SIZE (tup1) != ROW_SIZE (tup2)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_NO_SQUARE_MATRIX, m, NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + 2 * ALIGNED_SIZE_OF (A68_TUPLE));
  new_arr = *arr;
  new_tup1 = *tup2;
  new_tup2 = *tup1;
  PUT_DESCRIPTOR2 (new_arr, new_tup1, new_tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&ref_new) = new_row;
    SET_REF_SCOPE (&ref_new, scope);
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  DOWN_SWEEP_SEMA;
  self.unit = genie_transpose_function;
  self.source = p;
  return (self);
}

/*!
\brief push description for row vector
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_row_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROPAGATOR_T self;
  A68_ROW row, new_row;
  int k = 1;
  BOOL_T name = WHETHER (MOID (p), REF_SYMBOL);
  A68_ARRAY *arr, new_arr;
  A68_TUPLE tup1, tup2, *tup;
  MOID_T *m;
  UP_SWEEP_SEMA;
  if (WHETHER (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB (MOID (NEXT (q))) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    A68_PRINT_REF ("implicit deference", &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  if (DIM (arr) != 1) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_NO_VECTOR, m, PRIMARY, NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  m = (name ? SUB (MOID (p)) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 2;
  MOID (&new_arr) = m;
  new_arr.elem_size = arr->elem_size;
  new_arr.slice_offset = arr->slice_offset;
  new_arr.field_offset = arr->field_offset;
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&tup1) = k;
  UPB (&tup1) = k;
  tup1.span = 1;
  tup1.shift = k * tup1.span;
  tup1.k = 0;
  LWB (&tup2) = 1;
  UPB (&tup2) = ROW_SIZE (tup);
  tup2.span = tup->span;
  tup2.shift = tup->span;
  tup2.k = 0;
  PUT_DESCRIPTOR2 (new_arr, tup1, tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&ref_new) = new_row;
    SET_REF_SCOPE (&ref_new, scope);
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  DOWN_SWEEP_SEMA;
  self.unit = genie_row_function;
  self.source = p;
  return (self);
}


/*!
\brief push description for column vector
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_column_function (NODE_T * p)
{
  NODE_T *q = SUB (p);
  ADDR_T scope = PRIMAL_SCOPE;
  PROPAGATOR_T self;
  A68_ROW row, new_row;
  int k = 1;
  BOOL_T name = WHETHER (MOID (p), REF_SYMBOL);
  A68_ARRAY *arr, new_arr;
  A68_TUPLE tup1, tup2, *tup;
  MOID_T *m;
  UP_SWEEP_SEMA;
  if (WHETHER (q, TERTIARY)) {
    A68_INT x;
    EXECUTE_UNIT (q);
    POP_OBJECT (p, &x, A68_INT);
    k = VALUE (&x);
    FORWARD (q);
  }
  EXECUTE_UNIT (NEXT (q));
  m = (name ? SUB (MOID (NEXT (q))) : MOID (NEXT (q)));
  if (name) {
    A68_REF z;
    POP_REF (p, &z);
    A68_PRINT_REF ("implicit deference", &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  POP_OBJECT (p, &row, A68_ROW);
  GET_DESCRIPTOR (arr, tup, &row);
  m = (name ? SUB (MOID (p)) : MOID (p));
  new_row = heap_generator (p, m, ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  DIM (&new_arr) = 2;
  MOID (&new_arr) = m;
  new_arr.elem_size = arr->elem_size;
  new_arr.slice_offset = arr->slice_offset;
  new_arr.field_offset = arr->field_offset;
  ARRAY (&new_arr) = ARRAY (arr);
  LWB (&tup1) = 1;
  UPB (&tup1) = ROW_SIZE (tup);
  tup1.span = tup->span;
  tup1.shift = tup->span;
  tup1.k = 0;
  LWB (&tup2) = k;
  UPB (&tup2) = k;
  tup2.span = 1;
  tup2.shift = k * tup2.span;
  tup2.k = 0;
  PUT_DESCRIPTOR2 (new_arr, tup1, tup2, &new_row);
  if (name) {
    A68_REF ref_new = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&ref_new) = new_row;
    SET_REF_SCOPE (&ref_new, scope);
    PUSH_REF (p, ref_new);
  } else {
    PUSH_OBJECT (p, new_row, A68_ROW);
  }
  DOWN_SWEEP_SEMA;
  self.unit = genie_column_function;
  self.source = p;
  return (self);
}

/*!
\brief strcmp for qsort
\param a string
\param b string
\return a - b
**/

int qstrcmp (const void *a, const void *b)
{
  return (strcmp (*(char *const *) a, *(char *const *) b));
}

/*!
\brief sort row of string
\param p position in tree
**/

void genie_sort_row_string (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  ADDR_T pop_sp;
  int size;
  POP_REF (p, &z);
  pop_sp = stack_pointer;
  CHECK_REF (p, z, MODE (ROW_STRING));
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  if (size > 0) {
    A68_REF row, *base_ref;
    A68_ARRAY arrn;
    A68_TUPLE tupn;
    int j, k;
    BYTE_T *base = ADDRESS (&ARRAY (arr));
    char **ptrs = (char **) malloc (size * sizeof (char *));
    if (ptrs == NULL) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* Copy C-strings into the stack and sort. */
    for (j = 0, k = LWB (tup); k <= UPB (tup); j++, k++) {
      int addr = INDEX_1_DIM (arr, tup, k);
      A68_REF ref = *(A68_REF *) & (base[addr]);
      int len;
      CHECK_REF (p, ref, MODE (STRING));
      len = A68_ALIGN (a68_string_size (p, ref) + 1);
      if (stack_pointer + len > expr_stack_limit) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      ptrs[j] = (char *) STACK_TOP;
      a_to_c_string (p, (char *) STACK_TOP, ref);
      INCREMENT_STACK_POINTER (p, len);
    }
    qsort (ptrs, size, sizeof (char *), qstrcmp);
/* Construct an array of sorted strings. */
    z = heap_generator (p, MODE (ROW_STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_STRING), size * MOID_SIZE (MODE (STRING)));
    DIM (&arrn) = 1;
    MOID (&arrn) = MODE (STRING);
    arrn.elem_size = MOID_SIZE (MODE (STRING));
    arrn.slice_offset = 0;
    arrn.field_offset = 0;
    ARRAY (&arrn) = row;
    LWB (&tupn) = 1;
    UPB (&tupn) = size;
    tupn.shift = LWB (&tupn);
    tupn.span = 1;
    tupn.k = 0;
    PUT_DESCRIPTOR (arrn, tupn, &z);
    base_ref = (A68_REF *) ADDRESS (&row);
    for (k = 0; k < size; k++) {
      base_ref[k] = c_to_a_string (p, ptrs[k]);
    }
    free (ptrs);
    stack_pointer = pop_sp;
    PUSH_REF (p, z);
  } else {
/* This is how we sort an empty row of strings ... */
    stack_pointer = pop_sp;
    PUSH_REF (p, empty_row (p, MODE (ROW_STRING)));
  }
}
