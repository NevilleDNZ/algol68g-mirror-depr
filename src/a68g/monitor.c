//! @file monitor.c
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

// Gdb-style monitor for the interpreter.
// This is a basic monitor for Algol68G. It activates when the interpreter
// receives SIGINT (CTRL-C, for instance) or when PROC VOID break, debug or
// evaluate is called, or when a runtime error occurs and --debug is selected.
//
// The monitor allows single stepping (unit-wise through serial/enquiry
// clauses) and has basic means for inspecting call-frame stack and heap. 

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-frames.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-transput.h"
#include "a68g-parser.h"
#include "a68g-listing.h"

#define CANNOT_SHOW " unprintable value or uninitialised value"
#define MAX_ROW_ELEMS 24
#define NOT_A_NUM (-1)
#define NO_VALUE " uninitialised value"
#define TOP_MODE (A68_MON (_m_stack)[A68_MON (_m_sp) - 1])
#define LOGOUT_STRING "exit"

static void parse (FILE_T, NODE_T *, int);

static BOOL_T check_initialisation (NODE_T *, BYTE_T *, MOID_T *, BOOL_T *);

#define SKIP_ONE_SYMBOL(sym) {\
  while (!IS_SPACE ((sym)[0]) && (sym)[0] != NULL_CHAR) {\
    (sym)++;\
  }\
  while (IS_SPACE ((sym)[0]) && (sym)[0] != NULL_CHAR) {\
    (sym)++;\
  }}

#define SKIP_SPACE(sym) {\
  while (IS_SPACE ((sym)[0]) && (sym)[0] != NULL_CHAR) {\
    (sym)++;\
  }}

#define CHECK_MON_REF(p, z, m)\
  if (! INITIALISED (&z)) {\
    ASSERT (snprintf(A68 (edit_line), SNPRINTF_SIZE, "%s", moid_to_string ((m), MOID_WIDTH, NO_NODE)) >= 0);\
    monitor_error (NO_VALUE, A68 (edit_line));\
    QUIT_ON_ERROR;\
  } else if (IS_NIL (z)) {\
    ASSERT (snprintf(A68 (edit_line), SNPRINTF_SIZE, "%s", moid_to_string ((m), MOID_WIDTH, NO_NODE)) >= 0);\
    monitor_error ("accessing NIL name", A68 (edit_line));\
    QUIT_ON_ERROR;\
  }

#define QUIT_ON_ERROR\
  if (A68_MON (mon_errors) > 0) {\
    return;\
  }

#define PARSE_CHECK(f, p, d)\
  parse ((f), (p), (d));\
  QUIT_ON_ERROR;

#define SCAN_CHECK(f, p)\
  scan_sym((f), (p));\
  QUIT_ON_ERROR;

//! @brief Confirm that we really want to quit.

static BOOL_T confirm_exit (void)
{
  char *cmd;
  int k;
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Terminate %s (yes|no): ", A68 (a68_cmd_name)) >= 0);
  WRITELN (STDOUT_FILENO, A68 (output_line));
  cmd = read_string_from_tty (NULL);
  if (TO_UCHAR (cmd[0]) == TO_UCHAR (EOF_CHAR)) {
    return confirm_exit ();
  }
  for (k = 0; cmd[k] != NULL_CHAR; k++) {
    cmd[k] = (char) TO_LOWER (cmd[k]);
  }
  if (strcmp (cmd, "y") == 0) {
    return A68_TRUE;
  }
  if (strcmp (cmd, "yes") == 0) {
    return A68_TRUE;
  }
  if (strcmp (cmd, "n") == 0) {
    return A68_FALSE;
  }
  if (strcmp (cmd, "no") == 0) {
    return A68_FALSE;
  }
  return confirm_exit ();
}

//! @brief Give a monitor error message.

void monitor_error (char *msg, char *info)
{
  QUIT_ON_ERROR;
  A68_MON (mon_errors)++;
  bufcpy (A68_MON (error_text), msg, BUFFER_SIZE);
  WRITELN (STDOUT_FILENO, A68 (a68_cmd_name));
  WRITE (STDOUT_FILENO, ": monitor error: ");
  WRITE (STDOUT_FILENO, A68_MON (error_text));
  if (info != NO_TEXT) {
    WRITE (STDOUT_FILENO, " (");
    WRITE (STDOUT_FILENO, info);
    WRITE (STDOUT_FILENO, ")");
  }
  WRITE (STDOUT_FILENO, ".");
}

//! @brief Scan symbol from input.

static void scan_sym (FILE_T f, NODE_T * p)
{
  int k = 0;
  (void) f;
  (void) p;
  A68_MON (symbol)[0] = NULL_CHAR;
  A68_MON (attr) = 0;
  QUIT_ON_ERROR;
  while (IS_SPACE (A68_MON (expr)[A68_MON (pos)])) {
    A68_MON (pos)++;
  }
  if (A68_MON (expr)[A68_MON (pos)] == NULL_CHAR) {
    A68_MON (attr) = 0;
    A68_MON (symbol)[0] = NULL_CHAR;
    return;
  } else if (A68_MON (expr)[A68_MON (pos)] == ':') {
    if (strncmp (&(A68_MON (expr)[A68_MON (pos)]), ":=:", 3) == 0) {
      A68_MON (pos) += 3;
      bufcpy (A68_MON (symbol), ":=:", BUFFER_SIZE);
      A68_MON (attr) = IS_SYMBOL;
    } else if (strncmp (&(A68_MON (expr)[A68_MON (pos)]), ":/=:", 4) == 0) {
      A68_MON (pos) += 4;
      bufcpy (A68_MON (symbol), ":/=:", BUFFER_SIZE);
      A68_MON (attr) = ISNT_SYMBOL;
    } else if (strncmp (&(A68_MON (expr)[A68_MON (pos)]), ":=", 2) == 0) {
      A68_MON (pos) += 2;
      bufcpy (A68_MON (symbol), ":=", BUFFER_SIZE);
      A68_MON (attr) = ASSIGN_SYMBOL;
    } else {
      A68_MON (pos)++;
      bufcpy (A68_MON (symbol), ":", BUFFER_SIZE);
      A68_MON (attr) = COLON_SYMBOL;
    }
    return;
  } else if (A68_MON (expr)[A68_MON (pos)] == QUOTE_CHAR) {
    BOOL_T cont = A68_TRUE;
    A68_MON (pos)++;
    while (cont) {
      while (A68_MON (expr)[A68_MON (pos)] != QUOTE_CHAR) {
        A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      }
      if (A68_MON (expr)[++A68_MON (pos)] == QUOTE_CHAR) {
        A68_MON (symbol)[k++] = QUOTE_CHAR;
      } else {
        cont = A68_FALSE;
      }
    }
    A68_MON (symbol)[k] = NULL_CHAR;
    A68_MON (attr) = ROW_CHAR_DENOTATION;
    return;
  } else if (IS_LOWER (A68_MON (expr)[A68_MON (pos)])) {
    while (IS_LOWER (A68_MON (expr)[A68_MON (pos)]) || IS_DIGIT (A68_MON (expr)[A68_MON (pos)]) || IS_SPACE (A68_MON (expr)[A68_MON (pos)])) {
      if (IS_SPACE (A68_MON (expr)[A68_MON (pos)])) {
        A68_MON (pos)++;
      } else {
        A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      }
    }
    A68_MON (symbol)[k] = NULL_CHAR;
    A68_MON (attr) = IDENTIFIER;
    return;
  } else if (IS_UPPER (A68_MON (expr)[A68_MON (pos)])) {
    KEYWORD_T *kw;
    while (IS_UPPER (A68_MON (expr)[A68_MON (pos)])) {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
    }
    A68_MON (symbol)[k] = NULL_CHAR;
    kw = find_keyword (A68 (top_keyword), A68_MON (symbol));
    if (kw != NO_KEYWORD) {
      A68_MON (attr) = ATTRIBUTE (kw);
    } else {
      A68_MON (attr) = OPERATOR;
    }
    return;
  } else if (IS_DIGIT (A68_MON (expr)[A68_MON (pos)])) {
    while (IS_DIGIT (A68_MON (expr)[A68_MON (pos)])) {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
    }
    if (A68_MON (expr)[A68_MON (pos)] == 'r') {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      while (IS_XDIGIT (A68_MON (expr)[A68_MON (pos)])) {
        A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      }
      A68_MON (symbol)[k] = NULL_CHAR;
      A68_MON (attr) = BITS_DENOTATION;
      return;
    }
    if (A68_MON (expr)[A68_MON (pos)] != POINT_CHAR && A68_MON (expr)[A68_MON (pos)] != 'e' && A68_MON (expr)[A68_MON (pos)] != 'E') {
      A68_MON (symbol)[k] = NULL_CHAR;
      A68_MON (attr) = INT_DENOTATION;
      return;
    }
    if (A68_MON (expr)[A68_MON (pos)] == POINT_CHAR) {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      while (IS_DIGIT (A68_MON (expr)[A68_MON (pos)])) {
        A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      }
    }
    if (A68_MON (expr)[A68_MON (pos)] != 'e' && A68_MON (expr)[A68_MON (pos)] != 'E') {
      A68_MON (symbol)[k] = NULL_CHAR;
      A68_MON (attr) = REAL_DENOTATION;
      return;
    }
    A68_MON (symbol)[k++] = (char) TO_UPPER (A68_MON (expr)[A68_MON (pos)++]);
    if (A68_MON (expr)[A68_MON (pos)] == '+' || A68_MON (expr)[A68_MON (pos)] == '-') {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
    }
    while (IS_DIGIT (A68_MON (expr)[A68_MON (pos)])) {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
    }
    A68_MON (symbol)[k] = NULL_CHAR;
    A68_MON (attr) = REAL_DENOTATION;
    return;
  } else if (strchr (MONADS, A68_MON (expr)[A68_MON (pos)]) != NO_TEXT || strchr (NOMADS, A68_MON (expr)[A68_MON (pos)]) != NO_TEXT) {
    A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
    if (strchr (NOMADS, A68_MON (expr)[A68_MON (pos)]) != NO_TEXT) {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
    }
    if (A68_MON (expr)[A68_MON (pos)] == ':') {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      if (A68_MON (expr)[A68_MON (pos)] == '=') {
        A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      } else {
        A68_MON (symbol)[k] = NULL_CHAR;
        monitor_error ("invalid operator symbol", A68_MON (symbol));
      }
    } else if (A68_MON (expr)[A68_MON (pos)] == '=') {
      A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      if (A68_MON (expr)[A68_MON (pos)] == ':') {
        A68_MON (symbol)[k++] = A68_MON (expr)[A68_MON (pos)++];
      } else {
        A68_MON (symbol)[k] = NULL_CHAR;
        monitor_error ("invalid operator symbol", A68_MON (symbol));
      }
    }
    A68_MON (symbol)[k] = NULL_CHAR;
    A68_MON (attr) = OPERATOR;
    return;
  } else if (A68_MON (expr)[A68_MON (pos)] == '(') {
    A68_MON (pos)++;
    A68_MON (attr) = OPEN_SYMBOL;
    return;
  } else if (A68_MON (expr)[A68_MON (pos)] == ')') {
    A68_MON (pos)++;
    A68_MON (attr) = CLOSE_SYMBOL;
    return;
  } else if (A68_MON (expr)[A68_MON (pos)] == '[') {
    A68_MON (pos)++;
    A68_MON (attr) = SUB_SYMBOL;
    return;
  } else if (A68_MON (expr)[A68_MON (pos)] == ']') {
    A68_MON (pos)++;
    A68_MON (attr) = BUS_SYMBOL;
    return;
  } else if (A68_MON (expr)[A68_MON (pos)] == ',') {
    A68_MON (pos)++;
    A68_MON (attr) = COMMA_SYMBOL;
    return;
  } else if (A68_MON (expr)[A68_MON (pos)] == ';') {
    A68_MON (pos)++;
    A68_MON (attr) = SEMI_SYMBOL;
    return;
  }
}

