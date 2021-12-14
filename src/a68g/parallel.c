//! @file parallel.c
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2021 J. Marcel van der Veer <algol68g@xs4all.nl>.
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

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-frames.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-parser.h"
#include "a68g-transput.h"

// This code implements a parallel clause for Algol68G. 
// The parallel clause has been included for educational purposes; 
// this implementation is not the most efficient one.
// 
// POSIX threads are used to have separate registers and stack for each concurrent
// unit. Algol68G parallel units behave as POSIX threads - they have private 
// stacks. Hence an assignation to an object in another thread, does not change 
// that object in that other thread. Also jumps between threads are forbidden.

#if defined (BUILD_PARALLEL_CLAUSE)

// static pthread_mutex_t unit_sema = PTHREAD_MUTEX_INITIALIZER;

static void save_stacks (pthread_t);
static void restore_stacks (pthread_t);

#define SAVE_STACK(stk, st, si) {\
  A68_STACK_DESCRIPTOR *s = (stk);\
  BYTE_T *start = (st);\
  int size = (si);\
  if (size > 0) {\
    if (!((s != NULL) && (BYTES (s) > 0) && (size <= BYTES (s)))) {\
      if (SWAP (s) != NO_BYTE) {\
        a68_free (SWAP (s));\
      }\
      SWAP (s) = (BYTE_T *) get_heap_space ((size_t) size);\
      ABEND (SWAP (s) == NULL, ERROR_OUT_OF_CORE, __func__);\
    }\
    START (s) = start;\
    BYTES (s) = size;\
    COPY (SWAP (s), start, size);\
  } else {\
    START (s) = start;\
    BYTES (s) = 0;\
    if (SWAP (s) != NO_BYTE) {\
      a68_free (SWAP (s));\
    }\
    SWAP (s) = NO_BYTE;\
  }}

#define RESTORE_STACK(stk) {\
  A68_STACK_DESCRIPTOR *s = (stk);\
  if (s != NULL && BYTES (s) > 0) {\
    COPY (START (s), SWAP (s), BYTES (s));\
  }}

#define GET_THREAD_INDEX(z, ptid) {\
  int _k_;\
  pthread_t _tid_ = (ptid);\
  (z) = -1;\
  for (_k_ = 0; _k_ < A68_PAR (context_index) && (z) == -1; _k_++) {\
    if (SAME_THREAD (_tid_, ID (&(A68_PAR (context)[_k_])))) {\
      (z) = _k_;\
    }\
  }\
  ABEND ((z) == -1, ERROR_INTERNAL_CONSISTENCY, __func__);\
  }

#define ERROR_THREAD_FAULT "thread fault"

#define LOCK_THREAD {\
  ABEND (pthread_mutex_lock (&A68_PAR (unit_sema)) != 0, ERROR_THREAD_FAULT, __func__);\
  }

#define UNLOCK_THREAD {\
  ABEND (pthread_mutex_unlock (&A68_PAR (unit_sema)) != 0, ERROR_THREAD_FAULT, __func__);\
  }

//! @brief Does system stack grow up or down?.

static inline int stack_direction (BYTE_T * lwb)
{
  BYTE_T upb;
  if (&upb > lwb) {
    return (int) sizeof (BYTE_T);
  } else if (&upb < lwb) {
    return - (int) sizeof (BYTE_T);
  } else {
    ASSERT (A68_FALSE);
    return 0; // Pro forma
  }
}

//! @brief Whether we are in the main thread.

BOOL_T is_main_thread (void)
{
  return SAME_THREAD (A68_PAR (main_thread_id), pthread_self ());
}

//! @brief End a thread, beit normally or not.

void genie_abend_thread (void)
{
  int k;
  GET_THREAD_INDEX (k, pthread_self ());
  ACTIVE (&(A68_PAR (context)[k])) = A68_FALSE;
  UNLOCK_THREAD;
  pthread_exit (NULL);
}

//! @brief When we end execution in a parallel clause we zap all threads.

void genie_set_exit_from_threads (int ret)
{
  A68_PAR (abend_all_threads) = A68_TRUE;
  A68_PAR (exit_from_threads) = A68_TRUE;
  A68_PAR (par_return_code) = ret;
  genie_abend_thread ();
}

//! @brief When we jump out of a parallel clause we zap all threads.

void genie_abend_all_threads (NODE_T * p, jmp_buf * jump_stat, NODE_T * label)
{
  (void) p;
  A68_PAR (abend_all_threads) = A68_TRUE;
  A68_PAR (exit_from_threads) = A68_FALSE;
  A68_PAR (jump_buffer) = jump_stat;
  A68_PAR (jump_label) = label;
  if (!is_main_thread ()) {
    genie_abend_thread ();
  }
}

