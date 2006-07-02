/*!
\file parallel.c
\brief implements a parallel clause
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
This code implements a parallel clause for Algol68G. This parallel clause has
been included for educational purposes, and this implementation just emulates
a multi-processor machine. It cannot make use of actual multiple processors.
Since the interpreter is recursive, POSIX threads are used to have separate
registers and stack for each concurrent unit.
*/

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

#ifdef HAVE_POSIX_THREADS

typedef struct STACK STACK;

struct STACK {
  ADDR_T cur_ptr, ini_ptr;
  BYTE_T *swap, *start;
  int bytes;
};

int parallel_clauses = 0;
pthread_t main_thread_id = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static ADDR_T fp0, sp0;

/*
Set an upper limit for threads.
Don't copy POSIX_THREAD_THREADS_MAX since it may be ULONG_MAX.
*/

#define THREAD_LIMIT   256

#ifndef _POSIX_THREAD_THREADS_MAX
#define _POSIX_THREAD_THREADS_MAX	(THREAD_LIMIT)
#endif

#if (_POSIX_THREAD_THREADS_MAX < THREAD_LIMIT)
#define THREAD_MAX     (_POSIX_THREAD_THREADS_MAX)
#else
#define THREAD_MAX     (THREAD_LIMIT)
#endif

typedef struct A68_CONTEXT A68_CONTEXT;

struct A68_CONTEXT {
  pthread_t id;
  STACK stack, frame;
  NODE_T *unit;
  int stack_used;
  BYTE_T *thread_stack_offset;
};

static A68_CONTEXT context[THREAD_MAX];
static int condex = 0;

static void save_stacks (pthread_t);
static void restore_stacks (pthread_t);
#else
#define pthread_t int
#endif

BOOL_T in_par_clause = A_FALSE;

static BOOL_T zapped = A_FALSE;
static jmp_buf *jump_buffer;
static NODE_T *jump_label;

/*!
\brief count parallel_clauses
**/

void count_parallel_clauses (NODE_T * p)
{
#ifdef HAVE_POSIX_THREADS
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, PARALLEL_CLAUSE)) {
      parallel_clauses++;
    }
    count_parallel_clauses (SUB (p));
  }
#else
  (void) p;
  ABNORMAL_END (A_TRUE, ERROR_REQUIRE_THREADS, NULL);
#endif
}

/*!
\brief whether we are in the main thread
\return TRUE if in main thread or FALSE otherwise
**/

BOOL_T is_main_thread (void)
{
#ifdef HAVE_POSIX_THREADS
  return (main_thread_id == pthread_self ());
#else
  ABNORMAL_END (A_TRUE, ERROR_REQUIRE_THREADS, NULL);
  return (A_FALSE);
#endif
}

/*!
\brief end a thread, beit normally or not
**/

void genie_abend_thread (void)
{
#ifdef HAVE_POSIX_THREADS
  save_stacks (pthread_self ());
  pthread_mutex_unlock (&mutex);
  pthread_exit (NULL);
#else
  ABNORMAL_END (A_TRUE, ERROR_REQUIRE_THREADS, NULL);
#endif
}


/*!
\brief when we jump out of a parallel clause we zap all threads
\param p position in syntax tree, should not be NULL
\param jump_stat
\param label
**/

void zap_all_threads (NODE_T * p, jmp_buf * jump_stat, NODE_T * label)
{
  (void) p;
  zapped = A_TRUE;
  jump_buffer = jump_stat;
  jump_label = label;
  if (!is_main_thread ()) {
    genie_abend_thread ();
  }
}

/*!
\brief fill in tree what level of parallel clause we are in
\param p position in syntax tree, should not be NULL
\param n level counter
**/

void set_par_level (NODE_T * p, int n)
{
  for (; p != NULL; p = NEXT (p)) {
    if (WHETHER (p, PARALLEL_CLAUSE)) {
      PAR_LEVEL (p) = n + 1;
      set_par_level (SUB (p), PAR_LEVEL (p));
    } else {
      PAR_LEVEL (p) = n;
      set_par_level (SUB (p), PAR_LEVEL (p));
    }
  }
}

