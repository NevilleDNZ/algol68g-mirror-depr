//! @file prelude.c
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

//! @section Synopsis
//!
//! Standard prelude definitions.

#include "a68g.h"
#include "a68g-optimiser.h"
#include "a68g-prelude.h"
#include "a68g-prelude-mathlib.h"
#include "a68g-prelude-gsl.h"
#include "a68g-transput.h"
#include "a68g-mp.h"
#include "a68g-parser.h"
#include "a68g-physics.h"
#include "a68g-double.h"

#define A68_STD A68_TRUE
#define A68_EXT A68_FALSE

//! @brief Standard_environ_proc_name.

char *standard_environ_proc_name (GPROC f)
{
  TAG_T *i = IDENTIFIERS (A68_STANDENV);
  for (; i != NO_TAG; FORWARD (i)) {
    if (PROCEDURE (i) == f) {
      return NSYMBOL (NODE (i));
    }
  }
  return NO_TEXT;
}

//! @brief Enter tag in standenv symbol table.

void add_a68_standenv (BOOL_T portable, int a, NODE_T * n, char *c, MOID_T * m, int p, GPROC * q)
{
#define INSERT_TAG(l, n) {\
  NEXT (n) = *(l);\
  *(l) = (n);\
  }
  TAG_T *new_one = new_tag ();
  PROCEDURE_LEVEL (INFO (n)) = 0;
  USE (new_one) = A68_FALSE;
  HEAP (new_one) = HEAP_SYMBOL;
  TAG_TABLE (new_one) = A68_STANDENV;
  NODE (new_one) = n;
  VALUE (new_one) = (c != NO_TEXT ? TEXT (add_token (&A68 (top_token), c)) : NO_TEXT);
  PRIO (new_one) = p;
  PROCEDURE (new_one) = q;
  A68_STANDENV_PROC (new_one) = (BOOL_T) (q != NO_GPROC);
  UNIT (new_one) = NULL;
  PORTABLE (new_one) = portable;
  MOID (new_one) = m;
  NEXT (new_one) = NO_TAG;
  if (a == IDENTIFIER) {
    INSERT_TAG (&IDENTIFIERS (A68_STANDENV), new_one);
  } else if (a == OP_SYMBOL) {
    INSERT_TAG (&OPERATORS (A68_STANDENV), new_one);
  } else if (a == PRIO_SYMBOL) {
    INSERT_TAG (&PRIO (A68_STANDENV), new_one);
  } else if (a == INDICANT) {
    INSERT_TAG (&INDICANTS (A68_STANDENV), new_one);
  } else if (a == LABEL) {
    INSERT_TAG (&LABELS (A68_STANDENV), new_one);
  }
#undef INSERT_TAG
}

//! @brief Compose PROC moid from arguments - first result, than arguments.

MOID_T *a68_proc (MOID_T * m, ...)
{
  MOID_T *y, **z = &TOP_MOID (&A68_JOB);
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
  return add_mode (z, PROC_SYMBOL, count_pack_members (p), NO_NODE, m, p);
}

//! @brief Enter an identifier in standenv.

void a68_idf (BOOL_T portable, char *n, MOID_T * m, GPROC * q)
{
  add_a68_standenv (portable, IDENTIFIER, some_node (TEXT (add_token (&A68 (top_token), n))), NO_TEXT, m, 0, q);
}

//! @brief Enter a moid in standenv.

void a68_mode (int p, char *t, MOID_T ** m)
{
  (*m) = add_mode (&TOP_MOID (&A68_JOB), STANDARD, p, some_node (TEXT (find_keyword (A68 (top_keyword), t))), NO_MOID, NO_PACK);
}

//! @brief Enter a priority in standenv.

void a68_prio (char *p, int b)
{
  add_a68_standenv (A68_TRUE, PRIO_SYMBOL, some_node (TEXT (add_token (&A68 (top_token), p))), NO_TEXT, NO_MOID, b, NO_GPROC);
}

//! @brief Enter operator in standenv.

void a68_op (BOOL_T portable, char *n, MOID_T * m, GPROC * q)
{
  add_a68_standenv (portable, OP_SYMBOL, some_node (TEXT (add_token (&A68 (top_token), n))), NO_TEXT, m, 0, q);
}

//! @brief Enter standard modes in standenv.

void stand_moids (void)
{
  MOID_T *m;
  PACK_T *z;
// Primitive A68 moids.
  a68_mode (0, "VOID", &M_VOID);
// Standard precision.
  a68_mode (0, "INT", &M_INT);
  a68_mode (0, "REAL", &M_REAL);
  a68_mode (0, "COMPLEX", &M_COMPLEX);
  a68_mode (0, "COMPL", &M_COMPL);
  a68_mode (0, "BITS", &M_BITS);
  a68_mode (0, "BYTES", &M_BYTES);
// Multiple precision.
  a68_mode (1, "INT", &M_LONG_INT);
  a68_mode (1, "REAL", &M_LONG_REAL);
  a68_mode (1, "COMPLEX", &M_LONG_COMPLEX);
  a68_mode (1, "COMPL", &M_LONG_COMPL);
  a68_mode (1, "BITS", &M_LONG_BITS);
  a68_mode (1, "BYTES", &M_LONG_BYTES);
  a68_mode (2, "REAL", &M_LONG_LONG_REAL);
  a68_mode (2, "INT", &M_LONG_LONG_INT);
  a68_mode (2, "COMPLEX", &M_LONG_LONG_COMPLEX);
  a68_mode (2, "COMPL", &M_LONG_LONG_COMPL);
// Other.
  a68_mode (0, "BOOL", &M_BOOL);
  a68_mode (0, "CHAR", &M_CHAR);
  a68_mode (0, "STRING", &M_STRING);
  a68_mode (0, "FILE", &M_FILE);
  a68_mode (0, "CHANNEL", &M_CHANNEL);
  a68_mode (0, "PIPE", &M_PIPE);
  a68_mode (0, "FORMAT", &M_FORMAT);
  a68_mode (0, "SEMA", &M_SEMA);
  a68_mode (0, "SOUND", &M_SOUND);
  PORTABLE (M_PIPE) = A68_FALSE;
  HAS_ROWS (M_SOUND) = A68_TRUE;
  PORTABLE (M_SOUND) = A68_FALSE;
// ROWS.
  M_ROWS = add_mode (&TOP_MOID (&A68_JOB), ROWS_SYMBOL, 0, NO_NODE, NO_MOID, NO_PACK);
// REFs.
  M_REF_INT = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_INT, NO_PACK);
  M_REF_REAL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_REAL, NO_PACK);
  M_REF_COMPLEX = M_REF_COMPL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_COMPLEX, NO_PACK);
  M_REF_BITS = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_BITS, NO_PACK);
  M_REF_BYTES = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_BYTES, NO_PACK);
  M_REF_FORMAT = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_FORMAT, NO_PACK);
  M_REF_PIPE = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_PIPE, NO_PACK);
// Multiple precision.
  M_REF_LONG_INT = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_INT, NO_PACK);
  M_REF_LONG_REAL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_REAL, NO_PACK);
  M_REF_LONG_COMPLEX = M_REF_LONG_COMPL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_COMPLEX, NO_PACK);
  M_REF_LONG_LONG_INT = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_LONG_INT, NO_PACK);
  M_REF_LONG_LONG_REAL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_LONG_REAL, NO_PACK);
  M_REF_LONG_LONG_COMPLEX = M_REF_LONG_LONG_COMPL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_LONG_COMPLEX, NO_PACK);
  M_REF_LONG_BITS = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_BITS, NO_PACK);
  M_REF_LONG_BYTES = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_BYTES, NO_PACK);
// Other.
  M_REF_BOOL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_BOOL, NO_PACK);
  M_REF_CHAR = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_CHAR, NO_PACK);
  M_REF_FILE = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_FILE, NO_PACK);
  M_REF_REF_FILE = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_REF_FILE, NO_PACK);
  M_REF_SOUND = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_SOUND, NO_PACK);
// [] INT.
  M_ROW_INT = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_INT, NO_PACK);
  HAS_ROWS (M_ROW_INT) = A68_TRUE;
  SLICE (M_ROW_INT) = M_INT;
  M_REF_ROW_INT = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_ROW_INT, NO_PACK);
  NAME (M_REF_ROW_INT) = M_REF_INT;
// [] REAL.
  M_ROW_REAL = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_REAL, NO_PACK);
  HAS_ROWS (M_ROW_REAL) = A68_TRUE;
  SLICE (M_ROW_REAL) = M_REAL;
  M_REF_ROW_REAL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_ROW_REAL, NO_PACK);
  NAME (M_REF_ROW_REAL) = M_REF_REAL;
// [,] REAL.
  M_ROW_ROW_REAL = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 2, NO_NODE, M_REAL, NO_PACK);
  HAS_ROWS (M_ROW_ROW_REAL) = A68_TRUE;
  SLICE (M_ROW_ROW_REAL) = M_ROW_REAL;
  M_REF_ROW_ROW_REAL = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_ROW_ROW_REAL, NO_PACK);
  NAME (M_REF_ROW_ROW_REAL) = M_REF_ROW_REAL;
// [] COMPLEX.
  M_ROW_COMPLEX = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_COMPLEX, NO_PACK);
  HAS_ROWS (M_ROW_COMPLEX) = A68_TRUE;
  SLICE (M_ROW_COMPLEX) = M_COMPLEX;
  M_REF_ROW_COMPLEX = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_ROW_COMPLEX, NO_PACK);
  NAME (M_REF_ROW_COMPLEX) = M_REF_COMPLEX;
// [,] COMPLEX.
  M_ROW_ROW_COMPLEX = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 2, NO_NODE, M_COMPLEX, NO_PACK);
  HAS_ROWS (M_ROW_ROW_COMPLEX) = A68_TRUE;
  SLICE (M_ROW_ROW_COMPLEX) = M_ROW_COMPLEX;
  M_REF_ROW_ROW_COMPLEX = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_ROW_ROW_COMPLEX, NO_PACK);
  NAME (M_REF_ROW_ROW_COMPLEX) = M_REF_ROW_COMPLEX;
// [] BOOL.
  M_ROW_BOOL = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_BOOL, NO_PACK);
  HAS_ROWS (M_ROW_BOOL) = A68_TRUE;
  SLICE (M_ROW_BOOL) = M_BOOL;
// FLEX [] BOOL.
  m = add_mode (&TOP_MOID (&A68_JOB), FLEX_SYMBOL, 0, NO_NODE, M_ROW_BOOL, NO_PACK);
  HAS_ROWS (m) = A68_TRUE;
  M_FLEX_ROW_BOOL = m;
// [] BITS.
  M_ROW_BITS = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_BITS, NO_PACK);
  HAS_ROWS (M_ROW_BITS) = A68_TRUE;
  SLICE (M_ROW_BITS) = M_BITS;
// [] LONG BITS.
  M_ROW_LONG_BITS = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_LONG_BITS, NO_PACK);
  HAS_ROWS (M_ROW_LONG_BITS) = A68_TRUE;
  SLICE (M_ROW_LONG_BITS) = M_LONG_BITS;
// [] CHAR.
  M_ROW_CHAR = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_CHAR, NO_PACK);
  HAS_ROWS (M_ROW_CHAR) = A68_TRUE;
  SLICE (M_ROW_CHAR) = M_CHAR;
// [][] CHAR.
  M_ROW_ROW_CHAR = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_ROW_CHAR, NO_PACK);
  HAS_ROWS (M_ROW_ROW_CHAR) = A68_TRUE;
  SLICE (M_ROW_ROW_CHAR) = M_ROW_CHAR;
// MODE STRING = FLEX [] CHAR.
  m = add_mode (&TOP_MOID (&A68_JOB), FLEX_SYMBOL, 0, NO_NODE, M_ROW_CHAR, NO_PACK);
  HAS_ROWS (m) = A68_TRUE;
  M_FLEX_ROW_CHAR = m;
  EQUIVALENT (M_STRING) = m;
// REF [] CHAR.
  M_REF_ROW_CHAR = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_ROW_CHAR, NO_PACK);
  NAME (M_REF_ROW_CHAR) = M_REF_CHAR;
// PROC [] CHAR.
  M_PROC_ROW_CHAR = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, 0, NO_NODE, M_ROW_CHAR, NO_PACK);
// REF STRING = REF FLEX [] CHAR.
  M_REF_STRING = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, EQUIVALENT (M_STRING), NO_PACK);
  NAME (M_REF_STRING) = M_REF_CHAR;
  DEFLEXED (M_REF_STRING) = M_REF_ROW_CHAR;
// [] STRING.
  M_ROW_STRING = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_STRING, NO_PACK);
  HAS_ROWS (M_ROW_STRING) = A68_TRUE;
  SLICE (M_ROW_STRING) = M_STRING;
  DEFLEXED (M_ROW_STRING) = M_ROW_ROW_CHAR;
// PROC STRING.
  M_PROC_STRING = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, 0, NO_NODE, M_STRING, NO_PACK);
  DEFLEXED (M_PROC_STRING) = M_PROC_ROW_CHAR;
// COMPLEX.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REAL, TEXT (add_token (&A68 (top_token), "im")), NO_NODE);
  (void) add_mode_to_pack (&z, M_REAL, TEXT (add_token (&A68 (top_token), "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  EQUIVALENT (M_COMPLEX) = EQUIVALENT (M_COMPL) = m;
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REF_REAL, TEXT (add_token (&A68 (top_token), "im")), NO_NODE);
  (void) add_mode_to_pack (&z, M_REF_REAL, TEXT (add_token (&A68 (top_token), "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  NAME (M_REF_COMPLEX) = NAME (M_REF_COMPL) = m;
// LONG COMPLEX.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_LONG_REAL, TEXT (add_token (&A68 (top_token), "im")), NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_REAL, TEXT (add_token (&A68 (top_token), "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  EQUIVALENT (M_LONG_COMPLEX) = EQUIVALENT (M_LONG_COMPL) = m;
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REF_LONG_REAL, TEXT (add_token (&A68 (top_token), "im")), NO_NODE);
  (void) add_mode_to_pack (&z, M_REF_LONG_REAL, TEXT (add_token (&A68 (top_token), "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  NAME (M_REF_LONG_COMPLEX) = NAME (M_REF_LONG_COMPL) = m;
// LONG_LONG COMPLEX.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_LONG_LONG_REAL, TEXT (add_token (&A68 (top_token), "im")), NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_LONG_REAL, TEXT (add_token (&A68 (top_token), "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  EQUIVALENT (M_LONG_LONG_COMPLEX) = EQUIVALENT (M_LONG_LONG_COMPL) = m;
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REF_LONG_LONG_REAL, TEXT (add_token (&A68 (top_token), "im")), NO_NODE);
  (void) add_mode_to_pack (&z, M_REF_LONG_LONG_REAL, TEXT (add_token (&A68 (top_token), "re")), NO_NODE);
  m = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  NAME (M_REF_LONG_LONG_COMPLEX) = NAME (M_REF_LONG_LONG_COMPL) = m;
// NUMBER.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_INT, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_INT, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_LONG_INT, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_REAL, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_REAL, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_LONG_REAL, NO_TEXT, NO_NODE);
  M_NUMBER = add_mode (&TOP_MOID (&A68_JOB), UNION_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
// HEX_NUMBER.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_BOOL, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_CHAR, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_INT, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_INT, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_REAL, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_REAL, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_BITS, NO_TEXT, NO_NODE);
  (void) add_mode_to_pack (&z, M_LONG_BITS, NO_TEXT, NO_NODE);
  M_HEX_NUMBER = add_mode (&TOP_MOID (&A68_JOB), UNION_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
// SEMA.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REF_INT, NO_TEXT, NO_NODE);
  EQUIVALENT (M_SEMA) = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
// PROC VOID.
  z = NO_PACK;
  M_PROC_VOID = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, count_pack_members (z), NO_NODE, M_VOID, z);
// PROC (REAL) REAL.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REAL, NO_TEXT, NO_NODE);
  M_PROC_REAL_REAL = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, count_pack_members (z), NO_NODE, M_REAL, z);
// PROC (LONG_REAL) LONG_REAL.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_LONG_REAL, NO_TEXT, NO_NODE);
  M_PROC_LONG_REAL_LONG_REAL = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, count_pack_members (z), NO_NODE, M_LONG_REAL, z);
// IO: PROC (REF FILE) BOOL.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REF_FILE, NO_TEXT, NO_NODE);
  M_PROC_REF_FILE_BOOL = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, count_pack_members (z), NO_NODE, M_BOOL, z);
// IO: PROC (REF FILE) VOID.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REF_FILE, NO_TEXT, NO_NODE);
  M_PROC_REF_FILE_VOID = add_mode (&TOP_MOID (&A68_JOB), PROC_SYMBOL, count_pack_members (z), NO_NODE, M_VOID, z);
// IO: SIMPLIN and SIMPLOUT.
  M_SIMPLIN = add_mode (&TOP_MOID (&A68_JOB), IN_TYPE_MODE, 0, NO_NODE, NO_MOID, NO_PACK);
  M_ROW_SIMPLIN = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_SIMPLIN, NO_PACK);
  SLICE (M_ROW_SIMPLIN) = M_SIMPLIN;
  M_SIMPLOUT = add_mode (&TOP_MOID (&A68_JOB), OUT_TYPE_MODE, 0, NO_NODE, NO_MOID, NO_PACK);
  M_ROW_SIMPLOUT = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_SIMPLOUT, NO_PACK);
  SLICE (M_ROW_SIMPLOUT) = M_SIMPLOUT;
// PIPE.
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_INT, TEXT (add_token (&A68 (top_token), "pid")), NO_NODE);
  (void) add_mode_to_pack (&z, M_REF_FILE, TEXT (add_token (&A68 (top_token), "write")), NO_NODE);
  (void) add_mode_to_pack (&z, M_REF_FILE, TEXT (add_token (&A68 (top_token), "read")), NO_NODE);
  EQUIVALENT (M_PIPE) = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
  z = NO_PACK;
  (void) add_mode_to_pack (&z, M_REF_INT, TEXT (add_token (&A68 (top_token), "pid")), NO_NODE);
  (void) add_mode_to_pack (&z, M_REF_REF_FILE, TEXT (add_token (&A68 (top_token), "write")), NO_NODE);
  (void) add_mode_to_pack (&z, M_REF_REF_FILE, TEXT (add_token (&A68 (top_token), "read")), NO_NODE);
  NAME (M_REF_PIPE) = add_mode (&TOP_MOID (&A68_JOB), STRUCT_SYMBOL, count_pack_members (z), NO_NODE, NO_MOID, z);
}

