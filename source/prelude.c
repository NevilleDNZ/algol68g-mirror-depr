/*!
\file prelude.c
\brief builds symbol table for standard prelude
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
#include "transput.h"
#include "mp.h"
#include "gsl.h"

SYMBOL_TABLE_T *stand_env;

static MOID_T *m, *proc_int, *proc_real, *proc_real_real, *proc_real_real_real, *proc_real_real_real_real, *proc_complex_complex, *proc_bool, *proc_char, *proc_void;
static PACK_T *z;

/*!
\brief enter tag in standenv symbol table
\param portable whether portable
\param a attribute
\param n node where defined
\param c name of token
\param m moid of token
\param p priority, if applicable
\param q interpreter routine that executes this token
**/

static void add_stand_env (BOOL_T portable, int a, NODE_T * n, char *c, MOID_T * m, int p, GENIE_PROCEDURE * q)
{
#define INSERT_TAG(l, n) {NEXT (n) = *(l); *(l) = (n);}
  TAG_T *new_one = new_tag ();
  INFO (n)->PROCEDURE_LEVEL = 0;
  new_one->use = A68_FALSE;
  HEAP (new_one) = HEAP_SYMBOL;
  SYMBOL_TABLE (new_one) = stand_env;
  NODE (new_one) = n;
  VALUE (new_one) = (c != NULL ? add_token (&top_token, c)->text : NULL);
  PRIO (new_one) = p;
  new_one->procedure = q;
  new_one->stand_env_proc = q != NULL;
  new_one->unit = NULL;
  new_one->portable = portable;
  MOID (new_one) = m;
  NEXT (new_one) = NULL;
  if (a == IDENTIFIER) {
    INSERT_TAG (&stand_env->identifiers, new_one);
  } else if (a == OP_SYMBOL) {
    INSERT_TAG (&stand_env->operators, new_one);
  } else if (a == PRIO_SYMBOL) {
    INSERT_TAG (&PRIO (stand_env), new_one);
  } else if (a == INDICANT) {
    INSERT_TAG (&stand_env->indicants, new_one);
  } else if (a == LABEL) {
    INSERT_TAG (&stand_env->labels, new_one);
  }
#undef INSERT_TAG
}

/*!
\brief compose PROC moid from arguments - first result, than arguments
\param m result moid
\return entry in mode table
**/

static MOID_T *a68_proc (MOID_T * m, ...)
{
  MOID_T *y, **z = &(stand_env->moids);
  PACK_T *p = NULL, *q = NULL;
  va_list attribute;
  va_start (attribute, m);
  while ((y = va_arg (attribute, MOID_T *)) != NULL) {
    PACK_T *new_one = new_pack ();
    MOID (new_one) = y;
    new_one->text = NULL;
    NEXT (new_one) = NULL;
    if (q != NULL) {
      NEXT (q) = new_one;
    } else {
      p = new_one;
    }
    q = new_one;
  }
  va_end (attribute);
  add_mode (z, PROC_SYMBOL, count_pack_members (p), NULL, m, p);
  return (*z);
}

/*!
\brief enter an identifier in standenv
\param n name of identifier
\param m mode of identifier
\param q interpreter routine that executes this token
**/

static void a68_idf (BOOL_T portable, char *n, MOID_T * m, GENIE_PROCEDURE * q)
{
  add_stand_env (portable, IDENTIFIER, some_node (TEXT (add_token (&top_token, n))), NULL, m, 0, q);
}

/*!
\brief enter a moid in standenv
\param p sizety
\param t name of moid
\param m will point to entry in mode table
**/

static void a68_mode (int p, char *t, MOID_T ** m)
{
  add_mode (&stand_env->moids, STANDARD, p, some_node (TEXT (find_keyword (top_keyword, t))), NULL, NULL);
  *m = stand_env->moids;
}

/*!
\brief enter a priority in standenv
\param p name of operator
\param b priority of operator
**/

static void a68_prio (char *p, int b)
{
  add_stand_env (A68_TRUE, PRIO_SYMBOL, some_node (TEXT (add_token (&top_token, p))), NULL, NULL, b, NULL);
}

/*!
\brief enter operator in standenv
\param n name of operator
\param m mode of operator
\param q interpreter routine that executes this token
**/

static void a68_op (BOOL_T portable, char *n, MOID_T * m, GENIE_PROCEDURE * q)
{
  add_stand_env (portable, OP_SYMBOL, some_node (TEXT (add_token (&top_token, n))), NULL, m, 0, q);
}

/*!
\brief enter standard modes in standenv
**/

static void stand_moids (void)
{
/* Primitive A68 moids. */
  a68_mode (0, "VOID", &MODE (VOID));
/* Standard precision. */
  a68_mode (0, "INT", &MODE (INT));
  a68_mode (0, "REAL", &MODE (REAL));
  a68_mode (0, "COMPLEX", &MODE (COMPLEX));
  a68_mode (0, "COMPL", &MODE (COMPL));
  a68_mode (0, "BITS", &MODE (BITS));
  a68_mode (0, "BYTES", &MODE (BYTES));
/* Multiple precision. */
  a68_mode (1, "INT", &MODE (LONG_INT));
  a68_mode (1, "REAL", &MODE (LONG_REAL));
  a68_mode (1, "COMPLEX", &MODE (LONG_COMPLEX));
  a68_mode (1, "COMPL", &MODE (LONG_COMPL));
  a68_mode (1, "BITS", &MODE (LONG_BITS));
  a68_mode (1, "BYTES", &MODE (LONG_BYTES));
  a68_mode (2, "REAL", &MODE (LONGLONG_REAL));
  a68_mode (2, "INT", &MODE (LONGLONG_INT));
  a68_mode (2, "COMPLEX", &MODE (LONGLONG_COMPLEX));
  a68_mode (2, "COMPL", &MODE (LONGLONG_COMPL));
  a68_mode (2, "BITS", &MODE (LONGLONG_BITS));
/* Other. */
  a68_mode (0, "BOOL", &MODE (BOOL));
  a68_mode (0, "CHAR", &MODE (CHAR));
  a68_mode (0, "STRING", &MODE (STRING));
  a68_mode (0, "FILE", &MODE (FILE));
  a68_mode (0, "CHANNEL", &MODE (CHANNEL));
  a68_mode (0, "PIPE", &MODE (PIPE));
  a68_mode (0, "FORMAT", &MODE (FORMAT));
  a68_mode (0, "SEMA", &MODE (SEMA));
  a68_mode (0, "SOUND", &MODE (SOUND));
  MODE (SOUND)->has_rows = A68_TRUE;
/* ROWS. */
  add_mode (&stand_env->moids, ROWS_SYMBOL, 0, NULL, NULL, NULL);
  MODE (ROWS) = stand_env->moids;
/* REFs. */
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (INT), NULL);
  MODE (REF_INT) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (REAL), NULL);
  MODE (REF_REAL) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (COMPLEX), NULL);
  MODE (REF_COMPLEX) = MODE (REF_COMPL) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (BITS), NULL);
  MODE (REF_BITS) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (BYTES), NULL);
  MODE (REF_BYTES) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (FORMAT), NULL);
  MODE (REF_FORMAT) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (PIPE), NULL);
  MODE (REF_PIPE) = stand_env->moids;
/* Multiple precision. */
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_INT), NULL);
  MODE (REF_LONG_INT) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_REAL), NULL);
  MODE (REF_LONG_REAL) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_COMPLEX), NULL);
  MODE (REF_LONG_COMPLEX) = MODE (REF_LONG_COMPL) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONGLONG_INT), NULL);
  MODE (REF_LONGLONG_INT) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONGLONG_REAL), NULL);
  MODE (REF_LONGLONG_REAL) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONGLONG_COMPLEX), NULL);
  MODE (REF_LONGLONG_COMPLEX) = MODE (REF_LONGLONG_COMPL) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_BITS), NULL);
  MODE (REF_LONG_BITS) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONGLONG_BITS), NULL);
  MODE (REF_LONGLONG_BITS) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_BYTES), NULL);
  MODE (REF_LONG_BYTES) = stand_env->moids;
/* Other. */
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (BOOL), NULL);
  MODE (REF_BOOL) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (CHAR), NULL);
  MODE (REF_CHAR) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (FILE), NULL);
  MODE (REF_FILE) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (REF_FILE), NULL);
  MODE (REF_REF_FILE) = stand_env->moids;
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (SOUND), NULL);
  MODE (REF_SOUND) = stand_env->moids;
/* [] INT. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (INT), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_INT) = stand_env->moids;
  SLICE (MODE (ROW_INT)) = MODE (INT);
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_INT), NULL);
  MODE (REF_ROW_INT) = stand_env->moids;
  NAME (MODE (REF_ROW_INT)) = MODE (REF_INT);
/* [] REAL. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (REAL), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_REAL) = stand_env->moids;
  SLICE (MODE (ROW_REAL)) = MODE (REAL);
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_REAL), NULL);
  MODE (REF_ROW_REAL) = stand_env->moids;
  NAME (MODE (REF_ROW_REAL)) = MODE (REF_REAL);
/* [,] REAL. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 2, NULL, MODE (REAL), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROWROW_REAL) = stand_env->moids;
  SLICE (MODE (ROWROW_REAL)) = MODE (ROW_REAL);
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROWROW_REAL), NULL);
  MODE (REF_ROWROW_REAL) = stand_env->moids;
  NAME (MODE (REF_ROWROW_REAL)) = MODE (REF_ROW_REAL);
/* [] COMPLEX. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (COMPLEX), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_COMPLEX) = stand_env->moids;
  SLICE (MODE (ROW_COMPLEX)) = MODE (COMPLEX);
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_COMPLEX), NULL);
  MODE (REF_ROW_COMPLEX) = stand_env->moids;
  NAME (MODE (REF_ROW_COMPLEX)) = MODE (REF_COMPLEX);
/* [,] COMPLEX. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 2, NULL, MODE (COMPLEX), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROWROW_COMPLEX) = stand_env->moids;
  SLICE (MODE (ROWROW_COMPLEX)) = MODE (ROW_COMPLEX);
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROWROW_COMPLEX), NULL);
  MODE (REF_ROWROW_COMPLEX) = stand_env->moids;
  NAME (MODE (REF_ROWROW_COMPLEX)) = MODE (REF_ROW_COMPLEX);
/* [] BOOL. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (BOOL), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_BOOL) = stand_env->moids;
  SLICE (MODE (ROW_BOOL)) = MODE (BOOL);
/* [] BITS. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (BITS), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_BITS) = stand_env->moids;
  SLICE (MODE (ROW_BITS)) = MODE (BITS);
/* [] LONG BITS. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (LONG_BITS), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_LONG_BITS) = stand_env->moids;
  SLICE (MODE (ROW_LONG_BITS)) = MODE (LONG_BITS);
/* [] LONG LONG BITS. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (LONGLONG_BITS), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_LONGLONG_BITS) = stand_env->moids;
  SLICE (MODE (ROW_LONGLONG_BITS)) = MODE (LONGLONG_BITS);
/* [] CHAR. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (CHAR), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_CHAR) = stand_env->moids;
  SLICE (MODE (ROW_CHAR)) = MODE (CHAR);
/* [][] CHAR. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (ROW_CHAR), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_ROW_CHAR) = stand_env->moids;
  SLICE (MODE (ROW_ROW_CHAR)) = MODE (ROW_CHAR);
/* MODE STRING = FLEX [] CHAR. */
  add_mode (&stand_env->moids, FLEX_SYMBOL, 0, NULL, MODE (ROW_CHAR), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  stand_env->moids->deflexed_mode = MODE (ROW_CHAR);
  stand_env->moids->trim = MODE (ROW_CHAR);
  EQUIVALENT (MODE (STRING)) = stand_env->moids;
  DEFLEXED (MODE (STRING)) = MODE (ROW_CHAR);
/* REF [] CHAR. */
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_CHAR), NULL);
  MODE (REF_ROW_CHAR) = stand_env->moids;
  NAME (MODE (REF_ROW_CHAR)) = MODE (REF_CHAR);
/* PROC [] CHAR. */
  add_mode (&stand_env->moids, PROC_SYMBOL, 0, NULL, MODE (ROW_CHAR), NULL);
  MODE (PROC_ROW_CHAR) = stand_env->moids;
/* REF STRING = REF FLEX [] CHAR. */
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, EQUIVALENT (MODE (STRING)), NULL);
  MODE (REF_STRING) = stand_env->moids;
  NAME (MODE (REF_STRING)) = MODE (REF_CHAR);
  DEFLEXED (MODE (REF_STRING)) = MODE (REF_ROW_CHAR);
  TRIM (MODE (REF_STRING)) = MODE (REF_ROW_CHAR);
/* [] STRING. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (STRING), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_STRING) = stand_env->moids;
  SLICE (MODE (ROW_STRING)) = MODE (STRING);
  DEFLEXED (MODE (ROW_STRING)) = MODE (ROW_ROW_CHAR);
/* PROC STRING. */
  add_mode (&stand_env->moids, PROC_SYMBOL, 0, NULL, MODE (STRING), NULL);
  MODE (PROC_STRING) = stand_env->moids;
  DEFLEXED (MODE (PROC_STRING)) = MODE (PROC_ROW_CHAR);
/* COMPLEX. */
  z = NULL;
  add_mode_to_pack (&z, MODE (REAL), add_token (&top_token, "im")->text, NULL);
  add_mode_to_pack (&z, MODE (REAL), add_token (&top_token, "re")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (COMPLEX)) = EQUIVALENT (MODE (COMPL)) = stand_env->moids;
  MODE (COMPLEX) = MODE (COMPL) = stand_env->moids;
  z = NULL;
  add_mode_to_pack (&z, MODE (REF_REAL), add_token (&top_token, "im")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_REAL), add_token (&top_token, "re")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_COMPLEX)) = NAME (MODE (REF_COMPL)) = stand_env->moids;
/* LONG COMPLEX. */
  z = NULL;
  add_mode_to_pack (&z, MODE (LONG_REAL), add_token (&top_token, "im")->text, NULL);
  add_mode_to_pack (&z, MODE (LONG_REAL), add_token (&top_token, "re")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (LONG_COMPLEX)) = EQUIVALENT (MODE (LONG_COMPL)) = stand_env->moids;
  MODE (LONG_COMPLEX) = MODE (LONG_COMPL) = stand_env->moids;
  z = NULL;
  add_mode_to_pack (&z, MODE (REF_LONG_REAL), add_token (&top_token, "im")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_LONG_REAL), add_token (&top_token, "re")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_LONG_COMPLEX)) = NAME (MODE (REF_LONG_COMPL)) = stand_env->moids;
/* LONG_LONG COMPLEX. */
  z = NULL;
  add_mode_to_pack (&z, MODE (LONGLONG_REAL), add_token (&top_token, "im")->text, NULL);
  add_mode_to_pack (&z, MODE (LONGLONG_REAL), add_token (&top_token, "re")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (LONGLONG_COMPLEX)) = EQUIVALENT (MODE (LONGLONG_COMPL)) = stand_env->moids;
  MODE (LONGLONG_COMPLEX) = MODE (LONGLONG_COMPL) = stand_env->moids;
  z = NULL;
  add_mode_to_pack (&z, MODE (REF_LONGLONG_REAL), add_token (&top_token, "im")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_LONGLONG_REAL), add_token (&top_token, "re")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_LONGLONG_COMPLEX)) = NAME (MODE (REF_LONGLONG_COMPL)) = stand_env->moids;
/* NUMBER. */
  z = NULL;
  add_mode_to_pack (&z, MODE (INT), NULL, NULL);
  add_mode_to_pack (&z, MODE (LONG_INT), NULL, NULL);
  add_mode_to_pack (&z, MODE (LONGLONG_INT), NULL, NULL);
  add_mode_to_pack (&z, MODE (REAL), NULL, NULL);
  add_mode_to_pack (&z, MODE (LONG_REAL), NULL, NULL);
  add_mode_to_pack (&z, MODE (LONGLONG_REAL), NULL, NULL);
  add_mode (&stand_env->moids, UNION_SYMBOL, count_pack_members (z), NULL, NULL, z);
  MODE (NUMBER) = stand_env->moids;
/* SEMA. */
  z = NULL;
  add_mode_to_pack (&z, MODE (REF_INT), NULL, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (SEMA)) = stand_env->moids;
  MODE (SEMA) = stand_env->moids;
/* PROC VOID. */
  z = NULL;
  add_mode (&stand_env->moids, PROC_SYMBOL, count_pack_members (z), NULL, MODE (VOID), z);
  MODE (PROC_VOID) = stand_env->moids;
/* IO: PROC (REF FILE) BOOL. */
  z = NULL;
  add_mode_to_pack (&z, MODE (REF_FILE), NULL, NULL);
  add_mode (&stand_env->moids, PROC_SYMBOL, count_pack_members (z), NULL, MODE (BOOL), z);
  MODE (PROC_REF_FILE_BOOL) = stand_env->moids;
/* IO: PROC (REF FILE) VOID. */
  z = NULL;
  add_mode_to_pack (&z, MODE (REF_FILE), NULL, NULL);
  add_mode (&stand_env->moids, PROC_SYMBOL, count_pack_members (z), NULL, MODE (VOID), z);
  MODE (PROC_REF_FILE_VOID) = stand_env->moids;
