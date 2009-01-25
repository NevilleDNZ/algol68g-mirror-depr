/*!
\file names.c
\brief translates int attributes to string names
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


#include "algol68g.h"
#include "genie.h"
#include "gsl.h"
#include "transput.h"

static char *attribute_names[WILDCARD + 1] = {
  NULL,
  "A68_PATTERN",
  "ACCO_SYMBOL",
  "ALT_DO_PART",
  "ALT_DO_SYMBOL",
  "ALT_EQUALS_SYMBOL",
  "ALT_FORMAL_BOUNDS_LIST",
  "ANDF_SYMBOL",
  "AND_FUNCTION",
  "ARGUMENT",
  "ARGUMENT_LIST",
  "ASSERTION",
  "ASSERT_SYMBOL",
  "ASSIGNATION",
  "ASSIGN_SYMBOL",
  "ASSIGN_TO_SYMBOL",
  "AT_SYMBOL",
  "BEGIN_SYMBOL",
  "BITS_DENOTATION",
  "BITS_PATTERN",
  "BITS_SYMBOL",
  "BOLD_COMMENT_SYMBOL",
  "BOLD_PRAGMAT_SYMBOL",
  "BOLD_TAG",
  "BOOLEAN_PATTERN",
  "BOOL_SYMBOL",
  "BOUND",
  "BOUNDS",
  "BOUNDS_LIST",
  "BRIEF_ELIF_IF_PART",
  "BRIEF_INTEGER_OUSE_PART",
  "BRIEF_OPERATOR_DECLARATION",
  "BRIEF_UNITED_OUSE_PART",
  "BUS_SYMBOL",
  "BY_PART",
  "BY_SYMBOL",
  "BYTES_SYMBOL",
  "CALL",
  "CASE_PART",
  "CASE_SYMBOL",
  "CAST",
  "CHANNEL_SYMBOL",
  "CHAR_DENOTATION",
  "CHAR_SYMBOL",
  "CHOICE",
  "CHOICE_PATTERN",
  "CLOSED_CLAUSE",
  "CLOSE_SYMBOL",
  "CODE_CLAUSE",
  "CODE_SYMBOL",
  "COLLATERAL_CLAUSE",
  "COLLECTION",
  "COLON_SYMBOL",
  "COLUMN_FUNCTION",
  "COLUMN_SYMBOL",
  "COMMA_SYMBOL",
  "COMPLEX_PATTERN",
  "COMPLEX_SYMBOL",
  "COMPL_SYMBOL",
  "CONDITIONAL_CLAUSE",
  "DECLARATION_LIST",
  "DECLARER",
  "DEFINING_IDENTIFIER",
  "DEFINING_INDICANT",
  "DEFINING_OPERATOR",
  "DENOTATION",
  "DEPROCEDURING",
  "DEREFERENCING",
  "DIAGONAL_FUNCTION",
  "DIAGONAL_SYMBOL",
  "DO_PART",
  "DO_SYMBOL",
  "DOTDOT_SYMBOL",
  "DOWNTO_SYMBOL",
  "DYNAMIC_REPLICATOR",
  "EDOC_SYMBOL",
  "ELIF_IF_PART",
  "ELIF_PART",
  "ELIF_SYMBOL",
  "ELSE_BAR_SYMBOL",
  "ELSE_OPEN_PART",
  "ELSE_PART",
  "ELSE_SYMBOL",
  "EMPTY_SYMBOL",
  "ENCLOSED_CLAUSE",
  "END_SYMBOL",
  "ENQUIRY_CLAUSE",
  "ENVIRON_NAME",
  "ENVIRON_SYMBOL",
  "EQUALS_SYMBOL",
  "ERROR",
  "ESAC_SYMBOL",
  "EXIT_SYMBOL",
  "EXPONENT_FRAME",
  "FALSE_SYMBOL",
  "FIELD_IDENTIFIER",
  "FILE_SYMBOL",
  "FI_SYMBOL",
  "FIXED_C_PATTERN",
  "FLEX_SYMBOL",
  "FLOAT_C_PATTERN",
  "FORMAL_BOUNDS",
  "FORMAL_BOUNDS_LIST",
  "FORMAL_DECLARERS",
  "FORMAL_DECLARERS_LIST",
  "FORMAT_A_FRAME",
  "FORMAT_DELIMITER_SYMBOL",
  "FORMAT_D_FRAME",
  "FORMAT_E_FRAME",
  "FORMAT_I_FRAME",
  "FORMAT_ITEM_A",
  "FORMAT_ITEM_B",
  "FORMAT_ITEM_C",
  "FORMAT_CLOSE_SYMBOL",
  "FORMAT_ITEM_D",
  "FORMAT_ITEM_E",
  "FORMAT_ITEM_ESCAPE",
  "FORMAT_ITEM_F",
  "FORMAT_ITEM_G",
  "FORMAT_ITEM_H",
  "FORMAT_ITEM_I",
  "FORMAT_ITEM_J",
  "FORMAT_ITEM_K",
  "FORMAT_ITEM_L",
  "FORMAT_ITEM_M",
  "FORMAT_ITEM_MINUS",
  "FORMAT_ITEM_N",
  "FORMAT_ITEM_O",
  "FORMAT_OPEN_SYMBOL",
  "FORMAT_ITEM_P",
  "FORMAT_ITEM_PLUS",
  "FORMAT_ITEM_POINT",
  "FORMAT_ITEM_Q",
  "FORMAT_ITEM_R",
  "FORMAT_ITEM_S",
  "FORMAT_ITEM_T",
  "FORMAT_ITEM_U",
  "FORMAT_ITEM_V",
  "FORMAT_ITEM_W",
  "FORMAT_ITEM_X",
  "FORMAT_ITEM_Y",
  "FORMAT_ITEM_Z",
  "FORMAT_PATTERN",
  "FORMAT_POINT_FRAME",
  "FORMAT_SYMBOL",
  "FORMAT_TEXT",
  "FORMAT_Z_FRAME",
  "FORMULA",
  "FOR_PART",
  "FOR_SYMBOL",
  "FROM_PART",
  "FROM_SYMBOL",
  "GENERAL_PATTERN",
  "GENERATOR",
  "GENERIC_ARGUMENT",
  "GENERIC_ARGUMENT_LIST",
  "GO_SYMBOL",
  "GOTO_SYMBOL",
  "HEAP_SYMBOL",
  "IDENTIFIER",
  "IDENTITY_DECLARATION",
  "IDENTITY_RELATION",
  "IF_PART",
  "IF_SYMBOL",
  "INDICANT",
  "INITIALISER_SERIES",
  "INSERTION",
  "IN_SYMBOL",
  "INT_DENOTATION",
  "INTEGER_CASE_CLAUSE",
  "INTEGER_CHOICE_CLAUSE",
  "INTEGER_IN_PART",
  "INTEGER_OUT_PART",
  "INTEGRAL_C_PATTERN",
  "INTEGRAL_MOULD",
  "INTEGRAL_PATTERN",
  "INT_SYMBOL",
  "IN_TYPE_MODE",
  "ISNT_SYMBOL",
  "IS_SYMBOL",
  "JUMP",
  "KEYWORD",
  "LABEL",
  "LABELED_UNIT",
  "LABEL_IDENTIFIER",
  "LABEL_SEQUENCE",
  "LITERAL",
  "LOC_SYMBOL",
  "LONGETY",
  "LONG_SYMBOL",
  "LOOP_CLAUSE",
  "MAIN_SYMBOL",
  "MODE_DECLARATION",
  "MODE_SYMBOL",
  "MONADIC_FORMULA",
  "MONAD_SEQUENCE",
  "NIHIL",
  "NIL_SYMBOL",
  "OCCA_SYMBOL",
  "OD_SYMBOL",
  "OF_SYMBOL",
  "OPEN_PART",
  "OPEN_SYMBOL",
  "OPERATOR",
  "OPERATOR_DECLARATION",
  "OPERATOR_PLAN",
  "OP_SYMBOL",
  "ORF_SYMBOL",
  "OR_FUNCTION",
  "OUSE_CASE_PART",
  "OUSE_SYMBOL",
  "OUT_PART",
  "OUT_SYMBOL",
  "OUT_TYPE_MODE",
  "PARALLEL_CLAUSE",
  "PARAMETER",
  "PARAMETER_LIST",
  "PARAMETER_PACK",
  "PAR_SYMBOL",
  "PARTICULAR_PROGRAM",
  "PICTURE",
  "PICTURE_LIST",
  "PIPE_SYMBOL",
  "POINT_SYMBOL",
  "PRIMARY",
  "PRIORITY",
  "PRIORITY_DECLARATION",
  "PRIO_SYMBOL",
  "PROCEDURE_DECLARATION",
  "PROCEDURE_VARIABLE_DECLARATION",
  "PROCEDURING",
  "PROC_SYMBOL",
  "QUALIFIER",
  "RADIX_FRAME",
  "REAL_DENOTATION",
  "REAL_PATTERN",
  "REAL_SYMBOL",
  "REF_SYMBOL",
  "REPLICATOR",
  "ROUTINE_TEXT",
  "ROUTINE_UNIT",
  "ROW_ASSIGNATION",
  "ROW_ASSIGN_SYMBOL",
  "ROW_CHAR_DENOTATION",
  "ROW_FUNCTION",
  "ROWING",
  "ROWS_SYMBOL",
  "ROW_SYMBOL",
  "SECONDARY",
  "SELECTION",
  "SELECTOR",
  "SEMA_SYMBOL",
  "SEMI_SYMBOL",
  "SERIAL_CLAUSE",
  "SERIES_MODE",
  "SHORTETY",
  "SHORT_SYMBOL",
  "SIGN_MOULD",
  "SKIP",
  "SKIP_SYMBOL",
  "SLICE_OR_CALL",
  "SOME_CLAUSE",
  "SOUND_SYMBOL",
  "SPECIFIED_UNIT",
  "SPECIFIED_UNIT_LIST",
  "SPECIFIED_UNIT_UNIT",
  "SPECIFIER",
  "STANDARD",
  "STATIC_REPLICATOR",
  "STOWED_MODE",
  "STRING_C_PATTERN",
  "STRING_PATTERN",
  "STRING_SYMBOL",
  "STRUCT_SYMBOL",
  "STRUCTURED_FIELD",
  "STRUCTURED_FIELD_LIST",
  "STRUCTURE_PACK",
  "STYLE_I_COMMENT_SYMBOL",
  "STYLE_II_COMMENT_SYMBOL",
  "STYLE_I_PRAGMAT_SYMBOL",
  "SUB_SYMBOL",
  "SUB_UNIT",
  "TERTIARY",
  "THEN_BAR_SYMBOL",
  "THEN_PART",
  "THEN_SYMBOL",
  "TO_PART",
  "TO_SYMBOL",
  "TRANSPOSE_FUNCTION",
  "TRANSPOSE_SYMBOL",
  "TRIMMER",
  "TRUE_SYMBOL",
  "UNION_DECLARER_LIST",
  "UNION_PACK",
  "UNION_SYMBOL",
  "UNIT",
  "UNITED_CASE_CLAUSE",
  "UNITED_CHOICE",
  "UNITED_IN_PART",
  "UNITED_OUSE_PART",
  "UNITING",
  "UNIT_LIST",
  "UNIT_SERIES",
  "UNTIL_PART",
  "UNTIL_SYMBOL",
  "VARIABLE_DECLARATION",
  "VOIDING",
  "VOID_SYMBOL",
  "WHILE_PART",
  "WHILE_SYMBOL",
  "WIDENING",
  "WILDCARD"
};

/*!
\brief non_terminal_string
\param buf text buffer
\param att attribute
\return buf, containing name of non terminal string, or NULL
**/

