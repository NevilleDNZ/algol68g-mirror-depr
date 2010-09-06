/*!
\file algol68g.c
\brief driver routines for the compiler-interpreter.
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
Algol68G is an Algol 68 compiler-interpreter.

Please refer to the documentation that comes with this distribution for a
detailed description of Algol68G.
*/

#include "config.h"
#include "diagnostics.h"
#include "algol68g.h"
#include "genie.h"
#include "mp.h"

int global_argc; /* Keep argc and argv for reference from A68. */
char **global_argv;

#if defined ENABLE_TERMINFO
#include <term.h>
char term_buffer[2 * KILOBYTE];
char *term_type;
#endif
int term_width;

BOOL_T in_execution;
BYTE_T *system_stack_offset;
MODES_T a68_modes;
MODULE_T program;
char a68g_cmd_name[BUFFER_SIZE];
clock_t clock_res;
int stack_size;
int symbol_table_count, mode_count;
int new_nodes, new_modes, new_postulates, new_node_infos, new_genie_infos;
NODE_T **node_register;

#define EXTENSIONS 11
static char * extensions[EXTENSIONS] = {
  NULL,
  ".a68", ".A68",
  ".a68g", ".A68G",
  ".algol", ".ALGOL",
  ".algol68", ".ALGOL68",
  ".algol68g", ".ALGOL68G"
};

static void announce_phase (char *);
static void compiler_interpreter (void);

#if defined ENABLE_COMPILER
static void build_script (void);
static void load_script (void);
static void rewrite_script_source (void);
#endif

/*!
\brief state license of running a68g image
\param f file number
**/

void state_license (FILE_T f)
{
#define P(s)\
  ASSERT (snprintf(output_line, (size_t) BUFFER_SIZE, "%s\n", (s)) >= 0);\
  WRITE (f, output_line);
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Algol 68 Genie %s (%s), copyright 2001-%s J. Marcel van der Veer.\n", REVISION, RELEASE_DATE, RELEASE_YEAR) >= 0);
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
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Compiled on %s with gcc %s\n", OS_NAME, GCC_VERSION) >= 0);
  WRITE (f, output_line);
#else
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Compiled on %s\n", OS_NAME) >= 0);
  WRITE (f, output_line);
#endif
#endif
#if ! defined ENABLE_WIN32
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Configured on %s with options \"%s\"\n", CONFIGURE_DATE, CONFIGURE_OPTIONS) >= 0);
  WRITE (f, output_line);
#if defined ENABLE_GRAPHICS && defined A68_LIBPLOT_VERSION
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "GNU libplot %s\n", A68_LIBPLOT_VERSION) >= 0);
  WRITE (f, output_line);
#endif
#if defined ENABLE_NUMERICAL && defined A68_GSL_VERSION
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "GNU Scientific Library %s\n", A68_GSL_VERSION) >= 0);
  WRITE (f, output_line);
#endif
#if defined ENABLE_POSTGRESQL && defined A68_PG_VERSION
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "PostgreSQL libpq %s\n", A68_PG_VERSION) >= 0);
  WRITE (f, output_line);
#endif
#endif
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Alignment %d bytes\n", A68_ALIGNMENT) >= 0);
  WRITE (f, output_line);
  default_mem_sizes ();
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Default frame stack size: %d kB\n", frame_stack_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Default expression stack size: %d kB\n", expr_stack_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Default heap size: %d kB\n", heap_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Default handle pool size: %d kB\n", handle_pool_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Default stack overhead: %d kB\n", storage_overhead / KILOBYTE) >= 0);
  WRITE (f, output_line);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Effective system stack size: %d kB\n", stack_size / KILOBYTE) >= 0);
  WRITE (f, output_line);
}

/*!
\brief give brief help if someone types 'a68g -help'
\param f file number
**/

void online_help (FILE_T f)
{
#define P(s)\
  ASSERT (snprintf(output_line, (size_t) BUFFER_SIZE, "%s\n", (s)) >= 0);\
  WRITE (f, output_line);
  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  state_license (f);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Usage: %s [options | filename]", a68g_cmd_name) >= 0);
  WRITELN (f, output_line);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "For help: %s -apropos [keyword]", a68g_cmd_name) >= 0);
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
  if (term_width <= 1) {
    term_width = MAX_LINE_WIDTH;
  }
