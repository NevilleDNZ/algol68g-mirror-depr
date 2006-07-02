/*!
\file prelude.c
\brief builds symbol table for standard prelude
*/

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

#include "algol68g.h"
#include "genie.h"
#include "transput.h"
#include "mp.h"
#include "gsl.h"

SYMBOL_TABLE_T *stand_env;

static MOID_T *m, *proc_int, *proc_real, *proc_real_real, *proc_real_real_real, *proc_complex_complex, *proc_bool, *proc_char, *proc_void;
static PACK_T *z;

#define INSERT_TAG(l, n) {NEXT (n) = *(l); *(l) = (n);}

/*!
\brief enter tag in standenv symbol table
\param a
\param n
\param c
\param m
\param p
\param q
**/

static void add_stand_env (BOOL_T portable, int a, NODE_T * n, char *c, MOID_T * m, int p, GENIE_PROCEDURE * q)
{
  TAG_T *new_one = new_tag ();
  n->info->PROCEDURE_LEVEL = 0;
  n->info->PROCEDURE_NUMBER = 0;
  new_one->use = A_FALSE;
  HEAP (new_one) = HEAP_SYMBOL;
  SYMBOL_TABLE (new_one) = stand_env;
  NODE (new_one) = n;
  new_one->value = c != NULL ? add_token (&top_token, c)->text : NULL;
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
}

#undef INSERT_TAG

/*!
\brief compose PROC moid from arguments - first result, than arguments
\param m
\return
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
\param n
\param m
\param q
**/

static void a68_idf (BOOL_T portable, char *n, MOID_T * m, GENIE_PROCEDURE * q)
{
  add_stand_env (portable, IDENTIFIER, some_node (TEXT (add_token (&top_token, n))), NULL, m, 0, q);
}

/*!
\brief enter a MOID in standenv
\param p
\param t
\param m
**/

static void a68_mode (int p, char *t, MOID_T ** m)
{
  add_mode (&stand_env->moids, STANDARD, p, some_node (TEXT (find_keyword (top_keyword, t))), NULL, NULL);
  *m = stand_env->moids;
}

/*!
\brief enter a priority in standenv
\param p
\param b
**/

static void a68_prio (char *p, int b)
{
  add_stand_env (A_TRUE, PRIO_SYMBOL, some_node (TEXT (add_token (&top_token, p))), NULL, NULL, b, NULL);
}

/*!
\brief enter operator in standenv
\param n
\param m
\param q
**/

static void a68_op (BOOL_T portable, char *n, MOID_T * m, GENIE_PROCEDURE * q)
{
  add_stand_env (portable, OP_SYMBOL, some_node (TEXT (add_token (&top_token, n))), NULL, m, 0, q);
}

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
/* [] REAL and alikes. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (REAL), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROW_REAL) = stand_env->moids;
  SLICE (MODE (ROW_REAL)) = MODE (REAL);
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROW_REAL), NULL);
  MODE (REF_ROW_REAL) = stand_env->moids;
  NAME (MODE (REF_ROW_REAL)) = MODE (REF_REAL);
  add_mode (&stand_env->moids, ROW_SYMBOL, 2, NULL, MODE (REAL), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROWROW_REAL) = stand_env->moids;
  SLICE (MODE (ROWROW_REAL)) = MODE (ROW_REAL);
  add_mode (&stand_env->moids, REF_SYMBOL, 0, NULL, MODE (ROWROW_REAL), NULL);
  MODE (REF_ROWROW_REAL) = stand_env->moids;
  NAME (MODE (REF_ROWROW_REAL)) = MODE (REF_ROW_REAL);
/* [] INT. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (INT), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROW_INT) = stand_env->moids;
  SLICE (MODE (ROW_INT)) = MODE (INT);
/* [] BOOL. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (BOOL), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROW_BOOL) = stand_env->moids;
  SLICE (MODE (ROW_BOOL)) = MODE (BOOL);
/* [] BITS. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (BITS), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROW_BITS) = stand_env->moids;
  SLICE (MODE (ROW_BITS)) = MODE (BITS);
/* [] LONG BITS. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (LONG_BITS), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROW_LONG_BITS) = stand_env->moids;
  SLICE (MODE (ROW_LONG_BITS)) = MODE (LONG_BITS);
/* [] LONG LONG BITS. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (LONGLONG_BITS), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROW_LONGLONG_BITS) = stand_env->moids;
  SLICE (MODE (ROW_LONGLONG_BITS)) = MODE (LONGLONG_BITS);
/* [] CHAR. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (CHAR), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROW_CHAR) = stand_env->moids;
  SLICE (MODE (ROW_CHAR)) = MODE (CHAR);
/* [][] CHAR. */
  add_mode (&stand_env->moids, ROW_SYMBOL, 1, NULL, MODE (ROW_CHAR), NULL);
  stand_env->moids->has_rows = A_TRUE;
  MODE (ROW_ROW_CHAR) = stand_env->moids;
  SLICE (MODE (ROW_ROW_CHAR)) = MODE (ROW_CHAR);
/* MODE STRING = FLEX [] CHAR. */
  add_mode (&stand_env->moids, FLEX_SYMBOL, 0, NULL, MODE (ROW_CHAR), NULL);
  stand_env->moids->has_rows = A_TRUE;
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
  stand_env->moids->has_rows = A_TRUE;
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
  MODE (PIPE)->portable = A_FALSE;
  z = NULL;
  add_mode_to_pack (&z, MODE (REF_INT), add_token (&top_token, "pid")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_REF_FILE), add_token (&top_token, "write")->text, NULL);
  add_mode_to_pack (&z, MODE (REF_REF_FILE), add_token (&top_token, "read")->text, NULL);
  add_mode (&stand_env->moids, STRUCT_SYMBOL, count_pack_members (z), NULL, NULL, z);
  NAME (MODE (REF_PIPE)) = stand_env->moids;
}

static void stand_prelude (void)
{
/* Identifiers. */
  a68_idf (A_TRUE, "intlengths", MODE (INT), genie_int_lengths);
  a68_idf (A_TRUE, "intshorths", MODE (INT), genie_int_shorths);
  a68_idf (A_TRUE, "maxint", MODE (INT), genie_max_int);
  a68_idf (A_TRUE, "maxreal", MODE (REAL), genie_max_real);
  a68_idf (A_TRUE, "smallreal", MODE (REAL), genie_small_real);
  a68_idf (A_TRUE, "reallengths", MODE (INT), genie_real_lengths);
  a68_idf (A_TRUE, "realshorths", MODE (INT), genie_real_shorths);
  a68_idf (A_TRUE, "compllengths", MODE (INT), genie_complex_lengths);
  a68_idf (A_TRUE, "complshorths", MODE (INT), genie_complex_shorths);
  a68_idf (A_TRUE, "bitslengths", MODE (INT), genie_bits_lengths);
  a68_idf (A_TRUE, "bitsshorths", MODE (INT), genie_bits_shorths);
  a68_idf (A_TRUE, "bitswidth", MODE (INT), genie_bits_width);
  a68_idf (A_TRUE, "longbitswidth", MODE (INT), genie_long_bits_width);
  a68_idf (A_TRUE, "longlongbitswidth", MODE (INT), genie_longlong_bits_width);
  a68_idf (A_TRUE, "maxbits", MODE (BITS), genie_max_bits);
  a68_idf (A_TRUE, "longmaxbits", MODE (LONG_BITS), genie_long_max_bits);
  a68_idf (A_TRUE, "longlongmaxbits", MODE (LONGLONG_BITS), genie_longlong_max_bits);
  a68_idf (A_TRUE, "byteslengths", MODE (INT), genie_bytes_lengths);
  a68_idf (A_TRUE, "bytesshorths", MODE (INT), genie_bytes_shorths);
  a68_idf (A_TRUE, "byteswidth", MODE (INT), genie_bytes_width);
  a68_idf (A_TRUE, "maxabschar", MODE (INT), genie_max_abs_char);
  a68_idf (A_TRUE, "pi", MODE (REAL), genie_pi);
  a68_idf (A_TRUE, "dpi", MODE (LONG_REAL), genie_pi_long_mp);
  a68_idf (A_TRUE, "longpi", MODE (LONG_REAL), genie_pi_long_mp);
  a68_idf (A_TRUE, "qpi", MODE (LONGLONG_REAL), genie_pi_long_mp);
  a68_idf (A_TRUE, "longlongpi", MODE (LONGLONG_REAL), genie_pi_long_mp);
  a68_idf (A_TRUE, "intwidth", MODE (INT), genie_int_width);
  a68_idf (A_TRUE, "realwidth", MODE (INT), genie_real_width);
  a68_idf (A_TRUE, "expwidth", MODE (INT), genie_exp_width);
  a68_idf (A_TRUE, "longintwidth", MODE (INT), genie_long_int_width);
  a68_idf (A_TRUE, "longlongintwidth", MODE (INT), genie_longlong_int_width);
  a68_idf (A_TRUE, "longrealwidth", MODE (INT), genie_long_real_width);
  a68_idf (A_TRUE, "longlongrealwidth", MODE (INT), genie_longlong_real_width);
  a68_idf (A_TRUE, "longexpwidth", MODE (INT), genie_long_exp_width);
  a68_idf (A_TRUE, "longlongexpwidth", MODE (INT), genie_longlong_exp_width);
  a68_idf (A_TRUE, "longmaxint", MODE (LONG_INT), genie_long_max_int);
  a68_idf (A_TRUE, "longlongmaxint", MODE (LONGLONG_INT), genie_longlong_max_int);
  a68_idf (A_TRUE, "longsmallreal", MODE (LONG_REAL), genie_long_small_real);
  a68_idf (A_TRUE, "longlongsmallreal", MODE (LONGLONG_REAL), genie_longlong_small_real);
  a68_idf (A_TRUE, "longmaxreal", MODE (LONG_REAL), genie_long_max_real);
  a68_idf (A_TRUE, "longlongmaxreal", MODE (LONGLONG_REAL), genie_longlong_max_real);
  a68_idf (A_TRUE, "longbyteswidth", MODE (INT), genie_long_bytes_width);
  a68_idf (A_FALSE, "seconds", MODE (REAL), genie_seconds);
  a68_idf (A_FALSE, "clock", MODE (REAL), genie_cputime);
  a68_idf (A_FALSE, "cputime", MODE (REAL), genie_cputime);
  m = proc_int;
  a68_idf (A_FALSE, "collections", m, genie_garbage_collections);
  m = a68_proc (MODE (LONG_INT), NULL);
  a68_idf (A_FALSE, "garbage", m, genie_garbage_freed);
  m = proc_real;
  a68_idf (A_FALSE, "collectseconds", m, genie_garbage_seconds);
  a68_idf (A_FALSE, "stackpointer", MODE (INT), genie_stack_pointer);
  a68_idf (A_FALSE, "systemstackpointer", MODE (INT), genie_system_stack_pointer);
  a68_idf (A_FALSE, "systemstacksize", MODE (INT), genie_system_stack_size);
  a68_idf (A_FALSE, "actualstacksize", MODE (INT), genie_stack_pointer);
  m = proc_void;
  a68_idf (A_FALSE, "sweepheap", m, genie_sweep_heap);
  a68_idf (A_FALSE, "preemptivesweepheap", m, genie_preemptive_sweep_heap);
  a68_idf (A_FALSE, "break", m, genie_break);
  a68_idf (A_FALSE, "debug", m, genie_debug);
  a68_idf (A_FALSE, "monitor", m, genie_debug);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A_FALSE, "evaluate", m, genie_evaluate);
  m = a68_proc (MODE (INT), MODE (STRING), NULL);
  a68_idf (A_FALSE, "system", m, genie_system);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A_FALSE, "acronym", m, genie_acronym);
  a68_idf (A_FALSE, "vmsacronym", m, genie_acronym);
/* BITS procedures. */
  m = a68_proc (MODE (BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A_TRUE, "bitspack", m, genie_bits_pack);
  m = a68_proc (MODE (LONG_BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A_TRUE, "longbitspack", m, genie_long_bits_pack);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (ROW_BOOL), NULL);
  a68_idf (A_TRUE, "longlongbitspack", m, genie_long_bits_pack);
/* RNG procedures. */
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A_TRUE, "firstrandom", m, genie_first_random);
  m = proc_real;
  a68_idf (A_TRUE, "nextrandom", m, genie_next_random);
  a68_idf (A_TRUE, "random", m, genie_next_random);
  m = a68_proc (MODE (LONG_REAL), NULL);
  a68_idf (A_TRUE, "longnextrandom", m, genie_long_next_random);
  a68_idf (A_TRUE, "longrandom", m, genie_long_next_random);
  m = a68_proc (MODE (LONGLONG_REAL), NULL);
  a68_idf (A_TRUE, "longlongnextrandom", m, genie_long_next_random);
  a68_idf (A_TRUE, "longlongrandom", m, genie_long_next_random);
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
  a68_prio ("**", 8);
  a68_prio ("SHL", 8);
  a68_prio ("SHR", 8);
  a68_prio ("UP", 8);
  a68_prio ("DOWN", 8);
  a68_prio ("^", 8);
  a68_prio ("ELEMS", 8);
  a68_prio ("LWB", 8);
  a68_prio ("UPB", 8);
  a68_prio ("I", 9);
  a68_prio ("+*", 9);
