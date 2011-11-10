/*
\file monitor.c
\brief low-level monitor for the interpreter
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2011 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

/*
This is a basic monitor for Algol68G. It activates when the interpreter
receives SIGINT (CTRL-C, for instance) or when PROC VOID break, debug or
evaluate is called, or when a runtime error occurs and --debug is selected.

The monitor allows single stepping (unit-wise through serial/enquiry
clauses) and has basic means for inspecting call-frame stack and heap. 
*/

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

#if (defined HAVE_TERM_H && defined HAVE_LIBTERMCAP)
#include <term.h>
extern char term_buffer[];
#endif

#define CANNOT_SHOW " unprintable value or uninitialised value"
#define MAX_ROW_ELEMS 24
#define NOT_A_NUM (-1)
#define NO_VALUE " uninitialised value"
#define STACK_SIZE 32
#define TOP_MODE (_m_stack[_m_sp - 1])
#define LOGOUT_STRING "exit"

ADDR_T finish_frame_pointer = 0;
BOOL_T in_monitor = A68_FALSE;
char *watchpoint_expression = NO_TEXT;
int break_proc_level = 0;

static BOOL_T check_initialisation (NODE_T *, BYTE_T *, MOID_T *, BOOL_T *);
static char symbol[BUFFER_SIZE], error_text[BUFFER_SIZE], expr[BUFFER_SIZE];
static char prompt[BUFFER_SIZE];
static BOOL_T prompt_set = A68_FALSE;

static int current_frame = 0;
static int max_row_elems = MAX_ROW_ELEMS;
static int mon_errors = 0;
static int _m_sp;
static int pos, attr;
static int tabs = 0;
static MOID_T *_m_stack[STACK_SIZE];

static void parse (FILE_T, NODE_T *, int);

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
    ASSERT (snprintf(edit_line, SNPRINTF_SIZE, "%s", moid_to_string ((m), MOID_WIDTH, NO_NODE)) >= 0);\
    monitor_error (NO_VALUE, edit_line);\
    QUIT_ON_ERROR;\
  } else if (IS_NIL (z)) {\
    ASSERT (snprintf(edit_line, SNPRINTF_SIZE, "%s", moid_to_string ((m), MOID_WIDTH, NO_NODE)) >= 0);\
    monitor_error ("accessing NIL name", edit_line);\
    QUIT_ON_ERROR;\
  }

#define QUIT_ON_ERROR\
  if (mon_errors > 0) {\
    return;\
  }

#define PARSE_CHECK(f, p, d)\
  parse ((f), (p), (d));\
  QUIT_ON_ERROR;

#define SCAN_CHECK(f, p)\
  scan_sym((f), (p));\
  QUIT_ON_ERROR;

/*!
\brief confirm that we really want to quit
\return same
*/

static BOOL_T confirm_exit (void)
{
  char *cmd;
  int k;
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Terminate %s (yes|no): ", a68g_cmd_name) >= 0);
  WRITELN (STDOUT_FILENO, output_line);
  cmd = read_string_from_tty (NULL);
  if (TO_UCHAR (cmd[0]) == TO_UCHAR (EOF_CHAR)) {
    return (confirm_exit ());
  }
  for (k = 0; cmd[k] != NULL_CHAR; k++) {
    cmd[k] = (char) TO_LOWER (cmd[k]);
  }
  if (strcmp (cmd, "y") == 0) {
    return (A68_TRUE);
  }
  if (strcmp (cmd, "yes") == 0) {
    return (A68_TRUE);
  }
  if (strcmp (cmd, "n") == 0) {
    return (A68_FALSE);
  }
  if (strcmp (cmd, "no") == 0) {
    return (A68_FALSE);
  }
  return (confirm_exit ());
}

/*!
\brief give a monitor error message
\param msg error message
\param info extra information
*/

void monitor_error (char *msg, char *info)
{
  QUIT_ON_ERROR;
  mon_errors++;
  bufcpy (error_text, msg, BUFFER_SIZE);
  WRITELN (STDOUT_FILENO, a68g_cmd_name);
  WRITE (STDOUT_FILENO, ": monitor error: ");
  WRITE (STDOUT_FILENO, error_text);
  if (info != NO_TEXT) {
    WRITE (STDOUT_FILENO, " (");
    WRITE (STDOUT_FILENO, info);
    WRITE (STDOUT_FILENO, ")");
  }
  WRITE (STDOUT_FILENO, ".");
}

/*!
\brief scan symbol from input
\param f file number
\param p position in tree
*/

static void scan_sym (FILE_T f, NODE_T * p)
{
  int k = 0;
  (void) f;
  (void) p;
  symbol[0] = NULL_CHAR;
  attr = 0;
  QUIT_ON_ERROR;
  while (IS_SPACE (expr[pos])) {
    pos++;
  }
  if (expr[pos] == NULL_CHAR) {
    attr = 0;
    symbol[0] = NULL_CHAR;
    return;
  } else if (expr[pos] == ':') {
    if (strncmp (&(expr[pos]), ":=:", 3) == 0) {
      pos += 3;
      bufcpy (symbol, ":=:", BUFFER_SIZE);
      attr = IS_SYMBOL;
    } else if (strncmp (&(expr[pos]), ":/=:", 4) == 0) {
      pos += 4;
      bufcpy (symbol, ":/=:", BUFFER_SIZE);
      attr = ISNT_SYMBOL;
    } else if (strncmp (&(expr[pos]), ":=", 2) == 0) {
      pos += 2;
      bufcpy (symbol, ":=", BUFFER_SIZE);
      attr = ASSIGN_SYMBOL;
    } else {
      pos++;
      bufcpy (symbol, ":", BUFFER_SIZE);
      attr = COLON_SYMBOL;
    }
    return;
  } else if (expr[pos] == QUOTE_CHAR) {
    BOOL_T cont = A68_TRUE;
    pos++;
    while (cont) {
      while (expr[pos] != QUOTE_CHAR) {
        symbol[k++] = expr[pos++];
      }
      if (expr[++pos] == QUOTE_CHAR) {
        symbol[k++] = QUOTE_CHAR;
      } else {
        cont = A68_FALSE;
      }
    }
    symbol[k] = NULL_CHAR;
    attr = ROW_CHAR_DENOTATION;
    return;
  } else if (IS_LOWER (expr[pos])) {
    while (IS_LOWER (expr[pos]) || IS_DIGIT (expr[pos]) || IS_SPACE (expr[pos])) {
      if (IS_SPACE (expr[pos])) {
        pos++;
      } else {
        symbol[k++] = expr[pos++];
      }
    }
    symbol[k] = NULL_CHAR;
    attr = IDENTIFIER;
    return;
  } else if (IS_UPPER (expr[pos])) {
    KEYWORD_T *kw;
    while (IS_UPPER (expr[pos])) {
      symbol[k++] = expr[pos++];
    }
    symbol[k] = NULL_CHAR;
    kw = find_keyword (top_keyword, symbol);
    if (kw != NO_KEYWORD) {
      attr = ATTRIBUTE (kw);
    } else {
      attr = OPERATOR;
    }
    return;
  } else if (IS_DIGIT (expr[pos])) {
    while (IS_DIGIT (expr[pos])) {
      symbol[k++] = expr[pos++];
    }
    if (expr[pos] == 'r') {
      symbol[k++] = expr[pos++];
      while (IS_XDIGIT (expr[pos])) {
        symbol[k++] = expr[pos++];
      }
      symbol[k] = NULL_CHAR;
      attr = BITS_DENOTATION;
      return;
    }
    if (expr[pos] != POINT_CHAR && expr[pos] != 'e' && expr[pos] != 'E') {
      symbol[k] = NULL_CHAR;
      attr = INT_DENOTATION;
      return;
    }
    if (expr[pos] == POINT_CHAR) {
      symbol[k++] = expr[pos++];
      while (IS_DIGIT (expr[pos])) {
        symbol[k++] = expr[pos++];
      }
    }
    if (expr[pos] != 'e' && expr[pos] != 'E') {
      symbol[k] = NULL_CHAR;
      attr = REAL_DENOTATION;
      return;
    }
    symbol[k++] = (char) TO_UPPER (expr[pos++]);
    if (expr[pos] == '+' || expr[pos] == '-') {
      symbol[k++] = expr[pos++];
    }
    while (IS_DIGIT (expr[pos])) {
      symbol[k++] = expr[pos++];
    }
    symbol[k] = NULL_CHAR;
    attr = REAL_DENOTATION;
    return;
  } else if (a68g_strchr (MONADS, expr[pos]) != NO_TEXT || a68g_strchr (NOMADS, expr[pos]) != NO_TEXT) {
    symbol[k++] = expr[pos++];
    if (a68g_strchr (NOMADS, expr[pos]) != NO_TEXT) {
      symbol[k++] = expr[pos++];
    }
    if (expr[pos] == ':') {
      symbol[k++] = expr[pos++];
      if (expr[pos] == '=') {
        symbol[k++] = expr[pos++];
      } else {
        symbol[k] = NULL_CHAR;
        monitor_error ("invalid operator symbol", symbol);
      }
    } else if (expr[pos] == '=') {
      symbol[k++] = expr[pos++];
      if (expr[pos] == ':') {
        symbol[k++] = expr[pos++];
      } else {
        symbol[k] = NULL_CHAR;
        monitor_error ("invalid operator symbol", symbol);
      }
    }
    symbol[k] = NULL_CHAR;
    attr = OPERATOR;
    return;
  } else if (expr[pos] == '(') {
    pos++;
    attr = OPEN_SYMBOL;
    return;
  } else if (expr[pos] == ')') {
    pos++;
    attr = CLOSE_SYMBOL;
    return;
  } else if (expr[pos] == '[') {
    pos++;
    attr = SUB_SYMBOL;
    return;
  } else if (expr[pos] == ']') {
    pos++;
    attr = BUS_SYMBOL;
    return;
  } else if (expr[pos] == ',') {
    pos++;
    attr = COMMA_SYMBOL;
    return;
  } else if (expr[pos] == ';') {
    pos++;
    attr = SEMI_SYMBOL;
    return;
  }
}

/*!
\brief find a tag, searching symbol tables towards the root
\param table symbol table
\param a attribute
\param name name of token
\return entry in symbol table
**/

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
      ABEND (A68_TRUE, "impossible state in find_tag_global", NO_TEXT);
    }
    for (; s != NO_TAG; FORWARD (s)) {
      if (strcmp (NSYMBOL (NODE (s)), name) == 0) {
        return (s);
      }
    }
    return (find_tag_global (PREVIOUS (table), a, name));
  } else {
    return (NO_TAG);
  }
}

