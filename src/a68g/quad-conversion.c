//! @file quad-conversion.c
//!
//! @author (see below)
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
//! Fixed precision LONG LONG REAL/COMPLEX conversions.

// This code is based in part on the HPA Library, a branch of the CCMath library.
// The CCMath library and derived HPA Library are free software under the terms 
// of the GNU Lesser General Public License version 2.1 or any later version.
//
// Sources:
//   [https://directory.fsf.org/wiki/HPAlib]
//   [http://download-mirror.savannah.gnu.org/releases/hpalib/]
//   [http://savannah.nongnu.org/projects/hpalib]
//
//   Copyright 2000 Daniel A. Atkinson  [DanAtk@aol.com]
//   Copyright 2004 Ivano Primi         [ivprimi@libero.it]  
//   Copyright 2023 Marcel van der Veer [algol68g@xs4all.nl] - A68G version.

#include "a68g.h"

#if (A68_LEVEL >= 3)

#include "a68g-quad.h"

// Conversion.

//! @brief int_to_quad_real

QUAD_T int_to_quad_real (int n)
{
  REAL32 pe;
  unt_2 *pc, e;
  unt k, h;
  memset (pe, 0, sizeof (pe));
  k = ABS (n);
  pc = (unt_2 *) &k;
  if (n == 0) {
    return *(QUAD_T *) pe;
  }

#if (__BYTE_ORDER == __LITTLE_ENDIAN)
  pe[1] = *(pc + 1);
  pe[2] = *pc;
#else
  pe[1] = *pc;
  pe[2] = *(pc + 1);
#endif

  for (e = 0, h = 1; h <= k && e < ((8 * sizeof (unt)) - 1); h <<= 1, ++e);
  if (h <= k) {
    e += 1;
  }
  *pe = QUAD_REAL_BIAS + e - 1;
  if (n < 0) {
    *pe |= QUAD_REAL_M_SIGN;
  }
  lshift_quad_real ((8 * sizeof (unt)) - e, pe + 1, FLT256_LEN);
  return *(QUAD_T *) pe;
}

//! @brief quad_real_to_real

REAL_T quad_real_to_real (QUAD_T s)
{
  union {unt_2 pe[4]; REAL_T d;} v;
  unt_2 *pc, u;
  int_2 i, e;
  pc = (unt_2 *) &QV (s);
  u = *pc & QUAD_REAL_M_SIGN;
  e = (*pc & QUAD_REAL_M_EXP) - QUAD_REAL_DBL_BIAS;
  if (e >= QUAD_REAL_DBL_MAX) {
    return (!u ? DBL_MAX : -DBL_MAX);
  }
  if (e <= 0) {
    return 0.;
  }
  for (i = 0; i < 4; v.pe[i] = *++pc, i++);
  v.pe[0] &= QUAD_REAL_M_EXP;
  rshift_quad_real (QUAD_REAL_DBL_LEX - 1, v.pe, 4);
  v.pe[0] |= (e << (16 - QUAD_REAL_DBL_LEX));
  v.pe[0] |= u;
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
  u = v.pe[3];
  v.pe[3] = v.pe[0];
  v.pe[0] = u;
  u = v.pe[2];
  v.pe[2] = v.pe[1];
  v.pe[1] = u;
#endif
  return v.d;
}

//! @brief real_to_quad_real

QUAD_T real_to_quad_real (REAL_T y)
{
  REAL32 pe;
  unt_2 *pc, u;
  int_2 i, e;
  if (y < DBL_MIN && y > -DBL_MIN) {
    return QUAD_REAL_ZERO;
  }
  memset (pe, 0, sizeof (pe));
  pc = (unt_2 *) &y;
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
  pc += 3;
#endif
  u = *pc & QUAD_REAL_M_SIGN;
  e = QUAD_REAL_DBL_BIAS + ((*pc & QUAD_REAL_M_EXP) >> (16 - QUAD_REAL_DBL_LEX));
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
  for (i = 1; i < 5; pe[i] = *pc--, i++);
#else
  for (i = 1; i < 5; pe[i] = *pc++, i++);
#endif
  pc = pe + 1;
  lshift_quad_real (QUAD_REAL_DBL_LEX - 1, pc, 4);
  *pc |= QUAD_REAL_M_SIGN;
  *pe = e;
  *pe |= u;
  return *(QUAD_T *) pe;
}

//! @brief double_real_to_quad_real

