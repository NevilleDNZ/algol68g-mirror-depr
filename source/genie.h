/*!
\file genie.h
\brief contains genie related definitions
**/

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

#if ! defined A68G_GENIE_H
#define A68G_GENIE_H

/* Macros. */

#define INITIALISED(z) ((z)->status & INITIALISED_MASK)

#define BITS_WIDTH ((int) (1 + ceil (log (A68_MAX_INT) / log(2))))
#define INT_WIDTH ((int) (1 + floor (log (A68_MAX_INT) / log (10))))

#define CHAR_WIDTH (1 + (int) log10 ((double) SCHAR_MAX))
#define REAL_WIDTH (DBL_DIG)
#define EXP_WIDTH ((int) (1 + log10 ((double) DBL_MAX_10_EXP)))

#define HEAP_ADDRESS(n) ((BYTE_T *) & (heap_segment[n]))

#define LHS_MODE(p) (MOID (PACK (MOID (p))))
#define RHS_MODE(p) (MOID (NEXT (PACK (MOID (p)))))

/* Activation records in the frame stack. */

typedef struct ACTIVATION_RECORD ACTIVATION_RECORD;

struct ACTIVATION_RECORD
{
  ADDR_T static_link, dynamic_link, dynamic_scope, parameters;
  NODE_T *node;
  jmp_buf *jump_stat;
  BOOL_T proc_frame;
  int frame_no, frame_level, parameter_level;
#if defined ENABLE_PAR_CLAUSE
  pthread_t thread_id;
#endif
};

#define FRAME_ADDRESS(n) ((BYTE_T *) &(stack_segment[n]))
#define FRAME_CLEAR(m) FILL_ALIGNED ((BYTE_T *) FRAME_OFFSET (FRAME_INFO_SIZE), 0, (m))
#define FRAME_DYNAMIC_LINK(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->dynamic_link)
#define FRAME_DYNAMIC_SCOPE(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->dynamic_scope)
#define FRAME_INCREMENT(n) (SYMBOL_TABLE (FRAME_TREE(n))->ap_increment)
#define FRAME_INFO_SIZE (A68_ALIGN (ALIGNED_SIZE_OF (ACTIVATION_RECORD)))
#define FRAME_JUMP_STAT(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->jump_stat)
#define FRAME_LEXICAL_LEVEL(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->frame_level)
#define FRAME_LOCAL(n, m) (FRAME_ADDRESS ((n) + FRAME_INFO_SIZE + (m)))
#define FRAME_NUMBER(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->frame_no)
#define FRAME_OBJECT(n) (FRAME_OFFSET (FRAME_INFO_SIZE + (n)))
#define FRAME_OFFSET(n) (FRAME_ADDRESS (frame_pointer + (n)))
#define FRAME_OUTER(n) (SYMBOL_TABLE (FRAME_TREE(n))->outer)
#define FRAME_PARAMETER_LEVEL(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->parameter_level)
#define FRAME_PARAMETERS(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->parameters)
#define FRAME_PROC_FRAME(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->proc_frame)
#define FRAME_SIZE(fp) (FRAME_INFO_SIZE + FRAME_INCREMENT (fp))
#define FRAME_STATIC_LINK(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->static_link)
#define FRAME_TOP (FRAME_OFFSET (0))
#define FRAME_TREE(n) (NODE ((ACTIVATION_RECORD *) FRAME_ADDRESS(n)))

#if defined ENABLE_PAR_CLAUSE
#define FRAME_THREAD_ID(n) (((ACTIVATION_RECORD *) FRAME_ADDRESS(n))->thread_id)
#endif

/* Stack manipulation. */

#define STACK_ADDRESS(n) ((BYTE_T *) &(stack_segment[(n)]))
#define STACK_OFFSET(n) (STACK_ADDRESS (stack_pointer + (n)))
#define STACK_TOP (STACK_ADDRESS (stack_pointer))

/* External symbols. */

extern ADDR_T frame_pointer, stack_pointer, heap_pointer, handle_pointer, global_pointer, frame_start, frame_end, stack_start, stack_end, finish_frame_pointer;
extern A68_FORMAT nil_format;
extern A68_HANDLE nil_handle, *free_handles, *busy_handles;
extern A68_REF nil_ref;
extern A68_REF stand_in, stand_out;
extern BOOL_T in_monitor, do_confirm_exit;
extern BYTE_T *stack_segment, *heap_segment, *handle_segment;
extern MOID_T *top_expr_moid;
extern char *watchpoint_expression;
extern double cputime_0;
extern int global_level, max_lex_lvl, ret_code;
extern jmp_buf genie_exit_label, monitor_exit_label;

extern int frame_stack_size, expr_stack_size, heap_size, handle_pool_size;
extern int stack_limit, frame_stack_limit, expr_stack_limit;
extern int storage_overhead;

/* External symbols. */

#if defined ENABLE_PAR_CLAUSE
#endif

extern BOOL_T confirm_exit (void);
extern int free_handle_count, max_handle_count;
extern void genie_find_proc_op (NODE_T *, int *);
extern void sweep_heap (NODE_T *, ADDR_T);

