//! @file mp.c
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

// Multiprecision calculations are useful in these cases:
//
// Ill-conditioned linear systems
// Summation of large series
// Long-time or large-scale simulations
// Small-scale phenomena
// 'Experimental mathematics'
//
// The routines in this library follow algorithms as described in the
// literature, notably
// 
//   D.M. Smith, "Efficient Multiple-Precision Evaluation of Elementary Functions"
//   Mathematics of Computation 52 (1989) 131-134
// 
//   D.M. Smith, "A Multiple-Precision Division Algorithm"
//   Mathematics of Computation 66 (1996) 157-163
//
//   The GNU MPFR library documentation
// 
// Multiprecision libraries are (freely) available, but this one is particularly 
// designed to work with Algol68G. It implements following modes:
// 
//    LONG INT, LONG REAL, LONG COMPLEX, LONG BITS
//    LONG LONG INT, LONG LONG REAL, LONG LONG COMPLEX, LONG LONG BITS
//
// Note that recent implementations of GCC make available 64-bit LONG INT and 
// 128-bit LONG REAL. This suits many multiprecision needs already. 
// On such platforms, below code is used for LONG LONG modes only.
// Now that 64-bit integers are commonplace, this library has been adapted to
// exploit them. Having some more digits per word gives performance gain.
// This is implemented through macros to keep the library compatible with
// old platforms with 32-bit integers and 64-bit doubles.
// 
// Currently, LONG modes have a fixed precision, and LONG LONG modes have
// user-definable precision. Precisions span about 30 decimal digits for
// LONG modes up to (default) about 60 decimal digits for LONG LONG modes, a
// range that is thought to be adequate for most multiprecision applications.
//
// Although the maximum length of a number is in principle unbound, this 
// implementation is not designed for more than a few hundred decimal places. 
// At higher precisions, expect a performance penalty with respect to
// state of the art implementations that may for instance use convolution for
// multiplication. 
// 
// This library takes a sloppy approach towards LONG INT and LONG BITS which are
// implemented as LONG REAL and truncated where appropriate. This keeps the code
// short at the penalty of some performance loss.
// 
// As is common practice, mp numbers are represented by a row of digits
// in a large base. Layout of a mp number "z" is:
// 
//    MP_T *z;
//
//    MP_STATUS (z)        Status word
//    MP_EXPONENT (z)      Exponent with base MP_RADIX
//    MP_DIGIT (z, 1 .. N) Digits 1 .. N
// 
// Note that this library assumes IEEE 754 compatible implementation of
// type "double". It also assumes a 32- (or 64-) bit type "int".
// 
// Most legacy multiple precision libraries stored numbers as [] int*4.
// However, since division and multiplication are O(N ** 2) operations, it is
// advantageous to keep the base as high as possible. Modern computers handle
// doubles at similar or better speed as integers, therefore this library
// opts for storing numbers as [] words were a word is real*8 (legacy) or 
// int*8 (on f.i. ix86 processors that have real*10), trading space for speed.
// 
// Set a base such that "base^2" can be exactly represented by a word.
// To facilitate transput, we require a base that is a power of 10.
// 
// If we choose the base right then in multiplication and division we do not need
// to normalise intermediate results at each step since a number of additions
// can be made before overflow occurs. That is why we specify "MAX_REPR_INT".
// 
// Mind that the precision of a mp number is at worst just
// (LONG_MP_DIGITS - 1) * LOG_MP_RADIX + 1, since the most significant mp digit
// is also in range [0 .. MP_RADIX>. Do not specify less than 2 digits.
// 
// Since this software is distributed without any warranty, it is your
// responsibility to validate the behaviour of the routines and their accuracy
// using the source code provided. See the GNU General Public License for details.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-double.h"
#include "a68g-mp.h"

// Internal mp constants.

//! @brief Set number of digits for long long numbers.

void set_long_mp_digits (int n)
{
  A68_MP (varying_mp_digits) = n;
}

//! @brief Convert precision to digits for long long number.

int width_to_mp_digits (int n)
{
  return (int) ceil ((REAL_T) n / (REAL_T) LOG_MP_RADIX);
}

//! @brief Unformatted write of z to stdout; debugging routine.

#if defined (A68_DEBUG)
#if !defined (BUILD_WIN32)

void raw_write_mp (char *str, MP_T * z, int digs)
{
  int i;
  fprintf (stdout, "\n(%d digits)%s", digs, str);
  for (i = 1; i <= digs; i++) {
#if (A68_LEVEL >= 3)
    fprintf (stdout, " %09lld", (MP_INT_T) MP_DIGIT (z, i));
#else
    fprintf (stdout, " %07d", (MP_INT_T) MP_DIGIT (z, i));
#endif
  }
  fprintf (stdout, " E" A68_LD, (MP_INT_T) MP_EXPONENT (z));
  fprintf (stdout, " S" A68_LD, (MP_INT_T) MP_STATUS (z));
  fprintf (stdout, "\n");
  ASSERT (fflush (stdout) == 0);
}

#endif
#endif

//! @brief Whether z is a valid representation for its mode.

BOOL_T check_mp_int (MP_T * z, MOID_T * m)
{
  if (m == M_LONG_INT || m == M_LONG_BITS) {
    return (BOOL_T) ((MP_EXPONENT (z) >= (MP_T) 0) && (MP_EXPONENT (z) < (MP_T) LONG_MP_DIGITS));
  } else if (m == M_LONG_LONG_INT || m == M_LONG_LONG_BITS) {
    return (BOOL_T) ((MP_EXPONENT (z) >= (MP_T) 0) && (MP_EXPONENT (z) < (MP_T) A68_MP (varying_mp_digits)));
  }
  return A68_FALSE;
}

//! @brief |x|

MP_T *abs_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  (void) p;
  if (x != z) {
    (void) move_mp (z, x, digs);
  }
  MP_DIGIT (z, 1) = ABS (MP_DIGIT (z, 1));
  MP_STATUS (z) = (MP_T) INIT_MASK;
  return z;
}

