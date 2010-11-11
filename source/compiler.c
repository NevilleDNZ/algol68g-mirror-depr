/*!
\file code.c
\brief emit C code for Algol 68 constructs.
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
This file generates optimised C routines for many units in an Algol 68 source
program. A68G 1.x contained some general optimised routines. These are 
decommissioned in A68G 2.x that dynamically generates routines depending 
on the source code. The generated routines are compiled on the fly into a dynamic 
library that is linked by the running interpreter. 
To invoke this code generator specify option --optimise.
Currently the optimiser only considers units that operate on basic modes that are
contained in a single C struct, for instance primitive modes

  INT, REAL, BOOL, CHAR and BITS

and simple structures of these basic modes, such as

  COMPLEX

and also (single) references, rows and procedures

  REF MODE, [] MODE, PROC PARAMSETY MODE

The code generator employs a few simple optimisations like constant folding
and common subexpression elimination when DEREFERENCING or SLICING is 
performed; for instance 

  x[i + 1] := x[i + 1] + 1

translates into

  tmp = x[i + 1]; tmp := tmp + 1
 
There are no optimisations that are easily recognised by the back end compiler, 
for instance symbolic simplification.
*/

#include "config.h"
#include "algol68g.h"
#include "interpreter.h"
#include "mp.h"
#include "gsl.h"

#define BASIC(p, n) (basic_unit (locate ((p), (n)))) 

#define ELM "_elem"
#define TMP "_tmp"
#define ARG "_arg"
#define ARR "_array"
#define DRF "_deref"
#define DSP "_display"
#define FUN "_function"
#define REF "_ref"
#define SEL "_field"
#define TUP "_tuple"

#define A68_MAKE_NOTHING 0
#define A68_MAKE_OTHERS 1
#define A68_MAKE_FUNCTION 2

#define OFFSET_OFF(s) (OFFSET (NODE_PACK (SUB (s))))

#define NEEDS_SWEEP(m) (m != NULL && (WHETHER (m, REF_SYMBOL) || WHETHER (DEFLEX (m), ROW_SYMBOL)))
#define NEEDS_DNS(m) (m != NULL && (WHETHER (m, REF_SYMBOL) || WHETHER (m, PROC_SYMBOL) || WHETHER (m, UNION_SYMBOL) || WHETHER (m, FORMAT_SYMBOL)))

#define NAME_SIZE 100

static int indentation = 0;
static char line[BUFFER_SIZE], value[BUFFER_SIZE];

static BOOL_T basic_unit (NODE_T *);
static BOOL_T constant_unit (NODE_T *);
static char *compile_unit (NODE_T *, FILE_T, BOOL_T);
static void inline_unit (NODE_T *, FILE_T, int);
static void compile_units (NODE_T *, FILE_T);
static void indent (FILE_T, char *);
static void indentf (FILE_T, int);
static void push_unit (NODE_T *);

/* The phases we go through */
enum {L_NONE = 0, L_DECLARE = 1, L_INITIALISE, L_EXECUTE, L_EXECUTE_2, L_YIELD, L_PUSH};

/*************************************/
/* Pretty printing of C declarations */
/*************************************/

/*!
\brief add declaration to a tree
\param p top token
\param t token text
\return new entry
**/

typedef struct DEC_T DEC_T;

struct DEC_T {
  char *text;
  int level;
  DEC_T *sub, *less, *more;
};

static DEC_T *root_idf = NULL;

/*!
\brief add declaration to a tree
\param p top declaration
\param idf identifier name
\return new entry
**/

DEC_T *add_identifier (DEC_T ** p, int level, char *idf)
{
  char *z = new_temp_string (idf);
  while (*p != NULL) {
    int k = strcmp (z, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      ABEND(A68_TRUE, "duplicate declaration", z);
      return (*p);
    }
  }
  *p = (DEC_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (DEC_T));
  TEXT (*p) = z;
  LEVEL (*p) = level;  
  SUB (*p) = LESS (*p) = MORE (*p) = NULL;
  return (*p);
}

/*!
\brief add declaration to a tree
\param p top declaration
\param mode mode for identifier
\param idf identifier name
\return new entry
**/

DEC_T *add_declaration (DEC_T ** p, char *mode, int level, char *idf)
{
  char *z = new_temp_string (mode);
  while (*p != NULL) {
    int k = strcmp (z, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      add_identifier(&SUB(*p), level, idf);
      return (*p);
    }
  }
  *p = (DEC_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (DEC_T));
  TEXT (*p) = z;
  LEVEL (*p) = -1;
  SUB (*p) = LESS (*p) = MORE (*p) = NULL;
  add_identifier(&SUB(*p), level, idf);
  return (*p);
}

static BOOL_T put_idf_comma = A68_TRUE;

/*!
\brief print identifiers (following mode)
\param out file to print to
\param p top token
**/

void print_identifiers (FILE_T out, DEC_T *p)
{
  if (p != NULL) {
    print_identifiers (out, LESS (p));
    if (put_idf_comma) {
      WRITE (out, ", ");
    } else {
      put_idf_comma = A68_TRUE;
    }
    if (LEVEL (p) > 0) {
      int k = LEVEL (p);
      while (k--) {
        WRITE (out, "*");
      }
      WRITE (out, " ");
    }
    WRITE (out, TEXT (p)); 
    print_identifiers (out, MORE (p));
  }
}

/*!
\brief print declarations
\param out file to print to
\param p top token
**/

void print_declarations (FILE_T out, DEC_T *p)
{
  if (p != NULL) {
    print_declarations (out, LESS (p));
    indent (out, TEXT (p));
    WRITE (out, " ");
    put_idf_comma = A68_FALSE;
    print_identifiers (out, SUB (p));
    WRITELN (out, ";\n")
    print_declarations (out, MORE (p));
  }
}

/* BOOK keeps track of already seen (temporary) variables and denotations. */

typedef struct {
  int action, phase; 
  char * idf; 
  void * info; 
  int number;
  } BOOK;

enum {BOOK_NONE = 0, BOOK_DECL, BOOK_INIT, BOOK_DEREF, BOOK_ARRAY, BOOK_COMPILE};

#define MAX_BOOK 256
#define NOT_BOOKED NULL

BOOK temp_book[MAX_BOOK], perm_book[MAX_BOOK];
int temp_book_pointer, perm_book_pointer;

/*!
\brief book identifier to keep track of it for CSE
\param number number given to it
\param action some identification as L_DECLARE or DEREFERENCING
\param phase phase in which booking is made
**/

static void book_temp (int action, int phase, char * idf, void * info, int number)
{
  if (temp_book_pointer < MAX_BOOK) {
    temp_book[temp_book_pointer].action = action;
    temp_book[temp_book_pointer].phase = phase;
    temp_book[temp_book_pointer].idf = idf;
    temp_book[temp_book_pointer].info = info;
    temp_book[temp_book_pointer].number = number;
    temp_book_pointer ++;
  }
}

/*!
\brief whether identifier is temp_booked
\param action some identification as L_DECLARE or DEREFERENCING
\param phase phase in which booking is made
\return number given to it
**/

static BOOK * temp_booked (int action, int phase, char * idf)
{
  int k;
  for (k = 0; k < temp_book_pointer; k ++) {
    if (temp_book[k].idf == idf && temp_book[k].action == action && temp_book[k].phase >= phase) {
      return (& (temp_book[k]));
    }
  }
  return (NOT_BOOKED);
}

/*!
\brief book identifier to keep track of it for CSE
\param number number given to it
\param action some identification as L_DECLARE or DEREFERENCING
\param phase phase in which booking is made
**/

static void book_perm (int action, int phase, char * idf, void * info, int number)
{
  if (perm_book_pointer < MAX_BOOK) {
    perm_book[perm_book_pointer].action = action;
    perm_book[perm_book_pointer].phase = phase;
    perm_book[perm_book_pointer].idf = idf;
    perm_book[perm_book_pointer].info = info;
    perm_book[perm_book_pointer].number = number;
    perm_book_pointer ++;
  }
}

/*!
\brief whether identifier is perm_booked
\param action some identification as L_DECLARE or DEREFERENCING
\param phase phase in which booking is made
\return number given to it
**/

static BOOK * perm_booked (int action, int phase, char * idf, void * info)
{
  int k;
  for (k = 0; k < perm_book_pointer; k ++) {
    if (perm_book[k].idf == idf && perm_book[k].action == action &&
        perm_book[k].phase >= phase && perm_book[k].info == info) {
      return (& (perm_book[k]));
    }
  }
  return (NOT_BOOKED);
}

/*!
\brief make name
\param reg output buffer
\param name appropriate name
\param n distinghuising number
\return output buffer
**/

static char * make_name (char * reg, char * name, char * tag, int n)
{
  char num[NAME_SIZE];
  ASSERT (snprintf (num, (size_t) INT_WIDTH, "%d", n) >= 0);
  ABEND (NAME_SIZE - strlen (num) - strlen (tag) - 3 < 1, "make name error", NULL);
  ASSERT (strncpy (reg, name, NAME_SIZE - strlen (num) - strlen (tag) - 3) != NULL);
  if (strlen (tag) > 0) {
    ASSERT (strcat (reg, "_") != NULL);
    ASSERT (strcat (reg, tag) != NULL);
  }
  ASSERT (strcat (reg, "_") != NULL);
  ASSERT (strcat (reg, num) != NULL);
  ABEND (strlen (reg) > NAME_SIZE, "make name error", NULL);
  return (reg);
}

/*!
\brief whether two sub-trees are the same Algol 68 construct
\param l left-hand tree
\param r right-hand tree
\return same
**/

static BOOL_T same_tree (NODE_T * l, NODE_T *r)
{
  if (l == NULL) {
    return ((BOOL_T) (r == NULL));
  } else if (r == NULL) {
    return ((BOOL_T) (l == NULL));
  } else if (ATTRIBUTE (l) == ATTRIBUTE (r) && SYMBOL (l) == SYMBOL (r)) {
    return ((BOOL_T) (same_tree (SUB (l), SUB (r)) && same_tree (NEXT (l), NEXT (r))));
  } else {
    return (A68_FALSE);
  }
}

/* TRANSLATION tabulates translations for genie actions */
typedef int LEVEL_T;
typedef struct {GENIE_PROCEDURE * procedure; char * code;} TRANSLATION;

#define MONADICS 22
static TRANSLATION monadics[MONADICS] = {
  {genie_minus_int, "-"},
  {genie_minus_real, "-"},
  {genie_abs_int, "labs"},
  {genie_abs_real, "fabs"},
  {genie_sign_int, "SIGN"},
  {genie_sign_real, "SIGN"},
  {genie_entier_real, "a68g_entier"},
  {genie_round_real, "a68g_round"},
  {genie_not_bool, "!"},
  {genie_abs_bool, "(int) "},
  {genie_abs_bits, "(int) "},
  {genie_bin_int, "(unsigned) "},
  {genie_not_bits, "~"},
  {genie_abs_char, "TO_UCHAR"},
  {genie_repr_char, ""},
  {genie_re_complex, "a68g_re_complex"},
  {genie_im_complex, "a68g_im_complex"},
  {genie_minus_complex, "a68g_minus_complex"},
  {genie_abs_complex, "a68g_abs_complex"},
  {genie_arg_complex, "a68g_arg_complex"},
  {genie_conj_complex, "a68g_conj_complex"},
  {genie_idle, ""}
};

#define DYADICS 58
static TRANSLATION dyadics[DYADICS] = {
  {genie_add_int, "+"},
  {genie_sub_int, "-"},
  {genie_mul_int, "*"},
  {genie_over_int, "/"},
  {genie_mod_int, "a68g_mod_int"},
  {genie_div_int, "DIV_INT"},
  {genie_eq_int, "=="},
  {genie_ne_int, "!="},
  {genie_lt_int, "<"},
  {genie_gt_int, ">"},
  {genie_le_int, "<="},
  {genie_ge_int, ">="},
  {genie_plusab_int, "a68g_plusab_int"},
  {genie_minusab_int, "a68g_minusab_int"},
  {genie_timesab_int, "a68g_timesab_int"},
  {genie_overab_int, "a68g_overab_int"},
  {genie_add_real, "+"},
  {genie_sub_real, "-"},
  {genie_mul_real, "*"},
  {genie_div_real, "/"},
  {genie_pow_real, "a68g_pow_real"},
  {genie_pow_real_int, "a68g_pow_real_int"},
  {genie_eq_real, "=="},
  {genie_ne_real, "!="},
  {genie_lt_real, "<"},
  {genie_gt_real, ">"},
  {genie_le_real, "<="},
  {genie_ge_real, ">="},
  {genie_plusab_real, "a68g_plusab_real"},
  {genie_minusab_real, "a68g_minusab_real"},
  {genie_timesab_real, "a68g_timesab_real"},
  {genie_divab_real, "a68g_divab_real"},
  {genie_eq_char, "=="},
  {genie_ne_char, "!="},
  {genie_lt_char, "<"},
  {genie_gt_char, ">"},
  {genie_le_char, "<="},
  {genie_ge_char, ">="},
  {genie_eq_bool, "=="},
  {genie_ne_bool, "!="},
  {genie_and_bool, "&"},
  {genie_or_bool, "|"},
  {genie_and_bits, "&"},
  {genie_or_bits, "|"},
  {genie_eq_bits, "=="},
  {genie_ne_bits, "!="},
  {genie_shl_bits, "<<"},
  {genie_shr_bits, ">>"},
  {genie_icomplex, "a68g_i_complex"},
  {genie_iint_complex, "a68g_i_complex"},
  {genie_abs_complex, "a68g_abs_complex"},
  {genie_arg_complex, "a68g_arg_complex"},
  {genie_add_complex, "a68g_add_complex"},
  {genie_sub_complex, "a68g_sub_complex"},
  {genie_mul_complex, "a68g_mul_complex"},
  {genie_div_complex, "a68g_div_complex"},
  {genie_eq_complex, "a68g_eq_complex"},
  {genie_ne_complex, "a68g_ne_complex"}
};

#define FUNCTIONS 28
static TRANSLATION functions[FUNCTIONS] = {
  {genie_sqrt_real, "sqrt"},
  {genie_curt_real, "curt"},
  {genie_exp_real, "a68g_exp"},
  {genie_ln_real, "log"},
  {genie_log_real, "log10"},
  {genie_sin_real, "sin"},
  {genie_cos_real, "cos"},
  {genie_tan_real, "tan"},
  {genie_arcsin_real, "asin"},
  {genie_arccos_real, "acos"},
  {genie_arctan_real, "atan"},
  {genie_sinh_real, "sinh"},
  {genie_cosh_real, "cosh"},
  {genie_tanh_real, "tanh"},
  {genie_arcsinh_real, "a68g_asinh"},
  {genie_arccosh_real, "a68g_acosh"},
  {genie_arctanh_real, "a68g_atanh"},
  {genie_inverf_real, "inverf"},
  {genie_inverfc_real, "inverfc"},
  {genie_sqrt_complex, "a68g_sqrt_complex"},
  {genie_exp_complex, "a68g_exp_complex"},
  {genie_ln_complex, "a68g_ln_complex"},
  {genie_sin_complex, "a68g_sin_complex"},
  {genie_cos_complex, "a68g_cos_complex"},
  {genie_tan_complex, "a68g_tan_complex"},
  {genie_arcsin_complex, "a68g_arcsin_complex"},
  {genie_arccos_complex, "a68g_arccos_complex"},
  {genie_arctan_complex, "a68g_arctan_complex"}
};

