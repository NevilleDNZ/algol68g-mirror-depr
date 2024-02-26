//! @file parser.c
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
//! Mailloux-type Algol 68 parser driver.

// The Algol 68 grammar is a two level (Van Wijngaarden, "VW") grammar 
// that incorporates, as syntactical rules, the semantical rules in 
// other languages. Examples are correct use of symbols, modes and scope.
// 
// This code constitutes an effective "VW Algol 68 parser". A pragmatic
// approach was chosen since in the early days of Algol 68, many "ab initio" 
// implementations failed, probably because techniques to parse a language
// like Algol 68 had yet to be invented.
// 
// This is a Mailloux-type parser, in the sense that it scans a "phrase" for
// definitions needed for parsing. Algol 68 allows for tags to be used
// before they are defined, which gives freedom in top-down programming.
// 
//    B. J. Mailloux. On the implementation of Algol 68.
//    Thesis, Universiteit van Amsterdam (Mathematisch Centrum) [1968].
// 
// Technically, Mailloux's approach renders the two-level grammar LALR.
// 
// First part of the parser is the scanner. The source file is read,
// is tokenised, and if needed a refinement preprocessor elaborates a stepwise
// refined program. The result is a linear list of tokens that is input for the
// parser, that will transform the linear list into a syntax tree.
// 
// Algol68G tokenises all symbols before the bottom-up parser is invoked. 
// This means that scanning does not use information from the parser.
// The scanner does of course some rudimentary parsing. Format texts can have
// enclosed clauses in them, so we record information in a stack as to know
// what is being scanned. Also, the refinement preprocessor implements a
// (trivial) grammar.
// 
// The scanner supports two stropping regimes: "bold" (or "upper") and "quote". 
// Examples of both:
// 
//    bold stropping: BEGIN INT i = 1, j = 1; print (i + j) END
// 
//    quote stropping: 'BEGIN' 'INT' I = 1, J = 1; PRINT (I + J) 'END'
// 
// Quote stropping was used frequently in the (excusez-le-mot) punch-card age.
// Hence, bold stropping is the default. There also existed point stropping, 
// but that has not been implemented here.
// 
// Next part of the parser is a recursive-descent type to check parenthesis.
// Also a first set-up is made of symbol tables, needed by the bottom-up parser.
// Next part is the bottom-up parser, that parses without knowing modes while
// parsing and reducing. It can therefore not exchange "[]" with "()" as was
// blessed by the Revised Report. This is solved by treating CALL and SLICE as
// equivalent for the moment and letting the mode checker sort it out later.
// 
// Parsing progresses in various phases to avoid spurious diagnostics from a
// recovering parser. Every phase "tightens" the grammar more.
// An error in any phase makes the parser quit when that phase ends.
// The parser is forgiving in case of superfluous semicolons.
// 
// These are the parser phases:
// 
//  (1) Parenthesis are checked to see whether they match. Then, a top-down 
//      parser determines the basic-block structure of the program
//      so symbol tables can be set up that the bottom-up parser will consult
//      as you can define things before they are applied.
// 
//  (2) A bottom-up parser resolves the structure of the program.
// 
//  (3) After the symbol tables have been finalised, a small rearrangement of the
//      tree may be required where JUMPs have no GOTO. This leads to the
//      non-standard situation that JUMPs without GOTO can have the syntactic
//      position of a PRIMARY, SECONDARY or TERTIARY. The bottom-up parser also
//      does not check VICTAL correctness of declarers. This is done separately. 
//      Also structure of format texts is checked separately.
// 
// The parser sets up symbol tables and populates them as far as needed to parse
// the source. After the bottom-up parser terminates succesfully, the symbol tables
// are completed.
// 
//  (4) Next, modes are collected and rules for well-formedness and structural 
//      equivalence are applied. Then the symbol-table is completed now moids are 
//      all known.
// 
//  (5) Next phases are the mode checker and coercion inserter. The syntax tree is 
//      traversed to determine and check all modes, and to select operators. Then 
//      the tree is traversed again to insert coercions.
// 
//  (6) A static scope checker detects where objects are transported out of scope.
//      At run time, a dynamic scope checker will check that what the static scope 
//      checker cannot see.

