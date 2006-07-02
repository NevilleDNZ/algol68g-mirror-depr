/*!
\file generator.c
\brief generator and garbage collector routines
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

#undef DEBUG

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

#include <sys/time.h>

/*
Here are the generator and the garbage collector.

The generator allocates space in stack or heap and initialises dynamically sized objects.

A mark-and-sweep garbage collector defragments the heap. When called, it walks
the stack frames and marks the heap space that is still active. This marking
process is called "colouring" here since we "pour paint" into the heap.
The active blocks are then joined, the non-active blocks are forgotten.

When colouring the heap, "cookies" are placed in objects as to find circular
references.

Algol68G introduces several anonymous tags in the symbol tables that save
temporary REF or ROW results, so that they do not get prematurely swept.

The genie is not smart enough to handle every heap clog, e.g. when copying
STOWED objects. This seems not very elegant, but garbage collectors in general
cannot solve all core management problems. To avoid many of the "unforeseen"
heap clogs, we try to keep heap occupation low by sweeping the heap
occasionally, before it fills up completely. If this automatic mechanism does
not help, one can always invoke the garbage collector by calling "sweep heap"
from Algol 68 source text.

Mark-and-sweep is simple but since it walks recursive structures, it could
exhaust the C-stack (segment violation). A rough check is in place.
*/

void colour_object (BYTE_T *, MOID_T *);
void sweep_heap (NODE_T *, ADDR_T);
int garbage_collects, garbage_bytes_freed;
int free_handle_count, max_handle_count;
int block_heap_compacter;
A68_HANDLE *free_handles, *busy_handles;
double garbage_seconds;

#define DEF(p) (NEXT (NEXT (NODE (TAX (p)))))
#define MAX(u, v) ((u) = ((u) > (v) ? (u) : (v)))

A68_REF genie_allocate_declarer (NODE_T *, ADDR_T *, A68_REF, BOOL_T);

/* Total freed is kept in a LONG INT. */
MP_DIGIT_T garbage_total_freed[LONG_MP_DIGITS + 2];
static MP_DIGIT_T garbage_freed[LONG_MP_DIGITS + 2];

/*!
\brief PROC VOID sweep heap
\param p
**/

void genie_sweep_heap (NODE_T * p)
{
  sweep_heap (p, frame_pointer);
}

/*!
\brief PROC VOID preemptive sweep heap
\param p
**/

void genie_preemptive_sweep_heap (NODE_T * p)
{
  PREEMPTIVE_SWEEP;
}

/*!
\brief INT collections
\param p
**/

void genie_garbage_collections (NODE_T * p)
{
  PUSH_INT (p, garbage_collects);
}

/*!
\brief LONG INT garbage
\param p
**/

void genie_garbage_freed (NODE_T * p)
{
  PUSH (p, garbage_total_freed, moid_size (MODE (LONG_INT)));
}

/*!
\brief REAL collect seconds
\param p
**/

void genie_garbage_seconds (NODE_T * p)
{
/* Note that this timing is a rough cut. */
  PUSH_REAL (p, garbage_seconds);
}

/*!
\brief size available for an object in the heap
\return size available in bytes
**/

int heap_available ()
{
  return (heap_size - heap_pointer);
}

/*!
\brief initialise heap management
\param p
\param module
**/

void genie_init_heap (NODE_T * p, MODULE_T * module)
{
  A68_HANDLE *z;
  int k, max;
  (void) p;
  if (heap_segment == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, module->top_node, ERROR_OUT_OF_CORE);
    exit_genie (module->top_node, 1);
  }
  if (handle_segment == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, module->top_node, ERROR_OUT_OF_CORE);
    exit_genie (module->top_node, 1);
  }
  block_heap_compacter = 0;
  garbage_seconds = 0;
  SET_MP_ZERO (garbage_total_freed, LONG_MP_DIGITS);
  garbage_collects = 0;
  ABNORMAL_END (fixed_heap_pointer >= heap_size, ERROR_OUT_OF_CORE, NULL);
  heap_pointer = fixed_heap_pointer;
/* Assign handle space. */
  z = (A68_HANDLE *) handle_segment;
  free_handles = z;
  busy_handles = NULL;
  max = (int) handle_pool_size / sizeof (A68_HANDLE);
  free_handle_count = max;
  max_handle_count = max;
  for (k = 0; k < max; k++) {
    z[k].status = NULL_MASK;
    z[k].offset = 0;
    z[k].size = 0;
    NEXT (&z[k]) = (k == max - 1 ? NULL : &z[k + 1]);
    PREVIOUS (&z[k]) = (k == 0 ? NULL : &z[k - 1]);
  }
}

