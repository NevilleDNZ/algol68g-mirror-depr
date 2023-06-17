//! @file a68g-prelude.h
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

#if !defined (__A68G_PRELUDE_H__)
#define __A68G_PRELUDE_H__

#define A68_STD A68_TRUE
#define A68_EXT A68_FALSE

extern MOID_T *a68_proc (MOID_T *, ...);
extern void a68_idf (BOOL_T, char *, MOID_T *, GPROC *);
extern void a68_prio (char *, int);
extern void a68_op (BOOL_T, char *, MOID_T *, GPROC *);
extern void a68_mode (int, char *, MOID_T **);

// ALGOL68C type procs.

#define A68C_DEFIO(name, pname, mode) {\
  m = a68_proc (MODE (mode), M_REF_FILE, NO_MOID);\
  a68_idf (A68_EXT, "get" #name, m, genie_get_##pname);\
  m = a68_proc (M_VOID, M_REF_FILE, MODE (mode), NO_MOID);\
  a68_idf (A68_EXT, "put" #name, m, genie_put_##pname);\
  m = a68_proc (MODE (mode), NO_MOID);\
  a68_idf (A68_EXT, "read" #name, m, genie_read_##pname);\
  m = a68_proc (M_VOID, MODE (mode), NO_MOID);\
  a68_idf (A68_EXT, "print" #name, m, genie_print_##pname);\
}

#define IS_NL_FF(ch) ((ch) == NEWLINE_CHAR || (ch) == FORMFEED_CHAR)

#define A68_MONAD(n, MODE, OP)\
void n (NODE_T * p) {\
  MODE *i;\
  POP_OPERAND_ADDRESS (p, i, MODE);\
  VALUE (i) = OP (VALUE (i));\
}

#if (A68_LEVEL >= 3)
extern GPROC genie_lt_bits;
extern GPROC genie_gt_bits;
extern DOUBLE_T mp_to_double (NODE_T *, MP_T *, int);
extern MP_T *double_to_mp (NODE_T *, MP_T *, DOUBLE_T, int);
#endif

extern A68_REF c_string_to_row_char (NODE_T *, char *, int);
extern A68_REF c_to_a_string (NODE_T *, char *, int);
extern A68_REF empty_row (NODE_T *, MOID_T *);
extern A68_REF empty_string (NODE_T *);
extern A68_REF genie_make_row (NODE_T *, MOID_T *, int, ADDR_T);
extern A68_REF genie_store (NODE_T *, MOID_T *, A68_REF *, A68_REF *);
extern A68_REF heap_generator (NODE_T *, MOID_T *, int);
extern A68_REF tmp_to_a68_string (NODE_T *, char *);
extern ADDR_T calculate_internal_index (A68_TUPLE *, int);
extern BOOL_T close_device (NODE_T *, A68_FILE *);
extern BOOL_T genie_int_case_unit (NODE_T *, int, int *);
extern BOOL_T increment_internal_index (A68_TUPLE *, int);
extern char *a_to_c_string (NODE_T *, char *, A68_REF);
extern char *propagator_name (PROP_PROC * p);
extern FILE *a68_fopen (char *, char *, char *);
extern GPROC get_global_level;
extern GPROC initialise_frame;
extern int a68_finite (REAL_T);
extern int a68_isinf (REAL_T);
extern int a68_isnan (REAL_T);
extern int a68_string_size (NODE_T *, A68_REF);
extern int char_value (int);
extern int grep_in_string (char *, char *, int *, int *);
extern INT_T a68_round (REAL_T);
extern PROP_T genie_generator (NODE_T *);
extern REAL_T seconds (void);
extern REAL_T ten_up (int);
extern ssize_t io_read_conv (FILE_T, void *, size_t);
extern ssize_t io_read (FILE_T, void *, size_t);
extern ssize_t io_write_conv (FILE_T, const void *, size_t);
extern ssize_t io_write (FILE_T, const void *, size_t);
extern unt heap_available (void);
extern void a68_div_complex (A68_REAL *, A68_REAL *, A68_REAL *);
extern void a68_exit (int);
extern void a68_exp_complex (A68_REAL *, A68_REAL *);
extern void change_breakpoints (NODE_T *, unt, int, BOOL_T *, char *);
extern void change_masks (NODE_T *, unt, BOOL_T);
extern void colour_object (BYTE_T *, MOID_T *);
extern void deltagammainc (REAL_T *, REAL_T *, REAL_T, REAL_T, REAL_T, REAL_T);
extern void exit_genie (NODE_T *, int);
extern void gc_heap (NODE_T *, ADDR_T);
extern void genie_call_event_routine (NODE_T *, MOID_T *, A68_PROCEDURE *, ADDR_T, ADDR_T);
extern void genie_call_operator (NODE_T *, ADDR_T);
extern void genie_call_procedure (NODE_T *, MOID_T *, MOID_T *, MOID_T *, A68_PROCEDURE *, ADDR_T, ADDR_T);
extern void genie_check_initialisation (NODE_T *, BYTE_T *, MOID_T *);
extern void genie_f_and_becomes (NODE_T *, MOID_T *, GPROC *);
extern void genie_find_proc_op (NODE_T *, int *);
extern void genie_free (NODE_T *);
extern void genie_generator_internal (NODE_T *, MOID_T *, TAG_T *, LEAP_T, ADDR_T);
extern void genie_generator_stowed (NODE_T *, BYTE_T *, NODE_T **, ADDR_T *);
extern void genie_init_rng (void);
extern void genie_preprocess (NODE_T *, int *, void *);
extern void genie_push_undefined (NODE_T *, MOID_T *);
extern void genie_read_standard (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_serial_clause (NODE_T *, jmp_buf *);
extern void genie_serial_units (NODE_T *, NODE_T **, jmp_buf *, ADDR_T);
extern void genie_string_to_value (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_subscript (NODE_T *, A68_TUPLE **, INT_T *, NODE_T **);
extern void genie_value_to_string (NODE_T *, MOID_T *, BYTE_T *, int);
extern void genie_variable_dec (NODE_T *, NODE_T **, ADDR_T);
extern void genie (void *);
extern void genie_write_standard (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void initialise_internal_index (A68_TUPLE *, int);
extern void io_close_tty_line (void);
extern void io_write_string (FILE_T, const char *);
extern void monitor_error (char *, char *);
extern void mp_strtou (NODE_T *, MP_T *, char *, MOID_T *);
extern void print_internal_index (FILE_T, A68_TUPLE *, int);
extern void print_item (NODE_T *, FILE_T, BYTE_T *, MOID_T *);
extern void single_step (NODE_T *, unt);
extern void skip_nl_ff (NODE_T *, int *, A68_REF);
extern void stack_dump (FILE_T, ADDR_T, int, int *);
extern void value_sign_error (NODE_T *, MOID_T *, A68_REF);
extern void where_in_source (FILE_T, NODE_T *);

// Standard prelude RTS

extern GPROC genie_a68_argc;
extern GPROC genie_a68_argv;
extern GPROC genie_abend;
extern GPROC genie_abs_bits;
extern GPROC genie_abs_bool;
extern GPROC genie_abs_char;
extern GPROC genie_abs_complex;
extern GPROC genie_abs_int;
extern GPROC genie_abs_real;
extern GPROC genie_acos_complex;
extern GPROC genie_acosdg_real;
extern GPROC genie_acosh_complex;
extern GPROC genie_acosh_real;
extern GPROC genie_acos_real;
extern GPROC genie_acotdg_real;
extern GPROC genie_acot_real;
extern GPROC genie_acsc_real;
extern GPROC genie_add_bits;
extern GPROC genie_add_bytes;
extern GPROC genie_add_char;
extern GPROC genie_add_complex;
extern GPROC genie_add_int;
extern GPROC genie_add_long_bytes;
extern GPROC genie_add_mp_int;
extern GPROC genie_add_real;
extern GPROC genie_add_string;
extern GPROC genie_and_bits;
extern GPROC genie_and_bool;
extern GPROC genie_argc;
extern GPROC genie_arg_complex;
extern GPROC genie_argv;
extern GPROC genie_asec_real;
extern GPROC genie_asin_complex;
extern GPROC genie_asindg_real;
extern GPROC genie_asinh_complex;
extern GPROC genie_asinh_real;
extern GPROC genie_asin_real;
extern GPROC genie_associate;
extern GPROC genie_atan2dg_real;
extern GPROC genie_atan2_real;
extern GPROC genie_atan_complex;
extern GPROC genie_atandg_real;
extern GPROC genie_atanh_complex;
extern GPROC genie_atanh_real;
extern GPROC genie_atan_real;
extern GPROC genie_backspace;
extern GPROC genie_backtrace;
extern GPROC genie_beta_inc_cf_real;
extern GPROC genie_beta_real;
extern GPROC genie_bin_int;
extern GPROC genie_bin_possible;
extern GPROC genie_bits;
extern GPROC genie_bits_lengths;
extern GPROC genie_bits_pack;
extern GPROC genie_bits_shorths;
extern GPROC genie_bits_width;
extern GPROC genie_blank_char;
extern GPROC genie_block;
extern GPROC genie_break;
extern GPROC genie_bytes_lengths;
extern GPROC genie_bytespack;
extern GPROC genie_bytes_shorths;
extern GPROC genie_bytes_width;
extern GPROC genie_cd;
extern GPROC genie_char_in_string;
extern GPROC genie_choose_real;
extern GPROC genie_clear_bits;
extern GPROC genie_clear_long_bits;
extern GPROC genie_close;
extern GPROC genie_columns;
extern GPROC genie_complex_lengths;
extern GPROC genie_complex_shorths;
extern GPROC genie_compressible;
extern GPROC genie_conj_complex;
extern GPROC genie_cos_complex;
extern GPROC genie_cosdg_real;
extern GPROC genie_cosh_complex;
extern GPROC genie_cosh_real;
extern GPROC genie_cospi_real;
extern GPROC genie_cos_real;
extern GPROC genie_cotdg_real;
extern GPROC genie_cotpi_real;
extern GPROC genie_cot_real;
extern GPROC genie_cputime;
extern GPROC genie_create;
extern GPROC genie_create_pipe;
extern GPROC genie_csc_real;
extern GPROC genie_curt_real;
extern GPROC genie_debug;
extern GPROC genie_declaration;
extern GPROC genie_directory;
extern GPROC genie_divab_complex;
extern GPROC genie_divab_real;
extern GPROC genie_div_complex;
extern GPROC genie_div_int;
extern GPROC genie_div_real;
extern GPROC genie_draw_possible;
extern GPROC genie_dyad_elems;
extern GPROC genie_dyad_lwb;
extern GPROC genie_dyad_upb;
extern GPROC genie_elem_bits;
extern GPROC genie_elem_bytes;
extern GPROC genie_elem_long_bits;
extern GPROC genie_elem_long_bits;
extern GPROC genie_elem_long_bytes;
extern GPROC genie_elem_string;
extern GPROC genie_enquiry_clause;
extern GPROC genie_entier_real;
extern GPROC genie_eof;
extern GPROC genie_eoln;
extern GPROC genie_eq_bits;
extern GPROC genie_eq_bool;
extern GPROC genie_eq_bytes;
extern GPROC genie_eq_char;
extern GPROC genie_eq_complex;
extern GPROC genie_eq_int;
extern GPROC genie_eq_long_bytes;
extern GPROC genie_eq_real;
extern GPROC genie_eq_string;
extern GPROC genie_erase;
extern GPROC genie_erfc_real;
extern GPROC genie_erf_real;
extern GPROC genie_errno;
extern GPROC genie_error_char;
extern GPROC genie_establish;
extern GPROC genie_evaluate;
extern GPROC genie_exec;
extern GPROC genie_exec_sub;
extern GPROC genie_exec_sub_output;
extern GPROC genie_exec_sub_pipeline;
extern GPROC genie_exp_char;
extern GPROC genie_exp_complex;
extern GPROC genie_exp_real;
extern GPROC genie_exp_width;
extern GPROC genie_fact_real;
extern GPROC genie_file_is_block_device;
extern GPROC genie_file_is_char_device;
extern GPROC genie_file_is_directory;
extern GPROC genie_file_is_regular;
extern GPROC genie_file_mode;
extern GPROC genie_first_random;
extern GPROC genie_flip_char;
extern GPROC genie_flop_char;
extern GPROC genie_fork;
extern GPROC genie_formfeed_char;
extern GPROC genie_gamma_inc_f_real;
extern GPROC genie_gamma_inc_gf_real;
extern GPROC genie_gamma_inc_g_real;
extern GPROC genie_gamma_inc_h_real;
extern GPROC genie_gamma_real;
extern GPROC genie_garbage_collections;
extern GPROC genie_garbage_freed;
extern GPROC genie_garbage_refused;
extern GPROC genie_garbage_seconds;
extern GPROC genie_gc_heap;
extern GPROC genie_ge_bits;
extern GPROC genie_ge_bytes;
extern GPROC genie_ge_char;
extern GPROC genie_ge_int;
extern GPROC genie_ge_long_bits;
extern GPROC genie_ge_long_bytes;
extern GPROC genie_generator_bounds;
extern GPROC genie_ge_real;
extern GPROC genie_ge_string;
extern GPROC genie_get_bits;
extern GPROC genie_get_bool;
extern GPROC genie_get_char;
extern GPROC genie_get_complex;
extern GPROC genie_getenv;
extern GPROC genie_get_int;
extern GPROC genie_get_long_bits;
extern GPROC genie_get_long_int;
extern GPROC genie_get_long_real;
extern GPROC genie_get_possible;
extern GPROC genie_get_real;
extern GPROC genie_get_sound;
extern GPROC genie_get_string;
extern GPROC genie_grep_in_string;
extern GPROC genie_grep_in_substring;
extern GPROC genie_gt_bytes;
extern GPROC genie_gt_char;
extern GPROC genie_gt_int;
extern GPROC genie_gt_long_bytes;
extern GPROC genie_gt_real;
extern GPROC genie_gt_string;
extern GPROC genie_i_complex;
extern GPROC genie_identity_dec;
extern GPROC genie_idf;
extern GPROC genie_idle;
extern GPROC genie_i_int_complex;
extern GPROC genie_im_complex;
extern GPROC genie_infinity_real;
extern GPROC genie_init_heap;
extern GPROC genie_init_transput;
extern GPROC genie_int_lengths;
extern GPROC genie_int_shorths;
extern GPROC genie_int_width;
extern GPROC genie_inverfc_real;
extern GPROC genie_inverf_real;
extern GPROC genie_is_alnum;
extern GPROC genie_is_alpha;
extern GPROC genie_is_cntrl;
extern GPROC genie_is_digit;
extern GPROC genie_is_graph;
extern GPROC genie_is_lower;
extern GPROC genie_is_print;
extern GPROC genie_is_punct;
extern GPROC genie_is_space;
extern GPROC genie_is_upper;
extern GPROC genie_is_xdigit;
extern GPROC genie_last_char_in_string;
extern GPROC genie_le_bits;
extern GPROC genie_le_bytes;
extern GPROC genie_le_char;
extern GPROC genie_le_int;
extern GPROC genie_le_long_bits;
extern GPROC genie_le_long_bytes;
extern GPROC genie_leng_bytes;
extern GPROC genie_lengthen_long_bits_to_row_bool;
extern GPROC genie_le_real;
extern GPROC genie_le_string;
extern GPROC genie_lj_e_12_6;
extern GPROC genie_lj_f_12_6;
extern GPROC genie_ln1p_real;
extern GPROC genie_ln_beta_real;
extern GPROC genie_ln_choose_real;
extern GPROC genie_ln_complex;
extern GPROC genie_ln_fact_real;
extern GPROC genie_ln_gamma_real;
extern GPROC genie_ln_real;
extern GPROC genie_localtime;
extern GPROC genie_lock;
extern GPROC genie_log_real;
extern GPROC genie_long_bits_pack;
extern GPROC genie_long_bits_width;
extern GPROC genie_long_bytespack;
extern GPROC genie_long_bytes_width;
extern GPROC genie_long_exp_width;
extern GPROC genie_long_int_width;
extern GPROC genie_long_max_bits;
extern GPROC genie_long_max_int;
extern GPROC genie_long_max_real;
extern GPROC genie_long_min_real;
extern GPROC genie_long_next_random;
extern GPROC genie_long_real_width;
extern GPROC genie_long_small_real;
extern GPROC genie_lt_bytes;
extern GPROC genie_lt_char;
extern GPROC genie_lt_int;
extern GPROC genie_lt_long_bytes;
extern GPROC genie_lt_real;
extern GPROC genie_lt_string;
extern GPROC genie_make_term;
extern GPROC genie_max_abs_char;
extern GPROC genie_max_bits;
extern GPROC genie_max_int;
extern GPROC genie_max_real;
extern GPROC genie_min_real;
extern GPROC genie_minusab_complex;
extern GPROC genie_minusab_int;
extern GPROC genie_minusab_mp_int;
extern GPROC genie_minusab_real;
extern GPROC genie_minus_complex;
extern GPROC genie_minus_infinity_real;
extern GPROC genie_minus_int;
extern GPROC genie_minus_real;
extern GPROC genie_modab_int;
extern GPROC genie_mod_bits;
extern GPROC genie_mod_int;
extern GPROC genie_monad_elems;
extern GPROC genie_monad_lwb;
extern GPROC genie_monad_upb;
extern GPROC genie_mp_radix;
extern GPROC genie_mul_complex;
extern GPROC genie_mul_int;
extern GPROC genie_mul_mp_int;
extern GPROC genie_mul_real;
extern GPROC genie_ne_bits;
extern GPROC genie_ne_bool;
extern GPROC genie_ne_bytes;
extern GPROC genie_ne_char;
extern GPROC genie_ne_complex;
extern GPROC genie_ne_int;
extern GPROC genie_ne_long_bytes;
extern GPROC genie_ne_real;
extern GPROC genie_ne_string;
extern GPROC genie_new_line;
extern GPROC genie_newline_char;
extern GPROC genie_new_page;
extern GPROC genie_new_sound;
extern GPROC genie_next_random;
extern GPROC genie_next_rnd;
extern GPROC genie_not_bits;
extern GPROC genie_not_bool;
extern GPROC genie_null_char;
extern GPROC genie_odd_int;
extern GPROC genie_on_file_end;
extern GPROC genie_on_format_end;
extern GPROC genie_on_format_error;
extern GPROC genie_on_gc_event;
extern GPROC genie_on_line_end;
extern GPROC genie_on_open_error;
extern GPROC genie_on_page_end;
extern GPROC genie_on_transput_error;
extern GPROC genie_on_value_error;
extern GPROC genie_open;
extern GPROC genie_operator_dec;
extern GPROC genie_or_bits;
extern GPROC genie_or_bool;
extern GPROC genie_overab_int;
extern GPROC genie_over_bits;
extern GPROC genie_over_int;
extern GPROC genie_pi;
extern GPROC genie_plusab_bytes;
extern GPROC genie_plusab_complex;
extern GPROC genie_plusab_int;
extern GPROC genie_plusab_long_bytes;
extern GPROC genie_plusab_mp_int;
extern GPROC genie_plusab_real;
extern GPROC genie_plusab_string;
extern GPROC genie_plusto_bytes;
extern GPROC genie_plusto_long_bytes;
extern GPROC genie_plusto_string;
extern GPROC genie_pow_complex_int;
extern GPROC genie_pow_int;
extern GPROC genie_pow_real;
extern GPROC genie_pow_real_int;
extern GPROC genie_preemptive_gc_heap;
extern GPROC genie_print_bits;
extern GPROC genie_print_bool;
extern GPROC genie_print_char;
extern GPROC genie_print_complex;
extern GPROC genie_print_int;
extern GPROC genie_print_long_bits;
extern GPROC genie_print_long_int;
extern GPROC genie_print_long_real;
extern GPROC genie_print_real;
extern GPROC genie_print_string;
extern GPROC genie_print_string;
extern GPROC genie_proc_variable_dec;
extern GPROC genie_program_idf;
extern GPROC genie_put_bits;
extern GPROC genie_put_bool;
extern GPROC genie_put_char;
extern GPROC genie_put_complex;
extern GPROC genie_put_int;
extern GPROC genie_put_long_bits;
extern GPROC genie_put_long_int;
extern GPROC genie_put_long_real;
extern GPROC genie_put_possible;
extern GPROC genie_put_real;
extern GPROC genie_put_string;
extern GPROC genie_pwd;
extern GPROC genie_read;
extern GPROC genie_read_bin;
extern GPROC genie_read_bin_file;
extern GPROC genie_read_bits;
extern GPROC genie_read_bool;
extern GPROC genie_read_char;
extern GPROC genie_read_complex;
extern GPROC genie_read_file;
extern GPROC genie_read_file_format;
extern GPROC genie_read_format;
extern GPROC genie_read_int;
extern GPROC genie_read_line;
extern GPROC genie_read_long_bits;
extern GPROC genie_read_long_int;
extern GPROC genie_read_long_real;
extern GPROC genie_read_real;
extern GPROC genie_read_string;
extern GPROC genie_real_lengths;
extern GPROC genie_realpath;
extern GPROC genie_real_shorths;
extern GPROC genie_real_width;
extern GPROC genie_re_complex;
extern GPROC genie_reidf_possible;
extern GPROC genie_repr_char;
extern GPROC genie_reset;
extern GPROC genie_reset_errno;
extern GPROC genie_reset_possible;
extern GPROC genie_rol_bits;
extern GPROC genie_ror_bits;
extern GPROC genie_round_real;
extern GPROC genie_rows;
extern GPROC genie_sec_real;
extern GPROC genie_set;
extern GPROC genie_set_bits;
extern GPROC genie_set_long_bits;
extern GPROC genie_set_possible;
extern GPROC genie_set_return_code;
extern GPROC genie_set_sound;
extern GPROC genie_shl_bits;
extern GPROC genie_shorten_bytes;
extern GPROC genie_shr_bits;
extern GPROC genie_sign_int;
extern GPROC genie_sign_real;
extern GPROC genie_sin_complex;
extern GPROC genie_sindg_real;
extern GPROC genie_sinh_complex;
extern GPROC genie_sinh_real;
extern GPROC genie_sinpi_real;
extern GPROC genie_sin_real;
extern GPROC genie_sleep;
extern GPROC genie_small_real;
extern GPROC genie_sort_row_string;
extern GPROC genie_sound_channels;
extern GPROC genie_sound_rate;
extern GPROC genie_sound_resolution;
extern GPROC genie_sound_samples;
extern GPROC genie_space;
extern GPROC genie_sqrt_complex;
extern GPROC genie_sqrt_real;
extern GPROC genie_stack_pointer;
extern GPROC genie_stand_back;
extern GPROC genie_stand_back_channel;
extern GPROC genie_stand_draw_channel;
extern GPROC genie_stand_error;
extern GPROC genie_stand_error_channel;
extern GPROC genie_stand_in;
extern GPROC genie_stand_in_channel;
extern GPROC genie_stand_out;
extern GPROC genie_stand_out_channel;
extern GPROC genie_strerror;
extern GPROC genie_string_in_string;
extern GPROC genie_sub_bits;
extern GPROC genie_sub_complex;
extern GPROC genie_sub_in_string;
extern GPROC genie_sub_int;
extern GPROC genie_sub_mp_int;
extern GPROC genie_sub_real;
extern GPROC genie_system;
extern GPROC genie_system_heap_pointer;
extern GPROC genie_system_stack_pointer;
extern GPROC genie_system_stack_size;
extern GPROC genie_tab_char;
extern GPROC genie_tan_complex;
extern GPROC genie_tandg_real;
extern GPROC genie_tanh_complex;
extern GPROC genie_tanh_real;
extern GPROC genie_tanpi_real;
extern GPROC genie_tan_real;
extern GPROC genie_term;
extern GPROC genie_timesab_complex;
extern GPROC genie_timesab_int;
extern GPROC genie_timesab_mp_int;
extern GPROC genie_timesab_real;
extern GPROC genie_timesab_string;
extern GPROC genie_times_bits;
extern GPROC genie_times_char_int;
extern GPROC genie_times_int_char;
extern GPROC genie_times_int_string;
extern GPROC genie_times_string_int;
extern GPROC genie_to_lower;
extern GPROC genie_to_upper;
extern GPROC genie_unimplemented;
extern GPROC genie_utctime;
extern GPROC genie_waitpid;
extern GPROC genie_whole;
extern GPROC genie_write;
extern GPROC genie_write_bin;
extern GPROC genie_write_bin_file;
extern GPROC genie_write_file;
extern GPROC genie_write_file_format;
extern GPROC genie_write_format;
extern GPROC genie_xor_bits;
extern GPROC genie_xor_bool;

#if defined (S_ISFIFO)
extern GPROC genie_file_is_fifo;
#endif

#if defined (S_ISLNK)
extern GPROC genie_file_is_link;
#endif

#if defined (BUILD_PARALLEL_CLAUSE)
extern GPROC genie_down_sema;
extern GPROC genie_level_int_sema;
extern GPROC genie_level_sema_int;
extern GPROC genie_up_sema;
#endif

#if defined (BUILD_HTTP)
extern GPROC genie_http_content;
extern GPROC genie_tcp_request;
#endif

#if defined (HAVE_CURSES)
extern GPROC genie_curses_clear;
extern GPROC genie_curses_del_char;
extern GPROC genie_curses_green;
extern GPROC genie_curses_cyan;
extern GPROC genie_curses_white;
extern GPROC genie_curses_red;
extern GPROC genie_curses_yellow;
extern GPROC genie_curses_magenta;
extern GPROC genie_curses_blue;
extern GPROC genie_curses_green_inverse;
extern GPROC genie_curses_cyan_inverse;
extern GPROC genie_curses_white_inverse;
extern GPROC genie_curses_red_inverse;
extern GPROC genie_curses_yellow_inverse;
extern GPROC genie_curses_magenta_inverse;
extern GPROC genie_curses_blue_inverse;
extern GPROC genie_curses_columns;
extern GPROC genie_curses_end;
extern GPROC genie_curses_getchar;
extern GPROC genie_curses_lines;
extern GPROC genie_curses_move;
extern GPROC genie_curses_putchar;
extern GPROC genie_curses_refresh;
extern GPROC genie_curses_start;
#endif

#if defined (HAVE_POSTGRESQL)
extern GPROC genie_pq_backendpid;
extern GPROC genie_pq_cmdstatus;
extern GPROC genie_pq_cmdtuples;
extern GPROC genie_pq_connectdb;
extern GPROC genie_pq_db;
extern GPROC genie_pq_errormessage;
extern GPROC genie_pq_exec;
extern GPROC genie_pq_fformat;
extern GPROC genie_pq_finish;
extern GPROC genie_pq_fname;
extern GPROC genie_pq_fnumber;
extern GPROC genie_pq_getisnull;
extern GPROC genie_pq_getvalue;
extern GPROC genie_pq_host;
extern GPROC genie_pq_nfields;
extern GPROC genie_pq_ntuples;
extern GPROC genie_pq_options;
extern GPROC genie_pq_parameterstatus;
extern GPROC genie_pq_pass;
extern GPROC genie_pq_port;
extern GPROC genie_pq_protocolversion;
extern GPROC genie_pq_reset;
extern GPROC genie_pq_resulterrormessage;
extern GPROC genie_pq_serverversion;
extern GPROC genie_pq_socket;
extern GPROC genie_pq_tty;
extern GPROC genie_pq_user;
#endif

#if defined (HAVE_GNU_PLOTUTILS)
extern GPROC genie_draw_aspect;
extern GPROC genie_draw_atom;
extern GPROC genie_draw_background_colour;
extern GPROC genie_draw_background_colour_name;
extern GPROC genie_draw_circle;
extern GPROC genie_draw_clear;
extern GPROC genie_draw_colour;
extern GPROC genie_draw_colour_name;
extern GPROC genie_draw_fillstyle;
extern GPROC genie_draw_fontname;
extern GPROC genie_draw_fontsize;
extern GPROC genie_draw_get_colour_name;
extern GPROC genie_draw_line;
extern GPROC genie_draw_linestyle;
extern GPROC genie_draw_linewidth;
extern GPROC genie_draw_move;
extern GPROC genie_draw_point;
extern GPROC genie_draw_rect;
extern GPROC genie_draw_show;
extern GPROC genie_draw_star;
extern GPROC genie_draw_text;
extern GPROC genie_draw_textangle;
extern GPROC genie_make_device;
#endif

#if defined (BUILD_PARALLEL_CLAUSE)
extern PROP_T genie_parallel (NODE_T *);
extern BOOL_T is_main_thread (void);
extern void genie_abend_all_threads (NODE_T *, jmp_buf *, NODE_T *);
extern void genie_set_exit_from_threads (int);
#define SAME_THREAD(p, q) (pthread_equal((p), (q)) != 0)
#define OTHER_THREAD(p, q) (pthread_equal((p), (q)) == 0)
#endif

#if defined (BUILD_LINUX)
extern GPROC genie_sigsegv;
#endif

#endif
