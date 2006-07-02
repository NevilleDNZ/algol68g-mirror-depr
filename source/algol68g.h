/*!
\file algol68g.h
\brief
**/

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

#ifndef A68G_ALGOL68G_H
#define A68G_ALGOL68G_H

/* Includes needed by most files. */

#include "version.h"
#include "config.h"
#include "diagnostics.h"
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

#ifdef HAVE_POSIX_THREADS
#include <pthread.h>
#endif

#ifdef HAVE_POSTGRESQL
#include <libpq-fe.h>
#endif

/* System dependencies. */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef O_BINARY
#define O_BINARY 0x0000
#endif

#if defined WIN32_VERSION
/* typedef size_t ssize_t; */
#define S_IRGRP 0x040
#define S_IROTH 0x004
#endif

#if defined WIN32_VERSION && defined __MSVCRT__ && defined _environ
#undef _environ
#endif

#ifdef HAVE_PLOTUTILS
#include <plot.h>
#endif

/* Constants. */

#define KILOBYTE (1024L)
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)

#define LISTING_EXTENSION ".l"
#define A_TRUE 1
#define A_FALSE 0
#define TIME_FORMAT "%A %d-%b-%Y %H:%M:%S"

#define ALIGN_T void *
#define ALIGNMENT ((int) (sizeof (ALIGN_T)))
#define ALIGN(s) ((int) ((s) % ALIGNMENT) == 0 ? (s) : ((s) + ALIGNMENT - (s) % ALIGNMENT))
#define SIZE_OF(p) ((int) ALIGN (sizeof (p)))
#define MOID_SIZE(p) ALIGN ((p)->size)

#define BUFFER_SIZE (KILOBYTE)			/* BUFFER_SIZE exceeds actual requirements. */
#define SMALL_BUFFER_SIZE 128
#define MAX_ERRORS 8
#define MAX_PRIORITY 9				/* Algol 68 requirement. */
#define MIN_MEM_SIZE (256 * KILOBYTE)		/* Stack, heap blocks not smaller than this in kB. */
#define MAX_LINE_WIDTH (BUFFER_SIZE / 2)	/* Must be smaller than BUFFER_SIZE */
#define MOID_WIDTH 80

#define BYTES_WIDTH 32		/* More useful than the usual 4. */
#define LONG_BYTES_WIDTH 64	/* More useful than the usual 8. */

#define A_READ_ACCESS (O_RDONLY)
#define A_WRITE_ACCESS (O_WRONLY | O_CREAT | O_TRUNC)
#define A68_PROTECTION (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)	/* -rw-r--r-- */

#define MAX_INT 	(INT_MAX)
#define MIN_INT 	(INT_MIN)
#define MAX_UNT 	(UINT_MAX)
#define MAX_BITS	(UINT_MAX)

#define TO_UCHAR(c) ((c) >= 0 ? (int) (c) : (int) (UCHAR_MAX + (int) (c) + 1))

#define BLANK_CHAR      ' '
#define CR_CHAR		'\r'
#define EOF_CHAR	TO_UCHAR (EOF)
#define ERROR_CHAR      '*'
#define ESCAPE_CHAR	'\\'
#define EXPONENT_CHAR   'e'
#define FLIP_CHAR       'T'
#define FLOP_CHAR       'F'
#define FORMFEED_CHAR	'\f'
#define NEWLINE_CHAR	'\n'
#define NEWLINE_STRING	"\n"
#define NULL_CHAR	'\0'
#define POINT_CHAR 	'.'
#define QUOTE_CHAR	'"'
#define RADIX_CHAR      'r'
#define TAB_CHAR	'\t'

#define A68G_PI 	3.1415926535897932384626433832795029L	/* pi*/

#define MONADS  	"%^&+-~!?"
#define NOMADS  	"></=*"

#define PRIMAL_SCOPE 0

/*
Some (necessary) macros to overcome the ambiguity in having signed or unsigned
char on various systems. PDP-11s and IBM 370s are still haunting us with this.
*/

#define IS_CNTRL(c) iscntrl ((unsigned char) (c))
#define IS_DIGIT(c) isdigit ((unsigned char) (c))
#define IS_GRAPH(c) isgraph ((unsigned char) (c))
#define IS_LOWER(c) islower ((unsigned char) (c))
#define IS_PRINT(c) isprint ((unsigned char) (c))
#define IS_SPACE(c) isspace ((unsigned char) (c))
#define IS_UPPER(c) isupper ((unsigned char) (c))
#define IS_XDIGIT(c) isxdigit ((unsigned char) (c))
#define TO_LOWER(c) tolower ((unsigned char) (c))
#define TO_UPPER(c) toupper ((unsigned char) (c))

/* Type definitions. */

typedef int ADDR_T, FILE_T, LEAP_T;
typedef unsigned char BYTE_T, BOOL_T;
typedef unsigned STATUS_MASK;

/* Algol 68 type definitions. */

