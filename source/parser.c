/*!
\file parser.c
\brief hand-coded Algol 68 parser
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


/*
This file contains a hand-coded parser for Algol 68.

Parsing progresses in various phases to avoid spurious diagnostics from a
recovering parser. Every phase "tightens" the grammar more.
An error in any phase makes the parser quit when that phase ends.
The parser is forgiving in case of superfluous semicolons.

These are the phases:

 (1) Parenthesis are checked to see whether they match.

 (2) Then, a top-down parser determines the basic-block structure of the program
     so symbol tables can be set up that the bottom-up parser will consult
     as you can define things before they are applied.

 (3) A bottom-up parser tries to resolve the structure of the program.

 (4) After the symbol tables have been finalised, a small rearrangement of the
     tree may be required where JUMPs have no GOTO. This leads to the
     non-standard situation that JUMPs without GOTO can have the syntactic
     position of a PRIMARY, SECONDARY or TERTIARY. The mode checker will reject
     such constructs later on.

 (5) The bottom-up parser does not check VICTAL correctness of declarers. This
     is done separately. Also structure of a FORMAT_TEXT is checked separately.
*/

#include "algol68g.h"

static jmp_buf bottom_up_crash_exit, top_down_crash_exit;

static BOOL_T reduce_phrase (NODE_T *, int);
static BOOL_T victal_check_declarer (NODE_T *, int);
static NODE_T *reduce_dyadic (NODE_T *, int u);
static NODE_T *top_down_loop (NODE_T *);
static NODE_T *top_down_skip_unit (NODE_T *);
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
static void f (NODE_T *, void (*)(NODE_T *), BOOL_T *, ...);
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

/*!
\brief insert node
\param p node after which to insert, should not be NULL
\param att attribute for new node
**/

static void insert_node (NODE_T * p, int att)
{
  NODE_T *q = new_node ();
  *q = *p;
  ATTRIBUTE (q) = att;
  NEXT (p) = q;
  PREVIOUS (q) = p;
  if (NEXT (q) != NULL) {
    PREVIOUS (NEXT (q)) = q;
  }
}

/*!
\brief substitute brackets
\param p position in syntax tree, should not be NULL
**/

void substitute_brackets (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    substitute_brackets (SUB (p));
    switch (ATTRIBUTE (p)) {
    case ACCO_SYMBOL:
      ATTRIBUTE (p) = OPEN_SYMBOL;
      break;
    case OCCA_SYMBOL:
      ATTRIBUTE (p) = CLOSE_SYMBOL;
      break;
    case SUB_SYMBOL:
      ATTRIBUTE (p) = OPEN_SYMBOL;
      break;
    case BUS_SYMBOL:
      ATTRIBUTE (p) = CLOSE_SYMBOL;
      break;
    }
  }
}

/*!
\brief whether token terminates a unit
\param p position in syntax tree, should not be NULL
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
      return (0);
    }
  }
}

/*!
\brief whether token is a unit-terminator in a loop clause
\param p position in syntax tree, should not be NULL
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
      return (ATTRIBUTE (p));
    }
  default:
    {
      return (0);
    }
  }
}

/*!
\brief whether token cannot follow semicolon or EXIT
\param p position in syntax tree, should not be NULL
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
      return (0);
    }
  }
}

/*!
\brief get good attribute
\param p position in syntax tree, should not be NULL
\return
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
\brief intelligible diagnostic_node from syntax tree branch
\param p position in syntax tree, should not be NULL
\param q
\return
**/

