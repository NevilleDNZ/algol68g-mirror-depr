/*!
\file transput.c
\brief standard prelude implementation, transput only
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2006 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "algol68g.h"
#include "genie.h"
#include "mp.h"
#include "transput.h"

/*
Transput library - General routines and (formatted) transput.

But Eeyore wasn't listening. He was taking the balloon out, and putting
it back again, as happy as could be ... Winnie the Pooh, A.A. Milne.
- Revised Report on the Algorithmic Language Algol 68.
*/

A68_CHANNEL stand_in_channel, stand_out_channel, stand_draw_channel, stand_back_channel, stand_error_channel, associate_channel;
A68_REF stand_in, stand_out, stand_back, stand_error;
A68_FORMAT nil_format = { INITIALISED_MASK, NULL, 0 };

/*
Strings in transput are of arbitrary size. For this, we have transput buffers.
A transput buffer is a REF STRUCT (INT size, index, STRING buffer).
It is in the heap, but cannot be sweeped. If it is too small, we give up on
it and make a larger one.
*/

static A68_REF ref_transput_buffer[MAX_TRANSPUT_BUFFER];

/*!
\brief set max number of chars in a transput buffer
\param n
\param size
**/

void set_transput_buffer_size (int n, int size)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]));
  k->status = INITIALISED_MASK;
  k->value = size;
}

/*!
\brief set char index for transput buffer
\param n
\param index
\return
**/

void set_transput_buffer_index (int n, int index)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]) + SIZE_OF (A68_INT));
  k->status = INITIALISED_MASK;
  k->value = index;
}

/*!
\brief get max number of chars in a transput buffer
\param n
\return
**/

int get_transput_buffer_size (int n)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]));
  return (k->value);
}

/*!
\brief get char index for transput buffer
\param n
\return
**/

int get_transput_buffer_index (int n)
{
  A68_INT *k = (A68_INT *) (ADDRESS (&ref_transput_buffer[n]) + SIZE_OF (A68_INT));
  return (k->value);
}

/*!
\brief get char[] from transput buffer
\param n
\return
**/

char *get_transput_buffer (int n)
{
  return ((char *) (ADDRESS (&ref_transput_buffer[n]) + 2 * SIZE_OF (A68_INT)));
}

/*!
\brief mark transput buffer as no longer in use
\param n
**/

void unblock_transput_buffer (int n)
{
  set_transput_buffer_index (n, -1);
}

/*!
\brief find first unused transput buffer (for opening a file)
\param p position in syntax tree, should not be NULL position in syntax tree, should not be NULL
\return
**/

int get_unblocked_transput_buffer (NODE_T * p)
{
  int k;
  for (k = FIXED_TRANSPUT_BUFFERS; k < MAX_TRANSPUT_BUFFER; k++) {
    if (get_transput_buffer_index (k) == -1) {
      return (k);
    }
  }
/* Oops! */
  diagnostic_node (A_RUNTIME_ERROR, p, ERROR_TOO_MANY_OPEN_FILES);
  exit_genie (p, A_RUNTIME_ERROR);
  return (-1);
}

/*!
\brief empty contents of transput buffer
\param n
**/

void reset_transput_buffer (int n)
{
  set_transput_buffer_index (n, 0);
  (get_transput_buffer (n))[0] = NULL_CHAR;
}

/*!
\brief initialise transput buffers before use
\param p position in syntax tree, should not be NULL position in syntax tree, should not be NULL
**/

void init_transput_buffers (NODE_T * p)
{
  int k;
  for (k = 0; k < MAX_TRANSPUT_BUFFER; k++) {
    ref_transput_buffer[k] = heap_generator (p, MODE (ROWS), 2 * SIZE_OF (A68_INT) + TRANSPUT_BUFFER_SIZE);
    PROTECT_SWEEP_HANDLE (&ref_transput_buffer[k]);
    set_transput_buffer_size (k, TRANSPUT_BUFFER_SIZE);
    reset_transput_buffer (k);
  }
/* Last buffers are available for FILE values. */
  for (k = FIXED_TRANSPUT_BUFFERS; k < MAX_TRANSPUT_BUFFER; k++) {
    unblock_transput_buffer (k);
  }
}

/*!
\brief make a transput buffer larger
\param p position in syntax tree, should not be NULL
\param k
\param size
**/

void enlarge_transput_buffer (NODE_T * p, int k, int size)
{
  int index = get_transput_buffer_index (k);
  char *sb_1 = get_transput_buffer (k), *sb_2;
  UP_SWEEP_SEMA;
  UNPROTECT_SWEEP_HANDLE (&ref_transput_buffer[k]);
  ref_transput_buffer[k] = heap_generator (p, MODE (ROWS), 2 * SIZE_OF (A68_INT) + size);
  PROTECT_SWEEP_HANDLE (&ref_transput_buffer[k]);
  set_transput_buffer_size (k, size);
  set_transput_buffer_index (k, index);
  sb_2 = get_transput_buffer (k);
  bufcpy (sb_2, sb_1, BUFFER_SIZE);
  DOWN_SWEEP_SEMA;
}

/*!
\brief add char to transput buffer; if the buffer is full, make it larger
\param p position in syntax tree, should not be NULL
\param k
\param ch
**/

void add_char_transput_buffer (NODE_T * p, int k, char ch)
{
  char *sb = get_transput_buffer (k);
  int size = get_transput_buffer_size (k);
  int index = get_transput_buffer_index (k);
  if (index == size - 2) {
    enlarge_transput_buffer (p, k, 10 * size /* size + TRANSPUT_BUFFER_SIZE */ );
    add_char_transput_buffer (p, k, ch);
  } else {
    sb[index] = ch;
    sb[index + 1] = NULL_CHAR;
    set_transput_buffer_index (k, index + 1);
  }
}

/*!
\brief add char[] to transput buffer
\param p position in syntax tree, should not be NULL
\param k
\param ch
\return
**/

void add_string_transput_buffer (NODE_T * p, int k, char *ch)
{
  while (ch[0] != NULL_CHAR) {
    add_char_transput_buffer (p, k, ch[0]);
    ch++;
  }
}

/*!
\brief add A68 string to transput buffer
\param p position in syntax tree, should not be NULL
\param k
\param ref
\return
**/

void add_a_string_transput_buffer (NODE_T * p, int k, BYTE_T * ref)
{
  A68_REF row = *(A68_REF *) ref;
  TEST_INIT (p, row, MODE (ROWS));
  if (row.status & INITIALISED_MASK) {
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    int size;
    GET_DESCRIPTOR (arr, tup, &row);
    size = ROW_SIZE (tup);
    if (size > 0) {
      int i;
      BYTE_T *base_address = ADDRESS (&arr->array);
      for (i = tup->lower_bound; i <= tup->upper_bound; i++) {
        int addr = INDEX_1_DIM (arr, tup, i);
        A68_CHAR *ch = (A68_CHAR *) & (base_address[addr]);
        TEST_INIT (p, *ch, MODE (CHAR));
        add_char_transput_buffer (p, k, ch->value);
      }
    }
  }
}

/*!
\brief pop A68 string and add to buffer
\param p position in syntax tree, should not be NULL
\param k
\return
**/

void add_string_from_stack_transput_buffer (NODE_T * p, int k)
{
  DECREMENT_STACK_POINTER (p, SIZE_OF (A68_REF));
  add_a_string_transput_buffer (p, k, STACK_TOP);
}

/*!
\brief pop first character from transput buffer
\param k
\return
**/

char pop_char_transput_buffer (int k)
{
  char *sb = get_transput_buffer (k);
  int index = get_transput_buffer_index (k);
  if (index <= 0) {
    return (NULL_CHAR);
  } else {
    char ch = sb[0];
    MOVE (&sb[0], &sb[1], index);
    set_transput_buffer_index (k, index - 1);
    return (ch);
  }
}

/*!
\brief add transput buffer to A68 string
\param p position in syntax tree, should not be NULL
\param ref_str
\param str
\return
**/

static void add_c_string_to_a_string (NODE_T * p, A68_REF ref_str, char *str)
{
  A68_REF a, c, d;
  A68_ARRAY *a_1, *a_3;
  A68_TUPLE *t_1, *t_3;
  int l_1, l_2, u, v;
  BYTE_T *b_1, *b_3;
  l_2 = strlen (str);
/* left part. */
  TEST_NIL (p, ref_str, MODE (REF_STRING));
  a = *(A68_REF *) ADDRESS (&ref_str);
  TEST_INIT (p, a, MODE (STRING));
  GET_DESCRIPTOR (a_1, t_1, &a);
  l_1 = ROW_SIZE (t_1);
/* Sum string. */
  c = heap_generator (p, MODE (STRING), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&c);
  d = heap_generator (p, MODE (STRING), (l_1 + l_2) * SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&d);
/* Calculate again since heap sweeper might have moved data. */
  GET_DESCRIPTOR (a_1, t_1, &a);
/* Make descriptor of new string. */
  GET_DESCRIPTOR (a_3, t_3, &c);
  a_3->dimensions = 1;
  MOID (a_3) = MODE (CHAR);
  a_3->elem_size = SIZE_OF (A68_CHAR);
  a_3->slice_offset = 0;
  a_3->field_offset = 0;
  a_3->array = d;
  t_3->lower_bound = 1;
  t_3->upper_bound = l_1 + l_2;
  t_3->shift = t_3->lower_bound;
  t_3->span = 1;
/* add strings. */
  b_1 = ADDRESS (&a_1->array);
  b_3 = ADDRESS (&a_3->array);
  u = 0;
  for (v = t_1->lower_bound; v <= t_1->upper_bound; v++) {
    MOVE ((BYTE_T *) & b_3[u], (BYTE_T *) & b_1[INDEX_1_DIM (a_1, t_1, v)], SIZE_OF (A68_CHAR));
    u += SIZE_OF (A68_CHAR);
  }
  for (v = 0; v < l_2; v++) {
    A68_CHAR ch;
    ch.status = INITIALISED_MASK;
    ch.value = str[v];
    MOVE ((BYTE_T *) & b_3[u], (BYTE_T *) & ch, SIZE_OF (A68_CHAR));
    u += SIZE_OF (A68_CHAR);
  }
  UNPROTECT_SWEEP_HANDLE (&c);
  UNPROTECT_SWEEP_HANDLE (&d);
  *(A68_REF *) ADDRESS (&ref_str) = c;
}

/*!
\brief purge buffer for file
\param p position in syntax tree, should not be NULL
\param ref_file
\param k
\return
**/

void write_purge_buffer (NODE_T * p, A68_REF ref_file, int k)
{
  A68_FILE *file = FILE_DEREF (&ref_file);
  if (IS_NIL (file->string)) {
    if (!(file->fd == STDOUT_FILENO && halt_typing)) {
      io_write_string (file->fd, get_transput_buffer (k));
    }
  } else {
    add_c_string_to_a_string (p, file->string, get_transput_buffer (k));
  }
  reset_transput_buffer (k);
}

/* Routines that involve the A68 expression stack. */

/*!
\brief print A68 string on the stack to file
\param p position in syntax tree, should not be NULL
\param ref_file
\return
**/

void genie_write_string_from_stack (NODE_T * p, A68_REF ref_file)
{
  A68_REF row;
  int size;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROWS));
  size = a68_string_size (p, row);
  if (size > 0) {
    FILE_T f = FILE_DEREF (&ref_file)->fd;
    set_transput_buffer_index (OUTPUT_BUFFER, 0);       /* discard anything in there. */
    if (get_transput_buffer_size (OUTPUT_BUFFER) < 1 + size) {
      enlarge_transput_buffer (p, OUTPUT_BUFFER, 1 + size);
    }
    io_write_string (f, a_to_c_string (p, get_transput_buffer (OUTPUT_BUFFER), row));
  }
}

/*!
\brief allocate a temporary string on the stack
\param p position in syntax tree, should not be NULL
\param size
\return
**/

char *stack_string (NODE_T * p, int size)
{
  char *new_str = (char *) STACK_TOP;
  INCREMENT_STACK_POINTER (p, size);
  if (stack_pointer > expr_stack_limit) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  FILL (new_str, NULL_CHAR, size);
  return (new_str);
}

/* Transput basic RTS routines. */

/*!
\brief REF FILE standin
\param p position in syntax tree, should not be NULL
**/

void genie_stand_in (NODE_T * p)
{
  PUSH (p, &stand_in, SIZE_OF (A68_REF));
}

/*!
\brief REF FILE standout
\param p position in syntax tree, should not be NULL
**/

void genie_stand_out (NODE_T * p)
{
  PUSH (p, &stand_out, SIZE_OF (A68_REF));
}

/*!
\brief REF FILE standback
\param p position in syntax tree, should not be NULL
**/

void genie_stand_back (NODE_T * p)
{
  PUSH (p, &stand_back, SIZE_OF (A68_REF));
}

/*!
\brief REF FILE standerror
\param p position in syntax tree, should not be NULL
\return
**/

void genie_stand_error (NODE_T * p)
{
  PUSH (p, &stand_error, SIZE_OF (A68_REF));
}

/*!
\brief CHAR error char
\param p position in syntax tree, should not be NULL
**/

void genie_error_char (NODE_T * p)
{
  PUSH_CHAR (p, ERROR_CHAR);
}

/*!
\brief CHAR exp char
\param p position in syntax tree, should not be NULL
**/

void genie_exp_char (NODE_T * p)
{
  PUSH_CHAR (p, EXPONENT_CHAR);
}

/*!
\brief CHAR flip char
\param p position in syntax tree, should not be NULL
**/

void genie_flip_char (NODE_T * p)
{
  PUSH_CHAR (p, FLIP_CHAR);
}

/*!
\brief CHAR flop char
\param p position in syntax tree, should not be NULL
**/

void genie_flop_char (NODE_T * p)
{
  PUSH_CHAR (p, FLOP_CHAR);
}

/*!
\brief CHAR null char
\param p position in syntax tree, should not be NULL
**/

void genie_null_char (NODE_T * p)
{
  PUSH_CHAR (p, NULL_CHAR);
}

/*!
\brief CHAR blank
\param p position in syntax tree, should not be NULL
**/

void genie_blank_char (NODE_T * p)
{
  PUSH_CHAR (p, BLANK_CHAR);
}

/*!
\brief CHAR newline char
\param p position in syntax tree, should not be NULL
**/

void genie_newline_char (NODE_T * p)
{
  PUSH_CHAR (p, NEWLINE_CHAR);
}

/*!
\brief CHAR formfeed char
\param p position in syntax tree, should not be NULL
**/

void genie_formfeed_char (NODE_T * p)
{
  PUSH_CHAR (p, FORMFEED_CHAR);
}

/*!
\brief CHAR tab char
\param p position in syntax tree, should not be NULL
**/

void genie_tab_char (NODE_T * p)
{
  PUSH_CHAR (p, TAB_CHAR);
}

/*!
\brief CHANNEL standin channel
\param p position in syntax tree, should not be NULL
**/

void genie_stand_in_channel (NODE_T * p)
{
  PUSH_CHANNEL (p, stand_in_channel);
}

/*!
\brief CHANNEL standout channel
\param p position in syntax tree, should not be NULL
**/

void genie_stand_out_channel (NODE_T * p)
{
  PUSH_CHANNEL (p, stand_out_channel);
}

/*!
\brief CHANNEL stand draw channel
\param p position in syntax tree, should not be NULL
**/

void genie_stand_draw_channel (NODE_T * p)
{
  PUSH_CHANNEL (p, stand_draw_channel);
}

/*!
\brief CHANNEL standback channel
\param p position in syntax tree, should not be NULL
**/

void genie_stand_back_channel (NODE_T * p)
{
  PUSH_CHANNEL (p, stand_back_channel);
}

/*!
\brief CHANNEL standerror channel
\param p position in syntax tree, should not be NULL
**/

void genie_stand_error_channel (NODE_T * p)
{
  PUSH_CHANNEL (p, stand_error_channel);
}

/*!
\brief PROC STRING program idf
\param p position in syntax tree, should not be NULL
**/

void genie_program_idf (NODE_T * p)
{
  PUSH_REF (p, c_to_a_string (p, a68_prog.files.generic_name));
}

/* FILE and CHANNEL initialisations. */

/*!
\brief set_default_mended_procedure
\param z
**/

void set_default_mended_procedure (A68_PROCEDURE * z)
{
  z->status = INITIALISED_MASK;
  z->body = NULL;
  z->environ = 0;
}

/*!
\brief init_channel
\param chan
\param r
\param s
\param g
\param p position in syntax tree, should not be NULL
\param b
\param d
**/

static void init_channel (A68_CHANNEL * chan, BOOL_T r, BOOL_T s, BOOL_T g, BOOL_T p, BOOL_T b, BOOL_T d)
{
  chan->status = INITIALISED_MASK;
  chan->reset = r;
  chan->set = s;
  chan->get = g;
  chan->put = p;
  chan->bin = b;
  chan->draw = d;
  chan->compress = A_TRUE;
}

/*!
\brief set default event handlers
\param f
**/

void set_default_mended_procedures (A68_FILE * f)
{
  set_default_mended_procedure (&(f->file_end_mended));
  set_default_mended_procedure (&(f->page_end_mended));
  set_default_mended_procedure (&(f->line_end_mended));
  set_default_mended_procedure (&(f->value_error_mended));
  set_default_mended_procedure (&(f->open_error_mended));
  set_default_mended_procedure (&(f->transput_error_mended));
  set_default_mended_procedure (&(f->format_end_mended));
  set_default_mended_procedure (&(f->format_error_mended));
}

/*!
\brief set up a REF FILE object
\param p position in syntax tree, should not be NULL
\param ref_file
\param c
\param s
\param rm
\param wm
\param cm
\return
**/

static void init_file (NODE_T * p, A68_REF * ref_file, A68_CHANNEL c, FILE_T s, BOOL_T rm, BOOL_T wm, BOOL_T cm, char *env)
{
  A68_FILE *f;
  char *filename = (env == NULL ? NULL : getenv (env));
  *ref_file = heap_generator (p, MODE (REF_FILE), SIZE_OF (A68_FILE));
  PROTECT_SWEEP_HANDLE (ref_file);
  f = (A68_FILE *) ADDRESS (ref_file);
  f->status = INITIALISED_MASK;
  f->terminator = nil_ref;
  f->channel = c;
  if (filename != NULL && strlen (filename) > 0) {
    f->identification = heap_generator (p, MODE (C_STRING), 1 + strlen (filename));
    PROTECT_SWEEP_HANDLE (&(f->identification));
    bufcpy ((char *) ADDRESS (&(f->identification)), filename, BUFFER_SIZE);
    f->fd = -1;
    f->read_mood = A_FALSE;
    f->write_mood = A_FALSE;
    f->char_mood = A_FALSE;
    f->draw_mood = A_FALSE;
  } else {
    f->identification = nil_ref;
    f->fd = s;
    f->read_mood = rm;
    f->write_mood = wm;
    f->char_mood = cm;
    f->draw_mood = A_FALSE;
  }
  f->transput_buffer = get_unblocked_transput_buffer (p);
  reset_transput_buffer (f->transput_buffer);
  f->eof = A_FALSE;
  f->tmp_file = A_FALSE;
  f->opened = A_TRUE;
  f->open_exclusive = A_FALSE;
  f->format = nil_format;
  f->string = nil_ref;
  f->strpos = 0;
  set_default_mended_procedures (f);
}

/*!
\brief initialise the transput RTL
\param p position in syntax tree, should not be NULL
\return
**/

void genie_init_transput (NODE_T * p)
{
  init_transput_buffers (p);
/* Channels. */
  init_channel (&stand_in_channel, A_FALSE, A_FALSE, A_TRUE, A_FALSE, A_FALSE, A_FALSE);
  init_channel (&stand_out_channel, A_FALSE, A_FALSE, A_FALSE, A_TRUE, A_FALSE, A_FALSE);
  init_channel (&stand_back_channel, A_TRUE, A_TRUE, A_TRUE, A_TRUE, A_TRUE, A_FALSE);
  init_channel (&stand_error_channel, A_FALSE, A_FALSE, A_FALSE, A_TRUE, A_FALSE, A_FALSE);
  init_channel (&associate_channel, A_TRUE, A_TRUE, A_TRUE, A_TRUE, A_FALSE, A_FALSE);
#ifdef HAVE_PLOTUTILS
  init_channel (&stand_draw_channel, A_FALSE, A_FALSE, A_FALSE, A_FALSE, A_FALSE, A_TRUE);
#else
  init_channel (&stand_draw_channel, A_FALSE, A_FALSE, A_FALSE, A_FALSE, A_FALSE, A_TRUE);
#endif
/* Files. */
  init_file (p, &stand_in, stand_in_channel, STDIN_FILENO, A_TRUE, A_FALSE, A_TRUE, "A68G_STANDIN");
  init_file (p, &stand_out, stand_out_channel, STDOUT_FILENO, A_FALSE, A_TRUE, A_TRUE, "A68G_STANDOUT");
  init_file (p, &stand_back, stand_back_channel, -1, A_FALSE, A_FALSE, A_FALSE, NULL);
  init_file (p, &stand_error, stand_error_channel, STDERR_FILENO, A_FALSE, A_TRUE, A_TRUE, "A68G_STANDERROR");
}

/*!
\brief PROC (REF FILE) STRING idf
\param p position in syntax tree, should not be NULL
**/

void genie_idf (NODE_T * p)
{
  A68_REF ref_file, ref_filename;
  char *filename;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  ref_file = *(A68_REF *) STACK_TOP;
  ref_filename = FILE_DEREF (&ref_file)->identification;
  TEST_INIT (p, ref_filename, MODE (ROWS));
  TEST_NIL (p, ref_filename, MODE (ROWS));
  filename = (char *) ADDRESS (&ref_filename);
  PUSH_REF (p, c_to_a_string (p, filename));
}

/*!
\brief PROC (REF FILE) STRING term
\param p position in syntax tree, should not be NULL
**/

void genie_term (NODE_T * p)
{
  A68_REF ref_file, ref_term;
  char *term;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  ref_file = *(A68_REF *) STACK_TOP;
  ref_term = FILE_DEREF (&ref_file)->terminator;
  TEST_INIT (p, ref_term, MODE (ROWS));
  TEST_NIL (p, ref_term, MODE (ROWS));
  term = (char *) ADDRESS (&ref_term);
  PUSH_REF (p, c_to_a_string (p, term));
}

/*!
\brief PROC (REF FILE, STRING) VOID make term
\param p position in syntax tree, should not be NULL
**/

void genie_make_term (NODE_T * p)
{
  int size;
  A68_FILE *file;
  A68_REF ref_file, ref_str;
  POP_REF (p, &ref_str);
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  ref_file = *(A68_REF *) STACK_TOP;
  file = FILE_DEREF (&ref_file);
/* Don't check initialisation so we can "make term" before opening.
   That is ok. */
  size = a68_string_size (p, ref_str);
  if (((file->terminator.status & INITIALISED_MASK) != 0) && !IS_NIL (file->terminator)) {
    UNPROTECT_SWEEP_HANDLE (&(file->terminator));
  }
  file->terminator = heap_generator (p, MODE (C_STRING), 1 + size);
  PROTECT_SWEEP_HANDLE (&(file->terminator));
  a_to_c_string (p, (char *) ADDRESS (&(file->terminator)), ref_str);
}

/*!
\brief PROC (REF FILE) BOOL put possible
\param p position in syntax tree, should not be NULL
**/

void genie_put_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  PUSH_BOOL (p, file->channel.put);
}

/*!
\brief PROC (REF FILE) BOOL get possible
\param p position in syntax tree, should not be NULL
**/

void genie_get_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  PUSH_BOOL (p, file->channel.get);
}

/*!
\brief PROC (REF FILE) BOOL bin possible
\param p position in syntax tree, should not be NULL
**/

void genie_bin_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  PUSH_BOOL (p, file->channel.bin);
}

/*!
\brief PROC (REF FILE) BOOL set possible
\param p position in syntax tree, should not be NULL
**/

void genie_set_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  PUSH_BOOL (p, file->channel.set);
}

/*!
\brief PROC (REF FILE) BOOL reset possible
\param p position in syntax tree, should not be NULL
**/

void genie_reset_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  PUSH_BOOL (p, file->channel.reset);
}

/*!
\brief PROC (REF FILE) BOOL compressible
\param p position in syntax tree, should not be NULL
**/

void genie_compressible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  PUSH_BOOL (p, file->channel.compress);
}

/*!
\brief PROC (REF FILE) BOOL draw possible
\param p position in syntax tree, should not be NULL
**/

void genie_draw_possible (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  PUSH_BOOL (p, file->channel.draw);
}

/*!
\brief PROC (REF FILE, STRING, CHANNEL) INT open
\param p position in syntax tree, should not be NULL
**/

void genie_open (NODE_T * p)
{
  A68_CHANNEL channel;
  A68_REF ref_iden, ref_file;
  A68_FILE *file;
  int size;
  POP (p, &channel, SIZE_OF (A68_CHANNEL));
  POP_REF (p, &ref_iden);
  TEST_NIL (p, ref_iden, MODE (REF_STRING));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  file->status = INITIALISED_MASK;
  file->channel = channel;
  file->opened = A_TRUE;
  file->open_exclusive = A_FALSE;
  file->read_mood = A_FALSE;
  file->write_mood = A_FALSE;
  file->char_mood = A_FALSE;
  file->draw_mood = A_FALSE;
  file->tmp_file = A_FALSE;
  size = a68_string_size (p, ref_iden);
  if (((file->identification.status & INITIALISED_MASK) != 0) && !IS_NIL (file->identification)) {
    UNPROTECT_SWEEP_HANDLE (&(file->identification));
  }
  file->identification = heap_generator (p, MODE (C_STRING), 1 + size);
  PROTECT_SWEEP_HANDLE (&(file->identification));
  a_to_c_string (p, (char *) ADDRESS (&(file->identification)), ref_iden);
  file->terminator = nil_ref;
  file->format = nil_format;
  file->fd = -1;
  if (((file->string.status & INITIALISED_MASK) != 0) && !IS_NIL (file->string)) {
    UNPROTECT_SWEEP_HANDLE (&(file->string));
  }
  file->string = nil_ref;
  file->strpos = 0;
  file->device.stream = NULL;
  set_default_mended_procedures (file);
  PUSH_INT (p, 0);
}