typedef struct A68_ARRAY A68_ARRAY;
typedef struct A68_BITS A68_BITS;
typedef struct A68_BYTES A68_BYTES;
typedef struct A68_CHANNEL A68_CHANNEL;
typedef struct A68_CHAR A68_CHAR, A68_BOOL;
typedef struct A68_COLLITEM A68_COLLITEM;
typedef struct A68_FILE A68_FILE;
typedef struct A68_FORMAT A68_FORMAT;
typedef struct A68_HANDLE A68_HANDLE;
typedef struct A68_INT A68_INT;
typedef struct A68_LONG_BYTES A68_LONG_BYTES;
typedef struct A68_POINTER A68_POINTER;
typedef struct A68_PROCEDURE A68_PROCEDURE;
typedef struct A68_REAL A68_REAL;
typedef struct A68_REF A68_REF;
typedef struct A68_TUPLE A68_TUPLE;
typedef struct POSTULATE_T POSTULATE_T;
typedef struct FILES_T FILES_T;
typedef struct GENIE_INFO_T GENIE_INFO_T;
typedef struct KEYWORD_T KEYWORD_T;
typedef struct DIAGNOSTIC_T DIAGNOSTIC_T;
typedef struct MODES_T MODES_T;
typedef struct MODULE_T MODULE_T;
typedef struct MOID_LIST_T MOID_LIST_T;
typedef struct MOID_T MOID_T;
typedef struct NODE_INFO_T NODE_INFO_T;
typedef struct NODE_T NODE_T;
typedef struct OPTIONS_T OPTIONS_T;
typedef struct OPTION_LIST_T OPTION_LIST_T;
typedef struct PACK_T PACK_T;
typedef struct PRELUDE_T PRELUDE_T;
typedef struct PROPAGATOR_T PROPAGATOR_T;
typedef struct REFINEMENT_T REFINEMENT_T;
typedef struct SOID_LIST_T SOID_LIST_T;
typedef struct SOID_T SOID_T;
typedef struct SOURCE_LINE_T SOURCE_LINE_T;
typedef struct SYMBOL_TABLE_T SYMBOL_TABLE_T;
typedef struct TAG_T TAG_T;
typedef struct TOKEN_T TOKEN_T;

struct A68_HANDLE
{
  STATUS_MASK status;
  ADDR_T offset;
  int size;
  MOID_T *type;
  A68_HANDLE *next, *previous;
};

struct A68_INT
{
  STATUS_MASK status;
  int value;
};

struct A68_BITS
{
  STATUS_MASK status;
  unsigned value;
};

struct A68_REAL
{
  STATUS_MASK status;
  double value;
};

struct A68_CHAR
{
  STATUS_MASK status;
  signed char value;
};

struct A68_BYTES
{
  STATUS_MASK status;
  char value[BYTES_WIDTH + 1];
};

struct A68_LONG_BYTES
{
  STATUS_MASK status;
  char value[LONG_BYTES_WIDTH + 1];
};

struct A68_REF
{
  STATUS_MASK status;
  BYTE_T *segment;
  ADDR_T offset, scope;
  A68_HANDLE *handle;
};

struct A68_FORMAT
{
  STATUS_MASK status;
  NODE_T *body;
  ADDR_T environ;
};

struct A68_POINTER
{
  STATUS_MASK status;
  void *value;
};

struct A68_PROCEDURE
{
  STATUS_MASK status;
  void *body;
  A68_REF locale;
  MOID_T *proc_mode;
  ADDR_T environ;
};

struct A68_TUPLE
{
  int upper_bound, lower_bound, shift;
  int span, k;
};

struct A68_ARRAY
{
  MOID_T *type;
  int dimensions, elem_size;
  ADDR_T slice_offset, field_offset;
  A68_REF array;
};

struct A68_CHANNEL
{
  STATUS_MASK status;
  BOOL_T reset, set, get, put, bin, draw, compress;
};

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
#ifdef HAVE_PLOTUTILS
    plPlotter *plotter;
    plPlotterParams *plotter_params;
#endif
    BOOL_T device_made, device_opened;
    A68_REF device, page_size;
    int device_handle /* deprecated*/ , window_x_size, window_y_size;
    double x_coord, y_coord, red, green, blue;
  }
  device;
#ifdef HAVE_POSTGRESQL
PGconn *connection;
PGresult *result;
#endif
};

struct A68_COLLITEM
{
  STATUS_MASK status;
  int count;
};

/* Internal type definitions. */

typedef PROPAGATOR_T PROPAGATOR_PROCEDURE (NODE_T *);
typedef void GENIE_PROCEDURE (NODE_T *);

struct REFINEMENT_T
{
  REFINEMENT_T *next;
  char *name;
  SOURCE_LINE_T *line_defined, *line_applied;
  int applications;
  NODE_T *tree, *begin, *end;
};

struct PROPAGATOR_T
{
  PROPAGATOR_PROCEDURE *unit;
  NODE_T *source;
};

