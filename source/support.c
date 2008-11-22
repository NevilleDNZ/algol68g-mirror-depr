/*!
\file support.c
\brief small utility routines
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2008 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "algol68g.h"
#include "genie.h"

#if ! defined ENABLE_WIN32
#include <sys/resource.h>
#endif

ADDR_T fixed_heap_pointer, temp_heap_pointer;
POSTULATE_T *top_postulate, *old_postulate;
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
  ABNORMAL_END (z == NULL, ERROR_OUT_OF_CORE, NULL);
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
  char *z = (char *) get_heap_space (n);
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
  char *z = (char *) get_fixed_heap_space (n);
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
  ABNORMAL_END (get_fixed_heap_allowed == A68_FALSE, ERROR_INTERNAL_CONSISTENCY, NULL);
  fixed_heap_pointer += A68_ALIGN (s);
  ABNORMAL_END (fixed_heap_pointer >= temp_heap_pointer, ERROR_OUT_OF_CORE, NULL);
  ABNORMAL_END (((long) z) % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
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
  temp_heap_pointer -= A68_ALIGN (s);
  ABNORMAL_END (fixed_heap_pointer >= temp_heap_pointer, ERROR_OUT_OF_CORE, NULL);
  z = HEAP_ADDRESS (temp_heap_pointer);
  ABNORMAL_END (((long) z) % A68_ALIGNMENT != 0, ERROR_ALIGNMENT, NULL);
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
\brief new_node_info
\return same
**/

NODE_INFO_T *new_node_info (void)
{
  NODE_INFO_T *z = (NODE_INFO_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (NODE_INFO_T));
  MODULE (z) = NULL;
  z->PROCEDURE_LEVEL = 0;
  z->char_in_line = NULL;
  z->symbol = NULL;
  z->line = NULL;
  return (z);
}

/*!
\brief new_node
\return same
**/

NODE_T *new_node (void)
{
  NODE_T *z = (NODE_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (NODE_T));
  MASK (z) = NULL_MASK;
  z->info = new_node_info ();
  z->reduction = 0;
  ATTRIBUTE (z) = 0;
  z->annotation = 0;
  z->error = A68_FALSE;
  z->need_dns = A68_FALSE;
  PROPAGATOR (z).unit = NULL;
  PROPAGATOR (z).source = NULL;
  z->genie.whether_coercion = A68_FALSE;
  z->genie.whether_new_lexical_level = A68_FALSE;
  z->genie.seq = NULL;
  z->genie.seq_set = A68_FALSE;
  z->genie.parent = NULL;
  z->genie.constant = NULL;
  z->genie.argsize = 0;
  z->partial_proc = NULL;
  z->partial_locale = NULL;
  SYMBOL_TABLE (z) = NULL;
  MOID (z) = NULL;
  NEXT (z) = NULL;
  PREVIOUS (z) = NULL;
  SUB (z) = NULL;
  NEST (z) = NULL;
  z->inits = NULL;
  PACK (z) = NULL;
  TAX (z) = NULL;
  z->protect_sweep = NULL;
  return (z);
}

/*!
\brief new_symbol_table
\param p parent symbol table
\return same
**/

SYMBOL_TABLE_T *new_symbol_table (SYMBOL_TABLE_T * p)
{
  SYMBOL_TABLE_T *z = (SYMBOL_TABLE_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (SYMBOL_TABLE_T));
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
  z->inits = NULL;
  return (z);
}

/*!
\brief new_moid
\return same
**/

