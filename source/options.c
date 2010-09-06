/*!
\file options.c
\brief handles options
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

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
This code handles options to Algol68G.

Option syntax does not follow GNU standards.

Options come from:
  [1] A rc file (normally .a68grc).
  [2] The A68G_OPTIONS environment variable overrules [1].
  [3] Command line options overrule [2].
  [4] Pragmat items overrule [3]. 
*/

#include "config.h"
#include "diagnostics.h"
#include "algol68g.h"
#include "genie.h"
#include "mp.h"

OPTIONS_T *options;
BOOL_T no_warnings = A68_TRUE;

/*!
\brief set default values for options
\param module current module
**/

void default_options (void)
{
  no_warnings = A68_TRUE;
  program.options.backtrace = A68_FALSE;
  program.options.brackets = A68_FALSE;
  program.options.check_only = A68_FALSE;
  program.options.compile = A68_FALSE;
  program.options.cross_reference = A68_FALSE;
  program.options.debug = A68_FALSE;
  program.options.keep = A68_FALSE;
  program.options.moid_listing = A68_FALSE;
  program.options.nodemask = (STATUS_MASK) (ASSERT_MASK | SOURCE_MASK);
  program.options.optimise = A68_FALSE;
  program.options.portcheck = A68_FALSE;
  program.options.pragmat_sema = A68_TRUE;
  program.options.reductions = A68_FALSE;
  program.options.regression_test = A68_FALSE;
  program.options.rerun = A68_FALSE;
  program.options.run = A68_FALSE;
  program.options.run_script = A68_FALSE;
  program.options.source_listing = A68_FALSE;
  program.options.standard_prelude_listing = A68_FALSE;
  program.options.statistics_listing = A68_FALSE;
  program.options.strict = A68_FALSE;
  program.options.stropping = UPPER_STROPPING;
  program.options.time_limit = 0;
  program.options.trace = A68_FALSE;
  program.options.tree_listing = A68_FALSE;
  program.options.unused = A68_FALSE;
  program.options.verbose = A68_FALSE;
  program.options.version = A68_FALSE;
}

/*!
\brief error handler for options
\param l source line
\param option option text
\param info info text
**/

static void option_error (SOURCE_LINE_T * l, char *option, char *info)
{
  int k;
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", option) >= 0);
  for (k = 0; output_line[k] != NULL_CHAR; k++) {
    output_line[k] = (char) TO_LOWER (output_line[k]);
  }
  if (info != NULL) {
    ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, "%s option \"%s\"", info, output_line) >= 0);
  } else {
    ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, "error in option \"%s\"", output_line) >= 0);
  }
  scan_error (l, NULL, edit_line);
}

/*!
\brief strip minus preceeding a string
\param p text to strip
\return stripped string
**/

static char *strip_sign (char *p)
{
  while (p[0] == '-' || p[0] == '+') {
    p++;
  }
  return (new_string (p));
}

/*!
\brief add an option to the list, to be processed later
\param l option chain to link into
\param str option text
\param line source line
**/

void add_option_list (OPTION_LIST_T ** l, char *str, SOURCE_LINE_T * line)
{
  if (*l == NULL) {
    *l = (OPTION_LIST_T *) get_heap_space ((size_t) ALIGNED_SIZE_OF (OPTION_LIST_T));
    (*l)->scan = program.source_scan;
    (*l)->str = new_string (str);
    (*l)->processed = A68_FALSE;
    (*l)->line = line;
    NEXT (*l) = NULL;
  } else {
    add_option_list (&(NEXT (*l)), str, line);
  }
}

/*!
\brief initialise option handler
\param module current module
**/

void init_options (void)
{
  options = (OPTIONS_T *) malloc ((size_t) ALIGNED_SIZE_OF (OPTIONS_T));
  program.options.list = NULL;
}

/*!
\brief test equality of p and q, upper case letters in q are mandatory
\param module current module
\param p string to match
\param q pattern
\return whether equal
**/