static char *phrase_to_text (NODE_T * p, NODE_T * q)
{
#define MAX_TERMINALS 8
  int length = 0, count = 0;
  static char buffer[BUFFER_SIZE];
  buffer[0] = NULL_CHAR;
  for (; p != NULL && (q != NULL ? p != NEXT (q) : count < MAX_TERMINALS)
      && length < BUFFER_SIZE / 2; FORWARD (p)) {
    char *z = non_terminal_string (edit_line, get_good_attribute (p));
    if (strlen (buffer) > 1) {
      bufcat (buffer, ", ", BUFFER_SIZE);
    }
    if (z != NULL) {
      bufcat (buffer, z, BUFFER_SIZE);
    } else if (SYMBOL (p) != NULL) {
      snprintf (edit_line, BUFFER_SIZE, "\"%s\"", SYMBOL (p));
      bufcat (buffer, edit_line, BUFFER_SIZE);
    }
    count++;
    length = strlen (buffer);
  }
  if (p != NULL && (q == NULL ? A_FALSE : count == MAX_TERMINALS)) {
    bufcat (buffer, " ..", BUFFER_SIZE);
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
\param txt
\param n
\param bra
\param ket
\return
**/

static void bracket_check_error (char *txt, int n, char *bra, char *ket)
{
  if (n != 0) {
    char b[BUFFER_SIZE];
    snprintf (b, BUFFER_SIZE, "\"%s\" without matching \"%s\"", (n > 0 ? bra : ket), (n > 0 ? ket : bra));
    if (strlen (txt) > 0) {
      bufcat (txt, " and ", BUFFER_SIZE);
    }
    bufcat (txt, b, BUFFER_SIZE);
  }
}

/*!
\brief diagnose brackets in local branch of the tree
\param p position in syntax tree, should not be NULL
\return
**/

static char *bracket_check_diagnose (NODE_T * p)
{
  int begins = 0, opens = 0, format_opens = 0, subs = 0, ifs = 0, cases = 0, dos = 0, accos = 0;
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
    case FORMAT_ITEM_OPEN:
      {
        format_opens++;
        break;
      }
    case FORMAT_ITEM_CLOSE:
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
\param token from where to proceed
\return
**/

static NODE_T *bracket_check_parse (NODE_T * top, NODE_T * p)
{
  while (p != NULL) {
    int ket;
    NODE_T *q;
    switch (ATTRIBUTE (p)) {
    case BEGIN_SYMBOL:
      {
        q = bracket_check_parse (top, NEXT (p));
        ket = END_SYMBOL;
        break;
      }
    case OPEN_SYMBOL:
      {
        q = bracket_check_parse (top, NEXT (p));
        ket = CLOSE_SYMBOL;
        break;
      }
    case ACCO_SYMBOL:
      {
        q = bracket_check_parse (top, NEXT (p));
        ket = OCCA_SYMBOL;
        break;
      }
    case FORMAT_ITEM_OPEN:
      {
        q = bracket_check_parse (top, NEXT (p));
        ket = FORMAT_ITEM_CLOSE;
        break;
      }
    case SUB_SYMBOL:
      {
        q = bracket_check_parse (top, NEXT (p));
        ket = BUS_SYMBOL;
        break;
      }
    case IF_SYMBOL:
      {
        q = bracket_check_parse (top, NEXT (p));
        ket = FI_SYMBOL;
        break;
      }
    case CASE_SYMBOL:
      {
        q = bracket_check_parse (top, NEXT (p));
        ket = ESAC_SYMBOL;
        break;
      }
    case DO_SYMBOL:
      {
        q = bracket_check_parse (top, NEXT (p));
        ket = OD_SYMBOL;
        break;
      }
    case END_SYMBOL:
    case OCCA_SYMBOL:
    case CLOSE_SYMBOL:
    case FORMAT_ITEM_CLOSE:
    case BUS_SYMBOL:
    case FI_SYMBOL:
    case ESAC_SYMBOL:
    case OD_SYMBOL:
      {
        return (p);
      }
    default:
      {
        goto next;
      }
    }
    if (q == NULL || WHETHER_NOT (q, ket)) {
      char *diag = bracket_check_diagnose (top);
      diagnostic_node (A_SYNTAX_ERROR, p, ERROR_PARENTHESIS, strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS);
      longjmp (top_down_crash_exit, 1);
    }
    p = q;
  next:
    if (p != NULL) {
      FORWARD (p);
    }
  }
  return (p);
}

/*!
\brief driver for globally diagnosing non-matching tokens
\param top
**/

void check_parenthesis (NODE_T * top)
{
  if (!setjmp (top_down_crash_exit)) {
    if (bracket_check_parse (top, top) != NULL) {
      diagnostic_node (A_SYNTAX_ERROR, top, ERROR_PARENTHESIS, INFO_MISSING_KEYWORDS, NULL);
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
\param where error issued at this point
\param clause type of clause being processed
\param expected token expected
**/

static void top_down_diagnose (NODE_T * start, NODE_T * where, int clause, int expected)
{
  NODE_T *issue = (where != NULL ? where : start);
  if (expected != 0) {
    diagnostic_node (A_SYNTAX_ERROR, issue, ERROR_EXPECTED_NEAR, expected, clause, SYMBOL (start), start->info->line, NULL);
  } else {
    diagnostic_node (A_SYNTAX_ERROR, issue, ERROR_UNBALANCED_KEYWORD, clause, SYMBOL (start), start->info->line, NULL);
  }
}

/*!
\brief check for premature exhaustion of tokens
\param p position in syntax tree, should not be NULL
\param q
**/

static void tokens_exhausted (NODE_T * p, NODE_T * q)
{
  if (p == NULL) {
    diagnostic_node (A_SYNTAX_ERROR, q, ERROR_KEYWORD);
    longjmp (top_down_crash_exit, 1);
  }
}

/* This part specifically branches out loop clauses. */

/*!
\brief whether in cast or formula with loop clause
\param p position in syntax tree, should not be NULL
\return number of symbols to skip
**/

static int whether_loop_cast_formula (NODE_T * p)
{
/* Accept declarers that can appear in such casts but not much more. */
  if (WHETHER (p, VOID_SYMBOL)) {
    return (1);
  } else if (WHETHER (p, INT_SYMBOL)) {
    return (1);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (1);
  } else if (WHETHER (p, OPERATOR) || WHETHER (p, BOLD_TAG)) {
    return (1);
  } else if (whether (p, UNION_SYMBOL, OPEN_SYMBOL, 0)) {
    return (2);
  } else if (WHETHER (p, OPEN_SYMBOL) || WHETHER (p, SUB_SYMBOL)) {
    int k = 0;
    while (p != NULL && (WHETHER (p, OPEN_SYMBOL) || WHETHER (p, SUB_SYMBOL))) {
      FORWARD (p);
      k++;
    }
    return (p != NULL && whether (p, UNION_SYMBOL, OPEN_SYMBOL, 0) ? k : 0);
  }
  return (0);
}

/*!
\brief skip a unit in a loop clause (FROM u BY u TO u)
\param p position in syntax tree, should not be NULL
\return token from where to proceed
**/

static NODE_T *top_down_skip_loop_unit (NODE_T * p)
{
/* Unit may start with, or consist of, a loop. */
  if (whether_loop_keyword (p)) {
    p = top_down_loop (p);
  }
/* Skip rest of unit. */
  while (p != NULL) {
    int k = whether_loop_cast_formula (p);
    if (k != 0) {
/* operator-cast series ... */
      while (p != NULL && k != 0) {
        while (k != 0) {
          FORWARD (p);
          k--;
        }
        k = whether_loop_cast_formula (p);
      }
/* ... may be followed by a loop clause. */
      if (whether_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (whether_loop_keyword (p) || WHETHER (p, OD_SYMBOL)) {
/* new loop or end-of-loop. */
      return (p);
    } else if (WHETHER (p, COLON_SYMBOL)) {
      FORWARD (p);
/* skip routine header: loop clause. */
      if (p != NULL && whether_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (WHETHER (p, SEMI_SYMBOL) || WHETHER (p, COMMA_SYMBOL) || WHETHER (p, EXIT_SYMBOL)) {
/* Statement separators. */
      return (p);
    } else {
      FORWARD (p);
    }
  }
  return (NULL);
}

/*!
\brief skip a loop clause
\param p position in syntax tree, should not be NULL
\return token from where to proceed
**/

static NODE_T *top_down_skip_loop_series (NODE_T * p)
{
  BOOL_T z;
  do {
    p = top_down_skip_loop_unit (p);
    z = (p != NULL && (WHETHER (p, SEMI_SYMBOL) || WHETHER (p, EXIT_SYMBOL) || WHETHER (p, COMMA_SYMBOL) || WHETHER (p, COLON_SYMBOL)));
    if (z) {
      FORWARD (p);
    }
  }
  while (!(p == NULL || !z));
  return (p);
}

/*!
\brief branch out loop parts
\param p position in syntax tree, should not be NULL
\return token from where to proceed
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
    if (WHETHER (q, FROM_SYMBOL) || WHETHER (q, BY_SYMBOL) || WHETHER (q, TO_SYMBOL) || WHETHER (q, DOWNTO_SYMBOL) || WHETHER (q, WHILE_SYMBOL)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, 0);
      longjmp (top_down_crash_exit, 1);
    }
  }
  if (WHETHER (q, FROM_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_unit (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER (q, BY_SYMBOL) || WHETHER (q, TO_SYMBOL) || WHETHER (q, DOWNTO_SYMBOL) || WHETHER (q, WHILE_SYMBOL)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, 0);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), FROM_SYMBOL);
  }
  if (WHETHER (q, BY_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER (q, TO_SYMBOL) || WHETHER (q, DOWNTO_SYMBOL) || WHETHER (q, WHILE_SYMBOL)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, 0);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), BY_SYMBOL);
  }
  if (WHETHER (q, TO_SYMBOL) || WHETHER (q, DOWNTO_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER (q, WHILE_SYMBOL)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, 0);
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
  if (WHETHER (q, DO_SYMBOL) || WHETHER (q, ALT_DO_SYMBOL)) {
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
\brief driver for branching out loop parts
\param p position in syntax tree, should not be NULL
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
    if (whether_loop_keyword (q) != 0) {
      q = top_down_loop (q);
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief driver for branching out until parts
\param p position in syntax tree, should not be NULL
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

/* Branch anything except parts of a loop. */

/*!
\brief skip serial/enquiry clause (unit series)
\param p position in syntax tree, should not be NULL
\return token from where to proceed
**/

static NODE_T *top_down_series (NODE_T * p)
{
  BOOL_T z = A_TRUE;
  while (z) {
    z = A_FALSE;
    p = top_down_skip_unit (p);
    if (p != NULL) {
      if (WHETHER (p, SEMI_SYMBOL) || WHETHER (p, EXIT_SYMBOL) || WHETHER (p, COMMA_SYMBOL)) {
        z = A_TRUE;
        FORWARD (p);
      }
    }
  }
  return (p);
}

/*!
\brief branch out BEGIN .. END
\param begin_p
\return token from where to proceed
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
\brief branch out CODE .. EDOC
\param code_p
\return token from where to proceed
**/

static NODE_T *top_down_code (NODE_T * code_p)
{
  NODE_T *edoc_p = top_down_series (NEXT (code_p));
  if (edoc_p == NULL || WHETHER_NOT (edoc_p, EDOC_SYMBOL)) {
    diagnostic_node (A_SYNTAX_ERROR, code_p, ERROR_KEYWORD);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  } else {
    make_sub (code_p, edoc_p, CODE_SYMBOL);
    return (NEXT (code_p));
  }
}

/*!
\brief branch out ( .. )
\param open_p
\return token from where to proceed
**/

static NODE_T *top_down_open (NODE_T * open_p)
{
  NODE_T *then_bar_p = top_down_series (NEXT (open_p)), *elif_bar_p;
  if (then_bar_p != NULL && WHETHER (then_bar_p, CLOSE_SYMBOL)) {
    make_sub (open_p, then_bar_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (then_bar_p == NULL || WHETHER_NOT (then_bar_p, THEN_BAR_SYMBOL)) {
    top_down_diagnose (open_p, then_bar_p, ENCLOSED_CLAUSE, 0);
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
\brief branch out [ .. ]
\param sub_p
\return token from where to proceed
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
\brief branch out { .. }
\param acco_p
\return
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
\brief branch out IF .. THEN .. ELSE .. FI
\param if_p
\return token from where to proceed
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
\brief branch out CASE .. IN .. OUT .. ESAC
\param case_p
\return token from where to proceed
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
\param p position in syntax tree, should not be NULL
\return token from where to proceed
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
\brief branch out ( .. ) in a format
\param open_p
\return token from where to proceed
**/

static NODE_T *top_down_format_open (NODE_T * open_p)
{
  NODE_T *close_p = top_down_skip_format (NEXT (open_p));
  if (close_p != NULL && WHETHER (close_p, FORMAT_ITEM_CLOSE)) {
    make_sub (open_p, close_p, FORMAT_ITEM_OPEN);
    return (NEXT (open_p));
  } else {
    top_down_diagnose (open_p, close_p, 0, FORMAT_ITEM_CLOSE);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief skip a format text
\param p position in syntax tree, should not be NULL
\return token from where to proceed
**/

static NODE_T *top_down_skip_format (NODE_T * p)
{
  while (p != NULL) {
    if (WHETHER (p, FORMAT_ITEM_OPEN)) {
      p = top_down_format_open (p);
    } else if (WHETHER (p, FORMAT_ITEM_CLOSE) || WHETHER (p, FORMAT_DELIMITER_SYMBOL)) {
      return (p);
    } else {
      FORWARD (p);
    }
  }
  return (NULL);
}

/*!
\brief branch out $ .. $
\param p position in syntax tree, should not be NULL
\return token from where to proceed
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
        if (WHETHER (f, FORMAT_ITEM_OPEN)) {
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
\brief branch out phrases for the bottom-up parser
\param p position in syntax tree, should not be NULL
**/

void top_down_parser (NODE_T * p)
{
  if (p != NULL) {
    current_module = NULL;
    if (!setjmp (top_down_crash_exit)) {
      top_down_series (p);
      top_down_loops (p);
      top_down_untils (p);
      top_down_formats (p);
    }
  }
}

/*
Next part is the bottom-up parser, that parses without knowing about modes while
parsing and reducing. It can therefore not exchange "[]" with "()" as was
blessed by the Revised Report. This is solved by treating CALL and SLICE as
equivalent here and letting the mode checker sort it out.

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
*/

/*!
\brief whether a series is serial or collateral
\param p position in syntax tree, should not be NULL
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
/* Heuristic guess to give intelligible error message. */
    return ((semis + exits) >= commas ? SERIAL_CLAUSE : COLLATERAL_CLAUSE);
  }
}

/*!
\brief insert a node with attribute "a" after "p"
\param p position in syntax tree, should not be NULL
\param a
**/

static void pad_node (NODE_T * p, int a)
{
/*
This is used to fill information that Algol 68 does not require to be present.
Filling in gives one format for such construct; this helps later passes.
*/
  NODE_T *z = new_node ();
  *z = *p;
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
\brief diagnose for not-supported features
\param p position in syntax tree, should not be NULL
**/

static void not_supported (NODE_T * p)
{
  diagnostic_node (A_SYNTAX_ERROR, p, ERROR_FEATURE_UNSUPPORTED, NULL);
}

/*!
\brief diagnose for clauses not yielding a value
\param p position in syntax tree, should not be NULL
**/

static void empty_clause (NODE_T * p)
{
  diagnostic_node (A_SYNTAX_ERROR, p, ERROR_CLAUSE_WITHOUT_VALUE);
}

#ifndef HAVE_POSIX_THREADS

/*!
\brief diagnose for parallel clause
\param p position in syntax tree, should not be NULL
**/

static void par_clause (NODE_T * p)
{
  diagnostic_node (A_WARNING | FORCE_DIAGNOSTIC, p, WARNING_EXECUTED_AS, PARALLEL_CLAUSE, COLLATERAL_CLAUSE);
}

#endif

/*!
\brief diagnose for missing symbol
\param p position in syntax tree, should not be NULL
**/

static void missing_symbol (NODE_T * p)
{
  NODE_T *q = (p != NULL && NEXT (p) != NULL) ? NEXT (p) : p;
  diagnostic_node (A_SYNTAX_ERROR, q, ERROR_SYNTAX_MISSING_SYMBOL);
}

/*!
\brief diagnose for missing separator
\param p position in syntax tree, should not be NULL
**/

static void missing_separator (NODE_T * p)
{
  NODE_T *q = (p != NULL && NEXT (p) != NULL) ? NEXT (p) : p;
  diagnostic_node (A_SYNTAX_ERROR, q, ERROR_SYNTAX_MISSING_SEPARATOR);
}

/*!
\brief diagnose for wrong separator
\param p position in syntax tree, should not be NULL
**/

static void wrong_separator (NODE_T * p)
{
  NODE_T *q = (p != NULL && NEXT (p) != NULL) ? NEXT (p) : p;
  diagnostic_node (A_SYNTAX_ERROR, q, ERROR_SYNTAX_WRONG_SEPARATOR);
}

/*!
\brief whether match and reduce a sentence
\param p token where to start matching, should not be NULL
\param a if not NULL, procedure to execute upon match
\param z if not NULL, to be set to TRUE upon match
**/

static void f (NODE_T * p, void (*a) (NODE_T *), BOOL_T * z, ...)
{
  va_list list;
  int result, arg;
  NODE_T *head = p, *tail = NULL;
  va_start (list, z);
  result = va_arg (list, int);
  while ((arg = va_arg (list, int)) != 0) {
/* WILDCARD matches any Algol68G non terminal, but no keyword. */
    if (p != NULL && (arg == WILDCARD ? non_terminal_string (edit_line, ATTRIBUTE (p)) != NULL : arg == ATTRIBUTE (p))) {
      tail = p;
      FORWARD (p);
    } else {
      va_end (list);
      return;
    }
  }
  if (head != NULL && head->info->module->options.reductions) {
    where (STDOUT_FILENO, head);
    bufcpy (output_line, "", BUFFER_SIZE);
    bufcat (output_line, non_terminal_string (edit_line, result), BUFFER_SIZE);
    bufcat (output_line, "<-", BUFFER_SIZE);
    bufcat (output_line, phrase_to_text (head, tail), BUFFER_SIZE);
    io_write_string (STDOUT_FILENO, output_line);
  }
/* Execute procedure in case reduction succeeds. */
  if (a != NULL) {
    a (head);
  }
  make_sub (head, tail, result);
  va_end (list);
  if (z != NULL) {
    *z = A_TRUE;
  }
}

/*!
\brief driver for the bottom-up parser
\param p position in syntax tree, should not be NULL
**/

void bottom_up_parser (NODE_T * p)
{
  if (p != NULL) {
    current_module = p->info->module;
    if (!setjmp (bottom_up_crash_exit)) {
      ignore_superfluous_semicolons (p);
      reduce_particular_program (p);
    }
  }
}

/*!
\brief top-level reduction
\param p position in syntax tree, should not be NULL
**/

static void reduce_particular_program (NODE_T * p)
{
  NODE_T *q;
  int old_error_count = error_count;
/* A program is "label sequence; particular program" */
  extract_labels (p, SERIAL_CLAUSE /* a fake here, but ok. */ );
/* Parse the program itself. */
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    if (SUB (q) != NULL) {
      reduce_subordinate (q, SOME_CLAUSE);
    }
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, 0);
      f (q, NULL, &z, LABEL, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, 0);
    }
  }
/* Determine the encompassing enclosed clause. */
  for (q = p; q != NULL; FORWARD (q)) {
#ifdef HAVE_POSIX_THREADS
    f (q, NULL, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, 0);
#else
    f (q, par_clause, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, 0);
#endif
    f (q, NULL, NULL, ENCLOSED_CLAUSE, PARALLEL_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, LOOP_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, CODE_CLAUSE, 0);
  }
/* Try reducing the particular program. */
  q = p;
  f (q, NULL, NULL, PARTICULAR_PROGRAM, LABEL, ENCLOSED_CLAUSE, 0);
  f (q, NULL, NULL, PARTICULAR_PROGRAM, ENCLOSED_CLAUSE, 0);
  if (SUB (p) == NULL || NEXT (p) != NULL) {
    recover_from_error (p, PARTICULAR_PROGRAM, (error_count - old_error_count) > MAX_ERRORS);
  }
}

/*!
\brief reduce the sub-phrase that starts one level down
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
\return
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
        SUB (p) = SUB (SUB (p));
      }
    }
  }
}

/*!
\brief driver for reducing a phrase
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
\return
**/

BOOL_T reduce_phrase (NODE_T * p, int expect)
{
  int old_error_count = error_count, old_error_count2;
  BOOL_T declarer_pack = (expect == STRUCTURE_PACK || expect == PARAMETER_PACK || expect == FORMAL_DECLARERS || expect == UNION_PACK || expect == SPECIFIER);
/* Sample all info needed to decide whether a bold tag is operator or indicant. */
  extract_indicants (p);
  if (!declarer_pack) {
    extract_priorities (p);
    extract_operators (p);
  }
  old_error_count2 = error_count;
  elaborate_bold_tags (p);
  if ((error_count - old_error_count2) > 0) {
    longjmp (bottom_up_crash_exit, 1);
  }
/* Now we can reduce declarers, knowing which bold tags are indicants. */
  reduce_declarers (p, expect);
/* Parse the phrase, as appropriate. */
  if (!declarer_pack) {
    old_error_count2 = error_count;
    extract_declarations (p);
    if ((error_count - old_error_count2) > 0) {
      longjmp (bottom_up_crash_exit, 1);
    }
    extract_labels (p, expect);
    reduce_deeper_clauses_driver (p);
    reduce_statements (p, expect);
    reduce_right_to_left_constructs (p);
    reduce_constructs (p, expect);
    reduce_control_structure (p, expect);
  }
/* Do something intelligible if parsing failed. */
  if (SUB (p) == NULL || NEXT (p) != NULL) {
    recover_from_error (p, expect, (error_count - old_error_count) > MAX_ERRORS);
    return (A_FALSE);
  } else {
    return (A_TRUE);
  }
}

/*!
\brief driver for reducing declarers
\param p position in syntax tree, should not be NULL
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
      if (whether (q, OPEN_SYMBOL, COLON_SYMBOL, 0) && !(expect == GENERIC_ARGUMENT || expect == BOUNDS)) {
        reduce_subordinate (q, SPECIFIER);
      }
      if (whether (q, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, 0)) {
        reduce_subordinate (q, PARAMETER_PACK);
      }
      if (whether (q, OPEN_SYMBOL, VOID_SYMBOL, COLON_SYMBOL, 0)) {
        reduce_subordinate (q, PARAMETER_PACK);
      }
    }
  }
}

/*!
\brief driver for reducing control structure elements
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
/* Assignations. */
    if (WHETHER (p, TERTIARY)) {
      f (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, TERTIARY, 0);
      f (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, IDENTITY_RELATION, 0);
      f (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, AND_FUNCTION, 0);
      f (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, OR_FUNCTION, 0);
      f (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, ROUTINE_TEXT, 0);
      f (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, JUMP, 0);
      f (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, SKIP, 0);
      f (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, ASSIGNATION, 0);
    }
/* Routine texts with parameter pack. */
    else if (WHETHER (p, PARAMETER_PACK)) {
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, ASSIGNATION, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, IDENTITY_RELATION, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, AND_FUNCTION, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, OR_FUNCTION, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, JUMP, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, SKIP, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, TERTIARY, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, ROUTINE_TEXT, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, ASSIGNATION, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, IDENTITY_RELATION, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, AND_FUNCTION, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, OR_FUNCTION, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, JUMP, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, SKIP, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, TERTIARY, 0);
      f (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, ROUTINE_TEXT, 0);
    }
/* Routine texts without parameter pack. */
    else if (WHETHER (p, DECLARER)) {
      if (!(PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PARAMETER_PACK))) {
        f (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, ASSIGNATION, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, IDENTITY_RELATION, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, AND_FUNCTION, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, OR_FUNCTION, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, JUMP, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, SKIP, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, TERTIARY, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, ROUTINE_TEXT, 0);
      }
    } else if (WHETHER (p, VOID_SYMBOL)) {
      if (!(PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PARAMETER_PACK))) {
        f (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, ASSIGNATION, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, IDENTITY_RELATION, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, AND_FUNCTION, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, OR_FUNCTION, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, JUMP, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, SKIP, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, TERTIARY, 0);
        f (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, ROUTINE_TEXT, 0);
      }
    }
  }
}

/*!
\brief  graciously ignore extra semicolons
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
\return
**/

static void ignore_superfluous_semicolons (NODE_T * p)
{
/*
This routine relaxes the parser a bit with respect to superfluous semicolons,
for instance "FI; OD". These provoke only a warning.
*/
  for (; p != NULL; FORWARD (p)) {
    ignore_superfluous_semicolons (SUB (p));
    if (NEXT (p) != NULL && WHETHER (NEXT (p), SEMI_SYMBOL) && NEXT (NEXT (p)) == NULL) {
      diagnostic_node (A_WARNING | FORCE_DIAGNOSTIC, NEXT (p), WARNING_SKIPPED_SUPERFLUOUS, ATTRIBUTE (NEXT (p)));
      NEXT (p) = NULL;
    } else if (WHETHER (p, SEMI_SYMBOL) && whether_semicolon_less (NEXT (p))) {
      diagnostic_node (A_WARNING | FORCE_DIAGNOSTIC, p, WARNING_SKIPPED_SUPERFLUOUS, ATTRIBUTE (p));
      if (PREVIOUS (p) != NULL) {
        NEXT (PREVIOUS (p)) = NEXT (p);
      }
      PREVIOUS (NEXT (p)) = PREVIOUS (p);
    }
  }
}

/*!
\brief reduce constructs in proper order
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
**/

static void reduce_control_structure (NODE_T * p, int expect)
{
  reduce_enclosed_clause_bits (p, expect);
  reduce_enclosed_clauses (p);
}

/*!
\brief reduce lengths in declarers
\param p position in syntax tree, should not be NULL
**/

static void reduce_lengtheties (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    f (q, NULL, NULL, LONGETY, LONG_SYMBOL, 0);
    f (q, NULL, NULL, SHORTETY, SHORT_SYMBOL, 0);
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, LONGETY, LONGETY, LONG_SYMBOL, 0);
      f (q, NULL, &z, SHORTETY, SHORTETY, SHORT_SYMBOL, 0);
    }
  }
}

/*!
\brief reduce indicants
\param p position in syntax tree, should not be NULL
**/

static void reduce_indicants (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, INDICANT, INT_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, REAL_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, BITS_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, BYTES_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, COMPLEX_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, COMPL_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, BOOL_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, CHAR_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, FORMAT_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, STRING_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, FILE_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, CHANNEL_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, SEMA_SYMBOL, 0);
    f (q, NULL, NULL, INDICANT, PIPE_SYMBOL, 0);
  }
}

/*!
\brief reduce basic declarations, like LONG BITS, STRING, ..
\param p position in syntax tree, should not be NULL
**/

static void reduce_small_declarers (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (whether (q, LONGETY, INDICANT, 0)) {
      int a;
      if (SUB (NEXT (q)) == NULL) {
        diagnostic_node (A_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
        f (q, NULL, NULL, DECLARER, LONGETY, INDICANT, 0);
      } else {
        a = ATTRIBUTE (SUB (NEXT (q)));
        if (a == INT_SYMBOL || a == REAL_SYMBOL || a == BITS_SYMBOL || a == BYTES_SYMBOL || a == COMPLEX_SYMBOL || a == COMPL_SYMBOL) {
          f (q, NULL, NULL, DECLARER, LONGETY, INDICANT, 0);
        } else {
          diagnostic_node (A_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
          f (q, NULL, NULL, DECLARER, LONGETY, INDICANT, 0);
        }
      }
    } else if (whether (q, SHORTETY, INDICANT, 0)) {
      int a;
      if (SUB (NEXT (q)) == NULL) {
        diagnostic_node (A_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
        f (q, NULL, NULL, DECLARER, SHORTETY, INDICANT, 0);
      } else {
        a = ATTRIBUTE (SUB (NEXT (q)));
        if (a == INT_SYMBOL || a == REAL_SYMBOL || a == BITS_SYMBOL || a == BYTES_SYMBOL || a == COMPLEX_SYMBOL || a == COMPL_SYMBOL) {
          f (q, NULL, NULL, DECLARER, SHORTETY, INDICANT, 0);
        } else {
          diagnostic_node (A_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
          f (q, NULL, NULL, DECLARER, LONGETY, INDICANT, 0);
        }
      }
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, DECLARER, INDICANT, 0);
  }
}

/*!
\brief whether formal bounds
\param p position in syntax tree, should not be NULL
\return whether formal bounds
**/

static BOOL_T whether_formal_bounds (NODE_T * p)
{
  if (p == NULL) {
    return (A_TRUE);
  } else {
    switch (ATTRIBUTE (p)) {
    case OPEN_SYMBOL:
    case CLOSE_SYMBOL:
    case SUB_SYMBOL:
    case BUS_SYMBOL:
    case COMMA_SYMBOL:
    case COLON_SYMBOL:
    case DOTDOT_SYMBOL:
    case INT_DENOTER:
    case IDENTIFIER:
    case OPERATOR:
      return (whether_formal_bounds (SUB (p))
          && whether_formal_bounds (NEXT (p)));
    default:
      return (A_FALSE);
    }
  }
}

/*!
\brief reduce declarer lists for packs
\param p position in syntax tree, should not be NULL
**/

static void reduce_declarer_lists (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (NEXT (q) != NULL && SUB (NEXT (q)) != NULL) {
      if (WHETHER (q, STRUCT_SYMBOL)) {
        reduce_subordinate (NEXT (q), STRUCTURE_PACK);
        f (q, NULL, NULL, DECLARER, STRUCT_SYMBOL, STRUCTURE_PACK, 0);
      } else if (WHETHER (q, UNION_SYMBOL)) {
        reduce_subordinate (NEXT (q), UNION_PACK);
        f (q, NULL, NULL, DECLARER, UNION_SYMBOL, UNION_PACK, 0);
      } else if (WHETHER (q, PROC_SYMBOL)) {
        if (whether (q, PROC_SYMBOL, OPEN_SYMBOL, 0)) {
          if (!whether_formal_bounds (SUB_NEXT (q))) {
            reduce_subordinate (NEXT (q), FORMAL_DECLARERS);
          }
        }
      } else if (WHETHER (q, OP_SYMBOL)) {
        if (whether (q, OP_SYMBOL, OPEN_SYMBOL, 0)) {
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
\param p position in syntax tree, should not be NULL
**/

static void reduce_row_proc_op_declarers (NODE_T * p)
{
  BOOL_T z = A_TRUE;
  while (z) {
    NODE_T *q;
    z = A_FALSE;
    for (q = p; q != NULL; FORWARD (q)) {
/* FLEX DECL. */
      if (whether (q, FLEX_SYMBOL, DECLARER, 0)) {
        f (q, NULL, &z, DECLARER, FLEX_SYMBOL, DECLARER, 0);
      }
/* FLEX [] DECL. */
      if (whether (q, FLEX_SYMBOL, SUB_SYMBOL, DECLARER, 0)
          && SUB (NEXT (q)) != NULL) {
        reduce_subordinate (NEXT (q), BOUNDS);
        f (q, NULL, &z, DECLARER, FLEX_SYMBOL, BOUNDS, DECLARER, 0);
        f (q, NULL, &z, DECLARER, FLEX_SYMBOL, FORMAL_BOUNDS, DECLARER, 0);
      }
/* FLEX () DECL. */
      if (whether (q, FLEX_SYMBOL, OPEN_SYMBOL, DECLARER, 0)
          && SUB (NEXT (q)) != NULL) {
        if (!whether (q, FLEX_SYMBOL, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, 0)) {
          reduce_subordinate (NEXT (q), BOUNDS);
          f (q, NULL, &z, DECLARER, FLEX_SYMBOL, BOUNDS, DECLARER, 0);
          f (q, NULL, &z, DECLARER, FLEX_SYMBOL, FORMAL_BOUNDS, DECLARER, 0);
        }
      }
/* [] DECL. */
      if (whether (q, SUB_SYMBOL, DECLARER, 0) && SUB (q) != NULL) {
        reduce_subordinate (q, BOUNDS);
        f (q, NULL, &z, DECLARER, BOUNDS, DECLARER, 0);
        f (q, NULL, &z, DECLARER, FORMAL_BOUNDS, DECLARER, 0);
      }
/* () DECL. */
      if (whether (q, OPEN_SYMBOL, DECLARER, 0) && SUB (q) != NULL) {
        if (whether (q, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, 0)) {
/* Catch e.g. (INT i) () INT: */
          if (whether_formal_bounds (SUB (q))) {
            reduce_subordinate (q, BOUNDS);
            f (q, NULL, &z, DECLARER, BOUNDS, DECLARER, 0);
            f (q, NULL, &z, DECLARER, FORMAL_BOUNDS, DECLARER, 0);
          }
        } else {
          reduce_subordinate (q, BOUNDS);
          f (q, NULL, &z, DECLARER, BOUNDS, DECLARER, 0);
          f (q, NULL, &z, DECLARER, FORMAL_BOUNDS, DECLARER, 0);
        }
      }
    }
/* PROC DECL, PROC () DECL, OP () DECL. */
    for (q = p; q != NULL; FORWARD (q)) {
      int a = ATTRIBUTE (q);
      if (a == REF_SYMBOL) {
        f (q, NULL, &z, DECLARER, REF_SYMBOL, DECLARER, 0);
      } else if (a == PROC_SYMBOL) {
        f (q, NULL, &z, DECLARER, PROC_SYMBOL, DECLARER, 0);
        f (q, NULL, &z, DECLARER, PROC_SYMBOL, FORMAL_DECLARERS, DECLARER, 0);
        f (q, NULL, &z, DECLARER, PROC_SYMBOL, VOID_SYMBOL, 0);
        f (q, NULL, &z, DECLARER, PROC_SYMBOL, FORMAL_DECLARERS, VOID_SYMBOL, 0);
      } else if (a == OP_SYMBOL) {
        f (q, NULL, &z, OPERATOR_PLAN, OP_SYMBOL, FORMAL_DECLARERS, DECLARER, 0);
        f (q, NULL, &z, OPERATOR_PLAN, OP_SYMBOL, FORMAL_DECLARERS, VOID_SYMBOL, 0);
      }
    }
  }
}

/*!
\brief reduce structure packs
\param p position in syntax tree, should not be NULL
**/

static void reduce_struct_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, STRUCTURED_FIELD, DECLARER, IDENTIFIER, 0);
      f (q, NULL, &z, STRUCTURED_FIELD, STRUCTURED_FIELD, COMMA_SYMBOL, IDENTIFIER, 0);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, 0);
      f (q, NULL, &z, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, COMMA_SYMBOL, STRUCTURED_FIELD, 0);
      f (q, missing_separator, &z, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, 0);
      f (q, wrong_separator, &z, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, SEMI_SYMBOL, STRUCTURED_FIELD, 0);
    }
  }
  q = p;
  f (q, NULL, NULL, STRUCTURE_PACK, OPEN_SYMBOL, STRUCTURED_FIELD_LIST, CLOSE_SYMBOL, 0);
}

/*!
\brief reduce parameter packs
\param p position in syntax tree, should not be NULL
**/

static void reduce_parameter_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, PARAMETER, DECLARER, IDENTIFIER, 0);
      f (q, NULL, &z, PARAMETER, PARAMETER, COMMA_SYMBOL, IDENTIFIER, 0);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, PARAMETER_LIST, PARAMETER, 0);
      f (q, NULL, &z, PARAMETER_LIST, PARAMETER_LIST, COMMA_SYMBOL, PARAMETER, 0);
    }
  }
  q = p;
  f (q, NULL, NULL, PARAMETER_PACK, OPEN_SYMBOL, PARAMETER_LIST, CLOSE_SYMBOL, 0);
}