//! @brief Find a tag, searching symbol tables towards the root.

static TAG_T *find_tag (TABLE_T * table, int a, char *name)
{
  if (table != NO_TABLE) {
    TAG_T *s = NO_TAG;
    if (a == OP_SYMBOL) {
      s = OPERATORS (table);
    } else if (a == PRIO_SYMBOL) {
      s = PRIO (table);
    } else if (a == IDENTIFIER) {
      s = IDENTIFIERS (table);
    } else if (a == INDICANT) {
      s = INDICANTS (table);
    } else if (a == LABEL) {
      s = LABELS (table);
    } else {
      ABEND (A68_TRUE, ERROR_INTERNAL_CONSISTENCY, __func__);
    }
    for (; s != NO_TAG; FORWARD (s)) {
      if (strcmp (NSYMBOL (NODE (s)), name) == 0) {
        return s;
      }
    }
    return find_tag_global (PREVIOUS (table), a, name);
  } else {
    return NO_TAG;
  }
}

//! @brief Priority for symbol at input.

static int prio (FILE_T f, NODE_T * p)
{
  TAG_T *s = find_tag (A68_STANDENV, PRIO_SYMBOL, A68_MON (symbol));
  (void) p;
  (void) f;
  if (s == NO_TAG) {
    monitor_error ("unknown operator, cannot set priority", A68_MON (symbol));
    return 0;
  }
  return PRIO (s);
}

//! @brief Push a mode on the stack.

static void push_mode (FILE_T f, MOID_T * m)
{
  (void) f;
  if (A68_MON (_m_sp) < MON_STACK_SIZE) {
    A68_MON (_m_stack)[A68_MON (_m_sp)++] = m;
  } else {
    monitor_error ("expression too complex", NO_TEXT);
  }
}

//! @brief Dereference, WEAK or otherwise.

static BOOL_T deref_condition (int k, int context)
{
  MOID_T *u = A68_MON (_m_stack)[k];
  if (context == WEAK && SUB (u) != NO_MOID) {
    MOID_T *v = SUB (u);
    BOOL_T stowed = (BOOL_T) (IS_FLEX (v) || IS_ROW (v) || IS_STRUCT (v));
    return (BOOL_T) (IS_REF (u) && !stowed);
  } else {
    return (BOOL_T) (IS_REF (u));
  }
}

//! @brief Weak dereferencing.

static void deref (NODE_T * p, int k, int context)
{
  while (deref_condition (k, context)) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_MON_REF (p, z, A68_MON (_m_stack)[k]);
    A68_MON (_m_stack)[k] = SUB (A68_MON (_m_stack)[k]);
    PUSH (p, ADDRESS (&z), SIZE (A68_MON (_m_stack)[k]));
  }
}

//! @brief Search moid that matches indicant.

static MOID_T *search_mode (int refs, int leng, char *indy)
{
  MOID_T *m = NO_MOID, *z = NO_MOID;
  for (m = TOP_MOID (&A68_JOB); m != NO_MOID; FORWARD (m)) {
    if (NODE (m) != NO_NODE) {
      if (indy == NSYMBOL (NODE (m)) && leng == DIM (m)) {
        z = m;
        while (EQUIVALENT (z) != NO_MOID) {
          z = EQUIVALENT (z);
        }
      }
    }
  }
  if (z == NO_MOID) {
    monitor_error ("unknown indicant", indy);
    return NO_MOID;
  }
  for (m = TOP_MOID (&A68_JOB); m != NO_MOID; FORWARD (m)) {
    int k = 0;
    while (IS_REF (m)) {
      k++;
      m = SUB (m);
    }
    if (k == refs && m == z) {
      while (EQUIVALENT (z) != NO_MOID) {
        z = EQUIVALENT (z);
      }
      return z;
    }
  }
  return NO_MOID;
}

//! @brief Search operator X SYM Y.

static TAG_T *search_operator (char *sym, MOID_T * x, MOID_T * y)
{
  TAG_T *t;
  for (t = OPERATORS (A68_STANDENV); t != NO_TAG; FORWARD (t)) {
    if (strcmp (NSYMBOL (NODE (t)), sym) == 0) {
      PACK_T *p = PACK (MOID (t));
      if (x == MOID (p)) {
        FORWARD (p);
        if (p == NO_PACK && y == NO_MOID) {
// Matched in case of a monad.
          return t;
        } else if (p != NO_PACK && y != NO_MOID && y == MOID (p)) {
// Matched in case of a nomad.
          return t;
        }
      }
    }
  }
// Not found yet, try dereferencing.
  if (IS_REF (x)) {
    return search_operator (sym, SUB (x), y);
  }
  if (y != NO_MOID && IS_REF (y)) {
    return search_operator (sym, x, SUB (y));
  }
// Not found. Grrrr. Give a message.
  if (y == NO_MOID) {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s %s", sym, moid_to_string (x, MOID_WIDTH, NO_NODE)) >= 0);
  } else {
    ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s %s %s", moid_to_string (x, MOID_WIDTH, NO_NODE), sym, moid_to_string (y, MOID_WIDTH, NO_NODE)) >= 0);
  }
  monitor_error ("cannot find operator in standard environ", A68 (edit_line));
  return NO_TAG;
}

//! @brief Search identifier in frame stack and push value.

static void search_identifier (FILE_T f, NODE_T * p, ADDR_T a68_link, char *sym)
{
  if (a68_link > 0) {
    int dynamic_a68_link = FRAME_DYNAMIC_LINK (a68_link);
    if (A68_MON (current_frame) == 0 || (A68_MON (current_frame) == FRAME_NUMBER (a68_link))) {
      NODE_T *u = FRAME_TREE (a68_link);
      if (u != NO_NODE) {
        TABLE_T *q = TABLE (u);
        TAG_T *i = IDENTIFIERS (q);
        for (; i != NO_TAG; FORWARD (i)) {
          if (strcmp (NSYMBOL (NODE (i)), sym) == 0) {
            ADDR_T posit = a68_link + FRAME_INFO_SIZE + OFFSET (i);
            MOID_T *m = MOID (i);
            PUSH (p, FRAME_ADDRESS (posit), SIZE (m));
            push_mode (f, m);
            return;
          }
        }
      }
    }
    search_identifier (f, p, dynamic_a68_link, sym);
  } else {
    TABLE_T *q = A68_STANDENV;
    TAG_T *i = IDENTIFIERS (q);
    for (; i != NO_TAG; FORWARD (i)) {
      if (strcmp (NSYMBOL (NODE (i)), sym) == 0) {
        if (IS (MOID (i), PROC_SYMBOL)) {
          static A68_PROCEDURE z;
          STATUS (&z) = (STATUS_MASK_T) (INIT_MASK | STANDENV_PROC_MASK);
          PROCEDURE (&(BODY (&z))) = PROCEDURE (i);
          ENVIRON (&z) = 0;
          LOCALE (&z) = NO_HANDLE;
          MOID (&z) = MOID (i);
          PUSH_PROCEDURE (p, z);
        } else {
          (*(PROCEDURE (i))) (p);
        }
        push_mode (f, MOID (i));
        return;
      }
    }
    monitor_error ("cannot find identifier", sym);
  }
}

//! @brief Coerce arguments in a call.

static void coerce_arguments (FILE_T f, NODE_T * p, MOID_T * proc, int bot, int top, int top_sp)
{
  int k;
  PACK_T *u;
  ADDR_T pop_sp = top_sp;
  (void) f;
  if ((top - bot) != DIM (proc)) {
    monitor_error ("invalid procedure argument count", NO_TEXT);
  }
  QUIT_ON_ERROR;
  for (k = bot, u = PACK (proc); k < top; k++, FORWARD (u)) {
    if (A68_MON (_m_stack)[k] == MOID (u)) {
      PUSH (p, STACK_ADDRESS (pop_sp), SIZE (MOID (u)));
      pop_sp += SIZE (MOID (u));
    } else if (IS_REF (A68_MON (_m_stack)[k])) {
      A68_REF *v = (A68_REF *) STACK_ADDRESS (pop_sp);
      PUSH_REF (p, *v);
      pop_sp += A68_REF_SIZE;
      deref (p, k, STRONG);
      if (A68_MON (_m_stack)[k] != MOID (u)) {
        ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s to %s", moid_to_string (A68_MON (_m_stack)[k], MOID_WIDTH, NO_NODE), moid_to_string (MOID (u), MOID_WIDTH, NO_NODE)) >= 0);
        monitor_error ("invalid argument mode", A68 (edit_line));
      }
    } else {
      ASSERT (snprintf (A68 (edit_line), SNPRINTF_SIZE, "%s to %s", moid_to_string (A68_MON (_m_stack)[k], MOID_WIDTH, NO_NODE), moid_to_string (MOID (u), MOID_WIDTH, NO_NODE)) >= 0);
      monitor_error ("cannot coerce argument", A68 (edit_line));
    }
    QUIT_ON_ERROR;
  }
  MOVE (STACK_ADDRESS (top_sp), STACK_ADDRESS (pop_sp), A68_SP - pop_sp);
  A68_SP = top_sp + (A68_SP - pop_sp);
}

//! @brief Perform a selection.

static void selection (FILE_T f, NODE_T * p, char *field)
{
  BOOL_T name;
  MOID_T *moid;
  PACK_T *u, *v;
  SCAN_CHECK (f, p);
  if (A68_MON (attr) != IDENTIFIER && A68_MON (attr) != OPEN_SYMBOL) {
    monitor_error ("invalid selection syntax", NO_TEXT);
  }
  QUIT_ON_ERROR;
  PARSE_CHECK (f, p, MAX_PRIORITY + 1);
  deref (p, A68_MON (_m_sp) - 1, WEAK);
  if (IS_REF (TOP_MODE)) {
    name = A68_TRUE;
    u = PACK (NAME (TOP_MODE));
    moid = SUB (A68_MON (_m_stack)[--A68_MON (_m_sp)]);
    v = PACK (moid);
  } else {
    name = A68_FALSE;
    moid = A68_MON (_m_stack)[--A68_MON (_m_sp)];
    u = PACK (moid);
    v = PACK (moid);
  }
  if (!IS (moid, STRUCT_SYMBOL)) {
    monitor_error ("invalid selection mode", moid_to_string (moid, MOID_WIDTH, NO_NODE));
  }
  QUIT_ON_ERROR;
  for (; u != NO_PACK; FORWARD (u), FORWARD (v)) {
    if (strcmp (field, TEXT (u)) == 0) {
      if (name) {
        A68_REF *z = (A68_REF *) (STACK_OFFSET (-A68_REF_SIZE));
        CHECK_MON_REF (p, *z, moid);
        OFFSET (z) += OFFSET (v);
      } else {
        DECREMENT_STACK_POINTER (p, SIZE (moid));
        MOVE (STACK_TOP, STACK_OFFSET (OFFSET (v)), (unsigned) SIZE (MOID (u)));
        INCREMENT_STACK_POINTER (p, SIZE (MOID (u)));
      }
      push_mode (f, MOID (u));
      return;
    }
  }
  monitor_error ("invalid field name", field);
}

//! @brief Perform a call.

