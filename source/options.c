/*!
\file options.c
\brief handles options
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
This code handles options to Algol68G.

Option syntax does not follow GNU standards.

Options come from:
  [1] A rc file (normally .a68grc).
  [2] The A68G_OPTIONS environment variable overrules [1].
  [3] Command line options overrule [2].
  [4] Pragmat items overrule [3]. 
*/

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

OPTIONS_T *options;
BOOL_T a68c_diags, gnu_diags, no_warnings;

/*!
\brief
\param l
\param option
\return
**/

static void option_error (SOURCE_LINE_T * l, char *option, char *info)
{
  if (info != NULL) {
    snprintf (edit_line, BUFFER_SIZE, ERROR_OPTION_INFO, option, info);
  } else {
    snprintf (edit_line, BUFFER_SIZE, ERROR_OPTION, option);
  }
  scan_error (l, NULL, edit_line);
}

/*!
\brief
\param l
\param option
\return
**/

static void option_unrecognised (SOURCE_LINE_T * l, char *option)
{
  char *c;
  snprintf (edit_line, BUFFER_SIZE, ERROR_OPTION_UNKNOWN, option);
  for (c = edit_line; c[0] != NULL_CHAR; c++) {
    c[0] = TO_LOWER (c[0]);
  }
  scan_error (l, NULL, edit_line);
}

/*!
\brief strip minus signs preceeding a string
\param p
\return
**/

static char *strip_minus (char *p)
{
  while (p[0] == '-') {
    p++;
  }
  return (new_string (p));
}

/*!
\brief add an option to the list, to be processed later
\param l
\param str
\param line
**/

void add_option_list (OPTION_LIST_T ** l, char *str, SOURCE_LINE_T * line)
{
  if (*l == NULL) {
    *l = (OPTION_LIST_T *) get_heap_space (SIZE_OF (OPTION_LIST_T));
    (*l)->scan = source_scan;
    (*l)->str = new_string (str);
    (*l)->processed = A_FALSE;
    (*l)->line = line;
    NEXT (*l) = NULL;
  } else {
    add_option_list (&(NEXT (*l)), str, line);
  }
}

/*!
\brief initialise option handler
\param module
**/

void init_options (MODULE_T * module)
{
  options = (OPTIONS_T *) malloc ((size_t) SIZE_OF (OPTIONS_T));
  module->options.list = NULL;
}

/*!
\brief eq
\param module
\param p
\param q
\return
**/

static BOOL_T eq (MODULE_T * module, char *p, char *q)
{
/* Upper case letters in 'q' are mandatory, lower case must match. */
  if (module->options.pragmat_sema) {
    return (match_string (p, q, '='));
  } else {
    return (A_FALSE);
  }
}

/*!
\brief process echoes gathered in the option list
\param module
\param i
**/

void prune_echoes (MODULE_T * module, OPTION_LIST_T * i)
{
  while (i != NULL) {
    if (i->scan == source_scan) {
      char *p = strip_minus (i->str);
/* ECHO echoes a string. */
      if (eq (module, p, "ECHO")) {
        {
          char *car = a68g_strchr (p, '=');
          if (car != NULL) {
            io_close_tty_line ();
            snprintf (output_line, BUFFER_SIZE, "%s", &car[1]);
            io_write_string (STDOUT_FILENO, output_line);
          } else {
            FORWARD (i);
            if (i != NULL) {
              if (strcmp (i->str, "=") == 0) {
                FORWARD (i);
              }
              if (i != NULL) {
                io_close_tty_line ();
                snprintf (output_line, BUFFER_SIZE, "%s", i->str);
                io_write_string (STDOUT_FILENO, output_line);
              }
            }
          }
        }
      }
    }
    FORWARD (i);
  }
}

/*!
\brief process options gathered in the option list
\param p
\param i
\param error
\return
**/