/* IO: SIMPLIN and SIMPLOUT. */
  add_mode (&stand_env->moids, IN_TYPE_MODE, 0, NULL, NULL, NULL);
  MODE (SIMPLIN) = stand_env->moids;
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (SIMPLIN), NULL);
  MODE (ROW_SIMPLIN) = stand_env->moids;
  SLICE (MODE (ROW_SIMPLIN)) = MODE (SIMPLIN);
  add_mode (&stand_env->moids, OUT_TYPE_MODE, 0, NULL, NULL, NULL);
  MODE (SIMPLOUT) = stand_env->moids;
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (SIMPLOUT), NULL);
  MODE (ROW_SIMPLOUT) = stand_env->moids;
  SLICE (MODE (ROW_SIMPLOUT)) = MODE (SIMPLOUT);
/* PIPE. */
  z = NULL;
  add_mode_to_pack (&z, MODE (INT), add_token (&top_token, "pid")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_FILE), add_token (&top_token, "write")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_FILE), add_token (&top_token, "read")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (PIPE)) = stand_env->moids;
  MODE (PIPE) = stand_env->moids;
  MODE (PIPE)->portable = A68_FALSE;
  z = NULL;
  add_mode_to_pack (&z, MODE (REF_INT), add_token (&top_token, "pid")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_REF_FILE), add_token (&top_token, "write")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_REF_FILE), add_token (&top_token, "read")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_PIPE)) = stand_env->moids;
}

/*!
\brief set up standenv - general RR but not transput
**/

static void stand_prelude (void)
{
/* Identifiers. */
  a68_idf (A68_TRUE, "intlengths", MODE (INT), genie_int_lengths);
  a68_idf (A68_TRUE, "intshorths", MODE (INT), genie_int_shorths);
  a68_idf (A68_TRUE, "maxint", MODE (INT), genie_max_int);
  a68_idf (A68_TRUE, "maxreal", MODE (REAL), genie_max_real);
  a68_idf (A68_TRUE, "smallreal", MODE (REAL), genie_small_real);
  a68_idf (A68_TRUE, "reallengths", MODE (INT), genie_real_lengths);
  a68_idf (A68_TRUE, "realshorths", MODE (INT), genie_real_shorths);
  a68_idf (A68_TRUE, "compllengths", MODE (INT), genie_complex_lengths);
  a68_idf (A68_TRUE, "complshorths", MODE (INT), genie_complex_shorths);
  a68_idf (A68_TRUE, "bitslengths", MODE (INT), genie_bits_lengths);
  a68_idf (A68_TRUE, "bitsshorths", MODE (INT), genie_bits_shorths);
  a68_idf (A68_TRUE, "bitswidth", MODE (INT), genie_bits_width);
  a68_idf (A68_TRUE, "longbitswidth", MODE (INT), genie_long_bits_width);
  a68_idf (A68_TRUE, "longlongbitswidth", MODE (INT), genie_longlong_bits_width);
  a68_idf (A68_TRUE, "maxbits", MODE (BITS), genie_max_bits);
  a68_idf (A68_TRUE, "longmaxbits", MODE (LONG_BITS), genie_long_max_bits);
  a68_idf (A68_TRUE, "longlongmaxbits", MODE (LONGLONG_BITS), genie_longlong_max_bits);
  a68_idf (A68_TRUE, "byteslengths", MODE (INT), genie_bytes_lengths);
  a68_idf (A68_TRUE, "bytesshorths", MODE (INT), genie_bytes_shorths);
  a68_idf (A68_TRUE, "byteswidth", MODE (INT), genie_bytes_width);
  a68_idf (A68_TRUE, "maxabschar", MODE (INT), genie_max_abs_char);
  a68_idf (A68_TRUE, "pi", MODE (REAL), genie_pi);
  a68_idf (A68_TRUE, "dpi", MODE (LONG_REAL), genie_pi_long_mp);
  a68_idf (A68_TRUE, "longpi", MODE (LONG_REAL), genie_pi_long_mp);
  a68_idf (A68_TRUE, "qpi", MODE (LONGLONG_REAL), genie_pi_long_mp);
  a68_idf (A68_TRUE, "longlongpi", MODE (LONGLONG_REAL), genie_pi_long_mp);
  a68_idf (A68_TRUE, "intwidth", MODE (INT), genie_int_width);
  a68_idf (A68_TRUE, "realwidth", MODE (INT), genie_real_width);
  a68_idf (A68_TRUE, "expwidth", MODE (INT), genie_exp_width);
  a68_idf (A68_TRUE, "longintwidth", MODE (INT), genie_long_int_width);
  a68_idf (A68_TRUE, "longlongintwidth", MODE (INT), genie_longlong_int_width);
  a68_idf (A68_TRUE, "longrealwidth", MODE (INT), genie_long_real_width);
  a68_idf (A68_TRUE, "longlongrealwidth", MODE (INT), genie_longlong_real_width);
  a68_idf (A68_TRUE, "longexpwidth", MODE (INT), genie_long_exp_width);
  a68_idf (A68_TRUE, "longlongexpwidth", MODE (INT), genie_longlong_exp_width);
  a68_idf (A68_TRUE, "longmaxint", MODE (LONG_INT), genie_long_max_int);
  a68_idf (A68_TRUE, "longlongmaxint", MODE (LONGLONG_INT), genie_longlong_max_int);
  a68_idf (A68_TRUE, "longsmallreal", MODE (LONG_REAL), genie_long_small_real);
  a68_idf (A68_TRUE, "longlongsmallreal", MODE (LONGLONG_REAL), genie_longlong_small_real);
  a68_idf (A68_TRUE, "longmaxreal", MODE (LONG_REAL), genie_long_max_real);
  a68_idf (A68_TRUE, "longlongmaxreal", MODE (LONGLONG_REAL), genie_longlong_max_real);
  a68_idf (A68_TRUE, "longbyteswidth", MODE (INT), genie_long_bytes_width);
  a68_idf (A68_FALSE, "seconds", MODE (REAL), genie_seconds);
  a68_idf (A68_FALSE, "clock", MODE (REAL), genie_cputime);
  a68_idf (A68_FALSE, "cputime", MODE (REAL), genie_cputime);
  m = proc_int;
  a68_idf (A68_FALSE, "collections", m, genie_garbage_collections);
  m = a68_proc (MODE (LONG_INT), NULL);
  a68_idf (A68_FALSE, "garbage", m, genie_garbage_freed);
  m = proc_real;
  a68_idf (A68_FALSE, "collectseconds", m, genie_garbage_seconds);
  a68_idf (A68_FALSE, "stackpointer", MODE (INT), genie_stack_pointer);
  a68_idf (A68_FALSE, "systemstackpointer", MODE (INT), genie_system_stack_pointer);
  a68_idf (A68_FALSE, "systemstacksize", MODE (INT), genie_system_stack_size);
  a68_idf (A68_FALSE, "actualstacksize", MODE (INT), genie_stack_pointer);
  m = proc_void;
  a68_idf (A68_FALSE, "sweepheap", m, genie_sweep_heap);
  a68_idf (A68_FALSE, "preemptivesweepheap", m, genie_preemptive_sweep_heap);
  a68_idf (A68_FALSE, "break", m, genie_break);
  a68_idf (A68_FALSE, "debug", m, genie_debug);
  a68_idf (A68_FALSE, "monitor", m, genie_debug);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "evaluate", m, genie_evaluate);
  m = a68_proc (MODE (INT), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "system", m, genie_system);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "acronym", m, genie_acronym);
  a68_idf (A68_FALSE, "vmsacronym", m, genie_acronym);
/* BITS procedures. */
  m = a68_proc (MODE (BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A68_TRUE, "bitspack", m, genie_bits_pack);
  m = a68_proc (MODE (LONG_BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A68_TRUE, "longbitspack", m, genie_long_bits_pack);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A68_TRUE, "longlongbitspack", m, genie_long_bits_pack);
/* RNG procedures. */
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A68_TRUE, "firstrandom", m, genie_first_random);
  m = proc_real;
  a68_idf (A68_TRUE, "nextrandom", m, genie_next_random);
  a68_idf (A68_TRUE, "random", m, genie_next_random);
  m = a68_proc (MODE (LONG_REAL), NULL);
  a68_idf (A68_TRUE, "longnextrandom", m, genie_long_next_random);
  a68_idf (A68_TRUE, "longrandom", m, genie_long_next_random);
  m = a68_proc (MODE (LONGLONG_REAL), NULL);
  a68_idf (A68_TRUE, "longlongnextrandom", m, genie_long_next_random);
  a68_idf (A68_TRUE, "longlongrandom", m, genie_long_next_random);
/* Priorities. */
  a68_prio ("+:=", 1);
  a68_prio ("-:=", 1);
  a68_prio ("*:=", 1);
  a68_prio ("/:=", 1);
  a68_prio ("%:=", 1);
  a68_prio ("%*:=", 1);
  a68_prio ("+=:", 1);
  a68_prio ("PLUSAB", 1);
  a68_prio ("MINUSAB", 1);
  a68_prio ("TIMESAB", 1);
  a68_prio ("DIVAB", 1);
  a68_prio ("OVERAB", 1);
  a68_prio ("MODAB", 1);
  a68_prio ("PLUSTO", 1);
  a68_prio ("OR", 2);
  a68_prio ("AND", 3);
  a68_prio ("&", 3);
  a68_prio ("XOR", 3);
  a68_prio ("=", 4);
  a68_prio ("/=", 4);
  a68_prio ("~=", 4);
  a68_prio ("^=", 4);
  a68_prio ("<", 5);
  a68_prio ("<=", 5);
  a68_prio (">", 5);
  a68_prio (">=", 5);
  a68_prio ("EQ", 4);
  a68_prio ("NE", 4);
  a68_prio ("LT", 5);
  a68_prio ("LE", 5);
  a68_prio ("GT", 5);
  a68_prio ("GE", 5);
  a68_prio ("+", 6);
  a68_prio ("-", 6);
  a68_prio ("*", 7);
  a68_prio ("/", 7);
  a68_prio ("OVER", 7);
  a68_prio ("%", 7);
  a68_prio ("MOD", 7);
  a68_prio ("%*", 7);
  a68_prio ("ELEM", 7);
  a68_prio ("SET", 7);
  a68_prio ("CLEAR", 7);
  a68_prio ("**", 8);
  a68_prio ("SHL", 8);
  a68_prio ("SHR", 8);
  a68_prio ("UP", 8);
  a68_prio ("DOWN", 8);
  a68_prio ("^", 8);
  a68_prio ("ELEMS", 8);
  a68_prio ("LWB", 8);
  a68_prio ("UPB", 8);
  a68_prio ("SORT", 8);
  a68_prio ("I", 9);
  a68_prio ("+*", 9);
/* INT ops. */
  m = a68_proc (MODE (INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_int);
  a68_op (A68_TRUE, "ABS", m, genie_abs_int);
  a68_op (A68_TRUE, "SIGN", m, genie_sign_int);
  m = a68_proc (MODE (BOOL), MODE (INT), NULL);
  a68_op (A68_TRUE, "ODD", m, genie_odd_int);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_int);
  a68_op (A68_TRUE, "/=", m, genie_ne_int);
  a68_op (A68_TRUE, "~=", m, genie_ne_int);
  a68_op (A68_TRUE, "^=", m, genie_ne_int);
  a68_op (A68_TRUE, "<", m, genie_lt_int);
  a68_op (A68_TRUE, "<=", m, genie_le_int);
  a68_op (A68_TRUE, ">", m, genie_gt_int);
  a68_op (A68_TRUE, ">=", m, genie_ge_int);
  a68_op (A68_TRUE, "EQ", m, genie_eq_int);
  a68_op (A68_TRUE, "NE", m, genie_ne_int);
  a68_op (A68_TRUE, "LT", m, genie_lt_int);
  a68_op (A68_TRUE, "LE", m, genie_le_int);
  a68_op (A68_TRUE, "GT", m, genie_gt_int);
  a68_op (A68_TRUE, "GE", m, genie_ge_int);
  m = a68_proc (MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_int);
  a68_op (A68_TRUE, "-", m, genie_sub_int);
  a68_op (A68_TRUE, "*", m, genie_mul_int);
  a68_op (A68_TRUE, "OVER", m, genie_over_int);
  a68_op (A68_TRUE, "%", m, genie_over_int);
  a68_op (A68_TRUE, "MOD", m, genie_mod_int);
  a68_op (A68_TRUE, "%*", m, genie_mod_int);
  a68_op (A68_TRUE, "**", m, genie_pow_int);
  a68_op (A68_TRUE, "UP", m, genie_pow_int);
  a68_op (A68_TRUE, "^", m, genie_pow_int);
  m = a68_proc (MODE (REAL), MODE (INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "/", m, genie_div_int);
  m = a68_proc (MODE (REF_INT), MODE (REF_INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_int);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_int);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_int);
  a68_op (A68_TRUE, "%:=", m, genie_overab_int);
  a68_op (A68_TRUE, "%*:=", m, genie_modab_int);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_int);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_int);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_int);
  a68_op (A68_TRUE, "OVERAB", m, genie_overab_int);
  a68_op (A68_TRUE, "MODAB", m, genie_modab_int);
/* REAL ops. */
  m = proc_real_real;
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_real);
  a68_op (A68_TRUE, "ABS", m, genie_abs_real);
  m = a68_proc (MODE (INT), MODE (REAL), NULL);
  a68_op (A68_TRUE, "SIGN", m, genie_sign_real);
  a68_op (A68_TRUE, "ROUND", m, genie_round_real);
  a68_op (A68_TRUE, "ENTIER", m, genie_entier_real);
  m = a68_proc (MODE (BOOL), MODE (REAL), MODE (REAL), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_real);
  a68_op (A68_TRUE, "/=", m, genie_ne_real);
  a68_op (A68_TRUE, "~=", m, genie_ne_real);
  a68_op (A68_TRUE, "^=", m, genie_ne_real);
  a68_op (A68_TRUE, "<", m, genie_lt_real);
  a68_op (A68_TRUE, "<=", m, genie_le_real);
  a68_op (A68_TRUE, ">", m, genie_gt_real);
  a68_op (A68_TRUE, ">=", m, genie_ge_real);
  a68_op (A68_TRUE, "EQ", m, genie_eq_real);
  a68_op (A68_TRUE, "NE", m, genie_ne_real);
  a68_op (A68_TRUE, "LT", m, genie_lt_real);
  a68_op (A68_TRUE, "LE", m, genie_le_real);
  a68_op (A68_TRUE, "GT", m, genie_gt_real);
  a68_op (A68_TRUE, "GE", m, genie_ge_real);
  m = proc_real_real_real;
  a68_op (A68_TRUE, "+", m, genie_add_real);
  a68_op (A68_TRUE, "-", m, genie_sub_real);
  a68_op (A68_TRUE, "*", m, genie_mul_real);
  a68_op (A68_TRUE, "/", m, genie_div_real);
  a68_op (A68_TRUE, "**", m, genie_pow_real);
  a68_op (A68_TRUE, "UP", m, genie_pow_real);
  a68_op (A68_TRUE, "^", m, genie_pow_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (INT), NULL);
  a68_op (A68_TRUE, "**", m, genie_pow_real_int);
  a68_op (A68_TRUE, "UP", m, genie_pow_real_int);
  a68_op (A68_TRUE, "^", m, genie_pow_real_int);
  m = a68_proc (MODE (REF_REAL), MODE (REF_REAL), MODE (REAL), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_real);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_real);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_real);
  a68_op (A68_TRUE, "/:=", m, genie_divab_real);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_real);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_real);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_real);
  a68_op (A68_TRUE, "DIVAB", m, genie_divab_real);
  m = proc_real_real;
  a68_idf (A68_TRUE, "sqrt", m, genie_sqrt_real);
  a68_idf (A68_FALSE, "cbrt", m, genie_curt_real);
  a68_idf (A68_FALSE, "curt", m, genie_curt_real);
  a68_idf (A68_TRUE, "exp", m, genie_exp_real);
  a68_idf (A68_TRUE, "ln", m, genie_ln_real);
  a68_idf (A68_TRUE, "log", m, genie_log_real);
  a68_idf (A68_TRUE, "sin", m, genie_sin_real);
  a68_idf (A68_TRUE, "cos", m, genie_cos_real);
  a68_idf (A68_TRUE, "tan", m, genie_tan_real);
  a68_idf (A68_TRUE, "asin", m, genie_arcsin_real);
  a68_idf (A68_TRUE, "acos", m, genie_arccos_real);
  a68_idf (A68_TRUE, "atan", m, genie_arctan_real);
  a68_idf (A68_TRUE, "arcsin", m, genie_arcsin_real);
  a68_idf (A68_TRUE, "arccos", m, genie_arccos_real);
  a68_idf (A68_TRUE, "arctan", m, genie_arctan_real);
  a68_idf (A68_FALSE, "sinh", m, genie_sinh_real);
  a68_idf (A68_FALSE, "cosh", m, genie_cosh_real);
  a68_idf (A68_FALSE, "tanh", m, genie_tanh_real);
  a68_idf (A68_FALSE, "asinh", m, genie_arcsinh_real);
  a68_idf (A68_FALSE, "acosh", m, genie_arccosh_real);
  a68_idf (A68_FALSE, "atanh", m, genie_arctanh_real);
  a68_idf (A68_FALSE, "arcsinh", m, genie_arcsinh_real);
  a68_idf (A68_FALSE, "arccosh", m, genie_arccosh_real);
  a68_idf (A68_FALSE, "arctanh", m, genie_arctanh_real);
  a68_idf (A68_FALSE, "inverseerf", m, genie_inverf_real);
  a68_idf (A68_FALSE, "inverseerfc", m, genie_inverfc_real);
  m = proc_real_real_real;
  a68_idf (A68_FALSE, "arctan2", m, genie_atan2_real);
  m = proc_real_real_real_real;
  a68_idf (A68_FALSE, "lje126", m, genie_lj_e_12_6);
  a68_idf (A68_FALSE, "ljf126", m, genie_lj_f_12_6);
