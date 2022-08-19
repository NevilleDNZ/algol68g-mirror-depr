//! @file mem.c
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

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-genie.h"

//! @brief Initialise C and A68 heap management.

void init_heap (void)
{
  int heap_a_size = A68_ALIGN (A68 (heap_size));
  int handle_a_size = A68_ALIGN (A68 (handle_pool_size));
  int frame_a_size = A68_ALIGN (A68 (frame_stack_size));
  int expr_a_size = A68_ALIGN (A68 (expr_stack_size));
  int total_size = A68_ALIGN (heap_a_size + handle_a_size + frame_a_size + 2 * expr_a_size);
  BYTE_T *core;
  A68_HEAP = NO_BYTE;
  A68_HANDLES = NO_BYTE;
  A68_STACK = NO_BYTE;
  A68_SP = 0;
  A68_FP = 0;
  A68_HP = 0;
  A68_GLOBALS = 0;
  errno = 0;
  core = (BYTE_T *) (A68_ALIGN_T *) a68_alloc ((size_t) total_size, __func__, __LINE__);
  ABEND (core == NO_BYTE, ERROR_OUT_OF_CORE, __func__);
  A68_HEAP = &(core[0]);
  A68_HANDLES = &(A68_HEAP[heap_a_size]);
  A68_STACK = &(A68_HANDLES[handle_a_size]);
  A68 (fixed_heap_pointer) = A68_ALIGNMENT;
  A68 (temp_heap_pointer) = total_size;
  A68 (frame_start) = 0;
  A68 (frame_end) = A68 (stack_start) = A68 (frame_start) + frame_a_size;
  A68 (stack_end) = A68 (stack_start) + expr_a_size;
  ABEND (errno != 0, ERROR_ALLOCATION, __func__);
}

//! @brief aligned allocation.

void *a68_alloc (size_t s, const char *f, int line)
{
// We need this since malloc aligns to "standard C types".
// __float128 is not a standard type, apparently ...
  ABEND (s >= GIGABYTE, ERROR_OUT_OF_CORE, __func__);   // Huge chunks cause trouble!
  if (s > 0) {
    const unsigned size = sizeof (A68_ALIGN_T);
    unsigned words = s / size, bytes = s % size;
    void *p;
    LINK_ERRNO;
    errno = 0;
#if defined (BUILD_WIN32)
    (void) bytes;
    p = _aligned_malloc (s, size);
#elif defined (__APPLE__)
    p = malloc(size);
#else
    if (bytes != 0) {
      words++;
    }
    p = aligned_alloc (size, words * size);
#endif
    if (p == (void *) NULL || errno != 0) {
      static char msg[BUFFER_SIZE];
      snprintf (msg, SNPRINTF_SIZE, "cannot allocate %lu=%u*%u bytes; called from function %s, line %d", s, words, size, f, line);
      ABEND (A68_TRUE, ERROR_ALLOCATION, msg);
    }
    UNLINK_ERRNO;
    return p;
  } else {
    return (void *) NULL;
  }
}

void a68_free (void *z)
{
  if (z != NULL) {
#if defined (BUILD_WIN32)
// On WIN32, free cannot deallocate _aligned_malloc
    _aligned_free (z);
#else
    free (z);
#endif
  }
}

//! @brief Give pointer to block of "s" bytes.

BYTE_T *get_heap_space (size_t s)
{
  BYTE_T *z;
  ABEND (s == 0, ERROR_INVALID_SIZE, __func__);
  z = (BYTE_T *) (A68_ALIGN_T *) a68_alloc (A68_ALIGN (s), __func__, __LINE__);
  ABEND (z == NO_BYTE, ERROR_OUT_OF_CORE, __func__);
  return z;
}

//! @brief Make a new copy of concatenated strings.

char *new_string (char *t, ...)
{
  va_list vl;
  char *q, *z;
  int len = 0;
  va_start (vl, t);
  q = t;
  if (q == NO_TEXT) {
    return NO_TEXT;
  }
  while (q != NO_TEXT) {
    len += (int) strlen (q);
    q = va_arg (vl, char *);
  }
  va_end (vl);
  len++;
  z = (char *) get_heap_space ((size_t) len);
  z[0] = NULL_CHAR;
  q = t;
  va_start (vl, t);
  while (q != NO_TEXT) {
    bufcat (z, q, len);
    q = va_arg (vl, char *);
  }
  va_end (vl);
  return z;
}

