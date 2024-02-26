//! @file a68g-parser.h
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

#if !defined (__A68G_PARSER_H__)
#define __A68G_PARSER_H__

#define STOP_CHAR 127

extern BOOL_T dont_mark_here (NODE_T *);
extern BOOL_T is_coercion (NODE_T *);
extern BOOL_T is_firm (MOID_T *, MOID_T *);
extern BOOL_T is_firm (MOID_T *, MOID_T *);
extern BOOL_T is_formal_bounds (NODE_T *);
extern BOOL_T is_loop_keyword (NODE_T *);
extern BOOL_T is_modes_equivalent (MOID_T *, MOID_T *);
extern BOOL_T is_new_lexical_level (NODE_T *);
extern BOOL_T is_one_of (NODE_T * p, ...);
extern BOOL_T is_ref_refety_flex (MOID_T *);
extern BOOL_T is_semicolon_less (NODE_T *);
extern BOOL_T is_subset (MOID_T *, MOID_T *, int);
extern BOOL_T is_unitable (MOID_T *, MOID_T *, int);
extern BOOL_T is_unit_terminator (NODE_T *);
extern BOOL_T lexical_analyser (void);
extern BOOL_T match_string (char *, char *, char);
extern BOOL_T prove_moid_equivalence (MOID_T *, MOID_T *);
extern BOOL_T whether (NODE_T * p, ...);
extern char *phrase_to_text (NODE_T *, NODE_T **);
extern GINFO_T *new_genie_info (void);
extern int count_formal_bounds (NODE_T *);
extern int count_operands (NODE_T *);
extern int count_pack_members (PACK_T *);
extern int first_tag_global (TABLE_T *, char *);
extern int get_good_attribute (NODE_T *);
extern int is_identifier_or_label_global (TABLE_T *, char *);
extern KEYWORD_T *find_keyword_from_attribute (KEYWORD_T *, int);
extern KEYWORD_T *find_keyword (KEYWORD_T *, char *);
extern LINE_T *new_source_line (void);
extern MOID_T *add_mode (MOID_T **, int, int, NODE_T *, MOID_T *, PACK_T *);
extern MOID_T *depref_completely (MOID_T *);
extern MOID_T *get_mode_from_declarer (NODE_T *);
extern MOID_T *new_moid (void);
extern MOID_T *register_extra_mode (MOID_T **, MOID_T *);
extern MOID_T *unites_to (MOID_T *, MOID_T *);
extern NODE_INFO_T *new_node_info (void);
extern NODE_T *get_next_format_pattern (NODE_T *, A68_REF, BOOL_T);
extern NODE_T *new_node (void);
extern NODE_T *reduce_dyadic (NODE_T *, int u);
extern NODE_T *some_node (char *);
extern NODE_T *top_down_loop (NODE_T *);
extern NODE_T *top_down_skip_unit (NODE_T *);
extern PACK_T *absorb_union_pack (PACK_T *);
extern PACK_T *new_pack (void);
extern TABLE_T *find_level (NODE_T *, int);
extern TABLE_T *new_symbol_table (TABLE_T *);
extern TAG_T *add_tag (TABLE_T *, int, NODE_T *, MOID_T *, int);
extern TAG_T *find_tag_global (TABLE_T *, int, char *);
extern TAG_T *find_tag_local (TABLE_T *, int, char *);
extern TAG_T *new_tag (void);
extern TOKEN_T *add_token (TOKEN_T **, char *);
extern void a68_parser (void);
extern void add_mode_to_pack_end (PACK_T **, MOID_T *, char *, NODE_T *);
extern void add_mode_to_pack (PACK_T **, MOID_T *, char *, NODE_T *);
extern void append_source_line (char *, LINE_T **, int *, char *);
extern void assign_offsets (NODE_T *);
extern void assign_offsets_packs (MOID_T *);
extern void assign_offsets_table (TABLE_T *);
extern void bind_format_tags_to_tree (NODE_T *);
extern void bind_routine_tags_to_tree (NODE_T *);
extern void bottom_up_error_check (NODE_T *);
extern void bottom_up_parser (NODE_T *);
extern void check_parenthesis (NODE_T *);
extern void coercion_inserter (NODE_T *);
extern void coercion_inserter (NODE_T *);
extern void collect_taxes (NODE_T *);
extern void contract_union (MOID_T *);
extern void count_pictures (NODE_T *, int *);
extern void elaborate_bold_tags (NODE_T *);
extern void extract_declarations (NODE_T *);
extern void extract_declarations (NODE_T *);
extern void extract_identities (NODE_T *);
extern void extract_indicants (NODE_T *);
extern void extract_labels (NODE_T *, int);
extern void extract_operators (NODE_T *);
extern void extract_priorities (NODE_T *);
extern void extract_proc_identities (NODE_T *);
extern void extract_proc_variables (NODE_T *);
extern void extract_variables (NODE_T *);
extern void fill_symbol_table_outer (NODE_T *, TABLE_T *);
extern void finalise_symbol_table_setup (NODE_T *, int);
extern void free_genie_heap (NODE_T *);
extern void get_max_simplout_size (NODE_T *);
extern void get_refinements (void);
extern void ignore_superfluous_semicolons (NODE_T *);
extern void init_before_tokeniser (void);
extern void init_parser (void);
extern void jumps_from_procs (NODE_T * p);
extern void make_moid_list (MODULE_T *);
extern void make_special_mode (MOID_T **, int);
extern void make_standard_environ (void);
extern void make_sub (NODE_T *, NODE_T *, int);
extern void mark_auxilliary (NODE_T *);
extern void mark_jump_in_par (NODE_T *, BOOL_T);
extern void mark_moids (NODE_T *);
extern void mode_checker (NODE_T *);
extern void mode_checker (NODE_T *);
extern void portcheck (NODE_T *);
extern void preliminary_symbol_table_setup (NODE_T *);
extern void prune_echoes (OPTION_LIST_T *);
extern void put_refinements (void);
extern void rearrange_goto_less_jumps (NODE_T *);
extern void recover_from_error (NODE_T *, int, BOOL_T);
extern void reduce_arguments (NODE_T *);
extern void reduce_basic_declarations (NODE_T *);
extern void reduce_bounds (NODE_T *);
extern void reduce_branch (NODE_T *, int);
extern void reduce_collateral_clauses (NODE_T *);
extern void reduce_declaration_lists (NODE_T *);
extern void reduce_declarers (NODE_T *, int);
extern void reduce_enclosed_clauses (NODE_T *, int);
extern void reduce_enquiry_clauses (NODE_T *);
extern void reduce_erroneous_units (NODE_T *);
extern void reduce_format_texts (NODE_T *);
extern void reduce_formulae (NODE_T *);
extern void reduce_generic_arguments (NODE_T *);
extern void reduce (NODE_T *, void (*)(NODE_T *), BOOL_T *, ...);
extern void reduce_primaries (NODE_T *, int);
extern void reduce_primary_parts (NODE_T *, int);
extern void reduce_right_to_left_constructs (NODE_T * q);
extern void reduce_secondaries (NODE_T *);
extern void reduce_serial_clauses (NODE_T *);
extern void reduce_tertiaries (NODE_T *);
extern void reduce_units (NODE_T *);
extern void register_nodes (NODE_T *);
extern void renumber_moids (MOID_T *, int);
extern void renumber_nodes (NODE_T *, int *);
extern void reset_symbol_table_nest_count (NODE_T *);
extern void scope_checker (NODE_T *);
extern void scope_checker (NODE_T *);
extern void set_moid_sizes (MOID_T *);
extern void set_nest (NODE_T *, NODE_T *);
extern void set_proc_level (NODE_T *, int);
extern void set_up_tables (void);
extern void substitute_brackets (NODE_T *);
extern void tie_label_to_serial (NODE_T *);
extern void tie_label_to_unit (NODE_T *);
extern void top_down_parser (NODE_T *);
extern void verbosity (void);
extern void victal_checker (NODE_T *);
extern void warn_for_unused_tags (NODE_T *);
extern void widen_denotation (NODE_T *);

#endif
