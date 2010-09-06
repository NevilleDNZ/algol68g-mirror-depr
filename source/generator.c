/*!
\file generator.c
\brief generator and garbage collector routines
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "config.h"
#include "diagnostics.h"
#include "algol68g.h"
#include "genie.h"
#include "inline.h"
#include "mp.h"

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

#define DEF(p) (NEXT_NEXT (NODE (TAX (p))))
#define MAX(u, v) ((u) = ((u) > (v) ? (u) : (v)))

A68_REF genie_allocate_declarer (NODE_T *, ADDR_T *, A68_REF, BOOL_T);
void genie_generator_stowed (NODE_T *, BYTE_T *, NODE_T **, ADDR_T *, ADDR_T *);

/* Total freed is kept in a LONG INT. */
MP_DIGIT_T garbage_total_freed[LONG_MP_DIGITS + 2];
static MP_DIGIT_T garbage_freed[LONG_MP_DIGITS + 2];

extern int block_heap_compacter;
/*!
\brief PROC VOID sweep heap
\param p position in tree
**/

void genie_sweep_heap (NODE_T * p)
{
  sweep_heap (p, frame_pointer);
}

/*!
\brief PROC VOID preemptive sweep heap
\param p position in tree
**/

void genie_preemptive_sweep_heap (NODE_T * p)
{
  PREEMPTIVE_SWEEP;
}

/*!
\brief INT collections
\param p position in tree
**/

void genie_garbage_collections (NODE_T * p)
{
  PUSH_PRIMITIVE (p, garbage_collects, A68_INT);
}

/*!
\brief LONG INT garbage
\param p position in tree
**/

void genie_garbage_freed (NODE_T * p)
{
  PUSH (p, garbage_total_freed, moid_size (MODE (LONG_INT)));
}

/*!
\brief REAL collect seconds
\param p position in tree
**/

void genie_garbage_seconds (NODE_T * p)
{
/* Note that this timing is a rough cut. */
  PUSH_PRIMITIVE (p, garbage_seconds, A68_REAL);
}

/*!
\brief size available for an object in the heap
\return size available in bytes
**/

int heap_available (void)
{
  return (heap_size - heap_pointer);
}

/*!
\brief initialise heap management
\param p position in tree
\param module current module
**/

void genie_init_heap (NODE_T * p)
{
  A68_HANDLE *z;
  int k, max;
  (void) p;
  if (heap_segment == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, program.top_node, ERROR_OUT_OF_CORE);
    exit_genie (program.top_node, A68_RUNTIME_ERROR);
  }
  if (handle_segment == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, program.top_node, ERROR_OUT_OF_CORE);
    exit_genie (program.top_node, A68_RUNTIME_ERROR);
  }
  block_heap_compacter = 0;
  garbage_seconds = 0;
  SET_MP_ZERO (garbage_total_freed, LONG_MP_DIGITS);
  garbage_collects = 0;
  ABEND (fixed_heap_pointer >= heap_size, ERROR_OUT_OF_CORE, NULL);
  heap_pointer = fixed_heap_pointer;
  get_fixed_heap_allowed = A68_FALSE;
/* Assign handle space. */
  z = (A68_HANDLE *) handle_segment;
  free_handles = z;
  busy_handles = NULL;
  max = (int) handle_pool_size / (int) sizeof (A68_HANDLE);
  free_handle_count = max;
  max_handle_count = max;
  for (k = 0; k < max; k++) {
    STATUS (&(z[k])) = NULL_MASK;
    POINTER (&(z[k])) = NULL;
    SIZE (&(z[k])) = 0;
    NEXT (&z[k]) = (k == max - 1 ? NULL : &z[k + 1]);
    PREVIOUS (&z[k]) = (k == 0 ? NULL : &z[k - 1]);
  }
}

/*!
\brief whether mode must be coloured
\param m moid to colour
\return same
**/