//! @brief -x

MP_T *minus_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  (void) p;
  if (x != z) {
    (void) move_mp (z, x, digs);
  }
  MP_DIGIT (z, 1) = -(MP_DIGIT (z, 1));
  MP_STATUS (z) = (MP_T) INIT_MASK;
  return z;
}

//! @brief 1 - x

MP_T *one_minus_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  (void) sub_mp (p, z, mp_one (digs), x, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief x - 1

MP_T *minus_one_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  (void) sub_mp (p, z, x, mp_one (digs), digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief x + 1

MP_T *plus_one_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  (void) add_mp (p, z, x, mp_one (digs), digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief Test whether x = y.

BOOL_T same_mp (NODE_T * p, MP_T * x, MP_T * y, int digs)
{
  int k;
  (void) p;
  if ((MP_STATUS (x) == MP_STATUS (y)) && (MP_EXPONENT (x) == MP_EXPONENT (y))) {
    for (k = digs; k >= 1; k--) {
      if (MP_DIGIT (x, k) != MP_DIGIT (y, k)) {
        return A68_FALSE;
      }
    }
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Align 10-base z in a MP_RADIX mantissa.

MP_T *align_mp (MP_T * z, INT_T * expo, int digs)
{
  INT_T shift;
  if (*expo >= 0) {
    shift = LOG_MP_RADIX - (*expo) % LOG_MP_RADIX - 1;
    (*expo) /= LOG_MP_RADIX;
  } else {
    shift = (-(*expo) - 1) % LOG_MP_RADIX;
    (*expo) = ((*expo) + 1) / LOG_MP_RADIX;
    (*expo)--;
  }
// Optimising below code does not make the library noticeably faster.
  for (INT_T i = 1; i <= shift; i++) {
    INT_T carry = 0;
    for (INT_T j = 1; j <= digs; j++) {
      MP_INT_T k = ((MP_INT_T) MP_DIGIT (z, j)) % 10;
      MP_DIGIT (z, j) = (MP_T) ((MP_INT_T) (MP_DIGIT (z, j) / 10) + carry * (MP_RADIX / 10));
      carry = k;
    }
  }
  return z;
}

//! @brief Transform string into multi-precision number.

MP_T *strtomp (NODE_T * p, MP_T * z, char *s, int digs)
{
  BOOL_T ok = A68_TRUE;
  errno = 0;
  SET_MP_ZERO (z, digs);
  while (IS_SPACE (s[0])) {
    s++;
  }
// Get the sign.
  int sign = (s[0] == '-' ? -1 : 1);
  if (s[0] == '+' || s[0] == '-') {
    s++;
  }
// Scan mantissa digs and put them into "z".
  while (s[0] == '0') {
    s++;
  }
  int i = 0, dig = 1;
  INT_T sum = 0, dot = -1, one = -1, pow = 0, W = MP_RADIX / 10;
  while (s[i] != NULL_CHAR && dig <= digs && (IS_DIGIT (s[i]) || s[i] == POINT_CHAR)) {
    if (s[i] == POINT_CHAR) {
      dot = i;
    } else {
      int value = (int) s[i] - (int) '0';
      if (one < 0 && value > 0) {
        one = pow;
      }
      sum += W * value;
      if (one >= 0) {
        W /= 10;
      }
      pow++;
      if (W < 1) {
        MP_DIGIT (z, dig++) = (MP_T) sum;
        sum = 0;
        W = MP_RADIX / 10;
      }
    }
    i++;
  }
// Store the last digs.
  if (dig <= digs) {
    MP_DIGIT (z, dig++) = (MP_T) sum;
  }
// See if there is an exponent.
  INT_T expo;
  if (s[i] != NULL_CHAR && TO_UPPER (s[i]) == TO_UPPER (EXPONENT_CHAR)) {
    char *end;
    expo = (int) strtol (&(s[++i]), &end, 10);
    ok = (BOOL_T) (end[0] == NULL_CHAR);
  } else {
    expo = 0;
    ok = (BOOL_T) (s[i] == NULL_CHAR);
  }
// Calculate effective exponent.
  if (dot >= 0) {
    if (one > dot) {
      expo -= one - dot + 1;
    } else {
      expo += dot - 1;
    }
  } else {
    expo += pow - 1;
  }
  (void) align_mp (z, &expo, digs);
  MP_EXPONENT (z) = (MP_DIGIT (z, 1) == 0 ? 0 : (MP_T) expo);
  MP_DIGIT (z, 1) *= sign;
  check_mp_exp (p, z);
  if (errno == 0 && ok) {
    return z;
  } else {
    return NaN_MP;
  }
}

//! @brief Convert integer to multi-precison number.

MP_T *int_to_mp (NODE_T * p, MP_T * z, INT_T k, int digs)
{
  int sign_k = 1;
  if (k < 0) {
    k = -k;
    sign_k = -1;
  }
  int m = k, n = 0;
  while ((m /= MP_RADIX) != 0) {
    n++;
  }
  set_mp (z, 0, n, digs);
  int j;
  for (j = 1 + n; j >= 1; j--) {
    MP_DIGIT (z, j) = (MP_T) (k % MP_RADIX);
    k /= MP_RADIX;
  }
  MP_DIGIT (z, 1) = sign_k * MP_DIGIT (z, 1);
  check_mp_exp (p, z);
  return z;
}

//! @brief Convert unt to multi-precison number.

MP_T *unt_to_mp (NODE_T * p, MP_T * z, UNSIGNED_T k, int digs)
{
  int m = k, n = 0;
  while ((m /= MP_RADIX) != 0) {
    n++;
  }
  set_mp (z, 0, n, digs);
  int j;
  for (j = 1 + n; j >= 1; j--) {
    MP_DIGIT (z, j) = (MP_T) (k % MP_RADIX);
    k /= MP_RADIX;
  }
  check_mp_exp (p, z);
  return z;
}

//! @brief Convert multi-precision number to integer.

INT_T mp_to_int (NODE_T * p, MP_T * z, int digs)
{
// This routines looks a lot like "strtol". 
  INT_T expo = (int) MP_EXPONENT (z), sum = 0, weight = 1;
  if (expo >= digs) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MOID (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  BOOL_T negative = (BOOL_T) (MP_DIGIT (z, 1) < 0);
  if (negative) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  int j;
  for (j = 1 + expo; j >= 1; j--) {
    if ((MP_INT_T) MP_DIGIT (z, j) > A68_MAX_INT / weight) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, M_INT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    INT_T term = (MP_INT_T) MP_DIGIT (z, j) * weight;
    if (sum > A68_MAX_INT - term) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, M_INT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    sum += term;
    weight *= MP_RADIX;
  }
  return negative ? -sum : sum;
}

//! @brief Convert REAL_T to multi-precison number.

MP_T *real_to_mp (NODE_T * p, MP_T * z, REAL_T x, int digs)
{
  SET_MP_ZERO (z, digs);
  if (x == 0.0) {
    return z;
  }
// Small integers can be done better by int_to_mp.
  if (ABS (x) < MP_RADIX && trunc (x) == x) {
    return int_to_mp (p, z, (INT_T) trunc (x), digs);
  }
  int sign_x = SIGN (x);
// Scale to [0, 0.1>.
  REAL_T a = ABS (x);
  INT_T expo = (int) log10 (a);
  a /= ten_up (expo);
  expo--;
  if (a >= 1) {
    a /= 10;
    expo++;
  }
// Transport digs of x to the mantissa of z.
  INT_T sum = 0, weight = (MP_RADIX / 10);
  int j = 1, k;
  for (k = 0; a != 0.0 && j <= digs && k < REAL_DIGITS; k++) {
    REAL_T u = a * 10;
    REAL_T v = floor (u);
    a = u - v;
    sum += weight * (INT_T) v;
    weight /= 10;
    if (weight < 1) {
      MP_DIGIT (z, j++) = (MP_T) sum;
      sum = 0;
      weight = (MP_RADIX / 10);
    }
  }
// Store the last digs.
  if (j <= digs) {
    MP_DIGIT (z, j) = (MP_T) sum;
  }
  (void) align_mp (z, &expo, digs);
  MP_EXPONENT (z) = (MP_T) expo;
  MP_DIGIT (z, 1) *= sign_x;
  check_mp_exp (p, z);
  return z;
}

//! @brief Convert multi-precision number to real.

REAL_T mp_to_real (NODE_T * p, MP_T * z, int digs)
{
// This routine looks a lot like "strtod".
  (void) p;
  if (MP_EXPONENT (z) * (MP_T) LOG_MP_RADIX <= (MP_T) REAL_MIN_10_EXP) {
    return 0;
  } else {
    REAL_T sum = 0, weight = ten_up ((int) (MP_EXPONENT (z) * LOG_MP_RADIX));
    int j;
    for (j = 1; j <= digs && (j - 2) * LOG_MP_RADIX <= REAL_DIG; j++) {
      sum += ABS (MP_DIGIT (z, j)) * weight;
      weight /= MP_RADIX;
    }
    CHECK_REAL (p, sum);
    return MP_DIGIT (z, 1) >= 0 ? sum : -sum;
  }
}

//! @brief Normalise positive intermediate, fast.

static inline void norm_mp_light (MP_T * w, int k, int digs)
{
// Bring every digit back to [0 .. MP_RADIX>.
  MP_T *z = &MP_DIGIT (w, digs);
  int j;
  for (j = digs; j >= k; j--, z--) {
    if (z[0] >= MP_RADIX) {
      z[0] -= (MP_T) MP_RADIX;
      z[-1] += 1;
    } else if (z[0] < 0) {
      z[0] += (MP_T) MP_RADIX;
      z[-1] -= 1;
    }
  }
}

//! @brief Normalise positive intermediate.

static inline void norm_mp (MP_T * w, int k, int digs)
{
// Bring every digit back to [0 .. MP_RADIX>.
  int j;
  MP_T *z;
  for (j = digs, z = &MP_DIGIT (w, digs); j >= k; j--, z--) {
    if (z[0] >= (MP_T) MP_RADIX) {
      MP_T carry = (MP_T) ((MP_INT_T) (z[0] / (MP_T) MP_RADIX));
      z[0] -= carry * (MP_T) MP_RADIX;
      z[-1] += carry;
    } else if (z[0] < (MP_T) 0) {
      MP_T carry = (MP_T) 1 + (MP_T) ((MP_INT_T) ((-z[0] - 1) / (MP_T) MP_RADIX));
      z[0] += carry * (MP_T) MP_RADIX;
      z[-1] -= carry;
    }
  }
}

//! @brief Round multi-precision number.

static inline void round_internal_mp (MP_T * z, MP_T * w, int digs)
{
// Assume that w has precision of at least 2 + digs.
  int last = (MP_DIGIT (w, 1) == 0 ? 2 + digs : 1 + digs);
  if (MP_DIGIT (w, last) >= MP_RADIX / 2) {
    MP_DIGIT (w, last - 1) += 1;
  }
  if (MP_DIGIT (w, last - 1) >= MP_RADIX) {
    norm_mp (w, 2, last); // Hardly ever happens - no need to optimise
  }
  if (MP_DIGIT (w, 1) == 0) {
    (void) move_mp_part (&MP_DIGIT (z, 1), &MP_DIGIT (w, 2), digs);
    MP_EXPONENT (z) = MP_EXPONENT (w) - 1;
  } else if (z != w) {
    (void) move_mp_part (&MP_EXPONENT (z), &MP_EXPONENT (w), (1 + digs));
  }
// Zero is zero is zero.
  if (MP_DIGIT (z, 1) == 0) {
    MP_EXPONENT (z) = (MP_T) 0;
  }
}

//! @brief Truncate at decimal point.

MP_T *trunc_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  if (MP_EXPONENT (x) < 0) {
    SET_MP_ZERO (z, digs);
  } else if (MP_EXPONENT (x) >= (MP_T) digs) {
    errno = EDOM;
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, (IS (MOID (p), PROC_SYMBOL) ? SUB_MOID (p) : MOID (p)));
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    int k;
    (void) move_mp (z, x, digs);
    for (k = (int) (MP_EXPONENT (x) + 2); k <= digs; k++) {
      MP_DIGIT (z, k) = (MP_T) 0;
    }
  }
  return z;
}

//! @brief Floor - largest integer smaller than x.

MP_T *floor_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  if (MP_EXPONENT (x) < 0) {
    SET_MP_ZERO (z, digs);
  } else if (MP_EXPONENT (x) >= (MP_T) digs) {
    errno = EDOM;
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, (IS (MOID (p), PROC_SYMBOL) ? SUB_MOID (p) : MOID (p)));
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    int k;
    (void) move_mp (z, x, digs);
    for (k = (int) (MP_EXPONENT (x) + 2); k <= digs; k++) {
      MP_DIGIT (z, k) = (MP_T) 0;
    }
  }
  if (MP_DIGIT (x, 1) < 0 && ! same_mp (p, z, x, digs)) {
    (void) minus_one_mp (p, z, z, digs);
  }
  return z;
}

BOOL_T is_int_mp (NODE_T *p, MP_T *z, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *y = nil_mp (p, digs);
  trunc_mp (p, y, z, digs);
  BOOL_T tst = same_mp (p, y, z, digs);
  A68_SP = pop_sp;
  return tst;
}

//! @brief Shorten and round.

MP_T *shorten_mp (NODE_T * p, MP_T * z, int digs, MP_T * x, int digs_x)
{
  if (digs > digs_x) {
    return lengthen_mp (p, z, digs, x, digs_x);
  } else if (digs == digs_x) {
    return move_mp (z, x, digs);
  } else {
// Reserve extra digs for proper rounding.
    ADDR_T pop_sp = A68_SP;
    int digs_h = digs + 2;
    BOOL_T negative = (BOOL_T) (MP_DIGIT (x, 1) < 0);
    MP_T *w = nil_mp (p, digs_h);
    if (negative) {
      MP_DIGIT (x, 1) = -MP_DIGIT (x, 1);
    }
    MP_STATUS (w) = (MP_T) 0;
    MP_EXPONENT (w) = MP_EXPONENT (x) + 1;
    MP_DIGIT (w, 1) = (MP_T) 0;
    (void) move_mp_part (&MP_DIGIT (w, 2), &MP_DIGIT (x, 1), digs + 1);
    round_internal_mp (z, w, digs);
    if (negative) {
      MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
    }
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief Lengthen x and assign to z.

MP_T *lengthen_mp (NODE_T * p, MP_T * z, int digs_z, MP_T * x, int digs_x)
{
  if (digs_z < digs_x) {
    return shorten_mp (p, z, digs_z, x, digs_x);
  } else if (digs_z == digs_x) {
    return move_mp (z, x, digs_z);
  } else {
    if (z != x) {
      (void) move_mp_part (&MP_DIGIT (z, 1), &MP_DIGIT (x, 1), digs_x);
      MP_EXPONENT (z) = MP_EXPONENT (x);
      MP_STATUS (z) = MP_STATUS (x);
    }
    int j;
    for (j = 1 + digs_x; j <= digs_z; j++) {
      MP_DIGIT (z, j) = (MP_T) 0;
    }
  }
  return z;
}

//! @brief Set "z" to the sum of positive "x" and positive "y".

MP_T *add_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
  MP_STATUS (z) = (MP_T) INIT_MASK;
// Trivial cases.
  if (MP_DIGIT (x, 1) == (MP_T) 0) {
    (void) move_mp (z, y, digs);
    return z;
  } else if (MP_DIGIT (y, 1) == 0) {
    (void) move_mp (z, x, digs);
    return z;
  }
// We want positive arguments.
  ADDR_T pop_sp = A68_SP;
  MP_T x_1 = MP_DIGIT (x, 1), y_1 = MP_DIGIT (y, 1);
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_DIGIT (y, 1) = ABS (y_1);
  if (x_1 >= 0 && y_1 < 0) {
    (void) sub_mp (p, z, x, y, digs);
  } else if (x_1 < 0 && y_1 >= 0) {
    (void) sub_mp (p, z, y, x, digs);
  } else if (x_1 < 0 && y_1 < 0) {
    (void) add_mp (p, z, x, y, digs);
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  } else {
// Add.
    int digs_h = 2 + digs;
    MP_T *w = nil_mp (p, digs_h);
    if (MP_EXPONENT (x) == MP_EXPONENT (y)) {
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (x);
      int j;
      for (j = 1; j <= digs; j++) {
        MP_DIGIT (w, j + 1) = MP_DIGIT (x, j) + MP_DIGIT (y, j);
      }
      MP_DIGIT (w, digs_h) = (MP_T) 0;
    } else if (MP_EXPONENT (x) > MP_EXPONENT (y)) {
      int j, shl_y = (int) MP_EXPONENT (x) - (int) MP_EXPONENT (y);
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (x);
      for (j = 1; j < digs_h; j++) {
        int i_y = j - shl_y;
        MP_T x_j = (j > digs ? 0 : MP_DIGIT (x, j));
        MP_T y_j = (i_y <= 0 || i_y > digs ? 0 : MP_DIGIT (y, i_y));
        MP_DIGIT (w, j + 1) = x_j + y_j;
      }
    } else {
      int j, shl_x = (int) MP_EXPONENT (y) - (int) MP_EXPONENT (x);
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (y);
      for (j = 1; j < digs_h; j++) {
        int i_x = j - shl_x;
        MP_T x_j = (i_x <= 0 || i_x > digs ? 0 : MP_DIGIT (x, i_x));
        MP_T y_j = (j > digs ? 0 : MP_DIGIT (y, j));
        MP_DIGIT (w, j + 1) = x_j + y_j;
      }
    }
    norm_mp_light (w, 2, digs_h);
    round_internal_mp (z, w, digs);
    check_mp_exp (p, z);
  }
// Restore and exit.
  A68_SP = pop_sp;
  MP_T z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (y, 1) = y_1;
  MP_DIGIT (z, 1) = z_1;        // In case z IS x OR z IS y
  return z;
}

//! @brief Set "z" to the difference of positive "x" and positive "y".

MP_T *sub_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
  MP_STATUS (z) = (MP_T) INIT_MASK;
// Trivial cases.
  if (MP_DIGIT (x, 1) == (MP_T) 0) {
    (void) move_mp (z, y, digs);
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
    return z;
  } else if (MP_DIGIT (y, 1) == (MP_T) 0) {
    (void) move_mp (z, x, digs);
    return z;
  }
// We want positive arguments.
  ADDR_T pop_sp = A68_SP;
  MP_T x_1 = MP_DIGIT (x, 1), y_1 = MP_DIGIT (y, 1);
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_DIGIT (y, 1) = ABS (y_1);
  if (x_1 >= 0 && y_1 < 0) {
    (void) add_mp (p, z, x, y, digs);
  } else if (x_1 < 0 && y_1 >= 0) {
    (void) add_mp (p, z, y, x, digs);
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  } else if (x_1 < 0 && y_1 < 0) {
    (void) sub_mp (p, z, y, x, digs);
  } else {
// Subtract.
    BOOL_T negative = A68_FALSE;
    int j, fnz, digs_h = 2 + digs;
    MP_T *w = nil_mp (p, digs_h);
    if (MP_EXPONENT (x) == MP_EXPONENT (y)) {
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (x);
      for (j = 1; j <= digs; j++) {
        MP_DIGIT (w, j + 1) = MP_DIGIT (x, j) - MP_DIGIT (y, j);
      }
      MP_DIGIT (w, digs_h) = (MP_T) 0;
    } else if (MP_EXPONENT (x) > MP_EXPONENT (y)) {
      int shl_y = (int) MP_EXPONENT (x) - (int) MP_EXPONENT (y);
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (x);
      for (j = 1; j < digs_h; j++) {
        int i_y = j - shl_y;
        MP_T x_j = (j > digs ? 0 : MP_DIGIT (x, j));
        MP_T y_j = (i_y <= 0 || i_y > digs ? 0 : MP_DIGIT (y, i_y));
        MP_DIGIT (w, j + 1) = x_j - y_j;
      }
    } else {
      int shl_x = (int) MP_EXPONENT (y) - (int) MP_EXPONENT (x);
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (y);
      for (j = 1; j < digs_h; j++) {
        int i_x = j - shl_x;
        MP_T x_j = (i_x <= 0 || i_x > digs ? 0 : MP_DIGIT (x, i_x));
        MP_T y_j = (j > digs ? 0 : MP_DIGIT (y, j));
        MP_DIGIT (w, j + 1) = x_j - y_j;
      }
    }
// Correct if we subtract large from small.
    if (MP_DIGIT (w, 2) <= 0) {
      fnz = -1;
      for (j = 2; j <= digs_h && fnz < 0; j++) {
        if (MP_DIGIT (w, j) != 0) {
          fnz = j;
        }
      }
      negative = (BOOL_T) (MP_DIGIT (w, fnz) < 0);
      if (negative) {
        for (j = fnz; j <= digs_h; j++) {
          MP_DIGIT (w, j) = -MP_DIGIT (w, j);
        }
      }
    }
// Normalise.
    norm_mp_light (w, 2, digs_h);
    fnz = -1;
    for (j = 1; j <= digs_h && fnz < 0; j++) {
      if (MP_DIGIT (w, j) != 0) {
        fnz = j;
      }
    }
    if (fnz > 1) {
      int j2 = fnz - 1;
      for (j = 1; j <= digs_h - j2; j++) {
        MP_DIGIT (w, j) = MP_DIGIT (w, j + j2);
        MP_DIGIT (w, j + j2) = (MP_T) 0;
      }
      MP_EXPONENT (w) -= j2;
    }
// Round.
    round_internal_mp (z, w, digs);
    if (negative) {
      MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
    }
    check_mp_exp (p, z);
  }
// Restore and exit.
  A68_SP = pop_sp;
  MP_T z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (y, 1) = y_1;
  MP_DIGIT (z, 1) = z_1;        // In case z IS x OR z IS y
  return z;
}

//! @brief Set "z" to the product of "x" and "y".

MP_T *mul_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
  if (IS_ZERO_MP (x) || IS_ZERO_MP (y)) {
    SET_MP_ZERO (z, digs);
    return z;
  }
// Grammar school algorithm with intermittent normalisation.
  ADDR_T pop_sp = A68_SP;
  int i, digs_h = 2 + digs;
  MP_T x_1 = MP_DIGIT (x, 1), y_1 = MP_DIGIT (y, 1);
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_DIGIT (y, 1) = ABS (y_1);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_T *w = lit_mp (p, 0, MP_EXPONENT (x) + MP_EXPONENT (y) + 1, digs_h);
  int oflow = (int) FLOOR_MP ((MP_REAL_T) MAX_REPR_INT / (2 * MP_REAL_RADIX * MP_REAL_RADIX)) - 1;
  for (i = digs; i >= 1; i--) {
    MP_T yi = MP_DIGIT (y, i);
    if (yi != 0) {
      int k = digs_h - i;
      int j = (k > digs ? digs : k);
      MP_T *u = &MP_DIGIT (w, i + j), *v = &MP_DIGIT (x, j);
      if ((digs - i + 1) % oflow == 0) {
        norm_mp (w, 2, digs_h);
      }
      while (j-- >= 1) {
        (u--)[0] += yi * (v--)[0];
      }
    }
  }
  norm_mp (w, 2, digs_h);
  round_internal_mp (z, w, digs);
// Restore and exit.
  A68_SP = pop_sp;
  MP_T z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (y, 1) = y_1;
  MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
  check_mp_exp (p, z);
  return z;
}

//! @brief Set "z" to the quotient of "x" and "y".

MP_T *div_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
// This routine is based on
// 
//    D. M. Smith, "A Multiple-Precision Division Algorithm"
//    Mathematics of Computation 66 (1996) 157-163.
// 
// This is O(N^2) but runs faster than straightforward methods by skipping
// most of the intermediate normalisation and recovering from wrong
// guesses without separate correction steps.
//
// Depending on application, div_mp cost is circa 3 times that of mul_mp.
// Therefore Newton-Raphson division makes no sense here.
//
  if (IS_ZERO_MP (y)) {
    errno = ERANGE;
    return NaN_MP;
  }
// Determine normalisation interval assuming that q < 2b in each step.
#if (A68_LEVEL <= 2)
  int oflow = (int) FLOOR_MP ((MP_REAL_T) MAX_REPR_INT / (3 * MP_REAL_RADIX * MP_REAL_RADIX)) - 1;
#else
  int oflow = (int) FLOOR_MP ((MP_REAL_T) MAX_REPR_INT / (2 * MP_REAL_RADIX * MP_REAL_RADIX)) - 1;
#endif
//
  MP_T x_1 = MP_DIGIT (x, 1), y_1 = MP_DIGIT (y, 1);
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_DIGIT (y, 1) = ABS (y_1);
  MP_STATUS (z) = (MP_T) INIT_MASK;
// Slight optimisation when the denominator has few digits.
  int nzdigs = digs;
  while (MP_DIGIT (y, nzdigs) == 0 && nzdigs > 1) {
    nzdigs--;
  }
  if (nzdigs == 1 && MP_EXPONENT (y) == 0) {
    (void) div_mp_digit (p, z, x, MP_DIGIT (y, 1), digs);
    MP_T z_1 = MP_DIGIT (z, 1);
    MP_DIGIT (x, 1) = x_1;
    MP_DIGIT (y, 1) = y_1;
    MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
    check_mp_exp (p, z);
    return z;
  }
// Working nominator in which the quotient develops.
  ADDR_T pop_sp = A68_SP;
  int wdigs = 4 + digs;
  MP_T *w = lit_mp (p, 0, MP_EXPONENT (x) - MP_EXPONENT (y), wdigs);
  (void) move_mp_part (&MP_DIGIT (w, 2), &MP_DIGIT (x, 1), digs);
// Estimate the denominator. For small MP_RADIX add: MP_DIGIT (y, 4) / MP_REAL_RADIX.
  MP_REAL_T den = (MP_DIGIT (y, 1) * MP_REAL_RADIX + MP_DIGIT (y, 2)) * MP_REAL_RADIX + MP_DIGIT (y, 3);
  MP_T *t = &MP_DIGIT (w, 2);
  int k, len, first;
  for (k = 1, len = digs + 2, first = 3; k <= digs + 2; k++, len++, first++, t++) {
// Estimate quotient digit.
    MP_REAL_T q, nom = ((t[-1] * MP_REAL_RADIX + t[0]) * MP_REAL_RADIX + t[1]) * MP_REAL_RADIX + (wdigs >= (first + 2) ? t[2] : 0);
    if (nom == 0) {
      q = 0;
    } else {
// Correct the nominator.
      q = (MP_T) (MP_INT_T) (nom / den);
      int lim = MINIMUM (len, wdigs);
      if (nzdigs <= lim - first + 1) {
        lim = first + nzdigs - 1;
      }
      MP_T *u = t, *v = &MP_DIGIT (y, 1);
      int j;
      for (j = first; j <= lim; j++) {
        (u++)[0] -= q * (v++)[0];
      }
    }
    t[0] += t[-1] * MP_RADIX;
    t[-1] = q;
    if (k % oflow == 0 || k == digs + 2) {
      norm_mp (w, first, wdigs);
    }
  }
  norm_mp (w, 2, digs);
  round_internal_mp (z, w, digs);
// Restore and exit.
  A68_SP = pop_sp;
  MP_T z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (y, 1) = y_1;
  MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
  check_mp_exp (p, z);
  return z;
}

