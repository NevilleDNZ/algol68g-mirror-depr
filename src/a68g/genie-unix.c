//! @file unix.c
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
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-physics.h"
#include "a68g-numbers.h"
#include "a68g-optimiser.h"
#include "a68g-double.h"
#include "a68g-transput.h"

#define VECTOR_SIZE 512
#define FD_READ 0
#define FD_WRITE 1

#if defined (HAVE_DIRENT_H)

//! @brief PROC (STRING) [] STRING directory

void genie_directory (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  errno = 0;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, name)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
    PUSH_VALUE (p, A68_MAX_INT, A68_INT);
  } else {
    char *dir_name = a_to_c_string (p, buffer, name);
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int k, n = 0;
    A68_REF *base;
    DIR *dir;
    struct dirent *entry;
    dir = opendir (dir_name);
    if (dir == NULL) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    do {
      entry = readdir (dir);
      if (errno != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (entry != NULL) {
        n++;
      }
    } while (entry != NULL);
    rewinddir (dir);
    if (errno != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    NEW_ROW_1D (z, row, arr, tup, M_ROW_STRING, M_STRING, n);
    base = DEREF (A68_REF, &row);
    for (k = 0; k < n; k++) {
      entry = readdir (dir);
      if (errno != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      base[k] = c_to_a_string (p, D_NAME (entry), DEFAULT_WIDTH);
    }
    if (closedir (dir) != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PUSH_REF (p, z);
    a68_free (buffer);
  }
}

#endif

//! @brief PROC [] INT utc time

void genie_utctime (NODE_T * p)
{
  time_t dt;
  if (time (&dt) == (time_t) - 1) {
    (void) empty_row (p, M_ROW_INT);
  } else {
    A68_REF row;
    ADDR_T sp = A68_SP;
    struct tm *tod = gmtime (&dt);
    PUSH_VALUE (p, TM_YEAR (tod) + 1900, A68_INT);
    PUSH_VALUE (p, TM_MON (tod) + 1, A68_INT);
    PUSH_VALUE (p, TM_MDAY (tod), A68_INT);
    PUSH_VALUE (p, TM_HOUR (tod), A68_INT);
    PUSH_VALUE (p, TM_MIN (tod), A68_INT);
    PUSH_VALUE (p, TM_SEC (tod), A68_INT);
    PUSH_VALUE (p, TM_WDAY (tod) + 1, A68_INT);
    PUSH_VALUE (p, TM_ISDST (tod), A68_INT);
    row = genie_make_row (p, M_INT, 8, sp);
    A68_SP = sp;
    PUSH_REF (p, row);
  }
}

//! @brief PROC [] INT local time

void genie_localtime (NODE_T * p)
{
  time_t dt;
  if (time (&dt) == (time_t) - 1) {
    (void) empty_row (p, M_ROW_INT);
  } else {
    A68_REF row;
    ADDR_T sp = A68_SP;
    struct tm *tod = localtime (&dt);
    PUSH_VALUE (p, TM_YEAR (tod) + 1900, A68_INT);
    PUSH_VALUE (p, TM_MON (tod) + 1, A68_INT);
    PUSH_VALUE (p, TM_MDAY (tod), A68_INT);
    PUSH_VALUE (p, TM_HOUR (tod), A68_INT);
    PUSH_VALUE (p, TM_MIN (tod), A68_INT);
    PUSH_VALUE (p, TM_SEC (tod), A68_INT);
    PUSH_VALUE (p, TM_WDAY (tod) + 1, A68_INT);
    PUSH_VALUE (p, TM_ISDST (tod), A68_INT);
    row = genie_make_row (p, M_INT, 8, sp);
    A68_SP = sp;
    PUSH_REF (p, row);
  }
}

//! @brief PROC INT rows

void genie_rows (NODE_T * p)
{
  errno = 0;
  PUSH_VALUE (p, A68 (term_heigth), A68_INT);
}

//! @brief PROC INT columns

void genie_columns (NODE_T * p)
{
  errno = 0;
  PUSH_VALUE (p, A68 (term_width), A68_INT);
}

//! @brief PROC INT argc

void genie_argc (NODE_T * p)
{
  errno = 0;
  PUSH_VALUE (p, A68 (argc), A68_INT);
}

//! @brief PROC (INT) STRING argv

void genie_argv (NODE_T * p)
{
  A68_INT a68_index;
  errno = 0;
  POP_OBJECT (p, &a68_index, A68_INT);
  if (VALUE (&a68_index) >= 1 && VALUE (&a68_index) <= A68 (argc)) {
    char *q = A68 (argv)[VALUE (&a68_index) - 1];
    int n = (int) strlen (q);
// Allow for spaces ending in # to have A68 comment syntax with '#!'.
    while (n > 0 && (IS_SPACE (q[n - 1]) || q[n - 1] == '#')) {
      q[--n] = NULL_CHAR;
    }
    PUSH_REF (p, c_to_a_string (p, q, DEFAULT_WIDTH));
  } else {
    PUSH_REF (p, empty_string (p));
  }
}

//! @brief Find good argument

int find_good_arg (void)
{
  int i;
  for (i = 0; i < A68 (argc); i++) {
    char *q = A68 (argv)[i];
    if (strncmp (q, "--script", 8) == 0) {
      return i + 1;
    }
    if (strncmp (q, "--run-script", 12) == 0) {
      return i + 1;
    }
    if (strcmp (q, "--") == 0) {
      return i;
    }
    if (strcmp (q, "--exit") == 0) {
      return i;
    }
  }
  return 0;
}

//! @brief PROC INT a68g argc

void genie_a68_argc (NODE_T * p)
{
  errno = 0;
  PUSH_VALUE (p, A68 (argc) - find_good_arg (), A68_INT);
}

//! @brief PROC (INT) STRING a68_argv

void genie_a68_argv (NODE_T * p)
{
  A68_INT a68_index;
  int k;
  errno = 0;
  POP_OBJECT (p, &a68_index, A68_INT);
  k = VALUE (&a68_index);
  if (k > 1) {
    k += find_good_arg ();
  }
  if (k >= 1 && k <= A68 (argc)) {
    char *q = A68 (argv)[k - 1];
    int n = (int) strlen (q);
// Allow for spaces ending in # to have A68 comment syntax with '#!'.
    while (n > 0 && (IS_SPACE (q[n - 1]) || q[n - 1] == '#')) {
      q[--n] = NULL_CHAR;
    }
    PUSH_REF (p, c_to_a_string (p, q, DEFAULT_WIDTH));
  } else {
    PUSH_REF (p, empty_string (p));
  }
}

//! @brief PROC STRING pwd

void genie_pwd (NODE_T * p)
{
  size_t size = BUFFER_SIZE;
  char *buffer = NO_TEXT;
  BOOL_T cont = A68_TRUE;
  errno = 0;
  while (cont) {
    buffer = (char *) a68_alloc (size, __func__, __LINE__);
    if (buffer == NO_TEXT) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (getcwd (buffer, size) == buffer) {
      cont = A68_FALSE;
    } else {
      a68_free (buffer);
      cont = (BOOL_T) (errno == 0);
      size *= 2;
    }
  }
  if (buffer != NO_TEXT && errno == 0) {
    PUSH_REF (p, c_to_a_string (p, buffer, DEFAULT_WIDTH));
    a68_free (buffer);
  } else {
    PUSH_REF (p, empty_string (p));
  }
}

//! @brief PROC (STRING) INT cd

void genie_cd (NODE_T * p)
{
  A68_REF dir;
  char *buffer;
  errno = 0;
  POP_REF (p, &dir);
  CHECK_INIT (p, INITIALISED (&dir), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, dir)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    int rc = chdir (a_to_c_string (p, buffer, dir));
    if (rc == 0) {
      PUSH_VALUE (p, 0, A68_INT);
    } else {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    a68_free (buffer);
  }
}

//! @brief PROC (STRING) BITS

void genie_file_mode (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  errno = 0;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, name)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_VALUE (p, (unt) (ST_MODE (&status)), A68_BITS);
    } else {
      PUSH_VALUE (p, 0x0, A68_BITS);
    }
    a68_free (buffer);
  }
}