//! @brief Set up standenv - general RR but not transput.

void stand_prelude (void)
{
  MOID_T *m;
// Identifiers.
  a68_idf (A68_STD, "intlengths", M_INT, genie_int_lengths);
  a68_idf (A68_STD, "intshorths", M_INT, genie_int_shorths);
  a68_idf (A68_STD, "infinity", M_REAL, genie_infinity_real);
  a68_idf (A68_STD, "minusinfinity", M_REAL, genie_minus_infinity_real);
  a68_idf (A68_STD, "inf", M_REAL, genie_infinity_real);
  a68_idf (A68_STD, "mininf", M_REAL, genie_minus_infinity_real);
  a68_idf (A68_STD, "maxint", M_INT, genie_max_int);
  a68_idf (A68_STD, "mpradix", M_INT, genie_mp_radix);
  a68_idf (A68_STD, "maxreal", M_REAL, genie_max_real);
  a68_idf (A68_STD, "minreal", M_REAL, genie_min_real);
  a68_idf (A68_STD, "smallreal", M_REAL, genie_small_real);
  a68_idf (A68_STD, "reallengths", M_INT, genie_real_lengths);
  a68_idf (A68_STD, "realshorths", M_INT, genie_real_shorths);
  a68_idf (A68_STD, "compllengths", M_INT, genie_complex_lengths);
  a68_idf (A68_STD, "complshorths", M_INT, genie_complex_shorths);
  a68_idf (A68_STD, "bitslengths", M_INT, genie_bits_lengths);
  a68_idf (A68_STD, "bitsshorths", M_INT, genie_bits_shorths);
  a68_idf (A68_STD, "bitswidth", M_INT, genie_bits_width);
  a68_idf (A68_STD, "longbitswidth", M_INT, genie_long_bits_width);
  a68_idf (A68_STD, "maxbits", M_BITS, genie_max_bits);
  a68_idf (A68_STD, "byteslengths", M_INT, genie_bytes_lengths);
  a68_idf (A68_STD, "bytesshorths", M_INT, genie_bytes_shorths);
  a68_idf (A68_STD, "byteswidth", M_INT, genie_bytes_width);
  a68_idf (A68_STD, "maxabschar", M_INT, genie_max_abs_char);
  a68_idf (A68_STD, "pi", M_REAL, genie_pi);
  a68_idf (A68_STD, "qpi", M_LONG_LONG_REAL, genie_pi_mp);
  a68_idf (A68_STD, "longlongpi", M_LONG_LONG_REAL, genie_pi_mp);
  a68_idf (A68_STD, "intwidth", M_INT, genie_int_width);
  a68_idf (A68_STD, "realwidth", M_INT, genie_real_width);
  a68_idf (A68_STD, "expwidth", M_INT, genie_exp_width);
  a68_idf (A68_STD, "longintwidth", M_INT, genie_long_int_width);
  a68_idf (A68_STD, "longlongintwidth", M_INT, genie_long_mp_int_width);
  a68_idf (A68_STD, "longrealwidth", M_INT, genie_long_real_width);
  a68_idf (A68_STD, "longlongrealwidth", M_INT, genie_long_mp_real_width);
  a68_idf (A68_STD, "longexpwidth", M_INT, genie_long_exp_width);
  a68_idf (A68_STD, "longlongexpwidth", M_INT, genie_long_mp_exp_width);
  a68_idf (A68_STD, "longlongmaxint", M_LONG_LONG_INT, genie_long_mp_max_int);
  a68_idf (A68_STD, "longlongsmallreal", M_LONG_LONG_REAL, genie_long_mp_small_real);
  a68_idf (A68_STD, "longlongmaxreal", M_LONG_LONG_REAL, genie_long_mp_max_real);
  a68_idf (A68_STD, "longlongminreal", M_LONG_LONG_REAL, genie_long_mp_min_real);
  a68_idf (A68_STD, "longlonginfinity", M_LONG_LONG_REAL, genie_infinity_mp);
  a68_idf (A68_STD, "longlongminusinfinity", M_LONG_LONG_REAL, genie_minus_infinity_mp);
  a68_idf (A68_STD, "longlonginf", M_LONG_LONG_REAL, genie_infinity_mp);
  a68_idf (A68_STD, "longlongmininf", M_LONG_LONG_REAL, genie_minus_infinity_mp);
  a68_idf (A68_STD, "longbyteswidth", M_INT, genie_long_bytes_width);
  a68_idf (A68_EXT, "seconds", M_REAL, genie_cputime);
  a68_idf (A68_EXT, "clock", M_REAL, genie_cputime);
  a68_idf (A68_EXT, "cputime", M_REAL, genie_cputime);
  m = a68_proc (M_VOID, A68_MCACHE (proc_void), NO_MOID);
  a68_idf (A68_EXT, "ongcevent", m, genie_on_gc_event);
  a68_idf (A68_EXT, "collections", A68_MCACHE (proc_int), genie_garbage_collections);
  a68_idf (A68_EXT, "garbagecollections", A68_MCACHE (proc_int), genie_garbage_collections);
  a68_idf (A68_EXT, "garbagerefused", A68_MCACHE (proc_int), genie_garbage_refused);
  a68_idf (A68_EXT, "blocks", A68_MCACHE (proc_int), genie_block);
  a68_idf (A68_EXT, "garbage", A68_MCACHE (proc_int), genie_garbage_freed);
  a68_idf (A68_EXT, "garbagefreed", A68_MCACHE (proc_int), genie_garbage_freed);
  a68_idf (A68_EXT, "collectseconds", A68_MCACHE (proc_real), genie_garbage_seconds);
  a68_idf (A68_EXT, "garbageseconds", A68_MCACHE (proc_real), genie_garbage_seconds);
  a68_idf (A68_EXT, "stackpointer", M_INT, genie_stack_pointer);
  a68_idf (A68_EXT, "systemstackpointer", M_INT, genie_system_stack_pointer);
  a68_idf (A68_EXT, "systemstacksize", M_INT, genie_system_stack_size);
  a68_idf (A68_EXT, "actualstacksize", M_INT, genie_stack_pointer);
  a68_idf (A68_EXT, "heappointer", M_INT, genie_system_heap_pointer);
  a68_idf (A68_EXT, "systemheappointer", M_INT, genie_system_heap_pointer);
  a68_idf (A68_EXT, "gcheap", A68_MCACHE (proc_void), genie_gc_heap);
  a68_idf (A68_EXT, "sweepheap", A68_MCACHE (proc_void), genie_gc_heap);
  a68_idf (A68_EXT, "preemptivegc", A68_MCACHE (proc_void), genie_preemptive_gc_heap);
  a68_idf (A68_EXT, "preemptivesweep", A68_MCACHE (proc_void), genie_preemptive_gc_heap);
  a68_idf (A68_EXT, "preemptivesweepheap", A68_MCACHE (proc_void), genie_preemptive_gc_heap);
  a68_idf (A68_EXT, "backtrace", A68_MCACHE (proc_void), genie_backtrace);
  a68_idf (A68_EXT, "break", A68_MCACHE (proc_void), genie_break);
  a68_idf (A68_EXT, "debug", A68_MCACHE (proc_void), genie_debug);
  a68_idf (A68_EXT, "monitor", A68_MCACHE (proc_void), genie_debug);
  m = a68_proc (M_VOID, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "abend", m, genie_abend);
  m = a68_proc (M_STRING, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "evaluate", m, genie_evaluate);
  m = a68_proc (M_INT, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "system", m, genie_system);
  m = a68_proc (M_INT, M_INT, NO_MOID);
  a68_idf (A68_EXT, "sleep", m, genie_sleep);
// BITS procedures.
  m = a68_proc (M_BITS, M_ROW_BOOL, NO_MOID);
  a68_idf (A68_STD, "bitspack", m, genie_bits_pack);
// RNG procedures.
  m = a68_proc (M_VOID, M_INT, NO_MOID);
  a68_idf (A68_STD, "firstrandom", m, genie_first_random);
  m = A68_MCACHE (proc_real);
  a68_idf (A68_STD, "nextrandom", m, genie_next_random);
  a68_idf (A68_STD, "random", m, genie_next_random);
  a68_idf (A68_STD, "rnd", m, genie_next_rnd);
  m = a68_proc (M_LONG_LONG_REAL, NO_MOID);
  a68_idf (A68_STD, "longlongnextrandom", m, genie_long_next_random);
  a68_idf (A68_STD, "longlongrandom", m, genie_long_next_random);
// Priorities.
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
  a68_prio ("ROL", 8);
  a68_prio ("ROR", 8);
  a68_prio ("UP", 8);
  a68_prio ("DOWN", 8);
  a68_prio ("^", 8);
  a68_prio ("ELEMS", 8);
  a68_prio ("LWB", 8);
  a68_prio ("UPB", 8);
  a68_prio ("SORT", 8);
  a68_prio ("I", 9);
  a68_prio ("+*", 9);
// INT ops.
  m = a68_proc (M_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_int);
  a68_op (A68_STD, "ABS", m, genie_abs_int);
  a68_op (A68_STD, "SIGN", m, genie_sign_int);
//
  m = a68_proc (M_BOOL, M_INT, NO_MOID);
  a68_op (A68_STD, "ODD", m, genie_odd_int);
//
  m = a68_proc (M_BOOL, M_INT, M_INT, NO_MOID);
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
//
  m = a68_proc (M_INT, M_INT, M_INT, NO_MOID);
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
//
  m = a68_proc (M_REAL, M_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "/", m, genie_div_int);
//
  m = a68_proc (M_REF_INT, M_REF_INT, M_INT, NO_MOID);
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
// REAL ops.
  m = A68_MCACHE (proc_real_real);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_real);
  a68_op (A68_STD, "ABS", m, genie_abs_real);
  m = a68_proc (M_INT, M_REAL, NO_MOID);
  a68_op (A68_STD, "SIGN", m, genie_sign_real);
  a68_op (A68_STD, "ROUND", m, genie_round_real);
  a68_op (A68_STD, "ENTIER", m, genie_entier_real);
//
  m = a68_proc (M_BOOL, M_REAL, M_REAL, NO_MOID);
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
//
  m = A68_MCACHE (proc_real_real_real);
  a68_op (A68_STD, "+", m, genie_add_real);
  a68_op (A68_STD, "-", m, genie_sub_real);
  a68_op (A68_STD, "*", m, genie_mul_real);
  a68_op (A68_STD, "/", m, genie_div_real);
  a68_op (A68_STD, "**", m, genie_pow_real);
  a68_op (A68_STD, "UP", m, genie_pow_real);
  a68_op (A68_STD, "^", m, genie_pow_real);
//
  m = a68_proc (M_REAL, M_REAL, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_real_int);
  a68_op (A68_STD, "UP", m, genie_pow_real_int);
  a68_op (A68_STD, "^", m, genie_pow_real_int);
//
  m = a68_proc (M_REF_REAL, M_REF_REAL, M_REAL, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_real);
  a68_op (A68_STD, "-:=", m, genie_minusab_real);
  a68_op (A68_STD, "*:=", m, genie_timesab_real);
  a68_op (A68_STD, "/:=", m, genie_divab_real);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_real);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_real);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_real);
  a68_op (A68_STD, "DIVAB", m, genie_divab_real);
// Procedures
  m = A68_MCACHE (proc_real_real);
  a68_idf (A68_EXT, "acosdg", m, genie_acosdg_real);
  a68_idf (A68_EXT, "acosh", m, genie_acosh_real);
  a68_idf (A68_EXT, "acos", m, genie_acos_real);
  a68_idf (A68_EXT, "acotdg", m, genie_acotdg_real);
  a68_idf (A68_EXT, "acot", m, genie_acot_real);
  a68_idf (A68_EXT, "acsc", m, genie_acsc_real);
  a68_idf (A68_EXT, "arccosdg", m, genie_acosdg_real);
  a68_idf (A68_EXT, "arccosh", m, genie_acosh_real);
  a68_idf (A68_EXT, "arccotdg", m, genie_acotdg_real);
  a68_idf (A68_EXT, "arccot", m, genie_acot_real);
  a68_idf (A68_EXT, "arccsc", m, genie_acsc_real);
  a68_idf (A68_EXT, "arcsec", m, genie_asec_real);
  a68_idf (A68_EXT, "arcsindg", m, genie_asindg_real);
  a68_idf (A68_EXT, "arcsinh", m, genie_asinh_real);
  a68_idf (A68_EXT, "arctandg", m, genie_atandg_real);
  a68_idf (A68_EXT, "arctanh", m, genie_atanh_real);
  a68_idf (A68_EXT, "asec", m, genie_asec_real);
  a68_idf (A68_EXT, "asindg", m, genie_asindg_real);
  a68_idf (A68_EXT, "asinh", m, genie_asinh_real);
  a68_idf (A68_EXT, "asin", m, genie_asin_real);
  a68_idf (A68_EXT, "atandg", m, genie_atandg_real);
  a68_idf (A68_EXT, "atanh", m, genie_atanh_real);
  a68_idf (A68_EXT, "atan", m, genie_atan_real);
  a68_idf (A68_EXT, "cbrt", m, genie_curt_real);
  a68_idf (A68_EXT, "cosdg", m, genie_cosdg_real);
  a68_idf (A68_EXT, "cosh", m, genie_cosh_real);
  a68_idf (A68_EXT, "cospi", m, genie_cospi_real);
  a68_idf (A68_EXT, "cotdg", m, genie_cotdg_real);
  a68_idf (A68_EXT, "cot", m, genie_cot_real);
  a68_idf (A68_EXT, "cotpi", m, genie_cotpi_real);
  a68_idf (A68_EXT, "csc", m, genie_csc_real);
  a68_idf (A68_EXT, "curt", m, genie_curt_real);
  a68_idf (A68_EXT, "erfc", m, genie_erfc_real);
  a68_idf (A68_EXT, "erf", m, genie_erf_real);
  a68_idf (A68_EXT, "gamma", m, genie_gamma_real);
  a68_idf (A68_EXT, "inverfc", m, genie_inverfc_real);
  a68_idf (A68_EXT, "inverf", m, genie_inverf_real);
  a68_idf (A68_EXT, "inverseerfc", m, genie_inverfc_real);
  a68_idf (A68_EXT, "inverseerf", m, genie_inverf_real);
  a68_idf (A68_EXT, "ln1p", m, genie_ln1p_real);
  a68_idf (A68_EXT, "lngamma", m, genie_ln_gamma_real);
  a68_idf (A68_EXT, "sec", m, genie_sec_real);
  a68_idf (A68_EXT, "sindg", m, genie_sindg_real);
  a68_idf (A68_EXT, "sinh", m, genie_sinh_real);
  a68_idf (A68_EXT, "sinpi", m, genie_sinpi_real);
  a68_idf (A68_EXT, "tandg", m, genie_tandg_real);
  a68_idf (A68_EXT, "tanh", m, genie_tanh_real);
  a68_idf (A68_EXT, "tanpi", m, genie_tanpi_real);
  a68_idf (A68_STD, "arccos", m, genie_acos_real);
  a68_idf (A68_STD, "arcsin", m, genie_asin_real);
  a68_idf (A68_STD, "arctan", m, genie_atan_real);
  a68_idf (A68_STD, "cos", m, genie_cos_real);
  a68_idf (A68_STD, "exp", m, genie_exp_real);
  a68_idf (A68_STD, "ln", m, genie_ln_real);
  a68_idf (A68_STD, "log", m, genie_log_real);
  a68_idf (A68_STD, "sin", m, genie_sin_real);
  a68_idf (A68_STD, "sqrt", m, genie_sqrt_real);
  a68_idf (A68_STD, "tan", m, genie_tan_real);
// Miscellaneous.
  a68_idf (A68_EXT, "arctan2", A68_MCACHE (proc_real_real_real), genie_atan2_real);
  a68_idf (A68_EXT, "arctan2dg", A68_MCACHE (proc_real_real_real), genie_atan2dg_real);
  a68_idf (A68_EXT, "beta", A68_MCACHE (proc_real_real_real), genie_beta_real);
  a68_idf (A68_EXT, "betainc", A68_MCACHE (proc_real_real_real_real), genie_beta_inc_cf_real);
  a68_idf (A68_EXT, "choose", A68_MCACHE (proc_int_int_real), genie_choose_real);
  a68_idf (A68_EXT, "fact", A68_MCACHE (proc_int_real), genie_fact_real);
  a68_idf (A68_EXT, "gammainc", A68_MCACHE (proc_real_real_real), genie_gamma_inc_h_real);
  a68_idf (A68_EXT, "gammaincf", A68_MCACHE (proc_real_real_real), genie_gamma_inc_f_real);
  a68_idf (A68_EXT, "gammaincg", A68_MCACHE (proc_real_real_real_real_real), genie_gamma_inc_g_real);
  a68_idf (A68_EXT, "gammaincgf", A68_MCACHE (proc_real_real_real), genie_gamma_inc_gf_real);
  a68_idf (A68_EXT, "lje126", A68_MCACHE (proc_real_real_real_real), genie_lj_e_12_6);
  a68_idf (A68_EXT, "ljf126", A68_MCACHE (proc_real_real_real_real), genie_lj_f_12_6);
  a68_idf (A68_EXT, "lnbeta", A68_MCACHE (proc_real_real_real), genie_ln_beta_real);
  a68_idf (A68_EXT, "lnchoose", A68_MCACHE (proc_int_int_real), genie_ln_choose_real);
  a68_idf (A68_EXT, "lnfact", A68_MCACHE (proc_int_real), genie_ln_fact_real);
// COMPLEX ops.
  m = a68_proc (M_COMPLEX, M_REAL, M_REAL, NO_MOID);
  a68_op (A68_STD, "I", m, genie_i_complex);
  a68_op (A68_STD, "+*", m, genie_i_complex);
//
  m = a68_proc (M_COMPLEX, M_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "I", m, genie_i_int_complex);
  a68_op (A68_STD, "+*", m, genie_i_int_complex);
//
  m = a68_proc (M_REAL, M_COMPLEX, NO_MOID);
  a68_op (A68_STD, "RE", m, genie_re_complex);
  a68_op (A68_STD, "IM", m, genie_im_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_complex);
