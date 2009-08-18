/*!
\file engine.c
\brief routines executing primitive A68 actions
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "algol68g.h"
#include "genie.h"
#include "inline.h"
#include "mp.h"
#include "transput.h"

/*
This file contains interpreter ("genie") routines related to executing primitive
A68 actions.

The genie is self-optimising as when it traverses the tree, it stores terminals
it ends up in at the root where traversing for that terminal started.
Such piece of information is called a PROPAGATOR.
*/

void genie_serial_units_no_label (NODE_T *, int, NODE_T **);

PROPAGATOR_T genie_assignation_quick (NODE_T * p);
PROPAGATOR_T genie_loop (volatile NODE_T *);
PROPAGATOR_T genie_voiding_assignation_constant (NODE_T * p);
PROPAGATOR_T genie_voiding_assignation (NODE_T * p);

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
\param seq chain to link nodes into
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
        SEQUENCE (*seq) = save;
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
\param p position in tree
\param count number of constants initialised
**/

void genie_find_proc_op (NODE_T * p, int *count)
{
  for (; p != NULL; FORWARD (p)) {
    if (GENIE (p) != NULL && GENIE (p)->whether_new_lexical_level) {
/* Don't enter a new lexical level - it will have its own initialisation. */
      return;
    } else if (WHETHER (p, PROCEDURE_DECLARATION) || WHETHER (p, BRIEF_OPERATOR_DECLARATION)) {
      genie_init_proc_op (SUB (p), &(SEQUENCE (SYMBOL_TABLE (p))), count);
      return;
    } else {
      genie_find_proc_op (SUB (p), count);
    }
  }
}

void initialise_frame (NODE_T * p)
{
  if (SYMBOL_TABLE (p)->initialise_anon) {
    TAG_T *_a_;
    SYMBOL_TABLE (p)->initialise_anon = A68_FALSE;
    for (_a_ = SYMBOL_TABLE (p)->anonymous; _a_ != NULL; FORWARD (_a_)) {
      if (PRIO (_a_) == ROUTINE_TEXT) {
        int youngest = TAX (NODE (_a_))->youngest_environ;
        A68_PROCEDURE *_z_ = (A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (_a_)));
        STATUS (_z_) = INITIALISED_MASK;
        (BODY (_z_)).node = NODE (_a_);
        if (youngest > 0) {
          STATIC_LINK_FOR_FRAME (ENVIRON (_z_), 1 + youngest);
        } else {
          ENVIRON (_z_) = 0;
        }
        LOCALE (_z_) = NULL;
        MOID (_z_) = MOID (_a_);
        SYMBOL_TABLE (p)->initialise_anon = A68_TRUE;
      } else if (PRIO (_a_) == FORMAT_TEXT) {
        int youngest = TAX (NODE (_a_))->youngest_environ;
        A68_FORMAT *_z_ = (A68_FORMAT *) (FRAME_OBJECT (OFFSET (_a_)));
        STATUS (_z_) = INITIALISED_MASK;
        BODY (_z_) = NODE (_a_);
        if (youngest > 0) {
          STATIC_LINK_FOR_FRAME (ENVIRON (_z_), 1 + youngest);
        } else {
          ENVIRON (_z_) = 0;
        }
        SYMBOL_TABLE (p)->initialise_anon = A68_TRUE;
      }
    }
  }
  if (SYMBOL_TABLE (p)->proc_ops) {
    NODE_T *_q_;
    ADDR_T pop_sp;
    if (SEQUENCE (SYMBOL_TABLE (p)) == NULL) {
      int count = 0;
      genie_find_proc_op (p, &count);
      SYMBOL_TABLE (p)->proc_ops = (BOOL_T) (count > 0);
    }
    pop_sp = stack_pointer;
    for (_q_ = SEQUENCE (SYMBOL_TABLE (p)); _q_ != NULL; _q_ = SEQUENCE (_q_)) {
      NODE_T *u = NEXT_NEXT (_q_);
      if (WHETHER (u, ROUTINE_TEXT)) {
        PROPAGATOR_T *prop = &PROPAGATOR (u);
        NODE_T *src = prop->source;
        *(A68_PROCEDURE *) FRAME_OBJECT (OFFSET (TAX (_q_))) = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (src))));
      } else if ((WHETHER (u, UNIT) && WHETHER (SUB (u), ROUTINE_TEXT))) {
        PROPAGATOR_T *prop = &PROPAGATOR (SUB (u));
        NODE_T *src = prop->source;
        *(A68_PROCEDURE *) FRAME_OBJECT (OFFSET (TAX (_q_))) = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (src))));
      }
    }
  }
  SYMBOL_TABLE (p)->initialise_frame = (BOOL_T) (SYMBOL_TABLE (p)->initialise_anon || SYMBOL_TABLE (p)->proc_ops);
}

/*!
\brief dynamic scope check
\param p position in tree
\param m mode of object
\param w pointer to object
\param limit limiting frame pointer
\param info info for diagnostics
**/

void genie_dns_addr (NODE_T * p, MOID_T * m, BYTE_T * w, ADDR_T limit, char *info)
{
  if (m != NULL && w != NULL) {
    ADDR_T _limit_2 = (limit < global_pointer ? global_pointer : limit);
    if (WHETHER (m, REF_SYMBOL)) {
      SCOPE_CHECK (p, (GET_REF_SCOPE ((A68_REF *) w)), _limit_2, m, info);
    } else if (WHETHER (m, UNION_SYMBOL)) {
      genie_dns_addr (p, (MOID_T *) VALUE ((A68_UNION *) w), &(w[ALIGNED_SIZE_OF (A68_UNION)]), _limit_2, "united value");
    } else if (WHETHER (m, PROC_SYMBOL)) {
      A68_PROCEDURE *v = (A68_PROCEDURE *) w;
      SCOPE_CHECK (p, ENVIRON (v), _limit_2, m, info);
      if (LOCALE (v) != NULL) {
        BYTE_T *u = POINTER (LOCALE (v));
        PACK_T *s = PACK (MOID (v));
        for (; s != NULL; FORWARD (s)) {
          if (VALUE ((A68_BOOL *) & u[0]) == A68_TRUE) {
            genie_dns_addr (p, MOID (s), &u[ALIGNED_SIZE_OF (A68_BOOL)], _limit_2, "partial parameter value");
          }
          u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
        }
      }
    } else if (WHETHER (m, FORMAT_SYMBOL)) {
      SCOPE_CHECK (p, ((A68_FORMAT *) w)->environ, _limit_2, m, info);
    }
  }
}

/* 
The void * cast in next macro is to stop warnings about dropping a volatile
qualifier to a pointer. This is safe here.
*/

#define GENIE_DNS_STACK(p, m, limit, info)\
  if (p != NULL && GENIE (p) != NULL && GENIE (p)->need_dns && limit != PRIMAL_SCOPE) {\
    genie_dns_addr ((void *)(p), (m), (STACK_OFFSET (-MOID_SIZE (m))), (limit), (info));\
  }

#undef SCOPE_CHECK

/*!
\brief whether item at "w" of mode "q" is initialised
\param p position in tree
\param w pointer to object
\param q mode of object
**/

