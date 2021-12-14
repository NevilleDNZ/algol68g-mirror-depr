//! @file listing.c
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
#include "a68g-listing.h"
#include "a68g-parser.h"
#include "a68g-optimiser.h"

// Routines for making a "fat" listing file.

#define SHOW_EQ A68_FALSE

//! @brief a68_print_short_mode.

static void a68_print_short_mode (FILE_T f, MOID_T * z)
{
  if (IS (z, STANDARD)) {
    int i = DIM (z);
    if (i > 0) {
      while (i--) {
        WRITE (f, "LONG ");
      }
    } else if (i < 0) {
      while (i++) {
        WRITE (f, "SHORT ");
      }
    }
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s", NSYMBOL (NODE (z))) >= 0);
    WRITE (f, A68 (output_line));
  } else if (IS_REF (z) && IS (SUB (z), STANDARD)) {
    WRITE (f, "REF ");
    a68_print_short_mode (f, SUB (z));
  } else if (IS (z, PROC_SYMBOL) && PACK (z) == NO_PACK && IS (SUB (z), STANDARD)) {
    WRITE (f, "PROC ");
    a68_print_short_mode (f, SUB (z));
  } else {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "#%d", NUMBER (z)) >= 0);
    WRITE (f, A68 (output_line));
  }
}

//! @brief A68g_print_flat_mode.

void a68_print_flat_mode (FILE_T f, MOID_T * z)
{
  if (IS (z, STANDARD)) {
    int i = DIM (z);
    if (i > 0) {
      while (i--) {
        WRITE (f, "LONG ");
      }
    } else if (i < 0) {
      while (i++) {
        WRITE (f, "SHORT ");
      }
    }
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s", NSYMBOL (NODE (z))) >= 0);
    WRITE (f, A68 (output_line));
  } else if (IS_REF (z)) {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "REF ") >= 0);
    WRITE (f, A68 (output_line));
    a68_print_short_mode (f, SUB (z));
  } else if (IS (z, PROC_SYMBOL) && DIM (z) == 0) {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "PROC ") >= 0);
    WRITE (f, A68 (output_line));
    a68_print_short_mode (f, SUB (z));
  } else if (IS_ROW (z)) {
    int i = DIM (z);
    WRITE (f, "[");
    while (--i) {
      WRITE (f, ", ");
    }
    WRITE (f, "] ");
    a68_print_short_mode (f, SUB (z));
  } else {
    a68_print_short_mode (f, z);
  }
}

//! @brief Brief_fields_flat.

static void a68_print_short_pack (FILE_T f, PACK_T * pack)
{
  if (pack != NO_PACK) {
    a68_print_short_mode (f, MOID (pack));
    if (NEXT (pack) != NO_PACK) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", ") >= 0);
      WRITE (f, A68 (output_line));
      a68_print_short_pack (f, NEXT (pack));
    }
  }
}

//! @brief A68g_print_mode.

void a68_print_mode (FILE_T f, MOID_T * z)
{
  if (z != NO_MOID) {
    if (IS (z, STANDARD)) {
      a68_print_flat_mode (f, z);
    } else if (IS (z, INDICANT)) {
      WRITE (f, NSYMBOL (NODE (z)));
    } else if (z == M_COLLITEM) {
      WRITE (f, "\"COLLITEM\"");
    } else if (IS_REF (z)) {
      WRITE (f, "REF ");
      a68_print_flat_mode (f, SUB (z));
    } else if (IS_FLEX (z)) {
      WRITE (f, "FLEX ");
      a68_print_flat_mode (f, SUB (z));
    } else if (IS_ROW (z)) {
      int i = DIM (z);
      WRITE (f, "[");
      while (--i) {
        WRITE (f, ", ");
      }
      WRITE (f, "] ");
      a68_print_flat_mode (f, SUB (z));
    } else if (IS_STRUCT (z)) {
      WRITE (f, "STRUCT (");
      a68_print_short_pack (f, PACK (z));
      WRITE (f, ")");
    } else if (IS_UNION (z)) {
      WRITE (f, "UNION (");
      a68_print_short_pack (f, PACK (z));
      WRITE (f, ")");
    } else if (IS (z, PROC_SYMBOL)) {
      WRITE (f, "PROC ");
      if (PACK (z) != NO_PACK) {
        WRITE (f, "(");
        a68_print_short_pack (f, PACK (z));
        WRITE (f, ") ");
      }
      a68_print_flat_mode (f, SUB (z));
    } else if (IS (z, IN_TYPE_MODE)) {
      WRITE (f, "\"SIMPLIN\"");
    } else if (IS (z, OUT_TYPE_MODE)) {
      WRITE (f, "\"SIMPLOUT\"");
    } else if (IS (z, ROWS_SYMBOL)) {
      WRITE (f, "\"ROWS\"");
    } else if (IS (z, SERIES_MODE)) {
      WRITE (f, "\"SERIES\" (");
      a68_print_short_pack (f, PACK (z));
      WRITE (f, ")");
    } else if (IS (z, STOWED_MODE)) {
      WRITE (f, "\"STOWED\" (");
      a68_print_short_pack (f, PACK (z));
      WRITE (f, ")");
    }
  }
}

