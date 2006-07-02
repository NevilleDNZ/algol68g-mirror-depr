/*!
\file diagnostics.c
\brief error handling routines
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

#include "algol68g.h"
#include "genie.h"

#ifdef HAVE_CURSES
#include <curses.h>
#endif

#define TABULATE(n) (8 * (n / 8 + 1) - n)

int error_count, warning_count, run_time_error_count;

/*!
\brief whether unprintable control character
**/

BOOL_T unprintable (char ch)
{
  return ((ch < 32 || ch >= 127) && ch != TAB_CHAR);
}

/*!
\brief format for printing control character
\param 
**/

char *ctrl_char (int ch)
{
  static char str[SMALL_BUFFER_SIZE];
  ch = TO_UCHAR (ch);
  if (ch > 0 && ch < 27) {
    snprintf (str, SMALL_BUFFER_SIZE, "\\^%c", ch + 96);
  } else if (ch >= 27 && ch < 128) {
    snprintf (str, SMALL_BUFFER_SIZE, "'%c'", ch);
  } else {
    snprintf (str, SMALL_BUFFER_SIZE, "\\%02x", ch);
  }
  return (str);
}

/*!
\brief widen single char to string
\param 
\param
**/

static char *char_to_str (char ch)
{
  static char str[SMALL_BUFFER_SIZE];
  str[0] = ch;
  str[1] = NULL_CHAR;
  return (str);
}

/*!
\brief pretty-print diagnostic 
\param 
\param
**/

static void pretty_diag (FILE_T f, char *p)
{
  int pos, line_width = (f == STDOUT_FILENO ? term_width : MAX_LINE_WIDTH);
  if (a68c_diags) {
    io_write_string (f, "++++ ");
    pos = 5;
  } else {
    pos = 1;
  }
  while (p[0] != NULL_CHAR) {
    char *q;
    int k;
    if (IS_GRAPH (p[0])) {
      for (k = 0, q = p; q[0] != BLANK_CHAR && q[0] != NULL_CHAR && k <= line_width - 5; q++, k++) {
        ;
      }
    } else {
      k = 1;
    }
    if (k > line_width - 5) {
      k = 1;
    }
    if ((pos + k) >= line_width) {
      if (a68c_diags) {
        io_write_string (f, "\n     ");
        pos = 5;
      } else {
        io_write_string (f, NEWLINE_STRING);
        pos = 1;
      }
    }
    for (; k > 0; k--, p++, pos++) {
      io_write_string (f, char_to_str (p[0]));
    }
  }
  for (; p[0] == BLANK_CHAR; p++, pos++) {
    io_write_string (f, char_to_str (p[0]));
  }
}

/*!
\brief abnormal end
\param 
\param
\param
\param
**/

void abend (char *reason, char *info, char *file, int line)
{
  if (a68c_diags) {
    snprintf (output_line, BUFFER_SIZE, "Abnormal end in line %d of file \"%s\": %s", line, file, reason);
  } else {
    snprintf (output_line, BUFFER_SIZE, "abnormal end: %s: %d: %s", file, line, reason);
  }
  if (info != NULL) {
    bufcat (output_line, ", ", BUFFER_SIZE);
    bufcat (output_line, info, BUFFER_SIZE);
  }
  if (errno != 0) {
    bufcat (output_line, " (", BUFFER_SIZE);
    bufcat (output_line, ERROR_SPECIFICATION, BUFFER_SIZE);
    bufcat (output_line, ")", BUFFER_SIZE);
  }
  io_close_tty_line ();
  pretty_diag (STDOUT_FILENO, output_line);
  a68g_exit (EXIT_FAILURE);
}

/*!
\brief position in line 
\param 
\param
**/

static char *where_pos (SOURCE_LINE_T * p, NODE_T * q)
{
  char *pos;
  if (q != NULL && p == LINE (q)) {
    pos = q->info->char_in_line;
  } else {
    pos = p->string;
  }
  if (pos == NULL) {
    pos = p->string;
  }
  for (; IS_SPACE (pos[0]) && pos[0] != NULL_CHAR; pos++) {
    ;
  }
  if (pos[0] == NULL_CHAR) {
    pos = p->string;
  }
  return (pos);
}