/* INT ops. */
  m = a68_proc (MODE (INT), MODE (INT), NULL);
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_int);
  a68_op (A_TRUE, "ABS", m, genie_abs_int);
  a68_op (A_TRUE, "SIGN", m, genie_sign_int);
  m = a68_proc (MODE (BOOL), MODE (INT), NULL);
  a68_op (A_TRUE, "ODD", m, genie_odd_int);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (INT), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_int);
  a68_op (A_TRUE, "/=", m, genie_ne_int);
  a68_op (A_TRUE, "~=", m, genie_ne_int);
  a68_op (A_TRUE, "^=", m, genie_ne_int);
  a68_op (A_TRUE, "<", m, genie_lt_int);
  a68_op (A_TRUE, "<=", m, genie_le_int);
  a68_op (A_TRUE, ">", m, genie_gt_int);
  a68_op (A_TRUE, ">=", m, genie_ge_int);
  a68_op (A_TRUE, "EQ", m, genie_eq_int);
  a68_op (A_TRUE, "NE", m, genie_ne_int);
  a68_op (A_TRUE, "LT", m, genie_lt_int);
  a68_op (A_TRUE, "LE", m, genie_le_int);
  a68_op (A_TRUE, "GT", m, genie_gt_int);
  a68_op (A_TRUE, "GE", m, genie_ge_int);
  m = a68_proc (MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_op (A_TRUE, "+", m, genie_add_int);
  a68_op (A_TRUE, "-", m, genie_sub_int);
  a68_op (A_TRUE, "*", m, genie_mul_int);
  a68_op (A_TRUE, "OVER", m, genie_over_int);
  a68_op (A_TRUE, "%", m, genie_over_int);
  a68_op (A_TRUE, "MOD", m, genie_mod_int);
  a68_op (A_TRUE, "%*", m, genie_mod_int);
  a68_op (A_TRUE, "**", m, genie_pow_int);
  a68_op (A_TRUE, "UP", m, genie_pow_int);
  a68_op (A_TRUE, "^", m, genie_pow_int);
  m = a68_proc (MODE (REAL), MODE (INT), MODE (INT), NULL);
  a68_op (A_TRUE, "/", m, genie_div_int);
  m = a68_proc (MODE (REF_INT), MODE (REF_INT), MODE (INT), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_int);
  a68_op (A_TRUE, "-:=", m, genie_minusab_int);
  a68_op (A_TRUE, "*:=", m, genie_timesab_int);
  a68_op (A_TRUE, "%:=", m, genie_overab_int);
  a68_op (A_TRUE, "%*:=", m, genie_modab_int);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_int);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_int);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_int);
  a68_op (A_TRUE, "OVERAB", m, genie_overab_int);
  a68_op (A_TRUE, "MODAB", m, genie_modab_int);
/* REAL ops. */
  m = proc_real_real;
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_real);
  a68_op (A_TRUE, "ABS", m, genie_abs_real);
  a68_op (A_TRUE, "NINT", m, genie_nint_real);
  m = a68_proc (MODE (INT), MODE (REAL), NULL);
  a68_op (A_TRUE, "SIGN", m, genie_sign_real);
  a68_op (A_TRUE, "ROUND", m, genie_round_real);
  a68_op (A_TRUE, "ENTIER", m, genie_entier_real);
  m = a68_proc (MODE (BOOL), MODE (REAL), MODE (REAL), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_real);
  a68_op (A_TRUE, "/=", m, genie_ne_real);
  a68_op (A_TRUE, "~=", m, genie_ne_real);
  a68_op (A_TRUE, "^=", m, genie_ne_real);
  a68_op (A_TRUE, "<", m, genie_lt_real);
  a68_op (A_TRUE, "<=", m, genie_le_real);
  a68_op (A_TRUE, ">", m, genie_gt_real);
  a68_op (A_TRUE, ">=", m, genie_ge_real);
  a68_op (A_TRUE, "EQ", m, genie_eq_real);
  a68_op (A_TRUE, "NE", m, genie_ne_real);
  a68_op (A_TRUE, "LT", m, genie_lt_real);
  a68_op (A_TRUE, "LE", m, genie_le_real);
  a68_op (A_TRUE, "GT", m, genie_gt_real);
  a68_op (A_TRUE, "GE", m, genie_ge_real);
  m = proc_real_real_real;
  a68_op (A_TRUE, "+", m, genie_add_real);
  a68_op (A_TRUE, "-", m, genie_sub_real);
  a68_op (A_TRUE, "*", m, genie_mul_real);
  a68_op (A_TRUE, "/", m, genie_div_real);
  a68_op (A_TRUE, "**", m, genie_pow_real);
  a68_op (A_TRUE, "UP", m, genie_pow_real);
  a68_op (A_TRUE, "^", m, genie_pow_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (INT), NULL);
  a68_op (A_TRUE, "**", m, genie_pow_real_int);
  a68_op (A_TRUE, "UP", m, genie_pow_real_int);
  a68_op (A_TRUE, "^", m, genie_pow_real_int);
  m = a68_proc (MODE (REF_REAL), MODE (REF_REAL), MODE (REAL), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_real);
  a68_op (A_TRUE, "-:=", m, genie_minusab_real);
  a68_op (A_TRUE, "*:=", m, genie_timesab_real);
  a68_op (A_TRUE, "/:=", m, genie_divab_real);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_real);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_real);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_real);
  a68_op (A_TRUE, "DIVAB", m, genie_divab_real);
  m = proc_real_real;
  a68_idf (A_TRUE, "sqrt", m, genie_sqrt_real);
  a68_idf (A_FALSE, "cbrt", m, genie_curt_real);
  a68_idf (A_FALSE, "curt", m, genie_curt_real);
  a68_idf (A_TRUE, "exp", m, genie_exp_real);
  a68_idf (A_TRUE, "ln", m, genie_ln_real);
  a68_idf (A_TRUE, "log", m, genie_log_real);
  a68_idf (A_TRUE, "sin", m, genie_sin_real);
  a68_idf (A_TRUE, "cos", m, genie_cos_real);
  a68_idf (A_TRUE, "tan", m, genie_tan_real);
  a68_idf (A_TRUE, "asin", m, genie_arcsin_real);
  a68_idf (A_TRUE, "acos", m, genie_arccos_real);
  a68_idf (A_TRUE, "atan", m, genie_arctan_real);
  a68_idf (A_TRUE, "arcsin", m, genie_arcsin_real);
  a68_idf (A_TRUE, "arccos", m, genie_arccos_real);
  a68_idf (A_TRUE, "arctan", m, genie_arctan_real);
  a68_idf (A_FALSE, "sinh", m, genie_sinh_real);
  a68_idf (A_FALSE, "cosh", m, genie_cosh_real);
  a68_idf (A_FALSE, "tanh", m, genie_tanh_real);
  a68_idf (A_FALSE, "asinh", m, genie_arcsinh_real);
  a68_idf (A_FALSE, "acosh", m, genie_arccosh_real);
  a68_idf (A_FALSE, "atanh", m, genie_arctanh_real);
  a68_idf (A_FALSE, "arcsinh", m, genie_arcsinh_real);
  a68_idf (A_FALSE, "arccosh", m, genie_arccosh_real);
  a68_idf (A_FALSE, "arctanh", m, genie_arctanh_real);
  a68_idf (A_FALSE, "inverseerf", m, genie_inverf_real);
  a68_idf (A_FALSE, "inverseerfc", m, genie_inverfc_real);
  m = proc_real_real_real;
  a68_idf (A_FALSE, "arctan2", m, genie_atan2_real);
/* COMPLEX ops. */
  m = a68_proc (MODE (COMPLEX), MODE (REAL), MODE (REAL), NULL);
  a68_op (A_TRUE, "I", m, genie_icomplex);
  a68_op (A_TRUE, "+*", m, genie_icomplex);
  m = a68_proc (MODE (COMPLEX), MODE (INT), MODE (INT), NULL);
  a68_op (A_TRUE, "I", m, genie_iint_complex);
  a68_op (A_TRUE, "+*", m, genie_iint_complex);
  m = a68_proc (MODE (REAL), MODE (COMPLEX), NULL);
  a68_op (A_TRUE, "RE", m, genie_re_complex);
  a68_op (A_TRUE, "IM", m, genie_im_complex);
  a68_op (A_TRUE, "ABS", m, genie_abs_complex);
  a68_op (A_TRUE, "ARG", m, genie_arg_complex);
  m = proc_complex_complex;
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_complex);
  a68_op (A_TRUE, "CONJ", m, genie_conj_complex);
  m = a68_proc (MODE (BOOL), MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_complex);
  a68_op (A_TRUE, "/=", m, genie_ne_complex);
  a68_op (A_TRUE, "~=", m, genie_ne_complex);
  a68_op (A_TRUE, "^=", m, genie_ne_complex);
  a68_op (A_TRUE, "EQ", m, genie_eq_complex);
  a68_op (A_TRUE, "NE", m, genie_ne_complex);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A_TRUE, "+", m, genie_add_complex);
  a68_op (A_TRUE, "-", m, genie_sub_complex);
  a68_op (A_TRUE, "*", m, genie_mul_complex);
  a68_op (A_TRUE, "/", m, genie_div_complex);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), MODE (INT), NULL);
  a68_op (A_TRUE, "**", m, genie_pow_complex_int);
  a68_op (A_TRUE, "UP", m, genie_pow_complex_int);
  a68_op (A_TRUE, "^", m, genie_pow_complex_int);
  m = a68_proc (MODE (REF_COMPLEX), MODE (REF_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_complex);
  a68_op (A_TRUE, "-:=", m, genie_minusab_complex);
  a68_op (A_TRUE, "*:=", m, genie_timesab_complex);
  a68_op (A_TRUE, "/:=", m, genie_divab_complex);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_complex);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_complex);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_complex);
  a68_op (A_TRUE, "DIVAB", m, genie_divab_complex);
/* BOOL ops. */
  m = a68_proc (MODE (BOOL), MODE (BOOL), NULL);
  a68_op (A_TRUE, "NOT", m, genie_not_bool);
  a68_op (A_TRUE, "~", m, genie_not_bool);
  m = a68_proc (MODE (INT), MODE (BOOL), NULL);
  a68_op (A_TRUE, "ABS", m, genie_abs_bool);
  m = a68_proc (MODE (BOOL), MODE (BOOL), MODE (BOOL), NULL);
  a68_op (A_TRUE, "OR", m, genie_or_bool);
  a68_op (A_TRUE, "AND", m, genie_and_bool);
  a68_op (A_TRUE, "&", m, genie_and_bool);
  a68_op (A_FALSE, "XOR", m, genie_xor_bool);
  a68_op (A_TRUE, "=", m, genie_eq_bool);
  a68_op (A_TRUE, "/=", m, genie_ne_bool);
  a68_op (A_TRUE, "~=", m, genie_ne_bool);
  a68_op (A_TRUE, "^=", m, genie_ne_bool);
  a68_op (A_TRUE, "EQ", m, genie_eq_bool);
  a68_op (A_TRUE, "NE", m, genie_ne_bool);
/* CHAR ops. */
  m = a68_proc (MODE (BOOL), MODE (CHAR), MODE (CHAR), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_char);
  a68_op (A_TRUE, "/=", m, genie_ne_char);
  a68_op (A_TRUE, "~=", m, genie_ne_char);
  a68_op (A_TRUE, "^=", m, genie_ne_char);
  a68_op (A_TRUE, "<", m, genie_lt_char);
  a68_op (A_TRUE, "<=", m, genie_le_char);
  a68_op (A_TRUE, ">", m, genie_gt_char);
  a68_op (A_TRUE, ">=", m, genie_ge_char);
  a68_op (A_TRUE, "EQ", m, genie_eq_char);
  a68_op (A_TRUE, "NE", m, genie_ne_char);
  a68_op (A_TRUE, "LT", m, genie_lt_char);
  a68_op (A_TRUE, "LE", m, genie_le_char);
  a68_op (A_TRUE, "GT", m, genie_gt_char);
  a68_op (A_TRUE, "GE", m, genie_ge_char);
  m = a68_proc (MODE (INT), MODE (CHAR), NULL);
  a68_op (A_TRUE, "ABS", m, genie_abs_char);
  m = a68_proc (MODE (CHAR), MODE (INT), NULL);
  a68_op (A_TRUE, "REPR", m, genie_repr_char);
/* BITS ops. */
  m = a68_proc (MODE (INT), MODE (BITS), NULL);
  a68_op (A_TRUE, "ABS", m, genie_idle);
  m = a68_proc (MODE (BITS), MODE (INT), NULL);
  a68_op (A_TRUE, "BIN", m, genie_bin_int);
  m = a68_proc (MODE (BITS), MODE (BITS), NULL);
  a68_op (A_TRUE, "NOT", m, genie_not_bits);
  a68_op (A_TRUE, "~", m, genie_not_bits);
  m = a68_proc (MODE (BOOL), MODE (BITS), MODE (BITS), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_bits);
  a68_op (A_TRUE, "/=", m, genie_ne_bits);
  a68_op (A_TRUE, "~=", m, genie_ne_bits);
  a68_op (A_TRUE, "^=", m, genie_ne_bits);
  a68_op (A_TRUE, "<", m, genie_lt_bits);
  a68_op (A_TRUE, "<=", m, genie_le_bits);
  a68_op (A_TRUE, ">", m, genie_gt_bits);
  a68_op (A_TRUE, ">=", m, genie_ge_bits);
  a68_op (A_TRUE, "EQ", m, genie_eq_bits);
  a68_op (A_TRUE, "NE", m, genie_ne_bits);
  a68_op (A_TRUE, "LT", m, genie_lt_bits);
  a68_op (A_TRUE, "LE", m, genie_le_bits);
  a68_op (A_TRUE, "GT", m, genie_gt_bits);
  a68_op (A_TRUE, "GE", m, genie_ge_bits);
  m = a68_proc (MODE (BITS), MODE (BITS), MODE (BITS), NULL);
  a68_op (A_TRUE, "AND", m, genie_and_bits);
  a68_op (A_TRUE, "&", m, genie_and_bits);
  a68_op (A_TRUE, "OR", m, genie_or_bits);
  a68_op (A_FALSE, "XOR", m, genie_xor_bits);
  m = a68_proc (MODE (BITS), MODE (BITS), MODE (INT), NULL);
  a68_op (A_TRUE, "SHL", m, genie_shl_bits);
  a68_op (A_TRUE, "UP", m, genie_shl_bits);
  a68_op (A_TRUE, "SHR", m, genie_shr_bits);
  a68_op (A_TRUE, "DOWN", m, genie_shr_bits);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (BITS), NULL);
  a68_op (A_TRUE, "ELEM", m, genie_elem_bits);