#else
  term_width = MAX_LINE_WIDTH;
#endif
/* Determine clock resolution. */
  {
    clock_t t0 = clock (), t1;
    do {
      t1 = clock ();
    } while (t1 == t0);
    clock_res = (t1 - t0) / (clock_t) CLOCKS_PER_SEC;
  }
/* Set the main thread id. */
#if defined ENABLE_PAR_CLAUSE
  main_thread_id = pthread_self ();
#endif
  get_fixed_heap_allowed = A68_TRUE;
  system_stack_offset = &stack_offset;
  if (!setjmp (program.exit_compilation)) {
    init_tty ();
/* Initialise option handling. */
    init_options ();
    program.source_scan = 1;
    default_options ();
    default_mem_sizes ();
/* Initialise core. */
    stack_segment = NULL;
    heap_segment = NULL;
    handle_segment = NULL;
    get_stack_size ();
/* Well, let's start. */
    program.top_refinement = NULL;
    program.files.initial_name = NULL;
    program.files.generic_name = NULL;
    program.files.source.name = NULL;
    program.files.listing.name = NULL;
    program.files.object.name = NULL;
    program.files.library.name = NULL;
    program.files.binary.name = NULL;
    program.files.script.name = NULL;
/* Options are processed here. */
    read_rc_options ();
    read_env_options ();
/* Posix copies arguments from the command line. */
    if (argc <= 1) {
      online_help (STDOUT_FILENO);
      a68g_exit (EXIT_FAILURE);
    }
    for (argcc = 1; argcc < argc; argcc++) {
      add_option_list (&(program.options.list), argv[argcc], NULL);
    }
    if (!set_options (program.options.list, A68_TRUE)) {
      a68g_exit (EXIT_FAILURE);
    }
    if (program.options.regression_test) {
      bufcpy (a68g_cmd_name, "a68g", BUFFER_SIZE);
    }
/* Attention for --version. */
    if (program.options.version) {
      state_version (STDOUT_FILENO);
    }
/* Running a script */
   if (program.options.run_script) {
     load_script ();
   }
/* We translate the program. */
    if (program.files.initial_name == NULL || strlen (program.files.initial_name) == 0) {
      SCAN_ERROR (!program.options.version, NULL, NULL, ERROR_NO_INPUT_FILE);
    } else {
      compiler_interpreter ();
    }
    a68g_exit (program.error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    return (EXIT_SUCCESS);
  } else {
    diagnostics_to_terminal (program.top_line, A68_ALL_DIAGNOSTICS);
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
  if (ext == NULL) {
    return (A68_FALSE);
  }
  nlen = (int) strlen (program.files.source.name); 
  xlen = (int) strlen (ext);
  if (nlen > xlen && strcmp (&(program.files.source.name[nlen - xlen]), ext) == 0) { 
    char *fn = (char *) get_heap_space ((size_t) (nlen + 1));
    bufcpy (fn, program.files.source.name, nlen);
    fn[nlen - xlen] = NULL_CHAR;
    program.files.generic_name = new_string (fn);
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
  program.files.source.fd = -1;
  for (k = 0; k < EXTENSIONS && program.files.source.fd == -1; k ++) {
    int len;
    char * fn;
    if (extensions[k] == NULL) {
      len = (int) strlen (program.files.initial_name) + 1;
      fn = (char *) get_heap_space ((size_t) len);
      bufcpy (fn, program.files.initial_name, len);
    } else {
      len = (int) strlen (program.files.initial_name) + (int) strlen (extensions[k]) + 1;
      fn = (char *) get_heap_space ((size_t) len);
      bufcpy (fn, program.files.initial_name, len);
      bufcat (fn, extensions[k], len);
    }
    program.files.source.fd = open (fn, O_RDONLY | O_BINARY);
    if (program.files.source.fd != -1) {
      int l;
      BOOL_T cont = A68_TRUE;
      program.files.source.name = new_string (fn);
      program.files.generic_name = new_string (fn);
      for (l = 0; l < EXTENSIONS && cont; l ++) {
        if (strip_extension (extensions[l])) {
          cont = A68_FALSE;
        }
      }
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
  program.top_node = NULL;
  program.top_line = NULL;
  set_up_tables ();
/* Various initialisations. */
  program.error_count = program.warning_count = 0;
  RESET_ERRNO;
}

/*!
\brief pretty print memory size
**/

char *pretty_size (int k) {
  if (k >= 10 * MEGABYTE) {
    ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, "%dM", k / MEGABYTE) >= 0);
  } else if (k >= 10 * KILOBYTE) {
    ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, "%dk", k / KILOBYTE) >= 0);
  } else {
    ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, "%d", k) >= 0);
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
  program.tree_listing_safe = A68_FALSE;
  program.cross_reference_safe = A68_FALSE;
  in_execution = A68_FALSE;
  new_nodes = 0;
  new_modes = 0;
  new_postulates = 0;
  new_node_infos = 0;
  new_genie_infos = 0;
  init_postulates ();
