/*!
\file listing.c
\brief routines for making a listing file
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

#include "config.h"
#include "diagnostics.h"
#include "algol68g.h"
#include "genie.h"

#define SHOW_EQ A68_FALSE

char *bar[BUFFER_SIZE];

/*!
\brief brief_mode_string
\param p moid to print
\return pointer to string
**/

static char *brief_mode_string (MOID_T * p)
{
  static char q[BUFFER_SIZE];
  ASSERT (snprintf (q, BUFFER_SIZE, "mode %d", NUMBER (p)) >= 0);
  return (new_string (q));
}

/*!
\brief brief_mode_flat
\param f file number
\param z moid to print
**/

void brief_mode_flat (FILE_T f, MOID_T * z)
{
  if (WHETHER (z, STANDARD) || WHETHER (z, INDICANT)) {
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
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", SYMBOL (NODE (z))) >= 0);
    WRITE (f, output_line);
  } else {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", brief_mode_string (z)) >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief brief_fields_flat
\param f file number
\param pack pack to print
**/

static void brief_fields_flat (FILE_T f, PACK_T * pack)
{
  if (pack != NULL) {
    brief_mode_flat (f, MOID (pack));
    if (NEXT (pack) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", ") >= 0);
      WRITE (f, output_line);
      brief_fields_flat (f, NEXT (pack));
    }
  }
}

/*!
\brief brief_moid_flat
\param f file number
\param z moid to print
**/

void brief_moid_flat (FILE_T f, MOID_T * z)
{
  if (z != NULL) {
    if (WHETHER (z, STANDARD) || WHETHER (z, INDICANT)) {
      brief_mode_flat (f, z);
    } else if (z == MODE (COLLITEM)) {
      WRITE (f, "\"COLLITEM\"");
    } else if (WHETHER (z, REF_SYMBOL)) {
      WRITE (f, "REF ");
      brief_mode_flat (f, SUB (z));
    } else if (WHETHER (z, FLEX_SYMBOL)) {
      WRITE (f, "FLEX ");
      brief_mode_flat (f, SUB (z));
    } else if (WHETHER (z, ROW_SYMBOL)) {
      int i = DIM (z);
      WRITE (f, "[");
      while (--i) {
        WRITE (f, ", ");
      }
      WRITE (f, "] ");
      brief_mode_flat (f, SUB (z));
    } else if (WHETHER (z, STRUCT_SYMBOL)) {
      WRITE (f, "STRUCT (");
      brief_fields_flat (f, PACK (z));
      WRITE (f, ")");
    } else if (WHETHER (z, UNION_SYMBOL)) {
      WRITE (f, "UNION (");
      brief_fields_flat (f, PACK (z));
      WRITE (f, ")");
    } else if (WHETHER (z, PROC_SYMBOL)) {
      WRITE (f, "PROC ");
      if (PACK (z) != NULL) {
        WRITE (f, "(");
        brief_fields_flat (f, PACK (z));
        WRITE (f, ") ");
      }
      brief_mode_flat (f, SUB (z));
    } else if (WHETHER (z, IN_TYPE_MODE)) {
      WRITE (f, "\"SIMPLIN\"");
    } else if (WHETHER (z, OUT_TYPE_MODE)) {
      WRITE (f, "\"SIMPLOUT\"");
    } else if (WHETHER (z, ROWS_SYMBOL)) {
      WRITE (f, "\"ROWS\"");
    } else if (WHETHER (z, SERIES_MODE)) {
      WRITE (f, "\"SERIES\" (");
      brief_fields_flat (f, PACK (z));
      WRITE (f, ")");
    } else if (WHETHER (z, STOWED_MODE)) {
      WRITE (f, "\"STOWED\" (");
      brief_fields_flat (f, PACK (z));
      WRITE (f, ")");
    }
  }
}

/*!
\brief print_mode_flat
\param f file number
\param m moid to print
**/