//! @brief Set "z" to the integer quotient of "x" and "y".

MP_T *over_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
  if (MP_DIGIT (y, 1) == 0) {
    errno = ERANGE;
    return NaN_MP;
  }
  int digs_g = FUN_DIGITS (digs);
  ADDR_T pop_sp = A68_SP;
  MP_T *x_g = len_mp (p, x, digs, digs_g);
  MP_T *y_g = len_mp (p, y, digs, digs_g);
  MP_T *z_g = nil_mp (p, digs_g);
  (void) div_mp (p, z_g, x_g, y_g, digs_g);
  trunc_mp (p, z_g, z_g, digs_g);
  (void) shorten_mp (p, z, digs, z_g, digs_g);
  MP_STATUS (z) = (MP_T) INIT_MASK;
// Restore and exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief Set "z" to x mod y.

MP_T *mod_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
  if (MP_DIGIT (y, 1) == 0) {
    errno = EDOM;
    return NaN_MP;
  }
  int digs_g = FUN_DIGITS (digs);
  ADDR_T pop_sp = A68_SP;
  MP_T *x_g = len_mp (p, x, digs, digs_g);
  MP_T *y_g = len_mp (p, y, digs, digs_g);
  MP_T *z_g = nil_mp (p, digs_g);
// x mod y = x - y * trunc (x / y).
  (void) over_mp (p, z_g, x_g, y_g, digs_g);
  (void) mul_mp (p, z_g, y_g, z_g, digs_g);
  (void) sub_mp (p, z_g, x_g, z_g, digs_g);
  (void) shorten_mp (p, z, digs, z_g, digs_g);