/* COMPLEX ops. */
  m = a68_proc (MODE (COMPLEX), MODE (REAL), MODE (REAL), NULL);
  a68_op (A68_TRUE, "I", m, genie_icomplex);
  a68_op (A68_TRUE, "+*", m, genie_icomplex);
  m = a68_proc (MODE (COMPLEX), MODE (INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "I", m, genie_iint_complex);
  a68_op (A68_TRUE, "+*", m, genie_iint_complex);
  m = a68_proc (MODE (REAL), MODE (COMPLEX), NULL);
  a68_op (A68_TRUE, "RE", m, genie_re_complex);
  a68_op (A68_TRUE, "IM", m, genie_im_complex);
  a68_op (A68_TRUE, "ABS", m, genie_abs_complex);
  a68_op (A68_TRUE, "ARG", m, genie_arg_complex);
  m = proc_complex_complex;
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_complex);
  a68_op (A68_TRUE, "CONJ", m, genie_conj_complex);
  m = a68_proc (MODE (BOOL), MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_complex);
  a68_op (A68_TRUE, "/=", m, genie_ne_complex);
  a68_op (A68_TRUE, "~=", m, genie_ne_complex);
  a68_op (A68_TRUE, "^=", m, genie_ne_complex);
  a68_op (A68_TRUE, "EQ", m, genie_eq_complex);
  a68_op (A68_TRUE, "NE", m, genie_ne_complex);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_complex);
  a68_op (A68_TRUE, "-", m, genie_sub_complex);
  a68_op (A68_TRUE, "*", m, genie_mul_complex);
  a68_op (A68_TRUE, "/", m, genie_div_complex);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), MODE (INT), NULL);
  a68_op (A68_TRUE, "**", m, genie_pow_complex_int);
  a68_op (A68_TRUE, "UP", m, genie_pow_complex_int);
  a68_op (A68_TRUE, "^", m, genie_pow_complex_int);
  m = a68_proc (MODE (REF_COMPLEX), MODE (REF_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_complex);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_complex);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_complex);
  a68_op (A68_TRUE, "/:=", m, genie_divab_complex);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_complex);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_complex);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_complex);
  a68_op (A68_TRUE, "DIVAB", m, genie_divab_complex);
/* BOOL ops. */
  m = a68_proc (MODE (BOOL), MODE (BOOL), NULL);
  a68_op (A68_TRUE, "NOT", m, genie_not_bool);
  a68_op (A68_TRUE, "~", m, genie_not_bool);
  m = a68_proc (MODE (INT), MODE (BOOL), NULL);
  a68_op (A68_TRUE, "ABS", m, genie_abs_bool);
  m = a68_proc (MODE (BOOL), MODE (BOOL), MODE (BOOL), NULL);
  a68_op (A68_TRUE, "OR", m, genie_or_bool);
  a68_op (A68_TRUE, "AND", m, genie_and_bool);
  a68_op (A68_TRUE, "&", m, genie_and_bool);
  a68_op (A68_FALSE, "XOR", m, genie_xor_bool);
  a68_op (A68_TRUE, "=", m, genie_eq_bool);
  a68_op (A68_TRUE, "/=", m, genie_ne_bool);
  a68_op (A68_TRUE, "~=", m, genie_ne_bool);
  a68_op (A68_TRUE, "^=", m, genie_ne_bool);
  a68_op (A68_TRUE, "EQ", m, genie_eq_bool);
  a68_op (A68_TRUE, "NE", m, genie_ne_bool);
/* CHAR ops. */
  m = a68_proc (MODE (BOOL), MODE (CHAR), MODE (CHAR), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_char);
  a68_op (A68_TRUE, "/=", m, genie_ne_char);
  a68_op (A68_TRUE, "~=", m, genie_ne_char);
  a68_op (A68_TRUE, "^=", m, genie_ne_char);
  a68_op (A68_TRUE, "<", m, genie_lt_char);
  a68_op (A68_TRUE, "<=", m, genie_le_char);
  a68_op (A68_TRUE, ">", m, genie_gt_char);
  a68_op (A68_TRUE, ">=", m, genie_ge_char);
  a68_op (A68_TRUE, "EQ", m, genie_eq_char);
  a68_op (A68_TRUE, "NE", m, genie_ne_char);
  a68_op (A68_TRUE, "LT", m, genie_lt_char);
  a68_op (A68_TRUE, "LE", m, genie_le_char);
  a68_op (A68_TRUE, "GT", m, genie_gt_char);
  a68_op (A68_TRUE, "GE", m, genie_ge_char);
  m = a68_proc (MODE (INT), MODE (CHAR), NULL);
  a68_op (A68_TRUE, "ABS", m, genie_abs_char);
  m = a68_proc (MODE (CHAR), MODE (INT), NULL);
  a68_op (A68_TRUE, "REPR", m, genie_repr_char);
  m = a68_proc (MODE (BOOL), MODE (CHAR), NULL);
  a68_idf (A68_FALSE, "isalnum", m, genie_is_alnum);
  a68_idf (A68_FALSE, "isalpha", m, genie_is_alpha);
  a68_idf (A68_FALSE, "iscntrl", m, genie_is_cntrl);
  a68_idf (A68_FALSE, "isdigit", m, genie_is_digit);
  a68_idf (A68_FALSE, "isgraph", m, genie_is_graph);
  a68_idf (A68_FALSE, "islower", m, genie_is_lower);
  a68_idf (A68_FALSE, "isprint", m, genie_is_print);
  a68_idf (A68_FALSE, "ispunct", m, genie_is_punct);
  a68_idf (A68_FALSE, "isspace", m, genie_is_space);
  a68_idf (A68_FALSE, "isupper", m, genie_is_upper);
  a68_idf (A68_FALSE, "isxdigit", m, genie_is_xdigit);
  m = a68_proc (MODE (CHAR), MODE (CHAR), NULL);
  a68_idf (A68_FALSE, "tolower", m, genie_to_lower);
  a68_idf (A68_FALSE, "toupper", m, genie_to_upper);
/* BITS ops. */
  m = a68_proc (MODE (INT), MODE (BITS), NULL);
  a68_op (A68_TRUE, "ABS", m, genie_idle);
  m = a68_proc (MODE (BITS), MODE (INT), NULL);
  a68_op (A68_TRUE, "BIN", m, genie_bin_int);
  m = a68_proc (MODE (BITS), MODE (BITS), NULL);
  a68_op (A68_TRUE, "NOT", m, genie_not_bits);
  a68_op (A68_TRUE, "~", m, genie_not_bits);
  m = a68_proc (MODE (BOOL), MODE (BITS), MODE (BITS), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_bits);
  a68_op (A68_TRUE, "/=", m, genie_ne_bits);
  a68_op (A68_TRUE, "~=", m, genie_ne_bits);
  a68_op (A68_TRUE, "^=", m, genie_ne_bits);
  a68_op (A68_TRUE, "<=", m, genie_le_bits);
  a68_op (A68_TRUE, ">=", m, genie_ge_bits);
  a68_op (A68_TRUE, "EQ", m, genie_eq_bits);
  a68_op (A68_TRUE, "NE", m, genie_ne_bits);
  a68_op (A68_TRUE, "LE", m, genie_le_bits);
  a68_op (A68_TRUE, "GE", m, genie_ge_bits);
  m = a68_proc (MODE (BITS), MODE (BITS), MODE (BITS), NULL);
  a68_op (A68_TRUE, "AND", m, genie_and_bits);
  a68_op (A68_TRUE, "&", m, genie_and_bits);
  a68_op (A68_TRUE, "OR", m, genie_or_bits);
  a68_op (A68_FALSE, "XOR", m, genie_xor_bits);
  m = a68_proc (MODE (BITS), MODE (BITS), MODE (INT), NULL);
  a68_op (A68_TRUE, "SHL", m, genie_shl_bits);
  a68_op (A68_TRUE, "UP", m, genie_shl_bits);
  a68_op (A68_TRUE, "SHR", m, genie_shr_bits);
  a68_op (A68_TRUE, "DOWN", m, genie_shr_bits);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (BITS), NULL);
  a68_op (A68_TRUE, "ELEM", m, genie_elem_bits);
  m = a68_proc (MODE (BITS), MODE (INT), MODE (BITS), NULL);
  a68_op (A68_TRUE, "SET", m, genie_set_bits);
  a68_op (A68_TRUE, "CLEAR", m, genie_clear_bits);
/* BYTES ops. */
  m = a68_proc (MODE (BYTES), MODE (STRING), NULL);
  a68_idf (A68_TRUE, "bytespack", m, genie_bytespack);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (BYTES), NULL);
  a68_op (A68_TRUE, "ELEM", m, genie_elem_bytes);
  m = a68_proc (MODE (BYTES), MODE (BYTES), MODE (BYTES), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_bytes);
  m = a68_proc (MODE (REF_BYTES), MODE (REF_BYTES), MODE (BYTES), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_bytes);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_bytes);
  m = a68_proc (MODE (REF_BYTES), MODE (BYTES), MODE (REF_BYTES), NULL);
  a68_op (A68_TRUE, "+=:", m, genie_plusto_bytes);
  a68_op (A68_TRUE, "PLUSTO", m, genie_plusto_bytes);
  m = a68_proc (MODE (BOOL), MODE (BYTES), MODE (BYTES), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_bytes);
  a68_op (A68_TRUE, "/=", m, genie_ne_bytes);
  a68_op (A68_TRUE, "~=", m, genie_ne_bytes);
  a68_op (A68_TRUE, "^=", m, genie_ne_bytes);
  a68_op (A68_TRUE, "<", m, genie_lt_bytes);
  a68_op (A68_TRUE, "<=", m, genie_le_bytes);
  a68_op (A68_TRUE, ">", m, genie_gt_bytes);
  a68_op (A68_TRUE, ">=", m, genie_ge_bytes);
  a68_op (A68_TRUE, "EQ", m, genie_eq_bytes);
  a68_op (A68_TRUE, "NE", m, genie_ne_bytes);
  a68_op (A68_TRUE, "LT", m, genie_lt_bytes);
  a68_op (A68_TRUE, "LE", m, genie_le_bytes);
  a68_op (A68_TRUE, "GT", m, genie_gt_bytes);
  a68_op (A68_TRUE, "GE", m, genie_ge_bytes);
/* LONG BYTES ops. */
  m = a68_proc (MODE (LONG_BYTES), MODE (BYTES), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_leng_bytes);
  m = a68_proc (MODE (BYTES), MODE (LONG_BYTES), NULL);
  a68_idf (A68_TRUE, "SHORTEN", m, genie_shorten_bytes);
  m = a68_proc (MODE (LONG_BYTES), MODE (STRING), NULL);
  a68_idf (A68_TRUE, "longbytespack", m, genie_long_bytespack);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (LONG_BYTES), NULL);
  a68_op (A68_TRUE, "ELEM", m, genie_elem_long_bytes);
  m = a68_proc (MODE (LONG_BYTES), MODE (LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_long_bytes);
  m = a68_proc (MODE (REF_LONG_BYTES), MODE (REF_LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_long_bytes);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_long_bytes);
  m = a68_proc (MODE (REF_LONG_BYTES), MODE (LONG_BYTES), MODE (REF_LONG_BYTES), NULL);
  a68_op (A68_TRUE, "+=:", m, genie_plusto_long_bytes);
  a68_op (A68_TRUE, "PLUSTO", m, genie_plusto_long_bytes);
  m = a68_proc (MODE (BOOL), MODE (LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_bytes);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_bytes);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_bytes);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_bytes);
  a68_op (A68_TRUE, "<", m, genie_lt_long_bytes);
  a68_op (A68_TRUE, "<=", m, genie_le_long_bytes);
  a68_op (A68_TRUE, ">", m, genie_gt_long_bytes);
  a68_op (A68_TRUE, ">=", m, genie_ge_long_bytes);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_bytes);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_bytes);
  a68_op (A68_TRUE, "LT", m, genie_lt_long_bytes);
  a68_op (A68_TRUE, "LE", m, genie_le_long_bytes);
  a68_op (A68_TRUE, "GT", m, genie_gt_long_bytes);
  a68_op (A68_TRUE, "GE", m, genie_ge_long_bytes);
/* STRING ops. */
  m = a68_proc (MODE (BOOL), MODE (STRING), MODE (STRING), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_string);
  a68_op (A68_TRUE, "/=", m, genie_ne_string);
  a68_op (A68_TRUE, "~=", m, genie_ne_string);
  a68_op (A68_TRUE, "^=", m, genie_ne_string);
  a68_op (A68_TRUE, "<", m, genie_lt_string);
  a68_op (A68_TRUE, "<=", m, genie_le_string);
  a68_op (A68_TRUE, ">=", m, genie_ge_string);
  a68_op (A68_TRUE, ">", m, genie_gt_string);
  a68_op (A68_TRUE, "EQ", m, genie_eq_string);
  a68_op (A68_TRUE, "NE", m, genie_ne_string);
  a68_op (A68_TRUE, "LT", m, genie_lt_string);
  a68_op (A68_TRUE, "LE", m, genie_le_string);
  a68_op (A68_TRUE, "GE", m, genie_ge_string);
  a68_op (A68_TRUE, "GT", m, genie_gt_string);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (STRING), NULL);
  a68_op (A68_TRUE, "ELEM", m, genie_elem_string);
  m = a68_proc (MODE (STRING), MODE (CHAR), MODE (CHAR), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_char);
  m = a68_proc (MODE (STRING), MODE (STRING), MODE (STRING), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_string);
  m = a68_proc (MODE (REF_STRING), MODE (REF_STRING), MODE (STRING), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_string);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_string);
  m = a68_proc (MODE (REF_STRING), MODE (REF_STRING), MODE (INT), NULL);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_string);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_string);
  m = a68_proc (MODE (REF_STRING), MODE (STRING), MODE (REF_STRING), NULL);
  a68_op (A68_TRUE, "+=:", m, genie_plusto_string);
  a68_op (A68_TRUE, "PLUSTO", m, genie_plusto_string);
  m = a68_proc (MODE (STRING), MODE (STRING), MODE (INT), NULL);
  a68_op (A68_TRUE, "*", m, genie_times_string_int);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (STRING), NULL);
  a68_op (A68_TRUE, "*", m, genie_times_int_string);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (CHAR), NULL);
  a68_op (A68_TRUE, "*", m, genie_times_int_char);
  m = a68_proc (MODE (STRING), MODE (CHAR), MODE (INT), NULL);
  a68_op (A68_TRUE, "*", m, genie_times_char_int);
/* [] CHAR as cross term for STRING. */
  m = a68_proc (MODE (BOOL), MODE (ROW_CHAR), MODE (ROW_CHAR), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_string);
  a68_op (A68_TRUE, "/=", m, genie_ne_string);
  a68_op (A68_TRUE, "~=", m, genie_ne_string);
  a68_op (A68_TRUE, "^=", m, genie_ne_string);
  a68_op (A68_TRUE, "<", m, genie_lt_string);
  a68_op (A68_TRUE, "<=", m, genie_le_string);
  a68_op (A68_TRUE, ">=", m, genie_ge_string);
  a68_op (A68_TRUE, ">", m, genie_gt_string);
  a68_op (A68_TRUE, "EQ", m, genie_eq_string);
  a68_op (A68_TRUE, "NE", m, genie_ne_string);
  a68_op (A68_TRUE, "LT", m, genie_lt_string);
  a68_op (A68_TRUE, "LE", m, genie_le_string);
  a68_op (A68_TRUE, "GE", m, genie_ge_string);
  a68_op (A68_TRUE, "GT", m, genie_gt_string);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (ROW_CHAR), NULL);
  a68_op (A68_TRUE, "ELEM", m, genie_elem_string);
  m = a68_proc (MODE (STRING), MODE (ROW_CHAR), MODE (ROW_CHAR), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_string);
  m = a68_proc (MODE (STRING), MODE (ROW_CHAR), MODE (INT), NULL);
  a68_op (A68_TRUE, "*", m, genie_times_string_int);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (ROW_CHAR), NULL);
  a68_op (A68_TRUE, "*", m, genie_times_int_string);
/* SEMA ops. */
#if defined ENABLE_PAR_CLAUSE
  m = a68_proc (MODE (SEMA), MODE (INT), NULL);
  a68_op (A68_TRUE, "LEVEL", m, genie_level_sema_int);
  m = a68_proc (MODE (INT), MODE (SEMA), NULL);
  a68_op (A68_TRUE, "LEVEL", m, genie_level_int_sema);
  m = a68_proc (MODE (VOID), MODE (SEMA), NULL);
  a68_op (A68_TRUE, "UP", m, genie_up_sema);
  a68_op (A68_TRUE, "DOWN", m, genie_down_sema);
#else
  m = a68_proc (MODE (SEMA), MODE (INT), NULL);
  a68_op (A68_TRUE, "LEVEL", m, genie_unimplemented);
  m = a68_proc (MODE (INT), MODE (SEMA), NULL);
  a68_op (A68_TRUE, "LEVEL", m, genie_unimplemented);
  m = a68_proc (MODE (VOID), MODE (SEMA), NULL);
  a68_op (A68_TRUE, "UP", m, genie_unimplemented);
  a68_op (A68_TRUE, "DOWN", m, genie_unimplemented);