void print_mode_flat (FILE_T f, MOID_T * m)
{
  if (m != NULL) {
    brief_moid_flat (f, m);
    if (m->equivalent_mode != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", equi: %s", brief_mode_string (EQUIVALENT (m))) >= 0);
      WRITE (f, output_line);
    }
    if (SLICE (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", slice: %s", brief_mode_string (SLICE (m))) >= 0);
      WRITE (f, output_line);
    }
    if (ROWED (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", rowed: %s", brief_mode_string (ROWED (m))) >= 0);
      WRITE (f, output_line);
    }
    if (DEFLEXED (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", deflex: %s", brief_mode_string (DEFLEXED (m))) >= 0);
      WRITE (f, output_line);
    }
    if (MULTIPLE (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", multiple: %s", brief_mode_string (MULTIPLE (m))) >= 0);
      WRITE (f, output_line);
    }
    if (NAME (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", name: %s", brief_mode_string (NAME (m))) >= 0);
      WRITE (f, output_line);
    }
    if (TRIM (m) != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", trim: %s", brief_mode_string (TRIM (m))) >= 0);
      WRITE (f, output_line);
    }
    if (m->use == A68_FALSE) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", unused") >= 0);
      WRITE (f, output_line);
    }
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", size: %d", MOID_SIZE (m)) >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief xref_tags
\param f file number
\param s tag to print
\param a attribute
**/

static void xref_tags (FILE_T f, TAG_T * s, int a)
{
  for (; s != NULL; FORWARD (s)) {
    NODE_T *where_tag = NODE (s);
    if ((where_tag != NULL) && ((STATUS_TEST (where_tag, CROSS_REFERENCE_MASK)) || TAG_TABLE (s) == stand_env)) {
      WRITE (f, "\n     ");
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "tag %d ", NUMBER (s)) >= 0);
      WRITE (f, output_line);
      switch (a) {
      case IDENTIFIER:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Identifier %s ", SYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      case INDICANT:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Indicant %s ", SYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      case PRIO_SYMBOL:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Priority %s %d", SYMBOL (NODE (s)), PRIO (s)) >= 0);
          WRITE (f, output_line);
          break;
        }
      case OP_SYMBOL:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Operator %s ", SYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      case LABEL:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Label %s", SYMBOL (NODE (s))) >= 0);
          WRITE (f, output_line);
          break;
        }
      case ANONYMOUS:
        {
          switch (PRIO (s)) {
          case ROUTINE_TEXT:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Routine text ") >= 0);
              break;
            }
          case FORMAT_TEXT:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Format text ") >= 0);
              break;
            }
          case FORMAT_IDENTIFIER:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Format item ") >= 0);
              break;
            }
          case COLLATERAL_CLAUSE:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Display ") >= 0);
              break;
            }
          case GENERATOR:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Generator ") >= 0);
              break;
            }
          case PROTECT_FROM_SWEEP:
            {
              ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Sweep protect ") >= 0);
              break;
            }
          }
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      default:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "Internal %d ", a) >= 0);
          WRITE (f, output_line);
          brief_moid_flat (f, MOID (s));
          break;
        }
      }
      if (NODE (s) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", node %d", NUMBER (NODE (s))) >= 0);
        WRITE (f, output_line);
      }
      if (where_tag != NULL && where_tag->info != NULL && where_tag->info->line != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", line %d", LINE_NUMBER (where_tag)) >= 0);
        WRITE (f, output_line);
      }
    }
  }
}

/*!
\brief xref_decs
\param f file number
\param t symbol table
**/

static void xref_decs (FILE_T f, SYMBOL_TABLE_T * t)
{
  if (t->indicants != NULL) {
    xref_tags (f, t->indicants, INDICANT);
  }
  if (t->operators != NULL) {
    xref_tags (f, t->operators, OP_SYMBOL);
  }
  if (PRIO (t) != NULL) {
    xref_tags (f, PRIO (t), PRIO_SYMBOL);
  }
  if (t->identifiers != NULL) {
    xref_tags (f, t->identifiers, IDENTIFIER);
  }
  if (t->labels != NULL) {
    xref_tags (f, t->labels, LABEL);
  }
  if (t->anonymous != NULL) {
    xref_tags (f, t->anonymous, ANONYMOUS);
  }
}

