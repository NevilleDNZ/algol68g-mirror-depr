/**
@file syntax.c
@author J. Marcel van der Veer
@brief Hand-coded Algol 68 scanner and parser.

@section Copyright

This file is part of Algol 68 Genie - an Algol 68 compiler-interpreter.
Copyright 2001-2012 J. Marcel van der Veer <algol68g@xs4all.nl>.

@section License

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program. If not, see <http://www.gnu.org/licenses/>.

@section Description

Algol 68 grammar is defined as a two level (Van Wijngaarden) grammar that
incorporates, as syntactical rules, the "semantical" rules in 
other languages. Examples are correct use of symbols, modes and scope.
That is why so much functionality is in the "syntax.c" file. In fact, all
this material constitutes an effective "Algol 68 VW parser". This pragmatic
approach was chosen since in the early days of Algol 68, many "ab initio" 
implementations failed.

This is a Mailloux-type parser, in the sense that it scans a "phrase" for
definitions before it starts parsing, and therefore allows for tags to be used
before they are defined, which gives some freedom in top-down programming.
In 2011, FLACC documentation became available again. This documentation 
suggests that the set-up of this parser resembles that of FLACC, which 
supports the view that this is a Mailloux-type parser.

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

Next part of the parser is a recursive-descent type to check parenthesis.
Also a first set-up is made of symbol tables, needed by the bottom-up parser.
Next part is the bottom-up parser, that parses without knowing about modes while
parsing and reducing. It can therefore not exchange "[]" with "()" as was
blessed by the Revised Report. This is solved by treating CALL and SLICE as
equivalent for the moment and letting the mode checker sort it out later.

Parsing progresses in various phases to avoid spurious diagnostics from a
recovering parser. Every phase "tightens" the grammar more.
An error in any phase makes the parser quit when that phase ends.
The parser is forgiving in case of superfluous semicolons.

So those are the parser phases:

 (1) Parenthesis are checked to see whether they match. Then, a top-down 
     parser determines the basic-block structure of the program
     so symbol tables can be set up that the bottom-up parser will consult
     as you can define things before they are applied.

 (2) A bottom-up parser resolves the structure of the program.

 (3) After the symbol tables have been finalised, a small rearrangement of the
     tree may be required where JUMPs have no GOTO. This leads to the
     non-standard situation that JUMPs without GOTO can have the syntactic
     position of a PRIMARY, SECONDARY or TERTIARY. The bottom-up parser also
     does not check VICTAL correctness of declarers. This is done separately. 
     Also structure of format texts is checked separately.

The parser sets up symbol tables and populates them as far as needed to parse
the source. After the bottom-up parser terminates succesfully, the symbol tables
are completed.

 (4) Next, modes are collected and rules for well-formedness and structural 
     equivalence are applied. Then the symbol-table is completed now moids are 
     all known.

 (5) Next phases are the mode checker and coercion inserter. The syntax tree is 
     traversed to determine and check all modes, and to select operators. Then 
     the tree is traversed again to insert coercions.

 (6) A static scope checker detects where objects are transported out of scope.
     At run time, a dynamic scope checker will check that what the static scope 
     checker cannot see.

With respect to the mode checker: Algol 68 contexts are SOFT, WEAK, MEEK, FIRM 
and STRONG. These contexts are increasing in strength:

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
needs dynamic scope checking. This phase concludes the parser.
**/

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

static MOID_T *get_mode_from_declarer (NODE_T *);

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
TAG_T *error_tag;

static SOID_T *top_soid_list = NO_SOID;

static BOOL_T basic_coercions (MOID_T *, MOID_T *, int, int);
static BOOL_T is_coercible (MOID_T *, MOID_T *, int, int);
static BOOL_T is_nonproc (MOID_T *);
static void mode_check_enclosed (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_unit (NODE_T *, SOID_T *, SOID_T *);
static void mode_check_formula (NODE_T *, SOID_T *, SOID_T *);
static void coerce_enclosed (NODE_T *, SOID_T *);
static void coerce_operand (NODE_T *, SOID_T *);
static void coerce_formula (NODE_T *, SOID_T *);
static void coerce_unit (NODE_T *, SOID_T *);

#define DEPREF A68_TRUE
#define NO_DEPREF A68_FALSE

#define IF_MODE_IS_WELL(n) (! ((n) == MODE (ERROR) || (n) == MODE (UNDEFINED)))
#define INSERT_COERCIONS(n, p, q) make_strong ((n), (p), MOID (q))

#define STOP_CHAR 127

#define IN_PRELUDE(p) (LINE_NUMBER (p) <= 0)
#define EOL(c) ((c) == NEWLINE_CHAR || (c) == NULL_CHAR)

static BOOL_T stop_scanner = A68_FALSE, read_error = A68_FALSE, no_preprocessing = A68_FALSE;
static char *scan_buf;
static int max_scan_buf_length, source_file_size;
static int reductions = 0;
static jmp_buf bottom_up_crash_exit, top_down_crash_exit;

static BOOL_T victal_check_declarer (NODE_T *, int);
static NODE_T *reduce_dyadic (NODE_T *, int u);
static NODE_T *top_down_loop (NODE_T *);
static NODE_T *top_down_skip_unit (NODE_T *);
static void append_source_line (char *, LINE_T **, int *, char *);
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
static void reduce_declaration_lists (NODE_T *);
static void reduce_declarers (NODE_T *, int);
static void reduce_enclosed_clauses (NODE_T *, int);
static void reduce_enquiry_clauses (NODE_T *);
static void reduce_erroneous_units (NODE_T *);
static void reduce_format_texts (NODE_T *);
static void reduce_formulae (NODE_T *);
static void reduce_generic_arguments (NODE_T *);
static void reduce_primaries (NODE_T *, int);
static void reduce_primary_parts (NODE_T *, int);
static void reduce_right_to_left_constructs (NODE_T * q);
static void reduce_secondaries (NODE_T *);
static void reduce_serial_clauses (NODE_T *);
static void reduce_branch (NODE_T *, int);
static void reduce_tertiaries (NODE_T *);
static void reduce_units (NODE_T *);
static void reduce (NODE_T *, void (*)(NODE_T *), BOOL_T *, ...);

/* Standard environ */

static char *bold_prelude_start[] = {
  "BEGIN MODE DOUBLE = LONG REAL, QUAD = LONG LONG REAL;",
  "      start: commence:",
  "      BEGIN",
  NO_TEXT
  };

static char *bold_postlude[] = {
  "      END;",
  "      stop: abort: halt: SKIP",
  "END",
  NO_TEXT
  };

static char *quote_prelude_start[] = {
  "'BEGIN' 'MODE' 'DOUBLE' = 'LONG' 'REAL',"
  "               'QUAD' = 'LONG' 'LONG' 'REAL',"
  "               'DEVICE' = 'FILE',"
  "               'TEXT' = 'STRING';"
  "        START: COMMENCE:"
  "        'BEGIN'",
  NO_TEXT
  };

static char *quote_postlude[] = {
  "     'END';",
  "     STOP: ABORT: HALT: 'SKIP'",
  "'END'",
  NO_TEXT
  };

/**
@brief Is_ref_refety_flex.
@param m Mode under test.
@return See brief description.
**/

static BOOL_T is_ref_refety_flex (MOID_T * m)
{
  if (IS_REF_FLEX (m)) {
    return (A68_TRUE);
  } else if (IS (m, REF_SYMBOL)) {
    return (is_ref_refety_flex (SUB (m)));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Count pictures.
@param p Node in syntax tree.
@param k Counter.
**/

static void count_pictures (NODE_T * p, int *k)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PICTURE)) {
      (*k)++;
    }
    count_pictures (SUB (p), k);
  }
}

/**
@brief Count number of operands in operator parameter list.
@param p Node in syntax tree.
@return See brief description.
**/

static int count_operands (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, DECLARER)) {
      return (count_operands (NEXT (p)));
    } else if (IS (p, COMMA_SYMBOL)) {
      return (1 + count_operands (NEXT (p)));
    } else {
      return (count_operands (NEXT (p)) + count_operands (SUB (p)));
    }
  } else {
    return (0);
  }
}

/**
@brief Count formal bounds in declarer in tree.
@param p Node in syntax tree.
@return See brief description.
**/

static int count_formal_bounds (NODE_T * p)
{
  if (p == NO_NODE) {
    return (0);
  } else {
    if (IS (p, COMMA_SYMBOL)) {
      return (1);
    } else {
      return (count_formal_bounds (NEXT (p)) + count_formal_bounds (SUB (p)));
    }
  }
}

/**
@brief Count bounds in declarer in tree.
@param p Node in syntax tree.
@return See brief description.
**/

static int count_bounds (NODE_T * p)
{
  if (p == NO_NODE) {
    return (0);
  } else {
    if (IS (p, BOUND)) {
      return (1 + count_bounds (NEXT (p)));
    } else {
      return (count_bounds (NEXT (p)) + count_bounds (SUB (p)));
    }
  }
}

/**
@brief Count number of SHORTs or LONGs.
@param p Node in syntax tree.
@return See brief description.
**/

static int count_sizety (NODE_T * p)
{
  if (p == NO_NODE) {
    return (0);
  } else if (IS (p, LONGETY)) {
    return (count_sizety (SUB (p)) + count_sizety (NEXT (p)));
  } else if (IS (p, SHORTETY)) {
    return (count_sizety (SUB (p)) + count_sizety (NEXT (p)));
  } else if (IS (p, LONG_SYMBOL)) {
    return (1);
  } else if (IS (p, SHORT_SYMBOL)) {
    return (-1);
  } else {
    return (0);
  }
}

/**
@brief Count moids in a pack.
@param u Pack.
@return See brief description.
**/

int count_pack_members (PACK_T * u)
{
  int k = 0;
  for (; u != NO_PACK; FORWARD (u)) {
    k++;
  }
  return (k);
}

/**
@brief Replace a mode by its equivalent mode.
@param m Mode to replace.
**/

static void resolve_equivalent (MOID_T ** m)
{
  while ((*m) != NO_MOID && EQUIVALENT ((*m)) != NO_MOID && (*m) != EQUIVALENT (*m)) {
    (*m) = EQUIVALENT (*m);
  }
}

/**
@brief Save scanner state, for character look-ahead.
@param ref_l Source line.
@param ref_s Position in source line text.
@param ch Last scanned character.
**/

static void save_state (LINE_T * ref_l, char *ref_s, char ch)
{
  SCAN_STATE_L (&program) = ref_l;
  SCAN_STATE_S (&program) = ref_s;
  SCAN_STATE_C (&program) = ch;
}

/**
@brief Restore scanner state, for character look-ahead.
@param ref_l Source line.
@param ref_s Position in source line text.
@param ch Last scanned character.
**/

static void restore_state (LINE_T ** ref_l, char **ref_s, char *ch)
{
  *ref_l = SCAN_STATE_L (&program);
  *ref_s = SCAN_STATE_S (&program);
  *ch = SCAN_STATE_C (&program);
}

/**************************************/
/* Scanner, tokenises the source code */
/**************************************/

/**
@brief Whether ch is unworthy.
@param u Source line with error.
@param v Character in line.
@param ch
**/

static void unworthy (LINE_T * u, char *v, char ch)
{
  if (IS_PRINT (ch)) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s", ERROR_UNWORTHY_CHARACTER) >= 0);
  } else {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s %s", ERROR_UNWORTHY_CHARACTER, ctrl_char (ch)) >= 0);
  }
  scan_error (u, v, edit_line);
}

/**
@brief Concatenate lines that terminate in '\' with next line.
@param top Top source line.
**/

static void concatenate_lines (LINE_T * top)
{
  LINE_T *q;
/* Work from bottom backwards */
  for (q = top; q != NO_LINE && NEXT (q) != NO_LINE; q = NEXT (q)) {
    ;
  }
  for (; q != NO_LINE; BACKWARD (q)) {
    char *z = STRING (q);
    int len = (int) strlen (z);
    if (len >= 2 && z[len - 2] == BACKSLASH_CHAR && z[len - 1] == NEWLINE_CHAR && NEXT (q) != NO_LINE && STRING (NEXT (q)) != NO_TEXT) {
      z[len - 2] = NULL_CHAR;
      len += (int) strlen (STRING (NEXT (q)));
      z = (char *) get_fixed_heap_space ((size_t) (len + 1));
      bufcpy (z, STRING (q), len + 1);
      bufcat (z, STRING (NEXT (q)), len + 1);
      STRING (NEXT (q))[0] = NULL_CHAR;
      STRING (q) = z;
    }
  }
}

/**
@brief Whether u is bold tag v, independent of stropping regime.
@param u Symbol under test.
@param v Bold symbol .
@return Whether u is v.
**/

static BOOL_T is_bold (char *u, char *v)
{
  unsigned len = (unsigned) strlen (v);
  if (OPTION_STROPPING (&program) == QUOTE_STROPPING) {
    if (u[0] == '\'') {
      return ((BOOL_T) (strncmp (++u, v, len) == 0 && u[len] == '\''));
    } else {
      return (A68_FALSE);
    }
  } else {
    return ((BOOL_T) (strncmp (u, v, len) == 0 && !IS_UPPER (u[len])));
  }
}

/**
@brief Skip string.
@param top Current source line.
@param ch Current character in source line.
@return Whether string is properly terminated.
**/

static BOOL_T skip_string (LINE_T ** top, char **ch)
{
  LINE_T *u = *top;
  char *v = *ch;
  v++;
  while (u != NO_LINE) {
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
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  return (A68_FALSE);
}

/**
@brief Skip comment.
@param top Current source line.
@param ch Current character in source line.
@param delim Expected terminating delimiter.
@return Whether comment is properly terminated.
**/

static BOOL_T skip_comment (LINE_T ** top, char **ch, int delim)
{
  LINE_T *u = *top;
  char *v = *ch;
  v++;
  while (u != NO_LINE) {
    while (v[0] != NULL_CHAR) {
      if (is_bold (v, "COMMENT") && delim == BOLD_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (is_bold (v, "CO") && delim == STYLE_I_COMMENT_SYMBOL) {
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
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  return (A68_FALSE);
}

/**
@brief Skip rest of pragmat.
@param top Current source line.
@param ch Current character in source line.
@param delim Expected terminating delimiter.
@param whitespace Whether other pragmat items are allowed.
@return Whether pragmat is properly terminated.
**/

static BOOL_T skip_pragmat (LINE_T ** top, char **ch, int delim, BOOL_T whitespace)
{
  LINE_T *u = *top;
  char *v = *ch;
  while (u != NO_LINE) {
    while (v[0] != NULL_CHAR) {
      if (is_bold (v, "PRAGMAT") && delim == BOLD_PRAGMAT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (is_bold (v, "PR")
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
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  return (A68_FALSE);
}

/**
@brief Return pointer to next token within pragmat.
@param top Current source line.
@param ch Current character in source line.
@return Pointer to next item, NO_TEXT if none remains.
**/

static char *get_pragmat_item (LINE_T ** top, char **ch)
{
  LINE_T *u = *top; 
  char *v = *ch;
  while (u != NO_LINE) {
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
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  return (NO_TEXT);
}

/**
@brief Case insensitive strncmp for at most the number of chars in 'v'.
@param u String 1, must not be NO_TEXT.
@param v String 2, must not be NO_TEXT.
@return Alphabetic difference between 1 and 2.
**/

static int streq (char *u, char *v)
{
  int diff;
  for (diff = 0; diff == 0 && u[0] != NULL_CHAR && v[0] != NULL_CHAR; u++, v++) {
    diff = ((int) TO_LOWER (u[0])) - ((int) TO_LOWER (v[0]));
  }
  return (diff);
}

/**
@brief Scan for next pragmat and yield first pragmat item.
@param top Current source line.
@param ch Current character in source line.
@param delim Expected terminating delimiter.
@return Pointer to next item or NO_TEXT if none remain.
**/

static char *next_preprocessor_item (LINE_T ** top, char **ch, int *delim)
{
  LINE_T *u = *top;
  char *v = *ch;
  *delim = 0;
  while (u != NO_LINE) {
    while (v[0] != NULL_CHAR) {
      LINE_T *start_l = u;
      char *start_c = v;
/* STRINGs must be skipped */
      if (v[0] == QUOTE_CHAR) {
        SCAN_ERROR (!skip_string (&u, &v), start_l, start_c, ERROR_UNTERMINATED_STRING);
      }
/* COMMENTS must be skipped */
      else if (is_bold (v, "COMMENT")) {
        SCAN_ERROR (!skip_comment (&u, &v, BOLD_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (is_bold (v, "CO")) {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_I_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (v[0] == '#') {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_II_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (is_bold (v, "PRAGMAT") || is_bold (v, "PR")) {
/* We caught a PRAGMAT */
        char *item;
        if (is_bold (v, "PRAGMAT")) {
          *delim = BOLD_PRAGMAT_SYMBOL;
          v = &v[strlen ("PRAGMAT")];
        } else if (is_bold (v, "PR")) {
          *delim = STYLE_I_PRAGMAT_SYMBOL;
          v = &v[strlen ("PR")];
        }
        item = get_pragmat_item (&u, &v);
        SCAN_ERROR (item == NO_TEXT, start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
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
    if (u != NO_LINE) {
      v = &(STRING (u)[0]);
    } else {
      v = NO_TEXT;
    }
  }
  *top = u;
  *ch = v;
  return (NO_TEXT);
}

/**
@brief Include files.
@param top Top source line.
**/

static void include_files (LINE_T * top)
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
    LINE_T *s, *t, *u = top;
    char *v = &(STRING (u)[0]);
    make_pass = A68_FALSE;
    RESET_ERRNO;
    while (u != NO_LINE) {
      int pr_lim;
      char *item = next_preprocessor_item (&u, &v, &pr_lim);
      LINE_T *start_l = u;
      char *start_c = v;
/* Search for PR include "filename" PR */
      if (item != NO_TEXT && (streq (item, "INCLUDE") == 0 || streq (item, "READ") == 0)) {
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
        fnwid = (int) strlen (fnb) + 1;
        fn = (char *) get_fixed_heap_space ((size_t) fnwid);
        bufcpy (fn, fnb, fnwid);
/* Recursive include? Then *ignore* the file */
        for (t = top; t != NO_LINE; t = NEXT (t)) {
          if (strcmp (FILENAME (t), fn) == 0) {
            goto search_next_pragmat; /* Eeek! */
          }
        }
/* Access the file */
        RESET_ERRNO;
        fd = open (fn, O_RDONLY | O_BINARY);
        ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s \"%s\"", ERROR_SOURCE_FILE_OPEN, fn) >= 0);
        SCAN_ERROR (fd == -1, start_l, start_c, edit_line);
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
          scan_buf = (char *) get_temp_heap_space ((unsigned) (8 + max_scan_buf_length));
        }
/* Link all lines into the list */
        linum = 1;
        s = u;
        t = PREVIOUS (u);
        k = 0;
        if (fsize == 0) {
/* If file is empty, insert single empty line */
          scan_buf[0] = NEWLINE_CHAR;
          scan_buf[1] = NULL_CHAR;
          append_source_line (scan_buf, &t, &linum, fn);
        } else while (k < fsize) {
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
    search_next_pragmat:/* skip */ ;
    }
  }
}

/**
@brief Append a source line to the internal source file.
@param str Text line to be appended.
@param ref_l Previous source line.
@param line_num Previous source line number.
@param filename Name of file being read.
**/

static void append_source_line (char *str, LINE_T ** ref_l, int *line_num, char *filename)
{
  LINE_T *z = new_source_line ();
/* Allow shell command in first line, f.i. "#!/usr/share/bin/a68g" */
  if (*line_num == 1) {
    if (strlen (str) >= 2 && strncmp (str, "#!", 2) == 0) {
      ABEND (strstr (str, "run-script") != NO_TEXT, ERROR_SHELL_SCRIPT, NO_TEXT);
      (*line_num)++;
      return;
    }
  }
/* Link line into the chain */
  STRING (z) = new_fixed_string (str);
  FILENAME (z) = filename;
  NUMBER (z) = (*line_num)++;
  PRINT_STATUS (z) = NOT_PRINTED;
  LIST (z) = A68_TRUE;
  DIAGNOSTICS (z) = NO_DIAGNOSTIC;
  NEXT (z) = NO_LINE;
  PREVIOUS (z) = *ref_l;
  if (TOP_LINE (&program) == NO_LINE) {
    TOP_LINE (&program) = z;
  }
  if (*ref_l != NO_LINE) {
    NEXT (*ref_l) = z;
  }
  *ref_l = z;
}

/**
@brief Size of source file.
@return Size of file.
**/

static int get_source_size (void)
{
  FILE_T f = FILE_SOURCE_FD (&program);
/* This is why WIN32 must open as "read binary" */
  return ((int) lseek (f, 0, SEEK_END));
}

/**
@brief Append environment source lines.
@param str Line to append.
@param ref_l Source line after which to append.
@param line_num Number of source line 'ref_l'.
@param name Either "prelude" or "postlude".
**/

static void append_environ (char *str[], LINE_T ** ref_l, int *line_num, char *name)
{
  int k;
  for (k = 0; str[k] != NO_TEXT; k ++) {
    int zero_line_num = 0;
    (*line_num)++;
    append_source_line (str[k], ref_l, &zero_line_num, name);
  } 
/*
  char *text = new_string (str, NO_TEXT);
  while (text != NO_TEXT && text[0] != NULL_CHAR) {
    char *car = text;
    char *cdr = a68g_strchr (text, '!');
    int zero_line_num = 0;
    cdr[0] = NULL_CHAR;
    text = &cdr[1];
    (*line_num)++;
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s\n", car) >= 0);
    append_source_line (edit_line, ref_l, &zero_line_num, name);
  }
*/
}

/**
@brief Read script file and make internal copy.
@return Whether reading is satisfactory .
**/

static BOOL_T read_script_file (void)
{
  LINE_T * ref_l = NO_LINE;
  int k, n, num;
  unsigned len;
  BOOL_T file_end = A68_FALSE;
  char filename[BUFFER_SIZE], linenum[BUFFER_SIZE];
  char ch, * fn, * line; 
  char * buffer = (char *) get_temp_heap_space ((unsigned) (8 + source_file_size));
  FILE_T source = FILE_SOURCE_FD (&program);
  ABEND (source == -1, "source file not open", NO_TEXT);
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
    num = (int) strtol (linenum, NO_VAR, 10);
    ABEND (errno == ERANGE, "strange line number", NO_TEXT);
/* COPY original line into buffer */
    ASSERT (io_read (source, &ch, 1) == 1);
    line = &buffer[n];
    while (ch != NEWLINE_CHAR) {
      buffer[n++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
      ABEND ((unsigned) n >= len, "buffer overflow", NO_TEXT);
    }
    buffer[n++] = NEWLINE_CHAR;
    buffer[n] = NULL_CHAR;
    append_source_line (line, &ref_l, &num, fn);
  }
  return (A68_TRUE);
}

/**
@brief Read source file and make internal copy.
@return Whether reading is satisfactory .
**/

static BOOL_T read_source_file (void)
{
  LINE_T *ref_l = NO_LINE;
  int line_num = 0, k, bytes_read;
  ssize_t l;
  FILE_T f = FILE_SOURCE_FD (&program);
  char **prelude_start, **postlude, *buffer;
/* Prelude */
  if (OPTION_STROPPING (&program) == UPPER_STROPPING) {
    prelude_start = bold_prelude_start;
    postlude = bold_postlude;
  } else if (OPTION_STROPPING (&program) == QUOTE_STROPPING) {
    prelude_start = quote_prelude_start;
    postlude = quote_postlude;
  } else {
    prelude_start = postlude = NO_VAR;
  }
  append_environ (prelude_start, &ref_l, &line_num, "prelude");
/* Read the file into a single buffer, so we save on system calls */
  line_num = 1;
  buffer = (char *) get_temp_heap_space ((unsigned) (8 + source_file_size));
  RESET_ERRNO;
  ASSERT (lseek (f, 0, SEEK_SET) >= 0);
  ABEND (errno != 0, "error while reading source file", NO_TEXT);
  RESET_ERRNO;
  bytes_read = (int) io_read (f, buffer, (size_t) source_file_size);
  ABEND (errno != 0 || bytes_read != source_file_size, "error while reading source file", NO_TEXT);
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
    append_source_line (scan_buf, &ref_l, &line_num, FILE_SOURCE_NAME (&program));
    SCAN_ERROR (l != (ssize_t) strlen (scan_buf), NO_LINE, NO_TEXT, ERROR_FILE_SOURCE_CTRL);
  }
/* Postlude */
  append_environ (postlude, &ref_l, &line_num, "postlude");
/* Concatenate lines */
  concatenate_lines (TOP_LINE (&program));
/* Include files */
  include_files (TOP_LINE (&program));
  return (A68_TRUE);
}

/**
@brief Next_char get next character from internal copy of source file.
@param ref_l Source line we're scanning.
@param ref_s Character (in source line) we're scanning.
@param allow_typo Whether typographical display features are allowed.
@return Next char on input.
**/

static char next_char (LINE_T ** ref_l, char **ref_s, BOOL_T allow_typo)
{
  char ch;
#if defined NO_TYPO
  allow_typo = A68_FALSE;
#endif
  LOW_STACK_ALERT (NO_NODE);
/* Source empty? */
  if (*ref_l == NO_LINE) {
    return (STOP_CHAR);
  } else {
    LIST (*ref_l) = (BOOL_T) (OPTION_NODEMASK (&program) & SOURCE_MASK ? A68_TRUE : A68_FALSE);
/* Take new line? */
    if ((*ref_s)[0] == NEWLINE_CHAR || (*ref_s)[0] == NULL_CHAR) {
      *ref_l = NEXT (*ref_l);
      if (*ref_l == NO_LINE) {
        return (STOP_CHAR);
      }
      *ref_s = STRING (*ref_l);
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

/**
@brief Find first character that can start a valid symbol.
@param ref_c Pointer to character.
@param ref_l Source line we're scanning.
@param ref_s Character (in source line) we're scanning.
**/

static void get_good_char (char *ref_c, LINE_T ** ref_l, char **ref_s)
{
  while (*ref_c != STOP_CHAR && (IS_SPACE (*ref_c) || (*ref_c == NULL_CHAR))) {
    if (*ref_l != NO_LINE) {
      LIST (*ref_l) = (BOOL_T) (OPTION_NODEMASK (&program) & SOURCE_MASK ? A68_TRUE : A68_FALSE);
    }
    *ref_c = next_char (ref_l, ref_s, A68_FALSE);
  }
}

/**
@brief Handle a pragment (pragmat or comment).
@param type Type of pragment (#, CO, COMMENT, PR, PRAGMAT).
@param ref_l Source line we're scanning.
@param ref_c Character (in source line) we're scanning.
@return Pragment text as a string for binding in the tree.
**/

static char *pragment (int type, LINE_T ** ref_l, char **ref_c)
{
#define INIT_BUFFER {chars_in_buf = 0; scan_buf[chars_in_buf] = NULL_CHAR;}
#define ADD_ONE_CHAR(ch) {scan_buf[chars_in_buf ++] = ch; scan_buf[chars_in_buf] = NULL_CHAR;}
  char c = **ref_c, *term_s = NO_TEXT, *start_c = *ref_c;
  char *z;
  LINE_T *start_l = *ref_l;
  int term_s_length, chars_in_buf;
  BOOL_T stop, pragmat = A68_FALSE;
/* Set terminator */
  if (OPTION_STROPPING (&program) == UPPER_STROPPING) {
    if (type == STYLE_I_COMMENT_SYMBOL) {
      term_s = "CO";
    } else if (type == STYLE_II_COMMENT_SYMBOL) {
      term_s = "#";
    } else if (type == BOLD_COMMENT_SYMBOL) {
      term_s = "COMMENT";
    } else if (type == STYLE_I_PRAGMAT_SYMBOL) {
      term_s = "PR";
      pragmat = A68_TRUE;
    } else if (type == BOLD_PRAGMAT_SYMBOL) {
      term_s = "PRAGMAT";
      pragmat = A68_TRUE;
    }
  } else if (OPTION_STROPPING (&program) == QUOTE_STROPPING) {
    if (type == STYLE_I_COMMENT_SYMBOL) {
      term_s = "'CO'";
    } else if (type == STYLE_II_COMMENT_SYMBOL) {
      term_s = "#";
    } else if (type == BOLD_COMMENT_SYMBOL) {
      term_s = "'COMMENT'";
    } else if (type == STYLE_I_PRAGMAT_SYMBOL) {
      term_s = "'PR'";
      pragmat = A68_TRUE;
    } else if (type == BOLD_PRAGMAT_SYMBOL) {
      term_s = "'PRAGMAT'";
      pragmat = A68_TRUE;
    }
  }
  term_s_length = (int) strlen (term_s);
/* Scan for terminator */
  INIT_BUFFER;
  stop = A68_FALSE;
  while (stop == A68_FALSE) {
    SCAN_ERROR (c == STOP_CHAR, start_l, start_c, ERROR_UNTERMINATED_PRAGMENT);
/* A ".." or '..' delimited string in a PRAGMAT */
    if (pragmat && (c == QUOTE_CHAR || 
        (c == '\'' && OPTION_STROPPING (&program) == UPPER_STROPPING))) {
      char delim = c;
      BOOL_T eos = A68_FALSE;
      ADD_ONE_CHAR (c);
      c = next_char (ref_l, ref_c, A68_FALSE);
      while (!eos) {
        SCAN_ERROR (EOL (c), start_l, start_c, ERROR_LONG_STRING);
        if (c == delim) {
          ADD_ONE_CHAR (delim);
          save_state (*ref_l, *ref_c, c);
          c = next_char (ref_l, ref_c, A68_FALSE);
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
    } else if (EOL (c)) {
      ADD_ONE_CHAR (NEWLINE_CHAR);
    } else if (IS_PRINT (c) || IS_SPACE (c)) {
      ADD_ONE_CHAR (c);
    }
    if (chars_in_buf >= term_s_length) {
/* Check whether we encountered the terminator */
      stop = (BOOL_T) (strcmp (term_s, &(scan_buf[chars_in_buf - term_s_length])) == 0);
    }
    c = next_char (ref_l, ref_c, A68_FALSE);
  }
  scan_buf[chars_in_buf - term_s_length] = NULL_CHAR;
  z = new_string (term_s, scan_buf, term_s, NO_TEXT);
  if (type == STYLE_I_PRAGMAT_SYMBOL || type == BOLD_PRAGMAT_SYMBOL) {
    isolate_options (scan_buf, start_l);
  }
  return (z);
#undef ADD_ONE_CHAR
#undef INIT_BUFFER
}

/**
@brief Attribute for format item.
@param ch Format item in character form.
@return See brief description.
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

/**
@brief Whether input shows exponent character.
@param ref_l Source line we're scanning.
@param ref_s Character (in source line) we're scanning.
@param ch Last scanned char.
@return See brief description.
**/

static BOOL_T is_exp_char (LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  char exp_syms[3];
  if (OPTION_STROPPING (&program) == UPPER_STROPPING) {
    exp_syms[0] = EXPONENT_CHAR;
    exp_syms[1] = (char) TO_UPPER (EXPONENT_CHAR);
    exp_syms[2] = NULL_CHAR;
  } else {
    exp_syms[0] = (char) TO_UPPER (EXPONENT_CHAR);
    exp_syms[1] = BACKSLASH_CHAR;
    exp_syms[2] = NULL_CHAR;
  }
  save_state (*ref_l, *ref_s, *ch);
  if (strchr (exp_syms, *ch) != NO_TEXT) {
    *ch = next_char (ref_l, ref_s, A68_TRUE);
    ret = (BOOL_T) (strchr ("+-0123456789", *ch) != NO_TEXT);
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/**
@brief Whether input shows radix character.
@param ref_l Source line we're scanning.
@param ref_s Character (in source line) we're scanning.
@param ch Character to test.
@return See brief description.
**/

static BOOL_T is_radix_char (LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  save_state (*ref_l, *ref_s, *ch);
  if (OPTION_STROPPING (&program) == QUOTE_STROPPING) {
    if (*ch == TO_UPPER (RADIX_CHAR)) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("0123456789ABCDEF", *ch) != NO_TEXT);
    }
  } else {
    if (*ch == RADIX_CHAR) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("0123456789abcdef", *ch) != NO_TEXT);
    }
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/**
@brief Whether input shows decimal point.
@param ref_l Source line we're scanning.
@param ref_s Character (in source line) we're scanning.
@param ch Character to test.
@return See brief description.
**/

static BOOL_T is_decimal_point (LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  save_state (*ref_l, *ref_s, *ch);
  if (*ch == POINT_CHAR) {
    char exp_syms[3];
    if (OPTION_STROPPING (&program) == UPPER_STROPPING) {
      exp_syms[0] = EXPONENT_CHAR;
      exp_syms[1] = (char) TO_UPPER (EXPONENT_CHAR);
      exp_syms[2] = NULL_CHAR;
    } else {
      exp_syms[0] = (char) TO_UPPER (EXPONENT_CHAR);
      exp_syms[1] = BACKSLASH_CHAR;
      exp_syms[2] = NULL_CHAR;
    }
    *ch = next_char (ref_l, ref_s, A68_TRUE);
    if (strchr (exp_syms, *ch) != NO_TEXT) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("+-0123456789", *ch) != NO_TEXT);
    } else {
      ret = (BOOL_T) (strchr ("0123456789", *ch) != NO_TEXT);
    }
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/**
@brief Get next token from internal copy of source file..
@param in_format Are we scanning a format text.
@param ref_l Source line we're scanning.
@param ref_s Character (in source line) we're scanning.
@param start_l Line where token starts.
@param start_c Character where token starts.
@param att Attribute designated to token.
**/

static void get_next_token (BOOL_T in_format, LINE_T ** ref_l, char **ref_s, LINE_T ** start_l, char **start_c, int *att)
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
/***************/
/* In a format */
/***************/
  if (in_format) {
    char *format_items;
    if (OPTION_STROPPING (&program) == UPPER_STROPPING) {
      format_items = "/%\\+-.abcdefghijklmnopqrstuvwxyz";
    } else {/* if (OPTION_STROPPING (&program) == QUOTE_STROPPING) */
      format_items = "/%\\+-.ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }
    if (a68g_strchr (format_items, c) != NO_TEXT) {
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
/*******************/
/* Not in a format */
/*******************/
  if (IS_UPPER (c)) {
    if (OPTION_STROPPING (&program) == UPPER_STROPPING) {
/* Upper case word - bold tag */
      while (IS_UPPER (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
      sym[0] = NULL_CHAR;
      *att = BOLD_TAG;
    } else if (OPTION_STROPPING (&program) == QUOTE_STROPPING) {
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
    if (is_decimal_point (ref_l, ref_s, &c)) {
      (sym++)[0] = '0';
      (sym++)[0] = POINT_CHAR;
      c = next_char (ref_l, ref_s, A68_TRUE);
      SCAN_DIGITS (c);
      if (is_exp_char (ref_l, ref_s, &c)) {
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
    if (is_decimal_point (ref_l, ref_s, &c)) {
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (is_exp_char (ref_l, ref_s, &c)) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = '0';
        SCAN_EXPONENT_PART (c);
        *att = REAL_DENOTATION;
      } else {
        (sym++)[0] = POINT_CHAR;
        SCAN_DIGITS (c);
        if (is_exp_char (ref_l, ref_s, &c)) {
          SCAN_EXPONENT_PART (c);
        }
        *att = REAL_DENOTATION;
      }
    } else if (is_exp_char (ref_l, ref_s, &c)) {
      SCAN_EXPONENT_PART (c);
      *att = REAL_DENOTATION;
    } else if (is_radix_char (ref_l, ref_s, &c)) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (OPTION_STROPPING (&program) == UPPER_STROPPING) {
        while (IS_DIGIT (c) || strchr ("abcdef", c) != NO_TEXT) {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_TRUE);
        }
      } else {
        while (IS_DIGIT (c) || strchr ("ABCDEF", c) != NO_TEXT) {
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
      SCAN_ERROR (*ref_l == NO_LINE, *start_l, *start_c, ERROR_UNTERMINATED_STRING);
      c = next_char (ref_l, ref_s, A68_FALSE);
      if (c == QUOTE_CHAR) {
        (sym++)[0] = QUOTE_CHAR;
      } else {
        stop = A68_TRUE;
      }
    }
    sym[0] = NULL_CHAR;
    *att = (in_format ? LITERAL : ROW_CHAR_DENOTATION);
  } else if (a68g_strchr ("#$()[]{},;@", c) != NO_TEXT) {
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
  } else if (c == '!' && OPTION_STROPPING (&program) == QUOTE_STROPPING) {
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
    if (a68g_strchr (NOMADS, c) != NO_TEXT) {
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
  } else if (a68g_strchr (MONADS, c) != NO_TEXT || a68g_strchr (NOMADS, c) != NO_TEXT) {
/* Operator */
    char *scanned = sym;
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (a68g_strchr (NOMADS, c) != NO_TEXT) {
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

/**
@brief Whether att opens an embedded clause.
@param att Attribute under test.
@return Whether att opens an embedded clause.
**/

static BOOL_T open_nested_clause (int att)
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
  }
  return (A68_FALSE);
}

/**
@brief Whether att closes an embedded clause.
@param att Attribute under test.
@return Whether att closes an embedded clause.
**/

static BOOL_T close_nested_clause (int att)
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
  }
  return (A68_FALSE);
}

/**
@brief Cast a string to lower case.
@param p String to cast.
**/

static void make_lower_case (char *p)
{
  for (; p != NO_TEXT && p[0] != NULL_CHAR; p++) {
    p[0] = (char) TO_LOWER (p[0]);
  }
}

/**
@brief Construct a linear list of tokens.
@param root Node where to insert new symbol.
@param level Current recursive descent depth.
@param in_format Whether we scan a format.
@param l Current source line.
@param s Current character in source line.
@param start_l Source line where symbol starts.
@param start_c Character where symbol starts.
**/

static void tokenise_source (NODE_T ** root, int level, BOOL_T in_format, LINE_T ** l, char **s, LINE_T ** start_l, char **start_c)
{
  char *lpr = NO_TEXT;
  int lprt = 0;
  while (l != NO_VAR && !stop_scanner) {
    int att = 0;
    get_next_token (in_format, l, s, start_l, start_c, &att);
    if (scan_buf[0] == STOP_CHAR) {
      stop_scanner = A68_TRUE;
    } else if (strlen (scan_buf) > 0 || att == ROW_CHAR_DENOTATION || att == LITERAL) {
      KEYWORD_T *kw;
      char *c = NO_TEXT;
      BOOL_T make_node = A68_TRUE;
      char *trailing = NO_TEXT;
      if (att != IDENTIFIER) {
        kw = find_keyword (top_keyword, scan_buf);
      } else {
        kw = NO_KEYWORD;
      }
      if (!(kw != NO_KEYWORD && att != ROW_CHAR_DENOTATION)) {
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
        if (IS (kw, TO_SYMBOL)) {
/* Merge GO and TO to GOTO */
          if (*root != NO_NODE && IS (*root, GO_SYMBOL)) {
            ATTRIBUTE (*root) = GOTO_SYMBOL;
            NSYMBOL (*root) = TEXT (find_keyword (top_keyword, "GOTO"));
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
            char *nlpr = pragment (ATTRIBUTE (kw), l, s);
            if (lpr == NO_TEXT || (int) strlen (lpr) == 0) {
              lpr = nlpr;
            } else {
              lpr = new_string (lpr, "\n\n", nlpr, NO_TEXT);
            }
            lprt = att;
            make_node = A68_FALSE;
          } else if (att == STYLE_I_PRAGMAT_SYMBOL || att == BOLD_PRAGMAT_SYMBOL) {
            char *nlpr = pragment (ATTRIBUTE (kw), l, s);
            if (lpr == NO_TEXT || (int) strlen (lpr) == 0) {
              lpr = nlpr;
            } else {
              lpr = new_string (lpr, "\n\n", nlpr, NO_TEXT);
            }
            lprt = att;
            if (!stop_scanner) {
              (void) set_options (OPTION_LIST (&program), A68_FALSE);
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
            GINFO (q) = NO_GINFO;
            break;
          }
          default: {
            GINFO (q) = new_genie_info ();
            break;
          }
        }
        STATUS (q) = OPTION_NODEMASK (&program);
        LINE (INFO (q)) = *start_l;
        CHAR_IN_LINE (INFO (q)) = *start_c;
        PRIO (INFO (q)) = 0;
        PROCEDURE_LEVEL (INFO (q)) = 0;
        ATTRIBUTE (q) = att;
        NSYMBOL (q) = c;
        PREVIOUS (q) = *root;
        SUB (q) = NEXT (q) = NO_NODE;
        TABLE (q) = NO_TABLE;
        MOID (q) = NO_MOID;
        TAX (q) = NO_TAG;
        if (lpr != NO_TEXT) {
          NPRAGMENT (q) = lpr;
          NPRAGMENT_TYPE (q) = lprt;
          lpr = NO_TEXT;
          lprt = 0;
        }
        if (*root != NO_NODE) {
          NEXT (*root) = q;
        }
        if (TOP_NODE (&program) == NO_NODE) {
          TOP_NODE (&program) = q;
        }
        *root = q;
        if (trailing != NO_TEXT) {
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
      } else if (in_format && open_nested_clause (att)) {
        NODE_T *z = PREVIOUS (*root);
        if (z != NO_NODE && is_one_of (z, FORMAT_ITEM_N, FORMAT_ITEM_G, FORMAT_ITEM_H, FORMAT_ITEM_F)) {
          tokenise_source (root, level, A68_FALSE, l, s, start_l, start_c);
        } else if (att == OPEN_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (OPTION_BRACKETS (&program) && att == SUB_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (OPTION_BRACKETS (&program) && att == ACCO_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        }
      } else if (!in_format && level > 0 && open_nested_clause (att)) {
        tokenise_source (root, level + 1, A68_FALSE, l, s, start_l, start_c);
      } else if (!in_format && level > 0 && close_nested_clause (att)) {
        return;
      } else if (in_format && att == CLOSE_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (OPTION_BRACKETS (&program) && in_format && att == BUS_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (OPTION_BRACKETS (&program) && in_format && att == OCCA_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      }
    }
  }
}

/**
@brief Tokenise source file, build initial syntax tree.
@return Whether tokenising ended satisfactorily.
**/

BOOL_T lexical_analyser (void)
{
  LINE_T *l, *start_l = NO_LINE;
  char *s = NO_TEXT, *start_c = NO_TEXT;
  NODE_T *root = NO_NODE;
  scan_buf = NO_TEXT;
  max_scan_buf_length = source_file_size = get_source_size ();
/* Errors in file? */
  if (max_scan_buf_length == 0) {
    return (A68_FALSE);
  }
  if (OPTION_RUN_SCRIPT (&program)) {
    scan_buf = (char *) get_temp_heap_space ((unsigned) (8 + max_scan_buf_length));
    if (!read_script_file ()) {
      return (A68_FALSE);
    }
  } else {
    max_scan_buf_length += KILOBYTE; /* for the environ, more than enough */
    scan_buf = (char *) get_temp_heap_space ((unsigned) max_scan_buf_length);
/* Errors in file? */
    if (!read_source_file ()) {
      return (A68_FALSE);
    }
  }
/* Start tokenising */
  read_error = A68_FALSE;
  stop_scanner = A68_FALSE;
  if ((l = TOP_LINE (&program)) != NO_LINE) {
    s = STRING (l);
  }
  tokenise_source (&root, 0, A68_FALSE, &l, &s, &start_l, &start_c);
  return (A68_TRUE);
}

/************************************************/
/* A small refinement preprocessor for Algol68G */
/************************************************/

/**
@brief Whether refinement terminator.
@param p Position in syntax tree.
@return See brief description.
**/

static BOOL_T is_refinement_terminator (NODE_T * p)
{
  if (IS (p, POINT_SYMBOL)) {
    if (IN_PRELUDE (NEXT (p))) {
      return (A68_TRUE);
    } else {
      return (whether (p, POINT_SYMBOL, IDENTIFIER, COLON_SYMBOL, STOP));
    }
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Get refinement definitions in the internal source.
**/

void get_refinements (void)
{
  NODE_T *p = TOP_NODE (&program);
  TOP_REFINEMENT (&program) = NO_REFINEMENT;
/* First look where the prelude ends */
  while (p != NO_NODE && IN_PRELUDE (p)) {
    FORWARD (p);
  }
/* Determine whether the program contains refinements at all */
  while (p != NO_NODE && !IN_PRELUDE (p) && !is_refinement_terminator (p)) {
    FORWARD (p);
  }
  if (p == NO_NODE || IN_PRELUDE (p)) {
    return;
  }
/* Apparently this is code with refinements */
  FORWARD (p);
  if (p == NO_NODE || IN_PRELUDE (p)) {
/* Ok, we accept a program with no refinements as well */
    return;
  }
  while (p != NO_NODE && !IN_PRELUDE (p) && whether (p, IDENTIFIER, COLON_SYMBOL, STOP)) {
    REFINEMENT_T *new_one = (REFINEMENT_T *) get_fixed_heap_space ((size_t) SIZE_AL (REFINEMENT_T)), *x;
    BOOL_T exists;
    NEXT (new_one) = NO_REFINEMENT;
    NAME (new_one) = NSYMBOL (p);
    APPLICATIONS (new_one) = 0;
    LINE_DEFINED (new_one) = LINE (INFO (p));
    LINE_APPLIED (new_one) = NO_LINE;
    NODE_DEFINED (new_one) = p;
    BEGIN (new_one) = END (new_one) = NO_NODE;
    p = NEXT_NEXT (p);
    if (p == NO_NODE) {
      diagnostic_node (A68_SYNTAX_ERROR, NO_NODE, ERROR_REFINEMENT_EMPTY);
      return;
    } else {
      BEGIN (new_one) = p;
    }
    while (p != NO_NODE && ATTRIBUTE (p) != POINT_SYMBOL) {
      END (new_one) = p;
      FORWARD (p);
    }
    if (p == NO_NODE) {
      diagnostic_node (A68_SYNTAX_ERROR, NO_NODE, ERROR_SYNTAX_EXPECTED, POINT_SYMBOL);
      return;
    } else {
      FORWARD (p);
    }
/* Do we already have one by this name */
    x = TOP_REFINEMENT (&program);
    exists = A68_FALSE;
    while (x != NO_REFINEMENT && !exists) {
      if (NAME (x) == NAME (new_one)) {
        diagnostic_node (A68_SYNTAX_ERROR, NODE_DEFINED (new_one), ERROR_REFINEMENT_DEFINED);
        exists = A68_TRUE;
      }
      FORWARD (x);
    }
/* Straight insertion in chain */
    if (!exists) {
      NEXT (new_one) = TOP_REFINEMENT (&program);
      TOP_REFINEMENT (&program) = new_one;
    }
  }
  if (p != NO_NODE && !IN_PRELUDE (p)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_REFINEMENT_INVALID);
  }
}

/**
@brief Put refinement applications in the internal source.
**/

void put_refinements (void)
{
  REFINEMENT_T *x;
  NODE_T *p, *point;
/* If there are no refinements, there's little to do */
  if (TOP_REFINEMENT (&program) == NO_REFINEMENT) {
    return;
  }
/* Initialisation */
  x = TOP_REFINEMENT (&program);
  while (x != NO_REFINEMENT) {
    APPLICATIONS (x) = 0;
    FORWARD (x);
  }
/* Before we introduce infinite loops, find where closing-prelude starts */
  p = TOP_NODE (&program);
  while (p != NO_NODE && IN_PRELUDE (p)) {
    FORWARD (p);
  }
  while (p != NO_NODE && !IN_PRELUDE (p)) {
    FORWARD (p);
  }
  ABEND (p == NO_NODE, ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
  point = p;
/* We need to substitute until the first point */
  p = TOP_NODE (&program);
  while (p != NO_NODE && ATTRIBUTE (p) != POINT_SYMBOL) {
    if (IS (p, IDENTIFIER)) {
/* See if we can find its definition */
      REFINEMENT_T *y = NO_REFINEMENT;
      x = TOP_REFINEMENT (&program);
      while (x != NO_REFINEMENT && y == NO_REFINEMENT) {
        if (NAME (x) == NSYMBOL (p)) {
          y = x;
        } else {
          FORWARD (x);
        }
      }
      if (y != NO_REFINEMENT) {
/* We found its definition */
        APPLICATIONS (y)++;
        if (APPLICATIONS (y) > 1) {
          diagnostic_node (A68_SYNTAX_ERROR, NODE_DEFINED (y), ERROR_REFINEMENT_APPLIED);
          FORWARD (p);
        } else {
/* Tie the definition in the tree */
          LINE_APPLIED (y) = LINE (INFO (p));
          if (PREVIOUS (p) != NO_NODE) {
            NEXT (PREVIOUS (p)) = BEGIN (y);
          }
          if (BEGIN (y) != NO_NODE) {
            PREVIOUS (BEGIN (y)) = PREVIOUS (p);
          }
          if (NEXT (p) != NO_NODE) {
            PREVIOUS (NEXT (p)) = END (y);
          }
          if (END (y) != NO_NODE) {
            NEXT (END (y)) = NEXT (p);
          }
          p = BEGIN (y); /* So we can substitute the refinements within */
        }
      } else {
        FORWARD (p);
      }
    } else {
      FORWARD (p);
    }
  }
/* After the point we ignore it all until the prelude */
  if (p != NO_NODE && IS (p, POINT_SYMBOL)) {
    if (PREVIOUS (p) != NO_NODE) {
      NEXT (PREVIOUS (p)) = point;
    }
    if (PREVIOUS (point) != NO_NODE) {
      PREVIOUS (point) = PREVIOUS (p);
    }
  } else {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX_EXPECTED, POINT_SYMBOL);
  }
/* Has the programmer done it well? */
  if (ERROR_COUNT (&program) == 0) {
    x = TOP_REFINEMENT (&program);
    while (x != NO_REFINEMENT) {
      if (APPLICATIONS (x) == 0) {
        diagnostic_node (A68_SYNTAX_ERROR, NODE_DEFINED (x), ERROR_REFINEMENT_NOT_APPLIED);
      }
      FORWARD (x);
    }
  }
}

/*****************************************************/
/* Top-down parser, elaborates the control structure */
/*****************************************************/

/**
@brief Insert alt equals symbol.
@param p Node after which to insert.
**/

static void insert_alt_equals (NODE_T * p)
{
  NODE_T *q = new_node ();
  *q = *p;
  INFO (q) = new_node_info ();
  *INFO (q) = *INFO (p);
  GINFO (q) = new_genie_info ();
  *GINFO (q) = *GINFO (p);
  ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
  NSYMBOL (q) = TEXT (add_token (&top_token, "="));
  NEXT (p) = q;
  PREVIOUS (q) = p;
  if (NEXT (q) != NO_NODE) {
    PREVIOUS (NEXT (q)) = q;
  }
}

/**
@brief Substitute brackets.
@param p Node in syntax tree.
**/

void substitute_brackets (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
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

/**
@brief Whether token terminates a unit.
@param p Node in syntax tree.
\return TRUE or FALSE whether token terminates a unit
**/

static BOOL_T is_unit_terminator (NODE_T * p)
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
      return (A68_TRUE);
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether token is a unit-terminator in a loop clause.
@param p Node in syntax tree.
@return Whether token is a unit-terminator in a loop clause.
**/

static BOOL_T is_loop_keyword (NODE_T * p)
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
  }
  return (A68_FALSE);
}

/**
@brief Whether token cannot follow semicolon or EXIT.
@param p Node in syntax tree.
@return Whether token cannot follow semicolon or EXIT.
**/

static BOOL_T is_semicolon_less (NODE_T * p)
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
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/**
@brief Get good attribute.
@param p Node in syntax tree.
@return See brief description.
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

/**
@brief Preferably don't put intelligible diagnostic here.
@param p Node in syntax tree.
@return See brief description.
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
  }
  return (A68_FALSE);
}

/**
@brief Intelligible diagnostic from syntax tree branch.
@param p Node in syntax tree.
@param w Where to put error message.
@return See brief description.
**/

char *phrase_to_text (NODE_T * p, NODE_T ** w)
{
#define MAX_TERMINALS 8
  int count = 0, line = -1;
  static char buffer[BUFFER_SIZE];
  for (buffer[0] = NULL_CHAR; p != NO_NODE && count < MAX_TERMINALS; FORWARD (p)) {
    if (LINE_NUMBER (p) > 0) {
      int gatt = get_good_attribute (p);
      char *z = non_terminal_string (input_line, gatt);
/* 
Where to put the error message? Bob Uzgalis noted that actual content of a 
diagnostic is not as important as accurately indicating *were* the problem is! 
*/
      if (w != NO_VAR) {
        if (count == 0 || (*w) == NO_NODE) {
          *w = p;
        } else if (dont_mark_here (*w)) {
          *w = p;
        }
      }
/* Add initiation */
      if (count == 0) {
        if (w != NO_VAR) {
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
      if (z != NO_TEXT && SUB (p) != NO_NODE) {
        if (gatt == IDENTIFIER || gatt == OPERATOR || gatt == DENOTATION) {
          ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
          bufcat (buffer, edit_line, BUFFER_SIZE);
        } else {
          if (strchr ("aeio", z[0]) != NO_TEXT) {
            bufcat (buffer, " an", BUFFER_SIZE);
          } else {
            bufcat (buffer, " a", BUFFER_SIZE);
          }
          ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " %s", z) >= 0);
          bufcat (buffer, edit_line, BUFFER_SIZE);
        }
      } else if (z != NO_TEXT && SUB (p) == NO_NODE) {
        ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      } else if (NSYMBOL (p) != NO_TEXT) {
        ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      }
/* Add "starting in line nn" */
      if (z != NO_TEXT && line != LINE_NUMBER (p)) {
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
  if (p != NO_NODE && count == MAX_TERMINALS) {
    bufcat (buffer, " etcetera", BUFFER_SIZE);
  }
  return (buffer);
}

/**
@brief Intelligible diagnostic from syntax tree branch.
@param p Node in syntax tree.
@param w Where to put error message.
@return See brief description.
**/

char *phrase_to_text_2 (NODE_T * p, NODE_T ** w)
{
#define MAX_TERMINALS 8
  int count = 0;
  static char buffer[BUFFER_SIZE];
  for (buffer[0] = NULL_CHAR; p != NO_NODE && count < MAX_TERMINALS; FORWARD (p)) {
    if (LINE_NUMBER (p) > 0) {
      char *z = non_terminal_string (input_line, ATTRIBUTE (p));
/* 
Where to put the error message? Bob Uzgalis noted that actual content of a 
diagnostic is not as important as accurately indicating *were* the problem is! 
*/
      if (w != NO_VAR) {
        if (count == 0 || (*w) == NO_NODE) {
          *w = p;
        } else if (dont_mark_here (*w)) {
          *w = p;
        }
      }
/* Add initiation */
      if (count >= 1) {
        bufcat (buffer, ",", BUFFER_SIZE);
      }
/* Attribute or symbol */
      if (z != NO_TEXT) {
          ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " %s", z) >= 0);
          bufcat (buffer, edit_line, BUFFER_SIZE);
      } else if (NSYMBOL (p) != NO_TEXT) {
        ASSERT (snprintf (edit_line, SNPRINTF_SIZE, " \"%s\"", NSYMBOL (p)) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      }
      count++;
    }
  }
  if (p != NO_NODE && count == MAX_TERMINALS) {
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

/**
@brief Intelligible diagnostics for the bracket checker.
@param txt Buffer to which to append text.
@param n Count mismatch (~= 0).
@param bra Opening bracket.
@param ket Expected closing bracket.
@return See brief description.
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

/**
@brief Diagnose brackets in local branch of the tree.
@param p Node in syntax tree.
@return Error message.
**/

static char *bracket_check_diagnose (NODE_T * p)
{
  int begins = 0, opens = 0, format_delims = 0, format_opens = 0, subs = 0, ifs = 0, cases = 0, dos = 0, accos = 0;
  for (; p != NO_NODE; FORWARD (p)) {
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

/**
@brief Driver for locally diagnosing non-matching tokens.
@param top
@param p Node in syntax tree.
@return Token from where to continue.
**/

static NODE_T *bracket_check_parse (NODE_T * top, NODE_T * p)
{
  BOOL_T ignore_token;
  for (; p != NO_NODE; FORWARD (p)) {
    int ket = STOP;
    NODE_T *q = NO_NODE;
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
    } else if (q != NO_NODE && IS (q, ket)) {
      p = q;
    } else if (q == NO_NODE) {
      char *diag = bracket_check_diagnose (top);
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_PARENTHESIS, (strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS));
      longjmp (top_down_crash_exit, 1);
    } else {
      char *diag = bracket_check_diagnose (top);
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_PARENTHESIS_2, ATTRIBUTE (q), LINE (INFO (q)), ket, (strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS));
      longjmp (top_down_crash_exit, 1);
    }
  }
  return (NO_NODE);
}

/**
@brief Driver for globally diagnosing non-matching tokens.
@param top Top node in syntax tree.
**/

void check_parenthesis (NODE_T * top)
{
  if (!setjmp (top_down_crash_exit)) {
    if (bracket_check_parse (top, top) != NO_NODE) {
      diagnostic_node (A68_SYNTAX_ERROR, top, ERROR_PARENTHESIS, INFO_MISSING_KEYWORDS);
    }
  }
}

/*
Next is a top-down parser that branches out the basic blocks.
After this we can assign symbol tables to basic blocks.
*/

/**
@brief Give diagnose from top-down parser.
@param start Embedding clause starts here.
@param posit Error issued at this point.
@param clause Type of clause being processed.
@param expected Token expected.
**/

static void top_down_diagnose (NODE_T * start, NODE_T * posit, int clause, int expected)
{
  NODE_T *issue = (posit != NO_NODE ? posit : start);
  if (expected != 0) {
    diagnostic_node (A68_SYNTAX_ERROR, issue, ERROR_EXPECTED_NEAR, expected, clause, NSYMBOL (start), LINE (INFO (start)));
  } else {
    diagnostic_node (A68_SYNTAX_ERROR, issue, ERROR_UNBALANCED_KEYWORD, clause, NSYMBOL (start), LINE (INFO (start)));
  }
}

/**
@brief Check for premature exhaustion of tokens.
@param p Node in syntax tree.
@param q
**/

static void tokens_exhausted (NODE_T * p, NODE_T * q)
{
  if (p == NO_NODE) {
    diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_KEYWORD);
    longjmp (top_down_crash_exit, 1);
  }
}

/* This part specifically branches out loop clauses */

/**
@brief Whether in cast or formula with loop clause.
@param p Node in syntax tree.
@return Number of symbols to skip.
**/

static int is_loop_cast_formula (NODE_T * p)
{
/* Accept declarers that can appear in such casts but not much more */
  if (IS (p, VOID_SYMBOL)) {
    return (1);
  } else if (IS (p, INT_SYMBOL)) {
    return (1);
  } else if (IS (p, REF_SYMBOL)) {
    return (1);
  } else if (is_one_of (p, OPERATOR, BOLD_TAG, STOP)) {
    return (1);
  } else if (whether (p, UNION_SYMBOL, OPEN_SYMBOL, STOP)) {
    return (2);
  } else if (is_one_of (p, OPEN_SYMBOL, SUB_SYMBOL, STOP)) {
    int k;
    for (k = 0; p != NO_NODE && (is_one_of (p, OPEN_SYMBOL, SUB_SYMBOL, STOP)); FORWARD (p), k++) {
      ;
    }
    return (p != NO_NODE && (whether (p, UNION_SYMBOL, OPEN_SYMBOL, STOP) ? k : 0));
  }
  return (0);
}

/**
@brief Skip a unit in a loop clause (FROM u BY u TO u).
@param p Node in syntax tree.
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_skip_loop_unit (NODE_T * p)
{
/* Unit may start with, or consist of, a loop */
  if (is_loop_keyword (p)) {
    p = top_down_loop (p);
  }
/* Skip rest of unit */
  while (p != NO_NODE) {
    int k = is_loop_cast_formula (p);
    if (k != 0) {
/* operator-cast series .. */
      while (p != NO_NODE && k != 0) {
        while (k != 0) {
          FORWARD (p);
          k--;
        }
        k = is_loop_cast_formula (p);
      }
/* ... may be followed by a loop clause */
      if (is_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (is_loop_keyword (p) || IS (p, OD_SYMBOL)) {
/* new loop or end-of-loop */
      return (p);
    } else if (IS (p, COLON_SYMBOL)) {
      FORWARD (p);
/* skip routine header: loop clause */
      if (p != NO_NODE && is_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (is_one_of (p, SEMI_SYMBOL, COMMA_SYMBOL, STOP) || IS (p, EXIT_SYMBOL)) {
/* Statement separators */
      return (p);
    } else {
      FORWARD (p);
    }
  }
  return (NO_NODE);
}

/**
@brief Skip a loop clause.
@param p Node in syntax tree.
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_skip_loop_series (NODE_T * p)
{
  BOOL_T siga;
  do {
    p = top_down_skip_loop_unit (p);
    siga = (BOOL_T) (p != NO_NODE && (is_one_of (p, SEMI_SYMBOL, EXIT_SYMBOL, COMMA_SYMBOL, COLON_SYMBOL, STOP)));
    if (siga) {
      FORWARD (p);
    }
  } while (!(p == NO_NODE || !siga));
  return (p);
}

/**
@brief Make branch of loop parts.
@param p Node in syntax tree.
@return Token from where to proceed or NO_NODE.
**/

NODE_T *top_down_loop (NODE_T * p)
{
  NODE_T *start = p, *q = p, *save;
  if (IS (q, FOR_SYMBOL)) {
    tokens_exhausted (FORWARD (q), start);
    if (IS (q, IDENTIFIER)) {
      ATTRIBUTE (q) = DEFINING_IDENTIFIER;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, IDENTIFIER);
      longjmp (top_down_crash_exit, 1);
    }
    tokens_exhausted (FORWARD (q), start);
    if (is_one_of (q, FROM_SYMBOL, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, STOP)) {
      ;
    } else if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, STOP);
      longjmp (top_down_crash_exit, 1);
    }
  }
  if (IS (q, FROM_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_unit (NEXT (q));
    tokens_exhausted (q, start);
    if (is_one_of (q, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, STOP)) {
      ;
    } else if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, STOP);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), FROM_SYMBOL);
  }
  if (IS (q, BY_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (is_one_of (q, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, STOP)) {
      ;
    } else if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, STOP);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), BY_SYMBOL);
  }
  if (is_one_of (q, TO_SYMBOL, DOWNTO_SYMBOL, STOP)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (IS (q, WHILE_SYMBOL)) {
      ;
    } else if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, STOP);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), TO_SYMBOL);
  }
  if (IS (q, WHILE_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (IS (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, DO_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), WHILE_SYMBOL);
  }
  if (is_one_of (q, DO_SYMBOL, ALT_DO_SYMBOL, STOP)) {
    int k = ATTRIBUTE (q);
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (ISNT (q, OD_SYMBOL)) {
      top_down_diagnose (start, q, LOOP_CLAUSE, OD_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, q, k);
  }
  save = NEXT (start);
  make_sub (p, start, LOOP_CLAUSE);
  return (save);
}

/**
@brief Driver for making branches of loop parts.
@param p Node in syntax tree.
**/

static void top_down_loops (NODE_T * p)
{
  NODE_T *q = p;
  for (; q != NO_NODE; FORWARD (q)) {
    if (SUB (q) != NO_NODE) {
      top_down_loops (SUB (q));
    }
  }
  q = p;
  while (q != NO_NODE) {
    if (is_loop_keyword (q) != STOP) {
      q = top_down_loop (q);
    } else {
      FORWARD (q);
    }
  }
}

/**
@brief Driver for making branches of until parts.
@param p Node in syntax tree.
**/

static void top_down_untils (NODE_T * p)
{
  NODE_T *q = p;
  for (; q != NO_NODE; FORWARD (q)) {
    if (SUB (q) != NO_NODE) {
      top_down_untils (SUB (q));
    }
  }
  q = p;
  while (q != NO_NODE) {
    if (IS (q, UNTIL_SYMBOL)) {
      NODE_T *u = q;
      while (NEXT (u) != NO_NODE) {
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

/**
@brief Skip serial/enquiry clause (unit series).
@param p Node in syntax tree.
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_series (NODE_T * p)
{
  BOOL_T siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    p = top_down_skip_unit (p);
    if (p != NO_NODE) {
      if (is_one_of (p, SEMI_SYMBOL, EXIT_SYMBOL, COMMA_SYMBOL, STOP)) {
        siga = A68_TRUE;
        FORWARD (p);
      }
    }
  }
  return (p);
}

/**
@brief Make branch of BEGIN .. END.
@param begin_p
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_begin (NODE_T * begin_p)
{
  NODE_T *end_p = top_down_series (NEXT (begin_p));
  if (end_p == NO_NODE || ISNT (end_p, END_SYMBOL)) {
    top_down_diagnose (begin_p, end_p, ENCLOSED_CLAUSE, END_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NO_NODE);
  } else {
    make_sub (begin_p, end_p, BEGIN_SYMBOL);
    return (NEXT (begin_p));
  }
}

/**
@brief Make branch of CODE .. EDOC.
@param code_p
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_code (NODE_T * code_p)
{
  NODE_T *edoc_p = top_down_series (NEXT (code_p));
  if (edoc_p == NO_NODE || ISNT (edoc_p, EDOC_SYMBOL)) {
    diagnostic_node (A68_SYNTAX_ERROR, code_p, ERROR_KEYWORD);
    longjmp (top_down_crash_exit, 1);
    return (NO_NODE);
  } else {
    make_sub (code_p, edoc_p, CODE_SYMBOL);
    return (NEXT (code_p));
  }
}

/**
@brief Make branch of ( .. ).
@param open_p
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_open (NODE_T * open_p)
{
  NODE_T *then_bar_p = top_down_series (NEXT (open_p)), *elif_bar_p;
  if (then_bar_p != NO_NODE && IS (then_bar_p, CLOSE_SYMBOL)) {
    make_sub (open_p, then_bar_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (then_bar_p == NO_NODE || ISNT (then_bar_p, THEN_BAR_SYMBOL)) {
    top_down_diagnose (open_p, then_bar_p, ENCLOSED_CLAUSE, STOP);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (open_p, PREVIOUS (then_bar_p), OPEN_SYMBOL);
  elif_bar_p = top_down_series (NEXT (then_bar_p));
  if (elif_bar_p != NO_NODE && IS (elif_bar_p, CLOSE_SYMBOL)) {
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (open_p, elif_bar_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (elif_bar_p != NO_NODE && IS (elif_bar_p, THEN_BAR_SYMBOL)) {
    NODE_T *close_p = top_down_series (NEXT (elif_bar_p));
    if (close_p == NO_NODE || ISNT (close_p, CLOSE_SYMBOL)) {
      top_down_diagnose (open_p, elif_bar_p, ENCLOSED_CLAUSE, CLOSE_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (elif_bar_p, PREVIOUS (close_p), THEN_BAR_SYMBOL);
    make_sub (open_p, close_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (elif_bar_p != NO_NODE && IS (elif_bar_p, ELSE_BAR_SYMBOL)) {
    NODE_T *close_p = top_down_open (elif_bar_p);
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (open_p, elif_bar_p, OPEN_SYMBOL);
    return (close_p);
  } else {
    top_down_diagnose (open_p, elif_bar_p, ENCLOSED_CLAUSE, CLOSE_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NO_NODE);
  }
}

/**
@brief Make branch of [ .. ].
@param sub_p
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_sub (NODE_T * sub_p)
{
  NODE_T *bus_p = top_down_series (NEXT (sub_p));
  if (bus_p != NO_NODE && IS (bus_p, BUS_SYMBOL)) {
    make_sub (sub_p, bus_p, SUB_SYMBOL);
    return (NEXT (sub_p));
  } else {
    top_down_diagnose (sub_p, bus_p, 0, BUS_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NO_NODE);
  }
}

/**
@brief Make branch of { .. }.
@param acco_p
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_acco (NODE_T * acco_p)
{
  NODE_T *occa_p = top_down_series (NEXT (acco_p));
  if (occa_p != NO_NODE && IS (occa_p, OCCA_SYMBOL)) {
    make_sub (acco_p, occa_p, ACCO_SYMBOL);
    return (NEXT (acco_p));
  } else {
    top_down_diagnose (acco_p, occa_p, ENCLOSED_CLAUSE, OCCA_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NO_NODE);
  }
}

/**
@brief Make branch of IF .. THEN .. ELSE .. FI.
@param if_p
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_if (NODE_T * if_p)
{
  NODE_T *then_p = top_down_series (NEXT (if_p)), *elif_p;
  if (then_p == NO_NODE || ISNT (then_p, THEN_SYMBOL)) {
    top_down_diagnose (if_p, then_p, CONDITIONAL_CLAUSE, THEN_SYMBOL);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (if_p, PREVIOUS (then_p), IF_SYMBOL);
  elif_p = top_down_series (NEXT (then_p));
  if (elif_p != NO_NODE && IS (elif_p, FI_SYMBOL)) {
    make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
    make_sub (if_p, elif_p, IF_SYMBOL);
    return (NEXT (if_p));
  }
  if (elif_p != NO_NODE && IS (elif_p, ELSE_SYMBOL)) {
    NODE_T *fi_p = top_down_series (NEXT (elif_p));
    if (fi_p == NO_NODE || ISNT (fi_p, FI_SYMBOL)) {
      top_down_diagnose (if_p, fi_p, CONDITIONAL_CLAUSE, FI_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    } else {
      make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
      make_sub (elif_p, PREVIOUS (fi_p), ELSE_SYMBOL);
      make_sub (if_p, fi_p, IF_SYMBOL);
      return (NEXT (if_p));
    }
  }
  if (elif_p != NO_NODE && IS (elif_p, ELIF_SYMBOL)) {
    NODE_T *fi_p = top_down_if (elif_p);
    make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
    make_sub (if_p, elif_p, IF_SYMBOL);
    return (fi_p);
  } else {
    top_down_diagnose (if_p, elif_p, CONDITIONAL_CLAUSE, FI_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NO_NODE);
  }
}

/**
@brief Make branch of CASE .. IN .. OUT .. ESAC.
@param case_p
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_case (NODE_T * case_p)
{
  NODE_T *in_p = top_down_series (NEXT (case_p)), *ouse_p;
  if (in_p == NO_NODE || ISNT (in_p, IN_SYMBOL)) {
    top_down_diagnose (case_p, in_p, ENCLOSED_CLAUSE, IN_SYMBOL);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (case_p, PREVIOUS (in_p), CASE_SYMBOL);
  ouse_p = top_down_series (NEXT (in_p));
  if (ouse_p != NO_NODE && IS (ouse_p, ESAC_SYMBOL)) {
    make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
    make_sub (case_p, ouse_p, CASE_SYMBOL);
    return (NEXT (case_p));
  }
  if (ouse_p != NO_NODE && IS (ouse_p, OUT_SYMBOL)) {
    NODE_T *esac_p = top_down_series (NEXT (ouse_p));
    if (esac_p == NO_NODE || ISNT (esac_p, ESAC_SYMBOL)) {
      top_down_diagnose (case_p, esac_p, ENCLOSED_CLAUSE, ESAC_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    } else {
      make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
      make_sub (ouse_p, PREVIOUS (esac_p), OUT_SYMBOL);
      make_sub (case_p, esac_p, CASE_SYMBOL);
      return (NEXT (case_p));
    }
  }
  if (ouse_p != NO_NODE && IS (ouse_p, OUSE_SYMBOL)) {
    NODE_T *esac_p = top_down_case (ouse_p);
    make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
    make_sub (case_p, ouse_p, CASE_SYMBOL);
    return (esac_p);
  } else {
    top_down_diagnose (case_p, ouse_p, ENCLOSED_CLAUSE, ESAC_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NO_NODE);
  }
}

/**
@brief Skip a unit.
@param p Node in syntax tree.
@return Token from where to proceed or NO_NODE.
**/

NODE_T *top_down_skip_unit (NODE_T * p)
{
  while (p != NO_NODE && !is_unit_terminator (p)) {
    if (IS (p, BEGIN_SYMBOL)) {
      p = top_down_begin (p);
    } else if (IS (p, SUB_SYMBOL)) {
      p = top_down_sub (p);
    } else if (IS (p, OPEN_SYMBOL)) {
      p = top_down_open (p);
    } else if (IS (p, IF_SYMBOL)) {
      p = top_down_if (p);
    } else if (IS (p, CASE_SYMBOL)) {
      p = top_down_case (p);
    } else if (IS (p, CODE_SYMBOL)) {
      p = top_down_code (p);
    } else if (IS (p, ACCO_SYMBOL)) {
      p = top_down_acco (p);
    } else {
      FORWARD (p);
    }
  }
  return (p);
}

static NODE_T *top_down_skip_format (NODE_T *);

/**
@brief Make branch of ( .. ) in a format.
@param open_p
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_format_open (NODE_T * open_p)
{
  NODE_T *close_p = top_down_skip_format (NEXT (open_p));
  if (close_p != NO_NODE && IS (close_p, FORMAT_CLOSE_SYMBOL)) {
    make_sub (open_p, close_p, FORMAT_OPEN_SYMBOL);
    return (NEXT (open_p));
  } else {
    top_down_diagnose (open_p, close_p, 0, FORMAT_CLOSE_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NO_NODE);
  }
}

/**
@brief Skip a format text.
@param p Node in syntax tree.
@return Token from where to proceed or NO_NODE.
**/

static NODE_T *top_down_skip_format (NODE_T * p)
{
  while (p != NO_NODE) {
    if (IS (p, FORMAT_OPEN_SYMBOL)) {
      p = top_down_format_open (p);
    } else if (is_one_of (p, FORMAT_CLOSE_SYMBOL, FORMAT_DELIMITER_SYMBOL, STOP)) {
      return (p);
    } else {
      FORWARD (p);
    }
  }
  return (NO_NODE);
}

/**
@brief Make branch of $ .. $.
@param p Node in syntax tree.
**/

static void top_down_formats (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (SUB (q) != NO_NODE) {
      top_down_formats (SUB (q));
    }
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, FORMAT_DELIMITER_SYMBOL)) {
      NODE_T *f = NEXT (q);
      while (f != NO_NODE && ISNT (f, FORMAT_DELIMITER_SYMBOL)) {
        if (IS (f, FORMAT_OPEN_SYMBOL)) {
          f = top_down_format_open (f);
        } else {
          f = NEXT (f);
        }
      }
      if (f == NO_NODE) {
        top_down_diagnose (p, f, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL);
        longjmp (top_down_crash_exit, 1);
      } else {
        make_sub (q, f, FORMAT_DELIMITER_SYMBOL);
      }
    }
  }
}

/**
@brief Make branches of phrases for the bottom-up parser.
@param p Node in syntax tree.
**/

void top_down_parser (NODE_T * p)
{
  if (p != NO_NODE) {
    if (!setjmp (top_down_crash_exit)) {
      (void) top_down_series (p);
      top_down_loops (p);
      top_down_untils (p);
      top_down_formats (p);
    }
  }
}

/********************************************/
/* Bottom-up parser, reduces all constructs */
/********************************************/

/**
@brief Detect redefined keyword.
@param p Node in syntax tree.
@param construct Where detected.
*/

static void detect_redefined_keyword (NODE_T * p, int construct)
{
  if (p != NO_NODE && whether (p, KEYWORD, EQUALS_SYMBOL, STOP)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_REDEFINED_KEYWORD, NSYMBOL (p), construct);
  }
}

/**
@brief Whether a series is serial or collateral.
@param p Node in syntax tree.
@return Whether a series is serial or collateral.
**/

static int serial_or_collateral (NODE_T * p)
{
  NODE_T *q;
  int semis = 0, commas = 0, exits = 0;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, COMMA_SYMBOL)) {
      commas++;
    } else if (IS (q, SEMI_SYMBOL)) {
      semis++;
    } else if (IS (q, EXIT_SYMBOL)) {
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
    return ((semis + exits) >= (commas ? SERIAL_CLAUSE : COLLATERAL_CLAUSE));
  }
}

/**
@brief Whether formal bounds.
@param p Node in syntax tree.
@return Whether formal bounds.
**/

static BOOL_T is_formal_bounds (NODE_T * p)
{
  if (p == NO_NODE) {
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
        return ((BOOL_T) (is_formal_bounds (SUB (p)) && is_formal_bounds (NEXT (p))));
      }
    default:
      {
        return (A68_FALSE);
      }
    }
  }
}

/**
@brief Insert a node with attribute "a" after "p".
@param p Node in syntax tree.
@param a Attribute.
**/

static void pad_node (NODE_T * p, int a)
{
/*
This is used to fill information that Algol 68 does not require to be present.
Filling in gives one format for such construct; this helps later passes.
*/
  NODE_T *z = new_node ();
  *z = *p;
  if (GINFO (p) != NO_GINFO) {
    GINFO (z) = new_genie_info ();
  }
  PREVIOUS (z) = p;
  SUB (z) = NO_NODE;
  ATTRIBUTE (z) = a;
  MOID (z) = NO_MOID;
  if (NEXT (z) != NO_NODE) {
    PREVIOUS (NEXT (z)) = z;
  }
  NEXT (p) = z;
}

/**
@brief Diagnose extensions.
@param p Node in syntax tree.
**/

static void a68_extension (NODE_T * p)
{
  if (OPTION_PORTCHECK (&program)) {
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_EXTENSION);
  } else {
    diagnostic_node (A68_WARNING, p, WARNING_EXTENSION);
  }
}

/**
@brief Diagnose for clauses not yielding a value.
@param p Node in syntax tree.
**/

static void empty_clause (NODE_T * p)
{
  diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_CLAUSE_WITHOUT_VALUE);
}

#if ! defined HAVE_PARALLEL_CLAUSE

/**
@brief Diagnose for parallel clause.
@param p Node in syntax tree.
**/

static void par_clause (NODE_T * p)
{
  diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_NO_PARALLEL_CLAUSE);
}

#endif

/**
@brief Diagnose for missing symbol.
@param p Node in syntax tree.
**/

static void strange_tokens (NODE_T * p)
{
  NODE_T *q = ((p != NO_NODE && NEXT (p) != NO_NODE) ? NEXT (p) : p);
  diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_STRANGE_TOKENS);
}

/**
@brief Diagnose for strange separator.
@param p Node in syntax tree.
**/

static void strange_separator (NODE_T * p)
{
  NODE_T *q = ((p != NO_NODE && NEXT (p) != NO_NODE) ? NEXT (p) : p);
  diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_STRANGE_SEPARATOR);
}

/*
Here is a set of routines that gather definitions from phrases.
This way we can apply tags before defining them.
These routines do not look very elegant as they have to scan through all
kind of symbols to find a pattern that they recognise.
*/

/**
@brief Skip anything until a comma, semicolon or EXIT is found.
@param p Node in syntax tree.
@return Node from where to proceed.
**/

static NODE_T *skip_unit (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, COMMA_SYMBOL)) {
      return (p);
    } else if (IS (p, SEMI_SYMBOL)) {
      return (p);
    } else if (IS (p, EXIT_SYMBOL)) {
      return (p);
    }
  }
  return (NO_NODE);
}

/**
@brief Attribute of entry in symbol table.
@param table Current symbol table.
@param name Token name.
@return Attribute of entry in symbol table, or 0 if not found.
**/

static int find_tag_definition (TABLE_T * table, char *name)
{
  if (table != NO_TABLE) {
    int ret = 0;
    TAG_T *s;
    BOOL_T found;
    found = A68_FALSE;
    for (s = INDICANTS (table); s != NO_TAG && !found; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        ret += INDICANT;
        found = A68_TRUE;
      }
    }
    found = A68_FALSE;
    for (s = OPERATORS (table); s != NO_TAG && !found; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
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

/**
@brief Fill in whether bold tag is operator or indicant.
@param p Node in syntax tree.
**/

static void elaborate_bold_tags (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, BOLD_TAG)) {
      switch (find_tag_definition (TABLE (q), NSYMBOL (q))) {
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

/**
@brief Skip declarer, or argument pack and declarer.
@param p Node in syntax tree.
@return Node before token that is not part of pack or declarer .
**/

static NODE_T *skip_pack_declarer (NODE_T * p)
{
/* Skip () REF [] REF FLEX [] [] .. */
  while (p != NO_NODE && (is_one_of (p, SUB_SYMBOL, OPEN_SYMBOL, REF_SYMBOL, FLEX_SYMBOL, SHORT_SYMBOL, LONG_SYMBOL, STOP))) {
    FORWARD (p);
  }
/* Skip STRUCT (), UNION () or PROC [()] */
  if (p != NO_NODE && (is_one_of (p, STRUCT_SYMBOL, UNION_SYMBOL, STOP))) {
    return (NEXT (p));
  } else if (p != NO_NODE && IS (p, PROC_SYMBOL)) {
    return (skip_pack_declarer (NEXT (p)));
  } else {
    return (p);
  }
}

/**
@brief Search MODE A = .., B = .. and store indicants.
@param p Node in syntax tree.
**/

static void extract_indicants (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (IS (q, MODE_SYMBOL)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        detect_redefined_keyword (q, MODE_DECLARATION);
        if (whether (q, BOLD_TAG, EQUALS_SYMBOL, STOP)) {
/* Store in the symbol table, but also in the moid list.
   Position of definition (q) connects to this lexical level! 
*/
          ASSERT (add_tag (TABLE (p), INDICANT, q, NO_MOID, STOP) != NO_TAG);
          ASSERT (add_mode (&TOP_MOID (&program), INDICANT, 0, q, NO_MOID, NO_PACK) != NO_MOID);
          ATTRIBUTE (q) = DEFINING_INDICANT;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          q = skip_pack_declarer (NEXT (q));
          FORWARD (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

#define GET_PRIORITY(q, k)\
  RESET_ERRNO;\
  (k) = atoi (NSYMBOL (q));\
  if (errno != 0) {\
    diagnostic_node (A68_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY);\
    (k) = MAX_PRIORITY;\
  } else if ((k) < 1 || (k) > MAX_PRIORITY) {\
    diagnostic_node (A68_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY);\
    (k) = MAX_PRIORITY;\
  }

/**
@brief Search PRIO X = .., Y = .. and store priorities.
@param p Node in syntax tree.
**/

static void extract_priorities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (IS (q, PRIO_SYMBOL)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        detect_redefined_keyword (q, PRIORITY_DECLARATION);
/* An operator tag like ++ or && gives strange errors so we catch it here */
        if (whether (q, OPERATOR, OPERATOR, STOP)) {
          int k;
          NODE_T *y = q;
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG);
          ATTRIBUTE (q) = DEFINING_OPERATOR;
/* Remove one superfluous operator, and hope it was only one.   	 */
          NEXT (q) = NEXT_NEXT (q);
          PREVIOUS (NEXT (q)) = q;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (TABLE (p), PRIO_SYMBOL, y, NO_MOID, k) != NO_TAG);
          FORWARD (q);
        } else if (whether (q, OPERATOR, EQUALS_SYMBOL, INT_DENOTATION, STOP) || 
                   whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, INT_DENOTATION, STOP)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (TABLE (p), PRIO_SYMBOL, y, NO_MOID, k) != NO_TAG);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, IDENTIFIER, STOP)) {
          siga = A68_FALSE;
        } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, INT_DENOTATION, STOP)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (TABLE (p), PRIO_SYMBOL, y, NO_MOID, k) != NO_TAG);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, INT_DENOTATION, STOP) || 
                   whether (q, OPERATOR, INT_DENOTATION, STOP) || 
                   whether (q, EQUALS_SYMBOL, INT_DENOTATION, STOP)) {
/* The scanner cannot separate operator and "=" sign so we do this here */
          int len = (int) strlen (NSYMBOL (q));
          if (len > 1 && NSYMBOL (q)[len - 1] == '=') {
            int k;
            NODE_T *y = q;
            char *sym = (char *) get_temp_heap_space ((size_t) (len + 1));
            bufcpy (sym, NSYMBOL (q), len + 1);
            sym[len - 1] = NULL_CHAR;
            NSYMBOL (q) = TEXT (add_token (&top_token, sym));
            if (len > 2 && NSYMBOL (q)[len - 2] == ':' && NSYMBOL (q)[len - 3] != '=') {
              diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_OPERATOR_INVALID_END);
            }
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            insert_alt_equals (q);
            q = NEXT_NEXT (q);
            GET_PRIORITY (q, k);
            ATTRIBUTE (q) = PRIORITY;
            ASSERT (add_tag (TABLE (p), PRIO_SYMBOL, y, NO_MOID, k) != NO_TAG);
            FORWARD (q);
          } else {
            siga = A68_FALSE;
          }
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/**
@brief Search OP [( .. ) ..] X = .., Y = .. and store operators.
@param p Node in syntax tree.
**/

static void extract_operators (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (ISNT (q, OP_SYMBOL)) {
      FORWARD (q);
    } else {
      BOOL_T siga = A68_TRUE;
/* Skip operator plan */
      if (NEXT (q) != NO_NODE && IS (NEXT (q), OPEN_SYMBOL)) {
        q = skip_pack_declarer (NEXT (q));
      }
/* Sample operators */
      if (q != NO_NODE) {
        do {
          FORWARD (q);
          detect_redefined_keyword (q, OPERATOR_DECLARATION);
/* Unacceptable operator tags like ++ or && could give strange errors */
          if (whether (q, OPERATOR, OPERATOR, STOP)) {
            diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG);
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (TABLE (p), OP_SYMBOL, q, NO_MOID, STOP) != NO_TAG);
            NEXT (q) = NEXT_NEXT (q); /* Remove one superfluous operator, and hope it was only one */
            PREVIOUS (NEXT (q)) = q;
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, OPERATOR, EQUALS_SYMBOL, STOP) || 
                     whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, STOP)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (TABLE (p), OP_SYMBOL, q, NO_MOID, STOP) != NO_TAG);
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, BOLD_TAG, IDENTIFIER, STOP)) {
            siga = A68_FALSE;
          } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, STOP)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (TABLE (p), OP_SYMBOL, q, NO_MOID, STOP) != NO_TAG);
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (q != NO_NODE && (is_one_of (q, OPERATOR, BOLD_TAG, EQUALS_SYMBOL, STOP))) {
/* The scanner cannot separate operator and "=" sign so we do this here */
            int len = (int) strlen (NSYMBOL (q));
            if (len > 1 && NSYMBOL (q)[len - 1] == '=') {
              char *sym = (char *) get_temp_heap_space ((size_t) (len + 1));
              bufcpy (sym, NSYMBOL (q), len + 1);
              sym[len - 1] = NULL_CHAR;
              NSYMBOL (q) = TEXT (add_token (&top_token, sym));
              if (len > 2 && NSYMBOL (q)[len - 2] == ':' && NSYMBOL (q)[len - 3] != '=') {
                diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_OPERATOR_INVALID_END);
              }
              ATTRIBUTE (q) = DEFINING_OPERATOR;
              insert_alt_equals (q);
              ASSERT (add_tag (TABLE (p), OP_SYMBOL, q, NO_MOID, STOP) != NO_TAG);
              FORWARD (q);
              q = skip_unit (q);
            } else {
              siga = A68_FALSE;
            }
          } else {
            siga = A68_FALSE;
          }
        } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
      }
    }
  }
}

/**
@brief Search and store labels.
@param p Node in syntax tree.
@param expect Information the parser may have on what is expected.
**/

static void extract_labels (NODE_T * p, int expect)
{
  NODE_T *q;
/* Only handle candidate phrases as not to search indexers! */
  if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      if (whether (q, IDENTIFIER, COLON_SYMBOL, STOP)) {
        TAG_T *z = add_tag (TABLE (p), LABEL, q, NO_MOID, LOCAL_LABEL);
        ATTRIBUTE (q) = DEFINING_IDENTIFIER;
        UNIT (z) = NO_NODE;
      }
    }
  }
}

/**
@brief Search MOID x = .., y = .. and store identifiers.
@param p Node in syntax tree.
**/

static void extract_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (whether (q, DECLARER, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
      BOOL_T siga = A68_TRUE;
      do {
        if (whether ((FORWARD (q)), IDENTIFIER, EQUALS_SYMBOL, STOP)) {
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, STOP)) {
/* Handle common error in ALGOL 68 programs */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/**
@brief Search MOID x [:= ..], y [:= ..] and store identifiers.
@param p Node in syntax tree.
**/

static void extract_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (whether (q, DECLARER, IDENTIFIER, STOP)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, STOP)) {
          if (whether (q, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
/* Handle common error in ALGOL 68 programs */
            diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
            ATTRIBUTE (NEXT (q)) = ASSIGN_SYMBOL;
          }
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/**
@brief Search PROC x = .., y = .. and stores identifiers.
@param p Node in syntax tree.
**/

static void extract_proc_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
          TAG_T *t = add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER);
          IN_PROC (t) = A68_TRUE;
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, STOP)) {
/* Handle common error in ALGOL 68 programs */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/**
@brief Search PROC x [:= ..], y [:= ..]; store identifiers.
@param p Node in syntax tree.
**/

static void extract_proc_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, STOP)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, STOP)) {
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (FORWARD (q));
        } else if (whether (q, IDENTIFIER, EQUALS_SYMBOL, STOP)) {
/* Handle common error in ALGOL 68 programs */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (TABLE (p), IDENTIFIER, q, NO_MOID, NORMAL_IDENTIFIER) != NO_TAG);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ASSIGN_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NO_NODE && IS (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}


/**
@brief Schedule gathering of definitions in a phrase.
@param p Node in syntax tree.
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
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = OPERATOR;
    } else if (IS (q, ALT_EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = EQUALS_SYMBOL;
    }
  }
/* Get qualifiers */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (whether (q, LOC_SYMBOL, DECLARER, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, DECLARER, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, NEW_SYMBOL, DECLARER, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, LOC_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, NEW_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, STOP)) {
      make_sub (q, q, QUALIFIER);
    }
  }
/* Give priorities to operators */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, OPERATOR)) {
      if (find_tag_global (TABLE (q), OP_SYMBOL, NSYMBOL (q))) {
        TAG_T *s = find_tag_global (TABLE (q), PRIO_SYMBOL, NSYMBOL (q));
        if (s != NO_TAG) {
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

/**
@brief If match then reduce a sentence, the core BU parser routine.
@param p Token where to start matching.
@param a If not NO_NOTE, procedure to execute upon match.
@param z If not NO_TICK, to be set to TRUE upon match.
**/

static void reduce (NODE_T * p, void (*a) (NODE_T *), BOOL_T * z, ...)
{
  va_list list;
  int result, arg;
  NODE_T *head = p, *tail = NO_NODE;
  va_start (list, z);
  result = va_arg (list, int);
  while ((arg = va_arg (list, int)) != STOP)
  {
    BOOL_T keep_matching;
    if (p == NO_NODE) {
      keep_matching = A68_FALSE;
    } else if (arg == WILDCARD) {
/* WILDCARD matches any Algol68G non terminal, but no keyword */
      keep_matching = (BOOL_T) (non_terminal_string (edit_line, ATTRIBUTE (p)) != NO_TEXT);
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
  if (head != NO_NODE && OPTION_REDUCTIONS (&program) && LINE_NUMBER (head) > 0) {
    NODE_T *q;
    int count = 0;
    reductions++;
    WIS (head);
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "\nReduction %d: %s<-", reductions, non_terminal_string (edit_line, result)) >= 0);
    WRITE (STDOUT_FILENO, output_line);
    for (q = head; q != NO_NODE && tail != NO_NODE && q != NEXT (tail); FORWARD (q), count++) {
      int gatt = ATTRIBUTE (q);
      char *str = non_terminal_string (input_line, gatt);
      if (count > 0) {
        WRITE (STDOUT_FILENO, ", ");
      }
      if (str != NO_TEXT) {
        WRITE (STDOUT_FILENO, str);
        if (gatt == IDENTIFIER || gatt == OPERATOR || gatt == DENOTATION || gatt == INDICANT) {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, " \"%s\"", NSYMBOL (q)) >= 0);
          WRITE (STDOUT_FILENO, output_line);
        }
      } else {
        WRITE (STDOUT_FILENO, NSYMBOL (q));
      }
    }
  }
/* Make reduction */
  if (a != NO_NOTE) {
    a (head);
  }
  make_sub (head, tail, result);
  va_end (list);
  if (z != NO_TICK) {
    *z = A68_TRUE;
  }
}

/**
@brief Graciously ignore extra semicolons.
@param p Node in syntax tree.
**/

static void ignore_superfluous_semicolons (NODE_T * p)
{
/*
This routine relaxes the parser a bit with respect to superfluous semicolons,
for instance "FI; OD". These provoke only a warning.
*/
  for (; p != NO_NODE; FORWARD (p)) {
    ignore_superfluous_semicolons (SUB (p));
    if (NEXT (p) != NO_NODE && IS (NEXT (p), SEMI_SYMBOL) && NEXT_NEXT (p) == NO_NODE) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, NEXT (p), WARNING_SKIPPED_SUPERFLUOUS, ATTRIBUTE (NEXT (p)));
      NEXT (p) = NO_NODE;
    } else if (IS (p, SEMI_SYMBOL) && is_semicolon_less (NEXT (p))) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_SKIPPED_SUPERFLUOUS, ATTRIBUTE (p));
      if (PREVIOUS (p) != NO_NODE) {
        NEXT (PREVIOUS (p)) = NEXT (p);
      }
      PREVIOUS (NEXT (p)) = PREVIOUS (p);
    }
  }
}

/**
@brief Driver for the bottom-up parser.
@param p Node in syntax tree.
**/

void bottom_up_parser (NODE_T * p)
{
  if (p != NO_NODE) {
    if (!setjmp (bottom_up_crash_exit)) {
      NODE_T *q;
      int error_count_0 = ERROR_COUNT (&program);
      ignore_superfluous_semicolons (p);
/* A program is "label sequence; particular program" */
      extract_labels (p, SERIAL_CLAUSE/* a fake here, but ok */ );
/* Parse the program itself */
      for (q = p; q != NO_NODE; FORWARD (q)) {
        BOOL_T siga = A68_TRUE;
        if (SUB (q) != NO_NODE) {
          reduce_branch (q, SOME_CLAUSE);
        }
        while (siga) {
          siga = A68_FALSE;
          reduce (q, NO_NOTE, &siga, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, STOP);
          reduce (q, NO_NOTE, &siga, LABEL, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, STOP);
        }
      }
/* Determine the encompassing enclosed clause */
      for (q = p; q != NO_NODE; FORWARD (q)) {
    #if defined HAVE_PARALLEL_CLAUSE
        reduce (q, NO_NOTE, NO_TICK, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, STOP);
    #else
        reduce (q, par_clause, NO_TICK, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, STOP);
    #endif
        reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, PARALLEL_CLAUSE, STOP);
        reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CLOSED_CLAUSE, STOP);
        reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, STOP);
        reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, STOP);
        reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CASE_CLAUSE, STOP);
        reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CONFORMITY_CLAUSE, STOP);
        reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, LOOP_CLAUSE, STOP);
      }
/* Try reducing the particular program */
      q = p;
      reduce (q, NO_NOTE, NO_TICK, PARTICULAR_PROGRAM, LABEL, ENCLOSED_CLAUSE, STOP);
      reduce (q, NO_NOTE, NO_TICK, PARTICULAR_PROGRAM, ENCLOSED_CLAUSE, STOP);
      if (SUB (p) == NO_NODE || NEXT (p) != NO_NODE) {
        recover_from_error (p, PARTICULAR_PROGRAM, (BOOL_T) ((ERROR_COUNT (&program) - error_count_0) > MAX_ERRORS));
      }
    }
  }
}

/**
@brief Reduce code clause.
@param p Node in syntax tree.
**/

static void reduce_code_clause (NODE_T * p)
{
  BOOL_T siga = A68_TRUE;
  while (siga) {
    NODE_T *u;
    siga = A68_FALSE;
    for (u = p; u != NO_NODE; FORWARD (u)) {
      reduce (u, NO_NOTE, &siga, CODE_LIST, CODE_SYMBOL, ROW_CHAR_DENOTATION, STOP);
      reduce (u, NO_NOTE, &siga, CODE_LIST, CODE_LIST, ROW_CHAR_DENOTATION, STOP);
      reduce (u, NO_NOTE, &siga, CODE_LIST, CODE_LIST, COMMA_SYMBOL, ROW_CHAR_DENOTATION, STOP);
      reduce (u, NO_NOTE, &siga, CODE_CLAUSE, CODE_LIST, EDOC_SYMBOL, STOP);
    }
  }
}

/**
@brief Reduce the sub-phrase that starts one level down.
@param q Node in syntax tree.
@param expect Information the parser may have on what is expected.
**/

static void reduce_branch (NODE_T * q, int expect)
{
/*
If this is unsuccessful then it will at least copy the resulting attribute
as the parser can repair some faults. This gives less spurious diagnostics.
*/
  if (q != NO_NODE && SUB (q) != NO_NODE) {
    NODE_T *p = SUB (q), *u = NO_NODE;
    int error_count_0 = ERROR_COUNT (&program), error_count_02;
    BOOL_T declarer_pack = A68_FALSE, no_error;
    switch (expect) {
      case STRUCTURE_PACK:
      case PARAMETER_PACK:
      case FORMAL_DECLARERS:
      case UNION_PACK:
      case SPECIFIER: {
        declarer_pack = A68_TRUE;
      }
      default: {
        declarer_pack = A68_FALSE;
      }
    }
/* Sample all info needed to decide whether a bold tag is operator or indicant.
   Find the meaning of bold tags and quit in case of extra errors. */
    extract_indicants (p);
    if (!declarer_pack) {
      extract_priorities (p);
      extract_operators (p);
    }
    error_count_02 = ERROR_COUNT (&program);
    elaborate_bold_tags (p);
    if ((ERROR_COUNT (&program) - error_count_02) > 0) {
      longjmp (bottom_up_crash_exit, 1);
    }
/* Now we can reduce declarers, knowing which bold tags are indicants */
    reduce_declarers (p, expect);
/* Parse the phrase, as appropriate */
    if (expect == CODE_CLAUSE) {
      reduce_code_clause (p);
    } else if (declarer_pack == A68_FALSE) {
      error_count_02 = ERROR_COUNT (&program);
      extract_declarations (p);
      if ((ERROR_COUNT (&program) - error_count_02) > 0) {
        longjmp (bottom_up_crash_exit, 1);
      }
      extract_labels (p, expect);
      for (u = p; u != NO_NODE; FORWARD (u)) {
        if (SUB (u) != NO_NODE) {
          if (IS (u, FORMAT_DELIMITER_SYMBOL)) {
            reduce_branch (u, FORMAT_TEXT);
          } else if (IS (u, FORMAT_OPEN_SYMBOL)) {
            reduce_branch (u, FORMAT_TEXT);
          } else if (IS (u, OPEN_SYMBOL)) {
            if (NEXT (u) != NO_NODE && IS (NEXT (u), THEN_BAR_SYMBOL)) {
              reduce_branch (u, ENQUIRY_CLAUSE);
            } else if (PREVIOUS (u) != NO_NODE && IS (PREVIOUS (u), PAR_SYMBOL)) {
              reduce_branch (u, COLLATERAL_CLAUSE);
            }
          } else if (is_one_of (u, IF_SYMBOL, ELIF_SYMBOL, CASE_SYMBOL, OUSE_SYMBOL, WHILE_SYMBOL, UNTIL_SYMBOL, ELSE_BAR_SYMBOL, ACCO_SYMBOL, STOP)) {
            reduce_branch (u, ENQUIRY_CLAUSE);
          } else if (IS (u, BEGIN_SYMBOL)) {
            reduce_branch (u, SOME_CLAUSE);
          } else if (is_one_of (u, THEN_SYMBOL, ELSE_SYMBOL, OUT_SYMBOL, DO_SYMBOL, ALT_DO_SYMBOL, STOP)) {
            reduce_branch (u, SERIAL_CLAUSE);
          } else if (IS (u, IN_SYMBOL)) {
            reduce_branch (u, COLLATERAL_CLAUSE);
          } else if (IS (u, THEN_BAR_SYMBOL)) {
            reduce_branch (u, SOME_CLAUSE);
          } else if (IS (u, LOOP_CLAUSE)) {
            reduce_branch (u, ENCLOSED_CLAUSE);
          } else if (is_one_of (u, FOR_SYMBOL, FROM_SYMBOL, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, STOP)) {
            reduce_branch (u, UNIT);
          }
        }
      }
      reduce_primary_parts (p, expect);
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
      for (u = p; u != NO_NODE; FORWARD (u)) {
        if (SUB (u) != NO_NODE) {
          if (IS (u, CODE_SYMBOL)) {
            reduce_branch (u, CODE_CLAUSE);
          }
        }
      }
      reduce_right_to_left_constructs (p);
/* Reduce units and declarations */
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
            for (u = p; u != NO_NODE; FORWARD (u)) {
              reduce (u, NO_NOTE, NO_TICK, LABELED_UNIT, LABEL, UNIT, STOP);
              reduce (u, NO_NOTE, NO_TICK, SPECIFIED_UNIT, SPECIFIER, COLON_SYMBOL, UNIT, STOP);
            }
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
      reduce_enclosed_clauses (p, expect);
    }
/* Do something if parsing failed */
    if (SUB (p) == NO_NODE || NEXT (p) != NO_NODE) {
      recover_from_error (p, expect, (BOOL_T) ((ERROR_COUNT (&program) - error_count_0) > MAX_ERRORS));
      no_error = A68_FALSE;
    } else {
      no_error = A68_TRUE;
    }
    ATTRIBUTE (q) = ATTRIBUTE (p);
    if (no_error) {
      SUB (q) = SUB (p);
    }
  }
}

/**
@brief Driver for reducing declarers.
@param p Node in syntax tree.
@param expect Information the parser may have on what is expected.
**/

static void reduce_declarers (NODE_T * p, int expect)
{
  NODE_T *q; BOOL_T siga;
/* Reduce lengtheties */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    siga = A68_TRUE;
    reduce (q, NO_NOTE, NO_TICK, LONGETY, LONG_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, SHORTETY, SHORT_SYMBOL, STOP);
    while (siga) {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, LONGETY, LONGETY, LONG_SYMBOL, STOP);
      reduce (q, NO_NOTE, &siga, SHORTETY, SHORTETY, SHORT_SYMBOL, STOP);
    }
  }
/* Reduce indicants */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, INDICANT, INT_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, REAL_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, BITS_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, BYTES_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, COMPLEX_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, COMPL_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, BOOL_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, CHAR_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, FORMAT_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, STRING_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, FILE_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, CHANNEL_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, SEMA_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, PIPE_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, INDICANT, SOUND_SYMBOL, STOP);
  }
/* Reduce standard stuff */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (whether (q, LONGETY, INDICANT, STOP)) {
      int a;
      if (SUB_NEXT (q) == NO_NODE) {
        diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
        reduce (q, NO_NOTE, NO_TICK, DECLARER, LONGETY, INDICANT, STOP);
      } else {
        a = ATTRIBUTE (SUB_NEXT (q));
        if (a == INT_SYMBOL || a == REAL_SYMBOL || a == BITS_SYMBOL || a == BYTES_SYMBOL || a == COMPLEX_SYMBOL || a == COMPL_SYMBOL) {
          reduce (q, NO_NOTE, NO_TICK, DECLARER, LONGETY, INDICANT, STOP);
        } else {
          diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
          reduce (q, NO_NOTE, NO_TICK, DECLARER, LONGETY, INDICANT, STOP);
        }
      }
    } else if (whether (q, SHORTETY, INDICANT, STOP)) {
      int a;
      if (SUB_NEXT (q) == NO_NODE) {
        diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
        reduce (q, NO_NOTE, NO_TICK, DECLARER, SHORTETY, INDICANT, STOP);
      } else {
        a = ATTRIBUTE (SUB_NEXT (q));
        if (a == INT_SYMBOL || a == REAL_SYMBOL || a == BITS_SYMBOL || a == BYTES_SYMBOL || a == COMPLEX_SYMBOL || a == COMPL_SYMBOL) {
          reduce (q, NO_NOTE, NO_TICK, DECLARER, SHORTETY, INDICANT, STOP);
        } else {
          diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
          reduce (q, NO_NOTE, NO_TICK, DECLARER, LONGETY, INDICANT, STOP);
        }
      }
    }
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, DECLARER, INDICANT, STOP);
  }
/* Reduce declarer lists */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (NEXT (q) != NO_NODE && SUB_NEXT (q) != NO_NODE) {
      if (IS (q, STRUCT_SYMBOL)) {
        reduce_branch (NEXT (q), STRUCTURE_PACK);
        reduce (q, NO_NOTE, NO_TICK, DECLARER, STRUCT_SYMBOL, STRUCTURE_PACK, STOP);
      } else if (IS (q, UNION_SYMBOL)) {
        reduce_branch (NEXT (q), UNION_PACK);
        reduce (q, NO_NOTE, NO_TICK, DECLARER, UNION_SYMBOL, UNION_PACK, STOP);
      } else if (IS (q, PROC_SYMBOL)) {
        if (whether (q, PROC_SYMBOL, OPEN_SYMBOL, STOP)) {
          if (!is_formal_bounds (SUB_NEXT (q))) {
            reduce_branch (NEXT (q), FORMAL_DECLARERS);
          }
        }
      } else if (IS (q, OP_SYMBOL)) {
        if (whether (q, OP_SYMBOL, OPEN_SYMBOL, STOP)) {
          if (!is_formal_bounds (SUB_NEXT (q))) {
            reduce_branch (NEXT (q), FORMAL_DECLARERS);
          }
        }
      }
    }
  }
/* Reduce row, proc or op declarers */
  siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    for (q = p; q != NO_NODE; FORWARD (q)) {
/* FLEX DECL */
      if (whether (q, FLEX_SYMBOL, DECLARER, STOP)) {
        reduce (q, NO_NOTE, &siga, DECLARER, FLEX_SYMBOL, DECLARER, STOP);
      }
/* FLEX [] DECL */
      if (whether (q, FLEX_SYMBOL, SUB_SYMBOL, DECLARER, STOP) && SUB_NEXT (q) != NO_NODE) {
        reduce_branch (NEXT (q), BOUNDS);
        reduce (q, NO_NOTE, &siga, DECLARER, FLEX_SYMBOL, BOUNDS, DECLARER, STOP);
        reduce (q, NO_NOTE, &siga, DECLARER, FLEX_SYMBOL, FORMAL_BOUNDS, DECLARER, STOP);
      }
/* FLEX () DECL */
      if (whether (q, FLEX_SYMBOL, OPEN_SYMBOL, DECLARER, STOP) && SUB_NEXT (q) != NO_NODE) {
        if (!whether (q, FLEX_SYMBOL, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, STOP)) {
          reduce_branch (NEXT (q), BOUNDS);
          reduce (q, NO_NOTE, &siga, DECLARER, FLEX_SYMBOL, BOUNDS, DECLARER, STOP);
          reduce (q, NO_NOTE, &siga, DECLARER, FLEX_SYMBOL, FORMAL_BOUNDS, DECLARER, STOP);
        }
      }
/* [] DECL */
      if (whether (q, SUB_SYMBOL, DECLARER, STOP) && SUB (q) != NO_NODE) {
        reduce_branch (q, BOUNDS);
        reduce (q, NO_NOTE, &siga, DECLARER, BOUNDS, DECLARER, STOP);
        reduce (q, NO_NOTE, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, STOP);
      }
/* () DECL */
      if (whether (q, OPEN_SYMBOL, DECLARER, STOP) && SUB (q) != NO_NODE) {
        if (whether (q, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, STOP)) {
/* Catch e.g. (INT i) () INT: */
          if (is_formal_bounds (SUB (q))) {
            reduce_branch (q, BOUNDS);
            reduce (q, NO_NOTE, &siga, DECLARER, BOUNDS, DECLARER, STOP);
            reduce (q, NO_NOTE, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, STOP);
          }
        } else {
          reduce_branch (q, BOUNDS);
          reduce (q, NO_NOTE, &siga, DECLARER, BOUNDS, DECLARER, STOP);
          reduce (q, NO_NOTE, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, STOP);
        }
      }
    }
/* PROC DECL, PROC () DECL, OP () DECL */
    for (q = p; q != NO_NODE; FORWARD (q)) {
      int a = ATTRIBUTE (q);
      if (a == REF_SYMBOL) {
        reduce (q, NO_NOTE, &siga, DECLARER, REF_SYMBOL, DECLARER, STOP);
      } else if (a == PROC_SYMBOL) {
        reduce (q, NO_NOTE, &siga, DECLARER, PROC_SYMBOL, DECLARER, STOP);
        reduce (q, NO_NOTE, &siga, DECLARER, PROC_SYMBOL, FORMAL_DECLARERS, DECLARER, STOP);
        reduce (q, NO_NOTE, &siga, DECLARER, PROC_SYMBOL, VOID_SYMBOL, STOP);
        reduce (q, NO_NOTE, &siga, DECLARER, PROC_SYMBOL, FORMAL_DECLARERS, VOID_SYMBOL, STOP);
      } else if (a == OP_SYMBOL) {
        reduce (q, NO_NOTE, &siga, OPERATOR_PLAN, OP_SYMBOL, FORMAL_DECLARERS, DECLARER, STOP);
        reduce (q, NO_NOTE, &siga, OPERATOR_PLAN, OP_SYMBOL, FORMAL_DECLARERS, VOID_SYMBOL, STOP);
      }
    }
  }
/* Reduce packs etcetera */
  if (expect == STRUCTURE_PACK) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, STRUCTURED_FIELD, DECLARER, IDENTIFIER, STOP);
        reduce (q, NO_NOTE, &siga, STRUCTURED_FIELD, STRUCTURED_FIELD, COMMA_SYMBOL, IDENTIFIER, STOP);
      }
    }
    for (q = p; q != NO_NODE; FORWARD (q)) {
      siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, STOP);
        reduce (q, NO_NOTE, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, COMMA_SYMBOL, STRUCTURED_FIELD, STOP);
        reduce (q, strange_separator, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, STOP);
        reduce (q, strange_separator, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, SEMI_SYMBOL, STRUCTURED_FIELD, STOP);
      }
    }
    q = p;
    reduce (q, NO_NOTE, NO_TICK, STRUCTURE_PACK, OPEN_SYMBOL, STRUCTURED_FIELD_LIST, CLOSE_SYMBOL, STOP);
  } else if (expect == PARAMETER_PACK) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, PARAMETER, DECLARER, IDENTIFIER, STOP);
        reduce (q, NO_NOTE, &siga, PARAMETER, PARAMETER, COMMA_SYMBOL, IDENTIFIER, STOP);
      }
    }
    for (q = p; q != NO_NODE; FORWARD (q)) {
      siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, PARAMETER_LIST, PARAMETER, STOP);
        reduce (q, NO_NOTE, &siga, PARAMETER_LIST, PARAMETER_LIST, COMMA_SYMBOL, PARAMETER, STOP);
      }
    }
    q = p;
    reduce (q, NO_NOTE, NO_TICK, PARAMETER_PACK, OPEN_SYMBOL, PARAMETER_LIST, CLOSE_SYMBOL, STOP);
  } else if (expect == FORMAL_DECLARERS) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, FORMAL_DECLARERS_LIST, DECLARER, STOP);
        reduce (q, NO_NOTE, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, COMMA_SYMBOL, DECLARER, STOP);
        reduce (q, strange_separator, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, SEMI_SYMBOL, DECLARER, STOP);
        reduce (q, strange_separator, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, DECLARER, STOP);
      }
    }
    q = p;
    reduce (q, NO_NOTE, NO_TICK, FORMAL_DECLARERS, OPEN_SYMBOL, FORMAL_DECLARERS_LIST, CLOSE_SYMBOL, STOP);
  } else if (expect == UNION_PACK) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, UNION_DECLARER_LIST, DECLARER, STOP);
        reduce (q, NO_NOTE, &siga, UNION_DECLARER_LIST, VOID_SYMBOL, STOP);
        reduce (q, NO_NOTE, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, COMMA_SYMBOL, DECLARER, STOP);
        reduce (q, NO_NOTE, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, COMMA_SYMBOL, VOID_SYMBOL, STOP);
        reduce (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, SEMI_SYMBOL, DECLARER, STOP);
        reduce (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, SEMI_SYMBOL, VOID_SYMBOL, STOP);
        reduce (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, DECLARER, STOP);
        reduce (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, VOID_SYMBOL, STOP);
      }
    }
    q = p;
    reduce (q, NO_NOTE, NO_TICK, UNION_PACK, OPEN_SYMBOL, UNION_DECLARER_LIST, CLOSE_SYMBOL, STOP);
  } else if (expect == SPECIFIER) {
    reduce (p, NO_NOTE, NO_TICK, SPECIFIER, OPEN_SYMBOL, DECLARER, IDENTIFIER, CLOSE_SYMBOL, STOP);
    reduce (p, NO_NOTE, NO_TICK, SPECIFIER, OPEN_SYMBOL, DECLARER, CLOSE_SYMBOL, STOP);
    reduce (p, NO_NOTE, NO_TICK, SPECIFIER, OPEN_SYMBOL, VOID_SYMBOL, CLOSE_SYMBOL, STOP);
  } else {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      if (whether (q, OPEN_SYMBOL, COLON_SYMBOL, STOP) && !(expect == GENERIC_ARGUMENT || expect == BOUNDS)) {
        if (is_one_of (p, IN_SYMBOL, THEN_BAR_SYMBOL, STOP)) {
          reduce_branch (q, SPECIFIER);
        }
      }
      if (whether (q, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, STOP)) {
        reduce_branch (q, PARAMETER_PACK);
      }
      if (whether (q, OPEN_SYMBOL, VOID_SYMBOL, COLON_SYMBOL, STOP)) {
        reduce_branch (q, PARAMETER_PACK);
      }
    }
  }
}

/**
@brief Handle cases that need reducing from right-to-left.
@param p Node in syntax tree.
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
  if (p != NO_NODE) {
    reduce_right_to_left_constructs (NEXT (p));
/* Assignations */
    if (IS (p, TERTIARY)) {
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, TERTIARY, STOP);
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, IDENTITY_RELATION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, AND_FUNCTION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, OR_FUNCTION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, ROUTINE_TEXT, STOP);
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, JUMP, STOP);
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, SKIP, STOP);
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, ASSIGNATION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, CODE_CLAUSE, STOP);
    }
/* Routine texts with parameter pack */
    else if (IS (p, PARAMETER_PACK)) {
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, ASSIGNATION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, IDENTITY_RELATION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, AND_FUNCTION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, OR_FUNCTION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, JUMP, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, SKIP, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, TERTIARY, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, ROUTINE_TEXT, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, CODE_CLAUSE, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, ASSIGNATION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, IDENTITY_RELATION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, AND_FUNCTION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, OR_FUNCTION, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, JUMP, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, SKIP, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, TERTIARY, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, ROUTINE_TEXT, STOP);
      reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, CODE_CLAUSE, STOP);
    }
/* Routine texts without parameter pack */
    else if (IS (p, DECLARER)) {
      if (!(PREVIOUS (p) != NO_NODE && IS (PREVIOUS (p), PARAMETER_PACK))) {
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, ASSIGNATION, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, IDENTITY_RELATION, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, AND_FUNCTION, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, OR_FUNCTION, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, JUMP, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, SKIP, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, TERTIARY, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, ROUTINE_TEXT, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, CODE_CLAUSE, STOP);
      }
    } else if (IS (p, VOID_SYMBOL)) {
      if (!(PREVIOUS (p) != NO_NODE && IS (PREVIOUS (p), PARAMETER_PACK))) {
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, ASSIGNATION, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, IDENTITY_RELATION, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, AND_FUNCTION, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, OR_FUNCTION, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, JUMP, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, SKIP, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, TERTIARY, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, ROUTINE_TEXT, STOP);
        reduce (p, NO_NOTE, NO_TICK, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, CODE_CLAUSE, STOP);
      }
    }
  }
}

/**
@brief Reduce primary elements.
@param p Node in syntax tree.
@param expect Information the parser may have on what is expected.
**/

static void reduce_primary_parts (NODE_T * p, int expect)
{
  NODE_T *q = p;
  for (; q != NO_NODE; FORWARD (q)) {
    if (whether (q, IDENTIFIER, OF_SYMBOL, STOP)) {
      ATTRIBUTE (q) = FIELD_IDENTIFIER;
    }
    reduce (q, NO_NOTE, NO_TICK, ENVIRON_NAME, ENVIRON_SYMBOL, ROW_CHAR_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, NIHIL, NIL_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, SKIP, SKIP_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, SELECTOR, FIELD_IDENTIFIER, OF_SYMBOL, STOP);
/* JUMPs without GOTO are resolved later */
    reduce (q, NO_NOTE, NO_TICK, JUMP, GOTO_SYMBOL, IDENTIFIER, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, LONGETY, INT_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, LONGETY, REAL_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, LONGETY, BITS_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, SHORTETY, INT_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, SHORTETY, REAL_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, SHORTETY, BITS_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, INT_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, REAL_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, BITS_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, ROW_CHAR_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, TRUE_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, FALSE_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, DENOTATION, EMPTY_SYMBOL, STOP);
    if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
      BOOL_T siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, STOP);
        reduce (q, NO_NOTE, &siga, LABEL, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, STOP);
      }
    }
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
#if defined HAVE_PARALLEL_CLAUSE
    reduce (q, NO_NOTE, NO_TICK, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, STOP);
#else
    reduce (q, par_clause, NO_TICK, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, STOP);
#endif
    reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, PARALLEL_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CLOSED_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CASE_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CONFORMITY_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, LOOP_CLAUSE, STOP);
  }
}

/**
@brief Reduce primaries completely.
@param p Node in syntax tree.
@param expect Information the parser may have on what is expected.
**/

static void reduce_primaries (NODE_T * p, int expect)
{
  NODE_T *q = p;
  while (q != NO_NODE) {
    BOOL_T fwd = A68_TRUE, siga;
/* Primaries excepts call and slice */
    reduce (q, NO_NOTE, NO_TICK, PRIMARY, IDENTIFIER, STOP);
    reduce (q, NO_NOTE, NO_TICK, PRIMARY, DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, CAST, DECLARER, ENCLOSED_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, CAST, VOID_SYMBOL, ENCLOSED_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, ASSERTION, ASSERT_SYMBOL, ENCLOSED_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, PRIMARY, CAST, STOP);
    reduce (q, NO_NOTE, NO_TICK, PRIMARY, ENCLOSED_CLAUSE, STOP);
    reduce (q, NO_NOTE, NO_TICK, PRIMARY, FORMAT_TEXT, STOP);
/* Call and slice */
    siga = A68_TRUE;
    while (siga) {
      NODE_T *x = NEXT (q);
      siga = A68_FALSE;
      if (IS (q, PRIMARY) && x != NO_NODE) {
        if (IS (x, OPEN_SYMBOL)) {
          reduce_branch (NEXT (q), GENERIC_ARGUMENT);
          reduce (q, NO_NOTE, &siga, SPECIFICATION, PRIMARY, GENERIC_ARGUMENT, STOP);
          reduce (q, NO_NOTE, &siga, PRIMARY, SPECIFICATION, STOP);
        } else if (IS (x, SUB_SYMBOL)) {
          reduce_branch (NEXT (q), GENERIC_ARGUMENT);
          reduce (q, NO_NOTE, &siga, SPECIFICATION, PRIMARY, GENERIC_ARGUMENT, STOP);
          reduce (q, NO_NOTE, &siga, PRIMARY, SPECIFICATION, STOP);
        }
      }
    }
/* Now that call and slice are known, reduce remaining ( .. ) */
    if (IS (q, OPEN_SYMBOL) && SUB (q) != NO_NODE) {
      reduce_branch (q, SOME_CLAUSE);
      reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CLOSED_CLAUSE, STOP);
      reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, STOP);
      reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, STOP);
      reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CASE_CLAUSE, STOP);
      reduce (q, NO_NOTE, NO_TICK, ENCLOSED_CLAUSE, CONFORMITY_CLAUSE, STOP);
      if (PREVIOUS (q) != NO_NODE) {
        BACKWARD (q);
        fwd = A68_FALSE;
      }
    }
/* Format text items */
    if (expect == FORMAT_TEXT) {
      NODE_T *r;
      for (r = p; r != NO_NODE; FORWARD (r)) {
        reduce (r, NO_NOTE, NO_TICK, DYNAMIC_REPLICATOR, FORMAT_ITEM_N, ENCLOSED_CLAUSE, STOP);
        reduce (r, NO_NOTE, NO_TICK, GENERAL_PATTERN, FORMAT_ITEM_G, ENCLOSED_CLAUSE, STOP);
        reduce (r, NO_NOTE, NO_TICK, GENERAL_PATTERN, FORMAT_ITEM_H, ENCLOSED_CLAUSE, STOP);
        reduce (r, NO_NOTE, NO_TICK, FORMAT_PATTERN, FORMAT_ITEM_F, ENCLOSED_CLAUSE, STOP);
      }
    }
    if (fwd) {
      FORWARD (q);
    }
  }
}

/**
@brief Enforce that ambiguous patterns are separated by commas.
@param p Node in syntax tree.
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
  NODE_T *q, *last_pat = NO_NODE;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    switch (ATTRIBUTE (q)) {
    case INTEGRAL_PATTERN: /* These are the potentially ambiguous patterns */
    case REAL_PATTERN:
    case COMPLEX_PATTERN:
    case BITS_PATTERN:
      {
        if (last_pat != NO_NODE) {
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_COMMA_MUST_SEPARATE, ATTRIBUTE (last_pat), ATTRIBUTE (q));
        }
        last_pat = q;
        break;
      }
    case COMMA_SYMBOL:
      {
        last_pat = NO_NODE;
        break;
      }
    }
  }
}

/**
@brief Reduce format texts completely.
@param p Node in syntax tree.
@param pr Production rule.
@param let Letter.
**/

void reduce_c_pattern (NODE_T * p, int pr, int let)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_POINT, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, FORMAT_ITEM_POINT, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_POINT, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, FORMAT_ITEM_POINT, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, REPLICATOR, let, STOP);
    reduce (q, NO_NOTE, NO_TICK, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, STOP);
  }
}

/**
@brief Reduce format texts completely.
@param p Node in syntax tree.
**/

static void reduce_format_texts (NODE_T * p)
{
  NODE_T *q;
/* Replicators */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, REPLICATOR, STATIC_REPLICATOR, STOP);
    reduce (q, NO_NOTE, NO_TICK, REPLICATOR, DYNAMIC_REPLICATOR, STOP);
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
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, RADIX_FRAME, REPLICATOR, FORMAT_ITEM_R, STOP);
  }
/* Insertions */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, INSERTION, FORMAT_ITEM_X, STOP);
    reduce (q, NO_NOTE, NO_TICK, INSERTION, FORMAT_ITEM_Y, STOP);
    reduce (q, NO_NOTE, NO_TICK, INSERTION, FORMAT_ITEM_L, STOP);
    reduce (q, NO_NOTE, NO_TICK, INSERTION, FORMAT_ITEM_P, STOP);
    reduce (q, NO_NOTE, NO_TICK, INSERTION, FORMAT_ITEM_Q, STOP);
    reduce (q, NO_NOTE, NO_TICK, INSERTION, FORMAT_ITEM_K, STOP);
    reduce (q, NO_NOTE, NO_TICK, INSERTION, LITERAL, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, INSERTION, REPLICATOR, INSERTION, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, INSERTION, INSERTION, INSERTION, STOP);
    }
  }
/* Replicated suppressible frames */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, FORMAT_A_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_A, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_Z_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_Z, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_D_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_D, STOP);
  }
/* Suppressible frames */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, FORMAT_A_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_A, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_Z_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_Z, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_D_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_D, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_E_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_E, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_POINT_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_POINT, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_I_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_I, STOP);
  }
/* Replicated frames */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, FORMAT_A_FRAME, REPLICATOR, FORMAT_ITEM_A, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_Z_FRAME, REPLICATOR, FORMAT_ITEM_Z, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_D_FRAME, REPLICATOR, FORMAT_ITEM_D, STOP);
  }
/* Frames */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, FORMAT_A_FRAME, FORMAT_ITEM_A, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_Z_FRAME, FORMAT_ITEM_Z, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_D_FRAME, FORMAT_ITEM_D, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_E_FRAME, FORMAT_ITEM_E, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_I_FRAME, FORMAT_ITEM_I, STOP);
  }
/* Frames with an insertion */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, FORMAT_A_FRAME, INSERTION, FORMAT_A_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_Z_FRAME, INSERTION, FORMAT_Z_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_D_FRAME, INSERTION, FORMAT_D_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_E_FRAME, INSERTION, FORMAT_E_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_POINT_FRAME, INSERTION, FORMAT_POINT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMAT_I_FRAME, INSERTION, FORMAT_I_FRAME, STOP);
  }
/* String patterns */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, STRING_PATTERN, REPLICATOR, FORMAT_A_FRAME, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, STRING_PATTERN, FORMAT_A_FRAME, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, STRING_PATTERN, STRING_PATTERN, STRING_PATTERN, STOP);
      reduce (q, NO_NOTE, &siga, STRING_PATTERN, STRING_PATTERN, INSERTION, STRING_PATTERN, STOP);
    }
  }
/* Integral moulds */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, INTEGRAL_MOULD, FORMAT_Z_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, INTEGRAL_MOULD, FORMAT_D_FRAME, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, INTEGRAL_MOULD, INTEGRAL_MOULD, INTEGRAL_MOULD, STOP);
      reduce (q, NO_NOTE, &siga, INTEGRAL_MOULD, INTEGRAL_MOULD, INSERTION, STOP);
    }
  }
/* Sign moulds */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_ITEM_PLUS, STOP);
    reduce (q, NO_NOTE, NO_TICK, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_ITEM_MINUS, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, SIGN_MOULD, FORMAT_ITEM_PLUS, STOP);
    reduce (q, NO_NOTE, NO_TICK, SIGN_MOULD, FORMAT_ITEM_MINUS, STOP);
  }
/* Exponent frames */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, EXPONENT_FRAME, FORMAT_E_FRAME, SIGN_MOULD, INTEGRAL_MOULD, STOP);
    reduce (q, NO_NOTE, NO_TICK, EXPONENT_FRAME, FORMAT_E_FRAME, INTEGRAL_MOULD, STOP);
  }
/* Real patterns */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, FORMAT_POINT_FRAME, INTEGRAL_MOULD, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, EXPONENT_FRAME, STOP);
    reduce (q, NO_NOTE, NO_TICK, REAL_PATTERN, INTEGRAL_MOULD, EXPONENT_FRAME, STOP);
  }
/* Complex patterns */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, COMPLEX_PATTERN, REAL_PATTERN, FORMAT_I_FRAME, REAL_PATTERN, STOP);
  }
/* Bits patterns */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, BITS_PATTERN, RADIX_FRAME, INTEGRAL_MOULD, STOP);
  }
/* Integral patterns */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, INTEGRAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, STOP);
    reduce (q, NO_NOTE, NO_TICK, INTEGRAL_PATTERN, INTEGRAL_MOULD, STOP);
  }
/* Patterns */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, BOOLEAN_PATTERN, FORMAT_ITEM_B, COLLECTION, STOP);
    reduce (q, NO_NOTE, NO_TICK, CHOICE_PATTERN, FORMAT_ITEM_C, COLLECTION, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, BOOLEAN_PATTERN, FORMAT_ITEM_B, STOP);
    reduce (q, NO_NOTE, NO_TICK, GENERAL_PATTERN, FORMAT_ITEM_G, STOP);
    reduce (q, NO_NOTE, NO_TICK, GENERAL_PATTERN, FORMAT_ITEM_H, STOP);
  }
  ambiguous_patterns (p);
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, a68_extension, NO_TICK, A68_PATTERN, BITS_C_PATTERN, STOP);
    reduce (q, a68_extension, NO_TICK, A68_PATTERN, CHAR_C_PATTERN, STOP);
    reduce (q, a68_extension, NO_TICK, A68_PATTERN, FIXED_C_PATTERN, STOP);
    reduce (q, a68_extension, NO_TICK, A68_PATTERN, FLOAT_C_PATTERN, STOP);
    reduce (q, a68_extension, NO_TICK, A68_PATTERN, GENERAL_C_PATTERN, STOP);
    reduce (q, a68_extension, NO_TICK, A68_PATTERN, INTEGRAL_C_PATTERN, STOP);
    reduce (q, a68_extension, NO_TICK, A68_PATTERN, STRING_C_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, BITS_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, BOOLEAN_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, CHOICE_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, COMPLEX_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, FORMAT_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, GENERAL_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, INTEGRAL_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, REAL_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, A68_PATTERN, STRING_PATTERN, STOP);
  }
/* Pictures */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, PICTURE, INSERTION, STOP);
    reduce (q, NO_NOTE, NO_TICK, PICTURE, A68_PATTERN, STOP);
    reduce (q, NO_NOTE, NO_TICK, PICTURE, COLLECTION, STOP);
    reduce (q, NO_NOTE, NO_TICK, PICTURE, REPLICATOR, COLLECTION, STOP);
  }
/* Picture lists */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, PICTURE)) {
      BOOL_T siga = A68_TRUE;
      reduce (q, NO_NOTE, NO_TICK, PICTURE_LIST, PICTURE, STOP);
      while (siga) {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, PICTURE_LIST, PICTURE_LIST, COMMA_SYMBOL, PICTURE, STOP);
 /* We filtered ambiguous patterns, so commas may be omitted */
        reduce (q, NO_NOTE, &siga, PICTURE_LIST, PICTURE_LIST, PICTURE, STOP);
      }
    }
  }
}

/**
@brief Reduce secondaries completely.
@param p Node in syntax tree.
**/

static void reduce_secondaries (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, SECONDARY, PRIMARY, STOP);
    reduce (q, NO_NOTE, NO_TICK, GENERATOR, LOC_SYMBOL, DECLARER, STOP);
    reduce (q, NO_NOTE, NO_TICK, GENERATOR, HEAP_SYMBOL, DECLARER, STOP);
    reduce (q, NO_NOTE, NO_TICK, GENERATOR, NEW_SYMBOL, DECLARER, STOP);
    reduce (q, NO_NOTE, NO_TICK, SECONDARY, GENERATOR, STOP);
  }
  siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    for (q = p; NEXT (q) != NO_NODE; FORWARD (q)) {
      ;
    }
    for (; q != NO_NODE; BACKWARD (q)) {
      reduce (q, NO_NOTE, &siga, SELECTION, SELECTOR, SECONDARY, STOP);
      reduce (q, NO_NOTE, &siga, SECONDARY, SELECTION, STOP);
    }
  }
}

/**
@brief Whether "q" is an operator with priority "k".
@param q Operator token.
@param k Priority.
@return Whether "q" is an operator with priority "k".
**/

static int operator_with_priority (NODE_T * q, int k)
{
  return (NEXT (q) != NO_NODE && ATTRIBUTE (NEXT (q)) == OPERATOR && PRIO (INFO (NEXT (q))) == k);
}

/**
@brief Reduce formulae.
@param p Node in syntax tree.
**/

static void reduce_formulae (NODE_T * p)
{
  NODE_T *q = p;
  int priority;
  while (q != NO_NODE) {
    if (is_one_of (q, OPERATOR, SECONDARY, STOP)) {
      q = reduce_dyadic (q, STOP);
    } else {
      FORWARD (q);
    }
  }
/* Reduce the expression */
  for (priority = MAX_PRIORITY; priority >= 0; priority--) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      if (operator_with_priority (q, priority)) {
        BOOL_T siga = A68_FALSE;
        NODE_T *op = NEXT (q);
        if (IS (q, SECONDARY)) {
          reduce (q, NO_NOTE, &siga, FORMULA, SECONDARY, OPERATOR, SECONDARY, STOP);
          reduce (q, NO_NOTE, &siga, FORMULA, SECONDARY, OPERATOR, MONADIC_FORMULA, STOP);
          reduce (q, NO_NOTE, &siga, FORMULA, SECONDARY, OPERATOR, FORMULA, STOP);
        } else if (IS (q, MONADIC_FORMULA)) {
          reduce (q, NO_NOTE, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, SECONDARY, STOP);
          reduce (q, NO_NOTE, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, MONADIC_FORMULA, STOP);
          reduce (q, NO_NOTE, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, FORMULA, STOP);
        }
        if (priority == 0 && siga) {
          diagnostic_node (A68_SYNTAX_ERROR, op, ERROR_NO_PRIORITY);
        }
        siga = A68_TRUE;
        while (siga) {
          NODE_T *op2 = NEXT (q);
          siga = A68_FALSE;
          if (operator_with_priority (q, priority)) {
            reduce (q, NO_NOTE, &siga, FORMULA, FORMULA, OPERATOR, SECONDARY, STOP);
          }
          if (operator_with_priority (q, priority)) {
            reduce (q, NO_NOTE, &siga, FORMULA, FORMULA, OPERATOR, MONADIC_FORMULA, STOP);
          }
          if (operator_with_priority (q, priority)) {
            reduce (q, NO_NOTE, &siga, FORMULA, FORMULA, OPERATOR, FORMULA, STOP);
          }
          if (priority == 0 && siga) {
            diagnostic_node (A68_SYNTAX_ERROR, op2, ERROR_NO_PRIORITY);
          }
        }
      }
    }
  }
}

/**
@brief Reduce dyadic expressions.
@param p Node in syntax tree.
@param u Current priority.
@return Token from where to continue.
**/

static NODE_T *reduce_dyadic (NODE_T * p, int u)
{
/* We work inside out - higher priority expressions get reduced first */
  if (u > MAX_PRIORITY) {
    if (p == NO_NODE) {
      return (NO_NODE);
    } else if (IS (p, OPERATOR)) {
/* Reduce monadic formulas */
      NODE_T *q = p;
      BOOL_T siga;
      do {
        PRIO (INFO (q)) = 10;
        siga = (BOOL_T) ((NEXT (q) != NO_NODE) && (IS (NEXT (q), OPERATOR)));
        if (siga) {
          FORWARD (q);
        }
      } while (siga);
      reduce (q, NO_NOTE, NO_TICK, MONADIC_FORMULA, OPERATOR, SECONDARY, STOP);
      while (q != p) {
        BACKWARD (q);
        reduce (q, NO_NOTE, NO_TICK, MONADIC_FORMULA, OPERATOR, MONADIC_FORMULA, STOP);
      }
    }
    FORWARD (p);
  } else {
    p = reduce_dyadic (p, u + 1);
    while (p != NO_NODE && IS (p, OPERATOR) && PRIO (INFO (p)) == u) {
      FORWARD (p);
      p = reduce_dyadic (p, u + 1);
    }
  }
  return (p);
}

/**
@brief Reduce tertiaries completely.
@param p Node in syntax tree.
**/

static void reduce_tertiaries (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, TERTIARY, NIHIL, STOP);
    reduce (q, NO_NOTE, NO_TICK, FORMULA, MONADIC_FORMULA, STOP);
    reduce (q, NO_NOTE, NO_TICK, TERTIARY, FORMULA, STOP);
    reduce (q, NO_NOTE, NO_TICK, TERTIARY, SECONDARY, STOP);
  }
  siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    for (q = p; q != NO_NODE; FORWARD (q)) {
      reduce (q, NO_NOTE, &siga, TRANSPOSE_FUNCTION, TRANSPOSE_SYMBOL, TERTIARY, STOP);
      reduce (q, NO_NOTE, &siga, DIAGONAL_FUNCTION, TERTIARY, DIAGONAL_SYMBOL, TERTIARY, STOP);
      reduce (q, NO_NOTE, &siga, DIAGONAL_FUNCTION, DIAGONAL_SYMBOL, TERTIARY, STOP);
      reduce (q, NO_NOTE, &siga, COLUMN_FUNCTION, TERTIARY, COLUMN_SYMBOL, TERTIARY, STOP);
      reduce (q, NO_NOTE, &siga, COLUMN_FUNCTION, COLUMN_SYMBOL, TERTIARY, STOP);
      reduce (q, NO_NOTE, &siga, ROW_FUNCTION, TERTIARY, ROW_SYMBOL, TERTIARY, STOP);
      reduce (q, NO_NOTE, &siga, ROW_FUNCTION, ROW_SYMBOL, TERTIARY, STOP);
    }
    for (q = p; q != NO_NODE; FORWARD (q)) {
      reduce (q, a68_extension, &siga, TERTIARY, TRANSPOSE_FUNCTION, STOP);
      reduce (q, a68_extension, &siga, TERTIARY, DIAGONAL_FUNCTION, STOP);
      reduce (q, a68_extension, &siga, TERTIARY, COLUMN_FUNCTION, STOP);
      reduce (q, a68_extension, &siga, TERTIARY, ROW_FUNCTION, STOP);
    }
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, IDENTITY_RELATION, TERTIARY, IS_SYMBOL, TERTIARY, STOP);
    reduce (q, NO_NOTE, NO_TICK, IDENTITY_RELATION, TERTIARY, ISNT_SYMBOL, TERTIARY, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, AND_FUNCTION, TERTIARY, ANDF_SYMBOL, TERTIARY, STOP);
    reduce (q, NO_NOTE, NO_TICK, OR_FUNCTION, TERTIARY, ORF_SYMBOL, TERTIARY, STOP);
  }
}

/**
@brief Reduce units.
@param p Node in syntax tree.
**/

static void reduce_units (NODE_T * p)
{
  NODE_T *q;
/* Stray ~ is a SKIP */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, OPERATOR) && IS_LITERALLY (q, "~")) {
      ATTRIBUTE (q) = SKIP;
    }
  }
/* Reduce units */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, UNIT, ASSIGNATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, UNIT, IDENTITY_RELATION, STOP);
    reduce (q, a68_extension, NO_TICK, UNIT, AND_FUNCTION, STOP);
    reduce (q, a68_extension, NO_TICK, UNIT, OR_FUNCTION, STOP);
    reduce (q, NO_NOTE, NO_TICK, UNIT, ROUTINE_TEXT, STOP);
    reduce (q, NO_NOTE, NO_TICK, UNIT, JUMP, STOP);
    reduce (q, NO_NOTE, NO_TICK, UNIT, SKIP, STOP);
    reduce (q, NO_NOTE, NO_TICK, UNIT, TERTIARY, STOP);
    reduce (q, NO_NOTE, NO_TICK, UNIT, ASSERTION, STOP);
    reduce (q, NO_NOTE, NO_TICK, UNIT, CODE_CLAUSE, STOP);
  }
}

/**
@brief Reduce_generic arguments.
@param p Node in syntax tree.
**/

static void reduce_generic_arguments (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, UNIT)) {
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, COLON_SYMBOL, UNIT, AT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, COLON_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, COLON_SYMBOL, AT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, COLON_SYMBOL, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, DOTDOT_SYMBOL, UNIT, AT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, DOTDOT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, DOTDOT_SYMBOL, AT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, DOTDOT_SYMBOL, STOP);
    } else if (IS (q, COLON_SYMBOL)) {
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, COLON_SYMBOL, UNIT, AT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, COLON_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, COLON_SYMBOL, AT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, COLON_SYMBOL, STOP);
    } else if (IS (q, DOTDOT_SYMBOL)) {
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, DOTDOT_SYMBOL, UNIT, AT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, DOTDOT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, DOTDOT_SYMBOL, AT_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, NO_TICK, TRIMMER, DOTDOT_SYMBOL, STOP);
    }
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, TRIMMER, UNIT, AT_SYMBOL, UNIT, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, TRIMMER, AT_SYMBOL, UNIT, STOP);
  }
  for (q = p; q && NEXT (q); FORWARD (q)) {
    if (IS (q, COMMA_SYMBOL)) {
      if (!(ATTRIBUTE (NEXT (q)) == UNIT || ATTRIBUTE (NEXT (q)) == TRIMMER)) {
        pad_node (q, TRIMMER);
      }
    } else {
      if (IS (NEXT (q), COMMA_SYMBOL)) {
        if (ISNT (q, UNIT) && ISNT (q, TRIMMER)) {
          pad_node (q, TRIMMER);
        }
      }
    }
  }
  q = NEXT (p);
  ABEND (q == NO_NODE, "erroneous parser state", NO_TEXT);
  reduce (q, NO_NOTE, NO_TICK, GENERIC_ARGUMENT_LIST, UNIT, STOP);
  reduce (q, NO_NOTE, NO_TICK, GENERIC_ARGUMENT_LIST, TRIMMER, STOP);
  do {
    siga = A68_FALSE;
    reduce (q, NO_NOTE, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, COMMA_SYMBOL, UNIT, STOP);
    reduce (q, NO_NOTE, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, COMMA_SYMBOL, TRIMMER, STOP);
    reduce (q, strange_separator, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, UNIT, STOP);
    reduce (q, strange_separator, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, TRIMMER, STOP);
  } while (siga);
}

/**
@brief Reduce bounds.
@param p Node in syntax tree.
**/

static void reduce_bounds (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, BOUND, UNIT, COLON_SYMBOL, UNIT, STOP);
    reduce (q, NO_NOTE, NO_TICK, BOUND, UNIT, DOTDOT_SYMBOL, UNIT, STOP);
    reduce (q, NO_NOTE, NO_TICK, BOUND, UNIT, STOP);
  }
  q = NEXT (p);
  reduce (q, NO_NOTE, NO_TICK, BOUNDS_LIST, BOUND, STOP);
  reduce (q, NO_NOTE, NO_TICK, FORMAL_BOUNDS_LIST, COMMA_SYMBOL, STOP);
  reduce (q, NO_NOTE, NO_TICK, ALT_FORMAL_BOUNDS_LIST, COLON_SYMBOL, STOP);
  reduce (q, NO_NOTE, NO_TICK, ALT_FORMAL_BOUNDS_LIST, DOTDOT_SYMBOL, STOP);
  do {
    siga = A68_FALSE;
    reduce (q, NO_NOTE, &siga, BOUNDS_LIST, BOUNDS_LIST, COMMA_SYMBOL, BOUND, STOP);
    reduce (q, NO_NOTE, &siga, FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, COMMA_SYMBOL, STOP);
    reduce (q, NO_NOTE, &siga, ALT_FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, COLON_SYMBOL, STOP);
    reduce (q, NO_NOTE, &siga, ALT_FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, DOTDOT_SYMBOL, STOP);
    reduce (q, NO_NOTE, &siga, FORMAL_BOUNDS_LIST, ALT_FORMAL_BOUNDS_LIST, COMMA_SYMBOL, STOP);
    reduce (q, strange_separator, &siga, BOUNDS_LIST, BOUNDS_LIST, BOUND, STOP);
  } while (siga);
}

/**
@brief Reduce argument packs.
@param p Node in syntax tree.
**/

static void reduce_arguments (NODE_T * p)
{
  if (NEXT (p) != NO_NODE) {
    NODE_T *q = NEXT (p);
    BOOL_T siga;
    reduce (q, NO_NOTE, NO_TICK, ARGUMENT_LIST, UNIT, STOP);
    do {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, ARGUMENT_LIST, ARGUMENT_LIST, COMMA_SYMBOL, UNIT, STOP);
      reduce (q, strange_separator, &siga, ARGUMENT_LIST, ARGUMENT_LIST, UNIT, STOP);
    } while (siga);
  }
}

/**
@brief Reduce declarations.
@param p Node in syntax tree.
**/

static void reduce_basic_declarations (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, ENVIRON_NAME, ENVIRON_SYMBOL, ROW_CHAR_DENOTATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, PRIORITY_DECLARATION, PRIO_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, PRIORITY, STOP);
    reduce (q, NO_NOTE, NO_TICK, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, DECLARER, STOP);
    reduce (q, NO_NOTE, NO_TICK, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, VOID_SYMBOL, STOP);
    reduce (q, NO_NOTE, NO_TICK, PROCEDURE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, ROUTINE_TEXT, STOP);
    reduce (q, NO_NOTE, NO_TICK, PROCEDURE_VARIABLE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, STOP);
    reduce (q, NO_NOTE, NO_TICK, PROCEDURE_VARIABLE_DECLARATION, QUALIFIER, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, STOP);
    reduce (q, NO_NOTE, NO_TICK, BRIEF_OPERATOR_DECLARATION, OP_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, ROUTINE_TEXT, STOP);
/* Errors */
    reduce (q, strange_tokens, NO_TICK, PRIORITY_DECLARATION, PRIO_SYMBOL, -DEFINING_OPERATOR, -EQUALS_SYMBOL, -PRIORITY, STOP);
    reduce (q, strange_tokens, NO_TICK, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, -DECLARER, STOP);
    reduce (q, strange_tokens, NO_TICK, PROCEDURE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, -ROUTINE_TEXT, STOP);
    reduce (q, strange_tokens, NO_TICK, PROCEDURE_VARIABLE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, -ROUTINE_TEXT, STOP);
    reduce (q, strange_tokens, NO_TICK, PROCEDURE_VARIABLE_DECLARATION, QUALIFIER, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, -ROUTINE_TEXT, STOP);
    reduce (q, strange_tokens, NO_TICK, BRIEF_OPERATOR_DECLARATION, OP_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, -ROUTINE_TEXT, STOP);
/* Errors. WILDCARD catches TERTIARY which catches IDENTIFIER */
    reduce (q, strange_tokens, NO_TICK, PROCEDURE_DECLARATION, PROC_SYMBOL, WILDCARD, ROUTINE_TEXT, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, ENVIRON_NAME, ENVIRON_NAME, COMMA_SYMBOL, ROW_CHAR_DENOTATION, STOP);
      reduce (q, NO_NOTE, &siga, PRIORITY_DECLARATION, PRIORITY_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, PRIORITY, STOP);
      reduce (q, NO_NOTE, &siga, MODE_DECLARATION, MODE_DECLARATION, COMMA_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, DECLARER, STOP);
      reduce (q, NO_NOTE, &siga, MODE_DECLARATION, MODE_DECLARATION, COMMA_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, VOID_SYMBOL, STOP);
      reduce (q, NO_NOTE, &siga, PROCEDURE_DECLARATION, PROCEDURE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, ROUTINE_TEXT, STOP);
      reduce (q, NO_NOTE, &siga, PROCEDURE_VARIABLE_DECLARATION, PROCEDURE_VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, STOP);
      reduce (q, NO_NOTE, &siga, BRIEF_OPERATOR_DECLARATION, BRIEF_OPERATOR_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, ROUTINE_TEXT, STOP);
/* Errors. WILDCARD catches TERTIARY which catches IDENTIFIER */
      reduce (q, strange_tokens, &siga, PROCEDURE_DECLARATION, PROCEDURE_DECLARATION, COMMA_SYMBOL, WILDCARD, ROUTINE_TEXT, STOP);
    } while (siga);
  }
}

/**
@brief Reduce declaration lists.
@param p Node in syntax tree.
**/

static void reduce_declaration_lists (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, IDENTITY_DECLARATION, DECLARER, DEFINING_IDENTIFIER, EQUALS_SYMBOL, UNIT, STOP);
    reduce (q, NO_NOTE, NO_TICK, VARIABLE_DECLARATION, QUALIFIER, DECLARER, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP);
    reduce (q, NO_NOTE, NO_TICK, VARIABLE_DECLARATION, QUALIFIER, DECLARER, DEFINING_IDENTIFIER, STOP);
    reduce (q, NO_NOTE, NO_TICK, VARIABLE_DECLARATION, DECLARER, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP);
    reduce (q, NO_NOTE, NO_TICK, VARIABLE_DECLARATION, DECLARER, DEFINING_IDENTIFIER, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, IDENTITY_DECLARATION, IDENTITY_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, UNIT, STOP);
      reduce (q, NO_NOTE, &siga, VARIABLE_DECLARATION, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP);
      if (!whether (q, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP)) {
        reduce (q, NO_NOTE, &siga, VARIABLE_DECLARATION, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, STOP);
      }
    } while (siga);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, OPERATOR_DECLARATION, OPERATOR_PLAN, DEFINING_OPERATOR, EQUALS_SYMBOL, UNIT, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, OPERATOR_DECLARATION, OPERATOR_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, UNIT, STOP);
    } while (siga);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, MODE_DECLARATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, PRIORITY_DECLARATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, BRIEF_OPERATOR_DECLARATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, OPERATOR_DECLARATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, IDENTITY_DECLARATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, PROCEDURE_DECLARATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, PROCEDURE_VARIABLE_DECLARATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, VARIABLE_DECLARATION, STOP);
    reduce (q, NO_NOTE, NO_TICK, DECLARATION_LIST, ENVIRON_NAME, STOP);
  }
  for (q = p; q != NO_NODE; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      reduce (q, NO_NOTE, &siga, DECLARATION_LIST, DECLARATION_LIST, COMMA_SYMBOL, DECLARATION_LIST, STOP);
    } while (siga);
  }
}

/**
@brief Reduce serial clauses.
@param p Node in syntax tree.
**/

static void reduce_serial_clauses (NODE_T * p)
{
  if (NEXT (p) != NO_NODE) {
    NODE_T *q = NEXT (p), *u;
    BOOL_T siga, label_seen;
/* Check wrong exits */
    for (u = q; u != NO_NODE; FORWARD (u)) {
      if (IS (u, EXIT_SYMBOL)) {
        if (NEXT (u) == NO_NODE || ISNT (NEXT (u), LABELED_UNIT)) {
          diagnostic_node (A68_SYNTAX_ERROR, u, ERROR_LABELED_UNIT_MUST_FOLLOW);
        }
      }
    }
/* Check wrong jumps and declarations */
    for (u = q, label_seen = A68_FALSE; u != NO_NODE; FORWARD (u)) {
      if (IS (u, LABELED_UNIT)) {
        label_seen = A68_TRUE;
      } else if (IS (u, DECLARATION_LIST)) {
        if (label_seen) {
          diagnostic_node (A68_SYNTAX_ERROR, u, ERROR_LABEL_BEFORE_DECLARATION);
        }
      }
    }
/* Reduce serial clauses */
    reduce (q, NO_NOTE, NO_TICK, SERIAL_CLAUSE, LABELED_UNIT, STOP);
    reduce (q, NO_NOTE, NO_TICK, SERIAL_CLAUSE, UNIT, STOP);
    reduce (q, NO_NOTE, NO_TICK, INITIALISER_SERIES, DECLARATION_LIST, STOP);
    do {
      siga = A68_FALSE;
      if (IS (q, SERIAL_CLAUSE)) {
        reduce (q, NO_NOTE, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, SEMI_SYMBOL, UNIT, STOP);
        reduce (q, NO_NOTE, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, EXIT_SYMBOL, LABELED_UNIT, STOP);
        reduce (q, NO_NOTE, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, SEMI_SYMBOL, LABELED_UNIT, STOP);
        reduce (q, NO_NOTE, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, SEMI_SYMBOL, DECLARATION_LIST, STOP);
 /* Errors */
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COMMA_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COMMA_SYMBOL, LABELED_UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, COMMA_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COLON_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COLON_SYMBOL, LABELED_UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, COLON_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, UNIT, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, LABELED_UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, DECLARATION_LIST, STOP);
      } else if (IS (q, INITIALISER_SERIES)) {
        reduce (q, NO_NOTE, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, UNIT, STOP);
        reduce (q, NO_NOTE, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, LABELED_UNIT, STOP);
        reduce (q, NO_NOTE, &siga, INITIALISER_SERIES, INITIALISER_SERIES, SEMI_SYMBOL, DECLARATION_LIST, STOP);
 /* Errors */
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, LABELED_UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COMMA_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, LABELED_UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COLON_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, UNIT, STOP);
        reduce (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, LABELED_UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, DECLARATION_LIST, STOP);
      }
    } 
    while (siga);
  }
}

/**
@brief Reduce enquiry clauses.
@param p Node in syntax tree.
**/

static void reduce_enquiry_clauses (NODE_T * p)
{
  if (NEXT (p) != NO_NODE) {
    NODE_T *q = NEXT (p);
    BOOL_T siga;
    reduce (q, NO_NOTE, NO_TICK, ENQUIRY_CLAUSE, UNIT, STOP);
    reduce (q, NO_NOTE, NO_TICK, INITIALISER_SERIES, DECLARATION_LIST, STOP);
    do {
      siga = A68_FALSE;
      if (IS (q, ENQUIRY_CLAUSE)) {
        reduce (q, NO_NOTE, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, SEMI_SYMBOL, UNIT, STOP);
        reduce (q, NO_NOTE, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, SEMI_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, COMMA_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, COMMA_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, COLON_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, COLON_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, DECLARATION_LIST, STOP);
      } else if (IS (q, INITIALISER_SERIES)) {
        reduce (q, NO_NOTE, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, UNIT, STOP);
        reduce (q, NO_NOTE, &siga, INITIALISER_SERIES, INITIALISER_SERIES, SEMI_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COMMA_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COLON_SYMBOL, DECLARATION_LIST, STOP);
        reduce (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, UNIT, STOP);
        reduce (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, DECLARATION_LIST, STOP);
      }
    } 
    while (siga);
  }
}

/**
@brief Reduce collateral clauses.
@param p Node in syntax tree.
**/

static void reduce_collateral_clauses (NODE_T * p)
{
  if (NEXT (p) != NO_NODE) {
    NODE_T *q = NEXT (p);
    if (IS (q, UNIT)) {
      BOOL_T siga;
      reduce (q, NO_NOTE, NO_TICK, UNIT_LIST, UNIT, STOP);
      do {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, UNIT_LIST, UNIT_LIST, COMMA_SYMBOL, UNIT, STOP);
        reduce (q, strange_separator, &siga, UNIT_LIST, UNIT_LIST, UNIT, STOP);
      } while (siga);
    } else if (IS (q, SPECIFIED_UNIT)) {
      BOOL_T siga;
      reduce (q, NO_NOTE, NO_TICK, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, STOP);
      do {
        siga = A68_FALSE;
        reduce (q, NO_NOTE, &siga, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT_LIST, COMMA_SYMBOL, SPECIFIED_UNIT, STOP);
        reduce (q, strange_separator, &siga, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, STOP);
      } while (siga);
    }
  }
}

/**
@brief Reduces enclosed clauses.
@param q Node in syntax tree.
@param expect Information the parser may have on what is expected.
**/

static void reduce_enclosed_clauses (NODE_T * q, int expect)
{
  NODE_T *p = q;
  if (SUB (p) == NO_NODE) {
    if (IS (p, FOR_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, FOR_PART, FOR_SYMBOL, DEFINING_IDENTIFIER, STOP);
    } else if (IS (p, OPEN_SYMBOL)) {
      if (expect == ENQUIRY_CLAUSE) {
        reduce (p, NO_NOTE, NO_TICK, OPEN_PART, OPEN_SYMBOL, ENQUIRY_CLAUSE, STOP);
      } else if (expect == ARGUMENT) {
        reduce (p, NO_NOTE, NO_TICK, ARGUMENT, OPEN_SYMBOL, CLOSE_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, ARGUMENT, OPEN_SYMBOL, ARGUMENT_LIST, CLOSE_SYMBOL, STOP);
        reduce (p, empty_clause, NO_TICK, ARGUMENT, OPEN_SYMBOL, INITIALISER_SERIES, CLOSE_SYMBOL, STOP);
      } else if (expect == GENERIC_ARGUMENT) {
        if (whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, STOP)) {
          pad_node (p, TRIMMER);
          reduce (p, NO_NOTE, NO_TICK, GENERIC_ARGUMENT, OPEN_SYMBOL, TRIMMER, CLOSE_SYMBOL, STOP);
        }
        reduce (p, NO_NOTE, NO_TICK, GENERIC_ARGUMENT, OPEN_SYMBOL, GENERIC_ARGUMENT_LIST, CLOSE_SYMBOL, STOP);
      } else if (expect == BOUNDS) {
        reduce (p, NO_NOTE, NO_TICK, FORMAL_BOUNDS, OPEN_SYMBOL, CLOSE_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, BOUNDS, OPEN_SYMBOL, BOUNDS_LIST, CLOSE_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, FORMAL_BOUNDS, OPEN_SYMBOL, FORMAL_BOUNDS_LIST, CLOSE_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, FORMAL_BOUNDS, OPEN_SYMBOL, ALT_FORMAL_BOUNDS_LIST, CLOSE_SYMBOL, STOP);
      } else {
        reduce (p, NO_NOTE, NO_TICK, CLOSED_CLAUSE, OPEN_SYMBOL, SERIAL_CLAUSE, CLOSE_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, COLLATERAL_CLAUSE, OPEN_SYMBOL, UNIT_LIST, CLOSE_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, COLLATERAL_CLAUSE, OPEN_SYMBOL, CLOSE_SYMBOL, STOP);
        reduce (p, empty_clause, NO_TICK, CLOSED_CLAUSE, OPEN_SYMBOL, INITIALISER_SERIES, CLOSE_SYMBOL, STOP);
      }
    } else if (IS (p, SUB_SYMBOL)) {
      if (expect == GENERIC_ARGUMENT) {
        if (whether (p, SUB_SYMBOL, BUS_SYMBOL, STOP)) {
          pad_node (p, TRIMMER);
          reduce (p, NO_NOTE, NO_TICK, GENERIC_ARGUMENT, SUB_SYMBOL, TRIMMER, BUS_SYMBOL, STOP);
        }
        reduce (p, NO_NOTE, NO_TICK, GENERIC_ARGUMENT, SUB_SYMBOL, GENERIC_ARGUMENT_LIST, BUS_SYMBOL, STOP);
      } else if (expect == BOUNDS) {
        reduce (p, NO_NOTE, NO_TICK, FORMAL_BOUNDS, SUB_SYMBOL, BUS_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, BOUNDS, SUB_SYMBOL, BOUNDS_LIST, BUS_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, FORMAL_BOUNDS, SUB_SYMBOL, FORMAL_BOUNDS_LIST, BUS_SYMBOL, STOP);
        reduce (p, NO_NOTE, NO_TICK, FORMAL_BOUNDS, SUB_SYMBOL, ALT_FORMAL_BOUNDS_LIST, BUS_SYMBOL, STOP);
      }
    } else if (IS (p, BEGIN_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, COLLATERAL_CLAUSE, BEGIN_SYMBOL, UNIT_LIST, END_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, COLLATERAL_CLAUSE, BEGIN_SYMBOL, END_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CLOSED_CLAUSE, BEGIN_SYMBOL, SERIAL_CLAUSE, END_SYMBOL, STOP);
      reduce (p, empty_clause, NO_TICK, CLOSED_CLAUSE, BEGIN_SYMBOL, INITIALISER_SERIES, END_SYMBOL, STOP);
    } else if (IS (p, FORMAT_DELIMITER_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL, PICTURE_LIST, FORMAT_DELIMITER_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL, FORMAT_DELIMITER_SYMBOL, STOP);
    } else if (IS (p, FORMAT_OPEN_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, COLLECTION, FORMAT_OPEN_SYMBOL, PICTURE_LIST, FORMAT_CLOSE_SYMBOL, STOP);
    } else if (IS (p, IF_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, IF_PART, IF_SYMBOL, ENQUIRY_CLAUSE, STOP);
      reduce (p, empty_clause, NO_TICK, IF_PART, IF_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, THEN_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, THEN_PART, THEN_SYMBOL, SERIAL_CLAUSE, STOP);
      reduce (p, empty_clause, NO_TICK, THEN_PART, THEN_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, ELSE_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, ELSE_PART, ELSE_SYMBOL, SERIAL_CLAUSE, STOP);
      reduce (p, empty_clause, NO_TICK, ELSE_PART, ELSE_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, ELIF_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, ELIF_IF_PART, ELIF_SYMBOL, ENQUIRY_CLAUSE, STOP);
    } else if (IS (p, CASE_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, CASE_PART, CASE_SYMBOL, ENQUIRY_CLAUSE, STOP);
      reduce (p, empty_clause, NO_TICK, CASE_PART, CASE_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, IN_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, CASE_IN_PART, IN_SYMBOL, UNIT_LIST, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_IN_PART, IN_SYMBOL, SPECIFIED_UNIT_LIST, STOP);
    } else if (IS (p, OUT_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, OUT_PART, OUT_SYMBOL, SERIAL_CLAUSE, STOP);
      reduce (p, empty_clause, NO_TICK, OUT_PART, OUT_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, OUSE_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, OUSE_PART, OUSE_SYMBOL, ENQUIRY_CLAUSE, STOP);
    } else if (IS (p, THEN_BAR_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, CHOICE, THEN_BAR_SYMBOL, SERIAL_CLAUSE, STOP);
      reduce (p, NO_NOTE, NO_TICK, CASE_CHOICE_CLAUSE, THEN_BAR_SYMBOL, UNIT_LIST, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_CHOICE, THEN_BAR_SYMBOL, SPECIFIED_UNIT_LIST, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_CHOICE, THEN_BAR_SYMBOL, SPECIFIED_UNIT, STOP);
      reduce (p, empty_clause, NO_TICK, CHOICE, THEN_BAR_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, ELSE_BAR_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, ELSE_OPEN_PART, ELSE_BAR_SYMBOL, ENQUIRY_CLAUSE, STOP);
      reduce (p, empty_clause, NO_TICK, ELSE_OPEN_PART, ELSE_BAR_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, FROM_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, FROM_PART, FROM_SYMBOL, UNIT, STOP);
    } else if (IS (p, BY_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, BY_PART, BY_SYMBOL, UNIT, STOP);
    } else if (IS (p, TO_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, TO_PART, TO_SYMBOL, UNIT, STOP);
    } else if (IS (p, DOWNTO_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, TO_PART, DOWNTO_SYMBOL, UNIT, STOP);
    } else if (IS (p, WHILE_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, WHILE_PART, WHILE_SYMBOL, ENQUIRY_CLAUSE, STOP);
      reduce (p, empty_clause, NO_TICK, WHILE_PART, WHILE_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, UNTIL_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, UNTIL_PART, UNTIL_SYMBOL, ENQUIRY_CLAUSE, STOP);
      reduce (p, empty_clause, NO_TICK, UNTIL_PART, UNTIL_SYMBOL, INITIALISER_SERIES, STOP);
    } else if (IS (p, DO_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, DO_PART, DO_SYMBOL, SERIAL_CLAUSE, UNTIL_PART, OD_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, DO_PART, DO_SYMBOL, SERIAL_CLAUSE, OD_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, DO_PART, DO_SYMBOL, UNTIL_PART, OD_SYMBOL, STOP);
    } else if (IS (p, ALT_DO_SYMBOL)) {
      reduce (p, NO_NOTE, NO_TICK, ALT_DO_PART, ALT_DO_SYMBOL, SERIAL_CLAUSE, UNTIL_PART, OD_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, ALT_DO_PART, ALT_DO_SYMBOL, SERIAL_CLAUSE, OD_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, ALT_DO_PART, ALT_DO_SYMBOL, UNTIL_PART, OD_SYMBOL, STOP);
    }
  }
  p = q;
  if (SUB (p) != NO_NODE) {
    if (IS (p, OPEN_PART)) {
      reduce (p, NO_NOTE, NO_TICK, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, BRIEF_ELIF_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, CASE_CLAUSE, OPEN_PART, CASE_CHOICE_CLAUSE, CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CASE_CLAUSE, OPEN_PART, CASE_CHOICE_CLAUSE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CASE_CLAUSE, OPEN_PART, CASE_CHOICE_CLAUSE, BRIEF_OUSE_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_CLAUSE, OPEN_PART, CONFORMITY_CHOICE, CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_CLAUSE, OPEN_PART, CONFORMITY_CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_CLAUSE, OPEN_PART, CONFORMITY_CHOICE, BRIEF_CONFORMITY_OUSE_PART, STOP);
    } else if (IS (p, ELSE_OPEN_PART)) {
      reduce (p, NO_NOTE, NO_TICK, BRIEF_ELIF_PART, ELSE_OPEN_PART, CHOICE, CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, BRIEF_ELIF_PART, ELSE_OPEN_PART, CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, BRIEF_ELIF_PART, ELSE_OPEN_PART, CHOICE, BRIEF_ELIF_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, BRIEF_OUSE_PART, ELSE_OPEN_PART, CASE_CHOICE_CLAUSE, CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, BRIEF_OUSE_PART, ELSE_OPEN_PART, CASE_CHOICE_CLAUSE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, BRIEF_OUSE_PART, ELSE_OPEN_PART, CASE_CHOICE_CLAUSE, BRIEF_OUSE_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, BRIEF_CONFORMITY_OUSE_PART, ELSE_OPEN_PART, CONFORMITY_CHOICE, CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, BRIEF_CONFORMITY_OUSE_PART, ELSE_OPEN_PART, CONFORMITY_CHOICE, CLOSE_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, BRIEF_CONFORMITY_OUSE_PART, ELSE_OPEN_PART, CONFORMITY_CHOICE, BRIEF_CONFORMITY_OUSE_PART, STOP);
    } else if (IS (p, IF_PART)) {
      reduce (p, NO_NOTE, NO_TICK, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, ELSE_PART, FI_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, ELIF_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, FI_SYMBOL, STOP);
    } else if (IS (p, ELIF_IF_PART)) {
      reduce (p, NO_NOTE, NO_TICK, ELIF_PART, ELIF_IF_PART, THEN_PART, ELSE_PART, FI_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, ELIF_PART, ELIF_IF_PART, THEN_PART, FI_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, ELIF_PART, ELIF_IF_PART, THEN_PART, ELIF_PART, STOP);
    } else if (IS (p, CASE_PART)) {
      reduce (p, NO_NOTE, NO_TICK, CASE_CLAUSE, CASE_PART, CASE_IN_PART, OUT_PART, ESAC_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CASE_CLAUSE, CASE_PART, CASE_IN_PART, ESAC_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CASE_CLAUSE, CASE_PART, CASE_IN_PART, CASE_OUSE_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_CLAUSE, CASE_PART, CONFORMITY_IN_PART, OUT_PART, ESAC_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_CLAUSE, CASE_PART, CONFORMITY_IN_PART, ESAC_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_CLAUSE, CASE_PART, CONFORMITY_IN_PART, CONFORMITY_OUSE_PART, STOP);
    } else if (IS (p, OUSE_PART)) {
      reduce (p, NO_NOTE, NO_TICK, CASE_OUSE_PART, OUSE_PART, CASE_IN_PART, OUT_PART, ESAC_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CASE_OUSE_PART, OUSE_PART, CASE_IN_PART, ESAC_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CASE_OUSE_PART, OUSE_PART, CASE_IN_PART, CASE_OUSE_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_OUSE_PART, OUSE_PART, CONFORMITY_IN_PART, OUT_PART, ESAC_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_OUSE_PART, OUSE_PART, CONFORMITY_IN_PART, ESAC_SYMBOL, STOP);
      reduce (p, NO_NOTE, NO_TICK, CONFORMITY_OUSE_PART, OUSE_PART, CONFORMITY_IN_PART, CONFORMITY_OUSE_PART, STOP);
    } else if (IS (p, FOR_PART)) {
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, FROM_PART, TO_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, FROM_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, BY_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, TO_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, TO_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, FROM_PART, TO_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, FROM_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, BY_PART, TO_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, BY_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, TO_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FOR_PART, ALT_DO_PART, STOP);
    } else if (IS (p, FROM_PART)) {
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FROM_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FROM_PART, BY_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FROM_PART, TO_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FROM_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FROM_PART, BY_PART, TO_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FROM_PART, BY_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FROM_PART, TO_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, FROM_PART, ALT_DO_PART, STOP);
    } else if (IS (p, BY_PART)) {
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, BY_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, BY_PART, TO_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, BY_PART, ALT_DO_PART, STOP);
    } else if (IS (p, TO_PART)) {
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, TO_PART, WHILE_PART, ALT_DO_PART, STOP);
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, TO_PART, ALT_DO_PART, STOP);
    } else if (IS (p, WHILE_PART)) {
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, WHILE_PART, ALT_DO_PART, STOP);
    } else if (IS (p, DO_PART)) {
      reduce (p, NO_NOTE, NO_TICK, LOOP_CLAUSE, DO_PART, STOP);
    }
  }
}

/**
@brief Substitute reduction when a phrase could not be parsed.
@param p Node in syntax tree.
@param expect Information the parser may have on what is expected.
@param suppress Suppresses a diagnostic_node message (nested c.q. related diagnostics).
**/

static void recover_from_error (NODE_T * p, int expect, BOOL_T suppress)
{
/* This routine does not do fancy things as that might introduce more errors */
  NODE_T *q = p;
  if (p == NO_NODE) {
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
      if (ERROR_COUNT (&program) == 0) {
        diagnostic_node (A68_SYNTAX_ERROR, w, ERROR_SYNTAX_EXPECTED, expect);
      }
    } else {
      diagnostic_node (A68_SYNTAX_ERROR, w, ERROR_INVALID_SEQUENCE, seq, expect);
    }
    if (ERROR_COUNT (&program) >= MAX_ERRORS) {
      longjmp (bottom_up_crash_exit, 1);
    }
  }
/* Try to prevent spurious diagnostics by guessing what was expected */
  while (NEXT (q) != NO_NODE) {
    FORWARD (q);
  }
  if (is_one_of (p, BEGIN_SYMBOL, OPEN_SYMBOL, STOP)) {
    if (expect == ARGUMENT || expect == COLLATERAL_CLAUSE || expect == PARAMETER_PACK || expect == STRUCTURE_PACK || expect == UNION_PACK) {
      make_sub (p, q, expect);
    } else if (expect == ENQUIRY_CLAUSE) {
      make_sub (p, q, OPEN_PART);
    } else if (expect == FORMAL_DECLARERS) {
      make_sub (p, q, FORMAL_DECLARERS);
    } else {
      make_sub (p, q, CLOSED_CLAUSE);
    }
  } else if (IS (p, FORMAT_DELIMITER_SYMBOL) && expect == FORMAT_TEXT) {
    make_sub (p, q, FORMAT_TEXT);
  } else if (IS (p, CODE_SYMBOL)) {
    make_sub (p, q, CODE_CLAUSE);
  } else if (is_one_of (p, THEN_BAR_SYMBOL, CHOICE, STOP)) {
    make_sub (p, q, CHOICE);
  } else if (is_one_of (p, IF_SYMBOL, IF_PART, STOP)) {
    make_sub (p, q, IF_PART);
  } else if (is_one_of (p, THEN_SYMBOL, THEN_PART, STOP)) {
    make_sub (p, q, THEN_PART);
  } else if (is_one_of (p, ELSE_SYMBOL, ELSE_PART, STOP)) {
    make_sub (p, q, ELSE_PART);
  } else if (is_one_of (p, ELIF_SYMBOL, ELIF_IF_PART, STOP)) {
    make_sub (p, q, ELIF_IF_PART);
  } else if (is_one_of (p, CASE_SYMBOL, CASE_PART, STOP)) {
    make_sub (p, q, CASE_PART);
  } else if (is_one_of (p, OUT_SYMBOL, OUT_PART, STOP)) {
    make_sub (p, q, OUT_PART);
  } else if (is_one_of (p, OUSE_SYMBOL, OUSE_PART, STOP)) {
    make_sub (p, q, OUSE_PART);
  } else if (is_one_of (p, FOR_SYMBOL, FOR_PART, STOP)) {
    make_sub (p, q, FOR_PART);
  } else if (is_one_of (p, FROM_SYMBOL, FROM_PART, STOP)) {
    make_sub (p, q, FROM_PART);
  } else if (is_one_of (p, BY_SYMBOL, BY_PART, STOP)) {
    make_sub (p, q, BY_PART);
  } else if (is_one_of (p, TO_SYMBOL, DOWNTO_SYMBOL, TO_PART, STOP)) {
    make_sub (p, q, TO_PART);
  } else if (is_one_of (p, WHILE_SYMBOL, WHILE_PART, STOP)) {
    make_sub (p, q, WHILE_PART);
  } else if (is_one_of (p, UNTIL_SYMBOL, UNTIL_PART, STOP)) {
    make_sub (p, q, UNTIL_PART);
  } else if (is_one_of (p, DO_SYMBOL, DO_PART, STOP)) {
    make_sub (p, q, DO_PART);
  } else if (is_one_of (p, ALT_DO_SYMBOL, ALT_DO_PART, STOP)) {
    make_sub (p, q, ALT_DO_PART);
  } else if (non_terminal_string (edit_line, expect) != NO_TEXT) {
    make_sub (p, q, expect);
  }
}

/**
@brief Heuristic aid in pinpointing errors.
@param p Node in syntax tree.
**/

static void reduce_erroneous_units (NODE_T * p)
{
/* Constructs are reduced to units in an attempt to limit spurious diagnostics */
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
/* Some implementations allow selection from a tertiary, when there is no risk
of ambiguity. Algol68G follows RR, so some extra attention here to guide an
unsuspecting user */
    if (whether (q, SELECTOR, -SECONDARY, STOP)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, SECONDARY);
      reduce (q, NO_NOTE, NO_TICK, UNIT, SELECTOR, WILDCARD, STOP);
    }
/* Attention for identity relations that require tertiaries */
    if (whether (q, -TERTIARY, IS_SYMBOL, TERTIARY, STOP) || whether (q, TERTIARY, IS_SYMBOL, -TERTIARY, STOP) || whether (q, -TERTIARY, IS_SYMBOL, -TERTIARY, STOP)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, TERTIARY);
      reduce (q, NO_NOTE, NO_TICK, UNIT, WILDCARD, IS_SYMBOL, WILDCARD, STOP);
    } else if (whether (q, -TERTIARY, ISNT_SYMBOL, TERTIARY, STOP) || whether (q, TERTIARY, ISNT_SYMBOL, -TERTIARY, STOP) || whether (q, -TERTIARY, ISNT_SYMBOL, -TERTIARY, STOP)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, TERTIARY);
      reduce (q, NO_NOTE, NO_TICK, UNIT, WILDCARD, ISNT_SYMBOL, WILDCARD, STOP);
    }
  }
}

/* A posteriori checks of the syntax tree built by the BU parser */

/**
@brief Driver for a posteriori error checking.
@param p Node in syntax tree.
**/

void bottom_up_error_check (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, BOOLEAN_PATTERN)) {
      int k = 0;
      count_pictures (SUB (p), &k);
      if (!(k == 0 || k == 2)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_FORMAT_PICTURE_NUMBER, ATTRIBUTE (p));
      }
    } else {
      bottom_up_error_check (SUB (p));
    }
  }
}

/* Next part rearranges the tree after the symbol tables are finished */

/**
@brief Transfer IDENTIFIER to JUMP where appropriate.
@param p Node in syntax tree.
**/

void rearrange_goto_less_jumps (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      NODE_T *q = SUB (p);
      if (IS (q, TERTIARY)) {
        NODE_T *tertiary = q;
        q = SUB (q);
        if (q != NO_NODE && IS (q, SECONDARY)) {
          q = SUB (q);
          if (q != NO_NODE && IS (q, PRIMARY)) {
            q = SUB (q);
            if (q != NO_NODE && IS (q, IDENTIFIER)) {
              if (is_identifier_or_label_global (TABLE (q), NSYMBOL (q)) == LABEL) {
                ATTRIBUTE (tertiary) = JUMP;
                SUB (tertiary) = q;
              }
            }
          }
        }
      }
    } else if (IS (p, TERTIARY)) {
      NODE_T *q = SUB (p);
      if (q != NO_NODE && IS (q, SECONDARY)) {
        NODE_T *secondary = q;
        q = SUB (q);
        if (q != NO_NODE && IS (q, PRIMARY)) {
          q = SUB (q);
          if (q != NO_NODE && IS (q, IDENTIFIER)) {
            if (is_identifier_or_label_global (TABLE (q), NSYMBOL (q)) == LABEL) {
              ATTRIBUTE (secondary) = JUMP;
              SUB (secondary) = q;
            }
          }
        }
      }
    } else if (IS (p, SECONDARY)) {
      NODE_T *q = SUB (p);
      if (q != NO_NODE && IS (q, PRIMARY)) {
        NODE_T *primary = q;
        q = SUB (q);
        if (q != NO_NODE && IS (q, IDENTIFIER)) {
          if (is_identifier_or_label_global (TABLE (q), NSYMBOL (q)) == LABEL) {
            ATTRIBUTE (primary) = JUMP;
            SUB (primary) = q;
          }
        }
      }
    } else if (IS (p, PRIMARY)) {
      NODE_T *q = SUB (p);
      if (q != NO_NODE && IS (q, IDENTIFIER)) {
        if (is_identifier_or_label_global (TABLE (q), NSYMBOL (q)) == LABEL) {
          make_sub (q, q, JUMP);
        }
      }
    }
    rearrange_goto_less_jumps (SUB (p));
  }
}

/***********************************************************/
/* VICTAL checker for formal, actual and virtual declarers */
/***********************************************************/

/**
@brief Check generator.
@param p Node in syntax tree.
**/

static void victal_check_generator (NODE_T * p)
{
  if (!victal_check_declarer (NEXT (p), ACTUAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
  }
}

/**
@brief Check formal pack.
@param p Node in syntax tree.
@param x Expected attribute.
@param z Flag.
**/

static void victal_check_formal_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NO_NODE) {
    if (IS (p, FORMAL_DECLARERS)) {
      victal_check_formal_pack (SUB (p), x, z);
    } else if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_formal_pack (NEXT (p), x, z);
    } else if (IS (p, FORMAL_DECLARERS_LIST)) {
      victal_check_formal_pack (NEXT (p), x, z);
      victal_check_formal_pack (SUB (p), x, z);
    } else if (IS (p, DECLARER)) {
      victal_check_formal_pack (NEXT (p), x, z);
      (*z) &= victal_check_declarer (SUB (p), x);
    }
  }
}

/**
@brief Check operator declaration.
@param p Node in syntax tree.
**/

static void victal_check_operator_dec (NODE_T * p)
{
  if (IS (NEXT (p), FORMAL_DECLARERS)) {
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

/**
@brief Check mode declaration.
@param p Node in syntax tree.
**/

static void victal_check_mode_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
      victal_check_mode_dec (NEXT (p));
    } else if (is_one_of (p, MODE_SYMBOL, DEFINING_INDICANT, STOP)
               || is_one_of (p, EQUALS_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_mode_dec (NEXT (p));
    } else if (IS (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
    }
  }
}

/**
@brief Check variable declaration.
@param p Node in syntax tree.
**/

static void victal_check_variable_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
      victal_check_variable_dec (NEXT (p));
    } else if (is_one_of (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, STOP)
               || IS (p, COMMA_SYMBOL)) {
      victal_check_variable_dec (NEXT (p));
    } else if (IS (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (IS (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
      victal_check_variable_dec (NEXT (p));
    }
  }
}

/**
@brief Check identity declaration.
@param p Node in syntax tree.
**/

static void victal_check_identity_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
      victal_check_identity_dec (NEXT (p));
    } else if (is_one_of (p, DEFINING_IDENTIFIER, EQUALS_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_identity_dec (NEXT (p));
    } else if (IS (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (IS (p, DECLARER)) {
      if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      victal_check_identity_dec (NEXT (p));
    }
  }
}

/**
@brief Check routine pack.
@param p Node in syntax tree.
@param x Expected attribute.
@param z Flag.
**/

static void victal_check_routine_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NO_NODE) {
    if (IS (p, PARAMETER_PACK)) {
      victal_check_routine_pack (SUB (p), x, z);
    } else if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_routine_pack (NEXT (p), x, z);
    } else if (is_one_of (p, PARAMETER_LIST, PARAMETER, STOP)) {
      victal_check_routine_pack (NEXT (p), x, z);
      victal_check_routine_pack (SUB (p), x, z);
    } else if (IS (p, DECLARER)) {
      *z &= victal_check_declarer (SUB (p), x);
    }
  }
}

/**
@brief Check routine text.
@param p Node in syntax tree.
**/

static void victal_check_routine_text (NODE_T * p)
{
  if (IS (p, PARAMETER_PACK)) {
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

/**
@brief Check structure pack.
@param p Node in syntax tree.
@param x Expected attribute.
@param z Flag.
**/

static void victal_check_structure_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NO_NODE) {
    if (IS (p, STRUCTURE_PACK)) {
      victal_check_structure_pack (SUB (p), x, z);
    } else if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_structure_pack (NEXT (p), x, z);
    } else if (is_one_of (p, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, STOP)) {
      victal_check_structure_pack (NEXT (p), x, z);
      victal_check_structure_pack (SUB (p), x, z);
    } else if (IS (p, DECLARER)) {
      (*z) &= victal_check_declarer (SUB (p), x);
    }
  }
}

/**
@brief Check union pack.
@param p Node in syntax tree.
@param x Expected attribute.
@param z Flag.
**/

static void victal_check_union_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NO_NODE) {
    if (IS (p, UNION_PACK)) {
      victal_check_union_pack (SUB (p), x, z);
    } else if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, VOID_SYMBOL, STOP)) {
      victal_check_union_pack (NEXT (p), x, z);
    } else if (IS (p, UNION_DECLARER_LIST)) {
      victal_check_union_pack (NEXT (p), x, z);
      victal_check_union_pack (SUB (p), x, z);
    } else if (IS (p, DECLARER)) {
      victal_check_union_pack (NEXT (p), x, z);
      (*z) &= victal_check_declarer (SUB (p), FORMAL_DECLARER_MARK);
    }
  }
}

/**
@brief Check declarer.
@param p Node in syntax tree.
@param x Expected attribute.
**/

static BOOL_T victal_check_declarer (NODE_T * p, int x)
{
  if (p == NO_NODE) {
    return (A68_FALSE);
  } else if (IS (p, DECLARER)) {
    return (victal_check_declarer (SUB (p), x));
  } else if (is_one_of (p, LONGETY, SHORTETY, STOP)) {
    return (A68_TRUE);
  } else if (is_one_of (p, VOID_SYMBOL, INDICANT, STANDARD, STOP)) {
    return (A68_TRUE);
  } else if (IS (p, REF_SYMBOL)) {
    return (victal_check_declarer (NEXT (p), VIRTUAL_DECLARER_MARK));
  } else if (IS (p, FLEX_SYMBOL)) {
    return (victal_check_declarer (NEXT (p), x));
  } else if (IS (p, BOUNDS)) {
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
  } else if (IS (p, FORMAL_BOUNDS)) {
    victal_checker (SUB (p));
    if (x == ACTUAL_DECLARER_MARK) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return (A68_TRUE);
    } else {
      return (victal_check_declarer (NEXT (p), x));
    }
  } else if (IS (p, STRUCT_SYMBOL)) {
    BOOL_T z = A68_TRUE;
    victal_check_structure_pack (NEXT (p), x, &z);
    return (z);
  } else if (IS (p, UNION_SYMBOL)) {
    BOOL_T z = A68_TRUE;
    victal_check_union_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer pack");
    }
    return (A68_TRUE);
  } else if (IS (p, PROC_SYMBOL)) {
    if (IS (NEXT (p), FORMAL_DECLARERS)) {
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

/**
@brief Check cast.
@param p Node in syntax tree.
**/

static void victal_check_cast (NODE_T * p)
{
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    victal_checker (NEXT (p));
  }
}

/**
@brief Driver for checking VICTALITY of declarers.
@param p Node in syntax tree.
**/

void victal_checker (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
    } else if (IS (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
    } else if (IS (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
    } else if (IS (p, GENERATOR)) {
      victal_check_generator (SUB (p));
    } else if (IS (p, ROUTINE_TEXT)) {
      victal_check_routine_text (SUB (p));
    } else if (IS (p, OPERATOR_PLAN)) {
      victal_check_operator_dec (SUB (p));
    } else if (IS (p, CAST)) {
      victal_check_cast (SUB (p));
    } else {
      victal_checker (SUB (p));
    }
  }
}

/****************************************************/
/* Mode collection, equivalencing and derived modes */
/****************************************************/

/*************************/
/* Mode service routines */
/*************************/

/**
@brief Reset moid.
@param p Node in syntax tree.
**/

static void reset_moid_tree (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    MOID (p) = NO_MOID;
    reset_moid_tree (SUB (p));
  }
}

/**
@brief Renumber moids.
@param p Moid list.
@param n Index.
**/

void renumber_moids (MOID_T * p, int n)
{
  if (p != NO_MOID) {
    NUMBER (p) = n;
    renumber_moids (NEXT (p), n + 1);
  }
}

/**************************************************/
/* Routines for establishing equivalence of modes */
/* Routines for adding modes                      */
/**************************************************/

/*
After the initial version of the mode equivalencer was made to work (1993), I
found: Algol Bulletin 30.3.3 C.H.A. Koster: On infinite modes, 86-89 [1969],
which essentially concurs with the algorithm on mode equivalence I wrote (and
which is still here). It is basic logic anyway: prove equivalence of things
assuming their equivalence.
*/

/**
@brief Whether packs are equivalent, same sequence of equivalence modes.
@param s Pack 1.
@param t Pack 2.
@return See brief description.
**/

static BOOL_T is_packs_equivalent (PACK_T * s, PACK_T * t)
{
  for (; s != NO_PACK && t != NO_PACK; FORWARD (s), FORWARD (t)) {
    if (!is_modes_equivalent (MOID (s), MOID (t))) {
      return (A68_FALSE);
    }
    if (TEXT (s) != TEXT (t)) {
      return (A68_FALSE);
    }
  }
  return ((BOOL_T) (s == NO_PACK && t == NO_PACK));
}

/**
@brief Whether packs are equivalent, must be subsets.
@param s Pack 1.
@param t Pack 2.
@return See brief description.
**/

static BOOL_T is_united_packs_equivalent (PACK_T * s, PACK_T * t)
{
  PACK_T *p, *q; BOOL_T f;
/* whether s is a subset of t ... */
  for (p = s; p != NO_PACK; FORWARD (p)) {
    for (f = A68_FALSE, q = t; q != NO_PACK && !f; FORWARD (q)) {
      f = is_modes_equivalent (MOID (p), MOID (q));
    }
    if (!f) {
      return (A68_FALSE);
    }
  }
/* ... and whether t is a subset of s */
  for (p = t; p != NO_PACK; FORWARD (p)) {
    for (f = A68_FALSE, q = s; q != NO_PACK && !f; FORWARD (q)) {
      f = is_modes_equivalent (MOID (p), MOID (q));
    }
    if (!f) {
      return (A68_FALSE);
    }
  }
  return (A68_TRUE);
}

/**
@brief Whether moids a and b are structurally equivalent.
@param a Moid.
@param b Moid.
@return See brief description.
**/

BOOL_T is_modes_equivalent (MOID_T * a, MOID_T * b)
{
  if (a == NO_MOID || b == NO_MOID) {
/* Modes can be NO_MOID in partial argument lists */
    return (A68_FALSE);
  } else if (a == b) {
    return (A68_TRUE);
  } else if (a == MODE (ERROR) || b == MODE (ERROR)) {
    return (A68_FALSE);
  } else if (ATTRIBUTE (a) != ATTRIBUTE (b)) {
    return (A68_FALSE);
  } else if (DIM (a) != DIM (b)) {
    return (A68_FALSE);
  } else if (IS (a, STANDARD)) {
    return ((BOOL_T) (a == b));
  } else if (EQUIVALENT (a) == b || EQUIVALENT (b) == a) {
    return (A68_TRUE);
  } else if (is_postulated_pair (top_postulate, a, b) || is_postulated_pair (top_postulate, b, a)) {
    return (A68_TRUE);
  } else if (IS (a, INDICANT)) {
    if (NODE (a) == NO_NODE || NODE (b) == NO_NODE) {
      return (A68_FALSE);
    } else {
      return (NODE (a) == NODE (b));
    }
  }
  switch (ATTRIBUTE (a)) {
    case REF_SYMBOL: 
    case ROW_SYMBOL: 
    case FLEX_SYMBOL: {
      return (is_modes_equivalent (SUB (a), SUB (b)));
    }
  }
  if (IS (a, PROC_SYMBOL) && PACK (a) == NO_PACK && PACK (b) == NO_PACK) {
    return (is_modes_equivalent (SUB (a), SUB (b)));
  } else if (IS (a, STRUCT_SYMBOL)) {
    POSTULATE_T *save; BOOL_T z;
    save = top_postulate;
    make_postulate (&top_postulate, a, b);
    z = is_packs_equivalent (PACK (a), PACK (b));
    free_postulate_list (top_postulate, save);
    top_postulate = save;
    return (z);
  } else if (IS (a, UNION_SYMBOL)) {
    return (is_united_packs_equivalent (PACK (a), PACK (b)));
  } else if (IS (a, PROC_SYMBOL) && PACK (a) != NO_PACK && PACK (b) != NO_PACK) {
    POSTULATE_T *save; BOOL_T z;
    save = top_postulate;
    make_postulate (&top_postulate, a, b);
    z = is_modes_equivalent (SUB (a), SUB (b));
    if (z) {
      z = is_packs_equivalent (PACK (a), PACK (b));
    }
    free_postulate_list (top_postulate, save);
    top_postulate = save;
    return (z);
  } else if (IS (a, SERIES_MODE) || IS (a, STOWED_MODE)) {
    return (is_packs_equivalent (PACK (a), PACK (b)));
  }
  return (A68_FALSE);
}

/**
@brief Whether modes 1 and 2 are structurally equivalent.
@param p Mode 1.
@param q Mode 2.
@return See brief description.
**/

static BOOL_T prove_moid_equivalence (MOID_T * p, MOID_T * q)
{
/* Prove two modes to be equivalent under assumption that they indeed are */
  POSTULATE_T *save = top_postulate;
  BOOL_T z = is_modes_equivalent (p, q);
  free_postulate_list (top_postulate, save);
  top_postulate = save;
  return (z);
}

/**
@brief Register mode in the global mode table, if mode is unique.
@param z Mode table.
@param u Mode to enter.
@return Mode table entry.
**/

static MOID_T *register_extra_mode (MOID_T ** z, MOID_T * u)
{
  MOID_T *head = TOP_MOID (&program);
/* If we already know this mode, return the existing entry; otherwise link it in */
  for (; head != NO_MOID; FORWARD (head)) {
    if (prove_moid_equivalence (head, u)) {
      return (head);
    }
  }
/* Link to chain and exit */
  NUMBER (u) = mode_count++;
  NEXT (u) = (* z);
  return (* z = u);
}

/**
@brief Add mode "sub" to chain "z".
@param z Chain to insert into.
@param att Attribute.
@param dim Dimension.
@param node Node.
@param sub Subordinate mode.
@param pack Pack.
@return New entry.
**/

MOID_T *add_mode (MOID_T ** z, int att, int dim, NODE_T * node, MOID_T * sub, PACK_T * pack)
{
  MOID_T *new_mode = new_moid ();
  ABEND (att == REF_SYMBOL && sub == NO_MOID, ERROR_INTERNAL_CONSISTENCY, "store REF NULL");
  ABEND (att == FLEX_SYMBOL && sub == NO_MOID, ERROR_INTERNAL_CONSISTENCY, "store FLEX NULL");
  ABEND (att == ROW_SYMBOL && sub == NO_MOID, ERROR_INTERNAL_CONSISTENCY, "store [] NULL");
  USE (new_mode) = A68_FALSE;
  SIZE (new_mode) = 0;
  ATTRIBUTE (new_mode) = att;
  DIM (new_mode) = dim;
  NODE (new_mode) = node;
  HAS_ROWS (new_mode) = (BOOL_T) (att == ROW_SYMBOL);
  SUB (new_mode) = sub;
  PACK (new_mode) = pack;
  NEXT (new_mode) = NO_MOID;
  EQUIVALENT (new_mode) = NO_MOID;
  SLICE (new_mode) = NO_MOID;
  DEFLEXED (new_mode) = NO_MOID;
  NAME (new_mode) = NO_MOID;
  MULTIPLE (new_mode) = NO_MOID;
  ROWED (new_mode) = NO_MOID;
  return (register_extra_mode (z, new_mode));
}

/**
@brief Contract a UNION.
@param u United mode.
**/

static void contract_union (MOID_T * u)
{
  PACK_T *s = PACK (u);
  for (; s != NO_PACK; FORWARD (s)) {
    PACK_T *t = s;
    while (t != NO_PACK) {
      if (NEXT (t) != NO_PACK && MOID (NEXT (t)) == MOID (s)) {
        MOID (t) = MOID (t);
        NEXT (t) = NEXT_NEXT (t);
      } else {
        FORWARD (t);
      }
    }
  }
}

/**
@brief Absorb UNION pack.
@param u Pack.
@return Absorbed pack.
**/

static PACK_T *absorb_union_pack (PACK_T * u)
{
  BOOL_T go_on;
  PACK_T *t, *z;
  do {
    z = NO_PACK;
    go_on = A68_FALSE;
    for (t = u; t != NO_PACK; FORWARD (t)) {
      if (IS (MOID (t), UNION_SYMBOL)) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NO_PACK; FORWARD (s)) {
          (void) add_mode_to_pack (&z, MOID (s), NO_TEXT, NODE (s));
        }
      } else {
        (void) add_mode_to_pack (&z, MOID (t), NO_TEXT, NODE (t));
      }
    }
    u = z;
  } while (go_on);
  return (z);
}

/**
@brief Absorb nested series modes recursively.
@param p Mode.
**/

static void absorb_series_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NO_PACK, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NO_PACK; FORWARD (t)) {
      if (MOID (t) != NO_MOID && IS (MOID (t), SERIES_MODE)) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NO_PACK; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NO_TEXT, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NO_TEXT, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

/**
@brief Absorb nested series and united modes recursively.
@param p Mode.
**/

static void absorb_series_union_pack (MOID_T ** p)
{
  BOOL_T go_on;
  do {
    PACK_T *z = NO_PACK, *t;
    go_on = A68_FALSE;
    for (t = PACK (*p); t != NO_PACK; FORWARD (t)) {
      if (MOID (t) != NO_MOID && (IS (MOID (t), SERIES_MODE) || IS (MOID (t), UNION_SYMBOL))) {
        PACK_T *s;
        go_on = A68_TRUE;
        for (s = PACK (MOID (t)); s != NO_PACK; FORWARD (s)) {
          add_mode_to_pack (&z, MOID (s), NO_TEXT, NODE (s));
        }
      } else {
        add_mode_to_pack (&z, MOID (t), NO_TEXT, NODE (t));
      }
    }
    PACK (*p) = z;
  } while (go_on);
}

/**
@brief Make SERIES (u, v).
@param u Mode 1.
@param v Mode 2.
\return SERIES (u, v)
**/

static MOID_T *make_series_from_moids (MOID_T * u, MOID_T * v)
{
  MOID_T *x = new_moid ();
  ATTRIBUTE (x) = SERIES_MODE;
  add_mode_to_pack (&(PACK (x)), u, NO_TEXT, NODE (u));
  add_mode_to_pack (&(PACK (x)), v, NO_TEXT, NODE (v));
  absorb_series_pack (&x);
  DIM (x) = count_pack_members (PACK (x));
  (void) register_extra_mode (&TOP_MOID (&program), x);
  if (DIM (x) == 1) {
    return (MOID (PACK (x)));
  } else {
    return (x);
  }
}

/**
@brief Absorb firmly related unions in mode.
@param m United mode.
@return Absorbed "m".
**/

static MOID_T *absorb_related_subsets (MOID_T * m)
{
/*
For instance invalid UNION (PROC REF UNION (A, B), A, B) -> valid UNION (A, B),
which is used in balancing conformity clauses.
*/
  int mods;
  do {
    PACK_T *u = NO_PACK, *v;
    mods = 0;
    for (v = PACK (m); v != NO_PACK; FORWARD (v)) {
      MOID_T *n = depref_completely (MOID (v));
      if (IS (n, UNION_SYMBOL) && is_subset (n, m, SAFE_DEFLEXING)) {
/* Unpack it */
        PACK_T *w;
        for (w = PACK (n); w != NO_PACK; FORWARD (w)) {
          add_mode_to_pack (&u, MOID (w), NO_TEXT, NODE (w));
        }
        mods++;
      } else {
        add_mode_to_pack (&u, MOID (v), NO_TEXT, NODE (v));
      }
    }
    PACK (m) = absorb_union_pack (u);
  } while (mods != 0);
  return (m);
}

/**
@brief Make united mode, from mode that is a SERIES (..).
@param m Series mode.
@return Mode table entry.
**/

static MOID_T *make_united_mode (MOID_T * m)
{
  MOID_T *u;
  PACK_T *v, *w;
  int mods;
  if (m == NO_MOID) {
    return (MODE (ERROR));
  } else if (ATTRIBUTE (m) != SERIES_MODE) {
    return (m);
  }
/* Do not unite a single UNION */
  if (DIM (m) == 1 && IS (MOID (PACK (m)), UNION_SYMBOL)) {
    return (MOID (PACK (m)));
  }
/* Straighten the series */
  absorb_series_union_pack (&m);
/* Copy the series into a UNION */
  u = new_moid ();
  ATTRIBUTE (u) = UNION_SYMBOL;
  PACK (u) = NO_PACK;
  v = PACK (u);
  w = PACK (m);
  for (w = PACK (m); w != NO_PACK; FORWARD (w)) {
    add_mode_to_pack (&(PACK (u)), MOID (w), NO_TEXT, NODE (m));
  }
/* Absorb and contract the new UNION */
  do {
    mods = 0;
    absorb_series_union_pack (&u);
    DIM (u) = count_pack_members (PACK (u));
    PACK (u) = absorb_union_pack (PACK (u));
    contract_union (u);
  } while (mods != 0);
/* A UNION of one mode is that mode itself */
  if (DIM (u) == 1) {
    return (MOID (PACK (u)));
  } else {
    return (register_extra_mode (&TOP_MOID (&program), u));
  }
}

/**
@brief Add row and its slices to chain, recursively.
@param p Chain to insert into.
@param dim Dimension.
@param sub Mode of slice.
@param n Node in syntax tree.
@param derivate Whether derived, ie. not in the source.
@return Pointer to entry.
**/

static MOID_T *add_row (MOID_T ** p, int dim, MOID_T * sub, NODE_T * n, BOOL_T derivate)
{
  MOID_T * q = add_mode (p, ROW_SYMBOL, dim, n, sub, NO_PACK);
  DERIVATE (q) |= derivate;
  if (dim > 1) {
    SLICE (q) = add_row (&NEXT (q), dim - 1, sub, n, derivate);
  } else {
    SLICE (q) = sub;
  }
  return (q);
}

/**
@brief Add a moid to a pack, maybe with a (field) name.
@param p Pack.
@param m Moid to add.
@param text Field name.
@param node Node in syntax tree.
**/

void add_mode_to_pack (PACK_T ** p, MOID_T * m, char *text, NODE_T * node)
{
  PACK_T *z = new_pack ();
  MOID (z) = m;
  TEXT (z) = text;
  NODE (z) = node;
  NEXT (z) = *p;
  PREVIOUS (z) = NO_PACK;
  if (NEXT (z) != NO_PACK) {
    PREVIOUS (NEXT (z)) = z;
  }
/* Link in chain */
  *p = z;
}

/**
@brief Add a moid to a pack, maybe with a (field) name.
@param p Pack.
@param m Moid to add.
@param text Field name.
@param node Node in syntax tree.
**/

void add_mode_to_pack_end (PACK_T ** p, MOID_T * m, char *text, NODE_T * node)
{
  PACK_T *z = new_pack ();
  MOID (z) = m;
  TEXT (z) = text;
  NODE (z) = node;
  NEXT (z) = NO_PACK;
  if (NEXT (z) != NO_PACK) {
    PREVIOUS (NEXT (z)) = z;
  }
/* Link in chain */
  while ((*p) != NO_PACK) {
    p = &(NEXT (*p));
  }
  PREVIOUS (z) = (*p);
  (*p) = z;
}

/**
@brief Absorb UNION members.
@param m First MOID.
**/

static void absorb_unions (MOID_T * m)
{
/*
UNION (A, UNION (B, C)) = UNION (A, B, C) or
UNION (A, UNION (A, B)) = UNION (A, B).
*/
  for (; m != NO_MOID; FORWARD (m)) {
    if (IS (m, UNION_SYMBOL)) {
      PACK (m) = absorb_union_pack (PACK (m));
    }
  }
}


/**
@brief Contract UNIONs .
@param m First MOID.
**/

static void contract_unions (MOID_T * m)
{
/* UNION (A, B, A) -> UNION (A, B) */
  for (; m != NO_MOID; FORWARD (m)) {
    if (IS (m, UNION_SYMBOL) && EQUIVALENT (m) == NO_MOID) {
      contract_union (m);
    }
  }
}

/***************************************************/
/* Routines to collect MOIDs from the program text */
/***************************************************/

/**
@brief Search standard mode in standard environ.
@param sizety Sizety.
@param indicant Node in syntax tree.
@return Moid entry in standard environ.
**/

static MOID_T *search_standard_mode (int sizety, NODE_T * indicant)
{
  MOID_T *p = TOP_MOID (&program);
/* Search standard mode */
  for (; p != NO_MOID; FORWARD (p)) {
    if (IS (p, STANDARD) && DIM (p) == sizety && NSYMBOL (NODE (p)) == NSYMBOL (indicant)) {
      return (p);
    }
  }
/* Sanity check
  if (sizety == -2 || sizety == 2) {
    return (NO_MOID);
  }
*/
/* Map onto greater precision */
  if (sizety < 0) {
    return (search_standard_mode (sizety + 1, indicant));
  } else if (sizety > 0) {
    return (search_standard_mode (sizety - 1, indicant));
  } else {
    return (NO_MOID);
  }
}

/**
@brief Collect mode from STRUCT field.
@param p Node in syntax tree.
@param u Pack to insert to.
**/

static void get_mode_from_struct_field (NODE_T * p, PACK_T ** u)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTIFIER)) {
        ATTRIBUTE (p) = FIELD_IDENTIFIER;
        (void) add_mode_to_pack (u, NO_MOID, NSYMBOL (p), p);
    } else if (IS (p, DECLARER)) {
        MOID_T *new_one = get_mode_from_declarer (p);
        PACK_T *t;
        get_mode_from_struct_field (NEXT (p), u);
        for (t = *u; t && MOID (t) == NO_MOID; FORWARD (t)) {
          MOID (t) = new_one;
          MOID (NODE (t)) = new_one;
        }
    } else {
        get_mode_from_struct_field (NEXT (p), u);
        get_mode_from_struct_field (SUB (p), u);
    }
  }
}

/**
@brief Collect MODE from formal pack.
@param p Node in syntax tree.
@param u Pack to insert to.
**/

static void get_mode_from_formal_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NO_NODE) {
    if (IS (p, DECLARER)) {
        MOID_T *z;
        get_mode_from_formal_pack (NEXT (p), u);
        z = get_mode_from_declarer (p);
        (void) add_mode_to_pack (u, z, NO_TEXT, p);
     } else {
        get_mode_from_formal_pack (NEXT (p), u);
        get_mode_from_formal_pack (SUB (p), u);
    }
  }
}

/**
@brief Collect MODE or VOID from formal UNION pack.
@param p Node in syntax tree.
@param u Pack to insert to.
**/

static void get_mode_from_union_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NO_NODE) {
    if (IS (p, DECLARER) || IS (p, VOID_SYMBOL)) {
        MOID_T *z;
        get_mode_from_union_pack (NEXT (p), u);
        z = get_mode_from_declarer (p);
        (void) add_mode_to_pack (u, z, NO_TEXT, p);
    } else {
        get_mode_from_union_pack (NEXT (p), u);
        get_mode_from_union_pack (SUB (p), u);
    }
  }
}

/**
@brief Collect mode from PROC, OP pack.
@param p Node in syntax tree.
@param u Pack to insert to.
**/

static void get_mode_from_routine_pack (NODE_T * p, PACK_T ** u)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTIFIER)) {
      (void) add_mode_to_pack (u, NO_MOID, NO_TEXT, p);
    } else if (IS (p, DECLARER)) {
        MOID_T *z = get_mode_from_declarer (p);
        PACK_T *t = *u;
        for (; t != NO_PACK && MOID (t) == NO_MOID; FORWARD (t)) {
          MOID (t) = z;
          MOID (NODE (t)) = z;
        }
        (void) add_mode_to_pack (u, z, NO_TEXT, p);
    } else {
        get_mode_from_routine_pack (NEXT (p), u);
        get_mode_from_routine_pack (SUB (p), u);
    }
  }
}

/**
@brief Collect MODE from DECLARER.
@param p Node in syntax tree.
@return Mode table entry.
**/

static MOID_T *get_mode_from_declarer (NODE_T * p)
{
  if (p == NO_NODE) {
    return (NO_MOID);
  } else {
    if (IS (p, DECLARER)) {
      if (MOID (p) != NO_MOID) {
        return (MOID (p));
      } else {
        return (MOID (p) = get_mode_from_declarer (SUB (p)));
      }
    } else {
      if (IS (p, VOID_SYMBOL)) {
        MOID (p) = MODE (VOID);
        return (MOID (p));
      } else if (IS (p, LONGETY)) {
        if (whether (p, LONGETY, INDICANT, STOP)) {
          int k = count_sizety (SUB (p));
          MOID (p) = search_standard_mode (k, NEXT (p));
          return (MOID (p));
        } else {
          return (NO_MOID);
        }
      } else if (IS (p, SHORTETY)) {
        if (whether (p, SHORTETY, INDICANT, STOP)) {
          int k = count_sizety (SUB (p));
          MOID (p) = search_standard_mode (k, NEXT (p));
          return (MOID (p));
        } else {
          return (NO_MOID);
        }
      } else if (IS (p, INDICANT)) {
        MOID_T *q = search_standard_mode (0, p);
        if (q != NO_MOID) {
          MOID (p) = q;
        } else {
/* Position of definition tells indicants apart */
          TAG_T *y = find_tag_global (TABLE (p), INDICANT, NSYMBOL (p));
          if (y == NO_TAG) {
            diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG_2, NSYMBOL (p));
          } else {
            MOID (p) = add_mode (&TOP_MOID (&program), INDICANT, 0, NODE (y), NO_MOID, NO_PACK);
          }
        }
        return (MOID (p));
      } else if (IS (p, REF_SYMBOL)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, p, new_one, NO_PACK);
        return (MOID (p));
      } else if (IS (p, FLEX_SYMBOL)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (&TOP_MOID (&program), FLEX_SYMBOL, 0, p, new_one, NO_PACK);
        SLICE (MOID (p)) = SLICE (new_one);
        return (MOID (p));
      } else if (IS (p, FORMAL_BOUNDS)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_row (&TOP_MOID (&program), 1 + count_formal_bounds (SUB (p)), new_one, p, A68_FALSE);
        return (MOID (p));
      } else if (IS (p, BOUNDS)) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_row (&TOP_MOID (&program), count_bounds (SUB (p)), new_one, p, A68_FALSE);
        return (MOID (p));
      } else if (IS (p, STRUCT_SYMBOL)) {
        PACK_T *u = NO_PACK;
        get_mode_from_struct_field (NEXT (p), &u);
        MOID (p) = add_mode (&TOP_MOID (&program), STRUCT_SYMBOL, count_pack_members (u), p, NO_MOID, u);
        return (MOID (p));
      } else if (IS (p, UNION_SYMBOL)) {
        PACK_T *u = NO_PACK;
        get_mode_from_union_pack (NEXT (p), &u);
        MOID (p) = add_mode (&TOP_MOID (&program), UNION_SYMBOL, count_pack_members (u), p, NO_MOID, u);
        return (MOID (p));
      } else if (IS (p, PROC_SYMBOL)) {
        NODE_T *save = p;
        PACK_T *u = NO_PACK;
        MOID_T *new_one;
        if (IS (NEXT (p), FORMAL_DECLARERS)) {
          get_mode_from_formal_pack (SUB_NEXT (p), &u);
          FORWARD (p);
        }
        new_one = get_mode_from_declarer (NEXT (p));
        MOID (p) = add_mode (&TOP_MOID (&program), PROC_SYMBOL, count_pack_members (u), save, new_one, u);
        MOID (save) = MOID (p);
        return (MOID (p));
      } else {
        return (NO_MOID);
      }
    }
  }
}

/**
@brief Collect MODEs from a routine-text header.
@param p Node in syntax tree.
@return Mode table entry.
**/

static MOID_T *get_mode_from_routine_text (NODE_T * p)
{
  PACK_T *u = NO_PACK;
  MOID_T *n;
  NODE_T *q = p;
  if (IS (p, PARAMETER_PACK)) {
    get_mode_from_routine_pack (SUB (p), &u);
    FORWARD (p);
  }
  n = get_mode_from_declarer (p);
  return (add_mode (&TOP_MOID (&program), PROC_SYMBOL, count_pack_members (u), q, n, u));
}

/**
@brief Collect modes from operator-plan.
@param p Node in syntax tree.
@return Mode table entry.
**/

static MOID_T *get_mode_from_operator (NODE_T * p)
{
  PACK_T *u = NO_PACK;
  MOID_T *new_one;
  NODE_T *save = p;
  if (IS (NEXT (p), FORMAL_DECLARERS)) {
    get_mode_from_formal_pack (SUB_NEXT (p), &u);
    FORWARD (p);
  }
  new_one = get_mode_from_declarer (NEXT (p));
  MOID (p) = add_mode (&TOP_MOID (&program), PROC_SYMBOL, count_pack_members (u), save, new_one, u);
  return (MOID (p));
}

/**
@brief Collect mode from denotation.
@param p Node in syntax tree.
@param sizety Size of denotation.
@return Mode table entry.
**/

static void get_mode_from_denotation (NODE_T * p, int sizety)
{
  if (p != NO_NODE) {
    if (IS (p, ROW_CHAR_DENOTATION)) {
      if (strlen (NSYMBOL (p)) == 1) {
        MOID (p) = MODE (CHAR);
      } else {
        MOID (p) = MODE (ROW_CHAR);
      }
    } else if (IS (p, TRUE_SYMBOL) || IS (p, FALSE_SYMBOL)) {
      MOID (p) = MODE (BOOL);
    } else if (IS (p, INT_DENOTATION)) {
      if (sizety == 0) {
        MOID (p) = MODE (INT);
      } else if (sizety == 1) {
        MOID (p) = MODE (LONG_INT);
      } else if (sizety == 2) {
        MOID (p) = MODE (LONGLONG_INT);
      } else {
        MOID (p) = (sizety > 0 ? MODE (LONGLONG_INT) : MODE (INT));
      }
    } else if (IS (p, REAL_DENOTATION)) {
      if (sizety == 0) {
        MOID (p) = MODE (REAL);
      } else if (sizety == 1) {
        MOID (p) = MODE (LONG_REAL);
      } else if (sizety == 2) {
        MOID (p) = MODE (LONGLONG_REAL);
      } else {
        MOID (p) = (sizety > 0 ? MODE (LONGLONG_REAL) : MODE (REAL));
      }
    } else if (IS (p, BITS_DENOTATION)) {
      if (sizety == 0) {
        MOID (p) = MODE (BITS);
      } else if (sizety == 1) {
        MOID (p) = MODE (LONG_BITS);
      } else if (sizety == 2) {
        MOID (p) = MODE (LONGLONG_BITS);
      } else {
        MOID (p) = (sizety > 0 ? MODE (LONGLONG_BITS) : MODE (BITS));
      }
    } else if (IS (p, LONGETY) || IS (p, SHORTETY)) {
      get_mode_from_denotation (NEXT (p), count_sizety (SUB (p)));
      MOID (p) = MOID (NEXT (p));
    } else if (IS (p, EMPTY_SYMBOL)) {
      MOID (p) = MODE (VOID);
    }
  }
}

/**
@brief Collect modes from the syntax tree.
@param p Node in syntax tree.
@param attribute
**/

static void get_modes_from_tree (NODE_T * p, int attribute)
{
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, VOID_SYMBOL)) {
      MOID (q) = MODE (VOID);
    } else if (IS (q, DECLARER)) {
      if (attribute == VARIABLE_DECLARATION) {
        MOID_T *new_one = get_mode_from_declarer (q);
        MOID (q) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, new_one, NO_PACK);
      } else {
        MOID (q) = get_mode_from_declarer (q);
      }
    } else if (IS (q, ROUTINE_TEXT)) {
      MOID (q) = get_mode_from_routine_text (SUB (q));
    } else if (IS (q, OPERATOR_PLAN)) {
      MOID (q) = get_mode_from_operator (SUB (q));
    } else if (is_one_of (q, LOC_SYMBOL, HEAP_SYMBOL, NEW_SYMBOL, STOP)) {
      if (attribute == GENERATOR) {
        MOID_T *new_one = get_mode_from_declarer (NEXT (q));
        MOID (NEXT (q)) = new_one;
        MOID (q) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, NO_NODE, new_one, NO_PACK);
      }
    } else {
      if (attribute == DENOTATION) {
        get_mode_from_denotation (q, 0);
      }
    }
  }
  if (attribute != DENOTATION) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      if (SUB (q) != NO_NODE) {
        get_modes_from_tree (SUB (q), ATTRIBUTE (q));
      }
    }
  }
}

/**
@brief Collect modes from proc variables.
@param p Node in syntax tree.
**/

static void get_mode_from_proc_variables (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      get_mode_from_proc_variables (SUB (p));
      get_mode_from_proc_variables (NEXT (p));
    } else if (IS (p, QUALIFIER) || IS (p, PROC_SYMBOL) || IS (p, COMMA_SYMBOL)) {
      get_mode_from_proc_variables (NEXT (p));
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      MOID_T *new_one = MOID (NEXT_NEXT (p));
      MOID (p) = add_mode (&TOP_MOID (&program), REF_SYMBOL, 0, p, new_one, NO_PACK);
    }
  }
}

/**
@brief Collect modes from proc variable declarations.
@param p Node in syntax tree.
**/

static void get_mode_from_proc_var_declarations_tree (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    get_mode_from_proc_var_declarations_tree (SUB (p));
    if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      get_mode_from_proc_variables (p);
    }
  }
}
/**********************************/
/* Various routines to test modes */
/**********************************/

/**
@brief Whether a mode declaration refers to self or relates to void.
@param def Entry of indicant in mode table, NO_MOID if z is an applied mode.
@param z Mode to check.
@param yin Whether shields YIN.
@param yang Whether shields YANG.
@param video Whether shields VOID .
@return See brief description.
**/

static BOOL_T is_well_formed (MOID_T *def, MOID_T * z, BOOL_T yin, BOOL_T yang, BOOL_T video)
{
  if (z == NO_MOID) {
    return (A68_FALSE);
  } else if (yin && yang) {
    return (z == MODE (VOID) ? video : A68_TRUE);
  } else if (z == MODE (VOID)) {
    return (video);
  } else if (IS (z, STANDARD)) {
    return (A68_TRUE);
  } else if (IS (z, INDICANT)) {
    if (def == NO_MOID) {
/* Check an applied indicant for relation to VOID */
      while (z != NO_MOID) {
        z = EQUIVALENT (z);
      }
      if (z == MODE (VOID)) {
        return (video);
      } else {
        return (A68_TRUE);
      }
    } else {
      if (z == def || USE (z)) {
        return (yin && yang);
      } else {
        BOOL_T wwf;
        USE (z) = A68_TRUE;
        wwf = is_well_formed (def, EQUIVALENT (z), yin, yang, video);
        USE (z) = A68_FALSE;
        return (wwf);
      }
    }
  } else if (IS (z, REF_SYMBOL)) {
    return (is_well_formed (def, SUB (z), A68_TRUE, yang, A68_FALSE));
  } else if (IS (z, PROC_SYMBOL)) {
    return (PACK (z) != NO_PACK ? A68_TRUE : is_well_formed (def, SUB (z), A68_TRUE, yang, A68_TRUE));
  } else if (IS (z, ROW_SYMBOL)) {
    return (is_well_formed (def, SUB (z), yin, yang, A68_FALSE));
  } else if (IS (z, FLEX_SYMBOL)) {
    return (is_well_formed (def, SUB (z), yin, yang, A68_FALSE));
  } else if (IS (z, STRUCT_SYMBOL)) {
    PACK_T *s = PACK (z);
    for (; s != NO_PACK; FORWARD (s)) {
      if (! is_well_formed (def, MOID (s), yin, A68_TRUE, A68_FALSE)) {
        return (A68_FALSE);
      }
    }
    return (A68_TRUE);
  } else if (IS (z, UNION_SYMBOL)) {
    PACK_T *s = PACK (z);
    for (; s != NO_PACK; FORWARD (s)) {
      if (! is_well_formed (def, MOID (s), yin, yang, A68_TRUE)) {
        return (A68_FALSE);
      }
    }
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Replace a mode by its equivalent mode (walk chain).
@param q Mode to track.
**/

static void resolve_eq_members (MOID_T * q)
{
  PACK_T *p;
  resolve_equivalent (&SUB (q));
  resolve_equivalent (&DEFLEXED (q));
  resolve_equivalent (&MULTIPLE (q));
  resolve_equivalent (&NAME (q));
  resolve_equivalent (&SLICE (q));
  resolve_equivalent (&TRIM (q));
  resolve_equivalent (&ROWED (q));
  for (p = PACK (q); p != NO_PACK; FORWARD (p)) {
    resolve_equivalent (&MOID (p));
  }
}

/**
@brief Track equivalent tags.
@param z Tag to track.
**/

static void resolve_eq_tags (TAG_T * z)
{
  for (; z != NO_TAG; FORWARD (z)) {
    if (MOID (z) != NO_MOID) {
      resolve_equivalent (& MOID (z));
    }  
  }
}

/**
@brief Bind modes in syntax tree.
@param p Node in syntax tree.
**/

static void bind_modes (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    resolve_equivalent (& MOID (p));
    if (SUB (p) != NO_NODE && is_new_lexical_level (p)) {
      TABLE_T *s = TABLE (SUB (p));
      TAG_T *z = INDICANTS (s);
      for (; z != NO_TAG; FORWARD (z)) {
        if (NODE (z) != NO_NODE) {
          resolve_equivalent (& MOID (NEXT_NEXT (NODE (z))));
          MOID (z) = MOID (NEXT_NEXT (NODE (z)));
          MOID (NODE (z)) = MOID (z);
        }
      }
    } 
    bind_modes (SUB (p));
  }
}

/*
Routines for calculating subordinates for selections, for instance selection
from REF STRUCT (A) yields REF A fields and selection from [] STRUCT (A) yields
[] A fields.
*/

/**
@brief Make name pack.
@param src Source pack.
@param dst Destination pack with REF modes.
@param p Chain to insert new modes into.
**/

static void make_name_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p)
{
  if (src != NO_PACK) {
    MOID_T *z;
    make_name_pack (NEXT (src), dst, p);
    z = add_mode (p, REF_SYMBOL, 0, NO_NODE, MOID (src), NO_PACK);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

/**
@brief Make flex multiple row pack.
@param src Source pack.
@param dst Destination pack with REF modes.
@param p Chain to insert new modes into.
@param dim Dimension.
**/

static void make_flex_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NO_PACK) {
    MOID_T *z;
    make_flex_multiple_row_pack (NEXT (src), dst, p, dim);
    z = add_row (p, dim, MOID (src), NO_NODE, A68_FALSE);
    z = add_mode (p, FLEX_SYMBOL, 0, NO_NODE, z, NO_PACK);
    (void) add_mode_to_pack (dst, z, TEXT (src), NODE (src));
  }
}

/**
@brief Make name struct.
@param m Structured mode.
@param p Chain to insert new modes into.
@return Mode table entry.
**/

static MOID_T *make_name_struct (MOID_T * m, MOID_T ** p)
{
  PACK_T *u = NO_PACK;
  make_name_pack (PACK (m), &u, p);
  return (add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u));
}

/**
@brief Make name row.
@param m Rowed mode.
@param p Chain to insert new modes into.
@return Mode table entry.
**/

static MOID_T *make_name_row (MOID_T * m, MOID_T ** p)
{
  if (SLICE (m) != NO_MOID) {
    return (add_mode (p, REF_SYMBOL, 0, NO_NODE, SLICE (m), NO_PACK));
  } else if (SUB (m) != NO_MOID) {
    return (add_mode (p, REF_SYMBOL, 0, NO_NODE, SUB (m), NO_PACK));
  } else {
    return (NO_MOID); /* weird, FLEX INT or so ... */
  }
}

/**
@brief Make multiple row pack.
@param src Source pack.
@param dst Destination pack with REF modes.
@param p Chain to insert new modes into.
@param dim Dimension.
**/

static void make_multiple_row_pack (PACK_T * src, PACK_T ** dst, MOID_T ** p, int dim)
{
  if (src != NO_PACK) {
    make_multiple_row_pack (NEXT (src), dst, p, dim);
    (void) add_mode_to_pack (dst, add_row (p, dim, MOID (src), NO_NODE, A68_FALSE), TEXT (src), NODE (src));
  }
}

/**
@brief Make flex multiple struct.
@param m Structured mode.
@param p Chain to insert new modes into.
@param dim Dimension.
@return Mode table entry.
**/

static MOID_T *make_flex_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  PACK_T *u = NO_PACK;
  make_flex_multiple_row_pack (PACK (m), &u, p, dim);
  return (add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u));
}

/**
@brief Make multiple struct.
@param m Structured mode.
@param p Chain to insert new modes into.
@param dim Dimension.
@return Mode table entry.
**/

static MOID_T *make_multiple_struct (MOID_T * m, MOID_T ** p, int dim)
{
  PACK_T *u = NO_PACK;
  make_multiple_row_pack (PACK (m), &u, p, dim);
  return (add_mode (p, STRUCT_SYMBOL, DIM (m), NO_NODE, NO_MOID, u));
}

/**
@brief Whether mode has row.
@param m Mode under test.
@return See brief description.
**/

static BOOL_T is_mode_has_row (MOID_T * m)
{
  if (IS (m, STRUCT_SYMBOL) || IS (m, UNION_SYMBOL)) {
    BOOL_T k = A68_FALSE;
    PACK_T *p = PACK (m);
    for (; p != NO_PACK && k == A68_FALSE; FORWARD (p)) {
      HAS_ROWS (MOID (p)) = is_mode_has_row (MOID (p));
      k |= (HAS_ROWS (MOID (p)));
    }
    return (k);
  } else {
    return ((BOOL_T) (HAS_ROWS (m) || IS (m, ROW_SYMBOL) || IS (m, FLEX_SYMBOL)));
  }
}

/**
@brief Compute derived modes.
@param mod Module.
**/

static void compute_derived_modes (MODULE_T *mod)
{
  MOID_T *z;
  int k, len = 0, nlen = 1;
/* UNION things */
  absorb_unions (TOP_MOID (mod));
  contract_unions (TOP_MOID (mod));
/* The for-statement below prevents an endless loop */
  for (k = 1; k <= 10 && len != nlen; k ++) {
/* Make deflexed modes */
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (SUB (z) != NO_MOID) {
        if (IS_REF_FLEX (z) && DEFLEXED (SUB_SUB (z)) != NO_MOID) {
          DEFLEXED (z) = add_mode (&TOP_MOID (mod), REF_SYMBOL, 0, NODE (z), DEFLEXED (SUB_SUB (z)), NO_PACK);
        } else if (IS (z, REF_SYMBOL) && DEFLEXED (SUB (z)) != NO_MOID) {
          DEFLEXED (z) = add_mode (&TOP_MOID (mod), REF_SYMBOL, 0, NODE (z), DEFLEXED (SUB (z)), NO_PACK);
        } else if (IS (z, ROW_SYMBOL) && DEFLEXED (SUB (z)) != NO_MOID) {
          DEFLEXED (z) = add_mode (&TOP_MOID (mod), ROW_SYMBOL, DIM (z), NODE (z), DEFLEXED (SUB (z)), NO_PACK);
        } else if (IS (z, FLEX_SYMBOL) && DEFLEXED (SUB (z)) != NO_MOID) {
          DEFLEXED (z) = DEFLEXED (SUB (z));
        } else if (IS (z, FLEX_SYMBOL)) {
          DEFLEXED (z) = SUB (z);
        } else {
          DEFLEXED (z) = z;
        }
      }
    }
/* Derived modes for stowed modes */
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (NAME (z) == NO_MOID && IS (z, REF_SYMBOL)) {
        if (IS (SUB (z), STRUCT_SYMBOL)) {
          NAME (z) = make_name_struct (SUB (z), &TOP_MOID (mod));
        } else if (IS (SUB (z), ROW_SYMBOL)) {
          NAME (z) = make_name_row (SUB (z), &TOP_MOID (mod));
        } else if (IS (SUB (z), FLEX_SYMBOL) && SUB_SUB (z) != NO_MOID) {
          NAME (z) = make_name_row (SUB_SUB (z), &TOP_MOID (mod));
        }
      }
      if (MULTIPLE (z) != NO_MOID) {
        ;
      } else if (IS (z, REF_SYMBOL)) {
        if (MULTIPLE (SUB (z)) != NO_MOID) {
          MULTIPLE (z) = make_name_struct (MULTIPLE (SUB (z)), &TOP_MOID (mod));
        }
      } else if (IS (z, ROW_SYMBOL)) {
        if (IS (SUB (z), STRUCT_SYMBOL)) {
          MULTIPLE (z) = make_multiple_struct (SUB (z), &TOP_MOID (mod), DIM (z));
        }
      }
    }
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (TRIM (z) == NO_MOID && IS (z, FLEX_SYMBOL)) {
        TRIM (z) = SUB (z);
      }
      if (TRIM (z) == NO_MOID && IS_REF_FLEX (z)) {
        TRIM (z) = add_mode (&TOP_MOID (mod), REF_SYMBOL, 0, NODE (z), SUB_SUB (z), NO_PACK);
      }
    }
/* Fill out stuff for rows, f.i. inverse relations */
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (IS (z, ROW_SYMBOL) && DIM (z) > 0 && SUB (z) != NO_MOID && ! DERIVATE (z)) {
        (void) add_row (&TOP_MOID (mod), DIM (z) + 1, SUB (z), NODE (z), A68_TRUE);
      } else if (IS (z, REF_SYMBOL) && IS (SUB (z), ROW_SYMBOL) && ! DERIVATE (SUB (z))) {
        MOID_T *x = add_row (&TOP_MOID (mod), DIM (SUB (z)) + 1, SUB_SUB (z), NODE (SUB (z)), A68_TRUE);
        MOID_T *y = add_mode (&TOP_MOID (mod), REF_SYMBOL, 0, NODE (z), x, NO_PACK);
        NAME (y) = z;
      }
    }
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (IS (z, ROW_SYMBOL) && SLICE (z) != NO_MOID) {
        ROWED (SLICE (z)) = z;
      }
      if (IS (z, REF_SYMBOL)) {
        MOID_T *y = SUB (z);
        if (SLICE (y) != NO_MOID && IS (SLICE (y), ROW_SYMBOL) && NAME (z) != NO_MOID) {
          ROWED (NAME (z)) = z;
        }
      }
    }
    bind_modes (TOP_NODE (mod));
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (IS (z, INDICANT) && NODE (z) != NO_NODE) {
        EQUIVALENT (z) = MOID (NODE (z));
      }
    }
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      resolve_eq_members (z);
    }
    resolve_eq_tags (INDICANTS (a68g_standenv));
    resolve_eq_tags (IDENTIFIERS (a68g_standenv));
    resolve_eq_tags (OPERATORS (a68g_standenv));
    resolve_equivalent (&MODE (STRING));
    resolve_equivalent (&MODE (COMPLEX));
    resolve_equivalent (&MODE (COMPL));
    resolve_equivalent (&MODE (LONG_COMPLEX));
    resolve_equivalent (&MODE (LONG_COMPL));
    resolve_equivalent (&MODE (LONGLONG_COMPLEX));
    resolve_equivalent (&MODE (LONGLONG_COMPL));
    resolve_equivalent (&MODE (SEMA));
    resolve_equivalent (&MODE (PIPE));
/* UNION members could be resolved */
    absorb_unions (TOP_MOID (mod));
    contract_unions (TOP_MOID (mod));
/* FLEX INDICANT could be resolved */
    for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      if (IS (z, FLEX_SYMBOL) && SUB (z) != NO_MOID) {
        if (SUB_SUB (z) != NO_MOID && IS (SUB_SUB (z), STRUCT_SYMBOL)) {
          MULTIPLE (z) = make_flex_multiple_struct (SUB_SUB (z), &TOP_MOID (mod), DIM (SUB (z)));
        }
      }
    }
/* See what new known modes we have generated by resolving. */
    for (z = TOP_MOID (mod); z != STANDENV_MOID (&program); FORWARD (z)) {
      MOID_T *v;
      for (v = NEXT (z); v != NO_MOID; FORWARD (v)) {
        if (prove_moid_equivalence (z, v)) {
          EQUIVALENT (z) = v;
          EQUIVALENT (v) = NO_MOID;
        }
      } 
    }
/* Count the modes to check self consistency */
    len = nlen;
    for (nlen = 0, z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
      nlen++;
    }
  }
  ABEND (MODE (STRING) != MODE (FLEX_ROW_CHAR), "equivalencing is broken", NO_TEXT);
/* Find out what modes contain rows */
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    HAS_ROWS (z) = is_mode_has_row (z);
  }
/* Check flexible modes */
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, FLEX_SYMBOL) && ISNT (SUB (z), ROW_SYMBOL)) {
      diagnostic_node (A68_ERROR, NODE (z), ERROR_NOT_WELL_FORMED, z);
    }
  }
/* Check on fields in structured modes f.i. STRUCT (REAL x, INT n, REAL x) is wrong */
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, STRUCT_SYMBOL) && EQUIVALENT (z) == NO_MOID) {
      PACK_T *s = PACK (z);
      for (; s != NO_PACK; FORWARD (s)) {
        PACK_T *t = NEXT (s);
        BOOL_T x = A68_TRUE;
        for (t = NEXT (s); t != NO_PACK && x; FORWARD (t)) {
          if (TEXT (s) == TEXT (t)) {
            diagnostic_node (A68_ERROR, NODE (z), ERROR_MULTIPLE_FIELD);
            while (NEXT (s) != NO_PACK && TEXT (NEXT (s)) == TEXT (t)) {
              FORWARD (s);
            }
            x = A68_FALSE;
          }
        }
      }
    }
  }
/* Various union test */
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, UNION_SYMBOL) && EQUIVALENT (z) == NO_MOID) {
      PACK_T *s = PACK (z);
/* Discard unions with one member */
      if (count_pack_members (s) == 1) {
        diagnostic_node (A68_ERROR, NODE (z), ERROR_COMPONENT_NUMBER, z);
      }
/* Discard incestuous unions with firmly related modes */
      for (; s != NO_PACK; FORWARD (s)) {
        PACK_T *t;
        for (t = NEXT (s); t != NO_PACK; FORWARD (t)) {
          if (MOID (t) != MOID (s)) {
            if (is_firm (MOID (s), MOID (t))) {
              diagnostic_node (A68_ERROR, NODE (z), ERROR_COMPONENT_RELATED, z);
            }
          }
        }
      }
/* Discard incestuous unions with firmly related subsets */
      for (s = PACK (z); s != NO_PACK; FORWARD (s)) {
        MOID_T *n = depref_completely (MOID (s));
        if (IS (n, UNION_SYMBOL) && is_subset (n, z, NO_DEFLEXING)) {
          diagnostic_node (A68_ERROR, NODE (z), ERROR_SUBSET_RELATED, z, n);
        }
      }
    }
  }
/* Wrap up and exit */
/* Overwrite old equivalent modes now */
/*
  for (u = &TOP_MOID (mod); (*u) != NO_MOID; u = & NEXT (*u)) {
    while ((*u) != NO_MOID && EQUIVALENT (*u) != NO_MOID) {
      (*u) = NEXT (*u);
    }
  }
*/
  free_postulate_list (top_postulate, NO_POSTULATE);
  top_postulate = NO_POSTULATE;
}

/**
@brief Make list of all modes in the program.
@param mod Module to list modes of.
**/

void make_moid_list (MODULE_T *mod)
{
  MOID_T *z;
/* Collect modes from the syntax tree */
  reset_moid_tree (TOP_NODE (mod));
  get_modes_from_tree (TOP_NODE (mod), STOP);
  get_mode_from_proc_var_declarations_tree (TOP_NODE (mod));
/* Connect indicants to their declarers */
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, INDICANT)) {
      NODE_T *u = NODE (z);
      ABEND (NEXT (u) == NO_NODE, "error in mode table", NO_TEXT);
      ABEND (NEXT_NEXT (u) == NO_NODE, "error in mode table", NO_TEXT);
      ABEND (MOID (NEXT_NEXT (u)) == NO_MOID, "error in mode table", NO_TEXT);
      EQUIVALENT (z) = MOID (NEXT_NEXT (u));
    }
  }
/* Checks on wrong declarations */
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    USE (z) = A68_FALSE;
  }
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    if (IS (z, INDICANT) && EQUIVALENT (z) != NO_MOID) {
      if (!is_well_formed (z, EQUIVALENT (z), A68_FALSE, A68_FALSE, A68_TRUE)) {
        diagnostic_node (A68_ERROR, NODE (z), ERROR_NOT_WELL_FORMED, z);
      }
    } else if (NODE (z) != NO_NODE) {
      if (!is_well_formed (NO_MOID, z, A68_FALSE, A68_FALSE, A68_TRUE)) {
        diagnostic_node (A68_ERROR, NODE (z), ERROR_NOT_WELL_FORMED, z);
      }
    }
  }
  for (z = TOP_MOID (mod); z != NO_MOID; FORWARD (z)) {
    ABEND (USE (z), ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
  }
  if (ERROR_COUNT (mod) != 0) {
    return;
  }
  compute_derived_modes (mod);
  init_postulates ();
}

/****************************************/
/* Symbol table handling, managing TAGS */
/****************************************/

/**
@brief Set level for procedures.
@param p Node in syntax tree.
@param n Proc level number.
**/

void set_proc_level (NODE_T * p, int n)
{
  for (; p != NO_NODE; FORWARD (p)) {
    PROCEDURE_LEVEL (INFO (p)) = n;
    if (IS (p, ROUTINE_TEXT)) {
      set_proc_level (SUB (p), n + 1);
    } else {
      set_proc_level (SUB (p), n);
    }
  }
}

/**
@brief Set nests for diagnostics.
@param p Node in syntax tree.
@param s Start of enclosing nest.
**/

void set_nest (NODE_T * p, NODE_T * s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    NEST (p) = s;
    if (IS (p, PARTICULAR_PROGRAM)) {
      set_nest (SUB (p), p);
    } else if (IS (p, CLOSED_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, COLLATERAL_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, CONDITIONAL_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, CASE_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, CONFORMITY_CLAUSE) && LINE_NUMBER (p) != 0) {
      set_nest (SUB (p), p);
    } else if (IS (p, LOOP_CLAUSE) && LINE_NUMBER (p) != 0) {
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

/**
@brief Find a tag, searching symbol tables towards the root.
@param table Symbol table to search.
@param name Name of tag.
@return Type of tag, identifier or label or ....
**/

int first_tag_global (TABLE_T * table, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s = NO_TAG;
    for (s = IDENTIFIERS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (IDENTIFIER);
      }
    }
    for (s = INDICANTS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (INDICANT);
      }
    }
    for (s = LABELS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (LABEL);
      }
    }
    for (s = OPERATORS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (OP_SYMBOL);
      }
    }
    for (s = PRIO (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (PRIO_SYMBOL);
      }
    }
    return (first_tag_global (PREVIOUS (table), name));
  } else {
    return (STOP);
  }
}

#define PORTCHECK_TAX(p, q) {\
  if (q == A68_FALSE) {\
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_TAG_NOT_PORTABLE);\
  }}

/**
@brief Check portability of sub tree.
@param p Node in syntax tree.
**/

void portcheck (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    portcheck (SUB (p));
    if (OPTION_PORTCHECK (&program)) {
      if (IS (p, INDICANT) && MOID (p) != NO_MOID) {
        PORTCHECK_TAX (p, PORTABLE (MOID (p)));
        PORTABLE (MOID (p)) = A68_TRUE;
      } else if (IS (p, IDENTIFIER)) {
        PORTCHECK_TAX (p, PORTABLE (TAX (p)));
        PORTABLE (TAX (p)) = A68_TRUE;
      } else if (IS (p, OPERATOR)) {
        PORTCHECK_TAX (p, PORTABLE (TAX (p)));
        PORTABLE (TAX (p)) = A68_TRUE;
      }
    }
  }
}

/**
@brief Whether routine can be "lengthety-mapped".
@param z Name of routine.
@return See brief description.
**/

static BOOL_T is_mappable_routine (char *z)
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

/**
@brief Map "short sqrt" onto "sqrt" etcetera.
@param u Name of routine.
@return Tag to map onto.
**/

static TAG_T *bind_lengthety_identifier (char *u)
{
#define CAR(u, v) (strncmp (u, v, strlen(v)) == 0)
/*
We can only map routines blessed by "is_mappable_routine", so there is no
"short print" or "long char in string".
*/
  if (CAR (u, "short")) {
    do {
      char *v;
      TAG_T *w;
      u = &u[strlen ("short")];
      v = TEXT (add_token (&top_token, u));
      w = find_tag_local (a68g_standenv, IDENTIFIER, v);
      if (w != NO_TAG && is_mappable_routine (v)) {
        return (w);
      }
    } while (CAR (u, "short"));
  } else if (CAR (u, "long")) {
    do {
      char *v;
      TAG_T *w;
      u = &u[strlen ("long")];
      v = TEXT (add_token (&top_token, u));
      w = find_tag_local (a68g_standenv, IDENTIFIER, v);
      if (w != NO_TAG && is_mappable_routine (v)) {
        return (w);
      }
    } while (CAR (u, "long"));
  }
  return (NO_TAG);
#undef CAR
}

/**
@brief Bind identifier tags to the symbol table.
@param p Node in syntax tree.
**/

static void bind_identifier_tag_to_symbol_table (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    bind_identifier_tag_to_symbol_table (SUB (p));
    if (is_one_of (p, IDENTIFIER, DEFINING_IDENTIFIER, STOP)) {
      int att = first_tag_global (TABLE (p), NSYMBOL (p));
      TAG_T *z;
      if (att == STOP) {
        if ((z = bind_lengthety_identifier (NSYMBOL (p))) != NO_TAG) {
          MOID (p) = MOID (z);
/*
        } else {
          diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          z = add_tag (TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
          MOID (p) = MODE (ERROR);
*/
        }
        TAX (p) = z;
      } else {
        z = find_tag_global (TABLE (p), att, NSYMBOL (p));
        if (att == IDENTIFIER && z != NO_TAG) {
          MOID (p) = MOID (z);
        } else if (att == LABEL && z != NO_TAG) {
          ;
        } else if ((z = bind_lengthety_identifier (NSYMBOL (p))) != NO_TAG) {
          MOID (p) = MOID (z);
        } else {
          diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          z = add_tag (TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
          MOID (p) = MODE (ERROR);
        }
        TAX (p) = z;
        if (IS (p, DEFINING_IDENTIFIER)) {
          NODE (z) = p;
        }
      }
    }
  }
}

/**
@brief Bind indicant tags to the symbol table.
@param p Node in syntax tree.
**/

static void bind_indicant_tag_to_symbol_table (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    bind_indicant_tag_to_symbol_table (SUB (p));
    if (is_one_of (p, INDICANT, DEFINING_INDICANT, STOP)) {
      TAG_T *z = find_tag_global (TABLE (p), INDICANT, NSYMBOL (p));
      if (z != NO_TAG) {
        MOID (p) = MOID (z);
        TAX (p) = z;
        if (IS (p, DEFINING_INDICANT)) {
          NODE (z) = p;
        }
      }
    }
  }
}

/**
@brief Enter specifier identifiers in the symbol table.
@param p Node in syntax tree.
**/

static void tax_specifiers (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_specifiers (SUB (p));
    if (SUB (p) != NO_NODE && IS (p, SPECIFIER)) {
      tax_specifier_list (SUB (p));
    }
  }
}

/**
@brief Enter specifier identifiers in the symbol table.
@param p Node in syntax tree.
**/

static void tax_specifier_list (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, OPEN_SYMBOL)) {
      tax_specifier_list (NEXT (p));
    } else if (is_one_of (p, CLOSE_SYMBOL, VOID_SYMBOL, STOP)) {
      ;
    } else if (IS (p, IDENTIFIER)) {
      TAG_T *z = add_tag (TABLE (p), IDENTIFIER, p, NO_MOID, SPECIFIER_IDENTIFIER);
      HEAP (z) = LOC_SYMBOL;
    } else if (IS (p, DECLARER)) {
      tax_specifiers (SUB (p));
      tax_specifier_list (NEXT (p));
/* last identifier entry is identifier with this declarer */
      if (IDENTIFIERS (TABLE (p)) != NO_TAG && PRIO (IDENTIFIERS (TABLE (p))) == SPECIFIER_IDENTIFIER)
        MOID (IDENTIFIERS (TABLE (p))) = MOID (p);
    }
  }
}

/**
@brief Enter parameter identifiers in the symbol table.
@param p Node in syntax tree.
**/

static void tax_parameters (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE) {
      tax_parameters (SUB (p));
      if (IS (p, PARAMETER_PACK)) {
        tax_parameter_list (SUB (p));
      }
    }
  }
}

/**
@brief Enter parameter identifiers in the symbol table.
@param p Node in syntax tree.
**/

static void tax_parameter_list (NODE_T * p)
{
  if (p != NO_NODE) {
    if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_parameter_list (NEXT (p));
    } else if (IS (p, CLOSE_SYMBOL)) {
      ;
    } else if (is_one_of (p, PARAMETER_LIST, PARAMETER, STOP)) {
      tax_parameter_list (NEXT (p));
      tax_parameter_list (SUB (p));
    } else if (IS (p, IDENTIFIER)) {
/* parameters are always local */
      HEAP (add_tag (TABLE (p), IDENTIFIER, p, NO_MOID, PARAMETER_IDENTIFIER)) = LOC_SYMBOL;
    } else if (IS (p, DECLARER)) {
      TAG_T *s;
      tax_parameter_list (NEXT (p));
/* last identifier entries are identifiers with this declarer */
      for (s = IDENTIFIERS (TABLE (p)); s != NO_TAG && MOID (s) == NO_MOID; FORWARD (s)) {
        MOID (s) = MOID (p);
      }
      tax_parameters (SUB (p));
    }
  }
}

/**
@brief Enter FOR identifiers in the symbol table.
@param p Node in syntax tree.
**/

static void tax_for_identifiers (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_for_identifiers (SUB (p));
    if (IS (p, FOR_SYMBOL)) {
      if ((FORWARD (p)) != NO_NODE) {
        (void) add_tag (TABLE (p), IDENTIFIER, p, MODE (INT), LOOP_IDENTIFIER);
      }
    }
  }
}

/**
@brief Enter routine texts in the symbol table.
@param p Node in syntax tree.
**/

static void tax_routine_texts (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_routine_texts (SUB (p));
    if (IS (p, ROUTINE_TEXT)) {
      TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, MOID (p), ROUTINE_TEXT);
      TAX (p) = z;
      HEAP (z) = LOC_SYMBOL;
      USE (z) = A68_TRUE;
    }
  }
}

/**
@brief Enter format texts in the symbol table.
@param p Node in syntax tree.
**/

static void tax_format_texts (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_format_texts (SUB (p));
    if (IS (p, FORMAT_TEXT)) {
      TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, MODE (FORMAT), FORMAT_TEXT);
      TAX (p) = z;
      USE (z) = A68_TRUE;
    } else if (IS (p, FORMAT_DELIMITER_SYMBOL) && NEXT (p) != NO_NODE) {
      TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, MODE (FORMAT), FORMAT_IDENTIFIER);
      TAX (p) = z;
      USE (z) = A68_TRUE;
    }
  }
}

/**
@brief Enter FORMAT pictures in the symbol table.
@param p Node in syntax tree.
**/

static void tax_pictures (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_pictures (SUB (p));
    if (IS (p, PICTURE)) {
      TAX (p) = add_tag (TABLE (p), ANONYMOUS, p, MODE (COLLITEM), FORMAT_IDENTIFIER);
    }
  }
}

/**
@brief Enter generators in the symbol table.
@param p Node in syntax tree.
**/

static void tax_generators (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    tax_generators (SUB (p));
    if (IS (p, GENERATOR)) {
      if (IS (SUB (p), LOC_SYMBOL)) {
        TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, SUB_MOID (SUB (p)), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        TAX (p) = z;
      }
    }
  }
}

/**
@brief Find a firmly related operator for operands.
@param c Symbol table.
@param n Name of operator.
@param l Left operand mode.
@param r Right operand mode.
@param self Own tag of "n", as to not relate to itself.
@return Pointer to entry in table.
**/

static TAG_T *find_firmly_related_op (TABLE_T * c, char *n, MOID_T * l, MOID_T * r, TAG_T * self)
{
  if (c != NO_TABLE) {
    TAG_T *s = OPERATORS (c);
    for (; s != NO_TAG; FORWARD (s)) {
      if (s != self && NSYMBOL (NODE (s)) == n) {
        PACK_T *t = PACK (MOID (s));
        if (t != NO_PACK && is_firm (MOID (t), l)) {
/* catch monadic operator */
          if ((FORWARD (t)) == NO_PACK) {
            if (r == NO_MOID) {
              return (s);
            }
          } else {
/* catch dyadic operator */
            if (r != NO_MOID && is_firm (MOID (t), r)) {
              return (s);
            }
          }
        }
      }
    }
  }
  return (NO_TAG);
}

/**
@brief Check for firmly related operators in this range.
@param p Node in syntax tree.
@param s Operator tag to start from.
**/

static void test_firmly_related_ops_local (NODE_T * p, TAG_T * s)
{
  if (s != NO_TAG) {
    PACK_T *u = PACK (MOID (s));
    MOID_T *l = MOID (u);
    MOID_T *r = (NEXT (u) != NO_PACK ? MOID (NEXT (u)) : NO_MOID);
    TAG_T *t = find_firmly_related_op (TAG_TABLE (s), NSYMBOL (NODE (s)), l, r, s);
    if (t != NO_TAG) {
      if (TAG_TABLE (t) == a68g_standenv) {
        diagnostic_node (A68_ERROR, p, ERROR_OPERATOR_RELATED, MOID (s), NSYMBOL (NODE (s)), MOID (t), NSYMBOL (NODE (t)));
        ABEND (A68_TRUE, "standard environ error", NO_TEXT);
      } else {
        diagnostic_node (A68_ERROR, p, ERROR_OPERATOR_RELATED, MOID (s), NSYMBOL (NODE (s)), MOID (t), NSYMBOL (NODE (t)));
      }
    }
    if (NEXT (s) != NO_TAG) {
      test_firmly_related_ops_local ((p == NO_NODE ? NO_NODE : NODE (NEXT (s))), NEXT (s));
    }
  }
}

/**
@brief Find firmly related operators in this program.
@param p Node in syntax tree.
**/

static void test_firmly_related_ops (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE && is_new_lexical_level (p)) {
      TAG_T *oops = OPERATORS (TABLE (SUB (p)));
      if (oops != NO_TAG) {
        test_firmly_related_ops_local (NODE (oops), oops);
      }
    }
    test_firmly_related_ops (SUB (p));
  }
}

/**
@brief Driver for the processing of TAXes.
@param p Node in syntax tree.
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
  test_firmly_related_ops (p);
  test_firmly_related_ops_local (NO_NODE, OPERATORS (a68g_standenv));
}

/**
@brief Whether tag has already been declared in this range.
@param n Name of tag.
@param a Attribute of tag.
**/

static void already_declared (NODE_T * n, int a)
{
  if (find_tag_local (TABLE (n), a, NSYMBOL (n)) != NO_TAG) {
    diagnostic_node (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
}

/**
@brief Whether tag has already been declared in this range.
@param n Name of tag.
@param a Attribute of tag.
**/

static void already_declared_hidden (NODE_T * n, int a)
{
  TAG_T *s;
  if (find_tag_local (TABLE (n), a, NSYMBOL (n)) != NO_TAG) {
    diagnostic_node (A68_ERROR, n, ERROR_MULTIPLE_TAG);
  }
  if ((s = find_tag_global (PREVIOUS (TABLE (n)), a, NSYMBOL (n))) != NO_TAG) {
    if (TAG_TABLE (s) == a68g_standenv) {
      diagnostic_node (A68_WARNING, n, WARNING_HIDES_PRELUDE, MOID (s), NSYMBOL (n));
    } else {
      diagnostic_node (A68_WARNING, n, WARNING_HIDES, NSYMBOL (n));
    }
  }
}

/**
@brief Add tag to local symbol table.
@param s Table where to insert.
@param a Attribute.
@param n Name of tag.
@param m Mode of tag.
@param p Node in syntax tree.
@return Entry in symbol table.
**/

TAG_T *add_tag (TABLE_T * s, int a, NODE_T * n, MOID_T * m, int p)
{
#define INSERT_TAG(l, n) {NEXT (n) = *(l); *(l) = (n);}
  if (s != NO_TABLE) {
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
        INSERT_TAG (&IDENTIFIERS (s), z);
        break;
      }
    case INDICANT:{
        already_declared_hidden (n, INDICANT);
        already_declared (n, OP_SYMBOL);
        already_declared (n, PRIO_SYMBOL);
        INSERT_TAG (&INDICANTS (s), z);
        break;
      }
    case LABEL:{
        already_declared_hidden (n, LABEL);
        already_declared_hidden (n, IDENTIFIER);
        INSERT_TAG (&LABELS (s), z);
        break;
      }
    case OP_SYMBOL:{
        already_declared (n, INDICANT);
        INSERT_TAG (&OPERATORS (s), z);
        break;
      }
    case PRIO_SYMBOL:{
        already_declared (n, PRIO_SYMBOL);
        already_declared (n, INDICANT);
        INSERT_TAG (&PRIO (s), z);
        break;
      }
    case ANONYMOUS:{
        INSERT_TAG (&ANONYMOUS (s), z);
        break;
      }
    default:{
        ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, "add tag");
      }
    }
    return (z);
  } else {
    return (NO_TAG);
  }
}

/**
@brief Find a tag, searching symbol tables towards the root.
@param table Symbol table to search.
@param a Attribute of tag.
@param name Name of tag.
@return Entry in symbol table.
**/

TAG_T *find_tag_global (TABLE_T * table, int a, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s = NO_TAG;
    switch (a) {
    case IDENTIFIER:{
        s = IDENTIFIERS (table);
        break;
      }
    case INDICANT:{
        s = INDICANTS (table);
        break;
      }
    case LABEL:{
        s = LABELS (table);
        break;
      }
    case OP_SYMBOL:{
        s = OPERATORS (table);
        break;
      }
    case PRIO_SYMBOL:{
        s = PRIO (table);
        break;
      }
    default:{
        ABEND (A68_TRUE, "impossible state in find_tag_global", NO_TEXT);
        break;
      }
    }
    for (; s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (s);
      }
    }
    return (find_tag_global (PREVIOUS (table), a, name));
  } else {
    return (NO_TAG);
  }
}

/**
@brief Whether identifier or label global.
@param table Symbol table to search.
@param name Name of tag.
@return Attribute of tag.
**/

int is_identifier_or_label_global (TABLE_T * table, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s;
    for (s = IDENTIFIERS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (IDENTIFIER);
      }
    }
    for (s = LABELS (table); s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (LABEL);
      }
    }
    return (is_identifier_or_label_global (PREVIOUS (table), name));
  } else {
    return (0);
  }
}

/**
@brief Find a tag, searching only local symbol table.
@param table Symbol table to search.
@param a Attribute of tag.
@param name Name of tag.
@return Entry in symbol table.
**/

TAG_T *find_tag_local (TABLE_T * table, int a, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s = NO_TAG;
    if (a == OP_SYMBOL) {
      s = OPERATORS (table);
    } else if (a == PRIO_SYMBOL) {
      s = PRIO (table);
    } else if (a == IDENTIFIER) {
      s = IDENTIFIERS (table);
    } else if (a == INDICANT) {
      s = INDICANTS (table);
    } else if (a == LABEL) {
      s = LABELS (table);
    } else {
      ABEND (A68_TRUE, "impossible state in find_tag_local", NO_TEXT);
    }
    for (; s != NO_TAG; FORWARD (s)) {
      if (NSYMBOL (NODE (s)) == name) {
        return (s);
      }
    }
  }
  return (NO_TAG);
}

/**
@brief Whether context specifies HEAP or LOC for an identifier.
@param p Node in syntax tree.
@return Attribute of generator.
**/

static int tab_qualifier (NODE_T * p)
{
  if (p != NO_NODE) {
    if (is_one_of (p, UNIT, ASSIGNATION, TERTIARY, SECONDARY, GENERATOR, STOP)) {
      return (tab_qualifier (SUB (p)));
    } else if (is_one_of (p, LOC_SYMBOL, HEAP_SYMBOL, NEW_SYMBOL, STOP)) {
      return (ATTRIBUTE (p) == LOC_SYMBOL ? LOC_SYMBOL : HEAP_SYMBOL);
    } else {
      return (LOC_SYMBOL);
    }
  } else {
    return (LOC_SYMBOL);
  }
}

/**
@brief Enter identity declarations in the symbol table.
@param p Node in syntax tree.
@param m Mode of identifiers to enter (picked from the left-most one in fi. INT i = 1, j = 2).
**/

static void tax_identity_dec (NODE_T * p, MOID_T ** m)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTITY_DECLARATION)) {
      tax_identity_dec (SUB (p), m);
      tax_identity_dec (NEXT (p), m);
    } else if (IS (p, DECLARER)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_identity_dec (NEXT (p), m);
    } else if (IS (p, COMMA_SYMBOL)) {
      tax_identity_dec (NEXT (p), m);
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (TABLE (p), IDENTIFIER, NSYMBOL (p));
      MOID (p) = *m;
      HEAP (entry) = LOC_SYMBOL;
      TAX (p) = entry;
      MOID (entry) = *m;
      if (ATTRIBUTE (*m) == REF_SYMBOL) {
        HEAP (entry) = tab_qualifier (NEXT_NEXT (p));
      }
      tax_identity_dec (NEXT_NEXT (p), m);
    } else {
      tax_tags (p);
    }
  }
}

/**
@brief Enter variable declarations in the symbol table.
@param p Node in syntax tree.
@param q Qualifier of generator (HEAP, LOC) picked from left-most identifier.
@param m Mode of identifiers to enter (picked from the left-most one in fi. INT i, j, k).
**/

static void tax_variable_dec (NODE_T * p, int *q, MOID_T ** m)
{
  if (p != NO_NODE) {
    if (IS (p, VARIABLE_DECLARATION)) {
      tax_variable_dec (SUB (p), q, m);
      tax_variable_dec (NEXT (p), q, m);
    } else if (IS (p, DECLARER)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_variable_dec (NEXT (p), q, m);
    } else if (IS (p, QUALIFIER)) {
      *q = ATTRIBUTE (SUB (p));
      tax_variable_dec (NEXT (p), q, m);
    } else if (IS (p, COMMA_SYMBOL)) {
      tax_variable_dec (NEXT (p), q, m);
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (TABLE (p), IDENTIFIER, NSYMBOL (p));
      MOID (p) = *m;
      TAX (p) = entry;
      HEAP (entry) = *q;
      if (*q == LOC_SYMBOL) {
        TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, SUB (*m), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        BODY (entry) = z;
      } else {
        BODY (entry) = NO_TAG;
      }
      MOID (entry) = *m;
      tax_variable_dec (NEXT (p), q, m);
    } else {
      tax_tags (p);
    }
  }
}

/**
@brief Enter procedure variable declarations in the symbol table.
@param p Node in syntax tree.
@param q Qualifier of generator (HEAP, LOC) picked from left-most identifier.
**/

static void tax_proc_variable_dec (NODE_T * p, int *q)
{
  if (p != NO_NODE) {
    if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      tax_proc_variable_dec (SUB (p), q);
      tax_proc_variable_dec (NEXT (p), q);
    } else if (IS (p, QUALIFIER)) {
      *q = ATTRIBUTE (SUB (p));
      tax_proc_variable_dec (NEXT (p), q);
    } else if (is_one_of (p, PROC_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_proc_variable_dec (NEXT (p), q);
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (TABLE (p), IDENTIFIER, NSYMBOL (p));
      TAX (p) = entry;
      HEAP (entry) = *q;
      MOID (entry) = MOID (p);
      if (*q == LOC_SYMBOL) {
        TAG_T *z = add_tag (TABLE (p), ANONYMOUS, p, SUB_MOID (p), GENERATOR);
        HEAP (z) = LOC_SYMBOL;
        USE (z) = A68_TRUE;
        BODY (entry) = z;
      } else {
        BODY (entry) = NO_TAG;
      }
      tax_proc_variable_dec (NEXT (p), q);
    } else {
      tax_tags (p);
    }
  }
}

/**
@brief Enter procedure declarations in the symbol table.
@param p Node in syntax tree.
**/

static void tax_proc_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, PROCEDURE_DECLARATION)) {
      tax_proc_dec (SUB (p));
      tax_proc_dec (NEXT (p));
    } else if (is_one_of (p, PROC_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_proc_dec (NEXT (p));
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      TAG_T *entry = find_tag_local (TABLE (p), IDENTIFIER, NSYMBOL (p));
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

/**
@brief Check validity of operator declaration.
@param p Node in syntax tree.
@param u Moid for a operator-plan.
**/

static void check_operator_dec (NODE_T * p, MOID_T *u)
{
  int k = 0;
  if (u == NO_MOID) {
    NODE_T *pack = SUB_SUB (NEXT_NEXT (p)); /* Where the parameter pack is */
    if (ATTRIBUTE (NEXT_NEXT (p)) != ROUTINE_TEXT) {
      pack = SUB (pack);
    }
    k = 1 + count_operands (pack);
  } else {
    k = count_pack_members (PACK (u));
  }
  if (k < 1 && k > 2) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERAND_NUMBER);
    k = 0;
  }
  if (k == 1 && a68g_strchr (NOMADS, NSYMBOL (p)[0]) != NO_TEXT) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
  } else if (k == 2 && !find_tag_global (TABLE (p), PRIO_SYMBOL, NSYMBOL (p))) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_DYADIC_PRIORITY);
  }
}

/**
@brief Enter operator declarations in the symbol table.
@param p Node in syntax tree.
@param m Mode of operators to enter (picked from the left-most one).
**/

static void tax_op_dec (NODE_T * p, MOID_T ** m)
{
  if (p != NO_NODE) {
    if (IS (p, OPERATOR_DECLARATION)) {
      tax_op_dec (SUB (p), m);
      tax_op_dec (NEXT (p), m);
    } else if (IS (p, OPERATOR_PLAN)) {
      tax_tags (SUB (p));
      *m = MOID (p);
      tax_op_dec (NEXT (p), m);
    } else if (IS (p, OP_SYMBOL)) {
      tax_op_dec (NEXT (p), m);
    } else if (IS (p, COMMA_SYMBOL)) {
      tax_op_dec (NEXT (p), m);
    } else if (IS (p, DEFINING_OPERATOR)) {
      TAG_T *entry = OPERATORS (TABLE (p));
      check_operator_dec (p, *m);
      while (entry != NO_TAG && NODE (entry) != p) {
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

/**
@brief Enter brief operator declarations in the symbol table.
@param p Node in syntax tree.
**/

static void tax_brief_op_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, BRIEF_OPERATOR_DECLARATION)) {
      tax_brief_op_dec (SUB (p));
      tax_brief_op_dec (NEXT (p));
    } else if (is_one_of (p, OP_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_brief_op_dec (NEXT (p));
    } else if (IS (p, DEFINING_OPERATOR)) {
      TAG_T *entry = OPERATORS (TABLE (p));
      MOID_T *m = MOID (NEXT_NEXT (p));
      check_operator_dec (p, NO_MOID);
      while (entry != NO_TAG && NODE (entry) != p) {
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

/**
@brief Enter priority declarations in the symbol table.
@param p Node in syntax tree.
**/

static void tax_prio_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, PRIORITY_DECLARATION)) {
      tax_prio_dec (SUB (p));
      tax_prio_dec (NEXT (p));
    } else if (is_one_of (p, PRIO_SYMBOL, COMMA_SYMBOL, STOP)) {
      tax_prio_dec (NEXT (p));
    } else if (IS (p, DEFINING_OPERATOR)) {
      TAG_T *entry = PRIO (TABLE (p));
      while (entry != NO_TAG && NODE (entry) != p) {
        FORWARD (entry);
      }
      MOID (p) = NO_MOID;
      TAX (p) = entry;
      HEAP (entry) = LOC_SYMBOL;
      tax_prio_dec (NEXT (p));
    } else {
      tax_tags (p);
    }
  }
}

/**
@brief Enter TAXes in the symbol table.
@param p Node in syntax tree.
**/

static void tax_tags (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    int heap = LOC_SYMBOL;
    MOID_T *m = NO_MOID;
    if (IS (p, IDENTITY_DECLARATION)) {
      tax_identity_dec (p, &m);
    } else if (IS (p, VARIABLE_DECLARATION)) {
      tax_variable_dec (p, &heap, &m);
    } else if (IS (p, PROCEDURE_DECLARATION)) {
      tax_proc_dec (p);
    } else if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      tax_proc_variable_dec (p, &heap);
    } else if (IS (p, OPERATOR_DECLARATION)) {
      tax_op_dec (p, &m);
    } else if (IS (p, BRIEF_OPERATOR_DECLARATION)) {
      tax_brief_op_dec (p);
    } else if (IS (p, PRIORITY_DECLARATION)) {
      tax_prio_dec (p);
    } else {
      tax_tags (SUB (p));
    }
  }
}

/**
@brief Reset symbol table nest count.
@param p Node in syntax tree.
**/

void reset_symbol_table_nest_count (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE && is_new_lexical_level (p)) {
      NEST (TABLE (SUB (p))) = symbol_table_count++;
    }
    reset_symbol_table_nest_count (SUB (p));
  }
}

/**
@brief Bind routines in symbol table to the tree.
@param p Node in syntax tree.
**/

void bind_routine_tags_to_tree (NODE_T * p)
{
/* By inserting coercions etc. some may have shifted */
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ROUTINE_TEXT) && TAX (p) != NO_TAG) {
      NODE (TAX (p)) = p;
    }
    bind_routine_tags_to_tree (SUB (p));
  }
}

/**
@brief Bind formats in symbol table to tree.
@param p Node in syntax tree.
**/

void bind_format_tags_to_tree (NODE_T * p)
{
/* By inserting coercions etc. some may have shifted */
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FORMAT_TEXT) && TAX (p) != NO_TAG) {
      NODE (TAX (p)) = p;
    } else if (IS (p, FORMAT_DELIMITER_SYMBOL) && NEXT (p) != NO_NODE && TAX (p) != NO_TAG) {
      NODE (TAX (p)) = p;
    }
    bind_format_tags_to_tree (SUB (p));
  }
}

/**
@brief Fill outer level of symbol table.
@param p Node in syntax tree.
@param s Parent symbol table.
**/

void fill_symbol_table_outer (NODE_T * p, TABLE_T * s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (TABLE (p) != NO_TABLE) {
      OUTER (TABLE (p)) = s;
    }
    if (SUB (p) != NO_NODE && IS (p, ROUTINE_TEXT)) {
      fill_symbol_table_outer (SUB (p), TABLE (SUB (p)));
    } else if (SUB (p) != NO_NODE && IS (p, FORMAT_TEXT)) {
      fill_symbol_table_outer (SUB (p), TABLE (SUB (p)));
    } else {
      fill_symbol_table_outer (SUB (p), s);
    }
  }
}

/**
@brief Flood branch in tree with local symbol table "s".
@param p Node in syntax tree.
@param s Parent symbol table.
**/

static void flood_with_symbol_table_restricted (NODE_T * p, TABLE_T * s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    TABLE (p) = s;
    if (ATTRIBUTE (p) != ROUTINE_TEXT && ATTRIBUTE (p) != SPECIFIED_UNIT) {
      if (is_new_lexical_level (p)) {
        PREVIOUS (TABLE (SUB (p))) = s;
      } else {
        flood_with_symbol_table_restricted (SUB (p), s);
      }
    }
  }
}

/**
@brief Final structure of symbol table after parsing.
@param p Node in syntax tree.
@param l Current lexical level.
**/

void finalise_symbol_table_setup (NODE_T * p, int l)
{
  TABLE_T *s = TABLE (p);
  NODE_T *q = p;
  while (q != NO_NODE) {
/* routine texts are ranges */
    if (IS (q, ROUTINE_TEXT)) {
      flood_with_symbol_table_restricted (SUB (q), new_symbol_table (s));
    }
/* specifiers are ranges */
    else if (IS (q, SPECIFIED_UNIT)) {
      flood_with_symbol_table_restricted (SUB (q), new_symbol_table (s));
    }
/* level count and recursion */
    if (SUB (q) != NO_NODE) {
      if (is_new_lexical_level (q)) {
        LEX_LEVEL (SUB (q)) = l + 1;
        PREVIOUS (TABLE (SUB (q))) = s;
        finalise_symbol_table_setup (SUB (q), l + 1);
        if (IS (q, WHILE_PART)) {
/* This was a bug that went unnoticed for 15 years! */
          TABLE_T *s2 = TABLE (SUB (q));
          if ((FORWARD (q)) == NO_NODE) {
            return;
          }
          if (IS (q, ALT_DO_PART)) {
            PREVIOUS (TABLE (SUB (q))) = s2;
            LEX_LEVEL (SUB (q)) = l + 2;
            finalise_symbol_table_setup (SUB (q), l + 2);
          }
        }
      } else {
        TABLE (SUB (q)) = s;
        finalise_symbol_table_setup (SUB (q), l);
      }
    }
    TABLE (q) = s;
    if (IS (q, FOR_SYMBOL)) {
      FORWARD (q);
    }
    FORWARD (q);
  }
/* FOR identifiers are in the DO ... OD range */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    if (IS (q, FOR_SYMBOL)) {
      TABLE (NEXT (q)) = TABLE (SEQUENCE (NEXT (q)));
    }
  }
}

/**
@brief First structure of symbol table for parsing.
@param p Node in syntax tree.
**/

void preliminary_symbol_table_setup (NODE_T * p)
{
  NODE_T *q;
  TABLE_T *s = TABLE (p);
  BOOL_T not_a_for_range = A68_FALSE;
/* let the tree point to the current symbol table */
  for (q = p; q != NO_NODE; FORWARD (q)) {
    TABLE (q) = s;
  }
/* insert new tables when required */
  for (q = p; q != NO_NODE && !not_a_for_range; FORWARD (q)) {
    if (SUB (q) != NO_NODE) {
/* BEGIN ... END, CODE ... EDOC, DEF ... FED, DO ... OD, $ ... $, { ... } are ranges */
      if (is_one_of (q, BEGIN_SYMBOL, DO_SYMBOL, ALT_DO_SYMBOL, FORMAT_DELIMITER_SYMBOL, ACCO_SYMBOL, STOP)) {
        TABLE (SUB (q)) = new_symbol_table (s);
        preliminary_symbol_table_setup (SUB (q));
      }
/* ( ... ) is a range */
      else if (IS (q, OPEN_SYMBOL)) {
        if (whether (q, OPEN_SYMBOL, THEN_BAR_SYMBOL, STOP)) {
          TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NO_NODE) {
            not_a_for_range = A68_TRUE;
          } else {
            if (IS (q, THEN_BAR_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (IS (q, OPEN_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
/* don't worry about STRUCT (...), UNION (...), PROC (...) yet */
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* IF ... THEN ... ELSE ... FI are ranges */
      else if (IS (q, IF_SYMBOL)) {
        if (whether (q, IF_SYMBOL, THEN_SYMBOL, STOP)) {
          TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NO_NODE) {
            not_a_for_range = A68_TRUE;
          } else {
            if (IS (q, ELSE_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (IS (q, IF_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* CASE ... IN ... OUT ... ESAC are ranges */
      else if (IS (q, CASE_SYMBOL)) {
        if (whether (q, CASE_SYMBOL, IN_SYMBOL, STOP)) {
          TABLE (SUB (q)) = s;
          preliminary_symbol_table_setup (SUB (q));
          FORWARD (q);
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
          if ((FORWARD (q)) == NO_NODE) {
            not_a_for_range = A68_TRUE;
          } else {
            if (IS (q, OUT_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
            if (IS (q, CASE_SYMBOL)) {
              TABLE (SUB (q)) = new_symbol_table (s);
              preliminary_symbol_table_setup (SUB (q));
            }
          }
        } else {
          TABLE (SUB (q)) = new_symbol_table (s);
          preliminary_symbol_table_setup (SUB (q));
        }
      }
/* UNTIL ... OD is a range */
      else if (IS (q, UNTIL_SYMBOL) && SUB (q) != NO_NODE) {
        TABLE (SUB (q)) = new_symbol_table (s);
        preliminary_symbol_table_setup (SUB (q));
/* WHILE ... DO ... OD are ranges */
      } else if (IS (q, WHILE_SYMBOL)) {
        TABLE_T *u = new_symbol_table (s);
        TABLE (SUB (q)) = u;
        preliminary_symbol_table_setup (SUB (q));
        if ((FORWARD (q)) == NO_NODE) {
          not_a_for_range = A68_TRUE;
        } else if (IS (q, ALT_DO_SYMBOL)) {
          TABLE (SUB (q)) = new_symbol_table (u);
          preliminary_symbol_table_setup (SUB (q));
        }
      } else {
        TABLE (SUB (q)) = s;
        preliminary_symbol_table_setup (SUB (q));
      }
    }
  }
/* FOR identifiers will go to the DO ... OD range */
  if (!not_a_for_range) {
    for (q = p; q != NO_NODE; FORWARD (q)) {
      if (IS (q, FOR_SYMBOL)) {
        NODE_T *r = q;
        TABLE (NEXT (q)) = NO_TABLE;
        for (; r != NO_NODE && TABLE (NEXT (q)) == NO_TABLE; FORWARD (r)) {
          if ((is_one_of (r, WHILE_SYMBOL, ALT_DO_SYMBOL, STOP)) && (NEXT (q) != NO_NODE && SUB (r) != NO_NODE)) {
            TABLE (NEXT (q)) = TABLE (SUB (r));
            SEQUENCE (NEXT (q)) = SUB (r);
          }
        }
      }
    }
  }
}

/**
@brief Mark a mode as in use.
@param m Mode to mark.
**/

static void mark_mode (MOID_T * m)
{
  if (m != NO_MOID && USE (m) == A68_FALSE) {
    PACK_T *p = PACK (m);
    USE (m) = A68_TRUE;
    for (; p != NO_PACK; FORWARD (p)) {
      mark_mode (MOID (p));
      mark_mode (SUB (m));
      mark_mode (SLICE (m));
    }
  }
}

/**
@brief Traverse tree and mark modes as used.
@param p Node in syntax tree.
**/

void mark_moids (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    mark_moids (SUB (p));
    if (MOID (p) != NO_MOID) {
      mark_mode (MOID (p));
    }
  }
}

/**
@brief Mark various tags as used.
@param p Node in syntax tree.
**/

void mark_auxilliary (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE) {
/*
You get no warnings on unused PROC parameters. That is ok since A68 has some
parameters that you may not use at all - think of PROC (REF FILE) BOOL event
routines in transput.
*/
      mark_auxilliary (SUB (p));
    } else if (IS (p, OPERATOR)) {
      TAG_T *z;
      if (TAX (p) != NO_TAG) {
        USE (TAX (p)) = A68_TRUE;
      }
      if ((z = find_tag_global (TABLE (p), PRIO_SYMBOL, NSYMBOL (p))) != NO_TAG) {
        USE (z) = A68_TRUE;
      }
    } else if (IS (p, INDICANT)) {
      TAG_T *z = find_tag_global (TABLE (p), INDICANT, NSYMBOL (p));
      if (z != NO_TAG) {
        TAX (p) = z;
        USE (z) = A68_TRUE;
      }
    } else if (IS (p, IDENTIFIER)) {
      if (TAX (p) != NO_TAG) {
        USE (TAX (p)) = A68_TRUE;
      }
    }
  }
}

/**
@brief Check a single tag.
@param s Tag to check.
**/

static void unused (TAG_T * s)
{
  for (; s != NO_TAG; FORWARD (s)) {
    if (LINE_NUMBER (NODE (s)) > 0 && !USE (s)) {
      diagnostic_node (A68_WARNING, NODE (s), WARNING_TAG_UNUSED, NODE (s));
    }
  }
}

/**
@brief Driver for traversing tree and warn for unused tags.
@param p Node in syntax tree.
**/

void warn_for_unused_tags (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE) {
      if (is_new_lexical_level (p) && ATTRIBUTE (TABLE (SUB (p))) != ENVIRON_SYMBOL) {
        unused (OPERATORS (TABLE (SUB (p))));
        unused (PRIO (TABLE (SUB (p))));
        unused (IDENTIFIERS (TABLE (SUB (p))));
        unused (LABELS (TABLE (SUB (p))));
        unused (INDICANTS (TABLE (SUB (p))));
      }
    }
    warn_for_unused_tags (SUB (p));
  }
}

/**
@brief Mark jumps and procedured jumps.
@param p Node in syntax tree.
**/

void jumps_from_procs (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PROCEDURING)) {
      NODE_T *u = SUB_SUB (p);
      if (IS (u, GOTO_SYMBOL)) {
        FORWARD (u);
      }
      USE (TAX (u)) = A68_TRUE;
    } else if (IS (p, JUMP)) {
      NODE_T *u = SUB (p);
      if (IS (u, GOTO_SYMBOL)) {
        FORWARD (u);
      }
      if ((TAX (u) == NO_TAG) && (MOID (u) == NO_MOID) && (find_tag_global (TABLE (u), LABEL, NSYMBOL (u)) == NO_TAG)) {
        (void) add_tag (TABLE (u), LABEL, u, NO_MOID, LOCAL_LABEL);
        diagnostic_node (A68_ERROR, u, ERROR_UNDECLARED_TAG);
      } else {
         USE (TAX (u)) = A68_TRUE;
      }
    } else {
      jumps_from_procs (SUB (p));
    }
  }
}

/**
@brief Assign offset tags.
@param t Tag to start from.
@param base First (base) address.
@return End address.
**/

static ADDR_T assign_offset_tags (TAG_T * t, ADDR_T base)
{
  ADDR_T sum = base;
  for (; t != NO_TAG; FORWARD (t)) {
    ABEND (MOID (t) == NO_MOID, "tag has no mode", NSYMBOL (NODE (t)));
    SIZE (t) = moid_size (MOID (t));
    if (VALUE (t) == NO_TEXT) {
      OFFSET (t) = sum;
      sum += SIZE (t);
    }
  }
  return (sum);
}

/**
@brief Assign offsets table.
@param c Symbol table .
**/

void assign_offsets_table (TABLE_T * c)
{
  AP_INCREMENT (c) = assign_offset_tags (IDENTIFIERS (c), 0);
  AP_INCREMENT (c) = assign_offset_tags (OPERATORS (c), AP_INCREMENT (c));
  AP_INCREMENT (c) = assign_offset_tags (ANONYMOUS (c), AP_INCREMENT (c));
  AP_INCREMENT (c) = A68_ALIGN (AP_INCREMENT (c));
}

/**
@brief Assign offsets.
@param p Node in syntax tree.
**/

void assign_offsets (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (SUB (p) != NO_NODE && is_new_lexical_level (p)) {
      assign_offsets_table (TABLE (SUB (p)));
    }
    assign_offsets (SUB (p));
  }
}

/**
@brief Assign offsets packs in moid list.
@param q Moid to start from.
**/

void assign_offsets_packs (MOID_T * q)
{
  for (; q != NO_MOID; FORWARD (q)) {
    if (EQUIVALENT (q) == NO_MOID && IS (q, STRUCT_SYMBOL)) {
      PACK_T *p = PACK (q);
      ADDR_T offset = 0;
      for (; p != NO_PACK; FORWARD (p)) {
        SIZE (p) = moid_size (MOID (p));
        OFFSET (p) = offset;
        offset += SIZE (p);
      }
    }
  }
}

/**************************************/
/* MODE checker and coercion inserter */
/**************************************/


/**
@brief Give accurate error message.
@param n Node in syntax tree.
@param p Mode 1.
@param q Mode 2.
@param context Context.
@param deflex Deflexing regime.
@param depth Depth of recursion.
@return Error text.
**/

static char *mode_error_text (NODE_T * n, MOID_T * p, MOID_T * q, int context, int deflex, int depth)
{
#define TAIL(z) (&(z)[strlen (z)])
  static char txt[BUFFER_SIZE];
  if (depth == 1) {
    txt[0] = NULL_CHAR;
  }
  if (IS (p, SERIES_MODE)) {
    PACK_T *u = PACK (p);
    if (u == NO_PACK) {
      ASSERT (snprintf (txt, SNPRINTF_SIZE, "empty mode-list") >= 0);
    } else {
      for (; u != NO_PACK; FORWARD (u)) {
        if (MOID (u) != NO_MOID) {
          if (IS (MOID (u), SERIES_MODE)) {
            (void) mode_error_text (n, MOID (u), q, context, deflex, depth + 1);
          } else if (!is_coercible (MOID (u), q, context, deflex)) {
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
      }
    }
    if (depth == 1) {
      ASSERT (snprintf (TAIL (txt), SNPRINTF_SIZE, " cannot be coerced to %s", moid_to_string (q, MOID_ERROR_WIDTH, n)) >= 0);
    }
  } else if (IS (p, STOWED_MODE) && IS (q, FLEX_SYMBOL)) {
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
  return (txt);
#undef TAIL
}

/**
@brief Cannot coerce error.
@param p Node in syntax tree.
@param from Mode 1.
@param to Mode 2.
@param context Context.
@param deflex Deflexing regime.
@param att Attribute of context.
**/

static void cannot_coerce (NODE_T * p, MOID_T * from, MOID_T * to, int context, int deflex, int att)
{
  char *txt = mode_error_text (p, from, to, context, deflex, 1);
  if (att == STOP) {
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

/**
@brief Make SOID data structure.
@param s Soid buffer.
@param sort Sort.
@param type Mode.
@param attribute Attribute.
**/

static void make_soid (SOID_T * s, int sort, MOID_T * type, int attribute)
{
  ATTRIBUTE (s) = attribute;
  SORT (s) = sort;
  MOID (s) = type;
  CAST (s) = A68_FALSE;
}

/**
@brief Driver for mode checker.
@param p Node in syntax tree.
**/

void mode_checker (NODE_T * p)
{
  if (IS (p, PARTICULAR_PROGRAM)) {
    SOID_T x, y;
    top_soid_list = NO_SOID;
    make_soid (&x, STRONG, MODE (VOID), 0);
    mode_check_enclosed (SUB (p), &x, &y);
    MOID (p) = MOID (&y);
  }
}

/**
@brief Driver for coercion inserions.
@param p Node in syntax tree.
**/

void coercion_inserter (NODE_T * p)
{
  if (IS (p, PARTICULAR_PROGRAM)) {
    SOID_T q;
    make_soid (&q, STRONG, MODE (VOID), 0);
    coerce_enclosed (SUB (p), &q);
  }
}

/**
@brief Whether mode is not well defined.
@param p Mode.
@return See brief description.
**/

static BOOL_T is_mode_isnt_well (MOID_T * p)
{
  if (p == NO_MOID) {
    return (A68_TRUE);
  } else if (!IF_MODE_IS_WELL (p)) {
    return (A68_TRUE);
  } else if (PACK (p) != NO_PACK) {
    PACK_T *q = PACK (p);
    for (; q != NO_PACK; FORWARD (q)) {
      if (!IF_MODE_IS_WELL (MOID (q))) {
        return (A68_TRUE);
      }
    }
  }
  return (A68_FALSE);
}

/**
@brief Add SOID data to free chain.
@param root Top soid list.
**/

void free_soid_list (SOID_T *root) {
  if (root != NO_SOID) {
    SOID_T *q;
    for (q = root; NEXT (q) != NO_SOID; FORWARD (q)) {
      /* skip */;
    }
    NEXT (q) = top_soid_list;
    top_soid_list = root;
  }
}

/**
@brief Add SOID data structure to soid list.
@param root Top soid list.
@param where Node in syntax tree.
@param soid Entry to add.
**/

static void add_to_soid_list (SOID_T ** root, NODE_T * where, SOID_T * soid)
{
  if (*root != NO_SOID) {
    add_to_soid_list (&(NEXT (*root)), where, soid);
  } else {
    SOID_T *new_one;
    if (top_soid_list == NO_SOID) {
      new_one = (SOID_T *) get_temp_heap_space ((size_t) SIZE_AL (SOID_T));
    } else {
      new_one = top_soid_list;
      FORWARD (top_soid_list);
    }
    make_soid (new_one, SORT (soid), MOID (soid), 0);
    NODE (new_one) = where;
    NEXT (new_one) = NO_SOID;
    *root = new_one;
  }
}

/**
@brief Pack soids in moid, gather resulting moids from terminators in a clause.
@param top_sl Top soid list.
@param attribute Mode attribute.
@return Mode table entry.
**/

static MOID_T *pack_soids_in_moid (SOID_T * top_sl, int attribute)
{
  MOID_T *x = new_moid ();
  PACK_T *t, **p;
  ATTRIBUTE (x) = attribute;
  DIM (x) = 0;
  SUB (x) = NO_MOID;
  EQUIVALENT (x) = NO_MOID;
  SLICE (x) = NO_MOID;
  DEFLEXED (x) = NO_MOID;
  NAME (x) = NO_MOID;
  NEXT (x) = NO_MOID;
  PACK (x) = NO_PACK;
  p = &(PACK (x));
  for (; top_sl != NO_SOID; FORWARD (top_sl)) {
    t = new_pack ();
    MOID (t) = MOID (top_sl);
    TEXT (t) = NO_TEXT;
    NODE (t) = NODE (top_sl);
    NEXT (t) = NO_PACK;
    DIM (x)++;
    *p = t;
    p = &NEXT (t);
  }
  (void) register_extra_mode (&TOP_MOID (&program), x);
  return (x);
}

/**
@brief Whether "p" is compatible with "q".
@param p Mode.
@param q Mode.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_equal_modes (MOID_T * p, MOID_T * q, int deflex)
{
  if (deflex == FORCE_DEFLEXING) {
    return (DEFLEX (p) == DEFLEX (q));
  } else if (deflex == ALIAS_DEFLEXING) {
    if (IS (p, REF_SYMBOL) && IS (q, REF_SYMBOL)) {
      return (p == q || DEFLEX (p) == q);
    } else if (ISNT (p, REF_SYMBOL) && ISNT (q, REF_SYMBOL)) {
      return (DEFLEX (p) == DEFLEX (q));
    }
  } else if (deflex == SAFE_DEFLEXING) {
    if (ISNT (p, REF_SYMBOL) && ISNT (q, REF_SYMBOL)) {
      return (DEFLEX (p) == DEFLEX (q));
    }
  }
  return (p == q);
}

/**
@brief Whether mode is deprefable.
@param p Mode.
@return See brief description.
**/

BOOL_T is_deprefable (MOID_T * p)
{
  if (IS (p, REF_SYMBOL)) {
    return (A68_TRUE);
  } else {
    return ((BOOL_T) (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK));
  }
}

/**
@brief Depref mode once.
@param p Mode.
@return Single-depreffed mode.
**/

static MOID_T *depref_once (MOID_T * p)
{
  if (IS_REF_FLEX (p)) {
    return (SUB_SUB (p));
  } else if (IS (p, REF_SYMBOL)) {
    return (SUB (p));
  } else if (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    return (SUB (p));
  } else {
    return (NO_MOID);
  }
}

/**
@brief Depref mode completely.
@param p Mode.
@return Completely depreffed mode.
**/

MOID_T *depref_completely (MOID_T * p)
{
  while (is_deprefable (p)) {
    p = depref_once (p);
  }
  return (p);
}

/**
@brief Deproc_completely.
@param p Mode.
@return Completely deprocedured mode.
**/

static MOID_T *deproc_completely (MOID_T * p)
{
  while (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    p = depref_once (p);
  }
  return (p);
}

/**
@brief Depref rows.
@param p Mode.
@param q Mode.
@return Possibly depreffed mode.
**/

static MOID_T *depref_rows (MOID_T * p, MOID_T * q)
{
  if (q == MODE (ROWS)) {
    while (is_deprefable (p)) {
      p = depref_once (p);
    }
    return (p);
  } else {
    return (q);
  }
}

/**
@brief Derow mode, strip FLEX and BOUNDS.
@param p Mode.
@return See brief description.
**/

static MOID_T *derow (MOID_T * p)
{
  if (IS (p, ROW_SYMBOL) || IS (p, FLEX_SYMBOL)) {
    return (derow (SUB (p)));
  } else {
    return (p);
  }
}

/**
@brief Whether rows type.
@param p Mode.
@return See brief description.
**/

static BOOL_T is_rows_type (MOID_T * p)
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
      while (t != NO_PACK && go_on) {
        go_on &= is_rows_type (MOID (t));
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

/**
@brief Whether mode is PROC (REF FILE) VOID or FORMAT.
@param p Mode.
@return See brief description.
**/

static BOOL_T is_proc_ref_file_void_or_format (MOID_T * p)
{
  if (p == MODE (PROC_REF_FILE_VOID)) {
    return (A68_TRUE);
  } else if (p == MODE (FORMAT)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether mode can be transput.
@param p Mode.
@param rw Indicates Read or Write.
@return See brief description.
**/

static BOOL_T is_transput_mode (MOID_T * p, char rw)
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
  } else if (IS (p, UNION_SYMBOL) || IS (p, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (p);
    BOOL_T k = A68_TRUE;
    for (; q != NO_PACK && k; FORWARD (q)) {
      k = (BOOL_T) (k & (is_transput_mode (MOID (q), rw) || is_proc_ref_file_void_or_format (MOID (q))));
    }
    return (k);
  } else if (IS (p, FLEX_SYMBOL)) {
    if (SUB (p) == MODE (ROW_CHAR)) {
      return (A68_TRUE);
    } else {
      return ((BOOL_T) (rw == 'w' ? is_transput_mode (SUB (p), rw) : A68_FALSE));
    }
  } else if (IS (p, ROW_SYMBOL)) {
    return ((BOOL_T) (is_transput_mode (SUB (p), rw) || is_proc_ref_file_void_or_format (SUB (p))));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether mode is printable.
@param p Mode.
@return See brief description.
**/

static BOOL_T is_printable_mode (MOID_T * p)
{
  if (is_proc_ref_file_void_or_format (p)) {
    return (A68_TRUE);
  } else {
    return (is_transput_mode (p, 'w'));
  }
}

/**
@brief Whether mode is readable.
@param p Mode.
@return See brief description.
**/

static BOOL_T is_readable_mode (MOID_T * p)
{
  if (is_proc_ref_file_void_or_format (p)) {
    return (A68_TRUE);
  } else {
    return ((BOOL_T) (IS (p, REF_SYMBOL) ? is_transput_mode (SUB (p), 'r') : A68_FALSE));
  }
}

/**
@brief Whether name struct.
@param p Mode.
@return See brief description.
**/

static BOOL_T is_name_struct (MOID_T * p)
{
  return ((BOOL_T) (NAME (p) != NO_MOID ? IS (DEFLEX (SUB (p)), STRUCT_SYMBOL) : A68_FALSE));
}

/**
@brief Yield mode to unite to.
@param m Mode.
@param u United mode.
@return See brief description.
**/

MOID_T *unites_to (MOID_T * m, MOID_T * u)
{
/* Uniting U (m) */
  MOID_T *v = NO_MOID;
  PACK_T *p;
  if (u == MODE (SIMPLIN) || u == MODE (SIMPLOUT)) {
    return (m);
  }
  for (p = PACK (u); p != NO_PACK; FORWARD (p)) {
/* Prefer []->[] over []->FLEX [] */
    if (m == MOID (p)) {
      v = MOID (p);
    } else if (v == NO_MOID && DEFLEX (m) == DEFLEX (MOID (p))) {
      v = MOID (p);
    }
  }
  return (v);
}

/**
@brief Whether moid in pack.
@param u Mode.
@param v Pack.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_moid_in_pack (MOID_T * u, PACK_T * v, int deflex)
{
  for (; v != NO_PACK; FORWARD (v)) {
    if (is_equal_modes (u, MOID (v), deflex)) {
      return (A68_TRUE);
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether "p" is a subset of "q".
@param p Mode.
@param q Mode.
@param deflex Deflexing regime.
@return See brief description.
**/

BOOL_T is_subset (MOID_T * p, MOID_T * q, int deflex)
{
  PACK_T *u = PACK (p);
  BOOL_T j = A68_TRUE;
  for (; u != NO_PACK && j; FORWARD (u)) {
    j = (BOOL_T) (j && is_moid_in_pack (MOID (u), PACK (q), deflex));
  }
  return (j);
}

/**
@brief Whether "p" can be united to UNION "q".
@param p Mode.
@param q Mode.
@param deflex Deflexing regime.
@return See brief description.
**/

BOOL_T is_unitable (MOID_T * p, MOID_T * q, int deflex)
{
  if (IS (q, UNION_SYMBOL)) {
    if (IS (p, UNION_SYMBOL)) {
      return (is_subset (p, q, deflex));
    } else {
      return (is_moid_in_pack (p, PACK (q), deflex));
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether all or some components of "u" can be firmly coerced to a component mode of "v"..
@param u Mode.
@param v Mode .
@param all All coercible.
@param some Some coercible.
**/

static void investigate_firm_relations (PACK_T * u, PACK_T * v, BOOL_T * all, BOOL_T * some)
{
  *all = A68_TRUE;
  *some = A68_FALSE;
  for (; v != NO_PACK; FORWARD (v)) {
    PACK_T *w;
    BOOL_T k = A68_FALSE;
    for (w = u; w != NO_PACK; FORWARD (w)) {
      k |= is_coercible (MOID (w), MOID (v), FIRM, FORCE_DEFLEXING);
    }
    *some |= k;
    *all &= k;
  }
}

/**
@brief Whether there is a soft path from "p" to "q".
@param p Mode.
@param q Mode.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_softly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return (A68_TRUE);
  } else if (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    return (is_softly_coercible (SUB (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether there is a weak path from "p" to "q".
@param p Mode.
@param q Mode.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_weakly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return (A68_TRUE);
  } else if (is_deprefable (p)) {
    return (is_weakly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether there is a meek path from "p" to "q".
@param p Mode.
@param q Mode.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_meekly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return (A68_TRUE);
  } else if (is_deprefable (p)) {
    return (is_meekly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether there is a firm path from "p" to "q".
@param p Mode.
@param q Mode.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_firmly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return (A68_TRUE);
  } else if (q == MODE (ROWS) && is_rows_type (p)) {
    return (A68_TRUE);
  } else if (is_unitable (p, q, deflex)) {
    return (A68_TRUE);
  } else if (is_deprefable (p)) {
    return (is_firmly_coercible (depref_once (p), q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether "p" widens to "q".
@param p Mode.
@param q Mode.
@return See brief description.
**/

static MOID_T *widens_to (MOID_T * p, MOID_T * q)
{
  if (p == MODE (INT)) {
    if (q == MODE (LONG_INT) || q == MODE (LONGLONG_INT) || q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_INT));
    } else if (q == MODE (REAL) || q == MODE (COMPLEX)) {
      return (MODE (REAL));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (LONG_INT)) {
    if (q == MODE (LONGLONG_INT)) {
      return (MODE (LONGLONG_INT));
    } else if (q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_REAL));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (LONGLONG_INT)) {
    if (q == MODE (LONGLONG_REAL) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_REAL));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (REAL)) {
    if (q == MODE (LONG_REAL) || q == MODE (LONGLONG_REAL) || q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_REAL));
    } else if (q == MODE (COMPLEX)) {
      return (MODE (COMPLEX));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (COMPLEX)) {
    if (q == MODE (LONG_COMPLEX) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONG_COMPLEX));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (LONG_REAL)) {
    if (q == MODE (LONGLONG_REAL) || q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_REAL));
    } else if (q == MODE (LONG_COMPLEX)) {
      return (MODE (LONG_COMPLEX));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (LONG_COMPLEX)) {
    if (q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_COMPLEX));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (LONGLONG_REAL)) {
    if (q == MODE (LONGLONG_COMPLEX)) {
      return (MODE (LONGLONG_COMPLEX));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (BITS)) {
    if (q == MODE (LONG_BITS) || q == MODE (LONGLONG_BITS)) {
      return (MODE (LONG_BITS));
    } else if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (LONG_BITS)) {
    if (q == MODE (LONGLONG_BITS)) {
      return (MODE (LONGLONG_BITS));
    } else if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (LONGLONG_BITS)) {
    if (q == MODE (ROW_BOOL)) {
      return (MODE (ROW_BOOL));
    } else {
      return (NO_MOID);
    }
  } else if (p == MODE (BYTES) && q == MODE (ROW_CHAR)) {
    return (MODE (ROW_CHAR));
  } else if (p == MODE (LONG_BYTES) && q == MODE (ROW_CHAR)) {
    return (MODE (ROW_CHAR));
  } else {
    return (NO_MOID);
  }
}

/**
@brief Whether "p" widens to "q".
@param p Mode.
@param q Mode.
@return See brief description.
**/

static BOOL_T is_widenable (MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  if (z != NO_MOID) {
    return ((BOOL_T) (z == q ? A68_TRUE : is_widenable (z, q)));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether "p" is a REF ROW.
@param p Mode.
@return See brief description.
**/

static BOOL_T is_ref_row (MOID_T * p)
{
  return ((BOOL_T) (NAME (p) != NO_MOID ? IS (DEFLEX (SUB (p)), ROW_SYMBOL) : A68_FALSE));
}

/**
@brief Whether strong name.
@param p Mode.
@param q Mode.
@return See brief description.
**/

static BOOL_T is_strong_name (MOID_T * p, MOID_T * q)
{
  if (p == q) {
    return (A68_TRUE);
  } else if (is_ref_row (q)) {
    return (is_strong_name (p, NAME (q)));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether strong slice.
@param p Mode.
@param q Mode.
@return See brief description.
**/

static BOOL_T is_strong_slice (MOID_T * p, MOID_T * q)
{
  if (p == q || is_widenable (p, q)) {
    return (A68_TRUE);
  } else if (SLICE (q) != NO_MOID) {
    return (is_strong_slice (p, SLICE (q)));
  } else if (IS (q, FLEX_SYMBOL)) {
    return (is_strong_slice (p, SUB (q)));
  } else if (is_ref_row (q)) {
    return (is_strong_name (p, q));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether strongly coercible.
@param p Mode.
@param q Mode.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_strongly_coercible (MOID_T * p, MOID_T * q, int deflex)
{
/* Keep this sequence of statements */
  if (is_equal_modes (p, q, deflex)) {
    return (A68_TRUE);
  } else if (q == MODE (VOID)) {
    return (A68_TRUE);
  } else if ((q == MODE (SIMPLIN) || q == MODE (ROW_SIMPLIN)) && is_readable_mode (p)) {
    return (A68_TRUE);
  } else if (q == MODE (ROWS) && is_rows_type (p)) {
    return (A68_TRUE);
  } else if (is_unitable (p, derow (q), deflex)) {
    return (A68_TRUE);
  }
  if (is_ref_row (q) && is_strong_name (p, q)) {
    return (A68_TRUE);
  } else if (SLICE (q) != NO_MOID && is_strong_slice (p, q)) {
    return (A68_TRUE);
  } else if (IS (q, FLEX_SYMBOL) && is_strong_slice (p, q)) {
    return (A68_TRUE);
  } else if (is_widenable (p, q)) {
    return (A68_TRUE);
  } else if (is_deprefable (p)) {
    return (is_strongly_coercible (depref_once (p), q, deflex));
  } else if (q == MODE (SIMPLOUT) || q == MODE (ROW_SIMPLOUT)) {
    return (is_printable_mode (p));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether firm.
@param p Mode.
@param q Mode.
@return See brief description.
**/

BOOL_T is_firm (MOID_T * p, MOID_T * q)
{
  return ((BOOL_T) (is_firmly_coercible (p, q, SAFE_DEFLEXING) || is_firmly_coercible (q, p, SAFE_DEFLEXING)));
}

/**
@brief Whether coercible stowed.
@param p Mode.
@param q Mode.
@param c Context.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_coercible_stowed (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c == STRONG) {
    if (q == MODE (VOID)) {
      return (A68_TRUE);
    } else if (IS (q, FLEX_SYMBOL)) {
      PACK_T *u = PACK (p);
      BOOL_T j = A68_TRUE;
      for (; u != NO_PACK && j; FORWARD (u)) {
        j &= is_coercible (MOID (u), SLICE (SUB (q)), c, deflex);
      }
      return (j);
    } else if (IS (q, ROW_SYMBOL)) {
      PACK_T *u = PACK (p);
      BOOL_T j = A68_TRUE;
      for (; u != NO_PACK && j; FORWARD (u)) {
        j &= is_coercible (MOID (u), SLICE (q), c, deflex);
      }
      return (j);
    } else if (IS (q, PROC_SYMBOL) || IS (q, STRUCT_SYMBOL)) {
      PACK_T *u = PACK (p), *v = PACK (q);
      if (DIM (p) != DIM (q)) {
        return (A68_FALSE);
      } else {
        BOOL_T j = A68_TRUE;
        while (u != NO_PACK && v != NO_PACK && j) {
          j &= is_coercible (MOID (u), MOID (v), c, deflex);
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

/**
@brief Whether coercible series.
@param p Mode.
@param q Mode.
@param c Context.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_coercible_series (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (c != STRONG) {
    return (A68_FALSE);
  } else if (p == NO_MOID || q == NO_MOID) {
    return (A68_FALSE);
  } else if (IS (p, SERIES_MODE) && PACK (p) == NO_PACK) {
    return (A68_FALSE);
  } else if (IS (q, SERIES_MODE) && PACK (q) == NO_PACK) {
    return (A68_FALSE);
  } else if (PACK (p) == NO_PACK) {
    return (is_coercible (p, q, c, deflex));
  } else {
    PACK_T *u = PACK (p);
    BOOL_T j = A68_TRUE;
    for (; u != NO_PACK && j; FORWARD (u)) {
      if (MOID (u) != NO_MOID) {
        j &= is_coercible (MOID (u), q, c, deflex);
      }
    }
    return (j);
  }
}

/**
@brief Basic coercions.
@param p Mode.
@param q Mode.
@param c Context.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T basic_coercions (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (is_equal_modes (p, q, deflex)) {
    return (A68_TRUE);
  } else if (c == NO_SORT) {
    return ((BOOL_T) (p == q));
  } else if (c == SOFT) {
    return (is_softly_coercible (p, q, deflex));
  } else if (c == WEAK) {
    return (is_weakly_coercible (p, q, deflex));
  } else if (c == MEEK) {
    return (is_meekly_coercible (p, q, deflex));
  } else if (c == FIRM) {
    return (is_firmly_coercible (p, q, deflex));
  } else if (c == STRONG) {
    return (is_strongly_coercible (p, q, deflex));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether "p" can be coerced to "q" in a "c" context.
@param p Mode.
@param q Mode.
@param c Context.
@param deflex Deflexing regime.
@return See brief description.
**/

BOOL_T is_coercible (MOID_T * p, MOID_T * q, int c, int deflex)
{
  if (is_mode_isnt_well (p) || is_mode_isnt_well (q)) {
    return (A68_TRUE);
  } else if (is_equal_modes (p, q, deflex)) {
    return (A68_TRUE);
  } else if (p == MODE (HIP)) {
    return (A68_TRUE);
  } else if (IS (p, STOWED_MODE)) {
    return (is_coercible_stowed (p, q, c, deflex));
  } else if (IS (p, SERIES_MODE)) {
    return (is_coercible_series (p, q, c, deflex));
  } else if (p == MODE (VACUUM) && IS (DEFLEX (q), ROW_SYMBOL)) {
    return (A68_TRUE);
  } else {
    return (basic_coercions (p, q, c, deflex));
  }
}

/**
@brief Whether coercible in context.
@param p Soid.
@param q Soid.
@param deflex Deflexing regime.
@return See brief description.
**/

static BOOL_T is_coercible_in_context (SOID_T * p, SOID_T * q, int deflex)
{
  if (SORT (p) != SORT (q)) {
    return (A68_FALSE);
  } else if (MOID (p) == MOID (q)) {
    return (A68_TRUE);
  } else {
    return (is_coercible (MOID (p), MOID (q), SORT (q), deflex));
  }
}

/**
@brief Whether list "y" is balanced.
@param n Node in syntax tree.
@param y Soid list.
@param sort Sort.
@return See brief description.
**/

static BOOL_T is_balanced (NODE_T * n, SOID_T * y, int sort)
{
  if (sort == STRONG) {
    return (A68_TRUE);
  } else {
    BOOL_T k = A68_FALSE;
    for (; y != NO_SOID && !k; FORWARD (y)) {
      k = (BOOL_T) (ISNT (MOID (y), STOWED_MODE));
    }
    if (k == A68_FALSE) {
      diagnostic_node (A68_ERROR, n, ERROR_NO_UNIQUE_MODE);
    }
    return (k);
  }
}

/**
@brief A moid from "m" to which all other members can be coerced.
@param m Mode.
@param sort Sort.
@param return_depreffed Whether to depref.
@param deflex Deflexing regime.
@return See brief description.
**/

MOID_T *get_balanced_mode (MOID_T * m, int sort, BOOL_T return_depreffed, int deflex)
{
  MOID_T *common = NO_MOID;
  if (m != NO_MOID && !is_mode_isnt_well (m) && IS (m, UNION_SYMBOL)) {
    int depref_level;
    BOOL_T go_on = A68_TRUE;
/* Test for increasing depreffing */
    for (depref_level = 0; go_on; depref_level++) {
      PACK_T *p;
      go_on = A68_FALSE;
/* Test the whole pack */
      for (p = PACK (m); p != NO_PACK; FORWARD (p)) {
/* HIPs are not eligible of course */
        if (MOID (p) != MODE (HIP)) {
          MOID_T *candidate = MOID (p);
          int k;
/* Depref as far as allowed */
          for (k = depref_level; k > 0 && is_deprefable (candidate); k--) {
            candidate = depref_once (candidate);
          }
/* Only need testing if all allowed deprefs succeeded */
          if (k == 0) {
            PACK_T *q;
            MOID_T *to = (return_depreffed ? depref_completely (candidate) : candidate);
            BOOL_T all_coercible = A68_TRUE;
            go_on = A68_TRUE;
            for (q = PACK (m); q != NO_PACK && all_coercible; FORWARD (q)) {
              MOID_T *from = MOID (q);
              if (p != q && from != to) {
                all_coercible &= is_coercible (from, to, sort, deflex);
              }
            }
/* If the pack is coercible to the candidate, we mark the candidate.
   We continue searching for longest series of REF REF PROC REF . */
            if (all_coercible) {
              MOID_T *mark = (return_depreffed ? MOID (p) : candidate);
              if (common == NO_MOID) {
                common = mark;
              } else if (IS (candidate, FLEX_SYMBOL) && DEFLEX (candidate) == common) {
/* We prefer FLEX */
                common = mark;
              }
            }
          }
        }
      }/* for */
    }/* for */
  }
  return (common == NO_MOID ? m : common);
}

/**
@brief Whether we can search a common mode from a clause or not.
@param att Attribute.
@return See brief description.
**/

static BOOL_T clause_allows_balancing (int att)
{
  switch (att) {
  case CLOSED_CLAUSE:
  case CONDITIONAL_CLAUSE:
  case CASE_CLAUSE:
  case SERIAL_CLAUSE:
  case CONFORMITY_CLAUSE:
    {
      return (A68_TRUE);
    }
  }
  return (A68_FALSE);
}

/**
@brief A unique mode from "z".
@param z Soid.
@param deflex Deflexing regime.
@return See brief description.
**/

static MOID_T *determine_unique_mode (SOID_T * z, int deflex)
{
  if (z == NO_SOID) {
    return (NO_MOID);
  } else {
    MOID_T *x = MOID (z);
    if (is_mode_isnt_well (x)) {
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

/**
@brief Give a warning when a value is silently discarded.
@param p Node in syntax tree.
@param x Soid.
@param y Soid.
@param c Context.
**/

static void warn_for_voiding (NODE_T * p, SOID_T * x, SOID_T * y, int c)
{
  (void) c;
  if (CAST (x) == A68_FALSE) {
    if (MOID (x) == MODE (VOID) && MOID (y) != MODE (ERROR) && !(MOID (y) == MODE (VOID) || !is_nonproc (MOID (y)))) {
      if (IS (p, FORMULA)) {
        diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_VOIDED, MOID (y));
      } else {
        diagnostic_node (A68_WARNING, p, WARNING_VOIDED, MOID (y));
      }
    }
  }
}

/**
@brief Warn for things that are likely unintended.
@param p Node in syntax tree.
@param m Moid.
@param c Context.
@param u Attribute.
**/

static void semantic_pitfall (NODE_T * p, MOID_T * m, int c, int u)
{
/*
semantic_pitfall: warn for things that are likely unintended, for instance
                  REF INT i := LOC INT := 0, which should probably be
                  REF INT i = LOC INT := 0.
*/
  if (IS (p, u)) {
    diagnostic_node (A68_WARNING, p, WARNING_UNINTENDED, MOID (p), u, m, c);
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, STOP)) {
    semantic_pitfall (SUB (p), m, c, u);
  }
}

/**
@brief Insert coercion "a" in the tree.
@param l Node in syntax tree.
@param a Attribute.
@param m (coerced) moid
**/

static void make_coercion (NODE_T * l, int a, MOID_T * m)
{
  make_sub (l, l, a);
  MOID (l) = depref_rows (MOID (l), m);
}

/**
@brief Make widening coercion.
@param n Node in syntax tree.
@param p Mode.
@param q Mode.
**/

static void make_widening_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  MOID_T *z = widens_to (p, q);
  make_coercion (n, WIDENING, z);
  if (z != q) {
    make_widening_coercion (n, z, q);
  }
}

/**
@brief Make ref rowing coercion.
@param n Node in syntax tree.
@param p Mode.
@param q Mode.
**/

static void make_ref_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (is_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (is_ref_row (q)) {
      make_ref_rowing_coercion (n, p, NAME (q));
      make_coercion (n, ROWING, q);
    }
  }
}

/**
@brief Make rowing coercion.
@param n Node in syntax tree.
@param p Mode.
@param q Mode.
**/

static void make_rowing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) != DEFLEX (q)) {
    if (is_widenable (p, q)) {
      make_widening_coercion (n, p, q);
    } else if (SLICE (q) != NO_MOID) {
      make_rowing_coercion (n, p, SLICE (q));
      make_coercion (n, ROWING, q);
    } else if (IS (q, FLEX_SYMBOL)) {
      make_rowing_coercion (n, p, SUB (q));
    } else if (is_ref_row (q)) {
      make_ref_rowing_coercion (n, p, q);
    }
  }
}

/**
@brief Make uniting coercion.
@param n Node in syntax tree.
@param q Mode.
**/

static void make_uniting_coercion (NODE_T * n, MOID_T * q)
{
  make_coercion (n, UNITING, derow (q));
  if (IS (q, ROW_SYMBOL) || IS (q, FLEX_SYMBOL)) {
    make_rowing_coercion (n, derow (q), q);
  }
}

/**
@brief Make depreffing coercion.
@param n Node in syntax tree.
@param p Mode.
@param q Mode.
**/

static void make_depreffing_coercion (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (DEFLEX (p) == DEFLEX (q)) {
    return;
  } else if (q == MODE (SIMPLOUT) && is_printable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == MODE (ROW_SIMPLOUT) && is_printable_mode (p)) {
    make_coercion (n, UNITING, MODE (SIMPLOUT));
    make_coercion (n, ROWING, MODE (ROW_SIMPLOUT));
  } else if (q == MODE (SIMPLIN) && is_readable_mode (p)) {
    make_coercion (n, UNITING, q);
  } else if (q == MODE (ROW_SIMPLIN) && is_readable_mode (p)) {
    make_coercion (n, UNITING, MODE (SIMPLIN));
    make_coercion (n, ROWING, MODE (ROW_SIMPLIN));
  } else if (q == MODE (ROWS) && is_rows_type (p)) {
    make_coercion (n, UNITING, MODE (ROWS));
    MOID (n) = MODE (ROWS);
  } else if (is_widenable (p, q)) {
    make_widening_coercion (n, p, q);
  } else if (is_unitable (p, derow (q), SAFE_DEFLEXING)) {
    make_uniting_coercion (n, q);
  } else if (is_ref_row (q) && is_strong_name (p, q)) {
    make_ref_rowing_coercion (n, p, q);
  } else if (SLICE (q) != NO_MOID && is_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (IS (q, FLEX_SYMBOL) && is_strong_slice (p, q)) {
    make_rowing_coercion (n, p, q);
  } else if (IS (p, REF_SYMBOL)) {
    MOID_T *r = depref_once (p);
    make_coercion (n, DEREFERENCING, r);
    make_depreffing_coercion (n, r, q);
  } else if (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    MOID_T *r = SUB (p);
    make_coercion (n, DEPROCEDURING, r);
    make_depreffing_coercion (n, r, q);
  } else if (p != q) {
    cannot_coerce (n, p, q, NO_SORT, SKIP_DEFLEXING, 0);
  }
}

/**
@brief Whether p is a nonproc mode (that is voided directly).
@param p Mode.
@return See brief description.
**/

static BOOL_T is_nonproc (MOID_T * p)
{
  if (IS (p, PROC_SYMBOL) && PACK (p) == NO_PACK) {
    return (A68_FALSE);
  } else if (IS (p, REF_SYMBOL)) {
    return (is_nonproc (SUB (p)));
  } else {
    return (A68_TRUE);
  }
}

/**
@brief Make_void: voiden in an appropriate way.
@param p Node in syntax tree.
@param q Mode.
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
      if (is_nonproc (q)) {
        make_coercion (p, VOIDING, MODE (VOID));
        return;
      } else {
/* Descend the chain of e.g. REF PROC .. until a nonproc moid remains */
        MOID_T *z = q;
        while (!is_nonproc (z)) {
          if (IS (z, REF_SYMBOL)) {
            make_coercion (p, DEREFERENCING, SUB (z));
          }
          if (IS (z, PROC_SYMBOL) && NODE_PACK (p) == NO_PACK) {
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

/**
@brief Make strong coercion.
@param n Node in syntax tree.
@param p Mode.
@param q Mode.
**/

static void make_strong (NODE_T * n, MOID_T * p, MOID_T * q)
{
  if (q == MODE (VOID) && p != MODE (VOID)) {
    make_void (n, p);
  } else {
    make_depreffing_coercion (n, p, q);
  }
}

/**
@brief Mode check on bounds.
@param p Node in syntax tree.
**/

static void mode_check_bounds (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, MODE (INT), 0);
    mode_check_unit (p, &x, &y);
    if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&y), MODE (INT), MEEK, SAFE_DEFLEXING, UNIT);
    }
    mode_check_bounds (NEXT (p));
  } else {
    mode_check_bounds (SUB (p));
    mode_check_bounds (NEXT (p));
  }
}

/**
@brief Mode check declarer.
@param p Node in syntax tree.
**/

static void mode_check_declarer (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, BOUNDS)) {
    mode_check_bounds (SUB (p));
    mode_check_declarer (NEXT (p));
  } else {
    mode_check_declarer (SUB (p));
    mode_check_declarer (NEXT (p));
  }
}

/**
@brief Mode check identity declaration.
@param p Node in syntax tree.
**/

static void mode_check_identity_declaration (NODE_T * p)
{
  if (p != NO_NODE) {
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
        if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
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

/**
@brief Mode check variable declaration.
@param p Node in syntax tree.
**/

static void mode_check_variable_declaration (NODE_T * p)
{
  if (p != NO_NODE) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        mode_check_declarer (SUB (p));
        mode_check_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP)) {
          SOID_T x, y;
          make_soid (&x, STRONG, SUB_MOID (p), 0);
          mode_check_unit (NEXT_NEXT (p), &x, &y);
          if (!is_coercible_in_context (&y, &x, FORCE_DEFLEXING)) {
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

/**
@brief Mode check routine text.
@param p Node in syntax tree.
@param y Resulting soid.
**/

static void mode_check_routine_text (NODE_T * p, SOID_T * y)
{
  SOID_T w;
  if (IS (p, PARAMETER_PACK)) {
    mode_check_declarer (SUB (p));
    FORWARD (p);
  }
  mode_check_declarer (SUB (p));
  make_soid (&w, STRONG, MOID (p), 0);
  mode_check_unit (NEXT_NEXT (p), &w, y);
  if (!is_coercible_in_context (y, &w, FORCE_DEFLEXING)) {
    cannot_coerce (NEXT_NEXT (p), MOID (y), MOID (&w), STRONG, FORCE_DEFLEXING, UNIT);
  }
}

/**
@brief Mode check proc declaration.
@param p Node in syntax tree.
**/

static void mode_check_proc_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, ROUTINE_TEXT)) {
    SOID_T x, y;
    make_soid (&x, STRONG, NO_MOID, 0);
    mode_check_routine_text (SUB (p), &y);
  } else {
    mode_check_proc_declaration (SUB (p));
    mode_check_proc_declaration (NEXT (p));
  }
}

/**
@brief Mode check brief op declaration.
@param p Node in syntax tree.
**/

static void mode_check_brief_op_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, DEFINING_OPERATOR)) {
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

/**
@brief Mode check op declaration.
@param p Node in syntax tree.
**/

static void mode_check_op_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, DEFINING_OPERATOR)) {
    SOID_T y, x;
    make_soid (&x, STRONG, MOID (p), 0);
    mode_check_unit (NEXT_NEXT (p), &x, &y);
    if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_NEXT (p), MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, UNIT);
    }
  } else {
    mode_check_op_declaration (SUB (p));
    mode_check_op_declaration (NEXT (p));
  }
}

/**
@brief Mode check declaration list.
@param p Node in syntax tree.
**/

static void mode_check_declaration_list (NODE_T * p)
{
  if (p != NO_NODE) {
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

/**
@brief Mode check serial clause.
@param r Resulting soids.
@param p Node in syntax tree.
@param x Expected soid.
@param k Whether statement yields a value other than VOID.
**/

static void mode_check_serial (SOID_T ** r, NODE_T * p, SOID_T * x, BOOL_T k)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, INITIALISER_SERIES)) {
    mode_check_serial (r, SUB (p), x, A68_FALSE);
    mode_check_serial (r, NEXT (p), x, k);
  } else if (IS (p, DECLARATION_LIST)) {
    mode_check_declaration_list (SUB (p));
  } else if (is_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, STOP)) {
    mode_check_serial (r, NEXT (p), x, k);
  } else if (is_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, STOP)) {
    if (NEXT (p) != NO_NODE) {
      if (IS (NEXT (p), EXIT_SYMBOL) || IS (NEXT (p), END_SYMBOL) || IS (NEXT (p), CLOSE_SYMBOL)) {
        mode_check_serial (r, SUB (p), x, A68_TRUE);
      } else {
        mode_check_serial (r, SUB (p), x, A68_FALSE);
      }
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      mode_check_serial (r, SUB (p), x, A68_TRUE);
    }
  } else if (IS (p, LABELED_UNIT)) {
    mode_check_serial (r, SUB (p), x, k);
  } else if (IS (p, UNIT)) {
    SOID_T y;
    if (k) {
      mode_check_unit (p, x, &y);
    } else {
      SOID_T w;
      make_soid (&w, STRONG, MODE (VOID), 0);
      mode_check_unit (p, &w, &y);
    }
    if (NEXT (p) != NO_NODE) {
      mode_check_serial (r, NEXT (p), x, k);
    } else {
      if (k) {
        add_to_soid_list (r, p, &y);
      }
    }
  }
}

/**
@brief Mode check serial clause units.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
@param att Attribute (SERIAL or ENQUIRY).
**/

static void mode_check_serial_units (NODE_T * p, SOID_T * x, SOID_T * y, int att)
{
  SOID_T *top_sl = NO_SOID;
  (void) att;
  mode_check_serial (&top_sl, SUB (p), x, A68_TRUE);
  if (is_balanced (p, top_sl, SORT (x))) {
    MOID_T *result = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), result, SERIAL_CLAUSE);
  } else {
    make_soid (y, SORT (x), (MOID (x) != NO_MOID ? MOID (x) : MODE (ERROR)), 0);
  }
  free_soid_list (top_sl);
}

/**
@brief Mode check unit list.
@param r Resulting soids.
@param p Node in syntax tree.
@param x Expected soid.
**/

static void mode_check_unit_list (SOID_T ** r, NODE_T * p, SOID_T * x)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT_LIST)) {
    mode_check_unit_list (r, SUB (p), x);
    mode_check_unit_list (r, NEXT (p), x);
  } else if (IS (p, COMMA_SYMBOL)) {
    mode_check_unit_list (r, NEXT (p), x);
  } else if (IS (p, UNIT)) {
    SOID_T y;
    mode_check_unit (p, x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_unit_list (r, NEXT (p), x);
  }
}

/**
@brief Mode check struct display.
@param r Resulting soids.
@param p Node in syntax tree.
@param fields Pack.
**/

static void mode_check_struct_display (SOID_T ** r, NODE_T * p, PACK_T ** fields)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT_LIST)) {
    mode_check_struct_display (r, SUB (p), fields);
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (IS (p, COMMA_SYMBOL)) {
    mode_check_struct_display (r, NEXT (p), fields);
  } else if (IS (p, UNIT)) {
    SOID_T x, y;
    if (*fields != NO_PACK) {
      make_soid (&x, STRONG, MOID (*fields), 0);
      FORWARD (*fields);
    } else {
      make_soid (&x, STRONG, NO_MOID, 0);
    }
    mode_check_unit (p, &x, &y);
    add_to_soid_list (r, p, &y);
    mode_check_struct_display (r, NEXT (p), fields);
  }
}

/**
@brief Mode check get specified moids.
@param p Node in syntax tree.
@param u United mode to add to.
**/

static void mode_check_get_specified_moids (NODE_T * p, MOID_T * u)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (is_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, STOP)) {
      mode_check_get_specified_moids (SUB (p), u);
    } else if (IS (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      add_mode_to_pack (&(PACK (u)), m, NO_TEXT, NODE (m));
    }
  }
}

/**
@brief Mode check specified unit list.
@param r Resulting soids.
@param p Node in syntax tree.
@param x Expected soid.
@param u Resulting united mode.
**/

static void mode_check_specified_unit_list (SOID_T ** r, NODE_T * p, SOID_T * x, MOID_T * u)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (is_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, STOP)) {
      mode_check_specified_unit_list (r, SUB (p), x, u);
    } else if (IS (p, SPECIFIER)) {
      MOID_T *m = MOID (NEXT_SUB (p));
      if (u != NO_MOID && !is_unitable (m, u, SAFE_DEFLEXING)) {
        diagnostic_node (A68_ERROR, p, ERROR_NO_COMPONENT, m, u);
      }
    } else if (IS (p, UNIT)) {
      SOID_T y;
      mode_check_unit (p, x, &y);
      add_to_soid_list (r, p, &y);
    }
  }
}

/**
@brief Mode check united case parts.
@param ry Resulting soids.
@param p Node in syntax tree.
@param x Expected soid.
**/

static void mode_check_united_case_parts (SOID_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  MOID_T *u = NO_MOID, *v = NO_MOID, *w = NO_MOID;
/* Check the CASE part and deduce the united mode */
  make_soid (&enq_expct, STRONG, NO_MOID, 0);
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
    if (IS (u, UNION_SYMBOL)) {
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
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (is_one_of (p, CONFORMITY_OUSE_PART, BRIEF_CONFORMITY_OUSE_PART, STOP)) {
      mode_check_united_case_parts (ry, SUB (p), x);
    }
  }
}

/**
@brief Mode check united case.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_united_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T *top_sl = NO_SOID;
  MOID_T *z;
  mode_check_united_case_parts (&top_sl, p, x);
  if (!is_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NO_MOID) {
      make_soid (y, SORT (x), MOID (x), CONFORMITY_CLAUSE);

    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, CONFORMITY_CLAUSE);
  }
  free_soid_list (top_sl);
}

/**
@brief Mode check unit list 2.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_unit_list_2 (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T *top_sl = NO_SOID;
  if (MOID (x) != NO_MOID) {
    if (IS (MOID (x), FLEX_SYMBOL)) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (SUB_MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (IS (MOID (x), ROW_SYMBOL)) {
      SOID_T y2;
      make_soid (&y2, SORT (x), SLICE (MOID (x)), 0);
      mode_check_unit_list (&top_sl, SUB (p), &y2);
    } else if (IS (MOID (x), STRUCT_SYMBOL)) {
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

/**
@brief Mode check closed.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_closed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, SERIAL_CLAUSE)) {
    mode_check_serial_units (p, x, y, SERIAL_CLAUSE);
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, STOP)) {
    mode_check_closed (NEXT (p), x, y);
  }
  MOID (p) = MOID (y);
}

/**
@brief Mode check collateral.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_collateral (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (whether (p, BEGIN_SYMBOL, END_SYMBOL, STOP)
             || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, STOP)) {
    if (SORT (x) == STRONG) {
      MOID_T *z = (IS (MOID (x), FLEX_SYMBOL) ? SUB_MOID (x) : MOID (x));
      make_soid (y, STRONG, MODE (VACUUM), 0);
      if (SUB (z) != NO_MOID && HAS_ROWS (SUB (z))) {
        diagnostic_node (A68_ERROR, p, ERROR_VACUUM, "REF", MOID (x));
      }
    } else {
      make_soid (y, STRONG, MODE (UNDEFINED), 0);
    }
  } else {
    if (IS (p, UNIT_LIST)) {
      mode_check_unit_list_2 (p, x, y);
    } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, STOP)) {
      mode_check_collateral (NEXT (p), x, y);
    }
    MOID (p) = MOID (y);
  }
}

/**
@brief Mode check conditional 2.
@param ry Resulting soids.
@param p Node in syntax tree.
@param x Expected soid.
**/

static void mode_check_conditional_2 (SOID_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!is_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, ELSE_PART, CHOICE, STOP)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (is_one_of (p, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      mode_check_conditional_2 (ry, SUB (p), x);
    }
  }
}

/**
@brief Mode check conditional.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_conditional (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T *top_sl = NO_SOID;
  MOID_T *z;
  mode_check_conditional_2 (&top_sl, p, x);
  if (!is_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NO_MOID) {
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

/**
@brief Mode check int case 2.
@param ry Resulting soids.
@param p Node in syntax tree.
@param x Expected soid.
**/

static void mode_check_int_case_2 (SOID_T ** ry, NODE_T * p, SOID_T * x)
{
  SOID_T enq_expct, enq_yield;
  make_soid (&enq_expct, STRONG, MODE (INT), 0);
  mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
  if (!is_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
  }
  FORWARD (p);
  mode_check_unit_list (ry, NEXT_SUB (p), x);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      mode_check_serial (ry, NEXT_SUB (p), x, A68_TRUE);
    } else if (is_one_of (p, CASE_OUSE_PART, BRIEF_OUSE_PART, STOP)) {
      mode_check_int_case_2 (ry, SUB (p), x);
    }
  }
}

/**
@brief Mode check int case.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_int_case (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T *top_sl = NO_SOID;
  MOID_T *z;
  mode_check_int_case_2 (&top_sl, p, x);
  if (!is_balanced (p, top_sl, SORT (x))) {
    if (MOID (x) != NO_MOID) {
      make_soid (y, SORT (x), MOID (x), CASE_CLAUSE);
    } else {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    }
  } else {
    z = pack_soids_in_moid (top_sl, SERIES_MODE);
    make_soid (y, SORT (x), z, CASE_CLAUSE);
  }
  free_soid_list (top_sl);
}

/**
@brief Mode check loop 2.
@param p Node in syntax tree.
@param y Resulting soid.
**/

static void mode_check_loop_2 (NODE_T * p, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, FOR_PART)) {
    mode_check_loop_2 (NEXT (p), y);
  } else if (is_one_of (p, FROM_PART, BY_PART, TO_PART, STOP)) {
    SOID_T ix, iy;
    make_soid (&ix, STRONG, MODE (INT), 0);
    mode_check_unit (NEXT_SUB (p), &ix, &iy);
    if (!is_coercible_in_context (&iy, &ix, SAFE_DEFLEXING)) {
      cannot_coerce (NEXT_SUB (p), MOID (&iy), MODE (INT), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (IS (p, WHILE_PART)) {
    SOID_T enq_expct, enq_yield;
    make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
    mode_check_serial_units (NEXT_SUB (p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
    if (!is_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
    }
    mode_check_loop_2 (NEXT (p), y);
  } else if (is_one_of (p, DO_PART, ALT_DO_PART, STOP)) {
    SOID_T *z = NO_SOID;
    SOID_T ix;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&ix, STRONG, MODE (VOID), 0);
    if (IS (do_p, SERIAL_CLAUSE)) {
      mode_check_serial (&z, do_p, &ix, A68_TRUE);
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NO_NODE && IS (un_p, UNTIL_PART)) {
      SOID_T enq_expct, enq_yield;
      make_soid (&enq_expct, STRONG, MODE (BOOL), 0);
      mode_check_serial_units (NEXT_SUB (un_p), &enq_expct, &enq_yield, ENQUIRY_CLAUSE);
      if (!is_coercible_in_context (&enq_yield, &enq_expct, SAFE_DEFLEXING)) {
        cannot_coerce (un_p, MOID (&enq_yield), MOID (&enq_expct), MEEK, SAFE_DEFLEXING, ENQUIRY_CLAUSE);
      }
    }
    free_soid_list (z);
  }
}

/**
@brief Mode check loop.
@param p Node in syntax tree.
@param y Resulting soid.
**/

static void mode_check_loop (NODE_T * p, SOID_T * y)
{
  SOID_T *z = NO_SOID;
  mode_check_loop_2 (p, /* y */ z);
  make_soid (y, STRONG, MODE (VOID), 0);
}

/**
@brief Mode check enclosed.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

void mode_check_enclosed (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (IS (p, CLOSED_CLAUSE)) {
    mode_check_closed (SUB (p), x, y);
  } else if (IS (p, PARALLEL_CLAUSE)) {
    mode_check_collateral (SUB (NEXT_SUB (p)), x, y);
    make_soid (y, STRONG, MODE (VOID), 0);
    MOID (NEXT_SUB (p)) = MODE (VOID);
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    mode_check_collateral (SUB (p), x, y);
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    mode_check_conditional (SUB (p), x, y);
  } else if (IS (p, CASE_CLAUSE)) {
    mode_check_int_case (SUB (p), x, y);
  } else if (IS (p, CONFORMITY_CLAUSE)) {
    mode_check_united_case (SUB (p), x, y);
  } else if (IS (p, LOOP_CLAUSE)) {
    mode_check_loop (SUB (p), y);
  }
  MOID (p) = MOID (y);
}

/**
@brief Search table for operator.
@param t Tag chain to search.
@param n Name of operator.
@param x Lhs mode.
@param y Rhs mode.
@return Tag entry.
**/

static TAG_T *search_table_for_operator (TAG_T * t, char *n, MOID_T * x, MOID_T * y)
{
  if (is_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NO_MOID && is_mode_isnt_well (y)) {
    return (error_tag);
  }
  for (; t != NO_TAG; FORWARD (t)) {
    if (NSYMBOL (NODE (t)) == n) {
      PACK_T *p = PACK (MOID (t));
      if (is_coercible (x, MOID (p), FIRM, ALIAS_DEFLEXING)) {
        FORWARD (p);
        if (p == NO_PACK && y == NO_MOID) {
/* Matched in case of a monadic */
          return (t);
        } else if (p != NO_PACK && y != NO_MOID && is_coercible (y, MOID (p), FIRM, ALIAS_DEFLEXING)) {
/* Matched in case of a dyadic */
          return (t);
        }
      }
    }
  }
  return (NO_TAG);
}

/**
@brief Search chain of symbol tables and return matching operator "x n y" or "n x".
@param s Symbol table to start search.
@param n Name of token.
@param x Lhs mode.
@param y Rhs mode.
@return Tag entry.
**/

static TAG_T *search_table_chain_for_operator (TABLE_T * s, char * n, MOID_T * x, MOID_T * y)
{
  if (is_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NO_MOID && is_mode_isnt_well (y)) {
    return (error_tag);
  }
  while (s != NO_TABLE) {
    TAG_T *z = search_table_for_operator (OPERATORS (s), n, x, y);
    if (z != NO_TAG) {
      return (z);
    }
    BACKWARD (s);
  }
  return (NO_TAG);
}

/**
@brief Return a matching operator "x n y".
@param s Symbol table to start search.
@param n Name of token.
@param x Lhs mode.
@param y Rhs mode.
@return Tag entry.
**/

static TAG_T *find_operator (TABLE_T * s, char *n, MOID_T * x, MOID_T * y)
{
/* Coercions to operand modes are FIRM */
  TAG_T *z;
  MOID_T *u, *v;
/* (A) Catch exceptions first */
  if (x == NO_MOID && y == NO_MOID) {
    return (NO_TAG);
  } else if (is_mode_isnt_well (x)) {
    return (error_tag);
  } else if (y != NO_MOID && is_mode_isnt_well (y)) {
    return (error_tag);
  }
/* (B) MONADs */
  if (x != NO_MOID && y == NO_MOID) {
    z = search_table_chain_for_operator (s, n, x, NO_MOID);
    if (z != NO_TAG) {
      return (z);
    } else {
/* (B.2) A little trick to allow - (0, 1) or ABS (1, long pi) */
      if (is_coercible (x, MODE (COMPLEX), STRONG, SAFE_DEFLEXING)) {
        z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (COMPLEX), NO_MOID);
        if (z != NO_TAG) {
          return (z);
        }
      }
      if (is_coercible (x, MODE (LONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
        z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (LONG_COMPLEX), NO_MOID);
        if (z != NO_TAG) {
          return (z);
        }
      }
      if (is_coercible (x, MODE (LONGLONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
        z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (LONGLONG_COMPLEX), NO_MOID);
      }
    }
    return (NO_TAG);
  }
/* (C) DYADs */
  z = search_table_chain_for_operator (s, n, x, y);
  if (z != NO_TAG) {
    return (z);
  }
/* (C.2) Vector and matrix "strong coercions" in standard environ */
  u = depref_completely (x);
  v = depref_completely (y);
  if ((u == MODE (ROW_REAL) || u == MODE (ROWROW_REAL))
      || (v == MODE (ROW_REAL) || v == MODE (ROWROW_REAL))
      || (u == MODE (ROW_COMPLEX) || u == MODE (ROWROW_COMPLEX))
      || (v == MODE (ROW_COMPLEX) || v == MODE (ROWROW_COMPLEX))) {
    if (u == MODE (INT)) {
      z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (REAL), y);
      if (z != NO_TAG) {
        return (z);
      }
      z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (COMPLEX), y);
      if (z != NO_TAG) {
        return (z);
      }
    } else if (v == MODE (INT)) {
      z = search_table_for_operator (OPERATORS (a68g_standenv), n, x, MODE (REAL));
      if (z != NO_TAG) {
        return (z);
      }
      z = search_table_for_operator (OPERATORS (a68g_standenv), n, x, MODE (COMPLEX));
      if (z != NO_TAG) {
        return (z);
      }
    } else if (u == MODE (REAL)) {
      z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (COMPLEX), y);
      if (z != NO_TAG) {
        return (z);
      }
    } else if (v == MODE (REAL)) {
      z = search_table_for_operator (OPERATORS (a68g_standenv), n, x, MODE (COMPLEX));
      if (z != NO_TAG) {
        return (z);
      }
    }
  }
/* (C.3) Look in standenv for an appropriate cross-term */
  u = make_series_from_moids (x, y);
  u = make_united_mode (u);
  v = get_balanced_mode (u, STRONG, NO_DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (OPERATORS (a68g_standenv), n, v, v);
  if (z != NO_TAG) {
    return (z);
  }
  if (is_coercible_series (u, MODE (REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (REAL), MODE (REAL));
    if (z != NO_TAG) {
      return (z);
    }
  }
  if (is_coercible_series (u, MODE (LONG_REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (LONG_REAL), MODE (LONG_REAL));
    if (z != NO_TAG) {
      return (z);
    }
  }
  if (is_coercible_series (u, MODE (LONGLONG_REAL), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (LONGLONG_REAL), MODE (LONGLONG_REAL));
    if (z != NO_TAG) {
      return (z);
    }
  }
  if (is_coercible_series (u, MODE (COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (COMPLEX), MODE (COMPLEX));
    if (z != NO_TAG) {
      return (z);
    }
  }
  if (is_coercible_series (u, MODE (LONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (LONG_COMPLEX), MODE (LONG_COMPLEX));
    if (z != NO_TAG) {
      return (z);
    }
  }
  if (is_coercible_series (u, MODE (LONGLONG_COMPLEX), STRONG, SAFE_DEFLEXING)) {
    z = search_table_for_operator (OPERATORS (a68g_standenv), n, MODE (LONGLONG_COMPLEX), MODE (LONGLONG_COMPLEX));
    if (z != NO_TAG) {
      return (z);
    }
  }
/* (C.4) Now allow for depreffing for REF REAL +:= INT and alike */
  v = get_balanced_mode (u, STRONG, DEPREF, SAFE_DEFLEXING);
  z = search_table_for_operator (OPERATORS (a68g_standenv), n, v, v);
  if (z != NO_TAG) {
    return (z);
  }
  return (NO_TAG);
}

/**
@brief Mode check monadic operator.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_monadic_operator (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p != NO_NODE) {
    TAG_T *t;
    MOID_T *u;
    u = determine_unique_mode (y, SAFE_DEFLEXING);
    if (is_mode_isnt_well (u)) {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (u == MODE (HIP)) {
      diagnostic_node (A68_ERROR, NEXT (p), ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else {
      if (a68g_strchr (NOMADS, *(NSYMBOL (p))) != NO_TEXT) {
        t = NO_TAG;
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_OPERATOR_INVALID, NOMADS);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      } else {
        t = find_operator (TABLE (p), NSYMBOL (p), u, NO_MOID);
        if (t == NO_TAG) {
          diagnostic_node (A68_ERROR, p, ERROR_NO_MONADIC, u);
          make_soid (y, SORT (x), MODE (ERROR), 0);
        }
      }
      if (t != NO_TAG) {
        MOID (p) = MOID (t);
      }
      TAX (p) = t;
      if (t != NO_TAG && t != error_tag) {
        MOID (p) = MOID (t);
        make_soid (y, SORT (x), SUB_MOID (t), 0);
      } else {
        MOID (p) = MODE (ERROR);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
    }
  }
}

/**
@brief Mode check monadic formula.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_monadic_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e;
  make_soid (&e, FIRM, NO_MOID, 0);
  mode_check_formula (NEXT (p), &e, y);
  mode_check_monadic_operator (p, &e, y);
  make_soid (y, SORT (x), MOID (y), 0);
}

/**
@brief Mode check formula.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_formula (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T ls, rs;
  TAG_T *op;
  MOID_T *u, *v;
  if (IS (p, MONADIC_FORMULA)) {
    mode_check_monadic_formula (SUB (p), x, &ls);
  } else if (IS (p, FORMULA)) {
    mode_check_formula (SUB (p), x, &ls);
  } else if (IS (p, SECONDARY)) {
    SOID_T e;
    make_soid (&e, FIRM, NO_MOID, 0);
    mode_check_unit (SUB (p), &e, &ls);
  }
  u = determine_unique_mode (&ls, SAFE_DEFLEXING);
  MOID (p) = u;
  if (NEXT (p) == NO_NODE) {
    make_soid (y, SORT (x), u, 0);
  } else {
    NODE_T *q = NEXT_NEXT (p);
    if (IS (q, MONADIC_FORMULA)) {
      mode_check_monadic_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (IS (q, FORMULA)) {
      mode_check_formula (SUB (NEXT_NEXT (p)), x, &rs);
    } else if (IS (q, SECONDARY)) {
      SOID_T e;
      make_soid (&e, FIRM, NO_MOID, 0);
      mode_check_unit (SUB (q), &e, &rs);
    }
    v = determine_unique_mode (&rs, SAFE_DEFLEXING);
    MOID (q) = v;
    if (is_mode_isnt_well (u) || is_mode_isnt_well (v)) {
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (u == MODE (HIP)) {
      diagnostic_node (A68_ERROR, p, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else if (v == MODE (HIP)) {
      diagnostic_node (A68_ERROR, q, ERROR_INVALID_OPERAND, u);
      make_soid (y, SORT (x), MODE (ERROR), 0);
    } else {
      op = find_operator (TABLE (NEXT (p)), NSYMBOL (NEXT (p)), u, v);
      if (op == NO_TAG) {
        diagnostic_node (A68_ERROR, NEXT (p), ERROR_NO_DYADIC, u, v);
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
      if (op != NO_TAG) {
        MOID (NEXT (p)) = MOID (op);
      }
      TAX (NEXT (p)) = op;
      if (op != NO_TAG && op != error_tag) {
        make_soid (y, SORT (x), SUB_MOID (op), 0);
      } else {
        make_soid (y, SORT (x), MODE (ERROR), 0);
      }
    }
  }
}

/**
@brief Mode check assignation.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_assignation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T name, tmp, value;
  MOID_T *name_moid, *source_moid, *dest_moid, *ori;
/* Get destination mode */
  make_soid (&name, SOFT, NO_MOID, 0);
  mode_check_unit (SUB (p), &name, &tmp);
  dest_moid = MOID (&tmp);
/* SOFT coercion */
  ori = determine_unique_mode (&tmp, SAFE_DEFLEXING);
  name_moid = deproc_completely (ori);
  if (ATTRIBUTE (name_moid) != REF_SYMBOL) {
    if (IF_MODE_IS_WELL (name_moid)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_NAME, ori, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (p) = name_moid;
/* Get source mode */
  make_soid (&name, STRONG, SUB (name_moid), 0);
  mode_check_unit (NEXT_NEXT (p), &name, &value);
  if (!is_coercible_in_context (&value, &name, FORCE_DEFLEXING)) {
    source_moid = MOID (&value);
    cannot_coerce (p, MOID (&value), MOID (&name), STRONG, FORCE_DEFLEXING, UNIT);
    make_soid (y, SORT (x), MODE (ERROR), 0);
  } else {
    make_soid (y, SORT (x), name_moid, 0);
  }
}

/**
@brief Mode check identity relation.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_identity_relation (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  MOID_T *lhs, *rhs, *oril, *orir;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, SOFT, NO_MOID, 0);
  mode_check_unit (SUB (ln), &e, &l);
  mode_check_unit (SUB (rn), &e, &r);
/* SOFT coercion */
  oril = determine_unique_mode (&l, SAFE_DEFLEXING);
  orir = determine_unique_mode (&r, SAFE_DEFLEXING);
  lhs = deproc_completely (oril);
  rhs = deproc_completely (orir);
  if (IF_MODE_IS_WELL (lhs) && lhs != MODE (HIP) && ATTRIBUTE (lhs) != REF_SYMBOL) {
    diagnostic_node (A68_ERROR, ln, ERROR_NO_NAME, oril, ATTRIBUTE (SUB (ln)));
    lhs = MODE (ERROR);
  }
  if (IF_MODE_IS_WELL (rhs) && rhs != MODE (HIP) && ATTRIBUTE (rhs) != REF_SYMBOL) {
    diagnostic_node (A68_ERROR, rn, ERROR_NO_NAME, orir, ATTRIBUTE (SUB (rn)));
    rhs = MODE (ERROR);
  }
  if (lhs == MODE (HIP) && rhs == MODE (HIP)) {
    diagnostic_node (A68_ERROR, p, ERROR_NO_UNIQUE_MODE);
  }
  if (is_coercible (lhs, rhs, STRONG, SAFE_DEFLEXING)) {
    lhs = rhs;
  } else if (is_coercible (rhs, lhs, STRONG, SAFE_DEFLEXING)) {
    rhs = lhs;
  } else {
    cannot_coerce (NEXT (p), rhs, lhs, SOFT, SKIP_DEFLEXING, TERTIARY);
    lhs = rhs = MODE (ERROR);
  }
  MOID (ln) = lhs;
  MOID (rn) = rhs;
  make_soid (y, SORT (x), MODE (BOOL), 0);
}

/**
@brief Mode check bool functions ANDF and ORF.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_bool_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T e, l, r;
  NODE_T *ln = p, *rn = NEXT_NEXT (p);
  make_soid (&e, STRONG, MODE (BOOL), 0);
  mode_check_unit (SUB (ln), &e, &l);
  if (!is_coercible_in_context (&l, &e, SAFE_DEFLEXING)) {
    cannot_coerce (ln, MOID (&l), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  mode_check_unit (SUB (rn), &e, &r);
  if (!is_coercible_in_context (&r, &e, SAFE_DEFLEXING)) {
    cannot_coerce (rn, MOID (&r), MOID (&e), MEEK, SAFE_DEFLEXING, TERTIARY);
  }
  MOID (ln) = MODE (BOOL);
  MOID (rn) = MODE (BOOL);
  make_soid (y, SORT (x), MODE (BOOL), 0);
}

/**
@brief Mode check cast.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_cast (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w;
  mode_check_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  CAST (&w) = A68_TRUE;
  mode_check_enclosed (SUB_NEXT (p), &w, y);
  if (!is_coercible_in_context (y, &w, SAFE_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (y), MOID (&w), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
  }
  make_soid (y, SORT (x), MOID (p), 0);
}

/**
@brief Mode check assertion.
@param p Node in syntax tree.
**/

static void mode_check_assertion (NODE_T * p)
{
  SOID_T w, y;
  make_soid (&w, STRONG, MODE (BOOL), 0);
  mode_check_enclosed (SUB_NEXT (p), &w, &y);
  SORT (&y) = SORT (&w); /* Patch */
  if (!is_coercible_in_context (&y, &w, NO_DEFLEXING)) {
    cannot_coerce (NEXT (p), MOID (&y), MOID (&w), MEEK, NO_DEFLEXING, ENCLOSED_CLAUSE);
  }
}

/**
@brief Mode check argument list.
@param r Resulting soids.
@param p Node in syntax tree.
@param x Proc argument pack.
@param v Partial locale pack.
@param w Partial proc pack.
**/

static void mode_check_argument_list (SOID_T ** r, NODE_T * p, PACK_T ** x, PACK_T ** v, PACK_T ** w)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, GENERIC_ARGUMENT_LIST)) {
      ATTRIBUTE (p) = ARGUMENT_LIST;
    }
    if (IS (p, ARGUMENT_LIST)) {
      mode_check_argument_list (r, SUB (p), x, v, w);
    } else if (IS (p, UNIT)) {
      SOID_T y, z;
      if (*x != NO_PACK) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MOID (*x), NO_TEXT, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NO_MOID, 0);
      }
      mode_check_unit (p, &z, &y);
      add_to_soid_list (r, p, &y);
    } else if (IS (p, TRIMMER)) {
      SOID_T z;
      if (SUB (p) != NO_NODE) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, ARGUMENT);
        make_soid (&z, STRONG, MODE (ERROR), 0);
        add_mode_to_pack_end (v, MODE (VOID), NO_TEXT, p);
        add_mode_to_pack_end (w, MOID (*x), NO_TEXT, p);
        FORWARD (*x);
      } else if (*x != NO_PACK) {
        make_soid (&z, STRONG, MOID (*x), 0);
        add_mode_to_pack_end (v, MODE (VOID), NO_TEXT, p);
        add_mode_to_pack_end (w, MOID (*x), NO_TEXT, p);
        FORWARD (*x);
      } else {
        make_soid (&z, STRONG, NO_MOID, 0);
      }
      add_to_soid_list (r, p, &z);
    } else if (IS (p, SUB_SYMBOL) && !OPTION_BRACKETS (&program)) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX, CALL);
    }
  }
}

/**
@brief Mode check argument list 2.
@param p Node in syntax tree.
@param x Proc argument pack.
@param y Soid.
@param v Partial locale pack.
@param w Partial proc pack.
**/

static void mode_check_argument_list_2 (NODE_T * p, PACK_T * x, SOID_T * y, PACK_T ** v, PACK_T ** w)
{
  SOID_T *top_sl = NO_SOID;
  mode_check_argument_list (&top_sl, SUB (p), &x, v, w);
  make_soid (y, STRONG, pack_soids_in_moid (top_sl, STOWED_MODE), 0);
  free_soid_list (top_sl);
}

/**
@brief Mode check meek int.
@param p Node in syntax tree.
**/

static void mode_check_meek_int (NODE_T * p)
{
  SOID_T x, y;
  make_soid (&x, STRONG, MODE (INT), 0);
  mode_check_unit (p, &x, &y);
  if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
    cannot_coerce (p, MOID (&y), MOID (&x), MEEK, SAFE_DEFLEXING, 0);
  }
}

/**
@brief Mode check trimmer.
@param p Node in syntax tree.
**/

static void mode_check_trimmer (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, TRIMMER)) {
    mode_check_trimmer (SUB (p));
  } else if (IS (p, UNIT)) {
    mode_check_meek_int (p);
    mode_check_trimmer (NEXT (p));
  } else {
    mode_check_trimmer (NEXT (p));
  }
}

/**
@brief Mode check indexer.
@param p Node in syntax tree.
@param subs Subscript counter.
@param trims Trimmer counter.
**/

static void mode_check_indexer (NODE_T * p, int *subs, int *trims)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, TRIMMER)) {
    (*trims)++;
    mode_check_trimmer (SUB (p));
  } else if (IS (p, UNIT)) {
    (*subs)++;
    mode_check_meek_int (p);
  } else {
    mode_check_indexer (SUB (p), subs, trims);
    mode_check_indexer (NEXT (p), subs, trims);
  }
}

/**
@brief Mode check call.
@param p Node in syntax tree.
@param n Mode.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_call (NODE_T * p, MOID_T * n, SOID_T * x, SOID_T * y)
{
  SOID_T d;
  MOID (p) = n;
/* "partial_locale" is the mode of the locale */
  PARTIAL_LOCALE (GINFO (p)) = new_moid ();
  ATTRIBUTE (PARTIAL_LOCALE (GINFO (p))) = PROC_SYMBOL;
  PACK (PARTIAL_LOCALE (GINFO (p))) = NO_PACK;
  SUB (PARTIAL_LOCALE (GINFO (p))) = SUB (n);
/* "partial_proc" is the mode of the resulting proc */
  PARTIAL_PROC (GINFO (p)) = new_moid ();
  ATTRIBUTE (PARTIAL_PROC (GINFO (p))) = PROC_SYMBOL;
  PACK (PARTIAL_PROC (GINFO (p))) = NO_PACK;
  SUB (PARTIAL_PROC (GINFO (p))) = SUB (n);
/* Check arguments and construct modes */
  mode_check_argument_list_2 (NEXT (p), PACK (n), &d, &PACK (PARTIAL_LOCALE (GINFO (p))), &PACK (PARTIAL_PROC (GINFO (p))));
  DIM (PARTIAL_PROC (GINFO (p))) = count_pack_members (PACK (PARTIAL_PROC (GINFO (p))));
  DIM (PARTIAL_LOCALE (GINFO (p))) = count_pack_members (PACK (PARTIAL_LOCALE (GINFO (p))));
  PARTIAL_PROC (GINFO (p)) = register_extra_mode (&TOP_MOID (&program), PARTIAL_PROC (GINFO (p)));
  PARTIAL_LOCALE (GINFO (p)) = register_extra_mode (&TOP_MOID (&program), PARTIAL_LOCALE (GINFO (p)));
  if (DIM (MOID (&d)) != DIM (n)) {
    diagnostic_node (A68_ERROR, p, ERROR_ARGUMENT_NUMBER, n);
    make_soid (y, SORT (x), SUB (n), 0);
/*  make_soid (y, SORT (x), MODE (ERROR), 0); */
  } else {
    if (!is_coercible (MOID (&d), n, STRONG, ALIAS_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), n, STRONG, ALIAS_DEFLEXING, ARGUMENT);
    }
    if (DIM (PARTIAL_PROC (GINFO (p))) == 0) {
      make_soid (y, SORT (x), SUB (n), 0);
    } else {
      if (OPTION_PORTCHECK (&program)) {
        diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, NEXT (p), WARNING_EXTENSION);
      }
      make_soid (y, SORT (x), PARTIAL_PROC (GINFO (p)), 0);
    }
  }
}

/**
@brief Mode check slice.
@param p Node in syntax tree.
@param ori Original MODE.
@param x Expected soid.
@param y Resulting soid.
@return Whether construct is a CALL or a SLICE.
**/

static void mode_check_slice (NODE_T * p, MOID_T * ori, SOID_T * x, SOID_T * y)
{
  BOOL_T is_ref;
  int rowdim, subs, trims;
  MOID_T *m = depref_completely (ori), *n = ori;
/* WEAK coercion */
  while ((IS (n, REF_SYMBOL) && !is_ref_row (n)) || (IS (n, PROC_SYMBOL) && PACK (n) == NO_PACK)) {
    n = depref_once (n);
  }
  if (n == NO_MOID || !(SLICE (DEFLEX (n)) != NO_MOID || is_ref_row (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_ROW_OR_PROC, n, ATTRIBUTE (SUB (p)));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
  }

  MOID (p) = n;
  subs = trims = 0;
  mode_check_indexer (SUB_NEXT (p), &subs, &trims);
  if ((is_ref = is_ref_row (n)) != 0) {
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
      if (is_ref) {
        m = NAME (m);
      } else {
        if (IS (m, FLEX_SYMBOL)) {
          m = SUB (m);
        }
        m = SLICE (m);
      }
      ABEND (m == NO_MOID, "No mode in mode_check_slice", NO_TEXT);
      subs--;
    }
/* A trim cannot be but deflexed */
    if (ANNOTATION (NEXT (p)) == TRIMMER && TRIM (m) != NO_MOID) {
      ABEND (TRIM (m) == NO_MOID, ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
      make_soid (y, SORT (x), TRIM (m), 0);
    } else {
      make_soid (y, SORT (x), m, 0);
    }
  }
}

/**
@brief Mode check specification.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
@return Whether construct is a CALL or SLICE.
**/

static int mode_check_specification (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  MOID_T *m, *ori;
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (SUB (p), &w, &d);
  ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  m = depref_completely (ori);
  if (IS (m, PROC_SYMBOL)) {
/* Assume CALL */
    mode_check_call (p, m, x, y);
    return (CALL);
  } else if (IS (m, ROW_SYMBOL) || IS (m, FLEX_SYMBOL)) {
/* Assume SLICE */
    mode_check_slice (p, ori, x, y);
    return (SLICE);
  } else {
    if (m != MODE (ERROR)) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_MODE_SPECIFICATION, m);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return (PRIMARY);
  }
}

/**
@brief Mode check selection.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_selection (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  BOOL_T coerce;
  MOID_T *n, *str, *ori;
  PACK_T *t, *t_2;
  char *fs;
  BOOL_T deflex = A68_FALSE;
  NODE_T *secondary = SUB_NEXT (p);
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (secondary, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  coerce = A68_TRUE;
  while (coerce) {
    if (IS (n, STRUCT_SYMBOL)) {
      coerce = A68_FALSE;
      t = PACK (n);
    } else if (IS (n, REF_SYMBOL) && (IS (SUB (n), ROW_SYMBOL) || IS (SUB (n), FLEX_SYMBOL)) && MULTIPLE (n) != NO_MOID) {
      coerce = A68_FALSE;
      deflex = A68_TRUE;
      t = PACK (MULTIPLE (n));
    } else if ((IS (n, ROW_SYMBOL) || IS (n, FLEX_SYMBOL)) && MULTIPLE (n) != NO_MOID) {
      coerce = A68_FALSE;
      deflex = A68_TRUE;
      t = PACK (MULTIPLE (n));
    } else if (IS (n, REF_SYMBOL) && is_name_struct (n)) {
      coerce = A68_FALSE;
      t = PACK (NAME (n));
    } else if (is_deprefable (n)) {
      coerce = A68_TRUE;
      n = SUB (n);
      t = NO_PACK;
    } else {
      coerce = A68_FALSE;
      t = NO_PACK;
    }
  }
  if (t == NO_PACK) {
    if (IF_MODE_IS_WELL (MOID (&d))) {
      diagnostic_node (A68_ERROR, secondary, ERROR_NO_STRUCT, ori, ATTRIBUTE (secondary));
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  MOID (NEXT (p)) = n;
  fs = NSYMBOL (SUB (p));
  str = n;
  while (IS (str, REF_SYMBOL)) {
    str = SUB (str);
  }
  if (IS (str, FLEX_SYMBOL)) {
    str = SUB (str);
  }
  if (IS (str, ROW_SYMBOL)) {
    str = SUB (str);
  }
  t_2 = PACK (str);
  while (t != NO_PACK && t_2 != NO_PACK) {
    if (TEXT (t) == fs) {
      MOID_T *ret = MOID (t);
      if (deflex && TRIM (ret) != NO_MOID) {
        ret = TRIM (ret);
      }
      make_soid (y, SORT (x), ret, 0);
      MOID (p) = ret;
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

/**
@brief Mode check diagonal.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_diagonal (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T is_ref;
  if (IS (p, TERTIARY)) {
    make_soid (&w, STRONG, MODE (INT), 0);
    mode_check_unit (p, &w, &d);
    if (!is_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (IS (n, REF_SYMBOL) && !is_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NO_MOID && (IS (n, FLEX_SYMBOL) || IS_REF_FLEX (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NO_MOID || !(SLICE (DEFLEX (n)) != NO_MOID || is_ref_row (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((is_ref = is_ref_row (n)) != A68_FALSE) {
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
  if (is_ref) {
    n = NAME (n);
    ABEND (ISNT (n, REF_SYMBOL), "mode table error", PM (n));
  } else {
    n = SLICE (n);
  }
  ABEND (n == NO_MOID, "No mode in mode_check_diagonal", NO_TEXT);
  make_soid (y, SORT (x), n, 0);
}

/**
@brief Mode check transpose.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_transpose (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert = NEXT (p);
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T is_ref;
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (IS (n, REF_SYMBOL) && !is_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NO_MOID && (IS (n, FLEX_SYMBOL) || IS_REF_FLEX (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NO_MOID || !(SLICE (DEFLEX (n)) != NO_MOID || is_ref_row (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_MATRIX, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((is_ref = is_ref_row (n)) != A68_FALSE) {
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
  ABEND (n == NO_MOID, "No mode in mode_check_transpose", NO_TEXT);
  make_soid (y, SORT (x), n, 0);
}

/**
@brief Mode check row or column function.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_row_column_function (NODE_T * p, SOID_T * x, SOID_T * y)
{
  SOID_T w, d;
  NODE_T *tert;
  MOID_T *n, *ori;
  int rowdim;
  BOOL_T is_ref;
  if (IS (p, TERTIARY)) {
    make_soid (&w, STRONG, MODE (INT), 0);
    mode_check_unit (p, &w, &d);
    if (!is_coercible_in_context (&d, &w, SAFE_DEFLEXING)) {
      cannot_coerce (p, MOID (&d), MOID (&w), MEEK, SAFE_DEFLEXING, 0);
    }
    tert = NEXT_NEXT (p);
  } else {
    tert = NEXT (p);
  }
  make_soid (&w, WEAK, NO_MOID, 0);
  mode_check_unit (tert, &w, &d);
  n = ori = determine_unique_mode (&d, SAFE_DEFLEXING);
  while (IS (n, REF_SYMBOL) && !is_ref_row (n)) {
    n = depref_once (n);
  }
  if (n != NO_MOID && (IS (n, FLEX_SYMBOL) || IS_REF_FLEX (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_FLEX_ARGUMENT, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if (n == NO_MOID || !(SLICE (DEFLEX (n)) != NO_MOID || is_ref_row (n))) {
    if (IF_MODE_IS_WELL (n)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_VECTOR, ori, TERTIARY);
    }
    make_soid (y, SORT (x), MODE (ERROR), 0);
    return;
  }
  if ((is_ref = is_ref_row (n)) != A68_FALSE) {
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
  ABEND (n == NO_MOID, "No mode in mode_check_diagonal", NO_TEXT);
  make_soid (y, SORT (x), ROWED (n), 0);
}

/**
@brief Mode check format text.
@param p Node in syntax tree.
**/

static void mode_check_format_text (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    mode_check_format_text (SUB (p));
    if (IS (p, FORMAT_PATTERN)) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (FORMAT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (ROW_INT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    } else if (IS (p, DYNAMIC_REPLICATOR)) {
      SOID_T x, y;
      make_soid (&x, STRONG, MODE (INT), 0);
      mode_check_enclosed (SUB (NEXT_SUB (p)), &x, &y);
      if (!is_coercible_in_context (&y, &x, SAFE_DEFLEXING)) {
        cannot_coerce (p, MOID (&y), MOID (&x), STRONG, SAFE_DEFLEXING, ENCLOSED_CLAUSE);
      }
    }
  }
}

/**
@brief Mode check unit.
@param p Node in syntax tree.
@param x Expected soid.
@param y Resulting soid.
**/

static void mode_check_unit (NODE_T * p, SOID_T * x, SOID_T * y)
{
  if (p == NO_NODE) {
    return;
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, STOP)) {
    mode_check_unit (SUB (p), x, y);
/* Ex primary */
  } else if (IS (p, SPECIFICATION)) {
    ATTRIBUTE (p) = mode_check_specification (SUB (p), x, y);
    warn_for_voiding (p, x, y, ATTRIBUTE (p));
  } else if (IS (p, CAST)) {
    mode_check_cast (SUB (p), x, y);
    warn_for_voiding (p, x, y, CAST);
  } else if (IS (p, DENOTATION)) {
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, DENOTATION);
  } else if (IS (p, IDENTIFIER)) {
    if ((TAX (p) == NO_TAG) && (MOID (p) == NO_MOID)) {
      int att = first_tag_global (TABLE (p), NSYMBOL (p));
      if (att == STOP) {
        (void) add_tag (TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
        diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
        MOID (p) = MODE (ERROR);
      } else {
        TAG_T *z = find_tag_global (TABLE (p), att, NSYMBOL (p));
        if (att == IDENTIFIER && z != NO_TAG) {
          MOID (p) = MOID (z);
        } else {
          (void) add_tag (TABLE (p), IDENTIFIER, p, MODE (ERROR), NORMAL_IDENTIFIER);
          diagnostic_node (A68_ERROR, p, ERROR_UNDECLARED_TAG);
          MOID (p) = MODE (ERROR);
        }
      }
    }
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, IDENTIFIER);
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    mode_check_enclosed (SUB (p), x, y);
  } else if (IS (p, FORMAT_TEXT)) {
    mode_check_format_text (p);
    make_soid (y, SORT (x), MODE (FORMAT), 0);
    warn_for_voiding (p, x, y, FORMAT_TEXT);
/* Ex secondary */
  } else if (IS (p, GENERATOR)) {
    mode_check_declarer (SUB (p));
    make_soid (y, SORT (x), MOID (SUB (p)), 0);
    warn_for_voiding (p, x, y, GENERATOR);
  } else if (IS (p, SELECTION)) {
    mode_check_selection (SUB (p), x, y);
    warn_for_voiding (p, x, y, SELECTION);
/* Ex tertiary */
  } else if (IS (p, NIHIL)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  } else if (IS (p, FORMULA)) {
    mode_check_formula (p, x, y);
    if (ISNT (MOID (y), REF_SYMBOL)) {
      warn_for_voiding (p, x, y, FORMULA);
    }
  } else if (IS (p, DIAGONAL_FUNCTION)) {
    mode_check_diagonal (SUB (p), x, y);
    warn_for_voiding (p, x, y, DIAGONAL_FUNCTION);
  } else if (IS (p, TRANSPOSE_FUNCTION)) {
    mode_check_transpose (SUB (p), x, y);
    warn_for_voiding (p, x, y, TRANSPOSE_FUNCTION);
  } else if (IS (p, ROW_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, ROW_FUNCTION);
  } else if (IS (p, COLUMN_FUNCTION)) {
    mode_check_row_column_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, COLUMN_FUNCTION);
/* Ex unit */
  } else if (is_one_of (p, JUMP, SKIP, STOP)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  } else if (IS (p, ASSIGNATION)) {
    mode_check_assignation (SUB (p), x, y);
  } else if (IS (p, IDENTITY_RELATION)) {
    mode_check_identity_relation (SUB (p), x, y);
    warn_for_voiding (p, x, y, IDENTITY_RELATION);
  } else if (IS (p, ROUTINE_TEXT)) {
    mode_check_routine_text (SUB (p), y);
    make_soid (y, SORT (x), MOID (p), 0);
    warn_for_voiding (p, x, y, ROUTINE_TEXT);
  } else if (IS (p, ASSERTION)) {
    mode_check_assertion (SUB (p));
    make_soid (y, STRONG, MODE (VOID), 0);
  } else if (IS (p, AND_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, AND_FUNCTION);
  } else if (IS (p, OR_FUNCTION)) {
    mode_check_bool_function (SUB (p), x, y);
    warn_for_voiding (p, x, y, OR_FUNCTION);
  } else if (IS (p, CODE_CLAUSE)) {
    make_soid (y, STRONG, MODE (HIP), 0);
  }
  MOID (p) = MOID (y);
}

/**
@brief Coerce bounds.
@param p Node in syntax tree.
**/

static void coerce_bounds (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      SOID_T q;
      make_soid (&q, MEEK, MODE (INT), 0);
      coerce_unit (p, &q);
    } else {
      coerce_bounds (SUB (p));
    }
  }
}

/**
@brief Coerce declarer.
@param p Node in syntax tree.
**/

static void coerce_declarer (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, BOUNDS)) {
      coerce_bounds (SUB (p));
    } else {
      coerce_declarer (SUB (p));
    }
  }
}

/**
@brief Coerce identity declaration.
@param p Node in syntax tree.
**/

static void coerce_identity_declaration (NODE_T * p)
{
  if (p != NO_NODE) {
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

/**
@brief Coerce variable declaration.
@param p Node in syntax tree.
**/

static void coerce_variable_declaration (NODE_T * p)
{
  if (p != NO_NODE) {
    switch (ATTRIBUTE (p)) {
    case DECLARER:
      {
        coerce_declarer (SUB (p));
        coerce_variable_declaration (NEXT (p));
        break;
      }
    case DEFINING_IDENTIFIER:
      {
        if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP)) {
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

/**
@brief Coerce routine text.
@param p Node in syntax tree.
**/

static void coerce_routine_text (NODE_T * p)
{
  SOID_T w;
  if (IS (p, PARAMETER_PACK)) {
    FORWARD (p);
  }
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

/**
@brief Coerce proc declaration.
@param p Node in syntax tree.
**/

static void coerce_proc_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
  } else {
    coerce_proc_declaration (SUB (p));
    coerce_proc_declaration (NEXT (p));
  }
}

/**
@brief Coerce_op_declaration.
@param p Node in syntax tree.
**/

static void coerce_op_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, DEFINING_OPERATOR)) {
    SOID_T q;
    make_soid (&q, STRONG, MOID (p), 0);
    coerce_unit (NEXT_NEXT (p), &q);
  } else {
    coerce_op_declaration (SUB (p));
    coerce_op_declaration (NEXT (p));
  }
}

/**
@brief Coerce brief op declaration.
@param p Node in syntax tree.
**/

static void coerce_brief_op_declaration (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, DEFINING_OPERATOR)) {
    coerce_routine_text (SUB (NEXT_NEXT (p)));
  } else {
    coerce_brief_op_declaration (SUB (p));
    coerce_brief_op_declaration (NEXT (p));
  }
}

/**
@brief Coerce declaration list.
@param p Node in syntax tree.
**/

static void coerce_declaration_list (NODE_T * p)
{
  if (p != NO_NODE) {
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

/**
@brief Coerce serial.
@param p Node in syntax tree.
@param q Soid.
@param k Whether k yields value other than VOID.
**/

static void coerce_serial (NODE_T * p, SOID_T * q, BOOL_T k)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, INITIALISER_SERIES)) {
    coerce_serial (SUB (p), q, A68_FALSE);
    coerce_serial (NEXT (p), q, k);
  } else if (IS (p, DECLARATION_LIST)) {
    coerce_declaration_list (SUB (p));
  } else if (is_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, STOP)) {
    coerce_serial (NEXT (p), q, k);
  } else if (is_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, STOP)) {
    NODE_T *z = NEXT (p);
    if (z != NO_NODE) {
      if (IS (z, EXIT_SYMBOL) || IS (z, END_SYMBOL) || IS (z, CLOSE_SYMBOL) || IS (z, OCCA_SYMBOL)) {
        coerce_serial (SUB (p), q, A68_TRUE);
      } else {
        coerce_serial (SUB (p), q, A68_FALSE);
      }
    } else {
      coerce_serial (SUB (p), q, A68_TRUE);
    }
    coerce_serial (NEXT (p), q, k);
  } else if (IS (p, LABELED_UNIT)) {
    coerce_serial (SUB (p), q, k);
  } else if (IS (p, UNIT)) {
    if (k) {
      coerce_unit (p, q);
    } else {
      SOID_T strongvoid;
      make_soid (&strongvoid, STRONG, MODE (VOID), 0);
      coerce_unit (p, &strongvoid);
    }
  }
}

/**
@brief Coerce closed.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_closed (NODE_T * p, SOID_T * q)
{
  if (IS (p, SERIAL_CLAUSE)) {
    coerce_serial (p, q, A68_TRUE);
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, STOP)) {
    coerce_closed (NEXT (p), q);
  }
}

/**
@brief Coerce conditional.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_conditional (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (BOOL), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_serial (NEXT_SUB (p), q, A68_TRUE);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, ELSE_PART, CHOICE, STOP)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (is_one_of (p, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      coerce_conditional (SUB (p), q);
    }
  }
}

/**
@brief Coerce unit list.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_unit_list (NODE_T * p, SOID_T * q)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT_LIST)) {
    coerce_unit_list (SUB (p), q);
    coerce_unit_list (NEXT (p), q);
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, STOP)) {
    coerce_unit_list (NEXT (p), q);
  } else if (IS (p, UNIT)) {
    coerce_unit (p, q);
    coerce_unit_list (NEXT (p), q);
  }
}

/**
@brief Coerce int case.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_int_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (INT), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (is_one_of (p, CASE_OUSE_PART, BRIEF_OUSE_PART, STOP)) {
      coerce_int_case (SUB (p), q);
    }
  }
}

/**
@brief Coerce spec unit list.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_spec_unit_list (NODE_T * p, SOID_T * q)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (is_one_of (p, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, STOP)) {
      coerce_spec_unit_list (SUB (p), q);
    } else if (IS (p, UNIT)) {
      coerce_unit (p, q);
    }
  }
}

/**
@brief Coerce united case.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_united_case (NODE_T * p, SOID_T * q)
{
  SOID_T w;
  make_soid (&w, MEEK, MOID (SUB (p)), 0);
  coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
  FORWARD (p);
  coerce_spec_unit_list (NEXT_SUB (p), q);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      coerce_serial (NEXT_SUB (p), q, A68_TRUE);
    } else if (is_one_of (p, CONFORMITY_OUSE_PART, BRIEF_CONFORMITY_OUSE_PART, STOP)) {
      coerce_united_case (SUB (p), q);
    }
  }
}

/**
@brief Coerce loop.
@param p Node in syntax tree.
**/

static void coerce_loop (NODE_T * p)
{
  if (IS (p, FOR_PART)) {
    coerce_loop (NEXT (p));
  } else if (is_one_of (p, FROM_PART, BY_PART, TO_PART, STOP)) {
    SOID_T w;
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (NEXT_SUB (p), &w);
    coerce_loop (NEXT (p));
  } else if (IS (p, WHILE_PART)) {
    SOID_T w;
    make_soid (&w, MEEK, MODE (BOOL), 0);
    coerce_serial (NEXT_SUB (p), &w, A68_TRUE);
    coerce_loop (NEXT (p));
  } else if (is_one_of (p, DO_PART, ALT_DO_PART, STOP)) {
    SOID_T w;
    NODE_T *do_p = NEXT_SUB (p), *un_p;
    make_soid (&w, STRONG, MODE (VOID), 0);
    coerce_serial (do_p, &w, A68_TRUE);
    if (IS (do_p, SERIAL_CLAUSE)) {
      un_p = NEXT (do_p);
    } else {
      un_p = do_p;
    }
    if (un_p != NO_NODE && IS (un_p, UNTIL_PART)) {
      SOID_T sw;
      make_soid (&sw, MEEK, MODE (BOOL), 0);
      coerce_serial (NEXT_SUB (un_p), &sw, A68_TRUE);
    }
  }
}

/**
@brief Coerce struct display.
@param r Pack.
@param p Node in syntax tree.
**/

static void coerce_struct_display (PACK_T ** r, NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT_LIST)) {
    coerce_struct_display (r, SUB (p));
    coerce_struct_display (r, NEXT (p));
  } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, COMMA_SYMBOL, STOP)) {
    coerce_struct_display (r, NEXT (p));
  } else if (IS (p, UNIT)) {
    SOID_T s;
    make_soid (&s, STRONG, MOID (*r), 0);
    coerce_unit (p, &s);
    FORWARD (*r);
    coerce_struct_display (r, NEXT (p));
  }
}

/**
@brief Coerce collateral.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_collateral (NODE_T * p, SOID_T * q)
{
  if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, STOP) || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, STOP))) {
    if (IS (MOID (q), STRUCT_SYMBOL)) {
      PACK_T *t = PACK (MOID (q));
      coerce_struct_display (&t, p);
    } else if (IS (MOID (q), FLEX_SYMBOL)) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (SUB_MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else if (IS (MOID (q), ROW_SYMBOL)) {
      SOID_T w;
      make_soid (&w, STRONG, SLICE (MOID (q)), 0);
      coerce_unit_list (p, &w);
    } else {
/* if (MOID (q) != MODE (VOID)) */
      coerce_unit_list (p, q);
    }
  }
}

/**
@brief Coerce_enclosed.
@param p Node in syntax tree.
@param q Soid.
**/

void coerce_enclosed (NODE_T * p, SOID_T * q)
{
  if (IS (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (SUB (p), q);
  } else if (IS (p, CLOSED_CLAUSE)) {
    coerce_closed (SUB (p), q);
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    coerce_collateral (SUB (p), q);
  } else if (IS (p, PARALLEL_CLAUSE)) {
    coerce_collateral (SUB (NEXT_SUB (p)), q);
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    coerce_conditional (SUB (p), q);
  } else if (IS (p, CASE_CLAUSE)) {
    coerce_int_case (SUB (p), q);
  } else if (IS (p, CONFORMITY_CLAUSE)) {
    coerce_united_case (SUB (p), q);
  } else if (IS (p, LOOP_CLAUSE)) {
    coerce_loop (SUB (p));
  }
  MOID (p) = depref_rows (MOID (p), MOID (q));
}

/**
@brief Get monad moid.
@param p Node in syntax tree.
**/

static MOID_T *get_monad_moid (NODE_T * p)
{
  if (TAX (p) != NO_TAG && TAX (p) != error_tag) {
    MOID (p) = MOID (TAX (p));
    return (MOID (PACK (MOID (p))));
  } else {
    return (MODE (ERROR));
  }
}

/**
@brief Coerce monad oper.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_monad_oper (NODE_T * p, SOID_T * q)
{
  if (p != NO_NODE) {
    SOID_T z;
    make_soid (&z, FIRM, MOID (PACK (MOID (TAX (p)))), 0);
    INSERT_COERCIONS (NEXT (p), MOID (q), &z);
  }
}

/**
@brief Coerce monad formula.
@param p Node in syntax tree.
**/

static void coerce_monad_formula (NODE_T * p)
{
  SOID_T e;
  make_soid (&e, STRONG, get_monad_moid (p), 0);
  coerce_operand (NEXT (p), &e);
  coerce_monad_oper (p, &e);
}

/**
@brief Coerce operand.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_operand (NODE_T * p, SOID_T * q)
{
  if (IS (p, MONADIC_FORMULA)) {
    coerce_monad_formula (SUB (p));
    if (MOID (p) != MOID (q)) {
      make_sub (p, p, FORMULA);
      INSERT_COERCIONS (p, MOID (p), q);
      make_sub (p, p, TERTIARY);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, SECONDARY)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
  }
}

/**
@brief Coerce formula.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_formula (NODE_T * p, SOID_T * q)
{
  (void) q;
  if (IS (p, MONADIC_FORMULA) && NEXT (p) == NO_NODE) {
    coerce_monad_formula (SUB (p));
  } else {
    if (TAX (NEXT (p)) != NO_TAG && TAX (NEXT (p)) != error_tag) {
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

/**
@brief Coerce assignation.
@param p Node in syntax tree.
**/

static void coerce_assignation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, SOFT, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, SUB_MOID (p), 0);
  coerce_unit (NEXT_NEXT (p), &w);
}

/**
@brief Coerce relation.
@param p Node in syntax tree.
**/

static void coerce_relation (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_unit (SUB (p), &w);
  make_soid (&w, STRONG, MOID (NEXT_NEXT (p)), 0);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

/**
@brief Coerce bool function.
@param p Node in syntax tree.
**/

static void coerce_bool_function (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, STRONG, MODE (BOOL), 0);
  coerce_unit (SUB (p), &w);
  coerce_unit (SUB (NEXT_NEXT (p)), &w);
}

/**
@brief Coerce assertion.
@param p Node in syntax tree.
**/

static void coerce_assertion (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, MEEK, MODE (BOOL), 0);
  coerce_enclosed (SUB_NEXT (p), &w);
}

/**
@brief Coerce selection.
@param p Node in syntax tree.
**/

static void coerce_selection (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/**
@brief Coerce cast.
@param p Node in syntax tree.
**/

static void coerce_cast (NODE_T * p)
{
  SOID_T w;
  coerce_declarer (p);
  make_soid (&w, STRONG, MOID (p), 0);
  coerce_enclosed (NEXT (p), &w);
}

/**
@brief Coerce argument list.
@param r Pack.
@param p Node in syntax tree.
**/

static void coerce_argument_list (PACK_T ** r, NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ARGUMENT_LIST)) {
      coerce_argument_list (r, SUB (p));
    } else if (IS (p, UNIT)) {
      SOID_T s;
      make_soid (&s, STRONG, MOID (*r), 0);
      coerce_unit (p, &s);
      FORWARD (*r);
    } else if (IS (p, TRIMMER)) {
      FORWARD (*r);
    }
  }
}

/**
@brief Coerce call.
@param p Node in syntax tree.
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

/**
@brief Coerce meek int.
@param p Node in syntax tree.
**/

static void coerce_meek_int (NODE_T * p)
{
  SOID_T x;
  make_soid (&x, MEEK, MODE (INT), 0);
  coerce_unit (p, &x);
}

/**
@brief Coerce trimmer.
@param p Node in syntax tree.
**/

static void coerce_trimmer (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, UNIT)) {
      coerce_meek_int (p);
      coerce_trimmer (NEXT (p));
    } else {
      coerce_trimmer (NEXT (p));
    }
  }
}

/**
@brief Coerce indexer.
@param p Node in syntax tree.
**/

static void coerce_indexer (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, TRIMMER)) {
      coerce_trimmer (SUB (p));
    } else if (IS (p, UNIT)) {
      coerce_meek_int (p);
    } else {
      coerce_indexer (SUB (p));
      coerce_indexer (NEXT (p));
    }
  }
}

/**
@brief Coerce_slice.
@param p Node in syntax tree.
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

/**
@brief Mode coerce diagonal.
@param p Node in syntax tree.
**/

static void coerce_diagonal (NODE_T * p)
{
  SOID_T w;
  if (IS (p, TERTIARY)) {
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, /* WEAK */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/**
@brief Mode coerce transpose.
@param p Node in syntax tree.
**/

static void coerce_transpose (NODE_T * p)
{
  SOID_T w;
  make_soid (&w, /* WEAK */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/**
@brief Mode coerce row or column function.
@param p Node in syntax tree.
**/

static void coerce_row_column_function (NODE_T * p)
{
  SOID_T w;
  if (IS (p, TERTIARY)) {
    make_soid (&w, MEEK, MODE (INT), 0);
    coerce_unit (SUB (p), &w);
    FORWARD (p);
  }
  make_soid (&w, /* WEAK */ STRONG, MOID (NEXT (p)), 0);
  coerce_unit (SUB_NEXT (p), &w);
}

/**
@brief Coerce format text.
@param p Node in syntax tree.
**/

static void coerce_format_text (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    coerce_format_text (SUB (p));
    if (IS (p, FORMAT_PATTERN)) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (FORMAT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (ROW_INT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    } else if (IS (p, DYNAMIC_REPLICATOR)) {
      SOID_T x;
      make_soid (&x, STRONG, MODE (INT), 0);
      coerce_enclosed (SUB (NEXT_SUB (p)), &x);
    }
  }
}

/**
@brief Coerce unit.
@param p Node in syntax tree.
@param q Soid.
**/

static void coerce_unit (NODE_T * p, SOID_T * q)
{
  if (p == NO_NODE) {
    return;
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, STOP)) {
    coerce_unit (SUB (p), q);
    MOID (p) = MOID (SUB (p));
/* Ex primary */
  } else if (IS (p, CALL)) {
    coerce_call (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, SLICE)) {
    coerce_slice (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, CAST)) {
    coerce_cast (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (is_one_of (p, DENOTATION, IDENTIFIER, STOP)) {
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, FORMAT_TEXT)) {
    coerce_format_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    coerce_enclosed (p, q);
/* Ex secondary */
  } else if (IS (p, SELECTION)) {
    coerce_selection (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, GENERATOR)) {
    coerce_declarer (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
/* Ex tertiary */
  } else if (IS (p, NIHIL)) {
    if (ATTRIBUTE (MOID (q)) != REF_SYMBOL && MOID (q) != MODE (VOID)) {
      diagnostic_node (A68_ERROR, p, ERROR_NO_NAME_REQUIRED);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, FORMULA)) {
    coerce_formula (SUB (p), q);
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, DIAGONAL_FUNCTION)) {
    coerce_diagonal (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, TRANSPOSE_FUNCTION)) {
    coerce_transpose (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, ROW_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, COLUMN_FUNCTION)) {
    coerce_row_column_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
/* Ex unit */
  } else if (IS (p, JUMP)) {
    if (MOID (q) == MODE (PROC_VOID)) {
      make_sub (p, p, PROCEDURING);
    }
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, SKIP)) {
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, ASSIGNATION)) {
    coerce_assignation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
    MOID (p) = depref_rows (MOID (p), MOID (q));
  } else if (IS (p, IDENTITY_RELATION)) {
    coerce_relation (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, ROUTINE_TEXT)) {
    coerce_routine_text (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (is_one_of (p, AND_FUNCTION, OR_FUNCTION, STOP)) {
    coerce_bool_function (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  } else if (IS (p, ASSERTION)) {
    coerce_assertion (SUB (p));
    INSERT_COERCIONS (p, MOID (p), q);
  }
}

/**
@brief Widen denotation depending on mode required, this is an extension to A68.
@param p Node in syntax tree.
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
  if (OPTION_PORTCHECK (&program) && !(STATUS_TEST (SUB (q), OPTIMAL_MASK))) {\
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, q, WARNING_WIDENING_NOT_PORTABLE);\
  }
  NODE_T *q;
  for (q = p; q != NO_NODE; FORWARD (q)) {
    widen_denotation (SUB (q));
    if (IS (q, WIDENING) && IS (SUB (q), DENOTATION)) {
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

/********************************************************************/
/* Static scope checker, at run time we check dynamic scope as well */
/********************************************************************/

/*
Static scope checker. 
Also a little preparation for the monitor:
- indicates UNITs that can be interrupted.
*/


/**
@brief Scope_make_tuple.
@param e Level.
@param t Whether transient.
@return Tuple (e, t).
**/

static TUPLE_T scope_make_tuple (int e, int t)
{
  static TUPLE_T z;
  LEVEL (&z) = e;
  TRANSIENT (&z) = (BOOL_T) t;
  return (z);
}

/**
@brief Link scope information into the list.
@param sl Chain to link into.
@param p Node in syntax tree.
@param tup Tuple to link.
**/

static void scope_add (SCOPE_T ** sl, NODE_T * p, TUPLE_T tup)
{
  if (sl != NO_VAR) {
    SCOPE_T *ns = (SCOPE_T *) get_temp_heap_space ((unsigned) SIZE_AL (SCOPE_T));
    WHERE (ns) = p;
    TUPLE (ns) = tup;
    NEXT (ns) = *sl;
    *sl = ns;
  }
}

/**
@brief Scope_check.
@param top Top of scope chain.
@param mask What to check.
@param dest Level to check against.
@return Whether errors were detected.
**/

static BOOL_T scope_check (SCOPE_T * top, int mask, int dest)
{
  SCOPE_T *s;
  int errors = 0;
/* Transient names cannot be stored */
  if (mask & TRANSIENT) {
    for (s = top; s != NO_SCOPE; FORWARD (s)) {
      if (TRANSIENT (&TUPLE (s)) & TRANSIENT) {
        diagnostic_node (A68_ERROR, WHERE (s), ERROR_TRANSIENT_NAME);
        STATUS_SET (WHERE (s), SCOPE_ERROR_MASK);
        errors++;
      }
    }
  }
  for (s = top; s != NO_SCOPE; FORWARD (s)) {
    if (dest < LEVEL (&TUPLE (s)) && ! STATUS_TEST (WHERE (s), SCOPE_ERROR_MASK)) {
/* Potential scope violations */
      MOID_T *sw = MOID (WHERE (s));
      if (sw != NO_MOID) {
        if (IS (sw, REF_SYMBOL) || IS (sw, PROC_SYMBOL)
            || IS (sw, FORMAT_SYMBOL) || IS (sw, UNION_SYMBOL)) {
          diagnostic_node (A68_WARNING, WHERE (s), WARNING_SCOPE_STATIC, 
                           MOID (WHERE (s)), ATTRIBUTE (WHERE (s)));
        }
      }
      STATUS_SET (WHERE (s), SCOPE_ERROR_MASK);
      errors++;
    }
  }
  return ((BOOL_T) (errors == 0));
}

/**
@brief Scope_check_multiple.
@param top Top of scope chain.
@param mask What to check.
@param dest Level to check against.
@return Whether error.
**/

static BOOL_T scope_check_multiple (SCOPE_T * top, int mask, SCOPE_T * dest)
{
  BOOL_T no_err = A68_TRUE;
  for (; dest != NO_SCOPE; FORWARD (dest)) {
    no_err &= scope_check (top, mask, LEVEL (&TUPLE (dest)));
  }
  return (no_err);
}

/**
@brief Check_identifier_usage.
@param t Tag.
@param p Node in syntax tree.
**/

static void check_identifier_usage (TAG_T * t, NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, IDENTIFIER) && TAX (p) == t && ATTRIBUTE (MOID (t)) != PROC_SYMBOL) {
      diagnostic_node (A68_WARNING, p, WARNING_UNINITIALISED);
    }
    check_identifier_usage (t, SUB (p));
  }
}

/**
@brief Scope_find_youngest_outside.
@param s Chain to link into.
@param treshold Threshold level.
@return Youngest tuple outside.
**/

static TUPLE_T scope_find_youngest_outside (SCOPE_T * s, int treshold)
{
  TUPLE_T z = scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT);
  for (; s != NO_SCOPE; FORWARD (s)) {
    if (LEVEL (&TUPLE (s)) > LEVEL (&z) && LEVEL (&TUPLE (s)) <= treshold) {
      z = TUPLE (s);
    }
  }
  return (z);
}

/**
@brief Scope_find_youngest.
@param s Chain to link into.
@return Youngest tuple outside.
**/

static TUPLE_T scope_find_youngest (SCOPE_T * s)
{
  return (scope_find_youngest_outside (s, A68_MAX_INT));
}

/* Routines for determining scope of ROUTINE TEXT or FORMAT TEXT */

/**
@brief Get_declarer_elements.
@param p Node in syntax tree.
@param r Chain to link into.
@param no_ref Whether no REF seen yet.
**/

static void get_declarer_elements (NODE_T * p, SCOPE_T ** r, BOOL_T no_ref)
{
  if (p != NO_NODE) {
    if (IS (p, BOUNDS)) {
      gather_scopes_for_youngest (SUB (p), r);
    } else if (IS (p, INDICANT)) {
      if (MOID (p) != NO_MOID && TAX (p) != NO_TAG && HAS_ROWS (MOID (p)) && no_ref) {
        scope_add (r, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
      }
    } else if (IS (p, REF_SYMBOL)) {
      get_declarer_elements (NEXT (p), r, A68_FALSE);
    } else if (is_one_of (p, PROC_SYMBOL, UNION_SYMBOL, STOP)) {
      ;
    } else {
      get_declarer_elements (SUB (p), r, no_ref);
      get_declarer_elements (NEXT (p), r, no_ref);
    }
  }
}

/**
@brief Gather_scopes_for_youngest.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void gather_scopes_for_youngest (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if ((is_one_of (p, ROUTINE_TEXT, FORMAT_TEXT, STOP)) && (YOUNGEST_ENVIRON (TAX (p)) == PRIMAL_SCOPE)) {
      SCOPE_T *t = NO_SCOPE;
      TUPLE_T tup;
      gather_scopes_for_youngest (SUB (p), &t);
      tup = scope_find_youngest_outside (t, LEX_LEVEL (p));
      YOUNGEST_ENVIRON (TAX (p)) = LEVEL (&tup);
/* Direct link into list iso "gather_scopes_for_youngest (SUB (p), s);" */
      if (t != NO_SCOPE) {
        SCOPE_T *u = t;
        while (NEXT (u) != NO_SCOPE) {
          FORWARD (u);
        }
        NEXT (u) = *s;
        (*s) = t;
      }
    } else if (is_one_of (p, IDENTIFIER, OPERATOR, STOP)) {
      if (TAX (p) != NO_TAG && TAG_LEX_LEVEL (TAX (p)) != PRIMAL_SCOPE) {
        scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
      }
    } else if (IS (p, DECLARER)) {
      get_declarer_elements (p, s, A68_TRUE);
    } else {
      gather_scopes_for_youngest (SUB (p), s);
    }
  }
}

/**
@brief Get_youngest_environs.
@param p Node in syntax tree.
**/

static void get_youngest_environs (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (is_one_of (p, ROUTINE_TEXT, FORMAT_TEXT, STOP)) {
      SCOPE_T *s = NO_SCOPE;
      TUPLE_T tup;
      gather_scopes_for_youngest (SUB (p), &s);
      tup = scope_find_youngest_outside (s, LEX_LEVEL (p));
      YOUNGEST_ENVIRON (TAX (p)) = LEVEL (&tup);
    } else {
      get_youngest_environs (SUB (p));
    }
  }
}

/**
@brief Bind_scope_to_tag.
@param p Node in syntax tree.
**/

static void bind_scope_to_tag (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, DEFINING_IDENTIFIER) && MOID (p) == MODE (FORMAT)) {
      if (IS (NEXT_NEXT (p), FORMAT_TEXT)) {
        SCOPE (TAX (p)) = YOUNGEST_ENVIRON (TAX (NEXT_NEXT (p)));
        SCOPE_ASSIGNED (TAX (p)) = A68_TRUE;
      }
      return;
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      if (IS (NEXT_NEXT (p), ROUTINE_TEXT)) {
        SCOPE (TAX (p)) = YOUNGEST_ENVIRON (TAX (NEXT_NEXT (p)));
        SCOPE_ASSIGNED (TAX (p)) = A68_TRUE;
      }
      return;
    } else {
      bind_scope_to_tag (SUB (p));
    }
  }
}

/**
@brief Bind_scope_to_tags.
@param p Node in syntax tree.
**/

static void bind_scope_to_tags (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (is_one_of (p, PROCEDURE_DECLARATION, IDENTITY_DECLARATION, STOP)) {
      bind_scope_to_tag (SUB (p));
    } else {
      bind_scope_to_tags (SUB (p));
    }
  }
}

/**
@brief Scope_bounds.
@param p Node in syntax tree.
**/

static void scope_bounds (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      scope_statement (p, NO_VAR);
    } else {
      scope_bounds (SUB (p));
    }
  }
}

/**
@brief Scope_declarer.
@param p Node in syntax tree.
**/

static void scope_declarer (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, BOUNDS)) {
      scope_bounds (SUB (p));
    } else if (IS (p, INDICANT)) {
      ;
    } else if (IS (p, REF_SYMBOL)) {
      scope_declarer (NEXT (p));
    } else if (is_one_of (p, PROC_SYMBOL, UNION_SYMBOL, STOP)) {
      ;
    } else {
      scope_declarer (SUB (p));
      scope_declarer (NEXT (p));
    }
  }
}

/**
@brief Scope_identity_declaration.
@param p Node in syntax tree.
**/

static void scope_identity_declaration (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    scope_identity_declaration (SUB (p));
    if (IS (p, DEFINING_IDENTIFIER)) {
      NODE_T *unit = NEXT_NEXT (p);
      SCOPE_T *s = NO_SCOPE;
      TUPLE_T tup;
      int z = PRIMAL_SCOPE;
      if (ATTRIBUTE (MOID (TAX (p))) != PROC_SYMBOL) {
        check_identifier_usage (TAX (p), unit);
      }
      scope_statement (unit, &s);
      (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
      tup = scope_find_youngest (s);
      z = LEVEL (&tup);
      if (z < LEX_LEVEL (p)) {
        SCOPE (TAX (p)) = z;
        SCOPE_ASSIGNED (TAX (p)) = A68_TRUE;
      }
      STATUS_SET (unit, INTERRUPTIBLE_MASK);
      return;
    }
  }
}

/**
@brief Scope_variable_declaration.
@param p Node in syntax tree.
**/

static void scope_variable_declaration (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    scope_variable_declaration (SUB (p));
    if (IS (p, DECLARER)) {
      scope_declarer (SUB (p));
    } else if (IS (p, DEFINING_IDENTIFIER)) {
      if (whether (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, STOP)) {
        NODE_T *unit = NEXT_NEXT (p);
        SCOPE_T *s = NO_SCOPE;
        check_identifier_usage (TAX (p), unit);
        scope_statement (unit, &s);
        (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
        STATUS_SET (unit, INTERRUPTIBLE_MASK);
        return;
      }
    }
  }
}

/**
@brief Scope_procedure_declaration.
@param p Node in syntax tree.
**/

static void scope_procedure_declaration (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    scope_procedure_declaration (SUB (p));
    if (is_one_of (p, DEFINING_IDENTIFIER, DEFINING_OPERATOR, STOP)) {
      NODE_T *unit = NEXT_NEXT (p);
      SCOPE_T *s = NO_SCOPE;
      scope_statement (unit, &s);
      (void) scope_check (s, NOT_TRANSIENT, LEX_LEVEL (p));
      STATUS_SET (unit, INTERRUPTIBLE_MASK);
      return;
    }
  }
}

/**
@brief Scope_declaration_list.
@param p Node in syntax tree.
**/

static void scope_declaration_list (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTITY_DECLARATION)) {
      scope_identity_declaration (SUB (p));
    } else if (IS (p, VARIABLE_DECLARATION)) {
      scope_variable_declaration (SUB (p));
    } else if (IS (p, MODE_DECLARATION)) {
      scope_declarer (SUB (p));
    } else if (IS (p, PRIORITY_DECLARATION)) {
      ;
    } else if (IS (p, PROCEDURE_DECLARATION)) {
      scope_procedure_declaration (SUB (p));
    } else if (IS (p, PROCEDURE_VARIABLE_DECLARATION)) {
      scope_procedure_declaration (SUB (p));
    } else if (is_one_of (p, BRIEF_OPERATOR_DECLARATION, OPERATOR_DECLARATION, STOP)) {
      scope_procedure_declaration (SUB (p));
    } else {
      scope_declaration_list (SUB (p));
      scope_declaration_list (NEXT (p));
    }
  }
}

/**
@brief Scope_arguments.
@param p Node in syntax tree.
**/

static void scope_arguments (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      SCOPE_T *s = NO_SCOPE;
      scope_statement (p, &s);
      (void) scope_check (s, TRANSIENT, LEX_LEVEL (p));
    } else {
      scope_arguments (SUB (p));
    }
  }
}

/**
@brief Is_coercion.
@param p Node in syntax tree.
**/

BOOL_T is_coercion (NODE_T * p)
{
  if (p != NO_NODE) {
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

/**
@brief Scope_coercion.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_coercion (NODE_T * p, SCOPE_T ** s)
{
  if (is_coercion (p)) {
    if (IS (p, VOIDING)) {
      scope_coercion (SUB (p), NO_VAR);
    } else if (IS (p, DEREFERENCING)) {
/* Leave this to the dynamic scope checker */
      scope_coercion (SUB (p), NO_VAR);
    } else if (IS (p, DEPROCEDURING)) {
      scope_coercion (SUB (p), NO_VAR);
    } else if (IS (p, ROWING)) {
      SCOPE_T *z = NO_SCOPE;
      scope_coercion (SUB (p), &z);
      (void) scope_check (z, TRANSIENT, LEX_LEVEL (p));
      if (IS_REF_FLEX (MOID (SUB (p)))) {
        scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
      } else {
        scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), NOT_TRANSIENT)); 
      }
    } else if (IS (p, PROCEDURING)) {
/* Can only be a JUMP */
      NODE_T *q = SUB_SUB (p);
      if (IS (q, GOTO_SYMBOL)) {
        FORWARD (q);
      }
      scope_add (s, q, scope_make_tuple (TAG_LEX_LEVEL (TAX (q)), NOT_TRANSIENT));
    } else if (IS (p, UNITING)) {
      SCOPE_T *z = NO_SCOPE;
      scope_coercion (SUB (p), &z);
      (void) scope_check (z, TRANSIENT, LEX_LEVEL (p));
      scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), NOT_TRANSIENT)); 
    } else {
      scope_coercion (SUB (p), s);
    }
  } else {
    scope_statement (p, s);
  }
}

/**
@brief Scope_format_text.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_format_text (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FORMAT_PATTERN)) {
      scope_enclosed_clause (SUB (NEXT_SUB (p)), s);
    } else if (IS (p, FORMAT_ITEM_G) && NEXT (p) != NO_NODE) {
      scope_enclosed_clause (SUB_NEXT (p), s);
    } else if (IS (p, DYNAMIC_REPLICATOR)) {
      scope_enclosed_clause (SUB (NEXT_SUB (p)), s);
    } else {
      scope_format_text (SUB (p), s);
    }
  }
}

/**
@brief Scope_operand.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_operand (NODE_T * p, SCOPE_T ** s)
{
  if (IS (p, MONADIC_FORMULA)) {
    scope_operand (NEXT_SUB (p), s);
  } else if (IS (p, FORMULA)) {
    scope_formula (p, s);
  } else if (IS (p, SECONDARY)) {
    scope_statement (SUB (p), s);
  }
}

/**
@brief Scope_formula.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_formula (NODE_T * p, SCOPE_T ** s)
{
  NODE_T *q = SUB (p);
  SCOPE_T *s2 = NO_SCOPE;
  scope_operand (q, &s2);
  (void) scope_check (s2, TRANSIENT, LEX_LEVEL (p));
  if (NEXT (q) != NO_NODE) {
    SCOPE_T *s3 = NO_SCOPE;
    scope_operand (NEXT_NEXT (q), &s3);
    (void) scope_check (s3, TRANSIENT, LEX_LEVEL (p));
  }
  (void) s;
}

/**
@brief Scope_routine_text.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_routine_text (NODE_T * p, SCOPE_T ** s)
{
  NODE_T *q = SUB (p), *routine = (IS (q, PARAMETER_PACK) ? NEXT (q) : q);
  SCOPE_T *x = NO_SCOPE;
  TUPLE_T routine_tuple;
  scope_statement (NEXT_NEXT (routine), &x);
  (void) scope_check (x, TRANSIENT, LEX_LEVEL (p));
  routine_tuple = scope_make_tuple (YOUNGEST_ENVIRON (TAX (p)), NOT_TRANSIENT);
  scope_add (s, p, routine_tuple);
}

/**
@brief Scope_statement.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_statement (NODE_T * p, SCOPE_T ** s)
{
  if (is_coercion (p)) {
    scope_coercion (p, s);
  } else if (is_one_of (p, PRIMARY, SECONDARY, TERTIARY, UNIT, STOP)) {
    scope_statement (SUB (p), s);
  } else if (is_one_of (p, DENOTATION, NIHIL, STOP)) {
    scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
  } else if (IS (p, IDENTIFIER)) {
    if (IS (MOID (p), REF_SYMBOL)) {
      if (PRIO (TAX (p)) == PARAMETER_IDENTIFIER) {
        scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)) - 1, NOT_TRANSIENT));
      } else {
        if (HEAP (TAX (p)) == HEAP_SYMBOL) {
          scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
        } else if (SCOPE_ASSIGNED (TAX (p))) {
          scope_add (s, p, scope_make_tuple (SCOPE (TAX (p)), NOT_TRANSIENT));
        } else {
          scope_add (s, p, scope_make_tuple (TAG_LEX_LEVEL (TAX (p)), NOT_TRANSIENT));
        }
      }
    } else if (ATTRIBUTE (MOID (p)) == PROC_SYMBOL && SCOPE_ASSIGNED (TAX (p)) == A68_TRUE) {
      scope_add (s, p, scope_make_tuple (SCOPE (TAX (p)), NOT_TRANSIENT));
    } else if (MOID (p) == MODE (FORMAT) && SCOPE_ASSIGNED (TAX (p)) == A68_TRUE) {
      scope_add (s, p, scope_make_tuple (SCOPE (TAX (p)), NOT_TRANSIENT));
    }
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    scope_enclosed_clause (SUB (p), s);
  } else if (IS (p, CALL)) {
    SCOPE_T *x = NO_SCOPE;
    scope_statement (SUB (p), &x);
    (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
    scope_arguments (NEXT_SUB (p));
  } else if (IS (p, SLICE)) {
    SCOPE_T *x = NO_SCOPE;
    MOID_T *m = MOID (SUB (p));
    if (IS (m, REF_SYMBOL)) {
      if (ATTRIBUTE (SUB (p)) == PRIMARY && ATTRIBUTE (SUB_SUB (p)) == SLICE) {
        scope_statement (SUB (p), s);
      } else {
        scope_statement (SUB (p), &x);
        (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
      }
      if (IS (SUB (m), FLEX_SYMBOL)) {
        scope_add (s, SUB (p), scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
      }
      scope_bounds (SUB (NEXT_SUB (p)));
    }
    if (IS (MOID (p), REF_SYMBOL)) {
      scope_add (s, p, scope_find_youngest (x));
    }
  } else if (IS (p, FORMAT_TEXT)) {
    SCOPE_T *x = NO_SCOPE;
    scope_format_text (SUB (p), &x);
    scope_add (s, p, scope_find_youngest (x));
  } else if (IS (p, CAST)) {
    SCOPE_T *x = NO_SCOPE;
    scope_enclosed_clause (SUB (NEXT_SUB (p)), &x);
    (void) scope_check (x, NOT_TRANSIENT, LEX_LEVEL (p));
    scope_add (s, p, scope_find_youngest (x));
  } else if (IS (p, SELECTION)) {
    SCOPE_T *ns = NO_SCOPE;
    scope_statement (NEXT_SUB (p), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (p));
    if (is_ref_refety_flex (MOID (NEXT_SUB (p)))) {
      scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), TRANSIENT));
    }
    scope_add (s, p, scope_find_youngest (ns));
  } else if (IS (p, GENERATOR)) {
    if (IS (SUB (p), LOC_SYMBOL)) {
      if (NON_LOCAL (p) != NO_TABLE) {
        scope_add (s, p, scope_make_tuple (LEVEL (NON_LOCAL (p)), NOT_TRANSIENT));
      } else {
        scope_add (s, p, scope_make_tuple (LEX_LEVEL (p), NOT_TRANSIENT));
      }
    } else {
      scope_add (s, p, scope_make_tuple (PRIMAL_SCOPE, NOT_TRANSIENT));
    }
    scope_declarer (SUB (NEXT_SUB (p)));
  } else if (IS (p, DIAGONAL_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NO_SCOPE;
    if (IS (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NO_SCOPE;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (IS (p, TRANSPOSE_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NO_SCOPE;
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (IS (p, ROW_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NO_SCOPE;
    if (IS (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NO_SCOPE;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (IS (p, COLUMN_FUNCTION)) {
    NODE_T *q = SUB (p);
    SCOPE_T *ns = NO_SCOPE;
    if (IS (q, TERTIARY)) {
      scope_statement (SUB (q), &ns);
      (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
      ns = NO_SCOPE;
      FORWARD (q);
    }
    scope_statement (SUB_NEXT (q), &ns);
    (void) scope_check (ns, NOT_TRANSIENT, LEX_LEVEL (q));
    scope_add (s, p, scope_find_youngest (ns));
  } else if (IS (p, FORMULA)) {
    scope_formula (p, s);
  } else if (IS (p, ASSIGNATION)) {
    NODE_T *unit = NEXT (NEXT_SUB (p));
    SCOPE_T *ns = NO_SCOPE, *nd = NO_SCOPE;
    TUPLE_T tup;
    scope_statement (SUB_SUB (p), &nd);
    scope_statement (unit, &ns);
    (void) scope_check_multiple (ns, TRANSIENT, nd);
    tup = scope_find_youngest (nd);
    scope_add (s, p, scope_make_tuple (LEVEL (&tup), NOT_TRANSIENT));
  } else if (IS (p, ROUTINE_TEXT)) {
    scope_routine_text (p, s);
  } else if (is_one_of (p, IDENTITY_RELATION, AND_FUNCTION, OR_FUNCTION, STOP)) {
    SCOPE_T *n = NO_SCOPE;
    scope_statement (SUB (p), &n);
    scope_statement (NEXT (NEXT_SUB (p)), &n);
    (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  } else if (IS (p, ASSERTION)) {
    SCOPE_T *n = NO_SCOPE;
    scope_enclosed_clause (SUB (NEXT_SUB (p)), &n);
    (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  } else if (is_one_of (p, JUMP, SKIP, STOP)) {
    ;
  }
}

/**
@brief Scope_statement_list.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_statement_list (NODE_T * p, SCOPE_T ** s)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      STATUS_SET (p, INTERRUPTIBLE_MASK);
      scope_statement (p, s);
    } else {
      scope_statement_list (SUB (p), s);
    }
  }
}

/**
@brief Scope_serial_clause.
@param p Node in syntax tree.
@param s Chain to link into.
@param terminator Whether unit terminates clause.
**/

static void scope_serial_clause (NODE_T * p, SCOPE_T ** s, BOOL_T terminator)
{
  if (p != NO_NODE) {
    if (IS (p, INITIALISER_SERIES)) {
      scope_serial_clause (SUB (p), s, A68_FALSE);
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (IS (p, DECLARATION_LIST)) {
      scope_declaration_list (SUB (p));
    } else if (is_one_of (p, LABEL, SEMI_SYMBOL, EXIT_SYMBOL, STOP)) {
      scope_serial_clause (NEXT (p), s, terminator);
    } else if (is_one_of (p, SERIAL_CLAUSE, ENQUIRY_CLAUSE, STOP)) {
      if (NEXT (p) != NO_NODE) {
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
    } else if (IS (p, LABELED_UNIT)) {
      scope_serial_clause (SUB (p), s, terminator);
    } else if (IS (p, UNIT)) {
      STATUS_SET (p, INTERRUPTIBLE_MASK);
      if (terminator) {
        scope_statement (p, s);
      } else {
        scope_statement (p, NO_VAR);
      }
    }
  }
}

/**
@brief Scope_closed_clause.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_closed_clause (NODE_T * p, SCOPE_T ** s)
{
  if (p != NO_NODE) {
    if (IS (p, SERIAL_CLAUSE)) {
      scope_serial_clause (p, s, A68_TRUE);
    } else if (is_one_of (p, OPEN_SYMBOL, BEGIN_SYMBOL, STOP)) {
      scope_closed_clause (NEXT (p), s);
    }
  }
}

/**
@brief Scope_collateral_clause.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_collateral_clause (NODE_T * p, SCOPE_T ** s)
{
  if (p != NO_NODE) {
    if (!(whether (p, BEGIN_SYMBOL, END_SYMBOL, STOP) || whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, STOP))) {
      scope_statement_list (p, s);
    }
  }
}

/**
@brief Scope_conditional_clause.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_conditional_clause (NODE_T * p, SCOPE_T ** s)
{
  scope_serial_clause (NEXT_SUB (p), NO_VAR, A68_TRUE);
  FORWARD (p);
  scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, ELSE_PART, CHOICE, STOP)) {
      scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
    } else if (is_one_of (p, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      scope_conditional_clause (SUB (p), s);
    }
  }
}

/**
@brief Scope_case_clause.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_case_clause (NODE_T * p, SCOPE_T ** s)
{
  SCOPE_T *n = NO_SCOPE;
  scope_serial_clause (NEXT_SUB (p), &n, A68_TRUE);
  (void) scope_check (n, NOT_TRANSIENT, LEX_LEVEL (p));
  FORWARD (p);
  scope_statement_list (NEXT_SUB (p), s);
  if ((FORWARD (p)) != NO_NODE) {
    if (is_one_of (p, OUT_PART, CHOICE, STOP)) {
      scope_serial_clause (NEXT_SUB (p), s, A68_TRUE);
    } else if (is_one_of (p, CASE_OUSE_PART, BRIEF_OUSE_PART, STOP)) {
      scope_case_clause (SUB (p), s);
    } else if (is_one_of (p, CONFORMITY_OUSE_PART, BRIEF_CONFORMITY_OUSE_PART, STOP)) {
      scope_case_clause (SUB (p), s);
    }
  }
}

/**
@brief Scope_loop_clause.
@param p Node in syntax tree.
**/

static void scope_loop_clause (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, FOR_PART)) {
      scope_loop_clause (NEXT (p));
    } else if (is_one_of (p, FROM_PART, BY_PART, TO_PART, STOP)) {
      scope_statement (NEXT_SUB (p), NO_VAR);
      scope_loop_clause (NEXT (p));
    } else if (IS (p, WHILE_PART)) {
      scope_serial_clause (NEXT_SUB (p), NO_VAR, A68_TRUE);
      scope_loop_clause (NEXT (p));
    } else if (is_one_of (p, DO_PART, ALT_DO_PART, STOP)) {
      NODE_T *do_p = NEXT_SUB (p), *un_p;
      if (IS (do_p, SERIAL_CLAUSE)) {
        scope_serial_clause (do_p, NO_VAR, A68_TRUE);
        un_p = NEXT (do_p);
      } else {
        un_p = do_p;
      }
      if (un_p != NO_NODE && IS (un_p, UNTIL_PART)) {
        scope_serial_clause (NEXT_SUB (un_p), NO_VAR, A68_TRUE);
      }
    }
  }
}

/**
@brief Scope_enclosed_clause.
@param p Node in syntax tree.
@param s Chain to link into.
**/

static void scope_enclosed_clause (NODE_T * p, SCOPE_T ** s)
{
  if (IS (p, ENCLOSED_CLAUSE)) {
    scope_enclosed_clause (SUB (p), s);
  } else if (IS (p, CLOSED_CLAUSE)) {
    scope_closed_clause (SUB (p), s);
  } else if (is_one_of (p, COLLATERAL_CLAUSE, PARALLEL_CLAUSE, STOP)) {
    scope_collateral_clause (SUB (p), s);
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    scope_conditional_clause (SUB (p), s);
  } else if (is_one_of (p, CASE_CLAUSE, CONFORMITY_CLAUSE, STOP)) {
    scope_case_clause (SUB (p), s);
  } else if (IS (p, LOOP_CLAUSE)) {
    scope_loop_clause (SUB (p));
  }
}

/**
@brief Whether a symbol table contains no (anonymous) definition.
@param t Symbol table.
\return TRUE or FALSE
**/

static BOOL_T empty_table (TABLE_T * t)
{
  if (IDENTIFIERS (t) == NO_TAG) {
    return ((BOOL_T) (OPERATORS (t) == NO_TAG && INDICANTS (t) == NO_TAG));
  } else if (PRIO (IDENTIFIERS (t)) == LOOP_IDENTIFIER && NEXT (IDENTIFIERS (t)) == NO_TAG) {
    return ((BOOL_T) (OPERATORS (t) == NO_TAG && INDICANTS (t) == NO_TAG));
  } else if (PRIO (IDENTIFIERS (t)) == SPECIFIER_IDENTIFIER && NEXT (IDENTIFIERS (t)) == NO_TAG) {
    return ((BOOL_T) (OPERATORS (t) == NO_TAG && INDICANTS (t) == NO_TAG));
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Indicate non-local environs.
@param p Node in syntax tree.
@param max Lex level threshold.
**/

static void get_non_local_environs (NODE_T * p, int max)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ROUTINE_TEXT)) {
      get_non_local_environs (SUB (p), LEX_LEVEL (SUB (p)));
    } else if (IS (p, FORMAT_TEXT)) {
      get_non_local_environs (SUB (p), LEX_LEVEL (SUB (p)));
    } else {
      get_non_local_environs (SUB (p), max);
      NON_LOCAL (p) = NO_TABLE;
      if (TABLE (p) != NO_TABLE) {
        TABLE_T *q = TABLE (p);
        while (q != NO_TABLE && empty_table (q) 
               && PREVIOUS (q) != NO_TABLE
               && LEVEL (PREVIOUS (q)) >= max) {
          NON_LOCAL (p) = PREVIOUS (q);
          q = PREVIOUS (q);
        }  
      }
    }
  }
}

/**
@brief Scope_checker.
@param p Node in syntax tree.
**/

void scope_checker (NODE_T * p)
{
/* Establish scopes of routine texts and format texts */
  get_youngest_environs (p);
/* Find non-local environs */
  get_non_local_environs (p, PRIMAL_SCOPE);  
/* PROC and FORMAT identities can now be assigned a scope */
  bind_scope_to_tags (p);
/* Now check evertyhing else */
  scope_enclosed_clause (SUB (p), NO_VAR);
}

/* syntax.c*/

