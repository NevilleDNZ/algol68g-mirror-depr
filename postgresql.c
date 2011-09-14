/*!
\file postgresql.c
\brief interface to libpq
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2011 J. Marcel van der Veer <algol68g@xs4all.nl>.

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
PostgreSQL libpq interface based on initial work by Jaap Boender. 
Wraps "connection" and "result" objects in a FILE variable to support 
multiple connections.

Error codes:
0	Success
-1	No connection
-2	No result
-3	Other error
*/

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#if (defined HAVE_LIBPQ_FE_H && defined HAVE_LIBPQ)

#include "a68g.h"

#define LIBPQ_STRING "PostgreSQL libq"
#define ERROR_NOT_CONNECTED "not connected to a database"
#define ERROR_NO_QUERY_RESULT "no query result available"

/*!
\brief PROC pg connect db (REF FILE, STRING, REF STRING) INT
\param p position in tree
**/

void genie_pq_connectdb (NODE_T * p)
{
  A68_REF ref_string, ref_file, ref_z, conninfo;
  A68_FILE *file;
  POP_REF (p, &ref_string);
  CHECK_REF (p, ref_string, MODE (REF_STRING));
  POP_REF (p, &conninfo);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  if (IS_IN_HEAP (&ref_file) && !IS_IN_HEAP (&ref_string)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, MODE (REF_STRING));
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (IS_IN_FRAME (&ref_file) && IS_IN_FRAME (&ref_string)) {
    if (REF_SCOPE (&ref_string) > REF_SCOPE (&ref_file)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, MODE (REF_STRING));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
/* Initialise the file */
  file = FILE_DEREF (&ref_file);
  if (file->opened) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ALREADY_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  STATUS (file) = INITIALISED_MASK;
  file->channel = associate_channel;
  file->opened = A68_TRUE;
  file->open_exclusive = A68_FALSE;
  file->read_mood = A68_FALSE;
  file->write_mood = A68_FALSE;
  file->char_mood = A68_FALSE;
  file->draw_mood = A68_FALSE;
  file->tmp_file = A68_FALSE;
  if (INITIALISED (&(file->identification)) && !IS_NIL (file->identification)) {
    UNBLOCK_GC_HANDLE (&(file->identification));
  }
  file->identification = nil_ref;
  file->terminator = nil_ref;
  FORMAT (file) = nil_format;
  file->fd = -1;
  if (INITIALISED (&(file->string)) && !IS_NIL (file->string)) {
    UNBLOCK_GC_HANDLE (&(file->string));
  }
  file->string = ref_string;
  BLOCK_GC_HANDLE (&(file->string));
  file->strpos = 1;
  file->device.stream = NULL;
  set_default_mended_procedures (file);
/* Establish a connection */
  ref_z = heap_generator (p, MODE (C_STRING), 1 + a68_string_size (p, conninfo));
  file->connection = PQconnectdb (a_to_c_string (p, (char *) ADDRESS (&ref_z), conninfo));
  file->result = NULL;
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
  (void) PQsetErrorVerbosity (file->connection, PQERRORS_DEFAULT);
  if (PQstatus (file->connection) != CONNECTION_OK) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, 0, A68_INT);
  }
}

/*!
\brief PROC pq finish (REF FILE) VOID
\param p position in tree
**/

void genie_pq_finish (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result != NULL) {
    PQclear (file->result);
  }
  PQfinish (file->connection);
  file->connection = NULL;
  file->result = NULL;
  PUSH_PRIMITIVE (p, 0, A68_INT);
}

/*!
\brief PROC pq reset (REF FILE) VOID
\param p position in tree
**/

void genie_pq_reset (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result != NULL) {
    PQclear (file->result);
  }
  PQreset (file->connection);
  PUSH_PRIMITIVE (p, 0, A68_INT);
}

/*!
\brief PROC pq exec = (REF FILE, STRING) INT
\param p position in tree
**/

void genie_pq_exec (NODE_T * p)
{
  A68_REF ref_z, query;
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &query);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result != NULL) {
    PQclear (file->result);
  }
  ref_z = heap_generator (p, MODE (C_STRING), 1 + a68_string_size (p, query));
  file->result = PQexec (file->connection, a_to_c_string (p, (char *) ADDRESS (&ref_z), query));
  if ((PQresultStatus (file->result) != PGRES_TUPLES_OK)
      && (PQresultStatus (file->result) != PGRES_COMMAND_OK)) {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, 0, A68_INT);
  }
}

/*!
\brief PROC pq parameterstatus (REF FILE) INT
\param p position in tree
**/