//! @brief Print_mode_flat.

void print_mode_flat (FILE_T f, MOID_T * m)
{
  if (m != NO_MOID) {
    a68_print_mode (f, m);
    if (NODE (m) != NO_NODE && NUMBER (NODE (m)) > 0) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " node %d", NUMBER (NODE (m))) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (EQUIVALENT_MODE (m) != NO_MOID) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " equi #%d", NUMBER (EQUIVALENT (m))) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (SLICE (m) != NO_MOID) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " slice #%d", NUMBER (SLICE (m))) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (TRIM (m) != NO_MOID) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " trim #%d", NUMBER (TRIM (m))) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (ROWED (m) != NO_MOID) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " rowed #%d", NUMBER (ROWED (m))) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (DEFLEXED (m) != NO_MOID) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " deflex #%d", NUMBER (DEFLEXED (m))) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (MULTIPLE (m) != NO_MOID) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " multiple #%d", NUMBER (MULTIPLE (m))) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (NAME (m) != NO_MOID) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " name #%d", NUMBER (NAME (m))) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (USE (m)) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " used") >= 0);
      WRITE (f, A68 (output_line));
    }
    if (DERIVATE (m)) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " derivate") >= 0);
      WRITE (f, A68 (output_line));
    }
    if (SIZE (m) > 0) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " size %d", SIZE (m)) >= 0);
      WRITE (f, A68 (output_line));
    }
    if (HAS_ROWS (m)) {
      WRITE (f, " []");
    }
  }
}

//! @brief Xref_tags.

static void xref_tags (FILE_T f, TAG_T * s, int a)
{
  for (; s != NO_TAG; FORWARD (s)) {
    NODE_T *where_tag = NODE (s);
    if ((where_tag != NO_NODE) && ((STATUS_TEST (where_tag, CROSS_REFERENCE_MASK)) || TAG_TABLE (s) == A68_STANDENV)) {
      WRITE (f, "\n     ");
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "tag %d ", NUMBER (s)) >= 0);
      WRITE (f, A68 (output_line));
      switch (a) {
      case IDENTIFIER:
        {
          a68_print_mode (f, MOID (s));
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " %s", NSYMBOL (NODE (s))) >= 0);
          WRITE (f, A68 (output_line));
          break;
        }
      case INDICANT:
        {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "indicant %s ", NSYMBOL (NODE (s))) >= 0);
          WRITE (f, A68 (output_line));
          a68_print_mode (f, MOID (s));
          break;
        }
      case PRIO_SYMBOL:
        {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "priority %s %d", NSYMBOL (NODE (s)), PRIO (s)) >= 0);
          WRITE (f, A68 (output_line));
          break;
        }
      case OP_SYMBOL:
        {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "operator %s ", NSYMBOL (NODE (s))) >= 0);
          WRITE (f, A68 (output_line));
          a68_print_mode (f, MOID (s));
          break;
        }
      case LABEL:
        {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "label %s", NSYMBOL (NODE (s))) >= 0);
          WRITE (f, A68 (output_line));
          break;
        }
      case ANONYMOUS:
        {
          switch (PRIO (s)) {
          case ROUTINE_TEXT:
            {
              ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "routine text ") >= 0);
              break;
            }
          case FORMAT_TEXT:
            {
              ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "format text ") >= 0);
              break;
            }
          case FORMAT_IDENTIFIER:
            {
              ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "format item ") >= 0);
              break;
            }
          case COLLATERAL_CLAUSE:
            {
              ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "display ") >= 0);
              break;
            }
          case GENERATOR:
            {
              ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "generator ") >= 0);
              break;
            }
          }
          WRITE (f, A68 (output_line));
          a68_print_mode (f, MOID (s));
          break;
        }
      default:
        {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "internal %d ", a) >= 0);
          WRITE (f, A68 (output_line));
          a68_print_mode (f, MOID (s));
          break;
        }
      }
      if (NODE (s) != NO_NODE && NUMBER (NODE (s)) > 0) {
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", node %d", NUMBER (NODE (s))) >= 0);
        WRITE (f, A68 (output_line));
      }
      if (where_tag != NO_NODE && INFO (where_tag) != NO_NINFO && LINE (INFO (where_tag)) != NO_LINE) {
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", line %d", LINE_NUMBER (where_tag)) >= 0);
        WRITE (f, A68 (output_line));
      }
    }
  }
}