//
  m = A68_MCACHE (proc_complex_complex);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_complex);
//
  m = a68_proc (M_BOOL, M_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_complex);
  a68_op (A68_STD, "/=", m, genie_ne_complex);
  a68_op (A68_STD, "~=", m, genie_ne_complex);
  a68_op (A68_STD, "^=", m, genie_ne_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_complex);
  a68_op (A68_STD, "NE", m, genie_ne_complex);
//
  m = a68_proc (M_COMPLEX, M_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_complex);
  a68_op (A68_STD, "-", m, genie_sub_complex);
  a68_op (A68_STD, "*", m, genie_mul_complex);
  a68_op (A68_STD, "/", m, genie_div_complex);
//
  m = a68_proc (M_COMPLEX, M_COMPLEX, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_complex_int);
//
  m = a68_proc (M_REF_COMPLEX, M_REF_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_complex);
//
  m = A68_MCACHE (proc_complex_complex);
  a68_idf (A68_EXT, "cacosh", m, genie_acosh_complex);
  a68_idf (A68_EXT, "cacos", m, genie_acos_complex);
  a68_idf (A68_EXT, "carccosh", m, genie_acosh_complex);
  a68_idf (A68_EXT, "carccos", m, genie_acos_complex);
  a68_idf (A68_EXT, "carcsinh", m, genie_asinh_complex);
  a68_idf (A68_EXT, "carcsin", m, genie_asin_complex);
  a68_idf (A68_EXT, "carctanh", m, genie_atanh_complex);
  a68_idf (A68_EXT, "carctan", m, genie_atan_complex);
  a68_idf (A68_EXT, "casinh", m, genie_asinh_complex);
  a68_idf (A68_EXT, "casin", m, genie_asin_complex);
  a68_idf (A68_EXT, "catanh", m, genie_atanh_complex);
  a68_idf (A68_EXT, "catan", m, genie_atan_complex);
  a68_idf (A68_EXT, "ccosh", m, genie_cosh_complex);
  a68_idf (A68_EXT, "ccos", m, genie_cos_complex);
  a68_idf (A68_EXT, "cexp", m, genie_exp_complex);
  a68_idf (A68_EXT, "cln", m, genie_ln_complex);
  a68_idf (A68_EXT, "complexacosh", m, genie_acosh_complex);
  a68_idf (A68_EXT, "complexacos", m, genie_acos_complex);
  a68_idf (A68_EXT, "complexarccosh", m, genie_acosh_complex);
  a68_idf (A68_EXT, "complexarccos", m, genie_acos_complex);
  a68_idf (A68_EXT, "complexarcsinh", m, genie_asinh_complex);
  a68_idf (A68_EXT, "complexarcsin", m, genie_asin_complex);
  a68_idf (A68_EXT, "complexarctanh", m, genie_atanh_complex);
  a68_idf (A68_EXT, "complexarctan", m, genie_atan_complex);
  a68_idf (A68_EXT, "complexasinh", m, genie_asinh_complex);
  a68_idf (A68_EXT, "complexasin", m, genie_asin_complex);
  a68_idf (A68_EXT, "complexatanh", m, genie_atanh_complex);
  a68_idf (A68_EXT, "complexatan", m, genie_atan_complex);
  a68_idf (A68_EXT, "complexcosh", m, genie_cosh_complex);
  a68_idf (A68_EXT, "complexcos", m, genie_cos_complex);
  a68_idf (A68_EXT, "complexexp", m, genie_exp_complex);
  a68_idf (A68_EXT, "complexln", m, genie_ln_complex);
  a68_idf (A68_EXT, "complexsinh", m, genie_sinh_complex);
  a68_idf (A68_EXT, "complexsin", m, genie_sin_complex);
  a68_idf (A68_EXT, "complexsqrt", m, genie_sqrt_complex);
  a68_idf (A68_EXT, "complextanh", m, genie_tanh_complex);
  a68_idf (A68_EXT, "complextan", m, genie_tan_complex);
  a68_idf (A68_EXT, "csinh", m, genie_sinh_complex);
  a68_idf (A68_EXT, "csin", m, genie_sin_complex);
  a68_idf (A68_EXT, "csqrt", m, genie_sqrt_complex);
  a68_idf (A68_EXT, "ctanh", m, genie_tanh_complex);
  a68_idf (A68_EXT, "ctan", m, genie_tan_complex);
// BOOL ops.
  m = a68_proc (M_BOOL, M_BOOL, NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_bool);
  a68_op (A68_STD, "~", m, genie_not_bool);
  m = a68_proc (M_INT, M_BOOL, NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_abs_bool);
  m = a68_proc (M_BOOL, M_BOOL, M_BOOL, NO_MOID);
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
// CHAR ops.
  m = a68_proc (M_BOOL, M_CHAR, M_CHAR, NO_MOID);
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
  m = a68_proc (M_INT, M_CHAR, NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_abs_char);
  m = a68_proc (M_CHAR, M_INT, NO_MOID);
  a68_op (A68_STD, "REPR", m, genie_repr_char);
  m = a68_proc (M_BOOL, M_CHAR, NO_MOID);
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
  m = a68_proc (M_CHAR, M_CHAR, NO_MOID);
  a68_idf (A68_EXT, "tolower", m, genie_to_lower);
  a68_idf (A68_EXT, "toupper", m, genie_to_upper);
// BITS ops.
  m = a68_proc (M_INT, M_BITS, NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_abs_bits);
//
  m = a68_proc (M_BITS, M_INT, NO_MOID);
  a68_op (A68_STD, "BIN", m, genie_bin_int);
//
  m = a68_proc (M_BITS, M_BITS, NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_bits);
  a68_op (A68_STD, "~", m, genie_not_bits);
//
  m = a68_proc (M_BOOL, M_BITS, M_BITS, NO_MOID);
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
#if (A68_LEVEL >= 3)
  a68_op (A68_EXT, "<", m, genie_lt_bits);
  a68_op (A68_EXT, ">", m, genie_gt_bits);
  a68_op (A68_EXT, "LT", m, genie_lt_bits);
  a68_op (A68_EXT, "GT", m, genie_gt_bits);
#endif
//
  m = a68_proc (M_BITS, M_BITS, M_BITS, NO_MOID);
  a68_op (A68_STD, "AND", m, genie_and_bits);
  a68_op (A68_STD, "&", m, genie_and_bits);
  a68_op (A68_STD, "OR", m, genie_or_bits);
  a68_op (A68_EXT, "XOR", m, genie_xor_bits);
  a68_op (A68_EXT, "+", m, genie_add_bits);
  a68_op (A68_EXT, "-", m, genie_sub_bits);
  a68_op (A68_EXT, "*", m, genie_times_bits);
  a68_op (A68_EXT, "OVER", m, genie_over_bits);
  a68_op (A68_EXT, "MOD", m, genie_over_bits);
//
  m = a68_proc (M_BITS, M_BITS, M_INT, NO_MOID);
  a68_op (A68_STD, "SHL", m, genie_shl_bits);
  a68_op (A68_STD, "UP", m, genie_shl_bits);
  a68_op (A68_STD, "SHR", m, genie_shr_bits);
  a68_op (A68_STD, "DOWN", m, genie_shr_bits);
  a68_op (A68_EXT, "ROL", m, genie_rol_bits);
  a68_op (A68_EXT, "ROR", m, genie_ror_bits);
//
  m = a68_proc (M_BOOL, M_INT, M_BITS, NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_bits);
//
  m = a68_proc (M_BITS, M_INT, M_BITS, NO_MOID);
  a68_op (A68_STD, "SET", m, genie_set_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_bits);
// LONG LONG INT in software
  m = a68_proc (M_LONG_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_mp);
//
  m = a68_proc (M_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "SIGN", m, genie_sign_mp);
//
  m = a68_proc (M_BOOL, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "ODD", m, genie_odd_mp);
//
  m = a68_proc (M_LONG_LONG_INT, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "ENTIER", m, genie_entier_mp);
  a68_op (A68_STD, "ROUND", m, genie_round_mp);
//
  m = a68_proc (M_LONG_LONG_INT, M_LONG_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_mp_int);
  a68_op (A68_STD, "-", m, genie_sub_mp_int);
  a68_op (A68_STD, "*", m, genie_mul_mp_int);
  a68_op (A68_STD, "OVER", m, genie_over_mp);
  a68_op (A68_STD, "%", m, genie_over_mp);
  a68_op (A68_STD, "MOD", m, genie_mod_mp);
  a68_op (A68_STD, "%*", m, genie_mod_mp);
//
  m = a68_proc (M_REF_LONG_LONG_INT, M_REF_LONG_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_mp_int);
  a68_op (A68_STD, "-:=", m, genie_minusab_mp_int);
  a68_op (A68_STD, "*:=", m, genie_timesab_mp_int);
  a68_op (A68_STD, "%:=", m, genie_overab_mp);
  a68_op (A68_STD, "%*:=", m, genie_modab_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_mp_int);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_mp_int);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_mp_int);
  a68_op (A68_STD, "OVERAB", m, genie_overab_mp);
  a68_op (A68_STD, "MODAB", m, genie_modab_mp);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "/", m, genie_div_mp);
//
  m = a68_proc (M_BOOL, M_LONG_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "EQ", m, genie_eq_mp);
  a68_op (A68_STD, "NE", m, genie_ne_mp);
  a68_op (A68_STD, "GE", m, genie_ge_mp);
  a68_op (A68_STD, "GT", m, genie_gt_mp);
  a68_op (A68_STD, "LE", m, genie_le_mp);
  a68_op (A68_STD, "LT", m, genie_lt_mp);
  a68_op (A68_STD, "=", m, genie_eq_mp);
  a68_op (A68_STD, ">=", m, genie_ge_mp);
  a68_op (A68_STD, ">", m, genie_gt_mp);
  a68_op (A68_STD, "<=", m, genie_le_mp);
  a68_op (A68_STD, "<", m, genie_lt_mp);
  a68_op (A68_STD, "/=", m, genie_ne_mp);
  a68_op (A68_STD, "^=", m, genie_ne_mp);
  a68_op (A68_STD, "~=", m, genie_ne_mp);
//
  m = a68_proc (M_LONG_LONG_INT, M_LONG_LONG_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_mp_int_int);
  a68_op (A68_STD, "^", m, genie_pow_mp_int_int);
//
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
// LONG LONG REAL in software
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_abs_mp);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_mp);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "longlongarccosdg", m, genie_acosdg_mp);
  a68_idf (A68_EXT, "longlongarccosh", m, genie_acosh_mp);
  a68_idf (A68_EXT, "longlongarccotdg", m, genie_acotdg_mp);
  a68_idf (A68_EXT, "longlongarccot", m, genie_acot_mp);
  a68_idf (A68_EXT, "longlongarccsc", m, genie_acsc_mp);
  a68_idf (A68_EXT, "longlongarcsec", m, genie_asec_mp);
  a68_idf (A68_EXT, "longlongarcsindg", m, genie_asindg_mp);
  a68_idf (A68_EXT, "longlongarcsinh", m, genie_asinh_mp);
  a68_idf (A68_EXT, "longlongarctandg", m, genie_atandg_mp);
  a68_idf (A68_EXT, "longlongarctanh", m, genie_atanh_mp);
  a68_idf (A68_EXT, "longlongcbrt", m, genie_curt_mp);
  a68_idf (A68_EXT, "longlongcosdg", m, genie_cosdg_mp);
  a68_idf (A68_EXT, "longlongcosh", m, genie_cosh_mp);
  a68_idf (A68_EXT, "longlongcospi", m, genie_cospi_mp);
  a68_idf (A68_EXT, "longlongcotdg", m, genie_cotdg_mp);
  a68_idf (A68_EXT, "longlongcot", m, genie_cot_mp);
  a68_idf (A68_EXT, "longlongcotpi", m, genie_cotpi_mp);
  a68_idf (A68_EXT, "longlongcsc", m, genie_csc_mp);
  a68_idf (A68_EXT, "longlongcurt", m, genie_curt_mp);
  a68_idf (A68_EXT, "longlongerfc", m, genie_erfc_mp);
  a68_idf (A68_EXT, "longlongerf", m, genie_erf_mp);
  a68_idf (A68_EXT, "longlonggamma", m, genie_gamma_mp);
  a68_idf (A68_EXT, "longlonginverfc", m, genie_inverfc_mp);
  a68_idf (A68_EXT, "longlonginverf", m, genie_inverf_mp);
  a68_idf (A68_EXT, "longlonglngamma", m, genie_lngamma_mp);
  a68_idf (A68_EXT, "longlongsec", m, genie_sec_mp);
  a68_idf (A68_EXT, "longlongsindg", m, genie_sindg_mp);
  a68_idf (A68_EXT, "longlongsinh", m, genie_sinh_mp);
  a68_idf (A68_EXT, "longlongsinpi", m, genie_sinpi_mp);
  a68_idf (A68_EXT, "longlongtandg", m, genie_tandg_mp);
  a68_idf (A68_EXT, "longlongtanh", m, genie_tanh_mp);
  a68_idf (A68_EXT, "longlongtan", m, genie_tan_mp);
  a68_idf (A68_EXT, "longlongtanpi", m, genie_tanpi_mp);
  a68_idf (A68_EXT, "qacosdg", m, genie_acosdg_mp);
  a68_idf (A68_EXT, "qacosh", m, genie_acosh_mp);
  a68_idf (A68_EXT, "qacos", m, genie_acos_mp);
  a68_idf (A68_EXT, "qacotdg", m, genie_acotdg_mp);
  a68_idf (A68_EXT, "qacot", m, genie_acot_mp);
  a68_idf (A68_EXT, "qacsc", m, genie_acsc_mp);
  a68_idf (A68_EXT, "qasec", m, genie_asec_mp);
  a68_idf (A68_EXT, "qasindg", m, genie_asindg_mp);
  a68_idf (A68_EXT, "qasindg", m, genie_asindg_mp);
  a68_idf (A68_EXT, "qasinh", m, genie_asinh_mp);
  a68_idf (A68_EXT, "qasin", m, genie_asin_mp);
  a68_idf (A68_EXT, "qatandg", m, genie_atandg_mp);
  a68_idf (A68_EXT, "qatanh", m, genie_atanh_mp);
  a68_idf (A68_EXT, "qatan", m, genie_atan_mp);
  a68_idf (A68_EXT, "qcbrt", m, genie_curt_mp);
  a68_idf (A68_EXT, "qcosdg", m, genie_cosdg_mp);
  a68_idf (A68_EXT, "qcosh", m, genie_cosh_mp);
  a68_idf (A68_EXT, "qcos", m, genie_cos_mp);
  a68_idf (A68_EXT, "qcospi", m, genie_cospi_mp);
  a68_idf (A68_EXT, "qcotdg", m, genie_cotdg_mp);
  a68_idf (A68_EXT, "qcot", m, genie_cot_mp);
  a68_idf (A68_EXT, "qcotpi", m, genie_cotpi_mp);
  a68_idf (A68_EXT, "qcsc", m, genie_csc_mp);
  a68_idf (A68_EXT, "qcurt", m, genie_curt_mp);
  a68_idf (A68_EXT, "qerfc", m, genie_erfc_mp);
  a68_idf (A68_EXT, "qerf", m, genie_erf_mp);
  a68_idf (A68_EXT, "qexp", m, genie_exp_mp);
  a68_idf (A68_EXT, "qgamma", m, genie_gamma_mp);
  a68_idf (A68_EXT, "qinverfc", m, genie_inverfc_mp);
  a68_idf (A68_EXT, "qinverf", m, genie_inverf_mp);
  a68_idf (A68_EXT, "qlngamma", m, genie_lngamma_mp);
  a68_idf (A68_EXT, "qln", m, genie_ln_mp);
  a68_idf (A68_EXT, "qlog", m, genie_log_mp);
  a68_idf (A68_EXT, "qsec", m, genie_sec_mp);
  a68_idf (A68_EXT, "qsindg", m, genie_sindg_mp);
  a68_idf (A68_EXT, "qsinh", m, genie_sinh_mp);
  a68_idf (A68_EXT, "qsin", m, genie_sin_mp);
  a68_idf (A68_EXT, "qsinpi", m, genie_sinpi_mp);
  a68_idf (A68_EXT, "qsqrt", m, genie_sqrt_mp);
  a68_idf (A68_EXT, "qtandg", m, genie_tandg_mp);
  a68_idf (A68_EXT, "qtanh", m, genie_tanh_mp);
  a68_idf (A68_EXT, "qtan", m, genie_tan_mp);
  a68_idf (A68_EXT, "qtanpi", m, genie_tanpi_mp);
// RR.
  a68_idf (A68_STD, "longlongarccos", m, genie_acos_mp);
  a68_idf (A68_STD, "longlongarcsin", m, genie_asin_mp);
  a68_idf (A68_STD, "longlongarctan", m, genie_atan_mp);
  a68_idf (A68_STD, "longlongcos", m, genie_cos_mp);
  a68_idf (A68_STD, "longlongexp", m, genie_exp_mp);
  a68_idf (A68_STD, "longlongln", m, genie_ln_mp);
  a68_idf (A68_STD, "longlonglog", m, genie_log_mp);
  a68_idf (A68_STD, "longlongsin", m, genie_sin_mp);
  a68_idf (A68_STD, "longlongsqrt", m, genie_sqrt_mp);
  a68_idf (A68_STD, "longlongtan", m, genie_tan_mp);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "longlongarctan2dg", m, genie_atan2dg_mp);
  a68_idf (A68_EXT, "longlongarctan2", m, genie_atan2_mp);
  a68_idf (A68_EXT, "longlongbeta", m, genie_beta_mp);
  a68_idf (A68_EXT, "longlonggammaincf", m, genie_gamma_inc_f_mp);
  a68_idf (A68_EXT, "longlonggammaincgf", m, genie_gamma_inc_gf_mp);
  a68_idf (A68_EXT, "longlonggammainc", m, genie_gamma_inc_h_mp);
  a68_idf (A68_EXT, "longlonglnbeta", m, genie_lnbeta_mp);
  a68_idf (A68_EXT, "qarctan2dg", m, genie_atan2dg_mp);
  a68_idf (A68_EXT, "qatan2", m, genie_atan2_mp);
  a68_idf (A68_EXT, "qbeta", m, genie_beta_mp);
  a68_idf (A68_EXT, "qgammaincf", m, genie_gamma_inc_f_mp);
  a68_idf (A68_EXT, "qgammaincgf", m, genie_gamma_inc_gf_mp);
  a68_idf (A68_EXT, "qgammainc", m, genie_gamma_inc_h_mp);
  a68_idf (A68_EXT, "qlnbeta", m, genie_lnbeta_mp);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "longlongbetainc", m, genie_beta_inc_mp);
  a68_idf (A68_EXT, "qbetainc", m, genie_beta_inc_mp);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "longlonggammaincg", m, genie_gamma_inc_g_mp);
  a68_idf (A68_EXT, "qgammaincg", m, genie_gamma_inc_g_mp);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_mp);
  a68_op (A68_STD, "-", m, genie_sub_mp);
  a68_op (A68_STD, "*", m, genie_mul_mp);
  a68_op (A68_STD, "/", m, genie_div_mp);
  a68_op (A68_STD, "**", m, genie_pow_mp);
  a68_op (A68_STD, "UP", m, genie_pow_mp);
  a68_op (A68_STD, "^", m, genie_pow_mp);