//! @brief Save this thread and try to start another.

static void try_change_thread (NODE_T * p)
{
  if (is_main_thread ()) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
// Release the unit_sema so another thread can take it up ...
    save_stacks (pthread_self ());
    UNLOCK_THREAD;
// ... and take it up again!.
    LOCK_THREAD;
    restore_stacks (pthread_self ());
  }
}

//! @brief Store the stacks of threads.

static void save_stacks (pthread_t t)
{
  ADDR_T p, q, u, v;
  int k;
  GET_THREAD_INDEX (k, t);
// Store stack pointers.
  CUR_PTR (&FRAME (&(A68_PAR (context)[k]))) = A68_FP;
  CUR_PTR (&STACK (&(A68_PAR (context)[k]))) = A68_SP;
// Swap out evaluation stack.
  p = A68_SP;
  q = INI_PTR (&STACK (&(A68_PAR (context)[k])));
  SAVE_STACK (&(STACK (&(A68_PAR (context)[k]))), STACK_ADDRESS (q), p - q);
// Swap out frame stack.
  p = A68_FP;
  q = INI_PTR (&FRAME (&(A68_PAR (context)[k])));
  u = p + FRAME_SIZE (p);
  v = q + FRAME_SIZE (q);
// Consider the embedding thread.
  SAVE_STACK (&(FRAME (&(A68_PAR (context)[k]))), FRAME_ADDRESS (v), u - v);
}

//! @brief Restore stacks of thread.

static void restore_stacks (pthread_t t)
{
  if (ERROR_COUNT (&(A68 (job))) > 0 || A68_PAR (abend_all_threads)) {
    genie_abend_thread ();
  } else {
    int k;
    GET_THREAD_INDEX (k, t);
// Restore stack pointers.
    get_stack_size ();
    A68 (system_stack_offset) = THREAD_STACK_OFFSET (&(A68_PAR (context)[k]));
    A68_FP = CUR_PTR (&FRAME (&(A68_PAR (context)[k])));
    A68_SP = CUR_PTR (&STACK (&(A68_PAR (context)[k])));
// Restore stacks.
    RESTORE_STACK (&(STACK (&(A68_PAR (context)[k]))));
    RESTORE_STACK (&(FRAME (&(A68_PAR (context)[k]))));
  }
}

//! @brief Check whether parallel units have terminated.

static void check_parallel_units (BOOL_T * active, pthread_t parent)
{
  int k;
  for (k = 0; k < A68_PAR (context_index); k++) {
    if (parent == PARENT (&(A68_PAR (context)[k]))) {
      (*active) |= ACTIVE (&(A68_PAR (context)[k]));
    }
  }
}

//! @brief Execute one unit from a PAR clause.

static void *start_unit (void *arg)
{
  pthread_t t;
  int k;
  BYTE_T stack_offset;
  NODE_T *p;
  (void) arg;
  LOCK_THREAD;
  t = pthread_self ();
  GET_THREAD_INDEX (k, t);
  THREAD_STACK_OFFSET (&(A68_PAR (context)[k])) = (BYTE_T *) (&stack_offset - stack_direction (&stack_offset) * STACK_USED (&A68_PAR (context)[k]));
  restore_stacks (t);
  p = (NODE_T *) (UNIT (&(A68_PAR (context)[k])));
  EXECUTE_UNIT_TRACE (p);
  genie_abend_thread ();
  return (void *) NULL;
}

//! @brief Execute parallel units.

static void start_parallel_units (NODE_T * p, pthread_t parent)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      pthread_t new_id;
      pthread_attr_t new_at;
      size_t ss;
      BYTE_T stack_offset;
      A68_THREAD_CONTEXT *u;
// Set up a thread for this unit.
      if (A68_PAR (context_index) >= THREAD_MAX) {
        static char msg[BUFFER_SIZE];
        snprintf (msg, SNPRINTF_SIZE, "platform supports %d parallel units", THREAD_MAX);
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OVERFLOW, msg);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
// Fill out a context for this thread.
      u = &((A68_PAR (context)[A68_PAR (context_index)]));
      UNIT (u) = p;
      STACK_USED (u) = SYSTEM_STACK_USED;
      THREAD_STACK_OFFSET (u) = NO_BYTE;
      CUR_PTR (&STACK (u)) = A68_SP;
      CUR_PTR (&FRAME (u)) = A68_FP;
      INI_PTR (&STACK (u)) = A68_PAR (sp0);
      INI_PTR (&FRAME (u)) = A68_PAR (fp0);
      SWAP (&STACK (u)) = NO_BYTE;
      SWAP (&FRAME (u)) = NO_BYTE;
      START (&STACK (u)) = NO_BYTE;
      START (&FRAME (u)) = NO_BYTE;
      BYTES (&STACK (u)) = 0;
      BYTES (&FRAME (u)) = 0;
      ACTIVE (u) = A68_TRUE;