/*!
\brief priority for symbol at input
\param f file number
\param p position in tree
\return same
*/

static int prio (FILE_T f, NODE_T * p)
{
  TAG_T *s = find_tag (a68g_standenv, PRIO_SYMBOL, symbol);
  (void) p;
  (void) f;
  if (s == NO_TAG) {
    monitor_error ("unknown operator, cannot set priority", symbol);
    return (0);
  }
  return (PRIO (s));
}

/*!
\brief push a mode on the stack
\param f file number
\param m mode to push
*/

static void push_mode (FILE_T f, MOID_T * m)
{
  (void) f;
  if (_m_sp < STACK_SIZE) {
    _m_stack[_m_sp++] = m;
  } else {
    monitor_error ("expression too complex", NO_TEXT);
  }
}

/*!
\brief dereference, WEAK or otherwise
\param k position in mode stack
\param context context
\return whether value can be dereferenced further
*/

static BOOL_T deref_condition (int k, int context)
{
  MOID_T *u = _m_stack[k];
  if (context == WEAK && SUB (u) != NO_MOID) {
    MOID_T *v = SUB (u);
    BOOL_T stowed = (BOOL_T) (IS (v, FLEX_SYMBOL) || IS (v, ROW_SYMBOL) || IS (v, STRUCT_SYMBOL));
    return ((BOOL_T) (IS (u, REF_SYMBOL) && !stowed));
  } else {
    return ((BOOL_T) (IS (u, REF_SYMBOL)));
  }
}

/*!
\brief weak dereferencing
\param p position in tree
\param k position in mode stack
\int context context
*/

static void deref (NODE_T * p, int k, int context)
{
  while (deref_condition (k, context)) {
    A68_REF z;
    POP_REF (p, &z);
    CHECK_MON_REF (p, z, _m_stack[k]);
    _m_stack[k] = SUB (_m_stack[k]);
    PUSH (p, ADDRESS (&z), MOID_SIZE (_m_stack[k]));
  }
}

/*!
\brief search moid that matches indicant
\param refs whether we look for a REF indicant
\param leng sizety of indicant
\param indy indicant name
\return moiD
**/

static MOID_T *search_mode (int refs, int leng, char *indy)
{
  MOID_T *m = NO_MOID, *z = NO_MOID;
  for (m = TOP_MOID (&program); m != NO_MOID; FORWARD (m)) {
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
    return (NO_MOID);
  }
  for (m = TOP_MOID (&program); m != NO_MOID; FORWARD (m)) {
    int k = 0;
    while (IS (m, REF_SYMBOL)) {
      k++;
      m = SUB (m);
    }
    if (k == refs && m == z) {
      while (EQUIVALENT (z) != NO_MOID) {
        z = EQUIVALENT (z);
      }
      return (z);
    }
  }
  return (NO_MOID);
}

/*!
\brief search operator X SYM Y
\param sym operator name
\param x lhs mode
\param y rhs mode
\return entry in symbol table
*/

static TAG_T *search_operator (char *sym, MOID_T * x, MOID_T * y)
{
  TAG_T *t;
  for (t = OPERATORS (a68g_standenv); t != NO_TAG; FORWARD (t)) {
    if (strcmp (NSYMBOL (NODE (t)), sym) == 0) {
      PACK_T *p = PACK (MOID (t));
      if (x == MOID (p)) {
        FORWARD (p);
        if (p == NO_PACK && y == NO_MOID) {
/* Matched in case of a monad */
          return (t);
        } else if (p != NO_PACK && y != NO_MOID && y == MOID (p)) {
/* Matched in case of a nomad */
          return (t);
        }
      }
    }
  }
/* Not found yet, try dereferencing */
  if (IS (x, REF_SYMBOL)) {
    return (search_operator (sym, SUB (x), y));
  }
  if (y != NO_MOID && IS (y, REF_SYMBOL)) {
    return (search_operator (sym, x, SUB (y)));
  }
/* Not found. Grrrr. Give a message */
  if (y == NO_MOID) {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s %s", sym, moid_to_string (x, MOID_WIDTH, NO_NODE)) >= 0);
  } else {
    ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s %s %s", moid_to_string (x, MOID_WIDTH, NO_NODE), sym, moid_to_string (y, MOID_WIDTH, NO_NODE)) >= 0);
  }
  monitor_error ("cannot find operator in standard environ", edit_line);
  return (NO_TAG);
}

/*!
\brief search identifier in frame stack and push value
\param f file number
\param p position in tree
\param a68g_link current frame pointer
\param sym identifier name
*/

static void search_identifier (FILE_T f, NODE_T * p, ADDR_T a68g_link, char *sym)
{
  if (a68g_link > 0) {
    int dynamic_a68g_link = FRAME_DYNAMIC_LINK (a68g_link);
    if (current_frame == 0 || (current_frame == FRAME_NUMBER (a68g_link))) {
      NODE_T *u = FRAME_TREE (a68g_link);
      if (u != NO_NODE) {
        TABLE_T *q = TABLE (u);
        TAG_T *i = IDENTIFIERS (q);
        for (; i != NO_TAG; FORWARD (i)) {
          if (strcmp (NSYMBOL (NODE (i)), sym) == 0) {
            ADDR_T posit = a68g_link + FRAME_INFO_SIZE + OFFSET (i);
            MOID_T *m = MOID (i);
            PUSH (p, FRAME_ADDRESS (posit), MOID_SIZE (m));
            push_mode (f, m);
            return;
          }
        }
      }
    }
    search_identifier (f, p, dynamic_a68g_link, sym);
  } else {
    TABLE_T *q = a68g_standenv;
    TAG_T *i = IDENTIFIERS (q);
    for (; i != NO_TAG; FORWARD (i)) {
      if (strcmp (NSYMBOL (NODE (i)), sym) == 0) {
        if (IS (MOID (i), PROC_SYMBOL)) {
          static A68_PROCEDURE z;
          STATUS (&z) = (STATUS_MASK) (INITIALISED_MASK | STANDENV_PROC_MASK);
          PROCEDURE (&(BODY (&z))) = PROCEDURE (i);
          ENVIRON (&z) = 0;
          LOCALE (&z) = NO_HANDLE;
          MOID (&z) = MOID (i);
          PUSH_PROCEDURE (p, z);
        } else {
          UP_BLOCK_GC;
          (*(PROCEDURE (i))) (p);
          DOWN_BLOCK_GC;
        }
        push_mode (f, MOID (i));
        return;
      }
    }
    monitor_error ("cannot find identifier", sym);
  }
}

/*!
\brief coerce arguments in a call
\param f file number
\param p position in tree
*/

static void coerce_arguments (FILE_T f, NODE_T * p, MOID_T * proc, int bot, int top, int top_sp)
{
  int k;
  PACK_T *u;
  ADDR_T sp_2 = top_sp;
  (void) f;
  if ((top - bot) != DIM (proc)) {
    monitor_error ("invalid procedure argument count", NO_TEXT);
  }
  QUIT_ON_ERROR;
  for (k = bot, u = PACK (proc); k < top; k++, FORWARD (u)) {
    if (_m_stack[k] == MOID (u)) {
      PUSH (p, STACK_ADDRESS (sp_2), MOID_SIZE (MOID (u)));
      sp_2 += MOID_SIZE (MOID (u));
    } else if (IS (_m_stack[k], REF_SYMBOL)) {
      A68_REF *v = (A68_REF *) STACK_ADDRESS (sp_2);
      PUSH_REF (p, *v);
      sp_2 += ALIGNED_SIZE_OF (A68_REF);
      deref (p, k, STRONG);
      if (_m_stack[k] != MOID (u)) {
        ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s to %s", moid_to_string (_m_stack[k], MOID_WIDTH, NO_NODE), moid_to_string (MOID (u), MOID_WIDTH, NO_NODE)) >= 0);
        monitor_error ("invalid argument mode", edit_line);
      }
    } else {
      ASSERT (snprintf (edit_line, SNPRINTF_SIZE, "%s to %s", moid_to_string (_m_stack[k], MOID_WIDTH, NO_NODE), moid_to_string (MOID (u), MOID_WIDTH, NO_NODE)) >= 0);
      monitor_error ("cannot coerce argument", edit_line);
    }
    QUIT_ON_ERROR;
  }
  MOVE (STACK_ADDRESS (top_sp), STACK_ADDRESS (sp_2), stack_pointer - sp_2);
  stack_pointer = top_sp + (stack_pointer - sp_2);
}

/*!
\brief perform a selection
\param f file number
\param p position in tree
\param field field name
*/

static void selection (FILE_T f, NODE_T * p, char *field)
{
  BOOL_T name;
  MOID_T *moid;
  PACK_T *u, *v;
  SCAN_CHECK (f, p);
  if (attr != IDENTIFIER && attr != OPEN_SYMBOL) {
    monitor_error ("invalid selection syntax", NO_TEXT);
  }
  QUIT_ON_ERROR;
  PARSE_CHECK (f, p, MAX_PRIORITY + 1);
  deref (p, _m_sp - 1, WEAK);
  if (IS (TOP_MODE, REF_SYMBOL)) {
    name = A68_TRUE;
    u = PACK (NAME (TOP_MODE));
    moid = SUB (_m_stack[--_m_sp]);
    v = PACK (moid);
  } else {
    name = A68_FALSE;
    moid = _m_stack[--_m_sp];
    u = PACK (moid);
    v = PACK (moid);
  }
  if (ISNT (moid, STRUCT_SYMBOL)) {
    monitor_error ("invalid selection mode", moid_to_string (moid, MOID_WIDTH, NO_NODE));
  }
  QUIT_ON_ERROR;
  for (; u != NO_PACK; FORWARD (u), FORWARD (v)) {
    if (strcmp (field, TEXT (u)) == 0) {
      if (name) {
        A68_REF *z = (A68_REF *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REF)));
        CHECK_MON_REF (p, *z, moid);
        OFFSET (z) += OFFSET (v);
      } else {
        DECREMENT_STACK_POINTER (p, MOID_SIZE (moid));
        MOVE (STACK_TOP, STACK_OFFSET (OFFSET (v)), (unsigned) MOID_SIZE (MOID (u)));
        INCREMENT_STACK_POINTER (p, MOID_SIZE (MOID (u)));
      }
      push_mode (f, MOID (u));
      return;
    }
  }
  monitor_error ("invalid field name", field);
}

/*!
\brief perform a call
\param f file number
\param p position in tree
\param depth recursion depth
*/