struct SOURCE_LINE_T
{
  char *string, *filename;
  DIAGNOSTIC_T *diagnostics;
  int number, print_status;
  BOOL_T list;
  MODULE_T *module;
  NODE_T *top_node;
  SOURCE_LINE_T *next, *previous;
};

struct DIAGNOSTIC_T
{
  int attribute, number;
  NODE_T *where;
  SOURCE_LINE_T *line;
  char *text, *symbol;
  DIAGNOSTIC_T *next;
};

struct TOKEN_T
{
  char *text;
  TOKEN_T *less, *more;
};

struct KEYWORD_T
{
  int attribute;
  char *text;
  KEYWORD_T *less, *more;
};

struct TAG_T
{
  SYMBOL_TABLE_T *symbol_table;
  MOID_T *type;
  NODE_T *node, *unit;
  char *value;
  GENIE_PROCEDURE *procedure;
  BOOL_T scope_assigned, use, in_proc, stand_env_proc, loc_assigned, portable;
  int priority, heap, scope, size, youngest_environ;
  ADDR_T offset;
  TAG_T *next, *body;
};

struct SYMBOL_TABLE_T
{
  int level, nest, attribute /* MAIN, PRELUDE_T*/ ;
  char *environ;
  BOOL_T empty_table, initialise_frame, initialise_anon, proc_ops;
  ADDR_T ap_increment;
  SYMBOL_TABLE_T *previous;
  TAG_T *identifiers, *operators, *priority, *indicants, *labels, *anonymous;
  MOID_T *moids;
  NODE_T *jump_to, *inits;
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

struct MOID_T
{
  int attribute, number, dimensions, short_id, size;
  BOOL_T has_ref, has_flex, has_rows, in_standard_environ, well_formed, use, portable;
  NODE_T *node;
  PACK_T *pack;
  MOID_T *sub, *equivalent_mode, *slice, *deflexed_mode, *name, *multiple_mode, *trim, *next;
};

struct MOID_LIST_T
{
  SYMBOL_TABLE_T *coming_from_level;
  MOID_T *type;
  MOID_LIST_T *next;
};

struct NODE_INFO_T
{
  MODULE_T *module;
  int PROCEDURE_LEVEL, PROCEDURE_NUMBER, priority;
  char *char_in_line, *symbol, *expr;
  SOURCE_LINE_T *line;
};

struct GENIE_INFO_T
{
  PROPAGATOR_T propagator;
  BOOL_T whether_coercion, whether_new_lexical_level, seq_set;
  int level, argsize;
  NODE_T *parent, *seq;
  BYTE_T *offset;
  void *constant;
};

struct NODE_T
{
  STATUS_MASK mask;
  GENIE_INFO_T genie;
  MOID_T *type, *partial_proc, *partial_locale;
  NODE_INFO_T *info;
  NODE_T *next, *previous, *sub, *do_od_part, *inits, *nest;
  PACK_T *pack;
  REFINEMENT_T *refinement;
  SYMBOL_TABLE_T *symbol_table;
  TAG_T *tag, *protect_sweep;
  int attribute, annotation, par_level;
  BOOL_T dns, error;
};

struct POSTULATE_T
{
  MOID_T *a, *b;
  POSTULATE_T *next;
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

struct PRELUDE_T
{
  char *file_name;
  PRELUDE_T *next;
};

struct A68_STREAM
{
  char *name;
  FILE_T fd;
  int opened, writemood;
};

struct FILES_T
{
  char *path, *generic_name;
  struct A68_STREAM source, listing;
};

struct OPTION_LIST_T
{
  char *str;
  int scan;
  BOOL_T processed;
  SOURCE_LINE_T *line;
  OPTION_LIST_T *next;
};

struct OPTIONS_T
{
  OPTION_LIST_T *list;
  BOOL_T source_listing, standard_prelude_listing, tree_listing, verbose, version, cross_reference, check_only, statistics_listing, pragmat_sema, moid_listing, unused, trace, regression_test, stropping, brackets, reductions, portcheck;
  int time_limit, run, debug;
  STATUS_MASK nodemask;
};

struct MODES_T
{
  MOID_T *UNDEFINED, *ERROR, *HIP, *ROWS, *VACUUM, *C_STRING, *COLLITEM,
    *SEMA, *VOID, *INT, *REAL, *BOOL, *CHAR, *BITS, *BYTES, *REF_INT,
    *REF_REAL, *REF_BOOL, *REF_CHAR, *REF_BITS, *REF_BYTES, *ROW_REAL,
    *ROWROW_REAL, *ROW_BITS, *ROW_LONG_BITS, *ROW_LONGLONG_BITS,
    *REF_ROW_REAL, *REF_ROWROW_REAL, *ROW_CHAR, *ROW_ROW_CHAR, *STRING,
    *ROW_STRING, *NUMBER, *REF_ROW_CHAR, *REF_STRING, *PROC_ROW_CHAR,
    *PROC_STRING, *PROC_VOID, *PROC_REF_FILE_BOOL, *PROC_REF_FILE_VOID, *SIMPLIN, *SIMPLOUT, *ROW_SIMPLIN, *ROW_SIMPLOUT, *CHANNEL, *FILE, *REF_FILE, *ROW_BOOL, *ROW_INT, *FORMAT, *REF_FORMAT,
/* Multiple precision*/
   *LONG_INT, *REF_LONG_INT, *LONGLONG_INT, *REF_LONGLONG_INT, *LONG_REAL,
    *REF_LONG_REAL, *LONGLONG_REAL, *REF_LONGLONG_REAL, *COMPLEX,
    *REF_COMPLEX, *LONG_COMPLEX, *REF_LONG_COMPLEX, *LONGLONG_COMPLEX,
    *REF_LONGLONG_COMPLEX, *COMPL, *REF_COMPL, *LONG_COMPL, *REF_LONG_COMPL, *LONGLONG_COMPL, *REF_LONGLONG_COMPL, *LONG_BITS, *REF_LONG_BITS, *LONGLONG_BITS, *REF_LONGLONG_BITS, *LONG_BYTES, *REF_LONG_BYTES,
/* Other*/
   *PIPE, *REF_PIPE, *REF_REF_FILE;
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
};

/* Internal constants. */

enum
{
  UPPER_STROPPING = 1, QUOTE_STROPPING
};

enum
{
  NULL_MASK 			= 0x00000000L,
  INITIALISED_MASK 		= 0x00000001L,
  NO_SWEEP_MASK 		= 0x00000002L,
  COOKIE_MASK 			= 0x00000004L,
  ROW_COLOUR_MASK 		= 0x00000008L,
  CONSTANT_MASK 		= 0x00000010L,
  ASSIGNED_MASK 		= 0x00000020L,
  ALLOCATED_MASK 		= 0x00000040L,
  STANDENV_PROCEDURE_MASK 	= 0x00000080L,
  COLOUR_MASK 			= 0x00000100L,
  PROCEDURE_MASK 		= 0x00000200L,
  OPTIMAL_MASK 			= 0x00000400L,
  SERIAL_MASK 			= 0x00000800L,
  CROSS_REFERENCE_MASK 		= 0x00001000L,
  TREE_MASK 			= 0x00002000L,
  CODE_MASK 			= 0x00004000L,
  SYNTAX_TREE_MASK 		= 0x00008000L,
  SOURCE_MASK 			= 0x00010000L,
  TRACE_MASK 			= 0x00020000L,
  ASSERT_MASK 			= 0x00040000L,
  BREAKPOINT_MASK 		= 0x00080000L,
  SKIP_PROCEDURE_MASK  		= 0x00100000L,
  PARTIAL_CALL_MASK		= 0x00200000L
};

enum MODE_ATTRIBUTES
{
  MODE_NO_CHECK,
  MODE_INT,
  MODE_LONG_INT,
  MODE_LONGLONG_INT,
  MODE_REAL,
  MODE_LONG_REAL,
  MODE_LONGLONG_REAL,
  MODE_COMPLEX,
  MODE_LONG_COMPLEX,
  MODE_LONGLONG_COMPLEX,
  MODE_BOOL,
  MODE_CHAR,
  MODE_BITS,
  MODE_LONG_BITS,
  MODE_LONGLONG_BITS,
  MODE_BYTES,
  MODE_LONG_BYTES,
  MODE_FILE,
  MODE_FORMAT,
  MODE_PIPE
};

enum ATTRIBUTES
{
  ACCO_SYMBOL = 1,
  ALT_DO_PART,
  ALT_DO_SYMBOL,
  ALT_EQUALS_SYMBOL,
  ALT_FORMAL_BOUNDS_LIST,
  ANDF_SYMBOL,
  ANDTH_SYMBOL,
  AND_FUNCTION,
  ARGUMENT,
  ARGUMENT_LIST,
  ASSERTION,
  ASSERT_SYMBOL,
  ASSIGNATION,
  ASSIGN_SYMBOL,
  ASSIGN_TO_SYMBOL,
  AT_SYMBOL,
  BEGIN_SYMBOL,
  BITS_DENOTER,
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
  BYTES_SYMBOL,
  BY_PART,
  BY_SYMBOL,
  CALL,
  CASE_PART,
  CASE_SYMBOL,
  CAST,
  CHANNEL_SYMBOL,
  CHAR_SYMBOL,
  CHOICE,
  CHOICE_PATTERN,
  CLOSED_CLAUSE,
  CLOSE_SYMBOL,
  CODE_CLAUSE,
  CODE_SYMBOL,
  COLLATERAL_CLAUSE,
  COLLECTION,
  COLON_SYMBOL,
  COMMA_SYMBOL,
  COMPLEX_PATTERN,
  COMPLEX_SYMBOL,
  COMPL_SYMBOL,
  CONDITIONAL_CLAUSE,
  DECLARATION_LIST,
  DECLARER,
  DEFINING_IDENTIFIER,
  DEFINING_INDICANT,
  DEFINING_OPERATOR,
  DENOTER,
  DEPROCEDURING,
  DEREFERENCING,
  DOTDOT_SYMBOL,
  DOWNTO_SYMBOL,
  DO_PART,
  DO_SYMBOL,
  DYNAMIC_REPLICATOR,
  EDOC_SYMBOL,
  ELIF_IF_PART,
  ELIF_PART,
  ELIF_SYMBOL,
  ELSE_BAR_SYMBOL,
  ELSE_OPEN_PART,
  ELSE_PART,
  ELSE_SYMBOL,
  ELSF_SYMBOL,
  EMPTY_SYMBOL,
  ENCLOSED_CLAUSE,
  END_SYMBOL,
  ENQUIRY_CLAUSE,
  ENVIRON_NAME,
  ENVIRON_SYMBOL,
  EQUALS_SYMBOL,
  ERROR,
  ESAC_SYMBOL,
  EXIT_SYMBOL,
  EXPONENT_FRAME,
  FALSE_SYMBOL,
  FIELD_IDENTIFIER,
  FILE_SYMBOL,
  FIXED_C_PATTERN,
  FI_SYMBOL,
  FLEX_SYMBOL,
  FLOAT_C_PATTERN,
  FORMAL_BOUNDS,
  FORMAL_BOUNDS_LIST,
  FORMAL_DECLARERS,
  FORMAL_DECLARERS_LIST,
  FORMAT_A_FRAME,
  FORMAT_DELIMITER_SYMBOL,
  FORMAT_D_FRAME,
  FORMAT_E_FRAME,
  FORMAT_ITEM_A,
  FORMAT_ITEM_B,
  FORMAT_ITEM_C,
  FORMAT_ITEM_CLOSE,
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
  FORMAT_ITEM_OPEN,
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
  FORMAT_I_FRAME,
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
  GENERAL_PATTERN,
  GENERATOR,
  GENERIC_ARGUMENT,
  GENERIC_ARGUMENT_LIST,
  GOTO_SYMBOL,
  GO_SYMBOL,
  HEAP_SYMBOL,
  IDENTIFIER,
  IDENTITY_DECLARATION,
  IDENTITY_RELATION,
  IF_PART,
  IF_SYMBOL,
  INDICANT,
  INITIALISER_SERIES,
  INSERTION,
  INTEGER_CASE_CLAUSE,
  INTEGER_CHOICE_CLAUSE,
  INTEGER_IN_PART,
  INTEGER_OUT_PART,
  INTEGRAL_C_PATTERN,
  INTEGRAL_MOULD,
  INTEGRAL_PATTERN,
  INT_DENOTER,
  INT_SYMBOL,
  IN_SYMBOL,
  IN_TYPE_MODE,
  ISNT_SYMBOL,
  IS_SYMBOL,
  JUMP,
  LABEL,
  LABELED_UNIT,
  LABEL_IDENTIFIER,
  LABEL_SEQUENCE,
  LITERAL,
  LOC_SYMBOL,
  LONGETY,
  LONG_SYMBOL,
  LOOP_CLAUSE,
  MAIN_SYMBOL,
  MODE_DECLARATION,
  MODE_SYMBOL,
  MONADIC_FORMULA,
  MONAD_SEQUENCE,
  NIHIL,
  NIL_SYMBOL,
  OCCA_SYMBOL,
  OD_SYMBOL,
  OF_SYMBOL,
  OPEN_PART,
  OPEN_SYMBOL,
  OPERATOR,
  OPERATOR_DECLARATION,
  OPERATOR_PLAN,
  OP_SYMBOL,
  OREL_SYMBOL,
  ORF_SYMBOL,
  OR_FUNCTION,
  OUSE_CASE_PART,
  OUSE_SYMBOL,
  OUT_PART,
  OUT_SYMBOL,
  OUT_TYPE_MODE,
  PARALLEL_CLAUSE,
  PARAMETER,
  PARAMETER_LIST,
  PARAMETER_PACK,
  PARTICULAR_PROGRAM,
  PAR_SYMBOL,
  PATTERN,
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
  QUALIFIER,
  RADIX_FRAME,
  REAL_DENOTER,
  REAL_PATTERN,
  REAL_SYMBOL,
  REF_SYMBOL,
  REPLICATOR,
  ROUTINE_TEXT,
  ROUTINE_UNIT,
  ROWING,
  ROWS_SYMBOL,
  ROW_CHAR_DENOTER,
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
  SOME_CLAUSE,
  SPECIFIED_UNIT,
  SPECIFIED_UNIT_LIST,
  SPECIFIED_UNIT_UNIT,
  SPECIFIER,
  STANDARD,
  STATIC_REPLICATOR,
  STOWED_MODE,
  STRING_C_PATTERN,
  STRING_PATTERN,
  STRING_SYMBOL,
  STRUCTURED_FIELD,
  STRUCTURED_FIELD_LIST,
  STRUCTURE_PACK,
  STRUCT_SYMBOL,
  STYLE_II_COMMENT_SYMBOL,
  STYLE_I_COMMENT_SYMBOL,
  STYLE_I_PRAGMAT_SYMBOL,
  SUB_SYMBOL,
  SUB_UNIT,
  TERTIARY,
  THEF_SYMBOL,
  THEN_BAR_SYMBOL,
  THEN_PART,
  THEN_SYMBOL,
  TO_PART,
  TO_SYMBOL,
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
  VOIDING,
  VOID_SYMBOL,
  WHILE_PART,
  WHILE_SYMBOL,
  WIDENING,
  WILDCARD
};

enum
{ VIRTUAL_DECLARER_MARK, ACTUAL_DECLARER_MARK, FORMAL_DECLARER_MARK };

enum
{ NO_SORT, SOFT, WEAK, MEEK, FIRM, STRONG };

enum
{
  NORMAL_IDENTIFIER, ERROR_IDENTIFIER, LOOP_IDENTIFIER, FORMAT_IDENTIFIER,
  PARAMETER_IDENTIFIER, SPECIFIER_IDENTIFIER, LOCAL_LABEL, EXTERN_LABEL,
  ANONYMOUS, PROTECT_FROM_SWEEP
};

enum
{ NOT_PRINTED, TO_PRINT, PRINTED };

enum
{ A_ERROR = 1, A_SYNTAX_ERROR, A_WARNING, A_RUNTIME_ERROR, A_STORAGE_ERROR, A_ALL_DIAGNOSTICS, FORCE_DIAGNOSTIC = 128, A_FORCE_QUIT = 256 };

enum
{ NO_DEFLEXING = 1, SAFE_DEFLEXING, ALIAS_DEFLEXING, FORCE_DEFLEXING, SKIP_DEFLEXING };

/* Macros. */

#define ABS(n) ((n) >= 0 ? (n) : -(n))

#define SIGN(n) ((n) == 0 ? 0 : ((n) > 0 ? 1 : -1))

#ifdef NO_INLINE

#define COPY(d, s, n) memcpy (d, s, n)
#define MOVE(d, s, n) memmove (d, s, n)
#define FILL(d, s, n) memset (d, s, n)

#else

#define COPY(d, s, n) {\
  int m_k = (n); BYTE_T *m_u = (BYTE_T *) (d), *m_v = (BYTE_T *) (s);\
  while (m_k--) {*m_u++ = *m_v++;}}

#define MOVE(d, s, n) {\
  int m_k = (int) (n); BYTE_T *m_d = (BYTE_T *) (d), *m_s = (BYTE_T *) (s);\
  if (m_s < m_d) {\
    m_d += m_k; m_s += m_k;\
    while (m_k--) {*(--m_d) = *(--m_s);}\
  } else {\
    while (m_k--) {*(m_d++) = *(m_s++);}\
  }}