static void call (FILE_T f, NODE_T * p, int depth)
{
  A68_PROCEDURE z;
  NODE_T q;
  int args, old_m_sp;
  MOID_T *proc;
  (void) depth;
  QUIT_ON_ERROR;
  deref (p, A68_MON (_m_sp) - 1, STRONG);
  proc = A68_MON (_m_stack)[--A68_MON (_m_sp)];
  old_m_sp = A68_MON (_m_sp);
  if (!IS (proc, PROC_SYMBOL)) {
    monitor_error ("invalid procedure mode", moid_to_string (proc, MOID_WIDTH, NO_NODE));
  }
  QUIT_ON_ERROR;
  POP_PROCEDURE (p, &z);
  args = A68_MON (_m_sp);
  ADDR_T top_sp = A68_SP;
  if (A68_MON (attr) == OPEN_SYMBOL) {
    do {
      SCAN_CHECK (f, p);
      PARSE_CHECK (f, p, 0);
    } while (A68_MON (attr) == COMMA_SYMBOL);
    if (A68_MON (attr) != CLOSE_SYMBOL) {
      monitor_error ("unmatched parenthesis", NO_TEXT);
    }
    SCAN_CHECK (f, p);
  }
  coerce_arguments (f, p, proc, args, A68_MON (_m_sp), top_sp);
  if (STATUS (&z) & STANDENV_PROC_MASK) {
    MOID (&q) = A68_MON (_m_stack)[--A68_MON (_m_sp)];
    INFO (&q) = INFO (p);
    NSYMBOL (&q) = NSYMBOL (p);
    (void) ((*PROCEDURE (&(BODY (&z)))) (&q));
    A68_MON (_m_sp) = old_m_sp;
    push_mode (f, SUB_MOID (&z));
  } else {
    monitor_error ("can only call standard environ routines", NO_TEXT);
  }
}

//! @brief Perform a slice.

