//! @file a68g-transput.h
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2021 J. Marcel van der Veer <algol68g@xs4all.nl>.
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

#if !defined (__A68G_TRANSPUT_H__)
#define __A68G_TRANSPUT_H__

#if (A68_LEVEL >= 3)
extern char *long_sub_whole_double (NODE_T *, QUAD_WORD_T, int);
#endif

extern BOOL_T convert_radix_mp (NODE_T *, MP_T *, int, int, MOID_T *, MP_T *, MP_T *);
extern BOOL_T convert_radix (NODE_T *, UNSIGNED_T, int, int);
extern BOOL_T genie_string_to_value_internal (NODE_T *, MOID_T *, char *, BYTE_T *);
extern char digchar (int);
extern char *error_chars (char *, int);
extern char *fixed (NODE_T * p);
extern char *get_transput_buffer (int);
extern char *long_sub_fixed (NODE_T *, MP_T *, int, int, int);
extern char *long_sub_whole (NODE_T *, MP_T *, int, int);
extern char pop_char_transput_buffer (int);
extern char *real (NODE_T *);
extern char *sub_fixed_double (NODE_T *, DOUBLE_T, int, int, int);
extern char *sub_fixed (NODE_T *, REAL_T, int, int);
extern char *sub_whole (NODE_T *, INT_T, int);
extern char *whole (NODE_T * p);
extern FILE *a68_fopen (char *, char *, char *);
extern FILE_T open_physical_file (NODE_T *, A68_REF, int, mode_t);
extern GPROC genie_fixed;
extern GPROC genie_float;
extern GPROC genie_real;
extern GPROC genie_whole;
extern int char_scanner (A68_FILE *);
extern int end_of_format (NODE_T *, A68_REF);
extern int get_replicator_value (NODE_T *, BOOL_T);
extern int get_transput_buffer_index (int);
extern int get_transput_buffer_size (int);
extern int get_unblocked_transput_buffer (NODE_T *);
extern int store_file_entry (NODE_T *, FILE_T, char *, BOOL_T);
extern void add_a_string_transput_buffer (NODE_T *, int, BYTE_T *);
extern void add_chars_transput_buffer (NODE_T *, int, int, char *);
extern void add_string_from_stack_transput_buffer (NODE_T *, int);
extern void add_string_transput_buffer (NODE_T *, int, char *);
extern void end_of_file_error (NODE_T * p, A68_REF ref_file);
extern void enlarge_transput_buffer (NODE_T *, int, int);
extern void format_error (NODE_T *, A68_REF, char *);
extern void long_standardise (NODE_T *, MP_T *, int, int, int, int *);
extern void on_event_handler (NODE_T *, A68_PROCEDURE, A68_REF);
extern void open_error (NODE_T *, A68_REF, char *);
extern void pattern_error (NODE_T *, MOID_T *, int);
extern void plusab_transput_buffer (NODE_T *, int, char);
extern void plusto_transput_buffer (NODE_T *, char, int);
extern void read_insertion (NODE_T *, A68_REF);
extern void read_sound (NODE_T *, A68_REF, A68_SOUND *);
extern void reset_transput_buffer (int);
extern void set_default_event_procedure (A68_PROCEDURE *);
extern void set_default_event_procedures (A68_FILE *);
extern void set_transput_buffer_index (int, int);
extern void set_transput_buffer_size (int, int);
extern void standardise (REAL_T *, int, int, int *);
extern void transput_error (NODE_T *, A68_REF, MOID_T *);
extern void unchar_scanner (NODE_T *, A68_FILE *, char);
extern void value_error (NODE_T *, MOID_T *, A68_REF);
extern void write_insertion (NODE_T *, A68_REF, MOOD_T);
extern void write_purge_buffer (NODE_T *, A68_REF, int);
extern void write_sound (NODE_T *, A68_REF, A68_SOUND *);

#endif
