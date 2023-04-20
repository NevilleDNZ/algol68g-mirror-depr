//! @file a68g-defines.h
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

#if !defined (__A68G_DEFINES_H__)
#define __A68G_DEFINES_H__

// Constants

#define KILOBYTE ((unt) 1024)
#define MEGABYTE (KILOBYTE * KILOBYTE)
#define GIGABYTE (KILOBYTE * MEGABYTE)

#define A68_TRUE ((BOOL_T) 1)
#define A68_FALSE ((BOOL_T) 0)

#define BACKSLASH_CHAR '\\'
#define BLANK_CHAR ' '
#define CR_CHAR '\r'
#define EOF_CHAR (EOF)
#define ERROR_CHAR '*'
#define ESCAPE_CHAR '\033'
#define EXPONENT_CHAR 'e'
#define FLIP_CHAR 'T'
#define FLOP_CHAR 'F'
#define FORMFEED_CHAR '\f'
#define NEWLINE_CHAR '\n'
#define NULL_CHAR '\0'
#define POINT_CHAR '.'
#define QUOTE_CHAR '"'
#define RADIX_CHAR 'r'
#define TAB_CHAR '\t'

// File extensions
#define BINARY_EXTENSION ".o"
#define PLUGIN_EXTENSION ".so"
#define LISTING_EXTENSION ".l"
#define OBJECT_EXTENSION ".c"
#define PRETTY_EXTENSION ".f"
#define SCRIPT_EXTENSION ""

// Static options for GCC.
// 
// -fno-stack-protector is needed for Ubuntu etcetera that enforce -fstack-protector-strong 
// which may give an undefined reference to `__stack_chk_fail_local'.
// 
// -Wno-parentheses-equality is needed for OpenBSD.  
//

#define A68_GCC_OPTIONS "-DA68_OPTIMISE -ggdb -fno-stack-protector -Wno-parentheses-equality"

// Formats
#define DIGIT_BLANK ((unt) 0x2)
#define DIGIT_NORMAL ((unt) 0x1)
#define INSERTION_BLANK ((unt) 0x20)
#define INSERTION_NORMAL ((unt) 0x10)

#define MAX_RESTART 256

#define A68_DIR ".a68g"
#define A68_HISTORY_FILE ".a68g.edit.hist"
#define A68_NO_FILENO ((FILE_T) -1)
#define A68_PROTECTION (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)  // -rw-r--r--
#define A68_READ_ACCESS (O_RDONLY)
#define A68_WRITE_ACCESS (O_WRONLY | O_CREAT | O_TRUNC)
#define BUFFER_SIZE (KILOBYTE)
#define DEFAULT_WIDTH (-1)

#define EMBEDDED_FORMAT A68_TRUE
#define EVEN(k) ((k) % 2 == 0)
#define HIDDEN_TEMP_FILE_NAME ".a68g.tmp"
#define ITEM_NOT_USED (-1)
#define MAX_ERRORS 5
#define MAX_PRIORITY 9
#define MAX_TERM_HEIGTH 24
#define MAX_TERM_WIDTH (BUFFER_SIZE / 2)
#define MIN_MEM_SIZE (128 * KILOBYTE)
#define MOID_ERROR_WIDTH 80
#define MOID_WIDTH 80
#define MONADS "%^&+-~!?"
#define NEWLINE_STRING "\n"
#define NOMADS "></=*"
#define NOT_EMBEDDED_FORMAT A68_FALSE
#define NOT_PRINTED 1
#define OVER_2G(n) ((REAL_T) (n) > (REAL_T) (2 * GIGABYTE)) 
#define PRIMAL_SCOPE 0
#define SKIP_PATTERN A68_FALSE
#define SMALL_BUFFER_SIZE 128
#define SNPRINTF_SIZE ((size_t) (BUFFER_SIZE - 1))
#define TRANSPUT_BUFFER_SIZE BUFFER_SIZE
#define WANT_PATTERN A68_TRUE

#define MANT_DIGS(n) ((int) round ((n) * log10 (2.0)))
#define MANT_BITS(n) ((int) round ((n) / log10 (2.0)))
#define REAL_DIGITS MANT_DIGS (REAL_MANT_DIG)

// Macros

#define _SKIP_ { (void) 0;}

#define BUFCLR(z) {memset ((z), 0, BUFFER_SIZE + 1);}

#define ABS(n) ((n) >= 0 ? (n) : -(n))

#define MAX(u, v) (((u) > (v) ? (u) : (v)))
#define MAXIMISE(u, v) ((u) = MAX (u, v))