#include "a68g.h"
#include "a68g-parser.h"
#include "a68g-mp.h"
#include "a68g-postulates.h"
#include "a68g-prelude.h"

//! @brief First initialisations.

void init_before_tokeniser (void)
{
// Heap management set-up.
  errno = 0;
  init_heap ();
  A68 (top_keyword) = NO_KEYWORD;
  A68 (top_token) = NO_TOKEN;
  TOP_NODE (&A68_JOB) = NO_NODE;
  TOP_MOID (&A68_JOB) = NO_MOID;
  TOP_LINE (&A68_JOB) = NO_LINE;
  STANDENV_MOID (&A68_JOB) = NO_MOID;
  set_up_tables ();
// Various initialisations.
  ERROR_COUNT (&A68_JOB) = WARNING_COUNT (&A68_JOB) = 0;
  ABEND (errno != 0, ERROR_ALLOCATION, __func__);
  errno = 0;
}

void init_parser (void)
{
  A68_PARSER (stop_scanner) = A68_FALSE;
  A68_PARSER (read_error) = A68_FALSE;
  A68_PARSER (no_preprocessing) = A68_FALSE;
}

//! @brief Is_ref_refety_flex.

BOOL_T is_ref_refety_flex (MOID_T * m)
{
  if (IS_REF_FLEX (m)) {
    return A68_TRUE;
  } else if (IS_REF (m)) {
    return is_ref_refety_flex (SUB (m));
  } else {
    return A68_FALSE;
  }
}

//! @brief Count number of operands in operator parameter list.

int count_operands (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, DECLARER)) {
      return count_operands (NEXT (p));
    } else if (IS (p, COMMA_SYMBOL)) {
      return 1 + count_operands (NEXT (p));
    } else {
      return count_operands (NEXT (p)) + count_operands (SUB (p));
    }
  } else {
    return 0;
  }
}

//! @brief Count formal bounds in declarer in tree.

int count_formal_bounds (NODE_T * p)
{
  if (p == NO_NODE) {
    return 0;
  } else {
    if (IS (p, COMMA_SYMBOL)) {
      return 1;
    } else {
      return count_formal_bounds (NEXT (p)) + count_formal_bounds (SUB (p));
    }
  }
}

//! @brief Count pictures.

void count_pictures (NODE_T * p, int *k)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PICTURE)) {
      (*k)++;
    }
    count_pictures (SUB (p), k);
  }
}

//! @brief Whether token cannot follow semicolon or EXIT.

BOOL_T is_semicolon_less (NODE_T * p)
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
      return A68_TRUE;
    }
  default:
    {
      return A68_FALSE;
    }
  }
}

//! @brief Whether formal bounds.

BOOL_T is_formal_bounds (NODE_T * p)
{
  if (p == NO_NODE) {
    return A68_TRUE;
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
        return (BOOL_T) (is_formal_bounds (SUB (p)) && is_formal_bounds (NEXT (p)));
      }
    default:
      {
        return A68_FALSE;
      }
    }
  }
}

//! @brief Whether token terminates a unit.

BOOL_T is_unit_terminator (NODE_T * p)
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
      return A68_TRUE;
    }
  }
  return A68_FALSE;
}

//! @brief Whether token is a unit-terminator in a loop clause.

BOOL_T is_loop_keyword (NODE_T * p)
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
      return A68_TRUE;
    }
  }
  return A68_FALSE;
}

//! @brief Get good attribute.

int get_good_attribute (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case UNIT:
  case TERTIARY:
  case SECONDARY:
  case PRIMARY:
    {
      return get_good_attribute (SUB (p));
    }
  default:
    {
      return ATTRIBUTE (p);
    }
  }
}

//! @brief Preferably don't put intelligible diagnostic here.

BOOL_T dont_mark_here (NODE_T * p)
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
  case SERIAL_CLAUSE:
  case ENQUIRY_CLAUSE:
  case INITIALISER_SERIES:
  case DECLARATION_LIST:
    {
      return A68_TRUE;
    }
  }
  return A68_FALSE;
}

