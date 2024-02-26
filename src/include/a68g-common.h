//! @file a68g-common.h
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

#if !defined (__A68G_COMMON_H__)
#define __A68G_COMMON_H__

typedef struct MODULE_T MODULE_T;
struct MODULE_T
{
  BOOL_T tree_listing_safe, cross_reference_safe;
  FILES_T files;
  int error_count, warning_count, source_scan;
  jmp_buf rendez_vous;
  LINE_T *top_line;
  MOID_T *top_moid, *standenv_moid;
  NODE_T *top_node;
  OPTIONS_T options;
  PROP_T global_prop;
  REFINEMENT_T *top_refinement;
  struct
  {
    LINE_T *save_l;
    char *save_s, save_c;
  } scan_state;
};

typedef struct MODE_CACHE_T MODE_CACHE_T;
#define MODE(p)        A68 (a68_modes.p)
struct MODE_CACHE_T
{
  MOID_T *proc_bool;
  MOID_T *proc_char;
  MOID_T *proc_complex_complex;
  MOID_T *proc_int;
  MOID_T *proc_int_int;
  MOID_T *proc_int_int_real;
  MOID_T *proc_int_real;
  MOID_T *proc_int_real_real;
  MOID_T *proc_int_real_real_real;
  MOID_T *proc_real;
  MOID_T *proc_real_int_real;
  MOID_T *proc_real_real;
  MOID_T *proc_real_real_int_real;
  MOID_T *proc_real_real_real;
  MOID_T *proc_real_real_real_int;
  MOID_T *proc_real_real_real_real;
  MOID_T *proc_real_real_real_real_real;
  MOID_T *proc_real_real_real_real_real_real;
  MOID_T *proc_real_ref_real_ref_int_void;
  MOID_T *proc_void;
};

#define MAX_OPEN_FILES 64       // Some OS's won't open more than this number
#define MAX_TRANSPUT_BUFFER (MAX_OPEN_FILES)

typedef struct FILE_ENTRY FILE_ENTRY;
struct FILE_ENTRY
{
  NODE_T *pos;
  BOOL_T is_open, is_tmp;
  FILE_T fd;
  A68_REF idf;
};

// Administration for common (sub) expression elimination.
// BOOK keeps track of already seen (temporary) variables and denotations.

enum
{ BOOK_NONE = 0, BOOK_DECL, BOOK_INIT, BOOK_DEREF, BOOK_ARRAY, BOOK_COMPILE };

typedef struct BOOK_T BOOK_T;
struct BOOK_T
{
  int action, phase;
  char *idf;
  void *info;
  int number;
};

typedef struct UNIC_T UNIC_T;
struct UNIC_T
{
  char *fun;
};

//

#define A68(z)         (common.z)
#define A68_JOB        A68 (job)
#define A68_STANDENV   A68 (standenv)
#define A68_MCACHE(z)  A68 (mode_cache.z)

#define A68_SP         A68 (stack_pointer)
#define A68_FP         A68 (frame_pointer)
#define A68_HP         A68 (heap_pointer)
#define A68_GLOBALS    A68 (global_pointer)
#define A68_STACK      A68 (stack_segment)
#define A68_HEAP       A68 (heap_segment)
#define A68_HANDLES    A68 (handle_segment)

typedef struct GC_GLOBALS_T GC_GLOBALS_T;
#define A68_GC(z)      A68 (gc.z)
struct GC_GLOBALS_T
{
  A68_HANDLE *available_handles, *busy_handles;
  UNSIGNED_T free_handles, max_handles, sweeps, refused, freed, total;
  unt preemptive;
  REAL_T seconds;
};

typedef struct INDENT_GLOBALS_T INDENT_GLOBALS_T;
#define A68_INDENT(z)  A68 (indent.z)
struct INDENT_GLOBALS_T
{
  FILE_T fd;
  int ind, col;
  int indentation;
  BOOL_T use_folder;
};

#define MON_STACK_SIZE 32

typedef struct MONITOR_GLOBALS_T MONITOR_GLOBALS_T;
#define A68_MON(z)     A68 (mon.z)
struct MONITOR_GLOBALS_T
{
  ADDR_T finish_frame_pointer;
  BOOL_T in_monitor;
  BOOL_T prompt_set;
  BUFFER prompt, symbol, error_text, expr;
  char *watchpoint_expression;
  int break_proc_level;
  int current_frame;
  int max_row_elems;
  int mon_errors;
  int _m_sp;
  int pos, attr;
  int tabs;
  MOID_T *_m_stack[MON_STACK_SIZE];
};

typedef struct MP_GLOBALS_T MP_GLOBALS_T;
#define A68_MP(z)      A68 (mp.z)

struct MP_GLOBALS_T
{
  int mp_gamma_size;
  int mp_ln_10_size;
  int mp_ln_scale_size;
  int mp_one_size;
  int mp_pi_size;
  int varying_mp_digits;
  MP_T *mp_180_over_pi;
  MP_T **mp_gam_ck;
  MP_T *mp_half_pi;
  MP_T *mp_ln_10;
  MP_T *mp_ln_pi;
  MP_T *mp_ln_scale;
  MP_T *mp_one;
  MP_T *mp_pi;
  MP_T *mp_pi_over_180;
  MP_T *mp_sqrt_pi;
  MP_T *mp_sqrt_two_pi;
  MP_T *mp_two_pi;
};

#define MAX_BOOK 1024
#define MAX_UNIC 2048

