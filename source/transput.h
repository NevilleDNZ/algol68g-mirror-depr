/*!
\file transput.h
\brief contains transput related definitions
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

#if ! defined A68G_TRANSPUT_H
#define A68G_TRANSPUT_H

#define TRANSPUT_BUFFER_SIZE 	BUFFER_SIZE
#define ITEM_NOT_USED		(-1)
#define EMBEDDED_FORMAT 	A68_TRUE
#define NOT_EMBEDDED_FORMAT     A68_FALSE
#define WANT_PATTERN    	A68_TRUE
#define SKIP_PATTERN    	A68_FALSE

#define IS_NIL_FORMAT(f) (BODY (f) == NULL && ENVIRON (f) == 0)
#define NON_TERM(p) (find_non_terminal (top_non_terminal, ATTRIBUTE (p)))

#undef DEBUG

#define DIGIT_NORMAL		0x1
#define DIGIT_BLANK		0x2

#define INSERTION_NORMAL	0x10
#define INSERTION_BLANK		0x20

#define MAX_TRANSPUT_BUFFER	64	/* Some OS's open only 64 files. */

enum
{ INPUT_BUFFER = 0, OUTPUT_BUFFER, EDIT_BUFFER, UNFORMATTED_BUFFER, 
  FORMATTED_BUFFER, DOMAIN_BUFFER, PATH_BUFFER, REQUEST_BUFFER, 
  CONTENT_BUFFER, STRING_BUFFER, PATTERN_BUFFER, REPLACE_BUFFER, 
  READLINE_BUFFER, FIXED_TRANSPUT_BUFFERS
};

extern A68_CHANNEL stand_in_channel, stand_out_channel, stand_draw_channel, stand_back_channel, stand_error_channel, associate_channel;

extern GENIE_PROCEDURE genie_associate;
extern GENIE_PROCEDURE genie_backspace;
extern GENIE_PROCEDURE genie_bin_possible;
extern GENIE_PROCEDURE genie_blank_char;
extern GENIE_PROCEDURE genie_close;
extern GENIE_PROCEDURE genie_compressible;
extern GENIE_PROCEDURE genie_create;
extern GENIE_PROCEDURE genie_draw_possible;
extern GENIE_PROCEDURE genie_erase;
extern GENIE_PROCEDURE genie_real;
extern GENIE_PROCEDURE genie_error_char;
extern GENIE_PROCEDURE genie_establish;
extern GENIE_PROCEDURE genie_exp_char;
extern GENIE_PROCEDURE genie_fixed;
extern GENIE_PROCEDURE genie_flip_char;
extern GENIE_PROCEDURE genie_float;
extern GENIE_PROCEDURE genie_flop_char;
extern GENIE_PROCEDURE genie_formfeed_char;
extern GENIE_PROCEDURE genie_get_possible;
extern GENIE_PROCEDURE genie_init_transput;
extern GENIE_PROCEDURE genie_lock;
extern GENIE_PROCEDURE genie_new_line;
extern GENIE_PROCEDURE genie_new_page;
extern GENIE_PROCEDURE genie_newline_char;
extern GENIE_PROCEDURE genie_on_file_end;
extern GENIE_PROCEDURE genie_on_format_end;
extern GENIE_PROCEDURE genie_on_format_error;
extern GENIE_PROCEDURE genie_on_line_end;
extern GENIE_PROCEDURE genie_on_open_error;
extern GENIE_PROCEDURE genie_on_page_end;
extern GENIE_PROCEDURE genie_on_transput_error;
extern GENIE_PROCEDURE genie_on_value_error;
extern GENIE_PROCEDURE genie_open;
extern GENIE_PROCEDURE genie_print_bits;
extern GENIE_PROCEDURE genie_print_bool;
extern GENIE_PROCEDURE genie_print_char;
extern GENIE_PROCEDURE genie_print_complex;
extern GENIE_PROCEDURE genie_print_int;
extern GENIE_PROCEDURE genie_print_long_bits;
extern GENIE_PROCEDURE genie_print_long_complex;
extern GENIE_PROCEDURE genie_print_long_int;
extern GENIE_PROCEDURE genie_print_long_real;
extern GENIE_PROCEDURE genie_print_longlong_bits;
extern GENIE_PROCEDURE genie_print_longlong_complex;
extern GENIE_PROCEDURE genie_print_longlong_int;
extern GENIE_PROCEDURE genie_print_longlong_real;
extern GENIE_PROCEDURE genie_print_real;
extern GENIE_PROCEDURE genie_print_string;
extern GENIE_PROCEDURE genie_put_possible;
extern GENIE_PROCEDURE genie_read;
extern GENIE_PROCEDURE genie_read_bin;
extern GENIE_PROCEDURE genie_read_bin_file;
extern GENIE_PROCEDURE genie_read_bits;
extern GENIE_PROCEDURE genie_read_bool;
extern GENIE_PROCEDURE genie_read_char;
extern GENIE_PROCEDURE genie_read_complex;
extern GENIE_PROCEDURE genie_read_file;
extern GENIE_PROCEDURE genie_read_file_format;
extern GENIE_PROCEDURE genie_read_format;
extern GENIE_PROCEDURE genie_read_int;
extern GENIE_PROCEDURE genie_read_long_bits;
extern GENIE_PROCEDURE genie_read_long_complex;
extern GENIE_PROCEDURE genie_read_long_int;
extern GENIE_PROCEDURE genie_read_long_real;
extern GENIE_PROCEDURE genie_read_longlong_bits;
extern GENIE_PROCEDURE genie_read_longlong_complex;
extern GENIE_PROCEDURE genie_read_longlong_int;
extern GENIE_PROCEDURE genie_read_longlong_real;
extern GENIE_PROCEDURE genie_read_real;
extern GENIE_PROCEDURE genie_read_string;
extern GENIE_PROCEDURE genie_reidf_possible;
extern GENIE_PROCEDURE genie_reset;
extern GENIE_PROCEDURE genie_reset_possible;
extern GENIE_PROCEDURE genie_set;
extern GENIE_PROCEDURE genie_set_possible;
extern GENIE_PROCEDURE genie_space;
extern GENIE_PROCEDURE genie_tab_char;
extern GENIE_PROCEDURE genie_whole;
extern GENIE_PROCEDURE genie_write;
extern GENIE_PROCEDURE genie_write_bin;
extern GENIE_PROCEDURE genie_write_bin_file;
extern GENIE_PROCEDURE genie_write_file;
extern GENIE_PROCEDURE genie_write_file_format;
extern GENIE_PROCEDURE genie_write_format;