void a68_parser (void)
{
// Tokeniser.
  int renum;
  FILE_SOURCE_OPENED (&A68_JOB) = A68_TRUE;
  announce_phase ("initialiser");
  A68_PARSER (error_tag) = (TAG_T *) new_tag ();
  init_parser ();
  if (ERROR_COUNT (&A68_JOB) == 0) {
    int frame_stack_size_2 = A68 (frame_stack_size);
    int expr_stack_size_2 = A68 (expr_stack_size);
    int heap_size_2 = A68 (heap_size);
    int handle_pool_size_2 = A68 (handle_pool_size);
    BOOL_T ok;
    announce_phase ("tokeniser");
    ok = lexical_analyser ();
    if (!ok || errno != 0) {
      diagnostics_to_terminal (TOP_LINE (&A68_JOB), A68_ALL_DIAGNOSTICS);
      return;
    }
// Maybe the program asks for more memory through a PRAGMAT. We restart.
    if (frame_stack_size_2 != A68 (frame_stack_size) || expr_stack_size_2 != A68 (expr_stack_size) || heap_size_2 != A68 (heap_size) || handle_pool_size_2 != A68 (handle_pool_size)) {
      announce_phase ("tokeniser");
      free_syntax_tree (TOP_NODE (&A68_JOB));
      discard_heap ();
      init_before_tokeniser ();
      SOURCE_SCAN (&A68_JOB)++;
      ok = lexical_analyser ();
      verbosity ();
    }
    if (!ok || errno != 0) {
      diagnostics_to_terminal (TOP_LINE (&A68_JOB), A68_ALL_DIAGNOSTICS);
      return;
    }
    ASSERT (close (FILE_SOURCE_FD (&A68_JOB)) == 0);
    FILE_SOURCE_OPENED (&A68_JOB) = A68_FALSE;
    prune_echoes (OPTION_LIST (&A68_JOB));
    TREE_LISTING_SAFE (&A68_JOB) = A68_TRUE;
    renum = 0;
    renumber_nodes (TOP_NODE (&A68_JOB), &renum);
  }
// Now the default precision of LONG LONG modes is fixed.
  if (long_mp_digits () == 0) {
    set_long_mp_digits (LONG_LONG_MP_DIGITS);
  }
// Final initialisations.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    if (OPTION_REGRESSION_TEST (&A68_JOB)) {
      bufcpy (A68 (a68_cmd_name), "a68g", BUFFER_SIZE);
      io_close_tty_line ();
      WRITE (STDERR_FILENO, "[");
      WRITE (STDERR_FILENO, FILE_INITIAL_NAME (&A68_JOB));
      WRITE (STDERR_FILENO, "]\n");
    }
    A68_STANDENV = NO_TABLE;
    init_postulates ();
    A68 (mode_count) = 0;
    make_special_mode (&M_HIP, A68 (mode_count)++);
    make_special_mode (&M_UNDEFINED, A68 (mode_count)++);
    make_special_mode (&M_ERROR, A68 (mode_count)++);
    make_special_mode (&M_VACUUM, A68 (mode_count)++);
    make_special_mode (&M_C_STRING, A68 (mode_count)++);
    make_special_mode (&M_COLLITEM, A68 (mode_count)++);
    make_special_mode (&M_SOUND_DATA, A68 (mode_count)++);
  }
// Refinement preprocessor.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("preprocessor");
    get_refinements ();
    if (ERROR_COUNT (&A68_JOB) == 0) {
      put_refinements ();
    }
    renum = 0;
    renumber_nodes (TOP_NODE (&A68_JOB), &renum);
    verbosity ();
  }
// Top-down parser.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("parser phase 1");
    check_parenthesis (TOP_NODE (&A68_JOB));
    if (ERROR_COUNT (&A68_JOB) == 0) {
      if (OPTION_BRACKETS (&A68_JOB)) {
        substitute_brackets (TOP_NODE (&A68_JOB));
      }
      A68 (symbol_table_count) = 0;
      A68_STANDENV = new_symbol_table (NO_TABLE);
      LEVEL (A68_STANDENV) = 0;
      top_down_parser (TOP_NODE (&A68_JOB));
    }
    renum = 0;
    renumber_nodes (TOP_NODE (&A68_JOB), &renum);
    verbosity ();
  }
