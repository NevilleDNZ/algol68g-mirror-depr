/*!
\file algol68g.c
\brief
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

/*
Algol68G is an Algol 68 interpreter.

Please refer to the documentation that comes with this distribution for a
detailed description of Algol68G.
*/

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

int global_argc;                /* Keep argc and argv for reference from A68. */
char **global_argv;

#if defined ENABLE_TERMINFO
#include <term.h>
char term_buffer[2 * KILOBYTE];
char *term_type;
#endif
int term_width;

BYTE_T *system_stack_offset;
MODES_T a68_modes;
MODULE_T a68_prog;
char a68g_cmd_name[BUFFER_SIZE];
int stack_size;
int symbol_table_count, mode_count;
int new_nodes, new_modes, new_postulates, new_node_infos, new_genie_infos;

static void announce_phase (char *);
static void compiler_interpreter (void);

/*!
\brief state license of running a68g image
\param f file number
**/

void state_license (FILE_T f)
{
#define P(s)\
  CHECK_RETVAL (snprintf(output_line, (size_t) BUFFER_SIZE, "%s\n", (s)) >= 0);\
  WRITE (f, output_line);
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Algol 68 Genie %s (%s), copyright 2001-%s J. Marcel van der Veer.\n", REVISION, RELEASE_DATE, RELEASE_YEAR) >= 0);
  WRITE (f, output_line);
  P ("Algol 68 Genie is free software covered by the GNU General Public License.");
  P ("There is ABSOLUTELY NO WARRANTY for Algol 68 Genie.");
  P ("See the GNU General Public License for more details.");
  P ("");
#undef P
}

/*!
\brief state version of running a68g image
\param f file number
**/

void state_version (FILE_T f)
{
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  state_license (f);
  WRITELN (f, "");
#if ! defined ENABLE_WIN32
#if defined __GNUC__ && defined GCC_VERSION
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Compiled on %s with gcc %s\n", OS_NAME, GCC_VERSION) >= 0);
  WRITE (f, output_line);
#else
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Compiled on %s\n", OS_NAME) >= 0);
  WRITE (f, output_line);
#endif
#endif
#if ! defined ENABLE_WIN32
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Configured on %s with options \"%s\"\n", CONFIGURE_DATE, CONFIGURE_OPTIONS) >= 0);
  WRITE (f, output_line);
#endif
#if defined ENABLE_GRAPHICS && defined A68_LIBPLOT_VERSION
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "GNU libplot %s\n", A68_LIBPLOT_VERSION) >= 0);
  WRITE (f, output_line);
#endif
#if defined ENABLE_NUMERICAL && defined A68_GSL_VERSION
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "GNU Scientific Library %s\n", A68_GSL_VERSION) >= 0);
  WRITE (f, output_line);
#endif
#if defined ENABLE_POSTGRESQL && defined A68_PG_VERSION
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "PostgreSQL libpq %s\n", A68_PG_VERSION) >= 0);
  WRITE (f, output_line);
#endif
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Alignment %d bytes\n", A68_ALIGNMENT) >= 0);
  WRITE (f, output_line);
  default_mem_sizes ();
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Default frame stack size: %d kB\n", frame_stack_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Default expression stack size: %d kB\n", expr_stack_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Default heap size: %d kB\n", heap_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Default handle pool size: %d kB\n", handle_pool_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Default stack overhead: %d kB\n", storage_overhead / KILOBYTE) >= 0);
  WRITE (f, output_line);
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Effective system stack size: %d kB\n", stack_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
}

/*!
\brief give brief help if someone types 'a68g -help'
\param f file number
**/