#define CONSTANTS 29
static TRANSLATION constants[CONSTANTS] = {
  {genie_int_lengths, "3"},
  {genie_int_shorths, "1"},
  {genie_real_lengths, "3"},
  {genie_real_shorths, "1"},
  {genie_complex_lengths, "3"},
  {genie_complex_shorths, "1"},
  {genie_bits_lengths, "3"},
  {genie_bits_shorths, "1"},
  {genie_bytes_lengths, "2"},
  {genie_bytes_shorths, "1"},
  {genie_int_width, "INT_WIDTH"},
  {genie_long_int_width, "LONG_INT_WIDTH"},
  {genie_longlong_int_width, "LONGLONG_INT_WIDTH"},
  {genie_real_width, "REAL_WIDTH"},
  {genie_long_real_width, "LONG_REAL_WIDTH"},
  {genie_longlong_real_width, "LONGLONG_REAL_WIDTH"},
  {genie_exp_width, "EXP_WIDTH"},
  {genie_long_exp_width, "LONG_EXP_WIDTH"},
  {genie_longlong_exp_width, "LONGLONG_EXP_WIDTH"},
  {genie_bits_width, "BITS_WIDTH"},
  {genie_bytes_width, "BYTES_WIDTH"},
  {genie_long_bytes_width, "LONG_BYTES_WIDTH"},
  {genie_max_abs_char, "UCHAR_MAX"},
  {genie_max_int, "A68_MAX_INT"},
  {genie_max_real, "DBL_MAX"},
  {genie_min_real, "DBL_MIN"},
  {genie_null_char, "NULL_CHAR"},
  {genie_small_real, "DBL_EPSILON"},
  {genie_pi, "A68_PI"}
};

/********************/
/* Basic mode check */
/********************/

/*!
\brief whether primitive mode
\param m mode to check
\return same
**/

static BOOL_T primitive_mode (MOID_T * m)
{
  if (m == MODE (INT)) {
    return (A68_TRUE);
  } else if (m == MODE (REAL)) {
    return (A68_TRUE);
  } else if (m == MODE (BOOL)) {
    return (A68_TRUE);
  } else if (m == MODE (CHAR)) {
    return (A68_TRUE);
  } else if (m == MODE (BITS)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

#define FOLDER_MODE(m) (primitive_mode (m) || m == MODE (COMPLEX))

/*!
\brief whether basic mode
\param m mode to check
\return same
**/

static BOOL_T basic_mode (MOID_T * m)
{
  if (primitive_mode (m)) {
    return (A68_TRUE);
  } else if (WHETHER (m, REF_SYMBOL)) {
    if (WHETHER (SUB (m), REF_SYMBOL) || WHETHER (SUB (m), PROC_SYMBOL)) {
      return (A68_FALSE);
    } else {
      return (basic_mode (SUB (m)));
    }
  } else if (WHETHER (m, ROW_SYMBOL)) {
    if (primitive_mode (SUB (m))) {
      return (A68_TRUE);
    } else if (WHETHER (SUB (m), STRUCT_SYMBOL)) {
      return (basic_mode (SUB (m)));
    } else {
      return (A68_FALSE);
    }
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NULL; FORWARD (p)) {
      if (!primitive_mode (MOID (p))) {
        return (A68_FALSE);
      }
    }
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether basic mode
\param m mode to check
\return same
**/

static BOOL_T basic_mode_non_row (MOID_T * m)
{
  if (primitive_mode (m)) {
    return (A68_TRUE);
  } else if (WHETHER (m, REF_SYMBOL)) {
    if (WHETHER (SUB (m), REF_SYMBOL) || WHETHER (SUB (m), PROC_SYMBOL)) {
      return (A68_FALSE);
    } else {
      return (basic_mode_non_row (SUB (m)));
    }
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NULL; FORWARD (p)) {
      if (!primitive_mode (MOID (p))) {
        return (A68_FALSE);
      }
    }
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether stems from certain attribute
\param p position in tree
\param att attribute to comply to
\return same
**/

static NODE_T * locate (NODE_T * p, int att)
{
  if (WHETHER (p, VOIDING)) {
    return (locate (SUB (p), att));
  } else if (WHETHER (p, UNIT)) {
    return (locate (SUB (p), att));
  } else if (WHETHER (p, TERTIARY)) {
    return (locate (SUB (p), att));
  } else if (WHETHER (p, SECONDARY)) {
    return (locate (SUB (p), att));
  } else if (WHETHER (p, PRIMARY)) {
    return (locate (SUB (p), att));
  } else if (WHETHER (p, att)) {
    return (p);
  } else {
    return (NULL);
  }
}

/*************************/
/* ROUTINES TO EMIT CODE */
/*************************/

/*!
\brief write indented text
\param out output file descriptor
\param str text
**/

static void indent (FILE_T out, char *str)
{
  int j = indentation;
  if (out == 0) {
    return;
  }
  while (j -- > 0) {
    WRITE (out, "  ");
  }
  WRITE (out, str);
}

/*!
\brief write unindented text
\param out output file descriptor
\param str text
**/

static void undent (FILE_T out, char *str)
{
  if (out == 0) {
    return;
  }
  WRITE (out, str);
}

/*!
\brief write indent text
\param out output file descriptor
\param ret bytes written by snprintf
**/

static void indentf (FILE_T out, int ret)
{
  if (out == 0) {
    return;
  }
  if (ret >= 0) {
    indent (out, line);
  } else {
    ABEND(A68_TRUE, "Return value failure", ERROR_SPECIFICATION);
  }
}

/*!
\brief write unindent text
\param out output file descriptor
\param ret bytes written by snprintf
**/

static void undentf(FILE_T out, int ret)
{
  if (out == 0) {
    return;
  }
  if (ret >= 0) {
    WRITE (out, line);
  } else {
    ABEND(A68_TRUE, "Return value failure", ERROR_SPECIFICATION);
  }
}

/*!
\brief comment source line
\param p position in tree
\param out output file descriptor
**/

static void comment_tree (NODE_T * p, FILE_T out, int *want_space, int *max_print)
{
/* Take care not to generate nested comments */
#define UNDENT(out, p) {\
  char * q;\
  for (q = p; q[0] != NULL_CHAR; q ++) {\
    if (q[0] == '*' && q[1] == '/') {\
      undent (out, "\\*\\/");\
      q ++;\
    } else if (q[0] == '/' && q[1] == '*') {\
      undent (out, "\\/\\*");\
      q ++;\
    } else {\
      char w[2];\
      w[0] = q[0];\
      w[1] = NULL_CHAR;\
      undent (out, w);\
    }\
  }}
/**/
  for (; p != NULL && (*max_print) >= 0; FORWARD (p)) {
    if (WHETHER (p, ROW_CHAR_DENOTATION)) {
      if (*want_space != 0) {
        UNDENT (out, " ");
      }
      UNDENT (out, "\"");
      UNDENT (out, SYMBOL (p));
      UNDENT (out, "\"");
      *want_space = 2;
    } else if (SUB (p) != NULL) {
      comment_tree (SUB (p), out, want_space, max_print);
    } else  if (SYMBOL (p)[0] == '(' || SYMBOL (p)[0] == '[' || SYMBOL (p)[0] == '{') {
      if (*want_space == 2) {
        UNDENT (out, " ");
      }
      UNDENT (out, SYMBOL (p));
      *want_space = 0;
    } else  if (SYMBOL (p)[0] == ')' || SYMBOL (p)[0] == ']' || SYMBOL (p)[0] == '}') {
      UNDENT (out, SYMBOL (p));
      *want_space = 1;
    } else  if (SYMBOL (p)[0] == ';' || SYMBOL (p)[0] == ',') {
      UNDENT (out, SYMBOL (p));
      *want_space = 2;
    } else  if (strlen (SYMBOL (p)) == 1 && (SYMBOL (p)[0] == '.' || SYMBOL (p)[0] == ':')) {
      UNDENT (out, SYMBOL (p));
      *want_space = 2;
    } else {
      if (*want_space != 0) {
        UNDENT (out, " ");
      }
      if ((*max_print) > 0) {
        UNDENT (out, SYMBOL (p));
      } else if ((*max_print) == 0) {
        UNDENT (out, "...");
      }
      (*max_print)--;
      if (IS_UPPER (SYMBOL (p)[0])) {
        *want_space = 2;
      } else if (! IS_ALNUM (SYMBOL (p)[0])) {
        *want_space = 2;
      } else {
        *want_space = 1;
      }
    }
  }
#undef UNDENT
}

/*!
\brief comment source line
\param p position in tree
\param out output file descriptor
**/

static void comment_source (NODE_T * p, FILE_T out)
{
  int want_space = 0, max_print = 16;
  undentf(out, snprintf (line, BUFFER_SIZE, "/* %s: %d: ", LINE (p)->filename, NUMBER (LINE (p))));
  comment_tree (p, out, &want_space, &max_print);
  undent (out, " */\n");
}

/*!
\brief write prelude
\param out output file descriptor
**/

static void write_prelude (FILE_T out)
{
  indentf (out, snprintf (line, BUFFER_SIZE, "/* Algol 68 Genie %s */\n", REVISION));
  indentf (out, snprintf (line, BUFFER_SIZE, "/* \"%s\" */\n\n", program.files.object.name));
#if defined ENABLE_IEEE_754
  indent (out, "#define ENABLE_IEEE_754\n\n");
#endif
  indentf (out, snprintf (line, BUFFER_SIZE, "#include \"%s/a68g.h\"\n\n", COMPILER_INC));
  indent (out, "#define CODE(n) PROPAGATOR_T n (NODE_T * p) {\\\n");
  indent (out, "  PROPAGATOR_T self;\n\n");
  indent (out, "#define EDOC(n, q) self.unit = n;\\\n");
  indent (out, "  self.source = q;\\\n");
  indent (out, "  (void) p;\\\n");
  indent (out, "  return (self);}\n\n");
  indent (out, "#define DIV_INT(i, j) ((double) (i) / (double) (j))\n");
  indent (out, "#define N(n) (node_register[n])\n");
  indent (out, "#define S(z) (STATUS (z))\n");
  indent (out, "#define V(z) (VALUE (z))\n\n");
}

/*!
\brief write initialisation of frame
\**/

static void init_static_frame (FILE_T out, NODE_T * p)
{
  if (SYMBOL_TABLE (p)->ap_increment > 0) { 
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_CLEAR (%d);\n", SYMBOL_TABLE (p)->ap_increment));
  }
  if (LEX_LEVEL (p) == global_level) {
    indent (out, "global_pointer = frame_pointer;\n");
  }
  indentf (out, snprintf (line, BUFFER_SIZE, "if (SYMBOL_TABLE (N(%d))->initialise_frame) {\n", NUMBER (p)));
  indentation++;
  indentf (out, snprintf (line, BUFFER_SIZE, "initialise_frame (N(%d));\n", NUMBER (p)));
  indentation--;
  indent (out, "}\n");
}

/***********************/
/* Constant unit check */
/***********************/

/*!
\brief whether constant collateral clause
\param p position in tree
\return same
**/

static BOOL_T constant_collateral (NODE_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else if (WHETHER (p, UNIT)) {
    return ((BOOL_T) (FOLDER_MODE (MOID (p)) && constant_unit (SUB (p)) && constant_collateral (NEXT (p))));
  } else {
    return ((BOOL_T) (constant_collateral (SUB (p)) && constant_collateral (NEXT (p))));
  }
}

/*!
\brief whether constant serial clause
\param p position in tree
\return same
**/

static void count_constant_units (NODE_T * p, int * total, int * good)
{
  if (p != NULL) {
    if (WHETHER (p, UNIT)) {
      (* total) ++;
      if (constant_unit (p)) {
        (* good) ++;
      }
      count_constant_units (NEXT (p), total, good);
    } else {
      count_constant_units (SUB (p), total, good);
      count_constant_units (NEXT (p), total, good);
    }
  }
}

/*!
\brief whether constant serial clause
\param p position in tree
\param want > 0 is how many units we allow, <= 0 is don't care
\return same
**/

static BOOL_T constant_serial (NODE_T * p, int want)
{
  int total = 0, good = 0;
  count_constant_units (p, &total, &good);
  if (want > 0) {
    return (total == want && total == good);
  } else {
    return (total == good);
  }
}

/*!
\brief whether constant argument
\param p starting node
\return same
**/

static BOOL_T constant_argument (NODE_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else if (WHETHER (p, UNIT)) {
    return ((BOOL_T) (FOLDER_MODE (MOID (p)) && constant_unit (p) && constant_argument (NEXT (p))));
  } else {
    return ((BOOL_T) (constant_argument (SUB (p)) && constant_argument (NEXT (p))));
  }
}

/*!
\brief whether constant call
\param p starting node
\return same
**/

static BOOL_T constant_call (NODE_T * p)
{
  if (WHETHER (p, CALL)) {
    NODE_T * prim = SUB (p);
    NODE_T * idf = locate (prim, IDENTIFIER);
    if (idf != NULL) {
      int k;
      for (k = 0; k < FUNCTIONS; k ++) {
        if (TAX (idf)->procedure == functions[k].procedure) {
          NODE_T * args = NEXT (prim);
          return (constant_argument (args));
        }
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether constant monadic formula
\param p starting node
\return same
**/

static BOOL_T constant_monadic_formula (NODE_T * p)
{
  if (WHETHER (p, MONADIC_FORMULA)) {
    NODE_T * op = SUB (p);
    int k;
    for (k = 0; k < MONADICS; k ++) {
      if (TAX (op)->procedure == monadics[k].procedure) {
        NODE_T * rhs = NEXT (op);
        return (constant_unit (rhs));
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether constant dyadic formula
\param p starting node
\return same
**/

static BOOL_T constant_formula (NODE_T * p)
{
  if (WHETHER (p, FORMULA)) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NULL) {
      return (constant_monadic_formula (lhs));
    } else {
      int k;
      for (k = 0; k < DYADICS; k ++) {
        if (TAX (op)->procedure == dyadics[k].procedure) {
          NODE_T * rhs = NEXT (op);
          return ((BOOL_T) (constant_unit (lhs) && constant_unit (rhs)));
        }
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether constant unit
\param p starting node
\return same
**/

static BOOL_T constant_unit (NODE_T * p)
{
  if (p == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, UNIT)) {
    return (constant_unit (SUB (p)));
  } else if (WHETHER (p, TERTIARY)) {
    return (constant_unit (SUB (p)));
  } else if (WHETHER (p, SECONDARY)) {
    return (constant_unit (SUB (p)));
  } else if (WHETHER (p, PRIMARY)) {
    return (constant_unit (SUB (p)));
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    return (constant_unit (SUB (p)));
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    return (constant_serial (NEXT_SUB (p), 1));
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    return (FOLDER_MODE (MOID (p)) && constant_collateral (NEXT_SUB (p)));
  } else if (WHETHER (p, WIDENING)) {
    if (MOID (p) == MODE (REAL)) {
      return (constant_unit (SUB (p)));
    } else if (MOID (p) == MODE (COMPLEX)) {
      return (constant_unit (SUB (p)));
    } else {
      return (A68_FALSE);
    }
  } else if (WHETHER (p, IDENTIFIER)) {
    if (TAX (p)->stand_env_proc) {
      int k;
      for (k = 0; k < CONSTANTS; k ++) {
        if (TAX (p)->procedure == constants[k].procedure) {
          return (A68_TRUE);
        }
      }
      return (A68_FALSE);
    } else {
/* Possible constant folding */
      NODE_T * def = NODE (TAX (p));
      BOOL_T ret = A68_FALSE;
      if (STATUS (p) & COOKIE_MASK) {
        diagnostic_node (A68_WARNING, p, WARNING_UNINITIALISED, NULL);
      } else {
        STATUS (p) |= COOKIE_MASK;
        if (FOLDER_MODE (MOID (p)) && def != NULL && NEXT (def) != NULL && WHETHER (NEXT (def), EQUALS_SYMBOL)) {
          ret = constant_unit (NEXT_NEXT (def));
        }
      }
      STATUS (p) &= !(COOKIE_MASK);
      return (ret);
    }
  } else if (WHETHER (p, DENOTATION)) {
    return (primitive_mode (MOID (p)));
  } else if (WHETHER (p, MONADIC_FORMULA)) {
    return ((BOOL_T) (FOLDER_MODE (MOID (p)) && constant_monadic_formula (p)));
  } else if (WHETHER (p, FORMULA)) {
    return ((BOOL_T) (FOLDER_MODE (MOID (p)) && constant_formula (p)));
  } else if (WHETHER (p, CALL)) {
    return ((BOOL_T) (FOLDER_MODE (MOID (p)) && constant_call (p)));
  } else if (WHETHER (p, CAST)) {
    return ((BOOL_T) (FOLDER_MODE (MOID (SUB (p))) && constant_unit (NEXT_SUB (p))));
  } else {
    return (A68_FALSE);
  }
}

/********************/
/* Basic unit check */
/********************/

/*!
\brief whether basic collateral clause
\param p position in tree
\return same
**/

static BOOL_T basic_collateral (NODE_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else if (WHETHER (p, UNIT)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_unit (SUB (p)) && basic_collateral (NEXT (p))));
  } else {
    return ((BOOL_T) (basic_collateral (SUB (p)) && basic_collateral (NEXT (p))));
  }
}

/*!
\brief whether basic serial clause
\param p position in tree
\return same
**/

static void count_basic_units (NODE_T * p, int * total, int * good)
{
  if (p != NULL) {
    if (WHETHER (p, UNIT)) {
      (* total) ++;
      if (basic_unit (p)) {
        (* good) ++;
      }
      count_basic_units (NEXT (p), total, good);
    } else {
      count_basic_units (SUB (p), total, good);
      count_basic_units (NEXT (p), total, good);
    }
  }
}

/*!
\brief whether basic serial clause
\param p position in tree
\param want > 0 is how many units we allow, <= 0 is don't care
\return same
**/

static BOOL_T basic_serial (NODE_T * p, int want)
{
  int total = 0, good = 0;
  count_basic_units (p, &total, &good);
  if (want > 0) {
    return (total == want && total == good);
  } else {
    return (total == good);
  }
}

/*!
\brief whether basic indexer
\param p position in tree
\return same
**/

static BOOL_T basic_indexer (NODE_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else if (WHETHER (p, TRIMMER)) {
    return (A68_FALSE);
  } else if (WHETHER (p, UNIT)) {
    return (basic_unit (p));
  } else {
    return ((BOOL_T) (basic_indexer (SUB (p)) && basic_indexer (NEXT (p))));
  }
}

/*!
\brief whether basic slice
\param p starting node
\return same
**/

static BOOL_T basic_slice (NODE_T * p)
{
  if (WHETHER (p, SLICE)) {
    NODE_T * prim = SUB (p);
    NODE_T * idf = locate (prim, IDENTIFIER);
    if (idf != NULL) {
      NODE_T * indx = NEXT (prim);
      return (basic_indexer (indx));
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether basic argument
\param p starting node
\return same
**/

static BOOL_T basic_argument (NODE_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else if (WHETHER (p, UNIT)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_unit (p) && basic_argument (NEXT (p))));
  } else {
    return ((BOOL_T) (basic_argument (SUB (p)) && basic_argument (NEXT (p))));
  }
}

/*!
\brief whether basic call
\param p starting node
\return same
**/

static BOOL_T basic_call (NODE_T * p)
{
  if (WHETHER (p, CALL)) {
    NODE_T * prim = SUB (p);
    NODE_T * idf = locate (prim, IDENTIFIER);
    if (idf == NULL) {
      return (A68_FALSE);
    } else if (SUB_MOID (idf) == MOID (p)) { /* Prevent partial parametrisation */
      int k;
      for (k = 0; k < FUNCTIONS; k ++) {
        if (TAX (idf)->procedure == functions[k].procedure) {
          NODE_T * args = NEXT (prim);
          return (basic_argument (args));
        }
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether basic monadic formula
\param p starting node
\return same
**/

static BOOL_T basic_monadic_formula (NODE_T * p)
{
  if (WHETHER (p, MONADIC_FORMULA)) {
    NODE_T * op = SUB (p);
    int k;
    for (k = 0; k < MONADICS; k ++) {
      if (TAX (op)->procedure == monadics[k].procedure) {
        NODE_T * rhs = NEXT (op);
        return (basic_unit (rhs));
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether basic dyadic formula
\param p starting node
\return same
**/

static BOOL_T basic_formula (NODE_T * p)
{
  if (WHETHER (p, FORMULA)) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NULL) {
      return (basic_monadic_formula (lhs));
    } else {
      int k;
      for (k = 0; k < DYADICS; k ++) {
        if (TAX (op)->procedure == dyadics[k].procedure) {
          NODE_T * rhs = NEXT (op);
          return ((BOOL_T) (basic_unit (lhs) && basic_unit (rhs)));
        }
      }
    }
  }
  return (A68_FALSE);
}
/*!
\brief whether basic conditional clause
\param p starting node
\return same
**/

static BOOL_T basic_conditional (NODE_T * p)
{
  if (! (WHETHER (p, IF_PART) || WHETHER (p, OPEN_PART))) {
    return (A68_FALSE);
  }
  if (! basic_serial (NEXT_SUB (p), 1)) {
    return (A68_FALSE);
  }
  FORWARD (p);
  if (! (WHETHER (p, THEN_PART) || WHETHER (p, CHOICE))) {
    return (A68_FALSE);
  }
  if (! basic_serial (NEXT_SUB (p), 1)) {
    return (A68_FALSE);
  }
  FORWARD (p);
  if (WHETHER (p, ELSE_PART) || WHETHER (p, CHOICE)) {
    return (basic_serial (NEXT_SUB (p), 1));
  } else if (WHETHER (p, FI_SYMBOL)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether basic unit
\param p starting node
\return same
**/

static BOOL_T basic_unit (NODE_T * p)
{
  if (p == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, UNIT)) {
    return (basic_unit (SUB (p)));
  } else if (WHETHER (p, TERTIARY)) {
    return (basic_unit (SUB (p)));
  } else if (WHETHER (p, SECONDARY)) {
    return (basic_unit (SUB (p)));
  } else if (WHETHER (p, PRIMARY)) {
    return (basic_unit (SUB (p)));
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    return (basic_unit (SUB (p)));
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    return (basic_serial (NEXT_SUB (p), 1));
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    return (basic_mode (MOID (p)) && basic_collateral (NEXT_SUB (p)));
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    return (basic_mode (MOID (p)) && basic_conditional (SUB (p)));
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), IDENTIFIER) != NULL) {
    NODE_T * dst = SUB_SUB (p);
    NODE_T * src = NEXT_NEXT (dst);
    return ((BOOL_T) basic_unit (src) && basic_mode_non_row (MOID (src)));
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SLICE) != NULL) {
    NODE_T * dst = SUB_SUB (p);
    NODE_T * src = NEXT_NEXT (dst);
    NODE_T * slice = locate (dst, SLICE);
    return ((BOOL_T) (WHETHER (MOID (slice), REF_SYMBOL) && basic_slice (slice) && basic_unit (src) && basic_mode_non_row (MOID (src))));
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SELECTION) != NULL) {
    NODE_T * dst = SUB_SUB (p);
    NODE_T * src = NEXT_NEXT (dst);
    return ((BOOL_T) (locate (NEXT_SUB (locate (dst, SELECTION)), IDENTIFIER) != NULL && basic_unit (src) && basic_mode_non_row (MOID (dst))));
  } else if (WHETHER (p, VOIDING)) {
    return (basic_unit (SUB (p)));
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), IDENTIFIER)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && BASIC (SUB (p), IDENTIFIER)));
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SLICE)) {
    NODE_T * slice = locate (SUB (p), SLICE);
    return ((BOOL_T) (basic_mode (MOID (p)) && WHETHER (MOID (SUB (slice)), REF_SYMBOL) && basic_slice (slice)));
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SELECTION)) {
    return ((BOOL_T) (primitive_mode (MOID (p)) && BASIC (SUB (p), SELECTION)));
  } else if (WHETHER (p, WIDENING)) {
    if (MOID (p) == MODE (REAL)) {
      return (basic_unit (SUB (p)));
    } else if (MOID (p) == MODE (COMPLEX)) {
      return (basic_unit (SUB (p)));
    } else {
      return (A68_FALSE);
    }
  } else if (WHETHER (p, IDENTIFIER)) {
    if (TAX (p)->stand_env_proc) {
      int k;
      for (k = 0; k < CONSTANTS; k ++) {
        if (TAX (p)->procedure == constants[k].procedure) {
          return (A68_TRUE);
        }
      }
      return (A68_FALSE);
    } else {
      return (basic_mode (MOID (p)));
    }
  } else if (WHETHER (p, DENOTATION)) {
    return (primitive_mode (MOID (p)));
  } else if (WHETHER (p, MONADIC_FORMULA)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_monadic_formula (p)));
  } else if (WHETHER (p, FORMULA)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_formula (p)));
  } else if (WHETHER (p, CALL)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_call (p)));
  } else if (WHETHER (p, CAST)) {
    return ((BOOL_T) (FOLDER_MODE (MOID (SUB (p))) && basic_unit (NEXT_SUB (p))));
  } else if (WHETHER (p, SLICE)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_slice (p)));
  } else if (WHETHER (p, SELECTION)) {
    NODE_T * sec = locate (NEXT_SUB (p), IDENTIFIER);
    if (sec == NULL) {
      return (A68_FALSE);
    } else {
      return (basic_mode_non_row (MOID (sec)));
    }
  } else if (WHETHER (p, IDENTITY_RELATION)) {
#define GOOD(p) (locate (p, IDENTIFIER) != NULL && WHETHER (MOID (locate ((p), IDENTIFIER)), REF_SYMBOL))
    NODE_T * lhs = SUB (p);
    NODE_T * rhs = NEXT_NEXT (lhs);
    if (GOOD (lhs) && GOOD (rhs)) {
      return (A68_TRUE);
    } else if (GOOD (lhs) && locate (rhs, NIHIL) != NULL) {
      return (A68_TRUE);
    } else {
      return (A68_FALSE);
    }
#undef GOOD
  } else {
    return (A68_FALSE);
  }
}

