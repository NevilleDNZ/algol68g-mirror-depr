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
#include "algol68g.h"
#include "interpreter.h"
#include "mp.h"
#include "gsl.h"

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
/* Attention for --mips */
   if (program.options.mips) {
     bogus_mips ();
   }
/* Running a script */
   if (program.options.run_script) {
     load_script ();
   }
/* We translate the program. */
    if (program.files.initial_name == NULL || strlen (program.files.initial_name) == 0) {
      SCAN_ERROR (!program.options.version, NULL, NULL, ERROR_NO_SOURCE_FILE);
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
  SCAN_ERROR (program.files.initial_name == NULL, NULL, NULL, ERROR_NO_SOURCE_FILE);
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
    /* ASSERT (block_heap_compacter == 0); fails in case we jumped out of a RTS routine! */
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
BOOL_T no_warnings = A68_TRUE;

/*!
\brief set default values for options
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
  program.options.mips = A68_FALSE;
  program.options.moid_listing = A68_FALSE;
  program.options.nodemask = (STATUS_MASK) (ASSERT_MASK | SOURCE_MASK);
  program.options.opt_level = 0;
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
**/

void init_options (void)
{
  options = (OPTIONS_T *) malloc ((size_t) ALIGNED_SIZE_OF (OPTIONS_T));
  program.options.list = NULL;
}

/*!
\brief test equality of p and q, upper case letters in q are mandatory
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
\param i option chain
\param cmd_line whether command line argument
\return whether processing was successful
**/

BOOL_T set_options (OPTION_LIST_T * i, BOOL_T cmd_line)
{
  BOOL_T go_on = A68_TRUE, name_set = A68_FALSE, skip = A68_FALSE;
  OPTION_LIST_T *j = i;
  RESET_ERRNO;
  while (i != NULL && go_on) {
/* Once SCRIPT is processed we skip options on the command line. */
    if (cmd_line && skip) {
      FORWARD (i);
    } else {
      SOURCE_LINE_T *start_l = i->line;
      char *start_c = i->str;
      int n = (int) strlen (i->str);
/* Allow for spaces ending in # to have A68 comment syntax with '#!'. */
      while (n > 0 && (IS_SPACE((i->str)[n - 1]) || (i->str)[n - 1] == '#')) {
        (i->str)[--n] = NULL_CHAR;
      }
      if (!(i->processed)) {
/* Accept UNIX '-option [=] value' */
        BOOL_T minus_sign = (BOOL_T) ((i->str)[0] == '-');
        char *p = strip_sign (i->str);
        if (!minus_sign && eq (p, "#")) {
          ;
        } else if (!minus_sign && cmd_line) {
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
/* MIPS gives a bogus mips rating. */
        else if (eq (p, "MIPS")) {
          program.options.mips = A68_TRUE;
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
/* SCRIPT takes next argument as filename.
   Further options on the command line are not processed, but stored. */
        else if (eq (p, "Script") && cmd_line) {
          FORWARD (i);
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
          skip = A68_TRUE;
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
          program.options.opt_level = 1;
          program.options.run_script = A68_FALSE;
        } else if (eq (p, "NOCompile")) {
          program.options.compile = A68_FALSE;
          program.options.optimise = A68_FALSE;
          program.options.opt_level = 0;
          program.options.run_script = A68_FALSE;
        } else if (eq (p, "NO-Compile")) {
          program.options.compile = A68_FALSE;
          program.options.optimise = A68_FALSE;
          program.options.opt_level = 0;
          program.options.run_script = A68_FALSE;
        }
/* OPTIMISE and NOOPTIMISE switch on/off optimisation. */
        else if (eq (p, "Optimise")) {
          program.options.optimise = A68_TRUE;
          program.options.opt_level = 1;
        } else if (eq (p, "NOOptimise")) {
          program.options.optimise = A68_FALSE;
          program.options.opt_level = 0;
        } else if (eq (p, "NO-Optimise")) {
          program.options.optimise = A68_FALSE;
          program.options.opt_level = 0;
        } else if (eq (p, "Optimize")) {
          program.options.optimise = A68_TRUE;
          program.options.opt_level = 1;
        } else if (eq (p, "NOOptimize")) {
          program.options.optimise = A68_FALSE;
          program.options.opt_level = 0;
        } else if (eq (p, "NO-Optimize")) {
          program.options.optimise = A68_FALSE;
          program.options.opt_level = 0;
        }
/* Optimisation level 2 */
        else if (eq (p, "O2")) {
          program.options.optimise = A68_TRUE;
          program.options.opt_level = 2;
        }
/* RUN-SCRIPT runs a comiled .sh script. */
        else if (eq (p, "RUN-SCRIPT")) {
          FORWARD (i);
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
          skip = A68_TRUE;
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

/* Routines for making a listing file */

#define SHOW_EQ A68_FALSE

char *bar[BUFFER_SIZE];

/*!
\brief brief_mode_string
\param p moid to print
\return pointer to string
**/

static char *brief_mode_string (MOID_T * p)
{
  static char q[BUFFER_SIZE];
  ASSERT (snprintf (q, BUFFER_SIZE, "MODE_%d", NUMBER (p)) >= 0);
  return (new_string (q));
}

/*!
\brief brief_mode_flat
\param f file number
\param z moid to print
**/

void brief_mode_flat (FILE_T f, MOID_T * z)
{
  if (WHETHER (z, STANDARD) || WHETHER (z, INDICANT)) {
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
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", SYMBOL (NODE (z))) >= 0);
    WRITE (f, output_line);
  } else {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", brief_mode_string (z)) >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief brief_fields_flat
\param f file number
\param pack pack to print
**/

static void brief_fields_flat (FILE_T f, PACK_T * pack)
{
  if (pack != NULL) {
    brief_mode_flat (f, MOID (pack));
    if (NEXT (pack) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", ") >= 0);
      WRITE (f, output_line);
      brief_fields_flat (f, NEXT (pack));
    }
  }
}

/*!
\brief brief_moid_flat
\param f file number
\param z moid to print
**/

void brief_moid_flat (FILE_T f, MOID_T * z)
{
  if (z != NULL) {
    if (WHETHER (z, STANDARD) || WHETHER (z, INDICANT)) {
      brief_mode_flat (f, z);
    } else if (z == MODE (COLLITEM)) {
      WRITE (f, "\"COLLITEM\"");
    } else if (WHETHER (z, REF_SYMBOL)) {
      WRITE (f, "REF ");
      brief_mode_flat (f, SUB (z));
    } else if (WHETHER (z, FLEX_SYMBOL)) {
      WRITE (f, "FLEX ");
      brief_mode_flat (f, SUB (z));
    } else if (WHETHER (z, ROW_SYMBOL)) {
      int i = DIM (z);
      WRITE (f, "[");
      while (--i) {
        WRITE (f, ", ");
      }
      WRITE (f, "] ");
      brief_mode_flat (f, SUB (z));
    } else if (WHETHER (z, STRUCT_SYMBOL)) {
      WRITE (f, "STRUCT (");
      brief_fields_flat (f, PACK (z));
      WRITE (f, ")");
    } else if (WHETHER (z, UNION_SYMBOL)) {
      WRITE (f, "UNION (");
      brief_fields_flat (f, PACK (z));
      WRITE (f, ")");
    } else if (WHETHER (z, PROC_SYMBOL)) {
      WRITE (f, "PROC ");
      if (PACK (z) != NULL) {
        WRITE (f, "(");
        brief_fields_flat (f, PACK (z));
        WRITE (f, ") ");
      }
      brief_mode_flat (f, SUB (z));
    } else if (WHETHER (z, IN_TYPE_MODE)) {
      WRITE (f, "\"SIMPLIN\"");
    } else if (WHETHER (z, OUT_TYPE_MODE)) {
      WRITE (f, "\"SIMPLOUT\"");
    } else if (WHETHER (z, ROWS_SYMBOL)) {
      WRITE (f, "\"ROWS\"");
    } else if (WHETHER (z, SERIES_MODE)) {
      WRITE (f, "\"SERIES\" (");
      brief_fields_flat (f, PACK (z));
      WRITE (f, ")");
    } else if (WHETHER (z, STOWED_MODE)) {
      WRITE (f, "\"STOWED\" (");
      brief_fields_flat (f, PACK (z));
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
  if (m != NULL) {
    brief_moid_flat (f, m);
    if (m->equivalent_mode != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", equi: %s", brief_mode_string (EQUIVALENT (m))) >= 0);
      WRITE (f, output_line);
    }
    if (SLICE (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", slice: %s", brief_mode_string (SLICE (m))) >= 0);
      WRITE (f, output_line);
    }
    if (ROWED (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", rowed: %s", brief_mode_string (ROWED (m))) >= 0);
      WRITE (f, output_line);
    }
    if (DEFLEXED (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", deflex: %s", brief_mode_string (DEFLEXED (m))) >= 0);
      WRITE (f, output_line);
    }
    if (MULTIPLE (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", multiple: %s", brief_mode_string (MULTIPLE (m))) >= 0);
      WRITE (f, output_line);
    }
    if (NAME (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", name: %s", brief_mode_string (NAME (m))) >= 0);
      WRITE (f, output_line);
    }
    if (TRIM (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", trim: %s", brief_mode_string (TRIM (m))) >= 0);
      WRITE (f, output_line);
    }
    if (m->use == A68_FALSE) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", unused") >= 0);
      WRITE (f, output_line);
    }
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", size: %d", MOID_SIZE (m)) >= 0);
    WRITE (f, output_line);
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
  for (; s != NULL; FORWARD (s)) {
    NODE_T *where_tag = NODE (s);
    if ((where_tag != NULL) && ((STATUS_TEST (where_tag, CROSS_REFERENCE_MASK)) || TAG_TABLE (s) == stand_env)) {
      WRITE (f, "\n     ");
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "tag %d ", NUMBER (s)) >= 0);
      WRITE (f, output_line);
      switch (a) {
      case IDENTIFIER:
        {
          brief_moid_flat (f, MOID (s));
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, " %s", SYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          break;
        }
      case INDICANT:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Indicant %s ", SYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      case PRIO_SYMBOL:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Priority %s %d", SYMBOL (NODE (s)), PRIO (s)) >= 0);
          WRITE (f, output_line);
          break;
        }
      case OP_SYMBOL:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Operator %s ", SYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      case LABEL:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Label %s", SYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          break;
        }
      case ANONYMOUS:
        {
          switch (PRIO (s)) {
          case ROUTINE_TEXT:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Routine text ") >= 0);
              break;
            }
          case FORMAT_TEXT:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Format text ") >= 0);
              break;
            }
          case FORMAT_IDENTIFIER:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Format item ") >= 0);
              break;
            }
          case COLLATERAL_CLAUSE:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Display ") >= 0);
              break;
            }
          case GENERATOR:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Generator ") >= 0);
              break;
            }
          case PROTECT_FROM_SWEEP:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Sweep protect ") >= 0);
              break;
            }
          }
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      default:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Internal %d ", a) >= 0);
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      }
      if (NODE (s) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", node %d", NUMBER (NODE (s))) >= 0);
        WRITE (f, output_line);
      }
      if (where_tag != NULL && where_tag->info != NULL && where_tag->info->line != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", line %d", LINE_NUMBER (where_tag)) >= 0);
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

