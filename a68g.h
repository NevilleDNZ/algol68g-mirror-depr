/*!
\file algol68g.h
\brief general definitions for Algol 68 Genie
**/

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

#if ! defined A68G_ALGOL68G_H
#define A68G_ALGOL68G_H

/* Includes needed by most files */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#if defined HAVE_STRING_H
#include <string.h>
#elif defined HAVE_STRINGS_H
#include <strings.h>
#endif

#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

/* System dependencies */

#if defined HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
#include <pthread.h>
#endif

#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES)
#include <curses.h>
#endif

#if (defined HAVE_LIBPQ_FE_H && defined HAVE_LIBPQ)
#include <libpq-fe.h>
#endif

#if (defined HAVE_PLOT_H && defined HAVE_LIBPLOT)
#include <plot.h>
#endif

/* Type definitions */

typedef int ADDR_T, FILE_T, LEAP_T;
typedef unsigned char BYTE_T;
typedef int BOOL_T;
typedef unsigned STATUS_MASK;

#if defined HAVE_MACOS_X
#define __off_t off_t
#define __pid_t pid_t
#define __mode_t mode_t
#endif

#if defined HAVE_FREEBSD
#define __off_t off_t
#define __pid_t pid_t
#define __mode_t mode_t
#endif

#if ! defined O_BINARY
#define O_BINARY 0x0000
#endif

/* Constants */

#define KILOBYTE ((int) 1024)
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)

#define HIDDEN_TEMP_FILE_NAME ".a68g.tmp"
#define LISTING_EXTENSION ".l"
#define LIBRARY_EXTENSION ".so"
#define OBJECT_EXTENSION ".c"
#define SCRIPT_EXTENSION ".sh"
#define BINARY_EXTENSION ".o"
#define A68_TRUE ((BOOL_T) 1)
#define A68_FALSE ((BOOL_T) 0)
#define TIME_FORMAT "%A %d-%b-%Y %H:%M:%S"

typedef int *A68_ALIGN_T;
#define A68_ALIGNMENT ((int) (sizeof (A68_ALIGN_T)))
#define A68_ALIGN(s) ((int) ((s) % A68_ALIGNMENT) == 0 ? (s) : ((s) - (s) % A68_ALIGNMENT + A68_ALIGNMENT))
#define A68_ALIGN_8(s) ((int) ((s) % 8) == 0 ? (s) : ((s) - (s) % 8 + 8))
#define ALIGNED_SIZE_OF(p) ((int) A68_ALIGN (sizeof (p)))

#define MOID_SIZE(p) A68_ALIGN ((p)->size)

/* BUFFER_SIZE exceeds actual requirements */
#define BUFFER_SIZE (KILOBYTE)
#define SNPRINTF_SIZE ((size_t) BUFFER_SIZE)
#define SMALL_BUFFER_SIZE 128
#define MAX_ERRORS 10
/* MAX_PRIORITY follows from the revised report */
#define MAX_PRIORITY 9
/* Stack, heap blocks not smaller than MIN_MEM_SIZE */
#define MIN_MEM_SIZE (128 * KILOBYTE)
/* MAX_LINE_WIDTH must be smaller than BUFFER_SIZE */
#define MAX_LINE_WIDTH (BUFFER_SIZE / 2)
#define MOID_WIDTH 80

/* This BYTES_WIDTH is more useful than the usual 4 */
#define BYTES_WIDTH 32
/* This LONG_BYTES_WIDTH is more useful than the usual 8 */
#define LONG_BYTES_WIDTH 256

#define A68_READ_ACCESS (O_RDONLY)
#define A68_WRITE_ACCESS (O_WRONLY | O_CREAT | O_TRUNC)
#define A68_PROTECTION (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) /* -rw-r--r-- */

#define A68_MAX_INT (INT_MAX)
#define A68_MIN_INT (INT_MIN)
#define A68_MAX_UNT (UINT_MAX)
#define A68_MAX_BITS (UINT_MAX)
#define A68_PI 3.1415926535897932384626433832795029

#define TO_UCHAR(c) ((c) >= 0 ? (int) (c) : (int) (UCHAR_MAX + (int) (c) + 1))

#define BLANK_CHAR ' '
#define CR_CHAR '\r'
#define EOF_CHAR (EOF)
#define ERROR_CHAR '*'
#define ESCAPE_CHAR '\\'
#define EXPONENT_CHAR 'e'
#define FLIP_CHAR 'T'
#define FLOP_CHAR 'F'
#define FORMFEED_CHAR '\f'
#define NEWLINE_CHAR '\n'
#define NEWLINE_STRING "\n"
#define NULL_CHAR	'\0'
#define POINT_CHAR '.'
#define QUOTE_CHAR '"'
#define RADIX_CHAR 'r'
#define TAB_CHAR '\t'

#define MONADS "%^&+-~!?"
#define NOMADS "></=*"

#define PRIMAL_SCOPE 0

/* Status Masks */

#define NULL_MASK ((STATUS_MASK) 0x00000000)
#define IN_HEAP_MASK ((STATUS_MASK) 0x00000001)
#define IN_FRAME_MASK ((STATUS_MASK) 0x00000002)
#define IN_STACK_MASK ((STATUS_MASK) 0x00000004)
#define IN_HANDLE_MASK ((STATUS_MASK) 0x00000008)
#define INITIALISED_MASK ((STATUS_MASK) 0x00000010)
#define CONSTANT_MASK ((STATUS_MASK) 0x00000020)
#define BLOCK_GC_MASK ((STATUS_MASK) 0x00000040)
#define ROW_COLOUR_MASK ((STATUS_MASK) 0x00000080)
#define COOKIE_MASK ((STATUS_MASK) 0x00000100)
#define SCOPE_ERROR_MASK ((STATUS_MASK) 0x00000100)
#define ASSIGNED_MASK ((STATUS_MASK) 0x00000200)
#define ALLOCATED_MASK ((STATUS_MASK) 0x00000400)
#define STANDENV_PROC_MASK ((STATUS_MASK) 0x00000800)
#define COLOUR_MASK ((STATUS_MASK) 0x00001000)
#define PROCEDURE_MASK ((STATUS_MASK) 0x00002000)
#define OPTIMAL_MASK ((STATUS_MASK) 0x00004000)
#define SERIAL_MASK ((STATUS_MASK) 0x00008000)
#define CROSS_REFERENCE_MASK ((STATUS_MASK) 0x00010000)
#define TREE_MASK ((STATUS_MASK) 0x00020000)
#define CODE_MASK ((STATUS_MASK) 0x00040000)
#define SYNTAX_TREE_MASK ((STATUS_MASK) 0x00080000)
#define SOURCE_MASK ((STATUS_MASK) 0x00100000)
#define ASSERT_MASK ((STATUS_MASK) 0x00200000)
#define NIL_MASK ((STATUS_MASK) 0x00400000)
#define SKIP_PROCEDURE_MASK ((STATUS_MASK) 0x00800000)
#define SKIP_FORMAT_MASK ((STATUS_MASK) 0x00800000)
#define SKIP_UNION_MASK	0x00800000)
#define INTERRUPTIBLE_MASK ((STATUS_MASK) 0x01000000)
#define BREAKPOINT_MASK ((STATUS_MASK) 0x02000000)
#define BREAKPOINT_TEMPORARY_MASK ((STATUS_MASK) 0x04000000)
#define BREAKPOINT_INTERRUPT_MASK ((STATUS_MASK) 0x08000000)
#define BREAKPOINT_WATCH_MASK ((STATUS_MASK) 0x10000000)
#define BREAKPOINT_TRACE_MASK ((STATUS_MASK) 0x20000000)
#define SEQUENCE_MASK ((STATUS_MASK) 0x40000000)
#define BREAKPOINT_ERROR_MASK ((STATUS_MASK) 0xffffffff)

/* CODEX masks */

#define PROC_DECLARATION_MASK ((STATUS_MASK) 0x00000001)

/*
Some (necessary) macros to overcome the ambiguity in having signed or unsigned
char on various systems. PDP-11s and IBM 370s are still haunting us with this.
*/

#define IS_ALNUM(c) isalnum ((unsigned char) (c))
#define IS_ALPHA(c) isalpha ((unsigned char) (c))
#define IS_CNTRL(c) iscntrl ((unsigned char) (c))
#define IS_DIGIT(c) isdigit ((unsigned char) (c))
#define IS_GRAPH(c) isgraph ((unsigned char) (c))
#define IS_LOWER(c) islower ((unsigned char) (c))
#define IS_PRINT(c) isprint ((unsigned char) (c))
#define IS_PUNCT(c) ispunct ((unsigned char) (c))
#define IS_SPACE(c) isspace ((unsigned char) (c))
#define IS_UPPER(c) isupper ((unsigned char) (c))
#define IS_XDIGIT(c) isxdigit ((unsigned char) (c))
#define TO_LOWER(c) tolower ((unsigned char) (c))
#define TO_UPPER(c) toupper ((unsigned char) (c))

/* Type definitions */

typedef struct A68_ARRAY A68_ARRAY;
typedef struct A68_BITS A68_BITS;
typedef struct A68_BOOL A68_BOOL;
typedef struct A68_BYTES A68_BYTES;
typedef struct A68_CHANNEL A68_CHANNEL;
typedef struct A68_CHAR A68_CHAR;
typedef struct A68_COLLITEM A68_COLLITEM;
typedef struct A68_FILE A68_FILE;
typedef struct A68_FORMAT A68_FORMAT;
typedef struct A68_HANDLE A68_HANDLE;
typedef struct A68_INT A68_INT;
typedef struct A68_LONG_BYTES A68_LONG_BYTES;
typedef struct A68_UNION A68_UNION;
typedef struct A68_PROCEDURE A68_PROCEDURE;
typedef struct A68_REAL A68_REAL;
typedef struct A68_TUPLE A68_TUPLE;
typedef struct A68_SOUND A68_SOUND;

typedef struct DIAGNOSTIC_T DIAGNOSTIC_T;
typedef struct FILES_T FILES_T;
typedef struct GENIE_INFO_T GENIE_INFO_T;
typedef struct KEYWORD_T KEYWORD_T;
typedef struct MODES_T MODES_T;
typedef struct MODULE_T MODULE_T;
typedef struct MOID_LIST_T MOID_LIST_T;
typedef struct MOID_T MOID_T;
typedef struct NODE_INFO_T NODE_INFO_T;
typedef struct NODE_T NODE_T;
typedef struct OPTION_LIST_T OPTION_LIST_T;
typedef struct OPTIONS_T OPTIONS_T;
typedef struct PACK_T PACK_T;
typedef struct POSTULATE_T POSTULATE_T;
typedef struct PRELUDE_T PRELUDE_T;
typedef struct PROPAGATOR_T PROPAGATOR_T;
typedef struct REFINEMENT_T REFINEMENT_T;
typedef struct SOID_LIST_T SOID_LIST_T;
typedef struct SOID_T SOID_T;
typedef struct SOURCE_LINE_T SOURCE_LINE_T;
typedef struct SYMBOL_TABLE_T SYMBOL_TABLE_T;
typedef struct TAG_T TAG_T;
typedef struct TOKEN_T TOKEN_T;

typedef PROPAGATOR_T PROPAGATOR_PROCEDURE (NODE_T *);
typedef void GENIE_PROCEDURE (NODE_T *);

/* Macro's for fat A68 pointers */

#define REF_SCOPE(z) (((z)->u).scope)
#define REF_HANDLE(z) (((z)->u).handle)
#define REF_POINTER(z) (REF_HANDLE (z)->pointer)
#define REF_OFFSET(z) (OFFSET (z))

#define IS_IN_HEAP(z) (STATUS (z) & IN_HEAP_MASK)
#define IS_IN_FRAME(z) (STATUS (z) & IN_FRAME_MASK)
#define IS_IN_STACK(z) (STATUS (z) & IN_STACK_MASK)
#define IS_IN_HANDLE(z) (STATUS (z) & IN_HANDLE_MASK)
#define IS_NIL(p) ((BOOL_T) ((STATUS (&(p)) & NIL_MASK) != 0))
#define GET_REF_SCOPE(z) (IS_IN_HEAP (z) ? PRIMAL_SCOPE : REF_SCOPE (z))
#define SET_REF_SCOPE(z, s) { if (!IS_IN_HEAP (z)) { REF_SCOPE (z) = (s);}}

typedef struct A68_REF A68_REF, A68_ROW;
typedef BYTE_T * A68_STRUCT;

struct A68_HANDLE
{
  STATUS_MASK status;
  BYTE_T *pointer;
  int size;
  MOID_T *type;
  A68_HANDLE *next, *previous;
};

struct A68_REF
{
  STATUS_MASK status;
  ADDR_T offset;
  union {A68_HANDLE *handle; ADDR_T scope;} u;
};

/* Options struct */

struct OPTIONS_T
{
  OPTION_LIST_T *list;
  BOOL_T backtrace, brackets, check_only, clock, cross_reference, debug, compile, keep, local, mips, moid_listing, 
    object_listing, optimise, portcheck, pragmat_sema, reductions, regression_test, run, rerun,
    run_script, source_listing, standard_prelude_listing, statistics_listing, 
    strict, stropping, trace, tree_listing, unused, verbose, version; 
  int time_limit, opt_level; 
  STATUS_MASK nodemask;
};

/* Propagator type, that holds information for the interpreter */

struct PROPAGATOR_T
{
  PROPAGATOR_PROCEDURE *unit;
  NODE_T *source;
};

/* Algol 68 type definitions */

struct A68_ARRAY
{
  MOID_T *type;
  int dim, elem_size;
  ADDR_T slice_offset, field_offset;
  A68_REF array;
};

struct A68_BITS
{
  STATUS_MASK status;
  unsigned value;
};

struct A68_BYTES
{
  STATUS_MASK status;
  char value[BYTES_WIDTH + 1];
};

struct A68_CHANNEL
{
  STATUS_MASK status;
  BOOL_T reset, set, get, put, bin, draw, compress;
};

struct A68_BOOL
{
  STATUS_MASK status;
  int value;
};

struct A68_CHAR
{
  BYTE_T status;
  int value;
};

struct A68_COLLITEM
{
  STATUS_MASK status;
  int count;
};

struct A68_INT
{
  STATUS_MASK status;
  int value;
};

struct A68_FORMAT
{
  STATUS_MASK status;
  NODE_T *body;
  ADDR_T environ;
};

struct A68_LONG_BYTES
{
  STATUS_MASK status;
  char value[LONG_BYTES_WIDTH + 1];
};

struct A68_PROCEDURE
{
  STATUS_MASK status;
  union {NODE_T *node; GENIE_PROCEDURE *proc;} body;
  A68_HANDLE *locale;
  MOID_T *type;
  ADDR_T environ;
};

struct A68_REAL
{
  STATUS_MASK status;
  double value;
};

typedef A68_REAL A68_COMPLEX[2];
#define STATUS_RE(z) (STATUS (&(z)[0]))
#define STATUS_IM(z) (STATUS (&(z)[1]))
#define RE(z) (VALUE (&(z)[0]))
#define IM(z) (VALUE (&(z)[1]))

struct A68_STREAM
{
  char *name;
  FILE_T fd;
  BOOL_T opened, writemood;
};

struct A68_TUPLE
{
  int upper_bound, lower_bound, shift, span, k;
};

struct A68_UNION
{
  STATUS_MASK status;
  void *value;
};

struct A68_SOUND
{
  STATUS_MASK status;
  unsigned num_channels, sample_rate, bits_per_sample, num_samples;
  A68_REF data;
};

/* The FILE mode */

struct A68_FILE
{
  STATUS_MASK status;
  A68_REF identification, terminator, string;
  A68_CHANNEL channel;
  FILE_T fd;
  int transput_buffer, strpos;
  A68_FORMAT format;
  BOOL_T read_mood, write_mood, char_mood, draw_mood, opened, open_exclusive, eof, tmp_file;
  A68_PROCEDURE file_end_mended, page_end_mended, line_end_mended, value_error_mended, open_error_mended, transput_error_mended, format_end_mended, format_error_mended;
  ADDR_T frame_pointer, stack_pointer;	/* Since formats open frames*/
/* Next is for GNU plot*/
  struct
  {
    FILE *stream;
#if (defined HAVE_PLOT_H && defined HAVE_LIBPLOT)
    plPlotter *plotter;
    plPlotterParams *plotter_params;
#endif
    BOOL_T device_made, device_opened;
    A68_REF device, page_size;
    int device_handle /* deprecated*/ , window_x_size, window_y_size;
    double x_coord, y_coord, red, green, blue;
  }
  device;
#if (defined HAVE_LIBPQ_FE_H && defined HAVE_LIBPQ)
  PGconn *connection;
  PGresult *result;
#endif
};

/* Internal type definitions */

struct DIAGNOSTIC_T
{
  int attribute, number;
  NODE_T *where;
  SOURCE_LINE_T *line;
  char *text, *symbol;
  DIAGNOSTIC_T *next;
};

struct FILES_T
{
  char *path, *initial_name, *generic_name;
  struct A68_STREAM binary, library, script, object, source, listing;
};

struct KEYWORD_T
{
  int attribute;
  char *text;
  KEYWORD_T *less, *more;
};

struct MODES_T
{
  MOID_T *BITS, *BOOL, *BYTES, *CHANNEL, *CHAR, *COLLITEM, *COMPL, *COMPLEX,
    *C_STRING, *ERROR, *FILE, *FORMAT, *HIP, *INT, *LONG_BITS, *LONG_BYTES,
    *LONG_COMPL, *LONG_COMPLEX, *LONG_INT, *LONGLONG_BITS, *LONGLONG_COMPL,
    *LONGLONG_COMPLEX, *LONGLONG_INT, *LONGLONG_REAL, *LONG_REAL, *NUMBER,
    *PIPE, *PROC_REAL_REAL, *PROC_REF_FILE_BOOL, *PROC_REF_FILE_VOID, *PROC_ROW_CHAR,
    *PROC_STRING, *PROC_VOID, *REAL, *REF_BITS, *REF_BOOL, *REF_BYTES,
    *REF_CHAR, *REF_COMPL, *REF_COMPLEX, *REF_FILE, *REF_FORMAT, *REF_INT,
    *REF_LONG_BITS, *REF_LONG_BYTES, *REF_LONG_COMPL, *REF_LONG_COMPLEX,
    *REF_LONG_INT, *REF_LONGLONG_BITS, *REF_LONGLONG_COMPL,
    *REF_LONGLONG_COMPLEX, *REF_LONGLONG_INT, *REF_LONGLONG_REAL,
    *REF_LONG_REAL, *REF_PIPE, *REF_REAL, *REF_REF_FILE, *REF_ROW_CHAR,
    *REF_ROW_COMPLEX, *REF_ROW_INT, *REF_ROW_REAL, *REF_ROWROW_COMPLEX,
    *REF_ROWROW_REAL, *REF_SOUND, *REF_STRING, *ROW_BITS, *ROW_BOOL, *ROW_CHAR,
    *ROW_COMPLEX, *ROW_INT, *ROW_LONG_BITS, *ROW_LONGLONG_BITS, *ROW_REAL,
    *ROW_ROW_CHAR, *ROWROW_COMPLEX, *ROWROW_REAL, *ROWS, *ROW_SIMPLIN,
    *ROW_SIMPLOUT, *ROW_STRING, *SEMA, *SIMPLIN, *SIMPLOUT, *SOUND, *SOUND_DATA,
    *STRING, *UNDEFINED, *VACUUM, *VOID;
};

struct MODULE_T
{
  FILES_T files;
  SOURCE_LINE_T *top_line;
  OPTIONS_T options;
  REFINEMENT_T *top_refinement;
  NODE_T *top_node;
  struct {
    SOURCE_LINE_T *save_l;
    char *save_s;
    char save_c;
  } scan_state;
  int error_count, warning_count;
  int source_scan;
  jmp_buf exit_compilation;
  BOOL_T tree_listing_safe, cross_reference_safe;
  PROPAGATOR_T global_prop;
};

struct MOID_T
{
  int attribute, number, dim, short_id, size;
  BOOL_T has_ref, has_flex, has_rows, in_standard_environ, well_formed, use, portable;
  NODE_T *node;
  PACK_T *pack;
  MOID_T *sub, *equivalent_mode, *slice, *deflexed_mode, *name, *multiple_mode, *trim, *next, *rowed;
};

struct MOID_LIST_T
{
  SYMBOL_TABLE_T *coming_from_level;
  MOID_T *type;
  MOID_LIST_T *next;
};

struct NODE_T
{
  GENIE_INFO_T *genie;
  int number, attribute, annotation, par_level;
  MOID_T *type;
  NODE_INFO_T *info;
  NODE_T *next, *previous, *sub, *sequence, *nest;
  PACK_T *pack;
  STATUS_MASK status, codex;
  SYMBOL_TABLE_T *symbol_table;
  TAG_T *tag;
};

struct NODE_INFO_T
{
  int PROCEDURE_LEVEL, priority;
  char *char_in_line, *symbol, *expr;
  SOURCE_LINE_T *line;
};

struct GENIE_INFO_T
{
  PROPAGATOR_T propagator;
  BOOL_T whether_coercion, whether_new_lexical_level, need_dns;
  BYTE_T *offset;
  MOID_T *partial_proc, *partial_locale;
  NODE_T *parent;
  TAG_T *block_ref;
  char *compile_name;
  int level, argsize, size, compile_node;
  void *constant;
};

struct OPTION_LIST_T
{
  char *str;
  int scan;
  BOOL_T processed;
  SOURCE_LINE_T *line;
  OPTION_LIST_T *next;
};

struct PACK_T
{
  MOID_T *type;
  char *text;
  NODE_T *node;
  PACK_T *next, *previous;
  int size;
  ADDR_T offset;
};

struct POSTULATE_T
{
  MOID_T *a, *b;
  POSTULATE_T *next;
};

struct PRELUDE_T
{
  char *file_name;
  PRELUDE_T *next;
};

struct REFINEMENT_T
{
  REFINEMENT_T *next;
  char *name;
  SOURCE_LINE_T *line_defined, *line_applied;
  int applications;
  NODE_T *node_defined, *begin, *end;
};

struct SOID_T
{
  int attribute, sort, cast;
  MOID_T *type;
};

struct SOID_LIST_T
{
  NODE_T *where;
  SOID_T *yield;
  SOID_LIST_T *next;
};

struct SOURCE_LINE_T
{
  char marker[6], *string, *filename;
  DIAGNOSTIC_T *diagnostics;
  int number, print_status;
  BOOL_T list;
  SOURCE_LINE_T *next, *previous;
};

struct SYMBOL_TABLE_T
{
  int level, nest, attribute /* MAIN, PRELUDE_T*/ ;
  BOOL_T empty_table, initialise_frame, initialise_anon, proc_ops;
  ADDR_T ap_increment;
  SYMBOL_TABLE_T *previous, *outer;
  TAG_T *identifiers, *operators, *priority, *indicants, *labels, *anonymous;
  MOID_T *moids;
  NODE_T *jump_to, *sequence;
};

struct TAG_T
{
  STATUS_MASK status, codex;
  SYMBOL_TABLE_T *symbol_table;
  MOID_T *type;
  NODE_T *node, *unit;
  char *value;
  GENIE_PROCEDURE *procedure;
  BOOL_T scope_assigned, use, in_proc, stand_env_proc, loc_assigned, portable;
  int priority, heap, scope, size, youngest_environ, number;
  ADDR_T offset;
  TAG_T *next, *body;
};

struct TOKEN_T
{
  char *text;
  TOKEN_T *less, *more;
};

/* Internal constants */

enum
{
  UPPER_STROPPING = 1, QUOTE_STROPPING
};

