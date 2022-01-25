//! @file a68g.h
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

#if ! defined (__A68G_H__)
#define __A68G_H__

// Debugging switch, only useful during development.
#undef A68_DEBUG

// Configuration

#include "a68g-platform.h"
#include "a68g-includes.h"

// Build switches depending on platform.

#if (defined (BUILD_LINUX) && defined (HAVE_GCC) && defined (HAVE_DL))
#  define BUILD_A68_COMPILER
#else
// Untested, so disabled.
#  undef BUILD_A68_COMPILER
#endif

#if defined (BUILD_LINUX)
#  define BUILD_UNIX
#elif defined (BUILD_BSD)
#  define BUILD_UNIX
#elif defined (BUILD_HAIKU)
#  define BUILD_UNIX
#endif

// REAL_T should be a REAL*8 for external libs.
typedef double REAL_T; 

#if (A68_LEVEL >= 3)
#  include <quadmath.h>
#endif

// Can we access the internet?
#if defined (BUILD_WIN32)
#  undef BUILD_HTTP
#else
#  if (defined (HAVE_NETDB_H) && defined (HAVE_NETINET_IN_H) && defined (HAVE_SYS_SOCKET_H))
#    if (defined (BUILD_LINUX) || defined (BUILD_BSD) || defined (BUILD_HAIKU))
#      define BUILD_HTTP
#    endif
#  endif
#endif

// Compatibility.

#if ! defined (O_BINARY)
#  define O_BINARY 0x0000
#endif

// Forward type definitions.

typedef struct NODE_T NODE_T;
typedef unsigned STATUS_MASK_T, BOOL_T;

// Decide the internal representation of A68 modes.

#include "a68g-stddef.h"

#define ALIGNED __attribute__((aligned (sizeof (A68_ALIGN_T))))

#if (A68_LEVEL >= 3)
#  include "a68g-level-3.h"
#else // Vintage Algol 68 Genie (versions 1 and 2).
#  include "a68g-generic.h"
#endif

#define MP_REAL_RADIX ((MP_REAL_T) MP_RADIX)

#if defined (BUILD_WIN32)
typedef unsigned __off_t;
#  if defined (__MSVCRT__) && defined (_environ)
#    undef _environ
#  endif
#endif

#define A68_ALIGN(s) ((int) ((s) % A68_ALIGNMENT) == 0 ? (s) : ((s) - (s) % A68_ALIGNMENT + A68_ALIGNMENT))
#define A68_ALIGNMENT ((int) (sizeof (A68_ALIGN_T)))

#include "a68g-defines.h"
#include "a68g-stack.h"
#include "a68g-masks.h"
#include "a68g-enums.h"
#include "a68g-types.h"
#include "a68g-nil.h"
#include "a68g-diagnostics.h"
#include "a68g-common.h"
#include "a68g-lib.h"

// Global declarations

extern BOOL_T a68_mkstemp (char *, int, mode_t);
extern BYTE_T *get_fixed_heap_space (size_t);
extern BYTE_T *get_heap_space (size_t);
extern BYTE_T *get_temp_heap_space (size_t);
extern char *ctrl_char (int);
extern char *moid_to_string (MOID_T *, int, NODE_T *);
extern char *new_fixed_string (char *);
extern char *new_string (char *, ...);
extern char *new_temp_string (char *);
extern char *non_terminal_string (char *, int);
extern char *read_string_from_tty (char *);
extern char *standard_environ_proc_name (GPROC);
extern int get_row_size (A68_TUPLE *, int);
extern int moid_digits (MOID_T *);
extern int moid_size (MOID_T *);
extern int snprintf (char *, size_t, const char *, ...);
extern void *a68_alloc (size_t, const char *, int);
extern void a68_exit (int);
extern void a68_free (void *);
extern void a68_getty (int *, int *);
extern void * a68_memmove (void *, void *, size_t);
extern void abend (char *, char *, char *, int);
extern void announce_phase (char *);
extern void apropos (FILE_T, char *, char *);
extern void bufcat (char *, char *, int);
extern void bufcpy (char *, char *, int);
extern void default_mem_sizes (int);
extern void discard_heap (void);
extern void free_file_entries (void);
extern void free_syntax_tree (NODE_T *);
extern void get_stack_size (void);
extern void indenter (MODULE_T *);
extern void init_curses (void);
extern void init_file_entries (void);
extern void init_file_entry (int);
extern void init_heap (void);
extern void init_rng (unsigned);
extern void init_tty (void);
extern void install_signal_handlers (void);
extern void online_help (FILE_T);
extern void state_version (FILE_T);

// Below from R mathlib

extern void GetRNGstate(void);
extern void PutRNGstate(void);
extern REAL_T a68_unif_rand(void);
extern REAL_T R_unif_index(REAL_T);

#endif