void online_help (FILE_T f)
{
#define P(s)\
  CHECK_RETVAL (snprintf(output_line, (size_t) BUFFER_SIZE, "%s\n", (s)) >= 0);\
  WRITE (f, output_line);
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  state_license (f);
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "Usage: %s [options | filename]", a68g_cmd_name) >= 0);
  WRITELN (f, output_line);
  CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "For help: %s -apropos [keyword]", a68g_cmd_name) >= 0);
  WRITELN (f, output_line);
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
/* Get command name and discard path. */
  bufcpy (a68g_cmd_name, argv[0], BUFFER_SIZE);
  for (k = (int) strlen (a68g_cmd_name) - 1; k >= 0; k--) {
#if defined ENABLE_WIN32
    char delim = '\\';
#else
    char delim = '/';
#endif
    if (a68g_cmd_name[k] == delim) {
      MOVE (&a68g_cmd_name[0], &a68g_cmd_name[k + 1], (int) strlen (a68g_cmd_name) - k + 1);
      k = -1;
    }
  }
/* Try to read maximum line width on the terminal. */
#if defined ENABLE_TERMINFO
  term_type = getenv ("TERM");
  if (term_type == NULL) {
    term_width = MAX_LINE_WIDTH;
  } else if (tgetent (term_buffer, term_type) < 0) {
    term_width = MAX_LINE_WIDTH;
  } else {
    term_width = tgetnum ("co");
  }
#else
  term_width = MAX_LINE_WIDTH;
#endif
#if defined ENABLE_PAR_CLAUSE
  main_thread_id = pthread_self ();
#endif
  get_fixed_heap_allowed = A68_TRUE;
  system_stack_offset = &stack_offset;
  if (!setjmp (a68_prog.exit_compilation)) {
    init_tty ();
/* Initialise option handling. */
    init_options (&a68_prog);
    a68_prog.source_scan = 1;
    default_options (&a68_prog);
    default_mem_sizes ();
/* Initialise core. */
    stack_segment = NULL;
    heap_segment = NULL;
    handle_segment = NULL;
    get_stack_size ();
/* Well, let's start. */
    a68_prog.top_refinement = NULL;
    a68_prog.files.generic_name = NULL;
    a68_prog.files.source.name = NULL;
    a68_prog.files.listing.name = NULL;
/* Options are processed here. */
    read_rc_options (&a68_prog);
    read_env_options (&a68_prog);
/* Posix copies arguments from the command line. */
    if (argc <= 1) {
      online_help (STDOUT_FILENO);
      a68g_exit (EXIT_FAILURE);
    }
    for (argcc = 1; argcc < argc; argcc++) {
      add_option_list (&(a68_prog.options.list), argv[argcc], NULL);
    }
    if (!set_options (&a68_prog, a68_prog.options.list, A68_TRUE)) {
      a68g_exit (EXIT_FAILURE);
    }
    if (a68_prog.options.regression_test) {
      bufcpy (a68g_cmd_name, "a68g", BUFFER_SIZE);
    }
/* Attention for -version. */
    if (a68_prog.options.version) {
      state_version (STDOUT_FILENO);
    }
/* We translate the program. */
    if (a68_prog.files.generic_name == NULL || strlen (a68_prog.files.generic_name) == 0) {
      SCAN_ERROR (!a68_prog.options.version, NULL, NULL, ERROR_NO_INPUT_FILE);
    } else {
      compiler_interpreter ();
    }
    a68g_exit (a68_prog.error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    return (EXIT_SUCCESS);
  } else {
    diagnostics_to_terminal (a68_prog.top_line, A68_ALL_DIAGNOSTICS);
    a68g_exit (EXIT_FAILURE);
    return (EXIT_FAILURE);
  }
}

/*!
\brief try opening with a silent extension
\param ext extension to try
**/

static void whether_extension (char *ext)
{
  if (a68_prog.files.source.fd == -1) {
    int len = (int) strlen (a68_prog.files.source.name) + (int) strlen (ext) + 1;
    char *fn2 = (char *) get_heap_space ((size_t) len);
    bufcpy (fn2, a68_prog.files.source.name, len);
    bufcat (fn2, ext, len);
    a68_prog.files.source.fd = open (fn2, O_RDONLY | O_BINARY);
    if (a68_prog.files.source.fd != -1) {
      a68_prog.files.source.name = new_string (fn2);
    }
  }
}

