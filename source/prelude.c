/*!
\file prelude.c
\brief builds symbol table for standard prelude
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#define A68_STD A68_TRUE
#define A68_EXT A68_FALSE

SYMBOL_TABLE_T *stand_env;

static MOID_T *proc_int, *proc_real, *proc_real_real, *proc_real_real_real, *proc_real_real_real_real, *proc_complex_complex, *proc_bool, *proc_char, *proc_void;

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
  TAG_TABLE (new_one) = stand_env;
  NODE (new_one) = n;
  VALUE (new_one) = (c != NULL ? add_token (&top_token, c)->text : NULL);
  PRIO (new_one) = p;
  new_one->procedure = q;
  new_one->stand_env_proc = (BOOL_T) (q != NULL);
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
  (void) add_mode (z, PROC_SYMBOL, count_pack_members (p), NULL, m, p);
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
  (void) add_mode (&stand_env->moids, STANDARD, p, some_node (TEXT (find_keyword (top_keyword, t))), NULL, NULL);
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
  PACK_T *z;
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
  (void) add_mode (&stand_env->moids, ROWS_SYMBOL, 0, NULL, NULL, NULL);
  MODE (ROWS) = stand_env->moids;
/* REFs. */
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (INT), NULL);
  MODE (REF_INT) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (REAL), NULL);
  MODE (REF_REAL) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (COMPLEX), NULL);
  MODE (REF_COMPLEX) = MODE (REF_COMPL) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (BITS), NULL);
  MODE (REF_BITS) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (BYTES), NULL);
  MODE (REF_BYTES) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (FORMAT), NULL);
  MODE (REF_FORMAT) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (PIPE), NULL);
  MODE (REF_PIPE) = stand_env->moids;
/* Multiple precision. */
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_INT), NULL);
  MODE (REF_LONG_INT) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_REAL), NULL);
  MODE (REF_LONG_REAL) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_COMPLEX), NULL);
  MODE (REF_LONG_COMPLEX) = MODE (REF_LONG_COMPL) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONGLONG_INT), NULL);
  MODE (REF_LONGLONG_INT) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONGLONG_REAL), NULL);
  MODE (REF_LONGLONG_REAL) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONGLONG_COMPLEX), NULL);
  MODE (REF_LONGLONG_COMPLEX) = MODE (REF_LONGLONG_COMPL) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_BITS), NULL);
  MODE (REF_LONG_BITS) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONGLONG_BITS), NULL);
  MODE (REF_LONGLONG_BITS) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (LONG_BYTES), NULL);
  MODE (REF_LONG_BYTES) = stand_env->moids;
/* Other. */
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (BOOL), NULL);
  MODE (REF_BOOL) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (CHAR), NULL);
  MODE (REF_CHAR) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (FILE), NULL);
  MODE (REF_FILE) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (REF_FILE), NULL);
  MODE (REF_REF_FILE) = stand_env->moids;
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (SOUND), NULL);
  MODE (REF_SOUND) = stand_env->moids;
/* [] INT. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (INT), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_INT) = stand_env->moids;
  SLICE (MODE (ROW_INT)) = MODE (INT);
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_INT), NULL);
  MODE (REF_ROW_INT) = stand_env->moids;
  NAME (MODE (REF_ROW_INT)) = MODE (REF_INT);
/* [] REAL. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (REAL), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_REAL) = stand_env->moids;
  SLICE (MODE (ROW_REAL)) = MODE (REAL);
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_REAL), NULL);
  MODE (REF_ROW_REAL) = stand_env->moids;
  NAME (MODE (REF_ROW_REAL)) = MODE (REF_REAL);
/* [,] REAL. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 2, NULL, MODE (REAL), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROWROW_REAL) = stand_env->moids;
  SLICE (MODE (ROWROW_REAL)) = MODE (ROW_REAL);
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROWROW_REAL), NULL);
  MODE (REF_ROWROW_REAL) = stand_env->moids;
  NAME (MODE (REF_ROWROW_REAL)) = MODE (REF_ROW_REAL);
/* [] COMPLEX. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (COMPLEX), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_COMPLEX) = stand_env->moids;
  SLICE (MODE (ROW_COMPLEX)) = MODE (COMPLEX);
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_COMPLEX), NULL);
  MODE (REF_ROW_COMPLEX) = stand_env->moids;
  NAME (MODE (REF_ROW_COMPLEX)) = MODE (REF_COMPLEX);
/* [,] COMPLEX. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 2, NULL, MODE (COMPLEX), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROWROW_COMPLEX) = stand_env->moids;
  SLICE (MODE (ROWROW_COMPLEX)) = MODE (ROW_COMPLEX);
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROWROW_COMPLEX), NULL);
  MODE (REF_ROWROW_COMPLEX) = stand_env->moids;
  NAME (MODE (REF_ROWROW_COMPLEX)) = MODE (REF_ROW_COMPLEX);
/* [] BOOL. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (BOOL), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_BOOL) = stand_env->moids;
  SLICE (MODE (ROW_BOOL)) = MODE (BOOL);
/* [] BITS. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (BITS), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_BITS) = stand_env->moids;
  SLICE (MODE (ROW_BITS)) = MODE (BITS);
/* [] LONG BITS. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (LONG_BITS), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_LONG_BITS) = stand_env->moids;
  SLICE (MODE (ROW_LONG_BITS)) = MODE (LONG_BITS);
/* [] LONG LONG BITS. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (LONGLONG_BITS), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_LONGLONG_BITS) = stand_env->moids;
  SLICE (MODE (ROW_LONGLONG_BITS)) = MODE (LONGLONG_BITS);
/* [] CHAR. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (CHAR), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_CHAR) = stand_env->moids;
  SLICE (MODE (ROW_CHAR)) = MODE (CHAR);
/* [][] CHAR. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (ROW_CHAR), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_ROW_CHAR) = stand_env->moids;
  SLICE (MODE (ROW_ROW_CHAR)) = MODE (ROW_CHAR);
/* MODE STRING = FLEX [] CHAR. */
  (void) add_mode (&stand_env->moids, FLEX_SYMBOL, 0, NULL, MODE (ROW_CHAR), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  stand_env->moids->deflexed_mode = MODE (ROW_CHAR);
  stand_env->moids->trim = MODE (ROW_CHAR);
  EQUIVALENT (MODE (STRING)) = stand_env->moids;
  DEFLEXED (MODE (STRING)) = MODE (ROW_CHAR);
/* REF [] CHAR. */
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_CHAR), NULL);
  MODE (REF_ROW_CHAR) = stand_env->moids;
  NAME (MODE (REF_ROW_CHAR)) = MODE (REF_CHAR);
/* PROC [] CHAR. */
  (void) add_mode (&stand_env->moids, PROC_SYMBOL, 0, NULL, MODE (ROW_CHAR), NULL);
  MODE (PROC_ROW_CHAR) = stand_env->moids;
/* REF STRING = REF FLEX [] CHAR. */
  (void) add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, EQUIVALENT (MODE (STRING)), NULL);
  MODE (REF_STRING) = stand_env->moids;
  NAME (MODE (REF_STRING)) = MODE (REF_CHAR);
  DEFLEXED (MODE (REF_STRING)) = MODE (REF_ROW_CHAR);
  TRIM (MODE (REF_STRING)) = MODE (REF_ROW_CHAR);
/* [] STRING. */
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (STRING), NULL);
  stand_env->moids->has_rows = A68_TRUE;
  MODE (ROW_STRING) = stand_env->moids;
  SLICE (MODE (ROW_STRING)) = MODE (STRING);
  DEFLEXED (MODE (ROW_STRING)) = MODE (ROW_ROW_CHAR);
/* PROC STRING. */
  (void) add_mode (&stand_env->moids, PROC_SYMBOL, 0, NULL, MODE (STRING), NULL);
  MODE (PROC_STRING) = stand_env->moids;
  DEFLEXED (MODE (PROC_STRING)) = MODE (PROC_ROW_CHAR);
/* COMPLEX. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REAL), add_token (&top_token, "im")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (REAL), add_token (&top_token, "re")->text, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (COMPLEX)) = EQUIVALENT (MODE (COMPL)) = stand_env->moids;
  MODE (COMPLEX) = MODE (COMPL) = stand_env->moids;
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REF_REAL), add_token (&top_token, "im")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (REF_REAL), add_token (&top_token, "re")->text, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_COMPLEX)) = NAME (MODE (REF_COMPL)) = stand_env->moids;
/* LONG COMPLEX. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (LONG_REAL), add_token (&top_token, "im")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (LONG_REAL), add_token (&top_token, "re")->text, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (LONG_COMPLEX)) = EQUIVALENT (MODE (LONG_COMPL)) = stand_env->moids;
  MODE (LONG_COMPLEX) = MODE (LONG_COMPL) = stand_env->moids;
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REF_LONG_REAL), add_token (&top_token, "im")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (REF_LONG_REAL), add_token (&top_token, "re")->text, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_LONG_COMPLEX)) = NAME (MODE (REF_LONG_COMPL)) = stand_env->moids;
/* LONG_LONG COMPLEX. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (LONGLONG_REAL), add_token (&top_token, "im")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (LONGLONG_REAL), add_token (&top_token, "re")->text, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (LONGLONG_COMPLEX)) = EQUIVALENT (MODE (LONGLONG_COMPL)) = stand_env->moids;
  MODE (LONGLONG_COMPLEX) = MODE (LONGLONG_COMPL) = stand_env->moids;
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REF_LONGLONG_REAL), add_token (&top_token, "im")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (REF_LONGLONG_REAL), add_token (&top_token, "re")->text, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_LONGLONG_COMPLEX)) = NAME (MODE (REF_LONGLONG_COMPL)) = stand_env->moids;
/* NUMBER. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (INT), NULL, NULL);
  (void) add_mode_to_pack (&z, MODE (LONG_INT), NULL, NULL);
  (void) add_mode_to_pack (&z, MODE (LONGLONG_INT), NULL, NULL);
  (void) add_mode_to_pack (&z, MODE (REAL), NULL, NULL);
  (void) add_mode_to_pack (&z, MODE (LONG_REAL), NULL, NULL);
  (void) add_mode_to_pack (&z, MODE (LONGLONG_REAL), NULL, NULL);
  (void) add_mode (&stand_env->moids, UNION_SYMBOL, count_pack_members (z), NULL, NULL, z);
  MODE (NUMBER) = stand_env->moids;
/* SEMA. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REF_INT), NULL, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (SEMA)) = stand_env->moids;
  MODE (SEMA) = stand_env->moids;
/* PROC VOID. */
  z = NULL;
  (void) add_mode (&stand_env->moids, PROC_SYMBOL, count_pack_members (z), NULL, MODE (VOID), z);
  MODE (PROC_VOID) = stand_env->moids;
/* PROC (REAL) REAL. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REAL), NULL, NULL);
  (void) add_mode (&stand_env->moids, PROC_SYMBOL, count_pack_members (z), NULL, MODE (REAL), z);
  MODE (PROC_REAL_REAL) = stand_env->moids;
/* IO: PROC (REF FILE) BOOL. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REF_FILE), NULL, NULL);
  (void) add_mode (&stand_env->moids, PROC_SYMBOL, count_pack_members (z), NULL, MODE (BOOL), z);
  MODE (PROC_REF_FILE_BOOL) = stand_env->moids;
/* IO: PROC (REF FILE) VOID. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REF_FILE), NULL, NULL);
  (void) add_mode (&stand_env->moids, PROC_SYMBOL, count_pack_members (z), NULL, MODE (VOID), z);
  MODE (PROC_REF_FILE_VOID) = stand_env->moids;
/* IO: SIMPLIN and SIMPLOUT. */
  (void) add_mode (&stand_env->moids, IN_TYPE_MODE, 0, NULL, NULL, NULL);
  MODE (SIMPLIN) = stand_env->moids;
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (SIMPLIN), NULL);
  MODE (ROW_SIMPLIN) = stand_env->moids;
  SLICE (MODE (ROW_SIMPLIN)) = MODE (SIMPLIN);
  (void) add_mode (&stand_env->moids, OUT_TYPE_MODE, 0, NULL, NULL, NULL);
  MODE (SIMPLOUT) = stand_env->moids;
  (void) add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (SIMPLOUT), NULL);
  MODE (ROW_SIMPLOUT) = stand_env->moids;
  SLICE (MODE (ROW_SIMPLOUT)) = MODE (SIMPLOUT);
/* PIPE. */
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (INT), add_token (&top_token, "pid")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (REF_FILE), add_token (&top_token, "write")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (REF_FILE), add_token (&top_token, "read")->text, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  EQUIVALENT (MODE (PIPE)) = stand_env->moids;
  MODE (PIPE) = stand_env->moids;
  MODE (PIPE)->portable = A68_FALSE;
  z = NULL;
  (void) add_mode_to_pack (&z, MODE (REF_INT), add_token (&top_token, "pid")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (REF_REF_FILE), add_token (&top_token, "write")->text, NULL);
  (void) add_mode_to_pack (&z, MODE (REF_REF_FILE), add_token (&top_token, "read")->text, NULL);
  (void) add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_PIPE)) = stand_env->moids;
}

/*!
\brief set up standenv - general RR but not transput
**/

static void stand_prelude (void)
{
  MOID_T *m;
/* Identifiers. */
  a68_idf (A68_STD, "intlengths", MODE (INT), genie_int_lengths);
  a68_idf (A68_STD, "intshorths", MODE (INT), genie_int_shorths);
  a68_idf (A68_STD, "maxint", MODE (INT), genie_max_int);
  a68_idf (A68_STD, "maxreal", MODE (REAL), genie_max_real);
  a68_idf (A68_STD, "minreal", MODE (REAL), genie_min_real);
  a68_idf (A68_STD, "smallreal", MODE (REAL), genie_small_real);
  a68_idf (A68_STD, "reallengths", MODE (INT), genie_real_lengths);
  a68_idf (A68_STD, "realshorths", MODE (INT), genie_real_shorths);
  a68_idf (A68_STD, "compllengths", MODE (INT), genie_complex_lengths);
  a68_idf (A68_STD, "complshorths", MODE (INT), genie_complex_shorths);
  a68_idf (A68_STD, "bitslengths", MODE (INT), genie_bits_lengths);
  a68_idf (A68_STD, "bitsshorths", MODE (INT), genie_bits_shorths);
  a68_idf (A68_STD, "bitswidth", MODE (INT), genie_bits_width);
  a68_idf (A68_STD, "longbitswidth", MODE (INT), genie_long_bits_width);
  a68_idf (A68_STD, "longlongbitswidth", MODE (INT), genie_longlong_bits_width);
  a68_idf (A68_STD, "maxbits", MODE (BITS), genie_max_bits);
  a68_idf (A68_STD, "longmaxbits", MODE (LONG_BITS), genie_long_max_bits);
  a68_idf (A68_STD, "longlongmaxbits", MODE (LONGLONG_BITS), genie_longlong_max_bits);
  a68_idf (A68_STD, "byteslengths", MODE (INT), genie_bytes_lengths);
  a68_idf (A68_STD, "bytesshorths", MODE (INT), genie_bytes_shorths);
  a68_idf (A68_STD, "byteswidth", MODE (INT), genie_bytes_width);
  a68_idf (A68_STD, "maxabschar", MODE (INT), genie_max_abs_char);
  a68_idf (A68_STD, "pi", MODE (REAL), genie_pi);
  a68_idf (A68_STD, "dpi", MODE (LONG_REAL), genie_pi_long_mp);
  a68_idf (A68_STD, "longpi", MODE (LONG_REAL), genie_pi_long_mp);
  a68_idf (A68_STD, "qpi", MODE (LONGLONG_REAL), genie_pi_long_mp);
  a68_idf (A68_STD, "longlongpi", MODE (LONGLONG_REAL), genie_pi_long_mp);
  a68_idf (A68_STD, "intwidth", MODE (INT), genie_int_width);
  a68_idf (A68_STD, "realwidth", MODE (INT), genie_real_width);
  a68_idf (A68_STD, "expwidth", MODE (INT), genie_exp_width);
  a68_idf (A68_STD, "longintwidth", MODE (INT), genie_long_int_width);
  a68_idf (A68_STD, "longlongintwidth", MODE (INT), genie_longlong_int_width);
  a68_idf (A68_STD, "longrealwidth", MODE (INT), genie_long_real_width);
  a68_idf (A68_STD, "longlongrealwidth", MODE (INT), genie_longlong_real_width);
  a68_idf (A68_STD, "longexpwidth", MODE (INT), genie_long_exp_width);
  a68_idf (A68_STD, "longlongexpwidth", MODE (INT), genie_longlong_exp_width);
  a68_idf (A68_STD, "longmaxint", MODE (LONG_INT), genie_long_max_int);
  a68_idf (A68_STD, "longlongmaxint", MODE (LONGLONG_INT), genie_longlong_max_int);
  a68_idf (A68_STD, "longsmallreal", MODE (LONG_REAL), genie_long_small_real);
  a68_idf (A68_STD, "longlongsmallreal", MODE (LONGLONG_REAL), genie_longlong_small_real);
  a68_idf (A68_STD, "longmaxreal", MODE (LONG_REAL), genie_long_max_real);
  a68_idf (A68_STD, "longminreal", MODE (LONG_REAL), genie_long_min_real);
  a68_idf (A68_STD, "longlongmaxreal", MODE (LONGLONG_REAL), genie_longlong_max_real);
  a68_idf (A68_STD, "longlongminreal", MODE (LONGLONG_REAL), genie_longlong_min_real);
  a68_idf (A68_STD, "longbyteswidth", MODE (INT), genie_long_bytes_width);
  a68_idf (A68_EXT, "seconds", MODE (REAL), genie_seconds);
  a68_idf (A68_EXT, "clock", MODE (REAL), genie_cputime);
  a68_idf (A68_EXT, "cputime", MODE (REAL), genie_cputime);
  m = proc_int;
  a68_idf (A68_EXT, "collections", m, genie_garbage_collections);
  m = a68_proc (MODE (LONG_INT), NULL);
  a68_idf (A68_EXT, "garbage", m, genie_garbage_freed);
  m = proc_real;
  a68_idf (A68_EXT, "collectseconds", m, genie_garbage_seconds);
  a68_idf (A68_EXT, "stackpointer", MODE (INT), genie_stack_pointer);
  a68_idf (A68_EXT, "systemstackpointer", MODE (INT), genie_system_stack_pointer);
  a68_idf (A68_EXT, "systemstacksize", MODE (INT), genie_system_stack_size);
  a68_idf (A68_EXT, "actualstacksize", MODE (INT), genie_stack_pointer);
  m = proc_void;
  a68_idf (A68_EXT, "sweepheap", m, genie_sweep_heap);
  a68_idf (A68_EXT, "preemptivesweepheap", m, genie_preemptive_sweep_heap);
  a68_idf (A68_EXT, "break", m, genie_break);
  a68_idf (A68_EXT, "debug", m, genie_debug);
  a68_idf (A68_EXT, "monitor", m, genie_debug);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A68_EXT, "evaluate", m, genie_evaluate);
  m = a68_proc (MODE (INT), MODE (STRING), NULL);
  a68_idf (A68_EXT, "system", m, genie_system);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A68_EXT, "acronym", m, genie_acronym);
  a68_idf (A68_EXT, "vmsacronym", m, genie_acronym);
