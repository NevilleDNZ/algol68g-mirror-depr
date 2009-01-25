/*!
\file diagnostics.c
\brief error handling routines
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#if defined ENABLE_CURSES
#include <curses.h>
#endif

#define TABULATE(n) (8 * (n / 8 + 1) - n)

/*!
\brief whether unprintable control character
\param ch character under test
\return same
**/

BOOL_T unprintable (char ch)
{
  return (!IS_PRINT (ch) && ch != TAB_CHAR);
}

/*!
\brief format for printing control character
\param ch control character
\return string containing formatted character
**/

char *ctrl_char (int ch)
{
  static char str[SMALL_BUFFER_SIZE];
  ch = TO_UCHAR (ch);
  if (IS_CNTRL (ch) && IS_LOWER (ch + 96)) {
    snprintf (str, SMALL_BUFFER_SIZE, "\\^%c", ch + 96);
  } else {
    snprintf (str, SMALL_BUFFER_SIZE, "\\%02x", ch);
  }
  return (str);
}

/*!
\brief widen single char to string
\param ch character
\return (short) string
**/

static char *char_to_str (char ch)
{
  static char str[2];
  str[0] = ch;
  str[1] = NULL_CHAR;
  return (str);
}

/*!
\brief pretty-print diagnostic 
\param f file number
\param p text
**/

static void pretty_diag (FILE_T f, char *p)
{
  int pos = 1, line_width = (f == STDOUT_FILENO ? term_width : MAX_LINE_WIDTH);
  while (p[0] != NULL_CHAR) {
    char *q;
    int k;
/* Count the number of characters in token to print. */
    if (IS_GRAPH (p[0])) {
      for (k = 0, q = p; q[0] != BLANK_CHAR && q[0] != NULL_CHAR && k <= line_width; q++, k++) {
        ;
      }
    } else {
      k = 1;
    }
/* Now see if there is space for the token. */
    if (k > line_width) {
      k = 1;
    }
    if ((pos + k) >= line_width) {
      WRITE (f, NEWLINE_STRING);
      pos = 1;
    }
    for (; k > 0; k--, p++, pos++) {
      WRITE (f, char_to_str (p[0]));
    }
  }
  for (; p[0] == BLANK_CHAR; p++, pos++) {
    WRITE (f, char_to_str (p[0]));
  }
}

/*!
\brief abnormal end
\param reason why abend
\param info additional info
\param file name of source file where abend
\param line line in source file where abend
**/

void abend (char *reason, char *info, char *file, int line)
{
  snprintf (output_line, BUFFER_SIZE, "%s: abnormal end: %s: %d: %s", a68g_cmd_name, file, line, reason);
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
\param p source line 
\param q node pertaining to "p"
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
\param a source line
\param d diagnostic
\return pointer to character to mark
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
\param f file number
\param p source line
\param where node where to mark
\param diag whether and how to print diagnostics
**/

void write_source_line (FILE_T f, SOURCE_LINE_T * p, NODE_T * where, int diag)
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
  } else {
    WRITE (f, NEWLINE_STRING);
  }
  if (NUMBER (p) == 0) {
    snprintf (output_line, BUFFER_SIZE, "      ");
  } else {
    snprintf (output_line, BUFFER_SIZE, "%-5d ", NUMBER (p) % 100000);
  }
  WRITE (f, output_line);
