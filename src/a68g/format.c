//! @file format.c
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

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-frames.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-transput.h"

// Transput library - Formatted transput
// In Algol68G, a value of mode FORMAT looks like a routine text. The value
// comprises a pointer to its environment in the stack, and a pointer where the
// format text is at in the syntax tree.

#define INT_DIGITS "0123456789"
#define BITS_DIGITS "0123456789abcdefABCDEF"
#define INT_DIGITS_BLANK " 0123456789"
#define BITS_DIGITS_BLANK " 0123456789abcdefABCDEF"
#define SIGN_DIGITS " +-"

//! @brief Convert to other radix, binary up to hexadecimal.

BOOL_T convert_radix (NODE_T * p, UNSIGNED_T z, int radix, int width)
{
  reset_transput_buffer (EDIT_BUFFER);
  if (radix < 2 || radix > 16) {
    radix = 16;
  }
  if (width > 0) {
    while (width > 0) {
      int digit = (int) (z % (UNSIGNED_T) radix);
      plusto_transput_buffer (p, digchar (digit), EDIT_BUFFER);
      width--;
      z /= (UNSIGNED_T) radix;
    }
    return z == 0;
  } else if (width == 0) {
    do {
      int digit = (int) (z % (UNSIGNED_T) radix);
      plusto_transput_buffer (p, digchar (digit), EDIT_BUFFER);
      z /= (UNSIGNED_T) radix;
    } while (z > 0);
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Handle format error event.

void format_error (NODE_T * p, A68_REF ref_file, char *diag)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  A68_BOOL z;
  on_event_handler (p, FORMAT_ERROR_MENDED (f), ref_file);
  POP_OBJECT (p, &z, A68_BOOL);
  if (VALUE (&z) == A68_FALSE) {
    diagnostic (A68_RUNTIME_ERROR, p, diag);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief Initialise processing of pictures.

static void initialise_collitems (NODE_T * p)
{

// Every picture has a counter that says whether it has not been used OR the number
// of times it can still be used.

  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PICTURE)) {
      A68_COLLITEM *z = (A68_COLLITEM *) FRAME_LOCAL (A68_FP, OFFSET (TAX (p)));
      STATUS (z) = INIT_MASK;
      COUNT (z) = ITEM_NOT_USED;
    }
// Don't dive into f, g, n frames and collections.
    if (!(IS (p, ENCLOSED_CLAUSE) || IS (p, COLLECTION))) {
      initialise_collitems (SUB (p));
    }
  }
}

//! @brief Initialise processing of format text.

static void open_format_frame (NODE_T * p, A68_REF ref_file, A68_FORMAT * fmt, BOOL_T embedded, BOOL_T init)
{
// Open a new frame for the format text and save for return to embedding one.
  A68_FILE *file = FILE_DEREF (&ref_file);
  NODE_T *dollar;
  A68_FORMAT *save;
// Integrity check.
  if ((STATUS (fmt) & SKIP_FORMAT_MASK) || (BODY (fmt) == NO_NODE)) {
    format_error (p, ref_file, ERROR_FORMAT_UNDEFINED);
  }
// Ok, seems usable.
  dollar = SUB (BODY (fmt));
  OPEN_PROC_FRAME (dollar, ENVIRON (fmt));
  INIT_STATIC_FRAME (dollar);
// Save old format.
  save = (A68_FORMAT *) FRAME_LOCAL (A68_FP, OFFSET (TAX (dollar)));
  *save = (embedded == EMBEDDED_FORMAT ? FORMAT (file) : nil_format);
  FORMAT (file) = *fmt;
// Reset all collitems.
  if (init) {
    initialise_collitems (dollar);
  }
}

//! @brief Handle end-of-format event.

int end_of_format (NODE_T * p, A68_REF ref_file)
{
// Format-items return immediately to the embedding format text. The outermost
//format text calls "on format end".
  A68_FILE *file = FILE_DEREF (&ref_file);
  NODE_T *dollar = SUB (BODY (&FORMAT (file)));
  A68_FORMAT *save = (A68_FORMAT *) FRAME_LOCAL (A68_FP, OFFSET (TAX (dollar)));
  if (IS_NIL_FORMAT (save)) {
// Not embedded, outermost format: execute event routine.
    A68_BOOL z;
    on_event_handler (p, FORMAT_END_MENDED (FILE_DEREF (&ref_file)), ref_file);
    POP_OBJECT (p, &z, A68_BOOL);
    if (VALUE (&z) == A68_FALSE) {
// Restart format.
      A68_FP = FRAME_POINTER (file);
      A68_SP = STACK_POINTER (file);
      open_format_frame (p, ref_file, &FORMAT (file), NOT_EMBEDDED_FORMAT, A68_TRUE);
    }
    return NOT_EMBEDDED_FORMAT;
  } else {
// Embedded format, return to embedding format, cf. RR.
    CLOSE_FRAME;
    FORMAT (file) = *save;
    return EMBEDDED_FORMAT;
  }
}

//! @brief Return integral value of replicator.

int get_replicator_value (NODE_T * p, BOOL_T check)
{
  int z = 0;
  if (IS (p, STATIC_REPLICATOR)) {
    A68_INT u;
    if (genie_string_to_value_internal (p, M_INT, NSYMBOL (p), (BYTE_T *) & u) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, M_INT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    z = VALUE (&u);
  } else if (IS (p, DYNAMIC_REPLICATOR)) {
    A68_INT u;
    EXECUTE_UNIT (NEXT_SUB (p));
    POP_OBJECT (p, &u, A68_INT);
    z = VALUE (&u);
  } else if (IS (p, REPLICATOR)) {
    z = get_replicator_value (SUB (p), check);
  }
// Not conform RR as Andrew Herbert rightfully pointed out.
//  if (check && z < 0) {
//    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FORMAT_INVALID_REPLICATOR);
//    exit_genie (p, A68_RUNTIME_ERROR);
//  }
  if (z < 0) {
    z = 0;
  }
  return z;
}

//! @brief Return first available pattern.

static NODE_T *scan_format_pattern (NODE_T * p, A68_REF ref_file)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, PICTURE_LIST)) {
      NODE_T *prio = scan_format_pattern (SUB (p), ref_file);
      if (prio != NO_NODE) {
        return prio;
      }
    }
    if (IS (p, PICTURE)) {
      NODE_T *picture = SUB (p);
      A68_COLLITEM *collitem = (A68_COLLITEM *) FRAME_LOCAL (A68_FP, OFFSET (TAX (p)));
      if (COUNT (collitem) != 0) {
        if (IS (picture, A68_PATTERN)) {
          COUNT (collitem) = 0; // This pattern is now done
          picture = SUB (picture);
          if (ATTRIBUTE (picture) != FORMAT_PATTERN) {
            return picture;
          } else {
            NODE_T *pat;
            A68_FORMAT z;
            A68_FILE *file = FILE_DEREF (&ref_file);
            EXECUTE_UNIT (NEXT_SUB (picture));
            POP_OBJECT (p, &z, A68_FORMAT);
            open_format_frame (p, ref_file, &z, EMBEDDED_FORMAT, A68_TRUE);
            pat = scan_format_pattern (SUB (BODY (&FORMAT (file))), ref_file);
            if (pat != NO_NODE) {
              return pat;
            } else {
              (void) end_of_format (p, ref_file);
            }
          }
        } else if (IS (picture, INSERTION)) {
          A68_FILE *file = FILE_DEREF (&ref_file);
          if (READ_MOOD (file)) {
            read_insertion (picture, ref_file);
          } else if (WRITE_MOOD (file)) {
            write_insertion (picture, ref_file, INSERTION_NORMAL);
          } else {
            ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
          }
          COUNT (collitem) = 0; // This insertion is now done
        } else if (IS (picture, REPLICATOR) || IS (picture, COLLECTION)) {
          BOOL_T go_on = A68_TRUE;
          NODE_T *a68_select = NO_NODE;
          if (COUNT (collitem) == ITEM_NOT_USED) {
            if (IS (picture, REPLICATOR)) {
              COUNT (collitem) = get_replicator_value (SUB (p), A68_TRUE);
              go_on = (BOOL_T) (COUNT (collitem) > 0);
              FORWARD (picture);
            } else {
              COUNT (collitem) = 1;
            }
            initialise_collitems (NEXT_SUB (picture));
          } else if (IS (picture, REPLICATOR)) {
            FORWARD (picture);
          }
          while (go_on) {
// Get format item from collection. If collection is done, but repitition is not,
// then re-initialise the collection and repeat.
            a68_select = scan_format_pattern (NEXT_SUB (picture), ref_file);
            if (a68_select != NO_NODE) {
              return a68_select;
            } else {
              COUNT (collitem)--;
              go_on = (BOOL_T) (COUNT (collitem) > 0);
              if (go_on) {
                initialise_collitems (NEXT_SUB (picture));
              }
            }
          }
        }
      }
    }
  }
  return NO_NODE;
}

//! @brief Return first available pattern.

NODE_T *get_next_format_pattern (NODE_T * p, A68_REF ref_file, BOOL_T mood)
{
// "mood" can be WANT_PATTERN: pattern needed by caller, so perform end-of-format
// if needed or SKIP_PATTERN: just emptying current pattern/collection/format.
  A68_FILE *file = FILE_DEREF (&ref_file);
  if (BODY (&FORMAT (file)) == NO_NODE) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FORMAT_EXHAUSTED);
    exit_genie (p, A68_RUNTIME_ERROR);
    return NO_NODE;
  } else {
    NODE_T *pat = scan_format_pattern (SUB (BODY (&FORMAT (file))), ref_file);
    if (pat == NO_NODE) {
      if (mood == WANT_PATTERN) {
        int z;
        do {
          z = end_of_format (p, ref_file);
          pat = scan_format_pattern (SUB (BODY (&FORMAT (file))), ref_file);
        } while (z == EMBEDDED_FORMAT && pat == NO_NODE);
        if (pat == NO_NODE) {
          diagnostic (A68_RUNTIME_ERROR, p, ERROR_FORMAT_EXHAUSTED);
          exit_genie (p, A68_RUNTIME_ERROR);
        }
      }
    }
    return pat;
  }
}

//! @brief Diagnostic_node in case mode does not match picture.

void pattern_error (NODE_T * p, MOID_T * mode, int att)
{
  diagnostic (A68_RUNTIME_ERROR, p, ERROR_FORMAT_CANNOT_TRANSPUT, mode, att);
  exit_genie (p, A68_RUNTIME_ERROR);
}

//! @brief Unite value at top of stack to NUMBER.

static void unite_to_number (NODE_T * p, MOID_T * mode, BYTE_T * item)
{
  ADDR_T sp = A68_SP;
  PUSH_UNION (p, mode);
  PUSH (p, item, (int) SIZE (mode));
  A68_SP = sp + SIZE (M_NUMBER);
}

//! @brief Write a group of insertions.

void write_insertion (NODE_T * p, A68_REF ref_file, MOOD_T mood)
{
  for (; p != NO_NODE; FORWARD (p)) {
    write_insertion (SUB (p), ref_file, mood);
    if (IS (p, FORMAT_ITEM_L)) {
      plusab_transput_buffer (p, FORMATTED_BUFFER, NEWLINE_CHAR);
      write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
    } else if (IS (p, FORMAT_ITEM_P)) {
      plusab_transput_buffer (p, FORMATTED_BUFFER, FORMFEED_CHAR);
      write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
    } else if (IS (p, FORMAT_ITEM_X) || IS (p, FORMAT_ITEM_Q)) {
      plusab_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
    } else if (IS (p, FORMAT_ITEM_Y)) {
      PUSH_REF (p, ref_file);
      PUSH_VALUE (p, -1, A68_INT);
      genie_set (p);
    } else if (IS (p, LITERAL)) {
      if (mood & INSERTION_NORMAL) {
        add_string_transput_buffer (p, FORMATTED_BUFFER, NSYMBOL (p));
      } else if (mood & INSERTION_BLANK) {
        int j, k = (int) strlen (NSYMBOL (p));
        for (j = 1; j <= k; j++) {
          plusab_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
        }
      }
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      if (ATTRIBUTE (SUB_NEXT (p)) != FORMAT_ITEM_K) {
        for (j = 1; j <= k; j++) {
          write_insertion (NEXT (p), ref_file, mood);
        }
      } else {
        int pos = get_transput_buffer_index (FORMATTED_BUFFER);
        for (j = 1; j < (k - pos); j++) {
          plusab_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
        }
      }
      return;
    }
  }
}

