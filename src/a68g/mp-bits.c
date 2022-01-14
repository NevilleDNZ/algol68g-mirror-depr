//! @file mp-bits.c
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2022 J. Marcel van der Veer <algol68g@xs4all.nl>.
//
//! @section License
//
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the 
// Free Software Foundation; either version 3 of the License, or 
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
// more details. You should have received a copy of the GNU General Public 
// License along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-double.h"
#include "a68g-mp.h"
#include "a68g-numbers.h"
#include "a68g-transput.h"

#if (A68_LEVEL <= 2)

// This legacy code implements a quick-and-dirty LONG LONG BITS mode,
// constructed on top of the LONG LONG INT/REAL/COMPLEX library.
// It was essentially meant to have LONG LONG BITS for demonstration only. 
// There are obvious possibilities to improve this code, but discussions 
// suggested that workers needing long bit strings, in fields such as 
// cryptography, would be better off implementing their own optimally
// efficient tools, and investment in an efficient LONG LONG BITS library
// would not be worth the while.
// Hence in recent a68c versions, LONG BITS is a 128-bit quad word,
// and LONG LONG BITS is mapped onto LONG BITS.
//
// Below code is left in a68g for reference purposes, and in case a build of
// a version < 3 would be required.

#define MP_BITS_WIDTH(k) ((int) ceil ((k) * LOG_MP_RADIX * CONST_LOG2_10) - 1)
#define MP_BITS_WORDS(k) ((int) ceil ((REAL_T) MP_BITS_WIDTH (k) / (REAL_T) MP_BITS_BITS))

//! @brief Length in bits of mode.

int get_mp_bits_width (MOID_T * m)
{
  if (m == M_LONG_BITS) {
    return MP_BITS_WIDTH (LONG_MP_DIGITS);
  } else if (m == M_LONG_LONG_BITS) {
    return MP_BITS_WIDTH (A68_MP (varying_mp_digits));
  }
  return 0;
}

//! @brief Length in words of mode.

int get_mp_bits_words (MOID_T * m)
{
  if (m == M_LONG_BITS) {
    return MP_BITS_WORDS (LONG_MP_DIGITS);
  } else if (m == M_LONG_LONG_BITS) {
    return MP_BITS_WORDS (A68_MP (varying_mp_digits));
  }
  return 0;
}

//! @brief Convert z to a row of MP_BITS_T in the stack.