/* LONG BITS ops. */
  m = a68_proc (MODE (LONG_INT), MODE (LONG_BITS), NULL);
  a68_op (A_TRUE, "ABS", m, genie_idle);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "BIN", m, genie_bin_long_mp);
  m = a68_proc (MODE (BITS), MODE (LONG_BITS), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_shorten_long_mp_to_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (BITS), NULL);
  a68_op (A_TRUE, "LENG", m, genie_lengthen_unsigned_to_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A_TRUE, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A_TRUE, "NOT", m, genie_not_long_mp);
  a68_op (A_TRUE, "~", m, genie_not_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), MODE (LONG_BITS), NULL);
  a68_op (A_TRUE, "AND", m, genie_and_long_mp);
  a68_op (A_TRUE, "&", m, genie_and_long_mp);
  a68_op (A_TRUE, "OR", m, genie_or_long_mp);
  a68_op (A_FALSE, "XOR", m, genie_xor_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), MODE (INT), NULL);
  a68_op (A_TRUE, "SHL", m, genie_shl_long_mp);
  a68_op (A_TRUE, "UP", m, genie_shl_long_mp);
  a68_op (A_TRUE, "SHR", m, genie_shr_long_mp);
  a68_op (A_TRUE, "DOWN", m, genie_shr_long_mp);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (LONG_BITS), NULL);
  a68_op (A_TRUE, "ELEM", m, genie_elem_long_bits);
/* LONG LONG BITS. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_BITS), NULL);
  a68_op (A_TRUE, "ABS", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "BIN", m, genie_bin_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A_TRUE, "NOT", m, genie_not_long_mp);
  a68_op (A_TRUE, "~", m, genie_not_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A_TRUE, "AND", m, genie_and_long_mp);
  a68_op (A_TRUE, "&", m, genie_and_long_mp);
  a68_op (A_TRUE, "OR", m, genie_or_long_mp);
  a68_op (A_FALSE, "XOR", m, genie_xor_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), MODE (INT), NULL);
  a68_op (A_TRUE, "SHL", m, genie_shl_long_mp);
  a68_op (A_TRUE, "UP", m, genie_shl_long_mp);
  a68_op (A_TRUE, "SHR", m, genie_shr_long_mp);
  a68_op (A_TRUE, "DOWN", m, genie_shr_long_mp);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (LONGLONG_BITS), NULL);
  a68_op (A_TRUE, "ELEM", m, genie_elem_longlong_bits);
/* BYTES ops. */
  m = a68_proc (MODE (BYTES), MODE (STRING), NULL);
  a68_idf (A_TRUE, "bytespack", m, genie_bytespack);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (BYTES), NULL);
  a68_op (A_TRUE, "ELEM", m, genie_elem_bytes);
  m = a68_proc (MODE (BYTES), MODE (BYTES), MODE (BYTES), NULL);
  a68_op (A_TRUE, "+", m, genie_add_bytes);
  m = a68_proc (MODE (REF_BYTES), MODE (REF_BYTES), MODE (BYTES), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_bytes);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_bytes);
  m = a68_proc (MODE (REF_BYTES), MODE (BYTES), MODE (REF_BYTES), NULL);
  a68_op (A_TRUE, "+=:", m, genie_plusto_bytes);
  a68_op (A_TRUE, "PLUSTO", m, genie_plusto_bytes);
  m = a68_proc (MODE (BOOL), MODE (BYTES), MODE (BYTES), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_bytes);
  a68_op (A_TRUE, "/=", m, genie_ne_bytes);
  a68_op (A_TRUE, "~=", m, genie_ne_bytes);
  a68_op (A_TRUE, "^=", m, genie_ne_bytes);
  a68_op (A_TRUE, "<", m, genie_lt_bytes);
  a68_op (A_TRUE, "<=", m, genie_le_bytes);
  a68_op (A_TRUE, ">", m, genie_gt_bytes);
  a68_op (A_TRUE, ">=", m, genie_ge_bytes);
  a68_op (A_TRUE, "EQ", m, genie_eq_bytes);
  a68_op (A_TRUE, "NE", m, genie_ne_bytes);
  a68_op (A_TRUE, "LT", m, genie_lt_bytes);
  a68_op (A_TRUE, "LE", m, genie_le_bytes);
  a68_op (A_TRUE, "GT", m, genie_gt_bytes);
  a68_op (A_TRUE, "GE", m, genie_ge_bytes);
/* LONG BYTES ops. */
  m = a68_proc (MODE (LONG_BYTES), MODE (BYTES), NULL);
  a68_op (A_TRUE, "LENG", m, genie_leng_bytes);
  m = a68_proc (MODE (BYTES), MODE (LONG_BYTES), NULL);
  a68_idf (A_TRUE, "SHORTEN", m, genie_shorten_bytes);
  m = a68_proc (MODE (LONG_BYTES), MODE (STRING), NULL);
  a68_idf (A_TRUE, "longbytespack", m, genie_long_bytespack);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (LONG_BYTES), NULL);
  a68_op (A_TRUE, "ELEM", m, genie_elem_long_bytes);
  m = a68_proc (MODE (LONG_BYTES), MODE (LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A_TRUE, "+", m, genie_add_long_bytes);
  m = a68_proc (MODE (REF_LONG_BYTES), MODE (REF_LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_long_bytes);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_long_bytes);
  m = a68_proc (MODE (REF_LONG_BYTES), MODE (LONG_BYTES), MODE (REF_LONG_BYTES), NULL);
  a68_op (A_TRUE, "+=:", m, genie_plusto_long_bytes);
  a68_op (A_TRUE, "PLUSTO", m, genie_plusto_long_bytes);
  m = a68_proc (MODE (BOOL), MODE (LONG_BYTES), MODE (LONG_BYTES), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_bytes);
  a68_op (A_TRUE, "/=", m, genie_ne_long_bytes);
  a68_op (A_TRUE, "~=", m, genie_ne_long_bytes);
  a68_op (A_TRUE, "^=", m, genie_ne_long_bytes);
  a68_op (A_TRUE, "<", m, genie_lt_long_bytes);
  a68_op (A_TRUE, "<=", m, genie_le_long_bytes);
  a68_op (A_TRUE, ">", m, genie_gt_long_bytes);
  a68_op (A_TRUE, ">=", m, genie_ge_long_bytes);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_bytes);
  a68_op (A_TRUE, "NE", m, genie_ne_long_bytes);
  a68_op (A_TRUE, "LT", m, genie_lt_long_bytes);
  a68_op (A_TRUE, "LE", m, genie_le_long_bytes);
  a68_op (A_TRUE, "GT", m, genie_gt_long_bytes);
  a68_op (A_TRUE, "GE", m, genie_ge_long_bytes);
/* STRING ops. */
  m = a68_proc (MODE (BOOL), MODE (STRING), MODE (STRING), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_string);
  a68_op (A_TRUE, "/=", m, genie_ne_string);
  a68_op (A_TRUE, "~=", m, genie_ne_string);
  a68_op (A_TRUE, "^=", m, genie_ne_string);
  a68_op (A_TRUE, "<", m, genie_lt_string);
  a68_op (A_TRUE, "<=", m, genie_le_string);
  a68_op (A_TRUE, ">=", m, genie_ge_string);
  a68_op (A_TRUE, ">", m, genie_gt_string);
  a68_op (A_TRUE, "EQ", m, genie_eq_string);
  a68_op (A_TRUE, "NE", m, genie_ne_string);
  a68_op (A_TRUE, "LT", m, genie_lt_string);
  a68_op (A_TRUE, "LE", m, genie_le_string);
  a68_op (A_TRUE, "GE", m, genie_ge_string);
  a68_op (A_TRUE, "GT", m, genie_gt_string);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (STRING), NULL);
  a68_op (A_TRUE, "ELEM", m, genie_elem_string);
  m = a68_proc (MODE (STRING), MODE (CHAR), MODE (CHAR), NULL);
  a68_op (A_TRUE, "+", m, genie_add_char);
  m = a68_proc (MODE (STRING), MODE (STRING), MODE (STRING), NULL);
  a68_op (A_TRUE, "+", m, genie_add_string);
  m = a68_proc (MODE (REF_STRING), MODE (REF_STRING), MODE (STRING), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_string);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_string);
  m = a68_proc (MODE (REF_STRING), MODE (REF_STRING), MODE (INT), NULL);
  a68_op (A_TRUE, "*:=", m, genie_timesab_string);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_string);
  m = a68_proc (MODE (REF_STRING), MODE (STRING), MODE (REF_STRING), NULL);
  a68_op (A_TRUE, "+=:", m, genie_plusto_string);
  a68_op (A_TRUE, "PLUSTO", m, genie_plusto_string);
  m = a68_proc (MODE (STRING), MODE (STRING), MODE (INT), NULL);
  a68_op (A_TRUE, "*", m, genie_times_string_int);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (STRING), NULL);
  a68_op (A_TRUE, "*", m, genie_times_int_string);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (CHAR), NULL);
  a68_op (A_TRUE, "*", m, genie_times_int_char);
  m = a68_proc (MODE (STRING), MODE (CHAR), MODE (INT), NULL);
  a68_op (A_TRUE, "*", m, genie_times_char_int);
/* [] CHAR as cross term for STRING. */
  m = a68_proc (MODE (BOOL), MODE (ROW_CHAR), MODE (ROW_CHAR), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_string);
  a68_op (A_TRUE, "/=", m, genie_ne_string);
  a68_op (A_TRUE, "~=", m, genie_ne_string);
  a68_op (A_TRUE, "^=", m, genie_ne_string);
  a68_op (A_TRUE, "<", m, genie_lt_string);
  a68_op (A_TRUE, "<=", m, genie_le_string);
  a68_op (A_TRUE, ">=", m, genie_ge_string);
  a68_op (A_TRUE, ">", m, genie_gt_string);
  a68_op (A_TRUE, "EQ", m, genie_eq_string);
  a68_op (A_TRUE, "NE", m, genie_ne_string);
  a68_op (A_TRUE, "LT", m, genie_lt_string);
  a68_op (A_TRUE, "LE", m, genie_le_string);
  a68_op (A_TRUE, "GE", m, genie_ge_string);
  a68_op (A_TRUE, "GT", m, genie_gt_string);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (ROW_CHAR), NULL);
  a68_op (A_TRUE, "ELEM", m, genie_elem_string);
  m = a68_proc (MODE (STRING), MODE (ROW_CHAR), MODE (ROW_CHAR), NULL);
  a68_op (A_TRUE, "+", m, genie_add_string);
  m = a68_proc (MODE (STRING), MODE (ROW_CHAR), MODE (INT), NULL);
  a68_op (A_TRUE, "*", m, genie_times_string_int);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (ROW_CHAR), NULL);
  a68_op (A_TRUE, "*", m, genie_times_int_string);
/* SEMA ops. */
  m = a68_proc (MODE (SEMA), MODE (INT), NULL);
  a68_op (A_TRUE, "LEVEL", m, genie_level_sema_int);
  m = a68_proc (MODE (INT), MODE (SEMA), NULL);
  a68_op (A_TRUE, "LEVEL", m, genie_level_int_sema);
  m = a68_proc (MODE (VOID), MODE (SEMA), NULL);
  a68_op (A_TRUE, "UP", m, genie_up_sema);
  a68_op (A_TRUE, "DOWN", m, genie_down_sema);
/* ROWS ops. */
  m = a68_proc (MODE (INT), MODE (ROWS), NULL);
  a68_op (A_FALSE, "ELEMS", m, genie_monad_elems);
  a68_op (A_TRUE, "LWB", m, genie_monad_lwb);
  a68_op (A_TRUE, "UPB", m, genie_monad_upb);
  m = a68_proc (MODE (INT), MODE (INT), MODE (ROWS), NULL);
  a68_op (A_FALSE, "ELEMS", m, genie_dyad_elems);
  a68_op (A_TRUE, "LWB", m, genie_dyad_lwb);
  a68_op (A_TRUE, "UPB", m, genie_dyad_upb);
/* Binding for the multiple-precision library. */
/* LONG INT. */
  m = a68_proc (MODE (LONG_INT), MODE (INT), NULL);
  a68_op (A_TRUE, "LENG", m, genie_lengthen_int_to_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_long_mp);
  a68_op (A_TRUE, "ABS", m, genie_abs_long_mp);
  m = a68_proc (MODE (INT), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_shorten_long_mp_to_int);
  a68_op (A_TRUE, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "ODD", m, genie_odd_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "ENTIER", m, genie_entier_long_mp);
  a68_op (A_TRUE, "ROUND", m, genie_round_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "+", m, genie_add_long_int);
  a68_op (A_TRUE, "-", m, genie_minus_long_int);
  a68_op (A_TRUE, "*", m, genie_mul_long_int);
  a68_op (A_TRUE, "OVER", m, genie_over_long_mp);
  a68_op (A_TRUE, "%", m, genie_over_long_mp);
  a68_op (A_TRUE, "MOD", m, genie_mod_long_mp);
  a68_op (A_TRUE, "%*", m, genie_mod_long_mp);
  m = a68_proc (MODE (REF_LONG_INT), MODE (REF_LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_long_int);
  a68_op (A_TRUE, "-:=", m, genie_minusab_long_int);
  a68_op (A_TRUE, "*:=", m, genie_timesab_long_int);
  a68_op (A_TRUE, "%:=", m, genie_overab_long_mp);
  a68_op (A_TRUE, "%*:=", m, genie_modab_long_mp);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_long_int);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_long_int);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_long_int);
  a68_op (A_TRUE, "OVERAB", m, genie_overab_long_mp);
  a68_op (A_TRUE, "MODAB", m, genie_modab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "/", m, genie_div_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), MODE (INT), NULL);
  a68_op (A_TRUE, "**", m, genie_pow_long_mp_int_int);
  a68_op (A_TRUE, "^", m, genie_pow_long_mp_int_int);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_INT), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "I", m, genie_idle);
  a68_op (A_TRUE, "+*", m, genie_idle);