// Standard environment builder.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("standard environ builder");
    TABLE (TOP_NODE (&A68_JOB)) = new_symbol_table (A68_STANDENV);
    make_standard_environ ();
    STANDENV_MOID (&A68_JOB) = TOP_MOID (&A68_JOB);
    verbosity ();
  }
// Bottom-up parser.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("parser phase 2");
    preliminary_symbol_table_setup (TOP_NODE (&A68_JOB));
    bottom_up_parser (TOP_NODE (&A68_JOB));
    renum = 0;
    renumber_nodes (TOP_NODE (&A68_JOB), &renum);
    verbosity ();
  }
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("parser phase 3");
    bottom_up_error_check (TOP_NODE (&A68_JOB));
    victal_checker (TOP_NODE (&A68_JOB));
    if (ERROR_COUNT (&A68_JOB) == 0) {
      finalise_symbol_table_setup (TOP_NODE (&A68_JOB), 2);
      NEST (TABLE (TOP_NODE (&A68_JOB))) = A68 (symbol_table_count) = 3;
      reset_symbol_table_nest_count (TOP_NODE (&A68_JOB));
      fill_symbol_table_outer (TOP_NODE (&A68_JOB), TABLE (TOP_NODE (&A68_JOB)));
      set_nest (TOP_NODE (&A68_JOB), NO_NODE);
      set_proc_level (TOP_NODE (&A68_JOB), 1);
    }
    renum = 0;
    renumber_nodes (TOP_NODE (&A68_JOB), &renum);
    verbosity ();
  }
// Mode table builder.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("mode table builder");
    make_moid_list (&A68_JOB);
    verbosity ();
  }
  CROSS_REFERENCE_SAFE (&A68_JOB) = A68_TRUE;
// Symbol table builder.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("symbol table builder");
    collect_taxes (TOP_NODE (&A68_JOB));
    verbosity ();
  }
// Post parser.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("parser phase 4");
    rearrange_goto_less_jumps (TOP_NODE (&A68_JOB));
    verbosity ();
  }
// Mode checker.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("mode checker");
    mode_checker (TOP_NODE (&A68_JOB));
    verbosity ();
  }
// Coercion inserter.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("coercion enforcer");
    coercion_inserter (TOP_NODE (&A68_JOB));
    widen_denotation (TOP_NODE (&A68_JOB));
    get_max_simplout_size (TOP_NODE (&A68_JOB));
    set_moid_sizes (TOP_MOID (&A68_JOB));
    assign_offsets_table (A68_STANDENV);
    assign_offsets (TOP_NODE (&A68_JOB));
    assign_offsets_packs (TOP_MOID (&A68_JOB));
    renum = 0;
    renumber_nodes (TOP_NODE (&A68_JOB), &renum);
    verbosity ();
  }
// Application checker.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("application checker");
    mark_moids (TOP_NODE (&A68_JOB));
    mark_auxilliary (TOP_NODE (&A68_JOB));
    jumps_from_procs (TOP_NODE (&A68_JOB));
    warn_for_unused_tags (TOP_NODE (&A68_JOB));
    verbosity ();
  }
// Scope checker.
  if (ERROR_COUNT (&A68_JOB) == 0) {
    announce_phase ("static scope checker");
    tie_label_to_serial (TOP_NODE (&A68_JOB));
    tie_label_to_unit (TOP_NODE (&A68_JOB));
    bind_routine_tags_to_tree (TOP_NODE (&A68_JOB));
    bind_format_tags_to_tree (TOP_NODE (&A68_JOB));
    scope_checker (TOP_NODE (&A68_JOB));
    verbosity ();
  }
}

//! @brief Renumber nodes.

void renumber_nodes (NODE_T * p, int *n)
{
  for (; p != NO_NODE; FORWARD (p)) {
    NUMBER (p) = (*n)++;
    renumber_nodes (SUB (p), n);
  }
}