// Create the thread.
      errno = 0;
      if (pthread_attr_init (&new_at) != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (pthread_attr_setstacksize (&new_at, (size_t) A68 (stack_size)) != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (pthread_attr_getstacksize (&new_at, &ss) != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      ABEND ((size_t) ss != (size_t) A68 (stack_size), ERROR_ACTION, __func__);
      if (pthread_create (&new_id, &new_at, start_unit, NULL) != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      PARENT (u) = parent;
      ID (u) = new_id;
      A68_PAR (context_index)++;
      save_stacks (new_id);
    } else {
      start_parallel_units (SUB (p), parent);
    }
  }
}

//! @brief Execute one unit from a PAR clause.

static void *start_genie_parallel (void *arg)
{
  pthread_t t;
  int k;
  BYTE_T stack_offset;
  NODE_T *p;
  BOOL_T units_active;
  (void) arg;
  LOCK_THREAD;
  t = pthread_self ();
  GET_THREAD_INDEX (k, t);
  THREAD_STACK_OFFSET (&(A68_PAR (context)[k])) = (BYTE_T *) (&stack_offset - stack_direction (&stack_offset) * STACK_USED (&(A68_PAR (context)[k])));
  restore_stacks (t);
  p = (NODE_T *) (UNIT (&(A68_PAR (context)[k])));
// This is the thread spawned by the main thread, we spawn parallel units and await their completion.
  start_parallel_units (SUB (p), t);
  do {
    units_active = A68_FALSE;
    check_parallel_units (&units_active, pthread_self ());
    if (units_active) {
      try_change_thread (p);
    }
  } while (units_active);
  genie_abend_thread ();
  return (void *) NULL;
}

//! @brief Execute parallel clause.

PROP_T genie_parallel (NODE_T * p)
{
  int j;
  ADDR_T stack_s = 0, frame_s = 0;
  BYTE_T *system_stack_offset_s = NO_BYTE;
  if (is_main_thread ()) {
// Spawn first thread and await its completion.
    pthread_attr_t new_at;
    size_t ss;
    BYTE_T stack_offset;
    A68_THREAD_CONTEXT *u;
    LOCK_THREAD;
    A68_PAR (abend_all_threads) = A68_FALSE;
    A68_PAR (exit_from_threads) = A68_FALSE;
    A68_PAR (par_return_code) = 0;
    A68_PAR (sp0) = stack_s = A68_SP;
    A68_PAR (fp0) = frame_s = A68_FP;
    system_stack_offset_s = A68 (system_stack_offset);
    A68_PAR (context_index) = 0;
// Set up a thread for this unit.
    u = &(A68_PAR (context)[A68_PAR (context_index)]);
    UNIT (u) = p;
    STACK_USED (u) = SYSTEM_STACK_USED;
    THREAD_STACK_OFFSET (u) = NO_BYTE;
    CUR_PTR (&STACK (u)) = A68_SP;
    CUR_PTR (&FRAME (u)) = A68_FP;
    INI_PTR (&STACK (u)) = A68_PAR (sp0);
    INI_PTR (&FRAME (u)) = A68_PAR (fp0);
    SWAP (&STACK (u)) = NO_BYTE;
    SWAP (&FRAME (u)) = NO_BYTE;
    START (&STACK (u)) = NO_BYTE;
    START (&FRAME (u)) = NO_BYTE;
    BYTES (&STACK (u)) = 0;
    BYTES (&FRAME (u)) = 0;
    ACTIVE (u) = A68_TRUE;
// Spawn the first thread and join it to await its completion.
    errno = 0;
    if (pthread_attr_init (&new_at) != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (pthread_attr_setstacksize (&new_at, (size_t) A68 (stack_size)) != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (pthread_attr_getstacksize (&new_at, &ss) != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    ABEND ((size_t) ss != (size_t) A68 (stack_size), ERROR_ACTION, __func__);
    if (pthread_create (&A68_PAR (parent_thread_id), &new_at, start_genie_parallel, NULL) != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (errno != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PARENT (u) = A68_PAR (main_thread_id);
    ID (u) = A68_PAR (parent_thread_id);
    A68_PAR (context_index)++;
    save_stacks (A68_PAR (parent_thread_id));
    UNLOCK_THREAD;
    if (pthread_join (A68_PAR (parent_thread_id), NULL) != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
// The first spawned thread has completed, now clean up.
    for (j = 0; j < A68_PAR (context_index); j++) {
      if (ACTIVE (&(A68_PAR (context)[j])) && OTHER_THREAD (ID (&(A68_PAR (context)[j])), A68_PAR (main_thread_id)) && OTHER_THREAD (ID (&(A68_PAR (context)[j])), A68_PAR (parent_thread_id))) {
// If threads are zapped it is possible that some are active at this point!.
        if (pthread_join (ID (&(A68_PAR (context)[j])), NULL) != 0) {
          diagnostic (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
      if (SWAP (&STACK (&(A68_PAR (context)[j]))) != NO_BYTE) {
        a68_free (SWAP (&STACK (&(A68_PAR (context)[j]))));
        SWAP (&STACK (&(A68_PAR (context)[j]))) = NO_BYTE;
      }
      if (SWAP (&STACK (&(A68_PAR (context)[j]))) != NO_BYTE) {
        a68_free (SWAP (&STACK (&(A68_PAR (context)[j]))));
        SWAP (&STACK (&(A68_PAR (context)[j]))) = NO_BYTE;
      }
    }
// Now every thread should have ended.
    A68_PAR (context_index) = 0;
    A68_SP = stack_s;
    A68_FP = frame_s;
    get_stack_size ();
    A68 (system_stack_offset) = system_stack_offset_s;
// See if we ended execution in parallel clause.
    if (is_main_thread () && A68_PAR (exit_from_threads)) {
      exit_genie (p, A68_PAR (par_return_code));
    }
    if (is_main_thread () && ERROR_COUNT (&(A68 (job))) > 0) {
      exit_genie (p, A68_RUNTIME_ERROR);
    }
// See if we jumped out of the parallel clause(s).
    if (is_main_thread () && A68_PAR (abend_all_threads)) {
      JUMP_TO (TABLE (TAX (A68_PAR (jump_label)))) = UNIT (TAX (A68_PAR (jump_label)));
      longjmp (*(A68_PAR (jump_buffer)), 1);
    }
  } else {
// Not in the main thread, spawn parallel units and await completion.
    BOOL_T units_active;
    pthread_t t = pthread_self ();
// Spawn parallel units.
    start_parallel_units (SUB (p), t);
    do {
      units_active = A68_FALSE;
      check_parallel_units (&units_active, t);
      if (units_active) {
        try_change_thread (p);
      }
    } while (units_active);
  }
  return GPROP (p);
}

//! @brief OP LEVEL = (INT) SEMA

void genie_level_sema_int (NODE_T * p)
{
  A68_INT k;
  A68_REF s;
  POP_OBJECT (p, &k, A68_INT);
  s = heap_generator (p, M_INT, SIZE (M_INT));
  *DEREF (A68_INT, &s) = k;
  PUSH_REF (p, s);
}

//! @brief OP LEVEL = (SEMA) INT

void genie_level_int_sema (NODE_T * p)
{
  A68_REF s;
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), M_SEMA);
  PUSH_VALUE (p, VALUE (DEREF (A68_INT, &s)), A68_INT);
}

//! @brief OP UP = (SEMA) VOID

void genie_up_sema (NODE_T * p)
{
  A68_REF s;
  if (is_main_thread ()) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), M_SEMA);
  VALUE (DEREF (A68_INT, &s))++;
}

//! @brief OP DOWN = (SEMA) VOID

void genie_down_sema (NODE_T * p)
{
  A68_REF s;
  A68_INT *k;
  BOOL_T cont = A68_TRUE;
  if (is_main_thread ()) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), M_SEMA);
  while (cont) {
    k = DEREF (A68_INT, &s);
    if (VALUE (k) <= 0) {
      save_stacks (pthread_self ());
      while (VALUE (k) <= 0) {
        if (ERROR_COUNT (&(A68 (job))) > 0 || A68_PAR (abend_all_threads)) {
          genie_abend_thread ();
        }
        UNLOCK_THREAD;
// Waiting a bit relaxes overhead.
        int rc = usleep (10);
        ASSERT (rc == 0 || rc == EINTR);
        LOCK_THREAD;
// Garbage may be collected, so recalculate 'k'.
        k = DEREF (A68_INT, &s);
      }
      restore_stacks (pthread_self ());
      cont = A68_TRUE;
    } else {
      VALUE (k)--;
      cont = A68_FALSE;
    }
  }
}

#endif
