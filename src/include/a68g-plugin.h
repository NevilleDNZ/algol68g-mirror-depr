//! @file a68g-compiler.h
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

#if ! defined (__A68G_COMPILER_H__)
#define __A68G_COMPILER_H__

typedef union UFU UFU;

union UFU
{
  UNSIGNED_T u;
  REAL_T f;
};

#define BASIC(p, n) (basic_unit (stems_from ((p), (n))))

#define CON "const"
#define ELM "elem"
#define TMP "tmp"
#define ARG "arg"
#define ARR "array"
#define DEC "declarer"
#define DRF "deref"
#define DSP "display"
#define FUN "function"
#define PUP "pop"
#define REF "ref"
#define SEL "field"
#define TUP "tuple"

#define A68_MAKE_NOTHING 0
#define A68_MAKE_OTHERS 1
#define A68_MAKE_FUNCTION 2

#define OFFSET_OFF(s) (OFFSET (NODE_PACK (SUB (s))))
#define WIDEN_TO(p, a, b) (MOID (p) == MODE (b) && MOID (SUB (p)) == MODE (a))

#define NEEDS_DNS(m) (m != NO_MOID && (IS (m, REF_SYMBOL) || IS (m, PROC_SYMBOL) || IS (m, UNION_SYMBOL) || IS (m, FORMAT_SYMBOL)))

#define CODE_EXECUTE(p) {\
  indentf (out, snprintf (A68 (edit_line), SNPRINTF_SIZE, "EXECUTE_UNIT_TRACE (_NODE_ (%d));", NUMBER (p)));\
  }

#define NAME_SIZE 200

