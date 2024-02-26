//! @file rts-transput.c
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
//! Transput routines.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-transput.h"

// Transput - General routines and unformatted transput.
// But Eeyore wasn't listening. He was taking the balloon out, and putting
// it back again, as happy as could be ... Winnie the Pooh, A.A. Milne.
// - Revised Report on the Algorithmic Language Algol 68.

// File table handling 
// In a table we record opened files.
// When execution ends, unclosed files are closed, and temps are removed.
// This keeps /tmp free of spurious files :-)

//! @brief Init a file entry.

void init_file_entry (int k)
{
  if (k >= 0 && k < MAX_OPEN_FILES) {
    FILE_ENTRY *fe = &(A68 (file_entries)[k]);
    POS (fe) = NO_NODE;
    IS_OPEN (fe) = A68_FALSE;
    IS_TMP (fe) = A68_FALSE;
    FD (fe) = A68_NO_FILENO;
    IDF (fe) = nil_ref;
  }
}

//! @brief Initialise file entry table.

void init_file_entries (void)
{
  for (int k = 0; k < MAX_OPEN_FILES; k++) {
    init_file_entry (k);
  }
}

//! @brief Store file for later closing when not explicitly closed.

int store_file_entry (NODE_T * p, FILE_T fd, char *idf, BOOL_T is_tmp)
{
  for (int k = 0; k < MAX_OPEN_FILES; k++) {
    FILE_ENTRY *fe = &(A68 (file_entries)[k]);
    if (!IS_OPEN (fe)) {
      int len = 1 + (int) strlen (idf);
      POS (fe) = p;
      IS_OPEN (fe) = A68_TRUE;
      IS_TMP (fe) = is_tmp;
      FD (fe) = fd;
      IDF (fe) = heap_generator (p, M_C_STRING, len);
      BLOCK_GC_HANDLE (&(IDF (fe)));
      bufcpy (DEREF (char, &IDF (fe)), idf, len);
      return k;
    }
  }
  diagnostic (A68_RUNTIME_ERROR, p, ERROR_TOO_MANY_OPEN_FILES);
  exit_genie (p, A68_RUNTIME_ERROR);
  return -1;
}

//! @brief Close file and delete temp file.

void close_file_entry (NODE_T * p, int k)
{
  if (k >= 0 && k < MAX_OPEN_FILES) {
    FILE_ENTRY *fe = &(A68 (file_entries)[k]);
    if (IS_OPEN (fe)) {
// Close the file.
      if (FD (fe) != A68_NO_FILENO && close (FD (fe)) == -1) {
        init_file_entry (k);
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_CLOSE);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      IS_OPEN (fe) = A68_FALSE;
    }
  }
}

//! @brief Close file and delete temp file.

void free_file_entry (NODE_T * p, int k)
{
  close_file_entry (p, k);
  if (k >= 0 && k < MAX_OPEN_FILES) {
    FILE_ENTRY *fe = &(A68 (file_entries)[k]);
    if (IS_OPEN (fe)) {
// Attempt to remove a temp file, but ignore failure.
      if (FD (fe) != A68_NO_FILENO && IS_TMP (fe)) {
        if (!IS_NIL (IDF (fe))) {
          char *filename;
          CHECK_INIT (p, INITIALISED (&(IDF (fe))), M_ROWS);
          filename = DEREF (char, &IDF (fe));
          if (filename != NO_TEXT) {
            (void) remove (filename);
          }
        }
      }
// Restore the fields.
      if (!IS_NIL (IDF (fe))) {
        UNBLOCK_GC_HANDLE (&(IDF (fe)));
      }
      init_file_entry (k);
    }
  }
}

//! @brief Close all files and delete all temp files.

void free_file_entries (void)
{
  for (int k = 0; k < MAX_OPEN_FILES; k++) {
    free_file_entry (NO_NODE, k);
  }
}

// Strings in transput are of arbitrary size. For this, we have transput buffers.
// A transput buffer is a REF STRUCT (INT size, index, STRING buffer).
// It is in the heap, but cannot be gc'ed. If it is too small, we give up on
// it and make a larger one.

A68_REF ref_transput_buffer[MAX_TRANSPUT_BUFFER];

//! @brief Set max number of chars in a transput buffer.

void set_transput_buffer_size (int n, int size)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]));
  STATUS (k) = INIT_MASK;
  VALUE (k) = size;
}

//! @brief Set char index for transput buffer.

void set_transput_buffer_index (int n, int cindex)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]) + SIZE (M_INT));
  STATUS (k) = INIT_MASK;
  VALUE (k) = cindex;
}

//! @brief Get max number of chars in a transput buffer.

int get_transput_buffer_size (int n)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]));
  return VALUE (k);
}

//! @brief Get char index for transput buffer.

int get_transput_buffer_index (int n)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]) + SIZE (M_INT));
  return VALUE (k);
}

//! @brief Get char[] from transput buffer.

char *get_transput_buffer (int n)
{
  return (char *) (ADDRESS (&ref_transput_buffer[n]) + 2 * SIZE (M_INT));
}

//! @brief Mark transput buffer as no longer in use.

void unblock_transput_buffer (int n)
{
  set_transput_buffer_index (n, -1);
}

//! @brief Find first unused transput buffer (for opening a file).

int get_unblocked_transput_buffer (NODE_T * p)
{
  for (int k = FIXED_TRANSPUT_BUFFERS; k < MAX_TRANSPUT_BUFFER; k++) {
    if (get_transput_buffer_index (k) == -1) {
      return k;
    }
  }
// Oops!
  diagnostic (A68_RUNTIME_ERROR, p, ERROR_TOO_MANY_OPEN_FILES);
  exit_genie (p, A68_RUNTIME_ERROR);
  return -1;
}

//! @brief Empty contents of transput buffer.

void reset_transput_buffer (int n)
{
  set_transput_buffer_index (n, 0);
  (get_transput_buffer (n))[0] = NULL_CHAR;
}

//! @brief Initialise transput buffers before use.