//
  m = a68_proc (M_REF_LONG_LONG_REAL, M_REF_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_mp);
  a68_op (A68_STD, "-:=", m, genie_minusab_mp);
  a68_op (A68_STD, "*:=", m, genie_timesab_mp);
  a68_op (A68_STD, "/:=", m, genie_divab_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_mp);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_mp);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_mp);
  a68_op (A68_STD, "DIVAB", m, genie_divab_mp);
//
  m = a68_proc (M_BOOL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_mp);
  a68_op (A68_STD, "/=", m, genie_ne_mp);
  a68_op (A68_STD, "~=", m, genie_ne_mp);
  a68_op (A68_STD, "^=", m, genie_ne_mp);
  a68_op (A68_STD, "NE", m, genie_ne_mp);
  a68_op (A68_STD, "<", m, genie_lt_mp);
  a68_op (A68_STD, "LT", m, genie_lt_mp);
  a68_op (A68_STD, "<=", m, genie_le_mp);
  a68_op (A68_STD, "LE", m, genie_le_mp);
  a68_op (A68_STD, ">", m, genie_gt_mp);
  a68_op (A68_STD, "GT", m, genie_gt_mp);
  a68_op (A68_STD, ">=", m, genie_ge_mp);
  a68_op (A68_STD, "GE", m, genie_ge_mp);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_mp_int);
  a68_op (A68_STD, "UP", m, genie_pow_mp_int);
  a68_op (A68_STD, "^", m, genie_pow_mp_int);
//
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
// LONG LONG COMPLEX in software
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "RE", m, genie_re_mp_complex);
  a68_op (A68_STD, "IM", m, genie_im_mp_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_mp_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_mp_complex);
//
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_mp_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_mp_complex);
//
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_mp_complex);
  a68_op (A68_STD, "-", m, genie_sub_mp_complex);
  a68_op (A68_STD, "*", m, genie_mul_mp_complex);
  a68_op (A68_STD, "/", m, genie_div_mp_complex);
//
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_mp_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_mp_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_mp_complex_int);
//
  m = a68_proc (M_BOOL, M_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_mp_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_mp_complex);
  a68_op (A68_STD, "/=", m, genie_ne_mp_complex);
  a68_op (A68_STD, "~=", m, genie_ne_mp_complex);
  a68_op (A68_STD, "^=", m, genie_ne_mp_complex);
  a68_op (A68_STD, "NE", m, genie_ne_mp_complex);
//
  m = a68_proc (M_REF_LONG_LONG_COMPLEX, M_REF_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_mp_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_mp_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_mp_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_mp_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_mp_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_mp_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_mp_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_mp_complex);
//
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_idf (A68_EXT, "longlongcomplexacosh", m, genie_acosh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexacos", m, genie_acos_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexarccosh", m, genie_acosh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexarccos", m, genie_acos_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexarcsinh", m, genie_asinh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexarcsin", m, genie_asin_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexarctanh", m, genie_atanh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexarctan", m, genie_atan_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexasinh", m, genie_asinh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexasin", m, genie_asin_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexatanh", m, genie_atanh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexatan", m, genie_atan_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexcosh", m, genie_cosh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexcos", m, genie_cos_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexexp", m, genie_exp_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexln", m, genie_ln_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexsinh", m, genie_sinh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexsin", m, genie_sin_mp_complex);
  a68_idf (A68_EXT, "longlongcomplexsqrt", m, genie_sqrt_mp_complex);
  a68_idf (A68_EXT, "longlongcomplextanh", m, genie_tanh_mp_complex);
  a68_idf (A68_EXT, "longlongcomplextan", m, genie_tan_mp_complex);
  a68_idf (A68_EXT, "qcacosh", m, genie_acosh_mp_complex);
  a68_idf (A68_EXT, "qcacos", m, genie_acos_mp_complex);
  a68_idf (A68_EXT, "qcarccosh", m, genie_acosh_mp_complex);
  a68_idf (A68_EXT, "qcarccos", m, genie_acos_mp_complex);
  a68_idf (A68_EXT, "qcarcsinh", m, genie_asinh_mp_complex);
  a68_idf (A68_EXT, "qcarcsin", m, genie_asin_mp_complex);
  a68_idf (A68_EXT, "qcarctanh", m, genie_atanh_mp_complex);
  a68_idf (A68_EXT, "qcarctan", m, genie_atan_mp_complex);
  a68_idf (A68_EXT, "qcasinh", m, genie_asinh_mp_complex);
  a68_idf (A68_EXT, "qcasin", m, genie_asin_mp_complex);
  a68_idf (A68_EXT, "qcatanh", m, genie_atanh_mp_complex);
  a68_idf (A68_EXT, "qcatan", m, genie_atan_mp_complex);
  a68_idf (A68_EXT, "qccosh", m, genie_cosh_mp_complex);
  a68_idf (A68_EXT, "qccos", m, genie_cos_mp_complex);
  a68_idf (A68_EXT, "qcexp", m, genie_exp_mp_complex);
  a68_idf (A68_EXT, "qcln", m, genie_ln_mp_complex);
  a68_idf (A68_EXT, "qcsinh", m, genie_sinh_mp_complex);
  a68_idf (A68_EXT, "qcsin", m, genie_sin_mp_complex);
  a68_idf (A68_EXT, "qcsqrt", m, genie_sqrt_mp_complex);
  a68_idf (A68_EXT, "qctanh", m, genie_tanh_mp_complex);
  a68_idf (A68_EXT, "qctan", m, genie_tan_mp_complex);

// BYTES ops.
  m = a68_proc (M_BYTES, M_STRING, NO_MOID);
  a68_idf (A68_STD, "bytespack", m, genie_bytespack);
//
  m = a68_proc (M_CHAR, M_INT, M_BYTES, NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_bytes);
//
  m = a68_proc (M_BYTES, M_BYTES, M_BYTES, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_bytes);
//
  m = a68_proc (M_REF_BYTES, M_REF_BYTES, M_BYTES, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_bytes);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_bytes);
//
  m = a68_proc (M_REF_BYTES, M_BYTES, M_REF_BYTES, NO_MOID);
  a68_op (A68_STD, "+=:", m, genie_plusto_bytes);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_bytes);
//
  m = a68_proc (M_BOOL, M_BYTES, M_BYTES, NO_MOID);
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
// LONG BYTES ops.
  m = a68_proc (M_LONG_BYTES, M_BYTES, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_leng_bytes);
//
  m = a68_proc (M_BYTES, M_LONG_BYTES, NO_MOID);
  a68_idf (A68_STD, "SHORTEN", m, genie_shorten_bytes);
//
  m = a68_proc (M_LONG_BYTES, M_STRING, NO_MOID);
  a68_idf (A68_STD, "longbytespack", m, genie_long_bytespack);
//
  m = a68_proc (M_CHAR, M_INT, M_LONG_BYTES, NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_long_bytes);
//
  m = a68_proc (M_LONG_BYTES, M_LONG_BYTES, M_LONG_BYTES, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_long_bytes);
//
  m = a68_proc (M_REF_LONG_BYTES, M_REF_LONG_BYTES, M_LONG_BYTES, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_long_bytes);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_long_bytes);
//
  m = a68_proc (M_REF_LONG_BYTES, M_LONG_BYTES, M_REF_LONG_BYTES, NO_MOID);
  a68_op (A68_STD, "+=:", m, genie_plusto_long_bytes);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_long_bytes);
//
  m = a68_proc (M_BOOL, M_LONG_BYTES, M_LONG_BYTES, NO_MOID);
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
// STRING ops.
  m = a68_proc (M_BOOL, M_STRING, M_STRING, NO_MOID);
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
//
  m = a68_proc (M_STRING, M_CHAR, M_CHAR, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_char);
//
  m = a68_proc (M_STRING, M_STRING, M_STRING, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_string);
//
  m = a68_proc (M_REF_STRING, M_REF_STRING, M_STRING, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_string);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_string);
//
  m = a68_proc (M_REF_STRING, M_REF_STRING, M_INT, NO_MOID);
  a68_op (A68_STD, "*:=", m, genie_timesab_string);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_string);
//
  m = a68_proc (M_REF_STRING, M_STRING, M_REF_STRING, NO_MOID);
  a68_op (A68_STD, "+=:", m, genie_plusto_string);
  a68_op (A68_STD, "PLUSTO", m, genie_plusto_string);
//
  m = a68_proc (M_STRING, M_STRING, M_INT, NO_MOID);
  a68_op (A68_STD, "*", m, genie_times_string_int);
//
  m = a68_proc (M_STRING, M_INT, M_STRING, NO_MOID);
  a68_op (A68_STD, "*", m, genie_times_int_string);
//
  m = a68_proc (M_STRING, M_INT, M_CHAR, NO_MOID);
  a68_op (A68_STD, "*", m, genie_times_int_char);
//
  m = a68_proc (M_STRING, M_CHAR, M_INT, NO_MOID);
  a68_op (A68_STD, "*", m, genie_times_char_int);
//
  m = a68_proc (M_CHAR, M_INT, M_ROW_CHAR, NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_string);
//
  m = a68_proc (M_STRING, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "realpath", m, genie_realpath);
// SEMA ops.
#if defined (BUILD_PARALLEL_CLAUSE)
  m = a68_proc (M_SEMA, M_INT, NO_MOID);
  a68_op (A68_STD, "LEVEL", m, genie_level_sema_int);
//
  m = a68_proc (M_INT, M_SEMA, NO_MOID);
  a68_op (A68_STD, "LEVEL", m, genie_level_int_sema);
//
  m = a68_proc (M_VOID, M_SEMA, NO_MOID);
  a68_op (A68_STD, "UP", m, genie_up_sema);
  a68_op (A68_STD, "DOWN", m, genie_down_sema);
#else
  m = a68_proc (M_SEMA, M_INT, NO_MOID);
  a68_op (A68_STD, "LEVEL", m, genie_unimplemented);
//
  m = a68_proc (M_INT, M_SEMA, NO_MOID);
  a68_op (A68_STD, "LEVEL", m, genie_unimplemented);
//
  m = a68_proc (M_VOID, M_SEMA, NO_MOID);
  a68_op (A68_STD, "UP", m, genie_unimplemented);
  a68_op (A68_STD, "DOWN", m, genie_unimplemented);
#endif
// ROWS ops.
  m = a68_proc (M_INT, M_ROWS, NO_MOID);
  a68_op (A68_EXT, "ELEMS", m, genie_monad_elems);
  a68_op (A68_STD, "LWB", m, genie_monad_lwb);
  a68_op (A68_STD, "UPB", m, genie_monad_upb);
//
  m = a68_proc (M_INT, M_INT, M_ROWS, NO_MOID);
  a68_op (A68_EXT, "ELEMS", m, genie_dyad_elems);
  a68_op (A68_STD, "LWB", m, genie_dyad_lwb);
  a68_op (A68_STD, "UPB", m, genie_dyad_upb);
//
  m = a68_proc (M_ROW_STRING, M_ROW_STRING, NO_MOID);
  a68_op (A68_EXT, "SORT", m, genie_sort_row_string);
// Some "terminators" to handle the mapping of very short or very long modes.
// This allows you to write SHORT REAL z = SHORTEN pi while everything is
// silently mapped onto REAL.
  m = a68_proc (M_LONG_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_idle);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_idle);
//
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_idle);
//
  m = a68_proc (M_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
//
  m = a68_proc (M_REAL, M_REAL, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
//
  m = a68_proc (M_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
//
  m = a68_proc (M_BITS, M_BITS, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_idle);
// SOUND/RIFF procs.
  m = a68_proc (M_SOUND, M_INT, M_INT, M_INT, M_INT, NO_MOID);
  a68_idf (A68_EXT, "newsound", m, genie_new_sound);
  m = a68_proc (M_INT, M_SOUND, M_INT, M_INT, NO_MOID);
  a68_idf (A68_EXT, "getsound", m, genie_get_sound);
  m = a68_proc (M_VOID, M_SOUND, M_INT, M_INT, M_INT, NO_MOID);
  a68_idf (A68_EXT, "setsound", m, genie_set_sound);
  m = a68_proc (M_INT, M_SOUND, NO_MOID);
  a68_op (A68_EXT, "RESOLUTION", m, genie_sound_resolution);
  a68_op (A68_EXT, "CHANNELS", m, genie_sound_channels);
  a68_op (A68_EXT, "RATE", m, genie_sound_rate);
  a68_op (A68_EXT, "SAMPLES", m, genie_sound_samples);
}

//! @brief Set up standenv - transput.

void stand_mp_level_2 (void)
{
#if (A68_LEVEL <= 2)
  MOID_T *m;
  a68_idf (A68_STD, "dpi", M_LONG_REAL, genie_pi_mp);
  a68_idf (A68_STD, "longpi", M_LONG_REAL, genie_pi_mp);
  a68_idf (A68_STD, "longmaxbits", M_LONG_BITS, genie_long_max_bits);
  a68_idf (A68_STD, "longmaxint", M_LONG_INT, genie_long_max_int);
  a68_idf (A68_STD, "longsmallreal", M_LONG_REAL, genie_long_small_real);
  a68_idf (A68_STD, "longmaxreal", M_LONG_REAL, genie_long_max_real);
  a68_idf (A68_STD, "longminreal", M_LONG_REAL, genie_long_min_real);
  a68_idf (A68_STD, "longinfinity", M_LONG_REAL, genie_infinity_mp);
  a68_idf (A68_STD, "longminusinfinity", M_LONG_REAL, genie_minus_infinity_mp);
  a68_idf (A68_STD, "longinf", M_LONG_REAL, genie_infinity_mp);
  a68_idf (A68_STD, "longmininf", M_LONG_REAL, genie_minus_infinity_mp);
// LONG INT in software
  m = a68_proc (M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_mp);
//
  m = a68_proc (M_LONG_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_int_to_mp);
//
  m = a68_proc (M_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_mp_to_int);
  a68_op (A68_STD, "SIGN", m, genie_sign_mp);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_mp_to_long_mp);
//
  m = a68_proc (M_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_mp);
//
  m = a68_proc (M_LONG_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_mp_to_long_mp);
//
  m = a68_proc (M_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_mp);
//
  m = a68_proc (M_BOOL, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "ODD", m, genie_odd_mp);
//
  m = a68_proc (M_LONG_INT, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "ENTIER", m, genie_entier_mp);
  a68_op (A68_STD, "ROUND", m, genie_round_mp);
//
  m = a68_proc (M_LONG_INT, M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_mp_int);
  a68_op (A68_STD, "-", m, genie_sub_mp_int);
  a68_op (A68_STD, "*", m, genie_mul_mp_int);
  a68_op (A68_STD, "OVER", m, genie_over_mp);
  a68_op (A68_STD, "%", m, genie_over_mp);
  a68_op (A68_STD, "MOD", m, genie_mod_mp);
  a68_op (A68_STD, "%*", m, genie_mod_mp);
//
  m = a68_proc (M_REF_LONG_INT, M_REF_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_mp_int);
  a68_op (A68_STD, "-:=", m, genie_minusab_mp_int);
  a68_op (A68_STD, "*:=", m, genie_timesab_mp_int);
  a68_op (A68_STD, "%:=", m, genie_overab_mp);
  a68_op (A68_STD, "%*:=", m, genie_modab_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_mp_int);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_mp_int);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_mp_int);
  a68_op (A68_STD, "OVERAB", m, genie_overab_mp);
  a68_op (A68_STD, "MODAB", m, genie_modab_mp);
//
  m = a68_proc (M_BOOL, M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_mp);
  a68_op (A68_STD, "/=", m, genie_ne_mp);
  a68_op (A68_STD, "~=", m, genie_ne_mp);
  a68_op (A68_STD, "^=", m, genie_ne_mp);
  a68_op (A68_STD, "NE", m, genie_ne_mp);
  a68_op (A68_STD, "<", m, genie_lt_mp);
  a68_op (A68_STD, "LT", m, genie_lt_mp);
  a68_op (A68_STD, "<=", m, genie_le_mp);
  a68_op (A68_STD, "LE", m, genie_le_mp);
  a68_op (A68_STD, ">", m, genie_gt_mp);
  a68_op (A68_STD, "GT", m, genie_gt_mp);
  a68_op (A68_STD, ">=", m, genie_ge_mp);
  a68_op (A68_STD, "GE", m, genie_ge_mp);
//
  m = a68_proc (M_LONG_REAL, M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "/", m, genie_div_mp);
//
  m = a68_proc (M_LONG_INT, M_LONG_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_mp_int_int);
  a68_op (A68_STD, "^", m, genie_pow_mp_int_int);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
//
  m = a68_proc (M_LONG_REAL, M_REAL, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_real_to_mp);
//
  m = a68_proc (M_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_mp_to_real);
