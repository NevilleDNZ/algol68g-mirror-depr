/*!
\file parallel.c
\brief implements a parallel clause
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
This code implements a parallel clause for Algol68G. This parallel clause has
been included for educational purposes, and this implementation just emulates
a multi-processor machine. It cannot make use of actual multiple processors.

POSIX threads are used to have separate registers and stack for each concurrent 
unit. Algol68G parallel units behave as POSIX threads - they have private stacks.
Hence an assignation to an object in another thread, does not change that object
in that other thread. Also jumps between threads are forbidden.
*/

#if defined ENABLE_PAR_CLAUSE

#include "algol68g.h"
#include "genie.h"
#include "inline.h"
#include "mp.h"

typedef struct A68_STACK_DESCRIPTOR A68_STACK_DESCRIPTOR;
typedef struct A68_THREAD_CONTEXT A68_THREAD_CONTEXT;

struct A68_STACK_DESCRIPTOR
{
  ADDR_T cur_ptr, ini_ptr;
  BYTE_T *swap, *start;
  int bytes;
};

struct A68_THREAD_CONTEXT
{
  pthread_t parent, id;
  A68_STACK_DESCRIPTOR stack, frame;
  NODE_T *unit;
  int stack_used;
  BYTE_T *thread_stack_offset;
  BOOL_T active;
};

/*
Set an upper limit for number of threads.
Don't copy POSIX_THREAD_THREADS_MAX since it may be ULONG_MAX.
*/

#define THREAD_LIMIT   256

#if ! defined _POSIX_THREAD_THREADS_MAX
#define _POSIX_THREAD_THREADS_MAX	(THREAD_LIMIT)
#endif

#if (_POSIX_THREAD_THREADS_MAX < THREAD_LIMIT)
#define THREAD_MAX     (_POSIX_THREAD_THREADS_MAX)
#else
#define THREAD_MAX     (THREAD_LIMIT)
#endif

pthread_t main_thread_id = 0;
static A68_THREAD_CONTEXT context[THREAD_MAX];
static ADDR_T fp0, sp0;
static BOOL_T abend_all_threads = A68_FALSE, exit_from_threads = A68_FALSE;
static int context_index = 0;
static int par_return_code = 0;
static jmp_buf *jump_buffer;
static NODE_T *jump_label;
static pthread_mutex_t unit_sema = PTHREAD_MUTEX_INITIALIZER;
static pthread_t parent_thread_id = 0;

static void save_stacks (pthread_t);
static void restore_stacks (pthread_t);

#define SAVE_STACK(stk, st, si) {\
  A68_STACK_DESCRIPTOR *s = (stk);\
  BYTE_T *start = (st);\
  int size = (si);\
  if (size > 0) {\
    if (!((s != NULL) && (s->bytes > 0) && (size <= s->bytes))) {\
      if (s->swap != NULL) {\
        free (s->swap);\
      }\
      s->swap = (BYTE_T *) malloc ((size_t) size);\
      ABNORMAL_END (s->swap == NULL, ERROR_OUT_OF_CORE, NULL);\
    }\
    s->start = start;\
    s->bytes = size;\
    COPY (s->swap, start, size);\
  } else {\
    s->start = start;\
    s->bytes = 0;\
    if (s->swap != NULL) {\
      free (s->swap);\
    }\
    s->swap = NULL;\
  }}

#define RESTORE_STACK(stk) {\
  A68_STACK_DESCRIPTOR *s = (stk);\
  if (s != NULL && s->bytes > 0) {\
    COPY (s->start, s->swap, s->bytes);\
  }}

#define GET_THREAD_INDEX(z, ptid) {\
  int _k_;\
  pthread_t _tid_ = (ptid);\
  (z) = -1;\
  for (_k_ = 0; _k_ < context_index && (z) == -1; _k_++) {\
    if (pthread_equal (_tid_, context[_k_].id)) {\
      (z) = _k_;\
    }\
  }\
  ABNORMAL_END ((z) == -1, "thread id not registered", NULL);\
  }

#define ERROR_THREAD_FAULT "thread fault"

#define LOCK_THREAD {\
  ABNORMAL_END (pthread_mutex_lock (&unit_sema) != 0, ERROR_THREAD_FAULT, NULL);\
  }

#define UNLOCK_THREAD {\
  ABNORMAL_END (pthread_mutex_unlock (&unit_sema) != 0, ERROR_THREAD_FAULT, NULL);\
  }