//! @brief Register nodes.

void register_nodes (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    A68 (node_register)[NUMBER (p)] = p;
    register_nodes (SUB (p));
  }
}

//! @brief New_node_info.

NODE_INFO_T *new_node_info (void)
{
  NODE_INFO_T *z = (NODE_INFO_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (NODE_INFO_T));
  A68 (new_node_infos)++;
  PROCEDURE_LEVEL (z) = 0;
  CHAR_IN_LINE (z) = NO_TEXT;
  SYMBOL (z) = NO_TEXT;
  PRAGMENT (z) = NO_TEXT;
  PRAGMENT_TYPE (z) = 0;
  LINE (z) = NO_LINE;
  return z;
}

//! @brief New_genie_info.

GINFO_T *new_genie_info (void)
{
  GINFO_T *z = (GINFO_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (GINFO_T));
  A68 (new_genie_infos)++;
  UNIT (&PROP (z)) = NO_PPROC;
  SOURCE (&PROP (z)) = NO_NODE;
  PARTIAL_PROC (z) = NO_MOID;
  PARTIAL_LOCALE (z) = NO_MOID;
  IS_COERCION (z) = A68_FALSE;
  IS_NEW_LEXICAL_LEVEL (z) = A68_FALSE;
  NEED_DNS (z) = A68_FALSE;
  PARENT (z) = NO_NODE;
  OFFSET (z) = NO_BYTE;
  CONSTANT (z) = NO_CONSTANT;
  LEVEL (z) = 0;
  ARGSIZE (z) = 0;
  SIZE (z) = 0;
  COMPILE_NAME (z) = NO_TEXT;
  COMPILE_NODE (z) = 0;
  return z;
}

//! @brief New_node.

NODE_T *new_node (void)
{
  NODE_T *z = (NODE_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (NODE_T));
  A68 (new_nodes)++;
  STATUS (z) = NULL_MASK;
  CODEX (z) = NULL_MASK;
  TABLE (z) = NO_TABLE;
  INFO (z) = NO_NINFO;
  GINFO (z) = NO_GINFO;
  ATTRIBUTE (z) = 0;
  ANNOTATION (z) = 0;
  MOID (z) = NO_MOID;
  NEXT (z) = NO_NODE;
  PREVIOUS (z) = NO_NODE;
  SUB (z) = NO_NODE;
  NEST (z) = NO_NODE;
  NON_LOCAL (z) = NO_TABLE;
  TAX (z) = NO_TAG;
  SEQUENCE (z) = NO_NODE;
  PACK (z) = NO_PACK;
  return z;
}

//! @brief New_symbol_table.

TABLE_T *new_symbol_table (TABLE_T * p)
{
  TABLE_T *z = (TABLE_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (TABLE_T));
  NUM (z) = A68 (symbol_table_count);
  LEVEL (z) = A68 (symbol_table_count)++;
  NEST (z) = A68 (symbol_table_count);
  ATTRIBUTE (z) = 0;
  AP_INCREMENT (z) = 0;
  INITIALISE_FRAME (z) = A68_TRUE;
  PROC_OPS (z) = A68_TRUE;
  INITIALISE_ANON (z) = A68_TRUE;
  PREVIOUS (z) = p;
  OUTER (z) = NO_TABLE;
  IDENTIFIERS (z) = NO_TAG;
  OPERATORS (z) = NO_TAG;
  PRIO (z) = NO_TAG;
  INDICANTS (z) = NO_TAG;
  LABELS (z) = NO_TAG;
  ANONYMOUS (z) = NO_TAG;
  JUMP_TO (z) = NO_NODE;
  SEQUENCE (z) = NO_NODE;
  return z;
}

//! @brief New_moid.