static int fetch_integral (char *p, OPTION_LIST_T ** i, BOOL_T * error)
{
  SOURCE_LINE_T *start_l = (*i)->line;
  char *start_c = (*i)->str;
  char *car = NULL, *num = NULL;
  int k, mult = 1;
  *error = A_FALSE;
/* Fetch argument. */
  car = a68g_strchr (p, '=');
  if (car == NULL) {
    FORWARD (*i);
    *error = (*i == NULL);
    if (!error && strcmp ((*i)->str, "=") == 0) {
      FORWARD (*i);
      *error = ((*i) == NULL);
    }
    if (!*error) {
      num = (*i)->str;
    }
  } else {
    num = &car[1];
  }
/* Translate argument into integer. */
  if (*error) {
    option_error (start_l, start_c, NULL);
    return (0);
  } else {
    char *postfix;
    RESET_ERRNO;
    k = strtol (num, &postfix, 0);      /* Accept also octal and hex. */
    *error = (postfix == num);
    if (errno != 0 || *error) {
      option_error (start_l, start_c, NULL);
      *error = A_TRUE;
    } else if (k < 0) {
      option_error (start_l, start_c, NULL);
      *error = A_TRUE;
    } else {
/* Accept postfix multipliers: 32k, 64M, 1G. */
      if (postfix != NULL) {
        switch (postfix[0]) {
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
            option_error (start_l, start_c, NULL);
            *error = A_TRUE;
            break;
          }
        }
        if (postfix[0] != NULL_CHAR && postfix[1] != NULL_CHAR) {
          option_error (start_l, start_c, NULL);
          *error = A_TRUE;
        }
      }
    }
    if ((double) k * (double) mult > (double) MAX_INT) {
      errno = ERANGE;
      option_error (start_l, start_c, NULL);
    }
    return (k * mult);
  }
}

/*!
\brief process options gathered in the option list
\param module
\param i
\param cmd_line
\return
**/