static void call (FILE_T f, NODE_T * p, int depth)
{
  A68_PROCEDURE z;
  NODE_T q;
  int args, old_m_sp;
  ADDR_T top_sp;
  MOID_T *proc;
  (void) depth;
  QUIT_ON_ERROR;
  deref (p, _m_sp - 1, STRONG);
  proc = _m_stack[--_m_sp];
  old_m_sp = _m_sp;
  if (ISNT (proc, PROC_SYMBOL)) {
    monitor_error ("invalid procedure mode", moid_to_string (proc, MOID_WIDTH, NO_NODE));
  }
  QUIT_ON_ERROR;
  POP_PROCEDURE (p, &z);
  args = _m_sp;
  top_sp = stack_pointer;
  if (attr == OPEN_SYMBOL) {
    do {
      SCAN_CHECK (f, p);
      PARSE_CHECK (f, p, 0);
    } while (attr == COMMA_SYMBOL);
    if (attr != CLOSE_SYMBOL) {
      monitor_error ("unmatched parenthesis", NO_TEXT);
    }
    SCAN_CHECK (f, p);
  }
  coerce_arguments (f, p, proc, args, _m_sp, top_sp);
  if (STATUS (&z) & STANDENV_PROC_MASK) {
    MOID (&q) = _m_stack[--_m_sp];
    INFO (&q) = INFO (p);
    NSYMBOL (&q) = NSYMBOL (p);
    (void) ((*PROCEDURE (&(BODY (&z)))) (&q));
    _m_sp = old_m_sp;
    push_mode (f, SUB_MOID (&z));
  } else {
    monitor_error ("can only call standard environ routines", NO_TEXT);
  }
}

/*!
\brief perform a slice
\param f file number
\param p position in tree
\param depth recursion depth
*/

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
  deref (p, _m_sp - 1, WEAK);
  if (IS (TOP_MODE, REF_SYMBOL)) {
    name = A68_TRUE;
    res = NAME (TOP_MODE);
    deref (p, _m_sp - 1, STRONG);
    moid = _m_stack[--_m_sp];
  } else {
    name = A68_FALSE;
    moid = _m_stack[--_m_sp];
    res = SUB (moid);
  }
  if (ISNT (moid, ROW_SYMBOL) && ISNT (moid, FLEX_SYMBOL)) {
    monitor_error ("invalid row mode", moid_to_string (moid, MOID_WIDTH, NO_NODE));
  }
  QUIT_ON_ERROR;
/* Get descriptor */
  POP_REF (p, &z);
  CHECK_MON_REF (p, z, moid);
  GET_DESCRIPTOR (arr, tup, &z);
  if (IS (moid, FLEX_SYMBOL)) {
    dim = DIM (SUB (moid));
  } else {
    dim = DIM (moid);
  }
/* Get iindexer */
  args = _m_sp;
  if (attr == SUB_SYMBOL) {
    do {
      SCAN_CHECK (f, p);
      PARSE_CHECK (f, p, 0);
    } while (attr == COMMA_SYMBOL);
    if (attr != BUS_SYMBOL) {
      monitor_error ("unmatched parenthesis", NO_TEXT);
    }
    SCAN_CHECK (f, p);
  }
  if ((_m_sp - args) != dim) {
    monitor_error ("invalid slice index count", NO_TEXT);
  }
  QUIT_ON_ERROR;
  for (k = 0, iindex = 0; k < dim; k++, _m_sp--) {
    A68_TUPLE *t = &(tup[dim - k - 1]);
    A68_INT i;
    deref (p, _m_sp - 1, MEEK);
    if (TOP_MODE != MODE (INT)) {
      monitor_error ("invalid indexer mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
    }
    QUIT_ON_ERROR;
    POP_OBJECT (p, &i, A68_INT);
    if (VALUE (&i) < LOWER_BOUND (t) || VALUE (&i) > UPPER_BOUND (t)) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
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
    PUSH (p, ADDRESS (&(ARRAY (arr))) + address, MOID_SIZE (res));
  }
  push_mode (f, res);
}

/*!
\brief perform a call or a slice
\param f file number
\param p position in tree
\param depth recursion depth
*/

static void call_or_slice (FILE_T f, NODE_T * p, int depth)
{
  while (attr == OPEN_SYMBOL || attr == SUB_SYMBOL) {
    QUIT_ON_ERROR;
    if (attr == OPEN_SYMBOL) {
      call (f, p, depth);
    } else if (attr == SUB_SYMBOL) {
      slice (f, p, depth);
    }
  }
}

/*!
\brief parse expression on input
\param f file number
\param p position in tree
\param depth recursion depth
*/