// LONG REAL in software
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_mp);
  a68_op (A68_STD, "ABS", m, genie_abs_mp);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "dacosdg", m, genie_acosdg_mp);
  a68_idf (A68_EXT, "dacosh", m, genie_acosh_mp);
  a68_idf (A68_EXT, "dacos", m, genie_acos_mp);
  a68_idf (A68_EXT, "dacotdg", m, genie_acotdg_mp);
  a68_idf (A68_EXT, "dacot", m, genie_acot_mp);
  a68_idf (A68_EXT, "dacsc", m, genie_acsc_mp);
  a68_idf (A68_EXT, "dasec", m, genie_asec_mp);
  a68_idf (A68_EXT, "dasindg", m, genie_asindg_mp);
  a68_idf (A68_EXT, "dasinh", m, genie_asinh_mp);
  a68_idf (A68_EXT, "dasin", m, genie_asin_mp);
  a68_idf (A68_EXT, "datandg", m, genie_atandg_mp);
  a68_idf (A68_EXT, "datanh", m, genie_atanh_mp);
  a68_idf (A68_EXT, "datan", m, genie_atan_mp);
  a68_idf (A68_EXT, "dcbrt", m, genie_curt_mp);
  a68_idf (A68_EXT, "dcosdg", m, genie_cosdg_mp);
  a68_idf (A68_EXT, "dcosh", m, genie_cosh_mp);
  a68_idf (A68_EXT, "dcos", m, genie_cos_mp);
  a68_idf (A68_EXT, "dcospi", m, genie_cospi_mp);
  a68_idf (A68_EXT, "dcotdg", m, genie_cotdg_mp);
  a68_idf (A68_EXT, "dcot", m, genie_cot_mp);
  a68_idf (A68_EXT, "dcotpi", m, genie_cotpi_mp);
  a68_idf (A68_EXT, "dcsc", m, genie_csc_mp);
  a68_idf (A68_EXT, "dcurt", m, genie_curt_mp);
  a68_idf (A68_EXT, "derf", m, genie_erf_mp);
  a68_idf (A68_EXT, "derfc", m, genie_erfc_mp);
  a68_idf (A68_EXT, "dinverf", m, genie_inverf_mp);
  a68_idf (A68_EXT, "dinverfc", m, genie_inverfc_mp);
  a68_idf (A68_EXT, "dgamma", m, genie_gamma_mp);
  a68_idf (A68_EXT, "dlngamma", m, genie_lngamma_mp);
  a68_idf (A68_EXT, "dexp", m, genie_exp_mp);
  a68_idf (A68_EXT, "dln", m, genie_ln_mp);
  a68_idf (A68_EXT, "dlog", m, genie_log_mp);
  a68_idf (A68_EXT, "dsec", m, genie_sec_mp);
  a68_idf (A68_EXT, "dsindg", m, genie_sindg_mp);
  a68_idf (A68_EXT, "dsinh", m, genie_sinh_mp);
  a68_idf (A68_EXT, "dsin", m, genie_sin_mp);
  a68_idf (A68_EXT, "dsinpi", m, genie_sinpi_mp);
  a68_idf (A68_EXT, "dsqrt", m, genie_sqrt_mp);
  a68_idf (A68_EXT, "dtandg", m, genie_tandg_mp);
  a68_idf (A68_EXT, "dtanh", m, genie_tanh_mp);
  a68_idf (A68_EXT, "dtan", m, genie_tan_mp);
  a68_idf (A68_EXT, "dtanpi", m, genie_tan_mp);
  a68_idf (A68_EXT, "longarccosdg", m, genie_acosdg_mp);
  a68_idf (A68_EXT, "longarccosh", m, genie_acosh_mp);
  a68_idf (A68_EXT, "longarccotdg", m, genie_acosdg_mp);
  a68_idf (A68_EXT, "longarccot", m, genie_acot_mp);
  a68_idf (A68_EXT, "longarccsc", m, genie_acsc_mp);
  a68_idf (A68_EXT, "longarcsec", m, genie_asec_mp);
  a68_idf (A68_EXT, "longarcsindg", m, genie_asindg_mp);
  a68_idf (A68_EXT, "longarcsinh", m, genie_asinh_mp);
  a68_idf (A68_EXT, "longarctandg", m, genie_atandg_mp);
  a68_idf (A68_EXT, "longarctanh", m, genie_atanh_mp);
  a68_idf (A68_EXT, "longcbrt", m, genie_curt_mp);
  a68_idf (A68_EXT, "longcosdg", m, genie_cosdg_mp);
  a68_idf (A68_EXT, "longcosh", m, genie_cosh_mp);
  a68_idf (A68_EXT, "longcospi", m, genie_cospi_mp);
  a68_idf (A68_EXT, "longcotdg", m, genie_cotdg_mp);
  a68_idf (A68_EXT, "longcot", m, genie_cot_mp);
  a68_idf (A68_EXT, "longcotpi", m, genie_cotpi_mp);
  a68_idf (A68_EXT, "longcsc", m, genie_csc_mp);
  a68_idf (A68_EXT, "longcurt", m, genie_curt_mp);
  a68_idf (A68_EXT, "longerf", m, genie_erf_mp);
  a68_idf (A68_EXT, "longerfc", m, genie_erfc_mp);
  a68_idf (A68_EXT, "longinverfc", m, genie_inverfc_mp);
  a68_idf (A68_EXT, "longinverf", m, genie_inverf_mp);
  a68_idf (A68_EXT, "longgamma", m, genie_gamma_mp);
  a68_idf (A68_EXT, "longlngamma", m, genie_lngamma_mp);
  a68_idf (A68_EXT, "longsec", m, genie_sec_mp);
  a68_idf (A68_EXT, "longsindg", m, genie_sindg_mp);
  a68_idf (A68_EXT, "longsinh", m, genie_sinh_mp);
  a68_idf (A68_EXT, "longsinpi", m, genie_sinpi_mp);
  a68_idf (A68_EXT, "longtandg", m, genie_tandg_mp);
  a68_idf (A68_EXT, "longtanh", m, genie_tanh_mp);
  a68_idf (A68_EXT, "longtanpi", m, genie_tanpi_mp);
// RR.
  a68_idf (A68_STD, "longarccos", m, genie_acos_mp);
  a68_idf (A68_STD, "longarcsin", m, genie_asin_mp);
  a68_idf (A68_STD, "longarctan", m, genie_atan_mp);
  a68_idf (A68_STD, "longcos", m, genie_cos_mp);
  a68_idf (A68_STD, "longexp", m, genie_exp_mp);
  a68_idf (A68_STD, "longln", m, genie_ln_mp);
  a68_idf (A68_STD, "longlog", m, genie_log_mp);
  a68_idf (A68_STD, "longsin", m, genie_sin_mp);
  a68_idf (A68_STD, "longsqrt", m, genie_sqrt_mp);
  a68_idf (A68_STD, "longtan", m, genie_tan_mp);
//
  m = a68_proc (M_LONG_REAL, NO_MOID);
  a68_idf (A68_STD, "longnextrandom", m, genie_long_next_random);
  a68_idf (A68_STD, "longrandom", m, genie_long_next_random);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "dbeta", m, genie_beta_mp);
  a68_idf (A68_EXT, "dgammaincgf", m, genie_gamma_inc_gf_mp);
  a68_idf (A68_EXT, "dgammaincf", m, genie_gamma_inc_f_mp);
  a68_idf (A68_EXT, "dgammainc", m, genie_gamma_inc_h_mp);
  a68_idf (A68_EXT, "dlnbeta", m, genie_lnbeta_mp);
  a68_idf (A68_EXT, "longbeta", m, genie_beta_mp);
  a68_idf (A68_EXT, "longgammaincgf", m, genie_gamma_inc_gf_mp);
  a68_idf (A68_EXT, "longgammaincf", m, genie_gamma_inc_f_mp);
  a68_idf (A68_EXT, "longgammainc", m, genie_gamma_inc_h_mp);
  a68_idf (A68_EXT, "longlnbeta", m, genie_lnbeta_mp);
  a68_idf (A68_STD, "darctan2dg", m, genie_atan2dg_mp);
  a68_idf (A68_STD, "darctan2", m, genie_atan2_mp);
  a68_idf (A68_STD, "longarctan2dg", m, genie_atan2dg_mp);
  a68_idf (A68_STD, "longarctan2", m, genie_atan2_mp);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_STD, "longbetainc", m, genie_beta_inc_mp);
  a68_idf (A68_STD, "dbetainc", m, genie_beta_inc_mp);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "longgammaincg", m, genie_gamma_inc_g_mp);
  a68_idf (A68_EXT, "dgammaincg", m, genie_gamma_inc_g_mp);
//
  m = a68_proc (M_INT, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "SIGN", m, genie_sign_mp);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_mp);
  a68_op (A68_STD, "-", m, genie_sub_mp);
  a68_op (A68_STD, "*", m, genie_mul_mp);
  a68_op (A68_STD, "/", m, genie_div_mp);
  a68_op (A68_STD, "**", m, genie_pow_mp);
  a68_op (A68_STD, "UP", m, genie_pow_mp);
  a68_op (A68_STD, "^", m, genie_pow_mp);
//
  m = a68_proc (M_REF_LONG_REAL, M_REF_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_mp);
  a68_op (A68_STD, "-:=", m, genie_minusab_mp);
  a68_op (A68_STD, "*:=", m, genie_timesab_mp);
  a68_op (A68_STD, "/:=", m, genie_divab_mp);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_mp);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_mp);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_mp);
  a68_op (A68_STD, "DIVAB", m, genie_divab_mp);
//
  m = a68_proc (M_BOOL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_mp);
  a68_op (A68_STD, "/=", m, genie_ne_mp);
  a68_op (A68_STD, "~=", m, genie_ne_mp);
  a68_op (A68_STD, "^=", m, genie_ne_mp);
  a68_op (A68_STD, "NE", m, genie_ne_mp);
  a68_op (A68_STD, "<", m, genie_lt_mp);
  a68_op (A68_STD, "LT", m, genie_lt_mp);
  a68_op (A68_STD, "<=", m, genie_le_mp);
  a68_op (A68_STD, "LE", m, genie_le_mp);
  a68_op (A68_STD, ">", m, genie_gt_mp);
  a68_op (A68_STD, "GT", m, genie_gt_mp);
  a68_op (A68_STD, ">=", m, genie_ge_mp);
  a68_op (A68_STD, "GE", m, genie_ge_mp);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_mp_int);
  a68_op (A68_STD, "UP", m, genie_pow_mp_int);
  a68_op (A68_STD, "^", m, genie_pow_mp_int);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "I", m, genie_idle);
  a68_op (A68_STD, "+*", m, genie_idle);
// LONG COMPLEX in software
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_mp_complex_to_long_mp_complex);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_complex_to_mp_complex);
//
  m = a68_proc (M_LONG_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_complex_to_mp_complex);
//
  m = a68_proc (M_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_mp_complex_to_complex);
//
  m = a68_proc (M_LONG_REAL, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "RE", m, genie_re_mp_complex);
  a68_op (A68_STD, "IM", m, genie_im_mp_complex);
  a68_op (A68_STD, "ARG", m, genie_arg_mp_complex);
  a68_op (A68_STD, "ABS", m, genie_abs_mp_complex);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_mp_complex);
  a68_op (A68_STD, "CONJ", m, genie_conj_mp_complex);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_mp_complex);
  a68_op (A68_STD, "-", m, genie_sub_mp_complex);
  a68_op (A68_STD, "*", m, genie_mul_mp_complex);
  a68_op (A68_STD, "/", m, genie_div_mp_complex);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_COMPLEX, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_mp_complex_int);
  a68_op (A68_STD, "UP", m, genie_pow_mp_complex_int);
  a68_op (A68_STD, "^", m, genie_pow_mp_complex_int);
//
  m = a68_proc (M_BOOL, M_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_mp_complex);
  a68_op (A68_STD, "EQ", m, genie_eq_mp_complex);
  a68_op (A68_STD, "/=", m, genie_ne_mp_complex);
  a68_op (A68_STD, "~=", m, genie_ne_mp_complex);
  a68_op (A68_STD, "^=", m, genie_ne_mp_complex);
  a68_op (A68_STD, "NE", m, genie_ne_mp_complex);
//
  m = a68_proc (M_REF_LONG_COMPLEX, M_REF_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_mp_complex);
  a68_op (A68_STD, "-:=", m, genie_minusab_mp_complex);
  a68_op (A68_STD, "*:=", m, genie_timesab_mp_complex);
  a68_op (A68_STD, "/:=", m, genie_divab_mp_complex);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_mp_complex);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_mp_complex);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_mp_complex);
  a68_op (A68_STD, "DIVAB", m, genie_divab_mp_complex);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_idf (A68_EXT, "dcacos", m, genie_acos_mp_complex);
  a68_idf (A68_EXT, "dcasin", m, genie_asin_mp_complex);
  a68_idf (A68_EXT, "dcatan", m, genie_atan_mp_complex);
  a68_idf (A68_EXT, "dccos", m, genie_cos_mp_complex);
  a68_idf (A68_EXT, "dcexp", m, genie_exp_mp_complex);
  a68_idf (A68_EXT, "dcln", m, genie_ln_mp_complex);
  a68_idf (A68_EXT, "dcsin", m, genie_sin_mp_complex);
  a68_idf (A68_EXT, "dcsqrt", m, genie_sqrt_mp_complex);
  a68_idf (A68_EXT, "dctan", m, genie_tan_mp_complex);
  a68_idf (A68_EXT, "longcomplexarccos", m, genie_acos_mp_complex);
  a68_idf (A68_EXT, "longcomplexarcsin", m, genie_asin_mp_complex);
  a68_idf (A68_EXT, "longcomplexarctan", m, genie_atan_mp_complex);
  a68_idf (A68_EXT, "longcomplexcos", m, genie_cos_mp_complex);
  a68_idf (A68_EXT, "longcomplexexp", m, genie_exp_mp_complex);
  a68_idf (A68_EXT, "longcomplexln", m, genie_ln_mp_complex);
  a68_idf (A68_EXT, "longcomplexsin", m, genie_sin_mp_complex);
  a68_idf (A68_EXT, "longcomplexsqrt", m, genie_sqrt_mp_complex);
  a68_idf (A68_EXT, "longcomplextan", m, genie_tan_mp_complex);
// LONG BITS in software
  m = a68_proc (M_LONG_BITS, M_ROW_BOOL, NO_MOID);
  a68_idf (A68_STD, "longbitspack", m, genie_long_bits_pack);
//
  m = a68_proc (M_LONG_INT, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_idle);
//
  m = a68_proc (M_LONG_BITS, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "BIN", m, genie_bin_mp);
//
  m = a68_proc (M_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_mp_to_bits);
//
  m = a68_proc (M_LONG_BITS, M_BITS, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_unt_to_mp);
//
  m = a68_proc (M_LONG_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_mp);
  a68_op (A68_STD, "~", m, genie_not_mp);
//
  m = a68_proc (M_BOOL, M_LONG_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_mp);
  a68_op (A68_STD, "/=", m, genie_ne_mp);
  a68_op (A68_STD, "~=", m, genie_ne_mp);
  a68_op (A68_STD, "^=", m, genie_ne_mp);
  a68_op (A68_STD, "NE", m, genie_ne_mp);
  a68_op (A68_STD, "<=", m, genie_le_long_bits);
  a68_op (A68_STD, "LE", m, genie_le_long_bits);
  a68_op (A68_STD, ">=", m, genie_ge_long_bits);
  a68_op (A68_STD, "GE", m, genie_ge_long_bits);
//
  m = a68_proc (M_LONG_BITS, M_LONG_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "AND", m, genie_and_mp);
  a68_op (A68_STD, "&", m, genie_and_mp);
  a68_op (A68_STD, "OR", m, genie_or_mp);
  a68_op (A68_EXT, "XOR", m, genie_xor_mp);
//
  m = a68_proc (M_LONG_BITS, M_LONG_BITS, M_INT, NO_MOID);
  a68_op (A68_STD, "SHL", m, genie_shl_mp);
  a68_op (A68_STD, "UP", m, genie_shl_mp);
  a68_op (A68_STD, "SHR", m, genie_shr_mp);
  a68_op (A68_STD, "DOWN", m, genie_shr_mp);
//
  m = a68_proc (M_BOOL, M_INT, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_long_bits);
//
  m = a68_proc (M_LONG_BITS, M_INT, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "SET", m, genie_set_long_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_long_bits);
#endif
}

void stand_mp_level_3 (void)
{
#if (A68_LEVEL >= 3)
  MOID_T *m;
  a68_idf (A68_STD, "dpi", M_LONG_REAL, genie_pi_double);
  a68_idf (A68_STD, "longpi", M_LONG_REAL, genie_pi_double);
  a68_idf (A68_STD, "longmaxbits", M_LONG_BITS, genie_double_max_bits);
  a68_idf (A68_STD, "longmaxint", M_LONG_INT, genie_double_max_int);
  a68_idf (A68_STD, "longsmallreal", M_LONG_REAL, genie_double_small_real);
  a68_idf (A68_STD, "longmaxreal", M_LONG_REAL, genie_double_max_real);
  a68_idf (A68_STD, "longminreal", M_LONG_REAL, genie_double_min_real);
  a68_idf (A68_STD, "longinfinity", M_LONG_REAL, genie_infinity_double);
  a68_idf (A68_STD, "longminusinfinity", M_LONG_REAL, genie_minus_infinity_double);
  a68_idf (A68_STD, "longinf", M_LONG_REAL, genie_infinity_double);
  a68_idf (A68_STD, "longmininf", M_LONG_REAL, genie_minus_infinity_double);
// LONG INT as 128 bit
  m = a68_proc (M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_double_int);
  a68_op (A68_STD, "ABS", m, genie_abs_double_int);
//
  m = a68_proc (M_LONG_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_int_to_double_int);
//
  m = a68_proc (M_LONG_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_double_int_to_mp);
//
  m = a68_proc (M_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_int_to_int);
  a68_op (A68_STD, "SIGN", m, genie_sign_double_int);
//
  m = a68_proc (M_BOOL, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "ODD", m, genie_odd_double_int);
//
  m = a68_proc (M_LONG_INT, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "ENTIER", m, genie_entier_double);
  a68_op (A68_STD, "ROUND", m, genie_round_double);
//
  m = a68_proc (M_LONG_INT, M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_double_int);
  a68_op (A68_STD, "-", m, genie_sub_double_int);
  a68_op (A68_STD, "*", m, genie_mul_double_int);
  a68_op (A68_STD, "OVER", m, genie_over_double_int);
  a68_op (A68_STD, "%", m, genie_over_double_int);
  a68_op (A68_STD, "MOD", m, genie_mod_double_int);
  a68_op (A68_STD, "%*", m, genie_mod_double_int);
//
  m = a68_proc (M_LONG_INT, M_LONG_INT, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_double_int_int);
  a68_op (A68_STD, "^", m, genie_pow_double_int_int);
//
  m = a68_proc (M_REF_LONG_INT, M_REF_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_double_int);
  a68_op (A68_STD, "-:=", m, genie_minusab_double_int);
  a68_op (A68_STD, "*:=", m, genie_timesab_double_int);
  a68_op (A68_STD, "%:=", m, genie_overab_double_int);
  a68_op (A68_STD, "%*:=", m, genie_modab_double_int);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_double_int);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_double_int);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_double_int);
  a68_op (A68_STD, "OVERAB", m, genie_overab_double_int);
  a68_op (A68_STD, "MODAB", m, genie_modab_double_int);
