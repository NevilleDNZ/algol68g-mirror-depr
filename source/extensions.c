/*!
\file extensions.c
\brief extensions to A68 except partial parametrization
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
This code implements some UNIX/Linux related routines.
Partly contributions by Sian Leitch <sian@sleitch.nildram.co.uk>. 
*/

#ifdef WIN32_VERSION
#define PATTERN A68_PATTERN
#endif

#include "algol68g.h"
#include "genie.h"
#include "transput.h"

#ifndef WIN32_VERSION
#include <sys/wait.h>
#endif

#define VECTOR_SIZE 512
#define FD_READ 0
#define FD_WRITE 1

extern A68_REF tmp_to_a68_string (NODE_T *, char *);

extern A68_CHANNEL stand_in_channel, stand_out_channel, stand_draw_channel, stand_back_channel, stand_error_channel;

/*!
\brief PROC [] INT utc time
\param p position in syntax tree, should not be NULL
**/

void genie_utctime (NODE_T * p)
{
  time_t dt;
  if (time (&dt) == (time_t) - 1) {
    empty_row (p, MODE (ROW_INT));
  } else {
    A68_REF row;
    ADDR_T sp = stack_pointer;
    struct tm *tod = gmtime (&dt);
    PUSH_INT (p, tod->tm_year + 1900);
    PUSH_INT (p, tod->tm_mon + 1);
    PUSH_INT (p, tod->tm_mday);
    PUSH_INT (p, tod->tm_hour);
    PUSH_INT (p, tod->tm_min);
    PUSH_INT (p, tod->tm_sec);
    PUSH_INT (p, tod->tm_wday + 1);
    PUSH_INT (p, tod->tm_isdst);
    row = genie_make_row (p, MODE (INT), 8, sp);
    stack_pointer = sp;
    PUSH_REF (p, row);
  }
}

/*!
\brief PROC [] INT local time
\param p position in syntax tree, should not be NULL
**/

void genie_localtime (NODE_T * p)
{
  time_t dt;
  if (time (&dt) == (time_t) - 1) {
    empty_row (p, MODE (ROW_INT));
  } else {
    A68_REF row;
    ADDR_T sp = stack_pointer;
    struct tm *tod = localtime (&dt);
    PUSH_INT (p, tod->tm_year + 1900);
    PUSH_INT (p, tod->tm_mon + 1);
    PUSH_INT (p, tod->tm_mday);
    PUSH_INT (p, tod->tm_hour);
    PUSH_INT (p, tod->tm_min);
    PUSH_INT (p, tod->tm_sec);
    PUSH_INT (p, tod->tm_wday + 1);
    PUSH_INT (p, tod->tm_isdst);
    row = genie_make_row (p, MODE (INT), 8, sp);
    stack_pointer = sp;
    PUSH_REF (p, row);
  }
}

/*!
\brief PROC INT argc
\param p position in syntax tree, should not be NULL
**/

void genie_argc (NODE_T * p)
{
  RESET_ERRNO;
  PUSH_INT (p, global_argc);
}

/*!
\brief PROC (INT) STRING argv
\param p position in syntax tree, should not be NULL
**/

void genie_argv (NODE_T * p)
{
  A68_INT index;
  RESET_ERRNO;
  POP_INT (p, &index);
  if (index.value >= 1 && index.value <= global_argc) {
    PUSH_REF (p, c_to_a_string (p, global_argv[index.value - 1]));
  } else {
    PUSH_REF (p, empty_string (p));
  }
}

/*!
\brief convert [] STRING row to char *vec[]
\param p position in syntax tree, should not be NULL
\param row
**/

static void convert_string_vector (NODE_T * p, char *vec[], A68_REF row)
{
  BYTE_T *z = ADDRESS (&row);
  A68_ARRAY *arr = (A68_ARRAY *) & z[0];
  A68_TUPLE *tup = (A68_TUPLE *) & z[SIZE_OF (A68_ARRAY)];
  int k = 0;
  if (get_row_size (tup, arr->dimensions) != 0) {
    BYTE_T *base_addr = ADDRESS (&arr->array);
    BOOL_T done = A_FALSE;
    initialise_internal_index (tup, arr->dimensions);
    while (!done) {
      ADDR_T index = calculate_internal_index (tup, arr->dimensions);
      ADDR_T elem_addr = (index + arr->slice_offset) * arr->elem_size + arr->field_offset;
      BYTE_T *elem = &base_addr[elem_addr];
      int size = a68_string_size (p, *(A68_REF *) elem);
      TEST_INIT (p, *(A68_REF *) elem, MODE (STRING));
      vec[k] = (char *) get_heap_space (1 + size);
      a_to_c_string (p, vec[k], *(A68_REF *) elem);
      if (k == VECTOR_SIZE - 1) {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_TOO_MANY_ARGUMENTS);
        exit_genie (p, A_RUNTIME_ERROR);
      }
      if (strlen (vec[k]) > 0) {
        k++;
      }
      done = increment_internal_index (tup, arr->dimensions);
    }
  }
  vec[k] = NULL;
}