void genie_pq_parameterstatus (NODE_T * p)
{
  A68_REF ref_z, parameter;
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &parameter);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  ref_z = heap_generator (p, MODE (C_STRING), 1 + a68_string_size (p, parameter));
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, (char *) PQparameterStatus (file->connection, a_to_c_string (p, (char *) ADDRESS (&ref_z), parameter)));
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq cmdstatus (REF FILE) INT
\param p position in tree
**/

void genie_pq_cmdstatus (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQcmdStatus (file->result));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq cmdtuples (REF FILE) INT
\param p position in tree
**/

void genie_pq_cmdtuples (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQcmdTuples (file->result));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq ntuples (REF FILE) INT
\param p position in tree
**/

void genie_pq_ntuples (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -2, A68_INT);
    return;
  }
  PUSH_PRIMITIVE (p, (PQresultStatus (file->result)) == PGRES_TUPLES_OK ? PQntuples (file->result) : -3, A68_INT);
}

/*!
\brief PROC pq nfields (REF FILE) INT
\param p position in tree
**/

void genie_pq_nfields (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -2, A68_INT);
    return;
  }
  PUSH_PRIMITIVE (p, (PQresultStatus (file->result)) == PGRES_TUPLES_OK ? PQnfields (file->result) : -3, A68_INT);
}

/*!
\brief PROC pq fname (REF FILE, INT) INT
\param p position in tree
**/

void genie_pq_fname (NODE_T * p)
{
  A68_INT a68g_index;
  int upb;
  A68_REF ref_file;
  A68_FILE *file;
  POP_OBJECT (p, &a68g_index, A68_INT);
  CHECK_INIT (p, INITIALISED (&a68g_index), MODE (INT));
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -2, A68_INT);
    return;
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQnfields (file->result) : 0);
  if (VALUE (&a68g_index) < 1 || VALUE (&a68g_index) > upb) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQfname (file->result, VALUE (&a68g_index) - 1));
    file->strpos = 1;
  }
  PUSH_PRIMITIVE (p, 0, A68_INT);
}

/*!
\brief PROC pq fnumber = (REF FILE, STRING) INT
\param p position in tree
**/

void genie_pq_fnumber (NODE_T * p)
{
  A68_REF ref_z, name;
  A68_REF ref_file;
  A68_FILE *file;
  int k;
  POP_REF (p, &name);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -2, A68_INT);
    return;
  }
  ref_z = heap_generator (p, MODE (C_STRING), 1 + a68_string_size (p, name));
  k = PQfnumber (file->result, a_to_c_string (p, (char *) ADDRESS (&ref_z), name));
  if (k == -1) {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, k + 1, A68_INT);
  }
}

/*!
\brief PROC pq fformat (REF FILE, INT) INT
\param p position in tree
**/

void genie_pq_fformat (NODE_T * p)
{
  A68_INT a68g_index;
  int upb;
  A68_REF ref_file;
  A68_FILE *file;
  POP_OBJECT (p, &a68g_index, A68_INT);
  CHECK_INIT (p, INITIALISED (&a68g_index), MODE (INT));
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -2, A68_INT);
    return;
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQnfields (file->result) : 0);
  if (VALUE (&a68g_index) < 1 || VALUE (&a68g_index) > upb) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, PQfformat (file->result, VALUE (&a68g_index) - 1), A68_INT);
}

/*!
\brief PROC pq getvalue (REF FILE, INT, INT) INT
\param p position in tree
**/

void genie_pq_getvalue (NODE_T * p)
{
  A68_INT row, column;
  char *str;
  int upb;
  A68_REF ref_file;
  A68_FILE *file;
  POP_OBJECT (p, &column, A68_INT);
  CHECK_INIT (p, INITIALISED (&column), MODE (INT));
  POP_OBJECT (p, &row, A68_INT);
  CHECK_INIT (p, INITIALISED (&row), MODE (INT));
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -2, A68_INT);
    return;
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQnfields (file->result) : 0);
  if (VALUE (&column) < 1 || VALUE (&column) > upb) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQntuples (file->result) : 0);
  if (VALUE (&row) < 1 || VALUE (&row) > upb) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  str = PQgetvalue (file->result, VALUE (&row) - 1, VALUE (&column) - 1);
  if (str == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_NO_QUERY_RESULT);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, str);
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq getisnull (REF FILE, INT, INT) INT
\param p position in tree
**/

