/*!
\file regex.c
\brief regular expression support
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2006 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#include "algol68g.h"
#include "genie.h"
#include "transput.h"

/*!
\brief PROC char in string = (CHAR, REF INT, STRING) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_char_in_string (NODE_T * p)
{
  A68_CHAR c;
  A68_INT pos;
  A68_REF ref_pos, ref_str;
  char *q, ch;
  int k, len;
  POP_REF (p, &ref_str);
  POP_REF (p, &ref_pos);
  POP_CHAR (p, &c);
  reset_transput_buffer (PATTERN_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_str);
  len = get_transput_buffer_index (PATTERN_BUFFER);
  q = get_transput_buffer (PATTERN_BUFFER);
  ch = VALUE (&c);
  for (k = 0; k < len; k++) {
    if (q[k] == ch) {
      STATUS (&pos) = INITIALISED_MASK;
      VALUE (&pos) = k + 1;
      *(A68_INT *) ADDRESS (&ref_pos) = pos;
      PUSH_BOOL (p, A_TRUE);
      return;
    }
  }
  PUSH_BOOL (p, A_FALSE);
}

/*!
\brief PROC last char in string = (CHAR, REF INT, STRING) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_last_char_in_string (NODE_T * p)
{
  A68_CHAR c;
  A68_INT pos;
  A68_REF ref_pos, ref_str;
  char *q, ch;
  int k, len;
  POP_REF (p, &ref_str);
  POP_REF (p, &ref_pos);
  POP_CHAR (p, &c);
  reset_transput_buffer (PATTERN_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_str);
  len = get_transput_buffer_index (PATTERN_BUFFER);
  q = get_transput_buffer (PATTERN_BUFFER);
  ch = VALUE (&c);
  for (k = len - 1; k >= 0; k--) {
    if (q[k] == ch) {
      STATUS (&pos) = INITIALISED_MASK;
      VALUE (&pos) = k + 1;
      *(A68_INT *) ADDRESS (&ref_pos) = pos;
      PUSH_BOOL (p, A_TRUE);
      return;
    }
  }
  PUSH_BOOL (p, A_FALSE);
}

/*!
\brief PROC string in string = (STRING, REF INT, STRING) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_string_in_string (NODE_T * p)
{
  A68_REF ref_pos, ref_str, ref_pat;
  char *q;
  POP_REF (p, &ref_str);
  POP_REF (p, &ref_pos);
  POP_REF (p, &ref_pat);
  reset_transput_buffer (PATTERN_BUFFER);
  reset_transput_buffer (STRING_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_pat);
  add_a_string_transput_buffer (p, STRING_BUFFER, (BYTE_T *) & ref_str);
  q = strstr (get_transput_buffer (STRING_BUFFER), get_transput_buffer (PATTERN_BUFFER));
  if (q != NULL) {
    if (!IS_NIL (ref_pos)) {
      A68_INT pos;
      STATUS (&pos) = INITIALISED_MASK;
/* ANSI standard leaves pointer difference undefined. */
      VALUE (&pos) = 1 + (int) get_transput_buffer_index (STRING_BUFFER) - (int) strlen (q);
      *(A68_INT *) ADDRESS (&ref_pos) = pos;
    }
    PUSH_BOOL (p, A_TRUE);
  } else {
    PUSH_BOOL (p, A_FALSE);
  }
}

#if defined HAVE_REGEX

#include <regex.h>

/*!
\brief return code for regex interface
\param rc return code from regex routine
\return 0: match, 1: no match, 2: no core, 3: other error
**/

void push_grep_rc (NODE_T * p, int rc)
{
  switch (rc) {
  case 0:
    {
      PUSH_INT (p, 0);
      return;
    }
  case REG_NOMATCH:
    {
      PUSH_INT (p, 1);
      return;
    }
  case REG_ESPACE:
    {
      PUSH_INT (p, 3);
      return;
    }
  default:
    {
      PUSH_INT (p, 2);
      return;
    }
  }
}