/* File set-up. */
  SCAN_ERROR (program.files.initial_name == NULL, NULL, NULL, ERROR_NO_INPUT_FILE);
  program.files.binary.opened = A68_FALSE;
  program.files.binary.writemood = A68_TRUE;
  program.files.library.opened = A68_FALSE;
  program.files.library.writemood = A68_TRUE;
  program.files.listing.opened = A68_FALSE;
  program.files.listing.writemood = A68_TRUE;
  program.files.object.opened = A68_FALSE;
  program.files.object.writemood = A68_TRUE;
  program.files.script.opened = A68_FALSE;
  program.files.script.writemood = A68_FALSE;
  program.files.source.opened = A68_FALSE;
  program.files.source.writemood = A68_FALSE;
/*
Open the source file. Open it for binary reading for systems that require so (Win32).
Accept various silent extensions.
*/
  errno = 0;
  program.files.source.name = NULL;
  program.files.generic_name = NULL;
  open_with_extensions ();
  if (program.files.source.fd == -1) {
    scan_error (NULL, NULL, ERROR_SOURCE_FILE_OPEN);
  }
  ABEND (program.files.source.name == NULL, "no source file name", NULL);
  ABEND (program.files.generic_name == NULL, "no generic file name", NULL);
/* Isolate the path name. */
  program.files.path = new_string (program.files.generic_name);
  path_set = A68_FALSE;
  for (k = (int) strlen (program.files.path); k >= 0 && path_set == A68_FALSE; k--) {
#if defined ENABLE_WIN32
    char delim = '\\';
#else
    char delim = '/';
#endif
    if (program.files.path[k] == delim) {
      program.files.path[k + 1] = NULL_CHAR;
      path_set = A68_TRUE;
    }
  }
  if (path_set == A68_FALSE) {
    program.files.path[0] = NULL_CHAR;
  }
/* Object file. */
  len = 1 + (int) strlen (program.files.generic_name) + (int) strlen (OBJECT_EXTENSION);
  program.files.object.name = (char *) get_heap_space ((size_t) len);
  bufcpy (program.files.object.name, program.files.generic_name, len);
  bufcat (program.files.object.name, OBJECT_EXTENSION, len);
/* Binary */
  len = 1 + (int) strlen (program.files.generic_name) + (int) strlen (LIBRARY_EXTENSION);
  program.files.binary.name = (char *) get_heap_space ((size_t) len);
  bufcpy (program.files.binary.name, program.files.generic_name, len);
  bufcat (program.files.binary.name, BINARY_EXTENSION, len);
/* Library file. */
  len = 1 + (int) strlen (program.files.generic_name) + (int) strlen (LIBRARY_EXTENSION);
  program.files.library.name = (char *) get_heap_space ((size_t) len);
  bufcpy (program.files.library.name, program.files.generic_name, len);
  bufcat (program.files.library.name, LIBRARY_EXTENSION, len);