enum ATTRIBUTES
{
  NULL_ATTRIBUTE = 0,
  A68_PATTERN,
  ACCO_SYMBOL,
  ACTUAL_DECLARER_MARK,
  ALT_DO_PART,
  ALT_DO_SYMBOL,
  ALT_EQUALS_SYMBOL,
  ALT_FORMAL_BOUNDS_LIST,
  ANDF_SYMBOL,
  AND_FUNCTION,
  ANONYMOUS,
  ARGUMENT,
  ARGUMENT_LIST,
  ASSERTION,
  ASSERT_SYMBOL,
  ASSIGNATION,
  ASSIGN_SYMBOL,
  ASSIGN_TO_SYMBOL,
  AT_SYMBOL,
  BEGIN_SYMBOL,
  BITS_C_PATTERN,
  BITS_DENOTATION,
  BITS_PATTERN,
  BITS_SYMBOL,
  BOLD_COMMENT_SYMBOL,
  BOLD_PRAGMAT_SYMBOL,
  BOLD_TAG,
  BOOLEAN_PATTERN,
  BOOL_SYMBOL,
  BOUND,
  BOUNDS,
  BOUNDS_LIST,
  BRIEF_ELIF_PART,
  BRIEF_INTEGER_OUSE_PART,
  BRIEF_OPERATOR_DECLARATION,
  BRIEF_UNITED_OUSE_PART,
  BUS_SYMBOL,
  BY_PART,
  BY_SYMBOL,
  BYTES_SYMBOL,
  CALL,
  CASE_PART,
  CASE_SYMBOL,
  CAST,
  CHANNEL_SYMBOL,
  CHAR_C_PATTERN,
  CHAR_DENOTATION,
  CHAR_SYMBOL,
  CHOICE,
  CHOICE_PATTERN,
  CLASS_SYMBOL,
  CLOSED_CLAUSE,
  CLOSE_SYMBOL,
  CODE_CLAUSE,
  CODE_SYMBOL,
  COLLATERAL_CLAUSE,
  COLLECTION,
  COLON_SYMBOL,
  COLUMN_FUNCTION,
  COLUMN_SYMBOL,
  COMMA_SYMBOL,
  COMPLEX_PATTERN,
  COMPLEX_SYMBOL,
  COMPL_SYMBOL,
  CONDITIONAL_CLAUSE,
  CONSTRUCT,
  DECLARATION_LIST,
  DECLARER,
  DEFINING_IDENTIFIER,
  DEFINING_INDICANT,
  DEFINING_OPERATOR,
  DENOTATION,
  DEPROCEDURING,
  DEREFERENCING,
  DIAGONAL_FUNCTION,
  DIAGONAL_SYMBOL,
  DO_PART,
  DO_SYMBOL,
  DOTDOT_SYMBOL,
  DOWNTO_SYMBOL,
  DYNAMIC_REPLICATOR,
  EDOC_SYMBOL,
  ELIF_IF_PART,
  ELIF_PART,
  ELIF_SYMBOL,
  ELSE_BAR_SYMBOL,
  ELSE_OPEN_PART,
  ELSE_PART,
  ELSE_SYMBOL,
  EMPTY_SYMBOL,
  ENCLOSED_CLAUSE,
  END_SYMBOL,
  ENQUIRY_CLAUSE,
  ENVIRON_NAME,
  ENVIRON_SYMBOL,
  EQUALS_SYMBOL,
  ERROR,
  ERROR_IDENTIFIER,
  ESAC_SYMBOL,
  EXIT_SYMBOL,
  EXPONENT_FRAME,
  FALSE_SYMBOL,
  FIELD,
  FIELD_IDENTIFIER,
  FIELD_SELECTION,
  FILE_SYMBOL,
  FIRM,
  FI_SYMBOL,
  FIXED_C_PATTERN,
  FLEX_SYMBOL,
  FLOAT_C_PATTERN,
  FORMAL_BOUNDS,
  FORMAL_BOUNDS_LIST,
  FORMAL_DECLARER_MARK,
  FORMAL_DECLARERS,
  FORMAL_DECLARERS_LIST,
  FORMAT_A_FRAME,
  FORMAT_CLOSE_SYMBOL,
  FORMAT_DELIMITER_SYMBOL,
  FORMAT_D_FRAME,
  FORMAT_E_FRAME,
  FORMAT_IDENTIFIER,
  FORMAT_I_FRAME,
  FORMAT_ITEM_A,
  FORMAT_ITEM_B,
  FORMAT_ITEM_C,
  FORMAT_ITEM_D,
  FORMAT_ITEM_E,
  FORMAT_ITEM_ESCAPE,
  FORMAT_ITEM_F,
  FORMAT_ITEM_G,
  FORMAT_ITEM_H,
  FORMAT_ITEM_I,
  FORMAT_ITEM_J,
  FORMAT_ITEM_K,
  FORMAT_ITEM_L,
  FORMAT_ITEM_M,
  FORMAT_ITEM_MINUS,
  FORMAT_ITEM_N,
  FORMAT_ITEM_O,
  FORMAT_ITEM_P,
  FORMAT_ITEM_PLUS,
  FORMAT_ITEM_POINT,
  FORMAT_ITEM_Q,
  FORMAT_ITEM_R,
  FORMAT_ITEM_S,
  FORMAT_ITEM_T,
  FORMAT_ITEM_U,
  FORMAT_ITEM_V,
  FORMAT_ITEM_W,
  FORMAT_ITEM_X,
  FORMAT_ITEM_Y,
  FORMAT_ITEM_Z,
  FORMAT_OPEN_SYMBOL,
  FORMAT_PATTERN,
  FORMAT_POINT_FRAME,
  FORMAT_SYMBOL,
  FORMAT_TEXT,
  FORMAT_Z_FRAME,
  FORMULA,
  FOR_PART,
  FOR_SYMBOL,
  FROM_PART,
  FROM_SYMBOL,
  GENERAL_C_PATTERN,
  GENERAL_PATTERN,
  GENERATOR,
  GENERIC_ARGUMENT,
  GENERIC_ARGUMENT_LIST,
  GO_SYMBOL,
  GOTO_SYMBOL,
  HEAP_SYMBOL,
  IDENTIFIER,
  IDENTITY_DECLARATION,
  IDENTITY_RELATION,
  IF_PART,
  IF_SYMBOL,
  INDICANT,
  INITIALISER_SERIES,
  INSERTION,
  IN_SYMBOL,
  INT_DENOTATION,
  INTEGER_CASE_CLAUSE,
  INTEGER_CHOICE_CLAUSE,
  INTEGER_IN_PART,
  INTEGER_OUT_PART,
  INTEGRAL_C_PATTERN,
  INTEGRAL_MOULD,
  INTEGRAL_PATTERN,
  INT_SYMBOL,
  IN_TYPE_MODE,
  ISNT_SYMBOL,
  IS_SYMBOL,
  JUMP,
  KEYWORD,
  LABEL,
  LABELED_UNIT,
  LABEL_IDENTIFIER,
  LABEL_SEQUENCE,
  LITERAL,
  LOCAL_LABEL,
  LOC_SYMBOL,
  LONGETY,
  LONG_SYMBOL,
  LOOP_CLAUSE,
  LOOP_IDENTIFIER,
  MAIN_SYMBOL,
  MEEK,
  MODE_BITS,
  MODE_BOOL,
  MODE_BYTES,
  MODE_CHAR,
  MODE_COMPLEX,
  MODE_DECLARATION,
  MODE_FILE,
  MODE_FORMAT,
  MODE_INT,
  MODE_LONG_BITS,
  MODE_LONG_BYTES,
  MODE_LONG_COMPLEX,
  MODE_LONG_INT,
  MODE_LONGLONG_BITS,
  MODE_LONGLONG_COMPLEX,
  MODE_LONGLONG_INT,
  MODE_LONGLONG_REAL,
  MODE_LONG_REAL,
  MODE_NO_CHECK,
  MODE_PIPE,
  MODE_REAL,
  MODE_SOUND,
  MODE_SYMBOL,
  MONADIC_FORMULA,
  MONAD_SEQUENCE,
  NEW_SYMBOL,
  NIHIL,
  NIL_SYMBOL,
  NORMAL_IDENTIFIER,
  NO_SORT,
  OCCA_SYMBOL,
  OD_SYMBOL,
  OF_SYMBOL,
  OPEN_PART,
  OPEN_SYMBOL,
  OPERATOR,
  OPERATOR_DECLARATION,
  OPERATOR_PLAN,
  OP_SYMBOL,
  ORF_SYMBOL,
  OR_FUNCTION,
  OUSE_CASE_PART,
  OUSE_SYMBOL,
  OUT_PART,
  OUT_SYMBOL,
  OUT_TYPE_MODE,
  PARALLEL_CLAUSE,
  PARAMETER,
  PARAMETER_IDENTIFIER,
  PARAMETER_LIST,
  PARAMETER_PACK,
  PAR_SYMBOL,
  PARTICULAR_PROGRAM,
  PICTURE,
  PICTURE_LIST,
  PIPE_SYMBOL,
  POINT_SYMBOL,
  PRIMARY,
  PRIORITY,
  PRIORITY_DECLARATION,
  PRIO_SYMBOL,
  PROCEDURE_DECLARATION,
  PROCEDURE_VARIABLE_DECLARATION,
  PROCEDURING,
  PROC_SYMBOL,
  BLOCK_GC_REF,
  QUALIFIER,
  RADIX_FRAME,
  REAL_DENOTATION,
  REAL_PATTERN,
  REAL_SYMBOL,
  REF_SYMBOL,
  REPLICATOR,
  ROUTINE_TEXT,
  ROUTINE_UNIT,
  ROW_ASSIGNATION,
  ROW_ASSIGN_SYMBOL,
  ROW_CHAR_DENOTATION,
  ROW_FUNCTION,
  ROWING,
  ROWS_SYMBOL,
  ROW_SYMBOL,
  SECONDARY,
  SELECTION,
  SELECTOR,
  SEMA_SYMBOL,
  SEMI_SYMBOL,
  SERIAL_CLAUSE,
  SERIES_MODE,
  SHORTETY,
  SHORT_SYMBOL,
  SIGN_MOULD,
  SKIP,
  SKIP_SYMBOL,
  SLICE,
  SOFT,
  SOME_CLAUSE,
  SOUND_SYMBOL,
  SPECIFICATION,
  SPECIFIED_UNIT,
  SPECIFIED_UNIT_LIST,
  SPECIFIED_UNIT_UNIT,
  SPECIFIER,
  SPECIFIER_IDENTIFIER,
  STANDARD,
  STATIC_REPLICATOR,
  STOWED_MODE,
  STRING_C_PATTERN,
  STRING_PATTERN,
  STRING_SYMBOL,
  STRONG,
  STRUCT_SYMBOL,
  STRUCTURED_FIELD,
  STRUCTURED_FIELD_LIST,
  STRUCTURE_PACK,
  STYLE_I_COMMENT_SYMBOL,
  STYLE_II_COMMENT_SYMBOL,
  STYLE_I_PRAGMAT_SYMBOL,
  SUB_SYMBOL,
  SUB_UNIT,
  TERTIARY,
  THEN_BAR_SYMBOL,
  THEN_PART,
  THEN_SYMBOL,
  TO_PART,
  TO_SYMBOL,
  TRANSPOSE_FUNCTION,
  TRANSPOSE_SYMBOL,
  TRIMMER,
  TRUE_SYMBOL,
  UNION_DECLARER_LIST,
  UNION_PACK,
  UNION_SYMBOL,
  UNIT,
  UNITED_CASE_CLAUSE,
  UNITED_CHOICE,
  UNITED_IN_PART,
  UNITED_OUSE_PART,
  UNITING,
  UNIT_LIST,
  UNIT_SERIES,
  UNTIL_PART,
  UNTIL_SYMBOL,
  VARIABLE_DECLARATION,
  VIRTUAL_DECLARER_MARK,
  VOIDING,
  VOID_SYMBOL,
  WEAK,
  WHILE_PART,
  WHILE_SYMBOL,
  WIDENING,
  WILDCARD
};

enum
{ NOT_PRINTED, TO_PRINT, PRINTED };

enum {
  A68_NO_DIAGNOSTICS = 0,
  A68_ERROR,
  A68_SYNTAX_ERROR,
  A68_MATH_ERROR,
  A68_WARNING,
  A68_RUNTIME_ERROR,
  A68_SUPPRESS_SEVERITY,
  A68_ALL_DIAGNOSTICS,
  A68_RERUN,
  A68_FORCE_DIAGNOSTICS = 128,
  A68_FORCE_QUIT = 256
};

enum
{ NO_DEFLEXING = 1, SAFE_DEFLEXING, ALIAS_DEFLEXING, FORCE_DEFLEXING, SKIP_DEFLEXING };

/* Macros */

#define ABS(n) ((n) >= 0 ? (n) : -(n))

#define SIGN(n) ((n) == 0 ? 0 : ((n) > 0 ? 1 : -1))

#define COPY(d, s, n) {\
  int _m_k = (n); BYTE_T *_m_u = (BYTE_T *) (d), *_m_v = (BYTE_T *) (s);\
  while (_m_k--) {*_m_u++ = *_m_v++;}}

#define COPY_ALIGNED(d, s, n) {\
  int _m_k = (n); A68_ALIGN_T *_m_u = (A68_ALIGN_T *) (d), *_m_v = (A68_ALIGN_T *) (s);\
  while (_m_k > 0) {*_m_u++ = *_m_v++; _m_k -= A68_ALIGNMENT;}}

#define MOVE(d, s, n) {\
  int _m_k = (int) (n); BYTE_T *_m_d = (BYTE_T *) (d), *_m_s = (BYTE_T *) (s);\
  if (_m_s < _m_d) {\
    _m_d += _m_k; _m_s += _m_k;\
    while (_m_k--) {*(--_m_d) = *(--_m_s);}\
  } else {\
    while (_m_k--) {*(_m_d++) = *(_m_s++);}\
  }}

#define FILL(d, s, n) {\
   int _m_k = (n); BYTE_T *_m_u = (BYTE_T *) (d), _m_v = (BYTE_T) (s);\
   while (_m_k--) {*_m_u++ = _m_v;}}

#define FILL_ALIGNED(d, s, n) {\
   int _m_k = (n); A68_ALIGN_T *_m_u = (A68_ALIGN_T *) (d), _m_v = (A68_ALIGN_T) (s);\
   while (_m_k > 0) {*_m_u++ = _m_v; _m_k -= A68_ALIGNMENT;}}

#define ABEND(p, reason, info) {\
  if (p) {\
    abend ((char *) reason, (char *) info, __FILE__, __LINE__);\
  }}

#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES)
#define ASSERT(f) {\
  if (!(f)) {\
    if (a68g_curses_mode == A68_TRUE) {\
      (void) attrset (A_NORMAL);\
      (void) endwin ();\
      a68g_curses_mode = A68_FALSE;\
    }\
    ABEND(A68_TRUE, "Return value failure", ERROR_SPECIFICATION)\
  }}
#else
#define ASSERT(f) {\
  ABEND((!(f)), "Return value failure", ERROR_SPECIFICATION)\
  }
#endif

#define RESET_ERRNO {errno = 0;}

#define UNION_OFFSET (ALIGNED_SIZE_OF (A68_UNION))

/* Miscellaneous macros */

#define A68_SOUND_BYTES(s) ((int) ((s)->bits_per_sample) / 8 + (int) ((s)->bits_per_sample % 8 == 0 ? 0 : 1))
#define A68_SOUND_DATA_SIZE(s) ((int) ((s)->num_samples) * (int) ((s)->num_channels) *(int) ( A68_SOUND_BYTES (s)))
#define ARRAY(p) ((p)->array)
#define ANNOTATION(p) ((p)->annotation)
#define ATTRIBUTE(p) ((p)->attribute)
#define BODY(p) ((p)->body)
#define CAST(p) ((p)->cast)
#define DEFLEXED(p) ((p)->deflexed_mode)
#define DEFLEX(p) (DEFLEXED (p) != NULL ? DEFLEXED(p) : (p))
#define DIM(p) ((p)->dim)
#define ENVIRON(p) ((p)->environ)
#define EQUIVALENT(p) ((p)->equivalent_mode)
#define EXIT_COMPILATION longjmp(program.exit_compilation, 1)
#define FILE_DEREF(p) ((A68_FILE *) ADDRESS (p))
#define FORWARD(p) ((p) = NEXT (p))
#define FORMAT(p) ((p)->format)
#define HEAP(p) ((p)->heap)
#define GENIE(p) ((p)->genie)
#define INFO(p) ((p)->info)
#define LESS(p) ((p)->less)
#define LEX_LEVEL(p) (LEVEL (SYMBOL_TABLE (p)))
#define TAG_LEX_LEVEL(p) (LEVEL (TAG_TABLE (p)))
#define LEVEL(p) ((p)->level)
#define LINE_NUMBER(p) (NUMBER (LINE (p)))
#define LINE(p) ((p)->info->line)
#define LOCALE(p) ((p)->locale)
#define LWB(p) ((p)->lower_bound)
#define STATUS_SET(p, q) {STATUS (p) |= (q);}
#define STATUS_CLEAR(p, q) {STATUS (p) &= (~(q));}
#define STATUS_TEST(p, q) ((STATUS (p) & (q)) != (unsigned) 0)
#define MODE(p) (a68_modes.p)
#define MOID(p) ((p)->type)
#define MORE(p) ((p)->more)
#define MULTIPLE(p) ((p)->multiple_mode)
#define NAME(p) ((p)->name)
#define NEST(p) ((p)->nest)
#define NEXT_NEXT(p) (NEXT (NEXT (p)))
#define NEXT_NEXT_NEXT(p) (NEXT (NEXT_NEXT (p)))
#define NEXT(p) ((p)->next)
#define NEXT_SUB(p) (NEXT (SUB (p)))
#define NEXT_NEXT_SUB(p) (NEXT (NEXT_SUB (p)))
#define NODE(p) ((p)->node)
#define NUMBER(p) ((p)->number)
#define OFFSET(p) ((p)->offset)
#define OUTER(p) ((p)->outer)
#define PACK(p) ((p)->pack)
#define NODE_PACK(p) ((p)->pack)
#define PARENT(p) (GENIE (p)->parent)
#define PAR_LEVEL(p) ((p)->par_level)
#define POINTER(p) ((p)->pointer)
#define PREVIOUS(p) ((p)->previous)
#define PRIO(p) ((p)->priority)
#define PROC(p) ((p)->procedure)
#define PROPAGATOR(p) (GENIE (p)->propagator)
#define ROWED(p) ((p)->rowed)
#define SEQUENCE(p) ((p)->sequence)
#define SIZE(p) ((p)->size)
#define SLICE(p) ((p)->slice)
#define SORT(p) ((p)->sort)
#define STATUS(p) ((p)->status)
#define CODEX(p) ((p)->codex)
#define SUB_NEXT(p) (SUB (NEXT (p)))
#define SUB(p) ((p)->sub)
#define SUB_MOID(p) (SUB (MOID (p)))
#define SUB_SUB(p) (SUB (SUB (p)))
#define SYMBOL(p) ((p)->info->symbol)
#define SYMBOL_TABLE(p) ((p)->symbol_table)
#define TAG_TABLE(p) ((p)->symbol_table)
#define TAX(p) ((p)->tag)
#define TEXT(p) ((p)->text)
#define TRIM(p) ((p)->trim)
#define UPB(p) ((p)->upper_bound)
#define USE(p) ((p)->use)
#define VALUE(p) ((p)->value)
#define WHETHER_LITERALLY(p, s) (strcmp (SYMBOL (p), s) == 0)
#define WHETHER_NOT(p, s) (! WHETHER (p, s))
#define WHETHER(p, s) (ATTRIBUTE (p) == (s))

#define SCAN_ERROR(c, u, v, txt) if (c) {scan_error (u, v, txt);}
#define WRITE(f, s) io_write_string ((f), (s));
#define WRITELN(f, s) {io_close_tty_line (); WRITE ((f), (s));}

/* External definitions */

extern ADDR_T fixed_heap_pointer, temp_heap_pointer;
extern BOOL_T no_warnings;
extern BOOL_T in_execution;
extern BOOL_T halt_typing, time_limit_flag, listing_is_safe;
extern BOOL_T heap_is_fluid;
extern BYTE_T *system_stack_offset;
extern char output_line[], edit_line[], input_line[];
extern double begin_of_time;
extern double garbage_seconds;
extern int garbage_collects;
extern int stack_size;
extern int symbol_table_count, mode_count;
extern int term_width;
extern int new_nodes, new_modes, new_postulates, new_node_infos, new_genie_infos;
extern clock_t clock_res;
extern KEYWORD_T *top_keyword;
extern MODES_T a68_modes;
extern MODULE_T program;
extern MOID_LIST_T *top_moid_list, *old_moid_list;
extern POSTULATE_T *top_postulate, *top_postulate_list;
extern SYMBOL_TABLE_T *stand_env;
extern TAG_T *error_tag;
extern TOKEN_T *top_token;

extern int global_argc;
extern char **global_argv;
extern char a68g_cmd_name[];
extern NODE_T **node_register;