static BOOL_T moid_needs_colouring (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (A68_TRUE);
  } else if (WHETHER (m, PROC_SYMBOL)) {
    return (A68_TRUE);
  } else if (WHETHER (m, FLEX_SYMBOL) || WHETHER (m, ROW_SYMBOL)) {
    return (A68_TRUE);
  } else if (WHETHER (m, STRUCT_SYMBOL) || WHETHER (m, UNION_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NULL; FORWARD (p)) {
      if (moid_needs_colouring (MOID (p))) {
        return (A68_TRUE);
      }
    }
    return (A68_FALSE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief colour all elements of a row
\param z fat pointer to descriptor
\param m mode of row
**/

static void colour_row_elements (A68_REF * z, MOID_T * m)
{
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  GET_DESCRIPTOR (arr, tup, z);
/* Empty rows are trivial since we don't recognise ghost elements. */
  if (get_row_size (tup, DIM (arr)) > 0) {
/* The multi-dimensional sweeper. */
    BYTE_T *elem = ADDRESS (&ARRAY (arr));
    BOOL_T done = A68_FALSE;
    initialise_internal_index (tup, DIM (arr));
    while (!done) {
      ADDR_T iindex = calculate_internal_index (tup, DIM (arr));
      ADDR_T addr = ROW_ELEMENT (arr, iindex);
      colour_object (&elem[addr], SUB (m));
      done = increment_internal_index (tup, DIM (arr));
    }
  }
}

/*!
\brief colour an (active) object
\param item pointer to item to colour
\param m mode of item
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
    if (INITIALISED (z) && IS_IN_HEAP (z)) {
      if (STATUS_TEST (REF_HANDLE (z), COOKIE_MASK)) {
        return;
      }
      STATUS_SET (REF_HANDLE (z), (COOKIE_MASK | COLOUR_MASK));
      if (!IS_NIL (*z)) {
        colour_object (ADDRESS (z), SUB (m));
      }
      STATUS_CLEAR (REF_HANDLE (z), COOKIE_MASK);
    }
  } else if (WHETHER (m, FLEX_SYMBOL) || WHETHER (m, ROW_SYMBOL) || m == MODE (STRING)) {
/* Claim the descriptor and the row itself. */
    A68_REF *z = (A68_REF *) item;
    if (INITIALISED (z) && IS_IN_HEAP (z)) {
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      if (STATUS_TEST (REF_HANDLE (z), COOKIE_MASK)) {
        return;
      }
/* An array is ALWAYS in the heap. */
      STATUS_SET (REF_HANDLE (z), (COOKIE_MASK | COLOUR_MASK));
      GET_DESCRIPTOR (arr, tup, z);
      if (REF_HANDLE (&(ARRAY (arr))) != NULL) {
/* Assume its initialisation. */
        MOID_T *n = DEFLEX (m);
        STATUS_SET (REF_HANDLE (&(ARRAY (arr))), COLOUR_MASK);
        if (moid_needs_colouring (SUB (n))) {
          colour_row_elements (z, n);
        }
      }
      STATUS_CLEAR (REF_HANDLE (z), COOKIE_MASK);
    }
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
/* STRUCTures - colour fields. */
    PACK_T *p = PACK (m);
    for (; p != NULL; FORWARD (p)) {
      colour_object (&item[OFFSET (p)], MOID (p));
    }
  } else if (WHETHER (m, UNION_SYMBOL)) {
/* UNIONs - a united object may contain a value that needs colouring. */
    A68_UNION *z = (A68_UNION *) item;
    if (INITIALISED (z)) {
      MOID_T *united_moid = (MOID_T *) VALUE (z);
      colour_object (&item[ALIGNED_SIZE_OF (A68_UNION)], united_moid);
    }
  } else if (WHETHER (m, PROC_SYMBOL)) {
/* PROCs - save a locale and the objects it points to. */
    A68_PROCEDURE *z = (A68_PROCEDURE *) item;
    if (INITIALISED (z) && LOCALE (z) != NULL && !(STATUS_TEST (LOCALE (z), COOKIE_MASK))) {
      BYTE_T *u = POINTER (LOCALE (z));
      PACK_T *s = PACK (MOID (z));
      STATUS_SET (LOCALE (z), (COOKIE_MASK | COLOUR_MASK));
      for (; s != NULL; FORWARD (s)) {
        if (VALUE ((A68_BOOL *) & u[0]) == A68_TRUE) {
          colour_object (&u[ALIGNED_SIZE_OF (A68_BOOL)], MOID (s));
        }
        u = &(u[ALIGNED_SIZE_OF (A68_BOOL) + MOID_SIZE (MOID (s))]);
      }
      STATUS_CLEAR (LOCALE (z), COOKIE_MASK);
    }
  } else if (m == MODE (SOUND)) {
/* Claim the data of a SOUND object, that is in the heap. */
    A68_SOUND *w = (A68_SOUND *) item;
    if (INITIALISED (w)) {
      STATUS_SET (REF_HANDLE (&(w->data)), (COOKIE_MASK | COLOUR_MASK));
    }
  }
}