/*!
\brief PROC (REF FILE, STRING, CHANNEL) INT establish
\param p position in syntax tree, should not be NULL
\return
**/

void genie_establish (NODE_T * p)
{
  A68_CHANNEL channel;
  A68_REF ref_iden, ref_file;
  A68_FILE *file;
  int size;
  POP (p, &channel, SIZE_OF (A68_CHANNEL));
  POP_REF (p, &ref_iden);
  TEST_NIL (p, ref_iden, MODE (REF_STRING));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  file->status = INITIALISED_MASK;
  file->channel = channel;
  file->opened = A_TRUE;
  file->open_exclusive = A_TRUE;
  file->read_mood = A_FALSE;
  file->write_mood = A_FALSE;
  file->char_mood = A_FALSE;
  file->draw_mood = A_FALSE;
  file->tmp_file = A_FALSE;
  if (!file->channel.put) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  size = a68_string_size (p, ref_iden);
  if (((file->identification.status & INITIALISED_MASK) != 0) && !IS_NIL (file->identification)) {
    UNPROTECT_SWEEP_HANDLE (&(file->identification));
  }
  file->identification = heap_generator (p, MODE (C_STRING), 1 + size);
  PROTECT_SWEEP_HANDLE (&(file->identification));
  a_to_c_string (p, (char *) ADDRESS (&(file->identification)), ref_iden);
  file->terminator = nil_ref;
  file->format = nil_format;
  file->fd = -1;
  if (((file->string.status & INITIALISED_MASK) != 0) && !IS_NIL (file->string)) {
    UNPROTECT_SWEEP_HANDLE (&(file->string));
  }
  file->string = nil_ref;
  file->strpos = 0;
  file->device.stream = NULL;
  set_default_mended_procedures (file);
  PUSH_INT (p, 0);
}

/*!
\brief PROC (REF FILE, CHANNEL) INT create
\param p position in syntax tree, should not be NULL
**/

void genie_create (NODE_T * p)
{
  A68_CHANNEL channel;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &channel, SIZE_OF (A68_CHANNEL));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  file->status = INITIALISED_MASK;
  file->channel = channel;
  file->opened = A_TRUE;
  file->open_exclusive = A_FALSE;
  file->read_mood = A_FALSE;
  file->write_mood = A_FALSE;
  file->char_mood = A_FALSE;
  file->draw_mood = A_FALSE;
  file->tmp_file = A_TRUE;
  if (((file->identification.status & INITIALISED_MASK) != 0) && !IS_NIL (file->identification)) {
    UNPROTECT_SWEEP_HANDLE (&(file->identification));
  }
  file->identification = nil_ref;
  file->terminator = nil_ref;
  file->format = nil_format;
  file->fd = -1;
  if (((file->string.status & INITIALISED_MASK) != 0) && !IS_NIL (file->string)) {
    UNPROTECT_SWEEP_HANDLE (&(file->string));
  }
  file->string = nil_ref;
  file->strpos = 0;
  file->device.stream = NULL;
  set_default_mended_procedures (file);
  PUSH_INT (p, 0);
}

/*!
\brief PROC (REF FILE, REF STRING) VOID associate
\param p position in syntax tree, should not be NULL
**/

void genie_associate (NODE_T * p)
{
  A68_REF ref_string, ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_string);
  TEST_NIL (p, ref_string, MODE (REF_STRING));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  if (ref_file.segment == heap_segment && ref_string.segment != heap_segment) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, MODE (REF_STRING));
    exit_genie (p, A_RUNTIME_ERROR);
  } else if (ref_file.segment == frame_segment && ref_string.segment == frame_segment && ref_string.scope > ref_file.scope) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_SCOPE_DYNAMIC_1, MODE (REF_STRING));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  file = FILE_DEREF (&ref_file);
  file->status = INITIALISED_MASK;
  file->channel = associate_channel;
  file->opened = A_TRUE;
  file->open_exclusive = A_FALSE;
  file->read_mood = A_FALSE;
  file->write_mood = A_FALSE;
  file->char_mood = A_FALSE;
  file->draw_mood = A_FALSE;
  file->tmp_file = A_FALSE;
  if (((file->identification.status & INITIALISED_MASK) != 0) && !IS_NIL (file->identification)) {
    UNPROTECT_SWEEP_HANDLE (&(file->identification));
  }
  file->identification = nil_ref;
  file->terminator = nil_ref;
  file->format = nil_format;
  file->fd = -1;
  if (((file->string.status & INITIALISED_MASK) != 0) && !IS_NIL (file->string)) {
    UNPROTECT_SWEEP_HANDLE (&(file->string));
  }
  file->string = ref_string;
  PROTECT_SWEEP_HANDLE (&(file->string));
  file->strpos = 1;
  file->device.stream = NULL;
  set_default_mended_procedures (file);
}

/*!
\brief PROC (REF FILE) VOID close
\param p position in syntax tree, should not be NULL
**/

void genie_close (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened || (!file->read_mood && !file->write_mood && !file->draw_mood)) {
    return;
  }
  file->device.device_made = A_FALSE;
#ifdef HAVE_PLOTUTILS
  if (file->device.device_opened) {
    close_device (p, file);
    file->device.stream = NULL;
    return;
  }
#endif
  if (file->fd != -1 && close (file->fd) == -1) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_CLOSE);
    exit_genie (p, A_RUNTIME_ERROR);
  } else {
    file->fd = -1;
    file->opened = A_FALSE;
    unblock_transput_buffer (file->transput_buffer);
    set_default_mended_procedures (file);
  }
  if (file->tmp_file) {
/* Remove the file if it is temporary. */
    if (!IS_NIL (file->identification)) {
      char *filename;
      TEST_INIT (p, file->identification, MODE (ROWS));
      filename = (char *) ADDRESS (&(file->identification));
      if (remove (filename) != 0) {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_SCRATCH);
        exit_genie (p, A_RUNTIME_ERROR);
      }
      UNPROTECT_SWEEP_HANDLE (&(file->identification));
      file->identification = nil_ref;
    }
  }
}

/*!
\brief PROC (REF FILE) VOID lock
\param p position in syntax tree, should not be NULL
**/

void genie_lock (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened || (!file->read_mood && !file->write_mood && !file->draw_mood)) {
    return;
  }
  file->device.device_made = A_FALSE;
#ifdef HAVE_PLOTUTILS
  if (file->device.device_opened) {
    close_device (p, file);
    file->device.stream = NULL;
    return;
  }
#endif
#if !defined WIN32_VERSION
  RESET_ERRNO;
  fchmod (file->fd, (mode_t) 0x0);
  ABNORMAL_END (errno != 0, "cannot lock file", NULL);
#endif
  if (file->fd != -1 && close (file->fd) == -1) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_LOCK);
    exit_genie (p, A_RUNTIME_ERROR);
  } else {
    file->fd = -1;
    file->opened = A_FALSE;
    unblock_transput_buffer (file->transput_buffer);
    set_default_mended_procedures (file);
  }
  if (file->tmp_file) {
/* Remove the file if it is temporary. */
    if (!IS_NIL (file->identification)) {
      char *filename;
      TEST_INIT (p, file->identification, MODE (ROWS));
      filename = (char *) ADDRESS (&(file->identification));
      if (remove (filename) != 0) {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_SCRATCH);
        exit_genie (p, A_RUNTIME_ERROR);
      }
      UNPROTECT_SWEEP_HANDLE (&(file->identification));
      file->identification = nil_ref;
    }
  }
}

/*!
\brief PROC (REF FILE) VOID erase
\param p position in syntax tree, should not be NULL
**/

void genie_erase (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened || (!file->read_mood && !file->write_mood && !file->draw_mood)) {
    return;
  }
  file->device.device_made = A_FALSE;
#ifdef HAVE_PLOTUTILS
  if (file->device.device_opened) {
    close_device (p, file);
    file->device.stream = NULL;
    return;
  }
#endif
  if (file->fd != -1 && close (file->fd) == -1) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_SCRATCH);
    exit_genie (p, A_RUNTIME_ERROR);
  } else {
    file->fd = -1;
    file->opened = A_FALSE;
    unblock_transput_buffer (file->transput_buffer);
    set_default_mended_procedures (file);
  }
/* Remove the file. */
  if (!IS_NIL (file->identification)) {
    char *filename;
    TEST_INIT (p, file->identification, MODE (ROWS));
    filename = (char *) ADDRESS (&(file->identification));
    if (remove (filename) != 0) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_SCRATCH);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    UNPROTECT_SWEEP_HANDLE (&(file->identification));
    file->identification = nil_ref;
  }
}

/*!
\brief PROC (REF FILE) VOID reset
\param p position in syntax tree, should not be NULL
**/

void genie_reset (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!IS_NIL (file->string)) {
    file->strpos = 1;
  } else if (file->fd != -1 && close (file->fd) == -1) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_RESET);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  file->read_mood = A_FALSE;
  file->write_mood = A_FALSE;
  file->char_mood = A_FALSE;
  file->draw_mood = A_FALSE;
  file->fd = -1;
  set_default_mended_procedures (file);
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on file end
\param p position in syntax tree, should not be NULL
**/