/* LONG REAL. */
  m = a68_proc (MODE (LONG_REAL), MODE (REAL), NULL);
  a68_op (A_TRUE, "LENG", m, genie_lengthen_real_to_long_mp);
  m = a68_proc (MODE (REAL), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_shorten_long_mp_to_real);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_long_mp);
  a68_op (A_TRUE, "ABS", m, genie_abs_long_mp);
  a68_idf (A_TRUE, "longsqrt", m, genie_sqrt_long_mp);
  a68_idf (A_FALSE, "longcbrt", m, genie_curt_long_mp);
  a68_idf (A_FALSE, "longcurt", m, genie_curt_long_mp);
  a68_idf (A_TRUE, "longexp", m, genie_exp_long_mp);
  a68_idf (A_TRUE, "longln", m, genie_ln_long_mp);
  a68_idf (A_TRUE, "longlog", m, genie_log_long_mp);
  a68_idf (A_TRUE, "longsin", m, genie_sin_long_mp);
  a68_idf (A_TRUE, "longcos", m, genie_cos_long_mp);
  a68_idf (A_TRUE, "longtan", m, genie_tan_long_mp);
  a68_idf (A_TRUE, "longasin", m, genie_asin_long_mp);
  a68_idf (A_TRUE, "longacos", m, genie_acos_long_mp);
  a68_idf (A_TRUE, "longatan", m, genie_atan_long_mp);
  a68_idf (A_TRUE, "longarcsin", m, genie_asin_long_mp);
  a68_idf (A_TRUE, "longarccos", m, genie_acos_long_mp);
  a68_idf (A_TRUE, "longarctan", m, genie_atan_long_mp);
  a68_idf (A_FALSE, "longsinh", m, genie_sinh_long_mp);
  a68_idf (A_FALSE, "longcosh", m, genie_cosh_long_mp);
  a68_idf (A_FALSE, "longtanh", m, genie_tanh_long_mp);
  a68_idf (A_FALSE, "longasinh", m, genie_arcsinh_long_mp);
  a68_idf (A_FALSE, "longacosh", m, genie_arccosh_long_mp);
  a68_idf (A_FALSE, "longatanh", m, genie_arctanh_long_mp);
  a68_idf (A_FALSE, "longarcsinh", m, genie_arcsinh_long_mp);
  a68_idf (A_FALSE, "longarccosh", m, genie_arccosh_long_mp);
  a68_idf (A_FALSE, "longarctanh", m, genie_arctanh_long_mp);
  a68_idf (A_FALSE, "dsqrt", m, genie_sqrt_long_mp);
  a68_idf (A_FALSE, "dcbrt", m, genie_curt_long_mp);
  a68_idf (A_FALSE, "dcurt", m, genie_curt_long_mp);
  a68_idf (A_FALSE, "dexp", m, genie_exp_long_mp);
  a68_idf (A_FALSE, "dln", m, genie_ln_long_mp);
  a68_idf (A_FALSE, "dlog", m, genie_log_long_mp);
  a68_idf (A_FALSE, "dsin", m, genie_sin_long_mp);
  a68_idf (A_FALSE, "dcos", m, genie_cos_long_mp);
  a68_idf (A_FALSE, "dtan", m, genie_tan_long_mp);
  a68_idf (A_FALSE, "dasin", m, genie_asin_long_mp);
  a68_idf (A_FALSE, "dacos", m, genie_acos_long_mp);
  a68_idf (A_FALSE, "datan", m, genie_atan_long_mp);
  a68_idf (A_FALSE, "dsinh", m, genie_sinh_long_mp);
  a68_idf (A_FALSE, "dcosh", m, genie_cosh_long_mp);
  a68_idf (A_FALSE, "dtanh", m, genie_tanh_long_mp);
  a68_idf (A_FALSE, "dasinh", m, genie_arcsinh_long_mp);
  a68_idf (A_FALSE, "dacosh", m, genie_arccosh_long_mp);
  a68_idf (A_FALSE, "datanh", m, genie_arctanh_long_mp);
  m = a68_proc (MODE (INT), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "+", m, genie_add_long_mp);
  a68_op (A_TRUE, "-", m, genie_sub_long_mp);
  a68_op (A_TRUE, "*", m, genie_mul_long_mp);
  a68_op (A_TRUE, "/", m, genie_div_long_mp);
  a68_op (A_TRUE, "**", m, genie_pow_long_mp);
  a68_op (A_TRUE, "UP", m, genie_pow_long_mp);
  a68_op (A_TRUE, "^", m, genie_pow_long_mp);
  m = a68_proc (MODE (REF_LONG_REAL), MODE (REF_LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_long_mp);
  a68_op (A_TRUE, "-:=", m, genie_minusab_long_mp);
  a68_op (A_TRUE, "*:=", m, genie_timesab_long_mp);
  a68_op (A_TRUE, "/:=", m, genie_divab_long_mp);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_long_mp);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_long_mp);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_long_mp);
  a68_op (A_TRUE, "DIVAB", m, genie_divab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (INT), NULL);
  a68_op (A_TRUE, "**", m, genie_pow_long_mp_int);
  a68_op (A_TRUE, "UP", m, genie_pow_long_mp_int);
  a68_op (A_TRUE, "^", m, genie_pow_long_mp_int);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "I", m, genie_idle);
  a68_op (A_TRUE, "+*", m, genie_idle);
/* LONG COMPLEX. */
  m = a68_proc (MODE (LONG_COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A_TRUE, "LENG", m, genie_lengthen_complex_to_long_complex);
  m = a68_proc (MODE (COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_shorten_long_complex_to_complex);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_COMPLEX), NULL);
  a68_op (A_TRUE, "RE", m, genie_re_long_complex);
  a68_op (A_TRUE, "IM", m, genie_im_long_complex);
  a68_op (A_TRUE, "ARG", m, genie_arg_long_complex);
  a68_op (A_TRUE, "ABS", m, genie_abs_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_long_complex);
  a68_op (A_TRUE, "CONJ", m, genie_conj_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A_TRUE, "+", m, genie_add_long_complex);
  a68_op (A_TRUE, "-", m, genie_sub_long_complex);
  a68_op (A_TRUE, "*", m, genie_mul_long_complex);
  a68_op (A_TRUE, "/", m, genie_div_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), MODE (INT), NULL);
  a68_op (A_TRUE, "**", m, genie_pow_long_complex_int);
  a68_op (A_TRUE, "UP", m, genie_pow_long_complex_int);
  a68_op (A_TRUE, "^", m, genie_pow_long_complex_int);
  m = a68_proc (MODE (BOOL), MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_complex);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_complex);
  a68_op (A_TRUE, "/=", m, genie_ne_long_complex);
  a68_op (A_TRUE, "~=", m, genie_ne_long_complex);
  a68_op (A_TRUE, "NE", m, genie_ne_long_complex);
  m = a68_proc (MODE (REF_LONG_COMPLEX), MODE (REF_LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_long_complex);
  a68_op (A_TRUE, "-:=", m, genie_minusab_long_complex);
  a68_op (A_TRUE, "*:=", m, genie_timesab_long_complex);
  a68_op (A_TRUE, "/:=", m, genie_divab_long_complex);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_long_complex);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_long_complex);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_long_complex);
  a68_op (A_TRUE, "DIVAB", m, genie_divab_long_complex);
/* LONG LONG INT. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONG_INT), NULL);
  a68_op (A_TRUE, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_long_mp);
  a68_op (A_TRUE, "ABS", m, genie_abs_long_mp);
  m = a68_proc (MODE (INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "ODD", m, genie_odd_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_REAL), NULL);
  a68_op (A_TRUE, "ENTIER", m, genie_entier_long_mp);
  a68_op (A_TRUE, "ROUND", m, genie_round_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "+", m, genie_add_long_int);
  a68_op (A_TRUE, "-", m, genie_minus_long_int);
  a68_op (A_TRUE, "*", m, genie_mul_long_int);
  a68_op (A_TRUE, "OVER", m, genie_over_long_mp);
  a68_op (A_TRUE, "%", m, genie_over_long_mp);
  a68_op (A_TRUE, "MOD", m, genie_mod_long_mp);
  a68_op (A_TRUE, "%*", m, genie_mod_long_mp);
  m = a68_proc (MODE (REF_LONGLONG_INT), MODE (REF_LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_long_int);
  a68_op (A_TRUE, "-:=", m, genie_minusab_long_int);
  a68_op (A_TRUE, "*:=", m, genie_timesab_long_int);
  a68_op (A_TRUE, "%:=", m, genie_overab_long_mp);
  a68_op (A_TRUE, "%*:=", m, genie_modab_long_mp);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_long_int);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_long_int);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_long_int);
  a68_op (A_TRUE, "OVERAB", m, genie_overab_long_mp);
  a68_op (A_TRUE, "MODAB", m, genie_modab_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "/", m, genie_div_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), MODE (INT), NULL);
  a68_op (A_TRUE, "**", m, genie_pow_long_mp_int_int);
  a68_op (A_TRUE, "^", m, genie_pow_long_mp_int_int);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "I", m, genie_idle);
  a68_op (A_TRUE, "+*", m, genie_idle);
/* LONG LONG REAL. */
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONG_REAL), NULL);
  a68_op (A_TRUE, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_long_mp);
  a68_op (A_TRUE, "ABS", m, genie_abs_long_mp);
  a68_idf (A_TRUE, "longlongsqrt", m, genie_sqrt_long_mp);
  a68_idf (A_FALSE, "longlongcbrt", m, genie_curt_long_mp);
  a68_idf (A_FALSE, "longlongcurt", m, genie_curt_long_mp);
  a68_idf (A_TRUE, "longlongexp", m, genie_exp_long_mp);
  a68_idf (A_TRUE, "longlongln", m, genie_ln_long_mp);
  a68_idf (A_TRUE, "longlonglog", m, genie_log_long_mp);
  a68_idf (A_TRUE, "longlongsin", m, genie_sin_long_mp);
  a68_idf (A_TRUE, "longlongcos", m, genie_cos_long_mp);
  a68_idf (A_TRUE, "longlongtan", m, genie_tan_long_mp);
  a68_idf (A_TRUE, "longlongasin", m, genie_asin_long_mp);
  a68_idf (A_TRUE, "longlongacos", m, genie_acos_long_mp);
  a68_idf (A_TRUE, "longlongatan", m, genie_atan_long_mp);
  a68_idf (A_TRUE, "longlongarcsin", m, genie_asin_long_mp);
  a68_idf (A_TRUE, "longlongarccos", m, genie_acos_long_mp);
  a68_idf (A_TRUE, "longlongarctan", m, genie_atan_long_mp);
  a68_idf (A_FALSE, "longlongsinh", m, genie_sinh_long_mp);
  a68_idf (A_FALSE, "longlongcosh", m, genie_cosh_long_mp);
  a68_idf (A_FALSE, "longlongtanh", m, genie_tanh_long_mp);
  a68_idf (A_FALSE, "longlongasinh", m, genie_arcsinh_long_mp);
  a68_idf (A_FALSE, "longlongacosh", m, genie_arccosh_long_mp);
  a68_idf (A_FALSE, "longlongatanh", m, genie_arctanh_long_mp);
  a68_idf (A_FALSE, "longlongarcsinh", m, genie_arcsinh_long_mp);
  a68_idf (A_FALSE, "longlongarccosh", m, genie_arccosh_long_mp);
  a68_idf (A_FALSE, "longlongarctanh", m, genie_arctanh_long_mp);
  a68_idf (A_FALSE, "qsqrt", m, genie_sqrt_long_mp);
  a68_idf (A_FALSE, "qcbrt", m, genie_curt_long_mp);
  a68_idf (A_FALSE, "qcurt", m, genie_curt_long_mp);
  a68_idf (A_FALSE, "qexp", m, genie_exp_long_mp);
  a68_idf (A_FALSE, "qln", m, genie_ln_long_mp);
  a68_idf (A_FALSE, "qlog", m, genie_log_long_mp);
  a68_idf (A_FALSE, "qsin", m, genie_sin_long_mp);
  a68_idf (A_FALSE, "qcos", m, genie_cos_long_mp);
  a68_idf (A_FALSE, "qtan", m, genie_tan_long_mp);
  a68_idf (A_FALSE, "qasin", m, genie_asin_long_mp);
  a68_idf (A_FALSE, "qacos", m, genie_acos_long_mp);
  a68_idf (A_FALSE, "qatan", m, genie_atan_long_mp);
  a68_idf (A_FALSE, "qsinh", m, genie_sinh_long_mp);
  a68_idf (A_FALSE, "qcosh", m, genie_cosh_long_mp);
  a68_idf (A_FALSE, "qtanh", m, genie_tanh_long_mp);
  a68_idf (A_FALSE, "qasinh", m, genie_arcsinh_long_mp);
  a68_idf (A_FALSE, "qacosh", m, genie_arccosh_long_mp);
  a68_idf (A_FALSE, "qatanh", m, genie_arctanh_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A_TRUE, "+", m, genie_add_long_mp);
  a68_op (A_TRUE, "-", m, genie_sub_long_mp);
  a68_op (A_TRUE, "*", m, genie_mul_long_mp);
  a68_op (A_TRUE, "/", m, genie_div_long_mp);
  a68_op (A_TRUE, "**", m, genie_pow_long_mp);
  a68_op (A_TRUE, "UP", m, genie_pow_long_mp);
  a68_op (A_TRUE, "^", m, genie_pow_long_mp);
  m = a68_proc (MODE (REF_LONGLONG_REAL), MODE (REF_LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_long_mp);
  a68_op (A_TRUE, "-:=", m, genie_minusab_long_mp);
  a68_op (A_TRUE, "*:=", m, genie_timesab_long_mp);
  a68_op (A_TRUE, "/:=", m, genie_divab_long_mp);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_long_mp);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_long_mp);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_long_mp);
  a68_op (A_TRUE, "DIVAB", m, genie_divab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_mp);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_mp);
  a68_op (A_TRUE, "/=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "~=", m, genie_ne_long_mp);
  a68_op (A_TRUE, "NE", m, genie_ne_long_mp);
  a68_op (A_TRUE, "<", m, genie_lt_long_mp);
  a68_op (A_TRUE, "LT", m, genie_lt_long_mp);
  a68_op (A_TRUE, "<=", m, genie_le_long_mp);
  a68_op (A_TRUE, "LE", m, genie_le_long_mp);
  a68_op (A_TRUE, ">", m, genie_gt_long_mp);
  a68_op (A_TRUE, "GT", m, genie_gt_long_mp);
  a68_op (A_TRUE, ">=", m, genie_ge_long_mp);
  a68_op (A_TRUE, "GE", m, genie_ge_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (INT), NULL);
  a68_op (A_TRUE, "**", m, genie_pow_long_mp_int);
  a68_op (A_TRUE, "UP", m, genie_pow_long_mp_int);
  a68_op (A_TRUE, "^", m, genie_pow_long_mp_int);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A_TRUE, "I", m, genie_idle);
  a68_op (A_TRUE, "+*", m, genie_idle);