/*!
\brief initialise before tokenisation
**/

static void init_before_tokeniser (void)
{
/* Heap management set-up. */
  init_heap ();
  top_keyword = NULL;
  top_token = NULL;
  a68_prog.top_node = NULL;
  a68_prog.top_line = NULL;
  set_up_tables ();
/* Various initialisations. */
  a68_prog.error_count = a68_prog.warning_count = 0;
  RESET_ERRNO;
}

/*!
\brief pretty print memory size
**/

char *pretty_size (int k) {
  if (k >= 10 * MEGABYTE) {
    CHECK_RETVAL (snprintf (edit_line, (size_t) BUFFER_SIZE, "%dM", k / MEGABYTE) >= 0);
  } else if (k >= 10 * KILOBYTE) {
    CHECK_RETVAL (snprintf (edit_line, (size_t) BUFFER_SIZE, "%dk", k / KILOBYTE) >= 0);
  } else {
    CHECK_RETVAL (snprintf (edit_line, (size_t) BUFFER_SIZE, "%d", k) >= 0);
  }
  return (edit_line);
}

/*!
\brief verbose statistics
**/

static void verbosity (MODULE_T *m)
{
  if (m->options.verbose) {
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "\nNodes: %dx%d=%s", new_nodes, (int) sizeof (NODE_T), pretty_size (new_nodes * (int) sizeof (NODE_T))) >= 0);
    WRITE (STDERR_FILENO, output_line);
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "\nNode infos: %dx%d=%s", new_node_infos, (int) sizeof (NODE_INFO_T), pretty_size (new_node_infos * (int) sizeof (NODE_INFO_T))) >= 0);
    WRITE (STDERR_FILENO, output_line);
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "\nGenie infos: %dx%d=%s", new_genie_infos, (int) sizeof (GENIE_INFO_T), pretty_size (new_genie_infos * (int) sizeof (GENIE_INFO_T))) >= 0);
    WRITE (STDERR_FILENO, output_line);
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "\nModes: %dx%d=%s", new_modes, (int) sizeof (MOID_T), pretty_size (new_modes * (int) sizeof (MOID_T))) >= 0);
    WRITE (STDERR_FILENO, output_line);
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "\nPostulates: %dx%d=%s", new_postulates, (int) sizeof (POSTULATE_T), pretty_size (new_postulates * (int) sizeof (POSTULATE_T))) >= 0);
    WRITE (STDERR_FILENO, output_line);
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "\nMemory: %s", pretty_size (temp_heap_pointer)) >= 0);
    WRITE (STDERR_FILENO, output_line);
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "-%s", pretty_size (fixed_heap_pointer)) >= 0);
    WRITE (STDERR_FILENO, output_line);
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "=%s", pretty_size (temp_heap_pointer - fixed_heap_pointer)) >= 0);
    WRITE (STDERR_FILENO, output_line);
  }
}

/*!
\brief drives compilation and interpretation
**/

