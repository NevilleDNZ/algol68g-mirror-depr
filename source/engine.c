/*!
\file engine.c
\brief routines executing primitive A68 actions
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

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

/*
This file contains interpreter ("genie") routines related to executing primitive
A68 actions.

The genie is self-optimising as when it traverses the tree, it stores terminals
it ends up in at the root where traversing for that terminal started.
Such piece of information is called a PROPAGATOR.
*/

void genie_unit_trace (NODE_T *);
void genie_serial_units_no_label (NODE_T *, int, NODE_T **);

PROPAGATOR_T genie_voiding_assignation (NODE_T * p);

static PROPAGATOR_T global_prop;

/*
Since Algol 68 can pass procedures as parameters, we use static links rather
than a display. Static link access to non-local variables is more elaborate than
display access, but you don't have to copy the display on every call, which is
expensive in terms of time and stack space. Early versions of Algol68G did use a
display, but speed improvement was negligible and the code was less transparent.
So it was reverted to static links.
*/

/*!
\brief initialise PROC and OP identities
\param p starting node of a declaration
\param seq
\param count number of constants initialised
**/

static void genie_init_proc_op (NODE_T * p, NODE_T ** seq, int *count)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case OP_SYMBOL:
    case PROC_SYMBOL:
    case OPERATOR_PLAN:
    case DECLARER:
      {
        break;
      }
    case DEFINING_IDENTIFIER:
    case DEFINING_OPERATOR:
      {
/* Store position so we need not search again. */
        NODE_T *save = *seq;
        (*seq) = p;
        (*seq)->inits = save;
        (*count)++;
        return;
      }
    default:
      {
        genie_init_proc_op (SUB (p), seq, count);
        break;
      }
    }
  }
}

/*!
\brief initialise PROC and OP identity declarations
\param p position in the syntax tree, should not be NULL
\param count number of constants initialised
**/

static void genie_find_proc_op (NODE_T * p, int *count)
{
  for (; p != NULL; FORWARD (p)) {
    if (p->genie.whether_new_lexical_level) {
/* Don't enter a new lexical level - it will have its own initialisation. */
      return;
    } else if (WHETHER (p, PROCEDURE_DECLARATION) || WHETHER (p, BRIEF_OPERATOR_DECLARATION)) {
      genie_init_proc_op (SUB (p), &(SYMBOL_TABLE (p)->inits), count);
      return;
    } else {
      genie_find_proc_op (SUB (p), count);
    }
  }
}

/*!
\brief driver for initialising PROCs, OPs and FORMATs
\param p position in the syntax tree, should not be NULL
**/

void initialise_frame (NODE_T * p)
{
/* Initialise routines and formats - link to environment in the frame stack. */
  if (SYMBOL_TABLE (p)->initialise_anon) {
    TAG_T *a;
    SYMBOL_TABLE (p)->initialise_anon = A_FALSE;
    for (a = SYMBOL_TABLE (p)->anonymous; a != NULL; a = NEXT (a)) {
      switch (PRIO (a)) {
      case FORMAT_TEXT:
        {
/* Initialise format text. */
          int youngest = TAX (NODE (a))->youngest_environ;
          A68_FORMAT *z = (A68_FORMAT *) (FRAME_OFFSET (FRAME_INFO_SIZE + a->offset));
          z->status = INITIALISED_MASK;
          z->body = NODE (a);
          if (youngest > 0) {
            STATIC_LINK_FOR_FRAME (z->environ, 1 + youngest);
          } else {
            z->environ = 0;
          }
          SYMBOL_TABLE (p)->initialise_anon = A_TRUE;
          break;
        }
      case ROUTINE_TEXT:
        {
/* Initialise routine text. */
          int youngest = TAX (NODE (a))->youngest_environ;
          A68_PROCEDURE *z = (A68_PROCEDURE *) (FRAME_OFFSET (FRAME_INFO_SIZE + a->offset));
          z->status = INITIALISED_MASK;
          z->body = (void *) NODE (a);
          if (youngest > 0) {
            STATIC_LINK_FOR_FRAME (z->environ, 1 + youngest);
          } else {
            z->environ = 0;
          }
          z->locale = nil_ref;
          z->proc_mode = MOID (a);
          SYMBOL_TABLE (p)->initialise_anon = A_TRUE;
          break;
        }
      }
    }
  }
/* Initialise PROC and OP identities in this level, for mutual recursion. */
  if (SYMBOL_TABLE (p)->proc_ops) {
    NODE_T *q;
    ADDR_T pop_sp;
    if (SYMBOL_TABLE (p)->inits == NULL) {
/* Walk tree and set up sequence. */
      int count = 0;
      genie_find_proc_op (p, &count);
      SYMBOL_TABLE (p)->proc_ops = (count > 0);
    }
/* Walk sequence. */
    pop_sp = stack_pointer;
    for (q = SYMBOL_TABLE (p)->inits; q != NULL; q = q->inits) {
      NODE_T *u = NEXT (NEXT (q));
      if (WHETHER (u, ROUTINE_TEXT) || (WHETHER (u, UNIT) && WHETHER (SUB (u), ROUTINE_TEXT))) {
        EXECUTE_UNIT (u);
        stack_pointer = pop_sp;
        *(A68_PROCEDURE *) FRAME_OFFSET (FRAME_INFO_SIZE + TAX (q)->offset) = *(A68_PROCEDURE *) STACK_TOP;
      }
    }
  }
/* Record whether we did anything. */
  SYMBOL_TABLE (p)->initialise_frame = (SYMBOL_TABLE (p)->initialise_anon || SYMBOL_TABLE (p)->proc_ops);
}

#define SCOPE_CHECK(p, scope, limit, mode, info)\
  if (scope > limit) {\
    char txt[BUFFER_SIZE];\
    if (info == NULL) {\
      snprintf (txt, BUFFER_SIZE, ERROR_SCOPE_DYNAMIC_1);\
    } else {\
      snprintf (txt, BUFFER_SIZE, ERROR_SCOPE_DYNAMIC_2, info);\
    }\
    diagnostic_node (A_RUNTIME_ERROR, p, txt, mode);\
    exit_genie (p, A_RUNTIME_ERROR);\
  }

/*!
\brief dynamic scope check
\param p position in the syntax tree, should not be NULL
\param m
\param w
\param limit
\param info
**/