/*!
\brief free char *vec[]
**/

static void free_vector (char *vec[])
{
  int k = 0;
  while (vec[k] != NULL) {
    free (vec[k]);
    k++;
  }
}

/*!
\brief reset error number
\param p position in syntax tree, should not be NULL
**/

void genie_reset_errno (NODE_T * p)
{
  (void) *p;
  RESET_ERRNO;
}

/*!
\brief error number
\param p position in syntax tree, should not be NULL
**/

void genie_errno (NODE_T * p)
{
  PUSH_INT (p, errno);
}

/*!
\brief PROC strerror = (INT) STRING
\param p position in syntax tree, should not be NULL
**/

void genie_strerror (NODE_T * p)
{
  A68_INT i;
  POP_INT (p, &i);
  PUSH_REF (p, c_to_a_string (p, strerror (i.value)));
}

/*!
\brief set up file for usage in pipe
\param p position in syntax tree, should not be NULL
\param z
\param fd
\param chan
\param r_mood
\param w_mood
\param p position in syntax tree, should not be NULLid
**/

static void set_up_file (NODE_T * p, A68_REF * z, int fd, A68_CHANNEL chan, BOOL_T r_mood, BOOL_T w_mood, int pid)
{
  A68_FILE *f;
  *z = heap_generator (p, MODE (REF_FILE), SIZE_OF (A68_FILE));
  f = (A68_FILE *) ADDRESS (z);
  f->status = (pid < 0) ? 0 : INITIALISED_MASK;
  f->identification = nil_ref;
  f->terminator = nil_ref;
  f->channel = chan;
  f->fd = fd;
  f->device.stream = NULL;
  f->opened = A_TRUE;
  f->open_exclusive = A_FALSE;
  f->read_mood = r_mood;
  f->write_mood = w_mood;
  f->char_mood = A_TRUE;
  f->draw_mood = A_FALSE;
  f->format = nil_format;
  f->transput_buffer = get_unblocked_transput_buffer (p);
  reset_transput_buffer (f->transput_buffer);
  set_default_mended_procedures (f);
}

/*!
\brief create and push a pipe
\param p position in syntax tree, should not be NULL
\param fd_r
\param fd_w
\param p position in syntax tree, should not be NULLid
**/

static void genie_mkpipe (NODE_T * p, int fd_r, int fd_w, int pid)
{
  A68_REF r, w;
  RESET_ERRNO;
/* Set up pipe. */
  set_up_file (p, &r, fd_r, stand_in_channel, A_TRUE, A_FALSE, pid);
  set_up_file (p, &w, fd_w, stand_out_channel, A_FALSE, A_TRUE, pid);
/* push pipe. */
  PUSH_REF_FILE (p, r);
  PUSH_REF_FILE (p, w);
  PUSH_INT (p, pid);
}

/*!
\brief push an environment string
\param p position in syntax tree, should not be NULL
**/

void genie_getenv (NODE_T * p)
{
  A68_REF a_env;
  char *val, *z, *z_env;
  RESET_ERRNO;
  POP (p, &a_env, SIZE_OF (A68_REF));
  TEST_INIT (p, a_env, MODE (STRING));
  z_env = (char *) get_heap_space (1 + a68_string_size (p, a_env));
  z = a_to_c_string (p, z_env, a_env);
  val = getenv (z);
  if (val == NULL) {
    a_env = empty_string (p);
  } else {
    a_env = tmp_to_a68_string (p, val);
  }
  PUSH (p, &a_env, SIZE_OF (A68_REF));
}

/*!
\brief PROC fork = INT
\param p position in syntax tree, should not be NULL
**/

