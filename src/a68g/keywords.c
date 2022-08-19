//! @file keywords.c
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

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-genie.h"
#include "a68g-postulates.h"
#include "a68g-parser.h"
#include "a68g-options.h"
#include "a68g-optimiser.h"
#include "a68g-listing.h"

//! @brief Add token to the token tree.

TOKEN_T *add_token (TOKEN_T ** p, char *t)
{
  char *z = new_fixed_string (t);
  while (*p != NO_TOKEN) {
    int k = strcmp (z, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      return *p;
    }
  }
  *p = (TOKEN_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (TOKEN_T));
  TEXT (*p) = z;
  LESS (*p) = MORE (*p) = NO_TOKEN;
  return *p;
}

//! @brief Find keyword, from token name.

KEYWORD_T *find_keyword (KEYWORD_T * p, char *t)
{
  while (p != NO_KEYWORD) {
    int k = strcmp (t, TEXT (p));
    if (k < 0) {
      p = LESS (p);
    } else if (k > 0) {
      p = MORE (p);
    } else {
      return p;
    }
  }
  return NO_KEYWORD;
}

//! @brief Find keyword, from attribute.

KEYWORD_T *find_keyword_from_attribute (KEYWORD_T * p, int a)
{
  if (p == NO_KEYWORD) {
    return NO_KEYWORD;
  } else if (a == ATTRIBUTE (p)) {
    return p;
  } else {
    KEYWORD_T *z;
    if ((z = find_keyword_from_attribute (LESS (p), a)) != NO_KEYWORD) {
      return z;
    } else if ((z = find_keyword_from_attribute (MORE (p), a)) != NO_KEYWORD) {
      return z;
    } else {
      return NO_KEYWORD;
    }
  }
}

//! @brief Add keyword to the tree.