/*!
\brief reduce formal declarer packs
\param p position in syntax tree, should not be NULL
**/

static void reduce_formal_declarer_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, FORMAL_DECLARERS_LIST, DECLARER, 0);
      f (q, NULL, &z, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, COMMA_SYMBOL, DECLARER, 0);
      f (q, missing_separator, &z, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, DECLARER, 0);
    }
  }
  q = p;
  f (q, NULL, NULL, FORMAL_DECLARERS, OPEN_SYMBOL, FORMAL_DECLARERS_LIST, CLOSE_SYMBOL, 0);
}

/*!
\brief reduce union packs (formal declarers and VOID)
\param p position in syntax tree, should not be NULL
**/

static void reduce_union_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, UNION_DECLARER_LIST, DECLARER, 0);
      f (q, NULL, &z, UNION_DECLARER_LIST, VOID_SYMBOL, 0);
      f (q, NULL, &z, UNION_DECLARER_LIST, UNION_DECLARER_LIST, COMMA_SYMBOL, DECLARER, 0);
      f (q, NULL, &z, UNION_DECLARER_LIST, UNION_DECLARER_LIST, COMMA_SYMBOL, VOID_SYMBOL, 0);
      f (q, missing_separator, &z, UNION_DECLARER_LIST, UNION_DECLARER_LIST, DECLARER, 0);
      f (q, missing_separator, &z, UNION_DECLARER_LIST, UNION_DECLARER_LIST, VOID_SYMBOL, 0);
    }
  }
  q = p;
  f (q, NULL, NULL, UNION_PACK, OPEN_SYMBOL, UNION_DECLARER_LIST, CLOSE_SYMBOL, 0);
}

/*!
\brief reduce specifiers
\param p position in syntax tree, should not be NULL
**/

static void reduce_specifiers (NODE_T * p)
{
  NODE_T *q = p;
  f (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, DECLARER, IDENTIFIER, CLOSE_SYMBOL, 0);
  f (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, DECLARER, CLOSE_SYMBOL, 0);
  f (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, VOID_SYMBOL, CLOSE_SYMBOL, 0);
}

/*!
\brief reduce control structure elements
\param p position in syntax tree, should not be NULL
**/

static void reduce_deeper_clauses (NODE_T * p)
{
  if (WHETHER (p, FORMAT_DELIMITER_SYMBOL)) {
    reduce_subordinate (p, FORMAT_TEXT);
  } else if (WHETHER (p, FORMAT_ITEM_OPEN)) {
    reduce_subordinate (p, FORMAT_TEXT);
  } else if (WHETHER (p, OPEN_SYMBOL)) {
    if (NEXT (p) != NULL && WHETHER (NEXT (p), THEN_BAR_SYMBOL)) {
      reduce_subordinate (p, ENQUIRY_CLAUSE);
    } else if (PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PAR_SYMBOL)) {
      reduce_subordinate (p, COLLATERAL_CLAUSE);
    }
  } else if (WHETHER (p, IF_SYMBOL) || WHETHER (p, ELIF_SYMBOL) || WHETHER (p, CASE_SYMBOL) || WHETHER (p, OUSE_SYMBOL) || WHETHER (p, WHILE_SYMBOL) || WHETHER (p, UNTIL_SYMBOL) || WHETHER (p, ELSE_BAR_SYMBOL) || WHETHER (p, ACCO_SYMBOL)) {
    reduce_subordinate (p, ENQUIRY_CLAUSE);
  } else if (WHETHER (p, BEGIN_SYMBOL)) {
    reduce_subordinate (p, SOME_CLAUSE);
  } else if (WHETHER (p, THEN_SYMBOL) || WHETHER (p, ELSE_SYMBOL) || WHETHER (p, OUT_SYMBOL) || WHETHER (p, DO_SYMBOL) || WHETHER (p, ALT_DO_SYMBOL) || WHETHER (p, CODE_SYMBOL)) {
    reduce_subordinate (p, SERIAL_CLAUSE);
  } else if (WHETHER (p, IN_SYMBOL)) {
    reduce_subordinate (p, COLLATERAL_CLAUSE);
  } else if (WHETHER (p, THEN_BAR_SYMBOL)) {
    reduce_subordinate (p, SOME_CLAUSE);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    reduce_subordinate (p, ENCLOSED_CLAUSE);
  } else if (WHETHER (p, FOR_SYMBOL) || WHETHER (p, FROM_SYMBOL) || WHETHER (p, BY_SYMBOL) || WHETHER (p, TO_SYMBOL) || WHETHER (p, DOWNTO_SYMBOL)) {
    reduce_subordinate (p, UNIT);
  }
}

/*!
\brief reduce primary elements
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
**/

static void reduce_primary_bits (NODE_T * p, int expect)
{
  NODE_T *q = p;
  for (; q != NULL; FORWARD (q)) {
    if (whether (q, IDENTIFIER, OF_SYMBOL, 0)) {
      ATTRIBUTE (q) = FIELD_IDENTIFIER;
    }
    f (q, NULL, NULL, ENVIRON_NAME, ENVIRON_SYMBOL, ROW_CHAR_DENOTER, 0);
    f (q, NULL, NULL, NIHIL, NIL_SYMBOL, 0);
    f (q, NULL, NULL, SKIP, SKIP_SYMBOL, 0);
    f (q, NULL, NULL, SELECTOR, FIELD_IDENTIFIER, OF_SYMBOL, 0);
/* JUMPs without GOTO are resolved later. */
    f (q, NULL, NULL, JUMP, GOTO_SYMBOL, IDENTIFIER, 0);
    f (q, NULL, NULL, DENOTER, LONGETY, INT_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, LONGETY, REAL_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, LONGETY, BITS_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, SHORTETY, INT_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, SHORTETY, REAL_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, SHORTETY, BITS_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, INT_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, REAL_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, BITS_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, ROW_CHAR_DENOTER, 0);
    f (q, NULL, NULL, DENOTER, TRUE_SYMBOL, 0);
    f (q, NULL, NULL, DENOTER, FALSE_SYMBOL, 0);
    f (q, NULL, NULL, DENOTER, EMPTY_SYMBOL, 0);
    if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
      BOOL_T z = A_TRUE;
      while (z) {
        z = A_FALSE;
        f (q, NULL, &z, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, 0);
        f (q, NULL, &z, LABEL, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, 0);
      }
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
#ifdef HAVE_POSIX_THREADS
    f (q, NULL, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, 0);
#else
    f (q, par_clause, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, 0);
#endif
    f (q, NULL, NULL, ENCLOSED_CLAUSE, PARALLEL_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, LOOP_CLAUSE, 0);
    f (q, NULL, NULL, ENCLOSED_CLAUSE, CODE_CLAUSE, 0);
  }
}