void init_transput_buffers (NODE_T * p)
{
  for (int k = 0; k < MAX_TRANSPUT_BUFFER; k++) {
    ref_transput_buffer[k] = heap_generator (p, M_ROWS, 2 * SIZE (M_INT) + TRANSPUT_BUFFER_SIZE);
    BLOCK_GC_HANDLE (&ref_transput_buffer[k]);
    set_transput_buffer_size (k, TRANSPUT_BUFFER_SIZE);
    reset_transput_buffer (k);
  }
// Last buffers are available for FILE values.
  for (int k = FIXED_TRANSPUT_BUFFERS; k < MAX_TRANSPUT_BUFFER; k++) {
    unblock_transput_buffer (k);
  }
}

//! @brief Make a transput buffer larger.

void enlarge_transput_buffer (NODE_T * p, int k, int size)
{
  int n = get_transput_buffer_index (k);
  char *sb_1 = get_transput_buffer (k), *sb_2;
  UNBLOCK_GC_HANDLE (&ref_transput_buffer[k]);
  ref_transput_buffer[k] = heap_generator (p, M_ROWS, 2 * SIZE (M_INT) + size);
  BLOCK_GC_HANDLE (&ref_transput_buffer[k]);
  set_transput_buffer_size (k, size);
  set_transput_buffer_index (k, n);
  sb_2 = get_transput_buffer (k);
  bufcpy (sb_2, sb_1, size);
}

//! @brief Add char to transput buffer; if the buffer is full, make it larger.

void plusab_transput_buffer (NODE_T * p, int k, char ch)
{
  char *sb = get_transput_buffer (k);
  int size = get_transput_buffer_size (k);
  int n = get_transput_buffer_index (k);
  if (n == size - 2) {
    enlarge_transput_buffer (p, k, 10 * size);
    plusab_transput_buffer (p, k, ch);
  } else {
    sb[n] = ch;
    sb[n + 1] = NULL_CHAR;
    set_transput_buffer_index (k, n + 1);
  }
}

//! @brief Add char to transput buffer at the head; if the buffer is full, make it larger.

void plusto_transput_buffer (NODE_T * p, char ch, int k)
{
  char *sb = get_transput_buffer (k);
  int size = get_transput_buffer_size (k);
  int n = get_transput_buffer_index (k);
  if (n == size - 2) {
    enlarge_transput_buffer (p, k, 10 * size);
    plusto_transput_buffer (p, ch, k);
  } else {
    MOVE (&sb[1], &sb[0], (unt) size);
    sb[0] = ch;
    sb[n + 1] = NULL_CHAR;
    set_transput_buffer_index (k, n + 1);
  }
}

//! @brief Add chars to transput buffer.

void add_chars_transput_buffer (NODE_T * p, int k, int N, char *ch)
{
  for (int j = 0; j < N; j++) {
    plusab_transput_buffer (p, k, ch[j]);
  }
}

//! @brief Add char[] to transput buffer.

void add_string_transput_buffer (NODE_T * p, int k, char *ch)
{
  for (; ch[0] != NULL_CHAR; ch++) {
    plusab_transput_buffer (p, k, ch[0]);
  }
}

//! @brief Add A68 string to transput buffer.

void add_a_string_transput_buffer (NODE_T * p, int k, BYTE_T * ref)
{
  A68_REF row = *(A68_REF *) ref;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  CHECK_INIT (p, INITIALISED (&row), M_ROWS);
  GET_DESCRIPTOR (arr, tup, &row);
  if (ROW_SIZE (tup) > 0) {
    BYTE_T *base_address = DEREF (BYTE_T, &ARRAY (arr));
    for (int i = LWB (tup); i <= UPB (tup); i++) {
      int addr = INDEX_1_DIM (arr, tup, i);
      A68_CHAR *ch = (A68_CHAR *) & (base_address[addr]);
      CHECK_INIT (p, INITIALISED (ch), M_CHAR);
      plusab_transput_buffer (p, k, (char) VALUE (ch));
    }
  }
}

//! @brief Pop A68 string and add to buffer.

void add_string_from_stack_transput_buffer (NODE_T * p, int k)
{
  DECREMENT_STACK_POINTER (p, A68_REF_SIZE);
  add_a_string_transput_buffer (p, k, STACK_TOP);
}

//! @brief Pop first character from transput buffer.

char pop_char_transput_buffer (int k)
{
  char *sb = get_transput_buffer (k);
  int n = get_transput_buffer_index (k);
  if (n <= 0) {
    return NULL_CHAR;
  } else {
    char ch = sb[0];
    MOVE (&sb[0], &sb[1], n);
    set_transput_buffer_index (k, n - 1);
    return ch;
  }
}

//! @brief Add C string to A68 string.

void add_c_string_to_a_string (NODE_T * p, A68_REF ref_str, char *s)
{
  int l_2 = (int) strlen (s);
// left part.
  CHECK_REF (p, ref_str, M_REF_STRING);
  A68_REF a = *DEREF (A68_REF, &ref_str);
  CHECK_INIT (p, INITIALISED (&a), M_STRING);
  A68_ARRAY *a_1; A68_TUPLE *t_1;
  GET_DESCRIPTOR (a_1, t_1, &a);
  int l_1 = ROW_SIZE (t_1);
// Sum string.
  A68_REF c = heap_generator (p, M_STRING, DESCRIPTOR_SIZE (1));
  A68_REF d = heap_generator (p, M_STRING, (l_1 + l_2) * SIZE (M_CHAR));
// Calculate again since garbage collector might have moved data.
// Todo: GC should not move volatile data.
  GET_DESCRIPTOR (a_1, t_1, &a);
// Make descriptor of new string.
  A68_ARRAY *a_3; A68_TUPLE *t_3;
  GET_DESCRIPTOR (a_3, t_3, &c);
  DIM (a_3) = 1;
  MOID (a_3) = M_CHAR;
  ELEM_SIZE (a_3) = SIZE (M_CHAR);
  SLICE_OFFSET (a_3) = 0;
  FIELD_OFFSET (a_3) = 0;
  ARRAY (a_3) = d;
  LWB (t_3) = 1;
  UPB (t_3) = l_1 + l_2;
  SHIFT (t_3) = LWB (t_3);
  SPAN (t_3) = 1;
// add strings.
  BYTE_T *b_1 = (ROW_SIZE (t_1) > 0 ? DEREF (BYTE_T, &ARRAY (a_1)) : NO_BYTE);
  BYTE_T *b_3 = DEREF (BYTE_T, &ARRAY (a_3));
  int u = 0;
  for (int v = LWB (t_1); v <= UPB (t_1); v++) {
    MOVE ((BYTE_T *) & b_3[u], (BYTE_T *) & b_1[INDEX_1_DIM (a_1, t_1, v)], SIZE (M_CHAR));
    u += SIZE (M_CHAR);
  }
  for (int v = 0; v < l_2; v++) {
    A68_CHAR ch;
    STATUS (&ch) = INIT_MASK;
    VALUE (&ch) = s[v];
    MOVE ((BYTE_T *) & b_3[u], (BYTE_T *) & ch, SIZE (M_CHAR));
    u += SIZE (M_CHAR);
  }
  *DEREF (A68_REF, &ref_str) = c;
}

