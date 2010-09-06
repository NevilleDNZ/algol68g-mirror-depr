/*!
\file support.c
\brief small utility routines
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

#include "config.h"
#include "diagnostics.h"
#include "algol68g.h"
#include "genie.h"

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