#endif
/* ROWS ops. */
  m = a68_proc (MODE (INT), MODE (ROWS), NULL);
  a68_op (A68_FALSE, "ELEMS", m, genie_monad_elems);
  a68_op (A68_TRUE, "LWB", m, genie_monad_lwb);
  a68_op (A68_TRUE, "UPB", m, genie_monad_upb);
  m = a68_proc (MODE (INT), MODE (INT), MODE (ROWS), NULL);
  a68_op (A68_FALSE, "ELEMS", m, genie_dyad_elems);
  a68_op (A68_TRUE, "LWB", m, genie_dyad_lwb);
  a68_op (A68_TRUE, "UPB", m, genie_dyad_upb);
  m = a68_proc (MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_op (A68_FALSE, "SORT", m, genie_sort_row_string);
/* Binding for the multiple-precision library. */
/* LONG INT. */
  m = a68_proc (MODE (LONG_INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_lengthen_int_to_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_long_mp);
  a68_op (A68_TRUE, "ABS", m, genie_abs_long_mp);
  m = a68_proc (MODE (INT), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_shorten_long_mp_to_int);
  a68_op (A68_TRUE, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "ODD", m, genie_odd_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "ENTIER", m, genie_entier_long_mp);
  a68_op (A68_TRUE, "ROUND", m, genie_round_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_long_int);
  a68_op (A68_TRUE, "-", m, genie_minus_long_int);
  a68_op (A68_TRUE, "*", m, genie_mul_long_int);
  a68_op (A68_TRUE, "OVER", m, genie_over_long_mp);
  a68_op (A68_TRUE, "%", m, genie_over_long_mp);
  a68_op (A68_TRUE, "MOD", m, genie_mod_long_mp);
  a68_op (A68_TRUE, "%*", m, genie_mod_long_mp);
  m = a68_proc (MODE (REF_LONG_INT), MODE (REF_LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_long_int);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_long_int);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_long_int);
  a68_op (A68_TRUE, "%:=", m, genie_overab_long_mp);
  a68_op (A68_TRUE, "%*:=", m, genie_modab_long_mp);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_long_int);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_long_int);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_long_int);
  a68_op (A68_TRUE, "OVERAB", m, genie_overab_long_mp);
  a68_op (A68_TRUE, "MODAB", m, genie_modab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A68_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A68_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A68_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A68_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A68_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A68_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A68_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "/", m, genie_div_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "**", m, genie_pow_long_mp_int_int);
  a68_op (A68_TRUE, "^", m, genie_pow_long_mp_int_int);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "I", m, genie_idle);
  a68_op (A68_TRUE, "+*", m, genie_idle);
/* LONG REAL. */
  m = a68_proc (MODE (LONG_REAL), MODE (REAL), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_lengthen_real_to_long_mp);
  m = a68_proc (MODE (REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_shorten_long_mp_to_real);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_long_mp);
  a68_op (A68_TRUE, "ABS", m, genie_abs_long_mp);
  a68_idf (A68_TRUE, "longsqrt", m, genie_sqrt_long_mp);
  a68_idf (A68_FALSE, "longcbrt", m, genie_curt_long_mp);
  a68_idf (A68_FALSE, "longcurt", m, genie_curt_long_mp);
  a68_idf (A68_TRUE, "longexp", m, genie_exp_long_mp);
  a68_idf (A68_TRUE, "longln", m, genie_ln_long_mp);
  a68_idf (A68_TRUE, "longlog", m, genie_log_long_mp);
  a68_idf (A68_TRUE, "longsin", m, genie_sin_long_mp);
  a68_idf (A68_TRUE, "longcos", m, genie_cos_long_mp);
  a68_idf (A68_TRUE, "longtan", m, genie_tan_long_mp);
  a68_idf (A68_TRUE, "longasin", m, genie_asin_long_mp);
  a68_idf (A68_TRUE, "longacos", m, genie_acos_long_mp);
  a68_idf (A68_TRUE, "longatan", m, genie_atan_long_mp);
  a68_idf (A68_TRUE, "longarcsin", m, genie_asin_long_mp);
  a68_idf (A68_TRUE, "longarccos", m, genie_acos_long_mp);
  a68_idf (A68_TRUE, "longarctan", m, genie_atan_long_mp);
  a68_idf (A68_FALSE, "longsinh", m, genie_sinh_long_mp);
  a68_idf (A68_FALSE, "longcosh", m, genie_cosh_long_mp);
  a68_idf (A68_FALSE, "longtanh", m, genie_tanh_long_mp);
  a68_idf (A68_FALSE, "longasinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_FALSE, "longacosh", m, genie_arccosh_long_mp);
  a68_idf (A68_FALSE, "longatanh", m, genie_arctanh_long_mp);
  a68_idf (A68_FALSE, "longarcsinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_FALSE, "longarccosh", m, genie_arccosh_long_mp);
  a68_idf (A68_FALSE, "longarctanh", m, genie_arctanh_long_mp);
  a68_idf (A68_FALSE, "dsqrt", m, genie_sqrt_long_mp);
  a68_idf (A68_FALSE, "dcbrt", m, genie_curt_long_mp);
  a68_idf (A68_FALSE, "dcurt", m, genie_curt_long_mp);
  a68_idf (A68_FALSE, "dexp", m, genie_exp_long_mp);
  a68_idf (A68_FALSE, "dln", m, genie_ln_long_mp);
  a68_idf (A68_FALSE, "dlog", m, genie_log_long_mp);
  a68_idf (A68_FALSE, "dsin", m, genie_sin_long_mp);
  a68_idf (A68_FALSE, "dcos", m, genie_cos_long_mp);
  a68_idf (A68_FALSE, "dtan", m, genie_tan_long_mp);
  a68_idf (A68_FALSE, "dasin", m, genie_asin_long_mp);
  a68_idf (A68_FALSE, "dacos", m, genie_acos_long_mp);
  a68_idf (A68_FALSE, "datan", m, genie_atan_long_mp);
  a68_idf (A68_FALSE, "dsinh", m, genie_sinh_long_mp);
  a68_idf (A68_FALSE, "dcosh", m, genie_cosh_long_mp);
  a68_idf (A68_FALSE, "dtanh", m, genie_tanh_long_mp);
  a68_idf (A68_FALSE, "dasinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_FALSE, "dacosh", m, genie_arccosh_long_mp);
  a68_idf (A68_FALSE, "datanh", m, genie_arctanh_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_idf (A68_TRUE, "longarctan2", m, genie_atan2_long_mp);
  a68_idf (A68_TRUE, "darctan2", m, genie_atan2_long_mp);
  m = a68_proc (MODE (INT), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_long_mp);
  a68_op (A68_TRUE, "-", m, genie_sub_long_mp);
  a68_op (A68_TRUE, "*", m, genie_mul_long_mp);
  a68_op (A68_TRUE, "/", m, genie_div_long_mp);
  a68_op (A68_TRUE, "**", m, genie_pow_long_mp);
  a68_op (A68_TRUE, "UP", m, genie_pow_long_mp);
  a68_op (A68_TRUE, "^", m, genie_pow_long_mp);
  m = a68_proc (MODE (REF_LONG_REAL), MODE (REF_LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_long_mp);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_long_mp);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_long_mp);
  a68_op (A68_TRUE, "/:=", m, genie_divab_long_mp);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_long_mp);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_long_mp);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_long_mp);
  a68_op (A68_TRUE, "DIVAB", m, genie_divab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A68_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A68_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A68_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A68_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A68_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A68_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A68_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (INT), NULL);
  a68_op (A68_TRUE, "**", m, genie_pow_long_mp_int);
  a68_op (A68_TRUE, "UP", m, genie_pow_long_mp_int);
  a68_op (A68_TRUE, "^", m, genie_pow_long_mp_int);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "I", m, genie_idle);
  a68_op (A68_TRUE, "+*", m, genie_idle);
/* LONG COMPLEX. */
  m = a68_proc (MODE (LONG_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_lengthen_complex_to_long_complex);
  m = a68_proc (MODE (COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_shorten_long_complex_to_complex);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "RE", m, genie_re_long_complex);
  a68_op (A68_TRUE, "IM", m, genie_im_long_complex);
  a68_op (A68_TRUE, "ARG", m, genie_arg_long_complex);
  a68_op (A68_TRUE, "ABS", m, genie_abs_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_long_complex);
  a68_op (A68_TRUE, "CONJ", m, genie_conj_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_long_complex);
  a68_op (A68_TRUE, "-", m, genie_sub_long_complex);
  a68_op (A68_TRUE, "*", m, genie_mul_long_complex);
  a68_op (A68_TRUE, "/", m, genie_div_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), MODE (INT), NULL);
  a68_op (A68_TRUE, "**", m, genie_pow_long_complex_int);
  a68_op (A68_TRUE, "UP", m, genie_pow_long_complex_int);
  a68_op (A68_TRUE, "^", m, genie_pow_long_complex_int);
  m = a68_proc (MODE (BOOL), MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_complex);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_complex);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_complex);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_complex);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_complex);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_complex);
  m = a68_proc (MODE (REF_LONG_COMPLEX), MODE (REF_LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_long_complex);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_long_complex);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_long_complex);
  a68_op (A68_TRUE, "/:=", m, genie_divab_long_complex);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_long_complex);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_long_complex);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_long_complex);
  a68_op (A68_TRUE, "DIVAB", m, genie_divab_long_complex);
/* LONG BITS ops. */
  m = a68_proc (MODE (LONG_INT), MODE (LONG_BITS), NULL);
  a68_op (A68_TRUE, "ABS", m, genie_idle);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "BIN", m, genie_bin_long_mp);
  m = a68_proc (MODE (BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_shorten_long_mp_to_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (BITS), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_lengthen_unsigned_to_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_TRUE, "NOT", m, genie_not_long_mp);
  a68_op (A68_TRUE, "~", m, genie_not_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "<=", m, genie_le_long_bits);
  a68_op (A68_TRUE, "LE", m, genie_le_long_bits);
  a68_op (A68_TRUE, ">=", m, genie_ge_long_bits);
  a68_op (A68_TRUE, "GE", m, genie_ge_long_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_TRUE, "AND", m, genie_and_long_mp);
  a68_op (A68_TRUE, "&", m, genie_and_long_mp);
  a68_op (A68_TRUE, "OR", m, genie_or_long_mp);
  a68_op (A68_FALSE, "XOR", m, genie_xor_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), MODE (INT), NULL);
  a68_op (A68_TRUE, "SHL", m, genie_shl_long_mp);
  a68_op (A68_TRUE, "UP", m, genie_shl_long_mp);
  a68_op (A68_TRUE, "SHR", m, genie_shr_long_mp);
  a68_op (A68_TRUE, "DOWN", m, genie_shr_long_mp);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (LONG_BITS), NULL);
  a68_op (A68_TRUE, "ELEM", m, genie_elem_long_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (INT), MODE (LONG_BITS), NULL);
  a68_op (A68_TRUE, "SET", m, genie_set_long_bits);
  a68_op (A68_TRUE, "CLEAR", m, genie_clear_long_bits);
/* LONG LONG INT. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_long_mp);
  a68_op (A68_TRUE, "ABS", m, genie_abs_long_mp);
  m = a68_proc (MODE (INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "ODD", m, genie_odd_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_TRUE, "ENTIER", m, genie_entier_long_mp);
  a68_op (A68_TRUE, "ROUND", m, genie_round_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_long_int);
  a68_op (A68_TRUE, "-", m, genie_minus_long_int);
  a68_op (A68_TRUE, "*", m, genie_mul_long_int);
  a68_op (A68_TRUE, "OVER", m, genie_over_long_mp);
  a68_op (A68_TRUE, "%", m, genie_over_long_mp);
  a68_op (A68_TRUE, "MOD", m, genie_mod_long_mp);
  a68_op (A68_TRUE, "%*", m, genie_mod_long_mp);
  m = a68_proc (MODE (REF_LONGLONG_INT), MODE (REF_LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_long_int);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_long_int);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_long_int);
  a68_op (A68_TRUE, "%:=", m, genie_overab_long_mp);
  a68_op (A68_TRUE, "%*:=", m, genie_modab_long_mp);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_long_int);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_long_int);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_long_int);
  a68_op (A68_TRUE, "OVERAB", m, genie_overab_long_mp);
  a68_op (A68_TRUE, "MODAB", m, genie_modab_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "/", m, genie_div_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A68_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A68_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A68_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A68_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A68_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A68_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A68_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "**", m, genie_pow_long_mp_int_int);
  a68_op (A68_TRUE, "^", m, genie_pow_long_mp_int_int);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "I", m, genie_idle);
  a68_op (A68_TRUE, "+*", m, genie_idle);
/* LONG LONG REAL. */
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_long_mp);
  a68_op (A68_TRUE, "ABS", m, genie_abs_long_mp);
  a68_idf (A68_TRUE, "longlongsqrt", m, genie_sqrt_long_mp);
  a68_idf (A68_FALSE, "longlongcbrt", m, genie_curt_long_mp);
  a68_idf (A68_FALSE, "longlongcurt", m, genie_curt_long_mp);
  a68_idf (A68_TRUE, "longlongexp", m, genie_exp_long_mp);
  a68_idf (A68_TRUE, "longlongln", m, genie_ln_long_mp);
  a68_idf (A68_TRUE, "longlonglog", m, genie_log_long_mp);
  a68_idf (A68_TRUE, "longlongsin", m, genie_sin_long_mp);
  a68_idf (A68_TRUE, "longlongcos", m, genie_cos_long_mp);
  a68_idf (A68_TRUE, "longlongtan", m, genie_tan_long_mp);
  a68_idf (A68_TRUE, "longlongasin", m, genie_asin_long_mp);
  a68_idf (A68_TRUE, "longlongacos", m, genie_acos_long_mp);
  a68_idf (A68_TRUE, "longlongatan", m, genie_atan_long_mp);
  a68_idf (A68_TRUE, "longlongarcsin", m, genie_asin_long_mp);
  a68_idf (A68_TRUE, "longlongarccos", m, genie_acos_long_mp);
  a68_idf (A68_TRUE, "longlongarctan", m, genie_atan_long_mp);
  a68_idf (A68_FALSE, "longlongsinh", m, genie_sinh_long_mp);
  a68_idf (A68_FALSE, "longlongcosh", m, genie_cosh_long_mp);
  a68_idf (A68_FALSE, "longlongtanh", m, genie_tanh_long_mp);
  a68_idf (A68_FALSE, "longlongasinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_FALSE, "longlongacosh", m, genie_arccosh_long_mp);
  a68_idf (A68_FALSE, "longlongatanh", m, genie_arctanh_long_mp);
  a68_idf (A68_FALSE, "longlongarcsinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_FALSE, "longlongarccosh", m, genie_arccosh_long_mp);
  a68_idf (A68_FALSE, "longlongarctanh", m, genie_arctanh_long_mp);
  a68_idf (A68_FALSE, "qsqrt", m, genie_sqrt_long_mp);
  a68_idf (A68_FALSE, "qcbrt", m, genie_curt_long_mp);
  a68_idf (A68_FALSE, "qcurt", m, genie_curt_long_mp);
  a68_idf (A68_FALSE, "qexp", m, genie_exp_long_mp);
  a68_idf (A68_FALSE, "qln", m, genie_ln_long_mp);
  a68_idf (A68_FALSE, "qlog", m, genie_log_long_mp);
  a68_idf (A68_FALSE, "qsin", m, genie_sin_long_mp);
  a68_idf (A68_FALSE, "qcos", m, genie_cos_long_mp);
  a68_idf (A68_FALSE, "qtan", m, genie_tan_long_mp);
  a68_idf (A68_FALSE, "qasin", m, genie_asin_long_mp);
  a68_idf (A68_FALSE, "qacos", m, genie_acos_long_mp);
  a68_idf (A68_FALSE, "qatan", m, genie_atan_long_mp);
  a68_idf (A68_FALSE, "qsinh", m, genie_sinh_long_mp);
  a68_idf (A68_FALSE, "qcosh", m, genie_cosh_long_mp);
  a68_idf (A68_FALSE, "qtanh", m, genie_tanh_long_mp);
  a68_idf (A68_FALSE, "qasinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_FALSE, "qacosh", m, genie_arccosh_long_mp);
  a68_idf (A68_FALSE, "qatanh", m, genie_arctanh_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_idf (A68_TRUE, "longarctan2", m, genie_atan2_long_mp);
  a68_idf (A68_TRUE, "qarctan2", m, genie_atan2_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_long_mp);
  a68_op (A68_TRUE, "-", m, genie_sub_long_mp);
  a68_op (A68_TRUE, "*", m, genie_mul_long_mp);
  a68_op (A68_TRUE, "/", m, genie_div_long_mp);
  a68_op (A68_TRUE, "**", m, genie_pow_long_mp);
  a68_op (A68_TRUE, "UP", m, genie_pow_long_mp);
  a68_op (A68_TRUE, "^", m, genie_pow_long_mp);
  m = a68_proc (MODE (REF_LONGLONG_REAL), MODE (REF_LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_long_mp);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_long_mp);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_long_mp);
  a68_op (A68_TRUE, "/:=", m, genie_divab_long_mp);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_long_mp);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_long_mp);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_long_mp);
  a68_op (A68_TRUE, "DIVAB", m, genie_divab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A68_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A68_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A68_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A68_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A68_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A68_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A68_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (INT), NULL);
  a68_op (A68_TRUE, "**", m, genie_pow_long_mp_int);
  a68_op (A68_TRUE, "UP", m, genie_pow_long_mp_int);
  a68_op (A68_TRUE, "^", m, genie_pow_long_mp_int);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_TRUE, "I", m, genie_idle);
  a68_op (A68_TRUE, "+*", m, genie_idle);