void genie_check_initialisation (NODE_T * p, BYTE_T * w, MOID_T * q)
{
  switch (q->short_id) {
  case REF_SYMBOL:
    {
      A68_REF *z = (A68_REF *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case PROC_SYMBOL:
    {
      A68_PROCEDURE *z = (A68_PROCEDURE *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_INT:
    {
      A68_INT *z = (A68_INT *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_REAL:
    {
      A68_REAL *z = (A68_REAL *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_COMPLEX:
    {
      A68_REAL *r = (A68_REAL *) w;
      A68_REAL *i = (A68_REAL *) (w + ALIGNED_SIZE_OF (A68_REAL));
      CHECK_INIT (p, INITIALISED (r), q);
      CHECK_INIT (p, INITIALISED (i), q);
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
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_CHAR:
    {
      A68_CHAR *z = (A68_CHAR *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_BITS:
    {
      A68_BITS *z = (A68_BITS *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_BYTES:
    {
      A68_BYTES *z = (A68_BYTES *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_LONG_BYTES:
    {
      A68_LONG_BYTES *z = (A68_LONG_BYTES *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_FILE:
    {
      A68_FILE *z = (A68_FILE *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_FORMAT:
    {
      A68_FORMAT *z = (A68_FORMAT *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  case MODE_PIPE:
    {
      A68_REF *pipe_read = (A68_REF *) w;
      A68_REF *pipe_write = (A68_REF *) (w + ALIGNED_SIZE_OF (A68_REF));
      A68_INT *pid = (A68_INT *) (w + 2 * ALIGNED_SIZE_OF (A68_REF));
      CHECK_INIT (p, INITIALISED (pipe_read), q);
      CHECK_INIT (p, INITIALISED (pipe_write), q);
      CHECK_INIT (p, INITIALISED (pid), q);
      return;
    }
  case MODE_SOUND:
    {
      A68_SOUND *z = (A68_SOUND *) w;
      CHECK_INIT (p, INITIALISED (z), q);
      return;
    }
  }
}

/*!
\brief push constant stored in the tree
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_constant (NODE_T * p)
{
  PUSH (p, GENIE (p)->constant, GENIE (p)->size);
  return (PROPAGATOR (p));
}

/*!
\brief unite value in the stack and push result
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_uniting (NODE_T * p)
{
  PROPAGATOR_T self;
  ADDR_T sp = stack_pointer;
  MOID_T *u = MOID (p), *v = MOID (SUB (p));
  int size = MOID_SIZE (u);
  if (ATTRIBUTE (v) != UNION_SYMBOL) {
    PUSH_UNION (p, (void *) unites_to (v, u));
    EXECUTE_UNIT (SUB (p));
  } else {
    A68_UNION *m = (A68_UNION *) STACK_TOP;
    EXECUTE_UNIT (SUB (p));
    VALUE (m) = (void *) unites_to ((MOID_T *) VALUE (m), u);
  }
  stack_pointer = sp + size;
  self.unit = genie_uniting;
  self.source = p;
  return (self);
}

/*!
\brief store widened constant as a constant
\param p position in tree
\param m mode of object
\param self propagator to set
**/

static void make_constant_widening (NODE_T * p, MOID_T * m, PROPAGATOR_T * self)
{
  if (SUB (p) != NULL && GENIE (SUB (p))->constant != NULL) {
    int size = MOID_SIZE (m);
    self->unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((unsigned) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, (void *) (STACK_OFFSET (-size)), size);
  }
}

/*!
\brief (optimised) push INT widened to REAL
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_widening_int_to_real (NODE_T * p)
{
  A68_INT *i = (A68_INT *) STACK_TOP;
  A68_REAL *z = (A68_REAL *) STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REAL) - ALIGNED_SIZE_OF (A68_INT));
  VALUE (z) = (double) VALUE (i);
  STATUS (z) = INITIALISED_MASK;
  return (PROPAGATOR (p));
}

/*!
\brief widen value in the stack
\param p position in tree
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
    (void) genie_widening_int_to_real (p);
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
    PUSH_PRIMITIVE (p, 0.0, A68_REAL);
    make_constant_widening (p, MODE (COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONG_REAL), MODE (LONG_COMPLEX))) {
    MP_DIGIT_T *z;
    int digits = get_mp_digits (MODE (LONG_REAL));
    EXECUTE_UNIT (SUB (p));
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
    make_constant_widening (p, MODE (LONG_COMPLEX), &self);
  } else if (COERCE_FROM_TO (p, MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX))) {
    MP_DIGIT_T *z;
    int digits = get_mp_digits (MODE (LONGLONG_REAL));
    EXECUTE_UNIT (SUB (p));
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
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
    POP_OBJECT (p, &z, A68_BYTES);
    PUSH_REF (p, c_string_to_row_char (p, VALUE (&z), BYTES_WIDTH));
  } else if (COERCE_FROM_TO (p, MODE (LONG_BYTES), MODE (ROW_CHAR))) {
    A68_LONG_BYTES z;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &z, A68_LONG_BYTES);
    PUSH_REF (p, c_string_to_row_char (p, VALUE (&z), LONG_BYTES_WIDTH));
  } else if (COERCE_FROM_TO (p, MODE (BITS), MODE (ROW_BOOL))) {
    A68_BITS x;
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int k;
    unsigned bit;
    BYTE_T *base;
    EXECUTE_UNIT (SUB (p));
    POP_OBJECT (p, &x, A68_BITS);
    z = heap_generator (p, MODE (ROW_BOOL), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_BOOL), BITS_WIDTH * MOID_SIZE (MODE (BOOL)));
    DIM (&arr) = 1;
    MOID (&arr) = MODE (BOOL);
    arr.elem_size = MOID_SIZE (MODE (BOOL));
    arr.slice_offset = 0;
    arr.field_offset = 0;
    ARRAY (&arr) = row;
    LWB (&tup) = 1;
    UPB (&tup) = BITS_WIDTH;
    tup.shift = LWB (&tup);
    tup.span = 1;
    tup.k = 0;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = ADDRESS (&row) + MOID_SIZE (MODE (BOOL)) * (BITS_WIDTH - 1);
    bit = 1;
    for (k = BITS_WIDTH - 1; k >= 0; k--, base -= MOID_SIZE (MODE (BOOL)), bit <<= 1) {
      STATUS ((A68_BOOL *) base) = INITIALISED_MASK;
      VALUE ((A68_BOOL *) base) = (BOOL_T) ((VALUE (&x) & bit) != 0 ? A68_TRUE : A68_FALSE);
    }
    PUSH_REF (p, z);
    UNPROTECT_SWEEP_HANDLE (&z);
    PROTECT_FROM_SWEEP_STACK (p);
  } else if (COERCE_FROM_TO (p, MODE (LONG_BITS), MODE (ROW_BOOL)) || COERCE_FROM_TO (p, MODE (LONGLONG_BITS), MODE (ROW_BOOL))) {
    MOID_T *m = MOID (SUB (p));
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int size = get_mp_size (m), k, width = get_mp_bits_width (m), words = get_mp_bits_words (m);
    unsigned *bits;
    BYTE_T *base;
    MP_DIGIT_T *x;
    ADDR_T pop_sp = stack_pointer;
/* Calculate and convert BITS value. */
    EXECUTE_UNIT (SUB (p));
    x = (MP_DIGIT_T *) STACK_OFFSET (-size);
    bits = stack_mp_bits (p, x, m);
/* Make [] BOOL. */
    z = heap_generator (p, MODE (ROW_BOOL), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    PROTECT_SWEEP_HANDLE (&z);
    row = heap_generator (p, MODE (ROW_BOOL), width * MOID_SIZE (MODE (BOOL)));
    DIM (&arr) = 1;
    MOID (&arr) = MODE (BOOL);
    arr.elem_size = MOID_SIZE (MODE (BOOL));
    arr.slice_offset = 0;
    arr.field_offset = 0;
    ARRAY (&arr) = row;
    LWB (&tup) = 1;
    UPB (&tup) = width;
    tup.shift = LWB (&tup);
    tup.span = 1;
    tup.k = 0;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = ADDRESS (&row) + (width - 1) * MOID_SIZE (MODE (BOOL));
    k = width;
    while (k > 0) {
      unsigned bit = 0x1;
      int j;
      for (j = 0; j < MP_BITS_BITS && k >= 0; j++) {
        STATUS ((A68_BOOL *) base) = INITIALISED_MASK;
        VALUE ((A68_BOOL *) base) = (BOOL_T) ((bits[words - 1] & bit) ? A68_TRUE : A68_FALSE);
        base -= MOID_SIZE (MODE (BOOL));
        bit <<= 1;
        k--;
      }
      words--;
    }
    if (GENIE (SUB (p))->constant != NULL) {
      self.unit = genie_constant;
      PROTECT_SWEEP_HANDLE (&z);
      GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_REF));
      GENIE (p)->size = ALIGNED_SIZE_OF (A68_REF);
      COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_REF));
    } else {
      UNPROTECT_SWEEP_HANDLE (&z);
    }
    stack_pointer = pop_sp;
    PUSH_REF (p, z);
    PROTECT_FROM_SWEEP_STACK (p);
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CANNOT_WIDEN, MOID (SUB (p)), MOID (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  return (self);
#undef COERCE_FROM_TO
}

/*!
\brief cast a jump to a PROC VOID without executing the jump
\param p position in tree
**/

static void genie_proceduring (NODE_T * p)
{
  A68_PROCEDURE z;
  NODE_T *jump = SUB (p);
  NODE_T *q = SUB (jump);
  NODE_T *label = (WHETHER (q, GOTO_SYMBOL) ? NEXT (q) : q);
  STATUS (&z) = INITIALISED_MASK;
  (BODY (&z)).node = jump;
  STATIC_LINK_FOR_FRAME (ENVIRON (&z), 1 + TAG_LEX_LEVEL (TAX (label)));
  LOCALE (&z) = NULL;
  MOID (&z) = MODE (PROC_VOID);
  PUSH_PROCEDURE (p, z);
}

/*!
\brief (optimised) dereference value of a unit
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereferencing_quick (NODE_T * p)
{
  A68_REF *z = (A68_REF *) STACK_TOP;
  int size = MOID_SIZE (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  EXECUTE_UNIT (SUB (p));
  stack_pointer = pop_sp;
  CHECK_REF (p, *z, MOID (SUB (p)));
  PUSH (p, ADDRESS (z), size);
  CHECK_INIT_GENERIC (p, STACK_OFFSET (-size), MOID (p));
  return (PROPAGATOR (p));
}

/*!
\brief dereference an identifier
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_frame_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB (MOID (p));
  int size = MOID_SIZE (deref);
  FRAME_GET (z, A68_REF, p);
  PUSH (p, ADDRESS (z), size);
  CHECK_INIT_GENERIC (p, STACK_OFFSET (-size), deref);
  return (PROPAGATOR (p));
}

/*!
\brief dereference an identifier
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_generic_identifier (NODE_T * p)
{
  A68_REF *z;
  MOID_T *deref = SUB (MOID (p));
  int size = MOID_SIZE (deref);
  FRAME_GET (z, A68_REF, p);
  CHECK_REF (p, *z, MOID (SUB (p)));
  PUSH (p, ADDRESS (z), size);
  CHECK_INIT_GENERIC (p, STACK_OFFSET (-size), deref);
  return (PROPAGATOR (p));
}

/*!
\brief slice REF [] A to A
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *pr = SUB (p);
  A68_ARRAY *a;
  A68_TUPLE *t;
  A68_REF *z, u;
  int size = MOID_SIZE (SUB (MOID (p))), sindex;
  ADDR_T pop_sp = stack_pointer;
  BYTE_T *sp_top = STACK_TOP;
/* Get REF []. */
  UP_SWEEP_SEMA;
  GENIE_GET_OPR (pr, A68_REF, z, u);
  CHECK_REF (p, *z, MOID (SUB (p)));
  GET_DESCRIPTOR (a, t, (A68_ROW *) ADDRESS (z));
  for (sindex = 0, q = SEQUENCE (p); q != NULL; t++, q = SEQUENCE (q)) {
    A68_INT *j;
    int k;
    GENIE_GET_UNIT_ADDRESS (q, A68_INT, j);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    sindex += (t->span * k - t->shift);
    stack_pointer = pop_sp;
  }
/* Push element. */
  PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, sindex)]), size);
  CHECK_INIT_GENERIC (p, sp_top, SUB (MOID (p)));
/*  PROTECT_FROM_SWEEP_STACK (p); */
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief dereference SELECTION from a name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereference_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  MOID_T *result_mode = SUB (MOID (selector));
  int size = MOID_SIZE (result_mode);
  A68_REF *z = (A68_REF *) STACK_TOP;
  ADDR_T pop_sp = stack_pointer;
  EXECUTE_UNIT_INLINE (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  stack_pointer = pop_sp;
  PUSH (p, ADDRESS (z), size);
/*  PROTECT_FROM_SWEEP_STACK (p); */
  return (PROPAGATOR (p));
}

/*!
\brief dereference name in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dereferencing (NODE_T * p)
{
  A68_REF z;
  PROPAGATOR_T self;
  EXECUTE_UNIT_2 (SUB (p), self);
  POP_REF (p, &z);
  CHECK_REF (p, z, MOID (SUB (p)));
  PUSH (p, ADDRESS (&z), MOID_SIZE (MOID (p)));
  CHECK_INIT_GENERIC (p, STACK_OFFSET (-MOID_SIZE (MOID (p))), MOID (p));
  if (self.unit == genie_frame_identifier) {
    if (IS_IN_FRAME (&z)) {
      self.unit = genie_dereference_frame_identifier;
    } else {
      self.unit = genie_dereference_generic_identifier;
    }
    GENIE (self.source)->propagator.unit = self.unit;
  } else if (self.unit == genie_slice_name_quick) {
    self.unit = genie_dereference_slice_name_quick;
    GENIE (self.source)->propagator.unit = self.unit;
  } else if (self.unit == genie_selection_name_quick) {
    self.unit = genie_dereference_selection_name_quick;
    GENIE (self.source)->propagator.unit = self.unit;
  } else {
    self.unit = genie_dereferencing_quick;
    self.source = p;
  }
  return (self);
}

/*!
\brief deprocedure PROC in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_deproceduring (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_PROCEDURE *z, u;
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  NODE_T *pr = SUB (p);
  MOID_T *pr_mode = MOID (pr);
  self.unit = genie_deproceduring;
  self.source = p;
/* Get procedure. */
  GENIE_GET_OPR (pr, A68_PROCEDURE, z, u);
  CHECK_INIT_GENERIC (p, (BYTE_T *) z, pr_mode);
  genie_call_procedure (p, pr_mode, pr_mode, MODE (VOID), z, pop_sp, pop_fp);
  PROTECT_FROM_SWEEP_STACK (p);
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "deproceduring");
  return (self);
}

/*!
\brief voiden value in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_voiding (NODE_T * p)
{
  PROPAGATOR_T self, source;
  ADDR_T sp_for_voiding = stack_pointer;
  A68_TRACE ("enter genie_voiding", p);
  self.source = p;
  EXECUTE_UNIT_2 (SUB (p), source);
  stack_pointer = sp_for_voiding;
  if (source.unit == genie_assignation_quick) {
    self.unit = genie_voiding_assignation;
    self.source = source.source;
  } else if (source.unit == genie_assignation_constant) {
    self.unit = genie_voiding_assignation_constant;
    self.source = source.source;
  } else {
    self.unit = genie_voiding;
  }
  A68_TRACE ("exit genie_voiding", p);
  return (self);
}

/*!
\brief coerce value in the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_coercion (NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = genie_coercion;
  self.source = p;
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
  PROPAGATOR (p) = self;
  return (self);
}

/*!
\brief push argument units
\param p position in tree
\param seq chain to link nodes into
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
\param p position in tree
\param pr_mode full mode of procedure object
\param pproc mode of resulting proc
\param pmap mode of the locale
\param z procedure object to call
\param pop_sp stack pointer value to restore
\param pop_fp frame pointer value to restore
**/

void genie_partial_call (NODE_T * p, MOID_T * pr_mode, MOID_T * pproc, MOID_T * pmap, A68_PROCEDURE z, ADDR_T pop_sp, ADDR_T pop_fp)
{
  int voids = 0;
  BYTE_T *u, *v;
  PACK_T *s, *t;
  A68_REF ref;
  A68_HANDLE *loc;
/* Get locale for the new procedure descriptor. Copy is necessary. */
  if (LOCALE (&z) == NULL) {
    int size = 0;
    for (s = PACK (pr_mode); s != NULL; FORWARD (s)) {
      size += (ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s)));
    }
    ref = heap_generator (p, pr_mode, size);
    loc = REF_HANDLE (&ref);
  } else {
    int size = LOCALE (&z)->size;
    ref = heap_generator (p, pr_mode, size);
    loc = REF_HANDLE (&ref);
    COPY (POINTER (loc), POINTER (LOCALE (&z)), size);
  }
/* Move arguments from stack to locale using pmap. */
  u = POINTER (loc);
  s = PACK (pr_mode);
  v = STACK_ADDRESS (pop_sp);
  t = PACK (pmap);
  for (; t != NULL && s != NULL; FORWARD (t)) {
/* Skip already initialised arguments. */
    while (u != NULL && VALUE ((A68_BOOL *) & u[0])) {
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      FORWARD (s);
    }
    if (u != NULL && MOID (t) == MODE (VOID)) {
/* Move to next field in locale. */
      voids++;
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      FORWARD (s);
    } else {
/* Move argument from stack to locale. */
      A68_BOOL w;
      STATUS (&w) = INITIALISED_MASK;
      VALUE (&w) = A68_TRUE;
      *(A68_BOOL *) & u[0] = w;
      COPY (&(u[ALIGNED_SIZE_OF (A68_BOOL)]), v, MOID_SIZE (MOID (t)));
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      v = &(v[MOID_SIZE (MOID (t))]);
      FORWARD (s);
    }
  }
  stack_pointer = pop_sp;
  LOCALE (&z) = loc;
/* Is closure complete? */
  if (voids == 0) {
/* Closure is complete. Push locale onto the stack and call procedure body. */
    stack_pointer = pop_sp;
    u = POINTER (loc);
    v = STACK_ADDRESS (stack_pointer);
    s = PACK (pr_mode);
    for (; s != NULL; FORWARD (s)) {
      int size = MOID_SIZE (MOID (s));
      COPY (v, &u[ALIGNED_SIZE_OF (A68_BOOL)], size);
      u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + size]);
      v = &(v[MOID_SIZE (MOID (s))]);
      INCREMENT_STACK_POINTER (p, size);
    }
    genie_call_procedure (p, pr_mode, pproc, MODE (VOID), &z, pop_sp, pop_fp);
  } else {
/*  Closure is not complete. Return procedure body. */
    PUSH_PROCEDURE (p, z);
  }
}

/*!
\brief closure and deproceduring of routines with PARAMSETY
\param p position in tree
\param pr_mode full mode of procedure object
\param pproc mode of resulting proc
\param pmap mode of the locale
\param z procedure object to call
\param pop_sp stack pointer value to restore
\param pop_fp frame pointer value to restore
**/

void genie_call_procedure (NODE_T * p, MOID_T * pr_mode, MOID_T * pproc, MOID_T * pmap, A68_PROCEDURE * z, ADDR_T pop_sp, ADDR_T pop_fp)
{
  if (pmap != MODE (VOID) && pr_mode != pmap) {
    genie_partial_call (p, pr_mode, pproc, pmap, *z, pop_sp, pop_fp);
  } else if (STATUS (z) & STANDENV_PROC_MASK) {
    (void) ((*((BODY (z)).proc)) (p));
  } else if (STATUS (z) & SKIP_PROCEDURE_MASK) {
    stack_pointer = pop_sp;
    genie_push_undefined (p, SUB ((MOID (z))));
  } else {
    NODE_T *body = (BODY (z)).node;
    if (WHETHER (body, ROUTINE_TEXT)) {
      NODE_T *entry = SUB (body);
      PACK_T *args = PACK (pr_mode);
      ADDR_T fp0 = 0;
/* Copy arguments from stack to frame. */
      OPEN_PROC_FRAME (entry, ENVIRON (z));
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
      for (; args != NULL; FORWARD (args)) {
        int size = MOID_SIZE (MOID (args));
        COPY ((FRAME_OBJECT (fp0)), STACK_ADDRESS (pop_sp + fp0), size);
        fp0 += size;
      }
      stack_pointer = pop_sp;
      GENIE (p)->argsize = fp0;
/* Interpret routine text. */
      PREEMPTIVE_SWEEP;
      CHECK_TIME_LIMIT (p);
      if (DIM (pr_mode) > 0) {
/* With PARAMETERS. */
        entry = NEXT (NEXT_NEXT (entry));
      } else {
/* Without PARAMETERS. */
        entry = NEXT_NEXT (entry);
      }
      EXECUTE_UNIT (entry);
      if (frame_pointer == finish_frame_pointer) {
        change_masks (MODULE (INFO (p))->top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
      }
      CLOSE_FRAME;
      GENIE_DNS_STACK (p, SUB (pr_mode), frame_pointer, "procedure");
    } else {
      OPEN_PROC_FRAME (body, ENVIRON (z));
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
      EXECUTE_UNIT (body);
      if (frame_pointer == finish_frame_pointer) {
        change_masks (MODULE (INFO (p))->top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
      }
      CLOSE_FRAME;
      GENIE_DNS_STACK (p, SUB (pr_mode), frame_pointer, "procedure");
    }
  }
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_call_standenv_quick (NODE_T * p)
{
  NODE_T *pr = SUB (p), *q = SEQUENCE (p);
  TAG_T *proc = TAX (PROPAGATOR (pr).source);
/* Get arguments. */
  UP_SWEEP_SEMA;
  for (; q != NULL; q = SEQUENCE (q)) {
    EXECUTE_UNIT_INLINE (q);
  }
  DOWN_SWEEP_SEMA;
  (void) ((*(proc->procedure)) (p));
  return (PROPAGATOR (p));
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_call_quick (NODE_T * p)
{
  A68_PROCEDURE *z, u;
  NODE_T *pr = SUB (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
/* Get procedure. */
  GENIE_GET_OPR (pr, A68_PROCEDURE, z, u);
  CHECK_INIT_GENERIC (p, (BYTE_T *) z, MOID (pr));
/* Get arguments. */
  if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GENIE_INFO_T g;
    GENIE (&top_seq) = &g;
    genie_argument (NEXT (pr), &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
  } else {
    NODE_T *q = SEQUENCE (p);
    for (; q != NULL; q = SEQUENCE (q)) {
      EXECUTE_UNIT_INLINE (q);
    }
  }
  genie_call_procedure (p, MOID (z), GENIE (pr)->partial_proc, GENIE (pr)->partial_locale, z, pop_sp, pop_fp);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief call PROC with arguments and push result
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_call (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_PROCEDURE *z, u;
  NODE_T *pr = SUB (p);
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  self.unit = genie_call_quick;
  self.source = p;
/* Get procedure. */
  GENIE_GET_OPR (pr, A68_PROCEDURE, z, u);
  CHECK_INIT_GENERIC (p, (BYTE_T *) z, MOID (pr));
/* Get arguments. */
  if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GENIE_INFO_T g;
    GENIE (&top_seq) = &g;
    genie_argument (NEXT (pr), &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
  } else {
    NODE_T *q = SEQUENCE (p);
    for (; q != NULL; q = SEQUENCE (q)) {
      EXECUTE_UNIT_INLINE (q);
    }
  }
  genie_call_procedure (p, MOID (z), GENIE (pr)->partial_proc, GENIE (pr)->partial_locale, z, pop_sp, pop_fp);
  if (GENIE (pr)->partial_locale != MODE (VOID) && MOID (z) != GENIE (pr)->partial_locale) {
    /* skip */ ;
  } else if ((STATUS (z) & STANDENV_PROC_MASK) && (GENIE (p)->protect_sweep == NULL)) {
    if (PROPAGATOR (pr).unit == genie_identifier_standenv_proc) {
      self.unit = genie_call_standenv_quick;
    }
  }
  PROTECT_FROM_SWEEP_STACK (p);
  return (self);
}

/*!
\brief construct a descriptor "ref_new" for a trim of "ref_old"
\param p position in tree
\param ref_new new descriptor
\param ref_old old descriptor
\param offset calculates the offset of the trim
**/

static void genie_trimmer (NODE_T * p, BYTE_T * *ref_new, BYTE_T * *ref_old, int *offset)
{
  if (p != NULL) {
    if (WHETHER (p, UNIT)) {
      A68_INT k;
      A68_TUPLE *t;
      EXECUTE_UNIT (p);
      POP_OBJECT (p, &k, A68_INT);
      t = (A68_TUPLE *) * ref_old;
      if (VALUE (&k) < LWB (t) || VALUE (&k) > UPB (t)) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      (*offset) += t->span * VALUE (&k) - t->shift;
      /*
       * (*ref_old) += ALIGNED_SIZE_OF (A68_TUPLE);
       */
      (*ref_old) += sizeof (A68_TUPLE);
    } else if (WHETHER (p, TRIMMER)) {
      A68_INT k;
      NODE_T *q;
      int L, U, D;
      A68_TUPLE *old_tup = (A68_TUPLE *) * ref_old;
      A68_TUPLE *new_tup = (A68_TUPLE *) * ref_new;
/* TRIMMER is (l:u@r) with all units optional or (empty). */
      q = SUB (p);
      if (q == NULL) {
        L = LWB (old_tup);
        U = UPB (old_tup);
        D = 0;
      } else {
        BOOL_T absent = A68_TRUE;
/* Lower index. */
        if (q != NULL && WHETHER (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_OBJECT (p, &k, A68_INT);
          if (VALUE (&k) < LWB (old_tup)) {
            diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
          }
          L = VALUE (&k);
          FORWARD (q);
          absent = A68_FALSE;
        } else {
          L = LWB (old_tup);
        }
        if (q != NULL && (WHETHER (q, COLON_SYMBOL)
                          || WHETHER (q, DOTDOT_SYMBOL))) {
          FORWARD (q);
          absent = A68_FALSE;
        }
/* Upper index. */
        if (q != NULL && WHETHER (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_OBJECT (p, &k, A68_INT);
          if (VALUE (&k) > UPB (old_tup)) {
            diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
            exit_genie (p, A68_RUNTIME_ERROR);
          }
          U = VALUE (&k);
          FORWARD (q);
          absent = A68_FALSE;
        } else {
          U = UPB (old_tup);
        }
        if (q != NULL && WHETHER (q, AT_SYMBOL)) {
          FORWARD (q);
        }
/* Revised lower bound. */
        if (q != NULL && WHETHER (q, UNIT)) {
          EXECUTE_UNIT (q);
          POP_OBJECT (p, &k, A68_INT);
          D = L - VALUE (&k);
          FORWARD (q);
        } else {
          D = (absent ? 0 : L - 1);
        }
      }
      LWB (new_tup) = L - D;
      UPB (new_tup) = U - D;    /* (L - D) + (U - L) */
      new_tup->span = old_tup->span;
      new_tup->shift = old_tup->shift - D * new_tup->span;
      (*ref_old) += sizeof (A68_TUPLE);
      (*ref_new) += sizeof (A68_TUPLE);
    } else {
      genie_trimmer (SUB (p), ref_new, ref_old, offset);
      genie_trimmer (NEXT (p), ref_new, ref_old, offset);
    }
  }
}

/*!
\brief calculation of subscript
\param p position in tree
\param ref_heap
\param sum calculates the index of the subscript
\param seq chain to link nodes into
**/

void genie_subscript (NODE_T * p, A68_TUPLE ** tup, int *sum, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        A68_INT *k;
        A68_TUPLE *t = *tup;
        EXECUTE_UNIT (p);
        POP_ADDRESS (p, k, A68_INT);
        if (VALUE (k) < LWB (t) || VALUE (k) > UPB (t)) {
          diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
        (*tup)++;
        (*sum) += (t->span * VALUE (k) - t->shift);
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

/*!
\brief slice REF [] A to REF A
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_slice_name_quick (NODE_T * p)
{
  NODE_T *q, *pr = SUB (p);
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_ARRAY *a;
  ADDR_T pop_sp, scope;
  A68_TUPLE *t;
  int sindex;
/* Get row and save row from sweeper. */
  UP_SWEEP_SEMA;
  EXECUTE_UNIT_INLINE (pr);
  CHECK_REF (p, *z, MOID (SUB (p)));
  GET_DESCRIPTOR (a, t, (A68_ROW *) ADDRESS (z));
  pop_sp = stack_pointer;
  for (sindex = 0, q = SEQUENCE (p); q != NULL; t++, q = SEQUENCE (q)) {
    A68_INT *j;
    int k;
    GENIE_GET_UNIT_ADDRESS (q, A68_INT, j);
    k = VALUE (j);
    if (k < LWB (t) || k > UPB (t)) {
      diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (q, A68_RUNTIME_ERROR);
    }
    sindex += (t->span * k - t->shift);
    stack_pointer = pop_sp;
  }
  DOWN_SWEEP_SEMA;
/* Leave reference to element on the stack, preserving scope. */
  scope = GET_REF_SCOPE (z);
  *z = ARRAY (a);
  OFFSET (z) += ROW_ELEMENT (a, sindex);
  SET_REF_SCOPE (z, scope);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief push slice of a rowed object
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_slice (NODE_T * p)
{
  PROPAGATOR_T self, primary;
  ADDR_T pop_sp, scope = PRIMAL_SCOPE;
  BOOL_T slice_of_name = (BOOL_T) (WHETHER (MOID (SUB (p)), REF_SYMBOL));
  MOID_T *result_moid = slice_of_name ? SUB (MOID (p)) : MOID (p);
  NODE_T *indexer = NEXT_SUB (p);
  A68_TRACE ("enter genie_slice", p);
  self.unit = genie_slice;
  self.source = p;
  pop_sp = stack_pointer;
/* Get row. */
  UP_SWEEP_SEMA;
  EXECUTE_UNIT_2 (SUB (p), primary);
/* In case of slicing a REF [], we need the [] internally, so dereference. */
  if (slice_of_name) {
    A68_REF z;
    POP_REF (p, &z);
    A68_PRINT_REF ("implicit deference", &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    scope = GET_REF_SCOPE (&z);
    PUSH_REF (p, *(A68_REF *) ADDRESS (&z));
  }
  if (ANNOTATION (indexer) == SLICE) {
/* SLICING subscripts one element from an array. */
    A68_REF z;
    A68_ARRAY *a;
    A68_TUPLE *t;
    int sindex;
    POP_REF (p, &z);
    CHECK_REF (p, z, MOID (SUB (p)));
    GET_DESCRIPTOR (a, t, &z);
    if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      GENIE_INFO_T g;
      GENIE (&top_seq) = &g;
      sindex = 0;
      genie_subscript (indexer, &t, &sindex, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      STATUS_SET (p, SEQUENCE_MASK);
    } else {
      NODE_T *q;
      for (sindex = 0, q = SEQUENCE (p); q != NULL; t++, q = SEQUENCE (q)) {
        A68_INT *j;
        int k;
        GENIE_GET_UNIT_ADDRESS (q, A68_INT, j);
        k = VALUE (j);
        if (k < LWB (t) || k > UPB (t)) {
          diagnostic_node (A68_RUNTIME_ERROR, q, ERROR_INDEX_OUT_OF_BOUNDS);
          exit_genie (q, A68_RUNTIME_ERROR);
        }
        sindex += (t->span * k - t->shift);
      }
    }
/* Slice of a name yields a name. */
    stack_pointer = pop_sp;
    if (slice_of_name) {
      A68_REF name = ARRAY (a);
      name.offset += ROW_ELEMENT (a, sindex);
      SET_REF_SCOPE (&name, scope);
      PUSH_REF (p, name);
      if (STATUS_TEST (p, SEQUENCE_MASK)) {
        self.unit = genie_slice_name_quick;
        self.source = p;
      }
    } else {
      PUSH (p, &((ADDRESS (&(ARRAY (a))))[ROW_ELEMENT (a, sindex)]), MOID_SIZE (result_moid));
    }
    PROTECT_FROM_SWEEP_STACK (p);
    DOWN_SWEEP_SEMA;
    A68_TRACE ("exit genie_slice (slice)", p);
    return (self);
  } else if (ANNOTATION (indexer) == TRIMMER) {
/* Trimming selects a subarray from an array. */
    int offset;
    A68_REF z, ref_desc_copy;
    A68_ARRAY *old_des, *new_des;
    BYTE_T *ref_new, *ref_old;
    ref_desc_copy = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_ARRAY) + DIM (DEFLEX (result_moid)) * ALIGNED_SIZE_OF (A68_TUPLE));
/* Get descriptor. */
    POP_REF (p, &z);
/* Get indexer. */
    CHECK_REF (p, z, MOID (SUB (p)));
    old_des = (A68_ARRAY *) ADDRESS (&z);
    new_des = (A68_ARRAY *) ADDRESS (&ref_desc_copy);
    ref_old = ADDRESS (&z) + ALIGNED_SIZE_OF (A68_ARRAY);
    ref_new = ADDRESS (&ref_desc_copy) + ALIGNED_SIZE_OF (A68_ARRAY);
    DIM (new_des) = DIM (DEFLEX (result_moid));
    MOID (new_des) = MOID (old_des);
    new_des->elem_size = old_des->elem_size;
    offset = old_des->slice_offset;
    genie_trimmer (indexer, &ref_new, &ref_old, &offset);
    new_des->slice_offset = offset;
    new_des->field_offset = old_des->field_offset;
    ARRAY (new_des) = ARRAY (old_des);
/* Trim of a name is a name. */
    if (slice_of_name) {
      A68_REF ref_new2 = heap_generator (p, MOID (p), ALIGNED_SIZE_OF (A68_REF));
      *(A68_REF *) ADDRESS (&ref_new2) = ref_desc_copy;
      SET_REF_SCOPE (&ref_new2, scope);
      PUSH_REF (p, ref_new2);
    } else {
      PUSH_REF (p, ref_desc_copy);
    }
    PROTECT_FROM_SWEEP_STACK (p);
    DOWN_SWEEP_SEMA;
    A68_TRACE ("exit genie_slice (trimmer)", p);
    return (self);
  } else {
    ABNORMAL_END (A68_TRUE, "impossible state in genie_slice", NULL);
    return (self);
  }
}

/*!
\brief push value of denotation
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_denotation (NODE_T * p)
{
  MOID_T *moid = MOID (p);
  PROPAGATOR_T self;
  self.unit = genie_denotation;
  self.source = p;
  if (moid == MODE (INT)) {
/* INT denotation. */
    A68_INT z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    self.unit = genie_constant;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | CONSTANT_MASK);
    GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_INT));
    GENIE (p)->size = ALIGNED_SIZE_OF (A68_INT);
    COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_INT));
    PUSH_PRIMITIVE (p, VALUE ((A68_INT *) (GENIE (p)->constant)), A68_INT);
  } else if (moid == MODE (REAL)) {
/* REAL denotation. */
    A68_REAL z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z)
        == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | CONSTANT_MASK);
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_REAL));
    GENIE (p)->size = ALIGNED_SIZE_OF (A68_REAL);
    COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_REAL));
    PUSH_PRIMITIVE (p, VALUE ((A68_REAL *) (GENIE (p)->constant)), A68_REAL);
  } else if (moid == MODE (LONG_INT) || moid == MODE (LONGLONG_INT)) {
/* [LONG] LONG INT denotation. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (WHETHER (SUB (p), SHORTETY) || WHETHER (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (double) (INITIALISED_MASK | CONSTANT_MASK);
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, z, size);
  } else if (moid == MODE (LONG_REAL) || moid == MODE (LONGLONG_REAL)) {
/* [LONG] LONG REAL denotation. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (WHETHER (SUB (p), SHORTETY) || WHETHER (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (MP_DIGIT_T) (INITIALISED_MASK | CONSTANT_MASK);
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, z, size);
  } else if (moid == MODE (BITS)) {
/* BITS denotation. */
    A68_BITS z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, moid, SYMBOL (s), (BYTE_T *) & z)
        == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    self.unit = genie_constant;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | CONSTANT_MASK);
    GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_BITS));
    GENIE (p)->size = ALIGNED_SIZE_OF (A68_BITS);
    COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_BITS));
    PUSH_PRIMITIVE (p, VALUE ((A68_BITS *) (GENIE (p)->constant)), A68_BITS);
  } else if (moid == MODE (LONG_BITS) || moid == MODE (LONGLONG_BITS)) {
/* [LONG] LONG BITS denotation. */
    int digits = get_mp_digits (moid);
    MP_DIGIT_T *z;
    int size = get_mp_size (moid);
    NODE_T *number;
    if (WHETHER (SUB (p), SHORTETY) || WHETHER (SUB (p), LONGETY)) {
      number = NEXT_SUB (p);
    } else {
      number = SUB (p);
    }
    STACK_MP (z, p, digits);
    if (genie_string_to_value_internal (p, moid, SYMBOL (number), (BYTE_T *) z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, moid);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z[0] = (MP_DIGIT_T) (INITIALISED_MASK | CONSTANT_MASK);
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, z, size);
  } else if (moid == MODE (BOOL)) {
/* BOOL denotation. */
    A68_BOOL z;
    CHECK_RETVAL (genie_string_to_value_internal (p, MODE (BOOL), SYMBOL (p), (BYTE_T *) & z) == A68_TRUE);
    PUSH_PRIMITIVE (p, VALUE (&z), A68_BOOL);
  } else if (moid == MODE (CHAR)) {
/* CHAR denotation. */
    PUSH_PRIMITIVE (p, SYMBOL (p)[0], A68_CHAR);
  } else if (moid == MODE (ROW_CHAR)) {
/* [] CHAR denotation. */
/* Make a permanent string in the heap. */
    A68_REF z;
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    z = c_to_a_string (p, SYMBOL (p));
    GET_DESCRIPTOR (arr, tup, &z);
    PROTECT_SWEEP_HANDLE (&z);
    PROTECT_SWEEP_HANDLE (&(ARRAY (arr)));
    self.unit = genie_constant;
    GENIE (p)->constant = (void *) get_heap_space ((size_t) ALIGNED_SIZE_OF (A68_REF));
    GENIE (p)->size = ALIGNED_SIZE_OF (A68_REF);
    COPY (GENIE (p)->constant, &z, ALIGNED_SIZE_OF (A68_REF));
    PUSH_REF (p, *(A68_REF *) (GENIE (p)->constant));
  } else if (moid == MODE (VOID)) {
/* VOID denotation: EMPTY. */
    ;
  }
  return (self);
}