#ifdef HAVE_POSIX_THREADS

/*!
\brief save this thread and try to start another
\param p position in syntax tree, should not be NULL
**/

static void try_change_thread (NODE_T * p)
{
  if (!in_par_clause) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_PARALLEL_OUTSIDE);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  save_stacks (pthread_self ());
  pthread_mutex_unlock (&mutex);
  pthread_mutex_lock (&mutex);
  restore_stacks (pthread_self ());
}
#endif

#ifdef HAVE_POSIX_THREADS

/*!
\brief thread id to context index
\param id thread id
\return context index or -1 if thread id is not found
**/

static int get_index (pthread_t id)
{
  int k;
  for (k = 0; k < condex; k++) {
    if (pthread_equal (id, context[k].id)) {
      return (k);
    }
  }
  ABNORMAL_END (A_TRUE, "thread id not registered", NULL);
  return (-1);
}
#endif

#ifdef HAVE_POSIX_THREADS

/*!
\brief save a stack, only allocate if a block is too small
\param s stack where to store, should not be NULL
\param start first BYTE to copy, should not be NULL
\param size number of bytes to copy
**/

static void save (STACK * s, BYTE_T * start, int size)
{
  if (size > 0) {
    BYTE_T *z = NULL;
    if ((s != NULL) && (s->bytes > 0) && (size <= s->bytes)) {
      z = s->swap;
    } else {
      if (s->swap != NULL) {
        free (s->swap);
      }
      z = (BYTE_T *) malloc ((size_t) size);
      ABNORMAL_END (z == NULL, ERROR_OUT_OF_CORE, NULL);
    }
    s->start = start;
    s->bytes = size;
    COPY (z, start, size);
    s->swap = z;
  } else {
    s->bytes = 0;
    if (s->swap != NULL) {
      free (s->swap);
    }
    s->swap = NULL;
  }
}
#endif

#ifdef HAVE_POSIX_THREADS

/*!
\brief restore a stack
\param s stack from which to restore, should not be NULL
**/

static void restore (STACK * s)
{
  if (s != NULL && s->bytes > 0) {
    COPY (s->start, s->swap, s->bytes);
  }
}
#endif

#ifdef HAVE_POSIX_THREADS

/*!
\brief store the stacks of threads
\param t
\return
**/

static void save_stacks (pthread_t t)
{
  ADDR_T p, q, u, v;
  int k = get_index (t);
/* Store stack pointers.						*/
  context[k].frame.cur_ptr = frame_pointer;
  context[k].stack.cur_ptr = stack_pointer;
/* Swap out evaluation stack.   					*/
  p = context[k].stack.cur_ptr;
  q = context[k].stack.ini_ptr;
  save (&(context[k].stack), STACK_ADDRESS (q), p - q);
/* Swap out frame stack.						*/
  p = context[k].frame.cur_ptr;
  q = context[k].frame.ini_ptr;
  u = p + FRAME_SIZE (p);
  v = q + FRAME_SIZE (q);
  save (&(context[k].frame), FRAME_ADDRESS (v), u - v);
/* Consider the embedding thread.       				*/
}
#endif

#ifdef HAVE_POSIX_THREADS

/*!
\brief restore stacks of thread
\param t thread
**/

static void restore_stacks (pthread_t t)
{
  if (error_count > 0 || zapped) {
    genie_abend_thread ();
  } else {
    int k = get_index (t);
/* Restore stack pointers.      					*/
    get_stack_size ();
    system_stack_offset = context[k].thread_stack_offset;
    frame_pointer = context[k].frame.cur_ptr;
    stack_pointer = context[k].stack.cur_ptr;
/* Restore stacks.      						*/
    restore (&(context[k].stack));
    restore (&(context[k].frame));
  }
}
#endif

#ifdef HAVE_POSIX_THREADS

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
#endif

#ifdef HAVE_POSIX_THREADS

/*!
\brief execute one unit from a PAR clause
\param arg dummy argument
**/

