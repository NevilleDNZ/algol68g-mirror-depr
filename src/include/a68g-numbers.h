//! @file a68g-numbers.h
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2021 J. Marcel van der Veer <algol68g@xs4all.nl>.
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

#if !defined (__A68G_NUMBERS_H__)
#define __A68G_NUMBERS_H__

#define CONST_LOG2_10 3.32192809488736234787031942948939017586483139302458061205475640
#define CONST_PI_OVER_180 0.01745329251994329576923690768488612713442871888541725456097191
#define CONST_180_OVER_PI 57.2957795130823208767981548141051703324054724665643215491602439
#define CONST_PI_OVER_180_Q 0.01745329251994329576923690768488612713442871888541725456097191q
#define CONST_180_OVER_PI_Q 57.2957795130823208767981548141051703324054724665643215491602439q

// Abramowitz, Milton and Stegun, Irene A.
// Handbook of Mathematical Functions.
// New York:  Dover publications, Inc. (1970).
// All constants taken from this text are given to 25 significant digits.

#define CONST_E        2.718281828459045235360287471353    /* e */
#define CONST_EULER    0.577215664901532860606512090082    // Euler-Mascheroni
#define CONST_LOG2E    1.442695040888963407359924681002    /* log2(e) */
#define CONST_LOG10E   0.434294481903251827651128918917    /* log10(e) */
#define CONST_LN2      0.693147180559945309417232121458    /* ln(2) */
#define CONST_LN10     2.302585092994045684017991454684    /* ln(10) */
#define CONST_PI       3.141592653589793238462643383280    /* pi */
#define CONST_PI_Q     3.141592653589793238462643383280q   /* pi */
#define CONST_2PI      6.283185307179586476925286766559    /* 2*pi */
#define CONST_PI_2     1.570796326794896619231321691640    /* pi/2 */
#define CONST_PI_4     0.785398163397448309615660845820    /* pi/4 */
#define CONST_1_PI     0.318309886183790671537767526745    /* 1/pi */
#define CONST_2_PI     0.636619772367581343075535053490    /* 2/pi */
#define CONST_2_SQRTPI 1.128379167095512573896158903122    /* 2/sqrt(pi) */
#define CONST_SQRT2    1.414213562373095048801688724210    /* sqrt(2) */
#define CONST_SQRT1_2  0.707106781186547524400844362105    /* 1/sqrt(2) */

// R-Specific Constants

#define CONST_SQRT_3 1.732050807568877293527446341506   /* sqrt(3) */
#define CONST_SQRT_32 5.656854249492380195206754896838  /* sqrt(32) */
#define CONST_LOG10_2 0.301029995663981195213738894724  /* log10(2) */
#define CONST_SQRT_PI 1.772453850905516027298167483341  /* sqrt(pi) */
#define CONST_1_SQRT_2PI 0.398942280401432677939946059934       /* 1/sqrt(2pi) */
#define CONST_SQRT_2dPI 0.797884560802865355879892119869        /* sqrt(2/pi) */
#define CONST_LN_2PI 1.837877066409345483560659472811   /* log(2*pi) */
#define CONST_LN_SQRT_PI 0.572364942924700087071713675677       /* log(sqrt(pi)) == log(pi)/2 */
#define CONST_LN_SQRT_2PI 0.918938533204672741780329736406      /* log(sqrt(2*pi)) == log(2*pi)/2 */
#define CONST_LN_SQRT_PId2 0.225791352644727432363097614947     /* log(sqrt(pi/2)) */

#endif
