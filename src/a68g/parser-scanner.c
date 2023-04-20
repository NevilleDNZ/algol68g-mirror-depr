//! @file parser-scanner.c
//! @author J. Marcel van der Veer
//!
//! @section Copyright
//!
//! This file is part of Algol68G - an Algol 68 compiler-interpreter.
//! Copyright 2001-2023 J. Marcel van der Veer [algol68g@xs4all.nl].
//!
//! @section License
//!
//! This program is free software; you can redistribute it and/or modify it 
//! under the terms of the GNU General Public License as published by the 
//! Free Software Foundation; either version 3 of the License, or 
//! (at your option) any later version.
//!
//! This program is distributed in the hope that it will be useful, but 
//! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
//! or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
//! more details. You should have received a copy of the GNU General Public 
//! License along with this program. If not, see [http://www.gnu.org/licenses/].

//! @section Synopsis 
//!
//! Context-dependent Algol 68 tokeniser.

#include "a68g.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"
#include "a68g-options.h"
#include "a68g-environ.h"
#include "a68g-genie.h"

// Macros.

#define SCAN_DIGITS(c)\
  while (IS_DIGIT (c)) {\
    (sym++)[0] = (c);\
    (c) = next_char (ref_l, ref_s, A68_TRUE);\
  }

#define SCAN_EXPONENT_PART(c)\
  (sym++)[0] = EXPONENT_CHAR;\
  (c) = next_char (ref_l, ref_s, A68_TRUE);\
  if ((c) == '+' || (c) == '-') {\
    (sym++)[0] = (c);\
    (c) = next_char (ref_l, ref_s, A68_TRUE);\
  }\
  SCAN_ERROR (!IS_DIGIT (c), *start_l, *start_c, ERROR_EXPONENT_DIGIT);\
  SCAN_DIGITS (c)

//! @brief Save scanner state, for character look-ahead.

void save_state (LINE_T * ref_l, char *ref_s, char ch)
{
  SCAN_STATE_L (&A68_JOB) = ref_l;
  SCAN_STATE_S (&A68_JOB) = ref_s;
  SCAN_STATE_C (&A68_JOB) = ch;
}

//! @brief Restore scanner state, for character look-ahead.

void restore_state (LINE_T ** ref_l, char **ref_s, char *ch)
{
  *ref_l = SCAN_STATE_L (&A68_JOB);
  *ref_s = SCAN_STATE_S (&A68_JOB);
  *ch = SCAN_STATE_C (&A68_JOB);
}

//! @brief New_source_line.

LINE_T *new_source_line (void)
{
  LINE_T *z = (LINE_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (LINE_T));
  MARKER (z)[0] = NULL_CHAR;
  STRING (z) = NO_TEXT;
  FILENAME (z) = NO_TEXT;
  DIAGNOSTICS (z) = NO_DIAGNOSTIC;
  NUMBER (z) = 0;
  PRINT_STATUS (z) = 0;
  LIST (z) = A68_TRUE;
  NEXT (z) = NO_LINE;
  PREVIOUS (z) = NO_LINE;
  return z;
}

//! @brief Append a source line to the internal source file.

void append_source_line (char *str, LINE_T ** ref_l, int *line_num, char *filename)
{
  LINE_T *z = new_source_line ();
// Allow shell command in first line, f.i. "#!/usr/share/bin/a68g".
  if (*line_num == 1) {
    if (strlen (str) >= 2 && strncmp (str, "#!", 2) == 0) {
      ABEND (strstr (str, "run-script") != NO_TEXT, ERROR_SHELL_SCRIPT, __func__);
      (*line_num)++;
      return;
    }
  }
// Link line into the chain.
  STRING (z) = new_fixed_string (str);
  FILENAME (z) = filename;
  NUMBER (z) = (*line_num)++;
  PRINT_STATUS (z) = NOT_PRINTED;
  LIST (z) = A68_TRUE;
  DIAGNOSTICS (z) = NO_DIAGNOSTIC;
  NEXT (z) = NO_LINE;
  PREVIOUS (z) = *ref_l;
  if (TOP_LINE (&A68_JOB) == NO_LINE) {
    TOP_LINE (&A68_JOB) = z;
  }
  if (*ref_l != NO_LINE) {
    NEXT (*ref_l) = z;
  }
  *ref_l = z;
}

// Scanner, tokenises the source code.

//! @brief Whether ch is unworthy.

void unworthy (LINE_T * u, char *v, char ch)
{
  if (IS_PRINT (ch)) {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "*%s", ERROR_UNWORTHY_CHARACTER) >= 0);
  } else {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "*%s %s", ERROR_UNWORTHY_CHARACTER, ctrl_char (ch)) >= 0);
  }
  scan_error (u, v, A68 (edit_line));
}

//! @brief Concatenate lines that terminate in '\' with next line.

void concatenate_lines (LINE_T * top)
{
  LINE_T *q;
// Work from bottom backwards.
  for (q = top; q != NO_LINE && NEXT (q) != NO_LINE; q = NEXT (q)) {
    ;
  }
  for (; q != NO_LINE; BACKWARD (q)) {
    char *z = STRING (q);
    int len = (int) strlen (z);
    if (len >= 2 && z[len - 2] == BACKSLASH_CHAR && z[len - 1] == NEWLINE_CHAR && NEXT (q) != NO_LINE && STRING (NEXT (q)) != NO_TEXT) {
      z[len - 2] = NULL_CHAR;
      len += (int) strlen (STRING (NEXT (q)));
      z = (char *) get_fixed_heap_space ((size_t) (len + 1));
      bufcpy (z, STRING (q), len + 1);
      bufcat (z, STRING (NEXT (q)), len + 1);
      STRING (NEXT (q))[0] = NULL_CHAR;
      STRING (q) = z;
    }
  }
}

//! @brief Whether u is bold tag v, independent of stropping regime.

BOOL_T is_bold (char *u, char *v)
{
  unt len = (unt) strlen (v);
  if (OPTION_STROPPING (&A68_JOB) == QUOTE_STROPPING) {
    if (u[0] == '\'') {
      return (BOOL_T) (strncmp (++u, v, len) == 0 && u[len] == '\'');
    } else {
      return A68_FALSE;
    }
  } else {
    return (BOOL_T) (strncmp (u, v, len) == 0 && !IS_UPPER (u[len]));
  }
}

//! @brief Skip string.

