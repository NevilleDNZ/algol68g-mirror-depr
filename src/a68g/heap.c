//! @file heap.c
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2022 J. Marcel van der Veer <algol68g@xs4all.nl>.
//
//! @section License
//
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the 
// Free Software Foundation; either version 3 of the License, or 
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
// more details. You should have received a copy of the GNU General Public 
// License along with this program. If not, see <http://www.gnu.org/licenses/>.

// Generator and garbage collector routines.
// 
// The generator allocates space in stack or heap and initialises dynamically sized objects.
// 
// A mark-and-gc garbage collector defragments the heap. When called, it walks
// the stack frames and marks the heap space that is still active. This marking
// process is called "colouring" here since we "pour paint" into the heap.
// The active blocks are then joined, the non-active blocks are forgotten.
// 
// When colouring the heap, "cookies" are placed in objects as to find circular
// references.
// 
// Algol68G introduces several anonymous tags in the symbol tables that save
// temporary REF or ROW results, so that they do not get prematurely swept.
// 
// The genie is not smart enough to handle every heap clog, e.g. when copying
// STOWED objects. This seems not very elegant, but garbage collectors in general
// cannot solve all core management problems. To avoid many of the "unforeseen"
// heap clogs, we try to keep heap occupation low by garbage collecting 
// occasionally, before it fills up completely. If this automatic mechanism does
// not help, one can always invoke the garbage collector by calling "gc heap"
// from Algol 68 source text.
// 
// Mark-and-collect is simple but since it walks recursive structures, it could
// exhaust the C-stack (segment violation). A rough check is in place.
// 
// For dynamically sized objects, first bounds are evaluated (right first, then down).
// The object is generated keeping track of the bound-count.
// 
//      ...
//      [#1]
//      STRUCT
//      (
//      [#2]
//      STRUCT
//      (
//      [#3] A a, b, ...
//      )
//      ,                       Advance bound-count here, max is #3
//      [#4] B a, b, ...
//      )
//      ,                       Advance bound-count here, max is #4
//      [#5] C a, b, ...
//      ...
// 
// Bound-count is maximised when generator_stowed is entered recursively. 
// Bound-count is advanced when completing a STRUCTURED_FIELD.
//
// Note that A68G will not extend stack frames. Thus only 'static' LOC generators
// are in the stack, and 'dynamic' LOC generators go into the heap. These local 
// REFs in the heap get local scope however, and A68G's approach differs from the 
// CDC ALGOL 68 approach that put all generators in the heap.
//
// Note that part of memory is called 'COMMON'. This is meant for future extension
// where a68g would need to point to external objects. The adressing scheme is that
// of a HEAP pointer - handle pointer + offset.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-frames.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

#define DEF_NODE(p) (NEXT_NEXT (NODE (TAX (p))))

//! @brief PROC VOID gc heap

void genie_gc_heap (NODE_T * p)
{
  gc_heap (p, A68_FP);
}

//! @brief PROC VOID preemptive gc heap

void genie_preemptive_gc_heap (NODE_T * p)
{
  PREEMPTIVE_GC (DEFAULT_PREEMPTIVE);
}

//! @brief INT blocks

void genie_block (NODE_T * p)
{
  PUSH_VALUE (p, 0, A68_INT);
}

//! @brief INT garbage collections

void genie_garbage_collections (NODE_T * p)
{
  PUSH_VALUE (p, A68_GC (sweeps), A68_INT);
}

//! @brief INT garbage refused

void genie_garbage_refused (NODE_T * p)
{
  PUSH_VALUE (p, A68_GC (refused), A68_INT);
}

//! @brief LONG INT garbage freed

void genie_garbage_freed (NODE_T * p)
{
  PUSH_VALUE (p, A68_GC (total), A68_INT);
}

//! @brief REAL garbage seconds

void genie_garbage_seconds (NODE_T * p)
{
// Note that this timing is a rough cut.
  PUSH_VALUE (p, A68_GC (seconds), A68_REAL);
}

//! @brief Size available for an object in the heap.