static void slice (FILE_T f, NODE_T * p, int depth)
{
  MOID_T *moid, *res;
  A68_REF z;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  ADDR_T address;
  int dim, k, iindex, args;
  BOOL_T name;
  (void) depth;
  QUIT_ON_ERROR;
  deref (p, A68_MON (_m_sp) - 1, WEAK);
  if (IS_REF (TOP_MODE)) {
    name = A68_TRUE;
    res = NAME (TOP_MODE);
    deref (p, A68_MON (_m_sp) - 1, STRONG);
    moid = A68_MON (_m_stack)[--A68_MON (_m_sp)];
  } else {
    name = A68_FALSE;
    moid = A68_MON (_m_stack)[--A68_MON (_m_sp)];
    res = SUB (moid);
  }
  if (!IS_ROW (moid) && !IS_FLEX (moid)) {
    monitor_error ("invalid row mode", moid_to_string (moid, MOID_WIDTH, NO_NODE));
  }
  QUIT_ON_ERROR;
// Get descriptor.
  POP_REF (p, &z);
  CHECK_MON_REF (p, z, moid);
  GET_DESCRIPTOR (arr, tup, &z);
  if (IS_FLEX (moid)) {
    dim = DIM (SUB (moid));
  } else {
    dim = DIM (moid);
  }
// Get iindexer.
  args = A68_MON (_m_sp);
  if (A68_MON (attr) == SUB_SYMBOL) {
    do {
      SCAN_CHECK (f, p);
      PARSE_CHECK (f, p, 0);
    } while (A68_MON (attr) == COMMA_SYMBOL);
    if (A68_MON (attr) != BUS_SYMBOL) {
      monitor_error ("unmatched parenthesis", NO_TEXT);
    }
    SCAN_CHECK (f, p);
  }
  if ((A68_MON (_m_sp) - args) != dim) {
    monitor_error ("invalid slice index count", NO_TEXT);
  }
  QUIT_ON_ERROR;
  for (k = 0, iindex = 0; k < dim; k++, A68_MON (_m_sp)--) {
    A68_TUPLE *t = &(tup[dim - k - 1]);
    A68_INT i;
    deref (p, A68_MON (_m_sp) - 1, MEEK);
    if (TOP_MODE != M_INT) {
      monitor_error ("invalid indexer mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
    }
    QUIT_ON_ERROR;
    POP_OBJECT (p, &i, A68_INT);
    if (VALUE (&i) < LOWER_BOUND (t) || VALUE (&i) > UPPER_BOUND (t)) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    QUIT_ON_ERROR;
    iindex += SPAN (t) * VALUE (&i) - SHIFT (t);
  }
  address = ROW_ELEMENT (arr, iindex);
  if (name) {
    z = ARRAY (arr);
    OFFSET (&z) += address;
    REF_SCOPE (&z) = PRIMAL_SCOPE;
    PUSH_REF (p, z);
  } else {
    PUSH (p, ADDRESS (&(ARRAY (arr))) + address, SIZE (res));
  }
  push_mode (f, res);
}

//! @brief Perform a call or a slice.

static void call_or_slice (FILE_T f, NODE_T * p, int depth)
{
  while (A68_MON (attr) == OPEN_SYMBOL || A68_MON (attr) == SUB_SYMBOL) {
    QUIT_ON_ERROR;
    if (A68_MON (attr) == OPEN_SYMBOL) {
      call (f, p, depth);
    } else if (A68_MON (attr) == SUB_SYMBOL) {
      slice (f, p, depth);
    }
  }
}

//! @brief Parse expression on input.

static void parse (FILE_T f, NODE_T * p, int depth)
{
  LOW_STACK_ALERT (p);
  QUIT_ON_ERROR;
  if (depth <= MAX_PRIORITY) {
    if (depth == 0) {
// Identity relations.
      PARSE_CHECK (f, p, 1);
      while (A68_MON (attr) == IS_SYMBOL || A68_MON (attr) == ISNT_SYMBOL) {
        A68_REF x, y;
        BOOL_T res;
        int op = A68_MON (attr);
        if (TOP_MODE != M_HIP && !IS_REF (TOP_MODE)) {
          monitor_error ("identity relation operand must yield a name", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
        }
        SCAN_CHECK (f, p);
        PARSE_CHECK (f, p, 1);
        if (TOP_MODE != M_HIP && !IS_REF (TOP_MODE)) {
          monitor_error ("identity relation operand must yield a name", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
        }
        QUIT_ON_ERROR;
        if (TOP_MODE != M_HIP && A68_MON (_m_stack)[A68_MON (_m_sp) - 2] != M_HIP) {
          if (TOP_MODE != A68_MON (_m_stack)[A68_MON (_m_sp) - 2]) {
            monitor_error ("invalid identity relation operand mode", NO_TEXT);
          }
        }
        QUIT_ON_ERROR;
        A68_MON (_m_sp) -= 2;
        POP_REF (p, &y);
        POP_REF (p, &x);
        res = (BOOL_T) (ADDRESS (&x) == ADDRESS (&y));
        PUSH_VALUE (p, (BOOL_T) (op == IS_SYMBOL ? res : !res), A68_BOOL);
        push_mode (f, M_BOOL);
      }
    } else {
// Dyadic expressions.
      PARSE_CHECK (f, p, depth + 1);
      while (A68_MON (attr) == OPERATOR && prio (f, p) == depth) {
        int args;
        NODE_T q;
        TAG_T *opt;
        char name[BUFFER_SIZE];
        bufcpy (name, A68_MON (symbol), BUFFER_SIZE);
        args = A68_MON (_m_sp) - 1;
        ADDR_T top_sp = A68_SP - SIZE (A68_MON (_m_stack)[args]);
        SCAN_CHECK (f, p);
        PARSE_CHECK (f, p, depth + 1);
        opt = search_operator (name, A68_MON (_m_stack)[A68_MON (_m_sp) - 2], TOP_MODE);
        QUIT_ON_ERROR;
        coerce_arguments (f, p, MOID (opt), args, A68_MON (_m_sp), top_sp);
        A68_MON (_m_sp) -= 2;
        MOID (&q) = MOID (opt);
        INFO (&q) = INFO (p);
        NSYMBOL (&q) = NSYMBOL (p);
        (void) ((*(PROCEDURE (opt)))) (&q);
        push_mode (f, SUB_MOID (opt));
      }
    }
  } else if (A68_MON (attr) == OPERATOR) {
    int args;
    NODE_T q;
    TAG_T *opt;
    char name[BUFFER_SIZE];
    bufcpy (name, A68_MON (symbol), BUFFER_SIZE);
    args = A68_MON (_m_sp);
    ADDR_T top_sp = A68_SP;
    SCAN_CHECK (f, p);
    PARSE_CHECK (f, p, depth);
    opt = search_operator (name, TOP_MODE, NO_MOID);
    QUIT_ON_ERROR;
    coerce_arguments (f, p, MOID (opt), args, A68_MON (_m_sp), top_sp);
    A68_MON (_m_sp)--;
    MOID (&q) = MOID (opt);
    INFO (&q) = INFO (p);
    NSYMBOL (&q) = NSYMBOL (p);
    (void) ((*(PROCEDURE (opt))) (&q));
    push_mode (f, SUB_MOID (opt));
  } else if (A68_MON (attr) == REF_SYMBOL) {
    int refs = 0, length = 0;
    MOID_T *m = NO_MOID;
    while (A68_MON (attr) == REF_SYMBOL) {
      refs++;
      SCAN_CHECK (f, p);
    }
    while (A68_MON (attr) == LONG_SYMBOL) {
      length++;
      SCAN_CHECK (f, p);
    }
    m = search_mode (refs, length, A68_MON (symbol));
    QUIT_ON_ERROR;
    if (m == NO_MOID) {
      monitor_error ("unknown reference to mode", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    if (A68_MON (attr) != OPEN_SYMBOL) {
      monitor_error ("cast expects open-symbol", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    PARSE_CHECK (f, p, 0);
    if (A68_MON (attr) != CLOSE_SYMBOL) {
      monitor_error ("cast expects close-symbol", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    while (IS_REF (TOP_MODE) && TOP_MODE != m) {
      MOID_T *sub = SUB (TOP_MODE);
      A68_REF z;
      POP_REF (p, &z);
      CHECK_MON_REF (p, z, TOP_MODE);
      PUSH (p, ADDRESS (&z), SIZE (sub));
      TOP_MODE = sub;
    }
    if (TOP_MODE != m) {
      monitor_error ("invalid cast mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
    }
  } else if (A68_MON (attr) == LONG_SYMBOL) {
    int length = 0;
    MOID_T *m;
    while (A68_MON (attr) == LONG_SYMBOL) {
      length++;
      SCAN_CHECK (f, p);
    }
// Cast L INT -> L REAL.
    if (A68_MON (attr) == REAL_SYMBOL) {
      MOID_T *i = (length == 1 ? M_LONG_INT : M_LONG_LONG_INT);
      MOID_T *r = (length == 1 ? M_LONG_REAL : M_LONG_LONG_REAL);
      SCAN_CHECK (f, p);
      if (A68_MON (attr) != OPEN_SYMBOL) {
        monitor_error ("cast expects open-symbol", NO_TEXT);
      }
      SCAN_CHECK (f, p);
      PARSE_CHECK (f, p, 0);
      if (A68_MON (attr) != CLOSE_SYMBOL) {
        monitor_error ("cast expects close-symbol", NO_TEXT);
      }
      SCAN_CHECK (f, p);
      if (TOP_MODE != i) {
        monitor_error ("invalid cast argument mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
      }
      QUIT_ON_ERROR;
      TOP_MODE = r;
      return;
    }
// L INT or L REAL denotation.
    if (A68_MON (attr) == INT_DENOTATION) {
      m = (length == 1 ? M_LONG_INT : M_LONG_LONG_INT);
    } else if (A68_MON (attr) == REAL_DENOTATION) {
      m = (length == 1 ? M_LONG_REAL : M_LONG_LONG_REAL);
    } else if (A68_MON (attr) == BITS_DENOTATION) {
      m = (length == 1 ? M_LONG_BITS : M_LONG_LONG_BITS);
    } else {
      m = NO_MOID;
    }
    if (m != NO_MOID) {
      int digits = DIGITS (m);
      MP_T *z = nil_mp (p, digits);
      if (genie_string_to_value_internal (p, m, A68_MON (symbol), (BYTE_T *) z) == A68_FALSE) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, m);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      MP_STATUS (z) = (MP_T) (INIT_MASK | CONSTANT_MASK);
      push_mode (f, m);
      SCAN_CHECK (f, p);
    } else {
      monitor_error ("invalid mode", NO_TEXT);
    }
  } else if (A68_MON (attr) == INT_DENOTATION) {
    A68_INT z;
    if (genie_string_to_value_internal (p, M_INT, A68_MON (symbol), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, M_INT);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PUSH_VALUE (p, VALUE (&z), A68_INT);
    push_mode (f, M_INT);
    SCAN_CHECK (f, p);
  } else if (A68_MON (attr) == REAL_DENOTATION) {
    A68_REAL z;
    if (genie_string_to_value_internal (p, M_REAL, A68_MON (symbol), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, M_REAL);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PUSH_VALUE (p, VALUE (&z), A68_REAL);
    push_mode (f, M_REAL);
    SCAN_CHECK (f, p);
  } else if (A68_MON (attr) == BITS_DENOTATION) {
    A68_BITS z;
    if (genie_string_to_value_internal (p, M_BITS, A68_MON (symbol), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, M_BITS);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PUSH_VALUE (p, VALUE (&z), A68_BITS);
    push_mode (f, M_BITS);
    SCAN_CHECK (f, p);
  } else if (A68_MON (attr) == ROW_CHAR_DENOTATION) {
    if (strlen (A68_MON (symbol)) == 1) {
      PUSH_VALUE (p, A68_MON (symbol)[0], A68_CHAR);
      push_mode (f, M_CHAR);
    } else {
      A68_REF z;
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      z = c_to_a_string (p, A68_MON (symbol), DEFAULT_WIDTH);
      GET_DESCRIPTOR (arr, tup, &z);
      BLOCK_GC_HANDLE (&z);
      BLOCK_GC_HANDLE (&(ARRAY (arr)));
      PUSH_REF (p, z);
      push_mode (f, M_STRING);
      (void) tup;
    }
    SCAN_CHECK (f, p);
  } else if (A68_MON (attr) == TRUE_SYMBOL) {
    PUSH_VALUE (p, A68_TRUE, A68_BOOL);
    push_mode (f, M_BOOL);
    SCAN_CHECK (f, p);
  } else if (A68_MON (attr) == FALSE_SYMBOL) {
    PUSH_VALUE (p, A68_FALSE, A68_BOOL);
    push_mode (f, M_BOOL);
    SCAN_CHECK (f, p);
  } else if (A68_MON (attr) == NIL_SYMBOL) {
    PUSH_REF (p, nil_ref);
    push_mode (f, M_HIP);
    SCAN_CHECK (f, p);
  } else if (A68_MON (attr) == REAL_SYMBOL) {
    A68_INT k;
    SCAN_CHECK (f, p);
    if (A68_MON (attr) != OPEN_SYMBOL) {
      monitor_error ("cast expects open-symbol", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    PARSE_CHECK (f, p, 0);
    if (A68_MON (attr) != CLOSE_SYMBOL) {
      monitor_error ("cast expects close-symbol", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    if (TOP_MODE != M_INT) {
      monitor_error ("invalid cast argument mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
    }
    QUIT_ON_ERROR;
    POP_OBJECT (p, &k, A68_INT);
    PUSH_VALUE (p, (REAL_T) VALUE (&k), A68_REAL);
    TOP_MODE = M_REAL;
  } else if (A68_MON (attr) == IDENTIFIER) {
    ADDR_T old_sp = A68_SP;
    BOOL_T init;
    MOID_T *moid;
    char name[BUFFER_SIZE];
    bufcpy (name, A68_MON (symbol), BUFFER_SIZE);
    SCAN_CHECK (f, p);
    if (A68_MON (attr) == OF_SYMBOL) {
      selection (f, p, name);
    } else {
      search_identifier (f, p, A68_FP, name);
      QUIT_ON_ERROR;
      call_or_slice (f, p, depth);
    }
    moid = TOP_MODE;
    QUIT_ON_ERROR;
    if (check_initialisation (p, STACK_ADDRESS (old_sp), moid, &init)) {
      if (init == A68_FALSE) {
        monitor_error (NO_VALUE, name);
      }
    } else {
      monitor_error ("cannot process value of mode", moid_to_string (moid, MOID_WIDTH, NO_NODE));
    }
  } else if (A68_MON (attr) == OPEN_SYMBOL) {
    do {
      SCAN_CHECK (f, p);
      PARSE_CHECK (f, p, 0);
    } while (A68_MON (attr) == COMMA_SYMBOL);
    if (A68_MON (attr) != CLOSE_SYMBOL) {
      monitor_error ("unmatched parenthesis", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    call_or_slice (f, p, depth);
  } else {
    monitor_error ("invalid expression syntax", NO_TEXT);
  }
}

//! @brief Perform assignment.

static void assign (FILE_T f, NODE_T * p)
{
  LOW_STACK_ALERT (p);
  PARSE_CHECK (f, p, 0);
  if (A68_MON (attr) == ASSIGN_SYMBOL) {
    MOID_T *m = A68_MON (_m_stack)[--A68_MON (_m_sp)];
    A68_REF z;
    if (!IS_REF (m)) {
      monitor_error ("invalid destination mode", moid_to_string (m, MOID_WIDTH, NO_NODE));
    }
    QUIT_ON_ERROR;
    POP_REF (p, &z);
    CHECK_MON_REF (p, z, m);
    SCAN_CHECK (f, p);
    assign (f, p);
    QUIT_ON_ERROR;
    while (IS_REF (TOP_MODE) && TOP_MODE != SUB (m)) {
      MOID_T *sub = SUB (TOP_MODE);
      A68_REF y;
      POP_REF (p, &y);
      CHECK_MON_REF (p, y, TOP_MODE);
      PUSH (p, ADDRESS (&y), SIZE (sub));
      TOP_MODE = sub;
    }
    if (TOP_MODE != SUB (m) && TOP_MODE != M_HIP) {
      monitor_error ("invalid source mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
    }
    QUIT_ON_ERROR;
    POP (p, ADDRESS (&z), SIZE (TOP_MODE));
    PUSH_REF (p, z);
    TOP_MODE = m;
  }
}

//! @brief Evaluate expression on input.

static void evaluate (FILE_T f, NODE_T * p, char *str)
{
  LOW_STACK_ALERT (p);
  A68_MON (_m_sp) = 0;
  A68_MON (_m_stack)[0] = NO_MOID;
  A68_MON (pos) = 0;
  bufcpy (A68_MON (expr), str, BUFFER_SIZE);
  SCAN_CHECK (f, p);
  QUIT_ON_ERROR;
  assign (f, p);
  if (A68_MON (attr) != 0) {
    monitor_error ("trailing character in expression", A68_MON (symbol));
  }
}

//! @brief Convert string to int.

static int get_num_arg (char *num, char **rest)
{
  char *end;
  int k;
  if (rest != NO_VAR) {
    *rest = NO_TEXT;
  }
  if (num == NO_TEXT) {
    return NOT_A_NUM;
  }
  SKIP_ONE_SYMBOL (num);
  if (IS_DIGIT (num[0])) {
    errno = 0;
    k = (int) a68_strtou (num, &end, 10);
    if (end != num && errno == 0) {
      if (rest != NO_VAR) {
        *rest = end;
      }
      return k;
    } else {
      monitor_error ("invalid numerical argument", error_specification ());
      return NOT_A_NUM;
    }
  } else {
    if (num[0] != NULL_CHAR) {
      monitor_error ("invalid numerical argument", num);
    }
    return NOT_A_NUM;
  }
}

//! @brief Whether item at "w" of mode "q" is initialised.

static BOOL_T check_initialisation (NODE_T * p, BYTE_T * w, MOID_T * q, BOOL_T * result)
{
  BOOL_T initialised = A68_FALSE, recognised = A68_FALSE;
  (void) p;
  switch (SHORT_ID (q)) {
  case MODE_NO_CHECK:
  case UNION_SYMBOL:
    {
      initialised = A68_TRUE;
      recognised = A68_TRUE;
      break;
    }
  case REF_SYMBOL:
    {
      A68_REF *z = (A68_REF *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case PROC_SYMBOL:
    {
      A68_PROCEDURE *z = (A68_PROCEDURE *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_INT:
    {
      A68_INT *z = (A68_INT *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_REAL:
    {
      A68_REAL *z = (A68_REAL *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_COMPLEX:
    {
      A68_REAL *r = (A68_REAL *) w;
      A68_REAL *i = (A68_REAL *) (w + SIZE_ALIGNED (A68_REAL));
      initialised = (BOOL_T) (INITIALISED (r) && INITIALISED (i));
      recognised = A68_TRUE;
      break;
    }
#if (A68_LEVEL >= 3)
  case MODE_LONG_INT:
  case MODE_LONG_BITS:
    {
      A68_LONG_INT *z = (A68_LONG_INT *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_LONG_REAL:
    {
      A68_LONG_REAL *z = (A68_LONG_REAL *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
#else
  case MODE_LONG_INT:
  case MODE_LONG_REAL:
  case MODE_LONG_BITS:
    {
      MP_T *z = (MP_T *) w;
      initialised = (BOOL_T) ((unsigned) MP_STATUS (z) & INIT_MASK);
      recognised = A68_TRUE;
      break;
    }
#endif
  case MODE_LONG_LONG_INT:
  case MODE_LONG_LONG_REAL:
  case MODE_LONG_LONG_BITS:
    {
      MP_T *z = (MP_T *) w;
      initialised = (BOOL_T) ((unsigned) MP_STATUS (z) & INIT_MASK);
      recognised = A68_TRUE;
      break;
    }
  case MODE_LONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_mp ());
      initialised = (BOOL_T) (((unsigned) MP_STATUS (r) & INIT_MASK) && ((unsigned) MP_STATUS (i) & INIT_MASK));
      recognised = A68_TRUE;
      break;
    }
  case MODE_LONG_LONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_mp ());
      initialised = (BOOL_T) (((unsigned) MP_STATUS (r) & INIT_MASK) && ((unsigned) MP_STATUS (i) & INIT_MASK));
      recognised = A68_TRUE;
      break;
    }
  case MODE_BOOL:
    {
      A68_BOOL *z = (A68_BOOL *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_CHAR:
    {
      A68_CHAR *z = (A68_CHAR *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_BITS:
    {
      A68_BITS *z = (A68_BITS *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_BYTES:
    {
      A68_BYTES *z = (A68_BYTES *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_LONG_BYTES:
    {
      A68_LONG_BYTES *z = (A68_LONG_BYTES *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_FILE:
    {
      A68_FILE *z = (A68_FILE *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_FORMAT:
    {
      A68_FORMAT *z = (A68_FORMAT *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
      break;
    }
  case MODE_PIPE:
    {
      A68_REF *pipe_read = (A68_REF *) w;
      A68_REF *pipe_write = (A68_REF *) (w + A68_REF_SIZE);
      A68_INT *pid = (A68_INT *) (w + 2 * A68_REF_SIZE);
      initialised = (BOOL_T) (INITIALISED (pipe_read) && INITIALISED (pipe_write) && INITIALISED (pid));
      recognised = A68_TRUE;
      break;
    }
  case MODE_SOUND:
    {
      A68_SOUND *z = (A68_SOUND *) w;
      initialised = INITIALISED (z);
      recognised = A68_TRUE;
    }
  }
  if (result != NO_BOOL) {
    *result = initialised;
  }
  return recognised;
}

//! @brief Show value of object.

void print_item (NODE_T * p, FILE_T f, BYTE_T * item, MOID_T * mode)
{
  A68_REF nil_file = nil_ref;
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, mode, item, nil_file);
  if (get_transput_buffer_index (UNFORMATTED_BUFFER) > 0) {
    if (mode == M_CHAR || mode == M_ROW_CHAR || mode == M_STRING) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " \"%s\"", get_transput_buffer (UNFORMATTED_BUFFER)) >= 0);
      WRITE (f, A68 (output_line));
    } else {
      char *str = get_transput_buffer (UNFORMATTED_BUFFER);
      while (IS_SPACE (str[0])) {
        str++;
      }
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " %s", str) >= 0);
      WRITE (f, A68 (output_line));
    }
  } else {
    WRITE (f, CANNOT_SHOW);
  }
}

//! @brief Indented indent_crlf.

static void indent_crlf (FILE_T f)
{
  int k;
  io_close_tty_line ();
  for (k = 0; k < A68_MON (tabs); k++) {
    WRITE (f, "  ");
  }
}

//! @brief Show value of object.

static void show_item (FILE_T f, NODE_T * p, BYTE_T * item, MOID_T * mode)
{
  if (item == NO_BYTE || mode == NO_MOID) {
    return;
  }
  if (IS_REF (mode)) {
    A68_REF *z = (A68_REF *) item;
    if (IS_NIL (*z)) {
      if (INITIALISED (z)) {
        WRITE (STDOUT_FILENO, " = NIL");
      } else {
        WRITE (STDOUT_FILENO, NO_VALUE);
      }
    } else {
      if (INITIALISED (z)) {
        WRITE (STDOUT_FILENO, " refers to ");
        if (IS_IN_HEAP (z)) {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "heap(%p)", (void *) ADDRESS (z)) >= 0);
          WRITE (STDOUT_FILENO, A68 (output_line));
          A68_MON (tabs)++;
          show_item (f, p, ADDRESS (z), SUB (mode));
          A68_MON (tabs)--;
        } else if (IS_IN_FRAME (z)) {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "frame(" A68_LU ")", REF_OFFSET (z)) >= 0);
          WRITE (STDOUT_FILENO, A68 (output_line));
        } else if (IS_IN_STACK (z)) {
          ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "stack(" A68_LU ")", REF_OFFSET (z)) >= 0);
          WRITE (STDOUT_FILENO, A68 (output_line));
        }
      } else {
        WRITE (STDOUT_FILENO, NO_VALUE);
      }
    }
  } else if (mode == M_STRING) {
    if (!INITIALISED ((A68_REF *) item)) {
      WRITE (STDOUT_FILENO, NO_VALUE);
    } else {
      print_item (p, f, item, mode);
    }
  } else if ((IS_ROW (mode) || IS_FLEX (mode)) && mode != M_STRING) {
    MOID_T *deflexed = DEFLEX (mode);
    int old_tabs = A68_MON (tabs);
    A68_MON (tabs) += 2;
    if (!INITIALISED ((A68_REF *) item)) {
      WRITE (STDOUT_FILENO, NO_VALUE);
    } else {
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      int count = 0, act_count = 0, elems;
      GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
      elems = get_row_size (tup, DIM (arr));
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, ", %d element(s)", elems) >= 0);
      WRITE (f, A68 (output_line));
      if (get_row_size (tup, DIM (arr)) != 0) {
        BYTE_T *base_addr = ADDRESS (&ARRAY (arr));
        BOOL_T done = A68_FALSE;
        initialise_internal_index (tup, DIM (arr));
        while (!done && ++count <= (A68_MON (max_row_elems) + 1)) {
          if (count <= A68_MON (max_row_elems)) {
            ADDR_T row_index = calculate_internal_index (tup, DIM (arr));
            ADDR_T elem_addr = ROW_ELEMENT (arr, row_index);
            BYTE_T *elem = &base_addr[elem_addr];
            indent_crlf (f);
            WRITE (f, "[");
            print_internal_index (f, tup, DIM (arr));
            WRITE (f, "]");
            show_item (f, p, elem, SUB (deflexed));
            act_count++;
            done = increment_internal_index (tup, DIM (arr));
          }
        }
        indent_crlf (f);
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " %d element(s) written (%d%%)", act_count, (int) ((100.0 * act_count) / elems)) >= 0);
        WRITE (f, A68 (output_line));
      }
    }
    A68_MON (tabs) = old_tabs;
  } else if (IS_STRUCT (mode)) {
    PACK_T *q = PACK (mode);
    A68_MON (tabs)++;
    for (; q != NO_PACK; FORWARD (q)) {
      BYTE_T *elem = &item[OFFSET (q)];
      indent_crlf (f);
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "     %s \"%s\"", moid_to_string (MOID (q), MOID_WIDTH, NO_NODE), TEXT (q)) >= 0);
      WRITE (STDOUT_FILENO, A68 (output_line));
      show_item (f, p, elem, MOID (q));
    }
    A68_MON (tabs)--;
  } else if (IS (mode, UNION_SYMBOL)) {
    A68_UNION *z = (A68_UNION *) item;
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " united-moid %s", moid_to_string ((MOID_T *) (VALUE (z)), MOID_WIDTH, NO_NODE)) >= 0);
    WRITE (STDOUT_FILENO, A68 (output_line));
    show_item (f, p, &item[SIZE_ALIGNED (A68_UNION)], (MOID_T *) (VALUE (z)));
  } else if (mode == M_SIMPLIN) {
    A68_UNION *z = (A68_UNION *) item;
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " united-moid %s", moid_to_string ((MOID_T *) (VALUE (z)), MOID_WIDTH, NO_NODE)) >= 0);
    WRITE (STDOUT_FILENO, A68 (output_line));
  } else if (mode == M_SIMPLOUT) {
    A68_UNION *z = (A68_UNION *) item;
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " united-moid %s", moid_to_string ((MOID_T *) (VALUE (z)), MOID_WIDTH, NO_NODE)) >= 0);
    WRITE (STDOUT_FILENO, A68 (output_line));
  } else {
    BOOL_T init;
    if (check_initialisation (p, item, mode, &init)) {
      if (init) {
        if (IS (mode, PROC_SYMBOL)) {
          A68_PROCEDURE *z = (A68_PROCEDURE *) item;
          if (z != NO_PROCEDURE && STATUS (z) & STANDENV_PROC_MASK) {
            char *fname = standard_environ_proc_name (*(PROCEDURE (&BODY (z))));
            WRITE (STDOUT_FILENO, " standenv procedure");
            if (fname != NO_TEXT) {
              WRITE (STDOUT_FILENO, " (");
              WRITE (STDOUT_FILENO, fname);
              WRITE (STDOUT_FILENO, ")");
            }
          } else if (z != NO_PROCEDURE && STATUS (z) & SKIP_PROCEDURE_MASK) {
            WRITE (STDOUT_FILENO, " skip procedure");
          } else if (z != NO_PROCEDURE && (PROCEDURE (&BODY (z))) != NO_GPROC) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " line %d, environ at frame(" A68_LU "), locale %p", LINE_NUMBER ((NODE_T *) NODE (&BODY (z))), ENVIRON (z), (void *) LOCALE (z)) >= 0);
            WRITE (STDOUT_FILENO, A68 (output_line));
          } else {
            WRITE (STDOUT_FILENO, " cannot show value");
          }
        } else if (mode == M_FORMAT) {
          A68_FORMAT *z = (A68_FORMAT *) item;
          if (z != NO_FORMAT && BODY (z) != NO_NODE) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " line %d, environ at frame(" A68_LU ")", LINE_NUMBER (BODY (z)), ENVIRON (z)) >= 0);
            WRITE (STDOUT_FILENO, A68 (output_line));
          } else {
            monitor_error (CANNOT_SHOW, NO_TEXT);
          }
        } else if (mode == M_SOUND) {
          A68_SOUND *z = (A68_SOUND *) item;
          if (z != NO_SOUND) {
            ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%u channels, %u bits, %u rate, %u samples", NUM_CHANNELS (z), BITS_PER_SAMPLE (z), SAMPLE_RATE (z), NUM_SAMPLES (z)) >= 0);
            WRITE (STDOUT_FILENO, A68 (output_line));

          } else {
            monitor_error (CANNOT_SHOW, NO_TEXT);
          }
        } else {
          print_item (p, f, item, mode);
        }
      } else {
        WRITE (STDOUT_FILENO, NO_VALUE);
      }
    } else {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, " mode %s, %s", moid_to_string (mode, MOID_WIDTH, NO_NODE), CANNOT_SHOW) >= 0);
      WRITE (STDOUT_FILENO, A68 (output_line));
    }
  }
}

//! @brief Overview of frame item.

static void show_frame_item (FILE_T f, NODE_T * p, ADDR_T a68_link, TAG_T * q, int modif)
{
  ADDR_T addr = a68_link + FRAME_INFO_SIZE + OFFSET (q);
  ADDR_T loc = FRAME_INFO_SIZE + OFFSET (q);
  (void) p;
  indent_crlf (STDOUT_FILENO);
  if (modif != ANONYMOUS) {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "     frame(" A68_LU "=" A68_LU "+" A68_LU ") %s \"%s\"", addr, a68_link, loc, moid_to_string (MOID (q), MOID_WIDTH, NO_NODE), NSYMBOL (NODE (q))) >= 0);
    WRITE (STDOUT_FILENO, A68 (output_line));
    show_item (f, p, FRAME_ADDRESS (addr), MOID (q));
  } else {
    switch (PRIO (q)) {
    case GENERATOR:
      {
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "     frame(" A68_LU "=" A68_LU "+" A68_LU ") LOC %s", addr, a68_link, loc, moid_to_string (MOID (q), MOID_WIDTH, NO_NODE)) >= 0);
        WRITE (STDOUT_FILENO, A68 (output_line));
        break;
      }
    default:
      {
        ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "     frame(" A68_LU "=" A68_LU "+" A68_LU ") internal %s", addr, a68_link, loc, moid_to_string (MOID (q), MOID_WIDTH, NO_NODE)) >= 0);
        WRITE (STDOUT_FILENO, A68 (output_line));
        break;
      }
    }
    show_item (f, p, FRAME_ADDRESS (addr), MOID (q));
  }
}

//! @brief Overview of frame items.

static void show_frame_items (FILE_T f, NODE_T * p, ADDR_T a68_link, TAG_T * q, int modif)
{
  (void) p;
  for (; q != NO_TAG; FORWARD (q)) {
    show_frame_item (f, p, a68_link, q, modif);
  }
}

//! @brief Introduce stack frame.

static void intro_frame (FILE_T f, NODE_T * p, ADDR_T a68_link, int *printed)
{
  TABLE_T *q = TABLE (p);
  if (*printed > 0) {
    WRITELN (f, "");
  }
  (*printed)++;
  where_in_source (f, p);
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Stack frame %d at frame(" A68_LU "), level=%d, size=" A68_LU " bytes", FRAME_NUMBER (a68_link), a68_link, LEVEL (q), (UNSIGNED_T) (FRAME_INCREMENT (a68_link) + FRAME_INFO_SIZE)) >= 0);
  WRITELN (f, A68 (output_line));
}

//! @brief View contents of stack frame.

static void show_stack_frame (FILE_T f, NODE_T * p, ADDR_T a68_link, int *printed)
{
// show the frame starting at frame pointer 'a68_link', using symbol table from p as a map.
  if (p != NO_NODE) {
    TABLE_T *q = TABLE (p);
    intro_frame (f, p, a68_link, printed);
#if (A68_LEVEL >= 3)
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Dynamic link=frame(%llu), static link=frame(%llu), parameters=frame(%llu)", FRAME_DYNAMIC_LINK (a68_link), FRAME_STATIC_LINK (a68_link), FRAME_PARAMETERS (a68_link)) >= 0);
    WRITELN (STDOUT_FILENO, A68 (output_line));
#else
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Dynamic link=frame(%u), static link=frame(%u), parameters=frame(%u)", FRAME_DYNAMIC_LINK (a68_link), FRAME_STATIC_LINK (a68_link), FRAME_PARAMETERS (a68_link)) >= 0);
    WRITELN (STDOUT_FILENO, A68 (output_line));
#endif
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Procedure frame=%s", (FRAME_PROC_FRAME (a68_link) ? "yes" : "no")) >= 0);
    WRITELN (STDOUT_FILENO, A68 (output_line));
#if defined (BUILD_PARALLEL_CLAUSE)
    if (pthread_equal (FRAME_THREAD_ID (a68_link), A68_PAR (main_thread_id)) != 0) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "In main thread") >= 0);
    } else {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Not in main thread") >= 0);
    }
    WRITELN (STDOUT_FILENO, A68 (output_line));
#endif
    show_frame_items (f, p, a68_link, IDENTIFIERS (q), IDENTIFIER);
    show_frame_items (f, p, a68_link, OPERATORS (q), OPERATOR);
    show_frame_items (f, p, a68_link, ANONYMOUS (q), ANONYMOUS);
  }
}

//! @brief Shows lines around the line where 'p' is at.

static void list (FILE_T f, NODE_T * p, int n, int m)
{
  if (p != NO_NODE) {
    if (m == 0) {
      LINE_T *r = LINE (INFO (p));
      LINE_T *l = TOP_LINE (&A68_JOB);
      for (; l != NO_LINE; FORWARD (l)) {
        if (NUMBER (l) > 0 && abs (NUMBER (r) - NUMBER (l)) <= n) {
          write_source_line (f, l, NO_NODE, A68_TRUE);
        }
      }
    } else {
      LINE_T *l = TOP_LINE (&A68_JOB);
      for (; l != NO_LINE; FORWARD (l)) {
        if (NUMBER (l) > 0 && NUMBER (l) >= n && NUMBER (l) <= m) {
          write_source_line (f, l, NO_NODE, A68_TRUE);
        }
      }
    }
  }
}

//! @brief Overview of the heap.

void show_heap (FILE_T f, NODE_T * p, A68_HANDLE * z, int top, int n)
{
  int k = 0, m = n, sum = 0;
  (void) p;
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "size=" A68_LU " available=" A68_LU " garbage collections=%d", A68 (heap_size), heap_available (), A68_GC (sweeps)) >= 0);
  WRITELN (f, A68 (output_line));
  for (; z != NO_HANDLE; FORWARD (z), k++) {
    if (n > 0 && sum <= top) {
      n--;
      indent_crlf (f);
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "heap(%p+%d) %s", (void *) POINTER (z), SIZE (z), moid_to_string (MOID (z), MOID_WIDTH, NO_NODE)) >= 0);
      WRITE (f, A68 (output_line));
      sum += SIZE (z);
    }
  }
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "printed %d out of %d handles", m, k) >= 0);
  WRITELN (f, A68 (output_line));
}