BOOL_T set_options (MODULE_T * module, OPTION_LIST_T * i, BOOL_T cmd_line)
{
  BOOL_T go_on = A_TRUE, name_set = A_FALSE;
  OPTION_LIST_T *j = i;
  RESET_ERRNO;
  while (i != NULL && go_on) {
    SOURCE_LINE_T *start_l = i->line;
    char *start_c = i->str;
    if (!(i->processed)) {
/* Accept UNIX '-option [=] value' */
      BOOL_T minus_sign = ((i->str)[0] == '-');
      char *p = strip_minus (i->str);
      if (!minus_sign && cmd_line) {
/* Item without '-'s is generic filename. */
        if (!name_set) {
          module->files.generic_name = new_string (p);
          name_set = A_TRUE;
        } else {
          option_error (NULL, start_c, NULL);
        }
      }
/* Preprocessor items stop option processing. */
      else if (eq (module, p, "INCLUDE")) {
        go_on = A_FALSE;
      } else if (eq (module, p, "READ")) {
        go_on = A_FALSE;
      } else if (eq (module, p, "PREPROCESSOR")) {
        go_on = A_FALSE;
      } else if (eq (module, p, "NOPREPROCESSOR")) {
        go_on = A_FALSE;
      }
/* EXIT stops option processing. */
      else if (eq (module, p, "EXIT")) {
        go_on = A_FALSE;
      }
/* Empty item (from specifying '-' or '--') stops option processing. */
      else if (eq (module, p, "")) {
        go_on = A_FALSE;
      }
/* FILE accepts its argument as generic filename. */
      else if (eq (module, p, "File") && cmd_line) {
        FORWARD (i);
        if (i != NULL && strcmp (i->str, "=") == 0) {
          FORWARD (i);
        }
        if (i != NULL) {
          if (!name_set) {
            module->files.generic_name = new_string (i->str);
            name_set = A_TRUE;
          } else {
            option_error (start_l, start_c, NULL);
          }
        } else {
          option_error (start_l, start_c, NULL);
        }
      }
/* HELP gives online help. */
      else if (eq (module, p, "Help") && cmd_line == A_TRUE) {
        online_help (STDOUT_FILENO);
        a68g_exit (EXIT_SUCCESS);
      }
/* ECHO is treated later. */
      else if (eq (module, p, "ECHO")) {
        if (a68g_strchr (p, '=') == NULL) {
          FORWARD (i);
          if (i != NULL) {
            if (strcmp (i->str, "=") == 0) {
              FORWARD (i);
            }
          }
        }
      }
/* EXECUTE and PRINT execute their argument as Algol 68 text. */
      else if (eq (module, p, "Execute") || eq (module, p, "Print")) {
        if (cmd_line == A_FALSE) {
          option_error (start_l, start_c, "command line only");
        } else if ((FORWARD (i)) != NULL) {
          BOOL_T error = A_FALSE;
          if (strcmp (i->str, "=") == 0) {
            error = (FORWARD (i)) == NULL;
          }
          if (!error) {
            char name[BUFFER_SIZE];
            FILE *f;
            bufcpy (name, ".", BUFFER_SIZE);
            bufcat (name, A68G_NAME, BUFFER_SIZE);
            bufcat (name, ".x", BUFFER_SIZE);
            f = fopen (name, "w");
            ABNORMAL_END (f == NULL, "cannot open temp file", NULL);
            if (eq (module, p, "Execute")) {
              fprintf (f, "(%s)\n", i->str);
            } else {
              fprintf (f, "(print ((%s)))\n", i->str);
            }
            fclose (f);
            module->files.generic_name = new_string (name);
          } else {
            option_error (start_l, start_c, NULL);
          }
        } else {
          option_error (start_l, start_c, NULL);
        }
      }
/* HEAP, HANDLES, STACK, FRAME and OVERHEAD  set core allocation. */
      else if (eq (module, p, "HEAP") || eq (module, p, "HANDLES") || eq (module, p, "STACK") || eq (module, p, "FRAME") || eq (module, p, "OVERHEAD")) {
        BOOL_T error = A_FALSE;
        int k = fetch_integral (p, &i, &error);
/* Adjust size. */
        if (error || errno > 0) {
          option_error (start_l, start_c, NULL);
        } else if (k > 0) {
          if (k < MIN_MEM_SIZE) {
            option_error (start_l, start_c, NULL);
            k = MIN_MEM_SIZE;
          }
          if (eq (module, p, "HEAP")) {
            heap_size = k;
          } else if (eq (module, p, "HANDLE")) {
            handle_pool_size = k;
          } else if (eq (module, p, "STACK")) {
            expr_stack_size = k;
          } else if (eq (module, p, "FRAME")) {
            frame_stack_size = k;
          } else if (eq (module, p, "OVERHEAD")) {
            storage_overhead = k;
          }
        }
      }
/* BRACKETS extends Algol 68 syntax for brackets. */
      else if (eq (module, p, "BRackets")) {
        module->options.brackets = A_TRUE;
      }
/* REDUCTIONS gives parser reductions.*/
      else if (eq (module, p, "REDuctions")) {
        module->options.reductions = A_TRUE;
      }
/* A68CDIAGNOSTIC gives A68C style diagnostics. */
      else if (eq (module, p, "A68CDiagnostics")) {
        a68c_diags = A_TRUE;
        gnu_diags = A_FALSE;
      }
/* GNUDIAGNOSTIC gives GNU style diagnostics. */
      else if (eq (module, p, "GNUDiagnostics")) {
        gnu_diags = A_TRUE;
        a68c_diags = A_FALSE;
      }
/* VMSDIAGNOSTIC gives VMS style diagnostics. */
      else if (eq (module, p, "VMSDiagnostics")) {
        a68c_diags = A_FALSE;
        gnu_diags = A_FALSE;
      }
/* QUOTESTROPPING sets stropping to quote stropping. */
      else if (eq (module, p, "QUOTEstropping")) {
        module->options.stropping = QUOTE_STROPPING;
      }
/* UPPERSTROPPING sets stropping to upper stropping, which is nowadays the expected default. */
      else if (eq (module, p, "UPPERstropping")) {
        module->options.stropping = UPPER_STROPPING;
      }
/* CHECK and NORUN just check for syntax. */
      else if (eq (module, p, "Check") || eq (module, p, "NORun")) {
        module->options.check_only = A_TRUE;
      }
/* RUN overrides NORUN. */
      else if (eq (module, p, "RUN")) {
        module->options.run = A_TRUE;
      }
/* MONITOR or DEBUG invokes the debugger at runtime errors. */
      else if (eq (module, p, "MONitor") || eq (module, p, "DEBUG")) {
        module->options.debug = A_TRUE;
      }
/* REGRESSION is an option that sets preferences when running the Algol68G test suite. */
      else if (eq (module, p, "REGRESSION")) {
        module->options.regression_test = A_TRUE;
        gnu_diags = A_FALSE;
        module->options.time_limit = 10;
      }
/* NOWARNINGS switches warnings off. */
      else if (eq (module, p, "NOWarnings")) {
        no_warnings = A_TRUE;
      }
/* WARNINGS switches warnings on. */
      else if (eq (module, p, "Warnings")) {
        no_warnings = A_FALSE;
      }
/* NOPORTCHECK switches portcheck off. */
      else if (eq (module, p, "NOPORTcheck")) {
        module->options.portcheck = A_FALSE;
      }
/* PORTCHECK switches portcheck on. */
      else if (eq (module, p, "PORTcheck")) {
        module->options.portcheck = A_TRUE;
      }
/* PEDANTIC switches portcheck and warnings on. */
      else if (eq (module, p, "PEDANTIC")) {
        module->options.portcheck = A_TRUE;
        no_warnings = A_FALSE;
      }
/* PRAGMATS and NOPRAGMATS switch on/off pragmat processing. */
      else if (eq (module, p, "PRagmats")) {
        module->options.pragmat_sema = A_TRUE;
      } else if (eq (module, p, "NOPRagmats")) {
        module->options.pragmat_sema = A_FALSE;
      }
/* VERBOSE in case you want to know what Algol68G is doing. */
      else if (eq (module, p, "VERBose")) {
        module->options.verbose = A_TRUE;
      }
/* VERSION lists the current version at an appropriate time in the future. */
      else if (eq (module, p, "Version")) {
        module->options.version = A_TRUE;
      }
/* XREF and NOXREF switch on/off a cross reference. */
      else if (eq (module, p, "Xref")) {
        module->options.source_listing = A_TRUE;
        module->options.cross_reference = A_TRUE;
        module->options.nodemask |= (CROSS_REFERENCE_MASK | SOURCE_MASK);
      } else if (eq (module, p, "NOXref")) {
        module->options.nodemask &= ~(CROSS_REFERENCE_MASK | SOURCE_MASK);
      }
/* PRELUDELISTING cross references preludes, if they ever get implemented ... */
      else if (eq (module, p, "PRELUDElisting")) {
        module->options.standard_prelude_listing = A_TRUE;
      }
/* STATISTICS prints process statistics. */
      else if (eq (module, p, "STatistics")) {
        module->options.statistics_listing = A_TRUE;
      }
/* TREE and NOTREE switch on/off printing of the syntax tree. This gets bulky! */
      else if (eq (module, p, "TREE")) {
        module->options.source_listing = A_TRUE;
        module->options.tree_listing = A_TRUE;
        module->options.nodemask |= (TREE_MASK | SOURCE_MASK);
      } else if (eq (module, p, "NOTREE")) {
        module->options.nodemask ^= (TREE_MASK | SOURCE_MASK);
      }
/* UNUSED indicates unused tags. */
      else if (eq (module, p, "UNUSED")) {
        module->options.unused = A_TRUE;
      }
/* EXTENSIVE set of options for an extensive listing. */
      else if (eq (module, p, "EXTensive")) {
        module->options.source_listing = A_TRUE;
        module->options.tree_listing = A_TRUE;
        module->options.cross_reference = A_TRUE;
        module->options.moid_listing = A_TRUE;
        module->options.standard_prelude_listing = A_TRUE;
        module->options.statistics_listing = A_TRUE;
        module->options.unused = A_TRUE;
        module->options.nodemask |= (CROSS_REFERENCE_MASK | TREE_MASK | CODE_MASK | SOURCE_MASK);
      }
/* LISTING set of options for a default listing. */
      else if (eq (module, p, "LISTing")) {
        module->options.source_listing = A_TRUE;
        module->options.cross_reference = A_TRUE;
        module->options.statistics_listing = A_TRUE;
        module->options.nodemask |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
      }
/* TTY send listing to standout. Remnant from my mainframe past. */
      else if (eq (module, p, "TTY")) {
        module->options.cross_reference = A_TRUE;
        module->options.statistics_listing = A_TRUE;
        module->options.nodemask |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
      }
/* SOURCE and NOSOURCE print source lines. */
      else if (eq (module, p, "SOURCE")) {
        module->options.source_listing = A_TRUE;
        module->options.nodemask |= SOURCE_MASK;
      } else if (eq (module, p, "NOSOURCE")) {
        module->options.nodemask &= ~SOURCE_MASK;
      }
/* MOIDS prints an overview of moids used in the program. */
      else if (eq (module, p, "MOIDS")) {
        module->options.moid_listing = A_TRUE;
      }
/* ASSERTIONS and NOASSERTIONS switch on/off the processing of assertions. */
      else if (eq (module, p, "Assertions")) {
        module->options.nodemask |= ASSERT_MASK;
      } else if (eq (module, p, "NOAssertions")) {
        module->options.nodemask &= ~ASSERT_MASK;
      }
/* PRECISION sets the precision. */
      else if (eq (module, p, "PRECision")) {
        BOOL_T error = A_FALSE;
        int k = fetch_integral (p, &i, &error);
        if (error || errno > 0) {
          option_error (start_l, start_c, NULL);
        } else if (k > 1) {
          if (int_to_mp_digits (k) > long_mp_digits ()) {
            set_longlong_mp_digits (int_to_mp_digits (k));
          } else {
            k = 1;
            while (int_to_mp_digits (k) <= long_mp_digits ()) {
              k++;
            }
            option_error (start_l, start_c, NULL);
          }
        } else {
          option_error (start_l, start_c, NULL);
        }
      }
/* BREAK and NOBREAK switch on/off tracing of the running program. */
      else if (eq (module, p, "BReakpoint")) {
        module->options.nodemask |= BREAKPOINT_MASK;
      } else if (eq (module, p, "NOBReakpoint")) {
        module->options.nodemask &= ~BREAKPOINT_MASK;
      }
/* TRACE and NOTRACE switch on/off tracing of the running program. */
      else if (eq (module, p, "TRace")) {
        module->options.trace = A_TRUE;
        module->options.nodemask |= TRACE_MASK;
      } else if (eq (module, p, "NOTRace")) {
        module->options.nodemask &= ~TRACE_MASK;
      }
/* TIMELIMIT lets the interpreter stop after so-many seconds. */
      else if (eq (module, p, "TImelimit")) {
        BOOL_T error = A_FALSE;
        int k = fetch_integral (p, &i, &error);
        if (error || errno > 0) {
          option_error (start_l, start_c, NULL);
        } else if (k < 1) {
          option_error (start_l, start_c, NULL);
        } else {
          module->options.time_limit = k;
        }
      } else {
/* Unrecognised. */
        option_unrecognised (start_l, start_c);
      }
    }
/* Go processing next item, if present. */
    if (i != NULL) {
      FORWARD (i);
    }
  }
/* Mark options as processed. */
  for (; j != NULL; j = NEXT (j)) {
    j->processed = A_TRUE;
  }
  return (errno == 0);
}