/*!
\brief xref1_moid
\param f file number
\param p moid to xref
**/

static void xref1_moid (FILE_T f, MOID_T * p)
{
  if (USE (p) && (EQUIVALENT (p) == NULL || SHOW_EQ)) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n     %s %s ", brief_mode_string (p), moid_to_string (p, 132, NULL)) >= 0);
    WRITE (f, output_line);
    print_mode_flat (f, p);
    WRITE (f, NEWLINE_STRING);
  }
}

/*!
\brief xref_moids
\param f file number
\param p moid chain to xref
**/

static void xref_moids (FILE_T f, MOID_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    xref1_moid (f, p);
  }
}

/*!
\brief moid_listing
\param f file number
\param m moid list to xref
**/

static void moid_listing (FILE_T f, MOID_LIST_T * m)
{
  for (; m != NULL; FORWARD (m)) {
    xref1_moid (f, MOID (m));
  }
}

/*!
\brief cross_reference
\param file number
\param p top node
\param l source line
**/

static void cross_reference (FILE_T f, NODE_T * p, SOURCE_LINE_T * l)
{
  if (p != NULL && program.cross_reference_safe) {
    for (; p != NULL; FORWARD (p)) {
      if (whether_new_lexical_level (p) && l == LINE (p)) {
        SYMBOL_TABLE_T *c = SYMBOL_TABLE (SUB (p));
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n[level %d", c->level) >= 0);
        WRITE (f, output_line);
        if (PREVIOUS (c) == stand_env) {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", in standard environ]") >= 0);
        } else {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", in level %d]", PREVIOUS (c)->level) >= 0);
        }
        WRITE (f, output_line);
        if (c->moids != NULL) {
          xref_moids (f, c->moids);
        }
        if (c != NULL) {
          xref_decs (f, c);
        }
      }
      cross_reference (f, SUB (p), l);
    }
  }
}

/*!
\brief write_symbols
\param file number
\param p top node
\param count symbols written

static void write_symbols (FILE_T f, NODE_T * p, int *count)
{
  for (; p != NULL && (*count) < 5; FORWARD (p)) {
    if (SUB (p) != NULL) {
      write_symbols (f, SUB (p), count);
    } else {
      if (*count > 0) {
        WRITE (f, " ");
      }
      (*count)++;
      if (*count == 5) {
        WRITE (f, "...");
      } else {
        ASSERT (snprintf(output_line, (size_t) BUFFER_SIZE, "%s", SYMBOL (p)) >= 0);
        WRITE (f, output_line);
      }
    }
  }
}
**/

/*!
\brief tree_listing
\param f file number
\param q top node
\param x current level
\param l source line
\param ld index for indenting and drawing bars connecting nodes
**/