/* LONGLONG COMPLEX. */
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_op (A_TRUE, "LENG", m, genie_lengthen_long_complex_to_longlong_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_shorten_longlong_complex_to_long_complex);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A_TRUE, "RE", m, genie_re_long_complex);
  a68_op (A_TRUE, "IM", m, genie_im_long_complex);
  a68_op (A_TRUE, "ARG", m, genie_arg_long_complex);
  a68_op (A_TRUE, "ABS", m, genie_abs_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A_TRUE, "+", m, genie_idle);
  a68_op (A_TRUE, "-", m, genie_minus_long_complex);
  a68_op (A_TRUE, "CONJ", m, genie_conj_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A_TRUE, "+", m, genie_add_long_complex);
  a68_op (A_TRUE, "-", m, genie_sub_long_complex);
  a68_op (A_TRUE, "*", m, genie_mul_long_complex);
  a68_op (A_TRUE, "/", m, genie_div_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), MODE (INT), NULL);
  a68_op (A_TRUE, "**", m, genie_pow_long_complex_int);
  a68_op (A_TRUE, "UP", m, genie_pow_long_complex_int);
  a68_op (A_TRUE, "^", m, genie_pow_long_complex_int);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A_TRUE, "=", m, genie_eq_long_complex);
  a68_op (A_TRUE, "EQ", m, genie_eq_long_complex);
  a68_op (A_TRUE, "/=", m, genie_ne_long_complex);
  a68_op (A_TRUE, "~=", m, genie_ne_long_complex);
  a68_op (A_TRUE, "NE", m, genie_ne_long_complex);
  m = a68_proc (MODE (REF_LONGLONG_COMPLEX), MODE (REF_LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A_TRUE, "+:=", m, genie_plusab_long_complex);
  a68_op (A_TRUE, "-:=", m, genie_minusab_long_complex);
  a68_op (A_TRUE, "*:=", m, genie_timesab_long_complex);
  a68_op (A_TRUE, "/:=", m, genie_divab_long_complex);
  a68_op (A_TRUE, "PLUSAB", m, genie_plusab_long_complex);
  a68_op (A_TRUE, "MINUSAB", m, genie_minusab_long_complex);
  a68_op (A_TRUE, "TIMESAB", m, genie_timesab_long_complex);
  a68_op (A_TRUE, "DIVAB", m, genie_divab_long_complex);
/* Some "terminators" to handle the mapping of very short or very long modes.
   This allows you to write SHORT REAL z = SHORTEN pi while everything is
   silently mapped onto REAL. */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), NULL);
  a68_op (A_TRUE, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NULL);
  a68_op (A_TRUE, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_op (A_TRUE, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NULL);
  a68_op (A_TRUE, "LENG", m, genie_idle);
  m = a68_proc (MODE (INT), MODE (INT), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (REAL), MODE (REAL), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (BITS), MODE (BITS), NULL);
  a68_op (A_TRUE, "SHORTEN", m, genie_idle);
/* Vector and matrix. */
  m = a68_proc (MODE (VOID), MODE (REF_ROW_REAL), MODE (REAL), NULL);
  a68_idf (A_FALSE, "vectorset", m, genie_vector_set);
  m = a68_proc (MODE (VOID), MODE (REF_ROW_REAL), MODE (ROW_REAL), MODE (REAL), NULL);
  a68_idf (A_FALSE, "vectortimesscalar", m, genie_vector_times_scalar);
  m = a68_proc (MODE (VOID), MODE (REF_ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A_FALSE, "vectormove", m, genie_vector_move);
  m = a68_proc (MODE (VOID), MODE (REF_ROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A_FALSE, "vectorplus", m, genie_vector_add);
  a68_idf (A_FALSE, "vectorminus", m, genie_vector_sub);
  a68_idf (A_FALSE, "vectortimes", m, genie_vector_mul);
  a68_idf (A_FALSE, "vectordiv", m, genie_vector_div);
  m = a68_proc (MODE (REAL), MODE (ROW_REAL), MODE (ROW_REAL), NULL);
  a68_idf (A_FALSE, "vectorinnerproduct", m, genie_vector_inner_product);
  a68_idf (A_FALSE, "vectorinproduct", m, genie_vector_inner_product);
  m = proc_complex_complex;
  a68_idf (A_FALSE, "complexsqrt", m, genie_sqrt_complex);
  a68_idf (A_FALSE, "csqrt", m, genie_sqrt_complex);
  a68_idf (A_FALSE, "complexexp", m, genie_exp_complex);
  a68_idf (A_FALSE, "cexp", m, genie_exp_complex);
  a68_idf (A_FALSE, "complexln", m, genie_ln_complex);
  a68_idf (A_FALSE, "cln", m, genie_ln_complex);
  a68_idf (A_FALSE, "complexsin", m, genie_sin_complex);
  a68_idf (A_FALSE, "csin", m, genie_sin_complex);
  a68_idf (A_FALSE, "complexcos", m, genie_cos_complex);
  a68_idf (A_FALSE, "ccos", m, genie_cos_complex);
  a68_idf (A_FALSE, "complextan", m, genie_tan_complex);
  a68_idf (A_FALSE, "ctan", m, genie_tan_complex);
  a68_idf (A_FALSE, "complexasin", m, genie_arcsin_complex);
  a68_idf (A_FALSE, "casin", m, genie_arcsin_complex);
  a68_idf (A_FALSE, "complexacos", m, genie_arccos_complex);
  a68_idf (A_FALSE, "cacos", m, genie_arccos_complex);
  a68_idf (A_FALSE, "complexatan", m, genie_arctan_complex);
  a68_idf (A_FALSE, "catan", m, genie_arctan_complex);
  a68_idf (A_FALSE, "complexarcsin", m, genie_arcsin_complex);
  a68_idf (A_FALSE, "carcsin", m, genie_arcsin_complex);
  a68_idf (A_FALSE, "complexarccos", m, genie_arccos_complex);
  a68_idf (A_FALSE, "carccos", m, genie_arccos_complex);
  a68_idf (A_FALSE, "complexarctan", m, genie_arctan_complex);
  a68_idf (A_FALSE, "carctan", m, genie_arctan_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NULL);
  a68_idf (A_FALSE, "longcomplexsqrt", m, genie_sqrt_long_complex);
  a68_idf (A_FALSE, "dcsqrt", m, genie_sqrt_long_complex);
  a68_idf (A_FALSE, "longcomplexexp", m, genie_exp_long_complex);
  a68_idf (A_FALSE, "dcexp", m, genie_exp_long_complex);
  a68_idf (A_FALSE, "longcomplexln", m, genie_ln_long_complex);
  a68_idf (A_FALSE, "dcln", m, genie_ln_long_complex);
  a68_idf (A_FALSE, "longcomplexsin", m, genie_sin_long_complex);
  a68_idf (A_FALSE, "dcsin", m, genie_sin_long_complex);
  a68_idf (A_FALSE, "longcomplexcos", m, genie_cos_long_complex);
  a68_idf (A_FALSE, "dccos", m, genie_cos_long_complex);
  a68_idf (A_FALSE, "longcomplextan", m, genie_tan_long_complex);
  a68_idf (A_FALSE, "dctan", m, genie_tan_long_complex);
  a68_idf (A_FALSE, "longcomplexarcsin", m, genie_asin_long_complex);
  a68_idf (A_FALSE, "dcasin", m, genie_asin_long_complex);
  a68_idf (A_FALSE, "longcomplexarccos", m, genie_acos_long_complex);
  a68_idf (A_FALSE, "dcacos", m, genie_acos_long_complex);
  a68_idf (A_FALSE, "longcomplexarctan", m, genie_atan_long_complex);
  a68_idf (A_FALSE, "dcatan", m, genie_atan_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A_FALSE, "longlongcomplexsqrt", m, genie_sqrt_long_complex);
  a68_idf (A_FALSE, "qcsqrt", m, genie_sqrt_long_complex);
  a68_idf (A_FALSE, "longlongcomplexexp", m, genie_exp_long_complex);
  a68_idf (A_FALSE, "qcexp", m, genie_exp_long_complex);
  a68_idf (A_FALSE, "longlongcomplexln", m, genie_ln_long_complex);
  a68_idf (A_FALSE, "qcln", m, genie_ln_long_complex);
  a68_idf (A_FALSE, "longlongcomplexsin", m, genie_sin_long_complex);
  a68_idf (A_FALSE, "qcsin", m, genie_sin_long_complex);
  a68_idf (A_FALSE, "longlongcomplexcos", m, genie_cos_long_complex);
  a68_idf (A_FALSE, "qccos", m, genie_cos_long_complex);
  a68_idf (A_FALSE, "longlongcomplextan", m, genie_tan_long_complex);
  a68_idf (A_FALSE, "qctan", m, genie_tan_long_complex);
  a68_idf (A_FALSE, "longlongcomplexarcsin", m, genie_asin_long_complex);
  a68_idf (A_FALSE, "qcasin", m, genie_asin_long_complex);
  a68_idf (A_FALSE, "longlongcomplexarccos", m, genie_acos_long_complex);
  a68_idf (A_FALSE, "qcacos", m, genie_acos_long_complex);
  a68_idf (A_FALSE, "longlongcomplexarctan", m, genie_atan_long_complex);
  a68_idf (A_FALSE, "qcatan", m, genie_atan_long_complex);
}

static void stand_transput (void)
{
  a68_idf (A_TRUE, "errorchar", MODE (CHAR), genie_error_char);
  a68_idf (A_TRUE, "expchar", MODE (CHAR), genie_exp_char);
  a68_idf (A_TRUE, "flip", MODE (CHAR), genie_flip_char);
  a68_idf (A_TRUE, "flop", MODE (CHAR), genie_flop_char);
  a68_idf (A_FALSE, "blankcharacter", MODE (CHAR), genie_blank_char);
  a68_idf (A_TRUE, "blankchar", MODE (CHAR), genie_blank_char);
  a68_idf (A_TRUE, "blank", MODE (CHAR), genie_blank_char);
  a68_idf (A_FALSE, "nullcharacter", MODE (CHAR), genie_null_char);
  a68_idf (A_TRUE, "nullchar", MODE (CHAR), genie_null_char);
  a68_idf (A_FALSE, "newlinecharacter", MODE (CHAR), genie_newline_char);
  a68_idf (A_FALSE, "newlinechar", MODE (CHAR), genie_newline_char);
  a68_idf (A_FALSE, "formfeedcharacter", MODE (CHAR), genie_formfeed_char);
  a68_idf (A_FALSE, "formfeedchar", MODE (CHAR), genie_formfeed_char);
  a68_idf (A_FALSE, "tabcharacter", MODE (CHAR), genie_tab_char);
  a68_idf (A_FALSE, "tabchar", MODE (CHAR), genie_tab_char);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), NULL);
  a68_idf (A_TRUE, "whole", m, genie_whole);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), NULL);
  a68_idf (A_TRUE, "fixed", m, genie_fixed);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), MODE (INT), NULL);
  a68_idf (A_TRUE, "float", m, genie_float);
  a68_idf (A_TRUE, "standin", MODE (REF_FILE), genie_stand_in);
  a68_idf (A_TRUE, "standout", MODE (REF_FILE), genie_stand_out);
  a68_idf (A_TRUE, "standback", MODE (REF_FILE), genie_stand_back);
  a68_idf (A_FALSE, "standerror", MODE (REF_FILE), genie_stand_error);
  a68_idf (A_TRUE, "standinchannel", MODE (CHANNEL), genie_stand_in_channel);
  a68_idf (A_TRUE, "standoutchannel", MODE (CHANNEL), genie_stand_out_channel);
  a68_idf (A_FALSE, "standdrawchannel", MODE (CHANNEL), genie_stand_draw_channel);
  a68_idf (A_TRUE, "standbackchannel", MODE (CHANNEL), genie_stand_back_channel);
  a68_idf (A_FALSE, "standerrorchannel", MODE (CHANNEL), genie_stand_error_channel);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A_TRUE, "maketerm", m, genie_make_term);
  m = a68_proc (MODE (BOOL), MODE (CHAR), MODE (REF_INT), MODE (STRING), NULL);
  a68_idf (A_TRUE, "charinstring", m, genie_char_in_string);
  a68_idf (A_FALSE, "lastcharinstring", m, genie_last_char_in_string);
  m = a68_proc (MODE (BOOL), MODE (STRING), MODE (REF_INT), MODE (STRING), NULL);
  a68_idf (A_FALSE, "stringinstring", m, genie_string_in_string);
  m = a68_proc (MODE (STRING), MODE (REF_FILE), NULL);
  a68_idf (A_FALSE, "idf", m, genie_idf);
  a68_idf (A_FALSE, "term", m, genie_term);
  m = a68_proc (MODE (STRING), NULL);
  a68_idf (A_FALSE, "programidf", m, genie_program_idf);
/* Event routines. */
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (PROC_REF_FILE_BOOL), NULL);
  a68_idf (A_TRUE, "onfileend", m, genie_on_file_end);
  a68_idf (A_TRUE, "onpageeend", m, genie_on_page_end);
  a68_idf (A_TRUE, "onlineend", m, genie_on_line_end);
  a68_idf (A_TRUE, "onlogicalfileend", m, genie_on_file_end);
  a68_idf (A_TRUE, "onphysicalfileend", m, genie_on_file_end);
  a68_idf (A_TRUE, "onformatend", m, genie_on_format_end);
  a68_idf (A_TRUE, "onformaterror", m, genie_on_format_error);
  a68_idf (A_TRUE, "onvalueerror", m, genie_on_value_error);
  a68_idf (A_TRUE, "onopenerror", m, genie_on_open_error);
  a68_idf (A_FALSE, "ontransputerror", m, genie_on_transput_error);