//
  m = a68_proc (M_LONG_REAL, M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "/", m, genie_div_double_int);
//
  m = a68_proc (M_BOOL, M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_double_int);
  a68_op (A68_STD, "EQ", m, genie_eq_double_int);
  a68_op (A68_STD, "/=", m, genie_ne_double_int);
  a68_op (A68_STD, "~=", m, genie_ne_double_int);
  a68_op (A68_STD, "^=", m, genie_ne_double_int);
  a68_op (A68_STD, "NE", m, genie_ne_double_int);
  a68_op (A68_STD, "<", m, genie_lt_double_int);
  a68_op (A68_STD, "LT", m, genie_lt_double_int);
  a68_op (A68_STD, "<=", m, genie_le_double_int);
  a68_op (A68_STD, "LE", m, genie_le_double_int);
  a68_op (A68_STD, ">", m, genie_gt_double_int);
  a68_op (A68_STD, "GT", m, genie_gt_double_int);
  a68_op (A68_STD, ">=", m, genie_ge_double_int);
  a68_op (A68_STD, "GE", m, genie_ge_double_int);
// LONG REAL as 128 bit
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_double);
  a68_op (A68_STD, "ABS", m, genie_abs_double);
//
  m = a68_proc (M_INT, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "SIGN", m, genie_sign_double);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_double);
  a68_op (A68_STD, "-", m, genie_sub_double);
  a68_op (A68_STD, "*", m, genie_mul_double);
  a68_op (A68_STD, "/", m, genie_over_double);
  a68_op (A68_STD, "**", m, genie_pow_double);
  a68_op (A68_STD, "UP", m, genie_pow_double);
  a68_op (A68_STD, "^", m, genie_pow_double);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_double_int);
  a68_op (A68_STD, "UP", m, genie_pow_double_int);
  a68_op (A68_STD, "^", m, genie_pow_double_int);
//
  m = a68_proc (M_LONG_REAL, M_REAL, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_real_to_double);
//
  m = a68_proc (M_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_double_to_real);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_double_to_mp);
//
  m = a68_proc (M_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_mp_to_double);
//
  m = a68_proc (M_LONG_INT, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_mp_to_double_int);
//
  m = a68_proc (M_REF_LONG_REAL, M_REF_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_double);
  a68_op (A68_STD, "-:=", m, genie_minusab_double);
  a68_op (A68_STD, "*:=", m, genie_timesab_double);
  a68_op (A68_STD, "/:=", m, genie_divab_double);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_double);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_double);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_double);
  a68_op (A68_STD, "DIVAB", m, genie_divab_double);
//
  m = a68_proc (M_BOOL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_double);
  a68_op (A68_STD, "EQ", m, genie_eq_double);
  a68_op (A68_STD, "/=", m, genie_ne_double);
  a68_op (A68_STD, "~=", m, genie_ne_double);
  a68_op (A68_STD, "^=", m, genie_ne_double);
  a68_op (A68_STD, "NE", m, genie_ne_double);
  a68_op (A68_STD, "<", m, genie_lt_double);
  a68_op (A68_STD, "LT", m, genie_lt_double);
  a68_op (A68_STD, "<=", m, genie_le_double);
  a68_op (A68_STD, "LE", m, genie_le_double);
  a68_op (A68_STD, ">", m, genie_gt_double);
  a68_op (A68_STD, "GT", m, genie_gt_double);
  a68_op (A68_STD, ">=", m, genie_ge_double);
  a68_op (A68_STD, "GE", m, genie_ge_double);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "dacosdg", m, genie_acosdg_double);
  a68_idf (A68_EXT, "dacosh", m, genie_acosh_double);
  a68_idf (A68_EXT, "dacos", m, genie_acos_double);
  a68_idf (A68_EXT, "dacotdg", m, genie_acotdg_double);
  a68_idf (A68_EXT, "dacot", m, genie_acot_double);
  a68_idf (A68_EXT, "dacsc", m, genie_acsc_double);
  a68_idf (A68_EXT, "dasec", m, genie_asec_double);
  a68_idf (A68_EXT, "dasindg", m, genie_asindg_double);
  a68_idf (A68_EXT, "dasinh", m, genie_asinh_double);
  a68_idf (A68_EXT, "dasin", m, genie_asin_double);
  a68_idf (A68_EXT, "datandg", m, genie_atandg_double);
  a68_idf (A68_EXT, "datanh", m, genie_atanh_double);
  a68_idf (A68_EXT, "datan", m, genie_atan_double);
  a68_idf (A68_EXT, "dcbrt", m, genie_curt_double);
  a68_idf (A68_EXT, "dcosdg", m, genie_cosdg_double);
  a68_idf (A68_EXT, "dcosh", m, genie_cosh_double);
  a68_idf (A68_EXT, "dcos", m, genie_cos_double);
  a68_idf (A68_EXT, "dcospi", m, genie_cospi_double);
  a68_idf (A68_EXT, "dcotdg", m, genie_cotdg_double);
  a68_idf (A68_EXT, "dcot", m, genie_cot_double);
  a68_idf (A68_EXT, "dcotpi", m, genie_cotpi_double);
  a68_idf (A68_EXT, "dcsc", m, genie_csc_double);
  a68_idf (A68_EXT, "dcurt", m, genie_curt_double);
  a68_idf (A68_EXT, "derfc", m, genie_erfc_double);
  a68_idf (A68_EXT, "derf", m, genie_erf_double);
  a68_idf (A68_EXT, "dexp", m, genie_exp_double);
  a68_idf (A68_EXT, "dinverfc", m, genie_inverfc_double);
  a68_idf (A68_EXT, "dinverf", m, genie_inverf_double);
  a68_idf (A68_EXT, "dgamma", m, genie_gamma_double);
  a68_idf (A68_EXT, "dlngamma", m, genie_lngamma_double);
  a68_idf (A68_EXT, "dln", m, genie_ln_double);
  a68_idf (A68_EXT, "dlog", m, genie_log_double);
  a68_idf (A68_EXT, "dsec", m, genie_sec_double);
  a68_idf (A68_EXT, "dsindg", m, genie_sindg_double);
  a68_idf (A68_EXT, "dsinh", m, genie_sinh_double);
  a68_idf (A68_EXT, "dsin", m, genie_sin_double);
  a68_idf (A68_EXT, "dsinpi", m, genie_sinpi_double);
  a68_idf (A68_EXT, "dsqrt", m, genie_sqrt_double);
  a68_idf (A68_EXT, "dtandg", m, genie_tandg_double);
  a68_idf (A68_EXT, "dtanh", m, genie_tanh_double);
  a68_idf (A68_EXT, "dtan", m, genie_tan_double);
  a68_idf (A68_EXT, "dtanpi", m, genie_tanpi_double);
  a68_idf (A68_EXT, "longarccosdg", m, genie_acosdg_double);
  a68_idf (A68_EXT, "longarccosh", m, genie_acosh_double);
  a68_idf (A68_EXT, "longarccotdg", m, genie_acotdg_double);
  a68_idf (A68_EXT, "longarccot", m, genie_acot_double);
  a68_idf (A68_EXT, "longarccsc", m, genie_acsc_double);
  a68_idf (A68_EXT, "longarcsec", m, genie_asec_double);
  a68_idf (A68_EXT, "longarcsindg", m, genie_asindg_double);
  a68_idf (A68_EXT, "longarcsinh", m, genie_asinh_double);
  a68_idf (A68_EXT, "longarctandg", m, genie_atandg_double);
  a68_idf (A68_EXT, "longarctanh", m, genie_atanh_double);
  a68_idf (A68_EXT, "longcbrt", m, genie_curt_double);
  a68_idf (A68_EXT, "longcosdg", m, genie_cosdg_double);
  a68_idf (A68_EXT, "longcosh", m, genie_cosh_double);
  a68_idf (A68_EXT, "longcospi", m, genie_cospi_double);
  a68_idf (A68_EXT, "longcotdg", m, genie_cotdg_double);
  a68_idf (A68_EXT, "longcot", m, genie_cot_double);
  a68_idf (A68_EXT, "longcotpi", m, genie_cotpi_double);
  a68_idf (A68_EXT, "longcsc", m, genie_csc_double);
  a68_idf (A68_EXT, "longcurt", m, genie_curt_double);
  a68_idf (A68_EXT, "longerfc", m, genie_erfc_double);
  a68_idf (A68_EXT, "longerf", m, genie_erf_double);
  a68_idf (A68_EXT, "longinverfc", m, genie_inverfc_double);
  a68_idf (A68_EXT, "longinverf", m, genie_inverf_double);
  a68_idf (A68_EXT, "longgamma", m, genie_gamma_double);
  a68_idf (A68_EXT, "longlngamma", m, genie_lngamma_double);
  a68_idf (A68_EXT, "longsec", m, genie_sec_double);
  a68_idf (A68_EXT, "longsindg", m, genie_sindg_double);
  a68_idf (A68_EXT, "longsinh", m, genie_sinh_double);
  a68_idf (A68_EXT, "longsinpi", m, genie_sinpi_double);
  a68_idf (A68_EXT, "longtandg", m, genie_tandg_double);
  a68_idf (A68_EXT, "longtanh", m, genie_tanh_double);
  a68_idf (A68_EXT, "longtanpi", m, genie_tanpi_double);
  a68_idf (A68_STD, "longarccos", m, genie_acos_double);
  a68_idf (A68_STD, "longarcsin", m, genie_asin_double);
  a68_idf (A68_STD, "longarctan", m, genie_atan_double);
  a68_idf (A68_STD, "longcos", m, genie_cos_double);
  a68_idf (A68_STD, "longexp", m, genie_exp_double);
  a68_idf (A68_STD, "longln", m, genie_ln_double);
  a68_idf (A68_STD, "longlog", m, genie_log_double);
  a68_idf (A68_STD, "longsin", m, genie_sin_double);
  a68_idf (A68_STD, "longsqrt", m, genie_sqrt_double);
  a68_idf (A68_STD, "longtan", m, genie_tan_double);
//
  m = a68_proc (M_LONG_REAL, NO_MOID);
  a68_idf (A68_STD, "longnextrandom", m, genie_next_random_double);
  a68_idf (A68_STD, "longrandom", m, genie_next_random_double);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "darctan2dg", m, genie_atan2dg_double);
  a68_idf (A68_EXT, "darctan2", m, genie_atan2_double);
  a68_idf (A68_EXT, "dbeta", m, genie_beta_double);
  a68_idf (A68_EXT, "dgammaincgf", m, genie_gamma_inc_gf_double);
  a68_idf (A68_EXT, "dgammaincf", m, genie_gamma_inc_f_double);
  a68_idf (A68_EXT, "dgammainc", m, genie_gamma_inc_h_double);
  a68_idf (A68_EXT, "dlnbeta", m, genie_ln_beta_double);
  a68_idf (A68_EXT, "longarctan2dg", m, genie_atan2dg_double);
  a68_idf (A68_EXT, "longarctan2", m, genie_atan2_double);
  a68_idf (A68_EXT, "longbeta", m, genie_beta_double);
  a68_idf (A68_EXT, "longgammaincgf", m, genie_gamma_inc_gf_double);
  a68_idf (A68_EXT, "longgammaincf", m, genie_gamma_inc_f_double);
  a68_idf (A68_EXT, "longgammainc", m, genie_gamma_inc_h_double);
  a68_idf (A68_EXT, "longlnbeta", m, genie_ln_beta_double);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "longbetainc", m, genie_beta_inc_cf_double);
  a68_idf (A68_EXT, "dbetainc", m, genie_beta_inc_cf_double);
//
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "longgammaincg", m, genie_gamma_inc_g_double);
  a68_idf (A68_EXT, "dgammaincg", m, genie_gamma_inc_g_double);
// LONG BITS as 128 bit
  m = a68_proc (M_LONG_BITS, M_ROW_BOOL, NO_MOID);
  a68_idf (A68_STD, "longbitspack", m, genie_double_bits_pack);
//
  m = a68_proc (M_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_double_bits_to_bits);
//
  m = a68_proc (M_LONG_BITS, M_BITS, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_bits_to_double_bits);
//
  m = a68_proc (M_LONG_INT, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_idle);
//
  m = a68_proc (M_LONG_BITS, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "BIN", m, genie_bin_double_int);
//
  m = a68_proc (M_LONG_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_double_bits);
  a68_op (A68_STD, "~", m, genie_not_double_bits);
//
  m = a68_proc (M_LONG_BITS, M_LONG_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "AND", m, genie_and_double_bits);
  a68_op (A68_STD, "&", m, genie_and_double_bits);
  a68_op (A68_STD, "OR", m, genie_or_double_bits);
  a68_op (A68_EXT, "XOR", m, genie_xor_double_bits);
  a68_op (A68_EXT, "+", m, genie_add_double_bits);
  a68_op (A68_EXT, "-", m, genie_sub_double_bits);
  a68_op (A68_EXT, "*", m, genie_times_double_bits);
  a68_op (A68_EXT, "OVER", m, genie_over_double_bits);
  a68_op (A68_EXT, "MOD", m, genie_over_double_bits);
//
  m = a68_proc (M_BOOL, M_LONG_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_double_bits);
  a68_op (A68_STD, "/=", m, genie_ne_double_bits);
  a68_op (A68_STD, "~=", m, genie_ne_double_bits);
  a68_op (A68_STD, "^=", m, genie_ne_double_bits);
  a68_op (A68_STD, "<=", m, genie_le_double_bits);
  a68_op (A68_STD, ">=", m, genie_ge_double_bits);
  a68_op (A68_STD, "EQ", m, genie_eq_double_bits);
  a68_op (A68_STD, "NE", m, genie_ne_double_bits);
  a68_op (A68_STD, "LE", m, genie_le_double_bits);
  a68_op (A68_STD, "GE", m, genie_ge_double_bits);
  a68_op (A68_EXT, "<", m, genie_lt_double_bits);
  a68_op (A68_EXT, ">", m, genie_gt_double_bits);
  a68_op (A68_EXT, "LT", m, genie_lt_double_bits);
  a68_op (A68_EXT, "GT", m, genie_gt_double_bits);
//
  m = a68_proc (M_BOOL, M_INT, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_double_bits);
//
  m = a68_proc (M_LONG_BITS, M_INT, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "SET", m, genie_set_double_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_double_bits);
//
  m = a68_proc (M_LONG_BITS, M_LONG_BITS, M_INT, NO_MOID);
  a68_op (A68_STD, "SHL", m, genie_shl_double_bits);
  a68_op (A68_STD, "UP", m, genie_shl_double_bits);
  a68_op (A68_STD, "SHR", m, genie_shr_double_bits);
  a68_op (A68_STD, "DOWN", m, genie_shr_double_bits);
  a68_op (A68_EXT, "ROL", m, genie_rol_double_bits);
  a68_op (A68_EXT, "ROR", m, genie_ror_double_bits);
// LONG COMPLEX as 2 x 128 bit.
  m = a68_proc (M_LONG_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_complex_to_double_compl);
//
  m = a68_proc (M_LONG_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_double_compl_to_long_mp_complex);
//
  m = a68_proc (M_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_double_compl_to_complex);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_complex_to_double_compl);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_op (A68_STD, "I", m, genie_i_double_compl);
  a68_op (A68_STD, "+*", m, genie_i_double_compl);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_INT, M_LONG_INT, NO_MOID);
  a68_op (A68_STD, "I", m, genie_i_int_double_compl);
  a68_op (A68_STD, "+*", m, genie_i_int_double_compl);
//
  m = a68_proc (M_LONG_REAL, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "RE", m, genie_re_double_compl);
  a68_op (A68_STD, "IM", m, genie_im_double_compl);
  a68_op (A68_STD, "ABS", m, genie_abs_double_compl);
  a68_op (A68_STD, "ARG", m, genie_arg_double_compl);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+", m, genie_idle);
  a68_op (A68_STD, "-", m, genie_minus_double_compl);
  a68_op (A68_STD, "CONJ", m, genie_conj_double_compl);
//
  m = a68_proc (M_BOOL, M_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_double_compl);
  a68_op (A68_STD, "/=", m, genie_ne_double_compl);
  a68_op (A68_STD, "~=", m, genie_ne_double_compl);
  a68_op (A68_STD, "^=", m, genie_ne_double_compl);
  a68_op (A68_STD, "EQ", m, genie_eq_double_compl);
  a68_op (A68_STD, "NE", m, genie_ne_double_compl);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+", m, genie_add_double_compl);
  a68_op (A68_STD, "-", m, genie_sub_double_compl);
  a68_op (A68_STD, "*", m, genie_mul_double_compl);
  a68_op (A68_STD, "/", m, genie_div_double_compl);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_COMPLEX, M_INT, NO_MOID);
  a68_op (A68_STD, "**", m, genie_pow_double_compl_int);
  a68_op (A68_STD, "UP", m, genie_pow_double_compl_int);
  a68_op (A68_STD, "^", m, genie_pow_double_compl_int);