void genie_fork (NODE_T * p)
{
  RESET_ERRNO;
#if defined WIN32_VERSION
  PUSH_INT (p, -1);
#else
  PUSH_INT (p, fork ());
#endif
}

/*!
\brief PROC execve = (STRING, [] STRING, [] STRING) INT 
\param p position in syntax tree, should not be NULL
\return
**/

void genie_execve (NODE_T * p)
{
  int ret;
  A68_REF a_prog, a_args, a_env;
  char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
  RESET_ERRNO;
/* Pop parameters. */
  POP (p, &a_env, SIZE_OF (A68_REF));
  POP (p, &a_args, SIZE_OF (A68_REF));
  POP (p, &a_prog, SIZE_OF (A68_REF));
/* Convert strings and hasta el infinito. */
  prog = (char *) get_heap_space (1 + a68_string_size (p, a_prog));
  a_to_c_string (p, prog, a_prog);
  convert_string_vector (p, argv, a_args);
  convert_string_vector (p, envp, a_env);
  if (argv[0] == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  ret = execve (prog, argv, envp);
/* execve only returns if it fails. */
  free_vector (argv);
  free_vector (envp);
  free (prog);
  PUSH_INT (p, ret);
}

/*!
\brief PROC execve child = (STRING, [] STRING, [] STRING) INT
\param p position in syntax tree, should not be NULL
**/

void genie_execve_child (NODE_T * p)
{
  int pid;
  A68_REF a_prog, a_args, a_env;
  RESET_ERRNO;
/* Pop parameters. */
  POP (p, &a_env, SIZE_OF (A68_REF));
  POP (p, &a_args, SIZE_OF (A68_REF));
  POP (p, &a_prog, SIZE_OF (A68_REF));
/* Now create the pipes and fork. */
#if defined WIN32_VERSION
  pid = -1;
#else
  pid = fork ();
#endif
  if (pid == -1) {
    PUSH_INT (p, -1);
  } else if (pid == 0) {
/* Child process. */
    char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
/* Convert  strings. */
    prog = (char *) get_heap_space (1 + a68_string_size (p, a_prog));
    a_to_c_string (p, prog, a_prog);
    convert_string_vector (p, argv, a_args);
    convert_string_vector (p, envp, a_env);
    if (argv[0] == NULL) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    (void) execve (prog, argv, envp);
/* execve only returns if it fails - end child process. */
    a68g_exit (EXIT_FAILURE);
    PUSH_INT (p, 0);
  } else {
/* parent process. */
    PUSH_INT (p, pid);
  }
}

/*!
\brief PROC execve child pipe = (STRING, [] STRING, [] STRING) PIPE
\param p position in syntax tree, should not be NULL
**/

void genie_execve_child_pipe (NODE_T * p)
{
/*
Child redirects STDIN and STDOUT.
Return a PIPE that contains the descriptors for the parent.

       pipe ptoc
       ->W...R->
 PARENT         CHILD
       <-R...W<-
       pipe ctop
*/
  int pid, ptoc_fd[2], ctop_fd[2];
  A68_REF a_prog, a_args, a_env;
  RESET_ERRNO;
/* Pop parameters. */
  POP (p, &a_env, SIZE_OF (A68_REF));
  POP (p, &a_args, SIZE_OF (A68_REF));
  POP (p, &a_prog, SIZE_OF (A68_REF));
/* Now create the pipes and fork. */
#if defined WIN32_VERSION
  pid = -1;
#else
  if ((pipe (ptoc_fd) == -1) || (pipe (ctop_fd) == -1)) {
    genie_mkpipe (p, -1, -1, -1);
    return;
  }
  pid = fork ();
#endif
  if (pid == -1) {
/* Fork failure. */
    genie_mkpipe (p, -1, -1, -1);
    return;
  }
  if (pid == 0) {
/* Child process. */
    char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
/* Convert  strings. */
    prog = (char *) get_heap_space (1 + a68_string_size (p, a_prog));
    a_to_c_string (p, prog, a_prog);
    convert_string_vector (p, argv, a_args);
    convert_string_vector (p, envp, a_env);
/* Set up redirection. */
    close (ctop_fd[FD_READ]);
    close (ptoc_fd[FD_WRITE]);
    close (STDIN_FILENO);
    close (STDOUT_FILENO);
    dup2 (ptoc_fd[FD_READ], STDIN_FILENO);
    dup2 (ctop_fd[FD_WRITE], STDOUT_FILENO);
    if (argv[0] == NULL) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    (void) execve (prog, argv, envp);
/* execve only returns if it fails - end child process. */
    a68g_exit (EXIT_FAILURE);
    genie_mkpipe (p, -1, -1, -1);
  } else {
/* Parent process. */
    close (ptoc_fd[FD_READ]);
    close (ctop_fd[FD_WRITE]);
    genie_mkpipe (p, ctop_fd[FD_READ], ptoc_fd[FD_WRITE], pid);
  }
}

/*!
\brief PROC create pipe = PIPE
\param p position in syntax tree, should not be NULL
**/

void genie_create_pipe (NODE_T * p)
{
  RESET_ERRNO;
  genie_stand_in (p);
  genie_stand_out (p);
  PUSH_INT (p, -1);
}

/*!
\brief PROC wait pid = (INT) VOID
\param p position in syntax tree, should not be NULL
**/

void genie_waitpid (NODE_T * p)
{
  A68_INT k;
  RESET_ERRNO;
  POP_INT (p, &k);
#if !defined WIN32_VERSION
  waitpid (k.value, NULL, 0);
#endif
}

/*
Next part contains some routines that interface Algol68G and the curses library.
Be sure to know what you are doing when you use this, but on the other hand,
"reset" will always restore your terminal. 
*/

#ifdef HAVE_CURSES

#include <sys/time.h>
#include <curses.h>

#ifdef WIN32_VERSION
#undef PATTERN
#undef FD_READ
#undef FD_WRITE
#include <winsock.h>
#endif

BOOL_T curses_active = A_FALSE;

/*!
\brief clean_curses
**/

void clean_curses ()
{
  if (curses_active == A_TRUE) {
    attrset (A_NORMAL);
    endwin ();
    curses_active = A_FALSE;
  }
}

/*!
\brief init_curses
**/

void init_curses ()
{
  initscr ();
  cbreak ();                    /* raw () would cut off ctrl-c. */
  noecho ();
  nonl ();
  curs_set (0);
  curses_active = A_TRUE;
}

/*!
\brief watch stdin for input, do not wait very long
\return
**/

int rgetchar (void)
{
#ifdef WIN32_VERSION
  return (getch ());
#else
  int retval;
  struct timeval tv;
  fd_set rfds;
  tv.tv_sec = 0;
  tv.tv_usec = 100;
  FD_ZERO (&rfds);
  FD_SET (0, &rfds);
  retval = select (1, &rfds, NULL, NULL, &tv);
  if (retval) {
    /*
     * FD_ISSET(0, &rfds) will be true. 
     */
    return (getch ());
  } else {
    return (NULL_CHAR);
  }
#endif
}

/*!
\brief PROC curses start = VOID
\param p position in syntax tree, should not be NULL
**/

void genie_curses_start (NODE_T * p)
{
  (void) p;
  init_curses ();
}

/*!
\brief PROC curses end = VOID
\param p position in syntax tree, should not be NULL
**/

void genie_curses_end (NODE_T * p)
{
  (void) p;
  clean_curses ();
}

/*!
\brief PROC curses clear = VOID
\param p position in syntax tree, should not be NULL
**/

void genie_curses_clear (NODE_T * p)
{
  (void) p;
  if (curses_active == A_FALSE) {
    init_curses ();
  }
  clear ();
}

/*!
\brief PROC curses refresh = VOID
\param p position in syntax tree, should not be NULL
**/

void genie_curses_refresh (NODE_T * p)
{
  (void) p;
  if (curses_active == A_FALSE) {
    init_curses ();
  }
  refresh ();
}

/*!
\brief PROC curses lines = INT
\param p position in syntax tree, should not be NULL
**/

void genie_curses_lines (NODE_T * p)
{
  if (curses_active == A_FALSE) {
    init_curses ();
  }
  PUSH_INT (p, LINES);
}

/*!
\brief PROC curses columns = INT
\param p position in syntax tree, should not be NULL
**/

void genie_curses_columns (NODE_T * p)
{
  if (curses_active == A_FALSE) {
    init_curses ();
  }
  PUSH_INT (p, COLS);
}

/*!
\brief PROC curses getchar = CHAR
\param p position in syntax tree, should not be NULL
**/

void genie_curses_getchar (NODE_T * p)
{
  if (curses_active == A_FALSE) {
    init_curses ();
  }
  PUSH_CHAR (p, rgetchar ());
}

/*!
\brief PROC curses putchar = (CHAR) VOID
\param p position in syntax tree, should not be NULL
**/

void genie_curses_putchar (NODE_T * p)
{
  A68_CHAR ch;
  if (curses_active == A_FALSE) {
    init_curses ();
  }
  POP_CHAR (p, &ch);
  addch (ch.value);
}

/*!
\brief PROC curses move = (INT, INT) VOID
\param p position in syntax tree, should not be NULL
**/

void genie_curses_move (NODE_T * p)
{
  A68_INT i, j;
  if (curses_active == A_FALSE) {
    init_curses ();
  }
  POP_INT (p, &j);
  POP_INT (p, &i);
  move (i.value, j.value);
}

#endif                          /* HAVE_CURSES */

#ifdef HAVE_POSTGRESQL

/*
PostgreSQL libpq interface based on initial work by Jaap Boender. 
Wraps "connection" and "result" objects in a FILE variable to support 
multiple connections.

Error codes:
0	Success
-1	No connection
-2	No result
-3	Other error
*/

#define LIBPQ_STRING "PostgreSQL libq"
#define ERROR_NOT_CONNECTED "not connected to a database"
#define ERROR_NO_QUERY_RESULT "no query result available"

/*!
\brief PROC pg connect db (REF FILE, STRING, REF STRING) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_connectdb (NODE_T * p)
{
  A68_REF ref_string, ref_file, ref_z, conninfo;
  A68_FILE *file;
  POP_REF (p, &ref_string);
  TEST_NIL (p, ref_string, MODE (REF_STRING));
  POP_REF (p, &conninfo);
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  if (ref_file.segment == heap_segment && ref_string.segment != heap_segment) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, MODE (REF_STRING));
    exit_genie (p, A_RUNTIME_ERROR);
  } else if (ref_file.segment == frame_segment && ref_string.segment == frame_segment && ref_string.scope > ref_file.scope) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, MODE (REF_STRING));
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Initialise the file. */
  file = FILE_DEREF (&ref_file);
  if (file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_ALREADY_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  file->status = INITIALISED_MASK;
  file->channel = associate_channel;
  file->opened = A_TRUE;
  file->open_exclusive = A_FALSE;
  file->read_mood = A_FALSE;
  file->write_mood = A_FALSE;
  file->char_mood = A_FALSE;
  file->draw_mood = A_FALSE;
  file->tmp_file = A_FALSE;
  if (((file->identification.status & INITIALISED_MASK) != 0) && !IS_NIL (file->identification)) {
    UNPROTECT_SWEEP_HANDLE (&(file->identification));
  }
  file->identification = nil_ref;
  file->terminator = nil_ref;
  file->format = nil_format;
  file->fd = -1;
  if (((file->string.status & INITIALISED_MASK) != 0) && !IS_NIL (file->string)) {
    UNPROTECT_SWEEP_HANDLE (&(file->string));
  }
  file->string = ref_string;
  PROTECT_SWEEP_HANDLE (&(file->string));
  file->strpos = 1;
  file->device.stream = NULL;
  set_default_mended_procedures (file);
/* Establish a connection. */
  ref_z = heap_generator (p, MODE (C_STRING), 1 + a68_string_size (p, conninfo));
  file->connection = PQconnectdb (a_to_c_string (p, (char *) ADDRESS (&ref_z), conninfo));
  file->result = NULL;
  if (file->connection == NULL) {
    PUSH_INT (p, -3);
  }
  PQsetErrorVerbosity (file->connection, PQERRORS_DEFAULT);
  if (PQstatus (file->connection) != CONNECTION_OK) {
    PUSH_INT (p, -1);
  } else {
    PUSH_INT (p, 0);
  }
}

/*!
\brief PROC pq finish (REF FILE) VOID
\param p position in syntax tree, should not be NULL
**/

void genie_pq_finish (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result != NULL) {
    PQclear (file->result);
  }
  PQfinish (file->connection);
  file->connection = NULL;
  file->result = NULL;
  PUSH_INT (p, 0);
}