unsigned heap_available (void)
{
  return A68 (heap_size) - A68_HP;
}

//! @brief Initialise heap management.

void genie_init_heap (NODE_T * p)
{
  A68_HANDLE *z;
  int k, max;
  (void) p;
  if (A68_HEAP == NO_BYTE) {
    diagnostic (A68_RUNTIME_ERROR, TOP_NODE (&A68_JOB), ERROR_OUT_OF_CORE);
    exit_genie (TOP_NODE (&A68_JOB), A68_RUNTIME_ERROR);
  }
  if (A68_HANDLES == NO_BYTE) {
    diagnostic (A68_RUNTIME_ERROR, TOP_NODE (&A68_JOB), ERROR_OUT_OF_CORE);
    exit_genie (TOP_NODE (&A68_JOB), A68_RUNTIME_ERROR);
  }
  A68_GC (seconds) = 0;
  A68_GC (total) = 0;
  A68_GC (sweeps) = 0;
  A68_GC (refused) = 0;
  ABEND (A68 (fixed_heap_pointer) >= (A68 (heap_size) - MIN_MEM_SIZE), ERROR_OUT_OF_CORE, __func__);
  A68_HP = A68 (fixed_heap_pointer);
  A68 (heap_is_fluid) = A68_FALSE;
// Assign handle space.
  z = (A68_HANDLE *) A68_HANDLES;
  A68_GC (available_handles) = z;
  A68_GC (busy_handles) = NO_HANDLE;
  max = (unsigned) A68 (handle_pool_size) / (unsigned) sizeof (A68_HANDLE);
  A68_GC (free_handles) = max;
  A68_GC (max_handles) = max;
  for (k = 0; k < max; k++) {
    STATUS (&(z[k])) = NULL_MASK;
    POINTER (&(z[k])) = NO_BYTE;
    SIZE (&(z[k])) = 0;
    NEXT (&z[k]) = (k == max - 1 ? NO_HANDLE : &z[k + 1]);
    PREVIOUS (&z[k]) = (k == 0 ? NO_HANDLE : &z[k - 1]);
  }
}

//! @brief Whether mode must be coloured.

