/*!
\file algol68g.c
\brief
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
For the things we have to learn before we can do them,
                               we learn by doing them.

                     - Aristotle, Nichomachean Ethics


Algol68G is an Algol 68 interpreter.

Please refer to the documentation that comes with this distribution for a
detailed description of Algol68G.
*/

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

#ifdef WIN32_VERSION
#include <sys/time.h>
#else
#include <sys/times.h>
#endif

int global_argc;                /* Keep argc and argv for reference from A68. */
char **global_argv;

#ifdef HAVE_TERMINFO
#include <term.h>
char term_buffer[2 * KILOBYTE];
char *term_type;
#endif
int term_width;

BOOL_T tree_listing_safe, cross_reference_safe, moid_listing_safe;
BYTE_T *system_stack_offset;
MODES_T a68_modes;
MODULE_T a68_prog, *current_module;
int source_scan;
int stack_size;
int symbol_table_count, mode_count;
jmp_buf exit_compilation;

static void announce_phase (char *);
static void compiler_interpreter (void);

/*!
\brief state license of running a68g image
\param f file number
**/

void state_license (FILE_T f)
{
#define P(s)\
  snprintf (output_line, BUFFER_SIZE, "%s\n", (s));\
  io_write_string (f, output_line);

  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  snprintf (output_line, BUFFER_SIZE, "Algol 68 Genie %s, copyright 2006 J. Marcel van der Veer.\n", REVISION);
  io_write_string (f, output_line);
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
  snprintf (output_line, BUFFER_SIZE, "Algol 68 Genie %s, %s\n", REVISION, RELEASE_DATE);
  io_write_string (f, output_line);
#if ! defined WIN32_VERSION
  snprintf (output_line, BUFFER_SIZE, "Image \"%s\" compiled by %s on %s %s", A68G_NAME, USERID, __DATE__, __TIME__);
  io_write_string (f, output_line);
#endif
#if defined __GNUC__ && defined GCC_VERSION
  snprintf (output_line, BUFFER_SIZE, " with gcc %s\n", GCC_VERSION);
  io_write_string (f, output_line);
#else
  snprintf (output_line, BUFFER_SIZE, "\n", GCC_VERSION);
  io_write_string (f, output_line);
#endif
#if defined HAVE_PLOTUTILS && defined A68_LIBPLOT_VERSION
  snprintf (output_line, BUFFER_SIZE, "GNU Plotutils libplot %s\n", A68_LIBPLOT_VERSION);
  io_write_string (f, output_line);
#endif
#if defined HAVE_GSL && defined A68_GSL_VERSION
  snprintf (output_line, BUFFER_SIZE, "GNU Scientific Library %s\n", A68_GSL_VERSION);
  io_write_string (f, output_line);
#endif
#if defined HAVE_POSTGRESQL && defined A68_PG_VERSION
  snprintf (output_line, BUFFER_SIZE, "PostgreSQL libpq %s\n", A68_PG_VERSION);
  io_write_string (f, output_line);
#endif
  snprintf (output_line, BUFFER_SIZE, "Alignment %d bytes\n", ALIGNMENT);
  io_write_string (f, output_line);
  default_mem_sizes ();
  snprintf (output_line, BUFFER_SIZE, "Default frame stack size: %ld kB\n", frame_stack_size / KILOBYTE);
  io_write_string (f, output_line);
  snprintf (output_line, BUFFER_SIZE, "Default expression stack size: %ld kB\n", expr_stack_size / KILOBYTE);
  io_write_string (f, output_line);
  snprintf (output_line, BUFFER_SIZE, "Default heap size: %ld kB\n", heap_size / KILOBYTE);
  io_write_string (f, output_line);
  snprintf (output_line, BUFFER_SIZE, "Default handle pool size: %ld kB\n", handle_pool_size / KILOBYTE);
  io_write_string (f, output_line);
  snprintf (output_line, BUFFER_SIZE, "Default stack overhead: %ld kB\n", storage_overhead / KILOBYTE);
  io_write_string (f, output_line);
}

