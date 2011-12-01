/*!
\file algol68g.c
\brief driver routines for the compiler-interpreter.
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
Algol68G is an Algol 68 compiler-interpreter.

Please refer to the documentation that comes with this distribution for a
detailed description of Algol68G.
*/

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

int global_argc; /* Keep argc and argv for reference from A68 */
char **global_argv;

#if (defined HAVE_TERM_H && defined HAVE_LIBTERMCAP)
#include <term.h>
char term_buffer[2 * KILOBYTE];
char *term_type;
#endif

BOOL_T in_execution;
BYTE_T *system_stack_offset;
MODES_T a68_modes;
MODULE_T program;
NODE_T **node_register = NO_VAR;
char a68g_cmd_name[BUFFER_SIZE];
clock_t clock_res;
int new_nodes, new_modes, new_postulates, new_node_infos, new_genie_infos;
int stack_size;
int symbol_table_count, mode_count;
int term_width;
static int max_simplout_size;
static POSTULATE_T *postulates;

#define EXTENSIONS 11
static char * extensions[EXTENSIONS] = {
  NO_TEXT,
  ".a68", ".A68",
  ".a68g", ".A68G",
  ".algol", ".ALGOL",
  ".algol68", ".ALGOL68",
  ".algol68g", ".ALGOL68G"
};

static void announce_phase (char *);
static void compiler_interpreter (void);
static void moid_to_string_2 (char *, MOID_T *, int *, NODE_T *);

#if defined HAVE_COMPILER
static void build_script (void);
static void load_script (void);
static void rewrite_script_source (void);
#endif

/*!
\brief print k bytes from z; debugging routine
\param z byte string
\param k length
**/

void print_bytes (BYTE_T *z, int k)
{
  int j;
  for (j = 0; j < k; j ++) {
    printf ("%02x ", z[j]);
    }
  printf ("\n");
  ASSERT (fflush (stdout) == 0); /* print_bytes */
}

/*!
\brief unformatted write of z to stdout
\param str prompt
\param z mp number to print
\param digits precision in mp-digits
**/

void raw_write_mp (char *str, MP_T * z, int digits)
{
  int i;
  printf ("\n%s", str);
  for (i = 1; i <= digits; i++) {
    printf (" %07d", (int) MP_DIGIT (z, i));
  }
  printf (" ^ %d", (int) MP_EXPONENT (z));
  printf (" status=%d", (int) MP_STATUS (z));
  ASSERT (fflush (stdout) == 0); /* raw_write_mp */
}

/*!
\brief state license of running a68g image
\param f file number to write to
**/

void state_license (FILE_T f)
{
#define PR(s)\
  ASSERT (snprintf(output_line, SNPRINTF_SIZE, "%s\n", (s)) >= 0);\
  WRITE (f, output_line);
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Algol 68 Genie %s\n", PACKAGE_VERSION) >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Copyright (c) 2011 %s.\n", PACKAGE_BUGREPORT) >= 0);
  WRITE (f, output_line);
  PR ("");
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "This is free software covered by the GNU General Public License.\n") >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "There is ABSOLUTELY NO WARRANTY for Algol 68 Genie;\n") >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n") >= 0);
  WRITE (f, output_line);
  PR ("See the GNU General Public License for more details.");
  PR ("");
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Please report bugs to %s.\n", PACKAGE_BUGREPORT) >= 0);
  WRITE (f, output_line);
  PR ("");
#undef PR
}

/*!
\brief state version of running a68g image
\param f file number to write to
**/

void state_version (FILE_T f)
{
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  state_license (f);
  WRITELN (f, "");
#if defined HAVE_WIN32
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "This is a WIN32 executable.\n") >= 0);
  WRITE (f, output_line);
#endif
#if defined HAVE_COMPILER
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Compilation is supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Compilation is not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
#if defined HAVE_EDITOR
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Editor is supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Editor is not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Parallel-clause is supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Parallel-clause is not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES)
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Curses is supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Curses is not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
#if defined HAVE_REGEX_H
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Regular expressions are supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Regular expressions are not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
#if defined HAVE_HTTP
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "TCP/IP is supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "TCP/IP is not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
#if (defined HAVE_PLOT_H && defined HAVE_LIBPLOT)
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "GNU libplot is supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "GNU libplot is not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
#if (defined HAVE_GSL_GSL_BLAS_H && defined HAVE_LIBGSL)
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "GNU Scientific Library is supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "GNU Scientific Library is not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
#if (defined HAVE_LIBPQ_FE_H && defined HAVE_LIBPQ)
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "PostgreSQL is supported.\n") >= 0);
#else
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "PostgreSQL is not supported.\n") >= 0);
#endif
  WRITE (f, output_line);
}

/*!
\brief give brief help if someone types 'a68g --help'
\param f file number
**/

void online_help (FILE_T f)
{
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  state_license (f);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Usage: %s [options | filename]", a68g_cmd_name) >= 0);
  WRITELN (f, output_line);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "For help: %s --apropos [keyword]", a68g_cmd_name) >= 0);
  WRITELN (f, output_line);
}

/*!
\brief first initialisations
**/

static void init_before_tokeniser (void)
{
/* Heap management set-up */
  init_heap ();
  top_keyword = NO_KEYWORD;
  top_token = NO_TOKEN;
  TOP_NODE (&program) = NO_NODE;
  TOP_MOID (&program) = NO_MOID;
  TOP_LINE (&program) = NO_LINE;
  STANDENV_MOID (&program) = NO_MOID;
  set_up_tables ();
/* Various initialisations */
  ERROR_COUNT (&program) = WARNING_COUNT (&program) = 0;
  RESET_ERRNO;
}

/*!
\brief main entry point
\param argc arg count
\param argv arg string
\return exit code
**/

int main (int argc, char *argv[])
{
  BYTE_T stack_offset;
  int argcc, k;
  global_argc = argc;
  global_argv = argv;
  FILE_DIAGS_FD (&program) = -1;
/* Get command name and discard path */
  bufcpy (a68g_cmd_name, argv[0], BUFFER_SIZE);
  for (k = (int) strlen (a68g_cmd_name) - 1; k >= 0; k--) {
#if defined WIN32
    char delim = '\\';
#else
    char delim = '/';
#endif
    if (a68g_cmd_name[k] == delim) {
      MOVE (&a68g_cmd_name[0], &a68g_cmd_name[k + 1], (int) strlen (a68g_cmd_name) - k + 1);
      k = -1;
    }
  }
/* Try to read maximum line width on the terminal,
   used to pretty print diagnostics to same.
 */
#if (defined HAVE_TERM_H && defined HAVE_LIBTERMCAP)
  term_type = getenv ("TERM");
  if (term_type == NO_TEXT) {
    term_width = MAX_LINE_WIDTH;
  } else if (tgetent (term_buffer, term_type) < 0) {
    term_width = MAX_LINE_WIDTH;
  } else {
    term_width = tgetnum ("co");
  }
  if (term_width <= 1) {
    term_width = MAX_LINE_WIDTH;
  }
#else
  term_width = MAX_LINE_WIDTH;
#endif
/* Determine clock resolution */
  {
    clock_t t0 = clock (), t1;
    do {
      t1 = clock ();
    } while (t1 == t0);
    clock_res = (t1 - t0) / (clock_t) CLOCKS_PER_SEC;
  }
/* Set the main thread id */
#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
  main_thread_id = pthread_self ();
#endif
  heap_is_fluid = A68_TRUE;
  system_stack_offset = &stack_offset;
  init_file_entries ();
  if (!setjmp (RENDEZ_VOUS (&program))) {
    init_tty ();
/* Initialise option handling */
    init_options ();
    SOURCE_SCAN (&program) = 1;
    default_options (&program);
    default_mem_sizes ();
/* Initialise core */
    stack_segment = NO_BYTE;
    heap_segment = NO_BYTE;
    handle_segment = NO_BYTE;
    get_stack_size ();
/* Well, let's start */
    TOP_REFINEMENT (&program) = NO_REFINEMENT;
    FILE_INITIAL_NAME (&program) = NO_TEXT;
    FILE_GENERIC_NAME (&program) = NO_TEXT;
    FILE_SOURCE_NAME (&program) = NO_TEXT;
    FILE_LISTING_NAME (&program) = NO_TEXT;
    FILE_OBJECT_NAME (&program) = NO_TEXT;
    FILE_LIBRARY_NAME (&program) = NO_TEXT;
    FILE_BINARY_NAME (&program) = NO_TEXT;
    FILE_SCRIPT_NAME (&program) = NO_TEXT;
    FILE_DIAGS_NAME (&program) = NO_TEXT;
/* Options are processed here */
    read_rc_options ();
    read_env_options ();
/* Posix copies arguments from the command line */
    if (argc <= 1) {
      online_help (STDOUT_FILENO);
      a68g_exit (EXIT_FAILURE);
    }
    for (argcc = 1; argcc < argc; argcc++) {
      add_option_list (&(OPTION_LIST (&program)), argv[argcc], NO_LINE);
    }
    if (!set_options (OPTION_LIST (&program), A68_TRUE)) {
      a68g_exit (EXIT_FAILURE);
    }
    if (OPTION_REGRESSION_TEST (&program)) {
      bufcpy (a68g_cmd_name, "a68g", BUFFER_SIZE);
    }
/* Attention for --version */
    if (OPTION_VERSION (&program)) {
      state_version (STDOUT_FILENO);
    }
/* Start the UI */
    init_before_tokeniser ();
    if (OPTION_EDIT (&program)) {
#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES)
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Algol 68 Genie %s\n", PACKAGE_VERSION) >= 0);
      edit (output_line);
#else 
      errno = ENOTSUP;
      SCAN_ERROR (A68_TRUE, NO_LINE, NO_TEXT, "EDIT requires the ncurses library");
#endif
    }
/* Running a script */
#if defined HAVE_COMPILER
    if (OPTION_RUN_SCRIPT (&program)) {
      load_script ();
    }
#endif
/* We translate the program */
    if (FILE_INITIAL_NAME (&program) == NO_TEXT || strlen (FILE_INITIAL_NAME (&program)) == 0) {
      SCAN_ERROR (!OPTION_VERSION (&program), NO_LINE, NO_TEXT, ERROR_NO_SOURCE_FILE);
    } else {
      compiler_interpreter ();
    }
    a68g_exit (ERROR_COUNT (&program) == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    return (EXIT_SUCCESS);
  } else {
    diagnostics_to_terminal (TOP_LINE (&program), A68_ALL_DIAGNOSTICS);
    a68g_exit (EXIT_FAILURE);
    return (EXIT_FAILURE);
  }
}

/*!
\brief test extension and strip
\param ext extension to try
\return whether stripped
**/

static BOOL_T strip_extension (char * ext)
{
  int nlen, xlen;
  if (ext == NO_TEXT) {
    return (A68_FALSE);
  }
  nlen = (int) strlen (FILE_SOURCE_NAME (&program)); 
  xlen = (int) strlen (ext);
  if (nlen > xlen && strcmp (&(FILE_SOURCE_NAME (&program)[nlen - xlen]), ext) == 0) { 
    char *fn = (char *) get_heap_space ((size_t) (nlen + 1));
    bufcpy (fn, FILE_SOURCE_NAME (&program), nlen);
    fn[nlen - xlen] = NULL_CHAR;
    FILE_GENERIC_NAME (&program) = new_string (fn);
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief try opening with an extension
**/

static void open_with_extensions (void)
{
  int k;
  FILE_SOURCE_FD (&program) = -1;
  for (k = 0; k < EXTENSIONS && FILE_SOURCE_FD (&program) == -1; k ++) {
    int len;
    char * fn;
    if (extensions[k] == NO_TEXT) {
      len = (int) strlen (FILE_INITIAL_NAME (&program)) + 1;
      fn = (char *) get_heap_space ((size_t) len);
      bufcpy (fn, FILE_INITIAL_NAME (&program), len);
    } else {
      len = (int) strlen (FILE_INITIAL_NAME (&program)) + (int) strlen (extensions[k]) + 1;
      fn = (char *) get_heap_space ((size_t) len);
      bufcpy (fn, FILE_INITIAL_NAME (&program), len);
      bufcat (fn, extensions[k], len);
    }
    FILE_SOURCE_FD (&program) = open (fn, O_RDONLY | O_BINARY);
    if (FILE_SOURCE_FD (&program) != -1) {
      int l;
      BOOL_T cont = A68_TRUE;
      FILE_SOURCE_NAME (&program) = new_string (fn);
      FILE_GENERIC_NAME (&program) = new_string (fn);
      for (l = 0; l < EXTENSIONS && cont; l ++) {
        if (strip_extension (extensions[l])) {
          cont = A68_FALSE;
        }
      }
    }
  }
}

/*!
\brief pretty print memory size
**/

char *pretty_size (int k) {
  if (k >= 10 * MEGABYTE) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%dM", k / MEGABYTE) >= 0);
  } else if (k >= 10 * KILOBYTE) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%dk", k / KILOBYTE) >= 0);
  } else {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%d", k) >= 0);
  }
  return (edit_line);
}

/*!
\brief verbose statistics, only useful when debugging a68g
**/

static void verbosity (void)
{
  ;
}

/*!
\brief drives compilation and interpretation
**/

static void compiler_interpreter (void)
{
  int k, len, num;
  BOOL_T path_set = A68_FALSE;
  BOOL_T emitted = A68_FALSE;
  TREE_LISTING_SAFE (&program) = A68_FALSE;
  CROSS_REFERENCE_SAFE (&program) = A68_FALSE;
  in_execution = A68_FALSE;
  new_nodes = 0;
  new_modes = 0;
  new_postulates = 0;
  new_node_infos = 0;
  new_genie_infos = 0;
  init_postulates ();
/* File set-up */
  SCAN_ERROR (FILE_INITIAL_NAME (&program) == NO_TEXT, NO_LINE, NO_TEXT, ERROR_NO_SOURCE_FILE);
  FILE_BINARY_OPENED (&program) = A68_FALSE;
  FILE_BINARY_WRITEMOOD (&program) = A68_TRUE;
  FILE_LIBRARY_OPENED (&program) = A68_FALSE;
  FILE_LIBRARY_WRITEMOOD (&program) = A68_TRUE;
  FILE_LISTING_OPENED (&program) = A68_FALSE;
  FILE_LISTING_WRITEMOOD (&program) = A68_TRUE;
  FILE_OBJECT_OPENED (&program) = A68_FALSE;
  FILE_OBJECT_WRITEMOOD (&program) = A68_TRUE;
  FILE_SCRIPT_OPENED (&program) = A68_FALSE;
  FILE_SCRIPT_WRITEMOOD (&program) = A68_FALSE;
  FILE_SOURCE_OPENED (&program) = A68_FALSE;
  FILE_SOURCE_WRITEMOOD (&program) = A68_FALSE;
  FILE_DIAGS_OPENED (&program) = A68_FALSE;
  FILE_DIAGS_WRITEMOOD (&program) = A68_TRUE;
/*
Open the source file. 
Open it for binary reading for systems that require so (Win32).
Accept various silent extensions.
*/
  errno = 0;
  FILE_SOURCE_NAME (&program) = NO_TEXT;
  FILE_GENERIC_NAME (&program) = NO_TEXT;
  open_with_extensions ();
  if (FILE_SOURCE_FD (&program) == -1) {
    scan_error (NO_LINE, NO_TEXT, ERROR_SOURCE_FILE_OPEN);
  }
  ABEND (FILE_SOURCE_NAME (&program) == NO_TEXT, "no source file name", NO_TEXT);
  ABEND (FILE_GENERIC_NAME (&program) == NO_TEXT, "no generic file name", NO_TEXT);
/* Isolate the path name */
  FILE_PATH (&program) = new_string (FILE_GENERIC_NAME (&program));
  path_set = A68_FALSE;
  for (k = (int) strlen (FILE_PATH (&program)); k >= 0 && path_set == A68_FALSE; k--) {
#if defined WIN32
    char delim = '\\';
#else
    char delim = '/';
#endif
    if (FILE_PATH (&program)[k] == delim) {
      FILE_PATH (&program)[k + 1] = NULL_CHAR;
      path_set = A68_TRUE;
    }
  }
  if (path_set == A68_FALSE) {
    FILE_PATH (&program)[0] = NULL_CHAR;
  }
/* Object file */
  len = 1 + (int) strlen (FILE_GENERIC_NAME (&program)) + (int) strlen (OBJECT_EXTENSION);
  FILE_OBJECT_NAME (&program) = (char *) get_heap_space ((size_t) len);
  bufcpy (FILE_OBJECT_NAME (&program), FILE_GENERIC_NAME (&program), len);
  bufcat (FILE_OBJECT_NAME (&program), OBJECT_EXTENSION, len);
/* Binary */
  len = 1 + (int) strlen (FILE_GENERIC_NAME (&program)) + (int) strlen (LIBRARY_EXTENSION);
  FILE_BINARY_NAME (&program) = (char *) get_heap_space ((size_t) len);
  bufcpy (FILE_BINARY_NAME (&program), FILE_GENERIC_NAME (&program), len);
  bufcat (FILE_BINARY_NAME (&program), BINARY_EXTENSION, len);
/* Library file */
  len = 1 + (int) strlen (FILE_GENERIC_NAME (&program)) + (int) strlen (LIBRARY_EXTENSION);
  FILE_LIBRARY_NAME (&program) = (char *) get_heap_space ((size_t) len);
  bufcpy (FILE_LIBRARY_NAME (&program), FILE_GENERIC_NAME (&program), len);
  bufcat (FILE_LIBRARY_NAME (&program), LIBRARY_EXTENSION, len);
/* Listing file */
  len = 1 + (int) strlen (FILE_GENERIC_NAME (&program)) + (int) strlen (LISTING_EXTENSION);
  FILE_LISTING_NAME (&program) = (char *) get_heap_space ((size_t) len);
  bufcpy (FILE_LISTING_NAME (&program), FILE_GENERIC_NAME (&program), len);
  bufcat (FILE_LISTING_NAME (&program), LISTING_EXTENSION, len);
/* Script file */
  len = 1 + (int) strlen (FILE_GENERIC_NAME (&program)) + (int) strlen (SCRIPT_EXTENSION);
  FILE_SCRIPT_NAME (&program) = (char *) get_heap_space ((size_t) len);
  bufcpy (FILE_SCRIPT_NAME (&program), FILE_GENERIC_NAME (&program), len);
  bufcat (FILE_SCRIPT_NAME (&program), SCRIPT_EXTENSION, len);
/* Tokeniser */
  FILE_SOURCE_OPENED (&program) = A68_TRUE;
  announce_phase ("initialiser");
  error_tag = (TAG_T *) new_tag ();
  if (ERROR_COUNT (&program) == 0) {
    int frame_stack_size_2 = frame_stack_size;
    int expr_stack_size_2 = expr_stack_size;
    int heap_size_2 = heap_size;
    int handle_pool_size_2 = handle_pool_size;
    BOOL_T ok;
    announce_phase ("tokeniser");
    ok = lexical_analyser ();
    if (!ok || errno != 0) {
      diagnostics_to_terminal (TOP_LINE (&program), A68_ALL_DIAGNOSTICS);
      return;
    }
/* Maybe the program asks for more memory through a PRAGMAT. We restart */
    if (frame_stack_size_2 != frame_stack_size || expr_stack_size_2 != expr_stack_size || heap_size_2 != heap_size || handle_pool_size_2 != handle_pool_size) {
      discard_heap ();
      init_before_tokeniser ();
      SOURCE_SCAN (&program) ++;
      ok = lexical_analyser ();
      verbosity ();
    }
    if (!ok || errno != 0) {
      diagnostics_to_terminal (TOP_LINE (&program), A68_ALL_DIAGNOSTICS);
      return;
    }
    ASSERT (close (FILE_SOURCE_FD (&program)) == 0);
    FILE_SOURCE_OPENED (&program) = A68_FALSE;
    prune_echoes (OPTION_LIST (&program));
    TREE_LISTING_SAFE (&program) = A68_TRUE;
    num = 0;
    renumber_nodes (TOP_NODE (&program), &num);
  }
/* Final initialisations */
  if (ERROR_COUNT (&program) == 0) {
    a68g_standenv = NO_TABLE;
    init_postulates ();
    mode_count = 0;
    make_special_mode (&MODE (HIP), mode_count++);
    make_special_mode (&MODE (UNDEFINED), mode_count++);
    make_special_mode (&MODE (ERROR), mode_count++);
    make_special_mode (&MODE (VACUUM), mode_count++);
    make_special_mode (&MODE (C_STRING), mode_count++);
    make_special_mode (&MODE (COLLITEM), mode_count++);
    make_special_mode (&MODE (SOUND_DATA), mode_count++);
  }
/* Refinement preprocessor */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("preprocessor");
    get_refinements ();
    if (ERROR_COUNT (&program) == 0) {
      put_refinements ();
    }
    num = 0;
    renumber_nodes (TOP_NODE (&program), &num);
    verbosity ();
  }
/* Top-down parser */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("parser phase 1");
    check_parenthesis (TOP_NODE (&program));
    if (ERROR_COUNT (&program) == 0) {
      if (OPTION_BRACKETS (&program)) {
        substitute_brackets (TOP_NODE (&program));
      }
      symbol_table_count = 0;
      a68g_standenv = new_symbol_table (NO_TABLE);
      LEVEL (a68g_standenv) = 0;
      top_down_parser (TOP_NODE (&program));
    }
    num = 0;
    renumber_nodes (TOP_NODE (&program), &num);
    verbosity ();
  }
/* Standard environment builder */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("standard environ builder");
    TABLE (TOP_NODE (&program)) = new_symbol_table (a68g_standenv);
    make_standard_environ ();
    STANDENV_MOID (&program) = TOP_MOID (&program);
    verbosity ();
  }
/* Bottom-up parser */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("parser phase 2");
    preliminary_symbol_table_setup (TOP_NODE (&program));
    bottom_up_parser (TOP_NODE (&program));
    num = 0;
    renumber_nodes (TOP_NODE (&program), &num);
    verbosity ();
  }
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("parser phase 3");
    bottom_up_error_check (TOP_NODE (&program));
    victal_checker (TOP_NODE (&program));
    if (ERROR_COUNT (&program) == 0) {
      finalise_symbol_table_setup (TOP_NODE (&program), 2);
      NEST (TABLE (TOP_NODE (&program))) = symbol_table_count = 3;
      reset_symbol_table_nest_count (TOP_NODE (&program));
      fill_symbol_table_outer (TOP_NODE (&program), TABLE (TOP_NODE (&program)));
#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
      set_par_level (TOP_NODE (&program), 0);
#endif
      set_nest (TOP_NODE (&program), NO_NODE);
      set_proc_level (TOP_NODE (&program), 1);
    }
    num = 0;
    renumber_nodes (TOP_NODE (&program), &num);
    verbosity ();
  }
/* Mode table builder */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("mode table builder");
    make_moid_list (&program);
    verbosity ();
  }
  CROSS_REFERENCE_SAFE (&program) = A68_TRUE;
/* Symbol table builder */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("symbol table builder");
    collect_taxes (TOP_NODE (&program));
    verbosity ();
  }
/* Post parser */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("parser phase 4");
    rearrange_goto_less_jumps (TOP_NODE (&program));
    num = 0;
/*    renumber_nodes (TOP_NODE (&program), &num); */
    verbosity ();
  }
/* Mode checker */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("mode checker");
    mode_checker (TOP_NODE (&program));
/*    renumber_moids (TOP_MOID (&program), 0); */
    verbosity ();
  }
/* Coercion inserter */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("coercion enforcer");
    coercion_inserter (TOP_NODE (&program));
    widen_denotation (TOP_NODE (&program));
    protect_from_gc (TOP_NODE (&program));
    get_max_simplout_size (TOP_NODE (&program));
    set_moid_sizes (TOP_MOID (&program));
    assign_offsets_table (a68g_standenv);
    assign_offsets (TOP_NODE (&program));
    assign_offsets_packs (TOP_MOID (&program));
    num = 0;
    renumber_nodes (TOP_NODE (&program), &num);
    verbosity ();
  }
/* Application checker */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("application checker");
    mark_moids (TOP_NODE (&program));
    mark_auxilliary (TOP_NODE (&program));
    jumps_from_procs (TOP_NODE (&program));
    warn_for_unused_tags (TOP_NODE (&program));
    verbosity ();
  }
/* Scope checker */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("static scope checker");
    tie_label_to_serial (TOP_NODE (&program));
    tie_label_to_unit (TOP_NODE (&program));
    bind_routine_tags_to_tree (TOP_NODE (&program));
    bind_format_tags_to_tree (TOP_NODE (&program));
    scope_checker (TOP_NODE (&program));
    verbosity ();
  }
/* Portability checker */
  if (ERROR_COUNT (&program) == 0) {
    announce_phase ("portability checker");
    portcheck (TOP_NODE (&program));
    verbosity ();
  }
/* Finalise syntax tree */
  if (ERROR_COUNT (&program) == 0) {
    num = 0;
    renumber_nodes (TOP_NODE (&program), &num);
    NEST (TABLE (TOP_NODE (&program))) = symbol_table_count = 3;
    reset_symbol_table_nest_count (TOP_NODE (&program));
    verbosity ();
  }