/*!
\brief whether "m" is eligible for colouring
\param m
\return TRUE or FALSE
**/

static BOOL_T moid_needs_colouring (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (A_TRUE);
  } else if (WHETHER (m, FLEX_SYMBOL) || WHETHER (m, ROW_SYMBOL)) {
    return (A_TRUE);
  } else if (WHETHER (m, STRUCT_SYMBOL) || WHETHER (m, UNION_SYMBOL)) {
    PACK_T *p = PACK (m);
    BOOL_T k = A_FALSE;
    for (; p != NULL && !k; FORWARD (p)) {
      k |= moid_needs_colouring (MOID (p));
    }
    return (k);
  } else {
    return (A_FALSE);
  }
}

/*!
\brief colour all elements of a row
\param z
\param m
**/

static void colour_row_elements (A68_REF * z, MOID_T * m)
{
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  GET_DESCRIPTOR (arr, tup, z);
/* Empty rows are trivial since we don't recognise ghost elements. */
  if (get_row_size (tup, arr->dimensions) > 0) {
/* The multi-dimensional sweeper. */
    BYTE_T *elem = ADDRESS (&arr->array);
    BOOL_T done = A_FALSE;
    initialise_internal_index (tup, arr->dimensions);
    while (!done) {
      ADDR_T index = calculate_internal_index (tup, arr->dimensions);
      ADDR_T addr = ROW_ELEMENT (arr, index);
      colour_object (&elem[addr], SUB (m));
      done = increment_internal_index (tup, arr->dimensions);
    }
  }
}

/*!
\brief colour an (active) object
\param item
\param m
**/

