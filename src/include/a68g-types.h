//! @file a68g-types.h
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2022 J. Marcel van der Veer <algol68g@xs4all.nl>.
//
//! @section License
//
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the 
// Free Software Foundation; either version 3 of the License, or 
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
// more details. You should have received a copy of the GNU General Public 
// License along with this program. If not, see <http://www.gnu.org/licenses/>.

#if !defined (__A68G_TYPES_H__)
#define __A68G_TYPES_H__

// Type definitions

#define COMPLEX_T double complex

typedef int LEAP_T;
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
typedef struct A68_PROCEDURE A68_PROCEDURE;
typedef struct A68_REF A68_REF, A68_ROW;
typedef struct A68_SOUND A68_SOUND;
typedef struct A68_STREAM A68_STREAM;
typedef struct A68_TUPLE A68_TUPLE;
typedef struct A68_UNION A68_UNION;
typedef struct ACTIVATION_RECORD ACTIVATION_RECORD;
typedef struct DEC_T DEC_T;
typedef struct DIAGNOSTIC_T DIAGNOSTIC_T;
typedef struct FILES_T FILES_T;
typedef struct GINFO_T GINFO_T;
typedef struct KEYWORD_T KEYWORD_T;
typedef struct LINE_T LINE_T;
typedef struct MODES_T MODES_T;
typedef struct MOID_T MOID_T;
typedef struct NODE_INFO_T NODE_INFO_T;
typedef struct OPTION_LIST_T OPTION_LIST_T;
typedef struct OPTIONS_T OPTIONS_T;
typedef struct PACK_T PACK_T;
typedef struct POSTULATE_T POSTULATE_T;
typedef struct PROP_T PROP_T;
typedef struct REFINEMENT_T REFINEMENT_T;
typedef struct SOID_T SOID_T;
typedef struct TABLE_T TABLE_T;
typedef struct TAG_T TAG_T;
typedef struct TOKEN_T TOKEN_T;
typedef unt FILE_T, MOOD_T;
typedef void GPROC (NODE_T *);
typedef int CHAR_T;
typedef PROP_T PROP_PROC (NODE_T *);
typedef struct A68_REAL A68_REAL;
typedef MP_T A68_LONG[DEFAULT_DOUBLE_DIGITS + 2];

typedef unt char BYTE_T;
typedef BYTE_T *A68_STRUCT;

struct A68_REAL
{
  STATUS_MASK_T status;
  REAL_T value;
} ALIGNED;

struct DEC_T
{
  char *text;
  int level;
  DEC_T *sub, *less, *more;
};

struct ACTIVATION_RECORD
{
  ADDR_T static_link, dynamic_link, dynamic_scope, parameters;
  NODE_T *node;
  jmp_buf *jump_stat;
  BOOL_T proc_frame;
  int frame_no, frame_level, parameter_level;
#if defined (BUILD_PARALLEL_CLAUSE)
  pthread_t thread_id;
#endif
};


struct PROP_T
{
  PROP_PROC *unit;
  NODE_T *source;
};

struct A68_STREAM
{
  char *name;
  FILE_T fd;
  BOOL_T opened, writemood;
} ALIGNED;

struct DIAGNOSTIC_T
{
  int attribute, number;
  NODE_T *where;
  LINE_T *line;
  char *text, *symbol;
  DIAGNOSTIC_T *next;
};