BOOL_T skip_string (LINE_T ** top, char **ch)
{
  LINE_T *u = *top;
  char *v = *ch;
  v++;
  while (u != NO_LINE) {
    while (v[0] != NULL_CHAR) {
      if (v[0] == QUOTE_CHAR && v[1] != QUOTE_CHAR) {
        *top = u;
        *ch = &v[1];
        return A68_TRUE;
      } else if (v[0] == QUOTE_CHAR && v[1] == QUOTE_CHAR) {
        v += 2;
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  return A68_FALSE;
}

//! @brief Skip comment.

BOOL_T skip_comment (LINE_T ** top, char **ch, int delim)
{
  LINE_T *u = *top;
  char *v = *ch;
  v++;
  while (u != NO_LINE) {
    while (v[0] != NULL_CHAR) {
      if (is_bold (v, "COMMENT") && delim == BOLD_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return A68_TRUE;
      } else if (is_bold (v, "CO") && delim == STYLE_I_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return A68_TRUE;
      } else if (v[0] == '#' && delim == STYLE_II_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return A68_TRUE;
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  return A68_FALSE;
}

//! @brief Skip rest of pragmat.

BOOL_T skip_pragmat (LINE_T ** top, char **ch, int delim, BOOL_T whitespace)
{
  LINE_T *u = *top;
  char *v = *ch;
  while (u != NO_LINE) {
    while (v[0] != NULL_CHAR) {
      if (is_bold (v, "PRAGMAT") && delim == BOLD_PRAGMAT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return A68_TRUE;
      } else if (is_bold (v, "PR") && delim == STYLE_I_PRAGMAT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return A68_TRUE;
      } else {
        if (whitespace && !IS_SPACE (v[0]) && v[0] != NEWLINE_CHAR) {
          scan_error (u, v, ERROR_PRAGMENT);
        } else if (IS_UPPER (v[0])) {
// Skip a bold word as you may trigger on REPR, for instance ...
          while (IS_UPPER (v[0])) {
            v++;
          }
        } else {
          v++;
        }
      }
    }
    FORWARD (u);
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  return A68_FALSE;
}

//! @brief Return pointer to next token within pragmat.

char *get_pragmat_item (LINE_T ** top, char **ch)
{
  LINE_T *u = *top;
  char *v = *ch;
  while (u != NO_LINE) {
    while (v[0] != NULL_CHAR) {
      if (!IS_SPACE (v[0]) && v[0] != NEWLINE_CHAR) {
        *top = u;
        *ch = v;
        return v;
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  return NO_TEXT;
}

//! @brief Case insensitive strncmp for at most the number of chars in 'v'.

int streq (char *u, char *v)
{
  int diff;
  for (diff = 0; diff == 0 && u[0] != NULL_CHAR && v[0] != NULL_CHAR; u++, v++) {
    diff = ((int) TO_LOWER (u[0])) - ((int) TO_LOWER (v[0]));
  }
  return diff;
}

//! @brief Scan for next pragmat and yield first pragmat item.

char *next_preprocessor_item (LINE_T ** top, char **ch, int *delim)
{
  LINE_T *u = *top;
  char *v = *ch;
  *delim = 0;
  while (u != NO_LINE) {
    while (v[0] != NULL_CHAR) {
      LINE_T *start_l = u;
      char *start_c = v;
// STRINGs must be skipped.
      if (v[0] == QUOTE_CHAR) {
        SCAN_ERROR (!skip_string (&u, &v), start_l, start_c, ERROR_UNTERMINATED_STRING);
      }
// COMMENTS must be skipped.
      else if (is_bold (v, "COMMENT")) {
        SCAN_ERROR (!skip_comment (&u, &v, BOLD_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (is_bold (v, "CO")) {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_I_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (v[0] == '#') {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_II_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (is_bold (v, "PRAGMAT") || is_bold (v, "PR")) {
// We caught a PRAGMAT.
        char *item;
        if (is_bold (v, "PRAGMAT")) {
          *delim = BOLD_PRAGMAT_SYMBOL;
          v = &v[strlen ("PRAGMAT")];
        } else if (is_bold (v, "PR")) {
          *delim = STYLE_I_PRAGMAT_SYMBOL;
          v = &v[strlen ("PR")];
        }
        item = get_pragmat_item (&u, &v);
        SCAN_ERROR (item == NO_TEXT, start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
// Item "preprocessor" restarts preprocessing if it is off.
        if (A68_PARSER (no_preprocessing) && streq (item, "PREPROCESSOR") == 0) {
          A68_PARSER (no_preprocessing) = A68_FALSE;
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
// If preprocessing is switched off, we idle to closing bracket.
        else if (A68_PARSER (no_preprocessing)) {
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_FALSE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
// Item "nopreprocessor" stops preprocessing if it is on.
        if (streq (item, "NOPREPROCESSOR") == 0) {
          A68_PARSER (no_preprocessing) = A68_TRUE;
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
// Item "INCLUDE" includes a file.
        else if (streq (item, "INCLUDE") == 0) {
          *top = u;
          *ch = v;
          return item;
        }
// Item "READ" includes a file.
        else if (streq (item, "READ") == 0) {
          *top = u;
          *ch = v;
          return item;
        }
// Unrecognised item - probably options handled later by the tokeniser.
        else {
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_FALSE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
      } else if (IS_UPPER (v[0])) {
// Skip a bold word as you may trigger on REPR, for instance ...
        while (IS_UPPER (v[0])) {
          v++;
        }
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  *top = u;
  *ch = v;
  return NO_TEXT;
}

//! @brief Include files.

void include_files (LINE_T * top)
{
// include_files
// 
// syntax: PR read "filename" PR
//         PR include "filename" PR
// 
// The file gets inserted before the line containing the pragmat. In this way
// correct line numbers are preserved which helps diagnostics. A file that has
// been included will not be included a second time - it will be ignored. 
// A rigorous fail-safe, but there is no mechanism to prevent recursive includes 
// in A68 source code. User reports do not indicate sophisticated use of INCLUDE, 
// so this is fine for now.
// TODO - some day we might need `app', analogous to `cpp'.
//
  BOOL_T make_pass = A68_TRUE;
  while (make_pass) {
    LINE_T *s, *t, *u = top;
    char *v = &(STRING (u)[0]);
    make_pass = A68_FALSE;
    errno = 0;
    while (u != NO_LINE) {
      int pr_lim;
      char *item = next_preprocessor_item (&u, &v, &pr_lim);
      LINE_T *start_l = u;
      char *start_c = v;
// Search for PR include "filename" PR.
      if (item != NO_TEXT && (streq (item, "INCLUDE") == 0 || streq (item, "READ") == 0)) {
        FILE_T fd;
        int n, linum, fsize, k, bytes_read, fnwid;
        char *fbuf, delim;
        BUFFER fnb;
        char *fn;
// Skip to filename.
        while (IS_ALPHA (v[0])) {
          v++;
        }
        while (IS_SPACE (v[0])) {
          v++;
        }
// Scan quoted filename.
        SCAN_ERROR ((v[0] != QUOTE_CHAR && v[0] != '\''), start_l, start_c, ERROR_INCORRECT_FILENAME);
        delim = (v++)[0];
        n = 0;
        fnb[0] = NULL_CHAR;
// Scan Algol 68 string (note: "" denotes a ", while in C it concatenates).
        do {
          SCAN_ERROR (EOL (v[0]), start_l, start_c, ERROR_INCORRECT_FILENAME);
          SCAN_ERROR (n == BUFFER_SIZE - 1, start_l, start_c, ERROR_INCORRECT_FILENAME);
          if (v[0] == delim) {
            while (v[0] == delim && v[1] == delim) {
              SCAN_ERROR (n == BUFFER_SIZE - 1, start_l, start_c, ERROR_INCORRECT_FILENAME);
              fnb[n++] = delim;
              fnb[n] = NULL_CHAR;
              v += 2;
            }
          } else if (IS_PRINT (v[0])) {
            fnb[n++] = *(v++);
            fnb[n] = NULL_CHAR;
          } else {
            SCAN_ERROR (A68_TRUE, start_l, start_c, ERROR_INCORRECT_FILENAME);
          }
        } while (v[0] != delim);
// Insist that the pragmat is closed properly.
        v = &v[1];
        SCAN_ERROR (!skip_pragmat (&u, &v, pr_lim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        SCAN_ERROR (n == 0, start_l, start_c, ERROR_INCORRECT_FILENAME);
// Make the name relative to the position of the source file (C preprocessor standard).
        if (FILENAME (u) != NO_TEXT) {
          fn = a68_relpath (a68_dirname (FILENAME (u)), a68_dirname (fnb), a68_basename (fnb));
        } else {
          fn = a68_relpath (FILE_PATH (&A68_JOB), a68_dirname (fnb), a68_basename (fnb));
        }
// Do not check errno, since errno may be undefined here after a successful call.
        if (fn != NO_TEXT) {
          bufcpy (fnb, fn, BUFFER_SIZE);
        } else {
          char err[PATH_MAX + 1];
          bufcpy (err, ERROR_SOURCE_FILE_OPEN, PATH_MAX);
          bufcat (err, " ", PATH_MAX);
          bufcat (err, fnb, PATH_MAX);
          SCAN_ERROR (A68_TRUE, NO_LINE, NO_TEXT, err);
        }
        fnwid = (int) strlen (fnb) + 1;
        fn = (char *) get_fixed_heap_space ((size_t) fnwid);
        bufcpy (fn, fnb, fnwid);
// Ignore the file when included more than once.
        for (t = top; t != NO_LINE; t = NEXT (t)) {
          if (strcmp (FILENAME (t), fn) == 0) {
            goto search_next_pragmat;
          }
        }
        t = NO_LINE;
// Access the file.
        errno = 0;
        fd = open (fn, O_RDONLY | O_BINARY);
        ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "*%s \"%s\"", ERROR_SOURCE_FILE_OPEN, fn) >= 0);
        SCAN_ERROR (fd == -1, start_l, start_c, A68 (edit_line));
        errno = 0;
        fsize = (int) lseek (fd, 0, SEEK_END);
        ASSERT (fsize >= 0);
        SCAN_ERROR (errno != 0, start_l, start_c, ERROR_FILE_READ);
        fbuf = (char *) get_temp_heap_space ((unt) (8 + fsize));
        errno = 0;
        ASSERT (lseek (fd, 0, SEEK_SET) >= 0);
        SCAN_ERROR (errno != 0, start_l, start_c, ERROR_FILE_READ);
        errno = 0;
        bytes_read = (int) io_read (fd, fbuf, (size_t) fsize);
        SCAN_ERROR (errno != 0 || bytes_read != fsize, start_l, start_c, ERROR_FILE_READ);
// Buffer still usable?.
        if (fsize > A68_PARSER (max_scan_buf_length)) {
          A68_PARSER (max_scan_buf_length) = fsize;
          A68_PARSER (scan_buf) = (char *) get_temp_heap_space ((unt) (8 + A68_PARSER (max_scan_buf_length)));
        }
// Link all lines into the list.
        linum = 1;
        s = u;
        t = PREVIOUS (u);
        k = 0;
        if (fsize == 0) {
// If file is empty, insert single empty line.
          A68_PARSER (scan_buf)[0] = NEWLINE_CHAR;
          A68_PARSER (scan_buf)[1] = NULL_CHAR;
          append_source_line (A68_PARSER (scan_buf), &t, &linum, fn);
        } else
          while (k < fsize) {
            n = 0;
            A68_PARSER (scan_buf)[0] = NULL_CHAR;
            while (k < fsize && fbuf[k] != NEWLINE_CHAR) {
              SCAN_ERROR ((IS_CNTRL (fbuf[k]) && !IS_SPACE (fbuf[k])) || fbuf[k] == STOP_CHAR, start_l, start_c, ERROR_FILE_INCLUDE_CTRL);
              A68_PARSER (scan_buf)[n++] = fbuf[k++];
              A68_PARSER (scan_buf)[n] = NULL_CHAR;
            }
            A68_PARSER (scan_buf)[n++] = NEWLINE_CHAR;
            A68_PARSER (scan_buf)[n] = NULL_CHAR;
            if (k < fsize) {
              k++;
            }
            append_source_line (A68_PARSER (scan_buf), &t, &linum, fn);
          }
// Conclude and go find another include directive, if any.
        NEXT (t) = s;
        PREVIOUS (s) = t;
        concatenate_lines (top);
        ASSERT (close (fd) == 0);
        make_pass = A68_TRUE;
      }
    search_next_pragmat:_SKIP_;
    }
  }
}

//! @brief Size of source file.

int get_source_size (void)
{
  FILE_T f = FILE_SOURCE_FD (&A68_JOB);
// This is why WIN32 must open as "read binary".
  return (int) lseek (f, 0, SEEK_END);
}

//! @brief Append environment source lines.

void append_environ (char *str[], LINE_T ** ref_l, int *line_num, char *name)
{
  int k;
  for (k = 0; str[k] != NO_TEXT; k++) {
    int zero_line_num = 0;
    (*line_num)++;
    append_source_line (str[k], ref_l, &zero_line_num, name);
  }
}

//! @brief Read script file and make internal copy.

BOOL_T read_script_file (void)
{
  LINE_T *ref_l = NO_LINE;
  int k, n, num;
  unt len;
  BOOL_T file_end = A68_FALSE;
  BUFFER filename, linenum;
  char ch, *fn, *line;
  char *buffer = (char *) get_temp_heap_space ((unt) (8 + A68_PARSER (source_file_size)));
  FILE_T source = FILE_SOURCE_FD (&A68_JOB);
  ABEND (source == -1, ERROR_ACTION, __func__);
  buffer[0] = NULL_CHAR;
  n = 0;
  len = (unt) (8 + A68_PARSER (source_file_size));
  buffer = (char *) get_temp_heap_space (len);
  ASSERT (lseek (source, 0, SEEK_SET) >= 0);
  while (!file_end) {
// Read the original file name.
    filename[0] = NULL_CHAR;
    k = 0;
    if (io_read (source, &ch, 1) == 0) {
      file_end = A68_TRUE;
      continue;
    }
    while (ch != NEWLINE_CHAR) {
      filename[k++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
    }
    filename[k] = NULL_CHAR;
    fn = TEXT (add_token (&A68 (top_token), filename));
// Read the original file number.
    linenum[0] = NULL_CHAR;
    k = 0;
    ASSERT (io_read (source, &ch, 1) == 1);
    while (ch != NEWLINE_CHAR) {
      linenum[k++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
    }
    linenum[k] = NULL_CHAR;
    num = (int) strtol (linenum, NO_VAR, 10);
    ABEND (errno == ERANGE, ERROR_INTERNAL_CONSISTENCY, __func__);
// COPY original line into buffer.
    ASSERT (io_read (source, &ch, 1) == 1);
    line = &buffer[n];
    while (ch != NEWLINE_CHAR) {
      buffer[n++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
      ABEND ((unt) n >= len, ERROR_ACTION, __func__);
    }
    buffer[n++] = NEWLINE_CHAR;
    buffer[n] = NULL_CHAR;
    append_source_line (line, &ref_l, &num, fn);
  }
  return A68_TRUE;
}

//! @brief Read source file and make internal copy.

BOOL_T read_source_file (void)
{
  LINE_T *ref_l = NO_LINE;
  int line_num = 0, k, bytes_read;
  ssize_t l;
  FILE_T f = FILE_SOURCE_FD (&A68_JOB);
  char **prelude_start, **postlude, *buffer;
// Prelude.
  if (OPTION_STROPPING (&A68_JOB) == UPPER_STROPPING) {
    prelude_start = bold_prelude_start;
    postlude = bold_postlude;
  } else if (OPTION_STROPPING (&A68_JOB) == QUOTE_STROPPING) {
    prelude_start = quote_prelude_start;
    postlude = quote_postlude;
  } else {
    prelude_start = postlude = NO_VAR;
  }
  append_environ (prelude_start, &ref_l, &line_num, "prelude");
// Read the file into a single buffer, so we save on system calls.
  line_num = 1;
  errno = 0;
  buffer = (char *) get_temp_heap_space ((unt) (8 + A68_PARSER (source_file_size)));
  ABEND (errno != 0 || buffer == NO_TEXT, ERROR_ALLOCATION, __func__);
  ASSERT (lseek (f, 0, SEEK_SET) >= 0);
  ABEND (errno != 0, ERROR_ACTION, __func__);
  errno = 0;
  bytes_read = (int) io_read (f, buffer, (size_t) A68_PARSER (source_file_size));
  ABEND (errno != 0 || bytes_read != A68_PARSER (source_file_size), ERROR_ACTION, __func__);
// Link all lines into the list.
  k = 0;
  while (k < A68_PARSER (source_file_size)) {
    l = 0;
    A68_PARSER (scan_buf)[0] = NULL_CHAR;
    while (k < A68_PARSER (source_file_size) && buffer[k] != NEWLINE_CHAR) {
      if (k < A68_PARSER (source_file_size) - 1 && buffer[k] == CR_CHAR && buffer[k + 1] == NEWLINE_CHAR) {
        k++;
      } else {
        A68_PARSER (scan_buf)[l++] = buffer[k++];
        A68_PARSER (scan_buf)[l] = NULL_CHAR;
      }
    }
    A68_PARSER (scan_buf)[l++] = NEWLINE_CHAR;
    A68_PARSER (scan_buf)[l] = NULL_CHAR;
    if (k < A68_PARSER (source_file_size)) {
      k++;
    }
    append_source_line (A68_PARSER (scan_buf), &ref_l, &line_num, FILE_SOURCE_NAME (&A68_JOB));
    SCAN_ERROR (l != (ssize_t) strlen (A68_PARSER (scan_buf)), NO_LINE, NO_TEXT, ERROR_FILE_SOURCE_CTRL);
  }
// Postlude.
  append_environ (postlude, &ref_l, &line_num, "postlude");
// Concatenate lines.
  concatenate_lines (TOP_LINE (&A68_JOB));
// Include files.
  include_files (TOP_LINE (&A68_JOB));
  return A68_TRUE;
}

//! @brief Next_char get next character from internal copy of source file.

char next_char (LINE_T ** ref_l, char **ref_s, BOOL_T allow_typo)
{
  char ch;
#if defined (NO_TYPO)
  allow_typo = A68_FALSE;
#endif
  LOW_STACK_ALERT (NO_NODE);
// Source empty?.
  if (*ref_l == NO_LINE) {
    return STOP_CHAR;
  } else {
    LIST (*ref_l) = (BOOL_T) (OPTION_NODEMASK (&A68_JOB) & SOURCE_MASK ? A68_TRUE : A68_FALSE);
// Take new line?.
    if ((*ref_s)[0] == NEWLINE_CHAR || (*ref_s)[0] == NULL_CHAR) {
      *ref_l = NEXT (*ref_l);
      if (*ref_l == NO_LINE) {
        return STOP_CHAR;
      }
      *ref_s = STRING (*ref_l);
    } else {
      (*ref_s)++;
    }
// Deliver next char.
    ch = (*ref_s)[0];
    if (allow_typo && (IS_SPACE (ch) || ch == FORMFEED_CHAR)) {
      ch = next_char (ref_l, ref_s, allow_typo);
    }
    return ch;
  }
}

//! @brief Find first character that can start a valid symbol.

void get_good_char (char *ref_c, LINE_T ** ref_l, char **ref_s)
{
  while (*ref_c != STOP_CHAR && (IS_SPACE (*ref_c) || (*ref_c == NULL_CHAR))) {
    if (*ref_l != NO_LINE) {
      LIST (*ref_l) = (BOOL_T) (OPTION_NODEMASK (&A68_JOB) & SOURCE_MASK ? A68_TRUE : A68_FALSE);
    }
    *ref_c = next_char (ref_l, ref_s, A68_FALSE);
  }
}

//! @brief Handle a pragment (pragmat or comment).

char *pragment (int type, LINE_T ** ref_l, char **ref_c)
{
#define INIT_BUFFER {chars_in_buf = 0; A68_PARSER (scan_buf)[chars_in_buf] = NULL_CHAR;}
#define ADD_ONE_CHAR(ch) {A68_PARSER (scan_buf)[chars_in_buf ++] = ch; A68_PARSER (scan_buf)[chars_in_buf] = NULL_CHAR;}
  char c = **ref_c, *term_s = NO_TEXT, *start_c = *ref_c;
  char *z = NO_TEXT;
  LINE_T *start_l = *ref_l;
  int term_s_length, chars_in_buf;
  BOOL_T stop, pragmat = A68_FALSE;
// Set terminator.
  if (OPTION_STROPPING (&A68_JOB) == UPPER_STROPPING) {
    if (type == STYLE_I_COMMENT_SYMBOL) {
      term_s = "CO";
    } else if (type == STYLE_II_COMMENT_SYMBOL) {
      term_s = "#";
    } else if (type == BOLD_COMMENT_SYMBOL) {
      term_s = "COMMENT";
    } else if (type == STYLE_I_PRAGMAT_SYMBOL) {
      term_s = "PR";
      pragmat = A68_TRUE;
    } else if (type == BOLD_PRAGMAT_SYMBOL) {
      term_s = "PRAGMAT";
      pragmat = A68_TRUE;
    }
  } else if (OPTION_STROPPING (&A68_JOB) == QUOTE_STROPPING) {
    if (type == STYLE_I_COMMENT_SYMBOL) {
      term_s = "'CO'";
    } else if (type == STYLE_II_COMMENT_SYMBOL) {
      term_s = "#";
    } else if (type == BOLD_COMMENT_SYMBOL) {
      term_s = "'COMMENT'";
    } else if (type == STYLE_I_PRAGMAT_SYMBOL) {
      term_s = "'PR'";
      pragmat = A68_TRUE;
    } else if (type == BOLD_PRAGMAT_SYMBOL) {
      term_s = "'PRAGMAT'";
      pragmat = A68_TRUE;
    }
  }
  term_s_length = (int) strlen (term_s);
// Scan for terminator.
  INIT_BUFFER;
  stop = A68_FALSE;
  while (stop == A68_FALSE) {
    SCAN_ERROR (c == STOP_CHAR, start_l, start_c, ERROR_UNTERMINATED_PRAGMENT);
// A ".." or '..' delimited string in a PRAGMAT.
    if (pragmat && (c == QUOTE_CHAR || (c == '\'' && OPTION_STROPPING (&A68_JOB) == UPPER_STROPPING))) {
      char delim = c;
      BOOL_T eos = A68_FALSE;
      ADD_ONE_CHAR (c);
      c = next_char (ref_l, ref_c, A68_FALSE);
      while (!eos) {
        SCAN_ERROR (EOL (c), start_l, start_c, ERROR_LONG_STRING);
        if (c == delim) {
          ADD_ONE_CHAR (delim);
          save_state (*ref_l, *ref_c, c);
          c = next_char (ref_l, ref_c, A68_FALSE);
          if (c == delim) {
            c = next_char (ref_l, ref_c, A68_FALSE);
          } else {
            restore_state (ref_l, ref_c, &c);
            eos = A68_TRUE;
          }
        } else if (IS_PRINT (c)) {
          ADD_ONE_CHAR (c);
          c = next_char (ref_l, ref_c, A68_FALSE);
        } else {
          unworthy (start_l, start_c, c);
        }
      }
    } else if (EOL (c)) {
      ADD_ONE_CHAR (NEWLINE_CHAR);
    } else if (IS_PRINT (c) || IS_SPACE (c)) {
      ADD_ONE_CHAR (c);
    }
    if (chars_in_buf >= term_s_length) {
// Check whether we encountered the terminator.
      stop = (BOOL_T) (strcmp (term_s, &(A68_PARSER (scan_buf)[chars_in_buf - term_s_length])) == 0);
    }
    c = next_char (ref_l, ref_c, A68_FALSE);
  }
  A68_PARSER (scan_buf)[chars_in_buf - term_s_length] = NULL_CHAR;
  z = new_string (term_s, A68_PARSER (scan_buf), term_s, NO_TEXT);
  if (type == STYLE_I_PRAGMAT_SYMBOL || type == BOLD_PRAGMAT_SYMBOL) {
    isolate_options (A68_PARSER (scan_buf), start_l);
  }
  return z;
#undef ADD_ONE_CHAR
#undef INIT_BUFFER
}

//! @brief Attribute for format item.

int get_format_item (char ch)
{
  switch (TO_LOWER (ch)) {
  case 'a':{
      return FORMAT_ITEM_A;
    }
  case 'b':{
      return FORMAT_ITEM_B;
    }
  case 'c':{
      return FORMAT_ITEM_C;
    }
  case 'd':{
      return FORMAT_ITEM_D;
    }
  case 'e':{
      return FORMAT_ITEM_E;
    }
  case 'f':{
      return FORMAT_ITEM_F;
    }
  case 'g':{
      return FORMAT_ITEM_G;
    }
  case 'h':{
      return FORMAT_ITEM_H;
    }
  case 'i':{
      return FORMAT_ITEM_I;
    }
  case 'j':{
      return FORMAT_ITEM_J;
    }
  case 'k':{
      return FORMAT_ITEM_K;
    }
  case 'l':
  case '/':{
      return FORMAT_ITEM_L;
    }
  case 'm':{
      return FORMAT_ITEM_M;
    }
  case 'n':{
      return FORMAT_ITEM_N;
    }
  case 'o':{
      return FORMAT_ITEM_O;
    }
  case 'p':{
      return FORMAT_ITEM_P;
    }
  case 'q':{
      return FORMAT_ITEM_Q;
    }
  case 'r':{
      return FORMAT_ITEM_R;
    }
  case 's':{
      return FORMAT_ITEM_S;
    }
  case 't':{
      return FORMAT_ITEM_T;
    }
  case 'u':{
      return FORMAT_ITEM_U;
    }
  case 'v':{
      return FORMAT_ITEM_V;
    }
  case 'w':{
      return FORMAT_ITEM_W;
    }
  case 'x':{
      return FORMAT_ITEM_X;
    }
  case 'y':{
      return FORMAT_ITEM_Y;
    }
  case 'z':{
      return FORMAT_ITEM_Z;
    }
  case '+':{
      return FORMAT_ITEM_PLUS;
    }
  case '-':{
      return FORMAT_ITEM_MINUS;
    }
  case POINT_CHAR:{
      return FORMAT_ITEM_POINT;
    }
  case '%':{
      return FORMAT_ITEM_ESCAPE;
    }
  default:{
      return 0;
    }
  }
}

//! @brief Whether input shows exponent character.

BOOL_T is_exp_char (LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  char exp_syms[3];
  if (OPTION_STROPPING (&A68_JOB) == UPPER_STROPPING) {
    exp_syms[0] = EXPONENT_CHAR;
    exp_syms[1] = (char) TO_UPPER (EXPONENT_CHAR);
    exp_syms[2] = NULL_CHAR;
  } else {
    exp_syms[0] = (char) TO_UPPER (EXPONENT_CHAR);
    exp_syms[1] = BACKSLASH_CHAR;
    exp_syms[2] = NULL_CHAR;
  }
  save_state (*ref_l, *ref_s, *ch);
  if (strchr (exp_syms, *ch) != NO_TEXT) {
    *ch = next_char (ref_l, ref_s, A68_TRUE);
    ret = (BOOL_T) (strchr ("+-0123456789", *ch) != NO_TEXT);
  }
  restore_state (ref_l, ref_s, ch);
  return ret;
}

//! @brief Whether input shows radix character.

BOOL_T is_radix_char (LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  save_state (*ref_l, *ref_s, *ch);
  if (OPTION_STROPPING (&A68_JOB) == QUOTE_STROPPING) {
    if (*ch == TO_UPPER (RADIX_CHAR)) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("0123456789ABCDEF", *ch) != NO_TEXT);
    }
  } else {
    if (*ch == RADIX_CHAR) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("0123456789abcdef", *ch) != NO_TEXT);
    }
  }
  restore_state (ref_l, ref_s, ch);
  return ret;
}

//! @brief Whether input shows decimal point.

BOOL_T is_decimal_point (LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  save_state (*ref_l, *ref_s, *ch);
  if (*ch == POINT_CHAR) {
    char exp_syms[3];
    if (OPTION_STROPPING (&A68_JOB) == UPPER_STROPPING) {
      exp_syms[0] = EXPONENT_CHAR;
      exp_syms[1] = (char) TO_UPPER (EXPONENT_CHAR);
      exp_syms[2] = NULL_CHAR;
    } else {
      exp_syms[0] = (char) TO_UPPER (EXPONENT_CHAR);
      exp_syms[1] = BACKSLASH_CHAR;
      exp_syms[2] = NULL_CHAR;
    }
    *ch = next_char (ref_l, ref_s, A68_TRUE);
    if (strchr (exp_syms, *ch) != NO_TEXT) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("+-0123456789", *ch) != NO_TEXT);
    } else {
      ret = (BOOL_T) (strchr ("0123456789", *ch) != NO_TEXT);
    }
  }
  restore_state (ref_l, ref_s, ch);
  return ret;
}

//! @brief Get next token from internal copy of source file..

void get_next_token (BOOL_T in_format, LINE_T ** ref_l, char **ref_s, LINE_T ** start_l, char **start_c, int *att)
{
  char c = **ref_s, *sym = A68_PARSER (scan_buf);
  sym[0] = NULL_CHAR;
  get_good_char (&c, ref_l, ref_s);
  *start_l = *ref_l;
  *start_c = *ref_s;
  if (c == STOP_CHAR) {
// We are at EOF.
    (sym++)[0] = STOP_CHAR;
    sym[0] = NULL_CHAR;
    return;
  }
// In a format.
  if (in_format) {
    char *format_items;
    if (OPTION_STROPPING (&A68_JOB) == UPPER_STROPPING) {
      format_items = "/%\\+-.abcdefghijklmnopqrstuvwxyz";
    } else if (OPTION_STROPPING (&A68_JOB) == QUOTE_STROPPING) {
      format_items = "/%\\+-.ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    } else {
      format_items = "/%\\+-.abcdefghijklmnopqrstuvwxyz";
    }
    if (strchr (format_items, c) != NO_TEXT) {
// General format items.
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      *att = get_format_item (c);
      (void) next_char (ref_l, ref_s, A68_FALSE);
      return;
    }
    if (IS_DIGIT (c)) {
// INT denotation for static replicator.
      SCAN_DIGITS (c);
      sym[0] = NULL_CHAR;
      *att = STATIC_REPLICATOR;
      return;
    }
  }
// Not in a format.
  if (IS_UPPER (c)) {
    if (OPTION_STROPPING (&A68_JOB) == UPPER_STROPPING) {
// Upper case word - bold tag.
      while (IS_UPPER (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
      sym[0] = NULL_CHAR;
      *att = BOLD_TAG;
    } else if (OPTION_STROPPING (&A68_JOB) == QUOTE_STROPPING) {
      while (IS_UPPER (c) || IS_DIGIT (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_TRUE);
      }
      sym[0] = NULL_CHAR;
      *att = IDENTIFIER;
    }
  } else if (c == '\'') {
// Quote, uppercase word, quote - bold tag.
    int k = 0;
    c = next_char (ref_l, ref_s, A68_FALSE);
    while (IS_UPPER (c) || IS_DIGIT (c) || c == '_') {
      (sym++)[0] = c;
      k++;
      c = next_char (ref_l, ref_s, A68_TRUE);
    }
    SCAN_ERROR (k == 0, *start_l, *start_c, ERROR_QUOTED_BOLD_TAG);
    sym[0] = NULL_CHAR;
    *att = BOLD_TAG;
// Skip terminating quote, or complain if it is not there.
    SCAN_ERROR (c != '\'', *start_l, *start_c, ERROR_QUOTED_BOLD_TAG);
    c = next_char (ref_l, ref_s, A68_FALSE);
  } else if (IS_LOWER (c)) {
// Lower case word - identifier.
    while (IS_LOWER (c) || IS_DIGIT (c) || c == '_') {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_TRUE);
    }
    sym[0] = NULL_CHAR;
    *att = IDENTIFIER;
  } else if (c == POINT_CHAR) {
// Begins with a point symbol - point, dotdot, L REAL denotation.
    if (is_decimal_point (ref_l, ref_s, &c)) {
      (sym++)[0] = '0';
      (sym++)[0] = POINT_CHAR;
      c = next_char (ref_l, ref_s, A68_TRUE);
      SCAN_DIGITS (c);
      if (is_exp_char (ref_l, ref_s, &c)) {
        SCAN_EXPONENT_PART (c);
      }
      sym[0] = NULL_CHAR;
      *att = REAL_DENOTATION;
    } else {
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (c == POINT_CHAR) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = POINT_CHAR;
        sym[0] = NULL_CHAR;
        *att = DOTDOT_SYMBOL;
        c = next_char (ref_l, ref_s, A68_FALSE);
      } else {
        (sym++)[0] = POINT_CHAR;
        sym[0] = NULL_CHAR;
        *att = POINT_SYMBOL;
      }
    }
  } else if (IS_DIGIT (c)) {
// Something that begins with a digit - L INT denotation, L REAL denotation.
    SCAN_DIGITS (c);
    if (is_decimal_point (ref_l, ref_s, &c)) {
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (is_exp_char (ref_l, ref_s, &c)) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = '0';
        SCAN_EXPONENT_PART (c);
        *att = REAL_DENOTATION;
      } else {
        (sym++)[0] = POINT_CHAR;
        SCAN_DIGITS (c);
        if (is_exp_char (ref_l, ref_s, &c)) {
          SCAN_EXPONENT_PART (c);
        }
        *att = REAL_DENOTATION;
      }
    } else if (is_exp_char (ref_l, ref_s, &c)) {
      SCAN_EXPONENT_PART (c);
      *att = REAL_DENOTATION;
    } else if (is_radix_char (ref_l, ref_s, &c)) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (OPTION_STROPPING (&A68_JOB) == UPPER_STROPPING) {
        while (IS_DIGIT (c) || strchr ("abcdef", c) != NO_TEXT) {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_TRUE);
        }
      } else {
        while (IS_DIGIT (c) || strchr ("ABCDEF", c) != NO_TEXT) {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_TRUE);
        }
      }
      *att = BITS_DENOTATION;
    } else {
      *att = INT_DENOTATION;
    }
    sym[0] = NULL_CHAR;
  } else if (c == QUOTE_CHAR) {
// STRING denotation.
    BOOL_T stop = A68_FALSE;
    while (!stop) {
      c = next_char (ref_l, ref_s, A68_FALSE);
      while (c != QUOTE_CHAR && c != STOP_CHAR) {
        SCAN_ERROR (EOL (c), *start_l, *start_c, ERROR_LONG_STRING);
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
      SCAN_ERROR (*ref_l == NO_LINE, *start_l, *start_c, ERROR_UNTERMINATED_STRING);
      c = next_char (ref_l, ref_s, A68_FALSE);
      if (c == QUOTE_CHAR) {
        (sym++)[0] = QUOTE_CHAR;
      } else {
        stop = A68_TRUE;
      }
    }
    sym[0] = NULL_CHAR;
    *att = (in_format ? LITERAL : ROW_CHAR_DENOTATION);
  } else if (strchr ("#$()[]{},;@", c) != NO_TEXT) {
// Single character symbols.
    (sym++)[0] = c;
    (void) next_char (ref_l, ref_s, A68_FALSE);
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '|') {
// Bar.
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == ':') {
      (sym++)[0] = c;
      (void) next_char (ref_l, ref_s, A68_FALSE);
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '!' && OPTION_STROPPING (&A68_JOB) == QUOTE_STROPPING) {
// Bar, will be replaced with modern variant.
// For this reason ! is not a MONAD with quote-stropping.
    (sym++)[0] = '|';
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == ':') {
      (sym++)[0] = c;
      (void) next_char (ref_l, ref_s, A68_FALSE);
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == ':') {
// Colon, semicolon, IS, ISNT.
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == '=') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == ':') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
    } else if (c == '/') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == '=') {
        (sym++)[0] = c;
        if ((c = next_char (ref_l, ref_s, A68_FALSE)) == ':') {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == '=') {
        (sym++)[0] = c;
      }
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '=') {
// Operator starting with "=".
    char *scanned = sym;
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (strchr (NOMADS, c) != NO_TEXT) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_FALSE);
    }
    if (c == '=') {
      (sym++)[0] = c;
      if (next_char (ref_l, ref_s, A68_FALSE) == ':') {
        (sym++)[0] = ':';
        c = next_char (ref_l, ref_s, A68_FALSE);
        if (strlen (sym) < 4 && c == '=') {
          (sym++)[0] = '=';
          (void) next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      if (next_char (ref_l, ref_s, A68_FALSE) == '=') {
        (sym++)[0] = '=';
        (void) next_char (ref_l, ref_s, A68_FALSE);
      } else {
        SCAN_ERROR (!(strcmp (scanned, "=:") == 0 || strcmp (scanned, "==:") == 0), *start_l, *start_c, ERROR_INVALID_OPERATOR_TAG);
      }
    }
    sym[0] = NULL_CHAR;
    if (strcmp (scanned, "=") == 0) {
      *att = EQUALS_SYMBOL;
    } else {
      *att = OPERATOR;
    }
  } else if (strchr (MONADS, c) != NO_TEXT || strchr (NOMADS, c) != NO_TEXT) {
// Operator.
    char *scanned = sym;
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (strchr (NOMADS, c) != NO_TEXT) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_FALSE);
    }
    if (c == '=') {
      (sym++)[0] = c;
      if (next_char (ref_l, ref_s, A68_FALSE) == ':') {
        (sym++)[0] = ':';
        c = next_char (ref_l, ref_s, A68_FALSE);
        if (strlen (scanned) < 4 && c == '=') {
          (sym++)[0] = '=';
          (void) next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      if (next_char (ref_l, ref_s, A68_FALSE) == '=') {
        (sym++)[0] = '=';
        sym[0] = NULL_CHAR;
        (void) next_char (ref_l, ref_s, A68_FALSE);
      } else {
        SCAN_ERROR (strcmp (&(scanned[1]), "=:") != 0, *start_l, *start_c, ERROR_INVALID_OPERATOR_TAG);
      }
    }
    sym[0] = NULL_CHAR;
    *att = OPERATOR;
  } else {
// Afuuus ... strange characters!.
    unworthy (*start_l, *start_c, (int) c);
  }
}

//! @brief Whether att opens an embedded clause.

BOOL_T open_nested_clause (int att)
{
  switch (att) {
  case OPEN_SYMBOL:
  case BEGIN_SYMBOL:
  case PAR_SYMBOL:
  case IF_SYMBOL:
  case CASE_SYMBOL:
  case FOR_SYMBOL:
  case FROM_SYMBOL:
  case BY_SYMBOL:
  case TO_SYMBOL:
  case DOWNTO_SYMBOL:
  case WHILE_SYMBOL:
  case DO_SYMBOL:
  case SUB_SYMBOL:
  case ACCO_SYMBOL:
    {
      return A68_TRUE;
    }
  }
  return A68_FALSE;
}

//! @brief Whether att closes an embedded clause.

BOOL_T close_nested_clause (int att)
{
  switch (att) {
  case CLOSE_SYMBOL:
  case END_SYMBOL:
  case FI_SYMBOL:
  case ESAC_SYMBOL:
  case OD_SYMBOL:
  case BUS_SYMBOL:
  case OCCA_SYMBOL:
    {
      return A68_TRUE;
    }
  }
  return A68_FALSE;
}

//! @brief Cast a string to lower case.

void make_lower_case (char *p)
{
  for (; p != NO_TEXT && p[0] != NULL_CHAR; p++) {
    p[0] = (char) TO_LOWER (p[0]);
  }
}

//! @brief Construct a linear list of tokens.

void tokenise_source (NODE_T ** root, int level, BOOL_T in_format, LINE_T ** l, char **s, LINE_T ** start_l, char **start_c)
{
  char *lpr = NO_TEXT;
  int lprt = 0;
  while (l != NO_VAR && !A68_PARSER (stop_scanner)) {
    int att = 0;
    get_next_token (in_format, l, s, start_l, start_c, &att);
    if (A68_PARSER (scan_buf)[0] == STOP_CHAR) {
      A68_PARSER (stop_scanner) = A68_TRUE;
    } else if (strlen (A68_PARSER (scan_buf)) > 0 || att == ROW_CHAR_DENOTATION || att == LITERAL) {
      KEYWORD_T *kw;
      char *c = NO_TEXT;
      BOOL_T make_node = A68_TRUE;
      char *trailing = NO_TEXT;
      if (att != IDENTIFIER) {
        kw = find_keyword (A68 (top_keyword), A68_PARSER (scan_buf));
      } else {
        kw = NO_KEYWORD;
      }
      if (!(kw != NO_KEYWORD && att != ROW_CHAR_DENOTATION)) {
        if (att == IDENTIFIER) {
          make_lower_case (A68_PARSER (scan_buf));
        }
        if (att != ROW_CHAR_DENOTATION && att != LITERAL) {
          int len = (int) strlen (A68_PARSER (scan_buf));
          while (len >= 1 && A68_PARSER (scan_buf)[len - 1] == '_') {
            trailing = "_";
            A68_PARSER (scan_buf)[len - 1] = NULL_CHAR;
            len--;
          }
        }
        c = TEXT (add_token (&A68 (top_token), A68_PARSER (scan_buf)));
      } else {
        if (IS (kw, TO_SYMBOL)) {
// Merge GO and TO to GOTO.
          if (*root != NO_NODE && IS (*root, GO_SYMBOL)) {
            ATTRIBUTE (*root) = GOTO_SYMBOL;
            NSYMBOL (*root) = TEXT (find_keyword (A68 (top_keyword), "GOTO"));
            make_node = A68_FALSE;
          } else {
            att = ATTRIBUTE (kw);
            c = TEXT (kw);
          }
        } else {
          if (att == 0 || att == BOLD_TAG) {
            att = ATTRIBUTE (kw);
          }
          c = TEXT (kw);
// Handle pragments.
          if (att == STYLE_II_COMMENT_SYMBOL || att == STYLE_I_COMMENT_SYMBOL || att == BOLD_COMMENT_SYMBOL) {
            char *nlpr = pragment (ATTRIBUTE (kw), l, s);
            if (lpr == NO_TEXT || (int) strlen (lpr) == 0) {
              lpr = nlpr;
            } else {
              char *stale = lpr;
              lpr = new_string (lpr, "\n\n", nlpr, NO_TEXT);
              a68_free (nlpr);
              a68_free (stale);
            }
            lprt = att;
            make_node = A68_FALSE;
          } else if (att == STYLE_I_PRAGMAT_SYMBOL || att == BOLD_PRAGMAT_SYMBOL) {
            char *nlpr = pragment (ATTRIBUTE (kw), l, s);
            if (lpr == NO_TEXT || (int) strlen (lpr) == 0) {
              lpr = nlpr;
            } else {
              char *stale = lpr;
              lpr = new_string (lpr, "\n\n", nlpr, NO_TEXT);
              a68_free (nlpr);
              a68_free (stale);
            }
            lprt = att;
            if (!A68_PARSER (stop_scanner)) {
              (void) set_options (OPTION_LIST (&A68_JOB), A68_FALSE);
              make_node = A68_FALSE;
            }
          }
        }
      }
// Add token to the tree.
      if (make_node) {
        NODE_T *q = new_node ();
        INFO (q) = new_node_info ();
        switch (att) {
        case ASSIGN_SYMBOL:
        case END_SYMBOL:
        case ESAC_SYMBOL:
        case OD_SYMBOL:
        case OF_SYMBOL:
        case FI_SYMBOL:
        case CLOSE_SYMBOL:
        case BUS_SYMBOL:
        case COLON_SYMBOL:
        case COMMA_SYMBOL:
        case DOTDOT_SYMBOL:
        case SEMI_SYMBOL:
          {
            GINFO (q) = NO_GINFO;
            break;
          }
        default:{
            GINFO (q) = new_genie_info ();
            break;
          }
        }
        STATUS (q) = OPTION_NODEMASK (&A68_JOB);
        LINE (INFO (q)) = *start_l;
        CHAR_IN_LINE (INFO (q)) = *start_c;
        PRIO (INFO (q)) = 0;
        PROCEDURE_LEVEL (INFO (q)) = 0;
        ATTRIBUTE (q) = att;
        NSYMBOL (q) = c;
        PREVIOUS (q) = *root;
        SUB (q) = NEXT (q) = NO_NODE;
        TABLE (q) = NO_TABLE;
        MOID (q) = NO_MOID;
        TAX (q) = NO_TAG;
        if (lpr != NO_TEXT) {
          NPRAGMENT (q) = lpr;
          NPRAGMENT_TYPE (q) = lprt;
          lpr = NO_TEXT;
          lprt = 0;
        }
        if (*root != NO_NODE) {
          NEXT (*root) = q;
        }
        if (TOP_NODE (&A68_JOB) == NO_NODE) {
          TOP_NODE (&A68_JOB) = q;
        }
        *root = q;
        if (trailing != NO_TEXT) {
          diagnostic (A68_WARNING | A68_FORCE_DIAGNOSTICS, q, WARNING_TRAILING, trailing, att);
        }
      }
// Redirection in tokenising formats. The scanner is a recursive-descent type as
// to know when it scans a format text and when not. 
      if (in_format && att == FORMAT_DELIMITER_SYMBOL) {
        return;
      } else if (!in_format && att == FORMAT_DELIMITER_SYMBOL) {
        tokenise_source (root, level + 1, A68_TRUE, l, s, start_l, start_c);
      } else if (in_format && open_nested_clause (att)) {
        NODE_T *z = PREVIOUS (*root);
        if (z != NO_NODE && is_one_of (z, FORMAT_ITEM_N, FORMAT_ITEM_G, FORMAT_ITEM_H, FORMAT_ITEM_F, STOP)) {
          tokenise_source (root, level, A68_FALSE, l, s, start_l, start_c);
        } else if (att == OPEN_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (OPTION_BRACKETS (&A68_JOB) && att == SUB_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (OPTION_BRACKETS (&A68_JOB) && att == ACCO_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        }
      } else if (!in_format && level > 0 && open_nested_clause (att)) {
        tokenise_source (root, level + 1, A68_FALSE, l, s, start_l, start_c);
      } else if (!in_format && level > 0 && close_nested_clause (att)) {
        return;
      } else if (in_format && att == CLOSE_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (OPTION_BRACKETS (&A68_JOB) && in_format && att == BUS_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (OPTION_BRACKETS (&A68_JOB) && in_format && att == OCCA_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      }
    }
  }
}

//! @brief Tokenise source file, build initial syntax tree.

BOOL_T lexical_analyser (void)
{
  LINE_T *l = NO_LINE, *start_l = NO_LINE;
  char *s = NO_TEXT, *start_c = NO_TEXT;
  NODE_T *root = NO_NODE;
  A68_PARSER (scan_buf) = NO_TEXT;
  A68_PARSER (max_scan_buf_length) = A68_PARSER (source_file_size) = get_source_size ();
// Errors in file?.
  if (A68_PARSER (max_scan_buf_length) == 0) {
    return A68_FALSE;
  }
  if (OPTION_RUN_SCRIPT (&A68_JOB)) {
    A68_PARSER (scan_buf) = (char *) get_temp_heap_space ((unt) (8 + A68_PARSER (max_scan_buf_length)));
    if (!read_script_file ()) {
      return A68_FALSE;
    }
  } else {
    A68_PARSER (max_scan_buf_length) += KILOBYTE;       // for the environ, more than enough
    A68_PARSER (scan_buf) = (char *) get_temp_heap_space ((unt) A68_PARSER (max_scan_buf_length));
// Errors in file?.
    if (!read_source_file ()) {
      return A68_FALSE;
    }
  }
// Start tokenising.
  A68_PARSER (read_error) = A68_FALSE;
  A68_PARSER (stop_scanner) = A68_FALSE;
  if ((l = TOP_LINE (&A68_JOB)) != NO_LINE) {
    s = STRING (l);
  }
  tokenise_source (&root, 0, A68_FALSE, &l, &s, &start_l, &start_c);
  return A68_TRUE;
}