void add_keyword (KEYWORD_T ** p, int a, char *t)
{
  while (*p != NO_KEYWORD) {
    int k = strcmp (t, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else {
      p = &MORE (*p);
    }
  }
  *p = (KEYWORD_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (KEYWORD_T));
  ATTRIBUTE (*p) = a;
  TEXT (*p) = t;
  LESS (*p) = MORE (*p) = NO_KEYWORD;
}

//! @brief Make tables of keywords and non-terminals.

void set_up_tables (void)
{
// Entries are randomised to balance the tree.
  if (OPTION_STRICT (&A68_JOB) == A68_FALSE) {
    add_keyword (&A68 (top_keyword), ENVIRON_SYMBOL, "ENVIRON");
    add_keyword (&A68 (top_keyword), DOWNTO_SYMBOL, "DOWNTO");
    add_keyword (&A68 (top_keyword), UNTIL_SYMBOL, "UNTIL");
    add_keyword (&A68 (top_keyword), CLASS_SYMBOL, "CLASS");
    add_keyword (&A68 (top_keyword), NEW_SYMBOL, "NEW");
    add_keyword (&A68 (top_keyword), DIAGONAL_SYMBOL, "DIAG");
    add_keyword (&A68 (top_keyword), TRANSPOSE_SYMBOL, "TRNSP");
    add_keyword (&A68 (top_keyword), ROW_SYMBOL, "ROW");
    add_keyword (&A68 (top_keyword), COLUMN_SYMBOL, "COL");
    add_keyword (&A68 (top_keyword), CODE_SYMBOL, "CODE");
    add_keyword (&A68 (top_keyword), EDOC_SYMBOL, "EDOC");
    add_keyword (&A68 (top_keyword), ANDF_SYMBOL, "THEF");
    add_keyword (&A68 (top_keyword), ORF_SYMBOL, "ELSF");
    add_keyword (&A68 (top_keyword), ANDF_SYMBOL, "ANDTH");
    add_keyword (&A68 (top_keyword), ORF_SYMBOL, "OREL");
    add_keyword (&A68 (top_keyword), ANDF_SYMBOL, "ANDF");
    add_keyword (&A68 (top_keyword), ORF_SYMBOL, "ORF");
    add_keyword (&A68 (top_keyword), ALIF_SYMBOL, "ALIF");
  }
  add_keyword (&A68 (top_keyword), POINT_SYMBOL, ".");
  add_keyword (&A68 (top_keyword), COMPLEX_SYMBOL, "COMPLEX");
  add_keyword (&A68 (top_keyword), ACCO_SYMBOL, "{");
  add_keyword (&A68 (top_keyword), OCCA_SYMBOL, "}");
  add_keyword (&A68 (top_keyword), SOUND_SYMBOL, "SOUND");
  add_keyword (&A68 (top_keyword), COLON_SYMBOL, ":");
  add_keyword (&A68 (top_keyword), THEN_BAR_SYMBOL, "|");
  add_keyword (&A68 (top_keyword), SUB_SYMBOL, "[");
  add_keyword (&A68 (top_keyword), BY_SYMBOL, "BY");
  add_keyword (&A68 (top_keyword), OP_SYMBOL, "OP");
  add_keyword (&A68 (top_keyword), COMMA_SYMBOL, ",");
  add_keyword (&A68 (top_keyword), AT_SYMBOL, "AT");
  add_keyword (&A68 (top_keyword), PRIO_SYMBOL, "PRIO");
  add_keyword (&A68 (top_keyword), STYLE_I_COMMENT_SYMBOL, "CO");
  add_keyword (&A68 (top_keyword), END_SYMBOL, "END");
  add_keyword (&A68 (top_keyword), GO_SYMBOL, "GO");
  add_keyword (&A68 (top_keyword), TO_SYMBOL, "TO");
  add_keyword (&A68 (top_keyword), ELSE_BAR_SYMBOL, "|:");
  add_keyword (&A68 (top_keyword), THEN_SYMBOL, "THEN");
  add_keyword (&A68 (top_keyword), TRUE_SYMBOL, "TRUE");
  add_keyword (&A68 (top_keyword), PROC_SYMBOL, "PROC");
  add_keyword (&A68 (top_keyword), FOR_SYMBOL, "FOR");
  add_keyword (&A68 (top_keyword), GOTO_SYMBOL, "GOTO");
  add_keyword (&A68 (top_keyword), WHILE_SYMBOL, "WHILE");
  add_keyword (&A68 (top_keyword), IS_SYMBOL, ":=:");
  add_keyword (&A68 (top_keyword), ASSIGN_TO_SYMBOL, "=:");
  add_keyword (&A68 (top_keyword), COMPL_SYMBOL, "COMPL");
  add_keyword (&A68 (top_keyword), FROM_SYMBOL, "FROM");
  add_keyword (&A68 (top_keyword), BOLD_PRAGMAT_SYMBOL, "PRAGMAT");
  add_keyword (&A68 (top_keyword), BOLD_COMMENT_SYMBOL, "COMMENT");
  add_keyword (&A68 (top_keyword), DO_SYMBOL, "DO");
  add_keyword (&A68 (top_keyword), STYLE_II_COMMENT_SYMBOL, "#");
  add_keyword (&A68 (top_keyword), CASE_SYMBOL, "CASE");
  add_keyword (&A68 (top_keyword), LOC_SYMBOL, "LOC");
  add_keyword (&A68 (top_keyword), CHAR_SYMBOL, "CHAR");
  add_keyword (&A68 (top_keyword), ISNT_SYMBOL, ":/=:");
  add_keyword (&A68 (top_keyword), REF_SYMBOL, "REF");
  add_keyword (&A68 (top_keyword), NIL_SYMBOL, "NIL");
  add_keyword (&A68 (top_keyword), ASSIGN_SYMBOL, ":=");
  add_keyword (&A68 (top_keyword), FI_SYMBOL, "FI");
  add_keyword (&A68 (top_keyword), FILE_SYMBOL, "FILE");
  add_keyword (&A68 (top_keyword), PAR_SYMBOL, "PAR");
  add_keyword (&A68 (top_keyword), ASSERT_SYMBOL, "ASSERT");
  add_keyword (&A68 (top_keyword), OUSE_SYMBOL, "OUSE");
  add_keyword (&A68 (top_keyword), IN_SYMBOL, "IN");
  add_keyword (&A68 (top_keyword), LONG_SYMBOL, "LONG");
  add_keyword (&A68 (top_keyword), SEMI_SYMBOL, ";");
  add_keyword (&A68 (top_keyword), EMPTY_SYMBOL, "EMPTY");
  add_keyword (&A68 (top_keyword), MODE_SYMBOL, "MODE");
  add_keyword (&A68 (top_keyword), IF_SYMBOL, "IF");
  add_keyword (&A68 (top_keyword), OD_SYMBOL, "OD");
  add_keyword (&A68 (top_keyword), OF_SYMBOL, "OF");
  add_keyword (&A68 (top_keyword), STRUCT_SYMBOL, "STRUCT");
  add_keyword (&A68 (top_keyword), STYLE_I_PRAGMAT_SYMBOL, "PR");
  add_keyword (&A68 (top_keyword), BUS_SYMBOL, "]");
  add_keyword (&A68 (top_keyword), SKIP_SYMBOL, "SKIP");
  add_keyword (&A68 (top_keyword), SHORT_SYMBOL, "SHORT");
  add_keyword (&A68 (top_keyword), IS_SYMBOL, "IS");
  add_keyword (&A68 (top_keyword), ESAC_SYMBOL, "ESAC");
  add_keyword (&A68 (top_keyword), CHANNEL_SYMBOL, "CHANNEL");
  add_keyword (&A68 (top_keyword), REAL_SYMBOL, "REAL");
  add_keyword (&A68 (top_keyword), STRING_SYMBOL, "STRING");
  add_keyword (&A68 (top_keyword), BOOL_SYMBOL, "BOOL");
  add_keyword (&A68 (top_keyword), ISNT_SYMBOL, "ISNT");
  add_keyword (&A68 (top_keyword), FALSE_SYMBOL, "FALSE");
  add_keyword (&A68 (top_keyword), UNION_SYMBOL, "UNION");
  add_keyword (&A68 (top_keyword), OUT_SYMBOL, "OUT");
  add_keyword (&A68 (top_keyword), OPEN_SYMBOL, "(");
  add_keyword (&A68 (top_keyword), BEGIN_SYMBOL, "BEGIN");
  add_keyword (&A68 (top_keyword), FLEX_SYMBOL, "FLEX");
  add_keyword (&A68 (top_keyword), VOID_SYMBOL, "VOID");
  add_keyword (&A68 (top_keyword), BITS_SYMBOL, "BITS");
  add_keyword (&A68 (top_keyword), ELSE_SYMBOL, "ELSE");
  add_keyword (&A68 (top_keyword), EXIT_SYMBOL, "EXIT");
  add_keyword (&A68 (top_keyword), HEAP_SYMBOL, "HEAP");
  add_keyword (&A68 (top_keyword), INT_SYMBOL, "INT");
  add_keyword (&A68 (top_keyword), BYTES_SYMBOL, "BYTES");
  add_keyword (&A68 (top_keyword), PIPE_SYMBOL, "PIPE");
  add_keyword (&A68 (top_keyword), FORMAT_SYMBOL, "FORMAT");
  add_keyword (&A68 (top_keyword), SEMA_SYMBOL, "SEMA");
  add_keyword (&A68 (top_keyword), CLOSE_SYMBOL, ")");
  add_keyword (&A68 (top_keyword), AT_SYMBOL, "@");
  add_keyword (&A68 (top_keyword), ELIF_SYMBOL, "ELIF");
  add_keyword (&A68 (top_keyword), FORMAT_DELIMITER_SYMBOL, "$");
}