//! @brief Purge buffer for file.

void write_purge_buffer (NODE_T * p, A68_REF ref_file, int k)
{
  A68_FILE *file = FILE_DEREF (&ref_file);
  if (IS_NIL (STRING (file))) {
    if (!(FD (file) == STDOUT_FILENO && A68 (halt_typing))) {
      WRITE (FD (file), get_transput_buffer (k));
    }
  } else {
    add_c_string_to_a_string (p, STRING (file), get_transput_buffer (k));
  }
  reset_transput_buffer (k);
}

// Routines that involve the A68 expression stack.

//! @brief Allocate a temporary string on the stack.

char *stack_string (NODE_T * p, int size)
{
  char *new_str = (char *) STACK_TOP;
  INCREMENT_STACK_POINTER (p, size);
  if (A68_SP > A68 (expr_stack_limit)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  FILL (new_str, NULL_CHAR, size);
  return new_str;
}

// Transput basic RTS routines.

//! @brief REF FILE standin

void genie_stand_in (NODE_T * p)
{
  PUSH_REF (p, A68 (stand_in));
}

//! @brief REF FILE standout

void genie_stand_out (NODE_T * p)
{
  PUSH_REF (p, A68 (stand_out));
}

//! @brief REF FILE standback

void genie_stand_back (NODE_T * p)
{
  PUSH_REF (p, A68 (stand_back));
}

//! @brief REF FILE standerror

void genie_stand_error (NODE_T * p)
{
  PUSH_REF (p, A68 (stand_error));
}

//! @brief CHAR error char

void genie_error_char (NODE_T * p)
{
  PUSH_VALUE (p, ERROR_CHAR, A68_CHAR);
}

//! @brief CHAR exp char

void genie_exp_char (NODE_T * p)
{
  PUSH_VALUE (p, EXPONENT_CHAR, A68_CHAR);
}

//! @brief CHAR flip char

void genie_flip_char (NODE_T * p)
{
  PUSH_VALUE (p, FLIP_CHAR, A68_CHAR);
}

//! @brief CHAR flop char

void genie_flop_char (NODE_T * p)
{
  PUSH_VALUE (p, FLOP_CHAR, A68_CHAR);
}

//! @brief CHAR null char

void genie_null_char (NODE_T * p)
{
  PUSH_VALUE (p, NULL_CHAR, A68_CHAR);
}

//! @brief CHAR blank

void genie_blank_char (NODE_T * p)
{
  PUSH_VALUE (p, BLANK_CHAR, A68_CHAR);
}

//! @brief CHAR newline char

void genie_newline_char (NODE_T * p)
{
  PUSH_VALUE (p, NEWLINE_CHAR, A68_CHAR);
}

//! @brief CHAR formfeed char

void genie_formfeed_char (NODE_T * p)
{
  PUSH_VALUE (p, FORMFEED_CHAR, A68_CHAR);
}

//! @brief CHAR tab char

void genie_tab_char (NODE_T * p)
{
  PUSH_VALUE (p, TAB_CHAR, A68_CHAR);
}

//! @brief CHANNEL standin channel

void genie_stand_in_channel (NODE_T * p)
{
  PUSH_OBJECT (p, A68 (stand_in_channel), A68_CHANNEL);
}

//! @brief CHANNEL standout channel

void genie_stand_out_channel (NODE_T * p)
{
  PUSH_OBJECT (p, A68 (stand_out_channel), A68_CHANNEL);
}

//! @brief CHANNEL stand draw channel

void genie_stand_draw_channel (NODE_T * p)
{
  PUSH_OBJECT (p, A68 (stand_draw_channel), A68_CHANNEL);
}

//! @brief CHANNEL standback channel

void genie_stand_back_channel (NODE_T * p)
{
  PUSH_OBJECT (p, A68 (stand_back_channel), A68_CHANNEL);
}

//! @brief CHANNEL standerror channel

void genie_stand_error_channel (NODE_T * p)
{
  PUSH_OBJECT (p, A68 (stand_error_channel), A68_CHANNEL);
}

//! @brief PROC STRING program idf

void genie_program_idf (NODE_T * p)
{
  PUSH_REF (p, c_to_a_string (p, FILE_SOURCE_NAME (&A68_JOB), DEFAULT_WIDTH));
}

// FILE and CHANNEL initialisations.

//! @brief Set_default_event_procedure.

void set_default_event_procedure (A68_PROCEDURE * z)
{
  STATUS (z) = INIT_MASK;
  NODE (&(BODY (z))) = NO_NODE;
  ENVIRON (z) = 0;
}

//! @brief Initialise channel.

void init_channel (A68_CHANNEL * chan, BOOL_T r, BOOL_T s, BOOL_T g, BOOL_T p, BOOL_T b, BOOL_T d)
{
  STATUS (chan) = INIT_MASK;
  RESET (chan) = r;
  SET (chan) = s;
  GET (chan) = g;
  PUT (chan) = p;
  BIN (chan) = b;
  DRAW (chan) = d;
  COMPRESS (chan) = A68_TRUE;
}

//! @brief Set default event handlers.

void set_default_event_procedures (A68_FILE * f)
{
  set_default_event_procedure (&(FILE_END_MENDED (f)));
  set_default_event_procedure (&(PAGE_END_MENDED (f)));
  set_default_event_procedure (&(LINE_END_MENDED (f)));
  set_default_event_procedure (&(VALUE_ERROR_MENDED (f)));
  set_default_event_procedure (&(OPEN_ERROR_MENDED (f)));
  set_default_event_procedure (&(TRANSPUT_ERROR_MENDED (f)));
  set_default_event_procedure (&(FORMAT_END_MENDED (f)));
  set_default_event_procedure (&(FORMAT_ERROR_MENDED (f)));
}

//! @brief Set up a REF FILE object.

void init_file (NODE_T * p, A68_REF * ref_file, A68_CHANNEL c, FILE_T s, BOOL_T rm, BOOL_T wm, BOOL_T cm, char *env)
{
  char *filename = (env == NO_TEXT ? NO_TEXT : getenv (env));
  *ref_file = heap_generator (p, M_REF_FILE, SIZE (M_FILE));
  BLOCK_GC_HANDLE (ref_file);
  A68_FILE *f = FILE_DEREF (ref_file);
  STATUS (f) = INIT_MASK;
  TERMINATOR (f) = nil_ref;
  CHANNEL (f) = c;
  if (filename != NO_TEXT && strlen (filename) > 0) {
    int len = 1 + (int) strlen (filename);
    IDENTIFICATION (f) = heap_generator (p, M_C_STRING, len);
    BLOCK_GC_HANDLE (&(IDENTIFICATION (f)));
    bufcpy (DEREF (char, &IDENTIFICATION (f)), filename, len);
    FD (f) = A68_NO_FILENO;
    READ_MOOD (f) = A68_FALSE;
    WRITE_MOOD (f) = A68_FALSE;
    CHAR_MOOD (f) = A68_FALSE;
    DRAW_MOOD (f) = A68_FALSE;
  } else {
    IDENTIFICATION (f) = nil_ref;
    FD (f) = s;
    READ_MOOD (f) = rm;
    WRITE_MOOD (f) = wm;
    CHAR_MOOD (f) = cm;
    DRAW_MOOD (f) = A68_FALSE;
  }
  TRANSPUT_BUFFER (f) = get_unblocked_transput_buffer (p);
  reset_transput_buffer (TRANSPUT_BUFFER (f));
  END_OF_FILE (f) = A68_FALSE;
  TMP_FILE (f) = A68_FALSE;
  OPENED (f) = A68_TRUE;
  OPEN_EXCLUSIVE (f) = A68_FALSE;
  FORMAT (f) = nil_format;
  STRING (f) = nil_ref;
  STRPOS (f) = 0;
  FILE_ENTRY (f) = -1;
  set_default_event_procedures (f);
}

//! @brief Initialise the transput RTL.

void genie_init_transput (NODE_T * p)
{
  init_transput_buffers (p);
// Channels.
  init_channel (&(A68 (stand_in_channel)), A68_FALSE, A68_FALSE, A68_TRUE, A68_FALSE, A68_FALSE, A68_FALSE);
  init_channel (&(A68 (stand_out_channel)), A68_FALSE, A68_FALSE, A68_FALSE, A68_TRUE, A68_FALSE, A68_FALSE);
  init_channel (&(A68 (stand_back_channel)), A68_TRUE, A68_TRUE, A68_TRUE, A68_TRUE, A68_TRUE, A68_FALSE);
  init_channel (&(A68 (stand_error_channel)), A68_FALSE, A68_FALSE, A68_FALSE, A68_TRUE, A68_FALSE, A68_FALSE);
  init_channel (&(A68 (associate_channel)), A68_TRUE, A68_TRUE, A68_TRUE, A68_TRUE, A68_FALSE, A68_FALSE);
  init_channel (&(A68 (skip_channel)), A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE);
#if defined (HAVE_GNU_PLOTUTILS)
  init_channel (&(A68 (stand_draw_channel)), A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_TRUE);
#else
  init_channel (&(A68 (stand_draw_channel)), A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_FALSE, A68_TRUE);
#endif
// Files.
  init_file (p, &(A68 (stand_in)), A68 (stand_in_channel), STDIN_FILENO, A68_TRUE, A68_FALSE, A68_TRUE, "A68_STANDIN");
  init_file (p, &(A68 (stand_out)), A68 (stand_out_channel), STDOUT_FILENO, A68_FALSE, A68_TRUE, A68_TRUE, "A68_STANDOUT");
  init_file (p, &(A68 (stand_back)), A68 (stand_back_channel), A68_NO_FILENO, A68_FALSE, A68_FALSE, A68_FALSE, NO_TEXT);
  init_file (p, &(A68 (stand_error)), A68 (stand_error_channel), STDERR_FILENO, A68_FALSE, A68_TRUE, A68_TRUE, "A68_STANDERROR");
  init_file (p, &(A68 (skip_file)), A68 (skip_channel), A68_NO_FILENO, A68_FALSE, A68_FALSE, A68_FALSE, NO_TEXT);
}

//! @brief PROC (REF FILE) STRING idf

void genie_idf (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  ref_file = *(A68_REF *) STACK_TOP;
  A68_REF ref_filename = IDENTIFICATION (FILE_DEREF (&ref_file));
  CHECK_REF (p, ref_filename, M_ROWS);
  char *filename = DEREF (char, &ref_filename);
  PUSH_REF (p, c_to_a_string (p, filename, DEFAULT_WIDTH));
}

//! @brief PROC (REF FILE) STRING term

void genie_term (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  ref_file = *(A68_REF *) STACK_TOP;
  A68_REF ref_term = TERMINATOR (FILE_DEREF (&ref_file));
  CHECK_REF (p, ref_term, M_ROWS);
  char *term = DEREF (char, &ref_term);
  PUSH_REF (p, c_to_a_string (p, term, DEFAULT_WIDTH));
}

//! @brief PROC (REF FILE, STRING) VOID make term

void genie_make_term (NODE_T * p)
{
  A68_REF ref_file, str;
  POP_REF (p, &str);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  ref_file = *(A68_REF *) STACK_TOP;
  A68_FILE *file = FILE_DEREF (&ref_file);
// Don't check initialisation so we can "make term" before opening.
  int size = a68_string_size (p, str);
  if (INITIALISED (&(TERMINATOR (file))) && !IS_NIL (TERMINATOR (file))) {
    UNBLOCK_GC_HANDLE (&(TERMINATOR (file)));
  }
  TERMINATOR (file) = heap_generator (p, M_C_STRING, 1 + size);
  BLOCK_GC_HANDLE (&(TERMINATOR (file)));
  ASSERT (a_to_c_string (p, DEREF (char, &TERMINATOR (file)), str) != NO_TEXT);
}

//! @brief PROC (REF FILE) BOOL put possible

void genie_put_possible (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PUSH_VALUE (p, PUT (&CHANNEL (file)), A68_BOOL);
}

//! @brief PROC (REF FILE) BOOL get possible

void genie_get_possible (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PUSH_VALUE (p, GET (&CHANNEL (file)), A68_BOOL);
}

//! @brief PROC (REF FILE) BOOL bin possible

void genie_bin_possible (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PUSH_VALUE (p, BIN (&CHANNEL (file)), A68_BOOL);
}

//! @brief PROC (REF FILE) BOOL set possible

void genie_set_possible (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PUSH_VALUE (p, SET (&CHANNEL (file)), A68_BOOL);
}

//! @brief PROC (REF FILE) BOOL reidf possible

void genie_reidf_possible (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PUSH_VALUE (p, A68_FALSE, A68_BOOL);
}

//! @brief PROC (REF FILE) BOOL reset possible

void genie_reset_possible (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PUSH_VALUE (p, RESET (&CHANNEL (file)), A68_BOOL);
}

//! @brief PROC (REF FILE) BOOL compressible

void genie_compressible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PUSH_VALUE (p, COMPRESS (&CHANNEL (file)), A68_BOOL);
}

