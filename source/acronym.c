/*!
\file acronym.c
\brief VMS style acronyms
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

/* This code was contributed by Theo Vosse.  */

#include "algol68g.h"
#include "genie.h"
#include "inline.h"

static BOOL_T is_vowel (char);
static BOOL_T is_consonant (char);
static int qsort_strcmp (const void *, const void *);
static BOOL_T is_coda (char *, int);
static void get_init_sylls (char *, char *);
static void reduce_vowels (char *);
static int error_length (char *);
static BOOL_T remove_extra_coda (char *);
static void make_acronym (char *, char *);

/*!
\brief whether ch is a vowel
\param ch character under test
\return same
**/

static BOOL_T is_vowel (char ch)
{
  return ((BOOL_T) (a68g_strchr ("aeiouAEIOU", ch) != NULL));
}

/*!
\brief whether ch is consonant
\param ch character under test
\return same
**/

static BOOL_T is_consonant (char ch)
{
  return ((BOOL_T) (a68g_strchr ("qwrtypsdfghjklzxcvbnmQWRTYPSDFGHJKLZXCVBNM", ch) != NULL));
}

static char *codas[] = {
  "BT", "CH", "CHS", "CHT", "CHTS", "CT", "CTS", "D", "DS", "DST",
  "DT", "F", "FD", "FDS", "FDST", "FDT", "FS", "FST", "FT", "FTS", "FTST",
  "G", "GD",
  "GDS", "GDST", "GDT", "GS", "GST", "GT", "H", "K", "KS", "KST", "KT",
  "KTS", "KTST", "L", "LD", "LDS", "LDST", "LDT", "LF", "LFD", "LFS", "LFT",
  "LG", "LGD", "LGT", "LK", "LKS", "LKT", "LM", "LMD", "LMS", "LMT", "LP",
  "LPS",
  "LPT", "LS", "LSD", "LST", "LT", "LTS",
  "LTST", "M", "MBT", "MBTS", "MD", "MDS", "MDST", "MDT", "MF",
  "MP", "MPT", "MPTS", "MPTST", "MS", "MST", "MT", "N",
  "ND", "NDR", "NDS", "NDST", "NDT", "NG", "NGD", "NGS",
  "NGST", "NGT", "NK", "NKS", "NKST", "NKT", "NS", "NSD", "NST",
  "NT", "NTS", "NTST", "NTZ", "NX", "P", "PS", "PST", "PT", "PTS", "PTST",
  "R", "RCH", "RCHT", "RD", "RDS", "RDST", "RDT",
  "RG", "RGD", "RGS", "RGT", "RK", "RKS", "RKT",
  "RLS", "RM", "RMD", "RMS", "RMT", "RN", "RND", "RNS", "RNST", "RNT",
  "RP", "RPS", "RPT", "RS", "RSD", "RST", "RT", "RTS",
  "S", "SC", "SCH", "SCHT", "SCS", "SD", "SK",
  "SKS", "SKST", "SKT", "SP", "SPT", "ST", "STS", "T", "TS", "TST", "W",
  "WD", "WDS", "WDST", "WS", "WST", "WT",
  "X", "XT"
};

/*!
\brief compare function to pass to bsearch
\param key key to search
\param data data to search in
\return difference between key and data
**/

static int qsort_strcmp (const void *key, const void *data)
{
  return (strcmp ((char *) key, *(char **) data));
}

/*!
\brief whether first characters of string are a coda
\param str string under test
\param len number of characters
\return same
**/

static BOOL_T is_coda (char *str, int len)
{
  char str2[BUFFER_SIZE];
  strncpy (str2, str, BUFFER_SIZE);
  str2[len] = NULL_CHAR;
  return ((BOOL_T) (bsearch (str2, codas, sizeof (codas) / sizeof (char *), sizeof (char *), qsort_strcmp) != NULL));
}