/* Compiler */
  if (ERROR_COUNT (&program) == 0 && OPTION_OPTIMISE (&program)) {
    announce_phase ("optimiser (code generator)");
    num = 0;
    renumber_nodes (TOP_NODE (&program), &num);
    node_register = (NODE_T **) get_heap_space ((size_t) num * sizeof (NODE_T));
    ABEND (node_register == NO_VAR, "compiler cannot register nodes", NO_TEXT);
    register_nodes (TOP_NODE (&program)); 
    FILE_OBJECT_FD (&program) = open (FILE_OBJECT_NAME (&program), O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
    ABEND (FILE_OBJECT_FD (&program) == -1, "cannot open object file", NO_TEXT);
    FILE_OBJECT_OPENED (&program) = A68_TRUE;
    compiler (FILE_OBJECT_FD (&program));
    ASSERT (close (FILE_OBJECT_FD (&program)) == 0);
    FILE_OBJECT_OPENED (&program) = A68_FALSE;
    emitted = A68_TRUE;
  }
#if defined HAVE_COMPILER
/* Only compile C if the A68 compiler found no errors (constant folder for instance) */
  if (ERROR_COUNT (&program) == 0 && OPTION_OPTIMISE (&program) && !OPTION_RUN_SCRIPT (&program)) {
    char cmd[BUFFER_SIZE], options[BUFFER_SIZE], *optimisation, extra_inc[BUFFER_SIZE];
#if defined HAVE_USR_LOCAL_PGSQL_INCLUDE
    ASSERT (snprintf (extra_inc, SNPRINTF_SIZE, "-I. -I%s -I/usr/local/pgsql/include", INCLUDEDIR) >= 0);
#elif defined HAVE_USR_PKG_PGSQL_INCLUDE
    ASSERT (snprintf (extra_inc, SNPRINTF_SIZE, "-I. -I%s -I/usr/pkg/pgsql/include", INCLUDEDIR) >= 0);
#elif defined HAVE_OPT_LOCAL_PGSQL_INCLUDE
    ASSERT (snprintf (extra_inc, SNPRINTF_SIZE, "-I. -I%s -I/opt/local/pgsql/include", INCLUDEDIR) >= 0);
#else
    ASSERT (snprintf (extra_inc, SNPRINTF_SIZE, "-I. -I%s", INCLUDEDIR) >= 0);
#endif
    switch (OPTION_OPT_LEVEL (&program)) {
      case 0: {
        optimisation = "-O0";
        break;
      }
      case 1: {
        optimisation = "-O1";
        break;
      }
      case 2: {
        optimisation = "-O2";
        break;
      }
      case 3: {
        optimisation = "-O3";
        break;
      }
      default: {
        optimisation = "-O2";
        break;
      }
    }
    if (OPTION_RERUN (&program) == A68_FALSE) {
      announce_phase ("optimiser (code compiler)");

/*-------------------------------------------------------------+
| Build shared library using gcc.                              |
| TODO: One day this should be all portable between platforms. |
+-------------------------------------------------------------*/

/*
Compilation on Linux, FreeBSD or NetBSD using gcc
*/
#if (defined HAVE_LINUX || defined HAVE_FREEBSD || defined HAVE_NETBSD)
  #if defined HAVE_TUNING
      ASSERT (snprintf (options, SNPRINTF_SIZE, "%s %s %s -g", extra_inc, optimisation, HAVE_TUNING) >= 0);
  #else
      ASSERT (snprintf (options, SNPRINTF_SIZE, "%s %s -g", extra_inc, optimisation) >= 0);
  #endif
  #if defined HAVE_PIC
      bufcat (options, " ", BUFFER_SIZE);
      bufcat (options, HAVE_PIC, BUFFER_SIZE);
  #endif
      ASSERT (snprintf (cmd, SNPRINTF_SIZE, "gcc %s -c -o \"%s\" \"%s\"", options, FILE_BINARY_NAME (&program), FILE_OBJECT_NAME (&program)) >= 0);
      if (OPTION_VERBOSE (&program)) {
        WRITELN (STDOUT_FILENO, cmd);
      }
      ABEND (system (cmd) != 0, "cannot compile", cmd);
      ASSERT (snprintf (cmd, SNPRINTF_SIZE, "ld -export-dynamic -shared -o \"%s\" \"%s\"", FILE_LIBRARY_NAME (&program), FILE_BINARY_NAME (&program)) >= 0);
      if (OPTION_VERBOSE (&program)) {
        WRITELN (STDOUT_FILENO, cmd);
      }
      ABEND (system (cmd) != 0, "cannot link", cmd);
      ABEND (remove (FILE_BINARY_NAME (&program)) != 0, "cannot remove", cmd);
/*
Compilation on Mac OS X using gcc
*/
#elif defined HAVE_MAC_OS_X
  #if defined HAVE_TUNING
      ASSERT (snprintf (options, SNPRINTF_SIZE, "%s %s %s -g -fno-common -dynamic", extra_inc, optimisation, HAVE_TUNING) >= 0);
  #else
      ASSERT (snprintf (options, SNPRINTF_SIZE, "%s %s -g -fno-common -dynamic", extra_inc, optimisation) >= 0);
  #endif
  #if defined HAVE_PIC
      bufcat (options, " ", BUFFER_SIZE);
      bufcat (options, HAVE_PIC, BUFFER_SIZE);
  #endif
      ASSERT (snprintf (cmd, SNPRINTF_SIZE, "gcc %s -c -o \"%s\" \"%s\"", options, FILE_BINARY_NAME (&program), FILE_OBJECT_NAME (&program)) >= 0);
      if (OPTION_VERBOSE (&program)) {
        WRITELN (STDOUT_FILENO, cmd);
      }
      ABEND (system (cmd) != 0, "cannot compile", cmd);
      ASSERT (snprintf (cmd, SNPRINTF_SIZE, "libtool -dynamic -flat_namespace -undefined suppress -o %s %s", FILE_LIBRARY_NAME (&program), FILE_BINARY_NAME (&program)) >= 0);
      if (OPTION_VERBOSE (&program)) {
        WRITELN (STDOUT_FILENO, cmd);
      }
      ABEND (system (cmd) != 0, "cannot link", cmd);
      ABEND (remove (FILE_BINARY_NAME (&program)) != 0, "cannot remove", cmd);
#endif
    }
    verbosity ();
  }
#else
  if (OPTION_OPTIMISE (&program)) {
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, TOP_NODE (&program), WARNING_OPTIMISATION);
  }
#endif
/* Interpreter */
  diagnostics_to_terminal (TOP_LINE (&program), A68_ALL_DIAGNOSTICS);
  if (ERROR_COUNT (&program) == 0 && OPTION_COMPILE (&program) == A68_FALSE && (OPTION_CHECK_ONLY (&program) ? OPTION_RUN (&program) : A68_TRUE)) {
#if defined HAVE_COMPILER
  void * compile_lib;
#endif
#if defined HAVE_COMPILER
    if (OPTION_RUN_SCRIPT (&program)) {
      rewrite_script_source ();
    }
#endif
    if (OPTION_DEBUG (&program)) {
      state_license (STDOUT_FILENO);
    }
#if defined HAVE_COMPILER
    if (OPTION_OPTIMISE (&program)) {
      char libname[BUFFER_SIZE];
      void * a68g_lib;
      struct stat srcstat, objstat;
      int ret;
      announce_phase ("dynamic linker");
      ASSERT (snprintf (libname, SNPRINTF_SIZE, "./%s", FILE_LIBRARY_NAME (&program)) >= 0);
/* Check whether we are doing something rash */
      ret = stat (FILE_SOURCE_NAME (&program), &srcstat);
      ABEND (ret != 0, "cannot stat", FILE_SOURCE_NAME (&program));
      ret = stat (libname, &objstat);
      ABEND (ret != 0, "cannot stat", libname);
      if (OPTION_RERUN (&program)) {
        ABEND (ST_MTIME (&srcstat) > ST_MTIME (&objstat), "source file is younger than library", "do not specify RERUN");
      }
/* First load a68g itself so compiler code can resolve a68g symbols */
      a68g_lib = dlopen (NULL, RTLD_NOW | RTLD_GLOBAL);
      ABEND (a68g_lib == NULL, "compiler cannot resolve a68g symbols", dlerror ());
/* Then load compiler code */
      compile_lib = dlopen (libname, RTLD_NOW | RTLD_GLOBAL);
      ABEND (compile_lib == NULL, "compiler cannot resolve symbols", dlerror ());
    } else {
      compile_lib = NULL;
    }
    announce_phase ("genie");
    genie (compile_lib);
/* Unload compiler library */
    if (OPTION_OPTIMISE (&program)) {
      int ret = dlclose (compile_lib);
      ABEND (ret != 0, "cannot close shared library", dlerror ());
    }
#else
    genie (NO_NODE);
#endif
/* Free heap allocated by genie */
    free_genie_heap (TOP_NODE (&program));
/* Normal end of program */
    diagnostics_to_terminal (TOP_LINE (&program), A68_RUNTIME_ERROR);
    if (OPTION_DEBUG (&program) || OPTION_TRACE (&program) || OPTION_CLOCK (&program)) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\nGenie finished in %.2f seconds\n", seconds () - cputime_0) >= 0);
      WRITE (STDOUT_FILENO, output_line);
    }
    /* ASSERT (block_gc == 0); fails in case we jumped out of a RTS routine! */
    verbosity ();
  }
/* Setting up listing file */
  if (OPTION_MOID_LISTING (&program) || OPTION_TREE_LISTING (&program) || OPTION_SOURCE_LISTING (&program) || OPTION_OBJECT_LISTING (&program) || OPTION_STATISTICS_LISTING (&program)) {
    FILE_LISTING_FD (&program) = open (FILE_LISTING_NAME (&program), O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
    ABEND (FILE_LISTING_FD (&program) == -1, "cannot open listing file", NO_TEXT);
    FILE_LISTING_OPENED (&program) = A68_TRUE;
  } else {
    FILE_LISTING_OPENED (&program) = A68_FALSE;
  }
/* Write listing */
  if (FILE_LISTING_OPENED (&program)) {
    heap_is_fluid = A68_TRUE;
    write_listing_header ();
    write_source_listing ();
    write_tree_listing ();
    if (ERROR_COUNT (&program) == 0 && OPTION_OPTIMISE (&program)) {
      write_object_listing ();
    }
    write_listing ();
    ASSERT (close (FILE_LISTING_FD (&program)) == 0);
    FILE_LISTING_OPENED (&program) = A68_FALSE;
    verbosity ();
  }
/* Cleaning up the intermediate files */
#if defined HAVE_COMPILER
  if (OPTION_RUN_SCRIPT (&program) && !OPTION_KEEP (&program)) {
    if (emitted) {
      ABEND (remove (FILE_OBJECT_NAME (&program)) != 0, "cannot remove", FILE_OBJECT_NAME (&program));
    }
    ABEND (remove (FILE_SOURCE_NAME (&program)) != 0, "cannot remove", FILE_SOURCE_NAME (&program));
    ABEND (remove (FILE_LIBRARY_NAME (&program)) != 0, "cannot remove", FILE_LIBRARY_NAME (&program));
  } else if (OPTION_COMPILE (&program) && !OPTION_KEEP (&program)) {
    build_script ();
    if (emitted) {
      ABEND (remove (FILE_OBJECT_NAME (&program)) != 0, "cannot remove", FILE_OBJECT_NAME (&program));
    }
    ABEND (remove (FILE_LIBRARY_NAME (&program)) != 0, "cannot remove", FILE_LIBRARY_NAME (&program));
  } else if (OPTION_OPTIMISE (&program) && !OPTION_KEEP (&program)) {
    if (emitted) {
      ABEND (remove (FILE_OBJECT_NAME (&program)) != 0, "cannot remove", FILE_OBJECT_NAME (&program));
    }
  } else if (OPTION_RERUN (&program) && !OPTION_KEEP (&program)) {
    if (emitted) {
      ABEND (remove (FILE_OBJECT_NAME (&program)) != 0, "cannot remove", FILE_OBJECT_NAME (&program));
    }
  }
#endif
}

/*!
\brief exit a68g in an orderly manner
\param code exit code
**/

void a68g_exit (int code)
{
/*
  char name[BUFFER_SIZE];
  bufcpy (name, ".", BUFFER_SIZE);
  bufcat (name, a68g_cmd_name, BUFFER_SIZE);
  bufcat (name, ".x", BUFFER_SIZE);
  (void) (remove (name)); 
*/
/* Close unclosed files, remove temp files */
  free_file_entries ();
/* Close the terminal */
  io_close_tty_line ();
#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES)
/* 
"curses" might still be open if it was not closed from A68, or the program
was interrupted, or a runtime error occured. That wreaks havoc on your
terminal 
*/
  genie_curses_end (NO_NODE);
#endif
  exit (code);
}

/*!
\brief start bookkeeping for a phase
\param t name of phase
**/

static void announce_phase (char *t)
{
  if (OPTION_VERBOSE (&program)) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s: %s", a68g_cmd_name, t) >= 0);
    io_close_tty_line ();
    WRITE (STDOUT_FILENO, output_line);
  }
}

#if defined HAVE_COMPILER

/*!
\brief build shell script from program
**/

static void build_script (void)
{
  int ret;
  FILE_T script, source;
  LINE_T *sl;
  char cmd[BUFFER_SIZE];
#if ! defined HAVE_COMPILER
  return;
#endif
  announce_phase ("script builder");
/* Flatten the source file */
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_SOURCE_NAME (&program)) >= 0);
  source = open (cmd, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (source == -1, "cannot flatten source file", cmd);
  for (sl = TOP_LINE (&program); sl != NO_LINE; FORWARD (sl)) {
    if (strlen (STRING (sl)) == 0 || (STRING (sl))[strlen (STRING (sl)) - 1] != NEWLINE_CHAR) {
      ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s\n%d\n%s\n", FILENAME (sl), NUMBER (sl), STRING (sl)) >= 0);
    } else {
      ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s\n%d\n%s", FILENAME (sl), NUMBER (sl), STRING (sl)) >= 0);
    }
    WRITE (source, cmd);
  }
  ASSERT (close (source) == 0);
/* Compress source and library */
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "cp %s %s.%s", FILE_LIBRARY_NAME (&program), HIDDEN_TEMP_FILE_NAME, FILE_LIBRARY_NAME (&program)) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, "cannot copy", cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "tar czf %s.%s.tgz %s.%s %s.%s", HIDDEN_TEMP_FILE_NAME, FILE_GENERIC_NAME (&program), HIDDEN_TEMP_FILE_NAME, FILE_SOURCE_NAME (&program), HIDDEN_TEMP_FILE_NAME, FILE_LIBRARY_NAME (&program)) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, "cannot compress", cmd);
/* Compose script */
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_SCRIPT_NAME (&program)) >= 0);
  script = open (cmd, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (script == -1, "cannot compose script file", cmd);
  if (OPTION_LOCAL (&program)) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "#! ./a68g --run-script\n") >= 0);
  } else {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "#! %s/a68g --run-script\n", BINDIR) >= 0);
  }
  WRITE (script, output_line);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s\n--verify \"%s\"\n", FILE_GENERIC_NAME (&program), PACKAGE_STRING) >= 0);
  WRITE (script, output_line);
  ASSERT (close (script) == 0);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "cat %s.%s %s.%s.tgz > %s", HIDDEN_TEMP_FILE_NAME, FILE_SCRIPT_NAME (&program), HIDDEN_TEMP_FILE_NAME, FILE_GENERIC_NAME (&program), FILE_SCRIPT_NAME (&program)) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, "cannot compose script file", cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s", FILE_SCRIPT_NAME (&program)) >= 0);
  ret = chmod (cmd, (__mode_t) (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH)); /* -rwx-r--r-- */
  ABEND (ret != 0, "cannot compose script file", cmd);
  ABEND (ret != 0, "cannot remove", cmd);
/* Clean up */
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s.tgz", HIDDEN_TEMP_FILE_NAME, FILE_GENERIC_NAME (&program)) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, "cannot remove", cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_SOURCE_NAME (&program)) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, "cannot remove", cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_LIBRARY_NAME (&program)) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, "cannot remove", cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_SCRIPT_NAME (&program)) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, "cannot remove", cmd);
}

#endif

#if defined HAVE_COMPILER

/*!
\brief load program from shell script 
**/

static void load_script (void)
{
  int k;
  FILE_T script;
  char cmd[BUFFER_SIZE], ch;
#if ! defined HAVE_COMPILER
  return;
#endif
  announce_phase ("script loader");
/* Decompress the archive */
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "sed '1,3d' < %s | tar xzf -", FILE_INITIAL_NAME (&program)) >= 0);
  ABEND (system (cmd) != 0, "cannot decompress", cmd);
/* Reread the header */
  script = open (FILE_INITIAL_NAME (&program), O_RDONLY);
  ABEND (script == -1, "cannot open script file", cmd);
/* Skip the #! a68g line */
  ASSERT (io_read (script, &ch, 1) == 1);
  while (ch != NEWLINE_CHAR) {
    ASSERT (io_read (script, &ch, 1) == 1);
  }
/* Read the generic filename */
  input_line[0] = NULL_CHAR;
  k = 0;
  ASSERT (io_read (script, &ch, 1) == 1);
  while (ch != NEWLINE_CHAR) {
    input_line[k++] = ch;
    ASSERT (io_read (script, &ch, 1) == 1);
  }
  input_line[k] = NULL_CHAR;
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, input_line) >= 0);
  FILE_INITIAL_NAME (&program) = new_string (cmd);
/* Read options */
  input_line[0] = NULL_CHAR;
  k = 0;
  ASSERT (io_read (script, &ch, 1) == 1);
  while (ch != NEWLINE_CHAR) {
    input_line[k++] = ch;
    ASSERT (io_read (script, &ch, 1) == 1);
  }
  isolate_options (input_line, NO_LINE);
  (void) set_options (OPTION_LIST (&program), A68_FALSE);
  ASSERT (close (script) == 0);
}

#endif

#if defined HAVE_COMPILER

/*!
\brief rewrite source for shell script 
**/

static void rewrite_script_source (void)
{
  LINE_T * ref_l = NO_LINE;
  FILE_T source;
/* Rebuild the source file */
  ASSERT (remove (FILE_SOURCE_NAME (&program)) == 0);
  source = open (FILE_SOURCE_NAME (&program), O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (source == -1, "cannot rewrite source file", FILE_SOURCE_NAME (&program));
  for (ref_l = TOP_LINE (&program); ref_l != NO_LINE; FORWARD (ref_l)) {
    WRITE (source, STRING (ref_l));
    if (strlen (STRING (ref_l)) == 0 || (STRING (ref_l))[strlen (STRING (ref_l) - 1)] != NEWLINE_CHAR) {
      WRITE (source, NEWLINE_STRING);
    }
  }
/* Wrap it up */
  ASSERT (close (source) == 0);
}

#endif

/*
This code handles options to Algol68G.

Option syntax does not follow GNU standards.

Options come from:
  [1] A rc file (normally .a68grc).
  [2] The A68G_OPTIONS environment variable overrules [1].
  [3] Command line options overrule [2].
  [4] Pragmat items overrule [3]. 
*/

OPTIONS_T *options;

/*!
\brief set default values for options
**/

void default_options (MODULE_T *p)
{
  OPTION_NO_WARNINGS (p) = A68_TRUE;
  OPTION_BACKTRACE (p) = A68_FALSE;
  OPTION_BRACKETS (p) = A68_FALSE;
  OPTION_CHECK_ONLY (p) = A68_FALSE;
  OPTION_CLOCK (p) = A68_FALSE;
  OPTION_COMPILE (p) = A68_FALSE;
  OPTION_CROSS_REFERENCE (p) = A68_FALSE;
  OPTION_DEBUG (p) = A68_FALSE;
  OPTION_KEEP (p) = A68_FALSE;
  OPTION_LOCAL (p) = A68_FALSE;
  OPTION_MOID_LISTING (p) = A68_FALSE;
  OPTION_NODEMASK (p) = (STATUS_MASK) (ASSERT_MASK | SOURCE_MASK);
  OPTION_OPT_LEVEL (p) = 0;
  OPTION_OPTIMISE (p) = A68_FALSE;
  OPTION_PORTCHECK (p) = A68_FALSE;
  OPTION_PRAGMAT_SEMA (p) = A68_TRUE;
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
  OPTION_EDIT (p) = A68_FALSE;
  OPTION_TARGET (p) = NO_TEXT;
}

/*!
\brief error handler for options
\param l source line
\param option option text
\param info info text
**/

static void option_error (LINE_T * l, char *option, char *info)
{
  int k;
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s", option) >= 0);
  for (k = 0; output_line[k] != NULL_CHAR; k++) {
    output_line[k] = (char) TO_LOWER (output_line[k]);
  }
  if (info != NO_TEXT) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "error: %s option \"%s\"", info, output_line) >= 0);
  } else {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "error: in option \"%s\"", output_line) >= 0);
  }
  scan_error (l, NO_TEXT, edit_line);
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

void add_option_list (OPTION_LIST_T ** l, char *str, LINE_T * line)
{
  if (*l == NO_OPTION_LIST) {
    *l = (OPTION_LIST_T *) get_heap_space ((size_t) ALIGNED_SIZE_OF (OPTION_LIST_T));
    SCAN (*l) = SOURCE_SCAN (&program);
    STR (*l) = new_string (str);
    PROCESSED (*l) = A68_FALSE;
    LINE (*l) = line;
    NEXT (*l) = NO_OPTION_LIST;
  } else {
    add_option_list (&(NEXT (*l)), str, line);
  }
}

/*!
\brief initialise option handler
**/

void init_options (void)
{
  options = (OPTIONS_T *) malloc ((size_t) ALIGNED_SIZE_OF (OPTIONS_T));
  OPTION_LIST (&program) = NO_OPTION_LIST;
}

/*!
\brief test equality of p and q, upper case letters in q are mandatory
\param p string to match
\param q pattern
\return whether equal
**/

