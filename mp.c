/*!
\file mp.c
\brief multiprecision arithmetic library
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

/*
A MULTIPRECISION ARITHMETIC LIBRARY FOR ALGOL68G

The question that is often raised is what applications justify multiprecision
calculations. A number of applications, some of them practical, have surfaced
over the years.

Most common application of multiprecision calculations are numerically unstable
calculations, that require many significant digits to arrive at a reliable
result.

Multiprecision calculations are used in "experimental mathematics". An
increasingly important application of computers is in doing experiments on
mathematical systems, when such system is not easily, or not at all, tractable
by analysis.

One important area of applications is in pure mathematics. Although numerical
calculations cannot substitute a formal proof, calculations can be used to
explore conjectures and reject those that are not sound, before a lengthy
attempt at such proof is undertaken.

Multiprecision calculations are especially useful in the study of mathematical
constants. One of the oldest applications of multiprecision computation is to
explore whether the expansions of classical constants such as "pi", "e" or
"sqrt(2)" are random in some sense. For example, digits of "pi" have not shown
statistical anomalies even now that billions of digits have been calculated.

A practical application of multiprecision computation is the emerging
field of public-key cryptography, that has spawned much research into advanced
algorithms for factoring large integers.

An indirect application of multiprecision computation is integrity testing.
A unique feature of multiprecision calculations is that they are unforgiving
of hardware, program or compiler error. Even a single computational error will
almost certainly result in a completely incorrect outcome after a possibly
correct start.

The routines in this library follow algorithms as described in the
literature, notably

D.M. Smith, "Efficient Multiple-Precision Evaluation of Elementary Functions"
Mathematics of Computation 52 (1989) 131-134

D.M. Smith, "A Multiple-Precision Division Algorithm"
Mathematics of Computation 66 (1996) 157-163

There are multiprecision libraries (freely) available, but this one is
particularly designed to work with Algol68G. It implements following modes:

   LONG INT, LONG REAL, LONG COMPLEX, LONG BITS
   LONG LONG INT, LONG LONG REAL, LONG LONG COMPLEX, LONG LONG BITS

Currently, LONG modes have a fixed precision, and LONG LONG modes have
user-definable precision. Precisions span about 30 decimal digits for
LONG modes up to (default) about 60 decimal digits for LONG LONG modes, a
range that is said to be adequate for most multiprecision applications.

Although the maximum length of a mp number is unbound, this implementation
is not particularly designed for more than about a thousand digits. It will
work at higher precisions, but with a performance penalty with respect to
state of the art implementations that may for instance use convolution for
multiplication.

This library takes a sloppy approach towards LONG INT and LONG BITS which are
implemented as LONG REAL and truncated where appropriate. This keeps the code
short at the penalty of some performance loss.

As is common practice, mp numbers are represented by a row of digits
in a large base. Layout of a mp number "z" is:

   MP_T *z;
   MP_STATUS (z)        Status word
   MP_EXPONENT (z)      Exponent with base MP_RADIX
   MP_DIGIT (z, 1 .. N) Digits 1 .. N

Note that this library assumes IEEE 754 compatible implementation of
type "double". It also assumes 32 (or 64) bit type "int".

Most "vintage" multiple precision libraries stored numbers as [] int.
However, since division and multiplication are O (N ** 2) operations, it is
advantageous to keep the base as high as possible. Modern computers handle
doubles at similar or better speed as integers, therefore this library
opts for storing numbers as [] double, trading space for speed. This may
change when 64 bit integers become commonplace.

Set a base such that "base ** 2" can be exactly represented by "double".
To facilitate transput, we require a base that is a power of 10.

If we choose the base right then in multiplication and division we do not need
to normalise intermediate results at each step since a number of additions
can be made before overflow occurs. That is why we specify "MAX_REPR_INT".

Mind that the precision of a mp number is at worst just
(LONG_MP_DIGITS - 1) * LOG_MP_BASE + 1, since the most significant mp digit
is also in range [0 .. MP_RADIX>. Do not specify less than 2 digits.

Since this software is distributed without any warranty, it is your
responsibility to validate the behaviour of the routines and their accuracy
using the source code provided. See the GNU General Public License for details.
*/

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

/* If DOUBLE_PRECISION is defined functions are evaluated in double precision */

#undef DOUBLE_PRECISION

/* Internal mp constants */

static MP_T *ref_mp_pi = NO_MP;
static int mp_pi_size = -1;

static MP_T *ref_mp_ln_scale = NO_MP;
static int mp_ln_scale_size = -1;

static MP_T *ref_mp_ln_10 = NO_MP;
static int mp_ln_10_size = -1;

int varying_mp_digits = 10;

static int _j1_, _j2_;
#define MINIMUM(x, y) (_j1_ = (x), _j2_ = (y), _j1_ < _j2_ ? _j1_ : _j2_)

/*
GUARD_DIGITS: number of guard digits.

In calculations using intermediate results we will use guard digits.
We follow D.M. Smith in his recommendations for precisions greater than LONG.
*/

#if defined DOUBLE_PRECISION
#define GUARD_DIGITS(digits) (digits)
#else
#define GUARD_DIGITS(digits) (((digits) == LONG_MP_DIGITS) ? 2 : (LOG_MP_BASE <= 5) ? 3 : 2)
#endif

#define FUN_DIGITS(n) ((n) + GUARD_DIGITS (n))

/*!
\brief length in bytes of a long mp number
\return length in bytes of a long mp number
**/

size_t size_long_mp (void)
{
  return ((size_t) SIZE_MP (LONG_MP_DIGITS));
}

/*!
\brief length in digits of a long mp number
\return length in digits of a long mp number
**/

int long_mp_digits (void)
{
  return (LONG_MP_DIGITS);
}

/*!
\brief length in bytes of a long long mp number
\return length in bytes of a long long mp number
**/

size_t size_longlong_mp (void)
{
  return ((size_t) (SIZE_MP (varying_mp_digits)));
}

/*!
\brief length in digits of a long long mp number
\return length in digits of a long long mp number
**/

int longlong_mp_digits (void)
{
  return (varying_mp_digits);
}

/*!
\brief length in digits of mode
\param m mode
\return length in digits of mode m
**/

int get_mp_digits (MOID_T * m)
{
  if (m == MODE (LONG_INT) || m == MODE (LONG_REAL) || m == MODE (LONG_COMPLEX) || m == MODE (LONG_BITS)) {
    return (long_mp_digits ());
  } else if (m == MODE (LONGLONG_INT) || m == MODE (LONGLONG_REAL) || m == MODE (LONGLONG_COMPLEX) || m == MODE (LONGLONG_BITS)) {
    return (longlong_mp_digits ());
  }
  return (0);
}

/*!
\brief length in bytes of mode
\param m mode
\return length in bytes of mode m
**/

int get_mp_size (MOID_T * m)
{
  if (m == MODE (LONG_INT) || m == MODE (LONG_REAL) || m == MODE (LONG_COMPLEX) || m == MODE (LONG_BITS)) {
    return ((int) size_long_mp ());
  } else if (m == MODE (LONGLONG_INT) || m == MODE (LONGLONG_REAL) || m == MODE (LONGLONG_COMPLEX) || m == MODE (LONGLONG_BITS)) {
    return ((int) size_longlong_mp ());
  }
  return (0);
}

/*!
\brief length in bits of mode
\param m mode
\return length in bits of mode m
**/

int get_mp_bits_width (MOID_T * m)
{
  if (m == MODE (LONG_BITS)) {
    return (MP_BITS_WIDTH (LONG_MP_DIGITS));
  } else if (m == MODE (LONGLONG_BITS)) {
    return (MP_BITS_WIDTH (varying_mp_digits));
  }
  return (0);
}

/*!
\brief length in words of mode
\param m mode
\return length in words of mode m
**/

int get_mp_bits_words (MOID_T * m)
{
  if (m == MODE (LONG_BITS)) {
    return (MP_BITS_WORDS (LONG_MP_DIGITS));
  } else if (m == MODE (LONGLONG_BITS)) {
    return (MP_BITS_WORDS (varying_mp_digits));
  }
  return (0);
}

/*!
\brief whether z is a valid LONG INT
\param z mp number
\return same
**/

BOOL_T check_long_int (MP_T * z)
{
  return ((BOOL_T) ((MP_EXPONENT (z) >= (MP_T) 0) && (MP_EXPONENT (z) < (MP_T) LONG_MP_DIGITS)));
}

/*!
\brief whether z is a valid LONG LONG INT
\param z mp number
\return same
**/

BOOL_T check_longlong_int (MP_T * z)
{
  return ((BOOL_T) ((MP_EXPONENT (z) >= (MP_T) 0) && (MP_EXPONENT (z) < (MP_T) varying_mp_digits)));
}

/*!
\brief whether z is a valid representation for its mode
\param z mp number
\param m mode
\return same
**/

BOOL_T check_mp_int (MP_T * z, MOID_T * m)
{
  if (m == MODE (LONG_INT) || m == MODE (LONG_BITS)) {
    return (check_long_int (z));
  } else if (m == MODE (LONGLONG_INT) || m == MODE (LONGLONG_BITS)) {
    return (check_longlong_int (z));
  }
  return (A68_FALSE);
}

/*!
\brief convert precision to digits for long long number
\param n precision to convert
\return same
**/

int int_to_mp_digits (int n)
{
  return (2 + (int) ceil ((double) n / (double) LOG_MP_BASE));
}

/*!
\brief set number of digits for long long numbers
\param n number of digits
**/

void set_longlong_mp_digits (int n)
{
  varying_mp_digits = n;
}

/*!
\brief set "z" to short value x * MP_RADIX ** x_expo
\param z mp number to set
\param x most significant mp digit
\param x_expo mp exponent
\param digits precision in mp-digits
\return result "z"
**/

MP_T *set_mp_short (MP_T * z, MP_T x, int x_expo, int digits)
{
  MP_T *d = &MP_DIGIT ((z), 2);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  MP_EXPONENT (z) = (MP_T) x_expo;
  MP_DIGIT (z, 1) = (MP_T) x;
  while (--digits) {
    *d++ = (MP_T) 0;
  }
  return (z);
}

/*!
\brief test whether x = y
\param p position in tree
\param x mp number 1
\param y mp number 2
\param digits precision in mp-digits
\return same
**/