/* BITS procedures. */
  m = a68_proc (MODE (BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A68_STD, "bitspack", m, genie_bits_pack);
  m = a68_proc (MODE (LONG_BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A68_STD, "longbitspack", m, genie_long_bits_pack);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A68_STD, "longlongbitspack", m, genie_long_bits_pack);
/* RNG procedures. */
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A68_STD, "firstrandom", m, genie_first_random);
  m = proc_real;
  a68_idf (A68_STD, "nextrandom", m, genie_next_random);
  a68_idf (A68_STD, "random", m, genie_next_random);
  m = a68_proc (MODE (LONG_REAL), NULL);
  a68_idf (A68_STD, "longnextrandom", m, genie_long_next_random);
  a68_idf (A68_STD, "longrandom", m, genie_long_next_random);
  m = a68_proc (MODE (LONGLONG_REAL), NULL);
  a68_idf (A68_STD, "longlongnextrandom", m, genie_long_next_random);
  a68_idf (A68_STD, "longlongrandom", m, genie_long_next_random);
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
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_int);
  a68_op (A68_STD, "ABS", m, genie_abs_int);
  a68_op (A68_STD, "SIGN", m, genie_sign_int);
  m = a68_proc (MODE (BOOL), MODE (INT), NULL);
  a68_op (A68_STD, "ODD", m, genie_odd_int);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (INT), NULL);
  a68_op (A68_STD, "=", m, genie_eq_int);
  a68_op (A68_STD, "/=", m, genie_ne_int);
  a68_op (A68_STD, "~=", m, genie_ne_int);
  a68_op (A68_STD, "^=", m, genie_ne_int);
  a68_op (A68_STD, "<", m, genie_lt_int);
  a68_op (A68_STD, "<=", m, genie_le_int);
  a68_op (A68_STD, ">", m, genie_gt_int);
  a68_op (A68_STD, ">=", m, genie_ge_int);
  a68_op (A68_STD, "EQ", m, genie_eq_int);
  a68_op (A68_STD, "NE", m, genie_ne_int);
  a68_op (A68_STD, "LT", m, genie_lt_int);
  a68_op (A68_STD, "LE", m, genie_le_int);
  a68_op (A68_STD, "GT", m, genie_gt_int);
  a68_op (A68_STD, "GE", m, genie_ge_int);
  m = a68_proc (MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_op (A68_STD, "+", m, genie_add_int);
  a68_op (A68_STD, "-", m, genie_sub_int);
  a68_op (A68_STD, "*", m, genie_mul_int);
  a68_op (A68_STD, "OVER", m, genie_over_int);
  a68_op (A68_STD, "%", m, genie_over_int);
  a68_op (A68_STD, "MOD", m, genie_mod_int);
  a68_op (A68_STD, "%*", m, genie_mod_int);
  a68_op (A68_STD, "**", m, genie_pow_int);
  a68_op (A68_STD, "UP", m, genie_pow_int);
  a68_op (A68_STD, "^", m, genie_pow_int);
  m = a68_proc (MODE (REAL), MODE (INT), MODE (INT), NULL);
  a68_op (A68_STD, "/", m, genie_div_int);
  m = a68_proc (MODE (REF_INT), MODE (REF_INT), MODE (INT), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_int);
  a68_op (A68_STD, "-:=", m, genie_minusab_int);
  a68_op (A68_STD, "*:=", m, genie_timesab_int);
  a68_op (A68_STD, "%:=", m, genie_overab_int);
  a68_op (A68_STD, "%*:=", m, genie_modab_int);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_int);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_int);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_int);
  a68_op (A68_STD, "OVERAB", m, genie_overab_int);
  a68_op (A68_STD, "MODAB", m, genie_modab_int);
/* REAL ops. */
  m = proc_real_real;
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_real);
  a68_op (A68_STD, "ABS", m, genie_abs_real);
  m = a68_proc (MODE (INT), MODE (REAL), NULL);
  a68_op (A68_STD, "SIGN", m, genie_sign_real);
  a68_op (A68_STD, "ROUND", m, genie_round_real);
  a68_op (A68_STD, "ENTIER", m, genie_entier_real);
  m = a68_proc (MODE (BOOL), MODE (REAL), MODE (REAL), NULL);
  a68_op (A68_STD, "=", m, genie_eq_real);
  a68_op (A68_STD, "/=", m, genie_ne_real);
  a68_op (A68_STD, "~=", m, genie_ne_real);
  a68_op (A68_STD, "^=", m, genie_ne_real);
  a68_op (A68_STD, "<", m, genie_lt_real);
  a68_op (A68_STD, "<=", m, genie_le_real);
  a68_op (A68_STD, ">", m, genie_gt_real);
  a68_op (A68_STD, ">=", m, genie_ge_real);
  a68_op (A68_STD, "EQ", m, genie_eq_real);
  a68_op (A68_STD, "NE", m, genie_ne_real);
  a68_op (A68_STD, "LT", m, genie_lt_real);
  a68_op (A68_STD, "LE", m, genie_le_real);
  a68_op (A68_STD, "GT", m, genie_gt_real);
  a68_op (A68_STD, "GE", m, genie_ge_real);
  m = proc_real_real_real;
  a68_op (A68_STD, "+", m, genie_add_real);
  a68_op (A68_STD, "-", m, genie_sub_real);
  a68_op (A68_STD, "*", m, genie_mul_real);
  a68_op (A68_STD, "/", m, genie_div_real);
  a68_op (A68_STD, "**", m, genie_pow_real);
  a68_op (A68_STD, "UP", m, genie_pow_real);
  a68_op (A68_STD, "^", m, genie_pow_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (INT), NULL);
  a68_op (A68_STD, "**", m, genie_pow_real_int);
  a68_op (A68_STD, "UP", m, genie_pow_real_int);
  a68_op (A68_STD, "^", m, genie_pow_real_int);
  m = a68_proc (MODE (REF_REAL), MODE (REF_REAL), MODE (REAL), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_real);
  a68_op (A68_STD, "-:=", m, genie_minusab_real);
  a68_op (A68_STD, "*:=", m, genie_timesab_real);
  a68_op (A68_STD, "/:=", m, genie_divab_real);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_real);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_real);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_real);
  a68_op (A68_STD, "DIVAB", m, genie_divab_real);
  m = proc_real_real;
  a68_idf (A68_STD, "sqrt", m, genie_sqrt_real);
  a68_idf (A68_EXT, "cbrt", m, genie_curt_real);
  a68_idf (A68_EXT, "curt", m, genie_curt_real);
  a68_idf (A68_STD, "exp", m, genie_exp_real);
  a68_idf (A68_STD, "ln", m, genie_ln_real);
  a68_idf (A68_STD, "log", m, genie_log_real);
  a68_idf (A68_STD, "sin", m, genie_sin_real);
  a68_idf (A68_STD, "cos", m, genie_cos_real);
  a68_idf (A68_STD, "tan", m, genie_tan_real);
  a68_idf (A68_STD, "asin", m, genie_arcsin_real);
  a68_idf (A68_STD, "acos", m, genie_arccos_real);
  a68_idf (A68_STD, "atan", m, genie_arctan_real);
  a68_idf (A68_STD, "arcsin", m, genie_arcsin_real);
  a68_idf (A68_STD, "arccos", m, genie_arccos_real);
  a68_idf (A68_STD, "arctan", m, genie_arctan_real);
  a68_idf (A68_EXT, "sinh", m, genie_sinh_real);
  a68_idf (A68_EXT, "cosh", m, genie_cosh_real);
  a68_idf (A68_EXT, "tanh", m, genie_tanh_real);
  a68_idf (A68_EXT, "asinh", m, genie_arcsinh_real);
  a68_idf (A68_EXT, "acosh", m, genie_arccosh_real);
  a68_idf (A68_EXT, "atanh", m, genie_arctanh_real);
  a68_idf (A68_EXT, "arcsinh", m, genie_arcsinh_real);
  a68_idf (A68_EXT, "arccosh", m, genie_arccosh_real);
  a68_idf (A68_EXT, "arctanh", m, genie_arctanh_real);
  a68_idf (A68_EXT, "inverseerf", m, genie_inverf_real);
  a68_idf (A68_EXT, "inverseerfc", m, genie_inverfc_real);
  m = proc_real_real_real;
  a68_idf (A68_EXT, "arctan2", m, genie_atan2_real);
  m = proc_real_real_real_real;
  a68_idf (A68_EXT, "lje126", m, genie_lj_e_12_6);
  a68_idf (A68_EXT, "ljf126", m, genie_lj_f_12_6);
/* COMPLEX ops. */
  m = a68_proc (MODE (COMPLEX), MODE (REAL), MODE (REAL), NULL);
  a68_op (A68_STD, "I", m, genie_icomplex);
  a68_op (A68_STD, "+*", m, genie_icomplex);
  m = a68_proc (MODE (COMPLEX), MODE (INT), MODE (INT), NULL);
  a68_op (A68_STD, "I", m, genie_iint_complex);
  a68_op (A68_STD, "+*", m, genie_iint_complex);
  m = a68_proc (MODE (REAL), MODE (COMPLEX), NULL);
  a68_op (A68_STD, "RE", m, genie_re_complex);
  a68_op (A68_STD, "IM", m, genie_im_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_complex);
  m = proc_complex_complex;
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_complex);
  m = a68_proc (MODE (BOOL), MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_STD, "=", m, genie_eq_complex);
  a68_op (A68_STD, "/=", m, genie_ne_complex);
  a68_op (A68_STD, "~=", m, genie_ne_complex);
  a68_op (A68_STD, "^=", m, genie_ne_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_complex);
  a68_op (A68_STD, "NE", m, genie_ne_complex);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_STD, "+", m, genie_add_complex);
  a68_op (A68_STD, "-", m, genie_sub_complex);
  a68_op (A68_STD, "*", m, genie_mul_complex);
  a68_op (A68_STD, "/", m, genie_div_complex);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), MODE (INT), NULL);
  a68_op (A68_STD, "**", m, genie_pow_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_complex_int);
  m = a68_proc (MODE (REF_COMPLEX), MODE (REF_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_complex);
/* BOOL ops. */
  m = a68_proc (MODE (BOOL), MODE (BOOL), NULL);
  a68_op (A68_STD, "NOT", m, genie_not_bool);
  a68_op (A68_STD, "~", m, genie_not_bool);
  m = a68_proc (MODE (INT), MODE (BOOL), NULL);
  a68_op (A68_STD, "ABS", m, genie_abs_bool);
  m = a68_proc (MODE (BOOL), MODE (BOOL), MODE (BOOL), NULL);
  a68_op (A68_STD, "OR", m, genie_or_bool);
  a68_op (A68_STD, "AND", m, genie_and_bool);
  a68_op (A68_STD, "&", m, genie_and_bool);
  a68_op (A68_EXT, "XOR", m, genie_xor_bool);
  a68_op (A68_STD, "=", m, genie_eq_bool);
  a68_op (A68_STD, "/=", m, genie_ne_bool);
  a68_op (A68_STD, "~=", m, genie_ne_bool);
  a68_op (A68_STD, "^=", m, genie_ne_bool);
  a68_op (A68_STD, "EQ", m, genie_eq_bool);
  a68_op (A68_STD, "NE", m, genie_ne_bool);
/* CHAR ops. */
  m = a68_proc (MODE (BOOL), MODE (CHAR), MODE (CHAR), NULL);
  a68_op (A68_STD, "=", m, genie_eq_char);
  a68_op (A68_STD, "/=", m, genie_ne_char);
  a68_op (A68_STD, "~=", m, genie_ne_char);
  a68_op (A68_STD, "^=", m, genie_ne_char);
  a68_op (A68_STD, "<", m, genie_lt_char);
  a68_op (A68_STD, "<=", m, genie_le_char);
  a68_op (A68_STD, ">", m, genie_gt_char);
  a68_op (A68_STD, ">=", m, genie_ge_char);
  a68_op (A68_STD, "EQ", m, genie_eq_char);
  a68_op (A68_STD, "NE", m, genie_ne_char);
  a68_op (A68_STD, "LT", m, genie_lt_char);
  a68_op (A68_STD, "LE", m, genie_le_char);
  a68_op (A68_STD, "GT", m, genie_gt_char);
  a68_op (A68_STD, "GE", m, genie_ge_char);
  m = a68_proc (MODE (INT), MODE (CHAR), NULL);
  a68_op (A68_STD, "ABS", m, genie_abs_char);
  m = a68_proc (MODE (CHAR), MODE (INT), NULL);
  a68_op (A68_STD, "REPR", m, genie_repr_char);
  m = a68_proc (MODE (BOOL), MODE (CHAR), NULL);
  a68_idf (A68_EXT, "isalnum", m, genie_is_alnum);
  a68_idf (A68_EXT, "isalpha", m, genie_is_alpha);
  a68_idf (A68_EXT, "iscntrl", m, genie_is_cntrl);
  a68_idf (A68_EXT, "isdigit", m, genie_is_digit);
  a68_idf (A68_EXT, "isgraph", m, genie_is_graph);
  a68_idf (A68_EXT, "islower", m, genie_is_lower);
  a68_idf (A68_EXT, "isprint", m, genie_is_print);
  a68_idf (A68_EXT, "ispunct", m, genie_is_punct);
  a68_idf (A68_EXT, "isspace", m, genie_is_space);
  a68_idf (A68_EXT, "isupper", m, genie_is_upper);
  a68_idf (A68_EXT, "isxdigit", m, genie_is_xdigit);
  m = a68_proc (MODE (CHAR), MODE (CHAR), NULL);
  a68_idf (A68_EXT, "tolower", m, genie_to_lower);
  a68_idf (A68_EXT, "toupper", m, genie_to_upper);
/* BITS ops. */
  m = a68_proc (MODE (INT), MODE (BITS), NULL);
  a68_op (A68_STD, "ABS", m, genie_idle);
  m = a68_proc (MODE (BITS), MODE (INT), NULL);
  a68_op (A68_STD, "BIN", m, genie_bin_int);
  m = a68_proc (MODE (BITS), MODE (BITS), NULL);
  a68_op (A68_STD, "NOT", m, genie_not_bits);
  a68_op (A68_STD, "~", m, genie_not_bits);
  m = a68_proc (MODE (BOOL), MODE (BITS), MODE (BITS), NULL);
  a68_op (A68_STD, "=", m, genie_eq_bits);
  a68_op (A68_STD, "/=", m, genie_ne_bits);
  a68_op (A68_STD, "~=", m, genie_ne_bits);
  a68_op (A68_STD, "^=", m, genie_ne_bits);
  a68_op (A68_STD, "<=", m, genie_le_bits);
  a68_op (A68_STD, ">=", m, genie_ge_bits);
  a68_op (A68_STD, "EQ", m, genie_eq_bits);
  a68_op (A68_STD, "NE", m, genie_ne_bits);
  a68_op (A68_STD, "LE", m, genie_le_bits);
  a68_op (A68_STD, "GE", m, genie_ge_bits);
  m = a68_proc (MODE (BITS), MODE (BITS), MODE (BITS), NULL);
  a68_op (A68_STD, "AND", m, genie_and_bits);
  a68_op (A68_STD, "&", m, genie_and_bits);
  a68_op (A68_STD, "OR", m, genie_or_bits);
  a68_op (A68_EXT, "XOR", m, genie_xor_bits);
  m = a68_proc (MODE (BITS), MODE (BITS), MODE (INT), NULL);
  a68_op (A68_STD, "SHL", m, genie_shl_bits);
  a68_op (A68_STD, "UP", m, genie_shl_bits);
  a68_op (A68_STD, "SHR", m, genie_shr_bits);
  a68_op (A68_STD, "DOWN", m, genie_shr_bits);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (BITS), NULL);
  a68_op (A68_STD, "ELEM", m, genie_elem_bits);
  m = a68_proc (MODE (BITS), MODE (INT), MODE (BITS), NULL);
  a68_op (A68_STD, "SET", m, genie_set_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_bits);