//! @brief Make a new copy of "t".

char *new_fixed_string (char *t)
{
  int n = (int) (strlen (t) + 1);
  char *z = (char *) get_fixed_heap_space ((size_t) n);
  bufcpy (z, t, n);
  return z;
}

//! @brief Make a new copy of "t".

char *new_temp_string (char *t)
{
  int n = (int) (strlen (t) + 1);
  char *z = (char *) get_temp_heap_space ((size_t) n);
  bufcpy (z, t, n);
  return z;
}

//! @brief Get (preferably fixed) heap space.

BYTE_T *get_fixed_heap_space (size_t s)
{
  BYTE_T *z;
  if (A68 (heap_is_fluid)) {
    z = HEAP_ADDRESS (A68 (fixed_heap_pointer));
    A68 (fixed_heap_pointer) += A68_ALIGN ((int) s);
// Allow for extra storage for diagnostics etcetera 
    ABEND (A68 (fixed_heap_pointer) >= (A68 (heap_size) - MIN_MEM_SIZE), ERROR_OUT_OF_CORE, __func__);
    ABEND (((unsigned) A68 (temp_heap_pointer) - (unsigned) A68 (fixed_heap_pointer)) <= MIN_MEM_SIZE, ERROR_OUT_OF_CORE, __func__);
    return z;
  } else {
    return get_heap_space (s);
  }
}

//! @brief Get (preferably temporary) heap space.

BYTE_T *get_temp_heap_space (size_t s)
{
  BYTE_T *z;
  if (A68 (heap_is_fluid)) {
    A68 (temp_heap_pointer) -= A68_ALIGN ((int) s);
// Allow for extra storage for diagnostics etcetera.
    ABEND (((unsigned) A68 (temp_heap_pointer) - (unsigned) A68 (fixed_heap_pointer)) <= MIN_MEM_SIZE, ERROR_OUT_OF_CORE, __func__);
    z = HEAP_ADDRESS (A68 (temp_heap_pointer));
    return z;
  } else {
    return get_heap_space (s);
  }
}

//! @brief Get size of stack segment.

void get_stack_size (void)
{
#if defined (BUILD_WIN32)
  A68 (stack_size) = MEGABYTE;  // Guestimate
#else
  struct rlimit limits;
  errno = 0;
// Some systems do not implement RLIMIT_STACK so if getrlimit fails, we do not abend.
  if (!(getrlimit (RLIMIT_STACK, &limits) == 0 && errno == 0)) {
    A68 (stack_size) = MEGABYTE;
  }
  A68 (stack_size) = (unsigned) (RLIM_CUR (&limits) < RLIM_MAX (&limits) ? RLIM_CUR (&limits) : RLIM_MAX (&limits));
// A heuristic in case getrlimit yields extreme numbers: the frame stack is
// assumed to fill at a rate comparable to the C stack, so the C stack needs
// not be larger than the frame stack. This may not be true.
  if (A68 (stack_size) < KILOBYTE || (A68 (stack_size) > 96 * MEGABYTE && A68 (stack_size) > A68 (frame_stack_size))) {
    A68 (stack_size) = A68 (frame_stack_size);
  }
#endif
  A68 (stack_limit) = (A68 (stack_size) > (4 * A68 (storage_overhead)) ? (A68 (stack_size) - A68 (storage_overhead)) : A68 (stack_size) / 2);
}

//! @brief Free heap allocated by genie.

void genie_free (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    genie_free (SUB (p));
    if (GINFO (p) != NO_GINFO) {
      a68_free (CONSTANT (GINFO (p)));
      CONSTANT (GINFO (p)) = NO_CONSTANT;
      a68_free (COMPILE_NAME (GINFO (p)));
      COMPILE_NAME (GINFO (p)) = NO_TEXT;
    }
  }
}

//! @brief Free heap allocated by genie.

void free_syntax_tree (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    free_syntax_tree (SUB (p));
    a68_free (NPRAGMENT (p));
    NPRAGMENT (p) = NO_TEXT;
    DIAGNOSTIC_T *d = DIAGNOSTICS (LINE (INFO (p)));
    while (d != NO_DIAGNOSTIC) {
      a68_free (TEXT (d));
      DIAGNOSTIC_T *stale = d;
      FORWARD (d);
      a68_free (stale);
    }
    DIAGNOSTICS (LINE (INFO (p))) = NO_DIAGNOSTIC;
  }
}
