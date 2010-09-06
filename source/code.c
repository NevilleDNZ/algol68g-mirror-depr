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

  tmp := x[i + 1]; tmp := tmp + 1
 
There are no optimisations that are easily recognised by the back end compiler, 
for instance symbolic simplification.
*/

#include "config.h"
#include "algol68g.h"
#include "diagnostics.h"
#include "inline.h"
#include "genie.h"
#include "transput.h"
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

#define OFFSET_OFF(s) (OFFSET (NODE_PACK (SUB (s))))

#define NAME_SIZE 100

static int indentation = 0;
static char line[BUFFER_SIZE], value[BUFFER_SIZE];

static BOOL_T basic_unit (NODE_T *);
static BOOL_T constant_unit (NODE_T *);
static void push_unit (NODE_T *);
static void code_unit (NODE_T *, FILE_T, int);
static void indentf (FILE_T, int);
static char *compile_unit (NODE_T *, FILE_T, BOOL_T);

/* The phases we go through */
enum {L_NONE = 0, L_DECLARE = 1, L_INITIALISE, L_EXECUTE, L_EXECUTE_2, L_YIELD, L_PUSH};

#define MAKE_FUNCTION A68_TRUE
#define MAKE_NO_FUNCTION A68_FALSE

/* BOOK keeps track of already seen (temporary) variables and denotations. */
typedef struct {int action, phase; char * idf; void * info; int number;} BOOK;
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

static void comment_tree (NODE_T * p, FILE_T out, int *want_space)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ROW_CHAR_DENOTATION)) {
      char * q;
      if (*want_space != 0) {
        undent (out, " ");
      }
      undent (out, "\"");
      for (q = SYMBOL (p); q[0] != NULL_CHAR; q ++) {
/* Take care not to generate nested comments through strings */
        if (q[0] == '*' && q[1] == '/') {
          undent (out, "..");
          q ++;
        } else if (q[0] == '/' && q[1] == '*') {
          undent (out, "..");
          q ++;
        } else {
          char w[2];
          w[0] = q[0];
          w[1] = NULL_CHAR;
          undent (out, w);
        }
      }
      undent (out, "\"");
      *want_space = 2;
    } else if (SUB (p) != NULL) {
      comment_tree (SUB (p), out, want_space);
    } else  if (SYMBOL (p)[0] == '(' || SYMBOL (p)[0] == '[' || SYMBOL (p)[0] == '{') {
      if (*want_space == 2) {
        undent (out, " ");
      }
      undent (out, SYMBOL (p));
      *want_space = 0;
    } else  if (SYMBOL (p)[0] == ')' || SYMBOL (p)[0] == ']' || SYMBOL (p)[0] == '}') {
      undent (out, SYMBOL (p));
      *want_space = 1;
    } else  if (SYMBOL (p)[0] == ';' || SYMBOL (p)[0] == ',') {
      undent (out, SYMBOL (p));
      *want_space = 2;
    } else  if (strlen (SYMBOL (p)) == 1 && (SYMBOL (p)[0] == '.' || SYMBOL (p)[0] == ':')) {
      undent (out, SYMBOL (p));
      *want_space = 2;
    } else {
      if (*want_space != 0) {
        undent (out, " ");
      }
      undent (out, SYMBOL (p));
      if (IS_UPPER (SYMBOL (p)[0])) {
        *want_space = 2;
      } else if (! IS_ALNUM (SYMBOL (p)[0])) {
        *want_space = 2;
      } else {
        *want_space = 1;
      }
    }
  }
}

/*!
\brief comment source line
\param p position in tree
\param out output file descriptor
**/

static void comment_source (NODE_T * p, FILE_T out)
{
  int want_space = 0;
  undentf(out, snprintf (line, BUFFER_SIZE, "/* %s: %d: ", LINE (p)->filename, NUMBER (LINE (p))));
  comment_tree (p, out, &want_space);
  undent (out, " */\n");
}

/*!
\brief write prelude
\param out output file descriptor
**/