MP_BITS_T *stack_mp_bits (NODE_T * p, MP_T * z, MOID_T * m)
{
  int digits = DIGITS (m), words = get_mp_bits_words (m), k, lim;
  MP_BITS_T *row, mask;
  row = (MP_BITS_T *) STACK_ADDRESS (A68_SP);
  INCREMENT_STACK_POINTER (p, words * SIZE_ALIGNED (MP_BITS_T));
  MP_T *u = nil_mp (p, digits);
  MP_T *v = nil_mp (p, digits);
  MP_T *w = nil_mp (p, digits);
  (void) move_mp (u, z, digits);
// Argument check.
  if (MP_DIGIT (u, 1) < 0.0) {
    errno = EDOM;
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
// Convert radix MP_BITS_RADIX number.
  for (k = words - 1; k >= 0; k--) {
    (void) move_mp (w, u, digits);
    (void) over_mp_digit (p, u, u, (MP_T) MP_BITS_RADIX, digits);
    (void) mul_mp_digit (p, v, u, (MP_T) MP_BITS_RADIX, digits);
    (void) sub_mp (p, v, w, v, digits);
    row[k] = (MP_BITS_T) MP_DIGIT (v, 1);
  }
// Test on overflow: too many bits or not reduced to 0.
  mask = 0x1;
  lim = get_mp_bits_width (m) % MP_BITS_BITS;
  for (k = 1; k < lim; k++) {
    mask <<= 1;
    mask |= 0x1;
  }
  if ((row[0] & ~mask) != 0x0 || MP_DIGIT (u, 1) != 0.0) {
    errno = ERANGE;
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
// Exit.
  return row;
}

//! @brief Convert row of MP_BITS_T to LONG BITS.

MP_T *pack_mp_bits (NODE_T * p, MP_T * u, MP_BITS_T * row, MOID_T * m)
{
  int digits = DIGITS (m), words = get_mp_bits_words (m), k, lim;
  ADDR_T pop_sp = A68_SP;
// Discard excess bits.
  MP_BITS_T mask = 0x1, musk = 0x0;
  MP_T *v = nil_mp (p, digits);
  MP_T *w = nil_mp (p, digits);
  lim = get_mp_bits_width (m) % MP_BITS_BITS;
  for (k = 1; k < lim; k++) {
    mask <<= 1;
    mask |= 0x1;
  }
  row[0] &= mask;
  for (k = 1; k < (BITS_WIDTH - MP_BITS_BITS); k++) {
    musk <<= 1;
  }
  for (k = 0; k < MP_BITS_BITS; k++) {
    musk <<= 1;
    musk |= 0x1;
  }
// Convert.
  SET_MP_ZERO (u, digits);
  SET_MP_ONE (v, digits);
  for (k = words - 1; k >= 0; k--) {
    (void) mul_mp_digit (p, w, v, (MP_T) (musk & row[k]), digits);
    (void) add_mp (p, u, u, w, digits);
    if (k != 0) {
      (void) mul_mp_digit (p, v, v, (MP_T) MP_BITS_RADIX, digits);
    }
  }
  MP_STATUS (u) = (MP_T) INIT_MASK;
  A68_SP = pop_sp;
  return u;
}

//! @brief Convert multi-precision number to unsigned.

UNSIGNED_T mp_to_unsigned (NODE_T * p, MP_T * z, int digits)
{
// This routine looks a lot like "strtol". We do not use "mp_to_real" since int
// could be wider than 2 ** 52.
  int j, expo = (int) MP_EXPONENT (z);
  UNSIGNED_T sum = 0, weight = 1;
  if (expo >= digits) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MOID (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (j = 1 + expo; j >= 1; j--) {
    UNSIGNED_T term;
    if ((unsigned) MP_DIGIT (z, j) > UINT_MAX / weight) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, M_BITS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    term = (unsigned) MP_DIGIT (z, j) * weight;
    if (sum > UINT_MAX - term) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, M_BITS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    sum += term;
    weight *= MP_RADIX;
  }
  return sum;
}

//! @brief Whether LONG BITS value is in range.

void check_long_bits_value (NODE_T * p, MP_T * u, MOID_T * m)
{
  if (MP_EXPONENT (u) >= (MP_T) (DIGITS (m) - 1)) {
    ADDR_T pop_sp = A68_SP;
    (void) stack_mp_bits (p, u, m);
    A68_SP = pop_sp;
  }
}

//! @brief LONG BITS value of LONG BITS denotation

void mp_strtou (NODE_T * p, MP_T * z, char *str, MOID_T * m)
{
  int base = 0;
  char *radix = NO_TEXT;
  errno = 0;
  base = (int) a68_strtou (str, &radix, 10);
  if (radix != NO_TEXT && TO_UPPER (radix[0]) == TO_UPPER (RADIX_CHAR) && errno == 0) {
    int digits = DIGITS (m);
    ADDR_T pop_sp = A68_SP;
    char *q = radix;
    MP_T *v = nil_mp (p, digits);
    MP_T *w = nil_mp (p, digits);
    while (q[0] != NULL_CHAR) {
      q++;
    }
    SET_MP_ZERO (z, digits);
    SET_MP_ONE (w, digits);
    if (base < 2 || base > 16) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, base);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    while ((--q) != radix) {
      int digit = char_value (q[0]);
      if (digit >= 0 && digit < base) {
        (void) mul_mp_digit (p, v, w, (MP_T) digit, digits);
        (void) add_mp (p, z, z, v, digits);
      } else {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, m);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      (void) mul_mp_digit (p, w, w, (MP_T) base, digits);
    }
    check_long_bits_value (p, z, m);
    A68_SP = pop_sp;
  } else {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief Convert to other radix, binary up to hexadecimal.

BOOL_T convert_radix_mp (NODE_T * p, MP_T * u, int radix, int width, MOID_T * m, MP_T * v, MP_T * w)
{
  static char *images = "0123456789abcdef";
  if (width > 0 && (radix >= 2 && radix <= 16)) {
    MP_INT_T digit;
    int digits = DIGITS (m);
    BOOL_T success;
    (void) move_mp (w, u, digits);
    (void) over_mp_digit (p, u, u, (MP_T) radix, digits);
    (void) mul_mp_digit (p, v, u, (MP_T) radix, digits);
    (void) sub_mp (p, v, w, v, digits);
    digit = (MP_INT_T) MP_DIGIT (v, 1);
    success = convert_radix_mp (p, u, radix, width - 1, m, v, w);
    plusab_transput_buffer (p, EDIT_BUFFER, images[digit]);
    return success;
  } else {
    return (BOOL_T) (MP_DIGIT (u, 1) == 0);
  }
}

//! @brief OP LENG = (BITS) LONG BITS

void genie_lengthen_unsigned_to_mp (NODE_T * p)
{
  int digits = DIGITS (M_LONG_INT);
  A68_BITS k;
  POP_OBJECT (p, &k, A68_BITS);
  MP_T *z = nil_mp (p, digits);
  (void) unsigned_to_mp (p, z, (UNSIGNED_T) VALUE (&k), digits);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP BIN = (LONG INT) LONG BITS

void genie_bin_mp (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int size = SIZE (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *u = (MP_T *) STACK_OFFSET (-size);
// We convert just for the operand check.
  (void) stack_mp_bits (p, u, mode);
  MP_STATUS (u) = (MP_T) INIT_MASK;
  A68_SP = pop_sp;
}

//! @brief OP NOT = (LONG BITS) LONG BITS

void genie_not_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int size = SIZE (mode);
  ADDR_T pop_sp = A68_SP;
  int k, words = get_mp_bits_words (mode);
  MP_T *u = (MP_T *) STACK_OFFSET (-size);
  MP_BITS_T *row = stack_mp_bits (p, u, mode);
  for (k = 0; k < words; k++) {
    row[k] = ~row[k];
  }
  (void) pack_mp_bits (p, u, row, mode);
  A68_SP = pop_sp;
}

//! @brief OP SHORTEN = (LONG BITS) BITS

void genie_shorten_mp_to_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = DIGITS (mode), size = SIZE (mode);
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_VALUE (p, mp_to_unsigned (p, z, digits), A68_BITS);
}

//! @brief Get bit from LONG BITS.

unsigned elem_long_bits (NODE_T * p, ADDR_T k, MP_T * z, MOID_T * m)
{
  int n;
  ADDR_T pop_sp = A68_SP;
  MP_BITS_T *words = stack_mp_bits (p, z, m), mask = 0x1;
  k += (MP_BITS_BITS - get_mp_bits_width (m) % MP_BITS_BITS - 1);
  for (n = 0; n < MP_BITS_BITS - k % MP_BITS_BITS - 1; n++) {
    mask = mask << 1;
  }
  A68_SP = pop_sp;
  return (words[k / MP_BITS_BITS]) & mask;
}

//! @brief OP ELEM = (INT, LONG BITS) BOOL

void genie_elem_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  MP_BITS_T w;
  int bits = get_mp_bits_width (M_LONG_BITS), size = SIZE (M_LONG_BITS);
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + SIZE (M_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, M_INT);
  w = elem_long_bits (p, VALUE (i), z, M_LONG_BITS);
  DECREMENT_STACK_POINTER (p, size + SIZE (M_INT));
  PUSH_VALUE (p, (BOOL_T) (w != 0), A68_BOOL);
}

//! @brief OP ELEM = (INT, LONG LONG BITS) BOOL

void genie_elem_long_mp_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  MP_BITS_T w;
  int bits = get_mp_bits_width (M_LONG_LONG_BITS), size = SIZE (M_LONG_LONG_BITS);
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + SIZE (M_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, M_INT);
  w = elem_long_bits (p, VALUE (i), z, M_LONG_LONG_BITS);
  DECREMENT_STACK_POINTER (p, size + SIZE (M_INT));
  PUSH_VALUE (p, (BOOL_T) (w != 0), A68_BOOL);
}

//! @brief Set bit in LONG BITS.

static MP_BITS_T *set_long_bits (NODE_T * p, int k, MP_T * z, MOID_T * m, MP_BITS_T bit)
{
  int n;
  MP_BITS_T *words = stack_mp_bits (p, z, m), mask = 0x1;
  k += (MP_BITS_BITS - get_mp_bits_width (m) % MP_BITS_BITS - 1);
  for (n = 0; n < MP_BITS_BITS - k % MP_BITS_BITS - 1; n++) {
    mask = mask << 1;
  }
  if (bit == 0x1) {
    words[k / MP_BITS_BITS] = (words[k / MP_BITS_BITS]) | mask;
  } else {
    words[k / MP_BITS_BITS] = (words[k / MP_BITS_BITS]) & (~mask);
  }
  return words;
}

//! @brief OP SET = (INT, LONG BITS) VOID

void genie_set_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  MP_BITS_T *w;
  ADDR_T pop_sp = A68_SP;
  int bits = get_mp_bits_width (M_LONG_BITS), size = SIZE (M_LONG_BITS);
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + SIZE (M_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, M_INT);
  w = set_long_bits (p, VALUE (i), z, M_LONG_BITS, 0x1);
  (void) pack_mp_bits (p, (MP_T *) STACK_ADDRESS (pop_sp - size - SIZE (M_INT)), w, M_LONG_BITS);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, SIZE (M_INT));
}