QUAD_T double_real_to_quad_real (DOUBLE_T x)
{
  if (x == 0.0q) {
    return QUAD_REAL_ZERO;
  }
  int sinf = isinfq (x);
  if (sinf == 1) {
    return QUAD_REAL_PINF;
  } else if (sinf == -1) {
    return QUAD_REAL_MINF;
  }
  if (isnanq (x)) {
    return QUAD_REAL_NAN;
  }
  QUAD_T z = QUAD_REAL_ZERO;
  unt_2 *y = (unt_2 *) &x;
  for (unt i = 0; i < 8; i++) {
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
    QV (z)[i] = y[7 - i];
#else
    QV (z)[i] = y[i];
#endif
  }
// double_real skips the default first bit, HPA lib does not.
  unt_2 cy = 0x8000;
  for (unt i = 1; i < 8; i++) {
    if (QV (z)[i] & 0x1) {
      QV (z)[i] = (QV (z)[i] >> 1) | cy;
      cy = 0x8000;
    } else {
      QV (z)[i] = (QV (z)[i] >> 1) | cy;
      cy = 0x0;
    }
  }
  QV (z)[8] |= cy;
  return z;
}

DOUBLE_T quad_real_to_double_real (QUAD_T x)
{
  REAL16 y; REAL32 z;
  int i;
  memcpy (z, x.value, sizeof (QUAD_T));
// Catch NaN, +-Inf is handled correctly.
  if (isNaN_quad_real (&x)) {
    z[0] = 0x7fff;
    z[1] = 0xffff;
  }
// double_real skips the default first bit, HPA lib does not.
  unt_2 cy = (z[8] & 0x8000 ? 0x1 : 0x0);
  for (i = 7; i > 0; i--) {
    if (z[i] & 0x8000) {
      z[i] = (z[i] << 1) | cy;
      cy = 0x1;
    } else {
      z[i] = (z[i] << 1) | cy;
      cy = 0x0;
    }
  }
  for (i = 0; i < 8; i++) {
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
    y[i] = z[7 - i];
#else
    y[i] = z[i];
#endif
  }
// Avoid 'dereferencing type-punned pointer will break strict-aliasing rules'
  DOUBLE_T u;
  memcpy (&u, &y[0], sizeof (DOUBLE_T));
  return u;
}

//! @brief string_to_quad_real