//! @brief PROC (STRING) BOOL file is block device

void genie_file_is_block_device (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  errno = 0;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, name)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_VALUE (p, (BOOL_T) (S_ISBLK (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_VALUE (p, A68_FALSE, A68_BOOL);
    }
    a68_free (buffer);
  }
}

//! @brief PROC (STRING) BOOL file is char device

void genie_file_is_char_device (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  errno = 0;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, name)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_VALUE (p, (BOOL_T) (S_ISCHR (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_VALUE (p, A68_FALSE, A68_BOOL);
    }
    a68_free (buffer);
  }
}

//! @brief PROC (STRING) BOOL file is directory

void genie_file_is_directory (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  errno = 0;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, name)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_VALUE (p, (BOOL_T) (S_ISDIR (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_VALUE (p, A68_FALSE, A68_BOOL);
    }
    a68_free (buffer);
  }
}

//! @brief PROC (STRING) BOOL file is regular

void genie_file_is_regular (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  errno = 0;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, name)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_VALUE (p, (BOOL_T) (S_ISREG (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_VALUE (p, A68_FALSE, A68_BOOL);
    }
    a68_free (buffer);
  }
}

#if defined (S_ISFIFO)

//! @brief PROC (STRING) BOOL file is fifo

void genie_file_is_fifo (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  errno = 0;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, name)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_VALUE (p, (BOOL_T) (S_ISFIFO (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_VALUE (p, A68_FALSE, A68_BOOL);
    }
    a68_free (buffer);
  }
}