/* Enquiries on files. */
  a68_idf (A_TRUE, "putpossible", MODE (PROC_REF_FILE_BOOL), genie_put_possible);
  a68_idf (A_TRUE, "getpossible", MODE (PROC_REF_FILE_BOOL), genie_get_possible);
  a68_idf (A_TRUE, "binpossible", MODE (PROC_REF_FILE_BOOL), genie_bin_possible);
  a68_idf (A_TRUE, "setpossible", MODE (PROC_REF_FILE_BOOL), genie_set_possible);
  a68_idf (A_TRUE, "resetpossible", MODE (PROC_REF_FILE_BOOL), genie_reset_possible);
  a68_idf (A_FALSE, "drawpossible", MODE (PROC_REF_FILE_BOOL), genie_draw_possible);
  a68_idf (A_TRUE, "compressible", MODE (PROC_REF_FILE_BOOL), genie_compressible);
/* Handling of files. */
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), MODE (CHANNEL), NULL);
  a68_idf (A_TRUE, "open", m, genie_open);
  a68_idf (A_TRUE, "establish", m, genie_establish);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REF_STRING), NULL);
  a68_idf (A_TRUE, "associate", m, genie_associate);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (CHANNEL), NULL);
  a68_idf (A_TRUE, "create", m, genie_create);
  a68_idf (A_TRUE, "close", MODE (PROC_REF_FILE_VOID), genie_close);
  a68_idf (A_TRUE, "lock", MODE (PROC_REF_FILE_VOID), genie_lock);
  a68_idf (A_TRUE, "scratch", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A_TRUE, "erase", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A_TRUE, "reset", MODE (PROC_REF_FILE_VOID), genie_reset);
  a68_idf (A_TRUE, "scratch", MODE (PROC_REF_FILE_VOID), genie_erase);
  a68_idf (A_TRUE, "newline", MODE (PROC_REF_FILE_VOID), genie_new_line);
  a68_idf (A_TRUE, "newpage", MODE (PROC_REF_FILE_VOID), genie_new_page);
  a68_idf (A_TRUE, "space", MODE (PROC_REF_FILE_VOID), genie_space);
  m = a68_proc (MODE (VOID), MODE (ROW_SIMPLIN), NULL);
  a68_idf (A_TRUE, "read", m, genie_read);
  a68_idf (A_TRUE, "readbin", m, genie_read_bin);
  a68_idf (A_TRUE, "readf", m, genie_read_format);
  m = a68_proc (MODE (VOID), MODE (ROW_SIMPLOUT), NULL);
  a68_idf (A_TRUE, "print", m, genie_write);
  a68_idf (A_TRUE, "write", m, genie_write);
  a68_idf (A_TRUE, "printbin", m, genie_write_bin);
  a68_idf (A_TRUE, "writebin", m, genie_write_bin);
  a68_idf (A_TRUE, "printf", m, genie_write_format);
  a68_idf (A_TRUE, "writef", m, genie_write_format);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_SIMPLIN), NULL);
  a68_idf (A_TRUE, "get", m, genie_read_file);
  a68_idf (A_TRUE, "getf", m, genie_read_file_format);
  a68_idf (A_TRUE, "getbin", m, genie_read_bin_file);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_SIMPLOUT), NULL);
  a68_idf (A_TRUE, "put", m, genie_write_file);
  a68_idf (A_TRUE, "putf", m, genie_write_file_format);
  a68_idf (A_TRUE, "putbin", m, genie_write_bin_file);
/* ALGOL68C type procs. */
  m = proc_int;
  a68_idf (A_FALSE, "readint", m, genie_read_int);
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A_FALSE, "printint", m, genie_print_int);
  m = a68_proc (MODE (LONG_INT), NULL);
  a68_idf (A_FALSE, "readlongint", m, genie_read_long_int);
  m = a68_proc (MODE (VOID), MODE (LONG_INT), NULL);
  a68_idf (A_FALSE, "printlongint", m, genie_print_long_int);
  m = a68_proc (MODE (LONGLONG_INT), NULL);
  a68_idf (A_FALSE, "readlonglongint", m, genie_read_longlong_int);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_INT), NULL);
  a68_idf (A_FALSE, "printlonglongint", m, genie_print_longlong_int);
  m = proc_real;
  a68_idf (A_FALSE, "readreal", m, genie_read_real);
  m = a68_proc (MODE (VOID), MODE (REAL), NULL);
  a68_idf (A_FALSE, "printreal", m, genie_print_real);
  m = a68_proc (MODE (LONG_REAL), NULL);
  a68_idf (A_FALSE, "readlongreal", m, genie_read_long_real);
  a68_idf (A_FALSE, "readdouble", m, genie_read_long_real);
  m = a68_proc (MODE (VOID), MODE (LONG_REAL), NULL);
  a68_idf (A_FALSE, "printlongreal", m, genie_print_long_real);
  a68_idf (A_FALSE, "printdouble", m, genie_print_long_real);
  m = a68_proc (MODE (LONGLONG_REAL), NULL);
  a68_idf (A_FALSE, "readlonglongreal", m, genie_read_longlong_real);
  a68_idf (A_FALSE, "readquad", m, genie_read_longlong_real);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_REAL), NULL);
  a68_idf (A_FALSE, "printlonglongreal", m, genie_print_longlong_real);
  a68_idf (A_FALSE, "printquad", m, genie_print_longlong_real);
  m = a68_proc (MODE (COMPLEX), NULL);
  a68_idf (A_FALSE, "readcompl", m, genie_read_complex);
  a68_idf (A_FALSE, "readcomplex", m, genie_read_complex);
  m = a68_proc (MODE (VOID), MODE (COMPLEX), NULL);
  a68_idf (A_FALSE, "printcompl", m, genie_print_complex);
  a68_idf (A_FALSE, "printcomplex", m, genie_print_complex);
  m = a68_proc (MODE (LONG_COMPLEX), NULL);
  a68_idf (A_FALSE, "readlongcompl", m, genie_read_long_complex);
  a68_idf (A_FALSE, "readlongcomplex", m, genie_read_long_complex);
  m = a68_proc (MODE (VOID), MODE (LONG_COMPLEX), NULL);
  a68_idf (A_FALSE, "printlongcompl", m, genie_print_long_complex);
  a68_idf (A_FALSE, "printlongcomplex", m, genie_print_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A_FALSE, "readlonglongcompl", m, genie_read_longlong_complex);
  a68_idf (A_FALSE, "readlonglongcomplex", m, genie_read_longlong_complex);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_COMPLEX), NULL);
  a68_idf (A_FALSE, "printlonglongcompl", m, genie_print_longlong_complex);
  a68_idf (A_FALSE, "printlonglongcomplex", m, genie_print_longlong_complex);
  m = proc_bool;
  a68_idf (A_FALSE, "readbool", m, genie_read_bool);
  m = a68_proc (MODE (VOID), MODE (BOOL), NULL);
  a68_idf (A_FALSE, "printbool", m, genie_print_bool);
  m = a68_proc (MODE (BITS), NULL);
  a68_idf (A_FALSE, "readbits", m, genie_read_bits);
  m = a68_proc (MODE (LONG_BITS), NULL);
  a68_idf (A_FALSE, "readlongbits", m, genie_read_long_bits);
  m = a68_proc (MODE (LONGLONG_BITS), NULL);
  a68_idf (A_FALSE, "readlonglongbits", m, genie_read_longlong_bits);
  m = a68_proc (MODE (VOID), MODE (BITS), NULL);
  a68_idf (A_FALSE, "printbits", m, genie_print_bits);
  m = a68_proc (MODE (VOID), MODE (LONG_BITS), NULL);
  a68_idf (A_FALSE, "printlongbits", m, genie_print_long_bits);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_BITS), NULL);
  a68_idf (A_FALSE, "printlonglongbits", m, genie_print_longlong_bits);
  m = proc_char;
  a68_idf (A_FALSE, "readchar", m, genie_read_char);
  m = a68_proc (MODE (VOID), MODE (CHAR), NULL);
  a68_idf (A_FALSE, "printchar", m, genie_print_char);
  a68_idf (A_FALSE, "readstring", MODE (PROC_STRING), genie_read_string);
  m = a68_proc (MODE (VOID), MODE (STRING), NULL);
  a68_idf (A_FALSE, "printstring", m, genie_print_string);
}

static void stand_extensions (void)
{
#ifdef HAVE_PLOTUTILS
/* Drawing. */
  m = a68_proc (MODE (BOOL), MODE (REF_FILE), MODE (STRING), MODE (STRING), NULL);
  a68_idf (A_FALSE, "drawdevice", m, genie_make_device);
  a68_idf (A_FALSE, "makedevice", m, genie_make_device);
  m = a68_proc (MODE (REAL), MODE (REF_FILE), NULL);
  a68_idf (A_FALSE, "drawaspect", m, genie_draw_aspect);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), NULL);
  a68_idf (A_FALSE, "drawclear", m, genie_draw_clear);
  a68_idf (A_FALSE, "drawerase", m, genie_draw_clear);
  a68_idf (A_FALSE, "drawflush", m, genie_draw_show);
  a68_idf (A_FALSE, "drawshow", m, genie_draw_show);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A_FALSE, "drawfillstyle", m, genie_draw_filltype);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A_FALSE, "drawgetcolourname", m, genie_draw_get_colour_name);
  a68_idf (A_FALSE, "drawgetcolorname", m, genie_draw_get_colour_name);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A_FALSE, "drawcolor", m, genie_draw_colour);
  a68_idf (A_FALSE, "drawcolour", m, genie_draw_colour);
  a68_idf (A_FALSE, "drawbackgroundcolor", m, genie_draw_background_colour);
  a68_idf (A_FALSE, "drawbackgroundcolour", m, genie_draw_background_colour);
  a68_idf (A_FALSE, "drawcircle", m, genie_draw_circle);
  a68_idf (A_FALSE, "drawball", m, genie_draw_atom);
  a68_idf (A_FALSE, "drawstar", m, genie_draw_star);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A_FALSE, "drawpoint", m, genie_draw_point);
  a68_idf (A_FALSE, "drawline", m, genie_draw_line);
  a68_idf (A_FALSE, "drawmove", m, genie_draw_move);
  a68_idf (A_FALSE, "drawrect", m, genie_draw_rect);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (CHAR), MODE (CHAR), MODE (ROW_CHAR), NULL);
  a68_idf (A_FALSE, "drawtext", m, genie_draw_text);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_CHAR), NULL);
  a68_idf (A_FALSE, "drawlinestyle", m, genie_draw_linestyle);
  a68_idf (A_FALSE, "drawfontname", m, genie_draw_fontname);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), NULL);
  a68_idf (A_FALSE, "drawlinewidth", m, genie_draw_linewidth);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A_FALSE, "drawfontsize", m, genie_draw_fontsize);
  a68_idf (A_FALSE, "drawtextangle", m, genie_draw_textangle);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A_FALSE, "drawcolorname", m, genie_draw_colour_name);
  a68_idf (A_FALSE, "drawcolourname", m, genie_draw_colour_name);
  a68_idf (A_FALSE, "drawbackgroundcolorname", m, genie_draw_background_colour_name);
  a68_idf (A_FALSE, "drawbackgroundcolourname", m, genie_draw_background_colour_name);