//! @brief OP SET = (INT, LONG LONG BITS) BOOL

void genie_set_long_mp_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  MP_BITS_T *w;
  ADDR_T pop_sp = A68_SP;
  int bits = get_mp_bits_width (M_LONG_LONG_BITS), size = SIZE (M_LONG_LONG_BITS);
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + SIZE (M_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, M_INT);
  w = set_long_bits (p, VALUE (i), z, M_LONG_LONG_BITS, 0x1);
  (void) pack_mp_bits (p, (MP_T *) STACK_ADDRESS (pop_sp - size - SIZE (M_INT)), w, M_LONG_LONG_BITS);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, SIZE (M_INT));
}

//! @brief OP CLEAR = (INT, LONG BITS) BOOL

void genie_clear_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  MP_BITS_T *w;
  ADDR_T pop_sp = A68_SP;
  int bits = get_mp_bits_width (M_LONG_BITS), size = SIZE (M_LONG_BITS);
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + SIZE (M_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, M_INT);
  w = set_long_bits (p, VALUE (i), z, M_LONG_BITS, 0x0);
  (void) pack_mp_bits (p, (MP_T *) STACK_ADDRESS (pop_sp - size - SIZE (M_INT)), w, M_LONG_BITS);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, SIZE (M_INT));
}

