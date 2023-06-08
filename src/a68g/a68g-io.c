//! @file io.c
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

//! @brief Initialise output to STDOUT.

void init_tty (void)
{
  A68 (chars_in_tty_line) = 0;
  A68 (halt_typing) = A68_FALSE;
  change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
}

//! @brief Terminate current line on STDOUT.

void io_close_tty_line (void)
{
  if (A68 (chars_in_tty_line) > 0) {
    io_write_string (STDOUT_FILENO, NEWLINE_STRING);
  }
}

//! @brief Get a char from STDIN.

char get_stdin_char (void)
{
  ssize_t j;
  char ch[4];
  errno = 0;
  j = io_read_conv (STDIN_FILENO, &(ch[0]), 1);
  ABEND (j < 0, ERROR_ACTION, __func__);
  return (char) (j == 1 ? ch[0] : EOF_CHAR);
}

//! @brief Read string from STDIN, until NEWLINE_STRING.

char *read_string_from_tty (char *prompt)
{
#if defined (HAVE_READLINE)
  char *line = readline (prompt);
  if (line != NO_TEXT && (int) strlen (line) > 0) {
    add_history (line);
  }
  bufcpy (A68 (input_line), line, BUFFER_SIZE);
  A68 (chars_in_tty_line) = (int) strlen (A68 (input_line));
  a68_free (line);
  return A68 (input_line);

#else
  int ch, k = 0, n;
  if (prompt != NO_TEXT) {
    io_close_tty_line ();
    io_write_string (STDOUT_FILENO, prompt);
  }
  ch = get_stdin_char ();
  while (ch != NEWLINE_CHAR && k < BUFFER_SIZE - 1) {
    if (ch == EOF_CHAR) {
      A68 (input_line)[0] = EOF_CHAR;
      A68 (input_line)[1] = NULL_CHAR;
      A68 (chars_in_tty_line) = 1;
      return A68 (input_line);
    } else {
      A68 (input_line)[k++] = (char) ch;
      ch = get_stdin_char ();
    }
  }
  A68 (input_line)[k] = NULL_CHAR;
  n = (int) strlen (A68 (input_line));
  A68 (chars_in_tty_line) = (ch == NEWLINE_CHAR ? 0 : (n > 0 ? n : 1));
  return A68 (input_line);
#endif
}

//! @brief Write string to file.

void io_write_string (FILE_T f, const char *z)
{
  ssize_t j;
  errno = 0;
  if (f != STDOUT_FILENO && f != STDERR_FILENO) {
// Writing to file.
    j = io_write_conv (f, z, strlen (z));
    ABEND (j < 0, ERROR_ACTION, __func__);
  } else {
// Writing to TTY.
    int first, k;
// Write parts until end-of-string.
    first = 0;
    do {
      k = first;
// How far can we get?.
      while (z[k] != NULL_CHAR && z[k] != NEWLINE_CHAR) {
        k++;
      }
      if (k > first) {
// Write these characters.
        int n = k - first;
        j = io_write_conv (f, &(z[first]), (size_t) n);
        ABEND (j < 0, ERROR_ACTION, __func__);
        A68 (chars_in_tty_line) += n;
      }
      if (z[k] == NEWLINE_CHAR) {
// Pretty-print newline.
        k++;
        first = k;
        j = io_write_conv (f, NEWLINE_STRING, 1);
        ABEND (j < 0, ERROR_ACTION, __func__);
        A68 (chars_in_tty_line) = 0;
      }
    } while (z[k] != NULL_CHAR);
  }
}

//! @brief Read bytes from file into buffer.

ssize_t io_read (FILE_T fd, void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
#if defined (BUILD_WIN32)
    int bytes_read;
#else
    ssize_t bytes_read;
#endif
    errno = 0;
    bytes_read = read (fd, z, to_do);
    if (bytes_read < 0) {
      if (errno == EINTR) {
// interrupt, retry.
        bytes_read = 0;
        if (restarts++ > MAX_RESTART) {
          return -1;
        }
      } else {
// read error.
        return -1;
      }
    } else if (bytes_read == 0) {
      break;                    // EOF_CHAR
    }
    to_do -= (size_t) bytes_read;
    z += bytes_read;
  }
  return (ssize_t) n - (ssize_t) to_do; // return >= 0
}

//! @brief Writes n bytes from buffer to file.

ssize_t io_write (FILE_T fd, const void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
    ssize_t bytes_written;
    errno = 0;
    bytes_written = write (fd, z, to_do);
    if (bytes_written <= 0) {
      if (errno == EINTR) {
// interrupt, retry.
        bytes_written = 0;
        if (restarts++ > MAX_RESTART) {
          return -1;
        }
      } else {
// write error.
        return -1;
      }
    }
    to_do -= (size_t) bytes_written;
    z += bytes_written;
  }
  return (ssize_t) n;
}

//! @brief Read bytes from file into buffer.

ssize_t io_read_conv (FILE_T fd, void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
#if defined (BUILD_WIN32)
    int bytes_read;
#else
    ssize_t bytes_read;
#endif
    errno = 0;
    bytes_read = read (fd, z, to_do);
    if (bytes_read < 0) {
      if (errno == EINTR) {
// interrupt, retry.
        bytes_read = 0;
        if (restarts++ > MAX_RESTART) {
          return -1;
        }
      } else {
// read error.
        return -1;
      }
    } else if (bytes_read == 0) {
      break;                    // EOF_CHAR
    }
    to_do -= (size_t) bytes_read;
    z += bytes_read;
  }
  return (ssize_t) n - (ssize_t) to_do;
}

//! @brief Writes n bytes from buffer to file.

ssize_t io_write_conv (FILE_T fd, const void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
    ssize_t bytes_written;
    errno = 0;
    bytes_written = write (fd, z, to_do);
    if (bytes_written <= 0) {
      if (errno == EINTR) {
// interrupt, retry.
        bytes_written = 0;
        if (restarts++ > MAX_RESTART) {
          return -1;
        }
      } else {
// write error.
        return -1;
      }
    }
    to_do -= (size_t) bytes_written;
    z += bytes_written;
  }
  return (ssize_t) n;
}