extern A68_REF heap_generator (NODE_T *, MOID_T *, int);
extern ADDR_T calculate_internal_index (A68_TUPLE *, int);
extern BOOL_T increment_internal_index (A68_TUPLE *, int);
extern BOOL_T lexical_analyser (void);
extern BOOL_T match_string (char *, char *, char);
extern BOOL_T set_options (OPTION_LIST_T *, BOOL_T);
extern BOOL_T whether (NODE_T * p, ...);
extern BOOL_T whether_coercion (NODE_T *);
extern BOOL_T whether_firm (MOID_T *, MOID_T *);
extern BOOL_T whether_modes_equal (MOID_T *, MOID_T *, int);
extern BOOL_T whether_modes_equivalent (MOID_T *, MOID_T *);
extern BOOL_T whether_new_lexical_level (NODE_T *);
extern BOOL_T whether_one_of (NODE_T * p, ...);
extern BOOL_T whether_subset (MOID_T *, MOID_T *, int);
extern BOOL_T whether_unitable (MOID_T *, MOID_T *, int);
extern BYTE_T *get_fixed_heap_space (size_t);
extern BYTE_T *get_temp_heap_space (size_t);
extern GENIE_INFO_T *new_genie_info (void);
extern KEYWORD_T *find_keyword (KEYWORD_T *, char *);
extern KEYWORD_T *find_keyword_from_attribute (KEYWORD_T *, int);
extern MOID_T *add_mode (MOID_T **, int, int, NODE_T *, MOID_T *, PACK_T *);
extern MOID_T *depref_completely (MOID_T *);
extern MOID_T *new_moid (void);
extern MOID_T *unites_to (MOID_T *, MOID_T *);
extern NODE_INFO_T *new_node_info (void);
extern NODE_T *new_node (void);
extern NODE_T *some_node (char *);
extern PACK_T *absorb_union_pack (PACK_T *, int *);
extern PACK_T *new_pack (void);
extern POSTULATE_T *whether_postulated (POSTULATE_T *, MOID_T *);
extern POSTULATE_T *whether_postulated_pair (POSTULATE_T *, MOID_T *, MOID_T *);
extern SOURCE_LINE_T *new_source_line (void);
extern SYMBOL_TABLE_T *find_level (NODE_T *, int);
extern SYMBOL_TABLE_T *new_symbol_table (SYMBOL_TABLE_T *);
extern TAG_T *add_tag (SYMBOL_TABLE_T *, int, NODE_T *, MOID_T *, int);
extern TAG_T *find_tag_global (SYMBOL_TABLE_T *, int, char *);
extern TAG_T *find_tag_local (SYMBOL_TABLE_T *, int, char *);
extern TAG_T *new_tag (void);
extern TOKEN_T *add_token (TOKEN_T **, char *);
extern TOKEN_T *find_token (TOKEN_T **, char *);
extern char *a68g_strchr (char *, int);
extern char *a68g_strrchr (char *, int);
extern char *ctrl_char (int);
extern char *moid_to_string (MOID_T *, int, NODE_T *);
extern char *new_fixed_string (char *);
extern char *new_temp_string (char *);
extern char *new_string (char *);
extern char *non_terminal_string (char *, int);
extern char *phrase_to_text (NODE_T *, NODE_T **);
extern char *propagator_name (PROPAGATOR_PROCEDURE *p);
extern char *read_string_from_tty (char *);
extern char *standard_environ_proc_name (GENIE_PROCEDURE);
extern char digit_to_char (int);
extern double a68g_acosh (double);
extern double a68g_asinh (double);
extern double a68g_atan2 (double, double);
extern double a68g_atanh (double);
extern double a68g_exp (double);
extern double a68g_hypot (double, double);
extern double a68g_log1p (double);
extern double seconds (void);
extern double ten_up (int);
extern int a68g_round (double);
extern int count_pack_members (PACK_T *);
extern int first_tag_global (SYMBOL_TABLE_T *, char *);
extern int get_row_size (A68_TUPLE *, int);
extern int grep_in_string (char *, char *, int *, int *);
extern int heap_available (void);
extern int moid_size (MOID_T *);
extern int whether_identifier_or_label_global (SYMBOL_TABLE_T *, char *);
extern ssize_t io_read (FILE_T, void *, size_t);
extern ssize_t io_read_conv (FILE_T, void *, size_t);
extern ssize_t io_write (FILE_T, const void *, size_t);
extern ssize_t io_write_conv (FILE_T, const void *, size_t);
extern unsigned a68g_strtoul (char *, char **, int);
extern void *get_heap_space (size_t);
extern void BLOCK_GC_HANDLE (A68_REF *);
extern void UNBLOCK_GC_HANDLE (A68_REF *);
extern void a68g_exit (int);
extern void abend (char *, char *, char *, int);
extern void acronym (char *, char *);
extern void add_mode_to_pack (PACK_T **, MOID_T *, char *, NODE_T *);
extern void add_mode_to_pack_end (PACK_T **, MOID_T *, char *, NODE_T *);
extern void add_moid_list (MOID_LIST_T **, SYMBOL_TABLE_T *);
extern void add_moid_moid_list (NODE_T *, MOID_LIST_T **);
extern void add_option_list (OPTION_LIST_T **, char *, SOURCE_LINE_T *);
extern void add_single_moid_to_list (MOID_LIST_T **, MOID_T *, SYMBOL_TABLE_T *);
extern void apropos (FILE_T, char *, char *);
extern void assign_offsets (NODE_T *);
extern void assign_offsets_packs (MOID_LIST_T *);
extern void assign_offsets_table (SYMBOL_TABLE_T *);
extern void bind_format_tags_to_tree (NODE_T *);
extern void bind_routine_tags_to_tree (NODE_T *);
extern void bind_tag (TAG_T **, TAG_T *);
extern void bogus_mips (void);
extern void bottom_up_error_check (NODE_T *);
extern void bottom_up_parser (NODE_T *);
extern void brief_mode_flat (FILE_T, MOID_T *);
extern void brief_moid_flat (FILE_T, MOID_T *);
extern void bufcat (char *, char *, int);
extern void bufcpy (char *, char *, int);
extern void check_parenthesis (NODE_T *);
extern void coercion_inserter (NODE_T *);
extern void collect_taxes (NODE_T *);
extern void compiler (FILE_T);
extern void contract_union (MOID_T *, int *);
extern void default_mem_sizes (void);
extern void default_options (void);
extern void diagnostic_line (int, SOURCE_LINE_T *, char *, char *, ...);
extern void diagnostic_node (int, NODE_T *, char *, ...);
extern void diagnostics_to_terminal (SOURCE_LINE_T *, int);
extern void discard_heap (void);
extern void dump_heap (void);
extern void dump_stowed (NODE_T *, FILE_T, void *, MOID_T *, int);
extern void fill_symbol_table_outer (NODE_T *, SYMBOL_TABLE_T *);
extern void finalise_symbol_table_setup (NODE_T *, int);
extern void free_heap (void);
extern void free_postulate_list (POSTULATE_T *, POSTULATE_T *);
extern void free_postulates (void);
extern void genie_init_heap (NODE_T *);
extern void get_global_level (NODE_T *);
extern void get_max_simplout_size (NODE_T *);
extern void get_moid_list (MOID_LIST_T **, NODE_T *);
extern void get_refinements (void);
extern void get_stack_size (void);
extern void init_curses (void);
extern void init_heap (void);
extern void init_moid_list (void);
extern void init_options (void);
extern void init_postulates (void);
extern void init_tty (void);
extern void initialise_internal_index (A68_TUPLE *, int);
extern void io_close_tty_line (void);
extern void io_write_string (FILE_T, const char *);
extern void isolate_options (char *, SOURCE_LINE_T *);
extern void jumps_from_procs (NODE_T * p);
extern void list_source_line (FILE_T, SOURCE_LINE_T *, BOOL_T);
extern void maintain_mode_table (NODE_T *);
extern void make_postulate (POSTULATE_T **, MOID_T *, MOID_T *);
extern void make_soid (SOID_T *, int, MOID_T *, int);
extern void make_special_mode (MOID_T **, int);
extern void make_standard_environ (void);
extern void make_sub (NODE_T *, NODE_T *, int);
extern void mark_auxilliary (NODE_T *);
extern void mark_moids (NODE_T *);
extern void mode_checker (NODE_T *);
extern void monitor_error (char *, char *);
extern void online_help (FILE_T);
extern void optimise_tree (NODE_T *);
extern void portcheck (NODE_T *);
extern void preliminary_symbol_table_setup (NODE_T *);
extern void print_bytes (BYTE_T *, int);
extern void print_internal_index (FILE_T, A68_TUPLE *, int);
extern void print_mode_flat (FILE_T, MOID_T *);
extern void protect_from_gc (NODE_T *);
extern void prune_echoes (OPTION_LIST_T *);
extern void put_refinements (void);
extern void read_env_options (void);
extern void read_rc_options (void);
extern void rearrange_goto_less_jumps (NODE_T *);
extern void register_nodes (NODE_T *);
extern void remove_empty_symbol_tables (NODE_T *);
extern void remove_file_type (char *);
extern void renumber_nodes (NODE_T *, int *);
extern void reset_max_simplout_size (void);
extern void reset_moid_list (void);
extern void reset_symbol_table_nest_count (NODE_T *);
extern void scan_error (SOURCE_LINE_T *, char *, char *);
extern void scope_checker (NODE_T *);
extern void set_moid_sizes (MOID_LIST_T *);
extern void set_nest (NODE_T *, NODE_T *);
extern void set_par_level (NODE_T *, int);
extern void set_proc_level (NODE_T *, int);
extern void set_up_mode_table (NODE_T *);
extern void set_up_tables (void);
extern void state_license (FILE_T);
extern void state_version (FILE_T);
extern void substitute_brackets (NODE_T *);
extern void tie_label_to_serial (NODE_T *);
extern void tie_label_to_unit (NODE_T *);
extern void top_down_parser (NODE_T *);
extern void tree_listing (FILE_T, NODE_T *, int, SOURCE_LINE_T *, int *);
extern void victal_checker (NODE_T *);
extern void warn_for_unused_tags (NODE_T *);
extern void warn_tags_threads (NODE_T *);
extern void where_in_source (FILE_T, NODE_T *);
extern void widen_denotation (NODE_T *);
extern void write_listing (void);
extern void write_listing_header (void);
extern void write_object_listing (void);
extern void write_source_line (FILE_T, SOURCE_LINE_T *, NODE_T *, int);
extern void write_source_listing (void);
extern void write_tree_listing (void);

/* Diagnostic texts */

#define MOID_ERROR_WIDTH 80

#define ERROR_SPECIFICATION (errno == 0 ? NULL : strerror (errno))

#if defined HAVE_COMPILER
#define ERROR_SPECIFICATION_COMPILER (dlerror ())
#endif

#define ERROR_ACCESSING_NIL "attempt to access N"
#define ERROR_ALIGNMENT "alignment error"
#define ERROR_ARGUMENT_NUMBER "incorrect number of arguments for M"
#define ERROR_CANNOT_END_WITH_DECLARATION "clause cannot end with a declaration"
#define ERROR_CANNOT_OPEN_NAME "cannot open Z"
#define ERROR_CANNOT_WIDEN "cannot widen M to M"
#define ERROR_CANNOT_WRITE_LISTING "cannot write listing file"
#define ERROR_CHANNEL_DOES_NOT_ALLOW "channel does not allow Y"
#define ERROR_CLAUSE_WITHOUT_VALUE "clause does not yield a value"
#define ERROR_CLOSING_DEVICE "error while closing device"
#define ERROR_CLOSING_FILE "error while closing file"
#define ERROR_COMMA_MUST_SEPARATE "A and A must be separated by a comma-symbol"
#define ERROR_COMPONENT_NUMBER "M must have at least two components"
#define ERROR_COMPONENT_RELATED "M has firmly related components"
#define ERROR_CURSES "error in curses operation"
#define ERROR_CURSES_OFF_SCREEN "curses operation moves cursor off the screen"
#define ERROR_CYCLIC_MODE "M specifies no mode (references to self)"
#define ERROR_DEVICE_ALREADY_SET "device parameters already set"
#define ERROR_DEVICE_CANNOT_ALLOCATE "cannot allocate device parameters"
#define ERROR_DEVICE_CANNOT_OPEN "cannot open device"
#define ERROR_DEVICE_NOT_OPEN "device is not open"
#define ERROR_DEVICE_NOT_SET "device parameters not set"
#define ERROR_DIFFERENT_BOUNDS "rows have different bounds"
#define ERROR_DIVISION_BY_ZERO "attempt at M division by zero"
#define ERROR_DYADIC_PRIORITY "dyadic S has no priority declaration"
#define ERROR_EMPTY_ARGUMENT "empty argument"
#define ERROR_EMPTY_VALUE "attempt to use uninitialised M value"
#define ERROR_EMPTY_VALUE_FROM (ERROR_EMPTY_VALUE)
#define ERROR_EXPECTED_NEAR "B expected in A, near Z L"
#define ERROR_EXPECTED "Y expected"
#define ERROR_EXPONENT_DIGIT "invalid exponent digit"
#define ERROR_EXPONENT_INVALID "invalid M exponent"
#define ERROR_FALSE_ASSERTION "false assertion"
#define ERROR_FEATURE_UNSUPPORTED "unsupported feature S"
#define ERROR_FFT "fourier transform error; U; U"
#define ERROR_FILE_ACCESS "file access error"
#define ERROR_FILE_ALREADY_OPEN "file is already open"
#define ERROR_FILE_CANNOT_OPEN_FOR "cannot open Z for Y"
#define ERROR_FILE_CANT_RESET "cannot reset file"
#define ERROR_FILE_CANT_SET "cannot set file"
#define ERROR_FILE_CLOSE "error while closing file"
#define ERROR_FILE_ENDED "end of file reached"
#define ERROR_FILE_INCLUDE_CTRL "control characters in include file"
#define ERROR_FILE_LOCK "error while locking file"
#define ERROR_FILE_NO_TEMP "cannot create unique temporary file name"
#define ERROR_FILE_NOT_OPEN "file is not open"
#define ERROR_FILE_READ "error while reading file"
#define ERROR_FILE_RESET "error while resetting file"
#define ERROR_FILE_SCRATCH "error while scratching file"
#define ERROR_FILE_SET "error while setting file"
#define ERROR_FILE_SOURCE_CTRL "control characters in source file"
#define ERROR_FILE_TRANSPUT "error transputting M value"
#define ERROR_FILE_TRANSPUT_SIGN "error transputting sign in M value"
#define ERROR_FILE_WRONG_MOOD "file is in Y mood"
#define ERROR_FLEX_ROW "flexibility is a property of rows"
#define ERROR_FORMAT_CANNOT_TRANSPUT "cannot transput M value with A"
#define ERROR_FORMAT_EXHAUSTED "patterns exhausted in format"
#define ERROR_FORMAT_INTS_REQUIRED "1 .. 3 M arguments required"
#define ERROR_FORMAT_INVALID_REPLICATOR "negative replicator"
#define ERROR_FORMAT_PICTURE_NUMBER "incorrect number of pictures for A"
#define ERROR_FORMAT_PICTURES "number of pictures does not match number of arguments"
#define ERROR_FORMAT_UNDEFINED "cannot use undefined format"
#define ERROR_INCORRECT_FILENAME "incorrect filename"
#define ERROR_IN_DENOTATION "error in M denotation"
#define ERROR_INDEXER_NUMBER "incorrect number of indexers for M"
#define ERROR_INDEX_OUT_OF_BOUNDS "index out of bounds"
#define ERROR_INTERNAL_CONSISTENCY "internal consistency check failure"
#define ERROR_INVALID_ARGUMENT "invalid M argument"
#define ERROR_INVALID_DIMENSION "invalid dimension D"
#define ERROR_INVALID_OPERAND "M construct is an invalid operand"
#define ERROR_INVALID_OPERATOR_TAG "invalid operator tag"
#define ERROR_INVALID_PARAMETER "invalid parameter (U Z)"
#define ERROR_INVALID_PRIORITY "invalid priority declaration"
#define ERROR_INVALID_RADIX "invalid radix D"
#define ERROR_INVALID_SEQUENCE "U is not a valid A"
#define ERROR_INVALID_SIZE "object of invalid size"
#define ERROR_KEYWORD "check for missing or unmatched keyword in clause starting at S"
#define ERROR_LABEL_BEFORE_DECLARATION "declaration cannot follow a labeled unit"
#define ERROR_LABELED_UNIT_MUST_FOLLOW "S must be followed by a labeled unit"
#define ERROR_LABEL_IN_PAR_CLAUSE "target label is in another parallel unit"
#define ERROR_LAPLACE "laplace transform error; U; U"
#define ERROR_LINE_ENDED "end of line reached"
#define ERROR_LONG_STRING "string exceeds end of line"
#define ERROR_MATH_EXCEPTION "math exception E"
#define ERROR_MATH "M math error"
#define ERROR_MODE_SPECIFICATION "M construct must yield a routine, row or structured value"
#define ERROR_MP_OUT_OF_BOUNDS "multiprecision value out of bounds"
#define ERROR_MULTIPLE_FIELD "multiple declaration of field S"
#define ERROR_MULTIPLE_TAG "multiple declaration of tag S"
#define ERROR_NIL_DESCRIPTOR "descriptor is N"
#define ERROR_NO_COMPONENT "M is neither component nor subset of M"
#define ERROR_NO_DYADIC "dyadic operator O S O has not been declared"
#define ERROR_NO_FIELD "M has no field Z"
#define ERROR_NO_FLEX_ARGUMENT "M value from A cannot be flexible"
#define ERROR_NO_SOURCE_FILE "no source file specified"
#define ERROR_NO_MATRIX "M A does not yield a two-dimensional row"
#define ERROR_NO_MONADIC "monadic operator S O has not been declared"
#define ERROR_NO_NAME "M A does not yield a name"
#define ERROR_NO_NAME_REQUIRED "context does not require a name"
#define ERROR_NO_PARALLEL_CLAUSE "interpreter was compiled without support for the parallel-clause"
#define ERROR_NO_PRIORITY "S has no priority declaration"
#define ERROR_NO_PROC "M A does not yield a procedure taking arguments"
#define ERROR_NO_ROW_OR_PROC "M A does not yield a row or procedure"
#define ERROR_NO_SQUARE_MATRIX "M matrix is not square"
#define ERROR_NO_STRUCT "M A does not yield a structured value"
#define ERROR_NOT_WELL_FORMED "S does not specify a well formed mode"
#define ERROR_NO_UNION "M is not a united mode"
#define ERROR_NO_UNIQUE_MODE "construct has no unique mode"
#define ERROR_NO_VECTOR "M A does not yield a one-dimensional row"
#define ERROR_OPERAND_NUMBER "incorrect number of operands for S"
#define ERROR_OPERATOR_INVALID "monadic S cannot start with a character from Z"
#define ERROR_OPERATOR_RELATED "M Z is firmly related to M Z"
#define ERROR_OUT_OF_BOUNDS "M value out of bounds"
#define ERROR_OUT_OF_CORE "insufficient memory"
#define ERROR_PAGE_SIZE "error in page size"
#define ERROR_PARALLEL_CANNOT_CREATE "cannot create thread"
#define ERROR_PARALLEL_CANNOT_JOIN "cannot join thread"
#define ERROR_PARALLEL_OUTSIDE "invalid outside a parallel clause"
#define ERROR_PARALLEL_OVERFLOW "too many parallel units"
#define ERROR_PARENTHESIS_2 "incorrect parenthesis nesting; encountered X L but expected X; check for Y"
#define ERROR_PARENTHESIS "incorrect parenthesis nesting; check for Y"
#define ERROR_PRAGMENT "error in pragment"
#define ERROR_QUOTED_BOLD_TAG "error in quoted bold tag"
#define ERROR_REDEFINED_KEYWORD "attempt to redefine keyword U in A"
#define ERROR_REFINEMENT_APPLIED "refinement is applied more than once"
#define ERROR_REFINEMENT_DEFINED "refinement already defined"
#define ERROR_REFINEMENT_EMPTY "empty refinement at end of program"
#define ERROR_REFINEMENT_INVALID "invalid refinement"
#define ERROR_REFINEMENT_NOT_APPLIED "refinement is not applied"
#define ERROR_RELATED_MODES "M is related to M"
#define ERROR_REQUIRE_THREADS "parallel clause requires posix threads"
#define ERROR_RUNTIME_ERROR "runtime error"
#define ERROR_SHELL_SCRIPT "source is a shell script"
#define ERROR_SCOPE_DYNAMIC_0 "value is exported out of its scope"
#define ERROR_SCOPE_DYNAMIC_1 "M value is exported out of its scope"
#define ERROR_SCOPE_DYNAMIC_2 "M value from %s is exported out of its scope"
#define ERROR_SOUND_INTERNAL "error while processing M value (Y)"
#define ERROR_SOUND_INTERNAL_STRING "error while processing M value (Y \"Y\")"
#define ERROR_SOURCE_FILE_OPEN "error while opening source file"
#define ERROR_STACK_OVERFLOW "stack overflow"
#define ERROR_SUBSET_RELATED "M has firmly related subset M"
#define ERROR_SYNTAX "detected in A"
#define ERROR_SYNTAX_EXPECTED "expected A"
#define ERROR_SYNTAX_MIXED_DECLARATION "possibly mixed identity and variable declaration"
#define ERROR_SYNTAX_STRANGE_SEPARATOR "possibly a missing or erroneous separator nearby"
#define ERROR_SYNTAX_STRANGE_TOKENS "possibly a missing or erroneous symbol nearby"
#define ERROR_THREAD_ACTIVE "parallel clause terminated but thread still active"
#define ERROR_TIME_LIMIT_EXCEEDED "time limit exceeded"
#define ERROR_TOO_MANY_ARGUMENTS "too many arguments"
#define ERROR_TOO_MANY_OPEN_FILES "too many open files"
#define ERROR_TORRIX "linear algebra error; U; U"
#define ERROR_TRANSIENT_NAME "attempt at storing a transient name"
#define ERROR_UNBALANCED_KEYWORD "missing or unbalanced keyword in A, near Z L"
#define ERROR_UNDECLARED_TAG_2 "tag Z has not been declared properly"
#define ERROR_UNDECLARED_TAG "tag S has not been declared properly"
#define ERROR_UNDEFINED_TRANSPUT "transput of M value by this procedure is not defined"
#define ERROR_UNDETERMINDED_FILE_MOOD "file has undetermined mood"
#define ERROR_UNIMPLEMENTED_PRECISION "M precision is not implemented"
#define ERROR_UNIMPLEMENTED "S is either not implemented or not compiled"
#define ERROR_UNSPECIFIED "unspecified error"
#define ERROR_UNTERMINATED_COMMENT "unterminated comment"
#define ERROR_UNTERMINATED_PRAGMAT "unterminated pragmat"
#define ERROR_UNTERMINATED_PRAGMENT "unterminated pragment"
#define ERROR_UNTERMINATED_STRING "unterminated string"
#define ERROR_UNWORTHY_CHARACTER "unworthy character"
#define INFO_APPROPRIATE_DECLARER "appropriate declarer"
#define INFO_MISSING_KEYWORDS "missing or unmatched keyword"
#define WARNING_DEFINED_IN_OTHER_THREAD "definition of S is in the private stack of another thread"
#define WARNING_EXTENSION "@ is an extension"
#define WARNING_HIDES "declaration hides a declaration of S with larger reach"
#define WARNING_HIDES_PRELUDE "declaration hides prelude declaration of M S"
#define WARNING_OVERFLOW "M constant overflow"
#define WARNING_OPTIMISATION "optimisation has no effect on this platform"
#define WARNING_SCOPE_STATIC_1 "value from A could be exported out of its scope"
#define WARNING_SCOPE_STATIC_2 "M value from A could be exported out of its scope"
#define WARNING_SKIPPED_SUPERFLUOUS "skipped superfluous A"
#define WARNING_TAG_NOT_PORTABLE "tag S is not portable"
#define WARNING_TAG_UNUSED "tag S is not used"
#define WARNING_TRAILING "ignoring trailing character H in A"
#define WARNING_UNDERFLOW "M constant underflow"
#define WARNING_UNINITIALISED "identifier S might be used before being initialised"
#define WARNING_UNINTENDED "possibly unintended M A in M A"
#define WARNING_VOIDED "value of M @ will be voided"
#define WARNING_WIDENING_NOT_PORTABLE "implicit widening is not portable"

/* Interpreter related definitions */

extern BYTE_T *stack_segment, *heap_segment, *handle_segment;

/* Inline macros */

/* ADDRESS calculates the effective address of fat pointer z */

#if defined A68_DEBUG
static BYTE_T * a68g_address (A68_REF * z)
{
  BYTE_T *x;
  if (IS_IN_HEAP (z)) {
    x = (BYTE_T *) & REF_POINTER (z)[REF_OFFSET (z)];
    ABEND (x >= handle_segment, ERROR_INTERNAL_CONSISTENCY, NULL);
  } else {
    x = (BYTE_T *) & stack_segment[REF_OFFSET (z)];
    ABEND (x < stack_segment, ERROR_INTERNAL_CONSISTENCY, NULL);
  }
  return (x);
}
#define ADDRESS(z) (a68g_address (z))
#else
#define ADDRESS(z) (&((IS_IN_HEAP (z) ? REF_POINTER (z) : stack_segment)[REF_OFFSET (z)]))
#endif

#define LOCAL_ADDRESS(z) (& stack_segment[REF_OFFSET (z)])
#define ARRAY_ADDRESS(z) (&(REF_POINTER (z)[REF_OFFSET (z)]))
#define DEREF(mode, expr) ((mode *) ADDRESS (expr))

/* Prelude errors can also occur in the constant folder */

#define PRELUDE_ERROR(cond, p, txt, add)\
  if (cond) {\
    errno = ERANGE;\
    if (in_execution) {\
      diagnostic_node (A68_RUNTIME_ERROR, p, txt, add);\
      exit_genie (p, A68_RUNTIME_ERROR);\
    } else {\
      diagnostic_node (A68_MATH_ERROR, p, txt, add);\
    }}

/* Check on a NIL name */

#define CHECK_REF(p, z, m)\
  if (! INITIALISED (&z)) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE_FROM, (m));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  } else if (IS_NIL (z)) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_ACCESSING_NIL, (m));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }

/* Check whether the heap fills */

#define PREEMPTIVE_GC {\
  double f = (double) heap_pointer / (double) heap_size;\
  double h = (double) free_handle_count / (double) max_handle_count;\
  if (f > 0.8 || h < 0.2) {\
    gc_heap ((NODE_T *) p, frame_pointer);\
  }}

/* Operations on stack frames */

#define FRAME_ADDRESS(n) ((BYTE_T *) &(stack_segment[n]))
#define FRAME_CLEAR(m) FILL_ALIGNED ((BYTE_T *) FRAME_OFFSET (FRAME_INFO_SIZE), 0, (m))
#define FRAME_BLOCKS(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->blocks)
#define FRAME_DYNAMIC_LINK(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->dynamic_link)
#define FRAME_DYNAMIC_SCOPE(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->dynamic_scope)
#define FRAME_INCREMENT(n) (SYMBOL_TABLE (FRAME_TREE(n))->ap_increment)
#define FRAME_INFO_SIZE (A68_ALIGN_8 ((int) sizeof (ACTIVATION_RECORD)))
#define FRAME_JUMP_STAT(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->jump_stat)
#define FRAME_LEXICAL_LEVEL(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->frame_level)
#define FRAME_LOCAL(n, m) (FRAME_ADDRESS ((n) + FRAME_INFO_SIZE + (m)))
#define FRAME_NUMBER(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->frame_no)
#define FRAME_OBJECT(n) (FRAME_OFFSET (FRAME_INFO_SIZE + (n)))
#define FRAME_OFFSET(n) (FRAME_ADDRESS (frame_pointer + (n)))
#define FRAME_OUTER(n) (SYMBOL_TABLE (FRAME_TREE(n))->outer)
#define FRAME_PARAMETER_LEVEL(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->parameter_level)
#define FRAME_PARAMETERS(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->parameters)
#define FRAME_PROC_FRAME(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->proc_frame)
#define FRAME_SIZE(fp) (FRAME_INFO_SIZE + FRAME_INCREMENT (fp))
#define FRAME_STATIC_LINK(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->static_link)
#define FRAME_TOP (FRAME_OFFSET (0))
#define FRAME_TREE(n) (NODE ((ACTIVATION_RECORD *) FRAME_ADDRESS(n)))

#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
#define FRAME_THREAD_ID(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->thread_id)
#endif

#define FOLLOW_SL(dest, l) {\
  (dest) = frame_pointer;\
  if ((l) <= FRAME_PARAMETER_LEVEL ((dest))) {\
    (dest) = FRAME_PARAMETERS ((dest));\
  }\
  while ((l) != FRAME_LEXICAL_LEVEL ((dest))) {\
    (dest) = FRAME_STATIC_LINK ((dest));\
  }}

#define FOLLOW_STATIC_LINK(dest, l) {\
  if ((l) == global_level && global_pointer > 0) {\
    (dest) = global_pointer;\
  } else {\
    FOLLOW_SL (dest, l)\
  }}

#define FRAME_GET(dest, cast, p) {\
  ADDR_T _m_z;\
  FOLLOW_STATIC_LINK (_m_z, LEVEL (GENIE (p)));\
  (dest) = (cast *) & (OFFSET (GENIE (p))[_m_z]);\
  }

#define GET_FRAME(dest, cast, level, offset) {\
  ADDR_T _m_z;\
  FOLLOW_SL (_m_z, (level));\
  (dest) = (cast *) & (stack_segment [_m_z + FRAME_INFO_SIZE + (offset)]);\
  }

#define GET_GLOBAL(dest, cast, offset) {\
  (dest) = (cast *) & (stack_segment [global_pointer + FRAME_INFO_SIZE + (offset)]);\
  }

/***************************/
/* Macros for row-handling */
/***************************/

#define GET_DESCRIPTOR(a, t, p)\
  a = (A68_ARRAY *) ARRAY_ADDRESS (p);\
  t = (A68_TUPLE *) & (((BYTE_T *) (a)) [ALIGNED_SIZE_OF (A68_ARRAY)]);