/*!
\brief position in line where diagnostic points at
\param 
\param
**/

static char *diag_pos (SOURCE_LINE_T * p, DIAGNOSTIC_T * d)
{
  char *pos;
  if (d->where != NULL && p == LINE (d->where)) {
    pos = d->where->info->char_in_line;
  } else {
    pos = p->string;
  }
  if (pos == NULL) {
    pos = p->string;
  }
  for (; IS_SPACE (pos[0]) && pos[0] != NULL_CHAR; pos++) {
    ;
  }
  if (pos[0] == NULL_CHAR) {
    pos = p->string;
  }
  return (pos);
}

/*!
\brief write source line to file with diagnostics
\param f
\param p
\param where
**/

void write_source_line (FILE_T f, SOURCE_LINE_T * p, NODE_T * where, BOOL_T diag)
{
  char *c, *c0;
  int continuations = 0;
  int pos = 5, col;
  int line_width = (f == STDOUT_FILENO ? term_width : MAX_LINE_WIDTH);
  BOOL_T line_ended;
/* Terminate properly */
  if ((p->string)[strlen (p->string) - 1] == NEWLINE_CHAR) {
    (p->string)[strlen (p->string) - 1] = NULL_CHAR;
    if ((p->string)[strlen (p->string) - 1] == CR_CHAR) {
      (p->string)[strlen (p->string) - 1] = NULL_CHAR;
    }
  }
/* Print line number */
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
    if (p->number == 0) {
      snprintf (output_line, BUFFER_SIZE, "     ");
    } else {
      snprintf (output_line, BUFFER_SIZE, "%-4d ", p->number % 10000);
    }
  } else {
    if (p->number == 0) {
      snprintf (output_line, BUFFER_SIZE, "\n     ");
    } else {
      snprintf (output_line, BUFFER_SIZE, "\n%-4d ", p->number % 10000);
    }
  }
  io_write_string (f, output_line);