static void *start_unit (void *arg)
{
  pthread_t t;
  int k;
  BYTE_T stack_offset;
  (void) arg;
  pthread_mutex_lock (&mutex);
  t = pthread_self ();
  k = get_index (t);
  context[k].thread_stack_offset = (BYTE_T *) ((int) &stack_offset - stack_direction (&stack_offset) * context[k].stack_used);
  restore_stacks (t);
  EXECUTE_UNIT_TRACE ((NODE_T *) context[get_index (t)].unit);
  genie_abend_thread ();
  return ((void *) NULL);
}
#endif

#ifdef HAVE_POSIX_THREADS

/*!
\brief execute parallel units
\param p position in syntax tree, should not be NULL
**/

static void genie_parallel_units (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      pthread_t new_id;
      pthread_attr_t new_at;
      ssize_t ss;
      BYTE_T stack_offset;
/* Set up a thread for this unit.       				*/
      if (condex >= THREAD_MAX) {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_PARALLEL_OVERFLOW);
        exit_genie (p, A_RUNTIME_ERROR);
      }
/* Fill out a context for this thread.  				*/
      context[condex].unit = p;
      context[condex].stack_used = SYSTEM_STACK_USED;
      context[condex].thread_stack_offset = NULL;
      context[condex].stack.cur_ptr = stack_pointer;
      context[condex].frame.cur_ptr = frame_pointer;
      context[condex].stack.ini_ptr = sp0;
      context[condex].frame.ini_ptr = fp0;
      context[condex].stack.swap = NULL;
      context[condex].frame.swap = NULL;
      context[condex].stack.start = NULL;
      context[condex].frame.start = NULL;
      context[condex].stack.bytes = 0;
      context[condex].frame.bytes = 0;
/* Create the actual thread.    					*/
      RESET_ERRNO;
      pthread_attr_init (&new_at);
      pthread_attr_setstacksize (&new_at, stack_size);
      pthread_attr_getstacksize (&new_at, &ss);
      ABNORMAL_END (ss != stack_size, "cannot set thread stack size", NULL);
      pthread_create (&new_id, &new_at, start_unit, NULL);
      if (errno != 0) {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_CREATE);
        exit_genie (p, A_RUNTIME_ERROR);
      }
      context[condex].id = new_id;
      condex++;
      save_stacks (new_id);
    } else {
      genie_parallel_units (SUB (p));
    }
  }
}
#endif

/*!
\brief execute parallel clause
\param p position in syntax tree, should not be NULL
\return propagator for this routine
**/

PROPAGATOR_T genie_parallel (NODE_T * p)
{
#ifdef HAVE_POSIX_THREADS
  PROPAGATOR_T self;
  int j;
  ADDR_T stack_s = 0, frame_s = 0;
  BYTE_T *system_stack_offset_s = NULL;
  BOOL_T save_in_par_clause = in_par_clause;
  UP_SWEEP_SEMA;
  self.unit = genie_parallel;
  self.source = p;
  in_par_clause = A_TRUE;
  if (!save_in_par_clause) {
    zapped = A_FALSE;
    sp0 = stack_s = stack_pointer;
    fp0 = frame_s = frame_pointer;
    system_stack_offset_s = system_stack_offset;
  }
/* Claim the engine if in the outermost PAR level.      		*/
  if (!save_in_par_clause) {
    pthread_mutex_lock (&mutex);
    condex = 0;
  }
/* Spawn the units (they remain inactive when we blocked the engine).*/
  genie_parallel_units (SUB (p));
/* Free the engine if in the outermost PAR level.       		*/
  if (!save_in_par_clause) {
    pthread_mutex_unlock (&mutex);
  }
/* Join the other threads if in the outermost PAR level.		*/
  if (!save_in_par_clause) {
    for (j = 0; j < condex; j++) {
      RESET_ERRNO;
      pthread_join (context[j].id, NULL);
      if (errno != 0) {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_PARALLEL_CANNOT_JOIN);
        exit_genie (p, A_RUNTIME_ERROR);
      }
    }
/* Restore state for the outermost PAR level that is now exiting. */
    stack_pointer = stack_s;
    frame_pointer = frame_s;
    get_stack_size ();
    system_stack_offset = system_stack_offset_s;
    for (j = 0; j < condex; j++) {
      if (context[j].stack.swap != NULL) {
        free (context[j].stack.swap);
      }
      if (context[j].frame.swap != NULL) {
        free (context[j].frame.swap);
      }
    }
    condex = 0;
    if (error_count > 0) {
      exit_genie (p, A_RUNTIME_ERROR);
    }
  }
  DOWN_SWEEP_SEMA;
  in_par_clause = save_in_par_clause;
  if (is_main_thread () && zapped) {
/* Beam us up, Scotty!  						*/
    SYMBOL_TABLE (TAX (jump_label))->jump_to = TAX (jump_label)->unit;
    longjmp (*(jump_buffer), 1);
  }
  return (self);