/*!
\brief get_init_sylls
\param in input string
\param out output string
**/

static void get_init_sylls (char *in, char *out)
{
  char *coda;
  while (*in != NULL_CHAR) {
    if (isalpha (*in)) {
      while (*in != NULL_CHAR && isalpha (*in) && !is_vowel (*in)) {
        *out++ = (char) toupper (*in++);
      }
      while (*in != NULL_CHAR && is_vowel (*in)) {
        *out++ = (char) toupper (*in++);
      }
      coda = out;
      while (*in != NULL_CHAR && is_consonant (*in)) {
        *out++ = (char) toupper (*in++);
        *out = NULL_CHAR;
        if (!is_coda (coda, out - coda)) {
          out--;
          break;
        }
      }
      while (*in != NULL_CHAR && isalpha (*in)) {
        in++;
      }
      *out++ = '+';
    } else {
      in++;
    }
  }
  out[-1] = NULL_CHAR;
}

/*!
\brief reduce vowels in string
\param str string
**/

static void reduce_vowels (char *str)
{
  char *next;
  while (*str != NULL_CHAR) {
    next = a68g_strchr (str + 1, '+');
    if (next == NULL) {
      break;
    }
    if (!is_vowel (*str) && is_vowel (next[1])) {
      while (str != next && !is_vowel (*str)) {
        str++;
      }
      if (str != next) {
        memmove (str, next, strlen (next) + 1);
      }
    } else {
      while (*str != NULL_CHAR && *str != '+')
        str++;
    }
    if (*str == '+') {
      str++;
    }
  }
}

/*!
\brief remove boundaries in string
\param str string
\param max_len maximym length
**/

static void remove_boundaries (char *str, int max_len)
{
  int len = 0;
  while (*str != NULL_CHAR) {
    if (len >= max_len) {
      *str = NULL_CHAR;
      return;
    }
    if (*str == '+') {
      memmove (str, str + 1, strlen (str + 1) + 1);
    } else {
      str++;
      len++;
    }
  }
}

/*!
\brief error_length
\param str string
\return same
**/

static int error_length (char *str)
{
  int len = 0;
  while (*str != NULL_CHAR) {
    if (*str != '+') {
      len++;
    }
    str++;
  }
  return (len);
}

/*!
\brief remove extra coda
\param str string
\return whether operation succeeded
**/

static BOOL_T remove_extra_coda (char *str)
{
  int len;
  while (*str != NULL_CHAR) {
    if (is_vowel (*str) && str[1] != '+' && !is_vowel (str[1]) && str[2] != '+' && str[2] != NULL_CHAR) {
      for (len = 2; str[len] != NULL_CHAR && str[len] != '+'; len++);
      memmove (str + 1, str + len, strlen (str + len) + 1);
      return (A68_TRUE);
    }
    str++;
  }
  return (A68_FALSE);
}

/*!
\brief make acronym
\param in input string
\param out output string
**/

static void make_acronym (char *in, char *out)
{
  get_init_sylls (in, out);
  reduce_vowels (out);
  while (error_length (out) > 8 && remove_extra_coda (out));
  remove_boundaries (out, 8);
}

/*!
\brief push acronym of string in stack
\param p position in tree
**/

void genie_acronym (NODE_T * p)
{
  A68_REF z;
  int len;
  char *u, *v;
  POP_REF (p, &z);
  len = a68_string_size (p, z);
  u = (char *) malloc ((size_t) (len + 1));
  v = (char *) malloc ((size_t) (len + 1 + 8));
  (void) a_to_c_string (p, u, z);
  if (u != NULL && u[0] != NULL_CHAR && v != NULL) {
    make_acronym (u, v);
    PUSH_REF (p, c_to_a_string (p, v));
  } else {
    PUSH_REF (p, empty_string (p));
  }
  if (u != NULL) {
    free (u);
  }
  if (v != NULL) {
    free (v);
  }
}