/*!
\brief reduce primaries completely
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
**/

static void reduce_primaries (NODE_T * p, int expect)
{
  NODE_T *q = p;
  while (q != NULL) {
    BOOL_T fwd = A_TRUE, z;
/* Primaries excepts call and slice. */
    f (q, NULL, NULL, PRIMARY, IDENTIFIER, 0);
    f (q, NULL, NULL, PRIMARY, DENOTER, 0);
    f (q, NULL, NULL, CAST, DECLARER, ENCLOSED_CLAUSE, 0);
    f (q, NULL, NULL, CAST, VOID_SYMBOL, ENCLOSED_CLAUSE, 0);
    f (q, NULL, NULL, ASSERTION, ASSERT_SYMBOL, ENCLOSED_CLAUSE, 0);
    f (q, NULL, NULL, PRIMARY, CAST, 0);
    f (q, NULL, NULL, PRIMARY, ENCLOSED_CLAUSE, 0);
    f (q, NULL, NULL, PRIMARY, FORMAT_TEXT, 0);
/* Call and slice. */
    z = A_TRUE;
    while (z) {
      NODE_T *x = NEXT (q);
      z = A_FALSE;
      if (WHETHER (q, PRIMARY) && x != NULL) {
        if (WHETHER (x, OPEN_SYMBOL)) {
          reduce_subordinate (NEXT (q), GENERIC_ARGUMENT);
          f (q, NULL, &z, SLICE, PRIMARY, GENERIC_ARGUMENT, 0);
          f (q, NULL, &z, PRIMARY, SLICE, 0);
/*
	  reduce_subordinate (NEXT (q), ARGUMENT);
	  f (q, NULL, &z, CALL, PRIMARY, ARGUMENT, 0);
	  f (q, NULL, &z, PRIMARY, CALL, 0);
*/
        } else if (WHETHER (x, SUB_SYMBOL)) {
          reduce_subordinate (NEXT (q), GENERIC_ARGUMENT);
          f (q, NULL, &z, SLICE, PRIMARY, GENERIC_ARGUMENT, 0);
          f (q, NULL, &z, PRIMARY, SLICE, 0);
        }
      }
    }
/* Now that call and slice are known, reduce remaining ( .. ). */
    if (WHETHER (q, OPEN_SYMBOL) && SUB (q) != NULL) {
      reduce_subordinate (q, SOME_CLAUSE);
      f (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, 0);
      f (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, 0);
      f (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, 0);
      f (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, 0);
      f (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, 0);
      if (PREVIOUS (q) != NULL) {
        q = PREVIOUS (q);
        fwd = A_FALSE;
      }
    }
/* Format text items. */
    if (expect == FORMAT_TEXT) {
      NODE_T *r;
      for (r = p; r != NULL; r = NEXT (r)) {
        f (r, NULL, NULL, DYNAMIC_REPLICATOR, FORMAT_ITEM_N, ENCLOSED_CLAUSE, 0);
        f (r, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_G, ENCLOSED_CLAUSE, 0);
        f (r, NULL, NULL, FORMAT_PATTERN, FORMAT_ITEM_F, ENCLOSED_CLAUSE, 0);
      }
    }
    if (fwd) {
      FORWARD (q);
    }
  }
}

/*!
\brief enforce that ambiguous patterns are separated by commas
\param p position in syntax tree, should not be NULL
\return
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
    case INTEGRAL_PATTERN:     /* These are the potentially ambiguous patterns. */
    case REAL_PATTERN:
    case COMPLEX_PATTERN:
    case BITS_PATTERN:
      {
        if (last_pat != NULL) {
          diagnostic_node (A_SYNTAX_ERROR, q, ERROR_COMMA_MUST_SEPARATE, ATTRIBUTE (last_pat), ATTRIBUTE (q));
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
\param p position in syntax tree, should not be NULL
**/

static void reduce_format_texts (NODE_T * p)
{
  NODE_T *q;
/* Replicators. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, REPLICATOR, STATIC_REPLICATOR, 0);
    f (q, NULL, NULL, REPLICATOR, DYNAMIC_REPLICATOR, 0);
  }
/* "OTHER" patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, STRING_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_S, 0);
    f (q, NULL, NULL, STRING_C_PATTERN, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_S, 0);
    f (q, NULL, NULL, STRING_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_S, 0);
    f (q, NULL, NULL, STRING_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_S, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, INTEGRAL_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_D, 0);
    f (q, NULL, NULL, INTEGRAL_C_PATTERN, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_D, 0);
    f (q, NULL, NULL, INTEGRAL_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, FORMAT_ITEM_D, 0);
    f (q, NULL, NULL, INTEGRAL_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_D, 0);
    f (q, NULL, NULL, INTEGRAL_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_D, 0);
    f (q, NULL, NULL, INTEGRAL_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_D, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_F, 0);
    f (q, NULL, NULL, FIXED_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_F, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FLOAT_C_PATTERN, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, FORMAT_ITEM_E, 0);
  }
/* Radix frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, RADIX_FRAME, REPLICATOR, FORMAT_ITEM_R, 0);
  }
/* Insertions. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, INSERTION, FORMAT_ITEM_X, 0);
    f (q, not_supported, NULL, INSERTION, FORMAT_ITEM_Y, 0);
    f (q, NULL, NULL, INSERTION, FORMAT_ITEM_L, 0);
    f (q, NULL, NULL, INSERTION, FORMAT_ITEM_P, 0);
    f (q, NULL, NULL, INSERTION, FORMAT_ITEM_Q, 0);
    f (q, NULL, NULL, INSERTION, FORMAT_ITEM_K, 0);
    f (q, NULL, NULL, INSERTION, LITERAL, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, INSERTION, REPLICATOR, INSERTION, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, INSERTION, INSERTION, INSERTION, 0);
    }
  }
/* Replicated suppressible frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, FORMAT_A_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_A, 0);
    f (q, NULL, NULL, FORMAT_Z_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_Z, 0);
    f (q, NULL, NULL, FORMAT_D_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_D, 0);
  }
/* Suppressible frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, FORMAT_A_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_A, 0);
    f (q, NULL, NULL, FORMAT_Z_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_Z, 0);
    f (q, NULL, NULL, FORMAT_D_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_D, 0);
    f (q, NULL, NULL, FORMAT_E_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FORMAT_POINT_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_POINT, 0);
    f (q, NULL, NULL, FORMAT_I_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_I, 0);
  }
/* Replicated frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, FORMAT_A_FRAME, REPLICATOR, FORMAT_ITEM_A, 0);
    f (q, NULL, NULL, FORMAT_Z_FRAME, REPLICATOR, FORMAT_ITEM_Z, 0);
    f (q, NULL, NULL, FORMAT_D_FRAME, REPLICATOR, FORMAT_ITEM_D, 0);
  }
/* Frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, FORMAT_A_FRAME, FORMAT_ITEM_A, 0);
    f (q, NULL, NULL, FORMAT_Z_FRAME, FORMAT_ITEM_Z, 0);
    f (q, NULL, NULL, FORMAT_D_FRAME, FORMAT_ITEM_D, 0);
    f (q, NULL, NULL, FORMAT_E_FRAME, FORMAT_ITEM_E, 0);
    f (q, NULL, NULL, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT, 0);
    f (q, NULL, NULL, FORMAT_I_FRAME, FORMAT_ITEM_I, 0);
  }
/* Frames with an insertion. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, FORMAT_A_FRAME, INSERTION, FORMAT_A_FRAME, 0);
    f (q, NULL, NULL, FORMAT_Z_FRAME, INSERTION, FORMAT_Z_FRAME, 0);
    f (q, NULL, NULL, FORMAT_D_FRAME, INSERTION, FORMAT_D_FRAME, 0);
    f (q, NULL, NULL, FORMAT_E_FRAME, INSERTION, FORMAT_E_FRAME, 0);
    f (q, NULL, NULL, FORMAT_POINT_FRAME, INSERTION, FORMAT_POINT_FRAME, 0);
    f (q, NULL, NULL, FORMAT_I_FRAME, INSERTION, FORMAT_I_FRAME, 0);
  }
/* String patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, STRING_PATTERN, REPLICATOR, FORMAT_A_FRAME, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, STRING_PATTERN, FORMAT_A_FRAME, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, STRING_PATTERN, STRING_PATTERN, STRING_PATTERN, 0);
      f (q, NULL, &z, STRING_PATTERN, STRING_PATTERN, INSERTION, STRING_PATTERN, 0);
    }
  }
/* Integral moulds. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, INTEGRAL_MOULD, FORMAT_Z_FRAME, 0);
    f (q, NULL, NULL, INTEGRAL_MOULD, FORMAT_D_FRAME, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z = A_TRUE;
    while (z) {
      z = A_FALSE;
      f (q, NULL, &z, INTEGRAL_MOULD, INTEGRAL_MOULD, INTEGRAL_MOULD, 0);
      f (q, NULL, &z, INTEGRAL_MOULD, INTEGRAL_MOULD, INSERTION, 0);
    }
  }
/* Sign moulds. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_ITEM_PLUS, 0);
    f (q, NULL, NULL, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_ITEM_MINUS, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, SIGN_MOULD, FORMAT_ITEM_PLUS, 0);
    f (q, NULL, NULL, SIGN_MOULD, FORMAT_ITEM_MINUS, 0);
  }
/* Exponent frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, EXPONENT_FRAME, FORMAT_E_FRAME, SIGN_MOULD, INTEGRAL_MOULD, 0);
    f (q, NULL, NULL, EXPONENT_FRAME, FORMAT_E_FRAME, INTEGRAL_MOULD, 0);
  }
/* Real patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, 0);
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, 0);
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, 0);
    f (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, FORMAT_POINT_FRAME, INTEGRAL_MOULD, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, EXPONENT_FRAME, 0);
    f (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, EXPONENT_FRAME, 0);
  }
/* Complex patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, COMPLEX_PATTERN, REAL_PATTERN, FORMAT_I_FRAME, REAL_PATTERN, 0);
  }
/* Bits patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, BITS_PATTERN, RADIX_FRAME, INTEGRAL_MOULD, 0);
  }
/* Integral patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, INTEGRAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, 0);
    f (q, NULL, NULL, INTEGRAL_PATTERN, INTEGRAL_MOULD, 0);
  }
/* Patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, BOOLEAN_PATTERN, FORMAT_ITEM_B, COLLECTION, 0);
    f (q, NULL, NULL, CHOICE_PATTERN, FORMAT_ITEM_C, COLLECTION, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, BOOLEAN_PATTERN, FORMAT_ITEM_B, 0);
    f (q, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_G, 0);
  }
  ambiguous_patterns (p);
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, PATTERN, GENERAL_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, INTEGRAL_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, REAL_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, COMPLEX_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, BITS_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, STRING_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, BOOLEAN_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, CHOICE_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, FORMAT_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, STRING_C_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, INTEGRAL_C_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, FIXED_C_PATTERN, 0);
    f (q, NULL, NULL, PATTERN, FLOAT_C_PATTERN, 0);
  }
/* Pictures. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, PICTURE, INSERTION, 0);
    f (q, NULL, NULL, PICTURE, PATTERN, 0);
    f (q, NULL, NULL, PICTURE, COLLECTION, 0);
    f (q, NULL, NULL, PICTURE, REPLICATOR, COLLECTION, 0);
  }
/* Picture lists. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, PICTURE)) {
      BOOL_T z = A_TRUE;
      f (q, NULL, NULL, PICTURE_LIST, PICTURE, 0);
      while (z) {
        z = A_FALSE;
        f (q, NULL, &z, PICTURE_LIST, PICTURE_LIST, COMMA_SYMBOL, PICTURE, 0);
        /*
         * We filtered ambiguous patterns, so commas may be omitted. 
         */
        f (q, NULL, &z, PICTURE_LIST, PICTURE_LIST, PICTURE, 0);
      }
    }
  }
}

/*!
\brief reduce secondaries completely
\param p position in syntax tree, should not be NULL
**/

static void reduce_secondaries (NODE_T * p)
{
  NODE_T *q;
  BOOL_T z;
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, SECONDARY, PRIMARY, 0);
    f (q, NULL, NULL, GENERATOR, LOC_SYMBOL, DECLARER, 0);
    f (q, NULL, NULL, GENERATOR, HEAP_SYMBOL, DECLARER, 0);
    f (q, NULL, NULL, SECONDARY, GENERATOR, 0);
  }
  z = A_TRUE;
  while (z) {
    z = A_FALSE;
    q = p;
    while (NEXT (q) != NULL) {
      FORWARD (q);
    }
    for (; q != NULL; q = PREVIOUS (q)) {
      f (q, NULL, &z, SELECTION, SELECTOR, SECONDARY, 0);
      f (q, NULL, &z, SECONDARY, SELECTION, 0);
    }
  }
}

/*!
\brief whether "q" is an operator with priority "k"
\param q
\param k
\return whether "q" is an operator with priority "k"
**/

static int operator_with_priority (NODE_T * q, int k)
{
  return (NEXT (q) != NULL && ATTRIBUTE (NEXT (q)) == OPERATOR && PRIO (NEXT (q)->info) == k);
}

/*!
\brief reduce formulae
\param p position in syntax tree, should not be NULL
**/

static void reduce_formulae (NODE_T * p)
{
  NODE_T *q = p;
  int priority;
  while (q != NULL) {
    if (WHETHER (q, OPERATOR) || WHETHER (q, SECONDARY)) {
      q = reduce_dyadic (q, 0);
    } else {
      FORWARD (q);
    }
  }
/* Reduce the expression. */
  for (priority = MAX_PRIORITY; priority >= 0; priority--) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (operator_with_priority (q, priority)) {
        BOOL_T z = A_FALSE;
        NODE_T *op = NEXT (q);
        if (WHETHER (q, SECONDARY)) {
          f (q, NULL, &z, FORMULA, SECONDARY, OPERATOR, SECONDARY, 0);
          f (q, NULL, &z, FORMULA, SECONDARY, OPERATOR, MONADIC_FORMULA, 0);
          f (q, NULL, &z, FORMULA, SECONDARY, OPERATOR, FORMULA, 0);
        } else if (WHETHER (q, MONADIC_FORMULA)) {
          f (q, NULL, &z, FORMULA, MONADIC_FORMULA, OPERATOR, SECONDARY, 0);
          f (q, NULL, &z, FORMULA, MONADIC_FORMULA, OPERATOR, MONADIC_FORMULA, 0);
          f (q, NULL, &z, FORMULA, MONADIC_FORMULA, OPERATOR, FORMULA, 0);
        }
        if (priority == 0 && z) {
          diagnostic_node (A_SYNTAX_ERROR, op, ERROR_NO_PRIORITY, NULL);
        }
        z = A_TRUE;
        while (z) {
          NODE_T *op = NEXT (q);
          z = A_FALSE;
          if (operator_with_priority (q, priority)) {
            f (q, NULL, &z, FORMULA, FORMULA, OPERATOR, SECONDARY, 0);
          }
          if (operator_with_priority (q, priority)) {
            f (q, NULL, &z, FORMULA, FORMULA, OPERATOR, MONADIC_FORMULA, 0);
          }
          if (operator_with_priority (q, priority)) {
            f (q, NULL, &z, FORMULA, FORMULA, OPERATOR, FORMULA, 0);
          }
          if (priority == 0 && z) {
            diagnostic_node (A_SYNTAX_ERROR, op, ERROR_NO_PRIORITY, NULL);
          }
        }
      }
    }
  }
}