QUAD_T string_to_quad_real (const char *q, char **endptr)
{
#define QUAD_UPB (QUAD_REAL_MAX_10EX + 100)
  QUAD_T s, f;
  REAL32 pc;
  unt_2 *pn, *pf, *pa, *pb;
  unt_2 sfg, ibex, fbex;
  unt n;
  int j;
  int idex = 0, fdex = 0, c, m;
  int_2 noip, nofp;
  const char *ptr;
  pn = (unt_2 *) &QV (s);
  pf = (unt_2 *) &QV (f);
  for (j = 0; j <= FLT256_LEN; pn[j] = pf[j] = pc[j] = 0, ++j);
  sfg = 0;
  m = FLT256_LEN + 1;
  if (endptr != NULL) {
    *endptr = (char *) q;
  }
// Skip leading white space
  while ((isspace (*q))) {
    q++;
  }
// Sign 
  if (*q == '+') {
    ++q;
  } else if (*q == '-') {
    sfg = 0x8000;
    ++q;
  }
// Integer part 
  for (ptr = q; (c = *q - '0') >= 0 && c <= 9; ++q) {
    if (pn[0]) {
      ++idex;
    } else {
      lshift_quad_real (1, pn, m);
      for (j = 0; j < m; ++j) {
        pc[j] = pn[j];
      }
      lshift_quad_real (2, pn, m);
      for (n = (unt) c, pa = pn + FLT256_LEN, pb = pc + FLT256_LEN; pa >= pn; pa--, pb--) {
        n += *pa + *pb;
        *pa = n;
        n >>= 16;
      }
    }
  }
  for (j = 0; j < m && pn[j] == 0; ++j);
  if (j == m) {
    ibex = 0;
  } else {
    ibex = QUAD_REAL_BIAS + QUAD_REAL_MAX_P - 1;
    if (j != 0) {
      j <<= 4;
      ibex -= j;
      lshift_quad_real (j, pn, m);
    }
    while (pn[0]) {
      rshift_quad_real (1, pn, m);
      ++ibex;
    }
    pn[0] = ibex | sfg;
  }
  noip = ptr == q;
// End Integer part 
  if (*q == '.') {
// Fractionary part 
    for (j = 0; j <= FLT256_LEN; ++j) {
      pc[j] = 0;
    }
    for (ptr = ++q; (c = *q - '0') >= 0 && c <= 9 && pf[0] == 0; --fdex, ++q) {
      lshift_quad_real (1, pf, m);
      for (j = 0; j < m; ++j) {
        pc[j] = pf[j];
      }
      lshift_quad_real (2, pf, m);
      for (n = (unt) c, pa = pf + FLT256_LEN, pb = pc + FLT256_LEN; pa >= pf; pa--, pb--) {
        n += *pa + *pb;
        *pa = n;
        n >>= 16;
      }
    }
    for (j = 0; j < m && pf[j] == 0; ++j);
    if (j == m) {
      fbex = 0;
    } else {
      fbex = QUAD_REAL_BIAS + QUAD_REAL_MAX_P - 1;
      if (j != 0) {
        j <<= 4;
        fbex -= j;
        lshift_quad_real (j, pf, m);
      }
      while (pf[0]) {
        rshift_quad_real (1, pf, m);
        ++fbex;
      }
      pf[0] = fbex | sfg;
    }
    nofp = ptr == q;
  } else {
    nofp = 1;
  }
  if (noip && nofp) {
    return QUAD_REAL_NAN;
  } else {
// Ignore digits beyond precision.
    for (; *q >= '0' && *q <= '9'; q++);
    if (endptr != NULL) {
      *endptr = (char *) q;
    }
  }
// Exponent 
  if (tolower (*q) == 'e' || tolower (*q) == 'q') {
    ++q;
    sfg = 0;
    if (*q == '+') {
      ++q;
    } else if (*q == '-') {
      sfg = 1;
      ++q;
    }
    for (ptr = q, j = 0; (c = *q - '0') >= 0 && c <= 9 && j <= QUAD_UPB; ++q) {
      j <<= 1;
      m = j;
      j <<= 2;
      j += c + m;
    }
    if (ptr != q && (endptr != 0)) {
      *endptr = (char *) q;
    }
    if (sfg != 0) {
      j = -j;
    }
    idex += j;
    fdex += j;
  }
// Remark: s and f have the same sign (see above). 
  if (idex > QUAD_REAL_MAX_10EX || fdex > QUAD_REAL_MAX_10EX) {
    return ((s.value[0] & QUAD_REAL_M_SIGN) ? QUAD_REAL_MINF : QUAD_REAL_PINF);
  } else {
    if (idex != 0) {
      s = mul_quad_real (s, ten_up_quad_real (idex));
    }
    if (fdex != 0) {
      f = mul_quad_real (f, ten_up_quad_real (fdex));
    }
    return _add_quad_real_ (s, f);
  }
#undef QUAD_UPB
}

//! @brief atox

QUAD_T atox (const char *q)
{
  return string_to_quad_real (q, NULL);
}

// _scale_ is set by nP in formats.

int _scale_ = 1;

//! @brief srecordf

int srecordf (char *s, const char *format, ...)
{
  size_t N = BUFFER_SIZE;
  va_list ap;
  va_start (ap, format);
  int vsnprintf (char *, size_t, const char *, va_list);
// Print in new string, just in case 's' is also an argument!
  int M = N + 16; // A bit longer so we trap too long prints.
  char *t = malloc (M);
  int Np = vsnprintf (t, M, format, ap);
  va_end (ap);
  if (Np >= N) {
    free (t);
    QUAD_RTE ("srecordf", "overflow");
  } else {
    strcpy (s, t);
    free (t);
  }
  return Np;
}

//! @brief strlcat

char *strlcat (char *dst, char *src, int len)
{
// Fortran string lengths are len+1, last one is for null.
  int avail = len - (int) strlen (dst);
  if (avail <= (int) strlen (src)) {
    QUAD_RTE ("_strlcat", "overflow");
  } else {
    strncat (dst, src, avail);
  }
  dst[len] = '\0';
  return dst;
}

//! @brief plusab

static char *plusab (char *buf, char c)
{
  char z[2];
  z[0] = c;
  z[1] = '\0';
  strlcat (buf, z, BUFFER_SIZE);
  return buf;
}

//! @brief plusto

static char *plusto (char ch, char *buf)
{
  memmove (&buf[1], &buf[0], strlen(buf) + 1);
  buf[0] = ch;
  return buf;
}