#endif

#if defined (S_ISLNK)

//! @brief PROC (STRING) BOOL file is link

void genie_file_is_link (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  errno = 0;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), M_STRING);
  buffer = (char *) a68_alloc ((size_t) (1 + a68_string_size (p, name)), __func__, __LINE__);
  if (buffer == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_VALUE (p, (BOOL_T) (S_ISLNK (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_VALUE (p, A68_FALSE, A68_BOOL);
    }
    a68_free (buffer);
  }
}

#endif

//! @brief Convert [] STRING row to char *vec[].

void convert_string_vector (NODE_T * p, char *vec[], A68_REF row)
{
  BYTE_T *z = ADDRESS (&row);
  A68_ARRAY *arr = (A68_ARRAY *) & z[0];
  A68_TUPLE *tup = (A68_TUPLE *) & z[SIZE_ALIGNED (A68_ARRAY)];
  int k = 0;
  if (get_row_size (tup, DIM (arr)) > 0) {
    BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
    BOOL_T done = A68_FALSE;
    initialise_internal_index (tup, DIM (arr));
    while (!done) {
      ADDR_T a68_index = calculate_internal_index (tup, DIM (arr));
      ADDR_T elem_addr = (a68_index + SLICE_OFFSET (arr)) * ELEM_SIZE (arr) + FIELD_OFFSET (arr);
      BYTE_T *elem = &base_addr[elem_addr];
      int size = a68_string_size (p, *(A68_REF *) elem);
      CHECK_INIT (p, INITIALISED ((A68_REF *) elem), M_STRING);
      vec[k] = (char *) get_heap_space ((size_t) (1 + size));
      ASSERT (a_to_c_string (p, vec[k], *(A68_REF *) elem) != NO_TEXT);
      if (k == VECTOR_SIZE - 1) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_TOO_MANY_ARGUMENTS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (strlen (vec[k]) > 0) {
        k++;
      }
      done = increment_internal_index (tup, DIM (arr));
    }
  }
  vec[k] = NO_TEXT;
}

//! @brief Free char *vec[].

void free_vector (char *vec[])
{
  int k = 0;
  while (vec[k] != NO_TEXT) {
    a68_free (vec[k]);
    k++;
  }
}

//! @brief Reset error number.

void genie_reset_errno (NODE_T * p)
{
  (void) *p;
  errno = 0;
}

//! @brief Error number.

void genie_errno (NODE_T * p)
{
  PUSH_VALUE (p, errno, A68_INT);
}

//! @brief PROC strerror = (INT) STRING

void genie_strerror (NODE_T * p)
{
  A68_INT i;
  POP_OBJECT (p, &i, A68_INT);
  PUSH_REF (p, c_to_a_string (p, strerror (VALUE (&i)), DEFAULT_WIDTH));
}

//! @brief Set up file for usage in pipe.

void set_up_file (NODE_T * p, A68_REF * z, int fd, A68_CHANNEL chan, BOOL_T r_mood, BOOL_T w_mood, int pid)
{
  A68_FILE *f;
  *z = heap_generator (p, M_REF_FILE, SIZE (M_FILE));
  f = FILE_DEREF (z);
  STATUS (f) = (STATUS_MASK_T) ((pid < 0) ? 0 : INIT_MASK);
  IDENTIFICATION (f) = nil_ref;
  TERMINATOR (f) = nil_ref;
  CHANNEL (f) = chan;
  FD (f) = fd;
  STREAM (&DEVICE (f)) = NO_STREAM;
  OPENED (f) = A68_TRUE;
  OPEN_EXCLUSIVE (f) = A68_FALSE;
  READ_MOOD (f) = r_mood;
  WRITE_MOOD (f) = w_mood;
  CHAR_MOOD (f) = A68_TRUE;
  DRAW_MOOD (f) = A68_FALSE;
  FORMAT (f) = nil_format;
  TRANSPUT_BUFFER (f) = get_unblocked_transput_buffer (p);
  STRING (f) = nil_ref;
  reset_transput_buffer (TRANSPUT_BUFFER (f));
  set_default_event_procedures (f);
}

//! @brief Create and push a pipe.

void genie_mkpipe (NODE_T * p, int fd_r, int fd_w, int pid)
{
  A68_REF r, w;
  errno = 0;
// Set up pipe.
  set_up_file (p, &r, fd_r, A68 (stand_in_channel), A68_TRUE, A68_FALSE, pid);
  set_up_file (p, &w, fd_w, A68 (stand_out_channel), A68_FALSE, A68_TRUE, pid);
// push pipe.
  PUSH_REF (p, r);
  PUSH_REF (p, w);
  PUSH_VALUE (p, pid, A68_INT);
}

//! @brief Push an environment string.

void genie_getenv (NODE_T * p)
{
  A68_REF a_env;
  char *val, *z, *z_env;
  errno = 0;
  POP_REF (p, &a_env);
  CHECK_INIT (p, INITIALISED (&a_env), M_STRING);
  z_env = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_env)));
  z = a_to_c_string (p, z_env, a_env);
  val = getenv (z);
  if (val == NO_TEXT) {
    a_env = empty_string (p);
  } else {
    a_env = tmp_to_a68_string (p, val);
  }
  PUSH_REF (p, a_env);
}