void genie_on_file_end (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  file->file_end_mended = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on page end
\param p position in syntax tree, should not be NULL
**/

void genie_on_page_end (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  file->page_end_mended = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on line end
\param p position in syntax tree, should not be NULL
**/

void genie_on_line_end (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  file->line_end_mended = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on format end
\param p position in syntax tree, should not be NULL
**/

void genie_on_format_end (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  file->format_end_mended = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on format error
\param p position in syntax tree, should not be NULL
**/

void genie_on_format_error (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  file->format_error_mended = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on value error
\param p position in syntax tree, should not be NULL
**/

void genie_on_value_error (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  file->value_error_mended = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on open error
\param p position in syntax tree, should not be NULL
**/

void genie_on_open_error (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  file->open_error_mended = z;
}

/*!
\brief PROC (REF FILE, PROC (REF FILE) BOOL) VOID on transput error
\param p position in syntax tree, should not be NULL
**/

void genie_on_transput_error (NODE_T * p)
{
  A68_PROCEDURE z;
  A68_REF ref_file;
  A68_FILE *file;
  POP (p, &z, SIZE_OF (A68_PROCEDURE));
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  file->transput_error_mended = z;
}

/*!
\brief invoke event routine
\param p position in syntax tree, should not be NULL
\param z
\param ref_file
**/

void on_event_handler (NODE_T * p, A68_PROCEDURE z, A68_REF ref_file)
{
  if (z.body == NULL) {
/* Default procedure. */
    PUSH_BOOL (p, A_FALSE);
  } else {
    ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
    MOID_T *u = MODE (PROC_REF_FILE_BOOL);
    PUSH_REF_FILE (p, ref_file);
    genie_call_procedure (p, u, u, u, &z, pop_sp, pop_fp);
  }
}

/*!
\brief handle end-of-file event
\param p position in syntax tree, should not be NULL
\param ref_file
**/

void end_of_file_error (NODE_T * p, A68_REF ref_file)
{
  A68_BOOL z;
  on_event_handler (p, FILE_DEREF (&ref_file)->file_end_mended, ref_file);
  POP_BOOL (p, &z);
  if (z.value == A_FALSE) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_ENDED);
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief handle file-open-error event
\param p position in syntax tree, should not be NULL
\param ref_file
\param mode
**/

void open_error (NODE_T * p, A68_REF ref_file, char *mode)
{
  A68_BOOL z;
  on_event_handler (p, FILE_DEREF (&ref_file)->open_error_mended, ref_file);
  POP_BOOL (p, &z);
  if (z.value == A_FALSE) {
    A68_FILE *file;
    char *filename;
    TEST_NIL (p, ref_file, MODE (REF_FILE));
    file = FILE_DEREF (&ref_file);
    TEST_INIT (p, *file, MODE (FILE));
    if (!IS_NIL (file->identification)) {
      filename = (char *) ADDRESS (&(FILE_DEREF (&ref_file)->identification));
    } else {
      filename = "(NIL filename)";
    }
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_CANNOT_OPEN_FOR, filename, mode);
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief handle value_error event
\param p position in syntax tree, should not be NULL
\param m
\param ref_file
**/

void value_error (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  if (f->eof) {
    end_of_file_error (p, ref_file);
  } else {
    A68_BOOL z;
    on_event_handler (p, f->value_error_mended, ref_file);
    POP_BOOL (p, &z);
    if (z.value == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_TRANSPUT, m);
      exit_genie (p, A_RUNTIME_ERROR);
    }
  }
}

/*!
\brief handle transput-error event
\param p position in syntax tree, should not be NULL
\param ref_file
\param m
**/

void transput_error (NODE_T * p, A68_REF ref_file, MOID_T * m)
{
  A68_BOOL z;
  on_event_handler (p, FILE_DEREF (&ref_file)->transput_error_mended, ref_file);
  POP_BOOL (p, &z);
  if (z.value == A_FALSE) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_TRANSPUT, m);
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/* Implementation of put and get. */

/*!
\brief get next char from file
\param f
\return
**/

int char_scanner (A68_FILE * f)
{
  if (get_transput_buffer_index (f->transput_buffer) > 0) {
/* There are buffered characters. */
    f->eof = A_FALSE;
    return (pop_char_transput_buffer (f->transput_buffer));
  } else if (IS_NIL (f->string)) {
/* Fetch next CHAR from the FILE. */
    ssize_t chars_read;
    char ch;
    chars_read = io_read_conv (f->fd, &ch, 1);
    if (chars_read == 1) {
      f->eof = A_FALSE;
      return (ch);
    } else {
      f->eof = A_TRUE;
      return (EOF_CHAR);
    }
  } else {
/* File is associated with a STRING. Give next CHAR. When we're outside the
   STRING give EOF_CHAR. */
    A68_REF z = *(A68_REF *) (ADDRESS (&f->string));    /* Dereference REF STRING. */
    A68_ARRAY *a;
    A68_TUPLE *t;
    BYTE_T *base;
    A68_CHAR *ch;
    GET_DESCRIPTOR (a, t, &z);
    if (f->strpos < t->lower_bound || f->strpos > t->upper_bound) {
      f->eof = A_TRUE;
      return (EOF_CHAR);
    }
    base = ADDRESS (&(a->array));
    ch = (A68_CHAR *) & (base[INDEX_1_DIM (a, t, f->strpos)]);
    f->strpos++;
    return (ch->value);
  }
}

/*!
\brief push back look-ahead character to file
\param p position in syntax tree, should not be NULL
\param f
\param ch
**/

void unchar_scanner (NODE_T * p, A68_FILE * f, char ch)
{
  f->eof = A_FALSE;
  add_char_transput_buffer (p, f->transput_buffer, ch);
}

/*!
\brief PROC (REF FILE) VOID new line
\param p position in syntax tree, should not be NULL
**/

void genie_new_line (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->write_mood) {
    if (IS_NIL (file->string)) {
      io_write_string (file->fd, NEWLINE_STRING);
    } else {
      add_c_string_to_a_string (p, file->string, NEWLINE_STRING);
    }
  } else if (file->read_mood) {
    BOOL_T go_on = !file->eof;
    while (go_on) {
      int ch = char_scanner (file);
      go_on = (ch != NEWLINE_CHAR) && (ch != EOF_CHAR) && !file->eof;
    }
  } else {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief PROC (REF FILE) VOID new page
\param p position in syntax tree, should not be NULL
\return
**/

void genie_new_page (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->write_mood) {
    if (IS_NIL (file->string)) {
      io_write_string (file->fd, "\f");
    } else {
      add_c_string_to_a_string (p, file->string, "\f");
    }
  } else if (file->read_mood) {
    BOOL_T go_on = !file->eof;
    while (go_on) {
      int ch = char_scanner (file);
      go_on = (ch != FORMFEED_CHAR) && (ch != EOF_CHAR) && !file->eof;
    }
  } else {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief PROC (REF FILE) VOID space
\param p position in syntax tree, should not be NULL
**/

void genie_space (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->write_mood) {
    io_write_string (file->fd, " ");
  } else if (file->read_mood) {
    if (!file->eof) {
      (void) char_scanner (file);
    }
  } else {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "undetermined");
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

#define IS_NL_FF(ch) ((ch) == NEWLINE_CHAR || (ch) == FORMFEED_CHAR)

/*!
\brief skip new-lines and form-feeds
\param p position in syntax tree, should not be NULL
\param ch
\param ref_file
**/

void skip_nl_ff (NODE_T * p, int *ch, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  while ((*ch) != EOF_CHAR && IS_NL_FF (*ch)) {
    A68_BOOL *z = (A68_BOOL *) STACK_TOP;
    ADDR_T pop_sp = stack_pointer;
    if (*ch == NEWLINE_CHAR) {
      on_event_handler (p, f->line_end_mended, ref_file);
    } else if (*ch == FORMFEED_CHAR) {
      on_event_handler (p, f->page_end_mended, ref_file);
    }
    stack_pointer = pop_sp;
    if (z->value == A_FALSE) {
      (*ch) = char_scanner (f);
    }
  }
}

/*!
\brief scan an int from file
\param p position in syntax tree, should not be NULL
\param ref_file
**/

void scan_integer (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int ch;
  reset_transput_buffer (INPUT_BUFFER);
  ch = char_scanner (f);
  while (ch != EOF_CHAR && (IS_SPACE (ch) || IS_NL_FF (ch))) {
    if (IS_NL_FF (ch)) {
      skip_nl_ff (p, &ch, ref_file);
    } else {
      ch = char_scanner (f);
    }
  }
  if (ch != EOF_CHAR && (ch == '+' || ch == '-')) {
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
    ch = char_scanner (f);
  }
  while (ch != EOF_CHAR && IS_DIGIT (ch)) {
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
    ch = char_scanner (f);
  }
  if (ch != EOF_CHAR) {
    unchar_scanner (p, f, ch);
  }
}

/*!
\brief scan a real from file
\param p position in syntax tree, should not be NULL
\param ref_file
**/

void scan_real (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  char x_e = EXPONENT_CHAR;
  int ch;
  reset_transput_buffer (INPUT_BUFFER);
  ch = char_scanner (f);
  while (ch != EOF_CHAR && (IS_SPACE (ch) || IS_NL_FF (ch))) {
    if (IS_NL_FF (ch)) {
      skip_nl_ff (p, &ch, ref_file);
    } else {
      ch = char_scanner (f);
    }
  }
  if (ch != EOF_CHAR && (ch == '+' || ch == '-')) {
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
    ch = char_scanner (f);
  }
  while (ch != EOF_CHAR && IS_DIGIT (ch)) {
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
    ch = char_scanner (f);
  }
  if (ch == EOF_CHAR || !(ch == POINT_CHAR || TO_UPPER (ch) == TO_UPPER (x_e))) {
    goto salida;
  }
  if (ch == POINT_CHAR) {
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
    ch = char_scanner (f);
    while (ch != EOF_CHAR && IS_DIGIT (ch)) {
      add_char_transput_buffer (p, INPUT_BUFFER, ch);
      ch = char_scanner (f);
    }
  }
  if (ch == EOF_CHAR || TO_UPPER (ch) != TO_UPPER (x_e)) {
    goto salida;
  }
  if (TO_UPPER (ch) == TO_UPPER (x_e)) {
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
    ch = char_scanner (f);
    while (ch != EOF_CHAR && ch == BLANK_CHAR) {
      ch = char_scanner (f);
    }
    if (ch != EOF_CHAR && (ch == '+' || ch == '-')) {
      add_char_transput_buffer (p, INPUT_BUFFER, ch);
      ch = char_scanner (f);
    }
    while (ch != EOF_CHAR && IS_DIGIT (ch)) {
      add_char_transput_buffer (p, INPUT_BUFFER, ch);
      ch = char_scanner (f);
    }
  }
salida:
  if (ch != EOF_CHAR) {
    unchar_scanner (p, f, ch);
  }
}

/*!
\brief scan a bits from file
\param p position in syntax tree, should not be NULL
\param ref_file
**/

void scan_bits (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int ch, flip = FLIP_CHAR, flop = FLOP_CHAR;
  reset_transput_buffer (INPUT_BUFFER);
  ch = char_scanner (f);
  while (ch != EOF_CHAR && (IS_SPACE (ch) || IS_NL_FF (ch))) {
    if (IS_NL_FF (ch)) {
      skip_nl_ff (p, &ch, ref_file);
    } else {
      ch = char_scanner (f);
    }
  }
  while (ch != EOF_CHAR && (ch == flip || ch == flop)) {
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
    ch = char_scanner (f);
  }
  if (ch != EOF_CHAR) {
    unchar_scanner (p, f, ch);
  }
}

/*!
\brief scan a char from file
\param p position in syntax tree, should not be NULL
\param ref_file
**/

void scan_char (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int ch;
  reset_transput_buffer (INPUT_BUFFER);
  ch = char_scanner (f);
  skip_nl_ff (p, &ch, ref_file);
  if (ch != EOF_CHAR) {
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
  }
}

/*!
\brief scan a string from file
\param p position in syntax tree, should not be NULL
\param term
\param ref_file
**/

void scan_string (NODE_T * p, char *term, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  if (f->eof) {
    end_of_file_error (p, ref_file);
  } else {
    int ch = char_scanner (f);
    BOOL_T go_on = A_TRUE;
    reset_transput_buffer (INPUT_BUFFER);
    while (go_on) {
      if (ch == EOF_CHAR) {
        go_on = A_FALSE;
      } else if (term != NULL && a68g_strchr (term, ch) != NULL) {
        go_on = A_FALSE;
      } else if (IS_NL_FF (ch)) {
        A68_BOOL *z = (A68_BOOL *) STACK_TOP;
        ADDR_T pop_sp = stack_pointer;
        if (ch == NEWLINE_CHAR) {
          on_event_handler (p, f->line_end_mended, ref_file);
        } else if (ch == FORMFEED_CHAR) {
          on_event_handler (p, f->page_end_mended, ref_file);
        }
        stack_pointer = pop_sp;
        if (z->value == A_TRUE) {
          ch = char_scanner (f);
        } else {
          go_on = A_FALSE;
        }
      } else {
        add_char_transput_buffer (p, INPUT_BUFFER, ch);
        ch = char_scanner (f);
      }
    }
    if (ch != EOF_CHAR) {
      unchar_scanner (p, f, ch);
    } else if (get_transput_buffer_index (INPUT_BUFFER) == 0) {
      end_of_file_error (p, ref_file);
    }
  }
}

/*!
\brief open a file, or establish it
\param p position in syntax tree, should not be NULL
\param ref_file
\param mode required access
\param acc
\return
**/

FILE_T open_physical_file (NODE_T * p, A68_REF ref_file, int mode, mode_t acc)
{
  A68_FILE *file;
  A68_REF ref_filename;
  char *filename;
  (void) acc;
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!IS_NIL (file->string)) {
/* Associated file. */
    file->transput_buffer = get_unblocked_transput_buffer (p);
    reset_transput_buffer (file->transput_buffer);
    file->eof = A_FALSE;
    return (file->fd);
  } else if (IS_NIL (file->identification)) {
/* No identification, so generate a unique identification.
  "tmpnam" is not safe, "mkstemp" is Unix, so a68g brings its own tmpnam. */
#define TMP_SIZE 8
#define TRIALS 32
    char filename[TMP_SIZE + 32];
    char *letters = "0123456789abcdefghijklmnopqrstuvwxyz";
    int k, len = strlen (letters);
    BOOL_T good_file = A_FALSE;
    for (k = 0; k < TRIALS && good_file == A_FALSE; k++) {
      int j, index;
      bufcpy (filename, A68G_NAME, BUFFER_SIZE);
      bufcat (filename, ".", BUFFER_SIZE);
      for (j = 0; j < TMP_SIZE; j++) {
        char chars[2];
        do {
          index = (int) (rng_53_bit () * len);
        }
        while (index < 0 || index >= len);
        chars[0] = letters[index];
        chars[1] = NULL_CHAR;
        bufcat (filename, chars, BUFFER_SIZE);
      }
      bufcat (filename, ".tmp", BUFFER_SIZE);
      RESET_ERRNO;
      file->fd = open (filename, mode | O_EXCL, acc);
      good_file = (file->fd != -1 && errno == 0);
    }
#undef TMP_SIZE
#undef TRIALS
    if (good_file == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NO_TEMP);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    file->identification = heap_generator (p, MODE (C_STRING), 1 + strlen (filename));
    PROTECT_SWEEP_HANDLE (&(file->identification));
    bufcpy ((char *) ADDRESS (&(file->identification)), filename, BUFFER_SIZE);
    file->transput_buffer = get_unblocked_transput_buffer (p);
    reset_transput_buffer (file->transput_buffer);
    file->eof = A_FALSE;
    file->tmp_file = A_TRUE;
    return (file->fd);
  } else {
/* Opening an identified file. */
    ref_filename = file->identification;
    TEST_INIT (p, ref_filename, MODE (ROWS));
    TEST_NIL (p, ref_filename, MODE (ROWS));
    filename = (char *) ADDRESS (&ref_filename);
    if (file->open_exclusive) {
/* Establishing requires that the file does not exist. */
      if (mode == (A_WRITE_ACCESS)) {
        mode |= O_EXCL;
      }
      file->open_exclusive = A_FALSE;
    }
    file->fd = open (filename, mode, acc);
    file->transput_buffer = get_unblocked_transput_buffer (p);
    reset_transput_buffer (file->transput_buffer);
    file->eof = A_FALSE;
    return (file->fd);
  }
}

/*!
\brief call PROC (REF FILE) VOID during transput
\param p position in syntax tree, should not be NULL
\param ref_file
\param z
\return
**/

void genie_call_proc_ref_file_void (NODE_T * p, A68_REF ref_file, A68_PROCEDURE z)
{
  ADDR_T pop_sp = stack_pointer, pop_fp = frame_pointer;
  MOID_T *u = MODE (PROC_REF_FILE_VOID);
  PUSH_REF_FILE (p, ref_file);
  genie_call_procedure (p, u, u, u, &z, pop_sp, pop_fp);
  stack_pointer = pop_sp;       /* VOIDING. */
}

/* Unformatted transput. */

/*!
\brief char_value
\param ch
\return
**/

static int char_value (int ch)
{
  switch (ch) {
  case '0':
    {
      return (0);
    }
  case '1':
    {
      return (1);
    }
  case '2':
    {
      return (2);
    }
  case '3':
    {
      return (3);
    }
  case '4':
    {
      return (4);
    }
  case '5':
    {
      return (5);
    }
  case '6':
    {
      return (6);
    }
  case '7':
    {
      return (7);
    }
  case '8':
    {
      return (8);
    }
  case '9':
    {
      return (9);
    }
  case 'A':
  case 'a':
    {
      return (10);
    }
  case 'B':
  case 'b':
    {
      return (11);
    }
  case 'C':
  case 'c':
    {
      return (12);
    }
  case 'D':
  case 'd':
    {
      return (13);
    }
  case 'E':
  case 'e':
    {
      return (14);
    }
  case 'F':
  case 'f':
    {
      return (15);
    }
  default:
    {
      return (-1);
    }
  }
}

/*!
\brief own strtoul; some systems have no strtoul
\param str a string representing a valid unsigned int, empty or NULL
\param end
\param base
\return
**/

unsigned long a68g_strtoul (char *str, char **end, int base)
{
  if (str == NULL || str[0] == NULL_CHAR) {
    (*end) = NULL;
    errno = EDOM;
    return (0);
  } else {
    int j, k = 0, start;
    char *q = str;
    unsigned long mul = 1, sum = 0;
    while (IS_SPACE (q[k])) {
      k++;
    }
    if (q[k] == '+') {
      k++;
    }
    start = k;
    while (IS_XDIGIT (q[k])) {
      k++;
    }
    if (k == start) {
      if (end != NULL) {
        *end = str;
      }
      errno = EDOM;
      return (0);
    }
    if (end != NULL) {
      (*end) = &q[k];
    }
    for (j = k - 1; j >= start; j--) {
      if (char_value (q[j]) >= base) {
        errno = EDOM;
        return (0);
      } else {
        unsigned add = char_value (q[j]) * mul;
        if (MAX_UNT - sum >= add) {
          sum += add;
          mul *= base;
        } else {
          errno = ERANGE;
          return (0);
        }
      }
    }
    return (sum);
  }
}

/*!
\brief BITS value of BITS denoter
\param p position in syntax tree, should not be NULL
\param a
\return
**/

static unsigned bits_to_int (NODE_T * p, char *a)
{
  int base = 0;
  unsigned bits = 0;
  char *radix = NULL, *end = NULL;
  RESET_ERRNO;
  base = a68g_strtoul (a, &radix, 10);
  if (radix != NULL && TO_UPPER (radix[0]) == TO_UPPER (RADIX_CHAR) && errno == 0) {
    if (base < 2 || base > 16) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, base);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    bits = a68g_strtoul (&(radix[1]), &end, base);
    if (end != NULL && end[0] == NULL_CHAR && errno == 0) {
      return (bits);
    }
  }
  diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, MODE (BITS));
  exit_genie (p, A_RUNTIME_ERROR);
  return (0);
}

/*!
\brief LONG BITS value of LONG BITS denoter
\param p position in syntax tree, should not be NULL
\param z
\param a
\param m
**/

static void long_bits_to_long_int (NODE_T * p, MP_DIGIT_T * z, char *a, MOID_T * m)
{
  int base = 0;
  char *radix = NULL;
  RESET_ERRNO;
  base = a68g_strtoul (a, &radix, 10);
  if (radix != NULL && TO_UPPER (radix[0]) == TO_UPPER (RADIX_CHAR) && errno == 0) {
    int digits = get_mp_digits (m);
    ADDR_T pop_sp = stack_pointer;
    MP_DIGIT_T *v;
    MP_DIGIT_T *w;
    char *q = radix;
    STACK_MP (v, p, digits);
    STACK_MP (w, p, digits);
    while (q[0] != NULL_CHAR) {
      q++;
    }
    SET_MP_ZERO (z, digits);
    set_mp_short (w, (MP_DIGIT_T) 1, 0, digits);
    if (base < 2 || base > 16) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, base);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    while ((--q) != radix) {
      int digit = char_value (q[0]);
      if (digit >= 0 && digit < base) {
        mul_mp_digit (p, v, w, (MP_DIGIT_T) digit, digits);
        add_mp (p, z, z, v, digits);
      } else {
        diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, m);
        exit_genie (p, A_RUNTIME_ERROR);
      }
      mul_mp_digit (p, w, w, (MP_DIGIT_T) base, digits);
    }
    check_long_bits_value (p, z, m);
    stack_pointer = pop_sp;
  } else {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, m);
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief convert string to required mode and store
\param p position in syntax tree, should not be NULL
\param m
\param a
\param item
\return
**/

BOOL_T genie_string_to_value_internal (NODE_T * p, MOID_T * m, char *a, BYTE_T * item)
{
  RESET_ERRNO;
/* strto.. does not mind empty strings. */
  if (strlen (a) == 0) {
    return (A_FALSE);
  }
  if (m == MODE (INT)) {
    A68_INT *z = (A68_INT *) item;
    char *end;
    z->value = strtol (a, &end, 10);
    if (end[0] == NULL_CHAR && errno == 0) {
      z->status = INITIALISED_MASK;
      return (A_TRUE);
    } else {
      return (A_FALSE);
    }
  } else if (m == MODE (REAL)) {
    A68_REAL *z = (A68_REAL *) item;
    char *end;
    z->value = strtod (a, &end);
    if (end[0] == NULL_CHAR && errno == 0) {
      z->status = INITIALISED_MASK;
      return (A_TRUE);
    } else {
      return (A_FALSE);
    }
  } else if (m == MODE (LONG_INT) || m == MODE (LONGLONG_INT)) {
    int digits = get_mp_digits (m);
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    if (string_to_mp (p, z, a, digits) == NULL) {
      return (A_FALSE);
    }
    if (!check_mp_int (z, m)) {
      errno = ERANGE;
      return (A_FALSE);
    }
    MP_STATUS (z) = INITIALISED_MASK;
    return (A_TRUE);
  } else if (m == MODE (LONG_REAL) || m == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (m);
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    if (string_to_mp (p, z, a, digits) == NULL) {
      return (A_FALSE);
    }
    MP_STATUS (z) = INITIALISED_MASK;
    return (A_TRUE);
  } else if (m == MODE (BOOL)) {
    A68_BOOL *z = (A68_BOOL *) item;
    char q = a[0], flip = FLIP_CHAR, flop = FLOP_CHAR;
    if (q == flip || q == flop) {
      z->value = (q == flip);
      z->status = INITIALISED_MASK;
      return (A_TRUE);
    } else {
      return (A_FALSE);
    }
  } else if (m == MODE (BITS)) {
    A68_BITS *z = (A68_BITS *) item;
    int status = A_TRUE;
    if (a[0] == FLIP_CHAR || a[0] == FLOP_CHAR) {
/* [] BOOL denotation is "TTFFFFTFT ..." */
      if (strlen (a) > (size_t) BITS_WIDTH) {
        errno = ERANGE;
        status = A_FALSE;
      } else {
        int j = strlen (a) - 1;
        unsigned k = 0x1;
        z->value = 0;
        for (; j >= 0; j--) {
          if (a[j] == FLIP_CHAR) {
            z->value += k;
          } else if (a[j] != FLOP_CHAR) {
            status = A_FALSE;
          }
          k <<= 1;
        }
      }
    } else {
/* BITS denotation is also allowed. */
      z->value = bits_to_int (p, a);
    }
    if (errno != 0 || status == A_FALSE) {
      return (A_FALSE);
    }
    z->status = INITIALISED_MASK;
    return (A_TRUE);
  } else if (m == MODE (LONG_BITS) || m == MODE (LONGLONG_BITS)) {
    int digits = get_mp_digits (m);
    int status = A_TRUE;
    ADDR_T pop_sp = stack_pointer;
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    if (a[0] == FLIP_CHAR || a[0] == FLOP_CHAR) {
/* [] BOOL denotation is "TTFFFFTFT ..." */
      if (strlen (a) > (size_t) BITS_WIDTH) {
        errno = ERANGE;
        status = A_FALSE;
      } else {
        int j;
        MP_DIGIT_T *w;
        STACK_MP (w, p, digits);
        SET_MP_ZERO (z, digits);
        set_mp_short (w, (MP_DIGIT_T) 1, 0, digits);
        for (j = strlen (a) - 1; j >= 0; j--) {
          if (a[j] == FLIP_CHAR) {
            add_mp (p, z, z, w, digits);
          } else if (a[j] != FLOP_CHAR) {
            status = A_FALSE;
          }
          mul_mp_digit (p, w, w, (MP_DIGIT_T) 2, digits);
        }
      }
    } else {
/* BITS denotation is also allowed. */
      long_bits_to_long_int (p, z, a, m);
    }
    stack_pointer = pop_sp;
    if (errno != 0 || status == A_FALSE) {
      return (A_FALSE);
    }
    MP_STATUS (z) = INITIALISED_MASK;
    return (A_TRUE);
  }
  return (A_FALSE);
}

/*!
\brief convert string in input buffer to value of required mode
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
\return
**/

void genie_string_to_value (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  char *str = get_transput_buffer (INPUT_BUFFER);
  RESET_ERRNO;
  add_char_transput_buffer (p, INPUT_BUFFER, NULL_CHAR);        /* end string, just in case. */
  if (mode == MODE (INT)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (REAL)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (BOOL)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (BITS)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    if (genie_string_to_value_internal (p, mode, str, item) == A_FALSE) {
      value_error (p, mode, ref_file);
    }
  } else if (mode == MODE (CHAR)) {
    A68_CHAR *z = (A68_CHAR *) item;
    if (str[0] == NULL_CHAR) {
      value_error (p, mode, ref_file);
    } else {
      int len = strlen (str);
      if (len == 0 || len > 1) {
        value_error (p, mode, ref_file);
      }
      z->value = str[0];
      z->status = INITIALISED_MASK;
    }
  } else if (mode == MODE (STRING)) {
    A68_REF z;
    z = c_to_a_string (p, str);
    *(A68_REF *) item = z;
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief read object from file
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

void genie_read_standard (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  RESET_ERRNO;
  if (mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    scan_integer (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    scan_real (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (BOOL)) {
    scan_char (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (CHAR)) {
    scan_char (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (BITS) || mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    scan_bits (p, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (mode == MODE (STRING)) {
    char *term = (char *) ADDRESS (&f->terminator);
    scan_string (p, term, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (WHETHER (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NULL; FORWARD (q)) {
      genie_read_standard (p, MOID (q), &item[q->offset], ref_file);
    }
  } else if (WHETHER (mode, UNION_SYMBOL)) {
    A68_POINTER *z = (A68_POINTER *) item;
    if (!(z->status | INITIALISED_MASK) || z->value == NULL) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE, mode);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    genie_read_standard (p, (MOID_T *) (z->value), &item[SIZE_OF (A68_POINTER)], ref_file);
  } else if (WHETHER (mode, ROW_SYMBOL) || WHETHER (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    TEST_INIT (p, *(A68_REF *) item, MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, arr->dimensions) != 0) {
      BYTE_T *base_addr = ADDRESS (&arr->array);
      BOOL_T done = A_FALSE;
      initialise_internal_index (tup, arr->dimensions);
      while (!done) {
        ADDR_T index = calculate_internal_index (tup, arr->dimensions);
        ADDR_T elem_addr = ROW_ELEMENT (arr, index);
        genie_read_standard (p, SUB (deflexed), &base_addr[elem_addr], ref_file);
        done = increment_internal_index (tup, arr->dimensions);
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief PROC ([] SIMPLIN) VOID read
\param p position in syntax tree, should not be NULL
**/

void genie_read (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_in (p);
  PUSH_REF (p, row);
  genie_read_file (p);
}

/*!
\brief PROC (REF FILE, [] SIMPLIN) VOID get
\param p position in syntax tree, should not be NULL
**/

void genie_read_file (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROW_SIMPLIN));
  TEST_NIL (p, row, MODE (ROW_SIMPLIN));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->write_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.get) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "getting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->read_mood && !file->write_mood) {
    if (IS_NIL (file->string)) {
      if ((file->fd = open_physical_file (p, ref_file, A_READ_ACCESS, 0)) == -1) {
        open_error (p, ref_file, "getting");
      }
    } else {
      file->fd = open_physical_file (p, ref_file, A_READ_ACCESS, 0);
    }
    file->draw_mood = A_FALSE;
    file->read_mood = A_TRUE;
    file->write_mood = A_FALSE;
    file->char_mood = A_TRUE;
  }
  if (!file->char_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Read. */
  base_address = ADDRESS (&arr->array);
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_POINTER *z = (A68_POINTER *) & base_address[elem_index];
    MOID_T *mode = (MOID_T *) (z->value);
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + SIZE_OF (A68_POINTER)];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      /*
       * ignore. 
       */ ;
    } else {
      if (file->eof) {
        end_of_file_error (p, ref_file);
      }
      TEST_NIL (p, *(A68_REF *) item, mode);
      genie_read_standard (p, SUB (mode), ADDRESS ((A68_REF *) item), ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLIN));
  }
}

/*!
\brief convert value to string
\param p position in syntax tree, should not be NULL
\param moid
\param item
**/

void genie_value_to_string (NODE_T * p, MOID_T * moid, BYTE_T * item)
{
  if (moid == MODE (INT)) {
    A68_INT *z = (A68_INT *) item;
    PUSH_POINTER (p, MODE (INT));
    PUSH_INT (p, z->value);
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + SIZE_OF (A68_INT)));
    PUSH_INT (p, INT_WIDTH + 1);
    genie_whole (p);
  } else if (moid == MODE (LONG_INT)) {
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    PUSH_POINTER (p, MODE (LONG_INT));
    PUSH (p, z, get_mp_size (MODE (LONG_INT)));
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + get_mp_size (MODE (LONG_INT))));
    PUSH_INT (p, LONG_WIDTH + 1);
    genie_whole (p);
  } else if (moid == MODE (LONGLONG_INT)) {
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    PUSH_POINTER (p, MODE (LONGLONG_INT));
    PUSH (p, z, get_mp_size (MODE (LONGLONG_INT)));
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + get_mp_size (MODE (LONGLONG_INT))));
    PUSH_INT (p, LONGLONG_WIDTH + 1);
    genie_whole (p);
  } else if (moid == MODE (REAL)) {
    A68_REAL *z = (A68_REAL *) item;
    PUSH_POINTER (p, MODE (REAL));
    PUSH_REAL (p, z->value);
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + SIZE_OF (A68_REAL)));
    PUSH_INT (p, REAL_WIDTH + EXP_WIDTH + 4);
    PUSH_INT (p, REAL_WIDTH - 1);
    PUSH_INT (p, EXP_WIDTH + 1);
    genie_float (p);
  } else if (moid == MODE (LONG_REAL)) {
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    PUSH_POINTER (p, MODE (LONG_REAL));
    PUSH (p, z, get_mp_size (MODE (LONG_REAL)));
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + get_mp_size (MODE (LONG_REAL))));
    PUSH_INT (p, LONG_REAL_WIDTH + LONG_EXP_WIDTH + 4);
    PUSH_INT (p, LONG_REAL_WIDTH - 1);
    PUSH_INT (p, LONG_EXP_WIDTH + 1);
    genie_float (p);
  } else if (moid == MODE (LONGLONG_REAL)) {
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    PUSH_POINTER (p, MODE (LONGLONG_REAL));
    PUSH (p, z, get_mp_size (MODE (LONGLONG_REAL)));
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + get_mp_size (MODE (LONGLONG_REAL))));
    PUSH_INT (p, LONGLONG_REAL_WIDTH + LONGLONG_EXP_WIDTH + 4);
    PUSH_INT (p, LONGLONG_REAL_WIDTH - 1);
    PUSH_INT (p, LONGLONG_EXP_WIDTH + 1);
    genie_float (p);
  } else if (moid == MODE (BITS)) {
    A68_BITS *z = (A68_BITS *) item;
    char *str = stack_string (p, 8 + BITS_WIDTH);
    unsigned bit = 0x1;
    int j;
    for (j = 1; j < BITS_WIDTH; j++) {
      bit <<= 1;
    }
    for (j = 0; j < BITS_WIDTH; j++) {
      str[j] = (z->value & bit) ? FLIP_CHAR : FLOP_CHAR;
      bit >>= 1;
    }
    str[j] = NULL_CHAR;
  } else if (moid == MODE (LONG_BITS) || moid == MODE (LONGLONG_BITS)) {
    int bits = get_mp_bits_width (moid), word = get_mp_bits_words (moid);
    int cher = bits;
    char *str = stack_string (p, 8 + bits);
    ADDR_T pop_sp = stack_pointer;
    unsigned *row = stack_mp_bits (p, (MP_DIGIT_T *) item, moid);
    str[cher--] = NULL_CHAR;
    while (cher >= 0) {
      unsigned bit = 0x1;
      int j;
      for (j = 0; j < MP_BITS_BITS && cher >= 0; j++) {
        str[cher--] = (row[word - 1] & bit) ? FLIP_CHAR : FLOP_CHAR;
        bit <<= 1;
      }
      word--;
    }
    stack_pointer = pop_sp;
  }
}

/*!
\brief print object to file
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
\return
**/

void genie_write_standard (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  RESET_ERRNO;
  if (mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    genie_value_to_string (p, mode, item);
    add_string_from_stack_transput_buffer (p, UNFORMATTED_BUFFER);
  } else if (mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    genie_value_to_string (p, mode, item);
    add_string_from_stack_transput_buffer (p, UNFORMATTED_BUFFER);
  } else if (mode == MODE (BOOL)) {
    A68_BOOL *z = (A68_BOOL *) item;
    char flipflop = z->value == A_TRUE ? FLIP_CHAR : FLOP_CHAR;
    add_char_transput_buffer (p, UNFORMATTED_BUFFER, flipflop);
  } else if (mode == MODE (CHAR)) {
    A68_CHAR *ch = (A68_CHAR *) item;
    add_char_transput_buffer (p, UNFORMATTED_BUFFER, ch->value);
  } else if (mode == MODE (BITS) || mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    char *str = (char *) STACK_TOP;
    genie_value_to_string (p, mode, item);
    add_string_transput_buffer (p, UNFORMATTED_BUFFER, str);
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
/* Handle these separately since this is faster than straightening. */
    add_a_string_transput_buffer (p, UNFORMATTED_BUFFER, item);
  } else if (WHETHER (mode, UNION_SYMBOL)) {
    A68_POINTER *z = (A68_POINTER *) item;
    genie_write_standard (p, (MOID_T *) (z->value), &item[SIZE_OF (A68_POINTER)], ref_file);
  } else if (WHETHER (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NULL; FORWARD (q)) {
      BYTE_T *elem = &item[q->offset];
      GENIE_CHECK_INITIALISATION (p, elem, MOID (q));
      genie_write_standard (p, MOID (q), elem, ref_file);
    }
  } else if (WHETHER (mode, ROW_SYMBOL) || WHETHER (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    TEST_INIT (p, *(A68_REF *) item, MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, arr->dimensions) != 0) {
      BYTE_T *base_addr = ADDRESS (&arr->array);
      BOOL_T done = A_FALSE;
      initialise_internal_index (tup, arr->dimensions);
      while (!done) {
        ADDR_T index = calculate_internal_index (tup, arr->dimensions);
        ADDR_T elem_addr = ROW_ELEMENT (arr, index);
        BYTE_T *elem = &base_addr[elem_addr];
        GENIE_CHECK_INITIALISATION (p, elem, SUB (deflexed));
        genie_write_standard (p, SUB (deflexed), elem, ref_file);
        done = increment_internal_index (tup, arr->dimensions);
      }
    }
  }
  if (errno != 0) {
    ABNORMAL_END (IS_NIL (ref_file), "conversion error: ", strerror (errno));
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief PROC ([] SIMPLOUT) VOID print, write
\param p position in syntax tree, should not be NULL
**/

void genie_write (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_out (p);
  PUSH_REF (p, row);
  genie_write_file (p);
}

/*!
\brief PROC (REF FILE, [] SIMPLOUT) VOID put
\param p position in syntax tree, should not be NULL
\return
**/

void genie_write_file (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROW_SIMPLOUT));
  TEST_NIL (p, row, MODE (ROW_SIMPLOUT));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->read_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "read");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.put) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->read_mood && !file->write_mood) {
    if (IS_NIL (file->string)) {
      if ((file->fd = open_physical_file (p, ref_file, A_WRITE_ACCESS, A68_PROTECTION)) == -1) {
        open_error (p, ref_file, "putting");
      }
    } else {
      file->fd = open_physical_file (p, ref_file, A_READ_ACCESS, 0);
    }
    file->draw_mood = A_FALSE;
    file->read_mood = A_FALSE;
    file->write_mood = A_TRUE;
    file->char_mood = A_TRUE;
  }
  if (!file->char_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  base_address = ADDRESS (&(arr->array));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_POINTER *z = (A68_POINTER *) & (base_address[elem_index]);
    MOID_T *mode = (MOID_T *) (z->value);
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + SIZE_OF (A68_POINTER)];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      /*
       * ignore. 
       */ ;
    } else {
      reset_transput_buffer (UNFORMATTED_BUFFER);
      genie_write_standard (p, mode, item, ref_file);
      write_purge_buffer (p, ref_file, UNFORMATTED_BUFFER);
    }
    elem_index += MOID_SIZE (MODE (SIMPLOUT));
  }
}

/*!
\brief read object binary from file
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void genie_read_bin_standard (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  RESET_ERRNO;
  if (mode == MODE (INT)) {
    A68_INT *z = (A68_INT *) item;
    io_read (f->fd, &(z->value), SIZE_OF (z->value));
    z->status = INITIALISED_MASK;
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    io_read (f->fd, z, get_mp_size (mode));
    MP_STATUS (z) = INITIALISED_MASK;
  } else if (mode == MODE (REAL)) {
    A68_REAL *z = (A68_REAL *) item;
    io_read (f->fd, &(z->value), SIZE_OF (z->value));
    z->status = INITIALISED_MASK;
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    io_read (f->fd, z, get_mp_size (mode));
    MP_STATUS (z) = INITIALISED_MASK;
  } else if (mode == MODE (BOOL)) {
    A68_BOOL *z = (A68_BOOL *) item;
    io_read (f->fd, &(z->value), SIZE_OF (z->value));
    z->status = INITIALISED_MASK;
  } else if (mode == MODE (CHAR)) {
    A68_CHAR *z = (A68_CHAR *) item;
    io_read (f->fd, &(z->value), SIZE_OF (z->value));
    z->status = INITIALISED_MASK;
  } else if (mode == MODE (BITS)) {
    A68_BITS *z = (A68_BITS *) item;
    io_read (f->fd, &(z->value), SIZE_OF (z->value));
    z->status = INITIALISED_MASK;
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    MP_DIGIT_T *z = (MP_DIGIT_T *) item;
    io_read (f->fd, z, get_mp_size (mode));
    MP_STATUS (z) = INITIALISED_MASK;
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
    char *term = (char *) ADDRESS (&f->terminator);
    scan_string (p, term, ref_file);
    genie_string_to_value (p, mode, item, ref_file);
  } else if (WHETHER (mode, UNION_SYMBOL)) {
    A68_POINTER *z = (A68_POINTER *) item;
    if (!(z->status | INITIALISED_MASK) || z->value == NULL) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_EMPTY_VALUE, mode);
      exit_genie (p, A_RUNTIME_ERROR);
    }
    genie_read_bin_standard (p, (MOID_T *) (z->value), &item[SIZE_OF (A68_POINTER)], ref_file);
  } else if (WHETHER (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NULL; FORWARD (q)) {
      genie_read_bin_standard (p, MOID (q), &item[q->offset], ref_file);
    }
  } else if (WHETHER (mode, ROW_SYMBOL) || WHETHER (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    TEST_INIT (p, *(A68_REF *) item, MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, arr->dimensions) != 0) {
      BYTE_T *base_addr = ADDRESS (&arr->array);
      BOOL_T done = A_FALSE;
      initialise_internal_index (tup, arr->dimensions);
      while (!done) {
        ADDR_T index = calculate_internal_index (tup, arr->dimensions);
        ADDR_T elem_addr = ROW_ELEMENT (arr, index);
        genie_read_bin_standard (p, SUB (deflexed), &base_addr[elem_addr], ref_file);
        done = increment_internal_index (tup, arr->dimensions);
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief PROC ([] SIMPLIN) VOID read bin
\param p position in syntax tree, should not be NULL
\return
**/

void genie_read_bin (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROW_SIMPLIN));
  TEST_NIL (p, row, MODE (ROW_SIMPLIN));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  ref_file = stand_back;
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->write_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.get) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "getting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.bin) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "binary getting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->read_mood && !file->write_mood) {
    if ((file->fd = open_physical_file (p, ref_file, A_READ_ACCESS | O_BINARY, 0)) == -1) {
      open_error (p, ref_file, "binary getting");
    }
    file->draw_mood = A_FALSE;
    file->read_mood = A_TRUE;
    file->write_mood = A_FALSE;
    file->char_mood = A_FALSE;
  }
  if (file->char_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "text");
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Read. */
  elem_index = 0;
  base_address = ADDRESS (&(arr->array));
  for (k = 0; k < elems; k++) {
    A68_POINTER *z = (A68_POINTER *) & base_address[elem_index];
    MOID_T *mode = (MOID_T *) (z->value);
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + SIZE_OF (A68_POINTER)];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      /*
       * ignore. 
       */ ;
    } else {
      if (file->eof) {
        end_of_file_error (p, ref_file);
      }
      TEST_NIL (p, *(A68_REF *) item, mode);
      genie_read_bin_standard (p, SUB (mode), ADDRESS ((A68_REF *) item), ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLIN));
  }
}