//! @brief leading_spaces

static char *leading_spaces (char *buf, int width)
{
  if (width > 0) {
    int j = ABS (width) - (int) strlen (buf);
    while (--j >= 0) {
      (void) plusto (' ', buf);
    }
  }
  return buf;
}

//! @brief error_chars

static char *error_chars (char *buf, int n)
{
  int k = (n != 0 ? ABS (n) : 1);
  buf[k] = '\0';
  while (--k >= 0) {
    buf[k] = ERROR_CHAR;
  }
  return buf;
}

//! @brief strputc_quad_real

void strputc_quad_real (char c, char *buffer)
{
// Assumes that buffer is initially filled with \0.
  char *ptr;
  for (ptr = buffer; *ptr != '\0'; ptr++);
  *ptr = c;
}

//! @brief sprintfmt_quad_real

void sprintfmt_quad_real (char *buffer, const char *fmt, ...)
{
  BUFFER ibuff;
  BUFCLR (ibuff);
  va_list ap;
  va_start (ap, fmt);
  vsprintf (ibuff, fmt, ap);
  va_end (ap);
  strcat (buffer, ibuff);
}

static int special_value (char *s, QUAD_T u, int sign)
{
  if ((isPinf_quad_real (&u))) {
    if (sign != 0) {
      *s++ = '+';
    }
    strcpy (s, "Inf");
    return 1;
  } else if ((isMinf_quad_real (&u))) {
    strcpy (s, "-Inf");
    return 1;
  } else if ((isNaN_quad_real (&u))) {
    if (sign != 0) {
      *s++ = '\?';
    }
    strcpy (s, "NaN");
    return 1;
  } else {
    return 0;
  }
}

//! @brief subfixed_quad_real

char *subfixed_quad_real (char *buffer, QUAD_T u, int sign, int digs)
{
  BUFCLR (buffer);
  if ((special_value (buffer, u, sign))) {
    return buffer;
  }
// Added for VIF to be compatible with fortran F format.
  QUAD_T t = abs_quad_real (u);
  while (ge_quad_real (t, QUAD_REAL_TEN)) {
    t = div_quad_real (t, QUAD_REAL_TEN);
    digs++;
  }
// -- end of addition.
  digs = MIN (ABS (digs), FLT256_DIG);
// Put sign and take abs value.
  unt_2 *pa = (unt_2 *) &QV (u);
  if ((*pa & QUAD_REAL_M_SIGN)) {
    *pa ^= QUAD_REAL_M_SIGN;
    strputc_quad_real ('-', buffer);
  } else if (sign) {
    strputc_quad_real ('+', buffer);
  }
// Zero as special case.
  int k;
  if ((is0_quad_real (&u))) {
    sprintfmt_quad_real (buffer, "0.");
    for (k = 0; k < digs; ++k) {
      strputc_quad_real ('0', buffer);
    }
    return buffer;
  }
// Reduce argument.
  int mag = ((*pa & QUAD_REAL_M_EXP) - QUAD_REAL_BIAS);
  mag = (int) ((REAL_T) (mag + 1) * M_LOG10_2);
  if ((mag != 0)) {
    u = mul_quad_real (u, pwr_quad_real (QUAD_REAL_TEN, -mag));
  }
  while ((*pa & QUAD_REAL_M_EXP) < QUAD_REAL_BIAS) {
    --mag;
    u = mul_quad_real (u, QUAD_REAL_TEN);
  }
  if (mag < 0) {
    digs = MAX (0, digs + mag);
  }
// We store digits as pseudo-chars.
  BUFFER q;
  BUFCLR (q);
  char *p = q;
  BUFCLR (q);
//
  int dig;
  for (*p = 0, k = 0; k <= digs; ++k) {
    u = sfmod_quad_real (u, &dig);
    *(++p) = (char) dig;
    if (*pa == 0) {
      break;
    }
    u = mul_quad_real (QUAD_REAL_TEN, u);
  }
// Round fraction
  if (*pa != 0) {
    u = sfmod_quad_real (u, &dig);
    if (dig >= 5) {
      ++(*p);
    }
    while (*p == 10) {
      *p = 0;
      ++(*--p);
    }
  }
  p = q;
  if (*p == 0) {
    ++p;
  } else {
    ++mag;
  }
// Now sprintf 
  if (mag > QUAD_REAL_MAX_10EX) {
    sprintfmt_quad_real (buffer, "Inf");
    return buffer;
  }
//
  if (mag >= 0) {
    for (k = 0; k <= mag; k++) {
      if (k <= digs) {
        strputc_quad_real ('0' + p[k], buffer);
      } else {
        strputc_quad_real ('0', buffer);
      }
    }
    if (k <= digs) {
      strputc_quad_real ('.', buffer);
      for (; k <= digs; k++) {
        strputc_quad_real ('0' + p[k], buffer);
      }
    }
  } else {
    sprintfmt_quad_real (buffer, "0.");
    for (k = 1; k < ABS (mag) && digs >= 0; k++) {
      strputc_quad_real ('0', buffer);
      digs--;
    }
    for (k = 0; k <= digs; ++k) {
      strputc_quad_real ('0' + *p++, buffer);
    }
  }
  return buffer;
}