/*!
\brief colour active objects in the heap
\param fp running frame pointer
**/

static void colour_heap (ADDR_T fp)
{
  while (fp != 0) {
    NODE_T *p = FRAME_TREE (fp);
    SYMBOL_TABLE_T *q = SYMBOL_TABLE (p);
    if (q != NULL) {
      TAG_T *i;
      for (i = q->identifiers; i != NULL; FORWARD (i)) {
        colour_object (FRAME_LOCAL (fp, OFFSET (i)), MOID (i));
      }
      for (i = q->anonymous; i != NULL; FORWARD (i)) {
        if (PRIO (i) == GENERATOR || PRIO (i) == PROTECT_FROM_SWEEP) {
          colour_object (FRAME_LOCAL (fp, OFFSET (i)), MOID (i));
        }
      }
    }
    fp = FRAME_DYNAMIC_LINK (fp);
  }
}

/*!
\brief join all active blocks in the heap
**/

static void defragment_heap (void)
{
  A68_HANDLE *z;
/* Free handles. */
  z = busy_handles;
  while (z != NULL) {
    if (!(STATUS_TEST (z, COLOUR_MASK)) && !(STATUS_TEST (z, NO_SWEEP_MASK))) {
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
      STATUS_CLEAR (z, ALLOCATED_MASK);
      garbage_bytes_freed += z->size;
      free_handle_count++;
      z = y;
    } else {
      FORWARD (z);
    }
  }
/* There can be no uncoloured allocated handle. */
  for (z = busy_handles; z != NULL; FORWARD (z)) {
    ABEND (!(STATUS_TEST (z, COLOUR_MASK)) && !(STATUS_TEST (z, NO_SWEEP_MASK)), "bad GC consistency", NULL);
  }
/* Defragment the heap. */
  heap_pointer = fixed_heap_pointer;
  for (z = busy_handles; z != NULL && NEXT (z) != NULL; FORWARD (z)) {
    ;
  }
  for (; z != NULL; z = PREVIOUS (z)) {
    BYTE_T *dst = HEAP_ADDRESS (heap_pointer);
    if (dst != POINTER (z)) {
      MOVE (dst, POINTER (z), (unsigned) z->size);
    }
    STATUS_CLEAR (z, (COLOUR_MASK | COOKIE_MASK));
    POINTER (z) = dst;
    heap_pointer += (z->size);
    ABEND (heap_pointer % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
  }
}

/*!
\brief clean up garbage and defragment the heap
\param p position in tree
\param fp running frame pointer
**/

void sweep_heap (NODE_T * p, ADDR_T fp)
{
/* Must start with fp = current frame_pointer. */
  A68_HANDLE *z;
  double t0, t1;
  t0 = seconds ();
  if (block_heap_compacter == 0) {
/* Unfree handles are subject to inspection. */
    for (z = busy_handles; z != NULL; FORWARD (z)) {
      STATUS_CLEAR (z, (COLOUR_MASK | COOKIE_MASK));
    }
/* Pour paint into the heap to reveal active objects. */
    colour_heap (fp);
/* Start freeing and compacting. */
    garbage_bytes_freed = 0;
    defragment_heap ();
/* Stats and logging. */
    garbage_collects++;
    (void) int_to_mp (p, garbage_freed, (int) garbage_bytes_freed, LONG_MP_DIGITS);
    (void) add_mp (p, garbage_total_freed, garbage_total_freed, garbage_freed, LONG_MP_DIGITS);
  }
  t1 = seconds ();
/* C optimiser can make last digit differ, so next condition is 
   needed to determine a positive time difference. */
  if ((t1 - t0) > ((double) clock_res / 2.0)) {
    garbage_seconds += (t1 - t0);
  } else {
    garbage_seconds += ((double) clock_res / 2.0);
  }
}

/*!
\brief yield a handle that will point to a block in the heap
\param p position in tree
\param a68m mode of object
\return handle that points to object
**/

static A68_HANDLE *give_handle (NODE_T * p, MOID_T * a68m)
{
  if (free_handles != NULL) {
    A68_HANDLE *x = free_handles;
    free_handles = NEXT (x);
    if (free_handles != NULL) {
      PREVIOUS (free_handles) = NULL;
    }
    STATUS (x) = ALLOCATED_MASK;
    POINTER (x) = NULL;
    SIZE (x) = 0;
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
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  return (NULL);
}

/*!
\brief give a block of heap for an object of indicated mode
\param p position in tree
\param mode mode of object
\param size size in bytes to allocate
\return fat pointer to object in the heap
**/

A68_REF heap_generator (NODE_T * p, MOID_T * mode, int size)
{
/* Align. */
  ABEND (size < 0, ERROR_INVALID_SIZE, NULL);
  size = A68_ALIGN (size);
/* Now give it. */
  if (heap_available () >= size) {
    A68_HANDLE *x;
    A68_REF z;
    PREEMPTIVE_SWEEP;
    STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | IN_HEAP_MASK);
    OFFSET (&z) = 0;
    x = give_handle (p, mode);
    SIZE (x) = size;
    POINTER (x) = HEAP_ADDRESS (heap_pointer);
    FILL (x->pointer, 0, size);
    SET_REF_SCOPE (&z, PRIMAL_SCOPE);
    REF_HANDLE (&z) = x;
    ABEND (((long) ADDRESS (&z)) % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
    heap_pointer += size;
    return (z);
  } else {
/* No heap space. First sweep the heap. */
    sweep_heap (p, frame_pointer);
    if (heap_available () > size) {
      return (heap_generator (p, mode, size));
    } else {
/* Still no heap space. We must abend. */
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
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
    return (A68_FALSE);
  }
  if (WHETHER (m, PROC_SYMBOL)) {
    return (A68_FALSE);
  }
  if (WHETHER (m, UNION_SYMBOL)) {
    return (A68_FALSE);
  }
  if (m == MODE (VOID)) {
    return (A68_FALSE);
  }
  return (A68_TRUE);
}

/*!
\brief prepare bounds
\param p position in tree
**/

static void genie_prepare_bounds (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOUNDS_LIST)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, BOUND)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, UNIT)) {
      if (NEXT (p) != NULL && (whether_one_of (NEXT (p), COLON_SYMBOL, DOTDOT_SYMBOL, NULL_ATTRIBUTE))) {
        EXECUTE_UNIT (p);
        p = NEXT_NEXT (p);
      } else {
/* Default lower bound */
        PUSH_PRIMITIVE (p, 1, A68_INT);
      }
      EXECUTE_UNIT (p);
    }
  }
}