/*!
\brief reduce dyadic expressions
\param p position in syntax tree, should not be NULL
\param u
\return
**/

static NODE_T *reduce_dyadic (NODE_T * p, int u)
{
/* We work inside out - higher priority expressions get reduced first. */
  if (u > MAX_PRIORITY) {
    if (p == NULL) {
      return (NULL);
    } else if (WHETHER (p, OPERATOR)) {
/* Reduce monadic formulas. */
      NODE_T *q;
      BOOL_T z;
      q = p;
      do {
        PRIO (q->info) = 10;
        z = (NEXT (q) != NULL) && (WHETHER (NEXT (q), OPERATOR));
        if (z) {
          FORWARD (q);
        }
      }
      while (z);
      f (q, NULL, NULL, MONADIC_FORMULA, OPERATOR, SECONDARY, 0);
      while (q != p) {
        q = PREVIOUS (q);
        f (q, NULL, NULL, MONADIC_FORMULA, OPERATOR, MONADIC_FORMULA, 0);
      }
    }
    FORWARD (p);
  } else {
    p = reduce_dyadic (p, u + 1);
    while (p != NULL && WHETHER (p, OPERATOR) && PRIO (p->info) == u) {
      FORWARD (p);
      p = reduce_dyadic (p, u + 1);
    }
  }
  return (p);
}

/*!
\brief reduce tertiaries completely
\param p position in syntax tree, should not be NULL
**/

static void reduce_tertiaries (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, TERTIARY, NIHIL, 0);
    f (q, NULL, NULL, FORMULA, MONADIC_FORMULA, 0);
    f (q, NULL, NULL, TERTIARY, FORMULA, 0);
    f (q, NULL, NULL, TERTIARY, SECONDARY, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, IDENTITY_RELATION, TERTIARY, IS_SYMBOL, TERTIARY, 0);
    f (q, NULL, NULL, IDENTITY_RELATION, TERTIARY, ISNT_SYMBOL, TERTIARY, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, AND_FUNCTION, TERTIARY, ANDF_SYMBOL, TERTIARY, 0);
    f (q, NULL, NULL, AND_FUNCTION, TERTIARY, ANDTH_SYMBOL, TERTIARY, 0);
    f (q, NULL, NULL, OR_FUNCTION, TERTIARY, ORF_SYMBOL, TERTIARY, 0);
    f (q, NULL, NULL, OR_FUNCTION, TERTIARY, OREL_SYMBOL, TERTIARY, 0);
  }
}

/*!
\brief reduce declarations
\param p position in syntax tree, should not be NULL
**/

static void reduce_basic_declarations (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, ENVIRON_NAME, ENVIRON_SYMBOL, ROW_CHAR_DENOTER, 0);
    f (q, NULL, NULL, PRIORITY_DECLARATION, PRIO_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, PRIORITY, 0);
    f (q, NULL, NULL, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, DECLARER, 0);
    f (q, NULL, NULL, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, VOID_SYMBOL, 0);
    f (q, NULL, NULL, PROCEDURE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, ROUTINE_TEXT, 0);
    f (q, NULL, NULL, PROCEDURE_VARIABLE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, 0);
    f (q, NULL, NULL, PROCEDURE_VARIABLE_DECLARATION, QUALIFIER, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, 0);
    f (q, NULL, NULL, BRIEF_OPERATOR_DECLARATION, OP_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, ROUTINE_TEXT, 0);
/* Errors. WILDCARD catches TERTIARY which catches IDENTIFIER. */
    f (q, missing_symbol, NULL, PROCEDURE_DECLARATION, PROC_SYMBOL, WILDCARD, ROUTINE_TEXT, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z;
    do {
      z = A_FALSE;
      f (q, NULL, &z, ENVIRON_NAME, ENVIRON_NAME, COMMA_SYMBOL, ROW_CHAR_DENOTER, 0);
      f (q, NULL, &z, PRIORITY_DECLARATION, PRIORITY_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, PRIORITY, 0);
      f (q, NULL, &z, MODE_DECLARATION, MODE_DECLARATION, COMMA_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, DECLARER, 0);
      f (q, NULL, &z, MODE_DECLARATION, MODE_DECLARATION, COMMA_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, VOID_SYMBOL, 0);
      f (q, NULL, &z, PROCEDURE_DECLARATION, PROCEDURE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, ROUTINE_TEXT, 0);
      f (q, NULL, &z, PROCEDURE_VARIABLE_DECLARATION, PROCEDURE_VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, 0);
      f (q, NULL, &z, BRIEF_OPERATOR_DECLARATION, BRIEF_OPERATOR_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, ROUTINE_TEXT, 0);
/* Errors. WILDCARD catches TERTIARY which catches IDENTIFIER. */
      f (q, missing_symbol, &z, PROCEDURE_DECLARATION, PROCEDURE_DECLARATION, COMMA_SYMBOL, WILDCARD, ROUTINE_TEXT, 0);
    }
    while (z);
  }
}

/*!
\brief reduce units
\param p position in syntax tree, should not be NULL
**/

static void reduce_units (NODE_T * p)
{
  NODE_T *q;
/* Stray ~ is a SKIP. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, OPERATOR) && WHETHER_LITERALLY (q, "~")) {
      ATTRIBUTE (q) = SKIP;
    }
  }
/* Reduce units. */
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, UNIT, ASSIGNATION, 0);
    f (q, NULL, NULL, UNIT, IDENTITY_RELATION, 0);
    f (q, NULL, NULL, UNIT, AND_FUNCTION, 0);
    f (q, NULL, NULL, UNIT, OR_FUNCTION, 0);
    f (q, NULL, NULL, UNIT, ROUTINE_TEXT, 0);
    f (q, NULL, NULL, UNIT, JUMP, 0);
    f (q, NULL, NULL, UNIT, SKIP, 0);
    f (q, NULL, NULL, UNIT, TERTIARY, 0);
    f (q, NULL, NULL, UNIT, ASSERTION, 0);
  }
}

/*!
\brief reduce_generic arguments
\param p position in syntax tree, should not be NULL
**/

static void reduce_generic_arguments (NODE_T * p)
{
  NODE_T *q;
  BOOL_T z;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, UNIT)) {
      f (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, UNIT, AT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, AT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, 0);
      f (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, UNIT, AT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, AT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, 0);
    } else if (WHETHER (q, COLON_SYMBOL)) {
      f (q, NULL, NULL, TRIMMER, COLON_SYMBOL, UNIT, AT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, COLON_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, COLON_SYMBOL, AT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, COLON_SYMBOL, 0);
    } else if (WHETHER (q, DOTDOT_SYMBOL)) {
      f (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, UNIT, AT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, AT_SYMBOL, UNIT, 0);
      f (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, 0);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, TRIMMER, AT_SYMBOL, UNIT, 0);
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
  ABNORMAL_END (q == NULL, "erroneous parser state", NULL);
  f (q, NULL, NULL, GENERIC_ARGUMENT_LIST, UNIT, 0);
  f (q, NULL, NULL, GENERIC_ARGUMENT_LIST, TRIMMER, 0);
  do {
    z = A_FALSE;
    f (q, NULL, &z, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, COMMA_SYMBOL, UNIT, 0);
    f (q, NULL, &z, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, COMMA_SYMBOL, TRIMMER, 0);
    f (q, missing_separator, &z, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, UNIT, 0);
    f (q, missing_separator, &z, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, TRIMMER, 0);
  }
  while (z);
}

/*!
\brief reduce bounds
\param p position in syntax tree, should not be NULL
**/

static void reduce_bounds (NODE_T * p)
{
  NODE_T *q;
  BOOL_T z;
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, BOUND, UNIT, COLON_SYMBOL, UNIT, 0);
    f (q, NULL, NULL, BOUND, UNIT, DOTDOT_SYMBOL, UNIT, 0);
    f (q, NULL, NULL, BOUND, UNIT, 0);
  }
  q = NEXT (p);
  f (q, NULL, NULL, BOUNDS_LIST, BOUND, 0);
  f (q, NULL, NULL, FORMAL_BOUNDS_LIST, COMMA_SYMBOL, 0);
  f (q, NULL, NULL, ALT_FORMAL_BOUNDS_LIST, COLON_SYMBOL, 0);
  f (q, NULL, NULL, ALT_FORMAL_BOUNDS_LIST, DOTDOT_SYMBOL, 0);
  do {
    z = A_FALSE;
    f (q, NULL, &z, BOUNDS_LIST, BOUNDS_LIST, COMMA_SYMBOL, BOUND, 0);
    f (q, NULL, &z, FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, COMMA_SYMBOL, 0);
    f (q, NULL, &z, ALT_FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, COLON_SYMBOL, 0);
    f (q, NULL, &z, ALT_FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, DOTDOT_SYMBOL, 0);
    f (q, NULL, &z, FORMAL_BOUNDS_LIST, ALT_FORMAL_BOUNDS_LIST, COMMA_SYMBOL, 0);
    f (q, missing_separator, &z, BOUNDS_LIST, BOUNDS_LIST, BOUND, 0);
  }
  while (z);
}

/*!
\brief reduce argument packs
\param p position in syntax tree, should not be NULL
**/

static void reduce_arguments (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T z;
    f (q, NULL, NULL, ARGUMENT_LIST, UNIT, 0);
    do {
      z = A_FALSE;
      f (q, NULL, &z, ARGUMENT_LIST, ARGUMENT_LIST, COMMA_SYMBOL, UNIT, 0);
      f (q, missing_separator, &z, ARGUMENT_LIST, ARGUMENT_LIST, UNIT, 0);
    }
    while (z);
  }
}

/*!
\brief reduce declaration lists
\param p position in syntax tree, should not be NULL
**/

static void reduce_declaration_lists (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, IDENTITY_DECLARATION, DECLARER, DEFINING_IDENTIFIER, EQUALS_SYMBOL, UNIT, 0);
    f (q, NULL, NULL, VARIABLE_DECLARATION, QUALIFIER, DECLARER, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0);
    f (q, NULL, NULL, VARIABLE_DECLARATION, QUALIFIER, DECLARER, DEFINING_IDENTIFIER, 0);
    f (q, NULL, NULL, VARIABLE_DECLARATION, DECLARER, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0);
    f (q, NULL, NULL, VARIABLE_DECLARATION, DECLARER, DEFINING_IDENTIFIER, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z;
    do {
      z = A_FALSE;
      f (q, NULL, &z, IDENTITY_DECLARATION, IDENTITY_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, UNIT, 0);
      f (q, NULL, &z, VARIABLE_DECLARATION, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0);
      if (!whether (q, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, 0)) {
        f (q, NULL, &z, VARIABLE_DECLARATION, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, 0);
      }
    }
    while (z);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, OPERATOR_DECLARATION, OPERATOR_PLAN, DEFINING_OPERATOR, EQUALS_SYMBOL, UNIT, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z;
    do {
      z = A_FALSE;
      f (q, NULL, &z, OPERATOR_DECLARATION, OPERATOR_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, UNIT, 0);
    }
    while (z);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, DECLARATION_LIST, MODE_DECLARATION, 0);
    f (q, NULL, NULL, DECLARATION_LIST, PRIORITY_DECLARATION, 0);
    f (q, NULL, NULL, DECLARATION_LIST, BRIEF_OPERATOR_DECLARATION, 0);
    f (q, NULL, NULL, DECLARATION_LIST, OPERATOR_DECLARATION, 0);
    f (q, NULL, NULL, DECLARATION_LIST, IDENTITY_DECLARATION, 0);
    f (q, NULL, NULL, DECLARATION_LIST, PROCEDURE_DECLARATION, 0);
    f (q, NULL, NULL, DECLARATION_LIST, PROCEDURE_VARIABLE_DECLARATION, 0);
    f (q, NULL, NULL, DECLARATION_LIST, VARIABLE_DECLARATION, 0);
    f (q, NULL, NULL, DECLARATION_LIST, ENVIRON_NAME, 0);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T z;
    do {
      z = A_FALSE;
      f (q, NULL, &z, DECLARATION_LIST, DECLARATION_LIST, COMMA_SYMBOL, DECLARATION_LIST, 0);
    }
    while (z);
  }
}

/*!
\brief reduce labels and specifiers
\param p position in syntax tree, should not be NULL
**/

static void reduce_labels (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    f (q, NULL, NULL, LABELED_UNIT, LABEL, UNIT, 0);
    f (q, NULL, NULL, SPECIFIED_UNIT, SPECIFIER, COLON_SYMBOL, UNIT, 0);
  }
}

/*!
\brief signal badly used exits
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
\return
**/

static void precheck_serial_clause (NODE_T * q)
{
  NODE_T *p;
  BOOL_T label_seen = A_FALSE;
/* Wrong exits. */
  for (p = q; p != NULL; FORWARD (p)) {
    if (WHETHER (p, EXIT_SYMBOL)) {
      if (NEXT (p) == NULL || WHETHER_NOT (NEXT (p), LABELED_UNIT)) {
        diagnostic_node (A_SYNTAX_ERROR, p, ERROR_LABELED_UNIT_MUST_FOLLOW);
      }
    }
  }
/* Wrong jumps and declarations. */
  for (p = q; p != NULL; FORWARD (p)) {
    if (WHETHER (p, LABELED_UNIT)) {
      label_seen = A_TRUE;
    } else if (WHETHER (p, DECLARATION_LIST)) {
      if (label_seen) {
        diagnostic_node (A_SYNTAX_ERROR, p, ERROR_LABEL_BEFORE_DECLARATION);
      }
    }
  }
}

/*!
\brief reduce serial clauses
\param p position in syntax tree, should not be NULL
**/