char *non_terminal_string (char *buf, int att)
{
  if (att > 0 && att < WILDCARD) {
    if (attribute_names[att] != NULL) {
      char *q = buf;
      bufcpy (q, attribute_names[att], BUFFER_SIZE);
      while (q[0] != NULL_CHAR) {
        if (q[0] == '_') {
          q[0] = '-';
        } else {
          q[0] = TO_LOWER (q[0]);
        }
        q++;
      }
      return (buf);
    } else {
      return (NULL);
    }
  } else {
    return (NULL);
  }
}

/*!
\brief standard_environ_proc_name
\param f routine that implements a standard environ item
\return name of that what "f" implements
**/

char *standard_environ_proc_name (GENIE_PROCEDURE f)
{
  TAG_T *i = stand_env->identifiers;
  for (; i != NULL; FORWARD (i)) {
    if (i->procedure == f) {
      return (SYMBOL (NODE (i)));
    }
  }
  return (NULL);
}

/*!
\brief propagator_name
\param p propagator procedure
\return function name of "p"
**/

char *propagator_name (PROPAGATOR_PROCEDURE * p)
{
  if (p == genie_and_function) {
    return ("genie_and_function");
  }
  if (p == genie_assertion) {
    return ("genie_assertion");
  }
  if (p == genie_assignation) {
    return ("genie_assignation");
  }
  if (p == genie_assignation_constant) {
    return ("genie_assignation_constant");
  }
  if (p == genie_call) {
    return ("genie_call");
  }
  if (p == genie_cast) {
    return ("genie_cast");
  }
  if ((void *) p == (void *) genie_closed) {
    return ("genie_closed");
  }
  if (p == genie_coercion) {
    return ("genie_coercion");
  }
  if (p == genie_collateral) {
    return ("genie_collateral");
  }
  if (p == genie_column_function) {
    return ("genie_column_function");
  }
  if ((void *) p == (void *) genie_conditional) {
    return ("genie_conditional");
  }
  if (p == genie_constant) {
    return ("genie_constant");
  }
  if (p == genie_denotation) {
    return ("genie_denotation");
  }
  if (p == genie_deproceduring) {
    return ("genie_deproceduring");
  }
  if (p == genie_dereference_loc_identifier) {
    return ("genie_dereference_loc_identifier");
  }
  if (p == genie_dereference_selection_name_quick) {
    return ("genie_dereference_selection_name_quick");
  }
  if (p == genie_dereference_slice_name_quick) {
    return ("genie_dereference_slice_name_quick");
  }
  if (p == genie_dereferencing) {
    return ("genie_dereferencing");
  }
  if (p == genie_dereferencing_quick) {
    return ("genie_dereferencing_quick");
  }
  if (p == genie_diagonal_function) {
    return ("genie_diagonal_function");
  }
  if (p == genie_dyadic) {
    return ("genie_dyadic");
  }
  if (p == genie_dyadic_quick) {
    return ("genie_dyadic_quick");
  }
  if ((void *) p == (void *) genie_enclosed) {
    return ("genie_enclosed");
  }
  if (p == genie_format_text) {
    return ("genie_format_text");
  }
  if (p == genie_formula) {
    return ("genie_formula");
  }
  if (p == genie_formula_div_real) {
    return ("genie_formula_div_real");
  }
  if (p == genie_formula_eq_int) {
    return ("genie_formula_eq_int");
  }
  if (p == genie_formula_eq_real) {
    return ("genie_formula_eq_real");
  }
  if (p == genie_formula_ge_int) {
    return ("genie_formula_ge_int");
  }
  if (p == genie_formula_ge_real) {
    return ("genie_formula_ge_real");
  }
  if (p == genie_formula_gt_int) {
    return ("genie_formula_gt_int");
  }
  if (p == genie_formula_gt_real) {
    return ("genie_formula_gt_real");
  }
  if (p == genie_formula_le_int) {
    return ("genie_formula_le_int");
  }
  if (p == genie_formula_le_real) {
    return ("genie_formula_le_real");
  }
  if (p == genie_formula_lt_int) {
    return ("genie_formula_lt_int");
  }
  if (p == genie_formula_lt_real) {
    return ("genie_formula_lt_real");
  }
  if (p == genie_formula_minus_int) {
    return ("genie_formula_minus_int");
  }
  if (p == genie_formula_minus_int_constant) {
    return ("genie_formula_minus_int_constant");
  }
  if (p == genie_formula_minus_real) {
    return ("genie_formula_minus_real");
  }
  if (p == genie_formula_ne_int) {
    return ("genie_formula_ne_int");
  }
  if (p == genie_formula_ne_real) {
    return ("genie_formula_ne_real");
  }
  if (p == genie_formula_over_int) {
    return ("genie_formula_over_int");
  }
  if (p == genie_formula_plus_int) {
    return ("genie_formula_plus_int");
  }
  if (p == genie_formula_plus_int_constant) {
    return ("genie_formula_plus_int_constant");
  }
  if (p == genie_formula_plus_real) {
    return ("genie_formula_plus_real");
  }
  if (p == genie_formula_times_int) {
    return ("genie_formula_times_int");
  }
  if (p == genie_formula_times_real) {
    return ("genie_formula_times_real");
  }
  if (p == genie_generator) {
    return ("genie_generator");
  }
  if (p == genie_identifier) {
    return ("genie_identifier");
  }
  if (p == genie_identifier_standenv) {
    return ("genie_identifier_standenv");
  }
  if (p == genie_identifier_standenv_proc) {
    return ("genie_identifier_standenv_proc");
  }
  if (p == genie_identity_relation) {
    return ("genie_identity_relation");
  }
  if (p == genie_identity_relation_is_nil) {
    return ("genie_identity_relation_is_nil");
  }
  if (p == genie_identity_relation_isnt_nil) {
    return ("genie_identity_relation_isnt_nil");
  }
  if ((void *) p == (void *) genie_int_case) {
    return ("genie_int_case");
  }
  if (p == genie_loc_identifier) {
    return ("genie_loc_identifier");
  }
  if ((void *) p == (void *) genie_loop) {
    return ("genie_loop");
  }
  if (p == genie_monadic) {
    return ("genie_monadic");
  }
  if (p == genie_nihil) {
    return ("genie_nihil");
  }
  if (p == genie_or_function) {
    return ("genie_or_function");
  }
#if defined ENABLE_PAR_CLAUSE
  if (p == genie_parallel) {
    return ("genie_parallel");
  }
#endif
  if (p == genie_routine_text) {
    return ("genie_routine_text");
  }
  if (p == genie_row_function) {
    return ("genie_row_function");
  }
  if (p == genie_rowing) {
    return ("genie_rowing");
  }
  if (p == genie_rowing_ref_row_of_row) {
    return ("genie_rowing_ref_row_of_row");
  }
  if (p == genie_rowing_ref_row_row) {
    return ("genie_rowing_ref_row_row");
  }
  if (p == genie_rowing_row_of_row) {
    return ("genie_rowing_row_of_row");
  }
  if (p == genie_rowing_row_row) {
    return ("genie_rowing_row_row");
  }
  if (p == genie_selection) {
    return ("genie_selection");
  }
  if (p == genie_selection_name_quick) {
    return ("genie_selection_name_quick");
  }
  if (p == genie_selection_value_quick) {
    return ("genie_selection_value_quick");
  }
  if (p == genie_skip) {
    return ("genie_skip");
  }
  if (p == genie_slice) {
    return ("genie_slice");
  }
  if (p == genie_slice_name_quick) {
    return ("genie_slice_name_quick");
  }
  if (p == genie_transpose_function) {
    return ("genie_transpose_function");
  }
  if (p == genie_unit) {
    return ("genie_unit");
  }
  if ((void *) p == (void *) genie_united_case) {
    return ("genie_united_case");
  }
  if (p == genie_uniting) {
    return ("genie_uniting");
  }
  if (p == genie_voiding) {
    return ("genie_voiding");
  }
  if (p == genie_voiding_assignation) {
    return ("genie_voiding_assignation");
  }
  if (p == genie_voiding_assignation_constant) {
    return ("genie_voiding_assignation_constant");
  }
  if (p == genie_widening) {
    return ("genie_widening");
  }
  if (p == genie_widening_int_to_real) {
    return ("genie_widening_int_to_real");
  }
  return ("unknown propagator");
}