static void parse (FILE_T f, NODE_T * p, int depth)
{
  LOW_STACK_ALERT (p);
  QUIT_ON_ERROR;
  if (depth <= MAX_PRIORITY) {
    if (depth == 0) {
/* Identity relations */
      PARSE_CHECK (f, p, 1);
      while (attr == IS_SYMBOL || attr == ISNT_SYMBOL) {
        A68_REF x, y;
        BOOL_T res;
        int op = attr;
        if (TOP_MODE != MODE (HIP)
            && ISNT (TOP_MODE, REF_SYMBOL)) {
          monitor_error ("identity relation operand must yield a name", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
        }
        SCAN_CHECK (f, p);
        PARSE_CHECK (f, p, 1);
        if (TOP_MODE != MODE (HIP)
            && ISNT (TOP_MODE, REF_SYMBOL)) {
          monitor_error ("identity relation operand must yield a name", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
        }
        QUIT_ON_ERROR;
        if (TOP_MODE != MODE (HIP) && _m_stack[_m_sp - 2] != MODE (HIP)) {
          if (TOP_MODE != _m_stack[_m_sp - 2]) {
            monitor_error ("invalid identity relation operand mode", NO_TEXT);
          }
        }
        QUIT_ON_ERROR;
        _m_sp -= 2;
        POP_REF (p, &y);
        POP_REF (p, &x);
        res = (BOOL_T) (ADDRESS (&x) == ADDRESS (&y));
        PUSH_PRIMITIVE (p, (BOOL_T) (op == IS_SYMBOL ? res : !res), A68_BOOL);
        push_mode (f, MODE (BOOL));
      }
    } else {
/* Dyadic expressions */
      PARSE_CHECK (f, p, depth + 1);
      while (attr == OPERATOR && prio (f, p) == depth) {
        int args;
        ADDR_T top_sp;
        NODE_T q;
        TAG_T *opt;
        char name[BUFFER_SIZE];
        bufcpy (name, symbol, BUFFER_SIZE);
        args = _m_sp - 1;
        top_sp = stack_pointer - MOID_SIZE (_m_stack[args]);
        SCAN_CHECK (f, p);
        PARSE_CHECK (f, p, depth + 1);
        opt = search_operator (name, _m_stack[_m_sp - 2], TOP_MODE);
        QUIT_ON_ERROR;
        coerce_arguments (f, p, MOID (opt), args, _m_sp, top_sp);
        _m_sp -= 2;
        MOID (&q) = MOID (opt);
        INFO (&q) = INFO (p);
        NSYMBOL (&q) = NSYMBOL (p);
        UP_BLOCK_GC;
        (void) ((*(PROCEDURE (opt)))) (&q);
        DOWN_BLOCK_GC;
        push_mode (f, SUB_MOID (opt));
      }
    }
  } else if (attr == OPERATOR) {
    int args;
    ADDR_T top_sp;
    NODE_T q;
    TAG_T *opt;
    char name[BUFFER_SIZE];
    bufcpy (name, symbol, BUFFER_SIZE);
    args = _m_sp;
    top_sp = stack_pointer;
    SCAN_CHECK (f, p);
    PARSE_CHECK (f, p, depth);
    opt = search_operator (name, TOP_MODE, NO_MOID);
    QUIT_ON_ERROR;
    coerce_arguments (f, p, MOID (opt), args, _m_sp, top_sp);
    _m_sp--;
    MOID (&q) = MOID (opt);
    INFO (&q) = INFO (p);
    NSYMBOL (&q) = NSYMBOL (p);
    UP_BLOCK_GC;
    (void) ((*(PROCEDURE (opt))) (&q));
    DOWN_BLOCK_GC;
    push_mode (f, SUB_MOID (opt));
  } else if (attr == REF_SYMBOL) {
    int refs = 0, length = 0;
    MOID_T *m = NO_MOID;
    while (attr == REF_SYMBOL) {
      refs++;
      SCAN_CHECK (f, p);
    }
    while (attr == LONG_SYMBOL) {
      length++;
      SCAN_CHECK (f, p);
    }
    m = search_mode (refs, length, symbol);
    QUIT_ON_ERROR;
    if (m == NO_MOID) {
      monitor_error ("unknown reference to mode", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    if (attr != OPEN_SYMBOL) {
      monitor_error ("cast expects open-symbol", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    PARSE_CHECK (f, p, 0);
    if (attr != CLOSE_SYMBOL) {
      monitor_error ("cast expects close-symbol", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    while (IS (TOP_MODE, REF_SYMBOL) && TOP_MODE != m) {
      MOID_T *sub = SUB (TOP_MODE);
      A68_REF z;
      POP_REF (p, &z);
      CHECK_MON_REF (p, z, TOP_MODE);
      PUSH (p, ADDRESS (&z), MOID_SIZE (sub));
      TOP_MODE = sub;
    }
    if (TOP_MODE != m) {
      monitor_error ("invalid cast mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
    }
  } else if (attr == LONG_SYMBOL) {
    int length = 0;
    MOID_T *m;
    while (attr == LONG_SYMBOL) {
      length++;
      SCAN_CHECK (f, p);
    }
/* Cast L INT -> L REAL */
    if (attr == REAL_SYMBOL) {
      MOID_T *i = (length == 1 ? MODE (LONG_INT) : MODE (LONGLONG_INT));
      MOID_T *r = (length == 1 ? MODE (LONG_REAL) : MODE (LONGLONG_REAL));
      SCAN_CHECK (f, p);
      if (attr != OPEN_SYMBOL) {
        monitor_error ("cast expects open-symbol", NO_TEXT);
      }
      SCAN_CHECK (f, p);
      PARSE_CHECK (f, p, 0);
      if (attr != CLOSE_SYMBOL) {
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
/* L INT or L REAL denotation */
    if (attr == INT_DENOTATION) {
      m = (length == 1 ? MODE (LONG_INT) : MODE (LONGLONG_INT));
    } else if (attr == REAL_DENOTATION) {
      m = (length == 1 ? MODE (LONG_REAL) : MODE (LONGLONG_REAL));
    } else if (attr == BITS_DENOTATION) {
      m = (length == 1 ? MODE (LONG_BITS) : MODE (LONGLONG_BITS));
    } else {
      m = NO_MOID;
    }
    if (m != NO_MOID) {
      int digits = get_mp_digits (m);
      MP_T *z;
      STACK_MP (z, p, digits);
      if (genie_string_to_value_internal (p, m, symbol, (BYTE_T *) z) == A68_FALSE) {
        diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, m);
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      z[0] = (MP_T) (INITIALISED_MASK | CONSTANT_MASK);
      push_mode (f, m);
      SCAN_CHECK (f, p);
    } else {
      monitor_error ("invalid mode", NO_TEXT);
    }
  } else if (attr == INT_DENOTATION) {
    A68_INT z;
    if (genie_string_to_value_internal (p, MODE (INT), symbol, (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, MODE (INT));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PUSH_PRIMITIVE (p, VALUE (&z), A68_INT);
    push_mode (f, MODE (INT));
    SCAN_CHECK (f, p);
  } else if (attr == REAL_DENOTATION) {
    A68_REAL z;
    if (genie_string_to_value_internal (p, MODE (REAL), symbol, (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, MODE (REAL));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PUSH_PRIMITIVE (p, VALUE (&z), A68_REAL);
    push_mode (f, MODE (REAL));
    SCAN_CHECK (f, p);
  } else if (attr == BITS_DENOTATION) {
    A68_BITS z;
    if (genie_string_to_value_internal (p, MODE (BITS), symbol, (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, MODE (BITS));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
    PUSH_PRIMITIVE (p, VALUE (&z), A68_BITS);
    push_mode (f, MODE (BITS));
    SCAN_CHECK (f, p);
  } else if (attr == ROW_CHAR_DENOTATION) {
    if (strlen (symbol) == 1) {
      PUSH_PRIMITIVE (p, symbol[0], A68_CHAR);
      push_mode (f, MODE (CHAR));
    } else {
      A68_REF z;
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      z = c_to_a_string (p, symbol, DEFAULT_WIDTH);
      GET_DESCRIPTOR (arr, tup, &z);
      BLOCK_GC_HANDLE (&z);
      BLOCK_GC_HANDLE (&(ARRAY (arr)));
      PUSH_REF (p, z);
      push_mode (f, MODE (STRING));
    }
    SCAN_CHECK (f, p);
  } else if (attr == TRUE_SYMBOL) {
    PUSH_PRIMITIVE (p, A68_TRUE, A68_BOOL);
    push_mode (f, MODE (BOOL));
    SCAN_CHECK (f, p);
  } else if (attr == FALSE_SYMBOL) {
    PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
    push_mode (f, MODE (BOOL));
    SCAN_CHECK (f, p);
  } else if (attr == NIL_SYMBOL) {
    PUSH_REF (p, nil_ref);
    push_mode (f, MODE (HIP));
    SCAN_CHECK (f, p);
  } else if (attr == REAL_SYMBOL) {
    A68_INT k;
    SCAN_CHECK (f, p);
    if (attr != OPEN_SYMBOL) {
      monitor_error ("cast expects open-symbol", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    PARSE_CHECK (f, p, 0);
    if (attr != CLOSE_SYMBOL) {
      monitor_error ("cast expects close-symbol", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    if (TOP_MODE != MODE (INT)) {
      monitor_error ("invalid cast argument mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
    }
    QUIT_ON_ERROR;
    POP_OBJECT (p, &k, A68_INT);
    PUSH_PRIMITIVE (p, (double) VALUE (&k), A68_REAL);
    TOP_MODE = MODE (REAL);
  } else if (attr == IDENTIFIER) {
    ADDR_T old_sp = stack_pointer;
    BOOL_T init;
    MOID_T *moid;
    char name[BUFFER_SIZE];
    bufcpy (name, symbol, BUFFER_SIZE);
    SCAN_CHECK (f, p);
    if (attr == OF_SYMBOL) {
      selection (f, p, name);
    } else {
      search_identifier (f, p, frame_pointer, name);
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
  } else if (attr == OPEN_SYMBOL) {
    do {
      SCAN_CHECK (f, p);
      PARSE_CHECK (f, p, 0);
    } while (attr == COMMA_SYMBOL);
    if (attr != CLOSE_SYMBOL) {
      monitor_error ("unmatched parenthesis", NO_TEXT);
    }
    SCAN_CHECK (f, p);
    call_or_slice (f, p, depth);
  } else {
    monitor_error ("invalid expression syntax", NO_TEXT);
  }
}

/*!
\brief perform assignment
\param f file number
\param p position in tree
*/

static void assign (FILE_T f, NODE_T * p)
{
  LOW_STACK_ALERT (p);
  PARSE_CHECK (f, p, 0);
  if (attr == ASSIGN_SYMBOL) {
    MOID_T *m = _m_stack[--_m_sp];
    A68_REF z;
    if (ISNT (m, REF_SYMBOL)) {
      monitor_error ("invalid destination mode", moid_to_string (m, MOID_WIDTH, NO_NODE));
    }
    QUIT_ON_ERROR;
    POP_REF (p, &z);
    CHECK_MON_REF (p, z, m);
    SCAN_CHECK (f, p);
    assign (f, p);
    QUIT_ON_ERROR;
    while (IS (TOP_MODE, REF_SYMBOL) && TOP_MODE != SUB (m)) {
      MOID_T *sub = SUB (TOP_MODE);
      A68_REF y;
      POP_REF (p, &y);
      CHECK_MON_REF (p, y, TOP_MODE);
      PUSH (p, ADDRESS (&y), MOID_SIZE (sub));
      TOP_MODE = sub;
    }
    if (TOP_MODE != SUB (m) && TOP_MODE != MODE (HIP)) {
      monitor_error ("invalid source mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
    }
    QUIT_ON_ERROR;
    POP (p, ADDRESS (&z), MOID_SIZE (TOP_MODE));
    PUSH_REF (p, z);
    TOP_MODE = m;
  }
}

/*!
\brief evaluate expression on input
\param f file number
\param p position in tree
\param str expression string
*/

static void evaluate (FILE_T f, NODE_T * p, char *str)
{
  LOW_STACK_ALERT (p);
  _m_sp = 0;
  _m_stack[0] = NO_MOID;
  pos = 0;
  bufcpy (expr, str, BUFFER_SIZE);
  SCAN_CHECK (f, p);
  QUIT_ON_ERROR;
  assign (f, p);
  if (attr != 0) {
    monitor_error ("trailing character in expression", symbol);
  }
}

/*!
\brief convert string to int
\param num number to convert
\return int value or NOT_A_NUM if we cannot
*/

static int get_num_arg (char *num, char **rest)
{
  char *end;
  int k;
  if (rest != NO_VAR) {
    *rest = NO_TEXT;
  }
  if (num == NO_TEXT) {
    return (NOT_A_NUM);
  }
  SKIP_ONE_SYMBOL (num);
  if (IS_DIGIT (num[0])) {
    RESET_ERRNO;
    k = (int) a68g_strtoul (num, &end, 10);
    if (end != num && errno == 0) {
      if (rest != NO_VAR) {
        *rest = end;
      }
      return (k);
    } else {
      monitor_error ("invalid numerical argument", strerror (errno));
      return (NOT_A_NUM);
    }
  } else {
    if (num[0] != NULL_CHAR) {
      monitor_error ("invalid numerical argument", num);
    }
    return (NOT_A_NUM);
  }
}

/*!
\brief whether item at "w" of mode "q" is initialised
\param p position in tree
\param w pointer to object
\param q moid of object
\param result whether object is initialised
\return whether mode of object is recognised
**/

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
      A68_REAL *i = (A68_REAL *) (w + ALIGNED_SIZE_OF (A68_REAL));
      initialised = (BOOL_T) (INITIALISED (r) && INITIALISED (i));
      recognised = A68_TRUE;
      break;
    }
  case MODE_LONG_INT:
  case MODE_LONG_REAL:
  case MODE_LONG_BITS:
    {
      MP_T *z = (MP_T *) w;
      initialised = (BOOL_T) ((unsigned) z[0] & INITIALISED_MASK);
      recognised = A68_TRUE;
      break;
    }
  case MODE_LONGLONG_INT:
  case MODE_LONGLONG_REAL:
  case MODE_LONGLONG_BITS:
    {
      MP_T *z = (MP_T *) w;
      initialised = (BOOL_T) ((unsigned) z[0] & INITIALISED_MASK);
      recognised = A68_TRUE;
      break;
    }
  case MODE_LONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_long_mp ());
      initialised = (BOOL_T) (((unsigned) r[0] & INITIALISED_MASK) && ((unsigned) i[0] & INITIALISED_MASK));
      recognised = A68_TRUE;
      break;
    }
  case MODE_LONGLONG_COMPLEX:
    {
      MP_T *r = (MP_T *) w;
      MP_T *i = (MP_T *) (w + size_long_mp ());
      initialised = (BOOL_T) (((unsigned) r[0] & INITIALISED_MASK) && ((unsigned) i[0] & INITIALISED_MASK));
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
      A68_REF *pipe_write = (A68_REF *) (w + ALIGNED_SIZE_OF (A68_REF));
      A68_INT *pid = (A68_INT *) (w + 2 * ALIGNED_SIZE_OF (A68_REF));
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
  return (recognised);
}

/*!
\brief show value of object
\param p position in tree
\param f file number
\param item pointer to object
\param mode mode of object
**/

void print_item (NODE_T * p, FILE_T f, BYTE_T * item, MOID_T * mode)
{
  A68_REF nil_file = nil_ref;
  reset_transput_buffer (UNFORMATTED_BUFFER);
  genie_write_standard (p, mode, item, nil_file);
  if (get_transput_buffer_index (UNFORMATTED_BUFFER) > 0) {
    if (mode == MODE (CHAR) || mode == MODE (ROW_CHAR) || mode == MODE (STRING)) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " \"%s\"", get_transput_buffer (UNFORMATTED_BUFFER)) >= 0);
      WRITE (f, output_line);
    } else {
      char *str = get_transput_buffer (UNFORMATTED_BUFFER);
      while (IS_SPACE (str[0])) {
        str++;
      }
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " %s", str) >= 0);
      WRITE (f, output_line);
    }
  } else {
    WRITE (f, CANNOT_SHOW);
  }
}

/*!
\brief indented indent_crlf
\param f file number
**/

static void indent_crlf (FILE_T f)
{
  int k;
  io_close_tty_line ();
  for (k = 0; k < tabs; k++) {
    WRITE (f, "  ");
  }
}

/*!
\brief show value of object
\param p position in tree
\param f file number
\param item pointer to object
\param mode mode of object
**/

static void show_item (FILE_T f, NODE_T * p, BYTE_T * item, MOID_T * mode)
{
  if (item == NO_BYTE || mode == NO_MOID) {
    return;
  }
  if (IS (mode, REF_SYMBOL)) {
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
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, "heap(%p)", ADDRESS (z)) >= 0);
          WRITE (STDOUT_FILENO, output_line);
          tabs++;
          show_item (f, p, ADDRESS (z), SUB (mode));
          tabs--;
        } else if (IS_IN_FRAME (z)) {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, "frame(%d)", REF_OFFSET (z)) >= 0);
          WRITE (STDOUT_FILENO, output_line);
        } else if (IS_IN_STACK (z)) {
          ASSERT (snprintf (output_line, SNPRINTF_SIZE, "stack(%d)", REF_OFFSET (z)) >= 0);
          WRITE (STDOUT_FILENO, output_line);
        }
      } else {
        WRITE (STDOUT_FILENO, NO_VALUE);
      }
    }
  } else if (mode == MODE (STRING)) {
    if (!INITIALISED ((A68_REF *) item)) {
      WRITE (STDOUT_FILENO, NO_VALUE);
    } else {
      print_item (p, f, item, mode);
    }
  } else if ((IS (mode, ROW_SYMBOL) || IS (mode, FLEX_SYMBOL)) && mode != MODE (STRING)) {
    MOID_T *deflexed = DEFLEX (mode);
    int old_tabs = tabs;
    tabs += 2;
    if (!INITIALISED ((A68_REF *) item)) {
      WRITE (STDOUT_FILENO, NO_VALUE);
    } else {
      A68_ARRAY *arr;
      A68_TUPLE *tup;
      int count = 0, act_count = 0, elems;
      GET_DESCRIPTOR (arr, tup, (A68_REF *) item);
      elems = get_row_size (tup, DIM (arr));
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, ", %d element(s)", elems) >= 0);
      WRITE (f, output_line);
      if (get_row_size (tup, DIM (arr)) != 0) {
        BYTE_T *base_addr = ADDRESS (&ARRAY (arr));
        BOOL_T done = A68_FALSE;
        initialise_internal_index (tup, DIM (arr));
        while (!done && ++count <= (max_row_elems + 1)) {
          if (count <= max_row_elems) {
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
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, " %d element(s) written (%d%%)", act_count, (int) ((100.0 * act_count) / elems)) >= 0);
        WRITE (f, output_line);
      }
    }
    tabs = old_tabs;
  } else if (IS (mode, STRUCT_SYMBOL)) {
    PACK_T *q = PACK (mode);
    tabs++;
    for (; q != NO_PACK; FORWARD (q)) {
      BYTE_T *elem = &item[OFFSET (q)];
      indent_crlf (f);
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "     %s \"%s\"", moid_to_string (MOID (q), MOID_WIDTH, NO_NODE), TEXT (q)) >= 0);
      WRITE (STDOUT_FILENO, output_line);
      show_item (f, p, elem, MOID (q));
    }
    tabs--;
  } else if (IS (mode, UNION_SYMBOL)) {
    A68_UNION *z = (A68_UNION *) item;
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, " united-moid %s", moid_to_string ((MOID_T *) (VALUE (z)), MOID_WIDTH, NO_NODE)) >= 0);
    WRITE (STDOUT_FILENO, output_line);
    show_item (f, p, &item[ALIGNED_SIZE_OF (A68_UNION)], (MOID_T *) (VALUE (z)));
  } else if (mode == MODE (SIMPLIN)) {
    A68_UNION *z = (A68_UNION *) item;
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, " united-moid %s", moid_to_string ((MOID_T *) (VALUE (z)), MOID_WIDTH, NO_NODE)) >= 0);
    WRITE (STDOUT_FILENO, output_line);
  } else if (mode == MODE (SIMPLOUT)) {
    A68_UNION *z = (A68_UNION *) item;
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, " united-moid %s", moid_to_string ((MOID_T *) (VALUE (z)), MOID_WIDTH, NO_NODE)) >= 0);
    WRITE (STDOUT_FILENO, output_line);
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
            ASSERT (snprintf (output_line, SNPRINTF_SIZE, " line %d, environ at frame(%d), locale %p", LINE_NUMBER ((NODE_T *) NODE (&BODY (z))), ENVIRON (z), (void *) LOCALE (z)) >= 0);
            WRITE (STDOUT_FILENO, output_line);
          } else {
            WRITE (STDOUT_FILENO, " cannot show value");
          }
        } else if (mode == MODE (FORMAT)) {
          A68_FORMAT *z = (A68_FORMAT *) item;
          if (z != NO_FORMAT && BODY (z) != NO_NODE) {
            ASSERT (snprintf (output_line, SNPRINTF_SIZE, " line %d, environ at frame(%d)", LINE_NUMBER (BODY (z)), ENVIRON (z)) >= 0);
            WRITE (STDOUT_FILENO, output_line);
          } else {
            monitor_error (CANNOT_SHOW, NO_TEXT);
          }
        } else if (mode == MODE (SOUND)) {
          A68_SOUND *z = (A68_SOUND *) item;
          if (z != NO_SOUND) {
            ASSERT (snprintf (output_line, SNPRINTF_SIZE, "%u channels, %u bits, %u rate, %u samples", NUM_CHANNELS (z), BITS_PER_SAMPLE (z), SAMPLE_RATE (z), NUM_SAMPLES (z)) >= 0);
            WRITE (STDOUT_FILENO, output_line);

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
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, " mode %s, %s", moid_to_string (mode, MOID_WIDTH, NO_NODE), CANNOT_SHOW) >= 0);
      WRITE (STDOUT_FILENO, output_line);
    }
  }
}

