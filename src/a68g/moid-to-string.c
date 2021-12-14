//! @file moid-to-string.c
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
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-genie.h"
#include "a68g-postulates.h"

// A pretty printer for moids.
// For example "PROC (REF STRUCT (REF SELF, UNION (INT, VOID))) REF SELF"
// for a procedure yielding a pointer to an object of its own mode.

static void moid_to_string_2 (char *, MOID_T *, int *, NODE_T *);

//! @brief Add string to MOID text.

static void add_to_moid_text (char *dst, char *str, int *w)
{
  bufcat (dst, str, BUFFER_SIZE);
  (*w) -= (int) strlen (str);
}

//! @brief Find a tag, searching symbol tables towards the root.

TAG_T *find_indicant_global (TABLE_T * table, MOID_T * mode)
{
  if (table != NO_TABLE) {
    TAG_T *s;
    for (s = INDICANTS (table); s != NO_TAG; FORWARD (s)) {
      if (MOID (s) == mode) {
        return s;
      }
    }
    return find_indicant_global (PREVIOUS (table), mode);
  } else {
    return NO_TAG;
  }
}

//! @brief Pack to string.

static void pack_to_string (char *b, PACK_T * p, int *w, BOOL_T text, NODE_T * idf)
{
  for (; p != NO_PACK; FORWARD (p)) {
    moid_to_string_2 (b, MOID (p), w, idf);
    if (text) {
      if (TEXT (p) != NO_TEXT) {
        add_to_moid_text (b, " ", w);
        add_to_moid_text (b, TEXT (p), w);
      }
    }
    if (p != NO_PACK && NEXT (p) != NO_PACK) {
      add_to_moid_text (b, ", ", w);
    }
  }
}

//! @brief Moid to string 2.