//! @brief PROC (REF FILE) BOOL draw possible

void genie_draw_possible (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PUSH_VALUE (p, DRAW (&CHANNEL (file)), A68_BOOL);
}

//! @brief PROC (REF FILE, STRING, CHANNEL) INT open

void genie_open (NODE_T * p)
{
  A68_CHANNEL channel;
  POP_OBJECT (p, &channel, A68_CHANNEL);
  A68_REF ref_iden;
  POP_REF (p, &ref_iden);
  CHECK_REF (p, ref_iden, M_REF_STRING);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  STATUS (file) = INIT_MASK;
  FILE_ENTRY (file) = -1;
  CHANNEL (file) = channel;
  OPENED (file) = A68_TRUE;
  OPEN_EXCLUSIVE (file) = A68_FALSE;
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  TMP_FILE (file) = A68_FALSE;
  int size = a68_string_size (p, ref_iden);
  if (INITIALISED (&(IDENTIFICATION (file))) && !IS_NIL (IDENTIFICATION (file))) {
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  }
  IDENTIFICATION (file) = heap_generator (p, M_C_STRING, 1 + size);
  BLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  ASSERT (a_to_c_string (p, DEREF (char, &IDENTIFICATION (file)), ref_iden) != NO_TEXT);
  TERMINATOR (file) = nil_ref;
  FORMAT (file) = nil_format;
  FD (file) = A68_NO_FILENO;
  if (INITIALISED (&(STRING (file))) && !IS_NIL (STRING (file))) {
    UNBLOCK_GC_HANDLE (DEREF (A68_REF, &STRING (file)));
  }
  STRING (file) = nil_ref;
  STRPOS (file) = 0;
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
  STREAM (&DEVICE (file)) = NO_STREAM;
  set_default_event_procedures (file);
  {
    struct stat status;
    if (stat (DEREF (char, &IDENTIFICATION (file)), &status) == 0) {
      PUSH_VALUE (p, (S_ISREG (ST_MODE (&status)) != 0 ? 0 : 1), A68_INT);
    } else {
      PUSH_VALUE (p, 1, A68_INT);
    }
    errno = 0;
  }
}