//! @brief OP CLEAR = (INT, LONG LONG BITS) BOOL

void genie_clear_long_mp_bits (NODE_T * p)
{
  A68_INT *i;
  MP_T *z;
  MP_BITS_T *w;
  ADDR_T pop_sp = A68_SP;
  int bits = get_mp_bits_width (M_LONG_LONG_BITS), size = SIZE (M_LONG_LONG_BITS);
  z = (MP_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + SIZE (M_INT)));
  PRELUDE_ERROR (VALUE (i) < 1 || VALUE (i) > bits, p, ERROR_OUT_OF_BOUNDS, M_INT);
  w = set_long_bits (p, VALUE (i), z, M_LONG_LONG_BITS, 0x0);
  (void) pack_mp_bits (p, (MP_T *) STACK_ADDRESS (pop_sp - size - SIZE (M_INT)), w, M_LONG_LONG_BITS);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, SIZE (M_INT));
}

//! @brief PROC long bits pack = ([] BOOL) LONG BITS

void genie_long_bits_pack (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  A68_REF z;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base;
  int size, k, bits, digits;
  ADDR_T pop_sp;
  POP_REF (p, &z);
  CHECK_REF (p, z, M_ROW_BOOL);
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  bits = get_mp_bits_width (mode);
  digits = DIGITS (mode);
  PRELUDE_ERROR (size < 0 || size > bits, p, ERROR_OUT_OF_BOUNDS, M_ROW_BOOL);
// Convert so that LWB goes to MSB, so ELEM gives same order as [] BOOL.
  MP_T *sum = nil_mp (p, digits);
  pop_sp = A68_SP;
  MP_T *fact = lit_mp (p, 1, 0, digits);
  if (ROW_SIZE (tup) > 0) {
    base = DEREF (BYTE_T, &ARRAY (arr));
    for (k = UPB (tup); k >= LWB (tup); k--) {
      int addr = INDEX_1_DIM (arr, tup, k);
      A68_BOOL *boo = (A68_BOOL *) & (base[addr]);
      CHECK_INIT (p, INITIALISED (boo), M_BOOL);
      if (VALUE (boo)) {
        (void) add_mp (p, sum, sum, fact, digits);
      }
      (void) mul_mp_digit (p, fact, fact, (MP_T) 2, digits);
    }
  }
  A68_SP = pop_sp;
  MP_STATUS (sum) = (MP_T) INIT_MASK;
}

//! @brief OP SHL = (LONG BITS, INT) LONG BITS

void genie_shl_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int i, k, size = SIZE (mode), words = get_mp_bits_words (mode);
  MP_T *u;
  MP_BITS_T *row_u;
  ADDR_T pop_sp;
  A68_INT j;
// Pop number of bits.
  POP_OBJECT (p, &j, A68_INT);
  u = (MP_T *) STACK_OFFSET (-size);
  pop_sp = A68_SP;
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
        row_u[k] &= ~((MP_BITS_T) MP_BITS_RADIX);
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
  A68_SP = pop_sp;
}