static void code_prelude (FILE_T out)
{
  indentf (out, snprintf (line, BUFFER_SIZE, "/* \"%s\" generated by %s %s */\n\n", program.files.object.name, A68G_NAME, REVISION));
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
    if (idf != NULL) {
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
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_COMPLEX %s;\n", acc));      
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
        undent (out, "DBL_MAX");
      } else if (errno == ERANGE && conv == -HUGE_VAL) {
        undent (out, "(-DBL_MAX)");
      } else {
        ABEND (errno != 0, "strange conversion error", NULL);
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

static void code_fun_prelude (NODE_T * p, FILE_T out, char * fn)
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

static void code_fun_postlude (NODE_T * p, FILE_T out, char * fn)
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

static char * code_mode (MOID_T * m)
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

static void code_denotation (NODE_T * p, FILE_T out, int phase)
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

static void code_widening (NODE_T * p, FILE_T out, int phase)
{
  if (MOID (p) == MODE (REAL)) {
    if (phase == L_DECLARE) {
      code_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      code_unit (SUB (p), out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      undent (out, "(double) ("); 
      code_unit (SUB (p), out, L_YIELD);
      undent (out, ")");
    }
  } else if (MOID (p) == MODE (COMPLEX)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_COMPLEX %s;\n", acc));      
      code_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      code_unit (SUB (p), out, L_EXECUTE);
      indentf (out, snprintf (line, BUFFER_SIZE, "STATUS_RE (%s) = INITIALISED_MASK;\n", acc)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "STATUS_IM (%s) = INITIALISED_MASK;\n", acc)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "RE (%s) = (double) (", acc)); 
      code_unit (SUB (p), out, L_YIELD);
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

static void code_dereference_identifier (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * q = locate (SUB (p), IDENTIFIER); 
  ABEND (q == NULL, "not dereferencing an identifier", NULL);
  if (phase == L_DECLARE) {
    if (temp_booked (BOOK_DEREF, L_DECLARE, SYMBOL (q)) != NOT_BOOKED) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (q), "", NUMBER (p));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", code_mode (MOID (p)), idf)); 
      book_temp (BOOK_DEREF, L_DECLARE, SYMBOL (p), NULL, NUMBER (p));
      code_unit (SUB (p), out, L_DECLARE);
    }
  } else if (phase == L_EXECUTE) {
    if (temp_booked (BOOK_DEREF, L_EXECUTE, SYMBOL (q)) != NOT_BOOKED) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (q), "", NUMBER (p));
      code_unit (SUB (p), out, L_EXECUTE);
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS (", idf, code_mode (MOID (p)))); 
      book_temp (BOOK_DEREF, L_EXECUTE, SYMBOL (p), NULL, NUMBER (p));
      code_unit (SUB (p), out, L_YIELD);
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

static void code_identifier (NODE_T * p, FILE_T out, int phase)
{
/* Possible constant folding */
  NODE_T * def = NODE (TAX (p));
  if (primitive_mode (MOID (p)) && def != NULL && NEXT (def) != NULL && WHETHER (NEXT (def), EQUALS_SYMBOL)) {
    NODE_T * src = locate (NEXT_NEXT (def), DENOTATION);
    if (src != NULL) {
      code_denotation (src, out, phase);
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
      indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", code_mode (MOID (p)), idf));
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
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, %s, N(%d));\n", idf, code_mode (MOID (p)), NUMBER (p))); 
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

static void code_indexer (NODE_T * p, FILE_T out, int phase, int * k, char * tup)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    if (phase != L_YIELD) {
      code_unit (p, out, phase);
    } else {
      if ((*k) == 0) {
        undentf(out, snprintf (line, BUFFER_SIZE, "(%s[%d].span * (", tup, (*k)));
      } else {
        undentf(out, snprintf (line, BUFFER_SIZE, " + (%s[%d].span * (", tup, (*k)));
      }
      code_unit (p, out, L_YIELD);
      undentf(out, snprintf (line, BUFFER_SIZE, ") - %s[%d].shift)", tup, (*k)));
    }
    (*k) ++;
  } else {
    code_indexer (SUB (p), out, phase, k, tup);
    code_indexer (NEXT (p), out, phase, k, tup);
  }
}

/*!
\brief code dereferencing of slice
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void code_dereference_slice (NODE_T * p, FILE_T out, int phase)
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
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, code_mode (mode), drf, arr, tup));
      book_temp (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim)); 
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF %s; %s * %s;\n", elm, code_mode (mode), drf));
    }
    k = 0;
    code_indexer (indx, out, L_DECLARE, &k, NULL);
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
    code_indexer (indx, out, L_EXECUTE, &k, NULL);
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    code_indexer (indx, out, L_YIELD, &k, tup);
    undentf(out, snprintf (line, BUFFER_SIZE, ");\n"));     
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS(& %s);\n", drf, code_mode (mode), elm)); 
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

static void code_slice_ref_to_ref (NODE_T * p, FILE_T out, int phase)
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
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, code_mode (mode), drf, arr, tup));
      book_temp (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim)); 
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF %s; %s * %s;\n", elm, code_mode (mode), drf));
    }
    k = 0;
    code_indexer (indx, out, L_DECLARE, &k, NULL);
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
    code_indexer (indx, out, L_EXECUTE, &k, NULL);
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    code_indexer (indx, out, L_YIELD, &k, tup);
    undentf(out, snprintf (line, BUFFER_SIZE, ");\n"));
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS(& %s);\n", drf, code_mode (mode), elm)); 
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

static void code_slice (NODE_T * p, FILE_T out, int phase)
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
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, code_mode (mode), drf, arr, tup));
      book_temp (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim)); 
    } else if (same_tree (indx, (NODE_T *) (entry->info)) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF %s; %s * %s;\n", elm, code_mode (mode), drf));
    }
    k = 0;
    code_indexer (indx, out, L_DECLARE, &k, NULL);
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
    code_indexer (indx, out, L_EXECUTE, &k, NULL);
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    code_indexer (indx, out, L_YIELD, &k, tup);
    undentf(out, snprintf (line, BUFFER_SIZE, ");\n"));
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS(& %s);\n", drf, code_mode (mode), elm));
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

static void code_monadic_formula (NODE_T * p, FILE_T out, int phase)
{
  if (WHETHER (p, MONADIC_FORMULA) && MOID (p) == MODE (COMPLEX)) {
    NODE_T *op = SUB (p);
    NODE_T * rhs = NEXT (op);
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_COMPLEX %s;\n", acc));
      code_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      code_unit (rhs, out, L_EXECUTE);
      for (k = 0; k < MONADICS; k ++) {
        if (TAX (op)->procedure == monadics[k].procedure) {
          indentf (out, snprintf (line, BUFFER_SIZE, "%s (%s, ", monadics[k].code, acc));
          code_unit (rhs, out, L_YIELD);
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
      code_unit (rhs, out, phase);
    } else {
      int k;
      for (k = 0; k < MONADICS; k ++) {
        if (TAX (op)->procedure == monadics[k].procedure) {
          if (IS_ALNUM ((monadics[k].code)[0])) {
            undent (out, monadics[k].code);
            undent (out, "(");
            code_unit (rhs, out, L_YIELD);
            undent (out, ")");
          } else {
            undent (out, monadics[k].code);
            undent (out, "(");
            code_unit (rhs, out, L_YIELD);
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

static void code_formula (NODE_T * p, FILE_T out, int phase)
{
  if (WHETHER (p, FORMULA) && MOID (p) == MODE (COMPLEX)) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NULL) {
      code_monadic_formula (lhs, out, phase);
    } else if (phase == L_DECLARE) {
      NODE_T * rhs = NEXT (op);
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_COMPLEX %s;\n", acc));
      code_unit (lhs, out, L_DECLARE);
      code_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T * rhs = NEXT (op);
      char acc[NAME_SIZE];
      int k;
      (void) make_name (acc, TMP, "", NUMBER (p));
      code_unit (lhs, out, L_EXECUTE);
      code_unit (rhs, out, L_EXECUTE);
      for (k = 0; k < DYADICS; k ++) {
        if (TAX (op)->procedure == dyadics[k].procedure) {
          indentf (out, snprintf (line, BUFFER_SIZE, "%s (%s, ", dyadics[k].code, acc));
          code_unit (lhs, out, L_YIELD);
          undentf (out, snprintf (line, BUFFER_SIZE, ", "));
          code_unit (rhs, out, L_YIELD);
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
      code_monadic_formula (lhs, out, phase);
    } else if (phase != L_YIELD) {
      NODE_T * rhs = NEXT (op);
      code_unit (lhs, out, phase);
      code_unit (rhs, out, phase);
    } else {
      NODE_T * rhs = NEXT (op);
      int k;
      for (k = 0; k < DYADICS; k ++) {
        if (TAX (op)->procedure == dyadics[k].procedure) {
          if (IS_ALNUM ((dyadics[k].code)[0])) {
            undent (out, dyadics[k].code);
            undent (out, "(");
            code_unit (lhs, out, L_YIELD);
            undent (out, ", ");
            code_unit (rhs, out, L_YIELD);
            undent (out, ")");
          } else {
            undent (out, "(");
            code_unit (lhs, out, L_YIELD);
            undent (out, " ");
            undent (out, dyadics[k].code);
            undent (out, " ");
            code_unit (rhs, out, L_YIELD);
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

static void code_single_argument (NODE_T * p, FILE_T out, int phase)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ARGUMENT_LIST) || WHETHER (p, ARGUMENT)) {
      code_single_argument (SUB (p), out, phase);
    } else if (WHETHER (p, GENERIC_ARGUMENT_LIST) || WHETHER (p, GENERIC_ARGUMENT)) {
      code_single_argument (SUB (p), out, phase);
    } else if (WHETHER (p, UNIT)) {
      code_unit (p, out, phase);
    }
  }
}

/*!
\brief code call
\param p starting node
\return same
**/

static void code_call (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * args = NEXT (prim);
  NODE_T * idf = locate (prim, IDENTIFIER);
  if (MOID (p) == MODE (COMPLEX)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_COMPLEX %s;\n", acc));
      code_single_argument (args, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      code_single_argument (args, out, L_EXECUTE);
      for (k = 0; k < FUNCTIONS; k ++) {
        if (TAX (idf)->procedure == functions[k].procedure) {
          indentf (out, snprintf (line, BUFFER_SIZE, "%s (%s, ", functions[k].code, acc));
          code_single_argument (args, out, L_YIELD);
          undentf (out, snprintf (line, BUFFER_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, BUFFER_SIZE, "%s", acc));
    }
  } else if (basic_mode (MOID (p))) {
    if (phase != L_YIELD) {
      code_single_argument (args, out, phase);
    } else {
      int k;
      for (k = 0; k < FUNCTIONS; k ++) {
        if (TAX (idf)->procedure == functions[k].procedure) {
          undent (out, functions[k].code);
          undent (out, " (");
          code_single_argument (args, out, L_YIELD);
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

static void code_collateral_units (NODE_T * p, FILE_T out, int phase)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    if (phase == L_DECLARE) {
      code_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      code_unit (SUB (p), out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
      code_unit (SUB (p), out, L_YIELD);
      undentf(out, snprintf (line, BUFFER_SIZE, ", %s);\n", code_mode (MOID (p))));
    }
  } else {
    code_collateral_units (SUB (p), out, phase);
    code_collateral_units (NEXT (p), out, phase);
  }
}

/*!
\brief code collateral units
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static void code_collateral (NODE_T * p, FILE_T out, int phase)
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
    code_collateral_units (NEXT_SUB (p), out, L_DECLARE);    
  } else if (phase == L_EXECUTE) {
    code_collateral_units (NEXT_SUB (p), out, L_EXECUTE);    
    code_collateral_units (NEXT_SUB (p), out, L_YIELD);    
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

static void code_closed (NODE_T * p, FILE_T out, int phase)
{
  if (p == NULL) {
    return;
  } else if (phase != L_YIELD) {
    code_unit (SUB (NEXT_SUB (p)), out, phase);
  } else {
    undent (out, "(");
    code_unit (SUB (NEXT_SUB (p)), out, L_YIELD);
    undent (out, ")");
  }
}

/*!
\brief code basic closed clause
\param p starting node
\param out object file
\param phase phase of code generation
**/

static void code_conditional (NODE_T * p, FILE_T out, int phase)
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
    code_unit (SUB (NEXT_SUB (if_part)), out, L_DECLARE);
    code_unit (SUB (NEXT_SUB (then_part)), out, L_DECLARE);
    code_unit (SUB (NEXT_SUB (else_part)), out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    code_unit (SUB (NEXT_SUB (if_part)), out, L_EXECUTE);
    code_unit (SUB (NEXT_SUB (then_part)), out, L_EXECUTE);
    code_unit (SUB (NEXT_SUB (else_part)), out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    undent (out, "(");
    code_unit (SUB (NEXT_SUB (if_part)), out, L_YIELD);
    undent (out, " ? ");
    code_unit (SUB (NEXT_SUB (then_part)), out, L_YIELD);
    undent (out, " : ");
    if (else_part != NULL) {
      code_unit (SUB (NEXT_SUB (else_part)), out, L_YIELD);
    } else {
/* 
This is not an ideal solution although RR permits it;
an omitted else-part means SKIP: yield some value of the 
mode required. 
*/
      code_unit (SUB (NEXT_SUB (then_part)), out, L_YIELD);
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

static void code_dereference_selection (NODE_T * p, FILE_T out, int phase)
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
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s;\n", ref));
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), NULL, NUMBER (field)); 
    }
    if (entry == NOT_BOOKED || field_idf != (char *) (entry->info)) { 
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", code_mode (SUB_MOID (field)), sel));
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    code_unit (sec, out, L_DECLARE);
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
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, code_mode (SUB_MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    } else if (field_idf != (char *) (entry->info)) { 
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, code_mode (SUB_MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    code_unit (sec, out, L_EXECUTE);
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

static void code_selection (NODE_T * p, FILE_T out, int phase)
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
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_STRUCT %s;\n", ref));
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), NULL, NUMBER (field)); 
    }
    if (entry == NOT_BOOKED || field_idf != (char *) (entry->info)) { 
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", code_mode (MOID (field)), sel));
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    code_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK * entry = temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (idf));
    if (entry == NOT_BOOKED) {
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, BYTE_T, N(%d));\n", ref, NUMBER (locate (sec, IDENTIFIER)))); 
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (%s[%d]);\n", sel, code_mode (MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    } else if (field_idf != (char *) (entry->info)) { 
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (%s[%d]);\n", sel, code_mode (MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    code_unit (sec, out, L_EXECUTE);
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

static void code_selection_ref_to_ref (NODE_T * p, FILE_T out, int phase)
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
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s;\n", ref));
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), NULL, NUMBER (field)); 
    }
    if (entry == NOT_BOOKED || field_idf != (char *) (entry->info)) { 
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF %s;\n", sel));
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), (void *) field_idf, NUMBER (field)); 
    }
    code_unit (sec, out, L_DECLARE);
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
    code_unit (sec, out, L_EXECUTE);
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

static void code_ref_identifier (NODE_T * p, FILE_T out, int phase)
{
/* No folding - consider identifier */
  if (phase == L_DECLARE) {
    if (temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (p)) != NOT_BOOKED) {
      return; 
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, SYMBOL (p), "", NUMBER (p));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s;\n", idf));
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

static void code_identity_relation (NODE_T * p, FILE_T out, int phase)
{
#define GOOD(p) (locate (p, IDENTIFIER) != NULL && WHETHER (MOID (locate ((p), IDENTIFIER)), REF_SYMBOL))
  NODE_T * lhs = SUB (p);
  NODE_T * op = NEXT (lhs);
  NODE_T * rhs = NEXT (op);
  if (GOOD (lhs) && GOOD (rhs)) {
    if (phase == L_DECLARE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      code_ref_identifier (lidf, out, L_DECLARE);
      code_ref_identifier (ridf, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      code_ref_identifier (lidf, out, L_EXECUTE);
      code_ref_identifier (ridf, out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      if (WHETHER (op, IS_SYMBOL)) {
        undentf (out, snprintf (line, BUFFER_SIZE, "ADDRESS (")); 
        code_ref_identifier (lidf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ") == ADDRESS ("));  
        code_ref_identifier (ridf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ")")); 
      } else {
        undentf (out, snprintf (line, BUFFER_SIZE, "ADDRESS (")); 
        code_ref_identifier (lidf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ") != ADDRESS (")); 
        code_ref_identifier (ridf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ")")); 
      }
    }
  } else if (GOOD (lhs) && locate (rhs, NIHIL) != NULL) {
    if (phase == L_DECLARE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      code_ref_identifier (lidf, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      code_ref_identifier (lidf, out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      if (WHETHER (op, IS_SYMBOL)) {
        indentf (out, snprintf (line, BUFFER_SIZE, "IS_NIL (*")); 
        code_ref_identifier (lidf, out, L_YIELD);
        undentf(out, snprintf (line, BUFFER_SIZE, ")")); 
      } else {
        indentf (out, snprintf (line, BUFFER_SIZE, "!IS_NIL (*")); 
        code_ref_identifier (lidf, out, L_YIELD);
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

static void code_unit (NODE_T * p, FILE_T out, int phase)
{
  if (p == NULL) {
    return;
  } else if (constant_unit (p) && locate (p, DENOTATION) == NULL) {
    constant_folder (p, out, phase);
  } else if (WHETHER (p, UNIT)) {
    code_unit (SUB (p), out, phase);
  } else if (WHETHER (p, TERTIARY)) {
    code_unit (SUB (p), out, phase);
  } else if (WHETHER (p, SECONDARY)) {
    code_unit (SUB (p), out, phase);
  } else if (WHETHER (p, PRIMARY)) {
    code_unit (SUB (p), out, phase);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    code_unit (SUB (p), out, phase);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    code_closed (p, out, phase);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    code_collateral (p, out, phase);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    code_conditional (p, out, phase);
  } else if (WHETHER (p, WIDENING)) {
    code_widening (p, out, phase);
  } else if (WHETHER (p, IDENTIFIER)) {
    code_identifier (p, out, phase);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), IDENTIFIER) != NULL) {
    code_dereference_identifier (p, out, phase);
  } else if (WHETHER (p, SLICE)) {
    NODE_T * prim = SUB (p);
    MOID_T * mode = MOID (p);
    MOID_T * row_mode = DEFLEX (MOID (prim));
    if (mode == SUB (row_mode)) {
      code_slice (p, out, phase);
    } else if (WHETHER (mode, REF_SYMBOL) && WHETHER (row_mode, REF_SYMBOL) && SUB (mode) == SUB_SUB (row_mode)) {
      code_slice_ref_to_ref (p, out, phase);
    } else {
      ABEND (A68_TRUE, "strange mode for slice", NULL);
    }
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SLICE) != NULL) {
    code_dereference_slice (SUB (p), out, phase);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SELECTION) != NULL) {
    code_dereference_selection (SUB (p), out, phase);
  } else if (WHETHER (p, SELECTION)) {
    NODE_T * sec = NEXT_SUB (p);
    MOID_T * mode = MOID (p);
    MOID_T * struct_mode = MOID (sec);
    if (WHETHER (struct_mode, REF_SYMBOL) && WHETHER (mode, REF_SYMBOL)) {
      code_selection_ref_to_ref (p, out, phase);
    } else if (WHETHER (struct_mode, STRUCT_SYMBOL) && primitive_mode (mode)) {
      code_selection (p, out, phase);
    } else {
      ABEND (A68_TRUE, "strange mode for selection", NULL);
    }
  } else if (WHETHER (p, DENOTATION)) {
    code_denotation (p, out, phase);
  } else if (WHETHER (p, MONADIC_FORMULA)) {
    code_monadic_formula (p, out, phase);
  } else if (WHETHER (p, FORMULA)) {
    code_formula (p, out, phase);
  } else if (WHETHER (p, CALL)) {
    code_call (p, out, phase);
  } else if (WHETHER (p, CAST)) {
    code_unit (NEXT_SUB (p), out, phase);
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    code_identity_relation (p, out, phase);
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
    code_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, BUFFER_SIZE, ", %s);\n", code_mode (MOID (p)))); 
  } else if (basic_mode (MOID (p))) {
    indentf (out, snprintf (line, BUFFER_SIZE, "MOVE ((void *) STACK_TOP, (void *) ")); 
    code_unit (p, out, L_YIELD);
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
    code_unit (p, out, L_YIELD);
    undent (out, ";\n");
  } else if (basic_mode (MOID (p))) {
    indentf (out, snprintf (line, BUFFER_SIZE, "MOVE ((void *) %s, (void *) ", dst)); 
    code_unit (p, out, L_YIELD);
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

static char * compile_denotation (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
/*
We keep track of compiled denotations since one program can have several
same denotations, typically 0 or 1.
*/
  static char fn[NAME_SIZE];
  if (primitive_mode (MOID (p)) && mkfun) {
    BOOK * entry = perm_booked (BOOK_COMPILE, L_NONE, SYMBOL (p), MOID (p));
    if (entry != NOT_BOOKED) {
      (void) make_name (fn, "_denotation", "", NUMBER (entry));      
      return (fn);
    } else {
      (void) make_name (fn, "_denotation", "", NUMBER (p));
      comment_source (p, out);
      code_fun_prelude (p, out, fn);
      code_unit (p, out, L_DECLARE);
      code_unit (p, out, L_EXECUTE);
      indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
      code_unit (p, out, L_YIELD);
      undentf (out, snprintf (line, BUFFER_SIZE, ", %s);\n", code_mode (MOID (p)))); 
      code_fun_postlude (p, out, fn);
      book_perm (BOOK_COMPILE, L_NONE, SYMBOL (p), (void *) MOID (p), NUMBER (p)); 
      return (fn);
    }
  } else if (primitive_mode (MOID (p))) {
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
    code_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, BUFFER_SIZE, ", %s);\n", code_mode (MOID (p)))); 
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

static char * compile_cast (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_cast", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_unit (NEXT_SUB (p), out, L_DECLARE);
    code_unit (NEXT_SUB (p), out, L_EXECUTE);
    compile_push (NEXT_SUB (p), out);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_identifier (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_identifier", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_dereference_identifier (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_dereference_identifier", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_slice (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_slice", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_dereference_slice (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_deref_slice", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_selection (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_selection", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_dereference_selection (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_deref_selection", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_formula (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_formula", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    compile_push (p, out);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_voiding_formula (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_void_formula", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    code_unit (p, out, L_DECLARE);
    code_unit (p, out, L_EXECUTE);
    indent (out, "(void) (");
    code_unit (p, out, L_YIELD);
    undent (out, ");\n");
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static void code_arguments (NODE_T * p, FILE_T out, int phase, int * size)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT) && phase == L_PUSH) {
    indentf (out, snprintf (line, BUFFER_SIZE, "EXECUTE_UNIT (N(%d));\n", NUMBER (p)));
    code_arguments (NEXT (p), out, L_PUSH, size);
  } else if (WHETHER (p, UNIT)) {
    char arg[NAME_SIZE];
    (void) make_name (arg, ARG, "", NUMBER (p));
    if (phase == L_DECLARE) {
      indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", code_mode (MOID (p)), arg)); 
      code_unit (p, out, L_DECLARE);
    } else if (phase == L_INITIALISE) {
      code_unit (p, out, L_EXECUTE);
    } else if (phase == L_EXECUTE) {
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) FRAME_OBJECT (%d);\n", arg, code_mode (MOID (p)), *size)); 
      (*size) += MOID_SIZE (MOID (p));
    } else if (phase == L_YIELD && primitive_mode (MOID (p))) {
      indentf (out, snprintf (line, BUFFER_SIZE, "S (%s) = INITIALISED_MASK;\n", arg)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "V (%s) = ", arg)); 
      code_unit (p, out, L_YIELD);
      undent (out, ";\n");
    } else if (phase == L_YIELD && basic_mode (MOID (p))) {
      indentf (out, snprintf (line, BUFFER_SIZE, "MOVE ((void *) %s, (void *) ", arg)); 
      code_unit (p, out, L_YIELD);
      undentf (out, snprintf (line, BUFFER_SIZE, ", %d);\n", MOID_SIZE (MOID (p)))); 
    }
  } else {
    code_arguments (SUB (p), out, phase, size);
    code_arguments (NEXT (p), out, phase, size);
  }
}

/*!
\brief compile deproceduring
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_deproceduring (NODE_T * p, FILE_T out, BOOL_T mkfun)
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
    (void) make_name (fun, FUN, "", NUMBER (idf));
    (void) make_name (fn, "_deproc", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "A68_PROCEDURE * %s;\n", fun));
    indentf (out, snprintf (line, BUFFER_SIZE, "NODE_T * body;\n"));
    if (mkfun) {
      indent (out, "UP_SWEEP_SEMA;\n");
    }
/* Initialise */
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_PROCEDURE, N(%d));\n", fun, NUMBER (idf))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "body = SUB (BODY (%s).node);\n", fun)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
/* Execute procedure */
    indent (out, "EXECUTE_UNIT (NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, BUFFER_SIZE, "change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    if (GENIE (p)->protect_sweep != NULL) {
      indentf (out, snprintf (line, BUFFER_SIZE, "PROTECT_FROM_SWEEP_STACK (N(%d));\n", NUMBER (p))); 
    }
    if (mkfun) {
      indent (out, "DOWN_SWEEP_SEMA;\n");
      code_fun_postlude (p, out, fn);
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

static char * compile_voiding_deproceduring (NODE_T * p, FILE_T out, BOOL_T mkfun)
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
    (void) make_name (fun, FUN, "", NUMBER (idf));
    (void) make_name (fn, "_void_deproc", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    indentf (out, snprintf (line, BUFFER_SIZE, "A68_PROCEDURE * %s;\n", fun));
    indentf (out, snprintf (line, BUFFER_SIZE, "NODE_T * body;\n"));
    if (mkfun) {
      indent (out, "UP_SWEEP_SEMA;\n");
    }
/* Initialise */
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_PROCEDURE, N(%d));\n", fun, NUMBER (idf))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "body = SUB (BODY (%s).node);\n", fun)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
/* Execute procedure */
    indent (out, "EXECUTE_UNIT (NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, BUFFER_SIZE, "change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    indent (out, "CLOSE_FRAME;\n");
    if (mkfun) {
      indent (out, "DOWN_SWEEP_SEMA;\n");
      code_fun_postlude (p, out, fn);
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

static char * compile_call (NODE_T * p, FILE_T out, BOOL_T mkfun)
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
      if (mkfun) {
        code_fun_prelude (p, out, fn);
      }
      code_unit (p, out, L_DECLARE);
      code_unit (p, out, L_EXECUTE);
      compile_push (p, out);
      if (mkfun) {
        indent (out, "DOWN_SWEEP_SEMA;\n");
        code_fun_postlude (p, out, fn);
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
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    indentf (out, snprintf (line, BUFFER_SIZE, "A68_PROCEDURE * %s; NODE_T * body;\n", fun));
/* Compute arguments */
    size = 0;
    code_arguments (args, out, L_DECLARE, &size);
    if (mkfun) {
      indent (out, "UP_SWEEP_SEMA;\n");
    }
    code_arguments (args, out, L_INITIALISE, &size);
/* Initialise */
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_PROCEDURE, N(%d));\n", fun, NUMBER (idf))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "body = SUB (BODY (%s).node);\n", fun)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    size = 0;
    code_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    code_arguments (args, out, L_YIELD, &size);
/* Execute procedure */
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    indent (out, "EXECUTE_UNIT (NEXT_NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, BUFFER_SIZE, "change_masks (program.top_node, BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    if (GENIE (p)->protect_sweep != NULL) {
      indentf (out, snprintf (line, BUFFER_SIZE, "PROTECT_FROM_SWEEP_STACK (N(%d));\n", NUMBER (p))); 
    }
    if (mkfun) {
      indent (out, "DOWN_SWEEP_SEMA;\n");
      code_fun_postlude (p, out, fn);
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

static char * compile_voiding_call (NODE_T * p, FILE_T out, BOOL_T mkfun)
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
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    indentf (out, snprintf (line, BUFFER_SIZE, "A68_PROCEDURE * %s; NODE_T * body;\n", fun));
/* Compute arguments */
    size = 0;
    code_arguments (args, out, L_DECLARE, &size);
    if (mkfun) {
      indent (out, "UP_SWEEP_SEMA;\n");
    }
    code_arguments (args, out, L_INITIALISE, &size);
/* Initialise */
    indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_PROCEDURE, N(%d));\n", fun, NUMBER (idf))); 
    indentf (out, snprintf (line, BUFFER_SIZE, "body = SUB (BODY (%s).node);\n", fun)); 
    indentf (out, snprintf (line, BUFFER_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    size = 0;
    code_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    code_arguments (args, out, L_YIELD, &size);
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
    if (mkfun) {
      indent (out, "DOWN_SWEEP_SEMA;\n");
      code_fun_postlude (p, out, fn);
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

char * compile_voiding_assignation_selection (NODE_T * p, FILE_T out, BOOL_T mkfun)
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
    (void) make_name (fn, "_void_assign", "", NUMBER (dst));
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
/* Declare */
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    if (temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (idf)) == NOT_BOOKED) { 
      (void) make_name (ref, SYMBOL (idf), "", NUMBER (field));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s; /* %s */\n", ref, SYMBOL (idf)));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", code_mode (SUB_MOID (field)), sel));
      book_temp (BOOK_DECL, L_DECLARE, SYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else {
      int n = NUMBER (temp_booked (BOOK_DECL, L_DECLARE, SYMBOL (idf)));
      (void) make_name (ref, SYMBOL (idf), "", n);
      (void) make_name (sel, SEL, "", n);
    } 
    code_unit (src, out, L_DECLARE);
/* Initialise */
    if (temp_booked (BOOK_DECL, L_EXECUTE, SYMBOL (idf)) == NOT_BOOKED) { 
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", ref, NUMBER (locate (sec, IDENTIFIER)))); 
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, code_mode (SUB_MOID (field)), ref, OFFSET_OFF (field))); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (idf), (void *) field_idf, NUMBER (field));
    } 
    code_unit (src, out, L_EXECUTE);
/* Generate */
    compile_assign (src, out, sel);
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (mkfun) {
      code_fun_postlude (locate (p, ASSIGNATION), out, fn);
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

static char * compile_voiding_assignation_slice (NODE_T * p, FILE_T out, BOOL_T mkfun)
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
    (void) make_name (fn, "_void_assign", "", NUMBER (dst));
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
/* Declare */
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    if (temp_booked (BOOK_DECL, L_DECLARE, symbol) == NOT_BOOKED) { 
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, BUFFER_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, code_mode (mode), drf, arr, tup));
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
    code_indexer (indx, out, L_DECLARE, &k, NULL);
    code_unit (src, out, L_DECLARE);
/* Initialise */
    if (temp_booked (BOOK_DECL, L_EXECUTE, symbol) == NOT_BOOKED) { 
      indentf (out, snprintf (line, BUFFER_SIZE, "FRAME_GET (%s, A68_REF, N(%d));\n", idf, NUMBER (locate (prim, IDENTIFIER)))); 
      indentf (out, snprintf (line, BUFFER_SIZE, "GET_DESCRIPTOR (%s, %s, (A68_ROW *) ADDRESS (%s));\n", arr, tup, idf)); 
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = ARRAY (%s);\n", elm, arr)); 
      book_temp (BOOK_DECL, L_EXECUTE, SYMBOL (p), (void *) indx, NUMBER (prim));
    }
    k = 0;
    code_indexer (indx, out, L_EXECUTE, &k, NULL);
    indentf (out, snprintf (line, BUFFER_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    code_indexer (indx, out, L_YIELD, &k, tup);
    undentf(out, snprintf (line, BUFFER_SIZE, ");\n"));     
    indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS(& %s);\n", drf, code_mode (mode), elm)); 
    code_unit (src, out, L_EXECUTE);
/* Generate */
    compile_assign (src, out, drf);
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (mkfun) {
      code_fun_postlude (locate (p, ASSIGNATION), out, fn);
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

static char * compile_voiding_assignation_identifier (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  NODE_T * dst = SUB (locate (p, ASSIGNATION));
  NODE_T * src = NEXT_NEXT (dst);
  if (BASIC (dst, IDENTIFIER) && basic_unit (src) && basic_mode_non_row (MOID (src))) {
    static char fn[NAME_SIZE];
    char idf[NAME_SIZE];
    NODE_T * q = locate (dst, IDENTIFIER);
/* Declare */
    (void) make_name (fn, "_void_assign", "", NUMBER (dst));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    indentf (out, snprintf (line, BUFFER_SIZE, "ADDR_T pop_%d = stack_pointer;\n", NUMBER (p)));
    if (temp_booked (BOOK_DEREF, L_DECLARE, SYMBOL (q)) == NOT_BOOKED) {
      (void) make_name (idf, SYMBOL (q), "", NUMBER (p));
      indentf (out, snprintf (line, BUFFER_SIZE, "%s * %s;\n", code_mode (SUB_MOID (dst)), idf));
      book_temp (BOOK_DEREF, L_DECLARE, SYMBOL (q), NULL, NUMBER (p));
    } else {
      (void) make_name (idf, SYMBOL (q), "", NUMBER (temp_booked (BOOK_DEREF, L_DECLARE, SYMBOL (p))));
    }
    code_unit (dst, out, L_DECLARE);
    code_unit (src, out, L_DECLARE);
    code_unit (dst, out, L_EXECUTE);
    if (temp_booked (BOOK_DEREF, L_EXECUTE, SYMBOL (q)) == NOT_BOOKED) {
      indentf (out, snprintf (line, BUFFER_SIZE, "%s = (%s *) ADDRESS (", idf, code_mode (SUB_MOID (dst)))); 
      code_unit (dst, out, L_YIELD);
      undent (out, ");\n"); 
      book_temp (BOOK_DEREF, L_EXECUTE, SYMBOL (q), NULL, NUMBER (p));
    }
    code_unit (src, out, L_EXECUTE);
    compile_assign (src, out, idf);
    indentf (out, snprintf (line, BUFFER_SIZE, "stack_pointer = pop_%d;\n", NUMBER (p)));
    if (mkfun) {
      code_fun_postlude (locate (p, ASSIGNATION), out, fn);
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

static char * compile_identity_relation (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
#define GOOD(p) (locate (p, IDENTIFIER) != NULL && WHETHER (MOID (locate ((p), IDENTIFIER)), REF_SYMBOL))
  NODE_T * lhs = SUB (p);
  NODE_T * op = NEXT (lhs);
  NODE_T * rhs = NEXT (op);
  if (GOOD (lhs) && GOOD (rhs)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_identity", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_identity_relation (p, out, L_DECLARE);
    code_identity_relation (p, out, L_EXECUTE);
    indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
    code_identity_relation (p, out, L_YIELD);
    undentf(out, snprintf (line, BUFFER_SIZE, ", A68_BOOL);\n")); 
    if (mkfun) {
      code_fun_postlude (p, out, fn);
    }
    return (fn);
  } else if (GOOD (lhs) && locate (rhs, NIHIL) != NULL) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_identity", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_identity_relation (p, out, L_DECLARE);
    code_identity_relation (p, out, L_EXECUTE);
    indentf (out, snprintf (line, BUFFER_SIZE, "PUSH_PRIMITIVE (p, ")); 
    code_identity_relation (p, out, L_YIELD);
    undentf(out, snprintf (line, BUFFER_SIZE, ", A68_BOOL);\n")); 
    if (mkfun) {
      code_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NULL);
  }
#undef GOOD
}

/*!
\brief compile collateral clause
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_collateral_clause (NODE_T * p, FILE_T out, BOOL_T mkfun)
{
  if (basic_unit (p) && WHETHER (MOID (p), STRUCT_SYMBOL)) {
    static char fn[NAME_SIZE];
    (void) make_name (fn, "_collateral", "", NUMBER (p));
    comment_source (p, out);
    if (mkfun) {
      code_fun_prelude (p, out, fn);
    }
    code_collateral_units (NEXT_SUB (p), out, L_DECLARE);
    code_collateral_units (NEXT_SUB (p), out, L_EXECUTE);
    code_collateral_units (NEXT_SUB (p), out, L_YIELD);
    if (mkfun) {
      code_fun_postlude (p, out, fn);
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

static char * compile_conditional_clause (NODE_T * p, FILE_T out, BOOL_T mkfun)
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
  if (mkfun) {
    code_fun_prelude (q, out, fn);
  }
/* Collect declarations */
  if (WHETHER (p, IF_PART) || WHETHER (p, OPEN_PART)) {
    code_unit (SUB (NEXT_SUB (p)), out, L_DECLARE);
    code_unit (SUB (NEXT_SUB (p)), out, L_EXECUTE);
    indent (out, "if (");
    code_unit (SUB (NEXT_SUB (p)), out, L_YIELD);    
    undent (out, ") {\n");
    indentation ++;
  } else {
    ABEND (A68_TRUE, "if-part expected", NULL);
  }
  FORWARD (p);
  if (WHETHER (p, THEN_PART) || WHETHER (p, CHOICE)) {
    int pop = temp_book_pointer;
    (void) compile_unit (SUB (NEXT_SUB (p)), out, MAKE_NO_FUNCTION);
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
    (void) compile_unit (SUB (NEXT_SUB (p)), out, MAKE_NO_FUNCTION);
    indentation --;
    temp_book_pointer = pop;
  }
/* Done */
  indent (out, "}\n"); 
  (void) make_name (fn, "_conditional", "", NUMBER (q));
  if (mkfun) {
    code_fun_postlude (q, out, fn);
  }
  return (fn);
}

/*!
\brief compile serial units
\param out output file descriptor
\param p starting node
\return function name or NULL
**/

static char * compile_unit (NODE_T * p, FILE_T out, BOOL_T make_function)
{
#define COMPILE(p, out, fun, mkfun) {\
  char * fn = (fun) (p, out, mkfun);\
  if (mkfun && fn != NULL) {\
    GENIE (p)->compile_name = new_string (fn);\
    return (GENIE (p)->compile_name);\
  } else {\
    GENIE (p)->compile_name = NULL;\
    return (NULL);\
  }}
/**/
  if (p == NULL) {
    return (NULL);
  } 
  if (WHETHER (p, UNIT)) {
    COMPILE (SUB (p), out, compile_unit, make_function);
  } else if (WHETHER (p, TERTIARY)) {
    COMPILE (SUB (p), out, compile_unit, make_function);
  } else if (WHETHER (p, SECONDARY)) {
    COMPILE (SUB (p), out, compile_unit, make_function);
  } else if (WHETHER (p, PRIMARY)) {
    COMPILE (SUB (p), out, compile_unit, make_function);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    COMPILE (SUB (p), out, compile_unit, make_function);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    COMPILE (p, out, compile_collateral_clause, make_function);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    COMPILE (p, out, compile_conditional_clause, make_function);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), IDENTIFIER) != NULL) {
    COMPILE (p, out, compile_voiding_assignation_identifier, make_function);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SLICE) != NULL) {
    COMPILE (p, out, compile_voiding_assignation_slice, make_function);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SELECTION) != NULL) {
    COMPILE (p, out, compile_voiding_assignation_selection, make_function);
  } else if (WHETHER (p, SLICE)) {
    COMPILE (p, out, compile_slice, make_function);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SLICE) != NULL) {
    COMPILE (p, out, compile_dereference_slice, make_function);
  } else if (WHETHER (p, SELECTION)) {
    COMPILE (p, out, compile_selection, make_function);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), SELECTION) != NULL) {
    COMPILE (p, out, compile_dereference_selection, make_function);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), FORMULA)) {
    COMPILE (SUB (p), out, compile_voiding_formula, make_function);
  } else if (WHETHER (p, FORMULA)) {
    COMPILE (p, out, compile_formula, make_function);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), MONADIC_FORMULA)) {
    COMPILE (SUB (p), out, compile_voiding_formula, make_function);
  } else if (WHETHER (p, MONADIC_FORMULA)) {
    COMPILE (p, out, compile_formula, make_function);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), CALL)) {
    COMPILE (p, out, compile_voiding_call, make_function);
  } else if (WHETHER (p, CALL)) {
    COMPILE (p, out, compile_call, make_function);
  } else if (WHETHER (p, CAST)) {
    COMPILE (p, out, compile_cast, make_function);
  } else if (WHETHER (p, VOIDING) && WHETHER (SUB (p), DEPROCEDURING)) {
    COMPILE (p, out, compile_voiding_deproceduring, make_function);
  } else if (WHETHER (p, DEPROCEDURING)) {
    COMPILE (p, out, compile_deproceduring, make_function);
  } else if (WHETHER (p, DENOTATION)) {
    COMPILE (p, out, compile_denotation, make_function);
  } else if (WHETHER (p, DEREFERENCING) && locate (SUB (p), IDENTIFIER) != NULL) {
    COMPILE (p, out, compile_dereference_identifier, make_function);
  } else if (WHETHER (p, IDENTIFIER)) {
    COMPILE (p, out, compile_identifier, make_function);
  } else if (WHETHER (p, FORMULA)) {
    COMPILE (p, out, compile_formula, make_function);
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    COMPILE (p, out, compile_identity_relation, make_function);
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
      if (compile_unit (p, out, MAKE_FUNCTION) == NULL) {
        if (WHETHER (p, UNIT) && WHETHER (SUB (p), TERTIARY)) {
          compile_units (SUB_SUB (p), out);
        } else {
          compile_units (SUB (p), out);
        }
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
  code_prelude (out);
  get_global_level (program.top_node);
  compile_units (program.top_node, out);
  ABEND (indentation != 0, "indentation error", NULL);
}

/*********************/
/* Numerical library */
/*********************/

/*
Note that the interpreter has its own routines for these simple tasks that
often are optimised to work with values pushed on the stack, and that
perform runtime checks. These functions are not mangled to fit below routines.
*/

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
\brief OP ROUND = (REAL) INT
**/

int a68g_round (double x)
{
  if (x >= 0) {
    return ((int) (x + 0.5));
  } else {
    return ((int) (x - 0.5));
  }
}

/*!
\brief OP ENTIER = (REAL) INT
**/

int a68g_entier (double x)
{
  return ((int) floor (x));
}

/*!
PROC exp = (REAL) REAL
**/

double a68g_exp (double x)
{
  if (x < log (DBL_MIN)) {
    return (0.0);
  } else {
    return (exp (x));
  }
}

/*!
\brief PROC atan2 (REAL, REAL) REAL
**/

double a68g_atan2 (double x, double y)
{
  if (x == 0.0 && y == 0.0) {
    errno = EDOM;
    return (0.0);
  } else {
    BOOL_T flip = (BOOL_T) (y < 0.0);
    double z;
    y = ABS (y);
    if (x == 0.0) {
      z = A68_PI / 2.0;
    } else {
      BOOL_T flop = (BOOL_T) (x < 0.0);
      x = ABS (x);
      z = atan (y / x);
      if (flop) {
        z = A68_PI - z;
      }
    }
    if (flip) {
      z = -z;
    }
    return (z);
  }
}

/*!
\brief PROC asinh = (REAL) REAL
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
\brief PROC acosh = (REAL) REAL
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
\brief PROC atanh = (REAL) REAL
**/

double a68g_atanh (double x)
{
  double a = ABS (x);
  double s = (double) (x < 0 ? -1 : 1);
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
\brief OP MOD = (INT, INT) INT
**/

int a68g_mod_int (int i, int j)
{
  int k = i % j;
  return (k >= 0 ? k : k + labs (j));
}

/*!
\brief OP +:= = (REF INT, INT) REF INT
**/

A68_REF * a68g_plusab_int (A68_REF * i, int j)
{
  VALUE ((A68_INT *) ADDRESS (i)) += j;
  return (i);
}

/*!
\brief OP -:= = (REF INT, INT) REF INT
**/

A68_REF * a68g_minusab_int (A68_REF * i, int j)
{
  VALUE ((A68_INT *) ADDRESS (i)) -= j;
  return (i);
}

/*!
\brief OP *:= = (REF INT, INT) REF INT
**/

A68_REF * a68g_timesab_int (A68_REF * i, int j)
{
  VALUE ((A68_INT *) ADDRESS (i)) *= j;
  return (i);
}

/*!
\brief OP %:= = (REF INT, INT) REF INT
**/

A68_REF * a68g_overab_int (A68_REF * i, int j)
{
  VALUE ((A68_INT *) ADDRESS (i)) /= j;
  return (i);
}

/*!
\brief OP +:= = (REF REAL, REAL) REF REAL
**/

A68_REF * a68g_plusab_real (A68_REF * x, double y)
{
  VALUE ((A68_REAL *) ADDRESS (x)) += y;
  return (x);
}

/*!
\brief OP -:= = (REF REAL, REAL) REF REAL
**/

A68_REF * a68g_minusab_real (A68_REF * x, double y)
{
  VALUE ((A68_REAL *) ADDRESS (x)) -= y;
  return (x);
}

/*!
\brief OP *:= = (REF REAL, REAL) REF REAL
**/

A68_REF * a68g_timesab_real (A68_REF * x, double y)
{
  VALUE ((A68_REAL *) ADDRESS (x)) *= y;
  return (x);
}

/*!
\brief OP /:= = (REF REAL, REAL) REF REAL
**/

A68_REF * a68g_divab_real (A68_REF * x, double y)
{
  VALUE ((A68_REAL *) ADDRESS (x)) /= y;
  return (x);
}

/*!
\brief OP ** = (REAL, REAL) REAL
**/

double a68g_pow_real (double x, double y)
{
  return (exp (y * log (x)));
}

/*! 
\brief OP ** = (REAL, INT) REAL 
**/

double a68g_pow_real_int (double x, int n)
{
  switch (n) {
    case 2: return (x * x);
    case 3: return (x * x * x);
    case 4: {double y = x * x; return (y * y);}
    case 5: {double y = x * x; return (x * y * y);}
    case 6: {double y = x * x * x; return (y * y);}
    default: {
      int expo = 1, m = labs (n);
      BOOL_T cont = (m > 0);
      double mult = x, prod = 1;
      while (cont) {
        if ((m & expo) != 0) {
          prod *= mult;
        }
        expo *= 2;
        cont = (expo <= m);
        if (cont) {
          mult *= mult;
        }
      }
      return (n < 0 ? 1 / prod : prod);
    }
  }
}

/*!
\brief OP RE = (COMPLEX) REAL
**/

double a68g_re_complex (A68_REAL * z)
{
  return (RE (z));
}

/*!
\brief OP IM = (COMPLEX) REAL
\param p position in tree
**/

double a68g_im_complex (A68_REAL * z)
{
  return (IM (z));
}

/*!
\brief ABS = (COMPLEX) REAL
**/

double a68g_abs_complex (A68_REAL * z)
{
  return (a68g_hypot (RE (z), IM (z)));
}

/*!
\brief OP ARG = (COMPLEX) REAL
**/

double a68g_arg_complex (A68_REAL * z)
{
  if (RE (z) != 0 || IM (z) != 0) {
    return (atan2 (IM (z), RE (z)));
  } else {
    errno = EDOM;
    return (0); /* Error ! */
  }
}

/*!
\brief OP +* = (REAL, REAL) COMPLEX
**/

void a68g_i_complex (A68_REAL * z, double re, double im) 
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  RE (z) = re;
  IM (z) = im;
}

/*!
\brief OP - = (COMPLEX) COMPLEX
**/

void a68g_minus_complex (A68_REAL * z, A68_REAL * x) 
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  RE (z) = -RE (x);
  IM (z) = -IM (x);
}