/* LONGLONG COMPLEX. */
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_lengthen_long_complex_to_longlong_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_shorten_longlong_complex_to_long_complex);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "RE", m, genie_re_long_complex);
  a68_op (A68_TRUE, "IM", m, genie_im_long_complex);
  a68_op (A68_TRUE, "ARG", m, genie_arg_long_complex);
  a68_op (A68_TRUE, "ABS", m, genie_abs_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "+", m, genie_idle);
  a68_op (A68_TRUE, "-", m, genie_minus_long_complex);
  a68_op (A68_TRUE, "CONJ", m, genie_conj_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "+", m, genie_add_long_complex);
  a68_op (A68_TRUE, "-", m, genie_sub_long_complex);
  a68_op (A68_TRUE, "*", m, genie_mul_long_complex);
  a68_op (A68_TRUE, "/", m, genie_div_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), MODE (INT), NULL);
  a68_op (A68_TRUE, "**", m, genie_pow_long_complex_int);
  a68_op (A68_TRUE, "UP", m, genie_pow_long_complex_int);
  a68_op (A68_TRUE, "^", m, genie_pow_long_complex_int);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_complex);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_complex);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_complex);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_complex);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_complex);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_complex);
  m = a68_proc (MODE (REF_LONGLONG_COMPLEX), MODE (REF_LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "+:=", m, genie_plusab_long_complex);
  a68_op (A68_TRUE, "-:=", m, genie_minusab_long_complex);
  a68_op (A68_TRUE, "*:=", m, genie_timesab_long_complex);
  a68_op (A68_TRUE, "/:=", m, genie_divab_long_complex);
  a68_op (A68_TRUE, "PLUSAB", m, genie_plusab_long_complex);
  a68_op (A68_TRUE, "MINUSAB", m, genie_minusab_long_complex);
  a68_op (A68_TRUE, "TIMESAB", m, genie_timesab_long_complex);
  a68_op (A68_TRUE, "DIVAB", m, genie_divab_long_complex);
/* LONG LONG BITS. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_TRUE, "ABS", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "BIN", m, genie_bin_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_TRUE, "NOT", m, genie_not_long_mp);
  a68_op (A68_TRUE, "~", m, genie_not_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A68_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "^=", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A68_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A68_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A68_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A68_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_TRUE, "AND", m, genie_and_long_mp);
  a68_op (A68_TRUE, "&", m, genie_and_long_mp);
  a68_op (A68_TRUE, "OR", m, genie_or_long_mp);
  a68_op (A68_FALSE, "XOR", m, genie_xor_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), MODE (INT), NULL);
  a68_op (A68_TRUE, "SHL", m, genie_shl_long_mp);
  a68_op (A68_TRUE, "UP", m, genie_shl_long_mp);
  a68_op (A68_TRUE, "SHR", m, genie_shr_long_mp);
  a68_op (A68_TRUE, "DOWN", m, genie_shr_long_mp);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_TRUE, "ELEM", m, genie_elem_longlong_bits);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (INT), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_TRUE, "SET", m, genie_set_longlong_bits);
  a68_op (A68_TRUE, "CLEAR", m, genie_clear_longlong_bits);
/* Some "terminators" to handle the mapping of very short or very long modes.
   This allows you to write SHORT REAL z = SHORTEN pi while everything is
   silently mapped onto REAL. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_TRUE, "LENG", m, genie_idle);
  m = a68_proc (MODE (INT), MODE (INT), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (REAL), MODE (REAL), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (BITS), MODE (BITS), NULL);
  a68_op (A68_TRUE, "SHORTEN", m, genie_idle);
  m = proc_complex_complex;
  a68_idf (A68_FALSE, "complexsqrt", m, genie_sqrt_complex);
  a68_idf (A68_FALSE, "csqrt", m, genie_sqrt_complex);
  a68_idf (A68_FALSE, "complexexp", m, genie_exp_complex);
  a68_idf (A68_FALSE, "cexp", m, genie_exp_complex);
  a68_idf (A68_FALSE, "complexln", m, genie_ln_complex);
  a68_idf (A68_FALSE, "cln", m, genie_ln_complex);
  a68_idf (A68_FALSE, "complexsin", m, genie_sin_complex);
  a68_idf (A68_FALSE, "csin", m, genie_sin_complex);
  a68_idf (A68_FALSE, "complexcos", m, genie_cos_complex);
  a68_idf (A68_FALSE, "ccos", m, genie_cos_complex);
  a68_idf (A68_FALSE, "complextan", m, genie_tan_complex);
  a68_idf (A68_FALSE, "ctan", m, genie_tan_complex);
  a68_idf (A68_FALSE, "complexasin", m, genie_arcsin_complex);
  a68_idf (A68_FALSE, "casin", m, genie_arcsin_complex);
  a68_idf (A68_FALSE, "complexacos", m, genie_arccos_complex);
  a68_idf (A68_FALSE, "cacos", m, genie_arccos_complex);
  a68_idf (A68_FALSE, "complexatan", m, genie_arctan_complex);
  a68_idf (A68_FALSE, "catan", m, genie_arctan_complex);
  a68_idf (A68_FALSE, "complexarcsin", m, genie_arcsin_complex);
  a68_idf (A68_FALSE, "carcsin", m, genie_arcsin_complex);
  a68_idf (A68_FALSE, "complexarccos", m, genie_arccos_complex);
  a68_idf (A68_FALSE, "carccos", m, genie_arccos_complex);
  a68_idf (A68_FALSE, "complexarctan", m, genie_arctan_complex);
  a68_idf (A68_FALSE, "carctan", m, genie_arctan_complex);
#if defined ENABLE_NUMERICAL
  a68_idf (A68_FALSE, "complexsinh", m, genie_sinh_complex);
  a68_idf (A68_FALSE, "csinh", m, genie_sinh_complex);
  a68_idf (A68_FALSE, "complexcosh", m, genie_cosh_complex);
  a68_idf (A68_FALSE, "ccosh", m, genie_cosh_complex);
  a68_idf (A68_FALSE, "complextanh", m, genie_tanh_complex);
  a68_idf (A68_FALSE, "ctanh", m, genie_tanh_complex);
  a68_idf (A68_FALSE, "complexasinh", m, genie_arcsinh_complex);
  a68_idf (A68_FALSE, "casinh", m, genie_arcsinh_complex);
  a68_idf (A68_FALSE, "complexacosh", m, genie_arccosh_complex);
  a68_idf (A68_FALSE, "cacosh", m, genie_arccosh_complex);
  a68_idf (A68_FALSE, "complexatanh", m, genie_arctanh_complex);
  a68_idf (A68_FALSE, "catanh", m, genie_arctanh_complex);
  a68_idf (A68_FALSE, "complexarcsinh", m, genie_arcsinh_complex);
  a68_idf (A68_FALSE, "carcsinh", m, genie_arcsinh_complex);
  a68_idf (A68_FALSE, "complexarccosh", m, genie_arccosh_complex);
  a68_idf (A68_FALSE, "carccosh", m, genie_arccosh_complex);
  a68_idf (A68_FALSE, "complexarctanh", m, genie_arctanh_complex);
  a68_idf (A68_FALSE, "carctanh", m, genie_arctanh_complex);
#endif
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_idf (A68_FALSE, "longcomplexsqrt", m, genie_sqrt_long_complex);
  a68_idf (A68_FALSE, "dcsqrt", m, genie_sqrt_long_complex);
  a68_idf (A68_FALSE, "longcomplexexp", m, genie_exp_long_complex);
  a68_idf (A68_FALSE, "dcexp", m, genie_exp_long_complex);
  a68_idf (A68_FALSE, "longcomplexln", m, genie_ln_long_complex);
  a68_idf (A68_FALSE, "dcln", m, genie_ln_long_complex);
  a68_idf (A68_FALSE, "longcomplexsin", m, genie_sin_long_complex);
  a68_idf (A68_FALSE, "dcsin", m, genie_sin_long_complex);
  a68_idf (A68_FALSE, "longcomplexcos", m, genie_cos_long_complex);
  a68_idf (A68_FALSE, "dccos", m, genie_cos_long_complex);
  a68_idf (A68_FALSE, "longcomplextan", m, genie_tan_long_complex);
  a68_idf (A68_FALSE, "dctan", m, genie_tan_long_complex);
  a68_idf (A68_FALSE, "longcomplexarcsin", m, genie_asin_long_complex);
  a68_idf (A68_FALSE, "dcasin", m, genie_asin_long_complex);
  a68_idf (A68_FALSE, "longcomplexarccos", m, genie_acos_long_complex);
  a68_idf (A68_FALSE, "dcacos", m, genie_acos_long_complex);
  a68_idf (A68_FALSE, "longcomplexarctan", m, genie_atan_long_complex);
  a68_idf (A68_FALSE, "dcatan", m, genie_atan_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A68_FALSE, "longlongcomplexsqrt", m, genie_sqrt_long_complex);
  a68_idf (A68_FALSE, "qcsqrt", m, genie_sqrt_long_complex);
  a68_idf (A68_FALSE, "longlongcomplexexp", m, genie_exp_long_complex);
  a68_idf (A68_FALSE, "qcexp", m, genie_exp_long_complex);
  a68_idf (A68_FALSE, "longlongcomplexln", m, genie_ln_long_complex);
  a68_idf (A68_FALSE, "qcln", m, genie_ln_long_complex);
  a68_idf (A68_FALSE, "longlongcomplexsin", m, genie_sin_long_complex);
  a68_idf (A68_FALSE, "qcsin", m, genie_sin_long_complex);
  a68_idf (A68_FALSE, "longlongcomplexcos", m, genie_cos_long_complex);
  a68_idf (A68_FALSE, "qccos", m, genie_cos_long_complex);
  a68_idf (A68_FALSE, "longlongcomplextan", m, genie_tan_long_complex);
  a68_idf (A68_FALSE, "qctan", m, genie_tan_long_complex);
  a68_idf (A68_FALSE, "longlongcomplexarcsin", m, genie_asin_long_complex);
  a68_idf (A68_FALSE, "qcasin", m, genie_asin_long_complex);
  a68_idf (A68_FALSE, "longlongcomplexarccos", m, genie_acos_long_complex);
  a68_idf (A68_FALSE, "qcacos", m, genie_acos_long_complex);
  a68_idf (A68_FALSE, "longlongcomplexarctan", m, genie_atan_long_complex);
  a68_idf (A68_FALSE, "qcatan", m, genie_atan_long_complex);
/* SOUND/RIFF procs. */
  m = a68_proc (MODE (SOUND), MODE (INT), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_FALSE, "newsound", m, genie_new_sound);
  m = a68_proc (MODE (INT), MODE (SOUND), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_FALSE, "getsound", m, genie_get_sound);
  m = a68_proc (MODE (VOID), MODE (SOUND), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_FALSE, "setsound", m, genie_set_sound);
  m = a68_proc (MODE (INT), MODE (SOUND), NULL);
  a68_op (A68_FALSE, "RESOLUTION", m, genie_sound_resolution);
  a68_op (A68_FALSE, "CHANNELS", m, genie_sound_channels);
  a68_op (A68_FALSE, "RATE", m, genie_sound_rate);
  a68_op (A68_FALSE, "SAMPLES", m, genie_sound_samples);
}

/*!
\brief set up standenv - transput
**/

static void stand_transput (void)
{
  a68_idf (A68_TRUE, "errorchar", MODE (CHAR), genie_error_char);
  a68_idf (A68_TRUE, "expchar", MODE (CHAR), genie_exp_char);
  a68_idf (A68_TRUE, "flip", MODE (CHAR), genie_flip_char);
  a68_idf (A68_TRUE, "flop", MODE (CHAR), genie_flop_char);
  a68_idf (A68_FALSE, "blankcharacter", MODE (CHAR), genie_blank_char);
  a68_idf (A68_TRUE, "blankchar", MODE (CHAR), genie_blank_char);
  a68_idf (A68_TRUE, "blank", MODE (CHAR), genie_blank_char);
  a68_idf (A68_FALSE, "nullcharacter", MODE (CHAR), genie_null_char);
  a68_idf (A68_TRUE, "nullchar", MODE (CHAR), genie_null_char);
  a68_idf (A68_FALSE, "newlinecharacter", MODE (CHAR), genie_newline_char);
  a68_idf (A68_FALSE, "newlinechar", MODE (CHAR), genie_newline_char);
  a68_idf (A68_FALSE, "formfeedcharacter", MODE (CHAR), genie_formfeed_char);
  a68_idf (A68_FALSE, "formfeedchar", MODE (CHAR), genie_formfeed_char);
  a68_idf (A68_FALSE, "tabcharacter", MODE (CHAR), genie_tab_char);
  a68_idf (A68_FALSE, "tabchar", MODE (CHAR), genie_tab_char);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), NULL);
  a68_idf (A68_TRUE, "whole", m, genie_whole);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_TRUE, "fixed", m, genie_fixed);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_TRUE, "float", m, genie_float);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_TRUE, "real", m, genie_real);
  a68_idf (A68_TRUE, "standin", MODE (REF_FILE), genie_stand_in);
  a68_idf (A68_TRUE, "standout", MODE (REF_FILE), genie_stand_out);
  a68_idf (A68_TRUE, "standback", MODE (REF_FILE), genie_stand_back);
  a68_idf (A68_FALSE, "standerror", MODE (REF_FILE), genie_stand_error);
  a68_idf (A68_TRUE, "standinchannel", MODE (CHANNEL), genie_stand_in_channel);
  a68_idf (A68_TRUE, "standoutchannel", MODE (CHANNEL), genie_stand_out_channel);
  a68_idf (A68_FALSE, "standdrawchannel", MODE (CHANNEL), genie_stand_draw_channel);
  a68_idf (A68_TRUE, "standbackchannel", MODE (CHANNEL), genie_stand_back_channel);
  a68_idf (A68_FALSE, "standerrorchannel", MODE (CHANNEL), genie_stand_error_channel);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A68_TRUE, "maketerm", m, genie_make_term);
  m = a68_proc (MODE (BOOL), MODE (CHAR), MODE (REF_INT), MODE (STRING), NULL);
  a68_idf (A68_TRUE, "charinstring", m, genie_char_in_string);
  a68_idf (A68_FALSE, "lastcharinstring", m, genie_last_char_in_string);
  m = a68_proc (MODE (BOOL), MODE (STRING), MODE (REF_INT), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "stringinstring", m, genie_string_in_string);
  m = a68_proc (MODE (STRING), MODE (REF_FILE), NULL);
  a68_idf (A68_FALSE, "idf", m, genie_idf);
  a68_idf (A68_FALSE, "term", m, genie_term);
  m = a68_proc (MODE (STRING), NULL);
  a68_idf (A68_FALSE, "programidf", m, genie_program_idf);
/* Event routines. */
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (PROC_REF_FILE_BOOL), NULL);
  a68_idf (A68_TRUE, "onfileend", m, genie_on_file_end);
  a68_idf (A68_TRUE, "onpageend", m, genie_on_page_end);
  a68_idf (A68_TRUE, "onlineend", m, genie_on_line_end);
  a68_idf (A68_TRUE, "onlogicalfileend", m, genie_on_file_end);
  a68_idf (A68_TRUE, "onphysicalfileend", m, genie_on_file_end);
  a68_idf (A68_TRUE, "onformatend", m, genie_on_format_end);
  a68_idf (A68_TRUE, "onformaterror", m, genie_on_format_error);
  a68_idf (A68_TRUE, "onvalueerror", m, genie_on_value_error);
  a68_idf (A68_TRUE, "onopenerror", m, genie_on_open_error);
  a68_idf (A68_FALSE, "ontransputerror", m, genie_on_transput_error);
/* Enquiries on files. */
  a68_idf (A68_TRUE, "putpossible", MODE (PROC_REF_FILE_BOOL), genie_put_possible);
  a68_idf (A68_TRUE, "getpossible", MODE (PROC_REF_FILE_BOOL), genie_get_possible);
  a68_idf (A68_TRUE, "binpossible", MODE (PROC_REF_FILE_BOOL), genie_bin_possible);
  a68_idf (A68_TRUE, "setpossible", MODE (PROC_REF_FILE_BOOL), genie_set_possible);
  a68_idf (A68_TRUE, "resetpossible", MODE (PROC_REF_FILE_BOOL), genie_reset_possible);
  a68_idf (A68_TRUE, "reidfpossible", MODE (PROC_REF_FILE_BOOL), genie_reidf_possible);
  a68_idf (A68_FALSE, "drawpossible", MODE (PROC_REF_FILE_BOOL), genie_draw_possible);
  a68_idf (A68_TRUE, "compressible", MODE (PROC_REF_FILE_BOOL), genie_compressible);