/*!
\brief PROC pq reset (REF FILE) VOID
\param p position in syntax tree, should not be NULL
**/

void genie_pq_reset (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result != NULL) {
    PQclear (file->result);
  }
  PQreset (file->connection);
  PUSH_INT (p, 0);
}

/*!
\brief PROC pq exec = (REF FILE, STRING) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_exec (NODE_T * p)
{
  A68_REF ref_z, query;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &query, SIZE_OF (A68_REF));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result != NULL) {
    PQclear (file->result);
  }
  ref_z = heap_generator (p, MODE (C_STRING), 1 + a68_string_size (p, query));
  file->result = PQexec (file->connection, a_to_c_string (p, (char *) ADDRESS (&ref_z), query));
  if ((PQresultStatus (file->result) != PGRES_TUPLES_OK) && (PQresultStatus (file->result) != PGRES_COMMAND_OK)) {
    PUSH_INT (p, -3);
  } else {
    PUSH_INT (p, 0);
  }
}

/*!
\brief PROC pq parameterstatus (REF FILE) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_parameterstatus (NODE_T * p)
{
  A68_REF ref_z, parameter;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &parameter, SIZE_OF (A68_REF));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  ref_z = heap_generator (p, MODE (C_STRING), 1 + a68_string_size (p, parameter));
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, (char *) PQparameterStatus (file->connection, a_to_c_string (p, (char *) ADDRESS (&ref_z), parameter)));
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq cmdstatus (REF FILE) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_cmdstatus (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQcmdStatus (file->result));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq cmdtuples (REF FILE) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_cmdtuples (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQcmdTuples (file->result));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq ntuples (REF FILE) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_ntuples (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -2);
    return;
  }
  PUSH_INT (p, (PQresultStatus (file->result)) == PGRES_TUPLES_OK ? PQntuples (file->result) : -3);
}

/*!
\brief PROC pq nfields (REF FILE) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_nfields (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -2);
    return;
  }
  PUSH_INT (p, (PQresultStatus (file->result)) == PGRES_TUPLES_OK ? PQnfields (file->result) : -3);
}

/*!
\brief PROC pq fname (REF FILE, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_fname (NODE_T * p)
{
  A68_INT index;
  int upb;
  A68_REF ref_file;
  A68_FILE *file;
  POP_INT (p, &index);
  TEST_INIT (p, index, MODE (INT));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -2);
    return;
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQnfields (file->result) : 0);
  if (index.value < 1 || index.value > upb) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQfname (file->result, index.value - 1));
    file->strpos = 1;
  }
  PUSH_INT (p, 0);
}

/*!
\brief PROC pq fnumber = (REF FILE, STRING) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_fnumber (NODE_T * p)
{
  A68_REF ref_z, name;
  A68_REF ref_file;
  A68_FILE *file;
  int k;
  POP (p, &name, SIZE_OF (A68_REF));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -2);
    return;
  }
  ref_z = heap_generator (p, MODE (C_STRING), 1 + a68_string_size (p, name));
  k = PQfnumber (file->result, a_to_c_string (p, (char *) ADDRESS (&ref_z), name));
  if (k == -1) {
    PUSH_INT (p, -3);
  } else {
    PUSH_INT (p, k + 1);
  }
}

/*!
\brief PROC pq fformat (REF FILE, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_fformat (NODE_T * p)
{
  A68_INT index;
  int upb;
  A68_REF ref_file;
  A68_FILE *file;
  POP_INT (p, &index);
  TEST_INIT (p, index, MODE (INT));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -2);
    return;
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQnfields (file->result) : 0);
  if (index.value < 1 || index.value > upb) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  PUSH_INT (p, PQfformat (file->result, index.value - 1));
}

/*!
\brief PROC pq getvalue (REF FILE, INT, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_getvalue (NODE_T * p)
{
  A68_INT row, column;
  char *str;
  int upb;
  A68_REF ref_file;
  A68_FILE *file;
  POP_INT (p, &column);
  TEST_INIT (p, column, MODE (INT));
  POP_INT (p, &row);
  TEST_INIT (p, row, MODE (INT));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -2);
    return;
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQnfields (file->result) : 0);
  if (column.value < 1 || column.value > upb) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQntuples (file->result) : 0);
  if (row.value < 1 || row.value > upb) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  str = PQgetvalue (file->result, row.value - 1, column.value - 1);
  if (str == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_NO_QUERY_RESULT);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, str);
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq getisnull (REF FILE, INT, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pq_getisnull (NODE_T * p)
{
  A68_INT row, column;
  int upb;
  A68_REF ref_file;
  A68_FILE *file;
  POP_INT (p, &column);
  TEST_INIT (p, column, MODE (INT));
  POP_INT (p, &row);
  TEST_INIT (p, row, MODE (INT));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -2);
    return;
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQnfields (file->result) : 0);
  if (column.value < 1 || column.value > upb) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQntuples (file->result) : 0);
  if (row.value < 1 || row.value > upb) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  PUSH_INT (p, PQgetisnull (file->result, row.value - 1, column.value - 1));
}

/*!
\brief edit error message sting from libpq
\param p position in syntax tree, should not be NULL
**/