/*!
\brief PROC grep in string = (STRING, STRING, REF INT, REF INT) INT
\param p position in syntax tree, should not be NULL
\return 0: match, 1: no match, 2: no core, 3: other error
**/

void genie_grep_in_string (NODE_T * p)
{
  A68_REF ref_pat, ref_beg, ref_end, ref_str;
  int rc, nmatch, k, max_k, widest;
  regex_t compiled;
  regmatch_t *matches;
  POP_REF (p, &ref_end);
  POP_REF (p, &ref_beg);
  POP_REF (p, &ref_str);
  POP_REF (p, &ref_pat);
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
  nmatch = compiled.re_nsub;
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = malloc (nmatch * SIZE_OF (regmatch_t));
  if (nmatch > 0 && matches == NULL) {
    PUSH_INT (p, rc);
    regfree (&compiled);
    return;
  }
  rc = regexec (&compiled, get_transput_buffer (STRING_BUFFER), nmatch, matches, 0);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
/* Find widest match. Do not assume it is the first one. */
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = matches[k].rm_eo - (int) matches[k].rm_so;
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  if (!IS_NIL (ref_beg)) {
    A68_INT *i = (A68_INT *) ADDRESS (&ref_beg);
    STATUS (i) = INITIALISED_MASK;
    VALUE (i) = matches[max_k].rm_so + 1;
  }
  if (!IS_NIL (ref_end)) {
    A68_INT *i = (A68_INT *) ADDRESS (&ref_end);
    STATUS (i) = INITIALISED_MASK;
    VALUE (i) = matches[max_k].rm_eo;
  }
  free (matches);
  push_grep_rc (p, 0);
}

/*!
\brief PROC sub in string = (STRING, STRING, REF STRING) INT
\param p position in syntax tree, should not be NULL
\return 0: match, 1: no match, 2: no core, 3: other error
**/

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
    PUSH_INT (p, 3);
    return;
  }
  reset_transput_buffer (STRING_BUFFER);
  reset_transput_buffer (REPLACE_BUFFER);
  reset_transput_buffer (PATTERN_BUFFER);
  add_a_string_transput_buffer (p, PATTERN_BUFFER, (BYTE_T *) & ref_pat);
  add_a_string_transput_buffer (p, STRING_BUFFER, (BYTE_T *) (A68_REF *) ADDRESS (&ref_str));
  rc = regcomp (&compiled, get_transput_buffer (PATTERN_BUFFER), REG_NEWLINE | REG_EXTENDED);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
  nmatch = compiled.re_nsub;
  if (nmatch == 0) {
    nmatch = 1;
  }
  matches = malloc (nmatch * SIZE_OF (regmatch_t));
  if (nmatch > 0 && matches == NULL) {
    PUSH_INT (p, rc);
    regfree (&compiled);
    return;
  }
  rc = regexec (&compiled, get_transput_buffer (STRING_BUFFER), nmatch, matches, 0);
  if (rc != 0) {
    push_grep_rc (p, rc);
    regfree (&compiled);
    return;
  }
/* Find widest match. Do not assume it is the first one. */
  widest = 0;
  max_k = 0;
  for (k = 0; k < nmatch; k++) {
    int dif = matches[k].rm_eo - (int) matches[k].rm_so;
    if (dif > widest) {
      widest = dif;
      max_k = k;
    }
  }
  begin = matches[max_k].rm_so + 1;
  end = matches[max_k].rm_eo;
/* Substitute text. */
  txt = get_transput_buffer (STRING_BUFFER);
  for (k = 0; k < begin - 1; k++) {
    add_char_transput_buffer (p, REPLACE_BUFFER, txt[k]);
  }
  add_a_string_transput_buffer (p, REPLACE_BUFFER, (BYTE_T *) & ref_rep);
  for (k = end; k < get_transput_buffer_size (STRING_BUFFER); k++) {
    add_char_transput_buffer (p, REPLACE_BUFFER, txt[k]);
  }
  *(A68_REF *) ADDRESS (&ref_str) = c_to_a_string (p, get_transput_buffer (REPLACE_BUFFER));
  free (matches);
  push_grep_rc (p, 0);
}
#endif
