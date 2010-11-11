/*!
\file algol68g.h
\brief general definitions for Algol 68 Genie
**/

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

#if ! defined A68G_ALGOL68G_H
#define A68G_ALGOL68G_H

/* Type definitions. */

typedef int ADDR_T, FILE_T, LEAP_T;
typedef unsigned char BYTE_T;
typedef int BOOL_T;
typedef unsigned STATUS_MASK;

/* Includes needed by most files. */

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
#include <string.h>
#include <time.h>

#if defined ENABLE_COMPILER
#include <dlfcn.h>
#endif

#if defined ENABLE_PAR_CLAUSE
#include <pthread.h>
#endif

#if defined ENABLE_CURSES
#include <curses.h>
#if defined ENABLE_WIN32
#undef FD_READ
#undef FD_WRITE
#include <winsock.h>
#endif
#endif

#if defined ENABLE_MACOSX
#define __off_t off_t
#define __pid_t pid_t
#define __mode_t mode_t
#endif

#if defined ENABLE_POSTGRESQL
#include <libpq-fe.h>
#endif

/* System dependencies. */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#if ! defined O_BINARY
#define O_BINARY 0x0000
#endif

#if defined ENABLE_WIN32
typedef unsigned __off_t;
#define S_IRGRP 0x040
#define S_IROTH 0x004
#if defined __MSVCRT__ && defined _environ
#undef _environ
#endif
#endif

#if defined ENABLE_GRAPHICS
#include <plot.h>
#endif

/* Constants. */

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
#define A68_ALIGN(s) ((int) ((s) % A68_ALIGNMENT) == 0 ? (s) : ((s) + A68_ALIGNMENT - (s) % A68_ALIGNMENT))

#define ALIGNED_SIZE_OF(p) ((int) A68_ALIGN (sizeof (p)))
#define MOID_SIZE(p) A68_ALIGN ((p)->size)

/* BUFFER_SIZE exceeds actual requirements. */
#define BUFFER_SIZE (KILOBYTE)
#define SMALL_BUFFER_SIZE 128
#define MAX_ERRORS 10
/* MAX_PRIORITY follows from the revised report. */
#define MAX_PRIORITY 9
/* Stack, heap blocks not smaller than MIN_MEM_SIZE in kB. */
#define MIN_MEM_SIZE (32 * KILOBYTE)
/* MAX_LINE_WIDTH must be smaller than BUFFER_SIZE. */
#define MAX_LINE_WIDTH (BUFFER_SIZE / 2)
#define MOID_WIDTH 80

/* This BYTES_WIDTH is more useful than the usual 4. */
#define BYTES_WIDTH 32
/* This LONG_BYTES_WIDTH is more useful than the usual 8. */
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
#define NO_SWEEP_MASK ((STATUS_MASK) 0x00000040)
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

/* Type definitions. */

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

/* Macro's that work with fat pointers. */

#define REF_SCOPE(z) (((z)->u).scope)
#define REF_HANDLE(z) (((z)->u).handle)
#define REF_POINTER(z) (REF_HANDLE (z)->pointer)
#define REF_OFFSET(z) ((z)->offset)

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

/* Options struct. */

struct OPTIONS_T
{
  OPTION_LIST_T *list;
  BOOL_T backtrace, brackets, check_only, cross_reference, debug, compile, keep, mips, moid_listing, 
    object_listing, optimise, portcheck, pragmat_sema, reductions, regression_test, run, rerun,
    run_script, source_listing, standard_prelude_listing, statistics_listing, 
    strict, stropping, trace, tree_listing, unused, verbose, version; 
  int time_limit, opt_level; 
  STATUS_MASK nodemask;
};

/* Propagator type, that holds information for the interpreter. */

struct PROPAGATOR_T
{
  PROPAGATOR_PROCEDURE *unit;
  NODE_T *source;
};

/* Algol 68 type definitions. */

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
  BOOL_T value;
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
#define STATUS_RE(z) (STATUS (&z[0]))
#define STATUS_IM(z) (STATUS (&z[1]))
#define RE(z) (VALUE (&z[0]))
#define IM(z) (VALUE (&z[1]))

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

/* The FILE mode. */

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
#if defined ENABLE_GRAPHICS
    plPlotter *plotter;
    plPlotterParams *plotter_params;
#endif
    BOOL_T device_made, device_opened;
    A68_REF device, page_size;
    int device_handle /* deprecated*/ , window_x_size, window_y_size;
    double x_coord, y_coord, red, green, blue;
  }
  device;
#if defined ENABLE_POSTGRESQL
  PGconn *connection;
  PGresult *result;
#endif
};


/* Internal type definitions. */

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
  TAG_T *protect_sweep;
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

/* Internal constants. */

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
  BRIEF_ELIF_IF_PART,
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
  PROTECT_FROM_SWEEP,
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

/* Macros. */

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
    abend (reason, info, __FILE__, __LINE__);\
  }}

#if defined ENABLE_CURSES
#define ASSERT(f) {\
  extern BOOL_T curses_active;\
  if (!(f)) {\
    if (curses_active == A68_TRUE) {\
      (void) attrset (A_NORMAL);\
      (void) endwin ();\
      curses_active = A68_FALSE;\
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

/* Miscellaneous macros. */

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

/* External definitions. */

extern ADDR_T fixed_heap_pointer, temp_heap_pointer;
extern BOOL_T no_warnings;
extern BOOL_T in_execution;
extern BOOL_T halt_typing, time_limit_flag, listing_is_safe;
extern BOOL_T get_fixed_heap_allowed;
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
extern void PROTECT_SWEEP_HANDLE (A68_REF *);
extern void UNPROTECT_SWEEP_HANDLE (A68_REF *);
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
extern void print_internal_index (FILE_T, A68_TUPLE *, int);
extern void print_mode_flat (FILE_T, MOID_T *);
extern void protect_from_sweep (NODE_T *);
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

#if defined ENABLE_COMPILER
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

#endif