/* Listing file. */
  len = 1 + (int) strlen (program.files.generic_name) + (int) strlen (LISTING_EXTENSION);
  program.files.listing.name = (char *) get_heap_space ((size_t) len);
  bufcpy (program.files.listing.name, program.files.generic_name, len);
  bufcat (program.files.listing.name, LISTING_EXTENSION, len);
/* Script file */
  len = 1 + (int) strlen (program.files.generic_name) + (int) strlen (SCRIPT_EXTENSION);
  program.files.script.name = (char *) get_heap_space ((size_t) len);
  bufcpy (program.files.script.name, program.files.generic_name, len);
  bufcat (program.files.script.name, SCRIPT_EXTENSION, len);
/* Tokeniser. */
  program.files.source.opened = A68_TRUE;
  announce_phase ("initialiser");
  init_before_tokeniser ();
  error_tag = (TAG_T *) new_tag ();
  if (program.error_count == 0) {
    int frame_stack_size_2 = frame_stack_size;
    int expr_stack_size_2 = expr_stack_size;
    int heap_size_2 = heap_size;
    int handle_pool_size_2 = handle_pool_size;
    BOOL_T ok;
    announce_phase ("tokeniser");
    ok = lexical_analyser ();
    if (!ok || errno != 0) {
      diagnostics_to_terminal (program.top_line, A68_ALL_DIAGNOSTICS);
      return;
    }
/* Maybe the program asks for more memory through a PRAGMAT. We restart. */
    if (frame_stack_size_2 != frame_stack_size || expr_stack_size_2 != expr_stack_size || heap_size_2 != heap_size || handle_pool_size_2 != handle_pool_size) {
      discard_heap ();
      init_before_tokeniser ();
      program.source_scan++;
      ok = lexical_analyser ();
      verbosity ();
    }
    if (!ok || errno != 0) {
      diagnostics_to_terminal (program.top_line, A68_ALL_DIAGNOSTICS);
      return;
    }
    ASSERT (close (program.files.source.fd) == 0);
    program.files.source.opened = A68_FALSE;
    prune_echoes (program.options.list);
    program.tree_listing_safe = A68_TRUE;
    num = 0;
    renumber_nodes (program.top_node, &num);
  }