static BOOL_T moid_needs_colouring (MOID_T * m)
{
  if (IS_REF (m)) {
    return A68_TRUE;
  } else if (IS (m, PROC_SYMBOL)) {
    return A68_TRUE;
  } else if (IS_FLEX (m) || IS_ROW (m)) {
    return A68_TRUE;
  } else if (IS_STRUCT (m) || IS_UNION (m)) {
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      if (moid_needs_colouring (MOID (p))) {
        return A68_TRUE;
      }
    }
    return A68_FALSE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Colour all elements of a row.

static void colour_row_elements (A68_REF * z, MOID_T * m)
{
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  GET_DESCRIPTOR (arr, tup, z);
  if (get_row_size (tup, DIM (arr)) == 0) {
// Empty rows have a ghost elements.
    BYTE_T *elem = ADDRESS (&ARRAY (arr));
    colour_object (&elem[0], SUB (m));
  } else {
// The multi-dimensional garbage collector.
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

//! @brief Colour an (active) object.

void colour_object (BYTE_T * item, MOID_T * m)
{
  if (item == NO_BYTE || m == NO_MOID) {
    return;
  }
  if (!moid_needs_colouring (m)) {
    return;
  }
// Deeply recursive objects might exhaust the stack.
  LOW_STACK_ALERT (NO_NODE);
  if (IS_REF (m)) {
// REF AMODE colour pointer and object to which it refers.
    A68_REF *z = (A68_REF *) item;
    if (INITIALISED (z) && IS_IN_HEAP (z)) {
      if (STATUS_TEST (REF_HANDLE (z), COOKIE_MASK)) {
        return;
      }
      STATUS_SET (REF_HANDLE (z), (COOKIE_MASK | COLOUR_MASK));
      if (!IS_NIL (*z)) {
        colour_object (ADDRESS (z), SUB (m));
      }
//    STATUS_CLEAR (REF_HANDLE (z), COOKIE_MASK);.
    }
  } else if (IF_ROW (m)) {
// Claim the descriptor and the row itself.
    A68_REF *z = (A68_REF *) item;
    if (INITIALISED (z) && IS_IN_HEAP (z)) {
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      if (STATUS_TEST (REF_HANDLE (z), COOKIE_MASK)) {
        return;
      }
// An array is ALWAYS in the heap.
      STATUS_SET (REF_HANDLE (z), (COOKIE_MASK | COLOUR_MASK));
      GET_DESCRIPTOR (arr, tup, z);
      if (REF_HANDLE (&(ARRAY (arr))) != NO_HANDLE) {
// Assume its initialisation.
        MOID_T *n = DEFLEX (m);
        STATUS_SET (REF_HANDLE (&(ARRAY (arr))), COLOUR_MASK);
        if (moid_needs_colouring (SUB (n))) {
          colour_row_elements (z, n);
        }
      }
//    STATUS_CLEAR (REF_HANDLE (z), COOKIE_MASK);.
      (void) tup;
    }
  } else if (IS_STRUCT (m)) {
// STRUCTures - colour fields.
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      colour_object (&item[OFFSET (p)], MOID (p));
    }
  } else if (IS_UNION (m)) {
// UNIONs - a united object may contain a value that needs colouring.
    A68_UNION *z = (A68_UNION *) item;
    if (INITIALISED (z)) {
      MOID_T *united_moid = (MOID_T *) VALUE (z);
      colour_object (&item[A68_UNION_SIZE], united_moid);
    }
  } else if (IS (m, PROC_SYMBOL)) {
// PROCs - save a locale and the objects it points to.
    A68_PROCEDURE *z = (A68_PROCEDURE *) item;
    if (INITIALISED (z) && LOCALE (z) != NO_HANDLE && !(STATUS_TEST (LOCALE (z), COOKIE_MASK))) {
      BYTE_T *u = POINTER (LOCALE (z));
      PACK_T *s = PACK (MOID (z));
      STATUS_SET (LOCALE (z), (COOKIE_MASK | COLOUR_MASK));
      for (; s != NO_PACK; FORWARD (s)) {
        if (VALUE ((A68_BOOL *) & u[0]) == A68_TRUE) {
          colour_object (&u[SIZE (M_BOOL)], MOID (s));
        }
        u = &(u[SIZE (M_BOOL) + SIZE (MOID (s))]);
      }
//    STATUS_CLEAR (LOCALE (z), COOKIE_MASK);.
    }
  } else if (m == M_SOUND) {
// Claim the data of a SOUND object, that is in the heap.
    A68_SOUND *w = (A68_SOUND *) item;
    if (INITIALISED (w)) {
      STATUS_SET (REF_HANDLE (&(DATA (w))), (COOKIE_MASK | COLOUR_MASK));
    }
  }
}

//! @brief Colour active objects in the heap.

static void colour_heap (ADDR_T fp)
{
  while (fp != 0) {
    NODE_T *p = FRAME_TREE (fp);
    TABLE_T *q = TABLE (p);
    if (q != NO_TABLE) {
      TAG_T *i;
      for (i = IDENTIFIERS (q); i != NO_TAG; FORWARD (i)) {
        colour_object (FRAME_LOCAL (fp, OFFSET (i)), MOID (i));
      }
      for (i = ANONYMOUS (q); i != NO_TAG; FORWARD (i)) {
        if (PRIO (i) == GENERATOR) {
          colour_object (FRAME_LOCAL (fp, OFFSET (i)), MOID (i));
        }
      }
    }
    fp = FRAME_DYNAMIC_LINK (fp);
  }
}

//! @brief Join all active blocks in the heap.

static void defragment_heap (void)
{
  A68_HANDLE *z;
// Free handles.
  z = A68_GC (busy_handles);
  while (z != NO_HANDLE) {
    if (!(STATUS_TEST (z, COLOUR_MASK)) && !(STATUS_TEST (z, BLOCK_GC_MASK))) {
      A68_HANDLE *y = NEXT (z);
      if (PREVIOUS (z) == NO_HANDLE) {
        A68_GC (busy_handles) = NEXT (z);
      } else {
        NEXT (PREVIOUS (z)) = NEXT (z);
      }
      if (NEXT (z) != NO_HANDLE) {
        PREVIOUS (NEXT (z)) = PREVIOUS (z);
      }
      NEXT (z) = A68_GC (available_handles);
      PREVIOUS (z) = NO_HANDLE;
      if (NEXT (z) != NO_HANDLE) {
        PREVIOUS (NEXT (z)) = z;
      }
      A68_GC (available_handles) = z;
      STATUS_CLEAR (z, ALLOCATED_MASK);
      A68_GC (freed) += SIZE (z);
      A68_GC (free_handles)++;
      z = y;
    } else {
      FORWARD (z);
    }
  }
// There can be no uncoloured allocated handle.
  for (z = A68_GC (busy_handles); z != NO_HANDLE; FORWARD (z)) {
    ABEND (!(STATUS_TEST (z, COLOUR_MASK)) && !(STATUS_TEST (z, BLOCK_GC_MASK)), ERROR_INTERNAL_CONSISTENCY, __func__);
  }
// Defragment the heap.
  A68_HP = A68 (fixed_heap_pointer);
  for (z = A68_GC (busy_handles); z != NO_HANDLE && NEXT (z) != NO_HANDLE; FORWARD (z)) {
    ;
  }
  for (; z != NO_HANDLE; BACKWARD (z)) {
    BYTE_T *dst = HEAP_ADDRESS (A68_HP);
    if (dst != POINTER (z)) {
      MOVE (dst, POINTER (z), (unsigned) SIZE (z));
    }
    STATUS_CLEAR (z, (COLOUR_MASK | COOKIE_MASK));
    POINTER (z) = dst;
    A68_HP += (SIZE (z));
    ABEND (A68_HP % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, __func__);
  }
}

//! @brief Clean up garbage and defragment the heap.

void gc_heap (NODE_T * p, ADDR_T fp)
{
// Must start with fp = current frame_pointer.
  A68_HANDLE *z;
  REAL_T t0, t1;
#if defined (BUILD_PARALLEL_CLAUSE)
  if (OTHER_THREAD (FRAME_THREAD_ID (A68_FP), A68_PAR (main_thread_id))) {
    A68_GC (refused)++;
    return;
  }
#endif
// Take no risk when intermediate results are on the stack.
  if (A68_SP != A68 (stack_start)) {
    A68_GC (refused)++;
    return;
  }
// Give it a whirl then.
  t0 = seconds ();
// Unfree handles are subject to inspection.
  for (z = A68_GC (busy_handles); z != NO_HANDLE; FORWARD (z)) {
    STATUS_CLEAR (z, (COLOUR_MASK | COOKIE_MASK));
  }
// Pour paint into the heap to reveal active objects.
  colour_heap (fp);
// Start freeing and compacting.
  A68_GC (freed) = 0;
  defragment_heap ();
// Stats and logging.
  A68_GC (total) += A68_GC (freed);
  A68_GC (sweeps)++;
  t1 = seconds ();
// C optimiser can make last digit differ, so next condition is 
// needed to determine a positive time difference
  if ((t1 - t0) > ((REAL_T) A68 (clock_res) / 2.0)) {
    A68_GC (seconds) += (t1 - t0);
  } else {
    A68_GC (seconds) += ((REAL_T) A68 (clock_res) / 2.0);
  }
// Call the event handler.
  genie_call_event_routine (p, M_PROC_VOID, &A68 (on_gc_event), A68_SP, A68_FP);
}

//! @brief Yield a handle that will point to a block in the heap.

static A68_HANDLE *give_handle (NODE_T * p, MOID_T * a68m)
{
  if (A68_GC (available_handles) != NO_HANDLE) {
    A68_HANDLE *x = A68_GC (available_handles);
    A68_GC (available_handles) = NEXT (x);
    if (A68_GC (available_handles) != NO_HANDLE) {
      PREVIOUS (A68_GC (available_handles)) = NO_HANDLE;
    }
    STATUS (x) = ALLOCATED_MASK;
    POINTER (x) = NO_BYTE;
    SIZE (x) = 0;
    MOID (x) = a68m;
    NEXT (x) = A68_GC (busy_handles);
    PREVIOUS (x) = NO_HANDLE;
    if (NEXT (x) != NO_HANDLE) {
      PREVIOUS (NEXT (x)) = x;
    }
    A68_GC (busy_handles) = x;
    A68_GC (free_handles)--;
    return x;
  } else {
// Do not auto-GC!.
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  return NO_HANDLE;
}

//! @brief Give a block of heap for an object of indicated mode.

A68_REF heap_generator (NODE_T * p, MOID_T * mode, int size)
{
// Align.
  ABEND (size < 0, ERROR_INVALID_SIZE, __func__);
  size = A68_ALIGN (size);
// Now give it.
  if (heap_available () >= size) {
    A68_HANDLE *x;
    A68_REF z;
    STATUS (&z) = (STATUS_MASK_T) (INIT_MASK | IN_HEAP_MASK);
    OFFSET (&z) = 0;
    x = give_handle (p, mode);
    SIZE (x) = size;
    POINTER (x) = HEAP_ADDRESS (A68_HP);
    FILL (POINTER (x), 0, size);
    REF_SCOPE (&z) = PRIMAL_SCOPE;
    REF_HANDLE (&z) = x;
    ABEND (((long) ADDRESS (&z)) % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, __func__);
    A68_HP += size;
    return z;
  } else {
// Do not auto-GC!.
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
    return nil_ref;
  }
}

// Following implements the generator.

//! @brief Whether a moid needs work in allocation.

static BOOL_T mode_needs_allocation (MOID_T * m)
{
  if (IS_UNION (m)) {
    return A68_FALSE;
  } else {
    return HAS_ROWS (m);
  }
}

//! @brief Prepare bounds.

static void genie_compute_bounds (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, BOUNDS_LIST)) {
      genie_compute_bounds (SUB (p));
    } else if (IS (p, BOUND)) {
      genie_compute_bounds (SUB (p));
    } else if (IS (p, UNIT)) {
      if (NEXT (p) != NO_NODE && (is_one_of (NEXT (p), COLON_SYMBOL, DOTDOT_SYMBOL, STOP))) {
        EXECUTE_UNIT (p);
        p = NEXT_NEXT (p);
      } else {
// Default lower bound.
        PUSH_VALUE (p, 1, A68_INT);
      }
      EXECUTE_UNIT (p);
    }
  }
}