void genie_pq_getisnull (NODE_T * p)
{
  A68_INT row, column;
  int upb;
  A68_REF ref_file;
  A68_FILE *file;
  POP_OBJECT (p, &column, A68_INT);
  CHECK_INIT (p, INITIALISED (&column), MODE (INT));
  POP_OBJECT (p, &row, A68_INT);
  CHECK_INIT (p, INITIALISED (&row), MODE (INT));
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -2, A68_INT);
    return;
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQnfields (file->result) : 0);
  if (VALUE (&column) < 1 || VALUE (&column) > upb) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  upb = (PQresultStatus (file->result) == PGRES_TUPLES_OK ? PQntuples (file->result) : 0);
  if (VALUE (&row) < 1 || VALUE (&row) > upb) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, PQgetisnull (file->result, VALUE (&row) - 1, VALUE (&column) - 1), A68_INT);
}

/*!
\brief edit error message sting from libpq
\param p position in tree
**/

static char *pq_edit (char *str)
{
  if (str == NULL) {
    return ("");
  } else {
    static char edt[BUFFER_SIZE];
    char *q;
    int newlines = 0, len = (int) strlen (str);
    BOOL_T suppress_blank = A68_FALSE;
    q = edt;
    while (len > 0 && str[len - 1] == NEWLINE_CHAR) {
      str[len - 1] = NULL_CHAR;
      len = (int) strlen (str);
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
        suppress_blank = A68_TRUE;
        str++;
      } else if (IS_SPACE (str[0])) {
        if (suppress_blank) {
          str++;
        } else {
          if (str[1] != NEWLINE_CHAR) {
            *(q++) = BLANK_CHAR;
          }
          str++;
          suppress_blank = A68_TRUE;
        }
      } else {
        *(q++) = *(str++);
        suppress_blank = A68_FALSE;
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
\param p position in tree
**/

void genie_pq_errormessage (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    char str[BUFFER_SIZE];
    int upb;
    if (PQerrorMessage (file->connection) != NULL) {
      bufcpy (str, pq_edit (PQerrorMessage (file->connection)), BUFFER_SIZE);
      upb = (int) strlen (str);
      if (upb > 0 && str[upb - 1] == NEWLINE_CHAR) {
        str[upb - 1] = NULL_CHAR;
      }
    } else {
      bufcpy (str, "no error message available", BUFFER_SIZE);
    }
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, str);
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq resulterrormessage (REF FILE) INT 
\param p position in tree
**/

void genie_pq_resulterrormessage (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (file->result == NULL) {
    PUSH_PRIMITIVE (p, -2, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    char str[BUFFER_SIZE];
    int upb;
    if (PQresultErrorMessage (file->result) != NULL) {
      bufcpy (str, pq_edit (PQresultErrorMessage (file->result)), BUFFER_SIZE);
      upb = (int) strlen (str);
      if (upb > 0 && str[upb - 1] == NEWLINE_CHAR) {
        str[upb - 1] = NULL_CHAR;
      }
    } else {
      bufcpy (str, "no error message available", BUFFER_SIZE);
    }
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, str);
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq db (REF FILE) INT 
\param p position in tree
**/

void genie_pq_db (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQdb (file->connection));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq user (REF FILE) INT 
\param p position in tree
**/

void genie_pq_user (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQuser (file->connection));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq pass (REF FILE) INT 
\param p position in tree
**/

void genie_pq_pass (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQpass (file->connection));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq host (REF FILE) INT 
\param p position in tree
**/

void genie_pq_host (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQhost (file->connection));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq port (REF FILE) INT 
\param p position in tree
**/

void genie_pq_port (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQport (file->connection));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq tty (REF FILE) INT 
\param p position in tree
**/

void genie_pq_tty (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQtty (file->connection));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq options (REF FILE) INT 
\param p position in tree
**/

void genie_pq_options (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    *(A68_REF *) ADDRESS (&file->string) = c_to_a_string (p, PQoptions (file->connection));
    file->strpos = 1;
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq protocol version (REF FILE) INT 
\param p position in tree
**/

void genie_pq_protocolversion (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    PUSH_PRIMITIVE (p, PQprotocolVersion (file->connection), A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq server version (REF FILE) INT 
\param p position in tree
**/

void genie_pq_serverversion (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    PUSH_PRIMITIVE (p, PQserverVersion (file->connection), A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq socket (REF FILE) INT 
\param p position in tree
**/

void genie_pq_socket (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    PUSH_PRIMITIVE (p, PQsocket (file->connection), A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

/*!
\brief PROC pq backend pid (REF FILE) INT 
\param p position in tree
**/

void genie_pq_backendpid (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (file->connection == NULL) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (!IS_NIL (file->string)) {
    PUSH_PRIMITIVE (p, PQbackendPID (file->connection), A68_INT);
  } else {
    PUSH_PRIMITIVE (p, -3, A68_INT);
  }
}

#endif /* HAVE_LIBPQ_FE_H && defined HAVE_LIBPQ */