//! @brief Write string to file following current format.

static void write_string_pattern (NODE_T * p, MOID_T * mode, A68_REF ref_file, char **str)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      write_insertion (SUB (p), ref_file, INSERTION_NORMAL);
    } else if (IS (p, FORMAT_ITEM_A)) {
      if ((*str)[0] != NULL_CHAR) {
        plusab_transput_buffer (p, FORMATTED_BUFFER, (*str)[0]);
        (*str)++;
      } else {
        value_error (p, mode, ref_file);
      }
    } else if (IS (p, FORMAT_ITEM_S)) {
      if ((*str)[0] != NULL_CHAR) {
        (*str)++;
      } else {
        value_error (p, mode, ref_file);
      }
      return;
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        write_string_pattern (NEXT (p), mode, ref_file, str);
      }
      return;
    } else {
      write_string_pattern (SUB (p), mode, ref_file, str);
    }
  }
}

//! @brief Scan c_pattern.

void scan_c_pattern (NODE_T * p, BOOL_T * right_align, BOOL_T * sign, int *width, int *after, int *letter)
{
  if (IS (p, FORMAT_ITEM_ESCAPE)) {
    FORWARD (p);
  }
  if (IS (p, FORMAT_ITEM_MINUS)) {
    *right_align = A68_TRUE;
    FORWARD (p);
  } else {
    *right_align = A68_FALSE;
  }
  if (IS (p, FORMAT_ITEM_PLUS)) {
    *sign = A68_TRUE;
    FORWARD (p);
  } else {
    *sign = A68_FALSE;
  }
  if (IS (p, REPLICATOR)) {
    *width = get_replicator_value (SUB (p), A68_TRUE);
    FORWARD (p);
  }
  if (IS (p, FORMAT_ITEM_POINT)) {
    FORWARD (p);
  }
  if (IS (p, REPLICATOR)) {
    *after = get_replicator_value (SUB (p), A68_TRUE);
    FORWARD (p);
  }
  *letter = ATTRIBUTE (p);
}

//! @brief Write appropriate insertion from a choice pattern.

static void write_choice_pattern (NODE_T * p, A68_REF ref_file, int *count)
{
  for (; p != NO_NODE; FORWARD (p)) {
    write_choice_pattern (SUB (p), ref_file, count);
    if (IS (p, PICTURE)) {
      (*count)--;
      if (*count == 0) {
        write_insertion (SUB (p), ref_file, INSERTION_NORMAL);
      }
    }
  }
}

//! @brief Write appropriate insertion from a boolean pattern.

static void write_boolean_pattern (NODE_T * p, A68_REF ref_file, BOOL_T z)
{
  int k = (z ? 1 : 2);
  write_choice_pattern (p, ref_file, &k);
}

//! @brief Write value according to a general pattern.

static void write_number_generic (NODE_T * p, MOID_T * mode, BYTE_T * item, int mod)
{
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int size;
// Push arguments.
  unite_to_number (p, mode, item);
  EXECUTE_UNIT (NEXT_SUB (p));
  POP_REF (p, &row);
  GET_DESCRIPTOR (arr, tup, &row);
  size = ROW_SIZE (tup);
  if (size > 0) {
    int i;
    BYTE_T *base_address = DEREF (BYTE_T, &ARRAY (arr));
    for (i = LWB (tup); i <= UPB (tup); i++) {
      int addr = INDEX_1_DIM (arr, tup, i);
      int arg = VALUE ((A68_INT *) & (base_address[addr]));
      PUSH_VALUE (p, arg, A68_INT);
    }
  }
// Make a string.
  if (mod == FORMAT_ITEM_G) {
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
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_FORMAT_INTS_REQUIRED, M_INT);
        exit_genie (p, A68_RUNTIME_ERROR);
        break;
      }
    }
  } else if (mod == FORMAT_ITEM_H) {
    int def_expo = 0, def_mult;
    A68_INT a_width, a_after, a_expo, a_mult;
    STATUS (&a_width) = INIT_MASK;
    VALUE (&a_width) = 0;
    STATUS (&a_after) = INIT_MASK;
    VALUE (&a_after) = 0;
    STATUS (&a_expo) = INIT_MASK;
    VALUE (&a_expo) = 0;
    STATUS (&a_mult) = INIT_MASK;
    VALUE (&a_mult) = 0;
// Set default values 
    if (mode == M_REAL || mode == M_INT) {
      def_expo = EXP_WIDTH + 1;
    } else if (mode == M_LONG_REAL || mode == M_LONG_INT) {
      def_expo = LONG_EXP_WIDTH + 1;
    } else if (mode == M_LONG_LONG_REAL || mode == M_LONG_LONG_INT) {
      def_expo = LONG_LONG_EXP_WIDTH + 1;
    }
    def_mult = 3;
// Pop user values 
    switch (size) {
    case 1:
      {
        POP_OBJECT (p, &a_after, A68_INT);
        VALUE (&a_width) = VALUE (&a_after) + def_expo + 4;
        VALUE (&a_expo) = def_expo;
        VALUE (&a_mult) = def_mult;
        break;
      }
    case 2:
      {
        POP_OBJECT (p, &a_mult, A68_INT);
        POP_OBJECT (p, &a_after, A68_INT);
        VALUE (&a_width) = VALUE (&a_after) + def_expo + 4;
        VALUE (&a_expo) = def_expo;
        break;
      }
    case 3:
      {
        POP_OBJECT (p, &a_mult, A68_INT);
        POP_OBJECT (p, &a_after, A68_INT);
        POP_OBJECT (p, &a_width, A68_INT);
        VALUE (&a_expo) = def_expo;
        break;
      }
    case 4:
      {
        POP_OBJECT (p, &a_mult, A68_INT);
        POP_OBJECT (p, &a_expo, A68_INT);
        POP_OBJECT (p, &a_after, A68_INT);
        POP_OBJECT (p, &a_width, A68_INT);
        break;
      }
    default:
      {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_FORMAT_INTS_REQUIRED, M_INT);
        exit_genie (p, A68_RUNTIME_ERROR);
        break;
      }
    }
    PUSH_VALUE (p, VALUE (&a_width), A68_INT);
    PUSH_VALUE (p, VALUE (&a_after), A68_INT);
    PUSH_VALUE (p, VALUE (&a_expo), A68_INT);
    PUSH_VALUE (p, VALUE (&a_mult), A68_INT);
    genie_real (p);
  }
  add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
}

//! @brief Write %[-][+][w][.][d]s/d/i/f/e/b/o/x formats.

static void write_c_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  BOOL_T right_align, sign, invalid;
  int width = 0, after = 0, letter;
  ADDR_T pop_sp = A68_SP;
  char *str = NO_TEXT;
  if (IS (p, CHAR_C_PATTERN)) {
    A68_CHAR *z = (A68_CHAR *) item;
    char q[2];
    q[0] = (char) VALUE (z);
    q[1] = NULL_CHAR;
    str = (char *) &q;
    width = (int) strlen (str);
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
  } else if (IS (p, STRING_C_PATTERN)) {
    str = (char *) item;
    width = (int) strlen (str);
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
  } else if (IS (p, INTEGRAL_C_PATTERN)) {
    width = 0;
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
    unite_to_number (p, mode, item);
    PUSH_VALUE (p, (sign ? width : -width), A68_INT);
    str = whole (p);
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    int att = ATTRIBUTE (p), expval = 0, expo = 0;
    if (att == FLOAT_C_PATTERN || att == GENERAL_C_PATTERN) {
      int digits = 0;
      if (mode == M_REAL || mode == M_INT) {
        width = REAL_WIDTH + EXP_WIDTH + 4;
        after = REAL_WIDTH - 1;
        expo = EXP_WIDTH + 1;
      } else if (mode == M_LONG_REAL || mode == M_LONG_INT) {
        width = LONG_REAL_WIDTH + LONG_EXP_WIDTH + 4;
        after = LONG_REAL_WIDTH - 1;
        expo = LONG_EXP_WIDTH + 1;
      } else if (mode == M_LONG_LONG_REAL || mode == M_LONG_LONG_INT) {
        width = LONG_LONG_REAL_WIDTH + LONG_LONG_EXP_WIDTH + 4;
        after = LONG_LONG_REAL_WIDTH - 1;
        expo = LONG_LONG_EXP_WIDTH + 1;
      }
      scan_c_pattern (SUB (p), &right_align, &sign, &digits, &after, &letter);
      if (digits == 0 && after > 0) {
        width = after + expo + 4;
      } else if (digits > 0) {
        width = digits;
      }
      unite_to_number (p, mode, item);
      PUSH_VALUE (p, (sign ? width : -width), A68_INT);
      PUSH_VALUE (p, after, A68_INT);
      PUSH_VALUE (p, expo, A68_INT);
      PUSH_VALUE (p, 1, A68_INT);
      str = real (p);
      A68_SP = pop_sp;
    }
    if (att == GENERAL_C_PATTERN) {
      char *expch = strchr (str, EXPONENT_CHAR);
      if (expch != NO_TEXT) {
        expval = (int) strtol (&(expch[1]), NO_VAR, 10);
      }
    }
    if ((att == FIXED_C_PATTERN) || (att == GENERAL_C_PATTERN && (expval > -4 && expval <= after))) {
      int digits = 0;
      if (mode == M_REAL || mode == M_INT) {
        width = REAL_WIDTH + 2;
        after = REAL_WIDTH - 1;
      } else if (mode == M_LONG_REAL || mode == M_LONG_INT) {
        width = LONG_REAL_WIDTH + 2;
        after = LONG_REAL_WIDTH - 1;
      } else if (mode == M_LONG_LONG_REAL || mode == M_LONG_LONG_INT) {
        width = LONG_LONG_REAL_WIDTH + 2;
        after = LONG_LONG_REAL_WIDTH - 1;
      }
      scan_c_pattern (SUB (p), &right_align, &sign, &digits, &after, &letter);
      if (digits == 0 && after > 0) {
        width = after + 2;
      } else if (digits > 0) {
        width = digits;
      }
      unite_to_number (p, mode, item);
      PUSH_VALUE (p, (sign ? width : -width), A68_INT);
      PUSH_VALUE (p, after, A68_INT);
      str = fixed (p);
      A68_SP = pop_sp;
    }
  } else if (IS (p, BITS_C_PATTERN)) {
    int radix = 10, nibble = 1;
    width = 0;
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
    if (letter == FORMAT_ITEM_B) {
      radix = 2;
      nibble = 1;
    } else if (letter == FORMAT_ITEM_O) {
      radix = 8;
      nibble = 3;
    } else if (letter == FORMAT_ITEM_X) {
      radix = 16;
      nibble = 4;
    }
    if (width == 0) {
      if (mode == M_BITS) {
        width = (int) ceil ((REAL_T) BITS_WIDTH / (REAL_T) nibble);
      } else if (mode == M_LONG_BITS || mode == M_LONG_LONG_BITS) {
#if (A68_LEVEL <= 2)
        width = (int) ceil ((REAL_T) get_mp_bits_width (mode) / (REAL_T) nibble);
#else
        width = (int) ceil ((REAL_T) LONG_BITS_WIDTH / (REAL_T) nibble);
#endif
      }
    }
    if (mode == M_BITS) {
      A68_BITS *z = (A68_BITS *) item;
      reset_transput_buffer (EDIT_BUFFER);
      if (!convert_radix (p, VALUE (z), radix, width)) {
        errno = EDOM;
        value_error (p, mode, ref_file);
      }
      str = get_transput_buffer (EDIT_BUFFER);
    } else if (mode == M_LONG_BITS) {
#if (A68_LEVEL >= 3)
      A68_LONG_BITS *z = (A68_LONG_BITS *) item;
      reset_transput_buffer (EDIT_BUFFER);
      if (!convert_radix_double (p, VALUE (z), radix, width)) {
        errno = EDOM;
        value_error (p, mode, ref_file);
      }
      str = get_transput_buffer (EDIT_BUFFER);
#else
      int digits = DIGITS (mode);
      MP_T *u = (MP_T *) item;
      MP_T *v = nil_mp (p, digits);
      MP_T *w = nil_mp (p, digits);
      reset_transput_buffer (EDIT_BUFFER);
      if (!convert_radix_mp (p, u, radix, width, mode, v, w)) {
        errno = EDOM;
        value_error (p, mode, ref_file);
      }
      str = get_transput_buffer (EDIT_BUFFER);
#endif
    } else if (mode == M_LONG_LONG_BITS) {
#if (A68_LEVEL <= 2)
      int digits = DIGITS (mode);
      MP_T *u = (MP_T *) item;
      MP_T *v = nil_mp (p, digits);
      MP_T *w = nil_mp (p, digits);
      reset_transput_buffer (EDIT_BUFFER);
      if (!convert_radix_mp (p, u, radix, width, mode, v, w)) {
        errno = EDOM;
        value_error (p, mode, ref_file);
      }
      str = get_transput_buffer (EDIT_BUFFER);
#endif
    }
  }
