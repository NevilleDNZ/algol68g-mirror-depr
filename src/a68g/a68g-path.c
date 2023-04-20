//! @file a68g-path.c
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
//! Low-level file path routines.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-genie.h"
#include "a68g-postulates.h"
#include "a68g-parser.h"
#include "a68g-options.h"
#include "a68g-optimiser.h"
#include "a68g-listing.h"

#if defined (BUILD_WIN32)
#include <windows.h>
#endif

//! @brief Safely get dir name from path.

char *a68_dirname (char *src)
{
  int len = (int) strlen (src) + 1;
  char *cpy = (char *) get_fixed_heap_space (len + 1);
  char *dst = (char *) get_fixed_heap_space (len + 1);
  ABEND (cpy == NO_TEXT, ERROR_OUT_OF_CORE, __func__);
  ABEND (dst == NO_TEXT, ERROR_OUT_OF_CORE, __func__);
  bufcpy (cpy, src, len);
  bufcpy (dst, dirname (cpy), len);
  return dst;
}

//! @brief Safely get basename from path.

char *a68_basename (char *src)
{
  int len = (int) strlen (src) + 1;
  char *cpy = (char *) get_fixed_heap_space (len + 1);
  char *dst = (char *) get_fixed_heap_space (len + 1);
  ABEND (cpy == NO_TEXT, ERROR_OUT_OF_CORE, __func__);
  ABEND (dst == NO_TEXT, ERROR_OUT_OF_CORE, __func__);
  bufcpy (cpy, src, len);
  bufcpy (dst, basename (cpy), len);
  return dst;
}

//! @brief Compute relative path.

#if defined (BUILD_WIN32)

static char *win32_slash (char *p)
{
  char *q = p;
  while (*p != '\0') {
    if (*p == '\\') {
      *p = '/';
    }
    p++;
  }
  return q;
}

static char *win32_realpath (char *name, char *resolved)
{
  char *res = NO_TEXT;
  if (name == NO_TEXT || name[0] == '\0') {
    return NO_TEXT;
  }
  if (resolved == NO_TEXT) {
    res = (char *) get_fixed_heap_space (PATH_MAX + 1);
    if (res == NO_TEXT) {
      return NO_TEXT;
    }
  } else {
    res = resolved;
  }
  int rc = GetFullPathName (name, PATH_MAX, res, (char **) NO_TEXT);
  if (rc == 0) {
    return NO_TEXT;
  } else {
    win32_slash (res);
    struct stat st;
    if (stat (res, &st) < 0) { // Should be 'lstat', but mingw does not have that.
      if (resolved == NO_TEXT) {
        free (res);
        return NO_TEXT;
      }
    }
  }
  return res;
}

#endif

char *a68_relpath (char *p1, char *p2, char *fn)
{
  char q[PATH_MAX + 1];
  bufcpy (q, p1, PATH_MAX);
  bufcat (q, "/", PATH_MAX);
  bufcat (q, p2, PATH_MAX);
  bufcat (q, "/", PATH_MAX);
  bufcat (q, fn, PATH_MAX);
// Home directory shortcut ~ is a shell extension.
  if (strchr (q, '~') != NO_TEXT) {
    return NO_TEXT;
  }
  char *r = (char *) get_fixed_heap_space (PATH_MAX + 1);
  ABEND (r == NO_TEXT, ERROR_OUT_OF_CORE, __func__);
// Error handling in the caller!
  errno = 0;
#if defined (BUILD_WIN32)
  r = win32_realpath (q, NO_TEXT);
#else
  r = realpath (q, NO_TEXT);
#endif
  return r;
}

//! @brief PROC (STRING) STRING realpath

void genie_realpath (NODE_T * p)
{
  A68_REF str;
  char in[PATH_MAX + 1];
  char * out;
  POP_REF (p, &str);
  if (a_to_c_string (p, in, str) == NO_TEXT) {
    PUSH_REF (p, empty_string (p));
  } else {
// Note that ~ is not resolved since that is the shell, not libc.
#if defined (BUILD_WIN32)
    out = win32_realpath (in, NO_TEXT);
#else
    out = realpath (in, NO_TEXT);
#endif
    if (out == NO_TEXT) {
      PUSH_REF (p, empty_string (p));
    } else {
      PUSH_REF (p, c_to_a_string (p, out, PATH_MAX));
    }
  }
}