void genie_dns_addr (NODE_T * p, MOID_T * m, BYTE_T * w, ADDR_T limit, char *info)
{
  if (m != NULL && w != NULL) {
    ADDR_T _limit_2 = (limit < global_pointer ? global_pointer : limit);
    if (WHETHER (m, REF_SYMBOL)) {
      SCOPE_CHECK (p, ((A68_REF *) w)->scope, _limit_2, m, info);
    } else if (WHETHER (m, UNION_SYMBOL)) {
      genie_dns_addr (p, (MOID_T *) ((A68_UNION *) w)->value, &(w[SIZE_OF (A68_UNION)]), _limit_2, "united value");
    } else if (WHETHER (m, PROC_SYMBOL)) {
      A68_PROCEDURE *v = (A68_PROCEDURE *) w;
      SCOPE_CHECK (p, v->environ, _limit_2, m, info);
      if (!IS_NIL (v->locale)) {
        BYTE_T *u = ADDRESS (&(v->locale));
        PACK_T *s = PACK (v->proc_mode);
        for (; s != NULL; FORWARD (s)) {
          if (((A68_BOOL *) & u[0])->value == A_TRUE) {
            genie_dns_addr (p, MOID (s), &u[SIZE_OF (A68_BOOL)], _limit_2, "partial parameter value");
          }
          u = &(u[SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
        }
      }
    } else if (WHETHER (m, FORMAT_SYMBOL)) {
      SCOPE_CHECK (p, ((A68_FORMAT *) w)->environ, _limit_2, m, info);
    }
  }
}

#define GENIE_DNS_STACK(p, m, limit, info)\
  genie_dns_addr (p, m, STACK_OFFSET (-MOID_SIZE (m)), limit, info);

#undef SCOPE_CHECK

/*!
\brief whether item at "w" of mode "q" is initialised
\param p position in the syntax tree, should not be NULL
\param w
\param q
**/

void genie_check_initialisation (NODE_T * p, BYTE_T * w, MOID_T * q)
{
  switch (q->short_id) {
  case REF_SYMBOL:
    {
      A68_REF *z = (A68_REF *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case PROC_SYMBOL:
    {
      A68_PROCEDURE *z = (A68_PROCEDURE *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_INT:
    {
      A68_INT *z = (A68_INT *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_REAL:
    {
      A68_REAL *z = (A68_REAL *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_COMPLEX:
    {
      A68_REAL *r = (A68_REAL *) w;
      A68_REAL *i = (A68_REAL *) (w + SIZE_OF (A68_REAL));
      CHECK_INIT (p, r->status & INITIALISED_MASK, q);
      CHECK_INIT (p, i->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_LONG_INT:
  case MODE_LONGLONG_INT:
  case MODE_LONG_REAL:
  case MODE_LONGLONG_REAL:
  case MODE_LONG_BITS:
  case MODE_LONGLONG_BITS:
    {
      MP_DIGIT_T *z = (MP_DIGIT_T *) w;
      CHECK_INIT (p, (int) z[0] & INITIALISED_MASK, q);
      return;
    }
  case MODE_LONG_COMPLEX:
    {
      MP_DIGIT_T *r = (MP_DIGIT_T *) w;
      MP_DIGIT_T *i = (MP_DIGIT_T *) (w + size_long_mp ());
      CHECK_INIT (p, (int) r[0] & INITIALISED_MASK, q);
      CHECK_INIT (p, (int) i[0] & INITIALISED_MASK, q);
      return;
    }
  case MODE_LONGLONG_COMPLEX:
    {
      MP_DIGIT_T *r = (MP_DIGIT_T *) w;
      MP_DIGIT_T *i = (MP_DIGIT_T *) (w + size_longlong_mp ());
      CHECK_INIT (p, (int) r[0] & INITIALISED_MASK, q);
      CHECK_INIT (p, (int) i[0] & INITIALISED_MASK, q);
      return;
    }
  case MODE_BOOL:
    {
      A68_BOOL *z = (A68_BOOL *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_CHAR:
    {
      A68_CHAR *z = (A68_CHAR *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_BITS:
    {
      A68_BITS *z = (A68_BITS *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_BYTES:
    {
      A68_BYTES *z = (A68_BYTES *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_LONG_BYTES:
    {
      A68_LONG_BYTES *z = (A68_LONG_BYTES *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_FILE:
    {
      A68_FILE *z = (A68_FILE *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_FORMAT:
    {
      A68_FORMAT *z = (A68_FORMAT *) w;
      CHECK_INIT (p, z->status & INITIALISED_MASK, q);
      return;
    }
  case MODE_PIPE:
    {
      A68_REF *read = (A68_REF *) w;
      A68_REF *write = (A68_REF *) (w + SIZE_OF (A68_REF));
      A68_INT *pid = (A68_INT *) (w + 2 * SIZE_OF (A68_REF));
      CHECK_INIT (p, read->status & INITIALISED_MASK, q);
      CHECK_INIT (p, write->status & INITIALISED_MASK, q);
      CHECK_INIT (p, pid->status & INITIALISED_MASK, q);
      return;
    }
  }
}

/*!
\brief push constant stored in the tree
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_constant (NODE_T * p)
{
  PUSH (p, p->genie.constant, MOID_SIZE (MOID (p)));
  return (p->genie.propagator);
}

/*!
\brief push constant stored in the tree
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_constant_int (NODE_T * p)
{
  *(A68_INT *) STACK_TOP = *(A68_INT *) (p->genie.constant);
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_INT));
  return (p->genie.propagator);
}

/*!
\brief push constant stored in the tree
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_constant_real (NODE_T * p)
{
  *(A68_REAL *) STACK_TOP = *(A68_REAL *) (p->genie.constant);
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_REAL));
  return (p->genie.propagator);
}

/*!
\brief unite value in the stack and push result
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_uniting (NODE_T * p)
{
  PROPAGATOR_T self;
  ADDR_T sp = stack_pointer;
  MOID_T *u = MOID (p), *v = MOID (SUB (p));
  int size = MOID_SIZE (u);
  self.unit = genie_uniting;
  self.source = p;
  if (ATTRIBUTE (v) != UNION_SYMBOL) {
    PUSH_POINTER (p, (void *) unites_to (v, u));
    EXECUTE_UNIT (SUB (p));
  } else {
    A68_UNION *m = (A68_UNION *) STACK_TOP;
    EXECUTE_UNIT (SUB (p));
    m->value = (void *) unites_to ((MOID_T *) m->value, u);
  }
  stack_pointer = sp + size;
  return (self);
}

/*!
\brief store widened constant as a constant
\param p position in the syntax tree, should not be NULL
\param m
\param self
**/

static void make_constant_widening (NODE_T * p, MOID_T * m, PROPAGATOR_T * self)
{
  if (SUB (p) != NULL && SUB (p)->genie.constant != NULL) {
    int size = MOID_SIZE (m);
    if (m == MODE (REAL)) {
      self->unit = genie_constant_real;
    } else {
      self->unit = genie_constant;
    }
    p->genie.constant = (void *) get_heap_space ((unsigned) size);
    COPY (p->genie.constant, (void *) (STACK_OFFSET (-size)), (unsigned) size);
  }
}

/*!
\brief (optimised) push INT widened to REAL
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_widening_int_to_real (NODE_T * p)
{
  A68_INT *i = (A68_INT *) STACK_TOP;
  A68_REAL *z = (A68_REAL *) STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_REAL) - SIZE_OF (A68_INT));
  z->value = (double) i->value;
  z->status = INITIALISED_MASK;
  return (p->genie.propagator);
}

/*!
\brief widen value in the stack
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_widening (NODE_T * p)
{
#define COERCE_FROM_TO(p, a, b) (MOID (p) == (b) && MOID (SUB (p)) == (a))
  PROPAGATOR_T self;
  self.unit = genie_widening;
  self.source = p;
/* INT widenings. */
  if (COERCE_FROM_TO (p, MODE (INT), MODE (REAL))) {
    genie_widening_int_to_real (p);
    self.unit = genie_widening_int_to_real;
    make_constant_widening (p, MODE (REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (INT), MODE (LONG_INT))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_int_to_long_mp (p);
    make_constant_widening (p, MODE (LONG_INT), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_INT), MODE (LONGLONG_INT))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_mp_to_longlong_mp (p);
    make_constant_widening (p, MODE (LONGLONG_INT), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_INT), MODE (LONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
/* 1-1 mapping. */
    make_constant_widening (p, MODE (LONG_REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONGLONG_INT), MODE (LONGLONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
/* 1-1 mapping. */
    make_constant_widening (p, MODE (LONGLONG_REAL), &self);
  }
/* REAL widenings. */
  else if (COERCE_FROM_TO (p, MODE (REAL), MODE (LONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_real_to_long_mp (p);
    make_constant_widening (p, MODE (LONG_REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_REAL), MODE (LONGLONG_REAL))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_mp_to_longlong_mp (p);
    make_constant_widening (p, MODE (LONGLONG_REAL), &self);
  } else if (COERCE_FROM_TO (p, MODE (REAL), MODE (COMPLEX))) {
    EXECUTE_UNIT (SUB (p));
    PUSH_REAL (p, 0.0);
    make_constant_widening (p, MODE (COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_REAL), MODE (LONG_COMPLEX))) {
    MP_DIGIT_T *z;
    int digits = get_mp_digits (MODE (LONG_REAL));
    EXECUTE_UNIT (SUB (p));
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = INITIALISED_MASK;
    make_constant_widening (p, MODE (LONG_COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX))) {
    MP_DIGIT_T *z;
    int digits = get_mp_digits (MODE (LONGLONG_REAL));
    EXECUTE_UNIT (SUB (p));
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = INITIALISED_MASK;
    make_constant_widening (p, MODE (LONGLONG_COMPLEX), &self);
  }
/* COMPLEX widenings. */
  else if (COERCE_FROM_TO (p, MODE (COMPLEX), MODE (LONG_COMPLEX))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_complex_to_long_complex (p);
    make_constant_widening (p, MODE (LONG_COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_COMPLEX), MODE (LONGLONG_COMPLEX))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_complex_to_longlong_complex (p);
    make_constant_widening (p, MODE (LONGLONG_COMPLEX), &self);
  }
/* BITS widenings. */
  else if (COERCE_FROM_TO (p, MODE (BITS), MODE (LONG_BITS))) {
    EXECUTE_UNIT (SUB (p));
/* Treat unsigned as int, but that's ok. */
    genie_lengthen_int_to_long_mp (p);
    make_constant_widening (p, MODE (LONG_BITS), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_BITS), MODE (LONGLONG_BITS))) {
    EXECUTE_UNIT (SUB (p));
    genie_lengthen_long_mp_to_longlong_mp (p);
    make_constant_widening (p, MODE (LONGLONG_BITS), &self);
  }
/* Miscellaneous widenings. */
  else if (COERCE_FROM_TO (p, MODE (BYTES), MODE (ROW_CHAR))) {
    A68_BYTES z;
    EXECUTE_UNIT (SUB (p));
    POP (p, &z, SIZE_OF (A68_BYTES));
    PUSH_REF (p, c_string_to_row_char (p, z.value, BYTES_WIDTH));
  } else if (COERCE_FROM_TO (p, MODE (LONG_BYTES), MODE (ROW_CHAR))) {
    A68_LONG_BYTES z;
    EXECUTE_UNIT (SUB (p));
    POP (p, &z, SIZE_OF (A68_LONG_BYTES));
    PUSH_REF (p, c_string_to_row_char (p, z.value, LONG_BYTES_WIDTH));
  } else if (COERCE_FROM_TO (p, MODE (BITS), MODE (ROW_BOOL))) {
    A68_BITS x;
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int k;
    unsigned bit;
    A68_BOOL *base;
    EXECUTE_UNIT (SUB (p));
    POP (p, &x, SIZE_OF (A68_BITS));
    z = heap_generator (p, MODE (ROW_BOOL), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_BOOL), BITS_WIDTH * SIZE_OF (A68_BOOL));
    PROTECT_SWEEP_HANDLE (&row);
    arr.dimensions = 1;
    arr.type = MODE (BOOL);
    arr.elem_size = SIZE_OF (A68_BOOL);
    arr.slice_offset = 0;
    arr.field_offset = 0;
    arr.array = row;
    tup.lower_bound = 1;
    tup.upper_bound = BITS_WIDTH;
    tup.shift = tup.lower_bound;
    tup.span = 1;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = (A68_BOOL *) ADDRESS (&row);
    bit = 1;
    for (k = BITS_WIDTH - 1; k >= 0; k--) {
      A68_BOOL b;
      b.status = INITIALISED_MASK;
      b.value = (x.value & bit) != 0 ? A_TRUE : A_FALSE;
      base[k] = b;
      bit <<= 1;
    }
    if (SUB (p)->genie.constant != NULL) {
      self.unit = genie_constant;
      PROTECT_SWEEP_HANDLE (&z);
      p->genie.constant = (void *) get_heap_space (SIZE_OF (A68_REF));
      COPY (p->genie.constant, &z, SIZE_OF (A68_REF));
    } else {
      UNPROTECT_SWEEP_HANDLE (&z);
    }
    PUSH_REF (p, z);
    UNPROTECT_SWEEP_HANDLE (&row);
  } else if (COERCE_FROM_TO (p, MODE (LONG_BITS), MODE (ROW_BOOL)) || COERCE_FROM_TO (p, MODE (LONGLONG_BITS), MODE (ROW_BOOL))) {
    MOID_T *m = MOID (SUB (p));
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int size = get_mp_size (m), k, width = get_mp_bits_width (m), words = get_mp_bits_words (m);
    unsigned *bits;
    A68_BOOL *base;
    MP_DIGIT_T *x;
    ADDR_T pop_sp = stack_pointer;
/* Calculate and convert BITS value. */
    EXECUTE_UNIT (SUB (p));
    x = (MP_DIGIT_T *) STACK_OFFSET (-size);
    bits = stack_mp_bits (p, x, m);
/* Make [] BOOL. */
    z = heap_generator (p, MODE (ROW_BOOL), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_BOOL), width * SIZE_OF (A68_BOOL));
    PROTECT_SWEEP_HANDLE (&row);
    arr.dimensions = 1;
    arr.type = MODE (BOOL);
    arr.elem_size = SIZE_OF (A68_BOOL);
    arr.slice_offset = 0;
    arr.field_offset = 0;
    arr.array = row;
    tup.lower_bound = 1;
    tup.upper_bound = width;
    tup.shift = tup.lower_bound;
    tup.span = 1;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = (A68_BOOL *) ADDRESS (&row);
    k = width;
    while (k > 0) {
      unsigned bit = 0x1;
      int j;
      for (j = 0; j < MP_BITS_BITS && k >= 0; j++) {
        A68_BOOL b;
        b.status = INITIALISED_MASK;
        b.value = (bits[words - 1] & bit) ? A_TRUE : A_FALSE;
        base[--k] = b;
        bit <<= 1;
      }
      words--;
    }
    if (SUB (p)->genie.constant != NULL) {
      self.unit = genie_constant;
      PROTECT_SWEEP_HANDLE (&z);
      p->genie.constant = (void *) get_heap_space (SIZE_OF (A68_REF));
      COPY (p->genie.constant, &z, SIZE_OF (A68_REF));
    } else {
      UNPROTECT_SWEEP_HANDLE (&z);
    }
    stack_pointer = pop_sp;
    PUSH_REF (p, z);
    UNPROTECT_SWEEP_HANDLE (&row);
  } else {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CANNOT_WIDEN, MOID (SUB (p)), MOID (p));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  return (self);
#undef COERCE_FROM_TO
}

/*!
\brief cast a jump to a PROC VOID without executing the jump
\param p position in the syntax tree, should not be NULL
**/

static void genie_proceduring (NODE_T * p)
{
  A68_PROCEDURE z;
  NODE_T *jump = SUB (p);
  NODE_T *q = SUB (jump);
  NODE_T *label = (WHETHER (q, GOTO_SYMBOL) ? NEXT (q) : q);
  z.status = INITIALISED_MASK;
  z.body = (void *) jump;
  STATIC_LINK_FOR_FRAME (z.environ, 1 + LEX_LEVEL (TAX (label)));
  z.locale = nil_ref;
  z.proc_mode = MODE (PROC_VOID);
  PUSH (p, &z, SIZE_OF (A68_PROCEDURE));
}

/*!
\brief (optimised) dereference value of a unit
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_dereferencing_quick (NODE_T * p)
{
  A68_REF z;
  unsigned size = MOID_SIZE (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  stack_pointer = pop_sp;
  z = *(A68_REF *) STACK_TOP;
  TEST_NIL (p, z, MOID (SUB (p)));
  PUSH (p, ADDRESS (&z), size);
  GENIE_CHECK_INITIALISATION (p, STACK_OFFSET (-size), MOID (p));
  return (p->genie.propagator);
}

/*!
\brief dereference a local name
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_loc_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB (MOID (p));
  unsigned size = MOID_SIZE (deref);
  FRAME_GET (z, A68_REF, p);
  TEST_NIL (p, *z, MOID (p));
  PUSH (p, ADDRESS (z), size);
  GENIE_CHECK_INITIALISATION (p, STACK_OFFSET (-size), deref);
  return (p->genie.propagator);
}

/*!
\brief slice REF [] A to A
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_slice_name_quick (NODE_T * p)
{
  NODE_T *q;
  A68_ARRAY *x;
  A68_TUPLE *t;
  int index, size;
  A68_REF *z, u;
  int *k;
  ADDR_T pop_sp = stack_pointer;
/* Get row - save row from sweeper. */
  z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  PROTECT_FROM_SWEEP (p);
/* Pop REF [], dereference and get descriptor. */
  stack_pointer = pop_sp;
  TEST_NIL (p, *z, MOID (SUB (p)));
  u = *(A68_REF *) ADDRESS (z);
  x = (A68_ARRAY *) ADDRESS (&u);
/* Get indexer. */
  t = (A68_TUPLE *) HEAP_ADDRESS (u.handle->offset + SIZE_OF (A68_ARRAY));
  k = &(((A68_INT *) STACK_TOP)->value);
  UP_SWEEP_SEMA;
  for (index = 0, q = SEQUENCE (p); q != NULL; t++, q = SEQUENCE (q)) {
    EXECUTE_UNIT (q);
    stack_pointer = pop_sp;
    if (*k < t->lower_bound || *k > t->upper_bound) {
      diagnostic_node (A_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A_RUNTIME_ERROR);
    }
    index += (t->span * (*k - t->shift));
  }
  DOWN_SWEEP_SEMA;
/* Push element. */
  size = MOID_SIZE (SUB (MOID (p)));
  u = x->array;
  u.offset += ROW_ELEMENT (x, index);
  PUSH (p, ADDRESS (&u), size);
  GENIE_CHECK_INITIALISATION (p, STACK_OFFSET (-size), SUB (MOID (p)));
  return (p->genie.propagator);
}

/*!
\brief dereference name in the stack
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_dereferencing (NODE_T * p)
{
  A68_REF z;
  PROPAGATOR_T self = EXECUTE_UNIT (SUB (p));
  POP (p, &z, SIZE_OF (A68_REF));
  TEST_NIL (p, z, MOID (SUB (p)));
  PUSH (p, ADDRESS (&z), MOID_SIZE (MOID (p)));
  GENIE_CHECK_INITIALISATION (p, STACK_OFFSET (-MOID_SIZE (MOID (p))), MOID (p));
  if (self.unit == genie_loc_identifier) {
    self.unit = genie_dereference_loc_identifier;
    self.source->genie.propagator.unit = self.unit;
  } else if (self.unit == genie_slice_name_quick) {
    self.unit = genie_dereference_slice_name_quick;
    self.source->genie.propagator.unit = self.unit;
  } else {
    self.unit = genie_dereferencing_quick;
    self.source = p;
  }
  return (self);
}

/*!
\brief deprocedure PROC in the stack
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_deproceduring (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_PROCEDURE z;
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  MOID_T *pr_mode = MOID (SUB (p));
  self.unit = genie_deproceduring;
  self.source = p;
  EXECUTE_UNIT (SUB (p));
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  GENIE_CHECK_INITIALISATION (p, (BYTE_T *) & z, pr_mode);
  genie_call_procedure (p, pr_mode, pr_mode, MODE (VOID), &z, pop_sp, pop_fp);
  PROTECT_FROM_SWEEP (p);
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "yield from deproceduring");
  return (self);
}

/*!
\brief voiden value in the stack
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_voiding (NODE_T * p)
{
  PROPAGATOR_T self, source;
  ADDR_T sp_for_voiding = stack_pointer;
  self.source = p;
  source = EXECUTE_UNIT (SUB (p));
  stack_pointer = sp_for_voiding;
  if (source.unit == genie_assignation) {
    self.unit = genie_voiding_assignation;
    self.source = source.source;
  } else {
    self.unit = genie_voiding;
  }
  return (self);
}

/*!
\brief coerce value in the stack
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_coercion (NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = genie_coercion;
  self.source = p;
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case VOIDING:
      {
        self = genie_voiding (p);
        break;
      }
    case UNITING:
      {
        self = genie_uniting (p);
        break;
      }
    case WIDENING:
      {
        self = genie_widening (p);
        break;
      }
    case ROWING:
      {
        self = genie_rowing (p);
        break;
      }
    case DEREFERENCING:
      {
        self = genie_dereferencing (p);
        break;
      }
    case DEPROCEDURING:
      {
        self = genie_deproceduring (p);
        break;
      }
    case PROCEDURING:
      {
        genie_proceduring (p);
        break;
      }
    }
  }
  p->genie.propagator = self;
  return (self);
}

/*!
\brief push argument units
\param p position in the syntax tree, should not be NULL
\param seq
**/

static void genie_argument (NODE_T * p, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      EXECUTE_UNIT (p);
      SEQUENCE (*seq) = p;
      (*seq) = p;
      return;
    } else if (WHETHER (p, TRIMMER)) {
      return;
    } else {
      genie_argument (SUB (p), seq);
    }
  }
}

/*!
\brief evaluate partial call
\param p position in the syntax tree, should not be NULL
\param pr_mode
\param pproc
\param pmap
\param z
\param pop_sp
\param pop_fp
**/

void genie_partial_call (NODE_T * p, MOID_T * pr_mode, MOID_T * pproc, MOID_T * pmap, A68_PROCEDURE z, ADDR_T pop_sp, ADDR_T pop_fp)
{
  int voids = 0;
  BYTE_T *u, *v;
  PACK_T *s, *t;
  A68_REF loc;
/* Get locale. */
  if (IS_NIL (z.locale)) {
    int size = 0;
    for (s = PACK (pr_mode); s != NULL; FORWARD (s)) {
      size += (SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s)));
    }
    loc = heap_generator (p, pr_mode, size);
  } else {
    int size = z.locale.handle->size;
    loc = heap_generator (p, pr_mode, size);
    COPY (ADDRESS (&loc), ADDRESS (&(z.locale)), size);
  }
/* Move arguments from stack to locale using pmap. */
  u = (BYTE_T *) ADDRESS (&loc);
  s = PACK (pr_mode);
  v = STACK_ADDRESS (pop_sp);
  t = PACK (pmap);
  for (; t != NULL && s != NULL; FORWARD (t)) {
/* Skip already initialised arguments. */
    while (u != NULL && ((A68_BOOL *) & u[0])->value) {
      u = &(u[SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      FORWARD (s);
    }
    if (u != NULL && MOID (t) == MODE (VOID)) {
/* Move to next field in locale. */
      voids++;
      u = &(u[SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      FORWARD (s);
    } else {
/* Move argument from stack to locale. */
      A68_BOOL w;
      w.status = INITIALISED_MASK;
      w.value = A_TRUE;
      *(A68_BOOL *) & u[0] = w;
      COPY (&(u[SIZE_OF (A68_BOOL)]), v, MOID_SIZE (MOID (t)));
      u = &(u[SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      v = &(v[MOID_SIZE (MOID (t))]);
      FORWARD (s);
    }
  }
  stack_pointer = pop_sp;
  z.locale = loc;
/* Is closure complete? */
  if (voids == 0) {
/* Closure is complete. Push locale onto the stack and call procedure body. */
    stack_pointer = pop_sp;
    u = (BYTE_T *) ADDRESS (&loc);
    v = STACK_ADDRESS (stack_pointer);
    s = PACK (pr_mode);
    for (; s != NULL; FORWARD (s)) {
      int size = MOID_SIZE (MOID (s));
      COPY (v, &u[SIZE_OF (A68_BOOL)], size);
      u = &(u[SIZE_OF (A68_BOOL) + size]);
      v = &(v[MOID_SIZE (MOID (s))]);
      INCREMENT_STACK_POINTER (p, size);
    }
    genie_call_procedure (p, pr_mode, pproc, MODE (VOID), &z, pop_sp, pop_fp);
  } else {
/*  Closure is not complete. Return procedure body. */
    PUSH (p, &z, SIZE_OF (A68_PROCEDURE));
  }
}

/*!
\brief closure and deproceduring of routines with PARAMSETY
\param p position in the syntax tree, should not be NULL
\param pr_mode
\param pproc
\param pmap
\param z
\param pop_sp
\param pop_fp
**/

void genie_call_procedure (NODE_T * p, MOID_T * pr_mode, MOID_T * pproc, MOID_T * pmap, A68_PROCEDURE * z, ADDR_T pop_sp, ADDR_T pop_fp)
{
  if (pmap != MODE (VOID) && pr_mode != pmap) {
    genie_partial_call (p, pr_mode, pproc, pmap, *z, pop_sp, pop_fp);
  } else if (z->status & STANDENV_PROCEDURE_MASK) {
    (void) ((GENIE_PROCEDURE *) z->body) (p);
  } else if (z->status & SKIP_PROCEDURE_MASK) {
    stack_pointer = pop_sp;
    genie_push_undefined (p, SUB ((MOID_T *) z->body));
  } else {
    NODE_T *body = (NODE_T *) z->body;
    if (WHETHER (body, ROUTINE_TEXT)) {
      NODE_T *entry = SUB (body);
      PACK_T *args = PACK (pr_mode);
      ADDR_T fp0 = 0;
/* Copy arguments from stack to frame. */
      OPEN_PROC_FRAME (entry, z->environ);
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
      for (; args != NULL; FORWARD (args)) {
        int size = MOID_SIZE (MOID (args));
        COPY ((FRAME_OFFSET (FRAME_INFO_SIZE + fp0)), STACK_ADDRESS (pop_sp + fp0), (unsigned) size);
        fp0 += size;
      }
      stack_pointer = pop_sp;
      p->genie.argsize = fp0;
/* Interpret routine text. */
      PREEMPTIVE_SWEEP;
      CHECK_TIME_LIMIT (p);
      if (DIMENSION (pr_mode) > 0) {
/* With PARAMETERS. */
        EXECUTE_UNIT (NEXT (NEXT (NEXT (entry))));
      } else {
/* Without PARAMETERS. */
        EXECUTE_UNIT (NEXT (NEXT (entry)));
      }
      CLOSE_FRAME;
      GENIE_DNS_STACK (p, SUB (pr_mode), frame_pointer, "yield from procedure");
    } else {
      OPEN_PROC_FRAME (body, z->environ);
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
      EXECUTE_UNIT (body);
      CLOSE_FRAME;
      GENIE_DNS_STACK (p, SUB (pr_mode), frame_pointer, "yield from procedure");
    }
  }
}

/*!
\brief call PROC with arguments and push result
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_call (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_PROCEDURE z;
  NODE_T *pr = SUB (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  self.unit = genie_call;
  self.source = p;
/* Get procedure. */
  EXECUTE_UNIT (pr);
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
/* Get arguments. */
  if (SEQUENCE (p) == NULL && SEQUENCE_SET (p) == A_FALSE) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    genie_argument (NEXT (pr), &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    SEQUENCE_SET (p) = A_TRUE;
  } else {
    NODE_T *q = SEQUENCE (p);
    for (; q != NULL; q = SEQUENCE (q)) {
      EXECUTE_UNIT (q);
    }
  }
  genie_call_procedure (p, z.proc_mode, pr->partial_proc, pr->partial_locale, &z, pop_sp, pop_fp);
  PROTECT_FROM_SWEEP (p);
  return (self);
}

/*!
\brief construct a descriptor "ref_new" for a trim of "ref_old"
\param p position in the syntax tree, should not be NULL
\param ref_new
\param ref_old
\param offset
**/

static void genie_trimmer (NODE_T * p, BYTE_T * *ref_new, BYTE_T * *ref_old, int *offset)
{
  if (p != NULL) {
    if (WHETHER (p, UNIT)) {
      A68_INT k;
      A68_TUPLE *t;
      EXECUTE_UNIT (p);
      POP_INT (p, &k);
      t = (A68_TUPLE *) * ref_old;
      if (k.value < t->lower_bound || k.value > t->upper_bound) {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
        exit_genie (p, A_RUNTIME_ERROR);
      }
      (*offset) += t->span * (k.value - t->shift);
      (*ref_old) += SIZE_OF (A68_TUPLE);
    } else if (WHETHER (p, TRIMMER)) {
      A68_INT k;
      A68_TUPLE *old_tup, *new_tup;
      NODE_T *q;
      int L, U, D;
      old_tup = (A68_TUPLE *) * ref_old;
      new_tup = (A68_TUPLE *) * ref_new;
/* TRIMMER is (l:u@r) with all units optional or (empty). */
      q = SUB (p);
      if (q == NULL) {
        L = old_tup->lower_bound;
        U = old_tup->upper_bound;
        D = 0;
      } else {
        BOOL_T absent = A_TRUE;
/* Lower index. */
        if (q != NULL && WHETHER (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_INT (p, &k);
          if (k.value < old_tup->lower_bound) {
            diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
            exit_genie (p, A_RUNTIME_ERROR);
          }
          L = k.value;
          FORWARD (q);
          absent = A_FALSE;
        } else {
          L = old_tup->lower_bound;
        }
        if (q != NULL && (WHETHER (q, COLON_SYMBOL) || WHETHER (q, DOTDOT_SYMBOL))) {
          FORWARD (q);
          absent = A_FALSE;
        }
/* Upper index. */
        if (q != NULL && WHETHER (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_INT (p, &k);
          if (k.value > old_tup->upper_bound) {
            diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
            exit_genie (p, A_RUNTIME_ERROR);
          }
          U = k.value;
          FORWARD (q);
          absent = 0;
        } else {
          U = old_tup->upper_bound;
        }
        if (q != NULL && WHETHER (q, AT_SYMBOL)) {
          FORWARD (q);
        }
/* Revised lower bound. */
        if (q != NULL && WHETHER (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_INT (p, &k);
          D = L - k.value;
          FORWARD (q);
        } else {
          D = absent ? 0 : L - 1;
        }
      }
      new_tup->lower_bound = L - D;
      new_tup->upper_bound = U - D;     /* (L - D) + (U - L) */
      new_tup->span = old_tup->span;
      new_tup->shift = old_tup->shift - D;
      (*ref_old) += SIZE_OF (A68_TUPLE);
      (*ref_new) += SIZE_OF (A68_TUPLE);
    } else {
      genie_trimmer (SUB (p), ref_new, ref_old, offset);
      genie_trimmer (NEXT (p), ref_new, ref_old, offset);
    }
  }
}

/*!
\brief calculation of subscript
\param p position in the syntax tree, should not be NULL
\param ref_heap
\param sum
\param seq
**/

void genie_subscript (NODE_T * p, ADDR_T * ref_heap, int *sum, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        A68_INT *k;
        A68_TUPLE *t;
        EXECUTE_UNIT (p);
        POP_ADDRESS (p, k, A68_INT);
        t = (A68_TUPLE *) HEAP_ADDRESS (*ref_heap);
        if (k->value < t->lower_bound || k->value > t->upper_bound) {
          diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
          exit_genie (p, A_RUNTIME_ERROR);
        }
        (*ref_heap) += SIZE_OF (A68_TUPLE);
        (*sum) += t->span * (k->value - t->shift);
        SEQUENCE (*seq) = p;
        (*seq) = p;
        return;
      }
    case GENERIC_ARGUMENT:
    case GENERIC_ARGUMENT_LIST:
      {
        genie_subscript (SUB (p), ref_heap, sum, seq);
      }
    }
  }
}

/*!
\brief slice REF [] A to REF A
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_slice_name_quick (NODE_T * p)
{
  NODE_T *q;
  A68_REF *z, u;
  A68_ARRAY *x;
  ADDR_T pop_sp, scope;
  A68_TUPLE *t;
  int index, *k;
/* Get row and save row from sweeper. */
  z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  PROTECT_FROM_SWEEP (p);
/* Pop REF [], dereference and get descriptor. */
  pop_sp = stack_pointer;
  TEST_NIL (p, *z, MOID (SUB (p)));
  u = *(A68_REF *) ADDRESS (z);
  x = (A68_ARRAY *) ADDRESS (&u);
/* Get indexer. */
  t = (A68_TUPLE *) HEAP_ADDRESS (u.handle->offset + SIZE_OF (A68_ARRAY));
  k = &(((A68_INT *) STACK_TOP)->value);
  UP_SWEEP_SEMA;
  for (index = 0, q = SEQUENCE (p); q != NULL; t++, q = SEQUENCE (q)) {
    EXECUTE_UNIT (q);
    stack_pointer = pop_sp;
    if (*k < t->lower_bound || *k > t->upper_bound) {
      diagnostic_node (A_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A_RUNTIME_ERROR);
    }
    index += (t->span * (*k - t->shift));
  }
  DOWN_SWEEP_SEMA;
/* Push reference to element, preserving scope. */
  scope = z->scope;
  *z = x->array;
  z->offset += ROW_ELEMENT (x, index);
  z->scope = scope;
  return (p->genie.propagator);
}

/*!
\brief push slice of a rowed object
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_slice (NODE_T * p)
{
  PROPAGATOR_T self, primary;
  ADDR_T scope = 0;
  BOOL_T slice_of_name = WHETHER (MOID (SUB (p)), REF_SYMBOL);
  MOID_T *result_moid = slice_of_name ? SUB (MOID (p)) : MOID (p);
  NODE_T *indexer = NEXT_SUB (p);
  self.unit = genie_slice;
  self.source = p;
/* Get row and save from sweeper. */
  primary = EXECUTE_UNIT (SUB (p));
  PROTECT_FROM_SWEEP (p);
/* In case of slicing a REF [], we need the [] internally, so dereference. */
  if (slice_of_name) {
    A68_REF z;
    POP (p, &z, SIZE_OF (A68_REF));
    TEST_NIL (p, z, MOID (SUB (p)));
    scope = z.scope;
    PUSH (p, ADDRESS (&z), SIZE_OF (A68_REF));
  }
/* SLICING subscripts one element from an array. */
  if (ANNOTATION (indexer) == SLICE) {
    A68_REF z;
    A68_ARRAY *x;
    ADDR_T ref_heap, address;
    int index;
/* Get descriptor. */
    POP (p, &z, SIZE_OF (A68_REF));
    TEST_NIL (p, z, MOID (SUB (p)));
    x = (A68_ARRAY *) ADDRESS (&z);
/* Get indexer. */
    ref_heap = z.handle->offset + SIZE_OF (A68_ARRAY);
    index = 0;
    UP_SWEEP_SEMA;
    if (SEQUENCE (p) == NULL && SEQUENCE_SET (p) == A_FALSE) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      genie_subscript (indexer, &ref_heap, &index, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      SEQUENCE_SET (p) = A_TRUE;
    } else {
      NODE_T *q;
      for (q = SEQUENCE (p); q != NULL; q = SEQUENCE (q)) {
        A68_TUPLE *t = (A68_TUPLE *) HEAP_ADDRESS (ref_heap);
        A68_INT *k;
        EXECUTE_UNIT (q);
        POP_ADDRESS (q, k, A68_INT);
        if (k->value < t->lower_bound || k->value > t->upper_bound) {
          diagnostic_node (A_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
          exit_genie (q, A_RUNTIME_ERROR);
        }
        ref_heap += SIZE_OF (A68_TUPLE);
        index += t->span * (k->value - t->shift);
      }
    }
    DOWN_SWEEP_SEMA;
/* Slice of a name yields a name. */
    address = ROW_ELEMENT (x, index);
    if (slice_of_name) {
      A68_REF name = x->array;
      name.offset += address;
      name.scope = scope;
      PUSH (p, &name, SIZE_OF (A68_REF));
      if (SEQUENCE_SET (p) == A_TRUE) {
        self.unit = genie_slice_name_quick;
        self.source = p;
      }
    } else {
      PUSH (p, ADDRESS (&(x->array)) + address, MOID_SIZE (result_moid));
    }
    return (self);
  }
/* Trimming selects a subarray from an array. */
  else if (ANNOTATION (indexer) == TRIMMER) {
    int new_size = SIZE_OF (A68_ARRAY) + DEFLEX (result_moid)->dimensions * SIZE_OF (A68_TUPLE);
    A68_REF ref_desc_copy = heap_generator (p, MOID (p), new_size);
    A68_ARRAY *old_des, *new_des;
    A68_REF z;
    BYTE_T *ref_new, *ref_old;
    int offset;
/* Get descriptor. */
    POP (p, &z, SIZE_OF (A68_REF));
/* Get indexer. */
    TEST_NIL (p, z, MOID (SUB (p)));
    old_des = (A68_ARRAY *) ADDRESS (&z);
    new_des = (A68_ARRAY *) ADDRESS (&ref_desc_copy);
    ref_old = ADDRESS (&z) + SIZE_OF (A68_ARRAY);
    ref_new = ADDRESS (&ref_desc_copy) + SIZE_OF (A68_ARRAY);
    new_des->dimensions = DEFLEX (result_moid)->dimensions;
    MOID (new_des) = MOID (old_des);
    new_des->elem_size = old_des->elem_size;
    offset = old_des->slice_offset;
    UP_SWEEP_SEMA;
    genie_trimmer (indexer, &ref_new, &ref_old, &offset);
    DOWN_SWEEP_SEMA;
    new_des->slice_offset = offset;
    new_des->field_offset = old_des->field_offset;
    new_des->array = old_des->array;
/* Trim of a name is a name. */
    if (slice_of_name) {
      A68_REF ref_new = heap_generator (p, MOID (p), SIZE_OF (A68_REF));
      *(A68_REF *) ADDRESS (&ref_new) = ref_desc_copy;
      ref_new.scope = scope;
      PUSH (p, &ref_new, SIZE_OF (A68_REF));
    } else {
      PUSH (p, &ref_desc_copy, SIZE_OF (A68_REF));
    }
    return (self);
  } else {
    return (self);
  }
}

/*!
\brief push value of denoter
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_denoter (NODE_T * p)
{
  static PROPAGATOR_T self;
  MOID_T *moid = MOID (p);
  self.unit = genie_denoter;
  self.source = p;
  if (moid == MODE (INT)) {
/* INT denoter. */
    A68_INT z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z) == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, moid);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    self.unit = genie_constant_int;
    z.status = INITIALISED_MASK | CONSTANT_MASK;
    p->genie.constant = (void *) get_heap_space (SIZE_OF (A68_INT));
    COPY (p->genie.constant, &z, (unsigned) SIZE_OF (A68_INT));
    PUSH_INT (p, ((A68_INT *) (p->genie.constant))->value);
  } else if (moid == MODE (REAL)) {
/* REAL denoter. */
    A68_REAL z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z) == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, moid);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    z.status = INITIALISED_MASK | CONSTANT_MASK;
    self.unit = genie_constant_real;
    p->genie.constant = (void *) get_heap_space (SIZE_OF (A68_REAL));
    COPY (p->genie.constant, &z, (unsigned) SIZE_OF (A68_REAL));
    PUSH_REAL (p, ((A68_REAL *) (p->genie.constant))->value);
  } else if (moid == MODE (LONG_INT) || moid == MODE (LONGLONG_INT)) {
/* [LONG] LONG INT denoter. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number = (WHETHER (SUB (p), INT_DENOTER) ? SUB (p) : NEXT_SUB (p));
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, moid);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    z[0] = INITIALISED_MASK | CONSTANT_MASK;
    self.unit = genie_constant;
    p->genie.constant = (void *) get_heap_space (size);
    COPY (p->genie.constant, z, (unsigned) size);
  } else if (moid == MODE (LONG_REAL) || moid == MODE (LONGLONG_REAL)) {
/* [LONG] LONG REAL denoter. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number = (WHETHER (SUB (p), REAL_DENOTER) ? SUB (p) : NEXT_SUB (p));
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, moid);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    z[0] = INITIALISED_MASK | CONSTANT_MASK;
    self.unit = genie_constant;
    p->genie.constant = (void *) get_heap_space (size);
    COPY (p->genie.constant, z, (unsigned) size);
  } else if (moid == MODE (BITS)) {
/* BITS denoter. */
    A68_BITS z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z) == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, moid);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    self.unit = genie_constant;
    z.status = INITIALISED_MASK | CONSTANT_MASK;
    p->genie.constant = (void *) get_heap_space (SIZE_OF (A68_BITS));
    COPY (p->genie.constant, &z, (unsigned) SIZE_OF (A68_BITS));
    PUSH_BITS (p, ((A68_BITS *) (p->genie.constant))->value);
  } else if (moid == MODE (LONG_BITS) || moid == MODE (LONGLONG_BITS)) {
/* [LONG] LONG BITS denoter. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number = (WHETHER (SUB (p), BITS_DENOTER) ? SUB (p) : NEXT_SUB (p));
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, moid);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    z[0] = INITIALISED_MASK | CONSTANT_MASK;
    self.unit = genie_constant;
    p->genie.constant = (void *) get_heap_space (size);
    COPY (p->genie.constant, z, (unsigned) size);
  } else if (moid == MODE (BOOL)) {
/* BOOL denoter. */
    A68_BOOL z;
    genie_string_to_value_internal (p, MODE (BOOL), SYMBOL (p), (BYTE_T *) & z);
    PUSH_BOOL (p, z.value);
  } else if (moid == MODE (CHAR)) {
/* CHAR denoter. */
    PUSH_CHAR (p, SYMBOL (p)[0]);
  } else if (moid == MODE (ROW_CHAR)) {
/* [] CHAR denoter. */
/* Make a permanent string in the heap. */
    A68_REF z;
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    z = c_to_a_string (p, SYMBOL (p));
    GET_DESCRIPTOR (arr, tup, &z);
    PROTECT_SWEEP_HANDLE (&z);
    PROTECT_SWEEP_HANDLE (&(arr->array));
    self.unit = genie_constant;
    p->genie.constant = (void *) get_heap_space (SIZE_OF (A68_REF));
    COPY (p->genie.constant, &z, (unsigned) SIZE_OF (A68_REF));
    PUSH_REF (p, *(A68_REF *) (p->genie.constant));
  } else if (moid == MODE (VOID)) {
/* VOID denoter: EMPTY. */
    ;
  }
  return (self);
}

/*!
\brief push a local identifier
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_loc_identifier (NODE_T * p)
{
  BYTE_T *z;
  FRAME_GET (z, BYTE_T, p);
  /*
   * GENIE_CHECK_INITIALISATION (p, z, MOID (p)); 
   */
  PUSH (p, z, MOID_SIZE (MOID (p)));
  return (p->genie.propagator);
}

/*!
\brief push standard environ routine as PROC
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier_standenv_proc (NODE_T * p)
{
  static A68_PROCEDURE z;
  TAG_T *q = TAX (p);
  z.status = (INITIALISED_MASK | STANDENV_PROCEDURE_MASK);
  z.body = (void *) q->procedure;
  z.environ = 0;
  z.locale = nil_ref;
  z.proc_mode = MOID (p);
  PUSH (p, &z, SIZE_OF (A68_PROCEDURE));
  return (p->genie.propagator);
}

/*!
\brief (optimised) push identifier from standard environ
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier_standenv (NODE_T * p)
{
  (void) (TAX (p)->procedure) (p);
  return (p->genie.propagator);
}

/*!
\brief push identifier onto the stack
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier (NODE_T * p)
{
  static PROPAGATOR_T self;
  TAG_T *q = TAX (p);
  self.source = p;
  if (q->stand_env_proc != 0) {
    if (WHETHER (MOID (q), PROC_SYMBOL)) {
      genie_identifier_standenv_proc (p);
      self.unit = genie_identifier_standenv_proc;
    } else {
      genie_identifier_standenv (p);
      self.unit = genie_identifier_standenv;
    }
  } else {
    genie_loc_identifier (p);
    self.unit = genie_loc_identifier;
  }
  return (self);
}

/*!
\brief push result of cast (coercions are deeper in the tree)
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_cast (NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = genie_cast;
  self.source = p;
  EXECUTE_UNIT (NEXT_SUB (p));
  return (self);
}

/*!
\brief execute assertion
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_assertion (NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = genie_assertion;
  self.source = p;
  if (MASK (p) & ASSERT_MASK) {
    A68_BOOL z;
    EXECUTE_UNIT (NEXT_SUB (p));
    POP_BOOL (p, &z);
    if (z.value == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FALSE_ASSERTION);
      exit_genie (p, A_RUNTIME_ERROR);
    }
  }
  return (self);
}

/*!
\brief push format text
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_format_text (NODE_T * p)
{
  static PROPAGATOR_T self;
  A68_FORMAT z = *(A68_FORMAT *) (FRAME_OFFSET (FRAME_INFO_SIZE + TAX (p)->offset));
  self.unit = genie_format_text;
  self.source = p;
  PUSH (p, &z, SIZE_OF (A68_FORMAT));
  return (self);
}

/*!
\brief SELECTION from a value
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_selection_value (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *result_mode = MOID (selector);
  ADDR_T old_stack_pointer = stack_pointer;
  int size = MOID_SIZE (result_mode);
  EXECUTE_UNIT (NEXT (selector));
  stack_pointer = old_stack_pointer;
  MOVE (STACK_TOP, STACK_OFFSET (PACK (SUB (selector))->offset), (unsigned) size);
  INCREMENT_STACK_POINTER (selector, size);
  PROTECT_FROM_SWEEP (p);
  return (p->genie.propagator);
}

/*!
\brief SELECTION from a name
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_selection_name (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  A68_REF *z;
  EXECUTE_UNIT (NEXT (selector));
  z = (A68_REF *) (STACK_OFFSET (-SIZE_OF (A68_REF)));
  TEST_NIL (selector, *z, struct_mode);
  z->offset += PACK (SUB (selector))->offset;
  PROTECT_FROM_SWEEP (p);
  return (p->genie.propagator);
}

/*!
\brief push selection from secondary
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_selection (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  PROPAGATOR_T self;
  MOID_T *struct_mode = MOID (NEXT (selector)), *result_mode = MOID (selector);
  BOOL_T selection_of_name = (WHETHER (struct_mode, REF_SYMBOL));
  self.source = p;
  EXECUTE_UNIT (NEXT (selector));
/* Multiple selections. */
  if (selection_of_name && (WHETHER (SUB (struct_mode), FLEX_SYMBOL) || WHETHER (SUB (struct_mode), ROW_SYMBOL))) {
    A68_REF *row1, row2, row3;
    int dims, desc_size;
    POP_ADDRESS (selector, row1, A68_REF);
    TEST_NIL (p, *row1, struct_mode);
    row1 = (A68_REF *) ADDRESS (row1);
    dims = DEFLEX (SUB (struct_mode))->dimensions;
    desc_size = SIZE_OF (A68_ARRAY) + dims * SIZE_OF (A68_TUPLE);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), (BYTE_T *) ADDRESS (row1), (unsigned) desc_size);
    MOID (((A68_ARRAY *) ADDRESS (&row2))) = SUB (SUB (result_mode));
    ((A68_ARRAY *) ADDRESS (&row2))->field_offset += PACK (SUB (selector))->offset;
    row3 = heap_generator (selector, result_mode, SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&row3) = row2;
    PUSH (selector, &row3, SIZE_OF (A68_REF));
    self.unit = genie_selection;
    PROTECT_FROM_SWEEP (p);
  } else if (struct_mode != NULL && (WHETHER (struct_mode, FLEX_SYMBOL) || WHETHER (struct_mode, ROW_SYMBOL))) {
    A68_REF *row1, row2;
    int dims, desc_size;
    POP_ADDRESS (selector, row1, A68_REF);
    dims = DEFLEX (struct_mode)->dimensions;
    desc_size = SIZE_OF (A68_ARRAY) + dims * SIZE_OF (A68_TUPLE);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), (BYTE_T *) ADDRESS (row1), (unsigned) desc_size);
    MOID (((A68_ARRAY *) ADDRESS (&row2))) = SUB (result_mode);
    ((A68_ARRAY *) ADDRESS (&row2))->field_offset += PACK (SUB (selector))->offset;
    PUSH (selector, &row2, SIZE_OF (A68_REF));
    self.unit = genie_selection;
    PROTECT_FROM_SWEEP (p);
  }
/* Normal selections. */
  else if (selection_of_name && WHETHER (SUB (struct_mode), STRUCT_SYMBOL)) {
    A68_REF *z = (A68_REF *) (STACK_OFFSET (-SIZE_OF (A68_REF)));
    TEST_NIL (selector, *z, struct_mode);
    z->offset += PACK (SUB (selector))->offset;
    self.unit = genie_selection_name;
    PROTECT_FROM_SWEEP (p);
  } else if (WHETHER (struct_mode, STRUCT_SYMBOL)) {
    DECREMENT_STACK_POINTER (selector, MOID_SIZE (struct_mode));
    MOVE (STACK_TOP, STACK_OFFSET (PACK (SUB (selector))->offset), (unsigned) MOID_SIZE (result_mode));
    INCREMENT_STACK_POINTER (selector, MOID_SIZE (result_mode));
    self.unit = genie_selection_value;
    PROTECT_FROM_SWEEP (p);
  }
  return (self);
}

/*!
\brief call operator
\param p position in the syntax tree, should not be NULL
\param p position in the syntax tree, should not be NULLop_sp
**/

void genie_call_operator (NODE_T * p, ADDR_T pop_sp)
{
  A68_PROCEDURE *z;
  ADDR_T pop_fp = frame_pointer;
  MOID_T *pr_mode = MOID (TAX (p));
  FRAME_GET (z, A68_PROCEDURE, p);
  genie_call_procedure (p, pr_mode, z->proc_mode, pr_mode, z, pop_sp, pop_fp);
}

/*!
\brief push result of monadic formula OP "u"
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_monadic (NODE_T * p)
{
  NODE_T *op = SUB (p);
  NODE_T *u = NEXT (op);
  PROPAGATOR_T self;
  ADDR_T sp = stack_pointer;
  self.unit = genie_monadic;
  self.source = p;
  EXECUTE_UNIT (u);
  if (TAX (op)->procedure != NULL) {
    (void) (TAX (op)->procedure) (op);
  } else {
    genie_call_operator (op, sp);
  }
  PROTECT_FROM_SWEEP (p);
  return (self);
}

/*!
\brief push result of formula
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_formula (NODE_T * p)
{
  PROPAGATOR_T self, lhs, rhs;
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  ADDR_T sp = stack_pointer;
  self.unit = genie_formula;
  self.source = p;
  lhs = EXECUTE_UNIT (u);
  if (op != NULL) {
    NODE_T *v = NEXT (op);
    rhs = EXECUTE_UNIT (v);
/* Operate on top of stack and try to optimise the formula. */
    if (TAX (op)->procedure != NULL) {
      (void) (TAX (op)->procedure) (op);
    } else {
      genie_call_operator (op, sp);
    }
    PROTECT_FROM_SWEEP (p);
    return (self);
  } else if (lhs.unit == genie_monadic) {
    return (lhs);
  }
  return (self);
}

/*!
\brief push NIL
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_nihil (NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = genie_nihil;
  self.source = p;
  PUSH_REF (p, nil_ref);
  return (self);
}

/*!
\brief copies union with stowed components on top of the stack
\param p position in the syntax tree, should not be NULL
**/

static void genie_copy_union (NODE_T * p)
{
  A68_POINTER *u = (A68_POINTER *) STACK_TOP;
  MOID_T *v = (MOID_T *) u->value;
  if (v != NULL) {
    unsigned v_size = MOID_SIZE (v);
    INCREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));
    if (WHETHER (v, STRUCT_SYMBOL)) {
      A68_REF old, new_one;
      old.status = INITIALISED_MASK;
      old.segment = stack_segment;
      old.offset = stack_pointer;
      old.handle = &nil_handle;
      new_one = genie_copy_stowed (old, p, v);
      MOVE (STACK_TOP, ADDRESS (&old), v_size);
    } else if (WHETHER (v, ROW_SYMBOL) || WHETHER (v, FLEX_SYMBOL)) {
      A68_REF new_one, old = *(A68_REF *) STACK_TOP;
      new_one = genie_copy_stowed (old, p, v);
      MOVE (STACK_TOP, &new_one, SIZE_OF (A68_REF));
    }
    DECREMENT_STACK_POINTER (p, SIZE_OF (A68_POINTER));
  }
}

/*!
\brief internal workings of an assignment of stowed objects
\param p position in the syntax tree, should not be NULL
\param z
\param source_moid
**/

static void genie_assign_internal (NODE_T * p, A68_REF * z, MOID_T * source_moid)
{
  if (WHETHER (source_moid, FLEX_SYMBOL) || source_moid == MODE (STRING)) {
/* Assign to FLEX [] AMODE. */
    A68_REF old_one = *(A68_REF *) STACK_TOP, new_one;
    new_one = genie_copy_stowed (old_one, p, source_moid);
    *(A68_REF *) ADDRESS (z) = new_one;
  } else if (WHETHER (source_moid, ROW_SYMBOL)) {
/* Assign to [] AMODE. */
    A68_REF old_one = *(A68_REF *) STACK_TOP, new_one;
    new_one = genie_copy_stowed (old_one, p, source_moid);
    genie_assign_stowed (new_one, (A68_REF *) ADDRESS (z), p, source_moid);
  } else if (WHETHER (source_moid, STRUCT_SYMBOL)) {
/* STRUCT with row. */
    A68_REF old_one, new_one;
    old_one.status = INITIALISED_MASK;
    old_one.segment = stack_segment;
    old_one.offset = stack_pointer;
    old_one.handle = &nil_handle;
    new_one = genie_copy_stowed (old_one, p, source_moid);
    genie_assign_stowed (new_one, z, p, source_moid);
  } else if (WHETHER (source_moid, UNION_SYMBOL)) {
/* UNION with roold_one. */
    genie_copy_union (p);
    COPY (ADDRESS (z), STACK_TOP, MOID_SIZE (source_moid));
  }
}

/*!
\brief assign a value to a name and voiden
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_voiding_assignation (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_REF *z = (A68_REF *) STACK_TOP;
  MOID_T *source_moid = SUB (MOID (p));
  int size = MOID_SIZE (source_moid);
  ADDR_T pop_sp = stack_pointer, pop_fp = FRAME_DYNAMIC_SCOPE (frame_pointer);
  EXECUTE_UNIT (SUB (p));
  TEST_NIL (p, *z, MOID (p));
  FRAME_DYNAMIC_SCOPE (frame_pointer) = z->scope;
  EXECUTE_UNIT (NEXT (NEXT (SUB (p))));
  GENIE_DNS_STACK (p, source_moid, z->scope, NULL);
  FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (source_moid->has_rows) {
    genie_assign_internal (p, z, source_moid);
  } else {
    COPY (ADDRESS (z), STACK_TOP, (unsigned) size);
  }
  stack_pointer = pop_sp;
  self.unit = genie_voiding_assignation;
  self.source = p;
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_assignation (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_REF *z = (A68_REF *) STACK_TOP;
  MOID_T *source_moid = SUB (MOID (p));
  int size = MOID_SIZE (source_moid);
  ADDR_T pop_fp = FRAME_DYNAMIC_SCOPE (frame_pointer);
  EXECUTE_UNIT (SUB (p));
  TEST_NIL (p, *z, MOID (p));
  FRAME_DYNAMIC_SCOPE (frame_pointer) = z->scope;
  EXECUTE_UNIT (NEXT (NEXT (SUB (p))));
  GENIE_DNS_STACK (p, source_moid, z->scope, NULL);
  FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (source_moid->has_rows) {
    genie_assign_internal (p, z, source_moid);
  } else {
    COPY (ADDRESS (z), STACK_TOP, (unsigned) size);
  }
  self.unit = genie_assignation;
  self.source = p;
  return (self);
}

/*!
\brief push equality of two REFs
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_identity_relation (NODE_T * p)
{
  PROPAGATOR_T self;
  NODE_T *lhs = SUB (p), *rhs = NEXT (NEXT (lhs));
  A68_REF x, y;
  self.unit = genie_identity_relation;
  self.source = p;
  EXECUTE_UNIT (lhs);
  POP (p, &y, SIZE_OF (A68_REF));
  EXECUTE_UNIT (rhs);
  POP (p, &x, SIZE_OF (A68_REF));
  if (WHETHER (NEXT_SUB (p), IS_SYMBOL)) {
    PUSH_BOOL (p, ADDRESS (&x) == ADDRESS (&y));
  } else {
    PUSH_BOOL (p, ADDRESS (&x) != ADDRESS (&y));
  }
  return (self);
}

/*!
\brief push result of ANDF
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_and_function (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_BOOL x;
  self.unit = genie_and_function;
  self.source = p;
  EXECUTE_UNIT (SUB (p));
  POP_BOOL (p, &x);
  if (x.value == A_TRUE) {
    EXECUTE_UNIT (NEXT (NEXT (SUB (p))));
  } else {
    PUSH_BOOL (p, A_FALSE);
  }
  return (self);
}

/*!
\brief push result of ORF
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_or_function (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_BOOL x;
  self.unit = genie_or_function;
  self.source = p;
  EXECUTE_UNIT (SUB (p));
  POP_BOOL (p, &x);
  if (x.value == A_FALSE) {
    EXECUTE_UNIT (NEXT (NEXT (SUB (p))));
  } else {
    PUSH_BOOL (p, A_TRUE);
  }
  return (self);
}

/*!
\brief push routine text
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_routine_text (NODE_T * p)
{
  static PROPAGATOR_T self;
  A68_PROCEDURE z = *(A68_PROCEDURE *) (FRAME_OFFSET (FRAME_INFO_SIZE + TAX (p)->offset));
  self.unit = genie_routine_text;
  self.source = p;
  PUSH (p, &z, SIZE_OF (A68_PROCEDURE));
  return (self);
}

/*!
\brief push an undefined value of the required mode
\param p position in the syntax tree, should not be NULL
\param u
**/

void genie_push_undefined (NODE_T * p, MOID_T * u)
{
/* For primitive modes we push an initialised value. */
  if (u == MODE (VOID)) {
    /*
     * skip. 
     */ ;
  } else if (u == MODE (INT)) {
    PUSH_INT (p, (int) (rng_53_bit () * MAX_INT));
  } else if (u == MODE (REAL)) {
    PUSH_REAL (p, rng_53_bit ());
  } else if (u == MODE (COMPLEX)) {
    PUSH_REAL (p, rng_53_bit ());
    PUSH_REAL (p, rng_53_bit ());
  } else if (u == MODE (BOOL)) {
    PUSH_BOOL (p, rng_53_bit () < 0.5);
  } else if (u == MODE (CHAR)) {
    PUSH_CHAR (p, (char) (32 + 96 * rng_53_bit ()));
  } else if (u == MODE (BITS)) {
    PUSH_BITS (p, (unsigned) (rng_53_bit () * MAX_UNT));
  } else if (u == MODE (BYTES)) {
    PUSH_BYTES (p, "SKIP");
  } else if (u == MODE (LONG_BYTES)) {
    PUSH_LONG_BYTES (p, "SKIP");
  } else if (u == MODE (STRING)) {
    PUSH_REF (p, empty_string (p));
  } else if (u == MODE (LONG_INT) || u == MODE (LONGLONG_INT)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = INITIALISED_MASK;
  } else if (u == MODE (LONG_REAL) || u == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = INITIALISED_MASK;
  } else if (u == MODE (LONG_BITS) || u == MODE (LONGLONG_BITS)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = INITIALISED_MASK;
  } else if (u == MODE (LONG_COMPLEX) || u == MODE (LONGLONG_COMPLEX)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = INITIALISED_MASK;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = INITIALISED_MASK;
  } else if (WHETHER (u, REF_SYMBOL)) {
/* All REFs are NIL. */
    PUSH_REF (p, nil_ref);
  } else if (WHETHER (u, ROW_SYMBOL) || WHETHER (u, FLEX_SYMBOL)) {
/* [] AMODE or FLEX [] AMODE. */
    PUSH_REF (p, empty_row (p, u));
  } else if (WHETHER (u, STRUCT_SYMBOL)) {
/* STRUCT. */
    PACK_T *v;
    for (v = PACK (u); v != NULL; v = NEXT (v)) {
      genie_push_undefined (p, MOID (v));
    }
  } else if (WHETHER (u, UNION_SYMBOL)) {
/* UNION. */
    ADDR_T sp = stack_pointer;
    PUSH_POINTER (p, MOID (PACK (u)));
    genie_push_undefined (p, MOID (PACK (u)));
    stack_pointer = sp + MOID_SIZE (u);
  } else if (WHETHER (u, PROC_SYMBOL)) {
/* PROC. */
    A68_PROCEDURE z;
    z.status = INITIALISED_MASK | SKIP_PROCEDURE_MASK;
    z.body = (void *) u;
    z.environ = 0;
    z.locale = nil_ref;
    z.proc_mode = u;
    PUSH (p, &z, SIZE_OF (A68_PROCEDURE));
  } else {
/* FORMAT etc. - what arbitrary FORMAT would mean anything at all? */
    BYTE_T *sp = STACK_TOP;
    INCREMENT_STACK_POINTER (p, MOID_SIZE (u));
    FILL (sp, 0x00, (unsigned) MOID_SIZE (u));
  }
}

/*!
\brief push an undefined value of the required mode
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_skip (NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = genie_skip;
  self.source = p;
  if (MOID (p) != MODE (VOID)) {
    genie_push_undefined (p, MOID (p));
  }
  return (self);
}

/*!
\brief jump to the serial clause where the label is at
\param p position in the syntax tree, should not be NULL
**/

static void genie_jump (NODE_T * p)
{
/* Stack pointer and frame pointer were saved at target serial clause. */
  NODE_T *jump = SUB (p);
  NODE_T *label = (WHETHER (jump, GOTO_SYMBOL)) ? NEXT (jump) : jump;
  ADDR_T f = frame_pointer;
  jmp_buf *jump_stat;
/* Find the stack frame this jump points to. */
  BOOL_T found = A_FALSE;
  while (f > 0 && !found) {
    found = (SYMBOL_TABLE (TAX (label)) == SYMBOL_TABLE (FRAME_TREE (f))) && FRAME_JUMP_STAT (f) != NULL;
    if (!found) {
      f = FRAME_STATIC_LINK (f);
    }
  }
/* Beam us up, Scotty! */
#ifdef HAVE_POSIX_THREADS
  if (PAR_LEVEL (p) > 0) {
    if (PAR_LEVEL (p) > PAR_LEVEL (FRAME_TREE (f)) && PAR_LEVEL (FRAME_TREE (f)) != 0) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_LABEL_IN_PAR_CLAUSE);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    zap_all_threads (p, FRAME_JUMP_STAT (f), label);
  } else {
    jump_stat = FRAME_JUMP_STAT (f);
    SYMBOL_TABLE (TAX (label))->jump_to = TAX (label)->unit;
    longjmp (*(jump_stat), 1);
  }
#else
  jump_stat = FRAME_JUMP_STAT (f);
  SYMBOL_TABLE (TAX (label))->jump_to = TAX (label)->unit;
  longjmp (*(jump_stat), 1);
#endif
}

/*!
\brief execute a unit, tertiary, secondary or primary
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_unit (NODE_T * p)
{
  if (p->genie.whether_coercion) {
    global_prop = genie_coercion (p);
  } else {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        global_prop = EXECUTE_UNIT (SUB (p));
        break;
      }
    case TERTIARY:
    case SECONDARY:
    case PRIMARY:
      {
        global_prop = genie_unit (SUB (p));
        break;
      }
/* Ex primary. */
    case ENCLOSED_CLAUSE:
      {
        global_prop = genie_enclosed ((volatile NODE_T *) p);
        break;
      }
    case IDENTIFIER:
      {
        global_prop = genie_identifier (p);
        break;
      }
    case CALL:
      {
        global_prop = genie_call (p);
        break;
      }
    case SLICE:
      {
        global_prop = genie_slice (p);
        break;
      }
    case DENOTER:
      {
        global_prop = genie_denoter (p);
        break;
      }
    case CAST:
      {
        global_prop = genie_cast (p);
        break;
      }
    case FORMAT_TEXT:
      {
        global_prop = genie_format_text (p);
        break;
      }
/* Ex secondary. */
    case GENERATOR:
      {
        global_prop = genie_generator (p);
        break;
      }
    case SELECTION:
      {
        global_prop = genie_selection (p);
        break;
      }
/* Ex tertiary. */
    case FORMULA:
      {
        global_prop = genie_formula (p);
        break;
      }
    case MONADIC_FORMULA:
      {
        global_prop = genie_monadic (p);
        break;
      }
    case NIHIL:
      {
        global_prop = genie_nihil (p);
        break;
      }
/* Ex unit. */
    case ASSIGNATION:
      {
        global_prop = genie_assignation (p);
        break;
      }
    case IDENTITY_RELATION:
      {
        global_prop = genie_identity_relation (p);
        break;
      }
    case AND_FUNCTION:
      {
        global_prop = genie_and_function (p);
        break;
      }
    case OR_FUNCTION:
      {
        global_prop = genie_or_function (p);
        break;
      }
    case ROUTINE_TEXT:
      {
        global_prop = genie_routine_text (p);
        break;
      }
    case SKIP:
      {
        global_prop = genie_skip (p);
        break;
      }
    case JUMP:
      {
        global_prop.unit = genie_unit;
        global_prop.source = p;
        genie_jump (p);
        break;
      }
    case ASSERTION:
      {
        global_prop = genie_assertion (p);
        break;
      }
    }
  }
  p->genie.propagator = global_prop;
  return (global_prop);
}

/*!
\brief execute a unit, possibly in trace mood
\param p position in the syntax tree, should not be NULL
**/

void genie_unit_trace (NODE_T * p)
{
  if (sys_request_flag) {
    single_step (p, A_TRUE, A_FALSE);
  } else if (MASK (p) & BREAKPOINT_MASK) {
    if (INFO (p)->expr == NULL) {
      sys_request_flag = A_FALSE;
      single_step (p, A_FALSE, A_TRUE);
    } else if (breakpoint_expression (p)) {
      sys_request_flag = A_FALSE;
      single_step (p, A_FALSE, A_TRUE);
    }
  } else if (MASK (p) & TRACE_MASK) {
    where (STDOUT_FILENO, p);
  }
  EXECUTE_UNIT (p);
}

/*!
\brief execution of serial clause without labels
\param p position in the syntax tree, should not be NULL
\param p position in the syntax tree, should not be NULLop_sp
\param seq
**/

void genie_serial_units_no_label (NODE_T * p, int pop_sp, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        EXECUTE_UNIT_TRACE (p);
        SEQUENCE (*seq) = p;
        (*seq) = p;
        return;
      }
    case SEMI_SYMBOL:
      {
/* Voiden the expression stack. */
        stack_pointer = pop_sp;
        SEQUENCE (*seq) = p;
        (*seq) = p;
        break;
      }
    case DECLARATION_LIST:
      {
        genie_declaration (SUB (p));
        SEQUENCE (*seq) = p;
        (*seq) = p;
        return;
      }
    default:
      {
        genie_serial_units_no_label (SUB (p), pop_sp, seq);
        break;
      }
    }
  }
}