/*!
\brief set default core size
**/

void default_mem_sizes (void)
{
  frame_stack_size = 2 * MEGABYTE;
  expr_stack_size = MEGABYTE;
  heap_size = 25 * MEGABYTE;
  handle_pool_size = 4 * MEGABYTE;
  storage_overhead = 256 * KILOBYTE;
}

/*!
\brief set default values for options
\param module
**/

void default_options (MODULE_T * module)
{
  module->options.check_only = A_FALSE;
  module->options.moid_listing = A_FALSE;
  module->options.tree_listing = A_FALSE;
  module->options.source_listing = A_FALSE;
  module->options.statistics_listing = A_FALSE;
  module->options.standard_prelude_listing = A_FALSE;
  module->options.verbose = A_FALSE;
  module->options.version = A_FALSE;
  module->options.cross_reference = A_FALSE;
  module->options.unused = A_FALSE;
  module->options.pragmat_sema = A_TRUE;
  module->options.trace = A_FALSE;
  module->options.regression_test = A_FALSE;
  module->options.nodemask = ASSERT_MASK | SOURCE_MASK;
  module->options.time_limit = 0;
  module->options.stropping = UPPER_STROPPING;
  module->options.brackets = A_FALSE;
  module->options.reductions = A_FALSE;
  module->options.run = A_FALSE;
  module->options.portcheck = A_FALSE;
  module->options.debug = A_FALSE;
#if defined WIN32_VERSION
  a68c_diags = A_FALSE;
  gnu_diags = A_TRUE;
  no_warnings = A_TRUE;
#else
  a68c_diags = A_TRUE;
  gnu_diags = A_FALSE;
  no_warnings = A_TRUE;
#endif
}