/* Pretty print line */
  c = c0 = p->string;
  col = 1;
  line_ended = A_FALSE;
  while (!line_ended) {
    int len = 0;
    char *new_pos = NULL;
    if (c[0] == NULL_CHAR) {
      bufcpy (output_line, "", BUFFER_SIZE);
      line_ended = A_TRUE;
    } else {
      if (IS_GRAPH (c[0])) {
        char *c1;
        bufcpy (output_line, "", BUFFER_SIZE);
        for (c1 = c; IS_GRAPH (c1[0]) && len <= line_width - 5; c1++, len++) {
          bufcat (output_line, char_to_str (c1[0]), BUFFER_SIZE);
        }
        if (len > line_width - 5) {
          bufcpy (output_line, char_to_str (c[0]), BUFFER_SIZE);
          len = 1;
        }
        new_pos = &c[len];
        col += len;
      } else if (c[0] == TAB_CHAR) {
        int n = TABULATE (col);
        len = n;
        col += n;
        bufcpy (output_line, "", BUFFER_SIZE);
        while (n--) {
          bufcat (output_line, " ", BUFFER_SIZE);
        }
        new_pos = &c[1];
      } else if (unprintable (c[0])) {
        bufcpy (output_line, ctrl_char ((int) c[0]), BUFFER_SIZE);
        len = strlen (output_line);
        new_pos = &c[1];
        col++;
      } else {
        bufcpy (output_line, char_to_str (c[0]), BUFFER_SIZE);
        len = 1;
        new_pos = &c[1];
        col++;
      }
    }
    if (!line_ended && (pos + len) <= line_width) {
/* Still room - print a character */
      io_write_string (f, output_line);
      pos += len;
      c = new_pos;
    } else {
/* First see if there are diagnostics to be printed */
      BOOL_T y = A_FALSE, z = A_FALSE;
      DIAGNOSTIC_T *d = p->diagnostics;
      if (d != NULL || where != NULL) {
        char *c1;
        for (c1 = c0; c1 != c; c1++) {
          y |= (where != NULL && p == LINE (where) ? c1 == where_pos (p, where) : A_FALSE);
          if (diag) {
            for (d = p->diagnostics; d != NULL; FORWARD (d)) {
              z |= (c1 == diag_pos (p, d));
            }
          }
        }
      }
/* If diagnostics are to be printed then print marks */
      if (y || z) {
        DIAGNOSTIC_T *d;
        char *c1;
        int col_2 = 1;
        io_write_string (f, "\n     ");
        for (c1 = c0; c1 != c; c1++) {
          int k = 0, diags_at_this_pos = 0;
          for (d = p->diagnostics; d != NULL; FORWARD (d)) {
            if (c1 == diag_pos (p, d)) {
              diags_at_this_pos++;
              k = d->number;
            }
          }
          if (y == A_TRUE && c1 == where_pos (p, where)) {
            bufcpy (output_line, "^", BUFFER_SIZE);
          } else if (diags_at_this_pos != 0) {
            if (diags_at_this_pos == 1) {
              snprintf (output_line, BUFFER_SIZE, "%c", digit_to_char (k));
            } else {
              bufcpy (output_line, "*", BUFFER_SIZE);
            }
          } else {
            if (unprintable (c1[0])) {
              int n = strlen (ctrl_char (c1[0]));
              col_2 += 1;
              bufcpy (output_line, "", BUFFER_SIZE);
              while (n--) {
                bufcat (output_line, " ", BUFFER_SIZE);
              }
            } else if (c1[0] == TAB_CHAR) {
              int n = TABULATE (col_2);
              col_2 += n;
              bufcpy (output_line, "", BUFFER_SIZE);
              while (n--) {
                bufcat (output_line, " ", BUFFER_SIZE);
              }
            } else {
              bufcpy (output_line, " ", BUFFER_SIZE);
              col_2++;
            }
          }
          io_write_string (f, output_line);
        }
      }
/* Resume pretty printing of line */
      if (!line_ended) {
        continuations++;
        snprintf (output_line, BUFFER_SIZE, "\n.%1d   ", continuations);
        io_write_string (f, output_line);
        if (continuations >= 9) {
          io_write_string (f, "...");
          line_ended = A_TRUE;
        } else {
          c0 = c;
          pos = 5;
          col = 1;
        }
      }
    }
  }                             /* while */
/* Print the diagnostics */
  if (diag) {
    if (p->diagnostics != NULL) {
      DIAGNOSTIC_T *d;
      for (d = p->diagnostics; d != NULL; FORWARD (d)) {
        io_write_string (f, NEWLINE_STRING);
        pretty_diag (f, d->text);
      }
    }
  }
}

/*!
\brief write diagnostics to STDOUT
\param p
\param what
**/

void diagnostics_to_terminal (SOURCE_LINE_T * p, int what)
{
  for (; p != NULL; FORWARD (p)) {
    if (p->diagnostics != NULL) {
      BOOL_T z = A_FALSE;
      DIAGNOSTIC_T *d = p->diagnostics;
      for (; d != NULL; FORWARD (d)) {
        if (what == A_ALL_DIAGNOSTICS) {
          z |= (WHETHER (d, A_WARNING) || WHETHER (d, A_ERROR) || WHETHER (d, A_SYNTAX_ERROR));
        } else if (what == A_RUNTIME_ERROR) {
          z |= (WHETHER (d, A_RUNTIME_ERROR));
        }
      }
      if (z) {
        write_source_line (STDOUT_FILENO, p, NULL, A_TRUE);
      }
    }
  }
}

/*!
\brief give an intelligible error and exit
\param u
\param txt
**/

void scan_error (SOURCE_LINE_T * u, char *v, char *txt)
{
  if (errno != 0) {
    diagnostic_line (A_ERROR, u, v, txt, ERROR_SPECIFICATION, NULL);
  } else {
    diagnostic_line (A_ERROR, u, v, txt, ERROR_UNSPECIFIED, NULL);
  }
  longjmp (exit_compilation, 1);
}

/*
\brief get severity
\param sev
*/