//! @brief Search current frame and print it.

void stack_dump_current (FILE_T f, ADDR_T a68_link)
{
  if (a68_link > 0) {
    int dynamic_a68_link = FRAME_DYNAMIC_LINK (a68_link);
    NODE_T *p = FRAME_TREE (a68_link);
    if (p != NO_NODE && LEVEL (TABLE (p)) > 3) {
      if (FRAME_NUMBER (a68_link) == A68_MON (current_frame)) {
        int printed = 0;
        show_stack_frame (f, p, a68_link, &printed);
      } else {
        stack_dump_current (f, dynamic_a68_link);
      }
    }
  }
}

//! @brief Overview of the stack.

void stack_a68_link_dump (FILE_T f, ADDR_T a68_link, int depth, int *printed)
{
  if (depth > 0 && a68_link > 0) {
    NODE_T *p = FRAME_TREE (a68_link);
    if (p != NO_NODE && LEVEL (TABLE (p)) > 3) {
      show_stack_frame (f, p, a68_link, printed);
      stack_a68_link_dump (f, FRAME_STATIC_LINK (a68_link), depth - 1, printed);
    }
  }
}

//! @brief Overview of the stack.

void stack_dump (FILE_T f, ADDR_T a68_link, int depth, int *printed)
{
  if (depth > 0 && a68_link > 0) {
    NODE_T *p = FRAME_TREE (a68_link);
    if (p != NO_NODE && LEVEL (TABLE (p)) > 3) {
      show_stack_frame (f, p, a68_link, printed);
      stack_dump (f, FRAME_DYNAMIC_LINK (a68_link), depth - 1, printed);
    }
  }
}