/*!
\brief execution of serial clause with labels
\param p position in the syntax tree, should not be NULL
\param jump_to
\param exit_buf
\param p position in the syntax tree, should not be NULLop_sp
**/

void genie_serial_units (NODE_T * p, NODE_T ** jump_to, jmp_buf * exit_buf, int pop_sp)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        if (*jump_to == NULL) {
          EXECUTE_UNIT_TRACE (p);
        } else if (p == *jump_to) {
/* If we dropped in this clause from a jump then this unit is the target. */
          *jump_to = NULL;
          EXECUTE_UNIT_TRACE (p);
        }
        return;
      }
    case EXIT_SYMBOL:
      {
        if (*jump_to == NULL) {
          longjmp (*exit_buf, 1);
        }
        break;
      }
    case SEMI_SYMBOL:
      {
        if (*jump_to == NULL) {
/* Voiden the expression stack. */
          stack_pointer = pop_sp;
        }
        break;
      }
    default:
      {
        if (WHETHER (p, DECLARATION_LIST) && *jump_to == NULL) {
          genie_declaration (SUB (p));
          return;
        } else {
          genie_serial_units (SUB (p), jump_to, exit_buf, pop_sp);
        }
        break;
      }
    }
  }
}

/*!
\brief execute serial clause
\param p position in the syntax tree, should not be NULL
\param exit_buf
**/