/*!
\brief read options from the .rc file
\param module
**/

void read_rc_options (MODULE_T * module)
{
  FILE *f;
  int len = 2 + strlen (A68G_NAME) + strlen ("rc");
  char *name = (char *) get_heap_space (len);
  bufcpy (name, ".", len);
  bufcat (name, A68G_NAME, len);
  bufcat (name, "rc", len);
  f = fopen (name, "r");
  if (f != NULL) {
    while (!feof (f)) {
      if (fgets (input_line, BUFFER_SIZE, f) != NULL) {
        if (input_line[strlen (input_line) - 1] == NEWLINE_CHAR) {
          input_line[strlen (input_line) - 1] = NULL_CHAR;
        }
        isolate_options (module, input_line, NULL);
      }
    }
    fclose (f);
    set_options (module, module->options.list, A_FALSE);
  } else {
    errno = 0;
  }
}

/*!
\brief read options from A68G_OPTIONS
\param module
**/

void read_env_options (MODULE_T * module)
{
  if (getenv ("A68G_OPTIONS") != NULL) {
    isolate_options (module, getenv ("A68G_OPTIONS"), NULL);
    set_options (module, module->options.list, A_FALSE);
    errno = 0;
  }
}

/*!
\brief tokenise string 'p' that holds options
\param module
\param p
\param line
**/