/* BYTES ops. */
  m = a68_proc (MODE (BYTES), MODE (STRING), NULL);
  a68_idf (A68_STD, "bytespack", m, genie_bytespack);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (BYTES), NULL);
  a68_op (A68_STD, "ELEM", m, genie_elem_bytes);
  m = a68_proc (MODE (BYTES), MODE (BYTES), MODE (BYTES), NULL);
  a68_op (A68_STD, "+", m, genie_add_bytes);
  m = a68_proc (MODE (REF_BYTES), MODE (REF_BYTES), MODE (BYTES), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_bytes);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_bytes);
  m = a68_proc (MODE (REF_BYTES), MODE (BYTES), MODE (REF_BYTES), NULL);
  a68_op (A68_STD, "+=:", m, genie_plusto_bytes);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_bytes);
  m = a68_proc (MODE (BOOL), MODE (BYTES), MODE (BYTES), NULL);
  a68_op (A68_STD, "=", m, genie_eq_bytes);
  a68_op (A68_STD, "/=", m, genie_ne_bytes);
  a68_op (A68_STD, "~=", m, genie_ne_bytes);
  a68_op (A68_STD, "^=", m, genie_ne_bytes);
  a68_op (A68_STD, "<", m, genie_lt_bytes);
  a68_op (A68_STD, "<=", m, genie_le_bytes);
  a68_op (A68_STD, ">", m, genie_gt_bytes);
  a68_op (A68_STD, ">=", m, genie_ge_bytes);
  a68_op (A68_STD, "EQ", m, genie_eq_bytes);
  a68_op (A68_STD, "NE", m, genie_ne_bytes);
  a68_op (A68_STD, "LT", m, genie_lt_bytes);
  a68_op (A68_STD, "LE", m, genie_le_bytes);
  a68_op (A68_STD, "GT", m, genie_gt_bytes);
  a68_op (A68_STD, "GE", m, genie_ge_bytes);
/* LONG BYTES ops. */
  m = a68_proc (MODE (LONG_BYTES), MODE (BYTES), NULL);
  a68_op (A68_STD, "LENG", m, genie_leng_bytes);
  m = a68_proc (MODE (BYTES), MODE (LONG_BYTES), NULL);
  a68_idf (A68_STD, "SHORTEN", m, genie_shorten_bytes);
  m = a68_proc (MODE (LONG_BYTES), MODE (STRING), NULL);
  a68_idf (A68_STD, "longbytespack", m, genie_long_bytespack);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (LONG_BYTES), NULL);
  a68_op (A68_STD, "ELEM", m, genie_elem_long_bytes);
  m = a68_proc (MODE (LONG_BYTES), MODE (LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A68_STD, "+", m, genie_add_long_bytes);
  m = a68_proc (MODE (REF_LONG_BYTES), MODE (REF_LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_bytes);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_bytes);
  m = a68_proc (MODE (REF_LONG_BYTES), MODE (LONG_BYTES), MODE (REF_LONG_BYTES), NULL);
  a68_op (A68_STD, "+=:", m, genie_plusto_long_bytes);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_long_bytes);
  m = a68_proc (MODE (BOOL), MODE (LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_bytes);
  a68_op (A68_STD, "/=", m, genie_ne_long_bytes);
  a68_op (A68_STD, "~=", m, genie_ne_long_bytes);
  a68_op (A68_STD, "^=", m, genie_ne_long_bytes);
  a68_op (A68_STD, "<", m, genie_lt_long_bytes);
  a68_op (A68_STD, "<=", m, genie_le_long_bytes);
  a68_op (A68_STD, ">", m, genie_gt_long_bytes);
  a68_op (A68_STD, ">=", m, genie_ge_long_bytes);
  a68_op (A68_STD, "EQ", m, genie_eq_long_bytes);
  a68_op (A68_STD, "NE", m, genie_ne_long_bytes);
  a68_op (A68_STD, "LT", m, genie_lt_long_bytes);
  a68_op (A68_STD, "LE", m, genie_le_long_bytes);
  a68_op (A68_STD, "GT", m, genie_gt_long_bytes);
  a68_op (A68_STD, "GE", m, genie_ge_long_bytes);
/* STRING ops. */
  m = a68_proc (MODE (BOOL), MODE (STRING), MODE (STRING), NULL);
  a68_op (A68_STD, "=", m, genie_eq_string);
  a68_op (A68_STD, "/=", m, genie_ne_string);
  a68_op (A68_STD, "~=", m, genie_ne_string);
  a68_op (A68_STD, "^=", m, genie_ne_string);
  a68_op (A68_STD, "<", m, genie_lt_string);
  a68_op (A68_STD, "<=", m, genie_le_string);
  a68_op (A68_STD, ">=", m, genie_ge_string);
  a68_op (A68_STD, ">", m, genie_gt_string);
  a68_op (A68_STD, "EQ", m, genie_eq_string);
  a68_op (A68_STD, "NE", m, genie_ne_string);
  a68_op (A68_STD, "LT", m, genie_lt_string);
  a68_op (A68_STD, "LE", m, genie_le_string);
  a68_op (A68_STD, "GE", m, genie_ge_string);
  a68_op (A68_STD, "GT", m, genie_gt_string);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (STRING), NULL);
  a68_op (A68_STD, "ELEM", m, genie_elem_string);
  m = a68_proc (MODE (STRING), MODE (CHAR), MODE (CHAR), NULL);
  a68_op (A68_STD, "+", m, genie_add_char);
  m = a68_proc (MODE (STRING), MODE (STRING), MODE (STRING), NULL);
  a68_op (A68_STD, "+", m, genie_add_string);
  m = a68_proc (MODE (REF_STRING), MODE (REF_STRING), MODE (STRING), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_string);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_string);
  m = a68_proc (MODE (REF_STRING), MODE (REF_STRING), MODE (INT), NULL);
  a68_op (A68_STD, "*:=", m, genie_timesab_string);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_string);
  m = a68_proc (MODE (REF_STRING), MODE (STRING), MODE (REF_STRING), NULL);
  a68_op (A68_STD, "+=:", m, genie_plusto_string);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_string);
  m = a68_proc (MODE (STRING), MODE (STRING), MODE (INT), NULL);
  a68_op (A68_STD, "*", m, genie_times_string_int);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (STRING), NULL);
  a68_op (A68_STD, "*", m, genie_times_int_string);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (CHAR), NULL);
  a68_op (A68_STD, "*", m, genie_times_int_char);
  m = a68_proc (MODE (STRING), MODE (CHAR), MODE (INT), NULL);
  a68_op (A68_STD, "*", m, genie_times_char_int);
/* [] CHAR as cross term for STRING. */
  m = a68_proc (MODE (BOOL), MODE (ROW_CHAR), MODE (ROW_CHAR), NULL);
  a68_op (A68_STD, "=", m, genie_eq_string);
  a68_op (A68_STD, "/=", m, genie_ne_string);
  a68_op (A68_STD, "~=", m, genie_ne_string);
  a68_op (A68_STD, "^=", m, genie_ne_string);
  a68_op (A68_STD, "<", m, genie_lt_string);
  a68_op (A68_STD, "<=", m, genie_le_string);
  a68_op (A68_STD, ">=", m, genie_ge_string);
  a68_op (A68_STD, ">", m, genie_gt_string);
  a68_op (A68_STD, "EQ", m, genie_eq_string);
  a68_op (A68_STD, "NE", m, genie_ne_string);
  a68_op (A68_STD, "LT", m, genie_lt_string);
  a68_op (A68_STD, "LE", m, genie_le_string);
  a68_op (A68_STD, "GE", m, genie_ge_string);
  a68_op (A68_STD, "GT", m, genie_gt_string);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (ROW_CHAR), NULL);
  a68_op (A68_STD, "ELEM", m, genie_elem_string);
  m = a68_proc (MODE (STRING), MODE (ROW_CHAR), MODE (ROW_CHAR), NULL);
  a68_op (A68_STD, "+", m, genie_add_string);
  m = a68_proc (MODE (STRING), MODE (ROW_CHAR), MODE (INT), NULL);
  a68_op (A68_STD, "*", m, genie_times_string_int);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (ROW_CHAR), NULL);
  a68_op (A68_STD, "*", m, genie_times_int_string);
/* SEMA ops. */
#if defined ENABLE_PAR_CLAUSE
  m = a68_proc (MODE (SEMA), MODE (INT), NULL);
  a68_op (A68_STD, "LEVEL", m, genie_level_sema_int);
  m = a68_proc (MODE (INT), MODE (SEMA), NULL);
  a68_op (A68_STD, "LEVEL", m, genie_level_int_sema);
  m = a68_proc (MODE (VOID), MODE (SEMA), NULL);
  a68_op (A68_STD, "UP", m, genie_up_sema);
  a68_op (A68_STD, "DOWN", m, genie_down_sema);
#else
  m = a68_proc (MODE (SEMA), MODE (INT), NULL);
  a68_op (A68_STD, "LEVEL", m, genie_unimplemented);
  m = a68_proc (MODE (INT), MODE (SEMA), NULL);
  a68_op (A68_STD, "LEVEL", m, genie_unimplemented);
  m = a68_proc (MODE (VOID), MODE (SEMA), NULL);
  a68_op (A68_STD, "UP", m, genie_unimplemented);
  a68_op (A68_STD, "DOWN", m, genie_unimplemented);
#endif
/* ROWS ops. */
  m = a68_proc (MODE (INT), MODE (ROWS), NULL);
  a68_op (A68_EXT, "ELEMS", m, genie_monad_elems);
  a68_op (A68_STD, "LWB", m, genie_monad_lwb);
  a68_op (A68_STD, "UPB", m, genie_monad_upb);
  m = a68_proc (MODE (INT), MODE (INT), MODE (ROWS), NULL);
  a68_op (A68_EXT, "ELEMS", m, genie_dyad_elems);
  a68_op (A68_STD, "LWB", m, genie_dyad_lwb);
  a68_op (A68_STD, "UPB", m, genie_dyad_upb);
  m = a68_proc (MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_op (A68_EXT, "SORT", m, genie_sort_row_string);
/* Binding for the multiple-precision library. */
/* LONG INT. */
  m = a68_proc (MODE (LONG_INT), MODE (INT), NULL);
  a68_op (A68_STD, "LENG", m, genie_lengthen_int_to_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_long_mp);
  m = a68_proc (MODE (INT), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_int);
  a68_op (A68_STD, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "ODD", m, genie_odd_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "ENTIER", m, genie_entier_long_mp);
  a68_op (A68_STD, "ROUND", m, genie_round_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "+", m, genie_add_long_int);
  a68_op (A68_STD, "-", m, genie_minus_long_int);
  a68_op (A68_STD, "*", m, genie_mul_long_int);
  a68_op (A68_STD, "OVER", m, genie_over_long_mp);
  a68_op (A68_STD, "%", m, genie_over_long_mp);
  a68_op (A68_STD, "MOD", m, genie_mod_long_mp);
  a68_op (A68_STD, "%*", m, genie_mod_long_mp);
  m = a68_proc (MODE (REF_LONG_INT), MODE (REF_LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_int);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_int);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_int);
  a68_op (A68_STD, "%:=", m, genie_overab_long_mp);
  a68_op (A68_STD, "%*:=", m, genie_modab_long_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_int);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_int);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_int);
  a68_op (A68_STD, "OVERAB", m, genie_overab_long_mp);
  a68_op (A68_STD, "MODAB", m, genie_modab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_long_mp);
  a68_op (A68_STD, "/=", m, genie_ne_long_mp);
  a68_op (A68_STD, "~=", m, genie_ne_long_mp);
  a68_op (A68_STD, "^=", m, genie_ne_long_mp);
  a68_op (A68_STD, "NE", m, genie_ne_long_mp);
  a68_op (A68_STD, "<", m, genie_lt_long_mp);
  a68_op (A68_STD, "LT", m, genie_lt_long_mp);
  a68_op (A68_STD, "<=", m, genie_le_long_mp);
  a68_op (A68_STD, "LE", m, genie_le_long_mp);
  a68_op (A68_STD, ">", m, genie_gt_long_mp);
  a68_op (A68_STD, "GT", m, genie_gt_long_mp);
  a68_op (A68_STD, ">=", m, genie_ge_long_mp);
  a68_op (A68_STD, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "/", m, genie_div_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), MODE (INT), NULL);
  a68_op (A68_STD, "**", m, genie_pow_long_mp_int_int);
  a68_op (A68_STD, "^", m, genie_pow_long_mp_int_int);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