/*!
\brief OP CONJ = (COMPLEX) COMPLEX
**/

void a68g_conj_complex (A68_REAL * z, A68_REAL * x) 
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  RE (z) = RE (x);
  IM (z) = -IM (x);
}

/*!
\brief OP + = (COMPLEX, COMPLEX) COMPLEX
**/

void a68g_add_complex (A68_REAL * z, A68_REAL * x, A68_REAL * y)
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  RE (z) = RE (x) + RE (y);
  IM (z) = IM (x) + IM (y);
}

/*!
\brief OP - = (COMPLEX, COMPLEX) COMPLEX
**/

void a68g_sub_complex (A68_REAL * z, A68_REAL * x, A68_REAL * y)
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  RE (z) = RE (x) - RE (y);
  IM (z) = IM (x) - IM (y);
}

/*!
\brief OP * = (COMPLEX, COMPLEX) COMPLEX
**/

void a68g_mul_complex (A68_REAL * z, A68_REAL * x, A68_REAL * y)
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  RE (z) = RE (x) * RE (y) - IM (x) * IM (y);
  IM (z) = IM (x) * RE (y) + RE (x) * IM (y);
}

/*!
\brief OP / = (COMPLEX, COMPLEX) COMPLEX
**/

void a68g_div_complex (A68_REAL * z, A68_REAL * x, A68_REAL * y)
{
  if (RE (y) == 0 && IM (y) == 0) {
    RE (z) = 0.0;
    IM (z) = 0.0;
    errno = EDOM;
  } else  if (fabs (RE (y)) >= fabs (IM (y))) {
    double r = IM (y) / RE (y), den = RE (y) + r * IM (y);
    STATUS_RE (z) = INITIALISED_MASK;
    STATUS_IM (z) = INITIALISED_MASK;
    RE (z) = (RE (x) + r * IM (x)) / den;
    IM (z) = (IM (x) - r * RE (x)) / den;
  } else {
    double r = RE (y) / IM (y), den = IM (y) + r * RE (y);
    STATUS_RE (z) = INITIALISED_MASK;
    STATUS_IM (z) = INITIALISED_MASK;
    RE (z) = (RE (x) * r + IM (x)) / den;
    IM (z) = (IM (x) * r - RE (x)) / den;
  }
}