/*!
\brief push a local identifier
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_frame_identifier (NODE_T * p)
{
  BYTE_T *z;
  FRAME_GET (z, BYTE_T, p);
  PUSH (p, z, MOID_SIZE (MOID (p)));
  return (PROPAGATOR (p));
}

/*!
\brief push standard environ routine as PROC
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier_standenv_proc (NODE_T * p)
{
  A68_PROCEDURE z;
  TAG_T *q = TAX (p);
  STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | STANDENV_PROC_MASK);
  (BODY (&z)).proc = q->procedure;
  ENVIRON (&z) = 0;
  LOCALE (&z) = NULL;
  MOID (&z) = MOID (p);
  PUSH_PROCEDURE (p, z);
  return (PROPAGATOR (p));
}

/*!
\brief (optimised) push identifier from standard environ
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier_standenv (NODE_T * p)
{
  (void) ((*(TAX (p)->procedure)) (p));
  return (PROPAGATOR (p));
}

/*!
\brief push identifier onto the stack
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identifier (NODE_T * p)
{
  static PROPAGATOR_T self;
  TAG_T *q = TAX (p);
  self.source = p;
  if (q->stand_env_proc) {
    if (WHETHER (MOID (q), PROC_SYMBOL)) {
      (void) genie_identifier_standenv_proc (p);
      self.unit = genie_identifier_standenv_proc;
    } else {
      (void) genie_identifier_standenv (p);
      self.unit = genie_identifier_standenv;
    }
  } else if (STATUS_TEST (q, CONSTANT_MASK)) {
    int size = MOID_SIZE (MOID (p));
    BYTE_T *sp_0 = STACK_TOP;
    (void) genie_frame_identifier (p);
    GENIE (p)->constant = (void *) get_heap_space ((size_t) size);
    GENIE (p)->size = size;
    COPY (GENIE (p)->constant, (void *) sp_0, size);
    self.unit = genie_constant;
  } else {
    (void) genie_frame_identifier (p);
    self.unit = genie_frame_identifier;
  }
  return (self);
}

/*!
\brief push result of cast (coercions are deeper in the tree)
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_cast (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_TRACE ("enter genie_cast", p);
  EXECUTE_UNIT (NEXT_SUB (p));
  A68_TRACE ("exit genie_cast", p);
  self.unit = genie_cast;
  self.source = p;
  return (self);
}

/*!
\brief execute assertion
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_assertion (NODE_T * p)
{
  PROPAGATOR_T self;
  if (STATUS_TEST (p, ASSERT_MASK)) {
    A68_BOOL z;
    EXECUTE_UNIT (NEXT_SUB (p));
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FALSE_ASSERTION);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  self.unit = genie_assertion;
  self.source = p;
  return (self);
}

/*!
\brief push format text
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_format_text (NODE_T * p)
{
  static PROPAGATOR_T self;
  A68_FORMAT z = *(A68_FORMAT *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_FORMAT (p, z);
  self.unit = genie_format_text;
  self.source = p;
  return (self);
}

/*!
\brief SELECTION from a value
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_selection_value_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *result_mode = MOID (selector);
  ADDR_T old_stack_pointer = stack_pointer;
  int size = MOID_SIZE (result_mode);
  int offset = OFFSET (NODE_PACK (SUB (selector)));
  EXECUTE_UNIT_INLINE (NEXT (selector));
  stack_pointer = old_stack_pointer;
  if (offset > 0) {
    MOVE (STACK_TOP, STACK_OFFSET (offset), (unsigned) size);
  }
  INCREMENT_STACK_POINTER (selector, size);
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief SELECTION from a name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_selection_name_quick (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  MOID_T *struct_mode = MOID (NEXT (selector));
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT_INLINE (NEXT (selector));
  CHECK_REF (selector, *z, struct_mode);
  OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief push selection from secondary
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_selection (NODE_T * p)
{
  NODE_T *selector = SUB (p);
  PROPAGATOR_T self;
  MOID_T *struct_mode = MOID (NEXT (selector)), *result_mode = MOID (selector);
  BOOL_T selection_of_name = (BOOL_T) (WHETHER (struct_mode, REF_SYMBOL));
  self.source = p;
  self.unit = genie_selection;
  EXECUTE_UNIT (NEXT (selector));
/* Multiple selections. */
  if (selection_of_name && (WHETHER (SUB (struct_mode), FLEX_SYMBOL) || WHETHER (SUB (struct_mode), ROW_SYMBOL))) {
    A68_REF *row1, row2, row3;
    int dims, desc_size;
    UP_SWEEP_SEMA;
    POP_ADDRESS (selector, row1, A68_REF);
    CHECK_REF (p, *row1, struct_mode);
    row1 = (A68_REF *) ADDRESS (row1);
    dims = DIM (DEFLEX (SUB (struct_mode)));
    desc_size = ALIGNED_SIZE_OF (A68_ARRAY) + dims * ALIGNED_SIZE_OF (A68_TUPLE);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), (BYTE_T *) ADDRESS (row1), (unsigned) desc_size);
    MOID (((A68_ARRAY *) ADDRESS (&row2))) = SUB (SUB (result_mode));
    ((A68_ARRAY *) ADDRESS (&row2))->field_offset += OFFSET (NODE_PACK (SUB (selector)));
    row3 = heap_generator (selector, result_mode, ALIGNED_SIZE_OF (A68_REF));
    *(A68_REF *) ADDRESS (&row3) = row2;
    PUSH_REF (selector, row3);
    self.unit = genie_selection;
    DOWN_SWEEP_SEMA;
    PROTECT_FROM_SWEEP_STACK (p);
  } else if (struct_mode != NULL && (WHETHER (struct_mode, FLEX_SYMBOL) || WHETHER (struct_mode, ROW_SYMBOL))) {
    A68_REF *row1, row2;
    int dims, desc_size;
    UP_SWEEP_SEMA;
    POP_ADDRESS (selector, row1, A68_REF);
    dims = DIM (DEFLEX (struct_mode));
    desc_size = ALIGNED_SIZE_OF (A68_ARRAY) + dims * ALIGNED_SIZE_OF (A68_TUPLE);
    row2 = heap_generator (selector, result_mode, desc_size);
    MOVE (ADDRESS (&row2), (BYTE_T *) ADDRESS (row1), (unsigned) desc_size);
    MOID (((A68_ARRAY *) ADDRESS (&row2))) = SUB (result_mode);
    ((A68_ARRAY *) ADDRESS (&row2))->field_offset += OFFSET (NODE_PACK (SUB (selector)));
    PUSH_REF (selector, row2);
    self.unit = genie_selection;
    DOWN_SWEEP_SEMA;
    PROTECT_FROM_SWEEP_STACK (p);
  }