MOID_T *new_moid ()
{
  MOID_T *z = (MOID_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (MOID_T));
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
  z->equivalent_mode = 0;
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

PACK_T *new_pack ()
{
  PACK_T *z = (PACK_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (PACK_T));
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

TAG_T *new_tag ()
{
  TAG_T *z = (TAG_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (TAG_T));
  MASK (z) = NULL_MASK;
  SYMBOL_TABLE (z) = NULL;
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

SOURCE_LINE_T *new_source_line ()
{
  SOURCE_LINE_T *z = (SOURCE_LINE_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (SOURCE_LINE_T));
  z->marker[0] = NULL_CHAR;
  z->string = NULL;
  z->filename = NULL;
  z->diagnostics = NULL;
  NUMBER (z) = 0;
  z->print_status = 0;
  z->list = A68_TRUE;
  MODULE (z) = NULL;
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
  ROWED (*n) = 0;
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
  BOOL_T match = A68_TRUE, ret;
  while ((IS_UPPER (c[0]) || IS_DIGIT (c[0]) || c[0] == '-') && match) {
    match &= (TO_LOWER (x[0]) == TO_LOWER ((c++)[0]));
    if (!(x[0] == NULL_CHAR || x[0] == alt)) {
      x++;
    }
  }
  while (x[0] != NULL_CHAR && x[0] != alt && c[0] != NULL_CHAR && match) {
    match &= (TO_LOWER ((x++)[0]) == TO_LOWER ((c++)[0]));
  }
  ret = (match ? (x[0] == NULL_CHAR || x[0] == alt) : A68_FALSE);
  return (ret);
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
      match |= (WHETHER (p, a));
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
  ABNORMAL_END (p == NULL || q == NULL, ERROR_INTERNAL_CONSISTENCY, "make_sub");
  MOVE (z, p, ALIGNED_SIZE_OF (NODE_T));
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

double seconds ()
{
#if defined ENABLE_UNIX_CLOCK
  struct rusage rus;
  getrusage (RUSAGE_SELF, &rus);
  return ((double) (rus.ru_utime.tv_sec + rus.ru_utime.tv_usec * 1e-6));
#else
  return (clock () / (double) CLOCKS_PER_SEC);
#endif
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
  SYMBOL (z) = t;
  return (z);
}

/*!
\brief initialise use of elem-lists
**/

void init_postulates ()
{
  top_postulate = NULL;
  old_postulate = NULL;
}

/*!
\brief make old elem-list available for new use
**/

void reset_postulates ()
{
  old_postulate = top_postulate;
  top_postulate = NULL;
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
  if (old_postulate != NULL) {
    new_one = old_postulate;
    old_postulate = NEXT (old_postulate);
  } else {
    new_one = (POSTULATE_T *) get_temp_heap_space (ALIGNED_SIZE_OF (POSTULATE_T));
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
  ABNORMAL_END (core == NULL, ERROR_OUT_OF_CORE, NULL);
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
  *p = (TOKEN_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (TOKEN_T));
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
\brief add keyword to the tree
\param p top keyword
\param a attribute
\param t keyword text
**/

static void add_keyword (KEYWORD_T ** p, int a, char *t)
{
  while (*p != NULL) {
    int k = strcmp (t, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else {
      p = &MORE (*p);
    }
  }
  *p = (KEYWORD_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (KEYWORD_T));
  ATTRIBUTE (*p) = a;
  TEXT (*p) = t;
  LESS (*p) = MORE (*p) = NULL;
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


/*!
\brief make tables of keywords and non-terminals
**/

void set_up_tables ()
{
/* Entries are randomised to balance the tree. */
  add_keyword (&top_keyword, DIAGONAL_SYMBOL, "DIAG");
  add_keyword (&top_keyword, TRANSPOSE_SYMBOL, "TRNSP");
  add_keyword (&top_keyword, ROW_SYMBOL, "ROW");
  add_keyword (&top_keyword, COLUMN_SYMBOL, "COL");
  add_keyword (&top_keyword, ROW_ASSIGN_SYMBOL, "::=");
  add_keyword (&top_keyword, SOUND_SYMBOL, "SOUND");
  add_keyword (&top_keyword, ANDF_SYMBOL, "THEF");
  add_keyword (&top_keyword, ORF_SYMBOL, "ELSF");
  add_keyword (&top_keyword, POINT_SYMBOL, ".");
  add_keyword (&top_keyword, ACCO_SYMBOL, "{");
  add_keyword (&top_keyword, OCCA_SYMBOL, "}");
  add_keyword (&top_keyword, CODE_SYMBOL, "CODE");
  add_keyword (&top_keyword, EDOC_SYMBOL, "EDOC");
  add_keyword (&top_keyword, ENVIRON_SYMBOL, "ENVIRON");
  add_keyword (&top_keyword, COLON_SYMBOL, ":");
  add_keyword (&top_keyword, THEN_BAR_SYMBOL, "|");
  add_keyword (&top_keyword, SUB_SYMBOL, "[");
  add_keyword (&top_keyword, BY_SYMBOL, "BY");
  add_keyword (&top_keyword, OP_SYMBOL, "OP");
  add_keyword (&top_keyword, COMMA_SYMBOL, ",");
  add_keyword (&top_keyword, AT_SYMBOL, "AT");
  add_keyword (&top_keyword, PRIO_SYMBOL, "PRIO");
  add_keyword (&top_keyword, STYLE_I_COMMENT_SYMBOL, "CO");
  add_keyword (&top_keyword, END_SYMBOL, "END");
  add_keyword (&top_keyword, GO_SYMBOL, "GO");
  add_keyword (&top_keyword, TO_SYMBOL, "TO");
  add_keyword (&top_keyword, ELSE_BAR_SYMBOL, "|:");
  add_keyword (&top_keyword, THEN_SYMBOL, "THEN");
  add_keyword (&top_keyword, TRUE_SYMBOL, "TRUE");
  add_keyword (&top_keyword, PROC_SYMBOL, "PROC");
  add_keyword (&top_keyword, FOR_SYMBOL, "FOR");
  add_keyword (&top_keyword, GOTO_SYMBOL, "GOTO");
  add_keyword (&top_keyword, ANDF_SYMBOL, "ANDTH");
  add_keyword (&top_keyword, ORF_SYMBOL, "OREL");
  add_keyword (&top_keyword, WHILE_SYMBOL, "WHILE");
  add_keyword (&top_keyword, IS_SYMBOL, ":=:");
  add_keyword (&top_keyword, ASSIGN_TO_SYMBOL, "=:");
  add_keyword (&top_keyword, COMPLEX_SYMBOL, "COMPLEX");
  add_keyword (&top_keyword, COMPL_SYMBOL, "COMPL");
  add_keyword (&top_keyword, FROM_SYMBOL, "FROM");
  add_keyword (&top_keyword, BOLD_PRAGMAT_SYMBOL, "PRAGMAT");
  add_keyword (&top_keyword, BOLD_COMMENT_SYMBOL, "COMMENT");
  add_keyword (&top_keyword, DO_SYMBOL, "DO");
  add_keyword (&top_keyword, STYLE_II_COMMENT_SYMBOL, "#");
  add_keyword (&top_keyword, CASE_SYMBOL, "CASE");
  add_keyword (&top_keyword, LOC_SYMBOL, "LOC");
  add_keyword (&top_keyword, CHAR_SYMBOL, "CHAR");
  add_keyword (&top_keyword, ISNT_SYMBOL, ":/=:");
  add_keyword (&top_keyword, REF_SYMBOL, "REF");
  add_keyword (&top_keyword, NIL_SYMBOL, "NIL");
  add_keyword (&top_keyword, ASSIGN_SYMBOL, ":=");
  add_keyword (&top_keyword, FI_SYMBOL, "FI");
  add_keyword (&top_keyword, FILE_SYMBOL, "FILE");
  add_keyword (&top_keyword, PAR_SYMBOL, "PAR");
  add_keyword (&top_keyword, ASSERT_SYMBOL, "ASSERT");
  add_keyword (&top_keyword, OUSE_SYMBOL, "OUSE");
  add_keyword (&top_keyword, IN_SYMBOL, "IN");
  add_keyword (&top_keyword, LONG_SYMBOL, "LONG");
  add_keyword (&top_keyword, SEMI_SYMBOL, ";");
  add_keyword (&top_keyword, EMPTY_SYMBOL, "EMPTY");
  add_keyword (&top_keyword, MODE_SYMBOL, "MODE");
  add_keyword (&top_keyword, IF_SYMBOL, "IF");
  add_keyword (&top_keyword, OD_SYMBOL, "OD");
  add_keyword (&top_keyword, OF_SYMBOL, "OF");
  add_keyword (&top_keyword, STRUCT_SYMBOL, "STRUCT");
  add_keyword (&top_keyword, STYLE_I_PRAGMAT_SYMBOL, "PR");
  add_keyword (&top_keyword, BUS_SYMBOL, "]");
  add_keyword (&top_keyword, SKIP_SYMBOL, "SKIP");
  add_keyword (&top_keyword, SHORT_SYMBOL, "SHORT");
  add_keyword (&top_keyword, IS_SYMBOL, "IS");
  add_keyword (&top_keyword, ESAC_SYMBOL, "ESAC");
  add_keyword (&top_keyword, CHANNEL_SYMBOL, "CHANNEL");
  add_keyword (&top_keyword, ANDF_SYMBOL, "ANDF");
  add_keyword (&top_keyword, ORF_SYMBOL, "ORF");
  add_keyword (&top_keyword, REAL_SYMBOL, "REAL");
  add_keyword (&top_keyword, STRING_SYMBOL, "STRING");
  add_keyword (&top_keyword, BOOL_SYMBOL, "BOOL");
  add_keyword (&top_keyword, ISNT_SYMBOL, "ISNT");
  add_keyword (&top_keyword, FALSE_SYMBOL, "FALSE");
  add_keyword (&top_keyword, UNION_SYMBOL, "UNION");
  add_keyword (&top_keyword, OUT_SYMBOL, "OUT");
  add_keyword (&top_keyword, OPEN_SYMBOL, "(");
  add_keyword (&top_keyword, BEGIN_SYMBOL, "BEGIN");
  add_keyword (&top_keyword, FLEX_SYMBOL, "FLEX");
  add_keyword (&top_keyword, VOID_SYMBOL, "VOID");
  add_keyword (&top_keyword, BITS_SYMBOL, "BITS");
  add_keyword (&top_keyword, ELSE_SYMBOL, "ELSE");
  add_keyword (&top_keyword, DOWNTO_SYMBOL, "DOWNTO");
  add_keyword (&top_keyword, UNTIL_SYMBOL, "UNTIL");
  add_keyword (&top_keyword, EXIT_SYMBOL, "EXIT");
  add_keyword (&top_keyword, HEAP_SYMBOL, "HEAP");
  add_keyword (&top_keyword, INT_SYMBOL, "INT");
  add_keyword (&top_keyword, BYTES_SYMBOL, "BYTES");
  add_keyword (&top_keyword, PIPE_SYMBOL, "PIPE");
  add_keyword (&top_keyword, FORMAT_SYMBOL, "FORMAT");
  add_keyword (&top_keyword, SEMA_SYMBOL, "SEMA");
  add_keyword (&top_keyword, CLOSE_SYMBOL, ")");
  add_keyword (&top_keyword, AT_SYMBOL, "@");
  add_keyword (&top_keyword, ELIF_SYMBOL, "ELIF");
  add_keyword (&top_keyword, FORMAT_DELIMITER_SYMBOL, "$");
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
  BOOL_T neg_expo = (expo < 0);
  if (neg_expo) {
    expo = -expo;
  }
  ABNORMAL_END (expo > MAX_DOUBLE_EXPO, "exponent too large", NULL);
  for (dep = pow_10; expo != 0; expo >>= 1, dep++) {
    if (expo & 0x1) {
      dbl_expo *= *dep;
    }
  }
  return (neg_expo ? 1 / dbl_expo : dbl_expo);
}

/*!
\brief atan2 consistent with atan2_mp
\return same
**/

double a68g_atan2 (double x, double y)
{
  if (x == 0.0 && y == 0.0) {
    errno = EDOM;
    return (0.0);
  } else {
    BOOL_T flip = (y < 0.0);
    double z;
    y = ABS (y);
    if (x == 0.0) {
      z = A68G_PI / 2.0;
    } else {
      BOOL_T flop = (x < 0.0);
      x = ABS (x);
      z = atan (y / x);
      if (flop) {
        z = A68G_PI - z;
      }
    }
    if (flip) {
      z = -z;
    }
    return (z);
  }
}

/*!
\brief sqrt (x^2 + y^2) that does not needlessly overflow
\param x x
\param y y
\return same
**/

double a68g_hypot (double x, double y)
{
  double xabs = ABS (x), yabs = ABS (y);
  double min, max;
  if (xabs < yabs) {
    min = xabs;
    max = yabs;
  } else {
    min = yabs;
    max = xabs;
  }
  if (min == 0.0) {
    return (max);
  } else {
    double u = min / max;
    return (max * sqrt (1.0 + u * u));
  }
}

/*!
\brief log (1 + x) with anti-cancellation for IEEE 754
\param x x
\return same
**/

double a68g_log1p (double x)
{
  volatile double y;
  y = 1 + x;
  return log (y) - ((y - 1) - x) / y;   /* cancel errors with IEEE arithmetic. */
}

/*!
\brief inverse hyperbolic sine
\param x x
\return same
**/

double a68g_asinh (double x)
{
  double a = ABS (x), s = (x < 0.0 ? -1.0 : 1.0);
  if (a > 1.0 / sqrt (DBL_EPSILON)) {
    return (s * (log (a) + log (2.0)));
  } else if (a > 2.0) {
    return (s * log (2.0 * a + 1.0 / (a + sqrt (a * a + 1.0))));
  } else if (a > sqrt (DBL_EPSILON)) {
    double a2 = a * a;
    return (s * a68g_log1p (a + a2 / (1.0 + sqrt (1.0 + a2))));
  } else {
    return (x);
  }
}

/*!
\brief inverse hyperbolic cosine
\param x x
\return same
**/

double a68g_acosh (double x)
{
  if (x > 1.0 / sqrt (DBL_EPSILON)) {
    return (log (x) + log (2.0));
  } else if (x > 2.0) {
    return (log (2.0 * x - 1.0 / (sqrt (x * x - 1.0) + x)));
  } else if (x > 1.0) {
    double t = x - 1.0;
    return (a68g_log1p (t + sqrt (2.0 * t + t * t)));
  } else if (x == 1.0) {
    return (0.0);
  } else {
    errno = EDOM;
    return (0.0);
  }
}

/*!
\brief inverse hyperbolic tangent
\param x x
\return same
**/

double a68g_atanh (double x)
{
  double a = ABS (x);
  double s = (x < 0 ? -1 : 1);
  if (a >= 1.0) {
    errno = EDOM;
    return (0.0);
  } else if (a >= 0.5) {
    return (s * 0.5 * a68g_log1p (2 * a / (1.0 - a)));
  } else if (a > DBL_EPSILON) {
    return (s * 0.5 * a68g_log1p (2.0 * a + 2.0 * a * a / (1.0 - a)));
  } else {
    return (x);
  }
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
  nmatch = compiled.re_nsub;
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = malloc (nmatch * ALIGNED_SIZE_OF (regmatch_t));
  if (nmatch > 0 && matches == NULL) {
    regfree (&compiled);
    return (2);
  }
  rc = regexec (&compiled, str, nmatch, matches, 0);
  if (rc != 0) {
    regfree (&compiled);
    return (rc);
  }
/* Find widest match. Do not assume it is the first one. */
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = matches[k].rm_eo - (int) matches[k].rm_so;
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  if (start != NULL) {
    (*start) = matches[max_k].rm_so;
  }
  if (end != NULL) {
    (*end) = matches[max_k].rm_eo;
  }
  free (matches);
  return (0);
#else
  if (strstr (str, pat) != NULL) {
    return (0);
  } else {
    return (1);
  }
#endif /* ENABLE_REGEX */
}