static void xref_decs (FILE_T f, SYMBOL_TABLE_T * t)
{
  if (t->indicants != NULL) {
    xref_tags (f, t->indicants, INDICANT);
  }
  if (t->operators != NULL) {
    xref_tags (f, t->operators, OP_SYMBOL);
  }
  if (PRIO (t) != NULL) {
    xref_tags (f, PRIO (t), PRIO_SYMBOL);
  }
  if (t->identifiers != NULL) {
    xref_tags (f, t->identifiers, IDENTIFIER);
  }
  if (t->labels != NULL) {
    xref_tags (f, t->labels, LABEL);
  }
  if (t->anonymous != NULL) {
    xref_tags (f, t->anonymous, ANONYMOUS);
  }
}

/*!
\brief xref1_moid
\param f file number
\param p moid to xref
**/

static void xref1_moid (FILE_T f, MOID_T * p)
{
  if (/* USE (p) && */ (EQUIVALENT (p) == NULL || SHOW_EQ)) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n     %s ", brief_mode_string (p)) >= 0);
    WRITE (f, output_line);
    print_mode_flat (f, p);
  }
}

/*!
\brief xref_moids
\param f file number
\param p moid chain to xref
**/

static void xref_moids (FILE_T f, MOID_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    xref1_moid (f, p);
  }
}

/*!
\brief moid_listing
\param f file number
\param m moid list to xref
**/

static void moid_listing (FILE_T f, MOID_LIST_T * m)
{
  for (; m != NULL; FORWARD (m)) {
    xref1_moid (f, MOID (m));
  }
}

/*!
\brief cross_reference
\param file number
\param p top node
\param l source line
**/