/* Normal selections. */
  else if (selection_of_name && WHETHER (SUB (struct_mode), STRUCT_SYMBOL)) {
    A68_REF *z = (A68_REF *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REF)));
    CHECK_REF (selector, *z, struct_mode);
    OFFSET (z) += OFFSET (NODE_PACK (SUB (selector)));
    self.unit = genie_selection_name_quick;
    PROTECT_FROM_SWEEP_STACK (p);
  } else if (WHETHER (struct_mode, STRUCT_SYMBOL)) {
    DECREMENT_STACK_POINTER (selector, MOID_SIZE (struct_mode));
    MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (SUB (selector)))), (unsigned) MOID_SIZE (result_mode));
    INCREMENT_STACK_POINTER (selector, MOID_SIZE (result_mode));
    self.unit = genie_selection_value_quick;
    PROTECT_FROM_SWEEP_STACK (p);
  }
  return (self);
}

/*!
\brief push selection from primary
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_field_selection (NODE_T * p)
{
  PROPAGATOR_T self;
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  NODE_T *entry = p;
  A68_REF *z = (A68_REF *) STACK_TOP;
  A68_PROCEDURE *w = (A68_PROCEDURE *) STACK_TOP;
  self.source = entry;
  self.unit = genie_field_selection;
  EXECUTE_UNIT (SUB (p));
  for (p = SEQUENCE (SUB (p)); p != NULL; p = SEQUENCE (p)) {
    BOOL_T coerce = A68_TRUE;
    MOID_T *m = MOID (p);
    MOID_T *result_mode = MOID (NODE_PACK (p));
    while (coerce) {
      if (WHETHER (m, REF_SYMBOL) && WHETHER_NOT (SUB (m), STRUCT_SYMBOL)) {
        int size = MOID_SIZE (SUB (m));
        stack_pointer = pop_sp;
        CHECK_REF (p, *z, m);
        PUSH (p, ADDRESS (z), size);
        CHECK_INIT_GENERIC (p, STACK_OFFSET (-size), MOID (p));
        m = SUB (m);
      } else if (WHETHER (m, PROC_SYMBOL)) {
        CHECK_INIT_GENERIC (p, (BYTE_T *) w, m);
        genie_call_procedure (p, m, m, MODE (VOID), w, pop_sp, pop_fp);
        GENIE_DNS_STACK (p, MOID (p), frame_pointer, "deproceduring");
        m = SUB (m);
      } else {
        coerce = A68_FALSE;
      }
    }
    if (WHETHER (m, REF_SYMBOL) && WHETHER (SUB (m), STRUCT_SYMBOL)) {
      CHECK_REF (p, *z, m);
      OFFSET (z) += OFFSET (NODE_PACK (p));
    } else if (WHETHER (m, STRUCT_SYMBOL)) {
      stack_pointer = pop_sp;
      MOVE (STACK_TOP, STACK_OFFSET (OFFSET (NODE_PACK (p))), (unsigned) MOID_SIZE (result_mode));
      INCREMENT_STACK_POINTER (p, MOID_SIZE (result_mode));
    }
  }
  PROTECT_FROM_SWEEP_STACK (entry);
  return (self);
}

/*!
\brief call operator
\param p position in tree
\param pop_sp stack pointer value to restore
**/

