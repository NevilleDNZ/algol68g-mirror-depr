//! @file moids-diagnostic.c
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
//! MOID diagnostics routines.

#include "a68g.h"
#include "a68g-parser.h"
#include "a68g-prelude.h"
#include "a68g-moids.h"

//! @brief Give accurate error message.

char *mode_error_text (NODE_T * n, MOID_T * p, MOID_T * q, int context, int deflex, int depth)
{
#define TAIL(z) (&(z)[strlen (z)])
  static BUFFER txt;
  if (depth == 1) {
    txt[0] = NULL_CHAR;
  }
  if (IS (p, SERIES_MODE)) {
    PACK_T *u = PACK (p);
    int N = 0;
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
      N++;
    } else {
      for (; u != NO_PACK; FORWARD (u)) {
        if (MOID (u) != NO_MOID) {
          if (IS (MOID (u), SERIES_MODE)) {
            (void) mode_error_text (n, MOID (u), q, context, deflex, depth + 1);
          } else if (!is_coercible (MOID (u), q, context, deflex)) {
            int len = (int) strlen (txt);
            if (len > BUFFER_SIZE / 2) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
              N++;
            } else {
              if (strlen (txt) > 0) {
                ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
                N++;
              }
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "%s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
              N++;
            }
          }
        }
      }
    }
    if (depth == 1) {
      if (N == 0) {
        ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "mode") >= 0);
      }
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (q, MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (IS (p, STOWED_MODE) && IS_FLEX (q)) {
    PACK_T *u = PACK (p);
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NO_PACK; FORWARD (u)) {
        if (!is_coercible (MOID (u), SLICE (SUB (q)), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "%s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (SUB (q)), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (IS (p, STOWED_MODE) && IS (q, ROW_SYMBOL)) {
    PACK_T *u = PACK (p);
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NO_PACK; FORWARD (u)) {
        if (!is_coercible (MOID (u), SLICE (q), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "%s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (q), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (IS (p, STOWED_MODE) && (IS (q, PROC_SYMBOL) || IS (q, STRUCT_SYMBOL))) {
    PACK_T *u = PACK (p), *v = PACK (q);
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NO_PACK && v != NO_PACK; FORWARD (u), FORWARD (v)) {
        if (!is_coercible (MOID (u), MOID (v), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, "%s cannot be coerced to %s", moid_to_string (MOID (u), MOID_ERROR_WIDTH, n), moid_to_string (MOID (v), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
    }
  }
  return txt;
#undef TAIL
}

//! @brief Cannot coerce error.

void cannot_coerce (NODE_T * p, MOID_T * from, MOID_T * to, int context, int deflex, int att)
{
  char *txt = mode_error_text (p, from, to, context, deflex, 1);
  if (att == STOP) {
    if (strlen (txt) == 0) {
      diagnostic (A68_ERROR, p, "M cannot be coerced to M in C context", from, to, context);
    } else {
      diagnostic (A68_ERROR, p, "Y in C context", txt, context);
    }
  } else {
    if (strlen (txt) == 0) {
      diagnostic (A68_ERROR, p, "M cannot be coerced to M in C-A", from, to, context, att);
    } else {
      diagnostic (A68_ERROR, p, "Y in C-A", txt, context, att);
    }
  }
}

//! @brief Give a warning when a value is silently discarded.

void warn_for_voiding (NODE_T * p, SOID_T * x, SOID_T * y, int c)
{
  (void) c;
  if (CAST (x) == A68_FALSE) {
    if (MOID (x) == M_VOID && MOID (y) != M_ERROR && !(MOID (y) == M_VOID || !is_nonproc (MOID (y)))) {
      if (IS (p, FORMULA)) {
        diagnostic (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_VOIDED, MOID (y));
      } else {
        diagnostic (A68_WARNING, p, WARNING_VOIDED, MOID (y));
      }
    }
  }
}

//! @brief Warn for things that are likely unintended.

void semantic_pitfall (NODE_T * p, MOID_T * m, int c, int u)
{
// semantic_pitfall: warn for things that are likely unintended, for instance
//                   REF INT i := LOC INT := 0, which should probably be
//                   REF INT i = LOC INT := 0.
  if (IS (p, u)) {
    diagnostic (A68_WARNING, p, WARNING_UNINTENDED, MOID (p), u, m, c);
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, STOP)) {
    semantic_pitfall (SUB (p), m, c, u);
  }
}

