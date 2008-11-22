/*!
\file io.c
\brief low-level UNIX IO routines
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


#include "algol68g.h"
#include "genie.h"
#include "transput.h"

#define MAX_RESTART 256

BOOL_T halt_typing;
static int chars_in_tty_line;

char output_line[BUFFER_SIZE], edit_line[BUFFER_SIZE], input_line[BUFFER_SIZE];

/*!
\brief initialise output to STDOUT
**/

void init_tty (void)
{
  chars_in_tty_line = 0;
  halt_typing = A68_FALSE;
  change_masks (a68_prog.top_node, BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
}

/*!
\brief terminate current line on STDOUT
**/

void io_close_tty_line (void)
{
  if (chars_in_tty_line > 0) {
    io_write_string (STDOUT_FILENO, NEWLINE_STRING);
  }
}

/*!
\brief get a char from STDIN
\return same
**/

char get_stdin_char (void)
{
  ssize_t j;
  char ch[4];
  RESET_ERRNO;
  j = io_read_conv (STDIN_FILENO, &(ch[0]), 1);
  ABNORMAL_END (j < 0, "cannot read char from stdin", NULL);
  return (j == 1 ? ch[0] : EOF_CHAR);
}

/*!
\brief read string from STDIN, until NEWLINE_STRING
\param prompt prompt string
\return input line buffer
**/

char *read_string_from_tty (char *prompt)
{
  int ch, k = 0, n;
  if (prompt != NULL) {
    io_close_tty_line ();
    io_write_string (STDOUT_FILENO, prompt);
  }
  ch = get_stdin_char ();
  while (ch != NEWLINE_CHAR && k < BUFFER_SIZE - 1) {
    if (ch == EOF_CHAR) {
      input_line[0] = EOF_CHAR;
      input_line[1] = NULL_CHAR;
      chars_in_tty_line = 1;
      return (input_line);
    } else {
      input_line[k++] = ch;
      ch = get_stdin_char ();
    }
  }
  input_line[k] = NULL_CHAR;
  n = strlen (input_line);
  chars_in_tty_line = (ch == NEWLINE_CHAR ? 0 : (n > 0 ? n : 1));
  return (input_line);
}

/*!
\brief write string to file
\param f file number
\param z string to write
**/

void io_write_string (FILE_T f, const char *z)
{
  ssize_t j;
  RESET_ERRNO;
  if (f != STDOUT_FILENO && f != STDERR_FILENO) {
/* Writing to file. */
    j = io_write_conv (f, z, strlen (z));
    ABNORMAL_END (j < 0, "cannot write", NULL);
  } else {
/* Writing to TTY. */
    int first, k;
/* Write parts until end-of-string. */
    first = 0;
    do {
      k = first;
/* How far can we get? */
      while (z[k] != NULL_CHAR && z[k] != NEWLINE_CHAR) {
        k++;
      }
      if (k > first) {
/* Write these characters. */
        int n = k - first;
        j = io_write_conv (f, &(z[first]), n);
        ABNORMAL_END (j < 0, "cannot write", NULL);
        chars_in_tty_line += n;
      }
      if (z[k] == NEWLINE_CHAR) {
/* Pretty-print newline. */
        k++;
        first = k;
        j = io_write_conv (f, NEWLINE_STRING, 1);
        ABNORMAL_END (j < 0, "cannot write", NULL);
        chars_in_tty_line = 0;
      }
    } while (z[k] != NULL_CHAR);
  }
}

/*!
\brief read bytes from file into buffer
\param fd file descriptor, must be open
\param buf character buffer, size must be >= n
\param n maximum number of bytes to read
\return number of bytes read or -1 in case of error
**/

ssize_t io_read (FILE_T fd, void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
#if defined ENABLE_WIN32
    int bytes_read;
#else
    ssize_t bytes_read;
#endif
    RESET_ERRNO;
    bytes_read = read (fd, z, to_do);
    if (bytes_read < 0) {
      if (errno == EINTR) {
/* interrupt, retry. */
        bytes_read = 0;
        if (restarts++ > MAX_RESTART) {
          return (-1);
        }
      } else {
/* read error. */
        return (-1);
      }
    } else if (bytes_read == 0) {
      break;                    /* EOF_CHAR. */
    }
    to_do -= bytes_read;
    z += bytes_read;
  }
  return (n - to_do);           /* return >= 0 */
}

/*!
\brief writes n bytes from buffer to file
\param fd file descriptor, must be open
\param buf character buffer, size must be >= n
\param n maximum number of bytes to write
\return n or -1 in case of error
**/

ssize_t io_write (FILE_T fd, const void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
    ssize_t bytes_written;
    RESET_ERRNO;
    bytes_written = write (fd, z, to_do);
    if (bytes_written <= 0) {
      if (errno == EINTR) {
/* interrupt, retry. */
        bytes_written = 0;
        if (restarts++ > MAX_RESTART) {
          return (-1);
        }
      } else {
/* write error. */
        return (-1);
      }
    }
    to_do -= bytes_written;
    z += bytes_written;
  }
  return (n);
}

/*!
\brief read bytes from file into buffer
\param fd file descriptor, must be open
\param buf character buffer, size must be >= n
\param n maximum number of bytes to read
\return number of bytes read or -1 in case of error
**/

ssize_t io_read_conv (FILE_T fd, void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
#if defined ENABLE_WIN32
    int bytes_read;
#else
    ssize_t bytes_read;
#endif
    RESET_ERRNO;
    bytes_read = read (fd, z, to_do);
    if (bytes_read < 0) {
      if (errno == EINTR) {
/* interrupt, retry. */
        bytes_read = 0;
        if (restarts++ > MAX_RESTART) {
          return (-1);
        }
      } else {
/* read error. */
        return (-1);
      }
    } else if (bytes_read == 0) {
      break;                    /* EOF_CHAR. */
    }
    to_do -= bytes_read;
    z += bytes_read;
  }
  return (n - to_do);
}

/*!
\brief writes n bytes from buffer to file
\param fd file descriptor, must be open
\param buf character buffer, size must be >= n
\param n maximum number of bytes to write
\return n or -1 in case of error
**/

ssize_t io_write_conv (FILE_T fd, const void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
    ssize_t bytes_written;
    RESET_ERRNO;
    bytes_written = write (fd, z, to_do);
    if (bytes_written <= 0) {
      if (errno == EINTR) {
/* interrupt, retry. */
        bytes_written = 0;
        if (restarts++ > MAX_RESTART) {
          return (-1);
        }
      } else {
/* write error. */
        return (-1);
      }
    }
    to_do -= bytes_written;
    z += bytes_written;
  }
  return (n);
}