/*******************/
/* Constant folder */
/*******************/

/*!
\brief push denotation
\param p position in tree
\return same
**/

static void push_denotation (NODE_T * p)
{
#define PUSH_DENOTATION(mode, decl) {\
  decl z;\
  if (genie_string_to_value_internal (p, MODE (mode), SYMBOL (p), (BYTE_T *) & z) == A68_FALSE) {\
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, MODE (mode));\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  PUSH_PRIMITIVE (p, VALUE (&z), decl);}
/**/
  if (MOID (p) == MODE (INT)) {
    PUSH_DENOTATION (INT, A68_INT);
  } else if (MOID (p) == MODE (REAL)) {
    PUSH_DENOTATION (REAL, A68_REAL);
  } else if (MOID (p) == MODE (BOOL)) {
    PUSH_DENOTATION (BOOL, A68_BOOL);
  } else if (MOID (p) == MODE (CHAR)) {
    if ((SYMBOL (p))[0] == NULL_CHAR) {
      PUSH_PRIMITIVE (p, NULL_CHAR, A68_CHAR);
    } else {
      PUSH_PRIMITIVE (p, (SYMBOL (p))[0], A68_CHAR);
    }
  } else if (MOID (p) == MODE (BITS)) {
    PUSH_DENOTATION (BITS, A68_BITS);
  }
#undef PUSH_DENOTATION
}

/*!
\brief push widening
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static void push_widening (NODE_T * p)
{
  push_unit (SUB (p));
  if (MOID (p) == MODE (REAL)) {
    A68_INT k;
    POP_OBJECT (p, &k, A68_INT);
    PUSH_PRIMITIVE (p, (double) VALUE (&k), A68_REAL);
  } else if (MOID (p) == MODE (COMPLEX)) {
    A68_REAL x;
    POP_OBJECT (p, &x, A68_REAL);
    PUSH_PRIMITIVE (p, VALUE (&x), A68_REAL);
    PUSH_PRIMITIVE (p, 0.0, A68_REAL);
  }
}

/*!
\brief code collateral units
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static void push_collateral_units (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    push_unit (p);
  } else {
    push_collateral_units (SUB (p));
    push_collateral_units (NEXT (p));
  }
}

/*!
\brief code argument
\param p starting node
\return same
**/

static void push_argument (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      push_unit (p);
    } else {
      push_argument (SUB (p));
    }
  }
}

/*!
\brief whether constant unit
\param p starting node
\return same
**/

static void push_unit (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    push_unit (SUB (p));
  } else if (WHETHER (p, TERTIARY)) {
    push_unit (SUB (p));
  } else if (WHETHER (p, SECONDARY)) {
    push_unit (SUB (p));
  } else if (WHETHER (p, PRIMARY)) {
    push_unit (SUB (p));
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    push_unit (SUB (p));
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    push_unit (SUB (NEXT_SUB (p)));
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    push_collateral_units (NEXT_SUB (p));
  } else if (WHETHER (p, WIDENING)) {
    push_widening (p);
  } else if (WHETHER (p, IDENTIFIER)) {
    if (TAX (p)->stand_env_proc) {
      (void) (*(TAX (p)->procedure)) (p);
    } else {
    /* Possible constant folding */
      NODE_T * def = NODE (TAX (p));
      push_unit (NEXT_NEXT (def));
    }
  } else if (WHETHER (p, DENOTATION)) {
    push_denotation (p);
  } else if (WHETHER (p, MONADIC_FORMULA)) {
    NODE_T * op = SUB (p);
    NODE_T * rhs = NEXT (op); 
    push_unit (rhs);
    (*(TAX (op)->procedure)) (op);
  } else if (WHETHER (p, FORMULA)) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NULL) {
      push_unit (lhs);
    } else {
      NODE_T * rhs = NEXT (op);
      push_unit (lhs);
      push_unit (rhs);
      (*(TAX (op)->procedure)) (op);
    }
  } else if (WHETHER (p, CALL)) {
    NODE_T * prim = SUB (p);
    NODE_T * args = NEXT (prim);
    NODE_T * idf = locate (prim, IDENTIFIER);
    push_argument (args);
    (void) (*(TAX (idf)->procedure)) (idf);
  } else if (WHETHER (p, CAST)) {
    push_unit (NEXT_SUB (p));
  }
}

/*!
\brief Code constant folding
\param p node to start
**/

static void constant_folder (NODE_T * p, FILE_T out, int phase)
{
  if (phase == L_DECLARE) {
    if (MOID (p) == MODE (COMPLEX)) {
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      add_declaration (&root_idf, "A68_COMPLEX", 0, acc);
    }
  } else if (phase == L_EXECUTE) {
    if (MOID (p) == MODE (COMPLEX)) {
      A68_REAL re, im;
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &im, A68_REAL);
      POP_OBJECT (p, &re, A68_REAL);
      indentf (out, snprintf (line, BUFFER_SIZE, "STATUS_RE (%s) = INITIALISED_MASK;\n", acc)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "STATUS_IM (%s) = INITIALISED_MASK;\n", acc)); 
      ASSERT (snprintf (value, BUFFER_SIZE, "%.*g", REAL_WIDTH, VALUE (&re)) >= 0);
      indentf (out, snprintf (line, BUFFER_SIZE, "RE (%s) = %s;\n", acc, value)); 
      ASSERT (snprintf (value, BUFFER_SIZE, "%.*g", REAL_WIDTH, VALUE (&im)) >= 0);
      indentf (out, snprintf (line, BUFFER_SIZE, "IM (%s) = %s;\n", acc, value)); 
      ABEND (stack_pointer > 0, "stack not empty", NULL);
    }
  } else if (phase == L_YIELD) {
    if (MOID (p) == MODE (INT)) {
      A68_INT k;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &k, A68_INT);
      ASSERT (snprintf (line, BUFFER_SIZE, "%d", VALUE (&k)) >= 0);
      undent (out, line);
      ABEND (stack_pointer > 0, "stack not empty", NULL);
    } else if (MOID (p) == MODE (REAL)) {
      A68_REAL x;
      double conv;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &x, A68_REAL);