//! @brief PROC fork = INT

void genie_fork (NODE_T * p)
{
#if defined (BUILD_WIN32)
  PUSH_VALUE (p, -1, A68_INT);
#else
  int pid;
  errno = 0;
  pid = (int) fork ();
  PUSH_VALUE (p, pid, A68_INT);
#endif
}

//! @brief PROC execve = (STRING, [] STRING, [] STRING) INT 

void genie_exec (NODE_T * p)
{
  int ret;
  A68_REF a_prog, a_args, a_env;
  char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
  errno = 0;
// Pop parameters.
  POP_REF (p, &a_env);
  POP_REF (p, &a_args);
  POP_REF (p, &a_prog);
// Convert strings and hasta el infinito.
  prog = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_prog)));
  ASSERT (a_to_c_string (p, prog, a_prog) != NO_TEXT);
  convert_string_vector (p, argv, a_args);
  convert_string_vector (p, envp, a_env);
  if (argv[0] == NO_TEXT) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  ret = execve (prog, argv, envp);
// execve only returns if it fails.
  free_vector (argv);
  free_vector (envp);
  a68_free (prog);
  PUSH_VALUE (p, ret, A68_INT);
}

//! @brief PROC execve child = (STRING, [] STRING, [] STRING) INT

void genie_exec_sub (NODE_T * p)
{
  int pid;
  A68_REF a_prog, a_args, a_env;
  errno = 0;
// Pop parameters.
  POP_REF (p, &a_env);
  POP_REF (p, &a_args);
  POP_REF (p, &a_prog);
// Now create the pipes and fork.
#if defined (BUILD_WIN32)
  pid = -1;
  (void) pid;
  PUSH_VALUE (p, -1, A68_INT);
  return;
#else
  pid = (int) fork ();
  if (pid == -1) {
    PUSH_VALUE (p, -1, A68_INT);
  } else if (pid == 0) {
// Child process.
    char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
// Convert  strings.
    prog = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_prog)));
    ASSERT (a_to_c_string (p, prog, a_prog) != NO_TEXT);
    convert_string_vector (p, argv, a_args);
    convert_string_vector (p, envp, a_env);
    if (argv[0] == NO_TEXT) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    (void) execve (prog, argv, envp);