void colour_object (BYTE_T * item, MOID_T * m)
{
  if (item == NULL || m == NULL) {
    return;
  }
/* Deeply recursive objects might exhaust the stack. */
  LOW_STACK_ALERT (NULL);
  if (WHETHER (m, REF_SYMBOL)) {
/* REF AMODE colour pointer and object to which it refers. */
    A68_REF *z = (A68_REF *) item;
    if ((z != NULL) && (z->status & INITIALISED_MASK) && (z->handle != NULL)) {
      if (z->handle->status & COOKIE_MASK) {
/* Circular list. */
        return;
      }
      z->handle->status |= COOKIE_MASK;
      if (z->segment == heap_segment) {
        z->handle->status |= COLOUR_MASK;
      }
      if (!IS_NIL (*z)) {
        colour_object (ADDRESS (z), SUB (m));
      }
      z->handle->status &= ~COOKIE_MASK;
    }
  } else if (WHETHER (m, FLEX_SYMBOL) || WHETHER (m, ROW_SYMBOL) || m == MODE (STRING)) {
/* [] AMODE - colour all elements. */
    A68_REF *z = (A68_REF *) item;
/* Claim the descriptor and the row itself. */
    if ((z != NULL) && (z->status & INITIALISED_MASK) && (z->handle != NULL)) {
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      if (z->handle->status & COOKIE_MASK) {
/* Circular list. */
        return;
      }

/* An array is ALWAYS in the heap. */
      z->handle->status |= COOKIE_MASK;
      z->handle->status |= COLOUR_MASK;
      GET_DESCRIPTOR (arr, tup, z);
      if ((arr->array).handle != NULL) {
/* Assume its initialisation. */
        MOID_T *n = DEFLEX (m);
        (arr->array).handle->status |= COLOUR_MASK;
        if (moid_needs_colouring (SUB (n))) {
          colour_row_elements (z, n);
        }
      }
      z->handle->status &= ~COOKIE_MASK;
    }
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
/* STRUCTures - colour fields. */
    PACK_T *p = PACK (m);
    for (; p != NULL; FORWARD (p)) {
      colour_object (&item[p->offset], MOID (p));
    }
  } else if (WHETHER (m, UNION_SYMBOL)) {
/* UNIONs - a united object may contain a value that needs colouring. */
    A68_POINTER *z = (A68_POINTER *) item;
    if (z->status & INITIALISED_MASK) {
      MOID_T *united_moid = (MOID_T *) z->value;
      colour_object (&item[SIZE_OF (A68_POINTER)], united_moid);
    }
  } else if (WHETHER (m, PROC_SYMBOL)) {
/* PROCs - save a locale and the objects it points to. */
    A68_PROCEDURE *z = (A68_PROCEDURE *) item;
    if (!IS_NIL (z->locale) && !(z->locale.handle->status & COOKIE_MASK)) {
      BYTE_T *u = ADDRESS (&(z->locale));
      PACK_T *s = PACK (z->proc_mode);
      z->locale.handle->status |= COOKIE_MASK;
      for (; s != NULL; FORWARD (s)) {
        if (((A68_BOOL *) & u[0])->value == A_TRUE) {
          colour_object (&u[SIZE_OF (A68_BOOL)], MOID (s));
        }
        u = &(u[SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      }
      z->locale.handle->status |= COLOUR_MASK;
      z->locale.handle->status &= ~COOKIE_MASK;
    }
  }
}

/*!
\brief colour active objects in the heap
\param fp
**/

static void colour_heap (ADDR_T fp)
{
  while (fp != 0) {
    NODE_T *p = FRAME_TREE (fp);
    SYMBOL_TABLE_T *q = SYMBOL_TABLE (p);
    if (q != NULL) {
      TAG_T *i;
      for (i = q->identifiers; i != NULL; FORWARD (i)) {
        colour_object (FRAME_LOCAL (fp, i->offset), MOID (i));
      }
      for (i = q->anonymous; i != NULL; FORWARD (i)) {
        if (PRIO (i) == PROTECT_FROM_SWEEP) {
          colour_object (FRAME_LOCAL (fp, i->offset), MOID (i));
        }
      }
    }
    fp = FRAME_DYNAMIC_LINK (fp);
  }
}

/*!
\brief join all active blocks in the heap
**/

static void defragment_heap ()
{
  A68_HANDLE *z;
/* Free handles. */
  z = busy_handles;
  while (z != NULL) {
    if (!(z->status & COLOUR_MASK) && !(z->status & NO_SWEEP_MASK)) {
      A68_HANDLE *y = NEXT (z);
      if (PREVIOUS (z) == NULL) {
        busy_handles = NEXT (z);
      } else {
        NEXT (PREVIOUS (z)) = NEXT (z);
      }
      if (NEXT (z) != NULL) {
        PREVIOUS (NEXT (z)) = PREVIOUS (z);
      }
      NEXT (z) = free_handles;
      PREVIOUS (z) = NULL;
      if (NEXT (z) != NULL) {
        PREVIOUS (NEXT (z)) = z;
      }
      free_handles = z;
      z->status &= ~ALLOCATED_MASK;
      garbage_bytes_freed += z->size;
      free_handle_count++;
      z = y;
    } else {
      z = NEXT (z);
    }
  }
/* There can be no uncoloured allocated handle. */
  for (z = busy_handles; z != NULL; z = NEXT (z)) {
    ABNORMAL_END ((z->status & COLOUR_MASK) == 0 && (z->status & NO_SWEEP_MASK) == 0, "bad GC consistency", NULL);
  }
/* Order in the heap must be preserved. */
  for (z = busy_handles; z != NULL; z = NEXT (z)) {
    ABNORMAL_END (NEXT (z) != NULL && z->offset < NEXT (z)->offset, "bad GC order", NULL);
  }
/* Defragment the heap. */
  heap_pointer = fixed_heap_pointer;
  for (z = busy_handles; z != NULL && NEXT (z) != NULL; z = NEXT (z)) {
    ;
  }
  for (; z != NULL; z = PREVIOUS (z)) {
    MOVE (HEAP_ADDRESS (heap_pointer), HEAP_ADDRESS (z->offset), (unsigned) z->size);
    z->status &= ~COLOUR_MASK;
    z->offset = heap_pointer;
    heap_pointer += (z->size);
    ABNORMAL_END (heap_pointer % ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
  }
}

/*!
\brief clean up garbage and defragment the heap
\param p
\param fp
**/

void sweep_heap (NODE_T * p, ADDR_T fp)
{
/* Must start with fp = current frame_pointer. */
  A68_HANDLE *z;
  double t0, t1;
  if (block_heap_compacter > 0) {
    return;
  }
  t0 = seconds ();
/* Unfree handles are subject to inspection. */
  for (z = busy_handles; z != NULL; z = NEXT (z)) {
    z->status &= ~(COLOUR_MASK | COOKIE_MASK);
  }
/* Pour paint into the heap to reveal active objects. */
  colour_heap (fp);
/* Start freeing and compacting. */
  garbage_bytes_freed = 0;
  defragment_heap ();
/* Stats and logging. */
  garbage_collects++;
  int_to_mp (p, garbage_freed, (int) garbage_bytes_freed, LONG_MP_DIGITS);
  add_mp (p, garbage_total_freed, garbage_total_freed, garbage_freed, LONG_MP_DIGITS);
  t1 = seconds ();
  if (t1 > t0) {
    garbage_seconds += (t1 - t0);
#if defined CLK_TCK
  } else {
/* Add average value in case of slow clock. */
    garbage_seconds += ((1.0 / CLK_TCK) / 2.0);
#endif
  }
}

/*!
\brief yield a handle that will point to a block in the heap
\param p
\param a68m
\return
**/

static A68_HANDLE *give_handle (NODE_T * p, MOID_T * a68m)
{
  if (free_handles != NULL) {
    A68_HANDLE *x = free_handles;
    free_handles = NEXT (x);
    if (free_handles != NULL) {
      PREVIOUS (free_handles) = NULL;
    }
    x->status = ALLOCATED_MASK;
    x->offset = 0;
    x->size = 0;
    MOID (x) = a68m;
    NEXT (x) = busy_handles;
    PREVIOUS (x) = NULL;
    if (NEXT (x) != NULL) {
      PREVIOUS (NEXT (x)) = x;
    }
    busy_handles = x;
    free_handle_count--;
    return (x);
  } else {
    sweep_heap (p, frame_pointer);
    if (free_handles != NULL) {
      return (give_handle (p, a68m));
    } else {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A_RUNTIME_ERROR);
    }
  }
  return (NULL);
}

/*!
\brief give a block of heap for an object of indicated mode
\param p
\param mode
\param size
\return
**/

A68_REF heap_generator (NODE_T * p, MOID_T * mode, int size)
{
/* Align. */
  ABNORMAL_END (size < 0, ERROR_INVALID_SIZE, NULL);
  size = ALIGN (size);
/* Now give it. */
  if (heap_available () >= size) {
    A68_HANDLE *x;
    A68_REF z;
    PREEMPTIVE_SWEEP;
    x = give_handle (p, mode);
    x->size = size;
    x->offset = heap_pointer;
/* Set all values to uninitialised. */
    FILL (&heap_segment[heap_pointer], 0, (unsigned) size);
    heap_pointer += size;
    z.status = INITIALISED_MASK;
    z.segment = heap_segment;
    z.offset = 0;
    z.scope = PRIMAL_SCOPE;
    z.handle = x;
    ABNORMAL_END (((long) ADDRESS (&z)) % ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
    return (z);
  } else {
/* No heap space. First sweep the heap. */
    sweep_heap (p, frame_pointer);
    if (heap_available () > size) {
      return (heap_generator (p, mode, size));
    } else {
/* Still no heap space. We must abend. */
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A_RUNTIME_ERROR);
      return (nil_ref);
    }
  }
}

/*
Following implements the generator.

For dynamically sized objects, first bounds are evaluated (right first, then down).
The object is generated keeping track of the bound-count.

	...
	[#1]
	STRUCT
	(
	[#2]
	STRUCT
	(
	[#3] A a, b, ...
	)
	,			Advance bound-count here, max is #3
	[#4] B a, b, ...
	)
	,			Advance bound-count here, max is #4
	[#5] C a, b, ...
	...

Bound-count is maximised when generator_stowed is entered recursively. 
Bound-count is advanced when completing a STRUCTURED_FIELD.
*/

/*!
\brief whether a moid needs work in allocation
\param m moid under test
**/

static BOOL_T needs_allocation (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (A_FALSE);
  }
  if (WHETHER (m, PROC_SYMBOL)) {
    return (A_FALSE);
  }
  if (WHETHER (m, UNION_SYMBOL)) {
    return (A_FALSE);
  }
  if (m == MODE (VOID)) {
    return (A_FALSE);
  }
  return (A_TRUE);
}

/*!
\brief prepare bounds
\param p position in the syntax tree, should not be NULL
**/

#ifdef DEBUG
static void pr_int (char *s)
{
  A68_INT *z = (A68_INT *) STACK_OFFSET (-SIZE_OF (A68_INT));
  printf ("\npush %s %d %p", s, VALUE (z), z);
  fflush (stdout);
}
#endif

static void genie_prepare_bounds (NODE_T * p)
{
  for (; p != NULL; p = NEXT (p)) {
    if (WHETHER (p, BOUNDS_LIST)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, BOUND)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, UNIT)) {
      if (NEXT (p) != NULL && (WHETHER (NEXT (p), COLON_SYMBOL) || WHETHER (NEXT (p), DOTDOT_SYMBOL))) {
        EXECUTE_UNIT (p);
        p = NEXT (NEXT (p));
      } else {
/* Default lower bound */
        PUSH_INT (p, 1);
      }
#ifdef DEBUG
      pr_int ("lwb");
#endif
      EXECUTE_UNIT (p);
#ifdef DEBUG
      pr_int ("upb");
#endif
    }
  }
}