extern BOOK_T *signed_in (int, int, char *);
extern BOOL_T basic_argument (NODE_T *);
extern BOOL_T basic_call (NODE_T *);
extern BOOL_T basic_collateral (NODE_T *);
extern BOOL_T basic_conditional (NODE_T *);
extern BOOL_T basic_formula (NODE_T *);
extern BOOL_T basic_indexer (NODE_T *);
extern BOOL_T basic_mode (MOID_T *);
extern BOOL_T basic_mode_non_row (MOID_T *);
extern BOOL_T basic_monadic_formula (NODE_T *);
extern BOOL_T basic_serial (NODE_T *, int);
extern BOOL_T basic_slice (NODE_T *);
extern BOOL_T basic_unit (NODE_T *);
extern BOOL_T basic_unit (NODE_T *);
extern BOOL_T need_initialise_frame (NODE_T *);
extern BOOL_T primitive_mode (MOID_T *);
extern BOOL_T same_tree (NODE_T *, NODE_T *);
extern char *compile_call (NODE_T *, FILE_T);
extern char *compile_cast (NODE_T *, FILE_T);
extern char *compile_denotation (NODE_T *, FILE_T);
extern char *compile_dereference_identifier (NODE_T *, FILE_T);
extern char *compile_formula (NODE_T *, FILE_T);
extern char *compile_identifier (NODE_T *, FILE_T);
extern char *gen_basic_conditional (NODE_T *, FILE_T, int);
extern char *gen_basic (NODE_T *, FILE_T);
extern char *gen_call (NODE_T *, FILE_T, int);
extern char *gen_cast (NODE_T *, FILE_T, int);
extern char *gen_closed_clause (NODE_T *, FILE_T, int);
extern char *gen_code_clause (NODE_T *, FILE_T, int);
extern char *gen_collateral_clause (NODE_T *, FILE_T, int);
extern char *gen_conditional_clause (NODE_T *, FILE_T, int);
extern char *gen_denotation (NODE_T *, FILE_T, int);
extern char *gen_deproceduring (NODE_T *, FILE_T, int);
extern char *gen_dereference_identifier (NODE_T *, FILE_T, int);
extern char *gen_dereference_selection (NODE_T *, FILE_T, int);
extern char *gen_dereference_slice (NODE_T *, FILE_T, int);
extern char *gen_formula (NODE_T *, FILE_T, int);
extern char *gen_identifier (NODE_T *, FILE_T, int);
extern char *gen_identity_relation (NODE_T *, FILE_T, int);
extern char *gen_int_case_clause (NODE_T *, FILE_T, int);
extern char *gen_loop_clause (NODE_T *, FILE_T, int);
extern char *gen_selection (NODE_T *, FILE_T, int);
extern char *gen_slice (NODE_T *, FILE_T, int);
extern char *gen_uniting (NODE_T *, FILE_T, int);
extern char *gen_unit (NODE_T *, FILE_T, BOOL_T);
extern char *gen_voiding_assignation_identifier (NODE_T *, FILE_T, int);
extern char *gen_voiding_assignation_selection (NODE_T *, FILE_T, int);
extern char *gen_voiding_assignation_slice (NODE_T *, FILE_T, int);
extern char *gen_voiding_call (NODE_T *, FILE_T, int);
extern char *gen_voiding_deproceduring (NODE_T *, FILE_T, int);
extern char *gen_voiding_formula (NODE_T *, FILE_T, int);
extern char *inline_mode (MOID_T *);
extern char *internal_mode (MOID_T *);
extern char *make_name (char *, char *, char *, int);
extern char *make_unic_name (char *, char *, char *, char *);
extern char *moid_with_name (char *, MOID_T *, char *);
extern DEC_T *add_declaration (DEC_T **, char *, int, char *);
extern DEC_T *add_identifier (DEC_T **, int, char *);
extern NODE_T *stems_from (NODE_T *, int);
extern void comment_source (NODE_T *, FILE_T);
extern void constant_folder (NODE_T *, FILE_T, int);
extern void gen_assign (NODE_T *, FILE_T, char *);
extern void gen_basics (NODE_T *, FILE_T);
extern void gen_check_init (NODE_T *, FILE_T, char *);
extern void gen_declaration_list (NODE_T *, FILE_T, int *, char *);
extern void gen_push (NODE_T *, FILE_T);
extern void gen_serial_clause (NODE_T *, FILE_T, NODE_T **, int *, int *, char *, int);
extern void gen_units (NODE_T *, FILE_T);
extern void get_stack (NODE_T *, FILE_T, char *, char *);
extern void indentf (FILE_T, int);
extern void indentf (FILE_T, int);
extern void indent (FILE_T, char *);
extern void indent (FILE_T, char *);
extern void init_static_frame (FILE_T, NODE_T *);
extern void inline_arguments (NODE_T *, FILE_T, int, int *);
extern void inline_call (NODE_T *, FILE_T, int);
extern void inline_closed (NODE_T *, FILE_T, int);
extern void inline_collateral (NODE_T *, FILE_T, int);
extern void inline_collateral_units (NODE_T *, FILE_T, int);
extern void inline_comment_source (NODE_T *, FILE_T);
extern void inline_conditional (NODE_T *, FILE_T, int);
extern void inline_denotation (NODE_T *, FILE_T, int);
extern void inline_dereference_identifier (NODE_T *, FILE_T, int);
extern void inline_dereference_selection (NODE_T *, FILE_T, int);
extern void inline_dereference_slice (NODE_T *, FILE_T, int);
extern void inline_formula (NODE_T *, FILE_T, int);
extern void inline_identifier (NODE_T *, FILE_T, int);
extern void inline_identity_relation (NODE_T *, FILE_T, int);
extern void inline_indexer (NODE_T *, FILE_T, int, INT_T *, char *);
extern void inline_monadic_formula (NODE_T *, FILE_T, int);
extern void inline_ref_identifier (NODE_T *, FILE_T, int);
extern void inline_selection (NODE_T *, FILE_T, int);
extern void inline_selection_ref_to_ref (NODE_T *, FILE_T, int);
extern void inline_single_argument (NODE_T *, FILE_T, int);
extern void inline_slice (NODE_T *, FILE_T, int);
extern void inline_slice_ref_to_ref (NODE_T *, FILE_T, int);
extern void inline_unit (NODE_T *, FILE_T, int);
extern void inline_unit (NODE_T *, FILE_T, int);
extern void inline_widening (NODE_T *, FILE_T, int);
extern void print_declarations (FILE_T, DEC_T *);
extern void sign_in (int, int, char *, void *, int);
extern void sign_in_name (char *, int *);
extern void undentf (FILE_T, int);
extern void undent (FILE_T, char *);
extern void write_fun_postlude (NODE_T *, FILE_T, char *);
extern void write_fun_prelude (NODE_T *, FILE_T, char *);
extern void write_prelude (FILE_T);

// The phases we go through.

enum
{ L_NONE = 0, L_DECLARE = 1, L_INITIALISE, L_EXECUTE, L_EXECUTE_2, L_YIELD, L_PUSH };

#define UNIC_NAME(k) (A68_OPT (unic_functions)[k].fun)

enum
{ UNIC_EXISTS, UNIC_MAKE_NEW, UNIC_MAKE_ALT };

// TRANSLATION tabulates translations for genie actions.
// This tells what to call for an A68 action.

typedef int LEVEL_T;

typedef struct
{
  GPROC *procedure;
  char *code;
} TRANSLATION;

extern TRANSLATION *monadics, *dyadics, *functions;

extern TRANSLATION monadics_nocheck[];
extern TRANSLATION monadics_check[];
extern TRANSLATION dyadics_nocheck[];
extern TRANSLATION dyadics_check[];
extern TRANSLATION functions_nocheck[];
extern TRANSLATION functions_check[];
extern TRANSLATION constants[];

#endif