#define GET_DESCRIPTOR2(a, t1, t2, p)\
  a = (A68_ARRAY *) ARRAY_ADDRESS (p);\
  t1 = (A68_TUPLE *) & (((BYTE_T *) (a)) [ALIGNED_SIZE_OF (A68_ARRAY)]);\
  t2 = (A68_TUPLE *) & (((BYTE_T *) (a)) [ALIGNED_SIZE_OF (A68_ARRAY) + sizeof (A68_TUPLE)]);

#define PUT_DESCRIPTOR(a, t1, p) {\
  BYTE_T *a_p = ARRAY_ADDRESS (p);\
  *(A68_ARRAY *) a_p = (a);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [ALIGNED_SIZE_OF (A68_ARRAY)]) = (t1);\
  }

#define PUT_DESCRIPTOR2(a, t1, t2, p) {\
  /* ABEND (IS_NIL (*p), ERROR_NIL_DESCRIPTOR, NULL); */\
  BYTE_T *a_p = ARRAY_ADDRESS (p);\
  *(A68_ARRAY *) a_p = (a);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [ALIGNED_SIZE_OF (A68_ARRAY)]) = (t1);\
  *(A68_TUPLE *) &(((BYTE_T *) (a_p)) [ALIGNED_SIZE_OF (A68_ARRAY) + sizeof (A68_TUPLE)]) = (t2);\
  }

#define ROW_SIZE(t) ((UPB (t) >= LWB (t)) ? (UPB (t) - LWB (t) + 1) : 0)

#define ROW_ELEMENT(a, k) (((ADDR_T) k + (a)->slice_offset) * (a)->elem_size + (a)->field_offset)
#define ROW_ELEM(a, k, s) (((ADDR_T) k + (a)->slice_offset) * (s) + (a)->field_offset)

#define INDEX_1_DIM(a, t, k) ROW_ELEMENT (a, ((t)->span * (int) k - (t)->shift))

/*************/
/* Execution */
/*************/

#define EXECUTE_UNIT_2(p, dest) {\
  PROPAGATOR_T *_prop_ = &PROPAGATOR (p);\
  last_unit = p;\
  dest = (*(_prop_->unit)) (_prop_->source);\
  }

#define EXECUTE_UNIT(p) {\
  PROPAGATOR_T *_prop_ = &PROPAGATOR (p);\
  last_unit = p;\
  (void) (*(_prop_->unit)) (_prop_->source);\
  }

#define EXECUTE_UNIT_TRACE(p) {\
  if (STATUS_TEST (p, (BREAKPOINT_MASK | BREAKPOINT_TEMPORARY_MASK | \
      BREAKPOINT_INTERRUPT_MASK | BREAKPOINT_WATCH_MASK | BREAKPOINT_TRACE_MASK))) {\
    single_step ((p), STATUS (p));\
  }\
  EXECUTE_UNIT (p);\
  }

/***********************************/
/* Stuff for the garbage collector */
/***********************************/

/* Store intermediate REF to save it from the GC */

#define BLOCK_GC_REF(p, z)\
  if ((p)->block_ref != NULL) {\
    *(A68_REF *) FRAME_LOCAL (frame_pointer, OFFSET ((p)->block_ref)) = *(A68_REF *) (z);\
  }

/* Store REF on top of stack to save it from the GC */

#define BLOCK_GC_TOS(p)\
  if (GENIE (p)->block_ref != NULL) {\
    *(A68_REF *) FRAME_LOCAL (frame_pointer, OFFSET (GENIE (p)->block_ref)) =\
    *(A68_REF *) (STACK_OFFSET (- ALIGNED_SIZE_OF (A68_REF)));\
  }

/* Save a handle from the GC */

#define BLOCK_GC_HANDLE(z) { if (IS_IN_HEAP (z)) {STATUS_SET (REF_HANDLE(z), BLOCK_GC_MASK);} }
#define UNBLOCK_GC_HANDLE(z) { if (IS_IN_HEAP (z)) {STATUS_CLEAR (REF_HANDLE (z), BLOCK_GC_MASK);} }


/* Only the main thread can invoke or (un)block the GC */

extern int block_gc;

#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
#define UP_BLOCK_GC {\
  if (FRAME_THREAD_ID (frame_pointer) == main_thread_id) {\
    block_gc++;\
  }}
#define DOWN_BLOCK_GC {\
  if (FRAME_THREAD_ID (frame_pointer) == main_thread_id) {\
    block_gc--;\
  }}
#else
#define UP_BLOCK_GC {block_gc++;}
#define DOWN_BLOCK_GC {block_gc--;}
#endif

/* 
The void * cast in next macro is to stop warnings about dropping a volatile
qualifier to a pointer. This is safe here.
*/

#define GENIE_DNS_STACK(p, m, limit, info)\
  if (p != NULL && GENIE (p) != NULL && GENIE (p)->need_dns && limit != PRIMAL_SCOPE) {\
    genie_dns_addr ((void *)(p), (m), (STACK_OFFSET (-MOID_SIZE (m))), (limit), (info));\
  }

/* Tests for objects of mode INT */

#if defined HAVE_IEEE_754
#define CHECK_INT_ADDITION(p, i, j)\
  PRELUDE_ERROR (ABS ((double) (i) + (double) (j)) > (double) INT_MAX, p, ERROR_MATH, MODE (INT))
#define CHECK_INT_SUBTRACTION(p, i, j)\
  PRELUDE_ERROR (ABS ((double) (i) - (double) (j)) > (double) INT_MAX, p, ERROR_MATH, MODE (INT))
#define CHECK_INT_MULTIPLICATION(p, i, j)\
  PRELUDE_ERROR (ABS ((double) (i) * (double) (j)) > (double) INT_MAX, p, ERROR_MATH, MODE (INT))
#define CHECK_INT_DIVISION(p, i, j)\
  PRELUDE_ERROR ((j) == 0, p, ERROR_DIVISION_BY_ZERO, MODE (INT))
#else
#define CHECK_INT_ADDITION(p, i, j) {;}
#define CHECK_INT_SUBTRACTION(p, i, j) {;}
#define CHECK_INT_MULTIPLICATION(p, i, j) {;}
#define CHECK_INT_DIVISION(p, i, j) {;}
#endif

#define CHECK_INDEX(p, k, t) {\
  if (VALUE (k) < LWB (t) || VALUE (k) > UPB (t)) {\
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }}

/* Tests for objects of mode REAL */

#if defined HAVE_IEEE_754

#define NOT_A_REAL(x) (!finite (x))
#define CHECK_REAL_REPRESENTATION(p, u)\
  PRELUDE_ERROR (NOT_A_REAL (u), p, ERROR_MATH, MODE (REAL))
#define CHECK_REAL_ADDITION(p, u, v) CHECK_REAL_REPRESENTATION (p, (u) + (v))
#define CHECK_REAL_SUBTRACTION(p, u, v) CHECK_REAL_REPRESENTATION (p, (u) - (v))
#define CHECK_REAL_MULTIPLICATION(p, u, v) CHECK_REAL_REPRESENTATION (p, (u) * (v))
#define CHECK_REAL_DIVISION(p, u, v)\
  PRELUDE_ERROR ((v) == 0, p, ERROR_DIVISION_BY_ZERO, MODE (REAL))
#define CHECK_COMPLEX_REPRESENTATION(p, u, v)\
  PRELUDE_ERROR (NOT_A_REAL (u) || NOT_A_REAL (v), p, ERROR_MATH, MODE (COMPLEX))
#else
#define CHECK_REAL_REPRESENTATION(p, u) {;}
#define CHECK_REAL_ADDITION(p, u, v) {;}
#define CHECK_REAL_SUBTRACTION(p, u, v) {;}
#define CHECK_REAL_MULTIPLICATION(p, u, v) {;}
#define CHECK_REAL_DIVISION(p, u, v) {;}
#define CHECK_COMPLEX_REPRESENTATION(p, u, v) {;}
#endif

#define math_rte(p, z, m, t)\
  PRELUDE_ERROR (z, (p), ERROR_MATH, (m))

/*
Macro's for stack checking. Since the stacks grow by small amounts at a time
(A68 rows are in the heap), we check the stacks only at certain points: where
A68 recursion may set in, or in the garbage collector. We check whether there
still is sufficient overhead to make it to the next check.
*/

#define TOO_COMPLEX "program too complex"

#define SYSTEM_STACK_USED (ABS ((int) (system_stack_offset - &stack_offset)))
#define LOW_SYSTEM_STACK_ALERT(p) {\
  BYTE_T stack_offset;\
  if (stack_size > 0 && SYSTEM_STACK_USED >= stack_limit) {\
    errno = 0;\
    if ((p) == NULL) {\
      ABEND (A68_TRUE, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
    } else {\
      diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
      exit_genie ((p), A68_RUNTIME_ERROR);\
  }}}

#define LOW_STACK_ALERT(p) {\
  LOW_SYSTEM_STACK_ALERT (p);\
  if ((p) != NULL && (frame_pointer >= frame_stack_limit || stack_pointer >= expr_stack_limit)) { \
    errno = 0;\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }}

/* Opening of stack frames is in-line */

/* 
STATIC_LINK_FOR_FRAME: determine static link for stack frame.
new_lex_lvl: lexical level of new stack frame.
returns: static link for stack frame at 'new_lex_lvl'. 
*/

#define STATIC_LINK_FOR_FRAME(dest, new_lex_lvl) {\
  int _m_cur_lex_lvl = FRAME_LEXICAL_LEVEL (frame_pointer);\
  if (_m_cur_lex_lvl == (new_lex_lvl)) {\
    (dest) = FRAME_STATIC_LINK (frame_pointer);\
  } else if (_m_cur_lex_lvl > (new_lex_lvl)) {\
    ADDR_T _m_static_link = frame_pointer;\
    while (FRAME_LEXICAL_LEVEL (_m_static_link) >= (new_lex_lvl)) {\
      _m_static_link = FRAME_STATIC_LINK (_m_static_link);\
    }\
    (dest) = _m_static_link;\
  } else {\
    (dest) = frame_pointer;\
  }}

#define INIT_STATIC_FRAME(p) {\
  FRAME_CLEAR (SYMBOL_TABLE (p)->ap_increment);\
  if (SYMBOL_TABLE (p)->initialise_frame) {\
    initialise_frame (p);\
  }}

#define INIT_GLOBAL_POINTER(p) {\
  if (LEX_LEVEL (p) == global_level) {\
    global_pointer = frame_pointer;\
  }}

#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
#define OPEN_STATIC_FRAME(p) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act, *pre;\
  STATIC_LINK_FOR_FRAME (static_link, LEX_LEVEL (p));\
  pre = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->frame_no = pre->frame_no + 1;\
  act->frame_level = LEX_LEVEL (p);\
  act->parameter_level = pre->parameter_level;\
  act->parameters = pre->parameters;\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A68_FALSE;\
  act->thread_id = pthread_self ();\
  act->blocks = block_gc;\
  }
#else
#define OPEN_STATIC_FRAME(p) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act, *pre;\
  STATIC_LINK_FOR_FRAME (static_link, LEX_LEVEL (p));\
  pre = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->frame_no = pre->frame_no + 1;\
  act->frame_level = LEX_LEVEL (p);\
  act->parameter_level = pre->parameter_level;\
  act->parameters = pre->parameters;\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A68_FALSE;\
  act->blocks = block_gc;\
  }
#endif

#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
#define OPEN_PROC_FRAME(p, environ) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act;\
  PREEMPTIVE_GC;\
  LOW_STACK_ALERT (p);\
  static_link = (environ > 0 ? environ : frame_pointer);\
  if (frame_pointer < static_link) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_SCOPE_DYNAMIC_0);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->frame_no = FRAME_NUMBER (dynamic_link) + 1;\
  act->frame_level = LEX_LEVEL (p);\
  act->parameter_level = LEX_LEVEL (p);\
  act->parameters = frame_pointer;\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A68_TRUE;\
  act->thread_id = pthread_self ();\
  act->blocks = block_gc;\
  }
#else
#define OPEN_PROC_FRAME(p, environ) {\
  ADDR_T dynamic_link = frame_pointer, static_link;\
  ACTIVATION_RECORD *act;\
  PREEMPTIVE_GC;\
  LOW_STACK_ALERT (p);\
  static_link = (environ > 0 ? environ : frame_pointer);\
  if (frame_pointer < static_link) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_SCOPE_DYNAMIC_0);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  frame_pointer += FRAME_SIZE (dynamic_link);\
  act = (ACTIVATION_RECORD *) FRAME_ADDRESS (frame_pointer);\
  act->frame_no = FRAME_NUMBER (dynamic_link) + 1;\
  act->frame_level = LEX_LEVEL (p);\
  act->parameter_level = LEX_LEVEL (p);\
  act->parameters = frame_pointer;\
  act->static_link = static_link;\
  act->dynamic_link = dynamic_link;\
  act->dynamic_scope = frame_pointer;\
  act->node = p;\
  act->jump_stat = NULL;\
  act->proc_frame = A68_TRUE;\
  act->blocks = block_gc;\
  }
#endif

/*
Upon closing a frame we restore block_gc.
This to avoid that a RTS routine that blocks the garbage collector,
leaves it inoperative in case the routine quits through an event.
*/

#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
#define CLOSE_FRAME {\
  if (FRAME_THREAD_ID (frame_pointer) == main_thread_id) {\
    block_gc = FRAME_BLOCKS (frame_pointer);\
  }\
  frame_pointer = FRAME_DYNAMIC_LINK (frame_pointer);\
  }
#else
#define CLOSE_FRAME {\
  block_gc = FRAME_BLOCKS (frame_pointer);\
  frame_pointer = FRAME_DYNAMIC_LINK (frame_pointer);\
  }
#endif

/* Macros for check on initialisation of values */

#define CHECK_INIT(p, c, q)\
  if (!(c)) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE_FROM, (q));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }

#define SCOPE_CHECK(p, scope, limit, mode, info)\
  if (scope > limit) {\
    char txt[BUFFER_SIZE];\
    if (info == NULL) {\
      ASSERT (snprintf (txt, SNPRINTF_SIZE, ERROR_SCOPE_DYNAMIC_1) >= 0);\
    } else {\
      ASSERT (snprintf (txt, SNPRINTF_SIZE, ERROR_SCOPE_DYNAMIC_2, info) >= 0);\
    }\
    diagnostic_node (A68_RUNTIME_ERROR, p, txt, mode);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }

#define INCREMENT_STACK_POINTER(err, i) {stack_pointer += (ADDR_T) A68_ALIGN (i); (void) (err);}

#define DECREMENT_STACK_POINTER(err, i) {\
  stack_pointer -= A68_ALIGN (i);\
  (void) (err);\
  }

#define PUSH(p, addr, size) {\
  BYTE_T *_sp_ = STACK_TOP;\
  INCREMENT_STACK_POINTER ((p), (int) (size));\
  COPY (_sp_, (BYTE_T *) (addr), (int) (size));\
  }

#define PUSH_ALIGNED(p, addr, size) {\
  BYTE_T *_sp_ = STACK_TOP;\
  INCREMENT_STACK_POINTER ((p), (int) (size));\
  COPY_ALIGNED (_sp_, (BYTE_T *) (addr), (int) (size));\
  }

#define POP(p, addr, size) {\
  DECREMENT_STACK_POINTER((p), (int) (size));\
  COPY ((BYTE_T *) (addr), STACK_TOP, (int) (size));\
  }

#define POP_ALIGNED(p, addr, size) {\
  DECREMENT_STACK_POINTER((p), (int) (size));\
  COPY_ALIGNED ((BYTE_T *) (addr), STACK_TOP, (int) (size));\
  }

#define POP_ADDRESS(p, addr, type) {\
  DECREMENT_STACK_POINTER((p), (int) ALIGNED_SIZE_OF (type));\
  (addr) = (type *) STACK_TOP;\
  }

#define POP_OPERAND_ADDRESS(p, i, type) {\
  (void) (p);\
  (i) = (type *) (STACK_OFFSET (-ALIGNED_SIZE_OF (type)));\
  }

#define POP_OPERAND_ADDRESSES(p, i, j, type) {\
  DECREMENT_STACK_POINTER ((p), (int) ALIGNED_SIZE_OF (type));\
  (j) = (type *) STACK_TOP;\
  (i) = (type *) (STACK_OFFSET (-ALIGNED_SIZE_OF (type)));\
  }

#define POP_3_OPERAND_ADDRESSES(p, i, j, k, type) {\
  DECREMENT_STACK_POINTER ((p), (int) (2 * ALIGNED_SIZE_OF (type)));\
  (k) = (type *) (STACK_OFFSET (ALIGNED_SIZE_OF (type)));\
  (j) = (type *) STACK_TOP;\
  (i) = (type *) (STACK_OFFSET (-ALIGNED_SIZE_OF (type)));\
  }

#define PUSH_PRIMITIVE(p, z, mode) {\
  mode *_x_ = (mode *) STACK_TOP;\
  _x_->status = INITIALISED_MASK;\
  _x_->value = (z);\
  INCREMENT_STACK_POINTER((p), ALIGNED_SIZE_OF (mode));\
  }

#define PUSH_OBJECT(p, z, mode) {\
  *(mode *) STACK_TOP = (z);\
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (mode));\
  }

#define POP_OBJECT(p, z, mode) {\
  DECREMENT_STACK_POINTER((p), ALIGNED_SIZE_OF (mode));\
  (*(z)) = *((mode *) STACK_TOP);\
  }

#define PUSH_COMPLEX(p, re, im) {\
  PUSH_PRIMITIVE (p, re, A68_REAL);\
  PUSH_PRIMITIVE (p, im, A68_REAL);\
  }

#define POP_COMPLEX(p, re, im) {\
  POP_OBJECT (p, im, A68_REAL);\
  POP_OBJECT (p, re, A68_REAL);\
  }

#define PUSH_BYTES(p, k) {\
  A68_BYTES *_z_ = (A68_BYTES *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  strncpy (_z_->value, k, BYTES_WIDTH);\
  INCREMENT_STACK_POINTER((p), ALIGNED_SIZE_OF (A68_BYTES));\
  }

#define PUSH_LONG_BYTES(p, k) {\
  A68_LONG_BYTES *_z_ = (A68_LONG_BYTES *) STACK_TOP;\
  _z_->status = INITIALISED_MASK;\
  strncpy (_z_->value, k, LONG_BYTES_WIDTH);\
  INCREMENT_STACK_POINTER((p), ALIGNED_SIZE_OF (A68_LONG_BYTES));\
  }

#define PUSH_REF(p, z) PUSH_OBJECT (p, z, A68_REF)
#define PUSH_PROCEDURE(p, z) PUSH_OBJECT (p, z, A68_PROCEDURE)
#define PUSH_FORMAT(p, z) PUSH_OBJECT (p, z, A68_FORMAT)

#define POP_REF(p, z) POP_OBJECT (p, z, A68_REF)
#define POP_PROCEDURE(p, z) POP_OBJECT (p, z, A68_PROCEDURE)
#define POP_FORMAT(p, z) POP_OBJECT (p, z, A68_FORMAT)

#define PUSH_UNION(p, z) PUSH_PRIMITIVE (p, z, A68_UNION)

/* Macro's for standard environ */

#define A68_ENV_INT(n, k) void n (NODE_T *p) {PUSH_PRIMITIVE (p, (k), A68_INT);}
#define A68_ENV_REAL(n, z) void n (NODE_T *p) {PUSH_PRIMITIVE (p, (z), A68_REAL);}

/* Interpreter macros */

#define INITIALISED(z) ((BOOL_T) ((z)->status & INITIALISED_MASK))

#define BITS_WIDTH ((int) (1 + ceil (log ((double) A68_MAX_INT) / log((double) 2))))
#define INT_WIDTH ((int) (1 + floor (log ((double) A68_MAX_INT) / log ((double) 10))))

#define CHAR_WIDTH (1 + (int) log10 ((double) SCHAR_MAX))
#define REAL_WIDTH (DBL_DIG)
#define EXP_WIDTH ((int) (1 + log10 ((double) DBL_MAX_10_EXP)))

#define HEAP_ADDRESS(n) ((BYTE_T *) & (heap_segment[n]))

#define LHS_MODE(p) (MOID (PACK (MOID (p))))
#define RHS_MODE(p) (MOID (NEXT (PACK (MOID (p)))))

/* Activation records in the frame stack */

typedef struct ACTIVATION_RECORD ACTIVATION_RECORD;

struct ACTIVATION_RECORD
{
  ADDR_T static_link, dynamic_link, dynamic_scope, parameters;
  NODE_T *node;
  jmp_buf *jump_stat;
  BOOL_T proc_frame;
  int frame_no, frame_level, parameter_level, blocks;
#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
  pthread_t thread_id;
#endif
};

/* Stack manipulation */

#define STACK_ADDRESS(n) ((BYTE_T *) &(stack_segment[(n)]))
#define STACK_OFFSET(n) (STACK_ADDRESS (stack_pointer + (int) (n)))
#define STACK_TOP (STACK_ADDRESS (stack_pointer))

/* External symbols */

extern ADDR_T frame_pointer, stack_pointer, heap_pointer, handle_pointer, global_pointer, frame_start, frame_end, stack_start, stack_end, finish_frame_pointer;
extern A68_FORMAT nil_format;
extern A68_HANDLE nil_handle, *free_handles, *busy_handles;
extern A68_REF nil_ref;
extern BOOL_T in_monitor, do_confirm_exit;
extern MOID_T *top_expr_moid;
extern char *watchpoint_expression;
extern double cputime_0;
extern int global_level, max_lex_lvl, ret_code;
extern jmp_buf genie_exit_label, monitor_exit_label;

extern int frame_stack_size, expr_stack_size, heap_size, handle_pool_size;
extern int stack_limit, frame_stack_limit, expr_stack_limit;
extern int storage_overhead;

/* External symbols */

#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
#endif

extern int free_handle_count, max_handle_count;
extern void genie_find_proc_op (NODE_T *, int *);
extern void gc_heap (NODE_T *, ADDR_T);