void genie_serial_clause (NODE_T * p, jmp_buf * exit_buf)
{
  if (SYMBOL_TABLE (p)->labels == NULL) {
/* No labels in this clause. */
    if (SEQUENCE (p) == NULL && SEQUENCE_SET (p) == A_FALSE) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      genie_serial_units_no_label (SUB (p), stack_pointer, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      SEQUENCE_SET (p) = A_TRUE;
      MASK (p) |= SERIAL_MASK;
      if (SEQUENCE (p) != NULL && SEQUENCE (SEQUENCE (p)) == NULL) {
        MASK (p) |= OPTIMAL_MASK;
      }
    } else {
/* A linear list without labels. */
      NODE_T *q;
      ADDR_T pop_sp = stack_pointer;
      for (q = SEQUENCE (p); q != NULL; q = SEQUENCE (q)) {
        switch (ATTRIBUTE (q)) {
        case UNIT:
          {
            EXECUTE_UNIT_TRACE (q);
            break;
          }
        case SEMI_SYMBOL:
          {
            stack_pointer = pop_sp;
            break;
          }
        case DECLARATION_LIST:
          {
            genie_declaration (SUB (q));
            break;
          }
        }
      }
    }
  } else {
/* Labels in this clause. */
    jmp_buf jump_stat;
    ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
    ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
    FRAME_JUMP_STAT (frame_pointer) = &jump_stat;
    if (!setjmp (jump_stat)) {
      NODE_T *jump_to = NULL;
      genie_serial_units (SUB (p), &jump_to, exit_buf, stack_pointer);
    } else {
/* HIjol! Restore state and look for indicated unit. */
      NODE_T *jump_to = SYMBOL_TABLE (p)->jump_to;
/* If we jumped from a parallel clause we zap all units. */
#ifdef HAVE_POSIX_THREADS
      if (parallel_clauses > 0) {
        ABNORMAL_END (!is_main_thread (), "target label not in main thread", NULL);
      }
#endif
      stack_pointer = pop_sp;
      frame_pointer = pop_fp;
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
      genie_serial_units (SUB (p), &jump_to, exit_buf, stack_pointer);
    }
  }
}