#endif
#ifdef HAVE_GSL
  a68_idf (A_FALSE, "cgsspeedoflight", MODE (REAL), genie_cgs_speed_of_light);
  a68_idf (A_FALSE, "cgsgravitationalconstant", MODE (REAL), genie_cgs_gravitational_constant);
  a68_idf (A_FALSE, "cgsplanckconstant", MODE (REAL), genie_cgs_planck_constant_h);
  a68_idf (A_FALSE, "cgsplanckconstantbar", MODE (REAL), genie_cgs_planck_constant_hbar);
  a68_idf (A_FALSE, "cgsastronomicalunit", MODE (REAL), genie_cgs_astronomical_unit);
  a68_idf (A_FALSE, "cgslightyear", MODE (REAL), genie_cgs_light_year);
  a68_idf (A_FALSE, "cgsparsec", MODE (REAL), genie_cgs_parsec);
  a68_idf (A_FALSE, "cgsgravaccel", MODE (REAL), genie_cgs_grav_accel);
  a68_idf (A_FALSE, "cgselectronvolt", MODE (REAL), genie_cgs_electron_volt);
  a68_idf (A_FALSE, "cgsmasselectron", MODE (REAL), genie_cgs_mass_electron);
  a68_idf (A_FALSE, "cgsmassmuon", MODE (REAL), genie_cgs_mass_muon);
  a68_idf (A_FALSE, "cgsmassproton", MODE (REAL), genie_cgs_mass_proton);
  a68_idf (A_FALSE, "cgsmassneutron", MODE (REAL), genie_cgs_mass_neutron);
  a68_idf (A_FALSE, "cgsrydberg", MODE (REAL), genie_cgs_rydberg);
  a68_idf (A_FALSE, "cgsboltzmann", MODE (REAL), genie_cgs_boltzmann);
  a68_idf (A_FALSE, "cgsbohrmagneton", MODE (REAL), genie_cgs_bohr_magneton);
  a68_idf (A_FALSE, "cgsnuclearmagneton", MODE (REAL), genie_cgs_nuclear_magneton);
  a68_idf (A_FALSE, "cgselectronmagneticmoment", MODE (REAL), genie_cgs_electron_magnetic_moment);
  a68_idf (A_FALSE, "cgsprotonmagneticmoment", MODE (REAL), genie_cgs_proton_magnetic_moment);
  a68_idf (A_FALSE, "cgsmolargas", MODE (REAL), genie_cgs_molar_gas);
  a68_idf (A_FALSE, "cgsstandardgasvolume", MODE (REAL), genie_cgs_standard_gas_volume);
  a68_idf (A_FALSE, "cgsminute", MODE (REAL), genie_cgs_minute);
  a68_idf (A_FALSE, "cgshour", MODE (REAL), genie_cgs_hour);
  a68_idf (A_FALSE, "cgsday", MODE (REAL), genie_cgs_day);
  a68_idf (A_FALSE, "cgsweek", MODE (REAL), genie_cgs_week);
  a68_idf (A_FALSE, "cgsinch", MODE (REAL), genie_cgs_inch);
  a68_idf (A_FALSE, "cgsfoot", MODE (REAL), genie_cgs_foot);
  a68_idf (A_FALSE, "cgsyard", MODE (REAL), genie_cgs_yard);
  a68_idf (A_FALSE, "cgsmile", MODE (REAL), genie_cgs_mile);
  a68_idf (A_FALSE, "cgsnauticalmile", MODE (REAL), genie_cgs_nautical_mile);
  a68_idf (A_FALSE, "cgsfathom", MODE (REAL), genie_cgs_fathom);
  a68_idf (A_FALSE, "cgsmil", MODE (REAL), genie_cgs_mil);
  a68_idf (A_FALSE, "cgspoint", MODE (REAL), genie_cgs_point);
  a68_idf (A_FALSE, "cgstexpoint", MODE (REAL), genie_cgs_texpoint);
  a68_idf (A_FALSE, "cgsmicron", MODE (REAL), genie_cgs_micron);
  a68_idf (A_FALSE, "cgsangstrom", MODE (REAL), genie_cgs_angstrom);
  a68_idf (A_FALSE, "cgshectare", MODE (REAL), genie_cgs_hectare);
  a68_idf (A_FALSE, "cgsacre", MODE (REAL), genie_cgs_acre);
  a68_idf (A_FALSE, "cgsbarn", MODE (REAL), genie_cgs_barn);
  a68_idf (A_FALSE, "cgsliter", MODE (REAL), genie_cgs_liter);
  a68_idf (A_FALSE, "cgsusgallon", MODE (REAL), genie_cgs_us_gallon);
  a68_idf (A_FALSE, "cgsquart", MODE (REAL), genie_cgs_quart);
  a68_idf (A_FALSE, "cgspint", MODE (REAL), genie_cgs_pint);
  a68_idf (A_FALSE, "cgscup", MODE (REAL), genie_cgs_cup);
  a68_idf (A_FALSE, "cgsfluidounce", MODE (REAL), genie_cgs_fluid_ounce);
  a68_idf (A_FALSE, "cgstablespoon", MODE (REAL), genie_cgs_tablespoon);
  a68_idf (A_FALSE, "cgsteaspoon", MODE (REAL), genie_cgs_teaspoon);
  a68_idf (A_FALSE, "cgscanadiangallon", MODE (REAL), genie_cgs_canadian_gallon);
  a68_idf (A_FALSE, "cgsukgallon", MODE (REAL), genie_cgs_uk_gallon);
  a68_idf (A_FALSE, "cgsmilesperhour", MODE (REAL), genie_cgs_miles_per_hour);
  a68_idf (A_FALSE, "cgskilometersperhour", MODE (REAL), genie_cgs_kilometers_per_hour);
  a68_idf (A_FALSE, "cgsknot", MODE (REAL), genie_cgs_knot);
  a68_idf (A_FALSE, "cgspoundmass", MODE (REAL), genie_cgs_pound_mass);
  a68_idf (A_FALSE, "cgsouncemass", MODE (REAL), genie_cgs_ounce_mass);
  a68_idf (A_FALSE, "cgston", MODE (REAL), genie_cgs_ton);
  a68_idf (A_FALSE, "cgsmetricton", MODE (REAL), genie_cgs_metric_ton);
  a68_idf (A_FALSE, "cgsukton", MODE (REAL), genie_cgs_uk_ton);
  a68_idf (A_FALSE, "cgstroyounce", MODE (REAL), genie_cgs_troy_ounce);
  a68_idf (A_FALSE, "cgscarat", MODE (REAL), genie_cgs_carat);
  a68_idf (A_FALSE, "cgsunifiedatomicmass", MODE (REAL), genie_cgs_unified_atomic_mass);
  a68_idf (A_FALSE, "cgsgramforce", MODE (REAL), genie_cgs_gram_force);
  a68_idf (A_FALSE, "cgspoundforce", MODE (REAL), genie_cgs_pound_force);
  a68_idf (A_FALSE, "cgskilopoundforce", MODE (REAL), genie_cgs_kilopound_force);
  a68_idf (A_FALSE, "cgspoundal", MODE (REAL), genie_cgs_poundal);
  a68_idf (A_FALSE, "cgscalorie", MODE (REAL), genie_cgs_calorie);
  a68_idf (A_FALSE, "cgsbtu", MODE (REAL), genie_cgs_btu);
  a68_idf (A_FALSE, "cgstherm", MODE (REAL), genie_cgs_therm);
  a68_idf (A_FALSE, "cgshorsepower", MODE (REAL), genie_cgs_horsepower);
  a68_idf (A_FALSE, "cgsbar", MODE (REAL), genie_cgs_bar);
  a68_idf (A_FALSE, "cgsstdatmosphere", MODE (REAL), genie_cgs_std_atmosphere);
  a68_idf (A_FALSE, "cgstorr", MODE (REAL), genie_cgs_torr);
  a68_idf (A_FALSE, "cgsmeterofmercury", MODE (REAL), genie_cgs_meter_of_mercury);
  a68_idf (A_FALSE, "cgsinchofmercury", MODE (REAL), genie_cgs_inch_of_mercury);
  a68_idf (A_FALSE, "cgsinchofwater", MODE (REAL), genie_cgs_inch_of_water);
  a68_idf (A_FALSE, "cgspsi", MODE (REAL), genie_cgs_psi);
  a68_idf (A_FALSE, "cgspoise", MODE (REAL), genie_cgs_poise);
  a68_idf (A_FALSE, "cgsstokes", MODE (REAL), genie_cgs_stokes);
  a68_idf (A_FALSE, "cgsfaraday", MODE (REAL), genie_cgs_faraday);
  a68_idf (A_FALSE, "cgselectroncharge", MODE (REAL), genie_cgs_electron_charge);
  a68_idf (A_FALSE, "cgsgauss", MODE (REAL), genie_cgs_gauss);
  a68_idf (A_FALSE, "cgsstilb", MODE (REAL), genie_cgs_stilb);
  a68_idf (A_FALSE, "cgslumen", MODE (REAL), genie_cgs_lumen);
  a68_idf (A_FALSE, "cgslux", MODE (REAL), genie_cgs_lux);
  a68_idf (A_FALSE, "cgsphot", MODE (REAL), genie_cgs_phot);
  a68_idf (A_FALSE, "cgsfootcandle", MODE (REAL), genie_cgs_footcandle);
  a68_idf (A_FALSE, "cgslambert", MODE (REAL), genie_cgs_lambert);
  a68_idf (A_FALSE, "cgsfootlambert", MODE (REAL), genie_cgs_footlambert);
  a68_idf (A_FALSE, "cgscurie", MODE (REAL), genie_cgs_curie);
  a68_idf (A_FALSE, "cgsroentgen", MODE (REAL), genie_cgs_roentgen);
  a68_idf (A_FALSE, "cgsrad", MODE (REAL), genie_cgs_rad);
  a68_idf (A_FALSE, "cgssolarmass", MODE (REAL), genie_cgs_solar_mass);
  a68_idf (A_FALSE, "cgsbohrradius", MODE (REAL), genie_cgs_bohr_radius);
  a68_idf (A_FALSE, "cgsnewton", MODE (REAL), genie_cgs_newton);
  a68_idf (A_FALSE, "cgsdyne", MODE (REAL), genie_cgs_dyne);
  a68_idf (A_FALSE, "cgsjoule", MODE (REAL), genie_cgs_joule);
  a68_idf (A_FALSE, "cgserg", MODE (REAL), genie_cgs_erg);
  a68_idf (A_FALSE, "mksaspeedoflight", MODE (REAL), genie_mks_speed_of_light);
  a68_idf (A_FALSE, "mksagravitationalconstant", MODE (REAL), genie_mks_gravitational_constant);
  a68_idf (A_FALSE, "mksaplanckconstant", MODE (REAL), genie_mks_planck_constant_h);
  a68_idf (A_FALSE, "mksaplanckconstantbar", MODE (REAL), genie_mks_planck_constant_hbar);
  a68_idf (A_FALSE, "mksavacuumpermeability", MODE (REAL), genie_mks_vacuum_permeability);
  a68_idf (A_FALSE, "mksaastronomicalunit", MODE (REAL), genie_mks_astronomical_unit);
  a68_idf (A_FALSE, "mksalightyear", MODE (REAL), genie_mks_light_year);
  a68_idf (A_FALSE, "mksaparsec", MODE (REAL), genie_mks_parsec);
  a68_idf (A_FALSE, "mksagravaccel", MODE (REAL), genie_mks_grav_accel);
  a68_idf (A_FALSE, "mksaelectronvolt", MODE (REAL), genie_mks_electron_volt);
  a68_idf (A_FALSE, "mksamasselectron", MODE (REAL), genie_mks_mass_electron);
  a68_idf (A_FALSE, "mksamassmuon", MODE (REAL), genie_mks_mass_muon);
  a68_idf (A_FALSE, "mksamassproton", MODE (REAL), genie_mks_mass_proton);
  a68_idf (A_FALSE, "mksamassneutron", MODE (REAL), genie_mks_mass_neutron);
  a68_idf (A_FALSE, "mksarydberg", MODE (REAL), genie_mks_rydberg);
  a68_idf (A_FALSE, "mksaboltzmann", MODE (REAL), genie_mks_boltzmann);
  a68_idf (A_FALSE, "mksabohrmagneton", MODE (REAL), genie_mks_bohr_magneton);
  a68_idf (A_FALSE, "mksanuclearmagneton", MODE (REAL), genie_mks_nuclear_magneton);
  a68_idf (A_FALSE, "mksaelectronmagneticmoment", MODE (REAL), genie_mks_electron_magnetic_moment);
  a68_idf (A_FALSE, "mksaprotonmagneticmoment", MODE (REAL), genie_mks_proton_magnetic_moment);
  a68_idf (A_FALSE, "mksamolargas", MODE (REAL), genie_mks_molar_gas);
  a68_idf (A_FALSE, "mksastandardgasvolume", MODE (REAL), genie_mks_standard_gas_volume);
  a68_idf (A_FALSE, "mksaminute", MODE (REAL), genie_mks_minute);
  a68_idf (A_FALSE, "mksahour", MODE (REAL), genie_mks_hour);
  a68_idf (A_FALSE, "mksaday", MODE (REAL), genie_mks_day);
  a68_idf (A_FALSE, "mksaweek", MODE (REAL), genie_mks_week);
  a68_idf (A_FALSE, "mksainch", MODE (REAL), genie_mks_inch);
  a68_idf (A_FALSE, "mksafoot", MODE (REAL), genie_mks_foot);
  a68_idf (A_FALSE, "mksayard", MODE (REAL), genie_mks_yard);
  a68_idf (A_FALSE, "mksamile", MODE (REAL), genie_mks_mile);
  a68_idf (A_FALSE, "mksanauticalmile", MODE (REAL), genie_mks_nautical_mile);
  a68_idf (A_FALSE, "mksafathom", MODE (REAL), genie_mks_fathom);
  a68_idf (A_FALSE, "mksamil", MODE (REAL), genie_mks_mil);
  a68_idf (A_FALSE, "mksapoint", MODE (REAL), genie_mks_point);
  a68_idf (A_FALSE, "mksatexpoint", MODE (REAL), genie_mks_texpoint);
  a68_idf (A_FALSE, "mksamicron", MODE (REAL), genie_mks_micron);
  a68_idf (A_FALSE, "mksaangstrom", MODE (REAL), genie_mks_angstrom);
  a68_idf (A_FALSE, "mksahectare", MODE (REAL), genie_mks_hectare);
  a68_idf (A_FALSE, "mksaacre", MODE (REAL), genie_mks_acre);
  a68_idf (A_FALSE, "mksabarn", MODE (REAL), genie_mks_barn);
  a68_idf (A_FALSE, "mksaliter", MODE (REAL), genie_mks_liter);
  a68_idf (A_FALSE, "mksausgallon", MODE (REAL), genie_mks_us_gallon);
  a68_idf (A_FALSE, "mksaquart", MODE (REAL), genie_mks_quart);
  a68_idf (A_FALSE, "mksapint", MODE (REAL), genie_mks_pint);
  a68_idf (A_FALSE, "mksacup", MODE (REAL), genie_mks_cup);
  a68_idf (A_FALSE, "mksafluidounce", MODE (REAL), genie_mks_fluid_ounce);
  a68_idf (A_FALSE, "mksatablespoon", MODE (REAL), genie_mks_tablespoon);
  a68_idf (A_FALSE, "mksateaspoon", MODE (REAL), genie_mks_teaspoon);
  a68_idf (A_FALSE, "mksacanadiangallon", MODE (REAL), genie_mks_canadian_gallon);
  a68_idf (A_FALSE, "mksaukgallon", MODE (REAL), genie_mks_uk_gallon);
  a68_idf (A_FALSE, "mksamilesperhour", MODE (REAL), genie_mks_miles_per_hour);
  a68_idf (A_FALSE, "mksakilometersperhour", MODE (REAL), genie_mks_kilometers_per_hour);
  a68_idf (A_FALSE, "mksaknot", MODE (REAL), genie_mks_knot);
  a68_idf (A_FALSE, "mksapoundmass", MODE (REAL), genie_mks_pound_mass);
  a68_idf (A_FALSE, "mksaouncemass", MODE (REAL), genie_mks_ounce_mass);
  a68_idf (A_FALSE, "mksaton", MODE (REAL), genie_mks_ton);
  a68_idf (A_FALSE, "mksametricton", MODE (REAL), genie_mks_metric_ton);
  a68_idf (A_FALSE, "mksaukton", MODE (REAL), genie_mks_uk_ton);
  a68_idf (A_FALSE, "mksatroyounce", MODE (REAL), genie_mks_troy_ounce);
  a68_idf (A_FALSE, "mksacarat", MODE (REAL), genie_mks_carat);
  a68_idf (A_FALSE, "mksaunifiedatomicmass", MODE (REAL), genie_mks_unified_atomic_mass);
  a68_idf (A_FALSE, "mksagramforce", MODE (REAL), genie_mks_gram_force);
  a68_idf (A_FALSE, "mksapoundforce", MODE (REAL), genie_mks_pound_force);
  a68_idf (A_FALSE, "mksakilopoundforce", MODE (REAL), genie_mks_kilopound_force);
  a68_idf (A_FALSE, "mksapoundal", MODE (REAL), genie_mks_poundal);
  a68_idf (A_FALSE, "mksacalorie", MODE (REAL), genie_mks_calorie);
  a68_idf (A_FALSE, "mksabtu", MODE (REAL), genie_mks_btu);
  a68_idf (A_FALSE, "mksatherm", MODE (REAL), genie_mks_therm);
  a68_idf (A_FALSE, "mksahorsepower", MODE (REAL), genie_mks_horsepower);
  a68_idf (A_FALSE, "mksabar", MODE (REAL), genie_mks_bar);
  a68_idf (A_FALSE, "mksastdatmosphere", MODE (REAL), genie_mks_std_atmosphere);
  a68_idf (A_FALSE, "mksatorr", MODE (REAL), genie_mks_torr);
  a68_idf (A_FALSE, "mksameterofmercury", MODE (REAL), genie_mks_meter_of_mercury);
  a68_idf (A_FALSE, "mksainchofmercury", MODE (REAL), genie_mks_inch_of_mercury);
  a68_idf (A_FALSE, "mksainchofwater", MODE (REAL), genie_mks_inch_of_water);
  a68_idf (A_FALSE, "mksapsi", MODE (REAL), genie_mks_psi);
  a68_idf (A_FALSE, "mksapoise", MODE (REAL), genie_mks_poise);
  a68_idf (A_FALSE, "mksastokes", MODE (REAL), genie_mks_stokes);
  a68_idf (A_FALSE, "mksafaraday", MODE (REAL), genie_mks_faraday);
  a68_idf (A_FALSE, "mksaelectroncharge", MODE (REAL), genie_mks_electron_charge);
  a68_idf (A_FALSE, "mksagauss", MODE (REAL), genie_mks_gauss);
  a68_idf (A_FALSE, "mksastilb", MODE (REAL), genie_mks_stilb);
  a68_idf (A_FALSE, "mksalumen", MODE (REAL), genie_mks_lumen);
  a68_idf (A_FALSE, "mksalux", MODE (REAL), genie_mks_lux);
  a68_idf (A_FALSE, "mksaphot", MODE (REAL), genie_mks_phot);
  a68_idf (A_FALSE, "mksafootcandle", MODE (REAL), genie_mks_footcandle);
  a68_idf (A_FALSE, "mksalambert", MODE (REAL), genie_mks_lambert);
  a68_idf (A_FALSE, "mksafootlambert", MODE (REAL), genie_mks_footlambert);
  a68_idf (A_FALSE, "mksacurie", MODE (REAL), genie_mks_curie);
  a68_idf (A_FALSE, "mksaroentgen", MODE (REAL), genie_mks_roentgen);
  a68_idf (A_FALSE, "mksarad", MODE (REAL), genie_mks_rad);
  a68_idf (A_FALSE, "mksasolarmass", MODE (REAL), genie_mks_solar_mass);
  a68_idf (A_FALSE, "mksabohrradius", MODE (REAL), genie_mks_bohr_radius);
  a68_idf (A_FALSE, "mksavacuumpermittivity", MODE (REAL), genie_mks_vacuum_permittivity);
  a68_idf (A_FALSE, "mksanewton", MODE (REAL), genie_mks_newton);
  a68_idf (A_FALSE, "mksadyne", MODE (REAL), genie_mks_dyne);
  a68_idf (A_FALSE, "mksajoule", MODE (REAL), genie_mks_joule);
  a68_idf (A_FALSE, "mksaerg", MODE (REAL), genie_mks_erg);
  a68_idf (A_FALSE, "numfinestructure", MODE (REAL), genie_num_fine_structure);
  a68_idf (A_FALSE, "numavogadro", MODE (REAL), genie_num_avogadro);
  a68_idf (A_FALSE, "numyotta", MODE (REAL), genie_num_yotta);
  a68_idf (A_FALSE, "numzetta", MODE (REAL), genie_num_zetta);
  a68_idf (A_FALSE, "numexa", MODE (REAL), genie_num_exa);
  a68_idf (A_FALSE, "numpeta", MODE (REAL), genie_num_peta);
  a68_idf (A_FALSE, "numtera", MODE (REAL), genie_num_tera);
  a68_idf (A_FALSE, "numgiga", MODE (REAL), genie_num_giga);
  a68_idf (A_FALSE, "nummega", MODE (REAL), genie_num_mega);
  a68_idf (A_FALSE, "numkilo", MODE (REAL), genie_num_kilo);
  a68_idf (A_FALSE, "nummilli", MODE (REAL), genie_num_milli);
  a68_idf (A_FALSE, "nummicro", MODE (REAL), genie_num_micro);
  a68_idf (A_FALSE, "numnano", MODE (REAL), genie_num_nano);
  a68_idf (A_FALSE, "numpico", MODE (REAL), genie_num_pico);
  a68_idf (A_FALSE, "numfemto", MODE (REAL), genie_num_femto);
  a68_idf (A_FALSE, "numatto", MODE (REAL), genie_num_atto);
  a68_idf (A_FALSE, "numzepto", MODE (REAL), genie_num_zepto);
  a68_idf (A_FALSE, "numyocto", MODE (REAL), genie_num_yocto);
  m = proc_real_real;
  a68_idf (A_FALSE, "erf", m, genie_erf_real);
  a68_idf (A_FALSE, "erfc", m, genie_erfc_real);
  a68_idf (A_FALSE, "gamma", m, genie_gamma_real);
  a68_idf (A_FALSE, "lngamma", m, genie_lngamma_real);
  a68_idf (A_FALSE, "factorial", m, genie_factorial_real);
  a68_idf (A_FALSE, "airyai", m, genie_airy_ai_real);
  a68_idf (A_FALSE, "airybi", m, genie_airy_bi_real);
  a68_idf (A_FALSE, "airyaiderivative", m, genie_airy_ai_deriv_real);
  a68_idf (A_FALSE, "airybiderivative", m, genie_airy_bi_deriv_real);
  a68_idf (A_FALSE, "ellipticintegralk", m, genie_elliptic_integral_k_real);
  a68_idf (A_FALSE, "ellipticintegrale", m, genie_elliptic_integral_e_real);
  m = proc_real_real_real;
  a68_idf (A_FALSE, "beta", m, genie_beta_real);
  a68_idf (A_FALSE, "besseljn", m, genie_bessel_jn_real);
  a68_idf (A_FALSE, "besselyn", m, genie_bessel_yn_real);
  a68_idf (A_FALSE, "besselin", m, genie_bessel_in_real);
  a68_idf (A_FALSE, "besselexpin", m, genie_bessel_exp_in_real);
  a68_idf (A_FALSE, "besselkn", m, genie_bessel_kn_real);
  a68_idf (A_FALSE, "besselexpkn", m, genie_bessel_exp_kn_real);
  a68_idf (A_FALSE, "besseljl", m, genie_bessel_jl_real);
  a68_idf (A_FALSE, "besselyl", m, genie_bessel_yl_real);
  a68_idf (A_FALSE, "besselexpil", m, genie_bessel_exp_il_real);
  a68_idf (A_FALSE, "besselexpkl", m, genie_bessel_exp_kl_real);
  a68_idf (A_FALSE, "besseljnu", m, genie_bessel_jnu_real);
  a68_idf (A_FALSE, "besselynu", m, genie_bessel_ynu_real);
  a68_idf (A_FALSE, "besselinu", m, genie_bessel_inu_real);
  a68_idf (A_FALSE, "besselexpinu", m, genie_bessel_exp_inu_real);
  a68_idf (A_FALSE, "besselknu", m, genie_bessel_knu_real);
  a68_idf (A_FALSE, "besselexpknu", m, genie_bessel_exp_knu_real);
  a68_idf (A_FALSE, "ellipticintegralrc", m, genie_elliptic_integral_rc_real);
  a68_idf (A_FALSE, "incompletegamma", m, genie_gamma_inc_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A_FALSE, "incompletebeta", m, genie_beta_inc_real);
  a68_idf (A_FALSE, "ellipticintegralrf", m, genie_elliptic_integral_rf_real);
  a68_idf (A_FALSE, "ellipticintegralrd", m, genie_elliptic_integral_rd_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NULL);
  a68_idf (A_FALSE, "ellipticintegralrj", m, genie_elliptic_integral_rj_real);