/* LONG REAL. */
  m = a68_proc (MODE (LONG_REAL), MODE (REAL), NULL);
  a68_op (A68_STD, "LENG", m, genie_lengthen_real_to_long_mp);
  m = a68_proc (MODE (REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_real);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_long_mp);
  a68_idf (A68_STD, "longsqrt", m, genie_sqrt_long_mp);
  a68_idf (A68_EXT, "longcbrt", m, genie_curt_long_mp);
  a68_idf (A68_EXT, "longcurt", m, genie_curt_long_mp);
  a68_idf (A68_STD, "longexp", m, genie_exp_long_mp);
  a68_idf (A68_STD, "longln", m, genie_ln_long_mp);
  a68_idf (A68_STD, "longlog", m, genie_log_long_mp);
  a68_idf (A68_STD, "longsin", m, genie_sin_long_mp);
  a68_idf (A68_STD, "longcos", m, genie_cos_long_mp);
  a68_idf (A68_STD, "longtan", m, genie_tan_long_mp);
  a68_idf (A68_STD, "longasin", m, genie_asin_long_mp);
  a68_idf (A68_STD, "longacos", m, genie_acos_long_mp);
  a68_idf (A68_STD, "longatan", m, genie_atan_long_mp);
  a68_idf (A68_STD, "longarcsin", m, genie_asin_long_mp);
  a68_idf (A68_STD, "longarccos", m, genie_acos_long_mp);
  a68_idf (A68_STD, "longarctan", m, genie_atan_long_mp);
  a68_idf (A68_EXT, "longsinh", m, genie_sinh_long_mp);
  a68_idf (A68_EXT, "longcosh", m, genie_cosh_long_mp);
  a68_idf (A68_EXT, "longtanh", m, genie_tanh_long_mp);
  a68_idf (A68_EXT, "longasinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_EXT, "longacosh", m, genie_arccosh_long_mp);
  a68_idf (A68_EXT, "longatanh", m, genie_arctanh_long_mp);
  a68_idf (A68_EXT, "longarcsinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_EXT, "longarccosh", m, genie_arccosh_long_mp);
  a68_idf (A68_EXT, "longarctanh", m, genie_arctanh_long_mp);
  a68_idf (A68_EXT, "dsqrt", m, genie_sqrt_long_mp);
  a68_idf (A68_EXT, "dcbrt", m, genie_curt_long_mp);
  a68_idf (A68_EXT, "dcurt", m, genie_curt_long_mp);
  a68_idf (A68_EXT, "dexp", m, genie_exp_long_mp);
  a68_idf (A68_EXT, "dln", m, genie_ln_long_mp);
  a68_idf (A68_EXT, "dlog", m, genie_log_long_mp);
  a68_idf (A68_EXT, "dsin", m, genie_sin_long_mp);
  a68_idf (A68_EXT, "dcos", m, genie_cos_long_mp);
  a68_idf (A68_EXT, "dtan", m, genie_tan_long_mp);
  a68_idf (A68_EXT, "dasin", m, genie_asin_long_mp);
  a68_idf (A68_EXT, "dacos", m, genie_acos_long_mp);
  a68_idf (A68_EXT, "datan", m, genie_atan_long_mp);
  a68_idf (A68_EXT, "dsinh", m, genie_sinh_long_mp);
  a68_idf (A68_EXT, "dcosh", m, genie_cosh_long_mp);
  a68_idf (A68_EXT, "dtanh", m, genie_tanh_long_mp);
  a68_idf (A68_EXT, "dasinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_EXT, "dacosh", m, genie_arccosh_long_mp);
  a68_idf (A68_EXT, "datanh", m, genie_arctanh_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_idf (A68_STD, "longarctan2", m, genie_atan2_long_mp);
  a68_idf (A68_STD, "darctan2", m, genie_atan2_long_mp);
  m = a68_proc (MODE (INT), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "+", m, genie_add_long_mp);
  a68_op (A68_STD, "-", m, genie_sub_long_mp);
  a68_op (A68_STD, "*", m, genie_mul_long_mp);
  a68_op (A68_STD, "/", m, genie_div_long_mp);
  a68_op (A68_STD, "**", m, genie_pow_long_mp);
  a68_op (A68_STD, "UP", m, genie_pow_long_mp);
  a68_op (A68_STD, "^", m, genie_pow_long_mp);
  m = a68_proc (MODE (REF_LONG_REAL), MODE (REF_LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_mp);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_mp);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_mp);
  a68_op (A68_STD, "/:=", m, genie_divab_long_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_mp);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_mp);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_mp);
  a68_op (A68_STD, "DIVAB", m, genie_divab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_long_mp);
  a68_op (A68_STD, "/=", m, genie_ne_long_mp);
  a68_op (A68_STD, "~=", m, genie_ne_long_mp);
  a68_op (A68_STD, "^=", m, genie_ne_long_mp);
  a68_op (A68_STD, "NE", m, genie_ne_long_mp);
  a68_op (A68_STD, "<", m, genie_lt_long_mp);
  a68_op (A68_STD, "LT", m, genie_lt_long_mp);
  a68_op (A68_STD, "<=", m, genie_le_long_mp);
  a68_op (A68_STD, "LE", m, genie_le_long_mp);
  a68_op (A68_STD, ">", m, genie_gt_long_mp);
  a68_op (A68_STD, "GT", m, genie_gt_long_mp);
  a68_op (A68_STD, ">=", m, genie_ge_long_mp);
  a68_op (A68_STD, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (INT), NULL);
  a68_op (A68_STD, "**", m, genie_pow_long_mp_int);
  a68_op (A68_STD, "UP", m, genie_pow_long_mp_int);
  a68_op (A68_STD, "^", m, genie_pow_long_mp_int);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
/* LONG COMPLEX. */
  m = a68_proc (MODE (LONG_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_STD, "LENG", m, genie_lengthen_complex_to_long_complex);
  m = a68_proc (MODE (COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_complex_to_complex);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_STD, "RE", m, genie_re_long_complex);
  a68_op (A68_STD, "IM", m, genie_im_long_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_long_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_STD, "+", m, genie_add_long_complex);
  a68_op (A68_STD, "-", m, genie_sub_long_complex);
  a68_op (A68_STD, "*", m, genie_mul_long_complex);
  a68_op (A68_STD, "/", m, genie_div_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), MODE (INT), NULL);
  a68_op (A68_STD, "**", m, genie_pow_long_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_long_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_long_complex_int);
  m = a68_proc (MODE (BOOL), MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_long_complex);
  a68_op (A68_STD, "/=", m, genie_ne_long_complex);
  a68_op (A68_STD, "~=", m, genie_ne_long_complex);
  a68_op (A68_STD, "^=", m, genie_ne_long_complex);
  a68_op (A68_STD, "NE", m, genie_ne_long_complex);
  m = a68_proc (MODE (REF_LONG_COMPLEX), MODE (REF_LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_long_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_long_complex);
/* LONG BITS ops. */
  m = a68_proc (MODE (LONG_INT), MODE (LONG_BITS), NULL);
  a68_op (A68_STD, "ABS", m, genie_idle);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "BIN", m, genie_bin_long_mp);
  m = a68_proc (MODE (BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (BITS), NULL);
  a68_op (A68_STD, "LENG", m, genie_lengthen_unsigned_to_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_STD, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_STD, "NOT", m, genie_not_long_mp);
  a68_op (A68_STD, "~", m, genie_not_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_long_mp);
  a68_op (A68_STD, "/=", m, genie_ne_long_mp);
  a68_op (A68_STD, "~=", m, genie_ne_long_mp);
  a68_op (A68_STD, "^=", m, genie_ne_long_mp);
  a68_op (A68_STD, "NE", m, genie_ne_long_mp);
  a68_op (A68_STD, "<=", m, genie_le_long_bits);
  a68_op (A68_STD, "LE", m, genie_le_long_bits);
  a68_op (A68_STD, ">=", m, genie_ge_long_bits);
  a68_op (A68_STD, "GE", m, genie_ge_long_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A68_STD, "AND", m, genie_and_long_mp);
  a68_op (A68_STD, "&", m, genie_and_long_mp);
  a68_op (A68_STD, "OR", m, genie_or_long_mp);
  a68_op (A68_EXT, "XOR", m, genie_xor_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), MODE (INT), NULL);
  a68_op (A68_STD, "SHL", m, genie_shl_long_mp);
  a68_op (A68_STD, "UP", m, genie_shl_long_mp);
  a68_op (A68_STD, "SHR", m, genie_shr_long_mp);
  a68_op (A68_STD, "DOWN", m, genie_shr_long_mp);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (LONG_BITS), NULL);
  a68_op (A68_STD, "ELEM", m, genie_elem_long_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (INT), MODE (LONG_BITS), NULL);
  a68_op (A68_STD, "SET", m, genie_set_long_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_long_bits);
/* LONG LONG INT. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONG_INT), NULL);
  a68_op (A68_STD, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_long_mp);
  m = a68_proc (MODE (INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "ODD", m, genie_odd_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_STD, "ENTIER", m, genie_entier_long_mp);
  a68_op (A68_STD, "ROUND", m, genie_round_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "+", m, genie_add_long_int);
  a68_op (A68_STD, "-", m, genie_minus_long_int);
  a68_op (A68_STD, "*", m, genie_mul_long_int);
  a68_op (A68_STD, "OVER", m, genie_over_long_mp);
  a68_op (A68_STD, "%", m, genie_over_long_mp);
  a68_op (A68_STD, "MOD", m, genie_mod_long_mp);
  a68_op (A68_STD, "%*", m, genie_mod_long_mp);
  m = a68_proc (MODE (REF_LONGLONG_INT), MODE (REF_LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_int);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_int);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_int);
  a68_op (A68_STD, "%:=", m, genie_overab_long_mp);
  a68_op (A68_STD, "%*:=", m, genie_modab_long_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_int);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_int);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_int);
  a68_op (A68_STD, "OVERAB", m, genie_overab_long_mp);
  a68_op (A68_STD, "MODAB", m, genie_modab_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "/", m, genie_div_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_long_mp);
  a68_op (A68_STD, "/=", m, genie_ne_long_mp);
  a68_op (A68_STD, "~=", m, genie_ne_long_mp);
  a68_op (A68_STD, "^=", m, genie_ne_long_mp);
  a68_op (A68_STD, "NE", m, genie_ne_long_mp);
  a68_op (A68_STD, "<", m, genie_lt_long_mp);
  a68_op (A68_STD, "LT", m, genie_lt_long_mp);
  a68_op (A68_STD, "<=", m, genie_le_long_mp);
  a68_op (A68_STD, "LE", m, genie_le_long_mp);
  a68_op (A68_STD, ">", m, genie_gt_long_mp);
  a68_op (A68_STD, "GT", m, genie_gt_long_mp);
  a68_op (A68_STD, ">=", m, genie_ge_long_mp);
  a68_op (A68_STD, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), MODE (INT), NULL);
  a68_op (A68_STD, "**", m, genie_pow_long_mp_int_int);
  a68_op (A68_STD, "^", m, genie_pow_long_mp_int_int);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
/* LONG LONG REAL. */
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A68_STD, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_long_mp);
  a68_idf (A68_STD, "longlongsqrt", m, genie_sqrt_long_mp);
  a68_idf (A68_EXT, "longlongcbrt", m, genie_curt_long_mp);
  a68_idf (A68_EXT, "longlongcurt", m, genie_curt_long_mp);
  a68_idf (A68_STD, "longlongexp", m, genie_exp_long_mp);
  a68_idf (A68_STD, "longlongln", m, genie_ln_long_mp);
  a68_idf (A68_STD, "longlonglog", m, genie_log_long_mp);
  a68_idf (A68_STD, "longlongsin", m, genie_sin_long_mp);
  a68_idf (A68_STD, "longlongcos", m, genie_cos_long_mp);
  a68_idf (A68_STD, "longlongtan", m, genie_tan_long_mp);
  a68_idf (A68_STD, "longlongasin", m, genie_asin_long_mp);
  a68_idf (A68_STD, "longlongacos", m, genie_acos_long_mp);
  a68_idf (A68_STD, "longlongatan", m, genie_atan_long_mp);
  a68_idf (A68_STD, "longlongarcsin", m, genie_asin_long_mp);
  a68_idf (A68_STD, "longlongarccos", m, genie_acos_long_mp);
  a68_idf (A68_STD, "longlongarctan", m, genie_atan_long_mp);
  a68_idf (A68_EXT, "longlongsinh", m, genie_sinh_long_mp);
  a68_idf (A68_EXT, "longlongcosh", m, genie_cosh_long_mp);
  a68_idf (A68_EXT, "longlongtanh", m, genie_tanh_long_mp);
  a68_idf (A68_EXT, "longlongasinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_EXT, "longlongacosh", m, genie_arccosh_long_mp);
  a68_idf (A68_EXT, "longlongatanh", m, genie_arctanh_long_mp);
  a68_idf (A68_EXT, "longlongarcsinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_EXT, "longlongarccosh", m, genie_arccosh_long_mp);
  a68_idf (A68_EXT, "longlongarctanh", m, genie_arctanh_long_mp);
  a68_idf (A68_EXT, "qsqrt", m, genie_sqrt_long_mp);
  a68_idf (A68_EXT, "qcbrt", m, genie_curt_long_mp);
  a68_idf (A68_EXT, "qcurt", m, genie_curt_long_mp);
  a68_idf (A68_EXT, "qexp", m, genie_exp_long_mp);
  a68_idf (A68_EXT, "qln", m, genie_ln_long_mp);
  a68_idf (A68_EXT, "qlog", m, genie_log_long_mp);
  a68_idf (A68_EXT, "qsin", m, genie_sin_long_mp);
  a68_idf (A68_EXT, "qcos", m, genie_cos_long_mp);
  a68_idf (A68_EXT, "qtan", m, genie_tan_long_mp);
  a68_idf (A68_EXT, "qasin", m, genie_asin_long_mp);
  a68_idf (A68_EXT, "qacos", m, genie_acos_long_mp);
  a68_idf (A68_EXT, "qatan", m, genie_atan_long_mp);
  a68_idf (A68_EXT, "qsinh", m, genie_sinh_long_mp);
  a68_idf (A68_EXT, "qcosh", m, genie_cosh_long_mp);
  a68_idf (A68_EXT, "qtanh", m, genie_tanh_long_mp);
  a68_idf (A68_EXT, "qasinh", m, genie_arcsinh_long_mp);
  a68_idf (A68_EXT, "qacosh", m, genie_arccosh_long_mp);
  a68_idf (A68_EXT, "qatanh", m, genie_arctanh_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_idf (A68_STD, "longarctan2", m, genie_atan2_long_mp);
  a68_idf (A68_STD, "qarctan2", m, genie_atan2_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_STD, "+", m, genie_add_long_mp);
  a68_op (A68_STD, "-", m, genie_sub_long_mp);
  a68_op (A68_STD, "*", m, genie_mul_long_mp);
  a68_op (A68_STD, "/", m, genie_div_long_mp);
  a68_op (A68_STD, "**", m, genie_pow_long_mp);
  a68_op (A68_STD, "UP", m, genie_pow_long_mp);
  a68_op (A68_STD, "^", m, genie_pow_long_mp);
  m = a68_proc (MODE (REF_LONGLONG_REAL), MODE (REF_LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_mp);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_mp);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_mp);
  a68_op (A68_STD, "/:=", m, genie_divab_long_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_mp);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_mp);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_mp);
  a68_op (A68_STD, "DIVAB", m, genie_divab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_long_mp);
  a68_op (A68_STD, "/=", m, genie_ne_long_mp);
  a68_op (A68_STD, "~=", m, genie_ne_long_mp);
  a68_op (A68_STD, "^=", m, genie_ne_long_mp);
  a68_op (A68_STD, "NE", m, genie_ne_long_mp);
  a68_op (A68_STD, "<", m, genie_lt_long_mp);
  a68_op (A68_STD, "LT", m, genie_lt_long_mp);
  a68_op (A68_STD, "<=", m, genie_le_long_mp);
  a68_op (A68_STD, "LE", m, genie_le_long_mp);
  a68_op (A68_STD, ">", m, genie_gt_long_mp);
  a68_op (A68_STD, "GT", m, genie_gt_long_mp);
  a68_op (A68_STD, ">=", m, genie_ge_long_mp);
  a68_op (A68_STD, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (INT), NULL);
  a68_op (A68_STD, "**", m, genie_pow_long_mp_int);
  a68_op (A68_STD, "UP", m, genie_pow_long_mp_int);
  a68_op (A68_STD, "^", m, genie_pow_long_mp_int);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