#if defined ENABLE_GRAPHICS
extern GENIE_PROCEDURE genie_draw_aspect;
extern GENIE_PROCEDURE genie_draw_atom;
extern GENIE_PROCEDURE genie_draw_background_colour;
extern GENIE_PROCEDURE genie_draw_background_colour_name;
extern GENIE_PROCEDURE genie_draw_circle;
extern GENIE_PROCEDURE genie_draw_clear;
extern GENIE_PROCEDURE genie_draw_colour;
extern GENIE_PROCEDURE genie_draw_colour_name;
extern GENIE_PROCEDURE genie_draw_filltype;
extern GENIE_PROCEDURE genie_draw_fontname;
extern GENIE_PROCEDURE genie_draw_fontsize;
extern GENIE_PROCEDURE genie_draw_get_colour_name;
extern GENIE_PROCEDURE genie_draw_line;
extern GENIE_PROCEDURE genie_draw_linestyle;
extern GENIE_PROCEDURE genie_draw_linewidth;
extern GENIE_PROCEDURE genie_draw_move;
extern GENIE_PROCEDURE genie_draw_point;
extern GENIE_PROCEDURE genie_draw_rect;
extern GENIE_PROCEDURE genie_draw_show;
extern GENIE_PROCEDURE genie_draw_star;
extern GENIE_PROCEDURE genie_draw_text;
extern GENIE_PROCEDURE genie_draw_textangle;
extern GENIE_PROCEDURE genie_make_device;
#endif

extern FILE_T open_physical_file (NODE_T *, A68_REF, int, mode_t);
extern NODE_T *get_next_format_pattern (NODE_T *, A68_REF, BOOL_T);
extern char *error_chars (char *, int);
extern char *fixed (NODE_T * p);
extern char *fleet (NODE_T * p);
extern char *get_transput_buffer (int);
extern char *stack_string (NODE_T *, int);
extern char *sub_fixed (NODE_T *, double, int, int);
extern char *sub_whole (NODE_T *, int, int);
extern char *whole (NODE_T * p);
extern char get_flip_char (void);
extern char get_flop_char (void);
extern char pop_char_transput_buffer (int);
extern int char_scanner (A68_FILE *);
extern int end_of_format (NODE_T *, A68_REF);
extern int get_replicator_value (NODE_T *, BOOL_T);
extern int get_transput_buffer_index (int);
extern int get_transput_buffer_length (int);
extern int get_transput_buffer_size (int);
extern int get_unblocked_transput_buffer (NODE_T *);
extern void add_a_string_transput_buffer (NODE_T *, int, BYTE_T *);
extern void add_char_transput_buffer (NODE_T *, int, char);
extern void add_string_from_stack_transput_buffer (NODE_T *, int);
extern void add_string_transput_buffer (NODE_T *, int, char *);
extern void end_of_file_error (NODE_T * p, A68_REF ref_file);
extern void enlarge_transput_buffer (NODE_T *, int, int);
extern void format_error (NODE_T *, A68_REF, char *);
extern void genie_read_standard (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_string_to_value (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_value_to_string (NODE_T *, MOID_T *, BYTE_T *, int);
extern void genie_write_standard (NODE_T *, MOID_T *, BYTE_T *, A68_REF);
extern void genie_write_string_from_stack (NODE_T * p, A68_REF);
extern void on_event_handler (NODE_T *, A68_PROCEDURE, A68_REF);
extern void open_error (NODE_T *, A68_REF, char *);
extern void pattern_error (NODE_T *, MOID_T *, int);
extern void read_insertion (NODE_T *, A68_REF);
extern void reset_transput_buffer (int);
extern void set_default_mended_procedure (A68_PROCEDURE *);
extern void set_default_mended_procedures (A68_FILE *);
extern void set_transput_buffer_index (int, int);
extern void set_transput_buffer_length (int, int);
extern void set_transput_buffer_size (int, int);
extern void standardise (double *, int, int, int *);
extern void transput_error (NODE_T *, A68_REF, MOID_T *);
extern void unchar_scanner (NODE_T *, A68_FILE *, char);
extern void value_error (NODE_T *, MOID_T *, A68_REF);
extern void write_insertion (NODE_T *, A68_REF, unsigned);
extern void write_purge_buffer (NODE_T *, A68_REF, int);
extern void read_sound (NODE_T *, A68_REF, A68_SOUND *);
extern void write_sound (NODE_T *, A68_REF, A68_SOUND *);

#endif