void tree_listing (FILE_T f, NODE_T * q, int x, SOURCE_LINE_T * l, int *ld)
{
  for (; q != NULL; FORWARD (q)) {
    NODE_T *p = q;
    int k, dist;
    if (((STATUS_TEST (p, TREE_MASK))) && l == LINE (p)) {
      if (*ld < 0) {
        *ld = x;
      }
/* Indent. */
      WRITE (f, "\n     ");
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%02u %06u p%02u ", (unsigned) x, (unsigned) NUMBER (p), (unsigned) INFO (p)->PROCEDURE_LEVEL) >= 0);
      WRITE (f, output_line);
      if (PREVIOUS (SYMBOL_TABLE (p)) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "l%02u(%02u) ", (unsigned) (SYMBOL_TABLE (p) != NULL ? LEX_LEVEL (p) : -1), (unsigned) LEVEL (PREVIOUS (SYMBOL_TABLE (p)))) >= 0);
      } else {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "l%02u    ", (unsigned) (SYMBOL_TABLE (p) != NULL ? LEX_LEVEL (p) : -1)) >= 0);
      }
      WRITE (f, output_line);
      for (k = 0; k < (x - *ld); k++) {
        WRITE (f, bar[k]);
      }
      if (MOID (p) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s ", moid_to_string (MOID (p), MOID_WIDTH, NULL)) >= 0);
        WRITE (f, output_line);
      }
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "%s", non_terminal_string (edit_line, ATTRIBUTE (p))) >= 0);
      WRITE (f, output_line);
      if (SUB (p) == NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, " \"%s\"", SYMBOL (p)) >= 0);
        WRITE (f, output_line);
      }
      if (TAX (p) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", tag %06u", (unsigned) NUMBER (TAX (p))) >= 0);
        WRITE (f, output_line);
        if (MOID (TAX (p)) != NULL) {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", mode %06u", (unsigned) NUMBER (MOID (TAX (p)))) >= 0);
          WRITE (f, output_line);
        }
      }
      if (GENIE (p) != NULL && propagator_name (PROPAGATOR (p).unit) != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", %s", propagator_name (PROPAGATOR (p).unit)) >= 0);
        WRITE (f, output_line);
      }
    }
    dist = x - (*ld);
    if (dist >= 0 && dist < BUFFER_SIZE) {
      bar[dist] = (NEXT (p) != NULL && l == LINE (NEXT (p)) ? "|" : " ");
    }
    tree_listing (f, SUB (p), x + 1, l, ld);
    dist = x - (*ld);
    if (dist >= 0 && dist < BUFFER_SIZE) {
      bar[dist] = " ";
    }
  }
}

/*!
\brief leaves_to_print
\param p top node
\param l source line
\return number of nodes to be printed in tree listing
**/

static int leaves_to_print (NODE_T * p, SOURCE_LINE_T * l)
{
  int z = 0;
  for (; p != NULL && z == 0; FORWARD (p)) {
    if (l == LINE (p) && ((STATUS_TEST (p, TREE_MASK)))) {
      z++;
    } else {
      z += leaves_to_print (SUB (p), l);
    }
  }
  return (z);
}

/*!
\brief list_source_line
\param f file number
\param module current module
\param line source line
**/

void list_source_line (FILE_T f, SOURCE_LINE_T * line, BOOL_T tree)
{
  int k = (int) strlen (line->string) - 1;
  if (NUMBER (line) <= 0) {
/* Mask the prelude and postlude. */
    return;
  }
  if ((line->string)[k] == NEWLINE_CHAR) {
    (line->string)[k] = NULL_CHAR;
  }
/* Print source line. */
  write_source_line (f, line, NULL, A68_ALL_DIAGNOSTICS);
/* Cross reference for lexical levels starting at this line. */
  if (program.options.cross_reference) {
    cross_reference (f, program.top_node, line);
  }
/* Syntax tree listing connected with this line. */
  if (tree && program.options.tree_listing) {
    if (program.tree_listing_safe && leaves_to_print (program.top_node, line)) {
      int ld = -1, k2;
      WRITE (f, "\nSyntax tree");
      for (k2 = 0; k2 < BUFFER_SIZE; k2++) {
        bar[k2] = " ";
      }
      tree_listing (f, program.top_node, 1, line, &ld);
    }
  }
}

/*!
\brief source_listing
\param module current module
**/