//! @brief Xref_decs.

static void xref_decs (FILE_T f, TABLE_T * t)
{
  if (INDICANTS (t) != NO_TAG) {
    xref_tags (f, INDICANTS (t), INDICANT);
  }
  if (OPERATORS (t) != NO_TAG) {
    xref_tags (f, OPERATORS (t), OP_SYMBOL);
  }
  if (PRIO (t) != NO_TAG) {
    xref_tags (f, PRIO (t), PRIO_SYMBOL);
  }
  if (IDENTIFIERS (t) != NO_TAG) {
    xref_tags (f, IDENTIFIERS (t), IDENTIFIER);
  }
  if (LABELS (t) != NO_TAG) {
    xref_tags (f, LABELS (t), LABEL);
  }
  if (ANONYMOUS (t) != NO_TAG) {
    xref_tags (f, ANONYMOUS (t), ANONYMOUS);
  }
}

//! @brief Xref1_moid.

static void xref1_moid (FILE_T f, MOID_T * p)
{
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n     #%d ", NUMBER (p)) >= 0);
  WRITE (f, A68 (output_line));
  print_mode_flat (f, p);
}

//! @brief Moid_listing.

void moid_listing (FILE_T f, MOID_T * m)
{
  for (; m != NO_MOID; FORWARD (m)) {
    xref1_moid (f, m);
  }
  WRITE (f, "\n");
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n     MODE STRING  #%d ", NUMBER (M_STRING)) >= 0);
  WRITE (f, A68 (output_line));
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n     MODE COMPLEX #%d ", NUMBER (M_COMPLEX)) >= 0);
  WRITE (f, A68 (output_line));
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n     MODE SEMA    #%d ", NUMBER (M_SEMA)) >= 0);
  WRITE (f, A68 (output_line));
}

//! @brief Cross_reference.

static void cross_reference (FILE_T f, NODE_T * p, LINE_T * l)
{
  if (p != NO_NODE && CROSS_REFERENCE_SAFE (&(A68 (job)))) {
    for (; p != NO_NODE; FORWARD (p)) {
      if (is_new_lexical_level (p) && l == LINE (INFO (p))) {
        TABLE_T *c = TABLE (SUB (p));
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n\n[level %d", LEVEL (c)) >= 0);
        WRITE (f, A68 (output_line));
        if (PREVIOUS (c) == A68_STANDENV) {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", in standard environ") >= 0);
        } else {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", in level %d", LEVEL (PREVIOUS (c))) >= 0);
        }
        WRITE (f, A68 (output_line));
#if (A68_LEVEL >= 3)
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", %llu increment]", AP_INCREMENT (c)) >= 0);
#else
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", %u increment]", AP_INCREMENT (c)) >= 0);
#endif
        WRITE (f, A68 (output_line));
        if (c != NO_TABLE) {
          xref_decs (f, c);
        }
        WRITE (f, "\n");
      }
      cross_reference (f, SUB (p), l);
    }
  }
}