/*!
\brief execute enquiry clause
\param p position in the syntax tree, should not be NULL
**/

void genie_enquiry_clause (NODE_T * p)
{
  if (SEQUENCE (p) == NULL && SEQUENCE_SET (p) == A_FALSE) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    genie_serial_units_no_label (SUB (p), stack_pointer, &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    SEQUENCE_SET (p) = A_TRUE;
    if (SEQUENCE (p) != NULL && SEQUENCE (SEQUENCE (p)) == NULL) {
      MASK (p) |= OPTIMAL_MASK;
    }
  } else {
/* A linear list without labels (of course, it's an enquiry clause). */
    NODE_T *q;
    ADDR_T pop_sp = stack_pointer;
    MASK (p) |= SERIAL_MASK;
    for (q = SEQUENCE (p); q != NULL; q = SEQUENCE (q)) {
      switch (ATTRIBUTE (q)) {
      case UNIT:
        {
          EXECUTE_UNIT_TRACE (q);
          break;
        }
      case SEMI_SYMBOL:
        {
          stack_pointer = pop_sp;
          break;
        }
      case DECLARATION_LIST:
        {
          genie_declaration (SUB (q));
          break;
        }
      }
    }
  }
}

/*!
\brief execute collateral units
\param p position in the syntax tree, should not be NULL
\param count
**/