/*!
\brief overview of frame item
\param f file number
\param p position in tree
\param a68g_link current frame pointer
\param q tag
\param modif output modifier
**/

static void show_frame_item (FILE_T f, NODE_T * p, ADDR_T a68g_link, TAG_T * q, int modif)
{
  ADDR_T addr = a68g_link + FRAME_INFO_SIZE + OFFSET (q);
  ADDR_T loc = FRAME_INFO_SIZE + OFFSET (q);
  (void) p;
  indent_crlf (STDOUT_FILENO);
  if (modif != ANONYMOUS) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "     frame(%d=%d+%d) %s \"%s\"", addr, a68g_link, loc, moid_to_string (MOID (q), MOID_WIDTH, NO_NODE), NSYMBOL (NODE (q))) >= 0);
    WRITE (STDOUT_FILENO, output_line);
    show_item (f, p, FRAME_ADDRESS (addr), MOID (q));
  } else {
    switch (PRIO (q)) {
    case GENERATOR:
      {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "     frame(%d=%d+%d) LOC %s", addr, a68g_link, loc, moid_to_string (MOID (q), MOID_WIDTH, NO_NODE)) >= 0);
        WRITE (STDOUT_FILENO, output_line);
        break;
      }
    default:
      {
        ASSERT (snprintf (output_line, SNPRINTF_SIZE, "     frame(%d=%d+%d) internal %s", addr, a68g_link, loc, moid_to_string (MOID (q), MOID_WIDTH, NO_NODE)) >= 0);
        WRITE (STDOUT_FILENO, output_line);
        break;
      }
    }
    show_item (f, p, FRAME_ADDRESS (addr), MOID (q));
  }
}

/*!
\brief overview of frame items
\param f file number
\param p position in tree
\param a68g_link current frame pointer
\param q tag
\param modif output modifier
**/

static void show_frame_items (FILE_T f, NODE_T * p, ADDR_T a68g_link, TAG_T * q, int modif)
{
  (void) p;
  for (; q != NO_TAG; FORWARD (q)) {
    show_frame_item (f, p, a68g_link, q, modif);
  }
}

/*!
\brief introduce stack frame
\param f file number
\param p position in tree
\param a68g_link current frame pointer
\param printed printed item counter
**/

static void intro_frame (FILE_T f, NODE_T * p, ADDR_T a68g_link, int *printed)
{
  TABLE_T *q = TABLE (p);
  if (*printed > 0) {
    WRITELN (f, "");
  }
  (*printed)++;
  where_in_source (f, p);
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Stack frame %d at frame(%d), level=%d, size=%d bytes", FRAME_NUMBER (a68g_link), a68g_link, LEVEL (q), FRAME_INCREMENT (a68g_link) + FRAME_INFO_SIZE) >= 0);
  WRITELN (f, output_line);
}

/*!
\brief view contents of stack frame
\param f file number
\param p position in tree
\param a68g_link current frame pointer
\param printed printed item counter
**/

static void show_stack_frame (FILE_T f, NODE_T * p, ADDR_T a68g_link, int *printed)
{
/* show the frame starting at frame pointer 'a68g_link', using symbol table from p as a map */
  if (p != NO_NODE) {
    TABLE_T *q = TABLE (p);
    intro_frame (f, p, a68g_link, printed);
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Dynamic link=frame(%d), static link=frame(%d), parameters=frame(%d)", FRAME_DYNAMIC_LINK (a68g_link), FRAME_STATIC_LINK (a68g_link), FRAME_PARAMETERS (a68g_link)) >= 0);
    WRITELN (STDOUT_FILENO, output_line);
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Procedure frame=%s", (FRAME_PROC_FRAME (a68g_link) ? "yes" : "no")) >= 0);
    WRITELN (STDOUT_FILENO, output_line);
#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
    if (pthread_equal (FRAME_THREAD_ID (a68g_link), main_thread_id) != 0) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "In main thread") >= 0);
    } else {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Not in main thread") >= 0);
    }
    WRITELN (STDOUT_FILENO, output_line);
#endif
    show_frame_items (f, p, a68g_link, IDENTIFIERS (q), IDENTIFIER);
    show_frame_items (f, p, a68g_link, OPERATORS (q), OPERATOR);
    show_frame_items (f, p, a68g_link, ANONYMOUS (q), ANONYMOUS);
  }
}

/*!
\brief shows lines around the line where 'p' is at
\param f file number
\param p position in tree
\param n first line number
\param m last line number
**/