static BOOL_T eq (char *p, char *q)
{
/* Upper case letters in 'q' are mandatory, lower case must match */
  if (OPTION_PRAGMAT_SEMA (&program)) {
    return (match_string (p, q, '='));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief process echoes gathered in the option list
\param i option chain
**/

void prune_echoes (OPTION_LIST_T * i)
{
  while (i != NO_OPTION_LIST) {
    if (SCAN (i) == SOURCE_SCAN (&program)) {
      char *p = strip_sign (STR (i));
/* ECHO echoes a string */
      if (eq (p, "ECHO")) {
        {
          char *car = a68g_strchr (p, '=');
          if (car != NO_TEXT) {
            io_close_tty_line ();
            ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s", &car[1]) >= 0);
            WRITE (STDOUT_FILENO, output_line);
          } else {
            FORWARD (i);
            if (i != NO_OPTION_LIST) {
              if (strcmp (STR (i), "=") == 0) {
                FORWARD (i);
              }
              if (i != NO_OPTION_LIST) {
                io_close_tty_line ();
                ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s", STR (i)) >= 0);
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
  LINE_T *start_l = LINE (*i);
  char *start_c = STR (*i);
  char *car = NO_TEXT, *num = NO_TEXT;
  int k, mult = 1;
  *error = A68_FALSE;
/* Fetch argument */
  car = a68g_strchr (p, '=');
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
/* Translate argument into integer */
  if (*error) {
    option_error (start_l, start_c, "integer value required by");
    return (0);
  } else {
    char *suffix;
    RESET_ERRNO;
    k = strtol (num, &suffix, 0);      /* Accept also octal and hex */
    *error = (BOOL_T) (suffix == num);
    if (errno != 0 || *error) {
      option_error (start_l, start_c, "conversion error in");
      *error = A68_TRUE;
    } else if (k < 0) {
      option_error (start_l, start_c, "negative value in");
      *error = A68_TRUE;
    } else {
/* Accept suffix multipliers: 32k, 64M, 1G */
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
    if ((double) k * (double) mult > (double) A68_MAX_INT) {
      errno = ERANGE;
      option_error (start_l, start_c, "conversion overflow in");
    }
    return (k * mult);
  }
}

/*!
\brief process options gathered in the option list
\param i option chain
\param cmd_line whether command line argument
\return whether processing was successful
**/

BOOL_T set_options (OPTION_LIST_T * i, BOOL_T cmd_line)
{
  BOOL_T go_on = A68_TRUE, name_set = A68_FALSE, skip = A68_FALSE;
  OPTION_LIST_T *j = i;
  RESET_ERRNO;
  while (i != NO_OPTION_LIST && go_on) {
/* Once SCRIPT is processed we skip options on the command line */
    if (cmd_line && skip) {
      FORWARD (i);
    } else {
      LINE_T *start_l = LINE (i);
      char *start_c = STR (i);
      int n = (int) strlen (STR (i));
/* Allow for spaces ending in # to have A68 comment syntax with '#!' */
      while (n > 0 && (IS_SPACE((STR (i))[n - 1]) || (STR (i))[n - 1] == '#')) {
        (STR (i))[--n] = NULL_CHAR;
      }
      if (!(PROCESSED (i))) {
/* Accept UNIX '-option [=] value' */
        BOOL_T minus_sign = (BOOL_T) ((STR (i))[0] == '-');
        char *p = strip_sign (STR (i));
        if (!minus_sign && eq (p, "#")) {
          ;
        } else if (!minus_sign && cmd_line) {
/* Item without '-'s is a filename */
          if (!name_set) {
            FILE_INITIAL_NAME (&program) = new_string (p);
            name_set = A68_TRUE;
          } else {
            option_error (NO_LINE, start_c, "multiple source file names at");
          }
        }
/* Preprocessor items stop option processing */
        else if (eq (p, "INCLUDE")) {
          go_on = A68_FALSE;
        } else if (eq (p, "READ")) {
          go_on = A68_FALSE;
        } else if (eq (p, "PREPROCESSOR")) {
          go_on = A68_FALSE;
        } else if (eq (p, "NOPREPROCESSOR")) {
          go_on = A68_FALSE;
        }
/* EXIT stops option processing */
        else if (eq (p, "EXIT")) {
          go_on = A68_FALSE;
        }
/* Empty item (from specifying '-' or '--') stops option processing */
        else if (eq (p, "")) {
          go_on = A68_FALSE;
        }
/* FILE accepts its argument as filename */
        else if (eq (p, "File") && cmd_line) {
          FORWARD (i);
          if (i != NO_OPTION_LIST && strcmp (STR (i), "=") == 0) {
            FORWARD (i);
          }
          if (i != NO_OPTION_LIST) {
            if (!name_set) {
              FILE_INITIAL_NAME (&program) = new_string (STR (i));
              name_set = A68_TRUE;
            } else {
              option_error (start_l, start_c, "multiple source file names at");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
        }
/* TARGET accepts its argument as editor target */
        else if (eq (p, "TArget") && cmd_line) {
          FORWARD (i);
          if (i != NO_OPTION_LIST && strcmp (STR (i), "=") == 0) {
            FORWARD (i);
          }
          if (i != NO_OPTION_LIST) {
            OPTION_TARGET (&program) = new_string (STR (i));
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
        }
/* SCRIPT takes next argument as filename.
   Further options on the command line are not processed, but stored */
        else if (eq (p, "Script") && cmd_line) {
          FORWARD (i);
          if (i != NO_OPTION_LIST) {
            if (!name_set) {
              FILE_INITIAL_NAME (&program) = new_string (STR (i));
              name_set = A68_TRUE;
            } else {
              option_error (start_l, start_c, "multiple source file names at");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
          skip = A68_TRUE;
        }
/* VERIFY checks that argument is current a68g version number */
        else if (eq (p, "VERIFY")) {
          FORWARD (i);
          if (i != NO_OPTION_LIST && strcmp (STR (i), "=") == 0) {
            FORWARD (i);
          }
          if (i != NO_OPTION_LIST) {
            ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s verification \"%s\" does not match script verification \"%s\"", a68g_cmd_name, PACKAGE_STRING, STR (i)) >= 0);
            ABEND (strcmp (PACKAGE_STRING, STR (i)) != 0, new_string (output_line), "rebuild the script");
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
        }
/* HELP gives online help */
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
          a68g_exit (EXIT_SUCCESS);
        }
/* ECHO is treated later */
        else if (eq (p, "ECHO")) {
          if (a68g_strchr (p, '=') == NO_TEXT) {
            FORWARD (i);
            if (i != NO_OPTION_LIST) {
              if (strcmp (STR (i), "=") == 0) {
                FORWARD (i);
              }
            }
          }
        }
/* EDIT starts a basic editor */
        else if (eq (p, "Edit")) {
          if (cmd_line == A68_FALSE) {
            option_error (start_l, start_c, "command-line-only");
          } else {
            OPTION_EDIT (&program) = A68_TRUE;
          }
        }
/* EXECUTE and PRINT execute their argument as Algol 68 text */
        else if (eq (p, "EXECute") || eq (p, "X") || eq (p, "Print")) {
          if (cmd_line == A68_FALSE) {
            option_error (start_l, start_c, "command-line-only");
          } else if ((FORWARD (i)) != NO_OPTION_LIST) {
            BOOL_T error = A68_FALSE;
            if (strcmp (STR (i), "=") == 0) {
              error = (BOOL_T) ((FORWARD (i)) == NO_OPTION_LIST);
            }
            if (!error) {
              char name[BUFFER_SIZE];
              FILE *f;
              bufcpy (name, HIDDEN_TEMP_FILE_NAME, BUFFER_SIZE);
              bufcat (name, ".cmd.a68", BUFFER_SIZE);
              f = fopen (name, "w");
              ABEND (f == NO_FILE, "cannot open temp file", NO_TEXT);
              if (eq (p, "Execute") || eq (p, "X")) {
                fprintf (f, "(%s)\n", STR (i));
              } else {
                fprintf (f, "(print ((%s)))\n", STR (i));
              }
              ASSERT (fclose (f) == 0);
              FILE_INITIAL_NAME (&program) = new_string (name);
            } else {
              option_error (start_l, start_c, "unit required by");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
        }
/* HEAP, HANDLES, STACK, FRAME and OVERHEAD  set core allocation */
        else if (eq (p, "HEAP") || eq (p, "HANDLES") || eq (p, "STACK") || eq (p, "FRAME") || eq (p, "OVERHEAD")) {
          BOOL_T error = A68_FALSE;
          int k = fetch_integral (p, &i, &error);
/* Adjust size */
          if (error || errno > 0) {
            option_error (start_l, start_c, "conversion error in");
          } else if (k > 0) {
            if (k < MIN_MEM_SIZE) {
              option_error (start_l, start_c, "value less than minimum in");
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
/* COMPILE and NOCOMPILE switch on/off compilation */
        else if (eq (p, "Compile")) {
#if defined HAVE_LINUX
          OPTION_COMPILE (&program) = A68_TRUE;
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 2;
          OPTION_RUN_SCRIPT (&program) = A68_FALSE;
#else
          option_error (start_l, start_c, "linux-only");
#endif
        } else if (eq (p, "NOCompile")) {
          OPTION_COMPILE (&program) = A68_FALSE;
          OPTION_OPTIMISE (&program) = A68_FALSE;
          OPTION_OPT_LEVEL (&program) = 0;
          OPTION_RUN_SCRIPT (&program) = A68_FALSE;
        } else if (eq (p, "NO-Compile")) {
          OPTION_COMPILE (&program) = A68_FALSE;
          OPTION_OPTIMISE (&program) = A68_FALSE;
          OPTION_OPT_LEVEL (&program) = 0;
          OPTION_RUN_SCRIPT (&program) = A68_FALSE;
        }
/* OPTIMISE and NOOPTIMISE switch on/off optimisation */
        else if (eq (p, "OPTimise")) {
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 2;
        } else if (eq (p, "O0")) { 
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 0;
        } else if (eq (p, "O")) {
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 1;
        } else if (eq (p, "O1")) {
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 1;
        } else if (eq (p, "O2")) {
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 2;
        } else if (eq (p, "O3")) {
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 3;
        } else if (eq (p, "NOOptimise")) {
          OPTION_OPTIMISE (&program) = A68_FALSE;
          OPTION_OPT_LEVEL (&program) = 0;
        } else if (eq (p, "NO-Optimise")) {
          OPTION_OPTIMISE (&program) = A68_FALSE;
          OPTION_OPT_LEVEL (&program) = 0;
        } else if (eq (p, "NOOptimize")) {
          OPTION_OPTIMISE (&program) = A68_FALSE;
          OPTION_OPT_LEVEL (&program) = 0;
        } else if (eq (p, "NO-Optimize")) {
          OPTION_OPTIMISE (&program) = A68_FALSE;
          OPTION_OPT_LEVEL (&program) = 0;
        }
/* RUN-SCRIPT runs a comiled .sh script */
        else if (eq (p, "RUN-SCRIPT")) {
#if defined HAVE_LINUX
          FORWARD (i);
          if (i != NO_OPTION_LIST) {
            if (!name_set) {
              FILE_INITIAL_NAME (&program) = new_string (STR (i));
              name_set = A68_TRUE;
            } else {
              option_error (start_l, start_c, "multiple source file names at");
            }
          } else {
            option_error (start_l, start_c, "missing argument in");
          }
          skip = A68_TRUE;
          OPTION_RUN_SCRIPT (&program) = A68_TRUE;
          OPTION_COMPILE (&program) = A68_FALSE;
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 2;
#else
          option_error (start_l, start_c, "linux-only");
#endif
        } 
/* RERUN re-uses an existing .so file */
        else if (eq (p, "RERUN")) {
          OPTION_COMPILE (&program) = A68_FALSE;
          OPTION_RERUN (&program) = A68_TRUE;
          OPTION_OPTIMISE (&program) = A68_TRUE;
          OPTION_OPT_LEVEL (&program) = 2;
        } 
/* KEEP and NOKEEP switch off/on object file deletion */
        else if (eq (p, "KEEP")) {
          OPTION_KEEP (&program) = A68_TRUE;
        } else if (eq (p, "NOKEEP")) {
          OPTION_KEEP (&program) = A68_FALSE;
        } else if (eq (p, "NO-KEEP")) {
          OPTION_KEEP (&program) = A68_FALSE;
        }
/* BRACKETS extends Algol 68 syntax for brackets */
        else if (eq (p, "BRackets")) {
          OPTION_BRACKETS (&program) = A68_TRUE;
        }
/* REDUCTIONS gives parser reductions.*/
        else if (eq (p, "REDuctions")) {
          OPTION_REDUCTIONS (&program) = A68_TRUE;
        }
/* QUOTESTROPPING sets stropping to quote stropping */
        else if (eq (p, "QUOTEstropping")) {
          OPTION_STROPPING (&program) = QUOTE_STROPPING;
        } else if (eq (p, "QUOTE-stropping")) {
          OPTION_STROPPING (&program) = QUOTE_STROPPING;
        }
/* UPPERSTROPPING sets stropping to upper stropping, which is nowadays the expected default */
        else if (eq (p, "UPPERstropping")) {
          OPTION_STROPPING (&program) = UPPER_STROPPING;
        } else if (eq (p, "UPPER-stropping")) {
          OPTION_STROPPING (&program) = UPPER_STROPPING;
        }
/* CHECK and NORUN just check for syntax */
        else if (eq (p, "Check") || eq (p, "NORun") || eq (p, "NO-Run")) {
          OPTION_CHECK_ONLY (&program) = A68_TRUE;
        }
/* CLOCK times program execution */
        else if (eq (p, "CLock")) {
          OPTION_CLOCK (&program) = A68_TRUE;
        }
/* RUN overrides NORUN */
        else if (eq (p, "RUN")) {
          OPTION_RUN (&program) = A68_TRUE;
        }
/* MONITOR or DEBUG invokes the debugger at runtime errors */
        else if (eq (p, "MONitor") || eq (p, "DEBUG")) {
          OPTION_DEBUG (&program) = A68_TRUE;
        }
/* REGRESSION is an option that sets preferences when running the test suite - undocumented option */
        else if (eq (p, "REGRESSION")) {
          OPTION_NO_WARNINGS (&program) = A68_FALSE;
          OPTION_PORTCHECK (&program) = A68_TRUE;
          OPTION_REGRESSION_TEST (&program) = A68_TRUE;
          OPTION_TIME_LIMIT (&program) = 120;
          OPTION_KEEP (&program) = A68_TRUE;
          term_width = MAX_LINE_WIDTH;
        }
/* LOCAL assumes include files in the current directory - undocumented option */
        else if (eq (p, "LOCal")) {
          OPTION_LOCAL (&program) = A68_TRUE;
        }
/* NOWARNINGS switches unsuppressible warnings off */
        else if (eq (p, "NOWarnings")) {
          OPTION_NO_WARNINGS (&program) = A68_TRUE;
        } else if (eq (p, "NO-Warnings")) {
          OPTION_NO_WARNINGS (&program) = A68_TRUE;
        }
/* QUIET switches all warnings off */
        else if (eq (p, "Quiet")) {
          OPTION_QUIET (&program) = A68_TRUE;
        }
/* WARNINGS switches warnings on */
        else if (eq (p, "Warnings")) {
          OPTION_NO_WARNINGS (&program) = A68_FALSE;
        }
/* NOPORTCHECK switches portcheck off */
        else if (eq (p, "NOPORTcheck")) {
          OPTION_PORTCHECK (&program) = A68_FALSE;
        } else if (eq (p, "NO-PORTcheck")) {
          OPTION_PORTCHECK (&program) = A68_FALSE;
        }
/* PORTCHECK switches portcheck on */
        else if (eq (p, "PORTcheck")) {
          OPTION_PORTCHECK (&program) = A68_TRUE;
        }
/* PEDANTIC switches portcheck and warnings on */
        else if (eq (p, "PEDANTIC")) {
          OPTION_PORTCHECK (&program) = A68_TRUE;
          OPTION_NO_WARNINGS (&program) = A68_FALSE;
        }
/* PRAGMATS and NOPRAGMATS switch on/off pragmat processing */
        else if (eq (p, "PRagmats")) {
          OPTION_PRAGMAT_SEMA (&program) = A68_TRUE;
        } else if (eq (p, "NOPRagmats")) {
          OPTION_PRAGMAT_SEMA (&program) = A68_FALSE;
        } else if (eq (p, "NO-PRagmats")) {
          OPTION_PRAGMAT_SEMA (&program) = A68_FALSE;
        }
/* STRICT ignores A68G extensions to A68 syntax */
        else if (eq (p, "STRict")) {
          OPTION_STRICT (&program) = A68_TRUE;
          OPTION_PORTCHECK (&program) = A68_TRUE;
        }
/* VERBOSE in case you want to know what Algol68G is doing */
        else if (eq (p, "VERBose")) {
          OPTION_VERBOSE (&program) = A68_TRUE;
        }
/* VERSION lists the current version at an appropriate time in the future */
        else if (eq (p, "Version")) {
          OPTION_VERSION (&program) = A68_TRUE;
        }
/* XREF and NOXREF switch on/off a cross reference */
        else if (eq (p, "XREF")) {
          OPTION_SOURCE_LISTING (&program) = A68_TRUE;
          OPTION_CROSS_REFERENCE (&program) = A68_TRUE;
          OPTION_NODEMASK (&program) |= (CROSS_REFERENCE_MASK | SOURCE_MASK);
        } else if (eq (p, "NOXREF")) {
          OPTION_NODEMASK (&program) &= ~(CROSS_REFERENCE_MASK | SOURCE_MASK);
        } else if (eq (p, "NO-Xref")) {
          OPTION_NODEMASK (&program) &= ~(CROSS_REFERENCE_MASK | SOURCE_MASK);
        }
/* PRELUDELISTING cross references preludes, if they ever get implemented .. */
        else if (eq (p, "PRELUDElisting")) {
          OPTION_STANDARD_PRELUDE_LISTING (&program) = A68_TRUE;
        }
/* STATISTICS prints process statistics */
        else if (eq (p, "STatistics")) {
          OPTION_STATISTICS_LISTING (&program) = A68_TRUE;
        }
/* TREE and NOTREE switch on/off printing of the syntax tree. This gets bulky! */
        else if (eq (p, "TREE")) {
          OPTION_SOURCE_LISTING (&program) = A68_TRUE;
          OPTION_TREE_LISTING (&program) = A68_TRUE;
          OPTION_NODEMASK (&program) |= (TREE_MASK | SOURCE_MASK);
        } else if (eq (p, "NOTREE")) {
          OPTION_NODEMASK (&program) ^= (TREE_MASK | SOURCE_MASK);
        } else if (eq (p, "NO-TREE")) {
          OPTION_NODEMASK (&program) ^= (TREE_MASK | SOURCE_MASK);
        }
/* UNUSED indicates unused tags */
        else if (eq (p, "UNUSED")) {
          OPTION_UNUSED (&program) = A68_TRUE;
        }
/* EXTENSIVE set of options for an extensive listing */
        else if (eq (p, "EXTensive")) {
          OPTION_SOURCE_LISTING (&program) = A68_TRUE;
          OPTION_OBJECT_LISTING (&program) = A68_TRUE;
          OPTION_TREE_LISTING (&program) = A68_TRUE;
          OPTION_CROSS_REFERENCE (&program) = A68_TRUE;
          OPTION_MOID_LISTING (&program) = A68_TRUE;
          OPTION_STANDARD_PRELUDE_LISTING (&program) = A68_TRUE;
          OPTION_STATISTICS_LISTING (&program) = A68_TRUE;
          OPTION_UNUSED (&program) = A68_TRUE;
          OPTION_NODEMASK (&program) |= (CROSS_REFERENCE_MASK | TREE_MASK | CODE_MASK | SOURCE_MASK);
        }
/* LISTING set of options for a default listing */
        else if (eq (p, "Listing")) {
          OPTION_SOURCE_LISTING (&program) = A68_TRUE;
          OPTION_CROSS_REFERENCE (&program) = A68_TRUE;
          OPTION_STATISTICS_LISTING (&program) = A68_TRUE;
          OPTION_NODEMASK (&program) |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
        }
/* TTY send listing to standout. Remnant from my mainframe past */
        else if (eq (p, "TTY")) {
          OPTION_CROSS_REFERENCE (&program) = A68_TRUE;
          OPTION_STATISTICS_LISTING (&program) = A68_TRUE;
          OPTION_NODEMASK (&program) |= (SOURCE_MASK | CROSS_REFERENCE_MASK);
        }
/* SOURCE and NOSOURCE print source lines */
        else if (eq (p, "SOURCE")) {
          OPTION_SOURCE_LISTING (&program) = A68_TRUE;
          OPTION_NODEMASK (&program) |= SOURCE_MASK;
        } else if (eq (p, "NOSOURCE")) {
          OPTION_NODEMASK (&program) &= ~SOURCE_MASK;
        } else if (eq (p, "NO-SOURCE")) {
          OPTION_NODEMASK (&program) &= ~SOURCE_MASK;
        }
/* OBJECT and NOOBJECT print object lines */
        else if (eq (p, "OBJECT")) {
          OPTION_OBJECT_LISTING (&program) = A68_TRUE;
        } else if (eq (p, "NOOBJECT")) {
          OPTION_OBJECT_LISTING (&program) = A68_FALSE;
        } else if (eq (p, "NO-OBJECT")) {
          OPTION_OBJECT_LISTING (&program) = A68_FALSE;
        }
/* MOIDS prints an overview of moids used in the program */
        else if (eq (p, "MOIDS")) {
          OPTION_MOID_LISTING (&program) = A68_TRUE;
        }
/* ASSERTIONS and NOASSERTIONS switch on/off the processing of assertions */
        else if (eq (p, "Assertions")) {
          OPTION_NODEMASK (&program) |= ASSERT_MASK;
        } else if (eq (p, "NOAssertions")) {
          OPTION_NODEMASK (&program) &= ~ASSERT_MASK;
        } else if (eq (p, "NO-Assertions")) {
          OPTION_NODEMASK (&program) &= ~ASSERT_MASK;
        }
/* PRECISION sets the precision */
        else if (eq (p, "PRECision")) {
          BOOL_T error = A68_FALSE;
          int k = fetch_integral (p, &i, &error);
          if (error || errno > 0) {
            option_error (start_l, start_c, "conversion error in");
          } else if (k > 1) {
            if (int_to_mp_digits (k) > long_mp_digits ()) {
              set_longlong_mp_digits (int_to_mp_digits (k));
            } else {
              k = 1;
              while (int_to_mp_digits (k) <= long_mp_digits ()) {
                k++;
              }
              option_error (start_l, start_c, "value less than minimum in");
            }
          } else {
            option_error (start_l, start_c, "invalid value in");
          }
        }
/* BACKTRACE and NOBACKTRACE switch on/off stack backtracing */
        else if (eq (p, "BACKtrace")) {
          OPTION_BACKTRACE (&program) = A68_TRUE;
        } else if (eq (p, "NOBACKtrace")) {
          OPTION_BACKTRACE (&program) = A68_FALSE;
        } else if (eq (p, "NO-BACKtrace")) {
          OPTION_BACKTRACE (&program) = A68_FALSE;
        }
/* BREAK and NOBREAK switch on/off tracing of the running program */
        else if (eq (p, "BReakpoint")) {
          OPTION_NODEMASK (&program) |= BREAKPOINT_MASK;
        } else if (eq (p, "NOBReakpoint")) {
          OPTION_NODEMASK (&program) &= ~BREAKPOINT_MASK;
        } else if (eq (p, "NO-BReakpoint")) {
          OPTION_NODEMASK (&program) &= ~BREAKPOINT_MASK;
        }
/* TRACE and NOTRACE switch on/off tracing of the running program */
        else if (eq (p, "TRace")) {
          OPTION_TRACE (&program) = A68_TRUE;
          OPTION_NODEMASK (&program) |= BREAKPOINT_TRACE_MASK;
        } else if (eq (p, "NOTRace")) {
          OPTION_NODEMASK (&program) &= ~BREAKPOINT_TRACE_MASK;
        } else if (eq (p, "NO-TRace")) {
          OPTION_NODEMASK (&program) &= ~BREAKPOINT_TRACE_MASK;
        }
/* TIMELIMIT lets the interpreter stop after so-many seconds */
        else if (eq (p, "TImelimit") || eq (p, "TIME-Limit")) {
          BOOL_T error = A68_FALSE;
          int k = fetch_integral (p, &i, &error);
          if (error || errno > 0) {
            option_error (start_l, start_c, "conversion error in");
          } else if (k < 1) {
            option_error (start_l, start_c, "invalid time span in");
          } else {
            OPTION_TIME_LIMIT (&program) = k;
          }
        } else {
/* Unrecognised */
          option_error (start_l, start_c, "unrecognised");
        }
      }
/* Go processing next item, if present */
      if (i != NO_OPTION_LIST) {
        FORWARD (i);
      }
    }
  }
/* Mark options as processed */
  for (; j != NO_OPTION_LIST; FORWARD (j)) {
    PROCESSED (j) = A68_TRUE;
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
  storage_overhead = MIN_MEM_SIZE;
}

/*!
\brief read options from the .rc file
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
  if (f != NO_FILE) {
    while (!feof (f)) {
      if (fgets (input_line, BUFFER_SIZE, f) != NO_TEXT) {
        if (input_line[strlen (input_line) - 1] == NEWLINE_CHAR) {
          input_line[strlen (input_line) - 1] = NULL_CHAR;
        }
        isolate_options (input_line, NO_LINE);
      }
    }
    ASSERT (fclose (f) == 0);
    (void) set_options (OPTION_LIST (&program), A68_FALSE);
  } else {
    errno = 0;
  }
}

/*!
\brief read options from A68G_OPTIONS
**/

void read_env_options (void)
{
  if (getenv ("A68G_OPTIONS") != NULL) {
    isolate_options (getenv ("A68G_OPTIONS"), NO_LINE);
    (void) set_options (OPTION_LIST (&program), A68_FALSE);
    errno = 0;
  }
}

/*!
\brief tokenise string 'p' that holds options
\param p text
\param line source line
**/

void isolate_options (char *p, LINE_T * line)
{
  char *q;
/* 'q' points at first significant char in item .*/
  while (p[0] != NULL_CHAR) {
/* Skip white space .. */
    while ((p[0] == BLANK_CHAR || p[0] == TAB_CHAR || p[0] == ',') && p[0] != NULL_CHAR) {
      p++;
    }
/* ... then tokenise an item */
    if (p[0] != NULL_CHAR) {
/* Item can be "string". Note that these are not A68 strings */
      if (p[0] == QUOTE_CHAR || p[0] == '\'' || p[0] == '`') {
        char delim = p[0];
        p++;
        q = p;
        while (p[0] != delim && p[0] != NULL_CHAR) {
          p++;
        }
        if (p[0] != NULL_CHAR) {
          p[0] = NULL_CHAR;     /* p[0] was delimiter */
          p++;
        } else {
          scan_error (line, NO_TEXT, ERROR_UNTERMINATED_STRING);
        }
      } else {
/* Item is not a delimited string */
        q = p;
/* Tokenise symbol and gather it in the option list for later processing.
   Skip '='s, we accept if someone writes -prec=60 -heap=8192 */
        if (*q == '=') {
          p++;
        } else {
          /* Skip item */
          while (p[0] != BLANK_CHAR && p[0] != NULL_CHAR && p[0] != '=' && p[0] != ',') {
            p++;
          }
        }
        if (p[0] != NULL_CHAR) {
          p[0] = NULL_CHAR;
          p++;
        }
      }
/* 'q' points to first significant char in item, and 'p' points after item */
      add_option_list (&(OPTION_LIST (&program)), q, line);
    }
  }
}

/* Routines for making a listing file */

#define SHOW_EQ A68_FALSE

char *bar[BUFFER_SIZE];

/*!
\brief brief_mode_string
\param p moid to print
\return pointer to string
**/

static void a68g_print_short_mode (FILE_T f, MOID_T * z)
{
  if (IS (z, STANDARD)) {
    int i = DIM (z);
    if (i > 0) {
      while (i--) {
        WRITE (f, "LONG ");
      }
    } else if (i < 0) {
      while (i++) {
        WRITE (f, "SHORT ");
      }
    }
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s", NSYMBOL (NODE (z))) >= 0);
    WRITE (f, output_line);
  } else if (IS (z, REF_SYMBOL) && IS (SUB (z), STANDARD)) {
    WRITE (f, "REF ");
    a68g_print_short_mode (f, SUB (z));
  } else if (IS (z, PROC_SYMBOL) && PACK (z) == NO_PACK && IS (SUB (z), STANDARD)) {
    WRITE (f, "PROC ");
    a68g_print_short_mode (f, SUB (z));
  } else {  
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "#%d", NUMBER (z)) >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief a68g_print_flat_mode
\param f file number
\param z moid to print
**/

void a68g_print_flat_mode (FILE_T f, MOID_T * z)
{
  if (IS (z, STANDARD)) {
    int i = DIM (z);
    if (i > 0) {
      while (i--) {
        WRITE (f, "LONG ");
      }
    } else if (i < 0) {
      while (i++) {
        WRITE (f, "SHORT ");
      }
    }
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s", NSYMBOL (NODE (z))) >= 0);
    WRITE (f, output_line);
  } else if (IS (z, REF_SYMBOL)) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "REF ") >= 0);
    WRITE (f, output_line);
    a68g_print_short_mode (f, SUB (z));
  } else if (IS (z, PROC_SYMBOL) && DIM (z) == 0) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "PROC ") >= 0);
    WRITE (f, output_line);
    a68g_print_short_mode (f, SUB (z));
  } else if (IS (z, ROW_SYMBOL)) {
    int i = DIM (z);
    WRITE (f, "[");
    while (--i) {
      WRITE (f, ", ");
    }
    WRITE (f, "] ");
    a68g_print_short_mode (f, SUB (z));
  } else {
    a68g_print_short_mode (f, z);
  }
}

/*!
\brief brief_fields_flat
\param f file number
\param pack pack to print
**/

static void a68g_print_short_pack (FILE_T f, PACK_T * pack)
{
  if (pack != NO_PACK) {
    a68g_print_short_mode (f, MOID (pack));
    if (NEXT (pack) != NO_PACK) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", ") >= 0);
      WRITE (f, output_line);
      a68g_print_short_pack (f, NEXT (pack));
    }
  }
}

/*!
\brief a68g_print_mode
\param f file number
\param z moid to print
**/

void a68g_print_mode (FILE_T f, MOID_T * z)
{
  if (z != NO_MOID) {
    if (IS (z, STANDARD)) {
      a68g_print_flat_mode (f, z);
    } else if (IS (z, INDICANT)) {
      WRITE (f, NSYMBOL (NODE (z)));
    } else if (z == MODE (COLLITEM)) {
      WRITE (f, "\"COLLITEM\"");
    } else if (IS (z, REF_SYMBOL)) {
      WRITE (f, "REF ");
      a68g_print_flat_mode (f, SUB (z));
    } else if (IS (z, FLEX_SYMBOL)) {
      WRITE (f, "FLEX ");
      a68g_print_flat_mode (f, SUB (z));
    } else if (IS (z, ROW_SYMBOL)) {
      int i = DIM (z);
      WRITE (f, "[");
      while (--i) {
        WRITE (f, ", ");
      }
      WRITE (f, "] ");
      a68g_print_flat_mode (f, SUB (z));
    } else if (IS (z, STRUCT_SYMBOL)) {
      WRITE (f, "STRUCT (");
      a68g_print_short_pack (f, PACK (z));
      WRITE (f, ")");
    } else if (IS (z, UNION_SYMBOL)) {
      WRITE (f, "UNION (");
      a68g_print_short_pack (f, PACK (z));
      WRITE (f, ")");
    } else if (IS (z, PROC_SYMBOL)) {
      WRITE (f, "PROC ");
      if (PACK (z) != NO_PACK) {
        WRITE (f, "(");
        a68g_print_short_pack (f, PACK (z));
        WRITE (f, ") ");
      }
      a68g_print_flat_mode (f, SUB (z));
    } else if (IS (z, IN_TYPE_MODE)) {
      WRITE (f, "\"SIMPLIN\"");
    } else if (IS (z, OUT_TYPE_MODE)) {
      WRITE (f, "\"SIMPLOUT\"");
    } else if (IS (z, ROWS_SYMBOL)) {
      WRITE (f, "\"ROWS\"");
    } else if (IS (z, SERIES_MODE)) {
      WRITE (f, "\"SERIES\" (");
      a68g_print_short_pack (f, PACK (z));
      WRITE (f, ")");
    } else if (IS (z, STOWED_MODE)) {
      WRITE (f, "\"STOWED\" (");
      a68g_print_short_pack (f, PACK (z));
      WRITE (f, ")");
    }
  }
}

/*!
\brief print_mode_flat
\param f file number
\param m moid to print
**/

void print_mode_flat (FILE_T f, MOID_T * m)
{
  if (m != NO_MOID) {
    a68g_print_mode (f, m);
    if (NODE (m) != NO_NODE && NUMBER (NODE (m)) > 0) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " node %d", NUMBER (NODE (m))) >= 0);
      WRITE (f, output_line);
    }
    if (EQUIVALENT_MODE (m) != NO_MOID) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " equi #%d", NUMBER (EQUIVALENT (m))) >= 0);
      WRITE (f, output_line);
    }
    if (SLICE (m) != NO_MOID) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " slice #%d", NUMBER (SLICE (m))) >= 0);
      WRITE (f, output_line);
    }
    if (TRIM (m) != NO_MOID) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " trim #%d", NUMBER (TRIM (m))) >= 0);
      WRITE (f, output_line);
    }
    if (ROWED (m) != NO_MOID) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " rowed #%d", NUMBER (ROWED (m))) >= 0);
      WRITE (f, output_line);
    }
    if (DEFLEXED (m) != NO_MOID) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " deflex #%d", NUMBER (DEFLEXED (m))) >= 0);
      WRITE (f, output_line);
    }
    if (MULTIPLE (m) != NO_MOID) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " multiple #%d", NUMBER (MULTIPLE (m))) >= 0);
      WRITE (f, output_line);
    }
    if (NAME (m) != NO_MOID) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " name #%d", NUMBER (NAME (m))) >= 0);
      WRITE (f, output_line);
    }
    if (USE (m)) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " used") >= 0);
      WRITE (f, output_line);
    }
    if (DERIVATE (m)) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " derivate") >= 0);
      WRITE (f, output_line);
    }
    if (SIZE (m) > 0) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " size %d", MOID_SIZE (m)) >= 0);
      WRITE (f, output_line);
    }
    if (HAS_ROWS (m)) {
      WRITE (f, " []");
    }
  }
}