extern GENIE_PROCEDURE genie_abs_bool;
extern GENIE_PROCEDURE genie_abs_char;
extern GENIE_PROCEDURE genie_abs_complex;
extern GENIE_PROCEDURE genie_abs_int;
extern GENIE_PROCEDURE genie_abs_long_complex;
extern GENIE_PROCEDURE genie_abs_long_mp;
extern GENIE_PROCEDURE genie_abs_real;
extern GENIE_PROCEDURE genie_acos_long_complex;
extern GENIE_PROCEDURE genie_acos_long_mp;
extern GENIE_PROCEDURE genie_acronym;
extern GENIE_PROCEDURE genie_add_bytes;
extern GENIE_PROCEDURE genie_add_char;
extern GENIE_PROCEDURE genie_add_complex;
extern GENIE_PROCEDURE genie_add_int;
extern GENIE_PROCEDURE genie_add_long_bytes;
extern GENIE_PROCEDURE genie_add_long_complex;
extern GENIE_PROCEDURE genie_add_long_int;
extern GENIE_PROCEDURE genie_add_long_mp;
extern GENIE_PROCEDURE genie_add_real;
extern GENIE_PROCEDURE genie_add_string;
extern GENIE_PROCEDURE genie_and_bits;
extern GENIE_PROCEDURE genie_and_bool;
extern GENIE_PROCEDURE genie_and_long_mp;
extern GENIE_PROCEDURE genie_arccosh_long_mp;
extern GENIE_PROCEDURE genie_arccos_real;
extern GENIE_PROCEDURE genie_arcsinh_long_mp;
extern GENIE_PROCEDURE genie_arcsin_real;
extern GENIE_PROCEDURE genie_arctanh_long_mp;
extern GENIE_PROCEDURE genie_arctan_real;
extern GENIE_PROCEDURE genie_arg_complex;
extern GENIE_PROCEDURE genie_arg_long_complex;
extern GENIE_PROCEDURE genie_asin_long_complex;
extern GENIE_PROCEDURE genie_asin_long_mp;
extern GENIE_PROCEDURE genie_atan2_long_mp;
extern GENIE_PROCEDURE genie_atan2_real;
extern GENIE_PROCEDURE genie_atan_long_complex;
extern GENIE_PROCEDURE genie_atan_long_mp;
extern GENIE_PROCEDURE genie_bin_int;
extern GENIE_PROCEDURE genie_bin_long_mp;
extern GENIE_PROCEDURE genie_bits_lengths;
extern GENIE_PROCEDURE genie_bits_pack;
extern GENIE_PROCEDURE genie_bits_shorths;
extern GENIE_PROCEDURE genie_bits_width;
extern GENIE_PROCEDURE genie_break;
extern GENIE_PROCEDURE genie_bytes_lengths;
extern GENIE_PROCEDURE genie_bytespack;
extern GENIE_PROCEDURE genie_bytes_shorths;
extern GENIE_PROCEDURE genie_bytes_width;
extern GENIE_PROCEDURE genie_cd;
extern GENIE_PROCEDURE genie_char_in_string;
extern GENIE_PROCEDURE genie_clear_bits;
extern GENIE_PROCEDURE genie_clear_long_bits;
extern GENIE_PROCEDURE genie_clear_longlong_bits;
extern GENIE_PROCEDURE genie_complex_lengths;
extern GENIE_PROCEDURE genie_complex_shorths;
extern GENIE_PROCEDURE genie_conj_complex;
extern GENIE_PROCEDURE genie_conj_long_complex;
extern GENIE_PROCEDURE genie_cosh_long_mp;
extern GENIE_PROCEDURE genie_cos_long_complex;
extern GENIE_PROCEDURE genie_cos_long_mp;
extern GENIE_PROCEDURE genie_cos_real;
extern GENIE_PROCEDURE genie_cputime;
extern GENIE_PROCEDURE genie_curt_long_mp;
extern GENIE_PROCEDURE genie_curt_real;
extern GENIE_PROCEDURE genie_debug;
extern GENIE_PROCEDURE genie_directory;
extern GENIE_PROCEDURE genie_divab_complex;
extern GENIE_PROCEDURE genie_divab_long_complex;
extern GENIE_PROCEDURE genie_divab_long_mp;
extern GENIE_PROCEDURE genie_divab_real;
extern GENIE_PROCEDURE genie_div_complex;
extern GENIE_PROCEDURE genie_div_int;
extern GENIE_PROCEDURE genie_div_long_complex;
extern GENIE_PROCEDURE genie_div_long_mp;
extern GENIE_PROCEDURE genie_div_real;
extern GENIE_PROCEDURE genie_dyad_elems;
extern GENIE_PROCEDURE genie_dyad_lwb;
extern GENIE_PROCEDURE genie_dyad_upb;
extern GENIE_PROCEDURE genie_elem_bits;
extern GENIE_PROCEDURE genie_elem_bytes;
extern GENIE_PROCEDURE genie_elem_long_bits;
extern GENIE_PROCEDURE genie_elem_long_bits;
extern GENIE_PROCEDURE genie_elem_long_bytes;
extern GENIE_PROCEDURE genie_elem_longlong_bits;
extern GENIE_PROCEDURE genie_elem_string;
extern GENIE_PROCEDURE genie_entier_long_mp;
extern GENIE_PROCEDURE genie_entier_real;
extern GENIE_PROCEDURE genie_eq_bits;
extern GENIE_PROCEDURE genie_eq_bool;
extern GENIE_PROCEDURE genie_eq_bytes;
extern GENIE_PROCEDURE genie_eq_char;
extern GENIE_PROCEDURE genie_eq_complex;
extern GENIE_PROCEDURE genie_eq_int;
extern GENIE_PROCEDURE genie_eq_long_bytes;
extern GENIE_PROCEDURE genie_eq_long_complex;
extern GENIE_PROCEDURE genie_eq_long_mp;
extern GENIE_PROCEDURE genie_eq_real;
extern GENIE_PROCEDURE genie_eq_string;
extern GENIE_PROCEDURE genie_evaluate;
extern GENIE_PROCEDURE genie_exp_long_complex;
extern GENIE_PROCEDURE genie_exp_long_mp;
extern GENIE_PROCEDURE genie_exp_real;
extern GENIE_PROCEDURE genie_exp_width;
extern GENIE_PROCEDURE genie_file_is_block_device;
extern GENIE_PROCEDURE genie_file_is_char_device;
extern GENIE_PROCEDURE genie_file_is_directory;
extern GENIE_PROCEDURE genie_file_is_regular;
extern GENIE_PROCEDURE genie_file_mode;
extern GENIE_PROCEDURE genie_first_random;
extern GENIE_PROCEDURE genie_garbage_collections;
extern GENIE_PROCEDURE genie_garbage_freed;
extern GENIE_PROCEDURE genie_garbage_seconds;
extern GENIE_PROCEDURE genie_ge_bits;
extern GENIE_PROCEDURE genie_ge_bytes;
extern GENIE_PROCEDURE genie_ge_char;
extern GENIE_PROCEDURE genie_ge_int;
extern GENIE_PROCEDURE genie_ge_long_bits;
extern GENIE_PROCEDURE genie_ge_long_bytes;
extern GENIE_PROCEDURE genie_ge_long_mp;
extern GENIE_PROCEDURE genie_ge_real;
extern GENIE_PROCEDURE genie_ge_string;
extern GENIE_PROCEDURE genie_get_sound;
extern GENIE_PROCEDURE genie_gt_bytes;
extern GENIE_PROCEDURE genie_gt_char;
extern GENIE_PROCEDURE genie_gt_int;
extern GENIE_PROCEDURE genie_gt_long_bytes;
extern GENIE_PROCEDURE genie_gt_long_mp;
extern GENIE_PROCEDURE genie_gt_real;
extern GENIE_PROCEDURE genie_gt_string;
extern GENIE_PROCEDURE genie_icomplex;
extern GENIE_PROCEDURE genie_idf;
extern GENIE_PROCEDURE genie_idle;
extern GENIE_PROCEDURE genie_iint_complex;
extern GENIE_PROCEDURE genie_im_complex;
extern GENIE_PROCEDURE genie_im_long_complex;
extern GENIE_PROCEDURE genie_int_lengths;
extern GENIE_PROCEDURE genie_int_shorths;
extern GENIE_PROCEDURE genie_int_width;
extern GENIE_PROCEDURE genie_is_alnum;
extern GENIE_PROCEDURE genie_is_alpha;
extern GENIE_PROCEDURE genie_is_cntrl;
extern GENIE_PROCEDURE genie_is_digit;
extern GENIE_PROCEDURE genie_is_graph;
extern GENIE_PROCEDURE genie_is_lower;
extern GENIE_PROCEDURE genie_is_print;
extern GENIE_PROCEDURE genie_is_punct;
extern GENIE_PROCEDURE genie_is_space;
extern GENIE_PROCEDURE genie_is_upper;
extern GENIE_PROCEDURE genie_is_xdigit;
extern GENIE_PROCEDURE genie_last_char_in_string;
extern GENIE_PROCEDURE genie_le_bits;
extern GENIE_PROCEDURE genie_le_bytes;
extern GENIE_PROCEDURE genie_le_char;
extern GENIE_PROCEDURE genie_le_int;
extern GENIE_PROCEDURE genie_le_long_bits;
extern GENIE_PROCEDURE genie_le_long_bytes;
extern GENIE_PROCEDURE genie_le_long_mp;
extern GENIE_PROCEDURE genie_leng_bytes;
extern GENIE_PROCEDURE genie_lengthen_complex_to_long_complex;
extern GENIE_PROCEDURE genie_lengthen_int_to_long_mp;
extern GENIE_PROCEDURE genie_lengthen_long_complex_to_longlong_complex;
extern GENIE_PROCEDURE genie_lengthen_long_mp_to_longlong_mp;
extern GENIE_PROCEDURE genie_lengthen_real_to_long_mp;
extern GENIE_PROCEDURE genie_lengthen_unsigned_to_long_mp;
extern GENIE_PROCEDURE genie_le_real;
extern GENIE_PROCEDURE genie_le_string;
extern GENIE_PROCEDURE genie_lj_e_12_6;
extern GENIE_PROCEDURE genie_lj_f_12_6;
extern GENIE_PROCEDURE genie_ln_long_complex;
extern GENIE_PROCEDURE genie_ln_long_mp;
extern GENIE_PROCEDURE genie_ln_real;
extern GENIE_PROCEDURE genie_log_long_mp;
extern GENIE_PROCEDURE genie_log_real;
extern GENIE_PROCEDURE genie_long_bits_pack;
extern GENIE_PROCEDURE genie_long_bits_width;
extern GENIE_PROCEDURE genie_long_bytespack;
extern GENIE_PROCEDURE genie_long_bytes_width;
extern GENIE_PROCEDURE genie_long_exp_width;
extern GENIE_PROCEDURE genie_long_int_width;
extern GENIE_PROCEDURE genie_longlong_bits_width;
extern GENIE_PROCEDURE genie_longlong_exp_width;
extern GENIE_PROCEDURE genie_longlong_int_width;
extern GENIE_PROCEDURE genie_longlong_max_bits;
extern GENIE_PROCEDURE genie_longlong_max_int;
extern GENIE_PROCEDURE genie_longlong_max_real;
extern GENIE_PROCEDURE genie_longlong_min_real;
extern GENIE_PROCEDURE genie_longlong_real_width;
extern GENIE_PROCEDURE genie_longlong_small_real;
extern GENIE_PROCEDURE genie_long_max_bits;
extern GENIE_PROCEDURE genie_long_max_int;
extern GENIE_PROCEDURE genie_long_max_real;
extern GENIE_PROCEDURE genie_long_min_real;
extern GENIE_PROCEDURE genie_long_next_random;
extern GENIE_PROCEDURE genie_long_real_width;
extern GENIE_PROCEDURE genie_long_small_real;
extern GENIE_PROCEDURE genie_lt_bytes;
extern GENIE_PROCEDURE genie_lt_char;
extern GENIE_PROCEDURE genie_lt_int;
extern GENIE_PROCEDURE genie_lt_long_bytes;
extern GENIE_PROCEDURE genie_lt_long_mp;
extern GENIE_PROCEDURE genie_lt_real;
extern GENIE_PROCEDURE genie_lt_string;
extern GENIE_PROCEDURE genie_make_term;
extern GENIE_PROCEDURE genie_max_abs_char;
extern GENIE_PROCEDURE genie_max_bits;
extern GENIE_PROCEDURE genie_max_int;
extern GENIE_PROCEDURE genie_max_real;
extern GENIE_PROCEDURE genie_min_real;
extern GENIE_PROCEDURE genie_minusab_complex;
extern GENIE_PROCEDURE genie_minusab_int;
extern GENIE_PROCEDURE genie_minusab_long_complex;
extern GENIE_PROCEDURE genie_minusab_long_int;
extern GENIE_PROCEDURE genie_minusab_long_mp;
extern GENIE_PROCEDURE genie_minusab_real;
extern GENIE_PROCEDURE genie_minus_complex;
extern GENIE_PROCEDURE genie_minus_int;
extern GENIE_PROCEDURE genie_minus_long_complex;
extern GENIE_PROCEDURE genie_minus_long_int;
extern GENIE_PROCEDURE genie_minus_long_mp;
extern GENIE_PROCEDURE genie_minus_real;
extern GENIE_PROCEDURE genie_modab_int;
extern GENIE_PROCEDURE genie_modab_long_mp;
extern GENIE_PROCEDURE genie_mod_int;
extern GENIE_PROCEDURE genie_mod_long_mp;
extern GENIE_PROCEDURE genie_monad_elems;
extern GENIE_PROCEDURE genie_monad_lwb;
extern GENIE_PROCEDURE genie_monad_upb;
extern GENIE_PROCEDURE genie_mul_complex;
extern GENIE_PROCEDURE genie_mul_int;
extern GENIE_PROCEDURE genie_mul_long_complex;
extern GENIE_PROCEDURE genie_mul_long_int;
extern GENIE_PROCEDURE genie_mul_long_mp;
extern GENIE_PROCEDURE genie_mul_real;
extern GENIE_PROCEDURE genie_ne_bits;
extern GENIE_PROCEDURE genie_ne_bool;
extern GENIE_PROCEDURE genie_ne_bytes;
extern GENIE_PROCEDURE genie_ne_char;
extern GENIE_PROCEDURE genie_ne_complex;
extern GENIE_PROCEDURE genie_ne_int;
extern GENIE_PROCEDURE genie_ne_long_bytes;
extern GENIE_PROCEDURE genie_ne_long_complex;
extern GENIE_PROCEDURE genie_ne_long_mp;
extern GENIE_PROCEDURE genie_ne_real;
extern GENIE_PROCEDURE genie_ne_string;
extern GENIE_PROCEDURE genie_new_sound;
extern GENIE_PROCEDURE genie_next_random;
extern GENIE_PROCEDURE genie_not_bits;
extern GENIE_PROCEDURE genie_not_bool;
extern GENIE_PROCEDURE genie_not_long_mp;
extern GENIE_PROCEDURE genie_null_char;
extern GENIE_PROCEDURE genie_odd_int;
extern GENIE_PROCEDURE genie_odd_long_mp;
extern GENIE_PROCEDURE genie_or_bits;
extern GENIE_PROCEDURE genie_or_bool;
extern GENIE_PROCEDURE genie_or_long_mp;
extern GENIE_PROCEDURE genie_overab_int;
extern GENIE_PROCEDURE genie_overab_long_mp;
extern GENIE_PROCEDURE genie_over_int;
extern GENIE_PROCEDURE genie_over_long_mp;
extern GENIE_PROCEDURE genie_pi;
extern GENIE_PROCEDURE genie_pi_long_mp;
extern GENIE_PROCEDURE genie_plusab_bytes;
extern GENIE_PROCEDURE genie_plusab_complex;
extern GENIE_PROCEDURE genie_plusab_int;
extern GENIE_PROCEDURE genie_plusab_long_bytes;
extern GENIE_PROCEDURE genie_plusab_long_complex;
extern GENIE_PROCEDURE genie_plusab_long_int;
extern GENIE_PROCEDURE genie_plusab_long_mp;
extern GENIE_PROCEDURE genie_plusab_real;
extern GENIE_PROCEDURE genie_plusab_string;
extern GENIE_PROCEDURE genie_plusto_bytes;
extern GENIE_PROCEDURE genie_plusto_long_bytes;
extern GENIE_PROCEDURE genie_plusto_string;
extern GENIE_PROCEDURE genie_pow_complex_int;
extern GENIE_PROCEDURE genie_pow_int;
extern GENIE_PROCEDURE genie_pow_long_complex_int;
extern GENIE_PROCEDURE genie_pow_long_mp;
extern GENIE_PROCEDURE genie_pow_long_mp_int;
extern GENIE_PROCEDURE genie_pow_long_mp_int_int;
extern GENIE_PROCEDURE genie_pow_real;
extern GENIE_PROCEDURE genie_pow_real_int;
extern GENIE_PROCEDURE genie_preemptive_sweep_heap;
extern GENIE_PROCEDURE genie_program_idf;
extern GENIE_PROCEDURE genie_pwd;
extern GENIE_PROCEDURE genie_real_lengths;
extern GENIE_PROCEDURE genie_real_shorths;
extern GENIE_PROCEDURE genie_real_width;
extern GENIE_PROCEDURE genie_re_complex;
extern GENIE_PROCEDURE genie_re_long_complex;
extern GENIE_PROCEDURE genie_repr_char;
extern GENIE_PROCEDURE genie_round_long_mp;
extern GENIE_PROCEDURE genie_round_real;
extern GENIE_PROCEDURE genie_seconds;
extern GENIE_PROCEDURE genie_set_bits;
extern GENIE_PROCEDURE genie_set_long_bits;
extern GENIE_PROCEDURE genie_set_longlong_bits;
extern GENIE_PROCEDURE genie_set_sound;
extern GENIE_PROCEDURE genie_shl_bits;
extern GENIE_PROCEDURE genie_shl_long_mp;
extern GENIE_PROCEDURE genie_shorten_bytes;
extern GENIE_PROCEDURE genie_shorten_long_complex_to_complex;
extern GENIE_PROCEDURE genie_shorten_longlong_complex_to_long_complex;
extern GENIE_PROCEDURE genie_shorten_longlong_mp_to_long_mp;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_bits;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_int;
extern GENIE_PROCEDURE genie_shorten_long_mp_to_real;
extern GENIE_PROCEDURE genie_shr_bits;
extern GENIE_PROCEDURE genie_shr_long_mp;
extern GENIE_PROCEDURE genie_sign_int;
extern GENIE_PROCEDURE genie_sign_long_mp;
extern GENIE_PROCEDURE genie_sign_real;
extern GENIE_PROCEDURE genie_sinh_long_mp;
extern GENIE_PROCEDURE genie_sin_long_complex;
extern GENIE_PROCEDURE genie_sin_long_mp;
extern GENIE_PROCEDURE genie_sin_real;
extern GENIE_PROCEDURE genie_small_real;
extern GENIE_PROCEDURE genie_sort_row_string;
extern GENIE_PROCEDURE genie_sound_channels;
extern GENIE_PROCEDURE genie_sound_rate;
extern GENIE_PROCEDURE genie_sound_resolution;
extern GENIE_PROCEDURE genie_sound_samples;
extern GENIE_PROCEDURE genie_sqrt_long_complex;
extern GENIE_PROCEDURE genie_sqrt_long_mp;
extern GENIE_PROCEDURE genie_sqrt_real;
extern GENIE_PROCEDURE genie_stack_pointer;
extern GENIE_PROCEDURE genie_stand_back;
extern GENIE_PROCEDURE genie_stand_back_channel;
extern GENIE_PROCEDURE genie_stand_draw_channel;
extern GENIE_PROCEDURE genie_stand_error;
extern GENIE_PROCEDURE genie_stand_error_channel;
extern GENIE_PROCEDURE genie_stand_in;
extern GENIE_PROCEDURE genie_stand_in_channel;
extern GENIE_PROCEDURE genie_stand_out;
extern GENIE_PROCEDURE genie_stand_out_channel;
extern GENIE_PROCEDURE genie_string_in_string;
extern GENIE_PROCEDURE genie_sub_complex;
extern GENIE_PROCEDURE genie_sub_int;
extern GENIE_PROCEDURE genie_sub_long_complex;
extern GENIE_PROCEDURE genie_sub_long_mp;
extern GENIE_PROCEDURE genie_sub_real;
extern GENIE_PROCEDURE genie_sweep_heap;
extern GENIE_PROCEDURE genie_system;
extern GENIE_PROCEDURE genie_system_stack_pointer;
extern GENIE_PROCEDURE genie_system_stack_size;
extern GENIE_PROCEDURE genie_tanh_long_mp;
extern GENIE_PROCEDURE genie_tan_long_complex;
extern GENIE_PROCEDURE genie_tan_long_mp;
extern GENIE_PROCEDURE genie_tan_real;
extern GENIE_PROCEDURE genie_term;
extern GENIE_PROCEDURE genie_timesab_complex;
extern GENIE_PROCEDURE genie_timesab_int;
extern GENIE_PROCEDURE genie_timesab_long_complex;
extern GENIE_PROCEDURE genie_timesab_long_int;
extern GENIE_PROCEDURE genie_timesab_long_mp;
extern GENIE_PROCEDURE genie_timesab_real;
extern GENIE_PROCEDURE genie_timesab_string;
extern GENIE_PROCEDURE genie_times_char_int;
extern GENIE_PROCEDURE genie_times_int_char;
extern GENIE_PROCEDURE genie_times_int_string;
extern GENIE_PROCEDURE genie_times_string_int;
extern GENIE_PROCEDURE genie_to_lower;
extern GENIE_PROCEDURE genie_to_upper;
extern GENIE_PROCEDURE genie_unimplemented;
extern GENIE_PROCEDURE genie_vector_times_scalar;
extern GENIE_PROCEDURE genie_xor_bits;
extern GENIE_PROCEDURE genie_xor_bool;
extern GENIE_PROCEDURE genie_xor_long_mp;