/*!
\brief prepare bounds for a row
\param p position in tree
**/

void genie_generator_bounds (NODE_T * p)
{
  LOW_STACK_ALERT (p);
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOUNDS)) {
      genie_prepare_bounds (SUB (p));
    } else if (WHETHER (p, INDICANT)) {
      if (TAX (p) != NULL && MOID (TAX (p))->has_rows) {
        /* Continue from definition at MODE A = .. */
        genie_generator_bounds (DEF (p));
      }
    } else if (WHETHER (p, DECLARER) && needs_allocation (MOID (p)) == A68_FALSE) {
      return;
    } else {
      genie_generator_bounds (SUB (p));
    }
  }
}

/*!
\brief allocate a structure
\param p declarer in the syntax tree
**/

void genie_generator_field (NODE_T * p, BYTE_T ** q, NODE_T ** declarer, ADDR_T * sp, ADDR_T * max_sp)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, STRUCTURED_FIELD)) {
      genie_generator_field (SUB (p), q, declarer, sp, max_sp);
    }
    if (WHETHER (p, DECLARER)) {
      (*declarer) = SUB (p);
      FORWARD (p);
    }
    if (WHETHER (p, FIELD_IDENTIFIER)) {
      MOID_T *field_mode = MOID (*declarer);
      ADDR_T pop_sp = *sp;
      if (field_mode->has_rows && WHETHER_NOT (field_mode, UNION_SYMBOL)) {
        genie_generator_stowed (*declarer, *q, NULL, sp, max_sp);
      } else {
        MAX (*max_sp, *sp);
      }
      (*sp) = pop_sp;
      (*q) += MOID_SIZE (field_mode);
    }
  }
}