typedef struct OPTIMISER_GLOBALS_T OPTIMISER_GLOBALS_T;
#define A68_OPT(z)     A68 (optimiser.z)
struct OPTIMISER_GLOBALS_T
{
  BOOK_T cse_book[MAX_BOOK];
  BOOL_T put_idf_comma;
  DEC_T *root_idf;
  int code_errors;
  int cse_pointer;
  int indentation;
  int OPTION_CODE_LEVEL;
  int procedures;
  int unic_pointer;
  UNIC_T unic_functions[MAX_UNIC];
};

#if defined (BUILD_PARALLEL_CLAUSE)

typedef struct A68_STACK_DESCRIPTOR A68_STACK_DESCRIPTOR;
typedef struct A68_THREAD_CONTEXT A68_THREAD_CONTEXT;

struct A68_STACK_DESCRIPTOR
{
  ADDR_T cur_ptr, ini_ptr;
  BYTE_T *swap, *start;
  int bytes;
};

struct A68_THREAD_CONTEXT
{
  A68_STACK_DESCRIPTOR stack, frame;
  BOOL_T active;
  BYTE_T *thread_stack_offset;
  int stack_used;
  NODE_T *unit;
  pthread_t parent, id;
};

// Set an upper limit for number of threads.
// Note that _POSIX_THREAD_THREADS_MAX may be ULONG_MAX.

#define THREAD_LIMIT   256

#if (_POSIX_THREAD_THREADS_MAX < THREAD_LIMIT)
#undef  THREAD_LIMIT
#define THREAD_LIMIT   _POSIX_THREAD_THREADS_MAX
#endif

#if !defined _POSIX_THREAD_THREADS_MAX
#define _POSIX_THREAD_THREADS_MAX	(THREAD_LIMIT)
#endif

#if (_POSIX_THREAD_THREADS_MAX < THREAD_LIMIT)
#define THREAD_MAX     (_POSIX_THREAD_THREADS_MAX)
#else
#define THREAD_MAX     (THREAD_LIMIT)
#endif

typedef struct PARALLEL_GLOBALS_T PARALLEL_GLOBALS_T;
#define A68_PAR(z)     A68 (parallel.z)
struct PARALLEL_GLOBALS_T
{
  A68_THREAD_CONTEXT context[THREAD_MAX];
  ADDR_T fp0, sp0;
  BOOL_T abend_all_threads, exit_from_threads;
  int context_index;
  int par_return_code;
  jmp_buf *jump_buffer;
  NODE_T *jump_label;
  pthread_mutex_t unit_sema;
  pthread_t main_thread_id;
  pthread_t parent_thread_id;
};

#endif

typedef struct PARSER_GLOBALS_T PARSER_GLOBALS_T;
#define A68_PARSER(z)  A68 (parser.z)
struct PARSER_GLOBALS_T
{
  TAG_T *error_tag;
  BOOL_T stop_scanner, read_error, no_preprocessing;
  char *scan_buf;
  int max_scan_buf_length, source_file_size;
  int reductions;
  int tag_number;
  jmp_buf bottom_up_crash_exit, top_down_crash_exit;
};

typedef struct GLOBALS_T GLOBALS_T;

struct GLOBALS_T
{
  A68_CHANNEL stand_draw_channel, stand_error_channel, associate_channel, skip_channel;
  A68_CHANNEL stand_in_channel, stand_out_channel, stand_back_channel;
  A68_PROCEDURE on_gc_event;
  A68_REF stand_in, stand_out, stand_back, stand_error, skip_file;
  ADDR_T fixed_heap_pointer, temp_heap_pointer;
  ADDR_T frame_pointer, stack_pointer, heap_pointer, global_pointer;
  ADDR_T frame_start, frame_end, stack_start, stack_end;
  BOOL_T close_tty_on_exit;
  BOOL_T curses_mode;
  BOOL_T do_confirm_exit; 
  BOOL_T halt_typing;
  BOOL_T heap_is_fluid;
  BOOL_T in_execution;
  BOOL_T in_monitor;
  BOOL_T no_warnings;
  BYTE_T *stack_segment, *heap_segment, *handle_segment;
  BYTE_T *system_stack_offset;
  BUFFER a68_cmd_name;
  char **argv;
  char *f_plugin;
  char *marker[BUFFER_SIZE];
  BUFFER output_line, edit_line, input_line;
  clock_t clock_res;
  FILE_ENTRY file_entries[MAX_OPEN_FILES];
  GC_GLOBALS_T gc;
  INDENT_GLOBALS_T indent;
  int argc;
  int chars_in_tty_line;
  int global_level, max_lex_lvl;
  int max_simplout_size;
  int new_nodes, new_modes, new_postulates, new_node_infos, new_genie_infos;
  int ret_code, ret_line_number, ret_char_number;
  int symbol_table_count, mode_count; 
  int term_heigth, term_width;
  jmp_buf genie_exit_label;
  KEYWORD_T *top_keyword;
  MODE_CACHE_T mode_cache;
  MODES_T a68_modes;
  MODULE_T job;
  MONITOR_GLOBALS_T mon;
  MP_GLOBALS_T mp;
  NODE_T *f_entry;
  NODE_T **node_register;
  OPTIMISER_GLOBALS_T optimiser;
  OPTIONS_T *options;
  PARSER_GLOBALS_T parser;
  POSTULATE_T *postulates, *top_postulate, *top_postulate_list;
  REAL_T cputime_0;
  SOID_T *top_soid_list;
  TABLE_T *standenv;
  TAG_T *error_tag;
  TOKEN_T *top_token;
  unt frame_stack_size, expr_stack_size, heap_size, handle_pool_size, stack_size;
  unt stack_limit, frame_stack_limit, expr_stack_limit;
  unt storage_overhead;
#if defined (BUILD_PARALLEL_CLAUSE)
  PARALLEL_GLOBALS_T parallel;
#endif
};

extern GLOBALS_T common;

#endif
