//! @file a68g-options.c
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
//! Algol 68 Genie options.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-genie.h"
#include "a68g-options.h"
#include "a68g-parser.h"

// This code options to Algol68G.
// 
// Option syntax does not follow GNU standards.
// 
// Options come from:
//   [1] A rc file (normally .a68grc).
//   [2] The A68G_OPTIONS environment variable overrules [1].
//   [3] Command line options overrule [2].
//   [4] Pragmat items overrule [3]. 

//! @brief Set default core size.

void default_mem_sizes (int n)
{
#define SET_SIZE(m, n) {\
  ABEND (OVER_2G (n), ERROR_OUT_OF_CORE_2G, __func__);\
  (m) = (n);\
}

  if (n < 0) {
    n = 1;
  }
  SET_SIZE (A68 (frame_stack_size), 12 * n * MEGABYTE);
  SET_SIZE (A68 (expr_stack_size), 4 * n * MEGABYTE);
  SET_SIZE (A68 (heap_size), 32 * n * MEGABYTE);
  SET_SIZE (A68 (handle_pool_size), 16 * n * MEGABYTE);
  SET_SIZE (A68 (storage_overhead), MIN_MEM_SIZE);
#undef SET_SIZE
}

//! @brief Read options from the .rc file.

void read_rc_options (void)
{
  FILE *f;
  BUFFER name, new_name;
  ASSERT (snprintf (name, SNPRINTF_SIZE, ".%src", A68 (a68_cmd_name)) >= 0);
  f = a68_fopen (name, "r", new_name);
  if (f != NO_FILE) {
    while (!feof (f)) {
      if (fgets (A68 (input_line), BUFFER_SIZE, f) != NO_TEXT) {
        if (A68 (input_line)[strlen (A68 (input_line)) - 1] == NEWLINE_CHAR) {
          A68 (input_line)[strlen (A68 (input_line)) - 1] = NULL_CHAR;
        }
        isolate_options (A68 (input_line), NO_LINE);
      }
    }
    ASSERT (fclose (f) == 0);
    (void) set_options (OPTION_LIST (&A68_JOB), A68_FALSE);
  } else {
    errno = 0;
  }
}

//! @brief Read options from A68G_OPTIONS.

void read_env_options (void)
{
  if (getenv ("A68G_OPTIONS") != NULL) {
    isolate_options (getenv ("A68G_OPTIONS"), NO_LINE);
    (void) set_options (OPTION_LIST (&A68_JOB), A68_FALSE);
    errno = 0;
  }
}

//! @brief Tokenise string 'p' that holds options.

void isolate_options (char *p, LINE_T * line)
{
  char *q;
// 'q' points at first significant char in item.
  while (p[0] != NULL_CHAR) {
// Skip white space etc.
    while ((p[0] == BLANK_CHAR || p[0] == TAB_CHAR || p[0] == ',' || p[0] == NEWLINE_CHAR) && p[0] != NULL_CHAR) {
      p++;
    }
// ... then tokenise an item.
    if (p[0] != NULL_CHAR) {
// Item can be "string". Note that these are not A68 strings.
      if (p[0] == QUOTE_CHAR || p[0] == '\'' || p[0] == '`') {
        char delim = p[0];
        p++;
        q = p;
        while (p[0] != delim && p[0] != NULL_CHAR) {
          p++;
        }
        if (p[0] != NULL_CHAR) {
          p[0] = NULL_CHAR;     // p[0] was delimiter
          p++;
        } else {
          scan_error (line, NO_TEXT, ERROR_UNTERMINATED_STRING);
        }
      } else {
// Item is not a delimited string.
        q = p;
// Tokenise symbol and gather it in the option list for later processing.
// Skip '='s, we accept if someone writes -prec=60 -heap=8192
        if (*q == '=') {
          p++;
        } else {
// Skip item 
          while (p[0] != BLANK_CHAR && p[0] != NULL_CHAR && p[0] != '=' && p[0] != ',' && p[0] != NEWLINE_CHAR) {
            p++;
          }
        }
        if (p[0] != NULL_CHAR) {
          p[0] = NULL_CHAR;
          p++;
        }
      }
// 'q' points to first significant char in item, and 'p' points after item.
      add_option_list (&(OPTION_LIST (&A68_JOB)), q, line);
    }
  }
}

//! @brief Set default values for options.