#define FILL(d, s, n) {\
   int m_k = (n); BYTE_T *m_u = (BYTE_T *) (d), m_v = (BYTE_T) (s);\
   while (m_k--) {*m_u++ = m_v;}}

#endif

#define ABNORMAL_END(p, reason, info) {\
  if (p) {\
    abend (reason, info, __FILE__, __LINE__);\
  }}

#define RESET_ERRNO {errno = 0;}

#define A68_UNION A68_POINTER
#define UNION_OFFSET (SIZE_OF (A68_UNION))

/* Miscellaneous misery. */

#define ANNOTATION(p) ((p)->annotation)
#define ATTRIBUTE(p) ((p)->attribute)
#define DEFLEX(p) (DEFLEXED (p) != NULL ? DEFLEXED(p) : (p))
#define DEFLEXED(p) ((p)->deflexed_mode)
#define DIMENSION(p) ((p)->dimensions)
#define DNS(p) ((p)->dns)
#define ENVIRON(p) ((p)->environ)
#define EQUIVALENT(p) ((p)->equivalent_mode)
#define EXIT_COMPILATION longjmp(exit_compilation, 1)
#define FILE_DEREF(p) ((A68_FILE *) ADDRESS (p))
#define FORWARD(p) ((p) = NEXT (p))
#define GENIE_INFO(p) ((p)->genie_info)
#define HEAP(p) ((p)->heap)
#define INFO(p) ((p)->info)
#define LEX_LEVEL(p) (SYMBOL_TABLE (p)->level)
#define LINE(p) ((p)->info->line)
#define MASK(p) ((p)->mask)
#define MODE(p) (a68_modes.p)
#define MODULE(p) (INFO (p)->module)
#define MOID(p) ((p)->type)
#define MULTIPLE(p) ((p)->multiple_mode)
#define NAME(p) ((p)->name)
#define NEST(p) ((p)->nest)
#define NEXT(p) ((p)->next)
#define NEXT_SUB(p) (NEXT (SUB (p)))
#define NODE(p) ((p)->node)
#define NUMBER(p) ((p)->number)
#define PACK(p) ((p)->pack)
#define PARENT(p) ((p)->genie.parent)
#define PAR_LEVEL(p) ((p)->par_level)
#define PREVIOUS(p) ((p)->previous)
#define PRIO(p) ((p)->priority)
#define SEQUENCE(p) ((p)->genie.seq)
#define SEQUENCE_SET(p) ((p)->genie.seq_set)
#define SLICE(p) ((p)->slice)
#define SORT(p) ((p)->sort)
#define STATUS(p) ((p)->status)
#define SUB(p) ((p)->sub)
#define SUB_NEXT(p) (SUB (NEXT (p)))
#define SYMBOL(p) ((p)->info->symbol)
#define SYMBOL_TABLE(p) ((p)->symbol_table)
#define TAX(p) ((p)->tag)
#define TEXT(p) ((p)->text)
#define TRIM(p) ((p)->trim)
#define VALUE(p) ((p)->value)
#define WHETHER(p, s) (ATTRIBUTE (p) == (s))
#define WHETHER_LITERALLY(p, s) (strcmp (SYMBOL (p), s) == 0)
#define WHETHER_NOT(p, s) (ATTRIBUTE (p) != (s))