/*!
\brief OP = = (COMPLEX, COMPLEX) BOOL
**/

BOOL_T a68g_eq_complex (A68_REAL * x, A68_REAL * y)
{
  return (RE (x) == RE (y) && IM (x) == IM (y));
}

/*!
\brief OP /= = (COMPLEX, COMPLEX) BOOL
**/

BOOL_T a68g_ne_complex (A68_REAL * x, A68_REAL * y)
{
  return (!(RE (x) == RE (y) && IM (x) == IM (y)));
}

/*!
\brief PROC csqrt = (COMPLEX) COMPLEX
**/

void a68g_sqrt_complex (A68_REAL * z, A68_REAL * x)
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  if (RE (x) == 0.0 && IM (x) == 0.0) {
    RE (z) = 0.0;
    IM (z) = 0.0;
  } else {
    double re = fabs (RE (x)), im = fabs (IM (x)), w;
    if (re >= im) {
      double t = im / re;
      w = sqrt (re) * sqrt (0.5 * (1.0 + sqrt (1.0 + t * t)));
    } else {
      double t = re / im;
      w = sqrt (im) * sqrt (0.5 * (t + sqrt (1.0 + t * t)));
    }
    if (RE (x) >= 0.0) {
      RE (z) = w;
      IM (z) = IM (x) / (2.0 * w);
    } else {
      double ai = IM (x);
      double vi = (ai >= 0.0 ? w : -w);
      RE (z) = ai / (2.0 * vi);
      IM (z) = vi;
    }
  }
}