/* LONGLONG COMPLEX. */
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A68_STD, "LENG", m, genie_lengthen_long_complex_to_longlong_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_longlong_complex_to_long_complex);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_STD, "RE", m, genie_re_long_complex);
  a68_op (A68_STD, "IM", m, genie_im_long_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_long_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_STD, "+", m, genie_add_long_complex);
  a68_op (A68_STD, "-", m, genie_sub_long_complex);
  a68_op (A68_STD, "*", m, genie_mul_long_complex);
  a68_op (A68_STD, "/", m, genie_div_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), MODE (INT), NULL);
  a68_op (A68_STD, "**", m, genie_pow_long_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_long_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_long_complex_int);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_long_complex);
  a68_op (A68_STD, "/=", m, genie_ne_long_complex);
  a68_op (A68_STD, "~=", m, genie_ne_long_complex);
  a68_op (A68_STD, "^=", m, genie_ne_long_complex);
  a68_op (A68_STD, "NE", m, genie_ne_long_complex);
  m = a68_proc (MODE (REF_LONGLONG_COMPLEX), MODE (REF_LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_long_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_long_complex);
/* LONG LONG BITS. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_STD, "ABS", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "BIN", m, genie_bin_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_STD, "NOT", m, genie_not_long_mp);
  a68_op (A68_STD, "~", m, genie_not_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_STD, "=", m, genie_eq_long_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_long_mp);
  a68_op (A68_STD, "/=", m, genie_ne_long_mp);
  a68_op (A68_STD, "~=", m, genie_ne_long_mp);
  a68_op (A68_STD, "^=", m, genie_ne_long_mp);
  a68_op (A68_STD, "NE", m, genie_ne_long_mp);
  a68_op (A68_STD, "<=", m, genie_le_long_mp);
  a68_op (A68_STD, "LE", m, genie_le_long_mp);
  a68_op (A68_STD, ">=", m, genie_ge_long_mp);
  a68_op (A68_STD, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_STD, "AND", m, genie_and_long_mp);
  a68_op (A68_STD, "&", m, genie_and_long_mp);
  a68_op (A68_STD, "OR", m, genie_or_long_mp);
  a68_op (A68_EXT, "XOR", m, genie_xor_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), MODE (INT), NULL);
  a68_op (A68_STD, "SHL", m, genie_shl_long_mp);
  a68_op (A68_STD, "UP", m, genie_shl_long_mp);
  a68_op (A68_STD, "SHR", m, genie_shr_long_mp);
  a68_op (A68_STD, "DOWN", m, genie_shr_long_mp);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_STD, "ELEM", m, genie_elem_longlong_bits);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (INT), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_STD, "SET", m, genie_set_longlong_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_longlong_bits);
/* Some "terminators" to handle the mapping of very short or very long modes.
   This allows you to write SHORT REAL z = SHORTEN pi while everything is
   silently mapped onto REAL. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (MODE (INT), MODE (INT), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (REAL), MODE (REAL), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (BITS), MODE (BITS), NULL);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
  m = proc_complex_complex;
  a68_idf (A68_EXT, "complexsqrt", m, genie_sqrt_complex);
  a68_idf (A68_EXT, "csqrt", m, genie_sqrt_complex);
  a68_idf (A68_EXT, "complexexp", m, genie_exp_complex);
  a68_idf (A68_EXT, "cexp", m, genie_exp_complex);
  a68_idf (A68_EXT, "complexln", m, genie_ln_complex);
  a68_idf (A68_EXT, "cln", m, genie_ln_complex);
  a68_idf (A68_EXT, "complexsin", m, genie_sin_complex);
  a68_idf (A68_EXT, "csin", m, genie_sin_complex);
  a68_idf (A68_EXT, "complexcos", m, genie_cos_complex);
  a68_idf (A68_EXT, "ccos", m, genie_cos_complex);
  a68_idf (A68_EXT, "complextan", m, genie_tan_complex);
  a68_idf (A68_EXT, "ctan", m, genie_tan_complex);
  a68_idf (A68_EXT, "complexasin", m, genie_arcsin_complex);
  a68_idf (A68_EXT, "casin", m, genie_arcsin_complex);
  a68_idf (A68_EXT, "complexacos", m, genie_arccos_complex);
  a68_idf (A68_EXT, "cacos", m, genie_arccos_complex);
  a68_idf (A68_EXT, "complexatan", m, genie_arctan_complex);
  a68_idf (A68_EXT, "catan", m, genie_arctan_complex);
  a68_idf (A68_EXT, "complexarcsin", m, genie_arcsin_complex);
  a68_idf (A68_EXT, "carcsin", m, genie_arcsin_complex);
  a68_idf (A68_EXT, "complexarccos", m, genie_arccos_complex);
  a68_idf (A68_EXT, "carccos", m, genie_arccos_complex);
  a68_idf (A68_EXT, "complexarctan", m, genie_arctan_complex);
  a68_idf (A68_EXT, "carctan", m, genie_arctan_complex);
#if defined ENABLE_NUMERICAL
  a68_idf (A68_EXT, "complexsinh", m, genie_sinh_complex);
  a68_idf (A68_EXT, "csinh", m, genie_sinh_complex);
  a68_idf (A68_EXT, "complexcosh", m, genie_cosh_complex);
  a68_idf (A68_EXT, "ccosh", m, genie_cosh_complex);
  a68_idf (A68_EXT, "complextanh", m, genie_tanh_complex);
  a68_idf (A68_EXT, "ctanh", m, genie_tanh_complex);
  a68_idf (A68_EXT, "complexasinh", m, genie_arcsinh_complex);
  a68_idf (A68_EXT, "casinh", m, genie_arcsinh_complex);
  a68_idf (A68_EXT, "complexacosh", m, genie_arccosh_complex);
  a68_idf (A68_EXT, "cacosh", m, genie_arccosh_complex);
  a68_idf (A68_EXT, "complexatanh", m, genie_arctanh_complex);
  a68_idf (A68_EXT, "catanh", m, genie_arctanh_complex);
  a68_idf (A68_EXT, "complexarcsinh", m, genie_arcsinh_complex);
  a68_idf (A68_EXT, "carcsinh", m, genie_arcsinh_complex);
  a68_idf (A68_EXT, "complexarccosh", m, genie_arccosh_complex);
  a68_idf (A68_EXT, "carccosh", m, genie_arccosh_complex);
  a68_idf (A68_EXT, "complexarctanh", m, genie_arctanh_complex);
  a68_idf (A68_EXT, "carctanh", m, genie_arctanh_complex);
  m = a68_proc (MODE (REAL), proc_real_real, MODE (REAL), MODE (REF_REAL), NULL);
  a68_idf (A68_EXT, "laplace", m, genie_laplace);
#endif
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_idf (A68_EXT, "longcomplexsqrt", m, genie_sqrt_long_complex);
  a68_idf (A68_EXT, "dcsqrt", m, genie_sqrt_long_complex);
  a68_idf (A68_EXT, "longcomplexexp", m, genie_exp_long_complex);
  a68_idf (A68_EXT, "dcexp", m, genie_exp_long_complex);
  a68_idf (A68_EXT, "longcomplexln", m, genie_ln_long_complex);
  a68_idf (A68_EXT, "dcln", m, genie_ln_long_complex);
  a68_idf (A68_EXT, "longcomplexsin", m, genie_sin_long_complex);
  a68_idf (A68_EXT, "dcsin", m, genie_sin_long_complex);
  a68_idf (A68_EXT, "longcomplexcos", m, genie_cos_long_complex);
  a68_idf (A68_EXT, "dccos", m, genie_cos_long_complex);
  a68_idf (A68_EXT, "longcomplextan", m, genie_tan_long_complex);
  a68_idf (A68_EXT, "dctan", m, genie_tan_long_complex);
  a68_idf (A68_EXT, "longcomplexarcsin", m, genie_asin_long_complex);
  a68_idf (A68_EXT, "dcasin", m, genie_asin_long_complex);
  a68_idf (A68_EXT, "longcomplexarccos", m, genie_acos_long_complex);
  a68_idf (A68_EXT, "dcacos", m, genie_acos_long_complex);
  a68_idf (A68_EXT, "longcomplexarctan", m, genie_atan_long_complex);
  a68_idf (A68_EXT, "dcatan", m, genie_atan_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A68_EXT, "longlongcomplexsqrt", m, genie_sqrt_long_complex);
  a68_idf (A68_EXT, "qcsqrt", m, genie_sqrt_long_complex);
  a68_idf (A68_EXT, "longlongcomplexexp", m, genie_exp_long_complex);
  a68_idf (A68_EXT, "qcexp", m, genie_exp_long_complex);
  a68_idf (A68_EXT, "longlongcomplexln", m, genie_ln_long_complex);
  a68_idf (A68_EXT, "qcln", m, genie_ln_long_complex);
  a68_idf (A68_EXT, "longlongcomplexsin", m, genie_sin_long_complex);
  a68_idf (A68_EXT, "qcsin", m, genie_sin_long_complex);
  a68_idf (A68_EXT, "longlongcomplexcos", m, genie_cos_long_complex);
  a68_idf (A68_EXT, "qccos", m, genie_cos_long_complex);
  a68_idf (A68_EXT, "longlongcomplextan", m, genie_tan_long_complex);
  a68_idf (A68_EXT, "qctan", m, genie_tan_long_complex);
  a68_idf (A68_EXT, "longlongcomplexarcsin", m, genie_asin_long_complex);
  a68_idf (A68_EXT, "qcasin", m, genie_asin_long_complex);
  a68_idf (A68_EXT, "longlongcomplexarccos", m, genie_acos_long_complex);
  a68_idf (A68_EXT, "qcacos", m, genie_acos_long_complex);
  a68_idf (A68_EXT, "longlongcomplexarctan", m, genie_atan_long_complex);
  a68_idf (A68_EXT, "qcatan", m, genie_atan_long_complex);
/* SOUND/RIFF procs. */
  m = a68_proc (MODE (SOUND), MODE (INT), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_EXT, "newsound", m, genie_new_sound);
  m = a68_proc (MODE (INT), MODE (SOUND), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_EXT, "getsound", m, genie_get_sound);
  m = a68_proc (MODE (VOID), MODE (SOUND), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_EXT, "setsound", m, genie_set_sound);
  m = a68_proc (MODE (INT), MODE (SOUND), NULL);
  a68_op (A68_EXT, "RESOLUTION", m, genie_sound_resolution);
  a68_op (A68_EXT, "CHANNELS", m, genie_sound_channels);
  a68_op (A68_EXT, "RATE", m, genie_sound_rate);
  a68_op (A68_EXT, "SAMPLES", m, genie_sound_samples);
}

/*!
\brief set up standenv - transput
**/

static void stand_transput (void)
{
  MOID_T *m;
  a68_idf (A68_STD, "errorchar", MODE (CHAR), genie_error_char);
  a68_idf (A68_STD, "expchar", MODE (CHAR), genie_exp_char);
  a68_idf (A68_STD, "flip", MODE (CHAR), genie_flip_char);
  a68_idf (A68_STD, "flop", MODE (CHAR), genie_flop_char);
  a68_idf (A68_EXT, "blankcharacter", MODE (CHAR), genie_blank_char);
  a68_idf (A68_STD, "blankchar", MODE (CHAR), genie_blank_char);
  a68_idf (A68_STD, "blank", MODE (CHAR), genie_blank_char);
  a68_idf (A68_EXT, "nullcharacter", MODE (CHAR), genie_null_char);
  a68_idf (A68_STD, "nullchar", MODE (CHAR), genie_null_char);
  a68_idf (A68_EXT, "newlinecharacter", MODE (CHAR), genie_newline_char);
  a68_idf (A68_EXT, "newlinechar", MODE (CHAR), genie_newline_char);
  a68_idf (A68_EXT, "formfeedcharacter", MODE (CHAR), genie_formfeed_char);
  a68_idf (A68_EXT, "formfeedchar", MODE (CHAR), genie_formfeed_char);
  a68_idf (A68_EXT, "tabcharacter", MODE (CHAR), genie_tab_char);
  a68_idf (A68_EXT, "tabchar", MODE (CHAR), genie_tab_char);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), NULL);
  a68_idf (A68_STD, "whole", m, genie_whole);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_STD, "fixed", m, genie_fixed);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_STD, "float", m, genie_float);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_STD, "real", m, genie_real);
  a68_idf (A68_STD, "standin", MODE (REF_FILE), genie_stand_in);
  a68_idf (A68_STD, "standout", MODE (REF_FILE), genie_stand_out);
  a68_idf (A68_STD, "standback", MODE (REF_FILE), genie_stand_back);
  a68_idf (A68_EXT, "standerror", MODE (REF_FILE), genie_stand_error);
  a68_idf (A68_STD, "standinchannel", MODE (CHANNEL), genie_stand_in_channel);
  a68_idf (A68_STD, "standoutchannel", MODE (CHANNEL), genie_stand_out_channel);
  a68_idf (A68_EXT, "standdrawchannel", MODE (CHANNEL), genie_stand_draw_channel);
  a68_idf (A68_STD, "standbackchannel", MODE (CHANNEL), genie_stand_back_channel);
  a68_idf (A68_EXT, "standerrorchannel", MODE (CHANNEL), genie_stand_error_channel);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A68_STD, "maketerm", m, genie_make_term);
  m = a68_proc (MODE (BOOL), MODE (CHAR), MODE (REF_INT), MODE (STRING), NULL);
  a68_idf (A68_STD, "charinstring", m, genie_char_in_string);
  a68_idf (A68_EXT, "lastcharinstring", m, genie_last_char_in_string);
  m = a68_proc (MODE (BOOL), MODE (STRING), MODE (REF_INT), MODE (STRING), NULL);
  a68_idf (A68_EXT, "stringinstring", m, genie_string_in_string);
  m = a68_proc (MODE (STRING), MODE (REF_FILE), NULL);
  a68_idf (A68_EXT, "idf", m, genie_idf);
  a68_idf (A68_EXT, "term", m, genie_term);
  m = a68_proc (MODE (STRING), NULL);
  a68_idf (A68_EXT, "programidf", m, genie_program_idf);
/* Event routines. */
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (PROC_REF_FILE_BOOL), NULL);
  a68_idf (A68_STD, "onfileend", m, genie_on_file_end);
  a68_idf (A68_STD, "onpageend", m, genie_on_page_end);
  a68_idf (A68_STD, "onlineend", m, genie_on_line_end);
  a68_idf (A68_STD, "onlogicalfileend", m, genie_on_file_end);
  a68_idf (A68_STD, "onphysicalfileend", m, genie_on_file_end);
  a68_idf (A68_STD, "onformatend", m, genie_on_format_end);
  a68_idf (A68_STD, "onformaterror", m, genie_on_format_error);
  a68_idf (A68_STD, "onvalueerror", m, genie_on_value_error);
  a68_idf (A68_STD, "onopenerror", m, genie_on_open_error);
  a68_idf (A68_EXT, "ontransputerror", m, genie_on_transput_error);
/* Enquiries on files. */
  a68_idf (A68_STD, "putpossible", MODE (PROC_REF_FILE_BOOL), genie_put_possible);
  a68_idf (A68_STD, "getpossible", MODE (PROC_REF_FILE_BOOL), genie_get_possible);
  a68_idf (A68_STD, "binpossible", MODE (PROC_REF_FILE_BOOL), genie_bin_possible);
  a68_idf (A68_STD, "setpossible", MODE (PROC_REF_FILE_BOOL), genie_set_possible);
  a68_idf (A68_STD, "resetpossible", MODE (PROC_REF_FILE_BOOL), genie_reset_possible);
  a68_idf (A68_EXT, "rewindpossible", MODE (PROC_REF_FILE_BOOL), genie_reset_possible);
  a68_idf (A68_STD, "reidfpossible", MODE (PROC_REF_FILE_BOOL), genie_reidf_possible);
  a68_idf (A68_EXT, "drawpossible", MODE (PROC_REF_FILE_BOOL), genie_draw_possible);
  a68_idf (A68_STD, "compressible", MODE (PROC_REF_FILE_BOOL), genie_compressible);
  a68_idf (A68_EXT, "eof", MODE (PROC_REF_FILE_BOOL), genie_eof);
  a68_idf (A68_EXT, "eoln", MODE (PROC_REF_FILE_BOOL), genie_eoln);
/* Handling of files. */
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), MODE (CHANNEL), NULL);
  a68_idf (A68_STD, "open", m, genie_open);
  a68_idf (A68_STD, "establish", m, genie_establish);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REF_STRING), NULL);
  a68_idf (A68_STD, "associate", m, genie_associate);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (CHANNEL), NULL);
  a68_idf (A68_STD, "create", m, genie_create);
  a68_idf (A68_STD, "close", MODE (PROC_REF_FILE_VOID), genie_close);
  a68_idf (A68_STD, "lock", MODE (PROC_REF_FILE_VOID), genie_lock);
  a68_idf (A68_STD, "scratch", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A68_STD, "erase", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A68_STD, "reset", MODE (PROC_REF_FILE_VOID), genie_reset);
  a68_idf (A68_EXT, "rewind", MODE (PROC_REF_FILE_VOID), genie_reset);
  a68_idf (A68_STD, "scratch", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A68_STD, "newline", MODE (PROC_REF_FILE_VOID), genie_new_line);
  a68_idf (A68_STD, "newpage", MODE (PROC_REF_FILE_VOID), genie_new_page);
  a68_idf (A68_STD, "space", MODE (PROC_REF_FILE_VOID), genie_space);
  a68_idf (A68_STD, "backspace", MODE (PROC_REF_FILE_VOID), genie_backspace);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A68_STD, "set", m, genie_set);
  a68_idf (A68_STD, "seek", m, genie_set);
  m = a68_proc (MODE (VOID), MODE (ROW_SIMPLIN), NULL);
  a68_idf (A68_STD, "read", m, genie_read);
  a68_idf (A68_STD, "readbin", m, genie_read_bin);
  a68_idf (A68_STD, "readf", m, genie_read_format);
  m = a68_proc (MODE (VOID), MODE (ROW_SIMPLOUT), NULL);
  a68_idf (A68_STD, "print", m, genie_write);
  a68_idf (A68_STD, "write", m, genie_write);
  a68_idf (A68_STD, "printbin", m, genie_write_bin);
  a68_idf (A68_STD, "writebin", m, genie_write_bin);
  a68_idf (A68_STD, "printf", m, genie_write_format);
  a68_idf (A68_STD, "writef", m, genie_write_format);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_SIMPLIN), NULL);
  a68_idf (A68_STD, "get", m, genie_read_file);
  a68_idf (A68_STD, "getf", m, genie_read_file_format);
  a68_idf (A68_STD, "getbin", m, genie_read_bin_file);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_SIMPLOUT), NULL);
  a68_idf (A68_STD, "put", m, genie_write_file);
  a68_idf (A68_STD, "putf", m, genie_write_file_format);
  a68_idf (A68_STD, "putbin", m, genie_write_bin_file);