static BOOL_T eq (char *p, char *q)
{
/* Upper case letters in 'q' are mandatory, lower case must match. */
  if (program.options.pragmat_sema) {
    return (match_string (p, q, '='));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief process echoes gathered in the option list
\param module current module
\param i option chain
**/

void prune_echoes (OPTION_LIST_T * i)
{
  while (i != NULL) {
    if (i->scan == program.source_scan) {
      char *p = strip_sign (i->str);
/* ECHO echoes a string. */
      if (eq (p, "ECHO")) {
        {
          char *car = a68g_strchr (p, '=');
          if (car != NULL) {
            io_close_tty_line ();
            ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", &car[1]) >= 0);
            WRITE (STDOUT_FILENO, output_line);
          } else {
            FORWARD (i);
            if (i != NULL) {
              if (strcmp (i->str, "=") == 0) {
                FORWARD (i);
              }
              if (i != NULL) {
                io_close_tty_line ();
                ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", i->str) >= 0);
                WRITE (STDOUT_FILENO, output_line);
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
\brief translate integral option argument
\param p text
\param i option chain
\param error whether error
\return argument value
**/

static int fetch_integral (char *p, OPTION_LIST_T ** i, BOOL_T * error)
{
  SOURCE_LINE_T *start_l = (*i)->line;
  char *start_c = (*i)->str;
  char *car = NULL, *num = NULL;
  int k, mult = 1;
  *error = A68_FALSE;
/* Fetch argument. */
  car = a68g_strchr (p, '=');
  if (car == NULL) {
    FORWARD (*i);
    *error = (BOOL_T) (*i == NULL);
    if (!error && strcmp ((*i)->str, "=") == 0) {
      FORWARD (*i);
      *error = (BOOL_T) ((*i) == NULL);
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
    *error = (BOOL_T) (postfix == num);
    if (errno != 0 || *error) {
      option_error (start_l, start_c, NULL);
      *error = A68_TRUE;
    } else if (k < 0) {
      option_error (start_l, start_c, NULL);
      *error = A68_TRUE;
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
            *error = A68_TRUE;
            break;
          }
        }
        if (postfix[0] != NULL_CHAR && postfix[1] != NULL_CHAR) {
          option_error (start_l, start_c, NULL);
          *error = A68_TRUE;
        }
      }
    }
    if ((double) k * (double) mult > (double) A68_MAX_INT) {
      errno = ERANGE;
      option_error (start_l, start_c, NULL);
    }
    return (k * mult);
  }
}

/*!
\brief process options gathered in the option list
\param module current module
\param i option chain
\param cmd_line whether command line argument
\return whether processing was successful
**/

BOOL_T set_options (OPTION_LIST_T * i, BOOL_T cmd_line)
{
  BOOL_T go_on = A68_TRUE, name_set = A68_FALSE;
  OPTION_LIST_T *j = i;
  RESET_ERRNO;
  while (i != NULL && go_on) {
    SOURCE_LINE_T *start_l = i->line;
    char *start_c = i->str;
    if (!(i->processed)) {
/* Accept UNIX '-option [=] value' */
      BOOL_T minus_sign = (BOOL_T) ((i->str)[0] == '-');
      char *p = strip_sign (i->str);
      if (!minus_sign && cmd_line) {
/* Item without '-'s is a filename. */
        if (!name_set) {
          program.files.initial_name = new_string (p);
          name_set = A68_TRUE;
        } else {
          option_error (NULL, start_c, "will not reset initial file name by");
        }
      }
/* Preprocessor items stop option processing. */
      else if (eq (p, "INCLUDE")) {
        go_on = A68_FALSE;
      } else if (eq (p, "READ")) {
        go_on = A68_FALSE;
      } else if (eq (p, "PREPROCESSOR")) {
        go_on = A68_FALSE;
      } else if (eq (p, "NOPREPROCESSOR")) {
        go_on = A68_FALSE;
      }
/* EXIT stops option processing. */
      else if (eq (p, "EXIT")) {
        go_on = A68_FALSE;
      }
/* Empty item (from specifying '-' or '--') stops option processing. */
      else if (eq (p, "")) {
        go_on = A68_FALSE;
      }
/* FILE accepts its argument as filename. */
      else if (eq (p, "File") && cmd_line) {
        FORWARD (i);
        if (i != NULL && strcmp (i->str, "=") == 0) {
          FORWARD (i);
        }
        if (i != NULL) {
          if (!name_set) {
            program.files.initial_name = new_string (i->str);
            name_set = A68_TRUE;
          } else {
            option_error (start_l, start_c, NULL);
          }
        } else {
          option_error (start_l, start_c, NULL);
        }
      }
/* VERIFY checks that argument is current a68g version number. */
      else if (eq (p, "VERIFY")) {
        FORWARD (i);
        if (i != NULL && strcmp (i->str, "=") == 0) {
          FORWARD (i);
        }
        if (i != NULL) {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s verification \"%s\" does not match script verification \"%s\"", a68g_cmd_name, VERIFICATION, i->str) >= 0);
          ABEND (strcmp (VERIFICATION, i->str) != 0, new_string (output_line), "rebuild the script");
        } else {
          option_error (start_l, start_c, NULL);
        }
      }
/* HELP gives online help. */
      else if ((eq (p, "APropos") || eq (p, "Help") || eq (p, "INfo")) && cmd_line) {
        FORWARD (i);
        if (i != NULL && strcmp (i->str, "=") == 0) {
          FORWARD (i);
        }
        if (i != NULL) {
          apropos (STDOUT_FILENO, NULL, i->str);
        } else {
          apropos (STDOUT_FILENO, NULL, "options");
        }
        a68g_exit (EXIT_SUCCESS);
      }
/* ECHO is treated later. */
      else if (eq (p, "ECHO")) {
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
      else if (eq (p, "Execute") || eq (p, "Print")) {
        if (cmd_line == A68_FALSE) {
          option_error (start_l, start_c, "not at command line when encountering");
        } else if ((FORWARD (i)) != NULL) {
          BOOL_T error = A68_FALSE;
          if (strcmp (i->str, "=") == 0) {
            error = (BOOL_T) ((FORWARD (i)) == NULL);
          }
          if (!error) {
            char name[BUFFER_SIZE];
            FILE *f;
            bufcpy (name, HIDDEN_TEMP_FILE_NAME, BUFFER_SIZE);
            bufcat (name, ".cmd.a68", BUFFER_SIZE);
            f = fopen (name, "w");
            ABEND (f == NULL, "cannot open temp file", NULL);
            if (eq (p, "Execute")) {
              fprintf (f, "(%s)\n", i->str);
            } else {
              fprintf (f, "(print ((%s)))\n", i->str);
            }
            ASSERT (fclose (f) == 0);
            program.files.initial_name = new_string (name);
          } else {
            option_error (start_l, start_c, NULL);
          }
        } else {
          option_error (start_l, start_c, NULL);
        }
      }
/* HEAP, HANDLES, STACK, FRAME and OVERHEAD  set core allocation. */
      else if (eq (p, "HEAP") || eq (p, "HANDLES") || eq (p, "STACK") || eq (p, "FRAME") || eq (p, "OVERHEAD")) {
        BOOL_T error = A68_FALSE;
        int k = fetch_integral (p, &i, &error);
/* Adjust size. */
        if (error || errno > 0) {
          option_error (start_l, start_c, NULL);
        } else if (k > 0) {
          if (k < MIN_MEM_SIZE) {
            option_error (start_l, start_c, NULL);
            k = MIN_MEM_SIZE;
          }
          if (eq (p, "HEAP")) {
            heap_size = k;
          } else if (eq (p, "HANDLE")) {
            handle_pool_size = k;
          } else if (eq (p, "STACK")) {
            expr_stack_size = k;
          } else if (eq (p, "FRAME")) {
            frame_stack_size = k;
          } else if (eq (p, "OVERHEAD")) {
            storage_overhead = k;
          }
        }
      }
/* COMPILE and NOCOMPILE switch on/off compilation. */
      else if (eq (p, "Compile")) {
        program.options.compile = A68_TRUE;
        program.options.optimise = A68_TRUE;
        program.options.run_script = A68_FALSE;
      } else if (eq (p, "NOCompile")) {
        program.options.compile = A68_FALSE;
        program.options.optimise = A68_FALSE;
        program.options.run_script = A68_FALSE;
      } else if (eq (p, "NO-Compile")) {
        program.options.compile = A68_FALSE;
        program.options.optimise = A68_FALSE;
        program.options.run_script = A68_FALSE;
      }
/* OPTIMISE and NOOPTIMISE switch on/off optimisation. */
      else if (eq (p, "Optimise")) {
        program.options.optimise = A68_TRUE;
      } else if (eq (p, "NOOptimise")) {
        program.options.optimise = A68_FALSE;
      } else if (eq (p, "NO-Optimise")) {
        program.options.optimise = A68_FALSE;
      }
/* RUN-SCRIPT runs a comiled .sh script. */
      else if (eq (p, "RUN-SCRIPT")) {
        program.options.run_script = A68_TRUE;
        program.options.compile = A68_FALSE;
        program.options.optimise = A68_TRUE;
      } 
/* RERUN re-uses an existing .so file. */
      else if (eq (p, "RERUN")) {
        program.options.rerun = A68_TRUE;
        program.options.optimise = A68_TRUE;
      } 
/* KEEP and NOKEEP switch off/on object file deletion. */
      else if (eq (p, "KEEP")) {
        program.options.keep = A68_TRUE;
      } else if (eq (p, "NOKEEP")) {
        program.options.keep = A68_FALSE;
      } else if (eq (p, "NO-KEEP")) {
        program.options.keep = A68_FALSE;
      }
/* BRACKETS extends Algol 68 syntax for brackets. */
      else if (eq (p, "BRackets")) {
        program.options.brackets = A68_TRUE;
      }
/* REDUCTIONS gives parser reductions.*/
      else if (eq (p, "REDuctions")) {
        program.options.reductions = A68_TRUE;
      }
/* QUOTESTROPPING sets stropping to quote stropping. */
      else if (eq (p, "QUOTEstropping")) {
        program.options.stropping = QUOTE_STROPPING;
      } else if (eq (p, "QUOTE-stropping")) {
        program.options.stropping = QUOTE_STROPPING;
      }
/* UPPERSTROPPING sets stropping to upper stropping, which is nowadays the expected default. */
      else if (eq (p, "UPPERstropping")) {
        program.options.stropping = UPPER_STROPPING;
      } else if (eq (p, "UPPER-stropping")) {
        program.options.stropping = UPPER_STROPPING;
      }
/* CHECK and NORUN just check for syntax. */
      else if (eq (p, "Check") || eq (p, "NORun") || eq (p, "NO-Run")) {
        program.options.check_only = A68_TRUE;
      }
/* RUN overrides NORUN. */
      else if (eq (p, "RUN")) {
        program.options.run = A68_TRUE;
      }
/* MONITOR or DEBUG invokes the debugger at runtime errors. */
      else if (eq (p, "MONitor") || eq (p, "DEBUG")) {
        program.options.debug = A68_TRUE;
      }
/* REGRESSION is an option that sets preferences when running the Algol68G test suite. */
      else if (eq (p, "REGRESSION")) {
        no_warnings = A68_FALSE;
        program.options.portcheck = A68_TRUE;
        program.options.regression_test = A68_TRUE;
        program.options.time_limit = 30;
        term_width = MAX_LINE_WIDTH;
      }
/* NOWARNINGS switches warnings off. */
      else if (eq (p, "NOWarnings")) {
        no_warnings = A68_TRUE;
      } else if (eq (p, "NO-Warnings")) {
        no_warnings = A68_TRUE;
      }
/* WARNINGS switches warnings on. */
      else if (eq (p, "Warnings")) {
        no_warnings = A68_FALSE;
      }
/* NOPORTCHECK switches portcheck off. */
      else if (eq (p, "NOPORTcheck")) {
        program.options.portcheck = A68_FALSE;
      } else if (eq (p, "NO-PORTcheck")) {
        program.options.portcheck = A68_FALSE;
      }
/* PORTCHECK switches portcheck on. */
      else if (eq (p, "PORTcheck")) {
        program.options.portcheck = A68_TRUE;
      }
/* PEDANTIC switches portcheck and warnings on. */
      else if (eq (p, "PEDANTIC")) {
        program.options.portcheck = A68_TRUE;
        no_warnings = A68_FALSE;
      }
/* PRAGMATS and NOPRAGMATS switch on/off pragmat processing. */
      else if (eq (p, "PRagmats")) {
        program.options.pragmat_sema = A68_TRUE;
      } else if (eq (p, "NOPRagmats")) {
        program.options.pragmat_sema = A68_FALSE;
      } else if (eq (p, "NO-PRagmats")) {
        program.options.pragmat_sema = A68_FALSE;
      }
/* STRICT ignores A68G extensions to A68 syntax. */
      else if (eq (p, "STRict")) {
        program.options.strict = A68_TRUE;
        program.options.portcheck = A68_TRUE;
      }
/* VERBOSE in case you want to know what Algol68G is doing. */
      else if (eq (p, "VERBose")) {
        program.options.verbose = A68_TRUE;
      }
/* VERSION lists the current version at an appropriate time in the future. */
      else if (eq (p, "Version")) {
        program.options.version = A68_TRUE;
      }
/* XREF and NOXREF switch on/off a cross reference. */
      else if (eq (p, "Xref")) {
        program.options.source_listing = A68_TRUE;
        program.options.cross_reference = A68_TRUE;
        program.options.nodemask |= (CROSS_REFERENCE_MASK | SOURCE_MASK);
      } else if (eq (p, "NOXref")) {
        program.options.nodemask &= ~(CROSS_REFERENCE_MASK | SOURCE_MASK);
      } else if (eq (p, "NO-Xref")) {
        program.options.nodemask &= ~(CROSS_REFERENCE_MASK | SOURCE_MASK);
      }
/* PRELUDELISTING cross references preludes, if they ever get implemented ... */
      else if (eq (p, "PRELUDElisting")) {
        program.options.standard_prelude_listing = A68_TRUE;
      }
/* STATISTICS prints process statistics. */
      else if (eq (p, "STatistics")) {
        program.options.statistics_listing = A68_TRUE;
      }
/* TREE and NOTREE switch on/off printing of the syntax tree. This gets bulky! */
      else if (eq (p, "TREE")) {
        program.options.source_listing = A68_TRUE;
        program.options.tree_listing = A68_TRUE;
        program.options.nodemask |= (TREE_MASK | SOURCE_MASK);
      } else if (eq (p, "NOTREE")) {
        program.options.nodemask ^= (TREE_MASK | SOURCE_MASK);
      } else if (eq (p, "NO-TREE")) {
        program.options.nodemask ^= (TREE_MASK | SOURCE_MASK);
      }
/* UNUSED indicates unused tags. */
      else if (eq (p, "UNUSED")) {
        program.options.unused = A68_TRUE;
      }
/* EXTENSIVE set of options for an extensive listing. */
      else if (eq (p, "EXTensive")) {
        program.options.source_listing = A68_TRUE;
        program.options.object_listing = A68_TRUE;
        program.options.tree_listing = A68_TRUE;
        program.options.cross_reference = A68_TRUE;
        program.options.moid_listing = A68_TRUE;
        program.options.standard_prelude_listing = A68_TRUE;
        program.options.statistics_listing = A68_TRUE;
        program.options.unused = A68_TRUE;
        program.options.nodemask |= (CROSS_REFERENCE_MASK | TREE_MASK | CODE_MASK | SOURCE_MASK);
      }
/* LISTING set of options for a default listing. */
      else if (eq (p, "Listing")) {
        program.options.source_listing = A68_TRUE;
        program.options.cross_reference = A68_TRUE;
        program.options.statistics_listing = A68_TRUE;
        program.options.nodemask |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
      }
/* TTY send listing to standout. Remnant from my mainframe past. */
      else if (eq (p, "TTY")) {
        program.options.cross_reference = A68_TRUE;
        program.options.statistics_listing = A68_TRUE;
        program.options.nodemask |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
      }
/* SOURCE and NOSOURCE print source lines. */
      else if (eq (p, "SOURCE")) {
        program.options.source_listing = A68_TRUE;
        program.options.nodemask |= SOURCE_MASK;
      } else if (eq (p, "NOSOURCE")) {
        program.options.nodemask &= ~SOURCE_MASK;
      } else if (eq (p, "NO-SOURCE")) {
        program.options.nodemask &= ~SOURCE_MASK;
      }
/* OBJECT and NOOBJECT print object lines. */
      else if (eq (p, "OBJECT")) {
        program.options.object_listing = A68_TRUE;
      } else if (eq (p, "NOOBJECT")) {
        program.options.object_listing = A68_FALSE;
      } else if (eq (p, "NO-OBJECT")) {
        program.options.object_listing = A68_FALSE;
      }
/* MOIDS prints an overview of moids used in the program. */
      else if (eq (p, "MOIDS")) {
        program.options.moid_listing = A68_TRUE;
      }
/* ASSERTIONS and NOASSERTIONS switch on/off the processing of assertions. */
      else if (eq (p, "Assertions")) {
        program.options.nodemask |= ASSERT_MASK;
      } else if (eq (p, "NOAssertions")) {
        program.options.nodemask &= ~ASSERT_MASK;
      } else if (eq (p, "NO-Assertions")) {
        program.options.nodemask &= ~ASSERT_MASK;
      }
/* PRECISION sets the precision. */
      else if (eq (p, "PRECision")) {
        BOOL_T error = A68_FALSE;
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
/* BACKTRACE and NOBACKTRACE switch on/off stack backtracing. */
      else if (eq (p, "BACKtrace")) {
        program.options.backtrace = A68_TRUE;
      } else if (eq (p, "NOBACKtrace")) {
        program.options.backtrace = A68_FALSE;
      } else if (eq (p, "NO-BACKtrace")) {
        program.options.backtrace = A68_FALSE;
      }
/* BREAK and NOBREAK switch on/off tracing of the running program. */
      else if (eq (p, "BReakpoint")) {
        program.options.nodemask |= BREAKPOINT_MASK;
      } else if (eq (p, "NOBReakpoint")) {
        program.options.nodemask &= ~BREAKPOINT_MASK;
      } else if (eq (p, "NO-BReakpoint")) {
        program.options.nodemask &= ~BREAKPOINT_MASK;
      }
/* TRACE and NOTRACE switch on/off tracing of the running program. */
      else if (eq (p, "TRace")) {
        program.options.trace = A68_TRUE;
        program.options.nodemask |= BREAKPOINT_TRACE_MASK;
      } else if (eq (p, "NOTRace")) {
        program.options.nodemask &= ~BREAKPOINT_TRACE_MASK;
      } else if (eq (p, "NO-TRace")) {
        program.options.nodemask &= ~BREAKPOINT_TRACE_MASK;
      }
/* TIMELIMIT lets the interpreter stop after so-many seconds. */
      else if (eq (p, "TImelimit") || eq (p, "TIME-Limit")) {
        BOOL_T error = A68_FALSE;
        int k = fetch_integral (p, &i, &error);
        if (error || errno > 0) {
          option_error (start_l, start_c, NULL);
        } else if (k < 1) {
          option_error (start_l, start_c, NULL);
        } else {
          program.options.time_limit = k;
        }
      } else {
/* Unrecognised. */
        option_error (start_l, start_c, "unrecognised");
      }
    }
/* Go processing next item, if present. */
    if (i != NULL) {
      FORWARD (i);
    }
  }
/* Mark options as processed. */
  for (; j != NULL; FORWARD (j)) {
    j->processed = A68_TRUE;
  }
  return ((BOOL_T) (errno == 0));
}

/*!
\brief set default core size
**/

void default_mem_sizes (void)
{
  frame_stack_size = 3 * MEGABYTE;
  expr_stack_size = MEGABYTE;
  heap_size = 24 * MEGABYTE;
  handle_pool_size = 4 * MEGABYTE;
  storage_overhead = 512 * KILOBYTE;
}

/*!
\brief read options from the .rc file
\param module current module
**/

void read_rc_options (void)
{
  FILE *f;
  int len = 2 + (int) strlen (a68g_cmd_name) + (int) strlen ("rc");
  char *name = (char *) get_heap_space ((size_t) len);
  bufcpy (name, ".", len);
  bufcat (name, a68g_cmd_name, len);
  bufcat (name, "rc", len);
  f = fopen (name, "r");
  if (f != NULL) {
    while (!feof (f)) {
      if (fgets (input_line, BUFFER_SIZE, f) != NULL) {
        if (input_line[strlen (input_line) - 1] == NEWLINE_CHAR) {
          input_line[strlen (input_line) - 1] = NULL_CHAR;
        }
        isolate_options (input_line, NULL);
      }
    }
    ASSERT (fclose (f) == 0);
    (void) set_options (program.options.list, A68_FALSE);
  } else {
    errno = 0;
  }
}

/*!
\brief read options from A68G_OPTIONS
\param module current module
**/

void read_env_options (void)
{
  if (getenv ("A68G_OPTIONS") != NULL) {
    isolate_options (getenv ("A68G_OPTIONS"), NULL);
    (void) set_options (program.options.list, A68_FALSE);
    errno = 0;
  }
}

/*!
\brief tokenise string 'p' that holds options
\param module current module
\param p text
\param line source line
**/

void isolate_options (char *p, SOURCE_LINE_T * line)
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
          /* Skip item. */
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
      add_option_list (&(program.options.list), q, line);
    }
  }
}