extern PROPAGATOR_T genie_and_function (NODE_T *p);
extern PROPAGATOR_T genie_assertion (NODE_T *p);
extern PROPAGATOR_T genie_assignation_constant (NODE_T *p);
extern PROPAGATOR_T genie_assignation (NODE_T *p);
extern PROPAGATOR_T genie_call (NODE_T *p);
extern PROPAGATOR_T genie_cast (NODE_T *p);
extern PROPAGATOR_T genie_closed (volatile NODE_T *p);
extern PROPAGATOR_T genie_coercion (NODE_T *p);
extern PROPAGATOR_T genie_collateral (NODE_T *p);
extern PROPAGATOR_T genie_column_function (NODE_T *p);
extern PROPAGATOR_T genie_conditional (volatile NODE_T *p);
extern PROPAGATOR_T genie_constant (NODE_T *p);
extern PROPAGATOR_T genie_denotation (NODE_T *p);
extern PROPAGATOR_T genie_deproceduring (NODE_T *p);
extern PROPAGATOR_T genie_dereference_loc_identifier (NODE_T *p);
extern PROPAGATOR_T genie_dereference_selection_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_dereference_slice_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_dereferencing (NODE_T *p);
extern PROPAGATOR_T genie_dereferencing_quick (NODE_T *p);
extern PROPAGATOR_T genie_diagonal_function (NODE_T *p);
extern PROPAGATOR_T genie_dyadic (NODE_T *p);
extern PROPAGATOR_T genie_dyadic_quick (NODE_T *p);
extern PROPAGATOR_T genie_enclosed (volatile NODE_T *p);
extern PROPAGATOR_T genie_format_text (NODE_T *p);
extern PROPAGATOR_T genie_formula_div_real (NODE_T *p);
extern PROPAGATOR_T genie_formula_eq_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_eq_real (NODE_T *p);
extern PROPAGATOR_T genie_formula_ge_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_ge_real (NODE_T *p);
extern PROPAGATOR_T genie_formula_gt_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_gt_real (NODE_T *p);
extern PROPAGATOR_T genie_formula_le_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_le_real (NODE_T *p);
extern PROPAGATOR_T genie_formula_lt_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_lt_real (NODE_T *p);
extern PROPAGATOR_T genie_formula_minus_int_constant (NODE_T *p);
extern PROPAGATOR_T genie_formula_minus_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_minus_real (NODE_T *p);
extern PROPAGATOR_T genie_formula_ne_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_ne_real (NODE_T *p);
extern PROPAGATOR_T genie_formula (NODE_T *p);
extern PROPAGATOR_T genie_formula_over_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_plus_int_constant (NODE_T *p);
extern PROPAGATOR_T genie_formula_plus_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_plus_real (NODE_T *p);
extern PROPAGATOR_T genie_formula_times_int (NODE_T *p);
extern PROPAGATOR_T genie_formula_times_real (NODE_T *p);
extern PROPAGATOR_T genie_generator (NODE_T *p);
extern PROPAGATOR_T genie_identifier (NODE_T *p);
extern PROPAGATOR_T genie_identifier_standenv (NODE_T *p);
extern PROPAGATOR_T genie_identifier_standenv_proc (NODE_T *p);
extern PROPAGATOR_T genie_identity_relation_is_nil (NODE_T *p);
extern PROPAGATOR_T genie_identity_relation_isnt_nil (NODE_T *p);
extern PROPAGATOR_T genie_identity_relation (NODE_T *p);
extern PROPAGATOR_T genie_int_case (volatile NODE_T *p);
extern PROPAGATOR_T genie_loc_identifier (NODE_T *p);
extern PROPAGATOR_T genie_loop (volatile NODE_T *p);
extern PROPAGATOR_T genie_monadic (NODE_T *p);
extern PROPAGATOR_T genie_nihil (NODE_T *p);
extern PROPAGATOR_T genie_or_function (NODE_T *p);
extern PROPAGATOR_T genie_routine_text (NODE_T *p);
extern PROPAGATOR_T genie_row_function (NODE_T *p);
extern PROPAGATOR_T genie_rowing (NODE_T *p);
extern PROPAGATOR_T genie_rowing_ref_row_of_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_ref_row_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_row_of_row (NODE_T *p);
extern PROPAGATOR_T genie_rowing_row_row (NODE_T *p);
extern PROPAGATOR_T genie_selection_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_selection (NODE_T *p);
extern PROPAGATOR_T genie_selection_value_quick (NODE_T *p);
extern PROPAGATOR_T genie_skip (NODE_T *p);
extern PROPAGATOR_T genie_slice_name_quick (NODE_T *p);
extern PROPAGATOR_T genie_slice (NODE_T *p);
extern PROPAGATOR_T genie_transpose_function (NODE_T *p);
extern PROPAGATOR_T genie_united_case (volatile NODE_T *p);
extern PROPAGATOR_T genie_uniting (NODE_T *p);
extern PROPAGATOR_T genie_unit (NODE_T *p);
extern PROPAGATOR_T genie_voiding_assignation_constant (NODE_T *p);
extern PROPAGATOR_T genie_voiding_assignation (NODE_T *p);
extern PROPAGATOR_T genie_voiding (NODE_T *p);
extern PROPAGATOR_T genie_widening_int_to_real (NODE_T *p);
extern PROPAGATOR_T genie_widening (NODE_T *p);