//! @brief Prepare bounds for a row.

void genie_generator_bounds (NODE_T * p)
{
  LOW_STACK_ALERT (p);
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, BOUNDS)) {
      genie_compute_bounds (SUB (p));
    } else if (IS (p, INDICANT) && IS_LITERALLY (p, "STRING")) {
      return;
    } else if (IS (p, INDICANT)) {
      if (TAX (p) != NO_TAG && HAS_ROWS (MOID (TAX (p)))) {
// Continue from definition at MODE A = ....
        genie_generator_bounds (DEF_NODE (p));
      }
    } else if (IS (p, DECLARER) && !mode_needs_allocation (MOID (p))) {
      return;
    } else {
      genie_generator_bounds (SUB (p));
    }
  }
}

//! @brief Allocate a structure.

void genie_generator_field (NODE_T * p, BYTE_T ** faddr, NODE_T ** decl, ADDR_T * cur_sp, ADDR_T * top_sp)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, STRUCTURED_FIELD)) {
      genie_generator_field (SUB (p), faddr, decl, cur_sp, top_sp);
    }
    if (IS (p, DECLARER)) {
      (*decl) = SUB (p);
      FORWARD (p);
    }
    if (IS (p, FIELD_IDENTIFIER)) {
      MOID_T *fmoid = MOID (*decl);
      if (HAS_ROWS (fmoid) && ISNT (fmoid, UNION_SYMBOL)) {
        ADDR_T pop_sp = *cur_sp;
        genie_generator_stowed (*decl, *faddr, NO_VAR, cur_sp);
        *top_sp = *cur_sp;
        *cur_sp = pop_sp;
      }
      (*faddr) += SIZE (fmoid);
    }
  }
}