/*!
\brief xref_tags
\param f file number
\param s tag to print
\param a attribute
**/

static void xref_tags (FILE_T f, TAG_T * s, int a)
{
  for (; s != NO_TAG; FORWARD (s)) {
    NODE_T *where_tag = NODE (s);
    if ((where_tag != NO_NODE) && ((STATUS_TEST (where_tag, CROSS_REFERENCE_MASK)) || TAG_TABLE (s) == a68g_standenv)) {
      WRITE (f, "\n     ");
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "tag %d ", NUMBER (s)) >= 0);
      WRITE (f, output_line);
      switch (a) {
      case IDENTIFIER:
        {
          a68g_print_mode (f, MOID (s));
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, " %s", NSYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          break;
        }
      case INDICANT:
        {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, "indicant %s ", NSYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          a68g_print_mode (f, MOID (s));
          break;
        }
      case PRIO_SYMBOL:
        {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, "priority %s %d", NSYMBOL (NODE (s)), PRIO (s)) >= 0);
          WRITE (f, output_line);
          break;
        }
      case OP_SYMBOL:
        {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, "operator %s ", NSYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          a68g_print_mode (f, MOID (s));
          break;
        }
      case LABEL:
        {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, "label %s", NSYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          break;
        }
      case ANONYMOUS:
        {
          switch (PRIO (s)) {
          case ROUTINE_TEXT:
            {
              ASSERT (snprintf (output_line, SNPRINTF_SIZE, "routine text ") >= 0);
              break;
            }
          case FORMAT_TEXT:
            {
              ASSERT (snprintf (output_line, SNPRINTF_SIZE, "format text ") >= 0);
              break;
            }
          case FORMAT_IDENTIFIER:
            {
              ASSERT (snprintf (output_line, SNPRINTF_SIZE, "format item ") >= 0);
              break;
            }
          case COLLATERAL_CLAUSE:
            {
              ASSERT (snprintf (output_line, SNPRINTF_SIZE, "display ") >= 0);
              break;
            }
          case GENERATOR:
            {
              ASSERT (snprintf (output_line, SNPRINTF_SIZE, "generator ") >= 0);
              break;
            }
          case BLOCK_GC_REF:
            {
              ASSERT (snprintf (output_line, SNPRINTF_SIZE, "sweep protect ") >= 0);
              break;
            }
          }
          WRITE (f, output_line);
          a68g_print_mode (f, MOID (s));
          break;
        }
      default:
        {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, "internal %d ", a) >= 0);
          WRITE (f, output_line);
          a68g_print_mode (f, MOID (s));
          break;
        }
      }
      if (NODE (s) != NO_NODE && NUMBER (NODE (s)) > 0) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", node %d", NUMBER (NODE (s))) >= 0);
        WRITE (f, output_line);
      }
      if (where_tag != NO_NODE && INFO (where_tag) != NO_NINFO && LINE (INFO (where_tag)) != NO_LINE) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", line %d", LINE_NUMBER (where_tag)) >= 0);
        WRITE (f, output_line);
      }
    }
  }
}

/*!
\brief xref_decs
\param f file number
\param t symbol table
**/

static void xref_decs (FILE_T f, TABLE_T * t)
{
  if (INDICANTS (t) != NO_TAG) {
    xref_tags (f, INDICANTS (t), INDICANT);
  }
  if (OPERATORS (t) != NO_TAG) {
    xref_tags (f, OPERATORS (t), OP_SYMBOL);
  }
  if (PRIO (t) != NO_TAG) {
    xref_tags (f, PRIO (t), PRIO_SYMBOL);
  }
  if (IDENTIFIERS (t) != NO_TAG) {
    xref_tags (f, IDENTIFIERS (t), IDENTIFIER);
  }
  if (LABELS (t) != NO_TAG) {
    xref_tags (f, LABELS (t), LABEL);
  }
  if (ANONYMOUS (t) != NO_TAG) {
    xref_tags (f, ANONYMOUS (t), ANONYMOUS);
  }
}

/*!
\brief xref1_moid
\param f file number
\param p moid to xref
**/

static void xref1_moid (FILE_T f, MOID_T * p)
{
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n     #%d ", NUMBER (p)) >= 0);
  WRITE (f, output_line);
  print_mode_flat (f, p);
}

/*!
\brief moid_listing
\param f file number
\param m moid list to xref
**/

void moid_listing (FILE_T f, MOID_T * m)
{
  for (; m != NO_MOID; FORWARD (m)) {
    xref1_moid (f, m);
  }
  WRITE (f, "\n");
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n     MODE STRING  #%d ", NUMBER (MODE (STRING))) >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n     MODE COMPLEX #%d ", NUMBER (MODE (COMPLEX))) >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n     MODE SEMA    #%d ", NUMBER (MODE (SEMA))) >= 0);
  WRITE (f, output_line);
}

/*!
\brief cross_reference
\param file number
\param p top node
\param l source line
**/

static void cross_reference (FILE_T f, NODE_T * p, LINE_T * l)
{
  if (p != NO_NODE && CROSS_REFERENCE_SAFE (&program)) {
    for (; p != NO_NODE; FORWARD (p)) {
      if (is_new_lexical_level (p) && l == LINE (INFO (p))) {
        TABLE_T *c = TABLE (SUB (p));
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n\n[level %d", LEVEL (c)) >= 0);
        WRITE (f, output_line);
        if (PREVIOUS (c) == a68g_standenv) {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", in standard environ") >= 0);
        } else {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", in level %d", LEVEL (PREVIOUS (c))) >= 0);
        }
        WRITE (f, output_line);
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", %d increment]", AP_INCREMENT (c)) >= 0);
        WRITE (f, output_line);
        if (c != NO_TABLE) {
          xref_decs (f, c);
        }
        WRITE (f, "\n");
      }
      cross_reference (f, SUB (p), l);
    }
  }
}

/*!
\brief write_symbols
\param file number
\param p top node
\param count symbols written

static void write_symbols (FILE_T f, NODE_T * p, int *count)
{
  for (; p != NO_NODE && (*count) < 5; FORWARD (p)) {
    if (SUB (p) != NO_NODE) {
      write_symbols (f, SUB (p), count);
    } else {
      if (*count > 0) {
        WRITE (f, " ");
      }
      (*count)++;
      if (*count == 5) {
        WRITE (f, "...");
      } else {
        ASSERT (snprintf(output_line, SNPRINTF_SIZE, "%s", NSYMBOL (p)) >= 0);
        WRITE (f, output_line);
      }
    }
  }
}
**/

/*!
\brief tree_listing
\param f file number
\param q top node
\param x current level
\param l source line
\param ld index for indenting and drawing bars connecting nodes
**/

void tree_listing (FILE_T f, NODE_T * q, int x, LINE_T * l, int *ld)
{
  for (; q != NO_NODE; FORWARD (q)) {
    NODE_T *p = q;
    int k, dist;
    if (((STATUS_TEST (p, TREE_MASK))) && l == LINE (INFO (p))) {
      if (*ld < 0) {
        *ld = x;
      }
/* Indent */
      WRITE (f, "\n     ");
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%02d %06d p%02d ", x, NUMBER (p), PROCEDURE_LEVEL (INFO (p))) >= 0);
      WRITE (f, output_line);
      if (PREVIOUS (TABLE (p)) != NO_TABLE) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%02d-%02d-%02d ", 
          (TABLE (p) != NO_TABLE ? LEX_LEVEL (p) : 0), 
          (TABLE (p) != NO_TABLE ? LEVEL (PREVIOUS (TABLE (p))) : 0),
          (NON_LOCAL (p) != NO_TABLE ? LEVEL (NON_LOCAL (p)) : 0)
        ) >= 0);
      } else {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%02d-  -%02d", 
          (TABLE (p) != NO_TABLE ? LEX_LEVEL (p) : 0),
          (NON_LOCAL (p) != NO_TABLE ? LEVEL (NON_LOCAL (p)) : 0)
        ) >= 0);
      }
      WRITE (f, output_line);
      if (MOID (q) != NO_MOID) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "#%04d ", NUMBER (MOID (p))) >= 0);
      } else {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "      ") >= 0);
      }
      WRITE (f, output_line);
      for (k = 0; k < (x - *ld); k++) {
        WRITE (f, bar[k]);
      }
      if (MOID (p) != NO_MOID) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s ", moid_to_string (MOID (p), MOID_WIDTH, NO_NODE)) >= 0);
        WRITE (f, output_line);
      }
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s", non_terminal_string (edit_line, ATTRIBUTE (p))) >= 0);
      WRITE (f, output_line);
      if (SUB (p) == NO_NODE) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
        WRITE (f, output_line);
      }
      if (TAX (p) != NO_TAG) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", tag %06u", (unsigned) NUMBER (TAX (p))) >= 0);
        WRITE (f, output_line);
        if (MOID (TAX (p)) != NO_MOID) {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", mode %06u", (unsigned) NUMBER (MOID (TAX (p)))) >= 0);
          WRITE (f, output_line);
        }
      }
      if (GINFO (p) != NO_GINFO && propagator_name (UNIT (&GPROP (p))) != NO_TEXT) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", %s", propagator_name (UNIT (&GPROP (p)))) >= 0);
        WRITE (f, output_line);
      }
      if (GINFO (p) != NO_GINFO && COMPILE_NAME (GINFO (p)) != NO_TEXT) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", %s", COMPILE_NAME (GINFO (p))) >= 0);
        WRITE (f, output_line);
      }
      if (GINFO (p) != NO_GINFO && COMPILE_NODE (GINFO (p)) > 0) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", %6d", COMPILE_NODE (GINFO (p))) >= 0);
        WRITE (f, output_line);
      }
      if (GINFO (p) != NO_GINFO && BLOCK_REF (GINFO (p)) != NO_TAG) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, " *") >= 0);
        WRITE (f, output_line);
      }
    }
    dist = x - (*ld);
    if (dist >= 0 && dist < BUFFER_SIZE) {
      bar[dist] = (NEXT (p) != NO_NODE && l == LINE (INFO (NEXT (p))) ? "|" : " ");
    }
    tree_listing (f, SUB (p), x + 1, l, ld);
    dist = x - (*ld);
    if (dist >= 0 && dist < BUFFER_SIZE) {
      bar[dist] = " ";
    }
  }
}

/*!
\brief leaves_to_print
\param p top node
\param l source line
\return number of nodes to be printed in tree listing
**/

static int leaves_to_print (NODE_T * p, LINE_T * l)
{
  int z = 0;
  for (; p != NO_NODE && z == 0; FORWARD (p)) {
    if (l == LINE (INFO (p)) && ((STATUS_TEST (p, TREE_MASK)))) {
      z++;
    } else {
      z += leaves_to_print (SUB (p), l);
    }
  }
  return (z);
}

/*!
\brief list_source_line
\param f file number
\param line source line
**/

void list_source_line (FILE_T f, LINE_T * line, BOOL_T tree)
{
  int k = (int) strlen (STRING (line)) - 1;
  if (NUMBER (line) <= 0) {
/* Mask the prelude and postlude */
    return;
  }
  if ((STRING (line))[k] == NEWLINE_CHAR) {
    (STRING (line))[k] = NULL_CHAR;
  }
/* Print source line */
  write_source_line (f, line, NO_NODE, A68_ALL_DIAGNOSTICS);
/* Cross reference for lexical levels starting at this line */
  if (OPTION_CROSS_REFERENCE (&program)) {
    cross_reference (f, TOP_NODE (&program), line);
  }
/* Syntax tree listing connected with this line */
  if (tree && OPTION_TREE_LISTING (&program)) {
    if (TREE_LISTING_SAFE (&program) && leaves_to_print (TOP_NODE (&program), line)) {
      int ld = -1, k2;
      WRITE (f, "\n\nSyntax tree");
      for (k2 = 0; k2 < BUFFER_SIZE; k2++) {
        bar[k2] = " ";
      }
      tree_listing (f, TOP_NODE (&program), 1, line, &ld);
      WRITE (f, "\n");
    }
  }
}

/*!
\brief source_listing
**/

void write_source_listing (void)
{
  LINE_T *line = TOP_LINE (&program);
  FILE_T f = FILE_LISTING_FD (&program);
  int listed = 0;
  WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
  WRITE (FILE_LISTING_FD (&program), "\nSource listing");
  WRITE (FILE_LISTING_FD (&program), "\n------ -------");
  WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
  if (FILE_LISTING_OPENED (&program) == 0) {
    diagnostic_node (A68_ERROR, NO_NODE, ERROR_CANNOT_WRITE_LISTING);
    return;
  }
  for (; line != NO_LINE; FORWARD (line)) {
    if (NUMBER (line) > 0 && LIST (line)) {
      listed++;
    }
    list_source_line (f, line, A68_FALSE);
  }
/* Warn if there was no source at all */
  if (listed == 0) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n     No lines to list") >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief write_source_listing
**/

void write_tree_listing (void)
{
  LINE_T *line = TOP_LINE (&program);
  FILE_T f = FILE_LISTING_FD (&program);
  int listed = 0;
  WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
  WRITE (FILE_LISTING_FD (&program), "\nSyntax tree listing");
  WRITE (FILE_LISTING_FD (&program), "\n------ ---- -------");
  WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
  if (FILE_LISTING_OPENED (&program) == 0) {
    diagnostic_node (A68_ERROR, NO_NODE, ERROR_CANNOT_WRITE_LISTING);
    return;
  }
  for (; line != NO_LINE; FORWARD (line)) {
    if (NUMBER (line) > 0 && LIST (line)) {
      listed++;
    }
    list_source_line (f, line, A68_TRUE);
  }
/* Warn if there was no source at all */
  if (listed == 0) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n     No lines to list") >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief write_object_listing
**/

void write_object_listing (void)
{
  if (OPTION_OBJECT_LISTING (&program)) {
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&program), "\nObject listing");
    WRITE (FILE_LISTING_FD (&program), "\n------ -------");
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    compiler (FILE_LISTING_FD (&program));
  }
}

/*!
\brief write_listing
**/

void write_listing (void)
{
  FILE_T f = FILE_LISTING_FD (&program);
  if (OPTION_MOID_LISTING (&program)) {
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&program), "\nMode listing");
    WRITE (FILE_LISTING_FD (&program), "\n---- -------");
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    moid_listing (f, TOP_MOID (&program));
  }
  if (OPTION_STANDARD_PRELUDE_LISTING (&program) && a68g_standenv != NO_TABLE) {
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&program), "\nStandard prelude listing");
    WRITE (FILE_LISTING_FD (&program), "\n-------- ------- -------");
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    xref_decs (f, a68g_standenv);
  }
  if (TOP_REFINEMENT (&program) != NO_REFINEMENT) {
    REFINEMENT_T *x = TOP_REFINEMENT (&program);
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&program), "\nRefinement listing");
    WRITE (FILE_LISTING_FD (&program), "\n---------- -------");
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    while (x != NO_REFINEMENT) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n  \"%s\"", NAME (x)) >= 0);
      WRITE (f, output_line);
      if (LINE_DEFINED (x) != NO_LINE) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", defined in line %d", NUMBER (LINE_DEFINED (x))) >= 0);
        WRITE (f, output_line);
      }
      if (LINE_APPLIED (x) != NO_LINE) {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", applied in line %d", NUMBER (LINE_APPLIED (x))) >= 0);
        WRITE (f, output_line);
      }
      switch (APPLICATIONS (x)) {
      case 0:
        {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", not applied") >= 0);
          WRITE (f, output_line);
          break;
        }
      case 1:
        {
          break;
        }
      default:
        {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", applied more than once") >= 0);
          WRITE (f, output_line);
          break;
        }
      }
      FORWARD (x);
    }
  }
  if (OPTION_LIST (&program) != NO_OPTION_LIST) {
    OPTION_LIST_T *i;
    int k = 1;
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&program), "\nPragmat listing");
    WRITE (FILE_LISTING_FD (&program), "\n------- -------");
    WRITE (FILE_LISTING_FD (&program), NEWLINE_STRING);
    for (i = OPTION_LIST (&program); i != NO_OPTION_LIST; FORWARD (i)) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n%d: %s", k++, STR (i)) >= 0);
      WRITE (f, output_line);
    }
  }
}

/*!
\brief write_listing_header
**/

void write_listing_header (void)
{
  FILE_T f = FILE_LISTING_FD (&program);
  LINE_T * z;
  state_version (FILE_LISTING_FD (&program));
  WRITE (FILE_LISTING_FD (&program), "\nFile \"");
  WRITE (FILE_LISTING_FD (&program), FILE_SOURCE_NAME (&program));
  if (OPTION_STATISTICS_LISTING (&program)) {
    if (ERROR_COUNT (&program) + WARNING_COUNT (&program) > 0) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\nDiagnostics: %d error(s), %d warning(s)", ERROR_COUNT (&program), WARNING_COUNT (&program)) >= 0);
      WRITE (f, output_line);
      for (z = TOP_LINE (&program); z != NO_LINE; FORWARD (z)) {
        if (DIAGNOSTICS (z) != NO_DIAGNOSTIC) {
          write_source_line (f, z, NO_NODE, A68_TRUE);
        }
      }
    }
  }
}

/* Small utility routines */

/* Signal handlers */

/*!
\brief signal reading for segment violation
\param i dummy
**/

static void sigsegv_handler (int i)
{
  (void) i;
  exit (EXIT_FAILURE);
  return;
}

/*!
\brief raise SYSREQUEST so you get to a monitor
\param i dummy
**/

static void sigint_handler (int i)
{
  (void) i;
  ABEND (signal (SIGINT, sigint_handler) == SIG_ERR, "cannot install SIGINT handler", NO_TEXT);
  if (!(STATUS_TEST (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK) || in_monitor)) {
    STATUS_SET (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK);
    genie_break (TOP_NODE (&program));
  }
}

#if ! defined HAVE_WIN32

/*!
\brief signal reading from disconnected terminal
\param i dummy
**/

static void sigttin_handler (int i)
{
  (void) i;
  ABEND (A68_TRUE, "background process attempts reading from disconnected terminal", NO_TEXT);
}

/*!
\brief signal broken pipe
\param i dummy
**/

static void sigpipe_handler (int i)
{
  (void) i;
  ABEND (A68_TRUE, "forked process has broken the pipe", NO_TEXT);
}

/*!
\brief signal alarm - time limit check
\param i dummy
**/

static void sigalrm_handler (int i)
{
  (void) i;
  if (in_execution && !in_monitor) {
    double _m_t = (double) OPTION_TIME_LIMIT (&program);
    if (_m_t > 0 && (seconds () - cputime_0) > _m_t) {
      diagnostic_node (A68_RUNTIME_ERROR, (NODE_T *) last_unit, ERROR_TIME_LIMIT_EXCEEDED);
      exit_genie ((NODE_T *) last_unit, A68_RUNTIME_ERROR);
    }
  }
  (void) alarm (1);
}

#endif /* ! defined HAVE_WIN32 */

/*!
\brief install_signal_handlers
**/

void install_signal_handlers (void)
{
  ABEND (signal (SIGINT, sigint_handler) == SIG_ERR, "cannot install SIGINT handler", NO_TEXT);
  ABEND (signal (SIGSEGV, sigsegv_handler) == SIG_ERR, "cannot install SIGSEGV handler", NO_TEXT);
#if ! defined HAVE_WIN32
  ABEND (signal (SIGALRM, sigalrm_handler) == SIG_ERR, "cannot install SIGALRM handler", NO_TEXT);
  ABEND (signal (SIGPIPE, sigpipe_handler) == SIG_ERR, "cannot install SIGPIPE handler", NO_TEXT);
  ABEND (signal (SIGTTIN, sigttin_handler) == SIG_ERR, "cannot install SIGTTIN handler", NO_TEXT);
#endif /* ! defined HAVE_WIN32 */
}

ADDR_T fixed_heap_pointer, temp_heap_pointer;
POSTULATE_T *top_postulate, *top_postulate_list;
KEYWORD_T *top_keyword;
TOKEN_T *top_token;
BOOL_T heap_is_fluid;

static int tag_number = 0;

/*!
\brief pointer to block of "s" bytes
\param s block lenght in bytes
\return same
**/

BYTE_T *get_heap_space (size_t s)
{
  BYTE_T *z = (BYTE_T *) (A68_ALIGN_T *) malloc (A68_ALIGN (s));
  ABEND (z == NO_BYTE, ERROR_OUT_OF_CORE, NO_TEXT);
  return (z);
}

/*!
\brief make a new copy of "t"
\param t text
\return pointer
**/