//! @brief Overview of the stack.

void stack_trace (FILE_T f, ADDR_T a68_link, int depth, int *printed)
{
  if (depth > 0 && a68_link > 0) {
    int dynamic_a68_link = FRAME_DYNAMIC_LINK (a68_link);
    if (FRAME_PROC_FRAME (a68_link)) {
      NODE_T *p = FRAME_TREE (a68_link);
      show_stack_frame (f, p, a68_link, printed);
      stack_trace (f, dynamic_a68_link, depth - 1, printed);
    } else {
      stack_trace (f, dynamic_a68_link, depth, printed);
    }
  }
}

//! @brief Examine tags.

void examine_tags (FILE_T f, NODE_T * p, ADDR_T a68_link, TAG_T * q, char *sym, int *printed)
{
  for (; q != NO_TAG; FORWARD (q)) {
    if (NODE (q) != NO_NODE && strcmp (NSYMBOL (NODE (q)), sym) == 0) {
      intro_frame (f, p, a68_link, printed);
      show_frame_item (f, p, a68_link, q, PRIO (q));
    }
  }
}

//! @brief Search symbol in stack.

void examine_stack (FILE_T f, ADDR_T a68_link, char *sym, int *printed)
{
  if (a68_link > 0) {
    int dynamic_a68_link = FRAME_DYNAMIC_LINK (a68_link);
    NODE_T *p = FRAME_TREE (a68_link);
    if (p != NO_NODE) {
      TABLE_T *q = TABLE (p);
      examine_tags (f, p, a68_link, IDENTIFIERS (q), sym, printed);
      examine_tags (f, p, a68_link, OPERATORS (q), sym, printed);
    }
    examine_stack (f, dynamic_a68_link, sym, printed);
  }
}

//! @brief Set or reset breakpoints.

void change_breakpoints (NODE_T * p, unsigned set, int num, BOOL_T * is_set, char *loc_expr)
{
  for (; p != NO_NODE; FORWARD (p)) {
    change_breakpoints (SUB (p), set, num, is_set, loc_expr);
    if (set == BREAKPOINT_MASK) {
      if (LINE_NUMBER (p) == num && (STATUS_TEST (p, INTERRUPTIBLE_MASK)) && num != 0) {
        STATUS_SET (p, BREAKPOINT_MASK);
        if (EXPR (INFO (p)) != NO_TEXT) {
          a68_free (EXPR (INFO (p)));
        }
        EXPR (INFO (p)) = loc_expr;
        *is_set = A68_TRUE;
      }
    } else if (set == BREAKPOINT_TEMPORARY_MASK) {
      if (LINE_NUMBER (p) == num && (STATUS_TEST (p, INTERRUPTIBLE_MASK)) && num != 0) {
        STATUS_SET (p, BREAKPOINT_TEMPORARY_MASK);
        if (EXPR (INFO (p)) != NO_TEXT) {
          a68_free (EXPR (INFO (p)));
        }
        EXPR (INFO (p)) = loc_expr;
        *is_set = A68_TRUE;
      }
    } else if (set == NULL_MASK) {
      if (LINE_NUMBER (p) != num) {
        STATUS_CLEAR (p, (BREAKPOINT_MASK | BREAKPOINT_TEMPORARY_MASK));
        if (EXPR (INFO (p)) == NO_TEXT) {
          a68_free (EXPR (INFO (p)));
        }
        EXPR (INFO (p)) = NO_TEXT;
      } else if (num == 0) {
        STATUS_CLEAR (p, (BREAKPOINT_MASK | BREAKPOINT_TEMPORARY_MASK));
        if (EXPR (INFO (p)) != NO_TEXT) {
          a68_free (EXPR (INFO (p)));
        }
        EXPR (INFO (p)) = NO_TEXT;
      }
    }
  }
}

//! @brief List breakpoints.

static void list_breakpoints (NODE_T * p, int *listed)
{
  for (; p != NO_NODE; FORWARD (p)) {
    list_breakpoints (SUB (p), listed);
    if (STATUS_TEST (p, BREAKPOINT_MASK)) {
      (*listed)++;
      WIS (p);
      if (EXPR (INFO (p)) != NO_TEXT) {
        WRITELN (STDOUT_FILENO, "breakpoint condition \"");
        WRITE (STDOUT_FILENO, EXPR (INFO (p)));
        WRITE (STDOUT_FILENO, "\"");
      }
    }
  }
}

//! @brief Execute monitor command.