/*!
\brief PROC (REF FILE, [] SIMPLIN) VOID get bin
\param p position in syntax tree, should not be NULL
\return
**/

void genie_read_bin_file (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROW_SIMPLIN));
  TEST_NIL (p, row, MODE (ROW_SIMPLIN));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  ref_file = *(A68_REF *) STACK_TOP;
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->write_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.get) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "getting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.bin) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "binary getting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->read_mood && !file->write_mood) {
    if ((file->fd = open_physical_file (p, ref_file, A_READ_ACCESS | O_BINARY, 0)) == -1) {
      open_error (p, ref_file, "binary getting");
    }
    file->draw_mood = A_FALSE;
    file->read_mood = A_TRUE;
    file->write_mood = A_FALSE;
    file->char_mood = A_FALSE;
  }
  if (file->char_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "text");
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Read. */
  elem_index = 0;
  base_address = ADDRESS (&(arr->array));
  for (k = 0; k < elems; k++) {
    A68_POINTER *z = (A68_POINTER *) & base_address[elem_index];
    MOID_T *mode = (MOID_T *) (z->value);
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + SIZE_OF (A68_POINTER)];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      /*
       * ignore. 
       */ ;
    } else {
      if (file->eof) {
        end_of_file_error (p, ref_file);
      }
      TEST_NIL (p, *(A68_REF *) item, mode);
      genie_read_bin_standard (p, SUB (mode), ADDRESS ((A68_REF *) item), ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLIN));
  }
}

/*!
\brief write object binary to file
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void genie_write_bin_standard (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  RESET_ERRNO;
  if (mode == MODE (INT)) {
    io_write (f->fd, &(((A68_INT *) item)->value), SIZE_OF (((A68_INT *) item)->value));
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    io_write (f->fd, (MP_DIGIT_T *) item, get_mp_size (mode));
  } else if (mode == MODE (REAL)) {
    io_write (f->fd, &(((A68_REAL *) item)->value), SIZE_OF (((A68_REAL *) item)->value));
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    io_write (f->fd, (MP_DIGIT_T *) item, get_mp_size (mode));
  } else if (mode == MODE (BOOL)) {
    io_write (f->fd, &(((A68_BOOL *) item)->value), SIZE_OF (((A68_BOOL *) item)->value));
  } else if (mode == MODE (CHAR)) {
    io_write (f->fd, &(((A68_CHAR *) item)->value), SIZE_OF (((A68_CHAR *) item)->value));
  } else if (mode == MODE (BITS)) {
    io_write (f->fd, &(((A68_BITS *) item)->value), SIZE_OF (((A68_BITS *) item)->value));
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    io_write (f->fd, (MP_DIGIT_T *) item, get_mp_size (mode));
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
    reset_transput_buffer (UNFORMATTED_BUFFER);
    add_a_string_transput_buffer (p, UNFORMATTED_BUFFER, item);
    io_write_string (f->fd, get_transput_buffer (UNFORMATTED_BUFFER));
  } else if (WHETHER (mode, UNION_SYMBOL)) {
    A68_POINTER *z = (A68_POINTER *) item;
    genie_write_bin_standard (p, (MOID_T *) (z->value), &item[SIZE_OF (A68_POINTER)], ref_file);
  } else if (WHETHER (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NULL; FORWARD (q)) {
      BYTE_T *elem = &item[q->offset];
      GENIE_CHECK_INITIALISATION (p, elem, MOID (q));
      genie_write_bin_standard (p, MOID (q), elem, ref_file);
    }
  } else if (WHETHER (mode, ROW_SYMBOL) || WHETHER (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    TEST_INIT (p, *(A68_REF *) item, MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, arr->dimensions) != 0) {
      BYTE_T *base_addr = ADDRESS (&arr->array);
      BOOL_T done = A_FALSE;
      initialise_internal_index (tup, arr->dimensions);
      while (!done) {
        ADDR_T index = calculate_internal_index (tup, arr->dimensions);
        ADDR_T elem_addr = ROW_ELEMENT (arr, index);
        BYTE_T *elem = &base_addr[elem_addr];
        GENIE_CHECK_INITIALISATION (p, elem, SUB (deflexed));
        genie_write_bin_standard (p, SUB (deflexed), elem, ref_file);
        done = increment_internal_index (tup, arr->dimensions);
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief PROC ([] SIMPLOUT) VOID write bin, print bin
\param p position in syntax tree, should not be NULL
\return
**/

void genie_write_bin (NODE_T * p)
{
  A68_REF ref_file, row;
  A68_FILE *file;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROW_SIMPLOUT));
  TEST_NIL (p, row, MODE (ROW_SIMPLOUT));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  ref_file = stand_back;
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->read_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "read");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.put) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.bin) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "binary putting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->read_mood && !file->write_mood) {
    if ((file->fd = open_physical_file (p, ref_file, A_WRITE_ACCESS | O_BINARY, A68_PROTECTION)) == -1) {
      open_error (p, ref_file, "binary putting");
    }
    file->draw_mood = A_FALSE;
    file->read_mood = A_FALSE;
    file->write_mood = A_TRUE;
    file->char_mood = A_FALSE;
  }
  if (file->char_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "text");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  base_address = ADDRESS (&arr->array);
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_POINTER *z = (A68_POINTER *) & base_address[elem_index];
    MOID_T *mode = (MOID_T *) (z->value);
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + SIZE_OF (A68_POINTER)];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      /*
       * ignore. 
       */ ;
    } else {
      genie_write_bin_standard (p, mode, item, ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLOUT));
  }
}

/*!
\brief PROC (REF FILE, [] SIMPLOUT) VOID put bin
\param p position in syntax tree, should not be NULL
\return
**/

void genie_write_bin_file (NODE_T * p)
{
  A68_REF ref_file, row;
  A68_FILE *file;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROW_SIMPLOUT));
  TEST_NIL (p, row, MODE (ROW_SIMPLOUT));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  ref_file = *(A68_REF *) STACK_TOP;
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->read_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "read");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.put) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.bin) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "binary putting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->read_mood && !file->write_mood) {
    if ((file->fd = open_physical_file (p, ref_file, A_WRITE_ACCESS | O_BINARY, A68_PROTECTION)) == -1) {
      open_error (p, ref_file, "binary putting");
    }
    file->draw_mood = A_FALSE;
    file->read_mood = A_FALSE;
    file->write_mood = A_TRUE;
    file->char_mood = A_FALSE;
  }
  if (file->char_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "text");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  base_address = ADDRESS (&arr->array);
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_POINTER *z = (A68_POINTER *) & base_address[elem_index];
    MOID_T *mode = (MOID_T *) (z->value);
    BYTE_T *item = (BYTE_T *) & base_address[elem_index + SIZE_OF (A68_POINTER)];
    if (mode == MODE (PROC_REF_FILE_VOID)) {
      genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
    } else if (mode == MODE (FORMAT)) {
      /*
       * ignore. 
       */ ;
    } else {
      genie_write_bin_standard (p, mode, item, ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLOUT));
  }
}

/*
Next are formatting routines "whole", "fixed" and "float" for mode
INT, LONG INT and LONG LONG INT, and REAL, LONG REAL and LONG LONG REAL.
They are direct implementations of the routines described in the
Revised Report, although those were only meant as a specification.

The rest of Algol68G should only reference "genie_whole", "genie_fixed"
or "genie_float" since internal routines like "sub_fixed" may leave the
stack corrupted when called directly.
*/

/*!
\brief generate a string of error chars
\param s
\param n
\return
**/

char *error_chars (char *s, int n)
{
  int k = (n != 0 ? ABS (n) : 1);
  s[k] = NULL_CHAR;
  while (--k >= 0) {
    s[k] = ERROR_CHAR;
  }
  return (s);
}

/*!
\brief convert temporary C string to A68 string
\param p position in syntax tree, should not be NULL
\param temp_string
\return
**/

A68_REF tmp_to_a68_string (NODE_T * p, char *temp_string)
{
  A68_REF z;
/* no compaction allowed since temp_string might be up for sweeping ... */
  UP_SWEEP_SEMA;
  z = c_to_a_string (p, temp_string);
  DOWN_SWEEP_SEMA;
  return (z);
}

/*!
\brief add c to str, assuming that "str" is large enough
\param c
\param str
\return
**/

static char *plusto (char c, char *str)
{
  MOVE (&str[1], &str[0], (unsigned) (strlen (str) + 1));
  str[0] = c;
  return (str);
}

/*!
\brief add c to str, assuming that "str" is large enough
\param str
\param c
\return
**/

char *string_plusab_char (char *str, char c, int strwid)
{
  char z[2];
  z[0] = c;
  z[1] = NULL_CHAR;
  bufcat (str, z, strwid);
  return (str);
}

/*!
\brief add leading spaces to str until length is width
\param str
\param width
\return
**/

static char *leading_spaces (char *str, int width)
{
  int j = width - strlen (str);
  while (--j >= 0) {
    plusto (BLANK_CHAR, str);
  }
  return (str);
}

/*!
\brief convert int to char using a table
\param k
\return
**/

static char digchar (int k)
{
  char *s = "0123456789abcdef";
  if (k >= 0 && k < (int) strlen (s)) {
    return (s[k]);
  } else {
    return (ERROR_CHAR);
  }
}

/*!
\brief standard string for LONG INT
\param p position in syntax tree, should not be NULL
\param n
\param digits
\param width
\return
**/

char *long_sub_whole (NODE_T * p, MP_DIGIT_T * n, int digits, int width)
{
  char *s = stack_string (p, 8 + width);
  int len = 0;
  s[0] = NULL_CHAR;
  do {
    if (len < width) {
/* Sic transit gloria mundi. */
      int n_mod_10 = (int) MP_DIGIT (n, (int) (1 + MP_EXPONENT (n))) % 10;
      plusto (digchar (n_mod_10), s);
    }
    len++;
    over_mp_digit (p, n, n, (MP_DIGIT_T) 10, digits);
  }
  while (MP_DIGIT (n, 1) > 0);
  if (len > width) {
    error_chars (s, width);
  }
  return (s);
}

/*!
\brief standard string for INT
\param p position in syntax tree, should not be NULL
\param n
\param width
\return
**/

char *sub_whole (NODE_T * p, int n, int width)
{
  char *s = stack_string (p, 8 + width);
  int len = 0;
  s[0] = NULL_CHAR;
  do {
    if (len < width) {
      plusto (digchar (n % 10), s);
    }
    len++;
    n /= 10;
  }
  while (n != 0);
  if (len > width) {
    error_chars (s, width);
  }
  return (s);
}

/*!
\brief formatted string for NUMBER
\param p position in syntax tree, should not be NULL
\return
**/

char *whole (NODE_T * p)
{
  int pop_sp, arg_sp;
  A68_INT width;
  MOID_T *mode;
  POP_INT (p, &width);
  arg_sp = stack_pointer;
  DECREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
  pop_sp = stack_pointer;
  mode = (MOID_T *) (((A68_POINTER *) STACK_TOP)->value);
  if (mode == MODE (INT)) {
    int x = ((A68_INT *) (STACK_OFFSET (SIZE_OF (A68_POINTER))))->value;
    int length = ABS (width.value) - (x < 0 || width.value > 0 ? 1 : 0);
    int n = ABS (x);
    int size = (x < 0 ? 1 : (width.value > 0 ? 1 : 0));
    char *s;
    if (width.value == 0) {
      int m = n;
      length = 0;
      while ((m /= 10, length++, m != 0)) {
        ;
      }
    }
    size += length;
    size = 8 + (size > width.value ? size : width.value);
    s = stack_string (p, size);
    bufcpy (s, sub_whole (p, n, length), BUFFER_SIZE);
    if (length == 0 || a68g_strchr (s, ERROR_CHAR) != NULL) {
      error_chars (s, width.value);
    } else {
      if (x < 0) {
        plusto ('-', s);
      } else if (width.value > 0) {
        plusto ('+', s);
      }
      if (width.value != 0) {
        leading_spaces (s, ABS (width.value));
      }
    }
    return (s);
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    int digits = get_mp_digits (mode);
    int length, size;
    char *s;
    MP_DIGIT_T *n = (MP_DIGIT_T *) (STACK_OFFSET (SIZE_OF (A68_POINTER)));
    BOOL_T ltz;
    stack_pointer = arg_sp;     /* We keep the mp where it's at. */
    if (MP_EXPONENT (n) >= digits) {
      int max_length = (mode == MODE (LONG_INT) ? LONG_INT_WIDTH : LONGLONG_INT_WIDTH);
      int length = (width.value == 0 ? max_length : width.value);
      s = stack_string (p, 1 + length);
      error_chars (s, length);
      return (s);
    }
    ltz = MP_DIGIT (n, 1) < 0;
    length = ABS (width.value) - (ltz || width.value > 0 ? 1 : 0);
    size = (ltz ? 1 : (width.value > 0 ? 1 : 0));
    MP_DIGIT (n, 1) = ABS (MP_DIGIT (n, 1));
    if (width.value == 0) {
      MP_DIGIT_T *m;
      STACK_MP (m, p, digits);
      MOVE_MP (m, n, digits);
      length = 0;
      while ((over_mp_digit (p, m, m, (MP_DIGIT_T) 10, digits), length++, MP_DIGIT (m, 1) != 0)) {
        ;
      }
    }
    size += length;
    size = 8 + (size > width.value ? size : width.value);
    s = stack_string (p, size);
    bufcpy (s, long_sub_whole (p, n, digits, length), BUFFER_SIZE);
    if (length == 0 || a68g_strchr (s, ERROR_CHAR) != NULL) {
      error_chars (s, width.value);
    } else {
      if (ltz) {
        plusto ('-', s);
      } else if (width.value > 0) {
        plusto ('+', s);
      }
      if (width.value != 0) {
        leading_spaces (s, ABS (width.value));
      }
    }
    return (s);
  } else if (mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
    PUSH_INT (p, width.value);
    PUSH_INT (p, 0);
    return (fixed (p));
  }
  return (NULL);
}

/*!
\brief fetch next digit from LONG
\param p position in syntax tree, should not be NULL
\param y
\param digits
\return
**/

static char long_choose_dig (NODE_T * p, MP_DIGIT_T * y, int digits)
{
/* Assuming positive "y" */
  int pop_sp = stack_pointer, c;
  MP_DIGIT_T *t;
  STACK_MP (t, p, digits);
  mul_mp_digit (p, y, y, (MP_DIGIT_T) 10, digits);
  c = MP_EXPONENT (y) == 0 ? (int) MP_DIGIT (y, 1) : 0;
  if (c > 9) {
    c = 9;
  }
  set_mp_short (t, (MP_DIGIT_T) c, 0, digits);
  sub_mp (p, y, y, t, digits);
/* Reset the stack to prevent overflow, there may be many digits. */
  stack_pointer = pop_sp;
  return (digchar (c));
}

/*!
\brief standard string for LONG
\param p position in syntax tree, should not be NULL
\param x
\param digits
\param width
\param after
\return
**/

char *long_sub_fixed (NODE_T * p, MP_DIGIT_T * x, int digits, int width, int after)
{
  int strwid = 8 + width;
  char *str = stack_string (p, strwid);
  int before = 0, j, len, pop_sp = stack_pointer;
  BOOL_T overflow;
  MP_DIGIT_T *y;
  MP_DIGIT_T *s;
  MP_DIGIT_T *t;
  STACK_MP (y, p, digits);
  STACK_MP (s, p, digits);
  STACK_MP (t, p, digits);
  set_mp_short (t, (MP_DIGIT_T) (MP_RADIX / 10), -1, digits);
  pow_mp_int (p, t, t, after, digits);
  div_mp_digit (p, t, t, (MP_DIGIT_T) 2, digits);
  add_mp (p, y, x, t, digits);
  set_mp_short (s, (MP_DIGIT_T) 1, 0, digits);
  while ((sub_mp (p, t, y, s, digits), MP_DIGIT (t, 1) >= 0)) {
    before++;
    mul_mp_digit (p, s, s, (MP_DIGIT_T) 10, digits);
  }
  div_mp (p, y, y, s, digits);
  str[0] = NULL_CHAR;
  len = 0;
  overflow = A_FALSE;
  for (j = 0; j < before && !overflow; j++) {
    if (!(overflow = len >= width)) {
      string_plusab_char (str, long_choose_dig (p, y, digits), strwid);
      len++;
    }
  }
  if (after > 0 && !(overflow = len >= width)) {
    string_plusab_char (str, POINT_CHAR, strwid);
  }
  for (j = 0; j < after && !overflow; j++) {
    if (!(overflow = len >= width)) {
      string_plusab_char (str, long_choose_dig (p, y, digits), strwid);
      len++;
    }
  }
  if (overflow || (int) strlen (str) > width) {
    error_chars (str, width);
  }
  stack_pointer = pop_sp;
  return (str);
}

/*!
\brief fetch next digit from REAL
\param y
\return
**/

static char choose_dig (double *y)
{
/* Assuming positive "y" */
  int c = (int) (*y *= 10);
  if (c > 9) {
    c = 9;
  }
  *y -= c;
  return (digchar (c));
}

/*!
\brief standard string for REAL
\param p position in syntax tree, should not be NULL
\param x
\param width
\param after
\return
**/

char *sub_fixed (NODE_T * p, double x, int width, int after)
{
  int strwid = 8 + width;
  char *str = stack_string (p, strwid);
  int before = 0, j, len, expo;
  BOOL_T overflow;
  double y, z;
/* Round and scale. */
  z = y = x + 0.5 * ten_to_the_power (-after);
  expo = 0;
  while (z >= 1) {
    expo++;
    z /= 10;
  }
  before += expo;
/* Trick to avoid overflow. */
  if (expo > 30) {
    expo -= 30;
    y /= ten_to_the_power (30);
  }
/* Scale number. */
  y /= ten_to_the_power (expo);
  len = 0;
/* Put digits, prevent garbage from overstretching precision. */
  overflow = A_FALSE;
  for (j = 0; j < before && !overflow; j++) {
    if (!(overflow = len >= width)) {
      char ch = (len < REAL_WIDTH ? choose_dig (&y) : '0');
      string_plusab_char (str, ch, strwid);
      len++;
    }
  }
  if (after > 0 && !(overflow = len >= width)) {
    string_plusab_char (str, POINT_CHAR, strwid);
  }
  for (j = 0; j < after && !overflow; j++) {
    if (!(overflow = len >= width)) {
      char ch = (len < REAL_WIDTH ? choose_dig (&y) : '0');
      string_plusab_char (str, ch, strwid);
      len++;
    }
  }
  if (overflow || (int) strlen (str) > width) {
    error_chars (str, width);
  }
  return (str);
}

/*!
\brief formatted string for NUMBER
\param p position in syntax tree, should not be NULL
\return
**/

char *fixed (NODE_T * p)
{
  A68_INT width, after;
  MOID_T *mode;
  int pop_sp, arg_sp;
  POP_INT (p, &after);
  POP_INT (p, &width);
  arg_sp = stack_pointer;
  DECREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
  mode = (MOID_T *) (((A68_POINTER *) STACK_TOP)->value);
  pop_sp = stack_pointer;
  if (mode == MODE (REAL)) {
    double x = ((A68_REAL *) (STACK_OFFSET (SIZE_OF (A68_POINTER))))->value;
    int length = ABS (width.value) - (x < 0 || width.value > 0 ? 1 : 0);
    char *s;
    stack_pointer = arg_sp;
    if (after.value >= 0 && (length > after.value || width.value == 0)) {
      double y = ABS (x), z0, z1;
      if (width.value == 0) {
        length = (after.value == 0 ? 1 : 0);
        z0 = ten_to_the_power (-after.value);
        z1 = ten_to_the_power (length);
/*
	pow_real_int (p, &z0, 0.1, after.value);
	pow_real_int (p, &z1, 10.0, length);
*/
        while (y + 0.5 * z0 > z1) {
          length++;
          z1 *= 10.0;
        }
        length += (after.value == 0 ? 0 : after.value + 1);
      }
      s = stack_string (p, 8 + length);
      s = sub_fixed (p, y, length, after.value);
      if (a68g_strchr (s, ERROR_CHAR) == NULL) {
        if (length > (int) strlen (s) && (s[0] != NULL_CHAR ? s[0] == POINT_CHAR : A_TRUE) && y < 1.0) {
          plusto ('0', s);
        }
        if (x < 0) {
          plusto ('-', s);
        } else if (width.value > 0) {
          plusto ('+', s);
        }
        if (width.value != 0) {
          leading_spaces (s, ABS (width.value));
        }
        return (s);
      } else if (after.value > 0) {
        stack_pointer = arg_sp;
        PUSH_INT (p, width.value);
        PUSH_INT (p, after.value - 1);
        return (fixed (p));
      } else {
        return (error_chars (s, width.value));
      }
    } else {
      char *s = stack_string (p, 8 + ABS (width.value));
      return (error_chars (s, width.value));
    }
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (mode);
    int length;
    BOOL_T ltz;
    char *s;
    MP_DIGIT_T *x = (MP_DIGIT_T *) (STACK_OFFSET (SIZE_OF (A68_POINTER)));
    stack_pointer = arg_sp;
    ltz = MP_DIGIT (x, 1) < 0;
    MP_DIGIT (x, 1) = ABS (MP_DIGIT (x, 1));
    length = ABS (width.value) - (ltz || width.value > 0 ? 1 : 0);
    if (after.value >= 0 && (length > after.value || width.value == 0)) {
      MP_DIGIT_T *z0;
      MP_DIGIT_T *z1;
      MP_DIGIT_T *t;
      STACK_MP (z0, p, digits);
      STACK_MP (z1, p, digits);
      STACK_MP (t, p, digits);
      if (width.value == 0) {
        length = (after.value == 0 ? 1 : 0);
        set_mp_short (z0, (MP_DIGIT_T) (MP_RADIX / 10), -1, digits);
        set_mp_short (z1, (MP_DIGIT_T) 10, 0, digits);
        pow_mp_int (p, z0, z0, after.value, digits);
        pow_mp_int (p, z1, z1, length, digits);
        while ((div_mp_digit (p, t, z0, (MP_DIGIT_T) 2, digits), add_mp (p, t, x, t, digits), sub_mp (p, t, t, z1, digits), MP_DIGIT (t, 1) > 0)) {
          length++;
          mul_mp_digit (p, z1, z1, (MP_DIGIT_T) 10, digits);
        }
        length += (after.value == 0 ? 0 : after.value + 1);
      }
      s = stack_string (p, 8 + length);
      s = long_sub_fixed (p, x, digits, length, after.value);
      if (a68g_strchr (s, ERROR_CHAR) == NULL) {
        if (length > (int) strlen (s) && (s[0] != NULL_CHAR ? s[0] == POINT_CHAR : A_TRUE) && (MP_EXPONENT (x) < 0 || MP_DIGIT (x, 1) == 0)) {
          plusto ('0', s);
        }
        if (ltz) {
          plusto ('-', s);
        } else if (width.value > 0) {
          plusto ('+', s);
        }
        if (width.value != 0) {
          leading_spaces (s, ABS (width.value));
        }
        return (s);
      } else if (after.value > 0) {
        stack_pointer = arg_sp;
        MP_DIGIT (x, 1) = ltz ? -ABS (MP_DIGIT (x, 1)) : ABS (MP_DIGIT (x, 1));
        PUSH_INT (p, width.value);
        PUSH_INT (p, after.value - 1);
        return (fixed (p));
      } else {
        return (error_chars (s, width.value));
      }
    } else {
      char *s = stack_string (p, 8 + ABS (width.value));
      return (error_chars (s, width.value));
    }
  } else if (mode == MODE (INT)) {
    int x = ((A68_INT *) (STACK_OFFSET (SIZE_OF (A68_POINTER))))->value;
    PUSH_POINTER (p, MODE (REAL));
    PUSH_REAL (p, (double) x);
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + SIZE_OF (A68_REAL)));
    PUSH_INT (p, width.value);
    PUSH_INT (p, after.value);
    return (fixed (p));
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    stack_pointer = pop_sp;
    if (mode == MODE (LONG_INT)) {
      ((A68_POINTER *) STACK_TOP)->value = (void *) MODE (LONG_REAL);
    } else {
      ((A68_POINTER *) STACK_TOP)->value = (void *) MODE (LONGLONG_REAL);
    }
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
    PUSH_INT (p, width.value);
    PUSH_INT (p, after.value);
    return (fixed (p));
  }
  return (NULL);
}

/*!
\brief scale LONG for formatting
\param p position in syntax tree, should not be NULL
\param y
\param digits
\param before
\param after
\param q
**/