/* Avoid printing overflowing or underflowing values */
      ASSERT (snprintf (line, BUFFER_SIZE, "%.*g", REAL_WIDTH, VALUE (&x)) >= 0);
      errno = 0;
      conv = strtod (line, NULL);
      if (errno == ERANGE && conv == 0.0) {
        undent (out, "0.0");
      } else if (errno == ERANGE && conv == HUGE_VAL) {
        diagnostic_node (A68_WARNING, p, WARNING_OVERFLOW, MODE (REAL));
        undent (out, "DBL_MAX");
      } else if (errno == ERANGE && conv == -HUGE_VAL) {
        diagnostic_node (A68_WARNING, p, WARNING_OVERFLOW, MODE (REAL));
        undent (out, "(-DBL_MAX)");
      } else if (errno == ERANGE && conv >= 0) {
        diagnostic_node (A68_WARNING, p, WARNING_UNDERFLOW, MODE (REAL));
        undent (out, "DBL_MIN");
      } else if (errno == ERANGE && conv < 0) {
        diagnostic_node (A68_WARNING, p, WARNING_UNDERFLOW, MODE (REAL));
        undent (out, "(-DBL_MIN)");
      } else {
        if (strchr (line, '.') == NULL && strchr (line, 'e') == NULL && strchr (line, 'E') == NULL) {
          strncat (line, ".0", BUFFER_SIZE);
        }
        undent (out, line);
      }
      ABEND (stack_pointer > 0, "stack not empty", NULL);
    } else if (MOID (p) == MODE (BOOL)) {
      A68_BOOL b;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &b, A68_BOOL);
      ASSERT (snprintf (line, BUFFER_SIZE, "%s", (VALUE (&b) ? "A68_TRUE" : "A68_FALSE")) >= 0);
      undent (out, line);
      ABEND (stack_pointer > 0, "stack not empty", NULL);
    } else if (MOID (p) == MODE (CHAR)) {
      A68_CHAR c;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &c, A68_CHAR);
      if (VALUE (&c) == '\'') {
        undentf (out, snprintf (line, BUFFER_SIZE, "'\\\''"));
      } else if (VALUE (&c) == '\\') { 
        undentf (out, snprintf (line, BUFFER_SIZE, "'\\\\'"));
      } else if (VALUE (&c) == NULL_CHAR) {
        undentf (out, snprintf (line, BUFFER_SIZE, "NULL_CHAR"));
      } else if (IS_PRINT (VALUE (&c))) {
        undentf (out, snprintf (line, BUFFER_SIZE, "'%c'", VALUE (&c)));
      } else {
        undentf (out, snprintf (line, BUFFER_SIZE, "(int) 0x%04x", (unsigned) VALUE (&c)));
      }
      ABEND (stack_pointer > 0, "stack not empty", NULL);
    } else if (MOID (p) == MODE (BITS)) {
      A68_BITS b;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &b, A68_BITS);
      ASSERT (snprintf (line, BUFFER_SIZE, "0x%x", VALUE (&b)) >= 0);
      undent (out, line);
      ABEND (stack_pointer > 0, "stack not empty", NULL);
    } else if (MOID (p) == MODE (COMPLEX)) {
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      undentf (out, snprintf (line, BUFFER_SIZE, "(A68_REAL *) %s", acc)); 
    }
  }
}

/********************************/
/* COMPILATION OF PARTIAL UNITS */
/********************************/

/*!
\brief code function prelude
\param out output file descriptor
\param p position in tree
\param fn function name
**/

static void write_fun_prelude (NODE_T * p, FILE_T out, char * fn)
{
  (void) p;
  indentf (out, snprintf (line, BUFFER_SIZE, "CODE (%s)\n", fn));
  indentation ++;
  temp_book_pointer = 0;
}

/*!
\brief code function postlude
\param out output file descriptor
\param p position in tree
\param fn function name
**/

static void write_fun_postlude (NODE_T * p, FILE_T out, char * fn)
{
  (void) p;
  indentation --;
  indentf (out, snprintf (line, BUFFER_SIZE, "EDOC (%s, N(%d))\n\n", fn, NUMBER (p)));
  temp_book_pointer = 0;
}

/*!
\brief code an A68 mode
\param m mode to code
\return internal identifier for mode
**/

static char * inline_mode (MOID_T * m)
{
  if (m == MODE (INT)) {
    return ("A68_INT");
  } else if (m == MODE (REAL)) {
    return ("A68_REAL");
  } else if (m == MODE (BOOL)) {
    return ("A68_BOOL");
  } else if (m == MODE (CHAR)) {
    return ("A68_CHAR");
  } else if (m == MODE (BITS)) {
    return ("A68_BITS");
  } else if (WHETHER (m, REF_SYMBOL)) {
    return ("A68_REF");
  } else if (WHETHER (m, ROW_SYMBOL)) {
    return ("A68_ROW");
  } else if (WHETHER (m, PROC_SYMBOL)) {
    return ("A68_PROCEDURE");
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
    return ("A68_STRUCT");
  } else {
    return ("A68_ERROR");
  }
}

/*!
\brief code denotation
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_denotation (NODE_T * p, FILE_T out, int phase)
{
  if (phase != L_YIELD) {
    return;
  } else if (MOID (p) == MODE (INT)) {
    A68_INT z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, MODE (INT), SYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (INT));
    }
    undentf(out, snprintf (line, BUFFER_SIZE, "%d", VALUE (&z)));
  } else if (MOID (p) == MODE (REAL)) {
    if (strchr (SYMBOL (p), '.') == NULL && strchr (SYMBOL (p), 'e') == NULL && strchr (SYMBOL (p), 'E') == NULL) {
      undentf(out, snprintf (line, BUFFER_SIZE, "(double) %s", SYMBOL (p)));
    } else {
      undentf(out, snprintf (line, BUFFER_SIZE, "%s", SYMBOL (p)));
    }  
  } else if (MOID (p) == MODE (BOOL)) {
    undent (out, "(BOOL_T) A68_");
    undent (out, SYMBOL (p));
  } else if (MOID (p) == MODE (CHAR)) {
    if (SYMBOL (p)[0] == '\'') {
      undentf (out, snprintf (line, BUFFER_SIZE, "'\\''"));
    } else if (SYMBOL (p)[0] == NULL_CHAR) {
      undentf (out, snprintf (line, BUFFER_SIZE, "NULL_CHAR"));
    } else if (SYMBOL (p)[0] == '\\') { 
      undentf (out, snprintf (line, BUFFER_SIZE, "'\\\\'"));
    } else {
      undentf (out, snprintf (line, BUFFER_SIZE, "'%c'", (SYMBOL (p))[0]));
    }
  } else if (MOID (p) == MODE (BITS)) {
    A68_BITS z;
    NODE_T *s = WHETHER (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
    if (genie_string_to_value_internal (p, MODE (BITS), SYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (BITS));
    }
    ASSERT (snprintf (line, BUFFER_SIZE, "(unsigned) 0x%x", VALUE (&z)) >= 0);
    undent (out, line);
  }
}

/*!
\brief code widening
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_widening (NODE_T * p, FILE_T out, int phase)
{
  if (MOID (p) == MODE (REAL)) {
    if (phase == L_DECLARE) {
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      undent (out, "(double) ("); 
      inline_unit (SUB (p), out, L_YIELD);
      undent (out, ")");
    }
  } else if (MOID (p) == MODE (COMPLEX)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      add_declaration (&root_idf, "A68_COMPLEX", 0, acc);
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
      indentf (out, snprintf (line, BUFFER_SIZE, "STATUS_RE (%s) = INITIALISED_MASK;\n", acc)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "STATUS_IM (%s) = INITIALISED_MASK;\n", acc)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "RE (%s) = (double) (", acc)); 
      inline_unit (SUB (p), out, L_YIELD);
      undent (out, ");\n");
      indentf (out, snprintf (line, BUFFER_SIZE, "IM (%s) = 0.0;\n", acc)); 
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, BUFFER_SIZE, "(A68_REAL *) %s", acc)); 
    }
  }
}

/*!
\brief code dereferencing of identifier
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_dereference_identifier (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * q = locate (SUB (p), IDENTIFIER); 
  ABEND (q == NULL, "not dereferencing an identifier", NULL);
  if (phase == L_DECLARE) {
    if (temp_booked (BOOK_DEREF, L_DECLARE, SYMBOL (q)) != NOT_BOOKED) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (q), "", NUMBER (p));
      add_declaration (&root_idf, inline_mode (MOID (p)), 1, idf); 
      book_temp (BOOK_DEREF, L_DECLARE, SYMBOL (p), NULL, NUMBER (p));
      inline_unit (SUB (p), out, L_DECLARE);
    }
  } else if (phase == L_EXECUTE) {
    if (temp_booked (BOOK_DEREF, L_EXECUTE, SYMBOL (q)) != NOT_BOOKED) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (q), "", NUMBER (p));
      inline_unit (SUB (p), out, L_EXECUTE);
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS (", idf, inline_mode (MOID (p)))); 
      book_temp (BOOK_DEREF, L_EXECUTE, SYMBOL (p), NULL, NUMBER (p));
      inline_unit (SUB (p), out, L_YIELD);
      undent (out, ");\n");
    }
  } else if (phase == L_YIELD) {
    char idf[NAME_SIZE];
    if (temp_booked (BOOK_DEREF, L_EXECUTE, SYMBOL (q)) != NOT_BOOKED) {
      (void) make_name (idf, SYMBOL (q), "", temp_booked (BOOK_DEREF, L_DECLARE, SYMBOL (q))->number);
    } else {
      (void) make_name (idf, SYMBOL (q), "", NUMBER (p));
    }
    if (primitive_mode (MOID (p))) {
      undentf (out, snprintf (line, BUFFER_SIZE, "V (%s)", idf));
    } else if (MOID (p) == MODE (COMPLEX)) {
      undentf (out, snprintf (line, BUFFER_SIZE, "(A68_REAL *) (%s)", idf));
    } else if (basic_mode (MOID (p))) {
      undent (out, idf); 
    } 
  }
}

/*!
\brief code identifier
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_identifier (NODE_T * p, FILE_T out, int phase)
{
/* Possible constant folding */
  NODE_T * def = NODE (TAX (p));
  if (primitive_mode (MOID (p)) && def != NULL && NEXT (def) != NULL && WHETHER (NEXT (def), EQUALS_SYMBOL)) {
    NODE_T * src = locate (NEXT_NEXT (def), DENOTATION);
    if (src != NULL) {
      inline_denotation (src, out, phase);
      return;
    }
  }
/* No folding - consider identifier */
  if (phase == L_DECLARE) {
    if (temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (p)) != NOT_BOOKED) {
      return; 
    } else if (TAX(p)->stand_env_proc) {
      return; 
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (p), "", NUMBER (p));
      add_declaration (&root_idf, inline_mode (MOID (p)), 1, idf);
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (p), NULL, NUMBER (p)); 
    }
  } else if (phase == L_EXECUTE) {
    if (temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (p)) != NOT_BOOKED) {
      return; 
    } else if (TAX(p)->stand_env_proc) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (p), "", NUMBER (p));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, %s, N(%d));\n", idf, inline_mode (MOID (p)), NUMBER (p))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (p), NULL, NUMBER (p)); 
    }
  } else if (phase == L_YIELD) {
    if (TAX (p)->stand_env_proc) {
      int k;
      for (k = 0; k < CONSTANTS; k ++) {
        if (TAX (p)->procedure == constants[k].procedure) {
          undent (out, constants[k].code);
          return;
        }
      }
    } else {
      char idf[NAME_SIZE];
      BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (p));
      if (entry != NOT_BOOKED) {
        (void) make_name (idf, SYMBOL (p), "", entry->number);
      } else {
        (void) make_name (idf, SYMBOL (p), "", NUMBER (p));
      }
      if (primitive_mode (MOID (p))) {
        undentf (out, snprintf (line, BUFFER_SIZE, "V (%s)", idf));
      } else if (MOID (p) == MODE (COMPLEX)) {
        undentf (out, snprintf (line, BUFFER_SIZE, "(A68_REAL *) (%s)", idf));
      } else if (basic_mode (MOID (p))) {
        undent (out, idf); 
      }
    } 
  }
}