static void list (FILE_T f, NODE_T * p, int n, int m)
{
  if (p != NO_NODE) {
    if (m == 0) {
      LINE_T *r = LINE (INFO (p));
      LINE_T *l = TOP_LINE (&program);
      for (; l != NO_LINE; FORWARD (l)) {
        if (NUMBER (l) > 0 && abs (NUMBER (r) - NUMBER (l)) <= n) {
          write_source_line (f, l, NO_NODE, A68_TRUE);
        }
      }
    } else {
      LINE_T *l = TOP_LINE (&program);
      for (; l != NO_LINE; FORWARD (l)) {
        if (NUMBER (l) > 0 && NUMBER (l) >= n && NUMBER (l) <= m) {
          write_source_line (f, l, NO_NODE, A68_TRUE);
        }
      }
    }
  }
}

/*!
\brief overview of the heap
\param f file number
\param p position in tree
\param z handle where to start
\param top maximum size
\param n number of handles to print
**/

void show_heap (FILE_T f, NODE_T * p, A68_HANDLE * z, int top, int n)
{
  int k = 0, m = n, sum = 0;
  (void) p;
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "size=%d available=%d garbage collections=%d", heap_size, heap_available (), garbage_collects) >= 0);
  WRITELN (f, output_line);
  for (; z != NO_HANDLE; FORWARD (z), k++) {
    if (n > 0 && sum <= top) {
      n--;
      indent_crlf (f);
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "heap(%p+%d) %s", POINTER (z), SIZE (z), moid_to_string (MOID (z), MOID_WIDTH, NO_NODE)) >= 0);
      WRITE (f, output_line);
      sum += SIZE (z);
    }
  }
  ASSERT (snprintf (output_line, SNPRINTF_SIZE, "printed %d out of %d handles", m, k) >= 0);
  WRITELN (f, output_line);
}

/*!
\brief search current frame and print it
\param f file number
\param a68g_link current frame pointer
**/

void stack_dump_current (FILE_T f, ADDR_T a68g_link)
{
  if (a68g_link > 0) {
    int dynamic_a68g_link = FRAME_DYNAMIC_LINK (a68g_link);
    NODE_T *p = FRAME_TREE (a68g_link);
    if (p != NO_NODE && LEVEL (TABLE (p)) > 3) {
      if (FRAME_NUMBER (a68g_link) == current_frame) {
        int printed = 0;
        show_stack_frame (f, p, a68g_link, &printed);
      } else {
        stack_dump_current (f, dynamic_a68g_link);
      }
    }
  }
}

/*!
\brief overview of the stack
\param f file number
\param a68g_link current frame pointer
\param depth number of frames left to print
\param printed counts items printed
**/

void stack_a68g_link_dump (FILE_T f, ADDR_T a68g_link, int depth, int *printed)
{
  if (depth > 0 && a68g_link > 0) {
    NODE_T *p = FRAME_TREE (a68g_link);
    if (p != NO_NODE && LEVEL (TABLE (p)) > 3) {
      show_stack_frame (f, p, a68g_link, printed);
      stack_a68g_link_dump (f, FRAME_STATIC_LINK (a68g_link), depth - 1, printed);
    }
  }
}

/*!
\brief overview of the stack
\param f file number
\param a68g_link current frame pointer
\param depth number of frames left to print
\param printed counts items printed
**/

void stack_dump (FILE_T f, ADDR_T a68g_link, int depth, int *printed)
{
  if (depth > 0 && a68g_link > 0) {
    NODE_T *p = FRAME_TREE (a68g_link);
    if (p != NO_NODE && LEVEL (TABLE (p)) > 3) {
      show_stack_frame (f, p, a68g_link, printed);
      stack_dump (f, FRAME_DYNAMIC_LINK (a68g_link), depth - 1, printed);
    }
  }
}

/*!
\brief overview of the stack
\param f file number
\param a68g_link current frame pointer
\param depth number of frames left to print
\param printed counts items printed
**/

void stack_trace (FILE_T f, ADDR_T a68g_link, int depth, int *printed)
{
  if (depth > 0 && a68g_link > 0) {
    int dynamic_a68g_link = FRAME_DYNAMIC_LINK (a68g_link);
    if (FRAME_PROC_FRAME (a68g_link)) {
      NODE_T *p = FRAME_TREE (a68g_link);
      show_stack_frame (f, p, a68g_link, printed);
      stack_trace (f, dynamic_a68g_link, depth - 1, printed);
    } else {
      stack_trace (f, dynamic_a68g_link, depth, printed);
    }
  }
}

/*!
\brief examine tags
\param f file number
\param p position in tree
\param a68g_link current frame pointer
\param q tag
\param sym symbol name
**/

void examine_tags (FILE_T f, NODE_T * p, ADDR_T a68g_link, TAG_T * q, char *sym, int *printed)
{
  for (; q != NO_TAG; FORWARD (q)) {
    if (NODE (q) != NO_NODE && strcmp (NSYMBOL (NODE (q)), sym) == 0) {
      intro_frame (f, p, a68g_link, printed);
      show_frame_item (f, p, a68g_link, q, PRIO (q));
    }
  }
}

/*!
\brief search symbol in stack
\param f file number
\param a68g_link current frame pointer
\param sym symbol name
**/

void examine_stack (FILE_T f, ADDR_T a68g_link, char *sym, int *printed)
{
  if (a68g_link > 0) {
    int dynamic_a68g_link = FRAME_DYNAMIC_LINK (a68g_link);
    NODE_T *p = FRAME_TREE (a68g_link);
    if (p != NO_NODE) {
      TABLE_T *q = TABLE (p);
      examine_tags (f, p, a68g_link, IDENTIFIERS (q), sym, printed);
      examine_tags (f, p, a68g_link, OPERATORS (q), sym, printed);
    }
    examine_stack (f, dynamic_a68g_link, sym, printed);
  }
}

/*!
\brief set or reset breakpoints
\param p position in tree
\param set mask indicating what to set
\param is_set to check whether breakpoint is already set
\param expr expression associated with breakpoint
**/

void change_breakpoints (NODE_T * p, unsigned set, int num, BOOL_T * is_set, char *loc_expr)
{
  for (; p != NO_NODE; FORWARD (p)) {
    change_breakpoints (SUB (p), set, num, is_set, loc_expr);
    if (set == BREAKPOINT_MASK) {
      if (LINE_NUMBER (p) == num && (STATUS_TEST (p, INTERRUPTIBLE_MASK)) && num != 0) {
        STATUS_SET (p, BREAKPOINT_MASK);
        if (EXPR (INFO (p)) != NO_TEXT) {
          free (EXPR (INFO (p)));
        }
        EXPR (INFO (p)) = loc_expr;
        *is_set = A68_TRUE;
      }
    } else if (set == BREAKPOINT_TEMPORARY_MASK) {
      if (LINE_NUMBER (p) == num && (STATUS_TEST (p, INTERRUPTIBLE_MASK)) && num != 0) {
        STATUS_SET (p, BREAKPOINT_TEMPORARY_MASK);
        if (EXPR (INFO (p)) != NO_TEXT) {
          free (EXPR (INFO (p)));
        }
        EXPR (INFO (p)) = loc_expr;
        *is_set = A68_TRUE;
      }
    } else if (set == NULL_MASK) {
      if (LINE_NUMBER (p) != num) {
        STATUS_CLEAR (p, (BREAKPOINT_MASK | BREAKPOINT_TEMPORARY_MASK));
        if (EXPR (INFO (p)) == NO_TEXT) {
          free (EXPR (INFO (p)));
        }
        EXPR (INFO (p)) = NO_TEXT;
      } else if (num == 0) {
        STATUS_CLEAR (p, (BREAKPOINT_MASK | BREAKPOINT_TEMPORARY_MASK));
        if (EXPR (INFO (p)) != NO_TEXT) {
          free (EXPR (INFO (p)));
        }
        EXPR (INFO (p)) = NO_TEXT;
      }
    }
  }
}

/*!
\brief list breakpoints
\param p position in tree
\param listed counts listed items
**/

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

/*!
\brief execute monitor command
\param p position in tree
\param cmd command text
\return whether execution continues
**/