//! @brief Tree listing for source line.

BOOL_T empty_leave (NODE_T * p)
{
#define TEST_LEAVE(n)\
  if (IS (p, (n)) && NEXT (p) == NO_NODE && PREVIOUS (p) == NO_NODE) {\
    return A68_TRUE;\
  }
  TEST_LEAVE (ENCLOSED_CLAUSE);
  TEST_LEAVE (UNIT);
  TEST_LEAVE (TERTIARY);
  TEST_LEAVE (SECONDARY);
  TEST_LEAVE (PRIMARY);
  TEST_LEAVE (DENOTATION);
  return A68_FALSE;
#undef TEST_LEAVE
}

//! @brief Tree listing for source line.

void tree_listing (FILE_T f, NODE_T * q, int x, LINE_T * l, int *ld, BOOL_T comment)
{
  for (; q != NO_NODE; FORWARD (q)) {
    NODE_T *p = q;
    int k, dist;
    if (((STATUS_TEST (p, TREE_MASK)) || comment) && l == LINE (INFO (p))) {
      if (*ld < 0) {
        *ld = x;
      }
// Indent.
      if (comment && empty_leave (p)) {
        ;
      } else {
        if (comment) {
          WRITE (f, "\n// ");
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%06d ", NUMBER (p)) >= 0);
          WRITE (f, A68 (output_line));
        } else {
          WRITE (f, "\n     ");
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%02d %06d p%02d ", x, NUMBER (p), PROCEDURE_LEVEL (INFO (p))) >= 0);
          WRITE (f, A68 (output_line));
          if (PREVIOUS (TABLE (p)) != NO_TABLE) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%02d-%02d-%02d ", (TABLE (p) != NO_TABLE ? LEX_LEVEL (p) : 0), (TABLE (p) != NO_TABLE ? LEVEL (PREVIOUS (TABLE (p))) : 0), (NON_LOCAL (p) != NO_TABLE ? LEVEL (NON_LOCAL (p)) : 0)
                    ) >= 0);
          } else {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%02d-  -%02d", (TABLE (p) != NO_TABLE ? LEX_LEVEL (p) : 0), (NON_LOCAL (p) != NO_TABLE ? LEVEL (NON_LOCAL (p)) : 0)
                    ) >= 0);
          }
          WRITE (f, A68 (output_line));
          if (MOID (q) != NO_MOID) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "#%04d ", NUMBER (MOID (p))) >= 0);
          } else {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "      ") >= 0);
          }
          WRITE (f, A68 (output_line));
        }
        for (k = 0; k < (x - *ld); k++) {
          WRITE (f, A68 (marker)[k]);
        }
        if (MOID (p) != NO_MOID) {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s ", moid_to_string (MOID (p), MOID_WIDTH, NO_NODE)) >= 0);
          WRITE (f, A68 (output_line));
        }
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s", non_terminal_string (A68 (edit_line), ATTRIBUTE (p))) >= 0);
        WRITE (f, A68 (output_line));
        if (SUB (p) == NO_NODE) {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
          WRITE (f, A68 (output_line));
        }
        if (!comment) {
          if (TAX (p) != NO_TAG) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", tag %06u", (unsigned) NUMBER (TAX (p))) >= 0);
            WRITE (f, A68 (output_line));
            if (MOID (TAX (p)) != NO_MOID) {
              ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", mode %06u", (unsigned) NUMBER (MOID (TAX (p)))) >= 0);
              WRITE (f, A68 (output_line));
            }
          }
          if (GINFO (p) != NO_GINFO && propagator_name (UNIT (&GPROP (p))) != NO_TEXT) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", %s", propagator_name (UNIT (&GPROP (p)))) >= 0);
            WRITE (f, A68 (output_line));
          }
          if (GINFO (p) != NO_GINFO && COMPILE_NAME (GINFO (p)) != NO_TEXT) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", %s", COMPILE_NAME (GINFO (p))) >= 0);
            WRITE (f, A68 (output_line));
          }
          if (GINFO (p) != NO_GINFO && COMPILE_NODE (GINFO (p)) > 0) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", %6d", COMPILE_NODE (GINFO (p))) >= 0);
            WRITE (f, A68 (output_line));
          }
        }
      }
      dist = x - (*ld);
      if (dist >= 0 && dist < BUFFER_SIZE) {
        A68 (marker)[dist] = (NEXT (p) != NO_NODE && l == LINE (INFO (NEXT (p))) ? "|" : " ");
      }
    }
    tree_listing (f, SUB (p), x + 1, l, ld, comment);
    dist = x - (*ld);
    if (dist >= 0 && dist < BUFFER_SIZE) {
      A68 (marker)[dist] = " ";
    }
  }
}