#endif
  m = proc_int;
  a68_idf (A_FALSE, "argc", m, genie_argc);
  a68_idf (A_FALSE, "errno", m, genie_errno);
  a68_idf (A_FALSE, "fork", m, genie_fork);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A_FALSE, "argv", m, genie_argv);
  m = proc_void;
  a68_idf (A_FALSE, "reseterrno", m, genie_reset_errno);
  m = a68_proc (MODE (STRING), MODE (INT), NULL);
  a68_idf (A_FALSE, "strerror", m, genie_strerror);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A_FALSE, "execve", m, genie_execve);
  m = a68_proc (MODE (PIPE), NULL);
  a68_idf (A_FALSE, "createpipe", m, genie_create_pipe);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A_FALSE, "execvechild", m, genie_execve_child);
  m = a68_proc (MODE (PIPE), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NULL);
  a68_idf (A_FALSE, "execvechildpipe", m, genie_execve_child_pipe);
  m = a68_proc (MODE (STRING), MODE (STRING), NULL);
  a68_idf (A_FALSE, "getenv", m, genie_getenv);
  m = a68_proc (MODE (VOID), MODE (INT), NULL);
  a68_idf (A_FALSE, "waitpid", m, genie_waitpid);
  m = a68_proc (MODE (ROW_INT), NULL);
  a68_idf (A_FALSE, "utctime", m, genie_utctime);
  a68_idf (A_FALSE, "localtime", m, genie_localtime);
#ifdef HAVE_HTTP
  m = a68_proc (MODE (INT), MODE (REF_STRING), MODE (STRING), MODE (STRING), MODE (INT), NULL);
  a68_idf (A_FALSE, "httpcontent", m, genie_http_content);
  a68_idf (A_FALSE, "tcprequest", m, genie_tcp_request);
#endif
#ifdef HAVE_REGEX
  m = a68_proc (MODE (INT), MODE (STRING), MODE (STRING), MODE (REF_INT), MODE (REF_INT), NULL);
  a68_idf (A_FALSE, "grepinstring", m, genie_grep_in_string);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (STRING), MODE (REF_STRING), NULL);
  a68_idf (A_FALSE, "subinstring", m, genie_sub_in_string);
#endif
#ifdef HAVE_CURSES
  m = proc_void;
  a68_idf (A_FALSE, "cursesstart", m, genie_curses_start);
  a68_idf (A_FALSE, "cursesend", m, genie_curses_end);
  a68_idf (A_FALSE, "cursesclear", m, genie_curses_clear);
  a68_idf (A_FALSE, "cursesrefresh", m, genie_curses_refresh);
  m = proc_char;
  a68_idf (A_FALSE, "cursesgetchar", m, genie_curses_getchar);
  m = a68_proc (MODE (VOID), MODE (CHAR), NULL);
  a68_idf (A_FALSE, "cursesputchar", m, genie_curses_putchar);
  m = a68_proc (MODE (VOID), MODE (INT), MODE (INT), NULL);
  a68_idf (A_FALSE, "cursesmove", m, genie_curses_move);
  m = proc_int;
  a68_idf (A_FALSE, "curseslines", m, genie_curses_lines);
  a68_idf (A_FALSE, "cursescolumns", m, genie_curses_columns);
#endif
#ifdef HAVE_POSTGRESQL
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), MODE (REF_STRING), NULL);
  a68_idf (A_FALSE, "pqconnectdb", m, genie_pq_connectdb);
  m = a68_proc (MODE (INT), MODE (REF_FILE), NULL);
  a68_idf (A_FALSE, "pqfinish", m, genie_pq_finish);
  a68_idf (A_FALSE, "pqreset", m, genie_pq_reset);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), NULL);
  a68_idf (A_FALSE, "pqparameterstatus", m, genie_pq_parameterstatus);
  a68_idf (A_FALSE, "pqexec", m, genie_pq_exec);
  a68_idf (A_FALSE, "pqfnumber", m, genie_pq_fnumber);
  m = a68_proc (MODE (INT), MODE (REF_FILE), NULL);
  a68_idf (A_FALSE, "pqntuples", m, genie_pq_ntuples);
  a68_idf (A_FALSE, "pqnfields", m, genie_pq_nfields);
  a68_idf (A_FALSE, "pqcmdstatus", m, genie_pq_cmdstatus);
  a68_idf (A_FALSE, "pqcmdtuples", m, genie_pq_cmdtuples);
  a68_idf (A_FALSE, "pqerrormessage", m, genie_pq_errormessage);
  a68_idf (A_FALSE, "pqresulterrormessage", m, genie_pq_resulterrormessage);
  a68_idf (A_FALSE, "pqdb", m, genie_pq_db);
  a68_idf (A_FALSE, "pquser", m, genie_pq_user);
  a68_idf (A_FALSE, "pqpass", m, genie_pq_pass);
  a68_idf (A_FALSE, "pqhost", m, genie_pq_host);
  a68_idf (A_FALSE, "pqport", m, genie_pq_port);
  a68_idf (A_FALSE, "pqtty", m, genie_pq_tty);
  a68_idf (A_FALSE, "pqoptions", m, genie_pq_options);
  a68_idf (A_FALSE, "pqprotocolversion", m, genie_pq_protocolversion);
  a68_idf (A_FALSE, "pqserverversion", m, genie_pq_serverversion);
  a68_idf (A_FALSE, "pqsocket", m, genie_pq_socket);
  a68_idf (A_FALSE, "pqbackendpid", m, genie_pq_backendpid);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), NULL);
  a68_idf (A_FALSE, "pqfname", m, genie_pq_fname);
  a68_idf (A_FALSE, "pqfformat", m, genie_pq_fformat);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), MODE (INT), NULL);
  a68_idf (A_FALSE, "pqgetvalue", m, genie_pq_getvalue);
  a68_idf (A_FALSE, "pqgetisnull", m, genie_pq_getisnull);
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
  proc_complex_complex = a68_proc (MODE (COMPLEX), MODE (COMPLEX), NULL);
  proc_bool = a68_proc (MODE (BOOL), NULL);
  proc_char = a68_proc (MODE (CHAR), NULL);
  proc_void = a68_proc (MODE (VOID), NULL);
  stand_prelude ();
  stand_transput ();
  stand_extensions ();
}