/*!
\brief give brief help if someone types 'a68g -help'
\param f file number
**/

void online_help (FILE_T f)
{
#define P(s)\
  snprintf (output_line, BUFFER_SIZE, "%s\n", (s));\
  io_write_string (f, output_line);

  if (f == STDOUT_FILENO) {
    io_close_tty_line ();
  }
  state_license (f);
  snprintf (output_line, BUFFER_SIZE, "Usage: %s [options | filename]\n", A68G_NAME);
  io_write_string (f, output_line);
  P ("");
  P ("Options that execute Algol 68 code from the command line:");
  P ("");
  P ("   print unit                Print value yielded by Algol 68 unit \"unit\"");
  P ("   execute unit              Execute Algol 68 unit \"unit\"");
  P ("");
  P ("Options to control the listing file:");
  P ("");
  P ("   extensive                 Make extensive listing");
  P ("   listing                   Make concise listing");
  P ("   moids                     Make overview of moids in listing file");
  P ("   preludelisting            Make a listing of preludes");
  P ("   source, nosource          Switch listing of source lines in listing file on or off");
  P ("   statistics                Print statistics in listing file");
  P ("   tree, notree              Switch syntax tree listing in listing file on or off");
  P ("   unused                    Make an overview of unused tags in the listing file");
  P ("   xref, noxref              Switch cross reference in the listing file on or off");
  P ("");
  P ("Interpreter options:");
  P ("");
  P ("   assertions, noassertions  Switch elaboration of assertions on or off");
  P ("   precision number          Set precision for LONG LONG modes to \"number\" significant digits");
  P ("   timelimit number          Interrupt the interpreter after \"number\" seconds");
  P ("   trace, notrace            Switch tracing of a running program on or off");
  P ("");
  P ("Options to control the stropping regime:");
  P ("");
  P ("   boldstropping             Set stropping mode to bold stropping");
  P ("   quotestropping            Set stropping mode to quote stropping");
  P ("");
  P ("Options to control memory usage:");
  P ("");
  P ("   heap number               Set heap size to \"number\"");
  P ("   handles number            Set handle space size to \"number\"");
  P ("   frame number              Set frame stack size to \"number\"");
  P ("   stack number              Set expression stack size to \"number\"");
  P ("");
  P ("Miscellaneous options:");
  P ("");
  P ("   brackets                  Consider [ .. ] and { .. } as equivalent to ( .. )");
  P ("   check, norun              Check syntax only, interpreter does not start");
  P ("   run                       Override CHECK/NORUN options");
  P ("   echo string               Echo \"string\" to standard output");
  P ("   exit, --                  Ignore next options");
  P ("   file string               Accept string as generic filename");
  P ("   a68cdiagnostics           Give A68C style diagnostics");
  P ("   gnudiagnostics            Give GNU style diagnostics");
  P ("   vmsdiagnostics            Give VMS style diagnostics");
  P ("   pedantic                  Equivalent to WARNINGS PORTCHECK");
  P ("   portcheck, noportcheck    Switch portability warnings on or off");
  P ("   pragmats, nopragmats      Switch elaboration of pragmat items on or off");
  P ("   reductions                Print parser reductions");
  P ("   verbose                   Inform on program actions");
  P ("   version                   State version of the running copy");
  P ("   warnings, nowarnings      Switch warning diagnostics on or off");
#undef P
}

/*!
\brief main
\param argc
\param argv
\return
**/