MOID_T *new_moid (void)
{
  MOID_T *z = (MOID_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (MOID_T));
  A68 (new_modes)++;
  ATTRIBUTE (z) = 0;
  NUMBER (z) = 0;
  DIM (z) = 0;
  USE (z) = A68_FALSE;
  HAS_ROWS (z) = A68_FALSE;
  SIZE (z) = 0;
  DIGITS (z) = 0;
  SIZEC (z) = 0;
  DIGITSC (z) = 0;
  PORTABLE (z) = A68_TRUE;
  DERIVATE (z) = A68_FALSE;
  NODE (z) = NO_NODE;
  PACK (z) = NO_PACK;
  SUB (z) = NO_MOID;
  EQUIVALENT_MODE (z) = NO_MOID;
  SLICE (z) = NO_MOID;
  TRIM (z) = NO_MOID;
  DEFLEXED (z) = NO_MOID;
  NAME (z) = NO_MOID;
  MULTIPLE_MODE (z) = NO_MOID;
  NEXT (z) = NO_MOID;
  return z;
}

//! @brief New_pack.

PACK_T *new_pack (void)
{
  PACK_T *z = (PACK_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (PACK_T));
  MOID (z) = NO_MOID;
  TEXT (z) = NO_TEXT;
  NODE (z) = NO_NODE;
  NEXT (z) = NO_PACK;
  PREVIOUS (z) = NO_PACK;
  SIZE (z) = 0;
  OFFSET (z) = 0;
  return z;
}

//! @brief New_tag.

TAG_T *new_tag (void)
{
  TAG_T *z = (TAG_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (TAG_T));
  STATUS (z) = NULL_MASK;
  CODEX (z) = NULL_MASK;
  TAG_TABLE (z) = NO_TABLE;
  MOID (z) = NO_MOID;
  NODE (z) = NO_NODE;
  UNIT (z) = NO_NODE;
  VALUE (z) = NO_TEXT;
  A68_STANDENV_PROC (z) = 0;
  PROCEDURE (z) = NO_GPROC;
  SCOPE (z) = PRIMAL_SCOPE;
  SCOPE_ASSIGNED (z) = A68_FALSE;
  PRIO (z) = 0;
  USE (z) = A68_FALSE;
  IN_PROC (z) = A68_FALSE;
  HEAP (z) = A68_FALSE;
  SIZE (z) = 0;
  OFFSET (z) = 0;
  YOUNGEST_ENVIRON (z) = PRIMAL_SCOPE;
  LOC_ASSIGNED (z) = A68_FALSE;
  NEXT (z) = NO_TAG;
  BODY (z) = NO_TAG;
  PORTABLE (z) = A68_TRUE;
  NUMBER (z) = ++A68_PARSER (tag_number);
  return z;
}

//! @brief Make special, internal mode.

void make_special_mode (MOID_T ** n, int m)
{
  (*n) = new_moid ();
  ATTRIBUTE (*n) = 0;
  NUMBER (*n) = m;
  PACK (*n) = NO_PACK;
  SUB (*n) = NO_MOID;
  EQUIVALENT (*n) = NO_MOID;
  DEFLEXED (*n) = NO_MOID;
  NAME (*n) = NO_MOID;
  SLICE (*n) = NO_MOID;
  TRIM (*n) = NO_MOID;
  ROWED (*n) = NO_MOID;
}

//! @brief Whether x matches c; case insensitive.

BOOL_T match_string (char *x, char *c, char alt)
{
  BOOL_T match = A68_TRUE;
  while ((IS_UPPER (c[0]) || IS_DIGIT (c[0]) || c[0] == '-') && match) {
    match = (BOOL_T) (match & (TO_LOWER (x[0]) == TO_LOWER ((c++)[0])));
    if (!(x[0] == NULL_CHAR || x[0] == alt)) {
      x++;
    }
  }
  while (x[0] != NULL_CHAR && x[0] != alt && c[0] != NULL_CHAR && match) {
    match = (BOOL_T) (match & (TO_LOWER ((x++)[0]) == TO_LOWER ((c++)[0])));
  }
  return (BOOL_T) (match ? (x[0] == NULL_CHAR || x[0] == alt) : A68_FALSE);
}

//! @brief Whether attributes match in subsequent nodes.