void genie_call_operator (NODE_T * p, ADDR_T pop_sp)
{
  A68_PROCEDURE *z;
  ADDR_T pop_fp = frame_pointer;
  MOID_T *pr_mode = MOID (TAX (p));
  FRAME_GET (z, A68_PROCEDURE, p);
  genie_call_procedure (p, pr_mode, MOID (z), pr_mode, z, pop_sp, pop_fp);
}

/*!
\brief push result of monadic formula OP "u"
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_monadic (NODE_T * p)
{
  NODE_T *op = SUB (p);
  NODE_T *u = NEXT (op);
  PROPAGATOR_T self;
  ADDR_T sp = stack_pointer;
  EXECUTE_UNIT_INLINE (u);
  if (TAX (op)->procedure != NULL) {
    (void) ((*(TAX (op)->procedure)) (op));
  } else {
    genie_call_operator (op, sp);
  }
  PROTECT_FROM_SWEEP_STACK (p);
  self.unit = genie_monadic;
  self.source = p;
  return (self);
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dyadic_quick (NODE_T * p)
{
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  NODE_T *v = NEXT (op);
  EXECUTE_UNIT_INLINE (u);
  EXECUTE_UNIT_INLINE (v);
  (void) ((*(TAX (op)->procedure)) (op));
  return (PROPAGATOR (p));
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_dyadic (NODE_T * p)
{
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  NODE_T *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  EXECUTE_UNIT (u);
  EXECUTE_UNIT (v);
  if (TAX (op)->procedure != NULL) {
    (void) ((*(TAX (op)->procedure)) (op));
  } else {
    genie_call_operator (op, pop_sp);
  }
  PROTECT_FROM_SWEEP_STACK (p);
  return (PROPAGATOR (p));
}

/*!
\brief push result of formula
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula (NODE_T * p)
{
  PROPAGATOR_T self, lhs, rhs;
  NODE_T *u = SUB (p);
  NODE_T *op = NEXT (u);
  ADDR_T pop_sp = stack_pointer;
  self.unit = genie_formula;
  self.source = p;
  EXECUTE_UNIT_2 (u, lhs);
  if (op != NULL) {
    NODE_T *v = NEXT (op);
    GENIE_PROCEDURE *proc = TAX (op)->procedure;
    EXECUTE_UNIT_2 (v, rhs);
    self.unit = genie_dyadic;
    if (proc != NULL) {
      (void) ((*(proc)) (op));
      if (GENIE (p)->protect_sweep == NULL) {
#if 1
        if (proc == genie_add_int) {
          if (rhs.unit == genie_constant) {
            self.unit = genie_formula_plus_int_constant;
          } else {
            self.unit = genie_formula_plus_int;
          }
        } else if (proc == genie_sub_int) {
          if (rhs.unit == genie_constant) {
            self.unit = genie_formula_minus_int_constant;
          } else {
            self.unit = genie_formula_minus_int;
          }
        } else if (proc == genie_mul_int) {
          self.unit = genie_formula_times_int;
        } else if (proc == genie_over_int) {
          self.unit = genie_formula_over_int;
        } else if (proc == genie_eq_int) {
          self.unit = genie_formula_eq_int;
        } else if (proc == genie_ne_int) {
          self.unit = genie_formula_ne_int;
        } else if (proc == genie_lt_int) {
          self.unit = genie_formula_lt_int;
        } else if (proc == genie_le_int) {
          self.unit = genie_formula_le_int;
        } else if (proc == genie_gt_int) {
          self.unit = genie_formula_gt_int;
        } else if (proc == genie_ge_int) {
          self.unit = genie_formula_ge_int;
        } else if (proc == genie_add_real) {
          self.unit = genie_formula_plus_real;
        } else if (proc == genie_sub_real) {
          self.unit = genie_formula_minus_real;
        } else if (proc == genie_mul_real) {
          self.unit = genie_formula_times_real;
        } else if (proc == genie_div_real) {
          self.unit = genie_formula_div_real;
        } else if (proc == genie_eq_real) {
          self.unit = genie_formula_eq_real;
        } else if (proc == genie_ne_real) {
          self.unit = genie_formula_ne_real;
        } else if (proc == genie_lt_real) {
          self.unit = genie_formula_lt_real;
        } else if (proc == genie_le_real) {
          self.unit = genie_formula_le_real;
        } else if (proc == genie_gt_real) {
          self.unit = genie_formula_gt_real;
        } else if (proc == genie_ge_real) {
          self.unit = genie_formula_ge_real;
        } else {
          self.unit = genie_dyadic_quick;
        }
#else
        self.unit = genie_dyadic_quick;
#endif
      }
    } else {
      genie_call_operator (op, pop_sp);
    }
    PROTECT_FROM_SWEEP_STACK (p);
    return (self);
  } else if (lhs.unit == genie_monadic) {
    return (lhs);
  }
  return (self);
}

/*!
\brief push NIL
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_nihil (NODE_T * p)
{
  PROPAGATOR_T self;
  PUSH_REF (p, nil_ref);
  self.unit = genie_nihil;
  self.source = p;
  return (self);
}

/*!
\brief copies union with stowed components on top of the stack
\param p position in tree
**/

static void genie_copy_union (NODE_T * p)
{
  A68_UNION *u = (A68_UNION *) STACK_TOP;
  MOID_T *v = (MOID_T *) VALUE (u);
  if (v != NULL) {
    int v_size = MOID_SIZE (v);
    INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
    if (WHETHER (v, STRUCT_SYMBOL)) {
      A68_REF old, new_one;
      STATUS (&old) = (STATUS_MASK) (INITIALISED_MASK | IN_STACK_MASK);
      old.offset = stack_pointer;
      REF_HANDLE (&old) = &nil_handle;
      new_one = genie_copy_stowed (old, p, v);
      MOVE (STACK_TOP, ADDRESS (&old), v_size);
    } else if (WHETHER (v, ROW_SYMBOL) || WHETHER (v, FLEX_SYMBOL)) {
      A68_REF new_one, old = *(A68_REF *) STACK_TOP;
      new_one = genie_copy_stowed (old, p, v);
      MOVE (STACK_TOP, &new_one, ALIGNED_SIZE_OF (A68_REF));
    }
    DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_UNION));
  }
}

/*!
\brief copy a sound value, make new copy of sound data
\param p position in tree
\param dst destination
\param src source
**/

void genie_copy_sound (NODE_T * p, BYTE_T * dst, BYTE_T * src)
{
  A68_SOUND *w = (A68_SOUND *) dst;
  BYTE_T *wdata;
  int size = A68_SOUND_DATA_SIZE (w);
  COPY (dst, src, MOID_SIZE (MODE (SOUND)));
  wdata = ADDRESS (&(w->data));
  w->data = heap_generator (p, MODE (SOUND_DATA), size);
  COPY (wdata, ADDRESS (&(w->data)), size);
}

/*!
\brief internal workings of an assignment of stowed objects
\param p position in tree
\param z fat A68 pointer
\param source_moid
**/

static void genie_assign_internal (NODE_T * p, A68_REF * z, MOID_T * source_moid)
{
  if (WHETHER (source_moid, FLEX_SYMBOL) || source_moid == MODE (STRING)) {
/* Assign to FLEX [] AMODE. */
    A68_REF old_one = *(A68_REF *) STACK_TOP;
    *(A68_REF *) ADDRESS (z) = genie_copy_stowed (old_one, p, source_moid);
  } else if (WHETHER (source_moid, ROW_SYMBOL)) {
/* Assign to [] AMODE. */
    A68_REF old_one, dst_one;
    A68_ARRAY *dst_arr, *old_arr;
    A68_TUPLE *dst_tup, *old_tup;
    old_one = *(A68_REF *) STACK_TOP;
    dst_one = *(A68_REF *) ADDRESS (z);
    GET_DESCRIPTOR (dst_arr, dst_tup, &dst_one);
    GET_DESCRIPTOR (old_arr, old_tup, &old_one);
    if (ADDRESS (&(ARRAY (dst_arr))) != ADDRESS (&(ARRAY (old_arr))) && !(source_moid->slice->has_rows)) {
      (void) genie_assign_stowed (old_one, &dst_one, p, source_moid);
    } else {
      A68_REF new_one = genie_copy_stowed (old_one, p, source_moid);
      (void) genie_assign_stowed (new_one, &dst_one, p, source_moid);
    }
  } else if (WHETHER (source_moid, STRUCT_SYMBOL)) {
/* STRUCT with row. */
    A68_REF old_one, new_one;
    STATUS (&old_one) = (STATUS_MASK) (INITIALISED_MASK | IN_STACK_MASK);
    old_one.offset = stack_pointer;
    REF_HANDLE (&old_one) = &nil_handle;
    new_one = genie_copy_stowed (old_one, p, source_moid);
    (void) genie_assign_stowed (new_one, z, p, source_moid);
  } else if (WHETHER (source_moid, UNION_SYMBOL)) {
/* UNION with stowed. */
    genie_copy_union (p);
    COPY (ADDRESS (z), STACK_TOP, MOID_SIZE (source_moid));
  } else if (source_moid == MODE (SOUND)) {
    genie_copy_sound (p, ADDRESS (z), STACK_TOP);
  }
}