static void cross_reference (FILE_T f, NODE_T * p, SOURCE_LINE_T * l)
{
  if (p != NULL && program.cross_reference_safe) {
    for (; p != NULL; FORWARD (p)) {
      if (whether_new_lexical_level (p) && l == LINE (p)) {
        SYMBOL_TABLE_T *c = SYMBOL_TABLE (SUB (p));
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n[level %d", c->level) >= 0);
        WRITE (f, output_line);
        if (PREVIOUS (c) == stand_env) {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", in standard environ") >= 0);
        } else {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", in level %d", PREVIOUS (c)->level) >= 0);
        }
        WRITE (f, output_line);
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", %d increment]", c->ap_increment) >= 0);
        WRITE (f, output_line);
        if (c->moids != NULL) {
          xref_moids (f, c->moids);
        }
        if (c != NULL) {
          xref_decs (f, c);
        }
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
  for (; p != NULL && (*count) < 5; FORWARD (p)) {
    if (SUB (p) != NULL) {
      write_symbols (f, SUB (p), count);
    } else {
      if (*count > 0) {
        WRITE (f, " ");
      }
      (*count)++;
      if (*count == 5) {
        WRITE (f, "...");
      } else {
        ASSERT (snprintf(output_line, (size_t) BUFFER_SIZE, "%s", SYMBOL (p)) >= 0);
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

void tree_listing (FILE_T f, NODE_T * q, int x, SOURCE_LINE_T * l, int *ld)
{
  for (; q != NULL; FORWARD (q)) {
    NODE_T *p = q;
    int k, dist;
    if (((STATUS_TEST (p, TREE_MASK))) && l == LINE (p)) {
      if (*ld < 0) {
        *ld = x;
      }
/* Indent. */
      WRITE (f, "\n     ");
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%02u %06u p%02u ", (unsigned) x, (unsigned) NUMBER (p), (unsigned) INFO (p)->PROCEDURE_LEVEL) >= 0);
      WRITE (f, output_line);
      if (PREVIOUS (SYMBOL_TABLE (p)) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "l%02u(%02u) ", (unsigned) (SYMBOL_TABLE (p) != NULL ? LEX_LEVEL (p) : -1), (unsigned) LEVEL (PREVIOUS (SYMBOL_TABLE (p)))) >= 0);
      } else {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "l%02u    ", (unsigned) (SYMBOL_TABLE (p) != NULL ? LEX_LEVEL (p) : -1)) >= 0);
      }
      WRITE (f, output_line);
      for (k = 0; k < (x - *ld); k++) {
        WRITE (f, bar[k]);
      }
      if (MOID (p) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s ", moid_to_string (MOID (p), MOID_WIDTH, NULL)) >= 0);
        WRITE (f, output_line);
      }
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", non_terminal_string (edit_line, ATTRIBUTE (p))) >= 0);
      WRITE (f, output_line);
      if (SUB (p) == NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, " \"%s\"", SYMBOL (p)) >= 0);
        WRITE (f, output_line);
      }
      if (TAX (p) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", tag %06u", (unsigned) NUMBER (TAX (p))) >= 0);
        WRITE (f, output_line);
        if (MOID (TAX (p)) != NULL) {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", mode %06u", (unsigned) NUMBER (MOID (TAX (p)))) >= 0);
          WRITE (f, output_line);
        }
      }
      if (GENIE (p) != NULL && propagator_name (PROPAGATOR (p).unit) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", %s", propagator_name (PROPAGATOR (p).unit)) >= 0);
        WRITE (f, output_line);
      }
      if (GENIE (p) != NULL && GENIE (p)->compile_name != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", %s", GENIE (p)->compile_name) >= 0);
        WRITE (f, output_line);
      }
      if (GENIE (p) != NULL && GENIE (p)->compile_node > 0) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", %06d", GENIE (p)->compile_node) >= 0);
        WRITE (f, output_line);
      }
    }
    dist = x - (*ld);
    if (dist >= 0 && dist < BUFFER_SIZE) {
      bar[dist] = (NEXT (p) != NULL && l == LINE (NEXT (p)) ? "|" : " ");
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

static int leaves_to_print (NODE_T * p, SOURCE_LINE_T * l)
{
  int z = 0;
  for (; p != NULL && z == 0; FORWARD (p)) {
    if (l == LINE (p) && ((STATUS_TEST (p, TREE_MASK)))) {
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

void list_source_line (FILE_T f, SOURCE_LINE_T * line, BOOL_T tree)
{
  int k = (int) strlen (line->string) - 1;
  if (NUMBER (line) <= 0) {
/* Mask the prelude and postlude. */
    return;
  }
  if ((line->string)[k] == NEWLINE_CHAR) {
    (line->string)[k] = NULL_CHAR;
  }
/* Print source line. */
  write_source_line (f, line, NULL, A68_ALL_DIAGNOSTICS);
/* Cross reference for lexical levels starting at this line. */
  if (program.options.cross_reference) {
    cross_reference (f, program.top_node, line);
  }
/* Syntax tree listing connected with this line. */
  if (tree && program.options.tree_listing) {
    if (program.tree_listing_safe && leaves_to_print (program.top_node, line)) {
      int ld = -1, k2;
      WRITE (f, "\nSyntax tree");
      for (k2 = 0; k2 < BUFFER_SIZE; k2++) {
        bar[k2] = " ";
      }
      tree_listing (f, program.top_node, 1, line, &ld);
    }
  }
}

/*!
\brief source_listing
**/

void write_source_listing (void)
{
  SOURCE_LINE_T *line = program.top_line;
  FILE_T f = program.files.listing.fd;
  int listed = 0;
  WRITE (program.files.listing.fd, "\n");
  WRITE (program.files.listing.fd, "\nSource listing");
  WRITE (program.files.listing.fd, "\n------ -------");
  WRITE (program.files.listing.fd, "\n");
  if (program.files.listing.opened == 0) {
    diagnostic_node (A68_ERROR, NULL, ERROR_CANNOT_WRITE_LISTING, NULL);
    return;
  }
  for (; line != NULL; FORWARD (line)) {
    if (NUMBER (line) > 0 && line->list) {
      listed++;
    }
    list_source_line (f, line, A68_FALSE);
  }
/* Warn if there was no source at all. */
  if (listed == 0) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n     No lines to list") >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief write_source_listing
**/

void write_tree_listing (void)
{
  SOURCE_LINE_T *line = program.top_line;
  FILE_T f = program.files.listing.fd;
  int listed = 0;
  WRITE (program.files.listing.fd, "\n");
  WRITE (program.files.listing.fd, "\nSyntax tree listing");
  WRITE (program.files.listing.fd, "\n------ ---- -------");
  WRITE (program.files.listing.fd, "\n");
  if (program.files.listing.opened == 0) {
    diagnostic_node (A68_ERROR, NULL, ERROR_CANNOT_WRITE_LISTING, NULL);
    return;
  }
  for (; line != NULL; FORWARD (line)) {
    if (NUMBER (line) > 0 && line->list) {
      listed++;
    }
    list_source_line (f, line, A68_TRUE);
  }
/* Warn if there was no source at all. */
  if (listed == 0) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n     No lines to list") >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief write_object_listing
**/

void write_object_listing (void)
{
  if (program.options.object_listing) {
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nObject listing");
    WRITE (program.files.listing.fd, "\n------ -------");
    WRITE (program.files.listing.fd, "\n");
    compiler (program.files.listing.fd);
  }
}

/*!
\brief write_listing
**/

void write_listing (void)
{
  FILE_T f = program.files.listing.fd;
  if (program.options.moid_listing && top_moid_list != NULL) {
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nMode listing");
    WRITE (program.files.listing.fd, "\n---- -------");
    WRITE (program.files.listing.fd, "\n");
    moid_listing (f, top_moid_list);
  }
  if (program.options.standard_prelude_listing && stand_env != NULL) {
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nStandard prelude listing");
    WRITE (program.files.listing.fd, "\n-------- ------- -------");
    WRITE (program.files.listing.fd, "\n");
    xref_decs (f, stand_env);
  }
  if (program.top_refinement != NULL) {
    REFINEMENT_T *x = program.top_refinement;
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nRefinement listing");
    WRITE (program.files.listing.fd, "\n---------- -------");
    WRITE (program.files.listing.fd, "\n");
    while (x != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n  \"%s\"", x->name) >= 0);
      WRITE (f, output_line);
      if (x->line_defined != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", defined in line %d", NUMBER (x->line_defined)) >= 0);
        WRITE (f, output_line);
      }
      if (x->line_applied != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", applied in line %d", NUMBER (x->line_applied)) >= 0);
        WRITE (f, output_line);
      }
      switch (x->applications) {
      case 0:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", not applied") >= 0);
          WRITE (f, output_line);
          break;
        }
      case 1:
        {
          break;
        }
      default:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", applied more than once") >= 0);
          WRITE (f, output_line);
          break;
        }
      }
      FORWARD (x);
    }
  }
  if (program.options.list != NULL) {
    OPTION_LIST_T *i;
    int k = 1;
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nPragmat listing");
    WRITE (program.files.listing.fd, "\n------- -------");
    WRITE (program.files.listing.fd, "\n");
    for (i = program.options.list; i != NULL; FORWARD (i)) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n%d: %s", k++, i->str) >= 0);
      WRITE (f, output_line);
    }
  }
  WRITE (f, NEWLINE_STRING);
}

/*!
\brief write_listing_header
**/

