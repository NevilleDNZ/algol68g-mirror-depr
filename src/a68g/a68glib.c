//! @file a68glib.c
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

// This file consists in part of code licensed under LGPL.
// LGPL allows relicensing under GPL (see section 3 of the LGPL version 2.1,
// or section 2 option b of the LGPL version 3).
// This allows for reuse of LGPL code in A68G which is GPL code. 

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-double.h"
#include "a68g-numbers.h"

// Implement the snprintf function.
// Copyright (C) 2003-2020 Free Software Foundation, Inc.
// Written by Kaveh R. Ghazi <ghazi@caip.rutgers.edu>.
// Based on code from the GCC libiberty library which is covered by 
// the GNU GPL.

// Some pre-C99 system libraries do not implement snprintf correctly so one 
// cannot rely on the return value from a system version of this function.

int snprintf (char *s, size_t n, const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  int vsnprintf (char *, size_t, const char *, va_list);
  int result = vsnprintf (s, n, format, ap);
  va_end (ap);
  return result;
}

// @brief own memmove

void *a68_memmove (void *dest, void *src, size_t len)
{
  char *d = dest, *s = src;
  if (d < s) {
    while (len--) {
      *d++ = *s++;
    }
  } else {
    char *lasts = s + (len - 1), *lastd = d + (len - 1);
    while (len--) {
      *lastd-- = *lasts--;
    }
  }
  return dest;
}