//! @brief Leaves_to_print.

static int leaves_to_print (NODE_T * p, LINE_T * l)
{
  int z = 0;
  for (; p != NO_NODE && z == 0; FORWARD (p)) {
    if (l == LINE (INFO (p)) && ((STATUS_TEST (p, TREE_MASK)))) {
      z++;
    } else {
      z += leaves_to_print (SUB (p), l);
    }
  }
  return z;
}

//! @brief List_source_line.

void list_source_line (FILE_T f, LINE_T * line, BOOL_T tree)
{
  int k = (int) strlen (STRING (line)) - 1;
  if (NUMBER (line) <= 0) {
// Mask the prelude and postlude.
    return;
  }
  if ((STRING (line))[k] == NEWLINE_CHAR) {
    (STRING (line))[k] = NULL_CHAR;
  }
// Print source line.
  write_source_line (f, line, NO_NODE, A68_ALL_DIAGNOSTICS);
// Cross reference for lexical levels starting at this line.
  if (OPTION_CROSS_REFERENCE (&(A68 (job)))) {
    cross_reference (f, TOP_NODE (&(A68 (job))), line);
  }
// Syntax tree listing connected with this line.
  if (tree && OPTION_TREE_LISTING (&(A68 (job)))) {
    if (TREE_LISTING_SAFE (&(A68 (job))) && leaves_to_print (TOP_NODE (&(A68 (job))), line)) {
      int ld = -1, k2;
      WRITE (f, "\n\nSyntax tree");
      for (k2 = 0; k2 < BUFFER_SIZE; k2++) {
        A68 (marker)[k2] = " ";
      }
      tree_listing (f, TOP_NODE (&(A68 (job))), 1, line, &ld, A68_FALSE);
      WRITE (f, "\n");
    }
  }
}

//! @brief Source_listing.

void write_source_listing (void)
{
  LINE_T *line = TOP_LINE (&(A68 (job)));
  FILE_T f = FILE_LISTING_FD (&(A68 (job)));
  int listed = 0;
  WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
  WRITE (FILE_LISTING_FD (&(A68 (job))), "\nSource listing");
  WRITE (FILE_LISTING_FD (&(A68 (job))), "\n------ -------");
  WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
  if (FILE_LISTING_OPENED (&(A68 (job))) == 0) {
    diagnostic (A68_ERROR, NO_NODE, ERROR_CANNOT_WRITE_LISTING);
    return;
  }
  for (; line != NO_LINE; FORWARD (line)) {
    if (NUMBER (line) > 0 && LIST (line)) {
      listed++;
    }
    list_source_line (f, line, A68_FALSE);
  }
// Warn if there was no source at all.
  if (listed == 0) {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n     No lines to list") >= 0);
    WRITE (f, A68 (output_line));
  }
}

//! @brief Write_source_listing.

void write_tree_listing (void)
{
  LINE_T *line = TOP_LINE (&(A68 (job)));
  FILE_T f = FILE_LISTING_FD (&(A68 (job)));
  int listed = 0;
  WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
  WRITE (FILE_LISTING_FD (&(A68 (job))), "\nSyntax tree listing");
  WRITE (FILE_LISTING_FD (&(A68 (job))), "\n------ ---- -------");
  WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
  if (FILE_LISTING_OPENED (&(A68 (job))) == 0) {
    diagnostic (A68_ERROR, NO_NODE, ERROR_CANNOT_WRITE_LISTING);
    return;
  }
  for (; line != NO_LINE; FORWARD (line)) {
    if (NUMBER (line) > 0 && LIST (line)) {
      listed++;
    }
    list_source_line (f, line, A68_TRUE);
  }
// Warn if there was no source at all.
  if (listed == 0) {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n     No lines to list") >= 0);
    WRITE (f, A68 (output_line));
  }
}