static BOOL_T same_mp (NODE_T * p, MP_T * x, MP_T * y, int digits)
{
  int k;
  (void) p;
  if (MP_EXPONENT (x) == MP_EXPONENT (y)) {
    for (k = digits; k >= 1; k--) {
      if (MP_DIGIT (x, k) != MP_DIGIT (y, k)) {
        return (A68_FALSE);
      }
    }
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief align 10-base z in a MP_RADIX mantissa
\param z mp number
\param expo
\param digits precision in mp-digits
\return result "z"
**/

static MP_T *align_mp (MP_T * z, int *expo, int digits)
{
  int i, shift;
  if (*expo >= 0) {
    shift = LOG_MP_BASE - *expo % LOG_MP_BASE - 1;
    *expo /= LOG_MP_BASE;
  } else {
    shift = (-*expo - 1) % LOG_MP_BASE;
    *expo = (*expo + 1) / LOG_MP_BASE;
    (*expo)--;
  }
/* Now normalise "z" */
  for (i = 1; i <= shift; i++) {
    int j, carry = 0;
    for (j = 1; j <= digits; j++) {
      int k = (int) MP_DIGIT (z, j) % 10;
      MP_DIGIT (z, j) = (MP_T) ((int) (MP_DIGIT (z, j) / 10) + carry * (MP_RADIX / 10));
      carry = k;
    }
  }
  return (z);
}

/*!
\brief transform string into multi-precision number
\param p position in tree
\param z mp number
\param s string to convert
\param digits precision in mp-digits
\return result "z"
**/

MP_T *string_to_mp (NODE_T * p, MP_T * z, char *s, int digits)
{
  int i, j, sign, weight, sum, comma, power;
  int expo;
  BOOL_T ok = A68_TRUE;
  RESET_ERRNO;
  SET_MP_ZERO (z, digits);
  while (IS_SPACE (s[0])) {
    s++;
  }
/* Get the sign	*/
  sign = (s[0] == '-' ? -1 : 1);
  if (s[0] == '+' || s[0] == '-') {
    s++;
  }
/* Scan mantissa digits and put them into "z" */
  while (s[0] == '0') {
    s++;
  }
  i = 0;
  j = 1;
  sum = 0;
  comma = -1;
  power = 0;
  weight = MP_RADIX / 10;
  while (s[i] != NULL_CHAR && j <= digits && (IS_DIGIT (s[i]) || s[i] == POINT_CHAR)) {
    if (s[i] == POINT_CHAR) {
      comma = i;
    } else {
      int value = (int) s[i] - (int) '0';
      sum += weight * value;
      weight /= 10;
      power++;
      if (weight < 1) {
        MP_DIGIT (z, j++) = (MP_T) sum;
        sum = 0;
        weight = MP_RADIX / 10;
      }
    }
    i++;
  }
/* Store the last digits */
  if (j <= digits) {
    MP_DIGIT (z, j++) = (MP_T) sum;
  }
/* See if there is an exponent */
  expo = 0;
  if (s[i] != NULL_CHAR && TO_UPPER (s[i]) == TO_UPPER (EXPONENT_CHAR)) {
    char *end;
    expo = strtol (&(s[++i]), &end, 10);
    ok = (BOOL_T) (end[0] == NULL_CHAR);
  } else {
    ok = (BOOL_T) (s[i] == NULL_CHAR);
  }
/* Calculate effective exponent */
  expo += (comma >= 0 ? comma - 1 : power - 1);
  (void) align_mp (z, &expo, digits);
  MP_EXPONENT (z) = (MP_DIGIT (z, 1) == 0 ? 0 : (double) expo);
  MP_DIGIT (z, 1) *= sign;
  CHECK_MP_EXPONENT (p, z);
  if (errno == 0 && ok) {
    return (z);
  } else {
    return (NO_MP);
  }
}

/*!
\brief convert integer to multi-precison number
\param p position in tree
\param z mp number
\param k integer to convert
\param digits precision in mp-digits
\return result "z"
**/

MP_T *int_to_mp (NODE_T * p, MP_T * z, int k, int digits)
{
  int n = 0, j, sign_k = SIGN (k);
  int k2 = k;
  if (k < 0) {
    k = -k;
  }
  while ((k2 /= MP_RADIX) != 0) {
    n++;
  }
  SET_MP_ZERO (z, digits);
  MP_EXPONENT (z) = (MP_T) n;
  for (j = 1 + n; j >= 1; j--) {
    MP_DIGIT (z, j) = (MP_T) (k % MP_RADIX);
    k /= MP_RADIX;
  }
  MP_DIGIT (z, 1) = sign_k * MP_DIGIT (z, 1);
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief convert unsigned to multi-precison number
\param p position in tree
\param z mp number
\param k unsigned to convert
\param digits precision in mp-digits
\return result "z"
**/

MP_T *unsigned_to_mp (NODE_T * p, MP_T * z, unsigned k, int digits)
{
  int n = 0, j;
  unsigned k2 = k;
  while ((k2 /= MP_RADIX) != 0) {
    n++;
  }
  SET_MP_ZERO (z, digits);
  MP_EXPONENT (z) = (MP_T) n;
  for (j = 1 + n; j >= 1; j--) {
    MP_DIGIT (z, j) = (MP_T) (k % MP_RADIX);
    k /= MP_RADIX;
  }
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief convert multi-precision number to integer
\param p position in tree
\param z mp number
\param digits precision in mp-digits
\return result "z"
**/

int mp_to_int (NODE_T * p, MP_T * z, int digits)
{
/*
This routines looks a lot like "strtol". 
We do not use "mp_to_real" since int could be wider than 2 ** 52.
*/
  int j, expo = (int) MP_EXPONENT (z);
  int sum = 0, weight = 1;
  BOOL_T negative;
  if (expo >= digits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MOID (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  negative = (BOOL_T) (MP_DIGIT (z, 1) < 0);
  if (negative) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  for (j = 1 + expo; j >= 1; j--) {
    int term;
    if ((int) MP_DIGIT (z, j) > A68_MAX_INT / weight) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    term = (int) MP_DIGIT (z, j) * weight;
    if (sum > A68_MAX_INT - term) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    sum += term;
    weight *= MP_RADIX;
  }
  return (negative ? -sum : sum);
}

/*!
\brief convert multi-precision number to unsigned
\param p position in tree
\param z mp number
\param digits precision in mp-digits
\return result "z"
**/

unsigned mp_to_unsigned (NODE_T * p, MP_T * z, int digits)
{
/*
This routines looks a lot like "strtol". We do not use "mp_to_real" since int
could be wider than 2 ** 52.
*/
  int j, expo = (int) MP_EXPONENT (z);
  unsigned sum = 0, weight = 1;
  if (expo >= digits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MOID (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (j = 1 + expo; j >= 1; j--) {
    unsigned term;
    if ((unsigned) MP_DIGIT (z, j) > A68_MAX_UNT / weight) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BITS));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    term = (unsigned) MP_DIGIT (z, j) * weight;
    if (sum > A68_MAX_UNT - term) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BITS));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    sum += term;
    weight *= MP_RADIX;
  }
  return (sum);
}

/*!
\brief convert double to multi-precison number
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *real_to_mp (NODE_T * p, MP_T * z, double x, int digits)
{
  int j, k, sign_x, sum, weight;
  int expo;
  double a;
  MP_T *u;
  SET_MP_ZERO (z, digits);
  if (x == 0.0) {
    return (z);
  }
/* Small integers can be done better by int_to_mp */
  if (ABS (x) < MP_RADIX && (double) (int) x == x) {
    return (int_to_mp (p, z, (int) x, digits));
  }
  sign_x = SIGN (x);
/* Scale to [0, 0.1> */
  a = x = ABS (x);
  expo = (int) log10 (a);
  a /= ten_up (expo);
  expo--;
  if (a >= 1) {
    a /= 10;
    expo++;
  }
/* Transport digits of x to the mantissa of z */
  k = 0;
  j = 1;
  sum = 0;
  weight = (MP_RADIX / 10);
  u = &MP_DIGIT (z, 1);
  while (j <= digits && k < DBL_DIG) {
    double y = floor (a * 10);
    int value = (int) y;
    a = a * 10 - y;
    sum += weight * value;
    weight /= 10;
    if (weight < 1) {
      (u++)[0] = (MP_T) sum;
      sum = 0;
      weight = (MP_RADIX / 10);
    }
    k++;
  }
/* Store the last digits */
  if (j <= digits) {
    u[0] = (MP_T) sum;
  }
  (void) align_mp (z, &expo, digits);
  MP_EXPONENT (z) = (MP_T) expo;
  MP_DIGIT (z, 1) *= sign_x;
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief convert multi-precision number to double
\param p position in tree
\param z mp number
\param digits precision in mp-digits
\return result "z"
**/

double mp_to_real (NODE_T * p, MP_T * z, int digits)
{
/* This routine looks a lot like "strtod" */
  (void) p;
  if (MP_EXPONENT (z) * (MP_T) LOG_MP_BASE <= (MP_T) DBL_MIN_10_EXP) {
    return (0);
  } else {
    int j;
    double sum = 0, weight;
    weight = ten_up ((int) (MP_EXPONENT (z) * LOG_MP_BASE));
    for (j = 1; j <= digits && (j - 2) * LOG_MP_BASE <= DBL_DIG; j++) {
      sum += ABS (MP_DIGIT (z, j)) * weight;
      weight /= MP_RADIX;
    }
    CHECK_REAL_REPRESENTATION (p, sum);
    return (MP_DIGIT (z, 1) >= 0 ? sum : -sum);
  }
}

/*!
\brief convert z to a row of unsigned in the stack
\param p position in tree
\param z mp number
\param m mode of "z"
\return result "z"
**/

unsigned *stack_mp_bits (NODE_T * p, MP_T * z, MOID_T * m)
{
  int digits = get_mp_digits (m), words = get_mp_bits_words (m), k, lim;
  unsigned *row, mask;
  MP_T *u, *v, *w;
  row = (unsigned *) STACK_ADDRESS (stack_pointer);
  INCREMENT_STACK_POINTER (p, words * ALIGNED_SIZE_OF (unsigned));
  STACK_MP (u, p, digits);
  STACK_MP (v, p, digits);
  STACK_MP (w, p, digits);
  MOVE_MP (u, z, digits);
/* Argument check */
  if (MP_DIGIT (u, 1) < 0.0) {
    errno = EDOM;
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, (m == MODE (LONG_BITS) ? MODE (LONG_INT) : MODE (LONGLONG_INT)));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Convert radix MP_BITS_RADIX number */
  for (k = words - 1; k >= 0; k--) {
    MOVE_MP (w, u, digits);
    (void) over_mp_digit (p, u, u, (MP_T) MP_BITS_RADIX, digits);
    (void) mul_mp_digit (p, v, u, (MP_T) MP_BITS_RADIX, digits);
    (void) sub_mp (p, v, w, v, digits);
    row[k] = (unsigned) MP_DIGIT (v, 1);
  }
/* Test on overflow: too many bits or not reduced to 0 */
  mask = 0x1;
  lim = get_mp_bits_width (m) % MP_BITS_BITS;
  for (k = 1; k < lim; k++) {
    mask <<= 1;
    mask |= 0x1;
  }
  if ((row[0] & ~mask) != 0x0 || MP_DIGIT (u, 1) != 0.0) {
    errno = ERANGE;
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Exit */
  return (row);
}

/*!
\brief whether LONG BITS value is in range
\param p position in tree
\param u mp number
\param m mode of "u"
**/

void check_long_bits_value (NODE_T * p, MP_T * u, MOID_T * m)
{
  if (MP_EXPONENT (u) >= (MP_T) (get_mp_digits (m) - 1)) {
    ADDR_T pop_sp = stack_pointer;
    (void) stack_mp_bits (p, u, m);
    stack_pointer = pop_sp;
  }
}

/*!
\brief convert row of unsigned to LONG BITS
\param p position in tree
\param u mp number
\param row
\param m mode of "u"
\return result "u"
**/

MP_T *pack_mp_bits (NODE_T * p, MP_T * u, unsigned *row, MOID_T * m)
{
  int digits = get_mp_digits (m), words = get_mp_bits_words (m), k, lim;
  ADDR_T pop_sp = stack_pointer;
  MP_T *v, *w;
/* Discard excess bits */
  unsigned mask = 0x1, musk = 0x0;
  STACK_MP (v, p, digits);
  STACK_MP (w, p, digits);
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
/* Convert */
  SET_MP_ZERO (u, digits);
  (void) set_mp_short (v, (MP_T) 1, 0, digits);
  for (k = words - 1; k >= 0; k--) {
    (void) mul_mp_digit (p, w, v, (MP_T) (musk & row[k]), digits);
    (void) add_mp (p, u, u, w, digits);
    if (k != 0) {
      (void) mul_mp_digit (p, v, v, (MP_T) MP_BITS_RADIX, digits);
    }
  }
  MP_STATUS (u) = (MP_T) INITIALISED_MASK;
  stack_pointer = pop_sp;
  return (u);
}

/*!
\brief normalise positive intermediate, fast
\param w argument
\param k last digit to normalise
\param digits precision in mp-digits
**/

static void norm_mp_light (MP_T * w, int k, int digits)
{
/* Bring every digit back to [0 .. MP_RADIX> */
  int j;
  MP_T *z;
  for (j = digits, z = &MP_DIGIT (w, digits); j >= k; j--, z--) {
    if (z[0] >= MP_RADIX) {
      z[0] -= (MP_T) MP_RADIX;
      z[-1] += 1;
    } else if (z[0] < 0) {
      z[0] += (MP_T) MP_RADIX;
      z[-1] -= 1;
    }
  }
}

/*!
\brief normalise positive intermediate
\param w argument
\param k last digit to normalise
\param digits precision in mp-digits
**/

static void norm_mp (MP_T * w, int k, int digits)
{
/* Bring every digit back to [0 .. MP_RADIX> */
  int j;
  MP_T *z;
  for (j = digits, z = &MP_DIGIT (w, digits); j >= k; j--, z--) {
    if (z[0] >= (MP_T) MP_RADIX) {
      MP_T carry = (MP_T) ((int) (z[0] / (MP_T) MP_RADIX));
      z[0] -= carry * (MP_T) MP_RADIX;
      z[-1] += carry;
    } else if (z[0] < (MP_T) 0) {
      MP_T carry = (MP_T) 1 + (MP_T) ((int) ((-z[0] - 1) / (MP_T) MP_RADIX));
      z[0] += carry * (MP_T) MP_RADIX;
      z[-1] -= carry;
    }
  }
}

/*!
\brief round multi-precision number
\param z result
\param w argument, must be positive
\param digits precision in mp-digits
**/

static void round_internal_mp (MP_T * z, MP_T * w, int digits)
{
/* Assume that w has precision of at least 2 + digits */
  int last = (MP_DIGIT (w, 1) == 0 ? 2 + digits : 1 + digits);
  if (MP_DIGIT (w, last) >= MP_RADIX / 2) {
    MP_DIGIT (w, last - 1) += 1;
  }
  if (MP_DIGIT (w, last - 1) >= MP_RADIX) {
    norm_mp (w, 2, last);
  }
  if (MP_DIGIT (w, 1) == 0) {
    MOVE_DIGITS (&MP_DIGIT (z, 1), &MP_DIGIT (w, 2), digits);
    MP_EXPONENT (z) = MP_EXPONENT (w) - 1;
  } else {
/* Normally z != w, so no test on this */
    MOVE_DIGITS (&MP_EXPONENT (z), &MP_EXPONENT (w), (1 + digits));
  }
/* Zero is zero is zero */
  if (MP_DIGIT (z, 1) == 0) {
    MP_EXPONENT (z) = (MP_T) 0;
  }
}

/*!
\brief truncate at decimal point
\param p position in tree
\param z result
\param x argument
\param digits precision in mp-digits
**/

void trunc_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  if (MP_EXPONENT (x) < 0) {
    SET_MP_ZERO (z, digits);
  } else if (MP_EXPONENT (x) >= (MP_T) digits) {
    errno = EDOM;
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, (IS (MOID (p), PROC_SYMBOL) ? SUB_MOID (p) : MOID (p)));
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    int k;
    MOVE_MP (z, x, digits);
    for (k = (int) (MP_EXPONENT (x) + 2); k <= digits; k++) {
      MP_DIGIT (z, k) = (MP_T) 0;
    }
  }
}

/*!
\brief shorten and round
\param p position in tree
\param z result
\param digits precision in mp-digits
\param x mp number
\param digits precision in mp-digits_x
\return result "z"
**/

MP_T *shorten_mp (NODE_T * p, MP_T * z, int digits, MP_T * x, int digits_x)
{
  if (digits >= digits_x) {
    errno = EDOM;
    return (NO_MP);
  } else {
/* Reserve extra digits for proper rounding */
    int pop_sp = stack_pointer, digits_h = digits + 2;
    MP_T *w;
    BOOL_T negative = (BOOL_T) (MP_DIGIT (x, 1) < 0);
    STACK_MP (w, p, digits_h);
    if (negative) {
      MP_DIGIT (x, 1) = -MP_DIGIT (x, 1);
    }
    MP_STATUS (w) = (MP_T) 0;
    MP_EXPONENT (w) = MP_EXPONENT (x) + 1;
    MP_DIGIT (w, 1) = (MP_T) 0;
    MOVE_DIGITS (&MP_DIGIT (w, 2), &MP_DIGIT (x, 1), digits + 1);
    round_internal_mp (z, w, digits);
    if (negative) {
      MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
    }
    stack_pointer = pop_sp;
    return (z);
  }
}

/*!
\brief lengthen x and assign to z
\param p position in tree
\param z mp number
\param digits precision in mp-digits of "z"
\param x mp number
\param digits precision in mp-digits of "x"
\return result "z"
**/

MP_T *lengthen_mp (NODE_T * p, MP_T * z, int digits_z, MP_T * x, int digits_x)
{
  int j;
  (void) p;
  if (digits_z > digits_x) {
    if (z != x) {
      MOVE_DIGITS (&MP_DIGIT (z, 1), &MP_DIGIT (x, 1), digits_x);
      MP_EXPONENT (z) = MP_EXPONENT (x);
      MP_STATUS (z) = MP_STATUS (x);
    }
    for (j = 1 + digits_x; j <= digits_z; j++) {
      MP_DIGIT (z, j) = (MP_T) 0;
    }
  }
  return (z);
}

/*!
\brief set "z" to the sum of positive "x" and positive "y"
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *add_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *w, z_1, x_1 = MP_DIGIT (x, 1), y_1 = MP_DIGIT (y, 1);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
/* Trivial cases */
  if (MP_DIGIT (x, 1) == (MP_T) 0) {
    MOVE_MP (z, y, digits);
    return (z);
  } else if (MP_DIGIT (y, 1) == 0) {
    MOVE_MP (z, x, digits);
    return (z);
  }
/* We want positive arguments */
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_DIGIT (y, 1) = ABS (y_1);
  if (x_1 >= 0 && y_1 < 0) {
    (void) sub_mp (p, z, x, y, digits);
  } else if (x_1 < 0 && y_1 >= 0) {
    (void) sub_mp (p, z, y, x, digits);
  } else if (x_1 < 0 && y_1 < 0) {
    (void) add_mp (p, z, x, y, digits);
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  } else {
/* Add */
    int j, digits_h = 2 + digits;
    STACK_MP (w, p, digits_h);
    MP_DIGIT (w, 1) = (MP_T) 0;
    if (MP_EXPONENT (x) == MP_EXPONENT (y)) {
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (x);
      for (j = 1; j <= digits; j++) {
        MP_DIGIT (w, j + 1) = MP_DIGIT (x, j) + MP_DIGIT (y, j);
      }
      MP_DIGIT (w, digits_h) = (MP_T) 0;
    } else if (MP_EXPONENT (x) > MP_EXPONENT (y)) {
      int shl_y = (int) MP_EXPONENT (x) - (int) MP_EXPONENT (y);
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (x);
      for (j = 1; j < digits_h; j++) {
        int i_y = j - (int) shl_y;
        MP_T x_j = (j > digits ? 0 : MP_DIGIT (x, j));
        MP_T y_j = (i_y <= 0 || i_y > digits ? 0 : MP_DIGIT (y, i_y));
        MP_DIGIT (w, j + 1) = x_j + y_j;
      }
    } else {
      int shl_x = (int) MP_EXPONENT (y) - (int) MP_EXPONENT (x);
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (y);
      for (j = 1; j < digits_h; j++) {
        int i_x = j - (int) shl_x;
        MP_T x_j = (i_x <= 0 || i_x > digits ? 0 : MP_DIGIT (x, i_x));
        MP_T y_j = (j > digits ? 0 : MP_DIGIT (y, j));
        MP_DIGIT (w, j + 1) = x_j + y_j;
      }
    }
    norm_mp_light (w, 2, digits_h);
    round_internal_mp (z, w, digits);
    CHECK_MP_EXPONENT (p, z);
  }
/* Restore and exit */
  stack_pointer = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (y, 1) = y_1;
  MP_DIGIT (z, 1) = z_1;        /* In case z IS x OR z IS y */
  return (z);
}

/*!
\brief set "z" to the difference of positive "x" and positive "y"
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *sub_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  int fnz, k;
  MP_T *w, z_1, x_1 = MP_DIGIT (x, 1), y_1 = MP_DIGIT (y, 1);
  BOOL_T negative = A68_FALSE;
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
/* Trivial cases */
  if (MP_DIGIT (x, 1) == (MP_T) 0) {
    MOVE_MP (z, y, digits);
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
    return (z);
  } else if (MP_DIGIT (y, 1) == (MP_T) 0) {
    MOVE_MP (z, x, digits);
    return (z);
  }
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_DIGIT (y, 1) = ABS (y_1);
/* We want positive arguments */
  if (x_1 >= 0 && y_1 < 0) {
    (void) add_mp (p, z, x, y, digits);
  } else if (x_1 < 0 && y_1 >= 0) {
    (void) add_mp (p, z, y, x, digits);
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  } else if (x_1 < 0 && y_1 < 0) {
    (void) sub_mp (p, z, y, x, digits);
  } else {
/* Subtract */
    int j, digits_h = 2 + digits;
    STACK_MP (w, p, digits_h);
    MP_DIGIT (w, 1) = (MP_T) 0;
    if (MP_EXPONENT (x) == MP_EXPONENT (y)) {
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (x);
      for (j = 1; j <= digits; j++) {
        MP_DIGIT (w, j + 1) = MP_DIGIT (x, j) - MP_DIGIT (y, j);
      }
      MP_DIGIT (w, digits_h) = (MP_T) 0;
    } else if (MP_EXPONENT (x) > MP_EXPONENT (y)) {
      int shl_y = (int) MP_EXPONENT (x) - (int) MP_EXPONENT (y);
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (x);
      for (j = 1; j < digits_h; j++) {
        int i_y = j - (int) shl_y;
        MP_T x_j = (j > digits ? 0 : MP_DIGIT (x, j));
        MP_T y_j = (i_y <= 0 || i_y > digits ? 0 : MP_DIGIT (y, i_y));
        MP_DIGIT (w, j + 1) = x_j - y_j;
      }
    } else {
      int shl_x = (int) MP_EXPONENT (y) - (int) MP_EXPONENT (x);
      MP_EXPONENT (w) = (MP_T) 1 + MP_EXPONENT (y);
      for (j = 1; j < digits_h; j++) {
        int i_x = j - (int) shl_x;
        MP_T x_j = (i_x <= 0 || i_x > digits ? 0 : MP_DIGIT (x, i_x));
        MP_T y_j = (j > digits ? 0 : MP_DIGIT (y, j));
        MP_DIGIT (w, j + 1) = x_j - y_j;
      }
    }
/* Correct if we subtract large from small */
    if (MP_DIGIT (w, 2) <= 0) {
      fnz = -1;
      for (j = 2; j <= digits_h && fnz < 0; j++) {
        if (MP_DIGIT (w, j) != 0) {
          fnz = j;
        }
      }
      negative = (BOOL_T) (MP_DIGIT (w, fnz) < 0);
      if (negative) {
        for (j = fnz; j <= digits_h; j++) {
          MP_DIGIT (w, j) = -MP_DIGIT (w, j);
        }
      }
    }
/* Normalise */
    norm_mp_light (w, 2, digits_h);
    fnz = -1;
    for (j = 1; j <= digits_h && fnz < 0; j++) {
      if (MP_DIGIT (w, j) != 0) {
        fnz = j;
      }
    }
    if (fnz > 1) {
      int j2 = fnz - 1;
      for (k = 1; k <= digits_h - j2; k++) {
        MP_DIGIT (w, k) = MP_DIGIT (w, k + j2);
        MP_DIGIT (w, k + j2) = (MP_T) 0;
      }
      MP_EXPONENT (w) -= j2;
    }
/* Round */
    round_internal_mp (z, w, digits);
    if (negative) {
      MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
    }
    CHECK_MP_EXPONENT (p, z);
  }
/* Restore and exit */
  stack_pointer = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (y, 1) = y_1;
  MP_DIGIT (z, 1) = z_1;        /* In case z IS x OR z IS y */
  return (z);
}

/*!
\brief set "z" to the product of "x" and "y"
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *mul_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digits)
{
  MP_T *w, z_1, x_1 = MP_DIGIT (x, 1), y_1 = MP_DIGIT (y, 1);
  int i, oflow, digits_h = 2 + digits;
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_DIGIT (y, 1) = ABS (y_1);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  if (x_1 == 0 || y_1 == 0) {
    stack_pointer = pop_sp;
    MP_DIGIT (x, 1) = x_1;
    MP_DIGIT (y, 1) = y_1;
    SET_MP_ZERO (z, digits);
    return (z);
  }
/* Calculate z = x * y */
  STACK_MP (w, p, digits_h);
  SET_MP_ZERO (w, digits_h);
  MP_EXPONENT (w) = MP_EXPONENT (x) + MP_EXPONENT (y) + 1;
  oflow = (int) (floor) ((double) MAX_REPR_INT / (2 * (double) MP_RADIX * (double) MP_RADIX)) - 1;
  ABEND (oflow <= 1, "inadequate MP_RADIX", NO_TEXT);
  if (digits < oflow) {
    for (i = digits; i >= 1; i--) {
      MP_T yi = MP_DIGIT (y, i);
      if (yi != 0) {
        int k = digits_h - i;
        int j = (k > digits ? digits : k);
        MP_T *u = &MP_DIGIT (w, i + j);
        MP_T *v = &MP_DIGIT (x, j);
        while (j-- >= 1) {
          (u--)[0] += yi * (v--)[0];
        }
      }
    }
  } else {
    for (i = digits; i >= 1; i--) {
      MP_T yi = MP_DIGIT (y, i);
      if (yi != 0) {
        int k = digits_h - i;
        int j = (k > digits ? digits : k);
        MP_T *u = &MP_DIGIT (w, i + j);
        MP_T *v = &MP_DIGIT (x, j);
        if ((digits - i + 1) % oflow == 0) {
          norm_mp (w, 2, digits_h);
        }
        while (j-- >= 1) {
          (u--)[0] += yi * (v--)[0];
        }
      }
    }
  }
  norm_mp (w, 2, digits_h);
  round_internal_mp (z, w, digits);
/* Restore and exit */
  stack_pointer = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (y, 1) = y_1;
  MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief set "z" to the quotient of "x" and "y"
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *div_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digits)
{
/*
This routine is an implementation of

   D. M. Smith, "A Multiple-Precision Division Algorithm"
   Mathematics of Computation 66 (1996) 157-163.

This algorithm is O(N ** 2) but runs faster than straightforward methods by
skipping most of the intermediate normalisation and recovering from wrong
guesses without separate correction steps.
*/
  double xd;
  MP_T *t, *w, z_1, x_1 = MP_DIGIT (x, 1), y_1 = MP_DIGIT (y, 1);
  int k, oflow, digits_w = 4 + digits;
  ADDR_T pop_sp = stack_pointer;
  if (y_1 == 0) {
    errno = ERANGE;
    return (NO_MP);
  }
/* Determine normalisation interval assuming that q < 2b in each step */
  oflow = (int) (floor) ((double) MAX_REPR_INT / (3 * (double) MP_RADIX * (double) MP_RADIX)) - 1;
  ABEND (oflow <= 1, "inadequate MP_RADIX", NO_TEXT);
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_DIGIT (y, 1) = ABS (y_1);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
/* `w' will be the working nominator in which the quotient develops */
  STACK_MP (w, p, digits_w);
  MP_EXPONENT (w) = MP_EXPONENT (x) - MP_EXPONENT (y);
  MP_DIGIT (w, 1) = (MP_T) 0;
  MOVE_DIGITS (&MP_DIGIT (w, 2), &MP_DIGIT (x, 1), digits);
  MP_DIGIT (w, digits + 2) = (MP_T) 0;
  MP_DIGIT (w, digits + 3) = (MP_T) 0;
  MP_DIGIT (w, digits + 4) = (MP_T) 0;
/* Estimate the denominator. Take four terms to also suit small MP_RADIX */
  xd = (MP_DIGIT (y, 1) * MP_RADIX + MP_DIGIT (y, 2)) * MP_RADIX + MP_DIGIT (y, 3) + MP_DIGIT (y, 4) / MP_RADIX;
  t = &MP_DIGIT (w, 2);
  if (digits + 2 < oflow) {
    for (k = 1; k <= digits + 2; k++, t++) {
      double xn, q;
      int first = k + 2;
/* Estimate quotient digit */
      xn = ((t[-1] * MP_RADIX + t[0]) * MP_RADIX + t[1]) * MP_RADIX + (digits_w >= (first + 2) ? t[2] : 0);
      q = (double) ((int) (xn / xd));
      if (q != 0) {
/* Correct the nominator */
        int j, len = k + digits + 1;
        int lim = (len < digits_w ? len : digits_w);
        MP_T *u = t, *v = &MP_DIGIT (y, 1);
        for (j = first; j <= lim; j++) {
          (u++)[0] -= q * (v++)[0];
        }
      }
      t[0] += (t[-1] * MP_RADIX);
      t[-1] = q;
    }
  } else {
    for (k = 1; k <= digits + 2; k++, t++) {
      double xn, q;
      int first = k + 2;
      if (k % oflow == 0) {
        norm_mp (w, first, digits_w);
      }
/* Estimate quotient digit */
      xn = ((t[-1] * MP_RADIX + t[0]) * MP_RADIX + t[1]) * MP_RADIX + (digits_w >= (first + 2) ? t[2] : 0);
      q = (double) ((int) (xn / xd));
      if (q != 0) {
/* Correct the nominator */
        int j, len = k + digits + 1;
        int lim = (len < digits_w ? len : digits_w);
        MP_T *u = t, *v = &MP_DIGIT (y, 1);
        for (j = first; j <= lim; j++) {
          (u++)[0] -= q * (v++)[0];
        }
      }
      t[0] += (t[-1] * MP_RADIX);
      t[-1] = q;
    }
  }
  norm_mp (w, 2, digits_w);
  round_internal_mp (z, w, digits);
/* Restore and exit */
  stack_pointer = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (y, 1) = y_1;
  MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief set "z" to the integer quotient of "x" and "y"
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *over_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digits)
{
  int digits_g = FUN_DIGITS (digits);
  MP_T *x_g, *y_g, *z_g;
  ADDR_T pop_sp = stack_pointer;
  if (MP_DIGIT (y, 1) == 0) {
    errno = ERANGE;
    return (NO_MP);
  }
  STACK_MP (x_g, p, digits_g);
  STACK_MP (y_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  (void) lengthen_mp (p, y_g, digits_g, y, digits);
  (void) div_mp (p, z_g, x_g, y_g, digits_g);
  trunc_mp (p, z_g, z_g, digits_g);
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
/* Restore and exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to x mod y
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *mod_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digits)
{
  int digits_g = FUN_DIGITS (digits);
  ADDR_T pop_sp = stack_pointer;
  MP_T *x_g, *y_g, *z_g;
  if (MP_DIGIT (y, 1) == 0) {
    errno = EDOM;
    return (NO_MP);
  }
  STACK_MP (x_g, p, digits_g);
  STACK_MP (y_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) lengthen_mp (p, y_g, digits_g, y, digits);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
/* x mod y = x - y * trunc (x / y) */
  (void) over_mp (p, z_g, x_g, y_g, digits_g);
  (void) mul_mp (p, z_g, y_g, z_g, digits_g);
  (void) sub_mp (p, z_g, x_g, z_g, digits_g);
  (void) shorten_mp (p, z, digits, z_g, digits_g);
/* Restore and exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to the product of x and digit y
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *mul_mp_digit (NODE_T * p, MP_T * z, MP_T * x, MP_T y, int digits)
{
/* This is an O(N) routine for multiplication by a short value */
  MP_T *w, z_1, x_1 = MP_DIGIT (x, 1), y_1 = y, *u, *v;
  int j, digits_h = 2 + digits;
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  y = ABS (y_1);
  STACK_MP (w, p, digits_h);
  SET_MP_ZERO (w, digits_h);
  MP_EXPONENT (w) = MP_EXPONENT (x) + 1;
  j = digits;
  u = &MP_DIGIT (w, 1 + digits);
  v = &MP_DIGIT (x, digits);
  while (j-- >= 1) {
    (u--)[0] += y * (v--)[0];
  }
  norm_mp (w, 2, digits_h);
  round_internal_mp (z, w, digits);
/* Restore and exit */
  stack_pointer = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief set "z" to x/2
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *half_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  MP_T *w, z_1, x_1 = MP_DIGIT (x, 1), *u, *v;
  int j, digits_h = 2 + digits;
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  STACK_MP (w, p, digits_h);
  SET_MP_ZERO (w, digits_h);
/* Calculate x * 0.5 */
  MP_EXPONENT (w) = MP_EXPONENT (x);
  j = digits;
  u = &MP_DIGIT (w, 1 + digits);
  v = &MP_DIGIT (x, digits);
  while (j-- >= 1) {
    (u--)[0] += (MP_RADIX / 2) * (v--)[0];
  }
  norm_mp (w, 2, digits_h);
  round_internal_mp (z, w, digits);
/* Restore and exit */
  stack_pointer = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (z, 1) = (x_1 >= 0 ? z_1 : -z_1);
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief set "z" to the quotient of x and digit y
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *div_mp_digit (NODE_T * p, MP_T * z, MP_T * x, MP_T y, int digits)
{
  double xd;
  MP_T *t, *w, z_1, x_1 = MP_DIGIT (x, 1), y_1 = y;
  int k, oflow, digits_w = 4 + digits;
  ADDR_T pop_sp = stack_pointer;
  if (y == 0) {
    errno = ERANGE;
    return (NO_MP);
  }
/* Determine normalisation interval assuming that q < 2b in each step */
  oflow = (int) (floor) ((double) MAX_REPR_INT / (3 * (double) MP_RADIX * (double) MP_RADIX)) - 1;
  ABEND (oflow <= 1, "inadequate MP_RADIX", NO_TEXT);
/* Work with positive operands */
  MP_DIGIT (x, 1) = ABS (x_1);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  y = ABS (y_1);
  STACK_MP (w, p, digits_w);
  MP_EXPONENT (w) = MP_EXPONENT (x);
  MP_DIGIT (w, 1) = (MP_T) 0;
  MOVE_DIGITS (&MP_DIGIT (w, 2), &MP_DIGIT (x, 1), digits);
  MP_DIGIT (w, digits + 2) = (MP_T) 0;
  MP_DIGIT (w, digits + 3) = (MP_T) 0;
  MP_DIGIT (w, digits + 4) = (MP_T) 0;
/* Estimate the denominator */
  xd = (double) y *MP_RADIX * MP_RADIX;
  t = &MP_DIGIT (w, 2);
  if (digits + 2 < oflow) {
    for (k = 1; k <= digits + 2; k++, t++) {
      double xn, q;
      int first = k + 2;
/* Estimate quotient digit and correct */
      xn = ((t[-1] * MP_RADIX + t[0]) * MP_RADIX + t[1]) * MP_RADIX + (digits_w >= (first + 2) ? t[2] : 0);
      q = (double) ((int) (xn / xd));
      t[0] += (t[-1] * MP_RADIX - q * y);
      t[-1] = q;
    }
  } else {
    for (k = 1; k <= digits + 2; k++, t++) {
      double xn, q;
      int first = k + 2;
      if (k % oflow == 0) {
        norm_mp (w, first, digits_w);
      }
/* Estimate quotient digit and correct */
      xn = ((t[-1] * MP_RADIX + t[0]) * MP_RADIX + t[1]) * MP_RADIX + (digits_w >= (first + 2) ? t[2] : 0);
      q = (double) ((int) (xn / xd));
      t[0] += (t[-1] * MP_RADIX - q * y);
      t[-1] = q;
    }
  }
  norm_mp (w, 2, digits_w);
  round_internal_mp (z, w, digits);
/* Restore and exit */
  stack_pointer = pop_sp;
  z_1 = MP_DIGIT (z, 1);
  MP_DIGIT (x, 1) = x_1;
  MP_DIGIT (z, 1) = ((x_1 * y_1) >= 0 ? z_1 : -z_1);
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief set "z" to the integer quotient of "x" and "y"
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *over_mp_digit (NODE_T * p, MP_T * z, MP_T * x, MP_T y, int digits)
{
  int digits_g = FUN_DIGITS (digits);
  ADDR_T pop_sp = stack_pointer;
  MP_T *x_g, *z_g;
  if (y == 0) {
    errno = ERANGE;
    return (NO_MP);
  }
  STACK_MP (x_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  (void) div_mp_digit (p, z_g, x_g, y, digits_g);
  trunc_mp (p, z_g, z_g, digits_g);
  (void) shorten_mp (p, z, digits, z_g, digits_g);
/* Restore and exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to the reciprocal of "x"
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *rec_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *one;
  if (MP_DIGIT (x, 1) == 0) {
    errno = ERANGE;
    return (NO_MP);
  }
  STACK_MP (one, p, digits);
  (void) set_mp_short (one, (MP_T) 1, 0, digits);
  (void) div_mp (p, z, one, x, digits);
/* Restore and exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to "x" ** "n"
\param p position in tree
\param z mp number
\param x mp number
\param n integer power
\param digits precision in mp-digits
\return result "z"
**/

MP_T *pow_mp_int (NODE_T * p, MP_T * z, MP_T * x, int n, int digits)
{
  int pop_sp = stack_pointer, bit, digits_g = FUN_DIGITS (digits);
  BOOL_T negative;
  MP_T *z_g, *x_g;
  STACK_MP (z_g, p, digits_g);
  STACK_MP (x_g, p, digits_g);
  (void) set_mp_short (z_g, (MP_T) 1, 0, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  negative = (BOOL_T) (n < 0);
  if (negative) {
    n = -n;
  }
  bit = 1;
  while ((unsigned) bit <= (unsigned) n) {
    if (n & bit) {
      (void) mul_mp (p, z_g, z_g, x_g, digits_g);
    }
    (void) mul_mp (p, x_g, x_g, x_g, digits_g);
    bit *= 2;
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  if (negative) {
    (void) rec_mp (p, z, z, digits);
  }
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief set "z" to 10 ** "n"
\param p position in tree
\param z mp number
\param x mp number
\param n integer power
\param digits precision in mp-digits
\return result "z"
**/

MP_T *mp_ten_up (NODE_T * p, MP_T * z, int n, int digits)
{
  int pop_sp = stack_pointer, bit, digits_g = FUN_DIGITS (digits);
  BOOL_T negative;
  MP_T *z_g, *x_g;
  STACK_MP (z_g, p, digits_g);
  STACK_MP (x_g, p, digits_g);
  (void) set_mp_short (x_g, (MP_T) 10, 0, digits_g);
  (void) set_mp_short (z_g, (MP_T) 1, 0, digits_g);
  negative = (BOOL_T) (n < 0);
  if (negative) {
    n = -n;
  }
  bit = 1;
  while ((unsigned) bit <= (unsigned) n) {
    if (n & bit) {
      (void) mul_mp (p, z_g, z_g, x_g, digits_g);
    }
    (void) mul_mp (p, x_g, x_g, x_g, digits_g);
    bit *= 2;
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  if (negative) {
    (void) rec_mp (p, z, z, digits);
  }
  CHECK_MP_EXPONENT (p, z);
  return (z);
}

/*!
\brief test on |"z"| > 0.001 for argument reduction in "sin" and "exp"
\param z mp number
\param digits precision in mp-digits
\return result "z"
**/

static BOOL_T eps_mp (MP_T * z, int digits)
{
  if (MP_DIGIT (z, 1) == 0) {
    return (A68_FALSE);
  } else if (MP_EXPONENT (z) > -1) {
    return (A68_TRUE);
  } else if (MP_EXPONENT (z) < -1) {
    return (A68_FALSE);
  } else {
#if (MP_RADIX == DEFAULT_MP_RADIX)
/* More or less optimised for LONG and default LONG LONG precisions */
    return ((BOOL_T) (digits <= 10 ? ABS (MP_DIGIT (z, 1)) > 100000 : ABS (MP_DIGIT (z, 1)) > 10000));
#else
    switch (LOG_MP_BASE) {
    case 3:
      {
        return (ABS (MP_DIGIT (z, 1)) > 1);
      }
    case 4:
      {
        return (ABS (MP_DIGIT (z, 1)) > 10);
      }
    case 5:
      {
        return (ABS (MP_DIGIT (z, 1)) > 100);
      }
    case 6:
      {
        return (ABS (MP_DIGIT (z, 1)) > 1000);
      }
    default:
      {
        ABEND (A68_TRUE, "unexpected mp base", "");
        return (A68_FALSE);
      }
    }
#endif
  }
}

/*!
\brief set "z" to sqrt ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *sqrt_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits), digits_h;
  MP_T *tmp, *x_g, *z_g;
  BOOL_T reciprocal = A68_FALSE;
  if (MP_DIGIT (x, 1) == 0) {
    stack_pointer = pop_sp;
    SET_MP_ZERO (z, digits);
    return (z);
  }
  if (MP_DIGIT (x, 1) < 0) {
    stack_pointer = pop_sp;
    errno = EDOM;
    return (NO_MP);
  }
  STACK_MP (z_g, p, digits_g);
  STACK_MP (x_g, p, digits_g);
  STACK_MP (tmp, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
/* Scaling for small x; sqrt (x) = 1 / sqrt (1 / x) */
  if ((reciprocal = (BOOL_T) (MP_EXPONENT (x_g) < 0)) == A68_TRUE) {
    (void) rec_mp (p, x_g, x_g, digits_g);
  }
  if (ABS (MP_EXPONENT (x_g)) >= 2) {
/* For extreme arguments we want accurate results as well */
    int expo = (int) MP_EXPONENT (x_g);
    MP_EXPONENT (x_g) = (MP_T) (expo % 2);
    (void) sqrt_mp (p, z_g, x_g, digits_g);
    MP_EXPONENT (z_g) += (MP_T) (expo / 2);
  } else {
/* Argument is in range. Estimate the root as double */
    int decimals;
    double x_d = mp_to_real (p, x_g, digits_g);
    (void) real_to_mp (p, z_g, sqrt (x_d), digits_g);
/* Newton's method: x<n+1> = (x<n> + a / x<n>) / 2 */
    decimals = DOUBLE_ACCURACY;
    do {
      decimals <<= 1;
      digits_h = MINIMUM (1 + decimals / LOG_MP_BASE, digits_g);
      (void) div_mp (p, tmp, x_g, z_g, digits_h);
      (void) add_mp (p, tmp, z_g, tmp, digits_h);
      (void) half_mp (p, z_g, tmp, digits_h);
    } while (decimals < 2 * digits_g * LOG_MP_BASE);
  }
  if (reciprocal) {
    (void) rec_mp (p, z_g, z_g, digits);
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
/* Exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to curt ("x"), the cube root
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *curt_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits), digits_h;
  MP_T *tmp, *x_g, *z_g;
  BOOL_T reciprocal = A68_FALSE, change_sign = A68_FALSE;
  if (MP_DIGIT (x, 1) == 0) {
    stack_pointer = pop_sp;
    SET_MP_ZERO (z, digits);
    return (z);
  }
  if (MP_DIGIT (x, 1) < 0) {
    change_sign = A68_TRUE;
    MP_DIGIT (x, 1) = -MP_DIGIT (x, 1);
  }
  STACK_MP (z_g, p, digits_g);
  STACK_MP (x_g, p, digits_g);
  STACK_MP (tmp, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
/* Scaling for small x; curt (x) = 1 / curt (1 / x) */
  if ((reciprocal = (BOOL_T) (MP_EXPONENT (x_g) < 0)) == A68_TRUE) {
    (void) rec_mp (p, x_g, x_g, digits_g);
  }
  if (ABS (MP_EXPONENT (x_g)) >= 3) {
/* For extreme arguments we want accurate results as well */
    int expo = (int) MP_EXPONENT (x_g);
    MP_EXPONENT (x_g) = (MP_T) (expo % 3);
    (void) curt_mp (p, z_g, x_g, digits_g);
    MP_EXPONENT (z_g) += (MP_T) (expo / 3);
  } else {
/* Argument is in range. Estimate the root as double */
    int decimals;
    (void) real_to_mp (p, z_g, curt (mp_to_real (p, x_g, digits_g)), digits_g);
/* Newton's method: x<n+1> = (2 x<n> + a / x<n> ^ 2) / 3 */
    decimals = DOUBLE_ACCURACY;
    do {
      decimals <<= 1;
      digits_h = MINIMUM (1 + decimals / LOG_MP_BASE, digits_g);
      (void) mul_mp (p, tmp, z_g, z_g, digits_h);
      (void) div_mp (p, tmp, x_g, tmp, digits_h);
      (void) add_mp (p, tmp, z_g, tmp, digits_h);
      (void) add_mp (p, tmp, z_g, tmp, digits_h);
      (void) div_mp_digit (p, z_g, tmp, (MP_T) 3, digits_h);
    } while (decimals < digits_g * LOG_MP_BASE);
  }
  if (reciprocal) {
    (void) rec_mp (p, z_g, z_g, digits);
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
/* Exit */
  stack_pointer = pop_sp;
  if (change_sign) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  return (z);
}

/*!
\brief set "z" to sqrt ("x"^2 + "y"^2)
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *hypot_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *t, *u, *v;
  STACK_MP (t, p, digits);
  STACK_MP (u, p, digits);
  STACK_MP (v, p, digits);
  MOVE_MP (u, x, digits);
  MOVE_MP (v, y, digits);
  MP_DIGIT (u, 1) = ABS (MP_DIGIT (u, 1));
  MP_DIGIT (v, 1) = ABS (MP_DIGIT (v, 1));
  if (IS_ZERO_MP (u)) {
    MOVE_MP (z, v, digits);
  } else if (IS_ZERO_MP (v)) {
    MOVE_MP (z, u, digits);
  } else {
    (void) set_mp_short (t, (MP_T) 1, 0, digits);
    (void) sub_mp (p, z, u, v, digits);
    if (MP_DIGIT (z, 1) > 0) {
      (void) div_mp (p, z, v, u, digits);
      (void) mul_mp (p, z, z, z, digits);
      (void) add_mp (p, z, t, z, digits);
      (void) sqrt_mp (p, z, z, digits);
      (void) mul_mp (p, z, u, z, digits);
    } else {
      (void) div_mp (p, z, u, v, digits);
      (void) mul_mp (p, z, z, z, digits);
      (void) add_mp (p, z, t, z, digits);
      (void) sqrt_mp (p, z, z, digits);
      (void) mul_mp (p, z, v, z, digits);
    }
  }
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to exp ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *exp_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
/* Argument is reduced by using exp (z / (2 ** n)) ** (2 ** n) = exp(z) */
  int m, n, pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *x_g, *sum, *a68g_pow, *fac, *tmp;
  BOOL_T iterate;
  if (MP_DIGIT (x, 1) == 0) {
    (void) set_mp_short (z, (MP_T) 1, 0, digits);
    return (z);
  }
  STACK_MP (x_g, p, digits_g);
  STACK_MP (sum, p, digits_g);
  STACK_MP (a68g_pow, p, digits_g);
  STACK_MP (fac, p, digits_g);
  STACK_MP (tmp, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  m = 0;
/* Scale x down */
  while (eps_mp (x_g, digits_g)) {
    m++;
    (void) half_mp (p, x_g, x_g, digits_g);
  }
/* Calculate Taylor sum
   exp (z) = 1 + z / 1 ! + z ** 2 / 2 ! + .. */
  (void) set_mp_short (sum, (MP_T) 1, 0, digits_g);
  (void) add_mp (p, sum, sum, x_g, digits_g);
  (void) mul_mp (p, a68g_pow, x_g, x_g, digits_g);
#if (MP_RADIX == DEFAULT_MP_RADIX)
  (void) half_mp (p, tmp, a68g_pow, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 6, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 24, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 120, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 720, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 5040, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 40320, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 362880, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) set_mp_short (fac, (MP_T) (MP_T) 3628800, 0, digits_g);
  n = 10;
#else
  (void) set_mp_short (fac, (MP_T) 2, 0, digits_g);
  n = 2;
#endif
  iterate = (BOOL_T) (MP_DIGIT (a68g_pow, 1) != 0);
  while (iterate) {
    (void) div_mp (p, tmp, a68g_pow, fac, digits_g);
    if (MP_EXPONENT (tmp) <= (MP_EXPONENT (sum) - digits_g)) {
      iterate = A68_FALSE;
    } else {
      (void) add_mp (p, sum, sum, tmp, digits_g);
      (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
      n++;
      (void) mul_mp_digit (p, fac, fac, (MP_T) n, digits_g);
    }
  }
/* Square exp (x) up */
  while (m--) {
    (void) mul_mp (p, sum, sum, sum, digits_g);
  }
  (void) shorten_mp (p, z, digits, sum, digits_g);
/* Exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to exp ("x") - 1, assuming "x" to be close to 0
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *expm1_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  int n, pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *x_g, *sum, *a68g_pow, *fac, *tmp;
  BOOL_T iterate;
  if (MP_DIGIT (x, 1) == 0) {
    (void) set_mp_short (z, (MP_T) 1, 0, digits);
    return (z);
  }
  STACK_MP (x_g, p, digits_g);
  STACK_MP (sum, p, digits_g);
  STACK_MP (a68g_pow, p, digits_g);
  STACK_MP (fac, p, digits_g);
  STACK_MP (tmp, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
/* Calculate Taylor sum expm1 (z) = z / 1 ! + z ** 2 / 2 ! + .. */
  SET_MP_ZERO (sum, digits_g);
  (void) add_mp (p, sum, sum, x_g, digits_g);
  (void) mul_mp (p, a68g_pow, x_g, x_g, digits_g);
#if (MP_RADIX == DEFAULT_MP_RADIX)
  (void) half_mp (p, tmp, a68g_pow, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 6, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 24, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 120, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 720, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 5040, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 40320, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 362880, digits_g);
  (void) add_mp (p, sum, sum, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
  (void) set_mp_short (fac, (MP_T) (MP_T) 3628800, 0, digits_g);
  n = 10;
#else
  (void) set_mp_short (fac, (MP_T) 2, 0, digits_g);
  n = 2;
#endif
  iterate = (BOOL_T) (MP_DIGIT (a68g_pow, 1) != 0);
  while (iterate) {
    (void) div_mp (p, tmp, a68g_pow, fac, digits_g);
    if (MP_EXPONENT (tmp) <= (MP_EXPONENT (sum) - digits_g)) {
      iterate = A68_FALSE;
    } else {
      (void) add_mp (p, sum, sum, tmp, digits_g);
      (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
      n++;
      (void) mul_mp_digit (p, fac, fac, (MP_T) n, digits_g);
    }
  }
  (void) shorten_mp (p, z, digits, sum, digits_g);
/* Exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief ln scale with digits precision
\param p position in tree
\param z mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *mp_ln_scale (NODE_T * p, MP_T * z, int digits)
{
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *z_g;
  STACK_MP (z_g, p, digits_g);
/* First see if we can restore a previous calculation */
  if (digits_g <= mp_ln_scale_size) {
    MOVE_MP (z_g, ref_mp_ln_scale, digits_g);
  } else {
/* No luck with the kept value, we generate a longer one */
    (void) set_mp_short (z_g, (MP_T) 1, 1, digits_g);
    (void) ln_mp (p, z_g, z_g, digits_g);
    ref_mp_ln_scale = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digits_g));
    MOVE_MP (ref_mp_ln_scale, z_g, digits_g);
    mp_ln_scale_size = digits_g;
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief ln 10 with digits precision
\param p position in tree
\param z mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *mp_ln_10 (NODE_T * p, MP_T * z, int digits)
{
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *z_g;
  STACK_MP (z_g, p, digits_g);
/* First see if we can restore a previous calculation */
  if (digits_g <= mp_ln_10_size) {
    MOVE_MP (z_g, ref_mp_ln_10, digits_g);
  } else {
/* No luck with the kept value, we generate a longer one */
    (void) set_mp_short (z_g, (MP_T) 10, 0, digits_g);
    (void) ln_mp (p, z_g, z_g, digits_g);
    ref_mp_ln_10 = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digits_g));
    MOVE_MP (ref_mp_ln_10, z_g, digits_g);
    mp_ln_10_size = digits_g;
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to ln ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *ln_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
/* Depending on the argument we choose either Taylor or Newton */
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  BOOL_T negative, scale;
  MP_T *x_g, *z_g, expo = 0;
  if (MP_DIGIT (x, 1) <= 0) {
    errno = EDOM;
    return (NO_MP);
  }
  STACK_MP (x_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  STACK_MP (z_g, p, digits_g);
/* We use ln (1 / x) = - ln (x) */
  negative = (BOOL_T) (MP_EXPONENT (x_g) < 0);
  if (negative) {
    (void) rec_mp (p, x_g, x_g, digits);
  }
/* We want correct results for extreme arguments. We scale when "x_g" exceeds
   "MP_RADIX ** +- 2", using ln (x * MP_RADIX ** n) = ln (x) + n * ln (MP_RADIX).*/
  scale = (BOOL_T) (ABS (MP_EXPONENT (x_g)) >= 2);
  if (scale) {
    expo = MP_EXPONENT (x_g);
    MP_EXPONENT (x_g) = (MP_T) 0;
  }
  if (MP_EXPONENT (x_g) == 0 && MP_DIGIT (x_g, 1) == 1 && MP_DIGIT (x_g, 2) == 0) {
/* Taylor sum for x close to unity.
   ln (x) = (x - 1) - (x - 1) ** 2 / 2 + (x - 1) ** 3 / 3 - ...
   This is faster for small x and avoids cancellation */
    MP_T *one, *tmp, *a68g_pow;
    int n = 2;
    BOOL_T iterate;
    STACK_MP (one, p, digits_g);
    STACK_MP (tmp, p, digits_g);
    STACK_MP (a68g_pow, p, digits_g);
    (void) set_mp_short (one, (MP_T) 1, 0, digits_g);
    (void) sub_mp (p, x_g, x_g, one, digits_g);
    (void) mul_mp (p, a68g_pow, x_g, x_g, digits_g);
    MOVE_MP (z_g, x_g, digits_g);
    iterate = (BOOL_T) (MP_DIGIT (a68g_pow, 1) != 0);
    while (iterate) {
      (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) n, digits_g);
      if (MP_EXPONENT (tmp) <= (MP_EXPONENT (z_g) - digits_g)) {
        iterate = A68_FALSE;
      } else {
        MP_DIGIT (tmp, 1) = (n % 2 == 0 ? -MP_DIGIT (tmp, 1) : MP_DIGIT (tmp, 1));
        (void) add_mp (p, z_g, z_g, tmp, digits_g);
        (void) mul_mp (p, a68g_pow, a68g_pow, x_g, digits_g);
        n++;
      }
    }
  } else {
/* Newton's method: x<n+1> = x<n> - 1 + a / exp (x<n>) */
    MP_T *tmp, *z_0, *one;
    int decimals;
    STACK_MP (tmp, p, digits_g);
    STACK_MP (one, p, digits_g);
    STACK_MP (z_0, p, digits_g);
    (void) set_mp_short (one, (MP_T) 1, 0, digits_g);
    SET_MP_ZERO (z_0, digits_g);
/* Construct an estimate */
    (void) real_to_mp (p, z_g, log (mp_to_real (p, x_g, digits_g)), digits_g);
    decimals = DOUBLE_ACCURACY;
    do {
      int digits_h;
      decimals <<= 1;
      digits_h = MINIMUM (1 + decimals / LOG_MP_BASE, digits_g);
      (void) exp_mp (p, tmp, z_g, digits_h);
      (void) div_mp (p, tmp, x_g, tmp, digits_h);
      (void) sub_mp (p, z_g, z_g, one, digits_h);
      (void) add_mp (p, z_g, z_g, tmp, digits_h);
    } while (decimals < digits_g * LOG_MP_BASE);
  }
/* Inverse scaling */
  if (scale) {
/* ln (x * MP_RADIX ** n) = ln (x) + n * ln (MP_RADIX) */
    MP_T *ln_base;
    STACK_MP (ln_base, p, digits_g);
    (void) mp_ln_scale (p, ln_base, digits_g);
    (void) mul_mp_digit (p, ln_base, ln_base, (MP_T) expo, digits_g);
    (void) add_mp (p, z_g, z_g, ln_base, digits_g);
  }
  if (negative) {
    MP_DIGIT (z_g, 1) = -MP_DIGIT (z_g, 1);
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
/* Exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to log ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *log_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  int pop_sp = stack_pointer;
  MP_T *ln_10;
  STACK_MP (ln_10, p, digits);
  if (ln_mp (p, z, x, digits) == NO_MP) {
    errno = EDOM;
    return (NO_MP);
  }
  (void) mp_ln_10 (p, ln_10, digits);
  (void) div_mp (p, z, z, ln_10, digits);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "sh" and "ch" to sinh ("z") and cosh ("z") respectively
\param p position in tree
\param sh mp number
\param ch mp number
\param z mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *hyp_mp (NODE_T * p, MP_T * sh, MP_T * ch, MP_T * z, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *x_g, *y_g, *z_g;
  STACK_MP (x_g, p, digits);
  STACK_MP (y_g, p, digits);
  STACK_MP (z_g, p, digits);
  MOVE_MP (z_g, z, digits);
  (void) exp_mp (p, x_g, z_g, digits);
  (void) rec_mp (p, y_g, x_g, digits);
  (void) add_mp (p, ch, x_g, y_g, digits);
/* Avoid cancellation for sinh */
  if ((MP_DIGIT (x_g, 1) == 1 && MP_DIGIT (x_g, 2) == 0) || (MP_DIGIT (y_g, 1) == 1 && MP_DIGIT (y_g, 2) == 0)) {
    (void) expm1_mp (p, x_g, z_g, digits);
    MP_DIGIT (z_g, 1) = -MP_DIGIT (z_g, 1);
    (void) expm1_mp (p, y_g, z_g, digits);
  }
  (void) sub_mp (p, sh, x_g, y_g, digits);
  (void) half_mp (p, sh, sh, digits);
  (void) half_mp (p, ch, ch, digits);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to sinh ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *sinh_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  int digits_g = FUN_DIGITS (digits);
  MP_T *x_g, *y_g, *z_g;
  STACK_MP (x_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  STACK_MP (y_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) hyp_mp (p, z_g, y_g, x_g, digits_g);
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to asinh ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *asinh_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  if (IS_ZERO_MP (x)) {
    SET_MP_ZERO (z, digits);
    return (z);
  } else {
    ADDR_T pop_sp = stack_pointer;
    int digits_g;
    MP_T *x_g, *y_g, *z_g;
    if (MP_EXPONENT (x) >= -1) {
      digits_g = FUN_DIGITS (digits);
    } else {
/* Extra precision when x^2+1 gets close to 1 */
      digits_g = 2 * FUN_DIGITS (digits);
    }
    STACK_MP (x_g, p, digits_g);
    (void) lengthen_mp (p, x_g, digits_g, x, digits);
    STACK_MP (y_g, p, digits_g);
    STACK_MP (z_g, p, digits_g);
    (void) mul_mp (p, z_g, x_g, x_g, digits_g);
    (void) set_mp_short (y_g, (MP_T) 1, 0, digits_g);
    (void) add_mp (p, y_g, z_g, y_g, digits_g);
    (void) sqrt_mp (p, y_g, y_g, digits_g);
    (void) add_mp (p, y_g, y_g, x_g, digits_g);
    (void) ln_mp (p, z_g, y_g, digits_g);
    if (IS_ZERO_MP (z_g)) {
      MOVE_MP (z, x, digits);
    } else {
      (void) shorten_mp (p, z, digits, z_g, digits_g);
    }
    stack_pointer = pop_sp;
    return (z);
  }
}

/*!
\brief set "z" to cosh ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *cosh_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  int digits_g = FUN_DIGITS (digits);
  MP_T *x_g, *y_g, *z_g;
  STACK_MP (x_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  STACK_MP (y_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) hyp_mp (p, y_g, z_g, x_g, digits_g);
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to acosh ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *acosh_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  int digits_g;
  MP_T *x_g, *y_g, *z_g;
  if (MP_DIGIT (x, 1) == 1 && MP_DIGIT (x, 2) == 0) {
/* Extra precision when x^2-1 gets close to 0 */
    digits_g = 2 * FUN_DIGITS (digits);
  } else {
    digits_g = FUN_DIGITS (digits);
  }
  STACK_MP (x_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  STACK_MP (y_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) mul_mp (p, z_g, x_g, x_g, digits_g);
  (void) set_mp_short (y_g, (MP_T) 1, 0, digits_g);
  (void) sub_mp (p, y_g, z_g, y_g, digits_g);
  (void) sqrt_mp (p, y_g, y_g, digits_g);
  (void) add_mp (p, y_g, y_g, x_g, digits_g);
  (void) ln_mp (p, z_g, y_g, digits_g);
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to tanh ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *tanh_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  int digits_g = FUN_DIGITS (digits);
  MP_T *x_g, *y_g, *z_g;
  STACK_MP (x_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  STACK_MP (y_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) hyp_mp (p, y_g, z_g, x_g, digits_g);
  (void) div_mp (p, z_g, y_g, z_g, digits_g);
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to atanh ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *atanh_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  int digits_g = FUN_DIGITS (digits);
  MP_T *x_g, *y_g, *z_g;
  STACK_MP (x_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  STACK_MP (y_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) set_mp_short (y_g, (MP_T) 1, 0, digits_g);
  (void) add_mp (p, z_g, y_g, x_g, digits_g);
  (void) sub_mp (p, y_g, y_g, x_g, digits_g);
  (void) div_mp (p, y_g, z_g, y_g, digits_g);
  (void) ln_mp (p, z_g, y_g, digits_g);
  (void) half_mp (p, z_g, z_g, digits_g);
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief return "pi" with "digits" precision, using Borwein & Borwein AGM
\param p position in tree
\param api mp number
\param mult small multiplier
\param digits precision in mp-digits
\return result "api"
**/

MP_T *mp_pi (NODE_T * p, MP_T * api, int mult, int digits)
{
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  BOOL_T iterate;
  MP_T *pi_g, *one, *two, *x_g, *y_g, *u_g, *v_g;
  STACK_MP (pi_g, p, digits_g);
/* First see if we can restore a previous calculation */
  if (digits_g <= mp_pi_size) {
    MOVE_MP (pi_g, ref_mp_pi, digits_g);
  } else {
/* No luck with the kept value, hence we generate a longer "pi".
   Calculate "pi" using the Borwein & Borwein AGM algorithm.
   This AGM doubles the numbers of digits at every pass */
    STACK_MP (one, p, digits_g);
    STACK_MP (two, p, digits_g);
    STACK_MP (x_g, p, digits_g);
    STACK_MP (y_g, p, digits_g);
    STACK_MP (u_g, p, digits_g);
    STACK_MP (v_g, p, digits_g);
    (void) set_mp_short (one, (MP_T) 1, 0, digits_g);
    (void) set_mp_short (two, (MP_T) 2, 0, digits_g);
    (void) set_mp_short (x_g, (MP_T) 2, 0, digits_g);
    (void) sqrt_mp (p, x_g, x_g, digits_g);
    (void) add_mp (p, pi_g, x_g, two, digits_g);
    (void) sqrt_mp (p, y_g, x_g, digits_g);
    iterate = A68_TRUE;
    while (iterate) {
/* New x */
      (void) sqrt_mp (p, u_g, x_g, digits_g);
      (void) div_mp (p, v_g, one, u_g, digits_g);
      (void) add_mp (p, u_g, u_g, v_g, digits_g);
      (void) half_mp (p, x_g, u_g, digits_g);
/* New pi */
      (void) add_mp (p, u_g, x_g, one, digits_g);
      (void) add_mp (p, v_g, y_g, one, digits_g);
      (void) div_mp (p, u_g, u_g, v_g, digits_g);
      (void) mul_mp (p, v_g, pi_g, u_g, digits_g);
/* Done yet? */
      if (same_mp (p, v_g, pi_g, digits_g)) {
        iterate = A68_FALSE;
      } else {
        MOVE_MP (pi_g, v_g, digits_g);
/* New y */
        (void) sqrt_mp (p, u_g, x_g, digits_g);
        (void) div_mp (p, v_g, one, u_g, digits_g);
        (void) mul_mp (p, u_g, y_g, u_g, digits_g);
        (void) add_mp (p, u_g, u_g, v_g, digits_g);
        (void) add_mp (p, v_g, y_g, one, digits_g);
        (void) div_mp (p, y_g, u_g, v_g, digits_g);
      }
    }
/* Keep the result for future restore */
    ref_mp_pi = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digits_g));
    MOVE_MP (ref_mp_pi, pi_g, digits_g);
    mp_pi_size = digits_g;
  }
  switch (mult) {
  case MP_PI:
    {
      break;
    }
  case MP_TWO_PI:
    {
      (void) mul_mp_digit (p, pi_g, pi_g, (MP_T) 2, digits_g);
      break;
    }
  case MP_HALF_PI:
    {
      (void) half_mp (p, pi_g, pi_g, digits_g);
      break;
    }
  }
  (void) shorten_mp (p, api, digits, pi_g, digits_g);
  stack_pointer = pop_sp;
  return (api);
}

/*!
\brief set "z" to sin ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *sin_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
/* Use triple-angle relation to reduce argument */
  int pop_sp = stack_pointer, m, n, digits_g = FUN_DIGITS (digits);
  BOOL_T flip, negative, iterate, even;
  MP_T *x_g, *pi, *tpi, *hpi, *sqr, *tmp, *a68g_pow, *fac, *z_g;
/* We will use "pi" */
  STACK_MP (pi, p, digits_g);
  STACK_MP (tpi, p, digits_g);
  STACK_MP (hpi, p, digits_g);
  (void) mp_pi (p, pi, MP_PI, digits_g);
  (void) mp_pi (p, tpi, MP_TWO_PI, digits_g);
  (void) mp_pi (p, hpi, MP_HALF_PI, digits_g);
/* Argument reduction (1): sin (x) = sin (x mod 2 pi) */
  STACK_MP (x_g, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  (void) mod_mp (p, x_g, x_g, tpi, digits_g);
/* Argument reduction (2): sin (-x) = sin (x)
                           sin (x) = - sin (x - pi); pi < x <= 2 pi
                           sin (x) = sin (pi - x);   pi / 2 < x <= pi */
  negative = (BOOL_T) (MP_DIGIT (x_g, 1) < 0);
  if (negative) {
    MP_DIGIT (x_g, 1) = -MP_DIGIT (x_g, 1);
  }
  STACK_MP (tmp, p, digits_g);
  (void) sub_mp (p, tmp, x_g, pi, digits_g);
  flip = (BOOL_T) (MP_DIGIT (tmp, 1) > 0);
  if (flip) {                   /* x > pi */
    (void) sub_mp (p, x_g, x_g, pi, digits_g);
  }
  (void) sub_mp (p, tmp, x_g, hpi, digits_g);
  if (MP_DIGIT (tmp, 1) > 0) {  /* x > pi / 2 */
    (void) sub_mp (p, x_g, pi, x_g, digits_g);
  }
/* Argument reduction (3): (follows from De Moivre's theorem)
   sin (3x) = sin (x) * (3 - 4 sin ^ 2 (x)) */
  m = 0;
  while (eps_mp (x_g, digits_g)) {
    m++;
    (void) div_mp_digit (p, x_g, x_g, (MP_T) 3, digits_g);
  }
/* Taylor sum */
  STACK_MP (sqr, p, digits_g);
  STACK_MP (a68g_pow, p, digits_g);
  STACK_MP (fac, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  (void) mul_mp (p, sqr, x_g, x_g, digits_g);   /* sqr = x ** 2 */
  (void) mul_mp (p, a68g_pow, sqr, x_g, digits_g);      /* pow = x ** 3 */
  MOVE_MP (z_g, x_g, digits_g);
#if (MP_RADIX == DEFAULT_MP_RADIX)
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 6, digits_g);
  (void) sub_mp (p, z_g, z_g, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, sqr, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 120, digits_g);
  (void) add_mp (p, z_g, z_g, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, sqr, digits_g);
  (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) 5040, digits_g);
  (void) sub_mp (p, z_g, z_g, tmp, digits_g);
  (void) mul_mp (p, a68g_pow, a68g_pow, sqr, digits_g);
  (void) set_mp_short (fac, (MP_T) 362880, 0, digits_g);
  n = 9;
  even = A68_TRUE;
#else
  (void) set_mp_short (fac, (MP_T) 6, 0, digits_g);
  n = 3;
  even = A68_FALSE;
#endif
  iterate = (BOOL_T) (MP_DIGIT (a68g_pow, 1) != 0);
  while (iterate) {
    (void) div_mp (p, tmp, a68g_pow, fac, digits_g);
    if (MP_EXPONENT (tmp) <= (MP_EXPONENT (z_g) - digits_g)) {
      iterate = A68_FALSE;
    } else {
      if (even) {
        (void) add_mp (p, z_g, z_g, tmp, digits_g);
        even = A68_FALSE;
      } else {
        (void) sub_mp (p, z_g, z_g, tmp, digits_g);
        even = A68_TRUE;
      }
      (void) mul_mp (p, a68g_pow, a68g_pow, sqr, digits_g);
      (void) mul_mp_digit (p, fac, fac, (MP_T) (++n), digits_g);
      (void) mul_mp_digit (p, fac, fac, (MP_T) (++n), digits_g);
    }
  }
/* Inverse scaling using sin (3x) = sin (x) * (3 - 4 sin ** 2 (x))
   Use existing mp's for intermediates */
  (void) set_mp_short (fac, (MP_T) 3, 0, digits_g);
  while (m--) {
    (void) mul_mp (p, a68g_pow, z_g, z_g, digits_g);
    (void) mul_mp_digit (p, a68g_pow, a68g_pow, (MP_T) 4, digits_g);
    (void) sub_mp (p, a68g_pow, fac, a68g_pow, digits_g);
    (void) mul_mp (p, z_g, a68g_pow, z_g, digits_g);
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  if (negative ^ flip) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
/* Exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to cos ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *cos_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
/*
Use cos (x) = sin (pi / 2 - x).
Compute x mod 2 pi before subtracting to avoid cancellation.
*/
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *hpi, *tpi, *x_g, *y;
  STACK_MP (hpi, p, digits_g);
  STACK_MP (tpi, p, digits_g);
  STACK_MP (x_g, p, digits_g);
  STACK_MP (y, p, digits);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  (void) mp_pi (p, hpi, MP_HALF_PI, digits_g);
  (void) mp_pi (p, tpi, MP_TWO_PI, digits_g);
  (void) mod_mp (p, x_g, x_g, tpi, digits_g);
  (void) sub_mp (p, x_g, hpi, x_g, digits_g);
  (void) shorten_mp (p, y, digits, x_g, digits_g);
  (void) sin_mp (p, z, y, digits);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to tan ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *tan_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
/* Use tan (x) = sin (x) / sqrt (1 - sin ^ 2 (x)) */
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *sns, *cns, *one, *pi, *hpi, *x_g, *y_g;
  BOOL_T negate;
  STACK_MP (one, p, digits);
  STACK_MP (pi, p, digits_g);
  STACK_MP (hpi, p, digits_g);
  STACK_MP (x_g, p, digits_g);
  STACK_MP (y_g, p, digits_g);
  STACK_MP (sns, p, digits);
  STACK_MP (cns, p, digits);
/* Argument mod pi */
  (void) mp_pi (p, pi, MP_PI, digits_g);
  (void) mp_pi (p, hpi, MP_HALF_PI, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  (void) mod_mp (p, x_g, x_g, pi, digits_g);
  if (MP_DIGIT (x_g, 1) >= 0) {
    (void) sub_mp (p, y_g, x_g, hpi, digits_g);
    negate = (BOOL_T) (MP_DIGIT (y_g, 1) > 0);
  } else {
    (void) add_mp (p, y_g, x_g, hpi, digits_g);
    negate = (BOOL_T) (MP_DIGIT (y_g, 1) < 0);
  }
  (void) shorten_mp (p, x, digits, x_g, digits_g);
/* tan(x) = sin(x) / sqrt (1 - sin ** 2 (x)) */
  (void) sin_mp (p, sns, x, digits);
  (void) set_mp_short (one, (MP_T) 1, 0, digits);
  (void) mul_mp (p, cns, sns, sns, digits);
  (void) sub_mp (p, cns, one, cns, digits);
  (void) sqrt_mp (p, cns, cns, digits);
  if (div_mp (p, z, sns, cns, digits) == NO_MP) {
    errno = EDOM;
    stack_pointer = pop_sp;
    return (NO_MP);
  }
  stack_pointer = pop_sp;
  if (negate) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  return (z);
}

/*!
\brief set "z" to arcsin ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *asin_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *one, *x_g, *y, *z_g;
  STACK_MP (y, p, digits);
  STACK_MP (x_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  STACK_MP (one, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  (void) set_mp_short (one, (MP_T) 1, 0, digits_g);
  (void) mul_mp (p, z_g, x_g, x_g, digits_g);
  (void) sub_mp (p, z_g, one, z_g, digits_g);
  if (sqrt_mp (p, z_g, z_g, digits) == NO_MP) {
    errno = EDOM;
    stack_pointer = pop_sp;
    return (NO_MP);
  }
  if (MP_DIGIT (z_g, 1) == 0) {
    (void) mp_pi (p, z, MP_HALF_PI, digits);
    MP_DIGIT (z, 1) = (MP_DIGIT (x_g, 1) >= 0 ? MP_DIGIT (z, 1) : -MP_DIGIT (z, 1));
    stack_pointer = pop_sp;
    return (z);
  }
  if (div_mp (p, x_g, x_g, z_g, digits_g) == NO_MP) {
    errno = EDOM;
    stack_pointer = pop_sp;
    return (NO_MP);
  }
  (void) shorten_mp (p, y, digits, x_g, digits_g);
  (void) atan_mp (p, z, y, digits);
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to arccos ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *acos_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *one, *x_g, *y, *z_g;
  BOOL_T negative = (BOOL_T) (MP_DIGIT (x, 1) < 0);
  if (MP_DIGIT (x, 1) == 0) {
    (void) mp_pi (p, z, MP_HALF_PI, digits);
    stack_pointer = pop_sp;
    return (z);
  }
  STACK_MP (y, p, digits);
  STACK_MP (x_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  STACK_MP (one, p, digits_g);
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  (void) set_mp_short (one, (MP_T) 1, 0, digits_g);
  (void) mul_mp (p, z_g, x_g, x_g, digits_g);
  (void) sub_mp (p, z_g, one, z_g, digits_g);
  if (sqrt_mp (p, z_g, z_g, digits) == NO_MP) {
    errno = EDOM;
    stack_pointer = pop_sp;
    return (NO_MP);
  }
  if (div_mp (p, x_g, z_g, x_g, digits_g) == NO_MP) {
    errno = EDOM;
    stack_pointer = pop_sp;
    return (NO_MP);
  }
  (void) shorten_mp (p, y, digits, x_g, digits_g);
  (void) atan_mp (p, z, y, digits);
  if (negative) {
    (void) mp_pi (p, y, MP_PI, digits);
    (void) add_mp (p, z, z, y, digits);
  }
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to arctan ("x")
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *atan_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
/* Depending on the argument we choose either Taylor or Newton */
  int pop_sp = stack_pointer, digits_g = FUN_DIGITS (digits);
  MP_T *x_g, *z_g;
  BOOL_T flip, negative;
  STACK_MP (x_g, p, digits_g);
  STACK_MP (z_g, p, digits_g);
  if (MP_DIGIT (x, 1) == 0) {
    stack_pointer = pop_sp;
    SET_MP_ZERO (z, digits);
    return (z);
  }
  (void) lengthen_mp (p, x_g, digits_g, x, digits);
  negative = (BOOL_T) (MP_DIGIT (x_g, 1) < 0);
  if (negative) {
    MP_DIGIT (x_g, 1) = -MP_DIGIT (x_g, 1);
  }
/* For larger arguments we use atan(x) = pi/2 - atan(1/x) */
  flip = (BOOL_T) (((MP_EXPONENT (x_g) > 0) || (MP_EXPONENT (x_g) == 0 && MP_DIGIT (x_g, 1) > 1)) && (MP_DIGIT (x_g, 1) != 0));
  if (flip) {
    (void) rec_mp (p, x_g, x_g, digits_g);
  }
  if (MP_EXPONENT (x_g) < -1 || (MP_EXPONENT (x_g) == -1 && MP_DIGIT (x_g, 1) < MP_RADIX / 100)) {
/* Taylor sum for x close to zero.
   arctan (x) = x - x ** 3 / 3 + x ** 5 / 5 - x ** 7 / 7 + ..
   This is faster for small x and avoids cancellation */
    MP_T *tmp, *a68g_pow, *sqr;
    int n = 3;
    BOOL_T iterate, even;
    STACK_MP (tmp, p, digits_g);
    STACK_MP (a68g_pow, p, digits_g);
    STACK_MP (sqr, p, digits_g);
    (void) mul_mp (p, sqr, x_g, x_g, digits_g);
    (void) mul_mp (p, a68g_pow, sqr, x_g, digits_g);
    MOVE_MP (z_g, x_g, digits_g);
    even = A68_FALSE;
    iterate = (BOOL_T) (MP_DIGIT (a68g_pow, 1) != 0);
    while (iterate) {
      (void) div_mp_digit (p, tmp, a68g_pow, (MP_T) n, digits_g);
      if (MP_EXPONENT (tmp) <= (MP_EXPONENT (z_g) - digits_g)) {
        iterate = A68_FALSE;
      } else {
        if (even) {
          (void) add_mp (p, z_g, z_g, tmp, digits_g);
          even = A68_FALSE;
        } else {
          (void) sub_mp (p, z_g, z_g, tmp, digits_g);
          even = A68_TRUE;
        }
        (void) mul_mp (p, a68g_pow, a68g_pow, sqr, digits_g);
        n += 2;
      }
    }
  } else {
/* Newton's method: x<n+1> = x<n> - cos (x<n>) * (sin (x<n>) - a cos (x<n>)) */
    MP_T *tmp, *z_0, *sns, *cns, *one;
    int decimals, digits_h;
    STACK_MP (tmp, p, digits_g);
    STACK_MP (z_0, p, digits_g);
    STACK_MP (sns, p, digits_g);
    STACK_MP (cns, p, digits_g);
    STACK_MP (one, p, digits_g);
    SET_MP_ZERO (z_0, digits_g);
    (void) set_mp_short (one, (MP_T) 1, 0, digits_g);
/* Construct an estimate */
    (void) real_to_mp (p, z_g, atan (mp_to_real (p, x_g, digits_g)), digits_g);
    decimals = DOUBLE_ACCURACY;
    do {
      decimals <<= 1;
      digits_h = MINIMUM (1 + decimals / LOG_MP_BASE, digits_g);
      (void) sin_mp (p, sns, z_g, digits_h);
      (void) mul_mp (p, tmp, sns, sns, digits_h);
      (void) sub_mp (p, tmp, one, tmp, digits_h);
      (void) sqrt_mp (p, cns, tmp, digits_h);
      (void) mul_mp (p, tmp, x_g, cns, digits_h);
      (void) sub_mp (p, tmp, sns, tmp, digits_h);
      (void) mul_mp (p, tmp, tmp, cns, digits_h);
      (void) sub_mp (p, z_g, z_g, tmp, digits_h);
    } while (decimals < digits_g * LOG_MP_BASE);
  }
  if (flip) {
    MP_T *hpi;
    STACK_MP (hpi, p, digits_g);
    (void) sub_mp (p, z_g, mp_pi (p, hpi, MP_HALF_PI, digits_g), z_g, digits_g);
  }
  (void) shorten_mp (p, z, digits, z_g, digits_g);
  MP_DIGIT (z, 1) = (negative ? -MP_DIGIT (z, 1) : MP_DIGIT (z, 1));
/* Exit */
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "z" to atan2 ("x", "y")
\param p position in tree
\param z mp number
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return result "z"
**/

MP_T *atan2_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *t;
  STACK_MP (t, p, digits);
  if (MP_DIGIT (x, 1) == 0 && MP_DIGIT (y, 1) == 0) {
    errno = EDOM;
    stack_pointer = pop_sp;
    return (NO_MP);
  } else {
    BOOL_T flip = (BOOL_T) (MP_DIGIT (y, 1) < 0);
    MP_DIGIT (y, 1) = ABS (MP_DIGIT (y, 1));
    if (IS_ZERO_MP (x)) {
      (void) mp_pi (p, z, MP_HALF_PI, digits);
    } else {
      BOOL_T flop = (BOOL_T) (MP_DIGIT (x, 1) <= 0);
      MP_DIGIT (x, 1) = ABS (MP_DIGIT (x, 1));
      (void) div_mp (p, z, y, x, digits);
      (void) atan_mp (p, z, z, digits);
      if (flop) {
        (void) mp_pi (p, t, MP_PI, digits);
        (void) sub_mp (p, z, t, z, digits);
      }
    }
    if (flip) {
      MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
    }
  }
  stack_pointer = pop_sp;
  return (z);
}

/*!
\brief set "a" I "b" to "a" I "b" * "c" I "d"
\param p position in tree
\param a real mp number
\param b imaginary mp number
\param c real mp number
\param d imaginary mp number
\param digits precision in mp-digits
\return real part of result
**/

MP_T *cmul_mp (NODE_T * p, MP_T * a, MP_T * b, MP_T * c, MP_T * d, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  int digits_g = FUN_DIGITS (digits);
  MP_T *la, *lb, *lc, *ld, *ac, *bd, *ad, *bc;
  STACK_MP (la, p, digits_g);
  STACK_MP (lb, p, digits_g);
  STACK_MP (lc, p, digits_g);
  STACK_MP (ld, p, digits_g);
  (void) lengthen_mp (p, la, digits_g, a, digits);
  (void) lengthen_mp (p, lb, digits_g, b, digits);
  (void) lengthen_mp (p, lc, digits_g, c, digits);
  (void) lengthen_mp (p, ld, digits_g, d, digits);
  STACK_MP (ac, p, digits_g);
  STACK_MP (bd, p, digits_g);
  STACK_MP (ad, p, digits_g);
  STACK_MP (bc, p, digits_g);
  (void) mul_mp (p, ac, la, lc, digits_g);
  (void) mul_mp (p, bd, lb, ld, digits_g);
  (void) mul_mp (p, ad, la, ld, digits_g);
  (void) mul_mp (p, bc, lb, lc, digits_g);
  (void) sub_mp (p, la, ac, bd, digits_g);
  (void) add_mp (p, lb, ad, bc, digits_g);
  (void) shorten_mp (p, a, digits, la, digits_g);
  (void) shorten_mp (p, b, digits, lb, digits_g);
  stack_pointer = pop_sp;
  return (a);
}

/*!
\brief set "a" I "b" to "a" I "b" / "c" I "d"
\param p position in tree
\param a real mp number
\param b imaginary mp number
\param c real mp number
\param d imaginary mp number
\param digits precision in mp-digits
\return real part of result
**/

MP_T *cdiv_mp (NODE_T * p, MP_T * a, MP_T * b, MP_T * c, MP_T * d, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *q, *r;
  if (MP_DIGIT (c, 1) == (MP_T) 0 && MP_DIGIT (d, 1) == (MP_T) 0) {
    errno = ERANGE;
    return (NO_MP);
  }
  STACK_MP (q, p, digits);
  STACK_MP (r, p, digits);
  MOVE_MP (q, c, digits);
  MOVE_MP (r, d, digits);
  MP_DIGIT (q, 1) = ABS (MP_DIGIT (q, 1));
  MP_DIGIT (r, 1) = ABS (MP_DIGIT (r, 1));
  (void) sub_mp (p, q, q, r, digits);
  if (MP_DIGIT (q, 1) >= 0) {
    if (div_mp (p, q, d, c, digits) == NO_MP) {
      errno = ERANGE;
      return (NO_MP);
    }
    (void) mul_mp (p, r, d, q, digits);
    (void) add_mp (p, r, r, c, digits);
    (void) mul_mp (p, c, b, q, digits);
    (void) add_mp (p, c, c, a, digits);
    (void) div_mp (p, c, c, r, digits);
    (void) mul_mp (p, d, a, q, digits);
    (void) sub_mp (p, d, b, d, digits);
    (void) div_mp (p, d, d, r, digits);
  } else {
    if (div_mp (p, q, c, d, digits) == NO_MP) {
      errno = ERANGE;
      return (NO_MP);
    }
    (void) mul_mp (p, r, c, q, digits);
    (void) add_mp (p, r, r, d, digits);
    (void) mul_mp (p, c, a, q, digits);
    (void) add_mp (p, c, c, b, digits);
    (void) div_mp (p, c, c, r, digits);
    (void) mul_mp (p, d, b, q, digits);
    (void) sub_mp (p, d, d, a, digits);
    (void) div_mp (p, d, d, r, digits);
  }
  MOVE_MP (a, c, digits);
  MOVE_MP (b, d, digits);
  stack_pointer = pop_sp;
  return (a);
}

/*!
\brief set "r" I "i" to sqrt ("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *csqrt_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *re, *im;
  int digits_g = FUN_DIGITS (digits);
  STACK_MP (re, p, digits_g);
  STACK_MP (im, p, digits_g);
  (void) lengthen_mp (p, re, digits_g, r, digits);
  (void) lengthen_mp (p, im, digits_g, i, digits);
  if (IS_ZERO_MP (re) && IS_ZERO_MP (im)) {
    SET_MP_ZERO (re, digits_g);
    SET_MP_ZERO (im, digits_g);
  } else {
    MP_T *c1, *t, *x, *y, *u, *v, *w;
    STACK_MP (c1, p, digits_g);
    STACK_MP (t, p, digits_g);
    STACK_MP (x, p, digits_g);
    STACK_MP (y, p, digits_g);
    STACK_MP (u, p, digits_g);
    STACK_MP (v, p, digits_g);
    STACK_MP (w, p, digits_g);
    (void) set_mp_short (c1, (MP_T) 1, 0, digits_g);
    MOVE_MP (x, re, digits_g);
    MOVE_MP (y, im, digits_g);
    MP_DIGIT (x, 1) = ABS (MP_DIGIT (x, 1));
    MP_DIGIT (y, 1) = ABS (MP_DIGIT (y, 1));
    (void) sub_mp (p, w, x, y, digits_g);
    if (MP_DIGIT (w, 1) >= 0) {
      (void) div_mp (p, t, y, x, digits_g);
      (void) mul_mp (p, v, t, t, digits_g);
      (void) add_mp (p, u, c1, v, digits_g);
      (void) sqrt_mp (p, v, u, digits_g);
      (void) add_mp (p, u, c1, v, digits_g);
      (void) half_mp (p, v, u, digits_g);
      (void) sqrt_mp (p, u, v, digits_g);
      (void) sqrt_mp (p, v, x, digits_g);
      (void) mul_mp (p, w, u, v, digits_g);
    } else {
      (void) div_mp (p, t, x, y, digits_g);
      (void) mul_mp (p, v, t, t, digits_g);
      (void) add_mp (p, u, c1, v, digits_g);
      (void) sqrt_mp (p, v, u, digits_g);
      (void) add_mp (p, u, t, v, digits_g);
      (void) half_mp (p, v, u, digits_g);
      (void) sqrt_mp (p, u, v, digits_g);
      (void) sqrt_mp (p, v, y, digits_g);
      (void) mul_mp (p, w, u, v, digits_g);
    }
    if (MP_DIGIT (re, 1) >= 0) {
      MOVE_MP (re, w, digits_g);
      (void) add_mp (p, u, w, w, digits_g);
      (void) div_mp (p, im, im, u, digits_g);
    } else {
      if (MP_DIGIT (im, 1) < 0) {
        MP_DIGIT (w, 1) = -MP_DIGIT (w, 1);
      }
      (void) add_mp (p, v, w, w, digits_g);
      (void) div_mp (p, re, im, v, digits_g);
      MOVE_MP (im, w, digits_g);
    }
  }
  (void) shorten_mp (p, r, digits, re, digits_g);
  (void) shorten_mp (p, i, digits, im, digits_g);
  stack_pointer = pop_sp;
  return (r);
}

/*!
\brief set "r" I "i" to exp("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *cexp_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *re, *im, *u;
  int digits_g = FUN_DIGITS (digits);
  STACK_MP (re, p, digits_g);
  STACK_MP (im, p, digits_g);
  (void) lengthen_mp (p, re, digits_g, r, digits);
  (void) lengthen_mp (p, im, digits_g, i, digits);
  STACK_MP (u, p, digits_g);
  (void) exp_mp (p, u, re, digits_g);
  (void) cos_mp (p, re, im, digits_g);
  (void) sin_mp (p, im, im, digits_g);
  (void) mul_mp (p, re, re, u, digits_g);
  (void) mul_mp (p, im, im, u, digits_g);
  (void) shorten_mp (p, r, digits, re, digits_g);
  (void) shorten_mp (p, i, digits, im, digits_g);
  stack_pointer = pop_sp;
  return (r);
}

/*!
\brief set "r" I "i" to ln ("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *cln_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *re, *im, *u, *v, *s, *t;
  int digits_g = FUN_DIGITS (digits);
  STACK_MP (re, p, digits_g);
  STACK_MP (im, p, digits_g);
  (void) lengthen_mp (p, re, digits_g, r, digits);
  (void) lengthen_mp (p, im, digits_g, i, digits);
  STACK_MP (s, p, digits_g);
  STACK_MP (t, p, digits_g);
  STACK_MP (u, p, digits_g);
  STACK_MP (v, p, digits_g);
  MOVE_MP (u, re, digits_g);
  MOVE_MP (v, im, digits_g);
  (void) hypot_mp (p, s, u, v, digits_g);
  MOVE_MP (u, re, digits_g);
  MOVE_MP (v, im, digits_g);
  (void) atan2_mp (p, t, u, v, digits_g);
  (void) ln_mp (p, re, s, digits_g);
  MOVE_MP (im, t, digits_g);
  (void) shorten_mp (p, r, digits, re, digits_g);
  (void) shorten_mp (p, i, digits, im, digits_g);
  stack_pointer = pop_sp;
  return (r);
}

/*!
\brief set "r" I "i" to sin ("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *csin_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *re, *im, *s, *c, *sh, *ch;
  int digits_g = FUN_DIGITS (digits);
  STACK_MP (re, p, digits_g);
  STACK_MP (im, p, digits_g);
  (void) lengthen_mp (p, re, digits_g, r, digits);
  (void) lengthen_mp (p, im, digits_g, i, digits);
  STACK_MP (s, p, digits_g);
  STACK_MP (c, p, digits_g);
  STACK_MP (sh, p, digits_g);
  STACK_MP (ch, p, digits_g);
  if (IS_ZERO_MP (im)) {
    (void) sin_mp (p, re, re, digits_g);
    SET_MP_ZERO (im, digits_g);
  } else {
    (void) sin_mp (p, s, re, digits_g);
    (void) cos_mp (p, c, re, digits_g);
    (void) hyp_mp (p, sh, ch, im, digits_g);
    (void) mul_mp (p, re, s, ch, digits_g);
    (void) mul_mp (p, im, c, sh, digits_g);
  }
  (void) shorten_mp (p, r, digits, re, digits_g);
  (void) shorten_mp (p, i, digits, im, digits_g);
  stack_pointer = pop_sp;
  return (r);
}

/*!
\brief set "r" I "i" to cos ("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *ccos_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *re, *im, *s, *c, *sh, *ch;
  int digits_g = FUN_DIGITS (digits);
  STACK_MP (re, p, digits_g);
  STACK_MP (im, p, digits_g);
  (void) lengthen_mp (p, re, digits_g, r, digits);
  (void) lengthen_mp (p, im, digits_g, i, digits);
  STACK_MP (s, p, digits_g);
  STACK_MP (c, p, digits_g);
  STACK_MP (sh, p, digits_g);
  STACK_MP (ch, p, digits_g);
  if (IS_ZERO_MP (im)) {
    (void) cos_mp (p, re, re, digits_g);
    SET_MP_ZERO (im, digits_g);
  } else {
    (void) sin_mp (p, s, re, digits_g);
    (void) cos_mp (p, c, re, digits_g);
    (void) hyp_mp (p, sh, ch, im, digits_g);
    MP_DIGIT (sh, 1) = -MP_DIGIT (sh, 1);
    (void) mul_mp (p, re, c, ch, digits_g);
    (void) mul_mp (p, im, s, sh, digits_g);
  }
  (void) shorten_mp (p, r, digits, re, digits_g);
  (void) shorten_mp (p, i, digits, im, digits_g);
  stack_pointer = pop_sp;
  return (r);
}

/*!
\brief set "r" I "i" to tan ("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *ctan_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *s, *t, *u, *v;
  RESET_ERRNO;
  STACK_MP (s, p, digits);
  STACK_MP (t, p, digits);
  STACK_MP (u, p, digits);
  STACK_MP (v, p, digits);
  MOVE_MP (u, r, digits);
  MOVE_MP (v, i, digits);
  (void) csin_mp (p, u, v, digits);
  MOVE_MP (s, u, digits);
  MOVE_MP (t, v, digits);
  MOVE_MP (u, r, digits);
  MOVE_MP (v, i, digits);
  (void) ccos_mp (p, u, v, digits);
  (void) cdiv_mp (p, s, t, u, v, digits);
  MOVE_MP (r, s, digits);
  MOVE_MP (i, t, digits);
  stack_pointer = pop_sp;
  return (r);
}

/*!
\brief set "r" I "i" to asin ("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *casin_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *re, *im;
  int digits_g = FUN_DIGITS (digits);
  STACK_MP (re, p, digits_g);
  STACK_MP (im, p, digits_g);
  (void) lengthen_mp (p, re, digits_g, r, digits);
  (void) lengthen_mp (p, im, digits_g, i, digits);
  if (IS_ZERO_MP (im)) {
    (void) asin_mp (p, re, re, digits_g);
  } else {
    MP_T *c1, *u, *v, *a, *b;
    STACK_MP (c1, p, digits_g);
    (void) set_mp_short (c1, (MP_T) 1, 0, digits_g);
    STACK_MP (u, p, digits_g);
    STACK_MP (v, p, digits_g);
    STACK_MP (a, p, digits_g);
    STACK_MP (b, p, digits_g);
/* u=sqrt((r+1)^2+i^2), v=sqrt((r-1)^2+i^2) */
    (void) add_mp (p, a, re, c1, digits_g);
    (void) sub_mp (p, b, re, c1, digits_g);
    (void) hypot_mp (p, u, a, im, digits_g);
    (void) hypot_mp (p, v, b, im, digits_g);
/* a=(u+v)/2, b=(u-v)/2 */
    (void) add_mp (p, a, u, v, digits_g);
    (void) half_mp (p, a, a, digits_g);
    (void) sub_mp (p, b, u, v, digits_g);
    (void) half_mp (p, b, b, digits_g);
/* r=asin(b), i=ln(a+sqrt(a^2-1)) */
    (void) mul_mp (p, u, a, a, digits_g);
    (void) sub_mp (p, u, u, c1, digits_g);
    (void) sqrt_mp (p, u, u, digits_g);
    (void) add_mp (p, u, a, u, digits_g);
    (void) ln_mp (p, im, u, digits_g);
    (void) asin_mp (p, re, b, digits_g);
  }
  (void) shorten_mp (p, r, digits, re, digits_g);
  (void) shorten_mp (p, i, digits, im, digits_g);
  stack_pointer = pop_sp;
  return (re);
}

/*!
\brief set "r" I "i" to acos ("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *cacos_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *re, *im;
  int digits_g = FUN_DIGITS (digits);
  STACK_MP (re, p, digits_g);
  STACK_MP (im, p, digits_g);
  (void) lengthen_mp (p, re, digits_g, r, digits);
  (void) lengthen_mp (p, im, digits_g, i, digits);
  if (IS_ZERO_MP (im)) {
    (void) acos_mp (p, re, re, digits_g);
  } else {
    MP_T *c1, *u, *v, *a, *b;
    STACK_MP (c1, p, digits_g);
    (void) set_mp_short (c1, (MP_T) 1, 0, digits_g);
    STACK_MP (u, p, digits_g);
    STACK_MP (v, p, digits_g);
    STACK_MP (a, p, digits_g);
    STACK_MP (b, p, digits_g);
/* u=sqrt((r+1)^2+i^2), v=sqrt((r-1)^2+i^2) */
    (void) add_mp (p, a, re, c1, digits_g);
    (void) sub_mp (p, b, re, c1, digits_g);
    (void) hypot_mp (p, u, a, im, digits_g);
    (void) hypot_mp (p, v, b, im, digits_g);
/* a=(u+v)/2, b=(u-v)/2 */
    (void) add_mp (p, a, u, v, digits_g);
    (void) half_mp (p, a, a, digits_g);
    (void) sub_mp (p, b, u, v, digits_g);
    (void) half_mp (p, b, b, digits_g);
/* r=acos(b), i=-ln(a+sqrt(a^2-1)) */
    (void) mul_mp (p, u, a, a, digits_g);
    (void) sub_mp (p, u, u, c1, digits_g);
    (void) sqrt_mp (p, u, u, digits_g);
    (void) add_mp (p, u, a, u, digits_g);
    (void) ln_mp (p, im, u, digits_g);
    MP_DIGIT (im, 1) = -MP_DIGIT (im, 1);
    (void) acos_mp (p, re, b, digits_g);
  }
  (void) shorten_mp (p, r, digits, re, digits_g);
  (void) shorten_mp (p, i, digits, im, digits_g);
  stack_pointer = pop_sp;
  return (re);
}

/*!
\brief set "r" I "i" to atan ("r" I "i")
\param p position in tree
\param r mp real part
\param i mp imaginary part
\param digits precision in mp-digits
\return real part of result
**/

MP_T *catan_mp (NODE_T * p, MP_T * r, MP_T * i, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *re, *im, *u, *v;
  int digits_g = FUN_DIGITS (digits);
  STACK_MP (re, p, digits_g);
  STACK_MP (im, p, digits_g);
  (void) lengthen_mp (p, re, digits_g, r, digits);
  (void) lengthen_mp (p, im, digits_g, i, digits);
  STACK_MP (u, p, digits_g);
  STACK_MP (v, p, digits_g);
  if (IS_ZERO_MP (im)) {
    (void) atan_mp (p, u, re, digits_g);
    SET_MP_ZERO (v, digits_g);
  } else {
    MP_T *c1, *a, *b;
    STACK_MP (c1, p, digits_g);
    (void) set_mp_short (c1, (MP_T) 1, 0, digits_g);
    STACK_MP (a, p, digits_g);
    STACK_MP (b, p, digits_g);
/* a=sqrt(r^2+(i+1)^2), b=sqrt(r^2+(i-1)^2) */
    (void) add_mp (p, a, im, c1, digits_g);
    (void) sub_mp (p, b, im, c1, digits_g);
    (void) hypot_mp (p, u, re, a, digits_g);
    (void) hypot_mp (p, v, re, b, digits_g);
/* im=ln(a/b)/4 */
    (void) div_mp (p, u, u, v, digits_g);
    (void) ln_mp (p, u, u, digits_g);
    (void) half_mp (p, v, u, digits_g);
/* re=atan(2r/(1-r^2-i^2)) */
    (void) mul_mp (p, a, re, re, digits_g);
    (void) mul_mp (p, b, im, im, digits_g);
    (void) sub_mp (p, u, c1, a, digits_g);
    (void) sub_mp (p, b, u, b, digits_g);
    (void) add_mp (p, a, re, re, digits_g);
    (void) div_mp (p, a, a, b, digits_g);
    (void) atan_mp (p, u, a, digits_g);
    (void) half_mp (p, u, u, digits_g);
  }
  (void) shorten_mp (p, r, digits, u, digits_g);
  (void) shorten_mp (p, i, digits, v, digits_g);
  stack_pointer = pop_sp;
  return (re);
}

/*!
\brief comparison of "x" and "y"
\param p position in tree
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return whether x = y
**/

void eq_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *v;
  STACK_MP (v, p, digits);
  (void) sub_mp (p, v, x, y, digits);
  STATUS (z) = INITIALISED_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) == 0 ? A68_TRUE : A68_FALSE);
  stack_pointer = pop_sp;
}

/*!
\brief comparison of "x" and "y"
\param p position in tree
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return whether x != y
**/

void ne_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *v;
  STACK_MP (v, p, digits);
  (void) sub_mp (p, v, x, y, digits);
  STATUS (z) = INITIALISED_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) != 0 ? A68_TRUE : A68_FALSE);
  stack_pointer = pop_sp;
}

/*!
\brief comparison of "x" and "y"
\param p position in tree
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return whether x < y
**/

void lt_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *v;
  STACK_MP (v, p, digits);
  (void) sub_mp (p, v, x, y, digits);
  STATUS (z) = INITIALISED_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) < 0 ? A68_TRUE : A68_FALSE);
  stack_pointer = pop_sp;
}

/*!
\brief comparison of "x" and "y"
\param p position in tree
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return whether x <= y
**/

void le_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *v;
  STACK_MP (v, p, digits);
  (void) sub_mp (p, v, x, y, digits);
  STATUS (z) = INITIALISED_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) <= 0 ? A68_TRUE : A68_FALSE);
  stack_pointer = pop_sp;
}