// Did the conversion succeed?.
  if (IS (p, CHAR_C_PATTERN) || IS (p, STRING_C_PATTERN)) {
    invalid = A68_FALSE;
  } else {
    invalid = (strchr (str, ERROR_CHAR) != NO_TEXT);
  }
  if (invalid) {
    value_error (p, mode, ref_file);
    (void) error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
  } else {
// Align and output.
    if (width == 0) {
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else {
      if (right_align == A68_TRUE) {
        int blanks = width - (int) strlen (str);
        if (blanks >= 0) {
          while (blanks--) {
            plusab_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
          }
          add_string_transput_buffer (p, FORMATTED_BUFFER, str);
        } else {
          value_error (p, mode, ref_file);
          (void) error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
        }
      } else {
        int blanks;
        while (str[0] == BLANK_CHAR) {
          str++;
        }
        blanks = width - (int) strlen (str);
        if (blanks >= 0) {
          add_string_transput_buffer (p, FORMATTED_BUFFER, str);
          while (blanks--) {
            plusab_transput_buffer (p, FORMATTED_BUFFER, BLANK_CHAR);
          }
        } else {
          value_error (p, mode, ref_file);
          (void) error_chars (get_transput_buffer (FORMATTED_BUFFER), width);
        }
      }
    }
  }
}

//! @brief Read one char from file.

static char read_single_char (NODE_T * p, A68_REF ref_file)
{
  A68_FILE *file = FILE_DEREF (&ref_file);
  int ch = char_scanner (file);
  if (ch == EOF_CHAR) {
    end_of_file_error (p, ref_file);
  }
  return (char) ch;
}

//! @brief Scan n chars from file to input buffer.

static void scan_n_chars (NODE_T * p, int n, MOID_T * m, A68_REF ref_file)
{
  int k;
  (void) m;
  for (k = 0; k < n; k++) {
    int ch = read_single_char (p, ref_file);
    plusab_transput_buffer (p, INPUT_BUFFER, (char) ch);
  }
}

//! @brief Read %[-][+][w][.][d]s/d/i/f/e/b/o/x formats.

static void read_c_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  BOOL_T right_align, sign;
  int width, after, letter;
  ADDR_T pop_sp = A68_SP;
  reset_transput_buffer (INPUT_BUFFER);
  if (IS (p, CHAR_C_PATTERN)) {
    width = 0;
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
    if (width == 0) {
      genie_read_standard (p, mode, item, ref_file);
    } else {
      scan_n_chars (p, width, mode, ref_file);
      if (width > 1 && right_align == A68_FALSE) {
        for (; width > 1; width--) {
          (void) pop_char_transput_buffer (INPUT_BUFFER);
        }
      }
      genie_string_to_value (p, mode, item, ref_file);
    }
  } else if (IS (p, STRING_C_PATTERN)) {
    width = 0;
    scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
    if (width == 0) {
      genie_read_standard (p, mode, item, ref_file);
    } else {
      scan_n_chars (p, width, mode, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    }
  } else if (IS (p, INTEGRAL_C_PATTERN)) {
    if (mode != M_INT && mode != M_LONG_INT && mode != M_LONG_LONG_INT) {
      pattern_error (p, mode, ATTRIBUTE (p));
    } else {
      width = 0;
      scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
      if (width == 0) {
        genie_read_standard (p, mode, item, ref_file);
      } else {
        scan_n_chars (p, (sign != 0) ? width + 1 : width, mode, ref_file);
        genie_string_to_value (p, mode, item, ref_file);
      }
    }
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    if (mode != M_REAL && mode != M_LONG_REAL && mode != M_LONG_LONG_REAL) {
      pattern_error (p, mode, ATTRIBUTE (p));
    } else {
      width = 0;
      scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
      if (width == 0) {
        genie_read_standard (p, mode, item, ref_file);
      } else {
        scan_n_chars (p, (sign != 0) ? width + 1 : width, mode, ref_file);
        genie_string_to_value (p, mode, item, ref_file);
      }
    }
  } else if (IS (p, BITS_C_PATTERN)) {
    if (mode != M_BITS && mode != M_LONG_BITS && mode != M_LONG_LONG_BITS) {
      pattern_error (p, mode, ATTRIBUTE (p));
    } else {
      int radix = 10;
      char *str;
      width = 0;
      scan_c_pattern (SUB (p), &right_align, &sign, &width, &after, &letter);
      if (letter == FORMAT_ITEM_B) {
        radix = 2;
      } else if (letter == FORMAT_ITEM_O) {
        radix = 8;
      } else if (letter == FORMAT_ITEM_X) {
        radix = 16;
      }
      str = get_transput_buffer (INPUT_BUFFER);
      if (width == 0) {
        A68_FILE *file = FILE_DEREF (&ref_file);
        int ch;
        ASSERT (snprintf (str, (size_t) TRANSPUT_BUFFER_SIZE, "%dr", radix) >= 0);
        set_transput_buffer_index (INPUT_BUFFER, (int) strlen (str));
        ch = char_scanner (file);
        while (ch != EOF_CHAR && (IS_SPACE (ch) || IS_NL_FF (ch))) {
          if (IS_NL_FF (ch)) {
            skip_nl_ff (p, &ch, ref_file);
          } else {
            ch = char_scanner (file);
          }
        }
        while (ch != EOF_CHAR && IS_XDIGIT (ch)) {
          plusab_transput_buffer (p, INPUT_BUFFER, (char) ch);
          ch = char_scanner (file);
        }
        unchar_scanner (p, file, (char) ch);
      } else {
        ASSERT (snprintf (str, (size_t) TRANSPUT_BUFFER_SIZE, "%dr", radix) >= 0);
        set_transput_buffer_index (INPUT_BUFFER, (int) strlen (str));
        scan_n_chars (p, width, mode, ref_file);
      }
      genie_string_to_value (p, mode, item, ref_file);
    }
  }
  A68_SP = pop_sp;
}

// INTEGRAL, REAL, COMPLEX and BITS patterns.

//! @brief Count Z and D frames in a mould.

static void count_zd_frames (NODE_T * p, int *z)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, FORMAT_ITEM_D) || IS (p, FORMAT_ITEM_Z)) {
      (*z)++;
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        count_zd_frames (NEXT (p), z);
      }
      return;
    } else {
      count_zd_frames (SUB (p), z);
    }
  }
}

//! @brief Get sign from sign mould.

static NODE_T *get_sign (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    NODE_T *q = get_sign (SUB (p));
    if (q != NO_NODE) {
      return q;
    } else if (IS (p, FORMAT_ITEM_PLUS) || IS (p, FORMAT_ITEM_MINUS)) {
      return p;
    }
  }
  return NO_NODE;
}

//! @brief Shift sign through Z frames until non-zero digit or D frame.

static void shift_sign (NODE_T * p, char **q)
{
  for (; p != NO_NODE && (*q) != NO_TEXT; FORWARD (p)) {
    shift_sign (SUB (p), q);
    if (IS (p, FORMAT_ITEM_Z)) {
      if (((*q)[0] == '+' || (*q)[0] == '-') && (*q)[1] == '0') {
        char ch = (*q)[0];
        (*q)[0] = (*q)[1];
        (*q)[1] = ch;
        (*q)++;
      }
    } else if (IS (p, FORMAT_ITEM_D)) {
      (*q) = NO_TEXT;
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        shift_sign (NEXT (p), q);
      }
      return;
    }
  }
}

//! @brief Pad trailing blanks to integral until desired width.

static void put_zeroes_to_integral (NODE_T * p, int n)
{
  for (; n > 0; n--) {
    plusab_transput_buffer (p, EDIT_BUFFER, '0');
  }
}

//! @brief Pad a sign to integral representation.

static void put_sign_to_integral (NODE_T * p, int sign)
{
  NODE_T *sign_node = get_sign (SUB (p));
  if (IS (sign_node, FORMAT_ITEM_PLUS)) {
    plusab_transput_buffer (p, EDIT_BUFFER, (char) (sign >= 0 ? '+' : '-'));
  } else {
    plusab_transput_buffer (p, EDIT_BUFFER, (char) (sign >= 0 ? BLANK_CHAR : '-'));
  }
}

//! @brief Write point, exponent or plus-i-times symbol.

static void write_pie_frame (NODE_T * p, A68_REF ref_file, int att, int sym)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      write_insertion (p, ref_file, INSERTION_NORMAL);
    } else if (IS (p, att)) {
      write_pie_frame (SUB (p), ref_file, att, sym);
      return;
    } else if (IS (p, sym)) {
      add_string_transput_buffer (p, FORMATTED_BUFFER, NSYMBOL (p));
    } else if (IS (p, FORMAT_ITEM_S)) {
      return;
    }
  }
}

//! @brief Write sign when appropriate.

static void write_mould_put_sign (NODE_T * p, char **q)
{
  if ((*q)[0] == '+' || (*q)[0] == '-' || (*q)[0] == BLANK_CHAR) {
    plusab_transput_buffer (p, FORMATTED_BUFFER, (*q)[0]);
    (*q)++;
  }
}

//! @brief Write character according to a mould.

static void add_char_mould (NODE_T * p, char ch, char **q)
{
  if (ch != NULL_CHAR) {
    plusab_transput_buffer (p, FORMATTED_BUFFER, ch);
    (*q)++;
  }
}

//! @brief Write string according to a mould.

static void write_mould (NODE_T * p, A68_REF ref_file, int type, char **q, MOOD_T * mood)
{
  for (; p != NO_NODE; FORWARD (p)) {
// Insertions are inserted straight away. Note that we can suppress them using "mood", which is not standard A68.
    if (IS (p, INSERTION)) {
      write_insertion (SUB (p), ref_file, *mood);
    } else {
      write_mould (SUB (p), ref_file, type, q, mood);
// Z frames print blanks until first non-zero digits comes.
      if (IS (p, FORMAT_ITEM_Z)) {
        write_mould_put_sign (p, q);
        if ((*q)[0] == '0') {
          if (*mood & DIGIT_BLANK) {
            add_char_mould (p, BLANK_CHAR, q);
            *mood = (*mood & ~INSERTION_NORMAL) | INSERTION_BLANK;
          } else if (*mood & DIGIT_NORMAL) {
            add_char_mould (p, '0', q);
            *mood = (MOOD_T) (DIGIT_NORMAL | INSERTION_NORMAL);
          }
        } else {
          add_char_mould (p, (*q)[0], q);
          *mood = (MOOD_T) (DIGIT_NORMAL | INSERTION_NORMAL);
        }
      }
// D frames print a digit.
      else if (IS (p, FORMAT_ITEM_D)) {
        write_mould_put_sign (p, q);
        add_char_mould (p, (*q)[0], q);
        *mood = (MOOD_T) (DIGIT_NORMAL | INSERTION_NORMAL);
      }
// Suppressible frames.
      else if (IS (p, FORMAT_ITEM_S)) {
// Suppressible frames are ignored in a sign-mould.
        if (type == SIGN_MOULD) {
          write_mould (NEXT (p), ref_file, type, q, mood);
        } else if (type == INTEGRAL_MOULD) {
          if ((*q)[0] != NULL_CHAR) {
            (*q)++;
          }
        }
        return;
      }
// Replicator.
      else if (IS (p, REPLICATOR)) {
        int j, k = get_replicator_value (SUB (p), A68_TRUE);
        for (j = 1; j <= k; j++) {
          write_mould (NEXT (p), ref_file, type, q, mood);
        }
        return;
      }
    }
  }
}