extern A68_REF c_string_to_row_char (NODE_T *, char *, int);
extern A68_REF c_to_a_string (NODE_T *, char *);
extern A68_REF empty_row (NODE_T *, MOID_T *);
extern A68_REF empty_string (NODE_T *);
extern A68_REF genie_allocate_declarer (NODE_T *, ADDR_T *, A68_REF, BOOL_T);
extern A68_REF genie_assign_stowed (A68_REF, A68_REF *, NODE_T *, MOID_T *);
extern A68_REF genie_concatenate_rows (NODE_T *, MOID_T *, int, ADDR_T);
extern A68_REF genie_copy_stowed (A68_REF, NODE_T *, MOID_T *);
extern A68_REF genie_make_ref_row_of_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
extern A68_REF genie_make_ref_row_row (NODE_T *, MOID_T *, MOID_T *, ADDR_T);
extern A68_REF genie_make_row (NODE_T *, MOID_T *, int, ADDR_T);
extern BOOL_T close_device (NODE_T *, A68_FILE *);
extern BOOL_T genie_int_case_unit (NODE_T *, int, int *);
extern BOOL_T genie_no_user_symbols (SYMBOL_TABLE_T *);
extern BOOL_T genie_string_to_value_internal (NODE_T *, MOID_T *, char *, BYTE_T *);
extern BOOL_T genie_united_case_unit (NODE_T *, MOID_T *);
extern char *a_to_c_string (NODE_T *, char *, A68_REF);
extern char *genie_standard_format (NODE_T *, MOID_T *, void *);
extern double dabs (double);
extern double dmod (double, double);
extern double dsign (double);
extern double rng_53_bit (void);
extern int a68_string_size (NODE_T *, A68_REF);
extern int iabs (int);
extern int isign (int);
extern NODE_T *last_unit;
extern void change_breakpoints (NODE_T *, unsigned, int, BOOL_T *, char *);
extern void change_masks (NODE_T *, unsigned, BOOL_T);
extern void dump_frame (int);
extern void dump_stack (int);
extern void exit_genie (NODE_T *, int);
extern void free_genie_heap (NODE_T *);
extern void genie_call_operator (NODE_T *, ADDR_T);
extern void genie_call_procedure (NODE_T *, MOID_T *, MOID_T *, MOID_T *, A68_PROCEDURE *, ADDR_T, ADDR_T);
extern void genie_check_initialisation (NODE_T *, BYTE_T *, MOID_T *);
extern void genie_copy_sound (NODE_T *, BYTE_T *, BYTE_T *);
extern void genie_declaration (NODE_T *);
extern void genie_dump_frames ();
extern void genie_enquiry_clause (NODE_T *);
extern void genie_f_and_becomes (NODE_T *, MOID_T *, GENIE_PROCEDURE *);
extern void genie_generator_bounds (NODE_T *);
extern void genie_generator_internal (NODE_T *, MOID_T *, TAG_T *, LEAP_T, ADDR_T);
extern void genie_init_lib (NODE_T *);
extern void genie (MODULE_T *);
extern void genie_prepare_declarer (NODE_T *);
extern void genie_push_undefined (NODE_T *, MOID_T *);
extern void genie_serial_clause (NODE_T *, jmp_buf *);
extern void genie_serial_units (NODE_T *, NODE_T **, jmp_buf *, int);
extern void genie_subscript_linear (NODE_T *, ADDR_T *, int *);
extern void genie_subscript (NODE_T *, A68_TUPLE **, int *, NODE_T **);
extern void initialise_frame (NODE_T *);
extern void init_rng (unsigned long);
extern void install_signal_handlers (void);
extern void show_data_item (FILE_T, NODE_T *, MOID_T *, BYTE_T *);
extern void show_frame (NODE_T *, FILE_T);
extern void single_step (NODE_T *, unsigned);
extern void stack_dump (FILE_T, ADDR_T, int, int *);
extern void stack_dump_light (ADDR_T);
extern void un_init_frame (NODE_T *);
extern void where (FILE_T, NODE_T *);