/* ALGOL68C type procs. */
  m = proc_int;
  a68_idf (A68_EXT, "readint", m, genie_read_int);
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A68_EXT, "printint", m, genie_print_int);
  m = a68_proc (MODE (LONG_INT), NULL);
  a68_idf (A68_EXT, "readlongint", m, genie_read_long_int);
  m = a68_proc (MODE (VOID), MODE (LONG_INT), NULL);
  a68_idf (A68_EXT, "printlongint", m, genie_print_long_int);
  m = a68_proc (MODE (LONGLONG_INT), NULL);
  a68_idf (A68_EXT, "readlonglongint", m, genie_read_longlong_int);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_INT), NULL);
  a68_idf (A68_EXT, "printlonglongint", m, genie_print_longlong_int);
  m = proc_real;
  a68_idf (A68_EXT, "readreal", m, genie_read_real);
  m = a68_proc (MODE (VOID), MODE (REAL), NULL);
  a68_idf (A68_EXT, "printreal", m, genie_print_real);
  m = a68_proc (MODE (LONG_REAL), NULL);
  a68_idf (A68_EXT, "readlongreal", m, genie_read_long_real);
  a68_idf (A68_EXT, "readdouble", m, genie_read_long_real);
  m = a68_proc (MODE (VOID), MODE (LONG_REAL), NULL);
  a68_idf (A68_EXT, "printlongreal", m, genie_print_long_real);
  a68_idf (A68_EXT, "printdouble", m, genie_print_long_real);
  m = a68_proc (MODE (LONGLONG_REAL), NULL);
  a68_idf (A68_EXT, "readlonglongreal", m, genie_read_longlong_real);
  a68_idf (A68_EXT, "readquad", m, genie_read_longlong_real);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_REAL), NULL);
  a68_idf (A68_EXT, "printlonglongreal", m, genie_print_longlong_real);
  a68_idf (A68_EXT, "printquad", m, genie_print_longlong_real);
  m = a68_proc (MODE (COMPLEX), NULL);
  a68_idf (A68_EXT, "readcompl", m, genie_read_complex);
  a68_idf (A68_EXT, "readcomplex", m, genie_read_complex);
  m = a68_proc (MODE (VOID), MODE (COMPLEX), NULL);
  a68_idf (A68_EXT, "printcompl", m, genie_print_complex);
  a68_idf (A68_EXT, "printcomplex", m, genie_print_complex);
  m = a68_proc (MODE (LONG_COMPLEX), NULL);
  a68_idf (A68_EXT, "readlongcompl", m, genie_read_long_complex);
  a68_idf (A68_EXT, "readlongcomplex", m, genie_read_long_complex);
  m = a68_proc (MODE (VOID), MODE (LONG_COMPLEX), NULL);
  a68_idf (A68_EXT, "printlongcompl", m, genie_print_long_complex);
  a68_idf (A68_EXT, "printlongcomplex", m, genie_print_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A68_EXT, "readlonglongcompl", m, genie_read_longlong_complex);
  a68_idf (A68_EXT, "readlonglongcomplex", m, genie_read_longlong_complex);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A68_EXT, "printlonglongcompl", m, genie_print_longlong_complex);
  a68_idf (A68_EXT, "printlonglongcomplex", m, genie_print_longlong_complex);
  m = proc_bool;
  a68_idf (A68_EXT, "readbool", m, genie_read_bool);
  m = a68_proc (MODE (VOID), MODE (BOOL), NULL);
  a68_idf (A68_EXT, "printbool", m, genie_print_bool);
  m = a68_proc (MODE (BITS), NULL);
  a68_idf (A68_EXT, "readbits", m, genie_read_bits);
  m = a68_proc (MODE (LONG_BITS), NULL);
  a68_idf (A68_EXT, "readlongbits", m, genie_read_long_bits);
  m = a68_proc (MODE (LONGLONG_BITS), NULL);
  a68_idf (A68_EXT, "readlonglongbits", m, genie_read_longlong_bits);
  m = a68_proc (MODE (VOID), MODE (BITS), NULL);
  a68_idf (A68_EXT, "printbits", m, genie_print_bits);
  m = a68_proc (MODE (VOID), MODE (LONG_BITS), NULL);
  a68_idf (A68_EXT, "printlongbits", m, genie_print_long_bits);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_BITS), NULL);
  a68_idf (A68_EXT, "printlonglongbits", m, genie_print_longlong_bits);
  m = proc_char;
  a68_idf (A68_EXT, "readchar", m, genie_read_char);
  m = a68_proc (MODE (VOID), MODE (CHAR), NULL);
  a68_idf (A68_EXT, "printchar", m, genie_print_char);
  a68_idf (A68_EXT, "readstring", MODE (PROC_STRING), genie_read_string);
  m = a68_proc (MODE (VOID), MODE (STRING), NULL);
  a68_idf (A68_EXT, "printstring", m, genie_print_string);
}

/*!
\brief set up standenv - extensions
**/

static void stand_extensions (void)
{
  MOID_T *m = NULL;
  (void) m;                     /* To fool cc in case we have none of the libraries */
#if defined ENABLE_GRAPHICS
/* Drawing. */
  m = a68_proc (MODE (BOOL), MODE (REF_FILE), MODE (STRING), MODE (STRING), NULL);
  a68_idf (A68_EXT, "drawdevice", m, genie_make_device);
  a68_idf (A68_EXT, "makedevice", m, genie_make_device);
  m = a68_proc (MODE (REAL), MODE (REF_FILE), NULL);
  a68_idf (A68_EXT, "drawaspect", m, genie_draw_aspect);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), NULL);
  a68_idf (A68_EXT, "drawclear", m, genie_draw_clear);
  a68_idf (A68_EXT, "drawerase", m, genie_draw_clear);
  a68_idf (A68_EXT, "drawflush", m, genie_draw_show);
  a68_idf (A68_EXT, "drawshow", m, genie_draw_show);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A68_EXT, "drawfillstyle", m, genie_draw_filltype);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A68_EXT, "drawgetcolourname", m, genie_draw_get_colour_name);
  a68_idf (A68_EXT, "drawgetcolorname", m, genie_draw_get_colour_name);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A68_EXT, "drawcolor", m, genie_draw_colour);
  a68_idf (A68_EXT, "drawcolour", m, genie_draw_colour);
  a68_idf (A68_EXT, "drawbackgroundcolor", m, genie_draw_background_colour);
  a68_idf (A68_EXT, "drawbackgroundcolour", m, genie_draw_background_colour);
  a68_idf (A68_EXT, "drawcircle", m, genie_draw_circle);
  a68_idf (A68_EXT, "drawball", m, genie_draw_atom);
  a68_idf (A68_EXT, "drawstar", m, genie_draw_star);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A68_EXT, "drawpoint", m, genie_draw_point);
  a68_idf (A68_EXT, "drawline", m, genie_draw_line);
  a68_idf (A68_EXT, "drawmove", m, genie_draw_move);
  a68_idf (A68_EXT, "drawrect", m, genie_draw_rect);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (CHAR), MODE (CHAR), MODE (ROW_CHAR), NULL);
  a68_idf (A68_EXT, "drawtext", m, genie_draw_text);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_CHAR), NULL);
  a68_idf (A68_EXT, "drawlinestyle", m, genie_draw_linestyle);
  a68_idf (A68_EXT, "drawfontname", m, genie_draw_fontname);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), NULL);
  a68_idf (A68_EXT, "drawlinewidth", m, genie_draw_linewidth);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A68_EXT, "drawfontsize", m, genie_draw_fontsize);
  a68_idf (A68_EXT, "drawtextangle", m, genie_draw_textangle);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A68_EXT, "drawcolorname", m, genie_draw_colour_name);
  a68_idf (A68_EXT, "drawcolourname", m, genie_draw_colour_name);
  a68_idf (A68_EXT, "drawbackgroundcolorname", m, genie_draw_background_colour_name);
  a68_idf (A68_EXT, "drawbackgroundcolourname", m, genie_draw_background_colour_name);