//! @brief Write INT value using int pattern.

static void write_integral_pattern (NODE_T * p, MOID_T * mode, MOID_T * root, BYTE_T * item, A68_REF ref_file)
{
  errno = 0;
  if (!(mode == M_INT || mode == M_LONG_INT || mode == M_LONG_LONG_INT)) {
    pattern_error (p, root, ATTRIBUTE (p));
  } else {
    ADDR_T pop_sp = A68_SP;
    char *str = "*";
    int width = 0, sign = 0;
    MOOD_T mood;
// Dive into the pattern if needed.
    if (IS (p, INTEGRAL_PATTERN)) {
      p = SUB (p);
    }
// Find width.
    count_zd_frames (p, &width);
// Make string.
    reset_transput_buffer (EDIT_BUFFER);
    if (mode == M_INT) {
      A68_INT *z = (A68_INT *) item;
      sign = SIGN (VALUE (z));
      str = sub_whole (p, ABS (VALUE (z)), width);
    } else if (mode == M_LONG_INT) {
#if (A68_LEVEL >= 3)
      A68_LONG_INT *z = (A68_LONG_INT *) item;
      QUAD_WORD_T w = VALUE (z);
      sign = sign_int_16 (w);
      str = long_sub_whole_double (p, abs_int_16 (w), width);
#else
      MP_T *z = (MP_T *) item;
      sign = MP_SIGN (z);
      MP_DIGIT (z, 1) = ABS (MP_DIGIT (z, 1));
      str = long_sub_whole (p, z, DIGITS (mode), width);
#endif
    } else if (mode == M_LONG_LONG_INT) {
      MP_T *z = (MP_T *) item;
      sign = MP_SIGN (z);
      MP_DIGIT (z, 1) = ABS (MP_DIGIT (z, 1));
      str = long_sub_whole (p, z, DIGITS (mode), width);
    }
// Edit string and output.
    if (strchr (str, ERROR_CHAR) != NO_TEXT) {
      value_error (p, root, ref_file);
    }
    if (IS (p, SIGN_MOULD)) {
      put_sign_to_integral (p, sign);
    } else if (sign < 0) {
      value_sign_error (p, root, ref_file);
    }
    put_zeroes_to_integral (p, width - (int) strlen (str));
    add_string_transput_buffer (p, EDIT_BUFFER, str);
    str = get_transput_buffer (EDIT_BUFFER);
    mood = (MOOD_T) (DIGIT_BLANK | INSERTION_NORMAL);
    if (IS (p, SIGN_MOULD)) {
      if (str[0] == '+' || str[0] == '-') {
        shift_sign (SUB (p), &str);
      }
      str = get_transput_buffer (EDIT_BUFFER);
      write_mould (SUB (p), ref_file, SIGN_MOULD, &str, &mood);
      FORWARD (p);
    }
    if (IS (p, INTEGRAL_MOULD)) {       // This *should* be the case
      write_mould (SUB (p), ref_file, INTEGRAL_MOULD, &str, &mood);
    }
    A68_SP = pop_sp;
  }
}

//! @brief Write REAL value using real pattern.

static void write_real_pattern (NODE_T * p, MOID_T * mode, MOID_T * root, BYTE_T * item, A68_REF ref_file)
{
  errno = 0;
  if (!(mode == M_REAL || mode == M_LONG_REAL || mode == M_LONG_LONG_REAL || mode == M_INT || mode == M_LONG_INT || mode == M_LONG_LONG_INT)) {
    pattern_error (p, root, ATTRIBUTE (p));
  } else {
    ADDR_T pop_sp = A68_SP;
    int stag_digits = 0, frac_digits = 0, expo_digits = 0;
    int mant_length, sign = 0, exp_value;
    NODE_T *q, *sign_mould = NO_NODE, *stag_mould = NO_NODE, *point_frame = NO_NODE, *frac_mould = NO_NODE, *e_frame = NO_NODE, *expo_mould = NO_NODE;
    char *str = NO_TEXT, *stag_str = NO_TEXT, *frac_str = NO_TEXT;
    MOOD_T mood;
// Dive into pattern.
    q = ((IS (p, REAL_PATTERN)) ? SUB (p) : p);
// Dissect pattern and establish widths.
    if (q != NO_NODE && IS (q, SIGN_MOULD)) {
      sign_mould = q;
      count_zd_frames (SUB (sign_mould), &stag_digits);
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
      stag_mould = q;
      count_zd_frames (SUB (stag_mould), &stag_digits);
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, FORMAT_POINT_FRAME)) {
      point_frame = q;
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
      frac_mould = q;
      count_zd_frames (SUB (frac_mould), &frac_digits);
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, EXPONENT_FRAME)) {
      e_frame = SUB (q);
      expo_mould = NEXT_SUB (q);
      q = expo_mould;
      if (IS (q, SIGN_MOULD)) {
        count_zd_frames (SUB (q), &expo_digits);
        FORWARD (q);
      }
      if (IS (q, INTEGRAL_MOULD)) {
        count_zd_frames (SUB (q), &expo_digits);
      }
    }
// Make string representation.
    if (point_frame == NO_NODE) {
      mant_length = stag_digits;
    } else {
      mant_length = 1 + stag_digits + frac_digits;
    }
    if (mode == M_REAL || mode == M_INT) {
      REAL_T x;
      if (mode == M_REAL) {
        x = VALUE ((A68_REAL *) item);
      } else {
        x = (REAL_T) VALUE ((A68_INT *) item);
      }
      CHECK_REAL (p, x);
      exp_value = 0;
      sign = SIGN (x);
      if (sign_mould != NO_NODE) {
        put_sign_to_integral (sign_mould, sign);
      }
      x = ABS (x);
      if (expo_mould != NO_NODE) {
        standardise (&x, stag_digits, frac_digits, &exp_value);
      }
      str = sub_fixed (p, x, mant_length, frac_digits);
    } else if (mode == M_LONG_REAL || mode == M_LONG_INT) {
#if (A68_LEVEL >= 3)
      QUAD_WORD_T x = VALUE ((A68_DOUBLE *) item);
      if (mode == M_LONG_INT) {
        x = int_16_to_real_16 (p, x);
      }
      CHECK_DOUBLE_REAL (p, x.f);
      exp_value = 0;
      sign = sign_real_16 (x);
      if (sign_mould != NO_NODE) {
        put_sign_to_integral (sign_mould, sign);
      }
      x.f = fabsq (x.f);
      if (expo_mould != NO_NODE) {
        standardise_double (&(x.f), stag_digits, frac_digits, &exp_value);
      }
      str = sub_fixed_double (p, x.f, mant_length, frac_digits, LONG_REAL_WIDTH);
#else
      ADDR_T pop_sp2 = A68_SP;
      int digits = DIGITS (mode);
      MP_T *x = nil_mp (p, digits);
      (void) move_mp (x, (MP_T *) item, digits);
      exp_value = 0;
      sign = SIGN (x[2]);
      if (sign_mould != NO_NODE) {
        put_sign_to_integral (sign_mould, sign);
      }
      x[2] = ABS (x[2]);
      if (expo_mould != NO_NODE) {
        long_standardise (p, x, DIGITS (mode), stag_digits, frac_digits, &exp_value);
      }
      str = long_sub_fixed (p, x, DIGITS (mode), mant_length, frac_digits);
      A68_SP = pop_sp2;
#endif
    } else if (mode == M_LONG_LONG_REAL || mode == M_LONG_LONG_INT) {
      ADDR_T pop_sp2 = A68_SP;
      int digits = DIGITS (mode);
      MP_T *x = nil_mp (p, digits);
      (void) move_mp (x, (MP_T *) item, digits);
      exp_value = 0;
      sign = SIGN (x[2]);
      if (sign_mould != NO_NODE) {
        put_sign_to_integral (sign_mould, sign);
      }
      x[2] = ABS (x[2]);
      if (expo_mould != NO_NODE) {
        long_standardise (p, x, DIGITS (mode), stag_digits, frac_digits, &exp_value);
      }
      str = long_sub_fixed (p, x, DIGITS (mode), mant_length, frac_digits);
      A68_SP = pop_sp2;
    }
// Edit and output the string.
    if (strchr (str, ERROR_CHAR) != NO_TEXT) {
      value_error (p, root, ref_file);
    }
    reset_transput_buffer (STRING_BUFFER);
    add_string_transput_buffer (p, STRING_BUFFER, str);
    stag_str = get_transput_buffer (STRING_BUFFER);
    if (strchr (stag_str, ERROR_CHAR) != NO_TEXT) {
      value_error (p, root, ref_file);
    }
    str = strchr (stag_str, POINT_CHAR);
    if (str != NO_TEXT) {
      frac_str = &str[1];
      str[0] = NULL_CHAR;
    } else {
      frac_str = NO_TEXT;
    }
// Stagnant part.
    reset_transput_buffer (EDIT_BUFFER);
    if (sign_mould != NO_NODE) {
      put_sign_to_integral (sign_mould, sign);
    } else if (sign < 0) {
      value_sign_error (sign_mould, root, ref_file);
    }
    put_zeroes_to_integral (p, stag_digits - (int) strlen (stag_str));
    add_string_transput_buffer (p, EDIT_BUFFER, stag_str);
    stag_str = get_transput_buffer (EDIT_BUFFER);
    mood = (MOOD_T) (DIGIT_BLANK | INSERTION_NORMAL);
    if (sign_mould != NO_NODE) {
      if (stag_str[0] == '+' || stag_str[0] == '-') {
        shift_sign (SUB (p), &stag_str);
      }
      stag_str = get_transput_buffer (EDIT_BUFFER);
      write_mould (SUB (sign_mould), ref_file, SIGN_MOULD, &stag_str, &mood);
    }
    if (stag_mould != NO_NODE) {
      write_mould (SUB (stag_mould), ref_file, INTEGRAL_MOULD, &stag_str, &mood);
    }
// Point frame.
    if (point_frame != NO_NODE) {
      write_pie_frame (point_frame, ref_file, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT);
    }
// Fraction.
    if (frac_mould != NO_NODE) {
      reset_transput_buffer (EDIT_BUFFER);
      add_string_transput_buffer (p, EDIT_BUFFER, frac_str);
      frac_str = get_transput_buffer (EDIT_BUFFER);
      mood = (MOOD_T) (DIGIT_NORMAL | INSERTION_NORMAL);
      write_mould (SUB (frac_mould), ref_file, INTEGRAL_MOULD, &frac_str, &mood);
    }
// Exponent.
    if (expo_mould != NO_NODE) {
      A68_INT z;
      STATUS (&z) = INIT_MASK;
      VALUE (&z) = exp_value;
      if (e_frame != NO_NODE) {
        write_pie_frame (e_frame, ref_file, FORMAT_E_FRAME, FORMAT_ITEM_E);
      }
      write_integral_pattern (expo_mould, M_INT, root, (BYTE_T *) & z, ref_file);
    }
    A68_SP = pop_sp;
  }
}

//! @brief Write COMPLEX value using complex pattern.

static void write_complex_pattern (NODE_T * p, MOID_T * comp, MOID_T * root, BYTE_T * re, BYTE_T * im, A68_REF ref_file)
{
  NODE_T *reel, *plus_i_times, *imag;
  errno = 0;
// Dissect pattern.
  reel = SUB (p);
  plus_i_times = NEXT (reel);
  imag = NEXT (plus_i_times);
// Write pattern.
  write_real_pattern (reel, comp, root, re, ref_file);
  write_pie_frame (plus_i_times, ref_file, FORMAT_I_FRAME, FORMAT_ITEM_I);
  write_real_pattern (imag, comp, root, im, ref_file);
}