void long_standardise (NODE_T * p, MP_DIGIT_T * y, int digits, int before, int after, int *q)
{
  int j, pop_sp = stack_pointer;
  MP_DIGIT_T *f;
  MP_DIGIT_T *g;
  MP_DIGIT_T *h;
  MP_DIGIT_T *t;
  STACK_MP (f, p, digits);
  STACK_MP (g, p, digits);
  STACK_MP (h, p, digits);
  STACK_MP (t, p, digits);
  set_mp_short (g, (MP_DIGIT_T) 1, 0, digits);
  for (j = 0; j < before; j++) {
    mul_mp_digit (p, g, g, (MP_DIGIT_T) 10, digits);
  }
  div_mp_digit (p, h, g, (MP_DIGIT_T) 10, digits);
/* Speed huge exponents. */
  if ((MP_EXPONENT (y) - MP_EXPONENT (g)) > 1) {
    (*q) += LOG_MP_BASE * ((int) MP_EXPONENT (y) - (int) MP_EXPONENT (g) - 1);
    MP_EXPONENT (y) = MP_EXPONENT (g) + 1;
  }
  while ((sub_mp (p, t, y, g, digits), MP_DIGIT (t, 1) >= 0)) {
    div_mp_digit (p, y, y, (MP_DIGIT_T) 10, digits);
    (*q)++;
  }
  if (MP_DIGIT (y, 1) != 0) {
/* Speed huge exponents. */
    if ((MP_EXPONENT (y) - MP_EXPONENT (h)) < -1) {

      (*q) -= LOG_MP_BASE * ((int) MP_EXPONENT (h) - (int) MP_EXPONENT (y) - 1);
      MP_EXPONENT (y) = MP_EXPONENT (h) - 1;
    }
    while ((sub_mp (p, t, y, h, digits), MP_DIGIT (t, 1) < 0)) {
      mul_mp_digit (p, y, y, (MP_DIGIT_T) 10, digits);
      (*q)--;
    }
  }
  set_mp_short (f, (MP_DIGIT_T) 1, 0, digits);
  for (j = 0; j < after; j++) {
    div_mp_digit (p, f, f, (MP_DIGIT_T) 10, digits);
  }
  div_mp_digit (p, t, f, (MP_DIGIT_T) 2, digits);
  add_mp (p, t, y, t, digits);
  sub_mp (p, t, t, g, digits);
  if (MP_DIGIT (t, 1) >= 0) {
    MOVE_MP (y, h, digits);
    (*q)++;
  }
  stack_pointer = pop_sp;
}

/*!
\brief scale REAL for formatting
\param y
\param before
\param after
\param p position in syntax tree, should not be NULL
**/

void standardise (double *y, int before, int after, int *p)
{
  int j;
  double f, g = 1.0, h;
  for (j = 0; j < before; j++) {
    g *= 10.0;
  }
  h = g / 10.0;
  while (*y >= g) {
    *y *= 0.1;
    (*p)++;
  }
  if (*y != 0.0) {
    while (*y < h) {
      *y *= 10.0;
      (*p)--;
    }
  }
  f = 1.0;
  for (j = 0; j < after; j++) {
    f *= 0.1;
  }
  if (*y + 0.5 * f >= g) {
    *y = h;
    (*p)++;
  }
}

/*!
\brief formatted string for NUMBER
\param p position in syntax tree, should not be NULL
\return
**/

char *fleet (NODE_T * p)
{
  int pop_sp, arg_sp;
  A68_INT width, after, expo;
  MOID_T *mode;
/* POP arguments. */
  POP_INT (p, &expo);
  POP_INT (p, &after);
  POP_INT (p, &width);
  arg_sp = stack_pointer;
  DECREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
  mode = (MOID_T *) (((A68_POINTER *) STACK_TOP)->value);
  pop_sp = stack_pointer;
  if (mode == MODE (REAL)) {
    double x = ((A68_REAL *) (STACK_OFFSET (SIZE_OF (A68_POINTER))))->value;
    int before = ABS (width.value) - ABS (expo.value) - (after.value != 0 ? after.value + 1 : 0) - 2;
    stack_pointer = arg_sp;
#ifdef HAVE_IEEE_754
    if (isnan (x)) {
      int swid = 8 + ABS (width.value);
      char *s = stack_string (p, swid);
      if (width.value >= (int) strlen (NAN_STRING)) {
        FILL (s, BLANK_CHAR, width.value);
        bufcpy (s, NAN_STRING, swid);
        return (s);
      } else {
        return (error_chars (s, width.value));
      }
    } else if (isinf (x)) {
      int swid = 8 + ABS (width.value);
      char *s = stack_string (p, swid);
      if (width.value >= (int) strlen (INF_STRING)) {
        FILL (s, BLANK_CHAR, width.value);
        bufcpy (s, INF_STRING, swid);
        return (s);
      } else {
        return (error_chars (s, width.value));
      }
    }
#endif
    if (SIGN (before) + SIGN (after.value) > 0) {
      int strwid;
      char *s, *t1, *t2;
      double y = ABS (x);
      int q = 0;
      standardise (&y, before, after.value, &q);
      PUSH_POINTER (p, MODE (REAL));
      PUSH_REAL (p, SIGN (x) * y);
      INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + SIZE_OF (A68_REAL)));
      PUSH_INT (p, SIGN (width.value) * (ABS (width.value) - ABS (expo.value) - 1));
      PUSH_INT (p, after.value);
      t1 = fixed (p);
      PUSH_POINTER (p, MODE (INT));
      PUSH_INT (p, q);
      INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + SIZE_OF (A68_INT)));
      PUSH_INT (p, expo.value);
      t2 = whole (p);
      strwid = 8 + strlen (t1) + 1 + strlen (t2);
      s = stack_string (p, strwid);
      bufcpy (s, t1, BUFFER_SIZE);
      string_plusab_char (s, EXPONENT_CHAR, strwid);
      bufcat (s, t2, BUFFER_SIZE);
      if (expo.value == 0 || a68g_strchr (s, ERROR_CHAR) != NULL) {
        stack_pointer = arg_sp;
        PUSH_INT (p, width.value);
        PUSH_INT (p, after.value != 0 ? after.value - 1 : 0);
        PUSH_INT (p, expo.value > 0 ? expo.value + 1 : expo.value - 1);
        return (fleet (p));
      } else {
        return (s);
      }
    } else {
      char *s = stack_string (p, 8 + ABS (width.value));
      return (error_chars (s, width.value));
    }
  } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    int digits = get_mp_digits (mode);
    int before;
    BOOL_T ltz;
    MP_DIGIT_T *x = (MP_DIGIT_T *) (STACK_OFFSET (SIZE_OF (A68_POINTER)));
    stack_pointer = arg_sp;
    ltz = MP_DIGIT (x, 1) < 0;
    MP_DIGIT (x, 1) = ABS (MP_DIGIT (x, 1));
    before = ABS (width.value) - ABS (expo.value) - (after.value != 0 ? after.value + 1 : 0) - 2;
    if (SIGN (before) + SIGN (after.value) > 0) {
      int strwid;
      char *s, *t1, *t2;
      MP_DIGIT_T *z;
      int q = 0;
      STACK_MP (z, p, digits);
      MOVE_MP (z, x, digits);
      long_standardise (p, z, digits, before, after.value, &q);
      PUSH_POINTER (p, mode);
      MP_DIGIT (z, 1) = ltz ? -MP_DIGIT (z, 1) : MP_DIGIT (z, 1);
      PUSH (p, z, SIZE_MP (digits));
      INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + SIZE_MP (digits)));
      PUSH_INT (p, SIGN (width.value) * (ABS (width.value) - ABS (expo.value) - 1));
      PUSH_INT (p, after.value);
      t1 = fixed (p);
      PUSH_POINTER (p, MODE (INT));
      PUSH_INT (p, q);
      INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + SIZE_OF (A68_INT)));
      PUSH_INT (p, expo.value);
      t2 = whole (p);
      strwid = 8 + strlen (t1) + 1 + strlen (t2);
      s = stack_string (p, strwid);
      bufcpy (s, t1, BUFFER_SIZE);
      string_plusab_char (s, EXPONENT_CHAR, strwid);
      bufcat (s, t2, BUFFER_SIZE);
      if (expo.value == 0 || a68g_strchr (s, ERROR_CHAR) != NULL) {
        stack_pointer = arg_sp;
        PUSH_INT (p, width.value);
        PUSH_INT (p, after.value != 0 ? after.value - 1 : 0);
        PUSH_INT (p, expo.value > 0 ? expo.value + 1 : expo.value - 1);
        return (fleet (p));
      } else {
        return (s);
      }
    } else {
      char *s = stack_string (p, 8 + ABS (width.value));
      return (error_chars (s, width.value));
    }
  } else if (mode == MODE (INT)) {
    int x = ((A68_INT *) (STACK_OFFSET (SIZE_OF (A68_POINTER))))->value;
    PUSH_POINTER (p, MODE (REAL));
    PUSH_REAL (p, (double) x);
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)) - (SIZE_OF (A68_POINTER) + SIZE_OF (A68_REAL)));
    PUSH_INT (p, width.value);
    PUSH_INT (p, after.value);
    PUSH_INT (p, expo.value);
    return (fleet (p));
  } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    stack_pointer = pop_sp;
    if (mode == MODE (LONG_INT)) {
      ((A68_POINTER *) STACK_TOP)->value = (void *) MODE (LONG_REAL);
    } else {
      ((A68_POINTER *) STACK_TOP)->value = (void *) MODE (LONGLONG_REAL);
    }
    INCREMENT_STACK_POINTER (p, MOID_SIZE (MODE (NUMBER)));
    PUSH_INT (p, width.value);
    PUSH_INT (p, after.value);
    PUSH_INT (p, expo.value);
    return (fleet (p));
  }
  return (NULL);
}

/*!
\brief PROC (NUMBER, INT) STRING whole
\param p position in syntax tree, should not be NULL
**/

void genie_whole (NODE_T * p)
{
  int pop_sp = stack_pointer;
  A68_REF ref;
  char *str = whole (p);
  stack_pointer = pop_sp - SIZE_OF (A68_INT) - MOID_SIZE (MODE (NUMBER));
  ref = tmp_to_a68_string (p, str);
  PUSH (p, &ref, SIZE_OF (A68_REF));
}

/*!
\brief PROC (NUMBER, INT, INT) STRING fixed
\param p position in syntax tree, should not be NULL
\return
**/

void genie_fixed (NODE_T * p)
{
  int pop_sp = stack_pointer;
  A68_REF ref;
  char *str = fixed (p);
  stack_pointer = pop_sp - 2 * SIZE_OF (A68_INT) - MOID_SIZE (MODE (NUMBER));
  ref = tmp_to_a68_string (p, str);
  PUSH (p, &ref, SIZE_OF (A68_REF));
}

/*!
\brief PROC (NUMBER, INT, INT, INT) STRING float
\param p position in syntax tree, should not be NULL
\return
**/

void genie_float (NODE_T * p)
{
  int pop_sp = stack_pointer;
  A68_REF ref;
  char *str = fleet (p);
  stack_pointer = pop_sp - 3 * SIZE_OF (A68_INT) - MOID_SIZE (MODE (NUMBER));
  ref = tmp_to_a68_string (p, str);
  PUSH (p, &ref, SIZE_OF (A68_REF));
}

/* ALGOL68C routines. */

/*!
\brief PROC INT read int
\param p position in syntax tree, should not be NULL
**/

void genie_read_int (NODE_T * p)
{
  genie_read_standard (p, MODE (INT), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_INT));
}

/*!
\brief PROC LONG INT read long int
\param p position in syntax tree, should not be NULL
**/

void genie_read_long_int (NODE_T * p)
{
  genie_read_standard (p, MODE (LONG_INT), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, get_mp_size (MODE (LONG_INT)));
}

/*!
\brief PROC LONG LONG INT read long long int
\param p position in syntax tree, should not be NULL
**/

void genie_read_longlong_int (NODE_T * p)
{
  genie_read_standard (p, MODE (LONGLONG_INT), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, get_mp_size (MODE (LONGLONG_INT)));
}

/*!
\brief PROC REAL read real
\param p position in syntax tree, should not be NULL
**/

void genie_read_real (NODE_T * p)
{
  genie_read_standard (p, MODE (REAL), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_REAL));
}

/*!
\brief PROC LONG REAL read long real
\param p position in syntax tree, should not be NULL
**/

void genie_read_long_real (NODE_T * p)
{
  genie_read_standard (p, MODE (LONG_REAL), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, get_mp_size (MODE (LONG_REAL)));
}

/*!
\brief PROC LONG LONG REAL read long long real
\param p position in syntax tree, should not be NULL
**/

void genie_read_longlong_real (NODE_T * p)
{
  genie_read_standard (p, MODE (LONGLONG_REAL), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, get_mp_size (MODE (LONGLONG_REAL)));
}

/*!
\brief PROC COMPLEX read complex
\param p position in syntax tree, should not be NULL
**/

void genie_read_complex (NODE_T * p)
{
  genie_read_real (p);
  genie_read_real (p);
}

/*!
\brief PROC LONG COMPLEX read long complex
\param p position in syntax tree, should not be NULL
**/

void genie_read_long_complex (NODE_T * p)
{
  genie_read_long_real (p);
  genie_read_long_real (p);
}

/*!
\brief PROC LONG LONG COMPLEX read long long complex
\param p position in syntax tree, should not be NULL
**/

void genie_read_longlong_complex (NODE_T * p)
{
  genie_read_longlong_real (p);
  genie_read_longlong_real (p);
}

/*!
\brief PROC BOOL read bool
\param p position in syntax tree, should not be NULL
**/

void genie_read_bool (NODE_T * p)
{
  genie_read_standard (p, MODE (BOOL), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_BOOL));
}

/*!
\brief PROC BITS read bits
\param p position in syntax tree, should not be NULL
**/

void genie_read_bits (NODE_T * p)
{
  genie_read_standard (p, MODE (BITS), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_BITS));
}

/*!
\brief PROC LONG BITS read long bits
\param p position in syntax tree, should not be NULL
**/

void genie_read_long_bits (NODE_T * p)
{
  MP_DIGIT_T *z;
  STACK_MP (z, p, get_mp_digits (MODE (LONG_BITS)));
  genie_read_standard (p, MODE (LONG_BITS), (BYTE_T *) z, stand_in);
}

/*!
\brief PROC LONG LONG BITS read long long bits
\param p position in syntax tree, should not be NULL
**/

void genie_read_longlong_bits (NODE_T * p)
{
  MP_DIGIT_T *z;
  STACK_MP (z, p, get_mp_digits (MODE (LONGLONG_BITS)));
  genie_read_standard (p, MODE (LONGLONG_BITS), (BYTE_T *) z, stand_in);
}

/*!
\brief PROC CHAR read char
\param p position in syntax tree, should not be NULL
**/

void genie_read_char (NODE_T * p)
{
  genie_read_standard (p, MODE (CHAR), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_CHAR));
}

/*!
\brief PROC STRING read string
\param p position in syntax tree, should not be NULL
**/

void genie_read_string (NODE_T * p)
{
  genie_read_standard (p, MODE (STRING), STACK_TOP, stand_in);
  INCREMENT_STACK_POINTER (p, SIZE_OF (A68_REF));
}

/*!
\brief PROC (INT) VOID print int
\param p position in syntax tree, should not be NULL
**/

void genie_print_int (NODE_T * p)
{
  int size = MOID_SIZE (MODE (INT));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (INT), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG INT) VOID print long int
\param p position in syntax tree, should not be NULL
**/

void genie_print_long_int (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONG_INT));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (LONG_INT), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG LONG INT) VOID print long long int
\param p position in syntax tree, should not be NULL
**/

void genie_print_longlong_int (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONGLONG_INT));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (LONGLONG_INT), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (REAL) VOID print real
\param p position in syntax tree, should not be NULL
**/

void genie_print_real (NODE_T * p)
{
  int size = MOID_SIZE (MODE (REAL));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (REAL), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG REAL) VOID print long real
\param p position in syntax tree, should not be NULL
**/

void genie_print_long_real (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONG_REAL));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (LONG_REAL), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG LONG REAL) VOID print long long real
\param p position in syntax tree, should not be NULL
**/

void genie_print_longlong_real (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONGLONG_REAL));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (LONGLONG_REAL), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (COMPLEX) VOID print complex
\param p position in syntax tree, should not be NULL
**/

void genie_print_complex (NODE_T * p)
{
  int size = MOID_SIZE (MODE (COMPLEX));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (COMPLEX), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG COMPLEX) VOID print long complex
\param p position in syntax tree, should not be NULL
**/

void genie_print_long_complex (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONG_COMPLEX));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (LONG_COMPLEX), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG LONG COMPLEX) VOID print long long complex
\param p position in syntax tree, should not be NULL
**/

void genie_print_longlong_complex (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONGLONG_COMPLEX));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (LONGLONG_COMPLEX), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (CHAR) VOID print char
\param p position in syntax tree, should not be NULL
**/

void genie_print_char (NODE_T * p)
{
  int size = MOID_SIZE (MODE (CHAR));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (CHAR), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (BITS) VOID print bits
\param p position in syntax tree, should not be NULL
**/

void genie_print_bits (NODE_T * p)
{
  int size = MOID_SIZE (MODE (BITS));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (BITS), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG BITS) VOID print long bits
\param p position in syntax tree, should not be NULL
**/

void genie_print_long_bits (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONG_BITS));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (LONG_BITS), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (LONG LONG BITS) VOID print long long bits
\param p position in syntax tree, should not be NULL
**/

void genie_print_longlong_bits (NODE_T * p)
{
  int size = MOID_SIZE (MODE (LONGLONG_BITS));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (LONGLONG_BITS), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (BOOL) VOID print bool
\param p position in syntax tree, should not be NULL
**/

void genie_print_bool (NODE_T * p)
{
  int size = MOID_SIZE (MODE (BOOL));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, MODE (BOOL), STACK_OFFSET (-size), stand_out);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief PROC (STRING) VOID print string
\param p position in syntax tree, should not be NULL
**/

void genie_print_string (NODE_T * p)
{
  reset_transput_buffer (UNFORMATTED_BUFFER);
  add_string_from_stack_transput_buffer (p, UNFORMATTED_BUFFER);
  write_purge_buffer (p, stand_out, UNFORMATTED_BUFFER);

}

/*
Transput library - Formatted transput

In Algol68G, a value of mode FORMAT looks like a routine text. The value
comprises a pointer to its environment in the stack, and a pointer where the
format text is at in the syntax tree.
*/

#define INT_DIGITS "0123456789"
#define BITS_DIGITS "0123456789abcdefABCDEF"
#define INT_DIGITS_BLANK " 0123456789"
#define BITS_DIGITS_BLANK " 0123456789abcdefABCDEF"
#define SIGN_DIGITS " +-"

/*!
\brief handle format error event
\param p position in syntax tree, should not be NULL
\param ref_file
**/

void format_error (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  A68_BOOL z;
  on_event_handler (p, f->format_error_mended, ref_file);
  POP_BOOL (p, &z);
  if (z.value == A_FALSE) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FORMAT_PICTURES);
    exit_genie (p, A_RUNTIME_ERROR);
  }
}


/*!
\brief initialise processing of pictures
\param p position in syntax tree, should not be NULL
**/

static void initialise_collitems (NODE_T * p)
{
/*
Every picture has a counter that says whether it has not been used OR the number
of times it can still be used.
*/
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, PICTURE)) {
      A68_COLLITEM *z = (A68_COLLITEM *) FRAME_LOCAL (frame_pointer, TAX (p)->offset);
      z->status = INITIALISED_MASK;
      z->count = ITEM_NOT_USED;
    }
/* Don't dive into f, g, n frames and collections. */
    if (!(WHETHER (p, ENCLOSED_CLAUSE) || WHETHER (p, COLLECTION))) {
      initialise_collitems (SUB (p));
    }
  }
}

/*!
\brief initialise processing of format text
\param file
\param fmt
\param embedded
\param init
**/

static void open_format_frame (A68_FILE * file, A68_FORMAT * fmt, BOOL_T embedded, BOOL_T init)
{
/* Open a new frame for the format text and save for return to embedding one. */
  NODE_T *dollar = SUB (fmt->body);
  A68_FORMAT *save;
  OPEN_PROC_FRAME (dollar, fmt->environ);
/* Save old format. */
  save = (A68_FORMAT *) FRAME_LOCAL (frame_pointer, TAX (dollar)->offset);
  *save = (embedded == EMBEDDED_FORMAT ? file->format : nil_format);
  file->format = *fmt;
/* Reset all collitems. */
  if (init) {
    initialise_collitems (dollar);
  }
}

/*!
\brief handle end-of-format event
\param p position in syntax tree, should not be NULL
\param ref_file
\return
**/

int end_of_format (NODE_T * p, A68_REF ref_file)
{
/*
Format-items return immediately to the embedding format text. The outermost
format text calls "on format end".
*/
  A68_FILE *file = FILE_DEREF (&ref_file);
  NODE_T *dollar = SUB (file->format.body);
  A68_FORMAT *save = (A68_FORMAT *) FRAME_LOCAL (frame_pointer, TAX (dollar)->offset);
  if (IS_NIL_FORMAT (save)) {
/* Not embedded, outermost format: execute event routine. */
    A68_BOOL z;
    on_event_handler (p, (FILE_DEREF (&ref_file))->format_end_mended, ref_file);
    POP_BOOL (p, &z);
    if (z.value == A_FALSE) {
/* Restart format. */
      frame_pointer = file->frame_pointer;
      stack_pointer = file->stack_pointer;
      open_format_frame (file, &(file->format), NOT_EMBEDDED_FORMAT, A_TRUE);
    }
    return (NOT_EMBEDDED_FORMAT);
  } else {
/* Embedded format, return to embedding format, cf. RR. */
    CLOSE_FRAME;
    file->format = *save;
    return (EMBEDDED_FORMAT);
  }
}

/*!
\brief return integral value of replicator
\param p position in syntax tree, should not be NULL
\return
**/

int get_replicator_value (NODE_T * p)
{
  int z = 0;
  if (WHETHER (p, STATIC_REPLICATOR)) {
    A68_INT u;
    if (genie_string_to_value_internal (p, MODE (INT), SYMBOL (p), (BYTE_T *) & u) == A_FALSE) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_IN_DENOTER, MODE (INT));
      exit_genie (p, A_RUNTIME_ERROR);
    }
    z = u.value;
  } else if (WHETHER (p, DYNAMIC_REPLICATOR)) {
    A68_INT u;
    EXECUTE_UNIT (NEXT_SUB (p));
    POP_INT (p, &u);
    z = u.value;
  } else if (WHETHER (p, REPLICATOR)) {
    z = get_replicator_value (SUB (p));
  }
  return (z >= 0 ? z : 0);
}

/*!
\brief return first available pattern
\param p position in syntax tree, should not be NULL
\param ref_file
\return
**/

static NODE_T *scan_format_pattern (NODE_T * p, A68_REF ref_file)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, PICTURE_LIST)) {
      NODE_T *prio = scan_format_pattern (SUB (p), ref_file);
      if (prio != NULL) {
        return (prio);
      }
    }
    if (WHETHER (p, PICTURE)) {
      NODE_T *picture = SUB (p);
      A68_COLLITEM *collitem = (A68_COLLITEM *) FRAME_LOCAL (frame_pointer, TAX (p)->offset);
      if (collitem->count != 0) {
        if (WHETHER (picture, PATTERN)) {
          collitem->count = 0;  /* This pattern is now done. */
          picture = SUB (picture);
          if (ATTRIBUTE (picture) != FORMAT_PATTERN) {
            return (picture);
          } else {
            NODE_T *pat;
            A68_FORMAT z;
            A68_FILE *file = FILE_DEREF (&ref_file);
            EXECUTE_UNIT (NEXT_SUB (picture));
            POP (p, &z, SIZE_OF (A68_FORMAT));
            open_format_frame (file, &z, EMBEDDED_FORMAT, A_TRUE);
            pat = scan_format_pattern (SUB (file->format.body), ref_file);
            if (pat != NULL) {
              return (pat);
            } else {
              (void) end_of_format (p, ref_file);
            }
          }
        } else if (WHETHER (picture, INSERTION)) {
          A68_FILE *file = FILE_DEREF (&ref_file);
          if (file->read_mood) {
            read_insertion (picture, ref_file);
          } else if (file->write_mood) {
            write_insertion (picture, ref_file, INSERTION_NORMAL);
          } else {
            ABNORMAL_END (A_TRUE, "undetermined mood for insertion", NULL);
          }
          collitem->count = 0;  /* This insertion is now done. */
        } else if (WHETHER (picture, REPLICATOR) || WHETHER (picture, COLLECTION)) {
          BOOL_T go_on = A_TRUE;
          NODE_T *select = NULL;
          if (collitem->count == ITEM_NOT_USED) {
            if (WHETHER (picture, REPLICATOR)) {
              collitem->count = get_replicator_value (SUB (p));
              picture = NEXT (picture);
            } else {
              collitem->count = 1;
            }
            initialise_collitems (NEXT_SUB (picture));
          } else if (WHETHER (picture, REPLICATOR)) {
            picture = NEXT (picture);
          }
          while (go_on) {
/* Get format item from collection. If collection is done, but repitition is not,
   then re-initialise the collection and repeat. */
            select = scan_format_pattern (NEXT_SUB (picture), ref_file);
            if (select != NULL) {
              return (select);
            } else {
              collitem->count--;
              go_on = collitem->count > 0;
              if (go_on) {
                initialise_collitems (NEXT_SUB (picture));
              }
            }
          }
        }
      }
    }
  }
  return (NULL);
}

/*!
\brief return first available pattern
\param p position in syntax tree, should not be NULL
\param ref_file
\param mood
\return
**/

NODE_T *get_next_format_pattern (NODE_T * p, A68_REF ref_file, BOOL_T mood)
{
/*
"mood" can be WANT_PATTERN: pattern needed by caller, so perform end-of-format
if needed or SKIP_PATTERN: just emptying current pattern/collection/format.
*/
  A68_FILE *file = FILE_DEREF (&ref_file);
  if (file->format.body == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FORMAT_EXHAUSTED);
    exit_genie (p, A_RUNTIME_ERROR);
    return (NULL);
  } else {
    NODE_T *pat = scan_format_pattern (SUB (file->format.body), ref_file);
    if (pat == NULL) {
      if (mood == WANT_PATTERN) {
        int z;
        do {
          z = end_of_format (p, ref_file);
          pat = scan_format_pattern (SUB (file->format.body), ref_file);
        }
        while (z == EMBEDDED_FORMAT && pat == NULL);
        if (pat == NULL) {
          diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FORMAT_EXHAUSTED);
          exit_genie (p, A_RUNTIME_ERROR);
        }
      }
    }
    return (pat);
  }
}