/*!
\brief allocate a structure
\param p declarer in the syntax tree
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
\param p declarer in the syntax tree
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
    genie_generator_struct (SUB_NEXT (p), &r, sp, max_sp);
    return;
  }
  if (WHETHER (p, FLEX_SYMBOL)) {
    FORWARD (p);
  }
  if (WHETHER (p, BOUNDS)) {
    ADDR_T bla = *max_sp;
    A68_REF desc;
    MOID_T *slice_mode = MOID (NEXT (p));
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    BYTE_T *bounds = STACK_ADDRESS (*sp);
    int k, dim = DIM (DEFLEX (MOID (p)));
    int elem_size = MOID_SIZE (slice_mode), row_size = 1;
    UP_SWEEP_SEMA;
    desc = heap_generator (p, MOID (p), dim * ALIGNED_SIZE_OF (A68_TUPLE) + ALIGNED_SIZE_OF (A68_ARRAY));
    GET_DESCRIPTOR (arr, tup, &desc);
    for (k = 0; k < dim; k++) {
      tup[k].lower_bound = VALUE ((A68_INT *) bounds);
      bounds += ALIGNED_SIZE_OF (A68_INT);
      tup[k].upper_bound = VALUE ((A68_INT *) bounds);
      bounds += ALIGNED_SIZE_OF (A68_INT);
      tup[k].span = row_size;
      tup[k].shift = tup[k].lower_bound * tup[k].span;
      row_size *= ROW_SIZE (&tup[k]);
    }
    DIM (arr) = dim;
    MOID (arr) = slice_mode;
    arr->elem_size = elem_size;
    arr->slice_offset = 0;
    arr->field_offset = 0;
    ARRAY (arr) = heap_generator (p, MOID (p), row_size * elem_size);
    (*sp) += (dim * 2 * ALIGNED_SIZE_OF (A68_INT));
    MAX (bla, *sp);
    if (slice_mode->has_rows && needs_allocation (slice_mode)) {
      BYTE_T *elem = ADDRESS (&(ARRAY (arr)));
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
\param p declarer in the syntax tree
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
    STATUS (&name) = (STATUS_MASK) (INITIALISED_MASK | IN_FRAME_MASK);
    REF_HANDLE (&name) = &nil_handle;
    name.offset = frame_pointer + FRAME_INFO_SIZE + OFFSET (tag);
    SET_REF_SCOPE (&name, frame_pointer);
  } else {
    name = heap_generator (p, mode, MOID_SIZE (mode));
    SET_REF_SCOPE (&name, PRIMAL_SCOPE);
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
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_generator (NODE_T * p)
{
  PROPAGATOR_T self;
  ADDR_T pop_sp = stack_pointer;
  A68_REF z;
  if (NEXT_SUB (p) != NULL) {
    genie_generator_bounds (NEXT_SUB (p));
  }
  genie_generator_internal (NEXT_SUB (p), MOID (p), TAX (p), ATTRIBUTE (SUB (p)), pop_sp);
  POP_REF (p, &z);
  stack_pointer = pop_sp;
  PUSH_REF (p, z);
  PROTECT_FROM_SWEEP_STACK (p);
  self.unit = genie_generator;
  self.source = p;
  return (self);
}