/* Pretty print line */
  c = c0 = p->string;
  col = 1;
  line_ended = A68_FALSE;
  while (!line_ended) {
    int len = 0;
    char *new_pos = NULL;
    if (c[0] == NULL_CHAR) {
      bufcpy (output_line, "", BUFFER_SIZE);
      line_ended = A68_TRUE;
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
      WRITE (f, output_line);
      pos += len;
      c = new_pos;
    } else {
/* First see if there are diagnostics to be printed */
      BOOL_T y = A68_FALSE, z = A68_FALSE;
      DIAGNOSTIC_T *d = p->diagnostics;
      if (d != NULL || where != NULL) {
        char *c1;
        for (c1 = c0; c1 != c; c1++) {
          y |= (where != NULL && p == LINE (where) ? c1 == where_pos (p, where) : A68_FALSE);
          if (diag != A68_NO_DIAGNOSTICS) {
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
        WRITE (f, "\n      ");
        for (c1 = c0; c1 != c; c1++) {
          int k = 0, diags_at_this_pos = 0;
          for (d = p->diagnostics; d != NULL; FORWARD (d)) {
            if (c1 == diag_pos (p, d)) {
              diags_at_this_pos++;
              k = NUMBER (d);
            }
          }
          if (y == A68_TRUE && c1 == where_pos (p, where)) {
            bufcpy (output_line, "-", BUFFER_SIZE);
          } else if (diags_at_this_pos != 0) {
            if (diag == A68_NO_DIAGNOSTICS) {
              bufcpy (output_line, " ", BUFFER_SIZE);
            } else if (diags_at_this_pos == 1) {
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
          WRITE (f, output_line);
        }
      }
/* Resume pretty printing of line */
      if (!line_ended) {
        continuations++;
        snprintf (output_line, BUFFER_SIZE, "\n.%1d   ", continuations);
        WRITE (f, output_line);
        if (continuations >= 9) {
          WRITE (f, "...");
          line_ended = A68_TRUE;
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
        if (diag == A68_RUNTIME_ERROR) {
          if (WHETHER (d, A68_RUNTIME_ERROR)) {
            WRITE (f, NEWLINE_STRING);
            pretty_diag (f, d->text);
          }
        } else {
          WRITE (f, NEWLINE_STRING);
          pretty_diag (f, d->text);
        }
      }
    }
  }
}

/*!
\brief write diagnostics to STDOUT
\param p source line
\param what severity of diagnostics to print
**/

void diagnostics_to_terminal (SOURCE_LINE_T * p, int what)
{
  for (; p != NULL; FORWARD (p)) {
    if (p->diagnostics != NULL) {
      BOOL_T z = A68_FALSE;
      DIAGNOSTIC_T *d = p->diagnostics;
      for (; d != NULL; FORWARD (d)) {
        if (what == A68_ALL_DIAGNOSTICS) {
          z |= (WHETHER (d, A68_WARNING) || WHETHER (d, A68_ERROR) || WHETHER (d, A68_SYNTAX_ERROR) || WHETHER (d, A68_SUPPRESS_SEVERITY));
        } else if (what == A68_RUNTIME_ERROR) {
          z |= (WHETHER (d, A68_RUNTIME_ERROR));
        }
      }
      if (z) {
        write_source_line (STDOUT_FILENO, p, NULL, what);
      }
    }
  }
}

/*!
\brief give an intelligible error and exit
\param u source line
\param v where to mark
\param txt error text
**/

void scan_error (SOURCE_LINE_T * u, char *v, char *txt)
{
  if (errno != 0) {
    diagnostic_line (A68_SUPPRESS_SEVERITY, u, v, txt, ERROR_SPECIFICATION, NULL);
  } else {
    diagnostic_line (A68_SUPPRESS_SEVERITY, u, v, txt, ERROR_UNSPECIFIED, NULL);
  }
  longjmp (a68_prog.exit_compilation, 1);
}

/*
\brief get severity text
\param sev severity
\return same
*/

static char *get_severity (int sev)
{
  switch (sev) {
  case A68_ERROR:
    {
      a68_prog.error_count++;
      return ("error");
    }
  case A68_SYNTAX_ERROR:
    {
      a68_prog.error_count++;
      return ("syntax error");
    }
  case A68_RUNTIME_ERROR:
    {
      a68_prog.error_count++;
      return ("runtime error");
    }
  case A68_WARNING:
    {
      a68_prog.warning_count++;
      return ("warning");
    }
  case A68_SUPPRESS_SEVERITY:
    {
      a68_prog.error_count++;
      return (NULL);
    }
  default:
    {
      return (NULL);
    }
  }
}

/*!
\brief print diagnostic
\param sev severity
\param b diagnostic text
*/

static void write_diagnostic (int sev, char *b)
{
  char st[SMALL_BUFFER_SIZE];
  char *severity = get_severity (sev);
  if (severity == NULL) {
    snprintf (output_line, BUFFER_SIZE, "%s: %s.", a68g_cmd_name, b);
  } else {
    bufcpy (st, get_severity (sev), SMALL_BUFFER_SIZE);
    snprintf (output_line, BUFFER_SIZE, "%s: %s: %s.", a68g_cmd_name, st, b);
  }
  io_close_tty_line ();
  pretty_diag (STDOUT_FILENO, output_line);
}

/*!
\brief add diagnostic to source line
\param line source line
\param pos where to mark
\param sev severity
\param b diagnostic text
*/

static void add_diagnostic (SOURCE_LINE_T * line, char *pos, NODE_T * p, int sev, char *b)
{
/* Add diagnostic and choose GNU style or non-GNU style. */
  DIAGNOSTIC_T *msg = (DIAGNOSTIC_T *) get_heap_space (ALIGNED_SIZE_OF (DIAGNOSTIC_T));
  DIAGNOSTIC_T **ref_msg;
  char a[BUFFER_SIZE], st[SMALL_BUFFER_SIZE], nst[BUFFER_SIZE];
  char *severity = get_severity (sev);
  int k = 1;
  if (line == NULL && p == NULL) {
    return;
  }
  if (in_monitor) {
    monitor_error (b, NULL);
    return;
  }
  nst[0] = NULL_CHAR;
  if (line == NULL && p != NULL) {
    line = LINE (p);
  }
  while (line != NULL && NUMBER (line) == 0) {
    FORWARD (line);
  }
  if (line == NULL) {
    return;
  }
  ref_msg = &(line->diagnostics);
  while (*ref_msg != NULL) {
    ref_msg = &(NEXT (*ref_msg));
    k++;
  }
  if (p != NULL) {
    NODE_T *n;
    n = NEST (p);
    if (n != NULL && SYMBOL (n) != NULL) {
      char *nt = non_terminal_string (edit_line, ATTRIBUTE (n));
      if (nt != NULL) {
        if (NUMBER (LINE (n)) == 0) {
          snprintf (nst, BUFFER_SIZE, "detected in %s", nt);
        } else {
          if (MOID (n) != NULL) {
            if (NUMBER (LINE (n)) == NUMBER (line)) {
              snprintf (nst, BUFFER_SIZE, "detected in %s %s starting at \"%.64s\" in this line", moid_to_string (MOID (n), MOID_ERROR_WIDTH, p), nt, SYMBOL (n));
            } else {
              snprintf (nst, BUFFER_SIZE, "detected in %s %s starting at \"%.64s\" in line %d", moid_to_string (MOID (n), MOID_ERROR_WIDTH, p), nt, SYMBOL (n), NUMBER (LINE (n)));
            }
          } else {
            if (NUMBER (LINE (n)) == NUMBER (line)) {
              snprintf (nst, BUFFER_SIZE, "detected in %s starting at \"%.64s\" in this line", nt, SYMBOL (n));
            } else {
              snprintf (nst, BUFFER_SIZE, "detected in %s starting at \"%.64s\" in line %d", nt, SYMBOL (n), NUMBER (LINE (n)));
            }
          }
        }
      }
    }
  }
  if (severity == NULL) {
    if (line->filename != NULL && strcmp (a68_prog.files.source.name, line->filename) == 0) {
      snprintf (a, BUFFER_SIZE, "%s: %x: %s", a68g_cmd_name, k, b);
    } else if (line->filename != NULL) {
      snprintf (a, BUFFER_SIZE, "%s: %s: %x: %s", a68g_cmd_name, line->filename, k, b);
    } else {
      snprintf (a, BUFFER_SIZE, "%s: %x: %s", a68g_cmd_name, k, b);
    }
  } else {
    bufcpy (st, get_severity (sev), SMALL_BUFFER_SIZE);
    if (line->filename != NULL && strcmp (a68_prog.files.source.name, line->filename) == 0) {
      snprintf (a, BUFFER_SIZE, "%s: %s: %x: %s", a68g_cmd_name, st, k, b);
    } else if (line->filename != NULL) {
      snprintf (a, BUFFER_SIZE, "%s: %s: %s: %x: %s", a68g_cmd_name, line->filename, st, k, b);
    } else {
      snprintf (a, BUFFER_SIZE, "%s: %s: %x: %s", a68g_cmd_name, st, k, b);
    }
  }
  msg = (DIAGNOSTIC_T *) get_heap_space (ALIGNED_SIZE_OF (DIAGNOSTIC_T));
  *ref_msg = msg;
  ATTRIBUTE (msg) = sev;
  if (nst[0] != NULL_CHAR) {
    bufcat (a, " (", BUFFER_SIZE);
    bufcat (a, nst, BUFFER_SIZE);
    bufcat (a, ")", BUFFER_SIZE);
  }
  bufcat (a, ".", BUFFER_SIZE);
  msg->text = new_string (a);
  msg->where = p;
  msg->line = line;
  msg->symbol = pos;
  NUMBER (msg) = k;
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
S quoted symbol
U unquoted string literal
X expected attribute
Z quoted string literal. 
*/

#define COMPOSE_DIAGNOSTIC\
  while (t[0] != NULL_CHAR) {\
    if (t[0] == '#') {\
      extra_syntax = A68_FALSE;\
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
      if (NUMBER (a) == 0) {\
	bufcat (b, "in standard environment", BUFFER_SIZE);\
      } else {\
        if (p != NULL && NUMBER (a) == LINE_NUMBER (p)) {\
          snprintf (d, SMALL_BUFFER_SIZE, "in this line");\
	} else {\
          snprintf (d, SMALL_BUFFER_SIZE, "in line %d", NUMBER (a));\
        }\
	bufcat (b, d, BUFFER_SIZE);\
      }\
    } else if (t[0] == 'M') {\
      moid = va_arg (args, MOID_T *);\
      if (moid == NULL || moid == MODE (ERROR)) {\
	moid = MODE (UNDEFINED);\
      }\
      if (WHETHER (moid, SERIES_MODE)) {\
	if (PACK (moid) != NULL && NEXT (PACK (moid)) == NULL) {\
	  bufcat (b, moid_to_string (MOID (PACK (moid)), MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
	} else {\
	  bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
	}\
      } else {\
	bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
      }\
    } else if (t[0] == 'N') {\
      bufcat (b, "NIL name of mode ", BUFFER_SIZE);\
      moid = va_arg (args, MOID_T *);\
      if (moid != NULL) {\
	bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
      }\
    } else if (t[0] == 'O') {\
      moid = va_arg (args, MOID_T *);\
      if (moid == NULL || moid == MODE (ERROR)) {\
	moid = MODE (UNDEFINED);\
      }\
      if (moid == MODE (VOID)) {\
	bufcat (b, "UNION (VOID, ..)", BUFFER_SIZE);\
      } else if (WHETHER (moid, SERIES_MODE)) {\
	if (PACK (moid) != NULL && NEXT (PACK (moid)) == NULL) {\
	  bufcat (b, moid_to_string (MOID (PACK (moid)), MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
	} else {\
	  bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
	}\
      } else {\
	bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
      }\
    } else if (t[0] == 'S') {\
      if (p != NULL && SYMBOL (p) != NULL) {\
	bufcat (b, "\"", BUFFER_SIZE);\
	bufcat (b, SYMBOL (p), BUFFER_SIZE);\
	bufcat (b, "\"", BUFFER_SIZE);\
      } else {\
	bufcat (b, "symbol", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'U') {\
      char *str = va_arg (args, char *);\
      bufcat (b, str, BUFFER_SIZE);\
    } else if (t[0] == 'X') {\
      int att = va_arg (args, int);\
      char z[BUFFER_SIZE];\
      /* snprintf (z, BUFFER_SIZE, "\"%s\"", find_keyword_from_attribute (top_keyword, att)->text); */\
      non_terminal_string (z, att);\
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
\param p position in tree
\param str message string
\param ... various arguments needed by special symbols in str
**/

void diagnostic_node (int sev, NODE_T * p, char *str, ...)
{
  va_list args;
  MOID_T *moid = NULL;
  char *t = str, b[BUFFER_SIZE];
  BOOL_T force, extra_syntax = A68_TRUE, shortcut = A68_FALSE;
  int err = errno;
  va_start (args, str);
  b[0] = NULL_CHAR;
  force = (sev & A68_FORCE_DIAGNOSTICS) != 0;
  sev &= ~A68_FORCE_DIAGNOSTICS;
/* No warnings? */
  if (!force && sev == A68_WARNING && no_warnings) {
    return;
  }
/* Suppressed? */
  if (sev == A68_ERROR || sev == A68_SYNTAX_ERROR) {
    if (a68_prog.error_count == MAX_ERRORS) {
      bufcpy (b, "further error diagnostics suppressed", BUFFER_SIZE);
      sev = A68_ERROR;
      shortcut = A68_TRUE;
    } else if (a68_prog.error_count > MAX_ERRORS) {
      a68_prog.error_count++;
      return;
    }
  } else if (sev == A68_WARNING) {
    if (a68_prog.warning_count == MAX_ERRORS) {
      bufcpy (b, "further warning diagnostics suppressed", BUFFER_SIZE);
      shortcut = A68_TRUE;
    } else if (a68_prog.warning_count > MAX_ERRORS) {
      a68_prog.warning_count++;
      return;
    }
  }
  if (shortcut == A68_FALSE) {
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
\param p position in tree
\param str message string
\param ... various arguments needed by special symbols in str
**/

void diagnostic_line (int sev, SOURCE_LINE_T * line, char *pos, char *str, ...)
{
  va_list args;
  MOID_T *moid = NULL;
  char *t = str, b[BUFFER_SIZE];
  BOOL_T force, extra_syntax = A68_TRUE, shortcut = A68_FALSE;
  int err = errno;
  NODE_T *p = NULL;
  va_start (args, str);
  b[0] = NULL_CHAR;
  force = (sev & A68_FORCE_DIAGNOSTICS) != 0;
  sev &= ~A68_FORCE_DIAGNOSTICS;
/* No warnings? */
  if (!force && sev == A68_WARNING && no_warnings) {
    return;
  }
/* Suppressed? */
  if (sev == A68_ERROR || sev == A68_SYNTAX_ERROR) {
    if (a68_prog.error_count == MAX_ERRORS) {
      bufcpy (b, "further error diagnostics suppressed", BUFFER_SIZE);
      sev = A68_ERROR;
      shortcut = A68_TRUE;
    } else if (a68_prog.error_count > MAX_ERRORS) {
      a68_prog.error_count++;
      return;
    }
  } else if (sev == A68_WARNING) {
    if (a68_prog.warning_count == MAX_ERRORS) {
      bufcpy (b, "further warning diagnostics suppressed", BUFFER_SIZE);
      shortcut = A68_TRUE;
    } else if (a68_prog.warning_count > MAX_ERRORS) {
      a68_prog.warning_count++;
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