static void compiler_interpreter (void)
{
  int k, len, num;
  BOOL_T path_set = A68_FALSE;
  a68_prog.tree_listing_safe = A68_FALSE;
  a68_prog.cross_reference_safe = A68_FALSE;
  new_nodes = 0;
  new_modes = 0;
  new_postulates = 0;
  new_node_infos = 0;
  new_genie_infos = 0;
  init_postulates ();
/* File set-up. */
  SCAN_ERROR (a68_prog.files.generic_name == NULL, NULL, NULL, ERROR_NO_INPUT_FILE);
  a68_prog.files.source.name = new_string (a68_prog.files.generic_name);
  a68_prog.files.source.opened = A68_FALSE;
  a68_prog.files.listing.opened = A68_FALSE;
  a68_prog.files.source.writemood = A68_FALSE;
  a68_prog.files.listing.writemood = A68_TRUE;
/*
Open the source file. Open it for binary reading for systems that require so (Win32).
Accept various silent extensions.
*/
  errno = 0;
  a68_prog.files.source.fd = open (a68_prog.files.source.name, O_RDONLY | O_BINARY);
  whether_extension (".a68");
  whether_extension (".A68");
  whether_extension (".a68g");
  whether_extension (".A68G");
  whether_extension (".algol68");
  whether_extension (".ALGOL68");
  whether_extension (".algol68g");
  whether_extension (".ALGOL68G");
  if (a68_prog.files.source.fd == -1) {
    scan_error (NULL, NULL, ERROR_SOURCE_FILE_OPEN);
  }
/* Isolate the path name. */
  a68_prog.files.path = new_string (a68_prog.files.generic_name);
  path_set = A68_FALSE;
  for (k = (int) strlen (a68_prog.files.path); k >= 0 && path_set == A68_FALSE; k--) {
#if defined ENABLE_WIN32
    char delim = '\\';
#else
    char delim = '/';
#endif
    if (a68_prog.files.path[k] == delim) {
      a68_prog.files.path[k + 1] = NULL_CHAR;
      path_set = A68_TRUE;
    }
  }
  if (path_set == A68_FALSE) {
    a68_prog.files.path[0] = NULL_CHAR;
  }
/* Listing file. */
  len = 1 + (int) strlen (a68_prog.files.source.name) + (int) strlen (LISTING_EXTENSION);
  a68_prog.files.listing.name = (char *) get_heap_space ((size_t) len);
  bufcpy (a68_prog.files.listing.name, a68_prog.files.source.name, len);
  bufcat (a68_prog.files.listing.name, LISTING_EXTENSION, len);
/* Tokeniser. */
  a68_prog.files.source.opened = A68_TRUE;
  announce_phase ("initialiser");
  init_before_tokeniser ();
  error_tag = (TAG_T *) new_tag ();
  if (a68_prog.error_count == 0) {
    int frame_stack_size_2 = frame_stack_size;
    int expr_stack_size_2 = expr_stack_size;
    int heap_size_2 = heap_size;
    int handle_pool_size_2 = handle_pool_size;
    BOOL_T ok;
    announce_phase ("tokeniser");
    ok = lexical_analyzer (&a68_prog);
    if (!ok || errno != 0) {
      diagnostics_to_terminal (a68_prog.top_line, A68_ALL_DIAGNOSTICS);
      return;
    }
/* Maybe the program asks for more memory through a PRAGMAT. We restart. */
    if (frame_stack_size_2 != frame_stack_size || expr_stack_size_2 != expr_stack_size || heap_size_2 != heap_size || handle_pool_size_2 != handle_pool_size) {
      discard_heap ();
      init_before_tokeniser ();
      a68_prog.source_scan++;
      ok = lexical_analyzer (&a68_prog);
      verbosity (&a68_prog);
    }
    if (!ok || errno != 0) {
      diagnostics_to_terminal (a68_prog.top_line, A68_ALL_DIAGNOSTICS);
      return;
    }
    CHECK_RETVAL (close (a68_prog.files.source.fd) == 0);
    a68_prog.files.source.opened = A68_FALSE;
    prune_echoes (&a68_prog, a68_prog.options.list);
    a68_prog.tree_listing_safe = A68_TRUE;
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
  }
/* Final initialisations. */
  if (a68_prog.error_count == 0) {
    stand_env = NULL;
    init_postulates ();
    init_moid_list ();
    mode_count = 0;
    make_special_mode (&MODE (HIP), mode_count++);
    make_special_mode (&MODE (UNDEFINED), mode_count++);
    make_special_mode (&MODE (ERROR), mode_count++);
    make_special_mode (&MODE (VACUUM), mode_count++);
    make_special_mode (&MODE (C_STRING), mode_count++);
    make_special_mode (&MODE (COLLITEM), mode_count++);
    make_special_mode (&MODE (SOUND_DATA), mode_count++);
  }
/* Refinement preprocessor. */
  if (a68_prog.error_count == 0) {
    announce_phase ("preprocessor");
    get_refinements (&a68_prog);
    if (a68_prog.error_count == 0) {
      put_refinements (&a68_prog);
    }
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    verbosity (&a68_prog);
  }
/* Top-down parser. */
  if (a68_prog.error_count == 0) {
    announce_phase ("parser phase 1");
    check_parenthesis (a68_prog.top_node);
    if (a68_prog.error_count == 0) {
      if (a68_prog.options.brackets) {
        substitute_brackets (a68_prog.top_node);
      }
      symbol_table_count = 0;
      stand_env = new_symbol_table (NULL);
      stand_env->level = 0;
      top_down_parser (a68_prog.top_node);
    }
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    verbosity (&a68_prog);
  }
/* Standard environment builder. */
  if (a68_prog.error_count == 0) {
    announce_phase ("standard environ builder");
    SYMBOL_TABLE (a68_prog.top_node) = new_symbol_table (stand_env);
    make_standard_environ ();
    verbosity (&a68_prog);
  }
/* Bottom-up parser. */
  if (a68_prog.error_count == 0) {
    announce_phase ("parser phase 2");
    preliminary_symbol_table_setup (a68_prog.top_node);
    bottom_up_parser (a68_prog.top_node);
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    verbosity (&a68_prog);
  }
  if (a68_prog.error_count == 0) {
    announce_phase ("parser phase 3");
    bottom_up_error_check (a68_prog.top_node);
    victal_checker (a68_prog.top_node);
    if (a68_prog.error_count == 0) {
      finalise_symbol_table_setup (a68_prog.top_node, 2);
      SYMBOL_TABLE (a68_prog.top_node)->nest = symbol_table_count = 3;
      reset_symbol_table_nest_count (a68_prog.top_node);
      fill_symbol_table_outer (a68_prog.top_node, SYMBOL_TABLE (a68_prog.top_node));
#if defined ENABLE_PAR_CLAUSE
      set_par_level (a68_prog.top_node, 0);
#endif
      set_nest (a68_prog.top_node, NULL);
      set_proc_level (a68_prog.top_node, 1);
    }
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    verbosity (&a68_prog);
  }
/* Mode table builder. */
  if (a68_prog.error_count == 0) {
    announce_phase ("mode table builder");
    set_up_mode_table (a68_prog.top_node);
    verbosity (&a68_prog);
  }
  a68_prog.cross_reference_safe = /* (BOOL_T) (a68_prog.error_count == 0) */ A68_TRUE;
/* Symbol table builder. */
  if (a68_prog.error_count == 0) {
    announce_phase ("symbol table builder");
    collect_taxes (a68_prog.top_node);
    verbosity (&a68_prog);
  }
/* Post parser. */
  if (a68_prog.error_count == 0) {
    announce_phase ("parser phase 4");
    rearrange_goto_less_jumps (a68_prog.top_node);
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    verbosity (&a68_prog);
  }
/* Mode checker. */
  if (a68_prog.error_count == 0) {
    announce_phase ("mode checker");
    mode_checker (a68_prog.top_node);
    maintain_mode_table (a68_prog.top_node);
    verbosity (&a68_prog);
  }
/* Coercion inserter. */
  if (a68_prog.error_count == 0) {
    announce_phase ("coercion enforcer");
    coercion_inserter (a68_prog.top_node);
    widen_denotation (a68_prog.top_node);
    protect_from_sweep (a68_prog.top_node);
    reset_max_simplout_size ();
    get_max_simplout_size (a68_prog.top_node);
    reset_moid_list ();
    get_moid_list (&top_moid_list, a68_prog.top_node);
    set_moid_sizes (top_moid_list);
    assign_offsets_table (stand_env);
    assign_offsets (a68_prog.top_node);
    assign_offsets_packs (top_moid_list);
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    verbosity (&a68_prog);
  }
/* Application checker. */
  if (a68_prog.error_count == 0) {
    announce_phase ("application checker");
    mark_moids (a68_prog.top_node);
    mark_auxilliary (a68_prog.top_node);
    jumps_from_procs (a68_prog.top_node);
    warn_for_unused_tags (a68_prog.top_node);
    warn_tags_threads (a68_prog.top_node);
    verbosity (&a68_prog);
  }
/* Scope checker. */
  if (a68_prog.error_count == 0) {
    announce_phase ("static scope checker");
    tie_label_to_serial (a68_prog.top_node);
    tie_label_to_unit (a68_prog.top_node);
    bind_routine_tags_to_tree (a68_prog.top_node);
    bind_format_tags_to_tree (a68_prog.top_node);
    scope_checker (a68_prog.top_node);
    verbosity (&a68_prog);
  }
/* Portability checker. */
  if (a68_prog.error_count == 0) {
    announce_phase ("portability checker");
    portcheck (a68_prog.top_node);
    verbosity (&a68_prog);
  }
/* Optimisation, on ongoing project. */
  if (a68_prog.error_count == 0 && a68_prog.options.optimise) {
    announce_phase ("optimiser");
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    SYMBOL_TABLE (a68_prog.top_node)->nest = symbol_table_count = 3;
    reset_symbol_table_nest_count (a68_prog.top_node);
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    verbosity (&a68_prog);
  }
/* Interpreter. */
  diagnostics_to_terminal (a68_prog.top_line, A68_ALL_DIAGNOSTICS);
  if (a68_prog.error_count == 0 && (a68_prog.options.check_only ? a68_prog.options.run : A68_TRUE)) {
    announce_phase ("genie");
    num = 0;
    renumber_nodes (a68_prog.top_node, &num);
    if (a68_prog.options.debug) {
      state_license (STDOUT_FILENO);
    }
    genie (&a68_prog);
/* Free heap allocated by genie. */
    free_genie_heap (a68_prog.top_node);
/* Normal end of program. */
    diagnostics_to_terminal (a68_prog.top_line, A68_RUNTIME_ERROR);
    if (a68_prog.options.debug || a68_prog.options.trace) {
      CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "\nGenie finished in %.2f seconds\n", seconds () - cputime_0) >= 0);
      WRITE (STDOUT_FILENO, output_line);
    }
    verbosity (&a68_prog);
  }
