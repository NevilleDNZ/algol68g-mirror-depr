/*!
\file environ.c
\brief standard prelude implementation
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2012 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

#define A68_STD A68_TRUE
#define A68_EXT A68_FALSE

TABLE_T *a68g_standenv;

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

static void add_a68g_standenv (BOOL_T portable, int a, NODE_T * n, char *c, MOID_T * m, int p, GPROC * q)
{
#define INSERT_TAG(l, n) {\
  NEXT (n) = *(l);\
  *(l) = (n);\
  }
  TAG_T *new_one = new_tag ();
  PROCEDURE_LEVEL (INFO (n)) = 0;
  USE (new_one) = A68_FALSE;
  HEAP (new_one) = HEAP_SYMBOL;
  TAG_TABLE (new_one) = a68g_standenv;
  NODE (new_one) = n;
  VALUE (new_one) = (c != NO_TEXT ? TEXT (add_token (&top_token, c)) : NO_TEXT);
  PRIO (new_one) = p;
  PROCEDURE (new_one) = q;
  A68G_STANDENV_PROC (new_one) = (BOOL_T) (q != NO_GPROC);
  UNIT (new_one) = NULL;
  PORTABLE (new_one) = portable;
  MOID (new_one) = m;
  NEXT (new_one) = NO_TAG;
  if (a == IDENTIFIER) {
    INSERT_TAG (&IDENTIFIERS (a68g_standenv), new_one);
  } else if (a == OP_SYMBOL) {
    INSERT_TAG (&OPERATORS (a68g_standenv), new_one);
  } else if (a == PRIO_SYMBOL) {
    INSERT_TAG (&PRIO (a68g_standenv), new_one);
  } else if (a == INDICANT) {
    INSERT_TAG (&INDICANTS (a68g_standenv), new_one);
  } else if (a == LABEL) {
    INSERT_TAG (&LABELS (a68g_standenv), new_one);
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
  MOID_T *y, **z = &TOP_MOID (&program);
  PACK_T *p = NO_PACK, *q = NO_PACK;
  va_list attribute;
  va_start (attribute, m);
  while ((y = va_arg (attribute, MOID_T *)) != NO_MOID) {
    PACK_T *new_one = new_pack ();
    MOID (new_one) = y;
    TEXT (new_one) = NO_TEXT;
    NEXT (new_one) = NO_PACK;
    if (q != NO_PACK) {
      NEXT (q) = new_one;
    } else {
      p = new_one;
    }
    q = new_one;
  }
  va_end (attribute);
  return (add_mode (z, PROC_SYMBOL, count_pack_members (p), NO_NODE, m, p));
}

/*!
\brief enter an identifier in standenv
\param n name of identifier
\param m mode of identifier
\param q interpreter routine that executes this token
**/

static void a68_idf (BOOL_T portable, char *n, MOID_T * m, GPROC * q)
{
  add_a68g_standenv (portable, IDENTIFIER, some_node (TEXT (add_token (&top_token, n))), NO_TEXT, m, 0, q);
}

/*!
\brief enter a moid in standenv
\param p sizety
\param t name of moid
\param m will point to entry in mode table
**/

static void a68_mode (int p, char *t, MOID_T ** m)
{
  (*m) = add_mode (&TOP_MOID (&program), STANDARD, p, some_node (TEXT (find_keyword (top_keyword, t))), NO_MOID, NO_PACK);
}

/*!
\brief enter a priority in standenv
\param p name of operator
\param b priority of operator
**/

static void a68_prio (char *p, int b)
{
  add_a68g_standenv (A68_TRUE, PRIO_SYMBOL, some_node (TEXT (add_token (&top_token, p))), NO_TEXT, NO_MOID, b, NO_GPROC);
}

/*!
\brief enter operator in standenv
\param n name of operator
\param m mode of operator
\param q interpreter routine that executes this token
**/

static void a68_op (BOOL_T portable, char *n, MOID_T * m, GPROC * q)
{
  add_a68g_standenv (portable, OP_SYMBOL, some_node (TEXT (add_token (&top_token, n))), NO_TEXT, m, 0, q);
}

/*!
\brief enter standard modes in standenv
**/

static void stand_moids (void)
{
  MOID_T *m;
  PACK_T *z;
/* Primitive A68 moids */
  a68_mode (0, "VOID", &MODE (VOID));
/* Standard precision */
  a68_mode (0, "INT", &MODE (INT));
  a68_mode (0, "REAL", &MODE (REAL));
  a68_mode (0, "COMPLEX", &MODE (COMPLEX));
  a68_mode (0, "COMPL", &MODE (COMPL));
  a68_mode (0, "BITS", &MODE (BITS));
  a68_mode (0, "BYTES", &MODE (BYTES));
/* Multiple precision */
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
/* Other */
  a68_mode (0, "BOOL", &MODE (BOOL));
  a68_mode (0, "CHAR", &MODE (CHAR));
  a68_mode (0, "STRING", &MODE (STRING));
  a68_mode (0, "FILE", &MODE (FILE));
  a68_mode (0, "CHANNEL", &MODE (CHANNEL));
  a68_mode (0, "PIPE", &MODE (PIPE));
  a68_mode (0, "FORMAT", &MODE (FORMAT));
  a68_mode (0, "SEMA", &MODE (SEMA));
  a68_mode (0, "SOUND", &MODE (SOUND));
  PORTABLE (MODE (PIPE)) = A68_FALSE;
  HAS_ROWS (MODE (SOUND)) = A68_TRUE;
  PORTABLE (MODE (SOUND)) = A68_FALSE;
/* ROWS */
  MODE (ROWS) = add_mode (&TOP_MOID (&program), ROWS_SYMBOL, 0, NO_NODE, NO_MOID, NO_PACK);
/* REFs */
  MODE (REF_INT) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (INT), NO_PACK);
  MODE (REF_REAL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (REAL), NO_PACK);
  MODE (REF_COMPLEX) = MODE (REF_COMPL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (COMPLEX), NO_PACK);
  MODE (REF_BITS) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (BITS), NO_PACK);
  MODE (REF_BYTES) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (BYTES), NO_PACK);
  MODE (REF_FORMAT) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (FORMAT), NO_PACK);
  MODE (REF_PIPE) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (PIPE), NO_PACK);
/* Multiple precision */
  MODE (REF_LONG_INT) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONG_INT), NO_PACK);
  MODE (REF_LONG_REAL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONG_REAL), NO_PACK);
  MODE (REF_LONG_COMPLEX) = MODE (REF_LONG_COMPL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONG_COMPLEX), NO_PACK);
  MODE (REF_LONGLONG_INT) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONGLONG_INT), NO_PACK);
  MODE (REF_LONGLONG_REAL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONGLONG_REAL), NO_PACK);
  MODE (REF_LONGLONG_COMPLEX) = MODE (REF_LONGLONG_COMPL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONGLONG_COMPLEX), NO_PACK);
  MODE (REF_LONG_BITS) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONG_BITS), NO_PACK);
  MODE (REF_LONGLONG_BITS) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONGLONG_BITS), NO_PACK);
  MODE (REF_LONG_BYTES) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (LONG_BYTES), NO_PACK);
/* Other */
  MODE (REF_BOOL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (BOOL), NO_PACK);
  MODE (REF_CHAR) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (CHAR), NO_PACK);
  MODE (REF_FILE) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (FILE), NO_PACK);
  MODE (REF_REF_FILE) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (REF_FILE), NO_PACK);
  MODE (REF_SOUND) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (SOUND), NO_PACK);
/* [] INT */
  MODE (ROW_INT) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (INT), NO_PACK);
  HAS_ROWS (MODE (ROW_INT)) = A68_TRUE;
  SLICE (MODE (ROW_INT)) = MODE (INT);
  MODE (REF_ROW_INT) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (ROW_INT), NO_PACK);
  NAME (MODE (REF_ROW_INT)) = MODE (REF_INT);
/* [] REAL */
  MODE (ROW_REAL) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (REAL), NO_PACK);
  HAS_ROWS (MODE (ROW_REAL)) = A68_TRUE;
  SLICE (MODE (ROW_REAL)) = MODE (REAL);
  MODE (REF_ROW_REAL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (ROW_REAL), NO_PACK);
  NAME (MODE (REF_ROW_REAL)) = MODE (REF_REAL);
/* [,] REAL */
  MODE (ROWROW_REAL) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 2, NO_NODE, MODE (REAL), NO_PACK);
  HAS_ROWS (MODE (ROWROW_REAL)) = A68_TRUE;
  SLICE (MODE (ROWROW_REAL)) = MODE (ROW_REAL);
  MODE (REF_ROWROW_REAL) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (ROWROW_REAL), NO_PACK);
  NAME (MODE (REF_ROWROW_REAL)) = MODE (REF_ROW_REAL);
/* [] COMPLEX */
  MODE (ROW_COMPLEX) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (COMPLEX), NO_PACK);
  HAS_ROWS (MODE (ROW_COMPLEX)) = A68_TRUE;
  SLICE (MODE (ROW_COMPLEX)) = MODE (COMPLEX);
  MODE (REF_ROW_COMPLEX) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (ROW_COMPLEX), NO_PACK);
  NAME (MODE (REF_ROW_COMPLEX)) = MODE (REF_COMPLEX);
/* [,] COMPLEX */
  MODE (ROWROW_COMPLEX) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 2, NO_NODE, MODE (COMPLEX), NO_PACK);
  HAS_ROWS (MODE (ROWROW_COMPLEX)) = A68_TRUE;
  SLICE (MODE (ROWROW_COMPLEX)) = MODE (ROW_COMPLEX);
  MODE (REF_ROWROW_COMPLEX) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (ROWROW_COMPLEX), NO_PACK);
  NAME (MODE (REF_ROWROW_COMPLEX)) = MODE (REF_ROW_COMPLEX);
/* [] BOOL */
  MODE (ROW_BOOL) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (BOOL), NO_PACK);
  HAS_ROWS (MODE (ROW_BOOL)) = A68_TRUE;
  SLICE (MODE (ROW_BOOL)) = MODE (BOOL);
/* [] BITS */
  MODE (ROW_BITS) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (BITS), NO_PACK);
  HAS_ROWS (MODE (ROW_BITS)) = A68_TRUE;
  SLICE (MODE (ROW_BITS)) = MODE (BITS);
/* [] LONG BITS */
  MODE (ROW_LONG_BITS) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (LONG_BITS), NO_PACK);
  HAS_ROWS (MODE (ROW_LONG_BITS)) = A68_TRUE;
  SLICE (MODE (ROW_LONG_BITS)) = MODE (LONG_BITS);
/* [] LONG LONG BITS */
  MODE (ROW_LONGLONG_BITS) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (LONGLONG_BITS), NO_PACK);
  HAS_ROWS (MODE (ROW_LONGLONG_BITS)) = A68_TRUE;
  SLICE (MODE (ROW_LONGLONG_BITS)) = MODE (LONGLONG_BITS);
/* [] CHAR */
  MODE (ROW_CHAR) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (CHAR), NO_PACK);
  HAS_ROWS (MODE (ROW_CHAR)) = A68_TRUE;
  SLICE (MODE (ROW_CHAR)) = MODE (CHAR);
/* [][] CHAR */
  MODE (ROW_ROW_CHAR) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (ROW_CHAR), NO_PACK);
  HAS_ROWS (MODE (ROW_ROW_CHAR)) = A68_TRUE;
  SLICE (MODE (ROW_ROW_CHAR)) = MODE (ROW_CHAR);
/* MODE STRING = FLEX [] CHAR */
  m = add_mode (&TOP_MOID (&program), FLEX_SYMBOL, 0, NO_NODE, MODE (ROW_CHAR), NO_PACK);
  HAS_ROWS (m) = A68_TRUE;
  MODE (FLEX_ROW_CHAR) = m;
  EQUIVALENT (MODE (STRING)) = m;
/* REF [] CHAR */
  MODE (REF_ROW_CHAR) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, MODE (ROW_CHAR), NO_PACK);
  NAME (MODE (REF_ROW_CHAR)) = MODE (REF_CHAR);
/* PROC [] CHAR */
  MODE (PROC_ROW_CHAR) = add_mode (&TOP_MOID (&program), PROC_SYMBOL, 0, NO_NODE, MODE (ROW_CHAR), NO_PACK);
/* REF STRING = REF FLEX [] CHAR */
  MODE (REF_STRING) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, EQUIVALENT (MODE (STRING)), NO_PACK);
  NAME (MODE (REF_STRING)) = MODE (REF_CHAR);
  DEFLEXED (MODE (REF_STRING)) = MODE (REF_ROW_CHAR);
/* [] STRING */
  MODE (ROW_STRING) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (STRING), NO_PACK);
  HAS_ROWS (MODE (ROW_STRING)) = A68_TRUE;
  SLICE (MODE (ROW_STRING)) = MODE (STRING);
  DEFLEXED (MODE (ROW_STRING)) = MODE (ROW_ROW_CHAR);
/* PROC STRING */
  MODE (PROC_STRING) = add_mode (&TOP_MOID (&program), PROC_SYMBOL, 0, NO_NODE, MODE (STRING), NO_PACK);
  DEFLEXED (MODE (PROC_STRING)) = MODE (PROC_ROW_CHAR);
/* COMPLEX */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REAL), TEXT (add_token (&top_token, "im")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REAL), TEXT (add_token (&top_token, "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  EQUIVALENT (MODE (COMPLEX)) = EQUIVALENT (MODE (COMPL)) = m;
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REF_REAL), TEXT (add_token (&top_token, "im")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REF_REAL), TEXT (add_token (&top_token, "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  NAME (MODE (REF_COMPLEX)) = NAME (MODE (REF_COMPL)) = m;
/* LONG COMPLEX */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (LONG_REAL), TEXT (add_token (&top_token, "im")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (LONG_REAL), TEXT (add_token (&top_token, "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  EQUIVALENT (MODE (LONG_COMPLEX)) = EQUIVALENT (MODE (LONG_COMPL)) = m;
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REF_LONG_REAL), TEXT (add_token (&top_token, "im")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REF_LONG_REAL), TEXT (add_token (&top_token, "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  NAME (MODE (REF_LONG_COMPLEX)) = NAME (MODE (REF_LONG_COMPL)) = m;
/* LONG_LONG COMPLEX */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (LONGLONG_REAL), TEXT (add_token (&top_token, "im")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (LONGLONG_REAL), TEXT (add_token (&top_token, "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  EQUIVALENT (MODE (LONGLONG_COMPLEX)) = EQUIVALENT (MODE (LONGLONG_COMPL)) = m;
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REF_LONGLONG_REAL), TEXT (add_token (&top_token, "im")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REF_LONGLONG_REAL), TEXT (add_token (&top_token, "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  NAME (MODE (REF_LONGLONG_COMPLEX)) = NAME (MODE (REF_LONGLONG_COMPL)) = m;
/* NUMBER */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (INT), NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, MODE (LONG_INT), NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, MODE (LONGLONG_INT), NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REAL), NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, MODE (LONG_REAL), NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, MODE (LONGLONG_REAL), NO_TEXT, NO_NODE);
  MODE (NUMBER) = add_mode (&TOP_MOID (&program), UNION_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
/* SEMA */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REF_INT), NO_TEXT, NO_NODE);
  EQUIVALENT (MODE (SEMA)) = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
/* PROC VOID */
  z = NO_PACK;
  MODE (PROC_VOID) =  add_mode (&TOP_MOID (&program), PROC_SYMBOL, count_pack_members (z), NO_NODE, MODE (VOID), z);
/* PROC (REAL) REAL */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REAL), NO_TEXT, NO_NODE);
  MODE (PROC_REAL_REAL) = add_mode (&TOP_MOID (&program), PROC_SYMBOL, count_pack_members (z), NO_NODE, MODE (REAL), z);
/* IO: PROC (REF FILE) BOOL */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REF_FILE), NO_TEXT, NO_NODE);
  MODE (PROC_REF_FILE_BOOL) = add_mode (&TOP_MOID (&program), PROC_SYMBOL, count_pack_members (z), NO_NODE, MODE (BOOL), z);
/* IO: PROC (REF FILE) VOID */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REF_FILE), NO_TEXT, NO_NODE);
  MODE (PROC_REF_FILE_VOID) = add_mode (&TOP_MOID (&program), PROC_SYMBOL, count_pack_members (z), NO_NODE, MODE (VOID), z);
/* IO: SIMPLIN and SIMPLOUT */
  MODE (SIMPLIN) = add_mode (&TOP_MOID (&program), IN_TYPE_MODE, 0, NO_NODE, NO_MOID, NO_PACK);
  MODE (ROW_SIMPLIN) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (SIMPLIN), NO_PACK);
  SLICE (MODE (ROW_SIMPLIN)) = MODE (SIMPLIN);
  MODE (SIMPLOUT) = add_mode (&TOP_MOID (&program), OUT_TYPE_MODE, 0, NO_NODE, NO_MOID, NO_PACK);
  MODE (ROW_SIMPLOUT) = add_mode (&TOP_MOID (&program), ROW_SYMBOL, 1, NO_NODE, MODE (SIMPLOUT), NO_PACK);
  SLICE (MODE (ROW_SIMPLOUT)) = MODE (SIMPLOUT);
/* PIPE */
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (INT), TEXT (add_token (&top_token, "pid")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REF_FILE), TEXT (add_token (&top_token, "write")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REF_FILE), TEXT (add_token (&top_token, "read")), NO_NODE);
  EQUIVALENT (MODE (PIPE)) = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  z = NO_PACK;
  (void) add_mode_to_pack (&z, MODE (REF_INT), TEXT (add_token (&top_token, "pid")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REF_REF_FILE), TEXT (add_token (&top_token, "write")), NO_NODE);
  (void) add_mode_to_pack (&z, MODE (REF_REF_FILE), TEXT (add_token (&top_token, "read")), NO_NODE);
  NAME (MODE (REF_PIPE)) = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
}

/*!
\brief set up standenv - general RR but not transput
**/

static void stand_prelude (void)
{
  MOID_T *m;
/* Identifiers */
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
  a68_idf (A68_EXT, "seconds", MODE (REAL), genie_cputime);
  a68_idf (A68_EXT, "clock", MODE (REAL), genie_cputime);
  a68_idf (A68_EXT, "cputime", MODE (REAL), genie_cputime);
  a68_idf (A68_EXT, "collections", proc_int, genie_garbage_collections);
  a68_idf (A68_EXT, "blocks",proc_int, genie_block);
  m = a68_proc (MODE (VOID), proc_void, NO_MOID);
  a68_idf (A68_EXT, "ongcevent", m, genie_on_gc_event);
  m = a68_proc (MODE (LONG_INT), NO_MOID);
  a68_idf (A68_EXT, "garbage", m, genie_garbage_freed);
  a68_idf (A68_EXT, "collectseconds", proc_real, genie_garbage_seconds);
  a68_idf (A68_EXT, "stackpointer", MODE (INT), genie_stack_pointer);
  a68_idf (A68_EXT, "systemstackpointer", MODE (INT), genie_system_stack_pointer);
  a68_idf (A68_EXT, "systemstacksize", MODE (INT), genie_system_stack_size);
  a68_idf (A68_EXT, "actualstacksize", MODE (INT), genie_stack_pointer);
  m = proc_void;
  a68_idf (A68_EXT, "gcheap", m, genie_gc_heap);
  a68_idf (A68_EXT, "sweepheap", m, genie_gc_heap);
  a68_idf (A68_EXT, "preemptivegc", m, genie_preemptive_gc_heap);
  a68_idf (A68_EXT, "preemptivesweep", m, genie_preemptive_gc_heap);
  a68_idf (A68_EXT, "preemptivesweepheap", m, genie_preemptive_gc_heap);
  a68_idf (A68_EXT, "break", m, genie_break);
  a68_idf (A68_EXT, "debug", m, genie_debug);
  a68_idf (A68_EXT, "monitor", m, genie_debug);
  m = a68_proc (MODE (STRING), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "evaluate", m, genie_evaluate);
  m = a68_proc (MODE (INT), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "system", m, genie_system);
  m = a68_proc (MODE (STRING), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "acronym", m, genie_acronym);
  a68_idf (A68_EXT, "vmsacronym", m, genie_acronym);
/* BITS procedures */
  m = a68_proc (MODE (BITS), MODE (ROW_BOOL), NO_MOID);
  a68_idf (A68_STD, "bitspack", m, genie_bits_pack);
  m = a68_proc (MODE (LONG_BITS), MODE (ROW_BOOL), NO_MOID);
  a68_idf (A68_STD, "longbitspack", m, genie_long_bits_pack);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (ROW_BOOL), NO_MOID);
  a68_idf (A68_STD, "longlongbitspack", m, genie_long_bits_pack);
/* RNG procedures */
  m = a68_proc (MODE (VOID), MODE (INT), NO_MOID);
  a68_idf (A68_STD, "firstrandom", m, genie_first_random);
  m = proc_real;
  a68_idf (A68_STD, "nextrandom", m, genie_next_random);
  a68_idf (A68_STD, "random", m, genie_next_random);
  a68_idf (A68_STD, "rnd", m, genie_next_rnd);
  m = a68_proc (MODE (LONG_REAL), NO_MOID);
  a68_idf (A68_STD, "longnextrandom", m, genie_long_next_random);
  a68_idf (A68_STD, "longrandom", m, genie_long_next_random);
  m = a68_proc (MODE (LONGLONG_REAL), NO_MOID);
  a68_idf (A68_STD, "longlongnextrandom", m, genie_long_next_random);
  a68_idf (A68_STD, "longlongrandom", m, genie_long_next_random);
/* Priorities */
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
/* INT ops */
  m = a68_proc (MODE (INT), MODE (INT), NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_int);
  a68_op (A68_STD, "ABS", m, genie_abs_int);
  a68_op (A68_STD, "SIGN", m, genie_sign_int);
  m = a68_proc (MODE (BOOL), MODE (INT), NO_MOID);
  a68_op (A68_STD, "ODD", m, genie_odd_int);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (INT), NO_MOID);
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
  m = a68_proc (MODE (INT), MODE (INT), MODE (INT), NO_MOID);
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
  m = a68_proc (MODE (REAL), MODE (INT), MODE (INT), NO_MOID);
  a68_op (A68_STD, "/", m, genie_div_int);
  m = a68_proc (MODE (REF_INT), MODE (REF_INT), MODE (INT), NO_MOID);
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
/* REAL ops */
  m = proc_real_real;
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_real);
  a68_op (A68_STD, "ABS", m, genie_abs_real);
  m = a68_proc (MODE (INT), MODE (REAL), NO_MOID);
  a68_op (A68_STD, "SIGN", m, genie_sign_real);
  a68_op (A68_STD, "ROUND", m, genie_round_real);
  a68_op (A68_STD, "ENTIER", m, genie_entier_real);
  m = a68_proc (MODE (BOOL), MODE (REAL), MODE (REAL), NO_MOID);
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
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (INT), NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_real_int);
  a68_op (A68_STD, "UP", m, genie_pow_real_int);
  a68_op (A68_STD, "^", m, genie_pow_real_int);
  m = a68_proc (MODE (REF_REAL), MODE (REF_REAL), MODE (REAL), NO_MOID);
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
/* COMPLEX ops */
  m = a68_proc (MODE (COMPLEX), MODE (REAL), MODE (REAL), NO_MOID);
  a68_op (A68_STD, "I", m, genie_icomplex);
  a68_op (A68_STD, "+*", m, genie_icomplex);
  m = a68_proc (MODE (COMPLEX), MODE (INT), MODE (INT), NO_MOID);
  a68_op (A68_STD, "I", m, genie_iint_complex);
  a68_op (A68_STD, "+*", m, genie_iint_complex);
  m = a68_proc (MODE (REAL), MODE (COMPLEX), NO_MOID);
  a68_op (A68_STD, "RE", m, genie_re_complex);
  a68_op (A68_STD, "IM", m, genie_im_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_complex);
  m = proc_complex_complex;
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_complex);
  m = a68_proc (MODE (BOOL), MODE (COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_complex);
  a68_op (A68_STD, "/=", m, genie_ne_complex);
  a68_op (A68_STD, "~=", m, genie_ne_complex);
  a68_op (A68_STD, "^=", m, genie_ne_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_complex);
  a68_op (A68_STD, "NE", m, genie_ne_complex);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_complex);
  a68_op (A68_STD, "-", m, genie_sub_complex);
  a68_op (A68_STD, "*", m, genie_mul_complex);
  a68_op (A68_STD, "/", m, genie_div_complex);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), MODE (INT), NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_complex_int);
  m = a68_proc (MODE (REF_COMPLEX), MODE (REF_COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_complex);
/* BOOL ops */
  m = a68_proc (MODE (BOOL), MODE (BOOL), NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_bool);
  a68_op (A68_STD, "~", m, genie_not_bool);
  m = a68_proc (MODE (INT), MODE (BOOL), NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_abs_bool);
  m = a68_proc (MODE (BOOL), MODE (BOOL), MODE (BOOL), NO_MOID);
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
/* CHAR ops */
  m = a68_proc (MODE (BOOL), MODE (CHAR), MODE (CHAR), NO_MOID);
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
  m = a68_proc (MODE (INT), MODE (CHAR), NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_abs_char);
  m = a68_proc (MODE (CHAR), MODE (INT), NO_MOID);
  a68_op (A68_STD, "REPR", m, genie_repr_char);
  m = a68_proc (MODE (BOOL), MODE (CHAR), NO_MOID);
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
  m = a68_proc (MODE (CHAR), MODE (CHAR), NO_MOID);
  a68_idf (A68_EXT, "tolower", m, genie_to_lower);
  a68_idf (A68_EXT, "toupper", m, genie_to_upper);
/* BITS ops */
  m = a68_proc (MODE (INT), MODE (BITS), NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_abs_bits);
  m = a68_proc (MODE (BITS), MODE (INT), NO_MOID);
  a68_op (A68_STD, "BIN", m, genie_bin_int);
  m = a68_proc (MODE (BITS), MODE (BITS), NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_bits);
  a68_op (A68_STD, "~", m, genie_not_bits);
  m = a68_proc (MODE (BOOL), MODE (BITS), MODE (BITS), NO_MOID);
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
  m = a68_proc (MODE (BITS), MODE (BITS), MODE (BITS), NO_MOID);
  a68_op (A68_STD, "AND", m, genie_and_bits);
  a68_op (A68_STD, "&", m, genie_and_bits);
  a68_op (A68_STD, "OR", m, genie_or_bits);
  a68_op (A68_EXT, "XOR", m, genie_xor_bits);
  m = a68_proc (MODE (BITS), MODE (BITS), MODE (INT), NO_MOID);
  a68_op (A68_STD, "SHL", m, genie_shl_bits);
  a68_op (A68_STD, "UP", m, genie_shl_bits);
  a68_op (A68_STD, "SHR", m, genie_shr_bits);
  a68_op (A68_STD, "DOWN", m, genie_shr_bits);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (BITS), NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_bits);
  m = a68_proc (MODE (BITS), MODE (INT), MODE (BITS), NO_MOID);
  a68_op (A68_STD, "SET", m, genie_set_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_bits);
/* BYTES ops */
  m = a68_proc (MODE (BYTES), MODE (STRING), NO_MOID);
  a68_idf (A68_STD, "bytespack", m, genie_bytespack);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (BYTES), NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_bytes);
  m = a68_proc (MODE (BYTES), MODE (BYTES), MODE (BYTES), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_bytes);
  m = a68_proc (MODE (REF_BYTES), MODE (REF_BYTES), MODE (BYTES), NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_bytes);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_bytes);
  m = a68_proc (MODE (REF_BYTES), MODE (BYTES), MODE (REF_BYTES), NO_MOID);
  a68_op (A68_STD, "+=:", m, genie_plusto_bytes);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_bytes);
  m = a68_proc (MODE (BOOL), MODE (BYTES), MODE (BYTES), NO_MOID);
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
/* LONG BYTES ops */
  m = a68_proc (MODE (LONG_BYTES), MODE (BYTES), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_leng_bytes);
  m = a68_proc (MODE (BYTES), MODE (LONG_BYTES), NO_MOID);
  a68_idf (A68_STD, "SHORTEN", m, genie_shorten_bytes);
  m = a68_proc (MODE (LONG_BYTES), MODE (STRING), NO_MOID);
  a68_idf (A68_STD, "longbytespack", m, genie_long_bytespack);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (LONG_BYTES), NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_long_bytes);
  m = a68_proc (MODE (LONG_BYTES), MODE (LONG_BYTES), MODE (LONG_BYTES), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_long_bytes);
  m = a68_proc (MODE (REF_LONG_BYTES), MODE (REF_LONG_BYTES), MODE (LONG_BYTES), NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_bytes);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_bytes);
  m = a68_proc (MODE (REF_LONG_BYTES), MODE (LONG_BYTES), MODE (REF_LONG_BYTES), NO_MOID);
  a68_op (A68_STD, "+=:", m, genie_plusto_long_bytes);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_long_bytes);
  m = a68_proc (MODE (BOOL), MODE (LONG_BYTES), MODE (LONG_BYTES), NO_MOID);
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
/* STRING ops */
  m = a68_proc (MODE (BOOL), MODE (STRING), MODE (STRING), NO_MOID);
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
  m = a68_proc (MODE (STRING), MODE (CHAR), MODE (CHAR), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_char);
  m = a68_proc (MODE (STRING), MODE (STRING), MODE (STRING), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_string);
  m = a68_proc (MODE (REF_STRING), MODE (REF_STRING), MODE (STRING), NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_string);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_string);
  m = a68_proc (MODE (REF_STRING), MODE (REF_STRING), MODE (INT), NO_MOID);
  a68_op (A68_STD, "*:=", m, genie_timesab_string);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_string);
  m = a68_proc (MODE (REF_STRING), MODE (STRING), MODE (REF_STRING), NO_MOID);
  a68_op (A68_STD, "+=:", m, genie_plusto_string);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_string);
  m = a68_proc (MODE (STRING), MODE (STRING), MODE (INT), NO_MOID);
  a68_op (A68_STD, "*", m, genie_times_string_int);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (STRING), NO_MOID);
  a68_op (A68_STD, "*", m, genie_times_int_string);
  m = a68_proc (MODE (STRING), MODE (INT), MODE (CHAR), NO_MOID);
  a68_op (A68_STD, "*", m, genie_times_int_char);
  m = a68_proc (MODE (STRING), MODE (CHAR), MODE (INT), NO_MOID);
  a68_op (A68_STD, "*", m, genie_times_char_int);
  m = a68_proc (MODE (CHAR), MODE (INT), MODE (ROW_CHAR), NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_string);
/* SEMA ops */
#if defined HAVE_PARALLEL_CLAUSE
  m = a68_proc (MODE (SEMA), MODE (INT), NO_MOID);
  a68_op (A68_STD, "LEVEL", m, genie_level_sema_int);
  m = a68_proc (MODE (INT), MODE (SEMA), NO_MOID);
  a68_op (A68_STD, "LEVEL", m, genie_level_int_sema);
  m = a68_proc (MODE (VOID), MODE (SEMA), NO_MOID);
  a68_op (A68_STD, "UP", m, genie_up_sema);
  a68_op (A68_STD, "DOWN", m, genie_down_sema);
#else
  m = a68_proc (MODE (SEMA), MODE (INT), NO_MOID);
  a68_op (A68_STD, "LEVEL", m, genie_unimplemented);
  m = a68_proc (MODE (INT), MODE (SEMA), NO_MOID);
  a68_op (A68_STD, "LEVEL", m, genie_unimplemented);
  m = a68_proc (MODE (VOID), MODE (SEMA), NO_MOID);
  a68_op (A68_STD, "UP", m, genie_unimplemented);
  a68_op (A68_STD, "DOWN", m, genie_unimplemented);
#endif
/* ROWS ops */
  m = a68_proc (MODE (INT), MODE (ROWS), NO_MOID);
  a68_op (A68_EXT, "ELEMS", m, genie_monad_elems);
  a68_op (A68_STD, "LWB", m, genie_monad_lwb);
  a68_op (A68_STD, "UPB", m, genie_monad_upb);
  m = a68_proc (MODE (INT), MODE (INT), MODE (ROWS), NO_MOID);
  a68_op (A68_EXT, "ELEMS", m, genie_dyad_elems);
  a68_op (A68_STD, "LWB", m, genie_dyad_lwb);
  a68_op (A68_STD, "UPB", m, genie_dyad_upb);
  m = a68_proc (MODE (ROW_STRING), MODE (ROW_STRING), NO_MOID);
  a68_op (A68_EXT, "SORT", m, genie_sort_row_string);
/* Binding for the multiple-precision library */
/* LONG INT */
  m = a68_proc (MODE (LONG_INT), MODE (INT), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_int_to_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_long_mp);
  m = a68_proc (MODE (INT), MODE (LONG_INT), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_int);
  a68_op (A68_STD, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_INT), NO_MOID);
  a68_op (A68_STD, "ODD", m, genie_odd_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_REAL), NO_MOID);
  a68_op (A68_STD, "ENTIER", m, genie_entier_long_mp);
  a68_op (A68_STD, "ROUND", m, genie_round_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), MODE (LONG_INT), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_long_int);
  a68_op (A68_STD, "-", m, genie_sub_long_int);
  a68_op (A68_STD, "*", m, genie_mul_long_int);
  a68_op (A68_STD, "OVER", m, genie_over_long_mp);
  a68_op (A68_STD, "%", m, genie_over_long_mp);
  a68_op (A68_STD, "MOD", m, genie_mod_long_mp);
  a68_op (A68_STD, "%*", m, genie_mod_long_mp);
  m = a68_proc (MODE (REF_LONG_INT), MODE (REF_LONG_INT), MODE (LONG_INT), NO_MOID);
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
  m = a68_proc (MODE (BOOL), MODE (LONG_INT), MODE (LONG_INT), NO_MOID);
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
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_INT), MODE (LONG_INT), NO_MOID);
  a68_op (A68_STD, "/", m, genie_div_long_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONG_INT), MODE (INT), NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_long_mp_int_int);
  a68_op (A68_STD, "^", m, genie_pow_long_mp_int_int);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_INT), MODE (LONG_INT), NO_MOID);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
/* LONG REAL */
  m = a68_proc (MODE (LONG_REAL), MODE (REAL), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_real_to_long_mp);
  m = a68_proc (MODE (REAL), MODE (LONG_REAL), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_real);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), NO_MOID);
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
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (LONG_REAL), NO_MOID);
  a68_idf (A68_STD, "longarctan2", m, genie_atan2_long_mp);
  a68_idf (A68_STD, "darctan2", m, genie_atan2_long_mp);
  m = a68_proc (MODE (INT), MODE (LONG_REAL), NO_MOID);
  a68_op (A68_STD, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (LONG_REAL), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_long_mp);
  a68_op (A68_STD, "-", m, genie_sub_long_mp);
  a68_op (A68_STD, "*", m, genie_mul_long_mp);
  a68_op (A68_STD, "/", m, genie_div_long_mp);
  a68_op (A68_STD, "**", m, genie_pow_long_mp);
  a68_op (A68_STD, "UP", m, genie_pow_long_mp);
  a68_op (A68_STD, "^", m, genie_pow_long_mp);
  m = a68_proc (MODE (REF_LONG_REAL), MODE (REF_LONG_REAL), MODE (LONG_REAL), NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_mp);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_mp);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_mp);
  a68_op (A68_STD, "/:=", m, genie_divab_long_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_mp);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_mp);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_mp);
  a68_op (A68_STD, "DIVAB", m, genie_divab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_REAL), MODE (LONG_REAL), NO_MOID);
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
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_REAL), MODE (INT), NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_long_mp_int);
  a68_op (A68_STD, "UP", m, genie_pow_long_mp_int);
  a68_op (A68_STD, "^", m, genie_pow_long_mp_int);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_REAL), MODE (LONG_REAL), NO_MOID);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
/* LONG COMPLEX */
  m = a68_proc (MODE (LONG_COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_complex_to_long_complex);
  m = a68_proc (MODE (COMPLEX), MODE (LONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_complex_to_complex);
  m = a68_proc (MODE (LONG_REAL), MODE (LONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "RE", m, genie_re_long_complex);
  a68_op (A68_STD, "IM", m, genie_im_long_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_long_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_long_complex);
  a68_op (A68_STD, "-", m, genie_sub_long_complex);
  a68_op (A68_STD, "*", m, genie_mul_long_complex);
  a68_op (A68_STD, "/", m, genie_div_long_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), MODE (INT), NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_long_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_long_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_long_complex_int);
  m = a68_proc (MODE (BOOL), MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_long_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_long_complex);
  a68_op (A68_STD, "/=", m, genie_ne_long_complex);
  a68_op (A68_STD, "~=", m, genie_ne_long_complex);
  a68_op (A68_STD, "^=", m, genie_ne_long_complex);
  a68_op (A68_STD, "NE", m, genie_ne_long_complex);
  m = a68_proc (MODE (REF_LONG_COMPLEX), MODE (REF_LONG_COMPLEX), MODE (LONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_long_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_long_complex);
/* LONG BITS ops */
  m = a68_proc (MODE (LONG_INT), MODE (LONG_BITS), NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_idle);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_INT), NO_MOID);
  a68_op (A68_STD, "BIN", m, genie_bin_long_mp);
  m = a68_proc (MODE (BITS), MODE (LONG_BITS), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (BITS), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_unsigned_to_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONG_BITS), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_long_mp);
  a68_op (A68_STD, "~", m, genie_not_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONG_BITS), MODE (LONG_BITS), NO_MOID);
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
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), MODE (LONG_BITS), NO_MOID);
  a68_op (A68_STD, "AND", m, genie_and_long_mp);
  a68_op (A68_STD, "&", m, genie_and_long_mp);
  a68_op (A68_STD, "OR", m, genie_or_long_mp);
  a68_op (A68_EXT, "XOR", m, genie_xor_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONG_BITS), MODE (INT), NO_MOID);
  a68_op (A68_STD, "SHL", m, genie_shl_long_mp);
  a68_op (A68_STD, "UP", m, genie_shl_long_mp);
  a68_op (A68_STD, "SHR", m, genie_shr_long_mp);
  a68_op (A68_STD, "DOWN", m, genie_shr_long_mp);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (LONG_BITS), NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_long_bits);
  m = a68_proc (MODE (LONG_BITS), MODE (INT), MODE (LONG_BITS), NO_MOID);
  a68_op (A68_STD, "SET", m, genie_set_long_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_long_bits);
/* LONG LONG INT */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONG_INT), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_INT), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_long_mp);
  m = a68_proc (MODE (INT), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "SIGN", m, genie_sign_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "ODD", m, genie_odd_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_REAL), NO_MOID);
  a68_op (A68_STD, "ENTIER", m, genie_entier_long_mp);
  a68_op (A68_STD, "ROUND", m, genie_round_long_mp);
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_long_int);
  a68_op (A68_STD, "-", m, genie_sub_long_int);
  a68_op (A68_STD, "*", m, genie_mul_long_int);
  a68_op (A68_STD, "OVER", m, genie_over_long_mp);
  a68_op (A68_STD, "%", m, genie_over_long_mp);
  a68_op (A68_STD, "MOD", m, genie_mod_long_mp);
  a68_op (A68_STD, "%*", m, genie_mod_long_mp);
  m = a68_proc (MODE (REF_LONGLONG_INT), MODE (REF_LONGLONG_INT), MODE (LONGLONG_INT), NO_MOID);
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
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "/", m, genie_div_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NO_MOID);
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
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), MODE (INT), NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_long_mp_int_int);
  a68_op (A68_STD, "^", m, genie_pow_long_mp_int_int);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_INT), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
/* LONG LONG REAL */
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONG_REAL), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_long_mp_to_longlong_mp);
  m = a68_proc (MODE (LONG_REAL), MODE (LONGLONG_REAL), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NO_MOID);
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
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NO_MOID);
  a68_idf (A68_STD, "longarctan2", m, genie_atan2_long_mp);
  a68_idf (A68_STD, "qarctan2", m, genie_atan2_long_mp);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_long_mp);
  a68_op (A68_STD, "-", m, genie_sub_long_mp);
  a68_op (A68_STD, "*", m, genie_mul_long_mp);
  a68_op (A68_STD, "/", m, genie_div_long_mp);
  a68_op (A68_STD, "**", m, genie_pow_long_mp);
  a68_op (A68_STD, "UP", m, genie_pow_long_mp);
  a68_op (A68_STD, "^", m, genie_pow_long_mp);
  m = a68_proc (MODE (REF_LONGLONG_REAL), MODE (REF_LONGLONG_REAL), MODE (LONGLONG_REAL), NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_mp);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_mp);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_mp);
  a68_op (A68_STD, "/:=", m, genie_divab_long_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_mp);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_mp);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_mp);
  a68_op (A68_STD, "DIVAB", m, genie_divab_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NO_MOID);
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
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), MODE (INT), NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_long_mp_int);
  a68_op (A68_STD, "UP", m, genie_pow_long_mp_int);
  a68_op (A68_STD, "^", m, genie_pow_long_mp_int);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NO_MOID);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
/* LONGLONG COMPLEX */
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_long_complex_to_longlong_complex);
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_longlong_complex_to_long_complex);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "RE", m, genie_re_long_complex);
  a68_op (A68_STD, "IM", m, genie_im_long_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_long_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_long_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_long_complex);
  a68_op (A68_STD, "-", m, genie_sub_long_complex);
  a68_op (A68_STD, "*", m, genie_mul_long_complex);
  a68_op (A68_STD, "/", m, genie_div_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), MODE (INT), NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_long_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_long_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_long_complex_int);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_long_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_long_complex);
  a68_op (A68_STD, "/=", m, genie_ne_long_complex);
  a68_op (A68_STD, "~=", m, genie_ne_long_complex);
  a68_op (A68_STD, "^=", m, genie_ne_long_complex);
  a68_op (A68_STD, "NE", m, genie_ne_long_complex);
  m = a68_proc (MODE (REF_LONGLONG_COMPLEX), MODE (REF_LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_long_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_long_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_long_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_long_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_long_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_long_complex);
/* LONG LONG BITS */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_BITS), NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "BIN", m, genie_bin_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_long_mp);
  a68_op (A68_STD, "~", m, genie_not_long_mp);
  m = a68_proc (MODE (LONG_BITS), MODE (LONGLONG_BITS), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_longlong_mp_to_long_mp);
  m = a68_proc (MODE (BOOL), MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NO_MOID);
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
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NO_MOID);
  a68_op (A68_STD, "AND", m, genie_and_long_mp);
  a68_op (A68_STD, "&", m, genie_and_long_mp);
  a68_op (A68_STD, "OR", m, genie_or_long_mp);
  a68_op (A68_EXT, "XOR", m, genie_xor_long_mp);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), MODE (INT), NO_MOID);
  a68_op (A68_STD, "SHL", m, genie_shl_long_mp);
  a68_op (A68_STD, "UP", m, genie_shl_long_mp);
  a68_op (A68_STD, "SHR", m, genie_shr_long_mp);
  a68_op (A68_STD, "DOWN", m, genie_shr_long_mp);
  m = a68_proc (MODE (BOOL), MODE (INT), MODE (LONGLONG_BITS), NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_longlong_bits);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (INT), MODE (LONGLONG_BITS), NO_MOID);
  a68_op (A68_STD, "SET", m, genie_set_longlong_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_longlong_bits);
/* Some "terminators" to handle the mapping of very short or very long modes.
   This allows you to write SHORT REAL z = SHORTEN pi while everything is
   silently mapped onto REAL */
  m = a68_proc (MODE (LONGLONG_INT), MODE (LONGLONG_INT), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (MODE (LONGLONG_BITS), MODE (LONGLONG_BITS), NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (MODE (INT), MODE (INT), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (REAL), MODE (REAL), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
  m = a68_proc (MODE (BITS), MODE (BITS), NO_MOID);
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
#if defined HAVE_GNU_GSL
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
  m = a68_proc (MODE (REAL), proc_real_real, MODE (REAL), MODE (REF_REAL), NO_MOID);
  a68_idf (A68_EXT, "laplace", m, genie_laplace);
#endif
  m = a68_proc (MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), NO_MOID);
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
  m = a68_proc (MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), NO_MOID);
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
/* SOUND/RIFF procs */
  m = a68_proc (MODE (SOUND), MODE (INT), MODE (INT), MODE (INT), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "newsound", m, genie_new_sound);
  m = a68_proc (MODE (INT), MODE (SOUND), MODE (INT), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "getsound", m, genie_get_sound);
  m = a68_proc (MODE (VOID), MODE (SOUND), MODE (INT), MODE (INT), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "setsound", m, genie_set_sound);
  m = a68_proc (MODE (INT), MODE (SOUND), NO_MOID);
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
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), NO_MOID);
  a68_idf (A68_STD, "whole", m, genie_whole);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), NO_MOID);
  a68_idf (A68_STD, "fixed", m, genie_fixed);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), MODE (INT), NO_MOID);
  a68_idf (A68_STD, "float", m, genie_float);
  m = a68_proc (MODE (STRING), MODE (NUMBER), MODE (INT), MODE (INT), MODE (INT), MODE (INT), NO_MOID);
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
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (STRING), NO_MOID);
  a68_idf (A68_STD, "maketerm", m, genie_make_term);
  m = a68_proc (MODE (BOOL), MODE (CHAR), MODE (REF_INT), MODE (STRING), NO_MOID);
  a68_idf (A68_STD, "charinstring", m, genie_char_in_string);
  a68_idf (A68_EXT, "lastcharinstring", m, genie_last_char_in_string);
  m = a68_proc (MODE (BOOL), MODE (STRING), MODE (REF_INT), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "stringinstring", m, genie_string_in_string);
  m = a68_proc (MODE (STRING), MODE (REF_FILE), NO_MOID);
  a68_idf (A68_EXT, "idf", m, genie_idf);
  a68_idf (A68_EXT, "term", m, genie_term);
  m = a68_proc (MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "programidf", m, genie_program_idf);
/* Event routines */
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (PROC_REF_FILE_BOOL), NO_MOID);
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
/* Enquiries on files */
  a68_idf (A68_STD, "putpossible", MODE (PROC_REF_FILE_BOOL), genie_put_possible);
  a68_idf (A68_STD, "getpossible", MODE (PROC_REF_FILE_BOOL), genie_get_possible);
  a68_idf (A68_STD, "binpossible", MODE (PROC_REF_FILE_BOOL), genie_bin_possible);
  a68_idf (A68_STD, "setpossible", MODE (PROC_REF_FILE_BOOL), genie_set_possible);
  a68_idf (A68_STD, "resetpossible", MODE (PROC_REF_FILE_BOOL), genie_reset_possible);
  a68_idf (A68_EXT, "rewindpossible", MODE (PROC_REF_FILE_BOOL), genie_reset_possible);
  a68_idf (A68_STD, "reidfpossible", MODE (PROC_REF_FILE_BOOL), genie_reidf_possible);
  a68_idf (A68_EXT, "drawpossible", MODE (PROC_REF_FILE_BOOL), genie_draw_possible);
  a68_idf (A68_STD, "compressible", MODE (PROC_REF_FILE_BOOL), genie_compressible);
  a68_idf (A68_EXT, "endoffile", MODE (PROC_REF_FILE_BOOL), genie_eof);
  a68_idf (A68_EXT, "eof", MODE (PROC_REF_FILE_BOOL), genie_eof);
  a68_idf (A68_EXT, "endofline", MODE (PROC_REF_FILE_BOOL), genie_eoln);
  a68_idf (A68_EXT, "eoln", MODE (PROC_REF_FILE_BOOL), genie_eoln);
/* Handling of files */
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), MODE (CHANNEL), NO_MOID);
  a68_idf (A68_STD, "open", m, genie_open);
  a68_idf (A68_STD, "establish", m, genie_establish);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REF_STRING), NO_MOID);
  a68_idf (A68_STD, "associate", m, genie_associate);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (CHANNEL), NO_MOID);
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
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), NO_MOID);
  a68_idf (A68_STD, "set", m, genie_set);
  a68_idf (A68_STD, "seek", m, genie_set);
  m = a68_proc (MODE (VOID), MODE (ROW_SIMPLIN), NO_MOID);
  a68_idf (A68_STD, "read", m, genie_read);
  a68_idf (A68_STD, "readbin", m, genie_read_bin);
  a68_idf (A68_STD, "readf", m, genie_read_format);
  m = a68_proc (MODE (VOID), MODE (ROW_SIMPLOUT), NO_MOID);
  a68_idf (A68_STD, "print", m, genie_write);
  a68_idf (A68_STD, "write", m, genie_write);
  a68_idf (A68_STD, "printbin", m, genie_write_bin);
  a68_idf (A68_STD, "writebin", m, genie_write_bin);
  a68_idf (A68_STD, "printf", m, genie_write_format);
  a68_idf (A68_STD, "writef", m, genie_write_format);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_SIMPLIN), NO_MOID);
  a68_idf (A68_STD, "get", m, genie_read_file);
  a68_idf (A68_STD, "getf", m, genie_read_file_format);
  a68_idf (A68_STD, "getbin", m, genie_read_bin_file);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_SIMPLOUT), NO_MOID);
  a68_idf (A68_STD, "put", m, genie_write_file);
  a68_idf (A68_STD, "putf", m, genie_write_file_format);
  a68_idf (A68_STD, "putbin", m, genie_write_bin_file);
/* ALGOL68C type procs */
  m = proc_int;
  a68_idf (A68_EXT, "readint", m, genie_read_int);
  m = a68_proc (MODE (VOID), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "printint", m, genie_print_int);
  m = a68_proc (MODE (LONG_INT), NO_MOID);
  a68_idf (A68_EXT, "readlongint", m, genie_read_long_int);
  m = a68_proc (MODE (VOID), MODE (LONG_INT), NO_MOID);
  a68_idf (A68_EXT, "printlongint", m, genie_print_long_int);
  m = a68_proc (MODE (LONGLONG_INT), NO_MOID);
  a68_idf (A68_EXT, "readlonglongint", m, genie_read_longlong_int);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_INT), NO_MOID);
  a68_idf (A68_EXT, "printlonglongint", m, genie_print_longlong_int);
  m = proc_real;
  a68_idf (A68_EXT, "readreal", m, genie_read_real);
  m = a68_proc (MODE (VOID), MODE (REAL), NO_MOID);
  a68_idf (A68_EXT, "printreal", m, genie_print_real);
  m = a68_proc (MODE (LONG_REAL), NO_MOID);
  a68_idf (A68_EXT, "readlongreal", m, genie_read_long_real);
  a68_idf (A68_EXT, "readdouble", m, genie_read_long_real);
  m = a68_proc (MODE (VOID), MODE (LONG_REAL), NO_MOID);
  a68_idf (A68_EXT, "printlongreal", m, genie_print_long_real);
  a68_idf (A68_EXT, "printdouble", m, genie_print_long_real);
  m = a68_proc (MODE (LONGLONG_REAL), NO_MOID);
  a68_idf (A68_EXT, "readlonglongreal", m, genie_read_longlong_real);
  a68_idf (A68_EXT, "readquad", m, genie_read_longlong_real);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_REAL), NO_MOID);
  a68_idf (A68_EXT, "printlonglongreal", m, genie_print_longlong_real);
  a68_idf (A68_EXT, "printquad", m, genie_print_longlong_real);
  m = a68_proc (MODE (COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "readcompl", m, genie_read_complex);
  a68_idf (A68_EXT, "readcomplex", m, genie_read_complex);
  m = a68_proc (MODE (VOID), MODE (COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "printcompl", m, genie_print_complex);
  a68_idf (A68_EXT, "printcomplex", m, genie_print_complex);
  m = a68_proc (MODE (LONG_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "readlongcompl", m, genie_read_long_complex);
  a68_idf (A68_EXT, "readlongcomplex", m, genie_read_long_complex);
  m = a68_proc (MODE (VOID), MODE (LONG_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "printlongcompl", m, genie_print_long_complex);
  a68_idf (A68_EXT, "printlongcomplex", m, genie_print_long_complex);
  m = a68_proc (MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "readlonglongcompl", m, genie_read_longlong_complex);
  a68_idf (A68_EXT, "readlonglongcomplex", m, genie_read_longlong_complex);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "printlonglongcompl", m, genie_print_longlong_complex);
  a68_idf (A68_EXT, "printlonglongcomplex", m, genie_print_longlong_complex);
  m = proc_bool;
  a68_idf (A68_EXT, "readbool", m, genie_read_bool);
  m = a68_proc (MODE (VOID), MODE (BOOL), NO_MOID);
  a68_idf (A68_EXT, "printbool", m, genie_print_bool);
  m = a68_proc (MODE (BITS), NO_MOID);
  a68_idf (A68_EXT, "readbits", m, genie_read_bits);
  m = a68_proc (MODE (LONG_BITS), NO_MOID);
  a68_idf (A68_EXT, "readlongbits", m, genie_read_long_bits);
  m = a68_proc (MODE (LONGLONG_BITS), NO_MOID);
  a68_idf (A68_EXT, "readlonglongbits", m, genie_read_longlong_bits);
  m = a68_proc (MODE (VOID), MODE (BITS), NO_MOID);
  a68_idf (A68_EXT, "printbits", m, genie_print_bits);
  m = a68_proc (MODE (VOID), MODE (LONG_BITS), NO_MOID);
  a68_idf (A68_EXT, "printlongbits", m, genie_print_long_bits);
  m = a68_proc (MODE (VOID), MODE (LONGLONG_BITS), NO_MOID);
  a68_idf (A68_EXT, "printlonglongbits", m, genie_print_longlong_bits);
  m = proc_char;
  a68_idf (A68_EXT, "readchar", m, genie_read_char);
  m = a68_proc (MODE (VOID), MODE (CHAR), NO_MOID);
  a68_idf (A68_EXT, "printchar", m, genie_print_char);
  a68_idf (A68_EXT, "readstring", MODE (PROC_STRING), genie_read_string);
  a68_idf (A68_EXT, "readline", MODE (PROC_STRING), genie_read_line);
  m = a68_proc (MODE (VOID), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "printstring", m, genie_print_string);
/* Constants ex GSL */
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
}

/*!
\brief set up standenv - extensions
**/

static void stand_extensions (void)
{
  MOID_T *m = NO_MOID;
  (void) m;                     /* To fool cc in case we have none of the libraries */
#if defined HAVE_GNU_PLOTUTILS
/* Drawing */
  m = a68_proc (MODE (BOOL), MODE (REF_FILE), MODE (STRING), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "drawdevice", m, genie_make_device);
  a68_idf (A68_EXT, "makedevice", m, genie_make_device);
  m = a68_proc (MODE (REAL), MODE (REF_FILE), NO_MOID);
  a68_idf (A68_EXT, "drawaspect", m, genie_draw_aspect);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), NO_MOID);
  a68_idf (A68_EXT, "drawclear", m, genie_draw_clear);
  a68_idf (A68_EXT, "drawerase", m, genie_draw_clear);
  a68_idf (A68_EXT, "drawflush", m, genie_draw_show);
  a68_idf (A68_EXT, "drawshow", m, genie_draw_show);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "drawfillstyle", m, genie_draw_fillstyle);
  m = a68_proc (MODE (STRING), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "drawgetcolourname", m, genie_draw_get_colour_name);
  a68_idf (A68_EXT, "drawgetcolorname", m, genie_draw_get_colour_name);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), MODE (REAL), MODE (REAL), NO_MOID);
  a68_idf (A68_EXT, "drawcolor", m, genie_draw_colour);
  a68_idf (A68_EXT, "drawcolour", m, genie_draw_colour);
  a68_idf (A68_EXT, "drawbackgroundcolor", m, genie_draw_background_colour);
  a68_idf (A68_EXT, "drawbackgroundcolour", m, genie_draw_background_colour);
  a68_idf (A68_EXT, "drawcircle", m, genie_draw_circle);
  a68_idf (A68_EXT, "drawball", m, genie_draw_atom);
  a68_idf (A68_EXT, "drawstar", m, genie_draw_star);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), MODE (REAL), NO_MOID);
  a68_idf (A68_EXT, "drawpoint", m, genie_draw_point);
  a68_idf (A68_EXT, "drawline", m, genie_draw_line);
  a68_idf (A68_EXT, "drawmove", m, genie_draw_move);
  a68_idf (A68_EXT, "drawrect", m, genie_draw_rect);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (CHAR), MODE (CHAR), MODE (ROW_CHAR), NO_MOID);
  a68_idf (A68_EXT, "drawtext", m, genie_draw_text);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (ROW_CHAR), NO_MOID);
  a68_idf (A68_EXT, "drawlinestyle", m, genie_draw_linestyle);
  a68_idf (A68_EXT, "drawfontname", m, genie_draw_fontname);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (REAL), NO_MOID);
  a68_idf (A68_EXT, "drawlinewidth", m, genie_draw_linewidth);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "drawfontsize", m, genie_draw_fontsize);
  a68_idf (A68_EXT, "drawtextangle", m, genie_draw_textangle);
  m = a68_proc (MODE (VOID), MODE (REF_FILE), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "drawcolorname", m, genie_draw_colour_name);
  a68_idf (A68_EXT, "drawcolourname", m, genie_draw_colour_name);
  a68_idf (A68_EXT, "drawbackgroundcolorname", m, genie_draw_background_colour_name);
  a68_idf (A68_EXT, "drawbackgroundcolourname", m, genie_draw_background_colour_name);
#endif
#if defined HAVE_GNU_GSL
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
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NO_MOID);
  a68_idf (A68_EXT, "incompletebeta", m, genie_beta_inc_real);
  a68_idf (A68_EXT, "ellipticintegralrf", m, genie_elliptic_integral_rf_real);
  a68_idf (A68_EXT, "ellipticintegralrd", m, genie_elliptic_integral_rd_real);
  m = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NO_MOID);
  a68_idf (A68_EXT, "ellipticintegralrj", m, genie_elliptic_integral_rj_real);
/* Vector and matrix monadic */
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_vector_minus);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_matrix_minus);
  a68_op (A68_EXT, "T", m, genie_matrix_transpose);
  a68_op (A68_EXT, "INV", m, genie_matrix_inv);
  m = a68_proc (MODE (REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_op (A68_EXT, "DET", m, genie_matrix_det);
  a68_op (A68_EXT, "TRACE", m, genie_matrix_trace);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_vector_complex_minus);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_matrix_complex_minus);
  a68_op (A68_EXT, "T", m, genie_matrix_complex_transpose);
  a68_op (A68_EXT, "INV", m, genie_matrix_complex_inv);
  m = a68_proc (MODE (COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "DET", m, genie_matrix_complex_det);
  a68_op (A68_EXT, "TRACE", m, genie_matrix_complex_trace);
/* Vector and matrix dyadic */
  m = a68_proc (MODE (BOOL), MODE (ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "=", m, genie_vector_eq);
  a68_op (A68_EXT, "/=", m, genie_vector_ne);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "+", m, genie_vector_add);
  a68_op (A68_EXT, "-", m, genie_vector_sub);
  m = a68_proc (MODE (REF_ROW_REAL), MODE (REF_ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "+:=", m, genie_vector_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_vector_plusab);
  a68_op (A68_EXT, "-:=", m, genie_vector_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_vector_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_op (A68_EXT, "=", m, genie_matrix_eq);
  a68_op (A68_EXT, "/-", m, genie_matrix_ne);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_op (A68_EXT, "+", m, genie_matrix_add);
  a68_op (A68_EXT, "-", m, genie_matrix_sub);
  m = a68_proc (MODE (REF_ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_op (A68_EXT, "+:=", m, genie_matrix_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_matrix_plusab);
  a68_op (A68_EXT, "-:=", m, genie_matrix_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_matrix_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "=", m, genie_vector_complex_eq);
  a68_op (A68_EXT, "/=", m, genie_vector_complex_ne);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "+", m, genie_vector_complex_add);
  a68_op (A68_EXT, "-", m, genie_vector_complex_sub);
  m = a68_proc (MODE (REF_ROW_COMPLEX), MODE (REF_ROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "+:=", m, genie_vector_complex_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_vector_complex_plusab);
  a68_op (A68_EXT, "-:=", m, genie_vector_complex_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_vector_complex_minusab);
  m = a68_proc (MODE (BOOL), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "=", m, genie_matrix_complex_eq);
  a68_op (A68_EXT, "/=", m, genie_matrix_complex_ne);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "+", m, genie_matrix_complex_add);
  a68_op (A68_EXT, "-", m, genie_matrix_complex_sub);
  m = a68_proc (MODE (REF_ROWROW_COMPLEX), MODE (REF_ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "+:=", m, genie_matrix_complex_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_matrix_complex_plusab);
  a68_op (A68_EXT, "-:=", m, genie_matrix_complex_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_matrix_complex_minusab);
/* Vector and matrix scaling */
  m = a68_proc (MODE (ROW_REAL), MODE (REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_real_scale_vector);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (REAL), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_scale_real);
  a68_op (A68_EXT, "/", m, genie_vector_div_real);
  m = a68_proc (MODE (ROWROW_REAL), MODE (REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_real_scale_matrix);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REAL), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_scale_real);
  a68_op (A68_EXT, "/", m, genie_matrix_div_real);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_complex_scale_vector_complex);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_complex_scale_complex);
  a68_op (A68_EXT, "/", m, genie_vector_complex_div_complex);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_complex_scale_matrix_complex);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_scale_complex);
  a68_op (A68_EXT, "/", m, genie_matrix_complex_div_complex);
  m = a68_proc (MODE (REF_ROW_REAL), MODE (REF_ROW_REAL), MODE (REAL), NO_MOID);
  a68_op (A68_EXT, "*:=", m, genie_vector_scale_real_ab);
  a68_op (A68_EXT, "/:=", m, genie_vector_div_real_ab);
  m = a68_proc (MODE (REF_ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (REAL), NO_MOID);
  a68_op (A68_EXT, "*:=", m, genie_matrix_scale_real_ab);
  a68_op (A68_EXT, "/:=", m, genie_matrix_div_real_ab);
  m = a68_proc (MODE (REF_ROW_COMPLEX), MODE (REF_ROW_COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*:=", m, genie_vector_complex_scale_complex_ab);
  a68_op (A68_EXT, "/:=", m, genie_vector_complex_div_complex_ab);
  m = a68_proc (MODE (REF_ROWROW_COMPLEX), MODE (REF_ROWROW_COMPLEX), MODE (COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*:=", m, genie_matrix_complex_scale_complex_ab);
  a68_op (A68_EXT, "/:=", m, genie_matrix_complex_div_complex_ab);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_times_matrix);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_complex_times_matrix);
/* Matrix times vector or matrix */
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_times_vector);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_times_matrix);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_times_vector);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_times_matrix);
/* Vector and matrix miscellaneous */
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "vectorecho", m, genie_vector_echo);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "matrixecho", m, genie_matrix_echo);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "complvectorecho", m, genie_vector_complex_echo);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "complmatrixecho", m, genie_matrix_complex_echo);
   /**/ 
  m = a68_proc (MODE (REAL), MODE (ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_dot);
  m = a68_proc (MODE (COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_complex_dot);
  m = a68_proc (MODE (REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "NORM", m, genie_vector_norm);
  m = a68_proc (MODE (REAL), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "NORM", m, genie_vector_complex_norm);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_op (A68_EXT, "DYAD", m, genie_vector_dyad);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_op (A68_EXT, "DYAD", m, genie_vector_complex_dyad);
  a68_prio ("DYAD", 3);
/* LU decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROW_INT), MODE (REF_INT), NO_MOID);
  a68_idf (A68_EXT, "ludecomp", m, genie_matrix_lu);
  m = a68_proc (MODE (REAL), MODE (ROWROW_REAL), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "ludet", m, genie_matrix_lu_det);
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_INT), NO_MOID);
  a68_idf (A68_EXT, "luinv", m, genie_matrix_lu_inv);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_INT), MODE (ROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "lusolve", m, genie_matrix_lu_solve);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (REF_ROW_INT), MODE (REF_INT), NO_MOID);
  a68_idf (A68_EXT, "complexludecomp", m, genie_matrix_complex_lu);
  m = a68_proc (MODE (COMPLEX), MODE (ROWROW_COMPLEX), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "complexludet", m, genie_matrix_complex_lu_det);
  m = a68_proc (MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_INT), NO_MOID);
  a68_idf (A68_EXT, "complexluinv", m, genie_matrix_complex_lu_inv);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROWROW_COMPLEX), MODE (ROW_INT), MODE (ROW_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "complexlusolve", m, genie_matrix_complex_lu_solve);
/* SVD decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROWROW_REAL), MODE (REF_ROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "svdecomp", m, genie_matrix_svd);
  a68_idf (A68_EXT, "svddecomp", m, genie_matrix_svd);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "svdsolve", m, genie_matrix_svd_solve);
/* QR decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), MODE (REF_ROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "qrdecomp", m, genie_matrix_qr);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "qrsolve", m, genie_matrix_qr_solve);
  a68_idf (A68_EXT, "qrlssolve", m, genie_matrix_qr_ls_solve);
/* Cholesky decomposition */
  m = a68_proc (MODE (ROWROW_REAL), MODE (ROWROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "choleskydecomp", m, genie_matrix_ch);
  m = a68_proc (MODE (ROW_REAL), MODE (ROWROW_REAL), MODE (ROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "choleskysolve", m, genie_matrix_ch_solve);
/* FFT */
  m = a68_proc (MODE (ROW_INT), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "primefactors", m, genie_prime_factors);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "fftcomplexforward", m, genie_fft_complex_forward);
  a68_idf (A68_EXT, "fftcomplexbackward", m, genie_fft_complex_backward);
  a68_idf (A68_EXT, "fftcomplexinverse", m, genie_fft_complex_inverse);
  m = a68_proc (MODE (ROW_COMPLEX), MODE (ROW_REAL), NO_MOID);
  a68_idf (A68_EXT, "fftforward", m, genie_fft_forward);
  m = a68_proc (MODE (ROW_REAL), MODE (ROW_COMPLEX), NO_MOID);
  a68_idf (A68_EXT, "fftbackward", m, genie_fft_backward);
  a68_idf (A68_EXT, "fftinverse", m, genie_fft_inverse);
#endif
/* UNIX things */
  m = proc_int;
  a68_idf (A68_EXT, "rows", m, genie_rows);
  a68_idf (A68_EXT, "columns", m, genie_columns);
  a68_idf (A68_EXT, "argc", m, genie_argc);
  a68_idf (A68_EXT, "errno", m, genie_errno);
  a68_idf (A68_EXT, "fork", m, genie_fork);
  m = a68_proc (MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "getpwd", m, genie_pwd);
  m = a68_proc (MODE (INT), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "setpwd", m, genie_cd);
  m = a68_proc (MODE (BOOL), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "fileisdirectory", m, genie_file_is_directory);
  a68_idf (A68_EXT, "fileisregular", m, genie_file_is_block_device);
  a68_idf (A68_EXT, "fileisregular", m, genie_file_is_char_device);
  a68_idf (A68_EXT, "fileisregular", m, genie_file_is_regular);
#if defined __S_IFIFO
  a68_idf (A68_EXT, "fileisfifo", m, genie_file_is_fifo);
#endif
#if defined __S_IFLNK
  a68_idf (A68_EXT, "fileislink", m, genie_file_is_link);
#endif
  m = a68_proc (MODE (BITS), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "filemode", m, genie_file_mode);
  m = a68_proc (MODE (STRING), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "argv", m, genie_argv);
  m = proc_void;
  a68_idf (A68_EXT, "reseterrno", m, genie_reset_errno);
  m = a68_proc (MODE (STRING), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "strerror", m, genie_strerror);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NO_MOID);
  a68_idf (A68_EXT, "execve", m, genie_execve);
  m = a68_proc (MODE (PIPE), NO_MOID);
  a68_idf (A68_EXT, "createpipe", m, genie_create_pipe);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NO_MOID);
  a68_idf (A68_EXT, "execvechild", m, genie_execve_child);
  m = a68_proc (MODE (PIPE), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), NO_MOID);
  a68_idf (A68_EXT, "execvechildpipe", m, genie_execve_child_pipe);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (ROW_STRING), MODE (ROW_STRING), MODE (REF_STRING), NO_MOID);
  a68_idf (A68_EXT, "execveoutput", m, genie_execve_output);
  m = a68_proc (MODE (STRING), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "getenv", m, genie_getenv);
  m = a68_proc (MODE (VOID), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "waitpid", m, genie_waitpid);
  m = a68_proc (MODE (ROW_INT), NO_MOID);
  a68_idf (A68_EXT, "utctime", m, genie_utctime);
  a68_idf (A68_EXT, "localtime", m, genie_localtime);
#if defined HAVE_DIRENT_H
  m = a68_proc (MODE (ROW_STRING), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "getdirectory", m, genie_directory);
#endif
#if defined HAVE_HTTP
  m = a68_proc (MODE (INT), MODE (REF_STRING), MODE (STRING), MODE (STRING), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "httpcontent", m, genie_http_content);
  a68_idf (A68_EXT, "tcprequest", m, genie_tcp_request);
#endif
#if defined HAVE_REGEX_H
  m = a68_proc (MODE (INT), MODE (STRING), MODE (STRING), MODE (REF_INT), MODE (REF_INT), NO_MOID);
  a68_idf (A68_EXT, "grepinstring", m, genie_grep_in_string);
  a68_idf (A68_EXT, "grepinsubstring", m, genie_grep_in_substring);
  m = a68_proc (MODE (INT), MODE (STRING), MODE (STRING), MODE (REF_STRING), NO_MOID);
  a68_idf (A68_EXT, "subinstring", m, genie_sub_in_string);
#endif
#if defined HAVE_CURSES
  m = proc_void;
  a68_idf (A68_EXT, "cursesstart", m, genie_curses_start);
  a68_idf (A68_EXT, "cursesend", m, genie_curses_end);
  a68_idf (A68_EXT, "cursesclear", m, genie_curses_clear);
  a68_idf (A68_EXT, "cursesrefresh", m, genie_curses_refresh);
  a68_idf (A68_EXT, "cursesgreen", m, genie_curses_green);
  a68_idf (A68_EXT, "cursescyan", m, genie_curses_cyan);
  a68_idf (A68_EXT, "cursesred", m, genie_curses_red);
  a68_idf (A68_EXT, "cursesyellow", m, genie_curses_yellow);
  a68_idf (A68_EXT, "cursesmagenta", m, genie_curses_magenta);
  a68_idf (A68_EXT, "cursesblue", m, genie_curses_blue);
  a68_idf (A68_EXT, "curseswhite", m, genie_curses_white);
  a68_idf (A68_EXT, "cursesgreeninverse", m, genie_curses_green_inverse);
  a68_idf (A68_EXT, "cursescyaninverse", m, genie_curses_cyan_inverse);
  a68_idf (A68_EXT, "cursesredinverse", m, genie_curses_red_inverse);
  a68_idf (A68_EXT, "cursesyellowinverse", m, genie_curses_yellow_inverse);
  a68_idf (A68_EXT, "cursesmagentainverse", m, genie_curses_magenta_inverse);
  a68_idf (A68_EXT, "cursesblueinverse", m, genie_curses_blue_inverse);
  a68_idf (A68_EXT, "curseswhiteinverse", m, genie_curses_white_inverse);
  m = proc_char;
  a68_idf (A68_EXT, "cursesgetchar", m, genie_curses_getchar);
  m = a68_proc (MODE (VOID), MODE (CHAR), NO_MOID);
  a68_idf (A68_EXT, "cursesputchar", m, genie_curses_putchar);
  m = a68_proc (MODE (VOID), MODE (INT), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "cursesmove", m, genie_curses_move);
  m = proc_int;
  a68_idf (A68_EXT, "curseslines", m, genie_curses_lines);
  a68_idf (A68_EXT, "cursescolumns", m, genie_curses_columns);
  m = a68_proc (MODE (BOOL), MODE (CHAR), NO_MOID);
  a68_idf (A68_EXT, "cursesdelchar", m, genie_curses_del_char);
#endif
#if HAVE_POSTGRESQL
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), MODE (REF_STRING), NO_MOID);
  a68_idf (A68_EXT, "pqconnectdb", m, genie_pq_connectdb);
  m = a68_proc (MODE (INT), MODE (REF_FILE), NO_MOID);
  a68_idf (A68_EXT, "pqfinish", m, genie_pq_finish);
  a68_idf (A68_EXT, "pqreset", m, genie_pq_reset);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (STRING), NO_MOID);
  a68_idf (A68_EXT, "pqparameterstatus", m, genie_pq_parameterstatus);
  a68_idf (A68_EXT, "pqexec", m, genie_pq_exec);
  a68_idf (A68_EXT, "pqfnumber", m, genie_pq_fnumber);
  m = a68_proc (MODE (INT), MODE (REF_FILE), NO_MOID);
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
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), NO_MOID);
  a68_idf (A68_EXT, "pqfname", m, genie_pq_fname);
  a68_idf (A68_EXT, "pqfformat", m, genie_pq_fformat);
  m = a68_proc (MODE (INT), MODE (REF_FILE), MODE (INT), MODE (INT), NO_MOID);
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
  proc_int = a68_proc (MODE (INT), NO_MOID);
  proc_real = a68_proc (MODE (REAL), NO_MOID);
  proc_real_real = MODE (PROC_REAL_REAL);
  proc_real_real_real = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), NO_MOID);
  proc_real_real_real_real = a68_proc (MODE (REAL), MODE (REAL), MODE (REAL), MODE (REAL), NO_MOID);
  proc_complex_complex = a68_proc (MODE (COMPLEX), MODE (COMPLEX), NO_MOID);
  proc_bool = a68_proc (MODE (BOOL), NO_MOID);
  proc_char = a68_proc (MODE (CHAR), NO_MOID);
  proc_void = a68_proc (MODE (VOID), NO_MOID);
  stand_prelude ();
  stand_transput ();
  stand_extensions ();
}

/*!
Standard prelude implementation, except transput.
*/

/*
This file contains Algol68G's standard environ. Transput routines are not here.
Some of the LONG operations are generic for LONG and LONG LONG.
This file contains calculus related routines from the C library and GNU
scientific library. When GNU scientific library is not installed then the
routines in this file will give a runtime error when called. You can also choose
to not have them defined in "prelude.c".
*/

double inverf (double);
double inverfc (double);

double cputime_0;

#define A68_MONAD(n, MODE, OP)\
void n (NODE_T * p) {\
  MODE *i;\
  POP_OPERAND_ADDRESS (p, i, MODE);\
  VALUE (i) = OP (VALUE (i));\
}

/*!
\brief PROC (PROC VOID) VOID on gc event
\param p position in tree
**/

void genie_on_gc_event (NODE_T * p)
{
  POP_PROCEDURE (p, &on_gc_event);
}

/*!
\brief generic procedure for OP AND BECOMES (+:=, -:=, ...)
\param p position in tree
\param ref mode of destination
\param f pointer to function that performs operation
**/

void genie_f_and_becomes (NODE_T * p, MOID_T * ref, GPROC * f)
{
  MOID_T *mode = SUB (ref);
  int size = MOID_SIZE (mode);
  BYTE_T *src = STACK_OFFSET (-size), *addr;
  A68_REF *dst = (A68_REF *) STACK_OFFSET (-size - A68_REF_SIZE);
  CHECK_REF (p, *dst, ref);
  addr = ADDRESS (dst);
  PUSH (p, addr, size);
  genie_check_initialisation (p, STACK_OFFSET (-size), mode);
  PUSH (p, src, size);
  (*f) (p);
  POP (p, addr, size);
  DECREMENT_STACK_POINTER (p, size);
}

/* Environment enquiries */

A68_ENV_INT (genie_int_lengths, 3)
A68_ENV_INT (genie_int_shorths, 1)
A68_ENV_INT (genie_real_lengths, 3)
A68_ENV_INT (genie_real_shorths, 1)
A68_ENV_INT (genie_complex_lengths, 3)
A68_ENV_INT (genie_complex_shorths, 1)
A68_ENV_INT (genie_bits_lengths, 3)
A68_ENV_INT (genie_bits_shorths, 1)
A68_ENV_INT (genie_bytes_lengths, 2)
A68_ENV_INT (genie_bytes_shorths, 1)
A68_ENV_INT (genie_int_width, INT_WIDTH)
A68_ENV_INT (genie_long_int_width, LONG_INT_WIDTH)
A68_ENV_INT (genie_longlong_int_width, LONGLONG_INT_WIDTH)
A68_ENV_INT (genie_real_width, REAL_WIDTH)
A68_ENV_INT (genie_long_real_width, LONG_REAL_WIDTH)
A68_ENV_INT (genie_longlong_real_width, LONGLONG_REAL_WIDTH)
A68_ENV_INT (genie_exp_width, EXP_WIDTH)
A68_ENV_INT (genie_long_exp_width, LONG_EXP_WIDTH)
A68_ENV_INT (genie_longlong_exp_width, LONGLONG_EXP_WIDTH)
A68_ENV_INT (genie_bits_width, BITS_WIDTH)
A68_ENV_INT (genie_long_bits_width, get_mp_bits_width (MODE (LONG_BITS)))
A68_ENV_INT (genie_longlong_bits_width, get_mp_bits_width (MODE (LONGLONG_BITS)))
A68_ENV_INT (genie_bytes_width, BYTES_WIDTH)
A68_ENV_INT (genie_long_bytes_width, LONG_BYTES_WIDTH)
A68_ENV_INT (genie_max_abs_char, UCHAR_MAX)
A68_ENV_INT (genie_max_int, A68_MAX_INT)
A68_ENV_REAL (genie_max_real, DBL_MAX)
A68_ENV_REAL (genie_min_real, DBL_MIN)
A68_ENV_REAL (genie_small_real, DBL_EPSILON)
A68_ENV_REAL (genie_pi, A68_PI)
A68_ENV_REAL (genie_cputime, seconds () - cputime_0)
A68_ENV_INT (genie_stack_pointer, stack_pointer)
A68_ENV_INT (genie_system_stack_size, stack_size)

/*!
\brief INT system stack pointer
\param p position in tree
**/

void genie_system_stack_pointer (NODE_T * p)
{
  BYTE_T stack_offset;
  PUSH_PRIMITIVE (p, (int) (system_stack_offset - &stack_offset), A68_INT);
}

/*!
\brief LONG INT max long int
\param p position in tree
**/

void genie_long_max_int (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_INT));
  MP_T *z;
  int k, j = 1 + digits;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) (digits - 1);
  for (k = 2; k <= j; k++) {
    z[k] = (MP_T) (MP_RADIX - 1);
  }
}

/*!
\brief LONG LONG INT max long long int
\param p position in tree
**/

void genie_longlong_max_int (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONGLONG_INT));
  MP_T *z;
  int k, j = 1 + digits;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) (digits - 1);
  for (k = 2; k <= j; k++) {
    z[k] = (MP_T) (MP_RADIX - 1);
  }
}

/*!
\brief LONG REAL max long real
\param p position in tree
**/

void genie_long_max_real (NODE_T * p)
{
  int j, digits = get_mp_digits (MODE (LONG_REAL));
  MP_T *z;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) (MAX_MP_EXPONENT - 1);
  for (j = 2; j <= 1 + digits; j++) {
    z[j] = (MP_T) (MP_RADIX - 1);
  }
}

/*!
\brief LONG LONG REAL max long long real
\param p position in tree
**/

void genie_longlong_max_real (NODE_T * p)
{
  int j, digits = get_mp_digits (MODE (LONGLONG_REAL));
  MP_T *z;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) (MAX_MP_EXPONENT - 1);
  for (j = 2; j <= 1 + digits; j++) {
    z[j] = (MP_T) (MP_RADIX - 1);
  }
}

/*!
\brief LONG REAL min long real
\param p position in tree
**/

void genie_long_min_real (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL));
  MP_T *z;
  STACK_MP (z, p, digits);
  SET_MP_ZERO (z, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) -(MAX_MP_EXPONENT);
  MP_DIGIT (z, 1) = (MP_T) 1;
}

/*!
\brief LONG LONG REAL min long long real
\param p position in tree
**/

void genie_longlong_min_real (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONGLONG_REAL));
  MP_T *z;
  STACK_MP (z, p, digits);
  SET_MP_ZERO (z, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) -(MAX_MP_EXPONENT);
  MP_DIGIT (z, 1) = (MP_T) 1;
}

/*!
\brief LONG REAL small long real
\param p position in tree
**/

void genie_long_small_real (NODE_T * p)
{
  int j, digits = get_mp_digits (MODE (LONG_REAL));
  MP_T *z;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) -(digits - 1);
  MP_DIGIT (z, 1) = (MP_T) 1;
  for (j = 3; j <= 1 + digits; j++) {
    z[j] = (MP_T) 0;
  }
}

/*!
\brief LONG LONG REAL small long long real
\param p position in tree
**/

void genie_longlong_small_real (NODE_T * p)
{
  int j, digits = get_mp_digits (MODE (LONGLONG_REAL));
  MP_T *z;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) -(digits - 1);
  MP_DIGIT (z, 1) = (MP_T) 1;
  for (j = 3; j <= 1 + digits; j++) {
    z[j] = (MP_T) 0;
  }
}

/*!
\brief BITS max bits
\param p position in tree
**/

void genie_max_bits (NODE_T * p)
{
  PUSH_PRIMITIVE (p, A68_MAX_BITS, A68_BITS);
}

/*!
\brief LONG BITS long max bits
\param p position in tree
**/

void genie_long_max_bits (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_BITS));
  int width = get_mp_bits_width (MODE (LONG_BITS));
  ADDR_T pop_sp;
  MP_T *z, *one;
  STACK_MP (z, p, digits);
  pop_sp = stack_pointer;
  STACK_MP (one, p, digits);
  (void) set_mp_short (z, (MP_T) 2, 0, digits);
  (void) set_mp_short (one, (MP_T) 1, 0, digits);
  (void) pow_mp_int (p, z, z, width, digits);
  (void) sub_mp (p, z, z, one, digits);
  stack_pointer = pop_sp;
}

/*!
\brief LONG LONG BITS long long max bits
\param p position in tree
**/

void genie_longlong_max_bits (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONGLONG_BITS));
  int width = get_mp_bits_width (MODE (LONGLONG_BITS));
  ADDR_T pop_sp;
  MP_T *z, *one;
  STACK_MP (z, p, digits);
  pop_sp = stack_pointer;
  STACK_MP (one, p, digits);
  (void) set_mp_short (z, (MP_T) 2, 0, digits);
  (void) set_mp_short (one, (MP_T) 1, 0, digits);
  (void) pow_mp_int (p, z, z, width, digits);
  (void) sub_mp (p, z, z, one, digits);
  stack_pointer = pop_sp;
}

/*!
\brief LONG REAL long pi
\param p position in tree
**/

void genie_pi_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p));
  MP_T *z;
  STACK_MP (z, p, digits);
  (void) mp_pi (p, z, MP_PI, digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

/* BOOL operations */

/* OP NOT = (BOOL) BOOL */

A68_MONAD (genie_not_bool, A68_BOOL, (BOOL_T) !)

/*!
\brief OP ABS = (BOOL) INT
\param p position in tree
**/

void genie_abs_bool (NODE_T * p)
{
  A68_BOOL j;
  POP_OBJECT (p, &j, A68_BOOL);
  PUSH_PRIMITIVE (p, (VALUE (&j) ? 1 : 0), A68_INT);
}

#define A68_BOOL_DYAD(n, OP)\
void n (NODE_T * p) {\
  A68_BOOL *i, *j;\
  POP_OPERAND_ADDRESSES (p, i, j, A68_BOOL);\
  VALUE (i) = (BOOL_T) (VALUE (i) OP VALUE (j));\
}

A68_BOOL_DYAD (genie_and_bool, &)
A68_BOOL_DYAD (genie_or_bool, |)
A68_BOOL_DYAD (genie_xor_bool, ^)
A68_BOOL_DYAD (genie_eq_bool, ==)
A68_BOOL_DYAD (genie_ne_bool, !=)

/* INT operations */
/* OP - = (INT) INT */

A68_MONAD (genie_minus_int, A68_INT, -)

/*!
\brief OP ABS = (INT) INT
\param p position in tree
**/
void genie_abs_int (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = ABS (VALUE (j));
}

/*!
\brief OP SIGN = (INT) INT
\param p position in tree
**/

void genie_sign_int (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = SIGN (VALUE (j));
}

/*!
\brief OP ODD = (INT) INT
\param p position in tree
**/

void genie_odd_int (NODE_T * p)
{
  A68_INT j;
  POP_OBJECT (p, &j, A68_INT);
  PUSH_PRIMITIVE (p, (BOOL_T) ((VALUE (&j) >= 0 ? VALUE (&j) : -VALUE (&j)) % 2 == 1), A68_BOOL);
}

/*!
\brief OP + = (INT, INT) INT
\param p position in tree
**/

void genie_add_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  CHECK_INT_ADDITION (p, VALUE (i), VALUE (j));
  VALUE (i) += VALUE (j);
}

/*!
\brief OP - = (INT, INT) INT
\param p position in tree
**/

void genie_sub_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  CHECK_INT_SUBTRACTION (p, VALUE (i), VALUE (j));
  VALUE (i) -= VALUE (j);
}

/*!
\brief OP * = (INT, INT) INT
\param p position in tree
**/

void genie_mul_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  CHECK_INT_MULTIPLICATION (p, VALUE (i), VALUE (j));
  VALUE (i) *= VALUE (j);
}

/*!
\brief OP OVER = (INT, INT) INT
\param p position in tree
**/

void genie_over_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  PRELUDE_ERROR (VALUE (j) == 0, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
  VALUE (i) /= VALUE (j);
}

/*!
\brief OP MOD = (INT, INT) INT
\param p position in tree
**/

void genie_mod_int (NODE_T * p)
{
  A68_INT *i, *j;
  int k;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  PRELUDE_ERROR (VALUE (j) == 0, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
  k = VALUE (i) % VALUE (j);
  if (k < 0) {
    k += (VALUE (j) >= 0 ? VALUE (j) : -VALUE (j));
  }
  VALUE (i) = k;
}

/*!
\brief OP / = (INT, INT) REAL
\param p position in tree
**/

void genie_div_int (NODE_T * p)
{
  A68_INT i, j;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&j) == 0, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
  PUSH_PRIMITIVE (p, (double) (VALUE (&i)) / (double) (VALUE (&j)), A68_REAL);
}

/*!
\brief OP ** = (INT, INT) INT
\param p position in tree
**/

void genie_pow_int (NODE_T * p)
{
  A68_INT i, j;
  int expo, mult, prod;
  POP_OBJECT (p, &j, A68_INT);
  PRELUDE_ERROR (VALUE (&j) < 0, p, ERROR_EXPONENT_INVALID, MODE (INT));
  POP_OBJECT (p, &i, A68_INT);
  prod = 1;
  mult = VALUE (&i);
  expo = 1;
  while ((unsigned) expo <= (unsigned) (VALUE (&j))) {
    if (VALUE (&j) & expo) {
      CHECK_INT_MULTIPLICATION (p, prod, mult);
      prod *= mult;
    }
    expo <<= 1;
    if (expo <= VALUE (&j)) {
      CHECK_INT_MULTIPLICATION (p, mult, mult);
      mult *= mult;
    }
  }
  PUSH_PRIMITIVE (p, prod, A68_INT);
}

/* OP (INT, INT) BOOL */

#define A68_CMP_INT(n, OP)\
void n (NODE_T * p) {\
  A68_INT i, j;\
  POP_OBJECT (p, &j, A68_INT);\
  POP_OBJECT (p, &i, A68_INT);\
  PUSH_PRIMITIVE (p, (BOOL_T) (VALUE (&i) OP VALUE (&j)), A68_BOOL);\
  }

A68_CMP_INT (genie_eq_int, ==)
A68_CMP_INT (genie_ne_int, !=)
A68_CMP_INT (genie_lt_int, <)
A68_CMP_INT (genie_gt_int, >)
A68_CMP_INT (genie_le_int, <=)
A68_CMP_INT (genie_ge_int, >=)

/*!
\brief OP +:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_plusab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_add_int);
}

/*!
\brief OP -:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_minusab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_sub_int);
}

/*!
\brief OP *:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_timesab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_mul_int);
}

/*!
\brief OP %:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_overab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_over_int);
}

/*!
\brief OP %*:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_modab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_mod_int);
}

/*!
\brief OP LENG = (INT) LONG INT
\param p position in tree
**/

void genie_lengthen_int_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_INT));
  MP_T *z;
  A68_INT k;
  POP_OBJECT (p, &k, A68_INT);
  STACK_MP (z, p, digits);
  (void) int_to_mp (p, z, VALUE (&k), digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

/*!
\brief OP LENG = (BITS) LONG BITS
\param p position in tree
**/

void genie_lengthen_unsigned_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_INT));
  MP_T *z;
  A68_BITS k;
  POP_OBJECT (p, &k, A68_BITS);
  STACK_MP (z, p, digits);
  (void) unsigned_to_mp (p, z, (unsigned) VALUE (&k), digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

/*!
\brief OP SHORTEN = (LONG INT) INT
\param p position in tree
**/

void genie_shorten_long_mp_to_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *z;
  DECREMENT_STACK_POINTER (p, size);
  z = (MP_T *) STACK_TOP;
  MP_STATUS (z) = (MP_T) INIT_MASK;
  PUSH_PRIMITIVE (p, mp_to_int (p, z, digits), A68_INT);
}

/*!
\brief OP ODD = (LONG INT) BOOL
\param p position in tree
**/

void genie_odd_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  if (MP_EXPONENT (z) <= (MP_T) (digits - 1)) {
    PUSH_PRIMITIVE (p, (BOOL_T) ((int) (z[(int) (2 + MP_EXPONENT (z))]) % 2 != 0), A68_BOOL);
  } else {
    PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
  }
}

/*!
\brief test whether z is a valid LONG INT
\param p position in tree
\param z mp number
\param m mode associated with z
**/

void test_long_int_range (NODE_T * p, MP_T * z, MOID_T * m)
{
  PRELUDE_ERROR (!check_mp_int (z, m), p, ERROR_OUT_OF_BOUNDS, m);
}

/*!
\brief OP + = (LONG INT, LONG INT) LONG INT
\param p position in tree
**/

void genie_add_long_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) add_mp (p, x, x, y, digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP - = (LONG INT, LONG INT) LONG INT
\param p position in tree
**/

void genie_sub_long_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) sub_mp (p, x, x, y, digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP * = (LONG INT, LONG INT) LONG INT
\param p position in tree
**/

void genie_mul_long_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) mul_mp (p, x, x, y, digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP ** = (LONG MODE, INT) LONG INT
\param p position in tree
**/

void genie_pow_long_mp_int_int (NODE_T * p)
{
  MOID_T *m = LHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  A68_INT k;
  MP_T *x;
  POP_OBJECT (p, &k, A68_INT);
  x = (MP_T *) STACK_OFFSET (-size);
  (void) pow_mp_int (p, x, x, VALUE (&k), digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief OP +:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in tree
**/

void genie_plusab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_int);
}

/*!
\brief OP -:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in tree
**/

void genie_minusab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_long_int);
}

/*!
\brief OP *:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in tree
**/

void genie_timesab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_int);
}

/* REAL operations. REAL math is in gsl.c */

/* OP - = (REAL) REAL */

A68_MONAD (genie_minus_real, A68_REAL, -)

/*!
\brief OP ABS = (REAL) REAL
\param p position in tree
**/

void genie_abs_real (NODE_T * p)
{
  A68_REAL *x;
  POP_OPERAND_ADDRESS (p, x, A68_REAL);
  VALUE (x) = ABS (VALUE (x));
}

/*!
\brief OP ROUND = (REAL) INT
\param p position in tree
**/

void genie_round_real (NODE_T * p)
{
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  PRELUDE_ERROR (VALUE (&x) < -(double) A68_MAX_INT || VALUE (&x) > (double) A68_MAX_INT, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  PUSH_PRIMITIVE (p, a68g_round (VALUE (&x)), A68_INT);
}

/*!
\brief OP ENTIER = (REAL) INT
\param p position in tree
**/

void genie_entier_real (NODE_T * p)
{
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  PRELUDE_ERROR (VALUE (&x) < -(double) A68_MAX_INT || VALUE (&x) > (double)A68_MAX_INT, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  PUSH_PRIMITIVE (p, (int) floor (VALUE (&x)), A68_INT);
}

/*!
\brief OP SIGN = (REAL) INT
\param p position in tree
**/

void genie_sign_real (NODE_T * p)
{
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  PUSH_PRIMITIVE (p, SIGN (VALUE (&x)), A68_INT);
}

/*!
\brief OP + = (REAL, REAL) REAL
\param p position in tree
**/

void genie_add_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  VALUE (x) += VALUE (y);
  CHECK_REAL_REPRESENTATION (p, VALUE (x));
}

/*!
\brief OP - = (REAL, REAL) REAL
\param p position in tree
**/

void genie_sub_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  VALUE (x) -= VALUE (y);
  CHECK_REAL_REPRESENTATION (p, VALUE (x));
}

/*!
\brief OP * = (REAL, REAL) REAL
\param p position in tree
**/

void genie_mul_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  VALUE (x) *= VALUE (y);
  CHECK_REAL_REPRESENTATION (p, VALUE (x));
}

/*!
\brief OP / = (REAL, REAL) REAL
\param p position in tree
**/

void genie_div_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  PRELUDE_ERROR (VALUE (y) == 0.0, p, ERROR_DIVISION_BY_ZERO, MODE (REAL));
  VALUE (x) /= VALUE (y);
}

/*!
\brief OP ** = (REAL, INT) REAL
\param p position in tree
**/

void genie_pow_real_int (NODE_T * p)
{
  A68_INT j;
  A68_REAL x;
  int expo;
  double mult, prod;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  negative = (BOOL_T) (VALUE (&j) < 0);
  VALUE (&j) = (VALUE (&j) >= 0 ? VALUE (&j) : -VALUE (&j));
  POP_OBJECT (p, &x, A68_REAL);
  prod = 1;
  mult = VALUE (&x);
  expo = 1;
  while ((unsigned) expo <= (unsigned) (VALUE (&j))) {
    if (VALUE (&j) & expo) {
      CHECK_REAL_MULTIPLICATION (p, prod, mult);
      prod *= mult;
    }
    expo <<= 1;
    if (expo <= VALUE (&j)) {
      CHECK_REAL_MULTIPLICATION (p, mult, mult);
      mult *= mult;
    }
  }
  CHECK_REAL_REPRESENTATION (p, prod);
  if (negative) {
    prod = 1.0 / prod;
  }
  PUSH_PRIMITIVE (p, prod, A68_REAL);
}

/*!
\brief OP ** = (REAL, REAL) REAL
\param p position in tree
**/

void genie_pow_real (NODE_T * p)
{
  A68_REAL x, y;
  double z = 0;
  POP_OBJECT (p, &y, A68_REAL);
  POP_OBJECT (p, &x, A68_REAL);
  RESET_ERRNO;
  PRELUDE_ERROR (VALUE (&x) < 0.0, p, ERROR_INVALID_ARGUMENT, MODE (REAL));
  if (VALUE (&x) == 0.0) {
    if (VALUE (&y) < 0) {
      errno = ERANGE;
      MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);
    } else {
      z = (VALUE (&y) == 0.0 ? 1.0 : 0.0);
    }
  } else {
    z = exp (VALUE (&y) * log (VALUE (&x)));
    MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);
  }
  PUSH_PRIMITIVE (p, z, A68_REAL);
}

/* OP (REAL, REAL) BOOL */

#define A68_CMP_REAL(n, OP)\
void n (NODE_T * p) {\
  A68_REAL i, j;\
  POP_OBJECT (p, &j, A68_REAL);\
  POP_OBJECT (p, &i, A68_REAL);\
  PUSH_PRIMITIVE (p, (BOOL_T) (VALUE (&i) OP VALUE (&j)), A68_BOOL);\
  }

A68_CMP_REAL (genie_eq_real, ==)
A68_CMP_REAL (genie_ne_real, !=)
A68_CMP_REAL (genie_lt_real, <)
A68_CMP_REAL (genie_gt_real, >)
A68_CMP_REAL (genie_le_real, <=)
A68_CMP_REAL (genie_ge_real, >=)

/*!
\brief OP +:= = (REF REAL, REAL) REF REAL
\param p position in tree
**/

void genie_plusab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_add_real);
}

/*!
\brief OP -:= = (REF REAL, REAL) REF REAL
\param p position in tree
**/

void genie_minusab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_sub_real);
}

/*!
\brief OP *:= = (REF REAL, REAL) REF REAL
\param p position in tree
**/

void genie_timesab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_mul_real);
}

/*!
\brief OP /:= = (REF REAL, REAL) REF REAL
\param p position in tree
**/

void genie_divab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_div_real);
}

/*!
\brief OP LENG = (REAL) LONG REAL
\param p position in tree
**/

void genie_lengthen_real_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL));
  MP_T *z;
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  STACK_MP (z, p, digits);
  (void) real_to_mp (p, z, VALUE (&x), digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

/*!
\brief OP SHORTEN = (LONG REAL) REAL
\param p position in tree
**/

void genie_shorten_long_mp_to_real (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *z;
  DECREMENT_STACK_POINTER (p, size);
  z = (MP_T *) STACK_TOP;
  MP_STATUS (z) = (MP_T) INIT_MASK;
  PUSH_PRIMITIVE (p, mp_to_real (p, z, digits), A68_REAL);
}

/*!
\brief OP ROUND = (LONG REAL) LONG INT
\param p position in tree
**/

void genie_round_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  (void) round_mp (p, z, z, digits);
  stack_pointer = pop_sp;
}

/*!
\brief OP ENTIER = (LONG REAL) LONG INT
\param p position in tree
**/

void genie_entier_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (LHS_MODE (p)), size = get_mp_size (LHS_MODE (p));
  ADDR_T pop_sp = stack_pointer;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  (void) entier_mp (p, z, z, digits);
  stack_pointer = pop_sp;
}

/*!
\brief PROC long sqrt = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_sqrt_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (sqrt_mp (p, x, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long curt = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_curt_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (curt_mp (p, x, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long exp = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_exp_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) exp_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief PROC long ln = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_ln_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (ln_mp (p, x, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  MP_STATUS (x) = (MP_T) INIT_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief PROC long log = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_log_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (log_mp (p, x, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  MP_STATUS (x) = (MP_T) INIT_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief PROC long sinh = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_sinh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) sinh_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long cosh = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_cosh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) cosh_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long tanh = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_tanh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) tanh_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long arcsinh = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_arcsinh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) asinh_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long arccosh = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_arccosh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) acosh_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long arctanh = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_arctanh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) atanh_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long sin = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_sin_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) sin_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long cos = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_cos_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) cos_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long tan = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_tan_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (tan_mp (p, x, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long arcsin = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_asin_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (asin_mp (p, x, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long arccos = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_acos_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (acos_mp (p, x, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long arctan = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_atan_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  (void) atan_mp (p, x, x, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief PROC long arctan2 = (LONG REAL, LONG REAL) LONG REAL
\param p position in tree
**/

void genie_atan2_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  stack_pointer -= size;
  PRELUDE_ERROR (atan2_mp (p, x, y, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/* Arithmetic operations */

/*!
\brief OP LENG = (LONG MODE) LONG LONG MODE
\param p position in tree
**/

void genie_lengthen_long_mp_to_longlong_mp (NODE_T * p)
{
  MP_T *z;
  DECREMENT_STACK_POINTER (p, (int) size_long_mp ());
  STACK_MP (z, p, longlong_mp_digits ());
  (void) lengthen_mp (p, z, longlong_mp_digits (), z, long_mp_digits ());
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

/*!
\brief OP SHORTEN = (LONG LONG MODE) LONG MODE
\param p position in tree
**/

void genie_shorten_longlong_mp_to_long_mp (NODE_T * p)
{
  MP_T *z;
  MOID_T *m = SUB_MOID (p);
  DECREMENT_STACK_POINTER (p, (int) size_longlong_mp ());
  STACK_MP (z, p, long_mp_digits ());
  if (m == MODE (LONG_INT)) {
    PRELUDE_ERROR (MP_EXPONENT (z) > LONG_MP_DIGITS - 1, p, ERROR_OUT_OF_BOUNDS, m);
  }
  (void) shorten_mp (p, z, long_mp_digits (), z, longlong_mp_digits ());
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

/*!
\brief OP - = (LONG MODE) LONG MODE
\param p position in tree
**/

void genie_minus_long_mp (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
}

/*!
\brief OP ABS = (LONG MODE) LONG MODE
\param p position in tree
**/

void genie_abs_long_mp (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_DIGIT (z, 1) = ABS (MP_DIGIT (z, 1));
}

/*!
\brief OP SIGN = (LONG MODE) INT
\param p position in tree
**/

void genie_sign_long_mp (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_PRIMITIVE (p, SIGN (MP_DIGIT (z, 1)), A68_INT);
}

/*!
\brief OP + = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_add_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) add_mp (p, x, x, y, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP - = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_sub_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) sub_mp (p, x, x, y, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP * = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_mul_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) mul_mp (p, x, x, y, digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP / = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_div_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (div_mp (p, x, x, y, digits) == NO_MP, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_REAL));
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP % = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_over_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (over_mp (p, x, x, y, digits) == NO_MP, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_INT));
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP %* = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_mod_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (mod_mp (p, x, x, y, digits) == NO_MP, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_INT));
  if (MP_DIGIT (x, 1) < 0) {
    MP_DIGIT (y, 1) = ABS (MP_DIGIT (y, 1));
   (void) add_mp (p, x, x, y, digits);
  }
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP +:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_plusab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_mp);
}

/*!
\brief OP -:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_minusab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_long_mp);
}

/*!
\brief OP *:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_timesab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_mp);
}

/*!
\brief OP /:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_divab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_div_long_mp);
}

/*!
\brief OP %:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_overab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_over_long_mp);
}

/*!
\brief OP %*:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_modab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mod_long_mp);
}

/* OP (LONG MODE, LONG MODE) BOOL */

#define A68_CMP_LONG(n, OP)\
void n (NODE_T * p) {\
  MOID_T *mode = LHS_MODE (p);\
  A68_BOOL z;\
  int digits = get_mp_digits (mode), size = get_mp_size (mode);\
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);\
  MP_T *y = (MP_T *) STACK_OFFSET (-size);\
  OP (p, &z, x, y, digits);\
  DECREMENT_STACK_POINTER (p, 2 * size);\
  PUSH_PRIMITIVE (p, VALUE (&z), A68_BOOL);\
}

A68_CMP_LONG (genie_eq_long_mp, eq_mp)
A68_CMP_LONG (genie_ne_long_mp, ne_mp)
A68_CMP_LONG (genie_lt_long_mp, lt_mp)
A68_CMP_LONG (genie_gt_long_mp, gt_mp)
A68_CMP_LONG (genie_le_long_mp, le_mp)
A68_CMP_LONG (genie_ge_long_mp, ge_mp)

/*!
\brief OP ** = (LONG MODE, INT) LONG MODE
\param p position in tree
**/

void genie_pow_long_mp_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  A68_INT k;
  MP_T *x;
  POP_OBJECT (p, &k, A68_INT);
  x = (MP_T *) STACK_OFFSET (-size);
  (void) pow_mp_int (p, x, x, VALUE (&k), digits);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/*!
\brief OP ** = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_pow_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  MP_T *z;
  STACK_MP (z, p, digits);
  if (IS_ZERO_MP (x)) {
    if (MP_DIGIT (y, 1) < (MP_T) 0) {
      PRELUDE_ERROR (A68_TRUE, p, ERROR_INVALID_ARGUMENT, MOID (p));
    } else if (IS_ZERO_MP (y)) {
      (void) set_mp_short (x, (MP_T) 1, 0, digits);
    }
  } else {
    PRELUDE_ERROR (ln_mp (p, z, x, digits) == NO_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
    (void) mul_mp (p, z, y, z, digits);
    (void) exp_mp (p, x, z, digits);
  }
  stack_pointer = pop_sp - size;
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

/* Character operations */

/* OP (CHAR, CHAR) BOOL */

#define A68_CMP_CHAR(n, OP)\
void n (NODE_T * p) {\
  A68_CHAR i, j;\
  POP_OBJECT (p, &j, A68_CHAR);\
  POP_OBJECT (p, &i, A68_CHAR);\
  PUSH_PRIMITIVE (p, (BOOL_T) (TO_UCHAR (VALUE (&i)) OP TO_UCHAR (VALUE (&j))), A68_BOOL);\
  }

A68_CMP_CHAR (genie_eq_char, ==)
A68_CMP_CHAR (genie_ne_char, !=)
A68_CMP_CHAR (genie_lt_char, <)
A68_CMP_CHAR (genie_gt_char, >)
A68_CMP_CHAR (genie_le_char, <=)
A68_CMP_CHAR (genie_ge_char, >=)

/*!
\brief OP ABS = (CHAR) INT
\param p position in tree
**/

void genie_abs_char (NODE_T * p)
{
  A68_CHAR i;
  POP_OBJECT (p, &i, A68_CHAR);
  PUSH_PRIMITIVE (p, TO_UCHAR (VALUE (&i)), A68_INT);
}

/*!
\brief OP REPR = (INT) CHAR
\param p position in tree
**/

void genie_repr_char (NODE_T * p)
{
  A68_INT k;
  POP_OBJECT (p, &k, A68_INT);
  PRELUDE_ERROR (VALUE (&k) < 0 || VALUE (&k) > (int) UCHAR_MAX, p, ERROR_OUT_OF_BOUNDS, MODE (CHAR));
  PUSH_PRIMITIVE (p, (char) (VALUE (&k)), A68_CHAR);
}

/* OP (CHAR) BOOL */

#define A68_CHAR_BOOL(n, OP)\
void n (NODE_T * p) {\
  A68_CHAR ch;\
  POP_OBJECT (p, &ch, A68_CHAR);\
  PUSH_PRIMITIVE (p, (BOOL_T) (OP (VALUE (&ch)) == 0 ? A68_FALSE : A68_TRUE), A68_BOOL);\
  }

A68_CHAR_BOOL (genie_is_alnum, IS_ALNUM)
A68_CHAR_BOOL (genie_is_alpha, IS_ALPHA)
A68_CHAR_BOOL (genie_is_cntrl, IS_CNTRL)
A68_CHAR_BOOL (genie_is_digit, IS_DIGIT)
A68_CHAR_BOOL (genie_is_graph, IS_GRAPH)
A68_CHAR_BOOL (genie_is_lower, IS_LOWER)
A68_CHAR_BOOL (genie_is_print, IS_PRINT)
A68_CHAR_BOOL (genie_is_punct, IS_PUNCT)
A68_CHAR_BOOL (genie_is_space, IS_SPACE)
A68_CHAR_BOOL (genie_is_upper, IS_UPPER)
A68_CHAR_BOOL (genie_is_xdigit, IS_XDIGIT)

#define A68_CHAR_CHAR(n, OP)\
void n (NODE_T * p) {\
  A68_CHAR *ch;\
  POP_OPERAND_ADDRESS (p, ch, A68_CHAR);\
  VALUE (ch) = (char) (OP (TO_UCHAR (VALUE (ch))));\
}

A68_CHAR_CHAR (genie_to_lower, TO_LOWER)
A68_CHAR_CHAR (genie_to_upper, TO_UPPER)

/*!
\brief OP + = (CHAR, CHAR) STRING
\param p position in tree
**/

void genie_add_char (NODE_T * p)
{
  A68_CHAR a, b;
  A68_REF c, d;
  A68_ARRAY *a_3;
  A68_TUPLE *t_3;
  BYTE_T *b_3;
/* right part */
  POP_OBJECT (p, &b, A68_CHAR);
  CHECK_INIT (p, INITIALISED (&b), MODE (CHAR));
/* left part */
  POP_OBJECT (p, &a, A68_CHAR);
  CHECK_INIT (p, INITIALISED (&a), MODE (CHAR));
/* sum */
  c = heap_generator (p, MODE (STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  d = heap_generator (p, MODE (STRING), 2 * ALIGNED_SIZE_OF (A68_CHAR));
  GET_DESCRIPTOR (a_3, t_3, &c);
  DIM (a_3) = 1;
  MOID (a_3) = MODE (CHAR);
  ELEM_SIZE (a_3) = ALIGNED_SIZE_OF (A68_CHAR);
  SLICE_OFFSET (a_3) = 0;
  FIELD_OFFSET (a_3) = 0;
  ARRAY (a_3) = d;
  LWB (t_3) = 1;
  UPB (t_3) = 2;
  SHIFT (t_3) = LWB (t_3);
  SPAN (t_3) = 1;
/* add chars */
  b_3 = DEREF (BYTE_T, &ARRAY (a_3));
  MOVE ((BYTE_T *) & b_3[0], (BYTE_T *) & a, ALIGNED_SIZE_OF (A68_CHAR));
  MOVE ((BYTE_T *) & b_3[ALIGNED_SIZE_OF (A68_CHAR)], (BYTE_T *) & b, ALIGNED_SIZE_OF (A68_CHAR));
  PUSH_REF (p, c);
}

/*!
\brief OP ELEM = (INT, STRING) CHAR # ALGOL68C #
\param p position in tree
**/

void genie_elem_string (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *a;
  A68_TUPLE *t;
  A68_INT k;
  BYTE_T *base;
  A68_CHAR *ch;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (STRING));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (a, t, &z);
  PRELUDE_ERROR (VALUE (&k) < LWB (t), p, ERROR_INDEX_OUT_OF_BOUNDS, NO_TEXT);
  PRELUDE_ERROR (VALUE (&k) > UPB (t), p, ERROR_INDEX_OUT_OF_BOUNDS, NO_TEXT);
  base = DEREF (BYTE_T, &(ARRAY (a)));
  ch = (A68_CHAR *) & (base[INDEX_1_DIM (a, t, VALUE (&k))]);
  PUSH_PRIMITIVE (p, VALUE (ch), A68_CHAR);
}

/*!
\brief OP + = (STRING, STRING) STRING
\param p position in tree
**/

void genie_add_string (NODE_T * p)
{
  A68_REF a, b, c, d;
  A68_ARRAY *a_1, *a_2, *a_3;
  A68_TUPLE *t_1, *t_2, *t_3;
  int l_1, l_2, k, m;
  BYTE_T *b_1, *b_2, *b_3;
/* right part */
  POP_REF (p, &b);
  CHECK_INIT (p, INITIALISED (&b), MODE (STRING));
  GET_DESCRIPTOR (a_2, t_2, &b);
  l_2 = ROW_SIZE (t_2);
/* left part */
  POP_REF (p, &a);
  CHECK_REF (p, a, MODE (STRING));
  GET_DESCRIPTOR (a_1, t_1, &a);
  l_1 = ROW_SIZE (t_1);
/* sum */
  c = heap_generator (p, MODE (STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  d = heap_generator (p, MODE (STRING), (l_1 + l_2) * ALIGNED_SIZE_OF (A68_CHAR));
/* Calculate again since garbage collector might have moved data */
  GET_DESCRIPTOR (a_1, t_1, &a);
  GET_DESCRIPTOR (a_2, t_2, &b);
  GET_DESCRIPTOR (a_3, t_3, &c);
  DIM (a_3) = 1;
  MOID (a_3) = MODE (CHAR);
  ELEM_SIZE (a_3) = ALIGNED_SIZE_OF (A68_CHAR);
  SLICE_OFFSET (a_3) = 0;
  FIELD_OFFSET (a_3) = 0;
  ARRAY (a_3) = d;
  LWB (t_3) = 1;
  UPB (t_3) = l_1 + l_2;
  SHIFT (t_3) = LWB (t_3);
  SPAN (t_3) = 1;
/* add strings */
  b_3 = DEREF (BYTE_T, &ARRAY (a_3));
  m = 0;
  if (ROW_SIZE (t_1) > 0) {
    b_1 = DEREF (BYTE_T, &ARRAY (a_1));
    for (k = LWB (t_1); k <= UPB (t_1); k++) {
      MOVE ((BYTE_T *) & b_3[m], (BYTE_T *) & b_1[INDEX_1_DIM (a_1, t_1, k)], ALIGNED_SIZE_OF (A68_CHAR));
      m += ALIGNED_SIZE_OF (A68_CHAR);
    }
  }
  if (ROW_SIZE (t_2) > 0) {
    b_2 = DEREF (BYTE_T, &ARRAY (a_2));
    for (k = LWB (t_2); k <= UPB (t_2); k++) {
      MOVE ((BYTE_T *) & b_3[m], (BYTE_T *) & b_2[INDEX_1_DIM (a_2, t_2, k)], ALIGNED_SIZE_OF (A68_CHAR));
      m += ALIGNED_SIZE_OF (A68_CHAR);
    }
  }
  PUSH_REF (p, c);
}

/*!
\brief OP * = (INT, STRING) STRING
\param p position in tree
**/

void genie_times_int_string (NODE_T * p)
{
  A68_INT k;
  A68_REF a;
  POP_REF (p, &a);
  POP_OBJECT (p, &k, A68_INT);
  PRELUDE_ERROR (VALUE (&k) < 0, p, ERROR_INVALID_ARGUMENT, MODE (INT));
  PUSH_REF (p, empty_string (p));
  while (VALUE (&k)--) {
    PUSH_REF (p, a);
    genie_add_string (p);
  }
}

/*!
\brief OP * = (STRING, INT) STRING
\param p position in tree
**/

void genie_times_string_int (NODE_T * p)
{
  A68_INT k;
  A68_REF a;
  POP_OBJECT (p, &k, A68_INT);
  POP_REF (p, &a);
  PUSH_PRIMITIVE (p, VALUE (&k), A68_INT);
  PUSH_REF (p, a);
  genie_times_int_string (p);
}

/*!
\brief OP * = (INT, CHAR) STRING
\param p position in tree
**/

void genie_times_int_char (NODE_T * p)
{
  A68_INT str_size;
  A68_CHAR a;
  A68_REF z, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  BYTE_T *base;
  int k;
/* Pop operands */
  POP_OBJECT (p, &a, A68_CHAR);
  POP_OBJECT (p, &str_size, A68_INT);
  PRELUDE_ERROR (VALUE (&str_size) < 0, p, ERROR_INVALID_ARGUMENT, MODE (INT));
/* Make new_one string */
  z = heap_generator (p, MODE (ROW_CHAR), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  row = heap_generator (p, MODE (ROW_CHAR), (int) (VALUE (&str_size)) * ALIGNED_SIZE_OF (A68_CHAR));
  DIM (&arr) = 1;
  MOID (&arr) = MODE (CHAR);
  ELEM_SIZE (&arr) = ALIGNED_SIZE_OF (A68_CHAR);
  SLICE_OFFSET (&arr) = 0;
  FIELD_OFFSET (&arr) = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = VALUE (&str_size);
  SHIFT (&tup) = LWB (&tup);
  SPAN (&tup) = 1;
  K (&tup) = 0;
  PUT_DESCRIPTOR (arr, tup, &z);
/* Copy */
  base = ADDRESS (&row);
  for (k = 0; k < VALUE (&str_size); k++) {
    A68_CHAR ch;
    STATUS (&ch) = INIT_MASK;
    VALUE (&ch) = VALUE (&a);
    *(A68_CHAR *) & base[k * ALIGNED_SIZE_OF (A68_CHAR)] = ch;
  }
  PUSH_REF (p, z);
}

/*!
\brief OP * = (CHAR, INT) STRING
\param p position in tree
**/

void genie_times_char_int (NODE_T * p)
{
  A68_INT k;
  A68_CHAR a;
  POP_OBJECT (p, &k, A68_INT);
  POP_OBJECT (p, &a, A68_CHAR);
  PUSH_PRIMITIVE (p, VALUE (&k), A68_INT);
  PUSH_PRIMITIVE (p, VALUE (&a), A68_CHAR);
  genie_times_int_char (p);
}

/*!
\brief OP +:= = (REF STRING, STRING) REF STRING
\param p position in tree
**/

void genie_plusab_string (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_STRING), genie_add_string);
}

/*!
\brief OP +=: = (STRING, REF STRING) REF STRING
\param p position in tree
**/

void genie_plusto_string (NODE_T * p)
{
  A68_REF refa, a, b;
  POP_REF (p, &refa);
  CHECK_REF (p, refa, MODE (REF_STRING));
  a = * DEREF (A68_REF, &refa);
  CHECK_INIT (p, INITIALISED (&a), MODE (STRING));
  POP_REF (p, &b);
  PUSH_REF (p, b);
  PUSH_REF (p, a);
  genie_add_string (p);
  POP_REF (p, DEREF (A68_REF, &refa));
  PUSH_REF (p, refa);
}

/*!
\brief OP *:= = (REF STRING, INT) REF STRING
\param p position in tree
**/

void genie_timesab_string (NODE_T * p)
{
  A68_INT k;
  A68_REF refa, a;
  int i;
/* INT */
  POP_OBJECT (p, &k, A68_INT);
  PRELUDE_ERROR (VALUE (&k) < 0, p, ERROR_INVALID_ARGUMENT, MODE (INT));
/* REF STRING */
  POP_REF (p, &refa);
  CHECK_REF (p, refa, MODE (REF_STRING));
  a = * DEREF (A68_REF, &refa);
  CHECK_INIT (p, INITIALISED (&a), MODE (STRING));
/* Multiplication as repeated addition */
  PUSH_REF (p, empty_string (p));
  for (i = 1; i <= VALUE (&k); i++) {
    PUSH_REF (p, a);
    genie_add_string (p);
  }
/* The stack contains a STRING, promote to REF STRING */
  POP_REF (p, DEREF (A68_REF, &refa));
  PUSH_REF (p, refa);
}

/*!
\brief difference between two STRINGs in the stack
\param p position in tree
\return -1 if a < b, 0 if a = b or -1 if a > b
**/

static int string_difference (NODE_T * p)
{
  A68_REF row1, row2;
  A68_ARRAY *a_1, *a_2;
  A68_TUPLE *t_1, *t_2;
  BYTE_T *b_1, *b_2;
  int size, s_1, s_2, k, diff;
/* Pop operands */
  POP_REF (p, &row2);
  CHECK_INIT (p, INITIALISED (&row2), MODE (STRING));
  GET_DESCRIPTOR (a_2, t_2, &row2);
  s_2 = ROW_SIZE (t_2);
  POP_REF (p, &row1);
  CHECK_INIT (p, INITIALISED (&row1), MODE (STRING));
  GET_DESCRIPTOR (a_1, t_1, &row1);
  s_1 = ROW_SIZE (t_1);
/* Get difference */
  size = (s_1 > s_2 ? s_1 : s_2);
  diff = 0;
  b_1 = (s_1 > 0 ? DEREF (BYTE_T, &ARRAY (a_1)) : NO_BYTE);
  b_2 = (s_2 > 0 ? DEREF (BYTE_T, &ARRAY (a_2)) : NO_BYTE);
  for (k = 0; k < size && diff == 0; k++) {
    int a, b;
    if (s_1 > 0 && k < s_1) {
      A68_CHAR *ch = (A68_CHAR *) & b_1[INDEX_1_DIM (a_1, t_1, LWB (t_1) + k)];
      a = (int) VALUE (ch);
    } else {
      a = 0;
    }
    if (s_2 > 0 && k < s_2) {
      A68_CHAR *ch = (A68_CHAR *) & b_2[INDEX_1_DIM (a_2, t_2, LWB (t_2) + k)];
      b = (int) VALUE (ch);
    } else {
      b = 0;
    }
    diff += (TO_UCHAR (a) - TO_UCHAR (b));
  }
  return (diff);
}

/* OP (STRING, STRING) BOOL */

#define A68_CMP_STRING(n, OP)\
void n (NODE_T * p) {\
  int k = string_difference (p);\
  PUSH_PRIMITIVE (p, (BOOL_T) (k OP 0), A68_BOOL);\
}

A68_CMP_STRING (genie_eq_string, ==)
A68_CMP_STRING (genie_ne_string, !=)
A68_CMP_STRING (genie_lt_string, <)
A68_CMP_STRING (genie_gt_string, >)
A68_CMP_STRING (genie_le_string, <=)
A68_CMP_STRING (genie_ge_string, >=)

/* RNG functions are in gsl.c.*/

/*!
\brief PROC first random = (INT) VOID
\param p position in tree
**/
void genie_first_random (NODE_T * p)
{
  A68_INT i;
  POP_OBJECT (p, &i, A68_INT);
  init_rng ((unsigned long) VALUE (&i));
}

/*!
\brief PROC next random = REAL
\param p position in tree
**/

void genie_next_random (NODE_T * p)
{
  PUSH_PRIMITIVE (p, rng_53_bit (), A68_REAL);
}

/*!
\brief PROC rnd = REAL
\param p position in tree
**/

void genie_next_rnd (NODE_T * p)
{
  PUSH_PRIMITIVE (p, 2 * rng_53_bit () - 1, A68_REAL);
}

/*!
\brief PROC next long random = LONG REAL
\param p position in tree
**/

void genie_long_next_random (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p));
  MP_T *z;
  int k = 2 + digits;
  STACK_MP (z, p, digits);
  while (--k > 1) {
    z[k] = (MP_T) (int) (rng_53_bit () * MP_RADIX);
  }
  MP_EXPONENT (z) = (MP_T) (-1);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

/* BYTES operations */

/*!
\brief OP ELEM = (INT, BYTES) CHAR
\param p position in tree
**/

void genie_elem_bytes (NODE_T * p)
{
  A68_BYTES j;
  A68_INT i;
  POP_OBJECT (p, &j, A68_BYTES);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&i) < 1 || VALUE (&i) > BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  if (VALUE (&i) > (int) strlen (VALUE (&j))) {
    genie_null_char (p);
  } else {
    PUSH_PRIMITIVE (p, VALUE (&j)[VALUE (&i) - 1], A68_CHAR);
  }
}

/*!
\brief PROC bytes pack = (STRING) BYTES
\param p position in tree
**/

void genie_bytespack (NODE_T * p)
{
  A68_REF z;
  A68_BYTES b;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (STRING));
  PRELUDE_ERROR (a68_string_size (p, z) > BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (STRING));
  STATUS (&b) = INIT_MASK;
  ASSERT (a_to_c_string (p, VALUE (&b), z) != NO_TEXT);
  PUSH_BYTES (p, VALUE (&b));
}

/*!
\brief PROC bytes pack = (STRING) BYTES
\param p position in tree
**/

void genie_add_bytes (NODE_T * p)
{
  A68_BYTES *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BYTES);
  PRELUDE_ERROR (((int) strlen (VALUE (i)) + (int) strlen (VALUE (j))) > BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (BYTES));
  bufcat (VALUE (i), VALUE (j), BYTES_WIDTH);
}

/*!
\brief OP +:= = (REF BYTES, BYTES) REF BYTES
\param p position in tree
**/

void genie_plusab_bytes (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_BYTES), genie_add_bytes);
}

/*!
\brief OP +=: = (BYTES, REF BYTES) REF BYTES
\param p position in tree
**/

void genie_plusto_bytes (NODE_T * p)
{
  A68_BYTES i, *address, j;
  A68_REF z;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (REF_BYTES));
  address = DEREF (A68_BYTES, &z);
  CHECK_INIT (p, INITIALISED (address), MODE (BYTES));
  POP_OBJECT (p, &i, A68_BYTES);
  PRELUDE_ERROR (((int) strlen (VALUE (address)) + (int) strlen (VALUE (&i))) > BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (BYTES));
  bufcpy (VALUE (&j), VALUE (&i), BYTES_WIDTH);
  bufcat (VALUE (&j), VALUE (address), BYTES_WIDTH);
  bufcpy (VALUE (address), VALUE (&j), BYTES_WIDTH);
  PUSH_REF (p, z);
}

/*!
\brief difference between BYTE strings
\param p position in tree
\return difference between objects
**/

static int compare_bytes (NODE_T * p)
{
  A68_BYTES x, y;
  POP_OBJECT (p, &y, A68_BYTES);
  POP_OBJECT (p, &x, A68_BYTES);
  return (strcmp (VALUE (&x), VALUE (&y)));
}

/* OP (BYTES, BYTES) BOOL */

#define A68_CMP_BYTES(n, OP)\
void n (NODE_T * p) {\
  int k = compare_bytes (p);\
  PUSH_PRIMITIVE (p, (BOOL_T) (k OP 0), A68_BOOL);\
}

A68_CMP_BYTES (genie_eq_bytes, ==)
A68_CMP_BYTES (genie_ne_bytes, !=)
A68_CMP_BYTES (genie_lt_bytes, <)
A68_CMP_BYTES (genie_gt_bytes, >)
A68_CMP_BYTES (genie_le_bytes, <=)
A68_CMP_BYTES (genie_ge_bytes, >=)

/*!
\brief OP LENG = (BYTES) LONG BYTES
\param p position in tree
**/

void genie_leng_bytes (NODE_T * p)
{
  A68_BYTES a;
  POP_OBJECT (p, &a, A68_BYTES);
  PUSH_LONG_BYTES (p, VALUE (&a));
}

/*!
\brief OP SHORTEN = (LONG BYTES) BYTES
\param p position in tree
**/

void genie_shorten_bytes (NODE_T * p)
{
  A68_LONG_BYTES a;
  POP_OBJECT (p, &a, A68_LONG_BYTES);
  PRELUDE_ERROR (strlen (VALUE (&a)) >= BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (BYTES));
  PUSH_BYTES (p, VALUE (&a));
}

/*!
\brief OP ELEM = (INT, LONG BYTES) CHAR
\param p position in tree
**/

void genie_elem_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES j;
  A68_INT i;
  POP_OBJECT (p, &j, A68_LONG_BYTES);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&i) < 1 || VALUE (&i) > LONG_BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  if (VALUE (&i) > (int) strlen (VALUE (&j))) {
    genie_null_char (p);
  } else {
    PUSH_PRIMITIVE (p, VALUE (&j)[VALUE (&i) - 1], A68_CHAR);
  }
}

/*!
\brief PROC long bytes pack = (STRING) LONG BYTES
\param p position in tree
**/

void genie_long_bytespack (NODE_T * p)
{
  A68_REF z;
  A68_LONG_BYTES b;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (STRING));
  PRELUDE_ERROR (a68_string_size (p, z) > LONG_BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (STRING));
  STATUS (&b) = INIT_MASK;
  ASSERT (a_to_c_string (p, VALUE (&b), z) != NO_TEXT);
  PUSH_LONG_BYTES (p, VALUE (&b));
}

/*!
\brief OP + = (LONG BYTES, LONG BYTES) LONG BYTES
\param p position in tree
**/

void genie_add_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_LONG_BYTES);
  PRELUDE_ERROR (((int) strlen (VALUE (i)) + (int) strlen (VALUE (j))) > LONG_BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (LONG_BYTES));
  bufcat (VALUE (i), VALUE (j), LONG_BYTES_WIDTH);
}

/*!
\brief OP +:= = (REF LONG BYTES, LONG BYTES) REF LONG BYTES
\param p position in tree
**/

void genie_plusab_long_bytes (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_LONG_BYTES), genie_add_long_bytes);
}

/*!
\brief OP +=: = (LONG BYTES, REF LONG BYTES) REF LONG BYTES
\param p position in tree
**/

void genie_plusto_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES i, *address, j;
  A68_REF z;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (REF_LONG_BYTES));
  address = DEREF (A68_LONG_BYTES, &z);
  CHECK_INIT (p, INITIALISED (address), MODE (LONG_BYTES));
  POP_OBJECT (p, &i, A68_LONG_BYTES);
  PRELUDE_ERROR (((int) strlen (VALUE (address)) + (int) strlen (VALUE (&i))) > LONG_BYTES_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (LONG_BYTES));
  bufcpy (VALUE (&j), VALUE (&i), LONG_BYTES_WIDTH);
  bufcat (VALUE (&j), VALUE (address), LONG_BYTES_WIDTH);
  bufcpy (VALUE (address), VALUE (&j), LONG_BYTES_WIDTH);
  PUSH_REF (p, z);
}

/*!
\brief difference between LONG BYTE strings
\param p position in tree
\return difference between objects
**/

static int compare_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES x, y;
  POP_OBJECT (p, &y, A68_LONG_BYTES);
  POP_OBJECT (p, &x, A68_LONG_BYTES);
  return (strcmp (VALUE (&x), VALUE (&y)));
}

/* OP (LONG BYTES, LONG BYTES) BOOL */

#define A68_CMP_LONG_BYTES(n, OP)\
void n (NODE_T * p) {\
  int k = compare_long_bytes (p);\
  PUSH_PRIMITIVE (p, (BOOL_T) (k OP 0), A68_BOOL);\
}

A68_CMP_LONG_BYTES (genie_eq_long_bytes, ==)
A68_CMP_LONG_BYTES (genie_ne_long_bytes, !=)
A68_CMP_LONG_BYTES (genie_lt_long_bytes, <)
A68_CMP_LONG_BYTES (genie_gt_long_bytes, >)
A68_CMP_LONG_BYTES (genie_le_long_bytes, <=)
A68_CMP_LONG_BYTES (genie_ge_long_bytes, >=)

/* BITS operations */

/* OP NOT = (BITS) BITS */

A68_MONAD (genie_not_bits, A68_BITS, ~)

/*!
\brief OP AND = (BITS, BITS) BITS
\param p position in tree
**/

void genie_and_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) & VALUE (j);
}

/*!
\brief OP OR = (BITS, BITS) BITS
\param p position in tree
**/

void genie_or_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) | VALUE (j);
}

/*!
\brief OP XOR = (BITS, BITS) BITS
\param p position in tree
**/

void genie_xor_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) ^ VALUE (j);
}

/* OP = = (BITS, BITS) BOOL */

#define A68_CMP_BITS(n, OP)\
void n (NODE_T * p) {\
  A68_BITS i, j;\
  POP_OBJECT (p, &j, A68_BITS);\
  POP_OBJECT (p, &i, A68_BITS);\
  PUSH_PRIMITIVE (p, (BOOL_T) (VALUE (&i) OP VALUE (&j)), A68_BOOL);\
  }

A68_CMP_BITS (genie_eq_bits, ==)
A68_CMP_BITS (genie_ne_bits, !=)

/*!
\brief OP <= = (BITS, BITS) BOOL
\param p position in tree
**/

void genie_le_bits (NODE_T * p)
{
  A68_BITS i, j;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_BITS);
  PUSH_PRIMITIVE (p, (BOOL_T) ((VALUE (&i) | VALUE (&j)) == VALUE (&j)), A68_BOOL);
}

/*!
\brief OP >= = (BITS, BITS) BOOL
\param p position in tree
**/

void genie_ge_bits (NODE_T * p)
{
  A68_BITS i, j;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_BITS);
  PUSH_PRIMITIVE (p, (BOOL_T) ((VALUE (&i) | VALUE (&j)) == VALUE (&i)), A68_BOOL);
}

/*!
\brief OP SHL = (BITS, INT) BITS
\param p position in tree
**/

void genie_shl_bits (NODE_T * p)
{
  A68_BITS i;
  A68_INT j;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_BITS);
  if (VALUE (&j) >= 0) {
    PUSH_PRIMITIVE (p, VALUE (&i) << VALUE (&j), A68_BITS);
  } else {
    PUSH_PRIMITIVE (p, VALUE (&i) >> -VALUE (&j), A68_BITS);
  }
}

/*!
\brief OP SHR = (BITS, INT) BITS
\param p position in tree
**/

void genie_shr_bits (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  genie_shl_bits (p);           /* Conform RR */
}

/*!
\brief OP ELEM = (INT, BITS) BOOL
\param p position in tree
**/

void genie_elem_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  unsigned mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_PRIMITIVE (p, (BOOL_T) ((VALUE (&j) & mask) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
}

/*!
\brief OP SET = (INT, BITS) BITS
\param p position in tree
**/

void genie_set_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  unsigned mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_PRIMITIVE (p, VALUE (&j) | mask, A68_BITS);
}

/*!
\brief OP CLEAR = (INT, BITS) BITS
\param p position in tree
**/

void genie_clear_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  unsigned mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_PRIMITIVE (p, VALUE (&j) & ~mask, A68_BITS);
}

/*!
\brief OP ABS = (BITS) INT
\param p position in tree
**/

void genie_abs_bits (NODE_T * p)
{
  A68_BITS i;
  POP_OBJECT (p, &i, A68_BITS);
  PUSH_PRIMITIVE (p, (int) (VALUE (&i)), A68_INT);
}

/*!
\brief OP BIN = (INT) BITS
\param p position in tree
**/

void genie_bin_int (NODE_T * p)
{
  A68_INT i;
  POP_OBJECT (p, &i, A68_INT);
/* RR does not convert negative numbers. Algol68G does */
  PUSH_PRIMITIVE (p, (unsigned) (VALUE (&i)), A68_BITS);
}

/*!
\brief OP BIN = (LONG INT) LONG BITS
\param p position in tree
**/

void genie_bin_long_mp (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *u = (MP_T *) STACK_OFFSET (-size);
/* We convert just for the operand check */
  (void) stack_mp_bits (p, u, mode);
  MP_STATUS (u) = (MP_T) INIT_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief OP NOT = (LONG BITS) LONG BITS
\param p position in tree
**/

void genie_not_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  int k, words = get_mp_bits_words (mode);
  MP_T *u = (MP_T *) STACK_OFFSET (-size);
  unsigned *row = stack_mp_bits (p, u, mode);
  for (k = 0; k < words; k++) {
    row[k] = ~row[k];
  }
  (void) pack_mp_bits (p, u, row, mode);
  stack_pointer = pop_sp;
}

/*!
\brief OP SHORTEN = (LONG BITS) BITS
\param p position in tree
**/

void genie_shorten_long_mp_to_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_PRIMITIVE (p, mp_to_unsigned (p, z, digits), A68_BITS);
}

/*!
\brief get bit from LONG BITS
\param p position in tree
\param k element number
\param z mp number
\param m mode associated with z
\return same
**/

unsigned elem_long_bits (NODE_T * p, ADDR_T k, MP_T * z, MOID_T * m)
{
  int n;
  ADDR_T pop_sp = stack_pointer;
  unsigned *words = stack_mp_bits (p, z, m), mask = 0x1;
  k += (MP_BITS_BITS - get_mp_bits_width (m) % MP_BITS_BITS - 1);
  for (n = 0; n < MP_BITS_BITS - k % MP_BITS_BITS - 1; n++) {
    mask = mask << 1;
  }
  stack_pointer = pop_sp;
  return ((words[k / MP_BITS_BITS]) & mask);
}

/*!
\brief OP ELEM = (INT, LONG BITS) BOOL
\param p position in tree
**/

void genie_elem_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  unsigned w;
  int bits = get_mp_bits_width (MODE (LONG_BITS)), size = get_mp_size (MODE (LONG_BITS));
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  w = elem_long_bits (p, VALUE (i), z, MODE (LONG_BITS));
  DECREMENT_STACK_POINTER (p, size + ALIGNED_SIZE_OF (A68_INT));
  PUSH_PRIMITIVE (p, (BOOL_T) (w != 0), A68_BOOL);
}

/*!
\brief OP ELEM = (INT, LONG LONG BITS) BOOL
\param p position in tree
**/

void genie_elem_longlong_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  unsigned w;
  int bits = get_mp_bits_width (MODE (LONGLONG_BITS)), size = get_mp_size (MODE (LONGLONG_BITS));
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  w = elem_long_bits (p, VALUE (i), z, MODE (LONGLONG_BITS));
  DECREMENT_STACK_POINTER (p, size + ALIGNED_SIZE_OF (A68_INT));
  PUSH_PRIMITIVE (p, (BOOL_T) (w != 0), A68_BOOL);
}

/*!
\brief set bit in LONG BITS
\param p position in tree
\param k bit index
\param z mp number
\param m mode associated with z
**/

static unsigned *set_long_bits (NODE_T * p, int k, MP_T * z, MOID_T * m, unsigned bit)
{
  int n;
  unsigned *words = stack_mp_bits (p, z, m), mask = 0x1;
  k += (MP_BITS_BITS - get_mp_bits_width (m) % MP_BITS_BITS - 1);
  for (n = 0; n < MP_BITS_BITS - k % MP_BITS_BITS - 1; n++) {
    mask = mask << 1;
  }
  if (bit == 0x1) {
    words[k / MP_BITS_BITS] = (words[k / MP_BITS_BITS]) | mask;
  } else {
    words[k / MP_BITS_BITS] = (words[k / MP_BITS_BITS]) & (~mask);
  }
  return (words);
}

/*!
\brief OP SET = (INT, LONG BITS) VOID
\param p position in tree
**/

void genie_set_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  unsigned *w;
  ADDR_T pop_sp = stack_pointer;
  int bits = get_mp_bits_width (MODE (LONG_BITS)), size = get_mp_size (MODE (LONG_BITS));
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  w = set_long_bits (p, VALUE (i), z, MODE (LONG_BITS), 0x1);
  (void) pack_mp_bits (p, (MP_T *) STACK_ADDRESS (pop_sp - size - ALIGNED_SIZE_OF (A68_INT)), w, MODE (LONG_BITS));
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief OP SET = (INT, LONG LONG BITS) BOOL
\param p position in tree
**/

void genie_set_longlong_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  unsigned *w;
  ADDR_T pop_sp = stack_pointer;
  int bits = get_mp_bits_width (MODE (LONGLONG_BITS)), size = get_mp_size (MODE (LONGLONG_BITS));
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  w = set_long_bits (p, VALUE (i), z, MODE (LONGLONG_BITS), 0x1);
  (void) pack_mp_bits (p, (MP_T *) STACK_ADDRESS (pop_sp - size - ALIGNED_SIZE_OF (A68_INT)), w, MODE (LONGLONG_BITS));
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief OP CLEAR = (INT, LONG BITS) BOOL
\param p position in tree
**/

void genie_clear_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  unsigned *w;
  ADDR_T pop_sp = stack_pointer;
  int bits = get_mp_bits_width (MODE (LONG_BITS)), size = get_mp_size (MODE (LONG_BITS));
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  w = set_long_bits (p, VALUE (i), z, MODE (LONG_BITS), 0x0);
  (void) pack_mp_bits (p, (MP_T *) STACK_ADDRESS (pop_sp - size - ALIGNED_SIZE_OF (A68_INT)), w, MODE (LONG_BITS));
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief OP CLEAR = (INT, LONG LONG BITS) BOOL
\param p position in tree
**/

void genie_clear_longlong_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  unsigned *w;
  ADDR_T pop_sp = stack_pointer;
  int bits = get_mp_bits_width (MODE (LONGLONG_BITS)), size = get_mp_size (MODE (LONGLONG_BITS));
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
  w = set_long_bits (p, VALUE (i), z, MODE (LONGLONG_BITS), 0x0);
  (void) pack_mp_bits (p, (MP_T *) STACK_ADDRESS (pop_sp - size - ALIGNED_SIZE_OF (A68_INT)), w, MODE (LONGLONG_BITS));
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief PROC bits pack = ([] BOOL) BITS
\param p position in tree
**/

void genie_bits_pack (NODE_T * p)
{
  A68_REF z;
  A68_BITS b;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base;
  int size, k;
  unsigned bit;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (ROW_BOOL));
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  PRELUDE_ERROR (size < 0 || size > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, MODE (ROW_BOOL));
  VALUE (&b) = 0x0;
  if (ROW_SIZE (tup) > 0) {
    base = DEREF (BYTE_T, &ARRAY (arr));
    bit = 0x1;
    for (k = UPB (tup); k >= LWB (tup); k--) {
      int addr = INDEX_1_DIM (arr, tup, k);
      A68_BOOL *boo = (A68_BOOL *) & (base[addr]);
      CHECK_INIT (p, INITIALISED (boo), MODE (BOOL));
      if (VALUE (boo)) {
        VALUE (&b) |= bit;
      }
      bit <<= 1;
    }
  }
  STATUS (&b) = INIT_MASK;
  PUSH_OBJECT (p, b, A68_BITS);
}

/*!
\brief PROC long bits pack = ([] BOOL) LONG BITS
\param p position in tree
**/

void genie_long_bits_pack (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  A68_REF z;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base;
  int size, k, bits, digits;
  ADDR_T pop_sp;
  MP_T *sum, *fact;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (ROW_BOOL));
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  bits = get_mp_bits_width (mode);
  digits = get_mp_digits (mode);
  PRELUDE_ERROR (size < 0 || size > bits, p, ERROR_OUT_OF_BOUNDS, MODE (ROW_BOOL));
/* Convert so that LWB goes to MSB, so ELEM gives same order as [] BOOL */
  STACK_MP (sum, p, digits);
  SET_MP_ZERO (sum, digits);
  pop_sp = stack_pointer;
  STACK_MP (fact, p, digits);
  (void) set_mp_short (fact, (MP_T) 1, 0, digits);
  if (ROW_SIZE (tup) > 0) {
    base = DEREF (BYTE_T, &ARRAY (arr));
    for (k = UPB (tup); k >= LWB (tup); k--) {
      int addr = INDEX_1_DIM (arr, tup, k);
      A68_BOOL *boo = (A68_BOOL *) & (base[addr]);
      CHECK_INIT (p, INITIALISED (boo), MODE (BOOL));
      if (VALUE (boo)) {
       (void) add_mp (p, sum, sum, fact, digits);
      }
     (void) mul_mp_digit (p, fact, fact, (MP_T) 2, digits);
    }
  }
  stack_pointer = pop_sp;
  MP_STATUS (sum) = (MP_T) INIT_MASK;
}

/*!
\brief OP SHL = (LONG BITS, INT) LONG BITS
\param p position in tree
**/

void genie_shl_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int i, k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  MP_T *u;
  unsigned *row_u;
  ADDR_T pop_sp;
  A68_INT j;
/* Pop number of bits */
  POP_OBJECT (p, &j, A68_INT);
  u = (MP_T *) STACK_OFFSET (-size);
  pop_sp = stack_pointer;
  row_u = stack_mp_bits (p, u, mode);
  if (VALUE (&j) >= 0) {
    for (i = 0; i < VALUE (&j); i++) {
      BOOL_T carry = A68_FALSE;
      for (k = words - 1; k >= 0; k--) {
        row_u[k] <<= 1;
        if (carry) {
          row_u[k] |= 0x1;
        }
        carry = (BOOL_T) ((row_u[k] & MP_BITS_RADIX) != 0);
        row_u[k] &= ~((unsigned) MP_BITS_RADIX);
      }
    }
  } else {
    for (i = 0; i < -VALUE (&j); i++) {
      BOOL_T carry = A68_FALSE;
      for (k = 0; k < words; k++) {
        if (carry) {
          row_u[k] |= MP_BITS_RADIX;
        }
        carry = (BOOL_T) ((row_u[k] & 0x1) != 0);
        row_u[k] >>= 1;
      }
    }
  }
  (void) pack_mp_bits (p, u, row_u, mode);
  stack_pointer = pop_sp;
}

/*!
\brief OP SHR = (LONG BITS, INT) LONG BITS
\param p position in tree
**/

void genie_shr_long_mp (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  (void) genie_shl_long_mp (p);        /* Conform RR */
}

/*!
\brief OP <= = (LONG BITS, LONG BITS) BOOL
\param p position in tree
**/

void genie_le_long_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  BOOL_T result = A68_TRUE;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; (k < words) && result; k++) {
    result = (BOOL_T) (result & ((row_u[k] | row_v[k]) == row_v[k]));
  }
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, (BOOL_T) (result ? A68_TRUE : A68_FALSE), A68_BOOL);
}

/*!
\brief OP >= = (LONG BITS, LONG BITS) BOOL
\param p position in tree
**/

void genie_ge_long_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  BOOL_T result = A68_TRUE;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; (k < words) && result; k++) {
    result = (BOOL_T) (result & ((row_u[k] | row_v[k]) == row_u[k]));
  }
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, (BOOL_T) (result ? A68_TRUE : A68_FALSE), A68_BOOL);
}

/*!
\brief OP AND = (LONG BITS, LONG BITS) LONG BITS
\param p position in tree
**/

void genie_and_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] &= row_v[k];
  }
  (void) pack_mp_bits (p, u, row_u, mode);
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP OR = (LONG BITS, LONG BITS) LONG BITS
\param p position in tree
**/

void genie_or_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] |= row_v[k];
  }
  (void) pack_mp_bits (p, u, row_u, mode);
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP XOR = (LONG BITS, LONG BITS) LONG BITS
\param p position in tree
**/

void genie_xor_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] ^= row_v[k];
  }
  (void) pack_mp_bits (p, u, row_u, mode);
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

A68_ENV_REAL (genie_cgs_acre, GSL_CONST_CGSM_ACRE)
A68_ENV_REAL (genie_cgs_angstrom, GSL_CONST_CGSM_ANGSTROM)
A68_ENV_REAL (genie_cgs_astronomical_unit, GSL_CONST_CGSM_ASTRONOMICAL_UNIT)
A68_ENV_REAL (genie_cgs_bar, GSL_CONST_CGSM_BAR)
A68_ENV_REAL (genie_cgs_barn, GSL_CONST_CGSM_BARN)
A68_ENV_REAL (genie_cgs_bohr_magneton, GSL_CONST_CGSM_BOHR_MAGNETON)
A68_ENV_REAL (genie_cgs_bohr_radius, GSL_CONST_CGSM_BOHR_RADIUS)
A68_ENV_REAL (genie_cgs_boltzmann, GSL_CONST_CGSM_BOLTZMANN)
A68_ENV_REAL (genie_cgs_btu, GSL_CONST_CGSM_BTU)
A68_ENV_REAL (genie_cgs_calorie, GSL_CONST_CGSM_CALORIE)
A68_ENV_REAL (genie_cgs_canadian_gallon, GSL_CONST_CGSM_CANADIAN_GALLON)
A68_ENV_REAL (genie_cgs_carat, GSL_CONST_CGSM_CARAT)
A68_ENV_REAL (genie_cgs_cup, GSL_CONST_CGSM_CUP)
A68_ENV_REAL (genie_cgs_curie, GSL_CONST_CGSM_CURIE)
A68_ENV_REAL (genie_cgs_day, GSL_CONST_CGSM_DAY)
A68_ENV_REAL (genie_cgs_dyne, GSL_CONST_CGSM_DYNE)
A68_ENV_REAL (genie_cgs_electron_charge, GSL_CONST_CGSM_ELECTRON_CHARGE)
A68_ENV_REAL (genie_cgs_electron_magnetic_moment, GSL_CONST_CGSM_ELECTRON_MAGNETIC_MOMENT)
A68_ENV_REAL (genie_cgs_electron_volt, GSL_CONST_CGSM_ELECTRON_VOLT) 
A68_ENV_REAL (genie_cgs_erg, GSL_CONST_CGSM_ERG)
A68_ENV_REAL (genie_cgs_faraday, GSL_CONST_CGSM_FARADAY) 
A68_ENV_REAL (genie_cgs_fathom, GSL_CONST_CGSM_FATHOM)
A68_ENV_REAL (genie_cgs_fluid_ounce, GSL_CONST_CGSM_FLUID_OUNCE) 
A68_ENV_REAL (genie_cgs_foot, GSL_CONST_CGSM_FOOT)
A68_ENV_REAL (genie_cgs_footcandle, GSL_CONST_CGSM_FOOTCANDLE) 
A68_ENV_REAL (genie_cgs_footlambert, GSL_CONST_CGSM_FOOTLAMBERT)
A68_ENV_REAL (genie_cgs_gauss, GSL_CONST_CGSM_GAUSS) 
A68_ENV_REAL (genie_cgs_gram_force, GSL_CONST_CGSM_GRAM_FORCE)
A68_ENV_REAL (genie_cgs_grav_accel, GSL_CONST_CGSM_GRAV_ACCEL)
A68_ENV_REAL (genie_cgs_gravitational_constant, GSL_CONST_CGSM_GRAVITATIONAL_CONSTANT)
A68_ENV_REAL (genie_cgs_hectare, GSL_CONST_CGSM_HECTARE) 
A68_ENV_REAL (genie_cgs_horsepower, GSL_CONST_CGSM_HORSEPOWER)
A68_ENV_REAL (genie_cgs_hour, GSL_CONST_CGSM_HOUR) 
A68_ENV_REAL (genie_cgs_inch, GSL_CONST_CGSM_INCH)
A68_ENV_REAL (genie_cgs_inch_of_mercury, GSL_CONST_CGSM_INCH_OF_MERCURY)
A68_ENV_REAL (genie_cgs_inch_of_water, GSL_CONST_CGSM_INCH_OF_WATER) 
A68_ENV_REAL (genie_cgs_joule, GSL_CONST_CGSM_JOULE)
A68_ENV_REAL (genie_cgs_kilometers_per_hour, GSL_CONST_CGSM_KILOMETERS_PER_HOUR)
A68_ENV_REAL (genie_cgs_kilopound_force, GSL_CONST_CGSM_KILOPOUND_FORCE) 
A68_ENV_REAL (genie_cgs_knot, GSL_CONST_CGSM_KNOT)
A68_ENV_REAL (genie_cgs_lambert, GSL_CONST_CGSM_LAMBERT) 
A68_ENV_REAL (genie_cgs_light_year, GSL_CONST_CGSM_LIGHT_YEAR)
A68_ENV_REAL (genie_cgs_liter, GSL_CONST_CGSM_LITER) 
A68_ENV_REAL (genie_cgs_lumen, GSL_CONST_CGSM_LUMEN)
A68_ENV_REAL (genie_cgs_lux, GSL_CONST_CGSM_LUX) 
A68_ENV_REAL (genie_cgs_mass_electron, GSL_CONST_CGSM_MASS_ELECTRON)
A68_ENV_REAL (genie_cgs_mass_muon, GSL_CONST_CGSM_MASS_MUON) 
A68_ENV_REAL (genie_cgs_mass_neutron, GSL_CONST_CGSM_MASS_NEUTRON)
A68_ENV_REAL (genie_cgs_mass_proton, GSL_CONST_CGSM_MASS_PROTON)
A68_ENV_REAL (genie_cgs_meter_of_mercury, GSL_CONST_CGSM_METER_OF_MERCURY)
A68_ENV_REAL (genie_cgs_metric_ton, GSL_CONST_CGSM_METRIC_TON) 
A68_ENV_REAL (genie_cgs_micron, GSL_CONST_CGSM_MICRON)
A68_ENV_REAL (genie_cgs_mil, GSL_CONST_CGSM_MIL) 
A68_ENV_REAL (genie_cgs_mile, GSL_CONST_CGSM_MILE)
A68_ENV_REAL (genie_cgs_miles_per_hour, GSL_CONST_CGSM_MILES_PER_HOUR) 
A68_ENV_REAL (genie_cgs_minute, GSL_CONST_CGSM_MINUTE)
A68_ENV_REAL (genie_cgs_molar_gas, GSL_CONST_CGSM_MOLAR_GAS) 
A68_ENV_REAL (genie_cgs_nautical_mile, GSL_CONST_CGSM_NAUTICAL_MILE)
A68_ENV_REAL (genie_cgs_newton, GSL_CONST_CGSM_NEWTON) 
A68_ENV_REAL (genie_cgs_nuclear_magneton, GSL_CONST_CGSM_NUCLEAR_MAGNETON)
A68_ENV_REAL (genie_cgs_ounce_mass, GSL_CONST_CGSM_OUNCE_MASS) 
A68_ENV_REAL (genie_cgs_parsec, GSL_CONST_CGSM_PARSEC)
A68_ENV_REAL (genie_cgs_phot, GSL_CONST_CGSM_PHOT) 
A68_ENV_REAL (genie_cgs_pint, GSL_CONST_CGSM_PINT)
A68_ENV_REAL (genie_cgs_planck_constant_h, 6.6260693e-27) 
A68_ENV_REAL (genie_cgs_planck_constant_hbar, 6.6260693e-27 / (2 * A68_PI))
A68_ENV_REAL (genie_cgs_point, GSL_CONST_CGSM_POINT) 
A68_ENV_REAL (genie_cgs_poise, GSL_CONST_CGSM_POISE)
A68_ENV_REAL (genie_cgs_pound_force, GSL_CONST_CGSM_POUND_FORCE) 
A68_ENV_REAL (genie_cgs_pound_mass, GSL_CONST_CGSM_POUND_MASS)
A68_ENV_REAL (genie_cgs_poundal, GSL_CONST_CGSM_POUNDAL)
A68_ENV_REAL (genie_cgs_proton_magnetic_moment, GSL_CONST_CGSM_PROTON_MAGNETIC_MOMENT)
A68_ENV_REAL (genie_cgs_psi, GSL_CONST_CGSM_PSI) 
A68_ENV_REAL (genie_cgs_quart, GSL_CONST_CGSM_QUART)
A68_ENV_REAL (genie_cgs_rad, GSL_CONST_CGSM_RAD) 
A68_ENV_REAL (genie_cgs_roentgen, GSL_CONST_CGSM_ROENTGEN)
A68_ENV_REAL (genie_cgs_rydberg, GSL_CONST_CGSM_RYDBERG) 
A68_ENV_REAL (genie_cgs_solar_mass, GSL_CONST_CGSM_SOLAR_MASS)
A68_ENV_REAL (genie_cgs_speed_of_light, GSL_CONST_CGSM_SPEED_OF_LIGHT)
A68_ENV_REAL (genie_cgs_standard_gas_volume, GSL_CONST_CGSM_STANDARD_GAS_VOLUME)
A68_ENV_REAL (genie_cgs_std_atmosphere, GSL_CONST_CGSM_STD_ATMOSPHERE) 
A68_ENV_REAL (genie_cgs_stilb, GSL_CONST_CGSM_STILB)
A68_ENV_REAL (genie_cgs_stokes, GSL_CONST_CGSM_STOKES) 
A68_ENV_REAL (genie_cgs_tablespoon, GSL_CONST_CGSM_TABLESPOON)
A68_ENV_REAL (genie_cgs_teaspoon, GSL_CONST_CGSM_TEASPOON) 
A68_ENV_REAL (genie_cgs_texpoint, GSL_CONST_CGSM_TEXPOINT)
A68_ENV_REAL (genie_cgs_therm, GSL_CONST_CGSM_THERM) 
A68_ENV_REAL (genie_cgs_ton, GSL_CONST_CGSM_TON)
A68_ENV_REAL (genie_cgs_torr, GSL_CONST_CGSM_TORR) 
A68_ENV_REAL (genie_cgs_troy_ounce, GSL_CONST_CGSM_TROY_OUNCE)
A68_ENV_REAL (genie_cgs_uk_gallon, GSL_CONST_CGSM_UK_GALLON) 
A68_ENV_REAL (genie_cgs_uk_ton, GSL_CONST_CGSM_UK_TON)
A68_ENV_REAL (genie_cgs_unified_atomic_mass, GSL_CONST_CGSM_UNIFIED_ATOMIC_MASS)
A68_ENV_REAL (genie_cgs_us_gallon, GSL_CONST_CGSM_US_GALLON) 
A68_ENV_REAL (genie_cgs_week, GSL_CONST_CGSM_WEEK)
A68_ENV_REAL (genie_cgs_yard, GSL_CONST_CGSM_YARD) 
A68_ENV_REAL (genie_mks_acre, GSL_CONST_MKS_ACRE)
A68_ENV_REAL (genie_mks_angstrom, GSL_CONST_MKS_ANGSTROM)
A68_ENV_REAL (genie_mks_astronomical_unit, GSL_CONST_MKS_ASTRONOMICAL_UNIT) 
A68_ENV_REAL (genie_mks_bar, GSL_CONST_MKS_BAR)
A68_ENV_REAL (genie_mks_barn, GSL_CONST_MKS_BARN) 
A68_ENV_REAL (genie_mks_bohr_magneton, GSL_CONST_MKS_BOHR_MAGNETON)
A68_ENV_REAL (genie_mks_bohr_radius, GSL_CONST_MKS_BOHR_RADIUS) 
A68_ENV_REAL (genie_mks_boltzmann, GSL_CONST_MKS_BOLTZMANN)
A68_ENV_REAL (genie_mks_btu, GSL_CONST_MKS_BTU) 
A68_ENV_REAL (genie_mks_calorie, GSL_CONST_MKS_CALORIE)
A68_ENV_REAL (genie_mks_canadian_gallon, GSL_CONST_MKS_CANADIAN_GALLON) 
A68_ENV_REAL (genie_mks_carat, GSL_CONST_MKS_CARAT)
A68_ENV_REAL (genie_mks_cup, GSL_CONST_MKS_CUP) 
A68_ENV_REAL (genie_mks_curie, GSL_CONST_MKS_CURIE)
A68_ENV_REAL (genie_mks_day, GSL_CONST_MKS_DAY) 
A68_ENV_REAL (genie_mks_dyne, GSL_CONST_MKS_DYNE)
A68_ENV_REAL (genie_mks_electron_charge, GSL_CONST_MKS_ELECTRON_CHARGE)
A68_ENV_REAL (genie_mks_electron_magnetic_moment, GSL_CONST_MKS_ELECTRON_MAGNETIC_MOMENT)
A68_ENV_REAL (genie_mks_electron_volt, GSL_CONST_MKS_ELECTRON_VOLT) 
A68_ENV_REAL (genie_mks_erg, GSL_CONST_MKS_ERG)
A68_ENV_REAL (genie_mks_faraday, GSL_CONST_MKS_FARADAY) 
A68_ENV_REAL (genie_mks_fathom, GSL_CONST_MKS_FATHOM)
A68_ENV_REAL (genie_mks_fluid_ounce, GSL_CONST_MKS_FLUID_OUNCE) 
A68_ENV_REAL (genie_mks_foot, GSL_CONST_MKS_FOOT)
A68_ENV_REAL (genie_mks_footcandle, GSL_CONST_MKS_FOOTCANDLE) 
A68_ENV_REAL (genie_mks_footlambert, GSL_CONST_MKS_FOOTLAMBERT)
A68_ENV_REAL (genie_mks_gauss, GSL_CONST_MKS_GAUSS) 
A68_ENV_REAL (genie_mks_gram_force, GSL_CONST_MKS_GRAM_FORCE)
A68_ENV_REAL (genie_mks_grav_accel, GSL_CONST_MKS_GRAV_ACCEL)
A68_ENV_REAL (genie_mks_gravitational_constant, GSL_CONST_MKS_GRAVITATIONAL_CONSTANT)
A68_ENV_REAL (genie_mks_hectare, GSL_CONST_MKS_HECTARE) 
A68_ENV_REAL (genie_mks_horsepower, GSL_CONST_MKS_HORSEPOWER)
A68_ENV_REAL (genie_mks_hour, GSL_CONST_MKS_HOUR) 
A68_ENV_REAL (genie_mks_inch, GSL_CONST_MKS_INCH)
A68_ENV_REAL (genie_mks_inch_of_mercury, GSL_CONST_MKS_INCH_OF_MERCURY)
A68_ENV_REAL (genie_mks_inch_of_water, GSL_CONST_MKS_INCH_OF_WATER) 
A68_ENV_REAL (genie_mks_joule, GSL_CONST_MKS_JOULE)
A68_ENV_REAL (genie_mks_kilometers_per_hour, GSL_CONST_MKS_KILOMETERS_PER_HOUR)
A68_ENV_REAL (genie_mks_kilopound_force, GSL_CONST_MKS_KILOPOUND_FORCE) 
A68_ENV_REAL (genie_mks_knot, GSL_CONST_MKS_KNOT)
A68_ENV_REAL (genie_mks_lambert, GSL_CONST_MKS_LAMBERT) 
A68_ENV_REAL (genie_mks_light_year, GSL_CONST_MKS_LIGHT_YEAR)
A68_ENV_REAL (genie_mks_liter, GSL_CONST_MKS_LITER) 
A68_ENV_REAL (genie_mks_lumen, GSL_CONST_MKS_LUMEN)
A68_ENV_REAL (genie_mks_lux, GSL_CONST_MKS_LUX) 
A68_ENV_REAL (genie_mks_mass_electron, GSL_CONST_MKS_MASS_ELECTRON)
A68_ENV_REAL (genie_mks_mass_muon, GSL_CONST_MKS_MASS_MUON) 
A68_ENV_REAL (genie_mks_mass_neutron, GSL_CONST_MKS_MASS_NEUTRON)
A68_ENV_REAL (genie_mks_mass_proton, GSL_CONST_MKS_MASS_PROTON)
A68_ENV_REAL (genie_mks_meter_of_mercury, GSL_CONST_MKS_METER_OF_MERCURY)
A68_ENV_REAL (genie_mks_metric_ton, GSL_CONST_MKS_METRIC_TON) 
A68_ENV_REAL (genie_mks_micron, GSL_CONST_MKS_MICRON)
A68_ENV_REAL (genie_mks_mil, GSL_CONST_MKS_MIL) 
A68_ENV_REAL (genie_mks_mile, GSL_CONST_MKS_MILE)
A68_ENV_REAL (genie_mks_miles_per_hour, GSL_CONST_MKS_MILES_PER_HOUR) 
A68_ENV_REAL (genie_mks_minute, GSL_CONST_MKS_MINUTE)
A68_ENV_REAL (genie_mks_molar_gas, GSL_CONST_MKS_MOLAR_GAS) 
A68_ENV_REAL (genie_mks_nautical_mile, GSL_CONST_MKS_NAUTICAL_MILE)
A68_ENV_REAL (genie_mks_newton, GSL_CONST_MKS_NEWTON) 
A68_ENV_REAL (genie_mks_nuclear_magneton, GSL_CONST_MKS_NUCLEAR_MAGNETON)
A68_ENV_REAL (genie_mks_ounce_mass, GSL_CONST_MKS_OUNCE_MASS) 
A68_ENV_REAL (genie_mks_parsec, GSL_CONST_MKS_PARSEC)
A68_ENV_REAL (genie_mks_phot, GSL_CONST_MKS_PHOT) 
A68_ENV_REAL (genie_mks_pint, GSL_CONST_MKS_PINT)
A68_ENV_REAL (genie_mks_planck_constant_h, 6.6260693e-34) 
A68_ENV_REAL (genie_mks_planck_constant_hbar, 6.6260693e-34 / (2 * A68_PI))
A68_ENV_REAL (genie_mks_point, GSL_CONST_MKS_POINT) 
A68_ENV_REAL (genie_mks_poise, GSL_CONST_MKS_POISE)
A68_ENV_REAL (genie_mks_pound_force, GSL_CONST_MKS_POUND_FORCE) 
A68_ENV_REAL (genie_mks_pound_mass, GSL_CONST_MKS_POUND_MASS)
A68_ENV_REAL (genie_mks_poundal, GSL_CONST_MKS_POUNDAL)
A68_ENV_REAL (genie_mks_proton_magnetic_moment, GSL_CONST_MKS_PROTON_MAGNETIC_MOMENT)
A68_ENV_REAL (genie_mks_psi, GSL_CONST_MKS_PSI) 
A68_ENV_REAL (genie_mks_quart, GSL_CONST_MKS_QUART)
A68_ENV_REAL (genie_mks_rad, GSL_CONST_MKS_RAD) 
A68_ENV_REAL (genie_mks_roentgen, GSL_CONST_MKS_ROENTGEN)
A68_ENV_REAL (genie_mks_rydberg, GSL_CONST_MKS_RYDBERG) 
A68_ENV_REAL (genie_mks_solar_mass, GSL_CONST_MKS_SOLAR_MASS)
A68_ENV_REAL (genie_mks_speed_of_light, GSL_CONST_MKS_SPEED_OF_LIGHT)
A68_ENV_REAL (genie_mks_standard_gas_volume, GSL_CONST_MKS_STANDARD_GAS_VOLUME)
A68_ENV_REAL (genie_mks_std_atmosphere, GSL_CONST_MKS_STD_ATMOSPHERE) 
A68_ENV_REAL (genie_mks_stilb, GSL_CONST_MKS_STILB)
A68_ENV_REAL (genie_mks_stokes, GSL_CONST_MKS_STOKES) 
A68_ENV_REAL (genie_mks_tablespoon, GSL_CONST_MKS_TABLESPOON)
A68_ENV_REAL (genie_mks_teaspoon, GSL_CONST_MKS_TEASPOON) 
A68_ENV_REAL (genie_mks_texpoint, GSL_CONST_MKS_TEXPOINT)
A68_ENV_REAL (genie_mks_therm, GSL_CONST_MKS_THERM) 
A68_ENV_REAL (genie_mks_ton, GSL_CONST_MKS_TON)
A68_ENV_REAL (genie_mks_torr, GSL_CONST_MKS_TORR) 
A68_ENV_REAL (genie_mks_troy_ounce, GSL_CONST_MKS_TROY_OUNCE)
A68_ENV_REAL (genie_mks_uk_gallon, GSL_CONST_MKS_UK_GALLON) 
A68_ENV_REAL (genie_mks_uk_ton, GSL_CONST_MKS_UK_TON)
A68_ENV_REAL (genie_mks_unified_atomic_mass, GSL_CONST_MKS_UNIFIED_ATOMIC_MASS)
A68_ENV_REAL (genie_mks_us_gallon, GSL_CONST_MKS_US_GALLON)
A68_ENV_REAL (genie_mks_vacuum_permeability, GSL_CONST_MKS_VACUUM_PERMEABILITY)
A68_ENV_REAL (genie_mks_vacuum_permittivity, GSL_CONST_MKS_VACUUM_PERMITTIVITY) 
A68_ENV_REAL (genie_mks_week, GSL_CONST_MKS_WEEK)
A68_ENV_REAL (genie_mks_yard, GSL_CONST_MKS_YARD) 
A68_ENV_REAL (genie_num_atto, GSL_CONST_NUM_ATTO)
A68_ENV_REAL (genie_num_avogadro, GSL_CONST_NUM_AVOGADRO) 
A68_ENV_REAL (genie_num_exa, GSL_CONST_NUM_EXA)
A68_ENV_REAL (genie_num_femto, GSL_CONST_NUM_FEMTO) 
A68_ENV_REAL (genie_num_fine_structure, GSL_CONST_NUM_FINE_STRUCTURE)
A68_ENV_REAL (genie_num_giga, GSL_CONST_NUM_GIGA) 
A68_ENV_REAL (genie_num_kilo, GSL_CONST_NUM_KILO)
A68_ENV_REAL (genie_num_mega, GSL_CONST_NUM_MEGA) 
A68_ENV_REAL (genie_num_micro, GSL_CONST_NUM_MICRO)
A68_ENV_REAL (genie_num_milli, GSL_CONST_NUM_MILLI) 
A68_ENV_REAL (genie_num_nano, GSL_CONST_NUM_NANO)
A68_ENV_REAL (genie_num_peta, GSL_CONST_NUM_PETA) 
A68_ENV_REAL (genie_num_pico, GSL_CONST_NUM_PICO)
A68_ENV_REAL (genie_num_tera, GSL_CONST_NUM_TERA) 
A68_ENV_REAL (genie_num_yocto, GSL_CONST_NUM_YOCTO)
A68_ENV_REAL (genie_num_yotta, GSL_CONST_NUM_YOTTA) 
A68_ENV_REAL (genie_num_zepto, GSL_CONST_NUM_ZEPTO)
A68_ENV_REAL (genie_num_zetta, GSL_CONST_NUM_ZETTA)

/* Macros */

#define C_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  VALUE (x) = f (VALUE (x));\
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);

#define OWN_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  VALUE (x) = f (p, VALUE (x));\
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);

#define GSL_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  VALUE (x) = f (VALUE (x));\
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);

#define GSL_COMPLEX_FUNCTION(f)\
  gsl_complex x, z;\
  A68_REAL *rex, *imx;\
  imx = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));\
  rex = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));\
  GSL_SET_COMPLEX (&x, VALUE (rex), VALUE (imx));\
  (void) gsl_set_error_handler_off ();\
  RESET_ERRNO;\
  z = f (x);\
  MATH_RTE (p, errno != 0, MODE (COMPLEX), NO_TEXT);\
  VALUE (imx) = GSL_IMAG(z);\
  VALUE (rex) = GSL_REAL(z)

#define GSL_1_FUNCTION(p, f)\
  A68_REAL *x;\
  gsl_sf_result y;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), &y);\
  MATH_RTE (p, status != 0, MODE (REAL), (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&y)

#define GSL_2_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), &r);\
  MATH_RTE (p, status != 0, MODE (REAL), (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r)

#define GSL_2_INT_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f ((int) VALUE (x), VALUE (y), &r);\
  MATH_RTE (p, status != 0, MODE (REAL), (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r)

#define GSL_3_FUNCTION(p, f)\
  A68_REAL *x, *y, *z;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z),  &r);\
  MATH_RTE (p, status != 0, MODE (REAL), (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r)

#define GSL_1D_FUNCTION(p, f)\
  A68_REAL *x;\
  gsl_sf_result y;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), GSL_PREC_DOUBLE, &y);\
  MATH_RTE (p, status != 0, MODE (REAL), (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&y)

#define GSL_2D_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), GSL_PREC_DOUBLE, &r);\
  MATH_RTE (p, status != 0, MODE (REAL), (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r)

#define GSL_3D_FUNCTION(p, f)\
  A68_REAL *x, *y, *z;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z), GSL_PREC_DOUBLE, &r);\
  MATH_RTE (p, status != 0, MODE (REAL), (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r)

#define GSL_4D_FUNCTION(p, f)\
  A68_REAL *x, *y, *z, *rho;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, rho, A68_REAL);\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z), VALUE (rho), GSL_PREC_DOUBLE, &r);\
  MATH_RTE (p, status != 0, MODE (REAL), (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r)

/*!
\brief the cube root of x
\param x x
\return same
**/

     double curt (double x)
{
#define CBRT2 1.2599210498948731647672;
#define CBRT4 1.5874010519681994747517;
  int expo, sign;
  double z, x0;
  static double y[11] = {
    7.937005259840997e-01,
    8.193212706006459e-01,
    8.434326653017493e-01,
    8.662391053409029e-01,
    8.879040017426008e-01,
    9.085602964160699e-01,
    9.283177667225558e-01,
    9.472682371859097e-01,
    9.654893846056298e-01,
    9.830475724915586e-01,
    1.0
  };
  if (x == 0.0 || x == 1.0) {
    return (x);
  }
  if (x > 0.0) {
    sign = 1;
  } else {
    sign = -1;
    x = -x;
  }
  x = frexp (x, &expo);
/* Cube root in [0.5, 1] by Newton's method */
  z = x;
  x = y[(int) (20 * x - 10)];
  x0 = 0;
  while (ABS (x - x0) > DBL_EPSILON) {
    x0 = x;
    x = (z / (x * x) + x + x) / 3;
  }
/* Get exponent */
  if (expo >= 0) {
    int j = expo % 3;
    if (j == 1) {
      x *= CBRT2;
    } else if (j == 2) {
      x *= CBRT4;
    }
    expo /= 3;
  } else {
    int j = (-expo) % 3;
    if (j == 1) {
      x /= CBRT2;
    } else if (j == 2) {
      x /= CBRT4;
    }
    expo = -(-expo) / 3;
  }
  x = ldexp (x, expo);
  return (sign >= 0 ? x : -x);
}

/*!
\brief inverse complementary error function
\param y y
\return same
**/

double inverfc (double y)
{
  if (y < 0.0 || y > 2.0) {
    errno = EDOM;
    return (0.0);
  } else if (y == 0.0) {
    return (DBL_MAX);
  } else if (y == 1.0) {
    return (0.0);
  } else if (y == 2.0) {
    return (-DBL_MAX);
  } else {
/* Next is adapted code from a package that contains following statement:
   Copyright (c) 1996 Takuya Ooura.
   You may use, copy, modify this code for any purpose and without fee */
    double s, t, u, v, x, z;
    if (y <= 1.0) {
      z = y;
    } else {
      z = 2.0 - y;
    }
    v = 0.916461398268964 - log (z);
    u = sqrt (v);
    s = (log (u) + 0.488826640273108) / v;
    t = 1.0 / (u + 0.231729200323405);
    x = u * (1.0 - s * (s * 0.124610454613712 + 0.5)) - ((((-0.0728846765585675 * t + 0.269999308670029) * t + 0.150689047360223) * t + 0.116065025341614) * t + 0.499999303439796) * t;
    t = 3.97886080735226 / (x + 3.97886080735226);
    u = t - 0.5;
    s = (((((((((0.00112648096188977922 * u + 1.05739299623423047e-4) * u - 0.00351287146129100025) * u - 7.71708358954120939e-4) * u + 0.00685649426074558612) * u + 0.00339721910367775861) * u - 0.011274916933250487) * u - 0.0118598117047771104) * u + 0.0142961988697898018) * u + 0.0346494207789099922) * u + 0.00220995927012179067;
    s = ((((((((((((s * u - 0.0743424357241784861) * u - 0.105872177941595488) * u + 0.0147297938331485121) * u + 0.316847638520135944) * u + 0.713657635868730364) * u + 1.05375024970847138) * u + 1.21448730779995237) * u + 1.16374581931560831) * u + 0.956464974744799006) * u + 0.686265948274097816) * u + 0.434397492331430115) * u + 0.244044510593190935) * t - z * exp (x * x - 0.120782237635245222);
    x += s * (x * s + 1.0);
    return (y <= 1.0 ? x : -x);
  }
}

/*!
\brief inverse error function
\param y y
\return same
**/

double inverf (double y)
{
  return (inverfc (1 - y));
}

/*!
\brief PROC sqrt = (REAL) REAL
\param p position in tree
**/

void genie_sqrt_real (NODE_T * p)
{
  C_FUNCTION (p, sqrt);
}

/*!
\brief PROC curt = (REAL) REAL
\param p position in tree
**/

void genie_curt_real (NODE_T * p)
{
  C_FUNCTION (p, curt);
}

/*!
\brief PROC exp = (REAL) REAL
\param p position in tree
**/

void genie_exp_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_exp);
}

/*!
\brief PROC ln = (REAL) REAL
\param p position in tree
**/

void genie_ln_real (NODE_T * p)
{
  C_FUNCTION (p, log);
}

/*!
\brief PROC log = (REAL) REAL
\param p position in tree
**/

void genie_log_real (NODE_T * p)
{
  C_FUNCTION (p, log10);
}

/*!
\brief PROC sin = (REAL) REAL
\param p position in tree
**/

void genie_sin_real (NODE_T * p)
{
  C_FUNCTION (p, sin);
}

/*!
\brief PROC arcsin = (REAL) REAL
\param p position in tree
**/

void genie_arcsin_real (NODE_T * p)
{
  C_FUNCTION (p, asin);
}

/*!
\brief PROC cos = (REAL) REAL
\param p position in tree
**/

void genie_cos_real (NODE_T * p)
{
  C_FUNCTION (p, cos);
}

/*!
\brief PROC arccos = (REAL) REAL
\param p position in tree
**/

void genie_arccos_real (NODE_T * p)
{
  C_FUNCTION (p, acos);
}

/*!
\brief PROC tan = (REAL) REAL
\param p position in tree
**/

void genie_tan_real (NODE_T * p)
{
  C_FUNCTION (p, tan);
}

/*!
\brief PROC arctan = (REAL) REAL
\param p position in tree
**/

void genie_arctan_real (NODE_T * p)
{
  C_FUNCTION (p, atan);
}

/*!
\brief PROC arctan2 = (REAL) REAL
\param p position in tree
**/

void genie_atan2_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  RESET_ERRNO;
  PRELUDE_ERROR (VALUE (x) == 0.0 && VALUE (y) == 0.0, p, ERROR_INVALID_ARGUMENT, MODE (LONG_REAL));
  VALUE (x) = a68g_atan2 (VALUE (y), VALUE (x));
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

/*!
\brief PROC sinh = (REAL) REAL
\param p position in tree
**/

void genie_sinh_real (NODE_T * p)
{
  C_FUNCTION (p, sinh);
}

/*!
\brief PROC cosh = (REAL) REAL
\param p position in tree
**/

void genie_cosh_real (NODE_T * p)
{
  C_FUNCTION (p, cosh);
}

/*!
\brief PROC tanh = (REAL) REAL
\param p position in tree
**/

void genie_tanh_real (NODE_T * p)
{
  C_FUNCTION (p, tanh);
}

/*!
\brief PROC arcsinh = (REAL) REAL
\param p position in tree
**/

void genie_arcsinh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_asinh);
}

/*!
\brief PROC arccosh = (REAL) REAL
\param p position in tree
**/

void genie_arccosh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_acosh);
}

/*!
\brief PROC arctanh = (REAL) REAL
\param p position in tree
**/

void genie_arctanh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_atanh);
}

/*!
\brief PROC inverse erf = (REAL) REAL
\param p position in tree
**/

void genie_inverf_real (NODE_T * p)
{
  C_FUNCTION (p, inverf);
}

/*!
\brief PROC inverse erfc = (REAL) REAL 
\param p position in tree
**/

void genie_inverfc_real (NODE_T * p)
{
  C_FUNCTION (p, inverfc);
}

/*!
\brief PROC lj e 12 6 = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_lj_e_12_6 (NODE_T * p)
{
  A68_REAL *e, *s, *r;
  double u, u2, u6;
  POP_3_OPERAND_ADDRESSES (p, e, s, r, A68_REAL);
  u = (VALUE (s) / VALUE (r));
  u2 = u * u;
  u6 = u2 * u2 * u2;
  VALUE (e) = 4.0 * VALUE (e) * u6 * (u6 - 1.0);
}

/*!
\brief PROC lj f 12 6 = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_lj_f_12_6 (NODE_T * p)
{
  A68_REAL *e, *s, *r;
  double u, u2, u6;
  POP_3_OPERAND_ADDRESSES (p, e, s, r, A68_REAL);
  u = (VALUE (s) / VALUE (r));
  u2 = u * u;
  u6 = u2 * u2 * u2;
  VALUE (e) = 24.0 * VALUE (e) * u * u6 * (1.0 - 2.0 * u6);
}

#if defined HAVE_GNU_GSL

/* "Special" functions - but what is so "special" about them? */

/*!
\brief PROC erf = (REAL) REAL
\param p position in tree
**/

void genie_erf_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_erf_e);
}

/*!
\brief PROC erfc = (REAL) REAL
\param p position in tree
**/

void genie_erfc_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_erfc_e);
}

/*!
\brief PROC gamma = (REAL) REAL
\param p position in tree
**/

void genie_gamma_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_gamma_e);
}

/*!
\brief PROC gamma incomplete = (REAL, REAL) REAL
\param p position in tree
**/

void genie_gamma_inc_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_gamma_inc_P_e);
}

/*!
\brief PROC lngamma = (REAL) REAL
\param p position in tree
**/

void genie_lngamma_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_lngamma_e);
}

/*!
\brief PROC factorial = (REAL) REAL
\param p position in tree
**/

void genie_factorial_real (NODE_T * p)
{
/* gsl_sf_fact reduces argument to int, hence we do gamma (x + 1) */
  A68_REAL *z = (A68_REAL *) STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL));
  VALUE (z) += 1.0;
  {
    GSL_1_FUNCTION (p, gsl_sf_gamma_e);
  }
}

/*!
\brief PROC beta = (REAL, REAL) REAL
\param p position in tree
**/

void genie_beta_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_beta_e);
}

/*!
\brief PROC beta incomplete = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_beta_inc_real (NODE_T * p)
{
  GSL_3_FUNCTION (p, gsl_sf_beta_inc_e);
}

/*!
\brief PROC airy ai = (REAL) REAL
\param p position in tree
**/

void genie_airy_ai_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Ai_e);
}

/*!
\brief PROC airy bi = (REAL) REAL
\param p position in tree
**/

void genie_airy_bi_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Bi_e);
}

/*!
\brief PROC airy ai derivative = (REAL) REAL
\param p position in tree
**/

void genie_airy_ai_deriv_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Ai_deriv_e);
}

/*!
\brief PROC airy bi derivative = (REAL) REAL
\param p position in tree
**/

void genie_airy_bi_deriv_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Bi_deriv_e);
}

/*!
\brief PROC bessel jn = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_jn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Jn_e);
}

/*!
\brief PROC bessel yn = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_yn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Yn_e);
}

/*!
\brief PROC bessel in = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_in_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_In_e);
}

/*!
\brief PROC bessel exp in = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_in_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_In_scaled_e);
}

/*!
\brief PROC bessel kn = (REAL, REAL) REAL 
\param p position in tree
**/

void genie_bessel_kn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Kn_e);
}

/*!
\brief PROC bessel exp kn = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_kn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Kn_scaled_e);
}

/*!
\brief PROC bessel jl = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_jl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_jl_e);
}

/*!
\brief PROC bessel yl = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_yl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_yl_e);
}

/*!
\brief PROC bessel exp il = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_il_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_il_scaled_e);
}

/*!
\brief PROC bessel exp kl = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_kl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_kl_scaled_e);
}

/*!
\brief PROC bessel jnu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_jnu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Jnu_e);
}

/*!
\brief PROC bessel ynu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_ynu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Ynu_e);
}

/*!
\brief PROC bessel inu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_inu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Inu_e);
}

/*!
\brief PROC bessel exp inu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_inu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Inu_scaled_e);
}

/*!
\brief PROC bessel knu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_knu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Knu_e);
}

/*!
\brief PROC bessel exp knu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_knu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Knu_scaled_e);
}

/*!
\brief PROC elliptic integral k = (REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_k_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_ellint_Kcomp_e);
}

/*!
\brief PROC elliptic integral e = (REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_e_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_ellint_Ecomp_e);
}

/*!
\brief PROC elliptic integral rf = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_rf_real (NODE_T * p)
{
  GSL_3D_FUNCTION (p, gsl_sf_ellint_RF_e);
}

/*!
\brief PROC elliptic integral rd = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_rd_real (NODE_T * p)
{
  GSL_3D_FUNCTION (p, gsl_sf_ellint_RD_e);
}

/*!
\brief PROC elliptic integral rj = (REAL, REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_rj_real (NODE_T * p)
{
  GSL_4D_FUNCTION (p, gsl_sf_ellint_RJ_e);
}

/*!
\brief PROC elliptic integral rc = (REAL, REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_rc_real (NODE_T * p)
{
  GSL_2D_FUNCTION (p, gsl_sf_ellint_RC_e);
}

#endif

/*
Next part is a "stand-alone" version of GNU Scientific Library (GSL)
random number generator "taus113", based on GSL file "rng/taus113.c" that
has the notice:

Copyright (C) 2002 Atakan Gurkan
Based on the file taus.c which has the notice
Copyright (C) 1996, 1997, 1998, 1999, 2000 James Theiler, Brian Gough.

This is a maximally equidistributed combined, collision free
Tausworthe generator, with a period ~2^113 (~10^34).
The sequence is

x_n = (z1_n ^ z2_n ^ z3_n ^ z4_n)

b = (((z1_n <<  6) ^ z1_n) >> 13)
z1_{n+1} = (((z1_n & 4294967294) << 18) ^ b)
b = (((z2_n <<  2) ^ z2_n) >> 27)
z2_{n+1} = (((z2_n & 4294967288) <<  2) ^ b)
b = (((z3_n << 13) ^ z3_n) >> 21)
z3_{n+1} = (((z3_n & 4294967280) <<  7) ^ b)
b = (((z4_n <<  3)  ^ z4_n) >> 12)
z4_{n+1} = (((z4_n & 4294967168) << 13) ^ b)

computed modulo 2^32. In the formulas above '^' means exclusive-or
(C-notation), not exponentiation.
The algorithm is for 32-bit integers, hence a bitmask is used to clear
all but least significant 32 bits, after left shifts, to make the code
work on architectures where integers are 64-bit.

The generator is initialized with
zi = (69069 * z{i+1}) MOD 2^32 where z0 is the seed provided
During initialization a check is done to make sure that the initial seeds
have a required number of their most significant bits set.
After this, the state is passed through the RNG 10 times to ensure the
state satisfies a recurrence relation.

References:
P. L'Ecuyer, "Tables of Maximally-Equidistributed Combined LFSR Generators",
Mathematics of Computation, 68, 225 (1999), 261--269.
  http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme2.ps
P. L'Ecuyer, "Maximally Equidistributed Combined Tausworthe Generators",
Mathematics of Computation, 65, 213 (1996), 203--213.
  http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme.ps
the online version of the latter contains corrections to the print version.
*/

#define LCG(n) ((69069UL * n) & 0xffffffffUL)
#define TAUSWORTHE_MASK 0xffffffffUL
#define Z1(p) ((p)->z1)
#define Z2(p) ((p)->z2)
#define Z3(p) ((p)->z3)
#define Z4(p) ((p)->z4)

typedef struct
{
  unsigned long int z1, z2, z3, z4;
} taus113_state_t;

static taus113_state_t rng_state;

static unsigned long int taus113_get (taus113_state_t * state);
static void taus113_set (taus113_state_t * state, unsigned long int s);

/*!
\brief taus113_get
\param state state
\return same
**/

static unsigned long taus113_get (taus113_state_t * state)
{
  unsigned long b1, b2, b3, b4;
  b1 = ((((Z1 (state) << 6UL) & TAUSWORTHE_MASK) ^ Z1 (state)) >> 13UL);
  Z1 (state) = ((((Z1 (state) & 4294967294UL) << 18UL) & TAUSWORTHE_MASK) ^ b1);
  b2 = ((((Z2 (state) << 2UL) & TAUSWORTHE_MASK) ^ Z2 (state)) >> 27UL);
  Z2 (state) = ((((Z2 (state) & 4294967288UL) << 2UL) & TAUSWORTHE_MASK) ^ b2);
  b3 = ((((Z3 (state) << 13UL) & TAUSWORTHE_MASK) ^ Z3 (state)) >> 21UL);
  Z3 (state) = ((((Z3 (state) & 4294967280UL) << 7UL) & TAUSWORTHE_MASK) ^ b3);
  b4 = ((((Z4 (state) << 3UL) & TAUSWORTHE_MASK) ^ Z4 (state)) >> 12UL);
  Z4 (state) = ((((Z4 (state) & 4294967168UL) << 13UL) & TAUSWORTHE_MASK) ^ b4);
  return (Z1 (state) ^ Z2 (state) ^ Z3 (state) ^ Z4 (state));
}

/*!
\brief taus113_set
\param state state
\param s s
**/

static void taus113_set (taus113_state_t * state, unsigned long int s)
{
  if (!s) {
/* default seed is 1 */
    s = 1UL;
  }
  Z1 (state) = LCG (s);
  if (Z1 (state) < 2UL) {
    Z1 (state) += 2UL;
  }
  Z2 (state) = LCG (Z1 (state));
  if (Z2 (state) < 8UL) {
    Z2 (state) += 8UL;
  }
  Z3 (state) = LCG (Z2 (state));
  if (Z3 (state) < 16UL) {
    Z3 (state) += 16UL;
  }
  Z4 (state) = LCG (Z3 (state));
  if (Z4 (state) < 128UL) {
    Z4 (state) += 128UL;
  }
/* Calling RNG ten times to satify recurrence condition */
  (void) taus113_get (state);
  (void) taus113_get (state);
  (void) taus113_get (state);
  (void) taus113_get (state);
  (void) taus113_get (state);
  (void) taus113_get (state);
  (void) taus113_get (state);
  (void) taus113_get (state);
  (void) taus113_get (state);
  (void) taus113_get (state);
}

/*!
\brief initialise rng
\param u initialiser
**/

void init_rng (unsigned long u)
{
  taus113_set (&rng_state, u);
}

/*!
\brief rng 53 bit
\return same
**/

double rng_53_bit (void)
{
  double a = (double) (taus113_get (&rng_state) >> 5);
  double b = (double) (taus113_get (&rng_state) >> 6);
  return (a * /* 2^26 */ 67108864.0 + b) / /* 2^53 */ 9007199254740992.0;
}

/*
Rules for analytic calculations using GNU Emacs Calc:
(used to find the values for the test program)

[ LCG(n) := n * 69069 mod (2^32) ]

[ b1(x) := rsh(xor(lsh(x, 6), x), 13),
q1(x) := xor(lsh(and(x, 4294967294), 18), b1(x)),
b2(x) := rsh(xor(lsh(x, 2), x), 27),
q2(x) := xor(lsh(and(x, 4294967288), 2), b2(x)),
b3(x) := rsh(xor(lsh(x, 13), x), 21),
q3(x) := xor(lsh(and(x, 4294967280), 7), b3(x)),
b4(x) := rsh(xor(lsh(x, 3), x), 12),
q4(x) := xor(lsh(and(x, 4294967168), 13), b4(x))
]

[ S([z1,z2,z3,z4]) := [q1(z1), q2(z2), q3(z3), q4(z4)] ]		 
*/

/*
This file also contains Algol68G's standard environ for complex numbers.
Some of the LONG operations are generic for LONG and LONG LONG.

Some routines are based on
* GNU Scientific Library
* Abramowitz and Stegun.
*/

#if defined HAVE_GNU_GSL

#define GSL_COMPLEX_FUNCTION(f)\
  gsl_complex x, z;\
  A68_REAL *rex, *imx;\
  imx = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));\
  rex = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));\
  GSL_SET_COMPLEX (&x, VALUE (rex), VALUE (imx));\
  (void) gsl_set_error_handler_off ();\
  RESET_ERRNO;\
  z = f (x);\
  MATH_RTE (p, errno != 0, MODE (COMPLEX), NO_TEXT);\
  VALUE (imx) = GSL_IMAG(z);\
  VALUE (rex) = GSL_REAL(z)

#endif

/*!
\brief OP +* = (REAL, REAL) COMPLEX
\param p position in tree
**/

void genie_icomplex (NODE_T * p)
{
  (void) p;
}

/*!
\brief OP +* = (INT, INT) COMPLEX
\param p position in tree
**/

void genie_iint_complex (NODE_T * p)
{
  A68_INT re, im;
  POP_OBJECT (p, &im, A68_INT);
  POP_OBJECT (p, &re, A68_INT);
  PUSH_PRIMITIVE (p, (double) VALUE (&re), A68_REAL);
  PUSH_PRIMITIVE (p, (double) VALUE (&im), A68_REAL);
}

/*!
\brief OP RE = (COMPLEX) REAL
\param p position in tree
**/

void genie_re_complex (NODE_T * p)
{
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REAL));
}

/*!
\brief OP IM = (COMPLEX) REAL
\param p position in tree
**/

void genie_im_complex (NODE_T * p)
{
  A68_REAL im;
  POP_OBJECT (p, &im, A68_REAL);
  *(A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL))) = im;
}

/*!
\brief OP - = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_minus_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x;
  im_x = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  VALUE (im_x) = -VALUE (im_x);
  VALUE (re_x) = -VALUE (re_x);
  (void) p;
}

/*!
\brief ABS = (COMPLEX) REAL
\param p position in tree
**/

void genie_abs_complex (NODE_T * p)
{
  A68_REAL re_x, im_x;
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_PRIMITIVE (p, a68g_hypot (VALUE (&re_x), VALUE (&im_x)), A68_REAL);
}

/*!
\brief OP ARG = (COMPLEX) REAL
\param p position in tree
**/

void genie_arg_complex (NODE_T * p)
{
  A68_REAL re_x, im_x;
  POP_COMPLEX (p, &re_x, &im_x);
  PRELUDE_ERROR (VALUE (&re_x) == 0.0 && VALUE (&im_x) == 0.0, p, ERROR_INVALID_ARGUMENT, MODE (COMPLEX));
  PUSH_PRIMITIVE (p, atan2 (VALUE (&im_x), VALUE (&re_x)), A68_REAL);
}

/*!
\brief OP CONJ = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_conj_complex (NODE_T * p)
{
  A68_REAL *im;
  POP_OPERAND_ADDRESS (p, im, A68_REAL);
  VALUE (im) = -VALUE (im);
}

/*!
\brief OP + = (COMPLEX, COMPLEX) COMPLEX
\param p position in tree
**/

void genie_add_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  im_x = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  VALUE (im_x) += VALUE (&im_y);
  VALUE (re_x) += VALUE (&re_y);
  CHECK_COMPLEX_REPRESENTATION (p, VALUE (re_x), VALUE (im_x));
}

/*!
\brief OP - = (COMPLEX, COMPLEX) COMPLEX
\param p position in tree
**/

void genie_sub_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  im_x = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  VALUE (im_x) -= VALUE (&im_y);
  VALUE (re_x) -= VALUE (&re_y);
  CHECK_COMPLEX_REPRESENTATION (p, VALUE (re_x), VALUE (im_x));
}

/*!
\brief OP * = (COMPLEX, COMPLEX) COMPLEX
\param p position in tree
**/

void genie_mul_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  double re, im;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  re = VALUE (&re_x) * VALUE (&re_y) - VALUE (&im_x) * VALUE (&im_y);
  im = VALUE (&im_x) * VALUE (&re_y) + VALUE (&re_x) * VALUE (&im_y);
  CHECK_COMPLEX_REPRESENTATION (p, re, im);
  PUSH_COMPLEX (p, re, im);
}

/*!
\brief OP / = (COMPLEX, COMPLEX) COMPLEX
\param p position in tree
**/

void genie_div_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  double re = 0.0, im = 0.0;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
#if ! defined HAVE_IEEE_754
  PRELUDE_ERROR (VALUE (&re_y) == 0.0 && VALUE (&im_y) == 0.0, p, ERROR_DIVISION_BY_ZERO, MODE (COMPLEX));
#endif
  if (ABS (VALUE (&re_y)) >= ABS (VALUE (&im_y))) {
    double r = VALUE (&im_y) / VALUE (&re_y), den = VALUE (&re_y) + r * VALUE (&im_y);
    re = (VALUE (&re_x) + r * VALUE (&im_x)) / den;
    im = (VALUE (&im_x) - r * VALUE (&re_x)) / den;
  } else {
    double r = VALUE (&re_y) / VALUE (&im_y), den = VALUE (&im_y) + r * VALUE (&re_y);
    re = (VALUE (&re_x) * r + VALUE (&im_x)) / den;
    im = (VALUE (&im_x) * r - VALUE (&re_x)) / den;
  }
  CHECK_COMPLEX_REPRESENTATION (p, re, im);
  PUSH_COMPLEX (p, re, im);
}

/*!
\brief OP ** = (COMPLEX, INT) COMPLEX
\param p position in tree
**/

void genie_pow_complex_int (NODE_T * p)
{
  A68_REAL re_x, im_x;
  double re_y, im_y, re_z, im_z, rea;
  A68_INT j;
  int expo;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  POP_COMPLEX (p, &re_x, &im_x);
  re_z = 1.0;
  im_z = 0.0;
  re_y = VALUE (&re_x);
  im_y = VALUE (&im_x);
  expo = 1;
  negative = (BOOL_T) (VALUE (&j) < 0);
  if (negative) {
    VALUE (&j) = -VALUE (&j);
  }
  while ((unsigned) expo <= (unsigned) (VALUE (&j))) {
    if (expo & VALUE (&j)) {
      rea = re_z * re_y - im_z * im_y;
      im_z = re_z * im_y + im_z * re_y;
      re_z = rea;
    }
    rea = re_y * re_y - im_y * im_y;
    im_y = im_y * re_y + re_y * im_y;
    re_y = rea;
    expo <<= 1;
  }
  CHECK_COMPLEX_REPRESENTATION (p, re_z, im_z);
  if (negative) {
    PUSH_PRIMITIVE (p, 1.0, A68_REAL);
    PUSH_PRIMITIVE (p, 0.0, A68_REAL);
    PUSH_PRIMITIVE (p, re_z, A68_REAL);
    PUSH_PRIMITIVE (p, im_z, A68_REAL);
    genie_div_complex (p);
  } else {
    PUSH_PRIMITIVE (p, re_z, A68_REAL);
    PUSH_PRIMITIVE (p, im_z, A68_REAL);
  }
}

/*!
\brief OP = = (COMPLEX, COMPLEX) BOOL
\param p position in tree
**/

void genie_eq_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_PRIMITIVE (p, (BOOL_T) ((VALUE (&re_x) == VALUE (&re_y)) && (VALUE (&im_x) == VALUE (&im_y))), A68_BOOL);
}

/*!
\brief OP /= = (COMPLEX, COMPLEX) BOOL
\param p position in tree
**/

void genie_ne_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_PRIMITIVE (p, (BOOL_T) ! ((VALUE (&re_x) == VALUE (&re_y)) && (VALUE (&im_x) == VALUE (&im_y))), A68_BOOL);
}

/*!
\brief OP +:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in tree
**/

void genie_plusab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_add_complex);
}

/*!
\brief OP -:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in tree
**/

void genie_minusab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_sub_complex);
}

/*!
\brief OP *:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in tree
**/

void genie_timesab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_mul_complex);
}

/*!
\brief OP /:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in tree
**/

void genie_divab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_div_complex);
}

/*!
\brief OP LENG = (COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_lengthen_complex_to_long_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL));
  MP_T *z;
  A68_REAL a, b;
  POP_OBJECT (p, &b, A68_REAL);
  POP_OBJECT (p, &a, A68_REAL);
  STACK_MP (z, p, digits);
  (void) real_to_mp (p, z, VALUE (&a), digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  STACK_MP (z, p, digits);
  (void) real_to_mp (p, z, VALUE (&b), digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

/*!
\brief OP SHORTEN = (LONG COMPLEX) COMPLEX
\param p position in tree
**/

void genie_shorten_long_complex_to_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL)), size = get_mp_size (MODE (LONG_REAL));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, mp_to_real (p, a, digits), A68_REAL);
  PUSH_PRIMITIVE (p, mp_to_real (p, b, digits), A68_REAL);
}

/*!
\brief OP LENG = (LONG COMPLEX) LONG LONG COMPLEX
\param p position in tree
**/

void genie_lengthen_long_complex_to_longlong_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL)), size = get_mp_size (MODE (LONG_REAL));
  int digs_long = get_mp_digits (MODE (LONGLONG_REAL)), size_long = get_mp_size (MODE (LONGLONG_REAL));
  ADDR_T pop_sp = stack_pointer;
  MP_T *a, *b, *c, *d;
  b = (MP_T *) STACK_OFFSET (-size);
  a = (MP_T *) STACK_OFFSET (-2 * size);
  STACK_MP (c, p, digs_long);
  STACK_MP (d, p, digs_long);
  (void) lengthen_mp (p, c, digs_long, a, digits);
  (void) lengthen_mp (p, d, digs_long, b, digits);
  MOVE_MP (a, c, digs_long);
  MOVE_MP (&a[2 + digs_long], d, digs_long);
  stack_pointer = pop_sp;
  MP_STATUS (a) = (MP_T) INIT_MASK;
  (&a[2 + digs_long])[0] = (MP_T) INIT_MASK;
  INCREMENT_STACK_POINTER (p, 2 * (size_long - size));
}

/*!
\brief OP SHORTEN = (LONG LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_shorten_longlong_complex_to_long_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL)), size = get_mp_size (MODE (LONG_REAL));
  int digs_long = get_mp_digits (MODE (LONGLONG_REAL)), size_long = get_mp_size (MODE (LONGLONG_REAL));
  ADDR_T pop_sp = stack_pointer;
  MP_T *a, *b;
  b = (MP_T *) STACK_OFFSET (-size_long);
  a = (MP_T *) STACK_OFFSET (-2 * size_long);
  (void) shorten_mp (p, a, digits, a, digs_long);
  (void) shorten_mp (p, &a[2 + digits], digits, b, digs_long);
  stack_pointer = pop_sp;
  MP_STATUS (a) = (MP_T) INIT_MASK;
  (&a[2 + digits])[0] = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, 2 * (size_long - size));
}

/*!
\brief OP RE = (LONG COMPLEX) LONG REAL
\param p position in tree
**/

void genie_re_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, (int) size_long_mp ());
}

/*!
\brief OP IM = (LONG COMPLEX) LONG REAL
\param p position in tree
**/

void genie_im_long_complex (NODE_T * p)
{
  int digits = get_mp_digits (LHS_MODE (p)), size = get_mp_size (MOID (PACK (MOID (p))));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MOVE_MP (a, b, digits);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, (int) size_long_mp ());
}

/*!
\brief OP - = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_minus_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT (a, 1) = -MP_DIGIT (a, 1);
  MP_DIGIT (b, 1) = -MP_DIGIT (b, 1);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
}

/*!
\brief OP CONJ = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_conj_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT (b, 1) = -MP_DIGIT (b, 1);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
}

/*!
\brief OP ABS = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_abs_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *z;
  STACK_MP (z, p, digits);
  (void) hypot_mp (p, z, a, b, digits);
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
  MOVE_MP (a, z, digits);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief OP ARG = (LONG COMPLEX) LONG REAL
\param p position in tree
**/

void genie_arg_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *z;
  STACK_MP (z, p, digits);
  (void) atan2_mp (p, z, a, b, digits);
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
  MOVE_MP (a, z, digits);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief OP + = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_add_long_complex (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  (void) add_mp (p, b, b, d, digits);
  (void) add_mp (p, a, a, c, digits);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

/*!
\brief OP - = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_sub_long_complex (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  (void) sub_mp (p, b, b, d, digits);
  (void) sub_mp (p, a, a, c, digits);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

/*!
\brief OP * = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_mul_long_complex (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  (void) cmul_mp (p, a, b, c, d, digits);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

/*!
\brief OP / = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_div_long_complex (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  PRELUDE_ERROR (cdiv_mp (p, a, b, c, d, digits) == NO_MP, p, ERROR_DIVISION_BY_ZERO, mode);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

/*!
\brief OP ** = (LONG COMPLEX, INT) LONG COMPLEX
\param p position in tree
**/

void genie_pow_long_complex_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp;
  MP_T *re_x, *im_x, *re_y, *im_y, *re_z, *im_z, *rea, *acc;
  A68_INT j;
  int expo;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  pop_sp = stack_pointer;
  im_x = (MP_T *) STACK_OFFSET (-size);
  re_x = (MP_T *) STACK_OFFSET (-2 * size);
  STACK_MP (re_z, p, digits);
  (void) set_mp_short (re_z, (MP_T) 1, 0, digits);
  STACK_MP (im_z, p, digits);
  SET_MP_ZERO (im_z, digits);
  STACK_MP (re_y, p, digits);
  STACK_MP (im_y, p, digits);
  MOVE_MP (re_y, re_x, digits);
  MOVE_MP (im_y, im_x, digits);
  STACK_MP (rea, p, digits);
  STACK_MP (acc, p, digits);
  expo = 1;
  negative = (BOOL_T) (VALUE (&j) < 0);
  if (negative) {
    VALUE (&j) = -VALUE (&j);
  }
  while ((unsigned) expo <= (unsigned) (VALUE (&j))) {
    if (expo & VALUE (&j)) {
     (void) mul_mp (p, acc, im_z, im_y, digits);
     (void) mul_mp (p, rea, re_z, re_y, digits);
     (void) sub_mp (p, rea, rea, acc, digits);
     (void) mul_mp (p, acc, im_z, re_y, digits);
     (void) mul_mp (p, im_z, re_z, im_y, digits);
     (void) add_mp (p, im_z, im_z, acc, digits);
      MOVE_MP (re_z, rea, digits);
    }
   (void) mul_mp (p, acc, im_y, im_y, digits);
   (void) mul_mp (p, rea, re_y, re_y, digits);
   (void) sub_mp (p, rea, rea, acc, digits);
   (void) mul_mp (p, acc, im_y, re_y, digits);
   (void) mul_mp (p, im_y, re_y, im_y, digits);
   (void) add_mp (p, im_y, im_y, acc, digits);
    MOVE_MP (re_y, rea, digits);
    expo <<= 1;
  }
  stack_pointer = pop_sp;
  if (negative) {
   (void) set_mp_short (re_x, (MP_T) 1, 0, digits);
    SET_MP_ZERO (im_x, digits);
    INCREMENT_STACK_POINTER (p, 2 * size);
    genie_div_long_complex (p);
  } else {
    MOVE_MP (re_x, re_z, digits);
    MOVE_MP (im_x, im_z, digits);
  }
  MP_STATUS (re_x) = (MP_T) INIT_MASK;
  MP_STATUS (im_x) = (MP_T) INIT_MASK;
}

/*!
\brief OP = = (LONG COMPLEX, LONG COMPLEX) BOOL
\param p position in tree
**/

void genie_eq_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  genie_sub_long_complex (p);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, (BOOL_T) (MP_DIGIT (a, 1) == 0 && MP_DIGIT (b, 1) == 0), A68_BOOL);
}

/*!
\brief OP /= = (LONG COMPLEX, LONG COMPLEX) BOOL
\param p position in tree
**/

void genie_ne_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  genie_sub_long_complex (p);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, (BOOL_T) (MP_DIGIT (a, 1) != 0 || MP_DIGIT (b, 1) != 0), A68_BOOL);
}

/*!
\brief OP +:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in tree
**/

void genie_plusab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_complex);
}

/*!
\brief OP -:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in tree
**/

void genie_minusab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_long_complex);
}

/*!
\brief OP *:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in tree
**/

void genie_timesab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_complex);
}

/*!
\brief OP /:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in tree
**/

void genie_divab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_div_long_complex);
}

/*!
\brief PROC csqrt = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_sqrt_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (VALUE (re) == 0.0 && VALUE (im) == 0.0) {
    VALUE (re) = 0.0;
    VALUE (im) = 0.0;
  } else {
    double x = ABS (VALUE (re)), y = ABS (VALUE (im)), w;
    if (x >= y) {
      double t = y / x;
      w = sqrt (x) * sqrt (0.5 * (1.0 + sqrt (1.0 + t * t)));
    } else {
      double t = x / y;
      w = sqrt (y) * sqrt (0.5 * (t + sqrt (1.0 + t * t)));
    }
    if (VALUE (re) >= 0.0) {
      VALUE (re) = w;
      VALUE (im) = VALUE (im) / (2.0 * w);
    } else {
      double ai = VALUE (im);
      double vi = (ai >= 0.0 ? w : -w);
      VALUE (re) = ai / (2.0 * vi);
      VALUE (im) = vi;
    }
  }
  MATH_RTE (p, errno != 0, MODE (COMPLEX), NO_TEXT);
}

/*!
\brief PROC long csqrt = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_sqrt_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  RESET_ERRNO;
  (void) csqrt_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief PROC cexp = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_exp_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  double r;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  r = exp (VALUE (re));
  VALUE (re) = r * cos (VALUE (im));
  VALUE (im) = r * sin (VALUE (im));
  MATH_RTE (p, errno != 0, MODE (COMPLEX), NO_TEXT);
}

/*!
\brief PROC long cexp = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_exp_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  (void) cexp_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief PROC cln = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_ln_complex (NODE_T * p)
{
  A68_REAL *re, *im, r, th;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  PUSH_COMPLEX (p, VALUE (re), VALUE (im));
  genie_abs_complex (p);
  POP_OBJECT (p, &r, A68_REAL);
  PUSH_COMPLEX (p, VALUE (re), VALUE (im));
  genie_arg_complex (p);
  POP_OBJECT (p, &th, A68_REAL);
  VALUE (re) = log (VALUE (&r));
  VALUE (im) = VALUE (&th);
  MATH_RTE (p, errno != 0, MODE (COMPLEX), NO_TEXT);
}

/*!
\brief PROC long cln = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_ln_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  (void) cln_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief PROC csin = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_sin_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (VALUE (im) == 0.0) {
    VALUE (re) = sin (VALUE (re));
    VALUE (im) = 0.0;
  } else {
    double r = VALUE (re), i = VALUE (im);
    VALUE (re) = sin (r) * cosh (i);
    VALUE (im) = cos (r) * sinh (i);
  }
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);
}

/*!
\brief PROC long csin = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_sin_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  (void) csin_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief PROC ccos = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_cos_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (VALUE (im) == 0.0) {
    VALUE (re) = cos (VALUE (re));
    VALUE (im) = 0.0;
  } else {
    double r = VALUE (re), i = VALUE (im);
    VALUE (re) = cos (r) * cosh (i);
    VALUE (im) = sin (r) * sinh (-i);
  }
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);
}

/*!
\brief PROC long ccos = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_cos_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  (void) ccos_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief PROC ctan = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_tan_complex (NODE_T * p)
{
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  double r, i;
  A68_REAL u, v;
  RESET_ERRNO;
  r = VALUE (re);
  i = VALUE (im);
  PUSH_PRIMITIVE (p, r, A68_REAL);
  PUSH_PRIMITIVE (p, i, A68_REAL);
  genie_sin_complex (p);
  POP_OBJECT (p, &v, A68_REAL);
  POP_OBJECT (p, &u, A68_REAL);
  PUSH_PRIMITIVE (p, r, A68_REAL);
  PUSH_PRIMITIVE (p, i, A68_REAL);
  genie_cos_complex (p);
  VALUE (re) = VALUE (&u);
  VALUE (im) = VALUE (&v);
  genie_div_complex (p);
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);
}

/*!
\brief PROC long ctan = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_tan_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  (void) ctan_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief PROC carcsin= (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arcsin_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    VALUE (re) = asin (VALUE (re));
  } else {
    double r = VALUE (re), i = VALUE (im);
    double u = a68g_hypot (r + 1, i), v = a68g_hypot (r - 1, i);
    double a = 0.5 * (u + v), b = 0.5 * (u - v);
    VALUE (re) = asin (b);
    VALUE (im) = log (a + sqrt (a * a - 1));
  }
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);
}

/*!
\brief PROC long arcsin = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_asin_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  RESET_ERRNO;
  (void) casin_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief PROC carccos = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arccos_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    VALUE (re) = acos (VALUE (re));
  } else {
    double r = VALUE (re), i = VALUE (im);
    double u = a68g_hypot (r + 1, i), v = a68g_hypot (r - 1, i);
    double a = 0.5 * (u + v), b = 0.5 * (u - v);
    VALUE (re) = acos (b);
    VALUE (im) = -log (a + sqrt (a * a - 1));
  }
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);
}

/*!
\brief PROC long carccos = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_acos_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  RESET_ERRNO;
  (void) cacos_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

/*!
\brief PROC carctan = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arctan_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    VALUE (re) = atan (VALUE (re));
  } else {
    double r = VALUE (re), i = VALUE (im);
    double a = a68g_hypot (r, i + 1), b = a68g_hypot (r, i - 1);
    VALUE (re) = 0.5 * atan (2 * r / (1 - r * r - i * i));
    VALUE (im) = 0.5 * log (a / b);
  }
  MATH_RTE (p, errno != 0, MODE (REAL), NO_TEXT);
}

/*!
\brief PROC long catan = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_atan_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *im = (MP_T *) STACK_OFFSET (-size);
  RESET_ERRNO;
  (void) catan_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = (MP_T) INIT_MASK;
  MP_STATUS (im) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

#if defined HAVE_GNU_GSL

/*!
\brief PROC csinh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_sinh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_sinh);
}

/*!
\brief PROC ccosh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_cosh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_cosh);
}

/*!
\brief PROC ctanh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_tanh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_tanh);
}

/*!
\brief PROC carcsinh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arcsinh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arcsinh);
}

/*!
\brief PROC carccosh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arccosh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arccosh);
}

/*!
\brief PROC carctanh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arctanh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arctanh);
}

#endif /* defined HAVE_GNU_GSL */

/* Standard prelude implementation, transput */

/*
Transput library - General routines and (formatted) transput.
But Eeyore wasn't listening. He was taking the balloon out, and putting
it back again, as happy as could be ... Winnie the Pooh, A.A. Milne.
- Revised Report on the Algorithmic Language Algol 68.
*/

A68_CHANNEL stand_in_channel, stand_out_channel, stand_draw_channel, stand_back_channel, stand_error_channel, associate_channel, skip_channel;
A68_REF stand_in, stand_out, stand_back, stand_error, skip_file;
A68_FORMAT nil_format = {
  INIT_MASK, NULL, 0
};

/* File table handling 
In a table we record opened files.
When execution ends, unclosed files are closed, and temps are removed.
This keeps /tmp free of spurious files :-)
*/

typedef struct FILE_ENTRY FILE_ENTRY;

struct FILE_ENTRY {
  NODE_T *pos;
  BOOL_T is_open, is_tmp;
  FILE_T fd;
  A68_REF idf;
};

FILE_ENTRY file_entries[MAX_OPEN_FILES];

/*
\brief init an entry
**/

void init_file_entry (int k)
{
  if (k >= 0 && k < MAX_OPEN_FILES) {
    FILE_ENTRY *fe = &(file_entries[k]);
    POS (fe) = NO_NODE;
    IS_OPEN (fe) = A68_FALSE;
    IS_TMP (fe) = A68_FALSE;
    FD (fe) = A68_NO_FILENO;
    IDF (fe) = nil_ref;
  }
}

/*
\brief initialise file entry table
**/

void init_file_entries (void)
{
  int k;
  for (k = 0; k < MAX_OPEN_FILES; k ++) {
    init_file_entry (k);
  }
}

/*
\brief store file for later closing when not explicitly closed
\param p entry in syntax tree
\param fd file descriptor
\param idf file name
\param is_tmp whether file is a temp file
\return entry in table
**/

int store_file_entry (NODE_T *p, FILE_T fd, char *idf, BOOL_T is_tmp)
{
  int k;
  for (k = 0; k < MAX_OPEN_FILES; k ++)
  {
    FILE_ENTRY *fe = &(file_entries[k]);
    if (!IS_OPEN (fe)) {
      int len = 1 + (int) strlen (idf);
      POS (fe) = p;
      IS_OPEN (fe) = A68_TRUE;
      IS_TMP (fe) = is_tmp;
      FD (fe) = fd;
      IDF (fe) = heap_generator (p, MODE (C_STRING), len);
      BLOCK_GC_HANDLE (&(IDF (fe)));
      bufcpy (DEREF (char, &IDF (fe)), idf, len);
      return (k);
    }
  }
  diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_TOO_MANY_OPEN_FILES);
  exit_genie (p, A68_RUNTIME_ERROR);
  return (-1); /* Fool them */
}

/*
\brief close file and delete temp file
\param k entry in table
**/

static void close_file_entry (NODE_T *p, int k)
{
  if (k >= 0 && k < MAX_OPEN_FILES) {
    FILE_ENTRY *fe = &(file_entries[k]);
    if (IS_OPEN (fe)) {
/* Close the file */
      if (FD (fe) != A68_NO_FILENO && close (FD (fe)) == -1) {
        init_file_entry (k);
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_CLOSE);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      IS_OPEN (fe) = A68_FALSE;
    }
  }
}

/*
\brief close file and delete temp file
\param k entry in table
**/

static void free_file_entry (NODE_T *p, int k)
{
  close_file_entry (p, k);
  if (k >= 0 && k < MAX_OPEN_FILES) {
    FILE_ENTRY *fe = &(file_entries[k]);
    if (IS_OPEN (fe)) {
/* Attempt to remove a temp file, but ignore failure */
      if (FD (fe) != A68_NO_FILENO && IS_TMP (fe)) {
        if (!IS_NIL (IDF (fe))) {
          char *filename;
          CHECK_INIT (p, INITIALISED (&(IDF (fe))), MODE (ROWS));
          filename = DEREF (char, &IDF (fe));
          if (filename != NO_TEXT) {
            (void) remove (filename);
          }
        }
      }
/* Restore the fields */
      if (!IS_NIL (IDF (fe))) {
        UNBLOCK_GC_HANDLE (&(IDF (fe)));
      }
      init_file_entry (k);
    }
  }
}

/*
\brief close all files and delete all temp files
**/

void free_file_entries (void)
{
  int k;
  for (k = 0; k < MAX_OPEN_FILES; k ++) {
    free_file_entry (NO_NODE, k);
  }
}


/*!
\brief PROC char in string = (CHAR, REF INT, STRING) BOOL
\param p position in tree
**/

void genie_char_in_string (NODE_T * p)
{
  A68_CHAR c;
  A68_INT pos;
  A68_REF ref_pos, ref_str, row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  char *q, ch;
  int k, len;
  POP_REF (p, &ref_str);
  row = *(A68_REF *) & ref_str;
  CHECK_INIT (p, INITIALISED (&row), MODE (ROWS));
  GET_DESCRIPTOR (arr, tup, &row);
  POP_REF (p, &ref_pos);
  POP_OBJECT (p, &c, A68_CHAR);
  reset_transput_buffer (PATTERN_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_str);
  len = get_transput_buffer_index (PATTERN_BUFFER);
  q = get_transput_buffer (PATTERN_BUFFER);
  ch = (char) VALUE (&c);
  for (k = 0; k < len; k++) {
    if (q[k] == ch) {
      STATUS (&pos) = INIT_MASK;
      VALUE (&pos) = k + LOWER_BOUND (tup);
      * DEREF (A68_INT, &ref_pos) = pos;
      PUSH_PRIMITIVE (p, A68_TRUE, A68_BOOL);
      return;
    }
  }
  PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
}

/*!
\brief PROC last char in string = (CHAR, REF INT, STRING) BOOL
\param p position in tree
**/

void genie_last_char_in_string (NODE_T * p)
{
  A68_CHAR c;
  A68_INT pos;
  A68_REF ref_pos, ref_str, row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  char *q, ch;
  int k, len;
  POP_REF (p, &ref_str);
  row = *(A68_REF *) & ref_str;
  CHECK_INIT (p, INITIALISED (&row), MODE (ROWS));
  GET_DESCRIPTOR (arr, tup, &row);
  POP_REF (p, &ref_pos);
  POP_OBJECT (p, &c, A68_CHAR);
  reset_transput_buffer (PATTERN_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_str);
  len = get_transput_buffer_index (PATTERN_BUFFER);
  q = get_transput_buffer (PATTERN_BUFFER);
  ch = (char) VALUE (&c);
  for (k = len - 1; k >= 0; k--) {
    if (q[k] == ch) {
      STATUS (&pos) = INIT_MASK;
      VALUE (&pos) = k + LOWER_BOUND (tup);
      * DEREF (A68_INT, &ref_pos) = pos;
      PUSH_PRIMITIVE (p, A68_TRUE, A68_BOOL);
      return;
    }
  }
  PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
}

/*!
\brief PROC string in string = (STRING, REF INT, STRING) BOOL
\param p position in tree
**/

void genie_string_in_string (NODE_T * p)
{
  A68_REF ref_pos, ref_str, ref_pat, row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  char *q;
  POP_REF (p, &ref_str);
  row = *(A68_REF *) & ref_str;
  CHECK_INIT (p, INITIALISED (&row), MODE (ROWS));
  GET_DESCRIPTOR (arr, tup, &row);
  POP_REF (p, &ref_pos);
  POP_REF (p, &ref_pat);
  reset_transput_buffer (PATTERN_BUFFER);
  reset_transput_buffer (STRING_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_pat);
  add_a_string_transput_buffer (p, STRING_BUFFER, (BYTE_T *) & ref_str);
  q = strstr (get_transput_buffer (STRING_BUFFER), get_transput_buffer (PATTERN_BUFFER));
  if (q != NO_TEXT) {
    if (!IS_NIL (ref_pos)) {
      A68_INT pos;
      STATUS (&pos) = INIT_MASK;
/* ANSI standard leaves pointer difference undefined */
      VALUE (&pos) = LOWER_BOUND (tup) + (int) get_transput_buffer_index (STRING_BUFFER) - (int) strlen (q);
      * DEREF (A68_INT, &ref_pos) = pos;
    }
    PUSH_PRIMITIVE (p, A68_TRUE, A68_BOOL);
  } else {
    PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
  }
}

/*
Strings in transput are of arbitrary size. For this, we have transput buffers.
A transput buffer is a REF STRUCT (INT size, index, STRING buffer).
It is in the heap, but cannot be gced. If it is too small, we give up on
it and make a larger one.
*/

static A68_REF ref_transput_buffer[MAX_TRANSPUT_BUFFER];

/*!
\brief set max number of chars in a transput buffer
\param n transput buffer number
\param size max number of chars
**/

void set_transput_buffer_size (int n, int size)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]));
  STATUS (k) = INIT_MASK;
  VALUE (k) = size;
}

/*!
\brief set char index for transput buffer
\param n transput buffer number
\param cindex char index
**/

void set_transput_buffer_index (int n, int cindex)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]) + ALIGNED_SIZE_OF (A68_INT));
  STATUS (k) = INIT_MASK;
  VALUE (k) = cindex;
}

/*!
\brief get max number of chars in a transput buffer
\param n transput buffer number
\return same
**/

int get_transput_buffer_size (int n)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]));
  return (VALUE (k));
}

/*!
\brief get char index for transput buffer
\param n transput buffer number
\return same
**/

int get_transput_buffer_index (int n)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]) + ALIGNED_SIZE_OF (A68_INT));
  return (VALUE (k));
}

/*!
\brief get char[] from transput buffer
\param n transput buffer number
\return same
**/

char *get_transput_buffer (int n)
{
  return ((char *) (ADDRESS (&ref_transput_buffer[n]) + 2 * ALIGNED_SIZE_OF (A68_INT)));
}

/*!
\brief mark transput buffer as no longer in use
\param n transput buffer number
**/

void unblock_transput_buffer (int n)
{
  set_transput_buffer_index (n, -1);
}

/*!
\brief find first unused transput buffer (for opening a file)
\param p position in tree position in syntax tree
\return same
**/

int get_unblocked_transput_buffer (NODE_T * p)
{
  int k;
  for (k = FIXED_TRANSPUT_BUFFERS; k < MAX_TRANSPUT_BUFFER; k++) {
    if (get_transput_buffer_index (k) == -1) {
      return (k);
    }
  }
/* Oops! */
  diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_TOO_MANY_OPEN_FILES);
  exit_genie (p, A68_RUNTIME_ERROR);
  return (-1);
}

/*!
\brief empty contents of transput buffer
\param n transput buffer number
**/

void reset_transput_buffer (int n)
{
  set_transput_buffer_index (n, 0);
  (get_transput_buffer (n))[0] = NULL_CHAR;
}

/*!
\brief initialise transput buffers before use
\param p position in tree position in syntax tree
**/

void init_transput_buffers (NODE_T * p)
{
  int k;
  for (k = 0; k < MAX_TRANSPUT_BUFFER; k++) {
    ref_transput_buffer[k] = heap_generator (p, MODE (ROWS), 2 * ALIGNED_SIZE_OF (A68_INT) + TRANSPUT_BUFFER_SIZE);
    BLOCK_GC_HANDLE (&ref_transput_buffer[k]);
    set_transput_buffer_size (k, TRANSPUT_BUFFER_SIZE);
    reset_transput_buffer (k);
  }
/* Last buffers are available for FILE values */
  for (k = FIXED_TRANSPUT_BUFFERS; k < MAX_TRANSPUT_BUFFER; k++) {
    unblock_transput_buffer (k);
  }
}

/*!
\brief make a transput buffer larger
\param p position in tree
\param k transput buffer number
\param size new size in characters
**/

void enlarge_transput_buffer (NODE_T * p, int k, int size)
{
  int tbindex = get_transput_buffer_index (k);
  char *sb_1 = get_transput_buffer (k), *sb_2;
  UNBLOCK_GC_HANDLE (&ref_transput_buffer[k]);
  ref_transput_buffer[k] = heap_generator (p, MODE (ROWS), 2 * ALIGNED_SIZE_OF (A68_INT) + size);
  BLOCK_GC_HANDLE (&ref_transput_buffer[k]);
  set_transput_buffer_size (k, size);
  set_transput_buffer_index (k, tbindex);
  sb_2 = get_transput_buffer (k);
  bufcpy (sb_2, sb_1, size);
}

/*!
\brief add char to transput buffer; if the buffer is full, make it larger
\param p position in tree
\param k transput buffer number
\param ch char to add
**/

void add_char_transput_buffer (NODE_T * p, int k, char ch)
{
  char *sb = get_transput_buffer (k);
  int size = get_transput_buffer_size (k);
  int tbindex = get_transput_buffer_index (k);
  if (tbindex == size - 2) {
    enlarge_transput_buffer (p, k, 10 * size /* size + TRANSPUT_BUFFER_SIZE */ );
    add_char_transput_buffer (p, k, ch);
  } else {
    sb[tbindex] = ch;
    sb[tbindex + 1] = NULL_CHAR;
    set_transput_buffer_index (k, tbindex + 1);
  }
}

/*!
\brief add char[] to transput buffer
\param p position in tree
\param k transput buffer number
\param ch string to add
**/

void add_string_transput_buffer (NODE_T * p, int k, char *ch)
{
  for (; ch[0] != NULL_CHAR; ch++) {
    add_char_transput_buffer (p, k, ch[0]);
  }
}

/*!
\brief add A68 string to transput buffer
\param p position in tree
\param k transput buffer number
\param ref fat pointer to A68 string
**/

void add_a_string_transput_buffer (NODE_T * p, int k, BYTE_T * ref)
{
  A68_REF row = *(A68_REF *) ref;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  CHECK_INIT (p, INITIALISED (&row), MODE (ROWS));
  GET_DESCRIPTOR (arr, tup, &row);
  if (ROW_SIZE (tup) > 0) {
    int i;
    BYTE_T *base_address = DEREF (BYTE_T, &ARRAY (arr));
    for (i = LWB (tup); i <= UPB (tup); i++) {
      int addr = INDEX_1_DIM (arr, tup, i);
      A68_CHAR *ch = (A68_CHAR *) & (base_address[addr]);
      CHECK_INIT (p, INITIALISED (ch), MODE (CHAR));
      add_char_transput_buffer (p, k, (char) VALUE (ch));
    }
  }
}

/*!
\brief pop A68 string and add to buffer
\param p position in tree
\param k transput buffer number
**/

void add_string_from_stack_transput_buffer (NODE_T * p, int k)
{
  DECREMENT_STACK_POINTER (p, A68_REF_SIZE);
  add_a_string_transput_buffer (p, k, STACK_TOP);
}

/*!
\brief pop first character from transput buffer
\param k transput buffer number
\return same
**/

char pop_char_transput_buffer (int k)
{
  char *sb = get_transput_buffer (k);
  int tbindex = get_transput_buffer_index (k);
  if (tbindex <= 0) {
    return (NULL_CHAR);
  } else {
    char ch = sb[0];
    MOVE (&sb[0], &sb[1], tbindex);
    set_transput_buffer_index (k, tbindex - 1);
    return (ch);
  }
}

/*!
\brief add C string to A68 string
\param p position in tree
\param ref_str fat pointer to A68 string
\param str pointer to C string
**/

static void add_c_string_to_a_string (NODE_T * p, A68_REF ref_str, char *str)
{
  A68_REF a, c, d;
  A68_ARRAY *a_1, *a_3;
  A68_TUPLE *t_1, *t_3;
  int l_1, l_2, u, v;
  BYTE_T *b_1, *b_3;
  l_2 = (int) strlen (str);
/* left part */
  CHECK_REF (p, ref_str, MODE (REF_STRING));
  a = * DEREF (A68_REF, &ref_str);
  CHECK_INIT (p, INITIALISED (&a), MODE (STRING));
  GET_DESCRIPTOR (a_1, t_1, &a);
  l_1 = ROW_SIZE (t_1);
/* Sum string */
  c = heap_generator (p, MODE (STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  d = heap_generator (p, MODE (STRING), (l_1 + l_2) * ALIGNED_SIZE_OF (A68_CHAR));
/* Calculate again since garbage collector might have moved data */
  GET_DESCRIPTOR (a_1, t_1, &a);
/* Make descriptor of new string */
  GET_DESCRIPTOR (a_3, t_3, &c);
  DIM (a_3) = 1;
  MOID (a_3) = MODE (CHAR);
  ELEM_SIZE (a_3) = ALIGNED_SIZE_OF (A68_CHAR);
  SLICE_OFFSET (a_3) = 0;
  FIELD_OFFSET (a_3) = 0;
  ARRAY (a_3) = d;
  LWB (t_3) = 1;
  UPB (t_3) = l_1 + l_2;
  SHIFT (t_3) = LWB (t_3);
  SPAN (t_3) = 1;
/* add strings */
  b_1 = (ROW_SIZE (t_1) > 0 ? DEREF (BYTE_T, &ARRAY (a_1)) : NO_BYTE);
  b_3 = DEREF (BYTE_T, &ARRAY (a_3));
  u = 0;
  for (v = LWB (t_1); v <= UPB (t_1); v++) {
    MOVE ((BYTE_T *) & b_3[u], (BYTE_T *) & b_1[INDEX_1_DIM (a_1, t_1, v)], ALIGNED_SIZE_OF (A68_CHAR));
    u += ALIGNED_SIZE_OF (A68_CHAR);
  }
  for (v = 0; v < l_2; v++) {
    A68_CHAR ch;
    STATUS (&ch) = INIT_MASK;
    VALUE (&ch) = str[v];
    MOVE ((BYTE_T *) & b_3[u], (BYTE_T *) & ch, ALIGNED_SIZE_OF (A68_CHAR));
    u += ALIGNED_SIZE_OF (A68_CHAR);
  }
  * DEREF (A68_REF, &ref_str) = c;
}

/*!
\brief purge buffer for file
\param p position in tree
\param ref_file
\param k transput buffer number for file
**/

void write_purge_buffer (NODE_T * p, A68_REF ref_file, int k)
{
  A68_FILE *file = FILE_DEREF (&ref_file);
  if (IS_NIL (STRING (file))) {
    if (!(FD (file) == STDOUT_FILENO && halt_typing)) {
      WRITE (FD (file), get_transput_buffer (k));
    }
  } else {
    add_c_string_to_a_string (p, STRING (file), get_transput_buffer (k));
  }
  reset_transput_buffer (k);
}

/* Routines that involve the A68 expression stack */

/*!
\brief allocate a temporary string on the stack
\param p position in tree
\param size size in characters
\return same
**/

char *stack_string (NODE_T * p, int size)
{
  char *new_str = (char *) STACK_TOP;
  INCREMENT_STACK_POINTER (p, size);
  if (stack_pointer > expr_stack_limit) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  FILL (new_str, NULL_CHAR, size);
  return (new_str);
}

/* Transput basic RTS routines */

/*!
\brief REF FILE standin
\param p position in tree
**/

void genie_stand_in (NODE_T * p)
{
  PUSH_REF (p, stand_in);
}

/*!
\brief REF FILE standout
\param p position in tree
**/

void genie_stand_out (NODE_T * p)
{
  PUSH_REF (p, stand_out);
}

/*!
\brief REF FILE standback
\param p position in tree
**/

void genie_stand_back (NODE_T * p)
{
  PUSH_REF (p, stand_back);
}

/*!
\brief REF FILE standerror
\param p position in tree
**/

void genie_stand_error (NODE_T * p)
{
  PUSH_REF (p, stand_error);
}

/*!
\brief CHAR error char
\param p position in tree
**/

void genie_error_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, ERROR_CHAR, A68_CHAR);
}

/*!
\brief CHAR exp char
\param p position in tree
**/

void genie_exp_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, EXPONENT_CHAR, A68_CHAR);
}

/*!
\brief CHAR flip char
\param p position in tree
**/

void genie_flip_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, FLIP_CHAR, A68_CHAR);
}

/*!
\brief CHAR flop char
\param p position in tree
**/

void genie_flop_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, FLOP_CHAR, A68_CHAR);
}

/*!
\brief CHAR null char
\param p position in tree
**/

void genie_null_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, NULL_CHAR, A68_CHAR);
}

/*!
\brief CHAR blank
\param p position in tree
**/

void genie_blank_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, BLANK_CHAR, A68_CHAR);
}

/*!
\brief CHAR newline char
\param p position in tree
**/

void genie_newline_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, NEWLINE_CHAR, A68_CHAR);
}

/*!
\brief CHAR formfeed char
\param p position in tree
**/

void genie_formfeed_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, FORMFEED_CHAR, A68_CHAR);
}

/*!
\brief CHAR tab char
\param p position in tree
**/

void genie_tab_char (NODE_T * p)
{
  PUSH_PRIMITIVE (p, TAB_CHAR, A68_CHAR);
}

/*!
\brief CHANNEL standin channel
\param p position in tree
**/

void genie_stand_in_channel (NODE_T * p)
{
  PUSH_OBJECT (p, stand_in_channel, A68_CHANNEL);
}

/*!
\brief CHANNEL standout channel
\param p position in tree
**/

void genie_stand_out_channel (NODE_T * p)
{
  PUSH_OBJECT (p, stand_out_channel, A68_CHANNEL);
}

/*!
\brief CHANNEL stand draw channel
\param p position in tree
**/

void genie_stand_draw_channel (NODE_T * p)
{
  PUSH_OBJECT (p, stand_draw_channel, A68_CHANNEL);
}

/*!
\brief CHANNEL standback channel
\param p position in tree
**/

void genie_stand_back_channel (NODE_T * p)
{
  PUSH_OBJECT (p, stand_back_channel, A68_CHANNEL);
}

/*!
\brief CHANNEL standerror channel
\param p position in tree
**/

void genie_stand_error_channel (NODE_T * p)
{
  PUSH_OBJECT (p, stand_error_channel, A68_CHANNEL);
}

/*!
\brief PROC STRING program idf
\param p position in tree
**/

void genie_program_idf (NODE_T * p)
{
  PUSH_REF (p, c_to_a_string (p, FILE_SOURCE_NAME (&program), DEFAULT_WIDTH));
}

/* FILE and CHANNEL initialisations */

/*!
\brief set_default_event_procedure
\param z
**/

void set_default_event_procedure (A68_PROCEDURE * z)
{
  STATUS (z) = INIT_MASK;
  NODE (&(BODY (z))) = NO_NODE;
  ENVIRON (z) = 0;
}

/*!
\brief initialise channel
\param chan channel
\param r reset possible
\param s set possible
\param g get possible
\param p put possible
\param b bin possible
\param d draw possible
**/

static void init_channel (A68_CHANNEL * chan, BOOL_T r, BOOL_T s, BOOL_T g, BOOL_T p, BOOL_T b, BOOL_T d)
{
  STATUS (chan) = INIT_MASK;
  RESET (chan) = r;
  SET (chan) = s;
  GET (chan) = g;
  PUT (chan) = p;
  BIN (chan) = b;
  DRAW (chan) = d;
  COMPRESS (chan) = A68_TRUE;
}

/*!
\brief set default event handlers
\param f file
**/

void set_default_event_procedures (A68_FILE * f)
{
  set_default_event_procedure (&(FILE_END_MENDED (f)));
  set_default_event_procedure (&(PAGE_END_MENDED (f)));
  set_default_event_procedure (&(LINE_END_MENDED (f)));
  set_default_event_procedure (&(VALUE_ERROR_MENDED (f)));
  set_default_event_procedure (&(OPEN_ERROR_MENDED (f)));
  set_default_event_procedure (&(TRANSPUT_ERROR_MENDED (f)));
  set_default_event_procedure (&(FORMAT_END_MENDED (f)));
  set_default_event_procedure (&(FORMAT_ERROR_MENDED (f)));
}

/*!
\brief set up a REF FILE object
\param p position in tree
\param ref_file fat pointer to A68 file
\param c channel
\param s file number
\param rm read mood
\param wm write mood
\param cm char mood
**/

static void init_file (NODE_T * p, A68_REF * ref_file, A68_CHANNEL c, FILE_T s, BOOL_T rm, BOOL_T wm, BOOL_T cm, char *env)
{
  A68_FILE *f;
  char *filename = (env == NO_TEXT ? NO_TEXT : getenv (env));
  *ref_file = heap_generator (p, MODE (REF_FILE), ALIGNED_SIZE_OF (A68_FILE));
  BLOCK_GC_HANDLE (ref_file);
  f = FILE_DEREF (ref_file);
  STATUS (f) = INIT_MASK;
  TERMINATOR (f) = nil_ref;
  CHANNEL (f) = c;
  if (filename != NO_TEXT && strlen (filename) > 0) {
    int len = 1 + (int) strlen (filename);
    IDENTIFICATION (f) = heap_generator (p, MODE (C_STRING), len);
    BLOCK_GC_HANDLE (&(IDENTIFICATION (f)));
    bufcpy (DEREF (char, &IDENTIFICATION (f)), filename, len);
    FD (f) = A68_NO_FILENO;
    READ_MOOD (f) = A68_FALSE;
    WRITE_MOOD (f) = A68_FALSE;
    CHAR_MOOD (f) = A68_FALSE;
    DRAW_MOOD (f) = A68_FALSE;
  } else {
    IDENTIFICATION (f) = nil_ref;
    FD (f) = s;
    READ_MOOD (f) = rm;
    WRITE_MOOD (f) = wm;
    CHAR_MOOD (f) = cm;
    DRAW_MOOD (f) = A68_FALSE;
  }
  TRANSPUT_BUFFER (f) = get_unblocked_transput_buffer (p);
  reset_transput_buffer (TRANSPUT_BUFFER (f));
  END_OF_FILE (f) = A68_FALSE;
  TMP_FILE (f) = A68_FALSE;
  OPENED (f) = A68_TRUE;
  OPEN_EXCLUSIVE (f) = A68_FALSE;
  FORMAT (f) = nil_format;
  STRING (f) = nil_ref;
  STRPOS (f) = 0;
  FILE_ENTRY (f) = -1;
  set_default_event_procedures (f);
}

/*!
\brief initialise the transput RTL
\param p position in tree
**/

void genie_init_transput (NODE_T * p)
{
  init_transput_buffers (p);
/* Channels */
  init_channel (&stand_in_channel, A68_FALSE, A68_FALSE, A68_TRUE, A68_FALSE, A68_FALSE, A68_FALSE);
  init_channel (&stand_out_channel, A68_FALSE, A68_FALSE, A68_FALSE, A68_TRUE, A68_FALSE, A68_FALSE);
  init_channel (&stand_back_channel, A68_TRUE, A68_TRUE, A68_TRUE, A68_TRUE, A68_TRUE, A68_FALSE);
  init_channel (&stand_error_channel, A68_FALSE, A68_FALSE, A68_FALSE, A68_TRUE, A68_FALSE, A68_FALSE);
  init_channel (&associate_channel, A68_TRUE, A68_TRUE, A68_TRUE, A68_TRUE, A68_FALSE, A68_FALSE);
  init_channel (&skip_channel, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE);
#if defined HAVE_GNU_PLOTUTILS
  init_channel (&stand_draw_channel, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_TRUE);
#else /*  */
  init_channel (&stand_draw_channel, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_TRUE);
#endif /*  */
/* Files */
  init_file (p, &stand_in, stand_in_channel, STDIN_FILENO, A68_TRUE, A68_FALSE, A68_TRUE, "A68G_STANDIN");
  init_file (p, &stand_out, stand_out_channel, STDOUT_FILENO, A68_FALSE, A68_TRUE, A68_TRUE, "A68G_STANDOUT");
  init_file (p, &stand_back, stand_back_channel, A68_NO_FILENO, A68_FALSE, A68_FALSE, A68_FALSE, NO_TEXT);
  init_file (p, &stand_error, stand_error_channel, STDERR_FILENO, A68_FALSE, A68_TRUE, A68_TRUE, "A68G_STANDERROR");
  init_file (p, &skip_file, skip_channel, A68_NO_FILENO, A68_FALSE, A68_FALSE, A68_FALSE, NO_TEXT);
}

/*!
\brief PROC (REF FILE) STRING idf
\param p position in tree
**/

void genie_idf (NODE_T * p)
{
  A68_REF ref_file, ref_filename;
  char *filename;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  ref_file = *(A68_REF *) STACK_TOP;
  ref_filename = IDENTIFICATION (FILE_DEREF (&ref_file));
  CHECK_REF (p, ref_filename, MODE (ROWS));
  filename = DEREF (char, &ref_filename);
  PUSH_REF (p, c_to_a_string (p, filename, DEFAULT_WIDTH));
}

/*!
\brief PROC (REF FILE) STRING term
\param p position in tree
**/

void genie_term (NODE_T * p)
{
  A68_REF ref_file, ref_term;
  char *term;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  ref_file = *(A68_REF *) STACK_TOP;
  ref_term = TERMINATOR (FILE_DEREF (&ref_file));
  CHECK_REF (p, ref_term, MODE (ROWS));
  term = DEREF (char, &ref_term);
  PUSH_REF (p, c_to_a_string (p, term, DEFAULT_WIDTH));
}

/*!
\brief PROC (REF FILE, STRING) VOID make term
\param p position in tree
**/

void genie_make_term (NODE_T * p)
{
  int size;
  A68_FILE *file;
  A68_REF ref_file, str;
  POP_REF (p, &str);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  ref_file = *(A68_REF *) STACK_TOP;
  file = FILE_DEREF (&ref_file);
/* Don't check initialisation so we can "make term" before opening.
   That is ok */
  size = a68_string_size (p, str);
  if (INITIALISED (&(TERMINATOR (file))) && !IS_NIL (TERMINATOR (file))) {
    UNBLOCK_GC_HANDLE (&(TERMINATOR (file)));
  }
  TERMINATOR (file) = heap_generator (p, MODE (C_STRING), 1 + size);
  BLOCK_GC_HANDLE (&(TERMINATOR (file)));
  ASSERT (a_to_c_string (p, DEREF (char, &TERMINATOR (file)), str) != NO_TEXT);
}

/*!
\brief PROC (REF FILE) BOOL put possible
\param p position in tree
**/

void genie_put_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PUSH_PRIMITIVE (p, PUT (&CHANNEL (file)), A68_BOOL);
}

/*!
\brief PROC (REF FILE) BOOL get possible
\param p position in tree
**/

void genie_get_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PUSH_PRIMITIVE (p, GET (&CHANNEL (file)), A68_BOOL);
}

/*!
\brief PROC (REF FILE) BOOL bin possible
\param p position in tree
**/

void genie_bin_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PUSH_PRIMITIVE (p, BIN (&CHANNEL (file)), A68_BOOL);
}

/*!
\brief PROC (REF FILE) BOOL set possible
\param p position in tree
**/

void genie_set_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PUSH_PRIMITIVE (p, SET (&CHANNEL (file)), A68_BOOL);
}

/*!
\brief PROC (REF FILE) BOOL reidf possible
\param p position in tree
**/

void genie_reidf_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
}

/*!
\brief PROC (REF FILE) BOOL reset possible
\param p position in tree
**/

void genie_reset_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PUSH_PRIMITIVE (p, RESET (&CHANNEL (file)), A68_BOOL);
}

/*!
\brief PROC (REF FILE) BOOL compressible
\param p position in tree
**/

void genie_compressible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PUSH_PRIMITIVE (p, COMPRESS (&CHANNEL (file)), A68_BOOL);
}

/*!
\brief PROC (REF FILE) BOOL draw possible
\param p position in tree
**/

void genie_draw_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PUSH_PRIMITIVE (p, DRAW (&CHANNEL (file)), A68_BOOL);
}

/*!
\brief PROC (REF FILE, STRING, CHANNEL) INT open
\param p position in tree
**/

void genie_open (NODE_T * p)
{
  A68_CHANNEL channel;
  A68_REF ref_iden, ref_file;
  A68_FILE *file;
  int size;
  POP_OBJECT (p, &channel, A68_CHANNEL);
  POP_REF (p, &ref_iden);
  CHECK_REF (p, ref_iden, MODE (REF_STRING));
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  STATUS (file) = INIT_MASK;
  FILE_ENTRY (file) = -1;
  CHANNEL (file) = channel;
  OPENED (file) = A68_TRUE;
  OPEN_EXCLUSIVE (file) = A68_FALSE;
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  TMP_FILE (file) = A68_FALSE;
  size = a68_string_size (p, ref_iden);
  if (INITIALISED (&(IDENTIFICATION (file))) && !IS_NIL (IDENTIFICATION (file))) {
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  }
  IDENTIFICATION (file) = heap_generator (p, MODE (C_STRING), 1 + size);
  BLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  ASSERT (a_to_c_string (p, DEREF (char, &IDENTIFICATION (file)), ref_iden) != NO_TEXT);
  TERMINATOR (file) = nil_ref;
  FORMAT (file) = nil_format;
  FD (file) = A68_NO_FILENO;
  if (INITIALISED (&(STRING (file))) && !IS_NIL (STRING (file))) {
    UNBLOCK_GC_HANDLE (DEREF (A68_REF, &STRING (file)));
  }
  STRING (file) = nil_ref;
  STRPOS (file) = 0;
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
  STREAM (&DEVICE (file)) = NO_STREAM;
  set_default_event_procedures (file);
  {
    struct stat status;
    if (stat (DEREF (char, &IDENTIFICATION (file)), &status) == 0) {
      PUSH_PRIMITIVE (p, (S_ISREG (ST_MODE (&status)) != 0 ? 0 : 1), A68_INT);
    } else {
      PUSH_PRIMITIVE (p, 1, A68_INT);
    }
    RESET_ERRNO;
  }
}

/*!
\brief PROC (REF FILE, STRING, CHANNEL) INT establish
\param p position in tree
**/

void genie_establish (NODE_T * p)
{
  A68_CHANNEL channel;
  A68_REF ref_iden, ref_file;
  A68_FILE *file;
  int size;
  POP_OBJECT (p, &channel, A68_CHANNEL);
  POP_REF (p, &ref_iden);
  CHECK_REF (p, ref_iden, MODE (REF_STRING));
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  STATUS (file) = INIT_MASK;
  FILE_ENTRY (file) = -1;
  CHANNEL (file) = channel;
  OPENED (file) = A68_TRUE;
  OPEN_EXCLUSIVE (file) = A68_TRUE;
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  TMP_FILE (file) = A68_FALSE;
  if (!PUT (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  size = a68_string_size (p, ref_iden);
  if (INITIALISED (&(IDENTIFICATION (file))) && !IS_NIL (IDENTIFICATION (file))) {
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  }
  IDENTIFICATION (file) = heap_generator (p, MODE (C_STRING), 1 + size);
  BLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  ASSERT (a_to_c_string (p, DEREF (char, &IDENTIFICATION (file)), ref_iden) != NO_TEXT);
  TERMINATOR (file) = nil_ref;
  FORMAT (file) = nil_format;
  FD (file) = A68_NO_FILENO;
  if (INITIALISED (&(STRING (file))) && !IS_NIL (STRING (file))) {
    UNBLOCK_GC_HANDLE (DEREF (A68_REF, &STRING (file)));
  }
  STRING (file) = nil_ref;
  STRPOS (file) = 0;
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
  STREAM (&DEVICE (file)) = NO_STREAM;
  set_default_event_procedures (file);
  PUSH_PRIMITIVE (p, 0, A68_INT);
}

/*!
\brief PROC (REF FILE, CHANNEL) INT create
\param p position in tree
**/

void genie_create (NODE_T * p)
{
  A68_CHANNEL channel;
  A68_REF ref_file;
  A68_FILE *file;
  POP_OBJECT (p, &channel, A68_CHANNEL);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  STATUS (file) = INIT_MASK;
  FILE_ENTRY (file) = -1;
  CHANNEL (file) = channel;
  OPENED (file) = A68_TRUE;
  OPEN_EXCLUSIVE (file) = A68_FALSE;
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  TMP_FILE (file) = A68_TRUE;
  if (INITIALISED (&(IDENTIFICATION (file))) && !IS_NIL (IDENTIFICATION (file))) {
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  }
  IDENTIFICATION (file) = nil_ref;
  TERMINATOR (file) = nil_ref;
  FORMAT (file) = nil_format;
  FD (file) = A68_NO_FILENO;
  if (INITIALISED (&(STRING (file))) && !IS_NIL (STRING (file))) {
    UNBLOCK_GC_HANDLE (DEREF (A68_REF, &STRING (file)));
  }
  STRING (file) = nil_ref;
  STRPOS (file) = 0;
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
  STREAM (&DEVICE (file)) = NO_STREAM;
  set_default_event_procedures (file);
  PUSH_PRIMITIVE (p, 0, A68_INT);
}

/*!
\brief PROC (REF FILE, REF STRING) VOID associate
\param p position in tree
**/

void genie_associate (NODE_T * p)
{
  A68_REF ref_string, ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_string);
  CHECK_REF (p, ref_string, MODE (REF_STRING));
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  if (IS_IN_HEAP (&ref_file) && !IS_IN_HEAP (&ref_string)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, MODE (REF_STRING));
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (IS_IN_FRAME (&ref_file) && IS_IN_FRAME (&ref_string)) {
    if (REF_SCOPE (&ref_string) > REF_SCOPE (&ref_file)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, MODE (REF_STRING));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  file = FILE_DEREF (&ref_file);
  STATUS (file) = INIT_MASK;
  FILE_ENTRY (file) = -1;
  CHANNEL (file) = associate_channel;
  OPENED (file) = A68_TRUE;
  OPEN_EXCLUSIVE (file) = A68_FALSE;
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  TMP_FILE (file) = A68_FALSE;
  if (INITIALISED (&(IDENTIFICATION (file))) && !IS_NIL (IDENTIFICATION (file))) {
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  }
  IDENTIFICATION (file) = nil_ref;
  TERMINATOR (file) = nil_ref;
  FORMAT (file) = nil_format;
  FD (file) = A68_NO_FILENO;
  if (INITIALISED (&(STRING (file))) && !IS_NIL (STRING (file))) {
    UNBLOCK_GC_HANDLE (DEREF (A68_REF, &STRING (file)));
  }
  STRING (file) = ref_string;
  BLOCK_GC_HANDLE ((A68_REF *) (&(STRING (file))));
  STRPOS (file) = 1;
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
  STREAM (&DEVICE (file)) = NO_STREAM;
  set_default_event_procedures (file);
}

/*!
\brief PROC (REF FILE) VOID close
\param p position in tree
**/

void genie_close (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file) || (!READ_MOOD (file) && !WRITE_MOOD (file) && !DRAW_MOOD (file))) {
    return;
  }
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
#if defined HAVE_GNU_PLOTUTILS
  if (DEVICE_OPENED (&DEVICE (file))) {
    ASSERT(close_device (p, file) == A68_TRUE);
    STREAM (&DEVICE (file)) = NO_STREAM;
    return;
  }
#endif /*  */
  FD (file) = A68_NO_FILENO;
  OPENED (file) = A68_FALSE;
  unblock_transput_buffer (TRANSPUT_BUFFER (file));
  set_default_event_procedures (file);
  free_file_entry (p, FILE_ENTRY (file));
}

/*!
\brief PROC (REF FILE) VOID lock
\param p position in tree
**/

void genie_lock (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file) || (!READ_MOOD (file) && !WRITE_MOOD (file) && !DRAW_MOOD (file))) {
    return;
  }
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
#if defined HAVE_GNU_PLOTUTILS
  if (DEVICE_OPENED (&DEVICE (file))) {
    ASSERT(close_device (p, file) == A68_TRUE);
    STREAM (&DEVICE (file)) = NO_STREAM;
    return;
  }
#endif /*  */
#if ! defined HAVE_WIN32
  RESET_ERRNO;
  ASSERT (fchmod (FD (file), (mode_t) 0x0) != -1);
#endif
  if (FD (file) != A68_NO_FILENO && close (FD (file)) == -1) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_LOCK);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    FD (file) = A68_NO_FILENO;
    OPENED (file) = A68_FALSE;
    unblock_transput_buffer (TRANSPUT_BUFFER (file));
    set_default_event_procedures (file);
  }
  free_file_entry (p, FILE_ENTRY (file));
}

/*!
\brief PROC (REF FILE) VOID erase
\param p position in tree
**/

void genie_erase (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file) || (!READ_MOOD (file) && !WRITE_MOOD (file) && !DRAW_MOOD (file))) {
    return;
  }
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
#if defined HAVE_GNU_PLOTUTILS
  if (DEVICE_OPENED (&DEVICE (file))) {
    ASSERT(close_device (p, file) == A68_TRUE);
    STREAM (&DEVICE (file)) = NO_STREAM;
    return;
  }
#endif /*  */
  if (FD (file) != A68_NO_FILENO && close (FD (file)) == -1) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_SCRATCH);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    unblock_transput_buffer (TRANSPUT_BUFFER (file));
    set_default_event_procedures (file);
  }
/* Remove the file */
  if (!IS_NIL (IDENTIFICATION (file))) {
    char *filename;
    CHECK_INIT (p, INITIALISED (&(IDENTIFICATION (file))), MODE (ROWS));
    filename = DEREF (char, &IDENTIFICATION (file));
    if (remove (filename) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_SCRATCH);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
    IDENTIFICATION (file) = nil_ref;
  }
  init_file_entry (FILE_ENTRY (file));
}

/*!
\brief PROC (REF FILE) VOID backspace
\param p position in tree
**/

void genie_backspace (NODE_T * p)
{
  ADDR_T pop_sp = stack_pointer;
  PUSH_PRIMITIVE (p, -1, A68_INT);
  genie_set (p);
  stack_pointer = pop_sp;
}

/*!
\brief PROC (REF FILE, INT) INT set
\param p position in tree
**/

void genie_set (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_INT pos;
  POP_OBJECT (p, &pos, A68_INT);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!SET (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_CANT_SET);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!IS_NIL (STRING (file))) {
    A68_REF z = * DEREF (A68_REF, &STRING (file));
    A68_ARRAY *a;
    A68_TUPLE *t;
/* Circumvent buffering problems */
    STRPOS (file) -= get_transput_buffer_index (TRANSPUT_BUFFER (file));
    ASSERT (STRPOS (file) > 0);
    reset_transput_buffer (TRANSPUT_BUFFER (file));
/* Now set */
    CHECK_INT_ADDITION (p, STRPOS (file), VALUE (&pos));
    STRPOS (file) += VALUE (&pos);
    GET_DESCRIPTOR (a, t, &z);
    if (STRPOS (file) < LWB (t) || STRPOS (file) > UPB (t)) {
      A68_BOOL res;
      on_event_handler (p, FILE_END_MENDED (FILE_DEREF (&ref_file)), ref_file);
      POP_OBJECT (p, &res, A68_BOOL);
      if (VALUE (&res) == A68_FALSE) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ENDED);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
    }
    PUSH_PRIMITIVE (p, STRPOS (file), A68_INT);
  } else if (FD (file) == A68_NO_FILENO) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_RESET);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    __off_t curpos = lseek (FD (file), 0, SEEK_CUR);
    __off_t maxpos = lseek (FD (file), 0, SEEK_END);
    __off_t res = lseek (FD (file), curpos, SEEK_SET);
/* Circumvent buffering problems */
    int reserve = get_transput_buffer_index (TRANSPUT_BUFFER (file));
    curpos -= (__off_t) reserve;
    res = lseek (FD (file), -reserve, SEEK_CUR);
    ASSERT (res != -1 && errno == 0);
    reset_transput_buffer (TRANSPUT_BUFFER (file));
/* Now set */
    CHECK_INT_ADDITION (p, curpos, VALUE (&pos));
    curpos += VALUE (&pos);
    if (curpos < 0 || curpos >= maxpos) {
      A68_BOOL ret;
      on_event_handler (p, FILE_END_MENDED (FILE_DEREF (&ref_file)), ref_file);
      POP_OBJECT (p, &ret, A68_BOOL);
      if (VALUE (&ret) == A68_FALSE) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ENDED);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      PUSH_PRIMITIVE (p, (int) lseek (FD (file), 0, SEEK_CUR), A68_INT);
    } else {
      res = lseek (FD (file), curpos, SEEK_SET);
      if (res == -1 || errno != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_SET);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      PUSH_PRIMITIVE (p, (int) res, A68_INT);
    }
  }
}

/*!
\brief PROC (REF FILE) VOID reset
\param p position in tree
**/

void genie_reset (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!RESET (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_CANT_RESET);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (IS_NIL (STRING (file))) {
    close_file_entry (p, FILE_ENTRY (file));
  } else {
    STRPOS (file) = 1;
  }
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  FD (file) = A68_NO_FILENO;
/*  set_default_event_procedures (file); */
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on file end
\param p position in tree
**/

void genie_on_file_end (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP_PROCEDURE (p, &z);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  FILE_END_MENDED (file) = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on page end
\param p position in tree
**/

void genie_on_page_end (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP_PROCEDURE (p, &z);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  PAGE_END_MENDED (file) = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on line end
\param p position in tree
**/

void genie_on_line_end (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP_PROCEDURE (p, &z);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  LINE_END_MENDED (file) = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on format end
\param p position in tree
**/

void genie_on_format_end (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP_PROCEDURE (p, &z);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  FORMAT_END_MENDED (file) = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on format error
\param p position in tree
**/

void genie_on_format_error (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP_PROCEDURE (p, &z);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  FORMAT_ERROR_MENDED (file) = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on value error
\param p position in tree
**/

void genie_on_value_error (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP_PROCEDURE (p, &z);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  VALUE_ERROR_MENDED (file) = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on open error
\param p position in tree
**/

void genie_on_open_error (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP_PROCEDURE (p, &z);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  OPEN_ERROR_MENDED (file) = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on transput error
\param p position in tree
**/

void genie_on_transput_error (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP_PROCEDURE (p, &z);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  TRANSPUT_ERROR_MENDED (file) = z;
}

/*!
\brief invoke event routine
\param p position in tree
\param z routine to invoke
\param ref_file fat pointer to A68 file
**/

void on_event_handler (NODE_T * p, A68_PROCEDURE z, A68_REF ref_file)
{
  if (NODE (&(BODY (&z))) == NO_NODE) {
/* Default procedure */
    PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
  } else {
    ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
    PUSH_REF (p, ref_file);
    genie_call_event_routine (p, MODE (PROC_REF_FILE_BOOL), &z, pop_sp, pop_fp);
  }
}

/*!
\brief handle end-of-file event
\param p position in tree
\param ref_file fat pointer to A68 file
**/

void end_of_file_error (NODE_T * p, A68_REF ref_file)
{
  A68_BOOL z;
  on_event_handler (p, FILE_END_MENDED (FILE_DEREF (&ref_file)), ref_file);
  POP_OBJECT (p, &z, A68_BOOL);
  if (VALUE (&z) == A68_FALSE) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ENDED);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief handle file-open-error event
\param p position in tree
\param ref_file fat pointer to A68 file
\param mode mode for opening
**/

void open_error (NODE_T * p, A68_REF ref_file, char *mode)
{
  A68_BOOL z;
  on_event_handler (p, OPEN_ERROR_MENDED (FILE_DEREF (&ref_file)), ref_file);
  POP_OBJECT (p, &z, A68_BOOL);
  if (VALUE (&z) == A68_FALSE) {
    A68_FILE *file;
    char *filename;
    CHECK_REF (p, ref_file, MODE (REF_FILE));
    file = FILE_DEREF (&ref_file);
    CHECK_INIT (p, INITIALISED (file), MODE (FILE));
    if (!IS_NIL (IDENTIFICATION (file))) {
      filename = DEREF (char, &IDENTIFICATION (FILE_DEREF (&ref_file)));
    } else {
      filename = "(missing filename)";
    }
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_CANNOT_OPEN_FOR, filename, mode);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief handle value error event
\param p position in tree
\param m mode of object read or written
\param ref_file fat pointer to A68 file
**/

void value_error (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  if (END_OF_FILE (f)) {
    end_of_file_error (p, ref_file);
  } else {
    A68_BOOL z;
    on_event_handler (p, VALUE_ERROR_MENDED (f), ref_file);
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_TRANSPUT, m);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
}

/*!
\brief handle value_error event
\param p position in tree
\param m mode of object read or written
\param ref_file fat pointer to A68 file
**/

void value_sign_error (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  if (END_OF_FILE (f)) {
    end_of_file_error (p, ref_file);
  } else {
    A68_BOOL z;
    on_event_handler (p, VALUE_ERROR_MENDED (f), ref_file);
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_TRANSPUT_SIGN, m);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
}

/*!
\brief handle transput-error event
\param p position in tree
\param ref_file fat pointer to A68 file
\param m mode of object read or written
**/

void transput_error (NODE_T * p, A68_REF ref_file, MOID_T * m)
{
  A68_BOOL z;
  on_event_handler (p, TRANSPUT_ERROR_MENDED (FILE_DEREF (&ref_file)), ref_file);
  POP_OBJECT (p, &z, A68_BOOL);
  if (VALUE (&z) == A68_FALSE) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_TRANSPUT, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/* Implementation of put and get */

/*!
\brief get next char from file
\param f file
\return same
**/

int char_scanner (A68_FILE * f)
{
  if (get_transput_buffer_index (TRANSPUT_BUFFER (f)) > 0) {
/* There are buffered characters */
    END_OF_FILE (f) = A68_FALSE;
    return (pop_char_transput_buffer (TRANSPUT_BUFFER (f)));
  } else if (IS_NIL (STRING (f))) {
/* Fetch next CHAR from the FILE */
    ssize_t chars_read;
    char ch;
    chars_read = io_read_conv (FD (f), &ch, 1);
    if (chars_read == 1) {
      END_OF_FILE (f) = A68_FALSE;
      return (ch);
    } else {
      END_OF_FILE (f) = A68_TRUE;
      return (EOF_CHAR);
    }
  } else {
/* 
File is associated with a STRING. Give next CHAR. 
When we're outside the STRING give EOF_CHAR. 
*/
    A68_REF z = * DEREF (A68_REF, &STRING (f));
    A68_ARRAY *a;
    A68_TUPLE *t;
    BYTE_T *base;
    A68_CHAR *ch;
    GET_DESCRIPTOR (a, t, &z);
    if (ROW_SIZE (t) <= 0 || STRPOS (f) < LWB (t) || STRPOS (f) > UPB (t)) {
      END_OF_FILE (f) = A68_TRUE;
      return (EOF_CHAR);
    } else {
      base = DEREF (BYTE_T, &ARRAY (a));
      ch = (A68_CHAR *) & (base[INDEX_1_DIM (a, t, STRPOS (f))]);
      STRPOS (f)++;
      return (VALUE (ch));
    }
  }
}

/*!
\brief push back look-ahead character to file
\param p position in tree
\param f file
\param ch character to push
**/

void unchar_scanner (NODE_T * p, A68_FILE * f, char ch)
{
  END_OF_FILE (f) = A68_FALSE;
  add_char_transput_buffer (p, TRANSPUT_BUFFER (f), ch);
}

/*!
\brief PROC (REF FILE) BOOL eof
\param p position in tree
**/

void genie_eof (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (READ_MOOD (file)) {
    int ch = char_scanner (file);
    PUSH_PRIMITIVE (p, (BOOL_T) ((ch == EOF_CHAR || END_OF_FILE (file)) ? A68_TRUE : A68_FALSE), A68_BOOL);
    unchar_scanner (p, file, (char) ch);
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief PROC (REF FILE) BOOL eoln
\param p position in tree
**/

void genie_eoln (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (READ_MOOD (file)) {
    int ch = char_scanner (file);
    if (END_OF_FILE (file)) {
      end_of_file_error (p, ref_file);
    }
    PUSH_PRIMITIVE (p, (BOOL_T) (ch == NEWLINE_CHAR ? A68_TRUE : A68_FALSE), A68_BOOL);
    unchar_scanner (p, file, (char) ch);
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief PROC (REF FILE) VOID new line
\param p position in tree
**/

void genie_new_line (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    if (IS_NIL (STRING (file))) {
      WRITE (FD (file), NEWLINE_STRING);
    } else {
      add_c_string_to_a_string (p, STRING (file), NEWLINE_STRING);
    }
  } else if (READ_MOOD (file)) {
    BOOL_T go_on = A68_TRUE;
    while (go_on) {
      int ch;
      if (END_OF_FILE (file)) {
        end_of_file_error (p, ref_file);
      }
      ch = char_scanner (file);
      go_on = (BOOL_T) ((ch != NEWLINE_CHAR) && (ch != EOF_CHAR) && !END_OF_FILE (file));
    }
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief PROC (REF FILE) VOID new page
\param p position in tree
**/

void genie_new_page (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    if (IS_NIL (STRING (file))) {
      WRITE (FD (file), "\f");
    } else {
      add_c_string_to_a_string (p, STRING (file), "\f");
    }
  } else if (READ_MOOD (file)) {
    BOOL_T go_on = A68_TRUE;
    while (go_on) {
      int ch;
      if (END_OF_FILE (file)) {
        end_of_file_error (p, ref_file);
      }
      ch = char_scanner (file);
      go_on = (BOOL_T) ((ch != FORMFEED_CHAR) && (ch != EOF_CHAR) && !END_OF_FILE (file));
    }
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief PROC (REF FILE) VOID space
\param p position in tree
**/

void genie_space (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    WRITE (FD (file), " ");
  } else if (READ_MOOD (file)) {
    if (!END_OF_FILE (file)) {
      (void) char_scanner (file);
    }
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

#define IS_NL_FF(ch) ((ch) == NEWLINE_CHAR || (ch) == FORMFEED_CHAR)

/*!
\brief skip new-lines and form-feeds
\param p position in tree
\param ch pointer to scanned character
\param ref_file fat pointer to A68 file
**/

void skip_nl_ff (NODE_T * p, int *ch, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  while ((*ch) != EOF_CHAR && IS_NL_FF (*ch)) {
    A68_BOOL *z = (A68_BOOL *) STACK_TOP;
    ADDR_T pop_sp = stack_pointer;
    unchar_scanner (p, f, (char) (*ch));
    if (*ch == NEWLINE_CHAR) {
      on_event_handler (p, LINE_END_MENDED (f), ref_file);
      stack_pointer = pop_sp;
      if (VALUE (z) == A68_FALSE) {
        PUSH_REF (p, ref_file);
        genie_new_line (p);
      }
    } else if (*ch == FORMFEED_CHAR) {
      on_event_handler (p, PAGE_END_MENDED (f), ref_file);
      stack_pointer = pop_sp;
      if (VALUE (z) == A68_FALSE) {
        PUSH_REF (p, ref_file);
        genie_new_page (p);
      }
    }
    (*ch) = char_scanner (f);
  }
}

/*!
\brief scan an int from file
\param p position in tree
\param ref_file fat pointer to A68 file
**/

void scan_integer (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int ch;
  reset_transput_buffer (INPUT_BUFFER);
  ch = char_scanner (f);
  while (ch != EOF_CHAR && (IS_SPACE (ch) || IS_NL_FF (ch))) {
    if (IS_NL_FF (ch)) {
      skip_nl_ff (p, &ch, ref_file);
    } else {
      ch = char_scanner (f);
    }
  }
  if (ch != EOF_CHAR && (ch == '+' || ch == '-')) {
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
    ch = char_scanner (f);
  }
  while (ch != EOF_CHAR && IS_DIGIT (ch)) {
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
    ch = char_scanner (f);
  }
  if (ch != EOF_CHAR) {
    unchar_scanner (p, f, (char) ch);
  }
}

/*!
\brief scan a real from file
\param p position in tree
\param ref_file fat pointer to A68 file
**/

void scan_real (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  char x_e = EXPONENT_CHAR;
  int ch;
  reset_transput_buffer (INPUT_BUFFER);
  ch = char_scanner (f);
  while (ch != EOF_CHAR && (IS_SPACE (ch) || IS_NL_FF (ch))) {
    if (IS_NL_FF (ch)) {
      skip_nl_ff (p, &ch, ref_file);
    } else {
      ch = char_scanner (f);
    }
  }
  if (ch != EOF_CHAR && (ch == '+' || ch == '-')) {
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
    ch = char_scanner (f);
  }
  while (ch != EOF_CHAR && IS_DIGIT (ch)) {
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
    ch = char_scanner (f);
  }
  if (ch == EOF_CHAR || !(ch == POINT_CHAR || TO_UPPER (ch) == TO_UPPER (x_e))) {
    goto salida;
  }
  if (ch == POINT_CHAR) {
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
    ch = char_scanner (f);
    while (ch != EOF_CHAR && IS_DIGIT (ch)) {
      add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
      ch = char_scanner (f);
    }
  }
  if (ch == EOF_CHAR || TO_UPPER (ch) != TO_UPPER (x_e)) {
    goto salida;
  }
  if (TO_UPPER (ch) == TO_UPPER (x_e)) {
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
    ch = char_scanner (f);
    while (ch != EOF_CHAR && ch == BLANK_CHAR) {
      ch = char_scanner (f);
    }
    if (ch != EOF_CHAR && (ch == '+' || ch == '-')) {
      add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
      ch = char_scanner (f);
    }
    while (ch != EOF_CHAR && IS_DIGIT (ch)) {
      add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
      ch = char_scanner (f);
    }
  }
salida:if (ch != EOF_CHAR) {
    unchar_scanner (p, f, (char) ch);
  }
}

/*!
\brief scan a bits from file
\param p position in tree
\param ref_file fat pointer to A68 file
**/

void scan_bits (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int ch, flip = FLIP_CHAR, flop = FLOP_CHAR;
  reset_transput_buffer (INPUT_BUFFER);
  ch = char_scanner (f);
  while (ch != EOF_CHAR && (IS_SPACE (ch) || IS_NL_FF (ch))) {
    if (IS_NL_FF (ch)) {
      skip_nl_ff (p, &ch, ref_file);
    } else {
      ch = char_scanner (f);
    }
  }
  while (ch != EOF_CHAR && (ch == flip || ch == flop)) {
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
    ch = char_scanner (f);
  }
  if (ch != EOF_CHAR) {
    unchar_scanner (p, f, (char) ch);
  }
}

/*!
\brief scan a char from file
\param p position in tree
\param ref_file fat pointer to A68 file
**/

void scan_char (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int ch;
  reset_transput_buffer (INPUT_BUFFER);
  ch = char_scanner (f);
  skip_nl_ff (p, &ch, ref_file);
  if (ch != EOF_CHAR) {
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
  }
}

/*!
\brief scan a string from file
\param p position in tree
\param term string with terminators
\param ref_file fat pointer to A68 file
**/

void scan_string (NODE_T * p, char *term, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  if (END_OF_FILE (f)) {
    reset_transput_buffer (INPUT_BUFFER);
    end_of_file_error (p, ref_file);
  } else {
    BOOL_T go_on; int ch;
    reset_transput_buffer (INPUT_BUFFER);
    ch = char_scanner (f);
    go_on = A68_TRUE;
    while (go_on) {
      if (ch == EOF_CHAR || END_OF_FILE (f)) {
        if (get_transput_buffer_index (INPUT_BUFFER) == 0) {
          end_of_file_error (p, ref_file);
        }
        go_on = A68_FALSE;
      } else if (IS_NL_FF (ch)) {
        ADDR_T pop_sp = stack_pointer;
        unchar_scanner (p, f, (char) ch);
        if (ch == NEWLINE_CHAR) {
          on_event_handler (p, LINE_END_MENDED (f), ref_file);
        } else if (ch == FORMFEED_CHAR) {
          on_event_handler (p, PAGE_END_MENDED (f), ref_file);
        }
        stack_pointer = pop_sp;
        go_on = A68_FALSE;
      } else if (term != NO_TEXT && a68g_strchr (term, ch) != NO_TEXT) {
        go_on = A68_FALSE;
        unchar_scanner (p, f, (char) ch);
      } else {
        add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
        ch = char_scanner (f);
      }
    }
  }
}

/*!
\brief make temp file name
\param fn pointer to string to hold filename
\param permissions permissions to open file with
\return whether file is good for use
**/

BOOL_T a68g_mkstemp (char *fn, int flags, mode_t permissions)
{
/* "tmpnam" is not safe, "mkstemp" is Unix, so a68g brings its own tmpnam  */
#define TMP_SIZE 32
#define TRIALS 32
  char tfilename[BUFFER_SIZE];
  char *letters = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int i, k, len = (int) strlen (letters);
  BOOL_T good_file = A68_FALSE;
/* 
Next are prefixes to try.
First we try /tmp, and if that won't go, the current dir.
*/
  char *prefix[] = {
    "/tmp/a68g_",
    "./a68g_",
    NO_TEXT
  };
  for (i = 0; prefix[i] != NO_TEXT; i ++) {
    for (k = 0; k < TRIALS && good_file == A68_FALSE; k++) {
      int j, cindex;
      FILE_T fd;
      bufcpy (tfilename, prefix[i], BUFFER_SIZE);
      for (j = 0; j < TMP_SIZE; j++) {
        char chars[2];
        do {
          cindex = (int) (rng_53_bit () * len);
        } while (cindex < 0 || cindex >= len);
        chars[0] = letters[cindex];
        chars[1] = NULL_CHAR;
        bufcat (tfilename, chars, BUFFER_SIZE);
      }
      bufcat (tfilename, ".tmp", BUFFER_SIZE);
      RESET_ERRNO;
      fd = open (tfilename, flags | O_EXCL, permissions);
      good_file = (BOOL_T) (fd != A68_NO_FILENO && errno == 0);
      if (good_file) {
        (void) close (fd);
      }
    }
  }
  if (good_file) {
    bufcpy (fn, tfilename, BUFFER_SIZE);
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
#undef TMP_SIZE
#undef TRIALS
}

/*!
\brief open a file, or establish it
\param p position in tree
\param ref_file fat pointer to A68 file
\param flags required access mode
\param permissions optional permissions
\return file number
**/

FILE_T open_physical_file (NODE_T * p, A68_REF ref_file, int flags, mode_t permissions)
{
  A68_FILE *file;
  A68_REF ref_filename;
  char *filename;
  BOOL_T reading = (flags & ~O_BINARY) == A68_READ_ACCESS;
  BOOL_T writing = (flags & ~O_BINARY) == A68_WRITE_ACCESS;
  ABEND (reading == writing, ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!IS_NIL (STRING (file))) {
/* Associated file */
    TRANSPUT_BUFFER (file) = get_unblocked_transput_buffer (p);
    reset_transput_buffer (TRANSPUT_BUFFER (file));
    END_OF_FILE (file) = A68_FALSE;
    FILE_ENTRY (file) = -1;
    return (FD (file));
  } else if (IS_NIL (IDENTIFICATION (file))) {
/* No identification, so generate a unique identification. */
    if (reading) {
      return (A68_NO_FILENO);
    } else {
      char tfilename[BUFFER_SIZE];
      int len;
      if (!a68g_mkstemp (tfilename, flags, permissions)) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NO_TEMP);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      FD (file) = open (tfilename, flags, permissions);
      len = 1 + (int) strlen (tfilename);
      IDENTIFICATION (file) = heap_generator (p, MODE (C_STRING), len);
      BLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
      bufcpy (DEREF (char, &IDENTIFICATION (file)), tfilename, len);
      TRANSPUT_BUFFER (file) = get_unblocked_transput_buffer (p);
      reset_transput_buffer (TRANSPUT_BUFFER (file));
      END_OF_FILE (file) = A68_FALSE;
      TMP_FILE (file) = A68_TRUE;
      FILE_ENTRY (file) = store_file_entry (p, FD (file), tfilename, TMP_FILE (file));
      return (FD (file));
    }
  } else {
/* Opening an identified file */
    ref_filename = IDENTIFICATION (file);
    CHECK_REF (p, ref_filename, MODE (ROWS));
    filename = DEREF (char, &ref_filename);
    if (OPEN_EXCLUSIVE (file)) {
/* Establishing requires that the file does not exist */
      if (flags == (A68_WRITE_ACCESS)) {
        flags |= O_EXCL;
      }
      OPEN_EXCLUSIVE (file) = A68_FALSE;
    }
    FD (file) = open (filename, flags, permissions);
    TRANSPUT_BUFFER (file) = get_unblocked_transput_buffer (p);
    reset_transput_buffer (TRANSPUT_BUFFER (file));
    END_OF_FILE (file) = A68_FALSE;
    FILE_ENTRY (file) = store_file_entry (p, FD (file), filename, TMP_FILE (file));
    return (FD (file));
  }
}

/*!
\brief call PROC (REF FILE) VOID during transput
\param p position in tree
\param ref_file fat pointer to A68 file
\param z A68 routine to call
**/

void genie_call_proc_ref_file_void (NODE_T * p, A68_REF ref_file, A68_PROCEDURE z)
{
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  MOID_T *u = MODE (PROC_REF_FILE_VOID);
  PUSH_REF (p, ref_file);
  genie_call_procedure (p, MOID (&z), u, u, &z, pop_sp, pop_fp);
  stack_pointer = pop_sp;       /* VOIDING */
}

/* Unformatted transput */

/*!
\brief hexadecimal value of digit
\param ch digit
\return same
**/

static int char_value (int ch)
{
  switch (ch) {
  case '0':
    {
      return (0);
    }
  case '1':
    {
      return (1);
    }
  case '2':
    {
      return (2);
    }
  case '3':
    {
      return (3);
    }
  case '4':
    {
      return (4);
    }
  case '5':
    {
      return (5);
    }
  case '6':
    {
      return (6);
    }
  case '7':
    {
      return (7);
    }
  case '8':
    {
      return (8);
    }
  case '9':
    {
      return (9);
    }
  case 'A':
  case 'a':
    {
      return (10);
    }
  case 'B':
  case 'b':
    {
      return (11);
    }
  case 'C':
  case 'c':
    {
      return (12);
    }
  case 'D':
  case 'd':
    {
      return (13);
    }
  case 'E':
  case 'e':
    {
      return (14);
    }
  case 'F':
  case 'f':
    {
      return (15);
    }
  default:
    {
      return (-1);
    }
  }
}

/*!
\brief own strtoul; some systems have no strtoul
\param str string representing an unsigned int denotation
\param end points to first character after denotation
\param base exponent base
\return value of denotation in str
**/

unsigned a68g_strtoul (char *str, char **end, int base)
{
  if (str == NO_TEXT || str[0] == NULL_CHAR) {
    (*end) = NO_TEXT;
    errno = EDOM;
    return (0);
  } else {
    int j, k = 0, start;
    char *q = str;
    unsigned mul = 1, sum = 0;
    while (IS_SPACE (q[k])) {
      k++;
    }
    if (q[k] == '+') {
      k++;
    }
    start = k;
    while (IS_XDIGIT (q[k])) {
      k++;
    }
    if (k == start) {
      if (end != NO_VAR) {
        *end = str;
      }
      errno = EDOM;
      return (0);
    }
    if (end != NO_VAR) {
      (*end) = &q[k];
    }
    for (j = k - 1; j >= start; j--) {
      if (char_value (q[j]) >= base) {
        errno = EDOM;
        return (0);
      } else {
        unsigned add = (unsigned) ((unsigned) (char_value (q[j])) * mul);
        if (A68_MAX_UNT - sum >= add) {
          sum += add;
          mul *= (unsigned) base;
        } else {
          errno = ERANGE;
          return (0);
        }
      }
    }
    return (sum);
  }
}

/*!
\brief INT value of BITS denotation
\param p position in tree
\param str string with BITS denotation
\return same
**/

static unsigned bits_to_int (NODE_T * p, char *str)
{
  int base = 0;
  unsigned bits = 0;
  char *radix = NO_TEXT, *end = NO_TEXT;
  RESET_ERRNO;
  base = (int) a68g_strtoul (str, &radix, 10);
  if (radix != NO_TEXT && TO_UPPER (radix[0]) == TO_UPPER (RADIX_CHAR) && errno == 0) {
    if (base < 2 || base > 16) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, base);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    bits = a68g_strtoul (&(radix[1]), &end, base);
    if (end != NO_TEXT && end[0] == NULL_CHAR && errno == 0) {
      return (bits);
    }
  }
  diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, MODE (BITS));
  exit_genie (p, A68_RUNTIME_ERROR);
  return (0);
}

/*!
\brief LONG BITS value of LONG BITS denotation
\param p position in tree
\param z multi-precision number
\param str string with LONG BITS denotation 
\param m mode of 'z'
**/

static void long_bits_to_long_int (NODE_T * p, MP_T * z, char *str, MOID_T * m)
{
  int base = 0;
  char *radix = NO_TEXT;
  RESET_ERRNO;
  base = (int) a68g_strtoul (str, &radix, 10);
  if (radix != NO_TEXT && TO_UPPER (radix[0]) == TO_UPPER (RADIX_CHAR) && errno == 0) {
    int digits = get_mp_digits (m);
    ADDR_T pop_sp = stack_pointer;
    MP_T *v;
    MP_T *w;
    char *q = radix;
    STACK_MP (v, p, digits);
    STACK_MP (w, p, digits);
    while (q[0] != NULL_CHAR) {
      q++;
    }
    SET_MP_ZERO (z, digits);
    (void) set_mp_short (w, (MP_T) 1, 0, digits);
    if (base < 2 || base > 16) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, base);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    while ((--q) != radix) {
      int digit = char_value (q[0]);
      if (digit >= 0 && digit < base) {
        (void) mul_mp_digit (p, v, w, (MP_T) digit, digits);
        (void) add_mp (p, z, z, v, digits);
      } else {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, m);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      (void) mul_mp_digit (p, w, w, (MP_T) base, digits);
    }
    check_long_bits_value (p, z, m);
    stack_pointer = pop_sp;
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief convert string to required mode and store
\param p position in tree
\param m mode to convert to
\param a string to convert
\param item where to store result
\return whether conversion is successful
**/

BOOL_T genie_string_to_value_internal (NODE_T * p, MOID_T * m, char *a, BYTE_T * item)
{
  RESET_ERRNO;
/* strto.. does not mind empty strings */
  if (strlen (a) == 0) {
    return (A68_FALSE);
  }
  if (m == MODE (INT)) {
    A68_INT *z = (A68_INT *) item;
    char *end;
    VALUE (z) = (int) strtol (a, &end, 10);
    if (end[0] == NULL_CHAR && errno == 0) {
      STATUS (z) = INIT_MASK;
      return (A68_TRUE);
    } else {
      return (A68_FALSE);
    }
  } else if (m == MODE (REAL)) {
    A68_REAL *z = (A68_REAL *) item;
    char *end;
    VALUE (z) = strtod (a, &end);
    if (end[0] == NULL_CHAR && errno == 0) {
      STATUS (z) = INIT_MASK;
      return (A68_TRUE);
    } else {
      return (A68_FALSE);
    }
  } else if (m == MODE (LONG_INT) || m == MODE (LONGLONG_INT)) {
    int digits = get_mp_digits (m);
    MP_T *z = (MP_T *) item;
    if (string_to_mp (p, z, a, digits) == NO_MP) {
      return (A68_FALSE);
    }
    if (!check_mp_int (z, m)) {
      errno = ERANGE;
      return (A68_FALSE);
    }
    MP_STATUS (z) = (MP_T) INIT_MASK;
    return (A68_TRUE);
  } else if (m == MODE (LONG_REAL) || m == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (m);
    MP_T *z = (MP_T *) item;
    if (string_to_mp (p, z, a, digits) == NO_MP) {
      return (A68_FALSE);
    }
    MP_STATUS (z) = (MP_T) INIT_MASK;
    return (A68_TRUE);
  } else if (m == MODE (BOOL)) {
    A68_BOOL *z = (A68_BOOL *) item;
    char q = a[0], flip = FLIP_CHAR, flop = FLOP_CHAR;
    if (q == flip || q == flop) {
      VALUE (z) = (BOOL_T) (q == flip);
      STATUS (z) = INIT_MASK;
      return (A68_TRUE);
    } else {
      return (A68_FALSE);
    }
  } else if (m == MODE (BITS)) {
    A68_BITS *z = (A68_BITS *) item;
    int status = A68_TRUE;
    if (a[0] == FLIP_CHAR || a[0] == FLOP_CHAR) {
/* [] BOOL denotation is "TTFFFFTFT ..." */
      if (strlen (a) > (size_t) BITS_WIDTH) {
        errno = ERANGE;
        status = A68_FALSE;
      } else {
        int j = (int) strlen (a) - 1;
        unsigned k = 0x1;
        VALUE (z) = 0;
        for (; j >= 0; j--) {
          if (a[j] == FLIP_CHAR) {
            VALUE (z) += k;
          } else if (a[j] != FLOP_CHAR) {
            status = A68_FALSE;
          }
          k <<= 1;
        }
      }
    } else {
/* BITS denotation is also allowed */
      VALUE (z) = bits_to_int (p, a);
    }
    if (errno != 0 || status == A68_FALSE) {
      return (A68_FALSE);
    }
    STATUS (z) = INIT_MASK;
    return (A68_TRUE);
  } else if (m == MODE (LONG_BITS) || m == MODE (LONGLONG_BITS)) {
    int digits = get_mp_digits (m);
    int status = A68_TRUE;
    ADDR_T pop_sp = stack_pointer;
    MP_T *z = (MP_T *) item;
    if (a[0] == FLIP_CHAR || a[0] == FLOP_CHAR) {
/* [] BOOL denotation is "TTFFFFTFT ..." */
      if (strlen (a) > (size_t) BITS_WIDTH) {
        errno = ERANGE;
        status = A68_FALSE;
      } else {
        int j;
        MP_T *w;
        STACK_MP (w, p, digits);
        SET_MP_ZERO (z, digits);
        (void) set_mp_short (w, (MP_T) 1, 0, digits);
        for (j = (int) strlen (a) - 1; j >= 0; j--) {
          if (a[j] == FLIP_CHAR) {
            (void) add_mp (p, z, z, w, digits);
          } else if (a[j] != FLOP_CHAR) {
            status = A68_FALSE;
          }
          (void) mul_mp_digit (p, w, w, (MP_T) 2, digits);
        }
      }
    } else {
/* BITS denotation is also allowed */
      long_bits_to_long_int (p, z, a, m);
    }
    stack_pointer = pop_sp;
    if (errno != 0 || status == A68_FALSE) {
      return (A68_FALSE);
    }
    MP_STATUS (z) = (MP_T) INIT_MASK;
    return (A68_TRUE);
  }
  return (A68_FALSE);
}

/*!
\brief convert string in input buffer to value of required mode
\param p position in tree
\param mode mode to convert to
\param item where to store result
\param ref_file fat pointer to A68 file
**/

void genie_string_to_value (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  char *str = get_transput_buffer (INPUT_BUFFER);
  RESET_ERRNO;
  add_char_transput_buffer (p, INPUT_BUFFER, NULL_CHAR);        /* end string, just in case */
  if (mode == MODE (INT)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A68_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A68_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (REAL)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A68_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A68_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (BOOL)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A68_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (BITS)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A68_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A68_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (CHAR)) {
    A68_CHAR *z = (A68_CHAR *) item;
    if (str[0] == NULL_CHAR) {
/*      value_error (p, mode, ref_file); */
      VALUE (z) = NULL_CHAR;
      STATUS (z) = INIT_MASK;
    } else {
      int len = (int) strlen (str);
      if (len == 0 || len > 1) {
        value_error (p, mode, ref_file);
      }
      VALUE (z) = str[0];
      STATUS (z) = INIT_MASK;
    }
  } else if (mode == MODE (STRING)) {
    A68_REF z;
    z = c_to_a_string (p, str, get_transput_buffer_index (INPUT_BUFFER) - 1);
/*
    z = c_to_a_string (p, str, DEFAULT_WIDTH);
*/
    *(A68_REF *) item = z;
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief read object from file
\param p position in tree
\param mode mode to read
\param item where to store result
\param ref_file fat pointer to A68 file
**/

void genie_read_standard (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  RESET_ERRNO;
  if (mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    scan_integer (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    scan_real (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (BOOL)) {
    scan_char (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (CHAR)) {
    scan_char (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (BITS) || mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    scan_bits (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (STRING)) {
    char *term = DEREF (char, &TERMINATOR (f));
    scan_string (p, term, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (IS (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NO_PACK; FORWARD (q)) {
      genie_read_standard (p, MOID (q), &item[OFFSET (q)], ref_file);
    }
  } else if (IS (mode, UNION_SYMBOL)) {
    A68_UNION *z = (A68_UNION *) item;
    if (!(STATUS (z) | INIT_MASK) || VALUE (z) == NULL) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE, mode);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    genie_read_standard (p, (MOID_T *) (VALUE (z)), &item[A68_UNION_SIZE], ref_file);
  } else if (IS (mode, ROW_SYMBOL) || IS (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    CHECK_INIT (p, INITIALISED ((A68_REF *) item), mode);
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, DIM (arr)) > 0) {
      BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
      BOOL_T done = A68_FALSE;
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        ADDR_T a68g_index = calculate_internal_index (tup, DIM (arr));
        ADDR_T elem_addr = ROW_ELEMENT (arr, a68g_index);
        genie_read_standard (p, SUB (deflexed), &base_addr[elem_addr], ref_file);
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief PROC ([] SIMPLIN) VOID read
\param p position in tree
**/

void genie_read (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_in (p);
  PUSH_REF (p, row);
  genie_read_file (p);
}

/*!
\brief open for reading
\param p position in tree
**/

void open_for_reading (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *file = FILE_DEREF (&ref_file);
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!GET (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "getting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!READ_MOOD (file) && !WRITE_MOOD (file)) {
    if (IS_NIL (STRING (file))) {
      if ((FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS, 0)) == A68_NO_FILENO) {
        open_error (p, ref_file, "getting");
      }
    } else {
      FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS, 0);
    }
    DRAW_MOOD (file) = A68_FALSE;
    READ_MOOD (file) = A68_TRUE;
    WRITE_MOOD (file) = A68_FALSE;
    CHAR_MOOD (file) = A68_TRUE;
  }
  if (!CHAR_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief PROC (REF FILE, [] SIMPLIN) VOID get
\param p position in tree
**/

void genie_read_file (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  CHECK_REF (p, row, MODE (ROW_SIMPLIN));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  open_for_reading (p, ref_file);
/* Read */
  if (elems <= 0) {
    return;
  }
  base_address = DEREF (BYTE_T, &ARRAY (arr));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_UNION *z = (A68_UNION *) & base_address[elem_index];
    MOID_T *mode = (MOID_T *) (VALUE (z));
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + A68_UNION_SIZE];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, MODE (FORMAT));
      exit_genie (p, A68_RUNTIME_ERROR);
    } else if (mode == MODE (REF_SOUND)) {
      read_sound (p, ref_file, DEREF (A68_SOUND, (A68_REF *) item));
    } else {
      if (END_OF_FILE (file)) {
        end_of_file_error (p, ref_file);
      }
      CHECK_REF (p, *(A68_REF *) item, mode);
      genie_read_standard (p, SUB (mode), ADDRESS ((A68_REF *) item), ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLIN));
  }
}

/*!
\brief convert value to string
\param p position in tree
\param moid mode to convert to
\param item pointer to value
\param mod format modifier
**/

void genie_value_to_string (NODE_T * p, MOID_T * moid, BYTE_T * item, int mod)
{
  if (moid == MODE (INT)) {
    A68_INT *z = (A68_INT *) item;
    PUSH_UNION (p, MODE (INT));
    PUSH_PRIMITIVE (p, VALUE (z), A68_INT);
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + ALIGNED_SIZE_OF (A68_INT)));
    if (mod == FORMAT_ITEM_G) {
      PUSH_PRIMITIVE (p, INT_WIDTH + 1, A68_INT);
      genie_whole (p);
    } else if (mod == FORMAT_ITEM_H) {
      PUSH_PRIMITIVE (p, REAL_WIDTH + EXP_WIDTH + 4, A68_INT);
      PUSH_PRIMITIVE (p, REAL_WIDTH - 1, A68_INT);
      PUSH_PRIMITIVE (p, EXP_WIDTH + 1, A68_INT);
      PUSH_PRIMITIVE (p, 3, A68_INT);
      genie_real (p);
    }
  } else if (moid == MODE (LONG_INT)) {
    MP_T *z = (MP_T *) item;
    PUSH_UNION (p, MODE (LONG_INT));
    PUSH (p, z, get_mp_size (MODE (LONG_INT)));
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + get_mp_size (MODE (LONG_INT))));
    if (mod == FORMAT_ITEM_G) {
      PUSH_PRIMITIVE (p, LONG_WIDTH + 1, A68_INT);
      genie_whole (p);
    } else if (mod == FORMAT_ITEM_H) {
      PUSH_PRIMITIVE (p, LONG_REAL_WIDTH + LONG_EXP_WIDTH + 4, A68_INT);
      PUSH_PRIMITIVE (p, LONG_REAL_WIDTH - 1, A68_INT);
      PUSH_PRIMITIVE (p, LONG_EXP_WIDTH + 1, A68_INT);
      PUSH_PRIMITIVE (p, 3, A68_INT);
      genie_real (p);
    }
  } else if (moid == MODE (LONGLONG_INT)) {
    MP_T *z = (MP_T *) item;
    PUSH_UNION (p, MODE (LONGLONG_INT));
    PUSH (p, z, get_mp_size (MODE (LONGLONG_INT)));
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + get_mp_size (MODE (LONGLONG_INT))));
    if (mod == FORMAT_ITEM_G) {
      PUSH_PRIMITIVE (p, LONGLONG_WIDTH + 1, A68_INT);
      genie_whole (p);
    } else if (mod == FORMAT_ITEM_H) {
      PUSH_PRIMITIVE (p, LONGLONG_REAL_WIDTH + LONGLONG_EXP_WIDTH + 4, A68_INT);
      PUSH_PRIMITIVE (p, LONGLONG_REAL_WIDTH - 1, A68_INT);
      PUSH_PRIMITIVE (p, LONGLONG_EXP_WIDTH + 1, A68_INT);
      PUSH_PRIMITIVE (p, 3, A68_INT);
      genie_real (p);
    }
  } else if (moid == MODE (REAL)) {
    A68_REAL *z = (A68_REAL *) item;
    PUSH_UNION (p, MODE (REAL));
    PUSH_PRIMITIVE (p, VALUE (z), A68_REAL);
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + ALIGNED_SIZE_OF (A68_REAL)));
    PUSH_PRIMITIVE (p, REAL_WIDTH + EXP_WIDTH + 4, A68_INT);
    PUSH_PRIMITIVE (p, REAL_WIDTH - 1, A68_INT);
    PUSH_PRIMITIVE (p, EXP_WIDTH + 1, A68_INT);
    if (mod == FORMAT_ITEM_G) {
      genie_float (p);
    } else if (mod == FORMAT_ITEM_H) {
      PUSH_PRIMITIVE (p, 3, A68_INT);
      genie_real (p);
    }
  } else if (moid == MODE (LONG_REAL)) {
    MP_T *z = (MP_T *) item;
    PUSH_UNION (p, MODE (LONG_REAL));
    PUSH (p, z, (int) get_mp_size (MODE (LONG_REAL)));
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + get_mp_size (MODE (LONG_REAL))));
    PUSH_PRIMITIVE (p, LONG_REAL_WIDTH + LONG_EXP_WIDTH + 4, A68_INT);
    PUSH_PRIMITIVE (p, LONG_REAL_WIDTH - 1, A68_INT);
    PUSH_PRIMITIVE (p, LONG_EXP_WIDTH + 1, A68_INT);
    if (mod == FORMAT_ITEM_G) {
      genie_float (p);
    } else if (mod == FORMAT_ITEM_H) {
      PUSH_PRIMITIVE (p, 3, A68_INT);
      genie_real (p);
    }
  } else if (moid == MODE (LONGLONG_REAL)) {
    MP_T *z = (MP_T *) item;
    PUSH_UNION (p, MODE (LONGLONG_REAL));
    PUSH (p, z, (int) get_mp_size (MODE (LONGLONG_REAL)));
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + get_mp_size (MODE (LONGLONG_REAL))));
    PUSH_PRIMITIVE (p, LONGLONG_REAL_WIDTH + LONGLONG_EXP_WIDTH + 4, A68_INT);
    PUSH_PRIMITIVE (p, LONGLONG_REAL_WIDTH - 1, A68_INT);
    PUSH_PRIMITIVE (p, LONGLONG_EXP_WIDTH + 1, A68_INT);
    if (mod == FORMAT_ITEM_G) {
      genie_float (p);
    } else if (mod == FORMAT_ITEM_H) {
      PUSH_PRIMITIVE (p, 3, A68_INT);
      genie_real (p);
    }
  } else if (moid == MODE (BITS)) {
    A68_BITS *z = (A68_BITS *) item;
    char *str = stack_string (p, 8 + BITS_WIDTH);
    unsigned bit = 0x1;
    int j;
    for (j = 1; j < BITS_WIDTH; j++) {
      bit <<= 1;
    }
    for (j = 0; j < BITS_WIDTH; j++) {
      str[j] = (char) ((VALUE (z) & bit) ? FLIP_CHAR : FLOP_CHAR);
      bit >>= 1;
    }
    str[j] = NULL_CHAR;
  } else if (moid == MODE (LONG_BITS) || moid == MODE (LONGLONG_BITS)) {
    int bits = get_mp_bits_width (moid), word = get_mp_bits_words (moid);
    int cher = bits;
    char *str = stack_string (p, 8 + bits);
    ADDR_T pop_sp = stack_pointer;
    unsigned *row = stack_mp_bits (p, (MP_T *) item, moid);
    str[cher--] = NULL_CHAR;
    while (cher >= 0) {
      unsigned bit = 0x1;
      int j;
      for (j = 0; j < MP_BITS_BITS && cher >= 0; j++) {
        str[cher--] = (char) ((row[word - 1] & bit) ? FLIP_CHAR : FLOP_CHAR);
        bit <<= 1;
      }
      word--;
    }
    stack_pointer = pop_sp;
  }
}

/*!
\brief print object to file
\param p position in tree
\param mode mode of object to print
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

void genie_write_standard (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  RESET_ERRNO;
  if (mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    genie_value_to_string (p, mode, item, FORMAT_ITEM_G);
    add_string_from_stack_transput_buffer (p, UNFORMATTED_BUFFER);
  } else if (mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    genie_value_to_string (p, mode, item, FORMAT_ITEM_G);
    add_string_from_stack_transput_buffer (p, UNFORMATTED_BUFFER);
  } else if (mode == MODE (BOOL)) {
    A68_BOOL *z = (A68_BOOL *) item;
    char flipflop = (char) (VALUE (z) == A68_TRUE ? FLIP_CHAR : FLOP_CHAR);
    add_char_transput_buffer (p, UNFORMATTED_BUFFER, flipflop);
  } else if (mode == MODE (CHAR)) {
    A68_CHAR *ch = (A68_CHAR *) item;
    add_char_transput_buffer (p, UNFORMATTED_BUFFER, (char) VALUE (ch));
  } else if (mode == MODE (BITS) || mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    char *str = (char *) STACK_TOP;
    genie_value_to_string (p, mode, item, FORMAT_ITEM_G);
    add_string_transput_buffer (p, UNFORMATTED_BUFFER, str);
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
/* Handle these separately since this is faster than straightening */
    add_a_string_transput_buffer (p, UNFORMATTED_BUFFER, item);
  } else if (IS (mode, UNION_SYMBOL)) {
    A68_UNION *z = (A68_UNION *) item;
    genie_write_standard (p, (MOID_T *) (VALUE (z)), &item[A68_UNION_SIZE], ref_file);
  } else if (IS (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NO_PACK; FORWARD (q)) {
      BYTE_T *elem = &item[OFFSET (q)];
      genie_check_initialisation (p, elem, MOID (q));
      genie_write_standard (p, MOID (q), elem, ref_file);
    }
  } else if (IS (mode, ROW_SYMBOL) || IS (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    CHECK_INIT (p, INITIALISED ((A68_REF *) item), MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, DIM (arr)) > 0) {
      BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
      BOOL_T done = A68_FALSE;
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        ADDR_T a68g_index = calculate_internal_index (tup, DIM (arr));
        ADDR_T elem_addr = ROW_ELEMENT (arr, a68g_index);
        BYTE_T *elem = &base_addr[elem_addr];
        genie_check_initialisation (p, elem, SUB (deflexed));
        genie_write_standard (p, SUB (deflexed), elem, ref_file);
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
  if (errno != 0) {
    ABEND (IS_NIL (ref_file), "conversion error: ", error_specification ());
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief PROC ([] SIMPLOUT) VOID print, write
\param p position in tree
**/

void genie_write (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_out (p);
  PUSH_REF (p, row);
  genie_write_file (p);
}

/*!
\brief open for reading
\param p position in tree
**/

void open_for_writing (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *file = FILE_DEREF (&ref_file);
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (READ_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "read");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!PUT (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!READ_MOOD (file) && !WRITE_MOOD (file)) {
    if (IS_NIL (STRING (file))) {
      if ((FD (file) = open_physical_file (p, ref_file, A68_WRITE_ACCESS, A68_PROTECTION)) == A68_NO_FILENO) {
        open_error (p, ref_file, "putting");
      }
    } else {
      FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS, 0);
    }
    DRAW_MOOD (file) = A68_FALSE;
    READ_MOOD (file) = A68_FALSE;
    WRITE_MOOD (file) = A68_TRUE;
    CHAR_MOOD (file) = A68_TRUE;
  }
  if (!CHAR_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief PROC (REF FILE, [] SIMPLOUT) VOID put
\param p position in tree
**/

void genie_write_file (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  CHECK_REF (p, row, MODE (ROW_SIMPLOUT));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  open_for_writing (p, ref_file);
/* Write. */
  if (elems <= 0) {
    return;
  }
  base_address = DEREF (BYTE_T, &ARRAY (arr));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_UNION *z = (A68_UNION *) & (base_address[elem_index]);
    MOID_T *mode = (MOID_T *) (VALUE (z));
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + A68_UNION_SIZE];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, MODE (FORMAT));
      exit_genie (p, A68_RUNTIME_ERROR);
    } else if (mode == MODE (SOUND)) {
      write_sound (p, ref_file, (A68_SOUND *) item);
    } else {
      reset_transput_buffer (UNFORMATTED_BUFFER);
      genie_write_standard (p, mode, item, ref_file);
      write_purge_buffer (p, ref_file, UNFORMATTED_BUFFER);
    }
    elem_index += MOID_SIZE (MODE (SIMPLOUT));
  }
}

/*!
\brief read object binary from file
\param p position in tree
\param mode mode to read
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void genie_read_bin_standard (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  RESET_ERRNO;
  if (mode == MODE (INT)) {
    A68_INT *z = (A68_INT *) item;
    ASSERT (io_read (FD (f), &(VALUE (z)), sizeof (VALUE (z))) != -1);
    STATUS (z) = INIT_MASK;
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    MP_T *z = (MP_T *) item;
    ASSERT (io_read (FD (f), z, (size_t) get_mp_size (mode)) != -1);
    MP_STATUS (z) = (MP_T) INIT_MASK;
  } else if (mode == MODE (REAL)) {
    A68_REAL *z = (A68_REAL *) item;
    ASSERT (io_read (FD (f), &(VALUE (z)), sizeof (VALUE (z))) != -1);
    STATUS (z) = INIT_MASK;
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    MP_T *z = (MP_T *) item;
    ASSERT (io_read (FD (f), z, (size_t) get_mp_size (mode)) != -1);
    MP_STATUS (z) = (MP_T) INIT_MASK;
  } else if (mode == MODE (BOOL)) {
    A68_BOOL *z = (A68_BOOL *) item;
    ASSERT (io_read (FD (f), &(VALUE (z)), sizeof (VALUE (z))) != -1);
    STATUS (z) = INIT_MASK;
  } else if (mode == MODE (CHAR)) {
    A68_CHAR *z = (A68_CHAR *) item;
    ASSERT (io_read (FD (f), &(VALUE (z)), sizeof (VALUE (z))) != -1);
    STATUS (z) = INIT_MASK;
  } else if (mode == MODE (BITS)) {
    A68_BITS *z = (A68_BITS *) item;
    ASSERT (io_read (FD (f), &(VALUE (z)), sizeof (VALUE (z))) != -1);
    STATUS (z) = INIT_MASK;
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    MP_T *z = (MP_T *) item;
    ASSERT (io_read (FD (f), z, (size_t) get_mp_size (mode)) != -1);
    MP_STATUS (z) = (MP_T) INIT_MASK;
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
    int len, k;
    ASSERT (io_read (FD (f), &(len), sizeof (len)) != -1);
    reset_transput_buffer (UNFORMATTED_BUFFER);
    for (k = 0; k < len; k++) {
      A68_CHAR z;
      ASSERT (io_read (FD (f), &(VALUE (&z)), sizeof (VALUE (&z))) != -1);
      add_char_transput_buffer (p, UNFORMATTED_BUFFER, (char) VALUE (&z));
    }
    *(A68_REF *) item = c_to_a_string (p, get_transput_buffer (UNFORMATTED_BUFFER), DEFAULT_WIDTH);
  } else if (IS (mode, UNION_SYMBOL)) {
    A68_UNION *z = (A68_UNION *) item;
    if (!(STATUS (z) | INIT_MASK) || VALUE (z) == NULL) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE, mode);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    genie_read_bin_standard (p, (MOID_T *) (VALUE (z)), &item[A68_UNION_SIZE], ref_file);
  } else if (IS (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NO_PACK; FORWARD (q)) {
      genie_read_bin_standard (p, MOID (q), &item[OFFSET (q)], ref_file);
    }
  } else if (IS (mode, ROW_SYMBOL) || IS (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    CHECK_INIT (p, INITIALISED ((A68_REF *) item), MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, DIM (arr)) > 0) {
      BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
      BOOL_T done = A68_FALSE;
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        ADDR_T a68g_index = calculate_internal_index (tup, DIM (arr));
        ADDR_T elem_addr = ROW_ELEMENT (arr, a68g_index);
        genie_read_bin_standard (p, SUB (deflexed), &base_addr[elem_addr], ref_file);
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief PROC ([] SIMPLIN) VOID read bin
\param p position in tree
**/

void genie_read_bin (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_back (p);
  PUSH_REF (p, row);
  genie_read_bin_file (p);
}

/*!
\brief PROC (REF FILE, [] SIMPLIN) VOID get bin
\param p position in tree
**/

void genie_read_bin_file (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  CHECK_REF (p, row, MODE (ROW_SIMPLIN));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  ref_file = *(A68_REF *) STACK_TOP;
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!GET (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "getting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!BIN (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "binary getting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!READ_MOOD (file) && !WRITE_MOOD (file)) {
    if ((FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS | O_BINARY, 0)) == A68_NO_FILENO) {
      open_error (p, ref_file, "binary getting");
    }
    DRAW_MOOD (file) = A68_FALSE;
    READ_MOOD (file) = A68_TRUE;
    WRITE_MOOD (file) = A68_FALSE;
    CHAR_MOOD (file) = A68_FALSE;
  }
  if (CHAR_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "text");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Read */
  if (elems <= 0) {
    return;
  }
  elem_index = 0;
  base_address = DEREF (BYTE_T, &ARRAY (arr));
  for (k = 0; k < elems; k++) {
    A68_UNION *z = (A68_UNION *) & base_address[elem_index];
    MOID_T *mode = (MOID_T *) (VALUE (z));
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + A68_UNION_SIZE];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, MODE (FORMAT));
      exit_genie (p, A68_RUNTIME_ERROR);
    } else if (mode == MODE (REF_SOUND)) {
      read_sound (p, ref_file, (A68_SOUND *) ADDRESS ((A68_REF *) item));
    } else {
      if (END_OF_FILE (file)) {
        end_of_file_error (p, ref_file);
      }
      CHECK_REF (p, *(A68_REF *) item, mode);
      genie_read_bin_standard (p, SUB (mode), ADDRESS ((A68_REF *) item), ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLIN));
  }
}

/*!
\brief write object binary to file
\param p position in tree
\param mode mode to write
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void genie_write_bin_standard (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  RESET_ERRNO;
  if (mode == MODE (INT)) {
    ASSERT (io_write (FD (f), &(VALUE ((A68_INT *) item)), sizeof (VALUE ((A68_INT *) item))) != -1);
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    ASSERT (io_write (FD (f), (MP_T *) item, (size_t) get_mp_size (mode)) != -1);
  } else if (mode == MODE (REAL)) {
    ASSERT (io_write (FD (f), &(VALUE ((A68_REAL *) item)), sizeof (VALUE ((A68_REAL *) item))) != -1);
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    ASSERT (io_write (FD (f), (MP_T *) item, (size_t) get_mp_size (mode)) != -1);
  } else if (mode == MODE (BOOL)) {
    ASSERT (io_write (FD (f), &(VALUE ((A68_BOOL *) item)), sizeof (VALUE ((A68_BOOL *) item))) != -1);
  } else if (mode == MODE (CHAR)) {
    ASSERT (io_write (FD (f), &(VALUE ((A68_CHAR *) item)), sizeof (VALUE ((A68_CHAR *) item))) != -1);
  } else if (mode == MODE (BITS)) {
    ASSERT (io_write (FD (f), &(VALUE ((A68_BITS *) item)), sizeof (VALUE ((A68_BITS *) item))) != -1);
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    ASSERT (io_write (FD (f), (MP_T *) item, (size_t) get_mp_size (mode)) != -1);
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
    int len;
    reset_transput_buffer (UNFORMATTED_BUFFER);
    add_a_string_transput_buffer (p, UNFORMATTED_BUFFER, item);
    len = get_transput_buffer_index (UNFORMATTED_BUFFER);
    ASSERT (io_write (FD (f), &(len), sizeof (len)) != -1);
    WRITE (FD (f), get_transput_buffer (UNFORMATTED_BUFFER));
  } else if (IS (mode, UNION_SYMBOL)) {
    A68_UNION *z = (A68_UNION *) item;
    genie_write_bin_standard (p, (MOID_T *) (VALUE (z)), &item[A68_UNION_SIZE], ref_file);
  } else if (IS (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NO_PACK; FORWARD (q)) {
      BYTE_T *elem = &item[OFFSET (q)];
      genie_check_initialisation (p, elem, MOID (q));
      genie_write_bin_standard (p, MOID (q), elem, ref_file);
    }
  } else if (IS (mode, ROW_SYMBOL) || IS (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    CHECK_INIT (p, INITIALISED ((A68_REF *) item), MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, DIM (arr)) > 0) {
      BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
      BOOL_T done = A68_FALSE;
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        ADDR_T a68g_index = calculate_internal_index (tup, DIM (arr));
        ADDR_T elem_addr = ROW_ELEMENT (arr, a68g_index);
        BYTE_T *elem = &base_addr[elem_addr];
        genie_check_initialisation (p, elem, SUB (deflexed));
        genie_write_bin_standard (p, SUB (deflexed), elem, ref_file);
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief PROC ([] SIMPLOUT) VOID write bin, print bin
\param p position in tree
**/

void genie_write_bin (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_back (p);
  PUSH_REF (p, row);
  genie_write_bin_file (p);
}

/*!
\brief PROC (REF FILE, [] SIMPLOUT) VOID put bin
\param p position in tree
**/

void genie_write_bin_file (NODE_T * p)
{
  A68_REF ref_file, row;
  A68_FILE *file;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  CHECK_REF (p, row, MODE (ROW_SIMPLOUT));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  ref_file = *(A68_REF *) STACK_TOP;
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (READ_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "read");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!PUT (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!BIN (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "binary putting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!READ_MOOD (file) && !WRITE_MOOD (file)) {
    if ((FD (file) = open_physical_file (p, ref_file, A68_WRITE_ACCESS | O_BINARY, A68_PROTECTION)) == A68_NO_FILENO) {
      open_error (p, ref_file, "binary putting");
    }
    DRAW_MOOD (file) = A68_FALSE;
    READ_MOOD (file) = A68_FALSE;
    WRITE_MOOD (file) = A68_TRUE;
    CHAR_MOOD (file) = A68_FALSE;
  }
  if (CHAR_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "text");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (elems <= 0) {
    return;
  }
  base_address = DEREF (BYTE_T, &ARRAY (arr));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_UNION *z = (A68_UNION *) & base_address[elem_index];
    MOID_T *mode = (MOID_T *) (VALUE (z));
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + A68_UNION_SIZE];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, MODE (FORMAT));
      exit_genie (p, A68_RUNTIME_ERROR);
    } else if (mode == MODE (SOUND)) {
      write_sound (p, ref_file, (A68_SOUND *) item);
    } else {
      genie_write_bin_standard (p, mode, item, ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLOUT));
  }
}

/*
Next are formatting routines "whole", "fixed" and "float" for mode
INT, LONG INT and LONG LONG INT, and REAL, LONG REAL and LONG LONG REAL.
They are direct implementations of the routines described in the
Revised Report, although those were only meant as a specification.
The rest of Algol68G should only reference "genie_whole", "genie_fixed"
or "genie_float" since internal routines like "sub_fixed" may leave the
stack corrupted when called directly.
*/

/*!
\brief generate a string of error chars
\param s string to store error chars
\param n number of error chars
\return same
**/

char *error_chars (char *s, int n)
{
  int k = (n != 0 ? ABS (n) : 1);
  s[k] = NULL_CHAR;
  while (--k >= 0) {
    s[k] = ERROR_CHAR;
  }
  return (s);
}

/*!
\brief convert temporary C string to A68 string
\param p position in tree
\param temp_string temporary C string
\return same
**/

A68_REF tmp_to_a68_string (NODE_T * p, char *temp_string)
{
  A68_REF z;
/* no compaction allowed since temp_string might be up for garbage collecting .. */
  z = c_to_a_string (p, temp_string, DEFAULT_WIDTH);
  return (z);
}

/*!
\brief add c to str, assuming that "str" is large enough
\param c char to add before string
\param str string to add in front of
\return string
**/

static char *plusto (char c, char *str)
{
  MOVE (&str[1], &str[0], (unsigned) (strlen (str) + 1));
  str[0] = c;
  return (str);
}

/*!
\brief add c to str, assuming that "str" is large enough
\param str string to add to
\param c char to add
\param strwid width of 'str'
\return string
**/

char *string_plusab_char (char *str, char c, int strwid)
{
  char z[2];
  z[0] = c;
  z[1] = NULL_CHAR;
  bufcat (str, z, strwid);
  return (str);
}

/*!
\brief add leading spaces to str until length is width
\param str string to add in front of
\param width required width of 'str'
\return string
**/

static char *leading_spaces (char *str, int width)
{
  int j = width - (int) strlen (str);
  while (--j >= 0) {
    (void) plusto (BLANK_CHAR, str);
  }
  return (str);
}

/*!
\brief convert int to char using a table
\param k int to convert
\return character
**/

static char digchar (int k)
{
  char *s = "0123456789abcdef";
  if (k >= 0 && k < (int) strlen (s)) {
    return (s[k]);
  } else {
    return (ERROR_CHAR);
  }
}

/*!
\brief standard string for LONG INT
\param p position in tree
\param m mp number
\param digits digits
\param width width
\return same
**/

char *long_sub_whole (NODE_T * p, MP_T * m, int digits, int width)
{
  ADDR_T pop_sp;
  char *s;
  MP_T *n;
  int len = 0;
  s = stack_string (p, 8 + width);
  s[0] = NULL_CHAR;
  pop_sp = stack_pointer;
  STACK_MP (n, p, digits);
  MOVE_MP (n, m, digits);
  do {
    if (len < width) {
/* Sic transit gloria mundi */
      int n_mod_10 = (int) MP_DIGIT (n, (int) (1 + MP_EXPONENT (n))) % 10;
      (void) plusto (digchar (n_mod_10), s);
    }
    len++;
    (void) over_mp_digit (p, n, n, (MP_T) 10, digits);
  } while (MP_DIGIT (n, 1) > 0);
  if (len > width) {
    (void) error_chars (s, width);
  }
  stack_pointer = pop_sp;
  return (s);
}

/*!
\brief standard string for INT
\param p position in tree
\param n value
\param width width
\return same
**/

char *sub_whole (NODE_T * p, int n, int width)
{
  char *s = stack_string (p, 8 + width);
  int len = 0;
  s[0] = NULL_CHAR;
  do {
    if (len < width) {
      (void) plusto (digchar (n % 10), s);
    }
    len++;
    n /= 10;
  } while (n != 0);
  if (len > width) {
    (void) error_chars (s, width);
  }
  return (s);
}

/*!
\brief formatted string for NUMBER
\param p position in tree
\return string
**/

char *whole (NODE_T * p)
{
  int pop_sp, arg_sp;
  A68_INT width;
  MOID_T *mode;
  POP_OBJECT (p, &width, A68_INT);
  arg_sp = stack_pointer;
  DECREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
  pop_sp = stack_pointer;
  mode = (MOID_T *) (VALUE ((A68_UNION *) STACK_TOP));
  if (mode == MODE (INT)) {
    int x = VALUE ((A68_INT *) (STACK_OFFSET (A68_UNION_SIZE)));
    int length = ABS (VALUE (&width)) - (x < 0 || VALUE (&width) > 0 ? 1 : 0);
    int n = ABS (x);
    int size = (x < 0 ? 1 : (VALUE (&width) > 0 ? 1 : 0));
    char *s;
    if (VALUE (&width) == 0) {
      int m = n;
      length = 0;
      while ((m /= 10, length++, m != 0)) {
        ;
      }
    }
    size += length;
    size = 8 + (size > VALUE (&width) ? size : VALUE (&width));
    s = stack_string (p, size);
    bufcpy (s, sub_whole (p, n, length), size);
    if (length == 0 || a68g_strchr (s, ERROR_CHAR) != NO_TEXT) {
      (void) error_chars (s, VALUE (&width));
    } else {
      if (x < 0) {
        (void) plusto ('-', s);
      } else if (VALUE (&width) > 0) {
        (void) plusto ('+', s);
      }
      if (VALUE (&width) != 0) {
        (void) leading_spaces (s, ABS (VALUE (&width)));
      }
    }
    return (s);
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    int digits = get_mp_digits (mode);
    int length, size;
    char *s;
    MP_T *n = (MP_T *) (STACK_OFFSET (A68_UNION_SIZE));
    BOOL_T ltz;
    stack_pointer = arg_sp;     /* We keep the mp where it's at */
    if (MP_EXPONENT (n) >= (MP_T) digits) {
      int max_length = (mode == MODE (LONG_INT) ? LONG_INT_WIDTH : LONGLONG_INT_WIDTH);
      length = (VALUE (&width) == 0 ? max_length : VALUE (&width));
      s = stack_string (p, 1 + length);
      (void) error_chars (s, length);
      return (s);
    }
    ltz = (BOOL_T) (MP_DIGIT (n, 1) < 0);
    length = ABS (VALUE (&width)) - (ltz || VALUE (&width) > 0 ? 1 : 0);
    size = (ltz ? 1 : (VALUE (&width) > 0 ? 1 : 0));
    MP_DIGIT (n, 1) = ABS (MP_DIGIT (n, 1));
    if (VALUE (&width) == 0) {
      MP_T *m;
      STACK_MP (m, p, digits);
      MOVE_MP (m, n, digits);
      length = 0;
      while ((over_mp_digit (p, m, m, (MP_T) 10, digits), length++, MP_DIGIT (m, 1) != 0)) {
        ;
      }
    }
    size += length;
    size = 8 + (size > VALUE (&width) ? size : VALUE (&width));
    s = stack_string (p, size);
    bufcpy (s, long_sub_whole (p, n, digits, length), size);
    if (length == 0 || a68g_strchr (s, ERROR_CHAR) != NO_TEXT) {
      (void) error_chars (s, VALUE (&width));
    } else {
      if (ltz) {
        (void) plusto ('-', s);
      } else if (VALUE (&width) > 0) {
        (void) plusto ('+', s);
      }
      if (VALUE (&width) != 0) {
        (void) leading_spaces (s, ABS (VALUE (&width)));
      }
    }
    return (s);
  } else if (mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
    PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
    PUSH_PRIMITIVE (p, 0, A68_INT);
    return (fixed (p));
  }
  return (NO_TEXT);
}

/*!
\brief fetch next digit from LONG
\param p position in tree
\param y mp number
\param digits digits
\return next digit
**/

static char long_choose_dig (NODE_T * p, MP_T * y, int digits)
{
/* Assuming positive "y" */
  int pop_sp = stack_pointer, c;
  MP_T *t;
  STACK_MP (t, p, digits);
  (void) mul_mp_digit (p, y, y, (MP_T) 10, digits);
  c = MP_EXPONENT (y) == 0 ? (int) MP_DIGIT (y, 1) : 0;
  if (c > 9) {
    c = 9;
  }
  (void) set_mp_short (t, (MP_T) c, 0, digits);
  (void) sub_mp (p, y, y, t, digits);
/* Reset the stack to prevent overflow, there may be many digits */
  stack_pointer = pop_sp;
  return (digchar (c));
}

/*!
\brief standard string for LONG
\param p position in tree
\param x mp digit
\param digits digits
\param width width
\param after after
\return same
**/

char *long_sub_fixed (NODE_T * p, MP_T * x, int digits, int width, int after)
{
  int strwid = 8 + width;
  char *str = stack_string (p, strwid);
  int before = 0, j, len, pop_sp = stack_pointer;
  BOOL_T overflow;
  MP_T *y;
  MP_T *s;
  MP_T *t;
  STACK_MP (y, p, digits);
  STACK_MP (s, p, digits);
  STACK_MP (t, p, digits);
  (void) set_mp_short (t, (MP_T) (MP_RADIX / 10), -1, digits);
  (void) pow_mp_int (p, t, t, after, digits);
  (void) div_mp_digit (p, t, t, (MP_T) 2, digits);
  (void) add_mp (p, y, x, t, digits);
  (void) set_mp_short (s, (MP_T) 1, 0, digits);
  while ((sub_mp (p, t, y, s, digits), MP_DIGIT (t, 1) >= 0)) {
    before++;
    (void) mul_mp_digit (p, s, s, (MP_T) 10, digits);
  }
  (void) div_mp (p, y, y, s, digits);
  str[0] = NULL_CHAR;
  len = 0;
  overflow = A68_FALSE;
  for (j = 0; j < before && !overflow; j++) {
    if (!(overflow = (BOOL_T) (len >= width))) {
      (void) string_plusab_char (str, long_choose_dig (p, y, digits), strwid);
      len++;
    }
  }
  if (after > 0 && !(overflow = (BOOL_T) (len >= width))) {
    (void) string_plusab_char (str, POINT_CHAR, strwid);
  }
  for (j = 0; j < after && !overflow; j++) {
    if (!(overflow = (BOOL_T) (len >= width))) {
      (void) string_plusab_char (str, long_choose_dig (p, y, digits), strwid);
      len++;
    }
  }
  if (overflow || (int) strlen (str) > width) {
    (void) error_chars (str, width);
  }
  stack_pointer = pop_sp;
  return (str);
}

/*!
\brief fetch next digit from REAL
\param y value
\return next digit
**/

static char choose_dig (double *y)
{
/* Assuming positive "y" */
  int c = (int) (*y *= 10);
  if (c > 9) {
    c = 9;
  }
  *y -= c;
  return (digchar (c));
}

/*!
\brief standard string for REAL
\param p position in tree
\param x value
\param width width
\param after after
\return string
**/

char *sub_fixed (NODE_T * p, double x, int width, int after)
{
  int strwid = 8 + width;
  char *str = stack_string (p, strwid);
  int before = 0, j, len, expo;
  BOOL_T overflow;
  double y, z;
/* Round and scale */
  z = y = x + 0.5 * ten_up (-after);
  expo = 0;
  while (z >= 1) {
    expo++;
    z /= 10;
  }
  before += expo;
/* Trick to avoid overflow */
  if (expo > 30) {
    expo -= 30;
    y /= ten_up (30);
  }
/* Scale number */
  y /= ten_up (expo);
  len = 0;
/* Put digits, prevent garbage from overstretching precision */
  overflow = A68_FALSE;
  for (j = 0; j < before && !overflow; j++) {
    if (!(overflow = (BOOL_T) (len >= width))) {
      char ch = (char) (len < REAL_WIDTH ? choose_dig (&y) : '0');
      (void) string_plusab_char (str, ch, strwid);
      len++;
    }
  }
  if (after > 0 && !(overflow = (BOOL_T) (len >= width))) {
    (void) string_plusab_char (str, POINT_CHAR, strwid);
  }
  for (j = 0; j < after && !overflow; j++) {
    if (!(overflow = (BOOL_T) (len >= width))) {
      char ch = (char) (len < REAL_WIDTH ? choose_dig (&y) : '0');
      (void) string_plusab_char (str, ch, strwid);
      len++;
    }
  }
  if (overflow || (int) strlen (str) > width) {
    (void) error_chars (str, width);
  }
  return (str);
}

/*!
\brief formatted string for NUMBER
\param p position in tree
\return string
**/

char *fixed (NODE_T * p)
{
  A68_INT width, after;
  MOID_T *mode;
  int pop_sp, arg_sp;
  POP_OBJECT (p, &after, A68_INT);
  POP_OBJECT (p, &width, A68_INT);
  arg_sp = stack_pointer;
  DECREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
  mode = (MOID_T *) (VALUE ((A68_UNION *) STACK_TOP));
  pop_sp = stack_pointer;
  if (mode == MODE (REAL)) {
    double x = VALUE ((A68_REAL *) (STACK_OFFSET (A68_UNION_SIZE)));
    int length = ABS (VALUE (&width)) - (x < 0 || VALUE (&width) > 0 ? 1 : 0);
    char *s;
    CHECK_REAL_REPRESENTATION (p, x);
    stack_pointer = arg_sp;
    if (VALUE (&after) >= 0 && (length > VALUE (&after) || VALUE (&width) == 0)) {
      double y = ABS (x), z0, z1;
      if (VALUE (&width) == 0) {
        length = (VALUE (&after) == 0 ? 1 : 0);
        z0 = ten_up (-VALUE (&after));
        z1 = ten_up (length);
        while (y + 0.5 * z0 > z1) {
          length++;
          z1 *= 10.0;
        }
        length += (VALUE (&after) == 0 ? 0 : VALUE (&after) + 1);
      }
      s = stack_string (p, 8 + length);
      s = sub_fixed (p, y, length, VALUE (&after));
      if (a68g_strchr (s, ERROR_CHAR) == NO_TEXT) {
        if (length > (int) strlen (s) && (s[0] != NULL_CHAR ? s[0] == POINT_CHAR : A68_TRUE) && y < 1.0) {
          (void) plusto ('0', s);
        }
        if (x < 0) {
          (void) plusto ('-', s);
        } else if (VALUE (&width) > 0) {
          (void) plusto ('+', s);
        }
        if (VALUE (&width) != 0) {
          (void) leading_spaces (s, ABS (VALUE (&width)));
        }
        return (s);
      } else if (VALUE (&after) > 0) {
        stack_pointer = arg_sp;
        PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
        PUSH_PRIMITIVE (p, VALUE (&after) - 1, A68_INT);
        return (fixed (p));
      } else {
        return (error_chars (s, VALUE (&width)));
      }
    } else {
      s = stack_string (p, 8 + ABS (VALUE (&width)));
      return (error_chars (s, VALUE (&width)));
    }
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (mode);
    int length;
    BOOL_T ltz;
    char *s;
    MP_T *x = (MP_T *) (STACK_OFFSET (A68_UNION_SIZE));
    stack_pointer = arg_sp;
    ltz = (BOOL_T) (MP_DIGIT (x, 1) < 0);
    MP_DIGIT (x, 1) = ABS (MP_DIGIT (x, 1));
    length = ABS (VALUE (&width)) - (ltz || VALUE (&width) > 0 ? 1 : 0);
    if (VALUE (&after) >= 0 && (length > VALUE (&after) || VALUE (&width) == 0)) {
      MP_T *z0;
      MP_T *z1;
      MP_T *t;
      STACK_MP (z0, p, digits);
      STACK_MP (z1, p, digits);
      STACK_MP (t, p, digits);
      if (VALUE (&width) == 0) {
        length = (VALUE (&after) == 0 ? 1 : 0);
        (void) set_mp_short (z0, (MP_T) (MP_RADIX / 10), -1, digits);
        (void) set_mp_short (z1, (MP_T) 10, 0, digits);
        (void) pow_mp_int (p, z0, z0, VALUE (&after), digits);
        (void) pow_mp_int (p, z1, z1, length, digits);
        while ((div_mp_digit (p, t, z0, (MP_T) 2, digits), add_mp (p, t, x, t, digits), sub_mp (p, t, t, z1, digits), MP_DIGIT (t, 1) > 0)) {
          length++;
          (void) mul_mp_digit (p, z1, z1, (MP_T) 10, digits);
        }
        length += (VALUE (&after) == 0 ? 0 : VALUE (&after) + 1);
      }
      s = stack_string (p, 8 + length);
      s = long_sub_fixed (p, x, digits, length, VALUE (&after));
      if (a68g_strchr (s, ERROR_CHAR) == NO_TEXT) {
        if (length > (int) strlen (s) && (s[0] != NULL_CHAR ? s[0] == POINT_CHAR : A68_TRUE) && (MP_EXPONENT (x) < 0 || MP_DIGIT (x, 1) == 0)) {
          (void) plusto ('0', s);
        }
        if (ltz) {
          (void) plusto ('-', s);
        } else if (VALUE (&width) > 0) {
          (void) plusto ('+', s);
        }
        if (VALUE (&width) != 0) {
          (void) leading_spaces (s, ABS (VALUE (&width)));
        }
        return (s);
      } else if (VALUE (&after) > 0) {
        stack_pointer = arg_sp;
        MP_DIGIT (x, 1) = ltz ? -ABS (MP_DIGIT (x, 1)) : ABS (MP_DIGIT (x, 1));
        PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
        PUSH_PRIMITIVE (p, VALUE (&after) - 1, A68_INT);
        return (fixed (p));
      } else {
        return (error_chars (s, VALUE (&width)));
      }
    } else {
      s = stack_string (p, 8 + ABS (VALUE (&width)));
      return (error_chars (s, VALUE (&width)));
    }
  } else if (mode == MODE (INT)) {
    int x = VALUE ((A68_INT *) (STACK_OFFSET (A68_UNION_SIZE)));
    PUSH_UNION (p, MODE (REAL));
    PUSH_PRIMITIVE (p, (double) x, A68_REAL);
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + ALIGNED_SIZE_OF (A68_REAL)));
    PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&after), A68_INT);
    return (fixed (p));
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    stack_pointer = pop_sp;
    if (mode == MODE (LONG_INT)) {
      VALUE ((A68_UNION *) STACK_TOP) = (void *) MODE (LONG_REAL);
    } else {
      VALUE ((A68_UNION *) STACK_TOP) = (void *) MODE (LONGLONG_REAL);
    } INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
    PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&after), A68_INT);
    return (fixed (p));
  }
  return (NO_TEXT);
}

/*!
\brief scale LONG for formatting
\param p position in tree
\param y mp number
\param digits digits
\param before before
\param after after
\param q int multiplier
**/

void long_standardise (NODE_T * p, MP_T * y, int digits, int before, int after, int *q)
{
  int j, pop_sp = stack_pointer;
  MP_T *f;
  MP_T *g;
  MP_T *h;
  MP_T *t;
  STACK_MP (f, p, digits);
  STACK_MP (g, p, digits);
  STACK_MP (h, p, digits);
  STACK_MP (t, p, digits);
  (void) set_mp_short (g, (MP_T) 1, 0, digits);
  for (j = 0; j < before; j++) {
    (void) mul_mp_digit (p, g, g, (MP_T) 10, digits);
  }
  (void) div_mp_digit (p, h, g, (MP_T) 10, digits);
/* Speed huge exponents */
  if ((MP_EXPONENT (y) - MP_EXPONENT (g)) > 1) {
    (*q) += LOG_MP_BASE * ((int) MP_EXPONENT (y) - (int) MP_EXPONENT (g) - 1);
    MP_EXPONENT (y) = MP_EXPONENT (g) + 1;
  }
  while ((sub_mp (p, t, y, g, digits), MP_DIGIT (t, 1) >= 0)) {
    (void) div_mp_digit (p, y, y, (MP_T) 10, digits);
    (*q)++;
  }
  if (MP_DIGIT (y, 1) != 0) {
/* Speed huge exponents */
    if ((MP_EXPONENT (y) - MP_EXPONENT (h)) < -1) {
      (*q) -= LOG_MP_BASE * ((int) MP_EXPONENT (h) - (int) MP_EXPONENT (y) - 1);
      MP_EXPONENT (y) = MP_EXPONENT (h) - 1;
    }
    while ((sub_mp (p, t, y, h, digits), MP_DIGIT (t, 1) < 0)) {
      (void) mul_mp_digit (p, y, y, (MP_T) 10, digits);
      (*q)--;
    }
  }
  (void) set_mp_short (f, (MP_T) 1, 0, digits);
  for (j = 0; j < after; j++) {
    (void) div_mp_digit (p, f, f, (MP_T) 10, digits);
  }
  (void) div_mp_digit (p, t, f, (MP_T) 2, digits);
  (void) add_mp (p, t, y, t, digits);
  (void) sub_mp (p, t, t, g, digits);
  if (MP_DIGIT (t, 1) >= 0) {
    MOVE_MP (y, h, digits);
    (*q)++;
  }
  stack_pointer = pop_sp;
}

/*!
\brief scale REAL for formatting
\param y value
\param before before
\param after after
\param p int multiplier
**/

void standardise (double *y, int before, int after, int *p)
{
  int j;
  double f, g = 1.0, h;
  for (j = 0; j < before; j++) {
    g *= 10.0;
  }
  h = g / 10.0;
  while (*y >= g) {
    *y *= 0.1;
    (*p)++;
  }
  if (*y != 0.0) {
    while (*y < h) {
      *y *= 10.0;
      (*p)--;
    }
  }
  f = 1.0;
  for (j = 0; j < after; j++) {
    f *= 0.1;
  }
  if (*y + 0.5 * f >= g) {
    *y = h;
    (*p)++;
  }
}

/*!
\brief formatted string for NUMBER
\param p position in tree
\return string
**/

char *real (NODE_T * p)
{
  int pop_sp, arg_sp;
  A68_INT width, after, expo, frmt;
  MOID_T *mode;
/* POP arguments */
  POP_OBJECT (p, &frmt, A68_INT);
  POP_OBJECT (p, &expo, A68_INT);
  POP_OBJECT (p, &after, A68_INT);
  POP_OBJECT (p, &width, A68_INT);
  arg_sp = stack_pointer;
  DECREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
  mode = (MOID_T *) (VALUE ((A68_UNION *) STACK_TOP));
  pop_sp = stack_pointer;
  if (mode == MODE (REAL)) {
    double x = VALUE ((A68_REAL *) (STACK_OFFSET (A68_UNION_SIZE)));
    int before = ABS (VALUE (&width)) - ABS (VALUE (&expo)) - (VALUE (&after) != 0 ? VALUE (&after) + 1 : 0) - 2;
    CHECK_REAL_REPRESENTATION (p, x);
    stack_pointer = arg_sp;
#if defined HAVE_IEEE_754
    if (NOT_A_REAL (x)) {
      char *s = stack_string (p, 8 + ABS (VALUE (&width)));
      return (error_chars (s, VALUE (&width)));
    }
#endif /*  */
    if (SIGN (before) + SIGN (VALUE (&after)) > 0) {
      int strwid;
      char *s, *t1, *t2;
      double y = ABS (x);
      int q = 0;
      standardise (&y, before, VALUE (&after), &q);
      if (VALUE (&frmt) > 0) {
        while (q % VALUE (&frmt) != 0) {
          y *= 10;
          q--;
          if (VALUE (&after) > 0) {
            VALUE (&after)--;
          }
        }
      } else {
        double upb = ten_up (-VALUE (&frmt)), lwb = ten_up (-VALUE (&frmt) - 1);
        while (y < lwb) {
          y *= 10;
          q--;
          if (VALUE (&after) > 0) {
            VALUE (&after)--;
          }
        }
        while (y > upb) {
          y /= 10;
          q++;
          if (VALUE (&after) > 0) {
            VALUE (&after)++;
          }
        }
      }
      PUSH_UNION (p, MODE (REAL));
      PUSH_PRIMITIVE (p, SIGN (x) * y, A68_REAL);
      INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + ALIGNED_SIZE_OF (A68_REAL)));
      PUSH_PRIMITIVE (p, SIGN (VALUE (&width)) * (ABS (VALUE (&width)) - ABS (VALUE (&expo)) - 1), A68_INT);
      PUSH_PRIMITIVE (p, VALUE (&after), A68_INT);
      t1 = fixed (p);
      PUSH_UNION (p, MODE (INT));
      PUSH_PRIMITIVE (p, q, A68_INT);
      INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + ALIGNED_SIZE_OF (A68_INT)));
      PUSH_PRIMITIVE (p, VALUE (&expo), A68_INT);
      t2 = whole (p);
      strwid = 8 + (int) strlen (t1) + 1 + (int) strlen (t2);
      s = stack_string (p, strwid);
      bufcpy (s, t1, strwid);
      (void) string_plusab_char (s, EXPONENT_CHAR, strwid);
      bufcat (s, t2, strwid);
      if (VALUE (&expo) == 0 || a68g_strchr (s, ERROR_CHAR) != NO_TEXT) {
        stack_pointer = arg_sp;
        PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
        PUSH_PRIMITIVE (p, VALUE (&after) != 0 ? VALUE (&after) - 1 : 0, A68_INT);
        PUSH_PRIMITIVE (p, VALUE (&expo) > 0 ? VALUE (&expo) + 1 : VALUE (&expo) - 1, A68_INT);
        PUSH_PRIMITIVE (p, VALUE (&frmt), A68_INT);
        return (real (p));
      } else {
        return (s);
      }
    } else {
      char *s = stack_string (p, 8 + ABS (VALUE (&width)));
      return (error_chars (s, VALUE (&width)));
    }
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (mode);
    int before;
    MP_T *x = (MP_T *) (STACK_OFFSET (A68_UNION_SIZE));
    BOOL_T ltz = (BOOL_T) (MP_DIGIT (x, 1) < 0);
    stack_pointer = arg_sp;
    MP_DIGIT (x, 1) = ABS (MP_DIGIT (x, 1));
    before = ABS (VALUE (&width)) - ABS (VALUE (&expo)) - (VALUE (&after) != 0 ? VALUE (&after) + 1 : 0) - 2;
    if (SIGN (before) + SIGN (VALUE (&after)) > 0) {
      int strwid;
      char *s, *t1, *t2;
      MP_T *z;
      int q = 0;
      STACK_MP (z, p, digits);
      MOVE_MP (z, x, digits);
      long_standardise (p, z, digits, before, VALUE (&after), &q);
      if (VALUE (&frmt) > 0) {
        while (q % VALUE (&frmt) != 0) {
          (void) mul_mp_digit (p, z, z, (MP_T) 10, digits);
          q--;
          if (VALUE (&after) > 0) {
            VALUE (&after)--;
          }
        }
      } else {
        MP_T *dif, *lim;
        ADDR_T sp1 = stack_pointer;
        STACK_MP (dif, p, digits);
        STACK_MP (lim, p, digits);
        (void) mp_ten_up (p, lim, -VALUE (&frmt) - 1, digits);
        (void) sub_mp (p, dif, z, lim, digits);
        while (MP_DIGIT (dif, 1) < 0) {
          (void) mul_mp_digit (p, z, z, (MP_T) 10, digits);
          q--;
          if (VALUE (&after) > 0) {
            VALUE (&after)--;
          }
          (void) sub_mp (p, dif, z, lim, digits);
        }
        (void) mul_mp_digit (p, lim, lim, (MP_T) 10, digits);
        (void) sub_mp (p, dif, z, lim, digits);
        while (MP_DIGIT (dif, 1) > 0) {
          (void) div_mp_digit (p, z, z, (MP_T) 10, digits);
          q++;
          if (VALUE (&after) > 0) {
            VALUE (&after)++;
          }
          (void) sub_mp (p, dif, z, lim, digits);
        }
        stack_pointer = sp1;
      }
      PUSH_UNION (p, mode);
      MP_DIGIT (z, 1) = (ltz ? -MP_DIGIT (z, 1) : MP_DIGIT (z, 1));
      PUSH (p, z, SIZE_MP (digits));
      INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + SIZE_MP (digits)));
      PUSH_PRIMITIVE (p, SIGN (VALUE (&width)) * (ABS (VALUE (&width)) - ABS (VALUE (&expo)) - 1), A68_INT);
      PUSH_PRIMITIVE (p, VALUE (&after), A68_INT);
      t1 = fixed (p);
      PUSH_UNION (p, MODE (INT));
      PUSH_PRIMITIVE (p, q, A68_INT);
      INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + ALIGNED_SIZE_OF (A68_INT)));
      PUSH_PRIMITIVE (p, VALUE (&expo), A68_INT);
      t2 = whole (p);
      strwid = 8 + (int) strlen (t1) + 1 + (int) strlen (t2);
      s = stack_string (p, strwid);
      bufcpy (s, t1, strwid);
      (void) string_plusab_char (s, EXPONENT_CHAR, strwid);
      bufcat (s, t2, strwid);
      if (VALUE (&expo) == 0 || a68g_strchr (s, ERROR_CHAR) != NO_TEXT) {
        stack_pointer = arg_sp;
        PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
        PUSH_PRIMITIVE (p, VALUE (&after) != 0 ? VALUE (&after) - 1 : 0, A68_INT);
        PUSH_PRIMITIVE (p, VALUE (&expo) > 0 ? VALUE (&expo) + 1 : VALUE (&expo) - 1, A68_INT);
        PUSH_PRIMITIVE (p, VALUE (&frmt), A68_INT);
        return (real (p));
      } else {
        return (s);
      }
    } else {
      char *s = stack_string (p, 8 + ABS (VALUE (&width)));
      return (error_chars (s, VALUE (&width)));
    }
  } else if (mode == MODE (INT)) {
    int x = VALUE ((A68_INT *) (STACK_OFFSET (A68_UNION_SIZE)));
    PUSH_UNION (p, MODE (REAL));
    PUSH_PRIMITIVE (p, (double) x, A68_REAL);
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (A68_UNION_SIZE + ALIGNED_SIZE_OF (A68_REAL)));
    PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&after), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&expo), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&frmt), A68_INT);
    return (real (p));
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    stack_pointer = pop_sp;
    if (mode == MODE (LONG_INT)) {
      VALUE ((A68_UNION *) STACK_TOP) = (void *) MODE (LONG_REAL);
    } else {
      VALUE ((A68_UNION *) STACK_TOP) = (void *) MODE (LONGLONG_REAL);
    } INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
    PUSH_PRIMITIVE (p, VALUE (&width), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&after), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&expo), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&frmt), A68_INT);
    return (real (p));
  }
  return (NO_TEXT);
}

/*!
\brief PROC (NUMBER, INT) STRING whole
\param p position in tree
**/

void genie_whole (NODE_T * p)
{
  int pop_sp = stack_pointer;
  A68_REF ref;
  char *str = whole (p);
  stack_pointer = pop_sp - ALIGNED_SIZE_OF (A68_INT) - MOID_SIZE (MODE (NUMBER));
  ref = tmp_to_a68_string (p, str);
  PUSH_REF (p, ref);
}

/*!
\brief PROC (NUMBER, INT, INT) STRING fixed
\param p position in tree
**/

void genie_fixed (NODE_T * p)
{
  int pop_sp = stack_pointer;
  A68_REF ref;
  char *str = fixed (p);
  stack_pointer = pop_sp - 2 * ALIGNED_SIZE_OF (A68_INT) - MOID_SIZE (MODE (NUMBER));
  ref = tmp_to_a68_string (p, str);
  PUSH_REF (p, ref);
}

/*!
\brief PROC (NUMBER, INT, INT, INT) STRING eng
\param p position in tree
**/

void genie_real (NODE_T * p)
{
  int pop_sp = stack_pointer;
  A68_REF ref;
  char *str = real (p);
  stack_pointer = pop_sp - 4 * ALIGNED_SIZE_OF (A68_INT) - MOID_SIZE (MODE (NUMBER));
  ref = tmp_to_a68_string (p, str);
  PUSH_REF (p, ref);
}

/*!
\brief PROC (NUMBER, INT, INT, INT) STRING float
\param p position in tree
**/

void genie_float (NODE_T * p)
{
  PUSH_PRIMITIVE (p, 1, A68_INT);
  genie_real (p);
}

/* ALGOL68C routines */

/*!
\brief PROC INT read int
\param p position in tree
**/

void genie_read_int (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (INT), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief PROC LONG INT read long int
\param p position in tree
**/

void genie_read_long_int (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (LONG_INT), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, get_mp_size (MODE (LONG_INT)));
}

/*!
\brief PROC LONG LONG INT read long long int
\param p position in tree
**/

void genie_read_longlong_int (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (LONGLONG_INT), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, get_mp_size (MODE (LONGLONG_INT)));
}

/*!
\brief PROC REAL read real
\param p position in tree
**/

void genie_read_real (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (REAL), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REAL));
}

/*!
\brief PROC LONG REAL read long real
\param p position in tree
**/

void genie_read_long_real (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (LONG_REAL), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, get_mp_size (MODE (LONG_REAL)));
}

/*!
\brief PROC LONG LONG REAL read long long real
\param p position in tree
**/

void genie_read_longlong_real (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (LONGLONG_REAL), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, get_mp_size (MODE (LONGLONG_REAL)));
}

/*!
\brief PROC COMPLEX read complex
\param p position in tree
**/

void genie_read_complex (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_real (p);
  genie_read_real (p);
}

/*!
\brief PROC LONG COMPLEX read long complex
\param p position in tree
**/

void genie_read_long_complex (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_long_real (p);
  genie_read_long_real (p);
}

/*!
\brief PROC LONG LONG COMPLEX read long long complex
\param p position in tree
**/

void genie_read_longlong_complex (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_longlong_real (p);
  genie_read_longlong_real (p);
}

/*!
\brief PROC BOOL read bool
\param p position in tree
**/

void genie_read_bool (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (BOOL), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_BOOL));
}

/*!
\brief PROC BITS read bits
\param p position in tree
**/

void genie_read_bits (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (BITS), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_BITS));
}

/*!
\brief PROC LONG BITS read long bits
\param p position in tree
**/

void genie_read_long_bits (NODE_T * p)
{
  MP_T *z;
  STACK_MP (z, p, get_mp_digits (MODE (LONG_BITS)));
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (LONG_BITS), (BYTE_T *) z, stand_in);
}

/*!
\brief PROC LONG LONG BITS read long long bits
\param p position in tree
**/

void genie_read_longlong_bits (NODE_T * p)
{
  MP_T *z;
  STACK_MP (z, p, get_mp_digits (MODE (LONGLONG_BITS)));
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (LONGLONG_BITS), (BYTE_T *) z, stand_in);
}

/*!
\brief PROC CHAR read char
\param p position in tree
**/

void genie_read_char (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (CHAR), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_CHAR));
}

/*!
\brief PROC STRING read string
\param p position in tree
**/

void genie_read_string (NODE_T * p)
{
  open_for_reading (p, stand_in);
  genie_read_standard (p, MODE (STRING), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, A68_REF_SIZE);
}

/*!
\brief PROC STRING read line
\param p position in tree
**/

void genie_read_line (NODE_T * p)
{
#if defined HAVE_READLINE
  char * line = readline (""); 
  if (line != NO_TEXT && (int) strlen (line) > 0) {
    add_history (line);
  }
  PUSH_REF (p, c_to_a_string (p, line, DEFAULT_WIDTH));
  free (line);
#else 
  genie_read_string (p);
  genie_stand_in (p);
  genie_new_line (p);
#endif
}

/*!
\brief PROC (INT) VOID print int
\param p position in tree
**/

void genie_print_int (NODE_T * p)
{
  int size = MOID_SIZE (MODE (INT));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (INT), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG INT) VOID print long int
\param p position in tree
**/

void genie_print_long_int (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONG_INT));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (LONG_INT), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG LONG INT) VOID print long long int
\param p position in tree
**/

void genie_print_longlong_int (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONGLONG_INT));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (LONGLONG_INT), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (REAL) VOID print real
\param p position in tree
**/

void genie_print_real (NODE_T * p)
{
  int size = MOID_SIZE (MODE (REAL));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (REAL), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG REAL) VOID print long real
\param p position in tree
**/

void genie_print_long_real (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONG_REAL));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (LONG_REAL), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG LONG REAL) VOID print long long real
\param p position in tree
**/

void genie_print_longlong_real (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONGLONG_REAL));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (LONGLONG_REAL), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (COMPLEX) VOID print complex
\param p position in tree
**/

void genie_print_complex (NODE_T * p)
{
  int size = MOID_SIZE (MODE (COMPLEX));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (COMPLEX), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG COMPLEX) VOID print long complex
\param p position in tree
**/

void genie_print_long_complex (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONG_COMPLEX));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (LONG_COMPLEX), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG LONG COMPLEX) VOID print long long complex
\param p position in tree
**/

void genie_print_longlong_complex (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONGLONG_COMPLEX));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (LONGLONG_COMPLEX), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (CHAR) VOID print char
\param p position in tree
**/

void genie_print_char (NODE_T * p)
{
  int size = MOID_SIZE (MODE (CHAR));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (CHAR), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (BITS) VOID print bits
\param p position in tree
**/

void genie_print_bits (NODE_T * p)
{
  int size = MOID_SIZE (MODE (BITS));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (BITS), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG BITS) VOID print long bits
\param p position in tree
**/

void genie_print_long_bits (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONG_BITS));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (LONG_BITS), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG LONG BITS) VOID print long long bits
\param p position in tree
**/

void genie_print_longlong_bits (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONGLONG_BITS));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (LONGLONG_BITS), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (BOOL) VOID print bool
\param p position in tree
**/

void genie_print_bool (NODE_T * p)
{
  int size = MOID_SIZE (MODE (BOOL));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  genie_write_standard (p, MODE (BOOL), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (STRING) VOID print string
\param p position in tree
**/

void genie_print_string (NODE_T * p)
{
  reset_transput_buffer (UNFORMATTED_BUFFER);
  add_string_from_stack_transput_buffer (p, UNFORMATTED_BUFFER);
  open_for_writing (p, stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
}

/*
Transput library - Formatted transput
In Algol68G, a value of mode FORMAT looks like a routine text. The value
comprises a pointer to its environment in the stack, and a pointer where the
format text is at in the syntax tree.
*/

#define INT_DIGITS "0123456789"
#define BITS_DIGITS "0123456789abcdefABCDEF"
#define INT_DIGITS_BLANK " 0123456789"
#define BITS_DIGITS_BLANK " 0123456789abcdefABCDEF"
#define SIGN_DIGITS " +-"

/*!
\brief handle format error event
\param p position in tree
\param ref_file fat pointer to A68 file
**/

void format_error (NODE_T * p, A68_REF ref_file, char *diag)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  A68_BOOL z;
  on_event_handler (p, FORMAT_ERROR_MENDED (f), ref_file);
  POP_OBJECT (p, &z, A68_BOOL);
  if (VALUE (&z) == A68_FALSE) {
    diagnostic_node (A68_RUNTIME_ERROR, p, diag);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief initialise processing of pictures
\param p position in tree
**/

static void initialise_collitems (NODE_T * p)
{

/*
Every picture has a counter that says whether it has not been used OR the number
of times it can still be used.
*/

  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PICTURE)) {
      A68_COLLITEM *z = (A68_COLLITEM *) FRAME_LOCAL (frame_pointer, OFFSET (TAX (p)));
      STATUS (z) = INIT_MASK;
      COUNT (z) = ITEM_NOT_USED;
    }
/* Don't dive into f, g, n frames and collections */
    if (!(IS (p, ENCLOSED_CLAUSE) || IS (p, COLLECTION))) {
      initialise_collitems (SUB (p));
    }
  }
}

/*!
\brief initialise processing of format text
\param file file
\param fmt format
\param embedded whether embedded format
\param init whether to initialise collitems
**/

static void open_format_frame (NODE_T * p, A68_REF ref_file, A68_FORMAT * fmt, BOOL_T embedded, BOOL_T init)
{
/* Open a new frame for the format text and save for return to embedding one */
  A68_FILE *file = FILE_DEREF (&ref_file);
  NODE_T *dollar;
  A68_FORMAT *save;
/* Integrity check */
  if ((STATUS (fmt) & SKIP_FORMAT_MASK) || (BODY (fmt) == NO_NODE)) {
    format_error (p, ref_file, ERROR_FORMAT_UNDEFINED);
  }
/* Ok, seems usable */
  dollar = SUB (BODY (fmt));
  OPEN_PROC_FRAME (dollar, ENVIRON (fmt));
  INIT_STATIC_FRAME (dollar);
/* Save old format */
  save = (A68_FORMAT *) FRAME_LOCAL (frame_pointer, OFFSET (TAX (dollar)));
  *save = (embedded == EMBEDDED_FORMAT ? FORMAT (file) : nil_format);
  FORMAT (file) = *fmt;
/* Reset all collitems */
  if (init) {
    initialise_collitems (dollar);
  }
}

/*!
\brief handle end-of-format event
\param p position in tree
\param ref_file fat pointer to A68 file
\return whether format is embedded
**/

int end_of_format (NODE_T * p, A68_REF ref_file)
{
/*
Format-items return immediately to the embedding format text. The outermost
format text calls "on format end".
*/
  A68_FILE *file = FILE_DEREF (&ref_file);
  NODE_T *dollar = SUB (BODY (&FORMAT (file)));
  A68_FORMAT *save = (A68_FORMAT *) FRAME_LOCAL (frame_pointer, OFFSET (TAX (dollar)));
  if (IS_NIL_FORMAT (save)) {
/* Not embedded, outermost format: execute event routine */
    A68_BOOL z;
    on_event_handler (p, FORMAT_END_MENDED (FILE_DEREF (&ref_file)), ref_file);
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
/* Restart format */
      frame_pointer = FRAME_POINTER (file);
      stack_pointer = STACK_POINTER (file);
      open_format_frame (p, ref_file, &FORMAT (file), NOT_EMBEDDED_FORMAT, A68_TRUE);
    }
    return (NOT_EMBEDDED_FORMAT);
  } else {
/* Embedded format, return to embedding format, cf. RR */
    CLOSE_FRAME;
    FORMAT (file) = *save;
    return (EMBEDDED_FORMAT);
  }
}

/*!
\brief return integral value of replicator
\param p position in tree
\return same
**/

int get_replicator_value (NODE_T * p, BOOL_T check)
{
  int z = 0;
  if (IS (p, STATIC_REPLICATOR)) {
    A68_INT u;
    if (genie_string_to_value_internal (p, MODE (INT), NSYMBOL (p), (BYTE_T *) & u) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, MODE (INT));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z = VALUE (&u);
  } else if (IS (p, DYNAMIC_REPLICATOR)) {
    A68_INT u;
    EXECUTE_UNIT (NEXT_SUB (p));
    POP_OBJECT (p, &u, A68_INT);
    z = VALUE (&u);
  } else if (IS (p, REPLICATOR)) {
    z = get_replicator_value (SUB (p), check);
  }
  if (check && z < 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FORMAT_INVALID_REPLICATOR);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  return (z);
}

/*!
\brief return first available pattern
\param p position in tree
\param ref_file fat pointer to A68 file
\return same
**/

static NODE_T *scan_format_pattern (NODE_T * p, A68_REF ref_file)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PICTURE_LIST)) {
      NODE_T *prio = scan_format_pattern (SUB (p), ref_file);
      if (prio != NO_NODE) {
        return (prio);
      }
    }
    if (IS (p, PICTURE)) {
      NODE_T *picture = SUB (p);
      A68_COLLITEM *collitem = (A68_COLLITEM *) FRAME_LOCAL (frame_pointer, OFFSET (TAX (p)));
      if (COUNT (collitem) != 0) {
        if (IS (picture, A68_PATTERN)) {
          COUNT (collitem) = 0;  /* This pattern is now done */
          picture = SUB (picture);
          if (ATTRIBUTE (picture) != FORMAT_PATTERN) {
            return (picture);
          } else {
            NODE_T *pat;
            A68_FORMAT z;
            A68_FILE *file = FILE_DEREF (&ref_file);
            EXECUTE_UNIT (NEXT_SUB (picture));
            POP_OBJECT (p, &z, A68_FORMAT);
            open_format_frame (p, ref_file, &z, EMBEDDED_FORMAT, A68_TRUE);
            pat = scan_format_pattern (SUB (BODY (&FORMAT (file))), ref_file);
            if (pat != NO_NODE) {
              return (pat);
            } else {
              (void) end_of_format (p, ref_file);
            }
          }
        } else if (IS (picture, INSERTION)) {
          A68_FILE *file = FILE_DEREF (&ref_file);
          if (READ_MOOD (file)) {
            read_insertion (picture, ref_file);
          } else if (WRITE_MOOD (file)) {
            write_insertion (picture, ref_file, INSERTION_NORMAL);
          } else {
            ABEND (A68_TRUE, "undetermined mood for insertion", NO_TEXT);
          }
          COUNT (collitem) = 0;  /* This insertion is now done */
        } else if (IS (picture, REPLICATOR) || IS (picture, COLLECTION)) {
          BOOL_T go_on = A68_TRUE;
          NODE_T *a68g_select = NO_NODE;
          if (COUNT (collitem) == ITEM_NOT_USED) {
            if (IS (picture, REPLICATOR)) {
              COUNT (collitem) = get_replicator_value (SUB (p), A68_TRUE);
              go_on = (BOOL_T) (COUNT (collitem) > 0);
              FORWARD (picture);
            } else {
              COUNT (collitem) = 1;
            }
            initialise_collitems (NEXT_SUB (picture));
          } else if (IS (picture, REPLICATOR)) {
            FORWARD (picture);
          }
          while (go_on) {
/* Get format item from collection. If collection is done, but repitition is not,
   then re-initialise the collection and repeat */
            a68g_select = scan_format_pattern (NEXT_SUB (picture), ref_file);
            if (a68g_select != NO_NODE) {
              return (a68g_select);
            } else {
              COUNT (collitem)--;
              go_on = (BOOL_T) (COUNT (collitem) > 0);
              if (go_on) {
                initialise_collitems (NEXT_SUB (picture));
              }
            }
          }
        }
      }
    }
  }
  return (NO_NODE);
}

/*!
\brief return first available pattern
\param p position in tree
\param ref_file fat pointer to A68 file
\param mood mode of operation
\return same
**/

NODE_T *get_next_format_pattern (NODE_T * p, A68_REF ref_file, BOOL_T mood)
{
/*
"mood" can be WANT_PATTERN: pattern needed by caller, so perform end-of-format
if needed or SKIP_PATTERN: just emptying current pattern/collection/format.
*/
  A68_FILE *file = FILE_DEREF (&ref_file);
  if (BODY (&FORMAT (file)) == NO_NODE) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FORMAT_EXHAUSTED);
    exit_genie (p, A68_RUNTIME_ERROR);
    return (NO_NODE);
  } else {
    NODE_T *pat = scan_format_pattern (SUB (BODY (&FORMAT (file))), ref_file);
    if (pat == NO_NODE) {
      if (mood == WANT_PATTERN) {
        int z;
        do {
          z = end_of_format (p, ref_file);
          pat = scan_format_pattern (SUB (BODY (&FORMAT (file))), ref_file);
        } while (z == EMBEDDED_FORMAT && pat == NO_NODE);
        if (pat == NO_NODE) {
          diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FORMAT_EXHAUSTED);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
    }
    return (pat);
  }
}

/*!
\brief diagnostic_node in case mode does not match picture
\param p position in tree
\param mode mode of object read or written
\param att attribute
**/

void pattern_error (NODE_T * p, MOID_T * mode, int att)
{
  diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FORMAT_CANNOT_TRANSPUT, mode, att);
  exit_genie (p, A68_RUNTIME_ERROR);
}

/*!
\brief unite value at top of stack to NUMBER
\param p position in tree
\param mode mode of value
\param item pointer to value
**/

static void unite_to_number (NODE_T * p, MOID_T * mode, BYTE_T * item)
{
  ADDR_T sp = stack_pointer;
  PUSH_UNION (p, mode);
  PUSH (p, item, (int) MOID_SIZE (mode));
  stack_pointer = sp + MOID_SIZE (MODE (NUMBER));
}

/*!
\brief write a group of insertions
\param p position in tree
\param ref_file fat pointer to A68 file
\param mood mode of operation in case of error
**/

void write_insertion (NODE_T * p, A68_REF ref_file, unsigned mood)
{
  for (; p != NO_NODE; FORWARD (p)) {
    write_insertion (SUB (p), ref_file, mood);
    if (IS (p, FORMAT_ITEM_L)) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, NEWLINE_CHAR);
      write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
    } else if (IS (p, FORMAT_ITEM_P)) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, FORMFEED_CHAR);
      write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
    } else if (IS (p, FORMAT_ITEM_X) || IS (p, FORMAT_ITEM_Q)) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
    } else if (IS (p, FORMAT_ITEM_Y)) {
      PUSH_REF (p, ref_file);
      PUSH_PRIMITIVE (p, -1, A68_INT);
      genie_set (p);
    } else if (IS (p, LITERAL)) {
      if (mood & INSERTION_NORMAL) {
        add_string_transput_buffer (p, FORMATTED_BUFFER, NSYMBOL (p));
      } else if (mood & INSERTION_BLANK) {
        int j, k = (int) strlen (NSYMBOL (p));
        for (j = 1; j <= k; j++) {
          add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
        }
      }
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      if (ATTRIBUTE (SUB_NEXT (p)) != FORMAT_ITEM_K) {
        for (j = 1; j <= k; j++) {
          write_insertion (NEXT (p), ref_file, mood);
        }
      } else {
        int pos = get_transput_buffer_index (FORMATTED_BUFFER);
        for (j = 1; j < (k - pos); j++) {
          add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
        }
      }
      return;
    }
  }
}

/*!
\brief convert to other radix, binary up to hexadecimal
\param p position in tree
\param z value to convert
\param radix radix
\param width width of converted number
\return whether conversion is successful
**/

static BOOL_T convert_radix (NODE_T * p, unsigned z, int radix, int width)
{
  static char *images = "0123456789abcdef";
  if (width > 0 && (radix >= 2 && radix <= 16)) {
    int digit = (int) (z % (unsigned) radix);
    BOOL_T success = convert_radix (p, z / (unsigned) radix, radix, width - 1);
    add_char_transput_buffer (p, EDIT_BUFFER, images[digit]);
    return (success);
  } else {
    return ((BOOL_T) (z == 0));
  }
}

/*!
\brief convert to other radix, binary up to hexadecimal
\param p position in tree
\param u mp number
\param radix radix
\param width width of converted number
\param m mode of 'u'
\param v work mp number
\param w work mp number
\return whether conversion is successful
**/

static BOOL_T convert_radix_mp (NODE_T * p, MP_T * u, int radix, int width, MOID_T * m, MP_T * v, MP_T * w)
{
  static char *images = "0123456789abcdef";
  if (width > 0 && (radix >= 2 && radix <= 16)) {
    int digit, digits = get_mp_digits (m);
    BOOL_T success;
    MOVE_MP (w, u, digits);
    (void) over_mp_digit (p, u, u, (MP_T) radix, digits);
    (void) mul_mp_digit (p, v, u, (MP_T) radix, digits);
    (void) sub_mp (p, v, w, v, digits);
    digit = (int) MP_DIGIT (v, 1);
    success = convert_radix_mp (p, u, radix, width - 1, m, v, w);
    add_char_transput_buffer (p, EDIT_BUFFER, images[digit]);
    return (success);
  } else {
    return ((BOOL_T) (MP_DIGIT (u, 1) == 0));
  }
}

/*!
\brief write string to file following current format
\param p position in tree
\param mode mode of value
\param ref_file fat pointer to A68 file
\param str string to write
**/

static void write_string_pattern (NODE_T * p, MOID_T * mode, A68_REF ref_file, char **str)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      write_insertion (SUB (p), ref_file, INSERTION_NORMAL);
    } else if (IS (p, FORMAT_ITEM_A)) {
      if ((*str)[0] != NULL_CHAR) {
        add_char_transput_buffer (p, FORMATTED_BUFFER, (*str)[0]);
        (*str)++;
      } else {
        value_error (p, mode, ref_file);
      }
    } else if (IS (p, FORMAT_ITEM_S)) {
      if ((*str)[0] != NULL_CHAR) {
        (*str)++;
      } else {
        value_error (p, mode, ref_file);
      }
      return;
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        write_string_pattern (NEXT (p), mode, ref_file, str);
      }
      return;
    } else {
      write_string_pattern (SUB (p), mode, ref_file, str);
    }
  }
}

/*!
\brief scan c_pattern
\param p position in tree
\param str string to write
**/

void scan_c_pattern (NODE_T * p, BOOL_T * right_align, BOOL_T * sign, int *width, int *after, int *letter)
{
  if (IS (p, FORMAT_ITEM_ESCAPE)) {
    FORWARD (p);
  }
  if (IS (p, FORMAT_ITEM_MINUS)) {
    *right_align = A68_FALSE;
    FORWARD (p);
  } else {
    *right_align = A68_TRUE;
  }
  if (IS (p, FORMAT_ITEM_PLUS)) {
    *sign = A68_TRUE;
    FORWARD (p);
  } else {
    *sign = A68_FALSE;
  }
  if (IS (p, REPLICATOR)) {
    *width = get_replicator_value (SUB (p), A68_TRUE);
    FORWARD (p);
  }
  if (IS (p, FORMAT_ITEM_POINT)) {
    FORWARD (p);
  }
  if (IS (p, REPLICATOR)) {
    *after = get_replicator_value (SUB (p), A68_TRUE);
    FORWARD (p);
  }
  *letter = ATTRIBUTE (p);
}

/*!
\brief write appropriate insertion from a choice pattern
\param p position in tree
\param ref_file fat pointer to A68 file
\param count count to reach
**/

static void write_choice_pattern (NODE_T * p, A68_REF ref_file, int *count)
{
  for (; p != NO_NODE; FORWARD (p)) {
    write_choice_pattern (SUB (p), ref_file, count);
    if (IS (p, PICTURE)) {
      (*count)--;
      if (*count == 0) {
        write_insertion (SUB (p), ref_file, INSERTION_NORMAL);
      }
    }
  }
}

/*!
\brief write appropriate insertion from a boolean pattern
\param p position in tree
\param ref_file fat pointer to A68 file
\param z BOOL value
**/

static void write_boolean_pattern (NODE_T * p, A68_REF ref_file, BOOL_T z)
{
  int k = (z ? 1 : 2);
  write_choice_pattern (p, ref_file, &k);
}

/*!
\brief write value according to a general pattern
\param p position in tree
\param mode mode of value
\param item pointer to value
\param mod format modifier
**/

static void write_number_generic (NODE_T * p, MOID_T * mode, BYTE_T * item, int mod)
{
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int size;
/* Push arguments */
  unite_to_number (p, mode, item);
  EXECUTE_UNIT (NEXT_SUB (p));
  POP_REF (p, &row);
  GET_DESCRIPTOR (arr, tup, &row);
  size = ROW_SIZE (tup);
  if (size > 0) {
    int i;
    BYTE_T *base_address = DEREF (BYTE_T, &ARRAY (arr));
    for (i = LWB (tup); i <= UPB (tup); i++) {
      int addr = INDEX_1_DIM (arr, tup, i);
      int arg = VALUE ((A68_INT *) & (base_address[addr]));
      PUSH_PRIMITIVE (p, arg, A68_INT);
    }
  }
/* Make a string */
  if (mod == FORMAT_ITEM_G) {
    switch (size) {
    case 1:
      {
        genie_whole (p);
        break;
      }
    case 2:
      {
        genie_fixed (p);
        break;
      }
    case 3:
      {
        genie_float (p);
        break;
      }
    default:
      {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FORMAT_INTS_REQUIRED, MODE (INT));
        exit_genie (p, A68_RUNTIME_ERROR);
        break;
      }
    }
  } else if (mod == FORMAT_ITEM_H) {
    int def_expo = 0, def_mult;
    A68_INT a_width, a_after, a_expo, a_mult;
    STATUS (&a_width) = INIT_MASK;
    VALUE (&a_width) = 0;
    STATUS (&a_after) = INIT_MASK;
    VALUE (&a_after) = 0;
    STATUS (&a_expo) = INIT_MASK;
    VALUE (&a_expo) = 0;
    STATUS (&a_mult) = INIT_MASK;
    VALUE (&a_mult) = 0;
    /*
     * Set default values 
     */
    if (mode == MODE (REAL) || mode == MODE (INT)) {
      def_expo = EXP_WIDTH + 1;
    } else if (mode == MODE (LONG_REAL) || mode == MODE (LONG_INT)) {
      def_expo = LONG_EXP_WIDTH + 1;
    } else if (mode == MODE (LONGLONG_REAL) || mode == MODE (LONGLONG_INT)) {
      def_expo = LONGLONG_EXP_WIDTH + 1;
    }
    def_mult = 3;
    /*
     * Pop user values 
     */
    switch (size) {
    case 1:
      {
        POP_OBJECT (p, &a_after, A68_INT);
        VALUE (&a_width) = VALUE (&a_after) + def_expo + 4;
        VALUE (&a_expo) = def_expo;
        VALUE (&a_mult) = def_mult;
        break;
      }
    case 2:
      {
        POP_OBJECT (p, &a_mult, A68_INT);
        POP_OBJECT (p, &a_after, A68_INT);
        VALUE (&a_width) = VALUE (&a_after) + def_expo + 4;
        VALUE (&a_expo) = def_expo;
        break;
      }
    case 3:
      {
        POP_OBJECT (p, &a_mult, A68_INT);
        POP_OBJECT (p, &a_after, A68_INT);
        POP_OBJECT (p, &a_width, A68_INT);
        VALUE (&a_expo) = def_expo;
        break;
      }
    case 4:
      {
        POP_OBJECT (p, &a_mult, A68_INT);
        POP_OBJECT (p, &a_expo, A68_INT);
        POP_OBJECT (p, &a_after, A68_INT);
        POP_OBJECT (p, &a_width, A68_INT);
        break;
      }
    default:
      {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FORMAT_INTS_REQUIRED, MODE (INT));
        exit_genie (p, A68_RUNTIME_ERROR);
        break;
      }
    }
    PUSH_PRIMITIVE (p, VALUE (&a_width), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&a_after), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&a_expo), A68_INT);
    PUSH_PRIMITIVE (p, VALUE (&a_mult), A68_INT);
    genie_real (p);
  }
  add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
}

/*!
\brief write %[-][+][w][.][d]s/d/i/f/e/b/o/x formats
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void write_c_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  BOOL_T right_align, sign;
  int width, after, letter;
  ADDR_T pop_sp = stack_pointer;
  char *str = NO_TEXT;
  if (IS (p, CHAR_C_PATTERN)) {
    A68_CHAR *z = (A68_CHAR *) item;
    char q[2];
    q[0] = (char) VALUE (z);
    q[1] = NULL_CHAR;
    str = (char *) &q;
    width = (int) strlen (str);
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
  } else if (IS (p, STRING_C_PATTERN)) {
    str = (char *) item;
    width = (int) strlen (str);
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
  } else if (IS (p, INTEGRAL_C_PATTERN)) {
    width = 0;
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
    unite_to_number (p, mode, item);
    PUSH_PRIMITIVE (p, (sign ? width : -width), A68_INT);
    str = whole (p);
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    int att = ATTRIBUTE (p), expval = 0, expo = 0;
    if (att == FLOAT_C_PATTERN || att == GENERAL_C_PATTERN) {
      int digits = 0;
      if (mode == MODE (REAL) || mode == MODE (INT)) {
        width = REAL_WIDTH + EXP_WIDTH + 4;
        after = REAL_WIDTH - 1;
        expo = EXP_WIDTH + 1;
      } else if (mode == MODE (LONG_REAL) || mode == MODE (LONG_INT)) {
        width = LONG_REAL_WIDTH + LONG_EXP_WIDTH + 4;
        after = LONG_REAL_WIDTH - 1;
        expo = LONG_EXP_WIDTH + 1;
      } else if (mode == MODE (LONGLONG_REAL) || mode == MODE (LONGLONG_INT)) {
        width = LONGLONG_REAL_WIDTH + LONGLONG_EXP_WIDTH + 4;
        after = LONGLONG_REAL_WIDTH - 1;
        expo = LONGLONG_EXP_WIDTH + 1;
      }
      scan_c_pattern (SUB (p), &right_align, &sign, &digits, &after, &letter);
      if (digits == 0 && after > 0) {
        width = after + expo + 4;
      } else if (digits > 0) {
        width = digits;
      }
      unite_to_number (p, mode, item);
      PUSH_PRIMITIVE (p, (sign ? width : -width), A68_INT);
      PUSH_PRIMITIVE (p, after, A68_INT);
      PUSH_PRIMITIVE (p, expo, A68_INT);
      PUSH_PRIMITIVE (p, 1, A68_INT);
      str = real (p);
      stack_pointer = pop_sp;
    }
    if (att == GENERAL_C_PATTERN) {
      char *expch = strchr (str, EXPONENT_CHAR);
      if (expch != NO_TEXT) {
        expval = (int) strtol (&(expch[1]), NO_VAR, 10);
      }
    }
    if ((att == FIXED_C_PATTERN) || (att == GENERAL_C_PATTERN && (expval > -4 && expval <= after))) {
      int digits = 0;
      if (mode == MODE (REAL) || mode == MODE (INT)) {
        width = REAL_WIDTH + 2;
        after = REAL_WIDTH - 1;
      } else if (mode == MODE (LONG_REAL) || mode == MODE (LONG_INT)) {
        width = LONG_REAL_WIDTH + 2;
        after = LONG_REAL_WIDTH - 1;
      } else if (mode == MODE (LONGLONG_REAL) || mode == MODE (LONGLONG_INT)) {
        width = LONGLONG_REAL_WIDTH + 2;
        after = LONGLONG_REAL_WIDTH - 1;
      }
      scan_c_pattern (SUB (p), &right_align, &sign, &digits, &after, &letter);
      if (digits == 0 && after > 0) {
        width = after + 2;
      } else if (digits > 0) {
        width = digits;
      }
      unite_to_number (p, mode, item);
      PUSH_PRIMITIVE (p, (sign ? width : -width), A68_INT);
      PUSH_PRIMITIVE (p, after, A68_INT);
      str = fixed (p);
      stack_pointer = pop_sp;
    }
  } else if (IS (p, BITS_C_PATTERN)) {
    int radix = 10, nibble = 1;
    width = 0;
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
    if (letter == FORMAT_ITEM_B) {
      radix = 2;
      nibble = 1;
    } else if (letter == FORMAT_ITEM_O) {
      radix = 8;
      nibble = 3;
    } else if (letter == FORMAT_ITEM_X) {
      radix = 16;
      nibble = 4;
    }
    if (width == 0) {
      if (mode == MODE (BITS)) {
        width = (int) ceil ((double) BITS_WIDTH / (double) nibble);
      } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
        width = (int) ceil ((double) get_mp_bits_width (mode) / (double) nibble);
      }
    }
    if (mode == MODE (BITS)) {
      A68_BITS *z = (A68_BITS *) item;
      reset_transput_buffer (EDIT_BUFFER);
      if (!convert_radix (p, VALUE (z), radix, width)) {
        errno = EDOM;
        value_error (p, mode, ref_file);
      }
      str = get_transput_buffer (EDIT_BUFFER);
    } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
      int digits = get_mp_digits (mode);
      MP_T *u = (MP_T *) item;
      MP_T *v;
      MP_T *w;
      STACK_MP (v, p, digits);
      STACK_MP (w, p, digits);
      reset_transput_buffer (EDIT_BUFFER);
      if (!convert_radix_mp (p, u, radix, width, mode, v, w)) {
        errno = EDOM;
        value_error (p, mode, ref_file);
      }
      str = get_transput_buffer (EDIT_BUFFER);
    }
  }
/* Did the conversion succeed? */
  if (a68g_strchr (str, ERROR_CHAR) != NO_TEXT) {
    value_error (p, mode, ref_file);
    (void) error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
  } else {
/* Align and output */
    if (width == 0) {
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else {
      if (right_align == A68_TRUE) {
        int blanks = width - (int) strlen (str);
        if (blanks >= 0) {
          while (blanks--) {
            add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
          }
          add_string_transput_buffer (p, FORMATTED_BUFFER, str);
        } else {
          value_error (p, mode, ref_file);
          (void) error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
        }
      } else {
        int blanks;
        while (str[0] == BLANK_CHAR) {
          str++;
        }
        blanks = width - (int) strlen (str);
        if (blanks >= 0) {
          add_string_transput_buffer (p, FORMATTED_BUFFER, str);
          while (blanks--) {
            add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
          }
        } else {
          value_error (p, mode, ref_file);
          (void) error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
        }
      }
    }
  }
}

/*!
\brief read one char from file
\param p position in tree
\param ref_file fat pointer to A68 file
\return same
**/

static char read_single_char (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *file = FILE_DEREF (&ref_file);
  int ch = char_scanner (file);
  if (ch == EOF_CHAR) {
    end_of_file_error (p, ref_file);
  }
  return ((char) ch);
}

/*!
\brief scan n chars from file to input buffer
\param p position in tree
\param n chars to scan
\param m mode being scanned
\param ref_file fat pointer to A68 file
**/

static void scan_n_chars (NODE_T * p, int n, MOID_T * m, A68_REF ref_file)
{
  int k;
  (void) m;
  for (k = 0; k < n; k++) {
    int ch = read_single_char (p, ref_file);
    add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
  }
}

/*!
\brief read %[-][+][w][.][d]s/d/i/f/e/b/o/x formats
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void read_c_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  BOOL_T right_align, sign;
  int width, after, letter;
  ADDR_T pop_sp = stack_pointer;
  reset_transput_buffer (INPUT_BUFFER);
  if (IS (p, CHAR_C_PATTERN)) {
    width = 0;
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
    if (width == 0) {
      genie_read_standard (p, mode, item, ref_file);
    } else {
      scan_n_chars (p, width, mode, ref_file);
      if (width > 1 && right_align == A68_FALSE) {
        for (; width > 1; width--) {
          (void) pop_char_transput_buffer (INPUT_BUFFER);
        }
      }
      genie_string_to_value (p, mode, item, ref_file);
    }
  } else if (IS (p, STRING_C_PATTERN)) {
    width = 0;
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
    if (width == 0) {
      genie_read_standard (p, mode, item, ref_file);
    } else {
      scan_n_chars (p, width, mode, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    }
  } else if (IS (p, INTEGRAL_C_PATTERN)) {
    if (mode != MODE (INT) && mode != MODE (LONG_INT) && mode != MODE (LONGLONG_INT)) {
      pattern_error (p, mode, ATTRIBUTE (p));
    } else {
      width = 0;
      scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
      if (width == 0) {
        genie_read_standard (p, mode, item, ref_file);
      } else {
        scan_n_chars (p, (sign != 0) ? width + 1 : width, mode, ref_file);
        genie_string_to_value (p, mode, item, ref_file);
      }
    }
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    if (mode != MODE (REAL) && mode != MODE (LONG_REAL) && mode != MODE (LONGLONG_REAL)) {
      pattern_error (p, mode, ATTRIBUTE (p));
    } else {
      width = 0;
      scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
      if (width == 0) {
        genie_read_standard (p, mode, item, ref_file);
      } else {
        scan_n_chars (p, (sign != 0) ? width + 1 : width, mode, ref_file);
        genie_string_to_value (p, mode, item, ref_file);
      }
    }
  } else if (IS (p, BITS_C_PATTERN)) {
    if (mode != MODE (BITS) && mode != MODE (LONG_BITS) && mode != MODE (LONGLONG_BITS)) {
      pattern_error (p, mode, ATTRIBUTE (p));
    } else {
      int radix = 10;
      char *str;
      width = 0;
      scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
      if (letter == FORMAT_ITEM_B) {
        radix = 2;
      } else if (letter == FORMAT_ITEM_O) {
        radix = 8;
      } else if (letter == FORMAT_ITEM_X) {
        radix = 16;
      }
      str = get_transput_buffer (INPUT_BUFFER);
      if (width == 0) {
        A68_FILE *file = FILE_DEREF (&ref_file);
        int ch;
        ASSERT (snprintf (str, (size_t) TRANSPUT_BUFFER_SIZE, "%dr", radix) >= 0);
        set_transput_buffer_index (INPUT_BUFFER, (int) strlen (str));
        ch = char_scanner (file);
        while (ch != EOF_CHAR && (IS_SPACE (ch) || IS_NL_FF (ch))) {
          if (IS_NL_FF (ch)) {
            skip_nl_ff (p, &ch, ref_file);
          } else {
            ch = char_scanner (file);
          }
        }
        while (ch != EOF_CHAR && IS_XDIGIT (ch)) {
          add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
          ch = char_scanner (file);
        }
        unchar_scanner (p, file, (char) ch);
      } else {
        ASSERT (snprintf (str, (size_t) TRANSPUT_BUFFER_SIZE, "%dr", radix) >= 0);
        set_transput_buffer_index (INPUT_BUFFER, (int) strlen (str));
        scan_n_chars (p, width, mode, ref_file);
      }
      genie_string_to_value (p, mode, item, ref_file);
    }
  }
  stack_pointer = pop_sp;
}

/* INTEGRAL, REAL, COMPLEX and BITS patterns */

/*!
\brief count Z and D frames in a mould
\param p position in tree
\param z counting integer
**/

static void count_zd_frames (NODE_T * p, int *z)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FORMAT_ITEM_D) || IS (p, FORMAT_ITEM_Z)) {
      (*z)++;
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        count_zd_frames (NEXT (p), z);
      }
      return;
    } else {
      count_zd_frames (SUB (p), z);
    }
  }
}

/*!
\brief get sign from sign mould
\param p position in tree
\return position of sign in tree or NULL
**/

static NODE_T *get_sign (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    NODE_T *q = get_sign (SUB (p));
    if (q != NO_NODE) {
      return (q);
    } else if (IS (p, FORMAT_ITEM_PLUS) || IS (p, FORMAT_ITEM_MINUS)) {
      return (p);
    }
  }
  return (NO_NODE);
}

/*!
\brief shift sign through Z frames until non-zero digit or D frame
\param p position in tree
\param q string to propagate sign through
**/

static void shift_sign (NODE_T * p, char **q)
{
  for (; p != NO_NODE && (*q) != NO_TEXT; FORWARD (p)) {
    shift_sign (SUB (p), q);
    if (IS (p, FORMAT_ITEM_Z)) {
      if (((*q)[0] == '+' || (*q)[0] == '-') && (*q)[1] == '0') {
        char ch = (*q)[0];
        (*q)[0] = (*q)[1];
        (*q)[1] = ch;
        (*q)++;
      }
    } else if (IS (p, FORMAT_ITEM_D)) {
      (*q) = NO_TEXT;
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        shift_sign (NEXT (p), q);
      }
      return;
    }
  }
}

/*!
\brief pad trailing blanks to integral until desired width
\param p position in tree
\param n number of zeroes to pad
**/

static void put_zeroes_to_integral (NODE_T * p, int n)
{
  for (; n > 0; n--) {
    add_char_transput_buffer (p, EDIT_BUFFER, '0');
  }
}

/*!
\brief pad a sign to integral representation
\param p position in tree
\param sign sign
**/

static void put_sign_to_integral (NODE_T * p, int sign)
{
  NODE_T *sign_node = get_sign (SUB (p));
  if (IS (sign_node, FORMAT_ITEM_PLUS)) {
    add_char_transput_buffer (p, EDIT_BUFFER, (char) (sign >= 0 ? '+' : '-'));
  } else {
    add_char_transput_buffer (p, EDIT_BUFFER, (char) (sign >= 0 ? BLANK_CHAR : '-'));
  }
}

/*!
\brief write point, exponent or plus-i-times symbol
\param p position in tree
\param ref_file fat pointer to A68 file
\param att attribute
\param sym symbol to print when matched
**/

static void write_pie_frame (NODE_T * p, A68_REF ref_file, int att, int sym)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      write_insertion (p, ref_file, INSERTION_NORMAL);
    } else if (IS (p, att)) {
      write_pie_frame (SUB (p), ref_file, att, sym);
      return;
    } else if (IS (p, sym)) {
      add_string_transput_buffer (p, FORMATTED_BUFFER, NSYMBOL (p));
    } else if (IS (p, FORMAT_ITEM_S)) {
      return;
    }
  }
}

/*!
\brief write sign when appropriate
\param p position in tree
\param q string to write
**/

static void write_mould_put_sign (NODE_T * p, char **q)
{
  if ((*q)[0] == '+' || (*q)[0] == '-' || (*q)[0] == BLANK_CHAR) {
    add_char_transput_buffer (p, FORMATTED_BUFFER, (*q)[0]);
    (*q)++;
  }
}

/*!
\brief write string according to a mould
\param p position in tree
\param ref_file fat pointer to A68 file
\param type pattern type
\param q string to write
\param mood mode of operation
**/

static void add_char_mould (NODE_T *p, char ch, char **q)
{
  if (ch != NULL_CHAR) {
    add_char_transput_buffer (p, FORMATTED_BUFFER, ch);
    (*q)++;
  }
}

static void write_mould (NODE_T * p, A68_REF ref_file, int type, char **q, unsigned *mood)
{
  for (; p != NO_NODE; FORWARD (p)) {
/* Insertions are inserted straight away. Note that we can suppress them using "mood", which is not standard A68 */
    if (IS (p, INSERTION)) {
      write_insertion (SUB (p), ref_file, *mood);
    } else {
      write_mould (SUB (p), ref_file, type, q, mood);
/* Z frames print blanks until first non-zero digits comes */
      if (IS (p, FORMAT_ITEM_Z)) {
        write_mould_put_sign (p, q);
        if ((*q)[0] == '0') {
          if (*mood & DIGIT_BLANK) {
            add_char_mould (p, BLANK_CHAR, q);
            *mood = (*mood & ~INSERTION_NORMAL) | INSERTION_BLANK;
          } else if (*mood & DIGIT_NORMAL) {
            add_char_mould (p, '0', q);
            *mood = (unsigned) (DIGIT_NORMAL | INSERTION_NORMAL);
          }
        } else {
          add_char_mould (p, (*q)[0], q);
          *mood = (unsigned) (DIGIT_NORMAL | INSERTION_NORMAL);
        }
      }
/* D frames print a digit */
      else if (IS (p, FORMAT_ITEM_D)) {
        write_mould_put_sign (p, q);
        add_char_mould (p, (*q)[0], q);
        *mood = (unsigned) (DIGIT_NORMAL | INSERTION_NORMAL);
      }
/* Suppressible frames */
      else if (IS (p, FORMAT_ITEM_S)) {
/* Suppressible frames are ignored in a sign-mould */
        if (type == SIGN_MOULD) {
          write_mould (NEXT (p), ref_file, type, q, mood);
        } else if (type == INTEGRAL_MOULD) {
          if ((*q)[0] != NULL_CHAR) {
            (*q)++;
          }
        }
        return;
      }
/* Replicator */
      else if (IS (p, REPLICATOR)) {
        int j, k = get_replicator_value (SUB (p), A68_TRUE);
        for (j = 1; j <= k; j++) {
          write_mould (NEXT (p), ref_file, type, q, mood);
        }
        return;
      }
    }
  }
}

/*!
\brief write INT value using int pattern
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void write_integral_pattern (NODE_T * p, MOID_T * mode, MOID_T * root, BYTE_T * item, A68_REF ref_file)
{
  RESET_ERRNO;
  if (!(mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT))) {
    pattern_error (p, root, ATTRIBUTE (p));
  } else {
    ADDR_T old_stack_pointer = stack_pointer;
    char *str;
    int width = 0, sign = 0;
    unsigned mood;
/* Dive into the pattern if needed */
    if (IS (p, INTEGRAL_PATTERN)) {
      p = SUB (p);
    }
/* Find width */
    count_zd_frames (p, &width);
/* Make string */
    reset_transput_buffer (EDIT_BUFFER);
    if (mode == MODE (INT)) {
      A68_INT *z = (A68_INT *) item;
      sign = SIGN (VALUE (z));
      str = sub_whole (p, ABS (VALUE (z)), width);
    } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
      MP_T *z = (MP_T *) item;
      sign = SIGN (z[2]);
      z[2] = ABS (z[2]);
      str = long_sub_whole (p, z, get_mp_digits (mode), width);
    }
/* Edit string and output */
    if (a68g_strchr (str, ERROR_CHAR) != NO_TEXT) {
      value_error (p, root, ref_file);
    }
    if (IS (p, SIGN_MOULD)) {
      put_sign_to_integral (p, sign);
    } else if (sign < 0) {
      value_sign_error (p, root, ref_file);
    }
    put_zeroes_to_integral (p, width - (int) strlen (str));
    add_string_transput_buffer (p, EDIT_BUFFER, str);
    str = get_transput_buffer (EDIT_BUFFER);
    mood = (unsigned) (DIGIT_BLANK | INSERTION_NORMAL);
    if (IS (p, SIGN_MOULD)) {
      if (str[0] == '+' || str[0] == '-') {
        shift_sign (SUB (p), &str);
      }
      str = get_transput_buffer (EDIT_BUFFER);
      write_mould (SUB (p), ref_file, SIGN_MOULD, &str, &mood);
      FORWARD (p);
    }
    if (IS (p, INTEGRAL_MOULD)) {  /* This *should* be the case */
      write_mould (SUB (p), ref_file, INTEGRAL_MOULD, &str, &mood);
    }
    stack_pointer = old_stack_pointer;
  }
}

/*!
\brief write REAL value using real pattern
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void write_real_pattern (NODE_T * p, MOID_T * mode, MOID_T * root, BYTE_T * item, A68_REF ref_file)
{
  RESET_ERRNO;
  if (!(mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL) || mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT))) {
    pattern_error (p, root, ATTRIBUTE (p));
  } else {
    ADDR_T old_stack_pointer = stack_pointer;
    int sign_digits, stag_digits = 0, frac_digits = 0, expo_digits = 0;
    int mant_length, sign = 0, exp_value;
    NODE_T *q, *sign_mould = NO_NODE, *stag_mould = NO_NODE, *point_frame = NO_NODE, *frac_mould = NO_NODE, *e_frame = NO_NODE, *expo_mould = NO_NODE;
    char *str = NO_TEXT, *stag_str = NO_TEXT, *frac_str = NO_TEXT;
    unsigned mood;
/* Dive into pattern */
    q = ((IS (p, REAL_PATTERN)) ? SUB (p) : p);
/* Dissect pattern and establish widths */
    if (q != NO_NODE && IS (q, SIGN_MOULD)) {
      sign_mould = q;
      count_zd_frames (SUB (sign_mould), &stag_digits);
      FORWARD (q);
    }
    sign_digits = stag_digits;
    if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
      stag_mould = q;
      count_zd_frames (SUB (stag_mould), &stag_digits);
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, FORMAT_POINT_FRAME)) {
      point_frame = q;
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
      frac_mould = q;
      count_zd_frames (SUB (frac_mould), &frac_digits);
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, EXPONENT_FRAME)) {
      e_frame = SUB (q);
      expo_mould = NEXT_SUB (q);
      q = expo_mould;
      if (IS (q, SIGN_MOULD)) {
        count_zd_frames (SUB (q), &expo_digits);
        FORWARD (q);
      }
      if (IS (q, INTEGRAL_MOULD)) {
        count_zd_frames (SUB (q), &expo_digits);
      }
    }
/* Make string representation */
    if (point_frame == NO_NODE) {
      mant_length = stag_digits;
    } else {
      mant_length = 1 + stag_digits + frac_digits;
    }
    if (mode == MODE (REAL) || mode == MODE (INT)) {
      double x;
      if (mode == MODE (REAL)) {
        x = VALUE ((A68_REAL *) item);
      } else {
        x = (double) VALUE ((A68_INT *) item);
      }
#if defined HAVE_IEEE_754
      if (NOT_A_REAL (x)) {
        char *s = stack_string (p, 8 + mant_length);
        (void) error_chars (s, mant_length);
        add_string_transput_buffer (p, FORMATTED_BUFFER, s);
        stack_pointer = old_stack_pointer;
        return;
      }
#endif
      exp_value = 0;
      sign = SIGN (x);
      if (sign_mould != NO_NODE) {
        put_sign_to_integral (sign_mould, sign);
      }
      x = ABS (x);
      if (expo_mould != NO_NODE) {
        standardise (&x, stag_digits, frac_digits, &exp_value);
      }
      str = sub_fixed (p, x, mant_length, frac_digits);
    } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
      ADDR_T old_stack_pointer2 = stack_pointer;
      int digits = get_mp_digits (mode);
      MP_T *x;
      STACK_MP (x, p, digits);
      MOVE_MP (x, (MP_T *) item, digits);
      exp_value = 0;
      sign = SIGN (x[2]);
      if (sign_mould != NO_NODE) {
        put_sign_to_integral (sign_mould, sign);
      }
      x[2] = ABS (x[2]);
      if (expo_mould != NO_NODE) {
        long_standardise (p, x, get_mp_digits (mode), stag_digits, frac_digits, &exp_value);
      }
      str = long_sub_fixed (p, x, get_mp_digits (mode), mant_length, frac_digits);
      stack_pointer = old_stack_pointer2;
    }
/* Edit and output the string */
    if (a68g_strchr (str, ERROR_CHAR) != NO_TEXT) {
      value_error (p, root, ref_file);
    }
    reset_transput_buffer (STRING_BUFFER);
    add_string_transput_buffer (p, STRING_BUFFER, str);
    stag_str = get_transput_buffer (STRING_BUFFER);
    if (a68g_strchr (stag_str, ERROR_CHAR) != NO_TEXT) {
      value_error (p, root, ref_file);
    }
    str = a68g_strchr (stag_str, POINT_CHAR);
    if (str != NO_TEXT) {
      frac_str = &str[1];
      str[0] = NULL_CHAR;
    } else {
      frac_str = NO_TEXT;
    }
/* Stagnant part */
    reset_transput_buffer (EDIT_BUFFER);
    if (sign_mould != NO_NODE) {
      put_sign_to_integral (sign_mould, sign);
    } else if (sign < 0) {
      value_sign_error (sign_mould, root, ref_file);
    }
    put_zeroes_to_integral (p, stag_digits - (int) strlen (stag_str));
    add_string_transput_buffer (p, EDIT_BUFFER, stag_str);
    stag_str = get_transput_buffer (EDIT_BUFFER);
    mood = (unsigned) (DIGIT_BLANK | INSERTION_NORMAL);
    if (sign_mould != NO_NODE) {
      if (stag_str[0] == '+' || stag_str[0] == '-') {
        shift_sign (SUB (p), &stag_str);
      }
      stag_str = get_transput_buffer (EDIT_BUFFER);
      write_mould (SUB (sign_mould), ref_file, SIGN_MOULD, &stag_str, &mood);
    }
    if (stag_mould != NO_NODE) {
      write_mould (SUB (stag_mould), ref_file, INTEGRAL_MOULD, &stag_str, &mood);
    }
/* Point frame */
    if (point_frame != NO_NODE) {
      write_pie_frame (point_frame, ref_file, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT);
    }
/* Fraction */
    if (frac_mould != NO_NODE) {
      reset_transput_buffer (EDIT_BUFFER);
      add_string_transput_buffer (p, EDIT_BUFFER, frac_str);
      frac_str = get_transput_buffer (EDIT_BUFFER);
      mood = (unsigned) (DIGIT_NORMAL | INSERTION_NORMAL);
      write_mould (SUB (frac_mould), ref_file, INTEGRAL_MOULD, &frac_str, &mood);
    }
/* Exponent */
    if (expo_mould != NO_NODE) {
      A68_INT z;
      STATUS (&z) = INIT_MASK;
      VALUE (&z) = exp_value;
      if (e_frame != NO_NODE) {
        write_pie_frame (e_frame, ref_file, FORMAT_E_FRAME, FORMAT_ITEM_E);
      }
      write_integral_pattern (expo_mould, MODE (INT), root, (BYTE_T *) & z, ref_file);
    }
    stack_pointer = old_stack_pointer;
  }
}

/*!
\brief write COMPLEX value using complex pattern
\param p position in tree
\param comp mode of complex number
\param re pointer to real part
\param im pointer to imaginary part
\param ref_file fat pointer to A68 file
**/

static void write_complex_pattern (NODE_T * p, MOID_T * comp, MOID_T * root, BYTE_T * re, BYTE_T * im, A68_REF ref_file)
{
  NODE_T *reel, *plus_i_times, *imag;
  RESET_ERRNO;
/* Dissect pattern */
  reel = SUB (p);
  plus_i_times = NEXT (reel);
  imag = NEXT (plus_i_times);
/* Write pattern */
  write_real_pattern (reel, comp, root, re, ref_file);
  write_pie_frame (plus_i_times, ref_file, FORMAT_I_FRAME, FORMAT_ITEM_I);
  write_real_pattern (imag, comp, root, im, ref_file);
}

/*!
\brief write BITS value using bits pattern
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void write_bits_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  if (mode == MODE (BITS)) {
    int width = 0, radix;
    unsigned mood;
    A68_BITS *z = (A68_BITS *) item;
    char *str;
/* Establish width and radix */
    count_zd_frames (SUB (p), &width);
    radix = get_replicator_value (SUB_SUB (p), A68_TRUE);
    if (radix < 2 || radix > 16) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* Generate string of correct width */
    reset_transput_buffer (EDIT_BUFFER);
    if (!convert_radix (p, VALUE (z), radix, width)) {
      errno = EDOM;
      value_error (p, mode, ref_file);
    }
/* Output the edited string */
    mood = (unsigned) (DIGIT_BLANK | INSERTION_NORMAL);
    str = get_transput_buffer (EDIT_BUFFER);
    write_mould (NEXT_SUB (p), ref_file, INTEGRAL_MOULD, &str, &mood);
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    ADDR_T pop_sp = stack_pointer;
    int width = 0, radix, digits = get_mp_digits (mode);
    unsigned mood;
    MP_T *u = (MP_T *) item;
    MP_T *v;
    MP_T *w;
    char *str;
    STACK_MP (v, p, digits);
    STACK_MP (w, p, digits);
/* Establish width and radix */
    count_zd_frames (SUB (p), &width);
    radix = get_replicator_value (SUB_SUB (p), A68_TRUE);
    if (radix < 2 || radix > 16) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
/* Generate string of correct width */
    reset_transput_buffer (EDIT_BUFFER);
    if (!convert_radix_mp (p, u, radix, width, mode, v, w)) {
      errno = EDOM;
      value_error (p, mode, ref_file);
    }
/* Output the edited string */
    mood = (unsigned) (DIGIT_BLANK | INSERTION_NORMAL);
    str = get_transput_buffer (EDIT_BUFFER);
    write_mould (NEXT_SUB (p), ref_file, INTEGRAL_MOULD, &str, &mood);
    stack_pointer = pop_sp;
  }
}

/*!
\brief write value to file
\param p position in tree
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void genie_write_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) == NO_NODE) {
    genie_value_to_string (p, MODE (REAL), item, ATTRIBUTE (SUB (p)));
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
    write_number_generic (p, MODE (REAL), item, ATTRIBUTE (SUB (p)));
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    write_c_pattern (p, MODE (REAL), item, ref_file);
  } else if (IS (p, REAL_PATTERN)) {
    write_real_pattern (p, MODE (REAL), MODE (REAL), item, ref_file);
  } else if (IS (p, COMPLEX_PATTERN)) {
    A68_REAL im;
    STATUS (&im) = INIT_MASK;
    VALUE (&im) = 0.0;
    write_complex_pattern (p, MODE (REAL), MODE (COMPLEX), (BYTE_T *) item, (BYTE_T *) & im, ref_file);
  } else {
    pattern_error (p, MODE (REAL), ATTRIBUTE (p));
  }
}

/*!
\brief write value to file
\param p position in tree
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void genie_write_long_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) == NO_NODE) {
    genie_value_to_string (p, MODE (LONG_REAL), item, ATTRIBUTE (SUB (p)));
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
    write_number_generic (p, MODE (LONG_REAL), item, ATTRIBUTE (SUB (p)));
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    write_c_pattern (p, MODE (LONG_REAL), item, ref_file);
  } else if (IS (p, REAL_PATTERN)) {
    write_real_pattern (p, MODE (LONG_REAL), MODE (LONG_REAL), item, ref_file);
  } else if (IS (p, COMPLEX_PATTERN)) {
    ADDR_T old_stack_pointer = stack_pointer;
    MP_T *z;
    STACK_MP (z, p, get_mp_digits (MODE (LONG_REAL)));
    SET_MP_ZERO (z, get_mp_digits (MODE (LONG_REAL)));
    z[0] = (MP_T) INIT_MASK;
    write_complex_pattern (p, MODE (LONG_REAL), MODE (LONG_COMPLEX), item, (BYTE_T *) z, ref_file);
    stack_pointer = old_stack_pointer;
  } else {
    pattern_error (p, MODE (LONG_REAL), ATTRIBUTE (p));
  }
}

/*!
\brief write value to file
\param p position in tree
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void genie_write_longlong_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) == NO_NODE) {
    genie_value_to_string (p, MODE (LONGLONG_REAL), item, ATTRIBUTE (SUB (p)));
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
    write_number_generic (p, MODE (LONGLONG_REAL), item, ATTRIBUTE (SUB (p)));
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    write_c_pattern (p, MODE (LONGLONG_REAL), item, ref_file);
  } else if (IS (p, REAL_PATTERN)) {
    write_real_pattern (p, MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), item, ref_file);
  } else if (IS (p, COMPLEX_PATTERN)) {
    ADDR_T old_stack_pointer = stack_pointer;
    MP_T *z;
    STACK_MP (z, p, get_mp_digits (MODE (LONGLONG_REAL)));
    SET_MP_ZERO (z, get_mp_digits (MODE (LONGLONG_REAL)));
    z[0] = (MP_T) INIT_MASK;
    write_complex_pattern (p, MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX), item, (BYTE_T *) z, ref_file);
    stack_pointer = old_stack_pointer;
  } else {
    pattern_error (p, MODE (LONGLONG_REAL), ATTRIBUTE (p));
  }
}

/*!
\brief write value to file
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void genie_write_standard_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  RESET_ERRNO;
  if (mode == MODE (INT)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (pat)));
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NO_NODE) {
      write_number_generic (pat, MODE (INT), item, ATTRIBUTE (SUB (pat)));
    } else if (IS (pat, INTEGRAL_C_PATTERN) || IS (pat, FIXED_C_PATTERN) || IS (pat, FLOAT_C_PATTERN) || IS (pat, GENERAL_C_PATTERN)) {
      write_c_pattern (pat, MODE (INT), item, ref_file);
    } else if (IS (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, MODE (INT), MODE (INT), item, ref_file);
    } else if (IS (pat, REAL_PATTERN)) {
      write_real_pattern (pat, MODE (INT), MODE (INT), item, ref_file);
    } else if (IS (pat, COMPLEX_PATTERN)) {
      A68_REAL re, im;
      STATUS (&re) = INIT_MASK;
      VALUE (&re) = (double) VALUE ((A68_INT *) item);
      STATUS (&im) = INIT_MASK;
      VALUE (&im) = 0.0;
      write_complex_pattern (pat, MODE (REAL), MODE (COMPLEX), (BYTE_T *) & re, (BYTE_T *) & im, ref_file);
    } else if (IS (pat, CHOICE_PATTERN)) {
      int k = VALUE ((A68_INT *) item);
      write_choice_pattern (NEXT_SUB (pat), ref_file, &k);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (LONG_INT)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (pat)));
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NO_NODE) {
      write_number_generic (pat, MODE (LONG_INT), item, ATTRIBUTE (SUB (pat)));
    } else if (IS (pat, INTEGRAL_C_PATTERN) || IS (pat, FIXED_C_PATTERN) || IS (pat, FLOAT_C_PATTERN) || IS (pat, GENERAL_C_PATTERN)) {
      write_c_pattern (pat, MODE (LONG_INT), item, ref_file);
    } else if (IS (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, MODE (LONG_INT), MODE (LONG_INT), item, ref_file);
    } else if (IS (pat, REAL_PATTERN)) {
      write_real_pattern (pat, MODE (LONG_INT), MODE (LONG_INT), item, ref_file);
    } else if (IS (pat, COMPLEX_PATTERN)) {
      ADDR_T old_stack_pointer = stack_pointer;
      MP_T *z;
      STACK_MP (z, p, get_mp_digits (mode));
      SET_MP_ZERO (z, get_mp_digits (mode));
      z[0] = (MP_T) INIT_MASK;
      write_complex_pattern (pat, MODE (LONG_REAL), MODE (LONG_COMPLEX), item, (BYTE_T *) z, ref_file);
      stack_pointer = old_stack_pointer;
    } else if (IS (pat, CHOICE_PATTERN)) {
      int k = mp_to_int (p, (MP_T *) item, get_mp_digits (mode));
      write_choice_pattern (NEXT_SUB (pat), ref_file, &k);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (LONGLONG_INT)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (pat)));
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NO_NODE) {
      write_number_generic (pat, MODE (LONGLONG_INT), item, ATTRIBUTE (SUB (pat)));
    } else if (IS (pat, INTEGRAL_C_PATTERN) || IS (pat, FIXED_C_PATTERN) || IS (pat, FLOAT_C_PATTERN) || IS (pat, GENERAL_C_PATTERN)) {
      write_c_pattern (pat, MODE (LONGLONG_INT), item, ref_file);
    } else if (IS (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, MODE (LONGLONG_INT), MODE (LONGLONG_INT), item, ref_file);
    } else if (IS (pat, REAL_PATTERN)) {
      write_real_pattern (pat, MODE (INT), MODE (INT), item, ref_file);
    } else if (IS (pat, REAL_PATTERN)) {
      write_real_pattern (pat, MODE (LONGLONG_INT), MODE (LONGLONG_INT), item, ref_file);
    } else if (IS (pat, COMPLEX_PATTERN)) {
      ADDR_T old_stack_pointer = stack_pointer;
      MP_T *z;
      STACK_MP (z, p, get_mp_digits (MODE (LONGLONG_REAL)));
      SET_MP_ZERO (z, get_mp_digits (mode));
      z[0] = (MP_T) INIT_MASK;
      write_complex_pattern (pat, MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX), item, (BYTE_T *) z, ref_file);
      stack_pointer = old_stack_pointer;
    } else if (IS (pat, CHOICE_PATTERN)) {
      int k = mp_to_int (p, (MP_T *) item, get_mp_digits (mode));
      write_choice_pattern (NEXT_SUB (pat), ref_file, &k);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (REAL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_real_format (pat, item, ref_file);
  } else if (mode == MODE (LONG_REAL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_long_real_format (pat, item, ref_file);
  } else if (mode == MODE (LONGLONG_REAL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_longlong_real_format (pat, item, ref_file);
  } else if (mode == MODE (COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, MODE (REAL), MODE (COMPLEX), &item[0], &item[MOID_SIZE (MODE (REAL))], ref_file);
    } else {
/* Try writing as two REAL values */
      genie_write_real_format (pat, item, ref_file);
      genie_write_standard_format (p, MODE (REAL), &item[MOID_SIZE (MODE (REAL))], ref_file);
    }
  } else if (mode == MODE (LONG_COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, MODE (LONG_REAL), MODE (LONG_COMPLEX), &item[0], &item[MOID_SIZE (MODE (LONG_REAL))], ref_file);
    } else {
/* Try writing as two LONG REAL values */
      genie_write_long_real_format (pat, item, ref_file);
      genie_write_standard_format (p, MODE (LONG_REAL), &item[MOID_SIZE (MODE (LONG_REAL))], ref_file);
    }
  } else if (mode == MODE (LONGLONG_COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, MODE (LONGLONG_REAL), MODE (LONGLONG_COMPLEX), &item[0], &item[MOID_SIZE (MODE (LONGLONG_REAL))], ref_file);
    } else {
/* Try writing as two LONG LONG REAL values */
      genie_write_longlong_real_format (pat, item, ref_file);
      genie_write_standard_format (p, MODE (LONGLONG_REAL), &item[MOID_SIZE (MODE (LONGLONG_REAL))], ref_file);
    }
  } else if (mode == MODE (BOOL)) {
    A68_BOOL *z = (A68_BOOL *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, (char) (VALUE (z) == A68_TRUE ? FLIP_CHAR : FLOP_CHAR));
    } else if (IS (pat, BOOLEAN_PATTERN)) {
      if (NEXT_SUB (pat) == NO_NODE) {
        add_char_transput_buffer (p, FORMATTED_BUFFER, (char) (VALUE (z) == A68_TRUE ? FLIP_CHAR : FLOP_CHAR));
      } else {
        write_boolean_pattern (pat, ref_file, (BOOL_T) (VALUE (z) == A68_TRUE));
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (BITS)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      char *str = (char *) STACK_TOP;
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (p)));
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else if (IS (pat, BITS_PATTERN)) {
      write_bits_pattern (pat, MODE (BITS), item, ref_file);
    } else if (IS (pat, BITS_C_PATTERN)) {
      write_c_pattern (pat, MODE (BITS), item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      char *str = (char *) STACK_TOP;
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (p)));
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else if (IS (pat, BITS_PATTERN)) {
      write_bits_pattern (pat, mode, item, ref_file);
    } else if (IS (pat, BITS_C_PATTERN)) {
      write_c_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (CHAR)) {
    A68_CHAR *z = (A68_CHAR *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, (char) VALUE (z));
    } else if (IS (pat, STRING_PATTERN)) {
      char *q = get_transput_buffer (EDIT_BUFFER);
      reset_transput_buffer (EDIT_BUFFER);
      add_char_transput_buffer (p, EDIT_BUFFER, (char) VALUE (z));
      write_string_pattern (pat, mode, ref_file, &q);
      if (q[0] != NULL_CHAR) {
        value_error (p, mode, ref_file);
      }
    } else if (IS (pat, STRING_C_PATTERN)) {
      write_c_pattern (pat, mode, (BYTE_T *) z, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
/* Handle these separately instead of printing [] CHAR */
    A68_REF row = *(A68_REF *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      PUSH_REF (p, row);
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (IS (pat, STRING_PATTERN)) {
      char *q;
      PUSH_REF (p, row);
      reset_transput_buffer (EDIT_BUFFER);
      add_string_from_stack_transput_buffer (p, EDIT_BUFFER);
      q = get_transput_buffer (EDIT_BUFFER);
      write_string_pattern (pat, mode, ref_file, &q);
      if (q[0] != NULL_CHAR) {
        value_error (p, mode, ref_file);
      }
    } else if (IS (pat, STRING_C_PATTERN)) {
      char *q;
      PUSH_REF (p, row);
      reset_transput_buffer (EDIT_BUFFER);
      add_string_from_stack_transput_buffer (p, EDIT_BUFFER);
      q = get_transput_buffer (EDIT_BUFFER);
      write_c_pattern (pat, mode, (BYTE_T *) q, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (IS (mode, UNION_SYMBOL)) {
    A68_UNION *z = (A68_UNION *) item;
    genie_write_standard_format (p, (MOID_T *) (VALUE (z)), &item[A68_UNION_SIZE], ref_file);
  } else if (IS (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NO_PACK; FORWARD (q)) {
      BYTE_T *elem = &item[OFFSET (q)];
      genie_check_initialisation (p, elem, MOID (q));
      genie_write_standard_format (p, MOID (q), elem, ref_file);
    }
  } else if (IS (mode, ROW_SYMBOL) || IS (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    CHECK_INIT (p, INITIALISED ((A68_REF *) item), MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, DIM (arr)) > 0) {
      BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
      BOOL_T done = A68_FALSE;
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        ADDR_T a68g_index = calculate_internal_index (tup, DIM (arr));
        ADDR_T elem_addr = ROW_ELEMENT (arr, a68g_index);
        BYTE_T *elem = &base_addr[elem_addr];
        genie_check_initialisation (p, elem, SUB (deflexed));
        genie_write_standard_format (p, SUB (deflexed), elem, ref_file);
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief at end of write purge all insertions
\param p position in tree
\param ref_file fat pointer to A68 file
**/

static void purge_format_write (NODE_T * p, A68_REF ref_file)
{
/* Problem here is shutting down embedded formats */
  BOOL_T go_on;
  do {
    A68_FILE *file;
    NODE_T *dollar, *pat;
    A68_FORMAT *old_fmt;
    while ((pat = get_next_format_pattern (p, ref_file, SKIP_PATTERN)) != NO_NODE) {
      format_error (p, ref_file, ERROR_FORMAT_PICTURES);
    }
    file = FILE_DEREF (&ref_file);
    dollar = SUB (BODY (&FORMAT (file)));
    old_fmt = (A68_FORMAT *) FRAME_LOCAL (frame_pointer, OFFSET (TAX (dollar)));
    go_on = (BOOL_T) ! IS_NIL_FORMAT (old_fmt);
    if (go_on) {
/* Pop embedded format and proceed */
      (void) end_of_format (p, ref_file);
    }
  } while (go_on);
}

/*!
\brief PROC ([] SIMPLOUT) VOID print f, write f
\param p position in tree
**/

void genie_write_format (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_out (p);
  PUSH_REF (p, row);
  genie_write_file_format (p);
}

/*!
\brief PROC (REF FILE, [] SIMPLOUT) VOID put f
\param p position in tree
**/

void genie_write_file_format (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index, formats;
  ADDR_T save_frame_pointer, save_stack_pointer;
  POP_REF (p, &row);
  CHECK_REF (p, row, MODE (ROW_SIMPLOUT));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (READ_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "read");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!PUT (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!READ_MOOD (file) && !WRITE_MOOD (file)) {
    if (IS_NIL (STRING (file))) {
      if ((FD (file) = open_physical_file (p, ref_file, A68_WRITE_ACCESS, A68_PROTECTION)) == A68_NO_FILENO) {
        open_error (p, ref_file, "putting");
      }
    } else {
      FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS, 0);
    }
    DRAW_MOOD (file) = A68_FALSE;
    READ_MOOD (file) = A68_FALSE;
    WRITE_MOOD (file) = A68_TRUE;
    CHAR_MOOD (file) = A68_TRUE;
  }
  if (!CHAR_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Save stack state since formats have frames */
  save_frame_pointer = FRAME_POINTER (file);
  save_stack_pointer = STACK_POINTER (file);
  FRAME_POINTER (file) = frame_pointer;
  STACK_POINTER (file) = stack_pointer;
/* Process [] SIMPLOUT */
  if (BODY (&FORMAT (file)) != NO_NODE) {
    open_format_frame (p, ref_file, &FORMAT (file), NOT_EMBEDDED_FORMAT, A68_FALSE);
  }
  if (elems <= 0) {
    return;
  }
  formats = 0;
  base_address = DEREF (BYTE_T, &ARRAY (arr));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_UNION *z = (A68_UNION *) & (base_address[elem_index]);
    MOID_T *mode = (MOID_T *) (VALUE (z));
    BYTE_T *item = &(base_address[elem_index + A68_UNION_SIZE]);
    if (mode == MODE (FORMAT)) {
/* Forget about eventual active formats and set up new one */
      if (formats > 0) {
        purge_format_write (p, ref_file);
      }
      formats++;
      frame_pointer = FRAME_POINTER (file);
      stack_pointer = STACK_POINTER (file);
      open_format_frame (p, ref_file, (A68_FORMAT *) item, NOT_EMBEDDED_FORMAT, A68_TRUE);
    } else if (mode == MODE (PROC_REF_FILE_VOID)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, MODE (PROC_REF_FILE_VOID));
      exit_genie (p, A68_RUNTIME_ERROR);
    } else if (mode == MODE (SOUND)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, MODE (SOUND));
      exit_genie (p, A68_RUNTIME_ERROR);
    } else {
      genie_write_standard_format (p, mode, item, ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLOUT));
  }
/* Empty the format to purge insertions */
  purge_format_write (p, ref_file);
  BODY (&FORMAT (file)) = NO_NODE;
/* Dump the buffer */
  write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
/* Forget about active formats */
  frame_pointer = FRAME_POINTER (file);
  stack_pointer = STACK_POINTER (file);
  FRAME_POINTER (file) = save_frame_pointer;
  STACK_POINTER (file) = save_stack_pointer;
}

/*!
\brief give a value error in case a character is not among expected ones
\param p position in tree
\param m mode of value read or written
\param ref_file fat pointer to A68 file
\param items expected characters
\param ch actual character
\return whether character is expected
**/

static BOOL_T expect (NODE_T * p, MOID_T * m, A68_REF ref_file, const char *items, char ch)
{
  if (a68g_strchr ((char *) items, ch) == NO_TEXT) {
    value_error (p, m, ref_file);
    return (A68_FALSE);
  } else {
    return (A68_TRUE);
  }
}

/*!
\brief read a group of insertions
\param p position in tree
\param ref_file fat pointer to A68 file
**/

void read_insertion (NODE_T * p, A68_REF ref_file)
{

/*
Algol68G does not check whether the insertions are textually there. It just
skips them. This because we blank literals in sign moulds before the sign is
put, which is non-standard Algol68, but convenient.
*/

  A68_FILE *file = FILE_DEREF (&ref_file);
  for (; p != NO_NODE; FORWARD (p)) {
    read_insertion (SUB (p), ref_file);
    if (IS (p, FORMAT_ITEM_L)) {
      BOOL_T go_on = (BOOL_T) ! END_OF_FILE (file);
      while (go_on) {
        int ch = read_single_char (p, ref_file);
        go_on = (BOOL_T) ((ch != NEWLINE_CHAR) && (ch != EOF_CHAR) && !END_OF_FILE (file));
      }
    } else if (IS (p, FORMAT_ITEM_P)) {
      BOOL_T go_on = (BOOL_T) ! END_OF_FILE (file);
      while (go_on) {
        int ch = read_single_char (p, ref_file);
        go_on = (BOOL_T) ((ch != FORMFEED_CHAR) && (ch != EOF_CHAR) && !END_OF_FILE (file));
      }
    } else if (IS (p, FORMAT_ITEM_X) || IS (p, FORMAT_ITEM_Q)) {
      if (!END_OF_FILE (file)) {
        (void) read_single_char (p, ref_file);
      }
    } else if (IS (p, FORMAT_ITEM_Y)) {
      PUSH_REF (p, ref_file);
      PUSH_PRIMITIVE (p, -1, A68_INT);
      genie_set (p);
    } else if (IS (p, LITERAL)) {
      /* Skip characters, but don't check the literal. */
      int len = (int) strlen (NSYMBOL (p));
      while (len-- && !END_OF_FILE (file)) {
        (void) read_single_char (p, ref_file);
      }
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      if (ATTRIBUTE (SUB_NEXT (p)) != FORMAT_ITEM_K) {
        for (j = 1; j <= k; j++) {
          read_insertion (NEXT (p), ref_file);
        }
      } else {
        int pos = get_transput_buffer_index (INPUT_BUFFER);
        for (j = 1; j < (k - pos); j++) {
          if (!END_OF_FILE (file)) {
            (void) read_single_char (p, ref_file);
          }
        }
      }
      return; /* Don't delete this! */
    }
  }
}

/*!
\brief read string from file according current format
\param p position in tree
\param m mode being read
\param ref_file fat pointer to A68 file
**/

static void read_string_pattern (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (IS (p, FORMAT_ITEM_A)) {
      scan_n_chars (p, 1, m, ref_file);
    } else if (IS (p, FORMAT_ITEM_S)) {
      add_char_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
      return;
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        read_string_pattern (NEXT (p), m, ref_file);
      }
      return;
    } else {
      read_string_pattern (SUB (p), m, ref_file);
    }
  }
}

/*!
\brief traverse choice pattern
\param p position in tree
\param str string to match
\param len length to match
\param count counts literals
\param matches matching literals
\param first_match first matching literal
\param full_match whether match is complete (beyond 'len')
**/

static void traverse_choice_pattern (NODE_T * p, char *str, int len, int *count, int *matches, int *first_match, BOOL_T * full_match)
{
  for (; p != NO_NODE; FORWARD (p)) {
    traverse_choice_pattern (SUB (p), str, len, count, matches, first_match, full_match);
    if (IS (p, LITERAL)) {
      (*count)++;
      if (strncmp (NSYMBOL (p), str, (size_t) len) == 0) {
        (*matches)++;
        (*full_match) = (BOOL_T) ((*full_match) | (strcmp (NSYMBOL (p), str) == 0));
        if (*first_match == 0 && *full_match) {
          *first_match = *count;
        }
      }
    }
  }
}

/*!
\brief read appropriate insertion from a choice pattern
\param p position in tree
\param ref_file fat pointer to A68 file
\return length of longest match
**/

static int read_choice_pattern (NODE_T * p, A68_REF ref_file)
{

/*
This implementation does not have the RR peculiarity that longest
matching literal must be first, in case of non-unique first chars.
*/

  A68_FILE *file = FILE_DEREF (&ref_file);
  BOOL_T cont = A68_TRUE;
  int longest_match = 0, longest_match_len = 0;
  while (cont) {
    int ch = char_scanner (file);
    if (!END_OF_FILE (file)) {
      int len, count = 0, matches = 0, first_match = 0;
      BOOL_T full_match = A68_FALSE;
      add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
      len = get_transput_buffer_index (INPUT_BUFFER);
      traverse_choice_pattern (p, get_transput_buffer (INPUT_BUFFER), len, &count, &matches, &first_match, &full_match);
      if (full_match && matches == 1 && first_match > 0) {
        return (first_match);
      } else if (full_match && matches > 1 && first_match > 0) {
        longest_match = first_match;
        longest_match_len = len;
      } else if (matches == 0) {
        cont = A68_FALSE;
      }
    } else {
      cont = A68_FALSE;
    }
  }
  if (longest_match > 0) {
/* Push back look-ahead chars */
    if (get_transput_buffer_index (INPUT_BUFFER) > 0) {
      char *z = get_transput_buffer (INPUT_BUFFER);
      END_OF_FILE (file) = A68_FALSE;
      add_string_transput_buffer (p, TRANSPUT_BUFFER (file), &z[longest_match_len]);
    }
    return (longest_match);
  } else {
    value_error (p, MODE (INT), ref_file);
    return (0);
  }
}

/*!
\brief read value according to a general-pattern
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void read_number_generic (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_REF row;
  EXECUTE_UNIT (NEXT_SUB (p));
/* RR says to ignore parameters just calculated, so we will */
  POP_REF (p, &row);
  genie_read_standard (p, mode, item, ref_file);
}

/* INTEGRAL, REAL, COMPLEX and BITS patterns */

/*!
\brief read sign-mould according current format
\param p position in tree
\param m mode of value
\param ref_file fat pointer to A68 file
\param sign value of sign (-1, 0, 1)
**/

static void read_sign_mould (NODE_T * p, MOID_T * m, A68_REF ref_file, int *sign)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        read_sign_mould (NEXT (p), m, ref_file, sign);
      }
      return;                   /* Leave this! */
    } else {
      switch (ATTRIBUTE (p)) {
      case FORMAT_ITEM_Z:
      case FORMAT_ITEM_D:
      case FORMAT_ITEM_S:
      case FORMAT_ITEM_PLUS:
      case FORMAT_ITEM_MINUS:
        {
          int ch = read_single_char (p, ref_file);
/* When a sign has been read, digits are expected */
          if (*sign != 0) {
            if (expect (p, m, ref_file, INT_DIGITS, (char) ch)) {
              add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
            } else {
              add_char_transput_buffer (p, INPUT_BUFFER, '0');
            }
/* When a sign has not been read, a sign is expected.  If there is a digit
   in stead of a sign, the digit is accepted and '+' is assumed; RR demands a
   space to preceed the digit, Algol68G does not */
          } else {
            if (a68g_strchr (SIGN_DIGITS, ch) != NO_TEXT) {
              if (ch == '+') {
                *sign = 1;
              } else if (ch == '-') {
                *sign = -1;
              } else if (ch == BLANK_CHAR) {
                /*
                 * skip. 
                 */ ;
              }
            } else if (expect (p, m, ref_file, INT_DIGITS, (char) ch)) {
              add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
              *sign = 1;
            }
          }
          break;
        }
      default:
        {
          read_sign_mould (SUB (p), m, ref_file, sign);
          break;
        }
      }
    }
  }
}

/*!
\brief read mould according current format
\param p position in tree
\param m mode of value
\param ref_file fat pointer to A68 file
**/

static void read_integral_mould (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        read_integral_mould (NEXT (p), m, ref_file);
      }
      return;                   /* Leave this! */
    } else if (IS (p, FORMAT_ITEM_Z)) {
      int ch = read_single_char (p, ref_file);
      const char *digits = (m == MODE (BITS) || m == MODE (LONG_BITS) || m == MODE (LONGLONG_BITS)) ? BITS_DIGITS_BLANK : INT_DIGITS_BLANK;
      if (expect (p, m, ref_file, digits, (char) ch)) {
        add_char_transput_buffer (p, INPUT_BUFFER, (char) ((ch == BLANK_CHAR) ? '0' : ch));
      } else {
        add_char_transput_buffer (p, INPUT_BUFFER, '0');
      }
    } else if (IS (p, FORMAT_ITEM_D)) {
      int ch = read_single_char (p, ref_file);
      const char *digits = (m == MODE (BITS) || m == MODE (LONG_BITS) || m == MODE (LONGLONG_BITS)) ? BITS_DIGITS : INT_DIGITS;
      if (expect (p, m, ref_file, digits, (char) ch)) {
        add_char_transput_buffer (p, INPUT_BUFFER, (char) ch);
      } else {
        add_char_transput_buffer (p, INPUT_BUFFER, '0');
      }
    } else if (IS (p, FORMAT_ITEM_S)) {
      add_char_transput_buffer (p, INPUT_BUFFER, '0');
    } else {
      read_integral_mould (SUB (p), m, ref_file);
    }
  }
}

/*!
\brief read mould according current format
\param p position in tree
\param m mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void read_integral_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
  NODE_T *q = SUB (p);
  if (q != NO_NODE && IS (q, SIGN_MOULD)) {
    int sign = 0;
    char *z;
    add_char_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
    read_sign_mould (SUB (q), m, ref_file, &sign);
    z = get_transput_buffer (INPUT_BUFFER);
    z[0] = (char) ((sign == -1) ? '-' : '+');
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
  }
  genie_string_to_value (p, m, item, ref_file);
}

/*!
\brief read point, exponent or i-frame
\param p position in tree
\param m mode of value
\param ref_file fat pointer to A68 file
\param att frame attribute
\param item format item
\param ch representation of 'item'
**/

static void read_pie_frame (NODE_T * p, MOID_T * m, A68_REF ref_file, int att, int item, char ch)
{
/* Widen ch to a stringlet */
  char sym[3];
  sym[0] = ch;
  sym[1] = (char) TO_LOWER (ch);
  sym[2] = NULL_CHAR;
/* Now read the frame */
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      read_insertion (p, ref_file);
    } else if (IS (p, att)) {
      read_pie_frame (SUB (p), m, ref_file, att, item, ch);
      return;
    } else if (IS (p, FORMAT_ITEM_S)) {
      add_char_transput_buffer (p, INPUT_BUFFER, sym[0]);
      return;
    } else if (IS (p, item)) {
      int ch0 = read_single_char (p, ref_file);
      if (expect (p, m, ref_file, sym, (char) ch0)) {
        add_char_transput_buffer (p, INPUT_BUFFER, sym[0]);
      } else {
        add_char_transput_buffer (p, INPUT_BUFFER, sym[0]);
      }
    }
  }
}

/*!
\brief read REAL value using real pattern
\param p position in tree
\param m mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void read_real_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
/* Dive into pattern */
  NODE_T *q = (IS (p, REAL_PATTERN)) ? SUB (p) : p;
/* Dissect pattern */
  if (q != NO_NODE && IS (q, SIGN_MOULD)) {
    int sign = 0;
    char *z;
    add_char_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
    read_sign_mould (SUB (q), m, ref_file, &sign);
    z = get_transput_buffer (INPUT_BUFFER);
    z[0] = (char) ((sign == -1) ? '-' : '+');
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, FORMAT_POINT_FRAME)) {
    read_pie_frame (SUB (q), m, ref_file, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT, POINT_CHAR);
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, EXPONENT_FRAME)) {
    read_pie_frame (SUB (q), m, ref_file, FORMAT_E_FRAME, FORMAT_ITEM_E, EXPONENT_CHAR);
    q = NEXT_SUB (q);
    if (q != NO_NODE && IS (q, SIGN_MOULD)) {
      int k, sign = 0;
      char *z;
      add_char_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
      k = get_transput_buffer_index (INPUT_BUFFER);
      read_sign_mould (SUB (q), m, ref_file, &sign);
      z = get_transput_buffer (INPUT_BUFFER);
      z[k - 1] = (char) ((sign == -1) ? '-' : '+');
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
      read_integral_mould (SUB (q), m, ref_file);
      FORWARD (q);
    }
  }
  genie_string_to_value (p, m, item, ref_file);
}

/*!
\brief read COMPLEX value using complex pattern
\param p position in tree
\param comp mode of complex value
\param m mode of value fields
\param re pointer to real part
\param im pointer to imaginary part
\param ref_file fat pointer to A68 file
**/

static void read_complex_pattern (NODE_T * p, MOID_T * comp, MOID_T * m, BYTE_T * re, BYTE_T * im, A68_REF ref_file)
{
  NODE_T *reel, *plus_i_times, *imag;
/* Dissect pattern */
  reel = SUB (p);
  plus_i_times = NEXT (reel);
  imag = NEXT (plus_i_times);
/* Read pattern */
  read_real_pattern (reel, m, re, ref_file);
  reset_transput_buffer (INPUT_BUFFER);
  read_pie_frame (plus_i_times, comp, ref_file, FORMAT_I_FRAME, FORMAT_ITEM_I, 'I');
  reset_transput_buffer (INPUT_BUFFER);
  read_real_pattern (imag, m, im, ref_file);
}

/*!
\brief read BITS value according pattern
\param p position in tree
\param m mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void read_bits_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
  int radix;
  char *z;
  radix = get_replicator_value (SUB_SUB (p), A68_TRUE);
  if (radix < 2 || radix > 16) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  z = get_transput_buffer (INPUT_BUFFER);
  ASSERT (snprintf (z, (size_t) TRANSPUT_BUFFER_SIZE, "%dr", radix) >= 0);
  set_transput_buffer_index (INPUT_BUFFER, (int) strlen (z));
  read_integral_mould (NEXT_SUB (p), m, ref_file);
  genie_string_to_value (p, m, item, ref_file);
}

/*!
\brief read object with from file and store
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void genie_read_real_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) == NO_NODE) {
    genie_read_standard (p, mode, item, ref_file);
  } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
    read_number_generic (p, mode, item, ref_file);
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    read_c_pattern (p, mode, item, ref_file);
  } else if (IS (p, REAL_PATTERN)) {
    read_real_pattern (p, mode, item, ref_file);
  } else {
    pattern_error (p, mode, ATTRIBUTE (p));
  }
}

/*!
\brief read object with from file and store
\param p position in tree
\param mode mode of value
\param item pointer to value
\param ref_file fat pointer to A68 file
**/

static void genie_read_standard_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  RESET_ERRNO;
  reset_transput_buffer (INPUT_BUFFER);
  if (mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (pat, mode, item, ref_file);
    } else if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NO_NODE) {
      read_number_generic (pat, mode, item, ref_file);
    } else if (IS (pat, INTEGRAL_C_PATTERN)) {
      read_c_pattern (pat, mode, item, ref_file);
    } else if (IS (pat, INTEGRAL_PATTERN)) {
      read_integral_pattern (pat, mode, item, ref_file);
    } else if (IS (pat, CHOICE_PATTERN)) {
      int k = read_choice_pattern (pat, ref_file);
      if (mode == MODE (INT)) {
        A68_INT *z = (A68_INT *) item;
        VALUE (z) = k;
        STATUS (z) = (STATUS_MASK) ((VALUE (z) > 0) ? INIT_MASK : NULL_MASK);
      } else {
        MP_T *z = (MP_T *) item;
        if (k > 0) {
          (void) int_to_mp (p, z, k, get_mp_digits (mode));
          z[0] = (MP_T) INIT_MASK;
        } else {
          z[0] = (MP_T) NULL_MASK;
        }
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_read_real_format (pat, mode, item, ref_file);
  } else if (mode == MODE (COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, MODE (REAL), item, &item[MOID_SIZE (MODE (REAL))], ref_file);
    } else {
/* Try reading as two REAL values */
      genie_read_real_format (pat, MODE (REAL), item, ref_file);
      genie_read_standard_format (p, MODE (REAL), &item[MOID_SIZE (MODE (REAL))], ref_file);
    }
  } else if (mode == MODE (LONG_COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, MODE (LONG_REAL), item, &item[MOID_SIZE (MODE (LONG_REAL))], ref_file);
    } else {
/* Try reading as two LONG REAL values */
      genie_read_real_format (pat, MODE (LONG_REAL), item, ref_file);
      genie_read_standard_format (p, MODE (LONG_REAL), &item[MOID_SIZE (MODE (LONG_REAL))], ref_file);
    }
  } else if (mode == MODE (LONGLONG_COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, MODE (LONGLONG_REAL), item, &item[MOID_SIZE (MODE (LONGLONG_REAL))], ref_file);
    } else {
/* Try reading as two LONG LONG REAL values */
      genie_read_real_format (pat, MODE (LONGLONG_REAL), item, ref_file);
      genie_read_standard_format (p, MODE (LONGLONG_REAL), &item[MOID_SIZE (MODE (LONGLONG_REAL))], ref_file);
    }
  } else if (mode == MODE (BOOL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (IS (pat, BOOLEAN_PATTERN)) {
      if (NEXT_SUB (pat) == NO_NODE) {
        genie_read_standard (p, mode, item, ref_file);
      } else {
        A68_BOOL *z = (A68_BOOL *) item;
        int k = read_choice_pattern (pat, ref_file);
        if (k == 1 || k == 2) {
          VALUE (z) = (BOOL_T) ((k == 1) ? A68_TRUE : A68_FALSE);
          STATUS (z) = INIT_MASK;
        } else {
          STATUS (z) = NULL_MASK;
        }
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (BITS) || mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (IS (pat, BITS_PATTERN)) {
      read_bits_pattern (pat, mode, item, ref_file);
    } else if (IS (pat, BITS_C_PATTERN)) {
      read_c_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (CHAR)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (IS (pat, STRING_PATTERN)) {
      read_string_pattern (pat, MODE (CHAR), ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    } else if (IS (pat, CHAR_C_PATTERN)) {
      read_c_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
/* Handle these separately instead of reading [] CHAR */
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (IS (pat, STRING_PATTERN)) {
      read_string_pattern (pat, mode, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    } else if (IS (pat, STRING_C_PATTERN)) {
      read_c_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (IS (mode, UNION_SYMBOL)) {
    A68_UNION *z = (A68_UNION *) item;
    genie_read_standard_format (p, (MOID_T *) (VALUE (z)), &item[A68_UNION_SIZE], ref_file);
  } else if (IS (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NO_PACK; FORWARD (q)) {
      BYTE_T *elem = &item[OFFSET (q)];
      genie_read_standard_format (p, MOID (q), elem, ref_file);
    }
  } else if (IS (mode, ROW_SYMBOL) || IS (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    CHECK_INIT (p, INITIALISED ((A68_REF *) item), MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, DIM (arr)) > 0) {
      BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
      BOOL_T done = A68_FALSE;
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        ADDR_T a68g_index = calculate_internal_index (tup, DIM (arr));
        ADDR_T elem_addr = ROW_ELEMENT (arr, a68g_index);
        BYTE_T *elem = &base_addr[elem_addr];
        genie_read_standard_format (p, SUB (deflexed), elem, ref_file);
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief at end of read purge all insertions
\param p position in tree
\param ref_file fat pointer to A68 file
**/

static void purge_format_read (NODE_T * p, A68_REF ref_file)
{
  BOOL_T go_on;
  do {
    A68_FILE *file;
    NODE_T *dollar, *pat;
    A68_FORMAT *old_fmt;
/*
    while (get_next_format_pattern (p, ref_file, SKIP_PATTERN) != NO_NODE) {
	;
    }
*/
    while ((pat = get_next_format_pattern (p, ref_file, SKIP_PATTERN)) != NO_NODE) {
      format_error (p, ref_file, ERROR_FORMAT_PICTURES);
    }
    file = FILE_DEREF (&ref_file);
    dollar = SUB (BODY (&FORMAT (file)));
    old_fmt = (A68_FORMAT *) FRAME_LOCAL (frame_pointer, OFFSET (TAX (dollar)));
    go_on = (BOOL_T) ! IS_NIL_FORMAT (old_fmt);
    if (go_on) {
/* Pop embedded format and proceed */
      (void) end_of_format (p, ref_file);
    }
  } while (go_on);
}

/*!
\brief PROC ([] SIMPLIN) VOID read f
\param p position in tree
**/

void genie_read_format (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_in (p);
  PUSH_REF (p, row);
  genie_read_file_format (p);
}

/*!
\brief PROC (REF FILE, [] SIMPLIN) VOID get f
\param p position in tree
**/

void genie_read_file_format (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index, formats;
  ADDR_T save_frame_pointer, save_stack_pointer;
  POP_REF (p, &row);
  CHECK_REF (p, row, MODE (ROW_SIMPLIN));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), MODE (FILE));
  if (!OPENED (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!GET (&CHANNEL (file))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "getting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!READ_MOOD (file) && !WRITE_MOOD (file)) {
    if (IS_NIL (STRING (file))) {
      if ((FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS, 0)) == A68_NO_FILENO) {
        open_error (p, ref_file, "getting");
      }
    } else {
      FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS, 0);
    }
    DRAW_MOOD (file) = A68_FALSE;
    READ_MOOD (file) = A68_TRUE;
    WRITE_MOOD (file) = A68_FALSE;
    CHAR_MOOD (file) = A68_TRUE;
  }
  if (!CHAR_MOOD (file)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Save stack state since formats have frames */
  save_frame_pointer = FRAME_POINTER (file);
  save_stack_pointer = STACK_POINTER (file);
  FRAME_POINTER (file) = frame_pointer;
  STACK_POINTER (file) = stack_pointer;
/* Process [] SIMPLIN */
  if (BODY (&FORMAT (file)) != NO_NODE) {
    open_format_frame (p, ref_file, &FORMAT (file), NOT_EMBEDDED_FORMAT, A68_FALSE);
  }
  if (elems <= 0) {
    return;
  }
  formats = 0;
  base_address = DEREF (BYTE_T, &ARRAY (arr));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_UNION *z = (A68_UNION *) & (base_address[elem_index]);
    MOID_T *mode = (MOID_T *) (VALUE (z));
    BYTE_T *item = (BYTE_T *) & (base_address[elem_index + A68_UNION_SIZE]);
    if (mode == MODE (FORMAT)) {
/* Forget about eventual active formats and set up new one */
      if (formats > 0) {
        purge_format_read (p, ref_file);
      }
      formats++;
      frame_pointer = FRAME_POINTER (file);
      stack_pointer = STACK_POINTER (file);
      open_format_frame (p, ref_file, (A68_FORMAT *) item, NOT_EMBEDDED_FORMAT, A68_TRUE);
    } else if (mode == MODE (PROC_REF_FILE_VOID)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, MODE (PROC_REF_FILE_VOID));
      exit_genie (p, A68_RUNTIME_ERROR);
    } else if (mode == MODE (REF_SOUND)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, MODE (REF_SOUND));
      exit_genie (p, A68_RUNTIME_ERROR);
    } else {
      CHECK_REF (p, *(A68_REF *) item, mode);
      genie_read_standard_format (p, SUB (mode), ADDRESS ((A68_REF *) item), ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLIN));
  }
/* Empty the format to purge insertions */
  purge_format_read (p, ref_file);
  BODY (&FORMAT (file)) = NO_NODE;
/* Forget about active formats */
  frame_pointer = FRAME_POINTER (file);
  stack_pointer = STACK_POINTER (file);
  FRAME_POINTER (file) = save_frame_pointer;
  STACK_POINTER (file) = save_stack_pointer;
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
  return log (y) - ((y - 1) - x) / y;   /* cancel errors with IEEE arithmetic */
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
      int expo = 1, m = (int) labs (n);
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
    STATUS_RE (z) = INIT_MASK;
    STATUS_IM (z) = INIT_MASK;
    RE (z) = (RE (x) + r * IM (x)) / den;
    IM (z) = (IM (x) - r * RE (x)) / den;
  } else {
    double r = RE (y) / IM (y), den = IM (y) + r * RE (y);
    STATUS_RE (z) = INIT_MASK;
    STATUS_IM (z) = INIT_MASK;
    RE (z) = (RE (x) * r + IM (x)) / den;
    IM (z) = (IM (x) * r - RE (x)) / den;
  }
}

/*!
\brief PROC csqrt = (COMPLEX) COMPLEX
**/

void a68g_sqrt_complex (A68_REAL * z, A68_REAL * x)
{
  STATUS_RE (z) = INIT_MASK;
  STATUS_IM (z) = INIT_MASK;
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
  STATUS_RE (z) = INIT_MASK;
  STATUS_IM (z) = INIT_MASK;
  RE (z) = r * cos (IM (x));
  IM (z) = r * sin (IM (x));
}

/*!
\brief PROC cln = (COMPLEX) COMPLEX
**/

void a68g_ln_complex (A68_REAL * z, A68_REAL * x)
{
  STATUS_RE (z) = INIT_MASK;
  STATUS_IM (z) = INIT_MASK;
  RE (z) = log (a68g_abs_complex (x));
  IM (z) = a68g_arg_complex (x);
}


/*!
\brief PROC csin = (COMPLEX) COMPLEX
**/

void a68g_sin_complex (A68_REAL * z, A68_REAL * x)
{
  STATUS_RE (z) = INIT_MASK;
  STATUS_IM (z) = INIT_MASK;
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
  STATUS_RE (z) = INIT_MASK;
  STATUS_IM (z) = INIT_MASK;
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
  STATUS_RE (u) = INIT_MASK;
  STATUS_IM (u) = INIT_MASK;
  STATUS_RE (v) = INIT_MASK;
  STATUS_IM (v) = INIT_MASK;
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

/* Operators for ROWS */

/*!
\brief OP ELEMS = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack */
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, get_row_size (t, DIM (x)), A68_INT);
}

/*!
\brief OP LWB = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack */
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, LWB (t), A68_INT);
}

/*!
\brief OP UPB = (ROWS) INT
\param p position in syntax tree
**/

void genie_monad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack */
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, MODE (ROWS));
  GET_DESCRIPTOR (x, t, &z);
  PUSH_PRIMITIVE (p, UPB (t), A68_INT);
}

/*!
\brief OP ELEMS = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t, *u;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack */
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  u = &(t[VALUE (&k) - 1]);
  PUSH_PRIMITIVE (p, ROW_SIZE (u), A68_INT);
}

/*!
\brief OP LWB = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack */
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, LWB (&(t[VALUE (&k) - 1])), A68_INT);
}

/*!
\brief OP UPB = (INT, ROWS) INT
\param p position in syntax tree
**/

void genie_dyad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
/* Decrease pointer since a UNION is on the stack */
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, MODE (ROWS));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, UPB (&(t[VALUE (&k) - 1])), A68_INT);
}

/*
Implements SOUND values.
*/

#define MAX_BYTES 4
#define A68_LITTLE_ENDIAN A68_TRUE
#define A68_BIG_ENDIAN A68_FALSE

/* From public Microsoft RIFF documentation */

#define	WAVE_FORMAT_UNKNOWN		(0x0000)
#define	WAVE_FORMAT_PCM			(0x0001)
#define	WAVE_FORMAT_ADPCM		(0x0002)
#define WAVE_FORMAT_IEEE_FLOAT          (0x0003)
#define WAVE_FORMAT_IBM_FORMAT_CVSD	(0x0005)
#define	WAVE_FORMAT_ALAW		(0x0006)
#define	WAVE_FORMAT_MULAW		(0x0007)
#define	WAVE_FORMAT_OKI_ADPCM		(0x0010)
#define WAVE_FORMAT_DVI_ADPCM		(0x0011)
#define WAVE_FORMAT_MEDIASPACE_ADPCM	(0x0012)
#define WAVE_FORMAT_SIERRA_ADPCM	(0x0013)
#define WAVE_FORMAT_G723_ADPCM		(0X0014)
#define	WAVE_FORMAT_DIGISTD		(0x0015)
#define	WAVE_FORMAT_DIGIFIX		(0x0016)
#define WAVE_FORMAT_YAMAHA_ADPCM	(0x0020)
#define WAVE_FORMAT_SONARC		(0x0021)
#define WAVE_FORMAT_DSPGROUP_TRUESPEECH	(0x0022)
#define WAVE_FORMAT_ECHOSCI1		(0x0023)
#define WAVE_FORMAT_AUDIOFILE_AF36	(0x0024)
#define WAVE_FORMAT_APTX		(0x0025)
#define WAVE_FORMAT_AUDIOFILE_AF10	(0x0026)
#define WAVE_FORMAT_DOLBY_AC2           (0x0030)
#define WAVE_FORMAT_GSM610              (0x0031)
#define WAVE_FORMAT_ANTEX_ADPCME	(0x0033)
#define WAVE_FORMAT_CONTROL_RES_VQLPC	(0x0034)
#define WAVE_FORMAT_DIGIREAL		(0x0035)
#define WAVE_FORMAT_DIGIADPCM		(0x0036)
#define WAVE_FORMAT_CONTROL_RES_CR10	(0x0037)
#define WAVE_FORMAT_NMS_VBXADPCM	(0x0038)
#define WAVE_FORMAT_ROCKWELL_ADPCM      (0x003b)
#define WAVE_FORMAT_ROCKWELL_DIGITALK   (0x003c)
#define WAVE_FORMAT_G721_ADPCM          (0x0040)
#define WAVE_FORMAT_G728_CELP           (0x0041)
#define WAVE_FORMAT_MPEG                (0x0050)
#define WAVE_FORMAT_MPEGLAYER3          (0x0055)
#define WAVE_FORMAT_G726_ADPCM          (0x0064)
#define WAVE_FORMAT_G722_ADPCM          (0x0065)
#define WAVE_FORMAT_IBM_FORMAT_MULAW	(0x0101)
#define WAVE_FORMAT_IBM_FORMAT_ALAW	(0x0102)
#define WAVE_FORMAT_IBM_FORMAT_ADPCM	(0x0103)
#define WAVE_FORMAT_CREATIVE_ADPCM	(0x0200)
#define WAVE_FORMAT_FM_TOWNS_SND	(0x0300)
#define WAVE_FORMAT_OLIGSM		(0x1000)
#define WAVE_FORMAT_OLIADPCM		(0x1001)
#define WAVE_FORMAT_OLICELP		(0x1002)
#define WAVE_FORMAT_OLISBC		(0x1003)
#define WAVE_FORMAT_OLIOPR		(0x1004)
#define WAVE_FORMAT_EXTENSIBLE          (0xfffe)

static unsigned pow256[] = { 1, 256, 65536, 16777216 };

/*!
\brief test bits per sample
\param p position in tree
\param bps bits per second
**/

static void test_bits_per_sample (NODE_T * p, unsigned bps)
{
  if (bps <= 0 || bps > 24) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "unsupported number of bits per sample");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief code string into big-endian unsigned
\param p position in tree
\param s string to code
\param n chars to code
**/

static unsigned code_string (NODE_T * p, char *s, int n)
{
  unsigned v;
  int k, m;
  if (n > MAX_BYTES) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "too long word length");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (k = 0, m = n - 1, v = 0; k < n; k++, m--) {
    v += ((unsigned) s[k]) * pow256[m];
  } return (v);
}

/*!
\brief code unsigned into string
\param p position in tree
\param n value to code
**/

static char *code_unsigned (NODE_T * p, unsigned n)
{
  static char text[MAX_BYTES + 1];
  int k;
  (void) p;
  for (k = 0; k < MAX_BYTES; k++) {
    char ch = (char) (n % 0x100);
    if (ch == NULL_CHAR) {
      ch = BLANK_CHAR;
    } else if (ch < BLANK_CHAR) {
      ch = '?';
    }
    text[MAX_BYTES - k - 1] = ch;
    n >>= 8;
  }
  text[MAX_BYTES] = NULL_CHAR;
  return (text);
}

/*!
\brief WAVE format category
\param n category number
**/

static char *format_category (unsigned n)
{
  switch (n) {
  case WAVE_FORMAT_UNKNOWN:
    {
      return ("WAVE_FORMAT_UNKNOWN");
    }
  case WAVE_FORMAT_PCM:
    {
      return ("WAVE_FORMAT_PCM	");
    }
  case WAVE_FORMAT_ADPCM:
    {
      return ("WAVE_FORMAT_ADPCM");
    }
  case WAVE_FORMAT_IEEE_FLOAT:
    {
      return ("WAVE_FORMAT_IEEE_FLOAT");
    }
  case WAVE_FORMAT_IBM_FORMAT_CVSD:
    {
      return ("WAVE_FORMAT_IBM_FORMAT_CVSD");
    }
  case WAVE_FORMAT_ALAW:
    {
      return ("WAVE_FORMAT_ALAW");
    }
  case WAVE_FORMAT_MULAW:
    {
      return ("WAVE_FORMAT_MULAW");
    }
  case WAVE_FORMAT_OKI_ADPCM:
    {
      return ("WAVE_FORMAT_OKI_ADPCM");
    }
  case WAVE_FORMAT_DVI_ADPCM:
    {
      return ("WAVE_FORMAT_DVI_ADPCM");
    }
  case WAVE_FORMAT_MEDIASPACE_ADPCM:
    {
      return ("WAVE_FORMAT_MEDIASPACE_ADPCM");
    }
  case WAVE_FORMAT_SIERRA_ADPCM:
    {
      return ("WAVE_FORMAT_SIERRA_ADPCM");
    }
  case WAVE_FORMAT_G723_ADPCM:
    {
      return ("WAVE_FORMAT_G723_ADPCM");
    }
  case WAVE_FORMAT_DIGISTD:
    {
      return ("WAVE_FORMAT_DIGISTD");
    }
  case WAVE_FORMAT_DIGIFIX:
    {
      return ("WAVE_FORMAT_DIGIFIX");
    }
  case WAVE_FORMAT_YAMAHA_ADPCM:
    {
      return ("WAVE_FORMAT_YAMAHA_ADPCM");
    }
  case WAVE_FORMAT_SONARC:
    {
      return ("WAVE_FORMAT_SONARC");
    }
  case WAVE_FORMAT_DSPGROUP_TRUESPEECH:
    {
      return ("WAVE_FORMAT_DSPGROUP_TRUESPEECH");
    }
  case WAVE_FORMAT_ECHOSCI1:
    {
      return ("WAVE_FORMAT_ECHOSCI1");
    }
  case WAVE_FORMAT_AUDIOFILE_AF36:
    {
      return ("WAVE_FORMAT_AUDIOFILE_AF36");
    }
  case WAVE_FORMAT_APTX:
    {
      return ("WAVE_FORMAT_APTX");
    }
  case WAVE_FORMAT_AUDIOFILE_AF10:
    {
      return ("WAVE_FORMAT_AUDIOFILE_AF10");
    }
  case WAVE_FORMAT_DOLBY_AC2:
    {
      return ("WAVE_FORMAT_DOLBY_AC2");
    }
  case WAVE_FORMAT_GSM610:
    {
      return ("WAVE_FORMAT_GSM610 ");
    }
  case WAVE_FORMAT_ANTEX_ADPCME:
    {
      return ("WAVE_FORMAT_ANTEX_ADPCME");
    }
  case WAVE_FORMAT_CONTROL_RES_VQLPC:
    {
      return ("WAVE_FORMAT_CONTROL_RES_VQLPC");
    }
  case WAVE_FORMAT_DIGIREAL:
    {
      return ("WAVE_FORMAT_DIGIREAL");
    }
  case WAVE_FORMAT_DIGIADPCM:
    {
      return ("WAVE_FORMAT_DIGIADPCM");
    }
  case WAVE_FORMAT_CONTROL_RES_CR10:
    {
      return ("WAVE_FORMAT_CONTROL_RES_CR10");
    }
  case WAVE_FORMAT_NMS_VBXADPCM:
    {
      return ("WAVE_FORMAT_NMS_VBXADPCM");
    }
  case WAVE_FORMAT_ROCKWELL_ADPCM:
    {
      return ("WAVE_FORMAT_ROCKWELL_ADPCM");
    }
  case WAVE_FORMAT_ROCKWELL_DIGITALK:
    {
      return ("WAVE_FORMAT_ROCKWELL_DIGITALK");
    }
  case WAVE_FORMAT_G721_ADPCM:
    {
      return ("WAVE_FORMAT_G721_ADPCM");
    }
  case WAVE_FORMAT_G728_CELP:
    {
      return ("WAVE_FORMAT_G728_CELP");
    }
  case WAVE_FORMAT_MPEG:
    {
      return ("WAVE_FORMAT_MPEG");
    }
  case WAVE_FORMAT_MPEGLAYER3:
    {
      return ("WAVE_FORMAT_MPEGLAYER3");
    }
  case WAVE_FORMAT_G726_ADPCM:
    {
      return ("WAVE_FORMAT_G726_ADPCM");
    }
  case WAVE_FORMAT_G722_ADPCM:
    {
      return ("WAVE_FORMAT_G722_ADPCM");
    }
  case WAVE_FORMAT_IBM_FORMAT_MULAW:
    {
      return ("WAVE_FORMAT_IBM_FORMAT_MULAW");
    }
  case WAVE_FORMAT_IBM_FORMAT_ALAW:
    {
      return ("WAVE_FORMAT_IBM_FORMAT_ALAW");
    }
  case WAVE_FORMAT_IBM_FORMAT_ADPCM:
    {
      return ("WAVE_FORMAT_IBM_FORMAT_ADPCM");
    }
  case WAVE_FORMAT_CREATIVE_ADPCM:
    {
      return ("WAVE_FORMAT_CREATIVE_ADPCM");
    }
  case WAVE_FORMAT_FM_TOWNS_SND:
    {
      return ("WAVE_FORMAT_FM_TOWNS_SND");
    }
  case WAVE_FORMAT_OLIGSM:
    {
      return ("WAVE_FORMAT_OLIGSM");
    }
  case WAVE_FORMAT_OLIADPCM:
    {
      return ("WAVE_FORMAT_OLIADPCM");
    }
  case WAVE_FORMAT_OLICELP:
    {
      return ("WAVE_FORMAT_OLICELP");
    }
  case WAVE_FORMAT_OLISBC:
    {
      return ("WAVE_FORMAT_OLISBC");
    }
  case WAVE_FORMAT_OLIOPR:
    {
      return ("WAVE_FORMAT_OLIOPR");
    }
  case WAVE_FORMAT_EXTENSIBLE:
    {
      return ("WAVE_FORMAT_EXTENSIBLE");
    }
  default:
    {
      return ("other");
    }
  }
}

/*!
\brief read RIFF item
\param p position in tree
\param fd file number
\param n word length
\param little whether little-endian
**/

static unsigned read_riff_item (NODE_T * p, FILE_T fd, int n, BOOL_T little)
{
  unsigned v, z;
  int k, m, r;
  if (n > MAX_BYTES) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "too long word length");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (little) {
    for (k = 0, m = 0, v = 0; k < n; k++, m++) {
      z = 0;
      r = (int) io_read (fd, &z, (size_t) 1);
      if (r != 1 || errno != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "error while reading file");
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      v += z * pow256[m];
    }
  } else {
    for (k = 0, m = n - 1, v = 0; k < n; k++, m--) {
      z = 0;
      r = (int) io_read (fd, &z, (size_t) 1);
      if (r != 1 || errno != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "error while reading file");
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      v += z * pow256[m];
    }
  }
  return (v);
}

/*!
\brief read sound from file
\param p position in tree
\param ref_file pointer to file
\param w sound object
**/

void read_sound (NODE_T * p, A68_REF ref_file, A68_SOUND * w)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int r;
  unsigned fmt_cat;
  unsigned blockalign, byterate, chunksize, subchunk2size, z;
  BOOL_T data_read = A68_FALSE;
  if (read_riff_item (p, FD (f), 4, A68_BIG_ENDIAN) != code_string (p, "RIFF", 4)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "file format is not RIFF");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  chunksize = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
  if ((z = read_riff_item (p, FD (f), 4, A68_BIG_ENDIAN)) != code_string (p, "WAVE", 4)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL_STRING, MODE (SOUND), "file format is not \"WAVE\" but", code_unsigned (p, z));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Now read chunks */
  while (data_read == A68_FALSE) {
    z = read_riff_item (p, FD (f), 4, A68_BIG_ENDIAN);
    if (z == code_string (p, "fmt ", 4)) {
/* Read fmt chunk */
      int k, skip;
      z = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      skip = (int) z - 0x10;    /* Bytes to skip in extended wave format */
      fmt_cat = read_riff_item (p, FD (f), 2, A68_LITTLE_ENDIAN);
      if (fmt_cat != WAVE_FORMAT_PCM) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL_STRING, MODE (SOUND), "category is not WAVE_FORMAT_PCM but", format_category (fmt_cat));
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      NUM_CHANNELS (w) = read_riff_item (p, FD (f), 2, A68_LITTLE_ENDIAN);
      SAMPLE_RATE (w) = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      byterate = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      blockalign = read_riff_item (p, FD (f), 2, A68_LITTLE_ENDIAN);
      BITS_PER_SAMPLE (w) = read_riff_item (p, FD (f), 2, A68_LITTLE_ENDIAN);
      test_bits_per_sample (p, BITS_PER_SAMPLE (w));
      for (k = 0; k < skip; k++) {
        z = read_riff_item (p, FD (f), 1, A68_LITTLE_ENDIAN);
      }
    } else if (z == code_string (p, "LIST", 4)) {
/* Skip a LIST chunk */
      int k, skip;
      z = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      skip = (int) z;
      for (k = 0; k < skip; k++) {
        z = read_riff_item (p, FD (f), 1, A68_LITTLE_ENDIAN);
      }
    } else if (z == code_string (p, "cue ", 4)) {
/* Skip a cue chunk */
      int k, skip;
      z = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      skip = (int) z;
      for (k = 0; k < skip; k++) {
        z = read_riff_item (p, FD (f), 1, A68_LITTLE_ENDIAN);
      }
    } else if (z == code_string (p, "fact", 4)) {
/* Skip a fact chunk */
      int k, skip;
      z = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      skip = (int) z;
      for (k = 0; k < skip; k++) {
        z = read_riff_item (p, FD (f), 1, A68_LITTLE_ENDIAN);
      }
    } else if (z == code_string (p, "data", 4)) {
/* Read data chunk */
      subchunk2size = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      NUM_SAMPLES (w) = subchunk2size / NUM_CHANNELS (w) / (unsigned) A68_SOUND_BYTES (w);
      DATA (w) = heap_generator (p, MODE (SOUND_DATA), (int) subchunk2size);
      r = (int) io_read (FD (f), ADDRESS (&(DATA (w))), subchunk2size);
      if (r != (int) subchunk2size) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "cannot read all of the data");
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      data_read = A68_TRUE;
    } else {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL_STRING, MODE (SOUND), "chunk is", code_unsigned (p, z));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  STATUS (w) = INIT_MASK;
}

/*!
\brief write RIFF item
\param p position in tree
\param fd file number
\param z item
\param n number of chars
\param little whether little endian
**/

void write_riff_item (NODE_T * p, FILE_T fd, unsigned z, int n, BOOL_T little)
{
  int k, r;
  unsigned char y[MAX_BYTES];
  if (n > MAX_BYTES) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "too long word length");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (k = 0; k < n; k++) {
    y[k] = (unsigned char) (z & 0xff);
    z >>= 8;
  }
  if (little) {
    for (k = 0; k < n; k++) {
      ASSERT (io_write (fd, &(y[k]), 1) != -1);
    }
  } else {
    for (k = n - 1; k >= 0; k--) {
      r = (int) io_write (fd, &(y[k]), 1);
      if (r != 1) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "error while writing file");
        exit_genie (p, A68_RUNTIME_ERROR);
      }
    }
  }
}

/*!
\brief write sound to file
\param p position in tree
\param ref_file pointer to file
\param w sound object
**/

void write_sound (NODE_T * p, A68_REF ref_file, A68_SOUND * w)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int r;
  unsigned blockalign = NUM_CHANNELS (w) * (unsigned) (A68_SOUND_BYTES (w));
  unsigned byterate = SAMPLE_RATE (w) * blockalign;
  unsigned subchunk2size = NUM_SAMPLES (w) * blockalign;
  unsigned chunksize = 4 + (8 + 16) + (8 + subchunk2size);
  write_riff_item (p, FD (f), code_string (p, "RIFF", 4), 4, A68_BIG_ENDIAN);
  write_riff_item (p, FD (f), chunksize, 4, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), code_string (p, "WAVE", 4), 4, A68_BIG_ENDIAN);
  write_riff_item (p, FD (f), code_string (p, "fmt ", 4), 4, A68_BIG_ENDIAN);
  write_riff_item (p, FD (f), 16, 4, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), 1, 2, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), NUM_CHANNELS (w), 2, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), SAMPLE_RATE (w), 4, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), byterate, 4, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), blockalign, 2, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), BITS_PER_SAMPLE (w), 2, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), code_string (p, "data", 4), 4, A68_BIG_ENDIAN);
  write_riff_item (p, FD (f), subchunk2size, 4, A68_LITTLE_ENDIAN);
  if (IS_NIL (DATA (w))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "sound has no data");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  r = (int) io_write (FD (f), ADDRESS (&(DATA (w))), subchunk2size);
  if (r != (int) subchunk2size) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "error while writing file");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief PROC new sound = (INT bits, INT sample rate, INT channels, INT samples) SOUND
\param p position in tree
**/

void genie_new_sound (NODE_T * p)
{
  A68_INT num_channels, sample_rate, bits_per_sample, num_samples;
  A68_SOUND w;
  POP_OBJECT (p, &num_samples, A68_INT);
  POP_OBJECT (p, &num_channels, A68_INT);
  POP_OBJECT (p, &sample_rate, A68_INT);
  POP_OBJECT (p, &bits_per_sample, A68_INT);
  NUM_SAMPLES (&w) = (unsigned) (VALUE (&num_samples));
  NUM_CHANNELS (&w) = (unsigned) (VALUE (&num_channels));
  SAMPLE_RATE (&w) = (unsigned) (VALUE (&sample_rate));
  BITS_PER_SAMPLE (&w) = (unsigned) (VALUE (&bits_per_sample));
  test_bits_per_sample (p, BITS_PER_SAMPLE (&w));
  DATA_SIZE (&w) = (unsigned) A68_SOUND_DATA_SIZE (&w);
  DATA (&w) = heap_generator (p, MODE (SOUND_DATA), (int) DATA_SIZE (&w));
  STATUS (&w) = INIT_MASK;
  PUSH_OBJECT (p, w, A68_SOUND);
}

/*!
\brief PROC get sound = (SOUND w, INT channel, sample) INT
\param p position in tree
**/

void genie_get_sound (NODE_T * p)
{
  A68_INT channel, sample;
  A68_SOUND w;
  int addr, k, n, z, m;
  BYTE_T *d;
  POP_OBJECT (p, &sample, A68_INT);
  POP_OBJECT (p, &channel, A68_INT);
  POP_OBJECT (p, &w, A68_SOUND);
  if (!(VALUE (&channel) >= 1 && VALUE (&channel) <= (int) NUM_CHANNELS (&w))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "channel index out of range");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!(VALUE (&sample) >= 1 && VALUE (&sample) <= (int) NUM_SAMPLES (&w))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "sample index out of range");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (IS_NIL (DATA (&w))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "sound has no data");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  n = A68_SOUND_BYTES (&w);
  addr = ((VALUE (&sample) - 1) * (int) (NUM_CHANNELS (&w)) + (VALUE (&channel) - 1)) * n;
  ABEND (addr < 0 || addr >= (int) DATA_SIZE (&w), ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
  d = &(ADDRESS (&(DATA (&w)))[addr]);
/* Convert from little-endian, irrespective from the platform we work on */
  for (k = 0, z = 0, m = 0; k < n; k++) {
    z += ((int) (d[k]) * (int) (pow256[k]));
    m = k;
  }
  PUSH_PRIMITIVE (p, (d[m] & 0x80 ? (n == 4 ? z : z - (int) pow256[m + 1]) : z), A68_INT);
}

/*!
\brief PROC set sound = (SOUND w, INT channel, sample, value) VOID
\param p position in tree
**/

void genie_set_sound (NODE_T * p)
{
  A68_INT channel, sample, value;
  int addr, k, n, z;
  BYTE_T *d;
  A68_SOUND w;
  POP_OBJECT (p, &value, A68_INT);
  POP_OBJECT (p, &sample, A68_INT);
  POP_OBJECT (p, &channel, A68_INT);
  POP_OBJECT (p, &w, A68_SOUND);
  if (!(VALUE (&channel) >= 1 && VALUE (&channel) <= (int) NUM_CHANNELS (&w))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "channel index out of range");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!(VALUE (&sample) >= 1 && VALUE (&sample) <= (int) NUM_SAMPLES (&w))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "sample index out of range");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (IS_NIL (DATA (&w))) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, MODE (SOUND), "sound has no data");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  n = A68_SOUND_BYTES (&w);
  addr = ((VALUE (&sample) - 1) * (int) (NUM_CHANNELS (&w)) + (VALUE (&channel) - 1)) * n;
  ABEND (addr < 0 || addr >= (int) DATA_SIZE (&w), ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
  d = &(ADDRESS (&(DATA (&w)))[addr]);
/* Convert to little-endian */
  for (k = 0, z = VALUE (&value); k < n; k++) {
    d[k] = (BYTE_T) (z & 0xff);
    z >>= 8;
  }
}

/*!
\brief OP SOUND = (SOUND) INT
\param p position in tree
**/

void genie_sound_samples (NODE_T * p)
{
  A68_SOUND w;
  POP_OBJECT (p, &w, A68_SOUND);
  PUSH_PRIMITIVE (p, (int) (NUM_SAMPLES (&w)), A68_INT);
}

/*!
\brief OP RATE = (SOUND) INT
\param p position in tree
**/

void genie_sound_rate (NODE_T * p)
{
  A68_SOUND w;
  POP_OBJECT (p, &w, A68_SOUND);
  PUSH_PRIMITIVE (p, (int) (SAMPLE_RATE (&w)), A68_INT);
}

/*!
\brief OP CHANNELS = (SOUND) INT
\param p position in tree
**/

void genie_sound_channels (NODE_T * p)
{
  A68_SOUND w;
  POP_OBJECT (p, &w, A68_SOUND);
  PUSH_PRIMITIVE (p, (int) (NUM_CHANNELS (&w)), A68_INT);
}

/*!
\brief OP RESOLUTION = (SOUND) INT
\param p position in tree
**/

void genie_sound_resolution (NODE_T * p)
{
  A68_SOUND w;
  POP_OBJECT (p, &w, A68_SOUND);
  PUSH_PRIMITIVE (p, (int) (BITS_PER_SAMPLE (&w)), A68_INT);
}

/*!
Unix extensions to A68G
*/

#define MAX_RESTART 256

BOOL_T halt_typing;
static int chars_in_tty_line;

char output_line[BUFFER_SIZE], edit_line[BUFFER_SIZE], input_line[BUFFER_SIZE];

/*!
\brief initialise output to STDOUT
**/

void init_tty (void)
{
  chars_in_tty_line = 0;
  halt_typing = A68_FALSE;
  change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
}

/*!
\brief terminate current line on STDOUT
**/

void io_close_tty_line (void)
{
  if (chars_in_tty_line > 0) {
    io_write_string (STDOUT_FILENO, NEWLINE_STRING);
  }
}

/*!
\brief get a char from STDIN
\return same
**/

char get_stdin_char (void)
{
  ssize_t j;
  char ch[4];
  RESET_ERRNO;
  j = io_read_conv (STDIN_FILENO, &(ch[0]), 1);
  ABEND (j < 0, "cannot read char from stdin", NO_TEXT);
  return ((char) (j == 1 ? ch[0] : EOF_CHAR));
}

/*!
\brief read string from STDIN, until NEWLINE_STRING
\param prompt prompt string
\return input line buffer
**/

char *read_string_from_tty (char *prompt)
{
#if defined HAVE_READLINE
  char * line = readline (prompt); 
  if (line != NO_TEXT && (int) strlen (line) > 0) {
    add_history (line);
  }
  bufcpy (input_line, line, BUFFER_SIZE);
  chars_in_tty_line = (int) strlen (input_line);
  free (line);
  return (input_line);

#else
  int ch, k = 0, n;
  if (prompt != NO_TEXT) {
    io_close_tty_line ();
    io_write_string (STDOUT_FILENO, prompt);
  }
  ch = get_stdin_char ();
  while (ch != NEWLINE_CHAR && k < BUFFER_SIZE - 1) {
    if (ch == EOF_CHAR) {
      input_line[0] = EOF_CHAR;
      input_line[1] = NULL_CHAR;
      chars_in_tty_line = 1;
      return (input_line);
    } else {
      input_line[k++] = (char) ch;
      ch = get_stdin_char ();
    }
  }
  input_line[k] = NULL_CHAR;
  n = (int) strlen (input_line);
  chars_in_tty_line = (ch == NEWLINE_CHAR ? 0 : (n > 0 ? n : 1));
  return (input_line);
#endif
}

/*!
\brief write string to file
\param f file number
\param z string to write
**/

void io_write_string (FILE_T f, const char *z)
{
  ssize_t j;
  RESET_ERRNO;
  if (f != STDOUT_FILENO && f != STDERR_FILENO) {
/* Writing to file */
    j = io_write_conv (f, z, strlen (z));
    ABEND (j < 0, "cannot write", NO_TEXT);
  } else {
/* Writing to TTY */
    int first, k;
/* Write parts until end-of-string */
    first = 0;
    do {
      k = first;
/* How far can we get? */
      while (z[k] != NULL_CHAR && z[k] != NEWLINE_CHAR) {
        k++;
      }
      if (k > first) {
/* Write these characters */
        int n = k - first;
        j = io_write_conv (f, &(z[first]), (size_t) n);
        ABEND (j < 0, "cannot write", NO_TEXT);
        chars_in_tty_line += n;
      }
      if (z[k] == NEWLINE_CHAR) {
/* Pretty-print newline */
        k++;
        first = k;
        j = io_write_conv (f, NEWLINE_STRING, 1);
        ABEND (j < 0, "cannot write", NO_TEXT);
        chars_in_tty_line = 0;
      }
    } while (z[k] != NULL_CHAR);
  }
}

/*!
\brief read bytes from file into buffer
\param fd file descriptor, must be open
\param buf character buffer, size must be >= n
\param n maximum number of bytes to read
\return number of bytes read or -1 in case of error
**/

ssize_t io_read (FILE_T fd, void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
#if defined HAVE_WIN32
    int bytes_read;
#else
    ssize_t bytes_read;
#endif
    RESET_ERRNO;
    bytes_read = read (fd, z, to_do);
    if (bytes_read < 0) {
      if (errno == EINTR) {
/* interrupt, retry */
        bytes_read = 0;
        if (restarts++ > MAX_RESTART) {
          return (-1);
        }
      } else {
/* read error */
        return (-1);
      }
    } else if (bytes_read == 0) {
      break;                    /* EOF_CHAR */
    }
    to_do -= (size_t) bytes_read;
    z += bytes_read;
  }
  return ((ssize_t) n - (ssize_t) to_do);       /* return >= 0 */
}

/*!
\brief writes n bytes from buffer to file
\param fd file descriptor, must be open
\param buf character buffer, size must be >= n
\param n maximum number of bytes to write
\return n or -1 in case of error
**/

ssize_t io_write (FILE_T fd, const void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
    ssize_t bytes_written;
    RESET_ERRNO;
    bytes_written = write (fd, z, to_do);
    if (bytes_written <= 0) {
      if (errno == EINTR) {
/* interrupt, retry */
        bytes_written = 0;
        if (restarts++ > MAX_RESTART) {
          return (-1);
        }
      } else {
/* write error */
        return (-1);
      }
    }
    to_do -= (size_t) bytes_written;
    z += bytes_written;
  }
  return ((ssize_t) n);
}

/*!
\brief read bytes from file into buffer
\param fd file descriptor, must be open
\param buf character buffer, size must be >= n
\param n maximum number of bytes to read
\return number of bytes read or -1 in case of error
**/

ssize_t io_read_conv (FILE_T fd, void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
#if defined HAVE_WIN32
    int bytes_read;
#else
    ssize_t bytes_read;
#endif
    RESET_ERRNO;
    bytes_read = read (fd, z, to_do);
    if (bytes_read < 0) {
      if (errno == EINTR) {
/* interrupt, retry */
        bytes_read = 0;
        if (restarts++ > MAX_RESTART) {
          return (-1);
        }
      } else {
/* read error */
        return (-1);
      }
    } else if (bytes_read == 0) {
      break;                    /* EOF_CHAR */
    }
    to_do -= (size_t) bytes_read;
    z += bytes_read;
  }
  return ((ssize_t) n - (ssize_t) to_do);
}

/*!
\brief writes n bytes from buffer to file
\param fd file descriptor, must be open
\param buf character buffer, size must be >= n
\param n maximum number of bytes to write
\return n or -1 in case of error
**/

ssize_t io_write_conv (FILE_T fd, const void *buf, size_t n)
{
  size_t to_do = n;
  int restarts = 0;
  char *z = (char *) buf;
  while (to_do > 0) {
    ssize_t bytes_written;
    RESET_ERRNO;
    bytes_written = write (fd, z, to_do);
    if (bytes_written <= 0) {
      if (errno == EINTR) {
/* interrupt, retry */
        bytes_written = 0;
        if (restarts++ > MAX_RESTART) {
          return (-1);
        }
      } else {
/* write error */
        return (-1);
      }
    }
    to_do -= (size_t) bytes_written;
    z += bytes_written;
  }
  return ((ssize_t) n);
}

/*
This code implements some UNIX/Linux/BSD related routines.
In part contributed by Sian Leitch. 
*/

#define VECTOR_SIZE 512
#define FD_READ 0
#define FD_WRITE 1

extern A68_REF tmp_to_a68_string (NODE_T *, char *);

extern A68_CHANNEL stand_in_channel, stand_out_channel, stand_draw_channel, stand_back_channel, stand_error_channel;

#if defined HAVE_DIRENT_H

/*!
\brief PROC (STRING) [] STRING directory
\param p position in tree
**/

void genie_directory (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, name)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
    PUSH_PRIMITIVE (p, A68_MAX_INT, A68_INT);
  } else {
    char *dir_name = a_to_c_string (p, buffer, name);
    A68_REF z, row;
    A68_ARRAY arr;
    A68_TUPLE tup;
    int k, n = 0;
    A68_REF *base;
    DIR *dir;
    struct dirent *entry;
    dir = opendir (dir_name);
    if (dir == NULL) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    do {
      entry = readdir (dir);
      if (errno != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (entry != NULL) {
        n++;
      }
    } while (entry != NULL);
    rewinddir (dir);
    if (errno != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z = heap_generator (p, MODE (ROW_STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
    row = heap_generator (p, MODE (ROW_STRING), n * MOID_SIZE (MODE (STRING)));
    DIM (&arr) = 1;
    MOID (&arr) = MODE (STRING);
    ELEM_SIZE (&arr) = MOID_SIZE (MODE (STRING));
    SLICE_OFFSET (&arr) = 0;
    FIELD_OFFSET (&arr) = 0;
    ARRAY (&arr) = row;
    LWB (&tup) = 1;
    UPB (&tup) = n;
    SHIFT (&tup) = LWB (&tup);
    SPAN (&tup) = 1;
    K (&tup) = 0;
    PUT_DESCRIPTOR (arr, tup, &z);
    base = DEREF (A68_REF, &row);
    for (k = 0; k < n; k++) {
      entry = readdir (dir);
      if (errno != 0) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      base[k] = c_to_a_string (p, D_NAME (entry), DEFAULT_WIDTH);
    }
    if (closedir (dir) != 0) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PUSH_REF (p, z);
    free (buffer);
  }
}

#endif

/*!
\brief PROC [] INT utc time
\param p position in tree
**/

void genie_utctime (NODE_T * p)
{
  time_t dt;
  if (time (&dt) == (time_t) - 1) {
    (void) empty_row (p, MODE (ROW_INT));
  } else {
    A68_REF row;
    ADDR_T sp = stack_pointer;
    struct tm *tod = gmtime (&dt);
    PUSH_PRIMITIVE (p, TM_YEAR (tod) + 1900, A68_INT);
    PUSH_PRIMITIVE (p, TM_MON (tod) + 1, A68_INT);
    PUSH_PRIMITIVE (p, TM_MDAY (tod), A68_INT);
    PUSH_PRIMITIVE (p, TM_HOUR (tod), A68_INT);
    PUSH_PRIMITIVE (p, TM_MIN (tod), A68_INT);
    PUSH_PRIMITIVE (p, TM_SEC (tod), A68_INT);
    PUSH_PRIMITIVE (p, TM_WDAY (tod) + 1, A68_INT);
    PUSH_PRIMITIVE (p, TM_ISDST (tod), A68_INT);
    row = genie_make_row (p, MODE (INT), 8, sp);
    stack_pointer = sp;
    PUSH_REF (p, row);
  }
}

/*!
\brief PROC [] INT local time
\param p position in tree
**/

void genie_localtime (NODE_T * p)
{
  time_t dt;
  if (time (&dt) == (time_t) - 1) {
    (void) empty_row (p, MODE (ROW_INT));
  } else {
    A68_REF row;
    ADDR_T sp = stack_pointer;
    struct tm *tod = localtime (&dt);
    PUSH_PRIMITIVE (p, TM_YEAR (tod) + 1900, A68_INT);
    PUSH_PRIMITIVE (p, TM_MON (tod) + 1, A68_INT);
    PUSH_PRIMITIVE (p, TM_MDAY (tod), A68_INT);
    PUSH_PRIMITIVE (p, TM_HOUR (tod), A68_INT);
    PUSH_PRIMITIVE (p, TM_MIN (tod), A68_INT);
    PUSH_PRIMITIVE (p, TM_SEC (tod), A68_INT);
    PUSH_PRIMITIVE (p, TM_WDAY (tod) + 1, A68_INT);
    PUSH_PRIMITIVE (p, TM_ISDST (tod), A68_INT);
    row = genie_make_row (p, MODE (INT), 8, sp);
    stack_pointer = sp;
    PUSH_REF (p, row);
  }
}

/*!
\brief PROC INT rows
\param p position in tree
**/

void genie_rows (NODE_T * p)
{
  RESET_ERRNO;
  PUSH_PRIMITIVE (p, term_heigth, A68_INT);
}

/*!
\brief PROC INT columns
\param p position in tree
**/

void genie_columns (NODE_T * p)
{
  RESET_ERRNO;
  PUSH_PRIMITIVE (p, term_width, A68_INT);
}

/*!
\brief PROC INT argc
\param p position in tree
**/

void genie_argc (NODE_T * p)
{
  RESET_ERRNO;
  PUSH_PRIMITIVE (p, global_argc, A68_INT);
}

/*!
\brief PROC (INT) STRING argv
\param p position in tree
**/

void genie_argv (NODE_T * p)
{
  A68_INT a68g_index;
  RESET_ERRNO;
  POP_OBJECT (p, &a68g_index, A68_INT);
  if (VALUE (&a68g_index) >= 1 && VALUE (&a68g_index) <= global_argc) {
    char *q = global_argv[VALUE (&a68g_index) - 1];
    int n = (int) strlen (q);
/* Allow for spaces ending in # to have A68 comment syntax with '#!' */
    while (n > 0 && (IS_SPACE(q[n - 1]) || q[n - 1] == '#')) {
      q[--n] = NULL_CHAR;
    }
    PUSH_REF (p, c_to_a_string (p, q, DEFAULT_WIDTH));
  } else {
    PUSH_REF (p, empty_string (p));
  }
}

/*!
\brief PROC STRING pwd
\param p position in tree
**/

void genie_pwd (NODE_T * p)
{
  size_t size = BUFFER_SIZE;
  char *buffer = NO_TEXT;
  BOOL_T cont = A68_TRUE;
  RESET_ERRNO;
  while (cont) {
    buffer = (char *) malloc (size);
    if (buffer == NO_TEXT) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    if (getcwd (buffer, size) == buffer) {
      cont = A68_FALSE;
    } else {
      free (buffer);
      cont = (BOOL_T) (errno == 0);
      size *= 2;
    }
  }
  if (buffer != NO_TEXT && errno == 0) {
    PUSH_REF (p, c_to_a_string (p, buffer, DEFAULT_WIDTH));
    free (buffer);
  } else {
    PUSH_REF (p, empty_string (p));
  }
}

/*!
\brief PROC (STRING) INT cd
\param p position in tree
**/

void genie_cd (NODE_T * p)
{
  A68_REF dir;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &dir);
  CHECK_INIT (p, INITIALISED (&dir), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, dir)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    int rc = chdir (a_to_c_string (p, buffer, dir));
    if (rc == 0) {
      PUSH_PRIMITIVE (p, chdir (a_to_c_string (p, buffer, dir)), A68_INT);
    } else {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_FILE_ACCESS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    free (buffer);
  }
}

/*!
\brief PROC (STRING) BITS
\param p position in tree
**/

void genie_file_mode (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, name)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_PRIMITIVE (p, (unsigned) (ST_MODE (&status)), A68_BITS);
    } else {
      PUSH_PRIMITIVE (p, 0x0, A68_BITS);
    }
    free (buffer);
  }
}

/*!
\brief PROC (STRING) BOOL file is block device
\param p position in tree
**/

void genie_file_is_block_device (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, name)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_PRIMITIVE (p, (BOOL_T) (S_ISBLK (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
    }
    free (buffer);
  }
}

/*!
\brief PROC (STRING) BOOL file is char device
\param p position in tree
**/

void genie_file_is_char_device (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, name)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_PRIMITIVE (p, (BOOL_T) (S_ISCHR (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
    }
    free (buffer);
  }
}

/*!
\brief PROC (STRING) BOOL file is directory
\param p position in tree
**/

void genie_file_is_directory (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, name)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_PRIMITIVE (p, (BOOL_T) (S_ISDIR (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
    }
    free (buffer);
  }
}

/*!
\brief PROC (STRING) BOOL file is regular
\param p position in tree
**/

void genie_file_is_regular (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, name)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_PRIMITIVE (p, (BOOL_T) (S_ISREG (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
    }
    free (buffer);
  }
}

#if defined __S_IFIFO

/*!
\brief PROC (STRING) BOOL file is fifo
\param p position in tree
**/

void genie_file_is_fifo (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, name)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_PRIMITIVE (p, (BOOL_T) (S_ISFIFO (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
    }
    free (buffer);
  }
}

#endif

#if defined S_ISLNK

/*!
\brief PROC (STRING) BOOL file is link
\param p position in tree
**/

void genie_file_is_link (NODE_T * p)
{
  A68_REF name;
  char *buffer;
  RESET_ERRNO;
  POP_REF (p, &name);
  CHECK_INIT (p, INITIALISED (&name), MODE (STRING));
  buffer = (char *) malloc ((size_t) (1 + a68_string_size (p, name)));
  if (buffer == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_CORE);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    struct stat status;
    if (stat (a_to_c_string (p, buffer, name), &status) == 0) {
      PUSH_PRIMITIVE (p, (BOOL_T) (S_ISLNK (ST_MODE (&status)) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
    } else {
      PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
    }
    free (buffer);
  }
}

#endif

/*!
\brief convert [] STRING row to char *vec[]
\param p position in tree
\param vec string vector
\param row [] STRING
**/

static void convert_string_vector (NODE_T * p, char *vec[], A68_REF row)
{
  BYTE_T *z = ADDRESS (&row);
  A68_ARRAY *arr = (A68_ARRAY *) & z[0];
  A68_TUPLE *tup = (A68_TUPLE *) & z[ALIGNED_SIZE_OF (A68_ARRAY)];
  int k = 0;
  if (get_row_size (tup, DIM (arr)) > 0) {
    BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
    BOOL_T done = A68_FALSE;
    initialise_internal_index (tup, DIM (arr));
    while (!done) {
      ADDR_T a68g_index = calculate_internal_index (tup, DIM (arr));
      ADDR_T elem_addr = (a68g_index + SLICE_OFFSET (arr)) * ELEM_SIZE (arr) + FIELD_OFFSET (arr);
      BYTE_T *elem = &base_addr[elem_addr];
      int size = a68_string_size (p, *(A68_REF *) elem);
      CHECK_INIT (p, INITIALISED ((A68_REF *) elem), MODE (STRING));
      vec[k] = (char *) get_heap_space ((size_t) (1 + size));
      ASSERT (a_to_c_string (p, vec[k], *(A68_REF *) elem) != NO_TEXT);
      if (k == VECTOR_SIZE - 1) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_TOO_MANY_ARGUMENTS);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      if (strlen (vec[k]) > 0) {
        k++;
      }
      done = increment_internal_index (tup, DIM (arr));
    }
  }
  vec[k] = NO_TEXT;
}

/*!
\brief free char *vec[]
\param vec string vector
**/

static void free_vector (char *vec[])
{
  int k = 0;
  while (vec[k] != NO_TEXT) {
    free (vec[k]);
    k++;
  }
}

/*!
\brief reset error number
\param p position in tree
**/

void genie_reset_errno (NODE_T * p)
{
  (void) *p;
  RESET_ERRNO;
}

/*!
\brief error number
\param p position in tree
**/

void genie_errno (NODE_T * p)
{
  PUSH_PRIMITIVE (p, errno, A68_INT);
}

/*!
\brief PROC strerror = (INT) STRING
\param p position in tree
**/

void genie_strerror (NODE_T * p)
{
  A68_INT i;
  POP_OBJECT (p, &i, A68_INT);
  PUSH_REF (p, c_to_a_string (p, strerror (VALUE (&i)), DEFAULT_WIDTH));
}

/*!
\brief set up file for usage in pipe
\param p position in tree
\param z pointer to file
\param fd file number
\param chan channel
\param r_mood read mood
\param w_mood write mood
\param pid pid
**/

static void set_up_file (NODE_T * p, A68_REF * z, int fd, A68_CHANNEL chan, BOOL_T r_mood, BOOL_T w_mood, int pid)
{
  A68_FILE *f;
  *z = heap_generator (p, MODE (REF_FILE), ALIGNED_SIZE_OF (A68_FILE));
  f = FILE_DEREF (z);
  STATUS (f) = (STATUS_MASK) ((pid < 0) ? 0 : INIT_MASK);
  IDENTIFICATION (f) = nil_ref;
  TERMINATOR (f) = nil_ref;
  CHANNEL (f) = chan;
  FD (f) = fd;
  STREAM (&DEVICE (f)) = NO_STREAM;
  OPENED (f) = A68_TRUE;
  OPEN_EXCLUSIVE (f) = A68_FALSE;
  READ_MOOD (f) = r_mood;
  WRITE_MOOD (f) = w_mood;
  CHAR_MOOD (f) = A68_TRUE;
  DRAW_MOOD (f) = A68_FALSE;
  FORMAT (f) = nil_format;
  TRANSPUT_BUFFER (f) = get_unblocked_transput_buffer (p);
  STRING (f) = nil_ref;
  reset_transput_buffer (TRANSPUT_BUFFER (f));
  set_default_event_procedures (f);
}

/*!
\brief create and push a pipe
\param p position in tree
\param fd_r read file number
\param fd_w write file number
\param pid pid
**/

static void genie_mkpipe (NODE_T * p, int fd_r, int fd_w, int pid)
{
  A68_REF r, w;
  RESET_ERRNO;
/* Set up pipe */
  set_up_file (p, &r, fd_r, stand_in_channel, A68_TRUE, A68_FALSE, pid);
  set_up_file (p, &w, fd_w, stand_out_channel, A68_FALSE, A68_TRUE, pid);
/* push pipe */
  PUSH_REF (p, r);
  PUSH_REF (p, w);
  PUSH_PRIMITIVE (p, pid, A68_INT);
}

/*!
\brief push an environment string
\param p position in tree
**/

void genie_getenv (NODE_T * p)
{
  A68_REF a_env;
  char *val, *z, *z_env;
  RESET_ERRNO;
  POP_REF (p, &a_env);
  CHECK_INIT (p, INITIALISED (&a_env), MODE (STRING));
  z_env = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_env)));
  z = a_to_c_string (p, z_env, a_env);
  val = getenv (z);
  if (val == NO_TEXT) {
    a_env = empty_string (p);
  } else {
    a_env = tmp_to_a68_string (p, val);
  }
  PUSH_REF (p, a_env);
}

/*!
\brief PROC fork = INT
\param p position in tree
**/

void genie_fork (NODE_T * p)
{
#if defined HAVE_WIN32
  PUSH_PRIMITIVE (p, -1, A68_INT);
#else
  int pid;
  RESET_ERRNO;
  pid = (int) fork ();
  PUSH_PRIMITIVE (p, pid, A68_INT);
#endif
}

/*!
\brief PROC execve = (STRING, [] STRING, [] STRING) INT 
\param p position in tree
**/

void genie_execve (NODE_T * p)
{
  int ret;
  A68_REF a_prog, a_args, a_env;
  char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
  RESET_ERRNO;
/* Pop parameters */
  POP_REF (p, &a_env);
  POP_REF (p, &a_args);
  POP_REF (p, &a_prog);
/* Convert strings and hasta el infinito */
  prog = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_prog)));
  ASSERT (a_to_c_string (p, prog, a_prog) != NO_TEXT);
  convert_string_vector (p, argv, a_args);
  convert_string_vector (p, envp, a_env);
  if (argv[0] == NO_TEXT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
#if defined HAVE_WIN32
  ret = execve (prog, (const char * const *) argv, (const char * const *) envp);
#else
  ret = execve (prog, argv, envp);
#endif
/* execve only returns if it fails */
  free_vector (argv);
  free_vector (envp);
  free (prog);
  PUSH_PRIMITIVE (p, ret, A68_INT);
}

/*!
\brief PROC execve child = (STRING, [] STRING, [] STRING) INT
\param p position in tree
**/

void genie_execve_child (NODE_T * p)
{
  int pid;
  A68_REF a_prog, a_args, a_env;
  RESET_ERRNO;
/* Pop parameters */
  POP_REF (p, &a_env);
  POP_REF (p, &a_args);
  POP_REF (p, &a_prog);
/* Now create the pipes and fork */
#if defined HAVE_WIN32
  pid = -1;
  PUSH_PRIMITIVE (p, -1, A68_INT);
  return;
#else
  pid = (int) fork ();
  if (pid == -1) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
  } else if (pid == 0) {
/* Child process */
    char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
/* Convert  strings */
    prog = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_prog)));
    ASSERT (a_to_c_string (p, prog, a_prog) != NO_TEXT);
    convert_string_vector (p, argv, a_args);
    convert_string_vector (p, envp, a_env);
    if (argv[0] == NO_TEXT) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    (void) execve (prog, argv, envp);
/* execve only returns if it fails - end child process */
    a68g_exit (EXIT_FAILURE);
    PUSH_PRIMITIVE (p, 0, A68_INT);
  } else {
/* parent process */
    PUSH_PRIMITIVE (p, pid, A68_INT);
  }
#endif /* defined HAVE_WIN32 */
}

/*!
\brief PROC execve child pipe = (STRING, [] STRING, [] STRING) PIPE
\param p position in tree
**/

void genie_execve_child_pipe (NODE_T * p)
{
/*
Child redirects STDIN and STDOUT.
Return a PIPE that contains the descriptors for the parent.

       pipe ptoc
       ->W...R->
 PARENT         CHILD
       <-R...W<-
       pipe ctop
*/
  int pid;
#if ! defined HAVE_WIN32
  int ptoc_fd[2], ctop_fd[2];
#endif /* ! defined HAVE_WIN32 */
  A68_REF a_prog, a_args, a_env;
  RESET_ERRNO;
/* Pop parameters */
  POP_REF (p, &a_env);
  POP_REF (p, &a_args);
  POP_REF (p, &a_prog);
#if defined HAVE_WIN32
  pid = -1;
  genie_mkpipe (p, -1, -1, -1);
  return;
#else
/* Create the pipes and fork */
  if ((pipe (ptoc_fd) == -1) || (pipe (ctop_fd) == -1)) {
    genie_mkpipe (p, -1, -1, -1);
    return;
  }
  pid = (int) fork ();
  if (pid == -1) {
/* Fork failure */
    genie_mkpipe (p, -1, -1, -1);
    return;
  }
  if (pid == 0) {
/* Child process */
    char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
/* Convert  strings */
    prog = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_prog)));
    ASSERT (a_to_c_string (p, prog, a_prog) != NO_TEXT);
    convert_string_vector (p, argv, a_args);
    convert_string_vector (p, envp, a_env);
/* Set up redirection */
    ASSERT (close (ctop_fd[FD_READ]) == 0);
    ASSERT (close (ptoc_fd[FD_WRITE]) == 0);
    ASSERT (close (STDIN_FILENO) == 0);
    ASSERT (close (STDOUT_FILENO) == 0);
    ASSERT (dup2 (ptoc_fd[FD_READ], STDIN_FILENO) != -1);
    ASSERT (dup2 (ctop_fd[FD_WRITE], STDOUT_FILENO) != -1);
    if (argv[0] == NO_TEXT) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    (void) execve (prog, argv, envp);
/* execve only returns if it fails - end child process */
    a68g_exit (EXIT_FAILURE);
    genie_mkpipe (p, -1, -1, -1);
  } else {
/* Parent process */
    ASSERT (close (ptoc_fd[FD_READ]) == 0);
    ASSERT (close (ctop_fd[FD_WRITE]) == 0);
    genie_mkpipe (p, ctop_fd[FD_READ], ptoc_fd[FD_WRITE], pid);
  }
#endif /* defined HAVE_WIN32 */
}

/*!
\brief PROC execve_output = (STRING, [] STRING, [] STRING, REF_STRING) INT
\param p position in tree
**/

void genie_execve_output (NODE_T * p)
{
/*
Child redirects STDIN and STDOUT.

       pipe ptoc
       ->W...R->
 PARENT         CHILD
       <-R...W<-
       pipe ctop
*/
  int pid;
#if ! defined HAVE_WIN32
  int ptoc_fd[2], ctop_fd[2];
#endif /* ! defined HAVE_WIN32 */
  A68_REF a_prog, a_args, a_env, dest;
  RESET_ERRNO;
/* Pop parameters */
  POP_REF (p, &dest);
  POP_REF (p, &a_env);
  POP_REF (p, &a_args);
  POP_REF (p, &a_prog);
#if defined HAVE_WIN32
  pid = -1;
  PUSH_PRIMITIVE (p, -1, A68_INT);
  return;
#else
/* Create the pipes and fork */
  if ((pipe (ptoc_fd) == -1) || (pipe (ctop_fd) == -1)) {
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  pid = (int) fork ();
  if (pid == -1) {
/* Fork failure */
    PUSH_PRIMITIVE (p, -1, A68_INT);
    return;
  }
  if (pid == 0) {
/* Child process */
    char *prog, *argv[VECTOR_SIZE], *envp[VECTOR_SIZE];
/* Convert  strings */
    prog = (char *) get_heap_space ((size_t) (1 + a68_string_size (p, a_prog)));
    ASSERT (a_to_c_string (p, prog, a_prog) != NO_TEXT);
    convert_string_vector (p, argv, a_args);
    convert_string_vector (p, envp, a_env);
/* Set up redirection */
    ASSERT (close (ctop_fd[FD_READ]) == 0);
    ASSERT (close (ptoc_fd[FD_WRITE]) == 0);
    ASSERT (close (STDIN_FILENO) == 0);
    ASSERT (close (STDOUT_FILENO) == 0);
    ASSERT (dup2 (ptoc_fd[FD_READ], STDIN_FILENO) != -1);
    ASSERT (dup2 (ctop_fd[FD_WRITE], STDOUT_FILENO) != -1);
    if (argv[0] == NO_TEXT) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EMPTY_ARGUMENT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    (void) execve (prog, argv, envp);
/* execve only returns if it fails - end child process */
    a68g_exit (EXIT_FAILURE);
    PUSH_PRIMITIVE (p, -1, A68_INT);
  } else {
/* Parent process */
    char ch;
    int pipe_read, ret, status;
    ASSERT (close (ptoc_fd[FD_READ]) == 0);
    ASSERT (close (ctop_fd[FD_WRITE]) == 0);
    reset_transput_buffer (INPUT_BUFFER);
    do {
      pipe_read = (int) io_read_conv (ctop_fd[FD_READ], &ch, 1);
      if (pipe_read > 0) {
        add_char_transput_buffer (p, INPUT_BUFFER, ch);
      }
    } while (pipe_read > 0);
    do {
      ret = (int) waitpid ((__pid_t) pid, &status, 0);
    } while (ret == -1 && errno == EINTR);
    if (ret != pid) {
      status = -1;
    }
    if (!IS_NIL (dest)) {
      * DEREF (A68_REF, &dest) =
        c_to_a_string (p, get_transput_buffer (INPUT_BUFFER),
                          get_transput_buffer_index (INPUT_BUFFER));
    }
    PUSH_PRIMITIVE (p, ret, A68_INT);
  }
#endif /* defined HAVE_WIN32 */
}

/*!
\brief PROC create pipe = PIPE
\param p position in tree
**/

void genie_create_pipe (NODE_T * p)
{
  RESET_ERRNO;
  genie_stand_in (p);
  genie_stand_out (p);
  PUSH_PRIMITIVE (p, -1, A68_INT);
}

/*!
\brief PROC wait pid = (INT) VOID
\param p position in tree
**/

void genie_waitpid (NODE_T * p)
{
  A68_INT k;
  RESET_ERRNO;
  POP_OBJECT (p, &k, A68_INT);
#if ! defined HAVE_WIN32
  ASSERT (waitpid ((__pid_t) VALUE (&k), NULL, 0) != -1);
#endif
}

/*
Next part contains some routines that interface Algol68G and the curses library.
Be sure to know what you are doing when you use this, but on the other hand,
"reset" will always restore your terminal. 
*/

#if defined HAVE_CURSES

#define CHECK_CURSES_RETVAL(f) {\
  if (!(f)) {\
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CURSES);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }}

BOOL_T a68g_curses_mode = A68_FALSE;

/*!
\brief clean_curses
**/

void clean_curses (void)
{
  if (a68g_curses_mode == A68_TRUE) {
    (void) wattrset (stdscr, A_NORMAL);
    (void) endwin ();
    a68g_curses_mode = A68_FALSE;
  }
}

/*!
\brief init_curses
**/

void init_curses (void)
{
  (void) initscr ();
  (void) cbreak (); /* raw () would cut off ctrl-c */
  (void) noecho ();
  (void) nonl ();
  (void) curs_set (0);
  if (has_colors ()) {
    (void) start_color ();
  }
}

/*!
\brief watch stdin for input, do not wait very long
\return character read
**/

int rgetchar (void)
{
#if defined HAVE_WIN32
  if (kbhit ()) {
    return (getch ());
  } else {
    return (NULL_CHAR);
  }
#else
  int retval;
  struct timeval tv;
  fd_set rfds;
  TV_SEC (&tv) = 0;
  TV_USEC (&tv) = 100;
  FD_ZERO (&rfds);
  FD_SET (0, &rfds);
  retval = select (1, &rfds, NULL, NULL, &tv);
  if (retval) {
    /* FD_ISSET(0, &rfds) will be true.  */
    return (getch ());
  } else {
    return (NULL_CHAR);
  }
#endif
}

/*!
\brief PROC curses start = VOID
\param p position in tree
**/

void genie_curses_start (NODE_T * p)
{
  errno = 0;
  init_curses ();
  if (errno != 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CURSES);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  a68g_curses_mode = A68_TRUE;
}

/*!
\brief PROC curses end = VOID
\param p position in tree
**/

void genie_curses_end (NODE_T * p)
{
  (void) p;
  clean_curses ();
}

/*!
\brief PROC curses clear = VOID
\param p position in tree
**/

void genie_curses_clear (NODE_T * p)
{
  if (a68g_curses_mode == A68_FALSE) {
    genie_curses_start (p);
  }
  CHECK_CURSES_RETVAL (clear () != ERR);
}

/*!
\brief PROC curses refresh = VOID
\param p position in tree
**/

void genie_curses_refresh (NODE_T * p)
{
  if (a68g_curses_mode == A68_FALSE) {
    genie_curses_start (p);
  }
  CHECK_CURSES_RETVAL (refresh () != ERR);
}

/*!
\brief PROC curses lines = INT
\param p position in tree
**/

void genie_curses_lines (NODE_T * p)
{
  if (a68g_curses_mode == A68_FALSE) {
    genie_curses_start (p);
  }
  PUSH_PRIMITIVE (p, LINES, A68_INT);
}

/*!
\brief PROC curses columns = INT
\param p position in tree
**/

void genie_curses_columns (NODE_T * p)
{
  if (a68g_curses_mode == A68_FALSE) {
    genie_curses_start (p);
  }
  PUSH_PRIMITIVE (p, COLS, A68_INT);
}

/*!
\brief PROC curses getchar = CHAR
\param p position in tree
**/

void genie_curses_getchar (NODE_T * p)
{
  if (a68g_curses_mode == A68_FALSE) {
    genie_curses_start (p);
  }
  PUSH_PRIMITIVE (p, (char) rgetchar (), A68_CHAR);
}


/*!
\brief PROC curses colour = VOID
\param p position in tree
**/

#define GENIE_COLOUR(f, n, fg, bg)\
void f (NODE_T *p) {\
  (void) p;\
  if ((n) < COLOR_PAIRS) {\
    (void) init_pair (n, (fg), (bg));\
    wattrset (stdscr, COLOR_PAIR ((n)) | A_BOLD);\
  }\
}\
void f##_inverse (NODE_T *p) {\
  (void) p;\
  if ((n + 8) < COLOR_PAIRS) {\
    (void) init_pair ((n) + 8, (bg), (fg));\
    wattrset (stdscr, COLOR_PAIR (((n) + 8)));\
  }\
}

GENIE_COLOUR (genie_curses_blue, 1, COLOR_BLUE, COLOR_BLACK)
GENIE_COLOUR (genie_curses_cyan, 2, COLOR_CYAN, COLOR_BLACK)
GENIE_COLOUR (genie_curses_green, 3, COLOR_GREEN, COLOR_BLACK)
GENIE_COLOUR (genie_curses_magenta, 4, COLOR_MAGENTA, COLOR_BLACK)
GENIE_COLOUR (genie_curses_red, 5, COLOR_RED, COLOR_BLACK)
GENIE_COLOUR (genie_curses_white, 6, COLOR_WHITE, COLOR_BLACK)
GENIE_COLOUR (genie_curses_yellow, 7, COLOR_YELLOW, COLOR_BLACK)

/*!
\brief PROC curses delchar = (CHAR) BOOL
\param p position in tree
**/

void genie_curses_del_char (NODE_T * p) {
  A68_CHAR ch;
  int v;
  POP_OBJECT (p, &ch, A68_CHAR);
  v = (int) VALUE (&ch);
  PUSH_PRIMITIVE (p, 
    (BOOL_T) (v == 8 || v == 127 || v == KEY_BACKSPACE), A68_BOOL);
  }

/*!
\brief PROC curses putchar = (CHAR) VOID
\param p position in tree
**/

void genie_curses_putchar (NODE_T * p)
{
  A68_CHAR ch;
  if (a68g_curses_mode == A68_FALSE) {
    genie_curses_start (p);
  }
  POP_OBJECT (p, &ch, A68_CHAR);
  (void) (addch ((chtype) (VALUE (&ch))));
/*
  if (addch ((chtype) (VALUE (&ch))) == ERR) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CURSES_OFF_SCREEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
*/
}

/*!
\brief PROC curses move = (INT, INT) VOID
\param p position in tree
**/

void genie_curses_move (NODE_T * p)
{
  A68_INT i, j;
  if (a68g_curses_mode == A68_FALSE) {
    genie_curses_start (p);
  }
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_INT);
  if (VALUE (&i) < 0 || VALUE (&i) >= LINES) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CURSES_OFF_SCREEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (VALUE (&j) < 0 || VALUE (&j) >= COLS) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_CURSES_OFF_SCREEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  CHECK_CURSES_RETVAL(move (VALUE (&i), VALUE (&j)) != ERR);
}

#endif /* HAVE_CURSES */

#if defined HAVE_REGEX_H
/*!
\brief return code for regex interface
\param rc return code from regex routine
\return 0: match, 1: no match, 2: no core, 3: other error
**/

void push_grep_rc (NODE_T * p, int rc)
{
  switch (rc) {
  case 0:
    {
      PUSH_PRIMITIVE (p, 0, A68_INT);
      return;
    }
  case REG_NOMATCH:
    {
      PUSH_PRIMITIVE (p, 1, A68_INT);
      return;
    }
  case REG_ESPACE:
    {
      PUSH_PRIMITIVE (p, 3, A68_INT);
      return;
    }
  default:
    {
      PUSH_PRIMITIVE (p, 2, A68_INT);
      return;
    }
  }
}

/*!
\brief PROC grep in string = (STRING, STRING, REF INT, REF INT) INT
\param p position in tree
\return 0: match, 1: no match, 2: no core, 3: other error
**/

void genie_grep_in_string (NODE_T * p)
{
  A68_REF ref_pat, ref_beg, ref_end, ref_str, row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int rc, nmatch, k, max_k, widest;
  regex_t compiled;
  regmatch_t *matches;
  POP_REF (p, &ref_end);
  POP_REF (p, &ref_beg);
  POP_REF (p, &ref_str);
  POP_REF (p, &ref_pat);
  row = *(A68_REF *) & ref_str;
  CHECK_INIT (p, INITIALISED (&row), MODE (ROWS));
  GET_DESCRIPTOR (arr, tup, &row);
  reset_transput_buffer (PATTERN_BUFFER);
  reset_transput_buffer (STRING_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_pat);
  add_a_string_transput_buffer (p, STRING_BUFFER, (BYTE_T *) & ref_str);
  rc = regcomp (&compiled, get_transput_buffer (PATTERN_BUFFER), REG_NEWLINE | REG_EXTENDED);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
  nmatch = (int) (RE_NSUB (&compiled));
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = malloc ((size_t) (nmatch * ALIGNED_SIZE_OF (regmatch_t)));
  if (nmatch > 0 && matches == NULL) {
    rc = 2;
    PUSH_PRIMITIVE (p, rc, A68_INT);
    regfree (&compiled);
    return;
  }
  rc = regexec (&compiled, get_transput_buffer (STRING_BUFFER), (size_t) nmatch, matches, 0);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
/* Find widest match. Do not assume it is the first one */
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = (int) (RM_EO (&(matches[k]))) - (int) (RM_SO (&(matches[k])));
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  if (!IS_NIL (ref_beg)) {
    A68_INT *i = DEREF (A68_INT, &ref_beg);
    STATUS (i) = INIT_MASK;
    VALUE (i) = (int) (RM_SO (&(matches[max_k]))) + (int) (LOWER_BOUND (tup));
  }
  if (!IS_NIL (ref_end)) {
    A68_INT *i = DEREF (A68_INT, &ref_end);
    STATUS (i) = INIT_MASK;
    VALUE (i) = (int) (RM_EO (&(matches[max_k]))) + (int) (LOWER_BOUND (tup)) - 1;
  }
  free (matches);
  push_grep_rc (p, 0);
}

/*!
\brief PROC grep in substring = (STRING, STRING, REF INT, REF INT) INT
\param p position in tree
\return 0: match, 1: no match, 2: no core, 3: other error
**/

void genie_grep_in_substring (NODE_T * p)
{
  A68_REF ref_pat, ref_beg, ref_end, ref_str, row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int rc, nmatch, k, max_k, widest;
  regex_t compiled;
  regmatch_t *matches;
  POP_REF (p, &ref_end);
  POP_REF (p, &ref_beg);
  POP_REF (p, &ref_str);
  POP_REF (p, &ref_pat);
  row = *(A68_REF *) & ref_str;
  CHECK_INIT (p, INITIALISED (&row), MODE (ROWS));
  GET_DESCRIPTOR (arr, tup, &row);
  reset_transput_buffer (PATTERN_BUFFER);
  reset_transput_buffer (STRING_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_pat);
  add_a_string_transput_buffer (p, STRING_BUFFER, (BYTE_T *) & ref_str);
  rc = regcomp (&compiled, get_transput_buffer (PATTERN_BUFFER), REG_NEWLINE | REG_EXTENDED);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
  nmatch = (int) (RE_NSUB (&compiled));
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = malloc ((size_t) (nmatch * ALIGNED_SIZE_OF (regmatch_t)));
  if (nmatch > 0 && matches == NULL) {
    rc = 2;
    PUSH_PRIMITIVE (p, rc, A68_INT);
    regfree (&compiled);
    return;
  }
  rc = regexec (&compiled, get_transput_buffer (STRING_BUFFER), (size_t) nmatch, matches, REG_NOTBOL);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
/* Find widest match. Do not assume it is the first one */
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = (int) (RM_EO (&(matches[k]))) - (int) (RM_SO (&(matches[k])));
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  if (!IS_NIL (ref_beg)) {
    A68_INT *i = DEREF (A68_INT, &ref_beg);
    STATUS (i) = INIT_MASK;
    VALUE (i) = (int) (RM_SO (&(matches[max_k]))) + (int) (LOWER_BOUND (tup));
  }
  if (!IS_NIL (ref_end)) {
    A68_INT *i = DEREF (A68_INT, &ref_end);
    STATUS (i) = INIT_MASK;
    VALUE (i) = (int) (RM_EO (&(matches[max_k]))) + (int) (LOWER_BOUND (tup)) - 1;
  }
  free (matches);
  push_grep_rc (p, 0);
}

/*!
\brief PROC sub in string = (STRING, STRING, REF STRING) INT
\param p position in tree
\return 0: match, 1: no match, 2: no core, 3: other error
**/

void genie_sub_in_string (NODE_T * p)
{
  A68_REF ref_pat, ref_rep, ref_str;
  int rc, nmatch, k, max_k, widest, begin, end;
  char *txt;
  regex_t compiled;
  regmatch_t *matches;
  POP_REF (p, &ref_str);
  POP_REF (p, &ref_rep);
  POP_REF (p, &ref_pat);
  if (IS_NIL (ref_str)) {
    PUSH_PRIMITIVE (p, 3, A68_INT);
    return;
  }
  reset_transput_buffer (STRING_BUFFER);
  reset_transput_buffer (REPLACE_BUFFER);
  reset_transput_buffer (PATTERN_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_pat);
  add_a_string_transput_buffer (p, STRING_BUFFER, (BYTE_T *) DEREF (A68_REF, &ref_str));
  rc = regcomp (&compiled, get_transput_buffer (PATTERN_BUFFER), REG_NEWLINE | REG_EXTENDED);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
  nmatch = (int) (RE_NSUB (&compiled));
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = malloc ((size_t) (nmatch * ALIGNED_SIZE_OF (regmatch_t)));
  if (nmatch > 0 && matches == NULL) {
    PUSH_PRIMITIVE (p, rc, A68_INT);
    regfree (&compiled);
    return;
  }
  rc = regexec (&compiled, get_transput_buffer (STRING_BUFFER), (size_t) nmatch, matches, 0);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
/* Find widest match. Do not assume it is the first one */
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = (int) RM_EO (&(matches[k])) - (int) RM_SO (&(matches[k]));
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  begin = (int) RM_SO (&(matches[max_k])) + 1;
  end = (int) RM_EO (&(matches[max_k]));
/* Substitute text */
  txt = get_transput_buffer (STRING_BUFFER);
  for (k = 0; k < begin - 1; k++) {
    add_char_transput_buffer (p, REPLACE_BUFFER, txt[k]);
  }
  add_a_string_transput_buffer (p, REPLACE_BUFFER, (BYTE_T *) & ref_rep);
  for (k = end; k < get_transput_buffer_size (STRING_BUFFER); k++) {
    add_char_transput_buffer (p, REPLACE_BUFFER, txt[k]);
  }
  * DEREF (A68_REF, &ref_str) = c_to_a_string (p, get_transput_buffer (REPLACE_BUFFER), DEFAULT_WIDTH);
  free (matches);
  push_grep_rc (p, 0);
}

#endif /* HAVE_REGEX_H */

