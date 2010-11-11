/*!
\file mips.c
\brief bogus mips rating
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

/* Bogus MIPS is the classic Whetsone rating */

#include "config.h"
#include "algol68g.h"
#include "interpreter.h"

static double t, t1, t2, e1[5];
static int j, k, l;

static void pa (double e[])
{
  for (j = 0; j < 6; j++) {
    e[1] = (e[1] + e[2] + e[3] - e[4]) * t;
    e[2] = (e[1] + e[2] - e[3] + e[4]) * t;
    e[3] = (e[1] - e[2] + e[3] + e[4]) * t;
    e[4] = (-e[1] + e[2] + e[3] + e[4]) / t2;
  }
}

static void p0 (void)
{
  e1[j] = e1[k];
  e1[k] = e1[l];
  e1[l] = e1[j];
}

static void p3 (double x, double y, double *z)
{
  double x1, y1;
  x1 = x;
  y1 = y;
  x1 = t * (x1 + y1);
  y1 = t * (x1 + y1);
  *z = (x1 + y1) / t2;
}

void bogus_mips (void)
{
  int i, n1, n2, n3, n4, n6, n7, n8, n9, n10, n11;
  double x1, x2, x3, x4, x, y, z;
  int loop, ii, jj;
  int takt = 4096;
  double time_0, elapsed, rate;
  do {
    time_0 = seconds ();
    t = .499975;
    t1 = 0.50025;
    t2 = 2.0;
/* With loopcount loop=10, 1MWhets is executed */
    loop = takt;
    for (ii = 1, jj = 1; jj <= ii; jj++) {
      n1 = 0;
      n2 = 12 * loop;
      n3 = 14 * loop;
      n4 = 345 * loop;
      n6 = 210 * loop;
      n7 = 32 * loop;
      n8 = 899 * loop;
      n9 = 616 * loop;
      n10 = 0;
      n11 = 93 * loop;
/* Module 1: Simple identifiers */
      x1 = 1.0;
      x2 = -1.0;
      x3 = -1.0;
      x4 = -1.0;
      for (i = 1; i <= n1; i++) {
	x1 = (x1 + x2 + x3 - x4) * t;
	x2 = (x1 + x2 - x3 + x4) * t;
	x3 = (x1 - x2 + x3 + x4) * t;
	x4 = (-x1 + x2 + x3 + x4) * t;
      }
/* Module 2: array elements */
      e1[1] = 1.0;
      e1[2] = -1.0;
      e1[3] = -1.0;
      e1[4] = -1.0;
      for (i = 1; i <= n2; i++) {
	e1[1] = (e1[1] + e1[2] + e1[3] - e1[4]) * t;
	e1[2] = (e1[1] + e1[2] - e1[3] + e1[4]) * t;
	e1[3] = (e1[1] - e1[2] + e1[3] + e1[4]) * t;
	e1[4] = (-e1[1] + e1[2] + e1[3] + e1[4]) * t;
      }
/* Module 3: array as parameter */
      for (i = 1; i <= n3; i++) {
	pa (e1);
      }
/* Module 4: Conditional jumps */
      j = 1;
      for (i = 1; i <= n4; i++) {
	if (j == 1) {
	  j = 2;
	}
	else {
	  j = 3;
	}
	if (j > 2) {
	  j = 0;
	}
	else {
	  j = 1;
	}
	if (j < 1) {
	  j = 1;
	}
	else {
	  j = 0;
	}
      }
/* Module 5: omitted */
/* Module 6: integer arithmetic */
      j = 1;
      k = 2;
      l = 3;
      for (i = 1; i <= n6; i++) {
	j = j * (k - j) * (l - k);
	k = l * k - (l - j) * k;
	l = (l - k) * (k + j);
	e1[l - 1] = j + k + l;
	e1[k - 1] = j * k * l;
      }
/* Module 7: trigonometric functions */
      x = 0.5;
      y = 0.5;
      for (i = 1; i <= n7; i++) {
	x = t * atan (t2 * sin (x) * cos (x) / (cos (x + y) + cos (x - y) - 1.0));
	y = t * atan (t2 * sin (y) * cos (y) / (cos (x + y) + cos (x - y) - 1.0));
      }
/* Module 8: procedure calls */
      x = 1.0;
      y = 1.0;
      z = 1.0;
      for (i = 1; i <= n8; i++) {
	p3 (x, y, &z);
      }
/* Module 9: array references */
      j = 1;
      k = 2;
      l = 3;
      e1[1] = 1.0;
      e1[2] = 2.0;
      e1[3] = 3.0;
      for (i = 1; i <= n9; i++) {
	p0 ();
      }
/* Module 10: integer arithmetic */
      j = 2;
      k = 3;
      for (i = 1; i <= n10; i++) {
	j = j + k;
	k = j + k;
	j = k - j;
	k = k - j - j;
      }
/* Module 11: Standard functions */
      x = 0.75;
      for (i = 1; i <= n11; i++) {
	x = sqrt (exp (log (x) / t1));
      }
    }
    elapsed = seconds () - time_0;
    if (elapsed <= 3) {
      takt *= 2;
    }
  } while (elapsed <= 3);
  rate = (loop * ii) / (elapsed) / 10;
  ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Bogus MIPS: %.0f\n", rate) >= 0);
  WRITE (STDOUT_FILENO, output_line);
}