//
  m = a68_proc (M_REF_LONG_COMPLEX, M_REF_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_op (A68_STD, "+:=", m, genie_plusab_double_compl);
  a68_op (A68_STD, "-:=", m, genie_minusab_double_compl);
  a68_op (A68_STD, "*:=", m, genie_timesab_double_compl);
  a68_op (A68_STD, "/:=", m, genie_divab_double_compl);
  a68_op (A68_STD, "PLUSAB", m, genie_plusab_double_compl);
  a68_op (A68_STD, "MINUSAB", m, genie_minusab_double_compl);
  a68_op (A68_STD, "TIMESAB", m, genie_timesab_double_compl);
  a68_op (A68_STD, "DIVAB", m, genie_divab_double_compl);
//
  m = a68_proc (M_LONG_COMPLEX, M_LONG_COMPLEX, NO_MOID);
  a68_idf (A68_EXT, "dcacosh", m, genie_acosh_double_compl);
  a68_idf (A68_EXT, "dcacos", m, genie_acos_double_compl);
  a68_idf (A68_EXT, "dcarccosh", m, genie_acosh_double_compl);
  a68_idf (A68_EXT, "dcarccos", m, genie_acos_double_compl);
  a68_idf (A68_EXT, "dcarcsinh", m, genie_asinh_double_compl);
  a68_idf (A68_EXT, "dcarcsin", m, genie_asin_double_compl);
  a68_idf (A68_EXT, "dcarctanh", m, genie_atanh_double_compl);
  a68_idf (A68_EXT, "dcarctan", m, genie_atan_double_compl);
  a68_idf (A68_EXT, "dcasinh", m, genie_asinh_double_compl);
  a68_idf (A68_EXT, "dcasin", m, genie_asin_double_compl);
  a68_idf (A68_EXT, "dcatanh", m, genie_atanh_double_compl);
  a68_idf (A68_EXT, "dcatan", m, genie_atan_double_compl);
  a68_idf (A68_EXT, "dccosh", m, genie_cosh_double_compl);
  a68_idf (A68_EXT, "dccos", m, genie_cos_double_compl);
  a68_idf (A68_EXT, "dcexp", m, genie_exp_double_compl);
  a68_idf (A68_EXT, "dcln", m, genie_ln_double_compl);
  a68_idf (A68_EXT, "dcomplexacosh", m, genie_acosh_double_compl);
  a68_idf (A68_EXT, "dcomplexacos", m, genie_acos_double_compl);
  a68_idf (A68_EXT, "dcomplexarccosh", m, genie_acosh_double_compl);
  a68_idf (A68_EXT, "dcomplexarccos", m, genie_acos_double_compl);
  a68_idf (A68_EXT, "dcomplexarcsinh", m, genie_asinh_double_compl);
  a68_idf (A68_EXT, "dcomplexarcsin", m, genie_asin_double_compl);
  a68_idf (A68_EXT, "dcomplexarctanh", m, genie_atanh_double_compl);
  a68_idf (A68_EXT, "dcomplexarctan", m, genie_atan_double_compl);
  a68_idf (A68_EXT, "dcomplexasinh", m, genie_asinh_double_compl);
  a68_idf (A68_EXT, "dcomplexasin", m, genie_asin_double_compl);
  a68_idf (A68_EXT, "dcomplexatanh", m, genie_atanh_double_compl);
  a68_idf (A68_EXT, "dcomplexatan", m, genie_atan_double_compl);
  a68_idf (A68_EXT, "dcomplexcosh", m, genie_cosh_double_compl);
  a68_idf (A68_EXT, "dcomplexcos", m, genie_cos_double_compl);
  a68_idf (A68_EXT, "dcomplexexp", m, genie_exp_double_compl);
  a68_idf (A68_EXT, "dcomplexln", m, genie_ln_double_compl);
  a68_idf (A68_EXT, "dcomplexsin", m, genie_sin_double_compl);
  a68_idf (A68_EXT, "dcomplexsqrt", m, genie_sqrt_double_compl);
  a68_idf (A68_EXT, "dcomplextanh", m, genie_tanh_double_compl);
  a68_idf (A68_EXT, "dcomplextan", m, genie_tan_double_compl);
  a68_idf (A68_EXT, "dcsinh", m, genie_sinh_double_compl);
  a68_idf (A68_EXT, "dcsin", m, genie_sin_double_compl);
  a68_idf (A68_EXT, "dcsqrt", m, genie_sqrt_double_compl);
  a68_idf (A68_EXT, "dctanh", m, genie_tanh_double_compl);
  a68_idf (A68_EXT, "dctan", m, genie_tan_double_compl);
  a68_idf (A68_EXT, "longcacosh", m, genie_acosh_double_compl);
  a68_idf (A68_EXT, "longcacos", m, genie_acos_double_compl);
  a68_idf (A68_EXT, "longcarccosh", m, genie_acosh_double_compl);
  a68_idf (A68_EXT, "longcarccos", m, genie_acos_double_compl);
  a68_idf (A68_EXT, "longcarcsinh", m, genie_asinh_double_compl);
  a68_idf (A68_EXT, "longcarcsin", m, genie_asin_double_compl);
  a68_idf (A68_EXT, "longcarctanh", m, genie_atanh_double_compl);
  a68_idf (A68_EXT, "longcarctan", m, genie_atan_double_compl);
  a68_idf (A68_EXT, "longcasinh", m, genie_asinh_double_compl);
  a68_idf (A68_EXT, "longcasin", m, genie_asin_double_compl);
  a68_idf (A68_EXT, "longcatanh", m, genie_atanh_double_compl);
  a68_idf (A68_EXT, "longcatan", m, genie_atan_double_compl);
  a68_idf (A68_EXT, "longccosh", m, genie_cosh_double_compl);
  a68_idf (A68_EXT, "longccos", m, genie_cos_double_compl);
  a68_idf (A68_EXT, "longcexp", m, genie_exp_double_compl);
  a68_idf (A68_EXT, "longcln", m, genie_ln_double_compl);
  a68_idf (A68_EXT, "longcomplexacosh", m, genie_acosh_double_compl);
  a68_idf (A68_EXT, "longcomplexacos", m, genie_acos_double_compl);
  a68_idf (A68_EXT, "longcomplexarccosh", m, genie_acosh_double_compl);
  a68_idf (A68_EXT, "longcomplexarccos", m, genie_acos_double_compl);
  a68_idf (A68_EXT, "longcomplexarcsinh", m, genie_asinh_double_compl);
  a68_idf (A68_EXT, "longcomplexarcsin", m, genie_asin_double_compl);
  a68_idf (A68_EXT, "longcomplexarctanh", m, genie_atanh_double_compl);
  a68_idf (A68_EXT, "longcomplexarctan", m, genie_atan_double_compl);
  a68_idf (A68_EXT, "longcomplexasinh", m, genie_asinh_double_compl);
  a68_idf (A68_EXT, "longcomplexasin", m, genie_asin_double_compl);
  a68_idf (A68_EXT, "longcomplexatanh", m, genie_atanh_double_compl);
  a68_idf (A68_EXT, "longcomplexatan", m, genie_atan_double_compl);
  a68_idf (A68_EXT, "longcomplexcosh", m, genie_cosh_double_compl);
  a68_idf (A68_EXT, "longcomplexcos", m, genie_cos_double_compl);
  a68_idf (A68_EXT, "longcomplexexp", m, genie_exp_double_compl);
  a68_idf (A68_EXT, "longcomplexln", m, genie_ln_double_compl);
  a68_idf (A68_EXT, "longcomplexsinh", m, genie_sinh_double_compl);
  a68_idf (A68_EXT, "longcomplexsin", m, genie_sin_double_compl);
  a68_idf (A68_EXT, "longcomplexsqrt", m, genie_sqrt_double_compl);
  a68_idf (A68_EXT, "longcomplextanh", m, genie_tanh_double_compl);
  a68_idf (A68_EXT, "longcomplextan", m, genie_tan_double_compl);
  a68_idf (A68_EXT, "longcsinh", m, genie_sinh_double_compl);
  a68_idf (A68_EXT, "longcsin", m, genie_sin_double_compl);
  a68_idf (A68_EXT, "longcsqrt", m, genie_sqrt_double_compl);
  a68_idf (A68_EXT, "longctanh", m, genie_tanh_double_compl);
  a68_idf (A68_EXT, "longctan", m, genie_tan_double_compl);

#if defined (HAVE_GNU_MPFR)
  m = a68_proc (M_LONG_REAL, M_LONG_REAL, M_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "mpfrlonggammainc", m, genie_gamma_inc_double_mpfr);
  a68_idf (A68_EXT, "mpfrdgammainc", m, genie_gamma_inc_double_mpfr);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_idf (A68_STD, "mpfrlonglongbeta", m, genie_beta_mpfr);
  a68_idf (A68_STD, "mpfrqbeta", m, genie_beta_mpfr);
  a68_idf (A68_STD, "mpfrlonglonglnbeta", m, genie_ln_beta_mpfr);
  a68_idf (A68_STD, "mpfrqlnbeta", m, genie_ln_beta_mpfr);
  a68_idf (A68_STD, "mpfrlonglonggammainc", m, genie_gamma_inc_mpfr);
  a68_idf (A68_STD, "mpfrqgammainc", m, genie_gamma_inc_mpfr);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_idf (A68_STD, "mpfrlonglongbetainc", m, genie_beta_inc_mpfr);
  a68_idf (A68_STD, "mpfrqbetainc", m, genie_beta_inc_mpfr);
//
  m = a68_proc (M_LONG_LONG_REAL, M_LONG_LONG_REAL, NO_MOID);
  a68_idf (A68_EXT, "mpfrlonglonggamma", m, genie_gamma_mpfr);
  a68_idf (A68_EXT, "mpfrlonglonglngamma", m, genie_lngamma_mpfr);
  a68_idf (A68_EXT, "mpfrlonglongerfc", m, genie_mpfr_erfc_mp);
  a68_idf (A68_EXT, "mpfrlonglongerf", m, genie_mpfr_erf_mp);
  a68_idf (A68_EXT, "mpfrlonglonginverfc", m, genie_mpfr_inverfc_mp);
  a68_idf (A68_EXT, "mpfrlonglonginverf", m, genie_mpfr_inverf_mp);
  a68_idf (A68_EXT, "mpfrmp", m, genie_mpfr_mp);
  a68_idf (A68_EXT, "mpfrqgamma", m, genie_gamma_mpfr);
  a68_idf (A68_EXT, "mpfrqlngamma", m, genie_lngamma_mpfr);
  a68_idf (A68_EXT, "mpfrqerfc", m, genie_mpfr_erfc_mp);
  a68_idf (A68_EXT, "mpfrqerf", m, genie_mpfr_erf_mp);
  a68_idf (A68_EXT, "mpfrqinverfc", m, genie_mpfr_inverfc_mp);
  a68_idf (A68_EXT, "mpfrqinverf", m, genie_mpfr_inverf_mp);
  a68_idf (A68_EXT, "mpfrq", m, genie_mpfr_mp);
#endif
#endif
}

void stand_transput (void)
{
  MOID_T *m;
  a68_idf (A68_EXT, "blankcharacter", M_CHAR, genie_blank_char);
  a68_idf (A68_EXT, "formfeedcharacter", M_CHAR, genie_formfeed_char);
  a68_idf (A68_EXT, "formfeedchar", M_CHAR, genie_formfeed_char);
  a68_idf (A68_EXT, "newlinecharacter", M_CHAR, genie_newline_char);
  a68_idf (A68_EXT, "newlinechar", M_CHAR, genie_newline_char);
  a68_idf (A68_EXT, "nullcharacter", M_CHAR, genie_null_char);
  a68_idf (A68_EXT, "tabcharacter", M_CHAR, genie_tab_char);
  a68_idf (A68_EXT, "tabchar", M_CHAR, genie_tab_char);
  a68_idf (A68_STD, "blankchar", M_CHAR, genie_blank_char);
  a68_idf (A68_STD, "blank", M_CHAR, genie_blank_char);
  a68_idf (A68_STD, "errorchar", M_CHAR, genie_error_char);
  a68_idf (A68_STD, "expchar", M_CHAR, genie_exp_char);
  a68_idf (A68_STD, "flip", M_CHAR, genie_flip_char);
  a68_idf (A68_STD, "flop", M_CHAR, genie_flop_char);
  a68_idf (A68_STD, "nullchar", M_CHAR, genie_null_char);
//
  m = a68_proc (M_STRING, M_HEX_NUMBER, M_INT, M_INT, NO_MOID);
  a68_idf (A68_STD, "bits", m, genie_bits);
//
  m = a68_proc (M_STRING, M_NUMBER, M_INT, NO_MOID);
  a68_idf (A68_STD, "whole", m, genie_whole);
//
  m = a68_proc (M_STRING, M_NUMBER, M_INT, M_INT, NO_MOID);
  a68_idf (A68_STD, "fixed", m, genie_fixed);
//
  m = a68_proc (M_STRING, M_NUMBER, M_INT, M_INT, M_INT, NO_MOID);
  a68_idf (A68_STD, "float", m, genie_float);
//
  m = a68_proc (M_STRING, M_NUMBER, M_INT, M_INT, M_INT, M_INT, NO_MOID);
  a68_idf (A68_STD, "real", m, genie_real);
  a68_idf (A68_STD, "standin", M_REF_FILE, genie_stand_in);
  a68_idf (A68_STD, "standout", M_REF_FILE, genie_stand_out);
  a68_idf (A68_STD, "standback", M_REF_FILE, genie_stand_back);
  a68_idf (A68_EXT, "standerror", M_REF_FILE, genie_stand_error);
  a68_idf (A68_STD, "standinchannel", M_CHANNEL, genie_stand_in_channel);
  a68_idf (A68_STD, "standoutchannel", M_CHANNEL, genie_stand_out_channel);
  a68_idf (A68_EXT, "standdrawchannel", M_CHANNEL, genie_stand_draw_channel);
  a68_idf (A68_STD, "standbackchannel", M_CHANNEL, genie_stand_back_channel);
  a68_idf (A68_EXT, "standerrorchannel", M_CHANNEL, genie_stand_error_channel);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_STRING, NO_MOID);
  a68_idf (A68_STD, "maketerm", m, genie_make_term);
//
  m = a68_proc (M_BOOL, M_CHAR, M_REF_INT, M_STRING, NO_MOID);
  a68_idf (A68_STD, "charinstring", m, genie_char_in_string);
  a68_idf (A68_EXT, "lastcharinstring", m, genie_last_char_in_string);
//
  m = a68_proc (M_BOOL, M_STRING, M_REF_INT, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "stringinstring", m, genie_string_in_string);
//
  m = a68_proc (M_STRING, M_REF_FILE, NO_MOID);
  a68_idf (A68_EXT, "idf", m, genie_idf);
  a68_idf (A68_EXT, "term", m, genie_term);
//
  m = a68_proc (M_STRING, NO_MOID);
  a68_idf (A68_EXT, "programidf", m, genie_program_idf);
// Event routines.
  m = a68_proc (M_VOID, M_REF_FILE, M_PROC_REF_FILE_BOOL, NO_MOID);
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
// Enquiries on files.
  a68_idf (A68_EXT, "drawpossible", M_PROC_REF_FILE_BOOL, genie_draw_possible);
  a68_idf (A68_EXT, "endoffile", M_PROC_REF_FILE_BOOL, genie_eof);
  a68_idf (A68_EXT, "endofline", M_PROC_REF_FILE_BOOL, genie_eoln);
  a68_idf (A68_EXT, "eof", M_PROC_REF_FILE_BOOL, genie_eof);
  a68_idf (A68_EXT, "eoln", M_PROC_REF_FILE_BOOL, genie_eoln);
  a68_idf (A68_EXT, "rewindpossible", M_PROC_REF_FILE_BOOL, genie_reset_possible);
  a68_idf (A68_STD, "binpossible", M_PROC_REF_FILE_BOOL, genie_bin_possible);
  a68_idf (A68_STD, "compressible", M_PROC_REF_FILE_BOOL, genie_compressible);
  a68_idf (A68_STD, "getpossible", M_PROC_REF_FILE_BOOL, genie_get_possible);
  a68_idf (A68_STD, "putpossible", M_PROC_REF_FILE_BOOL, genie_put_possible);
  a68_idf (A68_STD, "reidfpossible", M_PROC_REF_FILE_BOOL, genie_reidf_possible);
  a68_idf (A68_STD, "resetpossible", M_PROC_REF_FILE_BOOL, genie_reset_possible);
  a68_idf (A68_STD, "setpossible", M_PROC_REF_FILE_BOOL, genie_set_possible);
// Handling of files.
  m = a68_proc (M_INT, M_REF_FILE, M_STRING, M_CHANNEL, NO_MOID);
  a68_idf (A68_STD, "open", m, genie_open);
  a68_idf (A68_STD, "establish", m, genie_establish);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_REF_STRING, NO_MOID);
  a68_idf (A68_STD, "associate", m, genie_associate);
//
  m = a68_proc (M_INT, M_REF_FILE, M_CHANNEL, NO_MOID);
  a68_idf (A68_EXT, "rewind", M_PROC_REF_FILE_VOID, genie_reset);
  a68_idf (A68_STD, "backspace", M_PROC_REF_FILE_VOID, genie_backspace);
  a68_idf (A68_STD, "close", M_PROC_REF_FILE_VOID, genie_close);
  a68_idf (A68_STD, "create", m, genie_create);
  a68_idf (A68_STD, "erase", M_PROC_REF_FILE_VOID, genie_erase);
  a68_idf (A68_STD, "lock", M_PROC_REF_FILE_VOID, genie_lock);
  a68_idf (A68_STD, "newline", M_PROC_REF_FILE_VOID, genie_new_line);
  a68_idf (A68_STD, "newpage", M_PROC_REF_FILE_VOID, genie_new_page);
  a68_idf (A68_STD, "reset", M_PROC_REF_FILE_VOID, genie_reset);
  a68_idf (A68_STD, "scratch", M_PROC_REF_FILE_VOID, genie_erase);
  a68_idf (A68_STD, "space", M_PROC_REF_FILE_VOID, genie_space);
//
  m = a68_proc (M_INT, M_REF_FILE, M_INT, NO_MOID);
  a68_idf (A68_STD, "set", m, genie_set);
  a68_idf (A68_STD, "seek", m, genie_set);
//
  m = a68_proc (M_VOID, M_ROW_SIMPLIN, NO_MOID);
  a68_idf (A68_STD, "read", m, genie_read);
  a68_idf (A68_STD, "readbin", m, genie_read_bin);
  a68_idf (A68_STD, "readf", m, genie_read_format);