//! @brief PROC (REF FILE, STRING, CHANNEL) INT establish

void genie_establish (NODE_T * p)
{
  A68_CHANNEL channel;
  POP_OBJECT (p, &channel, A68_CHANNEL);
  A68_REF ref_iden;
  POP_REF (p, &ref_iden);
  CHECK_REF (p, ref_iden, M_REF_STRING);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  STATUS (file) = INIT_MASK;
  FILE_ENTRY (file) = -1;
  CHANNEL (file) = channel;
  OPENED (file) = A68_TRUE;
  OPEN_EXCLUSIVE (file) = A68_TRUE;
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  TMP_FILE (file) = A68_FALSE;
  if (!PUT (&CHANNEL (file))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  int size = a68_string_size (p, ref_iden);
  if (INITIALISED (&(IDENTIFICATION (file))) && !IS_NIL (IDENTIFICATION (file))) {
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  }
  IDENTIFICATION (file) = heap_generator (p, M_C_STRING, 1 + size);
  BLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  ASSERT (a_to_c_string (p, DEREF (char, &IDENTIFICATION (file)), ref_iden) != NO_TEXT);
  TERMINATOR (file) = nil_ref;
  FORMAT (file) = nil_format;
  FD (file) = A68_NO_FILENO;
  if (INITIALISED (&(STRING (file))) && !IS_NIL (STRING (file))) {
    UNBLOCK_GC_HANDLE (DEREF (A68_REF, &STRING (file)));
  }
  STRING (file) = nil_ref;
  STRPOS (file) = 0;
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
  STREAM (&DEVICE (file)) = NO_STREAM;
  set_default_event_procedures (file);
  PUSH_VALUE (p, 0, A68_INT);
}

//! @brief PROC (REF FILE, CHANNEL) INT create

void genie_create (NODE_T * p)
{
  A68_CHANNEL channel;
  POP_OBJECT (p, &channel, A68_CHANNEL);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  STATUS (file) = INIT_MASK;
  FILE_ENTRY (file) = -1;
  CHANNEL (file) = channel;
  OPENED (file) = A68_TRUE;
  OPEN_EXCLUSIVE (file) = A68_FALSE;
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  TMP_FILE (file) = A68_TRUE;
  if (INITIALISED (&(IDENTIFICATION (file))) && !IS_NIL (IDENTIFICATION (file))) {
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  }
  IDENTIFICATION (file) = nil_ref;
  TERMINATOR (file) = nil_ref;
  FORMAT (file) = nil_format;
  FD (file) = A68_NO_FILENO;
  if (INITIALISED (&(STRING (file))) && !IS_NIL (STRING (file))) {
    UNBLOCK_GC_HANDLE (DEREF (A68_REF, &STRING (file)));
  }
  STRING (file) = nil_ref;
  STRPOS (file) = 0;
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
  STREAM (&DEVICE (file)) = NO_STREAM;
  set_default_event_procedures (file);
  PUSH_VALUE (p, 0, A68_INT);
}

//! @brief PROC (REF FILE, REF STRING) VOID associate

void genie_associate (NODE_T * p)
{
  A68_REF ref_string;
  POP_REF (p, &ref_string);
  CHECK_REF (p, ref_string, M_REF_STRING);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  if (IS_IN_HEAP (&ref_file) && !IS_IN_HEAP (&ref_string)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, M_REF_STRING);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (IS_IN_FRAME (&ref_file) && IS_IN_FRAME (&ref_string)) {
    if (REF_SCOPE (&ref_string) > REF_SCOPE (&ref_file)) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, M_REF_STRING);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  A68_FILE *file = FILE_DEREF (&ref_file);
  STATUS (file) = INIT_MASK;
  FILE_ENTRY (file) = -1;
  CHANNEL (file) = A68 (associate_channel);
  OPENED (file) = A68_TRUE;
  OPEN_EXCLUSIVE (file) = A68_FALSE;
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  TMP_FILE (file) = A68_FALSE;
  if (INITIALISED (&(IDENTIFICATION (file))) && !IS_NIL (IDENTIFICATION (file))) {
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
  }
  IDENTIFICATION (file) = nil_ref;
  TERMINATOR (file) = nil_ref;
  FORMAT (file) = nil_format;
  FD (file) = A68_NO_FILENO;
  if (INITIALISED (&(STRING (file))) && !IS_NIL (STRING (file))) {
    UNBLOCK_GC_HANDLE (DEREF (A68_REF, &STRING (file)));
  }
  STRING (file) = ref_string;
  BLOCK_GC_HANDLE ((A68_REF *) (&(STRING (file))));
  STRPOS (file) = 0;
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
  STREAM (&DEVICE (file)) = NO_STREAM;
  set_default_event_procedures (file);
}

//! @brief PROC (REF FILE) VOID close

void genie_close (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file) || (!READ_MOOD (file) && !WRITE_MOOD (file) && !DRAW_MOOD (file))) {
    return;
  }
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
#if defined (HAVE_GNU_PLOTUTILS)
  if (DEVICE_OPENED (&DEVICE (file))) {
    ASSERT (close_device (p, file) == A68_TRUE);
    STREAM (&DEVICE (file)) = NO_STREAM;
    return;
  }