/*!
\brief PROC cexp = (COMPLEX) COMPLEX
**/

void a68g_exp_complex (A68_REAL * z, A68_REAL * x)
{
  double r = exp (RE (x));
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  RE (z) = r * cos (IM (x));
  IM (z) = r * sin (IM (x));
}

/*!
\brief PROC cln = (COMPLEX) COMPLEX
**/

void a68g_ln_complex (A68_REAL * z, A68_REAL * x)
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  RE (z) = log (a68g_abs_complex (x));
  IM (z) = a68g_arg_complex (x);
}


/*!
\brief PROC csin = (COMPLEX) COMPLEX
**/

void a68g_sin_complex (A68_REAL * z, A68_REAL * x)
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  if (IM (x) == 0) {
    RE (z) = sin (RE (x));
    IM (z) = 0;
  } else {
    RE (z) = sin (RE (x)) * cosh (IM (x));
    IM (z) = cos (RE (x)) * sinh (IM (x));
  }
}

/*!
\brief PROC ccos = (COMPLEX) COMPLEX
**/

void a68g_cos_complex (A68_REAL * z, A68_REAL * x)
{
  STATUS_RE (z) = INITIALISED_MASK;
  STATUS_IM (z) = INITIALISED_MASK;
  if (IM (x) == 0) {
    RE (z) = cos (RE (x));
    IM (z) = 0;
  } else {
    RE (z) = cos (RE (x)) * cosh (IM (x));
    IM (z) = sin (RE (x)) * sinh (-IM (x));
  }
}