int main (int argc, char *argv[])
{
  BYTE_T stack_offset;
  int argcc;
#ifdef HAVE_TERMINFO
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
#ifdef HAVE_POSIX_THREADS
  main_thread_id = pthread_self ();
#endif
  global_argc = argc;
  global_argv = argv;
  system_stack_offset = &stack_offset;
  if (!setjmp (exit_compilation)) {
    init_tty ();
/* Initialise option handling. */
    init_options (&a68_prog);
    source_scan = 1;
    default_options (&a68_prog);
    default_mem_sizes ();
/* Inititialise core. */
    frame_segment = NULL;
    stack_segment = NULL;
    heap_segment = NULL;
    handle_segment = NULL;
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
    if (!set_options (&a68_prog, a68_prog.options.list, A_TRUE)) {
      a68g_exit (EXIT_FAILURE);
    }
/* Attention for -version. */
    if (a68_prog.options.version) {
      state_version (STDOUT_FILENO);
    }
/* We translate the program. */
    if (a68_prog.files.generic_name == NULL || strlen (a68_prog.files.generic_name) == 0) {
      SCAN_ERROR (!a68_prog.options.version, NULL, NULL, ERROR_NO_INPUT_FILE);
    } else {
      get_stack_size ();
      compiler_interpreter ();
    }
    a68g_exit (error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    return (EXIT_SUCCESS);
  } else {
    a68g_exit (EXIT_FAILURE);
    return (EXIT_FAILURE);
  }
}

/*!
\brief try opening with a silent extension
\param ext
\return
**/

static void whether_extension (char *ext)
{
  if (a68_prog.files.source.fd == -1) {
    char *fn2 = (char *) get_heap_space (strlen (a68_prog.files.source.name) + strlen (ext) + 1);
    bufcpy (fn2, a68_prog.files.source.name, BUFFER_SIZE);
    bufcat (fn2, ext, BUFFER_SIZE);
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
  error_count = warning_count = run_time_error_count = 0;
  RESET_ERRNO;
}

/*!
\brief drives compilation and interpretation
**/

static void compiler_interpreter (void)
{
  int k;
  BOOL_T path_set = A_FALSE;
  tree_listing_safe = A_FALSE;
  cross_reference_safe = A_FALSE;
  moid_listing_safe = A_FALSE;
  old_postulate = NULL;
  error_tag = (TAG_T *) new_tag;
/* File set-up. */
  SCAN_ERROR (a68_prog.files.generic_name == NULL, NULL, NULL, ERROR_NO_INPUT_FILE);
  a68_prog.files.source.name = new_string (a68_prog.files.generic_name);
  a68_prog.files.source.opened = A_FALSE;
  a68_prog.files.listing.opened = A_FALSE;
  a68_prog.files.source.writemood = A_FALSE;
  a68_prog.files.listing.writemood = A_TRUE;
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
  path_set = A_FALSE;
  for (k = strlen (a68_prog.files.path); k >= 0 && path_set == A_FALSE; k--) {
#ifdef WIN32_VERSION
    char delim = '\\';
#else
#ifdef VINTAGE_MAC_VERSION
    char delim = ':';
#else
    char delim = '/';
#endif
#endif
    if (a68_prog.files.path[k] == delim) {
      a68_prog.files.path[k + 1] = NULL_CHAR;
      path_set = A_TRUE;
    }
  }
  if (path_set == A_FALSE) {
    a68_prog.files.path[0] = NULL_CHAR;
  }
/* Listing file. */
  a68_prog.files.listing.name = (char *) get_heap_space (1 + strlen (a68_prog.files.source.name) + strlen (LISTING_EXTENSION));
  bufcpy (a68_prog.files.listing.name, a68_prog.files.source.name, BUFFER_SIZE);
  bufcat (a68_prog.files.listing.name, LISTING_EXTENSION, BUFFER_SIZE);
/* Tokeniser. */
  if (!setjmp (exit_compilation)) {
    a68_prog.files.source.opened = A_TRUE;
    announce_phase ("initialiser");
    init_before_tokeniser ();
    if (error_count == 0) {
      int frame_stack_size_2 = frame_stack_size;
      int expr_stack_size_2 = expr_stack_size;
      int heap_size_2 = heap_size;
      int handle_pool_size_2 = handle_pool_size;
      BOOL_T ok;
      announce_phase ("tokeniser");
      ok = lexical_analyzer (&a68_prog);
      if (!ok || errno != 0) {
        diagnostics_to_terminal (a68_prog.top_line, A_ALL_DIAGNOSTICS);
        return;
      }
/* Maybe the program asks for more memory through a PRAGMAT. We restart. */
      if (frame_stack_size_2 != frame_stack_size || expr_stack_size_2 != expr_stack_size || heap_size_2 != heap_size || handle_pool_size_2 != handle_pool_size) {
        discard_heap ();
        init_before_tokeniser ();
        source_scan++;
        ok = lexical_analyzer (&a68_prog);
      }
      if (!ok || errno != 0) {
        diagnostics_to_terminal (a68_prog.top_line, A_ALL_DIAGNOSTICS);
        return;
      }
      close (a68_prog.files.source.fd);
      a68_prog.files.source.opened = A_FALSE;
      prune_echoes (&a68_prog, a68_prog.options.list);
      tree_listing_safe = A_TRUE;
    }
/* Final initialisations. */
    if (error_count == 0) {
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
    }
/* Refinement preprocessor. */
    if (error_count == 0) {
      announce_phase ("preprocessor");
      get_refinements (&a68_prog);
      if (error_count == 0) {
        put_refinements (&a68_prog);
      }
    }
/* Top-down parser. */
    if (error_count == 0) {
      announce_phase ("parser phase 1");
      check_parenthesis (a68_prog.top_node);
      if (error_count == 0) {
        if (a68_prog.options.brackets) {
          substitute_brackets (a68_prog.top_node);
        }
        symbol_table_count = 0;
        stand_env = new_symbol_table (NULL);
        stand_env->level = 0;
        top_down_parser (a68_prog.top_node);
      }
    }
/* Standard environment builder. */
    if (error_count == 0) {
      announce_phase ("standard environ builder");
      SYMBOL_TABLE (a68_prog.top_node) = new_symbol_table (stand_env);
      make_standard_environ ();
    }
/* Bottom-up parser. */
    if (error_count == 0) {
      announce_phase ("parser phase 2");
      preliminary_symbol_table_setup (a68_prog.top_node);
      bottom_up_parser (a68_prog.top_node);
    }
    if (error_count == 0) {
      announce_phase ("parser phase 3");
      bottom_up_error_check (a68_prog.top_node);
      victal_checker (a68_prog.top_node);
      if (error_count == 0) {
        finalise_symbol_table_setup (a68_prog.top_node, 2);
        SYMBOL_TABLE (a68_prog.top_node)->nest = symbol_table_count = 3;
        reset_symbol_table_nest_count (a68_prog.top_node);
        set_par_level (a68_prog.top_node, 0);
        set_nests (a68_prog.top_node, NULL);
      }
    }
/* Mode table builder. */
    if (error_count == 0) {
      announce_phase ("mode table builder");
      set_up_mode_table (a68_prog.top_node);
    }
/* Symbol table builder. */
    if (error_count == 0) {
      moid_listing_safe = A_TRUE;
      announce_phase ("symbol table builder");
      collect_taxes (a68_prog.top_node);
    }
/* Post parser. */
    if (error_count == 0) {
      announce_phase ("parser phase 4");
      rearrange_goto_less_jumps (a68_prog.top_node);
    }
/* Mode checker. */
    if (error_count == 0) {
      cross_reference_safe = A_TRUE;
      announce_phase ("mode checker");
      mode_checker (a68_prog.top_node);
      maintain_mode_table (a68_prog.top_node);
    }
/* Coercion inserter. */
    if (error_count == 0) {
      announce_phase ("coercion enforcer");
      coercion_inserter (a68_prog.top_node);
      widen_denoter (a68_prog.top_node);
      widen_denoter (a68_prog.top_node);
      protect_from_sweep (a68_prog.top_node);
      reset_max_simplout_size ();
      get_max_simplout_size (a68_prog.top_node);
      reset_moid_list ();
      get_moid_list (&top_moid_list, a68_prog.top_node);
      set_moid_sizes (top_moid_list);
      assign_offsets_table (stand_env);
      assign_offsets (a68_prog.top_node);
      assign_offsets_packs (top_moid_list);
    }
/* Application checker. */
    if (error_count == 0) {
      announce_phase ("application checker");
      mark_moids (a68_prog.top_node);
      mark_auxilliary (a68_prog.top_node);
      jumps_from_procs (a68_prog.top_node);
      warn_for_unused_tags (a68_prog.top_node);
    }
/* Scope checker. */
    if (error_count == 0) {
      announce_phase ("static scope checker");
      tie_label_to_serial (a68_prog.top_node);
      tie_label_to_unit (a68_prog.top_node);
      bind_routine_tags_to_tree (a68_prog.top_node);
      bind_format_tags_to_tree (a68_prog.top_node);
      scope_checker (a68_prog.top_node);
    }
/* Portability checker. */
    if (error_count == 0) {
      announce_phase ("portability checker");
      portcheck (a68_prog.top_node);
    }
  }
  /*
   * if (!setjmp (exit_compilation)) 
   */
  /*
   * Interpreter. 
   */
  diagnostics_to_terminal (a68_prog.top_line, A_ALL_DIAGNOSTICS);
  if (error_count == 0 && (a68_prog.options.check_only ? a68_prog.options.run : A_TRUE)) {
    announce_phase ("genie");
    genie (&a68_prog);
  }
/* Setting up listing file. */
  if (a68_prog.options.moid_listing || a68_prog.options.tree_listing || a68_prog.options.source_listing || a68_prog.options.statistics_listing) {
    a68_prog.files.listing.fd = open (a68_prog.files.listing.name, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
    ABNORMAL_END (a68_prog.files.listing.fd == -1, "cannot open listing file", NULL);
    a68_prog.files.listing.opened = A_TRUE;
  } else {
    a68_prog.files.listing.opened = A_FALSE;
  }
/* Write listing. */
  if (a68_prog.files.listing.opened) {
    state_version (a68_prog.files.listing.fd);
    io_write_string (a68_prog.files.listing.fd, "\n++++ File \"");
    io_write_string (a68_prog.files.listing.fd, a68_prog.files.source.name);
    io_write_string (a68_prog.files.listing.fd, "\"");
    io_write_string (a68_prog.files.listing.fd, "\n++++ Source listing");
    source_listing (&a68_prog);
    write_listing (&a68_prog);
    close (a68_prog.files.listing.fd);
    a68_prog.files.listing.opened = A_FALSE;
  }
}

/*!
\brief exit a68g in an orderly manner
\param code
**/

void a68g_exit (int code)
{
  char name[BUFFER_SIZE];
  bufcpy (name, ".", BUFFER_SIZE);
  bufcat (name, A68G_NAME, BUFFER_SIZE);
  bufcat (name, ".x", BUFFER_SIZE);
  remove (name);
  io_close_tty_line ();
#ifdef HAVE_CURSES
/* "curses" might still be open if it was not closed from A68, or the program
   was interrupted, or a runtime error occured. That wreaks havoc on your
   terminal. */
  genie_curses_end (NULL);
#endif
  exit (code);
}

/*!
\brief start bookkeeping for a phase
\param t
\return
**/

static void announce_phase (char *t)
{
  if (a68_prog.options.verbose) {
    snprintf (output_line, BUFFER_SIZE, "%s: %s", A68G_NAME, t);
    io_close_tty_line ();
    io_write_string (STDOUT_FILENO, output_line);
  }
}