#endif
  FD (file) = A68_NO_FILENO;
  OPENED (file) = A68_FALSE;
  unblock_transput_buffer (TRANSPUT_BUFFER (file));
  set_default_event_procedures (file);
  free_file_entry (p, FILE_ENTRY (file));
}

//! @brief PROC (REF FILE) VOID lock

void genie_lock (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file) || (!READ_MOOD (file) && !WRITE_MOOD (file) && !DRAW_MOOD (file))) {
    return;
  }
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
#if defined (HAVE_GNU_PLOTUTILS)
  if (DEVICE_OPENED (&DEVICE (file))) {
    ASSERT (close_device (p, file) == A68_TRUE);
    STREAM (&DEVICE (file)) = NO_STREAM;
    return;
  }
#endif
#if defined (BUILD_UNIX)
  errno = 0;
  ASSERT (fchmod (FD (file), (mode_t) 0x0) != -1);
#endif
  if (FD (file) != A68_NO_FILENO && close (FD (file)) == -1) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_LOCK);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    FD (file) = A68_NO_FILENO;
    OPENED (file) = A68_FALSE;
    unblock_transput_buffer (TRANSPUT_BUFFER (file));
    set_default_event_procedures (file);
  }
  free_file_entry (p, FILE_ENTRY (file));
}

//! @brief PROC (REF FILE) VOID erase

void genie_erase (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file) || (!READ_MOOD (file) && !WRITE_MOOD (file) && !DRAW_MOOD (file))) {
    return;
  }
  DEVICE_MADE (&DEVICE (file)) = A68_FALSE;
#if defined (HAVE_GNU_PLOTUTILS)
  if (DEVICE_OPENED (&DEVICE (file))) {
    ASSERT (close_device (p, file) == A68_TRUE);
    STREAM (&DEVICE (file)) = NO_STREAM;
    return;
  }
#endif
  if (FD (file) != A68_NO_FILENO && close (FD (file)) == -1) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_SCRATCH);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    unblock_transput_buffer (TRANSPUT_BUFFER (file));
    set_default_event_procedures (file);
  }
// Remove the file.
  if (!IS_NIL (IDENTIFICATION (file))) {
    char *filename;
    CHECK_INIT (p, INITIALISED (&(IDENTIFICATION (file))), M_ROWS);
    filename = DEREF (char, &IDENTIFICATION (file));
    if (remove (filename) != 0) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_SCRATCH);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    UNBLOCK_GC_HANDLE (&(IDENTIFICATION (file)));
    IDENTIFICATION (file) = nil_ref;
  }
  init_file_entry (FILE_ENTRY (file));
}

//! @brief PROC (REF FILE) VOID backspace

void genie_backspace (NODE_T * p)
{
  ADDR_T pop_sp = A68_SP;
  PUSH_VALUE (p, -1, A68_INT);
  genie_set (p);
  A68_SP = pop_sp;
}

//! @brief PROC (REF FILE, INT) INT set