/*!
\brief comparison of "x" and "y"
\param p position in tree
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return whether x > y
**/

void gt_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *v;
  STACK_MP (v, p, digits);
  (void) sub_mp (p, v, x, y, digits);
  STATUS (z) = INITIALISED_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) > 0 ? A68_TRUE : A68_FALSE);
  stack_pointer = pop_sp;
}

/*!
\brief comparison of "x" and "y"
\param p position in tree
\param x mp number
\param y mp number
\param digits precision in mp-digits
\return whether x >= y
**/

void ge_mp (NODE_T * p, A68_BOOL * z, MP_T * x, MP_T * y, int digits)
{
  ADDR_T pop_sp = stack_pointer;
  MP_T *v;
  STACK_MP (v, p, digits);
  (void) sub_mp (p, v, x, y, digits);
  STATUS (z) = INITIALISED_MASK;
  VALUE (z) = (MP_DIGIT (v, 1) >= 0 ? A68_TRUE : A68_FALSE);
  stack_pointer = pop_sp;
}

/*!
\brief rounding
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return round (x)
**/

MP_T *round_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  MP_T *y;
  STACK_MP (y, p, digits);
  (void) set_mp_short (y, (MP_T) (MP_RADIX / 2), -1, digits);
  if (MP_DIGIT (x, 1) >= 0) {
    (void) add_mp (p, z, x, y, digits);
    (void) trunc_mp (p, z, z, digits);
  } else {
    (void) sub_mp (p, z, x, y, digits);
    (void) trunc_mp (p, z, z, digits);
  }
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  return (z);
}