char *new_string (char *t)
{
  int n = (int) (strlen (t) + 1);
  char *z = (char *) get_heap_space ((size_t) n);
  bufcpy (z, t, n);
  return (z);
}

/*!
\brief make a new copy of "t"
\param t text
\return pointer
**/

char *new_fixed_string (char *t)
{
  int n = (int) (strlen (t) + 1);
  char *z = (char *) get_fixed_heap_space ((size_t) n);
  bufcpy (z, t, n);
  return (z);
}

/*!
\brief make a new copy of "t"
\param t text
\return pointer
**/

char *new_temp_string (char *t)
{
  int n = (int) (strlen (t) + 1);
  char *z = (char *) get_temp_heap_space ((size_t) n);
  bufcpy (z, t, n);
  return (z);
}

/*!
\brief get (preferably fixed) heap space
\param s size in bytes
\return pointer to block
**/

BYTE_T *get_fixed_heap_space (size_t s)
{
  BYTE_T *z;
  if (heap_is_fluid) {
    z = HEAP_ADDRESS (fixed_heap_pointer);
    fixed_heap_pointer += A68_ALIGN ((int) s);
  /* Allow for extra storage for diagnostics etcetera */
    ABEND (fixed_heap_pointer >= (heap_size - MIN_MEM_SIZE), ERROR_OUT_OF_CORE, NO_TEXT);
    ABEND (((int) temp_heap_pointer - (int) fixed_heap_pointer) <= MIN_MEM_SIZE, ERROR_OUT_OF_CORE, NO_TEXT);
    return (z);
  } else { 
    return (get_heap_space (s));
  }
}

/*!
\brief get (preferably temporary) heap space
\param s size in bytes
\return pointer to block
**/

BYTE_T *get_temp_heap_space (size_t s)
{
  BYTE_T *z;
  /* ABEND (!heap_is_fluid, ERROR_INTERNAL_CONSISTENCY, NO_TEXT); */
  if (heap_is_fluid) {
    temp_heap_pointer -= A68_ALIGN ((int) s);
/* Allow for extra storage for diagnostics etcetera */
    ABEND (((int) temp_heap_pointer - (int) fixed_heap_pointer) <= MIN_MEM_SIZE, ERROR_OUT_OF_CORE, NO_TEXT);
    z = HEAP_ADDRESS (temp_heap_pointer);
    return (z);
  } else { 
    return (get_heap_space (s));
  }
}

/*!
\brief get size of stack segment
**/

void get_stack_size (void)
{
#if defined HAVE_WIN32
  stack_size = MEGABYTE; /* guess */
#else
  struct rlimit limits;
  RESET_ERRNO;
/* Some systems do not implement RLIMIT_STACK so if getrlimit fails, we do not abend */
  if (!(getrlimit (RLIMIT_STACK, &limits) == 0 && errno == 0)) {
    stack_size = MEGABYTE;
  }
  stack_size = (int) (RLIM_CUR (&limits) < RLIM_MAX (&limits) ? RLIM_CUR (&limits) : RLIM_MAX (&limits));
/* A heuristic in case getrlimit yields extreme numbers: the frame stack is
   assumed to fill at a rate comparable to the C stack, so the C stack needs
   not be larger than the frame stack. This may not be true */
  if (stack_size < KILOBYTE || (stack_size > 96 * MEGABYTE && stack_size > frame_stack_size)) {
    stack_size = frame_stack_size;
  }
#endif
  stack_limit = (stack_size > (4 * storage_overhead) ? (stack_size - storage_overhead) : stack_size / 2);
}

/*!
\brief convert integer to character
\param i integer
\return character
**/

char digit_to_char (int i)
{
  char *z = "0123456789abcdefghijklmnopqrstuvwxyz";
  if (i >= 0 && i < (int) strlen (z)) {
    return (z[i]);
  } else {
    return ('*');
  }
}

/*!
\brief renumber nodes
\param p position in tree
\param n node number counter
**/

void renumber_nodes (NODE_T * p, int *n)
{
  for (; p != NO_NODE; FORWARD (p)) {
    NUMBER (p) = (*n)++;
    renumber_nodes (SUB (p), n);
  }
}

/*!
\brief register nodes
\param p position in tree
**/

void register_nodes (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    node_register[NUMBER (p)] = p;
    register_nodes (SUB (p));
  }
}

/*!
\brief new_node_info
\return same
**/

NODE_INFO_T *new_node_info (void)
{
  NODE_INFO_T *z = (NODE_INFO_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (NODE_INFO_T));
  new_node_infos++;
  PROCEDURE_LEVEL (z) = 0;
  CHAR_IN_LINE (z) = NO_TEXT;
  SYMBOL (z) = NO_TEXT;
  LINE (z) = NO_LINE;
  return (z);
}

/*!
\brief new_genie_info
\return same
**/

GINFO_T *new_genie_info (void)
{
  GINFO_T *z = (GINFO_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (GINFO_T));
  new_genie_infos++;
  UNIT (&PROP (z)) = NO_PPROC;
  SOURCE (&PROP (z)) = NO_NODE;
  PARTIAL_PROC (z) = NO_MOID;
  PARTIAL_LOCALE (z) = NO_MOID;
  IS_COERCION (z) = A68_FALSE;
  IS_NEW_LEXICAL_LEVEL (z) = A68_FALSE;
  NEED_DNS (z) = A68_FALSE;
  PARENT (z) = NO_NODE;
  OFFSET (z) = NO_BYTE;
  CONSTANT (z) = NO_CONSTANT;
  LEVEL (z) = 0;
  ARGSIZE (z) = 0;
  SIZE (z) = 0;
  BLOCK_REF (z) = NO_TAG;
  COMPILE_NAME (z) = NO_TEXT;
  COMPILE_NODE (z) = 0;
  return (z);
}

/*!
\brief new_node
\return same
**/

NODE_T *new_node (void)
{
  NODE_T *z = (NODE_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (NODE_T));
  new_nodes++;
  STATUS (z) = NULL_MASK;
  CODEX (z) = NULL_MASK;
  TABLE (z) = NO_TABLE;
  INFO (z) = NO_NINFO;
  GINFO (z) = NO_GINFO;
  ATTRIBUTE (z) = 0;
  ANNOTATION (z) = 0;
  MOID (z) = NO_MOID;
  NEXT (z) = NO_NODE;
  PREVIOUS (z) = NO_NODE;
  SUB (z) = NO_NODE;
  NEST (z) = NO_NODE;
  NON_LOCAL (z) = NO_TABLE;
  TAX (z) = NO_TAG;
  SEQUENCE (z) = NO_NODE;
  PACK (z) = NO_PACK;
  return (z);
}

/*!
\brief new_symbol_table
\param p parent symbol table
\return same
**/

TABLE_T *new_symbol_table (TABLE_T * p)
{
  TABLE_T *z = (TABLE_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (TABLE_T));
  LEVEL (z) = symbol_table_count++;
  NEST (z) = symbol_table_count;
  ATTRIBUTE (z) = 0;
  AP_INCREMENT (z) = 0;
  INITIALISE_FRAME (z) = A68_TRUE;
  PROC_OPS (z) = A68_TRUE;
  INITIALISE_ANON (z) = A68_TRUE;
  PREVIOUS (z) = p;
  OUTER (z) = NO_TABLE;
  IDENTIFIERS (z) = NO_TAG;
  OPERATORS (z) = NO_TAG;
  PRIO (z) = NO_TAG;
  INDICANTS (z) = NO_TAG;
  LABELS (z) = NO_TAG;
  ANONYMOUS (z) = NO_TAG;
  JUMP_TO (z) = NO_NODE;
  SEQUENCE (z) = NO_NODE;
  return (z);
}

/*!
\brief new_moid
\return same
**/

MOID_T *new_moid (void)
{
  MOID_T *z = (MOID_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (MOID_T));
  new_modes++;
  ATTRIBUTE (z) = 0;
  NUMBER (z) = 0;
  DIM (z) = 0;
  USE (z) = A68_FALSE;
  HAS_ROWS (z) = A68_FALSE;
  SIZE (z) = 0;
  PORTABLE (z) = A68_TRUE;
  DERIVATE (z) = A68_FALSE;
  NODE (z) = NO_NODE;
  PACK (z) = NO_PACK;
  SUB (z) = NO_MOID;
  EQUIVALENT_MODE (z) = NO_MOID;
  SLICE (z) = NO_MOID;
  TRIM (z) = NO_MOID;
  DEFLEXED (z) = NO_MOID;
  NAME (z) = NO_MOID;
  MULTIPLE_MODE (z) = NO_MOID;
  NEXT (z) = NO_MOID;
  return (z);
}

/*!
\brief new_pack
\return same
**/

PACK_T *new_pack (void)
{
  PACK_T *z = (PACK_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (PACK_T));
  MOID (z) = NO_MOID;
  TEXT (z) = NO_TEXT;
  NODE (z) = NO_NODE;
  NEXT (z) = NO_PACK;
  PREVIOUS (z) = NO_PACK;
  SIZE (z) = 0;
  OFFSET (z) = 0;
  return (z);
}

/*!
\brief new_tag
\return same
**/

TAG_T *new_tag (void)
{
  TAG_T *z = (TAG_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (TAG_T));
  STATUS (z) = NULL_MASK;
  CODEX (z) = NULL_MASK;
  TAG_TABLE (z) = NO_TABLE;
  MOID (z) = NO_MOID;
  NODE (z) = NO_NODE;
  UNIT (z) = NO_NODE;
  VALUE (z) = NO_TEXT;
  A68G_STANDENV_PROC (z) = 0;
  PROCEDURE (z) = NO_GPROC;
  SCOPE (z) = PRIMAL_SCOPE;
  SCOPE_ASSIGNED (z) = A68_FALSE;
  PRIO (z) = 0;
  USE (z) = A68_FALSE;
  IN_PROC (z) = A68_FALSE;
  HEAP (z) = A68_FALSE;
  SIZE (z) = 0;
  OFFSET (z) = 0;
  YOUNGEST_ENVIRON (z) = PRIMAL_SCOPE;
  LOC_ASSIGNED (z) = A68_FALSE;
  NEXT (z) = NO_TAG;
  BODY (z) = NO_TAG;
  PORTABLE (z) = A68_TRUE;
  NUMBER (z) = ++tag_number;
  return (z);
}

/*!
\brief new_source_line
\return same
**/

LINE_T *new_source_line (void)
{
  LINE_T *z = (LINE_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (LINE_T));
  MARKER (z)[0] = NULL_CHAR;
  STRING (z) = NO_TEXT;
  FILENAME (z) = NO_TEXT;
  DIAGNOSTICS (z) = NO_DIAGNOSTIC;
  NUMBER (z) = 0;
  PRINT_STATUS (z) = 0;
  LIST (z) = A68_TRUE;
  NEXT (z) = NO_LINE;
  PREVIOUS (z) = NO_LINE;
  return (z);
}

/*!
\brief make special, internal mode
\param n chain to insert into
\param m moid number
**/

void make_special_mode (MOID_T ** n, int m)
{
  (*n) = new_moid ();
  ATTRIBUTE (*n) = 0;
  NUMBER (*n) = m;
  PACK (*n) = NO_PACK;
  SUB (*n) = NO_MOID;
  EQUIVALENT (*n) = NO_MOID;
  DEFLEXED (*n) = NO_MOID;
  NAME (*n) = NO_MOID;
  SLICE (*n) = NO_MOID;
  TRIM (*n) = NO_MOID;
  ROWED (*n) = NO_MOID;
}

/*!
\brief whether x matches c; case insensitive
\param string string to test
\param string to match, leading '-' or caps in c are mandatory
\param alt string terminator other than NULL_CHAR
\return whether match
**/

BOOL_T match_string (char *x, char *c, char alt)
{
  BOOL_T match = A68_TRUE;
  while ((IS_UPPER (c[0]) || IS_DIGIT (c[0]) || c[0] == '-') && match) {
    match = (BOOL_T) (match & (TO_LOWER (x[0]) == TO_LOWER ((c++)[0])));
    if (!(x[0] == NULL_CHAR || x[0] == alt)) {
      x++;
    }
  }
  while (x[0] != NULL_CHAR && x[0] != alt && c[0] != NULL_CHAR && match) {
    match = (BOOL_T) (match & (TO_LOWER ((x++)[0]) == TO_LOWER ((c++)[0])));
  }
  return ((BOOL_T) (match ? (x[0] == NULL_CHAR || x[0] == alt) : A68_FALSE));
}

/*!
\brief whether attributes match in subsequent nodes
\param p position in tree
\return whether match
**/

BOOL_T whether (NODE_T * p, ...)
{
  va_list vl;
  int a;
  va_start (vl, p);
  while ((a = va_arg (vl, int)) != STOP)
  {
    if (p != NO_NODE && a == WILDCARD) {
      FORWARD (p);
    } else if (p != NO_NODE && (a == KEYWORD)) {
      if (find_keyword_from_attribute (top_keyword, ATTRIBUTE (p)) != NO_KEYWORD) {
        FORWARD (p);
      } else {
        va_end (vl);
        return (A68_FALSE);
      }
    } else if (p != NO_NODE && (a >= 0 ? a == ATTRIBUTE (p) : (-a) != ATTRIBUTE (p))) {
      FORWARD (p);
    } else {
      va_end (vl);
      return (A68_FALSE);
    }
  }
  va_end (vl);
  return (A68_TRUE);
}

/*!
\brief whether one of a series of attributes matches a node
\param p position in tree
\return whether match
**/