/*!
\brief assign a value to a name and voiden
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_voiding_assignation_constant (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = (GENIE (NEXT_NEXT (dst))->propagator).source;
  ADDR_T pop_sp = stack_pointer;
  A68_REF *z = (A68_REF *) STACK_TOP;
  PROPAGATOR_T self;
  self.unit = genie_voiding_assignation_constant;
  self.source = p;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  COPY (ADDRESS (z), GENIE (src)->constant, GENIE (src)->size);
  stack_pointer = pop_sp;
  return (self);
}

/*!
\brief assign a value to a name and voiden
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_voiding_assignation (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *source_moid = SUB (MOID (p));
  ADDR_T pop_sp = stack_pointer, pop_fp = FRAME_DYNAMIC_SCOPE (frame_pointer);
  A68_REF *z, u;
  PROPAGATOR_T self;
  self.unit = genie_voiding_assignation;
  self.source = p;
  UP_SWEEP_SEMA;
  GENIE_GET_OPR (dst, A68_REF, z, u);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DYNAMIC_SCOPE (frame_pointer) = GET_REF_SCOPE (z);
  EXECUTE_UNIT_INLINE (src);
  GENIE_DNS_STACK (src, source_moid, GET_REF_SCOPE (z), "assignation");
  FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
  stack_pointer = pop_sp;
  if (source_moid->has_rows) {
    genie_assign_internal (p, z, source_moid);
  } else {
    COPY_ALIGNED (ADDRESS (z), STACK_TOP, MOID_SIZE (source_moid));
  }
  DOWN_SWEEP_SEMA;
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_assignation_constant (NODE_T * p)
{
  NODE_T *dst = SUB (p);
  NODE_T *src = (GENIE (NEXT_NEXT (dst))->propagator).source;
  A68_REF *z = (A68_REF *) STACK_TOP;
  PROPAGATOR_T self;
  self.unit = genie_assignation_constant;
  self.source = p;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  COPY (ADDRESS (z), GENIE (src)->constant, GENIE (src)->size);
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_assignation_quick (NODE_T * p)
{
  PROPAGATOR_T self;
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *source_moid = SUB (MOID (p));
  int size = MOID_SIZE (source_moid);
  ADDR_T pop_fp = FRAME_DYNAMIC_SCOPE (frame_pointer);
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DYNAMIC_SCOPE (frame_pointer) = GET_REF_SCOPE (z);
  EXECUTE_UNIT (src);
  GENIE_DNS_STACK (src, source_moid, GET_REF_SCOPE (z), "assignation");
  FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (source_moid->has_rows) {
    genie_assign_internal (p, z, source_moid);
  } else {
    COPY (ADDRESS (z), STACK_TOP, size);
  }
  self.unit = genie_assignation_quick;
  self.source = p;
  return (self);
}

/*!
\brief assign a value to a name and push the name
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_assignation (NODE_T * p)
{
  PROPAGATOR_T self, srp;
  NODE_T *dst = SUB (p);
  NODE_T *src = NEXT_NEXT (dst);
  MOID_T *source_moid = SUB (MOID (p));
  int size = MOID_SIZE (source_moid);
  ADDR_T pop_fp = FRAME_DYNAMIC_SCOPE (frame_pointer);
  A68_REF *z = (A68_REF *) STACK_TOP;
  EXECUTE_UNIT (dst);
  CHECK_REF (p, *z, MOID (p));
  FRAME_DYNAMIC_SCOPE (frame_pointer) = GET_REF_SCOPE (z);
  EXECUTE_UNIT_2 (src, srp);
  GENIE_DNS_STACK (src, source_moid, GET_REF_SCOPE (z), "assignation");
  FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_fp;
  DECREMENT_STACK_POINTER (p, size);
  if (source_moid->has_rows) {
    genie_assign_internal (p, z, source_moid);
  } else {
    COPY (ADDRESS (z), STACK_TOP, size);
  }
  if (srp.unit == genie_constant) {
    self.unit = genie_assignation_constant;
  } else {
    self.unit = genie_assignation_quick;
  }
  self.source = p;
  return (self);
}

/*!
\brief push equality of two REFs
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identity_relation_quick (NODE_T * p)
{
  PROPAGATOR_T self;
  NODE_T *lhs = SUB (p), *rhs = NEXT_NEXT (lhs);
  A68_REF x, y;
  self.unit = genie_identity_relation;
  self.source = p;
  EXECUTE_UNIT (lhs);
  POP_REF (p, &y);
  EXECUTE_UNIT (rhs);
  POP_REF (p, &x);
  if (WHETHER (NEXT_SUB (p), IS_SYMBOL)) {
    PUSH_PRIMITIVE (p, (BOOL_T) (ADDRESS (&x) == ADDRESS (&y)), A68_BOOL);
  } else {
    PUSH_PRIMITIVE (p, (BOOL_T) (ADDRESS (&x) != ADDRESS (&y)), A68_BOOL);
  }
  return (self);
}

/*!
\brief push equality of two REFs
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identity_relation (NODE_T * p)
{
  PROPAGATOR_T self, rhp;
  NODE_T *lhs = SUB (p), *rhs = NEXT_NEXT (lhs);
  A68_REF x, y;
  self.unit = genie_identity_relation_quick;
  self.source = p;
  EXECUTE_UNIT (lhs);
  POP_REF (p, &y);
  EXECUTE_UNIT_2 (rhs, rhp);
  POP_REF (p, &x);
  if (WHETHER (NEXT_SUB (p), IS_SYMBOL)) {
    if (rhp.unit == genie_nihil) {
      self.unit = genie_identity_relation_is_nil;
    }
    PUSH_PRIMITIVE (p, (BOOL_T) (ADDRESS (&x) == ADDRESS (&y)), A68_BOOL);
  } else {
    if (rhp.unit == genie_nihil) {
      self.unit = genie_identity_relation_isnt_nil;
    }
    PUSH_PRIMITIVE (p, (BOOL_T) (ADDRESS (&x) != ADDRESS (&y)), A68_BOOL);
  }
  return (self);
}

/*!
\brief push result of ANDF
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_and_function (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_BOOL x;
  EXECUTE_UNIT (SUB (p));
  POP_OBJECT (p, &x, A68_BOOL);
  if (VALUE (&x) == A68_TRUE) {
    EXECUTE_UNIT (NEXT_NEXT (SUB (p)));
  } else {
    PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
  }
  self.unit = genie_and_function;
  self.source = p;
  return (self);
}

/*!
\brief push result of ORF
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_or_function (NODE_T * p)
{
  PROPAGATOR_T self;
  A68_BOOL x;
  EXECUTE_UNIT (SUB (p));
  POP_OBJECT (p, &x, A68_BOOL);
  if (VALUE (&x) == A68_FALSE) {
    EXECUTE_UNIT (NEXT_NEXT (SUB (p)));
  } else {
    PUSH_PRIMITIVE (p, A68_TRUE, A68_BOOL);
  }
  self.unit = genie_or_function;
  self.source = p;
  return (self);
}

/*!
\brief push routine text
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_routine_text (NODE_T * p)
{
  static PROPAGATOR_T self;
  A68_PROCEDURE z = *(A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (p))));
  PUSH_PROCEDURE (p, z);
  self.unit = genie_routine_text;
  self.source = p;
  return (self);
}

/*!
\brief push an undefined value of the required mode
\param p position in tree
\param u mode of object to push
**/

void genie_push_undefined (NODE_T * p, MOID_T * u)
{
/* For primitive modes we push an initialised value. */
  if (u == MODE (VOID)) {
    /* skip */ ;
  } else if (u == MODE (INT)) {
    PUSH_PRIMITIVE (p, (int) (rng_53_bit () * A68_MAX_INT), A68_INT);
  } else if (u == MODE (REAL)) {
    PUSH_PRIMITIVE (p, (rng_53_bit ()), A68_REAL);
  } else if (u == MODE (BOOL)) {
    PUSH_PRIMITIVE (p, (BOOL_T) (rng_53_bit () < 0.5), A68_BOOL);
  } else if (u == MODE (CHAR)) {
    PUSH_PRIMITIVE (p, (char) (32 + 96 * rng_53_bit ()), A68_CHAR);
  } else if (u == MODE (BITS)) {
    PUSH_PRIMITIVE (p, (unsigned) (rng_53_bit () * A68_MAX_UNT), A68_BITS);
  } else if (u == MODE (COMPLEX)) {
    PUSH_COMPLEX (p, rng_53_bit (), rng_53_bit ());
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
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
  } else if (u == MODE (LONG_REAL) || u == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
  } else if (u == MODE (LONG_BITS) || u == MODE (LONGLONG_BITS)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
  } else if (u == MODE (LONG_COMPLEX) || u == MODE (LONGLONG_COMPLEX)) {
    int digits = get_mp_digits (u);
    MP_DIGIT_T *z;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
    STACK_MP (z, p, digits);
    SET_MP_ZERO (z, digits);
    z[0] = (MP_DIGIT_T) INITIALISED_MASK;
  } else if (WHETHER (u, REF_SYMBOL)) {
/* All REFs are NIL. */
    PUSH_REF (p, nil_ref);
  } else if (WHETHER (u, ROW_SYMBOL) || WHETHER (u, FLEX_SYMBOL)) {
/* [] AMODE or FLEX [] AMODE. */
    PUSH_REF (p, empty_row (p, u));
  } else if (WHETHER (u, STRUCT_SYMBOL)) {
/* STRUCT. */
    PACK_T *v;
    for (v = PACK (u); v != NULL; FORWARD (v)) {
      genie_push_undefined (p, MOID (v));
    }
  } else if (WHETHER (u, UNION_SYMBOL)) {
/* UNION. */
    ADDR_T sp = stack_pointer;
    PUSH_UNION (p, MOID (PACK (u)));
    genie_push_undefined (p, MOID (PACK (u)));
    stack_pointer = sp + MOID_SIZE (u);
  } else if (WHETHER (u, PROC_SYMBOL)) {
/* PROC. */
    A68_PROCEDURE z;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | SKIP_PROCEDURE_MASK);
    (BODY (&z)).node = NULL;
    ENVIRON (&z) = 0;
    LOCALE (&z) = NULL;
    MOID (&z) = u;
    PUSH_PROCEDURE (p, z);
  } else if (u == MODE (FORMAT)) {
/* FORMAT etc. - what arbitrary FORMAT could mean anything at all? */
    A68_FORMAT z;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | SKIP_FORMAT_MASK);
    BODY (&z) = NULL;
    ENVIRON (&z) = 0;
    PUSH_FORMAT (p, z);
  } else if (u == MODE (SIMPLOUT)) {
    ADDR_T sp = stack_pointer;
    PUSH_UNION (p, MODE (STRING));
    PUSH_REF (p, c_to_a_string (p, "SKIP"));
    stack_pointer = sp + MOID_SIZE (u);
  } else if (u == MODE (SIMPLIN)) {
    ADDR_T sp = stack_pointer;
    PUSH_UNION (p, MODE (REF_STRING));
    genie_push_undefined (p, MODE (REF_STRING));
    stack_pointer = sp + MOID_SIZE (u);
  } else if (u == MODE (REF_FILE)) {
    PUSH_REF (p, skip_file);
  } else if (u == MODE (FILE)) {
    A68_REF *z = (A68_REF *) STACK_TOP;
    int size = MOID_SIZE (MODE (FILE));
    ADDR_T pop_sp = stack_pointer;
    PUSH_REF (p, skip_file);
    stack_pointer = pop_sp;
    PUSH (p, ADDRESS (z), size);
  } else if (u == MODE (CHANNEL)) {
    PUSH_OBJECT (p, skip_channel, A68_CHANNEL);
  } else if (u == MODE (PIPE)) {
    genie_push_undefined (p, MODE (REF_FILE));
    genie_push_undefined (p, MODE (REF_FILE));
    genie_push_undefined (p, MODE (INT));
  } else if (u == MODE (SOUND)) {
    A68_SOUND *z = (A68_SOUND *) STACK_TOP;
    int size = MOID_SIZE (MODE (SOUND));
    INCREMENT_STACK_POINTER (p, size);
    FILL (z, 0, size);
    STATUS (z) = INITIALISED_MASK;
  } else {
    BYTE_T *_sp_ = STACK_TOP;
    int size = ALIGNED_SIZE_OF (u);
    INCREMENT_STACK_POINTER (p, size);
    FILL (_sp_, 0, size);
  }
}

/*!
\brief push an undefined value of the required mode
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_skip (NODE_T * p)
{
  PROPAGATOR_T self;
  if (MOID (p) != MODE (VOID)) {
    genie_push_undefined (p, MOID (p));
  }
  self.unit = genie_skip;
  self.source = p;
  return (self);
}

/*!
\brief jump to the serial clause where the label is at
\param p position in tree
**/