/* Handling of files. */
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), MODE (CHANNEL), NULL);
  a68_idf (A68_TRUE, "open", m, genie_open);
  a68_idf (A68_TRUE, "establish", m, genie_establish);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REF_STRING), NULL);
  a68_idf (A68_TRUE, "associate", m, genie_associate);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (CHANNEL), NULL);
  a68_idf (A68_TRUE, "create", m, genie_create);
  a68_idf (A68_TRUE, "close", MODE (PROC_REF_FILE_VOID), genie_close);
  a68_idf (A68_TRUE, "lock", MODE (PROC_REF_FILE_VOID), genie_lock);
  a68_idf (A68_TRUE, "scratch", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A68_TRUE, "erase", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A68_TRUE, "reset", MODE (PROC_REF_FILE_VOID), genie_reset);
  a68_idf (A68_TRUE, "scratch", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A68_TRUE, "newline", MODE (PROC_REF_FILE_VOID), genie_new_line);
  a68_idf (A68_TRUE, "newpage", MODE (PROC_REF_FILE_VOID), genie_new_page);
  a68_idf (A68_TRUE, "space", MODE (PROC_REF_FILE_VOID), genie_space);
  a68_idf (A68_TRUE, "backspace", MODE (PROC_REF_FILE_VOID), genie_backspace);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A68_TRUE, "set", m, genie_set);
  a68_idf (A68_TRUE, "seek", m, genie_set);
  m = a68_proc (MODE (VOID), MODE (ROW_SIMPLIN), NULL);
  a68_idf (A68_TRUE, "read", m, genie_read);
  a68_idf (A68_TRUE, "readbin", m, genie_read_bin);
  a68_idf (A68_TRUE, "readf", m, genie_read_format);
  m = a68_proc (MODE (VOID), MODE (ROW_SIMPLOUT), NULL);
  a68_idf (A68_TRUE, "print", m, genie_write);
  a68_idf (A68_TRUE, "write", m, genie_write);
  a68_idf (A68_TRUE, "printbin", m, genie_write_bin);
  a68_idf (A68_TRUE, "writebin", m, genie_write_bin);
  a68_idf (A68_TRUE, "printf", m, genie_write_format);
  a68_idf (A68_TRUE, "writef", m, genie_write_format);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_SIMPLIN), NULL);
  a68_idf (A68_TRUE, "get", m, genie_read_file);
  a68_idf (A68_TRUE, "getf", m, genie_read_file_format);
  a68_idf (A68_TRUE, "getbin", m, genie_read_bin_file);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_SIMPLOUT), NULL);
  a68_idf (A68_TRUE, "put", m, genie_write_file);
  a68_idf (A68_TRUE, "putf", m, genie_write_file_format);
  a68_idf (A68_TRUE, "putbin", m, genie_write_bin_file);
/* ALGOL68C type procs. */
  m = proc_int;
  a68_idf (A68_FALSE, "readint", m, genie_read_int);
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A68_FALSE, "printint", m, genie_print_int);
  m = a68_proc (MODE (LONG_INT), NULL);
  a68_idf (A68_FALSE, "readlongint", m, genie_read_long_int);
  m = a68_proc (MODE (VOID), MODE (LONG_INT), NULL);
  a68_idf (A68_FALSE, "printlongint", m, genie_print_long_int);
  m = a68_proc (MODE (LONGLONG_INT), NULL);
  a68_idf (A68_FALSE, "readlonglongint", m, genie_read_longlong_int);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_INT), NULL);
  a68_idf (A68_FALSE, "printlonglongint", m, genie_print_longlong_int);
  m = proc_real;
  a68_idf (A68_FALSE, "readreal", m, genie_read_real);
  m = a68_proc (MODE (VOID), MODE (REAL), NULL);
  a68_idf (A68_FALSE, "printreal", m, genie_print_real);
  m = a68_proc (MODE (LONG_REAL), NULL);
  a68_idf (A68_FALSE, "readlongreal", m, genie_read_long_real);
  a68_idf (A68_FALSE, "readdouble", m, genie_read_long_real);
  m = a68_proc (MODE (VOID), MODE (LONG_REAL), NULL);
  a68_idf (A68_FALSE, "printlongreal", m, genie_print_long_real);
  a68_idf (A68_FALSE, "printdouble", m, genie_print_long_real);
  m = a68_proc (MODE (LONGLONG_REAL), NULL);
  a68_idf (A68_FALSE, "readlonglongreal", m, genie_read_longlong_real);
  a68_idf (A68_FALSE, "readquad", m, genie_read_longlong_real);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_REAL), NULL);
  a68_idf (A68_FALSE, "printlonglongreal", m, genie_print_longlong_real);
  a68_idf (A68_FALSE, "printquad", m, genie_print_longlong_real);
  m = a68_proc (MODE (COMPLEX), NULL);
  a68_idf (A68_FALSE, "readcompl", m, genie_read_complex);
  a68_idf (A68_FALSE, "readcomplex", m, genie_read_complex);
  m = a68_proc (MODE (VOID), MODE (COMPLEX), NULL);
  a68_idf (A68_FALSE, "printcompl", m, genie_print_complex);
  a68_idf (A68_FALSE, "printcomplex", m, genie_print_complex);
  m = a68_proc (MODE (LONG_COMPLEX), NULL);
  a68_idf (A68_FALSE, "readlongcompl", m, genie_read_long_complex);
  a68_idf (A68_FALSE, "readlongcomplex", m, genie_read_long_complex);
  m = a68_proc (MODE (VOID), MODE (LONG_COMPLEX), NULL);
  a68_idf (A68_FALSE, "printlongcompl", m, genie_print_long_complex);
  a68_idf (A68_FALSE, "printlongcomplex", m, genie_print_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A68_FALSE, "readlonglongcompl", m, genie_read_longlong_complex);
  a68_idf (A68_FALSE, "readlonglongcomplex", m, genie_read_longlong_complex);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A68_FALSE, "printlonglongcompl", m, genie_print_longlong_complex);
  a68_idf (A68_FALSE, "printlonglongcomplex", m, genie_print_longlong_complex);
  m = proc_bool;
  a68_idf (A68_FALSE, "readbool", m, genie_read_bool);
  m = a68_proc (MODE (VOID), MODE (BOOL), NULL);
  a68_idf (A68_FALSE, "printbool", m, genie_print_bool);
  m = a68_proc (MODE (BITS), NULL);
  a68_idf (A68_FALSE, "readbits", m, genie_read_bits);
  m = a68_proc (MODE (LONG_BITS), NULL);
  a68_idf (A68_FALSE, "readlongbits", m, genie_read_long_bits);
  m = a68_proc (MODE (LONGLONG_BITS), NULL);
  a68_idf (A68_FALSE, "readlonglongbits", m, genie_read_longlong_bits);
  m = a68_proc (MODE (VOID), MODE (BITS), NULL);
  a68_idf (A68_FALSE, "printbits", m, genie_print_bits);
  m = a68_proc (MODE (VOID), MODE (LONG_BITS), NULL);
  a68_idf (A68_FALSE, "printlongbits", m, genie_print_long_bits);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_BITS), NULL);
  a68_idf (A68_FALSE, "printlonglongbits", m, genie_print_longlong_bits);
  m = proc_char;
  a68_idf (A68_FALSE, "readchar", m, genie_read_char);
  m = a68_proc (MODE (VOID), MODE (CHAR), NULL);
  a68_idf (A68_FALSE, "printchar", m, genie_print_char);
  a68_idf (A68_FALSE, "readstring", MODE (PROC_STRING), genie_read_string);
  m = a68_proc (MODE (VOID), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "printstring", m, genie_print_string);
}

/*!
\brief set up standenv - extensions
**/

static void stand_extensions (void)
{
#if defined ENABLE_GRAPHICS
/* Drawing. */
  m = a68_proc (MODE (BOOL), MODE (REF_FILE), MODE (STRING), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "drawdevice", m, genie_make_device);
  a68_idf (A68_FALSE, "makedevice", m, genie_make_device);
  m = a68_proc (MODE (REAL), MODE (REF_FILE), NULL);
  a68_idf (A68_FALSE, "drawaspect", m, genie_draw_aspect);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), NULL);
  a68_idf (A68_FALSE, "drawclear", m, genie_draw_clear);
  a68_idf (A68_FALSE, "drawerase", m, genie_draw_clear);
  a68_idf (A68_FALSE, "drawflush", m, genie_draw_show);
  a68_idf (A68_FALSE, "drawshow", m, genie_draw_show);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A68_FALSE, "drawfillstyle", m, genie_draw_filltype);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A68_FALSE, "drawgetcolourname", m, genie_draw_get_colour_name);
  a68_idf (A68_FALSE, "drawgetcolorname", m, genie_draw_get_colour_name);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A68_FALSE, "drawcolor", m, genie_draw_colour);
  a68_idf (A68_FALSE, "drawcolour", m, genie_draw_colour);
  a68_idf (A68_FALSE, "drawbackgroundcolor", m, genie_draw_background_colour);
  a68_idf (A68_FALSE, "drawbackgroundcolour", m, genie_draw_background_colour);
  a68_idf (A68_FALSE, "drawcircle", m, genie_draw_circle);
  a68_idf (A68_FALSE, "drawball", m, genie_draw_atom);
  a68_idf (A68_FALSE, "drawstar", m, genie_draw_star);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A68_FALSE, "drawpoint", m, genie_draw_point);
  a68_idf (A68_FALSE, "drawline", m, genie_draw_line);
  a68_idf (A68_FALSE, "drawmove", m, genie_draw_move);
  a68_idf (A68_FALSE, "drawrect", m, genie_draw_rect);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (CHAR), MODE (CHAR), MODE (ROW_CHAR), NULL);
  a68_idf (A68_FALSE, "drawtext", m, genie_draw_text);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_CHAR), NULL);
  a68_idf (A68_FALSE, "drawlinestyle", m, genie_draw_linestyle);
  a68_idf (A68_FALSE, "drawfontname", m, genie_draw_fontname);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), NULL);
  a68_idf (A68_FALSE, "drawlinewidth", m, genie_draw_linewidth);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A68_FALSE, "drawfontsize", m, genie_draw_fontsize);
  a68_idf (A68_FALSE, "drawtextangle", m, genie_draw_textangle);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "drawcolorname", m, genie_draw_colour_name);
  a68_idf (A68_FALSE, "drawcolourname", m, genie_draw_colour_name);
  a68_idf (A68_FALSE, "drawbackgroundcolorname", m, genie_draw_background_colour_name);
  a68_idf (A68_FALSE, "drawbackgroundcolourname", m, genie_draw_background_colour_name);