/*!
\brief does system stack grow up or down?
\param lwb BYTE in the stack to calibrate direction
\return 1 if stackpointer increases, -1 if stackpointer decreases, 0 in case of error
**/

static int stack_direction (BYTE_T * lwb)
{
  BYTE_T upb;
  if ((int) &upb > (int) lwb) {
    return (1);
  } else if ((int) &upb < (int) lwb) {
    return (-1);
  } else {
    return (0);
  }
}

/*!
\brief fill in tree what level of parallel clause we are in
\param p position in tree
\param n level counter
**/

void set_par_level (NODE_T * p, int n)
{
  for (; p != NULL; p = NEXT (p)) {
    if (WHETHER (p, PARALLEL_CLAUSE)) {
      PAR_LEVEL (p) = n + 1;
    } else {
      PAR_LEVEL (p) = n;
    }
    set_par_level (SUB (p), PAR_LEVEL (p));
  }
}

/*!
\brief whether we are in the main thread
\return same
**/

BOOL_T whether_main_thread (void)
{
  return (main_thread_id == pthread_self ());
}

/*!
\brief end a thread, beit normally or not
**/

void genie_abend_thread (void)
{
  int k;
  GET_THREAD_INDEX (k, pthread_self ());
  context[k].active = A68_FALSE;
  UNLOCK_THREAD;
  pthread_exit (NULL);
}

/*!
\brief when we end execution in a parallel clause we zap all threads
**/

void genie_set_exit_from_threads (int ret)
{
  abend_all_threads = A68_TRUE;
  exit_from_threads = A68_TRUE;
  par_return_code = ret;
  genie_abend_thread ();
}

/*!
\brief when we jump out of a parallel clause we zap all threads
\param p position in tree
\param jump_stat jump buffer
\param label node where label is at
**/

void genie_abend_all_threads (NODE_T * p, jmp_buf * jump_stat, NODE_T * label)
{
  (void) p;
  abend_all_threads = A68_TRUE;
  exit_from_threads = A68_FALSE;
  jump_buffer = jump_stat;
  jump_label = label;
  if (!whether_main_thread ()) {
    genie_abend_thread ();
  }
}

/*!
\brief save this thread and try to start another
\param p position in tree
**/

static void try_change_thread (NODE_T * p)
{
  if (whether_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
/* Release the unit_sema so another thread can take it up ... */
    save_stacks (pthread_self ());
    UNLOCK_THREAD;
/* ... and take it up again! */
    LOCK_THREAD;
    restore_stacks (pthread_self ());
  }
}

/*!
\brief store the stacks of threads
\param t thread number
**/

static void save_stacks (pthread_t t)
{
  ADDR_T p, q, u, v;
  int k;
  GET_THREAD_INDEX (k, t);
/* Store stack pointers. */
  context[k].frame.cur_ptr = frame_pointer;
  context[k].stack.cur_ptr = stack_pointer;
/* Swap out evaluation stack. */
  p = stack_pointer;
  q = context[k].stack.ini_ptr;
  SAVE_STACK (&(context[k].stack), STACK_ADDRESS (q), p - q);
/* Swap out frame stack. */
  p = frame_pointer;
  q = context[k].frame.ini_ptr;
  u = p + FRAME_SIZE (p);
  v = q + FRAME_SIZE (q);
/* Consider the embedding thread. */
  SAVE_STACK (&(context[k].frame), FRAME_ADDRESS (v), u - v);
}

/*!
\brief restore stacks of thread
\param t thread number
**/

static void restore_stacks (pthread_t t)
{
  if (a68_prog.error_count > 0 || abend_all_threads) {
    genie_abend_thread ();
  } else {
    int k;
    GET_THREAD_INDEX (k, t);
/* Restore stack pointers. */
    get_stack_size ();
    system_stack_offset = context[k].thread_stack_offset;
    frame_pointer = context[k].frame.cur_ptr;
    stack_pointer = context[k].stack.cur_ptr;
/* Restore stacks. */
    RESTORE_STACK (&(context[k].stack));
    RESTORE_STACK (&(context[k].frame));
  }
}

/*!
\brief check whether parallel units have terminated
\param active checks whether there are still active threads
\param parent parent thread number
**/

static void check_parallel_units (BOOL_T * active, pthread_t parent)
{
  int k;
  for (k = 0; k < context_index; k++) {
    if (parent == context[k].parent) {
      (*active) |= context[k].active;
    }
  }
}