void genie_set (NODE_T * p)
{
  A68_INT pos;
  POP_OBJECT (p, &pos, A68_INT);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!SET (&CHANNEL (file))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_CANT_SET);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!IS_NIL (STRING (file))) {
    A68_REF z = *DEREF (A68_REF, &STRING (file));
    A68_ARRAY *a;
    A68_TUPLE *t;
    int size;
// Circumvent buffering problems.
    STRPOS (file) -= get_transput_buffer_index (TRANSPUT_BUFFER (file));
    ASSERT (STRPOS (file) > 0);
    reset_transput_buffer (TRANSPUT_BUFFER (file));
// Now set.
    CHECK_INT_ADDITION (p, STRPOS (file), VALUE (&pos));
    STRPOS (file) += VALUE (&pos);
    GET_DESCRIPTOR (a, t, &z);
    size = ROW_SIZE (t);
    if (size <= 0 || STRPOS (file) < 0 || STRPOS (file) >= size) {
      A68_BOOL res;
      on_event_handler (p, FILE_END_MENDED (FILE_DEREF (&ref_file)), ref_file);
      POP_OBJECT (p, &res, A68_BOOL);
      if (VALUE (&res) == A68_FALSE) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ENDED);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
    }
    PUSH_VALUE (p, STRPOS (file), A68_INT);
  } else if (FD (file) == A68_NO_FILENO) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_RESET);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else {
    errno = 0;
    __off_t curpos = lseek (FD (file), 0, SEEK_CUR);
    __off_t maxpos = lseek (FD (file), 0, SEEK_END);
    __off_t res = lseek (FD (file), curpos, SEEK_SET);
// Circumvent buffering problems.
    int reserve = get_transput_buffer_index (TRANSPUT_BUFFER (file));
    curpos -= (__off_t) reserve;
    res = lseek (FD (file), -reserve, SEEK_CUR);
    ASSERT (res != -1 && errno == 0);
    reset_transput_buffer (TRANSPUT_BUFFER (file));
// Now set.
    CHECK_INT_ADDITION (p, curpos, VALUE (&pos));
    curpos += VALUE (&pos);
    if (curpos < 0 || curpos >= maxpos) {
      A68_BOOL ret;
      on_event_handler (p, FILE_END_MENDED (FILE_DEREF (&ref_file)), ref_file);
      POP_OBJECT (p, &ret, A68_BOOL);
      if (VALUE (&ret) == A68_FALSE) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ENDED);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      PUSH_VALUE (p, (int) lseek (FD (file), 0, SEEK_CUR), A68_INT);
    } else {
      res = lseek (FD (file), curpos, SEEK_SET);
      if (res == -1 || errno != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_SET);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      PUSH_VALUE (p, (int) res, A68_INT);
    }
  }
}

//! @brief PROC (REF FILE) VOID reset

void genie_reset (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!RESET (&CHANNEL (file))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_CANT_RESET);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (IS_NIL (STRING (file))) {
    close_file_entry (p, FILE_ENTRY (file));
  } else {
    STRPOS (file) = 0;
  }
  READ_MOOD (file) = A68_FALSE;
  WRITE_MOOD (file) = A68_FALSE;
  CHAR_MOOD (file) = A68_FALSE;
  DRAW_MOOD (file) = A68_FALSE;
  FD (file) = A68_NO_FILENO;
//  set_default_event_procedures (file);.
}

//! @brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on file end

void genie_on_file_end (NODE_T * p)
{
  A68_PROCEDURE z;
  POP_PROCEDURE (p, &z);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  FILE_END_MENDED (file) = z;
}

//! @brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on page end

void genie_on_page_end (NODE_T * p)
{
  A68_PROCEDURE z;
  POP_PROCEDURE (p, &z);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  PAGE_END_MENDED (file) = z;
}

//! @brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on line end

void genie_on_line_end (NODE_T * p)
{
  A68_PROCEDURE z;
  POP_PROCEDURE (p, &z);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  LINE_END_MENDED (file) = z;
}

//! @brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on format end

void genie_on_format_end (NODE_T * p)
{
  A68_PROCEDURE z;
  POP_PROCEDURE (p, &z);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  FORMAT_END_MENDED (file) = z;
}

//! @brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on format error

void genie_on_format_error (NODE_T * p)
{
  A68_PROCEDURE z;
  POP_PROCEDURE (p, &z);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  FORMAT_ERROR_MENDED (file) = z;
}

//! @brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on value error

void genie_on_value_error (NODE_T * p)
{
  A68_PROCEDURE z;
  POP_PROCEDURE (p, &z);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  VALUE_ERROR_MENDED (file) = z;
}

//! @brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on open error

void genie_on_open_error (NODE_T * p)
{
  A68_PROCEDURE z;
  POP_PROCEDURE (p, &z);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  OPEN_ERROR_MENDED (file) = z;
}

//! @brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on transput error

void genie_on_transput_error (NODE_T * p)
{
  A68_PROCEDURE z;
  POP_PROCEDURE (p, &z);
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  TRANSPUT_ERROR_MENDED (file) = z;
}

//! @brief Invoke event routine.

void on_event_handler (NODE_T * p, A68_PROCEDURE z, A68_REF ref_file)
{
  if (NODE (&(BODY (&z))) == NO_NODE) {
// Default procedure.
    PUSH_VALUE (p, A68_FALSE, A68_BOOL);
  } else {
    ADDR_T pop_sp = A68_SP, pop_fp = A68_FP;
    PUSH_REF (p, ref_file);
    genie_call_event_routine (p, M_PROC_REF_FILE_BOOL, &z, pop_sp, pop_fp);
  }
}

//! @brief Handle end-of-file event.

void end_of_file_error (NODE_T * p, A68_REF ref_file)
{
  on_event_handler (p, FILE_END_MENDED (FILE_DEREF (&ref_file)), ref_file);
  A68_BOOL z;
  POP_OBJECT (p, &z, A68_BOOL);
  if (VALUE (&z) == A68_FALSE) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_ENDED);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief Handle file-open-error event.