static BOOL_T single_stepper (NODE_T * p, char *cmd)
{
  A68_MON (mon_errors) = 0;
  errno = 0;
  if (strlen (cmd) == 0) {
    return A68_FALSE;
  }
  while (IS_SPACE (cmd[strlen (cmd) - 1])) {
    cmd[strlen (cmd) - 1] = NULL_CHAR;
  }
  if (match_string (cmd, "CAlls", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    int printed = 0;
    if (k > 0) {
      stack_trace (STDOUT_FILENO, A68_FP, k, &printed);
    } else if (k == 0) {
      stack_trace (STDOUT_FILENO, A68_FP, 3, &printed);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "Continue", NULL_CHAR) || match_string (cmd, "Resume", NULL_CHAR)) {
    A68 (do_confirm_exit) = A68_TRUE;
    return A68_TRUE;
  } else if (match_string (cmd, "DO", BLANK_CHAR) || match_string (cmd, "EXEC", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] != NULL_CHAR) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "return code %d", system (sym)) >= 0);
      WRITELN (STDOUT_FILENO, A68 (output_line));
    }
    return A68_FALSE;
  } else if (match_string (cmd, "ELems", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    if (k > 0) {
      A68_MON (max_row_elems) = k;
    }
    return A68_FALSE;
  } else if (match_string (cmd, "Evaluate", BLANK_CHAR) || match_string (cmd, "X", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] != NULL_CHAR) {
      ADDR_T old_sp = A68_SP;
      evaluate (STDOUT_FILENO, p, sym);
      if (A68_MON (mon_errors) == 0 && A68_MON (_m_sp) > 0) {
        MOID_T *res;
        BOOL_T cont = A68_TRUE;
        while (cont) {
          res = A68_MON (_m_stack)[0];
          WRITELN (STDOUT_FILENO, "(");
          WRITE (STDOUT_FILENO, moid_to_string (res, MOID_WIDTH, NO_NODE));
          WRITE (STDOUT_FILENO, ")");
          show_item (STDOUT_FILENO, p, STACK_ADDRESS (old_sp), res);
          cont = (BOOL_T) (IS_REF (res) && !IS_NIL (*(A68_REF *) STACK_ADDRESS (old_sp)));
          if (cont) {
            A68_REF z;
            POP_REF (p, &z);
            A68_MON (_m_stack)[0] = SUB (A68_MON (_m_stack)[0]);
            PUSH (p, ADDRESS (&z), SIZE (A68_MON (_m_stack)[0]));
          }
        }
      } else {
        monitor_error (CANNOT_SHOW, NO_TEXT);
      }
      A68_SP = old_sp;
      A68_MON (_m_sp) = 0;
    }
    return A68_FALSE;
  } else if (match_string (cmd, "EXamine", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] != NULL_CHAR && (IS_LOWER (sym[0]) || IS_UPPER (sym[0]))) {
      int printed = 0;
      examine_stack (STDOUT_FILENO, A68_FP, sym, &printed);
      if (printed == 0) {
        monitor_error ("tag not found", sym);
      }
    } else {
      monitor_error ("tag expected", NO_TEXT);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "EXIt", NULL_CHAR) || match_string (cmd, "HX", NULL_CHAR) || match_string (cmd, "Quit", NULL_CHAR) || strcmp (cmd, LOGOUT_STRING) == 0) {
    if (confirm_exit ()) {
      exit_genie (p, A68_RUNTIME_ERROR + A68_FORCE_QUIT);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "Frame", NULL_CHAR)) {
    if (A68_MON (current_frame) == 0) {
      int printed = 0;
      stack_dump (STDOUT_FILENO, A68_FP, 1, &printed);
    } else {
      stack_dump_current (STDOUT_FILENO, A68_FP);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "Frame", BLANK_CHAR)) {
    int n = get_num_arg (cmd, NO_VAR);
    A68_MON (current_frame) = (n > 0 ? n : 0);
    stack_dump_current (STDOUT_FILENO, A68_FP);
    return A68_FALSE;
  } else if (match_string (cmd, "HEAp", BLANK_CHAR)) {
    int top = get_num_arg (cmd, NO_VAR);
    if (top <= 0) {
      top = A68 (heap_size);
    }
    show_heap (STDOUT_FILENO, p, A68_GC (busy_handles), top, A68 (term_heigth) - 4);
    return A68_FALSE;
  } else if (match_string (cmd, "APropos", NULL_CHAR) || match_string (cmd, "Help", NULL_CHAR) || match_string (cmd, "INfo", NULL_CHAR)) {
    apropos (STDOUT_FILENO, NO_TEXT, "monitor");
    return A68_FALSE;
  } else if (match_string (cmd, "APropos", BLANK_CHAR) || match_string (cmd, "Help", BLANK_CHAR) || match_string (cmd, "INfo", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    apropos (STDOUT_FILENO, NO_TEXT, sym);
    return A68_FALSE;
  } else if (match_string (cmd, "HT", NULL_CHAR)) {
    A68 (halt_typing) = A68_TRUE;
    A68 (do_confirm_exit) = A68_TRUE;
    return A68_TRUE;
  } else if (match_string (cmd, "RT", NULL_CHAR)) {
    A68 (halt_typing) = A68_FALSE;
    A68 (do_confirm_exit) = A68_TRUE;
    return A68_TRUE;
  } else if (match_string (cmd, "Breakpoint", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] == NULL_CHAR) {
      int listed = 0;
      list_breakpoints (TOP_NODE (&A68_JOB), &listed);
      if (listed == 0) {
        WRITELN (STDOUT_FILENO, "No breakpoints set");
      }
      if (A68_MON (watchpoint_expression) != NO_TEXT) {
        WRITELN (STDOUT_FILENO, "Watchpoint condition \"");
        WRITE (STDOUT_FILENO, A68_MON (watchpoint_expression));
        WRITE (STDOUT_FILENO, "\"");
      } else {
        WRITELN (STDOUT_FILENO, "No watchpoint expression set");
      }
    } else if (IS_DIGIT (sym[0])) {
      char *mod;
      int k = get_num_arg (cmd, &mod);
      SKIP_SPACE (mod);
      if (mod[0] == NULL_CHAR) {
        BOOL_T set = A68_FALSE;
        change_breakpoints (TOP_NODE (&A68_JOB), BREAKPOINT_MASK, k, &set, NULL);
        if (set == A68_FALSE) {
          monitor_error ("cannot set breakpoint in that line", NO_TEXT);
        }
      } else if (match_string (mod, "IF", BLANK_CHAR)) {
        char *cexpr = mod;
        BOOL_T set = A68_FALSE;
        SKIP_ONE_SYMBOL (cexpr);
        change_breakpoints (TOP_NODE (&A68_JOB), BREAKPOINT_MASK, k, &set, new_string (cexpr, NO_TEXT));
        if (set == A68_FALSE) {
          monitor_error ("cannot set breakpoint in that line", NO_TEXT);
        }
      } else if (match_string (mod, "Clear", NULL_CHAR)) {
        change_breakpoints (TOP_NODE (&A68_JOB), NULL_MASK, k, NULL, NULL);
      } else {
        monitor_error ("invalid breakpoint command", NO_TEXT);
      }
    } else if (match_string (sym, "List", NULL_CHAR)) {
      int listed = 0;
      list_breakpoints (TOP_NODE (&A68_JOB), &listed);
      if (listed == 0) {
        WRITELN (STDOUT_FILENO, "No breakpoints set");
      }
      if (A68_MON (watchpoint_expression) != NO_TEXT) {
        WRITELN (STDOUT_FILENO, "Watchpoint condition \"");
        WRITE (STDOUT_FILENO, A68_MON (watchpoint_expression));
        WRITE (STDOUT_FILENO, "\"");
      } else {
        WRITELN (STDOUT_FILENO, "No watchpoint expression set");
      }
    } else if (match_string (sym, "Watch", BLANK_CHAR)) {
      char *cexpr = sym;
      SKIP_ONE_SYMBOL (cexpr);
      if (A68_MON (watchpoint_expression) != NO_TEXT) {
        a68_free (A68_MON (watchpoint_expression));
        A68_MON (watchpoint_expression) = NO_TEXT;
      }
      A68_MON (watchpoint_expression) = new_string (cexpr, NO_TEXT);
      change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_WATCH_MASK, A68_TRUE);
    } else if (match_string (sym, "Clear", BLANK_CHAR)) {
      char *mod = sym;
      SKIP_ONE_SYMBOL (mod);
      if (mod[0] == NULL_CHAR) {
        change_breakpoints (TOP_NODE (&A68_JOB), NULL_MASK, 0, NULL, NULL);
        if (A68_MON (watchpoint_expression) != NO_TEXT) {
          a68_free (A68_MON (watchpoint_expression));
          A68_MON (watchpoint_expression) = NO_TEXT;
        }
        change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_WATCH_MASK, A68_FALSE);
      } else if (match_string (mod, "ALL", NULL_CHAR)) {
        change_breakpoints (TOP_NODE (&A68_JOB), NULL_MASK, 0, NULL, NULL);
        if (A68_MON (watchpoint_expression) != NO_TEXT) {
          a68_free (A68_MON (watchpoint_expression));
          A68_MON (watchpoint_expression) = NO_TEXT;
        }
        change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_WATCH_MASK, A68_FALSE);
      } else if (match_string (mod, "Breakpoints", NULL_CHAR)) {
        change_breakpoints (TOP_NODE (&A68_JOB), NULL_MASK, 0, NULL, NULL);
      } else if (match_string (mod, "Watchpoint", NULL_CHAR)) {
        if (A68_MON (watchpoint_expression) != NO_TEXT) {
          a68_free (A68_MON (watchpoint_expression));
          A68_MON (watchpoint_expression) = NO_TEXT;
        }
        change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_WATCH_MASK, A68_FALSE);
      } else {
        monitor_error ("invalid breakpoint command", NO_TEXT);
      }
    } else {
      monitor_error ("invalid breakpoint command", NO_TEXT);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "List", BLANK_CHAR)) {
    char *cwhere;
    int n = get_num_arg (cmd, &cwhere);
    int m = get_num_arg (cwhere, NO_VAR);
    if (m == NOT_A_NUM) {
      if (n > 0) {
        list (STDOUT_FILENO, p, n, 0);
      } else if (n == NOT_A_NUM) {
        list (STDOUT_FILENO, p, 10, 0);
      }
    } else if (n > 0 && m > 0 && n <= m) {
      list (STDOUT_FILENO, p, n, m);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "PROmpt", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] != NULL_CHAR) {
      if (sym[0] == QUOTE_CHAR) {
        sym++;
      }
      if (sym[strlen (sym) - 1] == QUOTE_CHAR) {
        sym[strlen (sym) - 1] = NULL_CHAR;
      }
      bufcpy (A68_MON (prompt), sym, BUFFER_SIZE);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "RERun", NULL_CHAR) || match_string (cmd, "REStart", NULL_CHAR)) {
    if (confirm_exit ()) {
      exit_genie (p, A68_RERUN);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "RESET", NULL_CHAR)) {
    if (confirm_exit ()) {
      change_breakpoints (TOP_NODE (&A68_JOB), NULL_MASK, 0, NULL, NULL);
      if (A68_MON (watchpoint_expression) != NO_TEXT) {
        a68_free (A68_MON (watchpoint_expression));
        A68_MON (watchpoint_expression) = NO_TEXT;
      }
      change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_WATCH_MASK, A68_FALSE);
      exit_genie (p, A68_RERUN);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "LINk", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    int printed = 0;
    if (k > 0) {
      stack_a68_link_dump (STDOUT_FILENO, A68_FP, k, &printed);
    } else if (k == NOT_A_NUM) {
      stack_a68_link_dump (STDOUT_FILENO, A68_FP, 3, &printed);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "STAck", BLANK_CHAR) || match_string (cmd, "BT", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    int printed = 0;
    if (k > 0) {
      stack_dump (STDOUT_FILENO, A68_FP, k, &printed);
    } else if (k == NOT_A_NUM) {
      stack_dump (STDOUT_FILENO, A68_FP, 3, &printed);
    }
    return A68_FALSE;
  } else if (match_string (cmd, "Next", NULL_CHAR)) {
    change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_TEMPORARY_MASK, A68_TRUE);
    A68 (do_confirm_exit) = A68_FALSE;
    A68_MON (break_proc_level) = PROCEDURE_LEVEL (INFO (p));
    return A68_TRUE;
  } else if (match_string (cmd, "STEp", NULL_CHAR)) {
    change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_TEMPORARY_MASK, A68_TRUE);
    A68 (do_confirm_exit) = A68_FALSE;
    return A68_TRUE;
  } else if (match_string (cmd, "FINish", NULL_CHAR) || match_string (cmd, "OUT", NULL_CHAR)) {
    A68_MON (finish_frame_pointer) = FRAME_PARAMETERS (A68_FP);
    A68 (do_confirm_exit) = A68_FALSE;
    return A68_TRUE;
  } else if (match_string (cmd, "Until", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    if (k > 0) {
      BOOL_T set = A68_FALSE;
      change_breakpoints (TOP_NODE (&A68_JOB), BREAKPOINT_TEMPORARY_MASK, k, &set, NULL);
      if (set == A68_FALSE) {
        monitor_error ("cannot set breakpoint in that line", NO_TEXT);
        return A68_FALSE;
      }
      A68 (do_confirm_exit) = A68_FALSE;
      return A68_TRUE;
    } else {
      monitor_error ("line number expected", NO_TEXT);
      return A68_FALSE;
    }
  } else if (match_string (cmd, "Where", NULL_CHAR)) {
    WIS (p);
    return A68_FALSE;
  } else if (strcmp (cmd, "?") == 0) {
    apropos (STDOUT_FILENO, A68_MON (prompt), "monitor");
    return A68_FALSE;
  } else if (match_string (cmd, "Sizes", NULL_CHAR)) {
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Frame stack pointer=" A68_LU " available=" A68_LU, A68_FP, A68 (frame_stack_size) - A68_FP) >= 0);
    WRITELN (STDOUT_FILENO, A68 (output_line));
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Expression stack pointer=" A68_LU " available=" A68_LU, A68_SP, (UNSIGNED_T) (A68 (expr_stack_size) - A68_SP)) >= 0);
    WRITELN (STDOUT_FILENO, A68 (output_line));
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Heap size=" A68_LU " available=" A68_LU, A68 (heap_size), heap_available ()) >= 0);
    WRITELN (STDOUT_FILENO, A68 (output_line));
    ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Garbage collections=%d", A68_GC (sweeps)) >= 0);
    WRITELN (STDOUT_FILENO, A68 (output_line));
    return A68_FALSE;
  } else if (match_string (cmd, "XRef", NULL_CHAR)) {
    int k = LINE_NUMBER (p);
    LINE_T *line = TOP_LINE (&A68_JOB);
    for (; line != NO_LINE; FORWARD (line)) {
      if (NUMBER (line) > 0 && NUMBER (line) == k) {
        list_source_line (STDOUT_FILENO, line, A68_TRUE);
      }
    }
    return A68_FALSE;
  } else if (match_string (cmd, "XRef", BLANK_CHAR)) {
    LINE_T *line = TOP_LINE (&A68_JOB);
    int k = get_num_arg (cmd, NO_VAR);
    if (k == NOT_A_NUM) {
      monitor_error ("line number expected", NO_TEXT);
    } else {
      for (; line != NO_LINE; FORWARD (line)) {
        if (NUMBER (line) > 0 && NUMBER (line) == k) {
          list_source_line (STDOUT_FILENO, line, A68_TRUE);
        }
      }
    }
    return A68_FALSE;
  } else if (strlen (cmd) == 0) {
    return A68_FALSE;
  } else {
    monitor_error ("unrecognised command", NO_TEXT);
    return A68_FALSE;
  }
}