// Restore and exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief Set "z" to the product of x and digit y.

MP_T *mul_mp_digit (NODE_T * p, MP_T * z, MP_T * x, MP_T y, int digs)
{
// This is an O(N) routine for multiplication by a short value.
  MP_T x_1 = MP_DIGIT (x, 1);
  int digs_h = 2 + digs;
  ADDR_T pop_sp = A68_SP;
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_T y_1 = y;
  y = ABS (y_1);
  if (y == 2) {
    (void) add_mp (p, z, x, x, digs);
  } else {
    MP_T *w = lit_mp (p, 0, MP_EXPONENT (x) + 1, digs_h);
    MP_T *u = &MP_DIGIT (w, 1 + digs), *v = &MP_DIGIT (x, digs);
    int j = digs;
    while (j-- >= 1) {
      (u--)[0] += y * (v--)[0];
    }
    norm_mp (w, 2, digs_h);
    round_internal_mp (z, w, digs);
  }
// Restore and exit.
  A68_SP = pop_sp;
  MP_T z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
  check_mp_exp (p, z);
  return z;
}

//! @brief Set "z" to x/2.

MP_T *half_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  MP_T *w, z_1, x_1 = MP_DIGIT (x, 1), *u, *v;
  int j, digs_h = 2 + digs;
  ADDR_T pop_sp = A68_SP;
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_STATUS (z) = (MP_T) INIT_MASK;
// Calculate x * 0.5.
  w = lit_mp (p, 0, MP_EXPONENT (x), digs_h);
  j = digs;
  u = &MP_DIGIT (w, 1 + digs);
  v = &MP_DIGIT (x, digs);
  while (j-- >= 1) {
    (u--)[0] += (MP_RADIX / 2) * (v--)[0];
  }
  norm_mp (w, 2, digs_h);
  round_internal_mp (z, w, digs);