extern GENIE_PROCEDURE genie_abs_bool;
extern GENIE_PROCEDURE genie_abs_char;
extern GENIE_PROCEDURE genie_abs_complex;
extern GENIE_PROCEDURE genie_abs_int;
extern GENIE_PROCEDURE genie_abs_long_complex;
extern GENIE_PROCEDURE genie_abs_long_mp;
extern GENIE_PROCEDURE genie_abs_real;
extern GENIE_PROCEDURE genie_acos_long_complex;
extern GENIE_PROCEDURE genie_acos_long_mp;
extern GENIE_PROCEDURE genie_acronym;
extern GENIE_PROCEDURE genie_add_bytes;
extern GENIE_PROCEDURE genie_add_char;
extern GENIE_PROCEDURE genie_add_complex;
extern GENIE_PROCEDURE genie_add_int;
extern GENIE_PROCEDURE genie_add_long_bytes;
extern GENIE_PROCEDURE genie_add_long_complex;
extern GENIE_PROCEDURE genie_add_long_int;
extern GENIE_PROCEDURE genie_add_long_mp;
extern GENIE_PROCEDURE genie_add_real;
extern GENIE_PROCEDURE genie_add_string;
extern GENIE_PROCEDURE genie_and_bits;
extern GENIE_PROCEDURE genie_and_bool;
extern GENIE_PROCEDURE genie_and_long_mp;
extern GENIE_PROCEDURE genie_arccosh_long_mp;
extern GENIE_PROCEDURE genie_arccos_real;
extern GENIE_PROCEDURE genie_arcsinh_long_mp;
extern GENIE_PROCEDURE genie_arcsin_real;
extern GENIE_PROCEDURE genie_arctanh_long_mp;
extern GENIE_PROCEDURE genie_arctan_real;
extern GENIE_PROCEDURE genie_arg_complex;
extern GENIE_PROCEDURE genie_arg_long_complex;
extern GENIE_PROCEDURE genie_asin_long_complex;
extern GENIE_PROCEDURE genie_asin_long_mp;
extern GENIE_PROCEDURE genie_atan2_long_mp;
extern GENIE_PROCEDURE genie_atan2_real;
extern GENIE_PROCEDURE genie_atan_long_complex;
extern GENIE_PROCEDURE genie_atan_long_mp;
extern GENIE_PROCEDURE genie_abs_bits;
extern GENIE_PROCEDURE genie_bin_int;
extern GENIE_PROCEDURE genie_bin_long_mp;
extern GENIE_PROCEDURE genie_bits_lengths;
extern GENIE_PROCEDURE genie_bits_pack;
extern GENIE_PROCEDURE genie_bits_shorths;
extern GENIE_PROCEDURE genie_bits_width;
extern GENIE_PROCEDURE genie_block;
extern GENIE_PROCEDURE genie_break;
extern GENIE_PROCEDURE genie_bytes_lengths;
extern GENIE_PROCEDURE genie_bytespack;
extern GENIE_PROCEDURE genie_bytes_shorths;
extern GENIE_PROCEDURE genie_bytes_width;
extern GENIE_PROCEDURE genie_cd;
extern GENIE_PROCEDURE genie_char_in_string;
extern GENIE_PROCEDURE genie_clear_bits;
extern GENIE_PROCEDURE genie_clear_long_bits;
extern GENIE_PROCEDURE genie_clear_longlong_bits;
extern GENIE_PROCEDURE genie_complex_lengths;
extern GENIE_PROCEDURE genie_complex_shorths;
extern GENIE_PROCEDURE genie_conj_complex;
extern GENIE_PROCEDURE genie_conj_long_complex;
extern GENIE_PROCEDURE genie_cosh_long_mp;
extern GENIE_PROCEDURE genie_cos_long_complex;
extern GENIE_PROCEDURE genie_cos_long_mp;
extern GENIE_PROCEDURE genie_cos_real;
extern GENIE_PROCEDURE genie_cputime;
extern GENIE_PROCEDURE genie_curt_long_mp;
extern GENIE_PROCEDURE genie_curt_real;
extern GENIE_PROCEDURE genie_debug;
extern GENIE_PROCEDURE genie_directory;
extern GENIE_PROCEDURE genie_divab_complex;
extern GENIE_PROCEDURE genie_divab_long_complex;
extern GENIE_PROCEDURE genie_divab_long_mp;
extern GENIE_PROCEDURE genie_divab_real;
extern GENIE_PROCEDURE genie_div_complex;
extern GENIE_PROCEDURE genie_div_int;
extern GENIE_PROCEDURE genie_div_long_complex;
extern GENIE_PROCEDURE genie_div_long_mp;
extern GENIE_PROCEDURE genie_div_real;
extern GENIE_PROCEDURE genie_dyad_elems;
extern GENIE_PROCEDURE genie_dyad_lwb;
extern GENIE_PROCEDURE genie_dyad_upb;
extern GENIE_PROCEDURE genie_elem_bits;
extern GENIE_PROCEDURE genie_elem_bytes;
extern GENIE_PROCEDURE genie_elem_long_bits;
extern GENIE_PROCEDURE genie_elem_long_bits;
extern GENIE_PROCEDURE genie_elem_long_bytes;
extern GENIE_PROCEDURE genie_elem_longlong_bits;
extern GENIE_PROCEDURE genie_elem_string;
extern GENIE_PROCEDURE genie_entier_long_mp;
extern GENIE_PROCEDURE genie_entier_real;
extern GENIE_PROCEDURE genie_eq_bits;
extern GENIE_PROCEDURE genie_eq_bool;
extern GENIE_PROCEDURE genie_eq_bytes;
extern GENIE_PROCEDURE genie_eq_char;
extern GENIE_PROCEDURE genie_eq_complex;
extern GENIE_PROCEDURE genie_eq_int;
extern GENIE_PROCEDURE genie_eq_long_bytes;
extern GENIE_PROCEDURE genie_eq_long_complex;
extern GENIE_PROCEDURE genie_eq_long_mp;
extern GENIE_PROCEDURE genie_eq_real;
extern GENIE_PROCEDURE genie_eq_string;
extern GENIE_PROCEDURE genie_evaluate;
extern GENIE_PROCEDURE genie_exp_long_complex;
extern GENIE_PROCEDURE genie_exp_long_mp;
extern GENIE_PROCEDURE genie_exp_real;
extern GENIE_PROCEDURE genie_exp_width;
extern GENIE_PROCEDURE genie_file_is_block_device;
extern GENIE_PROCEDURE genie_file_is_char_device;
extern GENIE_PROCEDURE genie_file_is_directory;
extern GENIE_PROCEDURE genie_file_is_regular;
extern GENIE_PROCEDURE genie_file_mode;
extern GENIE_PROCEDURE genie_first_random;
extern GENIE_PROCEDURE genie_garbage_collections;
extern GENIE_PROCEDURE genie_garbage_freed;
extern GENIE_PROCEDURE genie_garbage_seconds;
extern GENIE_PROCEDURE genie_ge_bits;
extern GENIE_PROCEDURE genie_ge_bytes;
extern GENIE_PROCEDURE genie_ge_char;
extern GENIE_PROCEDURE genie_ge_int;
extern GENIE_PROCEDURE genie_ge_long_bits;
extern GENIE_PROCEDURE genie_ge_long_bytes;
extern GENIE_PROCEDURE genie_ge_long_mp;
extern GENIE_PROCEDURE genie_ge_real;
extern GENIE_PROCEDURE genie_ge_string;
extern GENIE_PROCEDURE genie_get_sound;
extern GENIE_PROCEDURE genie_gt_bytes;
extern GENIE_PROCEDURE genie_gt_char;
extern GENIE_PROCEDURE genie_gt_int;
extern GENIE_PROCEDURE genie_gt_long_bytes;
extern GENIE_PROCEDURE genie_gt_long_mp;
extern GENIE_PROCEDURE genie_gt_real;
extern GENIE_PROCEDURE genie_gt_string;
extern GENIE_PROCEDURE genie_icomplex;
extern GENIE_PROCEDURE genie_idf;
extern GENIE_PROCEDURE genie_idle;
extern GENIE_PROCEDURE genie_iint_complex;
extern GENIE_PROCEDURE genie_im_complex;
extern GENIE_PROCEDURE genie_im_long_complex;
extern GENIE_PROCEDURE genie_int_lengths;
extern GENIE_PROCEDURE genie_int_shorths;
extern GENIE_PROCEDURE genie_int_width;
extern GENIE_PROCEDURE genie_is_alnum;
extern GENIE_PROCEDURE genie_is_alpha;
extern GENIE_PROCEDURE genie_is_cntrl;
extern GENIE_PROCEDURE genie_is_digit;
extern GENIE_PROCEDURE genie_is_graph;
extern GENIE_PROCEDURE genie_is_lower;
extern GENIE_PROCEDURE genie_is_print;
extern GENIE_PROCEDURE genie_is_punct;
extern GENIE_PROCEDURE genie_is_space;
extern GENIE_PROCEDURE genie_is_upper;
extern GENIE_PROCEDURE genie_is_xdigit;
extern GENIE_PROCEDURE genie_last_char_in_string;
extern GENIE_PROCEDURE genie_le_bits;
extern GENIE_PROCEDURE genie_le_bytes;
extern GENIE_PROCEDURE genie_le_char;
extern GENIE_PROCEDURE genie_le_int;
extern GENIE_PROCEDURE genie_le_long_bits;
extern GENIE_PROCEDURE genie_le_long_bytes;
extern GENIE_PROCEDURE genie_le_long_mp;
extern GENIE_PROCEDURE genie_leng_bytes;
extern GENIE_PROCEDURE genie_lengthen_complex_to_long_complex;
extern GENIE_PROCEDURE genie_lengthen_int_to_long_mp;
extern GENIE_PROCEDURE genie_lengthen_long_complex_to_longlong_complex;
extern GENIE_PROCEDURE genie_lengthen_long_mp_to_longlong_mp;
extern GENIE_PROCEDURE genie_lengthen_real_to_long_mp;
extern GENIE_PROCEDURE genie_lengthen_unsigned_to_long_mp;
extern GENIE_PROCEDURE genie_le_real;
extern GENIE_PROCEDURE genie_le_string;
extern GENIE_PROCEDURE genie_lj_e_12_6;
extern GENIE_PROCEDURE genie_lj_f_12_6;
extern GENIE_PROCEDURE genie_ln_long_complex;
extern GENIE_PROCEDURE genie_ln_long_mp;
extern GENIE_PROCEDURE genie_ln_real;
extern GENIE_PROCEDURE genie_log_long_mp;
extern GENIE_PROCEDURE genie_log_real;
extern GENIE_PROCEDURE genie_long_bits_pack;
extern GENIE_PROCEDURE genie_long_bits_width;
extern GENIE_PROCEDURE genie_long_bytespack;
extern GENIE_PROCEDURE genie_long_bytes_width;
extern GENIE_PROCEDURE genie_long_exp_width;
extern GENIE_PROCEDURE genie_long_int_width;
extern GENIE_PROCEDURE genie_longlong_bits_width;
extern GENIE_PROCEDURE genie_longlong_exp_width;
extern GENIE_PROCEDURE genie_longlong_int_width;
extern GENIE_PROCEDURE genie_longlong_max_bits;
extern GENIE_PROCEDURE genie_longlong_max_int;
extern GENIE_PROCEDURE genie_longlong_max_real;
extern GENIE_PROCEDURE genie_longlong_min_real;
extern GENIE_PROCEDURE genie_longlong_real_width;
extern GENIE_PROCEDURE genie_longlong_small_real;
extern GENIE_PROCEDURE genie_long_max_bits;
extern GENIE_PROCEDURE genie_long_max_int;
extern GENIE_PROCEDURE genie_long_max_real;
extern GENIE_PROCEDURE genie_long_min_real;
extern GENIE_PROCEDURE genie_long_next_random;
extern GENIE_PROCEDURE genie_long_real_width;
extern GENIE_PROCEDURE genie_long_small_real;
extern GENIE_PROCEDURE genie_lt_bytes;
extern GENIE_PROCEDURE genie_lt_char;
extern GENIE_PROCEDURE genie_lt_int;
extern GENIE_PROCEDURE genie_lt_long_bytes;
extern GENIE_PROCEDURE genie_lt_long_mp;
extern GENIE_PROCEDURE genie_lt_real;
extern GENIE_PROCEDURE genie_lt_string;
extern GENIE_PROCEDURE genie_make_term;
extern GENIE_PROCEDURE genie_max_abs_char;
extern GENIE_PROCEDURE genie_max_bits;
extern GENIE_PROCEDURE genie_max_int;
extern GENIE_PROCEDURE genie_max_real;
extern GENIE_PROCEDURE genie_min_real;
extern GENIE_PROCEDURE genie_minusab_complex;
extern GENIE_PROCEDURE genie_minusab_int;
extern GENIE_PROCEDURE genie_minusab_long_complex;
extern GENIE_PROCEDURE genie_minusab_long_int;
extern GENIE_PROCEDURE genie_minusab_long_mp;
extern GENIE_PROCEDURE genie_minusab_real;
extern GENIE_PROCEDURE genie_minus_complex;
extern GENIE_PROCEDURE genie_minus_int;
extern GENIE_PROCEDURE genie_minus_long_complex;
extern GENIE_PROCEDURE genie_sub_long_int;
extern GENIE_PROCEDURE genie_minus_long_mp;
extern GENIE_PROCEDURE genie_minus_real;
extern GENIE_PROCEDURE genie_modab_int;
extern GENIE_PROCEDURE genie_modab_long_mp;
extern GENIE_PROCEDURE genie_mod_int;
extern GENIE_PROCEDURE genie_mod_long_mp;
extern GENIE_PROCEDURE genie_monad_elems;
extern GENIE_PROCEDURE genie_monad_lwb;
extern GENIE_PROCEDURE genie_monad_upb;
extern GENIE_PROCEDURE genie_mul_complex;
extern GENIE_PROCEDURE genie_mul_int;
extern GENIE_PROCEDURE genie_mul_long_complex;
extern GENIE_PROCEDURE genie_mul_long_int;
extern GENIE_PROCEDURE genie_mul_long_mp;
extern GENIE_PROCEDURE genie_mul_real;
extern GENIE_PROCEDURE genie_ne_bits;
extern GENIE_PROCEDURE genie_ne_bool;
extern GENIE_PROCEDURE genie_ne_bytes;
extern GENIE_PROCEDURE genie_ne_char;
extern GENIE_PROCEDURE genie_ne_complex;
extern GENIE_PROCEDURE genie_ne_int;
extern GENIE_PROCEDURE genie_ne_long_bytes;
extern GENIE_PROCEDURE genie_ne_long_complex;
extern GENIE_PROCEDURE genie_ne_long_mp;
extern GENIE_PROCEDURE genie_ne_real;
extern GENIE_PROCEDURE genie_ne_string;
extern GENIE_PROCEDURE genie_new_sound;
extern GENIE_PROCEDURE genie_next_random;
extern GENIE_PROCEDURE genie_not_bits;
extern GENIE_PROCEDURE genie_not_bool;
extern GENIE_PROCEDURE genie_not_long_mp;
extern GENIE_PROCEDURE genie_null_char;
extern GENIE_PROCEDURE genie_odd_int;
extern GENIE_PROCEDURE genie_odd_long_mp;
extern GENIE_PROCEDURE genie_or_bits;
extern GENIE_PROCEDURE genie_or_bool;
extern GENIE_PROCEDURE genie_or_long_mp;
extern GENIE_PROCEDURE genie_overab_int;
extern GENIE_PROCEDURE genie_overab_long_mp;
extern GENIE_PROCEDURE genie_over_int;
extern GENIE_PROCEDURE genie_over_long_mp;
extern GENIE_PROCEDURE genie_pi;
extern GENIE_PROCEDURE genie_pi_long_mp;
extern GENIE_PROCEDURE genie_plusab_bytes;
extern GENIE_PROCEDURE genie_plusab_complex;
extern GENIE_PROCEDURE genie_plusab_int;
extern GENIE_PROCEDURE genie_plusab_long_bytes;
extern GENIE_PROCEDURE genie_plusab_long_complex;
extern GENIE_PROCEDURE genie_plusab_long_int;
extern GENIE_PROCEDURE genie_plusab_long_mp;
extern GENIE_PROCEDURE genie_plusab_real;
extern GENIE_PROCEDURE genie_plusab_string;
extern GENIE_PROCEDURE genie_plusto_bytes;
extern GENIE_PROCEDURE genie_plusto_long_bytes;
extern GENIE_PROCEDURE genie_plusto_string;
extern GENIE_PROCEDURE genie_pow_complex_int;
extern GENIE_PROCEDURE genie_pow_int;
extern GENIE_PROCEDURE genie_pow_long_complex_int;
extern GENIE_PROCEDURE genie_pow_long_mp;
extern GENIE_PROCEDURE genie_pow_long_mp_int;
extern GENIE_PROCEDURE genie_pow_long_mp_int_int;
extern GENIE_PROCEDURE genie_pow_real;
extern GENIE_PROCEDURE genie_pow_real_int;
extern GENIE_PROCEDURE genie_preemptive_gc_heap;
extern GENIE_PROCEDURE genie_program_idf;
extern GENIE_PROCEDURE genie_pwd;
extern GENIE_PROCEDURE genie_real_lengths;
extern GENIE_PROCEDURE genie_real_shorths;
extern GENIE_PROCEDURE genie_real_width;
extern GENIE_PROCEDURE genie_re_complex;
extern GENIE_PROCEDURE genie_re_long_complex;
extern GENIE_PROCEDURE genie_repr_char;
extern GENIE_PROCEDURE genie_round_long_mp;
extern GENIE_PROCEDURE genie_round_real;
extern GENIE_PROCEDURE genie_set_bits;
extern GENIE_PROCEDURE genie_set_long_bits;
extern GENIE_PROCEDURE genie_set_longlong_bits;
extern GENIE_PROCEDURE genie_set_sound;
extern GENIE_PROCEDURE genie_shl_bits;
extern GENIE_PROCEDURE genie_shl_long_mp;
extern GENIE_PROCEDURE genie_shorten_bytes;
extern GENIE_PROCEDURE genie_shorten_long_complex_to_complex;
extern GENIE_PROCEDURE genie_shorten_longlong_complex_to_long_complex;
extern GENIE_PROCEDURE genie_shorten_longlong_mp_to_long_mp;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_bits;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_int;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_real;
extern GENIE_PROCEDURE genie_shr_bits;
extern GENIE_PROCEDURE genie_shr_long_mp;
extern GENIE_PROCEDURE genie_sign_int;
extern GENIE_PROCEDURE genie_sign_long_mp;
extern GENIE_PROCEDURE genie_sign_real;
extern GENIE_PROCEDURE genie_sinh_long_mp;
extern GENIE_PROCEDURE genie_sin_long_complex;
extern GENIE_PROCEDURE genie_sin_long_mp;
extern GENIE_PROCEDURE genie_sin_real;
extern GENIE_PROCEDURE genie_small_real;
extern GENIE_PROCEDURE genie_sort_row_string;
extern GENIE_PROCEDURE genie_sound_channels;
extern GENIE_PROCEDURE genie_sound_rate;
extern GENIE_PROCEDURE genie_sound_resolution;
extern GENIE_PROCEDURE genie_sound_samples;
extern GENIE_PROCEDURE genie_sqrt_long_complex;
extern GENIE_PROCEDURE genie_sqrt_long_mp;
extern GENIE_PROCEDURE genie_sqrt_real;
extern GENIE_PROCEDURE genie_stack_pointer;
extern GENIE_PROCEDURE genie_stand_back;
extern GENIE_PROCEDURE genie_stand_back_channel;
extern GENIE_PROCEDURE genie_stand_draw_channel;
extern GENIE_PROCEDURE genie_stand_error;
extern GENIE_PROCEDURE genie_stand_error_channel;
extern GENIE_PROCEDURE genie_stand_in;
extern GENIE_PROCEDURE genie_stand_in_channel;
extern GENIE_PROCEDURE genie_stand_out;
extern GENIE_PROCEDURE genie_stand_out_channel;
extern GENIE_PROCEDURE genie_string_in_string;
extern GENIE_PROCEDURE genie_sub_complex;
extern GENIE_PROCEDURE genie_sub_int;
extern GENIE_PROCEDURE genie_sub_long_complex;
extern GENIE_PROCEDURE genie_sub_long_mp;
extern GENIE_PROCEDURE genie_sub_real;
extern GENIE_PROCEDURE genie_gc_heap;
extern GENIE_PROCEDURE genie_system;
extern GENIE_PROCEDURE genie_system_stack_pointer;
extern GENIE_PROCEDURE genie_system_stack_size;
extern GENIE_PROCEDURE genie_tanh_long_mp;
extern GENIE_PROCEDURE genie_tan_long_complex;
extern GENIE_PROCEDURE genie_tan_long_mp;
extern GENIE_PROCEDURE genie_tan_real;
extern GENIE_PROCEDURE genie_term;
extern GENIE_PROCEDURE genie_timesab_complex;
extern GENIE_PROCEDURE genie_timesab_int;
extern GENIE_PROCEDURE genie_timesab_long_complex;
extern GENIE_PROCEDURE genie_timesab_long_int;
extern GENIE_PROCEDURE genie_timesab_long_mp;
extern GENIE_PROCEDURE genie_timesab_real;
extern GENIE_PROCEDURE genie_timesab_string;
extern GENIE_PROCEDURE genie_times_char_int;
extern GENIE_PROCEDURE genie_times_int_char;
extern GENIE_PROCEDURE genie_times_int_string;
extern GENIE_PROCEDURE genie_times_string_int;
extern GENIE_PROCEDURE genie_to_lower;
extern GENIE_PROCEDURE genie_to_upper;
extern GENIE_PROCEDURE genie_unimplemented;
extern GENIE_PROCEDURE genie_vector_times_scalar;
extern GENIE_PROCEDURE genie_xor_bits;
extern GENIE_PROCEDURE genie_xor_bool;
extern GENIE_PROCEDURE genie_xor_long_mp;

extern PROPAGATOR_T genie_and_function (NODE_T *p);
extern PROPAGATOR_T genie_assertion (NODE_T *p);
extern PROPAGATOR_T genie_assignation_constant (NODE_T *p);
extern PROPAGATOR_T genie_assignation (NODE_T *p);
extern PROPAGATOR_T genie_call (NODE_T *p);
extern PROPAGATOR_T genie_cast (NODE_T *p);
extern PROPAGATOR_T genie_closed (volatile NODE_T *p);
extern PROPAGATOR_T genie_coercion (NODE_T *p);
extern PROPAGATOR_T genie_collateral (NODE_T *p);
extern PROPAGATOR_T genie_column_function (NODE_T *p);
extern PROPAGATOR_T genie_conditional (volatile NODE_T *p);
extern PROPAGATOR_T genie_constant (NODE_T *p);
extern PROPAGATOR_T genie_denotation (NODE_T *p);
extern PROPAGATOR_T genie_deproceduring (NODE_T *p);
extern PROPAGATOR_T genie_dereference_frame_identifier (NODE_T *p);
extern PROPAGATOR_T genie_dereference_generic_identifier (NODE_T *p);
extern PROPAGATOR_T genie_dereference_selection_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_dereference_slice_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_dereferencing (NODE_T *p);
extern PROPAGATOR_T genie_dereferencing_quick (NODE_T *p);
extern PROPAGATOR_T genie_diagonal_function (NODE_T *p);
extern PROPAGATOR_T genie_dyadic (NODE_T *p);
extern PROPAGATOR_T genie_dyadic_quick (NODE_T *p);
extern PROPAGATOR_T genie_enclosed (volatile NODE_T *p);
extern PROPAGATOR_T genie_field_selection (NODE_T *p);
extern PROPAGATOR_T genie_format_text (NODE_T *p);
extern PROPAGATOR_T genie_formula (NODE_T *p);
extern PROPAGATOR_T genie_generator (NODE_T *p);
extern PROPAGATOR_T genie_identifier (NODE_T *p);
extern PROPAGATOR_T genie_identifier_standenv (NODE_T *p);
extern PROPAGATOR_T genie_identifier_standenv_proc (NODE_T *p);
extern PROPAGATOR_T genie_identity_relation (NODE_T *p);
extern PROPAGATOR_T genie_int_case (volatile NODE_T *p);
extern PROPAGATOR_T genie_frame_identifier (NODE_T *p);
extern PROPAGATOR_T genie_loop (volatile NODE_T *p);
extern PROPAGATOR_T genie_monadic (NODE_T *p);
extern PROPAGATOR_T genie_nihil (NODE_T *p);
extern PROPAGATOR_T genie_or_function (NODE_T *p);
extern PROPAGATOR_T genie_routine_text (NODE_T *p);
extern PROPAGATOR_T genie_row_function (NODE_T *p);
extern PROPAGATOR_T genie_rowing (NODE_T *p);
extern PROPAGATOR_T genie_rowing_ref_row_of_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_ref_row_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_row_of_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_row_row (NODE_T *p);
extern PROPAGATOR_T genie_selection_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_selection (NODE_T *p);
extern PROPAGATOR_T genie_selection_value_quick (NODE_T *p);
extern PROPAGATOR_T genie_skip (NODE_T *p);
extern PROPAGATOR_T genie_slice_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_slice (NODE_T *p);
extern PROPAGATOR_T genie_transpose_function (NODE_T *p);
extern PROPAGATOR_T genie_united_case (volatile NODE_T *p);
extern PROPAGATOR_T genie_uniting (NODE_T *p);
extern PROPAGATOR_T genie_unit (NODE_T *p);
extern PROPAGATOR_T genie_voiding_assignation_constant (NODE_T *p);
extern PROPAGATOR_T genie_voiding_assignation (NODE_T *p);
extern PROPAGATOR_T genie_voiding (NODE_T *p);
extern PROPAGATOR_T genie_widening_int_to_real (NODE_T *p);
extern PROPAGATOR_T genie_widening (NODE_T *p);