void default_options (MODULE_T * p)
{
  OPTION_BACKTRACE (p) = A68_FALSE;
  OPTION_BRACKETS (p) = A68_FALSE;
  OPTION_CHECK_ONLY (p) = A68_FALSE;
  OPTION_CLOCK (p) = A68_FALSE;
  OPTION_COMPILE_CHECK (p) = A68_FALSE;
  OPTION_COMPILE (p) = A68_FALSE;
  OPTION_CROSS_REFERENCE (p) = A68_FALSE;
  OPTION_DEBUG (p) = A68_FALSE;
  OPTION_FOLD (p) = A68_FALSE;
  OPTION_INDENT (p) = 2;
  OPTION_KEEP (p) = A68_FALSE;
  OPTION_LICENSE (p) = A68_FALSE;
  OPTION_MOID_LISTING (p) = A68_FALSE;
  OPTION_NODEMASK (p) = (STATUS_MASK_T) (ASSERT_MASK | SOURCE_MASK);
  OPTION_NO_WARNINGS (p) = A68_FALSE;
  OPTION_OPT_LEVEL (p) = NO_OPTIMISE;
  OPTION_PORTCHECK (p) = A68_FALSE;
  OPTION_PRAGMAT_SEMA (p) = A68_TRUE;
  OPTION_PRETTY (p) = A68_FALSE;
  OPTION_QUIET (p) = A68_FALSE;
  OPTION_REDUCTIONS (p) = A68_FALSE;
  OPTION_REGRESSION_TEST (p) = A68_FALSE;
  OPTION_RERUN (p) = A68_FALSE;
  OPTION_RUN (p) = A68_FALSE;
  OPTION_RUN_SCRIPT (p) = A68_FALSE;
  OPTION_SOURCE_LISTING (p) = A68_FALSE;
  OPTION_STANDARD_PRELUDE_LISTING (p) = A68_FALSE;
  OPTION_STATISTICS_LISTING (p) = A68_FALSE;
  OPTION_STRICT (p) = A68_FALSE;
  OPTION_STROPPING (p) = UPPER_STROPPING;
  OPTION_TIME_LIMIT (p) = 0;
  OPTION_TRACE (p) = A68_FALSE;
  OPTION_TREE_LISTING (p) = A68_FALSE;
  OPTION_UNUSED (p) = A68_FALSE;
  OPTION_VERBOSE (p) = A68_FALSE;
  OPTION_VERSION (p) = A68_FALSE;
  set_long_mp_digits (0);
}

//! @brief Error handler for options.

void option_error (LINE_T * l, char *option, char *info)
{
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s", option) >= 0);
  if (info != NO_TEXT) {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "*error: %s option \"%s\"", info, A68 (output_line)) >= 0);
  } else {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "*error: in option \"%s\"", A68 (output_line)) >= 0);
  }
  scan_error (l, NO_TEXT, A68 (edit_line));
}

//! @brief Strip minus preceeding a string.

char *strip_sign (char *p)
{
  while (p[0] == '-' || p[0] == '+') {
    p++;
  }
  return new_string (p, NO_TEXT);
}

//! @brief Add an option to the list, to be processed later.

void add_option_list (OPTION_LIST_T ** l, char *str, LINE_T * line)
{
  if (*l == NO_OPTION_LIST) {
    *l = (OPTION_LIST_T *) get_heap_space ((size_t) SIZE_ALIGNED (OPTION_LIST_T));
    SCAN (*l) = SOURCE_SCAN (&A68_JOB);
    STR (*l) = new_string (str, NO_TEXT);
    PROCESSED (*l) = A68_FALSE;
    LINE (*l) = line;
    NEXT (*l) = NO_OPTION_LIST;
  } else {
    add_option_list (&(NEXT (*l)), str, line);
  }
}

//! @brief Free an option list.

void free_option_list (OPTION_LIST_T * l)
{
  if (l != NO_OPTION_LIST) {
    free_option_list (NEXT (l));
    a68_free (STR (l));
    a68_free (l);
  }
}

//! @brief Initialise option handler.

void init_options (void)
{
  A68 (options) = (OPTIONS_T *) a68_alloc ((size_t) SIZE_ALIGNED (OPTIONS_T), __func__, __LINE__);
  OPTION_LIST (&A68_JOB) = NO_OPTION_LIST;
}

//! @brief Test equality of p and q, upper case letters in q are mandatory.

static inline BOOL_T eq (char *p, char *q)
{
// Upper case letters in 'q' are mandatory, lower case must match.
  if (OPTION_PRAGMAT_SEMA (&A68_JOB)) {
    return match_string (p, q, '=');
  } else {
    return A68_FALSE;
  }
}

//! @brief Process echoes gathered in the option list.

void prune_echoes (OPTION_LIST_T * i)
{
  while (i != NO_OPTION_LIST) {
    if (SCAN (i) == SOURCE_SCAN (&A68_JOB)) {
      char *p = strip_sign (STR (i));
// ECHO echoes a string.
      if (eq (p, "ECHO")) {
        {
          char *car = strchr (p, '=');
          if (car != NO_TEXT) {
            io_close_tty_line ();
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s", &car[1]) >= 0);
            WRITE (STDOUT_FILENO, A68 (output_line));
          } else {
            FORWARD (i);
            if (i != NO_OPTION_LIST) {
              if (strcmp (STR (i), "=") == 0) {
                FORWARD (i);
              }
              if (i != NO_OPTION_LIST) {
                io_close_tty_line ();
                ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s", STR (i)) >= 0);
                WRITE (STDOUT_FILENO, A68 (output_line));
              }
            }
          }
        }
      }
      a68_free (p);
    }
    if (i != NO_OPTION_LIST) {
      FORWARD (i);
    }
  }
}

//! @brief Translate integral option argument.