// execve only returns if it fails - end child process.
    a68_exit (EXIT_FAILURE);
    PUSH_VALUE (p, 0, A68_INT);
  } else {
// parent process.
    PUSH_VALUE (p, pid, A68_INT);
  }
#endif
}

//! @brief PROC execve child pipe = (STRING, [] STRING, [] STRING) PIPE

void genie_exec_sub_pipeline (NODE_T * p)
{
// Child redirects STDIN and STDOUT.
// Return a PIPE that contains the descriptors for the parent.
// 
//        pipe ptoc
//        ->W...R->
//  PARENT         CHILD
//        <-R...W<-
//        pipe ctop

  int pid;
#if defined (BUILD_UNIX)
  int ptoc_fd[2], ctop_fd[2];
#endif
  A68_REF a_prog, a_args, a_env;
  errno = 0;

// Pop parameters.
  POP_REF (p, &a_env);
  POP_REF (p, &a_args);
  POP_REF (p, &a_prog);
#if defined (BUILD_WIN32)
  pid = -1;
  (void) pid;
  genie_mkpipe (p, -1, -1, -1);
  return;
#else
// Create the pipes and fork.
  if ((pipe (ptoc_fd) == -1) || (pipe (ctop_fd) == -1)) {
    genie_mkpipe (p, -1, -1, -1);
    return;
  }
  pid = (int) fork ();
  if (pid == -1) {
// Fork failure.
    genie_mkpipe (p, -1, -1, -1);
    return;
  }
  if (pid == 0) {
// Child process.
    char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
// Convert  strings.
    prog = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_prog)));
    ASSERT (a_to_c_string (p, prog, a_prog) != NO_TEXT);
    convert_string_vector (p, argv, a_args);
    convert_string_vector (p, envp, a_env);
// Set up redirection.
    ASSERT (close (ctop_fd[FD_READ]) == 0);
    ASSERT (close (ptoc_fd[FD_WRITE]) == 0);
    ASSERT (close (STDIN_FILENO) == 0);
    ASSERT (close (STDOUT_FILENO) == 0);
    ASSERT (dup2 (ptoc_fd[FD_READ], STDIN_FILENO) != -1);
    ASSERT (dup2 (ctop_fd[FD_WRITE], STDOUT_FILENO) != -1);
    if (argv[0] == NO_TEXT) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    (void) execve (prog, argv, envp);
// execve only returns if it fails - end child process.
    a68_exit (EXIT_FAILURE);
    genie_mkpipe (p, -1, -1, -1);
  } else {
// Parent process.
    ASSERT (close (ptoc_fd[FD_READ]) == 0);
    ASSERT (close (ctop_fd[FD_WRITE]) == 0);
    genie_mkpipe (p, ctop_fd[FD_READ], ptoc_fd[FD_WRITE], pid);
  }
#endif
}