extern A68_REF c_string_to_row_char (NODE_T *, char *, int);
extern A68_REF c_to_a_string (NODE_T *, char *);
extern A68_REF empty_row (NODE_T *, MOID_T *);
extern A68_REF empty_string (NODE_T *);
extern A68_REF genie_allocate_declarer (NODE_T *, ADDR_T *, A68_REF, BOOL_T);
extern A68_REF genie_assign_stowed (A68_REF, A68_REF *, NODE_T *, MOID_T *);
extern A68_REF genie_concatenate_rows (NODE_T *, MOID_T *, int, ADDR_T);
extern A68_REF genie_copy_stowed (A68_REF, NODE_T *, MOID_T *);
extern A68_REF genie_make_ref_row_of_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
extern A68_REF genie_make_ref_row_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
extern A68_REF genie_make_row (NODE_T *, MOID_T *, int, ADDR_T);
extern BOOL_T close_device (NODE_T *, A68_FILE *);
extern BOOL_T genie_int_case_unit (NODE_T *, int, int *);
extern BOOL_T genie_no_user_symbols (SYMBOL_TABLE_T *);
extern BOOL_T genie_string_to_value_internal (NODE_T *, MOID_T *, char *, BYTE_T *);
extern BOOL_T genie_united_case_unit (NODE_T *, MOID_T *);
extern char *a_to_c_string (NODE_T *, char *, A68_REF);
extern char *genie_standard_format (NODE_T *, MOID_T *, void *);
extern double inverf (double);
extern double inverfc (double);
extern double rng_53_bit (void);
extern int a68_string_size (NODE_T *, A68_REF);
extern int iabs (int);
extern int isign (int);
extern NODE_T *last_unit;
extern void change_breakpoints (NODE_T *, unsigned, int, BOOL_T *, char *);
extern void change_masks (NODE_T *, unsigned, BOOL_T);
extern void dump_frame (int);
extern void dump_stack (int);
extern void exit_genie (NODE_T *, int);
extern void free_genie_heap (NODE_T *);
extern void genie_call_operator (NODE_T *, ADDR_T);
extern void genie_call_procedure (NODE_T *, MOID_T *, MOID_T *, MOID_T *, A68_PROCEDURE *, ADDR_T, ADDR_T);
extern void genie_check_initialisation (NODE_T *, BYTE_T *, MOID_T *);
extern void genie_copy_sound (NODE_T *, BYTE_T *, BYTE_T *);
extern void genie_declaration (NODE_T *);
extern void genie_dump_frames (void);
extern void genie_enquiry_clause (NODE_T *);
extern void genie_f_and_becomes (NODE_T *, MOID_T *, GENIE_PROCEDURE *);
extern void genie_generator_bounds (NODE_T *);
extern void genie_generator_internal (NODE_T *, MOID_T *, TAG_T *, LEAP_T, ADDR_T);
extern void genie_init_lib (NODE_T *);
extern void genie (void *);
extern void genie_prepare_declarer (NODE_T *);
extern void genie_push_undefined (NODE_T *, MOID_T *);
extern void genie_serial_clause (NODE_T *, jmp_buf *);
extern void genie_serial_units (NODE_T *, NODE_T **, jmp_buf *, int);
extern void genie_subscript_linear (NODE_T *, ADDR_T *, int *);
extern void genie_subscript (NODE_T *, A68_TUPLE **, int *, NODE_T **);
extern void get_global_pointer (NODE_T *);
extern void genie_variable_dec (NODE_T *, NODE_T **, ADDR_T);
extern void genie_identity_dec (NODE_T *);
extern void genie_proc_variable_dec (NODE_T *);
extern void genie_operator_dec (NODE_T *);
extern void initialise_frame (NODE_T *);
extern void init_rng (unsigned long);
extern void install_signal_handlers (void);
extern void show_data_item (FILE_T, NODE_T *, MOID_T *, BYTE_T *);
extern void show_frame (NODE_T *, FILE_T);
extern void single_step (NODE_T *, unsigned);
extern void stack_dump (FILE_T, ADDR_T, int, int *);
extern void stack_dump_light (ADDR_T);
extern void un_init_frame (NODE_T *);

extern void genie_argc (NODE_T *);
extern void genie_argv (NODE_T *);
extern void genie_create_pipe (NODE_T *);
extern void genie_utctime (NODE_T *);
extern void genie_localtime (NODE_T *);
extern void genie_errno (NODE_T *);
extern void genie_execve (NODE_T *);
extern void genie_execve_child (NODE_T *);
extern void genie_execve_child_pipe (NODE_T *);
extern void genie_execve_output (NODE_T *);
extern void genie_fork (NODE_T *);
extern void genie_getenv (NODE_T *);
extern void genie_reset_errno (NODE_T *);
extern void genie_strerror (NODE_T *);
extern void genie_waitpid (NODE_T *);

#if defined __S_IFIFO
extern GENIE_PROCEDURE genie_file_is_fifo;
#endif

#if defined __S_IFLNK
extern GENIE_PROCEDURE genie_file_is_link;
#endif

#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
extern pthread_t main_thread_id;

extern BOOL_T whether_main_thread (void);
extern GENIE_PROCEDURE genie_down_sema;
extern GENIE_PROCEDURE genie_level_int_sema;
extern GENIE_PROCEDURE genie_level_sema_int;
extern GENIE_PROCEDURE genie_up_sema;
extern PROPAGATOR_T genie_parallel (NODE_T *p);
extern void count_par_clauses (NODE_T *);
extern void genie_abend_all_threads (NODE_T *, jmp_buf *, NODE_T *);
extern void genie_set_exit_from_threads (int);
#endif

#if defined HAVE_HTTP
extern void genie_http_content (NODE_T *);
extern void genie_tcp_request (NODE_T *);
#endif

#if defined HAVE_REGEX_H
extern void genie_grep_in_string (NODE_T *);
extern void genie_sub_in_string (NODE_T *);
#endif

#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES)
extern BOOL_T a68g_curses_mode;
extern void genie_curses_clear (NODE_T *);
extern void genie_curses_columns (NODE_T *);
extern void genie_curses_end (NODE_T *);
extern void genie_curses_getchar (NODE_T *);
extern void genie_curses_lines (NODE_T *);
extern void genie_curses_move (NODE_T *);
extern void genie_curses_putchar (NODE_T *);
extern void genie_curses_refresh (NODE_T *);
extern void genie_curses_start (NODE_T *);
#endif

#if (defined HAVE_LIBPQ_FE_H && defined HAVE_LIBPQ)
extern void genie_pq_backendpid (NODE_T *);
extern void genie_pq_cmdstatus (NODE_T *);
extern void genie_pq_cmdtuples (NODE_T *);
extern void genie_pq_connectdb (NODE_T *);
extern void genie_pq_db (NODE_T *);
extern void genie_pq_errormessage (NODE_T *);
extern void genie_pq_exec (NODE_T *);
extern void genie_pq_fformat (NODE_T *);
extern void genie_pq_finish (NODE_T *);
extern void genie_pq_fname (NODE_T *);
extern void genie_pq_fnumber (NODE_T *);
extern void genie_pq_getisnull (NODE_T *);
extern void genie_pq_getvalue (NODE_T *);
extern void genie_pq_host (NODE_T *);
extern void genie_pq_nfields (NODE_T *);
extern void genie_pq_ntuples (NODE_T *);
extern void genie_pq_options (NODE_T *);
extern void genie_pq_parameterstatus (NODE_T *);
extern void genie_pq_pass (NODE_T *);
extern void genie_pq_port (NODE_T *);
extern void genie_pq_protocolversion (NODE_T *);
extern void genie_pq_reset (NODE_T *);
extern void genie_pq_resulterrormessage (NODE_T *);
extern void genie_pq_serverversion (NODE_T *);
extern void genie_pq_socket (NODE_T *);
extern void genie_pq_tty (NODE_T *);
extern void genie_pq_user (NODE_T *);
#endif

/* Transput related definitions */

#define TRANSPUT_BUFFER_SIZE BUFFER_SIZE
#define ITEM_NOT_USED (-1)
#define EMBEDDED_FORMAT A68_TRUE
#define NOT_EMBEDDED_FORMAT A68_FALSE
#define WANT_PATTERN A68_TRUE
#define SKIP_PATTERN A68_FALSE

#define IS_NIL_FORMAT(f) ((BOOL_T) (BODY (f) == NULL && ENVIRON (f) == 0))
#define NON_TERM(p) (find_non_terminal (top_non_terminal, ATTRIBUTE (p)))

#undef DEBUG

#define DIGIT_NORMAL ((unsigned) 0x1)
#define DIGIT_BLANK ((unsigned) 0x2)

#define INSERTION_NORMAL ((unsigned) 0x10)
#define INSERTION_BLANK ((unsigned) 0x20)

/* Some OS's open only 64 files */
#define MAX_TRANSPUT_BUFFER 64

enum
{ INPUT_BUFFER = 0, OUTPUT_BUFFER, EDIT_BUFFER, UNFORMATTED_BUFFER, 
  FORMATTED_BUFFER, DOMAIN_BUFFER, PATH_BUFFER, REQUEST_BUFFER, 
  CONTENT_BUFFER, STRING_BUFFER, PATTERN_BUFFER, REPLACE_BUFFER, 
  READLINE_BUFFER, FIXED_TRANSPUT_BUFFERS
};

extern A68_REF stand_in, stand_out, skip_file;
extern A68_CHANNEL stand_in_channel, stand_out_channel, stand_draw_channel, stand_back_channel, stand_error_channel, associate_channel, skip_channel;

extern GENIE_PROCEDURE genie_associate;
extern GENIE_PROCEDURE genie_backspace;
extern GENIE_PROCEDURE genie_bin_possible;
extern GENIE_PROCEDURE genie_blank_char;
extern GENIE_PROCEDURE genie_close;
extern GENIE_PROCEDURE genie_compressible;
extern GENIE_PROCEDURE genie_create;
extern GENIE_PROCEDURE genie_draw_possible;
extern GENIE_PROCEDURE genie_erase;
extern GENIE_PROCEDURE genie_real;
extern GENIE_PROCEDURE genie_eof;
extern GENIE_PROCEDURE genie_eoln;
extern GENIE_PROCEDURE genie_error_char;
extern GENIE_PROCEDURE genie_establish;
extern GENIE_PROCEDURE genie_exp_char;
extern GENIE_PROCEDURE genie_fixed;
extern GENIE_PROCEDURE genie_flip_char;
extern GENIE_PROCEDURE genie_float;
extern GENIE_PROCEDURE genie_flop_char;
extern GENIE_PROCEDURE genie_formfeed_char;
extern GENIE_PROCEDURE genie_get_possible;
extern GENIE_PROCEDURE genie_init_transput;
extern GENIE_PROCEDURE genie_lock;
extern GENIE_PROCEDURE genie_new_line;
extern GENIE_PROCEDURE genie_new_page;
extern GENIE_PROCEDURE genie_newline_char;
extern GENIE_PROCEDURE genie_on_file_end;
extern GENIE_PROCEDURE genie_on_format_end;
extern GENIE_PROCEDURE genie_on_format_error;
extern GENIE_PROCEDURE genie_on_line_end;
extern GENIE_PROCEDURE genie_on_open_error;
extern GENIE_PROCEDURE genie_on_page_end;
extern GENIE_PROCEDURE genie_on_transput_error;
extern GENIE_PROCEDURE genie_on_value_error;
extern GENIE_PROCEDURE genie_open;
extern GENIE_PROCEDURE genie_print_bits;
extern GENIE_PROCEDURE genie_print_bool;
extern GENIE_PROCEDURE genie_print_char;
extern GENIE_PROCEDURE genie_print_complex;
extern GENIE_PROCEDURE genie_print_int;
extern GENIE_PROCEDURE genie_print_long_bits;
extern GENIE_PROCEDURE genie_print_long_complex;
extern GENIE_PROCEDURE genie_print_long_int;
extern GENIE_PROCEDURE genie_print_long_real;
extern GENIE_PROCEDURE genie_print_longlong_bits;
extern GENIE_PROCEDURE genie_print_longlong_complex;
extern GENIE_PROCEDURE genie_print_longlong_int;
extern GENIE_PROCEDURE genie_print_longlong_real;
extern GENIE_PROCEDURE genie_print_real;
extern GENIE_PROCEDURE genie_print_string;
extern GENIE_PROCEDURE genie_put_possible;
extern GENIE_PROCEDURE genie_read;
extern GENIE_PROCEDURE genie_read_bin;
extern GENIE_PROCEDURE genie_read_bin_file;
extern GENIE_PROCEDURE genie_read_bits;
extern GENIE_PROCEDURE genie_read_bool;
extern GENIE_PROCEDURE genie_read_char;
extern GENIE_PROCEDURE genie_read_complex;
extern GENIE_PROCEDURE genie_read_file;
extern GENIE_PROCEDURE genie_read_file_format;
extern GENIE_PROCEDURE genie_read_format;
extern GENIE_PROCEDURE genie_read_int;
extern GENIE_PROCEDURE genie_read_long_bits;
extern GENIE_PROCEDURE genie_read_long_complex;
extern GENIE_PROCEDURE genie_read_long_int;
extern GENIE_PROCEDURE genie_read_long_real;
extern GENIE_PROCEDURE genie_read_longlong_bits;
extern GENIE_PROCEDURE genie_read_longlong_complex;
extern GENIE_PROCEDURE genie_read_longlong_int;
extern GENIE_PROCEDURE genie_read_longlong_real;
extern GENIE_PROCEDURE genie_read_real;
extern GENIE_PROCEDURE genie_read_string;
extern GENIE_PROCEDURE genie_reidf_possible;
extern GENIE_PROCEDURE genie_reset;
extern GENIE_PROCEDURE genie_reset_possible;
extern GENIE_PROCEDURE genie_set;
extern GENIE_PROCEDURE genie_set_possible;
extern GENIE_PROCEDURE genie_space;
extern GENIE_PROCEDURE genie_tab_char;
extern GENIE_PROCEDURE genie_whole;
extern GENIE_PROCEDURE genie_write;
extern GENIE_PROCEDURE genie_write_bin;
extern GENIE_PROCEDURE genie_write_bin_file;
extern GENIE_PROCEDURE genie_write_file;
extern GENIE_PROCEDURE genie_write_file_format;
extern GENIE_PROCEDURE genie_write_format;

#if (defined HAVE_PLOT_H && defined HAVE_LIBPLOT)
extern GENIE_PROCEDURE genie_draw_aspect;
extern GENIE_PROCEDURE genie_draw_atom;
extern GENIE_PROCEDURE genie_draw_background_colour;
extern GENIE_PROCEDURE genie_draw_background_colour_name;
extern GENIE_PROCEDURE genie_draw_circle;
extern GENIE_PROCEDURE genie_draw_clear;
extern GENIE_PROCEDURE genie_draw_colour;
extern GENIE_PROCEDURE genie_draw_colour_name;
extern GENIE_PROCEDURE genie_draw_fillstyle;
extern GENIE_PROCEDURE genie_draw_fontname;
extern GENIE_PROCEDURE genie_draw_fontsize;
extern GENIE_PROCEDURE genie_draw_get_colour_name;
extern GENIE_PROCEDURE genie_draw_line;
extern GENIE_PROCEDURE genie_draw_linestyle;
extern GENIE_PROCEDURE genie_draw_linewidth;
extern GENIE_PROCEDURE genie_draw_move;
extern GENIE_PROCEDURE genie_draw_point;
extern GENIE_PROCEDURE genie_draw_rect;
extern GENIE_PROCEDURE genie_draw_show;
extern GENIE_PROCEDURE genie_draw_star;
extern GENIE_PROCEDURE genie_draw_text;
extern GENIE_PROCEDURE genie_draw_textangle;
extern GENIE_PROCEDURE genie_make_device;
#endif

extern FILE_T open_physical_file (NODE_T *, A68_REF, int, mode_t);
extern NODE_T *get_next_format_pattern (NODE_T *, A68_REF, BOOL_T);
extern char *error_chars (char *, int);
extern char *fixed (NODE_T * p);
extern char *fleet (NODE_T * p);
extern char *get_transput_buffer (int);
extern char *stack_string (NODE_T *, int);
extern char *sub_fixed (NODE_T *, double, int, int);
extern char *sub_whole (NODE_T *, int, int);
extern char *whole (NODE_T * p);
extern char get_flip_char (void);
extern char get_flop_char (void);
extern char pop_char_transput_buffer (int);
extern int char_scanner (A68_FILE *);
extern int end_of_format (NODE_T *, A68_REF);
extern int get_replicator_value (NODE_T *, BOOL_T);
extern int get_transput_buffer_index (int);
extern int get_transput_buffer_length (int);
extern int get_transput_buffer_size (int);
extern int get_unblocked_transput_buffer (NODE_T *);
extern void add_a_string_transput_buffer (NODE_T *, int, BYTE_T *);
extern void add_char_transput_buffer (NODE_T *, int, char);
extern void add_string_from_stack_transput_buffer (NODE_T *, int);
extern void add_string_transput_buffer (NODE_T *, int, char *);
extern void end_of_file_error (NODE_T * p, A68_REF ref_file);
extern void enlarge_transput_buffer (NODE_T *, int, int);
extern void format_error (NODE_T *, A68_REF, char *);
extern void genie_dns_addr (NODE_T *, MOID_T *, BYTE_T *, ADDR_T, char *);
extern void genie_preprocess (NODE_T *, int *, void *);
extern void genie_read_standard (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_string_to_value (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_value_to_string (NODE_T *, MOID_T *, BYTE_T *, int);
extern void genie_write_standard (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_write_string_from_stack (NODE_T * p, A68_REF);
extern void on_event_handler (NODE_T *, A68_PROCEDURE, A68_REF);
extern void open_error (NODE_T *, A68_REF, char *);
extern void pattern_error (NODE_T *, MOID_T *, int);
extern void read_insertion (NODE_T *, A68_REF);
extern void reset_transput_buffer (int);
extern void set_default_mended_procedure (A68_PROCEDURE *);
extern void set_default_mended_procedures (A68_FILE *);
extern void set_transput_buffer_index (int, int);
extern void set_transput_buffer_length (int, int);
extern void set_transput_buffer_size (int, int);
extern void standardise (double *, int, int, int *);
extern void transput_error (NODE_T *, A68_REF, MOID_T *);
extern void unchar_scanner (NODE_T *, A68_FILE *, char);
extern void value_error (NODE_T *, MOID_T *, A68_REF);
extern void write_insertion (NODE_T *, A68_REF, unsigned);
extern void write_purge_buffer (NODE_T *, A68_REF, int);
extern void read_sound (NODE_T *, A68_REF, A68_SOUND *);
extern void write_sound (NODE_T *, A68_REF, A68_SOUND *);

/* Definitions for the multi-precision library */

#define DEFAULT_MP_RADIX 	10000000
#define DEFAULT_DOUBLE_DIGITS	5

typedef double MP_DIGIT_T;
typedef MP_DIGIT_T A68_LONG[DEFAULT_DOUBLE_DIGITS + 2]; 
typedef A68_LONG A68_LONG_COMPLEX[2];

#define MP_RADIX      	DEFAULT_MP_RADIX
#define LOG_MP_BASE    	7
#define MP_BITS_RADIX	8388608 /* Max power of two smaller than MP_RADIX */
#define MP_BITS_BITS	23

/* 28-35 decimal digits for LONG REAL */
#define LONG_MP_DIGITS 	DEFAULT_DOUBLE_DIGITS

#define MAX_REPR_INT   	9007199254740992.0	/* 2^53, max int in a double */
#define MAX_MP_EXPONENT 142857	/* Arbitrary. Let M = MAX_REPR_INT then the largest range is M / Log M / LOG_MP_BASE */

#define MAX_MP_PRECISION 5000	/* For larger precisions better algorithms exist  */

extern int varying_mp_digits;

#define DOUBLE_ACCURACY (DBL_DIG - 1)

#define LONG_EXP_WIDTH       (EXP_WIDTH)
#define LONGLONG_EXP_WIDTH   (EXP_WIDTH)

#define LONG_WIDTH           (LONG_MP_DIGITS * LOG_MP_BASE)
#define LONGLONG_WIDTH       (varying_mp_digits * LOG_MP_BASE)

#define LONG_INT_WIDTH       (1 + LONG_WIDTH)
#define LONGLONG_INT_WIDTH   (1 + LONGLONG_WIDTH)

/* When changing L REAL_WIDTH mind that a mp number may not have more than
   1 + (MP_DIGITS - 1) * LOG_MP_BASE digits. Next definition is in accordance
   with REAL_WIDTH */

#define LONG_REAL_WIDTH      ((LONG_MP_DIGITS - 1) * LOG_MP_BASE)
#define LONGLONG_REAL_WIDTH  ((varying_mp_digits - 1) * LOG_MP_BASE)

#define LOG2_10			3.321928094887362347870319430
#define MP_BITS_WIDTH(k)	((int) ceil((k) * LOG_MP_BASE * LOG2_10) - 1)
#define MP_BITS_WORDS(k)	((int) ceil ((double) MP_BITS_WIDTH (k) / (double) MP_BITS_BITS))

enum
{ MP_PI, MP_TWO_PI, MP_HALF_PI };

/* MP Macros */

#define MP_STATUS(z) ((z)[0])
#define MP_EXPONENT(z) ((z)[1])
#define MP_DIGIT(z, n) ((z)[(n) + 1])

#define SIZE_MP(digits) ((2 + digits) * ALIGNED_SIZE_OF (MP_DIGIT_T))

#define IS_ZERO_MP(z) (MP_DIGIT (z, 1) == (MP_DIGIT_T) 0)

#define MOVE_MP(z, x, digits) {\
  MP_DIGIT_T *_m_d = (z), *_m_s = (x); int _m_k = digits + 2;\
  while (_m_k--) {*_m_d++ = *_m_s++;}\
  }

#define MOVE_DIGITS(z, x, digits) {\
  MP_DIGIT_T *_m_d = (z), *_m_s = (x); int _m_k = digits;\
  while (_m_k--) {*_m_d++ = *_m_s++;}\
  }

#define CHECK_MP_INIT(p, z, m) {\
  if (! ((int) z[0] & INITIALISED_MASK)) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE, (m));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }}

#define CHECK_MP_EXPONENT(p, z) {\
  MP_DIGIT_T _expo_ = fabs (MP_EXPONENT (z));\
  if (_expo_ > MAX_MP_EXPONENT || (_expo_ == MAX_MP_EXPONENT && ABS (MP_DIGIT (z, 1)) > 1.0)) {\
      errno = ERANGE;\
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_MP_OUT_OF_BOUNDS, NULL);\
      exit_genie (p, A68_RUNTIME_ERROR);\
  }}

#define SET_MP_ZERO(z, digits) {\
  MP_DIGIT_T *_m_d = &MP_DIGIT ((z), 1); int _m_k = digits;\
  MP_STATUS (z) = (MP_DIGIT_T) INITIALISED_MASK;\
  MP_EXPONENT (z) = 0.0;\
  while (_m_k--) {*_m_d++ = 0.0;}\
  }

/* stack_mp: allocate temporary space in the evaluation stack */

#define STACK_MP(dest, p, digits) {\
  ADDR_T stack_mp_sp = stack_pointer;\
  if ((stack_pointer += SIZE_MP (digits)) > expr_stack_limit) {\
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  dest = (MP_DIGIT_T *) STACK_ADDRESS (stack_mp_sp);\
}

/* External procedures */