/*!
\brief execute one unit from a PAR clause
\param arg dummy argument
**/

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
  context[k].thread_stack_offset = (BYTE_T *) ((int) &stack_offset - stack_direction (&stack_offset) * context[k].stack_used);
  restore_stacks (t);
  p = (NODE_T *) (context[k].unit);
  EXECUTE_UNIT_TRACE (p);
  genie_abend_thread ();
  return ((void *) NULL);
}

/*!
\brief execute parallel units
\param p position in tree
\param parent parent thread number
**/

static void start_parallel_units (NODE_T * p, pthread_t parent)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      pthread_t new_id;
      pthread_attr_t new_at;
      size_t ss;
      BYTE_T stack_offset;
      A68_THREAD_CONTEXT *u;
/* Set up a thread for this unit. */
      if (context_index >= THREAD_MAX) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OVERFLOW);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
/* Fill out a context for this thread. */
      u = &(context[context_index]);
      u->unit = p;
      u->stack_used = SYSTEM_STACK_USED;
      u->thread_stack_offset = NULL;
      u->stack.cur_ptr = stack_pointer;
      u->frame.cur_ptr = frame_pointer;
      u->stack.ini_ptr = sp0;
      u->frame.ini_ptr = fp0;
      u->stack.swap = NULL;
      u->frame.swap = NULL;
      u->stack.start = NULL;
      u->frame.start = NULL;
      u->stack.bytes = 0;
      u->frame.bytes = 0;
      u->active = A68_TRUE;