//! @brief Write BITS value using bits pattern.

static void write_bits_pattern (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  MOOD_T mood;
  int width = 0, radix;
  char *str;
  ADDR_T pop_sp = A68_SP;
  if (mode == M_BITS) {
    A68_BITS *z = (A68_BITS *) item;
// Establish width and radix.
    count_zd_frames (SUB (p), &width);
    radix = get_replicator_value (SUB_SUB (p), A68_TRUE);
    if (radix < 2 || radix > 16) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
// Generate string of correct width.
    reset_transput_buffer (EDIT_BUFFER);
    if (!convert_radix (p, VALUE (z), radix, width)) {
      errno = EDOM;
      value_error (p, mode, ref_file);
    }
  } else if (mode == M_LONG_BITS) {
#if (A68_LEVEL >= 3)
    A68_LONG_BITS *z = (A68_LONG_BITS *) item;
// Establish width and radix.
    count_zd_frames (SUB (p), &width);
    radix = get_replicator_value (SUB_SUB (p), A68_TRUE);
    if (radix < 2 || radix > 16) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
// Generate string of correct width.
    reset_transput_buffer (EDIT_BUFFER);
    if (!convert_radix_double (p, VALUE (z), radix, width)) {
      errno = EDOM;
      value_error (p, mode, ref_file);
    }
#else
    int digits = DIGITS (mode);
    MP_T *u = (MP_T *) item;
    MP_T *v = nil_mp (p, digits);
    MP_T *w = nil_mp (p, digits);
// Establish width and radix.
    count_zd_frames (SUB (p), &width);
    radix = get_replicator_value (SUB_SUB (p), A68_TRUE);
    if (radix < 2 || radix > 16) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
// Generate string of correct width.
    reset_transput_buffer (EDIT_BUFFER);
    if (!convert_radix_mp (p, u, radix, width, mode, v, w)) {
      errno = EDOM;
      value_error (p, mode, ref_file);
    }
#endif
  } else if (mode == M_LONG_LONG_BITS) {
#if (A68_LEVEL <= 2)
    int digits = DIGITS (mode);
    MP_T *u = (MP_T *) item;
    MP_T *v = nil_mp (p, digits);
    MP_T *w = nil_mp (p, digits);
// Establish width and radix.
    count_zd_frames (SUB (p), &width);
    radix = get_replicator_value (SUB_SUB (p), A68_TRUE);
    if (radix < 2 || radix > 16) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
// Generate string of correct width.
    reset_transput_buffer (EDIT_BUFFER);
    if (!convert_radix_mp (p, u, radix, width, mode, v, w)) {
      errno = EDOM;
      value_error (p, mode, ref_file);
    }
#endif
  }
// Output the edited string.
  mood = (MOOD_T) (DIGIT_BLANK | INSERTION_NORMAL);
  str = get_transput_buffer (EDIT_BUFFER);
  write_mould (NEXT_SUB (p), ref_file, INTEGRAL_MOULD, &str, &mood);
  A68_SP = pop_sp;
}

//! @brief Write value to file.

static void genie_write_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) == NO_NODE) {
    genie_value_to_string (p, M_REAL, item, ATTRIBUTE (SUB (p)));
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
    write_number_generic (p, M_REAL, item, ATTRIBUTE (SUB (p)));
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    write_c_pattern (p, M_REAL, item, ref_file);
  } else if (IS (p, REAL_PATTERN)) {
    write_real_pattern (p, M_REAL, M_REAL, item, ref_file);
  } else if (IS (p, COMPLEX_PATTERN)) {
    A68_REAL im;
    STATUS (&im) = INIT_MASK;
    VALUE (&im) = 0.0;
    write_complex_pattern (p, M_REAL, M_COMPLEX, (BYTE_T *) item, (BYTE_T *) & im, ref_file);
  } else {
    pattern_error (p, M_REAL, ATTRIBUTE (p));
  }
}

//! @brief Write value to file.

static void genie_write_long_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) == NO_NODE) {
    genie_value_to_string (p, M_LONG_REAL, item, ATTRIBUTE (SUB (p)));
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
    write_number_generic (p, M_LONG_REAL, item, ATTRIBUTE (SUB (p)));
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    write_c_pattern (p, M_LONG_REAL, item, ref_file);
  } else if (IS (p, REAL_PATTERN)) {
    write_real_pattern (p, M_LONG_REAL, M_LONG_REAL, item, ref_file);
  } else if (IS (p, COMPLEX_PATTERN)) {
#if (A68_LEVEL >= 3)
    ADDR_T pop_sp = A68_SP;
    A68_LONG_REAL *z = (A68_LONG_REAL *) STACK_TOP;
    QUAD_WORD_T im;
    im.f = 0.0q;
    PUSH_VALUE (p, im, A68_LONG_REAL);
    write_complex_pattern (p, M_LONG_REAL, M_LONG_COMPLEX, item, (BYTE_T *) z, ref_file);
    A68_SP = pop_sp;
#else
    ADDR_T pop_sp = A68_SP;
    MP_T *z = nil_mp (p, DIGITS (M_LONG_REAL));
    z[0] = (MP_T) INIT_MASK;
    write_complex_pattern (p, M_LONG_REAL, M_LONG_COMPLEX, item, (BYTE_T *) z, ref_file);
    A68_SP = pop_sp;
#endif
  } else {
    pattern_error (p, M_LONG_REAL, ATTRIBUTE (p));
  }
}

//! @brief Write value to file.

static void genie_write_long_mp_real_format (NODE_T * p, BYTE_T * item, A68_REF ref_file)
{
  if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) == NO_NODE) {
    genie_value_to_string (p, M_LONG_LONG_REAL, item, ATTRIBUTE (SUB (p)));
    add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
  } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
    write_number_generic (p, M_LONG_LONG_REAL, item, ATTRIBUTE (SUB (p)));
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    write_c_pattern (p, M_LONG_LONG_REAL, item, ref_file);
  } else if (IS (p, REAL_PATTERN)) {
    write_real_pattern (p, M_LONG_LONG_REAL, M_LONG_LONG_REAL, item, ref_file);
  } else if (IS (p, COMPLEX_PATTERN)) {
    ADDR_T pop_sp = A68_SP;
    MP_T *z = nil_mp (p, DIGITS (M_LONG_LONG_REAL));
    z[0] = (MP_T) INIT_MASK;
    write_complex_pattern (p, M_LONG_LONG_REAL, M_LONG_LONG_COMPLEX, item, (BYTE_T *) z, ref_file);
    A68_SP = pop_sp;
  } else {
    pattern_error (p, M_LONG_LONG_REAL, ATTRIBUTE (p));
  }
}

//! @brief At end of write purge all insertions.

static void purge_format_write (NODE_T * p, A68_REF ref_file)
{
// Problem here is shutting down embedded formats.
  BOOL_T go_on;
  do {
    A68_FILE *file;
    NODE_T *dollar, *pat;
    A68_FORMAT *old_fmt;
    while ((pat = get_next_format_pattern (p, ref_file, SKIP_PATTERN)) != NO_NODE) {
      format_error (p, ref_file, ERROR_FORMAT_PICTURES);
    }
    file = FILE_DEREF (&ref_file);
    dollar = SUB (BODY (&FORMAT (file)));
    old_fmt = (A68_FORMAT *) FRAME_LOCAL (A68_FP, OFFSET (TAX (dollar)));
    go_on = (BOOL_T) ! IS_NIL_FORMAT (old_fmt);
    if (go_on) {
// Pop embedded format and proceed.
      (void) end_of_format (p, ref_file);
    }
  } while (go_on);
}

//! @brief Write value to file.