/*!
\brief diagnostic_node in case mode does not match picture
\param p position in syntax tree, should not be NULL
\param mode
\param att
**/

void pattern_error (NODE_T * p, MOID_T * mode, int att)
{
  diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FORMAT_CANNOT_TRANSPUT, mode, att);
  exit_genie (p, A_RUNTIME_ERROR);
}

/*!
\brief unite value at top of stack to NUMBER
\param p position in syntax tree, should not be NULL
\param mode
\param item
\return
**/

static void unite_to_number (NODE_T * p, MOID_T * mode, BYTE_T * item)
{
  ADDR_T sp = stack_pointer;
  PUSH_POINTER (p, mode);
  PUSH (p, item, MOID_SIZE (mode));
  stack_pointer = sp + MOID_SIZE (MODE (NUMBER));
}

/*!
\brief write a group of insertions
\param p position in syntax tree, should not be NULL
\param ref_file
\param mood
\return
**/

void write_insertion (NODE_T * p, A68_REF ref_file, unsigned mood)
{
  for (; p != NULL; FORWARD (p)) {
    write_insertion (SUB (p), ref_file, mood);
    if (WHETHER (p, FORMAT_ITEM_L)) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, NEWLINE_CHAR);
      write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
    } else if (WHETHER (p, FORMAT_ITEM_P)) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, FORMFEED_CHAR);
      write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
    } else if (WHETHER (p, FORMAT_ITEM_X) || WHETHER (p, FORMAT_ITEM_Q)) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
    } else if (WHETHER (p, FORMAT_ITEM_Y)) {
      /*
       * Not supported, parser has warned you. 
       */ ;
    } else if (WHETHER (p, LITERAL)) {
      if (mood & INSERTION_NORMAL) {
        add_string_transput_buffer (p, FORMATTED_BUFFER, SYMBOL (p));
      } else if (mood & INSERTION_BLANK) {
        int j, k = strlen (SYMBOL (p));
        for (j = 1; j <= k; j++) {
          add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
        }
      }
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      if (ATTRIBUTE (SUB (NEXT (p))) != FORMAT_ITEM_K) {
        for (j = 1; j <= k; j++) {
          write_insertion (NEXT (p), ref_file, mood);
        }
      } else {
        int pos = get_transput_buffer_index (FORMATTED_BUFFER);
        for (j = 1; j < (k - pos); j++) {
          add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
        }
      }
      return;
    }
  }
}

/*!
\brief write string to file following current format
\param p position in syntax tree, should not be NULL
\param mode
\param ref_file
\param str
**/

static void write_string_pattern (NODE_T * p, MOID_T * mode, A68_REF ref_file, char **str)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, INSERTION)) {
      write_insertion (SUB (p), ref_file, INSERTION_NORMAL);
    } else if (WHETHER (p, FORMAT_ITEM_A)) {
      if ((*str)[0] != NULL_CHAR) {
        add_char_transput_buffer (p, FORMATTED_BUFFER, (*str)[0]);
        (*str)++;
      } else {
        value_error (p, mode, ref_file);
      }
    } else if (WHETHER (p, FORMAT_ITEM_S)) {
      if ((*str)[0] != NULL_CHAR) {
        (*str)++;
      } else {
        value_error (p, mode, ref_file);
      }
      return;
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      for (j = 1; j <= k; j++) {
        write_string_pattern (NEXT (p), mode, ref_file, str);
      }
      return;
    } else {
      write_string_pattern (SUB (p), mode, ref_file, str);
    }
  }
}

/*!
\brief write string to file using C style format %[-][w]s
\param p position in syntax tree, should not be NULL
\param str
**/

static void write_string_c_style (NODE_T * p, char *str)
{
  int k, sign, width;
  if (WHETHER (p, STRING_C_PATTERN)) {
    NODE_T *q = NEXT_SUB (p);
/* Get sign. */
    if (WHETHER (q, FORMAT_ITEM_PLUS) || WHETHER (q, FORMAT_ITEM_MINUS)) {
      sign = ATTRIBUTE (q);
      FORWARD (q);
    } else {
      sign = 0;
    }
/* Get width. */
    if (WHETHER (q, REPLICATOR)) {
      width = get_replicator_value (SUB (q));
    } else {
      width = strlen (str);
    }
/* Output string. */
    k = width - strlen (str);
    if (k >= 0) {
      if (sign == FORMAT_ITEM_PLUS || sign == 0) {
        add_string_transput_buffer (p, FORMATTED_BUFFER, str);
      }
      while (k--) {
        add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
      }
      if (sign == FORMAT_ITEM_MINUS) {
        add_string_transput_buffer (p, FORMATTED_BUFFER, str);
      }
    } else {
      error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
    }
  }
}

/*!
\brief write appropriate insertion from a choice pattern
\param p position in syntax tree, should not be NULL
\param ref_file
\param count
\return
**/

static void write_choice_pattern (NODE_T * p, A68_REF ref_file, int *count)
{
  for (; p != NULL; FORWARD (p)) {
    write_choice_pattern (SUB (p), ref_file, count);
    if (WHETHER (p, PICTURE)) {
      (*count)--;
      if (*count == 0) {
        write_insertion (SUB (p), ref_file, INSERTION_NORMAL);
      }
    }
  }
}

/*!
\brief write appropriate insertion from a boolean pattern
\param p position in syntax tree, should not be NULL
\param ref_file
\param z
**/

static void write_boolean_pattern (NODE_T * p, A68_REF ref_file, BOOL_T z)
{
  int k = (z ? 1 : 2);
  write_choice_pattern (p, ref_file, &k);
}

/*!
\brief write value according to a general pattern
\param p position in syntax tree, should not be NULL
\param mode
\param item
**/

static void write_number_generic (NODE_T * p, MOID_T * mode, BYTE_T * item)
{
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int size;
/* Push arguments. */
  unite_to_number (p, mode, item);
  EXECUTE_UNIT (NEXT_SUB (p));
  POP_REF (p, &row);
  GET_DESCRIPTOR (arr, tup, &row);
  size = ROW_SIZE (tup);
  if (size > 0) {
    int i;
    BYTE_T *base_address = ADDRESS (&arr->array);
    for (i = tup->lower_bound; i <= tup->upper_bound; i++) {
      int addr = INDEX_1_DIM (arr, tup, i);
      int arg = ((A68_INT *) & (base_address[addr]))->value;
      PUSH_INT (p, arg);
    }
  }
/* Make a string. */
  switch (size) {
  case 1:
    {
      genie_whole (p);
      break;
    }
  case 2:
    {
      genie_fixed (p);
      break;
    }
  case 3:
    {
      genie_float (p);
      break;
    }
  default:
    {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FORMAT_INTS_REQUIRED, MODE (INT));
      exit_genie (p, A_RUNTIME_ERROR);
      break;
    }
  }
  add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
}

/*!
\brief handle %[+][-][w]d, %[+][-][w][.][d]f/e formats
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void write_number_c_style (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  BOOL_T sign = 0;
  int width = 0, after = 0;
  char *str = NULL;
  unite_to_number (p, mode, item);
/* Integral C pattern. */
  if (WHETHER (p, INTEGRAL_C_PATTERN)) {
    NODE_T *q = NEXT_SUB (p);
/* Get sign. */
    if (WHETHER (q, FORMAT_ITEM_PLUS) || WHETHER (q, FORMAT_ITEM_MINUS)) {
      sign = ATTRIBUTE (q);
      FORWARD (q);
    } else {
      sign = 0;
    }
/* Get width. */
    if (WHETHER (q, REPLICATOR)) {
      width = get_replicator_value (SUB (q));
    } else {
      width = 0;
    }
/* Make string. */
    PUSH_INT (p, sign != 0 ? width : -width);
    str = whole (p);
  }
/* Real C patterns. */
  else if (WHETHER (p, FIXED_C_PATTERN) || WHETHER (p, FLOAT_C_PATTERN)) {
    NODE_T *q = NEXT_SUB (p);
/* Get sign. */
    if (WHETHER (q, FORMAT_ITEM_PLUS) || WHETHER (q, FORMAT_ITEM_MINUS)) {
      sign = ATTRIBUTE (q);
      FORWARD (q);
    } else {
      sign = 0;
    }
/* Get width. */
    if (WHETHER (q, REPLICATOR)) {
      width = get_replicator_value (SUB (q));
      FORWARD (q);
    } else {
      width = 0;
    }
/* Skip point. */
    if (WHETHER (q, FORMAT_ITEM_POINT)) {
      FORWARD (q);
    }
/* Get decimals. */
    if (WHETHER (q, REPLICATOR)) {
      after = get_replicator_value (SUB (q));
      FORWARD (q);
    } else {
      after = 0;
    }
/* Make string. */
    if (WHETHER (p, FIXED_C_PATTERN)) {
      int num_width = 0, max = 0;
      if (mode == MODE (REAL) || mode == MODE (INT)) {
        max = REAL_WIDTH - 1;
      } else if (mode == MODE (LONG_REAL) || mode == MODE (LONG_INT)) {
        max = LONG_REAL_WIDTH - 1;
      } else if (mode == MODE (LONGLONG_REAL) || mode == MODE (LONGLONG_INT)) {
        max = LONGLONG_REAL_WIDTH - 1;
      }
      if (after < 0 || after > max) {
        after = max;
      }
      num_width = width;
      PUSH_INT (p, sign != 0 ? num_width : -num_width);
      PUSH_INT (p, after);
      str = fixed (p);
    } else if (WHETHER (p, FLOAT_C_PATTERN)) {
      int num_width = 0, max = 0, mex = 0, expo = 0;
      if (mode == MODE (REAL) || mode == MODE (INT)) {
        max = REAL_WIDTH - 1;
        mex = EXP_WIDTH + 1;
      } else if (mode == MODE (LONG_REAL) || mode == MODE (LONG_INT)) {
        max = LONG_REAL_WIDTH - 1;
        mex = LONG_EXP_WIDTH + 1;
      } else if (mode == MODE (LONGLONG_REAL) || mode == MODE (LONGLONG_INT)) {
        max = LONGLONG_REAL_WIDTH - 1;
        mex = LONGLONG_EXP_WIDTH + 1;
      }
      expo = mex + 1;
      if (after <= 0 && width > 0) {
        after = width - (expo + 4);
      }
      if (after <= 0 || after > max) {
        after = max;
      }
      num_width = after + expo + 4;
      PUSH_INT (p, sign != 0 ? num_width : -num_width);
      PUSH_INT (p, after);
      PUSH_INT (p, expo);
      str = fleet (p);
    }
  }
/* Did the conversion succeed? */
  if (a68g_strchr (str, ERROR_CHAR) != NULL) {
    value_error (p, mode, ref_file);
    error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
  } else {
/* Edit and output. */
    if (sign == FORMAT_ITEM_MINUS) {
      char *ch = str;
      while (ch[0] != NULL_CHAR && ch[0] == BLANK_CHAR) {
        ch++;
      }
      if (ch[0] != NULL_CHAR && ch[0] == '+') {
        ch[0] = BLANK_CHAR;
      }
    }
    if (width == 0) {
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else {
      int blanks = width - strlen (str);
      if (blanks >= 0) {
        while (blanks--) {
          add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
        }
        add_string_transput_buffer (p, FORMATTED_BUFFER, str);
      } else {
        value_error (p, mode, ref_file);
        error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
      }
    }
  }
}

/* INTEGRAL, REAL, COMPLEX and BITS patterns. */

/*!
\brief count Z and D frames in a mould
\param p position in syntax tree, should not be NULL
\param z
\return
**/

static void count_zd_frames (NODE_T * p, int *z)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, FORMAT_ITEM_D) || WHETHER (p, FORMAT_ITEM_Z)) {

      (*z)++;
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      for (j = 1; j <= k; j++) {
        count_zd_frames (NEXT (p), z);
      }
      return;
    } else {
      count_zd_frames (SUB (p), z);
    }
  }
}

/*!
\brief count D frames in a mould
\param p position in syntax tree, should not be NULL
\param z
\return
**/

static void count_d_frames (NODE_T * p, int *z)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, FORMAT_ITEM_D)) {

      (*z)++;
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      for (j = 1; j <= k; j++) {
        count_d_frames (NEXT (p), z);
      }
      return;
    } else {
      count_d_frames (SUB (p), z);
    }
  }
}

/*!
\brief get sign from sign mould
\param p position in syntax tree, should not be NULL
\return
**/

static NODE_T *get_sign (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    NODE_T *q = get_sign (SUB (p));
    if (q != NULL) {
      return (q);
    } else if (WHETHER (p, FORMAT_ITEM_PLUS) || WHETHER (p, FORMAT_ITEM_MINUS)) {
      return (p);
    }
  }
  return (NULL);
}

/*!
\brief shift sign through Z frames until non-zero digit or D frame
\param p position in syntax tree, should not be NULL
\param q
**/

static void shift_sign (NODE_T * p, char **q)
{
  for (; p != NULL && (*q) != NULL; FORWARD (p)) {
    shift_sign (SUB (p), q);
    if (WHETHER (p, FORMAT_ITEM_Z)) {
      if (((*q)[0] == '+' || (*q)[0] == '-') && (*q)[1] == '0') {
        char ch = (*q)[0];
        (*q)[0] = (*q)[1];
        (*q)[1] = ch;
        (*q)++;
      }
    } else if (WHETHER (p, FORMAT_ITEM_D)) {
      (*q) = NULL;
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      for (j = 1; j <= k; j++) {
        shift_sign (NEXT (p), q);
      }
      return;
    }
  }
}

/*!
\brief pad trailing blanks to integral until desired width
\param p position in syntax tree, should not be NULL
\param n
**/

static void put_zeroes_to_integral (NODE_T * p, int n)
{
  while (n--) {
    add_char_transput_buffer (p, EDIT_BUFFER, '0');
  }
}

/*!
\brief pad a sign to integral representation
\param p position in syntax tree, should not be NULL
\param sign
**/

static void put_sign_to_integral (NODE_T * p, int sign)
{
  NODE_T *sign_node = get_sign (SUB (p));
  if (WHETHER (sign_node, FORMAT_ITEM_PLUS)) {
    add_char_transput_buffer (p, EDIT_BUFFER, (sign >= 0 ? '+' : '-'));
  } else {
    add_char_transput_buffer (p, EDIT_BUFFER, (sign >= 0 ? BLANK_CHAR : '-'));
  }
}

/*!
\brief convert to other radix, binary up to hexadecimal
\param p position in syntax tree, should not be NULL
\param z
\param radix
\param width
\return
**/

static BOOL_T convert_radix (NODE_T * p, unsigned z, int radix, int width)
{
  static char *images = "0123456789abcdef";
  if (width > 0 && (radix >= 2 && radix <= 16)) {
    int digit = z % radix;
    BOOL_T success = convert_radix (p, z / radix, radix, width - 1);
    add_char_transput_buffer (p, EDIT_BUFFER, images[digit]);
    return (success);
  } else {
    return (z == 0);
  }
}

/*!
\brief convert to other radix, binary up to hexadecimal
\param p position in syntax tree, should not be NULL
\param u
\param radix
\param width
\param m
\param v
\param w
\return
**/

static BOOL_T convert_radix_mp (NODE_T * p, MP_DIGIT_T * u, int radix, int width, MOID_T * m, MP_DIGIT_T * v, MP_DIGIT_T * w)
{
  static char *images = "0123456789abcdef";
  if (width > 0 && (radix >= 2 && radix <= 16)) {
    int digit, digits = get_mp_digits (m);
    BOOL_T success;
    MOVE_MP (w, u, digits);
    over_mp_digit (p, u, u, (MP_DIGIT_T) radix, digits);
    mul_mp_digit (p, v, u, (MP_DIGIT_T) radix, digits);
    sub_mp (p, v, w, v, digits);
    digit = (int) MP_DIGIT (v, 1);
    success = convert_radix_mp (p, u, radix, width - 1, m, v, w);
    add_char_transput_buffer (p, EDIT_BUFFER, images[digit]);
    return (success);
  } else {
    return (MP_DIGIT (u, 1) == 0);
  }
}

/*!
\brief write point, exponent or plus-i-times symbol
\param p position in syntax tree, should not be NULL
\param ref_file
\param att
\param sym
**/

static void write_pie_frame (NODE_T * p, A68_REF ref_file, int att, int sym)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, INSERTION)) {
      write_insertion (p, ref_file, INSERTION_NORMAL);
    } else if (WHETHER (p, att)) {
      write_pie_frame (SUB (p), ref_file, att, sym);
      return;
    } else if (WHETHER (p, sym)) {
      add_string_transput_buffer (p, FORMATTED_BUFFER, SYMBOL (p));
    } else if (WHETHER (p, FORMAT_ITEM_S)) {
      return;
    }
  }
}

/*!
\brief write sign when appropriate
\param p position in syntax tree, should not be NULL
\param q
**/

static void write_mould_put_sign (NODE_T * p, char **q)
{
  if ((*q)[0] == '+' || (*q)[0] == '-' || (*q)[0] == BLANK_CHAR) {
    add_char_transput_buffer (p, FORMATTED_BUFFER, (*q)[0]);
    (*q)++;
  }
}

/*!
\brief write string according to a mould
\param p position in syntax tree, should not be NULL
\param ref_file
\param type
\param q
\param mood
**/

static void write_mould (NODE_T * p, A68_REF ref_file, int type, char **q, unsigned *mood)
{
  for (; p != NULL; FORWARD (p)) {
/* Insertions are inserted straight away. Note that we can suppress them using
   "mood", which is not standard A68. */
    if (WHETHER (p, INSERTION)) {
      write_insertion (SUB (p), ref_file, *mood);
    } else {
      write_mould (SUB (p), ref_file, type, q, mood);
/* Z frames print blanks until first non-zero digits comes. */
      if (WHETHER (p, FORMAT_ITEM_Z)) {
        write_mould_put_sign (p, q);
        if ((*q)[0] == '0') {
          if (*mood & DIGIT_BLANK) {
            add_char_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
            (*q)++;
            *mood = (*mood & ~INSERTION_NORMAL) | INSERTION_BLANK;
          } else if (*mood & DIGIT_NORMAL) {
            add_char_transput_buffer (p, FORMATTED_BUFFER, '0');
            (*q)++;
            *mood = DIGIT_NORMAL | INSERTION_NORMAL;
          }
        } else {
          add_char_transput_buffer (p, FORMATTED_BUFFER, (*q)[0]);
          (*q)++;
          *mood = DIGIT_NORMAL | INSERTION_NORMAL;
        }
      }
/* D frames print a digit. */
      else if (WHETHER (p, FORMAT_ITEM_D)) {
        write_mould_put_sign (p, q);
        add_char_transput_buffer (p, FORMATTED_BUFFER, (*q)[0]);
        (*q)++;
        *mood = DIGIT_NORMAL | INSERTION_NORMAL;
      }
/* Suppressible frames. */
      else if (WHETHER (p, FORMAT_ITEM_S)) {
/* Suppressible frames are ignored in a sign-mould. */
        if (type == SIGN_MOULD) {
          write_mould (NEXT (p), ref_file, type, q, mood);
        } else if (type == INTEGRAL_MOULD) {
          (*q)++;
        }
        return;
      }
/* Replicator. */
      else if (WHETHER (p, REPLICATOR)) {
        int j, k = get_replicator_value (SUB (p));
        for (j = 1; j <= k; j++) {
          write_mould (NEXT (p), ref_file, type, q, mood);
        }
        return;
      }
    }
  }
}

/*!
\brief write INT value using int pattern
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void write_integral_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  if (!(mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT))) {
    pattern_error (p, mode, ATTRIBUTE (p));
  } else {
    ADDR_T old_stack_pointer = stack_pointer;
    char *str;
    int width = 0, sign = 0;
    unsigned mood;
/* Dive into the pattern if needed. */
    if (WHETHER (p, INTEGRAL_PATTERN)) {
      p = SUB (p);
    }
/* Find width. */
    count_zd_frames (p, &width);
/* Make string. */
    reset_transput_buffer (EDIT_BUFFER);
    if (mode == MODE (INT)) {
      A68_INT *z = (A68_INT *) item;
      sign = SIGN (z->value);
      str = sub_whole (p, ABS (z->value), width);
    } else if (mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
      MP_DIGIT_T *z = (MP_DIGIT_T *) item;
      sign = SIGN (z[2]);
      z[2] = ABS (z[2]);
      str = long_sub_whole (p, z, get_mp_digits (mode), width);
    }
/* Edit string and output. */
    if (a68g_strchr (str, ERROR_CHAR) != NULL) {
      value_error (p, mode, ref_file);
    }
    if (WHETHER (p, SIGN_MOULD)) {
      put_sign_to_integral (p, sign);
    } else if (sign < 0) {
      value_error (p, mode, ref_file);
    }
    put_zeroes_to_integral (p, width - strlen (str));
    add_string_transput_buffer (p, EDIT_BUFFER, str);
    str = get_transput_buffer (EDIT_BUFFER);
    if (WHETHER (p, SIGN_MOULD)) {
      if (str[0] == '+' || str[0] == '-') {
        shift_sign (SUB (p), &str);
      }
      str = get_transput_buffer (EDIT_BUFFER);
      mood = DIGIT_BLANK | INSERTION_NORMAL;
      write_mould (SUB (p), ref_file, SIGN_MOULD, &str, &mood);
      FORWARD (p);
    }
    if (WHETHER (p, INTEGRAL_MOULD)) {  /* This *should* be the case. */
      mood = DIGIT_NORMAL | INSERTION_NORMAL;
      write_mould (SUB (p), ref_file, INTEGRAL_MOULD, &str, &mood);
    }
    stack_pointer = old_stack_pointer;
  }
}

/*!
\brief write REAL value using real pattern
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void write_real_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  if (!(mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL) || mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT))) {
    pattern_error (p, mode, ATTRIBUTE (p));
  } else {
    ADDR_T old_stack_pointer = stack_pointer;
    int stag_digits = 0, frac_digits = 0, expo_digits = 0;
    int stag_width = 0, frac_width = 0, expo_width = 0;
    int length, sign = 0, d_exp;
    NODE_T *q, *sign_mould = NULL, *stag_mould = NULL, *point_frame = NULL, *frac_mould = NULL, *e_frame = NULL, *expo_mould = NULL;
    char *str, *stag_str, *frac_str;
/* Dive into pattern. */
    q = (WHETHER (p, REAL_PATTERN)) ? SUB (p) : p;
/* Dissect pattern and establish widths. */
    if (q != NULL && WHETHER (q, SIGN_MOULD)) {
      sign_mould = q;
      count_zd_frames (SUB (sign_mould), &stag_width);
      count_d_frames (SUB (sign_mould), &stag_digits);
      FORWARD (q);
    }
    if (q != NULL && WHETHER (q, INTEGRAL_MOULD)) {
      stag_mould = q;
      count_zd_frames (SUB (stag_mould), &stag_width);
      count_zd_frames (SUB (stag_mould), &stag_digits);
      FORWARD (q);
    }
    if (q != NULL && WHETHER (q, FORMAT_POINT_FRAME)) {
      point_frame = q;
      FORWARD (q);
    }
    if (q != NULL && WHETHER (q, INTEGRAL_MOULD)) {
      frac_mould = q;
      count_zd_frames (SUB (frac_mould), &frac_width);
      count_zd_frames (SUB (frac_mould), &frac_digits);
      FORWARD (q);
    }
    if (q != NULL && WHETHER (q, EXPONENT_FRAME)) {
      e_frame = SUB (q);
      expo_mould = NEXT_SUB (q);
      q = expo_mould;
      if (WHETHER (q, SIGN_MOULD)) {
        count_zd_frames (SUB (q), &expo_width);
        count_d_frames (SUB (q), &expo_digits);
        FORWARD (q);
      }
      if (WHETHER (q, INTEGRAL_MOULD)) {
        count_zd_frames (SUB (q), &expo_width);
        count_d_frames (SUB (q), &expo_digits);
      }
    }
/* Make string representation. */
    reset_transput_buffer (EDIT_BUFFER);
    length = 1 + stag_width + frac_width;
    if (mode == MODE (REAL) || mode == MODE (INT)) {
      double x;
      if (mode == MODE (REAL)) {
        x = ((A68_REAL *) item)->value;
      } else {
        x = (double) ((A68_INT *) item)->value;
      }
#ifdef HAVE_IEEE_754
      if (isnan (x)) {
        int swid = 1 + length;
        char *s = stack_string (p, swid);
        if (length >= (int) strlen (NAN_STRING)) {
          FILL (s, BLANK_CHAR, length);
          bufcpy (s, NAN_STRING, swid);
        } else {
          error_chars (s, length);
        }
        add_string_transput_buffer (p, FORMATTED_BUFFER, s);
        stack_pointer = old_stack_pointer;
        return;
      } else if (isinf (x)) {
        int swid = 1 + length;
        char *s = stack_string (p, swid);
        if (length >= (int) strlen (INF_STRING)) {
          FILL (s, BLANK_CHAR, length);
          bufcpy (s, INF_STRING, swid);
        } else {
          error_chars (s, length);
        }
        add_string_transput_buffer (p, FORMATTED_BUFFER, s);
        stack_pointer = old_stack_pointer;
        return;
      }
#endif
      d_exp = 0;
      sign = SIGN (x);
      if (sign_mould != NULL) {
        put_sign_to_integral (sign_mould, sign);
      }
      x = ABS (x);
      if (expo_mould != NULL) {
        standardise (&x, stag_digits, frac_digits, &d_exp);
      }
      str = sub_fixed (p, x, length, frac_digits);
    } else if (mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
      ADDR_T old_stack_pointer = stack_pointer;
      int digits = get_mp_digits (mode);
      MP_DIGIT_T *x;
      STACK_MP (x, p, digits);
      MOVE_MP (x, (MP_DIGIT_T *) item, digits);
      d_exp = 0;
      sign = SIGN (x[2]);
      if (sign_mould != NULL) {
        put_sign_to_integral (sign_mould, sign);
      }
      x[2] = ABS (x[2]);
      if (expo_mould != NULL) {
        long_standardise (p, x, get_mp_digits (mode), stag_digits, frac_digits, &d_exp);
      }
      str = long_sub_fixed (p, x, get_mp_digits (mode), length, frac_digits);
      stack_pointer = old_stack_pointer;
    }