// Restore and exit.
  A68_SP = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (z, 1) = (x_1 >= 0 ? z_1 : -z_1);
  check_mp_exp (p, z);
  return z;
}

//! @brief Set "z" to x/10.

MP_T *tenth_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  MP_T *w, z_1, x_1 = MP_DIGIT (x, 1), *u, *v;
  int j, digs_h = 2 + digs;
  ADDR_T pop_sp = A68_SP;
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_STATUS (z) = (MP_T) INIT_MASK;
// Calculate x * 0.1.
  w = lit_mp (p, 0, MP_EXPONENT (x), digs_h);
  j = digs;
  u = &MP_DIGIT (w, 1 + digs);
  v = &MP_DIGIT (x, digs);
  while (j-- >= 1) {
    (u--)[0] += (MP_RADIX / 10) * (v--)[0];
  }
  norm_mp (w, 2, digs_h);
  round_internal_mp (z, w, digs);
// Restore and exit.
  A68_SP = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (z, 1) = (x_1 >= 0 ? z_1 : -z_1);
  check_mp_exp (p, z);
  return z;
}

//! @brief Set "z" to the quotient of x and digit y.

MP_T *div_mp_digit (NODE_T * p, MP_T * z, MP_T * x, MP_T y, int digs)
{
  if (y == 0) {
    errno = ERANGE;
    return NaN_MP;
  }
// Determine normalisation interval assuming that q < 2b in each step.
#if (A68_LEVEL <= 2)
  int oflow = (int) FLOOR_MP ((MP_REAL_T) MAX_REPR_INT / (3 * MP_REAL_RADIX * MP_REAL_RADIX)) - 1;
#else
  int oflow = (int) FLOOR_MP ((MP_REAL_T) MAX_REPR_INT / (2 * MP_REAL_RADIX * MP_REAL_RADIX)) - 1;
#endif
// Work with positive operands.
  ADDR_T pop_sp = A68_SP;
  MP_T x_1 = MP_DIGIT (x, 1), y_1 = y;
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  y = ABS (y_1);
//
  if (y == 2) {
    (void) half_mp (p, z, x, digs);
  } else if (y == 10) {
    (void) tenth_mp (p, z, x, digs);
  } else {
    int k, first, wdigs = 4 + digs;
    MP_T *w = lit_mp (p, 0, MP_EXPONENT (x), wdigs);
    (void) move_mp_part (&MP_DIGIT (w, 2), &MP_DIGIT (x, 1), digs);
// Estimate the denominator.
    MP_REAL_T den = (MP_REAL_T) y * MP_REAL_RADIX * MP_REAL_RADIX;
    MP_T *t = &MP_DIGIT (w, 2);
    for (k = 1, first = 3; k <= digs + 2; k++, first++, t++) {
// Estimate quotient digit and correct.
      MP_REAL_T nom = ((t[-1] * MP_REAL_RADIX + t[0]) * MP_REAL_RADIX + t[1]) * MP_REAL_RADIX + (wdigs >= (first + 2) ? t[2] : 0);
      MP_REAL_T q = (MP_T) (MP_INT_T) (nom / den);
      t[0] += t[-1] * MP_RADIX - q * y;
      t[-1] = q;
      if (k % oflow == 0 || k == digs + 2) {
        norm_mp (w, first, wdigs);
      }
    }
    norm_mp (w, 2, digs);
    round_internal_mp (z, w, digs);
  }
// Restore and exit.
  A68_SP = pop_sp;
  MP_T z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
  check_mp_exp (p, z);
  return z;
}