static char *get_severity (int sev)
{
  switch (sev) {
  case A_ERROR:
    {
      error_count++;
      return ("error");
    }
  case A_SYNTAX_ERROR:
    {
      error_count++;
      return ("syntax error");
    }
  case A_RUNTIME_ERROR:
    {
      error_count++;
      return ("runtime error");
    }
  case A_WARNING:
    {
      warning_count++;
      return ("warning");
    }
  default:
    {
      return (NULL);
    }
  }
}

/*!
\brief print diagnostic and choose GNU style or non-GNU style
*/

static void write_diagnostic (int sev, char *b)
{
  char st[SMALL_BUFFER_SIZE];
  bufcpy (st, get_severity (sev), SMALL_BUFFER_SIZE);
  if (gnu_diags) {
    if (a68_prog.files.generic_name != NULL) {
      snprintf (output_line, BUFFER_SIZE, "%s: %s: %s", a68_prog.files.generic_name, st, b);
    } else {
      snprintf (output_line, BUFFER_SIZE, "%s: %s: %s", A68G_NAME, st, b);
    }
  } else {
    st[0] = TO_UPPER (st[0]);
    b[0] = TO_UPPER (b[0]);
    snprintf (output_line, BUFFER_SIZE, "%s. %s.", st, b);
  }
  io_close_tty_line ();
  pretty_diag (STDOUT_FILENO, output_line);
}

/*!
\brief add diagnostic and choose GNU style or non-GNU style
*/

static void add_diagnostic (SOURCE_LINE_T * line, char *pos, NODE_T * p, int sev, char *b)
{
/* Add diagnostic and choose GNU style or non-GNU style. */
  DIAGNOSTIC_T *msg = (DIAGNOSTIC_T *) get_heap_space (SIZE_OF (DIAGNOSTIC_T));
  DIAGNOSTIC_T **ref_msg;
  char a[BUFFER_SIZE], st[SMALL_BUFFER_SIZE], nst[BUFFER_SIZE];
  int k = 1;
  if (line == NULL && p == NULL) {
    return;
  }
  if (in_monitor) {
    monitor_error (b, NULL);
    return;
  }
  bufcpy (st, get_severity (sev), SMALL_BUFFER_SIZE);
  nst[0] = NULL_CHAR;
  if (line == NULL && p != NULL) {
    line = LINE (p);
  }
  while (line != NULL && line->number == 0) {
    line = NEXT (line);
  }
  if (line == NULL) {
    return;
  }
  ref_msg = &(line->diagnostics);
  while (*ref_msg != NULL) {
    ref_msg = &(NEXT (*ref_msg));
    k++;
  }
  if (gnu_diags) {
    snprintf (a, BUFFER_SIZE, "%s: %d: %s: %s (%x)", line->filename, line->number, st, b, k);
  } else {
    if (p != NULL) {
      NODE_T *n;
      n = NEST (p);
      if (n != NULL && SYMBOL (n) != NULL) {
        char *nt = non_terminal_string (edit_line, ATTRIBUTE (n));
        if (nt != NULL) {
          if (NUMBER (LINE (n)) == 0) {
            snprintf (nst, BUFFER_SIZE, "Detected in %s.", nt);
          } else {
            if (MOID (n) != NULL) {
              if (NUMBER (LINE (n)) == NUMBER (line)) {
                snprintf (nst, BUFFER_SIZE, "Detected in %s %s starting at \"%.64s\" in this line.", moid_to_string (MOID (n), MOID_ERROR_WIDTH), nt, SYMBOL (n));
              } else {
                snprintf (nst, BUFFER_SIZE, "Detected in %s %s starting at \"%.64s\" in line %d.", moid_to_string (MOID (n), MOID_ERROR_WIDTH), nt, SYMBOL (n), NUMBER (LINE (n)));
              }
            } else {
              if (NUMBER (LINE (n)) == NUMBER (line)) {
                snprintf (nst, BUFFER_SIZE, "Detected in %s starting at \"%.64s\" in this line.", nt, SYMBOL (n));
              } else {
                snprintf (nst, BUFFER_SIZE, "Detected in %s starting at \"%.64s\" in line %d.", nt, SYMBOL (n), NUMBER (LINE (n)));
              }
            }
          }
        }
      }
    }
    st[0] = TO_UPPER (st[0]);
    b[0] = TO_UPPER (b[0]);
    if (line->filename != NULL && strcmp (a68_prog.files.source.name, line->filename) == 0) {
      snprintf (a, BUFFER_SIZE, "(%x) %s. %s.", k, st, b);
    } else if (line->filename != NULL) {
      snprintf (a, BUFFER_SIZE, "(%x) %s in file \"%s\". %s.", k, st, line->filename, b);
    } else {
      snprintf (a, BUFFER_SIZE, "(%x) %s. %s.", k, st, b);
    }
  }
  msg = (DIAGNOSTIC_T *) get_heap_space (SIZE_OF (DIAGNOSTIC_T));
  *ref_msg = msg;
  ATTRIBUTE (msg) = sev;
  if (nst[0] != NULL_CHAR) {
    bufcat (a, " ", BUFFER_SIZE);
    bufcat (a, nst, BUFFER_SIZE);
  }
  msg->text = new_string (a);
  msg->where = p;
  msg->line = line;
  msg->symbol = pos;
  msg->number = k;
  NEXT (msg) = NULL;
}