//! @brief fixed_quad_real

char *fixed_quad_real (char *buf, QUAD_T x, int width, int digs, int precision)
{
  width = ABS (width);
  digs = MIN (abs (digs), precision);
  subfixed_quad_real (buf, x, 0, digs);
  if (width > 0 && strlen (buf) > width) {
    return error_chars (buf, width);
  } else {
    return leading_spaces (buf, width);
  }
}

//! @brief float_quad_real

char *float_quad_real (char *buf, QUAD_T z, int width, int digs, int expos, int mult, int precision, char sym)
{
  buf[0] = '\0';
  width = ABS (width);
  digs = MIN (abs (digs), precision);
  expos = ABS (expos);
  if (expos > 5) {
    return error_chars (buf, width);
  }
// Scientific notation mult = 1, Engineering notation mult = 3
  mult = MAX (1, mult);
// Default _scale_ is 1.
  int q = 1;
  char *max = "1";
  QUAD_T x = abs_quad_real (z), lwb, upb;
//
  if (_scale_ < 0 || _scale_ > 3) {
    _scale_ = 1;
  }
  if (mult == 1) {
    if (_scale_ == 0) {
      lwb = QUAD_REAL_TENTH;
      upb = QUAD_REAL_ONE;
      q = 1;
      max = "0.1";
    } else if (_scale_ == 1) {
      lwb = QUAD_REAL_ONE;
      upb = QUAD_REAL_TEN;
      q = 0;
      max = "1";
    } else if (_scale_ == 2) {
      lwb = QUAD_REAL_TEN;
      upb = QUAD_REAL_HUNDRED;
      q = -1;
      max = "10";
    } else if (_scale_ == 3) {
      lwb = QUAD_REAL_HUNDRED;
      upb = QUAD_REAL_THOUSAND;
      max = "100";
      q = -2;
    }
  }
// Standardize.
  int p = 0;
  if (not0_quad_real (&x)) {
    p = (int) round (quad_real_to_real (log10_quad_real (abs_quad_real(x)))) + q;
    x = div_quad_real (x, ten_up_quad_real (p));
    if (le_quad_real (x, lwb)) {
      x = mul_quad_real (x, QUAD_REAL_TEN);
      p--;
    } 
    if (ge_quad_real (x, upb)) {
      x = div_quad_real (x, QUAD_REAL_TEN);
      p++;
    } 
    while (p % mult != 0) {
      x = mul_quad_real (x, QUAD_REAL_TEN);
      p--;
    }
  }
// Form number.
  BUFFER mant;
  subfixed_quad_real (mant, x, 0, digs);
// Correction of rounding issue by which |mant| equals UPB.
  if (strchr (mant, '*') == NULL && ge_quad_real (abs_quad_real (string_to_quad_real (mant, NULL)), upb)) {
    srecordf (mant, "%s", max);
    if (digs > 0) {
      plusab (mant, '.');
      for (int k = 0; k < digs; k++) {
        plusab (mant, '0');
      }
    }
    p++;
  }
//
  BUFFER fmt;
  if (sgn_quad_real (&z) >= 0) {
    srecordf (fmt, " %%s%c%%+0%dd", sym, expos);
  } else {
    srecordf (fmt, "-%%s%c%%+0%dd", sym, expos);
  }
  srecordf (buf, fmt, mant, p);
  if (width > 0 && (strchr (buf, '*') != NULL || strlen (buf) > width)) {
    if (digs > 0) {
      return float_quad_real (buf, z, width, digs - 1, expos, mult, precision, sym);
    } else {
      return error_chars (buf, width);
    }
  } else {
    return leading_spaces (buf, width);
  }
}

#endif