#endif
#if defined ENABLE_NUMERICAL
  a68_idf (A68_EXT, "cgsspeedoflight", MODE (REAL), genie_cgs_speed_of_light);
  a68_idf (A68_EXT, "cgsgravitationalconstant", MODE (REAL), genie_cgs_gravitational_constant);
  a68_idf (A68_EXT, "cgsplanckconstant", MODE (REAL), genie_cgs_planck_constant_h);
  a68_idf (A68_EXT, "cgsplanckconstantbar", MODE (REAL), genie_cgs_planck_constant_hbar);
  a68_idf (A68_EXT, "cgsastronomicalunit", MODE (REAL), genie_cgs_astronomical_unit);
  a68_idf (A68_EXT, "cgslightyear", MODE (REAL), genie_cgs_light_year);
  a68_idf (A68_EXT, "cgsparsec", MODE (REAL), genie_cgs_parsec);
  a68_idf (A68_EXT, "cgsgravaccel", MODE (REAL), genie_cgs_grav_accel);
  a68_idf (A68_EXT, "cgselectronvolt", MODE (REAL), genie_cgs_electron_volt);
  a68_idf (A68_EXT, "cgsmasselectron", MODE (REAL), genie_cgs_mass_electron);
  a68_idf (A68_EXT, "cgsmassmuon", MODE (REAL), genie_cgs_mass_muon);
  a68_idf (A68_EXT, "cgsmassproton", MODE (REAL), genie_cgs_mass_proton);
  a68_idf (A68_EXT, "cgsmassneutron", MODE (REAL), genie_cgs_mass_neutron);
  a68_idf (A68_EXT, "cgsrydberg", MODE (REAL), genie_cgs_rydberg);
  a68_idf (A68_EXT, "cgsboltzmann", MODE (REAL), genie_cgs_boltzmann);
  a68_idf (A68_EXT, "cgsbohrmagneton", MODE (REAL), genie_cgs_bohr_magneton);
  a68_idf (A68_EXT, "cgsnuclearmagneton", MODE (REAL), genie_cgs_nuclear_magneton);
  a68_idf (A68_EXT, "cgselectronmagneticmoment", MODE (REAL), genie_cgs_electron_magnetic_moment);
  a68_idf (A68_EXT, "cgsprotonmagneticmoment", MODE (REAL), genie_cgs_proton_magnetic_moment);
  a68_idf (A68_EXT, "cgsmolargas", MODE (REAL), genie_cgs_molar_gas);
  a68_idf (A68_EXT, "cgsstandardgasvolume", MODE (REAL), genie_cgs_standard_gas_volume);
  a68_idf (A68_EXT, "cgsminute", MODE (REAL), genie_cgs_minute);
  a68_idf (A68_EXT, "cgshour", MODE (REAL), genie_cgs_hour);
  a68_idf (A68_EXT, "cgsday", MODE (REAL), genie_cgs_day);
  a68_idf (A68_EXT, "cgsweek", MODE (REAL), genie_cgs_week);
  a68_idf (A68_EXT, "cgsinch", MODE (REAL), genie_cgs_inch);
  a68_idf (A68_EXT, "cgsfoot", MODE (REAL), genie_cgs_foot);
  a68_idf (A68_EXT, "cgsyard", MODE (REAL), genie_cgs_yard);
  a68_idf (A68_EXT, "cgsmile", MODE (REAL), genie_cgs_mile);
  a68_idf (A68_EXT, "cgsnauticalmile", MODE (REAL), genie_cgs_nautical_mile);
  a68_idf (A68_EXT, "cgsfathom", MODE (REAL), genie_cgs_fathom);
  a68_idf (A68_EXT, "cgsmil", MODE (REAL), genie_cgs_mil);
  a68_idf (A68_EXT, "cgspoint", MODE (REAL), genie_cgs_point);
  a68_idf (A68_EXT, "cgstexpoint", MODE (REAL), genie_cgs_texpoint);
  a68_idf (A68_EXT, "cgsmicron", MODE (REAL), genie_cgs_micron);
  a68_idf (A68_EXT, "cgsangstrom", MODE (REAL), genie_cgs_angstrom);
  a68_idf (A68_EXT, "cgshectare", MODE (REAL), genie_cgs_hectare);
  a68_idf (A68_EXT, "cgsacre", MODE (REAL), genie_cgs_acre);
  a68_idf (A68_EXT, "cgsbarn", MODE (REAL), genie_cgs_barn);
  a68_idf (A68_EXT, "cgsliter", MODE (REAL), genie_cgs_liter);
  a68_idf (A68_EXT, "cgsusgallon", MODE (REAL), genie_cgs_us_gallon);
  a68_idf (A68_EXT, "cgsquart", MODE (REAL), genie_cgs_quart);
  a68_idf (A68_EXT, "cgspint", MODE (REAL), genie_cgs_pint);
  a68_idf (A68_EXT, "cgscup", MODE (REAL), genie_cgs_cup);
  a68_idf (A68_EXT, "cgsfluidounce", MODE (REAL), genie_cgs_fluid_ounce);
  a68_idf (A68_EXT, "cgstablespoon", MODE (REAL), genie_cgs_tablespoon);
  a68_idf (A68_EXT, "cgsteaspoon", MODE (REAL), genie_cgs_teaspoon);
  a68_idf (A68_EXT, "cgscanadiangallon", MODE (REAL), genie_cgs_canadian_gallon);
  a68_idf (A68_EXT, "cgsukgallon", MODE (REAL), genie_cgs_uk_gallon);
  a68_idf (A68_EXT, "cgsmilesperhour", MODE (REAL), genie_cgs_miles_per_hour);
  a68_idf (A68_EXT, "cgskilometersperhour", MODE (REAL), genie_cgs_kilometers_per_hour);
  a68_idf (A68_EXT, "cgsknot", MODE (REAL), genie_cgs_knot);
  a68_idf (A68_EXT, "cgspoundmass", MODE (REAL), genie_cgs_pound_mass);
  a68_idf (A68_EXT, "cgsouncemass", MODE (REAL), genie_cgs_ounce_mass);
  a68_idf (A68_EXT, "cgston", MODE (REAL), genie_cgs_ton);
  a68_idf (A68_EXT, "cgsmetricton", MODE (REAL), genie_cgs_metric_ton);
  a68_idf (A68_EXT, "cgsukton", MODE (REAL), genie_cgs_uk_ton);
  a68_idf (A68_EXT, "cgstroyounce", MODE (REAL), genie_cgs_troy_ounce);
  a68_idf (A68_EXT, "cgscarat", MODE (REAL), genie_cgs_carat);
  a68_idf (A68_EXT, "cgsunifiedatomicmass", MODE (REAL), genie_cgs_unified_atomic_mass);
  a68_idf (A68_EXT, "cgsgramforce", MODE (REAL), genie_cgs_gram_force);
  a68_idf (A68_EXT, "cgspoundforce", MODE (REAL), genie_cgs_pound_force);
  a68_idf (A68_EXT, "cgskilopoundforce", MODE (REAL), genie_cgs_kilopound_force);
  a68_idf (A68_EXT, "cgspoundal", MODE (REAL), genie_cgs_poundal);
  a68_idf (A68_EXT, "cgscalorie", MODE (REAL), genie_cgs_calorie);
  a68_idf (A68_EXT, "cgsbtu", MODE (REAL), genie_cgs_btu);
  a68_idf (A68_EXT, "cgstherm", MODE (REAL), genie_cgs_therm);
  a68_idf (A68_EXT, "cgshorsepower", MODE (REAL), genie_cgs_horsepower);
  a68_idf (A68_EXT, "cgsbar", MODE (REAL), genie_cgs_bar);
  a68_idf (A68_EXT, "cgsstdatmosphere", MODE (REAL), genie_cgs_std_atmosphere);
  a68_idf (A68_EXT, "cgstorr", MODE (REAL), genie_cgs_torr);
  a68_idf (A68_EXT, "cgsmeterofmercury", MODE (REAL), genie_cgs_meter_of_mercury);
  a68_idf (A68_EXT, "cgsinchofmercury", MODE (REAL), genie_cgs_inch_of_mercury);
  a68_idf (A68_EXT, "cgsinchofwater", MODE (REAL), genie_cgs_inch_of_water);
  a68_idf (A68_EXT, "cgspsi", MODE (REAL), genie_cgs_psi);
  a68_idf (A68_EXT, "cgspoise", MODE (REAL), genie_cgs_poise);
  a68_idf (A68_EXT, "cgsstokes", MODE (REAL), genie_cgs_stokes);
  a68_idf (A68_EXT, "cgsfaraday", MODE (REAL), genie_cgs_faraday);
  a68_idf (A68_EXT, "cgselectroncharge", MODE (REAL), genie_cgs_electron_charge);
  a68_idf (A68_EXT, "cgsgauss", MODE (REAL), genie_cgs_gauss);
  a68_idf (A68_EXT, "cgsstilb", MODE (REAL), genie_cgs_stilb);
  a68_idf (A68_EXT, "cgslumen", MODE (REAL), genie_cgs_lumen);
  a68_idf (A68_EXT, "cgslux", MODE (REAL), genie_cgs_lux);
  a68_idf (A68_EXT, "cgsphot", MODE (REAL), genie_cgs_phot);
  a68_idf (A68_EXT, "cgsfootcandle", MODE (REAL), genie_cgs_footcandle);
  a68_idf (A68_EXT, "cgslambert", MODE (REAL), genie_cgs_lambert);
  a68_idf (A68_EXT, "cgsfootlambert", MODE (REAL), genie_cgs_footlambert);
  a68_idf (A68_EXT, "cgscurie", MODE (REAL), genie_cgs_curie);
  a68_idf (A68_EXT, "cgsroentgen", MODE (REAL), genie_cgs_roentgen);
  a68_idf (A68_EXT, "cgsrad", MODE (REAL), genie_cgs_rad);
  a68_idf (A68_EXT, "cgssolarmass", MODE (REAL), genie_cgs_solar_mass);
  a68_idf (A68_EXT, "cgsbohrradius", MODE (REAL), genie_cgs_bohr_radius);
  a68_idf (A68_EXT, "cgsnewton", MODE (REAL), genie_cgs_newton);
  a68_idf (A68_EXT, "cgsdyne", MODE (REAL), genie_cgs_dyne);
  a68_idf (A68_EXT, "cgsjoule", MODE (REAL), genie_cgs_joule);
  a68_idf (A68_EXT, "cgserg", MODE (REAL), genie_cgs_erg);
  a68_idf (A68_EXT, "mksaspeedoflight", MODE (REAL), genie_mks_speed_of_light);
  a68_idf (A68_EXT, "mksagravitationalconstant", MODE (REAL), genie_mks_gravitational_constant);
  a68_idf (A68_EXT, "mksaplanckconstant", MODE (REAL), genie_mks_planck_constant_h);
  a68_idf (A68_EXT, "mksaplanckconstantbar", MODE (REAL), genie_mks_planck_constant_hbar);
  a68_idf (A68_EXT, "mksavacuumpermeability", MODE (REAL), genie_mks_vacuum_permeability);
  a68_idf (A68_EXT, "mksaastronomicalunit", MODE (REAL), genie_mks_astronomical_unit);
  a68_idf (A68_EXT, "mksalightyear", MODE (REAL), genie_mks_light_year);
  a68_idf (A68_EXT, "mksaparsec", MODE (REAL), genie_mks_parsec);
  a68_idf (A68_EXT, "mksagravaccel", MODE (REAL), genie_mks_grav_accel);
  a68_idf (A68_EXT, "mksaelectronvolt", MODE (REAL), genie_mks_electron_volt);
  a68_idf (A68_EXT, "mksamasselectron", MODE (REAL), genie_mks_mass_electron);
  a68_idf (A68_EXT, "mksamassmuon", MODE (REAL), genie_mks_mass_muon);
  a68_idf (A68_EXT, "mksamassproton", MODE (REAL), genie_mks_mass_proton);
  a68_idf (A68_EXT, "mksamassneutron", MODE (REAL), genie_mks_mass_neutron);
  a68_idf (A68_EXT, "mksarydberg", MODE (REAL), genie_mks_rydberg);
  a68_idf (A68_EXT, "mksaboltzmann", MODE (REAL), genie_mks_boltzmann);
  a68_idf (A68_EXT, "mksabohrmagneton", MODE (REAL), genie_mks_bohr_magneton);
  a68_idf (A68_EXT, "mksanuclearmagneton", MODE (REAL), genie_mks_nuclear_magneton);
  a68_idf (A68_EXT, "mksaelectronmagneticmoment", MODE (REAL), genie_mks_electron_magnetic_moment);
  a68_idf (A68_EXT, "mksaprotonmagneticmoment", MODE (REAL), genie_mks_proton_magnetic_moment);
  a68_idf (A68_EXT, "mksamolargas", MODE (REAL), genie_mks_molar_gas);
  a68_idf (A68_EXT, "mksastandardgasvolume", MODE (REAL), genie_mks_standard_gas_volume);
  a68_idf (A68_EXT, "mksaminute", MODE (REAL), genie_mks_minute);
  a68_idf (A68_EXT, "mksahour", MODE (REAL), genie_mks_hour);
  a68_idf (A68_EXT, "mksaday", MODE (REAL), genie_mks_day);
  a68_idf (A68_EXT, "mksaweek", MODE (REAL), genie_mks_week);
  a68_idf (A68_EXT, "mksainch", MODE (REAL), genie_mks_inch);
  a68_idf (A68_EXT, "mksafoot", MODE (REAL), genie_mks_foot);
  a68_idf (A68_EXT, "mksayard", MODE (REAL), genie_mks_yard);
  a68_idf (A68_EXT, "mksamile", MODE (REAL), genie_mks_mile);
  a68_idf (A68_EXT, "mksanauticalmile", MODE (REAL), genie_mks_nautical_mile);
  a68_idf (A68_EXT, "mksafathom", MODE (REAL), genie_mks_fathom);
  a68_idf (A68_EXT, "mksamil", MODE (REAL), genie_mks_mil);
  a68_idf (A68_EXT, "mksapoint", MODE (REAL), genie_mks_point);
  a68_idf (A68_EXT, "mksatexpoint", MODE (REAL), genie_mks_texpoint);
  a68_idf (A68_EXT, "mksamicron", MODE (REAL), genie_mks_micron);
  a68_idf (A68_EXT, "mksaangstrom", MODE (REAL), genie_mks_angstrom);
  a68_idf (A68_EXT, "mksahectare", MODE (REAL), genie_mks_hectare);
  a68_idf (A68_EXT, "mksaacre", MODE (REAL), genie_mks_acre);
  a68_idf (A68_EXT, "mksabarn", MODE (REAL), genie_mks_barn);
  a68_idf (A68_EXT, "mksaliter", MODE (REAL), genie_mks_liter);
  a68_idf (A68_EXT, "mksausgallon", MODE (REAL), genie_mks_us_gallon);
  a68_idf (A68_EXT, "mksaquart", MODE (REAL), genie_mks_quart);
  a68_idf (A68_EXT, "mksapint", MODE (REAL), genie_mks_pint);
  a68_idf (A68_EXT, "mksacup", MODE (REAL), genie_mks_cup);
  a68_idf (A68_EXT, "mksafluidounce", MODE (REAL), genie_mks_fluid_ounce);
  a68_idf (A68_EXT, "mksatablespoon", MODE (REAL), genie_mks_tablespoon);
  a68_idf (A68_EXT, "mksateaspoon", MODE (REAL), genie_mks_teaspoon);
  a68_idf (A68_EXT, "mksacanadiangallon", MODE (REAL), genie_mks_canadian_gallon);
  a68_idf (A68_EXT, "mksaukgallon", MODE (REAL), genie_mks_uk_gallon);
  a68_idf (A68_EXT, "mksamilesperhour", MODE (REAL), genie_mks_miles_per_hour);
  a68_idf (A68_EXT, "mksakilometersperhour", MODE (REAL), genie_mks_kilometers_per_hour);
  a68_idf (A68_EXT, "mksaknot", MODE (REAL), genie_mks_knot);
  a68_idf (A68_EXT, "mksapoundmass", MODE (REAL), genie_mks_pound_mass);
  a68_idf (A68_EXT, "mksaouncemass", MODE (REAL), genie_mks_ounce_mass);
  a68_idf (A68_EXT, "mksaton", MODE (REAL), genie_mks_ton);
  a68_idf (A68_EXT, "mksametricton", MODE (REAL), genie_mks_metric_ton);
  a68_idf (A68_EXT, "mksaukton", MODE (REAL), genie_mks_uk_ton);
  a68_idf (A68_EXT, "mksatroyounce", MODE (REAL), genie_mks_troy_ounce);
  a68_idf (A68_EXT, "mksacarat", MODE (REAL), genie_mks_carat);
  a68_idf (A68_EXT, "mksaunifiedatomicmass", MODE (REAL), genie_mks_unified_atomic_mass);
  a68_idf (A68_EXT, "mksagramforce", MODE (REAL), genie_mks_gram_force);
  a68_idf (A68_EXT, "mksapoundforce", MODE (REAL), genie_mks_pound_force);
  a68_idf (A68_EXT, "mksakilopoundforce", MODE (REAL), genie_mks_kilopound_force);
  a68_idf (A68_EXT, "mksapoundal", MODE (REAL), genie_mks_poundal);
  a68_idf (A68_EXT, "mksacalorie", MODE (REAL), genie_mks_calorie);
  a68_idf (A68_EXT, "mksabtu", MODE (REAL), genie_mks_btu);
  a68_idf (A68_EXT, "mksatherm", MODE (REAL), genie_mks_therm);
  a68_idf (A68_EXT, "mksahorsepower", MODE (REAL), genie_mks_horsepower);
  a68_idf (A68_EXT, "mksabar", MODE (REAL), genie_mks_bar);
  a68_idf (A68_EXT, "mksastdatmosphere", MODE (REAL), genie_mks_std_atmosphere);
  a68_idf (A68_EXT, "mksatorr", MODE (REAL), genie_mks_torr);
  a68_idf (A68_EXT, "mksameterofmercury", MODE (REAL), genie_mks_meter_of_mercury);
  a68_idf (A68_EXT, "mksainchofmercury", MODE (REAL), genie_mks_inch_of_mercury);
  a68_idf (A68_EXT, "mksainchofwater", MODE (REAL), genie_mks_inch_of_water);
  a68_idf (A68_EXT, "mksapsi", MODE (REAL), genie_mks_psi);
  a68_idf (A68_EXT, "mksapoise", MODE (REAL), genie_mks_poise);
  a68_idf (A68_EXT, "mksastokes", MODE (REAL), genie_mks_stokes);
  a68_idf (A68_EXT, "mksafaraday", MODE (REAL), genie_mks_faraday);
  a68_idf (A68_EXT, "mksaelectroncharge", MODE (REAL), genie_mks_electron_charge);
  a68_idf (A68_EXT, "mksagauss", MODE (REAL), genie_mks_gauss);
  a68_idf (A68_EXT, "mksastilb", MODE (REAL), genie_mks_stilb);
  a68_idf (A68_EXT, "mksalumen", MODE (REAL), genie_mks_lumen);
  a68_idf (A68_EXT, "mksalux", MODE (REAL), genie_mks_lux);
  a68_idf (A68_EXT, "mksaphot", MODE (REAL), genie_mks_phot);
  a68_idf (A68_EXT, "mksafootcandle", MODE (REAL), genie_mks_footcandle);
  a68_idf (A68_EXT, "mksalambert", MODE (REAL), genie_mks_lambert);
  a68_idf (A68_EXT, "mksafootlambert", MODE (REAL), genie_mks_footlambert);
  a68_idf (A68_EXT, "mksacurie", MODE (REAL), genie_mks_curie);
  a68_idf (A68_EXT, "mksaroentgen", MODE (REAL), genie_mks_roentgen);
  a68_idf (A68_EXT, "mksarad", MODE (REAL), genie_mks_rad);
  a68_idf (A68_EXT, "mksasolarmass", MODE (REAL), genie_mks_solar_mass);
  a68_idf (A68_EXT, "mksabohrradius", MODE (REAL), genie_mks_bohr_radius);
  a68_idf (A68_EXT, "mksavacuumpermittivity", MODE (REAL), genie_mks_vacuum_permittivity);
  a68_idf (A68_EXT, "mksanewton", MODE (REAL), genie_mks_newton);
  a68_idf (A68_EXT, "mksadyne", MODE (REAL), genie_mks_dyne);
  a68_idf (A68_EXT, "mksajoule", MODE (REAL), genie_mks_joule);
  a68_idf (A68_EXT, "mksaerg", MODE (REAL), genie_mks_erg);
  a68_idf (A68_EXT, "numfinestructure", MODE (REAL), genie_num_fine_structure);
  a68_idf (A68_EXT, "numavogadro", MODE (REAL), genie_num_avogadro);
  a68_idf (A68_EXT, "numyotta", MODE (REAL), genie_num_yotta);
  a68_idf (A68_EXT, "numzetta", MODE (REAL), genie_num_zetta);
  a68_idf (A68_EXT, "numexa", MODE (REAL), genie_num_exa);
  a68_idf (A68_EXT, "numpeta", MODE (REAL), genie_num_peta);
  a68_idf (A68_EXT, "numtera", MODE (REAL), genie_num_tera);
  a68_idf (A68_EXT, "numgiga", MODE (REAL), genie_num_giga);
  a68_idf (A68_EXT, "nummega", MODE (REAL), genie_num_mega);
  a68_idf (A68_EXT, "numkilo", MODE (REAL), genie_num_kilo);
  a68_idf (A68_EXT, "nummilli", MODE (REAL), genie_num_milli);
  a68_idf (A68_EXT, "nummicro", MODE (REAL), genie_num_micro);
  a68_idf (A68_EXT, "numnano", MODE (REAL), genie_num_nano);
  a68_idf (A68_EXT, "numpico", MODE (REAL), genie_num_pico);
  a68_idf (A68_EXT, "numfemto", MODE (REAL), genie_num_femto);
  a68_idf (A68_EXT, "numatto", MODE (REAL), genie_num_atto);
  a68_idf (A68_EXT, "numzepto", MODE (REAL), genie_num_zepto);
  a68_idf (A68_EXT, "numyocto", MODE (REAL), genie_num_yocto);
  m = proc_real_real;
  a68_idf (A68_EXT, "erf", m, genie_erf_real);
  a68_idf (A68_EXT, "erfc", m, genie_erfc_real);
  a68_idf (A68_EXT, "gamma", m, genie_gamma_real);
  a68_idf (A68_EXT, "lngamma", m, genie_lngamma_real);
  a68_idf (A68_EXT, "factorial", m, genie_factorial_real);
  a68_idf (A68_EXT, "airyai", m, genie_airy_ai_real);
  a68_idf (A68_EXT, "airybi", m, genie_airy_bi_real);
  a68_idf (A68_EXT, "airyaiderivative", m, genie_airy_ai_deriv_real);
  a68_idf (A68_EXT, "airybiderivative", m, genie_airy_bi_deriv_real);
  a68_idf (A68_EXT, "ellipticintegralk", m, genie_elliptic_integral_k_real);
  a68_idf (A68_EXT, "ellipticintegrale", m, genie_elliptic_integral_e_real);
  m = proc_real_real_real;
  a68_idf (A68_EXT, "beta", m, genie_beta_real);
  a68_idf (A68_EXT, "besseljn", m, genie_bessel_jn_real);
  a68_idf (A68_EXT, "besselyn", m, genie_bessel_yn_real);
  a68_idf (A68_EXT, "besselin", m, genie_bessel_in_real);
  a68_idf (A68_EXT, "besselexpin", m, genie_bessel_exp_in_real);
  a68_idf (A68_EXT, "besselkn", m, genie_bessel_kn_real);
  a68_idf (A68_EXT, "besselexpkn", m, genie_bessel_exp_kn_real);
  a68_idf (A68_EXT, "besseljl", m, genie_bessel_jl_real);
  a68_idf (A68_EXT, "besselyl", m, genie_bessel_yl_real);
  a68_idf (A68_EXT, "besselexpil", m, genie_bessel_exp_il_real);
  a68_idf (A68_EXT, "besselexpkl", m, genie_bessel_exp_kl_real);
  a68_idf (A68_EXT, "besseljnu", m, genie_bessel_jnu_real);
  a68_idf (A68_EXT, "besselynu", m, genie_bessel_ynu_real);
  a68_idf (A68_EXT, "besselinu", m, genie_bessel_inu_real);
  a68_idf (A68_EXT, "besselexpinu", m, genie_bessel_exp_inu_real);
  a68_idf (A68_EXT, "besselknu", m, genie_bessel_knu_real);
  a68_idf (A68_EXT, "besselexpknu", m, genie_bessel_exp_knu_real);
  a68_idf (A68_EXT, "ellipticintegralrc", m, genie_elliptic_integral_rc_real);
  a68_idf (A68_EXT, "incompletegamma", m, genie_gamma_inc_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A68_EXT, "incompletebeta", m, genie_beta_inc_real);
  a68_idf (A68_EXT, "ellipticintegralrf", m, genie_elliptic_integral_rf_real);
  a68_idf (A68_EXT, "ellipticintegralrd", m, genie_elliptic_integral_rd_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A68_EXT, "ellipticintegralrj", m, genie_elliptic_integral_rj_real);