static BOOL_T single_stepper (NODE_T * p, char *cmd)
{
  mon_errors = 0;
  errno = 0;
  if (strlen (cmd) == 0) {
    return (A68_FALSE);
  }
  while (IS_SPACE (cmd[strlen (cmd) - 1])) {
    cmd[strlen (cmd) - 1] = NULL_CHAR;
  }
  if (match_string (cmd, "CAlls", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    int printed = 0;
    if (k > 0) {
      stack_trace (STDOUT_FILENO, frame_pointer, k, &printed);
    } else if (k == 0) {
      stack_trace (STDOUT_FILENO, frame_pointer, 3, &printed);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "Continue", NULL_CHAR) || match_string (cmd, "Resume", NULL_CHAR)) {
    do_confirm_exit = A68_TRUE;
    return (A68_TRUE);
  } else if (match_string (cmd, "DO", BLANK_CHAR) || match_string (cmd, "EXEC", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] != NULL_CHAR) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "return code %d", system (sym)) >= 0);
      WRITELN (STDOUT_FILENO, output_line);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "ELems", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    if (k > 0) {
      max_row_elems = k;
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "Evaluate", BLANK_CHAR) || match_string (cmd, "X", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] != NULL_CHAR) {
      ADDR_T old_sp = stack_pointer;
      evaluate (STDOUT_FILENO, p, sym);
      if (mon_errors == 0 && _m_sp > 0) {
        MOID_T *res;
        BOOL_T cont = A68_TRUE;
        while (cont) {
          res = _m_stack[0];
          WRITELN (STDOUT_FILENO, "(");
          WRITE (STDOUT_FILENO, moid_to_string (res, MOID_WIDTH, NO_NODE));
          WRITE (STDOUT_FILENO, ")");
          show_item (STDOUT_FILENO, p, STACK_ADDRESS (old_sp), res);
          cont = (BOOL_T) (IS (res, REF_SYMBOL) && !IS_NIL (*(A68_REF *) STACK_ADDRESS (old_sp)));
          if (cont) {
            A68_REF z;
            POP_REF (p, &z);
            _m_stack[0] = SUB (_m_stack[0]);
            PUSH (p, ADDRESS (&z), MOID_SIZE (_m_stack[0]));
          }
        }
      } else {
        monitor_error (CANNOT_SHOW, NO_TEXT);
      }
      stack_pointer = old_sp;
      _m_sp = 0;
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "EXamine", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] != NULL_CHAR && (IS_LOWER (sym[0]) || IS_UPPER (sym[0]))) {
      int printed = 0;
      examine_stack (STDOUT_FILENO, frame_pointer, sym, &printed);
      if (printed == 0) {
        monitor_error ("tag not found", sym);
      }
    } else {
      monitor_error ("tag expected", NO_TEXT);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "EXIt", NULL_CHAR) || match_string (cmd, "HX", NULL_CHAR) || match_string (cmd, "Quit", NULL_CHAR) || strcmp (cmd, LOGOUT_STRING) == 0) {
    if (confirm_exit ()) {
      exit_genie (p, A68_RUNTIME_ERROR + A68_FORCE_QUIT);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "Frame", NULL_CHAR)) {
    if (current_frame == 0) {
      int printed = 0;
      stack_dump (STDOUT_FILENO, frame_pointer, 1, &printed);
    } else {
      stack_dump_current (STDOUT_FILENO, frame_pointer);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "Frame", BLANK_CHAR)) {
    int n = get_num_arg (cmd, NO_VAR);
    current_frame = (n > 0 ? n : 0);
    stack_dump_current (STDOUT_FILENO, frame_pointer);
    return (A68_FALSE);
  } else if (match_string (cmd, "HEAp", BLANK_CHAR)) {
    int top = get_num_arg (cmd, NO_VAR);
    if (top <= 0) {
      top = heap_size;
    }
#if (defined HAVE_TERM_H && defined HAVE_LIBTERMCAP)
    {
      char *mon_tty_type = getenv ("TERM");
      int term_lines;
      if (mon_tty_type == NO_TEXT) {
        term_lines = 24;
      } else if (tgetent (term_buffer, mon_tty_type) < 0) {
        term_lines = 24;
      } else {
        term_lines = tgetnum ("li");
      }
      show_heap (STDOUT_FILENO, p, busy_handles, top, term_lines - 4);
    }
#else
    show_heap (STDOUT_FILENO, p, busy_handles, top, 20);
#endif
    return (A68_FALSE);
  } else if (match_string (cmd, "APropos", NULL_CHAR) || match_string (cmd, "Help", NULL_CHAR) || match_string (cmd, "INfo", NULL_CHAR)) {
    apropos (STDOUT_FILENO, NO_TEXT, "monitor");
    return (A68_FALSE);
  } else if (match_string (cmd, "APropos", BLANK_CHAR) || match_string (cmd, "Help", BLANK_CHAR) || match_string (cmd, "INfo", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    apropos (STDOUT_FILENO, NO_TEXT, sym);
    return (A68_FALSE);
  } else if (match_string (cmd, "HT", NULL_CHAR)) {
    halt_typing = A68_TRUE;
    do_confirm_exit = A68_TRUE;
    return (A68_TRUE);
  } else if (match_string (cmd, "RT", NULL_CHAR)) {
    halt_typing = A68_FALSE;
    do_confirm_exit = A68_TRUE;
    return (A68_TRUE);
  } else if (match_string (cmd, "Breakpoint", BLANK_CHAR)) {
    char *sym = cmd;
    SKIP_ONE_SYMBOL (sym);
    if (sym[0] == NULL_CHAR) {
      int listed = 0;
      list_breakpoints (TOP_NODE (&program), &listed);
      if (listed == 0) {
        WRITELN (STDOUT_FILENO, "No breakpoints set");
      }
      if (watchpoint_expression != NO_TEXT) {
        WRITELN (STDOUT_FILENO, "Watchpoint condition \"");
        WRITE (STDOUT_FILENO, watchpoint_expression);
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
        change_breakpoints (TOP_NODE (&program), BREAKPOINT_MASK, k, &set, NULL);
        if (set == A68_FALSE) {
          monitor_error ("cannot set breakpoint in that line", NO_TEXT);
        }
      } else if (match_string (mod, "IF", BLANK_CHAR)) {
        char *cexpr = mod;
        BOOL_T set = A68_FALSE;
        SKIP_ONE_SYMBOL (cexpr);
        change_breakpoints (TOP_NODE (&program), BREAKPOINT_MASK, k, &set, new_string (cexpr));
        if (set == A68_FALSE) {
          monitor_error ("cannot set breakpoint in that line", NO_TEXT);
        }
      } else if (match_string (mod, "Clear", NULL_CHAR)) {
        change_breakpoints (TOP_NODE (&program), NULL_MASK, k, NULL, NULL);
      } else {
        monitor_error ("invalid breakpoint command", NO_TEXT);
      }
    } else if (match_string (sym, "List", NULL_CHAR)) {
      int listed = 0;
      list_breakpoints (TOP_NODE (&program), &listed);
      if (listed == 0) {
        WRITELN (STDOUT_FILENO, "No breakpoints set");
      }
      if (watchpoint_expression != NO_TEXT) {
        WRITELN (STDOUT_FILENO, "Watchpoint condition \"");
        WRITE (STDOUT_FILENO, watchpoint_expression);
        WRITE (STDOUT_FILENO, "\"");
      } else {
        WRITELN (STDOUT_FILENO, "No watchpoint expression set");
      }
    } else if (match_string (sym, "Watch", BLANK_CHAR)) {
      char *cexpr = sym;
      SKIP_ONE_SYMBOL (cexpr);
      if (watchpoint_expression != NO_TEXT) {
        free (watchpoint_expression);
        watchpoint_expression = NO_TEXT;
      }
      watchpoint_expression = new_string (cexpr);
      change_masks (TOP_NODE (&program), BREAKPOINT_WATCH_MASK, A68_TRUE);
    } else if (match_string (sym, "Clear", BLANK_CHAR)) {
      char *mod = sym;
      SKIP_ONE_SYMBOL (mod);
      if (mod[0] == NULL_CHAR) {
        change_breakpoints (TOP_NODE (&program), NULL_MASK, 0, NULL, NULL);
        if (watchpoint_expression != NO_TEXT) {
          free (watchpoint_expression);
          watchpoint_expression = NO_TEXT;
        }
        change_masks (TOP_NODE (&program), BREAKPOINT_WATCH_MASK, A68_FALSE);
      } else if (match_string (mod, "ALL", NULL_CHAR)) {
        change_breakpoints (TOP_NODE (&program), NULL_MASK, 0, NULL, NULL);
        if (watchpoint_expression != NO_TEXT) {
          free (watchpoint_expression);
          watchpoint_expression = NO_TEXT;
        }
        change_masks (TOP_NODE (&program), BREAKPOINT_WATCH_MASK, A68_FALSE);
      } else if (match_string (mod, "Breakpoints", NULL_CHAR)) {
        change_breakpoints (TOP_NODE (&program), NULL_MASK, 0, NULL, NULL);
      } else if (match_string (mod, "Watchpoint", NULL_CHAR)) {
        if (watchpoint_expression != NO_TEXT) {
          free (watchpoint_expression);
          watchpoint_expression = NO_TEXT;
        }
        change_masks (TOP_NODE (&program), BREAKPOINT_WATCH_MASK, A68_FALSE);
      } else {
        monitor_error ("invalid breakpoint command", NO_TEXT);
      }
    } else {
      monitor_error ("invalid breakpoint command", NO_TEXT);
    }
    return (A68_FALSE);
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
    return (A68_FALSE);
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
      bufcpy (prompt, sym, BUFFER_SIZE);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "RERun", NULL_CHAR) || match_string (cmd, "REStart", NULL_CHAR)) {
    if (confirm_exit ()) {
      exit_genie (p, A68_RERUN);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "RESET", NULL_CHAR)) {
    if (confirm_exit ()) {
      change_breakpoints (TOP_NODE (&program), NULL_MASK, 0, NULL, NULL);
      if (watchpoint_expression != NO_TEXT) {
        free (watchpoint_expression);
        watchpoint_expression = NO_TEXT;
      }
      change_masks (TOP_NODE (&program), BREAKPOINT_WATCH_MASK, A68_FALSE);
      exit_genie (p, A68_RERUN);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "LINk", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    int printed = 0;
    if (k > 0) {
      stack_a68g_link_dump (STDOUT_FILENO, frame_pointer, k, &printed);
    } else if (k == NOT_A_NUM) {
      stack_a68g_link_dump (STDOUT_FILENO, frame_pointer, 3, &printed);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "STAck", BLANK_CHAR) || match_string (cmd, "BT", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    int printed = 0;
    if (k > 0) {
      stack_dump (STDOUT_FILENO, frame_pointer, k, &printed);
    } else if (k == NOT_A_NUM) {
      stack_dump (STDOUT_FILENO, frame_pointer, 3, &printed);
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "Next", NULL_CHAR)) {
    change_masks (TOP_NODE (&program), BREAKPOINT_TEMPORARY_MASK, A68_TRUE);
    do_confirm_exit = A68_FALSE;
    break_proc_level = PROCEDURE_LEVEL (INFO (p));
    return (A68_TRUE);
  } else if (match_string (cmd, "STEp", NULL_CHAR)) {
    change_masks (TOP_NODE (&program), BREAKPOINT_TEMPORARY_MASK, A68_TRUE);
    do_confirm_exit = A68_FALSE;
    return (A68_TRUE);
  } else if (match_string (cmd, "FINish", NULL_CHAR) || match_string (cmd, "OUT", NULL_CHAR)) {
    finish_frame_pointer = FRAME_PARAMETERS (frame_pointer);
    do_confirm_exit = A68_FALSE;
    return (A68_TRUE);
  } else if (match_string (cmd, "Until", BLANK_CHAR)) {
    int k = get_num_arg (cmd, NO_VAR);
    if (k > 0) {
      BOOL_T set = A68_FALSE;
      change_breakpoints (TOP_NODE (&program), BREAKPOINT_TEMPORARY_MASK, k, &set, NULL);
      if (set == A68_FALSE) {
        monitor_error ("cannot set breakpoint in that line", NO_TEXT);
        return (A68_FALSE);
      }
      do_confirm_exit = A68_FALSE;
      return (A68_TRUE);
    } else {
      monitor_error ("line number expected", NO_TEXT);
      return (A68_FALSE);
    }
  } else if (match_string (cmd, "Where", NULL_CHAR)) {
    WIS (p);
    return (A68_FALSE);
  } else if (strcmp (cmd, "?") == 0) {
    apropos (STDOUT_FILENO, prompt, "monitor");
    return (A68_FALSE);
  } else if (match_string (cmd, "Sizes", NULL_CHAR)) {
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Frame stack pointer=%d available=%d", frame_pointer, frame_stack_size - frame_pointer) >= 0);
    WRITELN (STDOUT_FILENO, output_line);
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Expression stack pointer=%d available=%d", stack_pointer, expr_stack_size - stack_pointer) >= 0);
    WRITELN (STDOUT_FILENO, output_line);
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Heap size=%d available=%d", heap_size, heap_available ()) >= 0);
    WRITELN (STDOUT_FILENO, output_line);
    ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Garbage collections=%d", garbage_collects) >= 0);
    WRITELN (STDOUT_FILENO, output_line);
    return (A68_FALSE);
  } else if (match_string (cmd, "XRef", NULL_CHAR)) {
    int k = LINE_NUMBER (p);
    LINE_T *line = TOP_LINE (&program);
    for (; line != NO_LINE; FORWARD (line)) {
      if (NUMBER (line) > 0 && NUMBER (line) == k) {
        list_source_line (STDOUT_FILENO, line, A68_TRUE);
      }
    }
    return (A68_FALSE);
  } else if (match_string (cmd, "XRef", BLANK_CHAR)) {
    LINE_T *line = TOP_LINE (&program);
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
    return (A68_FALSE);
  } else if (strlen (cmd) == 0) {
    return (A68_FALSE);
  } else {
    monitor_error ("unrecognised command", NO_TEXT);
    return (A68_FALSE);
  }
}

/*!
\brief evaluate conditional breakpoint expression
\param p position in tree
\return whether expression evaluates to TRUE
**/

static BOOL_T evaluate_breakpoint_expression (NODE_T * p)
{
  ADDR_T top_sp = stack_pointer;
  volatile BOOL_T res = A68_FALSE;
  UP_BLOCK_GC;
  mon_errors = 0;
  if (EXPR (INFO (p)) != NO_TEXT) {
    evaluate (STDOUT_FILENO, p, EXPR (INFO (p)));
    if (_m_sp != 1 || mon_errors != 0) {
      mon_errors = 0;
      monitor_error ("deleted invalid breakpoint expression", NO_TEXT);
      if (EXPR (INFO (p)) != NO_TEXT) {
        free (EXPR (INFO (p)));
      }
      EXPR (INFO (p)) = expr;
      res = A68_TRUE;
    } else if (TOP_MODE == MODE (BOOL)) {
      A68_BOOL z;
      POP_OBJECT (p, &z, A68_BOOL);
      res = (BOOL_T) (STATUS (&z) == INITIALISED_MASK && VALUE (&z) == A68_TRUE);
    } else {
      monitor_error ("deleted invalid breakpoint expression yielding mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
      if (EXPR (INFO (p)) != NO_TEXT) {
        free (EXPR (INFO (p)));
      }
      EXPR (INFO (p)) = expr;
      res = A68_TRUE;
    }
  }
  stack_pointer = top_sp;
  DOWN_BLOCK_GC;
  return (res);
}

/*!
\brief evaluate conditional watchpoint expression
\return whether expression evaluates to TRUE
**/

static BOOL_T evaluate_watchpoint_expression (NODE_T * p)
{
  ADDR_T top_sp = stack_pointer;
  volatile BOOL_T res = A68_FALSE;
  UP_BLOCK_GC;
  mon_errors = 0;
  if (watchpoint_expression != NO_TEXT) {
    evaluate (STDOUT_FILENO, p, watchpoint_expression);
    if (_m_sp != 1 || mon_errors != 0) {
      mon_errors = 0;
      monitor_error ("deleted invalid watchpoint expression", NO_TEXT);
      if (watchpoint_expression != NO_TEXT) {
        free (watchpoint_expression);
        watchpoint_expression = NO_TEXT;
      }
      res = A68_TRUE;
    }
    if (TOP_MODE == MODE (BOOL)) {
      A68_BOOL z;
      POP_OBJECT (p, &z, A68_BOOL);
      res = (BOOL_T) (STATUS (&z) == INITIALISED_MASK && VALUE (&z) == A68_TRUE);
    } else {
      monitor_error ("deleted invalid watchpoint expression yielding mode", moid_to_string (TOP_MODE, MOID_WIDTH, NO_NODE));
      if (watchpoint_expression != NO_TEXT) {
        free (watchpoint_expression);
        watchpoint_expression = NO_TEXT;
      }
      res = A68_TRUE;
    }
  }
  stack_pointer = top_sp;
  DOWN_BLOCK_GC;
  return (res);
}

/*!
\brief execute monitor
\param p position in tree
**/

void single_step (NODE_T * p, unsigned mask)
{
  volatile BOOL_T do_cmd = A68_TRUE;
  ADDR_T top_sp = stack_pointer;
  if (LINE_NUMBER (p) == 0) {
    return;
  }
#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES)
  genie_curses_end (NO_NODE);
#endif
  if (mask == (unsigned) BREAKPOINT_ERROR_MASK) {
    WRITELN (STDOUT_FILENO, "Monitor entered after an error");
    WIS ((p));
  } else if ((mask & BREAKPOINT_INTERRUPT_MASK) != 0) {
    WRITELN (STDOUT_FILENO, NEWLINE_STRING);
    WIS ((p));
    if (do_confirm_exit && confirm_exit ()) {
      exit_genie ((p), A68_RUNTIME_ERROR + A68_FORCE_QUIT);
    }
  } else if ((mask & BREAKPOINT_MASK) != 0) {
    if (EXPR (INFO (p)) != NO_TEXT) {
      if (!evaluate_breakpoint_expression (p)) {
        return;
      }
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Breakpoint (%s)", EXPR (INFO (p))) >= 0);
    } else {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Breakpoint") >= 0);
    }
    WRITELN (STDOUT_FILENO, output_line);
    WIS (p);
  } else if ((mask & BREAKPOINT_TEMPORARY_MASK) != 0) {
    if (break_proc_level != 0 && PROCEDURE_LEVEL (INFO (p)) > break_proc_level) {
      return;
    }
    change_masks (TOP_NODE (&program), BREAKPOINT_TEMPORARY_MASK, A68_FALSE);
    WRITELN (STDOUT_FILENO, "Temporary breakpoint (now removed)");
    WIS (p);
  } else if ((mask & BREAKPOINT_WATCH_MASK) != 0) {
    if (!evaluate_watchpoint_expression (p)) {
      return;
    }
    if (watchpoint_expression != NO_TEXT) {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Watchpoint (%s)", watchpoint_expression) >= 0);
    } else {
      ASSERT (snprintf (output_line, SNPRINTF_SIZE, "Watchpoint (now removed)") >= 0);
    }
    WRITELN (STDOUT_FILENO, output_line);
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
#if (defined HAVE_PTHREAD_H && defined HAVE_LIBPTHREAD)
  if (is_main_thread ()) {
    WRITELN (STDOUT_FILENO, "This is the main thread");
  } else {
    WRITELN (STDOUT_FILENO, "This is not the main thread");
  }
#endif
/* Entry into the monitor */
  if (prompt_set == A68_FALSE) {
    bufcpy (prompt, "(a68g) ", BUFFER_SIZE);
    prompt_set = A68_TRUE;
  }
  in_monitor = A68_TRUE;
  UP_BLOCK_GC;
  break_proc_level = 0;
  change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_FALSE);
  STATUS_CLEAR (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK);
  while (do_cmd) {
    char *cmd;
    stack_pointer = top_sp;
    io_close_tty_line ();
    while (strlen (cmd = read_string_from_tty (prompt)) == 0) {;
    }
    if (TO_UCHAR (cmd[0]) == TO_UCHAR (EOF_CHAR)) {
      bufcpy (cmd, LOGOUT_STRING, BUFFER_SIZE);
      WRITE (STDOUT_FILENO, LOGOUT_STRING);
      WRITE (STDOUT_FILENO, NEWLINE_STRING);
    }
    _m_sp = 0;
    do_cmd = (BOOL_T) (!single_stepper (p, cmd));
  }
  stack_pointer = top_sp;
  in_monitor = A68_FALSE;
  DOWN_BLOCK_GC;
  if (mask == (unsigned) BREAKPOINT_ERROR_MASK) {
    WRITELN (STDOUT_FILENO, "Continuing from an error might corrupt things");
    single_step (p, (unsigned) BREAKPOINT_ERROR_MASK);
  } else {
    WRITELN (STDOUT_FILENO, "Continuing ...");
    WRITELN (STDOUT_FILENO, "");
  }
}

/*!
\brief PROC debug = VOID
\param p position in tree
**/

void genie_debug (NODE_T * p)
{
  single_step (p, BREAKPOINT_INTERRUPT_MASK);
}

/*!
\brief PROC break = VOID
\param p position in tree
**/

void genie_break (NODE_T * p)
{
  (void) p;
  change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);
}

/*!
\brief PROC evaluate = (STRING) STRING
\param p position in tree
*/

void genie_evaluate (NODE_T * p)
{
  A68_REF u, v;
  volatile ADDR_T top_sp;
  UP_BLOCK_GC;
  v = empty_string (p);
/* Pop argument */
  POP_REF (p, (A68_REF *) & u);
  top_sp = stack_pointer;
  CHECK_MON_REF (p, u, MODE (STRING));
  reset_transput_buffer (UNFORMATTED_BUFFER);
  add_a_string_transput_buffer (p, UNFORMATTED_BUFFER, (BYTE_T *) & u);
  v = c_to_a_string (p, get_transput_buffer (UNFORMATTED_BUFFER), DEFAULT_WIDTH);
/* Evaluate in the monitor */
  in_monitor = A68_TRUE;
  mon_errors = 0;
  evaluate (STDOUT_FILENO, p, get_transput_buffer (UNFORMATTED_BUFFER));
  in_monitor = A68_FALSE;
  if (_m_sp != 1) {
    monitor_error ("invalid expression", NO_TEXT);
  }
  if (mon_errors == 0) {
    MOID_T *res;
    BOOL_T cont = A68_TRUE;
    while (cont) {
      res = TOP_MODE;
      cont = (BOOL_T) (IS (res, REF_SYMBOL) && !IS_NIL (*(A68_REF *) STACK_ADDRESS (top_sp)));
      if (cont) {
        A68_REF w;
        POP_REF (p, &w);
        TOP_MODE = SUB (TOP_MODE);
        PUSH (p, ADDRESS (&w), MOID_SIZE (TOP_MODE));
      }
    }
    reset_transput_buffer (UNFORMATTED_BUFFER);
    genie_write_standard (p, TOP_MODE, STACK_ADDRESS (top_sp), nil_ref);
    v = c_to_a_string (p, get_transput_buffer (UNFORMATTED_BUFFER), DEFAULT_WIDTH);
  }
  stack_pointer = top_sp;
  PUSH_REF (p, v);
  DOWN_BLOCK_GC;
}