/*!
\brief code indexer
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_indexer (NODE_T * p, FILE_T out, int phase, int * k, char * tup)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    if (phase != L_YIELD) {
      inline_unit (p, out, phase);
    } else {
      if ((*k) == 0) {
        undentf(out, snprintf (line, BUFFER_SIZE, "(%s[%d].span * (", tup, (*k)));
      } else {
        undentf(out, snprintf (line, BUFFER_SIZE, " + (%s[%d].span * (", tup, (*k)));
      }
      inline_unit (p, out, L_YIELD);
      undentf(out, snprintf (line, BUFFER_SIZE, ") - %s[%d].shift)", tup, (*k)));
    }
    (*k) ++;
  } else {
    inline_indexer (SUB (p), out, phase, k, tup);
    inline_indexer (NEXT (p), out, phase, k, tup);
  }
}

/*!
\brief code dereferencing of slice
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_dereference_slice (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * indx = NEXT (prim);
  MOID_T * mode = SUB_MOID (p);
  MOID_T * row_mode = DEFLEX (MOID (prim));
  char * symbol = SYMBOL (SUB (prim));
  char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE];
  int k;
  if (phase == L_DECLARE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NOT_BOOKED) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      add_declaration (&root_idf, "A68_REF", 1, idf);
      add_declaration (&root_idf, "A68_REF", 0, elm);
      add_declaration (&root_idf, "A68_ARRAY", 1, arr);
      add_declaration (&root_idf, "A68_TUPLE", 1, tup);
      add_declaration (&root_idf, inline_mode (mode), 1, drf);
      book_temp (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim)); 
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      add_declaration (&root_idf, "A68_REF", 0, elm);
      add_declaration (&root_idf, inline_mode (mode), 1, drf);
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NULL);
  } else if (phase == L_EXECUTE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, symbol);
    if (entry == NOT_BOOKED) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", idf, NUMBER (locate (prim, IDENTIFIER)))); 
      if (WHETHER (row_mode, REF_SYMBOL) && WHETHER (SUB (row_mode), ROW_SYMBOL)) {
        indentf (out, snprintf (line, BUFFER_SIZE, "GET_DESCRIPTOR (%s, %s, (A68_ROW *) ADDRESS (%s));\n", arr, tup, idf)); 
      } else {
        ABEND (A68_TRUE, "strange mode in dereference slice (execute)", NULL);
      }
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (arr, ARR, "", entry->number);
      (void) make_name (tup, TUP, "", entry->number);
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = ARRAY (%s);\n", elm, arr)); 
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NULL);
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf(out, snprintf (line, BUFFER_SIZE, ");\n"));     
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS(& %s);\n", drf, inline_mode (mode), elm)); 
  } else if (phase == L_YIELD) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NOT_BOOKED && same_tree (indx, (NODE_T *) (entry->info)) == A68_TRUE) {
      (void) make_name (drf, DRF, "", entry->number);
    } else {
      (void) make_name (drf, DRF, "", NUMBER (prim));
    }
    if (primitive_mode (mode)) {
      undentf (out, snprintf (line, BUFFER_SIZE, "V (%s)", drf));
    } else if (mode == MODE (COMPLEX)) {
      undentf (out, snprintf (line, BUFFER_SIZE, "(A68_REAL *) (%s)", drf));
    } else if (basic_mode (mode)) {
      undent (out, drf);
    } else {
      ABEND (A68_TRUE, "strange mode in dereference slice (yield)", NULL);
    }
  }
}

/*!
\brief code slice REF [] MODE -> REF MODE
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_slice_ref_to_ref (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * indx = NEXT (prim);
  MOID_T * mode = SUB_MOID (p);
  MOID_T * row_mode = DEFLEX (MOID (prim));
  char * symbol = SYMBOL (SUB (prim));
  char idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE], drf[NAME_SIZE];
  int k;
  if (phase == L_DECLARE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NOT_BOOKED) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      /*indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, inline_mode (mode), drf, arr, tup));*/
      add_declaration (&root_idf, "A68_REF", 1, idf);
      add_declaration (&root_idf, "A68_REF", 0, elm);
      add_declaration (&root_idf, "A68_ARRAY", 1, arr);
      add_declaration (&root_idf, "A68_TUPLE", 1, tup);
      add_declaration (&root_idf, inline_mode (mode), 1, drf);
      book_temp (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim)); 
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      /*indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF %s; %s * %s;\n", elm, inline_mode (mode), drf));*/
      add_declaration (&root_idf, "A68_REF", 0, elm);
      add_declaration (&root_idf, inline_mode (mode), 1, drf);
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NULL);
  } else if (phase == L_EXECUTE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, symbol);
    if (entry == NOT_BOOKED) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", idf, NUMBER (locate (prim, IDENTIFIER))));
      if (WHETHER (row_mode, REF_SYMBOL) && WHETHER (SUB (row_mode), ROW_SYMBOL)) {
        indentf (out, snprintf (line, BUFFER_SIZE, "GET_DESCRIPTOR (%s, %s, (A68_ROW *) ADDRESS (%s));\n", arr, tup, idf)); 
      } else {
        ABEND (A68_TRUE, "strange mode in slice (execute)", NULL);
      }
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (arr, ARR, "", entry->number);
      (void) make_name (tup, TUP, "", entry->number);
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = ARRAY (%s);\n", elm, arr)); 
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NULL);
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf(out, snprintf (line, BUFFER_SIZE, ");\n"));
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS(& %s);\n", drf, inline_mode (mode), elm)); 
  } else if (phase == L_YIELD) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NOT_BOOKED && same_tree (indx, (NODE_T *) (entry->info)) == A68_TRUE) {
      (void) make_name (elm, ELM, "", entry->number);
    } else {
      (void) make_name (elm, ELM, "", NUMBER (prim));
    }
    undentf (out, snprintf (line, BUFFER_SIZE, "(&%s)", elm));
  }
}

/*!
\brief code slice [] MODE -> MODE
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_slice (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * indx = NEXT (prim);
  MOID_T * mode = MOID (p);
  MOID_T * row_mode = DEFLEX (MOID (prim));
  char * symbol = SYMBOL (SUB (prim));
  char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE];
  int k;
  if (phase == L_DECLARE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NOT_BOOKED) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, inline_mode (mode), drf, arr, tup));
      book_temp (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim)); 
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF %s; %s * %s;\n", elm, inline_mode (mode), drf));
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NULL);
  } else if (phase == L_EXECUTE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, symbol);
    if (entry == NOT_BOOKED) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", idf, NUMBER (locate (prim, IDENTIFIER))));
      if (WHETHER (row_mode, REF_SYMBOL)) {
        indentf (out, snprintf (line, BUFFER_SIZE, "GET_DESCRIPTOR (%s, %s, (A68_ROW *) ADDRESS (%s));\n", arr, tup, idf)); 
      } else {
        indentf (out, snprintf (line, BUFFER_SIZE, "GET_DESCRIPTOR (%s, %s, (A68_ROW *) %s);\n", arr, tup, idf)); 
      }
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (arr, ARR, "", entry->number);
      (void) make_name (tup, TUP, "", entry->number);
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = ARRAY (%s);\n", elm, arr)); 
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NULL);
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf(out, snprintf (line, BUFFER_SIZE, ");\n"));
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS(& %s);\n", drf, inline_mode (mode), elm));
  } else if (phase == L_YIELD) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NOT_BOOKED && same_tree (indx, (NODE_T *) (entry->info)) == A68_TRUE) {
      (void) make_name (drf, DRF, "", entry->number);
    } else {
      (void) make_name (drf, DRF, "", NUMBER (prim));
    }
    if (primitive_mode (mode)) {
      undentf (out, snprintf (line, BUFFER_SIZE, "V (%s)", drf));
    } else if (mode == MODE (COMPLEX)) {
      undentf (out, snprintf (line, BUFFER_SIZE, "(A68_REAL *) (%s)", drf));
    } else if (basic_mode (mode)) {
      undentf (out, snprintf (line, BUFFER_SIZE, "%s", drf));
    } else {
      ABEND (A68_TRUE, "strange mode in slice (yield)", NULL);
    }
  }
}

/*!
\brief code monadic formula
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_monadic_formula (NODE_T * p, FILE_T out, int phase)
{
  if (WHETHER (p, MONADIC_FORMULA) && MOID (p) == MODE (COMPLEX)) {
    NODE_T *op = SUB (p);
    NODE_T * rhs = NEXT (op);
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      add_declaration (&root_idf, "A68_COMPLEX", 0, acc);
      inline_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_unit (rhs, out, L_EXECUTE);
      for (k = 0; k < MONADICS; k ++) {
        if (TAX (op)->procedure == monadics[k].procedure) {
          indentf (out, snprintf (line, BUFFER_SIZE, "%s (%s, ", monadics[k].code, acc));
          inline_unit (rhs, out, L_YIELD);
          undentf (out, snprintf (line, BUFFER_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, BUFFER_SIZE, "%s", acc));
    }
  } else if (WHETHER (p, MONADIC_FORMULA) && basic_mode (MOID (p))) {
    NODE_T *op = SUB (p);
    NODE_T * rhs = NEXT (op);
    if (phase != L_YIELD) {
      inline_unit (rhs, out, phase);
    } else {
      int k;
      for (k = 0; k < MONADICS; k ++) {
        if (TAX (op)->procedure == monadics[k].procedure) {
          if (IS_ALNUM ((monadics[k].code)[0])) {
            undent (out, monadics[k].code);
            undent (out, "(");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          } else {
            undent (out, monadics[k].code);
            undent (out, "(");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          }
        }
      }
    }
  }
}

/*!
\brief code dyadic formula
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_formula (NODE_T * p, FILE_T out, int phase)
{
  if (WHETHER (p, FORMULA) && MOID (p) == MODE (COMPLEX)) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NULL) {
      inline_monadic_formula (lhs, out, phase);
    } else if (phase == L_DECLARE) {
      NODE_T * rhs = NEXT (op);
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      add_declaration (&root_idf, "A68_COMPLEX", 0, acc);
      inline_unit (lhs, out, L_DECLARE);
      inline_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T * rhs = NEXT (op);
      char acc[NAME_SIZE];
      int k;
      (void) make_name (acc, TMP, "", NUMBER (p));
      inline_unit (lhs, out, L_EXECUTE);
      inline_unit (rhs, out, L_EXECUTE);
      for (k = 0; k < DYADICS; k ++) {
        if (TAX (op)->procedure == dyadics[k].procedure) {
          indentf (out, snprintf (line, BUFFER_SIZE, "%s (%s, ", dyadics[k].code, acc));
          inline_unit (lhs, out, L_YIELD);
          undentf (out, snprintf (line, BUFFER_SIZE, ", "));
          inline_unit (rhs, out, L_YIELD);
          undentf (out, snprintf (line, BUFFER_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      undentf (out, snprintf (line, BUFFER_SIZE, "%s", acc));
    }
  } else if (WHETHER (p, FORMULA) && basic_mode (MOID (p))) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NULL) {
      inline_monadic_formula (lhs, out, phase);
    } else if (phase != L_YIELD) {
      NODE_T * rhs = NEXT (op);
      inline_unit (lhs, out, phase);
      inline_unit (rhs, out, phase);
    } else {
      NODE_T * rhs = NEXT (op);
      int k;
      for (k = 0; k < DYADICS; k ++) {
        if (TAX (op)->procedure == dyadics[k].procedure) {
          if (IS_ALNUM ((dyadics[k].code)[0])) {
            undent (out, dyadics[k].code);
            undent (out, "(");
            inline_unit (lhs, out, L_YIELD);
            undent (out, ", ");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          } else {
            undent (out, "(");
            inline_unit (lhs, out, L_YIELD);
            undent (out, " ");
            undent (out, dyadics[k].code);
            undent (out, " ");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          }
        }
      }
    }
  }
}

/*!
\brief code argument
\param p starting node
\return same
**/

static void inline_single_argument (NODE_T * p, FILE_T out, int phase)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ARGUMENT_LIST) || WHETHER (p, ARGUMENT)) {
      inline_single_argument (SUB (p), out, phase);
    } else if (WHETHER (p, GENERIC_ARGUMENT_LIST) || WHETHER (p, GENERIC_ARGUMENT)) {
      inline_single_argument (SUB (p), out, phase);
    } else if (WHETHER (p, UNIT)) {
      inline_unit (p, out, phase);
    }
  }
}

/*!
\brief code call
\param p starting node
\return same
**/

static void inline_call (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * args = NEXT (prim);
  NODE_T * idf = locate (prim, IDENTIFIER);
  if (MOID (p) == MODE (COMPLEX)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      add_declaration (&root_idf, "A68_COMPLEX", 0, acc);
      inline_single_argument (args, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_single_argument (args, out, L_EXECUTE);
      for (k = 0; k < FUNCTIONS; k ++) {
        if (TAX (idf)->procedure == functions[k].procedure) {
          indentf (out, snprintf (line, BUFFER_SIZE, "%s (%s, ", functions[k].code, acc));
          inline_single_argument (args, out, L_YIELD);
          undentf (out, snprintf (line, BUFFER_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, BUFFER_SIZE, "%s", acc));
    }
  } else if (basic_mode (MOID (p))) {
    if (phase != L_YIELD) {
      inline_single_argument (args, out, phase);
    } else {
      int k;
      for (k = 0; k < FUNCTIONS; k ++) {
        if (TAX (idf)->procedure == functions[k].procedure) {
          undent (out, functions[k].code);
          undent (out, " (");
          inline_single_argument (args, out, L_YIELD);
          undent (out, ")");
        }
      }
    }
  }
}

/*!
\brief code collateral units
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static void inline_collateral_units (NODE_T * p, FILE_T out, int phase)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    if (phase == L_DECLARE) {
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
      inline_unit (SUB (p), out, L_YIELD);
      undentf(out, snprintf (line, BUFFER_SIZE, ", %s);\n", inline_mode (MOID (p))));
    }
  } else {
    inline_collateral_units (SUB (p), out, phase);
    inline_collateral_units (NEXT (p), out, phase);
  }
}

/*!
\brief code collateral units
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static void inline_collateral (NODE_T * p, FILE_T out, int phase)
{
  char dsp[NAME_SIZE];
  (void) make_name (dsp, DSP, "", NUMBER (p));
  if (p == NULL) {
    return;
  } else if (phase == L_DECLARE) {
    if (MOID (p) == MODE (COMPLEX)) {
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_COMPLEX * %s = (A68_COMPLEX *) STACK_TOP;\n", dsp));
    } else {
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_STRUCT * %s = (A68_STRUCT *) STACK_TOP;\n", dsp));
    }
    inline_collateral_units (NEXT_SUB (p), out, L_DECLARE);    
  } else if (phase == L_EXECUTE) {
    inline_collateral_units (NEXT_SUB (p), out, L_EXECUTE);    
    inline_collateral_units (NEXT_SUB (p), out, L_YIELD);    
  } else if (phase == L_YIELD) {
    undentf (out, snprintf (line, BUFFER_SIZE, "%s", dsp));
  }
}

/*!
\brief code basic closed clause
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_closed (NODE_T * p, FILE_T out, int phase)
{
  if (p == NULL) {
    return;
  } else if (phase != L_YIELD) {
    inline_unit (SUB (NEXT_SUB (p)), out, phase);
  } else {
    undent (out, "(");
    inline_unit (SUB (NEXT_SUB (p)), out, L_YIELD);
    undent (out, ")");
  }
}

/*!
\brief code basic closed clause
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_conditional (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * if_part = NULL, * then_part = NULL, * else_part = NULL;
  p = SUB (p);
  if (WHETHER (p, IF_PART) || WHETHER (p, OPEN_PART)) {
    if_part = p;
  } else {
    ABEND (A68_TRUE, "if-part expected", NULL);
  }
  FORWARD (p);
  if (WHETHER (p, THEN_PART) || WHETHER (p, CHOICE)) {
    then_part = p;
  } else {
    ABEND (A68_TRUE, "then-part expected", NULL);
  }
  FORWARD (p);
  if (WHETHER (p, ELSE_PART) || WHETHER (p, CHOICE)) {
    else_part = p;
  } else {
    else_part = NULL;
  }
  if (phase == L_DECLARE) {
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_DECLARE);
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_DECLARE);
    inline_unit (SUB (NEXT_SUB (else_part)), out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_EXECUTE);
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_EXECUTE);
    inline_unit (SUB (NEXT_SUB (else_part)), out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    undent (out, "(");
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_YIELD);
    undent (out, " ? ");
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_YIELD);
    undent (out, " : ");
    if (else_part != NULL) {
      inline_unit (SUB (NEXT_SUB (else_part)), out, L_YIELD);
    } else {
/* 
This is not an ideal solution although RR permits it;
an omitted else-part means SKIP: yield some value of the 
mode required. 
*/
      inline_unit (SUB (NEXT_SUB (then_part)), out, L_YIELD);
    }
    undent (out, ")");
  }
}

/*!
\brief code dereferencing of selection
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_dereference_selection (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * field = SUB (p);
  NODE_T * sec = NEXT (field);
  NODE_T * idf = locate (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char * field_idf = SYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (idf));
    if (entry == NOT_BOOKED) {
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      /* indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s;\n", ref)); */
      add_declaration (&root_idf, "A68_REF", 1, ref);
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), NULL, NUMBER (field)); 
    }
    if (entry == NOT_BOOKED || field_idf != (char *) (entry->info)) { 
      (void) make_name (sel, SEL, "", NUMBER (field));
      /* indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", inline_mode (SUB_MOID (field)), sel)); */
      add_declaration (&root_idf, inline_mode (SUB_MOID (field)), 1, sel);
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (idf));
    if (entry == NOT_BOOKED) {
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", ref, NUMBER (locate (sec, IDENTIFIER)))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), NULL, NUMBER (field)); 
    } 
    if (entry == NOT_BOOKED) {
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    } else if (field_idf != (char *) (entry->info)) { 
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (idf));
    if (entry != NOT_BOOKED && (char *) (entry->info) == field_idf) {
      (void) make_name (sel, SEL, "", entry->number);
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (SUB_MOID (p))) {
      undentf (out, snprintf (line, BUFFER_SIZE, "V (%s)", sel));
    } else if (SUB_MOID (p) == MODE (COMPLEX)) {
      undentf (out, snprintf (line, BUFFER_SIZE, "(A68_REAL *) (%s)", sel));
    } else if (basic_mode (SUB_MOID (p))) {
      undent (out, sel); 
    } else {
      ABEND (A68_TRUE, "strange mode in dereference selection (yield)", NULL);
    }
  }
}

