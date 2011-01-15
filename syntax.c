/*!
\file syntax.c
\brief hand-coded Algol 68 scanner and parser 
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2011 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

/*

AN EFFECTIVE ALGOL 68 PARSER

Algol 68 grammar is defined as a two level (Van Wijngaarden) grammar that
incorporates, as syntactical rules, a lot of the "semantical" rules in 
other languages. Examples are correct use of symbols, modes and scope.
That is why so much functionality is in the "syntax.c" file. In fact, all
this material constitutes an effective "Algol 68 parser". This pragmatic
approach was chosen since in the early days of Algol 68, many implementations
"ab initio" failed. If, from now on, I mention "parser", I mean a common
recursive-descent or bottom-up parser.

First part in this file is the scanner. The source file is read,
is tokenised, and if needed a refinement preprocessor elaborates a stepwise
refined program. The result is a linear list of tokens that is input for the
parser, that will transform the linear list into a syntax tree.

Algol68G tokenises all symbols before the bottom-up parser is invoked. 
This means that scanning does not use information from the parser.
The scanner does of course do some rudimentary parsing. Format texts can have
enclosed clauses in them, so we record information in a stack as to know
what is being scanned. Also, the refinement preprocessor implements a
(trivial) grammar.

The scanner supports two stropping regimes: bold and quote. Examples of both:

       bold stropping: BEGIN INT i = 1, j = 1; print (i + j) END

       quote stropping: 'BEGIN' 'INT' I = 1, J = 1; PRINT (I + J) 'END'

Quote stropping was used frequently in the (excusez-le-mot) punch-card age.
Hence, bold stropping is the default. There also existed point stropping, but
that has not been implemented here.

Next, this file contains a hand-coded parser for Algol 68.

First part of the parser is a recursive-descent type to check parenthesis.
Also a first set-up is made of symbol tables, needed by the bottom-up parser.
Next part is the bottom-up parser, that parses without knowing about modes while
parsing and reducing. It can therefore not exchange "[]" with "()" as was
blessed by the Revised Report. This is solved by treating CALL and SLICE as
equivalent for the moment and letting the mode checker sort it out later.

This is a Mailloux-type parser, in the sense that it scans a "phrase" for
definitions before it starts parsing, and therefore allows for tags to be used
before they are defined, which gives some freedom in top-down programming.

This parser sees the program as a set of "phrases" that needs reducing from
the inside out (bottom up). For instance

		IF a = b THEN RE a ELSE  pi * (IM a - IM b) FI
 Phrase level 3 			      +-----------+
 Phrase level 2    +---+      +--+       +----------------+
 Phrase level 1 +--------------------------------------------+

Roughly speaking, the BU parser will first work out level 3, than level 2, and
finally the level 1 phrase.

Parsing progresses in various phases to avoid spurious diagnostics from a
recovering parser. Every phase "tightens" the grammar more.
An error in any phase makes the parser quit when that phase ends.
The parser is forgiving in case of superfluous semicolons.

So those are the parser phases:

 (1) Parenthesis are checked to see whether they match.

 (2) Then, a top-down parser determines the basic-block structure of the program
     so symbol tables can be set up that the bottom-up parser will consult
     as you can define things before they are applied.

 (3) A bottom-up parser tries to resolve the structure of the program.

 (4) After the symbol tables have been finalised, a small rearrangement of the
     tree may be required where JUMPs have no GOTO. This leads to the
     non-standard situation that JUMPs without GOTO can have the syntactic
     position of a PRIMARY, SECONDARY or TERTIARY. 

 (5) The bottom-up parser does not check VICTAL correctness of declarers. This
     is done separately. Also structure of format texts is checked separately.

The parser sets up symbol tables and populates them as far as needed to parse
the source. After the bottom-up parser terminates succesfully, the symbol tables
are completed.

Next, modes are collected and rules for well-formedness and structural equivalence
are applied.

Next tasks are the mode checker and coercion inserter. The syntax tree is traversed 
to determine and check all modes. Next the tree is traversed again to insert
coercions.

Algol 68 contexts are SOFT, WEAK, MEEK, FIRM and STRONG.
These contexts are increasing in strength:

  SOFT: Deproceduring
  WEAK: Dereferencing to REF [] or REF STRUCT
  MEEK: Deproceduring and dereferencing
  FIRM: MEEK followed by uniting
  STRONG: FIRM followed by rowing, widening or voiding

Furthermore you will see in this file next switches:

(1) FORCE_DEFLEXING allows assignment compatibility between FLEX and non FLEX
rows. This can only be the case when there is no danger of altering bounds of a
non FLEX row.

(2) ALIAS_DEFLEXING prohibits aliasing a FLEX row to a non FLEX row (vice versa
is no problem) so that one cannot alter the bounds of a non FLEX row by
aliasing it to a FLEX row. This is particularly the case when passing names as
parameters to procedures:

   PROC x = (REF STRING s) VOID: ..., PROC y = (REF [] CHAR c) VOID: ...;

   x (LOC STRING);    # OK #

   x (LOC [10] CHAR); # Not OK, suppose x changes bounds of s! #
   y (LOC STRING);    # OK #
   y (LOC [10] CHAR); # OK #

(3) SAFE_DEFLEXING sets FLEX row apart from non FLEX row. This holds for names,
not for values, so common things are not rejected, for instance

   STRING x = read string;
   [] CHAR y = read string

(4) NO_DEFLEXING sets FLEX row apart from non FLEX row.

Finally, a static scope checker inspects the source. Note that Algol 68 also 
needs synamic scope checking. This concludes the syntactical analysis of the
source.

*/

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

#define STOP_CHAR 127

#define IN_PRELUDE(p) (LINE_NUMBER (p) <= 0)
#define EOL(c) ((c) == NEWLINE_CHAR || (c) == NULL_CHAR)

static BOOL_T stop_scanner = A68_FALSE, read_error = A68_FALSE, no_preprocessing = A68_FALSE;
static char *scan_buf;
static int max_scan_buf_length, source_file_size;
static int reductions = 0;
static jmp_buf bottom_up_crash_exit, top_down_crash_exit;

static BOOL_T reduce_phrase (NODE_T *, int);
static BOOL_T victal_check_declarer (NODE_T *, int);
static NODE_T *reduce_dyadic (NODE_T *, int u);
static NODE_T *top_down_loop (NODE_T *);
static NODE_T *top_down_skip_unit (NODE_T *);
static void append_source_line (char *, SOURCE_LINE_T **, int *, char *);
static void elaborate_bold_tags (NODE_T *);
static void extract_declarations (NODE_T *);
static void extract_identities (NODE_T *);
static void extract_indicants (NODE_T *);
static void extract_labels (NODE_T *, int);
static void extract_operators (NODE_T *);
static void extract_priorities (NODE_T *);
static void extract_proc_identities (NODE_T *);
static void extract_proc_variables (NODE_T *);
static void extract_variables (NODE_T *);
static void ignore_superfluous_semicolons (NODE_T *);
static void recover_from_error (NODE_T *, int, BOOL_T);
static void reduce_arguments (NODE_T *);
static void reduce_basic_declarations (NODE_T *);
static void reduce_bounds (NODE_T *);
static void reduce_collateral_clauses (NODE_T *);
static void reduce_constructs (NODE_T *, int);
static void reduce_control_structure (NODE_T *, int);
static void reduce_declaration_lists (NODE_T *);
static void reduce_declarer_lists (NODE_T *);
static void reduce_declarers (NODE_T *, int);
static void reduce_deeper_clauses (NODE_T *);
static void reduce_deeper_clauses_driver (NODE_T *);
static void reduce_enclosed_clause_bits (NODE_T *, int);
static void reduce_enclosed_clauses (NODE_T *);
static void reduce_enquiry_clauses (NODE_T *);
static void reduce_erroneous_units (NODE_T *);
static void reduce_formal_declarer_pack (NODE_T *);
static void reduce_format_texts (NODE_T *);
static void reduce_formulae (NODE_T *);
static void reduce_generic_arguments (NODE_T *);
static void reduce_indicants (NODE_T *);
static void reduce_labels (NODE_T *);
static void reduce_lengtheties (NODE_T *);
static void reduce_parameter_pack (NODE_T *);
static void reduce_particular_program (NODE_T *);
static void reduce_primaries (NODE_T *, int);
static void reduce_primary_bits (NODE_T *, int);
static void reduce_right_to_left_constructs (NODE_T * q);
static void reduce_row_proc_op_declarers (NODE_T *);
static void reduce_secondaries (NODE_T *);
static void reduce_serial_clauses (NODE_T *);
static void reduce_small_declarers (NODE_T *);
static void reduce_specifiers (NODE_T *);
static void reduce_statements (NODE_T *, int);
static void reduce_struct_pack (NODE_T *);
static void reduce_subordinate (NODE_T *, int);
static void reduce_tertiaries (NODE_T *);
static void reduce_union_pack (NODE_T *);
static void reduce_units (NODE_T *);
static void try_reduction (NODE_T *, void (*)(NODE_T *), BOOL_T *, ...);
void file_format_error (int);

/* Standard environ */

static char bold_prelude_start[] = "\
BEGIN MODE DOUBLE = LONG REAL;!\
      start: commence:!\
      BEGIN!";

static char bold_postlude[] = "\
      END;!\
      stop: abort: halt: SKIP!\
END!";

static char quote_prelude_start[] = "\
'BEGIN' 'MODE' 'DOUBLE' = 'LONG' 'REAL',!\
               'QUAD' = 'LONG' 'LONG' 'REAL',!\
               'DEVICE' = 'FILE',!\
               'TEXT' = 'STRING';!\
        START: COMMENCE:!\
        'BEGIN'!";

static char quote_postlude[] = "\
        'END';!\
        STOP: ABORT: HALT: 'SKIP'!\
'END'!";

/*!
\brief add keyword to the tree
\param p top keyword
\param a attribute
\param t keyword text
**/

static void add_keyword (KEYWORD_T ** p, int a, char *t)
{
  while (*p != NULL) {
    int k = strcmp (t, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else {
      p = &MORE (*p);
    }
  }
  *p = (KEYWORD_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (KEYWORD_T));
  ATTRIBUTE (*p) = a;
  TEXT (*p) = t;
  LESS (*p) = MORE (*p) = NULL;
}

/*!
\brief make tables of keywords and non-terminals
**/

void set_up_tables (void)
{
/* Entries are randomised to balance the tree */
  if (program.options.strict == A68_FALSE) {
    add_keyword (&top_keyword, ENVIRON_SYMBOL, "ENVIRON");
    add_keyword (&top_keyword, DOWNTO_SYMBOL, "DOWNTO");
    add_keyword (&top_keyword, UNTIL_SYMBOL, "UNTIL");
    add_keyword (&top_keyword, CLASS_SYMBOL, "CLASS");
    add_keyword (&top_keyword, NEW_SYMBOL, "NEW");
    add_keyword (&top_keyword, DIAGONAL_SYMBOL, "DIAG");
    add_keyword (&top_keyword, TRANSPOSE_SYMBOL, "TRNSP");
    add_keyword (&top_keyword, ROW_SYMBOL, "ROW");
    add_keyword (&top_keyword, COLUMN_SYMBOL, "COL");
    add_keyword (&top_keyword, ROW_ASSIGN_SYMBOL, "::=");
    add_keyword (&top_keyword, CODE_SYMBOL, "CODE");
    add_keyword (&top_keyword, EDOC_SYMBOL, "EDOC");
    add_keyword (&top_keyword, ANDF_SYMBOL, "THEF");
    add_keyword (&top_keyword, ORF_SYMBOL, "ELSF");
    add_keyword (&top_keyword, ANDF_SYMBOL, "ANDTH");
    add_keyword (&top_keyword, ORF_SYMBOL, "OREL");
    add_keyword (&top_keyword, ANDF_SYMBOL, "ANDF");
    add_keyword (&top_keyword, ORF_SYMBOL, "ORF");
  }
  add_keyword (&top_keyword, POINT_SYMBOL, ".");
  add_keyword (&top_keyword, COMPLEX_SYMBOL, "COMPLEX");
  add_keyword (&top_keyword, ACCO_SYMBOL, "{");
  add_keyword (&top_keyword, OCCA_SYMBOL, "}");
  add_keyword (&top_keyword, SOUND_SYMBOL, "SOUND");
  add_keyword (&top_keyword, COLON_SYMBOL, ":");
  add_keyword (&top_keyword, THEN_BAR_SYMBOL, "|");
  add_keyword (&top_keyword, SUB_SYMBOL, "[");
  add_keyword (&top_keyword, BY_SYMBOL, "BY");
  add_keyword (&top_keyword, OP_SYMBOL, "OP");
  add_keyword (&top_keyword, COMMA_SYMBOL, ",");
  add_keyword (&top_keyword, AT_SYMBOL, "AT");
  add_keyword (&top_keyword, PRIO_SYMBOL, "PRIO");
  add_keyword (&top_keyword, STYLE_I_COMMENT_SYMBOL, "CO");
  add_keyword (&top_keyword, END_SYMBOL, "END");
  add_keyword (&top_keyword, GO_SYMBOL, "GO");
  add_keyword (&top_keyword, TO_SYMBOL, "TO");
  add_keyword (&top_keyword, ELSE_BAR_SYMBOL, "|:");
  add_keyword (&top_keyword, THEN_SYMBOL, "THEN");
  add_keyword (&top_keyword, TRUE_SYMBOL, "TRUE");
  add_keyword (&top_keyword, PROC_SYMBOL, "PROC");
  add_keyword (&top_keyword, FOR_SYMBOL, "FOR");
  add_keyword (&top_keyword, GOTO_SYMBOL, "GOTO");
  add_keyword (&top_keyword, WHILE_SYMBOL, "WHILE");
  add_keyword (&top_keyword, IS_SYMBOL, ":=:");
  add_keyword (&top_keyword, ASSIGN_TO_SYMBOL, "=:");
  add_keyword (&top_keyword, COMPL_SYMBOL, "COMPL");
  add_keyword (&top_keyword, FROM_SYMBOL, "FROM");
  add_keyword (&top_keyword, BOLD_PRAGMAT_SYMBOL, "PRAGMAT");
  add_keyword (&top_keyword, BOLD_COMMENT_SYMBOL, "COMMENT");
  add_keyword (&top_keyword, DO_SYMBOL, "DO");
  add_keyword (&top_keyword, STYLE_II_COMMENT_SYMBOL, "#");
  add_keyword (&top_keyword, CASE_SYMBOL, "CASE");
  add_keyword (&top_keyword, LOC_SYMBOL, "LOC");
  add_keyword (&top_keyword, CHAR_SYMBOL, "CHAR");
  add_keyword (&top_keyword, ISNT_SYMBOL, ":/=:");
  add_keyword (&top_keyword, REF_SYMBOL, "REF");
  add_keyword (&top_keyword, NIL_SYMBOL, "NIL");
  add_keyword (&top_keyword, ASSIGN_SYMBOL, ":=");
  add_keyword (&top_keyword, FI_SYMBOL, "FI");
  add_keyword (&top_keyword, FILE_SYMBOL, "FILE");
  add_keyword (&top_keyword, PAR_SYMBOL, "PAR");
  add_keyword (&top_keyword, ASSERT_SYMBOL, "ASSERT");
  add_keyword (&top_keyword, OUSE_SYMBOL, "OUSE");
  add_keyword (&top_keyword, IN_SYMBOL, "IN");
  add_keyword (&top_keyword, LONG_SYMBOL, "LONG");
  add_keyword (&top_keyword, SEMI_SYMBOL, ";");
  add_keyword (&top_keyword, EMPTY_SYMBOL, "EMPTY");
  add_keyword (&top_keyword, MODE_SYMBOL, "MODE");
  add_keyword (&top_keyword, IF_SYMBOL, "IF");
  add_keyword (&top_keyword, OD_SYMBOL, "OD");
  add_keyword (&top_keyword, OF_SYMBOL, "OF");
  add_keyword (&top_keyword, STRUCT_SYMBOL, "STRUCT");
  add_keyword (&top_keyword, STYLE_I_PRAGMAT_SYMBOL, "PR");
  add_keyword (&top_keyword, BUS_SYMBOL, "]");
  add_keyword (&top_keyword, SKIP_SYMBOL, "SKIP");
  add_keyword (&top_keyword, SHORT_SYMBOL, "SHORT");
  add_keyword (&top_keyword, IS_SYMBOL, "IS");
  add_keyword (&top_keyword, ESAC_SYMBOL, "ESAC");
  add_keyword (&top_keyword, CHANNEL_SYMBOL, "CHANNEL");
  add_keyword (&top_keyword, REAL_SYMBOL, "REAL");
  add_keyword (&top_keyword, STRING_SYMBOL, "STRING");
  add_keyword (&top_keyword, BOOL_SYMBOL, "BOOL");
  add_keyword (&top_keyword, ISNT_SYMBOL, "ISNT");
  add_keyword (&top_keyword, FALSE_SYMBOL, "FALSE");
  add_keyword (&top_keyword, UNION_SYMBOL, "UNION");
  add_keyword (&top_keyword, OUT_SYMBOL, "OUT");
  add_keyword (&top_keyword, OPEN_SYMBOL, "(");
  add_keyword (&top_keyword, BEGIN_SYMBOL, "BEGIN");
  add_keyword (&top_keyword, FLEX_SYMBOL, "FLEX");
  add_keyword (&top_keyword, VOID_SYMBOL, "VOID");
  add_keyword (&top_keyword, BITS_SYMBOL, "BITS");
  add_keyword (&top_keyword, ELSE_SYMBOL, "ELSE");
  add_keyword (&top_keyword, EXIT_SYMBOL, "EXIT");
  add_keyword (&top_keyword, HEAP_SYMBOL, "HEAP");
  add_keyword (&top_keyword, INT_SYMBOL, "INT");
  add_keyword (&top_keyword, BYTES_SYMBOL, "BYTES");
  add_keyword (&top_keyword, PIPE_SYMBOL, "PIPE");
  add_keyword (&top_keyword, FORMAT_SYMBOL, "FORMAT");
  add_keyword (&top_keyword, SEMA_SYMBOL, "SEMA");
  add_keyword (&top_keyword, CLOSE_SYMBOL, ")");
  add_keyword (&top_keyword, AT_SYMBOL, "@");
  add_keyword (&top_keyword, ELIF_SYMBOL, "ELIF");
  add_keyword (&top_keyword, FORMAT_DELIMITER_SYMBOL, "$");
}

/*!
\brief save scanner state, for character look-ahead
\param ref_l source line
\param ref_s position in source line text
\param ch last scanned character
**/

static void save_state (SOURCE_LINE_T * ref_l, char *ref_s, char ch)
{
  program.scan_state.save_l = ref_l;
  program.scan_state.save_s = ref_s;
  program.scan_state.save_c = ch;
}

/*!
\brief restore scanner state, for character look-ahead
\param ref_l source line
\param ref_s position in source line text
\param ch last scanned character
**/

static void restore_state (SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  *ref_l = program.scan_state.save_l;
  *ref_s = program.scan_state.save_s;
  *ch = program.scan_state.save_c;
}

/*!
\brief whether ch is unworthy
\param u source line with error
\param v character in line
\param ch
**/

static void unworthy (SOURCE_LINE_T * u, char *v, char ch)
{
  if (IS_PRINT (ch)) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s", ERROR_UNWORTHY_CHARACTER) >= 0);
  } else {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s %s", ERROR_UNWORTHY_CHARACTER, ctrl_char (ch)) >= 0);
  }
  scan_error (u, v, edit_line);
}

/*!
\brief concatenate lines that terminate in '\' with next line
\param top top source line
**/

static void concatenate_lines (SOURCE_LINE_T * top)
{
  SOURCE_LINE_T *q;
/* Work from bottom backwards */
  for (q = top; q != NULL && NEXT (q) != NULL; q = NEXT (q)) {
    ;
  }
  for (; q != NULL; q = PREVIOUS (q)) {
    char *z = q->string;
    int len = (int) strlen (z);
    if (len >= 2 && z[len - 2] == ESCAPE_CHAR && z[len - 1] == NEWLINE_CHAR && NEXT (q) != NULL && NEXT (q)->string != NULL) {
      z[len - 2] = NULL_CHAR;
      len += (int) strlen (NEXT (q)->string);
      z = (char *) get_fixed_heap_space ((size_t) (len + 1));
      bufcpy (z, q->string, len + 1);
      bufcat (z, NEXT (q)->string, len + 1);
      NEXT (q)->string[0] = NULL_CHAR;
      q->string = z;
    }
  }
}

/*!
\brief whether u is bold tag v, independent of stropping regime
\param z source line
\param u symbol under test
\param v bold symbol 
\return whether u is v
**/

static BOOL_T whether_bold (char *u, char *v)
{
  unsigned len = (unsigned) strlen (v);
  if (program.options.stropping == QUOTE_STROPPING) {
    if (u[0] == '\'') {
      return ((BOOL_T) (strncmp (++u, v, len) == 0 && u[len] == '\''));
    } else {
      return (A68_FALSE);
    }
  } else {
    return ((BOOL_T) (strncmp (u, v, len) == 0 && !IS_UPPER (u[len])));
  }
}

/*!
\brief skip string
\param top current source line
\param ch current character in source line
\return whether string is properly terminated
**/

static BOOL_T skip_string (SOURCE_LINE_T ** top, char **ch)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  v++;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      if (v[0] == QUOTE_CHAR && v[1] != QUOTE_CHAR) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (v[0] == QUOTE_CHAR && v[1] == QUOTE_CHAR) {
        v += 2;
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  return (A68_FALSE);
}

/*!
\brief skip comment
\param top current source line
\param ch current character in source line
\param delim expected terminating delimiter
\return whether comment is properly terminated
**/

static BOOL_T skip_comment (SOURCE_LINE_T ** top, char **ch, int delim)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  v++;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      if (whether_bold (v, "COMMENT") && delim == BOLD_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (whether_bold (v, "CO") && delim == STYLE_I_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (v[0] == '#' && delim == STYLE_II_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  return (A68_FALSE);
}

/*!
\brief skip rest of pragmat
\param top current source line
\param ch current character in source line
\param delim expected terminating delimiter
\param whitespace whether other pragmat items are allowed
\return whether pragmat is properly terminated
**/

static BOOL_T skip_pragmat (SOURCE_LINE_T ** top, char **ch, int delim, BOOL_T whitespace)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      if (whether_bold (v, "PRAGMAT") && delim == BOLD_PRAGMAT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (whether_bold (v, "PR")
                 && delim == STYLE_I_PRAGMAT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else {
        if (whitespace && !IS_SPACE (v[0]) && v[0] != NEWLINE_CHAR) {
          scan_error (u, v, ERROR_PRAGMENT);
        } else if (IS_UPPER (v[0])) {
/* Skip a bold word as you may trigger on REPR, for instance .. */
          while (IS_UPPER (v[0])) {
            v++;
          }
        } else {
          v++;
        }
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  return (A68_FALSE);
}

/*!
\brief return pointer to next token within pragmat
\param top current source line
\param ch current character in source line
\return pointer to next item, NULL if none remains
**/

static char *get_pragmat_item (SOURCE_LINE_T ** top, char **ch)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      if (!IS_SPACE (v[0]) && v[0] != NEWLINE_CHAR) {
        *top = u;
        *ch = v;
        return (v);
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  return (NULL);
}

/*!
\brief case insensitive strncmp for at most the number of chars in 'v'
\param u string 1, must not be NULL
\param v string 2, must not be NULL
\return alphabetic difference between 1 and 2
**/

static int streq (char *u, char *v)
{
  int diff;
  for (diff = 0; diff == 0 && u[0] != NULL_CHAR && v[0] != NULL_CHAR; u++, v++) {
    diff = ((int) TO_LOWER (u[0])) - ((int) TO_LOWER (v[0]));
  }
  return (diff);
}

/*!
\brief scan for next pragmat and yield first pragmat item
\param top current source line
\param ch current character in source line
\param delim expected terminating delimiter
\return pointer to next item or NULL if none remain
**/

static char *next_preprocessor_item (SOURCE_LINE_T ** top, char **ch, int *delim)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  *delim = 0;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      SOURCE_LINE_T *start_l = u;
      char *start_c = v;
/* STRINGs must be skipped */
      if (v[0] == QUOTE_CHAR) {
        SCAN_ERROR (!skip_string (&u, &v), start_l, start_c, ERROR_UNTERMINATED_STRING);
      }
/* COMMENTS must be skipped */
      else if (whether_bold (v, "COMMENT")) {
        SCAN_ERROR (!skip_comment (&u, &v, BOLD_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (whether_bold (v, "CO")) {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_I_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (v[0] == '#') {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_II_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (whether_bold (v, "PRAGMAT") || whether_bold (v, "PR")) {
/* We caught a PRAGMAT */
        char *item;
        if (whether_bold (v, "PRAGMAT")) {
          *delim = BOLD_PRAGMAT_SYMBOL;
          v = &v[strlen ("PRAGMAT")];
        } else if (whether_bold (v, "PR")) {
          *delim = STYLE_I_PRAGMAT_SYMBOL;
          v = &v[strlen ("PR")];
        }
        item = get_pragmat_item (&u, &v);
        SCAN_ERROR (item == NULL, start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
/* Item "preprocessor" restarts preprocessing if it is off */
        if (no_preprocessing && streq (item, "PREPROCESSOR") == 0) {
          no_preprocessing = A68_FALSE;
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
/* If preprocessing is switched off, we idle to closing bracket */
        else if (no_preprocessing) {
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_FALSE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
/* Item "nopreprocessor" stops preprocessing if it is on */
        if (streq (item, "NOPREPROCESSOR") == 0) {
          no_preprocessing = A68_TRUE;
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
/* Item "INCLUDE" includes a file */
        else if (streq (item, "INCLUDE") == 0) {
          *top = u;
          *ch = v;
          return (item);
        }
/* Item "READ" includes a file */
        else if (streq (item, "READ") == 0) {
          *top = u;
          *ch = v;
          return (item);
        }
/* Unrecognised item - probably options handled later by the tokeniser */
        else {
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_FALSE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
      } else if (IS_UPPER (v[0])) {
/* Skip a bold word as you may trigger on REPR, for instance .. */
        while (IS_UPPER (v[0])) {
          v++;
        }
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  *top = u;
  *ch = v;
  return (NULL);
}

/*!
\brief include files
\param top top source line
**/

static void include_files (SOURCE_LINE_T * top)
{
/*
include_files

syntax: PR read "filename" PR
        PR include "filename" PR

The file gets inserted before the line containing the pragmat. In this way
correct line numbers are preserved which helps diagnostics. A file that has
been included will not be included a second time - it will be ignored. 
*/
  BOOL_T make_pass = A68_TRUE;
  while (make_pass) {
    SOURCE_LINE_T *s, *t, *u = top;
    char *v = &(u->string[0]);
    make_pass = A68_FALSE;
    RESET_ERRNO;
    while (u != NULL) {
      int pr_lim;
      char *item = next_preprocessor_item (&u, &v, &pr_lim);
      SOURCE_LINE_T *start_l = u;
      char *start_c = v;
/* Search for PR include "filename" PR */
      if (item != NULL && (streq (item, "INCLUDE") == 0 || streq (item, "READ") == 0)) {
        FILE_T fd;
        int n, linum, fsize, k, bytes_read, fnwid;
        char *fbuf, delim;
        char fnb[BUFFER_SIZE], *fn;
/* Skip to filename */
        if (streq (item, "INCLUDE") == 0) {
          v = &v[strlen ("INCLUDE")];
        } else {
          v = &v[strlen ("READ")];
        }
        while (IS_SPACE (v[0])) {
          v++;
        }
/* Scan quoted filename */
        SCAN_ERROR ((v[0] != QUOTE_CHAR && v[0] != '\''), start_l, start_c, ERROR_INCORRECT_FILENAME);
        delim = (v++)[0];
        n = 0;
        fnb[0] = NULL_CHAR;
/* Scan Algol 68 string (note: "" denotes a ", while in C it concatenates).*/
        do {
          SCAN_ERROR (EOL (v[0]), start_l, start_c, ERROR_INCORRECT_FILENAME);
          SCAN_ERROR (n == BUFFER_SIZE - 1, start_l, start_c, ERROR_INCORRECT_FILENAME);
          if (v[0] == delim) {
            while (v[0] == delim && v[1] == delim) {
              SCAN_ERROR (n == BUFFER_SIZE - 1, start_l, start_c, ERROR_INCORRECT_FILENAME);
              fnb[n++] = delim;
              fnb[n] = NULL_CHAR;
              v += 2;
            }
          } else if (IS_PRINT (v[0])) {
            fnb[n++] = *(v++);
            fnb[n] = NULL_CHAR;
          } else {
            SCAN_ERROR (A68_TRUE, start_l, start_c, ERROR_INCORRECT_FILENAME);
          }
        } while (v[0] != delim);
/* Insist that the pragmat is closed properly */
        v = &v[1];
        SCAN_ERROR (!skip_pragmat (&u, &v, pr_lim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
/* Filename valid? */
        SCAN_ERROR (n == 0, start_l, start_c, ERROR_INCORRECT_FILENAME);
        fnwid = (int) strlen (program.files.path) + (int) strlen (fnb) + 1;
        fn = (char *) get_fixed_heap_space ((size_t) fnwid);
        bufcpy (fn, program.files.path, fnwid);
        bufcat (fn, fnb, fnwid);
/* Recursive include? Then *ignore* the file */
        for (t = top; t != NULL; t = NEXT (t)) {
          if (strcmp (t->filename, fn) == 0) {
            goto search_next_pragmat;   /* Eeek! */
          }
        }
/* Access the file */
        RESET_ERRNO;
        fd = open (fn, O_RDONLY | O_BINARY);
        SCAN_ERROR (fd == -1, start_l, start_c, ERROR_SOURCE_FILE_OPEN);
/* Access the file */
        RESET_ERRNO;
        fsize = (int) lseek (fd, 0, SEEK_END);
        ASSERT (fsize >= 0);
        SCAN_ERROR (errno != 0, start_l, start_c, ERROR_FILE_READ);
        fbuf = (char *) get_temp_heap_space ((unsigned) (8 + fsize));
        RESET_ERRNO;
        ASSERT (lseek (fd, 0, SEEK_SET) >= 0);
        SCAN_ERROR (errno != 0, start_l, start_c, ERROR_FILE_READ);
        RESET_ERRNO;
        bytes_read = (int) io_read (fd, fbuf, (size_t) fsize);
        SCAN_ERROR (errno != 0 || bytes_read != fsize, start_l, start_c, ERROR_FILE_READ);
/* Buffer still usable? */
        if (fsize > max_scan_buf_length) {
          max_scan_buf_length = fsize;
          scan_buf = (char *)
            get_temp_heap_space ((unsigned)
                                 (8 + max_scan_buf_length));
        }
/* Link all lines into the list */
        linum = 1;
        s = u;
        t = PREVIOUS (u);
        k = 0;
        while (k < fsize) {
          n = 0;
          scan_buf[0] = NULL_CHAR;
          while (k < fsize && fbuf[k] != NEWLINE_CHAR) {
            SCAN_ERROR ((IS_CNTRL (fbuf[k]) && !IS_SPACE (fbuf[k])) || fbuf[k] == STOP_CHAR, start_l, start_c, ERROR_FILE_INCLUDE_CTRL);
            scan_buf[n++] = fbuf[k++];
            scan_buf[n] = NULL_CHAR;
          }
          scan_buf[n++] = NEWLINE_CHAR;
          scan_buf[n] = NULL_CHAR;
          if (k < fsize) {
            k++;
          }
          append_source_line (scan_buf, &t, &linum, fn);
        }
/* Conclude and go find another include directive, if any */
        NEXT (t) = s;
        PREVIOUS (s) = t;
        concatenate_lines (top);
        ASSERT (close (fd) == 0);
        make_pass = A68_TRUE;
      }
    search_next_pragmat:       /* skip */ ;
    }
  }
}

/*!
\brief append a source line to the internal source file
\param str text line to be appended
\param ref_l previous source line
\param line_num previous source line number
\param filename name of file being read
**/

static void append_source_line (char *str, SOURCE_LINE_T ** ref_l, int *line_num, char *filename)
{
  SOURCE_LINE_T *z = new_source_line ();
/* Allow shell command in first line, f.i. "#!/usr/share/bin/a68g" */
  if (*line_num == 1) {
    if (strlen (str) >= 2 && strncmp (str, "#!", 2) == 0) {
      ABEND (strstr (str, "run-script") != NULL, ERROR_SHELL_SCRIPT, NULL);
      (*line_num)++;
      return;
    }
  }
  if (program.options.reductions) {
    WRITELN (STDOUT_FILENO, "\"");
    WRITE (STDOUT_FILENO, str);
    WRITE (STDOUT_FILENO, "\"");
  }
/* Link line into the chain */
  z->string = new_fixed_string (str);
  z->filename = filename;
  NUMBER (z) = (*line_num)++;
  z->print_status = NOT_PRINTED;
  z->list = A68_TRUE;
  z->diagnostics = NULL;
  NEXT (z) = NULL;
  PREVIOUS (z) = *ref_l;
  if (program.top_line == NULL) {
    program.top_line = z;
  }
  if (*ref_l != NULL) {
    NEXT (*ref_l) = z;
  }
  *ref_l = z;
}

/*!
\brief size of source file
\return size of file
**/

static int get_source_size (void)
{
  FILE_T f = program.files.source.fd;
/* This is why WIN32 must open as "read binary" */
  return ((int) lseek (f, 0, SEEK_END));
}

/*!
\brief append environment source lines
\param str line to append
\param ref_l source line after which to append
\param line_num number of source line 'ref_l'
\param name either "prelude" or "postlude"
**/

static void append_environ (char *str, SOURCE_LINE_T ** ref_l, int *line_num, char *name)
{
  char *text = new_string (str);
  while (text != NULL && text[0] != NULL_CHAR) {
    char *car = text;
    char *cdr = a68g_strchr (text, '!');
    int zero_line_num = 0;
    cdr[0] = NULL_CHAR;
    text = &cdr[1];
    (*line_num)++;
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s\n", car) >= 0);
    append_source_line (edit_line, ref_l, &zero_line_num, name);
  }
}

/*!
\brief read script file and make internal copy
\return whether reading is satisfactory 
**/

static BOOL_T read_script_file (void)
{
  SOURCE_LINE_T * ref_l = NULL;
  int k, n, num;
  unsigned len;
  BOOL_T file_end = A68_FALSE;
  char filename[BUFFER_SIZE], linenum[BUFFER_SIZE];
  char ch, * fn, * line; 
  char * buffer = (char *) get_temp_heap_space ((unsigned) (8 + source_file_size));
  FILE_T source = program.files.source.fd;
  ABEND (source == -1, "source file not open", NULL);
  buffer[0] = NULL_CHAR;
  n = 0;
  len = (unsigned) (8 + source_file_size);
  buffer = (char *) get_temp_heap_space (len);
  ASSERT (lseek (source, 0, SEEK_SET) >= 0);
  while (!file_end) {
/* Read the original file name */
    filename[0] = NULL_CHAR;
    k = 0;
    if (io_read (source, &ch, 1) == 0) {
      file_end = A68_TRUE;
      continue;
    }
    while (ch != NEWLINE_CHAR) {
      filename[k++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
    }
    filename[k] = NULL_CHAR;
    fn = TEXT (add_token (&top_token, filename));
/* Read the original file number */
    linenum[0] = NULL_CHAR;
    k = 0;
    ASSERT (io_read (source, &ch, 1) == 1);
    while (ch != NEWLINE_CHAR) {
      linenum[k++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
    }
    linenum[k] = NULL_CHAR;
    num = strtol (linenum, NULL, 10);
    ABEND (errno == ERANGE, "strange line number", NULL);
/* COPY original line into buffer */
    ASSERT (io_read (source, &ch, 1) == 1);
    line = &buffer[n];
    while (ch != NEWLINE_CHAR) {
      buffer[n++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
      ABEND ((unsigned) n >= len, "buffer overflow", NULL);
    }
    buffer[n++] = NEWLINE_CHAR;
    buffer[n] = NULL_CHAR;
    append_source_line (line, &ref_l, &num, fn);
  }
  return (A68_TRUE);
}

/*!
\brief read source file and make internal copy
\return whether reading is satisfactory 
**/

static BOOL_T read_source_file (void)
{
  SOURCE_LINE_T *ref_l = NULL;
  int line_num = 0, k, bytes_read;
  ssize_t l;
  FILE_T f = program.files.source.fd;
  char *prelude_start, *postlude, *buffer;
/* Prelude */
  if (program.options.stropping == UPPER_STROPPING) {
    prelude_start = bold_prelude_start;
    postlude = bold_postlude;
  } else if (program.options.stropping == QUOTE_STROPPING) {
    prelude_start = quote_prelude_start;
    postlude = quote_postlude;
  } else {
    prelude_start = NULL;
    postlude = NULL;
  }
  append_environ (prelude_start, &ref_l, &line_num, "prelude");
/* Read the file into a single buffer, so we save on system calls */
  line_num = 1;
  buffer = (char *) get_temp_heap_space ((unsigned) (8 + source_file_size));
  RESET_ERRNO;
  ASSERT (lseek (f, 0, SEEK_SET) >= 0);
  ABEND (errno != 0, "error while reading source file", NULL);
  RESET_ERRNO;
  bytes_read = (int) io_read (f, buffer, (size_t) source_file_size);
  ABEND (errno != 0 || bytes_read != source_file_size, "error while reading source file", NULL);
/* Link all lines into the list */
  k = 0;
  while (k < source_file_size) {
    l = 0;
    scan_buf[0] = NULL_CHAR;
    while (k < source_file_size && buffer[k] != NEWLINE_CHAR) {
      if (k < source_file_size - 1 && buffer[k] == CR_CHAR && buffer[k + 1] == NEWLINE_CHAR) {
        k++;
      } else {
        scan_buf[l++] = buffer[k++];
        scan_buf[l] = NULL_CHAR;
      }
    }
    scan_buf[l++] = NEWLINE_CHAR;
    scan_buf[l] = NULL_CHAR;
    if (k < source_file_size) {
      k++;
    }
    append_source_line (scan_buf, &ref_l, &line_num, program.files.source.name);
    SCAN_ERROR (l != (ssize_t) strlen (scan_buf), NULL, NULL, ERROR_FILE_SOURCE_CTRL);
  }
/* Postlude */
  append_environ (postlude, &ref_l, &line_num, "postlude");
/* Concatenate lines */
  concatenate_lines (program.top_line);
/* Include files */
  include_files (program.top_line);
  return (A68_TRUE);
}

/*!
\brief next_char get next character from internal copy of source file
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\param allow_typo whether typographical display features are allowed
\return next char on input
**/

static char next_char (SOURCE_LINE_T ** ref_l, char **ref_s, BOOL_T allow_typo)
{
  char ch;
#if defined NO_TYPO
  allow_typo = A68_FALSE;
#endif
  LOW_STACK_ALERT (NULL);
/* Source empty? */
  if (*ref_l == NULL) {
    return (STOP_CHAR);
  } else {
    (*ref_l)->list = (BOOL_T) (program.options.nodemask & SOURCE_MASK ? A68_TRUE : A68_FALSE);
/* Take new line? */
    if ((*ref_s)[0] == NEWLINE_CHAR || (*ref_s)[0] == NULL_CHAR) {
      *ref_l = NEXT (*ref_l);
      if (*ref_l == NULL) {
        return (STOP_CHAR);
      }
      *ref_s = (*ref_l)->string;
    } else {
      (*ref_s)++;
    }
/* Deliver next char */
    ch = (*ref_s)[0];
    if (allow_typo && (IS_SPACE (ch) || ch == FORMFEED_CHAR)) {
      ch = next_char (ref_l, ref_s, allow_typo);
    }
    return (ch);
  }
}

/*!
\brief find first character that can start a valid symbol
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
**/

static void get_good_char (char *ref_c, SOURCE_LINE_T ** ref_l, char **ref_s)
{
  while (*ref_c != STOP_CHAR && (IS_SPACE (*ref_c) || (*ref_c == NULL_CHAR))) {
    if (*ref_l != NULL) {
      (*ref_l)->list = (BOOL_T) (program.options.nodemask & SOURCE_MASK ? A68_TRUE : A68_FALSE);
    }
    *ref_c = next_char (ref_l, ref_s, A68_FALSE);
  }
}

/*!
\brief handle a pragment (pragmat or comment)
\param type type of pragment (#, CO, COMMENT, PR, PRAGMAT)
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
**/

static void pragment (int type, SOURCE_LINE_T ** ref_l, char **ref_c)
{
#define INIT_BUFFER {chars_in_buf = 0; scan_buf[chars_in_buf] = NULL_CHAR;}
#define ADD_ONE_CHAR(ch) {scan_buf[chars_in_buf ++] = ch; scan_buf[chars_in_buf] = NULL_CHAR;}
  char c = **ref_c, *term_s = NULL, *start_c = *ref_c;
  SOURCE_LINE_T *start_l = *ref_l;
  int term_s_length, chars_in_buf;
  BOOL_T stop;
/* Set terminator */
  if (program.options.stropping == UPPER_STROPPING) {
    if (type == STYLE_I_COMMENT_SYMBOL) {
      term_s = "CO";
    } else if (type == STYLE_II_COMMENT_SYMBOL) {
      term_s = "#";
    } else if (type == BOLD_COMMENT_SYMBOL) {
      term_s = "COMMENT";
    } else if (type == STYLE_I_PRAGMAT_SYMBOL) {
      term_s = "PR";
    } else if (type == BOLD_PRAGMAT_SYMBOL) {
      term_s = "PRAGMAT";
    }
  } else if (program.options.stropping == QUOTE_STROPPING) {
    if (type == STYLE_I_COMMENT_SYMBOL) {
      term_s = "'CO'";
    } else if (type == STYLE_II_COMMENT_SYMBOL) {
      term_s = "#";
    } else if (type == BOLD_COMMENT_SYMBOL) {
      term_s = "'COMMENT'";
    } else if (type == STYLE_I_PRAGMAT_SYMBOL) {
      term_s = "'PR'";
    } else if (type == BOLD_PRAGMAT_SYMBOL) {
      term_s = "'PRAGMAT'";
    }
  }
  term_s_length = (int) strlen (term_s);
/* Scan for terminator, and process pragmat items */
  INIT_BUFFER;
  get_good_char (&c, ref_l, ref_c);
  stop = A68_FALSE;
  while (stop == A68_FALSE) {
    SCAN_ERROR (c == STOP_CHAR, start_l, start_c, ERROR_UNTERMINATED_PRAGMENT);
/* A ".." or '..' delimited string in a PRAGMAT */
    if ((c == QUOTE_CHAR || (c == '\'' && program.options.stropping == UPPER_STROPPING))
        && (type == STYLE_I_PRAGMAT_SYMBOL || type == BOLD_PRAGMAT_SYMBOL)) {
      char delim = c;
      BOOL_T eos = A68_FALSE;
      ADD_ONE_CHAR (c);
      c = next_char (ref_l, ref_c, A68_FALSE);
      while (!eos) {
        SCAN_ERROR (EOL (c), start_l, start_c, ERROR_LONG_STRING);
        if (c == delim) {
          ADD_ONE_CHAR (delim);
          c = next_char (ref_l, ref_c, A68_FALSE);
          save_state (*ref_l, *ref_c, c);
          if (c == delim) {
            c = next_char (ref_l, ref_c, A68_FALSE);
          } else {
            restore_state (ref_l, ref_c, &c);
            eos = A68_TRUE;
          }
        } else if (IS_PRINT (c)) {
          ADD_ONE_CHAR (c);
          c = next_char (ref_l, ref_c, A68_FALSE);
        } else {
          unworthy (start_l, start_c, c);
        }
      }
    }
/* On newline we empty the buffer and scan options when appropriate */
    else if (EOL (c)) {
      if (type == STYLE_I_PRAGMAT_SYMBOL || type == BOLD_PRAGMAT_SYMBOL) {
        isolate_options (scan_buf, start_l);
      }
      INIT_BUFFER;
    } else if (IS_PRINT (c)) {
      ADD_ONE_CHAR (c);
    }
    if (chars_in_buf >= term_s_length) {
/* Check whether we encountered the terminator */
      stop = (BOOL_T) (strcmp (term_s, &(scan_buf[chars_in_buf - term_s_length])) == 0);
    }
    c = next_char (ref_l, ref_c, A68_FALSE);
  }
  scan_buf[chars_in_buf - term_s_length] = NULL_CHAR;
#undef ADD_ONE_CHAR
#undef INIT_BUFFER
}

/*!
\brief attribute for format item
\param ch format item in character form
\return same
**/

static int get_format_item (char ch)
{
  switch (TO_LOWER (ch)) {
    case 'a': { return (FORMAT_ITEM_A); }
    case 'b': { return (FORMAT_ITEM_B); }
    case 'c': { return (FORMAT_ITEM_C); }
    case 'd': { return (FORMAT_ITEM_D); }
    case 'e': { return (FORMAT_ITEM_E); }
    case 'f': { return (FORMAT_ITEM_F); }
    case 'g': { return (FORMAT_ITEM_G); }
    case 'h': { return (FORMAT_ITEM_H); }
    case 'i': { return (FORMAT_ITEM_I); }
    case 'j': { return (FORMAT_ITEM_J); }
    case 'k': { return (FORMAT_ITEM_K); }
    case 'l': 
    case '/': { return (FORMAT_ITEM_L); }
    case 'm': { return (FORMAT_ITEM_M); }
    case 'n': { return (FORMAT_ITEM_N); }
    case 'o': { return (FORMAT_ITEM_O); }
    case 'p': { return (FORMAT_ITEM_P); }
    case 'q': { return (FORMAT_ITEM_Q); }
    case 'r': { return (FORMAT_ITEM_R); }
    case 's': { return (FORMAT_ITEM_S); }
    case 't': { return (FORMAT_ITEM_T); }
    case 'u': { return (FORMAT_ITEM_U); }
    case 'v': { return (FORMAT_ITEM_V); }
    case 'w': { return (FORMAT_ITEM_W); }
    case 'x': { return (FORMAT_ITEM_X); }
    case 'y': { return (FORMAT_ITEM_Y); }
    case 'z': { return (FORMAT_ITEM_Z); }
    case '+': { return (FORMAT_ITEM_PLUS); }
    case '-': { return (FORMAT_ITEM_MINUS); }
    case POINT_CHAR: { return (FORMAT_ITEM_POINT); }
    case '%': { return (FORMAT_ITEM_ESCAPE); }
    default: { return (0); }
  }
}

/* Macros */

#define SCAN_DIGITS(c)\
  while (IS_DIGIT (c)) {\
    (sym++)[0] = (c);\
    (c) = next_char (ref_l, ref_s, A68_TRUE);\
  }

#define SCAN_EXPONENT_PART(c)\
  (sym++)[0] = EXPONENT_CHAR;\
  (c) = next_char (ref_l, ref_s, A68_TRUE);\
  if ((c) == '+' || (c) == '-') {\
    (sym++)[0] = (c);\
    (c) = next_char (ref_l, ref_s, A68_TRUE);\
  }\
  SCAN_ERROR (!IS_DIGIT (c), *start_l, *start_c, ERROR_EXPONENT_DIGIT);\
  SCAN_DIGITS (c)

/*!
\brief whether input shows exponent character
\param m module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\param ch last scanned char
\return same
**/

static BOOL_T whether_exp_char (SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  char exp_syms[3];
  if (program.options.stropping == UPPER_STROPPING) {
    exp_syms[0] = EXPONENT_CHAR;
    exp_syms[1] = (char) TO_UPPER (EXPONENT_CHAR);
    exp_syms[2] = NULL_CHAR;
  } else {
    exp_syms[0] = (char) TO_UPPER (EXPONENT_CHAR);
    exp_syms[1] = ESCAPE_CHAR;
    exp_syms[2] = NULL_CHAR;
  }
  save_state (*ref_l, *ref_s, *ch);
  if (strchr (exp_syms, *ch) != NULL) {
    *ch = next_char (ref_l, ref_s, A68_TRUE);
    ret = (BOOL_T) (strchr ("+-0123456789", *ch) != NULL);
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/*!
\brief whether input shows radix character
\param m module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\return same
**/

static BOOL_T whether_radix_char (SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  save_state (*ref_l, *ref_s, *ch);
  if (program.options.stropping == QUOTE_STROPPING) {
    if (*ch == TO_UPPER (RADIX_CHAR)) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("0123456789ABCDEF", *ch) != NULL);
    }
  } else {
    if (*ch == RADIX_CHAR) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("0123456789abcdef", *ch) != NULL);
    }
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/*!
\brief whether input shows decimal point
\param m module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\return same
**/

static BOOL_T whether_decimal_point (SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  save_state (*ref_l, *ref_s, *ch);
  if (*ch == POINT_CHAR) {
    char exp_syms[3];
    if (program.options.stropping == UPPER_STROPPING) {
      exp_syms[0] = EXPONENT_CHAR;
      exp_syms[1] = (char) TO_UPPER (EXPONENT_CHAR);
      exp_syms[2] = NULL_CHAR;
    } else {
      exp_syms[0] = (char) TO_UPPER (EXPONENT_CHAR);
      exp_syms[1] = ESCAPE_CHAR;
      exp_syms[2] = NULL_CHAR;
    }
    *ch = next_char (ref_l, ref_s, A68_TRUE);
    if (strchr (exp_syms, *ch) != NULL) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("+-0123456789", *ch) != NULL);
    } else {
      ret = (BOOL_T) (strchr ("0123456789", *ch) != NULL);
    }
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/*!
\brief get next token from internal copy of source file.
\param in_format are we scanning a format text
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\param start_l line where token starts
\param start_c character where token starts
\param att attribute designated to token
**/

static void get_next_token (BOOL_T in_format, SOURCE_LINE_T ** ref_l, char **ref_s, SOURCE_LINE_T ** start_l, char **start_c, int *att)
{
  char c = **ref_s, *sym = scan_buf;
  sym[0] = NULL_CHAR;
  get_good_char (&c, ref_l, ref_s);
  *start_l = *ref_l;
  *start_c = *ref_s;
  if (c == STOP_CHAR) {
/* We are at EOF */
    (sym++)[0] = STOP_CHAR;
    sym[0] = NULL_CHAR;
    return;
  }
/*------------+
| In a format |
+------------*/
  if (in_format) {
    char *format_items;
    if (program.options.stropping == UPPER_STROPPING) {
      format_items = "/%\\+-.abcdefghijklmnopqrstuvwxyz";
    } else {                    /* if (program.options.stropping == QUOTE_STROPPING) */
      format_items = "/%\\+-.ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }
    if (a68g_strchr (format_items, c) != NULL) {
/* General format items */
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      *att = get_format_item (c);
      (void) next_char (ref_l, ref_s, A68_FALSE);
      return;
    }
    if (IS_DIGIT (c)) {
/* INT denotation for static replicator */
      SCAN_DIGITS (c);
      sym[0] = NULL_CHAR;
      *att = STATIC_REPLICATOR;
      return;
    }
  }
/*----------------+
| Not in a format |
+----------------*/
  if (IS_UPPER (c)) {
    if (program.options.stropping == UPPER_STROPPING) {
/* Upper case word - bold tag */
      while (IS_UPPER (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
      sym[0] = NULL_CHAR;
      *att = BOLD_TAG;
    } else if (program.options.stropping == QUOTE_STROPPING) {
      while (IS_UPPER (c) || IS_DIGIT (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_TRUE);
      }
      sym[0] = NULL_CHAR;
      *att = IDENTIFIER;
    }
  } else if (c == '\'') {
/* Quote, uppercase word, quote - bold tag */
    int k = 0;
    c = next_char (ref_l, ref_s, A68_FALSE);
    while (IS_UPPER (c) || IS_DIGIT (c) || c == '_') {
      (sym++)[0] = c;
      k++;
      c = next_char (ref_l, ref_s, A68_TRUE);
    }
    SCAN_ERROR (k == 0, *start_l, *start_c, ERROR_QUOTED_BOLD_TAG);
    sym[0] = NULL_CHAR;
    *att = BOLD_TAG;
/* Skip terminating quote, or complain if it is not there */
    SCAN_ERROR (c != '\'', *start_l, *start_c, ERROR_QUOTED_BOLD_TAG);
    c = next_char (ref_l, ref_s, A68_FALSE);
  } else if (IS_LOWER (c)) {
/* Lower case word - identifier */
    while (IS_LOWER (c) || IS_DIGIT (c) || c == '_') {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_TRUE);
    }
    sym[0] = NULL_CHAR;
    *att = IDENTIFIER;
  } else if (c == POINT_CHAR) {
/* Begins with a point symbol - point, dotdot, L REAL denotation */
    if (whether_decimal_point (ref_l, ref_s, &c)) {
      (sym++)[0] = '0';
      (sym++)[0] = POINT_CHAR;
      c = next_char (ref_l, ref_s, A68_TRUE);
      SCAN_DIGITS (c);
      if (whether_exp_char (ref_l, ref_s, &c)) {
        SCAN_EXPONENT_PART (c);
      }
      sym[0] = NULL_CHAR;
      *att = REAL_DENOTATION;
    } else {
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (c == POINT_CHAR) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = POINT_CHAR;
        sym[0] = NULL_CHAR;
        *att = DOTDOT_SYMBOL;
        c = next_char (ref_l, ref_s, A68_FALSE);
      } else {
        (sym++)[0] = POINT_CHAR;
        sym[0] = NULL_CHAR;
        *att = POINT_SYMBOL;
      }
    }
  } else if (IS_DIGIT (c)) {
/* Something that begins with a digit - L INT denotation, L REAL denotation */
    SCAN_DIGITS (c);
    if (whether_decimal_point (ref_l, ref_s, &c)) {
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (whether_exp_char (ref_l, ref_s, &c)) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = '0';
        SCAN_EXPONENT_PART (c);
        *att = REAL_DENOTATION;
      } else {
        (sym++)[0] = POINT_CHAR;
        SCAN_DIGITS (c);
        if (whether_exp_char (ref_l, ref_s, &c)) {
          SCAN_EXPONENT_PART (c);
        }
        *att = REAL_DENOTATION;
      }
    } else if (whether_exp_char (ref_l, ref_s, &c)) {
      SCAN_EXPONENT_PART (c);
      *att = REAL_DENOTATION;
    } else if (whether_radix_char (ref_l, ref_s, &c)) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (program.options.stropping == UPPER_STROPPING) {
        while (IS_DIGIT (c) || strchr ("abcdef", c) != NULL) {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_TRUE);
        }
      } else {
        while (IS_DIGIT (c) || strchr ("ABCDEF", c) != NULL) {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_TRUE);
        }
      }
      *att = BITS_DENOTATION;
    } else {
      *att = INT_DENOTATION;
    }
    sym[0] = NULL_CHAR;
  } else if (c == QUOTE_CHAR) {
/* STRING denotation */
    BOOL_T stop = A68_FALSE;
    while (!stop) {
      c = next_char (ref_l, ref_s, A68_FALSE);
      while (c != QUOTE_CHAR && c != STOP_CHAR) {
        SCAN_ERROR (EOL (c), *start_l, *start_c, ERROR_LONG_STRING);
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
      SCAN_ERROR (*ref_l == NULL, *start_l, *start_c, ERROR_UNTERMINATED_STRING);
      c = next_char (ref_l, ref_s, A68_FALSE);
      if (c == QUOTE_CHAR) {
        (sym++)[0] = QUOTE_CHAR;
      } else {
        stop = A68_TRUE;
      }
    }
    sym[0] = NULL_CHAR;
    *att = in_format ? LITERAL : ROW_CHAR_DENOTATION;
  } else if (a68g_strchr ("#$()[]{},;@", c) != NULL) {
/* Single character symbols */
    (sym++)[0] = c;
    (void) next_char (ref_l, ref_s, A68_FALSE);
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '|') {
/* Bar */
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == ':') {
      (sym++)[0] = c;
      (void) next_char (ref_l, ref_s, A68_FALSE);
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '!' && program.options.stropping == QUOTE_STROPPING) {
/* Bar, will be replaced with modern variant.
   For this reason ! is not a MONAD with quote-stropping */
    (sym++)[0] = '|';
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == ':') {
      (sym++)[0] = c;
      (void) next_char (ref_l, ref_s, A68_FALSE);
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == ':') {
/* Colon, semicolon, IS, ISNT */
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == '=') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == ':') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
    } else if (c == '/') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == '=') {
        (sym++)[0] = c;
        if ((c = next_char (ref_l, ref_s, A68_FALSE)) == ':') {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == '=') {
        (sym++)[0] = c;
      }
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '=') {
/* Operator starting with "=" */
    char *scanned = sym;
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (a68g_strchr (NOMADS, c) != NULL) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_FALSE);
    }
    if (c == '=') {
      (sym++)[0] = c;
      if (next_char (ref_l, ref_s, A68_FALSE) == ':') {
        (sym++)[0] = ':';
        c = next_char (ref_l, ref_s, A68_FALSE);
        if (strlen (sym) < 4 && c == '=') {
          (sym++)[0] = '=';
          (void) next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      if (next_char (ref_l, ref_s, A68_FALSE) == '=') {
        (sym++)[0] = '=';
        (void) next_char (ref_l, ref_s, A68_FALSE);
      } else {
        SCAN_ERROR (!(strcmp (scanned, "=:") == 0 || strcmp (scanned, "==:") == 0), *start_l, *start_c, ERROR_INVALID_OPERATOR_TAG);
      }
    }
    sym[0] = NULL_CHAR;
    if (strcmp (scanned, "=") == 0) {
      *att = EQUALS_SYMBOL;
    } else {
      *att = OPERATOR;
    }
  } else if (a68g_strchr (MONADS, c) != NULL || a68g_strchr (NOMADS, c) != NULL) {
/* Operator */
    char *scanned = sym;
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (a68g_strchr (NOMADS, c) != NULL) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_FALSE);
    }
    if (c == '=') {
      (sym++)[0] = c;
      if (next_char (ref_l, ref_s, A68_FALSE) == ':') {
        (sym++)[0] = ':';
        c = next_char (ref_l, ref_s, A68_FALSE);
        if (strlen (scanned) < 4 && c == '=') {
          (sym++)[0] = '=';
          (void) next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      if (next_char (ref_l, ref_s, A68_FALSE) == '=') {
        (sym++)[0] = '=';
        sym[0] = NULL_CHAR;
        (void) next_char (ref_l, ref_s, A68_FALSE);
      } else {
        SCAN_ERROR (strcmp (&(scanned[1]), "=:") != 0, *start_l, *start_c, ERROR_INVALID_OPERATOR_TAG);
      }
    }
    sym[0] = NULL_CHAR;
    *att = OPERATOR;
  } else {
/* Afuuus ... strange characters! */
    unworthy (*start_l, *start_c, (int) c);
  }
}

/*!
\brief whether att opens an embedded clause
\param att attribute under test
\return whether att opens an embedded clause
**/

static BOOL_T open_embedded_clause (int att)
{
  switch (att) {
  case OPEN_SYMBOL:
  case BEGIN_SYMBOL:
  case PAR_SYMBOL:
  case IF_SYMBOL:
  case CASE_SYMBOL:
  case FOR_SYMBOL:
  case FROM_SYMBOL:
  case BY_SYMBOL:
  case TO_SYMBOL:
  case DOWNTO_SYMBOL:
  case WHILE_SYMBOL:
  case DO_SYMBOL:
  case SUB_SYMBOL:
  case ACCO_SYMBOL:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether att closes an embedded clause
\param att attribute under test
\return whether att closes an embedded clause
**/

static BOOL_T close_embedded_clause (int att)
{
  switch (att) {
  case CLOSE_SYMBOL:
  case END_SYMBOL:
  case FI_SYMBOL:
  case ESAC_SYMBOL:
  case OD_SYMBOL:
  case BUS_SYMBOL:
  case OCCA_SYMBOL:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief cast a string to lower case
\param p string to cast
**/

static void make_lower_case (char *p)
{
  for (; p != NULL && p[0] != NULL_CHAR; p++) {
    p[0] = (char) TO_LOWER (p[0]);
  }
}

/*!
\brief construct a linear list of tokens
\param root node where to insert new symbol
\param level current recursive descent depth
\param in_format whether we scan a format
\param l current source line
\param s current character in source line
\param start_l source line where symbol starts
\param start_c character where symbol starts
**/

static void tokenise_source (NODE_T ** root, int level, BOOL_T in_format, SOURCE_LINE_T ** l, char **s, SOURCE_LINE_T ** start_l, char **start_c)
{
  while (l != NULL && !stop_scanner) {
    int att = 0;
    get_next_token (in_format, l, s, start_l, start_c, &att);
    if (scan_buf[0] == STOP_CHAR) {
      stop_scanner = A68_TRUE;
    } else if (strlen (scan_buf) > 0 || att == ROW_CHAR_DENOTATION || att == LITERAL) {
      KEYWORD_T *kw = find_keyword (top_keyword, scan_buf);
      char *c = NULL;
      BOOL_T make_node = A68_TRUE;
      char *trailing = NULL;
      if (!(kw != NULL && att != ROW_CHAR_DENOTATION)) {
        if (att == IDENTIFIER) {
          make_lower_case (scan_buf);
        }
        if (att != ROW_CHAR_DENOTATION && att != LITERAL) {
          int len = (int) strlen (scan_buf);
          while (len >= 1 && scan_buf[len - 1] == '_') {
            trailing = "_";
            scan_buf[len - 1] = NULL_CHAR;
            len--;
          }
        }
        c = TEXT (add_token (&top_token, scan_buf));
      } else {
        if (WHETHER (kw, TO_SYMBOL)) {
/* Merge GO and TO to GOTO */
          if (*root != NULL && WHETHER (*root, GO_SYMBOL)) {
            ATTRIBUTE (*root) = GOTO_SYMBOL;
            SYMBOL (*root) = TEXT (find_keyword (top_keyword, "GOTO"));
            make_node = A68_FALSE;
          } else {
            att = ATTRIBUTE (kw);
            c = TEXT (kw);
          }
        } else {
          if (att == 0 || att == BOLD_TAG) {
            att = ATTRIBUTE (kw);
          }
          c = TEXT (kw);
/* Handle pragments */
          if (att == STYLE_II_COMMENT_SYMBOL || att == STYLE_I_COMMENT_SYMBOL || att == BOLD_COMMENT_SYMBOL) {
            pragment (ATTRIBUTE (kw), l, s);
            make_node = A68_FALSE;
          } else if (att == STYLE_I_PRAGMAT_SYMBOL || att == BOLD_PRAGMAT_SYMBOL) {
            pragment (ATTRIBUTE (kw), l, s);
            if (!stop_scanner) {
              isolate_options (scan_buf, *start_l);
              (void) set_options (program.options.list, A68_FALSE);
              make_node = A68_FALSE;
            }
          }
        }
      }
/* Add token to the tree */
      if (make_node) {
        NODE_T *q = new_node ();
        INFO (q) = new_node_info ();
        switch (att) {
          case ASSIGN_SYMBOL:
          case END_SYMBOL:
          case ESAC_SYMBOL:
          case OD_SYMBOL:
          case OF_SYMBOL:
          case FI_SYMBOL:
          case CLOSE_SYMBOL:
          case BUS_SYMBOL:
          case COLON_SYMBOL:
          case COMMA_SYMBOL:
          case DOTDOT_SYMBOL:
          case SEMI_SYMBOL: 
          {
            GENIE (q) = NULL;
            break;
          }
          default: {
            GENIE (q) = new_genie_info ();
            break;
          }
        }
        STATUS (q) = program.options.nodemask;
        LINE (q) = *start_l;
        INFO (q)->char_in_line = *start_c;
        PRIO (INFO (q)) = 0;
        INFO (q)->PROCEDURE_LEVEL = 0;
        ATTRIBUTE (q) = att;
        SYMBOL (q) = c;
        if (program.options.reductions) {
          WRITELN (STDOUT_FILENO, "\"");
          WRITE (STDOUT_FILENO, c);
          WRITE (STDOUT_FILENO, "\"");
        }
        PREVIOUS (q) = *root;
        SUB (q) = NEXT (q) = NULL;
        SYMBOL_TABLE (q) = NULL;
        MOID (q) = NULL;
        TAX (q) = NULL;
        if (*root != NULL) {
          NEXT (*root) = q;
        }
        if (program.top_node == NULL) {
          program.top_node = q;
        }
        *root = q;
        if (trailing != NULL) {
          diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, q, WARNING_TRAILING, trailing, att);
        }
      }
/* 
Redirection in tokenising formats. The scanner is a recursive-descent type as
to know when it scans a format text and when not. 
*/
      if (in_format && att == FORMAT_DELIMITER_SYMBOL) {
        return;
      } else if (!in_format && att == FORMAT_DELIMITER_SYMBOL) {
        tokenise_source (root, level + 1, A68_TRUE, l, s, start_l, start_c);
      } else if (in_format && open_embedded_clause (att)) {
        NODE_T *z = PREVIOUS (*root);
        if (z != NULL && (WHETHER (z, FORMAT_ITEM_N) || WHETHER (z, FORMAT_ITEM_G) || WHETHER (z, FORMAT_ITEM_H) || WHETHER (z, FORMAT_ITEM_F))) {
          tokenise_source (root, level, A68_FALSE, l, s, start_l, start_c);
        } else if (att == OPEN_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (program.options.brackets && att == SUB_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (program.options.brackets && att == ACCO_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        }
      } else if (!in_format && level > 0 && open_embedded_clause (att)) {
        tokenise_source (root, level + 1, A68_FALSE, l, s, start_l, start_c);
      } else if (!in_format && level > 0 && close_embedded_clause (att)) {
        return;
      } else if (in_format && att == CLOSE_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (program.options.brackets && in_format && att == BUS_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (program.options.brackets && in_format && att == OCCA_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      }
    }
  }
}

/*!
\brief tokenise source file, build initial syntax tree
\return whether tokenising ended satisfactorily
**/

BOOL_T lexical_analyser (void)
{
  SOURCE_LINE_T *l, *start_l = NULL;
  char *s = NULL, *start_c = NULL;
  NODE_T *root = NULL;
  scan_buf = NULL;
  max_scan_buf_length = source_file_size = get_source_size ();
/* Errors in file? */
  if (max_scan_buf_length == 0) {
    return (A68_FALSE);
  }
  if (program.options.run_script) {
    scan_buf = (char *) get_temp_heap_space ((unsigned) (8 + max_scan_buf_length));
    if (!read_script_file ()) {
      return (A68_FALSE);
    }
  } else {
    max_scan_buf_length += (int) strlen (bold_prelude_start) + (int) strlen (bold_postlude);
    max_scan_buf_length += (int) strlen (quote_prelude_start) + (int) strlen (quote_postlude);
/* Allocate a scan buffer with 8 bytes extra space */
    scan_buf = (char *) get_temp_heap_space ((unsigned) (8 + max_scan_buf_length));
/* Errors in file? */
    if (!read_source_file ()) {
      return (A68_FALSE);
    }
  }
/* Start tokenising */
  read_error = A68_FALSE;
  stop_scanner = A68_FALSE;
  if ((l = program.top_line) != NULL) {
    s = l->string;
  }
  tokenise_source (&root, 0, A68_FALSE, &l, &s, &start_l, &start_c);
  return (A68_TRUE);
}

/* This is a small refinement preprocessor for Algol68G */

/*!
\brief whether refinement terminator
\param p position in syntax tree
\return same
**/

static BOOL_T whether_refinement_terminator (NODE_T * p)
{
  if (WHETHER (p, POINT_SYMBOL)) {
    if (IN_PRELUDE (NEXT (p))) {
      return (A68_TRUE);
    } else {
      return (whether (p, POINT_SYMBOL, IDENTIFIER, COLON_SYMBOL, 0));
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief get refinement definitions in the internal source
\param z module that reads source
**/

void get_refinements (void)
{
  NODE_T *p = program.top_node;
  program.top_refinement = NULL;
/* First look where the prelude ends */
  while (p != NULL && IN_PRELUDE (p)) {
    FORWARD (p);
  }
/* Determine whether the program contains refinements at all */
  while (p != NULL && !IN_PRELUDE (p) && !whether_refinement_terminator (p)) {
    FORWARD (p);
  }
  if (p == NULL || IN_PRELUDE (p)) {
    return;
  }
/* Apparently this is code with refinements */
  FORWARD (p);
  if (p == NULL || IN_PRELUDE (p)) {
/* Ok, we accept a program with no refinements as well */
    return;
  }
  while (p != NULL && !IN_PRELUDE (p) && whether (p, IDENTIFIER, COLON_SYMBOL, 0)) {
    REFINEMENT_T *new_one = (REFINEMENT_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (REFINEMENT_T)), *x;
    BOOL_T exists;
    NEXT (new_one) = NULL;
    new_one->name = SYMBOL (p);
    new_one->applications = 0;
    new_one->line_defined = LINE (p);
    new_one->line_applied = NULL;
    new_one->node_defined = p;
    new_one->begin = NULL;
    new_one->end = NULL;
    p = NEXT_NEXT (p);
    if (p == NULL) {
      diagnostic_node (A68_SYNTAX_ERROR, NULL, ERROR_REFINEMENT_EMPTY, NULL);
      return;
    } else {
      new_one->begin = p;
    }
    while (p != NULL && ATTRIBUTE (p) != POINT_SYMBOL) {
      new_one->end = p;
      FORWARD (p);
    }
    if (p == NULL) {
      diagnostic_node (A68_SYNTAX_ERROR, NULL, ERROR_SYNTAX_EXPECTED, POINT_SYMBOL, NULL);
      return;
    } else {
      FORWARD (p);
    }
/* Do we already have one by this name */
    x = program.top_refinement;
    exists = A68_FALSE;
    while (x != NULL && !exists) {
      if (x->name == new_one->name) {
        diagnostic_node (A68_SYNTAX_ERROR, new_one->node_defined, ERROR_REFINEMENT_DEFINED, NULL);
        exists = A68_TRUE;
      }
      FORWARD (x);
    }
/* Straight insertion in chain */
    if (!exists) {
      NEXT (new_one) = program.top_refinement;
      program.top_refinement = new_one;
    }
  }
  if (p != NULL && !IN_PRELUDE (p)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_REFINEMENT_INVALID, NULL);
  }
}

/*!
\brief put refinement applications in the internal source
\param z module that reads source
**/

void put_refinements (void)
{
  REFINEMENT_T *x;
  NODE_T *p, *point;
/* If there are no refinements, there's little to do */
  if (program.top_refinement == NULL) {
    return;
  }
/* Initialisation */
  x = program.top_refinement;
  while (x != NULL) {
    x->applications = 0;
    FORWARD (x);
  }
/* Before we introduce infinite loops, find where closing-prelude starts */
  p = program.top_node;
  while (p != NULL && IN_PRELUDE (p)) {
    FORWARD (p);
  }
  while (p != NULL && !IN_PRELUDE (p)) {
    FORWARD (p);
  }
  ABEND (p == NULL, ERROR_INTERNAL_CONSISTENCY, NULL);
  point = p;
/* We need to substitute until the first point */
  p = program.top_node;
  while (p != NULL && ATTRIBUTE (p) != POINT_SYMBOL) {
    if (WHETHER (p, IDENTIFIER)) {
/* See if we can find its definition */
      REFINEMENT_T *y = NULL;
      x = program.top_refinement;
      while (x != NULL && y == NULL) {
        if (x->name == SYMBOL (p)) {
          y = x;
        } else {
          FORWARD (x);
        }
      }
      if (y != NULL) {
/* We found its definition */
        y->applications++;
        if (y->applications > 1) {
          diagnostic_node (A68_SYNTAX_ERROR, y->node_defined, ERROR_REFINEMENT_APPLIED, NULL);
          FORWARD (p);
        } else {
/* Tie the definition in the tree */
          y->line_applied = LINE (p);
          if (PREVIOUS (p) != NULL) {
            NEXT (PREVIOUS (p)) = y->begin;
          }
          if (y->begin != NULL) {
            PREVIOUS (y->begin) = PREVIOUS (p);
          }
          if (NEXT (p) != NULL) {
            PREVIOUS (NEXT (p)) = y->end;
          }
          if (y->end != NULL) {
            NEXT (y->end) = NEXT (p);
          }
          p = y->begin;         /* So we can substitute the refinements within */
        }
      } else {
        FORWARD (p);
      }
    } else {
      FORWARD (p);
    }
  }
/* After the point we ignore it all until the prelude */
  if (p != NULL && WHETHER (p, POINT_SYMBOL)) {
    if (PREVIOUS (p) != NULL) {
      NEXT (PREVIOUS (p)) = point;
    }
    if (PREVIOUS (point) != NULL) {
      PREVIOUS (point) = PREVIOUS (p);
    }
  } else {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX_EXPECTED, POINT_SYMBOL, NULL);
  }
/* Has the programmer done it well? */
  if (program.error_count == 0) {
    x = program.top_refinement;
    while (x != NULL) {
      if (x->applications == 0) {
        diagnostic_node (A68_SYNTAX_ERROR, x->node_defined, ERROR_REFINEMENT_NOT_APPLIED, NULL);
      }
      FORWARD (x);
    }
  }
}

/* Parser starts here */

/*!
\brief insert node
\param p node after which to insert
\param att attribute for new node
**/

static void insert_node (NODE_T * p, int att)
{
  NODE_T *q = new_node ();
  *q = *p;
  if (GENIE (p) != NULL) {
    GENIE (q) = new_genie_info ();
  }
  ATTRIBUTE (q) = att;
  NEXT (p) = q;
  PREVIOUS (q) = p;
  if (NEXT (q) != NULL) {
    PREVIOUS (NEXT (q)) = q;
  }
}

/*!
\brief substitute brackets
\param p position in tree
**/

void substitute_brackets (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    substitute_brackets (SUB (p));
    switch (ATTRIBUTE (p)) {
    case ACCO_SYMBOL:
      {
        ATTRIBUTE (p) = OPEN_SYMBOL;
        break;
      }
    case OCCA_SYMBOL:
      {
        ATTRIBUTE (p) = CLOSE_SYMBOL;
        break;
      }
    case SUB_SYMBOL:
      {
        ATTRIBUTE (p) = OPEN_SYMBOL;
        break;
      }
    case BUS_SYMBOL:
      {
        ATTRIBUTE (p) = CLOSE_SYMBOL;
        break;
      }
    }
  }
}

/*!
\brief whether token terminates a unit
\param p position in tree
\return TRUE or FALSE whether token terminates a unit
**/

static int whether_unit_terminator (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case BUS_SYMBOL:
  case CLOSE_SYMBOL:
  case END_SYMBOL:
  case SEMI_SYMBOL:
  case EXIT_SYMBOL:
  case COMMA_SYMBOL:
  case THEN_BAR_SYMBOL:
  case ELSE_BAR_SYMBOL:
  case THEN_SYMBOL:
  case ELIF_SYMBOL:
  case ELSE_SYMBOL:
  case FI_SYMBOL:
  case IN_SYMBOL:
  case OUT_SYMBOL:
  case OUSE_SYMBOL:
  case ESAC_SYMBOL:
  case EDOC_SYMBOL:
  case OCCA_SYMBOL:
    {
      return (ATTRIBUTE (p));
    }
  default:
    {
      return (NULL_ATTRIBUTE);
    }
  }
}

/*!
\brief whether token is a unit-terminator in a loop clause
\param p position in tree
\return whether token is a unit-terminator in a loop clause
**/

static BOOL_T whether_loop_keyword (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case FOR_SYMBOL:
  case FROM_SYMBOL:
  case BY_SYMBOL:
  case TO_SYMBOL:
  case DOWNTO_SYMBOL:
  case WHILE_SYMBOL:
  case DO_SYMBOL:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether token cannot follow semicolon or EXIT
\param p position in tree
\return whether token cannot follow semicolon or EXIT
**/

static int whether_semicolon_less (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case BUS_SYMBOL:
  case CLOSE_SYMBOL:
  case END_SYMBOL:
  case SEMI_SYMBOL:
  case EXIT_SYMBOL:
  case THEN_BAR_SYMBOL:
  case ELSE_BAR_SYMBOL:
  case THEN_SYMBOL:
  case ELIF_SYMBOL:
  case ELSE_SYMBOL:
  case FI_SYMBOL:
  case IN_SYMBOL:
  case OUT_SYMBOL:
  case OUSE_SYMBOL:
  case ESAC_SYMBOL:
  case EDOC_SYMBOL:
  case OCCA_SYMBOL:
  case OD_SYMBOL:
  case UNTIL_SYMBOL:
    {
      return (ATTRIBUTE (p));
    }
  default:
    {
      return (NULL_ATTRIBUTE);
    }
  }
}

/*!
\brief get good attribute
\param p position in tree
\return same
**/

static int get_good_attribute (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case UNIT:
  case TERTIARY:
  case SECONDARY:
  case PRIMARY:
    {
      return (get_good_attribute (SUB (p)));
    }
  default:
    {
      return (ATTRIBUTE (p));
    }
  }
}

/*!
\brief preferably don't put intelligible diagnostic here
\param p position in tree
\return same
**/

static BOOL_T dont_mark_here (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ACCO_SYMBOL:
  case ALT_DO_SYMBOL:
  case ALT_EQUALS_SYMBOL:
  case ANDF_SYMBOL:
  case ASSERT_SYMBOL:
  case ASSIGN_SYMBOL:
  case ASSIGN_TO_SYMBOL:
  case AT_SYMBOL:
  case BEGIN_SYMBOL:
  case BITS_SYMBOL:
  case BOLD_COMMENT_SYMBOL:
  case BOLD_PRAGMAT_SYMBOL:
  case BOOL_SYMBOL:
  case BUS_SYMBOL:
  case BY_SYMBOL:
  case BYTES_SYMBOL:
  case CASE_SYMBOL:
  case CHANNEL_SYMBOL:
  case CHAR_SYMBOL:
  case CLOSE_SYMBOL:
  case CODE_SYMBOL:
  case COLON_SYMBOL:
  case COLUMN_SYMBOL:
  case COMMA_SYMBOL:
  case COMPLEX_SYMBOL:
  case COMPL_SYMBOL:
  case DIAGONAL_SYMBOL:
  case DO_SYMBOL:
  case DOTDOT_SYMBOL:
  case DOWNTO_SYMBOL:
  case EDOC_SYMBOL:
  case ELIF_SYMBOL:
  case ELSE_BAR_SYMBOL:
  case ELSE_SYMBOL:
  case EMPTY_SYMBOL:
  case END_SYMBOL:
  case ENVIRON_SYMBOL:
  case EQUALS_SYMBOL:
  case ESAC_SYMBOL:
  case EXIT_SYMBOL:
  case FALSE_SYMBOL:
  case FILE_SYMBOL:
  case FI_SYMBOL:
  case FLEX_SYMBOL:
  case FORMAT_DELIMITER_SYMBOL:
  case FORMAT_SYMBOL:
  case FOR_SYMBOL:
  case FROM_SYMBOL:
  case GO_SYMBOL:
  case GOTO_SYMBOL:
  case HEAP_SYMBOL:
  case IF_SYMBOL:
  case IN_SYMBOL:
  case INT_SYMBOL:
  case ISNT_SYMBOL:
  case IS_SYMBOL:
  case LOC_SYMBOL:
  case LONG_SYMBOL:
  case MAIN_SYMBOL:
  case MODE_SYMBOL:
  case NIL_SYMBOL:
  case OCCA_SYMBOL:
  case OD_SYMBOL:
  case OF_SYMBOL:
  case OPEN_SYMBOL:
  case OP_SYMBOL:
  case ORF_SYMBOL:
  case OUSE_SYMBOL:
  case OUT_SYMBOL:
  case PAR_SYMBOL:
  case PIPE_SYMBOL:
  case POINT_SYMBOL:
  case PRIO_SYMBOL:
  case PROC_SYMBOL:
  case REAL_SYMBOL:
  case REF_SYMBOL:
  case ROW_ASSIGN_SYMBOL:
  case ROWS_SYMBOL:
  case ROW_SYMBOL:
  case SEMA_SYMBOL:
  case SEMI_SYMBOL:
  case SHORT_SYMBOL:
  case SKIP_SYMBOL:
  case SOUND_SYMBOL:
  case STRING_SYMBOL:
  case STRUCT_SYMBOL:
  case STYLE_I_COMMENT_SYMBOL:
  case STYLE_II_COMMENT_SYMBOL:
  case STYLE_I_PRAGMAT_SYMBOL:
  case SUB_SYMBOL:
  case THEN_BAR_SYMBOL:
  case THEN_SYMBOL:
  case TO_SYMBOL:
  case TRANSPOSE_SYMBOL:
  case TRUE_SYMBOL:
  case UNION_SYMBOL:
  case UNTIL_SYMBOL:
  case VOID_SYMBOL:
  case WHILE_SYMBOL:
/* and than */
  case SERIAL_CLAUSE:
  case ENQUIRY_CLAUSE:
  case INITIALISER_SERIES:
  case DECLARATION_LIST:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief intelligible diagnostic from syntax tree branch
\param p position in tree
\param q
\return same
**/

char *phrase_to_text (NODE_T * p, NODE_T ** w)
{
#define MAX_TERMINALS 8
  int count = 0, line = -1;
  static char buffer[BUFFER_SIZE];
  for (buffer[0] = NULL_CHAR; p != NULL && count < MAX_TERMINALS; FORWARD (p)) {
    if (LINE_NUMBER (p) > 0) {
      int gatt = get_good_attribute (p);
      char *z = non_terminal_string (input_line, gatt);
/* 
Where to put the error message? Bob Uzgalis noted that actual content of a 
diagnostic is not as important as accurately indicating *were* the problem is! 
*/
      if (w != NULL) {
        if (count == 0 || (*w) == NULL) {
          *w = p;
        } else if (dont_mark_here (*w)) {
          *w = p;
        }
      }
/* Add initiation */
      if (count == 0) {
        if (w != NULL) {
          bufcat (buffer, "construct beginning with", BUFFER_SIZE);
        }
      } else if (count == 1) {
        bufcat (buffer, " followed by", BUFFER_SIZE);
      } else if (count == 2) {
        bufcat (buffer, " and then", BUFFER_SIZE);
      } else if (count >= 3) {
        bufcat (buffer, ",", BUFFER_SIZE);
      }
/* Attribute or symbol */
      if (z != NULL && SUB (p) != NULL) {
        if (gatt == IDENTIFIER || gatt == OPERATOR || gatt == DENOTATION) {
          ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " \"%s\"", SYMBOL (p)) >= 0);
          bufcat (buffer, edit_line, BUFFER_SIZE);
        } else {
          if (strchr ("aeio", z[0]) != NULL) {
            bufcat (buffer, " an", BUFFER_SIZE);
          } else {
            bufcat (buffer, " a", BUFFER_SIZE);
          }
          ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " %s", z) >= 0);
          bufcat (buffer, edit_line, BUFFER_SIZE);
        }
      } else if (z != NULL && SUB (p) == NULL) {
        ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " \"%s\"", SYMBOL (p)) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      } else if (SYMBOL (p) != NULL) {
        ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " \"%s\"", SYMBOL (p)) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      }
/* Add "starting in line nn" */
      if (z != NULL && line != LINE_NUMBER (p)) {
        line = LINE_NUMBER (p);
        if (gatt == SERIAL_CLAUSE || gatt == ENQUIRY_CLAUSE || gatt == INITIALISER_SERIES) {
          bufcat (buffer, " starting", BUFFER_SIZE);
        }
        ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " in line %d", line) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      }
      count++;
    }
  }
  if (p != NULL && count == MAX_TERMINALS) {
    bufcat (buffer, " etcetera", BUFFER_SIZE);
  }
  return (buffer);
}

/*
This is a parenthesis checker. After this checker, we know that at least
brackets are matched. This stabilises later parser phases.
Top-down parsing is done to place error diagnostics near offending lines.
*/

static char bracket_check_error_text[BUFFER_SIZE];

/*!
\brief intelligible diagnostics for the bracket checker
\param txt buffer to which to append text
\param n count mismatch (~= 0)
\param bra opening bracket
\param ket expected closing bracket
\return same
**/

static void bracket_check_error (char *txt, int n, char *bra, char *ket)
{
  if (n != 0) {
    char b[BUFFER_SIZE];
    ASSERT (snprintf (b, SNPRINTF_SIZE, "\"%s\" without matching \"%s\"", (n > 0 ? bra : ket), (n > 0 ? ket : bra)) >= 0);
    if (strlen (txt) > 0) {
      bufcat (txt, " and ", BUFFER_SIZE);
    }
    bufcat (txt, b, BUFFER_SIZE);
  }
}

/*!
\brief diagnose brackets in local branch of the tree
\param p position in tree
\return error message
**/

static char *bracket_check_diagnose (NODE_T * p)
{
  int begins = 0, opens = 0, format_delims = 0, format_opens = 0, subs = 0, ifs = 0, cases = 0, dos = 0, accos = 0;
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case BEGIN_SYMBOL:
      {
        begins++;
        break;
      }
    case END_SYMBOL:
      {
        begins--;
        break;
      }
    case OPEN_SYMBOL:
      {
        opens++;
        break;
      }
    case CLOSE_SYMBOL:
      {
        opens--;
        break;
      }
    case ACCO_SYMBOL:
      {
        accos++;
        break;
      }
    case OCCA_SYMBOL:
      {
        accos--;
        break;
      }
    case FORMAT_DELIMITER_SYMBOL:
      {
        if (format_delims == 0) {
          format_delims = 1;
        } else {
          format_delims = 0;
        }
        break;
      }
    case FORMAT_OPEN_SYMBOL:
      {
        format_opens++;
        break;
      }
    case FORMAT_CLOSE_SYMBOL:
      {
        format_opens--;
        break;
      }
    case SUB_SYMBOL:
      {
        subs++;
        break;
      }
    case BUS_SYMBOL:
      {
        subs--;
        break;
      }
    case IF_SYMBOL:
      {
        ifs++;
        break;
      }
    case FI_SYMBOL:
      {
        ifs--;
        break;
      }
    case CASE_SYMBOL:
      {
        cases++;
        break;
      }
    case ESAC_SYMBOL:
      {
        cases--;
        break;
      }
    case DO_SYMBOL:
      {
        dos++;
        break;
      }
    case OD_SYMBOL:
      {
        dos--;
        break;
      }
    }
  }
  bracket_check_error_text[0] = NULL_CHAR;
  bracket_check_error (bracket_check_error_text, begins, "BEGIN", "END");
  bracket_check_error (bracket_check_error_text, opens, "(", ")");
  bracket_check_error (bracket_check_error_text, format_opens, "(", ")");
  bracket_check_error (bracket_check_error_text, format_delims, "$", "$");
  bracket_check_error (bracket_check_error_text, accos, "{", "}");
  bracket_check_error (bracket_check_error_text, subs, "[", "]");
  bracket_check_error (bracket_check_error_text, ifs, "IF", "FI");
  bracket_check_error (bracket_check_error_text, cases, "CASE", "ESAC");
  bracket_check_error (bracket_check_error_text, dos, "DO", "OD");
  return (bracket_check_error_text);
}

/*!
\brief driver for locally diagnosing non-matching tokens
\param top
\param p position in tree
\return token from where to continue
**/

static NODE_T *bracket_check_parse (NODE_T * top, NODE_T * p)
{
  BOOL_T ignore_token;
  for (; p != NULL; FORWARD (p)) {
    int ket = NULL_ATTRIBUTE;
    NODE_T *q = NULL;
    ignore_token = A68_FALSE;
    switch (ATTRIBUTE (p)) {
    case BEGIN_SYMBOL:
      {
        ket = END_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case OPEN_SYMBOL:
      {
        ket = CLOSE_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case ACCO_SYMBOL:
      {
        ket = OCCA_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case FORMAT_OPEN_SYMBOL:
      {
        ket = FORMAT_CLOSE_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case SUB_SYMBOL:
      {
        ket = BUS_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case IF_SYMBOL:
      {
        ket = FI_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case CASE_SYMBOL:
      {
        ket = ESAC_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case DO_SYMBOL:
      {
        ket = OD_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case END_SYMBOL:
    case OCCA_SYMBOL:
    case CLOSE_SYMBOL:
    case FORMAT_CLOSE_SYMBOL:
    case BUS_SYMBOL:
    case FI_SYMBOL:
    case ESAC_SYMBOL:
    case OD_SYMBOL:
      {
        return (p);
      }
    default:
      {
        ignore_token = A68_TRUE;
      }
    }
    if (ignore_token) {
      ;
    } else if (q != NULL && WHETHER (q, ket)) {
      p = q;
    } else if (q == NULL) {
      char *diag = bracket_check_diagnose (top);
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_PARENTHESIS, strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS);
      longjmp (top_down_crash_exit, 1);
    } else {
      char *diag = bracket_check_diagnose (top);
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_PARENTHESIS_2, ATTRIBUTE (q), LINE (q), ket, strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS);
      longjmp (top_down_crash_exit, 1);
    }
  }
  return (NULL);
}

/*!
\brief driver for globally diagnosing non-matching tokens
\param top top node in syntax tree
**/

void check_parenthesis (NODE_T * top)
{
  if (!setjmp (top_down_crash_exit)) {
    if (bracket_check_parse (top, top) != NULL) {
      diagnostic_node (A68_SYNTAX_ERROR, top, ERROR_PARENTHESIS, INFO_MISSING_KEYWORDS, NULL);
    }
  }
}

/*
Next is a top-down parser that branches out the basic blocks.
After this we can assign symbol tables to basic blocks.
*/

/*!
\brief give diagnose from top-down parser
\param start embedding clause starts here
\param posit error issued at this point
\param clause type of clause being processed
\param expected token expected
**/

static void top_down_diagnose (NODE_T * start, NODE_T * posit, int clause, int expected)
{
  NODE_T *issue = (posit != NULL ? posit : start);
  if (expected != 0) {
    diagnostic_node (A68_SYNTAX_ERROR, issue, ERROR_EXPECTED_NEAR, expected, clause, SYMBOL (start), start->info->line, NULL);
  } else {
    diagnostic_node (A68_SYNTAX_ERROR, issue, ERROR_UNBALANCED_KEYWORD, clause, SYMBOL (start), LINE (start), NULL);
  }
}

/*!
\brief check for premature exhaustion of tokens
\param p position in tree
\param q
**/

static void tokens_exhausted (NODE_T * p, NODE_T * q)
{
  if (p == NULL) {
    diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_KEYWORD);
    longjmp (top_down_crash_exit, 1);
  }
}

/* This part specifically branches out loop clauses */

/*!
\brief whether in cast or formula with loop clause
\param p position in tree
\return number of symbols to skip
**/

static int whether_loop_cast_formula (NODE_T * p)
{
/* Accept declarers that can appear in such casts but not much more */
  if (WHETHER (p, VOID_SYMBOL)) {
    return (1);
  } else if (WHETHER (p, INT_SYMBOL)) {
    return (1);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (1);
  } else if (whether_one_of (p, OPERATOR, BOLD_TAG, NULL_ATTRIBUTE)) {
    return (1);
  } else if (whether (p, UNION_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE)) {
    return (2);
  } else if (whether_one_of (p, OPEN_SYMBOL, SUB_SYMBOL, NULL_ATTRIBUTE)) {
    int k;
    for (k = 0; p != NULL && (whether_one_of (p, OPEN_SYMBOL, SUB_SYMBOL, NULL_ATTRIBUTE)); FORWARD (p), k++) {
      ;
    }
    return (p != NULL && whether (p, UNION_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE) ? k : 0);
  }
  return (0);
}

/*!
\brief skip a unit in a loop clause (FROM u BY u TO u)
\param p position in tree
\return token from where to proceed or NULL
**/

static NODE_T *top_down_skip_loop_unit (NODE_T * p)
{
/* Unit may start with, or consist of, a loop */
  if (whether_loop_keyword (p)) {
    p = top_down_loop (p);
  }
/* Skip rest of unit */
  while (p != NULL) {
    int k = whether_loop_cast_formula (p);
    if (k != 0) {
/* operator-cast series .. */
      while (p != NULL && k != 0) {
        while (k != 0) {
          FORWARD (p);
          k--;
        }
        k = whether_loop_cast_formula (p);
      }
/* ... may be followed by a loop clause */
      if (whether_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (whether_loop_keyword (p) || WHETHER (p, OD_SYMBOL)) {
/* new loop or end-of-loop */
      return (p);
    } else if (WHETHER (p, COLON_SYMBOL)) {
      FORWARD (p);
/* skip routine header: loop clause */
      if (p != NULL && whether_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (whether_one_of (p, SEMI_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE) || WHETHER (p, EXIT_SYMBOL)) {
/* Statement separators */
      return (p);
    } else {
      FORWARD (p);
    }
  }
  return (NULL);
}

/*!
\brief skip a loop clause
\param p position in tree
\return token from where to proceed or NULL
**/

static NODE_T *top_down_skip_loop_series (NODE_T * p)
{
  BOOL_T siga;
  do {
    p = top_down_skip_loop_unit (p);
    siga = (BOOL_T) (p != NULL && (whether_one_of (p, SEMI_SYMBOL, EXIT_SYMBOL, COMMA_SYMBOL, COLON_SYMBOL, NULL_ATTRIBUTE)));
    if (siga) {
      FORWARD (p);
    }
  } while (!(p == NULL || !siga));
  return (p);
}

/*!
\brief make branch of loop parts
\param p position in tree
\return token from where to proceed or NULL
**/

NODE_T *top_down_loop (NODE_T * p)
{
  NODE_T *start = p, *q = p, *save;
  if (WHETHER (q, FOR_SYMBOL)) {
    tokens_exhausted (FORWARD (q), start);
    if (WHETHER (q, IDENTIFIER)) {
      ATTRIBUTE (q) = DEFINING_IDENTIFIER;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, IDENTIFIER);
      longjmp (top_down_crash_exit, 1);
    }
    tokens_exhausted (FORWARD (q), start);
    if (whether_one_of (q, FROM_SYMBOL, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, NULL_ATTRIBUTE);
      longjmp (top_down_crash_exit, 1);
    }
  }
  if (WHETHER (q, FROM_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_unit (NEXT (q));
    tokens_exhausted (q, start);
    if (whether_one_of (q, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, NULL_ATTRIBUTE);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), FROM_SYMBOL);
  }
  if (WHETHER (q, BY_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (whether_one_of (q, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, NULL_ATTRIBUTE);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), BY_SYMBOL);
  }
  if (whether_one_of (q, TO_SYMBOL, DOWNTO_SYMBOL, NULL_ATTRIBUTE)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER (q, WHILE_SYMBOL)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, NULL_ATTRIBUTE);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), TO_SYMBOL);
  }
  if (WHETHER (q, WHILE_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, DO_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), WHILE_SYMBOL);
  }
  if (whether_one_of (q, DO_SYMBOL, ALT_DO_SYMBOL, NULL_ATTRIBUTE)) {
    int k = ATTRIBUTE (q);
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER_NOT (q, OD_SYMBOL)) {
      top_down_diagnose (start, q, LOOP_CLAUSE, OD_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, q, k);
  }
  save = NEXT (start);
  make_sub (p, start, LOOP_CLAUSE);
  return (save);
}

/*!
\brief driver for making branches of loop parts
\param p position in tree
**/

static void top_down_loops (NODE_T * p)
{
  NODE_T *q = p;
  for (; q != NULL; FORWARD (q)) {
    if (SUB (q) != NULL) {
      top_down_loops (SUB (q));
    }
  }
  q = p;
  while (q != NULL) {
    if (whether_loop_keyword (q) != NULL_ATTRIBUTE) {
      q = top_down_loop (q);
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief driver for making branches of until parts
\param p position in tree
**/

static void top_down_untils (NODE_T * p)
{
  NODE_T *q = p;
  for (; q != NULL; FORWARD (q)) {
    if (SUB (q) != NULL) {
      top_down_untils (SUB (q));
    }
  }
  q = p;
  while (q != NULL) {
    if (WHETHER (q, UNTIL_SYMBOL)) {
      NODE_T *u = q;
      while (NEXT (u) != NULL) {
        FORWARD (u);
      }
      make_sub (q, PREVIOUS (u), UNTIL_SYMBOL);
      return;
    } else {
      FORWARD (q);
    }
  }
}

/* Branch anything except parts of a loop */

/*!
\brief skip serial/enquiry clause (unit series)
\param p position in tree
\return token from where to proceed or NULL
**/

static NODE_T *top_down_series (NODE_T * p)
{
  BOOL_T siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    p = top_down_skip_unit (p);
    if (p != NULL) {
      if (whether_one_of (p, SEMI_SYMBOL, EXIT_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
        siga = A68_TRUE;
        FORWARD (p);
      }
    }
  }
  return (p);
}

/*!
\brief make branch of BEGIN .. END
\param begin_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_begin (NODE_T * begin_p)
{
  NODE_T *end_p = top_down_series (NEXT (begin_p));
  if (end_p == NULL || WHETHER_NOT (end_p, END_SYMBOL)) {
    top_down_diagnose (begin_p, end_p, ENCLOSED_CLAUSE, END_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  } else {
    make_sub (begin_p, end_p, BEGIN_SYMBOL);
    return (NEXT (begin_p));
  }
}

/*!
\brief make branch of CODE .. EDOC
\param code_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_code (NODE_T * code_p)
{
  NODE_T *edoc_p = top_down_series (NEXT (code_p));
  if (edoc_p == NULL || WHETHER_NOT (edoc_p, EDOC_SYMBOL)) {
    diagnostic_node (A68_SYNTAX_ERROR, code_p, ERROR_KEYWORD);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  } else {
    make_sub (code_p, edoc_p, CODE_SYMBOL);
    return (NEXT (code_p));
  }
}

/*!
\brief make branch of ( .. )
\param open_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_open (NODE_T * open_p)
{
  NODE_T *then_bar_p = top_down_series (NEXT (open_p)), *elif_bar_p;
  if (then_bar_p != NULL && WHETHER (then_bar_p, CLOSE_SYMBOL)) {
    make_sub (open_p, then_bar_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (then_bar_p == NULL || WHETHER_NOT (then_bar_p, THEN_BAR_SYMBOL)) {
    top_down_diagnose (open_p, then_bar_p, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (open_p, PREVIOUS (then_bar_p), OPEN_SYMBOL);
  elif_bar_p = top_down_series (NEXT (then_bar_p));
  if (elif_bar_p != NULL && WHETHER (elif_bar_p, CLOSE_SYMBOL)) {
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (open_p, elif_bar_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (elif_bar_p != NULL && WHETHER (elif_bar_p, THEN_BAR_SYMBOL)) {
    NODE_T *close_p = top_down_series (NEXT (elif_bar_p));
    if (close_p == NULL || WHETHER_NOT (close_p, CLOSE_SYMBOL)) {
      top_down_diagnose (open_p, elif_bar_p, ENCLOSED_CLAUSE, CLOSE_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (elif_bar_p, PREVIOUS (close_p), THEN_BAR_SYMBOL);
    make_sub (open_p, close_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (elif_bar_p != NULL && WHETHER (elif_bar_p, ELSE_BAR_SYMBOL)) {
    NODE_T *close_p = top_down_open (elif_bar_p);
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (open_p, elif_bar_p, OPEN_SYMBOL);
    return (close_p);
  } else {
    top_down_diagnose (open_p, elif_bar_p, ENCLOSED_CLAUSE, CLOSE_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief make branch of [ .. ]
\param sub_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_sub (NODE_T * sub_p)
{
  NODE_T *bus_p = top_down_series (NEXT (sub_p));
  if (bus_p != NULL && WHETHER (bus_p, BUS_SYMBOL)) {
    make_sub (sub_p, bus_p, SUB_SYMBOL);
    return (NEXT (sub_p));
  } else {
    top_down_diagnose (sub_p, bus_p, 0, BUS_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief make branch of { .. }
\param acco_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_acco (NODE_T * acco_p)
{
  NODE_T *occa_p = top_down_series (NEXT (acco_p));
  if (occa_p != NULL && WHETHER (occa_p, OCCA_SYMBOL)) {
    make_sub (acco_p, occa_p, ACCO_SYMBOL);
    return (NEXT (acco_p));
  } else {
    top_down_diagnose (acco_p, occa_p, ENCLOSED_CLAUSE, OCCA_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief make branch of IF .. THEN .. ELSE .. FI
\param if_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_if (NODE_T * if_p)
{
  NODE_T *then_p = top_down_series (NEXT (if_p)), *elif_p;
  if (then_p == NULL || WHETHER_NOT (then_p, THEN_SYMBOL)) {
    top_down_diagnose (if_p, then_p, CONDITIONAL_CLAUSE, THEN_SYMBOL);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (if_p, PREVIOUS (then_p), IF_SYMBOL);
  elif_p = top_down_series (NEXT (then_p));
  if (elif_p != NULL && WHETHER (elif_p, FI_SYMBOL)) {
    make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
    make_sub (if_p, elif_p, IF_SYMBOL);
    return (NEXT (if_p));
  }
  if (elif_p != NULL && WHETHER (elif_p, ELSE_SYMBOL)) {
    NODE_T *fi_p = top_down_series (NEXT (elif_p));
    if (fi_p == NULL || WHETHER_NOT (fi_p, FI_SYMBOL)) {
      top_down_diagnose (if_p, fi_p, CONDITIONAL_CLAUSE, FI_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    } else {
      make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
      make_sub (elif_p, PREVIOUS (fi_p), ELSE_SYMBOL);
      make_sub (if_p, fi_p, IF_SYMBOL);
      return (NEXT (if_p));
    }
  }
  if (elif_p != NULL && WHETHER (elif_p, ELIF_SYMBOL)) {
    NODE_T *fi_p = top_down_if (elif_p);
    make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
    make_sub (if_p, elif_p, IF_SYMBOL);
    return (fi_p);
  } else {
    top_down_diagnose (if_p, elif_p, CONDITIONAL_CLAUSE, FI_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief make branch of CASE .. IN .. OUT .. ESAC
\param case_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_case (NODE_T * case_p)
{
  NODE_T *in_p = top_down_series (NEXT (case_p)), *ouse_p;
  if (in_p == NULL || WHETHER_NOT (in_p, IN_SYMBOL)) {
    top_down_diagnose (case_p, in_p, ENCLOSED_CLAUSE, IN_SYMBOL);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (case_p, PREVIOUS (in_p), CASE_SYMBOL);
  ouse_p = top_down_series (NEXT (in_p));
  if (ouse_p != NULL && WHETHER (ouse_p, ESAC_SYMBOL)) {
    make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
    make_sub (case_p, ouse_p, CASE_SYMBOL);
    return (NEXT (case_p));
  }
  if (ouse_p != NULL && WHETHER (ouse_p, OUT_SYMBOL)) {
    NODE_T *esac_p = top_down_series (NEXT (ouse_p));
    if (esac_p == NULL || WHETHER_NOT (esac_p, ESAC_SYMBOL)) {
      top_down_diagnose (case_p, esac_p, ENCLOSED_CLAUSE, ESAC_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    } else {
      make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
      make_sub (ouse_p, PREVIOUS (esac_p), OUT_SYMBOL);
      make_sub (case_p, esac_p, CASE_SYMBOL);
      return (NEXT (case_p));
    }
  }
  if (ouse_p != NULL && WHETHER (ouse_p, OUSE_SYMBOL)) {
    NODE_T *esac_p = top_down_case (ouse_p);
    make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
    make_sub (case_p, ouse_p, CASE_SYMBOL);
    return (esac_p);
  } else {
    top_down_diagnose (case_p, ouse_p, ENCLOSED_CLAUSE, ESAC_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief skip a unit
\param p position in tree
\return token from where to proceed or NULL
**/

NODE_T *top_down_skip_unit (NODE_T * p)
{
  while (p != NULL && !whether_unit_terminator (p)) {
    if (WHETHER (p, BEGIN_SYMBOL)) {
      p = top_down_begin (p);
    } else if (WHETHER (p, SUB_SYMBOL)) {
      p = top_down_sub (p);
    } else if (WHETHER (p, OPEN_SYMBOL)) {
      p = top_down_open (p);
    } else if (WHETHER (p, IF_SYMBOL)) {
      p = top_down_if (p);
    } else if (WHETHER (p, CASE_SYMBOL)) {
      p = top_down_case (p);
    } else if (WHETHER (p, CODE_SYMBOL)) {
      p = top_down_code (p);
    } else if (WHETHER (p, ACCO_SYMBOL)) {
      p = top_down_acco (p);
    } else {
      FORWARD (p);
    }
  }
  return (p);
}

static NODE_T *top_down_skip_format (NODE_T *);

/*!
\brief make branch of ( .. ) in a format
\param open_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_format_open (NODE_T * open_p)
{
  NODE_T *close_p = top_down_skip_format (NEXT (open_p));
  if (close_p != NULL && WHETHER (close_p, FORMAT_CLOSE_SYMBOL)) {
    make_sub (open_p, close_p, FORMAT_OPEN_SYMBOL);
    return (NEXT (open_p));
  } else {
    top_down_diagnose (open_p, close_p, 0, FORMAT_CLOSE_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief skip a format text
\param p position in tree
\return token from where to proceed or NULL
**/

static NODE_T *top_down_skip_format (NODE_T * p)
{
  while (p != NULL) {
    if (WHETHER (p, FORMAT_OPEN_SYMBOL)) {
      p = top_down_format_open (p);
    } else if (whether_one_of (p, FORMAT_CLOSE_SYMBOL, FORMAT_DELIMITER_SYMBOL, NULL_ATTRIBUTE)) {
      return (p);
    } else {
      FORWARD (p);
    }
  }
  return (NULL);
}

/*!
\brief make branch of $ .. $
\param p position in tree
\return token from where to proceed or NULL
**/

static void top_down_formats (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (SUB (q) != NULL) {
      top_down_formats (SUB (q));
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, FORMAT_DELIMITER_SYMBOL)) {
      NODE_T *f = NEXT (q);
      while (f != NULL && WHETHER_NOT (f, FORMAT_DELIMITER_SYMBOL)) {
        if (WHETHER (f, FORMAT_OPEN_SYMBOL)) {
          f = top_down_format_open (f);
        } else {
          f = NEXT (f);
        }
      }
      if (f == NULL) {
        top_down_diagnose (p, f, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL);
        longjmp (top_down_crash_exit, 1);
      } else {
        make_sub (q, f, FORMAT_DELIMITER_SYMBOL);
      }
    }
  }
}

/*!
\brief make branches of phrases for the bottom-up parser
\param p position in tree
**/

void top_down_parser (NODE_T * p)
{
  if (p != NULL) {
    if (!setjmp (top_down_crash_exit)) {
      (void) top_down_series (p);
      top_down_loops (p);
      top_down_untils (p);
      top_down_formats (p);
    }
  }
}


/*!
\brief detect redefined keyword
\param p position in tree
*/

static void detect_redefined_keyword (NODE_T * p, int construct)
{
  if (p != NULL && whether (p, KEYWORD, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_REDEFINED_KEYWORD, SYMBOL (p), construct, NULL);
  }
}

/*!
\brief whether a series is serial or collateral
\param p position in tree
\return whether a series is serial or collateral
**/

static int serial_or_collateral (NODE_T * p)
{
  NODE_T *q;
  int semis = 0, commas = 0, exits = 0;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, COMMA_SYMBOL)) {
      commas++;
    } else if (WHETHER (q, SEMI_SYMBOL)) {
      semis++;
    } else if (WHETHER (q, EXIT_SYMBOL)) {
      exits++;
    }
  }
  if (semis == 0 && exits == 0 && commas > 0) {
    return (COLLATERAL_CLAUSE);
  } else if ((semis > 0 || exits > 0) && commas == 0) {
    return (SERIAL_CLAUSE);
  } else if (semis == 0 && exits == 0 && commas == 0) {
    return (SERIAL_CLAUSE);
  } else {
/* Heuristic guess to give intelligible error message */
    return ((semis + exits) >= commas ? SERIAL_CLAUSE : COLLATERAL_CLAUSE);
  }
}

/*!
\brief insert a node with attribute "a" after "p"
\param p position in tree
\param a attribute
**/

static void pad_node (NODE_T * p, int a)
{
/*
This is used to fill information that Algol 68 does not require to be present.
Filling in gives one format for such construct; this helps later passes.
*/
  NODE_T *z = new_node ();
  *z = *p;
  if (GENIE (p) != NULL) {
    GENIE (z) = new_genie_info ();
  }
  PREVIOUS (z) = p;
  SUB (z) = NULL;
  ATTRIBUTE (z) = a;
  MOID (z) = NULL;
  if (NEXT (z) != NULL) {
    PREVIOUS (NEXT (z)) = z;
  }
  NEXT (p) = z;
}

/*!
\brief diagnose extensions
\param p position in tree
**/

static void a68_extension (NODE_T * p)
{
  if (program.options.portcheck) {
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_EXTENSION, NULL);
  } else {
    diagnostic_node (A68_WARNING, p, WARNING_EXTENSION, NULL);
  }
}

/*!
\brief diagnose for clauses not yielding a value
\param p position in tree
**/

static void empty_clause (NODE_T * p)
{
  diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_CLAUSE_WITHOUT_VALUE);
}

#if ! (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)

/*!
\brief diagnose for parallel clause
\param p position in tree
**/

static void par_clause (NODE_T * p)
{
  diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_NO_PARALLEL_CLAUSE);
}

#endif

/*!
\brief diagnose for missing symbol
\param p position in tree
**/

static void strange_tokens (NODE_T * p)
{
  NODE_T *q = (p != NULL && NEXT (p) != NULL) ? NEXT (p) : p;
  diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_STRANGE_TOKENS);
}

/*!
\brief diagnose for strange separator
\param p position in tree
**/

static void strange_separator (NODE_T * p)
{
  NODE_T *q = (p != NULL && NEXT (p) != NULL) ? NEXT (p) : p;
  diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_STRANGE_SEPARATOR);
}

/*!
\brief whether match and reduce a sentence
\param p token where to start matching
\param a if not NULL, procedure to execute upon match
\param z if not NULL, to be set to TRUE upon match
**/

static void try_reduction (NODE_T * p, void (*a) (NODE_T *), BOOL_T * z, ...)
{
  va_list list;
  int result, arg;
  NODE_T *head = p, *tail = NULL;
  va_start (list, z);
  result = va_arg (list, int);
  while ((arg = va_arg (list, int)) != NULL_ATTRIBUTE)
  {
    BOOL_T keep_matching;
    if (p == NULL) {
      keep_matching = A68_FALSE;
    } else if (arg == WILDCARD) {
/* WILDCARD matches any Algol68G non terminal, but no keyword */
      keep_matching = (BOOL_T) (non_terminal_string (edit_line, ATTRIBUTE (p)) != NULL);
    } else {
      if (arg >= 0) {
        keep_matching = (BOOL_T) (arg == ATTRIBUTE (p));
      } else {
        keep_matching = (BOOL_T) (arg != ATTRIBUTE (p));
      }
    }
    if (keep_matching) {
      tail = p;
      FORWARD (p);
    } else {
      va_end (list);
      return;
    }
  }
/* Print parser reductions */
  if (head != NULL && program.options.reductions && LINE_NUMBER (head) > 0) {
    NODE_T *q;
    int count = 0;
    reductions++;
    where_in_source (STDOUT_FILENO, head);
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\nReduction %d: %s<-", reductions, non_terminal_string (edit_line, result)) >= 0);
    WRITE (STDOUT_FILENO, output_line);
    for (q = head; q != NULL && tail != NULL && q != NEXT (tail); FORWARD (q), count++) {
      int gatt = ATTRIBUTE (q);
      char *str = non_terminal_string (input_line, gatt);
      if (count > 0) {
        WRITE (STDOUT_FILENO, ", ");
      }
      if (str != NULL) {
        WRITE (STDOUT_FILENO, str);
        if (gatt == IDENTIFIER || gatt == OPERATOR || gatt == DENOTATION || gatt == INDICANT) {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, " \"%s\"", SYMBOL (q)) >= 0);
          WRITE (STDOUT_FILENO, output_line);
        }
      } else {
        WRITE (STDOUT_FILENO, SYMBOL (q));
      }
    }
  }
/* Make reduction */
  if (a != NULL) {
    a (head);
  }
  make_sub (head, tail, result);
  va_end (list);
  if (z != NULL) {
    *z = A68_TRUE;
  }
}

/*!
\brief driver for the bottom-up parser
\param p position in tree
**/

void bottom_up_parser (NODE_T * p)
{
  if (p != NULL) {
    if (!setjmp (bottom_up_crash_exit)) {
      ignore_superfluous_semicolons (p);
      reduce_particular_program (p);
    }
  }
}

/*!
\brief top-level reduction
\param p position in tree
**/

static void reduce_particular_program (NODE_T * p)
{
  NODE_T *q;
  int error_count_0 = program.error_count;
/* A program is "label sequence; particular program" */
  extract_labels (p, SERIAL_CLAUSE /* a fake here, but ok */ );
/* Parse the program itself */
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    if (SUB (q) != NULL) {
      reduce_subordinate (q, SOME_CLAUSE);
    }
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, LABEL, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE);
    }
  }
/* Determine the encompassing enclosed clause */
  for (q = p; q != NULL; FORWARD (q)) {
#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
    try_reduction (q, NULL, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
#else
    try_reduction (q, par_clause, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
#endif
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, PARALLEL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, LOOP_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CODE_CLAUSE, NULL_ATTRIBUTE);
  }
/* Try reducing the particular program */
  q = p;
  try_reduction (q, NULL, NULL, PARTICULAR_PROGRAM, LABEL, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, PARTICULAR_PROGRAM, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
  if (SUB (p) == NULL || NEXT (p) != NULL) {
    recover_from_error (p, PARTICULAR_PROGRAM, (BOOL_T) ((program.error_count - error_count_0) > MAX_ERRORS));
  }
}

/*!
\brief reduce the sub-phrase that starts one level down
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_subordinate (NODE_T * p, int expect)
{
/*
If this is unsuccessful then it will at least copy the resulting attribute
as the parser can repair some faults. This gives less spurious diagnostics.
*/
  if (p != NULL) {
    if (SUB (p) != NULL) {
      BOOL_T no_error = reduce_phrase (SUB (p), expect);
      ATTRIBUTE (p) = ATTRIBUTE (SUB (p));
      if (no_error) {
        SUB (p) = SUB_SUB (p);
      }
    }
  }
}

/*!
\brief driver for reducing a phrase
\param p position in tree
\param expect information the parser may have on what is expected
\return whether errors occured
**/

BOOL_T reduce_phrase (NODE_T * p, int expect)
{
  int error_count_0 = program.error_count, error_count_02;
  BOOL_T declarer_pack = (BOOL_T) (expect == STRUCTURE_PACK || expect == PARAMETER_PACK || expect == FORMAL_DECLARERS || expect == UNION_PACK || expect == SPECIFIER);
/* Sample all info needed to decide whether a bold tag is operator or indicant */
  extract_indicants (p);
  if (!declarer_pack) {
    extract_priorities (p);
    extract_operators (p);
  }
  error_count_02 = program.error_count;
  elaborate_bold_tags (p);
  if ((program.error_count - error_count_02) > 0) {
    longjmp (bottom_up_crash_exit, 1);
  }
/* Now we can reduce declarers, knowing which bold tags are indicants */
  reduce_declarers (p, expect);
/* Parse the phrase, as appropriate */
  if (!declarer_pack) {
    error_count_02 = program.error_count;
    extract_declarations (p);
    if ((program.error_count - error_count_02) > 0) {
      longjmp (bottom_up_crash_exit, 1);
    }
    extract_labels (p, expect);
    reduce_deeper_clauses_driver (p);
    reduce_statements (p, expect);
    reduce_right_to_left_constructs (p);
    reduce_constructs (p, expect);
    reduce_control_structure (p, expect);
  }
/* Do something intelligible if parsing failed */
  if (SUB (p) == NULL || NEXT (p) != NULL) {
    recover_from_error (p, expect, (BOOL_T) ((program.error_count - error_count_0) > MAX_ERRORS));
    return (A68_FALSE);
  } else {
    return (A68_TRUE);
  }
}

/*!
\brief driver for reducing declarers
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_declarers (NODE_T * p, int expect)
{
  reduce_lengtheties (p);
  reduce_indicants (p);
  reduce_small_declarers (p);
  reduce_declarer_lists (p);
  reduce_row_proc_op_declarers (p);
  if (expect == STRUCTURE_PACK) {
    reduce_struct_pack (p);
  } else if (expect == PARAMETER_PACK) {
    reduce_parameter_pack (p);
  } else if (expect == FORMAL_DECLARERS) {
    reduce_formal_declarer_pack (p);
  } else if (expect == UNION_PACK) {
    reduce_union_pack (p);
  } else if (expect == SPECIFIER) {
    reduce_specifiers (p);
  } else {
    NODE_T *q;
    for (q = p; q != NULL; FORWARD (q)) {
      if (whether (q, OPEN_SYMBOL, COLON_SYMBOL, NULL_ATTRIBUTE) && !(expect == GENERIC_ARGUMENT || expect == BOUNDS)) {
        if (whether_one_of (p, IN_SYMBOL, THEN_BAR_SYMBOL, NULL_ATTRIBUTE)) {
          reduce_subordinate (q, SPECIFIER);
        }
      }
      if (whether (q, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, NULL_ATTRIBUTE)) {
        reduce_subordinate (q, PARAMETER_PACK);
      }
      if (whether (q, OPEN_SYMBOL, VOID_SYMBOL, COLON_SYMBOL, NULL_ATTRIBUTE)) {
        reduce_subordinate (q, PARAMETER_PACK);
      }
    }
  }
}

/*!
\brief driver for reducing control structure elements
\param p position in tree
**/

static void reduce_deeper_clauses_driver (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL) {
      reduce_deeper_clauses (p);
    }
  }
}

/*!
\brief reduces PRIMARY, SECONDARY, TERITARY and FORMAT TEXT
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_statements (NODE_T * p, int expect)
{
  reduce_primary_bits (p, expect);
  if (expect != ENCLOSED_CLAUSE) {
    reduce_primaries (p, expect);
    if (expect == FORMAT_TEXT) {
      reduce_format_texts (p);
    } else {
      reduce_secondaries (p);
      reduce_formulae (p);
      reduce_tertiaries (p);
    }
  }
}

/*!
\brief handle cases that need reducing from right-to-left
\param p position in tree
**/

static void reduce_right_to_left_constructs (NODE_T * p)
{
/*
Here are cases that need reducing from right-to-left whereas many things
can be reduced left-to-right. Assignations are a notable example; one could
discuss whether it would not be more natural to write 1 =: k in stead of
k := 1. The latter is said to be more natural, or it could be just computing
history. Meanwhile we use this routine.
*/
  if (p != NULL) {
    reduce_right_to_left_constructs (NEXT (p));
/* Assignations */
    if (WHETHER (p, TERTIARY)) {
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, JUMP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, SKIP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
    }
/* Routine texts with parameter pack */
    else if (WHETHER (p, PARAMETER_PACK)) {
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, JUMP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, SKIP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, JUMP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, SKIP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
    }
/* Routine texts without parameter pack */
    else if (WHETHER (p, DECLARER)) {
      if (!(PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PARAMETER_PACK))) {
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, JUMP, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, SKIP, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      }
    } else if (WHETHER (p, VOID_SYMBOL)) {
      if (!(PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PARAMETER_PACK))) {
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, JUMP, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, SKIP, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      }
    }
  }
}

/*!
\brief  graciously ignore extra semicolons
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void ignore_superfluous_semicolons (NODE_T * p)
{
/*
This routine relaxes the parser a bit with respect to superfluous semicolons,
for instance "FI; OD". These provoke only a warning.
*/
  for (; p != NULL; FORWARD (p)) {
    ignore_superfluous_semicolons (SUB (p));
    if (NEXT (p) != NULL && WHETHER (NEXT (p), SEMI_SYMBOL) && NEXT_NEXT (p) == NULL) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, NEXT (p), WARNING_SKIPPED_SUPERFLUOUS, ATTRIBUTE (NEXT (p)));
      NEXT (p) = NULL;
    } else if (WHETHER (p, SEMI_SYMBOL) && whether_semicolon_less (NEXT (p))) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_SKIPPED_SUPERFLUOUS, ATTRIBUTE (p));
      if (PREVIOUS (p) != NULL) {
        NEXT (PREVIOUS (p)) = NEXT (p);
      }
      PREVIOUS (NEXT (p)) = PREVIOUS (p);
    }
  }
}

/*!
\brief reduce constructs in proper order
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_constructs (NODE_T * p, int expect)
{
  reduce_basic_declarations (p);
  reduce_units (p);
  reduce_erroneous_units (p);
  if (expect != UNIT) {
    if (expect == GENERIC_ARGUMENT) {
      reduce_generic_arguments (p);
    } else if (expect == BOUNDS) {
      reduce_bounds (p);
    } else {
      reduce_declaration_lists (p);
      if (expect != DECLARATION_LIST) {
        reduce_labels (p);
        if (expect == SOME_CLAUSE) {
          expect = serial_or_collateral (p);
        }
        if (expect == SERIAL_CLAUSE) {
          reduce_serial_clauses (p);
        } else if (expect == ENQUIRY_CLAUSE) {
          reduce_enquiry_clauses (p);
        } else if (expect == COLLATERAL_CLAUSE) {
          reduce_collateral_clauses (p);
        } else if (expect == ARGUMENT) {
          reduce_arguments (p);
        }
      }
    }
  }
}

/*!
\brief reduce control structure
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_control_structure (NODE_T * p, int expect)
{
  reduce_enclosed_clause_bits (p, expect);
  reduce_enclosed_clauses (p);
}

/*!
\brief reduce lengths in declarers
\param p position in tree
**/

static void reduce_lengtheties (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    try_reduction (q, NULL, NULL, LONGETY, LONG_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SHORTETY, SHORT_SYMBOL, NULL_ATTRIBUTE);
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, LONGETY, LONGETY, LONG_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, SHORTETY, SHORTETY, SHORT_SYMBOL, NULL_ATTRIBUTE);
    }
  }
}

/*!
\brief reduce indicants
\param p position in tree
**/

static void reduce_indicants (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INDICANT, INT_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, REAL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, BITS_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, BYTES_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, COMPLEX_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, COMPL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, BOOL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, CHAR_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, FORMAT_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, STRING_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, FILE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, CHANNEL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, SEMA_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, PIPE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, SOUND_SYMBOL, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce basic declarations, like LONG BITS, STRING, ..
\param p position in tree
**/

static void reduce_small_declarers (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (whether (q, LONGETY, INDICANT, NULL_ATTRIBUTE)) {
      int a;
      if (SUB_NEXT (q) == NULL) {
        diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
        try_reduction (q, NULL, NULL, DECLARER, LONGETY, INDICANT, NULL_ATTRIBUTE);
      } else {
        a = ATTRIBUTE (SUB_NEXT (q));
        if (a == INT_SYMBOL || a == REAL_SYMBOL || a == BITS_SYMBOL || a == BYTES_SYMBOL || a == COMPLEX_SYMBOL || a == COMPL_SYMBOL) {
          try_reduction (q, NULL, NULL, DECLARER, LONGETY, INDICANT, NULL_ATTRIBUTE);
        } else {
          diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
          try_reduction (q, NULL, NULL, DECLARER, LONGETY, INDICANT, NULL_ATTRIBUTE);
        }
      }
    } else if (whether (q, SHORTETY, INDICANT, NULL_ATTRIBUTE)) {
      int a;
      if (SUB_NEXT (q) == NULL) {
        diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
        try_reduction (q, NULL, NULL, DECLARER, SHORTETY, INDICANT, NULL_ATTRIBUTE);
      } else {
        a = ATTRIBUTE (SUB_NEXT (q));
        if (a == INT_SYMBOL || a == REAL_SYMBOL || a == BITS_SYMBOL || a == BYTES_SYMBOL || a == COMPLEX_SYMBOL || a == COMPL_SYMBOL) {
          try_reduction (q, NULL, NULL, DECLARER, SHORTETY, INDICANT, NULL_ATTRIBUTE);
        } else {
          diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
          try_reduction (q, NULL, NULL, DECLARER, LONGETY, INDICANT, NULL_ATTRIBUTE);
        }
      }
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, DECLARER, INDICANT, NULL_ATTRIBUTE);
  }
}

/*!
\brief whether formal bounds
\param p position in tree
\return whether formal bounds
**/

static BOOL_T whether_formal_bounds (NODE_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else {
    switch (ATTRIBUTE (p)) {
    case OPEN_SYMBOL:
    case CLOSE_SYMBOL:
    case SUB_SYMBOL:
    case BUS_SYMBOL:
    case COMMA_SYMBOL:
    case COLON_SYMBOL:
    case DOTDOT_SYMBOL:
    case INT_DENOTATION:
    case IDENTIFIER:
    case OPERATOR:
      {
        return ((BOOL_T) (whether_formal_bounds (SUB (p)) && whether_formal_bounds (NEXT (p))));
      }
    default:
      {
        return (A68_FALSE);
      }
    }
  }
}

/*!
\brief reduce declarer lists for packs
\param p position in tree
**/

static void reduce_declarer_lists (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (NEXT (q) != NULL && SUB_NEXT (q) != NULL) {
      if (WHETHER (q, STRUCT_SYMBOL)) {
        reduce_subordinate (NEXT (q), STRUCTURE_PACK);
        try_reduction (q, NULL, NULL, DECLARER, STRUCT_SYMBOL, STRUCTURE_PACK, NULL_ATTRIBUTE);
      } else if (WHETHER (q, UNION_SYMBOL)) {
        reduce_subordinate (NEXT (q), UNION_PACK);
        try_reduction (q, NULL, NULL, DECLARER, UNION_SYMBOL, UNION_PACK, NULL_ATTRIBUTE);
      } else if (WHETHER (q, PROC_SYMBOL)) {
        if (whether (q, PROC_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE)) {
          if (!whether_formal_bounds (SUB_NEXT (q))) {
            reduce_subordinate (NEXT (q), FORMAL_DECLARERS);
          }
        }
      } else if (WHETHER (q, OP_SYMBOL)) {
        if (whether (q, OP_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE)) {
          if (!whether_formal_bounds (SUB_NEXT (q))) {
            reduce_subordinate (NEXT (q), FORMAL_DECLARERS);
          }
        }
      }
    }
  }
}

/*!
\brief reduce ROW, PROC and OP declarers
\param p position in tree
**/

static void reduce_row_proc_op_declarers (NODE_T * p)
{
  BOOL_T siga = A68_TRUE;
  while (siga) {
    NODE_T *q;
    siga = A68_FALSE;
    for (q = p; q != NULL; FORWARD (q)) {
/* FLEX DECL */
      if (whether (q, FLEX_SYMBOL, DECLARER, NULL_ATTRIBUTE)) {
        try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      }
/* FLEX [] DECL */
      if (whether (q, FLEX_SYMBOL, SUB_SYMBOL, DECLARER, NULL_ATTRIBUTE) && SUB_NEXT (q) != NULL) {
        reduce_subordinate (NEXT (q), BOUNDS);
        try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, BOUNDS, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
      }
/* FLEX () DECL */
      if (whether (q, FLEX_SYMBOL, OPEN_SYMBOL, DECLARER, NULL_ATTRIBUTE) && SUB_NEXT (q) != NULL) {
        if (!whether (q, FLEX_SYMBOL, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, NULL_ATTRIBUTE)) {
          reduce_subordinate (NEXT (q), BOUNDS);
          try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, BOUNDS, DECLARER, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
        }
      }
/* [] DECL */
      if (whether (q, SUB_SYMBOL, DECLARER, NULL_ATTRIBUTE) && SUB (q) != NULL) {
        reduce_subordinate (q, BOUNDS);
        try_reduction (q, NULL, &siga, DECLARER, BOUNDS, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
      }
/* () DECL */
      if (whether (q, OPEN_SYMBOL, DECLARER, NULL_ATTRIBUTE) && SUB (q) != NULL) {
        if (whether (q, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, NULL_ATTRIBUTE)) {
/* Catch e.g. (INT i) () INT: */
          if (whether_formal_bounds (SUB (q))) {
            reduce_subordinate (q, BOUNDS);
            try_reduction (q, NULL, &siga, DECLARER, BOUNDS, DECLARER, NULL_ATTRIBUTE);
            try_reduction (q, NULL, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
          }
        } else {
          reduce_subordinate (q, BOUNDS);
          try_reduction (q, NULL, &siga, DECLARER, BOUNDS, DECLARER, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
        }
      }
    }
/* PROC DECL, PROC () DECL, OP () DECL */
    for (q = p; q != NULL; FORWARD (q)) {
      int a = ATTRIBUTE (q);
      if (a == REF_SYMBOL) {
        try_reduction (q, NULL, &siga, DECLARER, REF_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      } else if (a == PROC_SYMBOL) {
        try_reduction (q, NULL, &siga, DECLARER, PROC_SYMBOL, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, PROC_SYMBOL, FORMAL_DECLARERS, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, PROC_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, PROC_SYMBOL, FORMAL_DECLARERS, VOID_SYMBOL, NULL_ATTRIBUTE);
      } else if (a == OP_SYMBOL) {
        try_reduction (q, NULL, &siga, OPERATOR_PLAN, OP_SYMBOL, FORMAL_DECLARERS, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, OPERATOR_PLAN, OP_SYMBOL, FORMAL_DECLARERS, VOID_SYMBOL, NULL_ATTRIBUTE);
      }
    }
  }
}

/*!
\brief reduce structure packs
\param p position in tree
**/

static void reduce_struct_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, STRUCTURED_FIELD, DECLARER, IDENTIFIER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, STRUCTURED_FIELD, STRUCTURED_FIELD, COMMA_SYMBOL, IDENTIFIER, NULL_ATTRIBUTE);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, COMMA_SYMBOL, STRUCTURED_FIELD, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, SEMI_SYMBOL, STRUCTURED_FIELD, NULL_ATTRIBUTE);
    }
  }
  q = p;
  try_reduction (q, NULL, NULL, STRUCTURE_PACK, OPEN_SYMBOL, STRUCTURED_FIELD_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce parameter packs
\param p position in tree
**/

static void reduce_parameter_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, PARAMETER, DECLARER, IDENTIFIER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PARAMETER, PARAMETER, COMMA_SYMBOL, IDENTIFIER, NULL_ATTRIBUTE);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, PARAMETER_LIST, PARAMETER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PARAMETER_LIST, PARAMETER_LIST, COMMA_SYMBOL, PARAMETER, NULL_ATTRIBUTE);
    }
  }
  q = p;
  try_reduction (q, NULL, NULL, PARAMETER_PACK, OPEN_SYMBOL, PARAMETER_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce formal declarer packs
\param p position in tree
**/

static void reduce_formal_declarer_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, FORMAL_DECLARERS_LIST, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, COMMA_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, SEMI_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, DECLARER, NULL_ATTRIBUTE);
    }
  }
  q = p;
  try_reduction (q, NULL, NULL, FORMAL_DECLARERS, OPEN_SYMBOL, FORMAL_DECLARERS_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce union packs (formal declarers and VOID)
\param p position in tree
**/

static void reduce_union_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, UNION_DECLARER_LIST, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, UNION_DECLARER_LIST, VOID_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, COMMA_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, COMMA_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, SEMI_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, SEMI_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, VOID_SYMBOL, NULL_ATTRIBUTE);
    }
  }
  q = p;
  try_reduction (q, NULL, NULL, UNION_PACK, OPEN_SYMBOL, UNION_DECLARER_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce specifiers
\param p position in tree
**/

static void reduce_specifiers (NODE_T * p)
{
  NODE_T *q = p;
  try_reduction (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, DECLARER, IDENTIFIER, CLOSE_SYMBOL, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, DECLARER, CLOSE_SYMBOL, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, VOID_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce control structure elements
\param p position in tree
**/

static void reduce_deeper_clauses (NODE_T * p)
{
  if (WHETHER (p, FORMAT_DELIMITER_SYMBOL)) {
    reduce_subordinate (p, FORMAT_TEXT);
  } else if (WHETHER (p, FORMAT_OPEN_SYMBOL)) {
    reduce_subordinate (p, FORMAT_TEXT);
  } else if (WHETHER (p, OPEN_SYMBOL)) {
    if (NEXT (p) != NULL && WHETHER (NEXT (p), THEN_BAR_SYMBOL)) {
      reduce_subordinate (p, ENQUIRY_CLAUSE);
    } else if (PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PAR_SYMBOL)) {
      reduce_subordinate (p, COLLATERAL_CLAUSE);
    }
  } else if (whether_one_of (p, IF_SYMBOL, ELIF_SYMBOL, CASE_SYMBOL, OUSE_SYMBOL, WHILE_SYMBOL, UNTIL_SYMBOL, ELSE_BAR_SYMBOL, ACCO_SYMBOL, NULL_ATTRIBUTE)) {
    reduce_subordinate (p, ENQUIRY_CLAUSE);
  } else if (WHETHER (p, BEGIN_SYMBOL)) {
    reduce_subordinate (p, SOME_CLAUSE);
  } else if (whether_one_of (p, THEN_SYMBOL, ELSE_SYMBOL, OUT_SYMBOL, DO_SYMBOL, ALT_DO_SYMBOL, CODE_SYMBOL, NULL_ATTRIBUTE)) {
    reduce_subordinate (p, SERIAL_CLAUSE);
  } else if (WHETHER (p, IN_SYMBOL)) {
    reduce_subordinate (p, COLLATERAL_CLAUSE);
  } else if (WHETHER (p, THEN_BAR_SYMBOL)) {
    reduce_subordinate (p, SOME_CLAUSE);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    reduce_subordinate (p, ENCLOSED_CLAUSE);
  } else if (whether_one_of (p, FOR_SYMBOL, FROM_SYMBOL, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, NULL_ATTRIBUTE)) {
    reduce_subordinate (p, UNIT);
  }
}

/*!
\brief reduce primary elements
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_primary_bits (NODE_T * p, int expect)
{
  NODE_T *q = p;
  for (; q != NULL; FORWARD (q)) {
    if (whether (q, IDENTIFIER, OF_SYMBOL, NULL_ATTRIBUTE)) {
      ATTRIBUTE (q) = FIELD_IDENTIFIER;
    }
    try_reduction (q, NULL, NULL, ENVIRON_NAME, ENVIRON_SYMBOL, ROW_CHAR_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, NIHIL, NIL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SKIP, SKIP_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SELECTOR, FIELD_IDENTIFIER, OF_SYMBOL, NULL_ATTRIBUTE);
/* JUMPs without GOTO are resolved later */
    try_reduction (q, NULL, NULL, JUMP, GOTO_SYMBOL, IDENTIFIER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, LONGETY, INT_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, LONGETY, REAL_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, LONGETY, BITS_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, SHORTETY, INT_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, SHORTETY, REAL_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, SHORTETY, BITS_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, INT_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, REAL_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, BITS_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, ROW_CHAR_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, TRUE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, FALSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, EMPTY_SYMBOL, NULL_ATTRIBUTE);
    if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
      BOOL_T siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        try_reduction (q, NULL, &siga, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, LABEL, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE);
      }
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
    try_reduction (q, NULL, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
#else
    try_reduction (q, par_clause, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
#endif
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, PARALLEL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, LOOP_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CODE_CLAUSE, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce primaries completely
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_primaries (NODE_T * p, int expect)
{
  NODE_T *q = p;
  while (q != NULL) {
    BOOL_T fwd = A68_TRUE, siga;
/* Primaries excepts call and slice */
    try_reduction (q, NULL, NULL, PRIMARY, IDENTIFIER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIMARY, DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, CAST, DECLARER, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, CAST, VOID_SYMBOL, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ASSERTION, ASSERT_SYMBOL, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIMARY, CAST, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIMARY, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIMARY, FORMAT_TEXT, NULL_ATTRIBUTE);
/* Call and slice */
    siga = A68_TRUE;
    while (siga) {
      NODE_T *x = NEXT (q);
      siga = A68_FALSE;
      if (WHETHER (q, PRIMARY) && x != NULL) {
        if (WHETHER (x, OPEN_SYMBOL)) {
          reduce_subordinate (NEXT (q), GENERIC_ARGUMENT);
          try_reduction (q, NULL, &siga, SPECIFICATION, PRIMARY, GENERIC_ARGUMENT, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, PRIMARY, SPECIFICATION, NULL_ATTRIBUTE);
        } else if (WHETHER (x, SUB_SYMBOL)) {
          reduce_subordinate (NEXT (q), GENERIC_ARGUMENT);
          try_reduction (q, NULL, &siga, SPECIFICATION, PRIMARY, GENERIC_ARGUMENT, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, PRIMARY, SPECIFICATION, NULL_ATTRIBUTE);
        }
      }
    }
/* Now that call and slice are known, reduce remaining ( .. ) */
    if (WHETHER (q, OPEN_SYMBOL) && SUB (q) != NULL) {
      reduce_subordinate (q, SOME_CLAUSE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, NULL_ATTRIBUTE);
      if (PREVIOUS (q) != NULL) {
        q = PREVIOUS (q);
        fwd = A68_FALSE;
      }
    }
/* Format text items */
    if (expect == FORMAT_TEXT) {
      NODE_T *r;
      for (r = p; r != NULL; FORWARD (r)) {
        try_reduction (r, NULL, NULL, DYNAMIC_REPLICATOR, FORMAT_ITEM_N, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
        try_reduction (r, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_G, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
        try_reduction (r, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_H, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
        try_reduction (r, NULL, NULL, FORMAT_PATTERN, FORMAT_ITEM_F, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
      }
    }
    if (fwd) {
      FORWARD (q);
    }
  }
}

/*!
\brief enforce that ambiguous patterns are separated by commas
\param p position in tree
**/

static void ambiguous_patterns (NODE_T * p)
{
/*
Example: printf (($+d.2d +d.2d$, 1, 2)) can produce either "+1.00 +2.00" or
"+1+002.00". A comma must be supplied to resolve the ambiguity.

The obvious thing would be to weave this into the syntax, letting the BU parser
sort it out. But the C-style patterns do not suffer from Algol 68 pattern
ambiguity, so by solving it this way we maximise freedom in writing the patterns
as we want without introducing two "kinds" of patterns, and so we have shorter
routines for implementing formatted transput. This is a pragmatic system.
*/
  NODE_T *q, *last_pat = NULL;
  for (q = p; q != NULL; FORWARD (q)) {
    switch (ATTRIBUTE (q)) {
    case INTEGRAL_PATTERN:     /* These are the potentially ambiguous patterns */
    case REAL_PATTERN:
    case COMPLEX_PATTERN:
    case BITS_PATTERN:
      {
        if (last_pat != NULL) {
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_COMMA_MUST_SEPARATE, ATTRIBUTE (last_pat), ATTRIBUTE (q));
        }
        last_pat = q;
        break;
      }
    case COMMA_SYMBOL:
      {
        last_pat = NULL;
        break;
      }
    }
  }
}

/*!
\brief reduce format texts completely
\param p position in tree
**/

void reduce_c_pattern (NODE_T * p, int pr, int let)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce format texts completely
\param p position in tree
**/

static void reduce_format_texts (NODE_T * p)
{
  NODE_T *q;
/* Replicators */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REPLICATOR, STATIC_REPLICATOR, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REPLICATOR, DYNAMIC_REPLICATOR, NULL_ATTRIBUTE);
  }
/* "OTHER" patterns */
  reduce_c_pattern (p, BITS_C_PATTERN, FORMAT_ITEM_B);
  reduce_c_pattern (p, BITS_C_PATTERN, FORMAT_ITEM_O);
  reduce_c_pattern (p, BITS_C_PATTERN, FORMAT_ITEM_X);
  reduce_c_pattern (p, CHAR_C_PATTERN, FORMAT_ITEM_C);
  reduce_c_pattern (p, FIXED_C_PATTERN, FORMAT_ITEM_F);
  reduce_c_pattern (p, FLOAT_C_PATTERN, FORMAT_ITEM_E);
  reduce_c_pattern (p, GENERAL_C_PATTERN, FORMAT_ITEM_G);
  reduce_c_pattern (p, INTEGRAL_C_PATTERN, FORMAT_ITEM_D);
  reduce_c_pattern (p, INTEGRAL_C_PATTERN, FORMAT_ITEM_I);
  reduce_c_pattern (p, STRING_C_PATTERN, FORMAT_ITEM_S);
/* Radix frames */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, RADIX_FRAME, REPLICATOR, FORMAT_ITEM_R, NULL_ATTRIBUTE);
  }
/* Insertions */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_X, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_Y, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_L, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_P, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_Q, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_K, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, LITERAL, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INSERTION, REPLICATOR, INSERTION, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, INSERTION, INSERTION, INSERTION, NULL_ATTRIBUTE);
    }
  }
/* Replicated suppressible frames */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_A, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_Z, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_D, NULL_ATTRIBUTE);
  }
/* Suppressible frames */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_A, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_Z, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_D, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_E_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_E, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_POINT_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_POINT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_I_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_I, NULL_ATTRIBUTE);
  }
/* Replicated frames */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, REPLICATOR, FORMAT_ITEM_A, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, REPLICATOR, FORMAT_ITEM_Z, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, REPLICATOR, FORMAT_ITEM_D, NULL_ATTRIBUTE);
  }
/* Frames */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, FORMAT_ITEM_A, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, FORMAT_ITEM_Z, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, FORMAT_ITEM_D, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_E_FRAME, FORMAT_ITEM_E, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_I_FRAME, FORMAT_ITEM_I, NULL_ATTRIBUTE);
  }
/* Frames with an insertion */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, INSERTION, FORMAT_A_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, INSERTION, FORMAT_Z_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, INSERTION, FORMAT_D_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_E_FRAME, INSERTION, FORMAT_E_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_POINT_FRAME, INSERTION, FORMAT_POINT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_I_FRAME, INSERTION, FORMAT_I_FRAME, NULL_ATTRIBUTE);
  }
/* String patterns */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, STRING_PATTERN, REPLICATOR, FORMAT_A_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, STRING_PATTERN, FORMAT_A_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, STRING_PATTERN, STRING_PATTERN, STRING_PATTERN, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, STRING_PATTERN, STRING_PATTERN, INSERTION, STRING_PATTERN, NULL_ATTRIBUTE);
    }
  }
/* Integral moulds */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INTEGRAL_MOULD, FORMAT_Z_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INTEGRAL_MOULD, FORMAT_D_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, INTEGRAL_MOULD, INTEGRAL_MOULD, INTEGRAL_MOULD, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, INTEGRAL_MOULD, INTEGRAL_MOULD, INSERTION, NULL_ATTRIBUTE);
    }
  }
/* Sign moulds */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_ITEM_PLUS, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_ITEM_MINUS, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, SIGN_MOULD, FORMAT_ITEM_PLUS, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SIGN_MOULD, FORMAT_ITEM_MINUS, NULL_ATTRIBUTE);
  }
/* Exponent frames */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, EXPONENT_FRAME, FORMAT_E_FRAME, SIGN_MOULD, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, EXPONENT_FRAME, FORMAT_E_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
  }
/* Real patterns */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, FORMAT_POINT_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
  }
/* Complex patterns */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, COMPLEX_PATTERN, REAL_PATTERN, FORMAT_I_FRAME, REAL_PATTERN, NULL_ATTRIBUTE);
  }
/* Bits patterns */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, BITS_PATTERN, RADIX_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
  }
/* Integral patterns */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INTEGRAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INTEGRAL_PATTERN, INTEGRAL_MOULD, NULL_ATTRIBUTE);
  }
/* Patterns */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, BOOLEAN_PATTERN, FORMAT_ITEM_B, COLLECTION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, CHOICE_PATTERN, FORMAT_ITEM_C, COLLECTION, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, BOOLEAN_PATTERN, FORMAT_ITEM_B, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_G, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_H, NULL_ATTRIBUTE);
  }
  ambiguous_patterns (p);
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, a68_extension, NULL, A68_PATTERN, BITS_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, CHAR_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, FIXED_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, FLOAT_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, GENERAL_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, INTEGRAL_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, STRING_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, BITS_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, BOOLEAN_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, CHOICE_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, COMPLEX_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, FORMAT_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, GENERAL_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, INTEGRAL_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, REAL_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, STRING_PATTERN, NULL_ATTRIBUTE);
  }
/* Pictures */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, PICTURE, INSERTION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PICTURE, A68_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PICTURE, COLLECTION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PICTURE, REPLICATOR, COLLECTION, NULL_ATTRIBUTE);
  }
/* Picture lists */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, PICTURE)) {
      BOOL_T siga = A68_TRUE;
      try_reduction (q, NULL, NULL, PICTURE_LIST, PICTURE, NULL_ATTRIBUTE);
      while (siga) {
        siga = A68_FALSE;
        try_reduction (q, NULL, &siga, PICTURE_LIST, PICTURE_LIST, COMMA_SYMBOL, PICTURE, NULL_ATTRIBUTE);
        /* We filtered ambiguous patterns, so commas may be omitted */
        try_reduction (q, NULL, &siga, PICTURE_LIST, PICTURE_LIST, PICTURE, NULL_ATTRIBUTE);
      }
    }
  }
}

/*!
\brief reduce secondaries completely
\param p position in tree
**/

static void reduce_secondaries (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, SECONDARY, PRIMARY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERATOR, LOC_SYMBOL, DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERATOR, HEAP_SYMBOL, DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERATOR, NEW_SYMBOL, DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SECONDARY, GENERATOR, NULL_ATTRIBUTE);
  }
  siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    for (q = p; NEXT (q) != NULL; FORWARD (q)) {
      ;
    }
    for (; q != NULL; q = PREVIOUS (q)) {
      try_reduction (q, NULL, &siga, SELECTION, SELECTOR, SECONDARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, SECONDARY, SELECTION, NULL_ATTRIBUTE);
    }
  }
}

/*!
\brief whether "q" is an operator with priority "k"
\param q operator token
\param k priority
\return whether "q" is an operator with priority "k"
**/

static int operator_with_priority (NODE_T * q, int k)
{
  return (NEXT (q) != NULL && ATTRIBUTE (NEXT (q)) == OPERATOR && PRIO (NEXT (q)->info) == k);
}

/*!
\brief reduce formulae
\param p position in tree
**/

static void reduce_formulae (NODE_T * p)
{
  NODE_T *q = p;
  int priority;
  while (q != NULL) {
    if (whether_one_of (q, OPERATOR, SECONDARY, NULL_ATTRIBUTE)) {
      q = reduce_dyadic (q, NULL_ATTRIBUTE);
    } else {
      FORWARD (q);
    }
  }
/* Reduce the expression */
  for (priority = MAX_PRIORITY; priority >= 0; priority--) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (operator_with_priority (q, priority)) {
        BOOL_T siga = A68_FALSE;
        NODE_T *op = NEXT (q);
        if (WHETHER (q, SECONDARY)) {
          try_reduction (q, NULL, &siga, FORMULA, SECONDARY, OPERATOR, SECONDARY, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, FORMULA, SECONDARY, OPERATOR, MONADIC_FORMULA, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, FORMULA, SECONDARY, OPERATOR, FORMULA, NULL_ATTRIBUTE);
        } else if (WHETHER (q, MONADIC_FORMULA)) {
          try_reduction (q, NULL, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, SECONDARY, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, MONADIC_FORMULA, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, FORMULA, NULL_ATTRIBUTE);
        }
        if (priority == 0 && siga) {
          diagnostic_node (A68_SYNTAX_ERROR, op, ERROR_NO_PRIORITY, NULL);
        }
        siga = A68_TRUE;
        while (siga) {
          NODE_T *op2 = NEXT (q);
          siga = A68_FALSE;
          if (operator_with_priority (q, priority)) {
            try_reduction (q, NULL, &siga, FORMULA, FORMULA, OPERATOR, SECONDARY, NULL_ATTRIBUTE);
          }
          if (operator_with_priority (q, priority)) {
            try_reduction (q, NULL, &siga, FORMULA, FORMULA, OPERATOR, MONADIC_FORMULA, NULL_ATTRIBUTE);
          }
          if (operator_with_priority (q, priority)) {
            try_reduction (q, NULL, &siga, FORMULA, FORMULA, OPERATOR, FORMULA, NULL_ATTRIBUTE);
          }
          if (priority == 0 && siga) {
            diagnostic_node (A68_SYNTAX_ERROR, op2, ERROR_NO_PRIORITY, NULL);
          }
        }
      }
    }
  }
}

/*!
\brief reduce dyadic expressions
\param p position in tree
\param u current priority
\return token from where to continue
**/

static NODE_T *reduce_dyadic (NODE_T * p, int u)
{
/* We work inside out - higher priority expressions get reduced first */
  if (u > MAX_PRIORITY) {
    if (p == NULL) {
      return (NULL);
    } else if (WHETHER (p, OPERATOR)) {
/* Reduce monadic formulas */
      NODE_T *q = p;
      BOOL_T siga;
      do {
        PRIO (INFO (q)) = 10;
        siga = (BOOL_T) ((NEXT (q) != NULL) && (WHETHER (NEXT (q), OPERATOR)));
        if (siga) {
          FORWARD (q);
        }
      } while (siga);
      try_reduction (q, NULL, NULL, MONADIC_FORMULA, OPERATOR, SECONDARY, NULL_ATTRIBUTE);
      while (q != p) {
        q = PREVIOUS (q);
        try_reduction (q, NULL, NULL, MONADIC_FORMULA, OPERATOR, MONADIC_FORMULA, NULL_ATTRIBUTE);
      }
    }
    FORWARD (p);
  } else {
    p = reduce_dyadic (p, u + 1);
    while (p != NULL && WHETHER (p, OPERATOR) && PRIO (INFO (p)) == u) {
      FORWARD (p);
      p = reduce_dyadic (p, u + 1);
    }
  }
  return (p);
}

/*!
\brief reduce tertiaries completely
\param p position in tree
**/

static void reduce_tertiaries (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, TERTIARY, NIHIL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMULA, MONADIC_FORMULA, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, TERTIARY, FORMULA, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, TERTIARY, SECONDARY, NULL_ATTRIBUTE);
  }
  siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    for (q = p; q != NULL; FORWARD (q)) {
      try_reduction (q, NULL, &siga, TRANSPOSE_FUNCTION, TRANSPOSE_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, DIAGONAL_FUNCTION, TERTIARY, DIAGONAL_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, DIAGONAL_FUNCTION, DIAGONAL_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, COLUMN_FUNCTION, TERTIARY, COLUMN_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, COLUMN_FUNCTION, COLUMN_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, ROW_FUNCTION, TERTIARY, ROW_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, ROW_FUNCTION, ROW_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
    }
    for (q = p; q != NULL; FORWARD (q)) {
      try_reduction (q, a68_extension, &siga, TERTIARY, TRANSPOSE_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (q, a68_extension, &siga, TERTIARY, DIAGONAL_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (q, a68_extension, &siga, TERTIARY, COLUMN_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (q, a68_extension, &siga, TERTIARY, ROW_FUNCTION, NULL_ATTRIBUTE);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, IDENTITY_RELATION, TERTIARY, IS_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, IDENTITY_RELATION, TERTIARY, ISNT_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, AND_FUNCTION, TERTIARY, ANDF_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, OR_FUNCTION, TERTIARY, ORF_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce declarations
\param p position in tree
**/

static void reduce_basic_declarations (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, ENVIRON_NAME, ENVIRON_SYMBOL, ROW_CHAR_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIORITY_DECLARATION, PRIO_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, PRIORITY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PROCEDURE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PROCEDURE_VARIABLE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PROCEDURE_VARIABLE_DECLARATION, QUALIFIER, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, BRIEF_OPERATOR_DECLARATION, OP_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
/* Errors */
    try_reduction (q, strange_tokens, NULL, PRIORITY_DECLARATION, PRIO_SYMBOL, -DEFINING_OPERATOR, -EQUALS_SYMBOL, -PRIORITY, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, -DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, PROCEDURE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, -ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, PROCEDURE_VARIABLE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, -ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, PROCEDURE_VARIABLE_DECLARATION, QUALIFIER, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, -ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, BRIEF_OPERATOR_DECLARATION, OP_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, -ROUTINE_TEXT, NULL_ATTRIBUTE);
/* Errors. WILDCARD catches TERTIARY which catches IDENTIFIER */
    try_reduction (q, strange_tokens, NULL, PROCEDURE_DECLARATION, PROC_SYMBOL, WILDCARD, ROUTINE_TEXT, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, ENVIRON_NAME, ENVIRON_NAME, COMMA_SYMBOL, ROW_CHAR_DENOTATION, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PRIORITY_DECLARATION, PRIORITY_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, PRIORITY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, MODE_DECLARATION, MODE_DECLARATION, COMMA_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, MODE_DECLARATION, MODE_DECLARATION, COMMA_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PROCEDURE_DECLARATION, PROCEDURE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PROCEDURE_VARIABLE_DECLARATION, PROCEDURE_VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, BRIEF_OPERATOR_DECLARATION, BRIEF_OPERATOR_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
/* Errors. WILDCARD catches TERTIARY which catches IDENTIFIER */
      try_reduction (q, strange_tokens, &siga, PROCEDURE_DECLARATION, PROCEDURE_DECLARATION, COMMA_SYMBOL, WILDCARD, ROUTINE_TEXT, NULL_ATTRIBUTE);
    } while (siga);
  }
}

/*!
\brief reduce units
\param p position in tree
**/

static void reduce_units (NODE_T * p)
{
  NODE_T *q;
/* Stray ~ is a SKIP */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, OPERATOR) && WHETHER_LITERALLY (q, "~")) {
      ATTRIBUTE (q) = SKIP;
    }
  }
/* Reduce units */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, UNIT, ASSIGNATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, IDENTITY_RELATION, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, UNIT, AND_FUNCTION, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, UNIT, OR_FUNCTION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, JUMP, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, SKIP, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, TERTIARY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, ASSERTION, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce_generic arguments
\param p position in tree
**/

static void reduce_generic_arguments (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, UNIT)) {
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, NULL_ATTRIBUTE);
    } else if (WHETHER (q, COLON_SYMBOL)) {
      try_reduction (q, NULL, NULL, TRIMMER, COLON_SYMBOL, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, COLON_SYMBOL, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, COLON_SYMBOL, NULL_ATTRIBUTE);
    } else if (WHETHER (q, DOTDOT_SYMBOL)) {
      try_reduction (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, NULL_ATTRIBUTE);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, TRIMMER, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, TRIMMER, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
  }
  for (q = p; q && NEXT (q); FORWARD (q)) {
    if (WHETHER (q, COMMA_SYMBOL)) {
      if (!(ATTRIBUTE (NEXT (q)) == UNIT || ATTRIBUTE (NEXT (q)) == TRIMMER)) {
        pad_node (q, TRIMMER);
      }
    } else {
      if (WHETHER (NEXT (q), COMMA_SYMBOL)) {
        if (WHETHER_NOT (q, UNIT) && WHETHER_NOT (q, TRIMMER)) {
          pad_node (q, TRIMMER);
        }
      }
    }
  }
  q = NEXT (p);
  ABEND (q == NULL, "erroneous parser state", NULL);
  try_reduction (q, NULL, NULL, GENERIC_ARGUMENT_LIST, UNIT, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, GENERIC_ARGUMENT_LIST, TRIMMER, NULL_ATTRIBUTE);
  do {
    siga = A68_FALSE;
    try_reduction (q, NULL, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, COMMA_SYMBOL, TRIMMER, NULL_ATTRIBUTE);
    try_reduction (q, strange_separator, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, strange_separator, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, TRIMMER, NULL_ATTRIBUTE);
  } while (siga);
}

/*!
\brief reduce bounds
\param p position in tree
**/

static void reduce_bounds (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, BOUND, UNIT, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, BOUND, UNIT, DOTDOT_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, BOUND, UNIT, NULL_ATTRIBUTE);
  }
  q = NEXT (p);
  try_reduction (q, NULL, NULL, BOUNDS_LIST, BOUND, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, FORMAL_BOUNDS_LIST, COMMA_SYMBOL, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, ALT_FORMAL_BOUNDS_LIST, COLON_SYMBOL, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, ALT_FORMAL_BOUNDS_LIST, DOTDOT_SYMBOL, NULL_ATTRIBUTE);
  do {
    siga = A68_FALSE;
    try_reduction (q, NULL, &siga, BOUNDS_LIST, BOUNDS_LIST, COMMA_SYMBOL, BOUND, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, COMMA_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, ALT_FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, COLON_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, ALT_FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, DOTDOT_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, FORMAL_BOUNDS_LIST, ALT_FORMAL_BOUNDS_LIST, COMMA_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, strange_separator, &siga, BOUNDS_LIST, BOUNDS_LIST, BOUND, NULL_ATTRIBUTE);
  } while (siga);
}

/*!
\brief reduce argument packs
\param p position in tree
**/

static void reduce_arguments (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T siga;
    try_reduction (q, NULL, NULL, ARGUMENT_LIST, UNIT, NULL_ATTRIBUTE);
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, ARGUMENT_LIST, ARGUMENT_LIST, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, ARGUMENT_LIST, ARGUMENT_LIST, UNIT, NULL_ATTRIBUTE);
    } while (siga);
  }
}

/*!
\brief reduce declaration lists
\param p position in tree
**/

static void reduce_declaration_lists (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, IDENTITY_DECLARATION, DECLARER, DEFINING_IDENTIFIER, EQUALS_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, VARIABLE_DECLARATION, QUALIFIER, DECLARER, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, VARIABLE_DECLARATION, QUALIFIER, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, VARIABLE_DECLARATION, DECLARER, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, VARIABLE_DECLARATION, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, IDENTITY_DECLARATION, IDENTITY_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, VARIABLE_DECLARATION, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, NULL_ATTRIBUTE);
      if (!whether (q, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, NULL_ATTRIBUTE)) {
        try_reduction (q, NULL, &siga, VARIABLE_DECLARATION, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE);
      }
    } while (siga);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, OPERATOR_DECLARATION, OPERATOR_PLAN, DEFINING_OPERATOR, EQUALS_SYMBOL, UNIT, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, OPERATOR_DECLARATION, OPERATOR_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, UNIT, NULL_ATTRIBUTE);
    } while (siga);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, DECLARATION_LIST, MODE_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, PRIORITY_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, BRIEF_OPERATOR_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, OPERATOR_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, IDENTITY_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, PROCEDURE_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, PROCEDURE_VARIABLE_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, VARIABLE_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, ENVIRON_NAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, DECLARATION_LIST, DECLARATION_LIST, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
    } while (siga);
  }
}

/*!
\brief reduce labels and specifiers
\param p position in tree
**/

static void reduce_labels (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, LABELED_UNIT, LABEL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SPECIFIED_UNIT, SPECIFIER, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
  }
}

/*!
\brief signal badly used exits
\param p position in tree
**/

static void precheck_serial_clause (NODE_T * q)
{
  NODE_T *p;
  BOOL_T label_seen = A68_FALSE;
/* Wrong exits */
  for (p = q; p != NULL; FORWARD (p)) {
    if (WHETHER (p, EXIT_SYMBOL)) {
      if (NEXT (p) == NULL || WHETHER_NOT (NEXT (p), LABELED_UNIT)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_LABELED_UNIT_MUST_FOLLOW);
      }
    }
  }
/* Wrong jumps and declarations */
  for (p = q; p != NULL; FORWARD (p)) {
    if (WHETHER (p, LABELED_UNIT)) {
      label_seen = A68_TRUE;
    } else if (WHETHER (p, DECLARATION_LIST)) {
      if (label_seen) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_LABEL_BEFORE_DECLARATION);
      }
    }
  }
}

/*!
\brief reduce serial clauses
\param p position in tree
**/

static void reduce_serial_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T siga;
    precheck_serial_clause (p);
    try_reduction (q, NULL, NULL, SERIAL_CLAUSE, LABELED_UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SERIAL_CLAUSE, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INITIALISER_SERIES, DECLARATION_LIST, NULL_ATTRIBUTE);
    do {
      siga = A68_FALSE;
      if (WHETHER (q, SERIAL_CLAUSE)) {
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, SEMI_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, EXIT_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, SEMI_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, SEMI_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        /* Errors */
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COMMA_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COLON_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, COLON_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, DECLARATION_LIST, NULL_ATTRIBUTE);
      } else if (WHETHER (q, INITIALISER_SERIES)) {
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, INITIALISER_SERIES, INITIALISER_SERIES, SEMI_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        /* Errors */
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COLON_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, DECLARATION_LIST, NULL_ATTRIBUTE);
      }
    } 
    while (siga);
  }
}

/*!
\brief reduce enquiry clauses
\param p position in tree
**/

static void reduce_enquiry_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T siga;
    try_reduction (q, NULL, NULL, ENQUIRY_CLAUSE, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INITIALISER_SERIES, DECLARATION_LIST, NULL_ATTRIBUTE);
    do {
      siga = A68_FALSE;
      if (WHETHER (q, ENQUIRY_CLAUSE)) {
        try_reduction (q, NULL, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, SEMI_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, SEMI_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, COLON_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, DECLARATION_LIST, NULL_ATTRIBUTE);
      } else if (WHETHER (q, INITIALISER_SERIES)) {
        try_reduction (q, NULL, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, INITIALISER_SERIES, INITIALISER_SERIES, SEMI_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COLON_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, DECLARATION_LIST, NULL_ATTRIBUTE);
      }
    } 
    while (siga);
  }
}

/*!
\brief reduce collateral clauses
\param p position in tree
**/

static void reduce_collateral_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    if (WHETHER (q, UNIT)) {
      BOOL_T siga;
      try_reduction (q, NULL, NULL, UNIT_LIST, UNIT, NULL_ATTRIBUTE);
      do {
        siga = A68_FALSE;
        try_reduction (q, NULL, &siga, UNIT_LIST, UNIT_LIST, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, UNIT_LIST, UNIT_LIST, UNIT, NULL_ATTRIBUTE);
      } while (siga);
    } else if (WHETHER (q, SPECIFIED_UNIT)) {
      BOOL_T siga;
      try_reduction (q, NULL, NULL, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE);
      do {
        siga = A68_FALSE;
        try_reduction (q, NULL, &siga, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT_LIST, COMMA_SYMBOL, SPECIFIED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE);
      } while (siga);
    }
  }
}

/*!
\brief reduces clause parts, before reducing the clause itself
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_enclosed_clause_bits (NODE_T * p, int expect)
{
  if (SUB (p) != NULL) {
    return;
  }
  if (WHETHER (p, FOR_SYMBOL)) {
    try_reduction (p, NULL, NULL, FOR_PART, FOR_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE);
  } else if (WHETHER (p, OPEN_SYMBOL)) {
    if (expect == ENQUIRY_CLAUSE) {
      try_reduction (p, NULL, NULL, OPEN_PART, OPEN_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    } else if (expect == ARGUMENT) {
      try_reduction (p, NULL, NULL, ARGUMENT, OPEN_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ARGUMENT, OPEN_SYMBOL, ARGUMENT_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, empty_clause, NULL, ARGUMENT, OPEN_SYMBOL, INITIALISER_SERIES, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    } else if (expect == GENERIC_ARGUMENT) {
      if (whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE)) {
        pad_node (p, TRIMMER);
        try_reduction (p, NULL, NULL, GENERIC_ARGUMENT, OPEN_SYMBOL, TRIMMER, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      }
      try_reduction (p, NULL, NULL, GENERIC_ARGUMENT, OPEN_SYMBOL, GENERIC_ARGUMENT_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    } else if (expect == BOUNDS) {
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, BOUNDS, OPEN_SYMBOL, BOUNDS_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, FORMAL_BOUNDS_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, ALT_FORMAL_BOUNDS_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    } else {
      try_reduction (p, NULL, NULL, CLOSED_CLAUSE, OPEN_SYMBOL, SERIAL_CLAUSE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, COLLATERAL_CLAUSE, OPEN_SYMBOL, UNIT_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, COLLATERAL_CLAUSE, OPEN_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, empty_clause, NULL, CLOSED_CLAUSE, OPEN_SYMBOL, INITIALISER_SERIES, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    }
  } else if (WHETHER (p, SUB_SYMBOL)) {
    if (expect == GENERIC_ARGUMENT) {
      if (whether (p, SUB_SYMBOL, BUS_SYMBOL, NULL_ATTRIBUTE)) {
        pad_node (p, TRIMMER);
        try_reduction (p, NULL, NULL, GENERIC_ARGUMENT, SUB_SYMBOL, TRIMMER, BUS_SYMBOL, NULL_ATTRIBUTE);
      }
      try_reduction (p, NULL, NULL, GENERIC_ARGUMENT, SUB_SYMBOL, GENERIC_ARGUMENT_LIST, BUS_SYMBOL, NULL_ATTRIBUTE);
    } else if (expect == BOUNDS) {
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, BUS_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, BOUNDS, SUB_SYMBOL, BOUNDS_LIST, BUS_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, FORMAL_BOUNDS_LIST, BUS_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, ALT_FORMAL_BOUNDS_LIST, BUS_SYMBOL, NULL_ATTRIBUTE);
    }
  } else if (WHETHER (p, BEGIN_SYMBOL)) {
    try_reduction (p, NULL, NULL, COLLATERAL_CLAUSE, BEGIN_SYMBOL, UNIT_LIST, END_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, COLLATERAL_CLAUSE, BEGIN_SYMBOL, END_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CLOSED_CLAUSE, BEGIN_SYMBOL, SERIAL_CLAUSE, END_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, CLOSED_CLAUSE, BEGIN_SYMBOL, INITIALISER_SERIES, END_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL)) {
    try_reduction (p, NULL, NULL, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL, PICTURE_LIST, FORMAT_DELIMITER_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL, FORMAT_DELIMITER_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FORMAT_OPEN_SYMBOL)) {
    try_reduction (p, NULL, NULL, COLLECTION, FORMAT_OPEN_SYMBOL, PICTURE_LIST, FORMAT_CLOSE_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, CODE_SYMBOL)) {
    try_reduction (p, NULL, NULL, CODE_CLAUSE, CODE_SYMBOL, SERIAL_CLAUSE, EDOC_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, IF_SYMBOL)) {
    try_reduction (p, NULL, NULL, IF_PART, IF_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, IF_PART, IF_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, THEN_SYMBOL)) {
    try_reduction (p, NULL, NULL, THEN_PART, THEN_SYMBOL, SERIAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, THEN_PART, THEN_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELSE_SYMBOL)) {
    try_reduction (p, NULL, NULL, ELSE_PART, ELSE_SYMBOL, SERIAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, ELSE_PART, ELSE_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELIF_SYMBOL)) {
    try_reduction (p, NULL, NULL, ELIF_IF_PART, ELIF_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
  } else if (WHETHER (p, CASE_SYMBOL)) {
    try_reduction (p, NULL, NULL, CASE_PART, CASE_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, CASE_PART, CASE_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, IN_SYMBOL)) {
    try_reduction (p, NULL, NULL, INTEGER_IN_PART, IN_SYMBOL, UNIT_LIST, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_IN_PART, IN_SYMBOL, SPECIFIED_UNIT_LIST, NULL_ATTRIBUTE);
  } else if (WHETHER (p, OUT_SYMBOL)) {
    try_reduction (p, NULL, NULL, OUT_PART, OUT_SYMBOL, SERIAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, OUT_PART, OUT_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, OUSE_SYMBOL)) {
    try_reduction (p, NULL, NULL, OUSE_CASE_PART, OUSE_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
  } else if (WHETHER (p, THEN_BAR_SYMBOL)) {
    try_reduction (p, NULL, NULL, CHOICE, THEN_BAR_SYMBOL, SERIAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CHOICE_CLAUSE, THEN_BAR_SYMBOL, UNIT_LIST, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CHOICE, THEN_BAR_SYMBOL, SPECIFIED_UNIT_LIST, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CHOICE, THEN_BAR_SYMBOL, SPECIFIED_UNIT, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, CHOICE, THEN_BAR_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELSE_BAR_SYMBOL)) {
    try_reduction (p, NULL, NULL, ELSE_OPEN_PART, ELSE_BAR_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, ELSE_OPEN_PART, ELSE_BAR_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FROM_SYMBOL)) {
    try_reduction (p, NULL, NULL, FROM_PART, FROM_SYMBOL, UNIT, NULL_ATTRIBUTE);
  } else if (WHETHER (p, BY_SYMBOL)) {
    try_reduction (p, NULL, NULL, BY_PART, BY_SYMBOL, UNIT, NULL_ATTRIBUTE);
  } else if (WHETHER (p, TO_SYMBOL)) {
    try_reduction (p, NULL, NULL, TO_PART, TO_SYMBOL, UNIT, NULL_ATTRIBUTE);
  } else if (WHETHER (p, DOWNTO_SYMBOL)) {
    try_reduction (p, NULL, NULL, TO_PART, DOWNTO_SYMBOL, UNIT, NULL_ATTRIBUTE);
  } else if (WHETHER (p, WHILE_SYMBOL)) {
    try_reduction (p, NULL, NULL, WHILE_PART, WHILE_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, WHILE_PART, WHILE_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, UNTIL_SYMBOL)) {
    try_reduction (p, NULL, NULL, UNTIL_PART, UNTIL_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, UNTIL_PART, UNTIL_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, DO_SYMBOL)) {
    try_reduction (p, NULL, NULL, DO_PART, DO_SYMBOL, SERIAL_CLAUSE, UNTIL_PART, OD_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, DO_PART, DO_SYMBOL, SERIAL_CLAUSE, OD_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, DO_PART, DO_SYMBOL, UNTIL_PART, OD_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ALT_DO_SYMBOL)) {
    try_reduction (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, SERIAL_CLAUSE, UNTIL_PART, OD_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, SERIAL_CLAUSE, OD_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, UNTIL_PART, OD_SYMBOL, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce enclosed clauses
\param p position in tree
**/

static void reduce_enclosed_clauses (NODE_T * p)
{
  if (SUB (p) == NULL) {
    return;
  }
  if (WHETHER (p, OPEN_PART)) {
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, BRIEF_ELIF_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELSE_OPEN_PART)) {
    try_reduction (p, NULL, NULL, BRIEF_ELIF_PART, ELSE_OPEN_PART, CHOICE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_ELIF_PART, ELSE_OPEN_PART, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_ELIF_PART, ELSE_OPEN_PART, CHOICE, BRIEF_ELIF_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, IF_PART)) {
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, ELSE_PART, FI_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, ELIF_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, FI_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELIF_IF_PART)) {
    try_reduction (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, ELSE_PART, FI_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, FI_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, ELIF_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, CASE_PART)) {
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, OUT_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, INTEGER_OUT_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, OUT_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, UNITED_OUSE_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, OUSE_CASE_PART)) {
    try_reduction (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, OUT_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, INTEGER_OUT_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, OUT_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, UNITED_OUSE_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FOR_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FROM_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, BY_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, BY_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, BY_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, BY_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, TO_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, WHILE_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, DO_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, DO_PART, NULL_ATTRIBUTE);
  }
}

/*!
\brief substitute reduction when a phrase could not be parsed
\param p position in tree
\param expect information the parser may have on what is expected
\param suppress suppresses a diagnostic_node message (nested c.q. related diagnostics)
**/

static void recover_from_error (NODE_T * p, int expect, BOOL_T suppress)
{
/* This routine does not do fancy things as that might introduce more errors */
  NODE_T *q = p;
  if (p == NULL) {
    return;
  }
  if (expect == SOME_CLAUSE) {
    expect = serial_or_collateral (p);
  }
  if (!suppress) {
/* Give an error message */
    NODE_T *w = p;
    char *seq = phrase_to_text (p, &w);
    if (strlen (seq) == 0) {
      if (program.error_count == 0) {
        diagnostic_node (A68_SYNTAX_ERROR, w, ERROR_SYNTAX_EXPECTED, expect, NULL);
      }
    } else {
      diagnostic_node (A68_SYNTAX_ERROR, w, ERROR_INVALID_SEQUENCE, seq, expect, NULL);
    }
    if (program.error_count >= MAX_ERRORS) {
      longjmp (bottom_up_crash_exit, 1);
    }
  }
/* Try to prevent spurious diagnostics by guessing what was expected */
  while (NEXT (q) != NULL) {
    FORWARD (q);
  }
  if (whether_one_of (p, BEGIN_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE)) {
    if (expect == ARGUMENT || expect == COLLATERAL_CLAUSE || expect == PARAMETER_PACK || expect == STRUCTURE_PACK || expect == UNION_PACK) {
      make_sub (p, q, expect);
    } else if (expect == ENQUIRY_CLAUSE) {
      make_sub (p, q, OPEN_PART);
    } else if (expect == FORMAL_DECLARERS) {
      make_sub (p, q, FORMAL_DECLARERS);
    } else {
      make_sub (p, q, CLOSED_CLAUSE);
    }
  } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL) && expect == FORMAT_TEXT) {
    make_sub (p, q, FORMAT_TEXT);
  } else if (WHETHER (p, CODE_SYMBOL)) {
    make_sub (p, q, CODE_CLAUSE);
  } else if (whether_one_of (p, THEN_BAR_SYMBOL, CHOICE, NULL_ATTRIBUTE)) {
    make_sub (p, q, CHOICE);
  } else if (whether_one_of (p, IF_SYMBOL, IF_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, IF_PART);
  } else if (whether_one_of (p, THEN_SYMBOL, THEN_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, THEN_PART);
  } else if (whether_one_of (p, ELSE_SYMBOL, ELSE_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, ELSE_PART);
  } else if (whether_one_of (p, ELIF_SYMBOL, ELIF_IF_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, ELIF_IF_PART);
  } else if (whether_one_of (p, CASE_SYMBOL, CASE_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, CASE_PART);
  } else if (whether_one_of (p, OUT_SYMBOL, OUT_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, OUT_PART);
  } else if (whether_one_of (p, OUSE_SYMBOL, OUSE_CASE_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, OUSE_CASE_PART);
  } else if (whether_one_of (p, FOR_SYMBOL, FOR_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, FOR_PART);
  } else if (whether_one_of (p, FROM_SYMBOL, FROM_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, FROM_PART);
  } else if (whether_one_of (p, BY_SYMBOL, BY_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, BY_PART);
  } else if (whether_one_of (p, TO_SYMBOL, DOWNTO_SYMBOL, TO_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, TO_PART);
  } else if (whether_one_of (p, WHILE_SYMBOL, WHILE_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, WHILE_PART);
  } else if (whether_one_of (p, UNTIL_SYMBOL, UNTIL_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, UNTIL_PART);
  } else if (whether_one_of (p, DO_SYMBOL, DO_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, DO_PART);
  } else if (whether_one_of (p, ALT_DO_SYMBOL, ALT_DO_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, ALT_DO_PART);
  } else if (non_terminal_string (edit_line, expect) != NULL) {
    make_sub (p, q, expect);
  }
}

/*!
\brief heuristic aid in pinpointing errors
\param p position in tree
**/

static void reduce_erroneous_units (NODE_T * p)
{
/* Constructs are reduced to units in an attempt to limit spurious diagnostics */
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
/* Some implementations allow selection from a tertiary, when there is no risk
of ambiguity. Algol68G follows RR, so some extra attention here to guide an
unsuspecting user */
    if (whether (q, SELECTOR, -SECONDARY, NULL_ATTRIBUTE)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, SECONDARY);
      try_reduction (q, NULL, NULL, UNIT, SELECTOR, WILDCARD, NULL_ATTRIBUTE);
    }
/* Attention for identity relations that require tertiaries */
    if (whether (q, -TERTIARY, IS_SYMBOL, TERTIARY, NULL_ATTRIBUTE) || whether (q, TERTIARY, IS_SYMBOL, -TERTIARY, NULL_ATTRIBUTE) || whether (q, -TERTIARY, IS_SYMBOL, -TERTIARY, NULL_ATTRIBUTE)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, TERTIARY);
      try_reduction (q, NULL, NULL, UNIT, WILDCARD, IS_SYMBOL, WILDCARD, NULL_ATTRIBUTE);
    } else if (whether (q, -TERTIARY, ISNT_SYMBOL, TERTIARY, NULL_ATTRIBUTE) || whether (q, TERTIARY, ISNT_SYMBOL, -TERTIARY, NULL_ATTRIBUTE) || whether (q, -TERTIARY, ISNT_SYMBOL, -TERTIARY, NULL_ATTRIBUTE)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, TERTIARY);
      try_reduction (q, NULL, NULL, UNIT, WILDCARD, ISNT_SYMBOL, WILDCARD, NULL_ATTRIBUTE);
    }
  }
}

/*
Here is a set of routines that gather definitions from phrases.
This way we can apply tags before defining them.
These routines do not look very elegant as they have to scan through all
kind of symbols to find a pattern that they recognise.
*/

/*!
\brief skip anything until a comma, semicolon or EXIT is found
\param p position in tree
\return node from where to proceed
**/

static NODE_T *skip_unit (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, COMMA_SYMBOL)) {
      return (p);
    } else if (WHETHER (p, SEMI_SYMBOL)) {
      return (p);
    } else if (WHETHER (p, EXIT_SYMBOL)) {
      return (p);
    }
  }
  return (NULL);
}

/*!
\brief attribute of entry in symbol table
\param table current symbol table
\param name token name
\return attribute of entry in symbol table, or 0 if not found
**/

static int find_tag_definition (SYMBOL_TABLE_T * table, char *name)
{
  if (table != NULL) {
    int ret = 0;
    TAG_T *s;
    BOOL_T found;
    found = A68_FALSE;
    for (s = table->indicants; s != NULL && !found; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        ret += INDICANT;
        found = A68_TRUE;
      }
    }
    found = A68_FALSE;
    for (s = table->operators; s != NULL && !found; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        ret += OPERATOR;
        found = A68_TRUE;
      }
    }
    if (ret == 0) {
      return (find_tag_definition (PREVIOUS (table), name));
    } else {
      return (ret);
    }
  } else {
    return (0);
  }
}

/*!
\brief fill in whether bold tag is operator or indicant
\param p position in tree
**/

static void elaborate_bold_tags (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, BOLD_TAG)) {
      switch (find_tag_definition (SYMBOL_TABLE (q), SYMBOL (q))) {
      case 0:
        {
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_UNDECLARED_TAG);
          break;
        }
      case INDICANT:
        {
          ATTRIBUTE (q) = INDICANT;
          break;
        }
      case OPERATOR:
        {
          ATTRIBUTE (q) = OPERATOR;
          break;
        }
      }
    }
  }
}

/*!
\brief skip declarer, or argument pack and declarer
\param p position in tree
\return node before token that is not part of pack or declarer 
**/

static NODE_T *skip_pack_declarer (NODE_T * p)
{
/* Skip () REF [] REF FLEX [] [] .. */
  while (p != NULL && (whether_one_of (p, SUB_SYMBOL, OPEN_SYMBOL, REF_SYMBOL, FLEX_SYMBOL, SHORT_SYMBOL, LONG_SYMBOL, NULL_ATTRIBUTE))) {
    FORWARD (p);
  }
/* Skip STRUCT (), UNION () or PROC [()] */
  if (p != NULL && (whether_one_of (p, STRUCT_SYMBOL, UNION_SYMBOL, NULL_ATTRIBUTE))) {
    return (NEXT (p));
  } else if (p != NULL && WHETHER (p, PROC_SYMBOL)) {
    return (skip_pack_declarer (NEXT (p)));
  } else {
    return (p);
  }
}

/*!
\brief search MODE A = .., B = .. and store indicants
\param p position in tree
**/

static void extract_indicants (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER (q, MODE_SYMBOL)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        detect_redefined_keyword (q, MODE_DECLARATION);
        if (whether (q, BOLD_TAG, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
          ASSERT (add_tag (SYMBOL_TABLE (p), INDICANT, q, NULL, NULL_ATTRIBUTE) != NULL);
          ATTRIBUTE (q) = DEFINING_INDICANT;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          q = skip_pack_declarer (NEXT (q));
          FORWARD (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

#define GET_PRIORITY(q, k)\
  RESET_ERRNO;\
  (k) = atoi (SYMBOL (q));\
  if (errno != 0) {\
    diagnostic_node (A68_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY, NULL);\
    (k) = MAX_PRIORITY;\
  } else if ((k) < 1 || (k) > MAX_PRIORITY) {\
    diagnostic_node (A68_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY, NULL);\
    (k) = MAX_PRIORITY;\
  }

/*!
\brief search PRIO X = .., Y = .. and store priorities
\param p position in tree
**/

static void extract_priorities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER (q, PRIO_SYMBOL)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        detect_redefined_keyword (q, PRIORITY_DECLARATION);
/* An operator tag like ++ or && gives strange errors so we catch it here */
        if (whether (q, OPERATOR, OPERATOR, NULL_ATTRIBUTE)) {
          int k;
          NODE_T *y = q;
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG, NULL);
          ATTRIBUTE (q) = DEFINING_OPERATOR;
/* Remove one superfluous operator, and hope it was only one.   	 */
          NEXT (q) = NEXT_NEXT (q);
          PREVIOUS (NEXT (q)) = q;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k) != NULL);
          FORWARD (q);
        } else if (whether (q, OPERATOR, EQUALS_SYMBOL, INT_DENOTATION, NULL_ATTRIBUTE) || 
                   whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, INT_DENOTATION, NULL_ATTRIBUTE)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k) != NULL);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, IDENTIFIER, NULL_ATTRIBUTE)) {
          siga = A68_FALSE;
        } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, INT_DENOTATION, NULL_ATTRIBUTE)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k) != NULL);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, INT_DENOTATION, NULL_ATTRIBUTE) || 
                   whether (q, OPERATOR, INT_DENOTATION, NULL_ATTRIBUTE) || 
                   whether (q, EQUALS_SYMBOL, INT_DENOTATION, NULL_ATTRIBUTE)) {
/* The scanner cannot separate operator and "=" sign so we do this here */
          int len = (int) strlen (SYMBOL (q));
          if (len > 1 && SYMBOL (q)[len - 1] == '=') {
            int k;
            NODE_T *y = q;
            char *sym = (char *) get_temp_heap_space ((size_t) (len + 1));
            bufcpy (sym, SYMBOL (q), len + 1);
            sym[len - 1] = NULL_CHAR;
            SYMBOL (q) = TEXT (add_token (&top_token, sym));
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            insert_node (q, ALT_EQUALS_SYMBOL);
            q = NEXT_NEXT (q);
            GET_PRIORITY (q, k);
            ATTRIBUTE (q) = PRIORITY;
            ASSERT (add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k) != NULL);
            FORWARD (q);
          } else {
            siga = A68_FALSE;
          }
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search OP [( .. ) ..] X = .., Y = .. and store operators
\param p position in tree
**/

static void extract_operators (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER_NOT (q, OP_SYMBOL)) {
      FORWARD (q);
    } else {
      BOOL_T siga = A68_TRUE;
/* Skip operator plan */
      if (NEXT (q) != NULL && WHETHER (NEXT (q), OPEN_SYMBOL)) {
        q = skip_pack_declarer (NEXT (q));
      }
/* Sample operators */
      if (q != NULL) {
        do {
          FORWARD (q);
          detect_redefined_keyword (q, OPERATOR_DECLARATION);
/* An unacceptable operator tag like ++ or && gives strange errors so we catch it here */
          if (whether (q, OPERATOR, OPERATOR, NULL_ATTRIBUTE)) {
            diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG, NULL);
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, NULL_ATTRIBUTE) != NULL);
            NEXT (q) = NEXT_NEXT (q);   /* Remove one superfluous operator, and hope it was only one */
            PREVIOUS (NEXT (q)) = q;
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, OPERATOR, EQUALS_SYMBOL, NULL_ATTRIBUTE) || 
                     whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, NULL_ATTRIBUTE) != NULL);
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, BOLD_TAG, IDENTIFIER, NULL_ATTRIBUTE)) {
            siga = A68_FALSE;
          } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, NULL_ATTRIBUTE) != NULL);
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (q != NULL && (whether_one_of (q, OPERATOR, BOLD_TAG, EQUALS_SYMBOL, NULL_ATTRIBUTE))) {
/* The scanner cannot separate operator and "=" sign so we do this here */
            int len = (int) strlen (SYMBOL (q));
            if (len > 1 && SYMBOL (q)[len - 1] == '=') {
              char *sym = (char *) get_temp_heap_space ((size_t) (len + 1));
              bufcpy (sym, SYMBOL (q), len + 1);
              sym[len - 1] = NULL_CHAR;
              SYMBOL (q) = TEXT (add_token (&top_token, sym));
              ATTRIBUTE (q) = DEFINING_OPERATOR;
              insert_node (q, ALT_EQUALS_SYMBOL);
              ASSERT (add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, NULL_ATTRIBUTE) != NULL);
              FORWARD (q);
              q = skip_unit (q);
            } else {
              siga = A68_FALSE;
            }
          } else {
            siga = A68_FALSE;
          }
        } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
      }
    }
  }
}

/*!
\brief search and store labels
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void extract_labels (NODE_T * p, int expect)
{
  NODE_T *q;
/* Only handle candidate phrases as not to search indexers! */
  if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (whether (q, IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE)) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), LABEL, q, NULL, LOCAL_LABEL);
        ATTRIBUTE (q) = DEFINING_IDENTIFIER;
        z->unit = NULL;
      }
    }
  }
}

/*!
\brief search MOID x = .., y = .. and store identifiers
\param p position in tree
**/

static void extract_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, DECLARER, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
      BOOL_T siga = A68_TRUE;
      do {
        if (whether ((FORWARD (q)), IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, NULL_ATTRIBUTE)) {
/* Handle common error in ALGOL 68 programs */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search MOID x [:= ..], y [:= ..] and store identifiers
\param p position in tree
**/

static void extract_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, DECLARER, IDENTIFIER, NULL_ATTRIBUTE)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, NULL_ATTRIBUTE)) {
          if (whether (q, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
/* Handle common error in ALGOL 68 programs */
            diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
            ATTRIBUTE (NEXT (q)) = ASSIGN_SYMBOL;
          }
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search PROC x = .., y = .. and stores identifiers
\param p position in tree
**/

static void extract_proc_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
          TAG_T *t = add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          t->in_proc = A68_TRUE;
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, NULL_ATTRIBUTE)) {
/* Handle common error in ALGOL 68 programs */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search PROC x [:= ..], y [:= ..]; store identifiers
\param p position in tree
**/

static void extract_proc_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, NULL_ATTRIBUTE)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, NULL_ATTRIBUTE)) {
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (FORWARD (q));
        } else if (whether (q, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
/* Handle common error in ALGOL 68 programs */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ASSIGN_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief schedule gathering of definitions in a phrase
\param p position in tree
**/

static void extract_declarations (NODE_T * p)
{
  NODE_T *q;
/* Get definitions so we know what is defined in this range */
  extract_identities (p);
  extract_variables (p);
  extract_proc_identities (p);
  extract_proc_variables (p);
/* By now we know whether "=" is an operator or not */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = OPERATOR;
    } else if (WHETHER (q, ALT_EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = EQUALS_SYMBOL;
    }
  }
/* Get qualifiers */
  for (q = p; q != NULL; FORWARD (q)) {
    if (whether (q, LOC_SYMBOL, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, NEW_SYMBOL, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, LOC_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, NEW_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
  }
/* Give priorities to operators */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, OPERATOR)) {
      if (find_tag_global (SYMBOL_TABLE (q), OP_SYMBOL, SYMBOL (q))) {
        TAG_T *s = find_tag_global (SYMBOL_TABLE (q), PRIO_SYMBOL, SYMBOL (q));
        if (s != NULL) {
          PRIO (INFO (q)) = PRIO (s);
        } else {
          PRIO (INFO (q)) = 0;
        }
      } else {
        diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_UNDECLARED_TAG);
        PRIO (INFO (q)) = 1;
      }
    }
  }
}

/* A posteriori checks of the syntax tree built by the BU parser */

/*!
\brief count pictures
\param p position in tree
\param k counter
**/

static void count_pictures (NODE_T * p, int *k)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, PICTURE)) {
      (*k)++;
    }
    count_pictures (SUB (p), k);
  }
}

/*!
\brief driver for a posteriori error checking
\param p position in tree
**/

void bottom_up_error_check (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOOLEAN_PATTERN)) {
      int k = 0;
      count_pictures (SUB (p), &k);
      if (!(k == 0 || k == 2)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_FORMAT_PICTURE_NUMBER, ATTRIBUTE (p), NULL);
      }
    } else {
      bottom_up_error_check (SUB (p));
    }
  }
}

/* Next part rearranges the tree after the symbol tables are finished */

/*!
\brief transfer IDENTIFIER to JUMP where appropriate
\param p position in tree
**/

void rearrange_goto_less_jumps (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      NODE_T *q = SUB (p);
      if (WHETHER (q, TERTIARY)) {
        NODE_T *tertiary = q;
        q = SUB (q);
        if (q != NULL && WHETHER (q, SECONDARY)) {
          q = SUB (q);
          if (q != NULL && WHETHER (q, PRIMARY)) {
            q = SUB (q);
            if (q != NULL && WHETHER (q, IDENTIFIER)) {
              if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q)) == LABEL) {
                ATTRIBUTE (tertiary) = JUMP;
                SUB (tertiary) = q;
              }
            }
          }
        }
      }
    } else if (WHETHER (p, TERTIARY)) {
      NODE_T *q = SUB (p);
      if (q != NULL && WHETHER (q, SECONDARY)) {
        NODE_T *secondary = q;
        q = SUB (q);
        if (q != NULL && WHETHER (q, PRIMARY)) {
          q = SUB (q);
          if (q != NULL && WHETHER (q, IDENTIFIER)) {
            if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q)) == LABEL) {
              ATTRIBUTE (secondary) = JUMP;
              SUB (secondary) = q;
            }
          }
        }
      }
    } else if (WHETHER (p, SECONDARY)) {
      NODE_T *q = SUB (p);
      if (q != NULL && WHETHER (q, PRIMARY)) {
        NODE_T *primary = q;
        q = SUB (q);
        if (q != NULL && WHETHER (q, IDENTIFIER)) {
          if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q)) == LABEL) {
            ATTRIBUTE (primary) = JUMP;
            SUB (primary) = q;
          }
        }
      }
    } else if (WHETHER (p, PRIMARY)) {
      NODE_T *q = SUB (p);
      if (q != NULL && WHETHER (q, IDENTIFIER)) {
        if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q)) == LABEL) {
          make_sub (q, q, JUMP);
        }
      }
    }
    rearrange_goto_less_jumps (SUB (p));
  }
}

/* VICTAL CHECKER. Checks use of formal, actual and virtual declarers */

/*!
\brief check generator
\param p position in tree
**/

static void victal_check_generator (NODE_T * p)
{
  if (!victal_check_declarer (NEXT (p), ACTUAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
  }
}

/*!
\brief check formal pack
\param p position in tree
\param x expected attribute
\param z flag
**/

static void victal_check_formal_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, FORMAL_DECLARERS)) {
      victal_check_formal_pack (SUB (p), x, z);
    } else if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_formal_pack (NEXT (p), x, z);
    } else if (WHETHER (p, FORMAL_DECLARERS_LIST)) {
      victal_check_formal_pack (NEXT (p), x, z);
      victal_check_formal_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      victal_check_formal_pack (NEXT (p), x, z);
      (*z) &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check operator declaration
\param p position in tree
**/

static void victal_check_operator_dec (NODE_T * p)
{
  if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
    BOOL_T z = A68_TRUE;
    victal_check_formal_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarers");
    }
    FORWARD (p);
  }
  if (!victal_check_declarer (NEXT (p), FORMAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
  }
}

/*!
\brief check mode declaration
\param p position in tree
**/

static void victal_check_mode_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
      victal_check_mode_dec (NEXT (p));
    } else if (whether_one_of (p, MODE_SYMBOL, DEFINING_INDICANT, NULL_ATTRIBUTE)
               || whether_one_of (p, EQUALS_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_mode_dec (NEXT (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
    }
  }
}

/*!
\brief check variable declaration
\param p position in tree
**/

static void victal_check_variable_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
      victal_check_variable_dec (NEXT (p));
    } else if (whether_one_of (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, NULL_ATTRIBUTE)
               || WHETHER (p, COMMA_SYMBOL)) {
      victal_check_variable_dec (NEXT (p));
    } else if (WHETHER (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
      victal_check_variable_dec (NEXT (p));
    }
  }
}

/*!
\brief check identity declaration
\param p position in tree
**/

static void victal_check_identity_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
      victal_check_identity_dec (NEXT (p));
    } else if (whether_one_of (p, DEFINING_IDENTIFIER, EQUALS_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_identity_dec (NEXT (p));
    } else if (WHETHER (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      victal_check_identity_dec (NEXT (p));
    }
  }
}

/*!
\brief check routine pack
\param p position in tree
\param x expected attribute
\param z flag
**/

static void victal_check_routine_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, PARAMETER_PACK)) {
      victal_check_routine_pack (SUB (p), x, z);
    } else if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_routine_pack (NEXT (p), x, z);
    } else if (whether_one_of (p, PARAMETER_LIST, PARAMETER, NULL_ATTRIBUTE)) {
      victal_check_routine_pack (NEXT (p), x, z);
      victal_check_routine_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      *z &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check routine text
\param p position in tree
**/

static void victal_check_routine_text (NODE_T * p)
{
  if (WHETHER (p, PARAMETER_PACK)) {
    BOOL_T z = A68_TRUE;
    victal_check_routine_pack (p, FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarers");
    }
    FORWARD (p);
  }
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
  }
  victal_checker (NEXT (p));
}

/*!
\brief check structure pack
\param p position in tree
\param x expected attribute
\param z flag
**/

static void victal_check_structure_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, STRUCTURE_PACK)) {
      victal_check_structure_pack (SUB (p), x, z);
    } else if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_structure_pack (NEXT (p), x, z);
    } else if (whether_one_of (p, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, NULL_ATTRIBUTE)) {
      victal_check_structure_pack (NEXT (p), x, z);
      victal_check_structure_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      (*z) &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check union pack
\param p position in tree
\param x expected attribute
\param z flag
**/

static void victal_check_union_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, UNION_PACK)) {
      victal_check_union_pack (SUB (p), x, z);
    } else if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_union_pack (NEXT (p), x, z);
    } else if (WHETHER (p, UNION_DECLARER_LIST)) {
      victal_check_union_pack (NEXT (p), x, z);
      victal_check_union_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      victal_check_union_pack (NEXT (p), x, z);
      (*z) &= victal_check_declarer (SUB (p), FORMAL_DECLARER_MARK);
    }
  }
}

/*!
\brief check declarer
\param p position in tree
\param x expected attribute
**/

static BOOL_T victal_check_declarer (NODE_T * p, int x)
{
  if (p == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, DECLARER)) {
    return (victal_check_declarer (SUB (p), x));
  } else if (whether_one_of (p, LONGETY, SHORTETY, NULL_ATTRIBUTE)) {
    return (A68_TRUE);
  } else if (whether_one_of (p, VOID_SYMBOL, INDICANT, STANDARD, NULL_ATTRIBUTE)) {
    return (A68_TRUE);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (victal_check_declarer (NEXT (p), VIRTUAL_DECLARER_MARK));
  } else if (WHETHER (p, FLEX_SYMBOL)) {
    return (victal_check_declarer (NEXT (p), x));
  } else if (WHETHER (p, BOUNDS)) {
    victal_checker (SUB (p));
    if (x == FORMAL_DECLARER_MARK) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return (A68_TRUE);
    } else if (x == VIRTUAL_DECLARER_MARK) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "virtual bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return (A68_TRUE);
    } else {
      return (victal_check_declarer (NEXT (p), x));
    }
  } else if (WHETHER (p, FORMAL_BOUNDS)) {
    victal_checker (SUB (p));
    if (x == ACTUAL_DECLARER_MARK) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return (A68_TRUE);
    } else {
      return (victal_check_declarer (NEXT (p), x));
    }
  } else if (WHETHER (p, STRUCT_SYMBOL)) {
    BOOL_T z = A68_TRUE;
    victal_check_structure_pack (NEXT (p), x, &z);
    return (z);
  } else if (WHETHER (p, UNION_SYMBOL)) {
    BOOL_T z = A68_TRUE;
    victal_check_union_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer pack");
    }
    return (A68_TRUE);
  } else if (WHETHER (p, PROC_SYMBOL)) {
    if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
      BOOL_T z = A68_TRUE;
      victal_check_formal_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
      if (!z) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      FORWARD (p);
    }
    if (!victal_check_declarer (NEXT (p), FORMAL_DECLARER_MARK)) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    }
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief check cast
\param p position in tree
**/

static void victal_check_cast (NODE_T * p)
{
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    victal_checker (NEXT (p));
  }
}

/*!
\brief driver for checking VICTALITY of declarers
\param p position in tree
**/

void victal_checker (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
    } else if (WHETHER (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
    } else if (WHETHER (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
    } else if (WHETHER (p, GENERATOR)) {
      victal_check_generator (SUB (p));
    } else if (WHETHER (p, ROUTINE_TEXT)) {
      victal_check_routine_text (SUB (p));
    } else if (WHETHER (p, OPERATOR_PLAN)) {
      victal_check_operator_dec (SUB (p));
    } else if (WHETHER (p, CAST)) {
      victal_check_cast (SUB (p));
    } else {
      victal_checker (SUB (p));
    }
  }
}

/*
\brief set level for procedures
\param p position in tree
\param n proc level number
*/

void set_proc_level (NODE_T * p, int n)
{
  for (; p != NULL; FORWARD (p)) {
    INFO (p)->PROCEDURE_LEVEL = n;
    if (WHETHER (p, ROUTINE_TEXT)) {
      set_proc_level (SUB (p), n + 1);
    } else {
      set_proc_level (SUB (p), n);
    }
  }
}

/*
\brief set nests for diagnostics
\param p position in tree
\param s start of enclosing nest
*/

void set_nest (NODE_T * p, NODE_T * s)
{
  for (; p != NULL; FORWARD (p)) {
    NEST (p) = s;
    if (WHETHER (p, PARTICULAR_PROGRAM)) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, CLOSED_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, COLLATERAL_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, CONDITIONAL_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, INTEGER_CASE_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, UNITED_CASE_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, LOOP_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else {
      set_nest (SUB (p), s);
    }
  }
}

/* Routines that work with tags and symbol tables */

static void tax_tags (NODE_T *);
static void tax_specifier_list (NODE_T *);
static void tax_parameter_list (NODE_T *);
static void tax_format_texts (NODE_T *);

/*!
\brief find a tag, searching symbol tables towards the root
\param table symbol table to search
\param a attribute of tag
\param name name of tag
\return type of tag, identifier or label or ...
**/

int first_tag_global (SYMBOL_TABLE_T * table, char *name)
{
  if (table != NULL) {
    TAG_T *s = NULL;
    for (s = table->identifiers; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (IDENTIFIER);
      }
    }
    for (s = table->indicants; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (INDICANT);
      }
    }
    for (s = table->labels; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (LABEL);
      }
    }
    for (s = table->operators; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (OP_SYMBOL);
      }
    }
    for (s = PRIO (table); s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (PRIO_SYMBOL);
      }
    }
    return (first_tag_global (PREVIOUS (table), name));
  } else {
    return (NULL_ATTRIBUTE);
  }
}

#define PORTCHECK_TAX(p, q) {\
  if (q == A68_FALSE) {\
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_TAG_NOT_PORTABLE, NULL);\
  }}

/*!
\brief check portability of sub tree
\param p position in tree
**/

void portcheck (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    portcheck (SUB (p));
    if (program.options.portcheck) {
      if (WHETHER (p, INDICANT) && MOID (p) != NULL) {
        PORTCHECK_TAX (p, MOID (p)->portable);
        MOID (p)->portable = A68_TRUE;
      } else if (WHETHER (p, IDENTIFIER)) {
        PORTCHECK_TAX (p, TAX (p)->portable);
        TAX (p)->portable = A68_TRUE;
      } else if (WHETHER (p, OPERATOR)) {
        PORTCHECK_TAX (p, TAX (p)->portable);
        TAX (p)->portable = A68_TRUE;
      }
    }
  }
}

/*!
\brief whether routine can be "lengthety-mapped"
\param z name of routine
\return same
**/

static BOOL_T whether_mappable_routine (char *z)
{
#define ACCEPT(u, v) {\
  if (strlen (u) >= strlen (v)) {\
    if (strcmp (&u[strlen (u) - strlen (v)], v) == 0) {\
      return (A68_TRUE);\
  }}}
/* Math routines */
  ACCEPT (z, "arccos");
  ACCEPT (z, "arcsin");
  ACCEPT (z, "arctan");
  ACCEPT (z, "cbrt");
  ACCEPT (z, "cos");
  ACCEPT (z, "curt");
  ACCEPT (z, "exp");
  ACCEPT (z, "ln");
  ACCEPT (z, "log");
  ACCEPT (z, "pi");
  ACCEPT (z, "sin");
  ACCEPT (z, "sqrt");
  ACCEPT (z, "tan");
/* Random generator */
  ACCEPT (z, "nextrandom");
  ACCEPT (z, "random");
/* BITS */
  ACCEPT (z, "bitspack");
/* Enquiries */
  ACCEPT (z, "maxint");
  ACCEPT (z, "intwidth");
  ACCEPT (z, "maxreal");
  ACCEPT (z, "realwidth");
  ACCEPT (z, "expwidth");
  ACCEPT (z, "maxbits");
  ACCEPT (z, "bitswidth");
  ACCEPT (z, "byteswidth");
  ACCEPT (z, "smallreal");
  return (A68_FALSE);
#undef ACCEPT
}

/*!
\brief map "short sqrt" onto "sqrt" etcetera
\param u name of routine
\return tag to map onto
**/

static TAG_T *bind_lengthety_identifier (char *u)
{
#define CAR(u, v) (strncmp (u, v, strlen(v)) == 0)
/*
We can only map routines blessed by "whether_mappable_routine", so there is no
"short print" or "long char in string".
*/
  if (CAR (u, "short")) {
    do {
      char *v;
      TAG_T *w;
      u = &u[strlen ("short")];
      v = TEXT (add_token (&top_token, u));
      w = find_tag_local (stand_env, IDENTIFIER, v);
      if (w != NULL && whether_mappable_routine (v)) {
        return (w);
      }
    } while (CAR (u, "short"));
  } else if (CAR (u, "long")) {
    do {
      char *v;
      TAG_T *w;
      u = &u[strlen ("long")];
      v = TEXT (add_token (&top_token, u));
      w = find_tag_local (stand_env, IDENTIFIER, v);
      if (w != NULL && whether_mappable_routine (v)) {
        return (w);
      }
    } while (CAR (u, "long"));
  }
  return (NULL);
#undef CAR
}

/*!
\brief bind identifier tags to the symbol table
\param p position in tree
**/

static void bind_identifier_tag_to_symbol_table (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    bind_identifier_tag_to_symbol_table (SUB (p));
    if (whether_one_of (p, IDENTIFIER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      int att = first_tag_global (SYMBOL_TABLE (p), SYMBOL (p));
      if (att != NULL_ATTRIBUTE) {
        TAG_T *z = find_tag_global (SYMBOL_TABLE (p), att, SYMBOL (p));
        if (att == IDENTIFIER && z != NULL) {
          MOID (p) = MOID (z);
        } else if (att == LABEL && z != NULL) {
          ;
        } else if ((z = bind_lengthety_identifier (SYMBOL (p))) != NULL) {
          MOID (p) = MOID (z);
        } else {
          diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          z = add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
          MOID (p) = MODE (ERROR);
        }
        TAX (p) = z;
        if (WHETHER (p, DEFINING_IDENTIFIER)) {
          NODE (z) = p;
        }
      }
    }
  }
}

/*!
\brief bind indicant tags to the symbol table
\param p position in tree
**/

static void bind_indicant_tag_to_symbol_table (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    bind_indicant_tag_to_symbol_table (SUB (p));
    if (whether_one_of (p, INDICANT, DEFINING_INDICANT, NULL_ATTRIBUTE)) {
      TAG_T *z = find_tag_global (SYMBOL_TABLE (p), INDICANT, SYMBOL (p));
      if (z != NULL) {
        MOID (p) = MOID (z);
        TAX (p) = z;
        if (WHETHER (p, DEFINING_INDICANT)) {
          NODE (z) = p;
        }
      }
    }
  }
}

/*!
\brief enter specifier identifiers in the symbol table
\param p position in tree
**/

static void tax_specifiers (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_specifiers (SUB (p));
    if (SUB (p) != NULL && WHETHER (p, SPECIFIER)) {
      tax_specifier_list (SUB (p));
    }
  }
}

/*!
\brief enter specifier identifiers in the symbol table
\param p position in tree
**/

static void tax_specifier_list (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, OPEN_SYMBOL)) {
      tax_specifier_list (NEXT (p));
    } else if (whether_one_of (p, CLOSE_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else if (WHETHER (p, IDENTIFIER)) {
      TAG_T *z = add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, NULL, SPECIFIER_IDENTIFIER);
      HEAP (z) = LOC_SYMBOL;
    } else if (WHETHER (p, DECLARER)) {
      tax_specifiers (SUB (p));
      tax_specifier_list (NEXT (p));
/* last identifier entry is identifier with this declarer */
      if (SYMBOL_TABLE (p)->identifiers != NULL && PRIO (SYMBOL_TABLE (p)->identifiers) == SPECIFIER_IDENTIFIER)
        MOID (SYMBOL_TABLE (p)->identifiers) = MOID (p);
    }
  }
}

/*!
\brief enter parameter identifiers in the symbol table
\param p position in tree
**/

static void tax_parameters (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL) {
      tax_parameters (SUB (p));
      if (WHETHER (p, PARAMETER_PACK)) {
        tax_parameter_list (SUB (p));
      }
    }
  }
}

/*!
\brief enter parameter identifiers in the symbol table
\param p position in tree
**/

static void tax_parameter_list (NODE_T * p)
{
  if (p != NULL) {
    if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_parameter_list (NEXT (p));
    } else if (WHETHER (p, CLOSE_SYMBOL)) {
      ;
    } else if (whether_one_of (p, PARAMETER_LIST, PARAMETER, NULL_ATTRIBUTE)) {
      tax_parameter_list (NEXT (p));
      tax_parameter_list (SUB (p));
    } else if (WHETHER (p, IDENTIFIER)) {
/* parameters are always local */
      HEAP (add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, NULL, PARAMETER_IDENTIFIER)) = LOC_SYMBOL;
    } else if (WHETHER (p, DECLARER)) {
      TAG_T *s;
      tax_parameter_list (NEXT (p));
/* last identifier entries are identifiers with this declarer */
      for (s = SYMBOL_TABLE (p)->identifiers; s != NULL && MOID (s) == NULL; FORWARD (s)) {
        MOID (s) = MOID (p);
      }
      tax_parameters (SUB (p));
    }
  }
}

/*!
\brief enter FOR identifiers in the symbol table
\param p position in tree
**/

static void tax_for_identifiers (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_for_identifiers (SUB (p));
    if (WHETHER (p, FOR_SYMBOL)) {
      if ((FORWARD (p)) != NULL) {
        (void) add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (INT), LOOP_IDENTIFIER);
      }
    }
  }
}

/*!
\brief enter routine texts in the symbol table
\param p position in tree
**/

static void tax_routine_texts (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_routine_texts (SUB (p));
    if (WHETHER (p, ROUTINE_TEXT)) {
      TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, MOID (p), ROUTINE_TEXT);
      TAX (p) = z;
      HEAP (z) = LOC_SYMBOL;
      USE (z) = A68_TRUE;
    }
  }
}

/*!
\brief enter format texts in the symbol table
\param p position in tree
**/

static void tax_format_texts (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_format_texts (SUB (p));
    if (WHETHER (p, FORMAT_TEXT)) {
      TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, MODE (FORMAT), FORMAT_TEXT);
      TAX (p) = z;
      USE (z) = A68_TRUE;
    } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL) && NEXT (p) != NULL) {
      TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, MODE (FORMAT), FORMAT_IDENTIFIER);
      TAX (p) = z;
      USE (z) = A68_TRUE;
    }
  }
}

/*!
\brief enter FORMAT pictures in the symbol table
\param p position in tree
**/

static void tax_pictures (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_pictures (SUB (p));
    if (WHETHER (p, PICTURE)) {
      TAX (p) = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, MODE (COLLITEM), FORMAT_IDENTIFIER);
    }
  }
}

/*!
\brief enter generators in the symbol table
\param p position in tree
**/

static void tax_generators (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    tax_generators (SUB (p));
    if (WHETHER (p, GENERATOR)) {
      if (WHETHER (SUB (p), LOC_SYMBOL)) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, SUB_MOID (SUB (p)), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        TAX (p) = z;
      }
    }
  }
}

/*!
\brief consistency check on fields in structured modes
\param p position in tree
**/

static void structure_fields_test (NODE_T * p)
{
/* STRUCT (REAL x, INT n, REAL x) is wrong */
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m = SYMBOL_TABLE (SUB (p))->moids;
      for (; m != NULL; FORWARD (m)) {
        if (WHETHER (m, STRUCT_SYMBOL) && m->equivalent_mode == NULL) {
/* check on identically named fields */
          PACK_T *s = PACK (m);
          for (; s != NULL; FORWARD (s)) {
            PACK_T *t = NEXT (s);
            BOOL_T k = A68_TRUE;
            for (t = NEXT (s); t != NULL && k; FORWARD (t)) {
              if (TEXT (s) == TEXT (t)) {
                diagnostic_node (A68_ERROR, p, ERROR_MULTIPLE_FIELD);
                while (NEXT (s) != NULL && TEXT (NEXT (s)) == TEXT (t)) {
                  FORWARD (s);
                }
                k = A68_FALSE;
              }
            }
          }
        }
      }
    }
    structure_fields_test (SUB (p));
  }
}

/*!
\brief incestuous union test
\param p position in tree
**/

static void incestuous_union_test (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *symbol_table = SYMBOL_TABLE (SUB (p));
      MOID_T *m = symbol_table->moids;
      for (; m != NULL; FORWARD (m)) {
        if (WHETHER (m, UNION_SYMBOL) && m->equivalent_mode == NULL) {
          PACK_T *s = PACK (m);
          BOOL_T x = A68_TRUE;
/* Discard unions with one member */
          if (count_pack_members (s) == 1) {
            SOID_T a;
            make_soid (&a, NO_SORT, m, 0);
            diagnostic_node (A68_ERROR, NODE (m), ERROR_COMPONENT_NUMBER, m);
            x = A68_FALSE;
          }
/* Discard unions with firmly related modes */
          for (; s != NULL && x; FORWARD (s)) {
            PACK_T *t = NEXT (s);
            for (; t != NULL; FORWARD (t)) {
              if (MOID (t) != MOID (s)) {
                if (whether_firm (MOID (s), MOID (t))) {
                  diagnostic_node (A68_ERROR, p, ERROR_COMPONENT_RELATED, m);
                }
              }
            }
          }
/* Discard unions with firmly related subsets */
          for (s = PACK (m); s != NULL && x; FORWARD (s)) {
            MOID_T *n = depref_completely (MOID (s));
            if (WHETHER (n, UNION_SYMBOL)
                && whether_subset (n, m, NO_DEFLEXING)) {
              SOID_T z;
              make_soid (&z, NO_SORT, n, 0);
              diagnostic_node (A68_ERROR, p, ERROR_SUBSET_RELATED, m, n);
            }
          }
        }
      }
    }
    incestuous_union_test (SUB (p));
  }
}

/*!
\brief find a firmly related operator for operands
\param c symbol table
\param n name of operator
\param l left operand mode
\param r right operand mode
\param self own tag of "n", as to not relate to itself
\return pointer to entry in table
**/

static TAG_T *find_firmly_related_op (SYMBOL_TABLE_T * c, char *n, MOID_T * l, MOID_T * r, TAG_T * self)
{
  if (c != NULL) {
    TAG_T *s = c->operators;
    for (; s != NULL; FORWARD (s)) {
      if (s != self && SYMBOL (NODE (s)) == n) {
        PACK_T *t = PACK (MOID (s));
        if (t != NULL && whether_firm (MOID (t), l)) {
/* catch monadic operator */
          if ((FORWARD (t)) == NULL) {
            if (r == NULL) {
              return (s);
            }
          } else {
/* catch dyadic operator */
            if (r != NULL && whether_firm (MOID (t), r)) {
              return (s);
            }
          }
        }
      }
    }
  }
  return (NULL);
}

/*!
\brief check for firmly related operators in this range
\param p position in tree
\param s operator tag to start from
**/

static void test_firmly_related_ops_local (NODE_T * p, TAG_T * s)
{
  if (s != NULL) {
    PACK_T *u = PACK (MOID (s));
    MOID_T *l = MOID (u);
    MOID_T *r = NEXT (u) != NULL ? MOID (NEXT (u)) : NULL;
    TAG_T *t = find_firmly_related_op (TAG_TABLE (s), SYMBOL (NODE (s)), l, r, s);
    if (t != NULL) {
      if (TAG_TABLE (t) == stand_env) {
        diagnostic_node (A68_ERROR, p, ERROR_OPERATOR_RELATED, MOID (s), SYMBOL (NODE (s)), MOID (t), SYMBOL (NODE (t)), NULL);
        ABEND (A68_TRUE, "standard environ error", NULL);
      } else {
        diagnostic_node (A68_ERROR, p, ERROR_OPERATOR_RELATED, MOID (s), SYMBOL (NODE (s)), MOID (t), SYMBOL (NODE (t)), NULL);
      }
    }
    if (NEXT (s) != NULL) {
      test_firmly_related_ops_local (p == NULL ? NULL : NODE (NEXT (s)), NEXT (s));
    }
  }
}

/*!
\brief find firmly related operators in this program
\param p position in tree
**/

static void test_firmly_related_ops (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      TAG_T *oops = SYMBOL_TABLE (SUB (p))->operators;
      if (oops != NULL) {
        test_firmly_related_ops_local (NODE (oops), oops);
      }
    }
    test_firmly_related_ops (SUB (p));
  }
}

/*!
\brief driver for the processing of TAXes
\param p position in tree
**/

void collect_taxes (NODE_T * p)
{
  tax_tags (p);
  tax_specifiers (p);
  tax_parameters (p);
  tax_for_identifiers (p);
  tax_routine_texts (p);
  tax_pictures (p);
  tax_format_texts (p);
  tax_generators (p);
  bind_identifier_tag_to_symbol_table (p);
  bind_indicant_tag_to_symbol_table (p);
  structure_fields_test (p);
  incestuous_union_test (p);
  test_firmly_related_ops (p);
  test_firmly_related_ops_local (NULL, stand_env->operators);
}

/*!
\brief whether tag has already been declared in this range
\param n name of tag
\param a attribute of tag
**/

static void already_declared (NODE_T * n, int a)
{
  if (find_tag_local (SYMBOL_TABLE (n), a, SYMBOL (n)) != NULL) {
    diagnostic_node (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
}

/*!
\brief whether tag has already been declared in this range
\param n name of tag
\param a attribute of tag
**/

static void already_declared_hidden (NODE_T * n, int a)
{
  TAG_T *s;
  if (find_tag_local (SYMBOL_TABLE (n), a, SYMBOL (n)) != NULL) {
    diagnostic_node (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
  if ((s = find_tag_global (PREVIOUS (SYMBOL_TABLE (n)), a, SYMBOL (n))) != NULL) {
    if (TAG_TABLE (s) == stand_env) {
      diagnostic_node (A68_WARNING, n, WARNING_HIDES_PRELUDE, MOID (s), SYMBOL (n));
    } else {
      diagnostic_node (A68_WARNING, n, WARNING_HIDES, SYMBOL (n));
    }
  }
}

/*!
\brief add tag to local symbol table
\param s table where to insert
\param a attribute
\param n name of tag
\param m mode of tag
\param p position in tree
\return entry in symbol table
**/

TAG_T *add_tag (SYMBOL_TABLE_T * s, int a, NODE_T * n, MOID_T * m, int p)
{
#define INSERT_TAG(l, n) {NEXT (n) = *(l); *(l) = (n);}
  if (s != NULL) {
    TAG_T *z = new_tag ();
    TAG_TABLE (z) = s;
    PRIO (z) = p;
    MOID (z) = m;
    NODE (z) = n;
/*    TAX(n) = z; */
    switch (a) {
    case IDENTIFIER:{
        already_declared_hidden (n, IDENTIFIER);
        already_declared_hidden (n, LABEL);
        INSERT_TAG (&s->identifiers, z);
        break;
      }
    case INDICANT:{
        already_declared_hidden (n, INDICANT);
        already_declared (n, OP_SYMBOL);
        already_declared (n, PRIO_SYMBOL);
        INSERT_TAG (&s->indicants, z);
        break;
      }
    case LABEL:{
        already_declared_hidden (n, LABEL);
        already_declared_hidden (n, IDENTIFIER);
        INSERT_TAG (&s->labels, z);
        break;
      }
    case OP_SYMBOL:{
        already_declared (n, INDICANT);
        INSERT_TAG (&s->operators, z);
        break;
      }
    case PRIO_SYMBOL:{
        already_declared (n, PRIO_SYMBOL);
        already_declared (n, INDICANT);
        INSERT_TAG (&PRIO (s), z);
        break;
      }
    case ANONYMOUS:{
        INSERT_TAG (&s->anonymous, z);
        break;
      }
    default:{
        ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "add tag");
      }
    }
    return (z);
  } else {
    return (NULL);
  }
#undef INSERT_TAG
}

/*!
\brief find a tag, searching symbol tables towards the root
\param table symbol table to search
\param a attribute of tag
\param name name of tag
\return entry in symbol table
**/

TAG_T *find_tag_global (SYMBOL_TABLE_T * table, int a, char *name)
{
  if (table != NULL) {
    TAG_T *s = NULL;
    switch (a) {
    case IDENTIFIER:{
        s = table->identifiers;
        break;
      }
    case INDICANT:{
        s = table->indicants;
        break;
      }
    case LABEL:{
        s = table->labels;
        break;
      }
    case OP_SYMBOL:{
        s = table->operators;
        break;
      }
    case PRIO_SYMBOL:{
        s = PRIO (table);
        break;
      }
    default:{
        ABEND (A68_TRUE, "impossible state in find_tag_global", NULL);
        break;
      }
    }
    for (; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (s);
      }
    }
    return (find_tag_global (PREVIOUS (table), a, name));
  } else {
    return (NULL);
  }
}

/*!
\brief whether identifier or label global
\param table symbol table to search
\param name name of tag
\return attribute of tag
**/

int whether_identifier_or_label_global (SYMBOL_TABLE_T * table, char *name)
{
  if (table != NULL) {
    TAG_T *s;
    for (s = table->identifiers; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (IDENTIFIER);
      }
    }
    for (s = table->labels; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (LABEL);
      }
    }
    return (whether_identifier_or_label_global (PREVIOUS (table), name));
  } else {
    return (0);
  }
}

/*!
\brief find a tag, searching only local symbol table
\param table symbol table to search
\param a attribute of tag
\param name name of tag
\return entry in symbol table
**/

TAG_T *find_tag_local (SYMBOL_TABLE_T * table, int a, char *name)
{
  if (table != NULL) {
    TAG_T *s = NULL;
    if (a == OP_SYMBOL) {
      s = table->operators;
    } else if (a == PRIO_SYMBOL) {
      s = PRIO (table);
    } else if (a == IDENTIFIER) {
      s = table->identifiers;
    } else if (a == INDICANT) {
      s = table->indicants;
    } else if (a == LABEL) {
      s = table->labels;
    } else {
      ABEND (A68_TRUE, "impossible state in find_tag_local", NULL);
    }
    for (; s != NULL; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        return (s);
      }
    }
  }
  return (NULL);
}

/*!
\brief whether context specifies HEAP or LOC for an identifier
\param p position in tree
\return attribute of generator
**/

static int tab_qualifier (NODE_T * p)
{
  if (p != NULL) {
    if (whether_one_of (p, UNIT, ASSIGNATION, TERTIARY, SECONDARY, GENERATOR, NULL_ATTRIBUTE)) {
      return (tab_qualifier (SUB (p)));
    } else if (whether_one_of (p, LOC_SYMBOL, HEAP_SYMBOL, NEW_SYMBOL, NULL_ATTRIBUTE)) {
      return (ATTRIBUTE (p) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
    } else {
      return (LOC_SYMBOL);
    }
  } else {
    return (LOC_SYMBOL);
  }
}

/*!
\brief enter identity declarations in the symbol table
\param p position in tree
\param m mode of identifiers to enter (picked from the left-most one in fi. INT i = 1, j = 2)
**/

static void tax_identity_dec (NODE_T * p, MOID_T ** m)
{
  if (p != NULL) {
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      tax_identity_dec (SUB (p), m);
      tax_identity_dec (NEXT (p), m);
    } else if (WHETHER (p, DECLARER)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_identity_dec (NEXT (p), m);
    } else if (WHETHER (p, COMMA_SYMBOL)) {
      tax_identity_dec (NEXT (p), m);
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (SYMBOL_TABLE (p), IDENTIFIER, SYMBOL (p));
      MOID (p) = *m;
      HEAP (entry) = LOC_SYMBOL;
      TAX (p) = entry;
      MOID (entry) = *m;
      if ((*m)->attribute == REF_SYMBOL) {
        HEAP (entry) = tab_qualifier (NEXT_NEXT (p));
      }
      tax_identity_dec (NEXT_NEXT (p), m);
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter variable declarations in the symbol table
\param p position in tree
\param q qualifier of generator (HEAP, LOC) picked from left-most identifier
\param m mode of identifiers to enter (picked from the left-most one in fi. INT i, j, k)
**/

static void tax_variable_dec (NODE_T * p, int *q, MOID_T ** m)
{
  if (p != NULL) {
    if (WHETHER (p, VARIABLE_DECLARATION)) {
      tax_variable_dec (SUB (p), q, m);
      tax_variable_dec (NEXT (p), q, m);
    } else if (WHETHER (p, DECLARER)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_variable_dec (NEXT (p), q, m);
    } else if (WHETHER (p, QUALIFIER)) {
      *q = ATTRIBUTE (SUB (p));
      tax_variable_dec (NEXT (p), q, m);
    } else if (WHETHER (p, COMMA_SYMBOL)) {
      tax_variable_dec (NEXT (p), q, m);
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (SYMBOL_TABLE (p), IDENTIFIER, SYMBOL (p));
      MOID (p) = *m;
      TAX (p) = entry;
      HEAP (entry) = *q;
      if (*q == LOC_SYMBOL) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, SUB (*m), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        BODY (entry) = z;
      } else {
        BODY (entry) = NULL;
      }
      MOID (entry) = *m;
      tax_variable_dec (NEXT (p), q, m);
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter procedure variable declarations in the symbol table
\param p position in tree
\param q qualifier of generator (HEAP, LOC) picked from left-most identifier
**/

static void tax_proc_variable_dec (NODE_T * p, int *q)
{
  if (p != NULL) {
    if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      tax_proc_variable_dec (SUB (p), q);
      tax_proc_variable_dec (NEXT (p), q);
    } else if (WHETHER (p, QUALIFIER)) {
      *q = ATTRIBUTE (SUB (p));
      tax_proc_variable_dec (NEXT (p), q);
    } else if (whether_one_of (p, PROC_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_proc_variable_dec (NEXT (p), q);
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (SYMBOL_TABLE (p), IDENTIFIER, SYMBOL (p));
      TAX (p) = entry;
      HEAP (entry) = *q;
      MOID (entry) = MOID (p);
      if (*q == LOC_SYMBOL) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), ANONYMOUS, p, SUB_MOID (p), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        BODY (entry) = z;
      } else {
        BODY (entry) = NULL;
      }
      tax_proc_variable_dec (NEXT (p), q);
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter procedure declarations in the symbol table
\param p position in tree
**/

static void tax_proc_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, PROCEDURE_DECLARATION)) {
      tax_proc_dec (SUB (p));
      tax_proc_dec (NEXT (p));
    } else if (whether_one_of (p, PROC_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_proc_dec (NEXT (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (SYMBOL_TABLE (p), IDENTIFIER, SYMBOL (p));
      MOID_T *m = MOID (NEXT_NEXT (p));
      MOID (p) = m;
      TAX (p) = entry;
      CODEX (entry) |= PROC_DECLARATION_MASK;
      HEAP (entry) = LOC_SYMBOL;
      MOID (entry) = m;
      tax_proc_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief count number of operands in operator parameter list
\param p position in tree
\return same
**/

static int count_operands (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, DECLARER)) {
      return (count_operands (NEXT (p)));
    } else if (WHETHER (p, COMMA_SYMBOL)) {
      return (1 + count_operands (NEXT (p)));
    } else {
      return (count_operands (NEXT (p)) + count_operands (SUB (p)));
    }
  } else {
    return (0);
  }
}

/*!
\brief check validity of operator declaration
\param p position in tree
**/

static void check_operator_dec (NODE_T * p)
{
  int k = 0;
  NODE_T *pack = SUB_SUB (NEXT_NEXT (p));     /* That's where the parameter pack is */
  if (ATTRIBUTE (NEXT_NEXT (p)) != ROUTINE_TEXT) {
    pack = SUB (pack);
  }
  k = 1 + count_operands (pack);
  if (k < 1 && k > 2) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERAND_NUMBER);
    k = 0;
  }
  if (k == 1 && a68g_strchr (NOMADS, SYMBOL (p)[0]) != NULL) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
  } else if (k == 2 && !find_tag_global (SYMBOL_TABLE (p), PRIO_SYMBOL, SYMBOL (p))) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_DYADIC_PRIORITY);
  }
}

/*!
\brief enter operator declarations in the symbol table
\param p position in tree
\param m mode of operators to enter (picked from the left-most one)
**/

static void tax_op_dec (NODE_T * p, MOID_T ** m)
{
  if (p != NULL) {
    if (WHETHER (p, OPERATOR_DECLARATION)) {
      tax_op_dec (SUB (p), m);
      tax_op_dec (NEXT (p), m);
    } else if (WHETHER (p, OPERATOR_PLAN)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_op_dec (NEXT (p), m);
    } else if (WHETHER (p, OP_SYMBOL)) {
      tax_op_dec (NEXT (p), m);
    } else if (WHETHER (p, COMMA_SYMBOL)) {
      tax_op_dec (NEXT (p), m);
    } else if (WHETHER (p, DEFINING_OPERATOR)) {
      TAG_T *entry = SYMBOL_TABLE (p)->operators;
      check_operator_dec (p);
      while (entry != NULL && NODE (entry) != p) {
        FORWARD (entry);
      }
      MOID (p) = *m;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      MOID (entry) = *m;
      tax_op_dec (NEXT (p), m);
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter brief operator declarations in the symbol table
\param p position in tree
**/

static void tax_brief_op_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, BRIEF_OPERATOR_DECLARATION)) {
      tax_brief_op_dec (SUB (p));
      tax_brief_op_dec (NEXT (p));
    } else if (whether_one_of (p, OP_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_brief_op_dec (NEXT (p));
    } else if (WHETHER (p, DEFINING_OPERATOR)) {
      TAG_T *entry = SYMBOL_TABLE (p)->operators;
      MOID_T *m = MOID (NEXT_NEXT (p));
      check_operator_dec (p);
      while (entry != NULL && NODE (entry) != p) {
        FORWARD (entry);
      }
      MOID (p) = m;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      MOID (entry) = m;
      tax_brief_op_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter priority declarations in the symbol table
\param p position in tree
**/

static void tax_prio_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, PRIORITY_DECLARATION)) {
      tax_prio_dec (SUB (p));
      tax_prio_dec (NEXT (p));
    } else if (whether_one_of (p, PRIO_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      tax_prio_dec (NEXT (p));
    } else if (WHETHER (p, DEFINING_OPERATOR)) {
      TAG_T *entry = PRIO (SYMBOL_TABLE (p));
      while (entry != NULL && NODE (entry) != p) {
        FORWARD (entry);
      }
      MOID (p) = NULL;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      tax_prio_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

/*!
\brief enter TAXes in the symbol table
\param p position in tree
**/

static void tax_tags (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    int heap = LOC_SYMBOL;
    MOID_T *m = NULL;
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      tax_identity_dec (p, &m);
    } else if (WHETHER (p, VARIABLE_DECLARATION)) {
      tax_variable_dec (p, &heap, &m);
    } else if (WHETHER (p, PROCEDURE_DECLARATION)) {
      tax_proc_dec (p);
    } else if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      tax_proc_variable_dec (p, &heap);
    } else if (WHETHER (p, OPERATOR_DECLARATION)) {
      tax_op_dec (p, &m);
    } else if (WHETHER (p, BRIEF_OPERATOR_DECLARATION)) {
      tax_brief_op_dec (p);
    } else if (WHETHER (p, PRIORITY_DECLARATION)) {
      tax_prio_dec (p);
    } else {
      tax_tags (SUB (p));
    }
  }
}

/*!
\brief reset symbol table nest count
\param p position in tree
**/

void reset_symbol_table_nest_count (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE (SUB (p))->nest = symbol_table_count++;
    }
    reset_symbol_table_nest_count (SUB (p));
  }
}

/*!
\brief bind routines in symbol table to the tree
\param p position in tree
**/

void bind_routine_tags_to_tree (NODE_T * p)
{
/* By inserting coercions etc. some may have shifted */
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ROUTINE_TEXT) && TAX (p) != NULL) {
      NODE (TAX (p)) = p;
    }
    bind_routine_tags_to_tree (SUB (p));
  }
}

/*!
\brief bind formats in symbol table to tree
\param p position in tree
**/

void bind_format_tags_to_tree (NODE_T * p)
{
/* By inserting coercions etc. some may have shifted */
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, FORMAT_TEXT) && TAX (p) != NULL) {
      NODE (TAX (p)) = p;
    } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL) && NEXT (p) != NULL && TAX (p) != NULL) {
      NODE (TAX (p)) = p;
    }
    bind_format_tags_to_tree (SUB (p));
  }
}

/*!
\brief fill outer level of symbol table
\param p position in tree
\param s parent symbol table
**/

void fill_symbol_table_outer (NODE_T * p, SYMBOL_TABLE_T * s)
{
  for (; p != NULL; FORWARD (p)) {
    if (SYMBOL_TABLE (p) != NULL) {
      OUTER (SYMBOL_TABLE (p)) = s;
    }
    if (SUB (p) != NULL && ATTRIBUTE (p) == ROUTINE_TEXT) {
      fill_symbol_table_outer (SUB (p), SYMBOL_TABLE (SUB (p)));
    } else if (SUB (p) != NULL && ATTRIBUTE (p) == FORMAT_TEXT) {
      fill_symbol_table_outer (SUB (p), SYMBOL_TABLE (SUB (p)));
    } else {
      fill_symbol_table_outer (SUB (p), s);
    }
  }
}

/*!
\brief flood branch in tree with local symbol table "s"
\param p position in tree
\param s parent symbol table
**/

static void flood_with_symbol_table_restricted (NODE_T * p, SYMBOL_TABLE_T * s)
{
  for (; p != NULL; FORWARD (p)) {
    SYMBOL_TABLE (p) = s;
    if (ATTRIBUTE (p) != ROUTINE_TEXT && ATTRIBUTE (p) != SPECIFIED_UNIT) {
      if (whether_new_lexical_level (p)) {
        PREVIOUS (SYMBOL_TABLE (SUB (p))) = s;
      } else {
        flood_with_symbol_table_restricted (SUB (p), s);
      }
    }
  }
}

/*!
\brief final structure of symbol table after parsing
\param p position in tree
\param l current lexical level
**/

void finalise_symbol_table_setup (NODE_T * p, int l)
{
  SYMBOL_TABLE_T *s = SYMBOL_TABLE (p);
  NODE_T *q = p;
  while (q != NULL) {
/* routine texts are ranges */
    if (WHETHER (q, ROUTINE_TEXT)) {
      flood_with_symbol_table_restricted (SUB (q), new_symbol_table (s));
    }
/* specifiers are ranges */
    else if (WHETHER (q, SPECIFIED_UNIT)) {
      flood_with_symbol_table_restricted (SUB (q), new_symbol_table (s));
    }
/* level count and recursion */
    if (SUB (q) != NULL) {
      if (whether_new_lexical_level (q)) {
        LEX_LEVEL (SUB (q)) = l + 1;
        PREVIOUS (SYMBOL_TABLE (SUB (q))) = s;
        finalise_symbol_table_setup (SUB (q), l + 1);
        if (WHETHER (q, WHILE_PART)) {
/* This was a bug that went 15 years unnoticed ! */
          SYMBOL_TABLE_T *s2 = SYMBOL_TABLE (SUB (q));
          if ((FORWARD (q)) == NULL) {
            return;
          }
          if (WHETHER (q, ALT_DO_PART)) {
            PREVIOUS (SYMBOL_TABLE (SUB (q))) = s2;
            LEX_LEVEL (SUB (q)) = l + 2;
            finalise_symbol_table_setup (SUB (q), l + 2);
          }
        }
      } else {
        SYMBOL_TABLE (SUB (q)) = s;
        finalise_symbol_table_setup (SUB (q), l);
      }
    }
    SYMBOL_TABLE (q) = s;
    if (WHETHER (q, FOR_SYMBOL)) {
      FORWARD (q);
    }
    FORWARD (q);
  }
/* FOR identifiers are in the DO ... OD range */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, FOR_SYMBOL)) {
      SYMBOL_TABLE (NEXT (q)) = SYMBOL_TABLE (NEXT (q)->sequence);
    }
  }
}

/*!
\brief first structure of symbol table for parsing
\param p position in tree
**/

void preliminary_symbol_table_setup (NODE_T * p)
{
  NODE_T *q;
  SYMBOL_TABLE_T *s = SYMBOL_TABLE (p);
  BOOL_T not_a_for_range = A68_FALSE;
/* let the tree point to the current symbol table */
  for (q = p; q != NULL; FORWARD (q)) {
    SYMBOL_TABLE (q) = s;
  }
/* insert new tables when required */
  for (q = p; q != NULL && !not_a_for_range; FORWARD (q)) {
    if (SUB (q) != NULL) {
/* BEGIN ... END, CODE ... EDOC, DEF ... FED, DO ... OD, $ ... $, { ... } are ranges */
      if (whether_one_of (q, BEGIN_SYMBOL, DO_SYMBOL, ALT_DO_SYMBOL, FORMAT_DELIMITER_SYMBOL, ACCO_SYMBOL, NULL_ATTRIBUTE)) {
        SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
        preliminary_symbol_table_setup (SUB (q));
      }
/* ( ... ) is a range */
      else if (WHETHER (q, OPEN_SYMBOL)) {
        if (whether (q, OPEN_SYMBOL, THEN_BAR_SYMBOL, 0)) {
          SYMBOL_TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NULL) {
            not_a_for_range = A68_TRUE;
          } else {
            if (WHETHER (q, THEN_BAR_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (WHETHER (q, OPEN_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
/* don't worry about STRUCT (...), UNION (...), PROC (...) yet */
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* IF ... THEN ... ELSE ... FI are ranges */
      else if (WHETHER (q, IF_SYMBOL)) {
        if (whether (q, IF_SYMBOL, THEN_SYMBOL, 0)) {
          SYMBOL_TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NULL) {
            not_a_for_range = A68_TRUE;
          } else {
            if (WHETHER (q, ELSE_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (WHETHER (q, IF_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* CASE ... IN ... OUT ... ESAC are ranges */
      else if (WHETHER (q, CASE_SYMBOL)) {
        if (whether (q, CASE_SYMBOL, IN_SYMBOL, 0)) {
          SYMBOL_TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NULL) {
            not_a_for_range = A68_TRUE;
          } else {
            if (WHETHER (q, OUT_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (WHETHER (q, CASE_SYMBOL)) {
              SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* UNTIL ... OD is a range */
      else if (WHETHER (q, UNTIL_SYMBOL) && SUB (q) != NULL) {
        SYMBOL_TABLE (SUB (q)) = new_symbol_table (s);
        preliminary_symbol_table_setup (SUB (q));
/* WHILE ... DO ... OD are ranges */
      } else if (WHETHER (q, WHILE_SYMBOL)) {
        SYMBOL_TABLE_T *u = new_symbol_table (s);
        SYMBOL_TABLE (SUB (q)) = u;
        preliminary_symbol_table_setup (SUB (q));
        if ((FORWARD (q)) == NULL) {
          not_a_for_range = A68_TRUE;
        } else if (WHETHER (q, ALT_DO_SYMBOL)) {
          SYMBOL_TABLE (SUB (q)) = new_symbol_table (u);
          preliminary_symbol_table_setup (SUB (q));
        }
      } else {
        SYMBOL_TABLE (SUB (q)) = s;
        preliminary_symbol_table_setup (SUB (q));
      }
    }
  }
/* FOR identifiers will go to the DO ... OD range */
  if (!not_a_for_range) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (WHETHER (q, FOR_SYMBOL)) {
        NODE_T *r = q;
        SYMBOL_TABLE (NEXT (q)) = NULL;
        for (; r != NULL && SYMBOL_TABLE (NEXT (q)) == NULL; FORWARD (r)) {
          if ((whether_one_of (r, WHILE_SYMBOL, ALT_DO_SYMBOL, NULL_ATTRIBUTE)) && (NEXT (q) != NULL && SUB (r) != NULL)) {
            SYMBOL_TABLE (NEXT (q)) = SYMBOL_TABLE (SUB (r));
            NEXT (q)->sequence = SUB (r);
          }
        }
      }
    }
  }
}

/*!
\brief mark a mode as in use
\param m mode to mark
**/

static void mark_mode (MOID_T * m)
{
  if (m != NULL && USE (m) == A68_FALSE) {
    PACK_T *p = PACK (m);
    USE (m) = A68_TRUE;
    for (; p != NULL; FORWARD (p)) {
      mark_mode (MOID (p));
      mark_mode (SUB (m));
      mark_mode (SLICE (m));
    }
  }
}

/*!
\brief traverse tree and mark modes as used
\param p position in tree
**/

void mark_moids (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    mark_moids (SUB (p));
    if (MOID (p) != NULL) {
      mark_mode (MOID (p));
    }
  }
}

/*!
\brief mark various tags as used
\param p position in tree
**/

void mark_auxilliary (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL) {
/*
You get no warnings on unused PROC parameters. That is ok since A68 has some
parameters that you may not use at all - think of PROC (REF FILE) BOOL event
routines in transput.
*/
      mark_auxilliary (SUB (p));
    } else if (WHETHER (p, OPERATOR)) {
      TAG_T *z;
      if (TAX (p) != NULL) {
        USE (TAX (p)) = A68_TRUE;
      }
      if ((z = find_tag_global (SYMBOL_TABLE (p), PRIO_SYMBOL, SYMBOL (p))) != NULL) {
        USE (z) = A68_TRUE;
      }
    } else if (WHETHER (p, INDICANT)) {
      TAG_T *z = find_tag_global (SYMBOL_TABLE (p), INDICANT, SYMBOL (p));
      if (z != NULL) {
        TAX (p) = z;
        USE (z) = A68_TRUE;
      }
    } else if (WHETHER (p, IDENTIFIER)) {
      if (TAX (p) != NULL) {
        USE (TAX (p)) = A68_TRUE;
      }
    }
  }
}

/*!
\brief check a single tag
\param s tag to check
**/

static void unused (TAG_T * s)
{
  for (; s != NULL; FORWARD (s)) {
    if (!USE (s)) {
      diagnostic_node (A68_WARNING, NODE (s), WARNING_TAG_UNUSED, NODE (s));
    }
  }
}

/*!
\brief driver for traversing tree and warn for unused tags
\param p position in tree
**/

void warn_for_unused_tags (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && LINE_NUMBER (p) != 0) {
      if (whether_new_lexical_level (p) && ATTRIBUTE (SYMBOL_TABLE (SUB (p))) != ENVIRON_SYMBOL) {
        unused (SYMBOL_TABLE (SUB (p))->operators);
        unused (PRIO (SYMBOL_TABLE (SUB (p))));
        unused (SYMBOL_TABLE (SUB (p))->identifiers);
        unused (SYMBOL_TABLE (SUB (p))->indicants);
      }
    }
    warn_for_unused_tags (SUB (p));
  }
}

/*!
\brief warn if tags are used between threads
\param p position in tree
**/

void warn_tags_threads (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    warn_tags_threads (SUB (p));
    if (whether_one_of (p, IDENTIFIER, OPERATOR, NULL_ATTRIBUTE)) {
      if (TAX (p) != NULL) {
        int plev_def = PAR_LEVEL (NODE (TAX (p))), plev_app = PAR_LEVEL (p);
        if (plev_def != 0 && plev_def != plev_app) {
          diagnostic_node (A68_WARNING, p, WARNING_DEFINED_IN_OTHER_THREAD);
        }
      }
    }
  }
}

/*!
\brief mark jumps and procedured jumps
\param p position in tree
**/

void jumps_from_procs (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, PROCEDURING)) {
      NODE_T *u = SUB_SUB (p);
      if (WHETHER (u, GOTO_SYMBOL)) {
        FORWARD (u);
      }
      USE (TAX (u)) = A68_TRUE;
    } else if (WHETHER (p, JUMP)) {
      NODE_T *u = SUB (p);
      if (WHETHER (u, GOTO_SYMBOL)) {
        FORWARD (u);
      }
      if ((TAX (u) == NULL) && (MOID (u) == NULL) && (find_tag_global (SYMBOL_TABLE (u), LABEL, SYMBOL (u)) == NULL)) {
        (void) add_tag (SYMBOL_TABLE (u), LABEL, u, NULL, LOCAL_LABEL);
        diagnostic_node (A68_ERROR, u, ERROR_UNDECLARED_TAG);
      } else {
         USE (TAX (u)) = A68_TRUE;
      }
    } else {
      jumps_from_procs (SUB (p));
    }
  }
}

/*!
\brief assign offset tags
\param t tag to start from
\param base first (base) address
\return end address
**/

static ADDR_T assign_offset_tags (TAG_T * t, ADDR_T base)
{
  ADDR_T sum = base;
  for (; t != NULL; FORWARD (t)) {
    SIZE (t) = moid_size (MOID (t));
    if (VALUE (t) == NULL) {
      OFFSET (t) = sum;
      sum += SIZE (t);
    }
  }
  return (sum);
}

/*!
\brief assign offsets table
\param c symbol table 
**/

void assign_offsets_table (SYMBOL_TABLE_T * c)
{
  c->ap_increment = assign_offset_tags (c->identifiers, 0);
  c->ap_increment = assign_offset_tags (c->operators, c->ap_increment);
  c->ap_increment = assign_offset_tags (c->anonymous, c->ap_increment);
  c->ap_increment = A68_ALIGN (c->ap_increment);
}

/*!
\brief assign offsets
\param p position in tree
**/

void assign_offsets (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      assign_offsets_table (SYMBOL_TABLE (SUB (p)));
    }
    assign_offsets (SUB (p));
  }
}

/*!
\brief assign offsets packs in moid list
\param moid_list moid list entry to start from
**/

void assign_offsets_packs (MOID_LIST_T * moid_list)
{
  MOID_LIST_T *q;
  for (q = moid_list; q != NULL; FORWARD (q)) {
    if (EQUIVALENT (MOID (q)) == NULL && WHETHER (MOID (q), STRUCT_SYMBOL)) {
      PACK_T *p = PACK (MOID (q));
      ADDR_T offset = 0;
      for (; p != NULL; FORWARD (p)) {
        SIZE (p) = moid_size (MOID (p));
        OFFSET (p) = offset;
        offset += SIZE (p);
      }
    }
  }
}

/*
Mode collection, equivalencing and derived modes
*/

static MOID_T *get_mode_from_declarer (NODE_T *);
BOOL_T check_yin_yang (NODE_T *, MOID_T *, BOOL_T, BOOL_T);
static void moid_to_string_2 (char *, MOID_T *, int *, NODE_T *);
static MOID_T *make_deflexed (MOID_T *, MOID_T **);

MOID_LIST_T *top_moid_list, *old_moid_list = NULL;
static int max_simplout_size;
static POSTULATE_T *postulates;

/*!
\brief add mode "sub" to chain "z"
\param z chain to insert into
\param att attribute
\param dim dimension
\param node node
\param sub subordinate mode
\param pack pack
\return new entry
**/

MOID_T *add_mode (MOID_T ** z, int att, int dim, NODE_T * node, MOID_T * sub, PACK_T * pack)
{
  MOID_T *new_mode = new_moid ();
  new_mode->in_standard_environ = (BOOL_T) (z == &(stand_env->moids));
  USE (new_mode) = A68_FALSE;
  SIZE (new_mode) = 0;
  NUMBER (new_mode) = mode_count++;
  ATTRIBUTE (new_mode) = att;
  DIM (new_mode) = dim;
  NODE (new_mode) = node;
  new_mode->well_formed = A68_TRUE;
  new_mode->has_rows = (BOOL_T) (att == ROW_SYMBOL);
  SUB (new_mode) = sub;
  PACK (new_mode) = pack;
  NEXT (new_mode) = *z;
  EQUIVALENT (new_mode) = NULL;
  SLICE (new_mode) = NULL;
  DEFLEXED (new_mode) = NULL;
  NAME (new_mode) = NULL;
  MULTIPLE (new_mode) = NULL;
  TRIM (new_mode) = NULL;
  ROWED (new_mode) = NULL;
/* Link to chain and exit */
  *z = new_mode;
  return (new_mode);
}

/*!
\brief add row and its slices to chain, recursively
\param p chain to insert into
\param dim dimension
\param sub mode of slice
\param n position in tree
\return pointer to entry
**/

static MOID_T *add_row (MOID_T ** p, int dim, MOID_T * sub, NODE_T * n)
{
  (void) add_mode (p, ROW_SYMBOL, dim, n, sub, NULL);
  if (dim > 1) {
    SLICE (*p) = add_row (&NEXT (*p), dim - 1, sub, n);
  } else {
    SLICE (*p) = sub;
  }
  return (*p);
}

/*!
\brief initialise moid list
**/

void init_moid_list (void)
{
  top_moid_list = NULL;
  old_moid_list = NULL;
}

/*!
\brief reset moid list
**/

void reset_moid_list (void)
{
  old_moid_list = top_moid_list;
  top_moid_list = NULL;
}

/*!
\brief add single moid to list
\param p moid list to insert to
\param q moid to insert
\param c symbol table to link to
**/

void add_single_moid_to_list (MOID_LIST_T ** p, MOID_T * q, SYMBOL_TABLE_T * c)
{
  MOID_LIST_T *m;
  if (old_moid_list == NULL) {
    m = (MOID_LIST_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (MOID_LIST_T));
  } else {
    m = old_moid_list;
    old_moid_list = NEXT (old_moid_list);
  }
  m->coming_from_level = c;
  MOID (m) = q;
  NEXT (m) = *p;
  *p = m;
}

/*!
\brief add moid list
\param p chain to insert into
\param c symbol table from which to insert moids
**/

void add_moids_from_table (MOID_LIST_T ** p, SYMBOL_TABLE_T * c)
{
  if (c != NULL) {
    MOID_T *q;
    for (q = c->moids; q != NULL; FORWARD (q)) {
      add_single_moid_to_list (p, q, c);
    }
  }
}

/*!
\brief add moids from symbol tables to moid list
\param p position in tree
\param q chain to insert to
**/

void add_moids_from_table_tree (NODE_T * p, MOID_LIST_T ** q)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL) {
      add_moids_from_table_tree (SUB (p), q);
      if (whether_new_lexical_level (p)) {
        add_moids_from_table (q, SYMBOL_TABLE (SUB (p)));
      }
    }
  }
}

/*!
\brief count moids in a pack
\param u pack
\return same
**/

int count_pack_members (PACK_T * u)
{
  int k = 0;
  for (; u != NULL; FORWARD (u)) {
    k++;
  }
  return (k);
}

/*!
\brief add a moid to a pack, maybe with a (field) name
\param p pack
\param m moid to add
\param text field name
\param node position in tree
**/

void add_mode_to_pack (PACK_T ** p, MOID_T * m, char *text, NODE_T * node)
{
  PACK_T *z = new_pack ();
  MOID (z) = m;
  TEXT (z) = text;
  NODE (z) = node;
  NEXT (z) = *p;
  PREVIOUS (z) = NULL;
  if (NEXT (z) != NULL) {
    PREVIOUS (NEXT (z)) = z;
  }
/* Link in chain */
  *p = z;
}

/*!
\brief add a moid to a pack, maybe with a (field) name
\param p pack
\param m moid to add
\param text field name
\param node position in tree
**/

void add_mode_to_pack_end (PACK_T ** p, MOID_T * m, char *text, NODE_T * node)
{
  PACK_T *z = new_pack ();
  MOID (z) = m;
  TEXT (z) = text;
  NODE (z) = node;
  NEXT (z) = NULL;
  if (NEXT (z) != NULL) {
    PREVIOUS (NEXT (z)) = z;
  }
/* Link in chain */
  while ((*p) != NULL) {
    p = &(NEXT (*p));
  }
  PREVIOUS (z) = (*p);
  (*p) = z;
}

/*!
\brief count formal bounds in declarer in tree
\param p position in tree
\return same
**/

static int count_formal_bounds (NODE_T * p)
{
  if (p == NULL) {
    return (0);
  } else {
    if (WHETHER (p, COMMA_SYMBOL)) {
      return (1);
    } else {
      return (count_formal_bounds (NEXT (p)) + count_formal_bounds (SUB (p)));
    }
  }
}

/*!
\brief count bounds in declarer in tree
\param p position in tree
\return same
**/

static int count_bounds (NODE_T * p)
{
  if (p == NULL) {
    return (0);
  } else {
    if (WHETHER (p, BOUND)) {
      return (1 + count_bounds (NEXT (p)));
    } else {
      return (count_bounds (NEXT (p)) + count_bounds (SUB (p)));
    }
  }
}

/*!
\brief count number of SHORTs or LONGs
\param p position in tree
\return same
**/

static int count_sizety (NODE_T * p)
{
  if (p == NULL) {
    return (0);
  } else {
    switch (ATTRIBUTE (p)) {
    case LONGETY:
      {
        return (count_sizety (SUB (p)) + count_sizety (NEXT (p)));
      }
    case SHORTETY:
      {
        return (count_sizety (SUB (p)) + count_sizety (NEXT (p)));
      }
    case LONG_SYMBOL:
      {
        return (1);
      }
    case SHORT_SYMBOL:
      {
        return (-1);
      }
    default:
      {
        return (0);
      }
    }
  }
}

/* Routines to collect MOIDs from the program text */

/*!
\brief collect standard mode
\param sizety sizety
\param indicant position in tree
\return moid entry in standard environ
**/

static MOID_T *get_mode_from_standard_moid (int sizety, NODE_T * indicant)
{
  MOID_T *p = stand_env->moids;
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, STANDARD) && DIM (p) == sizety && SYMBOL (NODE (p)) == SYMBOL (indicant)) {
      return (p);
    }
  }
  if (sizety < 0) {
    return (get_mode_from_standard_moid (sizety + 1, indicant));
  } else if (sizety > 0) {
    return (get_mode_from_standard_moid (sizety - 1, indicant));
  } else {
    return (NULL);
  }
}

/*!
\brief collect mode from STRUCT field
\param p position in tree
\param u pack to insert to
**/

static void get_mode_from_struct_field (NODE_T * p, PACK_T ** u)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case IDENTIFIER:
      {
        ATTRIBUTE (p) = FIELD_IDENTIFIER;
        (void) add_mode_to_pack (u, NULL, SYMBOL (p), p);
        break;
      }
    case DECLARER:
      {
        MOID_T *new_one = get_mode_from_declarer (p);
        PACK_T *t;
        get_mode_from_struct_field (NEXT (p), u);
        for (t = *u; t && MOID (t) == NULL; FORWARD (t)) {
          MOID (t) = new_one;
          MOID (NODE (t)) = new_one;
        }
        break;
      }
    default:
      {
        get_mode_from_struct_field (NEXT (p), u);
        get_mode_from_struct_field (SUB (p), u);
        break;
      }
    }
  }
}

/*!
\brief collect MODE from formal pack
\param p position in tree
\param u pack to insert to
**/

static void get_mode_from_formal_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        MOID_T *z;
        get_mode_from_formal_pack (NEXT (p), u);
        z = get_mode_from_declarer (p);
        (void) add_mode_to_pack (u, z, NULL, p);
        break;
      }
    default:
      {
        get_mode_from_formal_pack (NEXT (p), u);
        get_mode_from_formal_pack (SUB (p), u);
        break;
      }
    }
  }
}

/*!
\brief collect MODE or VOID from formal UNION pack
\param p position in tree
\param u pack to insert to
**/

static void get_mode_from_union_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
    case VOID_SYMBOL:
      {
        MOID_T *z;
        get_mode_from_union_pack (NEXT (p), u);
        z = get_mode_from_declarer (p);
        (void) add_mode_to_pack (u, z, NULL, p);
        break;
      }
    default:
      {
        get_mode_from_union_pack (NEXT (p), u);
        get_mode_from_union_pack (SUB (p), u);
        break;
      }
    }
  }
}

/*!
\brief collect mode from PROC, OP pack
\param p position in tree
\param u pack to insert to
**/

static void get_mode_from_routine_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case IDENTIFIER:
      {
        (void) add_mode_to_pack (u, NULL, NULL, p);
        break;
      }
    case DECLARER:
      {
        MOID_T *z = get_mode_from_declarer (p);
        PACK_T *t = *u;
        for (; t != NULL && MOID (t) == NULL; FORWARD (t)) {
          MOID (t) = z;
          MOID (NODE (t)) = z;
        }
        (void) add_mode_to_pack (u, z, NULL, p);
        break;
      }
    default:
      {
        get_mode_from_routine_pack (NEXT (p), u);
        get_mode_from_routine_pack (SUB (p), u);
        break;
      }
    }
  }
}

/*!
\brief collect MODE from DECLARER
\param p position in tree
\param put_where insert in symbol table from "p" or in its parent
\return mode table entry or NULL on error
**/

static MOID_T *get_mode_from_declarer (NODE_T * p)
{
  if (p == NULL) {
    return (NULL);
  } else {
    if (WHETHER (p, DECLARER)) {
      if (MOID (p) != NULL) {
        return (MOID (p));
      } else {
        return (MOID (p) = get_mode_from_declarer (SUB (p)));
      }
    } else {
      MOID_T **m = &(SYMBOL_TABLE (p)->moids);
      if (WHETHER (p, VOID_SYMBOL)) {
        MOID (p) = MODE (VOID);
        return (MOID (p));
      } else if (WHETHER (p, LONGETY)) {
        if (whether (p, LONGETY, INDICANT, 0)) {
          int k = count_sizety (SUB (p));
          MOID (p) = get_mode_from_standard_moid (k, NEXT (p));
          return (MOID (p));
        } else {
          return (NULL);
        }
      } else if (WHETHER (p, SHORTETY)) {
        if (whether (p, SHORTETY, INDICANT, 0)) {
          int k = count_sizety (SUB (p));
          MOID (p) = get_mode_from_standard_moid (k, NEXT (p));
          return (MOID (p));
        } else {
          return (NULL);
        }
      } else if (WHETHER (p, INDICANT)) {
        MOID_T *q = get_mode_from_standard_moid (0, p);
        if (q != NULL) {
          MOID (p) = q;
        } else {
          MOID (p) = add_mode (m, INDICANT, 0, p, NULL, NULL);
        }
        return (MOID (p));
      } else if (WHETHER (p, REF_SYMBOL)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (m, REF_SYMBOL, 0, p, new_one, NULL);
        return (MOID (p));
      } else if (WHETHER (p, FLEX_SYMBOL)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (m, FLEX_SYMBOL, 0, p, new_one, NULL);
        SLICE (MOID (p)) = SLICE (new_one);
        return (MOID (p));
      } else if (WHETHER (p, FORMAL_BOUNDS)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_row (m, 1 + count_formal_bounds (SUB (p)), new_one, p);
        return (MOID (p));
      } else if (WHETHER (p, BOUNDS)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_row (m, count_bounds (SUB (p)), new_one, p);
        return (MOID (p));
      } else if (WHETHER (p, STRUCT_SYMBOL)) {
        PACK_T *u = NULL;
        get_mode_from_struct_field (NEXT (p), &u);
        MOID (p) = add_mode (m, STRUCT_SYMBOL, count_pack_members (u), p, NULL, u);
        return (MOID (p));
      } else if (WHETHER (p, UNION_SYMBOL)) {
        PACK_T *u = NULL;
        get_mode_from_union_pack (NEXT (p), &u);
        MOID (p) = add_mode (m, UNION_SYMBOL, count_pack_members (u), p, NULL, u);
        return (MOID (p));
      } else if (WHETHER (p, PROC_SYMBOL)) {
        NODE_T *save = p;
        PACK_T *u = NULL;
        MOID_T *new_one;
        if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
          get_mode_from_formal_pack (SUB_NEXT (p), &u);
          FORWARD (p);
        }
        new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (m, PROC_SYMBOL, count_pack_members (u), save, new_one, u);
        MOID (save) = MOID (p);
        return (MOID (p));
      } else {
        return (NULL);
      }
    }
  }
}

/*!
\brief collect MODEs from a routine-text header
\param p position in tree
\return mode table entry
**/

static MOID_T *get_mode_from_routine_text (NODE_T * p)
{
  PACK_T *u = NULL;
  MOID_T **m, *n;
  NODE_T *q = p;
  m = &(PREVIOUS (SYMBOL_TABLE (p))->moids);
  if (WHETHER (p, PARAMETER_PACK)) {
    get_mode_from_routine_pack (SUB (p), &u);
    FORWARD (p);
  }
  n = get_mode_from_declarer (p);
  return (add_mode (m, PROC_SYMBOL, count_pack_members (u), q, n, u));
}

/*!
\brief collect modes from operator-plan
\param p position in tree
\return mode table entry
**/

static MOID_T *get_mode_from_operator (NODE_T * p)
{
  PACK_T *u = NULL;
  MOID_T **m = &(SYMBOL_TABLE (p)->moids), *new_one;
  NODE_T *save = p;
  if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
    get_mode_from_formal_pack (SUB_NEXT (p), &u);
    FORWARD (p);
  }
  new_one = get_mode_from_declarer (NEXT (p));
  MOID (p) = add_mode (m, PROC_SYMBOL, count_pack_members (u), save, new_one, u);
  return (MOID (p));
}

/*!
\brief collect mode from denotation
\param p position in tree
\return mode table entry
**/

static void get_mode_from_denotation (NODE_T * p, int sizety)
{
  if (p != NULL) {
    if (WHETHER (p, ROW_CHAR_DENOTATION)) {
      if (strlen (SYMBOL (p)) == 1) {
        MOID (p) = MODE (CHAR);
      } else {
        MOID (p) = MODE (ROW_CHAR);
      }
    } else if (WHETHER (p, TRUE_SYMBOL) || WHETHER (p, FALSE_SYMBOL)) {
      MOID (p) = MODE (BOOL);
    } else if (WHETHER (p, INT_DENOTATION)) {
      switch (sizety) {
      case 0:
        {
          MOID (p) = MODE (INT);
          break;
        }
      case 1:
        {
          MOID (p) = MODE (LONG_INT);
          break;
        }
      case 2:
        {
          MOID (p) = MODE (LONGLONG_INT);
          break;
        }
      default:
        {
          MOID (p) = (sizety > 0 ? MODE (LONGLONG_INT) : MODE (INT));
          break;
        }
      }
    } else if (WHETHER (p, REAL_DENOTATION)) {
      switch (sizety) {
      case 0:
        {
          MOID (p) = MODE (REAL);
          break;
        }
      case 1:
        {
          MOID (p) = MODE (LONG_REAL);
          break;
        }
      case 2:
        {
          MOID (p) = MODE (LONGLONG_REAL);
          break;
        }
      default:
        {
          MOID (p) = (sizety > 0 ? MODE (LONGLONG_REAL) : MODE (REAL));
          break;
        }
      }
    } else if (WHETHER (p, BITS_DENOTATION)) {
      switch (sizety) {
      case 0:
        {
          MOID (p) = MODE (BITS);
          break;
        }
      case 1:
        {
          MOID (p) = MODE (LONG_BITS);
          break;
        }
      case 2:
        {
          MOID (p) = MODE (LONGLONG_BITS);
          break;
        }
      default:
        {
          MOID (p) = MODE (BITS);
          break;
        }
      }
    } else if (WHETHER (p, LONGETY) || WHETHER (p, SHORTETY)) {
      get_mode_from_denotation (NEXT (p), count_sizety (SUB (p)));
      MOID (p) = MOID (NEXT (p));
    } else if (WHETHER (p, EMPTY_SYMBOL)) {
      MOID (p) = MODE (VOID);
    }
  }
}

/*!
\brief collect modes from the syntax tree
\param p position in tree
\param attribute
**/

static void get_modes_from_tree (NODE_T * p, int attribute)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, VOID_SYMBOL)) {
      MOID (q) = MODE (VOID);
    } else if (WHETHER (q, DECLARER)) {
      if (attribute == VARIABLE_DECLARATION) {
        MOID_T **m = &(SYMBOL_TABLE (q)->moids);
        MOID_T *new_one = get_mode_from_declarer (q);
        MOID (q) = add_mode (m, REF_SYMBOL, 0, NULL, new_one, NULL);
      } else {
        MOID (q) = get_mode_from_declarer (q);
      }
    } else if (WHETHER (q, ROUTINE_TEXT)) {
      MOID (q) = get_mode_from_routine_text (SUB (q));
    } else if (WHETHER (q, OPERATOR_PLAN)) {
      MOID (q) = get_mode_from_operator (SUB (q));
    } else if (whether_one_of (q, LOC_SYMBOL, HEAP_SYMBOL, NEW_SYMBOL, NULL)) {
      if (attribute == GENERATOR) {
        MOID_T **m = &(SYMBOL_TABLE (q)->moids);
        MOID_T *new_one = get_mode_from_declarer (NEXT (q));
        MOID (NEXT (q)) = new_one;
        MOID (q) = add_mode (m, REF_SYMBOL, 0, NULL, new_one, NULL);
      }
    } else {
      if (attribute == DENOTATION) {
        get_mode_from_denotation (q, 0);
      }
    }
  }
  if (attribute != DENOTATION) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (SUB (q) != NULL) {
        get_modes_from_tree (SUB (q), ATTRIBUTE (q));
      }
    }
  }
}

/*!
\brief collect modes from proc variables
\param p position in tree
**/

static void get_mode_from_proc_variables (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      get_mode_from_proc_variables (SUB (p));
      get_mode_from_proc_variables (NEXT (p));
    } else if (WHETHER (p, QUALIFIER) || WHETHER (p, PROC_SYMBOL) || WHETHER (p, COMMA_SYMBOL)) {
      get_mode_from_proc_variables (NEXT (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      MOID_T **m = &(SYMBOL_TABLE (p)->moids), *new_one = MOID (NEXT_NEXT (p));
      MOID (p) = add_mode (m, REF_SYMBOL, 0, p, new_one, NULL);
    }
  }
}

/*!
\brief collect modes from proc variable declarations
\param p position in tree
**/

static void get_mode_from_proc_var_declarations_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    get_mode_from_proc_var_declarations_tree (SUB (p));
    if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      get_mode_from_proc_variables (p);
    }
  }
}

/* Various routines to test modes */

/*!
\brief test whether a MODE shows VOID
\param m moid under test
\return same
**/

static BOOL_T whether_mode_has_void (MOID_T * m)
{
  if (m == MODE (VOID)) {
    return (A68_TRUE);
  } else if (whether_postulated_pair (top_postulate, m, NULL)) {
    return (A68_FALSE);
  } else {
    int z = ATTRIBUTE (m);
    make_postulate (&top_postulate, m, NULL);
    if (z == REF_SYMBOL || z == FLEX_SYMBOL || z == ROW_SYMBOL) {
      return whether_mode_has_void (SUB (m));
    } else if (z == STRUCT_SYMBOL) {
      PACK_T *p;
      for (p = PACK (m); p != NULL; FORWARD (p)) {
        if (whether_mode_has_void (MOID (p))) {
          return (A68_TRUE);
        }
      }
      return (A68_FALSE);
    } else if (z == UNION_SYMBOL) {
      PACK_T *p;
      for (p = PACK (m); p != NULL; FORWARD (p)) {
        if (MOID (p) != MODE (VOID) && whether_mode_has_void (MOID (p))) {
          return (A68_TRUE);
        }
      }
      return (A68_FALSE);
    } else if (z == PROC_SYMBOL) {
      PACK_T *p;
      for (p = PACK (m); p != NULL; FORWARD (p)) {
        if (whether_mode_has_void (MOID (p))) {
          return (A68_TRUE);
        }
      }
      if (SUB (m) == MODE (VOID)) {
        return (A68_FALSE);
      } else {
        return (whether_mode_has_void (SUB (m)));
      }
    } else {
      return (A68_FALSE);
    }
  }
}

/*!
\brief check for modes that are related to VOID
\param p position in tree
**/

static void check_relation_to_void_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m;
      for (m = SYMBOL_TABLE (SUB (p))->moids; m != NULL; FORWARD (m)) {
        free_postulate_list (top_postulate, NULL);
        top_postulate = NULL;
        if (NODE (m) != NULL && whether_mode_has_void (m)) {
          diagnostic_node (A68_ERROR, NODE (m), ERROR_RELATED_MODES, m, MODE (VOID));
        }
      }
    }
    check_relation_to_void_tree (SUB (p));
  }
}

/*!
\brief absorb UNION pack
\param t pack
\param mods modification count 
\return absorbed pack
**/

PACK_T *absorb_union_pack (PACK_T * t, int *mods)
{
  PACK_T *z = NULL;
  for (; t != NULL; FORWARD (t)) {
    if (WHETHER (MOID (t), UNION_SYMBOL)) {
      PACK_T *s;
      (*mods)++;
      for (s = PACK (MOID (t)); s != NULL; FORWARD (s)) {
        (void) add_mode_to_pack (&z, MOID (s), NULL, NODE (s));
      }
    } else {
      (void) add_mode_to_pack (&z, MOID (t), NULL, NODE (t));
    }
  }
  return (z);
}

/*!
\brief absorb UNION members troughout symbol tables
\param p position in tree
\param mods modification count 
**/

static void absorb_unions_tree (NODE_T * p, int *mods)
{
/*
UNION (A, UNION (B, C)) = UNION (A, B, C) or
UNION (A, UNION (A, B)) = UNION (A, B).
*/
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m;
      for (m = SYMBOL_TABLE (SUB (p))->moids; m != NULL; FORWARD (m)) {
        if (WHETHER (m, UNION_SYMBOL)) {
          PACK (m) = absorb_union_pack (PACK (m), mods);
        }
      }
    }
    absorb_unions_tree (SUB (p), mods);
  }
}

/*!
\brief contract a UNION
\param u united mode
\param mods modification count 
**/

void contract_union (MOID_T * u, int *mods)
{
  PACK_T *s = PACK (u);
  for (; s != NULL; FORWARD (s)) {
    PACK_T *t = s;
    while (t != NULL) {
      if (NEXT (t) != NULL && MOID (NEXT (t)) == MOID (s)) {
        (*mods)++;
        MOID (t) = MOID (t);
        NEXT (t) = NEXT_NEXT (t);
      } else {
        FORWARD (t);
      }
    }
  }
}

/*!
\brief contract UNIONs troughout symbol tables
\param p position in tree
\param mods modification count 
**/

static void contract_unions_tree (NODE_T * p, int *mods)
{
/* UNION (A, B, A) -> UNION (A, B) */
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m;
      for (m = SYMBOL_TABLE (SUB (p))->moids; m != NULL; FORWARD (m)) {
        if (WHETHER (m, UNION_SYMBOL) && EQUIVALENT (m) == NULL) {
          contract_union (m, mods);
        }
      }
    }
    contract_unions_tree (SUB (p), mods);
  }
}

/*!
\brief bind indicants in symbol tables to tags in syntax tree
\param p position in tree
**/

static void bind_indicants_to_tags_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *s = SYMBOL_TABLE (SUB (p));
      TAG_T *z;
      for (z = s->indicants; z != NULL; FORWARD (z)) {
        TAG_T *y = find_tag_global (s, INDICANT, SYMBOL (NODE (z)));
        if (y != NULL && NODE (y) != NULL) {
          MOID (z) = MOID (NEXT_NEXT (NODE (y)));
        }
      }
    }
    bind_indicants_to_tags_tree (SUB (p));
  }
}

/*!
\brief bind indicants in symbol tables to tags in syntax tree
\param p position in tree
**/

static void bind_indicants_to_modes_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *s = SYMBOL_TABLE (SUB (p));
      MOID_T *z;
      for (z = s->moids; z != NULL; FORWARD (z)) {
        if (WHETHER (z, INDICANT)) {
          TAG_T *y = find_tag_global (s, INDICANT, SYMBOL (NODE (z)));
          if (y != NULL && NODE (y) != NULL) {
            EQUIVALENT (z) = MOID (NEXT_NEXT (NODE (y)));
          } else {
            diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG_2, SYMBOL (NODE (z)));
          }
        }
      }
    }
    bind_indicants_to_modes_tree (SUB (p));
  }
}

/*!
\brief whether a mode declaration refers to self
\param table first tag in chain
\param p mode under test
\return same
**/

static BOOL_T cyclic_declaration (TAG_T * table, MOID_T * p)
{
  if (WHETHER (p, VOID_SYMBOL)) {
    return (A68_TRUE);
  } else if (WHETHER (p, INDICANT)) {
    if (whether_postulated (top_postulate, p)) {
      return (A68_TRUE);
    } else {
      TAG_T *z;
      for (z = table; z != NULL; FORWARD (z)) {
        if (SYMBOL (NODE (z)) == SYMBOL (NODE (p))) {
          make_postulate (&top_postulate, p, NULL);
          return (cyclic_declaration (table, MOID (z)));
        }
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief check for cyclic mode chains like MODE A = B, B = C, C = A
\param p position in tree
**/

static void check_cyclic_modes_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      TAG_T *table = SYMBOL_TABLE (SUB (p))->indicants, *z;
      for (z = table; z != NULL; FORWARD (z)) {
        free_postulate_list (top_postulate, NULL);
        top_postulate = NULL;
        if (cyclic_declaration (table, MOID (z))) {
          diagnostic_node (A68_ERROR, NODE (z), ERROR_CYCLIC_MODE, MOID (z));
        }
      }
    }
    check_cyclic_modes_tree (SUB (p));
  }
}

/*!
\brief check flex mode chains like MODE A = FLEX B, B = C, C = INT
\param p position in tree
**/

static void check_flex_modes_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *z;
      for (z = SYMBOL_TABLE (SUB (p))->moids; z != NULL; FORWARD (z)) {
        if (WHETHER (z, FLEX_SYMBOL)) {
          NODE_T *err = NODE (z);
          MOID_T *sub = SUB (z);
          while (WHETHER (sub, INDICANT)) {
            sub = EQUIVALENT (sub);
          }
          if (WHETHER_NOT (sub, ROW_SYMBOL)) {
            diagnostic_node (A68_ERROR, (err == NULL ? p : err), ERROR_FLEX_ROW);
          }
        }
      }
    }
    check_flex_modes_tree (SUB (p));
  }
}

/*!
\brief whether pack is well-formed
\param p position in tree
\param s pack
\param yin yin
\param yang yang
\return same
**/

static BOOL_T check_yin_yang_pack (NODE_T * p, PACK_T * s, BOOL_T yin, BOOL_T yang)
{
  for (; s != NULL; FORWARD (s)) {
    if (! check_yin_yang (p, MOID (s), yin, yang)) {
      return (A68_FALSE);
    }
  }
  return (A68_TRUE);
}

/*!
\brief whether mode is well-formed
\param def position in tree of definition
\param dec moid of declarer
\param yin yin
\param yang yang
\return same
**/

BOOL_T check_yin_yang (NODE_T * def, MOID_T * dec, BOOL_T yin, BOOL_T yang)
{
  if (dec->well_formed == A68_FALSE) {
    return (A68_TRUE);
  } else {
    if (WHETHER (dec, VOID_SYMBOL)) {
      return (A68_TRUE);
    } else if (WHETHER (dec, STANDARD)) {
      return (A68_TRUE);
    } else if (WHETHER (dec, INDICANT)) {
      if (SYMBOL (def) == SYMBOL (NODE (dec))) {
        return ((BOOL_T) (yin && yang));
      } else {
        TAG_T *s = SYMBOL_TABLE (def)->indicants;
        BOOL_T z = A68_TRUE;
        while (s != NULL && z) {
          if (SYMBOL (NODE (s)) == SYMBOL (NODE (dec))) {
            z = A68_FALSE;
          } else {
            FORWARD (s);
          }
        }
        return ((BOOL_T) (s == NULL ? A68_TRUE : check_yin_yang (def, MOID (s), yin, yang)));
      }
    } else if (WHETHER (dec, REF_SYMBOL)) {
      return ((BOOL_T) (yang ? A68_TRUE : check_yin_yang (def, SUB (dec), A68_TRUE, yang)));
    } else if (WHETHER (dec, FLEX_SYMBOL) || WHETHER (dec, ROW_SYMBOL)) {
      return ((BOOL_T) (check_yin_yang (def, SUB (dec), yin, yang)));
    } else if (WHETHER (dec, ROW_SYMBOL)) {
      return ((BOOL_T) (check_yin_yang (def, SUB (dec), yin, yang)));
    } else if (WHETHER (dec, STRUCT_SYMBOL)) {
      return ((BOOL_T) (yin ? A68_TRUE : check_yin_yang_pack (def, PACK (dec), yin, A68_TRUE)));
    } else if (WHETHER (dec, UNION_SYMBOL)) {
      return ((BOOL_T) check_yin_yang_pack (def, PACK (dec), yin, yang));
    } else if (WHETHER (dec, PROC_SYMBOL)) {
      if (PACK (dec) != NULL) {
        return (A68_TRUE);
      } else {
        return ((BOOL_T) (yang ? A68_TRUE : check_yin_yang (def, SUB (dec), A68_TRUE, yang)));
      }
    } else {
      return (A68_FALSE);
    }
  }
}

/*!
\brief check well-formedness of modes in the program
\param p position in tree
**/

static void check_well_formedness_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    check_well_formedness_tree (SUB (p));
    if (WHETHER (p, DEFINING_INDICANT)) {
      MOID_T *z = NULL;
      if (NEXT (p) != NULL && NEXT_NEXT (p) != NULL) {
        z = MOID (NEXT_NEXT (p));
      }
      if (!check_yin_yang (p, z, A68_FALSE, A68_FALSE)) {
        diagnostic_node (A68_ERROR, p, ERROR_NOT_WELL_FORMED);
        z->well_formed = A68_FALSE;
      }
    }
  }
}

/*
After the initial version of the mode equivalencer was made to work (1993), I
found: Algol Bulletin 30.3.3 C.H.A. Koster: On infinite modes, 86-89 [1969],
which essentially concurs with the algorithm on mode equivalence I wrote (and
which is still here). It is basic logic anyway: prove equivalence of things
postulating their equivalence.
*/

/*!
\brief whether packs s and t are equivalent
\param s pack 1
\param t pack 2
\return same
**/

static BOOL_T whether_packs_equivalent (PACK_T * s, PACK_T * t)
{
  for (; s != NULL && t != NULL; FORWARD (s), FORWARD (t)) {
    if (!whether_modes_equivalent (MOID (s), MOID (t))) {
      return (A68_FALSE);
    }
    if (TEXT (s) != TEXT (t)) {
      return (A68_FALSE);
    }
  }
  return ((BOOL_T) (s == NULL && t == NULL));
}

/*!
\brief whether packs contain each others' modes
\param s united pack 1
\param t united pack 2
\return same
**/

static BOOL_T whether_united_packs_equivalent (PACK_T * s, PACK_T * t)
{
  PACK_T *p, *q; BOOL_T f;
/* s is a subset of t .. */
  for (p = s; p != NULL; FORWARD (p)) {
    for (f = A68_FALSE, q = t; q != NULL && !f; FORWARD (q)) {
      f = whether_modes_equivalent (MOID (p), MOID (q));
    }
    if (!f) {
      return (A68_FALSE);
    }
  }
/* ... and t is a subset of s .. */
  for (p = t; p != NULL; FORWARD (p)) {
    for (f = A68_FALSE, q = s; q != NULL && !f; FORWARD (q)) {
      f = whether_modes_equivalent (MOID (p), MOID (q));
    }
    if (!f) {
      return (A68_FALSE);
    }
  }
  return (A68_TRUE);
}

/*!
\brief whether moids a and b are structurally equivalent
\param a moid
\param b moid
\return same
**/

BOOL_T whether_modes_equivalent (MOID_T * a, MOID_T * b)
{
  if (a == b) {
    return (A68_TRUE);
  } else if (ATTRIBUTE (a) != ATTRIBUTE (b)) {
    return (A68_FALSE);
  } else if (WHETHER (a, STANDARD)) {
    return ((BOOL_T) (a == b));
  } else if (EQUIVALENT (a) == b || EQUIVALENT (b) == a) {
    return (A68_TRUE);
  } else if (whether_postulated_pair (top_postulate, a, b) || whether_postulated_pair (top_postulate, b, a)) {
    return (A68_TRUE);
  } else if (WHETHER (a, INDICANT)) {
    return ((BOOL_T) whether_modes_equivalent (EQUIVALENT (a), EQUIVALENT (b)));
  } else if (WHETHER (a, REF_SYMBOL)) {
    return ((BOOL_T) whether_modes_equivalent (SUB (a), SUB (b)));
  } else if (WHETHER (a, FLEX_SYMBOL)) {
    return ((BOOL_T) whether_modes_equivalent (SUB (a), SUB (b)));
  } else if (WHETHER (a, ROW_SYMBOL)) {
    return ((BOOL_T) (DIM (a) == DIM (b) && whether_modes_equivalent (SUB (a), SUB (b))));
  } else if (WHETHER (a, PROC_SYMBOL) && DIM (a) == 0) {
    if (DIM (b) == 0) {
      return ((BOOL_T) whether_modes_equivalent (SUB (a), SUB (b)));
    } else {
      return (A68_FALSE);
    }
  } else if (WHETHER (a, STRUCT_SYMBOL)) {
    POSTULATE_T *save; BOOL_T z;
    if (DIM (a) != DIM (b)) {
      return (A68_FALSE);
    }
    save = top_postulate;
    make_postulate (&top_postulate, a, b);
    z = whether_packs_equivalent (PACK (a), PACK (b));
    free_postulate_list (top_postulate, save);
    top_postulate = save;
    return (z);
  } else if (WHETHER (a, UNION_SYMBOL)) {
    return ((BOOL_T) whether_united_packs_equivalent (PACK (a), PACK (b)));
  } else if (WHETHER (a, PROC_SYMBOL) && DIM (a) > 0) {
    POSTULATE_T *save; BOOL_T z;
    if (DIM (a) != DIM (b)) {
      return (A68_FALSE);
    }
    if (ATTRIBUTE (SUB (a)) != ATTRIBUTE (SUB (b))) {
      return (A68_FALSE);
    }
    if (WHETHER (SUB (a), STANDARD) && SUB (a) != SUB (b)) {
      return (A68_FALSE);
    }
    save = top_postulate;
    make_postulate (&top_postulate, a, b);
    z = whether_modes_equivalent (SUB (a), SUB (b));
    if (z) {
      z = whether_packs_equivalent (PACK (a), PACK (b));
    }
    free_postulate_list (top_postulate, save);
    top_postulate = save;
    return (z);
  } else if (WHETHER (a, SERIES_MODE) || WHETHER (a, STOWED_MODE)) {
     return ((BOOL_T) (DIM (a) == DIM (b) && whether_packs_equivalent (PACK (a), PACK (b))));
  }
  ABEND (A68_TRUE, "cannot decide in whether_modes_equivalent", NULL);
  return (A68_FALSE);
}

/*!
\brief whether modes 1 and 2 are structurally equivalent
\param p mode 1
\param q mode 2
\return same
**/

static BOOL_T prove_moid_equivalence (MOID_T * p, MOID_T * q)
{
/* Prove that two modes are equivalent under assumption that they are */
  POSTULATE_T *save = top_postulate;
  BOOL_T z = whether_modes_equivalent (p, q);
/* If modes are equivalent, mark this depending on which one is in standard environ */
  if (z) {
    if (q->in_standard_environ) {
      EQUIVALENT (p) = q;
    } else {
      EQUIVALENT (q) = p;
    }
  }
  free_postulate_list (top_postulate, save);
  top_postulate = save;
  return (z);
}

/*!
\brief find equivalent modes in program
\param start moid in moid list where to start
\param stop moid in moid list where to stop
**/

static void find_equivalent_moids (MOID_LIST_T * start, MOID_LIST_T * stop)
{
  for (; start != stop; FORWARD (start)) {
    MOID_LIST_T *p;
    MOID_T *master = MOID (start);
    for (p = NEXT (start); (p != NULL) && (EQUIVALENT (master) == NULL); FORWARD (p)) {
      MOID_T *slave = MOID (p);
      if ((EQUIVALENT (slave) == NULL) && (ATTRIBUTE (master) == ATTRIBUTE (slave)) && (DIM (master) == DIM (slave))) {
        (void) prove_moid_equivalence (slave, master);
      }
    }
  }
}

/*!
\brief replace a mode by its equivalent mode
\param m mode to replace
**/

static void track_equivalent_modes (MOID_T ** m)
{
  while ((*m) != NULL && EQUIVALENT ((*m)) != NULL) {
    (*m) = EQUIVALENT (*m);
  }
}

/*!
\brief replace a mode by its equivalent mode (walk chain)
\param q mode to track
**/

static void track_equivalent_one_moid (MOID_T * q)
{
  PACK_T *p;
  track_equivalent_modes (&SUB (q));
  track_equivalent_modes (&DEFLEXED (q));
  track_equivalent_modes (&MULTIPLE (q));
  track_equivalent_modes (&NAME (q));
  track_equivalent_modes (&SLICE (q));
  track_equivalent_modes (&TRIM (q));
  track_equivalent_modes (&ROWED (q));
  for (p = PACK (q); p != NULL; FORWARD (p)) {
    track_equivalent_modes (&MOID (p));
  }
}

/*!
\brief moid list track equivalent
\param q mode to track
**/

static void moid_list_track_equivalent (MOID_T * q)
{
  for (; q != NULL; FORWARD (q)) {
    track_equivalent_one_moid (q);
  }
}

/*!
\brief track equivalent tags
\param z tag to track
**/

static void track_equivalent_tags (TAG_T * z)
{
  for (; z != NULL; FORWARD (z)) {
    while (EQUIVALENT (MOID (z)) != NULL) {
      MOID (z) = EQUIVALENT (MOID (z));
    }
  }
}

/*!
\brief track equivalent tree
\param p position in tree
**/

static void track_equivalent_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (MOID (p) != NULL) {
      while (EQUIVALENT (MOID (p)) != NULL) {
        MOID (p) = EQUIVALENT (MOID (p));
      }
    }
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      if (SYMBOL_TABLE (SUB (p)) != NULL) {
        track_equivalent_tags (SYMBOL_TABLE (SUB (p))->indicants);
        moid_list_track_equivalent (SYMBOL_TABLE (SUB (p))->moids);
      }
    }
    track_equivalent_tree (SUB (p));
  }
}

/*!
\brief track equivalent standard modes
**/

static void track_equivalent_standard_modes (void)
{
  track_equivalent_modes (&MODE (BITS));
  track_equivalent_modes (&MODE (BOOL));
  track_equivalent_modes (&MODE (BYTES));
  track_equivalent_modes (&MODE (CHANNEL));
  track_equivalent_modes (&MODE (CHAR));
  track_equivalent_modes (&MODE (COLLITEM));
  track_equivalent_modes (&MODE (COMPL));
  track_equivalent_modes (&MODE (COMPLEX));
  track_equivalent_modes (&MODE (C_STRING));
  track_equivalent_modes (&MODE (ERROR));
  track_equivalent_modes (&MODE (FILE));
  track_equivalent_modes (&MODE (FORMAT));
  track_equivalent_modes (&MODE (HIP));
  track_equivalent_modes (&MODE (INT));
  track_equivalent_modes (&MODE (LONG_BITS));
  track_equivalent_modes (&MODE (LONG_BYTES));
  track_equivalent_modes (&MODE (LONG_COMPL));
  track_equivalent_modes (&MODE (LONG_COMPLEX));
  track_equivalent_modes (&MODE (LONG_INT));
  track_equivalent_modes (&MODE (LONGLONG_BITS));
  track_equivalent_modes (&MODE (LONGLONG_COMPL));
  track_equivalent_modes (&MODE (LONGLONG_COMPLEX));
  track_equivalent_modes (&MODE (LONGLONG_INT));
  track_equivalent_modes (&MODE (LONGLONG_REAL));
  track_equivalent_modes (&MODE (LONG_REAL));
  track_equivalent_modes (&MODE (NUMBER));
  track_equivalent_modes (&MODE (PIPE));
  track_equivalent_modes (&MODE (PROC_REF_FILE_BOOL));
  track_equivalent_modes (&MODE (PROC_REF_FILE_VOID));
  track_equivalent_modes (&MODE (PROC_ROW_CHAR));
  track_equivalent_modes (&MODE (PROC_STRING));
  track_equivalent_modes (&MODE (PROC_VOID));
  track_equivalent_modes (&MODE (REAL));
  track_equivalent_modes (&MODE (REF_BITS));
  track_equivalent_modes (&MODE (REF_BOOL));
  track_equivalent_modes (&MODE (REF_BYTES));
  track_equivalent_modes (&MODE (REF_CHAR));
  track_equivalent_modes (&MODE (REF_COMPL));
  track_equivalent_modes (&MODE (REF_COMPLEX));
  track_equivalent_modes (&MODE (REF_FILE));
  track_equivalent_modes (&MODE (REF_FORMAT));
  track_equivalent_modes (&MODE (REF_INT));
  track_equivalent_modes (&MODE (REF_LONG_BITS));
  track_equivalent_modes (&MODE (REF_LONG_BYTES));
  track_equivalent_modes (&MODE (REF_LONG_COMPL));
  track_equivalent_modes (&MODE (REF_LONG_COMPLEX));
  track_equivalent_modes (&MODE (REF_LONG_INT));
  track_equivalent_modes (&MODE (REF_LONGLONG_BITS));
  track_equivalent_modes (&MODE (REF_LONGLONG_COMPL));
  track_equivalent_modes (&MODE (REF_LONGLONG_COMPLEX));
  track_equivalent_modes (&MODE (REF_LONGLONG_INT));
  track_equivalent_modes (&MODE (REF_LONGLONG_REAL));
  track_equivalent_modes (&MODE (REF_LONG_REAL));
  track_equivalent_modes (&MODE (REF_PIPE));
  track_equivalent_modes (&MODE (REF_REAL));
  track_equivalent_modes (&MODE (REF_REF_FILE));
  track_equivalent_modes (&MODE (REF_ROW_CHAR));
  track_equivalent_modes (&MODE (REF_ROW_COMPLEX));
  track_equivalent_modes (&MODE (REF_ROW_INT));
  track_equivalent_modes (&MODE (REF_ROW_REAL));
  track_equivalent_modes (&MODE (REF_ROWROW_COMPLEX));
  track_equivalent_modes (&MODE (REF_ROWROW_REAL));
  track_equivalent_modes (&MODE (REF_SOUND));
  track_equivalent_modes (&MODE (REF_STRING));
  track_equivalent_modes (&MODE (ROW_BITS));
  track_equivalent_modes (&MODE (ROW_BOOL));
  track_equivalent_modes (&MODE (ROW_CHAR));
  track_equivalent_modes (&MODE (ROW_COMPLEX));
  track_equivalent_modes (&MODE (ROW_INT));
  track_equivalent_modes (&MODE (ROW_LONG_BITS));
  track_equivalent_modes (&MODE (ROW_LONGLONG_BITS));
  track_equivalent_modes (&MODE (ROW_REAL));
  track_equivalent_modes (&MODE (ROW_ROW_CHAR));
  track_equivalent_modes (&MODE (ROWROW_COMPLEX));
  track_equivalent_modes (&MODE (ROWROW_REAL));
  track_equivalent_modes (&MODE (ROWS));
  track_equivalent_modes (&MODE (ROW_SIMPLIN));
  track_equivalent_modes (&MODE (ROW_SIMPLOUT));
  track_equivalent_modes (&MODE (ROW_STRING));
  track_equivalent_modes (&MODE (SEMA));
  track_equivalent_modes (&MODE (SIMPLIN));
  track_equivalent_modes (&MODE (SIMPLOUT));
  track_equivalent_modes (&MODE (SOUND));
  track_equivalent_modes (&MODE (SOUND_DATA));
  track_equivalent_modes (&MODE (STRING));
  track_equivalent_modes (&MODE (UNDEFINED));
  track_equivalent_modes (&MODE (VACUUM));
  track_equivalent_modes (&MODE (VOID));
}

/*
Routines for calculating subordinates for selections, for instance selection
from REF STRUCT (A) yields REF A fields and selection from [] STRUCT (A) yields
[] A fields.
*/

/*!
\brief make name pack
\param src source pack
\param dst destination pack with REF modes
\param p chain to insert new modes into
**/

static void make_name_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p)
{
  if (src != NULL) {
    MOID_T *z;
    make_name_pack (NEXT (src), dst, p);
    z = add_mode (p, REF_SYMBOL, 0, NULL, MOID (src), NULL);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

/*!
\brief make name struct
\param m structured mode
\param p chain to insert new modes into
\return mode table entry
**/

static MOID_T *make_name_struct (MOID_T * m, MOID_T ** p)
{
  MOID_T *save;
  PACK_T *u = NULL;
  (void) add_mode (p, STRUCT_SYMBOL, DIM (m), NULL, NULL, NULL);
  save = *p;
  make_name_pack (PACK (m), &u, p);
  PACK (save) = u;
  return (save);
}

/*!
\brief make name row
\param m rowed mode
\param p chain to insert new modes into
\return mode table entry
**/

static MOID_T *make_name_row (MOID_T * m, MOID_T ** p)
{
  if (SLICE (m) != NULL) {
    return (add_mode (p, REF_SYMBOL, 0, NULL, SLICE (m), NULL));
  } else {
    return (add_mode (p, REF_SYMBOL, 0, NULL, SUB (m), NULL));
  }
}

/*!
\brief make structured names
\param p position in tree
\param mods modification count
**/

static void make_stowed_names_tree (NODE_T * p, int *mods)
{
  for (; p != NULL; FORWARD (p)) {
/* Dive into lexical levels */
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *symbol_table = SYMBOL_TABLE (SUB (p));
      MOID_T **topmoid = &(symbol_table->moids);
      BOOL_T k = A68_TRUE;
      while (k) {
        MOID_T *m = symbol_table->moids;
        k = A68_FALSE;
        for (; m != NULL; FORWARD (m)) {
          if (NAME (m) == NULL && WHETHER (m, REF_SYMBOL)) {
            if (WHETHER (SUB (m), STRUCT_SYMBOL)) {
              k = A68_TRUE;
              (*mods)++;
              NAME (m) = make_name_struct (SUB (m), topmoid);
            } else if (WHETHER (SUB (m), ROW_SYMBOL)) {
              k = A68_TRUE;
              (*mods)++;
              NAME (m) = make_name_row (SUB (m), topmoid);
            } else if (WHETHER (SUB (m), FLEX_SYMBOL)) {
              k = A68_TRUE;
              (*mods)++;
              NAME (m) = make_name_row (SUB_SUB (m), topmoid);
            }
          }
        }
      }
    }
    make_stowed_names_tree (SUB (p), mods);
  }
}

/*!
\brief make multiple row pack
\param src source pack
\param dst destination pack with REF modes
\param p chain to insert new modes into
\param dim dimension
**/

static void make_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NULL) {
    make_multiple_row_pack (NEXT (src), dst, p, dim);
    (void) add_mode_to_pack (dst, add_row (p, dim, MOID (src), NULL), TEXT (src), NODE (src));
  }
}

/*!
\brief make multiple struct
\param m structured mode
\param p chain to insert new modes into
\param dim dimension
\return mode table entry
**/

static MOID_T *make_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  MOID_T *save;
  PACK_T *u = NULL;
  (void) add_mode (p, STRUCT_SYMBOL, DIM (m), NULL, NULL, NULL);
  save = *p;
  make_multiple_row_pack (PACK (m), &u, p, dim);
  PACK (save) = u;
  return (save);
}

/*!
\brief make flex multiple row pack
\param src source pack
\param dst destination pack with REF modes
\param p chain to insert new modes into
\param dim dimension
**/

static void make_flex_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NULL) {
    MOID_T *z;
    make_flex_multiple_row_pack (NEXT (src), dst, p, dim);
    z = add_row (p, dim, MOID (src), NULL);
    z = add_mode (p, FLEX_SYMBOL, 0, NULL, z, NULL);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

/*!
\brief make flex multiple struct
\param m structured mode
\param p chain to insert new modes into
\param dim dimension
\return mode table entry
**/

static MOID_T *make_flex_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  MOID_T *x;
  PACK_T *u = NULL;
  (void) add_mode (p, STRUCT_SYMBOL, DIM (m), NULL, NULL, NULL);
  x = *p;
  make_flex_multiple_row_pack (PACK (m), &u, p, dim);
  PACK (x) = u;
  return (x);
}

/*!
\brief make multiple modes
\param p position in tree
\param mods modification count
**/

static void make_multiple_modes_tree (NODE_T * p, int *mods)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *symbol_table = SYMBOL_TABLE (SUB (p));
      MOID_T **top = &symbol_table->moids;
      BOOL_T z = A68_TRUE;
      while (z) {
        MOID_T *q = symbol_table->moids;
        z = A68_FALSE;
        for (; q != NULL; FORWARD (q)) {
          if (MULTIPLE (q) != NULL) {
            ;
          } else if (WHETHER (q, REF_SYMBOL)) {
            if (MULTIPLE (SUB (q)) != NULL) {
              (*mods)++;
              MULTIPLE (q) = make_name_struct (MULTIPLE (SUB (q)), top);
            }
          } else if (WHETHER (q, ROW_SYMBOL)) {
            if (WHETHER (SUB (q), STRUCT_SYMBOL)) {
              z = A68_TRUE;
              (*mods)++;
              MULTIPLE (q) = make_multiple_struct (SUB (q), top, DIM (q));
            }
          } else if (WHETHER (q, FLEX_SYMBOL)) {
            if (SUB_SUB (q) == NULL) {
              (*mods)++;        /* as yet unresolved FLEX INDICANT */
            } else {
              if (WHETHER (SUB_SUB (q), STRUCT_SYMBOL)) {
                z = A68_TRUE;
                (*mods)++;
                MULTIPLE (q) = make_flex_multiple_struct (SUB_SUB (q), top, DIM (SUB (q)));
              }
            }
          }
        }
      }
    }
    make_multiple_modes_tree (SUB (p), mods);
  }
}

/*!
\brief make multiple modes in standard environ
\param mods modification count
**/

static void make_multiple_modes_standenv (int *mods)
{
  MOID_T **top = &stand_env->moids;
  BOOL_T z = A68_TRUE;
  while (z) {
    MOID_T *q = stand_env->moids;
    z = A68_FALSE;
    for (; q != NULL; FORWARD (q)) {
      if (MULTIPLE (q) != NULL) {
        ;
      } else if (WHETHER (q, REF_SYMBOL)) {
        if (MULTIPLE (SUB (q)) != NULL) {
          (*mods)++;
          MULTIPLE (q) = make_name_struct (MULTIPLE (SUB (q)), top);
        }
      } else if (WHETHER (q, ROW_SYMBOL)) {
        if (WHETHER (SUB (q), STRUCT_SYMBOL)) {
          z = A68_TRUE;
          (*mods)++;
          MULTIPLE (q) = make_multiple_struct (SUB (q), top, DIM (q));
        }
      } else if (WHETHER (q, FLEX_SYMBOL)) {
        if (SUB_SUB (q) == NULL) {
          (*mods)++;            /* as yet unresolved FLEX INDICANT */
        } else {
          if (WHETHER (SUB_SUB (q), STRUCT_SYMBOL)) {
            z = A68_TRUE;
            (*mods)++;
            MULTIPLE (q) = make_flex_multiple_struct (SUB_SUB (q), top, DIM (SUB (q)));
          }
        }
      }
    }
  }
}

/*
Deflexing removes all FLEX from a mode,
for instance REF STRING -> REF [] CHAR.
*/

/*!
\brief whether mode has flex 2
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_flex_2 (MOID_T * m)
{
  if (whether_postulated (top_postulate, m)) {
    return (A68_FALSE);
  } else {
    make_postulate (&top_postulate, m, NULL);
    if (WHETHER (m, FLEX_SYMBOL)) {
      return (A68_TRUE);
    } else if (WHETHER (m, REF_SYMBOL)) {
      return (whether_mode_has_flex_2 (SUB (m)));
    } else if (WHETHER (m, PROC_SYMBOL)) {
      return (whether_mode_has_flex_2 (SUB (m)));
    } else if (WHETHER (m, ROW_SYMBOL)) {
      return (whether_mode_has_flex_2 (SUB (m)));
    } else if (WHETHER (m, STRUCT_SYMBOL)) {
      PACK_T *t = PACK (m);
      BOOL_T z = A68_FALSE;
      for (; t != NULL && !z; FORWARD (t)) {
        z |= whether_mode_has_flex_2 (MOID (t));
      }
      return (z);
    } else {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether mode has flex
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_flex (MOID_T * m)
{
  free_postulate_list (top_postulate, NULL);
  top_postulate = NULL;
  return (whether_mode_has_flex_2 (m));
}

/*!
\brief make deflexed pack
\param src source pack
\param dst destination pack with REF modes
\param p chain to insert new modes into
**/

static void make_deflexed_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p)
{
  if (src != NULL) {
    make_deflexed_pack (NEXT (src), dst, p);
    (void) add_mode_to_pack (dst, make_deflexed (MOID (src), p), TEXT (src), NODE (src));
  }
}

/*!
\brief make deflexed
\param m structured mode
\param p chain to insert new modes into
\return mode table entry
**/

static MOID_T *make_deflexed (MOID_T * m, MOID_T ** p)
{
  if (DEFLEXED (m) != NULL) {   /* Keep this condition on top */
    return (DEFLEXED (m));
  }
  if (WHETHER (m, REF_SYMBOL)) {
    MOID_T *new_one = make_deflexed (SUB (m), p);
    (void) add_mode (p, REF_SYMBOL, DIM (m), NULL, new_one, NULL);
    SUB (*p) = new_one;
    return (DEFLEXED (m) = *p);
  } else if (WHETHER (m, PROC_SYMBOL)) {
    MOID_T *save, *new_one;
    (void) add_mode (p, PROC_SYMBOL, DIM (m), NULL, NULL, PACK (m));
    save = *p;
/* Mark to prevent eventual cyclic references */
    DEFLEXED (m) = save;
    new_one = make_deflexed (SUB (m), p);
    SUB (save) = new_one;
    return (save);
  } else if (WHETHER (m, FLEX_SYMBOL)) {
    ABEND (SUB (m) == NULL, "NULL mode while deflexing", NULL);
    DEFLEXED (m) = make_deflexed (SUB (m), p);
    return (DEFLEXED (m));
  } else if (WHETHER (m, ROW_SYMBOL)) {
    MOID_T *new_sub, *new_slice;
    if (DIM (m) > 1) {
      new_slice = make_deflexed (SLICE (m), p);
      (void) add_mode (p, ROW_SYMBOL, DIM (m) - 1, NULL, new_slice, NULL);
      new_sub = make_deflexed (SUB (m), p);
    } else {
      new_sub = make_deflexed (SUB (m), p);
      new_slice = new_sub;
    }
    (void) add_mode (p, ROW_SYMBOL, DIM (m), NULL, new_sub, NULL);
    SLICE (*p) = new_slice;
    return (DEFLEXED (m) = *p);
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
    MOID_T *save;
    PACK_T *u = NULL;
    (void) add_mode (p, STRUCT_SYMBOL, DIM (m), NULL, NULL, NULL);
    save = *p;
/* Mark to prevent eventual cyclic references */
    DEFLEXED (m) = save;
    make_deflexed_pack (PACK (m), &u, p);
    PACK (save) = u;
    return (save);
  } else if (WHETHER (m, INDICANT)) {
    MOID_T *n = EQUIVALENT (m);
    ABEND (n == NULL, "NULL equivalent mode while deflexing", NULL);
    return (DEFLEXED (m) = make_deflexed (n, p));
  } else if (WHETHER (m, STANDARD)) {
    MOID_T *n = (DEFLEXED (m) != NULL ? DEFLEXED (m) : m);
    return (n);
  } else {
    return (m);
  }
}

/*!
\brief make deflexed modes
\param p position in tree
\param mods modification count
**/

static void make_deflexed_modes_tree (NODE_T * p, int *mods)
{
  for (; p != NULL; FORWARD (p)) {
/* Dive into lexical levels */
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      SYMBOL_TABLE_T *s = SYMBOL_TABLE (SUB (p));
      MOID_T *m, **top = &s->moids;
      for (m = s->moids; m != NULL; FORWARD (m)) {
/* 'Complete' deflexing */
        if (!m->has_flex) {
          m->has_flex = whether_mode_has_flex (m);
        }
        if (m->has_flex && DEFLEXED (m) == NULL) {
          (*mods)++;
          DEFLEXED (m) = make_deflexed (m, top);
          ABEND (whether_mode_has_flex (DEFLEXED (m)), "deflexing failed", moid_to_string (DEFLEXED (m), MOID_WIDTH, NULL));
        }
/* 'Light' deflexing needed for trims */
        if (TRIM (m) == NULL && WHETHER (m, FLEX_SYMBOL)) {
          (*mods)++;
          TRIM (m) = SUB (m);
        } else if (TRIM (m) == NULL && WHETHER (m, REF_SYMBOL) && WHETHER (SUB (m), FLEX_SYMBOL)) {
          (*mods)++;
          (void) add_mode (top, REF_SYMBOL, DIM (m), NULL, SUB_SUB (m), NULL);
          TRIM (m) = *top;
        }
      }
    }
    make_deflexed_modes_tree (SUB (p), mods);
  }
}

/*!
\brief make extra rows local, rows with one extra dimension
\param s symbol table
**/

static void make_extra_rows_local (SYMBOL_TABLE_T * s)
{
  MOID_T *m, **top = &(s->moids);
  for (m = s->moids; m != NULL; FORWARD (m)) {
    if (WHETHER (m, ROW_SYMBOL) && DIM (m) > 0 && SUB (m) != NULL) {
      (void) add_row (top, DIM (m) + 1, SUB (m), NODE (m));
    } else if (WHETHER (m, REF_SYMBOL) && WHETHER (SUB (m), ROW_SYMBOL)) {
      MOID_T *z = add_row (top, DIM (SUB (m)) + 1, SUB_SUB (m), NODE (SUB (m)));
      MOID_T *y = add_mode (top, REF_SYMBOL, 0, NODE (m), z, NULL);
      NAME (y) = m;
    }
  }
}

/*!
\brief make extra rows
\param p position in tree
**/

static void make_extra_rows_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
/* Dive into lexical levels */
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      make_extra_rows_local (SYMBOL_TABLE (SUB (p)));
    }
    make_extra_rows_tree (SUB (p));
  }
}

/*!
\brief whether mode has ref 2
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_ref_2 (MOID_T * m)
{
  if (whether_postulated (top_postulate, m)) {
    return (A68_FALSE);
  } else {
    make_postulate (&top_postulate, m, NULL);
    if (WHETHER (m, FLEX_SYMBOL)) {
      return (whether_mode_has_ref_2 (SUB (m)));
    } else if (WHETHER (m, REF_SYMBOL)) {
      return (A68_TRUE);
    } else if (WHETHER (m, ROW_SYMBOL)) {
      return (whether_mode_has_ref_2 (SUB (m)));
    } else if (WHETHER (m, STRUCT_SYMBOL)) {
      PACK_T *t = PACK (m);
      BOOL_T z = A68_FALSE;
      for (; t != NULL && !z; FORWARD (t)) {
        z |= whether_mode_has_ref_2 (MOID (t));
      }
      return (z);
    } else {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether mode has ref
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_ref (MOID_T * m)
{
  free_postulate_list (top_postulate, NULL);
  top_postulate = NULL;
  return (whether_mode_has_ref_2 (m));
}

/* Routines setting properties of modes */

/*!
\brief reset moid
\param p position in tree
**/

static void reset_moid_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    MOID (p) = NULL;
    reset_moid_tree (SUB (p));
  }
}

/*!
\brief renumber moids
\param p moid list
\return moids renumbered
**/

static int renumber_moids (MOID_LIST_T * p)
{
  if (p == NULL) {
    return (1);
  } else {
    return (1 + (NUMBER (MOID (p)) = renumber_moids (NEXT (p))));
  }
}

/*!
\brief whether mode has row
\param m mode under test
\return same
**/

static BOOL_T whether_mode_has_row (MOID_T * m)
{
  if (WHETHER (m, STRUCT_SYMBOL) || WHETHER (m, UNION_SYMBOL)) {
    BOOL_T k = A68_FALSE;
    PACK_T *p = PACK (m);
    for (; p != NULL && k == A68_FALSE; FORWARD (p)) {
      MOID (p)->has_rows = whether_mode_has_row (MOID (p));
      k |= (MOID (p)->has_rows);
    }
    return (k);
  } else {
    return ((BOOL_T) (m->has_rows || WHETHER (m, ROW_SYMBOL) || WHETHER (m, FLEX_SYMBOL)));
  }
}

/*!
\brief mark row modes
\param p position in tree
**/

static void mark_row_modes_tree (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
/* Dive into lexical levels */
    if (SUB (p) != NULL && whether_new_lexical_level (p)) {
      MOID_T *m;
      for (m = SYMBOL_TABLE (SUB (p))->moids; m != NULL; FORWARD (m)) {
        m->has_rows = whether_mode_has_row (m);
      }
    }
    mark_row_modes_tree (SUB (p));
  }
}

/*!
\brief set moid attributes
\param q moid in list where to start
**/

static void set_moid_attributes (MOID_LIST_T * q)
{
  for (; q != NULL; FORWARD (q)) {
    MOID_T *z = MOID (q);
    if (z->has_ref == A68_FALSE) {
      z->has_ref = whether_mode_has_ref (z);
    }
    if (z->has_flex == A68_FALSE) {
      z->has_flex = whether_mode_has_flex (z);
    }
    if (WHETHER (z, ROW_SYMBOL) && SLICE (z) != NULL) {
      ROWED (SLICE (z)) = z;
      track_equivalent_modes (&(ROWED (SLICE (z))));
    }
    if (WHETHER (z, REF_SYMBOL)) {
      MOID_T *y = SUB (z);
      if (SLICE (y) != NULL && WHETHER (SLICE (y), ROW_SYMBOL) && NAME (z) != NULL) {
        ROWED (NAME (z)) = z;
        track_equivalent_modes (&(ROWED (NAME (z))));
      }
    }
  }
}

/*!
\brief get moid list
\param top_moid_list top of moid list
\param top_node top node in tree
**/

void get_moid_list (MOID_LIST_T ** loc_top_moid_list, NODE_T * top_node)
{
  reset_moid_list ();
  add_moids_from_table (loc_top_moid_list, stand_env);
  add_moids_from_table_tree (top_node, loc_top_moid_list);
}

/*!
\brief construct moid list by expansion and contraction
\param top_node top node in tree
\param cycle_no cycle number
\return number of modifications
**/

static int expand_contract_moids (NODE_T * top_node, int cycle_no)
{
  int mods = 0;
  free_postulate_list (top_postulate, NULL);
  top_postulate = NULL;
  if (cycle_no >= 0) {          /* Experimental */
/* Calculate derived modes */
    make_multiple_modes_standenv (&mods);
    absorb_unions_tree (top_node, &mods);
    contract_unions_tree (top_node, &mods);
    make_multiple_modes_tree (top_node, &mods);
    make_stowed_names_tree (top_node, &mods);
    make_deflexed_modes_tree (top_node, &mods);
  }
/* Calculate equivalent modes */
  get_moid_list (&top_moid_list, top_node);
  bind_indicants_to_modes_tree (top_node);
  free_postulate_list (top_postulate, NULL);
  top_postulate = NULL;
  find_equivalent_moids (top_moid_list, NULL);
  track_equivalent_tree (top_node);
  track_equivalent_tags (stand_env->indicants);
  track_equivalent_tags (stand_env->identifiers);
  track_equivalent_tags (stand_env->operators);
  moid_list_track_equivalent (stand_env->moids);
  contract_unions_tree (top_node, &mods);
  set_moid_attributes (top_moid_list);
  track_equivalent_tree (top_node);
  track_equivalent_tags (stand_env->indicants);
  track_equivalent_tags (stand_env->identifiers);
  track_equivalent_tags (stand_env->operators);
  set_moid_sizes (top_moid_list);
  return (mods);
}

/*!
\brief maintain mode table
\param p position in tree
**/

void maintain_mode_table (NODE_T * p)
{
  (void) p;
  (void) renumber_moids (top_moid_list);
}

/*!
\brief make list of all modes in the program
\param top_node top node in tree
**/

void set_up_mode_table (NODE_T * top_node)
{
  reset_moid_tree (top_node);
  get_modes_from_tree (top_node, NULL_ATTRIBUTE);
  get_mode_from_proc_var_declarations_tree (top_node);
  make_extra_rows_local (stand_env);
  make_extra_rows_tree (top_node);
/* Tie MODE declarations to their respective a68_modes .. */
  bind_indicants_to_tags_tree (top_node);
  bind_indicants_to_modes_tree (top_node);
/* ... and check for cyclic definitions as MODE A = B, B = C, C = A */
  check_cyclic_modes_tree (top_node);
  check_flex_modes_tree (top_node);
  if (program.error_count == 0) {
/* Check yin-yang of modes */
    free_postulate_list (top_postulate, NULL);
    top_postulate = NULL;
    check_well_formedness_tree (top_node);
/* Construct the full moid list */
    if (program.error_count == 0) {
      int cycle = 0;
      track_equivalent_standard_modes ();
      while (expand_contract_moids (top_node, cycle) > 0 || cycle < 16) {
        ABEND (cycle++ > 32, "apparently indefinite loop in set_up_mode_table", NULL);
      }
/* Set standard modes */
      track_equivalent_standard_modes ();
/* Postlude */
      check_relation_to_void_tree (top_node);
      mark_row_modes_tree (top_node);
    }
  }
  init_postulates ();
}

/* Next are routines to calculate the size of a mode */

/*!
\brief reset max simplout size
**/

void reset_max_simplout_size (void)
{
  max_simplout_size = 0;
}

/*!
\brief max unitings to simplout
\param p position in tree
\param max maximum calculated moid size
**/

static void max_unitings_to_simplout (NODE_T * p, int *max)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNITING) && MOID (p) == MODE (SIMPLOUT)) {
      MOID_T *q = MOID (SUB (p));
      if (q != MODE (SIMPLOUT)) {
        int size = moid_size (q);
        if (size > *max) {
          *max = size;
        }
      }
    }
    max_unitings_to_simplout (SUB (p), max);
  }
}

/*!
\brief get max simplout size
\param p position in tree
**/

void get_max_simplout_size (NODE_T * p)
{
  max_simplout_size = 0;
  max_unitings_to_simplout (p, &max_simplout_size);
}

/*!
\brief set moid sizes
\param start moid to start from
**/

void set_moid_sizes (MOID_LIST_T * start)
{
  for (; start != NULL; FORWARD (start)) {
    SIZE (MOID (start)) = moid_size (MOID (start));
  }
}

/*!
\brief moid size 2
\param p moid to calculate
\return moid size
**/

static int moid_size_2 (MOID_T * p)
{
  if (p == NULL) {
    return (0);
  } else if (EQUIVALENT (p) != NULL) {
    return (moid_size_2 (EQUIVALENT (p)));
  } else if (p == MODE (HIP)) {
    return (0);
  } else if (p == MODE (VOID)) {
    return (0);
  } else if (p == MODE (INT)) {
    return (ALIGNED_SIZE_OF (A68_INT));
  } else if (p == MODE (LONG_INT)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_INT)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (REAL)) {
    return (ALIGNED_SIZE_OF (A68_REAL));
  } else if (p == MODE (LONG_REAL)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_REAL)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (BOOL)) {
    return (ALIGNED_SIZE_OF (A68_BOOL));
  } else if (p == MODE (CHAR)) {
    return (ALIGNED_SIZE_OF (A68_CHAR));
  } else if (p == MODE (ROW_CHAR)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (BITS)) {
    return (ALIGNED_SIZE_OF (A68_BITS));
  } else if (p == MODE (LONG_BITS)) {
    return ((int) size_long_mp ());
  } else if (p == MODE (LONGLONG_BITS)) {
    return ((int) size_longlong_mp ());
  } else if (p == MODE (BYTES)) {
    return (ALIGNED_SIZE_OF (A68_BYTES));
  } else if (p == MODE (LONG_BYTES)) {
    return (ALIGNED_SIZE_OF (A68_LONG_BYTES));
  } else if (p == MODE (FILE)) {
    return (ALIGNED_SIZE_OF (A68_FILE));
  } else if (p == MODE (CHANNEL)) {
    return (ALIGNED_SIZE_OF (A68_CHANNEL));
  } else if (p == MODE (FORMAT)) {
    return (ALIGNED_SIZE_OF (A68_FORMAT));
  } else if (p == MODE (SEMA)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (SOUND)) {
    return (ALIGNED_SIZE_OF (A68_SOUND));
  } else if (p == MODE (COLLITEM)) {
    return (ALIGNED_SIZE_OF (A68_COLLITEM));
  } else if (p == MODE (NUMBER)) {
    int k = 0;
    if (ALIGNED_SIZE_OF (A68_INT) > k) {
      k = ALIGNED_SIZE_OF (A68_INT);
    }
    if ((int) size_long_mp () > k) {
      k = (int) size_long_mp ();
    }
    if ((int) size_longlong_mp () > k) {
      k = (int) size_longlong_mp ();
    }
    if (ALIGNED_SIZE_OF (A68_REAL) > k) {
      k = ALIGNED_SIZE_OF (A68_REAL);
    }
    if ((int) size_long_mp () > k) {
      k = (int) size_long_mp ();
    }
    if ((int) size_longlong_mp () > k) {
      k = (int) size_longlong_mp ();
    }
    if (ALIGNED_SIZE_OF (A68_REF) > k) {
      k = ALIGNED_SIZE_OF (A68_REF);
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + k);
  } else if (p == MODE (SIMPLIN)) {
    int k = 0;
    if (ALIGNED_SIZE_OF (A68_REF) > k) {
      k = ALIGNED_SIZE_OF (A68_REF);
    }
    if (ALIGNED_SIZE_OF (A68_FORMAT) > k) {
      k = ALIGNED_SIZE_OF (A68_FORMAT);
    }
    if (ALIGNED_SIZE_OF (A68_PROCEDURE) > k) {
      k = ALIGNED_SIZE_OF (A68_PROCEDURE);
    }
    if (ALIGNED_SIZE_OF (A68_SOUND) > k) {
      k = ALIGNED_SIZE_OF (A68_SOUND);
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + k);
  } else if (p == MODE (SIMPLOUT)) {
    return (ALIGNED_SIZE_OF (A68_UNION) + max_simplout_size);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (WHETHER (p, PROC_SYMBOL)) {
    return (ALIGNED_SIZE_OF (A68_PROCEDURE));
  } else if (WHETHER (p, ROW_SYMBOL) && p != MODE (ROWS)) {
    return (ALIGNED_SIZE_OF (A68_REF));
  } else if (p == MODE (ROWS)) {
    return (ALIGNED_SIZE_OF (A68_UNION) + ALIGNED_SIZE_OF (A68_REF));
  } else if (WHETHER (p, FLEX_SYMBOL)) {
    return moid_size (SUB (p));
  } else if (WHETHER (p, STRUCT_SYMBOL)) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NULL; FORWARD (z)) {
      size += moid_size (MOID (z));
    }
    return (size);
  } else if (WHETHER (p, UNION_SYMBOL)) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NULL; FORWARD (z)) {
      if (moid_size (MOID (z)) > size) {
        size = moid_size (MOID (z));
      }
    }
    return (ALIGNED_SIZE_OF (A68_UNION) + size);
  } else if (PACK (p) != NULL) {
    PACK_T *z = PACK (p);
    int size = 0;
    for (; z != NULL; FORWARD (z)) {
      size += moid_size (MOID (z));
    }
    return (size);
  } else {
/* ? */
    return (0);
  }
}

/*!
\brief moid size
\param p moid to set size
\return moid size
**/

int moid_size (MOID_T * p)
{
  SIZE (p) = moid_size_2 (p);
  return (SIZE (p));
}

/* A pretty printer for moids */

/*!
\brief moid to string 3
\param dst text buffer
\param str string to concatenate
\param w estimated width
\param idf print indicants if one exists in this range
**/

static void add_to_moid_text (char *dst, char *str, int *w)
{
  bufcat (dst, str, BUFFER_SIZE);
  (*w) -= (int) strlen (str);
}

/*!
\brief find a tag, searching symbol tables towards the root
\param table symbol table to search
\param mode mode of the tag
\return entry in symbol table
**/

TAG_T *find_indicant_global (SYMBOL_TABLE_T * table, MOID_T * mode)
{
  if (table != NULL) {
    TAG_T *s;
    for (s = table->indicants; s != NULL; FORWARD (s)) {
      if (MOID (s) == mode) {
        return (s);
      }
    }
    return (find_indicant_global (PREVIOUS (table), mode));
  } else {
    return (NULL);
  }
}

/*!
\brief pack to string
\param b text buffer
\param p pack
\param w estimated width
\param text include field names
\param idf print indicants if one exists in this range
**/

static void pack_to_string (char *b, PACK_T * p, int *w, BOOL_T text, NODE_T * idf)
{
  for (; p != NULL; FORWARD (p)) {
    moid_to_string_2 (b, MOID (p), w, idf);
    if (text) {
      if (TEXT (p) != NULL) {
        add_to_moid_text (b, " ", w);
        add_to_moid_text (b, TEXT (p), w);
      }
    }
    if (p != NULL && NEXT (p) != NULL) {
      add_to_moid_text (b, ", ", w);
    }
  }
}

/*!
\brief moid to string 2
\param b text buffer
\param n moid
\param w estimated width
\param idf print indicants if one exists in this range
**/

static void moid_to_string_2 (char *b, MOID_T * n, int *w, NODE_T * idf)
{
/* Oops. Should not happen */
  if (n == NULL) {
    add_to_moid_text (b, "NULL", w);;
    return;
  }
/* Reference to self through REF or PROC */
  if (whether_postulated (postulates, n)) {
    add_to_moid_text (b, "SELF", w);
    return;
  }
/* If declared by a mode-declaration, present the indicant */
  if (idf != NULL) {
    TAG_T *indy = find_indicant_global (SYMBOL_TABLE (idf), n);
    if (indy != NULL) {
      add_to_moid_text (b, SYMBOL (NODE (indy)), w);
      return;
    }
  }
/* Write the standard modes */
  if (n == MODE (HIP)) {
    add_to_moid_text (b, "HIP", w);
  } else if (n == MODE (ERROR)) {
    add_to_moid_text (b, "ERROR", w);
  } else if (n == MODE (UNDEFINED)) {
    add_to_moid_text (b, "unresolved", w);
  } else if (n == MODE (C_STRING)) {
    add_to_moid_text (b, "C-STRING", w);
  } else if (n == MODE (COMPLEX) || n == MODE (COMPL)) {
    add_to_moid_text (b, "COMPLEX", w);
  } else if (n == MODE (LONG_COMPLEX) || n == MODE (LONG_COMPL)) {
    add_to_moid_text (b, "LONG COMPLEX", w);
  } else if (n == MODE (LONGLONG_COMPLEX) || n == MODE (LONGLONG_COMPL)) {
    add_to_moid_text (b, "LONG LONG COMPLEX", w);
  } else if (n == MODE (STRING)) {
    add_to_moid_text (b, "STRING", w);
  } else if (n == MODE (PIPE)) {
    add_to_moid_text (b, "PIPE", w);
  } else if (n == MODE (SOUND)) {
    add_to_moid_text (b, "SOUND", w);
  } else if (n == MODE (COLLITEM)) {
    add_to_moid_text (b, "COLLITEM", w);
  } else if (WHETHER (n, IN_TYPE_MODE)) {
    add_to_moid_text (b, "\"SIMPLIN\"", w);
  } else if (WHETHER (n, OUT_TYPE_MODE)) {
    add_to_moid_text (b, "\"SIMPLOUT\"", w);
  } else if (WHETHER (n, ROWS_SYMBOL)) {
    add_to_moid_text (b, "\"ROWS\"", w);
  } else if (n == MODE (VACUUM)) {
    add_to_moid_text (b, "\"VACUUM\"", w);
  } else if (WHETHER (n, VOID_SYMBOL) || WHETHER (n, STANDARD) || WHETHER (n, INDICANT)) {
    if (DIM (n) > 0) {
      int k = DIM (n);
      if ((*w) >= k * (int) strlen ("LONG ") + (int) strlen (SYMBOL (NODE (n)))) {
        while (k --) {
          add_to_moid_text (b, "LONG ", w);
        }
        add_to_moid_text (b, SYMBOL (NODE (n)), w);
      } else {
        add_to_moid_text (b, "..", w);
      }
    } else if (DIM (n) < 0) {
      int k = -DIM (n);
      if ((*w) >= k * (int) strlen ("LONG ") + (int) strlen (SYMBOL (NODE (n)))) {
        while (k --) {
          add_to_moid_text (b, "LONG ", w);
        }
        add_to_moid_text (b, SYMBOL (NODE (n)), w);
      } else {
        add_to_moid_text (b, "..", w);
      }
    } else if (DIM (n) == 0) {
      add_to_moid_text (b, SYMBOL (NODE (n)), w);
    }
/* Write compounded modes */
  } else if (WHETHER (n, REF_SYMBOL)) {
    if ((*w) >= (int) strlen ("REF ..")) {
      add_to_moid_text (b, "REF ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "REF ..", w);
    }
  } else if (WHETHER (n, FLEX_SYMBOL)) {
    if ((*w) >= (int) strlen ("FLEX ..")) {
      add_to_moid_text (b, "FLEX ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "FLEX ..", w);
    }
  } else if (WHETHER (n, ROW_SYMBOL)) {
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
  } else if (WHETHER (n, STRUCT_SYMBOL)) {
    int j = (int) strlen ("STRUCT ()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NULL);
      add_to_moid_text (b, "STRUCT (", w);
      pack_to_string (b, PACK (n), w, A68_TRUE, idf);
      add_to_moid_text (b, ")", w);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "STRUCT (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else if (WHETHER (n, UNION_SYMBOL)) {
    int j = (int) strlen ("UNION ()") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NULL);
      add_to_moid_text (b, "UNION (", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ")", w);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "UNION (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ")", w);
    }
  } else if (WHETHER (n, PROC_SYMBOL) && DIM (n) == 0) {
    if ((*w) >= (int) strlen ("PROC ..")) {
      add_to_moid_text (b, "PROC ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
    } else {
      add_to_moid_text (b, "PROC ..", w);
    }
  } else if (WHETHER (n, PROC_SYMBOL) && DIM (n) > 0) {
    int j = (int) strlen ("PROC () ..") + (DIM (n) - 1) * (int) strlen (".., ") + (int) strlen ("..");
    if ((*w) >= j) {
      POSTULATE_T *save = postulates;
      make_postulate (&postulates, n, NULL);
      add_to_moid_text (b, "PROC (", w);
      pack_to_string (b, PACK (n), w, A68_FALSE, idf);
      add_to_moid_text (b, ") ", w);
      moid_to_string_2 (b, SUB (n), w, idf);
      free_postulate_list (postulates, save);
      postulates = save;
    } else {
      int k = DIM (n);
      add_to_moid_text (b, "PROC (", w);
      while (k-- > 0) {
        add_to_moid_text (b, ",", w);
      }
      add_to_moid_text (b, ") ..", w);
    }
  } else if (WHETHER (n, SERIES_MODE) || WHETHER (n, STOWED_MODE)) {
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

/*!
\brief pretty-formatted mode "n"; "w" is a measure of width
\param n moid
\param w estimated width; if w is exceeded, modes are abbreviated
\return text buffer
**/

char *moid_to_string (MOID_T * n, int w, NODE_T * idf)
{
  char a[BUFFER_SIZE];
  a[0] = NULL_CHAR;
  if (w >= BUFFER_SIZE) {
    w = BUFFER_SIZE - 1;
  }
  postulates = NULL;
  if (n != NULL) {
    moid_to_string_2 (a, n, &w, idf);
  } else {
    bufcat (a, "NULL", BUFFER_SIZE);
  }
  return (new_string (a));
}

/* Static scope checker */

/*
Static scope checker. 
Also a little preparation for the monitor:
- indicates UNITs that can be interrupted.
*/

typedef struct TUPLE_T TUPLE_T;
typedef struct SCOPE_T SCOPE_T;

struct TUPLE_T
{
  int level;
  BOOL_T transient;
};

struct SCOPE_T
{
  NODE_T *where;
  TUPLE_T tuple;
  SCOPE_T *next;
};

enum
{ NOT_TRANSIENT = 0, TRANSIENT };

static void gather_scopes_for_youngest (NODE_T *, SCOPE_T **);
static void scope_statement (NODE_T *, SCOPE_T **);
static void scope_enclosed_clause (NODE_T *, SCOPE_T **);
static void scope_formula (NODE_T *, SCOPE_T **);
static void scope_routine_text (NODE_T *, SCOPE_T **);

/*!
\brief scope_make_tuple
\param e level
\param t whether transient
\return tuple (e, t)
**/

static TUPLE_T scope_make_tuple (int e, int t)
{
  static TUPLE_T z;
  z.level = e;
  z.transient = (BOOL_T) t;
  return (z);
}

/*!
\brief link scope information into the list
\param sl chain to link into
\param p position in tree
\param tup tuple to link
**/

static void scope_add (SCOPE_T ** sl, NODE_T * p, TUPLE_T tup)
{
  if (sl != NULL) {
    SCOPE_T *ns = (SCOPE_T *) get_temp_heap_space ((unsigned) ALIGNED_SIZE_OF (SCOPE_T));
    ns->where = p;
    ns->tuple = tup;
    NEXT (ns) = *sl;
    *sl = ns;
  }
}

/*!
\brief scope_check
\param top top of scope chain
\param mask what to check
\param dest level to check against
\return whether errors were detected
**/

static BOOL_T scope_check (SCOPE_T * top, int mask, int dest)
{
  SCOPE_T *s;
  int errors = 0;
/* Transient names cannot be stored */
  if (mask & TRANSIENT) {
    for (s = top; s != NULL; FORWARD (s)) {
      if (s->tuple.transient & TRANSIENT) {
        diagnostic_node (A68_ERROR, s->where, ERROR_TRANSIENT_NAME);
        STATUS_SET (s->where, SCOPE_ERROR_MASK);
        errors++;
      }
    }
  }
  for (s = top; s != NULL; FORWARD (s)) {
    if (dest < s->tuple.level && ! STATUS_TEST (s->where, SCOPE_ERROR_MASK)) {
/* Potential scope violations */
      if (MOID (s->where) == NULL) {
        diagnostic_node (A68_WARNING, s->where, WARNING_SCOPE_STATIC_1, ATTRIBUTE (s->where));
      } else {
        diagnostic_node (A68_WARNING, s->where, WARNING_SCOPE_STATIC_2, MOID (s->where), ATTRIBUTE (s->where));
      }
      STATUS_SET (s->where, SCOPE_ERROR_MASK);
      errors++;
    }
  }
  return ((BOOL_T) (errors == 0));
}

/*!
\brief scope_check_multiple
\param top top of scope chain
\param mask what to check
\param dest level to check against
\return whether error
**/

static BOOL_T scope_check_multiple (SCOPE_T * top, int mask, SCOPE_T * dest)
{
  BOOL_T no_err = A68_TRUE;
  for (; dest != NULL; FORWARD (dest)) {
    no_err &= scope_check (top, mask, dest->tuple.level);
  }
  return (no_err);
}

/*!
\brief check_identifier_usage
\param t tag
\param p position in tree
**/

static void check_identifier_usage (TAG_T * t, NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, IDENTIFIER) && TAX (p) == t && ATTRIBUTE (MOID (t)) != PROC_SYMBOL) {
      diagnostic_node (A68_WARNING, p, WARNING_UNINITIALISED);
    }
    check_identifier_usage (t, SUB (p));
  }
}

/*!
\brief scope_find_youngest_outside
\param s chain to link into
\param treshold threshold level
\return youngest tuple outside
**/

static TUPLE_T scope_find_youngest_outside (SCOPE_T * s, int treshold)
{
  TUPLE_T z = scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT);
  for (; s != NULL; FORWARD (s)) {
    if (s->tuple.level > z.level && s->tuple.level <= treshold) {
      z = s->tuple;
    }
  }
  return (z);
}

/*!
\brief scope_find_youngest
\param s chain to link into
\return youngest tuple outside
**/

static TUPLE_T scope_find_youngest (SCOPE_T * s)
{
  return (scope_find_youngest_outside (s, A68_MAX_INT));
}

/* Routines for determining scope of ROUTINE TEXT or FORMAT TEXT */

/*!
\brief get_declarer_elements
\param p position in tree
\param r chain to link into
\param no_ref whether no REF seen yet
**/

static void get_declarer_elements (NODE_T * p, SCOPE_T ** r, BOOL_T no_ref)
{
  if (p != NULL) {
    if (WHETHER (p, BOUNDS)) {
      gather_scopes_for_youngest (SUB (p), r);
    } else if (WHETHER (p, INDICANT)) {
      if (MOID (p) != NULL && TAX (p) != NULL && MOID (p)->has_rows && no_ref) {
        scope_add (r, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
      }
    } else if (WHETHER (p, REF_SYMBOL)) {
      get_declarer_elements (NEXT (p), r, A68_FALSE);
    } else if (whether_one_of (p, PROC_SYMBOL, UNION_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else {
      get_declarer_elements (SUB (p), r, no_ref);
      get_declarer_elements (NEXT (p), r, no_ref);
    }
  }
}

/*!
\brief gather_scopes_for_youngest
\param p position in tree
\param s chain to link into
**/

static void gather_scopes_for_youngest (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if ((whether_one_of (p, ROUTINE_TEXT, FORMAT_TEXT, NULL_ATTRIBUTE))
        && (TAX (p)->youngest_environ == PRIMAL_SCOPE)) {
      SCOPE_T *t = NULL;
      gather_scopes_for_youngest (SUB (p), &t);
      TAX (p)->youngest_environ = scope_find_youngest_outside (t, LEX_LEVEL (p)).level;
/* Direct link into list iso "gather_scopes_for_youngest (SUB (p), s);" */
      if (t != NULL) {
        SCOPE_T *u = t;
        while (NEXT (u) != NULL) {
          FORWARD (u);
        }
        NEXT (u) = *s;
        (*s) = t;
      }
    } else if (whether_one_of (p, IDENTIFIER, OPERATOR, NULL_ATTRIBUTE)) {
      if (TAX (p) != NULL && TAG_LEX_LEVEL (TAX (p)) != PRIMAL_SCOPE) {
        scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
      }
    } else if (WHETHER (p, DECLARER)) {
      get_declarer_elements (p, s, A68_TRUE);
    } else {
      gather_scopes_for_youngest (SUB (p), s);
    }
  }
}

/*!
\brief get_youngest_environs
\param p position in tree
**/

static void get_youngest_environs (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, ROUTINE_TEXT, FORMAT_TEXT, NULL_ATTRIBUTE)) {
      SCOPE_T *s = NULL;
      gather_scopes_for_youngest (SUB (p), &s);
      TAX (p)->youngest_environ = scope_find_youngest_outside (s, LEX_LEVEL (p)).level;
    } else {
      get_youngest_environs (SUB (p));
    }
  }
}

/*!
\brief bind_scope_to_tag
\param p position in tree
**/

static void bind_scope_to_tag (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, DEFINING_IDENTIFIER) && MOID (p) == MODE (FORMAT)) {
      if (WHETHER (NEXT_NEXT (p), FORMAT_TEXT)) {
        TAX (p)->scope = TAX (NEXT_NEXT (p))->youngest_environ;
        TAX (p)->scope_assigned = A68_TRUE;
      }
      return;
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      if (WHETHER (NEXT_NEXT (p), ROUTINE_TEXT)) {
        TAX (p)->scope = TAX (NEXT_NEXT (p))->youngest_environ;
        TAX (p)->scope_assigned = A68_TRUE;
      }
      return;
    } else {
      bind_scope_to_tag (SUB (p));
    }
  }
}

/*!
\brief bind_scope_to_tags
\param p position in tree
**/

static void bind_scope_to_tags (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, PROCEDURE_DECLARATION, IDENTITY_DECLARATION, NULL_ATTRIBUTE)) {
      bind_scope_to_tag (SUB (p));
    } else {
      bind_scope_to_tags (SUB (p));
    }
  }
}

/*!
\brief scope_bounds
\param p position in tree
**/

static void scope_bounds (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      scope_statement (p, NULL);
    } else {
      scope_bounds (SUB (p));
    }
  }
}

/*!
\brief scope_declarer
\param p position in tree
**/

static void scope_declarer (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, BOUNDS)) {
      scope_bounds (SUB (p));
    } else if (WHETHER (p, INDICANT)) {
      ;
    } else if (WHETHER (p, REF_SYMBOL)) {
      scope_declarer (NEXT (p));
    } else if (whether_one_of (p, PROC_SYMBOL, UNION_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else {
      scope_declarer (SUB (p));
      scope_declarer (NEXT (p));
    }
  }
}

/*!
\brief scope_identity_declaration
\param p position in tree
**/

static void scope_identity_declaration (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    scope_identity_declaration (SUB (p));
    if (WHETHER (p, DEFINING_IDENTIFIER)) {
      NODE_T *unit = NEXT_NEXT (p);
      SCOPE_T *s = NULL;
      int z = PRIMAL_SCOPE;
      if (ATTRIBUTE (MOID (TAX (p))) != PROC_SYMBOL) {
        check_identifier_usage (TAX (p), unit);
      }
      scope_statement (unit, &s);
      (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
      z = scope_find_youngest (s).level;
      if (z < LEX_LEVEL (p)) {
        TAX (p)->scope = z;
        TAX (p)->scope_assigned = A68_TRUE;
      }
      STATUS_SET (unit, INTERRUPTIBLE_MASK);
      return;
    }
  }
}

/*!
\brief scope_variable_declaration
\param p position in tree
**/

static void scope_variable_declaration (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    scope_variable_declaration (SUB (p));
    if (WHETHER (p, DECLARER)) {
      scope_declarer (SUB (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER)) {
      if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
        NODE_T *unit = NEXT_NEXT (p);
        SCOPE_T *s = NULL;
        check_identifier_usage (TAX (p), unit);
        scope_statement (unit, &s);
        (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
        STATUS_SET (unit, INTERRUPTIBLE_MASK);
        return;
      }
    }
  }
}

/*!
\brief scope_procedure_declaration
\param p position in tree
**/

static void scope_procedure_declaration (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    scope_procedure_declaration (SUB (p));
    if (whether_one_of (p, DEFINING_IDENTIFIER, DEFINING_OPERATOR, NULL_ATTRIBUTE)) {
      NODE_T *unit = NEXT_NEXT (p);
      SCOPE_T *s = NULL;
      scope_statement (unit, &s);
      (void) scope_check (s, NOT_TRANSIENT, LEX_LEVEL (p));
      STATUS_SET (unit, INTERRUPTIBLE_MASK);
      return;
    }
  }
}

/*!
\brief scope_declaration_list
\param p position in tree
**/

static void scope_declaration_list (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      scope_identity_declaration (SUB (p));
    } else if (WHETHER (p, VARIABLE_DECLARATION)) {
      scope_variable_declaration (SUB (p));
    } else if (WHETHER (p, MODE_DECLARATION)) {
      scope_declarer (SUB (p));
    } else if (WHETHER (p, PRIORITY_DECLARATION)) {
      ;
    } else if (WHETHER (p, PROCEDURE_DECLARATION)) {
      scope_procedure_declaration (SUB (p));
    } else if (WHETHER (p, PROCEDURE_VARIABLE_DECLARATION)) {
      scope_procedure_declaration (SUB (p));
    } else if (whether_one_of (p, BRIEF_OPERATOR_DECLARATION, OPERATOR_DECLARATION, NULL_ATTRIBUTE)) {
      scope_procedure_declaration (SUB (p));
    } else {
      scope_declaration_list (SUB (p));
      scope_declaration_list (NEXT (p));
    }
  }
}

/*!
\brief scope_arguments
\param p position in tree
**/

static void scope_arguments (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      SCOPE_T *s = NULL;
      scope_statement (p, &s);
      (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
    } else {
      scope_arguments (SUB (p));
    }
  }
}

/*!
\brief whether_transient_row
\param m mode of row
\return same
**/

static BOOL_T whether_transient_row (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return ((BOOL_T) (WHETHER (SUB (m), FLEX_SYMBOL)));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether_coercion
\param p position in tree
**/

BOOL_T whether_coercion (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DEPROCEDURING:
    case DEREFERENCING:
    case UNITING:
    case ROWING:
    case WIDENING:
    case VOIDING:
    case PROCEDURING:
      {
        return (A68_TRUE);
      }
    default:
      {
        return (A68_FALSE);
      }
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief scope_coercion
\param p position in tree
\param s chain to link into
**/

static void scope_coercion (NODE_T * p, SCOPE_T ** s)
{
  if (whether_coercion (p)) {
    if (WHETHER (p, VOIDING)) {
      scope_coercion (SUB (p), NULL);
    } else if (WHETHER (p, DEREFERENCING)) {
/* Leave this to the dynamic scope checker */
      scope_coercion (SUB (p), NULL);
    } else if (WHETHER (p, DEPROCEDURING)) {
      scope_coercion (SUB (p), NULL);
    } else if (WHETHER (p, ROWING)) {
      scope_coercion (SUB (p), s);
      if (whether_transient_row (MOID (SUB (p)))) {
        scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
      }
    } else if (WHETHER (p, PROCEDURING)) {
/* Can only be a JUMP */
      NODE_T *q = SUB_SUB (p);
      if (WHETHER (q, GOTO_SYMBOL)) {
        FORWARD (q);
      }
      scope_add (s, q, scope_make_tuple (TAG_LEX_LEVEL (TAX (q)), NOT_TRANSIENT));
    } else {
      scope_coercion (SUB (p), s);
    }
  } else {
    scope_statement (p, s);
  }
}

/*!
\brief scope_format_text
\param p position in tree
\param s chain to link into
**/

static void scope_format_text (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, FORMAT_PATTERN)) {
      scope_enclosed_clause (SUB (NEXT_SUB (p)), s);
    } else if (WHETHER (p, FORMAT_ITEM_G) && NEXT (p) != NULL) {
      scope_enclosed_clause (SUB_NEXT (p), s);
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      scope_enclosed_clause (SUB (NEXT_SUB (p)), s);
    } else {
      scope_format_text (SUB (p), s);
    }
  }
}

/*!
\brief whether_transient_selection
\param m mode under test
\return same
**/

static BOOL_T whether_transient_selection (MOID_T * m)
{
  if (WHETHER (m, REF_SYMBOL)) {
    return (whether_transient_selection (SUB (m)));
  } else {
    return ((BOOL_T) (WHETHER (m, FLEX_SYMBOL)));
  }
}

/*!
\brief scope_operand
\param p position in tree
\param s chain to link into
**/

static void scope_operand (NODE_T * p, SCOPE_T ** s)
{
  if (WHETHER (p, MONADIC_FORMULA)) {
    scope_operand (NEXT_SUB (p), s);
  } else if (WHETHER (p, FORMULA)) {
    scope_formula (p, s);
  } else if (WHETHER (p, SECONDARY)) {
    scope_statement (SUB (p), s);
  }
}

/*!
\brief scope_formula
\param p position in tree
\param s chain to link into
**/

static void scope_formula (NODE_T * p, SCOPE_T ** s)
{
  NODE_T *q = SUB (p);
  SCOPE_T *s2 = NULL;
  scope_operand (q, &s2);
  (void) scope_check (s2, TRANSIENT, LEX_LEVEL (p));
  if (NEXT (q) != NULL) {
    SCOPE_T *s3 = NULL;
    scope_operand (NEXT_NEXT (q), &s3);
    (void) scope_check (s3, TRANSIENT, LEX_LEVEL (p));
  }
  (void) s;
}

/*!
\brief scope_routine_text
\param p position in tree
\param s chain to link into
**/

static void scope_routine_text (NODE_T * p, SCOPE_T ** s)
{
  NODE_T *q = SUB (p), *routine = WHETHER (q, PARAMETER_PACK) ? NEXT (q) : q;
  SCOPE_T *x = NULL;
  TUPLE_T routine_tuple;
  scope_statement (NEXT_NEXT (routine), &x);
  (void) scope_check (x, TRANSIENT, LEX_LEVEL (p));
  routine_tuple = scope_make_tuple (TAX (p)->youngest_environ, NOT_TRANSIENT);
  scope_add (s, p, routine_tuple);
}

/*!
\brief scope_statement
\param p position in tree
\param s chain to link into
**/

static void scope_statement (NODE_T * p, SCOPE_T ** s)
{
  if (whether_coercion (p)) {
    scope_coercion (p, s);
  } else if (whether_one_of (p, PRIMARY, SECONDARY, TERTIARY, UNIT, NULL_ATTRIBUTE)) {
    scope_statement (SUB (p), s);
  } else if (whether_one_of (p, DENOTATION, NIHIL, NULL_ATTRIBUTE)) {
    scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
  } else if (WHETHER (p, IDENTIFIER)) {
    if (WHETHER (MOID (p), REF_SYMBOL)) {
      if (PRIO (TAX (p)) == PARAMETER_IDENTIFIER) {
        scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)) - 1, NOT_TRANSIENT));
      } else {
        if (HEAP (TAX (p)) == HEAP_SYMBOL) {
          scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
        } else if (TAX (p)->scope_assigned) {
          scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
        } else {
          scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
        }
      }
    } else if (ATTRIBUTE (MOID (p)) == PROC_SYMBOL && TAX (p)->scope_assigned == A68_TRUE) {
      scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
    } else if (MOID (p) == MODE (FORMAT) && TAX (p)->scope_assigned == A68_TRUE) {
      scope_add (s, p, scope_make_tuple (TAX (p)->scope, NOT_TRANSIENT));
    }
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    scope_enclosed_clause (SUB (p), s);
  } else if (WHETHER (p, CALL)) {
    SCOPE_T *x = NULL;
    scope_statement (SUB (p), &x);
    (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
    scope_arguments (NEXT_SUB (p));
  } else if (WHETHER (p, SLICE)) {
    SCOPE_T *x = NULL;
    MOID_T *m = MOID (SUB (p));
    if (WHETHER (m, REF_SYMBOL)) {
      if (ATTRIBUTE (SUB (p)) == PRIMARY && ATTRIBUTE (SUB_SUB (p)) == SLICE) {
        scope_statement (SUB (p), s);
      } else {
        scope_statement (SUB (p), &x);
        (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
      }
      if (WHETHER (SUB (m), FLEX_SYMBOL)) {
        scope_add (s, SUB (p), scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
      }
      scope_bounds (SUB (NEXT_SUB (p)));
    }
    if (WHETHER (MOID (p), REF_SYMBOL)) {
      scope_add (s, p, scope_find_youngest (x));
    }
  } else if (WHETHER (p, FORMAT_TEXT)) {
    SCOPE_T *x = NULL;
    scope_format_text (SUB (p), &x);
    scope_add (s, p, scope_find_youngest (x));
  } else if (WHETHER (p, CAST)) {
    SCOPE_T *x = NULL;
    scope_enclosed_clause (SUB (NEXT_SUB (p)), &x);
    (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
    scope_add (s, p, scope_find_youngest (x));
  } else if (WHETHER (p, FIELD_SELECTION)) {
    SCOPE_T *ns = NULL;
    scope_statement (SUB (p), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (p));
    if (whether_transient_selection (MOID (SUB (p)))) {
      scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
    }
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, SELECTION)) {
    SCOPE_T *ns = NULL;
    scope_statement (NEXT_SUB (p), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (p));
    if (whether_transient_selection (MOID (NEXT_SUB (p)))) {
      scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
    }
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, GENERATOR)) {
    if (WHETHER (SUB (p), LOC_SYMBOL)) {
      scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), NOT_TRANSIENT));
    } else {
      scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
    }
    scope_declarer (SUB (NEXT_SUB (p)));
  } else if (WHETHER (p, DIAGONAL_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NULL;
    if (WHETHER (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NULL;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, TRANSPOSE_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NULL;
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, ROW_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NULL;
    if (WHETHER (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NULL;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, COLUMN_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NULL;
    if (WHETHER (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NULL;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (WHETHER (p, FORMULA)) {
    scope_formula (p, s);
  } else if (WHETHER (p, ASSIGNATION)) {
    NODE_T *unit = NEXT (NEXT_SUB (p));
    SCOPE_T *ns = NULL, *nd = NULL;
    scope_statement (SUB_SUB (p), &nd);
    scope_statement (unit, &ns);
    (void) scope_check_multiple (ns, TRANSIENT, nd);
    scope_add (s, p, scope_make_tuple (scope_find_youngest (nd).level, NOT_TRANSIENT));
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    scope_routine_text (p, s);
  } else if (whether_one_of (p, IDENTITY_RELATION, AND_FUNCTION, OR_FUNCTION, NULL_ATTRIBUTE)) {
    SCOPE_T *n = NULL;
    scope_statement (SUB (p), &n);
    scope_statement (NEXT (NEXT_SUB (p)), &n);
    (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  } else if (WHETHER (p, ASSERTION)) {
    SCOPE_T *n = NULL;
    scope_enclosed_clause (SUB (NEXT_SUB (p)), &n);
    (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  } else if (whether_one_of (p, JUMP, SKIP, NULL_ATTRIBUTE)) {
    ;
  }
}

/*!
\brief scope_statement_list
\param p position in tree
\param s chain to link into
**/

static void scope_statement_list (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      STATUS_SET (p, INTERRUPTIBLE_MASK);
      scope_statement (p, s);
    } else {
      scope_statement_list (SUB (p), s);
    }
  }
}

/*!
\brief scope_serial_clause
\param p position in tree
\param s chain to link into
\param terminator whether unit terminates clause
**/

static void scope_serial_clause (NODE_T * p, SCOPE_T ** s, BOOL_T terminator)
{
  if (p != NULL) {
    if (WHETHER (p, INITIALISER_SERIES)) {
      scope_serial_clause (SUB (p), s, A68_FALSE);
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (WHETHER (p, DECLARATION_LIST)) {
      scope_declaration_list (SUB (p));
    } else if (whether_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, NULL_ATTRIBUTE)) {
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (whether_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, NULL_ATTRIBUTE)) {
      if (NEXT (p) != NULL) {
        int j = ATTRIBUTE (NEXT (p));
        if (j == EXIT_SYMBOL || j == END_SYMBOL || j == CLOSE_SYMBOL) {
          scope_serial_clause (SUB (p), s, A68_TRUE);
        } else {
          scope_serial_clause (SUB (p), s, A68_FALSE);
        }
      } else {
        scope_serial_clause (SUB (p), s, A68_TRUE);
      }
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (WHETHER (p, LABELED_UNIT)) {
      scope_serial_clause (SUB (p), s, terminator);
    } else if (WHETHER (p, UNIT)) {
      STATUS_SET (p, INTERRUPTIBLE_MASK);
      if (terminator) {
        scope_statement (p, s);
      } else {
        scope_statement (p, NULL);
      }
    }
  }
}

/*!
\brief scope_closed_clause
\param p position in tree
\param s chain to link into
**/

static void scope_closed_clause (NODE_T * p, SCOPE_T ** s)
{
  if (p != NULL) {
    if (WHETHER (p, SERIAL_CLAUSE)) {
      scope_serial_clause (p, s, A68_TRUE);
    } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
      scope_closed_clause (NEXT (p), s);
    }
  }
}

/*!
\brief scope_collateral_clause
\param p position in tree
\param s chain to link into
**/

static void scope_collateral_clause (NODE_T * p, SCOPE_T ** s)
{
  if (p != NULL) {
    if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, 0)
          || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0))) {
      scope_statement_list (p, s);
    }
  }
}

/*!
\brief scope_conditional_clause
\param p position in tree
\param s chain to link into
**/

static void scope_conditional_clause (NODE_T * p, SCOPE_T ** s)
{
  scope_serial_clause (NEXT_SUB (p), NULL, A68_TRUE);
  FORWARD (p);
  scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, ELSE_PART, CHOICE, NULL_ATTRIBUTE)) {
      scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
    } else if (whether_one_of (p, ELIF_PART, BRIEF_ELIF_PART, NULL_ATTRIBUTE)) {
      scope_conditional_clause (SUB (p), s);
    }
  }
}

/*!
\brief scope_case_clause
\param p position in tree
\param s chain to link into
**/

static void scope_case_clause (NODE_T * p, SCOPE_T ** s)
{
  SCOPE_T *n = NULL;
  scope_serial_clause (NEXT_SUB (p), &n, A68_TRUE);
  (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  FORWARD (p);
  scope_statement_list (NEXT_SUB (p), s);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
    } else if (whether_one_of (p, INTEGER_OUT_PART, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE)) {
      scope_case_clause (SUB (p), s);
    } else if (whether_one_of (p, UNITED_OUSE_PART, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE)) {
      scope_case_clause (SUB (p), s);
    }
  }
}

/*!
\brief scope_loop_clause
\param p position in tree
**/

static void scope_loop_clause (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, FOR_PART)) {
      scope_loop_clause (NEXT (p));
    } else if (whether_one_of (p, FROM_PART, BY_PART, TO_PART, NULL_ATTRIBUTE)) {
      scope_statement (NEXT_SUB (p), NULL);
      scope_loop_clause (NEXT (p));
    } else if (WHETHER (p, WHILE_PART)) {
      scope_serial_clause (NEXT_SUB (p), NULL, A68_TRUE);
      scope_loop_clause (NEXT (p));
    } else if (whether_one_of (p, DO_PART, ALT_DO_PART, NULL_ATTRIBUTE)) {
      NODE_T *do_p = NEXT_SUB (p), *un_p;
      if (WHETHER (do_p, SERIAL_CLAUSE)) {
        scope_serial_clause (do_p, NULL, A68_TRUE);
        un_p = NEXT (do_p);
      } else {
        un_p = do_p;
      }
      if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
        scope_serial_clause (NEXT_SUB (un_p), NULL, A68_TRUE);
      }
    }
  }
}

/*!
\brief scope_enclosed_clause
\param p position in tree
\param s chain to link into
**/

static void scope_enclosed_clause (NODE_T * p, SCOPE_T ** s)
{
  if (WHETHER (p, ENCLOSED_CLAUSE)) {
    scope_enclosed_clause (SUB (p), s);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    scope_closed_clause (SUB (p), s);
  } else if (whether_one_of (p, COLLATERAL_CLAUSE, PARALLEL_CLAUSE, NULL_ATTRIBUTE)) {
    scope_collateral_clause (SUB (p), s);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    scope_conditional_clause (SUB (p), s);
  } else if (whether_one_of (p, INTEGER_CASE_CLAUSE, UNITED_CASE_CLAUSE, NULL_ATTRIBUTE)) {
    scope_case_clause (SUB (p), s);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    scope_loop_clause (SUB (p));
  }
}

/*!
\brief scope_checker
\param p position in tree
**/

void scope_checker (NODE_T * p)
{
/* First establish scopes of routine texts and format texts */
  get_youngest_environs (p);
/* PROC and FORMAT identities can now be assigned a scope */
  bind_scope_to_tags (p);
/* Now check evertyhing else */
  scope_enclosed_clause (SUB (p), NULL);
}

/* Mode checker and coercion inserter */

TAG_T *error_tag;

static SOID_LIST_T *top_soid_list = NULL;

static BOOL_T basic_coercions (MOID_T *, MOID_T *, int, int);
static BOOL_T whether_coercible (MOID_T *, MOID_T *, int, int);
static BOOL_T whether_nonproc (MOID_T *);

static void mode_check_enclosed (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_unit (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_formula (NODE_T *, SOID_T *, SOID_T *);

static void coerce_enclosed (NODE_T *, SOID_T *);
static void coerce_operand (NODE_T *, SOID_T *);
static void coerce_formula (NODE_T *, SOID_T *);
static void coerce_unit (NODE_T *, SOID_T *);

#define DEPREF A68_TRUE
#define NO_DEPREF A68_FALSE

#define WHETHER_MODE_IS_WELL(n) (! ((n) == MODE (ERROR) || (n) == MODE (UNDEFINED)))
#define INSERT_COERCIONS(n, p, q) make_strong ((n), (p), MOID (q))

/*!
\brief give accurate error message
\param p mode 1
\param q mode 2
\param context context
\param deflex deflexing regime
\param depth depth of recursion
\return error text
**/

static char *mode_error_text (NODE_T * n, MOID_T * p, MOID_T * q, int context, int deflex, int depth)
{
#define TAIL(z) (&(z)[strlen (z)])
  static char txt[BUFFER_SIZE];
  if (depth == 1) {
    txt[0] = NULL_CHAR;
  }
  if (WHETHER (p, SERIES_MODE)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (MOID (u) != NULL) {
          if (WHETHER (MOID (u), SERIES_MODE)) {
            (void) mode_error_text (n, MOID (u), q, context, deflex, depth + 1);
          } else if (!whether_coercible (MOID (u), q, context, deflex)) {
            int len = (int) strlen (txt);
            if (len > BUFFER_SIZE / 2) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
            } else {
              if (strlen (txt) > 0) {
                ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
              }
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
            }
          }
        }
      }
    }
    if (depth == 1) {
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (q, MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && WHETHER (q, FLEX_SYMBOL)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (!whether_coercible (MOID (u), SLICE (SUB (q)), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (SUB (q)), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && WHETHER (q, ROW_SYMBOL)) {
    PACK_T *u = PACK (p);
    if (u == NULL) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL; FORWARD (u)) {
        if (!whether_coercible (MOID (u), SLICE (q), context, deflex)) {
          int len = (int) strlen (txt);
          if (len > BUFFER_SIZE / 2) {
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " etcetera") >= 0);
          } else {
            if (strlen (txt) > 0) {
              ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " and ") >= 0);
            }
            ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, moid_to_string (MOID (u), MOID_ERROR_WIDTH, n)) >= 0);
          }
        }
      }
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (SLICE (q), MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (WHETHER (p, STOWED_MODE) && (WHETHER (q, PROC_SYMBOL) || WHETHER (q, STRUCT_SYMBOL))) {
    PACK_T *u = PACK (p), *v = PACK (q);
    if (u == NULL) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NULL && v != NULL; FORWARD (u), FORWARD (v)) {
        if (!whether_coercible (MOID (u), MOID (v), context, deflex)) {
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
  return (txt);
#undef TAIL
}

/*!
\brief cannot coerce error
\param p position in tree
\param from mode 1
\param to mode 2
\param context context
\param deflex deflexing regime
\param att attribute of context
**/

static void cannot_coerce (NODE_T * p, MOID_T * from, MOID_T * to, int context, int deflex, int att)
{
  char *txt = mode_error_text (p, from, to, context, deflex, 1);
  if (att == NULL_ATTRIBUTE) {
    if (strlen (txt) == 0) {
      diagnostic_node (A68_ERROR, p, "M cannot be coerced to M in C context", from, to, context);
    } else {
      diagnostic_node (A68_ERROR, p, "Y in C context", txt, context);
    }
  } else {
    if (strlen (txt) == 0) {
      diagnostic_node (A68_ERROR, p, "M cannot be coerced to M in C-A", from, to, context, att);
    } else {
      diagnostic_node (A68_ERROR, p, "Y in C-A", txt, context, att);
    }
  }
}

/*!
\brief driver for mode checker
\param p position in tree
**/

void mode_checker (NODE_T * p)
{
  if (WHETHER (p, PARTICULAR_PROGRAM)) {
    SOID_T x, y;
    top_soid_list = NULL;
    make_soid (&x, STRONG, MODE (VOID), 0);
    mode_check_enclosed (SUB (p), &x, &y);
    MOID (p) = MOID (&y);
  }
}

/*!
\brief driver for coercion inserions
\param p position in tree
**/

void coercion_inserter (NODE_T * p)
{
  if (WHETHER (p, PARTICULAR_PROGRAM)) {
    SOID_T q;
    make_soid (&q, STRONG, MODE (VOID), 0);
    coerce_enclosed (SUB (p), &q);
  }
}

/*!
\brief whether mode is not well defined
\param p mode, may be NULL
\return same
**/

static BOOL_T whether_mode_isnt_well (MOID_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else if (!WHETHER_MODE_IS_WELL (p)) {
    return (A68_TRUE);
  } else if (PACK (p) != NULL) {
    PACK_T *q = PACK (p);
    for (; q != NULL; FORWARD (q)) {
      if (!WHETHER_MODE_IS_WELL (MOID (q))) {
        return (A68_TRUE);
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief make SOID data structure
\param s soid buffer
\param sort sort
\param type mode
\param attribute attribute
**/

void make_soid (SOID_T * s, int sort, MOID_T * type, int attribute)
{
  ATTRIBUTE (s) = attribute;
  SORT (s) = sort;
  MOID (s) = type;
  CAST (s) = A68_FALSE;
}

/*!
\brief add SOID data to free chain
\param root top soid list
**/

void free_soid_list (SOID_LIST_T *root) {
  if (root != NULL) {
    SOID_LIST_T *q;
    for (q = root; NEXT (q) != NULL; FORWARD (q)) {
      /* skip */;
    }
    NEXT (q) = top_soid_list;
    top_soid_list = root;
  }
}

/*!
\brief add SOID data structure to soid list
\param root top soid list
\param nwhere position in tree
\param soid entry to add
**/

static void add_to_soid_list (SOID_LIST_T ** root, NODE_T * nwhere, SOID_T * soid)
{
  if (*root != NULL) {
    add_to_soid_list (&(NEXT (*root)), nwhere, soid);
  } else {
    SOID_LIST_T *new_one;
    if (top_soid_list == NULL) {
      new_one = (SOID_LIST_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (SOID_LIST_T));
      new_one->yield = (SOID_T *) get_temp_heap_space ((size_t) ALIGNED_SIZE_OF (SOID_T));
    } else {
      new_one = top_soid_list;
      FORWARD (top_soid_list);
    }
    new_one->where = nwhere;
    make_soid (new_one->yield, SORT (soid), MOID (soid), 0);
    NEXT (new_one) = NULL;
    *root = new_one;
  }
}

/*!
\brief absorb nested series modes recursively
\param p mode
**/

static void absorb_series_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NULL, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NULL; FORWARD (t)) {
      if (MOID (t) != NULL && WHETHER (MOID (t), SERIES_MODE)) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NULL; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NULL, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NULL, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

/*!
\brief absorb nested series and united modes recursively
\param p mode
**/

static void absorb_series_union_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NULL, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NULL; FORWARD (t)) {
      if (MOID (t) != NULL && (WHETHER (MOID (t), SERIES_MODE) || WHETHER (MOID (t), UNION_SYMBOL))) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NULL; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NULL, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NULL, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

/*!
\brief make SERIES (u, v)
\param u mode 1
\param v mode 2
\return SERIES (u, v)
**/

static MOID_T *make_series_from_moids (MOID_T * u, MOID_T * v)
{
  MOID_T *x = new_moid ();
  ATTRIBUTE (x) = SERIES_MODE;
  add_mode_to_pack (&(PACK (x)), u, NULL, NODE (u));
  add_mode_to_pack (&(PACK (x)), v, NULL, NODE (v));
  absorb_series_pack (&x);
  DIM (x) = count_pack_members (PACK (x));
  add_single_moid_to_list (&top_moid_list, x, NULL);
  if (DIM (x) == 1) {
    return (MOID (PACK (x)));
  } else {
    return (x);
  }
}

/*!
\brief absorb firmly related unions in mode
\param m united mode
\return absorbed "m"
**/

static MOID_T *absorb_related_subsets (MOID_T * m)
{
/*
For instance invalid UNION (PROC REF UNION (A, B), A, B) -> valid UNION (A, B),
which is used in balancing conformity clauses.
*/
  int mods;
  do {
    PACK_T *u = NULL, *v;
    mods = 0;
    for (v = PACK (m); v != NULL; FORWARD (v)) {
      MOID_T *n = depref_completely (MOID (v));
      if (WHETHER (n, UNION_SYMBOL) && whether_subset (n, m, SAFE_DEFLEXING)) {
/* Unpack it */
        PACK_T *w;
        for (w = PACK (n); w != NULL; FORWARD (w)) {
          add_mode_to_pack (&u, MOID (w), NULL, NODE (w));
        }
        mods++;
      } else {
        add_mode_to_pack (&u, MOID (v), NULL, NODE (v));
      }
    }
    PACK (m) = absorb_union_pack (u, &mods);
  } while (mods != 0);
  return (m);
}

/*!
\brief register mode in the global mode table, if mode is unique
\param u mode to enter
\return mode table entry
**/

static MOID_T *register_extra_mode (MOID_T * u)
{
  MOID_LIST_T *z;
/* Check for equivalency */
  for (z = top_moid_list; z != NULL; FORWARD (z)) {
    MOID_T *v = MOID (z);
    BOOL_T w;
    free_postulate_list (top_postulate, NULL);
    top_postulate = NULL;
    w = (BOOL_T) (EQUIVALENT (v) == NULL && whether_modes_equivalent (v, u));
    if (w) {
      return (v);
    }
  }
/* Mode u is unique - include in the global moid list */
  z = (MOID_LIST_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (MOID_LIST_T));
  z->coming_from_level = NULL;
  MOID (z) = u;
  NEXT (z) = top_moid_list;
  ABEND (z == NULL, "NULL pointer", "register_extra_mode");
  top_moid_list = z;
  add_single_moid_to_list (&top_moid_list, u, NULL);
  return (u);
}

/*!
\brief make united mode, from mode that is a SERIES (..)
\param m series mode
\return mode table entry
**/

static MOID_T *make_united_mode (MOID_T * m)
{
  MOID_T *u;
  PACK_T *v, *w;
  int mods;
  if (m == NULL) {
    return (MODE (ERROR));
  } else if (ATTRIBUTE (m) != SERIES_MODE) {
    return (m);
  }
/* Do not unite a single UNION */
  if (DIM (m) == 1 && WHETHER (MOID (PACK (m)), UNION_SYMBOL)) {
    return (MOID (PACK (m)));
  }
/* Straighten the series */
  absorb_series_union_pack (&m);
/* Copy the series into a UNION */
  u = new_moid ();
  ATTRIBUTE (u) = UNION_SYMBOL;
  PACK (u) = NULL;
  v = PACK (u);
  w = PACK (m);
  for (w = PACK (m); w != NULL; FORWARD (w)) {
    add_mode_to_pack (&(PACK (u)), MOID (w), NULL, NODE (m));
  }
/* Absorb and contract the new UNION */
  do {
    mods = 0;
    absorb_series_union_pack (&u);
    DIM (u) = count_pack_members (PACK (u));
    PACK (u) = absorb_union_pack (PACK (u), &mods);
    contract_union (u, &mods);
  } while (mods != 0);
/* A UNION of one mode is that mode itself */
  if (DIM (u) == 1) {
    return (MOID (PACK (u)));
  } else {
    return (register_extra_mode (u));
  }
}

/*!
\brief pack soids in moid, gather resulting moids from terminators in a clause
\param top_sl top soid list
\param attribute mode attribute
\return mode table entry
**/

static MOID_T *pack_soids_in_moid (SOID_LIST_T * top_sl, int attribute)
{
  MOID_T *x = new_moid ();
  PACK_T *t, **p;
  NUMBER (x) = mode_count++;
  ATTRIBUTE (x) = attribute;
  DIM (x) = 0;
  SUB (x) = NULL;
  EQUIVALENT (x) = NULL;
  SLICE (x) = NULL;
  DEFLEXED (x) = NULL;
  NAME (x) = NULL;
  NEXT (x) = NULL;
  PACK (x) = NULL;
  p = &(PACK (x));
  for (; top_sl != NULL; FORWARD (top_sl)) {
    t = new_pack ();
    MOID (t) = MOID (top_sl->yield);
    TEXT (t) = NULL;
    NODE (t) = top_sl->where;
    NEXT (t) = NULL;
    DIM (x)++;
    *p = t;
    p = &NEXT (t);
  }
  add_single_moid_to_list (&top_moid_list, x, NULL);
  return (x);
}

/*!
\brief whether mode is deprefable
\param p mode
\return same
**/

BOOL_T whether_deprefable (MOID_T * p)
{
  if (WHETHER (p, REF_SYMBOL)) {
    return (A68_TRUE);
  } else {
    return ((BOOL_T) (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL));
  }
}

/*!
\brief depref mode once
\param p mode
\return single-depreffed mode
**/

static MOID_T *depref_once (MOID_T * p)
{
  if (WHETHER (p, REF_SYMBOL)) {
    return (SUB (p));
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (SUB (p));
  } else {
    return (NULL);
  }
}

/*!
\brief depref mode completely
\param p mode
\return completely depreffed mode
**/

MOID_T *depref_completely (MOID_T * p)
{
  while (whether_deprefable (p)) {
    p = depref_once (p);
  }
  return (p);
}

/*!
\brief deproc_completely
\param p mode
\return completely deprocedured mode
**/

static MOID_T *deproc_completely (MOID_T * p)
{
  while (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    p = depref_once (p);
  }
  return (p);
}

/*!
\brief depref rows
\param p mode
\param q mode
\return possibly depreffed mode
**/

static MOID_T *depref_rows (MOID_T * p, MOID_T * q)
{
  if (q == MODE (ROWS)) {
    while (whether_deprefable (p)) {
      p = depref_once (p);
    }
    return (p);
  } else {
    return (q);
  }
}

/*!
\brief derow mode, strip FLEX and BOUNDS
\param p mode
\return same
**/

static MOID_T *derow (MOID_T * p)
{
  if (WHETHER (p, ROW_SYMBOL) || WHETHER (p, FLEX_SYMBOL)) {
    return (derow (SUB (p)));
  } else {
    return (p);
  }
}

/*!
\brief whether rows type
\param p mode
\return same
**/

static BOOL_T whether_rows_type (MOID_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ROW_SYMBOL:
  case FLEX_SYMBOL:
    {
      return (A68_TRUE);
    }
  case UNION_SYMBOL:
    {
      PACK_T *t = PACK (p);
      BOOL_T go_on = A68_TRUE;
      while (t != NULL && go_on) {
        go_on &= whether_rows_type (MOID (t));
        FORWARD (t);
      }
      return (go_on);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether mode is PROC (REF FILE) VOID or FORMAT
\param p mode
\return same
**/

static BOOL_T whether_proc_ref_file_void_or_format (MOID_T * p)
{
  if (p == MODE (PROC_REF_FILE_VOID)) {
    return (A68_TRUE);
  } else if (p == MODE (FORMAT)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether mode can be transput
\param p mode
\return same
**/

static BOOL_T whether_transput_mode (MOID_T * p, char rw)
{
  if (p == MODE (INT)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_INT)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_INT)) {
    return (A68_TRUE);
  } else if (p == MODE (REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_REAL)) {
    return (A68_TRUE);
  } else if (p == MODE (BOOL)) {
    return (A68_TRUE);
  } else if (p == MODE (CHAR)) {
    return (A68_TRUE);
  } else if (p == MODE (BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_BITS)) {
    return (A68_TRUE);
  } else if (p == MODE (COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (LONG_COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (LONGLONG_COMPLEX)) {
    return (A68_TRUE);
  } else if (p == MODE (ROW_CHAR)) {
    return (A68_TRUE);
  } else if (p == MODE (STRING)) {
    return (A68_TRUE);
  } else if (p == MODE (SOUND)) {
    return (A68_TRUE);
  } else if (WHETHER (p, UNION_SYMBOL) || WHETHER (p, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (p);
    BOOL_T k = A68_TRUE;
    for (; q != NULL && k; FORWARD (q)) {
      k = (BOOL_T) (k & (whether_transput_mode (MOID (q), rw) || whether_proc_ref_file_void_or_format (MOID (q))));
    }
    return (k);
  } else if (WHETHER (p, FLEX_SYMBOL)) {
    return ((BOOL_T) (rw == 'w' ? whether_transput_mode (SUB (p), rw) : A68_FALSE));
  } else if (WHETHER (p, ROW_SYMBOL)) {
    return ((BOOL_T) (whether_transput_mode (SUB (p), rw) || whether_proc_ref_file_void_or_format (SUB (p))));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether mode is printable
\param p mode
\return same
**/

static BOOL_T whether_printable_mode (MOID_T * p)
{
  if (whether_proc_ref_file_void_or_format (p)) {
    return (A68_TRUE);
  } else {
    return (whether_transput_mode (p, 'w'));
  }
}

/*!
\brief whether mode is readable
\param p mode
\return same
**/

static BOOL_T whether_readable_mode (MOID_T * p)
{
  if (whether_proc_ref_file_void_or_format (p)) {
    return (A68_TRUE);
  } else {
    return ((BOOL_T) (WHETHER (p, REF_SYMBOL) ? whether_transput_mode (SUB (p), 'r') : A68_FALSE));
  }
}

/*!
\brief whether name struct
\param p mode
\return same
**/

static BOOL_T whether_name_struct (MOID_T * p)
{
  return ((BOOL_T) (p->name != NULL ? WHETHER (DEFLEX (SUB (p)), STRUCT_SYMBOL) : A68_FALSE));
}

/*!
\brief whether mode can be coerced to another in a certain context
\param u mode 1
\param v mode 2
\param context
\return same
**/

BOOL_T whether_modes_equal (MOID_T * u, MOID_T * v, int deflex)
{
  if (u == v) {
    return (A68_TRUE);
  } else {
    switch (deflex) {
    case SKIP_DEFLEXING:
    case FORCE_DEFLEXING:
      {
/* Allow any interchange between FLEX [] A and [] A */
        return ((BOOL_T) (DEFLEX (u) == DEFLEX (v)));
      }
    case ALIAS_DEFLEXING:
/* Cannot alias [] A to FLEX [] A, but vice versa is ok */
      {
        if (u->has_ref) {
          return ((BOOL_T) (DEFLEX (u) == v));
        } else {
          return (whether_modes_equal (u, v, SAFE_DEFLEXING));
        }
      }
    case SAFE_DEFLEXING:
      {
/* Cannot alias [] A to FLEX [] A but values are ok */
        if (!u->has_ref && !v->has_ref) {
          return (whether_modes_equal (u, v, FORCE_DEFLEXING));
        } else {
          return (A68_FALSE);
        }
    case NO_DEFLEXING:
        return (A68_FALSE);
      }
    }
  }
  return (A68_FALSE);
}

/*!
\brief yield mode to unite to
\param m mode
\param u united mode
\return same
**/

MOID_T *unites_to (MOID_T * m, MOID_T * u)
{
/* Uniting m->u */
  MOID_T *v = NULL;
  PACK_T *p;
  if (u == MODE (SIMPLIN) || u == MODE (SIMPLOUT)) {
    return (m);
  }
  for (p = PACK (u); p != NULL; FORWARD (p)) {
/* Prefer []->[] over []->FLEX [] */
    if (m == MOID (p)) {
      v = MOID (p);
    } else if (v == NULL && DEFLEX (m) == DEFLEX (MOID (p))) {
      v = MOID (p);
    }
  }
  return (v);
}

/*!
\brief whether moid in pack
\param u mode
\param v pack
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_moid_in_pack (MOID_T * u, PACK_T * v, int deflex)
{
  for (; v != NULL; FORWARD (v)) {
    if (whether_modes_equal (u, MOID (v), deflex)) {
      return (A68_TRUE);
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether "p" is a subset of "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

BOOL_T whether_subset (MOID_T * p, MOID_T * q, int deflex)
{
  PACK_T *u = PACK (p);
  BOOL_T j = A68_TRUE;
  for (; u != NULL && j; FORWARD (u)) {
    j = (BOOL_T) (j && whether_moid_in_pack (MOID (u), PACK (q), deflex));
  }
  return (j);
}

/*!
\brief whether "p" can be united to UNION "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

BOOL_T whether_unitable (MOID_T * p, MOID_T * q, int deflex)
{
  if (WHETHER (q, UNION_SYMBOL)) {
    if (WHETHER (p, UNION_SYMBOL)) {
      return (whether_subset (p, q, deflex));
    } else {
      return (whether_moid_in_pack (p, PACK (q), deflex));
    }
  }
  return (A68_FALSE);
}

/*!
\brief whether all or some components of "u" can be firmly coerced to a component mode of "v".
\param u mode
\param v mode 
\param all all coercible
\param some some coercible
**/

static void investigate_firm_relations (PACK_T * u, PACK_T * v, BOOL_T * all, BOOL_T * some)
{
  *all = A68_TRUE;
  *some = A68_FALSE;
  for (; v != NULL; FORWARD (v)) {
    PACK_T *w;
    BOOL_T k = A68_FALSE;
    for (w = u; w != NULL; FORWARD (w)) {
      k |= whether_coercible (MOID (w), MOID (v), FIRM, FORCE_DEFLEXING);
    }
    *some |= k;
    *all &= k;
  }
}

/*!
\brief whether there is a soft path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_softly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (whether_softly_coercible (SUB (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a weak path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_weakly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_weakly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a meek path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_meekly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_meekly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether there is a firm path from "p" to "q"
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_firmly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    return (A68_TRUE);
  } else if (whether_unitable (p, q, deflex)) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_firmly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" widens to "q"
\param p mode
\param q mode
\return same
**/

static MOID_T *widens_to (MOID_T * p, MOID_T * q)
{
  if (p == MODE (INT)) {
    if (q == MODE (LONG_INT) || q == MODE (LONGLONG_INT) || q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_INT));
    } else if (q == MODE (REAL) || q == MODE (COMPLEX)) {
      return (MODE (REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_INT)) {
    if (q == MODE (LONGLONG_INT)) {
      return (MODE (LONGLONG_INT));
    } else if (q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_INT)) {
    if (q == MODE (LONGLONG_REAL) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_REAL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (REAL)) {
    if (q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_REAL));
    } else if (q == MODE (COMPLEX)) {
      return (MODE (COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (COMPLEX)) {
    if (q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_REAL)) {
    if (q == MODE (LONGLONG_REAL) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_REAL));
    } else if (q == MODE (LONG_COMPLEX)) {
      return (MODE (LONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_COMPLEX)) {
    if (q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_REAL)) {
    if (q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_COMPLEX));
    } else {
      return (NULL);
    }
  } else if (p == MODE (BITS)) {
    if (q == MODE (LONG_BITS) || q == MODE (LONGLONG_BITS)) {
      return (MODE (LONG_BITS));
    } else if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONG_BITS)) {
    if (q == MODE (LONGLONG_BITS)) {
      return (MODE (LONGLONG_BITS));
    } else if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (LONGLONG_BITS)) {
    if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NULL);
    }
  } else if (p == MODE (BYTES) && q == MODE (ROW_CHAR)) {
    return (MODE (ROW_CHAR));
  } else if (p == MODE (LONG_BYTES) && q == MODE (ROW_CHAR)) {
    return (MODE (ROW_CHAR));
  } else {
    return (NULL);
  }
}

/*!
\brief whether "p" widens to "q"
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_widenable (MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  if (z != NULL) {
    return ((BOOL_T) (z == q ? A68_TRUE : whether_widenable (z, q)));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" is a REF ROW
\param p mode
\return same
**/

static BOOL_T whether_ref_row (MOID_T * p)
{
  return ((BOOL_T) (p->name != NULL ? WHETHER (DEFLEX (SUB (p)), ROW_SYMBOL) : A68_FALSE));
}

/*!
\brief whether strong name
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_strong_name (MOID_T * p, MOID_T * q)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (whether_ref_row (q)) {
    return (whether_strong_name (p, q->name));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether strong slice
\param p mode
\param q mode
\return same
**/

static BOOL_T whether_strong_slice (MOID_T * p, MOID_T * q)
{
  if (p == q || whether_widenable (p, q)) {
    return (A68_TRUE);
  } else if (SLICE (q) != NULL) {
    return (whether_strong_slice (p, SLICE (q)));
  } else if (WHETHER (q, FLEX_SYMBOL)) {
    return (whether_strong_slice (p, SUB (q)));
  } else if (whether_ref_row (q)) {
    return (whether_strong_name (p, q));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether strongly coercible
\param p mode
\param q mode
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_strongly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
/* Keep this sequence of statements */
  if (p == q) {
    return (A68_TRUE);
  } else if (q == MODE (VOID)) {
    return (A68_TRUE);
  } else if ((q == MODE (SIMPLIN) || q == MODE (ROW_SIMPLIN)) && whether_readable_mode (p)) {
    return (A68_TRUE);
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    return (A68_TRUE);
  } else if (whether_unitable (p, derow (q), deflex)) {
    return (A68_TRUE);
  }
  if (whether_ref_row (q) && whether_strong_name (p, q)) {
    return (A68_TRUE);
  } else if (SLICE (q) != NULL && whether_strong_slice (p, q)) {
    return (A68_TRUE);
  } else if (WHETHER (q, FLEX_SYMBOL) && whether_strong_slice (p, q)) {
    return (A68_TRUE);
  } else if (whether_widenable (p, q)) {
    return (A68_TRUE);
  } else if (whether_deprefable (p)) {
    return (whether_strongly_coercible (depref_once (p), q, deflex));
  } else if (q == MODE (SIMPLOUT) || q == MODE (ROW_SIMPLOUT)) {
    return (whether_printable_mode (p));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether firm
\param p mode
\param q mode
\return same
**/

BOOL_T whether_firm (MOID_T * p, MOID_T * q)
{
  return ((BOOL_T) (whether_firmly_coercible (p, q, SAFE_DEFLEXING) || whether_firmly_coercible (q, p, SAFE_DEFLEXING)));
}

/*!
\brief whether coercible stowed
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_stowed (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c == STRONG) {
    if (q == MODE (VOID)) {
      return (A68_TRUE);
    } else if (WHETHER (q, FLEX_SYMBOL)) {
      PACK_T *u = PACK (p);
      BOOL_T j = A68_TRUE;
      for (; u != NULL && j; FORWARD (u)) {
        j &= whether_coercible (MOID (u), SLICE (SUB (q)), c, deflex);
      }
      return (j);
    } else if (WHETHER (q, ROW_SYMBOL)) {
      PACK_T *u = PACK (p);
      BOOL_T j = A68_TRUE;
      for (; u != NULL && j; FORWARD (u)) {
        j &= whether_coercible (MOID (u), SLICE (q), c, deflex);
      }
      return (j);
    } else if (WHETHER (q, PROC_SYMBOL) || WHETHER (q, STRUCT_SYMBOL)) {
      PACK_T *u = PACK (p), *v = PACK (q);
      if (DIM (p) != DIM (q)) {
        return (A68_FALSE);
      } else {
        BOOL_T j = A68_TRUE;
        while (u != NULL && v != NULL && j) {
          j &= whether_coercible (MOID (u), MOID (v), c, deflex);
          FORWARD (u);
          FORWARD (v);
        }
        return (j);
      }
    } else {
      return (A68_FALSE);
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether coercible series
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_series (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c != STRONG) {
    return (A68_FALSE);
  } else if (p == NULL || q == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, SERIES_MODE) && PACK (p) == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (q, SERIES_MODE) && PACK (q) == NULL) {
    return (A68_FALSE);
  } else if (PACK (p) == NULL) {
    return (whether_coercible (p, q, c, deflex));
  } else {
    PACK_T *u = PACK (p);
    BOOL_T j = A68_TRUE;
    for (; u != NULL && j; FORWARD (u)) {
      if (MOID (u) != NULL) {
        j &= whether_coercible (MOID (u), q, c, deflex);
      }
    }
    return (j);
  }
}

/*!
\brief basic coercions
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

static BOOL_T basic_coercions (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (c == NO_SORT) {
    return ((BOOL_T) (p == q));
  } else if (c == SOFT) {
    return (whether_softly_coercible (p, q, deflex));
  } else if (c == WEAK) {
    return (whether_weakly_coercible (p, q, deflex));
  } else if (c == MEEK) {
    return (whether_meekly_coercible (p, q, deflex));
  } else if (c == FIRM) {
    return (whether_firmly_coercible (p, q, deflex));
  } else if (c == STRONG) {
    return (whether_strongly_coercible (p, q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether "p" can be coerced to "q" in a "c" context
\param p mode
\param q mode
\param c context
\param deflex deflexing regime
\return same
**/

BOOL_T whether_coercible (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (whether_mode_isnt_well (p) || whether_mode_isnt_well (q)) {
    return (A68_TRUE);
  } else if (p == q) {
    return (A68_TRUE);
  } else if (p == MODE (HIP)) {
    return (A68_TRUE);
  } else if (WHETHER (p, STOWED_MODE)) {
    return (whether_coercible_stowed (p, q, c, deflex));
  } else if (WHETHER (p, SERIES_MODE)) {
    return (whether_coercible_series (p, q, c, deflex));
  } else if (p == MODE (VACUUM) && WHETHER (DEFLEX (q), ROW_SYMBOL)) {
    return (A68_TRUE);
  } else if (basic_coercions (p, q, c, deflex)) {
    return (A68_TRUE);
  } else if (deflex == FORCE_DEFLEXING) {
/* Allow for any interchange between FLEX [] A and [] A */
    return (basic_coercions (DEFLEX (p), DEFLEX (q), c, FORCE_DEFLEXING));
  } else if (deflex == ALIAS_DEFLEXING) {
/* No aliasing of REF [] and REF FLEX [], but vv is ok and values too */
    if (p->has_ref) {
      return (basic_coercions (DEFLEX (p), q, c, ALIAS_DEFLEXING));
    } else {
      return (whether_coercible (p, q, c, SAFE_DEFLEXING));
    }
  } else if (deflex == SAFE_DEFLEXING) {
/* No aliasing of REF [] and REF FLEX [], but ok and values too */
    if (!p->has_ref && !q->has_ref) {
      return (whether_coercible (p, q, c, FORCE_DEFLEXING));
    } else {
      return (basic_coercions (p, q, c, SAFE_DEFLEXING));
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief whether coercible in context
\param p soid
\param q soid
\param deflex deflexing regime
\return same
**/

static BOOL_T whether_coercible_in_context (SOID_T * p, SOID_T * q, int deflex)
{
  if (SORT (p) != SORT (q)) {
    return (A68_FALSE);
  } else if (MOID (p) == MOID (q)) {
    return (A68_TRUE);
  } else {
    return (whether_coercible (MOID (p), MOID (q), SORT (q), deflex));
  }
}

/*!
\brief whether list "y" is balanced
\param n position in tree
\param y soid list
\param sort sort
\return same
**/

static BOOL_T whether_balanced (NODE_T * n, SOID_LIST_T * y, int sort)
{
  if (sort == STRONG) {
    return (A68_TRUE);
  } else {
    BOOL_T k = A68_FALSE;
    for (; y != NULL && !k; FORWARD (y)) {
      SOID_T *z = y->yield;
      k = (BOOL_T) (WHETHER_NOT (MOID (z), STOWED_MODE));
    }
    if (k == A68_FALSE) {
      diagnostic_node (A68_ERROR, n, ERROR_NO_UNIQUE_MODE);
    }
    return (k);
  }
}

/*!
\brief a moid from "m" to which all other members can be coerced
\param m mode
\param sort sort
\param return_depreffed whether to depref
\param deflex deflexing regime
\return same
**/

MOID_T *get_balanced_mode (MOID_T * m, int sort, BOOL_T return_depreffed, int deflex)
{
  MOID_T *common = NULL;
  if (m != NULL && !whether_mode_isnt_well (m) && WHETHER (m, UNION_SYMBOL)) {
    int depref_level;
    BOOL_T go_on = A68_TRUE;
/* Test for increasing depreffing */
    for (depref_level = 0; go_on; depref_level++) {
      PACK_T *p;
      go_on = A68_FALSE;
/* Test the whole pack */
      for (p = PACK (m); p != NULL; FORWARD (p)) {
/* HIPs are not eligible of course */
        if (MOID (p) != MODE (HIP)) {
          MOID_T *candidate = MOID (p);
          int k;
/* Depref as far as allowed */
          for (k = depref_level; k > 0 && whether_deprefable (candidate); k--) {
            candidate = depref_once (candidate);
          }
/* Only need testing if all allowed deprefs succeeded */
          if (k == 0) {
            PACK_T *q;
            MOID_T *to = return_depreffed ? depref_completely (candidate) : candidate;
            BOOL_T all_coercible = A68_TRUE;
            go_on = A68_TRUE;
            for (q = PACK (m); q != NULL && all_coercible; FORWARD (q)) {
              MOID_T *from = MOID (q);
              if (p != q && from != to) {
                all_coercible &= whether_coercible (from, to, sort, deflex);
              }
            }
/* If the pack is coercible to the candidate, we mark the candidate.
   We continue searching for longest series of REF REF PROC REF . */
            if (all_coercible) {
              MOID_T *mark = (return_depreffed ? MOID (p) : candidate);
              if (common == NULL) {
                common = mark;
              } else if (WHETHER (candidate, FLEX_SYMBOL) && DEFLEX (candidate) == common) {
/* We prefer FLEX */
                common = mark;
              }
            }
          }
        }
      }                         /* for */
    }                           /* for */
  }
  return (common == NULL ? m : common);
}

/*!
\brief whether we can search a common mode from a clause or not
\param att attribute
\return same
**/

static BOOL_T clause_allows_balancing (int att)
{
  switch (att) {
  case CLOSED_CLAUSE:
  case CONDITIONAL_CLAUSE:
  case INTEGER_CASE_CLAUSE:
  case SERIAL_CLAUSE:
  case UNITED_CASE_CLAUSE:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief a unique mode from "z"
\param z soid
\param deflex deflexing regime
\return same
**/

static MOID_T *determine_unique_mode (SOID_T * z, int deflex)
{
  if (z == NULL) {
    return (NULL);
  } else {
    MOID_T *x = MOID (z);
    if (whether_mode_isnt_well (x)) {
      return (MODE (ERROR));
    }
    x = make_united_mode (x);
    if (clause_allows_balancing (ATTRIBUTE (z))) {
      return (get_balanced_mode (x, STRONG, NO_DEPREF, deflex));
    } else {
      return (x);
    }
  }
}

/*!
\brief give a warning when a value is silently discarded
\param p position in tree
\param x soid
\param y soid
\param c context
**/

static void warn_for_voiding (NODE_T * p, SOID_T * x, SOID_T * y, int c)
{
  (void) c;
  if (CAST (x) == A68_FALSE) {
    if (MOID (x) == MODE (VOID) && MOID (y) != MODE (ERROR) && !(MOID (y) == MODE (VOID) || !whether_nonproc (MOID (y)))) {
      if (WHETHER (p, FORMULA)) {
        diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_VOIDED, MOID (y));
      } else {
        diagnostic_node (A68_WARNING, p, WARNING_VOIDED, MOID (y));
      }
    }
  }
}

/*!
\brief warn for things that are likely unintended
\param p position in tree
\param m moid
\param c context
\param u attribute
**/

static void semantic_pitfall (NODE_T * p, MOID_T * m, int c, int u)
{
/*
semantic_pitfall: warn for things that are likely unintended, for instance
                  REF INT i := LOC INT := 0, which should probably be
                  REF INT i = LOC INT := 0.
*/
  if (WHETHER (p, u)) {
    diagnostic_node (A68_WARNING, p, WARNING_UNINTENDED, MOID (p), u, m, c);
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    semantic_pitfall (SUB (p), m, c, u);
  }
}

/*!
\brief insert coercion "a" in the tree
\param l position in tree
\param a attribute
\param m (coerced) moid
**/

static void make_coercion (NODE_T * l, int a, MOID_T * m)
{
  make_sub (l, l, a);
  MOID (l) = depref_rows (MOID (l), m);
}

/*!
\brief make widening coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_widening_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  make_coercion (n, WIDENING, z);
  if (z != q) {
    make_widening_coercion (n, z, q);
  }
}

/*!
\brief make ref rowing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_ref_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (whether_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (whether_ref_row (q)) {
      make_ref_rowing_coercion (n, p, q->name);
      make_coercion (n, ROWING, q);
    }
  }
}

/*!
\brief make rowing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (whether_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (SLICE (q) != NULL) {
      make_rowing_coercion (n, p, SLICE (q));
      make_coercion (n, ROWING, q);
    } else if (WHETHER (q, FLEX_SYMBOL)) {
      make_rowing_coercion (n, p, SUB (q));
    } else if (whether_ref_row (q)) {
      make_ref_rowing_coercion (n, p, q);
    }
  }
}

/*!
\brief make uniting coercion
\param n position in tree
\param q mode
**/

static void make_uniting_coercion (NODE_T * n, MOID_T * q)
{
  make_coercion (n, UNITING, derow (q));
  if (WHETHER (q, ROW_SYMBOL) || WHETHER (q, FLEX_SYMBOL)) {
    make_rowing_coercion (n, derow (q), q);
  }
}

/*!
\brief make depreffing coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_depreffing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) == DEFLEX (q)) {
    return;
  } else if (q == MODE (SIMPLOUT) && whether_printable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == MODE (ROW_SIMPLOUT) && whether_printable_mode (p)) {
    make_coercion (n, UNITING, MODE (SIMPLOUT));
    make_coercion (n, ROWING, MODE (ROW_SIMPLOUT));
  } else if (q == MODE (SIMPLIN) && whether_readable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == MODE (ROW_SIMPLIN) && whether_readable_mode (p)) {
    make_coercion (n, UNITING, MODE (SIMPLIN));
    make_coercion (n, ROWING, MODE (ROW_SIMPLIN));
  } else if (q == MODE (ROWS) && whether_rows_type (p)) {
    make_coercion (n, UNITING, MODE (ROWS));
    MOID (n) = MODE (ROWS);
  } else if (whether_widenable (p, q)) {
    make_widening_coercion (n, p, q);
  } else if (whether_unitable (p, derow (q), SAFE_DEFLEXING)) {
    make_uniting_coercion (n, q);
  } else if (whether_ref_row (q) && whether_strong_name (p, q)) {
    make_ref_rowing_coercion (n, p, q);
  } else if (SLICE (q) != NULL && whether_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (WHETHER (q, FLEX_SYMBOL) && whether_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (WHETHER (p, REF_SYMBOL)) {
    MOID_T *r = /* DEFLEX */ (SUB (p));
    make_coercion (n, DEREFERENCING, r);
    make_depreffing_coercion (n, r, q);
  } else if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    MOID_T *r = SUB (p);
    make_coercion (n, DEPROCEDURING, r);
    make_depreffing_coercion (n, r, q);
  } else if (p != q) {
    cannot_coerce (n, p, q, NO_SORT, SKIP_DEFLEXING, 0);
  }
}

/*!
\brief whether p is a nonproc mode (that is voided directly)
\param p mode
\return same
**/

static BOOL_T whether_nonproc (MOID_T * p)
{
  if (WHETHER (p, PROC_SYMBOL) && PACK (p) == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (whether_nonproc (SUB (p)));
  } else {
    return (A68_TRUE);
  }
}

/*!
\brief make_void: voiden in an appropriate way
\param p position in tree
\param q mode
**/

static void make_void (NODE_T * p, MOID_T * q)
{
  switch (ATTRIBUTE (p)) {
  case ASSIGNATION:
  case IDENTITY_RELATION:
  case GENERATOR:
  case CAST:
  case DENOTATION:
    {
      make_coercion (p, VOIDING, MODE (VOID));
      return;
    }
  }
/* MORFs are an involved case */
  switch (ATTRIBUTE (p)) {
  case SELECTION:
  case SLICE:
  case ROUTINE_TEXT:
  case FORMULA:
  case CALL:
  case IDENTIFIER:
    {
/* A nonproc moid value is eliminated directly */
      if (whether_nonproc (q)) {
        make_coercion (p, VOIDING, MODE (VOID));
        return;
      } else {
/* Descend the chain of e.g. REF PROC .. until a nonproc moid remains */
        MOID_T *z = q;
        while (!whether_nonproc (z)) {
          if (WHETHER (z, REF_SYMBOL)) {
            make_coercion (p, DEREFERENCING, SUB (z));
          }
          if (WHETHER (z, PROC_SYMBOL) && NODE_PACK (p) == NULL) {
            make_coercion (p, DEPROCEDURING, SUB (z));
          }
          z = SUB (z);
        }
        if (z != MODE (VOID)) {
          make_coercion (p, VOIDING, MODE (VOID));
        }
        return;
      }
    }
  }
/* All other is voided straight away */
  make_coercion (p, VOIDING, MODE (VOID));
}

/*!
\brief make strong coercion
\param n position in tree
\param p mode
\param q mode
**/

static void make_strong (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (q == MODE (VOID) && p != MODE (VOID)) {
    make_void (n, p);
  } else {
    make_depreffing_coercion (n, p, q);
  }
}

/*!
\brief mode check on bounds
\param p position in tree
**/

static void mode_check_bounds (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, MODE (INT), 0);
    mode_check_unit (p, &x, &y);
    if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&y), MODE (INT), MEEK, SAFE_DEFLEXING, UNIT);
    }
    mode_check_bounds (NEXT (p));
  } else {
    mode_check_bounds (SUB (p));
    mode_check_bounds (NEXT (p));
  }
}

/*!
\brief mode check declarer
\param p position in tree
**/

static void mode_check_declarer (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, BOUNDS)) {
    mode_check_bounds (SUB (p));
    mode_check_declarer (NEXT (p));
  } else {
    mode_check_declarer (SUB (p));
    mode_check_declarer (NEXT (p));
  }
}

/*!
\brief mode check identity declaration
\param p position in tree
**/

static void mode_check_identity_declaration (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        mode_check_declarer (SUB (p));
        mode_check_identity_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        SOID_T x, y;
        make_soid (&x, STRONG, MOID (p), 0);
        mode_check_unit (NEXT_NEXT (p), &x, &y);
        if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
          cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
        } else if (MOID (&x) != MOID (&y)) {
/* Check for instance, REF INT i = LOC REF INT */
          semantic_pitfall (NEXT_NEXT (p), MOID (&x), IDENTITY_DECLARATION, GENERATOR);
        }
        break;
      }
    default:
      {
        mode_check_identity_declaration (SUB (p));
        mode_check_identity_declaration (NEXT (p));
        break;
      }
    }
  }
}

/*!
\brief mode check variable declaration
\param p position in tree
**/

static void mode_check_variable_declaration (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        mode_check_declarer (SUB (p));
        mode_check_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
          SOID_T x, y;
          make_soid (&x, STRONG, SUB_MOID (p), 0);
          mode_check_unit (NEXT_NEXT (p), &x, &y);
          if (!whether_coercible_in_context (&y, &x, FORCE_DEFLEXING)) {
            cannot_coerce (p, MOID (&y), MOID (&x), STRONG, FORCE_DEFLEXING, UNIT);
          } else if (SUB_MOID (&x) != MOID (&y)) {
/* Check for instance, REF INT i = LOC REF INT */
            semantic_pitfall (NEXT_NEXT (p), MOID (&x), VARIABLE_DECLARATION, GENERATOR);
          }
        }
        break;
      }
    default:
      {
        mode_check_variable_declaration (SUB (p));
        mode_check_variable_declaration (NEXT (p));
        break;
      }
    }
  }
}

/*!
\brief mode check routine text
\param p position in tree
\param y resulting soid
**/

static void mode_check_routine_text (NODE_T * p, SOID_T * y)
{
  SOID_T w;
  if (WHETHER (p, PARAMETER_PACK)) {
    mode_check_declarer (SUB (p));
    FORWARD (p);
  }
  mode_check_declarer (SUB (p));
  make_soid (&w, STRONG, MOID (p), 0);
  mode_check_unit (NEXT_NEXT (p), &w, y);
  if (!whether_coercible_in_context (y, &w, FORCE_DEFLEXING)) {
    cannot_coerce (NEXT_NEXT (p), MOID (y), MOID (&w), STRONG, FORCE_DEFLEXING, UNIT);
  }
}

/*!
\brief mode check proc declaration
\param p position in tree
**/

static void mode_check_proc_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, NULL, 0);
    mode_check_routine_text (SUB (p), &y);
  } else {
    mode_check_proc_declaration (SUB (p));
    mode_check_proc_declaration (NEXT (p));
  }
}

/*!
\brief mode check brief op declaration
\param p position in tree
**/

static void mode_check_brief_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    SOID_T y;
    if (MOID (p) != MOID (NEXT_NEXT (p))) {
      SOID_T y2, x;
      make_soid (&y2, NO_SORT, MOID (NEXT_NEXT (p)), 0);
      make_soid (&x, NO_SORT, MOID (p), 0);
      cannot_coerce (NEXT_NEXT (p), MOID (&y2), MOID (&x), STRONG, SKIP_DEFLEXING, ROUTINE_TEXT);
    }
    mode_check_routine_text (SUB (NEXT_NEXT (p)), &y);
  } else {
    mode_check_brief_op_declaration (SUB (p));
    mode_check_brief_op_declaration (NEXT (p));
  }
}

/*!
\brief mode check op declaration
\param p position in tree
**/

static void mode_check_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    SOID_T y, x;
    make_soid (&x, STRONG, MOID (p), 0);
    mode_check_unit (NEXT_NEXT (p), &x, &y);
    if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
    }
  } else {
    mode_check_op_declaration (SUB (p));
    mode_check_op_declaration (NEXT (p));
  }
}

/*!
\brief mode check declaration list
\param p position in tree
**/

static void mode_check_declaration_list (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case IDENTITY_DECLARATION:
      {
        mode_check_identity_declaration (SUB (p));
        break;
      }
    case VARIABLE_DECLARATION:
      {
        mode_check_variable_declaration (SUB (p));
        break;
      }
    case MODE_DECLARATION:
      {
        mode_check_declarer (SUB (p));
        break;
      }
    case PROCEDURE_DECLARATION:
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        mode_check_proc_declaration (SUB (p));
        break;
      }
    case BRIEF_OPERATOR_DECLARATION:
      {
        mode_check_brief_op_declaration (SUB (p));
        break;
      }
    case OPERATOR_DECLARATION:
      {
        mode_check_op_declaration (SUB (p));
        break;
      }
    default:
      {
        mode_check_declaration_list (SUB (p));
        mode_check_declaration_list (NEXT (p));
        break;
      }
    }
  }
}

/*!
\brief mode check serial clause
\param r resulting soids
\param p position in tree
\param x expected soid
\param k whether statement yields a value other than VOID
**/

static void mode_check_serial (SOID_LIST_T ** r, NODE_T * p, SOID_T * x, BOOL_T k)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, INITIALISER_SERIES)) {
    mode_check_serial (r, SUB (p), x, A68_FALSE);
    mode_check_serial (r, NEXT (p), x, k);
  } else if (WHETHER (p, DECLARATION_LIST)) {
    mode_check_declaration_list (SUB (p));
  } else if (whether_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, NULL_ATTRIBUTE)) {
    mode_check_serial (r, NEXT (p), x, k);
  } else if (whether_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, NULL_ATTRIBUTE)) {
    if (NEXT (p) != NULL) {
      if (WHETHER (NEXT (p), EXIT_SYMBOL) || WHETHER (NEXT (p), END_SYMBOL) || WHETHER (NEXT (p), CLOSE_SYMBOL)) {
        mode_check_serial (r, SUB (p), x, A68_TRUE);
      } else {
        mode_check_serial (r, SUB (p), x, A68_FALSE);
      }
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      mode_check_serial (r, SUB (p), x, A68_TRUE);
    }
  } else if (WHETHER (p, LABELED_UNIT)) {
    mode_check_serial (r, SUB (p), x, k);
  } else if (WHETHER (p, UNIT)) {
    SOID_T y;
    if (k) {
      mode_check_unit (p, x, &y);
    } else {
      SOID_T w;
      make_soid (&w, STRONG, MODE (VOID), 0);
      mode_check_unit (p, &w, &y);
    }
    if (NEXT (p) != NULL) {
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      if (k) {
        add_to_soid_list (r, p, &y);
      }
    }
  }
}

/*!
\brief mode check serial clause units
\param p position in tree
\param x expected soid
\param y resulting soid
\param att attribute (SERIAL or ENQUIRY)
**/

static void mode_check_serial_units (NODE_T * p, SOID_T * x, SOID_T * y, int att)
{
  SOID_LIST_T *top_sl = NULL;
  (void) att;
  mode_check_serial (&top_sl, SUB (p), x, A68_TRUE);
  if (whether_balanced (p, top_sl, SORT (x))) {
    MOID_T *result = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), result, SERIAL_CLAUSE);
  } else {
    make_soid (y, SORT (x), MOID (x) != NULL ? MOID (x) : MODE (ERROR), 0);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check unit list
\param r resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_unit_list (SOID_LIST_T ** r, NODE_T * p, SOID_T * x)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    mode_check_unit_list (r, SUB (p), x);
    mode_check_unit_list (r, NEXT (p), x);
  } else if (WHETHER (p, COMMA_SYMBOL)) {
    mode_check_unit_list (r, NEXT (p), x);
  } else if (WHETHER (p, UNIT)) {
    SOID_T y;
    mode_check_unit (p, x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_unit_list (r, NEXT (p), x);
  }
}

/*!
\brief mode check struct display
\param r resulting soids
\param p position in tree
\param fields pack
**/

static void mode_check_struct_display (SOID_LIST_T ** r, NODE_T * p, PACK_T ** fields)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    mode_check_struct_display (r, SUB (p), fields);
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (WHETHER (p, COMMA_SYMBOL)) {
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (WHETHER (p, UNIT)) {
    SOID_T x, y;
    if (*fields != NULL) {
      make_soid (&x, STRONG, MOID (*fields), 0);
      FORWARD (*fields);
    } else {
      make_soid (&x, STRONG, NULL, 0);
    }
    mode_check_unit (p, &x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_struct_display (r, NEXT (p), fields);
  }
}

/*!
\brief mode check get specified moids
\param p position in tree
\param u united mode to add to
**/

static void mode_check_get_specified_moids (NODE_T * p, MOID_T * u)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      mode_check_get_specified_moids (SUB (p), u);
    } else if (WHETHER (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      add_mode_to_pack (&(PACK (u)), m, NULL, NODE (m));
    }
  }
}

/*!
\brief mode check specified unit list
\param r resulting soids
\param p position in tree
\param x expected soid
\param u resulting united mode
**/

static void mode_check_specified_unit_list (SOID_LIST_T ** r, NODE_T * p, SOID_T * x, MOID_T * u)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      mode_check_specified_unit_list (r, SUB (p), x, u);
    } else if (WHETHER (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      if (u != NULL && !whether_unitable (m, u, SAFE_DEFLEXING)) {
        diagnostic_node (A68_ERROR, p, ERROR_NO_COMPONENT, m, u);
      }
    } else if (WHETHER (p, UNIT)) {
      SOID_T y;
      mode_check_unit (p, x, &y);
      add_to_soid_list (r, p, &y);
    }
  }
}

/*!
\brief mode check united case parts
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_united_case_parts (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  MOID_T *u = NULL, *v = NULL, *w = NULL;
/* Check the CASE part and deduce the united mode */
  make_soid (&enq_expct, STRONG, NULL, 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
/* Deduce the united mode from the enquiry clause */
  u = depref_completely (MOID (&enq_yield));
  u = make_united_mode (u);
  u = depref_completely (u);
/* Also deduce the united mode from the specifiers */
  v = new_moid ();
  ATTRIBUTE (v) = SERIES_MODE;
  mode_check_get_specified_moids (NEXT_SUB (NEXT (p)), v);
  v = make_united_mode (v);
/* Determine a resulting union */
  if (u == MODE (HIP)) {
    w = v;
  } else {
    if (WHETHER (u, UNION_SYMBOL)) {
      BOOL_T uv, vu, some;
      investigate_firm_relations (PACK (u), PACK (v), &uv, &some);
      investigate_firm_relations (PACK (v), PACK (u), &vu, &some);
      if (uv && vu) {
/* Every component has a specifier */
        w = u;
      } else if (!uv && !vu) {
/* Hmmmm ... let the coercer sort it out */
        w = u;
      } else {
/*  This is all the balancing we allow here for the moment. Firmly related
subsets are not valid so we absorb them. If this doesn't solve it then we
get a coercion-error later */
        w = absorb_related_subsets (u);
      }
    } else {
      diagnostic_node (A68_ERROR, NEXT_SUB (p), ERROR_NO_UNION, u);
      return;
    }
  }
  MOID (SUB (p)) = w;
  FORWARD (p);
/* Check the IN part */
  mode_check_specified_unit_list (ry, NEXT_SUB (p), x, w);
/* OUSE, OUT, ESAC */
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, UNITED_OUSE_PART, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE)) {
      mode_check_united_case_parts (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check united case
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_united_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_united_case_parts (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), UNITED_CASE_CLAUSE);

    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, UNITED_CASE_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check unit list 2
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_unit_list_2 (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  if (MOID (x) != NULL) {
    if (WHETHER (MOID (x), FLEX_SYMBOL)) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (SUB_MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (WHETHER (MOID (x), ROW_SYMBOL)) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (WHETHER (MOID (x), STRUCT_SYMBOL)) {
      PACK_T *y2 = PACK (MOID (x));
      mode_check_struct_display (&top_sl, SUB (p), &y2);
    } else {
      mode_check_unit_list (&top_sl, SUB (p), x);
    }
  } else {
    mode_check_unit_list (&top_sl, SUB (p), x);
  }
  make_soid (y, STRONG, pack_soids_in_moid (top_sl, STOWED_MODE), 0);
  free_soid_list (top_sl);
}

/*!
\brief mode check closed
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_closed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, SERIAL_CLAUSE)) {
    mode_check_serial_units (p, x, y, SERIAL_CLAUSE);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
    mode_check_closed (NEXT (p), x, y);
  }
  MOID (p) = MOID (y);
}

/*!
\brief mode check collateral
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_collateral (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (whether (p, BEGIN_SYMBOL, END_SYMBOL, 0)
             || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0)) {
    if (SORT (x) == STRONG) {
      make_soid (y, STRONG, MODE (VACUUM), 0);
    } else {
      make_soid (y, STRONG, MODE (UNDEFINED), 0);
    }
  } else {
    if (WHETHER (p, UNIT_LIST)) {
      mode_check_unit_list_2 (p, x, y);
    } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
      mode_check_collateral (NEXT (p), x, y);
    }
    MOID (p) = MOID (y);
  }
}

/*!
\brief mode check conditional 2
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_conditional_2 (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, ELSE_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, ELIF_PART, BRIEF_ELIF_PART, NULL_ATTRIBUTE)) {
      mode_check_conditional_2 (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check conditional
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_conditional (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_conditional_2 (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), CONDITIONAL_CLAUSE);
    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, CONDITIONAL_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check int case 2
\param ry resulting soids
\param p position in tree
\param x expected soid
**/

static void mode_check_int_case_2 (SOID_LIST_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, STRONG, MODE (INT), 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_unit_list (ry, NEXT_SUB (p), x);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (whether_one_of (p, INTEGER_OUT_PART, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE)) {
      mode_check_int_case_2 (ry, SUB (p), x);
    }
  }
}

/*!
\brief mode check int case
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_int_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_LIST_T *top_sl = NULL;
  MOID_T *z;
  mode_check_int_case_2 (&top_sl, p, x);
  if (!whether_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NULL) {
      make_soid (y, SORT (x), MOID (x), INTEGER_CASE_CLAUSE);
    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, INTEGER_CASE_CLAUSE);
  }
  free_soid_list (top_sl);
}

/*!
\brief mode check loop 2
\param p position in tree
\param y resulting soid
**/

static void mode_check_loop_2 (NODE_T * p, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, FOR_PART)) {
    mode_check_loop_2 (NEXT (p), y);
  } else if (whether_one_of (p, FROM_PART, BY_PART, TO_PART, NULL_ATTRIBUTE)) {
    SOID_T ix, iy;
    make_soid (&ix, STRONG, MODE (INT), 0);
    mode_check_unit (NEXT_SUB (p), &ix, &iy);
    if (!whether_coercible_in_context (&iy, &ix, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_SUB (p), MOID (&iy), MODE (INT), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (WHETHER (p, WHILE_PART)) {
    SOID_T enq_expct, enq_yield;
    make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
    mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
    if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (whether_one_of (p, DO_PART, ALT_DO_PART, NULL_ATTRIBUTE)) {
    SOID_LIST_T *z = NULL;
    SOID_T ix;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&ix, STRONG, MODE (VOID), 0);
    if (WHETHER (do_p, SERIAL_CLAUSE)) {
      mode_check_serial (&z, do_p, &ix, A68_TRUE);
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
      SOID_T enq_expct, enq_yield;
      make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
      mode_check_serial_units (NEXT_SUB (un_p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
      if (!whether_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
        cannot_coerce (un_p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
      }
    }
    free_soid_list (z);
  }
}

/*!
\brief mode check loop
\param p position in tree
\param y resulting soid
**/

static void mode_check_loop (NODE_T * p, SOID_T * y)
{
  SOID_T *z = NULL;
  mode_check_loop_2 (p, /* y */ z);
  make_soid (y, STRONG, MODE (VOID), 0);
}

/*!
\brief mode check enclosed
\param p position in tree
\param x expected soid
\param y resulting soid
**/

void mode_check_enclosed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    mode_check_closed (SUB (p), x, y);
  } else if (WHETHER (p, PARALLEL_CLAUSE)) {
    mode_check_collateral (SUB (NEXT_SUB (p)), x, y);
    make_soid (y, STRONG, MODE (VOID), 0);
    MOID (NEXT_SUB (p)) = MODE (VOID);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    mode_check_collateral (SUB (p), x, y);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    mode_check_conditional (SUB (p), x, y);
  } else if (WHETHER (p, INTEGER_CASE_CLAUSE)) {
    mode_check_int_case (SUB (p), x, y);
  } else if (WHETHER (p, UNITED_CASE_CLAUSE)) {
    mode_check_united_case (SUB (p), x, y);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    mode_check_loop (SUB (p), y);
  }
  MOID (p) = MOID (y);
}

/*!
\brief search table for operator
\param t tag chain to search
\param n name of operator
\param x lhs mode
\param y rhs mode
\param deflex deflexing regime
\return tag entry or NULL
**/

static TAG_T *search_table_for_operator (TAG_T * t, char *n, MOID_T * x, MOID_T * y, int deflex)
{
  if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
  for (; t != NULL; FORWARD (t)) {
    if (SYMBOL (NODE (t)) == n) {
      PACK_T *p = PACK (MOID (t));
      if (whether_coercible (x, MOID (p), FIRM, deflex)) {
        FORWARD (p);
        if (p == NULL && y == NULL) {
/* Matched in case of a monad */
          return (t);
        } else if (p != NULL && y != NULL && whether_coercible (y, MOID (p), FIRM, deflex)) {
/* Matched in case of a nomad */
          return (t);
        }
      }
    }
  }
  return (NULL);
}

/*!
\brief search chain of symbol tables and return matching operator "x n y" or "n x"
\param s symbol table to start search
\param n name of token
\param x lhs mode
\param y rhs mode
\param deflex deflexing regime
\return tag entry or NULL
**/

static TAG_T *search_table_chain_for_operator (SYMBOL_TABLE_T * s, char *n, MOID_T * x, MOID_T * y, int deflex)
{
  if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
  while (s != NULL) {
    TAG_T *z = search_table_for_operator (s->operators, n, x, y, deflex);
    if (z != NULL) {
      return (z);
    }
    s = PREVIOUS (s);
  }
  return (NULL);
}

/*!
\brief return a matching operator "x n y"
\param s symbol table to start search
\param n name of token
\param x lhs mode
\param y rhs mode
\return tag entry or NULL
**/

static TAG_T *find_operator (SYMBOL_TABLE_T * s, char *n, MOID_T * x, MOID_T * y)
{
/* Coercions to operand modes are FIRM */
  TAG_T *z;
  MOID_T *u, *v;
/* (A) Catch exceptions first */
  if (x == NULL && y == NULL) {
    return (NULL);
  } else if (whether_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NULL && whether_mode_isnt_well (y)) {
    return (error_tag);
  }
/* (B) MONADs */
  if (x != NULL && y == NULL) {
    z = search_table_chain_for_operator (s, n, x, NULL, SAFE_DEFLEXING);
    return (z);
  }
/* (C) NOMADs */
  z = search_table_chain_for_operator (s, n, x, y, SAFE_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
/* (D) Vector and matrix "strong coercions" in standard environ */
  u = depref_completely (x);
  v = depref_completely (y);
  if ((u == MODE (ROW_REAL) || u == MODE (ROWROW_REAL))
      || (v == MODE (ROW_REAL) || v == MODE (ROWROW_REAL))
      || (u == MODE (ROW_COMPLEX) || u == MODE (ROWROW_COMPLEX))
      || (v == MODE (ROW_COMPLEX) || v == MODE (ROWROW_COMPLEX))) {
    if (u == MODE (INT)) {
      z = search_table_for_operator (stand_env->operators, n, MODE (REAL), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
      z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (v == MODE (INT)) {
      z = search_table_for_operator (stand_env->operators, n, x, MODE (REAL), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
      z = search_table_for_operator (stand_env->operators, n, x, MODE (COMPLEX), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (u == MODE (REAL)) {
      z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), y, ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    } else if (v == MODE (REAL)) {
      z = search_table_for_operator (stand_env->operators, n, x, MODE (COMPLEX), ALIAS_DEFLEXING);
      if (z != NULL) {
        return (z);
      }
    }
  }
/* (E) Look in standenv for an appropriate cross-term */
  u = make_series_from_moids (x, y);
  u = make_united_mode (u);
  v = get_balanced_mode (u, STRONG, NO_DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (stand_env->operators, n, v, v, ALIAS_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
  if (whether_coercible_series (u, MODE (REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (REAL), MODE (REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONG_REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONG_REAL), MODE (LONG_REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONGLONG_REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONGLONG_REAL), MODE (LONGLONG_REAL), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (COMPLEX), MODE (COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONG_COMPLEX), MODE (LONG_COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
  if (whether_coercible_series (u, MODE (LONGLONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (stand_env->operators, n, MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX), ALIAS_DEFLEXING);
    if (z != NULL) {
      return (z);
    }
  }
/* (F) Now allow for depreffing for REF REAL +:= INT and alike */
  v = get_balanced_mode (u, STRONG, DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (stand_env->operators, n, v, v, ALIAS_DEFLEXING);
  if (z != NULL) {
    return (z);
  }
  return (NULL);
}

/*!
\brief mode check monadic operator
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_monadic_operator (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p != NULL) {
    TAG_T *t;
    MOID_T *u;
    u = determine_unique_mode (y, SAFE_DEFLEXING);
    if (whether_mode_isnt_well (u)) {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (u == MODE (HIP)) {
      diagnostic_node (A68_ERROR, NEXT (p), ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else {
      if (a68g_strchr (NOMADS, *(SYMBOL (p))) != NULL) {
        t = NULL;
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      } else {
        t = find_operator (SYMBOL_TABLE (p), SYMBOL (p), u, NULL);
        if (t == NULL) {
          diagnostic_node (A68_ERROR, p, ERROR_NO_MONADIC, u);
          make_soid (y, SORT (x), MODE (ERROR), 0);
        }
      }
      if (t != NULL) {
        MOID (p) = MOID (t);
      }
      TAX (p) = t;
      if (t != NULL && t != error_tag) {
        MOID (p) = MOID (t);
        make_soid (y, SORT (x), SUB_MOID (t), 0);
      } else {
        MOID (p) = MODE (ERROR);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
    }
  }
}

/*!
\brief mode check monadic formula
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_monadic_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e;
  make_soid (&e, FIRM, NULL, 0);
  mode_check_formula (NEXT (p), &e, y);
  mode_check_monadic_operator (p, &e, y);
  make_soid (y, SORT (x), MOID (y), 0);
}

/*!
\brief mode check formula
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T ls, rs;
  TAG_T *op;
  MOID_T *u, *v;
  if (WHETHER (p, MONADIC_FORMULA)) {
    mode_check_monadic_formula (SUB (p), x, &ls);
  } else if (WHETHER (p, FORMULA)) {
    mode_check_formula (SUB (p), x, &ls);
  } else if (WHETHER (p, SECONDARY)) {
    SOID_T e;
    make_soid (&e, FIRM, NULL, 0);
    mode_check_unit (SUB (p), &e, &ls);
  }
  u = determine_unique_mode (&ls, SAFE_DEFLEXING);
  MOID (p) = u;
  if (NEXT (p) == NULL) {
    make_soid (y, SORT (x), u, 0);
  } else {
    NODE_T *q = NEXT_NEXT (p);
    if (WHETHER (q, MONADIC_FORMULA)) {
      mode_check_monadic_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (WHETHER (q, FORMULA)) {
      mode_check_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (WHETHER (q, SECONDARY)) {
      SOID_T e;
      make_soid (&e, FIRM, NULL, 0);
      mode_check_unit (SUB (q), &e, &rs);
    }
    v = determine_unique_mode (&rs, SAFE_DEFLEXING);
    MOID (q) = v;
    if (whether_mode_isnt_well (u) || whether_mode_isnt_well (v)) {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (u == MODE (HIP)) {
      diagnostic_node (A68_ERROR, p, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (v == MODE (HIP)) {
      diagnostic_node (A68_ERROR, q, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else {
      op = find_operator (SYMBOL_TABLE (NEXT (p)), SYMBOL (NEXT (p)), u, v);
      if (op == NULL) {
        diagnostic_node (A68_ERROR, NEXT (p), ERROR_NO_DYADIC, u, v);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
      if (op != NULL) {
        MOID (NEXT (p)) = MOID (op);
      }
      TAX (NEXT (p)) = op;
      if (op != NULL && op != error_tag) {
        make_soid (y, SORT (x), SUB_MOID (op), 0);
      } else {
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
    }
  }
}

/*!
\brief mode check assignation
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_assignation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T name, tmp, value;
  MOID_T *name_moid, *source_moid, *dest_moid, *ori;
/* Get destination mode */
  make_soid (&name, SOFT, NULL, 0);
  mode_check_unit (SUB (p), &name, &tmp);
  dest_moid = MOID (&tmp);
/* SOFT coercion */
  ori = determine_unique_mode (&tmp, SAFE_DEFLEXING);
  name_moid = deproc_completely (ori);
  if (ATTRIBUTE (name_moid) != REF_SYMBOL) {
    if (WHETHER_MODE_IS_WELL (name_moid)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_NAME, ori, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (p) = name_moid;
/* Get source mode */
  make_soid (&name, STRONG, SUB (name_moid), 0);
  mode_check_unit (NEXT_NEXT (p), &name, &value);
  if (!whether_coercible_in_context (&value, &name, FORCE_DEFLEXING)) {
    source_moid = MOID (&value);
    cannot_coerce (p, MOID (&value), MOID (&name), STRONG, FORCE_DEFLEXING, UNIT);
    make_soid (y, SORT (x), MODE (ERROR), 0);
  } else {
    make_soid (y, SORT (x), name_moid, 0);
  }
}

/*!
\brief mode check identity relation
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_identity_relation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  MOID_T *lhs, *rhs, *oril, *orir;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, SOFT, NULL, 0);
  mode_check_unit (SUB (ln), &e, &l);
  mode_check_unit (SUB (rn), &e, &r);
/* SOFT coercion */
  oril = determine_unique_mode (&l, SAFE_DEFLEXING);
  orir = determine_unique_mode (&r, SAFE_DEFLEXING);
  lhs = deproc_completely (oril);
  rhs = deproc_completely (orir);
  if (WHETHER_MODE_IS_WELL (lhs) && lhs != MODE (HIP) && ATTRIBUTE (lhs) != REF_SYMBOL) {
    diagnostic_node (A68_ERROR, ln, ERROR_NO_NAME, oril, ATTRIBUTE (SUB (ln)));
    lhs = MODE (ERROR);
  }
  if (WHETHER_MODE_IS_WELL (rhs) && rhs != MODE (HIP) && ATTRIBUTE (rhs) != REF_SYMBOL) {
    diagnostic_node (A68_ERROR, rn, ERROR_NO_NAME, orir, ATTRIBUTE (SUB (rn)));
    rhs = MODE (ERROR);
  }
  if (lhs == MODE (HIP) && rhs == MODE (HIP)) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_UNIQUE_MODE);
  }
  if (whether_coercible (lhs, rhs, STRONG, SAFE_DEFLEXING)) {
    lhs = rhs;
  } else if (whether_coercible (rhs, lhs, STRONG, SAFE_DEFLEXING)) {
    rhs = lhs;
  } else {
    cannot_coerce (NEXT (p), rhs, lhs, SOFT, SKIP_DEFLEXING, TERTIARY);
    lhs = rhs = MODE (ERROR);
  }
  MOID (ln) = lhs;
  MOID (rn) = rhs;
  make_soid (y, SORT (x), MODE (BOOL), 0);
}

/*!
\brief mode check bool functions ANDF and ORF
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_bool_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, STRONG, MODE (BOOL), 0);
  mode_check_unit (SUB (ln), &e, &l);
  if (!whether_coercible_in_context (&l, &e, SAFE_DEFLEXING)) {
    cannot_coerce (ln, MOID (&l), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  mode_check_unit (SUB (rn), &e, &r);
  if (!whether_coercible_in_context (&r, &e, SAFE_DEFLEXING)) {
    cannot_coerce (rn, MOID (&r), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  MOID (ln) = MODE (BOOL);
  MOID (rn) = MODE (BOOL);
  make_soid (y, SORT (x), MODE (BOOL), 0);
}

/*!
\brief mode check cast
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_cast (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w;
  mode_check_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  w.cast = A68_TRUE;
  mode_check_enclosed (SUB_NEXT (p), &w, y);
  if (!whether_coercible_in_context (y, &w, SAFE_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (y), MOID (&w), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
  }
  make_soid (y, SORT (x), MOID (p), 0);
}

/*!
\brief mode check assertion
\param p position in tree
**/

static void mode_check_assertion (NODE_T * p)
{
  SOID_T w, y;
  make_soid (&w, STRONG, MODE (BOOL), 0);
  mode_check_enclosed (SUB_NEXT (p), &w, &y);
  SORT (&y) = SORT (&w);        /* Patch */
  if (!whether_coercible_in_context (&y, &w, NO_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (&y), MOID (&w), MEEK, NO_DEFLEXING, ENCLOSED_CLAUSE);
  }
}

/*!
\brief mode check argument list
\param r resulting soids
\param p position in tree
\param x proc argument pack
\param v partial locale pack
\param w partial proc pack
**/

static void mode_check_argument_list (SOID_LIST_T ** r, NODE_T * p, PACK_T ** x, PACK_T ** v, PACK_T ** w)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, GENERIC_ARGUMENT_LIST)) {
      ATTRIBUTE (p) = ARGUMENT_LIST;
    }
    if (WHETHER (p, ARGUMENT_LIST)) {
      mode_check_argument_list (r, SUB (p), x, v, w);
    } else if (WHETHER (p, UNIT)) {
      SOID_T y, z;
      if (*x != NULL) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MOID (*x), NULL, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NULL, 0);
      }
      mode_check_unit (p, &z, &y);
      add_to_soid_list (r, p, &y);
    } else if (WHETHER (p, TRIMMER)) {
      SOID_T z;
      if (SUB (p) != NULL) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, ARGUMENT);
        make_soid (&z, STRONG, MODE (ERROR), 0);
        add_mode_to_pack_end (v, MODE (VOID), NULL, p);
        add_mode_to_pack_end (w, MOID (*x), NULL, p);
        FORWARD (*x);
      } else if (*x != NULL) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MODE (VOID), NULL, p);
        add_mode_to_pack_end (w, MOID (*x), NULL, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NULL, 0);
      }
      add_to_soid_list (r, p, &z);
    } else if (WHETHER (p, SUB_SYMBOL) && !program.options.brackets) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, CALL);
    }
  }
}

/*!
\brief mode check argument list 2
\param p position in tree
\param x proc argument pack
\param y soid
\param v partial locale pack
\param w partial proc pack
**/

static void mode_check_argument_list_2 (NODE_T * p, PACK_T * x, SOID_T * y, PACK_T ** v, PACK_T ** w)
{
  SOID_LIST_T *top_sl = NULL;
  mode_check_argument_list (&top_sl, SUB (p), &x, v, w);
  make_soid (y, STRONG, pack_soids_in_moid (top_sl, STOWED_MODE), 0);
  free_soid_list (top_sl);
}

/*!
\brief mode check meek int
\param p position in tree
**/

static void mode_check_meek_int (NODE_T * p)
{
  SOID_T x, y;
  make_soid (&x, STRONG, MODE (INT), 0);
  mode_check_unit (p, &x, &y);
  if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&y), MOID (&x), MEEK, SAFE_DEFLEXING, 0);
  }
}

/*!
\brief mode check trimmer
\param p position in tree
**/

static void mode_check_trimmer (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, TRIMMER)) {
    mode_check_trimmer (SUB (p));
  } else if (WHETHER (p, UNIT)) {
    mode_check_meek_int (p);
    mode_check_trimmer (NEXT (p));
  } else {
    mode_check_trimmer (NEXT (p));
  }
}

/*!
\brief mode check indexer
\param p position in tree
\param subs subscript counter
\param trims trimmer counter
**/

static void mode_check_indexer (NODE_T * p, int *subs, int *trims)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, TRIMMER)) {
    (*trims)++;
    mode_check_trimmer (SUB (p));
  } else if (WHETHER (p, UNIT)) {
    (*subs)++;
    mode_check_meek_int (p);
  } else {
    mode_check_indexer (SUB (p), subs, trims);
    mode_check_indexer (NEXT (p), subs, trims);
  }
}

/*!
\brief mode check call
\param p position in tree
\param n mode
\param x expected soid
\param y resulting soid
**/

static void mode_check_call (NODE_T * p, MOID_T * n, SOID_T * x, SOID_T * y)
{
  SOID_T d;
  MOID (p) = n;
/* "partial_locale" is the mode of the locale */
  GENIE (p)->partial_locale = new_moid ();
  ATTRIBUTE (GENIE (p)->partial_locale) = PROC_SYMBOL;
  PACK (GENIE (p)->partial_locale) = NULL;
  SUB (GENIE (p)->partial_locale) = SUB (n);
/* "partial_proc" is the mode of the resulting proc */
  GENIE (p)->partial_proc = new_moid ();
  ATTRIBUTE (GENIE (p)->partial_proc) = PROC_SYMBOL;
  PACK (GENIE (p)->partial_proc) = NULL;
  SUB (GENIE (p)->partial_proc) = SUB (n);
/* Check arguments and construct modes */
  mode_check_argument_list_2 (NEXT (p), PACK (n), &d, &PACK (GENIE (p)->partial_locale), &PACK (GENIE (p)->partial_proc));
  DIM (GENIE (p)->partial_proc) = count_pack_members (PACK (GENIE (p)->partial_proc));
  DIM (GENIE (p)->partial_locale) = count_pack_members (PACK (GENIE (p)->partial_locale));
  GENIE (p)->partial_proc = register_extra_mode (GENIE (p)->partial_proc);
  GENIE (p)->partial_locale = register_extra_mode (GENIE (p)->partial_locale);
  if (DIM (MOID (&d)) != DIM (n)) {
    diagnostic_node (A68_ERROR, p, ERROR_ARGUMENT_NUMBER, n);
    make_soid (y, SORT (x), SUB (n), 0);
/*  make_soid (y, SORT (x), MODE (ERROR), 0); */
  } else {
    if (!whether_coercible (MOID (&d), n, STRONG, ALIAS_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), n, STRONG, ALIAS_DEFLEXING, ARGUMENT);
    }
    if (DIM (GENIE (p)->partial_proc) == 0) {
      make_soid (y, SORT (x), SUB (n), 0);
    } else {
      if (program.options.portcheck) {
        diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, NEXT (p), WARNING_EXTENSION, NULL);
      }
      make_soid (y, SORT (x), GENIE (p)->partial_proc, 0);
    }
  }
}

/*!
\brief mode check slice
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL or a SLICE
**/

static void mode_check_slice (NODE_T * p, MOID_T * ori, SOID_T * x, SOID_T * y)
{
  BOOL_T whether_ref;
  int rowdim, subs, trims;
  MOID_T *m = depref_completely (ori), *n = ori;
/* WEAK coercion */
  while ((WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) || (WHETHER (n, PROC_SYMBOL) && PACK (n) == NULL)) {
    n = depref_once (n);
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_ROW_OR_PROC, n, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
  }
  MOID (p) = n;
  subs = trims = 0;
  mode_check_indexer (SUB_NEXT (p), &subs, &trims);
  if ((whether_ref = whether_ref_row (n)) != 0) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if ((subs + trims) != rowdim) {
    diagnostic_node (A68_ERROR, p, ERROR_INDEXER_NUMBER, n);
    make_soid (y, SORT (x), MODE (ERROR), 0);
  } else {
    if (subs > 0 && trims == 0) {
      ANNOTATION (NEXT (p)) = SLICE;
      m = n;
    } else {
      ANNOTATION (NEXT (p)) = TRIMMER;
      m = n;
    }
    while (subs > 0) {
      if (whether_ref) {
        m = m->name;
      } else {
        if (WHETHER (m, FLEX_SYMBOL)) {
          m = SUB (m);
        }
        m = SLICE (m);
      }
      ABEND (m == NULL, "NULL mode in mode_check_slice", NULL);
      subs--;
    }
/* A trim cannot be but deflexed
    make_soid (y, SORT (x), ANNOTATION (NEXT (p)) == TRIMMER && m->deflexed_mode != NULL ? m->deflexed_mode : m,0);
*/
    make_soid (y, SORT (x), ANNOTATION (NEXT (p)) == TRIMMER && m->trim != NULL ? m->trim : m, 0);
  }
}

/*!
\brief mode check field selection
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL, SLICE or FIELD_SELECTION
**/

static void mode_check_field_identifiers (NODE_T * p, MOID_T ** m, NODE_T ** seq)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      if (MOID (p) != MODE (ERROR)) {
        ATTRIBUTE (p) = FIELD_IDENTIFIER;
      }
      NODE_PACK (p) = NODE_PACK (SUB (p));
      SEQUENCE (*seq) = p;
      (*seq) = p;
      SUB (p) = NULL;
    } else if (WHETHER (p, TERTIARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, SECONDARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, PRIMARY)) {
      MOID (p) = (*m);
      mode_check_field_identifiers (SUB (p), m, seq);
      NODE_PACK (p) = NODE_PACK (SUB (p));
    } else if (WHETHER (p, IDENTIFIER)) {
      BOOL_T coerce;
      MOID_T *n, *str;
      PACK_T *t, *t_2;
      char *fs;
      n = (*m);
      coerce = A68_TRUE;
      while (coerce) {
        if (WHETHER (n, STRUCT_SYMBOL)) {
          coerce = A68_FALSE;
          t = PACK (n);
        } else if (WHETHER (n, REF_SYMBOL) && (WHETHER (SUB (n), ROW_SYMBOL) || WHETHER (SUB (n), FLEX_SYMBOL)) && n->multiple_mode != NULL) {
          coerce = A68_FALSE;
          t = PACK (n->multiple_mode);
        } else if ((WHETHER (n, ROW_SYMBOL) || WHETHER (n, FLEX_SYMBOL)) && n->multiple_mode != NULL) {
          coerce = A68_FALSE;
          t = PACK (n->multiple_mode);
        } else if (WHETHER (n, REF_SYMBOL) && whether_name_struct (n)) {
          coerce = A68_FALSE;
          t = PACK (n->name);
        } else if (whether_deprefable (n)) {
          coerce = A68_TRUE;
          n = SUB (n);
          t = NULL;
        } else {
          coerce = A68_FALSE;
          t = NULL;
        }
      }
      if (t == NULL) {
        if (WHETHER_MODE_IS_WELL (*m)) {
          diagnostic_node (A68_ERROR, p, ERROR_NO_STRUCT, (*m), CONSTRUCT);
        }
        (*m) = MODE (ERROR);
        return;
      }
      fs = SYMBOL (p);
      str = n;
      while (WHETHER (str, REF_SYMBOL)) {
        str = SUB (str);
      }
      if (WHETHER (str, FLEX_SYMBOL)) {
        str = SUB (str);
      }
      if (WHETHER (str, ROW_SYMBOL)) {
        str = SUB (str);
      }
      t_2 = PACK (str);
      while (t != NULL && t_2 != NULL) {
        if (TEXT (t) == fs) {
          (*m) = MOID (t);
          MOID (p) = (*m);
          NODE_PACK (p) = t_2;
          return;
        }
        FORWARD (t);
        FORWARD (t_2);
      }
      diagnostic_node (A68_ERROR, p, ERROR_NO_FIELD, str, fs);
      (*m) = MODE (ERROR);
    } else if (WHETHER (p, GENERIC_ARGUMENT) || WHETHER (p, GENERIC_ARGUMENT_LIST)) {
      mode_check_field_identifiers (SUB (p), m, seq);
    } else if (whether_one_of (p, COMMA_SYMBOL, OPEN_SYMBOL, CLOSE_SYMBOL, SUB_SYMBOL, BUS_SYMBOL, NULL)) {
      /* ok */;
    } else {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, FIELD_IDENTIFIER);
      (*m) = MODE (ERROR);
    }
  }
}

static void mode_check_field_selection (NODE_T * p, MOID_T * m, SOID_T * x, SOID_T * y)
{
  MOID_T *ori = m;
  NODE_T *seq = p;
  mode_check_field_identifiers (NEXT (p), &ori, &seq);
  MOID (p) = MOID (SUB (p));
  make_soid (y, SORT (x), ori, 0);
}

/*!
\brief mode check specification
\param p position in tree
\param x expected soid
\param y resulting soid
\return whether construct is a CALL, SLICE or FIELD_SELECTION
**/

static int mode_check_specification (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  MOID_T *m, *ori;
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (SUB (p), &w, &d);
  ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  m = depref_completely (ori);
  if (WHETHER (m, PROC_SYMBOL)) {
/* Assume CALL */
    mode_check_call (p, m, x, y);
    return (CALL);
  } else if (WHETHER (m, ROW_SYMBOL) || WHETHER (m, FLEX_SYMBOL)) {
/* Assume SLICE */
    mode_check_slice (p, ori, x, y);
    return (SLICE);
  } else if (WHETHER (m, STRUCT_SYMBOL)) {
    mode_check_field_selection (p, ori, x, y);
    return (FIELD_SELECTION);
  } else {
    if (m != MODE (ERROR)) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_MODE_SPECIFICATION, m, NULL);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return (PRIMARY);
  }
}

/*!
\brief mode check selection
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_selection (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  BOOL_T coerce;
  MOID_T *n, *str, *ori;
  PACK_T *t, *t_2;
  char *fs;
  NODE_T *secondary = SUB_NEXT (p);
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (secondary, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  coerce = A68_TRUE;
  while (coerce) {
    if (WHETHER (n, STRUCT_SYMBOL)) {
      coerce = A68_FALSE;
      t = PACK (n);
    } else if (WHETHER (n, REF_SYMBOL) && (WHETHER (SUB (n), ROW_SYMBOL) || WHETHER (SUB (n), FLEX_SYMBOL)) && n->multiple_mode != NULL) {
      coerce = A68_FALSE;
      t = PACK (n->multiple_mode);
    } else if ((WHETHER (n, ROW_SYMBOL) || WHETHER (n, FLEX_SYMBOL)) && n->multiple_mode != NULL) {
      coerce = A68_FALSE;
      t = PACK (n->multiple_mode);
    } else if (WHETHER (n, REF_SYMBOL) && whether_name_struct (n)) {
      coerce = A68_FALSE;
      t = PACK (n->name);
    } else if (whether_deprefable (n)) {
      coerce = A68_TRUE;
      n = SUB (n);
      t = NULL;
    } else {
      coerce = A68_FALSE;
      t = NULL;
    }
  }
  if (t == NULL) {
    if (WHETHER_MODE_IS_WELL (MOID (&d))) {
      diagnostic_node (A68_ERROR, secondary, ERROR_NO_STRUCT, ori, ATTRIBUTE (secondary));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (NEXT (p)) = n;
  fs = SYMBOL (SUB (p));
  str = n;
  while (WHETHER (str, REF_SYMBOL)) {
    str = SUB (str);
  }
  if (WHETHER (str, FLEX_SYMBOL)) {
    str = SUB (str);
  }
  if (WHETHER (str, ROW_SYMBOL)) {
    str = SUB (str);
  }
  t_2 = PACK (str);
  while (t != NULL && t_2 != NULL) {
    if (TEXT (t) == fs) {
      make_soid (y, SORT (x), MOID (t), 0);
      MOID (p) = MOID (t);
      NODE_PACK (SUB (p)) = t_2;
      return;
    }
    FORWARD (t);
    FORWARD (t_2);
  }
  make_soid (&d, NO_SORT, n, 0);
  diagnostic_node (A68_ERROR, p, ERROR_NO_FIELD, str, fs);
  make_soid (y, SORT (x), MODE (ERROR), 0);
}

/*!
\brief mode check diagonal
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_diagonal (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, STRONG, MODE (INT), 0);
    mode_check_unit (p, &w, &d);
    if (!whether_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 2) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  if (whether_ref) {
    n = NAME (n);
  } else {
    n = SLICE (n);
  }
  ABEND (n == NULL, "NULL mode in mode_check_diagonal", NULL);
  make_soid (y, SORT (x), n, 0);
}

/*!
\brief mode check transpose
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_transpose (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert = NEXT (p);
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 2) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  ABEND (n == NULL, "NULL mode in mode_check_transpose", NULL);
  make_soid (y, SORT (x), n, 0);
}

/*!
\brief mode check row or column function
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_row_column_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T whether_ref;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, STRONG, MODE (INT), 0);
    mode_check_unit (p, &w, &d);
    if (!whether_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NULL, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (WHETHER (n, REF_SYMBOL) && !whether_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NULL && (WHETHER (n, FLEX_SYMBOL) || (WHETHER (n, REF_SYMBOL) && WHETHER (SUB (n), FLEX_SYMBOL)))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NULL || !(SLICE (DEFLEX (n)) != NULL || whether_ref_row (n))) {
    if (WHETHER_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((whether_ref = whether_ref_row (n)) != A68_FALSE) {
    rowdim = DIM (DEFLEX (SUB (n)));
  } else {
    rowdim = DIM (DEFLEX (n));
  }
  if (rowdim != 1) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (tert) = n;
  ABEND (n == NULL, "NULL mode in mode_check_diagonal", NULL);
  make_soid (y, SORT (x), ROWED (n), 0);
}

/*!
\brief mode check format text
\param p position in tree
**/

static void mode_check_format_text (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    mode_check_format_text (SUB (p));
    if (WHETHER (p, FORMAT_PATTERN)) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (FORMAT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (ROW_INT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (INT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!whether_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    }
  }
}

/*!
\brief mode check unit
\param p position in tree
\param x expected soid
\param y resulting soid
**/

static void mode_check_unit (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NULL) {
    return;
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    mode_check_unit (SUB (p), x, y);
/* Ex primary */
  } else if (WHETHER (p, SPECIFICATION)) {
    ATTRIBUTE (p) = mode_check_specification (SUB (p), x, y);
    if (WHETHER (p, FIELD_SELECTION) && program.options.portcheck) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_EXTENSION, NULL);
    } else if (WHETHER (p, FIELD_SELECTION)) {
      diagnostic_node (A68_WARNING, p, WARNING_EXTENSION, NULL);
    }
    warn_for_voiding (p, x, y, ATTRIBUTE (p));
  } else if (WHETHER (p, CAST)) {
    mode_check_cast (SUB (p), x, y);
    warn_for_voiding (p, x, y, CAST);
  } else if (WHETHER (p, DENOTATION)) {
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, DENOTATION);
  } else if (WHETHER (p, IDENTIFIER)) {
    if ((TAX (p) == NULL) && (MOID (p) == NULL)) {
      int att = first_tag_global (SYMBOL_TABLE (p), SYMBOL (p));
      if (att == NULL_ATTRIBUTE) {
        (void) add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
        diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
        MOID (p) = MODE (ERROR);
      } else {
        TAG_T *z = find_tag_global (SYMBOL_TABLE (p), att, SYMBOL (p));
        if (att == IDENTIFIER && z != NULL) {
          MOID (p) = MOID (z);
        } else {
          (void) add_tag (SYMBOL_TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
          diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          MOID (p) = MODE (ERROR);
        }
      }
    }
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, IDENTIFIER);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (WHETHER (p, FORMAT_TEXT)) {
    mode_check_format_text (p);
    make_soid (y, SORT (x), MODE (FORMAT), 0);
    warn_for_voiding (p, x, y, FORMAT_TEXT);
/* Ex secondary */
  } else if (WHETHER (p, GENERATOR)) {
    mode_check_declarer (SUB (p));
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, GENERATOR);
  } else if (WHETHER (p, SELECTION)) {
    mode_check_selection (SUB (p), x, y);
    warn_for_voiding (p, x, y, SELECTION);
/* Ex tertiary */
  } else if (WHETHER (p, NIHIL)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  } else if (WHETHER (p, FORMULA)) {
    mode_check_formula (p, x, y);
    if (WHETHER_NOT (MOID (y), REF_SYMBOL)) {
      warn_for_voiding (p, x, y, FORMULA);
    }
  } else if (WHETHER (p, DIAGONAL_FUNCTION)) {
    mode_check_diagonal (SUB (p), x, y);
    warn_for_voiding (p, x, y, DIAGONAL_FUNCTION);
  } else if (WHETHER (p, TRANSPOSE_FUNCTION)) {
    mode_check_transpose (SUB (p), x, y);
    warn_for_voiding (p, x, y, TRANSPOSE_FUNCTION);
  } else if (WHETHER (p, ROW_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, ROW_FUNCTION);
  } else if (WHETHER (p, COLUMN_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, COLUMN_FUNCTION);
/* Ex unit */
  } else if (whether_one_of (p, JUMP, SKIP, NULL_ATTRIBUTE)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  } else if (WHETHER (p, ASSIGNATION)) {
    mode_check_assignation (SUB (p), x, y);
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    mode_check_identity_relation (SUB (p), x, y);
    warn_for_voiding (p, x, y, IDENTITY_RELATION);
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    mode_check_routine_text (SUB (p), y);
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, ROUTINE_TEXT);
  } else if (WHETHER (p, ASSERTION)) {
    mode_check_assertion (SUB (p));
    make_soid (y, STRONG, MODE (VOID), 0);
  } else if (WHETHER (p, AND_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, AND_FUNCTION);
  } else if (WHETHER (p, OR_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, OR_FUNCTION);
  }
  MOID (p) = MOID (y);
}

/*!
\brief coerce bounds
\param p position in tree
**/

static void coerce_bounds (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      SOID_T q;
      make_soid (&q, MEEK, MODE (INT), 0);
      coerce_unit (p, &q);
    } else {
      coerce_bounds (SUB (p));
    }
  }
}

/*!
\brief coerce declarer
\param p position in tree
**/

static void coerce_declarer (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOUNDS)) {
      coerce_bounds (SUB (p));
    } else {
      coerce_declarer (SUB (p));
    }
  }
}

/*!
\brief coerce identity declaration
\param p position in tree
**/

static void coerce_identity_declaration (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        coerce_declarer (SUB (p));
        coerce_identity_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        SOID_T q;
        make_soid (&q, STRONG, MOID (p), 0);
        coerce_unit (NEXT_NEXT (p), &q);
        break;
      }
    default:
      {
        coerce_identity_declaration (SUB (p));
        coerce_identity_declaration (NEXT (p));
        break;
      }
    }
  }
}

/*!
\brief coerce variable declaration
\param p position in tree
**/

static void coerce_variable_declaration (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        coerce_declarer (SUB (p));
        coerce_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
          SOID_T q;
          make_soid (&q, STRONG, SUB_MOID (p), 0);
          coerce_unit (NEXT_NEXT (p), &q);
          break;
        }
      }
    default:
      {
        coerce_variable_declaration (SUB (p));
        coerce_variable_declaration (NEXT (p));
        break;
      }
    }
  }
}

/*!
\brief coerce routine text
\param p position in tree
**/

static void coerce_routine_text (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, PARAMETER_PACK)) {
    FORWARD (p);
  }
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

/*!
\brief coerce proc declaration
\param p position in tree
**/

static void coerce_proc_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
  } else {
    coerce_proc_declaration (SUB (p));
    coerce_proc_declaration (NEXT (p));
  }
}

/*!
\brief coerce_op_declaration
\param p position in tree
**/

static void coerce_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    SOID_T q;
    make_soid (&q, STRONG, MOID (p), 0);
    coerce_unit (NEXT_NEXT (p), &q);
  } else {
    coerce_op_declaration (SUB (p));
    coerce_op_declaration (NEXT (p));
  }
}

/*!
\brief coerce brief op declaration
\param p position in tree
**/

static void coerce_brief_op_declaration (NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, DEFINING_OPERATOR)) {
    coerce_routine_text (SUB (NEXT_NEXT (p)));
  } else {
    coerce_brief_op_declaration (SUB (p));
    coerce_brief_op_declaration (NEXT (p));
  }
}

/*!
\brief coerce declaration list
\param p position in tree
**/

static void coerce_declaration_list (NODE_T * p)
{
  if (p != NULL) {
    switch (ATTRIBUTE (p)) {
    case IDENTITY_DECLARATION:
      {
        coerce_identity_declaration (SUB (p));
        break;
      }
    case VARIABLE_DECLARATION:
      {
        coerce_variable_declaration (SUB (p));
        break;
      }
    case MODE_DECLARATION:
      {
        coerce_declarer (SUB (p));
        break;
      }
    case PROCEDURE_DECLARATION:
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        coerce_proc_declaration (SUB (p));
        break;
      }
    case BRIEF_OPERATOR_DECLARATION:
      {
        coerce_brief_op_declaration (SUB (p));
        break;
      }
    case OPERATOR_DECLARATION:
      {
        coerce_op_declaration (SUB (p));
        break;
      }
    default:
      {
        coerce_declaration_list (SUB (p));
        coerce_declaration_list (NEXT (p));
        break;
      }
    }
  }
}

/*!
\brief coerce serial
\param p position in tree
\param q soid
\param k whether k yields value other than VOID
**/

static void coerce_serial (NODE_T * p, SOID_T * q, BOOL_T k)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, INITIALISER_SERIES)) {
    coerce_serial (SUB (p), q, A68_FALSE);
    coerce_serial (NEXT (p), q, k);
  } else if (WHETHER (p, DECLARATION_LIST)) {
    coerce_declaration_list (SUB (p));
  } else if (whether_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_serial (NEXT (p), q, k);
  } else if (whether_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, NULL_ATTRIBUTE)) {
    NODE_T *z = NEXT (p);
    if (z != NULL) {
      if (WHETHER (z, EXIT_SYMBOL) || WHETHER (z, END_SYMBOL) || WHETHER (z, CLOSE_SYMBOL) || WHETHER (z, OCCA_SYMBOL)) {
        coerce_serial (SUB (p), q, A68_TRUE);
      } else {
        coerce_serial (SUB (p), q, A68_FALSE);
      }
    } else {
      coerce_serial (SUB (p), q, A68_TRUE);
    }
    coerce_serial (NEXT (p), q, k);
  } else if (WHETHER (p, LABELED_UNIT)) {
    coerce_serial (SUB (p), q, k);
  } else if (WHETHER (p, UNIT)) {
    if (k) {
      coerce_unit (p, q);
    } else {
      SOID_T strongvoid;
      make_soid (&strongvoid, STRONG, MODE (VOID), 0);
      coerce_unit (p, &strongvoid);
    }
  }
}

/*!
\brief coerce closed
\param p position in tree
\param q soid
**/

static void coerce_closed (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, SERIAL_CLAUSE)) {
    coerce_serial (p, q, A68_TRUE);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_closed (NEXT (p), q);
  }
}

/*!
\brief coerce conditional
\param p position in tree
\param q soid
**/

static void coerce_conditional (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (BOOL), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_serial (NEXT_SUB (p), q, A68_TRUE);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, ELSE_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, ELIF_PART, BRIEF_ELIF_PART, NULL_ATTRIBUTE)) {
      coerce_conditional (SUB (p), q);
    }
  }
}

/*!
\brief coerce unit list
\param p position in tree
\param q soid
**/

static void coerce_unit_list (NODE_T * p, SOID_T * q)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    coerce_unit_list (SUB (p), q);
    coerce_unit_list (NEXT (p), q);
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_unit_list (NEXT (p), q);
  } else if (WHETHER (p, UNIT)) {
    coerce_unit (p, q);
    coerce_unit_list (NEXT (p), q);
  }
}

/*!
\brief coerce int case
\param p position in tree
\param q soid
**/

static void coerce_int_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (INT), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, INTEGER_OUT_PART, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE)) {
      coerce_int_case (SUB (p), q);
    }
  }
}

/*!
\brief coerce spec unit list
\param p position in tree
\param q soid
**/

static void coerce_spec_unit_list (NODE_T * p, SOID_T * q)
{
  for (; p != NULL; FORWARD (p)) {
    if (whether_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE)) {
      coerce_spec_unit_list (SUB (p), q);
    } else if (WHETHER (p, UNIT)) {
      coerce_unit (p, q);
    }
  }
}

/*!
\brief coerce united case
\param p position in tree
\param q soid
**/

static void coerce_united_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MOID (SUB (p)), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_spec_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NULL) {
    if (whether_one_of (p, OUT_PART, CHOICE, NULL_ATTRIBUTE)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (whether_one_of (p, UNITED_OUSE_PART, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE)) {
      coerce_united_case (SUB (p), q);
    }
  }
}

/*!
\brief coerce loop
\param p position in tree
**/

static void coerce_loop (NODE_T * p)
{
  if (WHETHER (p, FOR_PART)) {
    coerce_loop (NEXT (p));
  } else if (whether_one_of (p, FROM_PART, BY_PART, TO_PART, NULL_ATTRIBUTE)) {
    SOID_T w;
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (NEXT_SUB (p), &w);
    coerce_loop (NEXT (p));
  } else if (WHETHER (p, WHILE_PART)) {
    SOID_T w;
    make_soid (&w, MEEK, MODE (BOOL), 0);
    coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
    coerce_loop (NEXT (p));
  } else if (whether_one_of (p, DO_PART, ALT_DO_PART, NULL_ATTRIBUTE)) {
    SOID_T w;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&w, STRONG, MODE (VOID), 0);
    coerce_serial (do_p, &w, A68_TRUE);
    if (WHETHER (do_p, SERIAL_CLAUSE)) {
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NULL && WHETHER (un_p, UNTIL_PART)) {
      SOID_T sw;
      make_soid (&sw, MEEK, MODE (BOOL), 0);
      coerce_serial (NEXT_SUB (un_p), &sw, A68_TRUE);
    }
  }
}

/*!
\brief coerce struct display
\param r pack
\param p position in tree
**/

static void coerce_struct_display (PACK_T ** r, NODE_T * p)
{
  if (p == NULL) {
    return;
  } else if (WHETHER (p, UNIT_LIST)) {
    coerce_struct_display (r, SUB (p));
    coerce_struct_display (r, NEXT (p));
  } else if (whether_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
    coerce_struct_display (r, NEXT (p));
  } else if (WHETHER (p, UNIT)) {
    SOID_T s;
    make_soid (&s, STRONG, MOID (*r), 0);
    coerce_unit (p, &s);
    FORWARD (*r);
    coerce_struct_display (r, NEXT (p));
  }
}

/*!
\brief coerce collateral
\param p position in tree
\param q soid
**/

static void coerce_collateral (NODE_T * p, SOID_T * q)
{
  if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, 0) || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0))) {
    if (WHETHER (MOID (q), STRUCT_SYMBOL)) {
      PACK_T *t = PACK (MOID (q));
      coerce_struct_display (&t, p);
    } else if (WHETHER (MOID (q), FLEX_SYMBOL)) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (SUB_MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else if (WHETHER (MOID (q), ROW_SYMBOL)) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else {
/* if (MOID (q) != MODE (VOID)) */
      coerce_unit_list (p, q);
    }
  }
}

/*!
\brief coerce_enclosed
\param p position in tree
\param q soid
**/

void coerce_enclosed (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (SUB (p), q);
  } else if (WHETHER (p, CLOSED_CLAUSE)) {
    coerce_closed (SUB (p), q);
  } else if (WHETHER (p, COLLATERAL_CLAUSE)) {
    coerce_collateral (SUB (p), q);
  } else if (WHETHER (p, PARALLEL_CLAUSE)) {
    coerce_collateral (SUB (NEXT_SUB (p)), q);
  } else if (WHETHER (p, CONDITIONAL_CLAUSE)) {
    coerce_conditional (SUB (p), q);
  } else if (WHETHER (p, INTEGER_CASE_CLAUSE)) {
    coerce_int_case (SUB (p), q);
  } else if (WHETHER (p, UNITED_CASE_CLAUSE)) {
    coerce_united_case (SUB (p), q);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    coerce_loop (SUB (p));
  }
  MOID (p) = depref_rows (MOID (p), MOID (q));
}

/*!
\brief get monad moid
\param p position in tree
**/

static MOID_T *get_monad_moid (NODE_T * p)
{
  if (TAX (p) != NULL && TAX (p) != error_tag) {
    MOID (p) = MOID (TAX (p));
    return (MOID (PACK (MOID (p))));
  } else {
    return (MODE (ERROR));
  }
}

/*!
\brief coerce monad oper
\param p position in tree
\param q soid
**/

static void coerce_monad_oper (NODE_T * p, SOID_T * q)
{
  if (p != NULL) {
    SOID_T z;
    make_soid (&z, FIRM, MOID (PACK (MOID (TAX (p)))), 0);
    INSERT_COERCIONS (NEXT (p), MOID (q), &z);
  }
}

/*!
\brief coerce monad formula
\param p position in tree
**/

static void coerce_monad_formula (NODE_T * p)
{
  SOID_T e;
  make_soid (&e, STRONG, get_monad_moid (p), 0);
  coerce_operand (NEXT (p), &e);
  coerce_monad_oper (p, &e);
}

/*!
\brief coerce operand
\param p position in tree
\param q soid
**/

static void coerce_operand (NODE_T * p, SOID_T * q)
{
  if (WHETHER (p, MONADIC_FORMULA)) {
    coerce_monad_formula (SUB (p));
    if (MOID (p) != MOID (q)) {
      make_sub (p, p, FORMULA);
      INSERT_COERCIONS (p, MOID (p), q);
      make_sub (p, p, TERTIARY);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, SECONDARY)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
  }
}

/*!
\brief coerce formula
\param p position in tree
\param q soid
**/

static void coerce_formula (NODE_T * p, SOID_T * q)
{
  (void) q;
  if (WHETHER (p, MONADIC_FORMULA) && NEXT (p) == NULL) {
    coerce_monad_formula (SUB (p));
  } else {
    if (TAX (NEXT (p)) != NULL && TAX (NEXT (p)) != error_tag) {
      SOID_T s;
      NODE_T *op = NEXT (p), *nq = NEXT_NEXT (p);
      MOID_T *w = MOID (op);
      MOID_T *u = MOID (PACK (w)), *v = MOID (NEXT (PACK (w)));
      make_soid (&s, STRONG, u, 0);
      coerce_operand (p, &s);
      make_soid (&s, STRONG, v, 0);
      coerce_operand (nq, &s);
    }
  }
}

/*!
\brief coerce assignation
\param p position in tree
**/

static void coerce_assignation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, SOFT, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, SUB_MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

/*!
\brief coerce relation
\param p position in tree
**/

static void coerce_relation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, MOID (NEXT_NEXT (p)), 0);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

/*!
\brief coerce bool function
\param p position in tree
**/

static void coerce_bool_function (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MODE (BOOL), 0);
  coerce_unit (SUB (p), &w);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

/*!
\brief coerce assertion
\param p position in tree
**/

static void coerce_assertion (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (BOOL), 0);
  coerce_enclosed (SUB_NEXT (p), &w);
}

/*!
\brief coerce field-selection
\param p position in tree
**/

static void coerce_field_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK */ STRONG, MOID (p), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce selection
\param p position in tree
**/

static void coerce_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce cast
\param p position in tree
**/

static void coerce_cast (NODE_T * p)
{
  SOID_T w;
  coerce_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_enclosed (NEXT (p), &w);
}

/*!
\brief coerce argument list
\param r pack
\param p position in tree
**/

static void coerce_argument_list (PACK_T ** r, NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, ARGUMENT_LIST)) {
      coerce_argument_list (r, SUB (p));
    } else if (WHETHER (p, UNIT)) {
      SOID_T s;
      make_soid (&s, STRONG, MOID (*r), 0);
      coerce_unit (p, &s);
      FORWARD (*r);
    } else if (WHETHER (p, TRIMMER)) {
      FORWARD (*r);
    }
  }
}

/*!
\brief coerce call
\param p position in tree
**/

static void coerce_call (NODE_T * p)
{
  MOID_T *proc = MOID (p);
  SOID_T w;
  PACK_T *t;
  make_soid (&w, MEEK, proc, 0);
  coerce_unit (SUB (p), &w);
  FORWARD (p);
  t = PACK (proc);
  coerce_argument_list (&t, SUB (p));
}

/*!
\brief coerce meek int
\param p position in tree
**/

static void coerce_meek_int (NODE_T * p)
{
  SOID_T x;
  make_soid (&x, MEEK, MODE (INT), 0);
  coerce_unit (p, &x);
}

/*!
\brief coerce trimmer
\param p position in tree
**/

static void coerce_trimmer (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, UNIT)) {
      coerce_meek_int (p);
      coerce_trimmer (NEXT (p));
    } else {
      coerce_trimmer (NEXT (p));
    }
  }
}

/*!
\brief coerce indexer
\param p position in tree
**/

static void coerce_indexer (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, TRIMMER)) {
      coerce_trimmer (SUB (p));
    } else if (WHETHER (p, UNIT)) {
      coerce_meek_int (p);
    } else {
      coerce_indexer (SUB (p));
      coerce_indexer (NEXT (p));
    }
  }
}

/*!
\brief coerce_slice
\param p position in tree
**/

static void coerce_slice (NODE_T * p)
{
  SOID_T w;
  MOID_T *row;
  row = MOID (p);
  make_soid (&w, /* WEAK */ STRONG, row, 0);
  coerce_unit (SUB (p), &w);
  coerce_indexer (SUB_NEXT (p));
}

/*!
\brief mode coerce diagonal
\param p position in tree
**/

static void coerce_diagonal (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, /* WEAK */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief mode coerce transpose
\param p position in tree
**/

static void coerce_transpose (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief mode coerce row or column function
\param p position in tree
**/

static void coerce_row_column_function (NODE_T * p)
{
  SOID_T w;
  if (WHETHER (p, TERTIARY)) {
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, /* WEAK */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/*!
\brief coerce format text
\param p position in tree
**/

static void coerce_format_text (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    coerce_format_text (SUB (p));
    if (WHETHER (p, FORMAT_PATTERN)) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (FORMAT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (ROW_INT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (INT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    }
  }
}

/*!
\brief coerce unit
\param p position in tree
\param q soid
**/

static void coerce_unit (NODE_T * p, SOID_T * q)
{
  if (p == NULL) {
    return;
  } else if (whether_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, NULL_ATTRIBUTE)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
/* Ex primary */
  } else if (WHETHER (p, CALL)) {
    coerce_call (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, SLICE)) {
    coerce_slice (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, FIELD_SELECTION)) {
    coerce_field_selection (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, CAST)) {
    coerce_cast (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (whether_one_of (p, DENOTATION, IDENTIFIER, NULL_ATTRIBUTE)) {
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, FORMAT_TEXT)) {
    coerce_format_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (p, q);
/* Ex secondary */
  } else if (WHETHER (p, SELECTION)) {
    coerce_selection (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, GENERATOR)) {
    coerce_declarer (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
/* Ex tertiary */
  } else if (WHETHER (p, NIHIL)) {
    if (ATTRIBUTE (MOID (q)) != REF_SYMBOL && MOID (q) != MODE (VOID)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_NAME_REQUIRED);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, DIAGONAL_FUNCTION)) {
    coerce_diagonal (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, TRANSPOSE_FUNCTION)) {
    coerce_transpose (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ROW_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, COLUMN_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
/* Ex unit */
  } else if (WHETHER (p, JUMP)) {
    if (MOID (q) == MODE (PROC_VOID)) {
      make_sub (p, p, PROCEDURING);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, SKIP)) {
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, ASSIGNATION)) {
    coerce_assignation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (WHETHER (p, IDENTITY_RELATION)) {
    coerce_relation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (whether_one_of (p, AND_FUNCTION, OR_FUNCTION, NULL_ATTRIBUTE)) {
    coerce_bool_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (WHETHER (p, ASSERTION)) {
    coerce_assertion (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  }
}

/*!
\brief widen_denotation
\param p position in tree
**/

void widen_denotation (NODE_T * p)
{
#define WIDEN {\
  *q = *(SUB (q));\
  ATTRIBUTE (q) = DENOTATION;\
  MOID (q) = lm;\
  STATUS_SET (q, OPTIMAL_MASK);\
  }
#define WARN_WIDENING\
  if (program.options.portcheck && !(STATUS_TEST (SUB (q), OPTIMAL_MASK))) {\
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, q, WARNING_WIDENING_NOT_PORTABLE);\
  }
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    widen_denotation (SUB (q));
    if (WHETHER (q, WIDENING) && WHETHER (SUB (q), DENOTATION)) {
      MOID_T *lm = MOID (q), *m = MOID (SUB (q));
      if (lm == MODE (LONGLONG_INT) && m == MODE (LONG_INT)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_INT) && m == MODE (INT)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONGLONG_REAL) && m == MODE (LONG_REAL)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_REAL) && m == MODE (REAL)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_REAL) && m == MODE (LONG_INT)) {
        WIDEN;
      }
      if (lm == MODE (REAL) && m == MODE (INT)) {
        WIDEN;
      }
      if (lm == MODE (LONGLONG_BITS) && m == MODE (LONG_BITS)) {
        WARN_WIDENING;
        WIDEN;
      }
      if (lm == MODE (LONG_BITS) && m == MODE (BITS)) {
        WARN_WIDENING;
        WIDEN;
      }
      return;
    }
  }
#undef WIDEN
#undef WARN_WIDENING
}