//! @brief Evaluate conditional breakpoint expression.

static BOOL_T evaluate_breakpoint_expression (NODE_T * p)
{
  ADDR_T top_sp = A68_SP;
  volatile BOOL_T res = A68_FALSE;
  A68_MON (mon_errors) = 0;
  if (EXPR (INFO (p)) != NO_TEXT) {
    evaluate (STDOUT_FILENO, p, EXPR (INFO (p)));
    if (A68_MON (_m_sp) != 1 || A68_MON (mon_errors) != 0) {
      A68_MON (mon_errors) = 0;
      monitor_error ("deleted invalid breakpoint expression", NO_TEXT);
      if (EXPR (INFO (p)) != NO_TEXT) {
        a68_free (EXPR (INFO (p)));
      }
      EXPR (INFO (p)) = A68_MON (expr);
      res = A68_TRUE;
    } else if (TOP_MODE == M_BOOL) {
      A68_BOOL z;
      POP_OBJECT (p, &z, A68_BOOL);
      res = (BOOL_T) (STATUS (&z) == INIT_MASK && VALUE (&z) == A68_TRUE);
    } else {
      monitor_error ("deleted invalid breakpoint expression yielding mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
      if (EXPR (INFO (p)) != NO_TEXT) {
        a68_free (EXPR (INFO (p)));
      }
      EXPR (INFO (p)) = A68_MON (expr);
      res = A68_TRUE;
    }
  }
  A68_SP = top_sp;
  return res;
}

//! @brief Evaluate conditional watchpoint expression.

static BOOL_T evaluate_watchpoint_expression (NODE_T * p)
{
  ADDR_T top_sp = A68_SP;
  volatile BOOL_T res = A68_FALSE;
  A68_MON (mon_errors) = 0;
  if (A68_MON (watchpoint_expression) != NO_TEXT) {
    evaluate (STDOUT_FILENO, p, A68_MON (watchpoint_expression));
    if (A68_MON (_m_sp) != 1 || A68_MON (mon_errors) != 0) {
      A68_MON (mon_errors) = 0;
      monitor_error ("deleted invalid watchpoint expression", NO_TEXT);
      if (A68_MON (watchpoint_expression) != NO_TEXT) {
        a68_free (A68_MON (watchpoint_expression));
        A68_MON (watchpoint_expression) = NO_TEXT;
      }
      res = A68_TRUE;
    }
    if (TOP_MODE == M_BOOL) {
      A68_BOOL z;
      POP_OBJECT (p, &z, A68_BOOL);
      res = (BOOL_T) (STATUS (&z) == INIT_MASK && VALUE (&z) == A68_TRUE);
    } else {
      monitor_error ("deleted invalid watchpoint expression yielding mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
      if (A68_MON (watchpoint_expression) != NO_TEXT) {
        a68_free (A68_MON (watchpoint_expression));
        A68_MON (watchpoint_expression) = NO_TEXT;
      }
      res = A68_TRUE;
    }
  }
  A68_SP = top_sp;
  return res;
}

//! @brief Execute monitor.

void single_step (NODE_T * p, unsigned mask)
{
  volatile BOOL_T do_cmd = A68_TRUE;
  ADDR_T top_sp = A68_SP;
  A68_MON (current_frame) = 0;
  A68_MON (max_row_elems) = MAX_ROW_ELEMS;
  A68_MON (mon_errors) = 0;
  A68_MON (tabs) = 0;
  A68_MON (prompt_set) = A68_FALSE;
  if (LINE_NUMBER (p) == 0) {
    return;
  }
#if defined (HAVE_CURSES)
  genie_curses_end (NO_NODE);
#endif
  if (mask == (unsigned) BREAKPOINT_ERROR_MASK) {
    WRITELN (STDOUT_FILENO, "Monitor entered after an error");
    WIS ((p));
  } else if ((mask & BREAKPOINT_INTERRUPT_MASK) != 0) {
    WRITELN (STDOUT_FILENO, NEWLINE_STRING);
    WIS ((p));
    if (A68 (do_confirm_exit) && confirm_exit ()) {
      exit_genie ((p), A68_RUNTIME_ERROR + A68_FORCE_QUIT);
    }
  } else if ((mask & BREAKPOINT_MASK) != 0) {
    if (EXPR (INFO (p)) != NO_TEXT) {
      if (!evaluate_breakpoint_expression (p)) {
        return;
      }
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Breakpoint (%s)", EXPR (INFO (p))) >= 0);
    } else {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Breakpoint") >= 0);
    }
    WRITELN (STDOUT_FILENO, A68 (output_line));
    WIS (p);
  } else if ((mask & BREAKPOINT_TEMPORARY_MASK) != 0) {
    if (A68_MON (break_proc_level) != 0 && PROCEDURE_LEVEL (INFO (p)) > A68_MON (break_proc_level)) {
      return;
    }
    change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_TEMPORARY_MASK, A68_FALSE);
    WRITELN (STDOUT_FILENO, "Temporary breakpoint (now removed)");
    WIS (p);
  } else if ((mask & BREAKPOINT_WATCH_MASK) != 0) {
    if (!evaluate_watchpoint_expression (p)) {
      return;
    }
    if (A68_MON (watchpoint_expression) != NO_TEXT) {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Watchpoint (%s)", A68_MON (watchpoint_expression)) >= 0);
    } else {
      ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "Watchpoint (now removed)") >= 0);
    }
    WRITELN (STDOUT_FILENO, A68 (output_line));
    WIS (p);
  } else if ((mask & BREAKPOINT_TRACE_MASK) != 0) {
    PROP_T *prop = &GPROP (p);
    WIS ((p));
    if (propagator_name (UNIT (prop)) != NO_TEXT) {
      WRITELN (STDOUT_FILENO, propagator_name (UNIT (prop)));
    }
    return;
  } else {
    WRITELN (STDOUT_FILENO, "Monitor entered with no valid reason (continuing execution)");
    WIS ((p));
    return;
  }
#if defined (BUILD_PARALLEL_CLAUSE)
  if (is_main_thread ()) {
    WRITELN (STDOUT_FILENO, "This is the main thread");
  } else {
    WRITELN (STDOUT_FILENO, "This is not the main thread");
  }
#endif
// Entry into the monitor.
  if (A68_MON (prompt_set) == A68_FALSE) {
    bufcpy (A68_MON (prompt), "(a68g) ", BUFFER_SIZE);
    A68_MON (prompt_set) = A68_TRUE;
  }
  A68_MON (in_monitor) = A68_TRUE;
  A68_MON (break_proc_level) = 0;
  change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
  STATUS_CLEAR (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK);
  while (do_cmd) {
    char *cmd;
    A68_SP = top_sp;
    io_close_tty_line ();
    while (strlen (cmd = read_string_from_tty (A68_MON (prompt))) == 0) {;
    }
    if (TO_UCHAR (cmd[0]) == TO_UCHAR (EOF_CHAR)) {
      bufcpy (cmd, LOGOUT_STRING, BUFFER_SIZE);
      WRITE (STDOUT_FILENO, LOGOUT_STRING);
      WRITE (STDOUT_FILENO, NEWLINE_STRING);
    }
    A68_MON (_m_sp) = 0;
    do_cmd = (BOOL_T) (!single_stepper (p, cmd));
  }
  A68_SP = top_sp;
  A68_MON (in_monitor) = A68_FALSE;
  if (mask == (unsigned) BREAKPOINT_ERROR_MASK) {
    WRITELN (STDOUT_FILENO, "Continuing from an error might corrupt things");
    single_step (p, (unsigned) BREAKPOINT_ERROR_MASK);
  } else {
    WRITELN (STDOUT_FILENO, "Continuing ...");
    WRITELN (STDOUT_FILENO, "");
  }
}

//! @brief PROC debug = VOID

void genie_debug (NODE_T * p)
{
  single_step (p, BREAKPOINT_INTERRUPT_MASK);
}

//! @brief PROC break = VOID

void genie_break (NODE_T * p)
{
  (void) p;
  change_masks (TOP_NODE (&A68_JOB), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
}

//! @brief PROC evaluate = (STRING) STRING

void genie_evaluate (NODE_T * p)
{
  A68_REF u, v;
  v = empty_string (p);
// Pop argument.
  POP_REF (p, (A68_REF *) & u);
  volatile ADDR_T top_sp = A68_SP;
  CHECK_MON_REF (p, u, M_STRING);
  reset_transput_buffer (UNFORMATTED_BUFFER);
  add_a_string_transput_buffer (p, UNFORMATTED_BUFFER, (BYTE_T *) & u);
  v = c_to_a_string (p, get_transput_buffer (UNFORMATTED_BUFFER), DEFAULT_WIDTH);
// Evaluate in the monitor.
  A68_MON (in_monitor) = A68_TRUE;
  A68_MON (mon_errors) = 0;
  evaluate (STDOUT_FILENO, p, get_transput_buffer (UNFORMATTED_BUFFER));
  A68_MON (in_monitor) = A68_FALSE;
  if (A68_MON (_m_sp) != 1) {
    monitor_error ("invalid expression", NO_TEXT);
  }
  if (A68_MON (mon_errors) == 0) {
    MOID_T *res;
    BOOL_T cont = A68_TRUE;
    while (cont) {
      res = TOP_MODE;
      cont = (BOOL_T) (IS_REF (res) && !IS_NIL (*(A68_REF *) STACK_ADDRESS (top_sp)));
      if (cont) {
        A68_REF w;
        POP_REF (p, &w);
        TOP_MODE = SUB (TOP_MODE);
        PUSH (p, ADDRESS (&w), SIZE (TOP_MODE));
      }
    }
    reset_transput_buffer (UNFORMATTED_BUFFER);
    genie_write_standard (p, TOP_MODE, STACK_ADDRESS (top_sp), nil_ref);
    v = c_to_a_string (p, get_transput_buffer (UNFORMATTED_BUFFER), DEFAULT_WIDTH);
  }
  A68_SP = top_sp;
  PUSH_REF (p, v);
}

//! @brief PROC abend = (STRING) VOID

void genie_abend (NODE_T * p)
{
  A68_REF u;
  POP_REF (p, (A68_REF *) & u);
  reset_transput_buffer (UNFORMATTED_BUFFER);
  add_a_string_transput_buffer (p, UNFORMATTED_BUFFER, (BYTE_T *) & u);
  diagnostic (A68_RUNTIME_ERROR | A68_NO_SYNTHESIS, p, get_transput_buffer (UNFORMATTED_BUFFER), NO_TEXT);
  exit_genie (p, A68_RUNTIME_ERROR);
}