static void genie_collateral_units (NODE_T * p, int *count)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      EXECUTE_UNIT_TRACE (p);
      GENIE_DNS_STACK (p, MOID (p), FRAME_DYNAMIC_SCOPE (frame_pointer), NULL);
      (*count)++;
      return;
    } else {
      genie_collateral_units (SUB (p), count);
    }
  }
}

/*!
\brief execute collateral clause
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_collateral (NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = genie_collateral;
  self.source = p;
/* VOID clause and STRUCT display. */
  if (MOID (p) == MODE (VOID) || WHETHER (MOID (p), STRUCT_SYMBOL)) {
    int count = 0;
    genie_collateral_units (SUB (p), &count);
  }
/* Row display. */
  else {
    A68_REF new_display;
    int count = 0;
    ADDR_T sp = stack_pointer;
    MOID_T *m = MOID (p);
    genie_collateral_units (SUB (p), &count);
    if (DEFLEX (m)->dimensions == 1) {
/* [] AMODE display. */
      new_display = genie_make_row (p, DEFLEX (m)->slice, count, sp);
      stack_pointer = sp;
      INCREMENT_STACK_POINTER (p, SIZE_OF (A68_REF));
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    } else {
/* [,,] AMODE display, we concatenate 1 + (n-1) to n dimensions. */
      new_display = genie_concatenate_rows (p, m, count, sp);
      stack_pointer = sp;
      INCREMENT_STACK_POINTER (p, SIZE_OF (A68_REF));
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    }
  }
  return (self);
}