/*!
\brief rounding
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return entier (x)
**/

MP_T *entier_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  if (MP_DIGIT (x, 1) >= 0) {
    (void) trunc_mp (p, z, x, digits);
  } else {
    MP_T *y;
    STACK_MP (y, p, digits);
    MOVE_MP (y, z, digits);
    (void) trunc_mp (p, z, x, digits);
    (void) sub_mp (p, y, y, z, digits);
    if (MP_DIGIT (y, 1) != 0) {
      (void) set_mp_short (y, (MP_T) 1, 0, digits);
      (void) sub_mp (p, z, z, y, digits);
    }
  }
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  return (z);
}

/*!
\brief absolute value
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return |x|
**/

MP_T *abs_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  (void) p;
  if (x != z) {
    MOVE_MP (z, x, digits);
  }
  MP_DIGIT (z, 1) = fabs (MP_DIGIT (z, 1));
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  return (z);
}

/*!
\brief change sign
\param p position in tree
\param z mp number
\param x mp number
\param digits precision in mp-digits
\return -x
**/

MP_T *minus_mp (NODE_T * p, MP_T * z, MP_T * x, int digits)
{
  (void) p;
  if (x != z) {
    MOVE_MP (z, x, digits);
  }
  MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  MP_STATUS (z) = (MP_T) INITIALISED_MASK;
  return (z);
}