static void genie_jump (NODE_T * p)
{
/* Stack pointer and frame pointer were saved at target serial clause. */
  NODE_T *jump = SUB (p);
  NODE_T *label = (WHETHER (jump, GOTO_SYMBOL)) ? NEXT (jump) : jump;
  ADDR_T target_frame_pointer = frame_pointer;
  jmp_buf *jump_stat = NULL;
/* Find the stack frame this jump points to. */
  BOOL_T found = A68_FALSE;
  while (target_frame_pointer > 0 && !found) {
    found = (BOOL_T) ((TAG_TABLE (TAX (label)) == SYMBOL_TABLE (FRAME_TREE (target_frame_pointer))) && FRAME_JUMP_STAT (target_frame_pointer) != NULL);
    if (!found) {
      target_frame_pointer = FRAME_STATIC_LINK (target_frame_pointer);
    }
  }
/* Beam us up, Scotty! */
#if defined ENABLE_PAR_CLAUSE
  {
    int curlev = PAR_LEVEL (p), tarlev = PAR_LEVEL (NODE (TAX (label)));
    if (curlev == tarlev) {
/* A jump within the same thread */
      jump_stat = FRAME_JUMP_STAT (target_frame_pointer);
      SYMBOL_TABLE (TAX (label))->jump_to = TAX (label)->unit;
      longjmp (*(jump_stat), 1);
    } else if (curlev > 0 && tarlev == 0) {
/* A jump out of all parallel clauses back into the main program */
      genie_abend_all_threads (p, FRAME_JUMP_STAT (target_frame_pointer), label);
      ABNORMAL_END (A68_TRUE, "should not return from genie_abend_all_threads", NULL);
    } else {
/* A jump between threads is forbidden in Algol68G */
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_LABEL_IN_PAR_CLAUSE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
#else
  jump_stat = FRAME_JUMP_STAT (target_frame_pointer);
  TAG_TABLE (TAX (label))->jump_to = TAX (label)->unit;
  longjmp (*(jump_stat), 1);
#endif
}

/*!
\brief execute a unit, tertiary, secondary or primary
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_unit (NODE_T * p)
{
  A68_TRACE ("enter genie_unit", p);
  if (GENIE (p)->whether_coercion) {
    MODULE (INFO (p))->global_prop = genie_coercion (p);
  } else {
    switch (ATTRIBUTE (p)) {
    case UNIT:
      {
        EXECUTE_UNIT_2 (SUB (p), MODULE (INFO (p))->global_prop);
        break;
      }
    case TERTIARY:
    case SECONDARY:
    case PRIMARY:
      {
        MODULE (INFO (p))->global_prop = genie_unit (SUB (p));
        break;
      }
/* Ex primary. */
    case ENCLOSED_CLAUSE:
      {
        MODULE (INFO (p))->global_prop = genie_enclosed ((volatile NODE_T *) p);
        break;
      }
    case IDENTIFIER:
      {
        MODULE (INFO (p))->global_prop = genie_identifier (p);
        break;
      }
    case CALL:
      {
        MODULE (INFO (p))->global_prop = genie_call (p);
        break;
      }
    case SLICE:
      {
        MODULE (INFO (p))->global_prop = genie_slice (p);
        break;
      }
    case FIELD_SELECTION:
      {
        MODULE (INFO (p))->global_prop = genie_field_selection (p);
        break;
      }
    case DENOTATION:
      {
        MODULE (INFO (p))->global_prop = genie_denotation (p);
        break;
      }
    case CAST:
      {
        MODULE (INFO (p))->global_prop = genie_cast (p);
        break;
      }
    case FORMAT_TEXT:
      {
        MODULE (INFO (p))->global_prop = genie_format_text (p);
        break;
      }
/* Ex secondary. */
    case GENERATOR:
      {
        MODULE (INFO (p))->global_prop = genie_generator (p);
        break;
      }
    case SELECTION:
      {
        MODULE (INFO (p))->global_prop = genie_selection (p);
        break;
      }
/* Ex tertiary. */
    case FORMULA:
      {
        MODULE (INFO (p))->global_prop = genie_formula (p);
        break;
      }
    case MONADIC_FORMULA:
      {
        MODULE (INFO (p))->global_prop = genie_monadic (p);
        break;
      }
    case NIHIL:
      {
        MODULE (INFO (p))->global_prop = genie_nihil (p);
        break;
      }
    case DIAGONAL_FUNCTION:
      {
        MODULE (INFO (p))->global_prop = genie_diagonal_function (p);
        break;
      }
    case TRANSPOSE_FUNCTION:
      {
        MODULE (INFO (p))->global_prop = genie_transpose_function (p);
        break;
      }
    case ROW_FUNCTION:
      {
        MODULE (INFO (p))->global_prop = genie_row_function (p);
        break;
      }
    case COLUMN_FUNCTION:
      {
        MODULE (INFO (p))->global_prop = genie_column_function (p);
        break;
      }
/* Ex unit. */
    case ASSIGNATION:
      {
        MODULE (INFO (p))->global_prop = genie_assignation (p);
        break;
      }
    case IDENTITY_RELATION:
      {
        MODULE (INFO (p))->global_prop = genie_identity_relation (p);
        break;
      }
    case ROUTINE_TEXT:
      {
        MODULE (INFO (p))->global_prop = genie_routine_text (p);
        break;
      }
    case SKIP:
      {
        MODULE (INFO (p))->global_prop = genie_skip (p);
        break;
      }
    case JUMP:
      {
        MODULE (INFO (p))->global_prop.unit = genie_unit;
        MODULE (INFO (p))->global_prop.source = p;
        genie_jump (p);
        break;
      }
    case AND_FUNCTION:
      {
        MODULE (INFO (p))->global_prop = genie_and_function (p);
        break;
      }
    case OR_FUNCTION:
      {
        MODULE (INFO (p))->global_prop = genie_or_function (p);
        break;
      }
    case ASSERTION:
      {
        MODULE (INFO (p))->global_prop = genie_assertion (p);
        break;
      }
    }
  }
  A68_TRACE ("exit genie_unit", p);
  return (PROPAGATOR (p) = MODULE (INFO (p))->global_prop);
}

/*!
\brief execution of serial clause without labels
\param p position in tree
\param p position in treeop_sp
\param seq chain to link nodes into
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
\param p position in tree
\param jump_to indicates node to jump to after jump
\param exit_buf jump buffer for EXITs
\param p position in treeop_sp
**/

void genie_serial_units (NODE_T * p, NODE_T ** jump_to, jmp_buf * exit_buf, int pop_sp)
{
  LOW_STACK_ALERT (p);
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
\param p position in tree
\param exit_buf jump buffer for EXITs
**/

void genie_serial_clause (NODE_T * p, jmp_buf * exit_buf)
{
  if (SYMBOL_TABLE (p)->labels == NULL) {
/* No labels in this clause. */
    if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
      NODE_T top_seq;
      NODE_T *seq = &top_seq;
      GENIE_INFO_T g;
      GENIE (&top_seq) = &g;
      genie_serial_units_no_label (SUB (p), stack_pointer, &seq);
      SEQUENCE (p) = SEQUENCE (&top_seq);
      STATUS_SET (p, SEQUENCE_MASK);
      STATUS_SET (p, SERIAL_MASK);
      if (SEQUENCE (p) != NULL && SEQUENCE (SEQUENCE (p)) == NULL) {
        STATUS_SET (p, OPTIMAL_MASK);
      }
    } else {
/* A linear list without labels. */
      NODE_T *q;
      ADDR_T pop_sp = stack_pointer;
      STATUS_SET (p, SERIAL_CLAUSE);
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
      stack_pointer = pop_sp;
      frame_pointer = pop_fp;
      FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
      genie_serial_units (SUB (p), &jump_to, exit_buf, stack_pointer);
    }
  }
}

/*!
\brief execute enquiry clause
\param p position in tree
**/

void genie_enquiry_clause (NODE_T * p)
{
  if (SEQUENCE (p) == NULL && ! STATUS_TEST (p, SEQUENCE_MASK)) {
    NODE_T top_seq;
    NODE_T *seq = &top_seq;
    GENIE_INFO_T g;
    GENIE (&top_seq) = &g;
    genie_serial_units_no_label (SUB (p), stack_pointer, &seq);
    SEQUENCE (p) = SEQUENCE (&top_seq);
    STATUS_SET (p, SEQUENCE_MASK);
    if (SEQUENCE (p) != NULL && SEQUENCE (SEQUENCE (p)) == NULL) {
      STATUS_SET (p, OPTIMAL_MASK);
    }
  } else {
/* A linear list without labels (of course, it's an enquiry clause). */
    NODE_T *q;
    ADDR_T pop_sp = stack_pointer;
    STATUS_SET (p, SERIAL_MASK);
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
\param p position in tree
\param count counts collateral units
**/

static void genie_collateral_units (NODE_T * p, int *count)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      EXECUTE_UNIT_TRACE (p);
      GENIE_DNS_STACK (p, MOID (p), FRAME_DYNAMIC_SCOPE (frame_pointer), "collateral units");
      (*count)++;
      return;
    } else {
      genie_collateral_units (SUB (p), count);
    }
  }
}

/*!
\brief execute collateral clause
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_collateral (NODE_T * p)
{
  PROPAGATOR_T self;
/* VOID clause and STRUCT display. */
  if (MOID (p) == MODE (VOID) || WHETHER (MOID (p), STRUCT_SYMBOL)) {
    int count = 0;
    genie_collateral_units (SUB (p), &count);
  } else {
/* Row display. */
    A68_REF new_display;
    int count = 0;
    ADDR_T sp = stack_pointer;
    MOID_T *m = MOID (p);
    genie_collateral_units (SUB (p), &count);
    if (DIM (DEFLEX (m)) == 1) {
/* [] AMODE display. */
      new_display = genie_make_row (p, DEFLEX (m)->slice, count, sp);
      stack_pointer = sp;
      INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REF));
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    } else {
/* [,,] AMODE display, we concatenate 1 + (n-1) to n dimensions. */
      new_display = genie_concatenate_rows (p, m, count, sp);
      stack_pointer = sp;
      INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REF));
      *(A68_REF *) STACK_ADDRESS (sp) = new_display;
    }
  }
  self.unit = genie_collateral;
  self.source = p;
  return (self);
}

/*!
\brief execute unit from integral-case in-part
\param p position in tree
\param k value of enquiry clause
\param count unit counter
\return whether a unit was executed
**/

BOOL_T genie_int_case_unit (NODE_T * p, int k, int *count)
{
  if (p == NULL) {
    return (A68_FALSE);
  } else {
    if (WHETHER (p, UNIT)) {
      if (k == *count) {
        EXECUTE_UNIT_TRACE (p);
        return (A68_TRUE);
      } else {
        (*count)++;
        return (A68_FALSE);
      }
    } else {
      if (genie_int_case_unit (SUB (p), k, count) != 0) {
        return (A68_TRUE);
      } else {
        return (genie_int_case_unit (NEXT (p), k, count));
      }
    }
  }
}

/*!
\brief execute unit from united-case in-part
\param p position in tree
\param m mode of enquiry clause
\return whether a unit was executed
**/

BOOL_T genie_united_case_unit (NODE_T * p, MOID_T * m)
{
  if (p == NULL) {
    return (A68_FALSE);
  } else {
    if (WHETHER (p, SPECIFIER)) {
      MOID_T *spec_moid = MOID (NEXT (SUB (p)));
      BOOL_T equal_modes;
      if (m != NULL) {
        if (WHETHER (spec_moid, UNION_SYMBOL)) {
          equal_modes = whether_unitable (m, spec_moid, SAFE_DEFLEXING);
        } else {
          equal_modes = (BOOL_T) (m == spec_moid);
        }
      } else {
        equal_modes = A68_FALSE;
      }
      if (equal_modes) {
        NODE_T *q = NEXT_NEXT (SUB (p));
        OPEN_STATIC_FRAME (p);
        if (WHETHER (q, IDENTIFIER)) {
          if (WHETHER (spec_moid, UNION_SYMBOL)) {
            COPY ((FRAME_OBJECT (OFFSET (TAX (q)))), STACK_TOP, MOID_SIZE (spec_moid));
          } else {
            COPY ((FRAME_OBJECT (OFFSET (TAX (q)))), STACK_OFFSET (ALIGNED_SIZE_OF (A68_UNION)), MOID_SIZE (spec_moid));
          }
        }
        EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
        CLOSE_FRAME;
        return (A68_TRUE);
      } else {
        return (A68_FALSE);
      }
    } else {
      if (genie_united_case_unit (SUB (p), m)) {
        return (A68_TRUE);
      } else {
        return (genie_united_case_unit (NEXT (p), m));
      }
    }
  }
}

/*!
\brief execute identity declaration
\param p position in tree
**/