static void moid_to_string_2 (char *b, MOID_T * n, int *w, NODE_T * idf)
{
// Oops. Should not happen.
  if (n == NO_MOID) {
    add_to_moid_text (b, "null", w);;
    return;
  }
// Reference to self through REF or PROC.
  if (is_postulated (A68 (postulates), n)) {
    add_to_moid_text (b, "SELF", w);
    return;
  }
// If declared by a mode-declaration, present the indicant.
  if (idf != NO_NODE && !IS (n, STANDARD)) {
    TAG_T *indy = find_indicant_global (TABLE (idf), n);
    if (indy != NO_TAG) {
      add_to_moid_text (b, NSYMBOL (NODE (indy)), w);
      return;
    }
  }
// Write the standard modes.
  if (n == M_HIP) {
    add_to_moid_text (b, "HIP", w);
  } else if (n == M_ERROR) {
    add_to_moid_text (b, "ERROR", w);
  } else if (n == M_UNDEFINED) {
    add_to_moid_text (b, "unresolved", w);
  } else if (n == M_C_STRING) {
    add_to_moid_text (b, "C-STRING", w);
  } else if (n == M_COMPLEX || n == M_COMPL) {
    add_to_moid_text (b, "COMPLEX", w);
  } else if (n == M_LONG_COMPLEX || n == M_LONG_COMPL) {
    add_to_moid_text (b, "LONG COMPLEX", w);
  } else if (n == M_LONG_LONG_COMPLEX || n == M_LONG_LONG_COMPL) {
    add_to_moid_text (b, "LONG LONG COMPLEX", w);
  } else if (n == M_STRING) {
    add_to_moid_text (b, "STRING", w);
  } else if (n == M_PIPE) {
    add_to_moid_text (b, "PIPE", w);
  } else if (n == M_SOUND) {
    add_to_moid_text (b, "SOUND", w);
  } else if (n == M_COLLITEM) {
    add_to_moid_text (b, "COLLITEM", w);
  } else if (IS (n, IN_TYPE_MODE)) {
    add_to_moid_text (b, "\"SIMPLIN\"", w);
  } else if (IS (n, OUT_TYPE_MODE)) {
    add_to_moid_text (b, "\"SIMPLOUT\"", w);
  } else if (IS (n, ROWS_SYMBOL)) {
    add_to_moid_text (b, "\"ROWS\"", w);
  } else if (n == M_VACUUM) {
    add_to_moid_text (b, "\"VACUUM\"", w);
  } else if (IS (n, VOID_SYMBOL) || IS (n, STANDARD) || IS (n, INDICANT)) {
    if (DIM (n) > 0) {
      int k = DIM (n);
      if ((*w) >= k * (int) strlen ("LONG ") + (int) strlen (NSYMBOL (NODE (n)))) {
        while (k--) {
          add_to_moid_text (b, "LONG ", w);
        }
        add_to_moid_text (b, NSYMBOL (NODE (n)), w);
      } else {
        add_to_moid_text (b, "..", w);
      }
    } else if (DIM (n) < 0) {
      int k = -DIM (n);
      if ((*w) >= k * (int) strlen ("LONG ") + (int) strlen (NSYMBOL (NODE (n)))) {
        while (k--) {
          add_to_moid_text (b, "LONG ", w);
        }
        add_to_moid_text (b, NSYMBOL (NODE (n)), w);
      } else {
        add_to_moid_text (b, "..", w);
      }
    } else if (DIM (n) == 0) {
      add_to_moid_text (b, NSYMBOL (NODE (n)), w);
    }
// Write compounded modes.
  } else if (IS_REF (n)) {
    if ((*w) >= (int) strlen ("REF ..")) {
      add_to_moid_text (b, "REF ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "REF ..", w);
    }
  } else if (IS_FLEX (n)) {
    if ((*w) >= (int) strlen ("FLEX ..")) {
      add_to_moid_text (b, "FLEX ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "FLEX ..", w);
    }
  } else if (IS_ROW (n)) {
    int j = (int) strlen ("[] ..") + (DIM (n) - 1) * (int) strlen (",");
    if ((*w) >= j) {
      int k = DIM (n) - 1;
      add_to_moid_text (b, "[", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, "] ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else if (DIM (n) == 1) {
      add_to_moid_text (b, "[] ..", w);
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "[", w);
      while (k--) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, "] ..", w);
    }
  } else if (IS_STRUCT (n)) {
    int j = (int) strlen ("STRUCT ()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = A68 (postulates);
      make_postulate (&A68 (postulates), n, NO_MOID);
      add_to_moid_text (b, "STRUCT (", w);
      pack_to_string (b, PACK (n), w, A68_TRUE, idf);
      add_to_moid_text (b, ")", w);
      free_postulate_list (A68 (postulates), save);
      A68 (postulates) = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "STRUCT (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else if (IS_UNION (n)) {
    int j = (int) strlen ("UNION ()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = A68 (postulates);
      make_postulate (&A68 (postulates), n, NO_MOID);
      add_to_moid_text (b, "UNION (", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ")", w);
      free_postulate_list (A68 (postulates), save);
      A68 (postulates) = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "UNION (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else if (IS (n, PROC_SYMBOL) && DIM (n) == 0) {
    if ((*w) >= (int) strlen ("PROC ..")) {
      add_to_moid_text (b, "PROC ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "PROC ..", w);
    }
  } else if (IS (n, PROC_SYMBOL) && DIM (n) > 0) {
    int j = (int) strlen ("PROC () ..") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = A68 (postulates);
      make_postulate (&A68 (postulates), n, NO_MOID);
      add_to_moid_text (b, "PROC (", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ") ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
      free_postulate_list (A68 (postulates), save);
      A68 (postulates) = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "PROC (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ") ..", w);
    }
  } else if (IS (n, SERIES_MODE) || IS (n, STOWED_MODE)) {
    int j = (int) strlen ("()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      add_to_moid_text (b, "(", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ")", w);
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "(", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else {
    char str[SMALL_BUFFER_SIZE];
    ASSERT (snprintf (str, (size_t) SMALL_BUFFER_SIZE, "\\%d", ATTRIBUTE (n)) >= 0);
    add_to_moid_text (b, str, w);
  }
}

//! @brief Pretty-formatted mode "n"; "w" is a measure of width.

char *moid_to_string (MOID_T * n, int w, NODE_T * idf)
{
#define MAX_MTS 8
// We use a static buffer of MAX_MTS strings. This value 8 should be safe.
// No more than MAX_MTS calls can be pending in for instance printf.
// Instead we could allocate each string on the heap but that leaks memory. 
  static int mts_buff_ptr = 0;
  static char mts_buff[8][BUFFER_SIZE];
  char *a = &(mts_buff[mts_buff_ptr][0]);
  mts_buff_ptr++;
  if (mts_buff_ptr >= MAX_MTS) {
    mts_buff_ptr = 0;
  }
  a[0] = NULL_CHAR;
  if (w >= BUFFER_SIZE) {
    w = BUFFER_SIZE - 1;
  }
  A68 (postulates) = NO_POSTULATE;
  if (n != NO_MOID) {
    moid_to_string_2 (a, n, &w, idf);
  } else {
    bufcat (a, "null", BUFFER_SIZE);
  }
  return a;
#undef MAX_MTS
}