struct FILES_T
{
  char *path, *initial_name, *generic_name;
  struct A68_STREAM binary, diags, library, script, object, source, listing, pretty;
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
    *C_STRING, *ERROR, *FILE, *FORMAT, *HEX_NUMBER, *HIP, *INT, *LONG_BITS, *LONG_BYTES,
    *LONG_COMPL, *LONG_COMPLEX, *LONG_INT, *LONG_LONG_BITS, *LONG_LONG_COMPL,
    *LONG_LONG_COMPLEX, *LONG_LONG_INT, *LONG_LONG_REAL, *LONG_REAL, *NUMBER, *PIPE,
    *PROC_REAL_REAL, *PROC_LONG_REAL_LONG_REAL, *PROC_REF_FILE_BOOL, *PROC_REF_FILE_VOID, *PROC_ROW_CHAR,
    *PROC_STRING, *PROC_VOID, *REAL, *REF_BITS, *REF_BOOL, *REF_BYTES,
    *REF_CHAR, *REF_COMPL, *REF_COMPLEX, *REF_FILE, *REF_FORMAT, *REF_INT,
    *REF_LONG_BITS, *REF_LONG_BYTES, *REF_LONG_COMPL, *REF_LONG_COMPLEX,
    *REF_LONG_INT, *REF_LONG_LONG_BITS, *REF_LONG_LONG_COMPL,
    *REF_LONG_LONG_COMPLEX, *REF_LONG_LONG_INT, *REF_LONG_LONG_REAL,
    *REF_LONG_REAL, *REF_PIPE, *REF_REAL, *REF_REF_FILE, *REF_ROW_CHAR, *REF_ROW_COMPLEX, *REF_ROW_INT, *REF_ROW_REAL, *REF_ROW_ROW_COMPLEX, *REF_ROW_ROW_REAL, *REF_SOUND, *REF_STRING, *ROW_BITS, *ROW_BOOL, *ROW_CHAR, *ROW_COMPLEX, *ROW_INT, *ROW_LONG_BITS, *ROW_LONG_LONG_BITS, *ROW_REAL, *ROW_ROW_CHAR, *ROW_ROW_COMPLEX, *ROW_ROW_REAL, *ROWS, *ROW_SIMPLIN, *ROW_SIMPLOUT, *ROW_STRING, *SEMA, *SIMPLIN, *SIMPLOUT, *SOUND, *SOUND_DATA, *STRING, *FLEX_ROW_CHAR, *FLEX_ROW_BOOL, *UNDEFINED, *VACUUM,
    *VOID;
};

struct OPTIONS_T
{
  OPTION_LIST_T *list;
  BOOL_T backtrace, brackets, check_only, clock, cross_reference, debug, compile, compile_check, keep, fold, license, moid_listing, object_listing, portcheck, pragmat_sema, pretty, reductions, regression_test, run, rerun, run_script, source_listing, standard_prelude_listing, statistics_listing, strict, stropping, trace, tree_listing, unused, verbose, version, no_warnings, quiet;
  int time_limit, opt_level, indent;
  STATUS_MASK_T nodemask;
};

struct MOID_T
{
  int attribute, dim, number, short_id, size, digits, sizec, digitsc;
  BOOL_T has_rows, use, portable, derivate;
  NODE_T *node;
  PACK_T *pack;
  MOID_T *sub, *equivalent_mode, *slice, *deflexed_mode, *name, *multiple_mode, *next, *rowed, *trim;
};
#define NO_MOID ((MOID_T *) NULL)

struct NODE_T
{
  GINFO_T *genie;
  int number, attribute, annotation;
  MOID_T *type;
  NODE_INFO_T *info;
  NODE_T *next, *previous, *sub, *sequence, *nest;
  PACK_T *pack;
  STATUS_MASK_T status, codex;
  TABLE_T *symbol_table, *non_local;
  TAG_T *tag;
};
#define NO_NODE ((NODE_T *) NULL)

struct NODE_INFO_T
{
  int procedure_level, priority, pragment_type;
  char *char_in_line, *symbol, *pragment, *expr;
  LINE_T *line;
};

struct GINFO_T
{
  PROP_T propagator;
  BOOL_T is_coercion, is_new_lexical_level, need_dns;
  BYTE_T *offset;
  MOID_T *partial_proc, *partial_locale;
  NODE_T *parent;
  char *compile_name;
  int level, argsize, size, compile_node;
  void *constant;
};