/*!
\brief execute unit from integral-case in-part
\param p position in the syntax tree, should not be NULL
\param k
\param count
\return
**/

BOOL_T genie_int_case_unit (NODE_T * p, int k, int *count)
{
  if (p == NULL) {
    return (A_FALSE);
  } else {
    if (WHETHER (p, UNIT)) {
      if (k == *count) {
        EXECUTE_UNIT_TRACE (p);
        return (A_TRUE);
      } else {
        (*count)++;
        return (A_FALSE);
      }
    } else {
      if (genie_int_case_unit (SUB (p), k, count) != 0) {
        return (A_TRUE);
      } else {
        return (genie_int_case_unit (NEXT (p), k, count));
      }
    }
  }
}

/*!
\brief execute unit from united-case in-part
\param p position in the syntax tree, should not be NULL
\param m
\return
**/

BOOL_T genie_united_case_unit (NODE_T * p, MOID_T * m)
{
  if (p == NULL) {
    return (A_FALSE);
  } else {
    if (WHETHER (p, SPECIFIER)) {
      MOID_T *spec_moid = MOID (NEXT (SUB (p)));
      BOOL_T equal_modes;
      if (m != NULL) {
        if (WHETHER (spec_moid, UNION_SYMBOL)) {
          equal_modes = whether_unitable (m, spec_moid, SAFE_DEFLEXING);
        } else {
          equal_modes = (m == spec_moid);
        }
      } else {
        equal_modes = A_FALSE;
      }
      if (equal_modes) {
        NODE_T *q = NEXT (NEXT (SUB (p)));
        OPEN_STATIC_FRAME (p);
        if (WHETHER (q, IDENTIFIER)) {
          if (WHETHER (spec_moid, UNION_SYMBOL)) {
            COPY ((FRAME_OFFSET (FRAME_INFO_SIZE + TAX (q)->offset)), STACK_TOP, (unsigned) MOID_SIZE (spec_moid));
          } else {
            COPY ((FRAME_OFFSET (FRAME_INFO_SIZE + TAX (q)->offset)), STACK_OFFSET (SIZE_OF (A68_UNION)), (unsigned) MOID_SIZE (spec_moid));
          }
        }
        EXECUTE_UNIT_TRACE (NEXT (NEXT (p)));
        CLOSE_FRAME;
        return (A_TRUE);
      } else {
        return (A_FALSE);
      }
    } else {
      if (genie_united_case_unit (SUB (p), m)) {
        return (A_TRUE);
      } else {
        return (genie_united_case_unit (NEXT (p), m));
      }
    }
  }
}

/*!
\brief execute identity declaration
\param p position in the syntax tree, should not be NULL
**/

static void genie_identity_dec (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_IDENTIFIER:
      {
        MOID_T *source_moid = MOID (p);
        unsigned size = MOID_SIZE (source_moid);
        BYTE_T *z = (FRAME_OFFSET (FRAME_INFO_SIZE + TAX (p)->offset));
        ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
        FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
        EXECUTE_UNIT (NEXT (NEXT (p)));
        GENIE_CHECK_INITIALISATION (NEXT (NEXT (p)), STACK_OFFSET (-size), source_moid);
        GENIE_DNS_STACK (NEXT (NEXT (p)), source_moid, frame_pointer, NULL);
        FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
        if (source_moid->has_rows) {
          DECREMENT_STACK_POINTER (p, size);
          if (WHETHER (source_moid, STRUCT_SYMBOL)) {
/* STRUCT with row. */
            A68_REF w, src;
            w.status = INITIALISED_MASK;
            w.segment = stack_segment;
            w.offset = stack_pointer;
            w.handle = &nil_handle;
            src = genie_copy_stowed (w, p, MOID (p));
            COPY (z, ADDRESS (&src), size);
          } else if (WHETHER (MOID (p), UNION_SYMBOL)) {
/* UNION with row. */
            genie_copy_union (p);
            COPY (z, STACK_TOP, size);
          } else if (WHETHER (MOID (p), ROW_SYMBOL) || WHETHER (MOID (p), FLEX_SYMBOL)) {
/* (FLEX) ROW. */
            *(A68_REF *) z = genie_copy_stowed (*(A68_REF *) STACK_TOP, p, MOID (p));
          }
        } else {
          POP (p, z, size);
        }
        return;
      }
    default:
      {
        genie_identity_dec (SUB (p));
        break;
      }
    }
  }
}

/*!
\brief execute variable declaration
\param p position in the syntax tree, should not be NULL
\param declarer
**/

static void genie_variable_dec (NODE_T * p, NODE_T ** declarer, ADDR_T sp)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, VARIABLE_DECLARATION)) {
      genie_variable_dec (SUB (p), declarer, sp);
    } else {
      if (WHETHER (p, DECLARER)) {
        (*declarer) = SUB (p);
        genie_generator_bounds (*declarer);
        FORWARD (p);
      }
      if (WHETHER (p, DEFINING_IDENTIFIER)) {
        MOID_T *ref_mode = MOID (p);
        TAG_T *tag = TAX (p);
        int leap = (HEAP (tag) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
        A68_REF *z = (A68_REF *) (FRAME_OFFSET (FRAME_INFO_SIZE + TAX (p)->offset));
        genie_generator_internal (*declarer, ref_mode, tag->body, leap, sp);
        POP_REF (p, z);
        if (NEXT (p) != NULL && WHETHER (NEXT (p), ASSIGN_SYMBOL)) {
          MOID_T *source_moid = SUB (MOID (p));
          int size = MOID_SIZE (source_moid);
          ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
          FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
          EXECUTE_UNIT (NEXT (NEXT (p)));
          GENIE_DNS_STACK (p, source_moid, frame_pointer, NULL);
          FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
          DECREMENT_STACK_POINTER (p, size);
          if (source_moid->has_rows) {
            genie_assign_internal (p, z, source_moid);
          } else {
            MOVE (ADDRESS (z), STACK_TOP, (unsigned) size);
          }
        }
      }
    }
  }
}

/*!
\brief execute PROC variable declaration
\param p position in the syntax tree, should not be NULL
**/

static void genie_proc_variable_dec (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_IDENTIFIER:
      {
        ADDR_T sp_for_voiding = stack_pointer;
        MOID_T *ref_mode = MOID (p);
        TAG_T *tag = TAX (p);
        int leap = (HEAP (tag) == LOC_SYMBOL) ? LOC_SYMBOL : HEAP_SYMBOL;
        A68_REF *z = (A68_REF *) (FRAME_OFFSET (FRAME_INFO_SIZE + TAX (p)->offset));
        genie_generator_internal (p, ref_mode, tag->body, leap, stack_pointer);
        POP_REF (p, z);
        if (NEXT (p) != NULL && WHETHER (NEXT (p), ASSIGN_SYMBOL)) {
          MOID_T *source_moid = SUB (MOID (p));
          int size = MOID_SIZE (source_moid);
          EXECUTE_UNIT (NEXT (NEXT (p)));
          DECREMENT_STACK_POINTER (p, size);
          MOVE (ADDRESS (z), STACK_TOP, (unsigned) size);
        }
        stack_pointer = sp_for_voiding; /* Voiding. */
        return;
      }
    default:
      {
        genie_proc_variable_dec (SUB (p));
        break;
      }
    }
  }
}

/*!
\brief execute operator declaration
\param p position in the syntax tree, should not be NULL
**/

static void genie_operator_dec (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_OPERATOR:
      {
        MOID_T *source_moid = MOID (p);
        unsigned size = MOID_SIZE (source_moid);
        BYTE_T *z = (FRAME_OFFSET (FRAME_INFO_SIZE + TAX (p)->offset));
        ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
        FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
        EXECUTE_UNIT (NEXT (NEXT (p)));
        GENIE_DNS_STACK (p, source_moid, frame_pointer, NULL);
        FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
        POP (p, z, size);
        return;
      }
    default:
      {
        genie_operator_dec (SUB (p));
        break;
      }
    }
  }
}

/*!
\brief execute declaration
\param p position in the syntax tree, should not be NULL
**/

void genie_declaration (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case MODE_DECLARATION:
    case PROCEDURE_DECLARATION:
    case BRIEF_OPERATOR_DECLARATION:
    case PRIORITY_DECLARATION:
      {
/* Already resolved. */
        return;
      }
    case IDENTITY_DECLARATION:
      {
        genie_identity_dec (SUB (p));
        break;
      }
    case OPERATOR_DECLARATION:
      {
        genie_operator_dec (SUB (p));
        break;
      }
    case VARIABLE_DECLARATION:
      {
        NODE_T *declarer = NULL;
        ADDR_T pop_sp = stack_pointer;
        genie_variable_dec (SUB (p), &declarer, stack_pointer);
/* Voiding to remove garbage from declarers. */
        stack_pointer = pop_sp;
        break;
      }
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        ADDR_T pop_sp = stack_pointer;
        genie_proc_variable_dec (SUB (p));
        stack_pointer = pop_sp;
        break;
      }
    default:
      {
        genie_declaration (SUB (p));
        break;
      }
    }
  }
}

PROPAGATOR_T genie_loop (volatile NODE_T *);
void genie_conditional (volatile NODE_T *, volatile MOID_T *);
void genie_int_case (volatile NODE_T *, volatile MOID_T *);
void genie_united_case (volatile NODE_T *, volatile MOID_T *);

/* PROP_CAST is a workaround for a type-checker bug in gcc 3.3 */
#define PROP_CAST(z) (* ((PROPAGATOR_T *) &(z)))

#define LABEL_FREE(p) {\
  NODE_T *m_q; ADDR_T pop_sp = stack_pointer;\
  for (m_q = SEQUENCE (p); m_q != NULL; m_q = SEQUENCE (m_q)) {\
    switch (ATTRIBUTE (m_q)) {\
    case UNIT: {\
      EXECUTE_UNIT_TRACE (m_q);\
      break;\
    }\
    case SEMI_SYMBOL: {\
      stack_pointer = pop_sp;\
      break;\
    }\
    case DECLARATION_LIST: {\
      genie_declaration (SUB (m_q));\
      break;\
    }\
    }\
  }}