//! @brief Write_object_listing.

void write_object_listing (void)
{
  if (OPTION_OBJECT_LISTING (&(A68 (job)))) {
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\nObject listing");
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\n------ -------");
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    compiler (FILE_LISTING_FD (&(A68 (job))));
  }
}

//! @brief Write_listing.

void write_listing (void)
{
  FILE_T f = FILE_LISTING_FD (&(A68 (job)));
  if (OPTION_MOID_LISTING (&(A68 (job)))) {
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\nMode listing");
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\n---- -------");
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    moid_listing (f, TOP_MOID (&(A68 (job))));
  }
  if (OPTION_STANDARD_PRELUDE_LISTING (&(A68 (job))) && A68_STANDENV != NO_TABLE) {
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\nStandard prelude listing");
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\n-------- ------- -------");
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    xref_decs (f, A68_STANDENV);
  }
  if (TOP_REFINEMENT (&(A68 (job))) != NO_REFINEMENT) {
    REFINEMENT_T *x = TOP_REFINEMENT (&(A68 (job)));
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\nRefinement listing");
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\n---------- -------");
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    while (x != NO_REFINEMENT) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n  \"%s\"", NAME (x)) >= 0);
      WRITE (f, A68 (output_line));
      if (LINE_DEFINED (x) != NO_LINE) {
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", defined in line %d", NUMBER (LINE_DEFINED (x))) >= 0);
        WRITE (f, A68 (output_line));
      }
      if (LINE_APPLIED (x) != NO_LINE) {
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", applied in line %d", NUMBER (LINE_APPLIED (x))) >= 0);
        WRITE (f, A68 (output_line));
      }
      switch (APPLICATIONS (x)) {
      case 0:
        {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", not applied") >= 0);
          WRITE (f, A68 (output_line));
          break;
        }
      case 1:
        {
          break;
        }
      default:
        {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", applied more than once") >= 0);
          WRITE (f, A68 (output_line));
          break;
        }
      }
      FORWARD (x);
    }
  }
  if (OPTION_LIST (&(A68 (job))) != NO_OPTION_LIST) {
    OPTION_LIST_T *i;
    int k = 1;
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\nPragmat listing");
    WRITE (FILE_LISTING_FD (&(A68 (job))), "\n------- -------");
    WRITE (FILE_LISTING_FD (&(A68 (job))), NEWLINE_STRING);
    for (i = OPTION_LIST (&(A68 (job))); i != NO_OPTION_LIST; FORWARD (i)) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\n%d: %s", k++, STR (i)) >= 0);
      WRITE (f, A68 (output_line));
    }
  }
}

//! @brief Write_listing_header.

void write_listing_header (void)
{
  FILE_T f = FILE_LISTING_FD (&(A68 (job)));
  LINE_T *z;
  state_version (FILE_LISTING_FD (&(A68 (job))));
  WRITE (FILE_LISTING_FD (&(A68 (job))), "\nFile \"");
  WRITE (FILE_LISTING_FD (&(A68 (job))), FILE_SOURCE_NAME (&(A68 (job))));
  WRITE (FILE_LISTING_FD (&(A68 (job))), "\"");
  if (OPTION_STATISTICS_LISTING (&(A68 (job)))) {
    if (ERROR_COUNT (&(A68 (job))) + WARNING_COUNT (&(A68 (job))) > 0) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "\nDiagnostics: %d error(s), %d warning(s)", ERROR_COUNT (&(A68 (job))), WARNING_COUNT (&(A68 (job)))) >= 0);
      WRITE (f, A68 (output_line));
      for (z = TOP_LINE (&(A68 (job))); z != NO_LINE; FORWARD (z)) {
        if (DIAGNOSTICS (z) != NO_DIAGNOSTIC) {
          write_source_line (f, z, NO_NODE, A68_TRUE);
        }
      }
    }
  }
}