/* Vector and matrix monadic. */
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_vector_minus);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_matrix_minus);
  a68_op (A68_EXT, "T", m, genie_matrix_transpose);
  a68_op (A68_EXT, "INV", m, genie_matrix_inv);
  m = a68_proc (MODE (REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_EXT, "DET", m, genie_matrix_det);
  a68_op (A68_EXT, "TRACE", m, genie_matrix_trace);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_vector_complex_minus);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_matrix_complex_minus);
  a68_op (A68_EXT, "T", m, genie_matrix_complex_transpose);
  a68_op (A68_EXT, "INV", m, genie_matrix_complex_inv);
  m = a68_proc (MODE (COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_EXT, "DET", m, genie_matrix_complex_det);
  a68_op (A68_EXT, "TRACE", m, genie_matrix_complex_trace);
/* Vector and matrix dyadic. */
  m = a68_proc (MODE (BOOL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "=", m, genie_vector_eq);
  a68_op (A68_EXT, "/=", m, genie_vector_ne);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "+", m, genie_vector_add);
  a68_op (A68_EXT, "-", m, genie_vector_sub);
  m = a68_proc (MODE (REF_ROW_REAL), MODE (REF_ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "+:=", m, genie_vector_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_vector_plusab);
  a68_op (A68_EXT, "-:=", m, genie_vector_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_vector_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_EXT, "=", m, genie_matrix_eq);
  a68_op (A68_EXT, "/-", m, genie_matrix_ne);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_EXT, "+", m, genie_matrix_add);
  a68_op (A68_EXT, "-", m, genie_matrix_sub);
  m = a68_proc (MODE (REF_ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_EXT, "+:=", m, genie_matrix_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_matrix_plusab);
  a68_op (A68_EXT, "-:=", m, genie_matrix_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_matrix_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "=", m, genie_vector_complex_eq);
  a68_op (A68_EXT, "/=", m, genie_vector_complex_ne);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "+", m, genie_vector_complex_add);
  a68_op (A68_EXT, "-", m, genie_vector_complex_sub);
  m = a68_proc (MODE (REF_ROW_COMPLEX), MODE (REF_ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "+:=", m, genie_vector_complex_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_vector_complex_plusab);
  a68_op (A68_EXT, "-:=", m, genie_vector_complex_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_vector_complex_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_EXT, "=", m, genie_matrix_complex_eq);
  a68_op (A68_EXT, "/=", m, genie_matrix_complex_ne);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_EXT, "+", m, genie_matrix_complex_add);
  a68_op (A68_EXT, "-", m, genie_matrix_complex_sub);
  m = a68_proc (MODE (REF_ROWROW_COMPLEX), MODE (REF_ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_EXT, "+:=", m, genie_matrix_complex_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_matrix_complex_plusab);
  a68_op (A68_EXT, "-:=", m, genie_matrix_complex_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_matrix_complex_minusab);
/* Vector and matrix scaling. */
  m = a68_proc (MODE (ROW_REAL), MODE (REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "*", m, genie_real_scale_vector);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (REAL), NULL);
  a68_op (A68_EXT, "*", m, genie_vector_scale_real);
  a68_op (A68_EXT, "/", m, genie_vector_div_real);
  m = a68_proc (MODE (ROWROW_REAL), MODE (REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_EXT, "*", m, genie_real_scale_matrix);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REAL), NULL);
  a68_op (A68_EXT, "*", m, genie_matrix_scale_real);
  a68_op (A68_EXT, "/", m, genie_matrix_div_real);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "*", m, genie_complex_scale_vector_complex);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_EXT, "*", m, genie_vector_complex_scale_complex);
  a68_op (A68_EXT, "/", m, genie_vector_complex_div_complex);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_EXT, "*", m, genie_complex_scale_matrix_complex);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_scale_complex);
  a68_op (A68_EXT, "/", m, genie_matrix_complex_div_complex);
  m = a68_proc (MODE (REF_ROW_REAL), MODE (REF_ROW_REAL), MODE (REAL), NULL);
  a68_op (A68_EXT, "*:=", m, genie_vector_scale_real_ab);
  a68_op (A68_EXT, "/:=", m, genie_vector_div_real_ab);
  m = a68_proc (MODE (REF_ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (REAL), NULL);
  a68_op (A68_EXT, "*:=", m, genie_matrix_scale_real_ab);
  a68_op (A68_EXT, "/:=", m, genie_matrix_div_real_ab);
  m = a68_proc (MODE (REF_ROW_COMPLEX), MODE (REF_ROW_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_EXT, "*:=", m, genie_vector_complex_scale_complex_ab);
  a68_op (A68_EXT, "/:=", m, genie_vector_complex_div_complex_ab);
  m = a68_proc (MODE (REF_ROWROW_COMPLEX), MODE (REF_ROWROW_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A68_EXT, "*:=", m, genie_matrix_complex_scale_complex_ab);
  a68_op (A68_EXT, "/:=", m, genie_matrix_complex_div_complex_ab);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_EXT, "*", m, genie_vector_times_matrix);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_EXT, "*", m, genie_vector_complex_times_matrix);
/* Matrix times vector or matrix. */
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "*", m, genie_matrix_times_vector);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_op (A68_EXT, "*", m, genie_matrix_times_matrix);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_times_vector);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_times_matrix);
/* Vector and matrix miscellaneous. */
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A68_EXT, "vectorecho", m, genie_vector_echo);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_idf (A68_EXT, "matrixecho", m, genie_matrix_echo);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_idf (A68_EXT, "complvectorecho", m, genie_vector_complex_echo);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NULL);
  a68_idf (A68_EXT, "complmatrixecho", m, genie_matrix_complex_echo);
   /**/ m = a68_proc (MODE (REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "*", m, genie_vector_dot);
  m = a68_proc (MODE (COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "*", m, genie_vector_complex_dot);
  m = a68_proc (MODE (REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "NORM", m, genie_vector_norm);
  m = a68_proc (MODE (REAL), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "NORM", m, genie_vector_complex_norm);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_op (A68_EXT, "DYAD", m, genie_vector_dyad);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_op (A68_EXT, "DYAD", m, genie_vector_complex_dyad);
/* LU decomposition. */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROW_INT), MODE (REF_INT), NULL);
  a68_idf (A68_EXT, "ludecomp", m, genie_matrix_lu);
  m = a68_proc (MODE (REAL), MODE (ROWROW_REAL), MODE (INT), NULL);
  a68_idf (A68_EXT, "ludet", m, genie_matrix_lu_det);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_INT), NULL);
  a68_idf (A68_EXT, "luinv", m, genie_matrix_lu_inv);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_INT), MODE (ROW_REAL), NULL);
  a68_idf (A68_EXT, "lusolve", m, genie_matrix_lu_solve);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (REF_ROW_INT), MODE (REF_INT), NULL);
  a68_idf (A68_EXT, "complexludecomp", m, genie_matrix_complex_lu);
  m = a68_proc (MODE (COMPLEX), MODE (ROWROW_COMPLEX), MODE (INT), NULL);
  a68_idf (A68_EXT, "complexludet", m, genie_matrix_complex_lu_det);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_INT), NULL);
  a68_idf (A68_EXT, "complexluinv", m, genie_matrix_complex_lu_inv);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_INT), MODE (ROW_COMPLEX), NULL);
  a68_idf (A68_EXT, "complexlusolve", m, genie_matrix_complex_lu_solve);
/* SVD decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (REF_ROW_REAL), NULL);
  a68_idf (A68_EXT, "svdecomp", m, genie_matrix_svd);
  a68_idf (A68_EXT, "svddecomp", m, genie_matrix_svd);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A68_EXT, "svdsolve", m, genie_matrix_svd_solve);
/* QR decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROW_REAL), NULL);
  a68_idf (A68_EXT, "qrdecomp", m, genie_matrix_qr);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A68_EXT, "qrsolve", m, genie_matrix_qr_solve);
  a68_idf (A68_EXT, "qrlssolve", m, genie_matrix_qr_ls_solve);
/* Cholesky decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NULL);
  a68_idf (A68_EXT, "choleskydecomp", m, genie_matrix_ch);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A68_EXT, "choleskysolve", m, genie_matrix_ch_solve);
/* FFT */
  m = a68_proc (MODE (ROW_INT), MODE (INT), NULL);
  a68_idf (A68_EXT, "primefactors", m, genie_prime_factors);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NULL);
  a68_idf (A68_EXT, "fftcomplexforward", m, genie_fft_complex_forward);
  a68_idf (A68_EXT, "fftcomplexbackward", m, genie_fft_complex_backward);
  a68_idf (A68_EXT, "fftcomplexinverse", m, genie_fft_complex_inverse);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_REAL), NULL);
  a68_idf (A68_EXT, "fftforward", m, genie_fft_forward);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_COMPLEX), NULL);
  a68_idf (A68_EXT, "fftbackward", m, genie_fft_backward);
  a68_idf (A68_EXT, "fftinverse", m, genie_fft_inverse);
#endif
/* UNIX things */
  m = proc_int;
  a68_idf (A68_EXT, "argc", m, genie_argc);
  a68_idf (A68_EXT, "errno", m, genie_errno);
  a68_idf (A68_EXT, "fork", m, genie_fork);
  m = a68_proc (MODE (STRING), NULL);
  a68_idf (A68_EXT, "getpwd", m, genie_pwd);
  m = a68_proc (MODE (INT), MODE (STRING), NULL);
  a68_idf (A68_EXT, "setpwd", m, genie_cd);
  m = a68_proc (MODE (BOOL), MODE (STRING), NULL);
  a68_idf (A68_EXT, "fileisdirectory", m, genie_file_is_directory);
  a68_idf (A68_EXT, "fileisregular", m, genie_file_is_block_device);
  a68_idf (A68_EXT, "fileisregular", m, genie_file_is_char_device);
  a68_idf (A68_EXT, "fileisregular", m, genie_file_is_regular);
#ifdef __S_IFIFO
  a68_idf (A68_EXT, "fileisfifo", m, genie_file_is_fifo);
#endif
#ifdef __S_IFLNK
  a68_idf (A68_EXT, "fileislink", m, genie_file_is_link);
#endif
  m = a68_proc (MODE (BITS), MODE (STRING), NULL);
  a68_idf (A68_EXT, "filemode", m, genie_file_mode);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A68_EXT, "argv", m, genie_argv);
  m = proc_void;
  a68_idf (A68_EXT, "reseterrno", m, genie_reset_errno);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A68_EXT, "strerror", m, genie_strerror);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A68_EXT, "execve", m, genie_execve);
  m = a68_proc (MODE (PIPE), NULL);
  a68_idf (A68_EXT, "createpipe", m, genie_create_pipe);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A68_EXT, "execvechild", m, genie_execve_child);
  m = a68_proc (MODE (PIPE), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A68_EXT, "execvechildpipe", m, genie_execve_child_pipe);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), MODE (REF_STRING), NULL);
  a68_idf (A68_EXT, "execveoutput", m, genie_execve_output);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A68_EXT, "getenv", m, genie_getenv);
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A68_EXT, "waitpid", m, genie_waitpid);
  m = a68_proc (MODE (ROW_INT), NULL);
  a68_idf (A68_EXT, "utctime", m, genie_utctime);
  a68_idf (A68_EXT, "localtime", m, genie_localtime);
#if defined ENABLE_DIRENT
  m = a68_proc (MODE (ROW_STRING), MODE (STRING), NULL);
  a68_idf (A68_EXT, "getdirectory", m, genie_directory);
#endif
#if defined ENABLE_HTTP
  m = a68_proc (MODE (INT), MODE (REF_STRING), MODE (STRING), MODE (STRING), MODE (INT), NULL);
  a68_idf (A68_EXT, "httpcontent", m, genie_http_content);
  a68_idf (A68_EXT, "tcprequest", m, genie_tcp_request);
#endif
#if defined ENABLE_REGEX
  m = a68_proc (MODE (INT), MODE (STRING), MODE (STRING), MODE (REF_INT), MODE (REF_INT), NULL);
  a68_idf (A68_EXT, "grepinstring", m, genie_grep_in_string);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (STRING), MODE (REF_STRING), NULL);
  a68_idf (A68_EXT, "subinstring", m, genie_sub_in_string);
#endif
#if defined ENABLE_CURSES
  m = proc_void;
  a68_idf (A68_EXT, "cursesstart", m, genie_curses_start);
  a68_idf (A68_EXT, "cursesend", m, genie_curses_end);
  a68_idf (A68_EXT, "cursesclear", m, genie_curses_clear);
  a68_idf (A68_EXT, "cursesrefresh", m, genie_curses_refresh);
  m = proc_char;
  a68_idf (A68_EXT, "cursesgetchar", m, genie_curses_getchar);
  m = a68_proc (MODE (VOID), MODE (CHAR), NULL);
  a68_idf (A68_EXT, "cursesputchar", m, genie_curses_putchar);
  m = a68_proc (MODE (VOID), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_EXT, "cursesmove", m, genie_curses_move);
  m = proc_int;
  a68_idf (A68_EXT, "curseslines", m, genie_curses_lines);
  a68_idf (A68_EXT, "cursescolumns", m, genie_curses_columns);
#endif
#if defined ENABLE_POSTGRESQL
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), MODE (REF_STRING), NULL);
  a68_idf (A68_EXT, "pqconnectdb", m, genie_pq_connectdb);
  m = a68_proc (MODE (INT), MODE (REF_FILE), NULL);
  a68_idf (A68_EXT, "pqfinish", m, genie_pq_finish);
  a68_idf (A68_EXT, "pqreset", m, genie_pq_reset);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A68_EXT, "pqparameterstatus", m, genie_pq_parameterstatus);
  a68_idf (A68_EXT, "pqexec", m, genie_pq_exec);
  a68_idf (A68_EXT, "pqfnumber", m, genie_pq_fnumber);
  m = a68_proc (MODE (INT), MODE (REF_FILE), NULL);
  a68_idf (A68_EXT, "pqntuples", m, genie_pq_ntuples);
  a68_idf (A68_EXT, "pqnfields", m, genie_pq_nfields);
  a68_idf (A68_EXT, "pqcmdstatus", m, genie_pq_cmdstatus);
  a68_idf (A68_EXT, "pqcmdtuples", m, genie_pq_cmdtuples);
  a68_idf (A68_EXT, "pqerrormessage", m, genie_pq_errormessage);
  a68_idf (A68_EXT, "pqresulterrormessage", m, genie_pq_resulterrormessage);
  a68_idf (A68_EXT, "pqdb", m, genie_pq_db);
  a68_idf (A68_EXT, "pquser", m, genie_pq_user);
  a68_idf (A68_EXT, "pqpass", m, genie_pq_pass);
  a68_idf (A68_EXT, "pqhost", m, genie_pq_host);
  a68_idf (A68_EXT, "pqport", m, genie_pq_port);
  a68_idf (A68_EXT, "pqtty", m, genie_pq_tty);
  a68_idf (A68_EXT, "pqoptions", m, genie_pq_options);
  a68_idf (A68_EXT, "pqprotocolversion", m, genie_pq_protocolversion);
  a68_idf (A68_EXT, "pqserverversion", m, genie_pq_serverversion);
  a68_idf (A68_EXT, "pqsocket", m, genie_pq_socket);
  a68_idf (A68_EXT, "pqbackendpid", m, genie_pq_backendpid);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A68_EXT, "pqfname", m, genie_pq_fname);
  a68_idf (A68_EXT, "pqfformat", m, genie_pq_fformat);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), MODE (INT), NULL);
  a68_idf (A68_EXT, "pqgetvalue", m, genie_pq_getvalue);
  a68_idf (A68_EXT, "pqgetisnull", m, genie_pq_getisnull);
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
  proc_real_real = MODE (PROC_REAL_REAL);
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