static char *pq_edit (char *str)
{
  if (str == NULL) {
    return ("");
  } else {
    static char edt[BUFFER_SIZE];
    char *q;
    int newlines = 0, len = strlen (str);
    BOOL_T suppress_blank = A_FALSE;
    q = edt;
    while (len > 0 && str[len - 1] == NEWLINE_CHAR) {
      str[len - 1] = NULL_CHAR;
      len = strlen (str);
    }
    while (str[0] != NULL_CHAR) {
      if (str[0] == CR_CHAR) {
        str++;
      } else if (str[0] == NEWLINE_CHAR) {
        if (newlines++ == 0) {
          *(q++) = POINT_CHAR;
          *(q++) = BLANK_CHAR;
          *(q++) = '(';
        } else {
          *(q++) = BLANK_CHAR;
        }
        suppress_blank = A_TRUE;
        str++;
      } else if (IS_SPACE (str[0])) {
        if (suppress_blank) {
          str++;
        } else {
          if (str[1] != NEWLINE_CHAR) {
            *(q++) = BLANK_CHAR;
          }
          str++;
          suppress_blank = A_TRUE;
        }
      } else {
        *(q++) = *(str++);
        suppress_blank = A_FALSE;
      }
    }
    if (newlines > 0) {
      *(q++) = ')';
    }
    q[0] = NULL_CHAR;
    return (edt);
  }
}