/*!
\brief code selection
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_selection (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * field = SUB (p);
  NODE_T * sec = NEXT (field);
  NODE_T * idf = locate (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char * field_idf = SYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (idf));
    if (entry == NOT_BOOKED) {
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      /* indentf (out, snprintf (line, BUFFER_SIZE, "A68_STRUCT %s;\n", ref)); */
      add_declaration (&root_idf, "A68_STRUCT", 0, ref);
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), NULL, NUMBER (field)); 
    }
    if (entry == NOT_BOOKED || field_idf != (char *) (entry->info)) { 
      (void) make_name (sel, SEL, "", NUMBER (field));
      /* indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", inline_mode (MOID (field)), sel)); */
      add_declaration (&root_idf, inline_mode (MOID (field)), 1, sel);
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (idf));
    if (entry == NOT_BOOKED) {
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, BYTE_T, N(%d));\n", ref, NUMBER (locate (sec, IDENTIFIER)))); 
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (%s[%d]);\n", sel, inline_mode (MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    } else if (field_idf != (char *) (entry->info)) { 
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (%s[%d]);\n", sel, inline_mode (MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (idf));
    if (entry != NOT_BOOKED && (char *) (entry->info) == field_idf) {
      (void) make_name (sel, SEL, "", entry->number);
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (MOID (p))) {
      undentf (out, snprintf (line, BUFFER_SIZE, "V (%s)", sel));
    } else {
      ABEND (A68_TRUE, "strange mode in selection (yield)", NULL);
    }
  }
}

/*!
\brief code selection
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_selection_ref_to_ref (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * field = SUB (p);
  NODE_T * sec = NEXT (field);
  NODE_T * idf = locate (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char * field_idf = SYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (idf));
    if (entry == NOT_BOOKED) {
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      /* indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s;\n", ref)); */
      add_declaration (&root_idf, "A68_REF", 1, ref);
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), NULL, NUMBER (field)); 
    }
    if (entry == NOT_BOOKED || field_idf != (char *) (entry->info)) { 
      (void) make_name (sel, SEL, "", NUMBER (field));
      /* indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF %s;\n", sel)); */
      add_declaration (&root_idf, "A68_REF", 0, sel);
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE_2, SYMBOL (idf));
    if (entry == NOT_BOOKED) {
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", ref, NUMBER (locate (sec, IDENTIFIER)))); 
      (void) make_name (sel, SEL, "", NUMBER (field));
      book_temp (BOOK_DECL, L_EXECUTE_2, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    } else if (field_idf != (char *) (entry->info)) { 
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      book_temp (BOOK_DECL, L_EXECUTE_2, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = *%s;\n", sel, ref)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (&%s) += %d;\n", sel, OFFSET_OFF (field))); 
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (idf));
    if (entry != NOT_BOOKED && (char *) (entry->info) == field_idf) {
      (void) make_name (sel, SEL, "", entry->number);
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (SUB_MOID (p))) {
      undentf (out, snprintf (line, BUFFER_SIZE, "(&%s)", sel));
    } else {
      ABEND (A68_TRUE, "strange mode in selection (yield)", NULL);
    }
  }
}

/*!
\brief code identifier
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_ref_identifier (NODE_T * p, FILE_T out, int phase)
{
/* No folding - consider identifier */
  if (phase == L_DECLARE) {
    if (temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (p)) != NOT_BOOKED) {
      return; 
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (p), "", NUMBER (p));
      /* indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s;\n", idf)); */
      add_declaration (&root_idf, "A68_REF", 1, idf);
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (p), NULL, NUMBER (p)); 
    }
  } else if (phase == L_EXECUTE) {
    if (temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (p)) != NOT_BOOKED) {
      return; 
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (p), "", NUMBER (p));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", idf, NUMBER (p))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (p), NULL, NUMBER (p)); 
    }
  } else if (phase == L_YIELD) {
    char idf[NAME_SIZE];
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (p));
    if (entry != NOT_BOOKED) {
      (void) make_name (idf, SYMBOL (p), "", entry->number);
    } else {
      (void) make_name (idf, SYMBOL (p), "", NUMBER (p));
    }
    undent (out, idf); 
  }
}

/*!
\brief code identity-relation
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static void inline_identity_relation (NODE_T * p, FILE_T out, int phase)
{
#define GOOD(p) (locate (p, IDENTIFIER) != NULL && WHETHER (MOID (locate ((p), IDENTIFIER)), REF_SYMBOL))
  NODE_T * lhs = SUB (p);
  NODE_T * op = NEXT (lhs);
  NODE_T * rhs = NEXT (op);
  if (GOOD (lhs) && GOOD (rhs)) {
    if (phase == L_DECLARE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_DECLARE);
      inline_ref_identifier (ridf, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_EXECUTE);
      inline_ref_identifier (ridf, out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      if (WHETHER (op, IS_SYMBOL)) {
        undentf (out, snprintf (line, BUFFER_SIZE, "ADDRESS (")); 
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ") == ADDRESS ("));  
        inline_ref_identifier (ridf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ")")); 
      } else {
        undentf (out, snprintf (line, BUFFER_SIZE, "ADDRESS (")); 
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ") != ADDRESS (")); 
        inline_ref_identifier (ridf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ")")); 
      }
    }
  } else if (GOOD (lhs) && locate (rhs, NIHIL) != NULL) {
    if (phase == L_DECLARE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      if (WHETHER (op, IS_SYMBOL)) {
        indentf (out, snprintf (line, BUFFER_SIZE, "IS_NIL (*")); 
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ")")); 
      } else {
        indentf (out, snprintf (line, BUFFER_SIZE, "!IS_NIL (*")); 
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ")"));
      } 
    }
  }
#undef GOOD
}

/*!
\brief code unit
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void inline_unit (NODE_T * p, FILE_T out, int phase)
{
  if (p == NULL) {
    return;
  } else if (constant_unit (p) && locate (p, DENOTATION) == NULL) {
    constant_folder (p, out, phase);
  } else if (WHETHER (p, UNIT)) {
    inline_unit (SUB (p), out, phase);
  } else if (WHETHER (p, TERTIARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (WHETHER (p, SECONDARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (WHETHER (p, PRIMARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    inline_unit (SUB (p), out, phase);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    inline_closed (p, out, phase);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    inline_collateral (p, out, phase);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    inline_conditional (p, out, phase);
  } else if (WHETHER (p, WIDENING)) {
    inline_widening (p, out, phase);
  } else if (WHETHER (p, IDENTIFIER)) {
    inline_identifier (p, out, phase);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), IDENTIFIER) != NULL) {
    inline_dereference_identifier (p, out, phase);
  } else if (WHETHER (p, SLICE)) {
    NODE_T * prim = SUB (p);
    MOID_T * mode = MOID (p);
    MOID_T * row_mode = DEFLEX (MOID (prim));
    if (mode == SUB (row_mode)) {
      inline_slice (p, out, phase);
    } else if (WHETHER (mode, REF_SYMBOL) && WHETHER (row_mode, REF_SYMBOL) && SUB (mode) == SUB_SUB (row_mode)) {
      inline_slice_ref_to_ref (p, out, phase);
    } else {
      ABEND (A68_TRUE, "strange mode for slice", NULL);
    }
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SLICE) != NULL) {
    inline_dereference_slice (SUB (p), out, phase);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SELECTION) != NULL) {
    inline_dereference_selection (SUB (p), out, phase);
  } else if (WHETHER (p, SELECTION)) {
    NODE_T * sec = NEXT_SUB (p);
    MOID_T * mode = MOID (p);
    MOID_T * struct_mode = MOID (sec);
    if (WHETHER (struct_mode, REF_SYMBOL) && WHETHER (mode, REF_SYMBOL)) {
      inline_selection_ref_to_ref (p, out, phase);
    } else if (WHETHER (struct_mode, STRUCT_SYMBOL) && primitive_mode (mode)) {
      inline_selection (p, out, phase);
    } else {
      ABEND (A68_TRUE, "strange mode for selection", NULL);
    }
  } else if (WHETHER (p, DENOTATION)) {
    inline_denotation (p, out, phase);
  } else if (WHETHER (p, MONADIC_FORMULA)) {
    inline_monadic_formula (p, out, phase);
  } else if (WHETHER (p, FORMULA)) {
    inline_formula (p, out, phase);
  } else if (WHETHER (p, CALL)) {
    inline_call (p, out, phase);
  } else if (WHETHER (p, CAST)) {
    inline_unit (NEXT_SUB (p), out, phase);
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    inline_identity_relation (p, out, phase);
  }
}

/*********************************/
/* COMPILATION OF COMPLETE UNITS */
/*********************************/

/*!
\brief compile push
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static void compile_push (NODE_T * p, FILE_T out) {
  if (primitive_mode (MOID (p))) {
    indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, BUFFER_SIZE, ", %s);\n", inline_mode (MOID (p)))); 
  } else if (basic_mode (MOID (p))) {
    indentf (out, snprintf (line, BUFFER_SIZE, "MOVE ((void *) STACK_TOP, (void *) ")); 
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, BUFFER_SIZE, ", %d);\n", MOID_SIZE (MOID (p)))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer += %d;\n", MOID_SIZE (MOID (p)))); 
  } else {
    ABEND (A68_TRUE, "cannot push", moid_to_string (MOID (p), 80, NULL));
  }
}

/*!
\brief compile assign (C source to C destination)
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static void compile_assign (NODE_T * p, FILE_T out, char * dst) {
  if (primitive_mode (MOID (p))) {
    indentf (out, snprintf (line, BUFFER_SIZE, "S (%s) = INITIALISED_MASK;\n", dst)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "V (%s) = ", dst)); 
    inline_unit (p, out, L_YIELD);
    undent (out, ";\n");
  } else if (basic_mode (MOID (p))) {
    indentf (out, snprintf (line, BUFFER_SIZE, "MOVE ((void *) %s, (void *) ", dst)); 
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, BUFFER_SIZE, ", %d);\n", MOID_SIZE (MOID (p)))); 
  } else {
    ABEND (A68_TRUE, "cannot assign", moid_to_string (MOID (p), 80, NULL));
  }
}

/*!
\brief compile denotation
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_denotation (NODE_T * p, FILE_T out, int compose_fun)
{
/*
We keep track of compiled denotations since one program can have several
same denotations, typically 0 or 1.
*/
  static char fn[NAME_SIZE];
  if (primitive_mode (MOID (p)) && compose_fun == A68_MAKE_FUNCTION) {
    BOOK * entry = perm_booked (BOOK_COMPILE, L_NONE, SYMBOL (p), MOID (p));
    if (entry != NOT_BOOKED) {
      (void) make_name (fn, "_denotation", "", NUMBER (entry));      
      return (fn);
    } else {
      (void) make_name (fn, "_denotation", "", NUMBER (p));
      comment_source (p, out);
      write_fun_prelude (p, out, fn);
      root_idf = NULL;
      inline_unit (p, out, L_DECLARE);
      print_declarations (out, root_idf);
      inline_unit (p, out, L_EXECUTE);
      indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (line, BUFFER_SIZE, ", %s);\n", inline_mode (MOID (p)))); 
      write_fun_postlude (p, out, fn);
      book_perm (BOOK_COMPILE, L_NONE, SYMBOL (p), (void *) MOID (p), NUMBER (p)); 
      return (fn);
    }
  } else if (primitive_mode (MOID (p))) {
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, BUFFER_SIZE, ", %s);\n", inline_mode (MOID (p)))); 
    return (NULL);
  } else {
    return (NULL);
  }
}

/*!
\brief compile cast
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_cast (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_cast", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_unit (NEXT_SUB (p), out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (NEXT_SUB (p), out, L_EXECUTE);
    compile_push (NEXT_SUB (p), out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile identifier
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_identifier", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile identifier
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_dereference_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_dereference_identifier", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile slice
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_slice", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile slice
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_dereference_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_deref_slice", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile selection
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_selection", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile selection
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_dereference_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_deref_selection", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile formula
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_formula (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_formula", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile voiding formula
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_voiding_formula (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_void_formula", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    root_idf = NULL;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    indent (out, "(void) (");
    inline_unit (p, out, L_YIELD);
    undent (out, ");\n");
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile call
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static void inline_arguments (NODE_T * p, FILE_T out, int phase, int * size)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT) && phase == L_PUSH) {
    indentf (out, snprintf (line, BUFFER_SIZE, "EXECUTE_UNIT (N(%d));\n", NUMBER (p)));
    inline_arguments (NEXT (p), out, L_PUSH, size);
  } else if (WHETHER (p, UNIT)) {
    char arg[NAME_SIZE];
    (void) make_name (arg, ARG, "", NUMBER (p));
    if (phase == L_DECLARE) {
      add_declaration (&root_idf, inline_mode (MOID (p)), 1, arg); 
      inline_unit (p, out, L_DECLARE);
    } else if (phase == L_INITIALISE) {
      inline_unit (p, out, L_EXECUTE);
    } else if (phase == L_EXECUTE) {
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) FRAME_OBJECT (%d);\n", arg, inline_mode (MOID (p)), *size)); 
      (*size) += MOID_SIZE (MOID (p));
    } else if (phase == L_YIELD && primitive_mode (MOID (p))) {
      indentf (out, snprintf (line, BUFFER_SIZE, "S (%s) = INITIALISED_MASK;\n", arg)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "V (%s) = ", arg)); 
      inline_unit (p, out, L_YIELD);
      undent (out, ";\n");
    } else if (phase == L_YIELD && basic_mode (MOID (p))) {
      indentf (out, snprintf (line, BUFFER_SIZE, "MOVE ((void *) %s, (void *) ", arg)); 
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (line, BUFFER_SIZE, ", %d);\n", MOID_SIZE (MOID (p)))); 
    }
  } else {
    inline_arguments (SUB (p), out, phase, size);
    inline_arguments (NEXT (p), out, phase, size);
  }
}

/*!
\brief compile deproceduring
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_deproceduring (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * idf = locate (SUB (p), IDENTIFIER);
  if (idf == NULL) {
    return (NULL);
  } else if (! (SUB_MOID (idf) == MODE (VOID) || basic_mode (SUB_MOID (idf)))) {
    return (NULL);
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return (NULL);
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE];
/* Declare */
    root_idf = NULL;
    (void) make_name (fun, FUN, "", NUMBER (idf));
    (void) make_name (fn, "_deproc", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "A68_PROCEDURE * %s;\n", fun));
    indentf (out, snprintf (line, BUFFER_SIZE, "NODE_T * body;\n"));
    print_declarations (out, root_idf);
/* Initialise */
    if (compose_fun != A68_MAKE_NOTHING) {
      indent (out, "UP_SWEEP_SEMA;\n");
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_PROCEDURE, N(%d));\n", fun, NUMBER (idf))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "body = SUB (BODY (%s).node);\n", fun)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (line, BUFFER_SIZE, "INIT_STATIC_FRAME (body);\n"));
/* Execute procedure */
    indent (out, "EXECUTE_UNIT (NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, BUFFER_SIZE, "change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    if (NEEDS_SWEEP (SUB_MOID (idf))) {
      indentf (out, snprintf (line, BUFFER_SIZE, "PROTECT_FROM_SWEEP_STACK (N(%d));\n", NUMBER (p))); 
    }
    if (compose_fun == A68_MAKE_FUNCTION) {
      indent (out, "DOWN_SWEEP_SEMA;\n");
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  }
}