/* Create the thread. */
      RESET_ERRNO;
      if (pthread_attr_init (&new_at) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (pthread_attr_setstacksize (&new_at, stack_size) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (pthread_attr_getstacksize (&new_at, &ss) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      ABNORMAL_END ((size_t) ss != (size_t) stack_size, "cannot set thread stack size", NULL);
      if (pthread_create (&new_id, &new_at, start_unit, NULL) != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      u->parent = parent;
      u->id = new_id;
      context_index++;
      save_stacks (new_id);
    } else {
      start_parallel_units (SUB (p), parent);
    }
  }
}

/*!
\brief execute one unit from a PAR clause
\param arg dummy argument
**/

static void *start_genie_parallel (void *arg)
{
  pthread_t t;
  int k;
  BYTE_T stack_offset;
  NODE_T *p;
  BOOL_T units_active;
  (void) arg;
  UP_SWEEP_SEMA;
  LOCK_THREAD;
  t = pthread_self ();
  GET_THREAD_INDEX (k, t);
  context[k].thread_stack_offset = (BYTE_T *) ((int) &stack_offset - stack_direction (&stack_offset) * context[k].stack_used);
  restore_stacks (t);
  p = (NODE_T *) (context[k].unit);
/* This is the thread spawned by the main thread, we spawn parallel units and await their completion. */
  start_parallel_units (SUB (p), t);
  do {
    CHECK_TIME_LIMIT (p);
    units_active = A68_FALSE;
    check_parallel_units (&units_active, pthread_self ());
    if (units_active) {
      try_change_thread (p);
    }
  } while (units_active);
  DOWN_SWEEP_SEMA;
  genie_abend_thread ();
  return ((void *) NULL);
}

/*!
\brief execute parallel clause
\param p position in tree
\return propagator for this routine
**/

PROPAGATOR_T genie_parallel (NODE_T * p)
{
  int j;
  ADDR_T stack_s = 0, frame_s = 0;
  BYTE_T *system_stack_offset_s = NULL;
  if (whether_main_thread ()) {
/* Here we are not in the main thread, spawn first thread and await its completion. */
    pthread_attr_t new_at;
    size_t ss;
    BYTE_T stack_offset;
    A68_THREAD_CONTEXT *u;
    LOCK_THREAD;
    abend_all_threads = A68_FALSE;
    exit_from_threads = A68_FALSE;
    par_return_code = 0;
    sp0 = stack_s = stack_pointer;
    fp0 = frame_s = frame_pointer;
    system_stack_offset_s = system_stack_offset;
    context_index = 0;
/* Set up a thread for this unit. */
    u = &(context[context_index]);
    u->unit = p;
    u->stack_used = SYSTEM_STACK_USED;
    u->thread_stack_offset = NULL;
    u->stack.cur_ptr = stack_pointer;
    u->frame.cur_ptr = frame_pointer;
    u->stack.ini_ptr = sp0;
    u->frame.ini_ptr = fp0;
    u->stack.swap = NULL;
    u->frame.swap = NULL;
    u->stack.start = NULL;
    u->frame.start = NULL;
    u->stack.bytes = 0;
    u->frame.bytes = 0;
    u->active = A68_TRUE;
/* Spawn the first thread and join it to await its completion. */
    RESET_ERRNO;
    if (pthread_attr_init (&new_at) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (pthread_attr_setstacksize (&new_at, stack_size) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (pthread_attr_getstacksize (&new_at, &ss) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    ABNORMAL_END ((size_t) ss != (size_t) stack_size, "cannot set thread stack size", NULL);
    if (pthread_create (&parent_thread_id, &new_at, start_genie_parallel, NULL) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (errno != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    u->parent = main_thread_id;
    u->id = parent_thread_id;
    context_index++;
    save_stacks (parent_thread_id);
    UNLOCK_THREAD;
    if (pthread_join (parent_thread_id, NULL) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* The first spawned thread has completed, now clean up. */
    for (j = 0; j < context_index; j++) {
      if (context[j].active && context[j].id != main_thread_id && context[j].id != parent_thread_id) {
/* If threads are being zapped it is possible that some are still active at this point! */
        if (pthread_join (context[j].id, NULL) != 0) {
          diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_THREAD_FAULT);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
      if (context[j].stack.swap != NULL) {
        free (context[j].stack.swap);
        context[j].stack.swap = NULL;
      }
      if (context[j].frame.swap != NULL) {
        free (context[j].frame.swap);
        context[j].frame.swap = NULL;
      }
    }
/* Now every thread should have ended. */
    context_index = 0;
    stack_pointer = stack_s;
    frame_pointer = frame_s;
    get_stack_size ();
    system_stack_offset = system_stack_offset_s;
/* See if we ended execution in parallel clause. */
    if (whether_main_thread () && exit_from_threads) {
      exit_genie (p, par_return_code);
    }
    if (whether_main_thread () && a68_prog.error_count > 0) {
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* See if we jumped out of the parallel clause(s). */
    if (whether_main_thread () && abend_all_threads) {
      SYMBOL_TABLE (TAX (jump_label))->jump_to = TAX (jump_label)->unit;
      longjmp (*(jump_buffer), 1);
    }
  } else {
/* Here we are not in the main thread, we spawn parallel units and await their completion. */
    BOOL_T units_active;
    pthread_t t = pthread_self ();
/* Spawn parallel units. */
    start_parallel_units (SUB (p), t);
    do {
      CHECK_TIME_LIMIT (p);
      units_active = A68_FALSE;
      check_parallel_units (&units_active, t);
      if (units_active) {
        try_change_thread (p);
      }
    } while (units_active);
  }
  return (PROPAGATOR (p));
}

/*!
\brief OP LEVEL = (INT) SEMA
\param p position in tree
**/

void genie_level_sema_int (NODE_T * p)
{
  A68_INT k;
  A68_REF s;
  POP_OBJECT (p, &k, A68_INT);
  s = heap_generator (p, MODE (INT), ALIGNED_SIZE_OF (A68_INT));
  *(A68_INT *) ADDRESS (&s) = k;
  PUSH_REF (p, s);
}

/*!
\brief OP LEVEL = (SEMA) INT
\param p position in tree
**/

void genie_level_int_sema (NODE_T * p)
{
  A68_REF s;
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), MODE (SEMA));
  PUSH_PRIMITIVE (p, VALUE ((A68_INT *) ADDRESS (&s)), A68_INT);
}

/*!
\brief OP UP = (SEMA) VOID
\param p position in tree
**/

void genie_up_sema (NODE_T * p)
{
  A68_REF s;
  if (whether_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), MODE (SEMA));
  VALUE ((A68_INT *) ADDRESS (&s))++;
}

/*!
\brief OP DOWN = (SEMA) VOID
\param p position in tree
**/

void genie_down_sema (NODE_T * p)
{
  A68_REF s;
  A68_INT *k;
  if (whether_main_thread ()) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  BOOL_T cont = A68_TRUE;
  POP_REF (p, &s);
  CHECK_INIT (p, INITIALISED (&s), MODE (SEMA));
  while (cont) {
    k = (A68_INT *) ADDRESS (&s);
    if (VALUE (k) <= 0) {
      save_stacks (pthread_self ());
      while (VALUE (k) <= 0) {
        if (a68_prog.error_count > 0 || abend_all_threads) {
          genie_abend_thread ();
        }
        CHECK_TIME_LIMIT (p);
        UNLOCK_THREAD;
/* Waiting a bit relaxes overhead. */
        usleep (10);
        LOCK_THREAD;
/* Garbage may be collected, so recalculate 'k'. */
        k = (A68_INT *) ADDRESS (&s);
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