#endif
#if defined ENABLE_NUMERICAL
  a68_idf (A68_FALSE, "cgsspeedoflight", MODE (REAL), genie_cgs_speed_of_light);
  a68_idf (A68_FALSE, "cgsgravitationalconstant", MODE (REAL), genie_cgs_gravitational_constant);
  a68_idf (A68_FALSE, "cgsplanckconstant", MODE (REAL), genie_cgs_planck_constant_h);
  a68_idf (A68_FALSE, "cgsplanckconstantbar", MODE (REAL), genie_cgs_planck_constant_hbar);
  a68_idf (A68_FALSE, "cgsastronomicalunit", MODE (REAL), genie_cgs_astronomical_unit);
  a68_idf (A68_FALSE, "cgslightyear", MODE (REAL), genie_cgs_light_year);
  a68_idf (A68_FALSE, "cgsparsec", MODE (REAL), genie_cgs_parsec);
  a68_idf (A68_FALSE, "cgsgravaccel", MODE (REAL), genie_cgs_grav_accel);
  a68_idf (A68_FALSE, "cgselectronvolt", MODE (REAL), genie_cgs_electron_volt);
  a68_idf (A68_FALSE, "cgsmasselectron", MODE (REAL), genie_cgs_mass_electron);
  a68_idf (A68_FALSE, "cgsmassmuon", MODE (REAL), genie_cgs_mass_muon);
  a68_idf (A68_FALSE, "cgsmassproton", MODE (REAL), genie_cgs_mass_proton);
  a68_idf (A68_FALSE, "cgsmassneutron", MODE (REAL), genie_cgs_mass_neutron);
  a68_idf (A68_FALSE, "cgsrydberg", MODE (REAL), genie_cgs_rydberg);
  a68_idf (A68_FALSE, "cgsboltzmann", MODE (REAL), genie_cgs_boltzmann);
  a68_idf (A68_FALSE, "cgsbohrmagneton", MODE (REAL), genie_cgs_bohr_magneton);
  a68_idf (A68_FALSE, "cgsnuclearmagneton", MODE (REAL), genie_cgs_nuclear_magneton);
  a68_idf (A68_FALSE, "cgselectronmagneticmoment", MODE (REAL), genie_cgs_electron_magnetic_moment);
  a68_idf (A68_FALSE, "cgsprotonmagneticmoment", MODE (REAL), genie_cgs_proton_magnetic_moment);
  a68_idf (A68_FALSE, "cgsmolargas", MODE (REAL), genie_cgs_molar_gas);
  a68_idf (A68_FALSE, "cgsstandardgasvolume", MODE (REAL), genie_cgs_standard_gas_volume);
  a68_idf (A68_FALSE, "cgsminute", MODE (REAL), genie_cgs_minute);
  a68_idf (A68_FALSE, "cgshour", MODE (REAL), genie_cgs_hour);
  a68_idf (A68_FALSE, "cgsday", MODE (REAL), genie_cgs_day);
  a68_idf (A68_FALSE, "cgsweek", MODE (REAL), genie_cgs_week);
  a68_idf (A68_FALSE, "cgsinch", MODE (REAL), genie_cgs_inch);
  a68_idf (A68_FALSE, "cgsfoot", MODE (REAL), genie_cgs_foot);
  a68_idf (A68_FALSE, "cgsyard", MODE (REAL), genie_cgs_yard);
  a68_idf (A68_FALSE, "cgsmile", MODE (REAL), genie_cgs_mile);
  a68_idf (A68_FALSE, "cgsnauticalmile", MODE (REAL), genie_cgs_nautical_mile);
  a68_idf (A68_FALSE, "cgsfathom", MODE (REAL), genie_cgs_fathom);
  a68_idf (A68_FALSE, "cgsmil", MODE (REAL), genie_cgs_mil);
  a68_idf (A68_FALSE, "cgspoint", MODE (REAL), genie_cgs_point);
  a68_idf (A68_FALSE, "cgstexpoint", MODE (REAL), genie_cgs_texpoint);
  a68_idf (A68_FALSE, "cgsmicron", MODE (REAL), genie_cgs_micron);
  a68_idf (A68_FALSE, "cgsangstrom", MODE (REAL), genie_cgs_angstrom);
  a68_idf (A68_FALSE, "cgshectare", MODE (REAL), genie_cgs_hectare);
  a68_idf (A68_FALSE, "cgsacre", MODE (REAL), genie_cgs_acre);
  a68_idf (A68_FALSE, "cgsbarn", MODE (REAL), genie_cgs_barn);
  a68_idf (A68_FALSE, "cgsliter", MODE (REAL), genie_cgs_liter);
  a68_idf (A68_FALSE, "cgsusgallon", MODE (REAL), genie_cgs_us_gallon);
  a68_idf (A68_FALSE, "cgsquart", MODE (REAL), genie_cgs_quart);
  a68_idf (A68_FALSE, "cgspint", MODE (REAL), genie_cgs_pint);
  a68_idf (A68_FALSE, "cgscup", MODE (REAL), genie_cgs_cup);
  a68_idf (A68_FALSE, "cgsfluidounce", MODE (REAL), genie_cgs_fluid_ounce);
  a68_idf (A68_FALSE, "cgstablespoon", MODE (REAL), genie_cgs_tablespoon);
  a68_idf (A68_FALSE, "cgsteaspoon", MODE (REAL), genie_cgs_teaspoon);
  a68_idf (A68_FALSE, "cgscanadiangallon", MODE (REAL), genie_cgs_canadian_gallon);
  a68_idf (A68_FALSE, "cgsukgallon", MODE (REAL), genie_cgs_uk_gallon);
  a68_idf (A68_FALSE, "cgsmilesperhour", MODE (REAL), genie_cgs_miles_per_hour);
  a68_idf (A68_FALSE, "cgskilometersperhour", MODE (REAL), genie_cgs_kilometers_per_hour);
  a68_idf (A68_FALSE, "cgsknot", MODE (REAL), genie_cgs_knot);
  a68_idf (A68_FALSE, "cgspoundmass", MODE (REAL), genie_cgs_pound_mass);
  a68_idf (A68_FALSE, "cgsouncemass", MODE (REAL), genie_cgs_ounce_mass);
  a68_idf (A68_FALSE, "cgston", MODE (REAL), genie_cgs_ton);
  a68_idf (A68_FALSE, "cgsmetricton", MODE (REAL), genie_cgs_metric_ton);
  a68_idf (A68_FALSE, "cgsukton", MODE (REAL), genie_cgs_uk_ton);
  a68_idf (A68_FALSE, "cgstroyounce", MODE (REAL), genie_cgs_troy_ounce);
  a68_idf (A68_FALSE, "cgscarat", MODE (REAL), genie_cgs_carat);
  a68_idf (A68_FALSE, "cgsunifiedatomicmass", MODE (REAL), genie_cgs_unified_atomic_mass);
  a68_idf (A68_FALSE, "cgsgramforce", MODE (REAL), genie_cgs_gram_force);
  a68_idf (A68_FALSE, "cgspoundforce", MODE (REAL), genie_cgs_pound_force);
  a68_idf (A68_FALSE, "cgskilopoundforce", MODE (REAL), genie_cgs_kilopound_force);
  a68_idf (A68_FALSE, "cgspoundal", MODE (REAL), genie_cgs_poundal);
  a68_idf (A68_FALSE, "cgscalorie", MODE (REAL), genie_cgs_calorie);
  a68_idf (A68_FALSE, "cgsbtu", MODE (REAL), genie_cgs_btu);
  a68_idf (A68_FALSE, "cgstherm", MODE (REAL), genie_cgs_therm);
  a68_idf (A68_FALSE, "cgshorsepower", MODE (REAL), genie_cgs_horsepower);
  a68_idf (A68_FALSE, "cgsbar", MODE (REAL), genie_cgs_bar);
  a68_idf (A68_FALSE, "cgsstdatmosphere", MODE (REAL), genie_cgs_std_atmosphere);
  a68_idf (A68_FALSE, "cgstorr", MODE (REAL), genie_cgs_torr);
  a68_idf (A68_FALSE, "cgsmeterofmercury", MODE (REAL), genie_cgs_meter_of_mercury);
  a68_idf (A68_FALSE, "cgsinchofmercury", MODE (REAL), genie_cgs_inch_of_mercury);
  a68_idf (A68_FALSE, "cgsinchofwater", MODE (REAL), genie_cgs_inch_of_water);
  a68_idf (A68_FALSE, "cgspsi", MODE (REAL), genie_cgs_psi);
  a68_idf (A68_FALSE, "cgspoise", MODE (REAL), genie_cgs_poise);
  a68_idf (A68_FALSE, "cgsstokes", MODE (REAL), genie_cgs_stokes);
  a68_idf (A68_FALSE, "cgsfaraday", MODE (REAL), genie_cgs_faraday);
  a68_idf (A68_FALSE, "cgselectroncharge", MODE (REAL), genie_cgs_electron_charge);
  a68_idf (A68_FALSE, "cgsgauss", MODE (REAL), genie_cgs_gauss);
  a68_idf (A68_FALSE, "cgsstilb", MODE (REAL), genie_cgs_stilb);
  a68_idf (A68_FALSE, "cgslumen", MODE (REAL), genie_cgs_lumen);
  a68_idf (A68_FALSE, "cgslux", MODE (REAL), genie_cgs_lux);
  a68_idf (A68_FALSE, "cgsphot", MODE (REAL), genie_cgs_phot);
  a68_idf (A68_FALSE, "cgsfootcandle", MODE (REAL), genie_cgs_footcandle);
  a68_idf (A68_FALSE, "cgslambert", MODE (REAL), genie_cgs_lambert);
  a68_idf (A68_FALSE, "cgsfootlambert", MODE (REAL), genie_cgs_footlambert);
  a68_idf (A68_FALSE, "cgscurie", MODE (REAL), genie_cgs_curie);
  a68_idf (A68_FALSE, "cgsroentgen", MODE (REAL), genie_cgs_roentgen);
  a68_idf (A68_FALSE, "cgsrad", MODE (REAL), genie_cgs_rad);
  a68_idf (A68_FALSE, "cgssolarmass", MODE (REAL), genie_cgs_solar_mass);
  a68_idf (A68_FALSE, "cgsbohrradius", MODE (REAL), genie_cgs_bohr_radius);
  a68_idf (A68_FALSE, "cgsnewton", MODE (REAL), genie_cgs_newton);
  a68_idf (A68_FALSE, "cgsdyne", MODE (REAL), genie_cgs_dyne);
  a68_idf (A68_FALSE, "cgsjoule", MODE (REAL), genie_cgs_joule);
  a68_idf (A68_FALSE, "cgserg", MODE (REAL), genie_cgs_erg);
  a68_idf (A68_FALSE, "mksaspeedoflight", MODE (REAL), genie_mks_speed_of_light);
  a68_idf (A68_FALSE, "mksagravitationalconstant", MODE (REAL), genie_mks_gravitational_constant);
  a68_idf (A68_FALSE, "mksaplanckconstant", MODE (REAL), genie_mks_planck_constant_h);
  a68_idf (A68_FALSE, "mksaplanckconstantbar", MODE (REAL), genie_mks_planck_constant_hbar);
  a68_idf (A68_FALSE, "mksavacuumpermeability", MODE (REAL), genie_mks_vacuum_permeability);
  a68_idf (A68_FALSE, "mksaastronomicalunit", MODE (REAL), genie_mks_astronomical_unit);
  a68_idf (A68_FALSE, "mksalightyear", MODE (REAL), genie_mks_light_year);
  a68_idf (A68_FALSE, "mksaparsec", MODE (REAL), genie_mks_parsec);
  a68_idf (A68_FALSE, "mksagravaccel", MODE (REAL), genie_mks_grav_accel);
  a68_idf (A68_FALSE, "mksaelectronvolt", MODE (REAL), genie_mks_electron_volt);
  a68_idf (A68_FALSE, "mksamasselectron", MODE (REAL), genie_mks_mass_electron);
  a68_idf (A68_FALSE, "mksamassmuon", MODE (REAL), genie_mks_mass_muon);
  a68_idf (A68_FALSE, "mksamassproton", MODE (REAL), genie_mks_mass_proton);
  a68_idf (A68_FALSE, "mksamassneutron", MODE (REAL), genie_mks_mass_neutron);
  a68_idf (A68_FALSE, "mksarydberg", MODE (REAL), genie_mks_rydberg);
  a68_idf (A68_FALSE, "mksaboltzmann", MODE (REAL), genie_mks_boltzmann);
  a68_idf (A68_FALSE, "mksabohrmagneton", MODE (REAL), genie_mks_bohr_magneton);
  a68_idf (A68_FALSE, "mksanuclearmagneton", MODE (REAL), genie_mks_nuclear_magneton);
  a68_idf (A68_FALSE, "mksaelectronmagneticmoment", MODE (REAL), genie_mks_electron_magnetic_moment);
  a68_idf (A68_FALSE, "mksaprotonmagneticmoment", MODE (REAL), genie_mks_proton_magnetic_moment);
  a68_idf (A68_FALSE, "mksamolargas", MODE (REAL), genie_mks_molar_gas);
  a68_idf (A68_FALSE, "mksastandardgasvolume", MODE (REAL), genie_mks_standard_gas_volume);
  a68_idf (A68_FALSE, "mksaminute", MODE (REAL), genie_mks_minute);
  a68_idf (A68_FALSE, "mksahour", MODE (REAL), genie_mks_hour);
  a68_idf (A68_FALSE, "mksaday", MODE (REAL), genie_mks_day);
  a68_idf (A68_FALSE, "mksaweek", MODE (REAL), genie_mks_week);
  a68_idf (A68_FALSE, "mksainch", MODE (REAL), genie_mks_inch);
  a68_idf (A68_FALSE, "mksafoot", MODE (REAL), genie_mks_foot);
  a68_idf (A68_FALSE, "mksayard", MODE (REAL), genie_mks_yard);
  a68_idf (A68_FALSE, "mksamile", MODE (REAL), genie_mks_mile);
  a68_idf (A68_FALSE, "mksanauticalmile", MODE (REAL), genie_mks_nautical_mile);
  a68_idf (A68_FALSE, "mksafathom", MODE (REAL), genie_mks_fathom);
  a68_idf (A68_FALSE, "mksamil", MODE (REAL), genie_mks_mil);
  a68_idf (A68_FALSE, "mksapoint", MODE (REAL), genie_mks_point);
  a68_idf (A68_FALSE, "mksatexpoint", MODE (REAL), genie_mks_texpoint);
  a68_idf (A68_FALSE, "mksamicron", MODE (REAL), genie_mks_micron);
  a68_idf (A68_FALSE, "mksaangstrom", MODE (REAL), genie_mks_angstrom);
  a68_idf (A68_FALSE, "mksahectare", MODE (REAL), genie_mks_hectare);
  a68_idf (A68_FALSE, "mksaacre", MODE (REAL), genie_mks_acre);
  a68_idf (A68_FALSE, "mksabarn", MODE (REAL), genie_mks_barn);
  a68_idf (A68_FALSE, "mksaliter", MODE (REAL), genie_mks_liter);
  a68_idf (A68_FALSE, "mksausgallon", MODE (REAL), genie_mks_us_gallon);
  a68_idf (A68_FALSE, "mksaquart", MODE (REAL), genie_mks_quart);
  a68_idf (A68_FALSE, "mksapint", MODE (REAL), genie_mks_pint);
  a68_idf (A68_FALSE, "mksacup", MODE (REAL), genie_mks_cup);
  a68_idf (A68_FALSE, "mksafluidounce", MODE (REAL), genie_mks_fluid_ounce);
  a68_idf (A68_FALSE, "mksatablespoon", MODE (REAL), genie_mks_tablespoon);
  a68_idf (A68_FALSE, "mksateaspoon", MODE (REAL), genie_mks_teaspoon);
  a68_idf (A68_FALSE, "mksacanadiangallon", MODE (REAL), genie_mks_canadian_gallon);
  a68_idf (A68_FALSE, "mksaukgallon", MODE (REAL), genie_mks_uk_gallon);
  a68_idf (A68_FALSE, "mksamilesperhour", MODE (REAL), genie_mks_miles_per_hour);
  a68_idf (A68_FALSE, "mksakilometersperhour", MODE (REAL), genie_mks_kilometers_per_hour);
  a68_idf (A68_FALSE, "mksaknot", MODE (REAL), genie_mks_knot);
  a68_idf (A68_FALSE, "mksapoundmass", MODE (REAL), genie_mks_pound_mass);
  a68_idf (A68_FALSE, "mksaouncemass", MODE (REAL), genie_mks_ounce_mass);
  a68_idf (A68_FALSE, "mksaton", MODE (REAL), genie_mks_ton);
  a68_idf (A68_FALSE, "mksametricton", MODE (REAL), genie_mks_metric_ton);
  a68_idf (A68_FALSE, "mksaukton", MODE (REAL), genie_mks_uk_ton);
  a68_idf (A68_FALSE, "mksatroyounce", MODE (REAL), genie_mks_troy_ounce);
  a68_idf (A68_FALSE, "mksacarat", MODE (REAL), genie_mks_carat);
  a68_idf (A68_FALSE, "mksaunifiedatomicmass", MODE (REAL), genie_mks_unified_atomic_mass);
  a68_idf (A68_FALSE, "mksagramforce", MODE (REAL), genie_mks_gram_force);
  a68_idf (A68_FALSE, "mksapoundforce", MODE (REAL), genie_mks_pound_force);
  a68_idf (A68_FALSE, "mksakilopoundforce", MODE (REAL), genie_mks_kilopound_force);
  a68_idf (A68_FALSE, "mksapoundal", MODE (REAL), genie_mks_poundal);
  a68_idf (A68_FALSE, "mksacalorie", MODE (REAL), genie_mks_calorie);
  a68_idf (A68_FALSE, "mksabtu", MODE (REAL), genie_mks_btu);
  a68_idf (A68_FALSE, "mksatherm", MODE (REAL), genie_mks_therm);
  a68_idf (A68_FALSE, "mksahorsepower", MODE (REAL), genie_mks_horsepower);
  a68_idf (A68_FALSE, "mksabar", MODE (REAL), genie_mks_bar);
  a68_idf (A68_FALSE, "mksastdatmosphere", MODE (REAL), genie_mks_std_atmosphere);
  a68_idf (A68_FALSE, "mksatorr", MODE (REAL), genie_mks_torr);
  a68_idf (A68_FALSE, "mksameterofmercury", MODE (REAL), genie_mks_meter_of_mercury);
  a68_idf (A68_FALSE, "mksainchofmercury", MODE (REAL), genie_mks_inch_of_mercury);
  a68_idf (A68_FALSE, "mksainchofwater", MODE (REAL), genie_mks_inch_of_water);
  a68_idf (A68_FALSE, "mksapsi", MODE (REAL), genie_mks_psi);
  a68_idf (A68_FALSE, "mksapoise", MODE (REAL), genie_mks_poise);
  a68_idf (A68_FALSE, "mksastokes", MODE (REAL), genie_mks_stokes);
  a68_idf (A68_FALSE, "mksafaraday", MODE (REAL), genie_mks_faraday);
  a68_idf (A68_FALSE, "mksaelectroncharge", MODE (REAL), genie_mks_electron_charge);
  a68_idf (A68_FALSE, "mksagauss", MODE (REAL), genie_mks_gauss);
  a68_idf (A68_FALSE, "mksastilb", MODE (REAL), genie_mks_stilb);
  a68_idf (A68_FALSE, "mksalumen", MODE (REAL), genie_mks_lumen);
  a68_idf (A68_FALSE, "mksalux", MODE (REAL), genie_mks_lux);
  a68_idf (A68_FALSE, "mksaphot", MODE (REAL), genie_mks_phot);
  a68_idf (A68_FALSE, "mksafootcandle", MODE (REAL), genie_mks_footcandle);
  a68_idf (A68_FALSE, "mksalambert", MODE (REAL), genie_mks_lambert);
  a68_idf (A68_FALSE, "mksafootlambert", MODE (REAL), genie_mks_footlambert);
  a68_idf (A68_FALSE, "mksacurie", MODE (REAL), genie_mks_curie);
  a68_idf (A68_FALSE, "mksaroentgen", MODE (REAL), genie_mks_roentgen);
  a68_idf (A68_FALSE, "mksarad", MODE (REAL), genie_mks_rad);
  a68_idf (A68_FALSE, "mksasolarmass", MODE (REAL), genie_mks_solar_mass);
  a68_idf (A68_FALSE, "mksabohrradius", MODE (REAL), genie_mks_bohr_radius);
  a68_idf (A68_FALSE, "mksavacuumpermittivity", MODE (REAL), genie_mks_vacuum_permittivity);
  a68_idf (A68_FALSE, "mksanewton", MODE (REAL), genie_mks_newton);
  a68_idf (A68_FALSE, "mksadyne", MODE (REAL), genie_mks_dyne);
  a68_idf (A68_FALSE, "mksajoule", MODE (REAL), genie_mks_joule);
  a68_idf (A68_FALSE, "mksaerg", MODE (REAL), genie_mks_erg);
  a68_idf (A68_FALSE, "numfinestructure", MODE (REAL), genie_num_fine_structure);
  a68_idf (A68_FALSE, "numavogadro", MODE (REAL), genie_num_avogadro);
  a68_idf (A68_FALSE, "numyotta", MODE (REAL), genie_num_yotta);
  a68_idf (A68_FALSE, "numzetta", MODE (REAL), genie_num_zetta);
  a68_idf (A68_FALSE, "numexa", MODE (REAL), genie_num_exa);
  a68_idf (A68_FALSE, "numpeta", MODE (REAL), genie_num_peta);
  a68_idf (A68_FALSE, "numtera", MODE (REAL), genie_num_tera);
  a68_idf (A68_FALSE, "numgiga", MODE (REAL), genie_num_giga);
  a68_idf (A68_FALSE, "nummega", MODE (REAL), genie_num_mega);
  a68_idf (A68_FALSE, "numkilo", MODE (REAL), genie_num_kilo);
  a68_idf (A68_FALSE, "nummilli", MODE (REAL), genie_num_milli);
  a68_idf (A68_FALSE, "nummicro", MODE (REAL), genie_num_micro);
  a68_idf (A68_FALSE, "numnano", MODE (REAL), genie_num_nano);
  a68_idf (A68_FALSE, "numpico", MODE (REAL), genie_num_pico);
  a68_idf (A68_FALSE, "numfemto", MODE (REAL), genie_num_femto);
  a68_idf (A68_FALSE, "numatto", MODE (REAL), genie_num_atto);
  a68_idf (A68_FALSE, "numzepto", MODE (REAL), genie_num_zepto);
  a68_idf (A68_FALSE, "numyocto", MODE (REAL), genie_num_yocto);
  m = proc_real_real;
  a68_idf (A68_FALSE, "erf", m, genie_erf_real);
  a68_idf (A68_FALSE, "erfc", m, genie_erfc_real);
  a68_idf (A68_FALSE, "gamma", m, genie_gamma_real);
  a68_idf (A68_FALSE, "lngamma", m, genie_lngamma_real);
  a68_idf (A68_FALSE, "factorial", m, genie_factorial_real);
  a68_idf (A68_FALSE, "airyai", m, genie_airy_ai_real);
  a68_idf (A68_FALSE, "airybi", m, genie_airy_bi_real);
  a68_idf (A68_FALSE, "airyaiderivative", m, genie_airy_ai_deriv_real);
  a68_idf (A68_FALSE, "airybiderivative", m, genie_airy_bi_deriv_real);
  a68_idf (A68_FALSE, "ellipticintegralk", m, genie_elliptic_integral_k_real);
  a68_idf (A68_FALSE, "ellipticintegrale", m, genie_elliptic_integral_e_real);
  m = proc_real_real_real;
  a68_idf (A68_FALSE, "beta", m, genie_beta_real);
  a68_idf (A68_FALSE, "besseljn", m, genie_bessel_jn_real);
  a68_idf (A68_FALSE, "besselyn", m, genie_bessel_yn_real);
  a68_idf (A68_FALSE, "besselin", m, genie_bessel_in_real);
  a68_idf (A68_FALSE, "besselexpin", m, genie_bessel_exp_in_real);
  a68_idf (A68_FALSE, "besselkn", m, genie_bessel_kn_real);
  a68_idf (A68_FALSE, "besselexpkn", m, genie_bessel_exp_kn_real);
  a68_idf (A68_FALSE, "besseljl", m, genie_bessel_jl_real);
  a68_idf (A68_FALSE, "besselyl", m, genie_bessel_yl_real);
  a68_idf (A68_FALSE, "besselexpil", m, genie_bessel_exp_il_real);
  a68_idf (A68_FALSE, "besselexpkl", m, genie_bessel_exp_kl_real);
  a68_idf (A68_FALSE, "besseljnu", m, genie_bessel_jnu_real);
  a68_idf (A68_FALSE, "besselynu", m, genie_bessel_ynu_real);
  a68_idf (A68_FALSE, "besselinu", m, genie_bessel_inu_real);
  a68_idf (A68_FALSE, "besselexpinu", m, genie_bessel_exp_inu_real);
  a68_idf (A68_FALSE, "besselknu", m, genie_bessel_knu_real);
  a68_idf (A68_FALSE, "besselexpknu", m, genie_bessel_exp_knu_real);
  a68_idf (A68_FALSE, "ellipticintegralrc", m, genie_elliptic_integral_rc_real);
  a68_idf (A68_FALSE, "incompletegamma", m, genie_gamma_inc_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A68_FALSE, "incompletebeta", m, genie_beta_inc_real);
  a68_idf (A68_FALSE, "ellipticintegralrf", m, genie_elliptic_integral_rf_real);
  a68_idf (A68_FALSE, "ellipticintegralrd", m, genie_elliptic_integral_rd_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A68_FALSE, "ellipticintegralrj", m, genie_elliptic_integral_rj_real);