void write_listing_header (void)
{
  FILE_T f = program.files.listing.fd;
  SOURCE_LINE_T * z;
  state_version (program.files.listing.fd);
  WRITE (program.files.listing.fd, "\nFile \"");
  WRITE (program.files.listing.fd, program.files.source.name);
  if (program.options.statistics_listing) {
    if (program.error_count + program.warning_count > 0) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\nDiagnostics: %d error(s), %d warning(s)", program.error_count, program.warning_count) >= 0);
      WRITE (f, output_line);
      for (z = program.top_line; z != NULL; FORWARD (z)) {
        if (z->diagnostics != NULL) {
          write_source_line (f, z, NULL, A68_TRUE);
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
  ABEND (signal (SIGINT, sigint_handler) == SIG_ERR, "cannot install SIGINT handler", NULL);
  if (!(STATUS_TEST (program.top_node, BREAKPOINT_INTERRUPT_MASK) || in_monitor)) {
    STATUS_SET (program.top_node, BREAKPOINT_INTERRUPT_MASK);
    genie_break (program.top_node);
  }
}

#if ! defined ENABLE_WIN32

/*!
\brief signal reading from disconnected terminal
\param i dummy
**/

static void sigttin_handler (int i)
{
  (void) i;
  ABEND (A68_TRUE, "background process attempts reading from disconnected terminal", NULL);
}

/*!
\brief signal broken pipe
\param i dummy
**/

static void sigpipe_handler (int i)
{
  (void) i;
  ABEND (A68_TRUE, "forked process has broken the pipe", NULL);
}

/*!
\brief signal alarm - time limit check
\param i dummy
**/

static void sigalrm_handler (int i)
{
  (void) i;
  if (in_execution && !in_monitor) {
    double _m_t = (double) program.options.time_limit;
    if (_m_t > 0 && (seconds () - cputime_0) > _m_t) {
      diagnostic_node (A68_RUNTIME_ERROR, (NODE_T *) last_unit, ERROR_TIME_LIMIT_EXCEEDED);
      exit_genie ((NODE_T *) last_unit, A68_RUNTIME_ERROR);
    }
  }
  (void) alarm (1);
}

#endif

/*!
\brief install_signal_handlers
**/

void install_signal_handlers (void)
{
  ABEND (signal (SIGINT, sigint_handler) == SIG_ERR, "cannot install SIGINT handler", NULL);
  ABEND (signal (SIGSEGV, sigsegv_handler) == SIG_ERR, "cannot install SIGSEGV handler", NULL);
#if ! defined ENABLE_WIN32
  ABEND (signal (SIGALRM, sigalrm_handler) == SIG_ERR, "cannot install SIGALRM handler", NULL);
  ABEND (signal (SIGPIPE, sigpipe_handler) == SIG_ERR, "cannot install SIGPIPE handler", NULL);
  ABEND (signal (SIGTTIN, sigttin_handler) == SIG_ERR, "cannot install SIGTTIN handler", NULL);
#endif
}

#if ! defined ENABLE_WIN32
#include <sys/resource.h>
#endif

ADDR_T fixed_heap_pointer, temp_heap_pointer;
POSTULATE_T *top_postulate, *top_postulate_list;
KEYWORD_T *top_keyword;
TOKEN_T *top_token;
BOOL_T get_fixed_heap_allowed;

static int tag_number = 0;

/*!
\brief actions when closing the heap
**/

void free_heap (void)
{
  return;
}

/*!
\brief pointer to block of "s" bytes
\param s block lenght in bytes
\return same
**/

void *get_heap_space (size_t s)
{
  char *z = (char *) (A68_ALIGN_T *) malloc (A68_ALIGN (s));
  ABEND (z == NULL, ERROR_OUT_OF_CORE, NULL);
  return ((void *) z);
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
\brief get_fixed_heap_space
\param s size in bytes
\return pointer to block
**/

BYTE_T *get_fixed_heap_space (size_t s)
{
  BYTE_T *z = HEAP_ADDRESS (fixed_heap_pointer);
  ABEND (get_fixed_heap_allowed == A68_FALSE, ERROR_INTERNAL_CONSISTENCY, NULL);
  fixed_heap_pointer += A68_ALIGN ((int) s);
  ABEND (fixed_heap_pointer >= temp_heap_pointer, ERROR_OUT_OF_CORE, NULL);
  ABEND (((long) z) % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
  return (z);
}

/*!
\brief get_temp_heap_space
\param s size in bytes
\return pointer to block
**/

BYTE_T *get_temp_heap_space (size_t s)
{
  BYTE_T *z;
  temp_heap_pointer -= A68_ALIGN ((int) s);
  ABEND (fixed_heap_pointer >= temp_heap_pointer, ERROR_OUT_OF_CORE, NULL);
  z = HEAP_ADDRESS (temp_heap_pointer);
  ABEND (((long) z) % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
  return (z);
}

/*!
\brief get size of stack segment
**/

void get_stack_size (void)
{
#if ! defined ENABLE_WIN32
  struct rlimit limits;
  RESET_ERRNO;
/* Some systems do not implement RLIMIT_STACK so if getrlimit fails, we do not abend. */
  if (!(getrlimit (RLIMIT_STACK, &limits) == 0 && errno == 0)) {
    stack_size = MEGABYTE;
  }
  stack_size = (int) (limits.rlim_cur < limits.rlim_max ? limits.rlim_cur : limits.rlim_max);
/* A heuristic in case getrlimit yields extreme numbers: the frame stack is
   assumed to fill at a rate comparable to the C stack, so the C stack needs
   not be larger than the frame stack. This may not be true. */
  if (stack_size < KILOBYTE || (stack_size > 96 * MEGABYTE && stack_size > frame_stack_size)) {
    stack_size = frame_stack_size;
  }
#elif defined ENABLE_WIN32
  stack_size = MEGABYTE;
#else
  stack_size = 0;               /* No stack check. */
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
  for (; p != NULL; FORWARD (p)) {
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
  for (; p != NULL; FORWARD (p)) {
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
  z->PROCEDURE_LEVEL = 0;
  z->char_in_line = NULL;
  z->symbol = NULL;
  z->line = NULL;
  return (z);
}

/*!
\brief new_genie_info
\return same
**/

GENIE_INFO_T *new_genie_info (void)
{
  GENIE_INFO_T *z = (GENIE_INFO_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (GENIE_INFO_T));
  new_genie_infos++;
  z->propagator.unit = NULL;
  z->propagator.source = NULL;
  z->partial_proc = NULL;
  z->partial_locale = NULL;
  z->whether_coercion = A68_FALSE;
  z->whether_new_lexical_level = A68_FALSE;
  z->need_dns = A68_FALSE;
  z->parent = NULL;
  z->offset = NULL;
  z->constant = NULL;
  z->level = 0;
  z->argsize = 0;
  z->size = 0;
  z->protect_sweep = NULL;
  z->compile_name = NULL;
  z->compile_node = 0;
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
  SYMBOL_TABLE (z) = NULL;
  INFO (z) = NULL;
  GENIE (z) = NULL;
  ATTRIBUTE (z) = 0;
  ANNOTATION (z) = 0;
  MOID (z) = NULL;
  NEXT (z) = NULL;
  PREVIOUS (z) = NULL;
  SUB (z) = NULL;
  NEST (z) = NULL;
  TAX (z) = NULL;
  SEQUENCE (z) = NULL;
  PACK (z) = NULL;
  return (z);
}

/*!
\brief new_symbol_table
\param p parent symbol table
\return same
**/

SYMBOL_TABLE_T *new_symbol_table (SYMBOL_TABLE_T * p)
{
  SYMBOL_TABLE_T *z = (SYMBOL_TABLE_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (SYMBOL_TABLE_T));
  z->level = symbol_table_count++;
  z->nest = symbol_table_count;
  ATTRIBUTE (z) = 0;
  z->ap_increment = 0;
  z->empty_table = A68_FALSE;
  z->initialise_frame = A68_TRUE;
  z->proc_ops = A68_TRUE;
  z->initialise_anon = A68_TRUE;
  PREVIOUS (z) = p;
  OUTER (z) = NULL;
  z->identifiers = NULL;
  z->operators = NULL;
  PRIO (z) = NULL;
  z->indicants = NULL;
  z->labels = NULL;
  z->anonymous = NULL;
  z->moids = NULL;
  z->jump_to = NULL;
  SEQUENCE (z) = NULL;
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
  z->well_formed = A68_FALSE;
  USE (z) = A68_FALSE;
  z->has_ref = A68_FALSE;
  z->has_flex = A68_FALSE;
  z->has_rows = A68_FALSE;
  z->in_standard_environ = A68_FALSE;
  SIZE (z) = 0;
  z->portable = A68_TRUE;
  NODE (z) = NULL;
  PACK (z) = NULL;
  SUB (z) = NULL;
  z->equivalent_mode = NULL;
  SLICE (z) = NULL;
  z->deflexed_mode = NULL;
  NAME (z) = NULL;
  z->multiple_mode = NULL;
  z->trim = NULL;
  NEXT (z) = NULL;
  return (z);
}

/*!
\brief new_pack
\return same
**/

PACK_T *new_pack (void)
{
  PACK_T *z = (PACK_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (PACK_T));
  MOID (z) = NULL;
  TEXT (z) = NULL;
  NODE (z) = NULL;
  NEXT (z) = NULL;
  PREVIOUS (z) = NULL;
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
  TAG_TABLE (z) = NULL;
  MOID (z) = NULL;
  NODE (z) = NULL;
  z->unit = NULL;
  VALUE (z) = NULL;
  z->stand_env_proc = 0;
  z->procedure = NULL;
  z->scope = PRIMAL_SCOPE;
  z->scope_assigned = A68_FALSE;
  PRIO (z) = 0;
  USE (z) = A68_FALSE;
  z->in_proc = A68_FALSE;
  HEAP (z) = A68_FALSE;
  SIZE (z) = 0;
  OFFSET (z) = 0;
  z->youngest_environ = PRIMAL_SCOPE;
  z->loc_assigned = A68_FALSE;
  NEXT (z) = NULL;
  BODY (z) = NULL;
  z->portable = A68_TRUE;
  NUMBER (z) = ++tag_number;
  return (z);
}

/*!
\brief new_source_line
\return same
**/

SOURCE_LINE_T *new_source_line (void)
{
  SOURCE_LINE_T *z = (SOURCE_LINE_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (SOURCE_LINE_T));
  z->marker[0] = NULL_CHAR;
  z->string = NULL;
  z->filename = NULL;
  z->diagnostics = NULL;
  NUMBER (z) = 0;
  z->print_status = 0;
  z->list = A68_TRUE;
  NEXT (z) = NULL;
  PREVIOUS (z) = NULL;
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
  PACK (*n) = NULL;
  SUB (*n) = NULL;
  EQUIVALENT (*n) = NULL;
  DEFLEXED (*n) = NULL;
  NAME (*n) = NULL;
  SLICE (*n) = NULL;
  ROWED (*n) = NULL;
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
  while ((a = va_arg (vl, int)) != NULL_ATTRIBUTE)
  {
    if (p != NULL && a == WILDCARD) {
      FORWARD (p);
    } else if (p != NULL && (a == KEYWORD)) {
      if (find_keyword_from_attribute (top_keyword, ATTRIBUTE (p)) != NULL) {
        FORWARD (p);
      } else {
        va_end (vl);
        return (A68_FALSE);
      }
    } else if (p != NULL && (a >= 0 ? a == ATTRIBUTE (p) : (-a) != ATTRIBUTE (p))) {
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

BOOL_T whether_one_of (NODE_T * p, ...)
{
  if (p != NULL) {
    va_list vl;
    int a;
    BOOL_T match = A68_FALSE;
    va_start (vl, p);
    while ((a = va_arg (vl, int)) != NULL_ATTRIBUTE)
    {
      match = (BOOL_T) (match | (BOOL_T) (WHETHER (p, a)));
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
  ABEND (p == NULL || q == NULL, ERROR_INTERNAL_CONSISTENCY, "make_sub");
  *z = *p;
  if (GENIE (p) != NULL) {
    GENIE (z) = new_genie_info ();
  }
  PREVIOUS (z) = NULL;
  if (p == q) {
    NEXT (z) = NULL;
  } else {
    if (NEXT (p) != NULL) {
      PREVIOUS (NEXT (p)) = z;
    }
    NEXT (p) = NEXT (q);
    if (NEXT (p) != NULL) {
      PREVIOUS (NEXT (p)) = p;
    }
    NEXT (q) = NULL;
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

SYMBOL_TABLE_T *find_level (NODE_T * n, int i)
{
  if (n == NULL) {
    return (NULL);
  } else {
    SYMBOL_TABLE_T *s = SYMBOL_TABLE (n);
    if (s != NULL && LEVEL (s) == i) {
      return (s);
    } else if ((s = find_level (SUB (n), i)) != NULL) {
      return (s);
    } else if ((s = find_level (NEXT (n), i)) != NULL) {
      return (s);
    } else {
      return (NULL);
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

BOOL_T whether_new_lexical_level (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ALT_DO_PART:
  case BRIEF_ELIF_IF_PART:
  case BRIEF_INTEGER_OUSE_PART:
  case BRIEF_UNITED_OUSE_PART:
  case CHOICE:
  case CLOSED_CLAUSE:
  case CONDITIONAL_CLAUSE:
  case DO_PART:
  case ELIF_PART:
  case ELSE_PART:
  case FORMAT_TEXT:
  case INTEGER_CASE_CLAUSE:
  case INTEGER_CHOICE_CLAUSE:
  case INTEGER_IN_PART:
  case INTEGER_OUT_PART:
  case OUT_PART:
  case ROUTINE_TEXT:
  case SPECIFIED_UNIT:
  case THEN_PART:
  case UNTIL_PART:
  case UNITED_CASE_CLAUSE:
  case UNITED_CHOICE:
  case UNITED_IN_PART:
  case UNITED_OUSE_PART:
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
  GENIE (z) = new_genie_info ();
  SYMBOL (z) = t;
  return (z);
}

/*!
\brief initialise use of elem-lists
**/

void init_postulates (void)
{
  top_postulate = NULL;
  top_postulate_list = NULL;
}

/*!
\brief make old postulates available for new use
\param start start of list to save
\param stop first element to not save
**/

void free_postulate_list (POSTULATE_T *start, POSTULATE_T *stop)
{
  POSTULATE_T *last;
  if (start == NULL && stop == NULL) {
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
  if (top_postulate_list != NULL) {
    new_one = top_postulate_list;
    FORWARD (top_postulate_list);
  } else {
    new_one = (POSTULATE_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (POSTULATE_T));
    new_postulates++;
  }
  new_one->a = a;
  new_one->b = b;
  NEXT (new_one) = *p;
  *p = new_one;
}

/*!
\brief where elements are in the list
\param p postulate chain
\param a moid 1
\param b moid 2
\return containing postulate or NULL
**/

POSTULATE_T *whether_postulated_pair (POSTULATE_T * p, MOID_T * a, MOID_T * b)
{
  for (; p != NULL; FORWARD (p)) {
    if (p->a == a && p->b == b) {
      return (p);
    }
  }
  return (NULL);
}

/*!
\brief where element is in the list
\param p postulate chain
\param a moid 1
\return containing postulate or NULL
**/

POSTULATE_T *whether_postulated (POSTULATE_T * p, MOID_T * a)
{
  for (; p != NULL; FORWARD (p)) {
    if (p->a == a) {
      return (p);
    }
  }
  return (NULL);
}

/*------------------+
| Control of C heap |
+------------------*/

/*!
\brief discard_heap
**/

void discard_heap (void)
{
  if (heap_segment != NULL) {
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
  ABEND (core == NULL, ERROR_OUT_OF_CORE, NULL);
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
  while (*p != NULL) {
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
  LESS (*p) = MORE (*p) = NULL;
  return (*p);
}

/*!
\brief find token in the token tree
\param p top token
\param t text to find
\return entry or NULL
**/

TOKEN_T *find_token (TOKEN_T ** p, char *t)
{
  while (*p != NULL) {
    int k = strcmp (t, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      return (*p);
    }
  }
  return (NULL);
}

/*!
\brief find keyword, from token name
\param p top keyword
\param t token text to find
\return entry or NULL
**/

KEYWORD_T *find_keyword (KEYWORD_T * p, char *t)
{
  while (p != NULL) {
    int k = strcmp (t, TEXT (p));
    if (k < 0) {
      p = LESS (p);
    } else if (k > 0) {
      p = MORE (p);
    } else {
      return (p);
    }
  }
  return (NULL);
}

/*!
\brief find keyword, from attribute
\param p top keyword
\param a token attribute
\return entry or NULL
**/

KEYWORD_T *find_keyword_from_attribute (KEYWORD_T * p, int a)
{
  if (p == NULL) {
    return (NULL);
  } else if (a == ATTRIBUTE (p)) {
    return (p);
  } else {
    KEYWORD_T *z;
    if ((z = find_keyword_from_attribute (LESS (p), a)) != NULL) {
      return (z);
    } else if ((z = find_keyword_from_attribute (MORE (p), a)) != NULL) {
      return (z);
    } else {
      return (NULL);
    }
  }
}

/* A list of 10 ^ 2 ^ n for conversion purposes on IEEE 754 platforms. */

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
/* This way appears sufficiently accurate. */
  double dbl_expo = 1.0, *dep;
  BOOL_T neg_expo = (BOOL_T) (expo < 0);
  if (neg_expo) {
    expo = -expo;
  }
  ABEND (expo > MAX_DOUBLE_EXPO, "exponent too large", NULL);
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
\brief search last char in string
\param str string to search
\param c character to find
\return pointer to last "c" in "str"
**/

char *a68g_strrchr (char *str, int c)
{
  return (strrchr (str, c));
}

/*!
\brief safely append to buffer
\param dst text buffer
\param src text to append
\param siz size of dst
**/

void bufcat (char *dst, char *src, int len)
{
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

/*!
\brief safely copy to buffer
\param dst text buffer
\param src text to append
\param siz size of dst
**/

void bufcpy (char *dst, char *src, int len)
{
  char *d = dst, *s = src;
  int n = len;
/* Copy as many fits */
  if (n > 0 && --n != 0) {
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
#if defined ENABLE_REGEX

#include <regex.h>

  int rc, nmatch, k, max_k, widest;
  regex_t compiled;
  regmatch_t *matches;
  rc = regcomp (&compiled, pat, REG_NEWLINE | REG_EXTENDED);
  if (rc != 0) {
    regfree (&compiled);
    return (rc);
  }
  nmatch = (int) (compiled.re_nsub);
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = malloc ((size_t) (nmatch * ALIGNED_SIZE_OF (regmatch_t)));
  if (nmatch > 0 && matches == NULL) {
    regfree (&compiled);
    return (2);
  }
  rc = regexec (&compiled, str, (size_t) nmatch, matches, 0);
  if (rc != 0) {
    regfree (&compiled);
    return (rc);
  }
/* Find widest match. Do not assume it is the first one. */
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = (int) matches[k].rm_eo - (int) matches[k].rm_so;
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  if (start != NULL) {
    (*start) = (int) matches[max_k].rm_so;
  }
  if (end != NULL) {
    (*end) = (int) matches[max_k].rm_eo;
  }
  free (matches);
  return (0);
#else
  (void) start;
  (void) end;
  if (strstr (str, pat) != NULL) {
    return (0);
  } else {
    return (1);
  }
#endif /* ENABLE_REGEX */
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
  return ((BOOL_T) (a68g_strchr ("aeiouAEIOU", ch) != NULL));
}

/*!
\brief whether ch is consonant
\param ch character under test
\return same
**/

static BOOL_T is_consonant (char ch)
{
  return ((BOOL_T) (a68g_strchr ("qwrtypsdfghjklzxcvbnmQWRTYPSDFGHJKLZXCVBNM", ch) != NULL));
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
  strncpy (str2, str, BUFFER_SIZE);
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
    if (next == NULL) {
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
  if (u != NULL && u[0] != NULL_CHAR && v != NULL) {
    make_acronym (u, v);
    PUSH_REF (p, c_to_a_string (p, v));
  } else {
    PUSH_REF (p, empty_string (p));
  }
  if (u != NULL) {
    free (u);
  }
  if (v != NULL) {
    free (v);
  }
}

/* Translate int attributes to string names */

static char *attribute_names[WILDCARD + 1] = {
  "NULL",
  "A68_PATTERN",
  "ACCO_SYMBOL",
  "ACTUAL_DECLARER_MARK",
  "STYLE_DO_PART",
  "STYLE_DO_SYMBOL",
  "STYLE_EQUALS_SYMBOL",
  "STYLE_FORMAL_BOUNDS_LIST",
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
  "BOLD_COMMENT_SYMBOL",
  "BOLD_PRAGMAT_SYMBOL",
  "BOLD_TAG",
  "BOOLEAN_PATTERN",
  "BOOL_SYMBOL",
  "BOUND",
  "BOUNDS",
  "BOUNDS_LIST",
  "BRIEF_ELIF_IF_PART",
  "BRIEF_INTEGER_OUSE_PART",
  "BRIEF_OPERATOR_DECLARATION",
  "BRIEF_UNITED_OUSE_PART",
  "BUS_SYMBOL",
  "BY_PART",
  "BY_SYMBOL",
  "BYTES_SYMBOL",
  "CALL",
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
  "DO_PART",
  "DO_SYMBOL",
  "DOTDOT_SYMBOL",
  "DOWNTO_SYMBOL",
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
  "FI_SYMBOL",
  "FIXED_C_PATTERN",
  "FLEX_SYMBOL",
  "FLOAT_C_PATTERN",
  "FORMAL_BOUNDS",
  "FORMAL_BOUNDS_LIST",
  "FORMAL_DECLARER_MARK",
  "FORMAL_DECLARERS",
  "FORMAL_DECLARERS_LIST",
  "FORMAT_A_FRAME",
  "FORMAT_CLOSE_SYMBOL",
  "FORMAT_DELIMITER_SYMBOL",
  "FORMAT_D_FRAME",
  "FORMAT_E_FRAME",
  "FORMAT_IDENTIFIER",
  "FORMAT_I_FRAME",
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
  "GO_SYMBOL",
  "GOTO_SYMBOL",
  "HEAP_SYMBOL",
  "IDENTIFIER",
  "IDENTITY_DECLARATION",
  "IDENTITY_RELATION",
  "IF_PART",
  "IF_SYMBOL",
  "INDICANT",
  "INITIALISER_SERIES",
  "INSERTION",
  "IN_SYMBOL",
  "INT_DENOTATION",
  "INTEGER_CASE_CLAUSE",
  "INTEGER_CHOICE_CLAUSE",
  "INTEGER_IN_PART",
  "INTEGER_OUT_PART",
  "INTEGRAL_C_PATTERN",
  "INTEGRAL_MOULD",
  "INTEGRAL_PATTERN",
  "INT_SYMBOL",
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
  "MODE_LONG_BITS",
  "MODE_LONG_BYTES",
  "MODE_LONG_COMPLEX",
  "MODE_LONG_INT",
  "MODE_LONGLONG_BITS",
  "MODE_LONGLONG_COMPLEX",
  "MODE_LONGLONG_INT",
  "MODE_LONGLONG_REAL",
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
  "OUSE_CASE_PART",
  "OUSE_SYMBOL",
  "OUT_PART",
  "OUT_SYMBOL",
  "OUT_TYPE_MODE",
  "PARALLEL_CLAUSE",
  "PARAMETER",
  "PARAMETER_IDENTIFIER",
  "PARAMETER_LIST",
  "PARAMETER_PACK",
  "PAR_SYMBOL",
  "PARTICULAR_PROGRAM",
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
  "PROTECT_FROM_SWEEP",
  "QUALIFIER",
  "RADIX_FRAME",
  "REAL_DENOTATION",
  "REAL_PATTERN",
  "REAL_SYMBOL",
  "REF_SYMBOL",
  "REPLICATOR",
  "ROUTINE_TEXT",
  "ROUTINE_UNIT",
  "ROW_ASSIGNATION",
  "ROW_ASSIGN_SYMBOL",
  "ROW_CHAR_DENOTATION",
  "ROW_FUNCTION",
  "ROWING",
  "ROWS_SYMBOL",
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
  "STRUCT_SYMBOL",
  "STRUCTURED_FIELD",
  "STRUCTURED_FIELD_LIST",
  "STRUCTURE_PACK",
  "STYLE_I_COMMENT_SYMBOL",
  "STYLE_II_COMMENT_SYMBOL",
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
  "UNITED_CASE_CLAUSE",
  "UNITED_CHOICE",
  "UNITED_IN_PART",
  "UNITED_OUSE_PART",
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
\return buf, containing name of non terminal string, or NULL
**/

char *non_terminal_string (char *buf, int att)
{
  if (att > 0 && att < WILDCARD) {
    if (attribute_names[att] != NULL) {
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
      return (NULL);
    }
  } else {
    return (NULL);
  }
}

/*!
\brief standard_environ_proc_name
\param f routine that implements a standard environ item
\return name of that what "f" implements
**/

char *standard_environ_proc_name (GENIE_PROCEDURE f)
{
  TAG_T *i = stand_env->identifiers;
  for (; i != NULL; FORWARD (i)) {
    if (i->procedure == f) {
      return (SYMBOL (NODE (i)));
    }
  }
  return (NULL);
}

/*!
\brief propagator_name
\param p propagator procedure
\return function name of "p"
**/

char *propagator_name (PROPAGATOR_PROCEDURE * p)
{
  if (p == genie_and_function) {
    return ("genie_and_function");
  }
  if (p == genie_assertion) {
    return ("genie_assertion");
  }
  if (p == genie_assignation) {
    return ("genie_assignation");
  }
  if (p == genie_assignation_constant) {
    return ("genie_assignation_constant");
  }
  if (p == genie_call) {
    return ("genie_call");
  }
  if (p == genie_cast) {
    return ("genie_cast");
  }
  if (p == (PROPAGATOR_PROCEDURE *) genie_closed) {
    return ("genie_closed");
  }
  if (p == genie_coercion) {
    return ("genie_coercion");
  }
  if (p == genie_collateral) {
    return ("genie_collateral");
  }
  if (p == genie_column_function) {
    return ("genie_column_function");
  }
  if (p == (PROPAGATOR_PROCEDURE *) genie_conditional) {
    return ("genie_conditional");
  }
  if (p == genie_constant) {
    return ("genie_constant");
  }
  if (p == genie_denotation) {
    return ("genie_denotation");
  }
  if (p == genie_deproceduring) {
    return ("genie_deproceduring");
  }
  if (p == genie_dereference_frame_identifier) {
    return ("genie_dereference_frame_identifier");
  }
  if (p == genie_dereference_selection_name_quick) {
    return ("genie_dereference_selection_name_quick");
  }
  if (p == genie_dereference_slice_name_quick) {
    return ("genie_dereference_slice_name_quick");
  }
  if (p == genie_dereferencing) {
    return ("genie_dereferencing");
  }
  if (p == genie_dereferencing_quick) {
    return ("genie_dereferencing_quick");
  }
  if (p == genie_diagonal_function) {
    return ("genie_diagonal_function");
  }
  if (p == genie_dyadic) {
    return ("genie_dyadic");
  }
  if (p == genie_dyadic_quick) {
    return ("genie_dyadic_quick");
  }
  if (p == (PROPAGATOR_PROCEDURE *) genie_enclosed) {
    return ("genie_enclosed");
  }
  if (p == genie_format_text) {
    return ("genie_format_text");
  }
  if (p == genie_formula) {
    return ("genie_formula");
  }
  if (p == genie_generator) {
    return ("genie_generator");
  }
  if (p == genie_identifier) {
    return ("genie_identifier");
  }
  if (p == genie_identifier_standenv) {
    return ("genie_identifier_standenv");
  }
  if (p == genie_identifier_standenv_proc) {
    return ("genie_identifier_standenv_proc");
  }
  if (p == genie_identity_relation) {
    return ("genie_identity_relation");
  }
  if (p == (PROPAGATOR_PROCEDURE *) genie_int_case) {
    return ("genie_int_case");
  }
  if (p == genie_field_selection) {
    return ("genie_field_selection");
  }
  if (p == genie_frame_identifier) {
    return ("genie_frame_identifier");
  }
  if (p == (PROPAGATOR_PROCEDURE *) genie_loop) {
    return ("genie_loop");
  }
  if (p == genie_monadic) {
    return ("genie_monadic");
  }
  if (p == genie_nihil) {
    return ("genie_nihil");
  }
  if (p == genie_or_function) {
    return ("genie_or_function");
  }
#if defined ENABLE_PAR_CLAUSE
  if (p == genie_parallel) {
    return ("genie_parallel");
  }
#endif
  if (p == genie_routine_text) {
    return ("genie_routine_text");
  }
  if (p == genie_row_function) {
    return ("genie_row_function");
  }
  if (p == genie_rowing) {
    return ("genie_rowing");
  }
  if (p == genie_rowing_ref_row_of_row) {
    return ("genie_rowing_ref_row_of_row");
  }
  if (p == genie_rowing_ref_row_row) {
    return ("genie_rowing_ref_row_row");
  }
  if (p == genie_rowing_row_of_row) {
    return ("genie_rowing_row_of_row");
  }
  if (p == genie_rowing_row_row) {
    return ("genie_rowing_row_row");
  }
  if (p == genie_selection) {
    return ("genie_selection");
  }
  if (p == genie_selection_name_quick) {
    return ("genie_selection_name_quick");
  }
  if (p == genie_selection_value_quick) {
    return ("genie_selection_value_quick");
  }
  if (p == genie_skip) {
    return ("genie_skip");
  }
  if (p == genie_slice) {
    return ("genie_slice");
  }
  if (p == genie_slice_name_quick) {
    return ("genie_slice_name_quick");
  }
  if (p == genie_transpose_function) {
    return ("genie_transpose_function");
  }
  if (p == genie_unit) {
    return ("genie_unit");
  }
  if (p == (PROPAGATOR_PROCEDURE *) genie_united_case) {
    return ("genie_united_case");
  }
  if (p == genie_uniting) {
    return ("genie_uniting");
  }
  if (p == genie_voiding) {
    return ("genie_voiding");
  }
  if (p == genie_voiding_assignation) {
    return ("genie_voiding_assignation");
  }
  if (p == genie_voiding_assignation_constant) {
    return ("genie_voiding_assignation_constant");
  }
  if (p == genie_widening) {
    return ("genie_widening");
  }
  if (p == genie_widening_int_to_real) {
    return ("genie_widening_int_to_real");
  }
  return (NULL);
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
  {NULL, NULL, NULL}
};

/*!
\brief print_info
\param f file number
\param prompt prompt text
\param k index of info item to print
**/

static void print_info (FILE_T f, char *prompt, int k)
{
  if (prompt != NULL) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s %s: %s.", prompt, info_text[k].term, info_text[k].def) >= 0);
  } else {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s: %s.", info_text[k].term, info_text[k].def) >= 0);
  }
  WRITELN (f, output_line);
}

/*!
\brief apropos
\param f file number
\param prompt prompt text
\param info item to print, NULL prints all
**/

void apropos (FILE_T f, char *prompt, char *item)
{
  int k, n = 0;
  if (item == NULL) {
    for (k = 0; info_text[k].cat != NULL; k++) {
      print_info (f, prompt, k);
    }
    return;
  }
  for (k = 0; info_text[k].cat != NULL; k++) {
    if (grep_in_string (item, info_text[k].cat, NULL, NULL) == 0) {
      print_info (f, prompt, k);
      n++;
    }
  }
  if (n > 0) {
    return;
  }
  for (k = 0; info_text[k].cat != NULL; k++) {
    if (grep_in_string (item, info_text[k].term, NULL, NULL) == 0) {
      print_info (f, prompt, k);
      n++;
    }
  }
  if (n > 0) {
    return;
  }
  for (k = 0; info_text[k].cat != NULL; k++) {
    if (grep_in_string (item, info_text[k].def, NULL, NULL) == 0) {
      print_info (f, prompt, k);
    }
  }
}

/* Error handling routines */

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
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s: exiting: %s: %d: %s", a68g_cmd_name, file, line, reason) >= 0);
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

void write_source_line (FILE_T f, SOURCE_LINE_T * p, NODE_T * nwhere, int diag)
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
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "      ") >= 0);
  } else {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%-5d ", NUMBER (p) % 100000) >= 0);
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
      DIAGNOSTIC_T *d = p->diagnostics;
      if (d != NULL || nwhere != NULL) {
        char *c1;
        for (c1 = c0; c1 != c; c1++) {
          y |= (BOOL_T) (nwhere != NULL && p == LINE (nwhere) ? c1 == where_pos (p, nwhere) : A68_FALSE);
          if (diag != A68_NO_DIAGNOSTICS) {
            for (d = p->diagnostics; d != NULL; FORWARD (d)) {
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
          for (d2 = p->diagnostics; d2 != NULL; FORWARD (d2)) {
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
              ASSERT (snprintf (output_line, BUFFER_SIZE, "%c", digit_to_char (k)) >= 0);
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
        ASSERT (snprintf (output_line, BUFFER_SIZE, "\n.%1d   ", continuations) >= 0);
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
          z = (BOOL_T) (z | (WHETHER (d, A68_WARNING) || WHETHER (d, A68_ERROR) || WHETHER (d, A68_SYNTAX_ERROR) || WHETHER (d, A68_MATH_ERROR) || WHETHER (d, A68_SUPPRESS_SEVERITY)));
        } else if (what == A68_RUNTIME_ERROR) {
          z = (BOOL_T) (z | (WHETHER (d, A68_RUNTIME_ERROR)));
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
  longjmp (program.exit_compilation, 1);
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
      program.error_count++;
      return ("error");
    }
  case A68_SYNTAX_ERROR:
    {
      program.error_count++;
      return ("syntax error");
    }
  case A68_RUNTIME_ERROR:
    {
      program.error_count++;
      return ("runtime error");
    }
  case A68_MATH_ERROR:
    {
      program.error_count++;
      return ("math error");
    }
  case A68_WARNING:
    {
      program.warning_count++;
      return ("warning");
    }
  case A68_SUPPRESS_SEVERITY:
    {
      program.error_count++;
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
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s: %s.", a68g_cmd_name, b) >= 0);
  } else {
    bufcpy (st, get_severity (sev), SMALL_BUFFER_SIZE);
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s: %s: %s.", a68g_cmd_name, st, b) >= 0);
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
  DIAGNOSTIC_T *msg = (DIAGNOSTIC_T *) get_heap_space ((size_t) ALIGNED_SIZE_OF (DIAGNOSTIC_T));
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
          ASSERT (snprintf (nst, (size_t) BUFFER_SIZE, "detected in %s", nt) >= 0);
        } else {
          if (MOID (n) != NULL) {
            if (NUMBER (LINE (n)) == NUMBER (line)) {
              ASSERT (snprintf (nst, (size_t) BUFFER_SIZE, "detected in %s %s starting at \"%.64s\" in this line", moid_to_string (MOID (n), MOID_ERROR_WIDTH, p), nt, SYMBOL (n)) >= 0);
            } else {
              ASSERT (snprintf (nst, (size_t) BUFFER_SIZE, "detected in %s %s starting at \"%.64s\" in line %d", moid_to_string (MOID (n), MOID_ERROR_WIDTH, p), nt, SYMBOL (n), NUMBER (LINE (n))) >= 0);
            }
          } else {
            if (NUMBER (LINE (n)) == NUMBER (line)) {
              ASSERT (snprintf (nst, (size_t) BUFFER_SIZE, "detected in %s starting at \"%.64s\" in this line", nt, SYMBOL (n)) >= 0);
            } else {
              ASSERT (snprintf (nst, (size_t) BUFFER_SIZE, "detected in %s starting at \"%.64s\" in line %d", nt, SYMBOL (n), NUMBER (LINE (n))) >= 0);
            }
          }
        }
      }
    }
  }
  if (severity == NULL) {
    if (line->filename != NULL && strcmp (program.files.source.name, line->filename) == 0) {
      ASSERT (snprintf (a, (size_t) BUFFER_SIZE, "%s: %x: %s", a68g_cmd_name, (unsigned) k, b) >= 0);
    } else if (line->filename != NULL) {
      ASSERT (snprintf (a, (size_t) BUFFER_SIZE, "%s: %s: %x: %s", a68g_cmd_name, line->filename, (unsigned) k, b) >= 0);
    } else {
      ASSERT (snprintf (a, (size_t) BUFFER_SIZE, "%s: %x: %s", a68g_cmd_name, (unsigned) k, b) >= 0);
    }
  } else {
    bufcpy (st, get_severity (sev), SMALL_BUFFER_SIZE);
    if (line->filename != NULL && strcmp (program.files.source.name, line->filename) == 0) {
      ASSERT (snprintf (a, (size_t) BUFFER_SIZE, "%s: %s: %x: %s", a68g_cmd_name, st, (unsigned) k, b) >= 0);
    } else if (line->filename != NULL) {
      ASSERT (snprintf (a, (size_t) BUFFER_SIZE, "%s: %s: %s: %x: %s", a68g_cmd_name, line->filename, st, (unsigned) k, b) >= 0);
    } else {
      ASSERT (snprintf (a, (size_t) BUFFER_SIZE, "%s: %s: %x: %s", a68g_cmd_name, st, (unsigned) k, b) >= 0);
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
      ASSERT (snprintf(d, (size_t) BUFFER_SIZE, "%d", a) >= 0);\
      bufcat (b, d, BUFFER_SIZE);\
    } else if (t[0] == 'H') {\
      char *a = va_arg (args, char *);\
      char d[SMALL_BUFFER_SIZE];\
      ASSERT (snprintf(d, (size_t) SMALL_BUFFER_SIZE, "\"%c\"", a[0]) >= 0);\
      bufcat (b, d, BUFFER_SIZE);\
    } else if (t[0] == 'L') {\
      SOURCE_LINE_T *a = va_arg (args, SOURCE_LINE_T *);\
      char d[SMALL_BUFFER_SIZE];\
      ABEND (a == NULL, "NULL source line in error", NULL);\
      if (NUMBER (a) == 0) {\
	bufcat (b, "in standard environment", BUFFER_SIZE);\
      } else {\
        if (p != NULL && NUMBER (a) == LINE_NUMBER (p)) {\
          ASSERT (snprintf(d, (size_t) SMALL_BUFFER_SIZE, "in this line") >= 0);\
	} else {\
          ASSERT (snprintf(d, (size_t) SMALL_BUFFER_SIZE, "in line %d", NUMBER (a)) >= 0);\
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
      char *loc_string = va_arg (args, char *);\
      bufcat (b, loc_string, BUFFER_SIZE);\
    } else if (t[0] == 'X') {\
      int att = va_arg (args, int);\
      char z[BUFFER_SIZE];\
      /* ASSERT (snprintf(z, (size_t) BUFFER_SIZE, "\"%s\"", find_keyword_from_attribute (top_keyword, att)->text) >= 0); */\
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
  MOID_T *moid = NULL;
  char *t = loc_str, b[BUFFER_SIZE];
  BOOL_T force, extra_syntax = A68_TRUE, shortcut = A68_FALSE;
  int err = errno;
  va_start (args, loc_str);
  b[0] = NULL_CHAR;
  force = (BOOL_T) ((sev & A68_FORCE_DIAGNOSTICS) != 0);
  sev &= ~A68_FORCE_DIAGNOSTICS;
/* No warnings? */
  if (!force && sev == A68_WARNING && no_warnings) {
    return;
  }
/* Suppressed? */
  if (sev == A68_ERROR || sev == A68_SYNTAX_ERROR) {
    if (program.error_count == MAX_ERRORS) {
      bufcpy (b, "further error diagnostics suppressed", BUFFER_SIZE);
      sev = A68_ERROR;
      shortcut = A68_TRUE;
    } else if (program.error_count > MAX_ERRORS) {
      program.error_count++;
      return;
    }
  } else if (sev == A68_WARNING) {
    if (program.warning_count == MAX_ERRORS) {
      bufcpy (b, "further warning diagnostics suppressed", BUFFER_SIZE);
      shortcut = A68_TRUE;
    } else if (program.warning_count > MAX_ERRORS) {
      program.warning_count++;
      return;
    }
  }
  if (shortcut == A68_FALSE) {
/* Synthesize diagnostic message. */
    COMPOSE_DIAGNOSTIC;
/* Add information from errno, if any. */
    if (err != 0) {
      char *loc_str2 = new_string (ERROR_SPECIFICATION);
      if (loc_str2 != NULL) {
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
\param loc_str message string
\param ... various arguments needed by special symbols in loc_str
**/

void diagnostic_line (int sev, SOURCE_LINE_T * line, char *pos, char *loc_str, ...)
{
  va_list args;
  MOID_T *moid = NULL;
  char *t = loc_str, b[BUFFER_SIZE];
  BOOL_T force, extra_syntax = A68_TRUE, shortcut = A68_FALSE;
  int err = errno;
  NODE_T *p = NULL;
  b[0] = NULL_CHAR;
  va_start (args, loc_str);
  force = (BOOL_T) ((sev & A68_FORCE_DIAGNOSTICS) != 0);
  sev &= ~A68_FORCE_DIAGNOSTICS;
/* No warnings? */
  if (!force && sev == A68_WARNING && no_warnings) {
    return;
  }
/* Suppressed? */
  if (sev == A68_ERROR || sev == A68_SYNTAX_ERROR) {
    if (program.error_count == MAX_ERRORS) {
      bufcpy (b, "further error diagnostics suppressed", BUFFER_SIZE);
      sev = A68_ERROR;
      shortcut = A68_TRUE;
    } else if (program.error_count > MAX_ERRORS) {
      program.error_count++;
      return;
    }
  } else if (sev == A68_WARNING) {
    if (program.warning_count == MAX_ERRORS) {
      bufcpy (b, "further warning diagnostics suppressed", BUFFER_SIZE);
      shortcut = A68_TRUE;
    } else if (program.warning_count > MAX_ERRORS) {
      program.warning_count++;
      return;
    }
  }
  if (!shortcut) {
/* Synthesize diagnostic message. */
    COMPOSE_DIAGNOSTIC;
/* Add information from errno, if any. */
    if (err != 0) {
      char *loc_str2 = new_string (ERROR_SPECIFICATION);
      if (loc_str2 != NULL) {
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
/* Construct a diagnostic message. */
  if (pos != NULL && IS_PRINT (*pos)) {
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
  if (line == NULL) {
    write_diagnostic (sev, b);
  } else {
    add_diagnostic (line, pos, NULL, sev, b);
  }
  va_end (args);
}