struct OPTION_LIST_T
{
  char *str;
  int scan;
  BOOL_T processed;
  LINE_T *line;
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

struct REFINEMENT_T
{
  REFINEMENT_T *next;
  char *name;
  LINE_T *line_defined, *line_applied;
  int applications;
  NODE_T *node_defined, *begin, *end;
};

struct SOID_T
{
  int attribute, sort, cast;
  MOID_T *type;
  NODE_T *node;
  SOID_T *next;
};

struct LINE_T
{
  char marker[6], *string, *filename;
  DIAGNOSTIC_T *diagnostics;
  int number, print_status;
  BOOL_T list;
  LINE_T *next, *previous;
};
#define NO_LINE ((LINE_T *) NULL)

struct TABLE_T
{
  int num, level, nest, attribute;
  BOOL_T initialise_frame, initialise_anon, proc_ops;
  ADDR_T ap_increment;
  TABLE_T *previous, *outer;
  TAG_T *identifiers, *operators, *priority, *indicants, *labels, *anonymous;
  NODE_T *jump_to, *sequence;
};
#define NO_TABLE ((TABLE_T *) NULL)

struct TAG_T
{
  STATUS_MASK_T status, codex;
  TABLE_T *symbol_table;
  MOID_T *type;
  NODE_T *node, *unit;
  char *value;
  GPROC *procedure;
  BOOL_T scope_assigned, use, in_proc, a68_standenv_proc, loc_assigned, portable;
  int priority, heap, scope, size, youngest_environ, number;
  ADDR_T offset;
  TAG_T *next, *body;
};
#define NO_TAG ((TAG_T *) NULL)

struct TOKEN_T
{
  char *text;
  TOKEN_T *less, *more;
};

//! @struct A68_HANDLE
//! @brief Handle for REF into the HEAP.
//! @details
//! A REF into the HEAP points at a HANDLE.
//! The HANDLE points at the actual object in the HEAP.
//! Garbage collection modifies HANDLEs, but not REFs.

struct A68_HANDLE
{
  STATUS_MASK_T status;
  BYTE_T *pointer;
  int size;
  MOID_T *type;
  A68_HANDLE *next, *previous;
} ALIGNED;

//! @struct A68_REF
//! @brief Fat A68 pointer.

struct A68_REF
{
  STATUS_MASK_T status;
  ADDR_T offset;
  ADDR_T scope; // Dynamic scope.
  A68_HANDLE *handle;
} ALIGNED;

//! @struct A68_ARRAY
//! @brief A68 array descriptor. 
//! @details
//! A row is an A68_REF to an A68_ARRAY.
//! 
//! An A68_ARRAY is followed by one A68_TUPLE per dimension.
//! 
//! @verbatim
//! A68_REF row -> A68_ARRAY ----+   ARRAY: Description of row, ref to elements
//!                A68_TUPLE 1   |   TUPLE: Bounds, one for every dimension
//!                ...           |
//!                A68_TUPLE dim |
//!                ...           |
//!                ...           |
//!                Element 1 <---+   Element: Sequential row elements, in the heap
//!                ...                        Not always contiguous - trims!
//! @endverbatim

struct A68_ARRAY
{
  MOID_T *type;
  int dim, elem_size;
  ADDR_T slice_offset, field_offset;
  A68_REF array;
} ALIGNED;

struct A68_BITS
{
  STATUS_MASK_T status;
  UNSIGNED_T value;
} ALIGNED;

struct A68_BYTES
{
  STATUS_MASK_T status;
  char value[BYTES_WIDTH + 1];
} ALIGNED;

struct A68_CHANNEL
{
  STATUS_MASK_T status;
  BOOL_T reset, set, get, put, bin, draw, compress;
} ALIGNED;

struct A68_BOOL
{
  STATUS_MASK_T status;
  BOOL_T value;
} ALIGNED;

struct A68_CHAR
{
  STATUS_MASK_T status;
  CHAR_T value;
} ALIGNED;

struct A68_COLLITEM
{
  STATUS_MASK_T status;
  int count;
};

struct A68_INT
{
  STATUS_MASK_T status;
  INT_T value;
} ALIGNED;

//! @struct A68_FORMAT
//! @brief A68 format descriptor.
//! @details
//! A format behaves very much like a procedure.

struct A68_FORMAT
{
  STATUS_MASK_T status;
  NODE_T *body;      // Entry point in syntax tree.
  ADDR_T fp_environ; // Frame pointer to environ.
} ALIGNED;

struct A68_LONG_BYTES
{
  STATUS_MASK_T status;
  char value[LONG_BYTES_WIDTH + 1];
} ALIGNED;

//! @struct A68_PROCEDURE
//! @brief A68 procedure descriptor.

struct A68_PROCEDURE
{
  STATUS_MASK_T status;
  union
  {
    NODE_T *node;
    GPROC *procedure;
  } body;             // Entry point in syntax tree or precompiled C procedure.
  A68_HANDLE *locale; // Locale for partial parametrisation.
  MOID_T *type;
  ADDR_T fp_environ;  // Frame pointer to environ.
} ALIGNED;

typedef A68_REAL A68_COMPLEX[2];

//! @struct A68_TUPLE
//! @brief A tuple containing bounds etcetera for one dimension.

struct A68_TUPLE
{
  INT_T upper_bound, lower_bound, shift, span, k;
} ALIGNED;

struct A68_UNION
{
  STATUS_MASK_T status;
  void *value;
} ALIGNED;

struct A68_SOUND
{
  STATUS_MASK_T status;
  unt num_channels, sample_rate, bits_per_sample, num_samples, data_size;
  A68_REF data;
};

struct A68_FILE
{
  STATUS_MASK_T status;
  A68_CHANNEL channel;
  A68_FORMAT format;
  A68_PROCEDURE file_end_mended, page_end_mended, line_end_mended, value_error_mended, open_error_mended, transput_error_mended, format_end_mended, format_error_mended;
  A68_REF identification, terminator, string;
  ADDR_T frame_pointer, stack_pointer;  // Since formats open frames
  BOOL_T read_mood, write_mood, char_mood, draw_mood, opened, open_exclusive, end_of_file, tmp_file;
  FILE_T fd;
  int transput_buffer, strpos, file_entry;
  struct
  {
    FILE *stream;
#if defined (HAVE_GNU_PLOTUTILS)
    plPlotter *plotter;
    plPlotterParams *plotter_params;
#endif
    BOOL_T device_made, device_opened;
    A68_REF device, page_size;
    int device_handle /* deprecated */ , window_x_size, window_y_size;
    REAL_T x_coord, y_coord, red, green, blue;
  }
  device;
#if defined (HAVE_POSTGRESQL)
# if ! defined (A68_OPTIMISE)
  PGconn *connection;
  PGresult *result;
# endif
#endif
};

#define M_BITS (MODE (BITS))
#define M_BOOL (MODE (BOOL))
#define M_BYTES (MODE (BYTES))
#define M_CHANNEL (MODE (CHANNEL))
#define M_CHAR (MODE (CHAR))
#define M_COLLITEM (MODE (COLLITEM))
#define M_COMPL (MODE (COMPL))
#define M_COMPLEX (MODE (COMPLEX))
#define M_C_STRING (MODE (C_STRING))
#define M_ERROR (MODE (ERROR))
#define M_FILE (MODE (FILE))
#define M_FLEX_ROW_BOOL (MODE (FLEX_ROW_BOOL))
#define M_FLEX_ROW_CHAR (MODE (FLEX_ROW_CHAR))
#define M_HEX_NUMBER (MODE (HEX_NUMBER))
#define M_HIP (MODE (HIP))
#define M_INT (MODE (INT))
#define M_LONG_BITS (MODE (LONG_BITS))
#define M_LONG_BYTES (MODE (LONG_BYTES))
#define M_LONG_COMPL (MODE (LONG_COMPL))
#define M_LONG_COMPLEX (MODE (LONG_COMPLEX))
#define M_LONG_INT (MODE (LONG_INT))
#define M_LONG_LONG_BITS (MODE (LONG_LONG_BITS))
#define M_LONG_LONG_COMPL (MODE (LONG_LONG_COMPL))
#define M_LONG_LONG_COMPLEX (MODE (LONG_LONG_COMPLEX))
#define M_LONG_LONG_INT (MODE (LONG_LONG_INT))
#define M_LONG_LONG_REAL (MODE (LONG_LONG_REAL))
#define M_LONG_REAL (MODE (LONG_REAL))
#define M_NIL (MODE (NIL))
#define M_NUMBER (MODE (NUMBER))
#define M_PIPE (MODE (PIPE))
#define M_PROC_REAL_REAL (MODE (PROC_REAL_REAL))
#define M_PROC_LONG_REAL_LONG_REAL (MODE (PROC_LONG_REAL_LONG_REAL))
#define M_PROC_REF_FILE_BOOL (MODE (PROC_REF_FILE_BOOL))
#define M_PROC_REF_FILE_VOID (MODE (PROC_REF_FILE_VOID))
#define M_PROC_ROW_CHAR (MODE (PROC_ROW_CHAR))
#define M_PROC_STRING (MODE (PROC_STRING))
#define M_PROC_VOID (MODE (PROC_VOID))
#define M_REAL (MODE (REAL))
#define M_REF_BITS (MODE (REF_BITS))
#define M_REF_BOOL (MODE (REF_BOOL))
#define M_REF_BYTES (MODE (REF_BYTES))
#define M_REF_CHAR (MODE (REF_CHAR))
#define M_REF_COMPL (MODE (REF_COMPL))
#define M_REF_COMPLEX (MODE (REF_COMPLEX))
#define M_REF_FILE (MODE (REF_FILE))
#define M_REF_FORMAT (MODE (REF_FORMAT))
#define M_REF_INT (MODE (REF_INT))
#define M_REF_LONG_BITS (MODE (REF_LONG_BITS))
#define M_REF_LONG_BYTES (MODE (REF_LONG_BYTES))
#define M_REF_LONG_COMPL (MODE (REF_LONG_COMPL))
#define M_REF_LONG_COMPLEX (MODE (REF_LONG_COMPLEX))
#define M_REF_LONG_INT (MODE (REF_LONG_INT))
#define M_REF_LONG_LONG_COMPL (MODE (REF_LONG_LONG_COMPL))
#define M_REF_LONG_LONG_COMPLEX (MODE (REF_LONG_LONG_COMPLEX))
#define M_REF_LONG_LONG_INT (MODE (REF_LONG_LONG_INT))
#define M_REF_LONG_LONG_REAL (MODE (REF_LONG_LONG_REAL))
#define M_REF_LONG_REAL (MODE (REF_LONG_REAL))
#define M_REF_PIPE (MODE (REF_PIPE))
#define M_REF_REAL (MODE (REF_REAL))
#define M_REF_REF_FILE (MODE (REF_REF_FILE))
#define M_REF_ROW_CHAR (MODE (REF_ROW_CHAR))
#define M_REF_ROW_COMPLEX (MODE (REF_ROW_COMPLEX))
#define M_REF_ROW_INT (MODE (REF_ROW_INT))
#define M_REF_ROW_REAL (MODE (REF_ROW_REAL))
#define M_REF_ROW_ROW_COMPLEX (MODE (REF_ROW_ROW_COMPLEX))
#define M_REF_ROW_ROW_REAL (MODE (REF_ROW_ROW_REAL))
#define M_REF_SOUND (MODE (REF_SOUND))
#define M_REF_STRING (MODE (REF_STRING))
#define M_ROW_BITS (MODE (ROW_BITS))
#define M_ROW_BOOL (MODE (ROW_BOOL))
#define M_ROW_CHAR (MODE (ROW_CHAR))
#define M_ROW_COMPLEX (MODE (ROW_COMPLEX))
#define M_ROW_INT (MODE (ROW_INT))
#define M_ROW_LONG_BITS (MODE (ROW_LONG_BITS))
#define M_ROW_LONG_LONG_BITS (MODE (ROW_LONG_LONG_BITS))
#define M_ROW_REAL (MODE (ROW_REAL))
#define M_ROW_ROW_CHAR (MODE (ROW_ROW_CHAR))
#define M_ROW_ROW_COMPLEX (MODE (ROW_ROW_COMPLEX))
#define M_ROW_ROW_REAL (MODE (ROW_ROW_REAL))
#define M_ROW_SIMPLIN (MODE (ROW_SIMPLIN))
#define M_ROW_SIMPLOUT (MODE (ROW_SIMPLOUT))
#define M_ROW_STRING (MODE (ROW_STRING))
#define M_SEMA (MODE (SEMA))
#define M_SIMPLIN (MODE (SIMPLIN))
#define M_SIMPLOUT (MODE (SIMPLOUT))
#define M_SOUND_DATA (MODE (SOUND_DATA))
#define M_STRING (MODE (STRING))
#define M_UNDEFINED (MODE (UNDEFINED))
#define M_VACUUM (MODE (VACUUM))
#define M_VOID (MODE (VOID))
#define M_FORMAT (MODE (FORMAT))
#define M_ROWS (MODE (ROWS))
#define M_SOUND (MODE (SOUND))

#endif