/*!
\brief compile deproceduring
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_voiding_deproceduring (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * idf = locate (SUB_SUB (p), IDENTIFIER);
  if (idf == NULL) {
    return (NULL);
  } else if (! (SUB_MOID (idf) == MODE (VOID) || basic_mode (SUB_MOID (idf)))) {
    return (NULL);
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return (NULL);
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE];
/* Declare */
    root_idf = NULL;
    (void) make_name (fun, FUN, "", NUMBER (idf));
    (void) make_name (fn, "_void_deproc", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    indentf (out, snprintf (line, BUFFER_SIZE, "A68_PROCEDURE * %s;\n", fun));
    indentf (out, snprintf (line, BUFFER_SIZE, "NODE_T * body;\n"));
    print_declarations (out, root_idf);
/* Initialise */
    if (compose_fun != A68_MAKE_NOTHING) {
      indent (out, "UP_SWEEP_SEMA;\n");
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_PROCEDURE, N(%d));\n", fun, NUMBER (idf))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "body = SUB (BODY (%s).node);\n", fun)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (line, BUFFER_SIZE, "INIT_STATIC_FRAME (body);\n"));
/* Execute procedure */
    indent (out, "EXECUTE_UNIT (NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, BUFFER_SIZE, "change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    indent (out, "CLOSE_FRAME;\n");
    if (compose_fun == A68_MAKE_FUNCTION) {
      indent (out, "DOWN_SWEEP_SEMA;\n");
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  }
}

/*!
\brief compile call
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_call (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * proc = SUB (p);
  NODE_T * args = NEXT (proc);
  NODE_T * idf = locate (proc, IDENTIFIER);
  if (idf == NULL) {
    return (NULL);
  } else if (! (SUB_MOID (proc) == MODE (VOID) || basic_mode (SUB_MOID (proc)))) {
    return (NULL);
  } else if (DIM (MOID (proc)) == 0) {
    return (NULL);
  } else if (TAX (idf)->stand_env_proc) {
    if (basic_call (p)) {
      static char fn[NAME_SIZE];
      char fun[NAME_SIZE];
      (void) make_name (fun, FUN, "", NUMBER (proc));
      (void) make_name (fn, "_call", "", NUMBER (p));
      comment_source (p, out);
      if (compose_fun == A68_MAKE_FUNCTION) {
        write_fun_prelude (p, out, fn);
      }
      root_idf = NULL;
      inline_unit (p, out, L_DECLARE);
      print_declarations (out, root_idf);
      inline_unit (p, out, L_EXECUTE);
      compile_push (p, out);
      if (compose_fun == A68_MAKE_FUNCTION) {
        write_fun_postlude (p, out, fn);
      }
      return (fn);
    } else {
      return (NULL);
    }
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return (NULL);
  } else if (DIM (GENIE (proc)->partial_proc) != 0) {
    return (NULL);
  } else if (!basic_argument (args)) {
    return (NULL);
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE];
    int size;
/* Declare */
    (void) make_name (fun, FUN, "", NUMBER (proc));
    (void) make_name (fn, "_call", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    indentf (out, snprintf (line, BUFFER_SIZE, "A68_PROCEDURE * %s; NODE_T * body;\n", fun));
/* Compute arguments */
    size = 0;
    root_idf = NULL;
    inline_arguments (args, out, L_DECLARE, &size);
    print_declarations (out, root_idf);
/* Initialise */
    if (compose_fun != A68_MAKE_NOTHING) {
      indent (out, "UP_SWEEP_SEMA;\n");
    }
    inline_arguments (args, out, L_INITIALISE, &size);
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_PROCEDURE, N(%d));\n", fun, NUMBER (idf))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "body = SUB (BODY (%s).node);\n", fun)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (line, BUFFER_SIZE, "INIT_STATIC_FRAME (body);\n"));
    size = 0;
    inline_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    inline_arguments (args, out, L_YIELD, &size);
/* Execute procedure */
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    indent (out, "EXECUTE_UNIT (NEXT_NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, BUFFER_SIZE, "change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    if (NEEDS_SWEEP (SUB_MOID (proc))) {
      indentf (out, snprintf (line, BUFFER_SIZE, "PROTECT_FROM_SWEEP_STACK (N(%d));\n", NUMBER (p))); 
    }
    if (compose_fun == A68_MAKE_FUNCTION) {
      indent (out, "DOWN_SWEEP_SEMA;\n");
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  }
}

/*!
\brief compile call
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_voiding_call (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * proc = SUB (locate (p, CALL));
  NODE_T * args = NEXT (proc);
  NODE_T * idf = locate (proc, IDENTIFIER);
  if (idf == NULL) {
    return (NULL);
  } else if (! (SUB_MOID (proc) == MODE (VOID) || basic_mode (SUB_MOID (proc)))) {
    return (NULL);
  } else if (DIM (MOID (proc)) == 0) {
    return (NULL);
  } else if (TAX (idf)->stand_env_proc) {
    return (NULL);
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return (NULL);
  } else if (DIM (GENIE (proc)->partial_proc) != 0) {
    return (NULL);
  } else if (!basic_argument (args)) {
    return (NULL);
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE];
    int size;
/* Declare */
    (void) make_name (fun, FUN, "", NUMBER (proc));
    (void) make_name (fn, "_void_call", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    indentf (out, snprintf (line, BUFFER_SIZE, "A68_PROCEDURE * %s; NODE_T * body;\n", fun));
/* Compute arguments */
    size = 0;
    root_idf = NULL;
    inline_arguments (args, out, L_DECLARE, &size);
    print_declarations (out, root_idf);
/* Initialise */
    if (compose_fun != A68_MAKE_NOTHING) {
      indent (out, "UP_SWEEP_SEMA;\n");
    }
    inline_arguments (args, out, L_INITIALISE, &size);
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_PROCEDURE, N(%d));\n", fun, NUMBER (idf))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "body = SUB (BODY (%s).node);\n", fun)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (line, BUFFER_SIZE, "INIT_STATIC_FRAME (body);\n"));
    size = 0;
    inline_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    inline_arguments (args, out, L_YIELD, &size);
/* Execute procedure */
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    indent (out, "EXECUTE_UNIT (NEXT_NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, BUFFER_SIZE, "change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (compose_fun == A68_MAKE_FUNCTION) {
      indent (out, "DOWN_SWEEP_SEMA;\n");
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  }
}

/*!
\brief compile voiding assignation
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

char * compile_voiding_assignation_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * dst = SUB (locate (p, ASSIGNATION));
  NODE_T * src = NEXT_NEXT (dst);
  if (BASIC (dst, SELECTION) && basic_unit (src) && basic_mode_non_row (MOID (dst))) {
    NODE_T * field = SUB (locate (dst, SELECTION));
    NODE_T * sec = NEXT (field);
    NODE_T * idf = locate (sec, IDENTIFIER);
    char sel[NAME_SIZE], ref[NAME_SIZE];
    char * field_idf = SYMBOL (SUB (field));
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_void_assign", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
/* Declare */
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    root_idf = NULL;
    if (temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (idf)) == NOT_BOOKED) { 
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s; /* %s */\n", ref, SYMBOL (idf)));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", inline_mode (SUB_MOID (field)), sel));
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else {
      int n = NUMBER (temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (idf)));
      (void) make_name (ref, SYMBOL (idf), "", n);
      (void) make_name (sel, SEL, "", n);
    } 
    inline_unit (src, out, L_DECLARE);
    print_declarations (out, root_idf);
/* Initialise */
    if (temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (idf)) == NOT_BOOKED) { 
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", ref, NUMBER (locate (sec, IDENTIFIER)))); 
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field));
    } 
    inline_unit (src, out, L_EXECUTE);
/* Generate */
    compile_assign (src, out, sel);
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile voiding assignation
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_voiding_assignation_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * dst = SUB (locate (p, ASSIGNATION));
  NODE_T * src = NEXT_NEXT (dst);
  NODE_T * slice = locate (SUB (dst), SLICE);
  NODE_T * prim = SUB (slice);
  MOID_T * mode = SUB_MOID (dst);
  MOID_T * row_mode = DEFLEX (MOID (prim));
  if (WHETHER (row_mode, REF_SYMBOL) && basic_slice (slice) && basic_unit (src) && basic_mode_non_row (MOID (src))) {
    NODE_T * indx = NEXT (prim);
    char * symbol = SYMBOL (SUB (prim));
    char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE];
    static char fn[NAME_SIZE];
    int k;
    comment_source (p, out);
    (void) make_name (fn, "_void_assign", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
/* Declare */
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    root_idf = NULL;
    if (temp_booked (BOOK_DECL, L_DECLARE, symbol) == NOT_BOOKED) { 
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      add_declaration (&root_idf, "A68_REF", 1, idf);
      add_declaration (&root_idf, "A68_REF", 0, elm);
      add_declaration (&root_idf, "A68_ARRAY", 1, arr);
      add_declaration (&root_idf, "A68_TUPLE", 1, tup);
      add_declaration (&root_idf, inline_mode (mode), 1, drf);
      book_temp (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim));
    } else {
      int n = NUMBER (temp_booked (BOOK_DECL, L_EXECUTE, symbol));
      (void) make_name (idf, symbol, "", n);
      (void) make_name (arr, ARR, "", n);
      (void) make_name (tup, TUP, "", n);
      (void) make_name (elm, ELM, "", n);
      (void) make_name (drf, DRF, "", n);
    } 
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NULL);
    inline_unit (src, out, L_DECLARE);
    print_declarations (out, root_idf);
/* Initialise */
    if (temp_booked (BOOK_DECL, L_EXECUTE, symbol) == NOT_BOOKED) { 
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", idf, NUMBER (locate (prim, IDENTIFIER)))); 
      indentf (out, snprintf (line, BUFFER_SIZE, "GET_DESCRIPTOR (%s, %s, (A68_ROW *) ADDRESS (%s));\n", arr, tup, idf)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = ARRAY (%s);\n", elm, arr)); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (p), (void *) indx, NUMBER (prim));
    }
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NULL);
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf(out, snprintf (line, BUFFER_SIZE, ");\n"));     
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS(& %s);\n", drf, inline_mode (mode), elm)); 
    inline_unit (src, out, L_EXECUTE);