static void genie_write_standard_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file, int *formats)
{
  errno = 0;
  ABEND (mode == NO_MOID, ERROR_INTERNAL_CONSISTENCY, __func__);
  if (mode == M_FORMAT) {
    A68_FILE *file;
    CHECK_REF (p, ref_file, M_REF_FILE);
    file = FILE_DEREF (&ref_file);
// Forget about eventual active formats and set up new one.
    if (*formats > 0) {
      purge_format_write (p, ref_file);
    }
    (*formats)++;
    A68_FP = FRAME_POINTER (file);
    A68_SP = STACK_POINTER (file);
    open_format_frame (p, ref_file, (A68_FORMAT *) item, NOT_EMBEDDED_FORMAT, A68_TRUE);
  } else if (mode == M_PROC_REF_FILE_VOID) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, M_PROC_REF_FILE_VOID);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (mode == M_SOUND) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, M_SOUND);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (mode == M_INT) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (pat)));
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NO_NODE) {
      write_number_generic (pat, M_INT, item, ATTRIBUTE (SUB (pat)));
    } else if (IS (pat, INTEGRAL_C_PATTERN) || IS (pat, FIXED_C_PATTERN) || IS (pat, FLOAT_C_PATTERN) || IS (pat, GENERAL_C_PATTERN)) {
      write_c_pattern (pat, M_INT, item, ref_file);
    } else if (IS (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, M_INT, M_INT, item, ref_file);
    } else if (IS (pat, REAL_PATTERN)) {
      write_real_pattern (pat, M_INT, M_INT, item, ref_file);
    } else if (IS (pat, COMPLEX_PATTERN)) {
      A68_REAL re, im;
      STATUS (&re) = INIT_MASK;
      VALUE (&re) = (REAL_T) VALUE ((A68_INT *) item);
      STATUS (&im) = INIT_MASK;
      VALUE (&im) = 0.0;
      write_complex_pattern (pat, M_REAL, M_COMPLEX, (BYTE_T *) & re, (BYTE_T *) & im, ref_file);
    } else if (IS (pat, CHOICE_PATTERN)) {
      int k = VALUE ((A68_INT *) item);
      write_choice_pattern (NEXT_SUB (pat), ref_file, &k);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_LONG_INT) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (pat)));
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NO_NODE) {
      write_number_generic (pat, M_LONG_INT, item, ATTRIBUTE (SUB (pat)));
    } else if (IS (pat, INTEGRAL_C_PATTERN) || IS (pat, FIXED_C_PATTERN) || IS (pat, FLOAT_C_PATTERN) || IS (pat, GENERAL_C_PATTERN)) {
      write_c_pattern (pat, M_LONG_INT, item, ref_file);
    } else if (IS (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, M_LONG_INT, M_LONG_INT, item, ref_file);
    } else if (IS (pat, REAL_PATTERN)) {
      write_real_pattern (pat, M_LONG_INT, M_LONG_INT, item, ref_file);
    } else if (IS (pat, COMPLEX_PATTERN)) {
#if (A68_LEVEL >= 3)
      ADDR_T pop_sp = A68_SP;
      A68_LONG_REAL *z = (A68_LONG_REAL *) STACK_TOP;
      QUAD_WORD_T im;
      im.f = 0.0q;
      PUSH_VALUE (p, im, A68_LONG_REAL);
      write_complex_pattern (p, M_LONG_REAL, M_LONG_COMPLEX, item, (BYTE_T *) z, ref_file);
      A68_SP = pop_sp;
#else
      ADDR_T pop_sp = A68_SP;
      MP_T *z = nil_mp (p, DIGITS (mode));
      z[0] = (MP_T) INIT_MASK;
      write_complex_pattern (pat, M_LONG_REAL, M_LONG_COMPLEX, item, (BYTE_T *) z, ref_file);
      A68_SP = pop_sp;
#endif
    } else if (IS (pat, CHOICE_PATTERN)) {
      INT_T k = mp_to_int (p, (MP_T *) item, DIGITS (mode));
      int sk;
      CHECK_INT_SHORTEN (p, k);
      sk = (int) k;
      write_choice_pattern (NEXT_SUB (pat), ref_file, &sk);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_LONG_LONG_INT) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (pat)));
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NO_NODE) {
      write_number_generic (pat, M_LONG_LONG_INT, item, ATTRIBUTE (SUB (pat)));
    } else if (IS (pat, INTEGRAL_C_PATTERN) || IS (pat, FIXED_C_PATTERN) || IS (pat, FLOAT_C_PATTERN) || IS (pat, GENERAL_C_PATTERN)) {
      write_c_pattern (pat, M_LONG_LONG_INT, item, ref_file);
    } else if (IS (pat, INTEGRAL_PATTERN)) {
      write_integral_pattern (pat, M_LONG_LONG_INT, M_LONG_LONG_INT, item, ref_file);
    } else if (IS (pat, REAL_PATTERN)) {
      write_real_pattern (pat, M_INT, M_INT, item, ref_file);
    } else if (IS (pat, REAL_PATTERN)) {
      write_real_pattern (pat, M_LONG_LONG_INT, M_LONG_LONG_INT, item, ref_file);
    } else if (IS (pat, COMPLEX_PATTERN)) {
      ADDR_T pop_sp = A68_SP;
      MP_T *z = nil_mp (p, DIGITS (M_LONG_LONG_REAL));
      z[0] = (MP_T) INIT_MASK;
      write_complex_pattern (pat, M_LONG_LONG_REAL, M_LONG_LONG_COMPLEX, item, (BYTE_T *) z, ref_file);
      A68_SP = pop_sp;
    } else if (IS (pat, CHOICE_PATTERN)) {
      INT_T k = mp_to_int (p, (MP_T *) item, DIGITS (mode));
      int sk;
      CHECK_INT_SHORTEN (p, k);
      sk = (int) k;
      write_choice_pattern (NEXT_SUB (pat), ref_file, &sk);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_REAL) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_real_format (pat, item, ref_file);
  } else if (mode == M_LONG_REAL) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_long_real_format (pat, item, ref_file);
  } else if (mode == M_LONG_LONG_REAL) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_write_long_mp_real_format (pat, item, ref_file);
  } else if (mode == M_COMPLEX) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, M_REAL, M_COMPLEX, &item[0], &item[SIZE (M_REAL)], ref_file);
    } else {
// Try writing as two REAL values.
      genie_write_real_format (pat, item, ref_file);
      genie_write_standard_format (p, M_REAL, &item[SIZE (M_REAL)], ref_file, formats);
    }
  } else if (mode == M_LONG_COMPLEX) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, M_LONG_REAL, M_LONG_COMPLEX, &item[0], &item[SIZE (M_LONG_REAL)], ref_file);
    } else {
// Try writing as two LONG REAL values.
      genie_write_long_real_format (pat, item, ref_file);
      genie_write_standard_format (p, M_LONG_REAL, &item[SIZE (M_LONG_REAL)], ref_file, formats);
    }
  } else if (mode == M_LONG_LONG_COMPLEX) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      write_complex_pattern (pat, M_LONG_LONG_REAL, M_LONG_LONG_COMPLEX, &item[0], &item[SIZE (M_LONG_LONG_REAL)], ref_file);
    } else {
// Try writing as two LONG LONG REAL values.
      genie_write_long_mp_real_format (pat, item, ref_file);
      genie_write_standard_format (p, M_LONG_LONG_REAL, &item[SIZE (M_LONG_LONG_REAL)], ref_file, formats);
    }
  } else if (mode == M_BOOL) {
    A68_BOOL *z = (A68_BOOL *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      plusab_transput_buffer (p, FORMATTED_BUFFER, (char) (VALUE (z) == A68_TRUE ? FLIP_CHAR : FLOP_CHAR));
    } else if (IS (pat, BOOLEAN_PATTERN)) {
      if (NEXT_SUB (pat) == NO_NODE) {
        plusab_transput_buffer (p, FORMATTED_BUFFER, (char) (VALUE (z) == A68_TRUE ? FLIP_CHAR : FLOP_CHAR));
      } else {
        write_boolean_pattern (pat, ref_file, (BOOL_T) (VALUE (z) == A68_TRUE));
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_BITS) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      char *str = (char *) STACK_TOP;
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (p)));
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else if (IS (pat, BITS_PATTERN)) {
      write_bits_pattern (pat, M_BITS, item, ref_file);
    } else if (IS (pat, BITS_C_PATTERN)) {
      write_c_pattern (pat, M_BITS, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_LONG_BITS || mode == M_LONG_LONG_BITS) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      char *str = (char *) STACK_TOP;
      genie_value_to_string (p, mode, item, ATTRIBUTE (SUB (p)));
      add_string_transput_buffer (p, FORMATTED_BUFFER, str);
    } else if (IS (pat, BITS_PATTERN)) {
      write_bits_pattern (pat, mode, item, ref_file);
    } else if (IS (pat, BITS_C_PATTERN)) {
      write_c_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_CHAR) {
    A68_CHAR *z = (A68_CHAR *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      plusab_transput_buffer (p, FORMATTED_BUFFER, (char) VALUE (z));
    } else if (IS (pat, STRING_PATTERN)) {
      char *q = get_transput_buffer (EDIT_BUFFER);
      reset_transput_buffer (EDIT_BUFFER);
      plusab_transput_buffer (p, EDIT_BUFFER, (char) VALUE (z));
      write_string_pattern (pat, mode, ref_file, &q);
      if (q[0] != NULL_CHAR) {
        value_error (p, mode, ref_file);
      }
    } else if (IS (pat, STRING_C_PATTERN)) {
      char zz[2];
      zz[0] = VALUE (z);
      zz[1] = '\0';
      (void) c_to_a_string (pat, zz, 1);
      write_c_pattern (pat, mode, (BYTE_T *) zz, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_ROW_CHAR || mode == M_STRING) {
// Handle these separately instead of printing [] CHAR.
    A68_REF row = *(A68_REF *) item;
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      PUSH_REF (p, row);
      add_string_from_stack_transput_buffer (p, FORMATTED_BUFFER);
    } else if (IS (pat, STRING_PATTERN)) {
      char *q;
      PUSH_REF (p, row);
      reset_transput_buffer (EDIT_BUFFER);
      add_string_from_stack_transput_buffer (p, EDIT_BUFFER);
      q = get_transput_buffer (EDIT_BUFFER);
      write_string_pattern (pat, mode, ref_file, &q);
      if (q[0] != NULL_CHAR) {
        value_error (p, mode, ref_file);
      }
    } else if (IS (pat, STRING_C_PATTERN)) {
      char *q;
      PUSH_REF (p, row);
      reset_transput_buffer (EDIT_BUFFER);
      add_string_from_stack_transput_buffer (p, EDIT_BUFFER);
      q = get_transput_buffer (EDIT_BUFFER);
      write_c_pattern (pat, mode, (BYTE_T *) q, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (IS_UNION (mode)) {
    A68_UNION *z = (A68_UNION *) item;
    genie_write_standard_format (p, (MOID_T *) (VALUE (z)), &item[A68_UNION_SIZE], ref_file, formats);
  } else if (IS_STRUCT (mode)) {
    PACK_T *q = PACK (mode);
    for (; q != NO_PACK; FORWARD (q)) {
      BYTE_T *elem = &item[OFFSET (q)];
      genie_check_initialisation (p, elem, MOID (q));
      genie_write_standard_format (p, MOID (q), elem, ref_file, formats);
    }
  } else if (IS_ROW (mode) || IS_FLEX (mode)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    CHECK_INIT (p, INITIALISED ((A68_REF *) item), M_ROWS);
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, DIM (arr)) > 0) {
      BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
      BOOL_T done = A68_FALSE;
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        ADDR_T a68_index = calculate_internal_index (tup, DIM (arr));
        ADDR_T elem_addr = ROW_ELEMENT (arr, a68_index);
        BYTE_T *elem = &base_addr[elem_addr];
        genie_check_initialisation (p, elem, SUB (deflexed));
        genie_write_standard_format (p, SUB (deflexed), elem, ref_file, formats);
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

//! @brief PROC ([] SIMPLOUT) VOID print f, write f

void genie_write_format (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_out (p);
  PUSH_REF (p, row);
  genie_write_file_format (p);
}

//! @brief PROC (REF FILE, [] SIMPLOUT) VOID put f

void genie_write_file_format (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index, formats;
  ADDR_T pop_fp, pop_sp;
  POP_REF (p, &row);
  CHECK_REF (p, row, M_ROW_SIMPLOUT);
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  file = FILE_DEREF (&ref_file);
  CHECK_INIT (p, INITIALISED (file), M_FILE);
  if (!OPENED (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_NOT_OPEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (DRAW_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "draw");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (READ_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "read");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!PUT (&CHANNEL (file))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "putting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!READ_MOOD (file) && !WRITE_MOOD (file)) {
    if (IS_NIL (STRING (file))) {
      if ((FD (file) = open_physical_file (p, ref_file, A68_WRITE_ACCESS, A68_PROTECTION)) == A68_NO_FILENO) {
        open_error (p, ref_file, "putting");
      }
    } else {
      FD (file) = open_physical_file (p, ref_file, A68_WRITE_ACCESS, 0);
    }
    DRAW_MOOD (file) = A68_FALSE;
    READ_MOOD (file) = A68_FALSE;
    WRITE_MOOD (file) = A68_TRUE;
    CHAR_MOOD (file) = A68_TRUE;
  }
  if (!CHAR_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
// Save stack state since formats have frames.
  pop_fp = FRAME_POINTER (file);
  pop_sp = STACK_POINTER (file);
  FRAME_POINTER (file) = A68_FP;
  STACK_POINTER (file) = A68_SP;
// Process [] SIMPLOUT.
  if (BODY (&FORMAT (file)) != NO_NODE) {
    open_format_frame (p, ref_file, &FORMAT (file), NOT_EMBEDDED_FORMAT, A68_FALSE);
  }
  if (elems <= 0) {
    return;
  }
  formats = 0;
  base_address = DEREF (BYTE_T, &ARRAY (arr));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_UNION *z = (A68_UNION *) & (base_address[elem_index]);
    MOID_T *mode = (MOID_T *) (VALUE (z));
    BYTE_T *item = &(base_address[elem_index + A68_UNION_SIZE]);
    genie_write_standard_format (p, mode, item, ref_file, &formats);
    elem_index += SIZE (M_SIMPLOUT);
  }
// Empty the format to purge insertions.
  purge_format_write (p, ref_file);
  BODY (&FORMAT (file)) = NO_NODE;
// Dump the buffer.
  write_purge_buffer (p, ref_file, FORMATTED_BUFFER);
// Forget about active formats.
  A68_FP = FRAME_POINTER (file);
  A68_SP = STACK_POINTER (file);
  FRAME_POINTER (file) = pop_fp;
  STACK_POINTER (file) = pop_sp;
}

//! @brief Give a value error in case a character is not among expected ones.

static BOOL_T expect (NODE_T * p, MOID_T * m, A68_REF ref_file, const char *items, char ch)
{
  if (strchr ((char *) items, ch) == NO_TEXT) {
    value_error (p, m, ref_file);
    return A68_FALSE;
  } else {
    return A68_TRUE;
  }
}

//! @brief Read a group of insertions.

void read_insertion (NODE_T * p, A68_REF ref_file)
{

// Algol68G does not check whether the insertions are textually there. It just
//skips them. This because we blank literals in sign moulds before the sign is
// put, which is non-standard Algol68, but convenient.

  A68_FILE *file = FILE_DEREF (&ref_file);
  for (; p != NO_NODE; FORWARD (p)) {
    read_insertion (SUB (p), ref_file);
    if (IS (p, FORMAT_ITEM_L)) {
      BOOL_T go_on = (BOOL_T) ! END_OF_FILE (file);
      while (go_on) {
        int ch = read_single_char (p, ref_file);
        go_on = (BOOL_T) ((ch != NEWLINE_CHAR) && (ch != EOF_CHAR) && !END_OF_FILE (file));
      }
    } else if (IS (p, FORMAT_ITEM_P)) {
      BOOL_T go_on = (BOOL_T) ! END_OF_FILE (file);
      while (go_on) {
        int ch = read_single_char (p, ref_file);
        go_on = (BOOL_T) ((ch != FORMFEED_CHAR) && (ch != EOF_CHAR) && !END_OF_FILE (file));
      }
    } else if (IS (p, FORMAT_ITEM_X) || IS (p, FORMAT_ITEM_Q)) {
      if (!END_OF_FILE (file)) {
        (void) read_single_char (p, ref_file);
      }
    } else if (IS (p, FORMAT_ITEM_Y)) {
      PUSH_REF (p, ref_file);
      PUSH_VALUE (p, -1, A68_INT);
      genie_set (p);
    } else if (IS (p, LITERAL)) {
// Skip characters, but don't check the literal. 
      int len = (int) strlen (NSYMBOL (p));
      while (len-- && !END_OF_FILE (file)) {
        (void) read_single_char (p, ref_file);
      }
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      if (ATTRIBUTE (SUB_NEXT (p)) != FORMAT_ITEM_K) {
        for (j = 1; j <= k; j++) {
          read_insertion (NEXT (p), ref_file);
        }
      } else {
        int pos = get_transput_buffer_index (INPUT_BUFFER);
        for (j = 1; j < (k - pos); j++) {
          if (!END_OF_FILE (file)) {
            (void) read_single_char (p, ref_file);
          }
        }
      }
      return;                   // Don't delete this!
    }
  }
}

//! @brief Read string from file according current format.

static void read_string_pattern (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (IS (p, FORMAT_ITEM_A)) {
      scan_n_chars (p, 1, m, ref_file);
    } else if (IS (p, FORMAT_ITEM_S)) {
      plusab_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
      return;
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        read_string_pattern (NEXT (p), m, ref_file);
      }
      return;
    } else {
      read_string_pattern (SUB (p), m, ref_file);
    }
  }
}

//! @brief Traverse choice pattern.

static void traverse_choice_pattern (NODE_T * p, char *str, int len, int *count, int *matches, int *first_match, BOOL_T * full_match)
{
  for (; p != NO_NODE; FORWARD (p)) {
    traverse_choice_pattern (SUB (p), str, len, count, matches, first_match, full_match);
    if (IS (p, LITERAL)) {
      (*count)++;
      if (strncmp (NSYMBOL (p), str, (size_t) len) == 0) {
        (*matches)++;
        (*full_match) = (BOOL_T) ((*full_match) | (strcmp (NSYMBOL (p), str) == 0));
        if (*first_match == 0 && *full_match) {
          *first_match = *count;
        }
      }
    }
  }
}

//! @brief Read appropriate insertion from a choice pattern.

static int read_choice_pattern (NODE_T * p, A68_REF ref_file)
{

// This implementation does not have the RR peculiarity that longest
// matching literal must be first, in case of non-unique first chars.

  A68_FILE *file = FILE_DEREF (&ref_file);
  BOOL_T cont = A68_TRUE;
  int longest_match = 0, longest_match_len = 0;
  while (cont) {
    int ch = char_scanner (file);
    if (!END_OF_FILE (file)) {
      int len, count = 0, matches = 0, first_match = 0;
      BOOL_T full_match = A68_FALSE;
      plusab_transput_buffer (p, INPUT_BUFFER, (char) ch);
      len = get_transput_buffer_index (INPUT_BUFFER);
      traverse_choice_pattern (p, get_transput_buffer (INPUT_BUFFER), len, &count, &matches, &first_match, &full_match);
      if (full_match && matches == 1 && first_match > 0) {
        return first_match;
      } else if (full_match && matches > 1 && first_match > 0) {
        longest_match = first_match;
        longest_match_len = len;
      } else if (matches == 0) {
        cont = A68_FALSE;
      }
    } else {
      cont = A68_FALSE;
    }
  }
  if (longest_match > 0) {
// Push back look-ahead chars.
    if (get_transput_buffer_index (INPUT_BUFFER) > 0) {
      char *z = get_transput_buffer (INPUT_BUFFER);
      END_OF_FILE (file) = A68_FALSE;
      add_string_transput_buffer (p, TRANSPUT_BUFFER (file), &z[longest_match_len]);
    }
    return longest_match;
  } else {
    value_error (p, M_INT, ref_file);
    return 0;
  }
}

//! @brief Read value according to a general-pattern.

static void read_number_generic (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  A68_REF row;
  EXECUTE_UNIT (NEXT_SUB (p));
// RR says to ignore parameters just calculated, so we will.
  POP_REF (p, &row);
  genie_read_standard (p, mode, item, ref_file);
}

// INTEGRAL, REAL, COMPLEX and BITS patterns.

//! @brief Read sign-mould according current format.

static void read_sign_mould (NODE_T * p, MOID_T * m, A68_REF ref_file, int *sign)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        read_sign_mould (NEXT (p), m, ref_file, sign);
      }
      return;                   // Leave this!
    } else {
      switch (ATTRIBUTE (p)) {
      case FORMAT_ITEM_Z:
      case FORMAT_ITEM_D:
      case FORMAT_ITEM_S:
      case FORMAT_ITEM_PLUS:
      case FORMAT_ITEM_MINUS:
        {
          int ch = read_single_char (p, ref_file);
// When a sign has been read, digits are expected.
          if (*sign != 0) {
            if (expect (p, m, ref_file, INT_DIGITS, (char) ch)) {
              plusab_transput_buffer (p, INPUT_BUFFER, (char) ch);
            } else {
              plusab_transput_buffer (p, INPUT_BUFFER, '0');
            }
// When a sign has not been read, a sign is expected.  If there is a digit
// in stead of a sign, the digit is accepted and '+' is assumed; RR demands a
// space to preceed the digit, Algol68G does not.
          } else {
            if (strchr (SIGN_DIGITS, ch) != NO_TEXT) {
              if (ch == '+') {
                *sign = 1;
              } else if (ch == '-') {
                *sign = -1;
              } else if (ch == BLANK_CHAR) {
                ;
              }
            } else if (expect (p, m, ref_file, INT_DIGITS, (char) ch)) {
              plusab_transput_buffer (p, INPUT_BUFFER, (char) ch);
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

//! @brief Read mould according current format.

static void read_integral_mould (NODE_T * p, MOID_T * m, A68_REF ref_file)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      read_insertion (SUB (p), ref_file);
    } else if (IS (p, REPLICATOR)) {
      int j, k = get_replicator_value (SUB (p), A68_TRUE);
      for (j = 1; j <= k; j++) {
        read_integral_mould (NEXT (p), m, ref_file);
      }
      return;                   // Leave this!
    } else if (IS (p, FORMAT_ITEM_Z)) {
      int ch = read_single_char (p, ref_file);
      const char *digits = (m == M_BITS || m == M_LONG_BITS || m == M_LONG_LONG_BITS) ? BITS_DIGITS_BLANK : INT_DIGITS_BLANK;
      if (expect (p, m, ref_file, digits, (char) ch)) {
        plusab_transput_buffer (p, INPUT_BUFFER, (char) ((ch == BLANK_CHAR) ? '0' : ch));
      } else {
        plusab_transput_buffer (p, INPUT_BUFFER, '0');
      }
    } else if (IS (p, FORMAT_ITEM_D)) {
      int ch = read_single_char (p, ref_file);
      const char *digits = (m == M_BITS || m == M_LONG_BITS || m == M_LONG_LONG_BITS) ? BITS_DIGITS : INT_DIGITS;
      if (expect (p, m, ref_file, digits, (char) ch)) {
        plusab_transput_buffer (p, INPUT_BUFFER, (char) ch);
      } else {
        plusab_transput_buffer (p, INPUT_BUFFER, '0');
      }
    } else if (IS (p, FORMAT_ITEM_S)) {
      plusab_transput_buffer (p, INPUT_BUFFER, '0');
    } else {
      read_integral_mould (SUB (p), m, ref_file);
    }
  }
}

//! @brief Read mould according current format.

static void read_integral_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
  NODE_T *q = SUB (p);
  if (q != NO_NODE && IS (q, SIGN_MOULD)) {
    int sign = 0;
    char *z;
    plusab_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
    read_sign_mould (SUB (q), m, ref_file, &sign);
    z = get_transput_buffer (INPUT_BUFFER);
    z[0] = (char) ((sign == -1) ? '-' : '+');
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
  }
  genie_string_to_value (p, m, item, ref_file);
}

//! @brief Read point, exponent or i-frame.

static void read_pie_frame (NODE_T * p, MOID_T * m, A68_REF ref_file, int att, int item, char ch)
{
// Widen ch to a stringlet.
  char sym[3];
  sym[0] = ch;
  sym[1] = (char) TO_LOWER (ch);
  sym[2] = NULL_CHAR;
// Now read the frame.
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, INSERTION)) {
      read_insertion (p, ref_file);
    } else if (IS (p, att)) {
      read_pie_frame (SUB (p), m, ref_file, att, item, ch);
      return;
    } else if (IS (p, FORMAT_ITEM_S)) {
      plusab_transput_buffer (p, INPUT_BUFFER, sym[0]);
      return;
    } else if (IS (p, item)) {
      int ch0 = read_single_char (p, ref_file);
      if (expect (p, m, ref_file, sym, (char) ch0)) {
        plusab_transput_buffer (p, INPUT_BUFFER, sym[0]);
      } else {
        plusab_transput_buffer (p, INPUT_BUFFER, sym[0]);
      }
    }
  }
}

