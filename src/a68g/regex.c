//! @file regex.c
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

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-transput.h"

//! @brief grep in string (STRING, STRING, REF INT, REF INT) INT.

int grep_in_string (char *pat, char *str, int *start, int *end)
{
  int rc, nmatch, k, max_k, widest;
  regex_t compiled;
  regmatch_t *matches;
  rc = regcomp (&compiled, pat, REG_NEWLINE | REG_EXTENDED);
  if (rc != 0) {
    regfree (&compiled);
    return rc;
  }
  nmatch = (int) (RE_NSUB (&compiled));
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = a68_alloc ((size_t) (nmatch * SIZE_ALIGNED (regmatch_t)), __func__, __LINE__);
  if (nmatch > 0 && matches == NO_REGMATCH) {
    regfree (&compiled);
    return 2;
  }
  rc = regexec (&compiled, str, (size_t) nmatch, matches, 0);
  if (rc != 0) {
    regfree (&compiled);
    return rc;
  }
// Find widest match. Do not assume it is the first one.
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = (int) RM_EO (&matches[k]) - (int) RM_SO (&matches[k]);
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  if (start != NO_INT) {
    (*start) = (int) RM_SO (&matches[max_k]);
  }
  if (end != NO_INT) {
    (*end) = (int) RM_EO (&matches[max_k]);
  }
  a68_free (matches);
  return 0;
}

//! @brief Return code for regex interface.

void push_grep_rc (NODE_T * p, int rc)
{
  switch (rc) {
  case 0:
    {
      PUSH_VALUE (p, 0, A68_INT);
      return;
    }
  case REG_NOMATCH:
    {
      PUSH_VALUE (p, 1, A68_INT);
      return;
    }
  case REG_ESPACE:
    {
      PUSH_VALUE (p, 3, A68_INT);
      return;
    }
  default:
    {
      PUSH_VALUE (p, 2, A68_INT);
      return;
    }
  }
}

//! @brief PROC grep in string = (STRING, STRING, REF INT, REF INT) INT

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
  CHECK_INIT (p, INITIALISED (&row), M_ROWS);
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
  matches = a68_alloc ((size_t) (nmatch * SIZE_ALIGNED (regmatch_t)), __func__, __LINE__);
  if (nmatch > 0 && matches == NULL) {
    rc = 2;
    PUSH_VALUE (p, rc, A68_INT);
    regfree (&compiled);
    return;
  }
  rc = regexec (&compiled, get_transput_buffer (STRING_BUFFER), (size_t) nmatch, matches, 0);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
// Find widest match. Do not assume it is the first one.
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
  a68_free (matches);
  push_grep_rc (p, 0);
}

//! @brief PROC grep in substring = (STRING, STRING, REF INT, REF INT) INT

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
  CHECK_INIT (p, INITIALISED (&row), M_ROWS);
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
  matches = a68_alloc ((size_t) (nmatch * SIZE_ALIGNED (regmatch_t)), __func__, __LINE__);
  if (nmatch > 0 && matches == NULL) {
    rc = 2;
    PUSH_VALUE (p, rc, A68_INT);
    regfree (&compiled);
    return;
  }
  rc = regexec (&compiled, get_transput_buffer (STRING_BUFFER), (size_t) nmatch, matches, REG_NOTBOL);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
// Find widest match. Do not assume it is the first one.
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
  a68_free (matches);
  push_grep_rc (p, 0);
}

//! @brief PROC sub in string = (STRING, STRING, REF STRING) INT

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
    PUSH_VALUE (p, 3, A68_INT);
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
  matches = a68_alloc ((size_t) (nmatch * SIZE_ALIGNED (regmatch_t)), __func__, __LINE__);
  if (nmatch > 0 && matches == NULL) {
    PUSH_VALUE (p, rc, A68_INT);
    regfree (&compiled);
    return;
  }
  rc = regexec (&compiled, get_transput_buffer (STRING_BUFFER), (size_t) nmatch, matches, 0);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
// Find widest match. Do not assume it is the first one.
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
// Substitute text.
  txt = get_transput_buffer (STRING_BUFFER);
  for (k = 0; k < begin - 1; k++) {
    plusab_transput_buffer (p, REPLACE_BUFFER, txt[k]);
  }
  add_a_string_transput_buffer (p, REPLACE_BUFFER, (BYTE_T *) & ref_rep);
  for (k = end; k < get_transput_buffer_size (STRING_BUFFER); k++) {
    plusab_transput_buffer (p, REPLACE_BUFFER, txt[k]);
  }
  *DEREF (A68_REF, &ref_str) = c_to_a_string (p, get_transput_buffer (REPLACE_BUFFER), DEFAULT_WIDTH);
  a68_free (matches);
  push_grep_rc (p, 0);
}