/* Edit and output the string. */
    if (a68g_strchr (str, ERROR_CHAR) != NULL) {
      value_error (p, mode, ref_file);
    }
    put_zeroes_to_integral (p, length - strlen (str));
    add_string_transput_buffer (p, EDIT_BUFFER, str);
    stag_str = get_transput_buffer (EDIT_BUFFER);
    if (a68g_strchr (stag_str, ERROR_CHAR) != NULL) {
      value_error (p, mode, ref_file);
    }
    str = a68g_strchr (stag_str, POINT_CHAR);
    if (frac_mould != NULL) {
      frac_str = &str[1];
    }
    if (str != NULL) {
      str[0] = NULL_CHAR;
    }
/* Stagnant sign. */
    if (sign_mould != NULL) {
      int digits = 0;
      count_zd_frames (SUB (sign_mould), &digits);
      if (digits > 0) {
        unsigned mood = DIGIT_BLANK | INSERTION_NORMAL;
        str = stag_str;
        if (str[0] == '+' || str[0] == '-') {
          shift_sign (SUB (sign_mould), &str);
        }
        write_mould (SUB (sign_mould), ref_file, SIGN_MOULD, &stag_str, &mood);
      } else {
        write_mould_put_sign (SUB (sign_mould), &stag_str);
      }
    } else if (sign < 0) {
      value_error (p, mode, ref_file);
    }
/* Stagnant part. */
    if (stag_mould != NULL) {
      unsigned mood = DIGIT_NORMAL | INSERTION_NORMAL;
      write_mould (SUB (stag_mould), ref_file, INTEGRAL_MOULD, &stag_str, &mood);
    }
/* Fraction. */
    if (frac_mould != NULL) {
      unsigned mood = DIGIT_NORMAL | INSERTION_NORMAL;
      if (point_frame != NULL) {
        write_pie_frame (point_frame, ref_file, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT);
      }
      write_mould (SUB (frac_mould), ref_file, INTEGRAL_MOULD, &frac_str, &mood);
    }
/* Exponent. */
    if (expo_mould != NULL) {
      A68_INT z;
      if (e_frame != NULL) {
        write_pie_frame (e_frame, ref_file, FORMAT_E_FRAME, FORMAT_ITEM_E);
      }
      z.status = INITIALISED_MASK;
      z.value = d_exp;
      write_integral_pattern (expo_mould, MODE (INT), (BYTE_T *) & z, ref_file);
    }
    stack_pointer = old_stack_pointer;
  }
}

/*!
\brief write COMPLEX value using complex pattern
\param p position in syntax tree, should not be NULL
\param comp
\param re
\param im
\param ref_file
**/

static void write_complex_pattern (NODE_T * p, MOID_T * comp, BYTE_T * re, BYTE_T * im, A68_REF ref_file)
{
  NODE_T *real, *plus_i_times, *imag;
/* Dissect pattern. */
  real = SUB (p);
  plus_i_times = NEXT (real);
  imag = NEXT (plus_i_times);
/* Write pattern. */
  write_real_pattern (real, comp, re, ref_file);
  write_pie_frame (plus_i_times, ref_file, FORMAT_I_FRAME, FORMAT_ITEM_I);
  write_real_pattern (imag, comp, im, ref_file);
}

/*!
\brief write BITS value using bits pattern
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void write_bits_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  if (mode == MODE (BITS)) {
    int width = 0, radix;
    unsigned mood;
    A68_BITS *z = (A68_BITS *) item;
    char *str;
/* Establish width and radix. */
    count_zd_frames (SUB (p), &width);
    radix = get_replicator_value (SUB (SUB (p)));
    if (radix < 2 || radix > 16) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
      exit_genie (p, A_RUNTIME_ERROR);
    }
/* Generate string of correct width. */
    reset_transput_buffer (EDIT_BUFFER);
    if (!convert_radix (p, z->value, radix, width)) {
      errno = EDOM;
      value_error (p, mode, ref_file);
    }
/* Output the edited string. */
    mood = DIGIT_NORMAL & INSERTION_NORMAL;
    str = get_transput_buffer (EDIT_BUFFER);
    write_mould (NEXT_SUB (p), ref_file, INTEGRAL_MOULD, &str, &mood);
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    ADDR_T pop_sp = stack_pointer;
    int width = 0, radix, digits = get_mp_digits (mode);
    unsigned mood;
    MP_DIGIT_T *u = (MP_DIGIT_T *) item;
    MP_DIGIT_T *v;
    MP_DIGIT_T *w;
    char *str;
    STACK_MP (v, p, digits);
    STACK_MP (w, p, digits);
/* Establish width and radix. */
    count_zd_frames (SUB (p), &width);
    radix = get_replicator_value (SUB (SUB (p)));
    if (radix < 2 || radix > 16) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
      exit_genie (p, A_RUNTIME_ERROR);
    }
/* Generate string of correct width. */
    reset_transput_buffer (EDIT_BUFFER);
    if (!convert_radix_mp (p, u, radix, width, mode, v, w)) {
      errno = EDOM;
      value_error (p, mode, ref_file);
    }
/* Output the edited string. */
    mood = DIGIT_NORMAL & INSERTION_NORMAL;
    str = get_transput_buffer (EDIT_BUFFER);
    write_mould (NEXT_SUB (p), ref_file, INTEGRAL_MOULD, &str, &mood);
    stack_pointer = pop_sp;
  }
}

/*!
\brief write value to file
\param p position in syntax tree, should not be NULL
\param item
\param ref_file
\return
**/

static void genie_write_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) == NULL) {
    genie_value_to_string (p, MODE (REAL), item);
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
    write_number_generic (p, MODE (REAL), item);
  } else if (WHETHER (p, FIXED_C_PATTERN) || WHETHER (p, FLOAT_C_PATTERN)) {
    write_number_c_style (p, MODE (REAL), item, ref_file);
  } else if (WHETHER (p, REAL_PATTERN)) {
    write_real_pattern (p, MODE (REAL), item, ref_file);
  } else if (WHETHER (p, COMPLEX_PATTERN)) {
    A68_REAL im;
    im.status = INITIALISED_MASK;
    im.value = 0.0;
    write_complex_pattern (p, MODE (REAL), (BYTE_T *) item, (BYTE_T *) & im, ref_file);
  } else {
    pattern_error (p, MODE (REAL), ATTRIBUTE (p));
  }
}

/*!
\brief write value to file
\param p position in syntax tree, should not be NULL
\param item
\param ref_file
**/

static void genie_write_long_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) == NULL) {
    genie_value_to_string (p, MODE (LONG_REAL), item);
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
    write_number_generic (p, MODE (LONG_REAL), item);
  } else if (WHETHER (p, FIXED_C_PATTERN) || WHETHER (p, FLOAT_C_PATTERN)) {
    write_number_c_style (p, MODE (LONG_REAL), item, ref_file);
  } else if (WHETHER (p, REAL_PATTERN)) {
    write_real_pattern (p, MODE (LONG_REAL), item, ref_file);
  } else if (WHETHER (p, COMPLEX_PATTERN)) {
    ADDR_T old_stack_pointer = stack_pointer;
    MP_DIGIT_T *z;
    STACK_MP (z, p, get_mp_digits (MODE (LONG_REAL)));
    SET_MP_ZERO (z, get_mp_digits (MODE (LONG_REAL)));
    z[0] = INITIALISED_MASK;
    write_complex_pattern (p, MODE (LONG_REAL), item, (BYTE_T *) z, ref_file);
    stack_pointer = old_stack_pointer;
  } else {
    pattern_error (p, MODE (LONG_REAL), ATTRIBUTE (p));
  }
}

/*!
\brief write value to file
\param p position in syntax tree, should not be NULL
\param item
\param ref_file
\return
**/

static void genie_write_longlong_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) == NULL) {
    genie_value_to_string (p, MODE (LONGLONG_REAL), item);
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
    write_number_generic (p, MODE (LONGLONG_REAL), item);
  } else if (WHETHER (p, FIXED_C_PATTERN) || WHETHER (p, FLOAT_C_PATTERN)) {
    write_number_c_style (p, MODE (LONGLONG_REAL), item, ref_file);
  } else if (WHETHER (p, REAL_PATTERN)) {
    write_real_pattern (p, MODE (LONGLONG_REAL), item, ref_file);
  } else if (WHETHER (p, COMPLEX_PATTERN)) {
    ADDR_T old_stack_pointer = stack_pointer;
    MP_DIGIT_T *z;
    STACK_MP (z, p, get_mp_digits (MODE (LONGLONG_REAL)));
    SET_MP_ZERO (z, get_mp_digits (MODE (LONGLONG_REAL)));
    z[0] = INITIALISED_MASK;
    write_complex_pattern (p, MODE (LONGLONG_REAL), item, (BYTE_T *) z, ref_file);
    stack_pointer = old_stack_pointer;
  } else {
    pattern_error (p, MODE (LONGLONG_REAL), ATTRIBUTE (p));
  }
}

/*!
\brief write value to file
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void genie_write_standard_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  RESET_ERRNO;
  if (mode == MODE (INT)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      genie_value_to_string (p, mode, item);
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NULL) {
      write_number_generic (pat, MODE (INT), item);
    } else if (WHETHER (pat, INTEGRAL_C_PATTERN) || WHETHER (pat, FIXED_C_PATTERN) || WHETHER (pat, FLOAT_C_PATTERN)) {
      write_number_c_style (pat, MODE (INT), item, ref_file);
    } else if (WHETHER (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, MODE (INT), item, ref_file);
    } else if (WHETHER (pat, REAL_PATTERN)) {
      write_real_pattern (pat, MODE (INT), item, ref_file);
    } else if (WHETHER (pat, COMPLEX_PATTERN)) {
      A68_REAL re, im;
      re.status = INITIALISED_MASK;
      re.value = (double) ((A68_INT *) item)->value, im.status = INITIALISED_MASK;
      im.value = 0.0;
      write_complex_pattern (pat, MODE (REAL), (BYTE_T *) & re, (BYTE_T *) & im, ref_file);
    } else if (WHETHER (pat, CHOICE_PATTERN)) {
      int k = ((A68_INT *) item)->value;
      write_choice_pattern (NEXT_SUB (pat), ref_file, &k);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (LONG_INT)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      genie_value_to_string (p, mode, item);
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NULL) {
      write_number_generic (pat, MODE (LONG_INT), item);
    } else if (WHETHER (pat, INTEGRAL_C_PATTERN) || WHETHER (pat, FIXED_C_PATTERN) || WHETHER (pat, FLOAT_C_PATTERN)) {
      write_number_c_style (pat, MODE (LONG_INT), item, ref_file);
    } else if (WHETHER (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, MODE (LONG_INT), item, ref_file);
    } else if (WHETHER (pat, REAL_PATTERN)) {
      write_real_pattern (pat, MODE (LONG_INT), item, ref_file);
    } else if (WHETHER (pat, COMPLEX_PATTERN)) {
      ADDR_T old_stack_pointer = stack_pointer;
      MP_DIGIT_T *z;
      STACK_MP (z, p, get_mp_digits (mode));
      SET_MP_ZERO (z, get_mp_digits (mode));
      z[0] = INITIALISED_MASK;
      write_complex_pattern (pat, MODE (LONG_REAL), item, (BYTE_T *) z, ref_file);
      stack_pointer = old_stack_pointer;
    } else if (WHETHER (pat, CHOICE_PATTERN)) {
      int k = mp_to_int (p, (MP_DIGIT_T *) item, get_mp_digits (mode));
      write_choice_pattern (NEXT_SUB (pat), ref_file, &k);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (LONGLONG_INT)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      genie_value_to_string (p, mode, item);
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NULL) {
      write_number_generic (pat, MODE (LONGLONG_INT), item);
    } else if (WHETHER (pat, INTEGRAL_C_PATTERN) || WHETHER (pat, FIXED_C_PATTERN) || WHETHER (pat, FLOAT_C_PATTERN)) {
      write_number_c_style (pat, MODE (LONGLONG_INT), item, ref_file);
    } else if (WHETHER (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, MODE (LONGLONG_INT), item, ref_file);
    } else if (WHETHER (pat, REAL_PATTERN)) {
      write_real_pattern (pat, MODE (INT), item, ref_file);
    } else if (WHETHER (pat, REAL_PATTERN)) {
      write_real_pattern (pat, MODE (LONGLONG_INT), item, ref_file);
    } else if (WHETHER (pat, COMPLEX_PATTERN)) {
      ADDR_T old_stack_pointer = stack_pointer;
      MP_DIGIT_T *z;
      STACK_MP (z, p, get_mp_digits (MODE (LONGLONG_REAL)));
      SET_MP_ZERO (z, get_mp_digits (mode));
      z[0] = INITIALISED_MASK;
      write_complex_pattern (pat, MODE (LONGLONG_REAL), item, (BYTE_T *) z, ref_file);
      stack_pointer = old_stack_pointer;
    } else if (WHETHER (pat, CHOICE_PATTERN)) {
      int k = mp_to_int (p, (MP_DIGIT_T *) item, get_mp_digits (mode));
      write_choice_pattern (NEXT_SUB (pat), ref_file, &k);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (REAL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_real_format (pat, item, ref_file);
  } else if (mode == MODE (LONG_REAL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_long_real_format (pat, item, ref_file);
  } else if (mode == MODE (LONGLONG_REAL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_longlong_real_format (pat, item, ref_file);
  } else if (mode == MODE (COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, MODE (REAL), &item[0], &item[MOID_SIZE (MODE (REAL))], ref_file);
    } else {
/* Try writing as two REAL values. */
      genie_write_real_format (pat, item, ref_file);
      genie_write_standard_format (p, MODE (REAL), &item[MOID_SIZE (MODE (REAL))], ref_file);
    }
  } else if (mode == MODE (LONG_COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, MODE (LONG_REAL), &item[0], &item[MOID_SIZE (MODE (LONG_REAL))], ref_file);
    } else {
/* Try writing as two LONG REAL values. */
      genie_write_long_real_format (pat, item, ref_file);
      genie_write_standard_format (p, MODE (LONG_REAL), &item[MOID_SIZE (MODE (LONG_REAL))], ref_file);
    }
  } else if (mode == MODE (LONGLONG_COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, MODE (LONGLONG_REAL), &item[0], &item[MOID_SIZE (MODE (LONGLONG_REAL))], ref_file);
    } else {
/* Try writing as two LONG LONG REAL values. */
      genie_write_longlong_real_format (pat, item, ref_file);
      genie_write_standard_format (p, MODE (LONGLONG_REAL), &item[MOID_SIZE (MODE (LONGLONG_REAL))], ref_file);
    }
  } else if (mode == MODE (BOOL)) {
    A68_BOOL *z = (A68_BOOL *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, (z->value == A_TRUE ? FLIP_CHAR : FLOP_CHAR));
    } else if (WHETHER (pat, BOOLEAN_PATTERN)) {
      if (NEXT_SUB (pat) == NULL) {
        add_char_transput_buffer (p, FORMATTED_BUFFER, (z->value == A_TRUE ? FLIP_CHAR : FLOP_CHAR));
      } else {
        write_boolean_pattern (pat, ref_file, z->value == A_TRUE);
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (BITS)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      char *str = (char *) STACK_TOP;
      genie_value_to_string (p, mode, item);
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else if (WHETHER (pat, BITS_PATTERN)) {
      write_bits_pattern (pat, MODE (BITS), item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      char *str = (char *) STACK_TOP;
      genie_value_to_string (p, mode, item);
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else if (WHETHER (pat, BITS_PATTERN)) {
      write_bits_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (CHAR)) {
    A68_CHAR *z = (A68_CHAR *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      add_char_transput_buffer (p, FORMATTED_BUFFER, z->value);
    } else if (WHETHER (pat, STRING_PATTERN)) {
      char *q = get_transput_buffer (EDIT_BUFFER);
      add_char_transput_buffer (p, EDIT_BUFFER, z->value);
      write_string_pattern (pat, mode, ref_file, &q);
      if (q[0] != NULL_CHAR) {
        value_error (p, mode, ref_file);
      }
    } else if (WHETHER (pat, STRING_C_PATTERN)) {
      char q[2];
      q[0] = z->value;
      q[1] = NULL_CHAR;
      write_string_c_style (pat, q);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
/* Handle these separately instead of printing [] CHAR. */
    A68_REF row = *(A68_REF *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      PUSH_REF (p, row);
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (WHETHER (pat, STRING_PATTERN)) {
      char *q;
      PUSH_REF (p, row);
      reset_transput_buffer (EDIT_BUFFER);
      add_string_from_stack_transput_buffer (p, EDIT_BUFFER);
      q = get_transput_buffer (EDIT_BUFFER);
      write_string_pattern (pat, mode, ref_file, &q);
      if (q[0] != NULL_CHAR) {
        value_error (p, mode, ref_file);
      }
    } else if (WHETHER (pat, STRING_C_PATTERN)) {
      char *q;
      PUSH_REF (p, row);
      reset_transput_buffer (EDIT_BUFFER);
      add_string_from_stack_transput_buffer (p, EDIT_BUFFER);
      q = get_transput_buffer (EDIT_BUFFER);
      write_string_c_style (pat, q);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (WHETHER (mode, UNION_SYMBOL)) {
    A68_POINTER *z = (A68_POINTER *) item;
    genie_write_standard_format (p, (MOID_T *) (z->value), &item[SIZE_OF (A68_POINTER)], ref_file);
  } else if (WHETHER (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NULL; FORWARD (q)) {
      BYTE_T *elem = &item[q->offset];
      GENIE_CHECK_INITIALISATION (p, elem, MOID (q));
      genie_write_standard_format (p, MOID (q), elem, ref_file);
    }
  } else if (WHETHER (mode, ROW_SYMBOL) || WHETHER (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    TEST_INIT (p, *(A68_REF *) item, MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, arr->dimensions) != 0) {
      BYTE_T *base_addr = ADDRESS (&arr->array);
      BOOL_T done = A_FALSE;
      initialise_internal_index (tup, arr->dimensions);
      while (!done) {
        ADDR_T index = calculate_internal_index (tup, arr->dimensions);
        ADDR_T elem_addr = ROW_ELEMENT (arr, index);
        BYTE_T *elem = &base_addr[elem_addr];
        GENIE_CHECK_INITIALISATION (p, elem, SUB (deflexed));
        genie_write_standard_format (p, SUB (deflexed), elem, ref_file);
        done = increment_internal_index (tup, arr->dimensions);
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief at end of write purge all insertions
\param p position in syntax tree, should not be NULL
\param ref_file
**/

static void purge_format_write (NODE_T * p, A68_REF ref_file)
{
/* Problem here is shutting down embedded formats. */
  BOOL_T go_on;
  do {
    A68_FILE *file;
    NODE_T *dollar, *pat;
    A68_FORMAT *old_fmt;
    while ((pat = get_next_format_pattern (p, ref_file, SKIP_PATTERN)) != NULL) {
      format_error (p, ref_file);
    }
    file = FILE_DEREF (&ref_file);
    dollar = SUB (file->format.body);
    old_fmt = (A68_FORMAT *) FRAME_LOCAL (frame_pointer, TAX (dollar)->offset);
    go_on = !IS_NIL_FORMAT (old_fmt);
    if (go_on) {
/* Pop embedded format and proceed. */
      end_of_format (p, ref_file);
    }
  }
  while (go_on);
}

/*!
\brief PROC ([] SIMPLOUT) VOID print f, write f
\param p position in syntax tree, should not be NULL
**/

void genie_write_format (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_out (p);
  PUSH_REF (p, row);
  genie_write_file_format (p);
}

/*!
\brief PROC (REF FILE, [] SIMPLOUT) VOID put f
\param p position in syntax tree, should not be NULL
**/

void genie_write_file_format (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index, formats;
  ADDR_T save_frame_pointer, save_stack_pointer;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROW_SIMPLOUT));
  TEST_NIL (p, row, MODE (ROW_SIMPLOUT));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->read_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "read");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.put) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->read_mood && !file->write_mood) {
    if (IS_NIL (file->string)) {
      if ((file->fd = open_physical_file (p, ref_file, A_WRITE_ACCESS, A68_PROTECTION)) == -1) {
        open_error (p, ref_file, "putting");
      }
    } else {
      file->fd = open_physical_file (p, ref_file, A_READ_ACCESS, 0);
    }
    file->draw_mood = A_FALSE;
    file->read_mood = A_FALSE;
    file->write_mood = A_TRUE;
    file->char_mood = A_TRUE;
  }
  if (!file->char_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Save stack state since formats have frames. */
  save_frame_pointer = file->frame_pointer;
  save_stack_pointer = file->stack_pointer;
  file->frame_pointer = frame_pointer;
  file->stack_pointer = stack_pointer;
/* Process [] SIMPLOUT. */
  if (file->format.body != NULL) {
    open_format_frame (file, &(file->format), NOT_EMBEDDED_FORMAT, A_FALSE);
  }
  formats = 0;
  base_address = ADDRESS (&(arr->array));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_POINTER *z = (A68_POINTER *) & (base_address[elem_index]);
    MOID_T *mode = (MOID_T *) (z->value);
    BYTE_T *item = &(base_address[elem_index + SIZE_OF (A68_POINTER)]);
    if (mode == MODE (FORMAT)) {
/* Forget about eventual active formats and set up new one. */
      if (formats > 0) {
        purge_format_write (p, ref_file);
      }
      formats++;
      frame_pointer = file->frame_pointer;
      stack_pointer = file->stack_pointer;
      open_format_frame (file, (A68_FORMAT *) item, NOT_EMBEDDED_FORMAT, A_TRUE);
    } else if (mode == MODE (PROC_REF_FILE_VOID)) {
/* Ignore
	  genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
*/
    } else {
      genie_write_standard_format (p, mode, item, ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLOUT));
  }
/* Empty the format to purge insertions. */
  purge_format_write (p, ref_file);
  file->format.body = NULL;
/* Dump the buffer. */
  write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
/* Forget about active formats. */
  frame_pointer = file->frame_pointer;
  stack_pointer = file->stack_pointer;
  file->frame_pointer = save_frame_pointer;
  file->stack_pointer = save_stack_pointer;
}

/*!
\brief give a value error in case a character is not among expected ones
\param p position in syntax tree, should not be NULL
\param m
\param ref_file
\param items
\param ch
\return
**/

static BOOL_T expect (NODE_T * p, MOID_T * m, A68_REF ref_file, const char *items, char ch)
{
  if (a68g_strchr ((char *) items, ch) == NULL) {
    value_error (p, m, ref_file);
    return (A_FALSE);
  } else {
    return (A_TRUE);
  }
}

/*!
\brief read one char from file
\param p position in syntax tree, should not be NULL
\param ref_file
\return
**/

static char read_single_char (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *file = FILE_DEREF (&ref_file);
  int ch = char_scanner (file);
  if (ch == EOF_CHAR) {
    end_of_file_error (p, ref_file);
  }
  return (ch);
}

/*!
\brief scan n chars from file to input buffer
\param p position in syntax tree, should not be NULL
\param n
\param m
\param ref_file
**/

static void scan_n_chars (NODE_T * p, int n, MOID_T * m, A68_REF ref_file)
{
  int k;
  (void) m;
  for (k = 0; k < n; k++) {
    int ch = read_single_char (p, ref_file);
    add_char_transput_buffer (p, INPUT_BUFFER, ch);
  }
}


/*!
\brief read a group of insertions
\param p position in syntax tree, should not be NULL
\param ref_file
**/

void read_insertion (NODE_T * p, A68_REF ref_file)
{
/*
Algol68G does not check whether the insertions are textually there. It just
skips them. This because we blank literals in sign moulds before the sign is
put, which is non-standard Algol68, but convenient.
*/
  A68_FILE *file = FILE_DEREF (&ref_file);
  for (; p != NULL; FORWARD (p)) {
    read_insertion (SUB (p), ref_file);
    if (WHETHER (p, FORMAT_ITEM_L)) {
      BOOL_T go_on = !file->eof;
      while (go_on) {
        int ch = read_single_char (p, ref_file);
        go_on = (ch != NEWLINE_CHAR) && (ch != EOF_CHAR) && !file->eof;
      }
    } else if (WHETHER (p, FORMAT_ITEM_P)) {
      BOOL_T go_on = !file->eof;
      while (go_on) {
        int ch = read_single_char (p, ref_file);
        go_on = (ch != FORMFEED_CHAR) && (ch != EOF_CHAR) && !file->eof;
      }
    } else if (WHETHER (p, FORMAT_ITEM_X) || WHETHER (p, FORMAT_ITEM_Q)) {
      if (!file->eof) {
        (void) read_single_char (p, ref_file);
      }
    } else if (WHETHER (p, FORMAT_ITEM_Y)) {
      /*
       * Not implemented - parser has warned you. 
       */ ;
    } else if (WHETHER (p, LITERAL)) {
      /*
       * Skip characters, don't check whether the literal wordly is there. 
       */
      int len = strlen (SYMBOL (p));
      while (len-- && !file->eof) {
        (void) read_single_char (p, ref_file);
      }
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      if (ATTRIBUTE (SUB (NEXT (p))) != FORMAT_ITEM_K) {
        for (j = 1; j <= k; j++) {
          read_insertion (NEXT (p), ref_file);
        }
      } else {
        int pos = get_transput_buffer_index (INPUT_BUFFER);
        for (j = 1; j < (k - pos); j++) {
          if (!file->eof) {
            (void) read_single_char (p, ref_file);
          }
        }
      }
      return;                   /* Don't delete this! */
    }
  }
}

/*!
\brief read string from file according current format
\param p position in syntax tree, should not be NULL
\param m
\param ref_file
**/

static void read_string_pattern (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (WHETHER (p, FORMAT_ITEM_A)) {
      scan_n_chars (p, 1, m, ref_file);
    } else if (WHETHER (p, FORMAT_ITEM_S)) {
      add_char_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
      return;
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      for (j = 1; j <= k; j++) {
        read_string_pattern (NEXT (p), m, ref_file);
      }
      return;
    } else {
      read_string_pattern (SUB (p), m, ref_file);
    }
  }
}

/*!
\brief read string from file using C style format %[-][w]s
\param p position in syntax tree, should not be NULL
\param m
\param ref_file
**/

static void read_string_c_style (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  if (WHETHER (p, STRING_C_PATTERN)) {
    NODE_T *q = NEXT_SUB (p);
/* Skip sign. */
    if (WHETHER (q, FORMAT_ITEM_PLUS) || WHETHER (q, FORMAT_ITEM_MINUS)) {
      FORWARD (q);
    }
/* If width is specified than get exactly that width. */
    if (WHETHER (q, REPLICATOR)) {
      int width = get_replicator_value (SUB (q));
      scan_n_chars (p, width, m, ref_file);
    } else {
      genie_read_standard (p, MODE (ROW_CHAR), (BYTE_T *) get_transput_buffer (INPUT_BUFFER), ref_file);
    }
  }
}

/*!
\brief traverse choice pattern
\param p position in syntax tree, should not be NULL
\param str
\param len
\param count
\param matches
\param first_match
\param full_match
**/

static void traverse_choice_pattern (NODE_T * p, char *str, int len, int *count, int *matches, int *first_match, BOOL_T * full_match)
{
  for (; p != NULL; FORWARD (p)) {
    traverse_choice_pattern (SUB (p), str, len, count, matches, first_match, full_match);
    if (WHETHER (p, LITERAL)) {
      (*count)++;
      if (strncmp (SYMBOL (p), str, len) == 0) {
        (*matches)++;
        *full_match |= (strcmp (SYMBOL (p), str) == 0);
        if (*first_match == 0 && *full_match) {
          *first_match = *count;
        }
      }
    }
  }
}

/*!
\brief read appropriate insertion from a choice pattern
\param p position in syntax tree, should not be NULL
\param ref_file
\return
**/

static int read_choice_pattern (NODE_T * p, A68_REF ref_file)
{
/*
This implementation does not have the RR peculiarity that longest
matching literal must be first, in case of non-unique first chars.
*/
  A68_FILE *file = FILE_DEREF (&ref_file);
  BOOL_T cont = A_TRUE;
  int longest_match = 0, longest_match_len = 0;
  while (cont) {
    int ch = char_scanner (file);
    if (!file->eof) {
      int len, count = 0, matches = 0, first_match = 0;
      BOOL_T full_match = A_FALSE;
      add_char_transput_buffer (p, INPUT_BUFFER, ch);
      len = get_transput_buffer_index (INPUT_BUFFER);
      traverse_choice_pattern (p, get_transput_buffer (INPUT_BUFFER), len, &count, &matches, &first_match, &full_match);
      if (full_match && matches == 1 && first_match > 0) {
        return (first_match);
      } else if (full_match && matches > 1 && first_match > 0) {
        longest_match = first_match;
        longest_match_len = len;
      } else if (matches == 0) {
        cont = A_FALSE;
      }
    } else {
      cont = A_FALSE;
    }
  }
  if (longest_match > 0) {
/* Push back look-ahead chars. */
    if (get_transput_buffer_index (INPUT_BUFFER) > 0) {
      char *z = get_transput_buffer (INPUT_BUFFER);
      file->eof = A_FALSE;
      add_string_transput_buffer (p, file->transput_buffer, &z[longest_match_len]);
    }
    return (longest_match);
  } else {
    value_error (p, MODE (INT), ref_file);
    return (0);
  }
}

/*!
\brief read value according to a general-pattern
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void read_number_generic (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_REF row;
  EXECUTE_UNIT (NEXT_SUB (p));
/* RR says to ignore parameters just calculated, so we will. */
  POP_REF (p, &row);
  genie_read_standard (p, mode, item, ref_file);
}

/*!
\brief handle %[+][-][w]d, %[+][-][w][.][d]f/e formats
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
\return
**/

static void read_number_c_style (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  BOOL_T sign;
  int width, after;
/* Integral C pattern. */
  if (WHETHER (p, INTEGRAL_C_PATTERN)) {
    NODE_T *q = NEXT_SUB (p);
    if (mode != MODE (INT) && mode != MODE (LONG_INT) && mode != MODE (LONGLONG_INT)) {
      pattern_error (p, mode, ATTRIBUTE (p));
      return;
    }
/* Get sign. */
    if (WHETHER (q, FORMAT_ITEM_PLUS) || WHETHER (q, FORMAT_ITEM_MINUS)) {
      sign = ATTRIBUTE (q);
      FORWARD (q);
    } else {
      sign = 0;
    }
/* Get width. */
    if (WHETHER (q, REPLICATOR)) {
      width = get_replicator_value (SUB (q));
    } else {
      width = 0;
    }
/* Read string. */
    if (width == 0) {
      genie_read_standard (p, mode, item, ref_file);
    } else {
      scan_n_chars (p, (sign != 0) ? width + 1 : width, mode, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    }
  }
/* Real C patterns. */
  else if (WHETHER (p, FIXED_C_PATTERN) || WHETHER (p, FLOAT_C_PATTERN)) {
    NODE_T *q = NEXT_SUB (p);
    if (mode != MODE (REAL) && mode != MODE (LONG_REAL) && mode != MODE (LONGLONG_REAL)) {
      pattern_error (p, mode, ATTRIBUTE (p));
      return;
    }
/* Get sign. */
    if (WHETHER (q, FORMAT_ITEM_PLUS) || WHETHER (q, FORMAT_ITEM_MINUS)) {
      sign = ATTRIBUTE (q);
      FORWARD (q);
    } else {
      sign = 0;
    }
/* Get width. */
    if (WHETHER (q, REPLICATOR)) {
      width = get_replicator_value (SUB (q));
      FORWARD (q);
    } else {
      width = 0;
    }
/* Skip point. */
    if (WHETHER (q, FORMAT_ITEM_POINT)) {
      FORWARD (q);
    }
/* Get decimals. */
    if (WHETHER (q, REPLICATOR)) {
      after = get_replicator_value (SUB (q));
      FORWARD (q);
    } else {
      after = 0;
    }
/* Read string. */
    if (width == 0) {
      genie_read_standard (p, mode, item, ref_file);
    } else {
      scan_n_chars (p, (sign != 0) ? width + 1 : width, mode, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    }
  }
}

/* INTEGRAL, REAL, COMPLEX and BITS patterns. */

/*!
\brief read sign-mould according current format
\param p position in syntax tree, should not be NULL
\param m
\param ref_file
\param sign
**/

static void read_sign_mould (NODE_T * p, MOID_T * m, A68_REF ref_file, int *sign)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      for (j = 1; j <= k; j++) {
        read_sign_mould (NEXT (p), m, ref_file, sign);
      }
      return;                   /* Leave this! */
    } else {
      switch (ATTRIBUTE (p)) {
      case FORMAT_ITEM_Z:
      case FORMAT_ITEM_D:
      case FORMAT_ITEM_S:
      case FORMAT_ITEM_PLUS:
      case FORMAT_ITEM_MINUS:
        {
          int ch = read_single_char (p, ref_file);
/* When a sign has been read, digits are expected. */
          if (*sign != 0) {
            if (expect (p, m, ref_file, INT_DIGITS, ch)) {
              add_char_transput_buffer (p, INPUT_BUFFER, ch);
            } else {
              add_char_transput_buffer (p, INPUT_BUFFER, '0');
            }
/* When a sign has not been read, a sign is expected.  If there is a digit
   in stead of a sign, the digit is accepted and '+' is assumed; RR demands a
   space to preceed the digit, Algol68G does not. */
          } else {
            if (a68g_strchr (SIGN_DIGITS, ch) != NULL) {
              if (ch == '+') {
                *sign = 1;
              } else if (ch == '-') {
                *sign = -1;
              } else if (ch == BLANK_CHAR) {
                /*
                 * skip. 
                 */ ;
              }
            } else if (expect (p, m, ref_file, INT_DIGITS, ch)) {
              add_char_transput_buffer (p, INPUT_BUFFER, ch);
              *sign = 1;
            }
          }
          break;
        }
      default:
        {
          read_sign_mould (SUB (p), m, ref_file, sign);
          break;
        }
      }
    }
  }
}

/*!
\brief read mould according current format
\param p position in syntax tree, should not be NULL
\param m
\param ref_file
**/

static void read_integral_mould (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (WHETHER (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p));
      for (j = 1; j <= k; j++) {
        read_integral_mould (NEXT (p), m, ref_file);
      }
      return;                   /* Leave this! */
    } else if (WHETHER (p, FORMAT_ITEM_Z)) {
      int ch = read_single_char (p, ref_file);
      const char *digits = (m == MODE (BITS) || m == MODE (LONG_BITS) || m == MODE (LONGLONG_BITS)) ? BITS_DIGITS_BLANK : INT_DIGITS_BLANK;
      if (expect (p, m, ref_file, digits, ch)) {
        add_char_transput_buffer (p, INPUT_BUFFER, (ch == BLANK_CHAR) ? '0' : ch);
      } else {
        add_char_transput_buffer (p, INPUT_BUFFER, '0');
      }
    } else if (WHETHER (p, FORMAT_ITEM_D)) {
      int ch = read_single_char (p, ref_file);
      const char *digits = (m == MODE (BITS) || m == MODE (LONG_BITS) || m == MODE (LONGLONG_BITS)) ? BITS_DIGITS : INT_DIGITS;
      if (expect (p, m, ref_file, digits, ch)) {
        add_char_transput_buffer (p, INPUT_BUFFER, ch);
      } else {
        add_char_transput_buffer (p, INPUT_BUFFER, '0');
      }
    } else if (WHETHER (p, FORMAT_ITEM_S)) {
      add_char_transput_buffer (p, INPUT_BUFFER, '0');
    } else {
      read_integral_mould (SUB (p), m, ref_file);
    }
  }
}

/*!
\brief read mould according current format
\param p position in syntax tree, should not be NULL
\param m
\param item
\param ref_file
**/

static void read_integral_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
  NODE_T *q = SUB (p);
  if (q != NULL && WHETHER (q, SIGN_MOULD)) {
    int sign = 0;
    char *z;
    add_char_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
    read_sign_mould (SUB (q), m, ref_file, &sign);
    z = get_transput_buffer (INPUT_BUFFER);
    z[0] = (sign == -1) ? '-' : '+';
    FORWARD (q);
  }
  if (q != NULL && WHETHER (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
  }
  genie_string_to_value (p, m, item, ref_file);
}

/*!
\brief read point, exponent or i-frame
\param p position in syntax tree, should not be NULL
\param m
\param ref_file
\param att
\param item
\param ch
**/

static void read_pie_frame (NODE_T * p, MOID_T * m, A68_REF ref_file, int att, int item, char ch)
{
/* Widen ch to a stringlet. */
  char sym[3];
  sym[0] = ch;
  sym[1] = TO_LOWER (ch);
  sym[2] = NULL_CHAR;
/* Now read the frame. */
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, INSERTION)) {
      read_insertion (p, ref_file);
    } else if (WHETHER (p, att)) {
      read_pie_frame (SUB (p), m, ref_file, att, item, ch);
      return;
    } else if (WHETHER (p, FORMAT_ITEM_S)) {
      add_char_transput_buffer (p, INPUT_BUFFER, sym[0]);
      return;
    } else if (WHETHER (p, item)) {
      int ch0 = read_single_char (p, ref_file);
      if (expect (p, m, ref_file, sym, ch0)) {
        add_char_transput_buffer (p, INPUT_BUFFER, sym[0]);
      } else {
        add_char_transput_buffer (p, INPUT_BUFFER, sym[0]);
      }
    }
  }
}