//! @brief PROC execve output = (STRING, [] STRING, [] STRING, REF_STRING) INT

void genie_exec_sub_output (NODE_T * p)
{
// Child redirects STDIN and STDOUT.
// 
//        pipe ptoc
//        ->W...R->
//  PARENT         CHILD
//        <-R...W<-
//       pipe ctop

  int pid;
#if defined (BUILD_UNIX)
  int ptoc_fd[2], ctop_fd[2];
#endif
  A68_REF a_prog, a_args, a_env, dest;
  errno = 0;
// Pop parameters.
  POP_REF (p, &dest);
  POP_REF (p, &a_env);
  POP_REF (p, &a_args);
  POP_REF (p, &a_prog);
#if defined (BUILD_WIN32)
  pid = -1;
  (void) pid;
  PUSH_VALUE (p, -1, A68_INT);
  return;
#else
// Create the pipes and fork.
  if ((pipe (ptoc_fd) == -1) || (pipe (ctop_fd) == -1)) {
    PUSH_VALUE (p, -1, A68_INT);
    return;
  }
  pid = (int) fork ();
  if (pid == -1) {
// Fork failure.
    PUSH_VALUE (p, -1, A68_INT);
    return;
  }
  if (pid == 0) {
// Child process.
    char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
// Convert  strings.
    prog = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_prog)));
    ASSERT (a_to_c_string (p, prog, a_prog) != NO_TEXT);
    convert_string_vector (p, argv, a_args);
    convert_string_vector (p, envp, a_env);
// Set up redirection.
    ASSERT (close (ctop_fd[FD_READ]) == 0);
    ASSERT (close (ptoc_fd[FD_WRITE]) == 0);
    ASSERT (close (STDIN_FILENO) == 0);
    ASSERT (close (STDOUT_FILENO) == 0);
    ASSERT (dup2 (ptoc_fd[FD_READ], STDIN_FILENO) != -1);
    ASSERT (dup2 (ctop_fd[FD_WRITE], STDOUT_FILENO) != -1);
    if (argv[0] == NO_TEXT) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    (void) execve (prog, argv, envp);
// execve only returns if it fails - end child process.
    a68_exit (EXIT_FAILURE);
    PUSH_VALUE (p, -1, A68_INT);
  } else {
// Parent process.
    char ch;
    int pipe_read, ret, status;
    ASSERT (close (ptoc_fd[FD_READ]) == 0);
    ASSERT (close (ctop_fd[FD_WRITE]) == 0);
    reset_transput_buffer (INPUT_BUFFER);
    do {
      pipe_read = (int) io_read_conv (ctop_fd[FD_READ], &ch, 1);
      if (pipe_read > 0) {
        plusab_transput_buffer (p, INPUT_BUFFER, ch);
      }
    } while (pipe_read > 0);
    do {
      ret = (int) waitpid ((a68_pid_t) pid, &status, 0);
    } while (ret == -1 && errno == EINTR);
    if (ret != pid) {
      status = -1;
    }
    if (!IS_NIL (dest)) {
      *DEREF (A68_REF, &dest) = c_to_a_string (p, get_transput_buffer (INPUT_BUFFER), get_transput_buffer_index (INPUT_BUFFER));
    }
    ASSERT (close (ptoc_fd[FD_WRITE]) == 0);
    ASSERT (close (ctop_fd[FD_READ]) == 0);
    PUSH_VALUE (p, ret, A68_INT);
  }
#endif
}

//! @brief PROC create pipe = PIPE

void genie_create_pipe (NODE_T * p)
{
  errno = 0;
  genie_stand_in (p);
  genie_stand_out (p);
  PUSH_VALUE (p, -1, A68_INT);
}

//! @brief PROC wait pid = (INT) VOID

void genie_waitpid (NODE_T * p)
{
  A68_INT k;
  errno = 0;
  POP_OBJECT (p, &k, A68_INT);
#if defined (BUILD_UNIX)
  ASSERT (waitpid ((a68_pid_t) VALUE (&k), NULL, 0) != -1);
#endif
}