/* Final initialisations. */
  if (program.error_count == 0) {
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
  if (program.error_count == 0) {
    announce_phase ("preprocessor");
    get_refinements ();
    if (program.error_count == 0) {
      put_refinements ();
    }
    num = 0;
    renumber_nodes (program.top_node, &num);
    verbosity ();
  }
/* Top-down parser. */
  if (program.error_count == 0) {
    announce_phase ("parser phase 1");
    check_parenthesis (program.top_node);
    if (program.error_count == 0) {
      if (program.options.brackets) {
        substitute_brackets (program.top_node);
      }
      symbol_table_count = 0;
      stand_env = new_symbol_table (NULL);
      stand_env->level = 0;
      top_down_parser (program.top_node);
    }
    num = 0;
    renumber_nodes (program.top_node, &num);
    verbosity ();
  }
/* Standard environment builder. */
  if (program.error_count == 0) {
    announce_phase ("standard environ builder");
    SYMBOL_TABLE (program.top_node) = new_symbol_table (stand_env);
    make_standard_environ ();
    verbosity ();
  }
/* Bottom-up parser. */
  if (program.error_count == 0) {
    announce_phase ("parser phase 2");
    preliminary_symbol_table_setup (program.top_node);
    bottom_up_parser (program.top_node);
    num = 0;
    renumber_nodes (program.top_node, &num);
    verbosity ();
  }
  if (program.error_count == 0) {
    announce_phase ("parser phase 3");
    bottom_up_error_check (program.top_node);
    victal_checker (program.top_node);
    if (program.error_count == 0) {
      finalise_symbol_table_setup (program.top_node, 2);
      SYMBOL_TABLE (program.top_node)->nest = symbol_table_count = 3;
      reset_symbol_table_nest_count (program.top_node);
      fill_symbol_table_outer (program.top_node, SYMBOL_TABLE (program.top_node));
#if defined ENABLE_PAR_CLAUSE
      set_par_level (program.top_node, 0);
#endif
      set_nest (program.top_node, NULL);
      set_proc_level (program.top_node, 1);
    }
    num = 0;
    renumber_nodes (program.top_node, &num);
    verbosity ();
  }
/* Mode table builder. */
  if (program.error_count == 0) {
    announce_phase ("mode table builder");
    set_up_mode_table (program.top_node);
    verbosity ();
  }
  program.cross_reference_safe = /* (BOOL_T) (program.error_count == 0) */ A68_TRUE;
/* Symbol table builder. */
  if (program.error_count == 0) {
    announce_phase ("symbol table builder");
    collect_taxes (program.top_node);
    verbosity ();
  }
/* Post parser. */
  if (program.error_count == 0) {
    announce_phase ("parser phase 4");
    rearrange_goto_less_jumps (program.top_node);
    num = 0;
    renumber_nodes (program.top_node, &num);
    verbosity ();
  }
/* Mode checker. */
  if (program.error_count == 0) {
    announce_phase ("mode checker");
    mode_checker (program.top_node);
    maintain_mode_table (program.top_node);
    verbosity ();
  }
/* Coercion inserter. */
  if (program.error_count == 0) {
    announce_phase ("coercion enforcer");
    coercion_inserter (program.top_node);
    widen_denotation (program.top_node);
    protect_from_sweep (program.top_node);
    reset_max_simplout_size ();
    get_max_simplout_size (program.top_node);
    reset_moid_list ();
    get_moid_list (&top_moid_list, program.top_node);
    set_moid_sizes (top_moid_list);
    assign_offsets_table (stand_env);
    assign_offsets (program.top_node);
    assign_offsets_packs (top_moid_list);
    num = 0;
    renumber_nodes (program.top_node, &num);
    verbosity ();
  }
/* Application checker. */
  if (program.error_count == 0) {
    announce_phase ("application checker");
    mark_moids (program.top_node);
    mark_auxilliary (program.top_node);
    jumps_from_procs (program.top_node);
    warn_for_unused_tags (program.top_node);
    warn_tags_threads (program.top_node);
    verbosity ();
  }
/* Scope checker. */
  if (program.error_count == 0) {
    announce_phase ("static scope checker");
    tie_label_to_serial (program.top_node);
    tie_label_to_unit (program.top_node);
    bind_routine_tags_to_tree (program.top_node);
    bind_format_tags_to_tree (program.top_node);
    scope_checker (program.top_node);
    verbosity ();
  }
/* Portability checker. */
  if (program.error_count == 0) {
    announce_phase ("portability checker");
    portcheck (program.top_node);
    verbosity ();
  }
/* Finalise syntax tree. */
  if (program.error_count == 0) {
    num = 0;
    renumber_nodes (program.top_node, &num);
    SYMBOL_TABLE (program.top_node)->nest = symbol_table_count = 3;
    reset_symbol_table_nest_count (program.top_node);
    verbosity ();
  }
/* Compiler. */
  if (program.error_count == 0 && program.options.optimise) {
    announce_phase ("optimiser (code generator)");
    num = 0;
    renumber_nodes (program.top_node, &num);
    node_register = (NODE_T **) get_heap_space ((size_t) num * sizeof (NODE_T));
    ABEND (node_register == NULL, "compiler cannot register nodes", NULL);
    register_nodes (program.top_node); 
    program.files.object.fd = open (program.files.object.name, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
    ABEND (program.files.object.fd == -1, "cannot open object file", NULL);
    program.files.object.opened = A68_TRUE;
    compiler (program.files.object.fd);
    ASSERT (close (program.files.object.fd) == 0);
    program.files.object.opened = A68_FALSE;
    emitted = A68_TRUE;
  }
#if defined ENABLE_COMPILER
/* Only compile C if the A68 compiler found no errors (constant folder for instance) */
  if (program.error_count == 0 && program.options.optimise && !program.options.run_script) {
    int ret;
    char cmd[BUFFER_SIZE];
    if (program.options.rerun == A68_FALSE) {
      announce_phase ("optimiser (code compiler)");
/* Build shared library using gcc. */
      ABEND (GCC_VERSION == 0, "compiler requires gcc", "use option --nocompile"); 
#ifdef ENABLE_PAR_CLAUSE
      ASSERT (snprintf (cmd, BUFFER_SIZE, "gcc -DENABLE_PAR_CLAUSE -c %s -o \"%s.o\" \"%s\"", GCC_OPTIONS, program.files.generic_name, program.files.object.name) >= 0);
#else
      ASSERT (snprintf (cmd, BUFFER_SIZE, "gcc -c %s -o \"%s\" \"%s\"", GCC_OPTIONS, program.files.binary.name, program.files.object.name) >= 0);
#endif
      ret = system (cmd);
      ABEND (ret != 0, "gcc cannot compile", cmd);
#if defined ENABLE_LINUX
      ASSERT (snprintf (cmd, BUFFER_SIZE, "gcc -shared -o \"%s\" \"%s\"", program.files.library.name, program.files.binary.name) >= 0);
      ret = system (cmd);
      ABEND (ret != 0, "gcc cannot link", cmd);
#elif defined ENABLE_MACOSX
      ASSERT (snprintf (cmd, BUFFER_SIZE, "libtool %s -o %s %s", LIBTOOL_OPTIONS, program.files.library.name, program.files.binary.name) >= 0);
      ret = system (cmd);
      ABEND (ret != 0, "libtool cannot link", cmd);
#else
      ABEND (ret != 0, "cannot link", cmd);
#endif
      ret = remove (program.files.binary.name);
      ABEND (ret != 0, "cannot remove", cmd);
    }
    verbosity ();
  }
#endif
/* Interpreter. */
  diagnostics_to_terminal (program.top_line, A68_ALL_DIAGNOSTICS);
  if (program.error_count == 0 && program.options.compile == A68_FALSE && (program.options.check_only ? program.options.run : A68_TRUE)) {
#ifdef ENABLE_COMPILER
  void * compile_lib;
#endif
    if (program.options.run_script) {
      rewrite_script_source ();
    }
    if (program.options.debug) {
      state_license (STDOUT_FILENO);
    }
#ifdef ENABLE_COMPILER
    if (program.options.optimise) {
      char libname[BUFFER_SIZE];
      void * a68g_lib;
      struct stat srcstat, objstat;
      int ret;
      announce_phase ("dynamic linker");
      ASSERT (snprintf (libname, BUFFER_SIZE, "./%s", program.files.library.name) >= 0);
/* Check whether we are doing something rash. */
      ret = stat (program.files.source.name, &srcstat);
      ABEND (ret != 0, "cannot stat", program.files.source.name);
      ret = stat (libname, &objstat);
      ABEND (ret != 0, "cannot stat", libname);
      if (program.options.rerun) {
        ABEND (srcstat.st_mtime > objstat.st_mtime, "source file is younger than library", "do not specify RERUN");
      }
/* First load a68g itself so compiler code can resolve a68g symbols. */
      a68g_lib = dlopen (NULL, RTLD_NOW | RTLD_GLOBAL);
      ABEND (a68g_lib == NULL, "compiler cannot resolve a68g symbols", dlerror ());
/* Then load compiler code. */
      compile_lib = dlopen (libname, RTLD_NOW | RTLD_GLOBAL);
      ABEND (compile_lib == NULL, "compiler cannot resolve symbols", dlerror ());
    } else {
      compile_lib = NULL;
    }
    announce_phase ("genie");
    genie (compile_lib);
/* Unload compiler library. */
    if (program.options.optimise) {
      int ret = dlclose (compile_lib);
      ABEND (ret != 0, "cannot close shared library", dlerror ());
    }
#else
    genie (NULL);
#endif
/* Free heap allocated by genie. */
    free_genie_heap (program.top_node);
/* Normal end of program. */
    diagnostics_to_terminal (program.top_line, A68_RUNTIME_ERROR);
    if (program.options.debug || program.options.trace) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\nGenie finished in %.2f seconds\n", seconds () - cputime_0) >= 0);
      WRITE (STDOUT_FILENO, output_line);
    }
    verbosity ();
  }
/* Setting up listing file. */
  if (program.options.moid_listing || program.options.tree_listing || program.options.source_listing || program.options.object_listing || program.options.statistics_listing) {
    program.files.listing.fd = open (program.files.listing.name, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
    ABEND (program.files.listing.fd == -1, "cannot open listing file", NULL);
    program.files.listing.opened = A68_TRUE;
  } else {
    program.files.listing.opened = A68_FALSE;
  }
/* Write listing. */
  if (program.files.listing.opened) {
    write_listing_header ();
    write_source_listing ();
    write_tree_listing ();
    if (program.error_count == 0 && program.options.optimise) {
      write_object_listing ();
    }
    write_listing ();
    ASSERT (close (program.files.listing.fd) == 0);
    program.files.listing.opened = A68_FALSE;
    verbosity ();
  }
/* Cleaning up the intermediate files */
#if defined ENABLE_COMPILER
  if (program.options.run_script && !program.options.keep) {
    if (emitted) {
      ABEND (remove (program.files.object.name) != 0, "cannot remove", program.files.object.name);
    }
    ABEND (remove (program.files.source.name) != 0, "cannot remove", program.files.source.name);
    ABEND (remove (program.files.library.name) != 0, "cannot remove", program.files.library.name);
  } else if (program.options.compile && !program.options.keep) {
    build_script ();
    if (emitted) {
      ABEND (remove (program.files.object.name) != 0, "cannot remove", program.files.object.name);
    }
    ABEND (remove (program.files.library.name) != 0, "cannot remove", program.files.library.name);
  } else if (program.options.optimise && !program.options.keep) {
    if (emitted) {
      ABEND (remove (program.files.object.name) != 0, "cannot remove", program.files.object.name);
    }
  } else if (program.options.rerun && !program.options.keep) {
    if (emitted) {
      ABEND (remove (program.files.object.name) != 0, "cannot remove", program.files.object.name);
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
  char name[BUFFER_SIZE];
  bufcpy (name, ".", BUFFER_SIZE);
  bufcat (name, a68g_cmd_name, BUFFER_SIZE);
  bufcat (name, ".x", BUFFER_SIZE);
  /* (void) (remove (name)); */
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
  if (program.options.verbose) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s: %s", a68g_cmd_name, t) >= 0);
    io_close_tty_line ();
    WRITE (STDOUT_FILENO, output_line);
  }
}

#if defined ENABLE_COMPILER

/*!
\brief build shell script from program
**/

static void build_script (void)
{
  int ret;
  FILE_T script, source;
  SOURCE_LINE_T *sl;
  char cmd[BUFFER_SIZE];
#if ! defined ENABLE_COMPILER
  return;
#endif
  announce_phase ("script builder");
/* Flatten the source file */
  ASSERT (snprintf (cmd, BUFFER_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, program.files.source.name) >= 0);
  source = open (cmd, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (source == -1, "cannot flatten source file", cmd);
  for (sl = program.top_line; sl != NULL; FORWARD (sl)) {
    if (strlen (sl->string) == 0 || (sl->string)[strlen (sl->string) - 1] != NEWLINE_CHAR) {
      ASSERT (snprintf (cmd, BUFFER_SIZE, "%s\n%d\n%s\n", sl->filename, NUMBER (sl), sl->string) >= 0);
    } else {
      ASSERT (snprintf (cmd, BUFFER_SIZE, "%s\n%d\n%s", sl->filename, NUMBER (sl), sl->string) >= 0);
    }
    WRITE (source, cmd);
  }
  ASSERT (close (source) == 0);
/* Compress source and library */
  ASSERT (snprintf (cmd, BUFFER_SIZE, "cp %s %s.%s", program.files.library.name, HIDDEN_TEMP_FILE_NAME, program.files.library.name) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, "cannot copy", cmd);
  ASSERT (snprintf (cmd, BUFFER_SIZE, "tar czf %s.%s.tgz %s.%s %s.%s", HIDDEN_TEMP_FILE_NAME, program.files.generic_name, HIDDEN_TEMP_FILE_NAME, program.files.source.name, HIDDEN_TEMP_FILE_NAME, program.files.library.name) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, "cannot compress", cmd);
/* Compose script */
  ASSERT (snprintf (cmd, BUFFER_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, program.files.script.name) >= 0);
  script = open (cmd, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (script == -1, "cannot compose script file", cmd);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "#! %s/a68g --run-script\n", INSTALL_BIN) >= 0);
  WRITE (script, output_line);
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s\n--verify \"%s\"\n", program.files.generic_name, VERIFICATION) >= 0);
  WRITE (script, output_line);
  ASSERT (close (script) == 0);
  ASSERT (snprintf (cmd, BUFFER_SIZE, "cat %s.%s %s.%s.tgz > %s", HIDDEN_TEMP_FILE_NAME, program.files.script.name, HIDDEN_TEMP_FILE_NAME, program.files.generic_name, program.files.script.name) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, "cannot compose script file", cmd);
  ASSERT (snprintf (cmd, BUFFER_SIZE, "%s", program.files.script.name) >= 0);
  ret = chmod (cmd, (__mode_t) (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH)); /* -rwx-r--r-- */
  ABEND (ret != 0, "cannot compose script file", cmd);
  ABEND (ret != 0, "cannot remove", cmd);
/* Clean up */
  ASSERT (snprintf (cmd, BUFFER_SIZE, "%s.%s.tgz", HIDDEN_TEMP_FILE_NAME, program.files.generic_name) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, "cannot remove", cmd);
  ASSERT (snprintf (cmd, BUFFER_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, program.files.source.name) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, "cannot remove", cmd);
  ASSERT (snprintf (cmd, BUFFER_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, program.files.library.name) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, "cannot remove", cmd);
  ASSERT (snprintf (cmd, BUFFER_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, program.files.script.name) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, "cannot remove", cmd);
}