static void genie_identity_dec (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_IDENTIFIER:
      {
        MOID_T *source_moid = MOID (p);
        NODE_T *src = NEXT_NEXT (p);
        unsigned size = (unsigned) MOID_SIZE (source_moid);
        BYTE_T *z = (FRAME_OBJECT (OFFSET (TAX (p))));
        ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
        FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
        EXECUTE_UNIT_TRACE (src);
        CHECK_INIT_GENERIC (src, STACK_OFFSET (-size), source_moid);
        GENIE_DNS_STACK (src, source_moid, frame_pointer, "identity-declaration");
        FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
        if (source_moid->has_rows) {
          DECREMENT_STACK_POINTER (p, (int) size);
          if (WHETHER (source_moid, STRUCT_SYMBOL)) {
/* STRUCT with row. */
            A68_REF w, src2;
            STATUS (&w) = (STATUS_MASK) (INITIALISED_MASK | IN_STACK_MASK);
            w.offset = stack_pointer;
            REF_HANDLE (&w) = &nil_handle;
            src2 = genie_copy_stowed (w, p, MOID (p));
            COPY (z, ADDRESS (&src2), (int) size);
          } else if (WHETHER (MOID (p), UNION_SYMBOL)) {
/* UNION with row. */
            genie_copy_union (p);
            COPY (z, STACK_TOP, (int) size);
          } else if (WHETHER (MOID (p), ROW_SYMBOL) || WHETHER (MOID (p), FLEX_SYMBOL)) {
/* (FLEX) ROW. */
            *(A68_REF *) z = genie_copy_stowed (*(A68_REF *) STACK_TOP, p, MOID (p));
          } else if (MOID (p) == MODE (SOUND)) {
            COPY (z, STACK_TOP, (int) size);
          }
        } else if (PROPAGATOR (src).unit == genie_constant) {
          STATUS_SET (TAX (p), CONSTANT_MASK);
          POP_ALIGNED (p, z, size);
        } else {
          POP_ALIGNED (p, z, size);
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
\param p position in tree
\param declarer pointer to the declarer
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
        A68_REF *z = (A68_REF *) (FRAME_OBJECT (OFFSET (TAX (p))));
        genie_generator_internal (*declarer, ref_mode, BODY (tag), leap, sp);
        POP_REF (p, z);
        if (NEXT (p) != NULL && WHETHER (NEXT (p), ASSIGN_SYMBOL)) {
          MOID_T *source_moid = SUB (MOID (p));
          NODE_T *src = NEXT_NEXT (p);
          int size = MOID_SIZE (source_moid);
          ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
          FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
          EXECUTE_UNIT_TRACE (src);
          GENIE_DNS_STACK (src, source_moid, frame_pointer, "variable-declaration");
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
\param p position in tree
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
        int leap = (HEAP (tag) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
        A68_REF *z = (A68_REF *) (FRAME_OBJECT (OFFSET (TAX (p))));
        genie_generator_internal (p, ref_mode, BODY (tag), leap, stack_pointer);
        POP_REF (p, z);
        if (NEXT (p) != NULL && WHETHER (NEXT (p), ASSIGN_SYMBOL)) {
          MOID_T *source_moid = SUB (MOID (p));
          int size = MOID_SIZE (source_moid);
          ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
          FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
          EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
          GENIE_DNS_STACK (p, SUB (ref_mode), frame_pointer, "procedure-variable-declaration");
          FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
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
\param p position in tree
**/

static void genie_operator_dec (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case DEFINING_OPERATOR:
      {
        A68_PROCEDURE *z = (A68_PROCEDURE *) (FRAME_OBJECT (OFFSET (TAX (p))));
        ADDR_T pop_dns = FRAME_DYNAMIC_SCOPE (frame_pointer);
        FRAME_DYNAMIC_SCOPE (frame_pointer) = frame_pointer;
        EXECUTE_UNIT_TRACE (NEXT_NEXT (p));
        GENIE_DNS_STACK (p, MOID (p), frame_pointer, "operator-declaration");
        FRAME_DYNAMIC_SCOPE (frame_pointer) = pop_dns;
        POP_PROCEDURE (p, z);
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
\param p position in tree
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

/*
#define LABEL_FREE(p) {\
  NODE_T *_m_q; ADDR_T pop_sp = stack_pointer;\
  for (_m_q = SEQUENCE (p); _m_q != NULL; _m_q = SEQUENCE (_m_q)) {\
    switch (ATTRIBUTE (_m_q)) {\
    case UNIT: {\
      EXECUTE_UNIT_TRACE (_m_q);\
      break;\
    }\
    case SEMI_SYMBOL: {\
      stack_pointer = pop_sp;\
      break;\
    }\
    case DECLARATION_LIST: {\
      genie_declaration (SUB (_m_q));\
      break;\
    }\
    }\
  }}
*/

#define LABEL_FREE(p) {\
  NODE_T *_m_q; ADDR_T pop_sp_lf = stack_pointer;\
  for (_m_q = SEQUENCE (p); _m_q != NULL; _m_q = SEQUENCE (_m_q)) {\
    if (WHETHER (_m_q, UNIT)) {\
      EXECUTE_UNIT_TRACE (_m_q);\
    } else if (WHETHER (_m_q, DECLARATION_LIST)) {\
      genie_declaration (SUB (_m_q));\
    }\
    if (SEQUENCE (_m_q) != NULL) {\
      stack_pointer = pop_sp_lf;\
      _m_q = SEQUENCE (_m_q);\
    }\
  }}

#define SERIAL_CLAUSE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT_INLINE (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    LABEL_FREE (p);\
  } else {\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) p, (jmp_buf *) exit_buf);\
  }}

#define SERIAL_CLAUSE_TRACE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT_TRACE (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    LABEL_FREE (p);\
  } else {\
    if (!setjmp (exit_buf)) {\
      genie_serial_clause ((NODE_T *) p, (jmp_buf *) exit_buf);\
  }}

#define ENQUIRY_CLAUSE(p)\
  if (STATUS_TEST (p, OPTIMAL_MASK)) {\
    EXECUTE_UNIT_INLINE (SEQUENCE (p));\
  } else if (STATUS_TEST (p, SERIAL_MASK)) {\
    LABEL_FREE (p);\
  } else {\
    genie_enquiry_clause ((NODE_T *) p);\
  }

/*!
\brief execute integral-case-clause
\param p position in tree
**/

PROPAGATOR_T genie_int_case (volatile NODE_T * p)
{
  volatile int unit_count;
  volatile BOOL_T found_unit;
  jmp_buf exit_buf;
  A68_INT k;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* CASE or OUSE. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  POP_OBJECT (q, &k, A68_INT);
/* IN. */
  FORWARD (q);
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  unit_count = 1;
  found_unit = genie_int_case_unit (NEXT_SUB ((NODE_T *) q), (int) VALUE (&k), (int *) &unit_count);
  CLOSE_FRAME;
/* OUT. */
  if (!found_unit) {
    FORWARD (q);
    switch (ATTRIBUTE (q)) {
    case CHOICE:
    case OUT_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
        SERIAL_CLAUSE (NEXT_SUB (q));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case ESAC_SYMBOL:
      {
        genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
        break;
      }
    default:
      {
        MOID (SUB ((NODE_T *) q)) = (MOID_T *) yield;
        (void) genie_int_case (q);
        break;
      }
    }
  }
/* ESAC. */
  CLOSE_FRAME;
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "integer-case-clause");
  PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
  return (PROPAGATOR (p));
}

/*!
\brief execute united-case-clause
\param p position in tree
**/

PROPAGATOR_T genie_united_case (volatile NODE_T * p)
{
  volatile BOOL_T found_unit = A68_FALSE;
  volatile MOID_T *um;
  volatile ADDR_T pop_sp;
  jmp_buf exit_buf;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* CASE or OUSE. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  pop_sp = stack_pointer;
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  stack_pointer = pop_sp;
  um = (volatile MOID_T *) VALUE ((A68_UNION *) STACK_TOP);
/* IN. */
  FORWARD (q);
  if (um != NULL) {
    OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
    found_unit = genie_united_case_unit (NEXT_SUB ((NODE_T *) q), (MOID_T *) um);
    CLOSE_FRAME;
  } else {
    found_unit = A68_FALSE;
  }
/* OUT. */
  if (!found_unit) {
    FORWARD (q);
    switch (ATTRIBUTE (q)) {
    case CHOICE:
    case OUT_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
        SERIAL_CLAUSE (NEXT_SUB (q));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case ESAC_SYMBOL:
      {
        genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
        break;
      }
    default:
      {
        MOID (SUB ((NODE_T *) q)) = (MOID_T *) yield;
        (void) genie_united_case (q);
        break;
      }
    }
  }
/* ESAC. */
  CLOSE_FRAME;
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "united-case-clause");
  PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
  return (PROPAGATOR (p));
}

/*!
\brief execute conditional-clause
\param p position in tree
**/

PROPAGATOR_T genie_conditional (volatile NODE_T * p)
{
  volatile int pop_sp = stack_pointer;
  jmp_buf exit_buf;
  volatile NODE_T *q = SUB (p);
  volatile MOID_T *yield = MOID (q);
/* IF or ELIF. */
  OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
  ENQUIRY_CLAUSE (NEXT_SUB (q));
  stack_pointer = pop_sp;
  FORWARD (q);
  if (VALUE ((A68_BOOL *) STACK_TOP) == A68_TRUE) {
/* THEN. */
    OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
    SERIAL_CLAUSE (NEXT_SUB (q));
    CLOSE_FRAME;
  } else {
/* ELSE. */
    FORWARD (q);
    switch (ATTRIBUTE (q)) {
    case CHOICE:
    case ELSE_PART:
      {
        OPEN_STATIC_FRAME ((NODE_T *) SUB (q));
        SERIAL_CLAUSE (NEXT_SUB (q));
        CLOSE_FRAME;
        break;
      }
    case CLOSE_SYMBOL:
    case FI_SYMBOL:
      {
        genie_push_undefined ((NODE_T *) q, (MOID_T *) yield);
        break;
      }
    default:
      {
        MOID (SUB ((NODE_T *) q)) = (MOID_T *) yield;
        (void) genie_conditional (q);
        break;
      }
    }
  }
/* FI. */
  CLOSE_FRAME;
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "conditional-clause");
  PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
  return (PROPAGATOR (p));
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
\param p position in tree
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
    from = VALUE ((A68_INT *) STACK_TOP);
    FORWARD (p);
  } else {
    from = 1;
  }
/* BY unit. */
  if (WHETHER (p, BY_PART)) {
    EXECUTE_UNIT (NEXT_SUB (p));
    stack_pointer = pop_sp;
    by = VALUE ((A68_INT *) STACK_TOP);
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
    to = VALUE ((A68_INT *) STACK_TOP);
    to_part = p;
    FORWARD (p);
  } else {
    to = (by >= 0 ? A68_MAX_INT : -A68_MAX_INT);
  }
  q = NEXT_SUB (p);
/* Here the loop part starts.
   We open the frame only once and reinitialise if necessary. */
  OPEN_STATIC_FRAME ((NODE_T *) q);
  counter = from;
/* Does the loop contain conditionals? */
  if (WHETHER (p, WHILE_PART)) {
    conditional = A68_TRUE;
  } else if (WHETHER (p, DO_PART) || WHETHER (p, ALT_DO_PART)) {
    NODE_T *un_p = NEXT_SUB (p);
    if (WHETHER (un_p, SERIAL_CLAUSE)) {
      un_p = NEXT (un_p);
    }
    conditional = (BOOL_T) (un_p != NULL && WHETHER (un_p, UNTIL_PART));
  } else {
    conditional = A68_FALSE;
  }
  if (conditional) {
/* [FOR ...] [WHILE ...] DO [...] [UNTIL ...] OD. */
    go_on = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
    while (go_on) {
      if (for_part != NULL) {
        A68_INT *z = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (for_part))));
        STATUS (z) = INITIALISED_MASK;
        VALUE (z) = counter;
      }
      stack_pointer = pop_sp;
      if (WHETHER (p, WHILE_PART)) {
        ENQUIRY_CLAUSE (q);
        stack_pointer = pop_sp;
        go_on = (BOOL_T) (VALUE ((A68_BOOL *) STACK_TOP) == A68_TRUE);
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
          go_on = (BOOL_T) (VALUE ((A68_BOOL *) STACK_TOP) == A68_FALSE);
          CLOSE_FRAME;
        }
        if (WHETHER (p, WHILE_PART)) {
          CLOSE_FRAME;
        }
/* Increment counter. */
        if (go_on) {
          INCREMENT_COUNTER;
          go_on = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
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
    go_on = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
    while (go_on) {
      if (for_part != NULL) {
        A68_INT *z = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (for_part))));
        STATUS (z) = INITIALISED_MASK;
        VALUE (z) = counter;
      }
      stack_pointer = pop_sp;
      SERIAL_CLAUSE_TRACE (NEXT_SUB (p));
      INCREMENT_COUNTER;
      go_on = (BOOL_T) ((by > 0 && counter <= to) || (by < 0 && counter >= to) || (by == 0));
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
  return (PROPAGATOR (p));
}

#undef INCREMENT_COUNTER
#undef LOOP_OVERFLOW

/*!
\brief execute closed clause
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_closed (volatile NODE_T * p)
{
  jmp_buf exit_buf;
  volatile NODE_T *q = NEXT_SUB (p);
  OPEN_STATIC_FRAME ((NODE_T *) q);
  SERIAL_CLAUSE (q);
  CLOSE_FRAME;
  GENIE_DNS_STACK (p, MOID (p), frame_pointer, "closed-clause");
  PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
  return (PROPAGATOR (p));
}

/*!
\brief execute enclosed clause
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_enclosed (volatile NODE_T * p)
{
  PROPAGATOR_T self;
  self.unit = (PROPAGATOR_PROCEDURE *) genie_enclosed;
  self.source = (NODE_T *) p;
  switch (ATTRIBUTE (p)) {
  case PARTICULAR_PROGRAM:
    {
      self = genie_enclosed (SUB (p));
      break;
    }
  case ENCLOSED_CLAUSE:
    {
      self = genie_enclosed (SUB (p));
      break;
    }
  case CLOSED_CLAUSE:
    {
      (void) genie_closed ((NODE_T *) p);
      self.unit = (PROPAGATOR_PROCEDURE *) genie_closed;
      self.source = (NODE_T *) p;
      break;
    }
#if defined ENABLE_PAR_CLAUSE
  case PARALLEL_CLAUSE:
    {
      (void) genie_parallel ((NODE_T *) NEXT_SUB (p));
      GENIE_DNS_STACK (p, MOID (p), frame_pointer, "parallel-clause");
      PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
      break;
    }
#endif
  case COLLATERAL_CLAUSE:
    {
      (void) genie_collateral ((NODE_T *) p);
      GENIE_DNS_STACK (p, MOID (p), frame_pointer, "collateral-clause");
      PROTECT_FROM_SWEEP_STACK ((NODE_T *) p);
      break;
    }
  case CONDITIONAL_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_conditional (p);
      self.unit = (PROPAGATOR_PROCEDURE *) genie_conditional;
      self.source = (NODE_T *) p;
      break;
    }
  case INTEGER_CASE_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_int_case (p);
      self.unit = (PROPAGATOR_PROCEDURE *) genie_int_case;
      self.source = (NODE_T *) p;
      break;
    }
  case UNITED_CASE_CLAUSE:
    {
      MOID (SUB ((NODE_T *) p)) = MOID (p);
      (void) genie_united_case (p);
      self.unit = (PROPAGATOR_PROCEDURE *) genie_united_case;
      self.source = (NODE_T *) p;
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
  PROPAGATOR (p) = self;
  return (self);
}