//! @brief Set "z" to the integer quotient of "x" and "y".

MP_T *over_mp_digit (NODE_T * p, MP_T * z, MP_T * x, MP_T y, int digs)
{
  if (y == 0) {
    errno = ERANGE;
    return NaN_MP;
  }
  int digs_g = FUN_DIGITS (digs);
  ADDR_T pop_sp = A68_SP;
  MP_T *x_g = len_mp (p, x, digs, digs_g);
  MP_T *z_g = nil_mp (p, digs_g);
  (void) div_mp_digit (p, z_g, x_g, y, digs_g);
  trunc_mp (p, z_g, z_g, digs_g);
  (void) shorten_mp (p, z, digs, z_g, digs_g);
// Restore and exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief Set "z" to the reciprocal of "x".

MP_T *rec_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  if (IS_ZERO_MP (x)) {
    errno = ERANGE;
    return NaN_MP;
  }
  ADDR_T pop_sp = A68_SP;
  (void) div_mp (p, z, mp_one (digs), x, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief LONG REAL long pi

void genie_pi_mp (NODE_T * p)
{
  int digs = DIGITS (MOID (p));
  MP_T *z = nil_mp (p, digs);
  (void) mp_pi (p, z, MP_PI, digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief Set "z" to "x" ** "n".

MP_T *pow_mp_int (NODE_T * p, MP_T * z, MP_T * x, INT_T n, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int bit, digs_g = FUN_DIGITS (digs);
  BOOL_T negative;
  MP_T *x_g = len_mp (p, x, digs, digs_g);
  MP_T *z_g = lit_mp (p, 1, 0, digs_g);
  negative = (BOOL_T) (n < 0);
  if (negative) {
    n = -n;
  }
  bit = 1;
  while ((unt) bit <= (unt) n) {
    if (n & bit) {
      (void) mul_mp (p, z_g, z_g, x_g, digs_g);
    }
    (void) mul_mp (p, x_g, x_g, x_g, digs_g);
    bit <<= 1;
  }
  (void) shorten_mp (p, z, digs, z_g, digs_g);
  A68_SP = pop_sp;
  if (negative) {
    (void) rec_mp (p, z, z, digs);
  }
  check_mp_exp (p, z);
  return z;
}

//! @brief Set "z" to "x" ** "y".

MP_T *pow_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
  PRELUDE_ERROR (ln_mp (p, z, x, digs) == NaN_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  (void) mul_mp (p, z, y, z, digs);
  (void) exp_mp (p, z, z, digs);
  return z;
}

//! @brief Set "z" to 10 ** "n".

MP_T *ten_up_mp (NODE_T * p, MP_T * z, int n, int digs)
{
#if (A68_LEVEL >= 3)
  static MP_T y[LOG_MP_RADIX] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 };
#else
  static MP_T y[LOG_MP_RADIX] = { 1, 10, 100, 1000, 10000, 100000, 1000000 };
#endif
  if (n >= 0) {
    set_mp (z, y[n % LOG_MP_RADIX], n / LOG_MP_RADIX, digs);
  } else {
    set_mp (z, y[(LOG_MP_RADIX + n % LOG_MP_RADIX) % LOG_MP_RADIX], (n + 1) / LOG_MP_RADIX - 1, digs);
  }
  check_mp_exp (p, z);
  return z;
}

//! @brief Comparison of "x" and "y".

void eq_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *v = nil_mp (p, digs);
  (void) sub_mp (p, v, x, y, digs);
  STATUS (z) = INIT_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) == 0 ? A68_TRUE : A68_FALSE);
  A68_SP = pop_sp;
}