/*!
\brief prepare bounds for a row
\param p position in the syntax tree, should not be NULL
**/

void genie_generator_bounds (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOUNDS)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, INDICANT)) {
      if (TAX (p) != NULL && MOID (TAX (p))->has_rows) {
        /*
         * Continue from definition at MODE A = .. 
         */
        genie_generator_bounds (DEF (p));
      }
    } else if (WHETHER (p, DECLARER) && needs_allocation (MOID (p)) == A_FALSE) {
      return;
    } else {
      genie_generator_bounds (SUB (p));
    }
  }
}

/*!
\brief allocate a structure
\param p declarer in the syntax tree, should not be NULL
**/

void genie_generator_stowed (NODE_T *, BYTE_T *, NODE_T **, ADDR_T *, ADDR_T *);

void genie_generator_field (NODE_T * p, BYTE_T ** q, NODE_T ** declarer, ADDR_T * sp, ADDR_T * max_sp)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, STRUCTURED_FIELD)) {
      genie_generator_field (SUB (p), q, declarer, sp, max_sp);
    }
    if (WHETHER (p, DECLARER)) {
      *declarer = SUB (p);
      FORWARD (p);
    }
    if (WHETHER (p, FIELD_IDENTIFIER)) {
      MOID_T *field_mode = MOID (*declarer);
      ADDR_T pop_sp = *sp;
#ifdef DEBUG
      printf ("\n%s %s", moid_to_string (field_mode, MOID_WIDTH), SYMBOL (p));
      fflush (stdout);
#endif
      if (field_mode->has_rows && WHETHER_NOT (field_mode, UNION_SYMBOL)) {
        genie_generator_stowed (*declarer, *q, NULL, sp, max_sp);
      }
      *sp = pop_sp;
      *q += MOID_SIZE (field_mode);
    }
  }
}