//! @brief Allocate a structure.

void genie_generator_struct (NODE_T * p, BYTE_T ** faddr, ADDR_T * cur_sp)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, STRUCTURED_FIELD_LIST)) {
      genie_generator_struct (SUB (p), faddr, cur_sp);
    } else if (IS (p, STRUCTURED_FIELD)) {
      NODE_T *decl = NO_NODE;
      ADDR_T top_sp = *cur_sp;
      genie_generator_field (SUB (p), faddr, &decl, cur_sp, &top_sp);
      *cur_sp = top_sp;
    }
  }
}

//! @brief Allocate a stowed object.

void genie_generator_stowed (NODE_T * p, BYTE_T * addr, NODE_T ** decl, ADDR_T * cur_sp)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, INDICANT) && IS_LITERALLY (p, "STRING")) {
// The standard prelude definition is hard coded here.
    *((A68_REF *) addr) = empty_string (p);
    return;
  } else if (IS (p, INDICANT) && TAX (p) != NO_TAG) {
// Continue from definition at MODE A = ..
    genie_generator_stowed (DEF_NODE (p), addr, decl, cur_sp);
    return;
  } else if (IS (p, DECLARER) && mode_needs_allocation (MOID (p))) {
    genie_generator_stowed (SUB (p), addr, decl, cur_sp);
    return;
  } else if (IS_STRUCT (p)) {
    BYTE_T *faddr = addr;
    genie_generator_struct (SUB_NEXT (p), &faddr, cur_sp);
    return;
  } else if (IS_FLEX (p)) {
    genie_generator_stowed (NEXT (p), addr, decl, cur_sp);
    return;
  } else if (IS (p, BOUNDS)) {
    A68_REF desc;
    MOID_T *rmod = MOID (p), *smod = MOID (NEXT (p));
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    BYTE_T *bounds = STACK_ADDRESS (*cur_sp);
    int k, dim = DIM (DEFLEX (rmod));
    int esiz = SIZE (smod), rsiz = 1;
    BOOL_T alloc_sub, alloc_str;
    NODE_T *in = SUB_NEXT (p);
    if (IS (in, INDICANT) && IS_LITERALLY (in, "STRING")) {
      alloc_str = A68_TRUE;
      alloc_sub = A68_FALSE;
    } else {
      alloc_sub = mode_needs_allocation (smod);
      alloc_str = A68_FALSE;
    }
    desc = heap_generator (p, rmod, DESCRIPTOR_SIZE (dim));
    GET_DESCRIPTOR (arr, tup, &desc);
    for (k = 0; k < dim; k++) {
      CHECK_INIT (p, INITIALISED ((A68_INT *) bounds), M_INT);
      LWB (&tup[k]) = VALUE ((A68_INT *) bounds);
      bounds += SIZE (M_INT);
      CHECK_INIT (p, INITIALISED ((A68_INT *) bounds), M_INT);
      UPB (&tup[k]) = VALUE ((A68_INT *) bounds);
      bounds += SIZE (M_INT);
      SPAN (&tup[k]) = rsiz;
      SHIFT (&tup[k]) = LWB (&tup[k]) * SPAN (&tup[k]);
      rsiz *= ROW_SIZE (&tup[k]);
    }
    DIM (arr) = dim;
    MOID (arr) = smod;
    ELEM_SIZE (arr) = esiz;
    SLICE_OFFSET (arr) = 0;
    FIELD_OFFSET (arr) = 0;
    (*cur_sp) += (dim * 2 * SIZE (M_INT));
// Generate a new row. Note that STRING is handled explicitly since
// it has implicit bounds 
    if (rsiz == 0) {
// Generate a ghost element.
      ADDR_T top_sp = *cur_sp;
      BYTE_T *elem;
      ARRAY (arr) = heap_generator (p, rmod, esiz);
      elem = ADDRESS (&(ARRAY (arr)));
      if (alloc_sub) {
        genie_generator_stowed (NEXT (p), &(elem[0]), NO_VAR, cur_sp);
        top_sp = *cur_sp;
      } else if (alloc_str) {
        *(A68_REF *) elem = empty_string (p);
      }
      (*cur_sp) = top_sp;
    } else {
      ADDR_T pop_sp = *cur_sp, top_sp = *cur_sp;
      BYTE_T *elem;
      ARRAY (arr) = heap_generator (p, rmod, rsiz * esiz);
      elem = ADDRESS (&(ARRAY (arr)));
      for (k = 0; k < rsiz; k++) {
        if (alloc_sub) {
          (*cur_sp) = pop_sp;
          genie_generator_stowed (NEXT (p), &(elem[k * esiz]), NO_VAR, cur_sp);
          top_sp = *cur_sp;
        } else if (alloc_str) {
          *(A68_REF *) (&(elem[k * esiz])) = empty_string (p);
        }
      }
      (*cur_sp) = top_sp;
    }
    *(A68_REF *) addr = desc;
    return;
  }
}