//! @brief Read REAL value using real pattern.

static void read_real_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
// Dive into pattern.
  NODE_T *q = (IS (p, REAL_PATTERN)) ? SUB (p) : p;
// Dissect pattern.
  if (q != NO_NODE && IS (q, SIGN_MOULD)) {
    int sign = 0;
    char *z;
    plusab_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
    read_sign_mould (SUB (q), m, ref_file, &sign);
    z = get_transput_buffer (INPUT_BUFFER);
    z[0] = (char) ((sign == -1) ? '-' : '+');
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, FORMAT_POINT_FRAME)) {
    read_pie_frame (SUB (q), m, ref_file, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT, POINT_CHAR);
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
    read_integral_mould (SUB (q), m, ref_file);
    FORWARD (q);
  }
  if (q != NO_NODE && IS (q, EXPONENT_FRAME)) {
    read_pie_frame (SUB (q), m, ref_file, FORMAT_E_FRAME, FORMAT_ITEM_E, EXPONENT_CHAR);
    q = NEXT_SUB (q);
    if (q != NO_NODE && IS (q, SIGN_MOULD)) {
      int k, sign = 0;
      char *z;
      plusab_transput_buffer (p, INPUT_BUFFER, BLANK_CHAR);
      k = get_transput_buffer_index (INPUT_BUFFER);
      read_sign_mould (SUB (q), m, ref_file, &sign);
      z = get_transput_buffer (INPUT_BUFFER);
      z[k - 1] = (char) ((sign == -1) ? '-' : '+');
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, INTEGRAL_MOULD)) {
      read_integral_mould (SUB (q), m, ref_file);
      FORWARD (q);
    }
  }
  genie_string_to_value (p, m, item, ref_file);
}

//! @brief Read COMPLEX value using complex pattern.

static void read_complex_pattern (NODE_T * p, MOID_T * comp, MOID_T * m, BYTE_T * re, BYTE_T * im, A68_REF ref_file)
{
  NODE_T *reel, *plus_i_times, *imag;
// Dissect pattern.
  reel = SUB (p);
  plus_i_times = NEXT (reel);
  imag = NEXT (plus_i_times);
// Read pattern.
  read_real_pattern (reel, m, re, ref_file);
  reset_transput_buffer (INPUT_BUFFER);
  read_pie_frame (plus_i_times, comp, ref_file, FORMAT_I_FRAME, FORMAT_ITEM_I, 'I');
  reset_transput_buffer (INPUT_BUFFER);
  read_real_pattern (imag, m, im, ref_file);
}

//! @brief Read BITS value according pattern.