/*!
\brief read REAL value using real pattern
\param p position in syntax tree, should not be NULL
\param m
\param item
\param ref_file
**/

static void read_real_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
/* Dive into pattern. */
  NODE_T *q = (WHETHER (p, REAL_PATTERN)) ? SUB (p) : p;
/* Dissect pattern. */
  if (q != NULL && WHETHER (q, SIGN_MOULD)) {
    int sign = 0;
    char *z;
    add_char_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
    read_sign_mould (SUB (q), m, ref_file, &sign);
    z = get_transput_buffer (INPUT_BUFFER);
    z[0] = (sign == -1) ? '-' : '+';
    FORWARD (q);
  }
  if (q != NULL && WHETHER (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
    FORWARD (q);
  }
  if (q != NULL && WHETHER (q, FORMAT_POINT_FRAME)) {
    read_pie_frame (SUB (q), m, ref_file, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT, POINT_CHAR);
    FORWARD (q);
  }
  if (q != NULL && WHETHER (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
    FORWARD (q);
  }
  if (q != NULL && WHETHER (q, EXPONENT_FRAME)) {
    read_pie_frame (SUB (q), m, ref_file, FORMAT_E_FRAME, FORMAT_ITEM_E, EXPONENT_CHAR);
    q = NEXT_SUB (q);
    if (q != NULL && WHETHER (q, SIGN_MOULD)) {
      int k, sign = 0;
      char *z;
      add_char_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
      k = get_transput_buffer_index (INPUT_BUFFER);
      read_sign_mould (SUB (q), m, ref_file, &sign);
      z = get_transput_buffer (INPUT_BUFFER);
      z[k - 1] = (sign == -1) ? '-' : '+';
      FORWARD (q);
    }
    if (q != NULL && WHETHER (q, INTEGRAL_MOULD)) {
      read_integral_mould (SUB (q), m, ref_file);
      FORWARD (q);
    }
  }
  genie_string_to_value (p, m, item, ref_file);
}

/*!
\brief read COMPLEX value using complex pattern
\param p position in syntax tree, should not be NULL
\param comp
\param m
\param re
\param im
\param ref_file
**/

static void read_complex_pattern (NODE_T * p, MOID_T * comp, MOID_T * m, BYTE_T * re, BYTE_T * im, A68_REF ref_file)
{
  NODE_T *real, *plus_i_times, *imag;
/* Dissect pattern. */
  real = SUB (p);
  plus_i_times = NEXT (real);
  imag = NEXT (plus_i_times);
/* Read pattern. */
  read_real_pattern (real, m, re, ref_file);
  reset_transput_buffer (INPUT_BUFFER);
  read_pie_frame (plus_i_times, comp, ref_file, FORMAT_I_FRAME, FORMAT_ITEM_I, 'I');
  reset_transput_buffer (INPUT_BUFFER);
  read_real_pattern (imag, m, im, ref_file);
}

/*!
\brief read BITS value according pattern
\param p position in syntax tree, should not be NULL
\param m
\param item
\param ref_file
**/

static void read_bits_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
  int radix;
  char *z;
  radix = get_replicator_value (SUB (SUB (p)));
  if (radix < 2 || radix > 16) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  z = get_transput_buffer (INPUT_BUFFER);
  snprintf (z, TRANSPUT_BUFFER_SIZE, "%dr", radix);
  set_transput_buffer_index (INPUT_BUFFER, strlen (z));
  read_integral_mould (NEXT_SUB (p), m, ref_file);
  genie_string_to_value (p, m, item, ref_file);
}

/*!
\brief read object with from file and store
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void genie_read_real_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) == NULL) {
    genie_read_standard (p, mode, item, ref_file);
  } else if (WHETHER (p, GENERAL_PATTERN) && NEXT_SUB (p) != NULL) {
    read_number_generic (p, mode, item, ref_file);
  } else if (WHETHER (p, FIXED_C_PATTERN) || WHETHER (p, FLOAT_C_PATTERN)) {
    read_number_c_style (p, mode, item, ref_file);
  } else if (WHETHER (p, REAL_PATTERN)) {
    read_real_pattern (p, mode, item, ref_file);
  } else {
    pattern_error (p, mode, ATTRIBUTE (p));
  }
}

/*!
\brief read object with from file and store
\param p position in syntax tree, should not be NULL
\param mode
\param item
\param ref_file
**/

static void genie_read_standard_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  RESET_ERRNO;
  reset_transput_buffer (INPUT_BUFFER);
  if (mode == MODE (INT) || mode == MODE (LONG_INT) || mode == MODE (LONGLONG_INT)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      genie_read_standard (pat, mode, item, ref_file);
    } else if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NULL) {
      read_number_generic (pat, mode, item, ref_file);
    } else if (WHETHER (pat, INTEGRAL_C_PATTERN)) {
      read_number_c_style (pat, mode, item, ref_file);
    } else if (WHETHER (pat, INTEGRAL_PATTERN)) {
      read_integral_pattern (pat, mode, item, ref_file);
    } else if (WHETHER (pat, CHOICE_PATTERN)) {
      int k = read_choice_pattern (pat, ref_file);
      if (mode == MODE (INT)) {
        A68_INT *z = (A68_INT *) item;
        z->value = k;
        z->status = (z->value > 0) ? INITIALISED_MASK : NULL_MASK;
      } else {
        MP_DIGIT_T *z = (MP_DIGIT_T *) item;
        if (k > 0) {
          int_to_mp (p, z, k, get_mp_digits (mode));
          z[0] = INITIALISED_MASK;
        } else {
          z[0] = NULL_MASK;
        }
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (REAL) || mode == MODE (LONG_REAL) || mode == MODE (LONGLONG_REAL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_read_real_format (pat, mode, item, ref_file);
  } else if (mode == MODE (COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, MODE (REAL), item, &item[MOID_SIZE (MODE (REAL))], ref_file);
    } else {
/* Try reading as two REAL values. */
      genie_read_real_format (pat, MODE (REAL), item, ref_file);
      genie_read_standard_format (p, MODE (REAL), &item[MOID_SIZE (MODE (REAL))], ref_file);
    }
  } else if (mode == MODE (LONG_COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, MODE (LONG_REAL), item, &item[MOID_SIZE (MODE (LONG_REAL))], ref_file);
    } else {
/* Try reading as two LONG REAL values. */
      genie_read_real_format (pat, MODE (LONG_REAL), item, ref_file);
      genie_read_standard_format (p, MODE (LONG_REAL), &item[MOID_SIZE (MODE (LONG_REAL))], ref_file);
    }
  } else if (mode == MODE (LONGLONG_COMPLEX)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, MODE (LONGLONG_REAL), item, &item[MOID_SIZE (MODE (LONGLONG_REAL))], ref_file);
    } else {
/* Try reading as two LONG LONG REAL values. */
      genie_read_real_format (pat, MODE (LONGLONG_REAL), item, ref_file);
      genie_read_standard_format (p, MODE (LONGLONG_REAL), &item[MOID_SIZE (MODE (LONGLONG_REAL))], ref_file);
    }
  } else if (mode == MODE (BOOL)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (WHETHER (pat, BOOLEAN_PATTERN)) {
      if (NEXT_SUB (pat) == NULL) {
        genie_read_standard (p, mode, item, ref_file);
      } else {
        A68_BOOL *z = (A68_BOOL *) item;
        int k = read_choice_pattern (pat, ref_file);
        if (k == 1 || k == 2) {
          z->value = (k == 1) ? A_TRUE : A_FALSE;
          z->status = INITIALISED_MASK;
        } else {
          z->status = NULL_MASK;
        }
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (BITS) || mode == MODE (LONG_BITS) || mode == MODE (LONGLONG_BITS)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (WHETHER (pat, BITS_PATTERN)) {
      read_bits_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (CHAR)) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (WHETHER (pat, STRING_PATTERN)) {
      read_string_pattern (pat, MODE (CHAR), ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    } else if (WHETHER (pat, STRING_C_PATTERN)) {
      read_string_pattern (pat, MODE (CHAR), ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
/* Handle these separately instead of reading [] CHAR. */
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (WHETHER (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NULL) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (WHETHER (pat, STRING_PATTERN)) {
      read_string_pattern (pat, mode, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    } else if (WHETHER (pat, STRING_C_PATTERN)) {
      read_string_c_style (pat, mode, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (WHETHER (mode, UNION_SYMBOL)) {
    A68_POINTER *z = (A68_POINTER *) item;
    genie_read_standard_format (p, (MOID_T *) (z->value), &item[SIZE_OF (A68_POINTER)], ref_file);
  } else if (WHETHER (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    for (; q != NULL; FORWARD (q)) {
      BYTE_T *elem = &item[q->offset];
      genie_read_standard_format (p, MOID (q), elem, ref_file);
    }
  } else if (WHETHER (mode, ROW_SYMBOL) || WHETHER (mode, FLEX_SYMBOL)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    TEST_INIT (p, *(A68_REF *) item, MODE (ROWS));
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, arr->dimensions) != 0) {
      BYTE_T *base_addr = ADDRESS (&arr->array);
      BOOL_T done = A_FALSE;
      initialise_internal_index (tup, arr->dimensions);
      while (!done) {
        ADDR_T index = calculate_internal_index (tup, arr->dimensions);
        ADDR_T elem_addr = ROW_ELEMENT (arr, index);
        BYTE_T *elem = &base_addr[elem_addr];
        genie_read_standard_format (p, SUB (deflexed), elem, ref_file);
        done = increment_internal_index (tup, arr->dimensions);
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

/*!
\brief at end of read purge all insertions
\param p position in syntax tree, should not be NULL
\param ref_file
**/

static void purge_format_read (NODE_T * p, A68_REF ref_file)
{
  BOOL_T go_on;
  do {
    A68_FILE *file;
    NODE_T *dollar, *pat;
    A68_FORMAT *old_fmt;
/*
    while (get_next_format_pattern (p, ref_file, SKIP_PATTERN) != NULL) {
	;
    }
*/
    while ((pat = get_next_format_pattern (p, ref_file, SKIP_PATTERN)) != NULL) {
      format_error (p, ref_file);
    }
    file = FILE_DEREF (&ref_file);
    dollar = SUB (file->format.body);
    old_fmt = (A68_FORMAT *) FRAME_LOCAL (frame_pointer, TAX (dollar)->offset);
    go_on = !IS_NIL_FORMAT (old_fmt);
    if (go_on) {
/* Pop embedded format and proceed. */
      end_of_format (p, ref_file);
    }
  }
  while (go_on);
}

/*!
\brief PROC ([] SIMPLIN) VOID read f
\param p position in syntax tree, should not be NULL
**/

void genie_read_format (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_in (p);
  PUSH_REF (p, row);
  genie_read_file_format (p);
}

/*!
\brief PROC (REF FILE, [] SIMPLIN) VOID get f
\param p position in syntax tree, should not be NULL
**/

void genie_read_file_format (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index, formats;
  ADDR_T save_frame_pointer, save_stack_pointer;
  POP_REF (p, &row);
  TEST_INIT (p, row, MODE (ROW_SIMPLIN));
  TEST_NIL (p, row, MODE (ROW_SIMPLIN));
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  TEST_NIL (p, ref_file, MODE (REF_FILE));
  file = FILE_DEREF (&ref_file);
  TEST_INIT (p, *file, MODE (FILE));
  if (!file->opened) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->draw_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (file->write_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "write");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->channel.get) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "getting");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (!file->read_mood && !file->write_mood) {
    if (IS_NIL (file->string)) {
      if ((file->fd = open_physical_file (p, ref_file, A_READ_ACCESS, 0)) == -1) {
        open_error (p, ref_file, "getting");
      }
    } else {
      file->fd = open_physical_file (p, ref_file, A_READ_ACCESS, 0);
    }
    file->draw_mood = A_FALSE;
    file->read_mood = A_TRUE;
    file->write_mood = A_FALSE;
    file->char_mood = A_TRUE;
  }
  if (!file->char_mood) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Save stack state since formats have frames. */
  save_frame_pointer = file->frame_pointer;
  save_stack_pointer = file->stack_pointer;
  file->frame_pointer = frame_pointer;
  file->stack_pointer = stack_pointer;
/* Process [] SIMPLIN. */
  if (file->format.body != NULL) {
    open_format_frame (file, &(file->format), NOT_EMBEDDED_FORMAT, A_FALSE);
  }
  formats = 0;
  base_address = ADDRESS (&(arr->array));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_POINTER *z = (A68_POINTER *) & (base_address[elem_index]);
    MOID_T *mode = (MOID_T *) (z->value);
    BYTE_T *item = (BYTE_T *) & (base_address[elem_index + SIZE_OF (A68_POINTER)]);
    if (mode == MODE (FORMAT)) {
/* Forget about eventual active formats and set up new one. */
      if (formats > 0) {
        purge_format_read (p, ref_file);
      }
      formats++;
      frame_pointer = file->frame_pointer;
      stack_pointer = file->stack_pointer;
      open_format_frame (file, (A68_FORMAT *) item, NOT_EMBEDDED_FORMAT, A_TRUE);
    } else if (mode == MODE (PROC_REF_FILE_VOID)) {
/* Ignore
	  genie_call_proc_ref_file_void (p, ref_file, *(A68_PROCEDURE *) item);
*/
    } else {
      TEST_NIL (p, *(A68_REF *) item, mode);
      genie_read_standard_format (p, SUB (mode), ADDRESS ((A68_REF *) item), ref_file);
    }
    elem_index += MOID_SIZE (MODE (SIMPLIN));
  }
/* Empty the format to purge insertions. */
  purge_format_read (p, ref_file);
  file->format.body = NULL;
/* Forget about active formats. */
  frame_pointer = file->frame_pointer;
  stack_pointer = file->stack_pointer;
  file->frame_pointer = save_frame_pointer;
  file->stack_pointer = save_stack_pointer;
}