/*!
\brief allocate a structure
\param p declarer in the syntax tree, should not be NULL
**/

void genie_generator_struct (NODE_T * p, BYTE_T ** q, ADDR_T * sp, ADDR_T * max_sp)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, STRUCTURED_FIELD_LIST)) {
      genie_generator_struct (SUB (p), q, sp, max_sp);
    } else if (WHETHER (p, STRUCTURED_FIELD)) {
      NODE_T *declarer = NULL;
      ADDR_T bla = *max_sp;
      genie_generator_field (SUB (p), q, &declarer, sp, &bla);
      *max_sp = bla;
      *sp = *max_sp;
    }
  }
}

/*!
\brief allocate a stowed object
\param p declarer in the syntax tree, should not be NULL
**/

void genie_generator_stowed (NODE_T * p, BYTE_T * q, NODE_T ** declarer, ADDR_T * sp, ADDR_T * max_sp)
{
  if (p == NULL) {
    return;
  }
  if (WHETHER (p, INDICANT)) {
    if (MOID (p) == MODE (STRING)) {
      *((A68_REF *) q) = empty_string (p);
    } else if (TAX (p) != NULL) {
/* Continue from definition at MODE A = .. */
      genie_generator_stowed (DEF (p), q, declarer, sp, max_sp);
    }
    return;
  }
  if (WHETHER (p, DECLARER) && needs_allocation (MOID (p))) {
    genie_generator_stowed (SUB (p), q, declarer, sp, max_sp);
    return;
  }
  if (WHETHER (p, STRUCT_SYMBOL)) {
    BYTE_T *r = q;
    genie_generator_struct (SUB (NEXT (p)), &r, sp, max_sp);
    return;
  }
  if (WHETHER (p, FLEX_SYMBOL)) {
    FORWARD (p);
  }
  if (WHETHER (p, BOUNDS)) {
    ADDR_T bla = *max_sp;
    A68_REF desc, elems;
    MOID_T *slice_mode = MOID (NEXT (p));
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    A68_INT *bounds = (A68_INT *) STACK_ADDRESS (*sp);
    int k, dimensions = DIMENSION (DEFLEX (MOID (p)));
    int elem_size = MOID_SIZE (slice_mode), row_size = 1;
    UP_SWEEP_SEMA;
    desc = heap_generator (p, MOID (p), dimensions * SIZE_OF (A68_TUPLE) + SIZE_OF (A68_ARRAY));
    GET_DESCRIPTOR (arr, tup, &desc);
    for (k = 0; k < dimensions; k++) {
      tup[k].lower_bound = VALUE (&(bounds[2 * k]));
      tup[k].upper_bound = VALUE (&(bounds[2 * k + 1]));
      tup[k].span = row_size;
      tup[k].shift = tup[k].lower_bound;
#ifdef DEBUG
      printf ("\n%d:%d", tup[k].lower_bound, tup[k].upper_bound);
      fflush (stdout);
#endif
      row_size *= ROW_SIZE (&tup[k]);
    }
    elems = heap_generator (p, MOID (p), row_size * elem_size);
    arr->dimensions = dimensions;
    arr->type = slice_mode;
    arr->elem_size = elem_size;
    arr->slice_offset = 0;
    arr->field_offset = 0;
    arr->array = elems;
    (*sp) += (dimensions * 2 * SIZE_OF (A68_INT));
    MAX (bla, *sp);
    if (slice_mode->has_rows && needs_allocation (slice_mode)) {
      BYTE_T *elem = ADDRESS (&elems);
      for (k = 0; k < row_size; k++) {
        ADDR_T pop_sp = *sp;
        bla = *max_sp;
        genie_generator_stowed (NEXT (p), &(elem[k * elem_size]), NULL, sp, &bla);
        *sp = pop_sp;
      }
    }
    *max_sp = bla;
    *sp = *max_sp;
    *(A68_REF *) q = desc;
    DOWN_SWEEP_SEMA;
    return;
  }
}