/*!
\brief PROC pq errormessage (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_errormessage (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    char str[BUFFER_SIZE];
    int upb;
    if (PQerrorMessage (file->connection) != NULL) {
      bufcpy (str, pq_edit (PQerrorMessage (file->connection)), BUFFER_SIZE);
      upb = strlen (str);
      if (upb > 0 && str[upb - 1] == NEWLINE_CHAR) {
        str[upb - 1] = NULL_CHAR;
      }
    } else {
      bufcpy (str, "no error message available", BUFFER_SIZE);
    }
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, str);
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq resulterrormessage (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_resulterrormessage (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (file->result == NULL) {
    PUSH_INT (p, -2);
    return;
  }
  if (!IS_NIL (file->string)) {
    char str[BUFFER_SIZE];
    int upb;
    if (PQresultErrorMessage (file->result) != NULL) {
      bufcpy (str, pq_edit (PQresultErrorMessage (file->result)), BUFFER_SIZE);
      upb = strlen (str);
      if (upb > 0 && str[upb - 1] == NEWLINE_CHAR) {
        str[upb - 1] = NULL_CHAR;
      }
    } else {
      bufcpy (str, "no error message available", BUFFER_SIZE);
    }
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, str);
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq db (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_db (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQdb (file->connection));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq user (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_user (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQuser (file->connection));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq pass (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_pass (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQpass (file->connection));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq host (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_host (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQhost (file->connection));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq port (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_port (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQport (file->connection));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq tty (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_tty (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQtty (file->connection));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq options (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_options (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQoptions (file->connection));
    file->strpos = 1;
    PUSH_INT (p, 0);
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq protocol version (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_protocolversion (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    PUSH_INT (p, PQprotocolVersion (file->connection));
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq server version (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_serverversion (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    PUSH_INT (p, PQserverVersion (file->connection));
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq socket (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_socket (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    PUSH_INT (p, PQsocket (file->connection));
  } else {
    PUSH_INT (p, -3);
  }
}

/*!
\brief PROC pq backend pid (REF FILE) INT 
\param p position in syntax tree, should not be NULL
**/

void genie_pq_backendpid (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (file->connection == NULL) {
    PUSH_INT (p, -1);
    return;
  }
  if (!IS_NIL (file->string)) {
    PUSH_INT (p, PQbackendPID (file->connection));
  } else {
    PUSH_INT (p, -3);
  }
}

#endif                          /* HAVE_POSTGRESQL */