#else
  PROPAGATOR_T self;
  self.unit = genie_parallel;
  self.source = p;
  diagnostic_node (A_RUNTIME_ERROR, p, ERROR_REQUIRE_THREADS);
  exit_genie (p, A_RUNTIME_ERROR);
  return (self);
#endif
}

#define CHECK_INIT(p, c, q)\
  if (!(c)) {\
    diagnostic_node (A_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE_FROM, (q));\
    exit_genie ((p), A_RUNTIME_ERROR);\
  }

/*!
\brief OP LEVEL = (INT) SEMA
\param p position in syntax tree, should not be NULL
**/

void genie_level_sema_int (NODE_T * p)
{
#ifdef HAVE_POSIX_THREADS
  A68_INT k;
  A68_REF s;
  POP_INT (p, &k);
  s = heap_generator (p, MODE (INT), SIZE_OF (A68_INT));
  *(A68_INT *) ADDRESS (&s) = k;
  PUSH_REF (p, s);
#else
  diagnostic_node (A_RUNTIME_ERROR, p, ERROR_REQUIRE_THREADS);
  exit_genie (p, A_RUNTIME_ERROR);
#endif
}


/*!
\brief OP LEVEL = (SEMA) INT
\param p position in syntax tree, should not be NULL
**/

void genie_level_int_sema (NODE_T * p)
{
#ifdef HAVE_POSIX_THREADS
  A68_REF s;
  POP_REF (p, &s);
  CHECK_INIT (p, s.status & INITIALISED_MASK, MODE (SEMA));
  PUSH_INT (p, ((A68_INT *) ADDRESS (&s))->value);
#else
  diagnostic_node (A_RUNTIME_ERROR, p, ERROR_REQUIRE_THREADS);
  exit_genie (p, A_RUNTIME_ERROR);
#endif
}

/*!
\brief OP UP = (SEMA) VOID
\param p position in syntax tree, should not be NULL
**/

void genie_up_sema (NODE_T * p)
{
#ifdef HAVE_POSIX_THREADS
  A68_REF s;
  POP_REF (p, &s);
  CHECK_INIT (p, s.status & INITIALISED_MASK, MODE (SEMA));
  ((A68_INT *) ADDRESS (&s))->value++;
#else
  diagnostic_node (A_RUNTIME_ERROR, p, ERROR_REQUIRE_THREADS);
  exit_genie (p, A_RUNTIME_ERROR);
#endif
}

/*!
\brief OP DOWN = (SEMA) VOID
\param p position in syntax tree, should not be NULL
**/

void genie_down_sema (NODE_T * p)
{
#ifdef HAVE_POSIX_THREADS
  A68_REF s;
  A68_INT *k;
  BOOL_T cont = A_TRUE;
  POP_REF (p, &s);
  CHECK_INIT (p, s.status & INITIALISED_MASK, MODE (SEMA));
  while (cont) {
    k = (A68_INT *) ADDRESS (&s);
    if (k->value <= 0) {
      CHECK_TIME_LIMIT (p);
      try_change_thread (p);
      cont = A_TRUE;
    } else {
      k->value--;
      cont = A_FALSE;
    }
  }
#else
  diagnostic_node (A_RUNTIME_ERROR, p, ERROR_REQUIRE_THREADS);
  exit_genie (p, A_RUNTIME_ERROR);
#endif
}