/*!
\brief PROC ctan = (COMPLEX) COMPLEX
**/

void a68g_tan_complex (A68_REAL * z, A68_REAL * x)
{
  A68_COMPLEX u, v;
  STATUS_RE (u) = INITIALISED_MASK;
  STATUS_IM (u) = INITIALISED_MASK;
  STATUS_RE (v) = INITIALISED_MASK;
  STATUS_IM (v) = INITIALISED_MASK;
  if (IM (x) == 0) {
    RE (u) = sin (RE (x));
    IM (u) = 0;
    RE (v) = cos (RE (x));
    IM (v) = 0;
  } else {
    RE (u) = sin (RE (x)) * cosh (IM (x));
    IM (u) = cos (RE (x)) * sinh (IM (x));
    RE (v) = cos (RE (x)) * cosh (IM (x));
    IM (v) = sin (RE (x)) * sinh (-IM (x));
  }
  a68g_div_complex (z, u, v);
}

/*!
\brief PROC casin = (COMPLEX) COMPLEX
**/

void a68g_arcsin_complex (A68_REAL * z, A68_REAL * x)
{
  double r = RE (x), i = IM (x);
  if (i == 0) {
    RE (z) = asin (r);
    IM (z) = 0;
  } else {
    double u = a68g_hypot (r + 1, i), v = a68g_hypot (r - 1, i);
    double a = 0.5 * (u + v), b = 0.5 * (u - v);
    RE (z) = asin (b);
    IM (z) = log (a + sqrt (a * a - 1));
  }

}

/*!
\brief PROC cacos = (COMPLEX) COMPLEX
**/

void a68g_arccos_complex (A68_REAL * z, A68_REAL * x)
{
  double r = RE (x), i = IM (x);
  if (i == 0) {
    RE (z) = acos (r);
    IM (z) = 0;
  } else {
    double u = a68g_hypot (r + 1, i), v = a68g_hypot (r - 1, i);
    double a = 0.5 * (u + v), b = 0.5 * (u - v);
    RE (z) = acos (b);
    IM (z) = -log (a + sqrt (a * a - 1));
  }

}

/*!
\brief PROC catan = (COMPLEX) COMPLEX
**/

void a68g_arctan_complex (A68_REAL * z, A68_REAL * x)
{
  double r = RE (x), i = IM (x);
  if (i == 0) {
    RE (z) = atan (r);
    IM (z) = 0;
  } else {
    double a = a68g_hypot (r, i + 1), b = a68g_hypot (r, i - 1);
    RE (z) = 0.5 * atan (2 * r / (1 - r * r - i * i));
    IM (z) = 0.5 * log (a / b);
  }
}