#define MIN(u, v) (((u) < (v) ? (u) : (v)))
#define MINIMISE(u, v) ((u) = MIN (u, v))

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

#if defined (HAVE_CURSES)
#define ASSERT(f) {\
  if (!(f)) {\
    if (A68 (curses_mode) == A68_TRUE) {\
      (void) attrset (A_NORMAL);\
      (void) endwin ();\
      A68 (curses_mode) = A68_FALSE;\
    }\
    ABEND (A68_TRUE, ERROR_ASSERTION, __func__)\
  }}
#else
#define ASSERT(f) {\
  ABEND((!(f)), ERROR_ASSERTION, __func__)\
  }
#endif

// Some macros to overcome the ambiguity in having signed or unt char 
// on various systems. PDP-11s and IBM 370s are still haunting us with this.

#define IS_ALNUM(c) isalnum ((unt char) (c))
#define IS_ALPHA(c) isalpha ((unt char) (c))
#define IS_CNTRL(c) iscntrl ((unt char) (c))
#define IS_DIGIT(c) isdigit ((unt char) (c))
#define IS_GRAPH(c) isgraph ((unt char) (c))
#define IS_LOWER(c) islower ((unt char) (c))
#define IS_PRINT(c) isprint ((unt char) (c))
#define IS_PUNCT(c) ispunct ((unt char) (c))
#define IS_SPACE(c) isspace ((unt char) (c))
#define IS_UPPER(c) isupper ((unt char) (c))
#define IS_XDIGIT(c) isxdigit ((unt char) (c))
#define TO_LOWER(c) (char) tolower ((unt char) (c))
#define TO_UCHAR(c) ((c) >= 0 ? (int) (c) : (int) (UCHAR_MAX + (int) (c) + 1))
#define TO_UPPER(c) (char) toupper ((unt char) (c))

// Macro's for fat A68 pointers

#define ADDRESS(z) (&(((IS_IN_HEAP (z) || IS_IN_COMMON (z)) ? REF_POINTER (z) : A68_STACK)[REF_OFFSET (z)]))
#define ARRAY_ADDRESS(z) (&(REF_POINTER (z)[REF_OFFSET (z)]))
#define DEREF(mode, expr) ((mode *) ADDRESS (expr))
#define FILE_DEREF(p) DEREF (A68_FILE, (p))
#define HEAP_ADDRESS(n) ((BYTE_T *) & (A68_HEAP[n]))
#define IS_IN_FRAME(z) (STATUS (z) & IN_FRAME_MASK)
#define IS_IN_HEAP(z) (STATUS (z) & IN_HEAP_MASK)
#define IS_IN_COMMON(z) (STATUS (z) & IN_COMMON_MASK)
#define IS_IN_STACK(z) (STATUS (z) & IN_STACK_MASK)
#define IS_NIL(p) ((BOOL_T) ((STATUS (&(p)) & NIL_MASK) != 0))
#define LOCAL_ADDRESS(z) (& A68_STACK[REF_OFFSET (z)])
#define REF_HANDLE(z) (HANDLE (z))
#define REF_OFFSET(z) (OFFSET (z))
#define REF_POINTER(z) (POINTER (REF_HANDLE (z)))
#define REF_SCOPE(z) (SCOPE (z))
#define STACK_ADDRESS(n) ((BYTE_T *) &(A68_STACK[(n)]))
#define STACK_OFFSET(n) (STACK_ADDRESS (A68_SP + (int) (n)))
#define STACK_TOP (STACK_ADDRESS (A68_SP))

// Miscellaneous macros

#define IN_PRELUDE(p) (LINE_NUMBER (p) <= 0)
#define EOL(c) ((c) == NEWLINE_CHAR || (c) == NULL_CHAR)

#define SIZE_ALIGNED(p) ((int) A68_ALIGN (sizeof (p)))
#define A68_REF_SIZE (SIZE_ALIGNED (A68_REF))
#define A68_UNION_SIZE (SIZE_ALIGNED (A68_UNION))