#endif

#if defined ENABLE_COMPILER

/*!
\brief load program from shell script 
**/

static void load_script (void)
{
  int k;
  FILE_T script;
  char cmd[BUFFER_SIZE], ch;
#if ! defined ENABLE_COMPILER
  return;
#endif
  announce_phase ("script loader");
/* Decompress the archive */
  ASSERT (snprintf (cmd, BUFFER_SIZE, "sed '1,3d' < %s | tar xzf -", program.files.initial_name) >= 0);
  ABEND (system (cmd) != 0, "cannot decompress", cmd);
/* Reread the header */
  script = open (program.files.initial_name, O_RDONLY);
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
  ASSERT (snprintf (cmd, BUFFER_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, input_line) >= 0);
  program.files.initial_name = new_string (cmd);
/* Read options */
  input_line[0] = NULL_CHAR;
  k = 0;
  ASSERT (io_read (script, &ch, 1) == 1);
  while (ch != NEWLINE_CHAR) {
    input_line[k++] = ch;
    ASSERT (io_read (script, &ch, 1) == 1);
  }
  isolate_options (input_line, NULL);
  (void) set_options (program.options.list, A68_FALSE);
  ASSERT (close (script) == 0);
}

#endif

#if defined ENABLE_COMPILER

/*!
\brief rewrite source for shell script 
**/

static void rewrite_script_source (void)
{
  SOURCE_LINE_T * ref_l = NULL;
  FILE_T source;
/* Rebuild the source file */
  ASSERT (remove (program.files.source.name) == 0);
  source = open (program.files.source.name, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (source == -1, "cannot rewrite source file", program.files.source.name);
  for (ref_l = program.top_line; ref_l != NULL; FORWARD (ref_l)) {
    WRITE (source, ref_l->string);
    if (strlen (ref_l->string) == 0 || (ref_l->string)[strlen (ref_l->string - 1)] != NEWLINE_CHAR) {
      WRITE (source, "\n");
    }
  }
/* Wrap it up */
  ASSERT (close (source) == 0);
}

#endif