#define SCAN_ERROR(c, u, v, txt) if (c) {scan_error (u, v, txt);}

/* External definitions. */

extern ADDR_T fixed_heap_pointer, temp_heap_pointer;
extern BOOL_T a68c_diags, gnu_diags, no_warnings;
extern BOOL_T halt_typing, sys_request_flag, time_limit_flag, listing_is_safe;
extern BOOL_T tree_listing_safe, cross_reference_safe, moid_listing_safe;
extern BYTE_T *system_stack_offset;
extern KEYWORD_T *top_keyword;
extern MODES_T a68_modes;
extern MODULE_T *current_module;
extern MODULE_T a68_prog;
extern MOID_LIST_T *top_moid_list, *old_moid_list;
extern POSTULATE_T *top_postulate, *old_postulate;
extern SYMBOL_TABLE_T *stand_env;
extern TAG_T *error_tag;
extern TOKEN_T *top_token;
extern char output_line[], edit_line[], input_line[];
extern double a68g_acosh (double);
extern double a68g_asinh (double);
extern double a68g_atanh (double);
extern double a68g_hypot (double, double);
extern double a68g_log1p (double);
extern double begin_of_time;
extern double garbage_seconds;
extern int error_count, warning_count, run_time_error_count;
extern int garbage_collects;
extern int source_scan;
extern int stack_size;
extern int symbol_table_count, mode_count;
extern int term_width;
extern jmp_buf exit_compilation;