int fetch_integral (char *p, OPTION_LIST_T ** i, BOOL_T * error)
{
  LINE_T *start_l = LINE (*i);
  char *start_c = STR (*i);
  char *car = NO_TEXT, *num = NO_TEXT;
  INT_T k, mult = 1;
  *error = A68_FALSE;
// Fetch argument.
  car = strchr (p, '=');
  if (car == NO_TEXT) {
    FORWARD (*i);
    *error = (BOOL_T) (*i == NO_OPTION_LIST);
    if (!error && strcmp (STR (*i), "=") == 0) {
      FORWARD (*i);
      *error = (BOOL_T) (*i == NO_OPTION_LIST);
    }
    if (!*error) {
      num = STR (*i);
    }
  } else {
    num = &car[1];
    *error = (BOOL_T) (num[0] == NULL_CHAR);
  }
// Translate argument into integer.
  if (*error) {
    option_error (start_l, start_c, "integer value required by");
    return 0;
  } else {
    char *suffix;
    errno = 0;
    k = (int) strtol (num, &suffix, 0); // Accept also octal and hex
    *error = (BOOL_T) (suffix == num);
    if (errno != 0 || *error) {
      option_error (start_l, start_c, "conversion error in");
      *error = A68_TRUE;
    } else if (k < 0) {
      option_error (start_l, start_c, "negative value in");
      *error = A68_TRUE;
    } else {
// Accept suffix multipliers: 32k, 64M, 1G.
      if (suffix != NO_TEXT) {
        switch (suffix[0]) {
        case NULL_CHAR:
          {
            mult = 1;
            break;
          }
        case 'k':
        case 'K':
          {
            mult = KILOBYTE;
            break;
          }
        case 'm':
        case 'M':
          {
            mult = MEGABYTE;
            break;
          }
        case 'g':
        case 'G':
          {
            mult = GIGABYTE;
            break;
          }
        default:
          {
            option_error (start_l, start_c, "unknown suffix in");
            *error = A68_TRUE;
            break;
          }
        }
        if (suffix[0] != NULL_CHAR && suffix[1] != NULL_CHAR) {
          option_error (start_l, start_c, "unknown suffix in");
          *error = A68_TRUE;
        }
      }
    }
    if (OVER_2G ((REAL_T) k * (REAL_T) mult)) {
      errno = ERANGE;
      option_error (start_l, start_c, ERROR_OVER_2G);
    }
    return k * mult;
  }
}

//! @brief Process options gathered in the option list.

BOOL_T set_options (OPTION_LIST_T * i, BOOL_T cmd_line)
{
  BOOL_T go_on = A68_TRUE, name_set = A68_FALSE, skip = A68_FALSE;
  OPTION_LIST_T *j = i;
  errno = 0;
  while (i != NO_OPTION_LIST && go_on) {
// Once SCRIPT is processed we skip options on the command line.
    if (cmd_line && skip) {
      FORWARD (i);
    } else {
      LINE_T *start_l = LINE (i);
      char *start_c = STR (i);
      int n = (int) strlen (STR (i));
// Allow for spaces ending in # to have A68 comment syntax with '#!'.
      while (n > 0 && (IS_SPACE ((STR (i))[n - 1]) || (STR (i))[n - 1] == '#')) {
        (STR (i))[--n] = NULL_CHAR;
      }
      if (!(PROCESSED (i))) {
// Accept UNIX '-option [=] value'.
        BOOL_T minus_sign = (BOOL_T) ((STR (i))[0] == '-');
        char *p = strip_sign (STR (i));
        char *stale = p;
        if (!minus_sign && eq (p, "#")) {
          ;
        } else if (!minus_sign && cmd_line) {
// Item without '-'s is a filename.
          if (!name_set) {
            FILE_INITIAL_NAME (&A68_JOB) = new_string (p, NO_TEXT);
            name_set = A68_TRUE;
          } else {
            option_error (NO_LINE, start_c, "multiple source file names at");
          }
        } else if (eq (p, "INCLUDE")) {
// Preprocessor items stop option processing.
          go_on = A68_FALSE;
        } else if (eq (p, "READ")) {
          go_on = A68_FALSE;
        } else if (eq (p, "PREPROCESSOR")) {
          go_on = A68_FALSE;
        } else if (eq (p, "NOPREPROCESSOR")) {
          go_on = A68_FALSE;
        } else if (eq (p, "TECHnicalities")) {
// TECH prints out some tech stuff.
          state_version (STDOUT_FILENO);
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_REF) = %u", (unt) sizeof (A68_REF)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_PROCEDURE) = %u", (unt) sizeof (A68_PROCEDURE)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
#if (A68_LEVEL >= 3)
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (DOUBLE_T) = %u", (unt) sizeof (DOUBLE_T)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (DOUBLE_NUM_T) = %u", (unt) sizeof (DOUBLE_NUM_T)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
#endif
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_INT) = %u", (unt) sizeof (A68_INT)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_REAL) = %u", (unt) sizeof (A68_REAL)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_BOOL) = %u", (unt) sizeof (A68_BOOL)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_CHAR) = %u", (unt) sizeof (A68_CHAR)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_BITS) = %u", (unt) sizeof (A68_BITS)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
#if (A68_LEVEL >= 3)
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_LONG_REAL) = %u", (unt) sizeof (A68_LONG_REAL)) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
#else
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_LONG_REAL) = %u", (unt) size_mp ()) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
#endif
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "sizeof (A68_LONG_LONG_REAL) = %u", (unt) size_long_mp ()) >= 0);
          WRITELN (STDOUT_FILENO, A68 (output_line));
          WRITELN (STDOUT_FILENO, "");
          exit (EXIT_SUCCESS);
        }
// EXIT stops option processing.
        else if (eq (p, "EXIT")) {
          go_on = A68_FALSE;
        }
// Empty item (from specifying '-' or '--') stops option processing.
        else if (eq (p, "")) {
          go_on = A68_FALSE;
        }