extern void genie_argc (NODE_T *);
extern void genie_argv (NODE_T *);
extern void genie_create_pipe (NODE_T *);
extern void genie_utctime (NODE_T *);
extern void genie_localtime (NODE_T *);
extern void genie_errno (NODE_T *);
extern void genie_execve (NODE_T *);
extern void genie_execve_child (NODE_T *);
extern void genie_execve_child_pipe (NODE_T *);
extern void genie_execve_output (NODE_T *);
extern void genie_fork (NODE_T *);
extern void genie_getenv (NODE_T *);
extern void genie_reset_errno (NODE_T *);
extern void genie_strerror (NODE_T *);
extern void genie_waitpid (NODE_T *);

#ifdef __S_IFIFO
extern GENIE_PROCEDURE genie_file_is_fifo;
#endif

#ifdef __S_IFLNK
extern GENIE_PROCEDURE genie_file_is_link;
#endif

#if defined ENABLE_PAR_CLAUSE
extern pthread_t main_thread_id;

extern BOOL_T whether_main_thread (void);
extern GENIE_PROCEDURE genie_down_sema;
extern GENIE_PROCEDURE genie_level_int_sema;
extern GENIE_PROCEDURE genie_level_sema_int;
extern GENIE_PROCEDURE genie_up_sema;
extern PROPAGATOR_T genie_parallel (NODE_T *p);
extern void count_par_clauses (NODE_T *);
extern void genie_abend_all_threads (NODE_T *, jmp_buf *, NODE_T *);
extern void genie_set_exit_from_threads (int);
#endif