void open_error (NODE_T * p, A68_REF ref_file, char *mode)
{
  on_event_handler (p, OPEN_ERROR_MENDED (FILE_DEREF (&ref_file)), ref_file);
  A68_BOOL z;
  POP_OBJECT (p, &z, A68_BOOL);
  if (VALUE (&z) == A68_FALSE) {
    CHECK_REF (p, ref_file, M_REF_FILE);
    A68_FILE *file = FILE_DEREF (&ref_file);
    CHECK_INIT (p, INITIALISED (file), M_FILE);
    char *filename;
    if (!IS_NIL (IDENTIFICATION (file))) {
      filename = DEREF (char, &IDENTIFICATION (FILE_DEREF (&ref_file)));
    } else {
      filename = "(missing filename)";
    }
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_CANNOT_OPEN_FOR, filename, mode);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief Handle value error event.

void value_error (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  if (END_OF_FILE (f)) {
    end_of_file_error (p, ref_file);
  } else {
    on_event_handler (p, VALUE_ERROR_MENDED (f), ref_file);
    A68_BOOL z;
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_TRANSPUT, m);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
}

//! @brief Handle value_error event.

void value_sign_error (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  if (END_OF_FILE (f)) {
    end_of_file_error (p, ref_file);
  } else {
    on_event_handler (p, VALUE_ERROR_MENDED (f), ref_file);
    A68_BOOL z;
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_TRANSPUT_SIGN, m);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
}

//! @brief Handle transput-error event.

void transput_error (NODE_T * p, A68_REF ref_file, MOID_T * m)
{
  on_event_handler (p, TRANSPUT_ERROR_MENDED (FILE_DEREF (&ref_file)), ref_file);
  A68_BOOL z;
  POP_OBJECT (p, &z, A68_BOOL);
  if (VALUE (&z) == A68_FALSE) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_TRANSPUT, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

// Implementation of put and get.

//! @brief Get next char from file.

int char_scanner (A68_FILE * f)
{
  if (get_transput_buffer_index (TRANSPUT_BUFFER (f)) > 0) {
// There are buffered characters.
    END_OF_FILE (f) = A68_FALSE;
    return pop_char_transput_buffer (TRANSPUT_BUFFER (f));
  } else if (IS_NIL (STRING (f))) {
// Fetch next CHAR from the FILE.
    char ch;
    ssize_t chars_read = io_read_conv (FD (f), &ch, 1);
    if (chars_read == 1) {
      END_OF_FILE (f) = A68_FALSE;
      return ch;
    } else {
      END_OF_FILE (f) = A68_TRUE;
      return EOF_CHAR;
    }
  } else {
// File is associated with a STRING. Give next CHAR. 
// When we're outside the STRING give EOF_CHAR. 
    A68_REF z = *DEREF (A68_REF, &STRING (f)); A68_ARRAY *a; A68_TUPLE *t;
    GET_DESCRIPTOR (a, t, &z);
    int k = STRPOS (f) + LWB (t);
    if (ROW_SIZE (t) <= 0 || k < LWB (t) || k > UPB (t)) {
      END_OF_FILE (f) = A68_TRUE;
      return EOF_CHAR;
    } else {
      BYTE_T *base = DEREF (BYTE_T, &ARRAY (a));
      A68_CHAR *ch = (A68_CHAR *) & (base[INDEX_1_DIM (a, t, k)]);
      STRPOS (f)++;
      return VALUE (ch);
    }
  }
}

//! @brief Push back look-ahead character to file.

void unchar_scanner (NODE_T * p, A68_FILE * f, char ch)
{
  END_OF_FILE (f) = A68_FALSE;
  plusab_transput_buffer (p, TRANSPUT_BUFFER (f), ch);
}

//! @brief PROC (REF FILE) BOOL eof

void genie_eof (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (READ_MOOD (file)) {
    int ch = char_scanner (file);
    PUSH_VALUE (p, (BOOL_T) ((ch == EOF_CHAR || END_OF_FILE (file)) ? A68_TRUE : A68_FALSE), A68_BOOL);
    unchar_scanner (p, file, (char) ch);
  } else {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief PROC (REF FILE) BOOL eoln

void genie_eoln (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (READ_MOOD (file)) {
    int ch = char_scanner (file);
    if (END_OF_FILE (file)) {
      end_of_file_error (p, ref_file);
    }
    PUSH_VALUE (p, (BOOL_T) (ch == NEWLINE_CHAR ? A68_TRUE : A68_FALSE), A68_BOOL);
    unchar_scanner (p, file, (char) ch);
  } else {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief PROC (REF FILE) VOID new line

void genie_new_line (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    on_event_handler (p, LINE_END_MENDED (file), ref_file);
    if (IS_NIL (STRING (file))) {
      WRITE (FD (file), NEWLINE_STRING);
    } else {
      add_c_string_to_a_string (p, STRING (file), NEWLINE_STRING);
    }
  } else if (READ_MOOD (file)) {
    BOOL_T go_on = A68_TRUE;
    while (go_on) {
      int ch;
      if (END_OF_FILE (file)) {
        end_of_file_error (p, ref_file);
      }
      ch = char_scanner (file);
      go_on = (BOOL_T) ((ch != NEWLINE_CHAR) && (ch != EOF_CHAR) && !END_OF_FILE (file));
    }
  } else {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief PROC (REF FILE) VOID new page

void genie_new_page (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    on_event_handler (p, PAGE_END_MENDED (file), ref_file);
    if (IS_NIL (STRING (file))) {
      WRITE (FD (file), "\f");
    } else {
      add_c_string_to_a_string (p, STRING (file), "\f");
    }
  } else if (READ_MOOD (file)) {
    BOOL_T go_on = A68_TRUE;
    while (go_on) {
      if (END_OF_FILE (file)) {
        end_of_file_error (p, ref_file);
      }
      int ch = char_scanner (file);
      go_on = (BOOL_T) ((ch != FORMFEED_CHAR) && (ch != EOF_CHAR) && !END_OF_FILE (file));
    }
  } else {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief PROC (REF FILE) VOID space

void genie_space (NODE_T * p)
{
  A68_REF ref_file;
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  A68_FILE *file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (WRITE_MOOD (file)) {
    WRITE (FD (file), " ");
  } else if (READ_MOOD (file)) {
    if (!END_OF_FILE (file)) {
      (void) char_scanner (file);
    }
  } else {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