/* Setting up listing file. */
  if (a68_prog.options.moid_listing || a68_prog.options.tree_listing || a68_prog.options.source_listing || a68_prog.options.statistics_listing) {
    a68_prog.files.listing.fd = open (a68_prog.files.listing.name, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
    ABNORMAL_END (a68_prog.files.listing.fd == -1, "cannot open listing file", NULL);
    a68_prog.files.listing.opened = A68_TRUE;
  } else {
    a68_prog.files.listing.opened = A68_FALSE;
  }
/* Write listing. */
  if (a68_prog.files.listing.opened) {
    write_listing_header (&a68_prog);
    source_listing (&a68_prog);
    write_listing (&a68_prog);
    CHECK_RETVAL (close (a68_prog.files.listing.fd) == 0);
    a68_prog.files.listing.opened = A68_FALSE;
    verbosity (&a68_prog);
  }
}

/*!
\brief exit a68g in an orderly manner
\param code exit code
**/

void a68g_exit (int code)
{
  char name[BUFFER_SIZE];
  bufcpy (name, ".", BUFFER_SIZE);
  bufcat (name, a68g_cmd_name, BUFFER_SIZE);
  bufcat (name, ".x", BUFFER_SIZE);
  (void) (remove (name));
  io_close_tty_line ();
#if defined ENABLE_CURSES
/* "curses" might still be open if it was not closed from A68, or the program
   was interrupted, or a runtime error occured. That wreaks havoc on your
   terminal. */
  genie_curses_end (NULL);
#endif
  exit (code);
}

/*!
\brief start bookkeeping for a phase
\param t name of phase
**/

static void announce_phase (char *t)
{
  if (a68_prog.options.verbose) {
    CHECK_RETVAL (snprintf (output_line, (size_t) BUFFER_SIZE, "%s: %s", a68g_cmd_name, t) >= 0);
    io_close_tty_line ();
    WRITE (STDOUT_FILENO, output_line);
  }
}