#define A68_SOUND_BYTES(s) ((int) (BITS_PER_SAMPLE (s)) / 8 + (int) (BITS_PER_SAMPLE (s) % 8 == 0 ? 0 : 1))
#define A68_SOUND_DATA_SIZE(s) ((int) (NUM_SAMPLES (s)) * (int) (NUM_CHANNELS (s)) * (int) (A68_SOUND_BYTES (s)))
#define BACKWARD(p) (p = PREVIOUS (p))
#define BITS_WIDTH ((int) (1 + ceil (log ((REAL_T) A68_MAX_INT) / log ((REAL_T) 2))))
#define DEFLEX(p) (DEFLEXED (p) != NO_MOID ? DEFLEXED(p) : (p))
#define FORWARD(p) ((p) = NEXT (p))
#define INT_WIDTH ((int) (1 + floor (log ((REAL_T) A68_MAX_INT) / log ((REAL_T) 10))))
#define LONG_INT_WIDTH (1 + LONG_WIDTH)
#define PM(m) (moid_to_string (m, 132, NO_NODE))
#define SIGN(n) ((n) == 0 ? 0 : ((n) > 0 ? 1 : -1))
#define STATUS_CLEAR(p, q) {STATUS (p) &= (~(q));}
#define STATUS_SET(p, q) {STATUS (p) |= (q);}
#define STATUS_TEST(p, q) ((STATUS (p) & (q)) != (unt) 0)
#define WIS(p) where_in_source (STDOUT_FILENO, (p))
#define WRITE(f, s) io_write_string ((f), (s));
#define WRITELN(f, s) {WRITE ((f), "\n"); WRITE ((f), (s));}

// Access macros