/*
Legend for special symbols:
# skip extra syntactical information
@ node = non terminal
A att = non terminal
B kw = keyword
C context
D argument in decimal
E string literal from errno
H char argument
K int argument as 'k', 'M' or 'G'
L line number
M moid - if error mode return without giving a message
N mode - MODE (NIL)
O moid - operand
S symbol
X expected attribute
Z string literal. 
*/

#define COMPOSE_DIAGNOSTIC\
  while (t[0] != NULL_CHAR) {\
    if (t[0] == '#') {\
      extra_syntax = A_FALSE;\
    } else if (t[0] == '@') {\
      char *nt = non_terminal_string (edit_line, ATTRIBUTE (p));\
      if (t != NULL) {\
	bufcat (b, nt, BUFFER_SIZE);\
      } else {\
	bufcat (b, "construct", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'A') {\
      int att = va_arg (args, int);\
      char *nt = non_terminal_string (edit_line, att);\
      if (nt != NULL) {\
	bufcat (b, nt, BUFFER_SIZE);\
      } else {\
	bufcat (b, "construct", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'B') {\
      int att = va_arg (args, int);\
      KEYWORD_T *nt = find_keyword_from_attribute (top_keyword, att);\
      if (nt != NULL) {\
	bufcat (b, "\"", BUFFER_SIZE);\
	bufcat (b, nt->text, BUFFER_SIZE);\
	bufcat (b, "\"", BUFFER_SIZE);\
      } else {\
	bufcat (b, "keyword", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'C') {\
      int att = va_arg (args, int);\
      if (att == NO_SORT) {\
	bufcat (b, "this", BUFFER_SIZE);\
      }\
      if (att == SOFT) {\
	bufcat (b, "a soft", BUFFER_SIZE);\
      } else if (att == WEAK) {\
	bufcat (b, "a weak", BUFFER_SIZE);\
      } else if (att == MEEK) {\
	bufcat (b, "a meek", BUFFER_SIZE);\
      } else if (att == FIRM) {\
	bufcat (b, "a firm", BUFFER_SIZE);\
      } else if (att == STRONG) {\
	bufcat (b, "a strong", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'D') {\
      int a = va_arg (args, int);\
      char d[BUFFER_SIZE];\
      snprintf (d, BUFFER_SIZE, "%d", a);\
      bufcat (b, d, BUFFER_SIZE);\
    } else if (t[0] == 'H') {\
      char *a = va_arg (args, char *);\
      char d[SMALL_BUFFER_SIZE];\
      snprintf (d, SMALL_BUFFER_SIZE, "'%c'", a[0]);\
      bufcat (b, d, BUFFER_SIZE);\
    } else if (t[0] == 'L') {\
      SOURCE_LINE_T *a = va_arg (args, SOURCE_LINE_T *);\
      char d[SMALL_BUFFER_SIZE];\
      ABNORMAL_END (a == NULL, "NULL source line in error", NULL);\
      if (a->number > 0) {\
        if (p != NULL && a->number == LINE (p)->number) {\
          snprintf (d, SMALL_BUFFER_SIZE, "in this line");\
	} else {\
          snprintf (d, SMALL_BUFFER_SIZE, "in line %d", a->number);\
        }\
	bufcat (b, d, BUFFER_SIZE);\
      }\
    } else if (t[0] == 'M') {\
      moid = va_arg (args, MOID_T *);\
      if (moid == MODE (ERROR)) {\
	return;\
      } else if (moid == NULL) {\
	bufcat (b, "\"NULL\"", BUFFER_SIZE);\
      } else if (WHETHER (moid, SERIES_MODE)) {\
	if (PACK (moid) != NULL && NEXT (PACK (moid)) == NULL) {\
	  bufcat (b, moid_to_string (MOID (PACK (moid)), MOID_ERROR_WIDTH), BUFFER_SIZE);\
	} else {\
	  bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH), BUFFER_SIZE);\
	}\
      } else {\
	bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH), BUFFER_SIZE);\
      }\
    } else if (t[0] == 'N') {\
      bufcat (b, "NIL value of mode ", BUFFER_SIZE);\
      moid = va_arg (args, MOID_T *);\
      if (moid != NULL) {\
	bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH), BUFFER_SIZE);\
      }\
    } else if (t[0] == 'O') {\
      moid = va_arg (args, MOID_T *);\
      if (moid == MODE (ERROR)) {\
	return;\
      } else if (moid == NULL) {\
	bufcat (b, "\"NULL\"", BUFFER_SIZE);\
      } else if (moid == MODE (VOID)) {\
	bufcat (b, "UNION (VOID, ..)", BUFFER_SIZE);\
      } else if (WHETHER (moid, SERIES_MODE)) {\
	if (PACK (moid) != NULL && NEXT (PACK (moid)) == NULL) {\
	  bufcat (b, moid_to_string (MOID (PACK (moid)), MOID_ERROR_WIDTH), BUFFER_SIZE);\
	} else {\
	  bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH), BUFFER_SIZE);\
	}\
      } else {\
	bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH), BUFFER_SIZE);\
      }\
    } else if (t[0] == 'S') {\
      if (p != NULL && SYMBOL (p) != NULL) {\
	bufcat (b, "\"", BUFFER_SIZE);\
	bufcat (b, SYMBOL (p), BUFFER_SIZE);\
	bufcat (b, "\"", BUFFER_SIZE);\
      } else {\
	bufcat (b, "symbol", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'X') {\
      int att = va_arg (args, int);\
      char z[BUFFER_SIZE];\
      snprintf (z, BUFFER_SIZE, "\"%s\"", find_keyword_from_attribute (top_keyword, att)->text);\
      bufcat (b, new_string (z), BUFFER_SIZE);\
    } else if (t[0] == 'Y') {\
      char *str = va_arg (args, char *);\
      bufcat (b, str, BUFFER_SIZE);\
    } else if (t[0] == 'Z') {\
      char *str = va_arg (args, char *);\
      bufcat (b, "\"", BUFFER_SIZE);\
      bufcat (b, str, BUFFER_SIZE);\
      bufcat (b, "\"", BUFFER_SIZE);\
    } else {\
      char q[2];\
      q[0] = t[0];\
      q[1] = NULL_CHAR;\
      bufcat (b, q, BUFFER_SIZE);\
    }\
    t++;\
  }

/*!
\brief give a diagnostic message
\param sev severity
\param p position in syntax tree, should not be NULL
\param str message string
\param ... various arguments needed by special symbols in str
**/

void diagnostic_node (int sev, NODE_T * p, char *str, ...)
{
  va_list args;
  MOID_T *moid = NULL;
  char *t = str, b[BUFFER_SIZE];
  BOOL_T force, extra_syntax = A_TRUE, shortcut = A_FALSE;
  int err = errno;
  va_start (args, str);
  b[0] = NULL_CHAR;
  force = (sev & FORCE_DIAGNOSTIC) != 0;
  sev &= ~FORCE_DIAGNOSTIC;
/* No warnings? */
  if (!force && sev == A_WARNING && no_warnings) {
    return;
  }
/* Suppressed? */
  if (sev == A_ERROR || sev == A_SYNTAX_ERROR) {
    if (error_count == MAX_ERRORS) {
      bufcpy (b, "further error diagnostics suppressed", BUFFER_SIZE);
      sev = A_ERROR;
      shortcut = A_TRUE;
    } else if (error_count > MAX_ERRORS) {
      error_count++;
      return;
    }
  } else if (sev == A_WARNING) {
    if (warning_count == MAX_ERRORS) {
      bufcpy (b, "further warning diagnostics suppressed", BUFFER_SIZE);
      shortcut = A_TRUE;
    } else if (warning_count > MAX_ERRORS) {
      warning_count++;
      return;
    }
  }
  if (!shortcut) {
/* Synthesize diagnostic message. */
    COMPOSE_DIAGNOSTIC;
/* Add information from errno, if any. */
    if (err != 0) {
      char *str = new_string (ERROR_SPECIFICATION);
      if (str != NULL) {
        char *stu;
        bufcat (b, " (", BUFFER_SIZE);
        for (stu = str; stu[0] != NULL_CHAR; stu++) {
          stu[0] = TO_LOWER (stu[0]);
        }
        bufcat (b, str, BUFFER_SIZE);
        bufcat (b, ")", BUFFER_SIZE);
      }
    }
  }
/* Construct a diagnostic message. */
  if (p == NULL) {
    write_diagnostic (sev, b);
  } else {
    add_diagnostic (NULL, NULL, p, sev, b);
  }
  va_end (args);
}

/*!
\brief give a diagnostic message
\param sev severity
\param p position in syntax tree, should not be NULL
\param str message string
\param ... various arguments needed by special symbols in str
**/

void diagnostic_line (int sev, SOURCE_LINE_T * line, char *pos, char *str, ...)
{
  va_list args;
  MOID_T *moid = NULL;
  char *t = str, b[BUFFER_SIZE];
  BOOL_T force, extra_syntax = A_TRUE, shortcut = A_FALSE;
  int err = errno;
  NODE_T *p = NULL;
  va_start (args, str);
  b[0] = NULL_CHAR;
  force = (sev & FORCE_DIAGNOSTIC) != 0;
  sev &= ~FORCE_DIAGNOSTIC;
/* No warnings? */
  if (!force && sev == A_WARNING && no_warnings) {
    return;
  }
/* Suppressed? */
  if (sev == A_ERROR || sev == A_SYNTAX_ERROR) {
    if (error_count == MAX_ERRORS) {
      bufcpy (b, "further error diagnostics suppressed", BUFFER_SIZE);
      sev = A_ERROR;
      shortcut = A_TRUE;
    } else if (error_count > MAX_ERRORS) {
      error_count++;
      return;
    }
  } else if (sev == A_WARNING) {
    if (warning_count == MAX_ERRORS) {
      bufcpy (b, "further warning diagnostics suppressed", BUFFER_SIZE);
      shortcut = A_TRUE;
    } else if (warning_count > MAX_ERRORS) {
      warning_count++;
      return;
    }
  }
  if (!shortcut) {
/* Synthesize diagnostic message. */
    COMPOSE_DIAGNOSTIC;
/* Add information from errno, if any. */
    if (err != 0) {
      char *str = new_string (ERROR_SPECIFICATION);
      if (str != NULL) {
        char *stu;
        bufcat (b, " (", BUFFER_SIZE);
        for (stu = str; stu[0] != NULL_CHAR; stu++) {
          stu[0] = TO_LOWER (stu[0]);
        }
        bufcat (b, str, BUFFER_SIZE);
        bufcat (b, ")", BUFFER_SIZE);
      }
    }
  }
/* Construct a diagnostic message. */
  if (line == NULL) {
    write_diagnostic (sev, b);
  } else {
    add_diagnostic (line, pos, NULL, sev, b);
  }
  va_end (args);
}