#if defined ENABLE_HTTP
extern void genie_http_content (NODE_T *);
extern void genie_tcp_request (NODE_T *);
#endif

#if defined ENABLE_REGEX
extern void genie_grep_in_string (NODE_T *);
extern void genie_sub_in_string (NODE_T *);
#endif

#if defined ENABLE_CURSES
extern BOOL_T curses_active;
extern void genie_curses_clear (NODE_T *);
extern void genie_curses_columns (NODE_T *);
extern void genie_curses_end (NODE_T *);
extern void genie_curses_getchar (NODE_T *);
extern void genie_curses_lines (NODE_T *);
extern void genie_curses_move (NODE_T *);
extern void genie_curses_putchar (NODE_T *);
extern void genie_curses_refresh (NODE_T *);
extern void genie_curses_start (NODE_T *);
#endif

#if defined ENABLE_POSTGRESQL
extern void genie_pq_backendpid (NODE_T *);
extern void genie_pq_cmdstatus (NODE_T *);
extern void genie_pq_cmdtuples (NODE_T *);
extern void genie_pq_connectdb (NODE_T *);
extern void genie_pq_db (NODE_T *);
extern void genie_pq_errormessage (NODE_T *);
extern void genie_pq_exec (NODE_T *);
extern void genie_pq_fformat (NODE_T *);
extern void genie_pq_finish (NODE_T *);
extern void genie_pq_fname (NODE_T *);
extern void genie_pq_fnumber (NODE_T *);
extern void genie_pq_getisnull (NODE_T *);
extern void genie_pq_getvalue (NODE_T *);
extern void genie_pq_host (NODE_T *);
extern void genie_pq_nfields (NODE_T *);
extern void genie_pq_ntuples (NODE_T *);
extern void genie_pq_options (NODE_T *);
extern void genie_pq_parameterstatus (NODE_T *);
extern void genie_pq_pass (NODE_T *);
extern void genie_pq_port (NODE_T *);
extern void genie_pq_protocolversion (NODE_T *);
extern void genie_pq_reset (NODE_T *);
extern void genie_pq_resulterrormessage (NODE_T *);
extern void genie_pq_serverversion (NODE_T *);
extern void genie_pq_socket (NODE_T *);
extern void genie_pq_tty (NODE_T *);
extern void genie_pq_user (NODE_T *);
#endif

#endif