void isolate_options (MODULE_T * module, char *p, SOURCE_LINE_T * line)
{
  char *q;
/* 'q' points at first significant char in item .*/
  while (p[0] != NULL_CHAR) {
/* Skip white space ... */
    while ((p[0] == BLANK_CHAR || p[0] == TAB_CHAR || p[0] == ',') && p[0] != NULL_CHAR) {
      p++;
    }
/* ... then tokenise an item. */
    if (p[0] != NULL_CHAR) {
/* Item can be "string". Note that these are not A68 strings. */
      if (p[0] == QUOTE_CHAR || p[0] == '\'' || p[0] == '`') {
        char delim = p[0];
        p++;
        q = p;
        while (p[0] != delim && p[0] != NULL_CHAR) {
          p++;
        }
        if (p[0] != NULL_CHAR) {
          p[0] = NULL_CHAR;     /* p[0] was delimiter. */
          p++;
        } else {
          scan_error (line, NULL, ERROR_UNTERMINATED_STRING);
        }
      } else {
/* Item is not a delimited string. */
        q = p;
/* Tokenise symbol and gather it in the option list for later processing.
   Skip '='s, we accept if someone writes -prec=60 -heap=8192. */
        if (*q == '=') {
          p++;
        } else {
          /*
           * Skip item. 
           */
          while (p[0] != BLANK_CHAR && p[0] != NULL_CHAR && p[0] != '=' && p[0] != ',') {
            p++;
          }
        }
        if (p[0] != NULL_CHAR) {
          p[0] = NULL_CHAR;
          p++;
        }
      }
/* 'q' points to first significant char in item, and 'p' points after item. */
      add_option_list (&(module->options.list), q, line);
    }
  }
}