BOOL_T is_one_of (NODE_T * p, ...)
{
  if (p != NO_NODE) {
    va_list vl;
    int a;
    BOOL_T match = A68_FALSE;
    va_start (vl, p);
    while ((a = va_arg (vl, int)) != STOP)
    {
      match = (BOOL_T) (match | (BOOL_T) (IS (p, a)));
    }
    va_end (vl);
    return (match);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief isolate nodes p-q making p a branch to p-q
\param p first node to branch
\param q last node to branch
\param t attribute for branch
**/

void make_sub (NODE_T * p, NODE_T * q, int t)
{
  NODE_T *z = new_node ();
  ABEND (p == NO_NODE || q == NO_NODE, ERROR_INTERNAL_CONSISTENCY, "make_sub");
  *z = *p;
  if (GINFO (p) != NO_GINFO) {
    GINFO (z) = new_genie_info ();
  }
  PREVIOUS (z) = NO_NODE;
  if (p == q) {
    NEXT (z) = NO_NODE;
  } else {
    if (NEXT (p) != NO_NODE) {
      PREVIOUS (NEXT (p)) = z;
    }
    NEXT (p) = NEXT (q);
    if (NEXT (p) != NO_NODE) {
      PREVIOUS (NEXT (p)) = p;
    }
    NEXT (q) = NO_NODE;
  }
  SUB (p) = z;
  ATTRIBUTE (p) = t;
}

/*!
\brief find symbol table at level 'i'
\param n position in tree
\param i level
\return same
**/

TABLE_T *find_level (NODE_T * n, int i)
{
  if (n == NO_NODE) {
    return (NO_TABLE);
  } else {
    TABLE_T *s = TABLE (n);
    if (s != NO_TABLE && LEVEL (s) == i) {
      return (s);
    } else if ((s = find_level (SUB (n), i)) != NO_TABLE) {
      return (s);
    } else if ((s = find_level (NEXT (n), i)) != NO_TABLE) {
      return (s);
    } else {
      return (NO_TABLE);
    }
  }
}

/*!
\brief time versus arbitrary origin
\return same
**/

double seconds (void)
{
  return ((double) clock () / (double) CLOCKS_PER_SEC);
}

/*!
\brief whether 'p' is top of lexical level
\param p position in tree
\return same
**/

BOOL_T is_new_lexical_level (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ALT_DO_PART:
  case BRIEF_ELIF_PART:
  case BRIEF_OUSE_PART:
  case BRIEF_CONFORMITY_OUSE_PART:
  case CHOICE:
  case CLOSED_CLAUSE:
  case CONDITIONAL_CLAUSE:
  case DO_PART:
  case ELIF_PART:
  case ELSE_PART:
  case FORMAT_TEXT:
  case CASE_CLAUSE:
  case CASE_CHOICE_CLAUSE:
  case CASE_IN_PART:
  case CASE_OUSE_PART:
  case OUT_PART:
  case ROUTINE_TEXT:
  case SPECIFIED_UNIT:
  case THEN_PART:
  case UNTIL_PART:
  case CONFORMITY_CLAUSE:
  case CONFORMITY_CHOICE:
  case CONFORMITY_IN_PART:
  case CONFORMITY_OUSE_PART:
  case WHILE_PART:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief some_node
\param t token text
\return same
**/

NODE_T *some_node (char *t)
{
  NODE_T *z = new_node ();
  INFO (z) = new_node_info ();
  GINFO (z) = new_genie_info ();
  NSYMBOL (z) = t;
  return (z);
}

/*!
\brief initialise use of elem-lists
**/

void init_postulates (void)
{
  top_postulate = NO_POSTULATE;
  top_postulate_list = NO_POSTULATE;
}

/*!
\brief make old postulates available for new use
\param start start of list to save
\param stop first element to not save
**/

void free_postulate_list (POSTULATE_T *start, POSTULATE_T *stop)
{
  POSTULATE_T *last;
  if (start == stop) {
    return;
  }
  for (last = start; NEXT (last) != stop; FORWARD (last)) {
    /* skip */;
  }
  NEXT (last) = top_postulate_list;
  top_postulate_list = start;
}

/*!
\brief add elements to elem-list
\param p postulate chain
\param a moid 1
\param b moid 2
**/

void make_postulate (POSTULATE_T ** p, MOID_T * a, MOID_T * b)
{
  POSTULATE_T *new_one;
  if (top_postulate_list != NO_POSTULATE) {
    new_one = top_postulate_list;
    FORWARD (top_postulate_list);
  } else {
    new_one = (POSTULATE_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (POSTULATE_T));
    new_postulates++;
  }
  A (new_one) = a;
  B (new_one) = b;
  NEXT (new_one) = *p;
  *p = new_one;
}

/*!
\brief where elements are in the list
\param p postulate chain
\param a moid 1
\param b moid 2
\return containing postulate
**/

POSTULATE_T *is_postulated_pair (POSTULATE_T * p, MOID_T * a, MOID_T * b)
{
  for (; p != NO_POSTULATE; FORWARD (p)) {
    if (A (p) == a && B (p) == b) {
      return (p);
    }
  }
  return (NO_POSTULATE);
}

/*!
\brief where element is in the list
\param p postulate chain
\param a moid 1
\return containing postulate
**/

POSTULATE_T *is_postulated (POSTULATE_T * p, MOID_T * a)
{
  for (; p != NO_POSTULATE; FORWARD (p)) {
    if (A (p) == a) {
      return (p);
    }
  }
  return (NO_POSTULATE);
}

/*------------------+
| Control of C heap |
+------------------*/

/*!
\brief discard_heap
**/

void discard_heap (void)
{
  if (heap_segment != NO_BYTE) {
    free (heap_segment);
  }
  fixed_heap_pointer = 0;
  temp_heap_pointer = 0;
}

/*!
\brief initialise C and A68 heap management
**/

void init_heap (void)
{
  int heap_a_size = A68_ALIGN (heap_size);
  int handle_a_size = A68_ALIGN (handle_pool_size);
  int frame_a_size = A68_ALIGN (frame_stack_size);
  int expr_a_size = A68_ALIGN (expr_stack_size);
  int total_size = A68_ALIGN (heap_a_size + handle_a_size + frame_a_size + expr_a_size);
  BYTE_T *core = (BYTE_T *) (A68_ALIGN_T *) malloc ((size_t) total_size);
  ABEND (core == NO_BYTE, ERROR_OUT_OF_CORE, NO_TEXT);
  heap_segment = &core[0];
  handle_segment = &heap_segment[heap_a_size];
  stack_segment = &handle_segment[handle_a_size];
  fixed_heap_pointer = A68_ALIGNMENT;
  temp_heap_pointer = total_size;
  frame_start = 0;              /* actually, heap_a_size + handle_a_size */
  frame_end = stack_start = frame_start + frame_a_size;
  stack_end = stack_start + expr_a_size;
}

/*!
\brief add token to the token tree
\param p top token
\param t token text
\return new entry
**/

TOKEN_T *add_token (TOKEN_T ** p, char *t)
{
  char *z = new_fixed_string (t);
  while (*p != NO_TOKEN) {
    int k = strcmp (z, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      return (*p);
    }
  }
  *p = (TOKEN_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (TOKEN_T));
  TEXT (*p) = z;
  LESS (*p) = MORE (*p) = NO_TOKEN;
  return (*p);
}

/*!
\brief find keyword, from token name
\param p top keyword
\param t token text to find
\return entry
**/

KEYWORD_T *find_keyword (KEYWORD_T * p, char *t)
{
  while (p != NO_KEYWORD) {
    int k = strcmp (t, TEXT (p));
    if (k < 0) {
      p = LESS (p);
    } else if (k > 0) {
      p = MORE (p);
    } else {
      return (p);
    }
  }
  return (NO_KEYWORD);
}

/*!
\brief find keyword, from attribute
\param p top keyword
\param a token attribute
\return entry
**/

KEYWORD_T *find_keyword_from_attribute (KEYWORD_T * p, int a)
{
  if (p == NO_KEYWORD) {
    return (NO_KEYWORD);
  } else if (a == ATTRIBUTE (p)) {
    return (p);
  } else {
    KEYWORD_T *z;
    if ((z = find_keyword_from_attribute (LESS (p), a)) != NO_KEYWORD) {
      return (z);
    } else if ((z = find_keyword_from_attribute (MORE (p), a)) != NO_KEYWORD) {
      return (z);
    } else {
      return (NO_KEYWORD);
    }
  }
}

/* A list of 10 ^ 2 ^ n for conversion purposes on IEEE 754 platforms */

#define MAX_DOUBLE_EXPO 511

static double pow_10[] = {
  10.0, 100.0, 1.0e4, 1.0e8, 1.0e16, 1.0e32, 1.0e64, 1.0e128, 1.0e256
};

/*!
\brief 10 ** expo
\param expo exponent
\return same
**/

double ten_up (int expo)
{
/* This way appears sufficiently accurate */
  double dbl_expo = 1.0, *dep;
  BOOL_T neg_expo = (BOOL_T) (expo < 0);
  if (neg_expo) {
    expo = -expo;
  }
  ABEND (expo > MAX_DOUBLE_EXPO, "exponent too large", NO_TEXT);
  for (dep = pow_10; expo != 0; expo >>= 1, dep++) {
    if (expo & 0x1) {
      dbl_expo *= *dep;
    }
  }
  return (neg_expo ? 1 / dbl_expo : dbl_expo);
}

/*!
\brief search first char in string
\param str string to search
\param c character to find
\return pointer to first "c" in "str"
**/

char *a68g_strchr (char *str, int c)
{
  return (strchr (str, c));
}

/*!
\brief safely append to buffer
\param dst text buffer
\param src text to append
\param len size of dst
**/

void bufcat (char *dst, char *src, int len)
{
  if (src != NO_TEXT) {
    char *d = dst, *s = src;
    int n = len, dlen;
  /* Find end of dst and left-adjust; do not go past end */
    for (; n-- != 0 && d[0] != NULL_CHAR; d++) {
      ;
    }
    dlen = d - dst;
    n = len - dlen;
    if (n > 0) {
      while (s[0] != NULL_CHAR) {
        if (n != 1) {
          (d++)[0] = s[0];
          n--;
        }
        s++;
      }
      d[0] = NULL_CHAR;
    }
  /* Better sure than sorry */
    dst[len - 1] = NULL_CHAR;
  }
}

/*!
\brief safely copy to buffer
\param dst text buffer
\param src text to append
\param siz size of dst
**/

void bufcpy (char *dst, char *src, int len)
{
  if (src != NO_TEXT) {
    char *d = dst, *s = src;
    int n = len;
  /* Copy as many as fit */
    if (n > 0 && --n > 0) {
      do {
        if (((d++)[0] = (s++)[0]) == NULL_CHAR) {
          break;
        }
      } while (--n > 0);
    }
    if (n == 0 && len > 0) {
  /* Not enough room in dst, so terminate */
      d[0] = NULL_CHAR;
    }
  /* Better sure than sorry */
    dst[len - 1] = NULL_CHAR;
  }
}

/*!
\brief grep in string (STRING, STRING, REF INT, REF INT) INT
\param pat search string or regular expression if supported
\param str string to match
\param start index of first character in first matching substring
\param start index of last character in first matching substring
\return 0: match, 1: no match, 2: no core, 3: other error
**/

int grep_in_string (char *pat, char *str, int *start, int *end)
{
#if defined HAVE_REGEX_H
  int rc, nmatch, k, max_k, widest;
  regex_t compiled;
  regmatch_t *matches;
  rc = regcomp (&compiled, pat, REG_NEWLINE | REG_EXTENDED);
  if (rc != 0) {
    regfree (&compiled);
    return (rc);
  }
  nmatch = (int) (RE_NSUB (&compiled));
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = malloc ((size_t) (nmatch * ALIGNED_SIZE_OF (regmatch_t)));
  if (nmatch > 0 && matches == NO_REGMATCH) {
    regfree (&compiled);
    return (2);
  }
  rc = regexec (&compiled, str, (size_t) nmatch, matches, 0);
  if (rc != 0) {
    regfree (&compiled);
    return (rc);
  }
/* Find widest match. Do not assume it is the first one */
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = (int) RM_EO (&matches[k]) - (int) RM_SO (&matches[k]);
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  if (start != NO_INT) {
    (*start) = (int) RM_SO (&matches[max_k]);
  }
  if (end != NO_INT) {
    (*end) = (int) RM_EO (&matches[max_k]);
  }
  free (matches);
  return (0);
#else
  (void) start;
  (void) end;
  if (strstr (str, pat) != NO_TEXT) {
    return (0);
  } else {
    return (1);
  }
#endif /* HAVE_REGEX_H */
}

/* VMS style acronyms */

/* This code was contributed by Theo Vosse.  */

static BOOL_T is_vowel (char);
static BOOL_T is_consonant (char);
static int qsort_strcmp (const void *, const void *);
static BOOL_T is_coda (char *, int);
static void get_init_sylls (char *, char *);
static void reduce_vowels (char *);
static int error_length (char *);
static BOOL_T remove_extra_coda (char *);
static void make_acronym (char *, char *);

/*!
\brief whether ch is a vowel
\param ch character under test
\return same
**/

static BOOL_T is_vowel (char ch)
{
  return ((BOOL_T) (a68g_strchr ("aeiouAEIOU", ch) != NO_TEXT));
}

/*!
\brief whether ch is consonant
\param ch character under test
\return same
**/

static BOOL_T is_consonant (char ch)
{
  return ((BOOL_T) (a68g_strchr ("qwrtypsdfghjklzxcvbnmQWRTYPSDFGHJKLZXCVBNM", ch) != NO_TEXT));
}

static char *codas[] = {
  "BT", "CH", "CHS", "CHT", "CHTS", "CT", "CTS", "D", "DS", "DST",
  "DT", "F", "FD", "FDS", "FDST", "FDT", "FS", "FST", "FT", "FTS", "FTST",
  "G", "GD",
  "GDS", "GDST", "GDT", "GS", "GST", "GT", "H", "K", "KS", "KST", "KT",
  "KTS", "KTST", "L", "LD", "LDS", "LDST", "LDT", "LF", "LFD", "LFS", "LFT",
  "LG", "LGD", "LGT", "LK", "LKS", "LKT", "LM", "LMD", "LMS", "LMT", "LP",
  "LPS",
  "LPT", "LS", "LSD", "LST", "LT", "LTS",
  "LTST", "M", "MBT", "MBTS", "MD", "MDS", "MDST", "MDT", "MF",
  "MP", "MPT", "MPTS", "MPTST", "MS", "MST", "MT", "N",
  "ND", "NDR", "NDS", "NDST", "NDT", "NG", "NGD", "NGS",
  "NGST", "NGT", "NK", "NKS", "NKST", "NKT", "NS", "NSD", "NST",
  "NT", "NTS", "NTST", "NTZ", "NX", "P", "PS", "PST", "PT", "PTS", "PTST",
  "R", "RCH", "RCHT", "RD", "RDS", "RDST", "RDT",
  "RG", "RGD", "RGS", "RGT", "RK", "RKS", "RKT",
  "RLS", "RM", "RMD", "RMS", "RMT", "RN", "RND", "RNS", "RNST", "RNT",
  "RP", "RPS", "RPT", "RS", "RSD", "RST", "RT", "RTS",
  "S", "SC", "SCH", "SCHT", "SCS", "SD", "SK",
  "SKS", "SKST", "SKT", "SP", "SPT", "ST", "STS", "T", "TS", "TST", "W",
  "WD", "WDS", "WDST", "WS", "WST", "WT",
  "X", "XT"
};

/*!
\brief compare function to pass to bsearch
\param key key to search
\param data data to search in
\return difference between key and data
**/

static int qsort_strcmp (const void *key, const void *data)
{
  return (strcmp ((char *) key, *(char **) data));
}

/*!
\brief whether first characters of string are a coda
\param str string under test
\param len number of characters
\return same
**/

static BOOL_T is_coda (char *str, int len)
{
  char str2[BUFFER_SIZE];
  bufcpy (str2, str, BUFFER_SIZE);
  str2[len] = NULL_CHAR;
  return ((BOOL_T) (bsearch (str2, codas, sizeof (codas) / sizeof (char *), sizeof (char *), qsort_strcmp) != NULL));
}

/*!
\brief get_init_sylls
\param in input string
\param out output string
**/

static void get_init_sylls (char *in, char *out)
{
  char *coda;
  while (*in != NULL_CHAR) {
    if (isalpha (*in)) {
      while (*in != NULL_CHAR && isalpha (*in) && !is_vowel (*in)) {
        *out++ = (char) toupper (*in++);
      }
      while (*in != NULL_CHAR && is_vowel (*in)) {
        *out++ = (char) toupper (*in++);
      }
      coda = out;
      while (*in != NULL_CHAR && is_consonant (*in)) {
        *out++ = (char) toupper (*in++);
        *out = NULL_CHAR;
        if (!is_coda (coda, out - coda)) {
          out--;
          break;
        }
      }
      while (*in != NULL_CHAR && isalpha (*in)) {
        in++;
      }
      *out++ = '+';
    } else {
      in++;
    }
  }
  out[-1] = NULL_CHAR;
}

/*!
\brief reduce vowels in string
\param str string
**/

static void reduce_vowels (char *str)
{
  char *next;
  while (*str != NULL_CHAR) {
    next = a68g_strchr (str + 1, '+');
    if (next == NO_TEXT) {
      break;
    }
    if (!is_vowel (*str) && is_vowel (next[1])) {
      while (str != next && !is_vowel (*str)) {
        str++;
      }
      if (str != next) {
        memmove (str, next, strlen (next) + 1);
      }
    } else {
      while (*str != NULL_CHAR && *str != '+')
        str++;
    }
    if (*str == '+') {
      str++;
    }
  }
}

/*!
\brief remove boundaries in string
\param str string
\param max_len maximym length
**/

static void remove_boundaries (char *str, int max_len)
{
  int len = 0;
  while (*str != NULL_CHAR) {
    if (len >= max_len) {
      *str = NULL_CHAR;
      return;
    }
    if (*str == '+') {
      memmove (str, str + 1, strlen (str + 1) + 1);
    } else {
      str++;
      len++;
    }
  }
}

/*!
\brief error_length
\param str string
\return same
**/

static int error_length (char *str)
{
  int len = 0;
  while (*str != NULL_CHAR) {
    if (*str != '+') {
      len++;
    }
    str++;
  }
  return (len);
}

/*!
\brief remove extra coda
\param str string
\return whether operation succeeded
**/

static BOOL_T remove_extra_coda (char *str)
{
  int len;
  while (*str != NULL_CHAR) {
    if (is_vowel (*str) && str[1] != '+' && !is_vowel (str[1]) && str[2] != '+' && str[2] != NULL_CHAR) {
      for (len = 2; str[len] != NULL_CHAR && str[len] != '+'; len++);
      memmove (str + 1, str + len, strlen (str + len) + 1);
      return (A68_TRUE);
    }
    str++;
  }
  return (A68_FALSE);
}

/*!
\brief make acronym
\param in input string
\param out output string
**/

static void make_acronym (char *in, char *out)
{
  get_init_sylls (in, out);
  reduce_vowels (out);
  while (error_length (out) > 8 && remove_extra_coda (out));
  remove_boundaries (out, 8);
}

/*!
\brief push acronym of string in stack
\param p position in tree
**/

void genie_acronym (NODE_T * p)
{
  A68_REF z;
  int len;
  char *u, *v;
  POP_REF (p, &z);
  len = a68_string_size (p, z);
  u = (char *) malloc ((size_t) (len + 1));
  v = (char *) malloc ((size_t) (len + 1 + 8));
  (void) a_to_c_string (p, u, z);
  if (u != NO_TEXT && u[0] != NULL_CHAR && v != NO_TEXT) {
    make_acronym (u, v);
    PUSH_REF (p, c_to_a_string (p, v, DEFAULT_WIDTH));
  } else {
    PUSH_REF (p, empty_string (p));
  }
  if (u != NO_TEXT) {
    free (u);
  }
  if (v != NO_TEXT) {
    free (v);
  }
}

/* Translate int attributes to string names */

static char *attribute_names[WILDCARD + 1] = {
  NO_TEXT,
  "A68_PATTERN",
  "ACCO_SYMBOL",
  "ACTUAL_DECLARER_MARK",
  "ALT_DO_PART",
  "ALT_DO_SYMBOL",
  "ALT_EQUALS_SYMBOL",
  "ALT_FORMAL_BOUNDS_LIST",
  "ANDF_SYMBOL",
  "AND_FUNCTION",
  "ANONYMOUS",
  "ARGUMENT",
  "ARGUMENT_LIST",
  "ASSERTION",
  "ASSERT_SYMBOL",
  "ASSIGNATION",
  "ASSIGN_SYMBOL",
  "ASSIGN_TO_SYMBOL",
  "AT_SYMBOL",
  "BEGIN_SYMBOL",
  "BITS_C_PATTERN",
  "BITS_DENOTATION",
  "BITS_PATTERN",
  "BITS_SYMBOL",
  "BLOCK_GC_REF",
  "BOLD_COMMENT_SYMBOL",
  "BOLD_PRAGMAT_SYMBOL",
  "BOLD_TAG",
  "BOOLEAN_PATTERN",
  "BOOL_SYMBOL",
  "BOUND",
  "BOUNDS",
  "BOUNDS_LIST",
  "BRIEF_OUSE_PART",
  "BRIEF_CONFORMITY_OUSE_PART",
  "BRIEF_ELIF_PART",
  "BRIEF_OPERATOR_DECLARATION",
  "BUS_SYMBOL",
  "BYTES_SYMBOL",
  "BY_PART",
  "BY_SYMBOL",
  "CALL",
  "CASE_CHOICE_CLAUSE",
  "CASE_CLAUSE",
  "CASE_IN_PART",
  "CASE_OUSE_PART",
  "CASE_PART",
  "CASE_SYMBOL",
  "CAST",
  "CHANNEL_SYMBOL",
  "CHAR_C_PATTERN",
  "CHAR_DENOTATION",
  "CHAR_SYMBOL",
  "CHOICE",
  "CHOICE_PATTERN",
  "CLASS_SYMBOL",
  "CLOSED_CLAUSE",
  "CLOSE_SYMBOL",
  "CODE_CLAUSE",
  "CODE_LIST",
  "CODE_SYMBOL",
  "COLLATERAL_CLAUSE",
  "COLLECTION",
  "COLON_SYMBOL",
  "COLUMN_FUNCTION",
  "COLUMN_SYMBOL",
  "COMMA_SYMBOL",
  "COMPLEX_PATTERN",
  "COMPLEX_SYMBOL",
  "COMPL_SYMBOL",
  "CONDITIONAL_CLAUSE",
  "CONFORMITY_CHOICE",
  "CONFORMITY_CLAUSE",
  "CONFORMITY_IN_PART",
  "CONFORMITY_OUSE_PART",
  "CONSTRUCT",
  "DECLARATION_LIST",
  "DECLARER",
  "DEFINING_IDENTIFIER",
  "DEFINING_INDICANT",
  "DEFINING_OPERATOR",
  "DENOTATION",
  "DEPROCEDURING",
  "DEREFERENCING",
  "DIAGONAL_FUNCTION",
  "DIAGONAL_SYMBOL",
  "DOTDOT_SYMBOL",
  "DOWNTO_SYMBOL",
  "DO_PART",
  "DO_SYMBOL",
  "DYNAMIC_REPLICATOR",
  "EDOC_SYMBOL",
  "ELIF_IF_PART",
  "ELIF_PART",
  "ELIF_SYMBOL",
  "ELSE_BAR_SYMBOL",
  "ELSE_OPEN_PART",
  "ELSE_PART",
  "ELSE_SYMBOL",
  "EMPTY_SYMBOL",
  "ENCLOSED_CLAUSE",
  "END_SYMBOL",
  "ENQUIRY_CLAUSE",
  "ENVIRON_NAME",
  "ENVIRON_SYMBOL",
  "EQUALS_SYMBOL",
  "ERROR",
  "ERROR_IDENTIFIER",
  "ESAC_SYMBOL",
  "EXIT_SYMBOL",
  "EXPONENT_FRAME",
  "FALSE_SYMBOL",
  "FIELD",
  "FIELD_IDENTIFIER",
  "FIELD_SELECTION",
  "FILE_SYMBOL",
  "FIRM",
  "FIXED_C_PATTERN",
  "FI_SYMBOL",
  "FLEX_SYMBOL",
  "FLOAT_C_PATTERN",
  "FORMAL_BOUNDS",
  "FORMAL_BOUNDS_LIST",
  "FORMAL_DECLARERS",
  "FORMAL_DECLARERS_LIST",
  "FORMAL_DECLARER_MARK",
  "FORMAT_A_FRAME",
  "FORMAT_CLOSE_SYMBOL",
  "FORMAT_DELIMITER_SYMBOL",
  "FORMAT_D_FRAME",
  "FORMAT_E_FRAME",
  "FORMAT_IDENTIFIER",
  "FORMAT_ITEM_A",
  "FORMAT_ITEM_B",
  "FORMAT_ITEM_C",
  "FORMAT_ITEM_D",
  "FORMAT_ITEM_E",
  "FORMAT_ITEM_ESCAPE",
  "FORMAT_ITEM_F",
  "FORMAT_ITEM_G",
  "FORMAT_ITEM_H",
  "FORMAT_ITEM_I",
  "FORMAT_ITEM_J",
  "FORMAT_ITEM_K",
  "FORMAT_ITEM_L",
  "FORMAT_ITEM_M",
  "FORMAT_ITEM_MINUS",
  "FORMAT_ITEM_N",
  "FORMAT_ITEM_O",
  "FORMAT_ITEM_P",
  "FORMAT_ITEM_PLUS",
  "FORMAT_ITEM_POINT",
  "FORMAT_ITEM_Q",
  "FORMAT_ITEM_R",
  "FORMAT_ITEM_S",
  "FORMAT_ITEM_T",
  "FORMAT_ITEM_U",
  "FORMAT_ITEM_V",
  "FORMAT_ITEM_W",
  "FORMAT_ITEM_X",
  "FORMAT_ITEM_Y",
  "FORMAT_ITEM_Z",
  "FORMAT_I_FRAME",
  "FORMAT_OPEN_SYMBOL",
  "FORMAT_PATTERN",
  "FORMAT_POINT_FRAME",
  "FORMAT_SYMBOL",
  "FORMAT_TEXT",
  "FORMAT_Z_FRAME",
  "FORMULA",
  "FOR_PART",
  "FOR_SYMBOL",
  "FROM_PART",
  "FROM_SYMBOL",
  "GENERAL_C_PATTERN",
  "GENERAL_PATTERN",
  "GENERATOR",
  "GENERIC_ARGUMENT",
  "GENERIC_ARGUMENT_LIST",
  "GOTO_SYMBOL",
  "GO_SYMBOL",
  "HEAP_SYMBOL",
  "IDENTIFIER",
  "IDENTITY_DECLARATION",
  "IDENTITY_RELATION",
  "IF_PART",
  "IF_SYMBOL",
  "INDICANT",
  "INITIALISER_SERIES",
  "INSERTION",
  "INTEGRAL_C_PATTERN",
  "INTEGRAL_MOULD",
  "INTEGRAL_PATTERN",
  "INT_DENOTATION",
  "INT_SYMBOL",
  "IN_SYMBOL",
  "IN_TYPE_MODE",
  "ISNT_SYMBOL",
  "IS_SYMBOL",
  "JUMP",
  "KEYWORD",
  "LABEL",
  "LABELED_UNIT",
  "LABEL_IDENTIFIER",
  "LABEL_SEQUENCE",
  "LITERAL",
  "LOCAL_LABEL",
  "LOC_SYMBOL",
  "LONGETY",
  "LONG_SYMBOL",
  "LOOP_CLAUSE",
  "LOOP_IDENTIFIER",
  "MAIN_SYMBOL",
  "MEEK",
  "MODE_BITS",
  "MODE_BOOL",
  "MODE_BYTES",
  "MODE_CHAR",
  "MODE_COMPLEX",
  "MODE_DECLARATION",
  "MODE_FILE",
  "MODE_FORMAT",
  "MODE_INT",
  "MODE_LONGLONG_BITS",
  "MODE_LONGLONG_COMPLEX",
  "MODE_LONGLONG_INT",
  "MODE_LONGLONG_REAL",
  "MODE_LONG_BITS",
  "MODE_LONG_BYTES",
  "MODE_LONG_COMPLEX",
  "MODE_LONG_INT",
  "MODE_LONG_REAL",
  "MODE_NO_CHECK",
  "MODE_PIPE",
  "MODE_REAL",
  "MODE_SOUND",
  "MODE_SYMBOL",
  "MONADIC_FORMULA",
  "MONAD_SEQUENCE",
  "NEW_SYMBOL",
  "NIHIL",
  "NIL_SYMBOL",
  "NORMAL_IDENTIFIER",
  "NO_SORT",
  "OCCA_SYMBOL",
  "OD_SYMBOL",
  "OF_SYMBOL",
  "OPEN_PART",
  "OPEN_SYMBOL",
  "OPERATOR",
  "OPERATOR_DECLARATION",
  "OPERATOR_PLAN",
  "OP_SYMBOL",
  "ORF_SYMBOL",
  "OR_FUNCTION",
  "OUSE_PART",
  "OUSE_SYMBOL",
  "OUT_PART",
  "OUT_SYMBOL",
  "OUT_TYPE_MODE",
  "PARALLEL_CLAUSE",
  "PARAMETER",
  "PARAMETER_IDENTIFIER",
  "PARAMETER_LIST",
  "PARAMETER_PACK",
  "PARTICULAR_PROGRAM",
  "PAR_SYMBOL",
  "PICTURE",
  "PICTURE_LIST",
  "PIPE_SYMBOL",
  "POINT_SYMBOL",
  "PRIMARY",
  "PRIORITY",
  "PRIORITY_DECLARATION",
  "PRIO_SYMBOL",
  "PROCEDURE_DECLARATION",
  "PROCEDURE_VARIABLE_DECLARATION",
  "PROCEDURING",
  "PROC_SYMBOL",
  "QUALIFIER",
  "RADIX_FRAME",
  "REAL_DENOTATION",
  "REAL_PATTERN",
  "REAL_SYMBOL",
  "REF_SYMBOL",
  "REPLICATOR",
  "ROUTINE_TEXT",
  "ROUTINE_UNIT",
  "ROWING",
  "ROWS_SYMBOL",
  "ROW_ASSIGNATION",
  "ROW_ASSIGN_SYMBOL",
  "ROW_CHAR_DENOTATION",
  "ROW_FUNCTION",
  "ROW_SYMBOL",
  "SECONDARY",
  "SELECTION",
  "SELECTOR",
  "SEMA_SYMBOL",
  "SEMI_SYMBOL",
  "SERIAL_CLAUSE",
  "SERIES_MODE",
  "SHORTETY",
  "SHORT_SYMBOL",
  "SIGN_MOULD",
  "SKIP",
  "SKIP_SYMBOL",
  "SLICE",
  "SOFT",
  "SOME_CLAUSE",
  "SOUND_SYMBOL",
  "SPECIFICATION",
  "SPECIFIED_UNIT",
  "SPECIFIED_UNIT_LIST",
  "SPECIFIED_UNIT_UNIT",
  "SPECIFIER",
  "SPECIFIER_IDENTIFIER",
  "STANDARD",
  "STATIC_REPLICATOR",
  "STOWED_MODE",
  "STRING_C_PATTERN",
  "STRING_PATTERN",
  "STRING_SYMBOL",
  "STRONG",
  "STRUCTURED_FIELD",
  "STRUCTURED_FIELD_LIST",
  "STRUCTURE_PACK",
  "STRUCT_SYMBOL",
  "STYLE_II_COMMENT_SYMBOL",
  "STYLE_I_COMMENT_SYMBOL",
  "STYLE_I_PRAGMAT_SYMBOL",
  "SUB_SYMBOL",
  "SUB_UNIT",
  "TERTIARY",
  "THEN_BAR_SYMBOL",
  "THEN_PART",
  "THEN_SYMBOL",
  "TO_PART",
  "TO_SYMBOL",
  "TRANSPOSE_FUNCTION",
  "TRANSPOSE_SYMBOL",
  "TRIMMER",
  "TRUE_SYMBOL",
  "UNION_DECLARER_LIST",
  "UNION_PACK",
  "UNION_SYMBOL",
  "UNIT",
  "UNITING",
  "UNIT_LIST",
  "UNIT_SERIES",
  "UNTIL_PART",
  "UNTIL_SYMBOL",
  "VARIABLE_DECLARATION",
  "VIRTUAL_DECLARER_MARK",
  "VOIDING",
  "VOID_SYMBOL",
  "WEAK",
  "WHILE_PART",
  "WHILE_SYMBOL",
  "WIDENING",
  "WILDCARD"
};

/*!
\brief non_terminal_string
\param buf text buffer
\param att attribute
\return buf, containing name of non terminal string
**/

char *non_terminal_string (char *buf, int att)
{
  if (att > 0 && att < WILDCARD) {
    if (attribute_names[att] != NO_TEXT) {
      char *q = buf;
      bufcpy (q, attribute_names[att], BUFFER_SIZE);
      while (q[0] != NULL_CHAR) {
        if (q[0] == '_') {
          q[0] = '-';
        } else {
          q[0] = (char) TO_LOWER (q[0]);
        }
        q++;
      }
      return (buf);
    } else {
      return (NO_TEXT);
    }
  } else {
    return (NO_TEXT);
  }
}

/*!
\brief standard_environ_proc_name
\param f routine that implements a standard environ item
\return name of that what "f" implements
**/

char *standard_environ_proc_name (GPROC f)
{
  TAG_T *i = IDENTIFIERS (a68g_standenv);
  for (; i != NO_TAG; FORWARD (i)) {
    if (PROCEDURE (i) == f) {
      return (NSYMBOL (NODE (i)));
    }
  }
  return (NO_TEXT);
}

/* Interactive help */

typedef struct A68_INFO A68_INFO;

struct A68_INFO
{
  char *cat;
  char *term;
  char *def;
};

static A68_INFO info_text[] = {
  {"monitor", "breakpoint clear [all]", "clear breakpoints and watchpoint expression"},
  {"monitor", "breakpoint clear breakpoints", "clear breakpoints"},
  {"monitor", "breakpoint clear watchpoint", "clear watchpoint expression"},
  {"monitor", "breakpoint [list]", "list breakpoints"},
  {"monitor", "breakpoint \"n\" clear", "clear breakpoints in line \"n\""},
  {"monitor", "breakpoint \"n\" if \"expression\"", "break in line \"n\" when expression evaluates to true"},
  {"monitor", "breakpoint \"n\"", "set breakpoints in line \"n\""},
  {"monitor", "breakpoint watch \"expression\"", "break on watchpoint expression when it evaluates to true"},
  {"monitor", "calls [n]", "print \"n\" frames in the call stack (default n=3)"},
  {"monitor", "continue, resume", "continue execution"},
  {"monitor", "do \"command\", exec \"command\"", "pass \"command\" to the shell and print return code"},
  {"monitor", "elems [n]", "print first \"n\" elements of rows (default n=24)"},
  {"monitor", "evaluate \"expression\", x \"expression\"", "print result of \"expression\""},
  {"monitor", "examine \"n\"", "print value of symbols named \"n\" in the call stack"},
  {"monitor", "exit, hx, quit", "terminates the program"},
  {"monitor", "finish, out", "continue execution until current procedure incarnation is finished"},
  {"monitor", "frame 0", "set current stack frame to top of frame stack"},
  {"monitor", "frame \"n\"", "set current stack frame to \"n\""},
  {"monitor", "frame", "print contents of the current stack frame"},
  {"monitor", "heap \"n\"", "print contents of the heap with address not greater than \"n\""},
  {"monitor", "help [expression]", "print brief help text"},
  {"monitor", "ht", "halts typing to standard output"},
  {"monitor", "list [n]", "show \"n\" lines around the interrupted line (default n=10)"},
  {"monitor", "next", "continue execution to next interruptable unit (do not enter routine-texts)"},
  {"monitor", "prompt \"s\"", "set prompt to \"s\""},
  {"monitor", "rerun, restart", "restarts a program without resetting breakpoints"},
  {"monitor", "reset", "restarts a program and resets breakpoints"},
  {"monitor", "rt", "resumes typing to standard output"},
  {"monitor", "sizes", "print size of memory segments"},
  {"monitor", "stack [n]", "print \"n\" frames in the stack (default n=3)"},
  {"monitor", "step", "continue execution to next interruptable unit"},
  {"monitor", "until \"n\"", "continue execution until line number \"n\" is reached"},
  {"monitor", "where", "print the interrupted line"},
  {"monitor", "xref \"n\"", "give detailed information on source line \"n\""},
  {"options", "--assertions, --noassertions", "switch elaboration of assertions on or off"},
  {"options", "--backtrace, --nobacktrace", "switch stack backtracing in case of a runtime error"},
  {"options", "--boldstropping", "set stropping mode to bold stropping"},
  {"options", "--brackets", "consider [ .. ] and { .. } as equivalent to ( .. )"},
  {"options", "--check, --norun", "check syntax only, interpreter does not start"},
  {"options", "--clock", "report execution time excluding compilation time"},
  {"options", "--debug, --monitor", "start execution in the debugger and debug in case of runtime error"},
  {"options", "--echo string", "echo \"string\" to standard output"},
  {"options", "--execute unit", "execute algol 68 unit \"unit\""},
  {"options", "--exit, --", "ignore next options"},
  {"options", "--extensive", "make extensive listing"},
  {"options", "--file string", "accept string as generic filename"},
  {"options", "--frame \"number\"", "set frame stack size to \"number\""},
  {"options", "--handles \"number\"", "set handle space size to \"number\""},
  {"options", "--heap \"number\"", "set heap size to \"number\""},
  {"options", "--keep, --nokeep", "switch object file deletion off or on"},
  {"options", "--listing", "make concise listing"},
  {"options", "--moids", "make overview of moids in listing file"},
  {"options", "-O0, -O1, -O2, -O3", "switch compilation on and pass option to back-end C compiler"},
  {"options", "--optimise, --nooptimise", "switch compilation on or off"},
  {"options", "--pedantic", "equivalent to --warnings --portcheck"},
  {"options", "--portcheck, --noportcheck", "switch portability warnings on or off"},
  {"options", "--pragmats, --nopragmats", "switch elaboration of pragmat items on or off"},
  {"options", "--precision \"number\"", "set precision for long long modes to \"number\" significant digits"},
  {"options", "--preludelisting", "make a listing of preludes"},
  {"options", "--print unit", "print value yielded by algol 68 unit \"unit\""},
  {"options", "--quotestropping", "set stropping mode to quote stropping"},
  {"options", "--reductions", "print parser reductions"},
  {"options", "--run", "override --check/--norun options"},
  {"options", "--rerun", "run using already compiled code"},
  {"options", "--script", "set next option as source file name; pass further options to algol 68 program"},
  {"options", "--source, --nosource", "switch listing of source lines in listing file on or off"},
  {"options", "--stack \"number\"", "set expression stack size to \"number\""},
  {"options", "--statistics", "print statistics in listing file"},
  {"options", "--strict", "disable most extensions to Algol 68 syntax"},
  {"options", "--timelimit \"number\"", "interrupt the interpreter after \"number\" seconds"},
  {"options", "--trace, --notrace", "switch tracing of a running program on or off"},
  {"options", "--tree, --notree", "switch syntax tree listing in listing file on or off"},
  {"options", "--unused", "make an overview of unused tags in the listing file"},
  {"options", "--verbose", "inform on program actions"},
  {"options", "--version", "state version of the running copy"},
  {"options", "--warnings, --nowarnings", "switch warning diagnostics on or off"},
  {"options", "--xref, --noxref", "switch cross reference in the listing file on or off"},
  {NO_TEXT, NO_TEXT, NO_TEXT}
};

/*!
\brief print_info
\param f file number
\param prompt prompt text
\param k index of info item to print
**/

static void print_info (FILE_T f, char *prompt, int k)
{
  if (prompt != NO_TEXT) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s %s: %s.", prompt, TERM (&info_text[k]), DEF (&info_text[k])) >= 0);
  } else {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s: %s.", TERM (&info_text[k]), DEF (&info_text[k])) >= 0);
  }
  WRITELN (f, output_line);
}

/*!
\brief apropos
\param f file number
\param prompt prompt text
\param info item to print
**/

void apropos (FILE_T f, char *prompt, char *item)
{
  int k, n = 0;
  if (item == NO_TEXT) {
    for (k = 0; CAT (&info_text[k]) != NO_TEXT; k++) {
      print_info (f, prompt, k);
    }
    return;
  }
  for (k = 0; CAT (&info_text[k]) != NO_TEXT; k++) {
    if (grep_in_string (item, CAT (&info_text[k]), NO_INT, NO_INT) == 0) {
      print_info (f, prompt, k);
      n++;
    }
  }
  if (n > 0) {
    return;
  }
  for (k = 0; CAT (&info_text[k]) != NO_TEXT; k++) {
    if (grep_in_string (item, TERM (&info_text[k]), NO_INT, NO_INT) == 0 || grep_in_string (item, DEF (&info_text[k]), NO_INT, NO_INT) == 0) {
      print_info (f, prompt, k);
      n++;
    }
  }
}

/* Error handling routines */

#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES)
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
  return ((BOOL_T) (!IS_PRINT (ch) && ch != TAB_CHAR));
}

/*!
\brief format for printing control character
\param ch control character
\return string containing formatted character
**/

char *ctrl_char (int ch)
{
  static char loc_str[SMALL_BUFFER_SIZE];
  ch = TO_UCHAR (ch);
  if (IS_CNTRL (ch) && IS_LOWER (ch + 96)) {
    ASSERT (snprintf (loc_str, (size_t) SMALL_BUFFER_SIZE, "\\^%c", ch + 96) >= 0);
  } else {
    ASSERT (snprintf (loc_str, (size_t) SMALL_BUFFER_SIZE, "\\%02x", (unsigned) ch) >= 0);
  }
  return (loc_str);
}

/*!
\brief widen single char to string
\param ch character
\return (short) string
**/

static char *char_to_str (char ch)
{
  static char loc_str[2];
  loc_str[0] = ch;
  loc_str[1] = NULL_CHAR;
  return (loc_str);
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
/* Count the number of characters in token to print */
    if (IS_GRAPH (p[0])) {
      for (k = 0, q = p; q[0] != BLANK_CHAR && q[0] != NULL_CHAR && k <= line_width; q++, k++) {
        ;
      }
    } else {
      k = 1;
    }
/* Now see if there is space for the token */
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
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s: exiting: %s: %d: %s", a68g_cmd_name, file, line, reason) >= 0);
  if (info != NO_TEXT) {
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

static char *where_pos (LINE_T * p, NODE_T * q)
{
  char *pos;
  if (q != NO_NODE && p == LINE (INFO (q))) {
    pos = CHAR_IN_LINE (INFO (q));
  } else {
    pos = STRING (p);
  }
  if (pos == NO_TEXT) {
    pos = STRING (p);
  }
  for (; IS_SPACE (pos[0]) && pos[0] != NULL_CHAR; pos++) {
    ;
  }
  if (pos[0] == NULL_CHAR) {
    pos = STRING (p);
  }
  return (pos);
}

/*!
\brief position in line where diagnostic points at
\param a source line
\param d diagnostic
\return pointer to character to mark
**/

static char *diag_pos (LINE_T * p, DIAGNOSTIC_T * d)
{
  char *pos;
  if (WHERE (d) != NO_NODE && p == LINE (INFO (WHERE (d)))) {
    pos = CHAR_IN_LINE (INFO (WHERE (d)));
  } else {
    pos = STRING (p);
  }
  if (pos == NO_TEXT) {
    pos = STRING (p);
  }
  for (; IS_SPACE (pos[0]) && pos[0] != NULL_CHAR; pos++) {
    ;
  }
  if (pos[0] == NULL_CHAR) {
    pos = STRING (p);
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

void write_source_line (FILE_T f, LINE_T * p, NODE_T * nwhere, int diag)
{
  char *c, *c0;
  int continuations = 0;
  int pos = 5, col;
  int line_width = (f == STDOUT_FILENO ? term_width : MAX_LINE_WIDTH);
  BOOL_T line_ended;
/* Terminate properly */
  if ((STRING (p))[strlen (STRING (p)) - 1] == NEWLINE_CHAR) {
    (STRING (p))[strlen (STRING (p)) - 1] = NULL_CHAR;
    if ((STRING (p))[strlen (STRING (p)) - 1] == CR_CHAR) {
      (STRING (p))[strlen (STRING (p)) - 1] = NULL_CHAR;
    }
  }
/* Print line number */
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  } else {
    WRITE (f, NEWLINE_STRING);
  }
  if (NUMBER (p) == 0) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "      ") >= 0);
  } else {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%-5d ", NUMBER (p) % 100000) >= 0);
  }
  WRITE (f, output_line);
/* Pretty print line */
  c = c0 = STRING (p);
  col = 1;
  line_ended = A68_FALSE;
  while (!line_ended) {
    int len = 0;
    char *new_pos = NO_TEXT;
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
        len = (int) strlen (output_line);
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
      DIAGNOSTIC_T *d = DIAGNOSTICS (p);
      if (d != NO_DIAGNOSTIC || nwhere != NO_NODE) {
        char *c1;
        for (c1 = c0; c1 != c; c1++) {
          y |= (BOOL_T) (nwhere != NO_NODE && p == LINE (INFO (nwhere)) ? c1 == where_pos (p, nwhere) : A68_FALSE);
          if (diag != A68_NO_DIAGNOSTICS) {
            for (d = DIAGNOSTICS (p); d != NO_DIAGNOSTIC; FORWARD (d)) {
              z = (BOOL_T) (z | (c1 == diag_pos (p, d)));
            }
          }
        }
      }
/* If diagnostics are to be printed then print marks */
      if (y || z) {
        DIAGNOSTIC_T *d2;
        char *c1;
        int col_2 = 1;
        WRITE (f, "\n      ");
        for (c1 = c0; c1 != c; c1++) {
          int k = 0, diags_at_this_pos = 0;
          for (d2 = DIAGNOSTICS (p); d2 != NO_DIAGNOSTIC; FORWARD (d2)) {
            if (c1 == diag_pos (p, d2)) {
              diags_at_this_pos++;
              k = NUMBER (d2);
            }
          }
          if (y == A68_TRUE && c1 == where_pos (p, nwhere)) {
            bufcpy (output_line, "-", BUFFER_SIZE);
          } else if (diags_at_this_pos != 0) {
            if (diag == A68_NO_DIAGNOSTICS) {
              bufcpy (output_line, " ", BUFFER_SIZE);
            } else if (diags_at_this_pos == 1) {
              ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%c", digit_to_char (k)) >= 0);
            } else {
              bufcpy (output_line, "*", BUFFER_SIZE);
            }
          } else {
            if (unprintable (c1[0])) {
              int n = (int) strlen (ctrl_char (c1[0]));
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
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\n.%1d   ", continuations) >= 0);
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
    if (DIAGNOSTICS (p) != NO_DIAGNOSTIC) {
      DIAGNOSTIC_T *d;
      for (d = DIAGNOSTICS (p); d != NO_DIAGNOSTIC; FORWARD (d)) {
        if (diag == A68_RUNTIME_ERROR) {
          if (IS (d, A68_RUNTIME_ERROR)) {
            WRITE (f, NEWLINE_STRING);
            pretty_diag (f, TEXT (d));
          }
        } else {
          WRITE (f, NEWLINE_STRING);
          pretty_diag (f, TEXT (d));
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

void diagnostics_to_terminal (LINE_T * p, int what)
{
  for (; p != NO_LINE; FORWARD (p)) {
    if (DIAGNOSTICS (p) != NO_DIAGNOSTIC) {
      BOOL_T z = A68_FALSE;
      DIAGNOSTIC_T *d = DIAGNOSTICS (p);
      for (; d != NO_DIAGNOSTIC; FORWARD (d)) {
        if (what == A68_ALL_DIAGNOSTICS) {
          z = (BOOL_T) (z | (IS (d, A68_WARNING) || IS (d, A68_ERROR) || IS (d, A68_SYNTAX_ERROR) || IS (d, A68_MATH_ERROR) || IS (d, A68_RUNTIME_ERROR) || IS (d, A68_SUPPRESS_SEVERITY)));
        } else if (what == A68_RUNTIME_ERROR) {
          z = (BOOL_T) (z | (IS (d, A68_RUNTIME_ERROR)));
        }
      }
      if (z) {
        write_source_line (STDOUT_FILENO, p, NO_NODE, what);
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

void scan_error (LINE_T * u, char *v, char *txt)
{
  if (errno != 0) {
    diagnostic_line (A68_SUPPRESS_SEVERITY, u, v, txt, ERROR_SPECIFICATION);
  } else {
    diagnostic_line (A68_SUPPRESS_SEVERITY, u, v, txt, ERROR_UNSPECIFIED);
  }
  longjmp (RENDEZ_VOUS (&program), 1);
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
      return ("error");
    }
  case A68_SYNTAX_ERROR:
    {
      return ("syntax error");
    }
  case A68_RUNTIME_ERROR:
    {
      return ("runtime error");
    }
  case A68_MATH_ERROR:
    {
      return ("math error");
    }
  case A68_WARNING:
    {
      return ("warning");
    }
  case A68_SUPPRESS_SEVERITY:
    {
      return (NO_TEXT);
    }
  default:
    {
      return (NO_TEXT);
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
  if (severity == NO_TEXT) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s: %s.", a68g_cmd_name, b) >= 0);
  } else {
    bufcpy (st, get_severity (sev), SMALL_BUFFER_SIZE);
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%s: %s: %s.", a68g_cmd_name, st, b) >= 0);
  }
  io_close_tty_line ();
  pretty_diag (STDOUT_FILENO, output_line);
}

/*!
\brief add diagnostic to source line
\param line source line
\param pos where to mark
\param p node to mark
\param sev severity
\param b diagnostic text
*/

static void add_diagnostic (LINE_T * line, char *pos, NODE_T * p, int sev, char *b)
{
/* Add diagnostic and choose GNU style or non-GNU style */
  DIAGNOSTIC_T *msg = (DIAGNOSTIC_T *) get_heap_space ((size_t) ALIGNED_SIZE_OF (DIAGNOSTIC_T));
  DIAGNOSTIC_T **ref_msg;
  char a[BUFFER_SIZE], st[SMALL_BUFFER_SIZE], nst[BUFFER_SIZE];
  char *severity = get_severity (sev);
  int k = 1;
  if (line == NO_LINE && p == NO_NODE) {
    return;
  }
  if (in_monitor) {
    monitor_error (b, NO_TEXT);
    return;
  }
  nst[0] = NULL_CHAR;
  if (line == NO_LINE && p != NO_NODE) {
    line = LINE (INFO (p));
  }
  while (line != NO_LINE && NUMBER (line) == 0) {
    FORWARD (line);
  }
  if (line == NO_LINE) {
    return;
  }
  ref_msg = &(DIAGNOSTICS (line));
  while (*ref_msg != NO_DIAGNOSTIC) {
    ref_msg = &(NEXT (*ref_msg));
    k++;
  }
  if (p != NO_NODE) {
    NODE_T *n = NEST (p);
    if (n != NO_NODE && NSYMBOL (n) != NO_TEXT) {
      char *nt = non_terminal_string (edit_line, ATTRIBUTE (n));
      if (nt != NO_TEXT) {
        if (LINE_NUMBER (n) == 0) {
          ASSERT (snprintf (nst, SNPRINTF_SIZE, "detected in %s", nt) >= 0);
        } else {
          if (MOID (n) != NO_MOID) {
            if (LINE_NUMBER (n) == NUMBER (line)) {
              ASSERT (snprintf (nst, SNPRINTF_SIZE, "detected in %s %s starting at \"%.64s\" in this line", moid_to_string (MOID (n), MOID_ERROR_WIDTH, p), nt, NSYMBOL (n)) >= 0);
            } else {
              ASSERT (snprintf (nst, SNPRINTF_SIZE, "detected in %s %s starting at \"%.64s\" in line %d", moid_to_string (MOID (n), MOID_ERROR_WIDTH, p), nt, NSYMBOL (n), LINE_NUMBER (n)) >= 0);
            }
          } else {
            if (LINE_NUMBER (n) == NUMBER (line)) {
              ASSERT (snprintf (nst, SNPRINTF_SIZE, "detected in %s starting at \"%.64s\" in this line", nt, NSYMBOL (n)) >= 0);
            } else {
              ASSERT (snprintf (nst, SNPRINTF_SIZE, "detected in %s starting at \"%.64s\" in line %d", nt, NSYMBOL (n), LINE_NUMBER (n)) >= 0);
            }
          }
        }
      }
    }
  }
  if (severity == NO_TEXT) {
    if (FILENAME (line) != NO_TEXT && strcmp (FILE_SOURCE_NAME (&program), FILENAME (line)) == 0) {
      ASSERT (snprintf (a, SNPRINTF_SIZE, "%s: %x: %s", a68g_cmd_name, (unsigned) k, b) >= 0);
    } else if (FILENAME (line) != NO_TEXT) {
      ASSERT (snprintf (a, SNPRINTF_SIZE, "%s: %s: %x: %s", a68g_cmd_name, FILENAME (line), (unsigned) k, b) >= 0);
    } else {
      ASSERT (snprintf (a, SNPRINTF_SIZE, "%s: %x: %s", a68g_cmd_name, (unsigned) k, b) >= 0);
    }
  } else {
    bufcpy (st, get_severity (sev), SMALL_BUFFER_SIZE);
    if (FILENAME (line) != NO_TEXT && strcmp (FILE_SOURCE_NAME (&program), FILENAME (line)) == 0) {
      ASSERT (snprintf (a, SNPRINTF_SIZE, "%s: %s: %x: %s", a68g_cmd_name, st, (unsigned) k, b) >= 0);
    } else if (FILENAME (line) != NO_TEXT) {
      ASSERT (snprintf (a, SNPRINTF_SIZE, "%s: %s: %s: %x: %s", a68g_cmd_name, FILENAME (line), st, (unsigned) k, b) >= 0);
    } else {
      ASSERT (snprintf (a, SNPRINTF_SIZE, "%s: %s: %x: %s", a68g_cmd_name, st, (unsigned) k, b) >= 0);
    }
  }
  msg = (DIAGNOSTIC_T *) get_heap_space ((size_t) ALIGNED_SIZE_OF (DIAGNOSTIC_T));
  *ref_msg = msg;
  ATTRIBUTE (msg) = sev;
  if (nst[0] != NULL_CHAR) {
    bufcat (a, " (", BUFFER_SIZE);
    bufcat (a, nst, BUFFER_SIZE);
    bufcat (a, ")", BUFFER_SIZE);
  }
  bufcat (a, ".", BUFFER_SIZE);
  TEXT (msg) = new_string (a);
  WHERE (msg) = p;
  LINE (msg) = line;
  SYMBOL (msg) = pos;
  NUMBER (msg) = k;
  NEXT (msg) = NO_DIAGNOSTIC;
}

/*
Legend for special symbols:
# skip extra syntactical information
@ non terminal
A non terminal
B keyword
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
      if (t != NO_TEXT) {\
	bufcat (b, nt, BUFFER_SIZE);\
      } else {\
	bufcat (b, "construct", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'A') {\
      int att = va_arg (args, int);\
      char *nt = non_terminal_string (edit_line, att);\
      if (nt != NO_TEXT) {\
	bufcat (b, nt, BUFFER_SIZE);\
      } else {\
	bufcat (b, "construct", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'B') {\
      int att = va_arg (args, int);\
      KEYWORD_T *nt = find_keyword_from_attribute (top_keyword, att);\
      if (nt != NO_KEYWORD) {\
	bufcat (b, "\"", BUFFER_SIZE);\
	bufcat (b, TEXT (nt), BUFFER_SIZE);\
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
      ASSERT (snprintf(d, SNPRINTF_SIZE, "%d", a) >= 0);\
      bufcat (b, d, BUFFER_SIZE);\
    } else if (t[0] == 'H') {\
      char *a = va_arg (args, char *);\
      char d[SMALL_BUFFER_SIZE];\
      ASSERT (snprintf(d, (size_t) SMALL_BUFFER_SIZE, "\"%c\"", a[0]) >= 0);\
      bufcat (b, d, BUFFER_SIZE);\
    } else if (t[0] == 'L') {\
      LINE_T *a = va_arg (args, LINE_T *);\
      char d[SMALL_BUFFER_SIZE];\
      ABEND (a == NO_LINE, "null source line in error", NO_TEXT);\
      if (NUMBER (a) == 0) {\
	bufcat (b, "in standard environment", BUFFER_SIZE);\
      } else {\
        if (p != NO_NODE && NUMBER (a) == LINE_NUMBER (p)) {\
          ASSERT (snprintf(d, (size_t) SMALL_BUFFER_SIZE, "in this line") >= 0);\
	} else {\
          ASSERT (snprintf(d, (size_t) SMALL_BUFFER_SIZE, "in line %d", NUMBER (a)) >= 0);\
        }\
	bufcat (b, d, BUFFER_SIZE);\
      }\
    } else if (t[0] == 'M') {\
      moid = va_arg (args, MOID_T *);\
      if (moid == NO_MOID || moid == MODE (ERROR)) {\
	moid = MODE (UNDEFINED);\
      }\
      if (IS (moid, SERIES_MODE)) {\
	if (PACK (moid) != NO_PACK && NEXT (PACK (moid)) == NO_PACK) {\
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
      if (moid != NO_MOID) {\
	bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
      }\
    } else if (t[0] == 'O') {\
      moid = va_arg (args, MOID_T *);\
      if (moid == NO_MOID || moid == MODE (ERROR)) {\
	moid = MODE (UNDEFINED);\
      }\
      if (moid == MODE (VOID)) {\
	bufcat (b, "UNION (VOID, ..)", BUFFER_SIZE);\
      } else if (IS (moid, SERIES_MODE)) {\
	if (PACK (moid) != NO_PACK && NEXT (PACK (moid)) == NO_PACK) {\
	  bufcat (b, moid_to_string (MOID (PACK (moid)), MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
	} else {\
	  bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
	}\
      } else {\
	bufcat (b, moid_to_string (moid, MOID_ERROR_WIDTH, p), BUFFER_SIZE);\
      }\
    } else if (t[0] == 'S') {\
      if (p != NO_NODE && NSYMBOL (p) != NO_TEXT) {\
	bufcat (b, "\"", BUFFER_SIZE);\
	bufcat (b, NSYMBOL (p), BUFFER_SIZE);\
	bufcat (b, "\"", BUFFER_SIZE);\
      } else {\
	bufcat (b, "symbol", BUFFER_SIZE);\
      }\
    } else if (t[0] == 'U') {\
      char *loc_string = va_arg (args, char *);\
      bufcat (b, loc_string, BUFFER_SIZE);\
    } else if (t[0] == 'X') {\
      int att = va_arg (args, int);\
      char z[BUFFER_SIZE];\
      /* ASSERT (snprintf(z, SNPRINTF_SIZE, "\"%s\"", TEXT (find_keyword_from_attribute (top_keyword, att))) >= 0); */\
      (void) non_terminal_string (z, att);\
      bufcat (b, new_string (z), BUFFER_SIZE);\
    } else if (t[0] == 'Y') {\
      char *loc_string = va_arg (args, char *);\
      bufcat (b, loc_string, BUFFER_SIZE);\
    } else if (t[0] == 'Z') {\
      char *loc_string = va_arg (args, char *);\
      bufcat (b, "\"", BUFFER_SIZE);\
      bufcat (b, loc_string, BUFFER_SIZE);\
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
\param loc_str message string
\param ... various arguments needed by special symbols in loc_str
**/

void diagnostic_node (int sev, NODE_T * p, char *loc_str, ...)
{
  va_list args;
  MOID_T *moid = NO_MOID;
  char *t = loc_str, b[BUFFER_SIZE];
  BOOL_T force, extra_syntax = A68_TRUE, shortcut = A68_FALSE;
  int err = errno;
  va_start (args, loc_str);
  b[0] = NULL_CHAR;
  force = (BOOL_T) ((sev & A68_FORCE_DIAGNOSTICS) != 0);
  sev &= ~A68_FORCE_DIAGNOSTICS;
/* No warnings? */
  if (!force && sev == A68_WARNING && OPTION_NO_WARNINGS (&program)) {
    return;
  }
  if (sev == A68_WARNING && OPTION_QUIET (&program)) {
    return;
  }
/* Suppressed? */
  if (sev == A68_ERROR || sev == A68_SYNTAX_ERROR) {
    if (ERROR_COUNT (&program) == MAX_ERRORS) {
      bufcpy (b, "further error diagnostics suppressed", BUFFER_SIZE);
      sev = A68_ERROR;
      shortcut = A68_TRUE;
    } else if (ERROR_COUNT (&program) > MAX_ERRORS) {
      ERROR_COUNT (&program)++;
      return;
    }
  } else if (sev == A68_WARNING) {
    if (WARNING_COUNT (&program) == MAX_ERRORS) {
      bufcpy (b, "further warning diagnostics suppressed", BUFFER_SIZE);
      shortcut = A68_TRUE;
    } else if (WARNING_COUNT (&program) > MAX_ERRORS) {
      WARNING_COUNT (&program)++;
      return;
    }
  }
  if (shortcut == A68_FALSE) {
/* Synthesize diagnostic message */
    COMPOSE_DIAGNOSTIC;
/* Add information from errno, if any */
    if (err != 0) {
      char *loc_str2 = new_string (ERROR_SPECIFICATION);
      if (loc_str2 != NO_TEXT) {
        char *stu;
        bufcat (b, " (", BUFFER_SIZE);
        for (stu = loc_str2; stu[0] != NULL_CHAR; stu++) {
          stu[0] = (char) TO_LOWER (stu[0]);
        }
        bufcat (b, loc_str2, BUFFER_SIZE);
        bufcat (b, ")", BUFFER_SIZE);
      }
    }
  }
/* Construct a diagnostic message */
  if (sev == A68_WARNING) {
    WARNING_COUNT (&program)++;
  } else {
    ERROR_COUNT (&program)++;
  }
  if (p == NO_NODE) {
    write_diagnostic (sev, b);
  } else {
    add_diagnostic (NO_LINE, NO_TEXT, p, sev, b);
  }
  va_end (args);
}

/*!
\brief give a diagnostic message
\param sev severity
\param p position in tree
\param loc_str message string
\param ... various arguments needed by special symbols in loc_str
**/

void diagnostic_line (int sev, LINE_T * line, char *pos, char *loc_str, ...)
{
  va_list args;
  MOID_T *moid = NO_MOID;
  char *t = loc_str, b[BUFFER_SIZE];
  BOOL_T force, extra_syntax = A68_TRUE, shortcut = A68_FALSE;
  int err = errno;
  NODE_T *p = NO_NODE;
  b[0] = NULL_CHAR;
  va_start (args, loc_str);
  force = (BOOL_T) ((sev & A68_FORCE_DIAGNOSTICS) != 0);
  sev &= ~A68_FORCE_DIAGNOSTICS;
/* No warnings? */
  if (!force && sev == A68_WARNING && OPTION_NO_WARNINGS (&program)) {
    return;
  }
  if (sev == A68_WARNING && OPTION_QUIET (&program)) {
    return;
  }
/* Suppressed? */
  if (sev == A68_ERROR || sev == A68_SYNTAX_ERROR) {
    if (ERROR_COUNT (&program) == MAX_ERRORS) {
      bufcpy (b, "further error diagnostics suppressed", BUFFER_SIZE);
      sev = A68_ERROR;
      shortcut = A68_TRUE;
    } else if (ERROR_COUNT (&program) > MAX_ERRORS) {
      ERROR_COUNT (&program)++;
      return;
    }
  } else if (sev == A68_WARNING) {
    if (WARNING_COUNT (&program) == MAX_ERRORS) {
      bufcpy (b, "further warning diagnostics suppressed", BUFFER_SIZE);
      shortcut = A68_TRUE;
    } else if (WARNING_COUNT (&program) > MAX_ERRORS) {
      WARNING_COUNT (&program)++;
      return;
    }
  }
  if (!shortcut) {
/* Synthesize diagnostic message */
    COMPOSE_DIAGNOSTIC;
/* Add information from errno, if any */
    if (err != 0) {
      char *loc_str2 = new_string (ERROR_SPECIFICATION);
      if (loc_str2 != NO_TEXT) {
        char *stu;
        bufcat (b, " (", BUFFER_SIZE);
        for (stu = loc_str2; stu[0] != NULL_CHAR; stu++) {
          stu[0] = (char) TO_LOWER (stu[0]);
        }
        bufcat (b, loc_str2, BUFFER_SIZE);
        bufcat (b, ")", BUFFER_SIZE);
      }
    }
  }
/* Construct a diagnostic message */
  if (pos != NO_TEXT && IS_PRINT (*pos)) {
    bufcat (b, " (detected at", BUFFER_SIZE);
    if (*pos == '\"') {
      bufcat (b, " quote-character", BUFFER_SIZE);
    } else {
      bufcat (b, " character \"", BUFFER_SIZE);
      bufcat (b, char_to_str (*pos), BUFFER_SIZE);
      bufcat (b, "\"", BUFFER_SIZE);
    }
    bufcat (b, ")", BUFFER_SIZE);
  }
  if (sev == A68_WARNING) {
    WARNING_COUNT (&program)++;
  } else {
    ERROR_COUNT (&program)++;
  }
  if (line == NO_LINE) {
    write_diagnostic (sev, b);
  } else {
    add_diagnostic (line, pos, NO_NODE, sev, b);
  }
  va_end (args);
}

/*!
\brief add keyword to the tree
\param p top keyword
\param a attribute
\param t keyword text
**/

static void add_keyword (KEYWORD_T ** p, int a, char *t)
{
  while (*p != NO_KEYWORD) {
    int k = strcmp (t, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else {
      p = &MORE (*p);
    }
  }
  *p = (KEYWORD_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (KEYWORD_T));
  ATTRIBUTE (*p) = a;
  TEXT (*p) = t;
  LESS (*p) = MORE (*p) = NO_KEYWORD;
}

/*!
\brief make tables of keywords and non-terminals
**/

void set_up_tables (void)
{
/* Entries are randomised to balance the tree */
  if (OPTION_STRICT (&program) == A68_FALSE) {
    add_keyword (&top_keyword, ENVIRON_SYMBOL, "ENVIRON");
    add_keyword (&top_keyword, DOWNTO_SYMBOL, "DOWNTO");
    add_keyword (&top_keyword, UNTIL_SYMBOL, "UNTIL");
    add_keyword (&top_keyword, CLASS_SYMBOL, "CLASS");
    add_keyword (&top_keyword, NEW_SYMBOL, "NEW");
    add_keyword (&top_keyword, DIAGONAL_SYMBOL, "DIAG");
    add_keyword (&top_keyword, TRANSPOSE_SYMBOL, "TRNSP");
    add_keyword (&top_keyword, ROW_SYMBOL, "ROW");
    add_keyword (&top_keyword, COLUMN_SYMBOL, "COL");
    add_keyword (&top_keyword, ROW_ASSIGN_SYMBOL, "::=");
    add_keyword (&top_keyword, CODE_SYMBOL, "CODE");
    add_keyword (&top_keyword, EDOC_SYMBOL, "EDOC");
    add_keyword (&top_keyword, ANDF_SYMBOL, "THEF");
    add_keyword (&top_keyword, ORF_SYMBOL, "ELSF");
    add_keyword (&top_keyword, ANDF_SYMBOL, "ANDTH");
    add_keyword (&top_keyword, ORF_SYMBOL, "OREL");
    add_keyword (&top_keyword, ANDF_SYMBOL, "ANDF");
    add_keyword (&top_keyword, ORF_SYMBOL, "ORF");
  }
  add_keyword (&top_keyword, POINT_SYMBOL, ".");
  add_keyword (&top_keyword, COMPLEX_SYMBOL, "COMPLEX");
  add_keyword (&top_keyword, ACCO_SYMBOL, "{");
  add_keyword (&top_keyword, OCCA_SYMBOL, "}");
  add_keyword (&top_keyword, SOUND_SYMBOL, "SOUND");
  add_keyword (&top_keyword, COLON_SYMBOL, ":");
  add_keyword (&top_keyword, THEN_BAR_SYMBOL, "|");
  add_keyword (&top_keyword, SUB_SYMBOL, "[");
  add_keyword (&top_keyword, BY_SYMBOL, "BY");
  add_keyword (&top_keyword, OP_SYMBOL, "OP");
  add_keyword (&top_keyword, COMMA_SYMBOL, ",");
  add_keyword (&top_keyword, AT_SYMBOL, "AT");
  add_keyword (&top_keyword, PRIO_SYMBOL, "PRIO");
  add_keyword (&top_keyword, STYLE_I_COMMENT_SYMBOL, "CO");
  add_keyword (&top_keyword, END_SYMBOL, "END");
  add_keyword (&top_keyword, GO_SYMBOL, "GO");
  add_keyword (&top_keyword, TO_SYMBOL, "TO");
  add_keyword (&top_keyword, ELSE_BAR_SYMBOL, "|:");
  add_keyword (&top_keyword, THEN_SYMBOL, "THEN");
  add_keyword (&top_keyword, TRUE_SYMBOL, "TRUE");
  add_keyword (&top_keyword, PROC_SYMBOL, "PROC");
  add_keyword (&top_keyword, FOR_SYMBOL, "FOR");
  add_keyword (&top_keyword, GOTO_SYMBOL, "GOTO");
  add_keyword (&top_keyword, WHILE_SYMBOL, "WHILE");
  add_keyword (&top_keyword, IS_SYMBOL, ":=:");
  add_keyword (&top_keyword, ASSIGN_TO_SYMBOL, "=:");
  add_keyword (&top_keyword, COMPL_SYMBOL, "COMPL");
  add_keyword (&top_keyword, FROM_SYMBOL, "FROM");
  add_keyword (&top_keyword, BOLD_PRAGMAT_SYMBOL, "PRAGMAT");
  add_keyword (&top_keyword, BOLD_COMMENT_SYMBOL, "COMMENT");
  add_keyword (&top_keyword, DO_SYMBOL, "DO");
  add_keyword (&top_keyword, STYLE_II_COMMENT_SYMBOL, "#");
  add_keyword (&top_keyword, CASE_SYMBOL, "CASE");
  add_keyword (&top_keyword, LOC_SYMBOL, "LOC");
  add_keyword (&top_keyword, CHAR_SYMBOL, "CHAR");
  add_keyword (&top_keyword, ISNT_SYMBOL, ":/=:");
  add_keyword (&top_keyword, REF_SYMBOL, "REF");
  add_keyword (&top_keyword, NIL_SYMBOL, "NIL");
  add_keyword (&top_keyword, ASSIGN_SYMBOL, ":=");
  add_keyword (&top_keyword, FI_SYMBOL, "FI");
  add_keyword (&top_keyword, FILE_SYMBOL, "FILE");
  add_keyword (&top_keyword, PAR_SYMBOL, "PAR");
  add_keyword (&top_keyword, ASSERT_SYMBOL, "ASSERT");
  add_keyword (&top_keyword, OUSE_SYMBOL, "OUSE");
  add_keyword (&top_keyword, IN_SYMBOL, "IN");
  add_keyword (&top_keyword, LONG_SYMBOL, "LONG");
  add_keyword (&top_keyword, SEMI_SYMBOL, ";");
  add_keyword (&top_keyword, EMPTY_SYMBOL, "EMPTY");
  add_keyword (&top_keyword, MODE_SYMBOL, "MODE");
  add_keyword (&top_keyword, IF_SYMBOL, "IF");
  add_keyword (&top_keyword, OD_SYMBOL, "OD");
  add_keyword (&top_keyword, OF_SYMBOL, "OF");
  add_keyword (&top_keyword, STRUCT_SYMBOL, "STRUCT");
  add_keyword (&top_keyword, STYLE_I_PRAGMAT_SYMBOL, "PR");
  add_keyword (&top_keyword, BUS_SYMBOL, "]");
  add_keyword (&top_keyword, SKIP_SYMBOL, "SKIP");
  add_keyword (&top_keyword, SHORT_SYMBOL, "SHORT");
  add_keyword (&top_keyword, IS_SYMBOL, "IS");
  add_keyword (&top_keyword, ESAC_SYMBOL, "ESAC");
  add_keyword (&top_keyword, CHANNEL_SYMBOL, "CHANNEL");
  add_keyword (&top_keyword, REAL_SYMBOL, "REAL");
  add_keyword (&top_keyword, STRING_SYMBOL, "STRING");
  add_keyword (&top_keyword, BOOL_SYMBOL, "BOOL");
  add_keyword (&top_keyword, ISNT_SYMBOL, "ISNT");
  add_keyword (&top_keyword, FALSE_SYMBOL, "FALSE");
  add_keyword (&top_keyword, UNION_SYMBOL, "UNION");
  add_keyword (&top_keyword, OUT_SYMBOL, "OUT");
  add_keyword (&top_keyword, OPEN_SYMBOL, "(");
  add_keyword (&top_keyword, BEGIN_SYMBOL, "BEGIN");
  add_keyword (&top_keyword, FLEX_SYMBOL, "FLEX");
  add_keyword (&top_keyword, VOID_SYMBOL, "VOID");
  add_keyword (&top_keyword, BITS_SYMBOL, "BITS");
  add_keyword (&top_keyword, ELSE_SYMBOL, "ELSE");
  add_keyword (&top_keyword, EXIT_SYMBOL, "EXIT");
  add_keyword (&top_keyword, HEAP_SYMBOL, "HEAP");
  add_keyword (&top_keyword, INT_SYMBOL, "INT");
  add_keyword (&top_keyword, BYTES_SYMBOL, "BYTES");
  add_keyword (&top_keyword, PIPE_SYMBOL, "PIPE");
  add_keyword (&top_keyword, FORMAT_SYMBOL, "FORMAT");
  add_keyword (&top_keyword, SEMA_SYMBOL, "SEMA");
  add_keyword (&top_keyword, CLOSE_SYMBOL, ")");
  add_keyword (&top_keyword, AT_SYMBOL, "@");
  add_keyword (&top_keyword, ELIF_SYMBOL, "ELIF");
  add_keyword (&top_keyword, FORMAT_DELIMITER_SYMBOL, "$");
}

/* Next are routines to calculate the size of a mode */

/*!
\brief max unitings to simplout
\param p position in tree
\param max maximum calculated moid size
**/

static void max_unitings_to_simplout (NODE_T * p, int *max)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNITING) && MOID (p) == MODE (SIMPLOUT)) {
      MOID_T *q = MOID (SUB (p));
      if (q != MODE (SIMPLOUT)) {
        int size = moid_size (q);
        if (size > *max) {
          *max = size;
        }
      }
    }
    max_unitings_to_simplout (SUB (p), max);
  }
}

/*!
\brief get max simplout size
\param p position in tree
**/

void get_max_simplout_size (NODE_T * p)
{
  max_simplout_size = ALIGNED_SIZE_OF (A68_REF); /* For anonymous SKIP */
  max_unitings_to_simplout (p, &max_simplout_size);
}

/*!
\brief set moid sizes
\param z moid to start from
**/

void set_moid_sizes (MOID_T * z)
{
  for (; z != NO_MOID; FORWARD (z)) {
    SIZE (z) = moid_size (z);
  }
}

/*!
\brief moid size 2
\param p moid to calculate
\return moid size
**/

static int moid_size_2 (MOID_T * p)
{
  if (p == NO_MOID) {
    return (0);
  } else if (EQUIVALENT (p) != NO_MOID) {
    return (moid_size_2 (EQUIVALENT (p)));
  } else if (p == MODE (HIP)) {
    return (0);
  } else if (p == MODE (VOID)) {
    return (0);
  } else if (p == MODE (INT)) {
    return (ALIGNED_SIZE_OF (A68_INT));
  } else if (p == MODE (LONG_INT)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_INT)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (REAL)) {
    return (ALIGNED_SIZE_OF (A68_REAL));
  } else if (p == MODE (LONG_REAL)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_REAL)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (BOOL)) {
    return (ALIGNED_SIZE_OF (A68_BOOL));
  } else if (p == MODE (CHAR)) {
    return (ALIGNED_SIZE_OF (A68_CHAR));
  } else if (p == MODE (ROW_CHAR)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (BITS)) {
    return (ALIGNED_SIZE_OF (A68_BITS));
  } else if (p == MODE (LONG_BITS)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_BITS)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (BYTES)) {
    return (ALIGNED_SIZE_OF (A68_BYTES));
  } else if (p == MODE (LONG_BYTES)) {
    return (ALIGNED_SIZE_OF (A68_LONG_BYTES));
  } else if (p == MODE (FILE)) {
    return (ALIGNED_SIZE_OF (A68_FILE));
  } else if (p == MODE (CHANNEL)) {
    return (ALIGNED_SIZE_OF (A68_CHANNEL));
  } else if (p == MODE (FORMAT)) {
    return (ALIGNED_SIZE_OF (A68_FORMAT));
  } else if (p == MODE (SEMA)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (SOUND)) {
    return (ALIGNED_SIZE_OF (A68_SOUND));
  } else if (p == MODE (COLLITEM)) {
    return (ALIGNED_SIZE_OF (A68_COLLITEM));
  } else if (p == MODE (NUMBER)) {
    int k = 0;
    if (ALIGNED_SIZE_OF (A68_INT) > k) {
      k = ALIGNED_SIZE_OF (A68_INT);
    }
    if ((int) size_long_mp () > k) {
      k = (int) size_long_mp ();
    }
    if ((int) size_longlong_mp () > k) {
      k = (int) size_longlong_mp ();
    }
    if (ALIGNED_SIZE_OF (A68_REAL) > k) {
      k = ALIGNED_SIZE_OF (A68_REAL);
    }
    if ((int) size_long_mp () > k) {
      k = (int) size_long_mp ();
    }
    if ((int) size_longlong_mp () > k) {
      k = (int) size_longlong_mp ();
    }
    if (ALIGNED_SIZE_OF (A68_REF) > k) {
      k = ALIGNED_SIZE_OF (A68_REF);
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + k);
  } else if (p == MODE (SIMPLIN)) {
    int k = 0;
    if (ALIGNED_SIZE_OF (A68_REF) > k) {
      k = ALIGNED_SIZE_OF (A68_REF);
    }
    if (ALIGNED_SIZE_OF (A68_FORMAT) > k) {
      k = ALIGNED_SIZE_OF (A68_FORMAT);
    }
    if (ALIGNED_SIZE_OF (A68_PROCEDURE) > k) {
      k = ALIGNED_SIZE_OF (A68_PROCEDURE);
    }
    if (ALIGNED_SIZE_OF (A68_SOUND) > k) {
      k = ALIGNED_SIZE_OF (A68_SOUND);
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + k);
  } else if (p == MODE (SIMPLOUT)) {
    return (ALIGNED_SIZE_OF (A68_UNION) + max_simplout_size);
  } else if (IS (p, REF_SYMBOL)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (IS (p, PROC_SYMBOL)) {
    return (ALIGNED_SIZE_OF (A68_PROCEDURE));
  } else if (IS (p, ROW_SYMBOL) && p != MODE (ROWS)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (ROWS)) {
    return (ALIGNED_SIZE_OF (A68_UNION) + ALIGNED_SIZE_OF (A68_REF));
  } else if (IS (p, FLEX_SYMBOL)) {
    return moid_size (SUB (p));
  } else if (IS (p, STRUCT_SYMBOL)) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NO_PACK; FORWARD (z)) {
      size += moid_size (MOID (z));
    }
    return (size);
  } else if (IS (p, UNION_SYMBOL)) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NO_PACK; FORWARD (z)) {
      if (moid_size (MOID (z)) > size) {
        size = moid_size (MOID (z));
      }
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + size);
  } else if (PACK (p) != NO_PACK) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NO_PACK; FORWARD (z)) {
      size += moid_size (MOID (z));
    }
    return (size);
  } else {
/* ? */
    return (0);
  }
}

/*!
\brief moid size
\param p moid to set size
\return moid size
**/

int moid_size (MOID_T * p)
{
  SIZE (p) = moid_size_2 (p);
  return (SIZE (p));
}

/******************************/
/* A pretty printer for moids */
/******************************/

/*!
\brief moid to string 3
\param dst text buffer
\param str string to concatenate
\param w estimated width
\param idf print indicants if one exists in this range
**/

static void add_to_moid_text (char *dst, char *str, int *w)
{
  bufcat (dst, str, BUFFER_SIZE);
  (*w) -= (int) strlen (str);
}

/*!
\brief find a tag, searching symbol tables towards the root
\param table symbol table to search
\param mode mode of the tag
\return entry in symbol table
**/

TAG_T *find_indicant_global (TABLE_T * table, MOID_T * mode)
{
  if (table != NO_TABLE) {
    TAG_T *s;
    for (s = INDICANTS (table); s != NO_TAG; FORWARD (s)) {
      if (MOID (s) == mode) {
        return (s);
      }
    }
    return (find_indicant_global (PREVIOUS (table), mode));
  } else {
    return (NO_TAG);
  }
}

/*!
\brief pack to string
\param b text buffer
\param p pack
\param w estimated width
\param text include field names
\param idf print indicants if one exists in this range
**/

static void pack_to_string (char *b, PACK_T * p, int *w, BOOL_T text, NODE_T * idf)
{
  for (; p != NO_PACK; FORWARD (p)) {
    moid_to_string_2 (b, MOID (p), w, idf);
    if (text) {
      if (TEXT (p) != NO_TEXT) {
        add_to_moid_text (b, " ", w);
        add_to_moid_text (b, TEXT (p), w);
      }
    }
    if (p != NO_PACK && NEXT (p) != NO_PACK) {
      add_to_moid_text (b, ", ", w);
    }
  }
}

/*!
\brief moid to string 2
\param b text buffer
\param n moid
\param w estimated width
\param idf print indicants if one exists in this range
**/

static void moid_to_string_2 (char *b, MOID_T * n, int *w, NODE_T * idf)
{
/* Oops. Should not happen */
  if (n == NO_MOID) {
    add_to_moid_text (b, "null", w);;
    return;
  }
/* Reference to self through REF or PROC */
  if (is_postulated (postulates, n)) {
    add_to_moid_text (b, "SELF", w);
    return;
  }
/* If declared by a mode-declaration, present the indicant */
  if (idf != NO_NODE && ISNT (n, STANDARD)) {
    TAG_T *indy = find_indicant_global (TABLE (idf), n);
    if (indy != NO_TAG) {
      add_to_moid_text (b, NSYMBOL (NODE (indy)), w);
      return;
    }
  }
/* Write the standard modes */
  if (n == MODE (HIP)) {
    add_to_moid_text (b, "HIP", w);
  } else if (n == MODE (ERROR)) {
    add_to_moid_text (b, "ERROR", w);
  } else if (n == MODE (UNDEFINED)) {
    add_to_moid_text (b, "unresolved", w);
  } else if (n == MODE (C_STRING)) {
    add_to_moid_text (b, "C-STRING", w);
  } else if (n == MODE (COMPLEX) || n == MODE (COMPL)) {
    add_to_moid_text (b, "COMPLEX", w);
  } else if (n == MODE (LONG_COMPLEX) || n == MODE (LONG_COMPL)) {
    add_to_moid_text (b, "LONG COMPLEX", w);
  } else if (n == MODE (LONGLONG_COMPLEX) || n == MODE (LONGLONG_COMPL)) {
    add_to_moid_text (b, "LONG LONG COMPLEX", w);
  } else if (n == MODE (STRING)) {
    add_to_moid_text (b, "STRING", w);
  } else if (n == MODE (PIPE)) {
    add_to_moid_text (b, "PIPE", w);
  } else if (n == MODE (SOUND)) {
    add_to_moid_text (b, "SOUND", w);
  } else if (n == MODE (COLLITEM)) {
    add_to_moid_text (b, "COLLITEM", w);
  } else if (IS (n, IN_TYPE_MODE)) {
    add_to_moid_text (b, "\"SIMPLIN\"", w);
  } else if (IS (n, OUT_TYPE_MODE)) {
    add_to_moid_text (b, "\"SIMPLOUT\"", w);
  } else if (IS (n, ROWS_SYMBOL)) {
    add_to_moid_text (b, "\"ROWS\"", w);
  } else if (n == MODE (VACUUM)) {
    add_to_moid_text (b, "\"VACUUM\"", w);
  } else if (IS (n, VOID_SYMBOL) || IS (n, STANDARD) || IS (n, INDICANT)) {
    if (DIM (n) > 0) {
      int k = DIM (n);
      if ((*w) >= k * (int) strlen ("LONG ") + (int) strlen (NSYMBOL (NODE (n)))) {
        while (k --) {
          add_to_moid_text (b, "LONG ", w);
        }
        add_to_moid_text (b, NSYMBOL (NODE (n)), w);
      } else {
        add_to_moid_text (b, "..", w);
      }
    } else if (DIM (n) < 0) {
      int k = -DIM (n);
      if ((*w) >= k * (int) strlen ("LONG ") + (int) strlen (NSYMBOL (NODE (n)))) {
        while (k --) {
          add_to_moid_text (b, "LONG ", w);
        }
        add_to_moid_text (b, NSYMBOL (NODE (n)), w);
      } else {
        add_to_moid_text (b, "..", w);
      }
    } else if (DIM (n) == 0) {
      add_to_moid_text (b, NSYMBOL (NODE (n)), w);
    }
/* Write compounded modes */
  } else if (IS (n, REF_SYMBOL)) {
    if ((*w) >= (int) strlen ("REF ..")) {
      add_to_moid_text (b, "REF ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "REF ..", w);
    }
  } else if (IS (n, FLEX_SYMBOL)) {
    if ((*w) >= (int) strlen ("FLEX ..")) {
      add_to_moid_text (b, "FLEX ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "FLEX ..", w);
    }
  } else if (IS (n, ROW_SYMBOL)) {
    int j = (int) strlen ("[] ..") + (DIM (n) - 1) * (int) strlen (",");
    if ((*w) >= j) {
      int k = DIM (n) - 1;
      add_to_moid_text (b, "[", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, "] ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else if (DIM (n) == 1) {
      add_to_moid_text (b, "[] ..", w);
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "[", w);
      while (k--) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, "] ..", w);
    }
  } else if (IS (n, STRUCT_SYMBOL)) {
    int j = (int) strlen ("STRUCT ()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NO_MOID);
      add_to_moid_text (b, "STRUCT (", w);
      pack_to_string (b, PACK (n), w, A68_TRUE, idf);
      add_to_moid_text (b, ")", w);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "STRUCT (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else if (IS (n, UNION_SYMBOL)) {
    int j = (int) strlen ("UNION ()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NO_MOID);
      add_to_moid_text (b, "UNION (", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ")", w);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "UNION (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else if (IS (n, PROC_SYMBOL) && DIM (n) == 0) {
    if ((*w) >= (int) strlen ("PROC ..")) {
      add_to_moid_text (b, "PROC ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "PROC ..", w);
    }
  } else if (IS (n, PROC_SYMBOL) && DIM (n) > 0) {
    int j = (int) strlen ("PROC () ..") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NO_MOID);
      add_to_moid_text (b, "PROC (", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ") ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "PROC (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ") ..", w);
    }
  } else if (IS (n, SERIES_MODE) || IS (n, STOWED_MODE)) {
    int j = (int) strlen ("()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      add_to_moid_text (b, "(", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ")", w);
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "(", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else {
    char str[SMALL_BUFFER_SIZE];
    ASSERT (snprintf (str, (size_t) SMALL_BUFFER_SIZE, "\\%d", ATTRIBUTE (n)) >= 0);
    add_to_moid_text (b, str, w);
  }
}

/*!
\brief pretty-formatted mode "n"; "w" is a measure of width
\param n moid
\param w estimated width; if w is exceeded, modes are abbreviated
\param idf print indicants if one exists in this range
\return text buffer
**/

char *moid_to_string (MOID_T * n, int w, NODE_T * idf)
{
  char a[BUFFER_SIZE];
  a[0] = NULL_CHAR;
  if (w >= BUFFER_SIZE) {
    w = BUFFER_SIZE - 1;
  }
  postulates = NO_POSTULATE;
  if (n != NO_MOID) {
    moid_to_string_2 (a, n, &w, idf);
  } else {
    bufcat (a, "null", BUFFER_SIZE);
  }
  return (new_string (a));
}