/* Generate */
    compile_assign (src, out, drf);
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile voiding assignation
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_voiding_assignation_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * dst = SUB (locate (p, ASSIGNATION));
  NODE_T * src = NEXT_NEXT (dst);
  if (BASIC (dst, IDENTIFIER) && basic_unit (src) && basic_mode_non_row (MOID (src))) {
    static char fn[NAME_SIZE];
    char idf[NAME_SIZE];
    NODE_T * q = locate (dst, IDENTIFIER);
/* Declare */
    (void) make_name (fn, "_void_assign", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    root_idf = NULL;
    if (temp_booked (BOOK_DEREF, L_DECLARE, SYMBOL (q)) == NOT_BOOKED) {
      (void) make_name (idf, SYMBOL (q), "", NUMBER (p));
      add_declaration (&root_idf, inline_mode (SUB_MOID (dst)), 1, idf);
      book_temp (BOOK_DEREF, L_DECLARE, SYMBOL (q), NULL, NUMBER (p));
    } else {
      (void) make_name (idf, SYMBOL (q), "", NUMBER (temp_booked (BOOK_DEREF, L_DECLARE, SYMBOL (p))));
    }
    inline_unit (dst, out, L_DECLARE);
    inline_unit (src, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (dst, out, L_EXECUTE);
    if (temp_booked (BOOK_DEREF, L_EXECUTE, SYMBOL (q)) == NOT_BOOKED) {
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS (", idf, inline_mode (SUB_MOID (dst)))); 
      inline_unit (dst, out, L_YIELD);
      undent (out, ");\n"); 
      book_temp (BOOK_DEREF, L_EXECUTE, SYMBOL (q), NULL, NUMBER (p));
    }
    inline_unit (src, out, L_EXECUTE);
    compile_assign (src, out, idf);
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile identity-relation
\param p starting node
\param out output file descriptor
\return function name or NULL
**/

static char * compile_identity_relation (NODE_T * p, FILE_T out, int compose_fun)
{
#define GOOD(p) (locate (p, IDENTIFIER) != NULL && WHETHER (MOID (locate ((p), IDENTIFIER)), REF_SYMBOL))
  NODE_T * lhs = SUB (p);
  NODE_T * op = NEXT (lhs);
  NODE_T * rhs = NEXT (op);
  if (GOOD (lhs) && GOOD (rhs)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_identity", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_identity_relation (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_identity_relation (p, out, L_EXECUTE);
    indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
    inline_identity_relation (p, out, L_YIELD);
    undentf(out, snprintf (line, BUFFER_SIZE, ", A68_BOOL);\n")); 
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else if (GOOD (lhs) && locate (rhs, NIHIL) != NULL) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_identity", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_identity_relation (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_identity_relation (p, out, L_EXECUTE);
    indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
    inline_identity_relation (p, out, L_YIELD);
    undentf(out, snprintf (line, BUFFER_SIZE, ", A68_BOOL);\n")); 
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
#undef GOOD
}

/*!
\brief compile closed clause
\param out output file descriptor
\param p starting node
\return function name
**/

static void compile_serial_clause (NODE_T * p, FILE_T out, NODE_T ** last, int * units, int * decs, int pop, int compose_fun)
{
#define GENERATE(p) {\
  if (GENIE (p)->compile_name != NULL && program.options.opt_level > 1) {\
    indentf (out, snprintf (line, BUFFER_SIZE, "%s (N(%d));\n", GENIE (p)->compile_name, GENIE (p)->compile_node));\
  } else {\
    indentf (out, snprintf (line, BUFFER_SIZE, "EXECUTE_UNIT_TRACE (N(%d));\n", GENIE (p)->compile_node));\
  }}
/**/
  for (; p != NULL; FORWARD (p)) {
    if (compose_fun == A68_MAKE_OTHERS) {
      if (WHETHER (p, UNIT)) {
        (* units) ++;
      }
      if (WHETHER (p, DECLARATION_LIST)) {
        (* decs) ++;
      }
      if (WHETHER (p, UNIT) || WHETHER (p, DECLARATION_LIST)) {
        if (compile_unit (p, out, A68_MAKE_FUNCTION) == NULL) {
          if (WHETHER (p, UNIT) && WHETHER (SUB (p), TERTIARY)) {
            compile_units (SUB_SUB (p), out);
          } else {
            compile_units (SUB (p), out);
          }
        } else if (SUB (p) != NULL && GENIE (SUB (p)) != NULL && GENIE (SUB (p))->compile_node > 0) {
          GENIE (p)->compile_node = GENIE (SUB (p))->compile_node;
          GENIE (p)->compile_name = GENIE (SUB (p))->compile_name;
        }
        return;
      } else {
        compile_serial_clause (SUB (p), out, last, units, decs, pop, compose_fun);
      }
    } else switch (ATTRIBUTE (p)) {
      case UNIT:
        {
          (* last) = p;
          comment_source (p, out);
          if (GENIE (p)->compile_node > 0) {
            GENERATE (p);
          } else if (GENIE (SUB (p))->compile_node > 0) {
            GENERATE (SUB (p));
          } else {
            indentf (out, snprintf (line, BUFFER_SIZE, "EXECUTE_UNIT_TRACE (N(%d));\n", NUMBER (p)));\
          }
          (* units) ++;
          return;
        }
      case SEMI_SYMBOL:
        {
          if (WHETHER (* last, UNIT) && MOID (* last) == MODE (VOID)) { 
            break;
          } else if (WHETHER (* last, DECLARATION_LIST)) {
            break;
          } else {
            indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", pop));
          }
          break;
        }
      case DECLARATION_LIST:
        {
          (* last) = p;
          comment_source (p, out);
          indentf (out, snprintf (line, BUFFER_SIZE, "genie_declaration (SUB (N(%d)));\n", NUMBER (p))); 
          (* decs) ++;
          return;
        }
      default:
        {
          compile_serial_clause (SUB (p), out, last, units, decs, pop, compose_fun);
          break;
        }
    }
  }
#undef GENERATE
}

/*!
\brief compile closed clause
\param out output file descriptor
\param p starting node
\return function name
**/

static char * compile_closed_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *sc = NEXT_SUB (p);
  if (SYMBOL_TABLE (sc)->labels == NULL) {
    static char fn[NAME_SIZE];
    int units = 0, decs = 0;
    NODE_T * last = NULL;
    compile_serial_clause (sc, out, &last, &units, &decs, NUMBER (p), A68_MAKE_OTHERS);
    (void) make_name (fn, "_closed", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_STATIC_FRAME (N(%d));\n", NUMBER (sc)));
    init_static_frame (out, sc);
    units = decs = 0;
    last = NULL;
    compile_serial_clause (sc, out, &last, &units, &decs, NUMBER (p), A68_MAKE_FUNCTION);
    indent (out, "CLOSE_FRAME;\n");
    if (NEEDS_DNS (MOID (p))) {
      indentf (out, snprintf (line, BUFFER_SIZE, "GENIE_DNS_STACK (N(%d), MOID (N(%d)), frame_pointer, \"closed-clause\");\n", NUMBER (p), NUMBER (p)));
    }
    if (NEEDS_SWEEP (MOID (p))) {
      indentf (out, snprintf (line, BUFFER_SIZE, "PROTECT_FROM_SWEEP_STACK ((NODE_T *) N(%d));\n", NUMBER (p)));
    }
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile collateral clause
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_collateral_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p) && WHETHER (MOID (p), STRUCT_SYMBOL)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_collateral", "", NUMBER (p));
    comment_source (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NULL;
    inline_collateral_units (NEXT_SUB (p), out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_collateral_units (NEXT_SUB (p), out, L_EXECUTE);
    inline_collateral_units (NEXT_SUB (p), out, L_YIELD);
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
}

/*!
\brief compile conditional clause
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_conditional_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  NODE_T * q = SUB (p);
  if (! (basic_mode (MOID (p)) || MOID (p) == MODE (VOID))) {
    return (NULL);
  }
  p = q;
  if (!basic_conditional (p)) {
    return (NULL);
  }
  (void) make_name (fn, "_conditional", "", NUMBER (q));
  comment_source (p, out);
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (q, out, fn);
  }
/* Collect declarations */
  if (WHETHER (p, IF_PART) || WHETHER (p, OPEN_PART)) {
    root_idf = NULL;
    inline_unit (SUB (NEXT_SUB (p)), out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (SUB (NEXT_SUB (p)), out, L_EXECUTE);
    indent (out, "if (");
    inline_unit (SUB (NEXT_SUB (p)), out, L_YIELD);    
    undent (out, ") {\n");
    indentation ++;
  } else {
    ABEND (A68_TRUE, "if-part expected", NULL);
  }
  FORWARD (p);
  if (WHETHER (p, THEN_PART) || WHETHER (p, CHOICE)) {
    int pop = temp_book_pointer;
    (void) compile_unit (SUB (NEXT_SUB (p)), out, A68_MAKE_NOTHING);
    indentation --;
    temp_book_pointer = pop;
  } else {
    ABEND (A68_TRUE, "then-part expected", NULL);
  }
  FORWARD (p);
  if (WHETHER (p, ELSE_PART) || WHETHER (p, CHOICE)) {
    int pop = temp_book_pointer;
    indent (out, "} else {\n"); 
    indentation ++;
    (void) compile_unit (SUB (NEXT_SUB (p)), out, A68_MAKE_NOTHING);
    indentation --;
    temp_book_pointer = pop;
  }
/* Done */
  indent (out, "}\n"); 
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_postlude (q, out, fn);
  }
  return (fn);
}

/*!
\brief compile loop clause
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_loop_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *for_part = NULL, *from_part = NULL, *by_part = NULL, *to_part = NULL, * downto_part = NULL, * sc;
  static char fn[NAME_SIZE], idf[NAME_SIZE], z[NAME_SIZE];
  NODE_T * q = SUB (p), * last = NULL;
  int units, decs;
  BOOL_T sweep;
/* FOR identifier. */
  if (WHETHER (q, FOR_PART)) {
    for_part = NEXT_SUB (q);
    FORWARD (q);
  }
/* FROM unit. */
  if (WHETHER (p, FROM_PART)) {
    from_part = NEXT_SUB (q);
    if (! basic_unit (from_part)) {
      return (NULL);
    }
    FORWARD (q);
  }
/* BY unit. */
  if (WHETHER (q, BY_PART)) {
    by_part = NEXT_SUB (q);
    if (! basic_unit (by_part)) {
      return (NULL);
    }
    FORWARD (q);
  }
/* TO unit, DOWNTO unit. */
  if (WHETHER (q, TO_PART)) {
    if (WHETHER (SUB (q), TO_SYMBOL)) {
      to_part = NEXT_SUB (q);
      if (! basic_unit (to_part)) {
        return (NULL);
      }
    } else if (WHETHER (SUB (q), DOWNTO_SYMBOL)) {
      downto_part = NEXT_SUB (q);
      if (! basic_unit (downto_part)) {
        return (NULL);
      }
    } 
    FORWARD (q);
  } 
/* Does the loop contain conditionals? Tant pis */
  if (WHETHER (q, WHILE_PART)) {
    return (NULL);
  } else if (WHETHER (q, DO_PART) || WHETHER (q, ALT_DO_PART)) {
    sc = q = NEXT_SUB (q);
    if (WHETHER (q, SERIAL_CLAUSE)) {
      FORWARD (q);
    }
    if (q != NULL && WHETHER (q, UNTIL_PART)) {
      return (NULL);
    }
  } else {
    return (NULL);
  }
  if (SYMBOL_TABLE (sc)->labels != NULL) {
    return (NULL);
  }
/* A simple loop clause is compiled */
  units = decs = 0;
  compile_serial_clause (sc, out, &last, &units, &decs, NUMBER (p), A68_MAKE_OTHERS);
  sweep = (decs > 0);
  comment_source (p, out);
  (void) make_name (fn, "_loop", "", NUMBER (p));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  root_idf = NULL;
  (void) make_name (idf, "k", "", NUMBER (p));
  add_declaration (&root_idf, "int", 0, idf);
  (void) make_name (z, "z", "", NUMBER (p));
  add_declaration (&root_idf, "A68_INT", 1, z);
  if (from_part != NULL) {
    inline_unit (from_part, out, L_DECLARE);
  }
  if (by_part != NULL) {
    inline_unit (by_part, out, L_DECLARE);
  }
  if (to_part != NULL) {
    inline_unit (to_part, out, L_DECLARE);
  }
  if (downto_part != NULL) {
    inline_unit (downto_part, out, L_DECLARE);
  }
  print_declarations (out, root_idf);
  indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
  if (from_part != NULL) {
    inline_unit (from_part, out, L_EXECUTE);
  }
  if (by_part != NULL) {
    inline_unit (by_part, out, L_EXECUTE);
  }
  if (to_part != NULL) {
    inline_unit (to_part, out, L_EXECUTE);
  }
  if (downto_part != NULL) {
    inline_unit (downto_part, out, L_EXECUTE);
  }
  indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_STATIC_FRAME (N(%d));\n", NUMBER (sc)));
  init_static_frame (out, sc);
  if (for_part != NULL) {
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (N(%d)))));\n", z, NUMBER (for_part)));
  }
/* The loop, finally */
  indentf (out, snprintf (line, BUFFER_SIZE, "for (%s = ", idf));
  if (from_part == NULL) {
    undent (out, "1");
  } else {
    inline_unit (from_part, out, L_YIELD);
  }
  undent (out, "; ");
  if (to_part == NULL && downto_part == NULL) {
    undent (out, "A68_TRUE");
  } else {
    undent (out, idf);
    if (to_part != NULL) {
      undent (out, " <= ");
    } else if (downto_part != NULL) {
      undent (out, " >= ");
    }
    inline_unit (to_part, out, L_YIELD);
  }
  undent (out, "; ");
  if (by_part == NULL) {
    undent (out, idf);
    if (to_part != NULL) {
      undent (out, " ++");
    } else if (downto_part != NULL) {
      undent (out, " --");
    } else {
      undent (out, " ++");
    }
  } else {
    undent (out, idf);
    if (to_part != NULL) {
      undent (out, " += ");
    } else if (downto_part != NULL) {
      undent (out, " -= ");
    } else {
      undent (out, " += ");
    }
    inline_unit (by_part, out, L_YIELD);
  }
  undent (out, ") {\n"); 
  indentation++;
  if (sweep) {
    indent (out, "PREEMPTIVE_SWEEP;\n");
  }
  if (for_part != NULL) {
    indentf (out, snprintf (line, BUFFER_SIZE, "STATUS (%s) = INITIALISED_MASK;\n", z));
    indentf (out, snprintf (line, BUFFER_SIZE, "VALUE (%s) = %s;\n", z, idf));
  }
  units = decs = 0;
  compile_serial_clause (sc, out, &last, &units, &decs, NUMBER (p), A68_MAKE_FUNCTION);
/* Re-initialise if necessary */
  indent (out, "if (");
  if (to_part == NULL && downto_part == NULL) {
    undent (out, "A68_TRUE");
  } else {
    undent (out, idf);
    if (to_part != NULL) {
      undent (out, " < ");
    } else if (downto_part != NULL) {
      undent (out, " > ");
    }
    inline_unit (to_part, out, L_YIELD);
  }
  undent (out, ") {\n"); 
  indentation++;
  if (SYMBOL_TABLE (sc)->ap_increment > 0) { 
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_CLEAR (%d);\n", SYMBOL_TABLE (sc)->ap_increment));
  }
  indentf (out, snprintf (line, BUFFER_SIZE, "if (SYMBOL_TABLE (N(%d))->initialise_frame) {\n", NUMBER (sc)));
  indentation++;
  indentf (out, snprintf (line, BUFFER_SIZE, "initialise_frame (N(%d));\n", NUMBER (sc)));
  indentation--;
  indent (out, "}\n");
  indentation--;
  indent (out, "}\n");
/* End of loop */
  indentation--;
  indent (out, "}\n"); 
  indent (out, "CLOSE_FRAME;\n");
  indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_postlude (p, out, fn);
  }
  return (fn);
}

/*!
\brief compile serial units
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_unit (NODE_T * p, FILE_T out, BOOL_T compose_fun)
{
/**/
#define COMPILE(p, out, fun, compose_fun) {\
  char * fn = (fun) (p, out, compose_fun);\
  if (compose_fun == A68_MAKE_FUNCTION && fn != NULL) {\
    GENIE (p)->compile_name = new_string (fn);\
    if (SUB (p) != NULL && GENIE (SUB (p))->compile_node > 0) {\
      GENIE (p)->compile_node = GENIE (SUB (p))->compile_node;\
    } else {\
      GENIE (p)->compile_node = NUMBER (p);\
    }\
    return (GENIE (p)->compile_name);\
  } else {\
    GENIE (p)->compile_name = NULL;\
    GENIE (p)->compile_node = 0;\
    return (NULL);\
  }}
/**/
  if (p == NULL) {
    return (NULL);
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, ENCLOSED_CLAUSE, NULL_ATTRIBUTE)) {
    COMPILE (SUB (p), out, compile_unit, compose_fun);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    COMPILE (p, out, compile_closed_clause, compose_fun);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    COMPILE (p, out, compile_collateral_clause, compose_fun);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    COMPILE (p, out, compile_conditional_clause, compose_fun);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    COMPILE (p, out, compile_loop_clause, compose_fun);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), IDENTIFIER) != NULL) {
    COMPILE (p, out, compile_voiding_assignation_identifier, compose_fun);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SLICE) != NULL) {
    COMPILE (p, out, compile_voiding_assignation_slice, compose_fun);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SELECTION) != NULL) {
    COMPILE (p, out, compile_voiding_assignation_selection, compose_fun);
  } else if (WHETHER (p, SLICE)) {
    COMPILE (p, out, compile_slice, compose_fun);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SLICE) != NULL) {
    COMPILE (p, out, compile_dereference_slice, compose_fun);
  } else if (WHETHER (p, SELECTION)) {
    COMPILE (p, out, compile_selection, compose_fun);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SELECTION) != NULL) {
    COMPILE (p, out, compile_dereference_selection, compose_fun);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), FORMULA)) {
    COMPILE (SUB (p), out, compile_voiding_formula, compose_fun);
  } else if (WHETHER (p, FORMULA)) {
    COMPILE (p, out, compile_formula, compose_fun);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), MONADIC_FORMULA)) {
    COMPILE (SUB (p), out, compile_voiding_formula, compose_fun);
  } else if (WHETHER (p, MONADIC_FORMULA)) {
    COMPILE (p, out, compile_formula, compose_fun);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), CALL)) {
    COMPILE (p, out, compile_voiding_call, compose_fun);
  } else if (WHETHER (p, CALL)) {
    COMPILE (p, out, compile_call, compose_fun);
  } else if (WHETHER (p, CAST)) {
    COMPILE (p, out, compile_cast, compose_fun);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), DEPROCEDURING)) {
    COMPILE (p, out, compile_voiding_deproceduring, compose_fun);
  } else if (WHETHER (p, DEPROCEDURING)) {
    COMPILE (p, out, compile_deproceduring, compose_fun);
  } else if (WHETHER (p, DENOTATION)) {
    COMPILE (p, out, compile_denotation, compose_fun);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), IDENTIFIER) != NULL) {
    COMPILE (p, out, compile_dereference_identifier, compose_fun);
  } else if (WHETHER (p, IDENTIFIER)) {
    COMPILE (p, out, compile_identifier, compose_fun);
  } else if (WHETHER (p, FORMULA)) {
    COMPILE (p, out, compile_formula, compose_fun);
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    COMPILE (p, out, compile_identity_relation, compose_fun);
  } else {
    return (NULL);
  }
#undef COMPILE
}
  
/*!
\brief compile units
\param p starting node
\param out output file descriptor
**/

void compile_units (NODE_T * p, FILE_T out)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT) || WHETHER (p, TERTIARY)) {
      if (compile_unit (p, out, A68_MAKE_FUNCTION) == NULL) {
        if (WHETHER (p, UNIT) && WHETHER (SUB (p), TERTIARY)) {
          compile_units (SUB_SUB (p), out);
        } else {
          compile_units (SUB (p), out);
        }
      } else if (SUB (p) != NULL && GENIE (SUB (p)) != NULL && GENIE (SUB (p))->compile_node > 0) {
        GENIE (p)->compile_node = GENIE (SUB (p))->compile_node;
        GENIE (p)->compile_name = GENIE (SUB (p))->compile_name;
      }
    } else {
      compile_units (SUB (p), out);
    }
  }
}

/*!
\brief compiler driver
\param p module to compile
\param out output file descriptor
**/

void compiler (FILE_T out)
{
  indentation = 0;
  temp_book_pointer = 0;
  perm_book_pointer = 0;
  root_idf = NULL;
  global_level = A68_MAX_INT;
  global_pointer = 0;
  get_global_level (SUB (program.top_node));
  write_prelude (out);
  get_global_level (program.top_node);
  compile_units (program.top_node, out);
  ABEND (indentation != 0, "indentation error", NULL);
}