//! @brief OP SHR = (LONG BITS, INT) LONG BITS

void genie_shr_mp (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  (void) genie_shl_mp (p);      // Conform RR
}

//! @brief OP <= = (LONG BITS, LONG BITS) BOOL

void genie_le_long_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = SIZE (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = A68_SP;
  BOOL_T result = A68_TRUE;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  MP_BITS_T *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; (k < words) && result; k++) {
    result = (BOOL_T) (result & ((row_u[k] | row_v[k]) == row_v[k]));
  }
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_VALUE (p, (BOOL_T) (result ? A68_TRUE : A68_FALSE), A68_BOOL);
}

//! @brief OP >= = (LONG BITS, LONG BITS) BOOL

void genie_ge_long_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = SIZE (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = A68_SP;
  BOOL_T result = A68_TRUE;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  MP_BITS_T *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; (k < words) && result; k++) {
    result = (BOOL_T) (result & ((row_u[k] | row_v[k]) == row_u[k]));
  }
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_VALUE (p, (BOOL_T) (result ? A68_TRUE : A68_FALSE), A68_BOOL);
}

//! @brief OP AND = (LONG BITS, LONG BITS) LONG BITS

void genie_and_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = SIZE (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  MP_BITS_T *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] &= row_v[k];
  }
  (void) pack_mp_bits (p, u, row_u, mode);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP OR = (LONG BITS, LONG BITS) LONG BITS

void genie_or_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = SIZE (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  MP_BITS_T *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] |= row_v[k];
  }
  (void) pack_mp_bits (p, u, row_u, mode);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP XOR = (LONG BITS, LONG BITS) LONG BITS

void genie_xor_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = SIZE (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *u = (MP_T *) STACK_OFFSET (-2 * size), *v = (MP_T *) STACK_OFFSET (-size);
  MP_BITS_T *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] ^= row_v[k];
  }
  (void) pack_mp_bits (p, u, row_u, mode);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief LONG BITS long max bits

void genie_long_max_bits (NODE_T * p)
{
  int digits = DIGITS (M_LONG_BITS);
  int width = get_mp_bits_width (M_LONG_BITS);
  ADDR_T pop_sp;
  MP_T *z = nil_mp (p, digits);
  pop_sp = A68_SP;
  (void) set_mp (z, (MP_T) 2, 0, digits);
  (void) pow_mp_int (p, z, z, width, digits);
  (void) minus_one_mp (p, z, z, digits);
  A68_SP = pop_sp;
}

//! @brief LONG LONG BITS long long max bits

void genie_long_mp_max_bits (NODE_T * p)
{
  int digits = DIGITS (M_LONG_LONG_BITS);
  int width = get_mp_bits_width (M_LONG_LONG_BITS);
  MP_T *z = nil_mp (p, digits);
  ADDR_T pop_sp = A68_SP;
  (void) set_mp (z, (MP_T) 2, 0, digits);
  (void) pow_mp_int (p, z, z, width, digits);
  (void) minus_one_mp (p, z, z, digits);
  A68_SP = pop_sp;
}

//! @brief Lengthen LONG BITS to [] BOOL.

void genie_lengthen_long_bits_to_row_bool (NODE_T * p)
{
  MOID_T *m = MOID (SUB (p));
  A68_REF z, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  int size = SIZE (m), k, width = get_mp_bits_width (m), words = get_mp_bits_words (m);
  MP_BITS_T *bits;
  BYTE_T *base;
  MP_T *x;
  ADDR_T pop_sp = A68_SP;
// Calculate and convert BITS value.
  x = (MP_T *) STACK_OFFSET (-size);
  bits = stack_mp_bits (p, x, m);
// Make [] BOOL.
  NEW_ROW_1D (z, row, arr, tup, M_ROW_BOOL, M_BOOL, width);
  PUT_DESCRIPTOR (arr, tup, &z);
  base = ADDRESS (&row) + (width - 1) * SIZE (M_BOOL);
  k = width;
  while (k > 0) {
    MP_BITS_T bit = 0x1;
    int j;
    for (j = 0; j < MP_BITS_BITS && k >= 0; j++) {
      STATUS ((A68_BOOL *) base) = INIT_MASK;
      VALUE ((A68_BOOL *) base) = (BOOL_T) ((bits[words - 1] & bit) ? A68_TRUE : A68_FALSE);
      base -= SIZE (M_BOOL);
      bit <<= 1;
      k--;
    }
    words--;
  }
  A68_SP = pop_sp;
  PUSH_REF (p, z);
}

#endif