static void read_bits_pattern (NODE_T * p, MOID_T * m, BYTE_T * item, A68_REF ref_file)
{
  int radix;
  char *z;
  radix = get_replicator_value (SUB_SUB (p), A68_TRUE);
  if (radix < 2 || radix > 16) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, radix);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  z = get_transput_buffer (INPUT_BUFFER);
  ASSERT (snprintf (z, (size_t) TRANSPUT_BUFFER_SIZE, "%dr", radix) >= 0);
  set_transput_buffer_index (INPUT_BUFFER, (int) strlen (z));
  read_integral_mould (NEXT_SUB (p), m, ref_file);
  genie_string_to_value (p, m, item, ref_file);
}

//! @brief Read object with from file and store.

static void genie_read_real_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file)
{
  if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) == NO_NODE) {
    genie_read_standard (p, mode, item, ref_file);
  } else if (IS (p, GENERAL_PATTERN) && NEXT_SUB (p) != NO_NODE) {
    read_number_generic (p, mode, item, ref_file);
  } else if (IS (p, FIXED_C_PATTERN) || IS (p, FLOAT_C_PATTERN) || IS (p, GENERAL_C_PATTERN)) {
    read_c_pattern (p, mode, item, ref_file);
  } else if (IS (p, REAL_PATTERN)) {
    read_real_pattern (p, mode, item, ref_file);
  } else {
    pattern_error (p, mode, ATTRIBUTE (p));
  }
}

//! @brief At end of read purge all insertions.

static void purge_format_read (NODE_T * p, A68_REF ref_file)
{
  BOOL_T go_on;
  do {
    A68_FILE *file;
    NODE_T *dollar, *pat;
    A68_FORMAT *old_fmt;
    while ((pat = get_next_format_pattern (p, ref_file, SKIP_PATTERN)) != NO_NODE) {
      format_error (p, ref_file, ERROR_FORMAT_PICTURES);
    }
    file = FILE_DEREF (&ref_file);
    dollar = SUB (BODY (&FORMAT (file)));
    old_fmt = (A68_FORMAT *) FRAME_LOCAL (A68_FP, OFFSET (TAX (dollar)));
    go_on = (BOOL_T) ! IS_NIL_FORMAT (old_fmt);
    if (go_on) {
// Pop embedded format and proceed.
      (void) end_of_format (p, ref_file);
    }
  } while (go_on);
}

//! @brief Read object with from file and store.

static void genie_read_standard_format (NODE_T * p, MOID_T * mode, BYTE_T * item, A68_REF ref_file, int *formats)
{
  errno = 0;
  reset_transput_buffer (INPUT_BUFFER);
  if (mode == M_FORMAT) {
    A68_FILE *file;
    CHECK_REF (p, ref_file, M_REF_FILE);
    file = FILE_DEREF (&ref_file);
// Forget about eventual active formats and set up new one.
    if (*formats > 0) {
      purge_format_read (p, ref_file);
    }
    (*formats)++;
    A68_FP = FRAME_POINTER (file);
    A68_SP = STACK_POINTER (file);
    open_format_frame (p, ref_file, (A68_FORMAT *) item, NOT_EMBEDDED_FORMAT, A68_TRUE);
  } else if (mode == M_PROC_REF_FILE_VOID) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, M_PROC_REF_FILE_VOID);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (mode == M_REF_SOUND) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_UNDEFINED_TRANSPUT, M_REF_SOUND);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (IS_REF (mode)) {
    CHECK_REF (p, *(A68_REF *) item, mode);
    genie_read_standard_format (p, SUB (mode), ADDRESS ((A68_REF *) item), ref_file, formats);
  } else if (mode == M_INT || mode == M_LONG_INT || mode == M_LONG_LONG_INT) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (pat, mode, item, ref_file);
    } else if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) != NO_NODE) {
      read_number_generic (pat, mode, item, ref_file);
    } else if (IS (pat, INTEGRAL_C_PATTERN)) {
      read_c_pattern (pat, mode, item, ref_file);
    } else if (IS (pat, INTEGRAL_PATTERN)) {
      read_integral_pattern (pat, mode, item, ref_file);
    } else if (IS (pat, CHOICE_PATTERN)) {
      int k = read_choice_pattern (pat, ref_file);
      if (mode == M_INT) {
        A68_INT *z = (A68_INT *) item;
        VALUE (z) = k;
        STATUS (z) = (STATUS_MASK_T) ((VALUE (z) > 0) ? INIT_MASK : NULL_MASK);
      } else {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_DEPRECATED, mode);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_REAL || mode == M_LONG_REAL || mode == M_LONG_LONG_REAL) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    genie_read_real_format (pat, mode, item, ref_file);
  } else if (mode == M_COMPLEX) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, M_REAL, item, &item[SIZE (M_REAL)], ref_file);
    } else {
// Try reading as two REAL values.
      genie_read_real_format (pat, M_REAL, item, ref_file);
      genie_read_standard_format (p, M_REAL, &item[SIZE (M_REAL)], ref_file, formats);
    }
  } else if (mode == M_LONG_COMPLEX) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, M_LONG_REAL, item, &item[SIZE (M_LONG_REAL)], ref_file);
    } else {
// Try reading as two LONG REAL values.
      genie_read_real_format (pat, M_LONG_REAL, item, ref_file);
      genie_read_standard_format (p, M_LONG_REAL, &item[SIZE (M_LONG_REAL)], ref_file, formats);
    }
  } else if (mode == M_LONG_LONG_COMPLEX) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, COMPLEX_PATTERN)) {
      read_complex_pattern (pat, mode, M_LONG_LONG_REAL, item, &item[SIZE (M_LONG_LONG_REAL)], ref_file);
    } else {
// Try reading as two LONG LONG REAL values.
      genie_read_real_format (pat, M_LONG_LONG_REAL, item, ref_file);
      genie_read_standard_format (p, M_LONG_LONG_REAL, &item[SIZE (M_LONG_LONG_REAL)], ref_file, formats);
    }
  } else if (mode == M_BOOL) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (IS (pat, BOOLEAN_PATTERN)) {
      if (NEXT_SUB (pat) == NO_NODE) {
        genie_read_standard (p, mode, item, ref_file);
      } else {
        A68_BOOL *z = (A68_BOOL *) item;
        int k = read_choice_pattern (pat, ref_file);
        if (k == 1 || k == 2) {
          VALUE (z) = (BOOL_T) ((k == 1) ? A68_TRUE : A68_FALSE);
          STATUS (z) = INIT_MASK;
        } else {
          STATUS (z) = NULL_MASK;
        }
      }
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_BITS || mode == M_LONG_BITS || mode == M_LONG_LONG_BITS) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (IS (pat, BITS_PATTERN)) {
      read_bits_pattern (pat, mode, item, ref_file);
    } else if (IS (pat, BITS_C_PATTERN)) {
      read_c_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_CHAR) {
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (IS (pat, STRING_PATTERN)) {
      read_string_pattern (pat, M_CHAR, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    } else if (IS (pat, CHAR_C_PATTERN)) {
      read_c_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (mode == M_ROW_CHAR || mode == M_STRING) {
// Handle these separately instead of reading [] CHAR.
    NODE_T *pat = get_next_format_pattern (p, ref_file, WANT_PATTERN);
    if (IS (pat, GENERAL_PATTERN) && NEXT_SUB (pat) == NO_NODE) {
      genie_read_standard (p, mode, item, ref_file);
    } else if (IS (pat, STRING_PATTERN)) {
      read_string_pattern (pat, mode, ref_file);
      genie_string_to_value (p, mode, item, ref_file);
    } else if (IS (pat, STRING_C_PATTERN)) {
      read_c_pattern (pat, mode, item, ref_file);
    } else {
      pattern_error (p, mode, ATTRIBUTE (pat));
    }
  } else if (IS_UNION (mode)) {
    A68_UNION *z = (A68_UNION *) item;
    genie_read_standard_format (p, (MOID_T *) (VALUE (z)), &item[A68_UNION_SIZE], ref_file, formats);
  } else if (IS_STRUCT (mode)) {
    PACK_T *q = PACK (mode);
    for (; q != NO_PACK; FORWARD (q)) {
      BYTE_T *elem = &item[OFFSET (q)];
      genie_read_standard_format (p, MOID (q), elem, ref_file, formats);
    }
  } else if (IS_ROW (mode) || IS_FLEX (mode)) {
    MOID_T *deflexed = DEFLEX (mode);
    A68_ARRAY *arr;
    A68_TUPLE *tup;
    CHECK_INIT (p, INITIALISED ((A68_REF *) item), M_ROWS);
    GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
    if (get_row_size (tup, DIM (arr)) > 0) {
      BYTE_T *base_addr = DEREF (BYTE_T, &ARRAY (arr));
      BOOL_T done = A68_FALSE;
      initialise_internal_index (tup, DIM (arr));
      while (!done) {
        ADDR_T a68_index = calculate_internal_index (tup, DIM (arr));
        ADDR_T elem_addr = ROW_ELEMENT (arr, a68_index);
        BYTE_T *elem = &base_addr[elem_addr];
        genie_read_standard_format (p, SUB (deflexed), elem, ref_file, formats);
        done = increment_internal_index (tup, DIM (arr));
      }
    }
  }
  if (errno != 0) {
    transput_error (p, ref_file, mode);
  }
}

//! @brief PROC ([] SIMPLIN) VOID read f

void genie_read_format (NODE_T * p)
{
  A68_REF row;
  POP_REF (p, &row);
  genie_stand_in (p);
  PUSH_REF (p, row);
  genie_read_file_format (p);
}

//! @brief PROC (REF FILE, [] SIMPLIN) VOID get f

void genie_read_file_format (NODE_T * p)
{
  A68_REF ref_file;
  A68_FILE *file;
  A68_REF row;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base_address;
  int elems, k, elem_index, formats;
  ADDR_T pop_fp, pop_sp;
  POP_REF (p, &row);
  CHECK_REF (p, row, M_ROW_SIMPLIN);
  GET_DESCRIPTOR (arr, tup, &row);
  elems = ROW_SIZE (tup);
  POP_REF (p, &ref_file);
  CHECK_REF (p, ref_file, M_REF_FILE);
  file = FILE_DEREF (&ref_file);
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
  }
  if (!GET (&CHANNEL (file))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_CHANNEL_DOES_NOT_ALLOW, "getting");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!READ_MOOD (file) && !WRITE_MOOD (file)) {
    if (IS_NIL (STRING (file))) {
      if ((FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS, 0)) == A68_NO_FILENO) {
        open_error (p, ref_file, "getting");
      }
    } else {
      FD (file) = open_physical_file (p, ref_file, A68_READ_ACCESS, 0);
    }
    DRAW_MOOD (file) = A68_FALSE;
    READ_MOOD (file) = A68_TRUE;
    WRITE_MOOD (file) = A68_FALSE;
    CHAR_MOOD (file) = A68_TRUE;
  }
  if (!CHAR_MOOD (file)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_FILE_WRONG_MOOD, "binary");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
// Save stack state since formats have frames.
  pop_fp = FRAME_POINTER (file);
  pop_sp = STACK_POINTER (file);
  FRAME_POINTER (file) = A68_FP;
  STACK_POINTER (file) = A68_SP;
// Process [] SIMPLIN.
  if (BODY (&FORMAT (file)) != NO_NODE) {
    open_format_frame (p, ref_file, &FORMAT (file), NOT_EMBEDDED_FORMAT, A68_FALSE);
  }
  if (elems <= 0) {
    return;
  }
  formats = 0;
  base_address = DEREF (BYTE_T, &ARRAY (arr));
  elem_index = 0;
  for (k = 0; k < elems; k++) {
    A68_UNION *z = (A68_UNION *) & (base_address[elem_index]);
    MOID_T *mode = (MOID_T *) (VALUE (z));
    BYTE_T *item = (BYTE_T *) & (base_address[elem_index + A68_UNION_SIZE]);
    genie_read_standard_format (p, mode, item, ref_file, &formats);
    elem_index += SIZE (M_SIMPLIN);
  }
// Empty the format to purge insertions.
  purge_format_read (p, ref_file);
  BODY (&FORMAT (file)) = NO_NODE;
// Forget about active formats.
  A68_FP = FRAME_POINTER (file);
  A68_SP = STACK_POINTER (file);
  FRAME_POINTER (file) = pop_fp;
  STACK_POINTER (file) = pop_sp;
}