BOOL_T whether (NODE_T * p, ...)
{
  va_list vl;
  int a;
  va_start (vl, p);
  while ((a = va_arg (vl, int)) != STOP)
  {
    if (p != NO_NODE && a == WILDCARD) {
      FORWARD (p);
    } else if (p != NO_NODE && (a == KEYWORD)) {
      if (find_keyword_from_attribute (A68 (top_keyword), ATTRIBUTE (p)) != NO_KEYWORD) {
        FORWARD (p);
      } else {
        va_end (vl);
        return A68_FALSE;
      }
    } else if (p != NO_NODE && (a >= 0 ? a == ATTRIBUTE (p) : (-a) != ATTRIBUTE (p))) {
      FORWARD (p);
    } else {
      va_end (vl);
      return A68_FALSE;
    }
  }
  va_end (vl);
  return A68_TRUE;
}

//! @brief Whether one of a series of attributes matches a node.

BOOL_T is_one_of (NODE_T * p, ...)
{
  if (p != NO_NODE) {
    va_list vl;
    int a;
    BOOL_T match = A68_FALSE;
    va_start (vl, p);
    while ((a = va_arg (vl, int)) != STOP)
    {
      match = (BOOL_T) (match | (BOOL_T) (IS (p, a)));
    }
    va_end (vl);
    return match;
  } else {
    return A68_FALSE;
  }
}

//! @brief Isolate nodes p-q making p a branch to p-q.

void make_sub (NODE_T * p, NODE_T * q, int t)
{
  NODE_T *z = new_node ();
  ABEND (p == NO_NODE || q == NO_NODE, ERROR_INTERNAL_CONSISTENCY, __func__);
  *z = *p;
  if (GINFO (p) != NO_GINFO) {
    GINFO (z) = new_genie_info ();
  }
  PREVIOUS (z) = NO_NODE;
  if (p == q) {
    NEXT (z) = NO_NODE;
  } else {
    if (NEXT (p) != NO_NODE) {
      PREVIOUS (NEXT (p)) = z;
    }
    NEXT (p) = NEXT (q);
    if (NEXT (p) != NO_NODE) {
      PREVIOUS (NEXT (p)) = p;
    }
    NEXT (q) = NO_NODE;
  }
  SUB (p) = z;
  ATTRIBUTE (p) = t;
}

//! @brief Find symbol table at level 'i'.

TABLE_T *find_level (NODE_T * n, int i)
{
  if (n == NO_NODE) {
    return NO_TABLE;
  } else {
    TABLE_T *s = TABLE (n);
    if (s != NO_TABLE && LEVEL (s) == i) {
      return s;
    } else if ((s = find_level (SUB (n), i)) != NO_TABLE) {
      return s;
    } else if ((s = find_level (NEXT (n), i)) != NO_TABLE) {
      return s;
    } else {
      return NO_TABLE;
    }
  }
}

//! @brief Whether 'p' is top of lexical level.

BOOL_T is_new_lexical_level (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ALT_DO_PART:
  case BRIEF_ELIF_PART:
  case BRIEF_OUSE_PART:
  case BRIEF_CONFORMITY_OUSE_PART:
  case CHOICE:
  case CLOSED_CLAUSE:
  case CONDITIONAL_CLAUSE:
  case DO_PART:
  case ELIF_PART:
  case ELSE_PART:
  case FORMAT_TEXT:
  case CASE_CLAUSE:
  case CASE_CHOICE_CLAUSE:
  case CASE_IN_PART:
  case CASE_OUSE_PART:
  case OUT_PART:
  case ROUTINE_TEXT:
  case SPECIFIED_UNIT:
  case THEN_PART:
  case UNTIL_PART:
  case CONFORMITY_CLAUSE:
  case CONFORMITY_CHOICE:
  case CONFORMITY_IN_PART:
  case CONFORMITY_OUSE_PART:
  case WHILE_PART:
    {
      return A68_TRUE;
    }
  default:
    {
      return A68_FALSE;
    }
  }
}

//! @brief Some_node.

NODE_T *some_node (char *t)
{
  NODE_T *z = new_node ();
  INFO (z) = new_node_info ();
  GINFO (z) = new_genie_info ();
  NSYMBOL (z) = t;
  return z;
}