/*!
\brief generate space and push a REF
\param p declarer in the syntax tree, should not be NULL
\param ref_mode REF mode to be generated
\param tag associated internal LOC for this generator
\param leap where to generate space
\param sp stack pointer to locate bounds
**/

void genie_generator_internal (NODE_T * p, MOID_T * ref_mode, TAG_T * tag, LEAP_T leap, ADDR_T sp)
{
  MOID_T *mode = SUB (ref_mode);
  A68_REF name;
  UP_SWEEP_SEMA;
/* Set up a REF MODE object, either in the stack or in the heap. */
  if (leap == LOC_SYMBOL) {
    name.status = INITIALISED_MASK;
    name.segment = frame_segment;
    name.handle = &nil_handle;
    name.offset = frame_pointer + FRAME_INFO_SIZE + tag->offset;
    name.scope = frame_pointer;
  } else {
    name = heap_generator (p, mode, MOID_SIZE (mode));
    name.scope = PRIMAL_SCOPE;
  }
  if (mode->has_rows) {
    ADDR_T cur_sp = sp, max_sp = sp;
    genie_generator_stowed (p, ADDRESS (&name), NULL, &cur_sp, &max_sp);
  }
  PUSH_REF (p, name);
  DOWN_SWEEP_SEMA;
}

/*!
\brief push a name refering to allocated space
\param p position in the syntax tree, should not be NULL
\return a propagator for this action
**/

PROPAGATOR_T genie_generator (NODE_T * p)
{
  PROPAGATOR_T self;
  ADDR_T pop_sp = stack_pointer;
  A68_REF z;
  self.unit = genie_generator;
  self.source = p;
  genie_generator_bounds (NEXT (SUB (p)));
  genie_generator_internal (NEXT (SUB (p)), MOID (p), TAX (p), ATTRIBUTE (SUB (p)), pop_sp);
  POP_REF (p, &z);
  stack_pointer = pop_sp;
  PUSH_REF (p, z);
  PROTECT_FROM_SWEEP (p);
  return (self);
}