static void reduce_serial_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T z;
    precheck_serial_clause (p);
    f (q, NULL, NULL, SERIAL_CLAUSE, LABELED_UNIT, 0);
    f (q, NULL, NULL, SERIAL_CLAUSE, UNIT, 0);
    f (q, NULL, NULL, INITIALISER_SERIES, DECLARATION_LIST, 0);
    do {
      z = A_FALSE;
      if (WHETHER (q, SERIAL_CLAUSE)) {
        f (q, NULL, &z, SERIAL_CLAUSE, SERIAL_CLAUSE, SEMI_SYMBOL, UNIT, 0);
        f (q, NULL, &z, SERIAL_CLAUSE, SERIAL_CLAUSE, EXIT_SYMBOL, LABELED_UNIT, 0);
        f (q, NULL, &z, SERIAL_CLAUSE, SERIAL_CLAUSE, SEMI_SYMBOL, LABELED_UNIT, 0);
        f (q, NULL, &z, INITIALISER_SERIES, SERIAL_CLAUSE, SEMI_SYMBOL, DECLARATION_LIST, 0);
        /*
         * Errors. 
         */
        f (q, wrong_separator, &z, SERIAL_CLAUSE, SERIAL_CLAUSE, COMMA_SYMBOL, UNIT, 0);
        f (q, wrong_separator, &z, SERIAL_CLAUSE, SERIAL_CLAUSE, COMMA_SYMBOL, LABELED_UNIT, 0);
        f (q, wrong_separator, &z, INITIALISER_SERIES, SERIAL_CLAUSE, COMMA_SYMBOL, DECLARATION_LIST, 0);
        f (q, missing_separator, &z, SERIAL_CLAUSE, SERIAL_CLAUSE, UNIT, 0);
        f (q, missing_separator, &z, SERIAL_CLAUSE, SERIAL_CLAUSE, LABELED_UNIT, 0);
        f (q, missing_separator, &z, INITIALISER_SERIES, SERIAL_CLAUSE, DECLARATION_LIST, 0);
      } else if (WHETHER (q, INITIALISER_SERIES)) {
        f (q, NULL, &z, SERIAL_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, UNIT, 0);
        f (q, NULL, &z, SERIAL_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, LABELED_UNIT, 0);
        f (q, NULL, &z, INITIALISER_SERIES, INITIALISER_SERIES, SEMI_SYMBOL, DECLARATION_LIST, 0);
        /*
         * Errors. 
         */
        f (q, wrong_separator, &z, SERIAL_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, UNIT, 0);
        f (q, wrong_separator, &z, SERIAL_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, LABELED_UNIT, 0);
        f (q, wrong_separator, &z, INITIALISER_SERIES, INITIALISER_SERIES, COMMA_SYMBOL, DECLARATION_LIST, 0);
        f (q, missing_separator, &z, SERIAL_CLAUSE, INITIALISER_SERIES, UNIT, 0);
        f (q, missing_separator, &z, SERIAL_CLAUSE, INITIALISER_SERIES, LABELED_UNIT, 0);
        f (q, missing_separator, &z, INITIALISER_SERIES, INITIALISER_SERIES, DECLARATION_LIST, 0);
      }
    }
    while (z);
  }
}

/*!
\brief reduce enquiry clauses
\param p position in syntax tree, should not be NULL
**/

static void reduce_enquiry_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T z;
    f (q, NULL, NULL, ENQUIRY_CLAUSE, UNIT, 0);
    f (q, NULL, NULL, INITIALISER_SERIES, DECLARATION_LIST, 0);
    do {
      z = A_FALSE;
      if (WHETHER (q, ENQUIRY_CLAUSE)) {
        f (q, NULL, &z, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, SEMI_SYMBOL, UNIT, 0);
        f (q, NULL, &z, INITIALISER_SERIES, ENQUIRY_CLAUSE, SEMI_SYMBOL, DECLARATION_LIST, 0);
        f (q, missing_separator, &z, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, UNIT, 0);
        f (q, missing_separator, &z, INITIALISER_SERIES, ENQUIRY_CLAUSE, DECLARATION_LIST, 0);
      } else if (WHETHER (q, INITIALISER_SERIES)) {
        f (q, NULL, &z, ENQUIRY_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, UNIT, 0);
        f (q, NULL, &z, INITIALISER_SERIES, INITIALISER_SERIES, SEMI_SYMBOL, DECLARATION_LIST, 0);
        f (q, missing_separator, &z, ENQUIRY_CLAUSE, INITIALISER_SERIES, UNIT, 0);
        f (q, missing_separator, &z, INITIALISER_SERIES, INITIALISER_SERIES, DECLARATION_LIST, 0);
      }
    }
    while (z);
  }
}

/*!
\brief reduce collateral clauses
\param p position in syntax tree, should not be NULL
**/

static void reduce_collateral_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    if (WHETHER (q, UNIT)) {
      BOOL_T z;
      f (q, NULL, NULL, UNIT_LIST, UNIT, 0);
      do {
        z = A_FALSE;
        f (q, NULL, &z, UNIT_LIST, UNIT_LIST, COMMA_SYMBOL, UNIT, 0);
        f (q, missing_separator, &z, UNIT_LIST, UNIT_LIST, UNIT, 0);
      }
      while (z);
    } else if (WHETHER (q, SPECIFIED_UNIT)) {
      BOOL_T z;
      f (q, NULL, NULL, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, 0);
      do {
        z = A_FALSE;
        f (q, NULL, &z, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT_LIST, COMMA_SYMBOL, SPECIFIED_UNIT, 0);
        f (q, missing_separator, &z, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, 0);
      }
      while (z);
    }
  }
}

/*!
\brief reduces clause parts, before reducing the clause itself
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
**/

static void reduce_enclosed_clause_bits (NODE_T * p, int expect)
{
  if (SUB (p) != NULL) {
    return;
  }
  if (WHETHER (p, FOR_SYMBOL)) {
    f (p, NULL, NULL, FOR_PART, FOR_SYMBOL, DEFINING_IDENTIFIER, 0);
  } else if (WHETHER (p, OPEN_SYMBOL)) {
    if (expect == ENQUIRY_CLAUSE) {
      f (p, NULL, NULL, OPEN_PART, OPEN_SYMBOL, ENQUIRY_CLAUSE, 0);
    } else if (expect == ARGUMENT) {
      f (p, NULL, NULL, ARGUMENT, OPEN_SYMBOL, CLOSE_SYMBOL, 0);
      f (p, NULL, NULL, ARGUMENT, OPEN_SYMBOL, ARGUMENT_LIST, CLOSE_SYMBOL, 0);
      f (p, empty_clause, NULL, ARGUMENT, OPEN_SYMBOL, INITIALISER_SERIES, CLOSE_SYMBOL, 0);
    } else if (expect == GENERIC_ARGUMENT) {
      if (whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, 0)) {
        pad_node (p, TRIMMER);
        f (p, NULL, NULL, GENERIC_ARGUMENT, OPEN_SYMBOL, TRIMMER, CLOSE_SYMBOL, 0);
      }
      f (p, NULL, NULL, GENERIC_ARGUMENT, OPEN_SYMBOL, GENERIC_ARGUMENT_LIST, CLOSE_SYMBOL, 0);
    } else if (expect == BOUNDS) {
      f (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, CLOSE_SYMBOL, 0);
      f (p, NULL, NULL, BOUNDS, OPEN_SYMBOL, BOUNDS_LIST, CLOSE_SYMBOL, 0);
      f (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, FORMAL_BOUNDS_LIST, CLOSE_SYMBOL, 0);
      f (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, ALT_FORMAL_BOUNDS_LIST, CLOSE_SYMBOL, 0);
    } else {
      f (p, NULL, NULL, CLOSED_CLAUSE, OPEN_SYMBOL, SERIAL_CLAUSE, CLOSE_SYMBOL, 0);
      f (p, NULL, NULL, COLLATERAL_CLAUSE, OPEN_SYMBOL, UNIT_LIST, CLOSE_SYMBOL, 0);
      f (p, NULL, NULL, COLLATERAL_CLAUSE, OPEN_SYMBOL, CLOSE_SYMBOL, 0);
      f (p, empty_clause, NULL, CLOSED_CLAUSE, OPEN_SYMBOL, INITIALISER_SERIES, CLOSE_SYMBOL, 0);
    }
  } else if (WHETHER (p, SUB_SYMBOL)) {
    if (expect == GENERIC_ARGUMENT) {
      if (whether (p, SUB_SYMBOL, BUS_SYMBOL, 0)) {
        pad_node (p, TRIMMER);
        f (p, NULL, NULL, GENERIC_ARGUMENT, SUB_SYMBOL, TRIMMER, BUS_SYMBOL, 0);
      }
      f (p, NULL, NULL, GENERIC_ARGUMENT, SUB_SYMBOL, GENERIC_ARGUMENT_LIST, BUS_SYMBOL, 0);
    } else if (expect == BOUNDS) {
      f (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, BUS_SYMBOL, 0);
      f (p, NULL, NULL, BOUNDS, SUB_SYMBOL, BOUNDS_LIST, BUS_SYMBOL, 0);
      f (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, FORMAL_BOUNDS_LIST, BUS_SYMBOL, 0);
      f (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, ALT_FORMAL_BOUNDS_LIST, BUS_SYMBOL, 0);
    }
  } else if (WHETHER (p, BEGIN_SYMBOL)) {
    f (p, NULL, NULL, COLLATERAL_CLAUSE, BEGIN_SYMBOL, UNIT_LIST, END_SYMBOL, 0);
    f (p, NULL, NULL, COLLATERAL_CLAUSE, BEGIN_SYMBOL, END_SYMBOL, 0);
    f (p, NULL, NULL, CLOSED_CLAUSE, BEGIN_SYMBOL, SERIAL_CLAUSE, END_SYMBOL, 0);
    f (p, empty_clause, NULL, CLOSED_CLAUSE, BEGIN_SYMBOL, INITIALISER_SERIES, END_SYMBOL, 0);
  } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL)) {
    f (p, NULL, NULL, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL, PICTURE_LIST, FORMAT_DELIMITER_SYMBOL, 0);
    f (p, NULL, NULL, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL, FORMAT_DELIMITER_SYMBOL, 0);
  } else if (WHETHER (p, FORMAT_ITEM_OPEN)) {
    f (p, NULL, NULL, COLLECTION, FORMAT_ITEM_OPEN, PICTURE_LIST, FORMAT_ITEM_CLOSE, 0);
  } else if (WHETHER (p, CODE_SYMBOL)) {
    f (p, NULL, NULL, CODE_CLAUSE, CODE_SYMBOL, SERIAL_CLAUSE, EDOC_SYMBOL, 0);
  } else if (WHETHER (p, IF_SYMBOL)) {
    f (p, NULL, NULL, IF_PART, IF_SYMBOL, ENQUIRY_CLAUSE, 0);
    f (p, empty_clause, NULL, IF_PART, IF_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, THEN_SYMBOL)) {
    f (p, NULL, NULL, THEN_PART, THEN_SYMBOL, SERIAL_CLAUSE, 0);
    f (p, empty_clause, NULL, THEN_PART, THEN_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, ELSE_SYMBOL)) {
    f (p, NULL, NULL, ELSE_PART, ELSE_SYMBOL, SERIAL_CLAUSE, 0);
    f (p, empty_clause, NULL, ELSE_PART, ELSE_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, ELIF_SYMBOL)) {
    f (p, NULL, NULL, ELIF_IF_PART, ELIF_SYMBOL, ENQUIRY_CLAUSE, 0);
  } else if (WHETHER (p, CASE_SYMBOL)) {
    f (p, NULL, NULL, CASE_PART, CASE_SYMBOL, ENQUIRY_CLAUSE, 0);
    f (p, empty_clause, NULL, CASE_PART, CASE_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, IN_SYMBOL)) {
    f (p, NULL, NULL, INTEGER_IN_PART, IN_SYMBOL, UNIT_LIST, 0);
    f (p, NULL, NULL, UNITED_IN_PART, IN_SYMBOL, SPECIFIED_UNIT_LIST, 0);
  } else if (WHETHER (p, OUT_SYMBOL)) {
    f (p, NULL, NULL, OUT_PART, OUT_SYMBOL, SERIAL_CLAUSE, 0);
    f (p, empty_clause, NULL, OUT_PART, OUT_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, OUSE_SYMBOL)) {
    f (p, NULL, NULL, OUSE_CASE_PART, OUSE_SYMBOL, ENQUIRY_CLAUSE, 0);
  } else if (WHETHER (p, THEN_BAR_SYMBOL)) {
    f (p, NULL, NULL, CHOICE, THEN_BAR_SYMBOL, SERIAL_CLAUSE, 0);
    f (p, NULL, NULL, INTEGER_CHOICE_CLAUSE, THEN_BAR_SYMBOL, UNIT_LIST, 0);
    f (p, NULL, NULL, UNITED_CHOICE, THEN_BAR_SYMBOL, SPECIFIED_UNIT_LIST, 0);
    f (p, NULL, NULL, UNITED_CHOICE, THEN_BAR_SYMBOL, SPECIFIED_UNIT, 0);
    f (p, empty_clause, NULL, CHOICE, THEN_BAR_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, ELSE_BAR_SYMBOL)) {
    f (p, NULL, NULL, ELSE_OPEN_PART, ELSE_BAR_SYMBOL, ENQUIRY_CLAUSE, 0);
    f (p, empty_clause, NULL, ELSE_OPEN_PART, ELSE_BAR_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, FROM_SYMBOL)) {
    f (p, NULL, NULL, FROM_PART, FROM_SYMBOL, UNIT, 0);
  } else if (WHETHER (p, BY_SYMBOL)) {
    f (p, NULL, NULL, BY_PART, BY_SYMBOL, UNIT, 0);
  } else if (WHETHER (p, TO_SYMBOL)) {
    f (p, NULL, NULL, TO_PART, TO_SYMBOL, UNIT, 0);
  } else if (WHETHER (p, DOWNTO_SYMBOL)) {
    f (p, NULL, NULL, TO_PART, DOWNTO_SYMBOL, UNIT, 0);
  } else if (WHETHER (p, WHILE_SYMBOL)) {
    f (p, NULL, NULL, WHILE_PART, WHILE_SYMBOL, ENQUIRY_CLAUSE, 0);
    f (p, empty_clause, NULL, WHILE_PART, WHILE_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, UNTIL_SYMBOL)) {
    f (p, NULL, NULL, UNTIL_PART, UNTIL_SYMBOL, ENQUIRY_CLAUSE, 0);
    f (p, empty_clause, NULL, UNTIL_PART, UNTIL_SYMBOL, INITIALISER_SERIES, 0);
  } else if (WHETHER (p, DO_SYMBOL)) {
    f (p, NULL, NULL, DO_PART, DO_SYMBOL, SERIAL_CLAUSE, UNTIL_PART, OD_SYMBOL, 0);
    f (p, NULL, NULL, DO_PART, DO_SYMBOL, SERIAL_CLAUSE, OD_SYMBOL, 0);
    f (p, NULL, NULL, DO_PART, DO_SYMBOL, UNTIL_PART, OD_SYMBOL, 0);
  } else if (WHETHER (p, ALT_DO_SYMBOL)) {
    f (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, SERIAL_CLAUSE, UNTIL_PART, OD_SYMBOL, 0);
    f (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, SERIAL_CLAUSE, OD_SYMBOL, 0);
    f (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, UNTIL_PART, OD_SYMBOL, 0);
  }
}

/*!
\brief reduce enclosed clauses
\param p position in syntax tree, should not be NULL
**/