//! @brief Generate space and push a REF.

void genie_generator_internal (NODE_T * p, MOID_T * ref_mode, TAG_T * tag, LEAP_T leap, ADDR_T sp)
{
// Set up a REF MODE object, either in the stack or in the heap.
  MOID_T *mode = SUB (ref_mode);
  A68_REF name = nil_ref;
  if (leap == LOC_SYMBOL) {
    STATUS (&name) = (STATUS_MASK_T) (INIT_MASK | IN_FRAME_MASK);
    REF_HANDLE (&name) = (A68_HANDLE *) & nil_handle;
    OFFSET (&name) = A68_FP + FRAME_INFO_SIZE + OFFSET (tag);
    REF_SCOPE (&name) = A68_FP;
  } else if (leap == -LOC_SYMBOL && NON_LOCAL (p) != NO_TABLE) {
    ADDR_T lev;
    name = heap_generator (p, mode, SIZE (mode));
    FOLLOW_SL (lev, LEVEL (NON_LOCAL (p)));
    REF_SCOPE (&name) = lev;
  } else if (leap == -LOC_SYMBOL) {
    name = heap_generator (p, mode, SIZE (mode));
    REF_SCOPE (&name) = A68_FP;
  } else if (leap == HEAP_SYMBOL || leap == -HEAP_SYMBOL) {
    name = heap_generator (p, mode, SIZE (mode));
    REF_SCOPE (&name) = PRIMAL_SCOPE;
  } else if (leap == NEW_SYMBOL || leap == -NEW_SYMBOL) {
    name = heap_generator (p, mode, SIZE (mode));
    REF_SCOPE (&name) = PRIMAL_SCOPE;
  } else {
    ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
  }
  if (HAS_ROWS (mode)) {
    ADDR_T cur_sp = sp;
    genie_generator_stowed (p, ADDRESS (&name), NO_VAR, &cur_sp);
  }
  PUSH_REF (p, name);
}

//! @brief Push a name refering to allocated space.

PROP_T genie_generator (NODE_T * p)
{
  PROP_T self;
  ADDR_T pop_sp = A68_SP;
  A68_REF z;
  if (NEXT_SUB (p) != NO_NODE) {
    genie_generator_bounds (NEXT_SUB (p));
  }
  genie_generator_internal (NEXT_SUB (p), MOID (p), TAX (p), -ATTRIBUTE (SUB (p)), pop_sp);
  POP_REF (p, &z);
  A68_SP = pop_sp;
  PUSH_REF (p, z);
  UNIT (&self) = genie_generator;
  SOURCE (&self) = p;
  return self;
}

// Control of C heap

//! @brief Discard_heap.

void discard_heap (void)
{
  if (A68_HEAP != NO_BYTE) {
    a68_free (A68_HEAP);
  }
  A68 (fixed_heap_pointer) = 0;
  A68 (temp_heap_pointer) = 0;
}