extern int global_argc;
extern char **global_argv;

extern A68_REF heap_generator (NODE_T *, MOID_T *, int);
extern ADDR_T calculate_internal_index (A68_TUPLE *, int);
extern BOOL_T increment_internal_index (A68_TUPLE *, int);
extern BOOL_T lexical_analyzer (MODULE_T *);
extern BOOL_T match_string (char *, char *, char);
extern BOOL_T modes_equivalent (MOID_T *, MOID_T *);
extern BOOL_T set_options (MODULE_T *, OPTION_LIST_T *, BOOL_T);
extern BOOL_T whether (NODE_T * p, ...);
extern BOOL_T whether_coercion (NODE_T *);
extern BOOL_T whether_firm (MOID_T *, MOID_T *);
extern BOOL_T whether_modes_equal (MOID_T *, MOID_T *, int);
extern BOOL_T whether_new_lexical_level (NODE_T *);
extern BOOL_T whether_subset (MOID_T *, MOID_T *, int);
extern BOOL_T whether_unitable (MOID_T *, MOID_T *, int);
extern BYTE_T *get_fixed_heap_space (size_t);
extern BYTE_T *get_temp_heap_space (size_t);
extern KEYWORD_T *find_keyword (KEYWORD_T *, char *);
extern KEYWORD_T *find_keyword_from_attribute (KEYWORD_T *, int);
extern MOID_T *add_mode (MOID_T **, int, int, NODE_T *, MOID_T *, PACK_T *);
extern MOID_T *depref_completely (MOID_T *);
extern MOID_T *new_moid (void);
extern MOID_T *unites_to (MOID_T *, MOID_T *);
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
extern char *moid_to_string (MOID_T *, int);
extern char *new_fixed_string (char *);
extern char *new_string (char *);
extern char *non_terminal_string (char *, int);
extern char *read_string_from_tty (char *);
extern char *standard_environ_proc_name (GENIE_PROCEDURE);
extern char digit_to_char (int);
extern double seconds (void);
extern double ten_to_the_power (int);
extern int count_pack_members (PACK_T *);
extern int get_row_size (A68_TUPLE *, int);
extern int heap_available ();
extern int moid_size (MOID_T *);
extern int whether_identifier_or_label_global (SYMBOL_TABLE_T *, char *);
extern size_t io_read_string (FILE_T, char *, size_t);
extern ssize_t io_read (FILE_T, void *, size_t);
extern ssize_t io_read_conv (FILE_T, void *, size_t);
extern ssize_t io_write (FILE_T, const void *, size_t);
extern ssize_t io_write_conv (FILE_T, const void *, size_t);
extern unsigned long a68g_strtoul (char *, char **, int);
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
extern void assign_offsets (NODE_T *);
extern void assign_offsets_packs (MOID_LIST_T *);
extern void assign_offsets_table (SYMBOL_TABLE_T *);
extern void bind_format_tags_to_tree (NODE_T *);
extern void bind_routine_tags_to_tree (NODE_T *);
extern void bind_tag (TAG_T **, TAG_T *);
extern void bottom_up_error_check (NODE_T *);
extern void bottom_up_parser (NODE_T *);
extern void bufcat (char *, char *, int);
extern void bufcpy (char *, char *, int);
extern void check_parenthesis (NODE_T *);
extern void coercion_inserter (NODE_T *);
extern void collect_taxes (NODE_T *);
extern void contract_union (MOID_T *, int *);
extern void default_mem_sizes ();
extern void default_options (MODULE_T *);
extern void diagnostic_line (int, SOURCE_LINE_T *, char *, char *, ...);
extern void diagnostic_node (int, NODE_T *, char *, ...);
extern void diagnostics_to_terminal (SOURCE_LINE_T *, int);
extern void discard_heap (void);
extern void dump_heap ();
extern void finalise_symbol_table_setup (NODE_T *, int);
extern void free_heap (void);
extern void free_postulates (void);
extern void genie_init_heap (NODE_T *, MODULE_T *);
extern void get_max_simplout_size (NODE_T *);
extern void get_moid_list (MOID_LIST_T **, NODE_T *);
extern void get_refinements (MODULE_T *);
extern void get_stack_size (void);
extern void init_curses (void);
extern void init_heap (void);
extern void init_moid_list ();
extern void init_options (MODULE_T *);
extern void init_postulates (void);
extern void init_tty (void);
extern void initialise_internal_index (A68_TUPLE *, int);
extern void io_close_tty_line (void);
extern void io_write_string (FILE_T, const char *);
extern void isolate_options (MODULE_T *, char *, SOURCE_LINE_T *);
extern void jumps_from_procs (NODE_T * p);
extern void maintain_mode_table (NODE_T *);
extern void make_postulate (POSTULATE_T **, MOID_T *, MOID_T *);
extern void make_soid (SOID_T *, int, MOID_T *, int);
extern void make_special_mode (MOID_T **, int);
extern void make_standard_environ (void);
extern void make_sub (NODE_T *, NODE_T *, int);
extern void mark_auxilliary (NODE_T *);
extern void mark_moids (NODE_T *);
extern void math_rte (NODE_T *, BOOL_T, MOID_T *, const char *);
extern void mode_checker (NODE_T *);
extern void monitor_error (char *, char *);
extern void msg (int, int, NODE_T *, SOID_T *, SOID_T *, char *);
extern void online_help (FILE_T);
extern void portcheck (NODE_T *);
extern void preliminary_symbol_table_setup (NODE_T *);
extern void print_internal_index (FILE_T, A68_TUPLE *, int);
extern void protect_from_sweep (NODE_T *);
extern void prune_echoes (MODULE_T *, OPTION_LIST_T *);
extern void put_refinements (MODULE_T *);
extern void read_env_options (MODULE_T *);
extern void read_rc_options (MODULE_T *);
extern void rearrange_goto_less_jumps (NODE_T *);
extern void remove_empty_symbol_tables (NODE_T *);
extern void remove_file_type (char *);
extern void renumber_nodes (NODE_T *, int *);
extern void reset_max_simplout_size (void);
extern void reset_moid_list ();
extern void reset_postulates (void);
extern void reset_symbol_table_nest_count (NODE_T *);
extern void scan_error (SOURCE_LINE_T *, char *, char *);
extern void scope_checker (NODE_T *);
extern void set_moid_sizes (MOID_LIST_T *);
extern void set_nests (NODE_T *, NODE_T *);
extern void set_par_level (NODE_T *, int);
extern void set_up_mode_table (NODE_T *);
extern void set_up_tables ();
extern void source_listing (MODULE_T *);
extern void state_license (FILE_T);
extern void state_version (FILE_T);
extern void substitute_brackets (NODE_T *);
extern void tie_label_to_serial (NODE_T *);
extern void tie_label_to_unit (NODE_T *);
extern void top_down_parser (NODE_T *);
extern void victal_checker (NODE_T *);
extern void warn_for_unused_tags (NODE_T *);
extern void where (FILE_T, NODE_T *);
extern void widen_denoter (NODE_T *);
extern void write_listing (MODULE_T *);
extern void write_source_line (FILE_T, SOURCE_LINE_T *, NODE_T *, BOOL_T);

#endif