static void reduce_enclosed_clauses (NODE_T * p)
{
  if (SUB (p) == NULL) {
    return;
  }
  if (WHETHER (p, OPEN_PART)) {
    f (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, BRIEF_ELIF_IF_PART, 0);
    f (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, BRIEF_INTEGER_OUSE_PART, 0);
    f (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, BRIEF_UNITED_OUSE_PART, 0);
  } else if (WHETHER (p, ELSE_OPEN_PART)) {
    f (p, NULL, NULL, BRIEF_ELIF_IF_PART, ELSE_OPEN_PART, CHOICE, CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, BRIEF_ELIF_IF_PART, ELSE_OPEN_PART, CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, BRIEF_ELIF_IF_PART, ELSE_OPEN_PART, CHOICE, BRIEF_ELIF_IF_PART, 0);
    f (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, BRIEF_INTEGER_OUSE_PART, 0);
    f (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, CLOSE_SYMBOL, 0);
    f (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, BRIEF_UNITED_OUSE_PART, 0);
  } else if (WHETHER (p, IF_PART)) {
    f (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, ELSE_PART, FI_SYMBOL, 0);
    f (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, ELIF_PART, 0);
    f (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, FI_SYMBOL, 0);
  } else if (WHETHER (p, ELIF_IF_PART)) {
    f (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, ELSE_PART, FI_SYMBOL, 0);
    f (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, FI_SYMBOL, 0);
    f (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, ELIF_PART, 0);
  } else if (WHETHER (p, CASE_PART)) {
    f (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, OUT_PART, ESAC_SYMBOL, 0);
    f (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, ESAC_SYMBOL, 0);
    f (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, INTEGER_OUT_PART, 0);
    f (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, OUT_PART, ESAC_SYMBOL, 0);
    f (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, ESAC_SYMBOL, 0);
    f (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, UNITED_OUSE_PART, 0);
  } else if (WHETHER (p, OUSE_CASE_PART)) {
    f (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, OUT_PART, ESAC_SYMBOL, 0);
    f (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, ESAC_SYMBOL, 0);
    f (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, INTEGER_OUT_PART, 0);
    f (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, OUT_PART, ESAC_SYMBOL, 0);
    f (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, ESAC_SYMBOL, 0);
    f (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, UNITED_OUSE_PART, 0);
  } else if (WHETHER (p, FOR_PART)) {
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, TO_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, TO_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, TO_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, TO_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, TO_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, TO_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, ALT_DO_PART, 0);
  } else if (WHETHER (p, FROM_PART)) {
    f (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, TO_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, TO_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, TO_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, ALT_DO_PART, 0);
  } else if (WHETHER (p, BY_PART)) {
    f (p, NULL, NULL, LOOP_CLAUSE, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, BY_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, BY_PART, TO_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, BY_PART, ALT_DO_PART, 0);
  } else if (WHETHER (p, TO_PART)) {
    f (p, NULL, NULL, LOOP_CLAUSE, TO_PART, WHILE_PART, ALT_DO_PART, 0);
    f (p, NULL, NULL, LOOP_CLAUSE, TO_PART, ALT_DO_PART, 0);
  } else if (WHETHER (p, WHILE_PART)) {
    f (p, NULL, NULL, LOOP_CLAUSE, WHILE_PART, ALT_DO_PART, 0);
  } else if (WHETHER (p, DO_PART)) {
    f (p, NULL, NULL, LOOP_CLAUSE, DO_PART, 0);
  }
}

/*!
\brief substitute reduction when a phrase could not be parsed
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
\param suppress suppresses a diagnostic_node message (nested c.q. related diagnostics)
\return
**/

static void recover_from_error (NODE_T * p, int expect, BOOL_T suppress)
{
/* This routine does not do fancy things as that might introduce more errors. */ NODE_T *q = p;
  if (p == NULL) {
    return;
  }
  if (!suppress) {
/* Give a general error message. */
    if (expect == SOME_CLAUSE) {
      expect = serial_or_collateral (p);
    }
    diagnostic_node (A_SYNTAX_ERROR, p, ERROR_INVALID_SEQUENCE, expect, phrase_to_text (p, NULL), NULL);
    if (error_count >= MAX_ERRORS) {
      longjmp (bottom_up_crash_exit, 1);
    }
  }
/* Try to prevent spurious diagnostics by guessing what was expected. */
  while (NEXT (q) != NULL) {
    FORWARD (q);
  }
  if (WHETHER (p, BEGIN_SYMBOL) || WHETHER (p, OPEN_SYMBOL)) {
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
  } else if (WHETHER (p, THEN_BAR_SYMBOL) || WHETHER (p, CHOICE)) {
    make_sub (p, q, CHOICE);
  } else if (WHETHER (p, IF_SYMBOL) || WHETHER (p, IF_PART)) {
    make_sub (p, q, IF_PART);
  } else if (WHETHER (p, THEN_SYMBOL) || WHETHER (p, THEN_PART)) {
    make_sub (p, q, THEN_PART);
  } else if (WHETHER (p, ELSE_SYMBOL) || WHETHER (p, ELSE_PART)) {
    make_sub (p, q, ELSE_PART);
  } else if (WHETHER (p, ELIF_SYMBOL) || WHETHER (p, ELIF_IF_PART)) {
    make_sub (p, q, ELIF_IF_PART);
  } else if (WHETHER (p, CASE_SYMBOL) || WHETHER (p, CASE_PART)) {
    make_sub (p, q, CASE_PART);
  } else if (WHETHER (p, OUT_SYMBOL) || WHETHER (p, OUT_PART)) {
    make_sub (p, q, OUT_PART);
  } else if (WHETHER (p, OUSE_SYMBOL) || WHETHER (p, OUSE_CASE_PART)) {
    make_sub (p, q, OUSE_CASE_PART);
  } else if (WHETHER (p, FOR_SYMBOL) || WHETHER (p, FOR_PART)) {
    make_sub (p, q, FOR_PART);
  } else if (WHETHER (p, FROM_SYMBOL) || WHETHER (p, FROM_PART)) {
    make_sub (p, q, FROM_PART);
  } else if (WHETHER (p, BY_SYMBOL) || WHETHER (p, BY_PART)) {
    make_sub (p, q, BY_PART);
  } else if (WHETHER (p, TO_SYMBOL) || WHETHER (p, DOWNTO_SYMBOL) || WHETHER (p, TO_PART)) {
    make_sub (p, q, TO_PART);
  } else if (WHETHER (p, WHILE_SYMBOL) || WHETHER (p, WHILE_PART)) {
    make_sub (p, q, WHILE_PART);
  } else if (WHETHER (p, UNTIL_SYMBOL) || WHETHER (p, UNTIL_PART)) {
    make_sub (p, q, UNTIL_PART);
  } else if (WHETHER (p, DO_SYMBOL) || WHETHER (p, DO_PART)) {
    make_sub (p, q, DO_PART);
  } else if (WHETHER (p, ALT_DO_SYMBOL) || WHETHER (p, ALT_DO_PART)) {
    make_sub (p, q, ALT_DO_PART);
  } else if (non_terminal_string (edit_line, expect) != NULL) {
    make_sub (p, q, expect);
  }
}

/*!
\brief heuristic aid in pinpointing errors
\param p position in syntax tree, should not be NULL
**/

static void reduce_erroneous_units (NODE_T * p)
{
/* Constructs are reduced to units in an attempt to limit spurious diagnostics. */
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
/* Some implementations allow selection from a tertiary, when there is no risk
of ambiguity. Algol68G follows RR, so some extra attention here to guide an
unsuspecting user. */
    if (whether (q, SELECTOR, -SECONDARY, 0)) {
      diagnostic_node (A_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, SECONDARY);
      f (q, NULL, NULL, UNIT, SELECTOR, WILDCARD, 0);
    }
/* Attention for identity relations that require tertiaries. */
    if (whether (q, -TERTIARY, IS_SYMBOL, TERTIARY, 0) || whether (q, TERTIARY, IS_SYMBOL, -TERTIARY, 0) || whether (q, -TERTIARY, IS_SYMBOL, -TERTIARY, 0)) {
      diagnostic_node (A_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, TERTIARY);
      f (q, NULL, NULL, UNIT, WILDCARD, IS_SYMBOL, WILDCARD, 0);
    } else if (whether (q, -TERTIARY, ISNT_SYMBOL, TERTIARY, 0) || whether (q, TERTIARY, ISNT_SYMBOL, -TERTIARY, 0) || whether (q, -TERTIARY, ISNT_SYMBOL, -TERTIARY, 0)) {
      diagnostic_node (A_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, TERTIARY);
      f (q, NULL, NULL, UNIT, WILDCARD, ISNT_SYMBOL, WILDCARD, 0);
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
\param q
\return node from where to proceed
**/

static NODE_T *skip_unit (NODE_T * q)
{
  for (; q != NULL; FORWARD (q)) {
    if (WHETHER (q, COMMA_SYMBOL)) {
      return (q);
    } else if (WHETHER (q, SEMI_SYMBOL)) {
      return (q);
    } else if (WHETHER (q, EXIT_SYMBOL)) {
      return (q);
    }
  }
  return (NULL);
}

/*!
\brief attribute of entry in symbol table
\param table
\param name
\return attribute of entry in symbol table, or 0 if not found
**/

static int find_tag_definition (SYMBOL_TABLE_T * table, char *name)
{
  if (table != NULL) {
    int ret = 0;
    TAG_T *s;
    BOOL_T found;
    found = A_FALSE;
    for (s = table->indicants; s != NULL && !found; s = NEXT (s)) {
      if (SYMBOL (NODE (s)) == name) {
        ret += INDICANT;
        found = A_TRUE;
      }
    }
    found = A_FALSE;
    for (s = table->operators; s != NULL && !found; s = NEXT (s)) {
      if (SYMBOL (NODE (s)) == name) {
        ret += OPERATOR;
        found = A_TRUE;
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
\param p position in syntax tree, should not be NULL
**/

static void elaborate_bold_tags (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, BOLD_TAG)) {
      switch (find_tag_definition (SYMBOL_TABLE (q), SYMBOL (q))) {
      case 0:
        {
          diagnostic_node (A_SYNTAX_ERROR, q, ERROR_UNDECLARED_TAG);
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
\param p position in syntax tree, should not be NULL
\return node before token that is not part of pack or declarer 
**/

static NODE_T *skip_pack_declarer (NODE_T * p)
{
/* Skip () REF [] REF FLEX [] [] ... */
  while (p != NULL && (WHETHER (p, SUB_SYMBOL) || WHETHER (p, OPEN_SYMBOL) || WHETHER (p, REF_SYMBOL) || WHETHER (p, FLEX_SYMBOL) || WHETHER (p, SHORT_SYMBOL) || WHETHER (p, LONG_SYMBOL))) {
    FORWARD (p);
  }
/* Skip STRUCT (), UNION () or PROC [()]. */
  if (p != NULL && (WHETHER (p, STRUCT_SYMBOL) || WHETHER (p, UNION_SYMBOL))) {
    return (NEXT (p));
  } else if (p != NULL && WHETHER (p, PROC_SYMBOL)) {
    return (skip_pack_declarer (NEXT (p)));
  } else {
    return (p);
  }
}

/*!
\brief search MODE A = .., B = .. and store indicants
\param p position in syntax tree, should not be NULL
**/

static void extract_indicants (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER (q, MODE_SYMBOL)) {
      BOOL_T z = A_TRUE;
      do {
        FORWARD (q);
        if (whether (q, BOLD_TAG, EQUALS_SYMBOL, 0)) {
          add_tag (SYMBOL_TABLE (p), INDICANT, q, NULL, 0);
          ATTRIBUTE (q) = DEFINING_INDICANT;
          FORWARD (q);
          q->attribute = ALT_EQUALS_SYMBOL;
          q = skip_pack_declarer (NEXT (q));
          FORWARD (q);
        } else {
          z = A_FALSE;
        }
      }
      while (z && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

#define GET_PRIORITY(q, k)\
  RESET_ERRNO;\
  (k) = atoi (SYMBOL (q));\
  if (errno != 0) {\
    diagnostic_node (A_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY, NULL);\
    (k) = MAX_PRIORITY;\
  } else if ((k) < 1 || (k) > MAX_PRIORITY) {\
    diagnostic_node (A_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY, NULL);\
    (k) = MAX_PRIORITY;\
  }

/*!
\brief search PRIO X = .., Y = .. and store priorities
\param p position in syntax tree, should not be NULL
**/

static void extract_priorities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER (q, PRIO_SYMBOL)) {
      BOOL_T z = A_TRUE;
      do {
        FORWARD (q);
/* An operator tag like ++ or && gives strange errors so we catch it here. */
        if (whether (q, OPERATOR, OPERATOR, 0)) {
          int k;
          NODE_T *y = q;
          diagnostic_node (A_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG, NULL);
          ATTRIBUTE (q) = DEFINING_OPERATOR;
/* Remove one superfluous operator, and hope it was only one.   	 */
          NEXT (q) = NEXT (NEXT (q));
          PREVIOUS (NEXT (q)) = q;
          FORWARD (q);
          q->attribute = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, INT_DENOTER, 0)
            || whether (q, OPERATOR, EQUALS_SYMBOL, INT_DENOTER, 0)
            || whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, INT_DENOTER, 0)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          q->attribute = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, INT_DENOTER, 0)
            || whether (q, OPERATOR, INT_DENOTER, 0)
            || whether (q, EQUALS_SYMBOL, INT_DENOTER, 0)) {
/* The scanner cannot separate operator and "=" sign so we do this here. */
          int len = (int) strlen (SYMBOL (q));
          if (len > 1 && SYMBOL (q)[len - 1] == '=') {
            int k;
            NODE_T *y = q;
            char *sym = (char *) get_temp_heap_space (len);
            bufcpy (sym, SYMBOL (q), len - 1);
            sym[len] = NULL_CHAR;
            SYMBOL (q) = TEXT (add_token (&top_token, sym));
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            insert_node (q, ALT_EQUALS_SYMBOL);
            q = NEXT (NEXT (q));
            GET_PRIORITY (q, k);
            ATTRIBUTE (q) = PRIORITY;
            add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k);
            FORWARD (q);
          } else {
            diagnostic_node (A_SYNTAX_ERROR, (q != NULL ? q : p), ERROR_SYNTAX_EXPECTED, EQUALS_SYMBOL, NULL);
          }
        } else {
          z = A_FALSE;
        }
      }
      while (z && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search OP [( .. ) ..] X = .., Y = .. and store operators
\param p position in syntax tree, should not be NULL
**/

static void extract_operators (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER_NOT (q, OP_SYMBOL)) {
      FORWARD (q);
    } else {
      BOOL_T z = A_TRUE;
/* Skip operator plan. */
      if (NEXT (q) != NULL && WHETHER (NEXT (q), OPEN_SYMBOL)) {
        q = skip_pack_declarer (NEXT (q));
      }
/* Sample operators. */
      if (q != NULL) {
        do {
          FORWARD (q);
/* An unacceptable operator tag like ++ or && gives strange errors so we catch it here. */
          if (whether (q, OPERATOR, OPERATOR, 0)) {
            diagnostic_node (A_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG, NULL);
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, 0);
            NEXT (q) = NEXT (NEXT (q)); /* Remove one superfluous operator, and hope it was only one. */
            PREVIOUS (NEXT (q)) = q;
            FORWARD (q);
            q->attribute = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, OPERATOR, EQUALS_SYMBOL, 0)
              || whether (q, BOLD_TAG, EQUALS_SYMBOL, 0)
              || whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, 0)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, 0);
            FORWARD (q);
            q->attribute = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (q != NULL && (WHETHER (q, OPERATOR) || WHETHER (q, BOLD_TAG)
                  || WHETHER (q, EQUALS_SYMBOL))) {
/* The scanner cannot separate operator and "=" sign so we do this here. */
            int len = (int) strlen (SYMBOL (q));
            if (len > 1 && SYMBOL (q)[len - 1] == '=') {
              char *sym = (char *) get_temp_heap_space (len);
              bufcpy (sym, SYMBOL (q), len - 1);
              sym[len] = NULL_CHAR;
              SYMBOL (q) = TEXT (add_token (&top_token, sym));
              ATTRIBUTE (q) = DEFINING_OPERATOR;
              insert_node (q, ALT_EQUALS_SYMBOL);
              add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, 0);
              FORWARD (q);
              q = skip_unit (q);
            } else {
              diagnostic_node (A_SYNTAX_ERROR, (q != NULL ? q : p), ERROR_SYNTAX_EXPECTED, EQUALS_SYMBOL, NULL);
            }
          } else {
            z = A_FALSE;
          }
        }
        while (z && q != NULL && WHETHER (q, COMMA_SYMBOL));
      }
    }
  }
}

/*!
\brief search and store labels
\param p position in syntax tree, should not be NULL
\param expect information the parser may have on what is expected
**/

static void extract_labels (NODE_T * p, int expect)
{
  NODE_T *q;
/* Only handle candidate phrases as not to search indexers! */
  if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (whether (q, IDENTIFIER, COLON_SYMBOL, 0)) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), LABEL, q, NULL, LOCAL_LABEL);
        ATTRIBUTE (q) = DEFINING_IDENTIFIER;
        z->unit = NULL;
      }
    }
  }
}