#define A(p) ((p)->a)
#define A68_STANDENV_PROC(p) ((p)->a68_standenv_proc)
#define ACTION(p) ((p)->action)
#define ACTIVE(p) ((p)->active)
#define ADDR(p) ((p)->addr)
#define ANNOTATION(p) ((p)->annotation)
#define ANONYMOUS(p) ((p)->anonymous)
#define APPLICATIONS(p) ((p)->applications)
#define AP_INCREMENT(p) ((p)->ap_increment)
#define ARGSIZE(p) ((p)->argsize)
#define ARRAY(p) ((p)->array)
#define ATTRIBUTE(p) ((p)->attribute)
#define B(p) ((p)->b)
#define BEGIN(p) ((p)->begin)
#define BIN(p) ((p)->bin)
#define BITS_PER_SAMPLE(p) ((p)->bits_per_sample)
#define BLUE(p) ((p)->blue)
#define BODY(p) ((p)->body)
#define BSTATE(p) ((p)->bstate)
#define BYTES(p) ((p)->bytes)
#define CAST(p) ((p)->cast)
#define CAT(p) ((p)->cat)
#define CHANNEL(p) ((p)->channel)
#define CHAR_IN_LINE(p) ((p)->char_in_line)
#define CHAR_MOOD(p) ((p)->char_mood)
#define CMD(p) ((p)->cmd)
#define CMD_ROW(p) ((p)->cmd_row)
#define CODE(p) ((p)->code)
#define CODEX(p) ((p)->codex)
#define COLLECT(p) ((p)->collect)
#define COMPILED(p) ((p)->compiled)
#define COMPILE_NAME(p) ((p)->compile_name)
#define COMPILE_NODE(p) ((p)->compile_node)
#define COMPRESS(p) ((p)->compress)
#define CONNECTION(p) ((p)->connection)
#define CONSTANT(p) ((p)->constant)
#define COUNT(p) ((p)->count)
#define CROSS_REFERENCE_SAFE(p) ((p)->cross_reference_safe)
#define CUR_PTR(p) ((p)->cur_ptr)
#define DATA(p) ((p)->data)
#define DATA_SIZE(p) ((p)->data_size)
#define DATE(p) ((p)->date)
#define DEF(p) ((p)->def)
#define DEFLEXED(p) ((p)->deflexed_mode)
#define DERIVATE(p) ((p)->derivate)
#define DEVICE(p) ((p)->device)
#define DEVICE_HANDLE(p) ((p)->device_handle)
#define DEVICE_MADE(p) ((p)->device_made)
#define DEVICE_OPENED(p) ((p)->device_opened)
#define DIAGNOSTICS(p) ((p)->diagnostics)
#define DIGITS(p) ((p)->digits)
#define DIGITSC(p) ((p)->digitsc)
#define DIM(p) ((p)->dim)
#define DISPLAY(p) ((p)->display)
#define DRAW(p) ((p)->draw)
#define DRAW_MOOD(p) ((p)->draw_mood)
#define DUMP(p) ((p)->dump)
#define DYNAMIC_LINK(p) ((p)->dynamic_link)
#define DYNAMIC_SCOPE(p) ((p)->dynamic_scope)
#define D_NAME(p) ((p)->d_name)
#define ELEM_SIZE(p) ((p)->elem_size)
#define END(p) ((p)->end)
#define END_OF_FILE(p) ((p)->end_of_file)
#define ENVIRON(p) ((p)->fp_environ)
#define EQUIVALENT(p) ((p)->equivalent_mode)
#define EQUIVALENT_MODE(p) ((p)->equivalent_mode)
#define ERROR_COUNT(p) ((p)->error_count)
#define RENDEZ_VOUS(p) ((p)->rendez_vous)
#define EXPR(p) ((p)->expr)
#define F(p) ((p)->f)
#define FACTOR(p) ((p)->factor)
#define FD(p) ((p)->fd)
#define FIELD_OFFSET(p) ((p)->field_offset)
#define FILENAME(p) ((p)->filename)
#define FILES(p) ((p)->files)
#define FILE_BINARY_NAME(p) (FILES (p).binary.name)
#define FILE_BINARY_OPENED(p) (FILES (p).binary.opened)
#define FILE_BINARY_WRITEMOOD(p) (FILES (p).binary.writemood)
#define FILE_DIAGS_FD(p) (FILES (p).diags.fd)
#define FILE_DIAGS_NAME(p) (FILES (p).diags.name)
#define FILE_DIAGS_OPENED(p) (FILES (p).diags.opened)
#define FILE_DIAGS_WRITEMOOD(p) (FILES (p).diags.writemood)
#define FILE_END_MENDED(p) ((p)->file_end_mended)
#define FILE_ENTRY(p) ((p)->file_entry)
#define FILE_GENERIC_NAME(p) (FILES (p).generic_name)
#define FILE_INITIAL_NAME(p) (FILES (p).initial_name)
#define FILE_PLUGIN_NAME(p) (FILES (p).plugin.name)
#define FILE_PLUGIN_OPENED(p) (FILES (p).plugin.opened)
#define FILE_PLUGIN_WRITEMOOD(p) (FILES (p).plugin.writemood)
#define FILE_LISTING_FD(p) (FILES (p).listing.fd)
#define FILE_LISTING_NAME(p) (FILES (p).listing.name)
#define FILE_LISTING_OPENED(p) (FILES (p).listing.opened)
#define FILE_LISTING_WRITEMOOD(p) (FILES (p).listing.writemood)
#define FILE_OBJECT_FD(p) (FILES (p).object.fd)
#define FILE_OBJECT_NAME(p) (FILES (p).object.name)
#define FILE_OBJECT_OPENED(p) (FILES (p).object.opened)
#define FILE_OBJECT_WRITEMOOD(p) (FILES (p).object.writemood)
#define FILE_PATH(p) (FILES (p).path)
#define FILE_PRETTY_FD(p) (FILES (p).pretty.fd)
#define FILE_PRETTY_NAME(p) (FILES (p).pretty.name)
#define FILE_PRETTY_OPENED(p) (FILES (p).pretty.opened)
#define FILE_PRETTY_WRITEMOOD(p) (FILES (p).pretty.writemood)
#define FILE_SCRIPT_NAME(p) (FILES (p).script.name)
#define FILE_SCRIPT_OPENED(p) (FILES (p).script.opened)
#define FILE_SCRIPT_WRITEMOOD(p) (FILES (p).script.writemood)
#define FILE_SOURCE_FD(p) (FILES (p).source.fd)
#define FILE_SOURCE_NAME(p) (FILES (p).source.name)
#define FILE_SOURCE_OPENED(p) (FILES (p).source.opened)
#define FILE_SOURCE_WRITEMOOD(p) (FILES (p).source.writemood)
#define FIND(p) ((p)->find)
#define FORMAT(p) ((p)->format)
#define FORMAT_END_MENDED(p) ((p)->format_end_mended)
#define FORMAT_ERROR_MENDED(p) ((p)->format_error_mended)
#define FRAME(p) ((p)->frame)
#define FRAME_LEVEL(p) ((p)->frame_level)
#define FRAME_NO(p) ((p)->frame_no)
#define FRAME_POINTER(p) ((p)->frame_pointer)
#define FUNCTION(p) ((p)->function)
#define G(p) ((p)->g)
#define GINFO(p) ((p)->genie)
#define GET(p) ((p)->get)
#define GLOBAL_PROP(p) ((p)->global_prop)
#define GPARENT(p) (PARENT (GINFO (p)))
#define GREEN(p) ((p)->green)
#define H(p) ((p)->h)
#define HANDLE(p) ((p)->handle)
#define HAS_ROWS(p) ((p)->has_rows)
#define HEAP(p) ((p)->heap)
#define HEAP_POINTER(p) ((p)->heap_pointer)
#define H_ADDR(p) ((p)->h_addr)
#define H_LENGTH(p) ((p)->h_length)
#define ID(p) ((p)->id)
#define IDENTIFICATION(p) ((p)->identification)
#define IDENTIFIERS(p) ((p)->identifiers)
#define IDF(p) ((p)->idf)
#define IM(z) (VALUE (&(z)[1]))
#define IN(p) ((p)->in)
#define INDEX(p) ((p)->index)
#define INDICANTS(p) ((p)->indicants)
#define INFO(p) ((p)->info)
#define INITIALISE_ANON(p) ((p)->initialise_anon)
#define INITIALISE_FRAME(p) ((p)->initialise_frame)
#define INI_PTR(p) ((p)->ini_ptr)
#define INS_MODE(p) ((p)->ins_mode)
#define IN_CMD(p) ((p)->in_cmd)
#define IN_FORBIDDEN(p) ((p)->in_forbidden)
#define IN_PREFIX(p) ((p)->in_prefix)
#define IN_PROC(p) ((p)->in_proc)
#define IN_TEXT(p) ((p)->in_text)
#define IS_COMPILED(p) ((p)->is_compiled)
#define IS_OPEN(p) ((p)->is_open)
#define IS_TMP(p) ((p)->is_tmp)
#define JUMP_STAT(p) ((p)->jump_stat)
#define JUMP_TO(p) ((p)->jump_to)
#define K(q) ((q)->k)
#define LABELS(p) ((p)->labels)
#define LAST(p) ((p)->last)
#define LAST_LINE(p) ((p)->last_line)
#define LESS(p) ((p)->less)
#define LEVEL(p) ((p)->level)
#define LEX_LEVEL(p) (LEVEL (TABLE (p)))
#define LINBUF(p) ((p)->linbuf)
#define LINE(p) ((p)->line)
#define LINE_APPLIED(p) ((p)->line_applied)
#define LINE_DEFINED(p) ((p)->line_defined)
#define LINE_END_MENDED(p) ((p)->line_end_mended)
#define LINE_NUMBER(p) (NUMBER (LINE (INFO (p))))
#define LINSIZ(p) ((p)->linsiz)
#define LIST(p) ((p)->list)
#define ln(x) (log (x))
#define LOCALE(p) ((p)->locale)
#define LOC_ASSIGNED(p) ((p)->loc_assigned)
#define LOWER_BOUND(p) ((p)->lower_bound)
#define LWB(p) ((p)->lower_bound)
#define MARKER(p) ((p)->marker)
#define MATCH(p) ((p)->match)
#define MODIFIED(p) ((p)->modified)
#define MOID(p) ((p)->type)
#define MORE(p) ((p)->more)
#define MSGS(p) ((p)->msgs)
#define MULTIPLE(p) ((p)->multiple_mode)
#define MULTIPLE_MODE(p) ((p)->multiple_mode)
#define M_EO(p) ((p)->m_eo)
#define M_MATCH(p) ((p)->match)
#define M_SO(p) ((p)->m_so)
#define NAME(p) ((p)->name)
#define NEED_DNS(p) ((p)->need_dns)
#define NEGATE(p) ((p)->negate)
#define NEST(p) ((p)->nest)
#define NEW_FILE(p) ((p)->new_file)
#define NEXT(p) ((p)->next)
#define NEXT_NEXT(p) (NEXT (NEXT (p)))
#define NEXT_NEXT_NEXT(p) (NEXT (NEXT_NEXT (p)))
#define NEXT_SUB(p) (NEXT (SUB (p)))
#define NF(p) ((p)->nf)
#define NODE(p) ((p)->node)
#define NODE_DEFINED(p) ((p)->node_defined)
#define NODE_PACK(p) ((p)->pack)
#define NON_LOCAL(p) ((p)->non_local)
#define NCHAR_IN_LINE(p) (CHAR_IN_LINE (INFO (p)))
#define NPRAGMENT(p) (PRAGMENT (INFO (p)))
#define NPRAGMENT_TYPE(p) (PRAGMENT_TYPE (INFO (p)))
#define NSYMBOL(p) (SYMBOL (INFO (p)))
#define NUM(p) ((p)->num)
#define NUMBER(p) ((p)->number)
#define NUM_CHANNELS(p) ((p)->num_channels)
#define NUM_MATCH(p) ((p)->num_match)
#define NUM_SAMPLES(p) ((p)->num_samples)
#define OFFSET(p) ((p)->offset)
#define OPENED(p) ((p)->opened)
#define OPEN_ERROR_MENDED(p) ((p)->open_error_mended)
#define OPEN_EXCLUSIVE(p) ((p)->open_exclusive)
#define OPER(p) ((p)->oper)
#define OPERATORS(p) ((p)->operators)
#define OPTIONS(p) ((p)->options)
#define OPTION_BACKTRACE(p) (OPTIONS (p).backtrace)
#define OPTION_BRACKETS(p) (OPTIONS (p).brackets)
#define OPTION_CHECK_ONLY(p) (OPTIONS (p).check_only)
#define OPTION_CLOCK(p) (OPTIONS (p).clock)
#define OPTION_COMPILE(p) (OPTIONS (p).compile)
#define OPTION_COMPILE_CHECK(p) (OPTIONS (p).compile_check)
#define OPTION_CROSS_REFERENCE(p) (OPTIONS (p).cross_reference)
#define OPTION_DEBUG(p) (OPTIONS (p).debug)
#define OPTION_FOLD(p) (OPTIONS (p).fold)
#define OPTION_INDENT(p) (OPTIONS (p).indent)
#define OPTION_KEEP(p) (OPTIONS (p).keep)
#define OPTION_LICENSE(p) (OPTIONS (p).license)
#define OPTION_LIST(p) (OPTIONS (p).list)
#define OPTION_LOCAL(p) (OPTIONS (p).local)
#define OPTION_MOID_LISTING(p) (OPTIONS (p).moid_listing)
#define OPTION_NODEMASK(p) (OPTIONS (p).nodemask)
#define OPTION_NO_WARNINGS(p) (OPTIONS (p).no_warnings)
#define OPTION_OBJECT_LISTING(p) (OPTIONS (p).object_listing)
#define OPTION_OPT_LEVEL(p) (OPTIONS (p).opt_level)
#define OPTION_PORTCHECK(p) (OPTIONS (p).portcheck)
#define OPTION_PRAGMAT_SEMA(p) (OPTIONS (p).pragmat_sema)
#define OPTION_PRETTY(p) (OPTIONS (p).pretty)
#define OPTION_QUIET(p) (OPTIONS (p).quiet)
#define OPTION_REDUCTIONS(p) (OPTIONS (p).reductions)
#define OPTION_REGRESSION_TEST(p) (OPTIONS (p).regression_test)
#define OPTION_RERUN(p) (OPTIONS (p).rerun)
#define OPTION_RUN(p) (OPTIONS (p).run)
#define OPTION_RUN_SCRIPT(p) (OPTIONS (p).run_script)
#define OPTION_SOURCE_LISTING(p) (OPTIONS (p).source_listing)
#define OPTION_STANDARD_PRELUDE_LISTING(p) (OPTIONS (p).standard_prelude_listing)
#define OPTION_STATISTICS_LISTING(p) (OPTIONS (p).statistics_listing)
#define OPTION_STRICT(p) (OPTIONS (p).strict)
#define OPTION_STROPPING(p) (OPTIONS (p).stropping)
#define OPTION_TIME_LIMIT(p) (OPTIONS (p).time_limit)
#define OPTION_TRACE(p) (OPTIONS (p).trace)
#define OPTION_TREE_LISTING(p) (OPTIONS (p).tree_listing)
#define OPTION_UNUSED(p) (OPTIONS (p).unused)
#define OPTION_VERBOSE(p) (OPTIONS (p).verbose)
#define OPTION_VERSION(p) (OPTIONS (p).version)
#define OUT(p) ((p)->out)
#define OUTER(p) ((p)->outer)
#define P(q) ((q)->p)
#define PACK(p) ((p)->pack)
#define PAGE_END_MENDED(p) ((p)->page_end_mended)
#define A68_PAGE_SIZE(p) ((p)->page_size)
#define PARAMETERS(p) ((p)->parameters)
#define PARAMETER_LEVEL(p) ((p)->parameter_level)
#define GSL_PARAMS(p) ((p)->params)
#define PARENT(p) ((p)->parent)
#define PARTIAL_LOCALE(p) ((p)->partial_locale)
#define PARTIAL_PROC(p) ((p)->partial_proc)
#define PATTERN(p) ((p)->pattern)
#define PERM(p) ((p)->perm)
#define PERMS(p) ((p)->perms)
#define IDF_ROW(p) ((p)->idf_row)
#define PHASE(p) ((p)->phase)
#define PLOTTER(p) ((p)->plotter)
#define PLOTTER_PARAMS(p) ((p)->plotter_params)
#define POINTER(p) ((p)->pointer)
#define PORTABLE(p) ((p)->portable)
#define POS(p) ((p)->pos)
#define PRAGMENT(p) ((p)->pragment)
#define PRAGMENT_TYPE(p) ((p)->pragment_type)
#define PRECMD(p) ((p)->precmd)
#define PREVIOUS(p) ((p)->previous)
#define PRINT_STATUS(p) ((p)->print_status)
#define PRIO(p) ((p)->priority)
#define PROCEDURE(p) ((p)->procedure)
#define PROCEDURE_LEVEL(p) ((p)->procedure_level)
#define PROCESSED(p) ((p)->processed)
#define PROC_FRAME(p) ((p)->proc_frame)
#define PROC_OPS(p) ((p)->proc_ops)
#define GPROP(p) (GINFO (p)->propagator)
#define PROP(p) ((p)->propagator)
#define PS(p) ((p)->ps)
#define PUT(p) ((p)->put)
#define P_PROTO(p) ((p)->p_proto)
#define R(p) ((p)->r)
#define RE(z) (VALUE (&(z)[0]))
#define READ_MOOD(p) ((p)->read_mood)
#define RED(p) ((p)->red)
#define REPL(p) ((p)->repl)
#define RESERVED(p) ((p)->reserved)
#define RESET(p) ((p)->reset)
#define RESULT(p) ((p)->result)
#define RE_NSUB(p) ((p)->re_nsub)
#define RLIM_CUR(p) ((p)->rlim_cur)
#define RLIM_MAX(p) ((p)->rlim_max)
#define RM_EO(p) ((p)->rm_eo)
#define RM_SO(p) ((p)->rm_so)
#define ROWED(p) ((p)->rowed)
#define S(p) ((p)->s)
#define SAMPLE_RATE(p) ((p)->sample_rate)
#define SCAN_STATE_C(p) ((p)->scan_state.save_c)
#define SCAN_STATE_L(p) ((p)->scan_state.save_l)
#define SCAN_STATE_S(p) ((p)->scan_state.save_s)
#define SCALE_ROW(p) ((p)->scale_row)
#define SCAN(p) ((p)->scan)
#define SCAN_ERROR(c, u, v, txt) if (c) {scan_error (u, v, txt);}
#define SCAN_WARNING(c, u, v, txt) if (c) {scan_warning (u, v, txt);}
#define SCOPE(p) ((p)->scope)
#define SCOPE_ASSIGNED(p) ((p)->scope_assigned)
#define SEARCH(p) ((p)->search)
#define SELECT(p) ((p)->select)
#define SEQUENCE(p) ((p)->sequence)
#define SET(p) ((p)->set)
#define SHIFT(p) ((p)->shift)
#define SHORT_ID(p) ((p)->short_id)
#define SIN_ADDR(p) ((p)->sin_addr)
#define SIN_FAMILY(p) ((p)->sin_family)
#define SIN_PORT(p) ((p)->sin_port)
#define SIZE(p) ((p)->size)
#define SIZE1(p) ((p)->size1)
#define SIZE2(p) ((p)->size2)
#define SIZEC(p) ((p)->sizec)
#define SLICE(p) ((p)->slice)
#define SLICE_OFFSET(p) ((p)->slice_offset)
#define SO(p) ((p)->so)
#define SORT(p) ((p)->sort)
#define SOURCE(p) ((p)->source)
#define SOURCE_SCAN(p) ((p)->source_scan)
#define SPAN(p) ((p)->span)
#define STACK(p) ((p)->stack)
#define STACK_POINTER(p) ((p)->stack_pointer)
#define STACK_USED(p) ((p)->stack_used)
#define STANDENV_MOID(p) ((p)->standenv_moid)
#define START(p) ((p)->start)
#define STATIC_LINK(p) ((p)->static_link)
#define STATUS(p) ((p)->status)
#define STATUS_IM(z) (STATUS (&(z)[1]))
#define STATUS_RE(z) (STATUS (&(z)[0]))
#define STR(p) ((p)->str)
#define STREAM(p) ((p)->stream)
#define STRING(p) ((p)->string)
#define STRPOS(p) ((p)->strpos)
#define ST_MODE(p) ((p)->st_mode)
#define ST_MTIME(p) ((p)->st_mtime)
#define SUB(p) ((p)->sub)
#define SUBSET(p) ((p)->subset)
#define SUB_MOID(p) (SUB (MOID (p)))
#define SUB_NEXT(p) (SUB (NEXT (p)))
#define SUB_SUB(p) (SUB (SUB (p)))
#define SWAP(p) ((p)->swap)
#define SYMBOL(p) ((p)->symbol)
#define SYNC(p) ((p)->sync)
#define SYNC_INDEX(p) ((p)->sync_index)
#define SYNC_LINE(p) ((p)->sync_line)
#define S_PORT(p) ((p)->s_port)
#define TABLE(p) ((p)->symbol_table)
#define TABS(p) ((p)->tabs)
#define TAG_LEX_LEVEL(p) (LEVEL (TAG_TABLE (p)))
#define TAG_TABLE(p) ((p)->symbol_table)
#define TAX(p) ((p)->tag)
#define TERM(p) ((p)->term)
#define TERMINATOR(p) ((p)->terminator)
#define TEXT(p) ((p)->text)
#define THREAD_ID(p) ((p)->thread_id)
#define THREAD_STACK_OFFSET(p) ((p)->thread_stack_offset)
#define TMP_FILE(p) ((p)->tmp_file)
#define TMP_TEXT(p) ((p)->tmp_text)
#define TM_HOUR(p) ((p)->tm_hour)
#define TM_ISDST(p) ((p)->tm_isdst)
#define TM_MDAY(p) ((p)->tm_mday)
#define TM_MIN(p) ((p)->tm_min)
#define TM_MON(p) ((p)->tm_mon)
#define TM_SEC(p) ((p)->tm_sec)
#define TM_WDAY(p) ((p)->tm_wday)
#define TM_YEAR(p) ((p)->tm_year)
#define TOF(p) ((p)->tof)
#define TOP_LINE(p) ((p)->top_line)
#define TOP_MOID(p) ((p)->top_moid)
#define TOP_NODE(p) ((p)->top_node)
#define TOP_REFINEMENT(p) ((p)->top_refinement)
#define TRANS(p) ((p)->trans)
#define TRANSIENT(p) ((p)->transient)
#define TRANSPUT_BUFFER(p) ((p)->transput_buffer)
#define TRANSPUT_ERROR_MENDED(p) ((p)->transput_error_mended)
#define TREE_LISTING_SAFE(p) ((p)->tree_listing_safe)
#define TRIM(p) ((p)->trim)
#define TUPLE(p) ((p)->tuple)
#define TV_SEC(p) ((p)->tv_sec)
#define TV_USEC(p) ((p)->tv_usec)
#define UNDO(p) ((p)->undo)
#define UNDO_LINE(p) ((p)->undo_line)
#define UNION_OFFSET (SIZE_ALIGNED (A68_UNION))
#define UNIT(p) ((p)->unit)
#define UPB(p) ((p)->upper_bound)
#define UPPER_BOUND(p) ((p)->upper_bound)
#define USE(p) ((p)->use)
#define VAL(p) ((p)->val)
#define VALUE(p) ((p)->value)
#define VALUE_ERROR_MENDED(p) ((p)->value_error_mended)
#define WARNING_COUNT(p) ((p)->warning_count)
#define WHERE(p) ((p)->where)
#define IF_ROW(m) (IS_FLEX (m) || IS_ROW (m) || m == M_STRING)
#define IS_COERCION(p) ((p)->is_coercion)
#define IS_FLEX(m) IS ((m), FLEX_SYMBOL)
#define IS_LITERALLY(p, s) (strcmp (NSYMBOL (p), s) == 0)
#define IS_NEW_LEXICAL_LEVEL(p) ((p)->is_new_lexical_level)
#define ISNT(p, s) (! IS (p, s))
#define IS(p, s) (ATTRIBUTE (p) == (s))
#define IS_REF_FLEX(m) (IS (m, REF_SYMBOL) && IS (SUB (m), FLEX_SYMBOL))
#define IS_REF(m) IS ((m), REF_SYMBOL)
#define IS_ROW(m) IS ((m), ROW_SYMBOL)
#define IS_STRUCT(m) IS ((m), STRUCT_SYMBOL)
#define IS_UNION(m) IS ((m), UNION_SYMBOL)
#define WINDOW_X_SIZE(p) ((p)->window_x_size)
#define WINDOW_Y_SIZE(p) ((p)->window_y_size)
#define WRITE_MOOD(p) ((p)->write_mood)
#define X(p) ((p)->x)
#define X_COORD(p) ((p)->x_coord)
#define Y(p) ((p)->y)
#define YOUNGEST_ENVIRON(p) ((p)->youngest_environ)
#define Y_COORD(p) ((p)->y_coord)

#endif