// FILE accepts its argument as filename.
        else if (eq (p, "File") && cmd_line) {
          FORWARD (i);
          if (i != NO_OPTION_LIST && strcmp (STR (i), "=") == 0) {
            FORWARD (i);
          }
          if (i != NO_OPTION_LIST) {
            if (!name_set) {
              FILE_INITIAL_NAME (&A68_JOB) = new_string (STR (i), NO_TEXT);
              name_set = A68_TRUE;
            } else {
              option_error (start_l, start_c, "multiple source file names at");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
        }
// NEED or LIBrary require the argument as environ.
        else if (eq (p, "NEED") || eq (p, "LIBrary")) {
          FORWARD (i);
          if (i != NO_OPTION_LIST && strcmp (STR (i), "=") == 0) {
            FORWARD (i);
          }
          if (i == NO_OPTION_LIST) {
            option_error (start_l, start_c, "missing argument in");
          } else {
            char *q = strip_sign (STR (i));
            if (eq (q, "MVS")) {
              WRITELN (STDOUT_FILENO, "mvs required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
            }
            if (eq (q, "mpfr")) {
#if !defined (HAVE_GNU_MPFR)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "GNU MPFR required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "mathlib")) {
#if !defined (HAVE_MATHLIB)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "R mathlib required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "quadmath")) {
#if !defined (HAVE_QUADMATH)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "quadmath required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "gsl")) {
#if !defined (HAVE_GSL)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "GNU Scientific Library required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "plotutils")) {
#if !defined (HAVE_GNU_PLOTUTILS)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "plotutils required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "curses")) {
#if !defined (HAVE_CURSES)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "curses required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "ieee")) {
#if !defined (HAVE_IEEE_754)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "IEEE required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "linux")) {
#if !defined (BUILD_LINUX)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "linux required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "threads")) {
#if !defined (BUILD_PARALLEL_CLAUSE)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "threads required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "postgresql")) {
#if !defined (HAVE_POSTGRESQL)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "postgresql required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
            if (eq (q, "compiler")) {
#if !defined (BUILD_A68_COMPILER)
              io_close_tty_line ();
              WRITE (STDOUT_FILENO, "compiler required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
#endif
            }
#if !defined (BUILD_HTTP)
            if (eq (q, "http")) {
              io_close_tty_line ();
              WRITELN (STDOUT_FILENO, "HTTP support required - exiting graciously");
              a68_exit (EXIT_SUCCESS);
            }
#endif
          }
        }
// SCRIPT takes next argument as filename.
// Further options on the command line are not processed, but stored.
        else if (eq (p, "Script") && cmd_line) {
          FORWARD (i);
          if (i != NO_OPTION_LIST) {
            if (!name_set) {
              FILE_INITIAL_NAME (&A68_JOB) = new_string (STR (i), NO_TEXT);
              name_set = A68_TRUE;
            } else {
              option_error (start_l, start_c, "multiple source file names at");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
          skip = A68_TRUE;
        }
// VERIFY checks that argument is current a68g version number.
        else if (eq (p, "VERIFY")) {
          FORWARD (i);
          if (i != NO_OPTION_LIST && strcmp (STR (i), "=") == 0) {
            FORWARD (i);
          }
          if (i != NO_OPTION_LIST) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s verification \"%s\" does not match script verification \"%s\"", A68 (a68_cmd_name), PACKAGE_STRING, STR (i)) >= 0);
            ABEND (strcmp (PACKAGE_STRING, STR (i)) != 0, new_string (A68 (output_line), __func__), "outdated script");
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
        }
// HELP gives online help.
        else if ((eq (p, "APropos") || eq (p, "Help") || eq (p, "INfo")) && cmd_line) {
          FORWARD (i);
          if (i != NO_OPTION_LIST && strcmp (STR (i), "=") == 0) {
            FORWARD (i);
          }
          if (i != NO_OPTION_LIST) {
            apropos (STDOUT_FILENO, NO_TEXT, STR (i));
          } else {
            apropos (STDOUT_FILENO, NO_TEXT, "options");
          }
          a68_exit (EXIT_SUCCESS);
        }
// ECHO is treated later.
        else if (eq (p, "ECHO")) {
          if (strchr (p, '=') == NO_TEXT) {
            FORWARD (i);
            if (i != NO_OPTION_LIST) {
              if (strcmp (STR (i), "=") == 0) {
                FORWARD (i);
              }
            }
          }
        }
// EXECUTE and PRINT execute their argument as Algol 68 text.
        else if (eq (p, "Execute") || eq (p, "X") || eq (p, "Print")) {
          if (cmd_line == A68_FALSE) {
            option_error (start_l, start_c, "command-line-only");
          } else if ((FORWARD (i)) != NO_OPTION_LIST) {
            BOOL_T error = A68_FALSE;
            if (strcmp (STR (i), "=") == 0) {
              error = (BOOL_T) ((FORWARD (i)) == NO_OPTION_LIST);
            }
            if (!error) {
              BUFFER name, new_name;
              FILE *f;
              int s_errno = errno;
              bufcpy (name, HIDDEN_TEMP_FILE_NAME, BUFFER_SIZE);
              bufcat (name, ".a68", BUFFER_SIZE);
              f = a68_fopen (name, "w", new_name);
              ABEND (f == NO_FILE, ERROR_ACTION, __func__);
              errno = s_errno;
              if (eq (p, "Execute") || eq (p, "X")) {
                fprintf (f, "(%s)\n", STR (i));
              } else {
                fprintf (f, "(print (((%s), new line)))\n", STR (i));
              }
              ASSERT (fclose (f) == 0);
              FILE_INITIAL_NAME (&A68_JOB) = new_string (new_name, NO_TEXT);
            } else {
              option_error (start_l, start_c, "unit required by");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
        }
// STORAGE, HEAP, HANDLES, STACK, FRAME and OVERHEAD set core allocation.
        else if (eq (p, "STOrage")) {
          BOOL_T error = A68_FALSE;
          int k = fetch_integral (p, &i, &error);
// Adjust size.
          if (error || errno > 0) {
            option_error (start_l, start_c, "conversion error in");
          } else if (k > 0) {
            default_mem_sizes (k);
          }
        } else if (eq (p, "HEAP") || eq (p, "HANDLES") || eq (p, "STACK") || eq (p, "FRAME") || eq (p, "OVERHEAD")) {
          BOOL_T error = A68_FALSE;
          int k = fetch_integral (p, &i, &error);
// Adjust size.
          if (error || errno > 0) {
            option_error (start_l, start_c, "conversion error in");
          } else if (k > 0) {
            if (k < MIN_MEM_SIZE) {
              option_error (start_l, start_c, "value less than minimum in");
              k = MIN_MEM_SIZE;
            }
            if (eq (p, "HEAP")) {
              A68 (heap_size) = k;
            } else if (eq (p, "HANDLES")) {
              A68 (handle_pool_size) = k;
            } else if (eq (p, "STACK")) {
              A68 (expr_stack_size) = k;
            } else if (eq (p, "FRAME")) {
              A68 (frame_stack_size) = k;
            } else if (eq (p, "OVERHEAD")) {
              A68 (storage_overhead) = k;
            }
          }
        }
// COMPILE and NOCOMPILE switch on/off compilation.
        else if (eq (p, "Compile")) {
#if defined (BUILD_LINUX) || defined (BUILD_BSD)
          OPTION_COMPILE (&A68_JOB) = A68_TRUE;
          OPTION_COMPILE_CHECK (&A68_JOB) = A68_TRUE;
          if (OPTION_OPT_LEVEL (&A68_JOB) < OPTIMISE_1) {
            OPTION_OPT_LEVEL (&A68_JOB) = OPTIMISE_1;
          }
          OPTION_RUN_SCRIPT (&A68_JOB) = A68_FALSE;
#else
          option_error (start_l, start_c, "linux-only option");
#endif
        } else if (eq (p, "NOCompile") || eq (p, "NO-Compile")) {
          OPTION_COMPILE (&A68_JOB) = A68_FALSE;
          OPTION_RUN_SCRIPT (&A68_JOB) = A68_FALSE;
        }
// OPTIMISE and NOOPTIMISE switch on/off optimisation.
        else if (eq (p, "NOOptimize") || eq (p, "NO-Optimize")) {
          OPTION_OPT_LEVEL (&A68_JOB) = NO_OPTIMISE;
        } else if (eq (p, "O0")) {
          OPTION_OPT_LEVEL (&A68_JOB) = NO_OPTIMISE;
        } else if (eq (p, "OG")) {
          OPTION_COMPILE_CHECK (&A68_JOB) = A68_TRUE;
          OPTION_OPT_LEVEL (&A68_JOB) = OPTIMISE_0;
        } else if (eq (p, "OPTimise") || eq (p, "OPTimize")) {
          OPTION_COMPILE_CHECK (&A68_JOB) = A68_TRUE;
          OPTION_OPT_LEVEL (&A68_JOB) = OPTIMISE_1;
        } else if (eq (p, "O") || eq (p, "O1")) {
          OPTION_COMPILE_CHECK (&A68_JOB) = A68_TRUE;
          OPTION_OPT_LEVEL (&A68_JOB) = OPTIMISE_1;
        } else if (eq (p, "O2")) {
          OPTION_COMPILE_CHECK (&A68_JOB) = A68_FALSE;
          OPTION_OPT_LEVEL (&A68_JOB) = OPTIMISE_2;
        } else if (eq (p, "O3")) {
          OPTION_COMPILE_CHECK (&A68_JOB) = A68_FALSE;
          OPTION_OPT_LEVEL (&A68_JOB) = OPTIMISE_3;
        } else if (eq (p, "Ofast")) {
          OPTION_COMPILE_CHECK (&A68_JOB) = A68_FALSE;
          OPTION_OPT_LEVEL (&A68_JOB) = OPTIMISE_FAST;
        }
// ERROR-CHECK generates (some) runtime checks for O2, O3, Ofast.
        else if (eq (p, "ERRor-check")) {
          OPTION_COMPILE_CHECK (&A68_JOB) = A68_TRUE;
        }
// RUN-SCRIPT runs a compiled .sh script.
        else if (eq (p, "RUN-SCRIPT")) {
#if defined (BUILD_LINUX) || defined (BUILD_BSD)
          FORWARD (i);
          if (i != NO_OPTION_LIST) {
            if (!name_set) {
              FILE_INITIAL_NAME (&A68_JOB) = new_string (STR (i), NO_TEXT);
              name_set = A68_TRUE;
            } else {
              option_error (start_l, start_c, "multiple source file names at");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
          skip = A68_TRUE;
          OPTION_RUN_SCRIPT (&A68_JOB) = A68_TRUE;
          OPTION_NO_WARNINGS (&A68_JOB) = A68_TRUE;
          OPTION_COMPILE (&A68_JOB) = A68_FALSE;
#else
          option_error (start_l, start_c, "linux-only option");
#endif
        }
// RUN-QUOTE-SCRIPT runs a compiled .sh script.
        else if (eq (p, "RUN-QUOTE-SCRIPT")) {
#if defined (BUILD_LINUX) || defined (BUILD_BSD)
          FORWARD (i);
          if (i != NO_OPTION_LIST) {
            if (!name_set) {
              FILE_INITIAL_NAME (&A68_JOB) = new_string (STR (i), NO_TEXT);
              name_set = A68_TRUE;
            } else {
              option_error (start_l, start_c, "multiple source file names at");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
          skip = A68_TRUE;
          OPTION_RUN_SCRIPT (&A68_JOB) = A68_TRUE;
          OPTION_STROPPING (&A68_JOB) = QUOTE_STROPPING;
          OPTION_COMPILE (&A68_JOB) = A68_FALSE;
#else
          option_error (start_l, start_c, "linux-only option");
#endif
        }
// RERUN re-uses an existing .so file.
        else if (eq (p, "RERUN")) {
          OPTION_COMPILE (&A68_JOB) = A68_FALSE;
          OPTION_RERUN (&A68_JOB) = A68_TRUE;
          if (OPTION_OPT_LEVEL (&A68_JOB) < OPTIMISE_1) {
            OPTION_OPT_LEVEL (&A68_JOB) = OPTIMISE_1;
          }
        }
// KEEP and NOKEEP switch off/on object file deletion.
        else if (eq (p, "KEEP")) {
          OPTION_KEEP (&A68_JOB) = A68_TRUE;
        } else if (eq (p, "NOKEEP")) {
          OPTION_KEEP (&A68_JOB) = A68_FALSE;
        } else if (eq (p, "NO-KEEP")) {
          OPTION_KEEP (&A68_JOB) = A68_FALSE;
        }
// BRACKETS extends Algol 68 syntax for brackets.
        else if (eq (p, "BRackets")) {
          OPTION_BRACKETS (&A68_JOB) = A68_TRUE;
        }
// PRETTY and INDENT perform basic pretty printing.
// This is meant for synthetic code.
        else if (eq (p, "PRETty-print")) {
          OPTION_PRETTY (&A68_JOB) = A68_TRUE;
          OPTION_CHECK_ONLY (&A68_JOB) = A68_TRUE;
        } else if (eq (p, "INDENT")) {
          OPTION_PRETTY (&A68_JOB) = A68_TRUE;
          OPTION_CHECK_ONLY (&A68_JOB) = A68_TRUE;
        }
// FOLD performs constant folding in basic lay-out formatting..
        else if (eq (p, "FOLD")) {
          OPTION_INDENT (&A68_JOB) = A68_TRUE;
          OPTION_FOLD (&A68_JOB) = A68_TRUE;
          OPTION_CHECK_ONLY (&A68_JOB) = A68_TRUE;
        }
// REDUCTIONS gives parser reductions.
        else if (eq (p, "REDuctions")) {
          OPTION_REDUCTIONS (&A68_JOB) = A68_TRUE;
        }
// QUOTESTROPPING sets stropping to quote stropping.
        else if (eq (p, "QUOTEstropping")) {
          OPTION_STROPPING (&A68_JOB) = QUOTE_STROPPING;
        } else if (eq (p, "QUOTE-stropping")) {
          OPTION_STROPPING (&A68_JOB) = QUOTE_STROPPING;
        }
// UPPERSTROPPING sets stropping to upper stropping, which is nowadays the expected default.
        else if (eq (p, "UPPERstropping")) {
          OPTION_STROPPING (&A68_JOB) = UPPER_STROPPING;
        } else if (eq (p, "UPPER-stropping")) {
          OPTION_STROPPING (&A68_JOB) = UPPER_STROPPING;
        }
// CHECK and NORUN just check for syntax.
        else if (eq (p, "CHeck") || eq (p, "NORun") || eq (p, "NO-Run")) {
          OPTION_CHECK_ONLY (&A68_JOB) = A68_TRUE;
        }
// CLOCK times program execution.
        else if (eq (p, "CLock")) {
          OPTION_CLOCK (&A68_JOB) = A68_TRUE;
        }
// RUN overrides NORUN.
        else if (eq (p, "RUN")) {
          OPTION_RUN (&A68_JOB) = A68_TRUE;
        }
// MONITOR or DEBUG invokes the debugger at runtime errors.
        else if (eq (p, "MONitor") || eq (p, "DEBUG")) {
          OPTION_DEBUG (&A68_JOB) = A68_TRUE;
        }
// REGRESSION is an option that sets preferences when running the test suite - undocumented option.
        else if (eq (p, "REGRESSION")) {
          OPTION_NO_WARNINGS (&A68_JOB) = A68_FALSE;
          OPTION_PORTCHECK (&A68_JOB) = A68_TRUE;
          OPTION_REGRESSION_TEST (&A68_JOB) = A68_TRUE;
          OPTION_TIME_LIMIT (&A68_JOB) = 300;
          OPTION_KEEP (&A68_JOB) = A68_TRUE;
          A68 (term_width) = MAX_TERM_WIDTH;
        }
// LICense states the license
        else if (eq (p, "LICense")) {
          OPTION_LICENSE (&A68_JOB) = A68_TRUE;
        }
// NOWARNINGS switches unsuppressible warnings off.
        else if (eq (p, "NOWarnings")) {
          OPTION_NO_WARNINGS (&A68_JOB) = A68_TRUE;
        } else if (eq (p, "NO-Warnings")) {
          OPTION_NO_WARNINGS (&A68_JOB) = A68_TRUE;
        }
// QUIET switches all warnings off.
        else if (eq (p, "Quiet")) {
          OPTION_QUIET (&A68_JOB) = A68_TRUE;
        }
// WARNINGS switches warnings on.
        else if (eq (p, "Warnings")) {
          OPTION_NO_WARNINGS (&A68_JOB) = A68_FALSE;
        }
// NOPORTCHECK switches portcheck off.
        else if (eq (p, "NOPORTcheck")) {
          OPTION_PORTCHECK (&A68_JOB) = A68_FALSE;
        } else if (eq (p, "NO-PORTcheck")) {
          OPTION_PORTCHECK (&A68_JOB) = A68_FALSE;
        }
// PORTCHECK switches portcheck on.
        else if (eq (p, "PORTcheck")) {
          OPTION_PORTCHECK (&A68_JOB) = A68_TRUE;
        }
// PEDANTIC switches portcheck and warnings on.
        else if (eq (p, "PEDANTIC")) {
          OPTION_PORTCHECK (&A68_JOB) = A68_TRUE;
          OPTION_NO_WARNINGS (&A68_JOB) = A68_FALSE;
        }
// PRAGMATS and NOPRAGMATS switch on/off pragmat processing.
        else if (eq (p, "PRagmats")) {
          OPTION_PRAGMAT_SEMA (&A68_JOB) = A68_TRUE;
        } else if (eq (p, "NOPRagmats")) {
          OPTION_PRAGMAT_SEMA (&A68_JOB) = A68_FALSE;
        } else if (eq (p, "NO-PRagmats")) {
          OPTION_PRAGMAT_SEMA (&A68_JOB) = A68_FALSE;
        }
// STRICT ignores A68G extensions to A68 syntax.
        else if (eq (p, "STRict")) {
          OPTION_STRICT (&A68_JOB) = A68_TRUE;
          OPTION_PORTCHECK (&A68_JOB) = A68_TRUE;
        }
// VERBOSE in case you want to know what Algol68G is doing.
        else if (eq (p, "VERBose")) {
          OPTION_VERBOSE (&A68_JOB) = A68_TRUE;
        }
// VERSION lists the current version at an appropriate time in the future.
        else if (eq (p, "Version")) {
          OPTION_VERSION (&A68_JOB) = A68_TRUE;
        } else if (eq (p, "MODular-arithmetic")) {
          OPTION_NODEMASK (&A68_JOB) |= MODULAR_MASK;
        } else if (eq (p, "NOMODular-arithmetic")) {
          OPTION_NODEMASK (&A68_JOB) &= ~MODULAR_MASK;
        } else if (eq (p, "NO-MODular-arithmetic")) {
          OPTION_NODEMASK (&A68_JOB) &= ~MODULAR_MASK;
        }
// XREF and NOXREF switch on/off a cross reference.
        else if (eq (p, "XREF")) {
          OPTION_SOURCE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_CROSS_REFERENCE (&A68_JOB) = A68_TRUE;
          OPTION_NODEMASK (&A68_JOB) |= (CROSS_REFERENCE_MASK | SOURCE_MASK);
        } else if (eq (p, "NOXREF")) {
          OPTION_NODEMASK (&A68_JOB) &= ~(CROSS_REFERENCE_MASK | SOURCE_MASK);
        } else if (eq (p, "NO-Xref")) {
          OPTION_NODEMASK (&A68_JOB) &= ~(CROSS_REFERENCE_MASK | SOURCE_MASK);
        }
// PRELUDELISTING cross references preludes, if they ever get implemented ...
        else if (eq (p, "PRELUDElisting")) {
          OPTION_SOURCE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_CROSS_REFERENCE (&A68_JOB) = A68_TRUE;
          OPTION_STATISTICS_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_NODEMASK (&A68_JOB) |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
          OPTION_STANDARD_PRELUDE_LISTING (&A68_JOB) = A68_TRUE;
        }
// STATISTICS prints process statistics.
        else if (eq (p, "STatistics")) {
          OPTION_STATISTICS_LISTING (&A68_JOB) = A68_TRUE;
        }
// TREE and NOTREE switch on/off printing of the syntax tree. This gets bulky!.
        else if (eq (p, "TREE")) {
          OPTION_SOURCE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_TREE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_NODEMASK (&A68_JOB) |= (TREE_MASK | SOURCE_MASK);
        } else if (eq (p, "NOTREE")) {
          OPTION_NODEMASK (&A68_JOB) ^= (TREE_MASK | SOURCE_MASK);
        } else if (eq (p, "NO-TREE")) {
          OPTION_NODEMASK (&A68_JOB) ^= (TREE_MASK | SOURCE_MASK);
        }
// UNUSED indicates unused tags.
        else if (eq (p, "UNUSED")) {
          OPTION_UNUSED (&A68_JOB) = A68_TRUE;
        }
// EXTENSIVE set of options for an extensive listing.
        else if (eq (p, "EXTensive")) {
          OPTION_SOURCE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_OBJECT_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_TREE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_CROSS_REFERENCE (&A68_JOB) = A68_TRUE;
          OPTION_MOID_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_STANDARD_PRELUDE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_STATISTICS_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_UNUSED (&A68_JOB) = A68_TRUE;
          OPTION_NODEMASK (&A68_JOB) |= (CROSS_REFERENCE_MASK | TREE_MASK | CODE_MASK | SOURCE_MASK);
        }
// LISTING set of options for a default listing.
        else if (eq (p, "Listing")) {
          OPTION_SOURCE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_CROSS_REFERENCE (&A68_JOB) = A68_TRUE;
          OPTION_STATISTICS_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_NODEMASK (&A68_JOB) |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
        }
// TTY send listing to standout. Remnant from my mainframe past.
        else if (eq (p, "TTY")) {
          OPTION_CROSS_REFERENCE (&A68_JOB) = A68_TRUE;
          OPTION_STATISTICS_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_NODEMASK (&A68_JOB) |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
        }
// SOURCE and NOSOURCE print source lines.
        else if (eq (p, "SOURCE")) {
          OPTION_SOURCE_LISTING (&A68_JOB) = A68_TRUE;
          OPTION_NODEMASK (&A68_JOB) |= SOURCE_MASK;
        } else if (eq (p, "NOSOURCE")) {
          OPTION_NODEMASK (&A68_JOB) &= ~SOURCE_MASK;
        } else if (eq (p, "NO-SOURCE")) {
          OPTION_NODEMASK (&A68_JOB) &= ~SOURCE_MASK;
        }
// OBJECT and NOOBJECT print object lines.
        else if (eq (p, "OBJECT")) {
          OPTION_OBJECT_LISTING (&A68_JOB) = A68_TRUE;
        } else if (eq (p, "NOOBJECT")) {
          OPTION_OBJECT_LISTING (&A68_JOB) = A68_FALSE;
        } else if (eq (p, "NO-OBJECT")) {
          OPTION_OBJECT_LISTING (&A68_JOB) = A68_FALSE;
        }
// MOIDS prints an overview of moids used in the program.
        else if (eq (p, "MOIDS")) {
          OPTION_MOID_LISTING (&A68_JOB) = A68_TRUE;
        }
// ASSERTIONS and NOASSERTIONS switch on/off the processing of assertions.
        else if (eq (p, "Assertions")) {
          OPTION_NODEMASK (&A68_JOB) |= ASSERT_MASK;
        } else if (eq (p, "NOAssertions")) {
          OPTION_NODEMASK (&A68_JOB) &= ~ASSERT_MASK;
        } else if (eq (p, "NO-Assertions")) {
          OPTION_NODEMASK (&A68_JOB) &= ~ASSERT_MASK;
        }
// PRECISION sets the LONG LONG precision.
        else if (eq (p, "PRECision")) {
          BOOL_T error = A68_FALSE;
          int N = fetch_integral (p, &i, &error);
          int k = width_to_mp_digits (N);
          if (k <= 0 || error || errno > 0) {
            option_error (start_l, start_c, "invalid value in");
          } else if (long_mp_digits () > 0 && long_mp_digits () != k) {
            option_error (start_l, start_c, "different precision was already specified in");
          } else if (k > mp_digits ()) {
            set_long_mp_digits (k);
          } else {
            option_error (start_l, start_c, "attempt to set LONG LONG precision lower than LONG precision");
          }
        }
// BACKTRACE and NOBACKTRACE switch on/off stack backtracing.
        else if (eq (p, "BACKtrace")) {
          OPTION_BACKTRACE (&A68_JOB) = A68_TRUE;
        } else if (eq (p, "NOBACKtrace")) {
          OPTION_BACKTRACE (&A68_JOB) = A68_FALSE;
        } else if (eq (p, "NO-BACKtrace")) {
          OPTION_BACKTRACE (&A68_JOB) = A68_FALSE;
        }
// BREAK and NOBREAK switch on/off tracing of the running program.
        else if (eq (p, "BReakpoint")) {
          OPTION_NODEMASK (&A68_JOB) |= BREAKPOINT_MASK;
        } else if (eq (p, "NOBReakpoint")) {
          OPTION_NODEMASK (&A68_JOB) &= ~BREAKPOINT_MASK;
        } else if (eq (p, "NO-BReakpoint")) {
          OPTION_NODEMASK (&A68_JOB) &= ~BREAKPOINT_MASK;
        }
// TRACE and NOTRACE switch on/off tracing of the running program.
        else if (eq (p, "TRace")) {
          OPTION_TRACE (&A68_JOB) = A68_TRUE;
          OPTION_NODEMASK (&A68_JOB) |= BREAKPOINT_TRACE_MASK;
        } else if (eq (p, "NOTRace")) {
          OPTION_NODEMASK (&A68_JOB) &= ~BREAKPOINT_TRACE_MASK;
        } else if (eq (p, "NO-TRace")) {
          OPTION_NODEMASK (&A68_JOB) &= ~BREAKPOINT_TRACE_MASK;
        }
// TIMELIMIT lets the interpreter stop after so-many seconds.
        else if (eq (p, "TImelimit") || eq (p, "TIME-Limit")) {
          BOOL_T error = A68_FALSE;
          int k = fetch_integral (p, &i, &error);
          if (error || errno > 0) {
            option_error (start_l, start_c, "conversion error in");
          } else if (k < 1) {
            option_error (start_l, start_c, "invalid time span in");
          } else {
            OPTION_TIME_LIMIT (&A68_JOB) = k;
          }
        } else {
// Unrecognised.
          option_error (start_l, start_c, "unrecognised");
        }
        a68_free (stale);
      }
// Go processing next item, if present.
      if (i != NO_OPTION_LIST) {
        FORWARD (i);
      }
    }
  }
// Mark options as processed.
  for (; j != NO_OPTION_LIST; FORWARD (j)) {
    PROCESSED (j) = A68_TRUE;
  }
  return (BOOL_T) (errno == 0);
}
