//! @file a68g-moids.h
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

#if !defined (__A68G_MOIDS_H__)
#define __A68G_MOIDS_H__

BOOL_T basic_coercions (MOID_T *, MOID_T *, int, int);
BOOL_T clause_allows_balancing (int);
BOOL_T is_balanced (NODE_T *, SOID_T *, int);
BOOL_T is_coercible_in_context (SOID_T *, SOID_T *, int);
BOOL_T is_coercible (MOID_T *, MOID_T *, int, int);
BOOL_T is_coercible_series (MOID_T *, MOID_T *, int, int);
BOOL_T is_coercible_stowed (MOID_T *, MOID_T *, int, int);
BOOL_T is_deprefable (MOID_T *);
BOOL_T is_equal_modes (MOID_T *, MOID_T *, int);
BOOL_T is_firmly_coercible (MOID_T *, MOID_T *, int);
BOOL_T is_firm (MOID_T *, MOID_T *);
BOOL_T is_meekly_coercible (MOID_T *, MOID_T *, int);
BOOL_T is_mode_isnt_well (MOID_T *);
BOOL_T is_moid_in_pack (MOID_T *, PACK_T *, int);
BOOL_T is_name_struct (MOID_T *);
BOOL_T is_nonproc (MOID_T *);
BOOL_T is_printable_mode (MOID_T *);
BOOL_T is_proc_ref_file_void_or_format (MOID_T *);
BOOL_T is_readable_mode (MOID_T *);
BOOL_T is_ref_row (MOID_T *);
BOOL_T is_rows_type (MOID_T *);
BOOL_T is_softly_coercible (MOID_T *, MOID_T *, int);
BOOL_T is_strongly_coercible (MOID_T *, MOID_T *, int);
BOOL_T is_strong_name (MOID_T *, MOID_T *);
BOOL_T is_strong_slice (MOID_T *, MOID_T *);
BOOL_T is_subset (MOID_T *, MOID_T *, int);
BOOL_T is_transput_mode (MOID_T *, char);
BOOL_T is_unitable (MOID_T *, MOID_T *, int);
BOOL_T is_weakly_coercible (MOID_T *, MOID_T *, int);
BOOL_T is_widenable (MOID_T *, MOID_T *);
char *mode_error_text (NODE_T *, MOID_T *, MOID_T *, int, int, int);
MOID_T *absorb_related_subsets (MOID_T *);
MOID_T *depref_completely (MOID_T *);
MOID_T *depref_once (MOID_T *);
MOID_T *depref_rows (MOID_T *, MOID_T *);
MOID_T *deproc_completely (MOID_T *);
MOID_T *derow (MOID_T *);
MOID_T *determine_unique_mode (SOID_T *, int);
MOID_T *get_balanced_mode (MOID_T *, int, BOOL_T, int);
MOID_T *make_series_from_moids (MOID_T *, MOID_T *);
MOID_T *make_united_mode (MOID_T *);
MOID_T *pack_soids_in_moid (SOID_T *, int);
MOID_T *unites_to (MOID_T *, MOID_T *);
MOID_T *widens_to (MOID_T *, MOID_T *);
void absorb_series_pack (MOID_T **);
void absorb_series_union_pack (MOID_T **);
void add_to_soid_list (SOID_T **, NODE_T *, SOID_T *);
void cannot_coerce (NODE_T *, MOID_T *, MOID_T *, int, int, int);
void coerce_enclosed (NODE_T *, SOID_T *);
void coerce_formula (NODE_T *, SOID_T *);
void coerce_operand (NODE_T *, SOID_T *);
void coerce_unit (NODE_T *, SOID_T *);
void free_soid_list (SOID_T *);
void investigate_firm_relations (PACK_T *, PACK_T *, BOOL_T *, BOOL_T *);
void make_coercion (NODE_T *, int, MOID_T *);
void make_depreffing_coercion (NODE_T *, MOID_T *, MOID_T *);
void make_ref_rowing_coercion (NODE_T *, MOID_T *, MOID_T *);
void make_rowing_coercion (NODE_T *, MOID_T *, MOID_T *);
void make_soid (SOID_T *, int, MOID_T *, int);
void make_strong (NODE_T *, MOID_T *, MOID_T *);
void make_uniting_coercion (NODE_T *, MOID_T *);
void make_void (NODE_T *, MOID_T *);
void make_widening_coercion (NODE_T *, MOID_T *, MOID_T *);
void mode_check_enclosed (NODE_T *, SOID_T *, SOID_T *);
void mode_check_formula (NODE_T *, SOID_T *, SOID_T *);
void mode_check_unit (NODE_T *, SOID_T *, SOID_T *);
void semantic_pitfall (NODE_T *, MOID_T *, int, int);
void warn_for_voiding (NODE_T *, SOID_T *, SOID_T *, int);

#define DEPREF A68_TRUE
#define NO_DEPREF A68_FALSE

#define IF_MODE_IS_WELL(n) (! ((n) == M_ERROR || (n) == M_UNDEFINED))
#define INSERT_COERCIONS(n, p, q) make_strong ((n), (p), MOID (q))

#endif