//
  m = a68_proc (M_VOID, M_ROW_SIMPLOUT, NO_MOID);
  a68_idf (A68_STD, "printbin", m, genie_write_bin);
  a68_idf (A68_STD, "printf", m, genie_write_format);
  a68_idf (A68_STD, "print", m, genie_write);
  a68_idf (A68_STD, "writebin", m, genie_write_bin);
  a68_idf (A68_STD, "writef", m, genie_write_format);
  a68_idf (A68_STD, "write", m, genie_write);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_ROW_SIMPLIN, NO_MOID);
  a68_idf (A68_STD, "get", m, genie_read_file);
  a68_idf (A68_STD, "getf", m, genie_read_file_format);
  a68_idf (A68_STD, "getbin", m, genie_read_bin_file);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_ROW_SIMPLOUT, NO_MOID);
  a68_idf (A68_STD, "put", m, genie_write_file);
  a68_idf (A68_STD, "putf", m, genie_write_file_format);
  a68_idf (A68_STD, "putbin", m, genie_write_bin_file);

  A68C_DEFIO (bits, bits, BITS);
  A68C_DEFIO (bool, bool, BOOL);
  A68C_DEFIO (char, char, CHAR);
  A68C_DEFIO (compl, complex, COMPLEX);
  A68C_DEFIO (complex, complex, COMPLEX);
  A68C_DEFIO (double, long_real, LONG_REAL);
  A68C_DEFIO (int, int, INT);
  A68C_DEFIO (longbits, long_bits, LONG_BITS);
  A68C_DEFIO (longcomplex, mp_complex, LONG_COMPLEX);
  A68C_DEFIO (longcompl, mp_complex, LONG_COMPLEX);
  A68C_DEFIO (longint, long_int, LONG_INT);
  A68C_DEFIO (longlongcomplex, long_mp_complex, LONG_LONG_COMPLEX);
  A68C_DEFIO (longlongcompl, long_mp_complex, LONG_LONG_COMPLEX);
  A68C_DEFIO (longlongint, long_mp_int, LONG_LONG_INT);
  A68C_DEFIO (longlongreal, long_mp_real, LONG_LONG_REAL);
  A68C_DEFIO (longreal, long_real, LONG_REAL);
  A68C_DEFIO (quad, long_mp_real, LONG_LONG_REAL);
  A68C_DEFIO (real, real, REAL);
  A68C_DEFIO (string, string, STRING);
  a68_idf (A68_EXT, "readline", M_PROC_STRING, genie_read_line);
}

//! @brief Set up standenv - extensions.

void stand_extensions (void)
{
  MOID_T *m = NO_MOID;
// UNIX things.
  m = A68_MCACHE (proc_int);
  a68_idf (A68_EXT, "rows", m, genie_rows);
  a68_idf (A68_EXT, "columns", m, genie_columns);
  a68_idf (A68_EXT, "argc", m, genie_argc);
  a68_idf (A68_EXT, "a68gargc", m, genie_a68_argc);
  a68_idf (A68_EXT, "errno", m, genie_errno);
  a68_idf (A68_EXT, "fork", m, genie_fork);
//
  m = a68_proc (M_STRING, NO_MOID);
  a68_idf (A68_EXT, "getpwd", m, genie_pwd);
//
  m = a68_proc (M_INT, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "setpwd", m, genie_cd);
//
  m = a68_proc (M_BOOL, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "fileisdirectory", m, genie_file_is_directory);
  a68_idf (A68_EXT, "fileisblockdevice", m, genie_file_is_block_device);
  a68_idf (A68_EXT, "fileischardevice", m, genie_file_is_char_device);
  a68_idf (A68_EXT, "fileisregular", m, genie_file_is_regular);
#if defined (S_ISFIFO)
  a68_idf (A68_EXT, "fileisfifo", m, genie_file_is_fifo);
#endif
#if defined (S_ISLNK)
  a68_idf (A68_EXT, "fileislink", m, genie_file_is_link);
#endif
  m = a68_proc (M_BITS, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "filemode", m, genie_file_mode);
//
  m = a68_proc (M_STRING, M_INT, NO_MOID);
  a68_idf (A68_EXT, "argv", m, genie_argv);
  a68_idf (A68_EXT, "a68gargv", m, genie_a68_argv);
  a68_idf (A68_EXT, "reseterrno", A68_MCACHE (proc_void), genie_reset_errno);
//
  m = a68_proc (M_STRING, M_INT, NO_MOID);
  a68_idf (A68_EXT, "strerror", m, genie_strerror);
//
  m = a68_proc (M_INT, M_STRING, M_ROW_STRING, M_ROW_STRING, NO_MOID);
  a68_idf (A68_EXT, "exec", m, genie_exec);
  a68_idf (A68_EXT, "execve", m, genie_exec);
//
  m = a68_proc (M_PIPE, NO_MOID);
  a68_idf (A68_EXT, "createpipe", m, genie_create_pipe);
//
  m = a68_proc (M_INT, M_STRING, M_ROW_STRING, M_ROW_STRING, NO_MOID);
  a68_idf (A68_EXT, "execsub", m, genie_exec_sub);
  a68_idf (A68_EXT, "execvechild", m, genie_exec_sub);
//
  m = a68_proc (M_PIPE, M_STRING, M_ROW_STRING, M_ROW_STRING, NO_MOID);
  a68_idf (A68_EXT, "execsubpipeline", m, genie_exec_sub_pipeline);
  a68_idf (A68_EXT, "execvechildpipe", m, genie_exec_sub_pipeline);
//
  m = a68_proc (M_INT, M_STRING, M_ROW_STRING, M_ROW_STRING, M_REF_STRING, NO_MOID);
  a68_idf (A68_EXT, "execsuboutput", m, genie_exec_sub_output);
  a68_idf (A68_EXT, "execveoutput", m, genie_exec_sub_output);
//
  m = a68_proc (M_STRING, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "getenv", m, genie_getenv);
//
  m = a68_proc (M_VOID, M_INT, NO_MOID);
  a68_idf (A68_EXT, "waitpid", m, genie_waitpid);
//
  m = a68_proc (M_ROW_INT, NO_MOID);
  a68_idf (A68_EXT, "utctime", m, genie_utctime);
  a68_idf (A68_EXT, "localtime", m, genie_localtime);
//
  m = a68_proc (M_INT, M_STRING, M_STRING, M_REF_INT, M_REF_INT, NO_MOID);
  a68_idf (A68_EXT, "grepinstring", m, genie_grep_in_string);
  a68_idf (A68_EXT, "grepinsubstring", m, genie_grep_in_substring);
//
  m = a68_proc (M_INT, M_STRING, M_STRING, M_REF_STRING, NO_MOID);
  a68_idf (A68_EXT, "subinstring", m, genie_sub_in_string);
#if defined (HAVE_DIRENT_H)
  m = a68_proc (M_ROW_STRING, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "getdirectory", m, genie_directory);
#endif
#if defined (BUILD_HTTP)
  m = a68_proc (M_INT, M_REF_STRING, M_STRING, M_STRING, M_INT, NO_MOID);
  a68_idf (A68_EXT, "httpcontent", m, genie_http_content);
  a68_idf (A68_EXT, "tcprequest", m, genie_tcp_request);
#endif
}

#if defined (HAVE_GNU_PLOTUTILS)

void stand_plot (void)
{
  MOID_T *m = NO_MOID;
// Drawing.
  m = a68_proc (M_BOOL, M_REF_FILE, M_STRING, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "drawdevice", m, genie_make_device);
  a68_idf (A68_EXT, "makedevice", m, genie_make_device);
//
  m = a68_proc (M_REAL, M_REF_FILE, NO_MOID);
  a68_idf (A68_EXT, "drawaspect", m, genie_draw_aspect);
//
  m = a68_proc (M_VOID, M_REF_FILE, NO_MOID);
  a68_idf (A68_EXT, "drawclear", m, genie_draw_clear);
  a68_idf (A68_EXT, "drawerase", m, genie_draw_clear);
  a68_idf (A68_EXT, "drawflush", m, genie_draw_show);
  a68_idf (A68_EXT, "drawshow", m, genie_draw_show);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_INT, NO_MOID);
  a68_idf (A68_EXT, "drawfillstyle", m, genie_draw_fillstyle);
//
  m = a68_proc (M_STRING, M_INT, NO_MOID);
  a68_idf (A68_EXT, "drawgetcolourname", m, genie_draw_get_colour_name);
  a68_idf (A68_EXT, "drawgetcolorname", m, genie_draw_get_colour_name);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_REAL, M_REAL, M_REAL, NO_MOID);
  a68_idf (A68_EXT, "drawcolor", m, genie_draw_colour);
  a68_idf (A68_EXT, "drawcolour", m, genie_draw_colour);
  a68_idf (A68_EXT, "drawbackgroundcolor", m, genie_draw_background_colour);
  a68_idf (A68_EXT, "drawbackgroundcolour", m, genie_draw_background_colour);
  a68_idf (A68_EXT, "drawcircle", m, genie_draw_circle);
  a68_idf (A68_EXT, "drawball", m, genie_draw_atom);
  a68_idf (A68_EXT, "drawstar", m, genie_draw_star);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_REAL, M_REAL, NO_MOID);
  a68_idf (A68_EXT, "drawpoint", m, genie_draw_point);
  a68_idf (A68_EXT, "drawline", m, genie_draw_line);
  a68_idf (A68_EXT, "drawmove", m, genie_draw_move);
  a68_idf (A68_EXT, "drawrect", m, genie_draw_rect);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_CHAR, M_CHAR, M_ROW_CHAR, NO_MOID);
  a68_idf (A68_EXT, "drawtext", m, genie_draw_text);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_ROW_CHAR, NO_MOID);
  a68_idf (A68_EXT, "drawlinestyle", m, genie_draw_linestyle);
  a68_idf (A68_EXT, "drawfontname", m, genie_draw_fontname);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_REAL, NO_MOID);
  a68_idf (A68_EXT, "drawlinewidth", m, genie_draw_linewidth);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_INT, NO_MOID);
  a68_idf (A68_EXT, "drawfontsize", m, genie_draw_fontsize);
  a68_idf (A68_EXT, "drawtextangle", m, genie_draw_textangle);
//
  m = a68_proc (M_VOID, M_REF_FILE, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "drawcolorname", m, genie_draw_colour_name);
  a68_idf (A68_EXT, "drawcolourname", m, genie_draw_colour_name);
  a68_idf (A68_EXT, "drawbackgroundcolorname", m, genie_draw_background_colour_name);
  a68_idf (A68_EXT, "drawbackgroundcolourname", m, genie_draw_background_colour_name);
}

#endif

#if defined (HAVE_CURSES)

void stand_curses (void)
{
  MOID_T *m;
  a68_idf (A68_EXT, "cursesstart", A68_MCACHE (proc_void), genie_curses_start);
  a68_idf (A68_EXT, "cursesend", A68_MCACHE (proc_void), genie_curses_end);
  a68_idf (A68_EXT, "cursesclear", A68_MCACHE (proc_void), genie_curses_clear);
  a68_idf (A68_EXT, "cursesrefresh", A68_MCACHE (proc_void), genie_curses_refresh);
  a68_idf (A68_EXT, "cursesgreen", A68_MCACHE (proc_void), genie_curses_green);
  a68_idf (A68_EXT, "cursescyan", A68_MCACHE (proc_void), genie_curses_cyan);
  a68_idf (A68_EXT, "cursesred", A68_MCACHE (proc_void), genie_curses_red);
  a68_idf (A68_EXT, "cursesyellow", A68_MCACHE (proc_void), genie_curses_yellow);
  a68_idf (A68_EXT, "cursesmagenta", A68_MCACHE (proc_void), genie_curses_magenta);
  a68_idf (A68_EXT, "cursesblue", A68_MCACHE (proc_void), genie_curses_blue);
  a68_idf (A68_EXT, "curseswhite", A68_MCACHE (proc_void), genie_curses_white);
  a68_idf (A68_EXT, "cursesgreeninverse", A68_MCACHE (proc_void), genie_curses_green_inverse);
  a68_idf (A68_EXT, "cursescyaninverse", A68_MCACHE (proc_void), genie_curses_cyan_inverse);
  a68_idf (A68_EXT, "cursesredinverse", A68_MCACHE (proc_void), genie_curses_red_inverse);
  a68_idf (A68_EXT, "cursesyellowinverse", A68_MCACHE (proc_void), genie_curses_yellow_inverse);
  a68_idf (A68_EXT, "cursesmagentainverse", A68_MCACHE (proc_void), genie_curses_magenta_inverse);
  a68_idf (A68_EXT, "cursesblueinverse", A68_MCACHE (proc_void), genie_curses_blue_inverse);
  a68_idf (A68_EXT, "curseswhiteinverse", A68_MCACHE (proc_void), genie_curses_white_inverse);
//
  m = A68_MCACHE (proc_char);
  a68_idf (A68_EXT, "cursesgetchar", m, genie_curses_getchar);
//
  m = a68_proc (M_VOID, M_CHAR, NO_MOID);
  a68_idf (A68_EXT, "cursesputchar", m, genie_curses_putchar);
//
  m = a68_proc (M_VOID, M_INT, M_INT, NO_MOID);
  a68_idf (A68_EXT, "cursesmove", m, genie_curses_move);
//
  m = A68_MCACHE (proc_int);
  a68_idf (A68_EXT, "curseslines", m, genie_curses_lines);
  a68_idf (A68_EXT, "cursescolumns", m, genie_curses_columns);
//
  m = a68_proc (M_BOOL, M_CHAR, NO_MOID);
  a68_idf (A68_EXT, "cursesdelchar", m, genie_curses_del_char);
}

#endif

#if defined (HAVE_POSTGRESQL)

void stand_postgresql (void)
{
  MOID_T *m = NO_MOID;
  m = a68_proc (M_INT, M_REF_FILE, M_STRING, M_REF_STRING, NO_MOID);
  a68_idf (A68_EXT, "pqconnectdb", m, genie_pq_connectdb);
//
  m = a68_proc (M_INT, M_REF_FILE, NO_MOID);
  a68_idf (A68_EXT, "pqfinish", m, genie_pq_finish);
  a68_idf (A68_EXT, "pqreset", m, genie_pq_reset);
//
  m = a68_proc (M_INT, M_REF_FILE, M_STRING, NO_MOID);
  a68_idf (A68_EXT, "pqparameterstatus", m, genie_pq_parameterstatus);
  a68_idf (A68_EXT, "pqexec", m, genie_pq_exec);
  a68_idf (A68_EXT, "pqfnumber", m, genie_pq_fnumber);
//
  m = a68_proc (M_INT, M_REF_FILE, NO_MOID);
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
//
  m = a68_proc (M_INT, M_REF_FILE, M_INT, NO_MOID);
  a68_idf (A68_EXT, "pqfname", m, genie_pq_fname);
  a68_idf (A68_EXT, "pqfformat", m, genie_pq_fformat);
//
  m = a68_proc (M_INT, M_REF_FILE, M_INT, M_INT, NO_MOID);
  a68_idf (A68_EXT, "pqgetvalue", m, genie_pq_getvalue);
  a68_idf (A68_EXT, "pqgetisnull", m, genie_pq_getisnull);
}

#endif

#if defined (BUILD_LINUX)
void stand_linux (void)
{
  a68_idf (A68_EXT, "sigsegv", A68_MCACHE (proc_void), genie_sigsegv);
}
#endif

//! @brief Build the standard environ symbol table.

void make_standard_environ (void)
{
  stand_moids ();
//
  A68_MCACHE (proc_bool) = a68_proc (M_BOOL, NO_MOID);
  A68_MCACHE (proc_char) = a68_proc (M_CHAR, NO_MOID);
  A68_MCACHE (proc_complex_complex) = a68_proc (M_COMPLEX, M_COMPLEX, NO_MOID);
  A68_MCACHE (proc_int) = a68_proc (M_INT, NO_MOID);
  A68_MCACHE (proc_int_int) = a68_proc (M_INT, M_INT, NO_MOID);
  A68_MCACHE (proc_int_int_real) = a68_proc (M_REAL, M_INT, M_INT, NO_MOID);
  A68_MCACHE (proc_int_real) = a68_proc (M_REAL, M_INT, NO_MOID);
  A68_MCACHE (proc_int_real_real) = a68_proc (M_REAL, M_INT, M_REAL, NO_MOID);
  A68_MCACHE (proc_int_real_real_real) = a68_proc (M_REAL, M_INT, M_REAL, M_REAL, NO_MOID);
  A68_MCACHE (proc_real) = a68_proc (M_REAL, NO_MOID);
  A68_MCACHE (proc_real_int_real) = a68_proc (M_REAL, M_REAL, M_INT, NO_MOID);
  A68_MCACHE (proc_real_real_int_real) = a68_proc (M_REAL, M_REAL, M_REAL, M_INT, NO_MOID);
  A68_MCACHE (proc_real_real) = M_PROC_REAL_REAL;
  A68_MCACHE (proc_real_real_real) = a68_proc (M_REAL, M_REAL, M_REAL, NO_MOID);
  A68_MCACHE (proc_real_real_real_int) = a68_proc (M_INT, M_REAL, M_REAL, M_REAL, NO_MOID);
  A68_MCACHE (proc_real_real_real_real) = a68_proc (M_REAL, M_REAL, M_REAL, M_REAL, NO_MOID);
  A68_MCACHE (proc_real_real_real_real_real) = a68_proc (M_REAL, M_REAL, M_REAL, M_REAL, M_REAL, NO_MOID);
  A68_MCACHE (proc_real_real_real_real_real_real) = a68_proc (M_REAL, M_REAL, M_REAL, M_REAL, M_REAL, M_REAL, NO_MOID);
  A68_MCACHE (proc_real_ref_real_ref_int_void) = a68_proc (M_VOID, M_REAL, M_REF_REAL, M_REF_INT, NO_MOID);
  A68_MCACHE (proc_void) = a68_proc (M_VOID, NO_MOID);
//
  stand_prelude ();
  stand_mp_level_2 ();
  stand_mp_level_3 ();
  stand_transput ();
  stand_extensions ();
#if (A68_LEVEL <= 2)
  stand_longlong_bits ();
#endif
#if defined (BUILD_LINUX)
  stand_linux ();
#endif
#if defined (HAVE_GSL)
  stand_gsl ();
#endif
#if defined (HAVE_MATHLIB)
  stand_mathlib ();
#endif
#if defined (HAVE_GNU_PLOTUTILS)
  stand_plot ();
#endif
#if defined (HAVE_CURSES)
  stand_curses ();
#endif
#if defined (HAVE_POSTGRESQL)
  stand_postgresql ();
#endif
}