/* Vector and matrix monadic. */
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "+", m, genie_idle);
  a68_op (A68_FALSE, "-", m, genie_vector_minus);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_FALSE, "+", m, genie_idle);
  a68_op (A68_FALSE, "-", m, genie_matrix_minus);
  a68_op (A68_FALSE, "T", m, genie_matrix_transpose);
  a68_op (A68_FALSE, "INV", m, genie_matrix_inv);
  m = a68_proc (MODE (REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_FALSE, "DET", m, genie_matrix_det);
  a68_op (A68_FALSE, "TRACE", m, genie_matrix_trace);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "+", m, genie_idle);
  a68_op (A68_FALSE, "-", m, genie_vector_complex_minus);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "+", m, genie_idle);
  a68_op (A68_FALSE, "-", m, genie_matrix_complex_minus);
  a68_op (A68_FALSE, "T", m, genie_matrix_complex_transpose);
  a68_op (A68_FALSE, "INV", m, genie_matrix_complex_inv);
  m = a68_proc (MODE (COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "DET", m, genie_matrix_complex_det);
  a68_op (A68_FALSE, "TRACE", m, genie_matrix_complex_trace);
/* Vector and matrix dyadic. */
  m = a68_proc (MODE (BOOL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "=", m, genie_vector_eq);
  a68_op (A68_FALSE, "/=", m, genie_vector_ne);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "+", m, genie_vector_add);
  a68_op (A68_FALSE, "-", m, genie_vector_sub);
  m = a68_proc (MODE (REF_ROW_REAL), MODE (REF_ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "+:=", m, genie_vector_plusab);
  a68_op (A68_FALSE, "PLUSAB", m, genie_vector_plusab);
  a68_op (A68_FALSE, "-:=", m, genie_vector_minusab);
  a68_op (A68_FALSE, "MINUSAB", m, genie_vector_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_FALSE, "=", m, genie_matrix_eq);
  a68_op (A68_FALSE, "/-", m, genie_matrix_ne);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_FALSE, "+", m, genie_matrix_add);
  a68_op (A68_FALSE, "-", m, genie_matrix_sub);
  m = a68_proc (MODE (REF_ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_FALSE, "+:=", m, genie_matrix_plusab);
  a68_op (A68_FALSE, "PLUSAB", m, genie_matrix_plusab);
  a68_op (A68_FALSE, "-:=", m, genie_matrix_minusab);
  a68_op (A68_FALSE, "MINUSAB", m, genie_matrix_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "=", m, genie_vector_complex_eq);
  a68_op (A68_FALSE, "/=", m, genie_vector_complex_ne);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "+", m, genie_vector_complex_add);
  a68_op (A68_FALSE, "-", m, genie_vector_complex_sub);
  m = a68_proc (MODE (REF_ROW_COMPLEX), MODE (REF_ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "+:=", m, genie_vector_complex_plusab);
  a68_op (A68_FALSE, "PLUSAB", m, genie_vector_complex_plusab);
  a68_op (A68_FALSE, "-:=", m, genie_vector_complex_minusab);
  a68_op (A68_FALSE, "MINUSAB", m, genie_vector_complex_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "=", m, genie_matrix_complex_eq);
  a68_op (A68_FALSE, "/=", m, genie_matrix_complex_ne);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "+", m, genie_matrix_complex_add);
  a68_op (A68_FALSE, "-", m, genie_matrix_complex_sub);
  m = a68_proc (MODE (REF_ROWROW_COMPLEX), MODE (REF_ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "+:=", m, genie_matrix_complex_plusab);
  a68_op (A68_FALSE, "PLUSAB", m, genie_matrix_complex_plusab);
  a68_op (A68_FALSE, "-:=", m, genie_matrix_complex_minusab);
  a68_op (A68_FALSE, "MINUSAB", m, genie_matrix_complex_minusab);
/* Vector and matrix scaling. */
  m = a68_proc (MODE (ROW_REAL), MODE (REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "*", m, genie_real_scale_vector);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (REAL), NULL);
  a68_op (A68_FALSE, "*", m, genie_vector_scale_real);
  a68_op (A68_FALSE, "/", m, genie_vector_div_real);
  m = a68_proc (MODE (ROWROW_REAL), MODE (REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_FALSE, "*", m, genie_real_scale_matrix);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REAL), NULL);
  a68_op (A68_FALSE, "*", m, genie_matrix_scale_real);
  a68_op (A68_FALSE, "/", m, genie_matrix_div_real);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "*", m, genie_complex_scale_vector_complex);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_FALSE, "*", m, genie_vector_complex_scale_complex);
  a68_op (A68_FALSE, "/", m, genie_vector_complex_div_complex);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "*", m, genie_complex_scale_matrix_complex);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_FALSE, "*", m, genie_matrix_complex_scale_complex);
  a68_op (A68_FALSE, "/", m, genie_matrix_complex_div_complex);
  m = a68_proc (MODE (REF_ROW_REAL), MODE (REF_ROW_REAL), MODE (REAL), NULL);
  a68_op (A68_FALSE, "*:=", m, genie_vector_scale_real_ab);
  a68_op (A68_FALSE, "/:=", m, genie_vector_div_real_ab);
  m = a68_proc (MODE (REF_ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (REAL), NULL);
  a68_op (A68_FALSE, "*:=", m, genie_matrix_scale_real_ab);
  a68_op (A68_FALSE, "/:=", m, genie_matrix_div_real_ab);
  m = a68_proc (MODE (REF_ROW_COMPLEX), MODE (REF_ROW_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_FALSE, "*:=", m, genie_vector_complex_scale_complex_ab);
  a68_op (A68_FALSE, "/:=", m, genie_vector_complex_div_complex_ab);
  m = a68_proc (MODE (REF_ROWROW_COMPLEX), MODE (REF_ROWROW_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_FALSE, "*:=", m, genie_matrix_complex_scale_complex_ab);
  a68_op (A68_FALSE, "/:=", m, genie_matrix_complex_div_complex_ab);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_FALSE, "*", m, genie_vector_times_matrix);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "*", m, genie_vector_complex_times_matrix);
/* Matrix times vector or matrix. */
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "*", m, genie_matrix_times_vector);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_FALSE, "*", m, genie_matrix_times_matrix);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "*", m, genie_matrix_complex_times_vector);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "*", m, genie_matrix_complex_times_matrix);
/* Vector and matrix miscellaneous. */
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A68_FALSE, "vectorecho", m, genie_vector_echo);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_idf (A68_FALSE, "matrixecho", m, genie_matrix_echo);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_idf (A68_FALSE, "complvectorecho", m, genie_vector_complex_echo);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_idf (A68_FALSE, "complmatrixecho", m, genie_matrix_complex_echo);
   /**/ m = a68_proc (MODE (REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "*", m, genie_vector_dot);
  m = a68_proc (MODE (COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "*", m, genie_vector_complex_dot);
  m = a68_proc (MODE (REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "NORM", m, genie_vector_norm);
  m = a68_proc (MODE (REAL), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "NORM", m, genie_vector_complex_norm);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_FALSE, "DYAD", m, genie_vector_dyad);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_FALSE, "DYAD", m, genie_vector_complex_dyad);
/* LU decomposition. */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROW_INT), MODE (REF_INT), NULL);
  a68_idf (A68_FALSE, "ludecomp", m, genie_matrix_lu);
  m = a68_proc (MODE (REAL), MODE (ROWROW_REAL), MODE (INT), NULL);
  a68_idf (A68_FALSE, "ludet", m, genie_matrix_lu_det);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_INT), NULL);
  a68_idf (A68_FALSE, "luinv", m, genie_matrix_lu_inv);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_INT), MODE (ROW_REAL), NULL);
  a68_idf (A68_FALSE, "lusolve", m, genie_matrix_lu_solve);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (REF_ROW_INT), MODE (REF_INT), NULL);
  a68_idf (A68_FALSE, "complexludecomp", m, genie_matrix_complex_lu);
  m = a68_proc (MODE (COMPLEX), MODE (ROWROW_COMPLEX), MODE (INT), NULL);
  a68_idf (A68_FALSE, "complexludet", m, genie_matrix_complex_lu_det);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_INT), NULL);
  a68_idf (A68_FALSE, "complexluinv", m, genie_matrix_complex_lu_inv);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_INT), MODE (ROW_COMPLEX), NULL);
  a68_idf (A68_FALSE, "complexlusolve", m, genie_matrix_complex_lu_solve);
/* SVD decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (REF_ROW_REAL), NULL);
  a68_idf (A68_FALSE, "svdecomp", m, genie_matrix_svd);
  a68_idf (A68_FALSE, "svddecomp", m, genie_matrix_svd);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A68_FALSE, "svdsolve", m, genie_matrix_svd_solve);
/* QR decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROW_REAL), NULL);
  a68_idf (A68_FALSE, "qrdecomp", m, genie_matrix_qr);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A68_FALSE, "qrsolve", m, genie_matrix_qr_solve);
  a68_idf (A68_FALSE, "qrlssolve", m, genie_matrix_qr_ls_solve);
/* Cholesky decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_idf (A68_FALSE, "choleskydecomp", m, genie_matrix_ch);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A68_FALSE, "choleskysolve", m, genie_matrix_ch_solve);
/* FFT */
  m = a68_proc (MODE (ROW_INT), MODE (INT), NULL);
  a68_idf (A68_FALSE, "primefactors", m, genie_prime_factors);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_idf (A68_FALSE, "fftcomplexforward", m, genie_fft_complex_forward);
  a68_idf (A68_FALSE, "fftcomplexbackward", m, genie_fft_complex_backward);
  a68_idf (A68_FALSE, "fftcomplexinverse", m, genie_fft_complex_inverse);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_REAL), NULL);
  a68_idf (A68_FALSE, "fftforward", m, genie_fft_forward);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_COMPLEX), NULL);
  a68_idf (A68_FALSE, "fftbackward", m, genie_fft_backward);
  a68_idf (A68_FALSE, "fftinverse", m, genie_fft_inverse);
#endif
/* UNIX things */
  m = proc_int;
  a68_idf (A68_FALSE, "argc", m, genie_argc);
  a68_idf (A68_FALSE, "errno", m, genie_errno);
  a68_idf (A68_FALSE, "fork", m, genie_fork);
  m = a68_proc (MODE (STRING), NULL);
  a68_idf (A68_FALSE, "getpwd", m, genie_pwd);
  m = a68_proc (MODE (INT), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "setpwd", m, genie_cd);
  m = a68_proc (MODE (BOOL), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "fileisdirectory", m, genie_file_is_directory);
  a68_idf (A68_FALSE, "fileisregular", m, genie_file_is_block_device);
  a68_idf (A68_FALSE, "fileisregular", m, genie_file_is_char_device);
  a68_idf (A68_FALSE, "fileisregular", m, genie_file_is_regular);
#ifdef __S_IFIFO
  a68_idf (A68_FALSE, "fileisfifo", m, genie_file_is_fifo);
#endif
#ifdef __S_IFLNK
  a68_idf (A68_FALSE, "fileislink", m, genie_file_is_link);
#endif
  m = a68_proc (MODE (BITS), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "filemode", m, genie_file_mode);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A68_FALSE, "argv", m, genie_argv);
  m = proc_void;
  a68_idf (A68_FALSE, "reseterrno", m, genie_reset_errno);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A68_FALSE, "strerror", m, genie_strerror);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A68_FALSE, "execve", m, genie_execve);
  m = a68_proc (MODE (PIPE), NULL);
  a68_idf (A68_FALSE, "createpipe", m, genie_create_pipe);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A68_FALSE, "execvechild", m, genie_execve_child);
  m = a68_proc (MODE (PIPE), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A68_FALSE, "execvechildpipe", m, genie_execve_child_pipe);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), MODE (REF_STRING), NULL);
  a68_idf (A68_FALSE, "execveoutput", m, genie_execve_output);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "getenv", m, genie_getenv);
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A68_FALSE, "waitpid", m, genie_waitpid);
  m = a68_proc (MODE (ROW_INT), NULL);
  a68_idf (A68_FALSE, "utctime", m, genie_utctime);
  a68_idf (A68_FALSE, "localtime", m, genie_localtime);
#if defined ENABLE_DIRENT
  m = a68_proc (MODE (ROW_STRING), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "getdirectory", m, genie_directory);
#endif
#if defined ENABLE_HTTP
  m = a68_proc (MODE (INT), MODE (REF_STRING), MODE (STRING), MODE (STRING), MODE (INT), NULL);
  a68_idf (A68_FALSE, "httpcontent", m, genie_http_content);
  a68_idf (A68_FALSE, "tcprequest", m, genie_tcp_request);
#endif
#if defined ENABLE_REGEX
  m = a68_proc (MODE (INT), MODE (STRING), MODE (STRING), MODE (REF_INT), MODE (REF_INT), NULL);
  a68_idf (A68_FALSE, "grepinstring", m, genie_grep_in_string);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (STRING), MODE (REF_STRING), NULL);
  a68_idf (A68_FALSE, "subinstring", m, genie_sub_in_string);
#endif
#if defined ENABLE_CURSES
  m = proc_void;
  a68_idf (A68_FALSE, "cursesstart", m, genie_curses_start);
  a68_idf (A68_FALSE, "cursesend", m, genie_curses_end);
  a68_idf (A68_FALSE, "cursesclear", m, genie_curses_clear);
  a68_idf (A68_FALSE, "cursesrefresh", m, genie_curses_refresh);
  m = proc_char;
  a68_idf (A68_FALSE, "cursesgetchar", m, genie_curses_getchar);
  m = a68_proc (MODE (VOID), MODE (CHAR), NULL);
  a68_idf (A68_FALSE, "cursesputchar", m, genie_curses_putchar);
  m = a68_proc (MODE (VOID), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_FALSE, "cursesmove", m, genie_curses_move);
  m = proc_int;
  a68_idf (A68_FALSE, "curseslines", m, genie_curses_lines);
  a68_idf (A68_FALSE, "cursescolumns", m, genie_curses_columns);
#endif
#if defined ENABLE_POSTGRESQL
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), MODE (REF_STRING), NULL);
  a68_idf (A68_FALSE, "pqconnectdb", m, genie_pq_connectdb);
  m = a68_proc (MODE (INT), MODE (REF_FILE), NULL);
  a68_idf (A68_FALSE, "pqfinish", m, genie_pq_finish);
  a68_idf (A68_FALSE, "pqreset", m, genie_pq_reset);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A68_FALSE, "pqparameterstatus", m, genie_pq_parameterstatus);
  a68_idf (A68_FALSE, "pqexec", m, genie_pq_exec);
  a68_idf (A68_FALSE, "pqfnumber", m, genie_pq_fnumber);
  m = a68_proc (MODE (INT), MODE (REF_FILE), NULL);
  a68_idf (A68_FALSE, "pqntuples", m, genie_pq_ntuples);
  a68_idf (A68_FALSE, "pqnfields", m, genie_pq_nfields);
  a68_idf (A68_FALSE, "pqcmdstatus", m, genie_pq_cmdstatus);
  a68_idf (A68_FALSE, "pqcmdtuples", m, genie_pq_cmdtuples);
  a68_idf (A68_FALSE, "pqerrormessage", m, genie_pq_errormessage);
  a68_idf (A68_FALSE, "pqresulterrormessage", m, genie_pq_resulterrormessage);
  a68_idf (A68_FALSE, "pqdb", m, genie_pq_db);
  a68_idf (A68_FALSE, "pquser", m, genie_pq_user);
  a68_idf (A68_FALSE, "pqpass", m, genie_pq_pass);
  a68_idf (A68_FALSE, "pqhost", m, genie_pq_host);
  a68_idf (A68_FALSE, "pqport", m, genie_pq_port);
  a68_idf (A68_FALSE, "pqtty", m, genie_pq_tty);
  a68_idf (A68_FALSE, "pqoptions", m, genie_pq_options);
  a68_idf (A68_FALSE, "pqprotocolversion", m, genie_pq_protocolversion);
  a68_idf (A68_FALSE, "pqserverversion", m, genie_pq_serverversion);
  a68_idf (A68_FALSE, "pqsocket", m, genie_pq_socket);
  a68_idf (A68_FALSE, "pqbackendpid", m, genie_pq_backendpid);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A68_FALSE, "pqfname", m, genie_pq_fname);
  a68_idf (A68_FALSE, "pqfformat", m, genie_pq_fformat);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_FALSE, "pqgetvalue", m, genie_pq_getvalue);
  a68_idf (A68_FALSE, "pqgetisnull", m, genie_pq_getisnull);
#endif
}

/*!
\brief build the standard environ symbol table
**/

void make_standard_environ (void)
{
  stand_moids ();
  proc_int = a68_proc (MODE (INT), NULL);
  proc_real = a68_proc (MODE (REAL), NULL);
  proc_real_real = a68_proc (MODE (REAL), MODE (REAL), NULL);
  proc_real_real_real = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  proc_real_real_real_real = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  proc_complex_complex = a68_proc (MODE (COMPLEX), MODE (COMPLEX), NULL);
  proc_bool = a68_proc (MODE (BOOL), NULL);
  proc_char = a68_proc (MODE (CHAR), NULL);
  proc_void = a68_proc (MODE (VOID), NULL);
  stand_prelude ();
  stand_transput ();
  stand_extensions ();
}