#define SERIAL_CLAUSE(p)\
  if (MASK (p) & OPTIMAL_MASK) {\
    EXECUTE_UNIT (SEQUENCE (p));\
  } else if (MASK (p) & SERIAL_MASK) {\
    LABEL_FREE (p);\
  } else {\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) p, (jmp_buf *) exit_buf);\
  }}

#define SERIAL_CLAUSE_TRACE(p)\
  if (MASK (p) & OPTIMAL_MASK) {\
    EXECUTE_UNIT_TRACE (SEQUENCE (p));\
  } else if (MASK (p) & SERIAL_MASK) {\
    LABEL_FREE (p);\
  } else {\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) p, (jmp_buf *) exit_buf);\
  }}

#define ENQUIRY_CLAUSE(p)\
  if (MASK (p) & OPTIMAL_MASK) {\
    EXECUTE_UNIT (SEQUENCE (p));\
  } else if (MASK (p) & SERIAL_MASK) {\
    LABEL_FREE (p);\
  } else {\
    genie_enquiry_clause ((NODE_T *) p);\
  }

/*!
\brief execute enclosed clause
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_enclosed (volatile NODE_T * p)
{
  PROPAGATOR_T self;
  jmp_buf exit_buf;
  self.unit = (PROPAGATOR_PROCEDURE *) genie_enclosed;
  self.source = (NODE_T *) p;
  switch (ATTRIBUTE (p)) {
  case PARTICULAR_PROGRAM:
    {
      (void) genie_enclosed (SUB (p));
      break;
    }
  case ENCLOSED_CLAUSE:
    {
      (void) genie_enclosed (SUB (p));
      break;
    }
  case CLOSED_CLAUSE:
    {
      volatile NODE_T *q = NEXT_SUB (p);
      OPEN_STATIC_FRAME ((NODE_T *) q);
      SERIAL_CLAUSE (q);
      CLOSE_FRAME;
      break;
    }
  case PARALLEL_CLAUSE:
    {
      (void) genie_parallel ((NODE_T *) NEXT_SUB (p));
      break;
    }
  case COLLATERAL_CLAUSE:
    {
      (void) genie_collateral ((NODE_T *) p);
      break;
    }
  case CONDITIONAL_CLAUSE:
    {
      genie_conditional ((NODE_T *) SUB (p), MOID (p));
      break;
    }
  case INTEGER_CASE_CLAUSE:
    {
      genie_int_case (SUB ((NODE_T *) p), MOID (p));
      break;
    }
  case UNITED_CASE_CLAUSE:
    {
      genie_united_case (SUB ((NODE_T *) p), MOID (p));
      break;
    }
  case LOOP_CLAUSE:
    {
      (void) genie_loop (SUB ((NODE_T *) p));
      self.unit = (PROPAGATOR_PROCEDURE *) genie_loop;
      self.source = SUB ((NODE_T *) p);
      break;
    }
  }
  if (MOID (p) != NULL && MOID (p) != MODE (VOID)) {
    GENIE_DNS_STACK ((NODE_T *) p, MOID ((NODE_T *) p), frame_pointer, NULL);
  }
  PROTECT_FROM_SWEEP ((NODE_T *) p);
  PROP_CAST (p->genie.propagator) = PROP_CAST (self);
  return (self);
}

/*!
\brief execute integral-case-clause
\param p position in the syntax tree, should not be NULL
\param yield
**/

void genie_int_case (volatile NODE_T * p, volatile MOID_T * yield)
{
  volatile int unit_count;
  volatile BOOL_T found_unit;
  jmp_buf exit_buf;
  A68_INT k;
/* CASE. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
  ENQUIRY_CLAUSE (NEXT_SUB (p));
  POP_INT (p, &k);
/* IN. */
  FORWARD (p);
  OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
  unit_count = 1;
  found_unit = genie_int_case_unit (NEXT_SUB ((NODE_T *) p), (int) k.value, (int *) &unit_count);
  CLOSE_FRAME;
/* OUT. */
  if (!found_unit) {
    FORWARD (p);
    switch (ATTRIBUTE (p)) {
    case CHOICE:
    case OUT_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
        SERIAL_CLAUSE (NEXT_SUB (p));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case ESAC_SYMBOL:
      {
        genie_push_undefined ((NODE_T *) p, (MOID_T *) yield);
        break;
      }
    default:
      {
        genie_int_case ((NODE_T *) SUB (p), (MOID_T *) yield);
        break;
      }
    }
  }
/* ESAC. */
  CLOSE_FRAME;
}

/*!
\brief execute united-case-clause
\param p position in the syntax tree, should not be NULL
\param yield
**/

void genie_united_case (volatile NODE_T * p, volatile MOID_T * yield)
{
  volatile BOOL_T found_unit = A_FALSE;
  volatile MOID_T *um;
  volatile ADDR_T pop_sp;
  jmp_buf exit_buf;
/* CASE. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
  pop_sp = stack_pointer;
  ENQUIRY_CLAUSE (NEXT_SUB (p));
  stack_pointer = pop_sp;
  um = (volatile MOID_T *) ((A68_UNION *) STACK_TOP)->value;
/* IN. */
  FORWARD (p);
  if (um != NULL) {
    OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
    found_unit = genie_united_case_unit (NEXT_SUB ((NODE_T *) p), (MOID_T *) um);
    CLOSE_FRAME;
  } else {
    found_unit = A_FALSE;
  }
/* OUT. */
  if (!found_unit) {
    FORWARD (p);
    switch (ATTRIBUTE (p)) {
    case CHOICE:
    case OUT_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
        SERIAL_CLAUSE (NEXT_SUB (p));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case ESAC_SYMBOL:
      {
        genie_push_undefined ((NODE_T *) p, (MOID_T *) yield);
        break;
      }
    default:
      {
        genie_united_case (SUB ((NODE_T *) p), (MOID_T *) yield);
        break;
      }
    }
  }
/* ESAC. */
  CLOSE_FRAME;
}

/*!
\brief execute conditional-clause
\param p position in the syntax tree, should not be NULL
\param yield
**/

void genie_conditional (volatile NODE_T * p, volatile MOID_T * yield)
{
  volatile int pop_sp = stack_pointer;
  jmp_buf exit_buf;
/* IF. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
  ENQUIRY_CLAUSE (NEXT_SUB (p));
  stack_pointer = pop_sp;
  FORWARD (p);
  if (((A68_BOOL *) STACK_TOP)->value == A_TRUE) {
/* THEN. */
    OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
    SERIAL_CLAUSE (NEXT_SUB (p));
    CLOSE_FRAME;
  } else {
/* ELSE. */
    FORWARD (p);
    switch (ATTRIBUTE (p)) {
    case CHOICE:
    case ELSE_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (p));
        SERIAL_CLAUSE (NEXT_SUB (p));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case FI_SYMBOL:
      {
        genie_push_undefined ((NODE_T *) p, (MOID_T *) yield);
        break;
      }
    default:
      {
        genie_conditional (SUB ((NODE_T *) p), (MOID_T *) yield);
        break;
      }
    }
  }
/* FI. */
  CLOSE_FRAME;
}

/*
INCREMENT_COUNTER procures that the counter only increments if there is
a for-part or a to-part. Otherwise an infinite loop would trigger overflow
when the anonymous counter reaches max int, which is strange behaviour.
*/

#define INCREMENT_COUNTER\
  if (!(for_part == NULL && to_part == NULL)) {\
    TEST_INT_ADDITION ((NODE_T *) p, counter, by);\
    counter += by;\
  }

/*!
\brief execute loop-clause
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_loop (volatile NODE_T * p)
{
  volatile ADDR_T pop_sp = stack_pointer;
  volatile int from, by, to, counter;
  volatile BOOL_T go_on, conditional;
  volatile NODE_T *for_part = NULL, *to_part = NULL, *q;
  jmp_buf exit_buf;
/* FOR  identifier. */
  if (WHETHER (p, FOR_PART)) {
    for_part = NEXT_SUB (p);
    FORWARD (p);
  }
/* FROM unit. */
  if (WHETHER (p, FROM_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    from = ((A68_INT *) STACK_TOP)->value;
    FORWARD (p);
  } else {
    from = 1;
  }
/* BY unit. */
  if (WHETHER (p, BY_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    by = ((A68_INT *) STACK_TOP)->value;
    FORWARD (p);
  } else {
    by = 1;
  }
/* TO unit, DOWNTO unit. */
  if (WHETHER (p, TO_PART)) {
    if (WHETHER (SUB (p), DOWNTO_SYMBOL)) {
      by = -by;
    }
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    to = ((A68_INT *) STACK_TOP)->value;
    to_part = p;
    FORWARD (p);
  } else {
    to = (by >= 0 ? MAX_INT : -MAX_INT);
  }
  q = NEXT_SUB (p);
/* Here the loop part starts.
   We open the frame only once and reinitialise if necessary. */
  OPEN_STATIC_FRAME ((NODE_T *) q);
  counter = from;
/* Does the loop contain conditionals? */
  if (WHETHER (p, WHILE_PART)) {
    conditional = A_TRUE;
  } else if (WHETHER (p, DO_PART) || WHETHER (p, ALT_DO_PART)) {
    NODE_T *un_p = NEXT_SUB (p);
    if (WHETHER (un_p, SERIAL_CLAUSE)) {
      un_p = NEXT (un_p);
    }
    conditional = (un_p != NULL && WHETHER (un_p, UNTIL_PART));
  } else {
    conditional = A_FALSE;
  }
  if (conditional) {
/* [FOR ...] [WHILE ...] DO [...] [UNTIL ...] OD. */
    go_on = (by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0);
    while (go_on) {
      if (for_part != NULL) {
        A68_INT *z = (A68_INT *) (FRAME_OFFSET (FRAME_INFO_SIZE + TAX (for_part)->offset));
        z->status = INITIALISED_MASK;
        z->value = counter;
      }
      stack_pointer = pop_sp;
      if (WHETHER (p, WHILE_PART)) {
        ENQUIRY_CLAUSE (q);
        stack_pointer = pop_sp;
        go_on = ((A68_BOOL *) STACK_TOP)->value == A_TRUE;
      }
      if (go_on) {
        volatile NODE_T *do_p = p, *un_p;
        if (WHETHER (p, WHILE_PART)) {
          do_p = NEXT_SUB (NEXT (p));
          OPEN_STATIC_FRAME ((NODE_T *) do_p);
        } else {
          do_p = NEXT_SUB (p);
        }
        if (WHETHER (do_p, SERIAL_CLAUSE)) {
          SERIAL_CLAUSE_TRACE (do_p);
          un_p = NEXT (do_p);
        } else {
          un_p = do_p;
        }
/* UNTIL part. */
        if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
          NODE_T *v = NEXT_SUB (un_p);
          OPEN_STATIC_FRAME ((NODE_T *) v);
          stack_pointer = pop_sp;
          ENQUIRY_CLAUSE (v);
          stack_pointer = pop_sp;
          go_on = ((A68_BOOL *) STACK_TOP)->value == A_FALSE;
          CLOSE_FRAME;
        }
        if (WHETHER (p, WHILE_PART)) {
          CLOSE_FRAME;
        }
/* Increment counter. */
        if (go_on) {
          INCREMENT_COUNTER;
          go_on = (by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0);
        }
/* The genie cannot take things to next iteration: re-initialise stack frame. */
        if (go_on) {
          PREEMPTIVE_SWEEP;
          CHECK_TIME_LIMIT (p);
          FRAME_CLEAR (SYMBOL_TABLE (q)->ap_increment);
          if (SYMBOL_TABLE (q)->initialise_frame) {
            initialise_frame ((NODE_T *) q);
          }
        }
      }
    }
  } else {
/* [FOR ...] DO ... OD. */
    go_on = (by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0);
    while (go_on) {
      if (for_part != NULL) {
        A68_INT *z = (A68_INT *) (FRAME_OFFSET (FRAME_INFO_SIZE + TAX (for_part)->offset));
        z->status = INITIALISED_MASK;
        z->value = counter;
      }
      stack_pointer = pop_sp;
      SERIAL_CLAUSE_TRACE (NEXT_SUB (p));
      INCREMENT_COUNTER;
      go_on = (by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0);
/* The genie cannot take things to next iteration: re-initialise stack frame. */
      if (go_on) {
        PREEMPTIVE_SWEEP;
        CHECK_TIME_LIMIT (p);
        FRAME_CLEAR (SYMBOL_TABLE (q)->ap_increment);
        if (SYMBOL_TABLE (q)->initialise_frame) {
          initialise_frame ((NODE_T *) q);
        }
      }
    }
  }
/* OD. */
  CLOSE_FRAME;
  stack_pointer = pop_sp;
  return (PROP_CAST (p->genie.propagator));
}

#undef INCREMENT_COUNTER
#undef LOOP_OVERFLOW