/*!
\brief search MOID x = .., y = .. and store identifiers
\param p position in syntax tree, should not be NULL
**/

static void extract_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, DECLARER, IDENTIFIER, EQUALS_SYMBOL, 0)) {
      BOOL_T z = A_TRUE;
      do {
        if (whether ((FORWARD (q)), IDENTIFIER, EQUALS_SYMBOL, 0)) {
          add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          FORWARD (q);
          q->attribute = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, 0)) {
/* Handle common error in ALGOL 68 programs. */
          diagnostic_node (A_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          (FORWARD (q))->attribute = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          z = A_FALSE;
        }
      }
      while (z && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search MOID x [:= ..], y [:= ..] and store identifiers
\param p position in syntax tree, should not be NULL
**/

static void extract_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, DECLARER, IDENTIFIER, 0)) {
      BOOL_T z = A_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, 0)) {
          if (whether (q, IDENTIFIER, EQUALS_SYMBOL, 0)) {
/* Handle common error in ALGOL 68 programs. */
            diagnostic_node (A_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
            NEXT (q)->attribute = ASSIGN_SYMBOL;
          }
          add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (q);
        } else {
          z = A_FALSE;
        }
      }
      while (z && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search PROC x = .., y = .. and stores identifiers
\param p position in syntax tree, should not be NULL
**/

static void extract_proc_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, EQUALS_SYMBOL, 0)) {
      BOOL_T z = A_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, EQUALS_SYMBOL, 0)) {
          TAG_T *t = add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          t->in_proc = A_TRUE;
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          (FORWARD (q))->attribute = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, 0)) {
/* Handle common error in ALGOL 68 programs. */
          diagnostic_node (A_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          (FORWARD (q))->attribute = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          z = A_FALSE;
        }
      }
      while (z && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search PROC x [:= ..], y [:= ..]; store identifiers
\param p position in syntax tree, should not be NULL
**/

static void extract_proc_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, 0)) {
      BOOL_T z = A_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, 0)) {
          add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (FORWARD (q));
        } else if (whether (q, IDENTIFIER, EQUALS_SYMBOL, 0)) {
/* Handle common error in ALGOL 68 programs. */
          diagnostic_node (A_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          (FORWARD (q))->attribute = ASSIGN_SYMBOL;
          q = skip_unit (q);
        } else {
          z = A_FALSE;
        }
      }
      while (z && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief schedule gathering of definitions in a phrase
\param p position in syntax tree, should not be NULL
**/

static void extract_declarations (NODE_T * p)
{
  NODE_T *q;
/* Get definitions so we know what is defined in this range. */
  extract_identities (p);
  extract_variables (p);
  extract_proc_identities (p);
  extract_proc_variables (p);
/* By now we know whether "=" is an operator or not. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = OPERATOR;
    } else if (WHETHER (q, ALT_EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = EQUALS_SYMBOL;
    }
  }
/* Get qualifiers. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (whether (q, LOC_SYMBOL, DECLARER, DEFINING_IDENTIFIER, 0)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, DECLARER, DEFINING_IDENTIFIER, 0)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, LOC_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, 0)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, 0)) {
      make_sub (q, q, QUALIFIER);
    }
  }
/* Give priorities to operators. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, OPERATOR)) {
      if (find_tag_global (SYMBOL_TABLE (q), OP_SYMBOL, SYMBOL (q))) {
        TAG_T *s = find_tag_global (SYMBOL_TABLE (q), PRIO_SYMBOL, SYMBOL (q));
        if (s != NULL) {
          PRIO (q->info) = PRIO (s);
        } else {
          PRIO (q->info) = 0;
        }
      } else {
        diagnostic_node (A_SYNTAX_ERROR, q, ERROR_UNDECLARED_TAG);
        PRIO (q->info) = 1;
      }
    }
  }
}

/* A posteriori checks of the syntax tree built by the BU parser. */

/*!
\brief count pictures
\param p position in syntax tree, should not be NULL
\param k
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
\param p position in syntax tree, should not be NULL
**/

void bottom_up_error_check (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOOLEAN_PATTERN)) {
      int k = 0;
      count_pictures (SUB (p), &k);
      if (k != 2) {
        diagnostic_node (A_SYNTAX_ERROR, p, ERROR_FORMAT_PICTURE_NUMBER, ATTRIBUTE (p), NULL);
      }
    } else {
      bottom_up_error_check (SUB (p));
    }
  }
}

/* Next part rearranges the tree after the symbol tables are finished. */

/*!
\brief transfer IDENTIFIER to JUMP where appropriate
\param p position in syntax tree, should not be NULL
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
        if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q))
            == LABEL) {
          make_sub (q, q, JUMP);
        }
      }
    }
    rearrange_goto_less_jumps (SUB (p));
  }
}

/* VICTAL CHECKER. Checks use of formal, actual and virtual declarers. */

/*!
\brief check generator
\param p position in syntax tree, should not be NULL
**/

static void victal_check_generator (NODE_T * p)
{
  if (!victal_check_declarer (NEXT (p), ACTUAL_DECLARER_MARK)) {
    diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
  }
}

/*!
\brief check formal pack
\param p position in syntax tree, should not be NULL
\param x
\param z
**/

static void victal_check_formal_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, FORMAL_DECLARERS)) {
      victal_check_formal_pack (SUB (p), x, z);
    } else if (WHETHER (p, OPEN_SYMBOL) || WHETHER (p, COMMA_SYMBOL)) {
      victal_check_formal_pack (NEXT (p), x, z);
    } else if (WHETHER (p, FORMAL_DECLARERS_LIST)) {
      victal_check_formal_pack (NEXT (p), x, z);
      victal_check_formal_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      victal_check_formal_pack (NEXT (p), x, z);
      *z &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check operator declaration
\param p position in syntax tree, should not be NULL
**/

static void victal_check_operator_dec (NODE_T * p)
{
  if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
    BOOL_T z = A_TRUE;
    victal_check_formal_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarers");
    }
    FORWARD (p);
  }
  if (!victal_check_declarer (NEXT (p), FORMAL_DECLARER_MARK)) {
    diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
  }
}

/*!
\brief check mode declaration
\param p position in syntax tree, should not be NULL
**/

static void victal_check_mode_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
      victal_check_mode_dec (NEXT (p));
    } else if (WHETHER (p, MODE_SYMBOL) || WHETHER (p, DEFINING_INDICANT)
        || WHETHER (p, EQUALS_SYMBOL) || WHETHER (p, COMMA_SYMBOL)) {
      victal_check_mode_dec (NEXT (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
    }
  }
}

/*!
\brief check variable declaration
\param p position in syntax tree, should not be NULL
**/

static void victal_check_variable_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
      victal_check_variable_dec (NEXT (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER) || WHETHER (p, ASSIGN_SYMBOL)
        || WHETHER (p, COMMA_SYMBOL)) {
      victal_check_variable_dec (NEXT (p));
    } else if (WHETHER (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
      victal_check_variable_dec (NEXT (p));
    }
  }
}

/*!
\brief check identity declaration
\param p position in syntax tree, should not be NULL
**/

static void victal_check_identity_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
      victal_check_identity_dec (NEXT (p));
    } else if (WHETHER (p, DEFINING_IDENTIFIER) || WHETHER (p, EQUALS_SYMBOL)
        || WHETHER (p, COMMA_SYMBOL)) {
      victal_check_identity_dec (NEXT (p));
    } else if (WHETHER (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
        diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      victal_check_identity_dec (NEXT (p));
    }
  }
}

/*!
\brief check routine pack
\param p position in syntax tree, should not be NULL
\param x
\param z
\return
**/

static void victal_check_routine_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, PARAMETER_PACK)) {
      victal_check_routine_pack (SUB (p), x, z);
    } else if (WHETHER (p, OPEN_SYMBOL) || WHETHER (p, COMMA_SYMBOL)) {
      victal_check_routine_pack (NEXT (p), x, z);
    } else if (WHETHER (p, PARAMETER_LIST) || WHETHER (p, PARAMETER)) {
      victal_check_routine_pack (NEXT (p), x, z);
      victal_check_routine_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      *z &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check routine text
\param p position in syntax tree, should not be NULL
**/

static void victal_check_routine_text (NODE_T * p)
{
  if (WHETHER (p, PARAMETER_PACK)) {
    BOOL_T z = A_TRUE;
    victal_check_routine_pack (p, FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarers");
    }
    FORWARD (p);
  }
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
  }
  victal_checker (NEXT (p));
}

/*!
\brief check structure pack
\param p position in syntax tree, should not be NULL
\param x
\param z
**/

static void victal_check_structure_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, STRUCTURE_PACK)) {
      victal_check_structure_pack (SUB (p), x, z);
    } else if (WHETHER (p, OPEN_SYMBOL) || WHETHER (p, COMMA_SYMBOL)) {
      victal_check_structure_pack (NEXT (p), x, z);
    } else if (WHETHER (p, STRUCTURED_FIELD_LIST)
        || WHETHER (p, STRUCTURED_FIELD)) {
      victal_check_structure_pack (NEXT (p), x, z);
      victal_check_structure_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      *z &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check union pack
\param p position in syntax tree, should not be NULL
\param x
\param z
**/

static void victal_check_union_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, UNION_PACK)) {
      victal_check_union_pack (SUB (p), x, z);
    } else if (WHETHER (p, OPEN_SYMBOL) || WHETHER (p, COMMA_SYMBOL)
        || WHETHER (p, VOID_SYMBOL)) {
      victal_check_union_pack (NEXT (p), x, z);
    } else if (WHETHER (p, UNION_DECLARER_LIST)) {
      victal_check_union_pack (NEXT (p), x, z);
      victal_check_union_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      victal_check_union_pack (NEXT (p), x, z);
      *z &= victal_check_declarer (SUB (p), FORMAL_DECLARER_MARK);
    }
  }
}

/*!
\brief check declarer
\param p position in syntax tree, should not be NULL
\param x
**/

static BOOL_T victal_check_declarer (NODE_T * p, int x)
{
  if (p == NULL) {
    return (A_FALSE);
  } else if (WHETHER (p, DECLARER)) {
    return (victal_check_declarer (SUB (p), x));
  } else if (WHETHER (p, LONGETY) || WHETHER (p, SHORTETY)) {
    return (A_TRUE);
  } else if (WHETHER (p, VOID_SYMBOL) || WHETHER (p, INDICANT)
      || WHETHER (p, STANDARD)) {
    return (A_TRUE);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (victal_check_declarer (NEXT (p), VIRTUAL_DECLARER_MARK));
  } else if (WHETHER (p, FLEX_SYMBOL)) {
    return (victal_check_declarer (NEXT (p), x));
  } else if (WHETHER (p, BOUNDS)) {
    victal_checker (SUB (p));
    if (x == FORMAL_DECLARER_MARK) {
      diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal bounds");
      victal_check_declarer (NEXT (p), x);
      return (A_TRUE);
    } else if (x == VIRTUAL_DECLARER_MARK) {
      diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "virtual bounds");
      victal_check_declarer (NEXT (p), x);
      return (A_TRUE);
    } else {
      return (victal_check_declarer (NEXT (p), x));
    }
  } else if (WHETHER (p, FORMAL_BOUNDS)) {
    victal_checker (SUB (p));
    if (x == ACTUAL_DECLARER_MARK) {
      diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual bounds");
      victal_check_declarer (NEXT (p), x);
      return (A_TRUE);
    } else {
      return (victal_check_declarer (NEXT (p), x));
    }
  } else if (WHETHER (p, STRUCT_SYMBOL)) {
    BOOL_T z = A_TRUE;
    victal_check_structure_pack (NEXT (p), x, &z);
    return (z);
  } else if (WHETHER (p, UNION_SYMBOL)) {
    BOOL_T z = A_TRUE;
    victal_check_union_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer pack");
    }
    return (A_TRUE);
  } else if (WHETHER (p, PROC_SYMBOL)) {
    if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
      BOOL_T z = A_TRUE;
      victal_check_formal_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
      if (!z) {
        diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      FORWARD (p);
    }
    if (!victal_check_declarer (NEXT (p), FORMAL_DECLARER_MARK)) {
      diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    }
    return (A_TRUE);
  } else {
    return (A_FALSE);
  }
}

/*!
\brief check cast
\param p position in syntax tree, should not be NULL
**/

static void victal_check_cast (NODE_T * p)
{
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic_node (A_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    victal_checker (NEXT (p));
  }
}

/*!
\brief driver for checking VICTALITY of declarers
\param p position in syntax tree, should not be NULL
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
\brief set nests for diagnostics
\param p position in syntax tree, should not be NULL
\param s start of enclosing nest
*/

void set_nests (NODE_T * p, NODE_T * s)
{
  for (; p != NULL; FORWARD (p)) {
    NEST (p) = s;
    if (WHETHER (p, PARTICULAR_PROGRAM)) {
      set_nests (SUB (p), p);
    } else if (WHETHER (p, CLOSED_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nests (SUB (p), p);
    } else if (WHETHER (p, COLLATERAL_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nests (SUB (p), p);
    } else if (WHETHER (p, CONDITIONAL_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nests (SUB (p), p);
    } else if (WHETHER (p, INTEGER_CASE_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nests (SUB (p), p);
    } else if (WHETHER (p, UNITED_CASE_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nests (SUB (p), p);
    } else if (WHETHER (p, LOOP_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nests (SUB (p), p);
    } else {
      set_nests (SUB (p), s);
    }
  }
}