void write_source_listing (void)
{
  SOURCE_LINE_T *line = program.top_line;
  FILE_T f = program.files.listing.fd;
  int listed = 0;
  WRITE (program.files.listing.fd, "\n");
  WRITE (program.files.listing.fd, "\nSource listing");
  WRITE (program.files.listing.fd, "\n------ -------");
  WRITE (program.files.listing.fd, "\n");
  if (program.files.listing.opened == 0) {
    diagnostic_node (A68_ERROR, NULL, ERROR_CANNOT_WRITE_LISTING, NULL);
    return;
  }
  for (; line != NULL; FORWARD (line)) {
    if (NUMBER (line) > 0 && line->list) {
      listed++;
    }
    list_source_line (f, line, A68_FALSE);
  }
/* Warn if there was no source at all. */
  if (listed == 0) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n     No lines to list") >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief write_source_listing
\param module current module
**/

void write_tree_listing (void)
{
  SOURCE_LINE_T *line = program.top_line;
  FILE_T f = program.files.listing.fd;
  int listed = 0;
  WRITE (program.files.listing.fd, "\n");
  WRITE (program.files.listing.fd, "\nSyntax tree listing");
  WRITE (program.files.listing.fd, "\n------ ---- -------");
  WRITE (program.files.listing.fd, "\n");
  if (program.files.listing.opened == 0) {
    diagnostic_node (A68_ERROR, NULL, ERROR_CANNOT_WRITE_LISTING, NULL);
    return;
  }
  for (; line != NULL; FORWARD (line)) {
    if (NUMBER (line) > 0 && line->list) {
      listed++;
    }
    list_source_line (f, line, A68_TRUE);
  }
/* Warn if there was no source at all. */
  if (listed == 0) {
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n     No lines to list") >= 0);
    WRITE (f, output_line);
  }
}

/*!
\brief write_object_listing
\param module current module
**/

void write_object_listing (void)
{
  if (program.options.object_listing) {
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nObject listing");
    WRITE (program.files.listing.fd, "\n------ -------");
    WRITE (program.files.listing.fd, "\n");
    compiler (program.files.listing.fd);
  }
}

/*!
\brief write_listing
\param module current module
**/

void write_listing (void)
{
  FILE_T f = program.files.listing.fd;
  if (program.options.moid_listing && top_moid_list != NULL) {
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nMode listing");
    WRITE (program.files.listing.fd, "\n---- -------");
    WRITE (program.files.listing.fd, "\n");
    moid_listing (f, top_moid_list);
  }
  if (program.options.standard_prelude_listing && stand_env != NULL) {
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nStandard prelude listing");
    WRITE (program.files.listing.fd, "\n-------- ------- -------");
    WRITE (program.files.listing.fd, "\n");
    xref_decs (f, stand_env);
  }
  if (program.top_refinement != NULL) {
    REFINEMENT_T *x = program.top_refinement;
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nRefinement listing");
    WRITE (program.files.listing.fd, "\n---------- -------");
    WRITE (program.files.listing.fd, "\n");
    while (x != NULL) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n  \"%s\"", x->name) >= 0);
      WRITE (f, output_line);
      if (x->line_defined != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", defined in line %d", NUMBER (x->line_defined)) >= 0);
        WRITE (f, output_line);
      }
      if (x->line_applied != NULL) {
        ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", applied in line %d", NUMBER (x->line_applied)) >= 0);
        WRITE (f, output_line);
      }
      switch (x->applications) {
      case 0:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", not applied") >= 0);
          WRITE (f, output_line);
          break;
        }
      case 1:
        {
          break;
        }
      default:
        {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, ", applied more than once") >= 0);
          WRITE (f, output_line);
          break;
        }
      }
      FORWARD (x);
    }
  }
  if (program.options.list != NULL) {
    OPTION_LIST_T *i;
    int k = 1;
    WRITE (program.files.listing.fd, "\n");
    WRITE (program.files.listing.fd, "\nPragmat listing");
    WRITE (program.files.listing.fd, "\n------- -------");
    WRITE (program.files.listing.fd, "\n");
    for (i = program.options.list; i != NULL; FORWARD (i)) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\n%d: %s", k++, i->str) >= 0);
      WRITE (f, output_line);
    }
  }
  WRITE (f, NEWLINE_STRING);
}

/*!
\brief write_listing_header
\param module current module
**/

void write_listing_header (void)
{
  FILE_T f = program.files.listing.fd;
  SOURCE_LINE_T * z;
  state_version (program.files.listing.fd);
  WRITE (program.files.listing.fd, "\nFile \"");
  WRITE (program.files.listing.fd, program.files.source.name);
  if (program.options.statistics_listing) {
    if (program.error_count + program.warning_count > 0) {
      ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\nDiagnostics: %d error(s), %d warning(s)", program.error_count, program.warning_count) >= 0);
      WRITE (f, output_line);
      for (z = program.top_line; z != NULL; FORWARD (z)) {
        if (z->diagnostics != NULL) {
          write_source_line (f, z, NULL, A68_TRUE);
        }
      }
    }
  }
}