//! @brief Comparison of "x" and "y".

void ne_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *v = nil_mp (p, digs);
  (void) sub_mp (p, v, x, y, digs);
  STATUS (z) = INIT_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) != 0 ? A68_TRUE : A68_FALSE);
  A68_SP = pop_sp;
}

//! @brief Comparison of "x" and "y".

void lt_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *v = nil_mp (p, digs);
  (void) sub_mp (p, v, x, y, digs);
  STATUS (z) = INIT_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) < 0 ? A68_TRUE : A68_FALSE);
  A68_SP = pop_sp;
}

//! @brief Comparison of "x" and "y".

void le_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *v = nil_mp (p, digs);
  (void) sub_mp (p, v, x, y, digs);
  STATUS (z) = INIT_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) <= 0 ? A68_TRUE : A68_FALSE);
  A68_SP = pop_sp;
}

//! @brief Comparison of "x" and "y".

void gt_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *v = nil_mp (p, digs);
  (void) sub_mp (p, v, x, y, digs);
  STATUS (z) = INIT_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) > 0 ? A68_TRUE : A68_FALSE);
  A68_SP = pop_sp;
}

//! @brief Comparison of "x" and "y".

void ge_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *v = nil_mp (p, digs);
  (void) sub_mp (p, v, x, y, digs);
  STATUS (z) = INIT_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) >= 0 ? A68_TRUE : A68_FALSE);
  A68_SP = pop_sp;
}

//! @brief round (x).

MP_T *round_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  MP_T *y = nil_mp (p, digs);
  SET_MP_HALF (y, digs);
  if (MP_DIGIT (x, 1) >= 0) {
    (void) add_mp (p, z, x, y, digs);
    (void) trunc_mp (p, z, z, digs);
  } else {
    (void) sub_mp (p, z, x, y, digs);
    (void) trunc_mp (p, z, z, digs);
  }
  MP_STATUS (z) = (MP_T) INIT_MASK;
  return z;
}

//! @brief Entier (x).

MP_T *entier_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  if (MP_DIGIT (x, 1) >= 0) {
    (void) trunc_mp (p, z, x, digs);
  } else {
    MP_T *y = nil_mp (p, digs);
    (void) move_mp (y, z, digs);
    (void) trunc_mp (p, z, x, digs);
    (void) sub_mp (p, y, y, z, digs);
    if (MP_DIGIT (y, 1) != 0) {
      SET_MP_ONE (y, digs);
      (void) sub_mp (p, z, z, y, digs);
    }
  }
  MP_STATUS (z) = (MP_T) INIT_MASK;
  return z;
}