extern BOOL_T check_long_int (MP_DIGIT_T *);
extern BOOL_T check_longlong_int (MP_DIGIT_T *);
extern BOOL_T check_mp_int (MP_DIGIT_T *, MOID_T *);
extern MP_DIGIT_T *abs_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *minus_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *round_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *entier_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern void eq_mp (NODE_T *, A68_BOOL *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern void ne_mp (NODE_T *, A68_BOOL *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern void lt_mp (NODE_T *, A68_BOOL *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern void le_mp (NODE_T *, A68_BOOL *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern void gt_mp (NODE_T *, A68_BOOL *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern void ge_mp (NODE_T *, A68_BOOL *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *acos_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *acosh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *add_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *asin_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *asinh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *atan2_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *atan_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *atanh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cacos_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *casin_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *catan_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *ccos_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cdiv_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cexp_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cln_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cmul_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cos_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cosh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *csin_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *csqrt_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *ctan_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *curt_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *div_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *div_mp_digit (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T, int);
extern MP_DIGIT_T *exp_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *expm1_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *hyp_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *hypot_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *int_to_mp (NODE_T *, MP_DIGIT_T *, int, int);
extern MP_DIGIT_T *lengthen_mp (NODE_T *, MP_DIGIT_T *, int, MP_DIGIT_T *, int);
extern MP_DIGIT_T *ln_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *log_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *mod_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *mp_pi (NODE_T *, MP_DIGIT_T *, int, int);
extern MP_DIGIT_T *mp_ten_up (NODE_T *, MP_DIGIT_T *, int, int);
extern MP_DIGIT_T *mul_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *mul_mp_digit (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T, int);
extern MP_DIGIT_T *over_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *over_mp_digit (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T, int);
extern MP_DIGIT_T *pack_mp_bits (NODE_T *, MP_DIGIT_T *, unsigned *, MOID_T *);
extern MP_DIGIT_T *pow_mp_int (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int, int);
extern MP_DIGIT_T *real_to_mp (NODE_T *, MP_DIGIT_T *, double, int);
extern MP_DIGIT_T *set_mp_short (MP_DIGIT_T *, MP_DIGIT_T, int, int);
extern MP_DIGIT_T *shorten_mp (NODE_T *, MP_DIGIT_T *, int, MP_DIGIT_T *, int);
extern MP_DIGIT_T *sin_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *sinh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *sqrt_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *string_to_mp (NODE_T *, MP_DIGIT_T *, char *, int);
extern MP_DIGIT_T *sub_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *tan_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *tanh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *unsigned_to_mp (NODE_T *, MP_DIGIT_T *, unsigned, int);
extern char *long_sub_fixed (NODE_T *, MP_DIGIT_T *, int, int, int);
extern char *long_sub_whole (NODE_T *, MP_DIGIT_T *, int, int);
extern double mp_to_real (NODE_T *, MP_DIGIT_T *, int);
extern int get_mp_bits_width (MOID_T *);
extern int get_mp_bits_words (MOID_T *);
extern int get_mp_digits (MOID_T *);
extern int get_mp_size (MOID_T *);
extern int int_to_mp_digits (int);
extern int long_mp_digits (void);
extern int longlong_mp_digits (void);
extern int mp_to_int (NODE_T *, MP_DIGIT_T *, int);
extern size_t size_long_mp (void);
extern size_t size_longlong_mp (void);
extern unsigned *stack_mp_bits (NODE_T *, MP_DIGIT_T *, MOID_T *);
extern unsigned mp_to_unsigned (NODE_T *, MP_DIGIT_T *, int);
extern void check_long_bits_value (NODE_T *, MP_DIGIT_T *, MOID_T *);
extern void long_standardise (NODE_T *, MP_DIGIT_T *, int, int, int, int *);
extern void raw_write_mp (char *, MP_DIGIT_T *, int);
extern void set_longlong_mp_digits (int);
extern void trunc_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);

/* gsl.h */

extern GENIE_PROCEDURE genie_exp_complex;
extern GENIE_PROCEDURE genie_cos_complex;
extern GENIE_PROCEDURE genie_cosh_complex;
extern GENIE_PROCEDURE genie_arccos_complex;
extern GENIE_PROCEDURE genie_arccosh_complex;
extern GENIE_PROCEDURE genie_arccosh_real;
extern GENIE_PROCEDURE genie_arcsin_complex;
extern GENIE_PROCEDURE genie_arcsinh_complex;
extern GENIE_PROCEDURE genie_arcsinh_real;
extern GENIE_PROCEDURE genie_arctan_complex;
extern GENIE_PROCEDURE genie_arctanh_complex;
extern GENIE_PROCEDURE genie_arctanh_real;
extern GENIE_PROCEDURE genie_inverf_real;
extern GENIE_PROCEDURE genie_inverfc_real;
extern GENIE_PROCEDURE genie_ln_complex;
extern GENIE_PROCEDURE genie_sin_complex;
extern GENIE_PROCEDURE genie_sinh_complex;
extern GENIE_PROCEDURE genie_sqrt_complex;
extern GENIE_PROCEDURE genie_tan_complex;
extern GENIE_PROCEDURE genie_tanh_complex;

#if (defined HAVE_GSL_GSL_BLAS_H && defined HAVE_LIBGSL)
extern GENIE_PROCEDURE genie_complex_scale_matrix_complex;
extern GENIE_PROCEDURE genie_complex_scale_vector_complex;
extern GENIE_PROCEDURE genie_laplace;
extern GENIE_PROCEDURE genie_matrix_add;
extern GENIE_PROCEDURE genie_matrix_ch;
extern GENIE_PROCEDURE genie_matrix_ch_solve;
extern GENIE_PROCEDURE genie_matrix_complex_add;
extern GENIE_PROCEDURE genie_matrix_complex_det;
extern GENIE_PROCEDURE genie_matrix_complex_div_complex;
extern GENIE_PROCEDURE genie_matrix_complex_div_complex_ab;
extern GENIE_PROCEDURE genie_matrix_complex_echo;
extern GENIE_PROCEDURE genie_matrix_complex_eq;
extern GENIE_PROCEDURE genie_matrix_complex_inv;
extern GENIE_PROCEDURE genie_matrix_complex_lu;
extern GENIE_PROCEDURE genie_matrix_complex_lu_det;
extern GENIE_PROCEDURE genie_matrix_complex_lu_inv;
extern GENIE_PROCEDURE genie_matrix_complex_lu_solve;
extern GENIE_PROCEDURE genie_matrix_complex_minus;
extern GENIE_PROCEDURE genie_matrix_complex_minusab;
extern GENIE_PROCEDURE genie_matrix_complex_ne;
extern GENIE_PROCEDURE genie_matrix_complex_plusab;
extern GENIE_PROCEDURE genie_matrix_complex_scale_complex;
extern GENIE_PROCEDURE genie_matrix_complex_scale_complex_ab;
extern GENIE_PROCEDURE genie_matrix_complex_sub;
extern GENIE_PROCEDURE genie_matrix_complex_times_matrix;
extern GENIE_PROCEDURE genie_matrix_complex_times_vector;
extern GENIE_PROCEDURE genie_matrix_complex_trace;
extern GENIE_PROCEDURE genie_matrix_complex_transpose;
extern GENIE_PROCEDURE genie_matrix_det;
extern GENIE_PROCEDURE genie_matrix_div_real;
extern GENIE_PROCEDURE genie_matrix_div_real_ab;
extern GENIE_PROCEDURE genie_matrix_echo;
extern GENIE_PROCEDURE genie_matrix_eq;
extern GENIE_PROCEDURE genie_matrix_inv;
extern GENIE_PROCEDURE genie_matrix_lu;
extern GENIE_PROCEDURE genie_matrix_lu_det;
extern GENIE_PROCEDURE genie_matrix_lu_inv;
extern GENIE_PROCEDURE genie_matrix_lu_solve;
extern GENIE_PROCEDURE genie_matrix_minus;
extern GENIE_PROCEDURE genie_matrix_minusab;
extern GENIE_PROCEDURE genie_matrix_ne;
extern GENIE_PROCEDURE genie_matrix_plusab;
extern GENIE_PROCEDURE genie_matrix_qr;
extern GENIE_PROCEDURE genie_matrix_qr_ls_solve;
extern GENIE_PROCEDURE genie_matrix_qr_solve;
extern GENIE_PROCEDURE genie_matrix_scale_real;
extern GENIE_PROCEDURE genie_matrix_scale_real_ab;
extern GENIE_PROCEDURE genie_matrix_sub;
extern GENIE_PROCEDURE genie_matrix_svd;
extern GENIE_PROCEDURE genie_matrix_svd_solve;
extern GENIE_PROCEDURE genie_matrix_times_matrix;
extern GENIE_PROCEDURE genie_matrix_times_vector;
extern GENIE_PROCEDURE genie_matrix_trace;
extern GENIE_PROCEDURE genie_matrix_transpose;
extern GENIE_PROCEDURE genie_real_scale_matrix;
extern GENIE_PROCEDURE genie_real_scale_vector;
extern GENIE_PROCEDURE genie_vector_add;
extern GENIE_PROCEDURE genie_vector_complex_add;
extern GENIE_PROCEDURE genie_vector_complex_div_complex;
extern GENIE_PROCEDURE genie_vector_complex_div_complex_ab;
extern GENIE_PROCEDURE genie_vector_complex_dot;
extern GENIE_PROCEDURE genie_vector_complex_dyad;
extern GENIE_PROCEDURE genie_vector_complex_echo;
extern GENIE_PROCEDURE genie_vector_complex_eq;
extern GENIE_PROCEDURE genie_vector_complex_minus;
extern GENIE_PROCEDURE genie_vector_complex_minusab;
extern GENIE_PROCEDURE genie_vector_complex_ne;
extern GENIE_PROCEDURE genie_vector_complex_norm;
extern GENIE_PROCEDURE genie_vector_complex_plusab;
extern GENIE_PROCEDURE genie_vector_complex_scale_complex;
extern GENIE_PROCEDURE genie_vector_complex_scale_complex_ab;
extern GENIE_PROCEDURE genie_vector_complex_sub;
extern GENIE_PROCEDURE genie_vector_complex_times_matrix;
extern GENIE_PROCEDURE genie_vector_div_real;
extern GENIE_PROCEDURE genie_vector_div_real_ab;
extern GENIE_PROCEDURE genie_vector_dot;
extern GENIE_PROCEDURE genie_vector_dyad;
extern GENIE_PROCEDURE genie_vector_echo;
extern GENIE_PROCEDURE genie_vector_eq;
extern GENIE_PROCEDURE genie_vector_minus;
extern GENIE_PROCEDURE genie_vector_minusab;
extern GENIE_PROCEDURE genie_vector_ne;
extern GENIE_PROCEDURE genie_vector_norm;
extern GENIE_PROCEDURE genie_vector_plusab;
extern GENIE_PROCEDURE genie_vector_scale_real;
extern GENIE_PROCEDURE genie_vector_scale_real_ab;
extern GENIE_PROCEDURE genie_vector_sub;
extern GENIE_PROCEDURE genie_vector_times_matrix;
#endif

#if (defined HAVE_GSL_GSL_BLAS_H && defined HAVE_LIBGSL)
extern GENIE_PROCEDURE genie_prime_factors;
extern GENIE_PROCEDURE genie_fft_complex_forward;
extern GENIE_PROCEDURE genie_fft_complex_backward;
extern GENIE_PROCEDURE genie_fft_complex_inverse;
extern GENIE_PROCEDURE genie_fft_forward;
extern GENIE_PROCEDURE genie_fft_backward;
extern GENIE_PROCEDURE genie_fft_inverse;
#endif

#if (defined HAVE_GSL_GSL_BLAS_H && defined HAVE_LIBGSL)
extern GENIE_PROCEDURE genie_cgs_speed_of_light;
extern GENIE_PROCEDURE genie_cgs_gravitational_constant;
extern GENIE_PROCEDURE genie_cgs_planck_constant_h;
extern GENIE_PROCEDURE genie_cgs_planck_constant_hbar;
extern GENIE_PROCEDURE genie_cgs_vacuum_permeability;
extern GENIE_PROCEDURE genie_cgs_astronomical_unit;
extern GENIE_PROCEDURE genie_cgs_light_year;
extern GENIE_PROCEDURE genie_cgs_parsec;
extern GENIE_PROCEDURE genie_cgs_grav_accel;
extern GENIE_PROCEDURE genie_cgs_electron_volt;
extern GENIE_PROCEDURE genie_cgs_mass_electron;
extern GENIE_PROCEDURE genie_cgs_mass_muon;
extern GENIE_PROCEDURE genie_cgs_mass_proton;
extern GENIE_PROCEDURE genie_cgs_mass_neutron;
extern GENIE_PROCEDURE genie_cgs_rydberg;
extern GENIE_PROCEDURE genie_cgs_boltzmann;
extern GENIE_PROCEDURE genie_cgs_bohr_magneton;
extern GENIE_PROCEDURE genie_cgs_nuclear_magneton;
extern GENIE_PROCEDURE genie_cgs_electron_magnetic_moment;
extern GENIE_PROCEDURE genie_cgs_proton_magnetic_moment;
extern GENIE_PROCEDURE genie_cgs_molar_gas;
extern GENIE_PROCEDURE genie_cgs_standard_gas_volume;
extern GENIE_PROCEDURE genie_cgs_minute;
extern GENIE_PROCEDURE genie_cgs_hour;
extern GENIE_PROCEDURE genie_cgs_day;
extern GENIE_PROCEDURE genie_cgs_week;
extern GENIE_PROCEDURE genie_cgs_inch;
extern GENIE_PROCEDURE genie_cgs_foot;
extern GENIE_PROCEDURE genie_cgs_yard;
extern GENIE_PROCEDURE genie_cgs_mile;
extern GENIE_PROCEDURE genie_cgs_nautical_mile;
extern GENIE_PROCEDURE genie_cgs_fathom;
extern GENIE_PROCEDURE genie_cgs_mil;
extern GENIE_PROCEDURE genie_cgs_point;
extern GENIE_PROCEDURE genie_cgs_texpoint;
extern GENIE_PROCEDURE genie_cgs_micron;
extern GENIE_PROCEDURE genie_cgs_angstrom;
extern GENIE_PROCEDURE genie_cgs_hectare;
extern GENIE_PROCEDURE genie_cgs_acre;
extern GENIE_PROCEDURE genie_cgs_barn;
extern GENIE_PROCEDURE genie_cgs_liter;
extern GENIE_PROCEDURE genie_cgs_us_gallon;
extern GENIE_PROCEDURE genie_cgs_quart;
extern GENIE_PROCEDURE genie_cgs_pint;
extern GENIE_PROCEDURE genie_cgs_cup;
extern GENIE_PROCEDURE genie_cgs_fluid_ounce;
extern GENIE_PROCEDURE genie_cgs_tablespoon;
extern GENIE_PROCEDURE genie_cgs_teaspoon;
extern GENIE_PROCEDURE genie_cgs_canadian_gallon;
extern GENIE_PROCEDURE genie_cgs_uk_gallon;
extern GENIE_PROCEDURE genie_cgs_miles_per_hour;
extern GENIE_PROCEDURE genie_cgs_kilometers_per_hour;
extern GENIE_PROCEDURE genie_cgs_knot;
extern GENIE_PROCEDURE genie_cgs_pound_mass;
extern GENIE_PROCEDURE genie_cgs_ounce_mass;
extern GENIE_PROCEDURE genie_cgs_ton;
extern GENIE_PROCEDURE genie_cgs_metric_ton;
extern GENIE_PROCEDURE genie_cgs_uk_ton;
extern GENIE_PROCEDURE genie_cgs_troy_ounce;
extern GENIE_PROCEDURE genie_cgs_carat;
extern GENIE_PROCEDURE genie_cgs_unified_atomic_mass;
extern GENIE_PROCEDURE genie_cgs_gram_force;
extern GENIE_PROCEDURE genie_cgs_pound_force;
extern GENIE_PROCEDURE genie_cgs_kilopound_force;
extern GENIE_PROCEDURE genie_cgs_poundal;
extern GENIE_PROCEDURE genie_cgs_calorie;
extern GENIE_PROCEDURE genie_cgs_btu;
extern GENIE_PROCEDURE genie_cgs_therm;
extern GENIE_PROCEDURE genie_cgs_horsepower;
extern GENIE_PROCEDURE genie_cgs_bar;
extern GENIE_PROCEDURE genie_cgs_std_atmosphere;
extern GENIE_PROCEDURE genie_cgs_torr;
extern GENIE_PROCEDURE genie_cgs_meter_of_mercury;
extern GENIE_PROCEDURE genie_cgs_inch_of_mercury;
extern GENIE_PROCEDURE genie_cgs_inch_of_water;
extern GENIE_PROCEDURE genie_cgs_psi;
extern GENIE_PROCEDURE genie_cgs_poise;
extern GENIE_PROCEDURE genie_cgs_stokes;
extern GENIE_PROCEDURE genie_cgs_faraday;
extern GENIE_PROCEDURE genie_cgs_electron_charge;
extern GENIE_PROCEDURE genie_cgs_gauss;
extern GENIE_PROCEDURE genie_cgs_stilb;
extern GENIE_PROCEDURE genie_cgs_lumen;
extern GENIE_PROCEDURE genie_cgs_lux;
extern GENIE_PROCEDURE genie_cgs_phot;
extern GENIE_PROCEDURE genie_cgs_footcandle;
extern GENIE_PROCEDURE genie_cgs_lambert;
extern GENIE_PROCEDURE genie_cgs_footlambert;
extern GENIE_PROCEDURE genie_cgs_curie;
extern GENIE_PROCEDURE genie_cgs_roentgen;
extern GENIE_PROCEDURE genie_cgs_rad;
extern GENIE_PROCEDURE genie_cgs_solar_mass;
extern GENIE_PROCEDURE genie_cgs_bohr_radius;
extern GENIE_PROCEDURE genie_cgs_vacuum_permittivity;
extern GENIE_PROCEDURE genie_cgs_newton;
extern GENIE_PROCEDURE genie_cgs_dyne;
extern GENIE_PROCEDURE genie_cgs_joule;
extern GENIE_PROCEDURE genie_cgs_erg;
extern GENIE_PROCEDURE genie_mks_speed_of_light;
extern GENIE_PROCEDURE genie_mks_gravitational_constant;
extern GENIE_PROCEDURE genie_mks_planck_constant_h;
extern GENIE_PROCEDURE genie_mks_planck_constant_hbar;
extern GENIE_PROCEDURE genie_mks_vacuum_permeability;
extern GENIE_PROCEDURE genie_mks_astronomical_unit;
extern GENIE_PROCEDURE genie_mks_light_year;
extern GENIE_PROCEDURE genie_mks_parsec;
extern GENIE_PROCEDURE genie_mks_grav_accel;
extern GENIE_PROCEDURE genie_mks_electron_volt;
extern GENIE_PROCEDURE genie_mks_mass_electron;
extern GENIE_PROCEDURE genie_mks_mass_muon;
extern GENIE_PROCEDURE genie_mks_mass_proton;
extern GENIE_PROCEDURE genie_mks_mass_neutron;
extern GENIE_PROCEDURE genie_mks_rydberg;
extern GENIE_PROCEDURE genie_mks_boltzmann;
extern GENIE_PROCEDURE genie_mks_bohr_magneton;
extern GENIE_PROCEDURE genie_mks_nuclear_magneton;
extern GENIE_PROCEDURE genie_mks_electron_magnetic_moment;
extern GENIE_PROCEDURE genie_mks_proton_magnetic_moment;
extern GENIE_PROCEDURE genie_mks_molar_gas;
extern GENIE_PROCEDURE genie_mks_standard_gas_volume;
extern GENIE_PROCEDURE genie_mks_minute;
extern GENIE_PROCEDURE genie_mks_hour;
extern GENIE_PROCEDURE genie_mks_day;
extern GENIE_PROCEDURE genie_mks_week;
extern GENIE_PROCEDURE genie_mks_inch;
extern GENIE_PROCEDURE genie_mks_foot;
extern GENIE_PROCEDURE genie_mks_yard;
extern GENIE_PROCEDURE genie_mks_mile;
extern GENIE_PROCEDURE genie_mks_nautical_mile;
extern GENIE_PROCEDURE genie_mks_fathom;
extern GENIE_PROCEDURE genie_mks_mil;
extern GENIE_PROCEDURE genie_mks_point;
extern GENIE_PROCEDURE genie_mks_texpoint;
extern GENIE_PROCEDURE genie_mks_micron;
extern GENIE_PROCEDURE genie_mks_angstrom;
extern GENIE_PROCEDURE genie_mks_hectare;
extern GENIE_PROCEDURE genie_mks_acre;
extern GENIE_PROCEDURE genie_mks_barn;
extern GENIE_PROCEDURE genie_mks_liter;
extern GENIE_PROCEDURE genie_mks_us_gallon;
extern GENIE_PROCEDURE genie_mks_quart;
extern GENIE_PROCEDURE genie_mks_pint;
extern GENIE_PROCEDURE genie_mks_cup;
extern GENIE_PROCEDURE genie_mks_fluid_ounce;
extern GENIE_PROCEDURE genie_mks_tablespoon;
extern GENIE_PROCEDURE genie_mks_teaspoon;
extern GENIE_PROCEDURE genie_mks_canadian_gallon;
extern GENIE_PROCEDURE genie_mks_uk_gallon;
extern GENIE_PROCEDURE genie_mks_miles_per_hour;
extern GENIE_PROCEDURE genie_mks_kilometers_per_hour;
extern GENIE_PROCEDURE genie_mks_knot;
extern GENIE_PROCEDURE genie_mks_pound_mass;
extern GENIE_PROCEDURE genie_mks_ounce_mass;
extern GENIE_PROCEDURE genie_mks_ton;
extern GENIE_PROCEDURE genie_mks_metric_ton;
extern GENIE_PROCEDURE genie_mks_uk_ton;
extern GENIE_PROCEDURE genie_mks_troy_ounce;
extern GENIE_PROCEDURE genie_mks_carat;
extern GENIE_PROCEDURE genie_mks_unified_atomic_mass;
extern GENIE_PROCEDURE genie_mks_gram_force;
extern GENIE_PROCEDURE genie_mks_pound_force;
extern GENIE_PROCEDURE genie_mks_kilopound_force;
extern GENIE_PROCEDURE genie_mks_poundal;
extern GENIE_PROCEDURE genie_mks_calorie;
extern GENIE_PROCEDURE genie_mks_btu;
extern GENIE_PROCEDURE genie_mks_therm;
extern GENIE_PROCEDURE genie_mks_horsepower;
extern GENIE_PROCEDURE genie_mks_bar;
extern GENIE_PROCEDURE genie_mks_std_atmosphere;
extern GENIE_PROCEDURE genie_mks_torr;
extern GENIE_PROCEDURE genie_mks_meter_of_mercury;
extern GENIE_PROCEDURE genie_mks_inch_of_mercury;
extern GENIE_PROCEDURE genie_mks_inch_of_water;
extern GENIE_PROCEDURE genie_mks_psi;
extern GENIE_PROCEDURE genie_mks_poise;
extern GENIE_PROCEDURE genie_mks_stokes;
extern GENIE_PROCEDURE genie_mks_faraday;
extern GENIE_PROCEDURE genie_mks_electron_charge;
extern GENIE_PROCEDURE genie_mks_gauss;
extern GENIE_PROCEDURE genie_mks_stilb;
extern GENIE_PROCEDURE genie_mks_lumen;
extern GENIE_PROCEDURE genie_mks_lux;
extern GENIE_PROCEDURE genie_mks_phot;
extern GENIE_PROCEDURE genie_mks_footcandle;
extern GENIE_PROCEDURE genie_mks_lambert;
extern GENIE_PROCEDURE genie_mks_footlambert;
extern GENIE_PROCEDURE genie_mks_curie;
extern GENIE_PROCEDURE genie_mks_roentgen;
extern GENIE_PROCEDURE genie_mks_rad;
extern GENIE_PROCEDURE genie_mks_solar_mass;
extern GENIE_PROCEDURE genie_mks_bohr_radius;
extern GENIE_PROCEDURE genie_mks_vacuum_permittivity;
extern GENIE_PROCEDURE genie_mks_newton;
extern GENIE_PROCEDURE genie_mks_dyne;
extern GENIE_PROCEDURE genie_mks_joule;
extern GENIE_PROCEDURE genie_mks_erg;
extern GENIE_PROCEDURE genie_num_fine_structure;
extern GENIE_PROCEDURE genie_num_avogadro;
extern GENIE_PROCEDURE genie_num_yotta;
extern GENIE_PROCEDURE genie_num_zetta;
extern GENIE_PROCEDURE genie_num_exa;
extern GENIE_PROCEDURE genie_num_peta;
extern GENIE_PROCEDURE genie_num_tera;
extern GENIE_PROCEDURE genie_num_giga;
extern GENIE_PROCEDURE genie_num_mega;
extern GENIE_PROCEDURE genie_num_kilo;
extern GENIE_PROCEDURE genie_num_milli;
extern GENIE_PROCEDURE genie_num_micro;
extern GENIE_PROCEDURE genie_num_nano;
extern GENIE_PROCEDURE genie_num_pico;
extern GENIE_PROCEDURE genie_num_femto;
extern GENIE_PROCEDURE genie_num_atto;
extern GENIE_PROCEDURE genie_num_zepto;
extern GENIE_PROCEDURE genie_num_yocto;
extern GENIE_PROCEDURE genie_airy_ai_deriv_real;
extern GENIE_PROCEDURE genie_airy_ai_real;
extern GENIE_PROCEDURE genie_airy_bi_deriv_real;
extern GENIE_PROCEDURE genie_airy_bi_real;
extern GENIE_PROCEDURE genie_bessel_exp_il_real;
extern GENIE_PROCEDURE genie_bessel_exp_in_real;
extern GENIE_PROCEDURE genie_bessel_exp_inu_real;
extern GENIE_PROCEDURE genie_bessel_exp_kl_real;
extern GENIE_PROCEDURE genie_bessel_exp_kn_real;
extern GENIE_PROCEDURE genie_bessel_exp_knu_real;
extern GENIE_PROCEDURE genie_bessel_in_real;
extern GENIE_PROCEDURE genie_bessel_inu_real;
extern GENIE_PROCEDURE genie_bessel_jl_real;
extern GENIE_PROCEDURE genie_bessel_jn_real;
extern GENIE_PROCEDURE genie_bessel_jnu_real;
extern GENIE_PROCEDURE genie_bessel_kn_real;
extern GENIE_PROCEDURE genie_bessel_knu_real;
extern GENIE_PROCEDURE genie_bessel_yl_real;
extern GENIE_PROCEDURE genie_bessel_yn_real;
extern GENIE_PROCEDURE genie_bessel_ynu_real;
extern GENIE_PROCEDURE genie_beta_inc_real;
extern GENIE_PROCEDURE genie_beta_real;
extern GENIE_PROCEDURE genie_elliptic_integral_e_real;
extern GENIE_PROCEDURE genie_elliptic_integral_k_real;
extern GENIE_PROCEDURE genie_elliptic_integral_rc_real;
extern GENIE_PROCEDURE genie_elliptic_integral_rd_real;
extern GENIE_PROCEDURE genie_elliptic_integral_rf_real;
extern GENIE_PROCEDURE genie_elliptic_integral_rj_real;
extern GENIE_PROCEDURE genie_factorial_real;
extern GENIE_PROCEDURE genie_gamma_inc_real;
extern GENIE_PROCEDURE genie_gamma_real;
extern GENIE_PROCEDURE genie_lngamma_real;
#endif

extern GENIE_PROCEDURE genie_erf_real;
extern GENIE_PROCEDURE genie_erfc_real;
extern GENIE_PROCEDURE genie_cosh_real;
extern GENIE_PROCEDURE genie_sinh_real;
extern GENIE_PROCEDURE genie_tanh_real;

extern void calc_rte (NODE_T *, BOOL_T, MOID_T *, const char *);
extern double curt (double);

#define GSL_CONST_NUM_FINE_STRUCTURE (7.297352533e-3) /* 1 */
#define GSL_CONST_NUM_AVOGADRO (6.02214199e23) /* 1 / mol */
#define GSL_CONST_NUM_YOTTA (1e24) /* 1 */
#define GSL_CONST_NUM_ZETTA (1e21) /* 1 */
#define GSL_CONST_NUM_EXA (1e18) /* 1 */
#define GSL_CONST_NUM_PETA (1e15) /* 1 */
#define GSL_CONST_NUM_TERA (1e12) /* 1 */
#define GSL_CONST_NUM_GIGA (1e9) /* 1 */
#define GSL_CONST_NUM_MEGA (1e6) /* 1 */
#define GSL_CONST_NUM_KILO (1e3) /* 1 */
#define GSL_CONST_NUM_MILLI (1e-3) /* 1 */
#define GSL_CONST_NUM_MICRO (1e-6) /* 1 */
#define GSL_CONST_NUM_NANO (1e-9) /* 1 */
#define GSL_CONST_NUM_PICO (1e-12) /* 1 */
#define GSL_CONST_NUM_FEMTO (1e-15) /* 1 */
#define GSL_CONST_NUM_ATTO (1e-18) /* 1 */
#define GSL_CONST_NUM_ZEPTO (1e-21) /* 1 */
#define GSL_CONST_NUM_YOCTO (1e-24) /* 1 */

#define GSL_CONST_CGSM_GAUSS (1.0) /* cm / A s^2  */
#define GSL_CONST_CGSM_SPEED_OF_LIGHT (2.99792458e10) /* cm / s */
#define GSL_CONST_CGSM_GRAVITATIONAL_CONSTANT (6.673e-8) /* cm^3 / g s^2 */
#define GSL_CONST_CGSM_PLANCKS_CONSTANT_H (6.62606896e-27) /* g cm^2 / s */
#define GSL_CONST_CGSM_PLANCKS_CONSTANT_HBAR (1.05457162825e-27) /* g cm^2 / s */
#define GSL_CONST_CGSM_ASTRONOMICAL_UNIT (1.49597870691e13) /* cm */
#define GSL_CONST_CGSM_LIGHT_YEAR (9.46053620707e17) /* cm */
#define GSL_CONST_CGSM_PARSEC (3.08567758135e18) /* cm */
#define GSL_CONST_CGSM_GRAV_ACCEL (9.80665e2) /* cm / s^2 */
#define GSL_CONST_CGSM_ELECTRON_VOLT (1.602176487e-12) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_MASS_ELECTRON (9.10938188e-28) /* g */
#define GSL_CONST_CGSM_MASS_MUON (1.88353109e-25) /* g */
#define GSL_CONST_CGSM_MASS_PROTON (1.67262158e-24) /* g */
#define GSL_CONST_CGSM_MASS_NEUTRON (1.67492716e-24) /* g */
#define GSL_CONST_CGSM_RYDBERG (2.17987196968e-11) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_BOLTZMANN (1.3806504e-16) /* g cm^2 / K s^2 */
#define GSL_CONST_CGSM_MOLAR_GAS (8.314472e7) /* g cm^2 / K mol s^2 */
#define GSL_CONST_CGSM_STANDARD_GAS_VOLUME (2.2710981e4) /* cm^3 / mol */
#define GSL_CONST_CGSM_MINUTE (6e1) /* s */
#define GSL_CONST_CGSM_HOUR (3.6e3) /* s */
#define GSL_CONST_CGSM_DAY (8.64e4) /* s */
#define GSL_CONST_CGSM_WEEK (6.048e5) /* s */
#define GSL_CONST_CGSM_INCH (2.54e0) /* cm */
#define GSL_CONST_CGSM_FOOT (3.048e1) /* cm */
#define GSL_CONST_CGSM_YARD (9.144e1) /* cm */
#define GSL_CONST_CGSM_MILE (1.609344e5) /* cm */
#define GSL_CONST_CGSM_NAUTICAL_MILE (1.852e5) /* cm */
#define GSL_CONST_CGSM_FATHOM (1.8288e2) /* cm */
#define GSL_CONST_CGSM_MIL (2.54e-3) /* cm */
#define GSL_CONST_CGSM_POINT (3.52777777778e-2) /* cm */
#define GSL_CONST_CGSM_TEXPOINT (3.51459803515e-2) /* cm */
#define GSL_CONST_CGSM_MICRON (1e-4) /* cm */
#define GSL_CONST_CGSM_ANGSTROM (1e-8) /* cm */
#define GSL_CONST_CGSM_HECTARE (1e8) /* cm^2 */
#define GSL_CONST_CGSM_ACRE (4.04685642241e7) /* cm^2 */
#define GSL_CONST_CGSM_BARN (1e-24) /* cm^2 */
#define GSL_CONST_CGSM_LITER (1e3) /* cm^3 */
#define GSL_CONST_CGSM_US_GALLON (3.78541178402e3) /* cm^3 */
#define GSL_CONST_CGSM_QUART (9.46352946004e2) /* cm^3 */
#define GSL_CONST_CGSM_PINT (4.73176473002e2) /* cm^3 */
#define GSL_CONST_CGSM_CUP (2.36588236501e2) /* cm^3 */
#define GSL_CONST_CGSM_FLUID_OUNCE (2.95735295626e1) /* cm^3 */
#define GSL_CONST_CGSM_TABLESPOON (1.47867647813e1) /* cm^3 */
#define GSL_CONST_CGSM_TEASPOON (4.92892159375e0) /* cm^3 */
#define GSL_CONST_CGSM_CANADIAN_GALLON (4.54609e3) /* cm^3 */
#define GSL_CONST_CGSM_UK_GALLON (4.546092e3) /* cm^3 */
#define GSL_CONST_CGSM_MILES_PER_HOUR (4.4704e1) /* cm / s */
#define GSL_CONST_CGSM_KILOMETERS_PER_HOUR (2.77777777778e1) /* cm / s */
#define GSL_CONST_CGSM_KNOT (5.14444444444e1) /* cm / s */
#define GSL_CONST_CGSM_POUND_MASS (4.5359237e2) /* g */
#define GSL_CONST_CGSM_OUNCE_MASS (2.8349523125e1) /* g */
#define GSL_CONST_CGSM_TON (9.0718474e5) /* g */
#define GSL_CONST_CGSM_METRIC_TON (1e6) /* g */
#define GSL_CONST_CGSM_UK_TON (1.0160469088e6) /* g */
#define GSL_CONST_CGSM_TROY_OUNCE (3.1103475e1) /* g */
#define GSL_CONST_CGSM_CARAT (2e-1) /* g */
#define GSL_CONST_CGSM_UNIFIED_ATOMIC_MASS (1.660538782e-24) /* g */
#define GSL_CONST_CGSM_GRAM_FORCE (9.80665e2) /* cm g / s^2 */
#define GSL_CONST_CGSM_POUND_FORCE (4.44822161526e5) /* cm g / s^2 */
#define GSL_CONST_CGSM_KILOPOUND_FORCE (4.44822161526e8) /* cm g / s^2 */
#define GSL_CONST_CGSM_POUNDAL (1.38255e4) /* cm g / s^2 */
#define GSL_CONST_CGSM_CALORIE (4.1868e7) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_BTU (1.05505585262e10) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_THERM (1.05506e15) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_HORSEPOWER (7.457e9) /* g cm^2 / s^3 */
#define GSL_CONST_CGSM_BAR (1e6) /* g / cm s^2 */
#define GSL_CONST_CGSM_STD_ATMOSPHERE (1.01325e6) /* g / cm s^2 */
#define GSL_CONST_CGSM_TORR (1.33322368421e3) /* g / cm s^2 */
#define GSL_CONST_CGSM_METER_OF_MERCURY (1.33322368421e6) /* g / cm s^2 */
#define GSL_CONST_CGSM_INCH_OF_MERCURY (3.38638815789e4) /* g / cm s^2 */
#define GSL_CONST_CGSM_INCH_OF_WATER (2.490889e3) /* g / cm s^2 */
#define GSL_CONST_CGSM_PSI (6.89475729317e4) /* g / cm s^2 */
#define GSL_CONST_CGSM_POISE (1e0) /* g / cm s */
#define GSL_CONST_CGSM_STOKES (1e0) /* cm^2 / s */
#define GSL_CONST_CGSM_STILB (1e0) /* cd / cm^2 */
#define GSL_CONST_CGSM_LUMEN (1e0) /* cd sr */
#define GSL_CONST_CGSM_LUX (1e-4) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_PHOT (1e0) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_FOOTCANDLE (1.076e-3) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_LAMBERT (1e0) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_FOOTLAMBERT (1.07639104e-3) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_CURIE (3.7e10) /* 1 / s */
#define GSL_CONST_CGSM_ROENTGEN (2.58e-8) /* abamp s / g */
#define GSL_CONST_CGSM_RAD (1e2) /* cm^2 / s^2 */
#define GSL_CONST_CGSM_SOLAR_MASS (1.98892e33) /* g */
#define GSL_CONST_CGSM_BOHR_RADIUS (5.291772083e-9) /* cm */
#define GSL_CONST_CGSM_NEWTON (1e5) /* cm g / s^2 */
#define GSL_CONST_CGSM_DYNE (1e0) /* cm g / s^2 */
#define GSL_CONST_CGSM_JOULE (1e7) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_ERG (1e0) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_STEFAN_BOLTZMANN_CONSTANT (5.67040047374e-5) /* g / K^4 s^3 */
#define GSL_CONST_CGSM_THOMSON_CROSS_SECTION (6.65245893699e-25) /* cm^2 */
#define GSL_CONST_CGSM_BOHR_MAGNETON (9.27400899e-21) /* abamp cm^2 */
#define GSL_CONST_CGSM_NUCLEAR_MAGNETON (5.05078317e-24) /* abamp cm^2 */
#define GSL_CONST_CGSM_ELECTRON_MAGNETIC_MOMENT (9.28476362e-21) /* abamp cm^2 */
#define GSL_CONST_CGSM_PROTON_MAGNETIC_MOMENT (1.410606633e-23) /* abamp cm^2 */
#define GSL_CONST_CGSM_FARADAY (9.64853429775e3) /* abamp s / mol */
#define GSL_CONST_CGSM_ELECTRON_CHARGE (1.602176487e-20) /* abamp s */

#define GSL_CONST_MKS_SPEED_OF_LIGHT (2.99792458e8) /* m / s */
#define GSL_CONST_MKS_GRAVITATIONAL_CONSTANT (6.673e-11) /* m^3 / kg s^2 */
#define GSL_CONST_MKS_PLANCKS_CONSTANT_H (6.62606896e-34) /* kg m^2 / s */
#define GSL_CONST_MKS_PLANCKS_CONSTANT_HBAR (1.05457162825e-34) /* kg m^2 / s */
#define GSL_CONST_MKS_ASTRONOMICAL_UNIT (1.49597870691e11) /* m */
#define GSL_CONST_MKS_LIGHT_YEAR (9.46053620707e15) /* m */
#define GSL_CONST_MKS_PARSEC (3.08567758135e16) /* m */
#define GSL_CONST_MKS_GRAV_ACCEL (9.80665e0) /* m / s^2 */
#define GSL_CONST_MKS_ELECTRON_VOLT (1.602176487e-19) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_MASS_ELECTRON (9.10938188e-31) /* kg */
#define GSL_CONST_MKS_MASS_MUON (1.88353109e-28) /* kg */
#define GSL_CONST_MKS_MASS_PROTON (1.67262158e-27) /* kg */
#define GSL_CONST_MKS_MASS_NEUTRON (1.67492716e-27) /* kg */
#define GSL_CONST_MKS_RYDBERG (2.17987196968e-18) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_BOLTZMANN (1.3806504e-23) /* kg m^2 / K s^2 */
#define GSL_CONST_MKS_MOLAR_GAS (8.314472e0) /* kg m^2 / K mol s^2 */
#define GSL_CONST_MKS_STANDARD_GAS_VOLUME (2.2710981e-2) /* m^3 / mol */
#define GSL_CONST_MKS_MINUTE (6e1) /* s */
#define GSL_CONST_MKS_HOUR (3.6e3) /* s */
#define GSL_CONST_MKS_DAY (8.64e4) /* s */
#define GSL_CONST_MKS_WEEK (6.048e5) /* s */
#define GSL_CONST_MKS_INCH (2.54e-2) /* m */
#define GSL_CONST_MKS_FOOT (3.048e-1) /* m */
#define GSL_CONST_MKS_YARD (9.144e-1) /* m */
#define GSL_CONST_MKS_MILE (1.609344e3) /* m */
#define GSL_CONST_MKS_NAUTICAL_MILE (1.852e3) /* m */
#define GSL_CONST_MKS_FATHOM (1.8288e0) /* m */
#define GSL_CONST_MKS_MIL (2.54e-5) /* m */
#define GSL_CONST_MKS_POINT (3.52777777778e-4) /* m */
#define GSL_CONST_MKS_TEXPOINT (3.51459803515e-4) /* m */
#define GSL_CONST_MKS_MICRON (1e-6) /* m */
#define GSL_CONST_MKS_ANGSTROM (1e-10) /* m */
#define GSL_CONST_MKS_HECTARE (1e4) /* m^2 */
#define GSL_CONST_MKS_ACRE (4.04685642241e3) /* m^2 */
#define GSL_CONST_MKS_BARN (1e-28) /* m^2 */
#define GSL_CONST_MKS_LITER (1e-3) /* m^3 */
#define GSL_CONST_MKS_US_GALLON (3.78541178402e-3) /* m^3 */
#define GSL_CONST_MKS_QUART (9.46352946004e-4) /* m^3 */
#define GSL_CONST_MKS_PINT (4.73176473002e-4) /* m^3 */
#define GSL_CONST_MKS_CUP (2.36588236501e-4) /* m^3 */
#define GSL_CONST_MKS_FLUID_OUNCE (2.95735295626e-5) /* m^3 */
#define GSL_CONST_MKS_TABLESPOON (1.47867647813e-5) /* m^3 */
#define GSL_CONST_MKS_TEASPOON (4.92892159375e-6) /* m^3 */
#define GSL_CONST_MKS_CANADIAN_GALLON (4.54609e-3) /* m^3 */
#define GSL_CONST_MKS_UK_GALLON (4.546092e-3) /* m^3 */
#define GSL_CONST_MKS_MILES_PER_HOUR (4.4704e-1) /* m / s */
#define GSL_CONST_MKS_KILOMETERS_PER_HOUR (2.77777777778e-1) /* m / s */
#define GSL_CONST_MKS_KNOT (5.14444444444e-1) /* m / s */
#define GSL_CONST_MKS_POUND_MASS (4.5359237e-1) /* kg */
#define GSL_CONST_MKS_OUNCE_MASS (2.8349523125e-2) /* kg */
#define GSL_CONST_MKS_TON (9.0718474e2) /* kg */
#define GSL_CONST_MKS_METRIC_TON (1e3) /* kg */
#define GSL_CONST_MKS_UK_TON (1.0160469088e3) /* kg */
#define GSL_CONST_MKS_TROY_OUNCE (3.1103475e-2) /* kg */
#define GSL_CONST_MKS_CARAT (2e-4) /* kg */
#define GSL_CONST_MKS_UNIFIED_ATOMIC_MASS (1.660538782e-27) /* kg */
#define GSL_CONST_MKS_GRAM_FORCE (9.80665e-3) /* kg m / s^2 */
#define GSL_CONST_MKS_POUND_FORCE (4.44822161526e0) /* kg m / s^2 */
#define GSL_CONST_MKS_KILOPOUND_FORCE (4.44822161526e3) /* kg m / s^2 */
#define GSL_CONST_MKS_POUNDAL (1.38255e-1) /* kg m / s^2 */
#define GSL_CONST_MKS_CALORIE (4.1868e0) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_BTU (1.05505585262e3) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_THERM (1.05506e8) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_HORSEPOWER (7.457e2) /* kg m^2 / s^3 */
#define GSL_CONST_MKS_BAR (1e5) /* kg / m s^2 */
#define GSL_CONST_MKS_STD_ATMOSPHERE (1.01325e5) /* kg / m s^2 */
#define GSL_CONST_MKS_TORR (1.33322368421e2) /* kg / m s^2 */
#define GSL_CONST_MKS_METER_OF_MERCURY (1.33322368421e5) /* kg / m s^2 */
#define GSL_CONST_MKS_INCH_OF_MERCURY (3.38638815789e3) /* kg / m s^2 */
#define GSL_CONST_MKS_INCH_OF_WATER (2.490889e2) /* kg / m s^2 */
#define GSL_CONST_MKS_PSI (6.89475729317e3) /* kg / m s^2 */
#define GSL_CONST_MKS_POISE (1e-1) /* kg m^-1 s^-1 */
#define GSL_CONST_MKS_STOKES (1e-4) /* m^2 / s */
#define GSL_CONST_MKS_STILB (1e4) /* cd / m^2 */
#define GSL_CONST_MKS_LUMEN (1e0) /* cd sr */
#define GSL_CONST_MKS_LUX (1e0) /* cd sr / m^2 */
#define GSL_CONST_MKS_PHOT (1e4) /* cd sr / m^2 */
#define GSL_CONST_MKS_FOOTCANDLE (1.076e1) /* cd sr / m^2 */
#define GSL_CONST_MKS_LAMBERT (1e4) /* cd sr / m^2 */
#define GSL_CONST_MKS_FOOTLAMBERT (1.07639104e1) /* cd sr / m^2 */
#define GSL_CONST_MKS_CURIE (3.7e10) /* 1 / s */
#define GSL_CONST_MKS_ROENTGEN (2.58e-4) /* A s / kg */
#define GSL_CONST_MKS_RAD (1e-2) /* m^2 / s^2 */
#define GSL_CONST_MKS_SOLAR_MASS (1.98892e30) /* kg */
#define GSL_CONST_MKS_BOHR_RADIUS (5.291772083e-11) /* m */
#define GSL_CONST_MKS_NEWTON (1e0) /* kg m / s^2 */
#define GSL_CONST_MKS_DYNE (1e-5) /* kg m / s^2 */
#define GSL_CONST_MKS_JOULE (1e0) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_ERG (1e-7) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_STEFAN_BOLTZMANN_CONSTANT (5.67040047374e-8) /* kg / K^4 s^3 */
#define GSL_CONST_MKS_THOMSON_CROSS_SECTION (6.65245893699e-29) /* m^2 */
#define GSL_CONST_MKS_BOHR_MAGNETON (9.27400899e-24) /* A m^2 */
#define GSL_CONST_MKS_NUCLEAR_MAGNETON (5.05078317e-27) /* A m^2 */
#define GSL_CONST_MKS_ELECTRON_MAGNETIC_MOMENT (9.28476362e-24) /* A m^2 */
#define GSL_CONST_MKS_PROTON_MAGNETIC_MOMENT (1.410606633e-26) /* A m^2 */
#define GSL_CONST_MKS_FARADAY (9.64853429775e4) /* A s / mol */
#define GSL_CONST_MKS_ELECTRON_CHARGE (1.602176487e-19) /* A s */
#define GSL_CONST_MKS_VACUUM_PERMITTIVITY (8.854187817e-12) /* A^2 s^4 / kg m^3 */
#define GSL_CONST_MKS_VACUUM_PERMEABILITY (1.25663706144e-6) /* kg m / A^2 s^2 */
#define GSL_CONST_MKS_DEBYE (3.33564095198e-30) /* A s^2 / m^2 */
#define GSL_CONST_MKS_GAUSS (1e-4) /* kg / A s^2 */

/* Library for code generator */

extern double a68g_pow_real_int (double, int);
extern double a68g_pow_real (double, double);
extern void a68g_div_complex (A68_REAL *, A68_REAL *, A68_REAL *);
extern void a68g_sqrt_complex (A68_REAL *, A68_REAL *);
extern void a68g_exp_complex (A68_REAL *, A68_REAL *);
extern void a68g_ln_complex (A68_REAL *, A68_REAL *);
extern void a68g_sin_complex (A68_REAL *, A68_REAL *);
extern void a68g_cos_complex (A68_REAL *, A68_REAL *);

/* INT operators that are inlined in compiled code */

/* OP MOD = (INT, INT) INT */
#define a68g_mod_int(/* int */ i, j) (((i) % (j)) >= 0 ? ((i) % (j)) : ((i) % (j)) + labs (j))

/* OP +:= = (REF INT, INT) REF INT */
#define a68g_plusab_int(/* A68_REF * */ i, /* int */ j) (VALUE ((A68_INT *) ADDRESS (i)) += (j), (i))

/* OP -:= = (REF INT, INT) REF INT */
#define a68g_minusab_int(/* A68_REF * */ i, /* int */ j) (VALUE ((A68_INT *) ADDRESS (i)) -= (j), (i))

/* OP *:= = (REF INT, INT) REF INT */
#define a68g_timesab_int(/* A68_REF * */ i, /* int */ j) (VALUE ((A68_INT *) ADDRESS (i)) *= (j), (i))

/* OP /:= = (REF INT, INT) REF INT */
#define a68g_overab_int(/* A68_REF * */ i, /* int */ j) (VALUE ((A68_INT *) ADDRESS (i)) /= (j), (i))

/* REAL operators that are inlined in compiled code */

/* OP ENTIER = (REAL) INT */
#define a68g_entier(/* double */ x) ((int) floor (x))

/* OP +:= = (REF REAL, REAL) REF REAL */
#define a68g_plusab_real(/* A68_REF * */ i, /* double */ j) (VALUE ((A68_REAL *) ADDRESS (i)) += (j), (i))

/* OP -:= = (REF REAL, REAL) REF REAL */
#define a68g_minusab_real(/* A68_REF * */ i, /* double */ j) (VALUE ((A68_REAL *) ADDRESS (i)) -= (j), (i))

/* OP *:= = (REF REAL, REAL) REF REAL */
#define a68g_timesab_real(/* A68_REF * */ i, /* double */ j) (VALUE ((A68_REAL *) ADDRESS (i)) *= (j), (i))

/* OP /:= = (REF REAL, REAL) REF REAL */
#define a68g_overab_real(/* A68_REF * */ i, /* double */ j) (VALUE ((A68_REAL *) ADDRESS (i)) /= (j), (i))

/* COMPLEX operators that are inlined in compiled code */

/* OP RE = (COMPLEX) REAL */
#define a68g_re_complex(/* A68_REAL * */ z) (RE (z))

/* OP IM = (COMPLEX) REAL */
#define a68g_im_complex(/* A68_REAL * */ z) (IM (z))

/* ABS = (COMPLEX) REAL */
#define a68g_abs_complex(/* A68_REAL * */ z) a68g_hypot (RE (z), IM (z))

/* OP ARG = (COMPLEX) REAL */
#define a68g_arg_complex(/* A68_REAL * */ z) atan2 (IM (z), RE (z))

/* OP +* = (REAL, REAL) COMPLEX */
#define a68g_i_complex(/* A68_REAL * */ z, /* double */ re, im) {\
  STATUS_RE (z) = INITIALISED_MASK;\
  STATUS_IM (z) = INITIALISED_MASK;\
  RE (z) = re;\
  IM (z) = im;}

/* OP - = (COMPLEX) COMPLEX */
#define a68g_minus_complex(/* A68_REAL * */ z, x) {\
  STATUS_RE (z) = INITIALISED_MASK;\
  STATUS_IM (z) = INITIALISED_MASK;\
  RE (z) = -RE (x);\
  IM (z) = -IM (x);}

/* OP CONJ = (COMPLEX) COMPLEX */
#define a68g_conj_complex(/* A68_REAL * */ z, x) {\
  STATUS_RE (z) = INITIALISED_MASK;\
  STATUS_IM (z) = INITIALISED_MASK;\
  RE (z) = RE (x);\
  IM (z) = -IM (x);}

/* OP + = (COMPLEX, COMPLEX) COMPLEX */
#define a68g_add_complex(/* A68_REAL * */ z, x, y) {\
  STATUS_RE (z) = INITIALISED_MASK;\
  STATUS_IM (z) = INITIALISED_MASK;\
  RE (z) = RE (x) + RE (y);\
  IM (z) = IM (x) + IM (y);}

/* OP - = (COMPLEX, COMPLEX) COMPLEX */
#define a68g_sub_complex(/* A68_REAL * */ z, x, y) {\
  STATUS_RE (z) = INITIALISED_MASK;\
  STATUS_IM (z) = INITIALISED_MASK;\
  RE (z) = RE (x) - RE (y);\
  IM (z) = IM (x) - IM (y);}

/* OP * = (COMPLEX, COMPLEX) COMPLEX */
#define a68g_mul_complex(/* A68_REAL * */ z, x, y) {\
  STATUS_RE (z) = INITIALISED_MASK;\
  STATUS_IM (z) = INITIALISED_MASK;\
  RE (z) = RE (x) * RE (y) - IM (x) * IM (y);\
  IM (z) = IM (x) * RE (y) + RE (x) * IM (y);}

/* OP = = (COMPLEX, COMPLEX) BOOL */
#define a68g_eq_complex(/* A68_REAL * */ x, y) (RE (x) == RE (y) && IM (x) == IM (y))

/* OP /= = (COMPLEX, COMPLEX) BOOL */
#define a68g_ne_complex(/* A68_REAL * */ x, y) (! a68g_eq_complex (x, y))

#endif
