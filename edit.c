/*!
\file edit.c
\brief full screen editor for Algol 68 Genie
**/

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
This is an experimental curses-based UNIX approximation of big-iron editors 
like the XEDIT/ISPF editors. It is still undocumented as it is not fully debugged.
I already use it for daily editing work, but you might still loose your work.
Don't say I did not warn you!

The editor is a very basic IDE for Algol 68 Genie, it can for instance take you
to diagnostic positions in the code.
This editor is meant for maintaining small to intermediate sized source code.
The command set is small, and there is no huge file support.
The editor supports prefix commands and text folding, like the
XEDIT/ISPF editors.

+-- Command overview ---
| target  A target as command, sets the current line
| ADD [n] Add n lines
| AGAIN   Repeat last search
| C       Change command /find/replace/ [target [n|* [m|*]]]
| CASE    Switch case of character under cursor
| CDELETE Delete to end of line
| COPY    Copy up to 1st target to after 2nd target, [n] copies
| DELETE  Delete up to [target]
| INDENT  Indent text to a column
| FILE    Save file [to target filename] and quit
| FOLD    [[TO] target] Folds lines either up TO target, or matching target.
| MESSAGE visualise a message from RUN or SYNTAX
| MOVE    target target [n]  Up to 1st target to after 2nd target, n copies
| PAGE    [[+-]n|*] Forward or backward paging
| QQUIT   Categorically quit
| READ    Insert filename after current line
| RESET   Reset prefixes
| RUN     run text with a68g
| SCALE   Put the sacle [OFF] or at line [n]
| S       Change command /find/replace/ [target [n|* [m|*]]]
| SYNTAX  alias for run --pedantic --norun
| TOGGLE  Toggle between current line and command line
| UNDO    Undo until last command that made a back-up copy
| WRITE   [target filename]
| 
| =         AGAIN, repeat last saved command
| ?         restore last saved command in the command buffer
| &cmd      execute cmd and leave it in the command buffer
|
| Targets (XEDIT syntax):
| :n          Absolute line number n
| [+]n        n lines down
| -n          n lines up
| .name       line name as entered in prefix area
| [+]*        top of file
| -*          end of file
| [+]/REGEXP/ line matching REGEXP, search forward
| -/REGEXP/   line matching REGEXP, search backward
|
| All targets can have [+k|-k] relative offset, f.i. /IF/+1
|
| For POSIX BRE regular expressions REGEXP we have:
| /regexp/          regexp must match
| /regexp/&/regexp/ both must match
| /regexp/|/regexp/ one or both must match
| /regexp/^/regexp/ one must match, but not both
|
| Prefix commands
| /    Make line the current line
| A[n] Add n new lines below this line
| C[n] Copy lines; use P (after) or Q (before) for destination
| CC   Copy block of lines marker; use P (after) or Q (before) for destination
| D[n] Delete lines
| DD   Delete block of lines marker
| I    Indent line to column [<|>][n]
| II   Indent block of lines
| J    Join with next line
| P[n] Add n copies after this line
| Q[n] Add n copies before this line
| X[n] Move lines; use P (after) or Q (before) for destination
| XX   Move block of lines marker; use P (after) or Q (before) for destination
+---
*/

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

/* Without CURSES or REGEX, we have no editor, so: */

#if (defined HAVE_CURSES_H && defined HAVE_LIBNCURSES && HAVE_REGEX_H)

#define TEXT_COLOUR 1
#define HILIGHT_COLOUR 2
#define SYSTEM_COLOUR 3

#define BACKSPACE 127
#define BLANK  "       "
#define BLOCK_SIZE 4
#define BOTSTR "* * * End of Data * * *"
#define DATE_STRING "%a %d-%b-%Y %H:%M"
#define EMPTY_STRING(s) ((s) == NULL || (s)[0] == NULL_CHAR)
#define FD_READ 0
#define FD_WRITE 1
#define HISTORY 16
#define IN_TEXT(z) ((z) != NULL && NUMBER (z) > 0)
#define IS_EOF(z) (!NOT_EOF(z))
#define IS_TOF(z) (!NOT_TOF(z))
#define MARGIN 7
#define MAX_PF 64
#define NOT_EOF(z) ((z) != NULL && NEXT (z) != NULL)
#define NOT_TOF(z) ((z) != NULL && PREVIOUS (z) != NULL)
#define PREFIX "====== "
#define PROMPT "=====> "
#define PROTECTED(s) ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cursor in protected area", (s)) >= 0)
#define SELECT(p) ((p)->select)
#define SUBST_ERROR -1
#define TAB_STOP 8
#define TEXT_WIDTH (COLS - MARGIN)
#define TOPSTR "* * * Top of Data * * *"
#define TRAILING(s) ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: trailing text", (s)) >= 0)
#define WRONG_TARGET -1

static char pf_bind[MAX_PF][BUFFER_SIZE];
static char history[HISTORY][BUFFER_SIZE];
static int histix = 0;

#define KEY_CTRL(n) TO_UCHAR ((int) n - 0x40)

#define SAVE_CURSOR(dd, curs) {\
  curs->row0 = curs->row;\
  curs->col0 = curs->col;\
  }

#define CURSOR_TO_SAVE(dd, curs) {\
  curs->row = curs->row0;\
  curs->col = curs->col0;\
  }

#define CURSOR_TO_CURRENT(dd, curs) {\
  curs->sync_line = dd->curr;\
  curs->sync_index = 0;\
  curs->sync = A68_TRUE;\
  }

#define CURSOR_TO_COMMAND(dd, curs) {\
  curs->row = dd->display.cmd_row;\
  curs->col = MARGIN;\
  curs->sync = A68_FALSE;\
  }

#define CHECK_ERRNO(cmd) {\
  if (errno != 0) {\
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s", cmd, ERROR_SPECIFICATION) >= 0);\
    bufcpy (scr->dl0, ERROR_SPECIFICATION, BUFFER_SIZE);\
    return;\
  }}

#define SKIP_WHITE(w) {\
  while ((w) != NULL && (w)[0] != NULL_CHAR && IS_SPACE ((w)[0])) {\
    (w)++;\
  }}

#define NO_ARGS(c, z) {\
  if ((z) != NULL && (z)[0] != NULL_CHAR) {\
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: unexpected argument", c) >= 0);\
    curs->row = scr->cmd_row;\
    curs->col = MARGIN;\
    return;\
  }}

#define ARGS(c, z) {\
  if ((z) == NULL || (z)[0] == NULL_CHAR) {\
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: missing argument", c) >= 0);\
    curs->row = scr->cmd_row;\
    curs->col = MARGIN;\
    return;\
  }}

#define XABEND(p, reason, info) {\
  if (p) {\
    endwin ();\
    abend ((char *) reason, (char *) info, __FILE__, __LINE__);\
  }}

typedef struct KEY KEY;
struct KEY {
  int code, trans;
  char *name;
};

/* Key substitutions */

KEY trans_tab[] = {
  {8, 263, "KEY_BACKSPACE"},
  {13, 10, "LF Line feed"},
  {529, 259, "KEY_UP"},
  {530, 260, "KEY_LEFT"},
  {531, 261, "KEY_RIGHT"},
  {532, 258, "KEY_DOWN"},
  {KEY_ENTER, 10, "LF Line feed"},
  {-1, -1, NULL}
};

/* Keys defined by curses */

KEY key_tab[] = {
  {0, 0, "NUL Null character"},
  {1, 1, "SOH Start of Header"},
  {2, 2, "STX Start of Text"},
  {3, 3, "ETX End of Text"},
  {4, 4, "EOT End of Transmission"},
  {5, 5, "ENQ Enquiry"},
  {6, 6, "ACK Acknowledgment"},
  {7, 7, "BEL Bell"},
  {8, 8, "BS Backspace"},
  {9, 9, "HT Horizontal"},
  {10, 10, "LF Line feed"},
  {11, 11, "VT Vertical Tab"},
  {12, 12, "FF Form feed"},
  {13, 13, "CR Carriage return"},
  {14, 14, "SO Shift Out"},
  {15, 15, "SI Shift In"},
  {16, 16, "DLE Data Link Escape"},
  {17, 17, "DC1 Device Control 1 XON"},
  {18, 18, "DC2 Device Control 2"},
  {19, 19, "DC3 Device Control 3 XOFF"},
  {20, 20, "DC4 Device Control 4"},
  {21, 21, "NAK Negative Acknowledgement"},
  {22, 22, "SYN Synchronous idle"},
  {23, 23, "ETB End of Transmission Block"},
  {24, 24, "CAN Cancel"},
  {25, 25, "EM End of Medium"},
  {26, 26, "SUB Substitute"},
  {27, 27, "ESC Escape"},
  {28, 28, "FS File Separator"},
  {29, 29, "GS Group Separator"},
  {30, 30, "RS Record Separator"},
  {31, 31, "US Unit Separator"},
  {257, 257, "KEY_BREAK"},
  {257, 257, "KEY_MIN"},
  {258, 258, "KEY_DOWN"},
  {259, 259, "KEY_UP"},
  {260, 260, "KEY_LEFT"},
  {261, 261, "KEY_RIGHT"},
  {262, 262, "KEY_HOME"},
  {263, 263, "KEY_BACKSPACE"},
  {264, 264, "KEY_F0"},
  {265, 265, "KEY_F1"},
  {266, 266, "KEY_F2"},
  {267, 267, "KEY_F3"},
  {268, 268, "KEY_F4"},
  {269, 269, "KEY_F5"},
  {270, 270, "KEY_F6"},
  {271, 271, "KEY_F7"},
  {272, 272, "KEY_F8"},
  {273, 273, "KEY_F9"},
  {274, 274, "KEY_F10"},
  {275, 275, "KEY_F11"},
  {276, 276, "KEY_F12"},
  {277, 277, "KEY_F13"},
  {278, 278, "KEY_F14"},
  {279, 279, "KEY_F15"},
  {280, 280, "KEY_F16"},
  {281, 281, "KEY_F17"},
  {282, 282, "KEY_F18"},
  {283, 283, "KEY_F19"},
  {284, 284, "KEY_F20"},
  {285, 285, "KEY_F21"},
  {286, 286, "KEY_F22"},
  {287, 287, "KEY_F23"},
  {288, 288, "KEY_F24"},
  {289, 289, "KEY_F25"},
  {290, 290, "KEY_F26"},
  {291, 291, "KEY_F27"},
  {292, 292, "KEY_F28"},
  {293, 293, "KEY_F29"},
  {294, 294, "KEY_F30"},
  {295, 295, "KEY_F31"},
  {296, 296, "KEY_F32"},
  {297, 297, "KEY_F33"},
  {298, 298, "KEY_F34"},
  {299, 299, "KEY_F35"},
  {300, 300, "KEY_F36"},
  {301, 301, "KEY_F37"},
  {302, 302, "KEY_F38"},
  {303, 303, "KEY_F39"},
  {304, 304, "KEY_F40"},
  {305, 305, "KEY_F41"},
  {306, 306, "KEY_F42"},
  {307, 307, "KEY_F43"},
  {308, 308, "KEY_F44"},
  {309, 309, "KEY_F45"},
  {310, 310, "KEY_F46"},
  {311, 311, "KEY_F47"},
  {312, 312, "KEY_F48"},
  {313, 313, "KEY_F49"},
  {314, 314, "KEY_F50"},
  {315, 315, "KEY_F51"},
  {316, 316, "KEY_F52"},
  {317, 317, "KEY_F53"},
  {318, 318, "KEY_F54"},
  {319, 319, "KEY_F55"},
  {320, 320, "KEY_F56"},
  {321, 321, "KEY_F57"},
  {322, 322, "KEY_F58"},
  {323, 323, "KEY_F59"},
  {324, 324, "KEY_F60"},
  {325, 325, "KEY_F61"},
  {326, 326, "KEY_F62"},
  {327, 327, "KEY_F63"},
  {328, 328, "KEY_DL"},
  {329, 329, "KEY_IL"},
  {330, 330, "KEY_DC"},
  {331, 331, "KEY_IC"},
  {332, 332, "KEY_EIC"},
  {333, 333, "KEY_CLEAR"},
  {334, 334, "KEY_EOS"},
  {335, 335, "KEY_EOL"},
  {336, 336, "KEY_SF"},
  {337, 337, "KEY_SR"},
  {338, 338, "KEY_NPAGE"},
  {339, 339, "KEY_PPAGE"},
  {340, 340, "KEY_STAB"},
  {341, 341, "KEY_CTAB"},
  {342, 342, "KEY_CATAB"},
  {343, 343, "KEY_ENTER"},
  {344, 344, "KEY_SRESET"},
  {345, 345, "KEY_RESET"},
  {346, 346, "KEY_PRINT"},
  {347, 347, "KEY_LL"},
  {348, 348, "KEY_A1"},
  {349, 349, "KEY_A3"},
  {350, 350, "KEY_B2"},
  {351, 351, "KEY_C1"},
  {352, 352, "KEY_C3"},
  {353, 353, "KEY_BTAB"},
  {354, 354, "KEY_BEG"},
  {355, 355, "KEY_CANCEL"},
  {356, 356, "KEY_CLOSE"},
  {357, 357, "KEY_COMMAND"},
  {358, 358, "KEY_COPY"},
  {359, 359, "KEY_CREATE"},
  {360, 360, "KEY_END"},
  {361, 361, "KEY_EXIT"},
  {362, 362, "KEY_FIND"},
  {363, 363, "KEY_HELP"},
  {364, 364, "KEY_MARK"},
  {365, 365, "KEY_MESSAGE"},
  {366, 366, "KEY_MOVE"},
  {367, 367, "KEY_NEXT"},
  {368, 368, "KEY_OPEN"},
  {369, 369, "KEY_OPTIONS"},
  {370, 370, "KEY_PREVIOUS"},
  {371, 371, "KEY_REDO"},
  {372, 372, "KEY_REFERENCE"},
  {373, 373, "KEY_REFRESH"},
  {374, 374, "KEY_REPLACE"},
  {375, 375, "KEY_RESTART"},
  {376, 376, "KEY_RESUME"},
  {377, 377, "KEY_SAVE"},
  {378, 378, "KEY_SBEG"},
  {379, 379, "KEY_SCANCEL"},
  {380, 380, "KEY_SCOMMAND"},
  {381, 381, "KEY_SCOPY"},
  {382, 382, "KEY_SCREATE"},
  {383, 383, "KEY_SDC"},
  {384, 384, "KEY_SDL"},
  {385, 385, "KEY_SELECT"},
  {386, 386, "KEY_SEND"},
  {387, 387, "KEY_SEOL"},
  {388, 388, "KEY_SEXIT"},
  {389, 389, "KEY_SFIND"},
  {390, 390, "KEY_SHELP"},
  {391, 391, "KEY_SHOME"},
  {392, 392, "KEY_SIC"},
  {393, 393, "KEY_SLEFT"},
  {394, 394, "KEY_SMESSAGE"},
  {395, 395, "KEY_SMOVE"},
  {396, 396, "KEY_SNEXT"},
  {397, 397, "KEY_SOPTIONS"},
  {398, 398, "KEY_SPREVIOUS"},
  {399, 399, "KEY_SPRINT"},
  {400, 400, "KEY_SREDO"},
  {401, 401, "KEY_SREPLACE"},
  {402, 402, "KEY_SRIGHT"},
  {403, 403, "KEY_SRSUME"},
  {404, 404, "KEY_SSAVE"},
  {405, 405, "KEY_SSUSPEND"},
  {406, 406, "KEY_SUNDO"},
  {407, 407, "KEY_SUSPEND"},
  {408, 408, "KEY_UNDO"},
  {409, 409, "KEY_MOUSE"},
  {410, 410, "KEY_RESIZE"},
  {511, 511, "KEY_MAX"},
  {-1, -1, NULL}
};

/* VT100 numeric keypad */

struct KEY dec_key[] = {
  {KEY_UP, KEY_UP, "\033OA"},
  {KEY_DOWN, KEY_DOWN, "\033OB"},
  {KEY_RIGHT, KEY_RIGHT, "\033OC"},
  {KEY_LEFT, KEY_LEFT, "\033OD"},
  {KEY_F0 + 1, KEY_F0 + 1, "\033OP"},
  {KEY_F0 + 2, KEY_F0 + 2, "\033OQ"},
  {KEY_F0 + 3, KEY_F0 + 3, "\033OR"},
  {KEY_F0 + 4, KEY_F0 + 4, "\033OS"},
  {KEY_F0 + 13, KEY_F0 + 13, "\033[1;2P"},
  {KEY_F0 + 14, KEY_F0 + 14, "\033[1;2Q"},
  {KEY_F0 + 15, KEY_F0 + 15, "\033[1;2R"},
  {KEY_F0 + 16, KEY_F0 + 16, "\033[1;2S"},
  {KEY_ENTER, KEY_ENTER, "\033OM"},
  {'*', '*', "\033Oj"},
  {'+', '+', "\033Ok"},
  {',', ',', "\033Ol"},
  {'-', '-', "\033Om"},
  {'.', '.', "\033On"},
  {'/', '/', "\033Oo"},
  {'0', '0', "\033Op"},
  {'1', '1', "\033Oq"},
  {'2', '2', "\033Or"},
  {'3', '3', "\033Os"},
  {'4', '4', "\033Ot"},
  {'5', '5', "\033Ou"},
  {'6', '6', "\033Ov"},
  {'7', '7', "\033Ow"},
  {'8', '8', "\033Ox"},
  {'9', '9', "\033Oy"},
  {KEY_B2, KEY_B2, "\033[E"},
  {KEY_END, KEY_END, "\033[4~"},
  {KEY_HOME, KEY_HOME, "\033[1~"},
  {-1, -1, NULL}
};

typedef struct LINE LINE;
struct LINE {
  int number, reserved;
  char precmd[MARGIN + 1];
  char *text;
  LINE *next, *previous;
  BOOL_T select;
};

typedef struct CURSOR CURSOR;
struct CURSOR {
  int row, col, row0, col0, index;
  LINE *line, *last;
  BOOL_T in_forbidden, in_prefix, in_text, in_cmd;
  BOOL_T sync;
  int sync_index;
  LINE *sync_line;
  unsigned bstate;
};

typedef struct DISPLAY DISPLAY;
struct DISPLAY {
  int scale_row, cmd_row, pf_row;
  LINE *last_line;
  char status[BUFFER_SIZE];
  char tmp_text[BUFFER_SIZE];
  char cmd[BUFFER_SIZE];
  char dl0[BUFFER_SIZE];
  CURSOR curs;
  BOOL_T ins_mode;
};

typedef struct REGEXP REGEXP;
struct REGEXP {
  BOOL_T is_compiled;
  char pattern[BUFFER_SIZE];
  regex_t compiled;
  regmatch_t *match;
  size_t num_match;
};

typedef struct DATASET DATASET;
struct DATASET {
  mode_t perms;
  char name[BUFFER_SIZE];
  char perm[BUFFER_SIZE];
  char date[BUFFER_SIZE];
  char undo[BUFFER_SIZE];
  int size, alts, tabs, num, undo_line, search;
  LINE *tof; /* top-of-file */
  BOOL_T new_file;
  BOOL_T subset;
  BOOL_T refresh;
  BOOL_T collect;
  DISPLAY display;
  LINE *curr; /* Current line, above the scale */
  LINE *bl_start, *bl_end; /* block at last copy or move */
  REGEXP targ1, targ2, find, repl;
  char oper; /* regexp operator: & or | */
  FILE_T msgs;
  ADDR_T heap_pointer;
  jmp_buf edit_exit_label;
  char *linbuf;
  int linsiz;
};

/* Forward routines */

static void edit_do_cmd (DATASET *);
static void edit_do_prefix (DATASET *);
static void edit_loop (DATASET *);
static void edit_dataset (DATASET *, int, char *, char *);
static void edit_garbage_collect (DATASET *, char *);
static void set_current (DATASET *, char *, char *);

/*
\brief store command in a cyclic buffer
\param cmd command to store
*/

static void edit_add_history (char *cmd)
{
  bufcpy (history[histix], cmd, BUFFER_SIZE); 
  histix ++;
  if (histix == HISTORY) {
    histix = 0;
  }
}

/*
\brief whether there is space on the heap
\param s bytes to allocate
\return same
**/

static BOOL_T heap_full (int as) 
{
  BOOL_T heap_up = (fixed_heap_pointer + as) >= (heap_size - MIN_MEM_SIZE);
  BOOL_T heap_down = ((int) temp_heap_pointer - (int) (fixed_heap_pointer + as)) <= MIN_MEM_SIZE;
  return (heap_up || heap_down);
}

/*
\brief allocate heap space for editor
\param dd dataset that allocates
\param s bytes to allocate
\return pointer to same
**/

static BYTE_T *edit_get_heap (DATASET *dd, size_t s)
{
  DISPLAY *scr = &(dd->display);
  BYTE_T *z;
  int as = A68_ALIGN ((int) s);
  XABEND (heap_is_fluid == A68_FALSE, ERROR_INTERNAL_CONSISTENCY, NULL);
  /* If there is no space left, we collect garbage */
  if (heap_full (as) && dd->collect) {
    edit_garbage_collect (dd, "edit");
  }
  if (heap_full (as)) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "edit: out of memory") >= 0);
    return (NULL);
  }
  /* Allocate space */
  z = HEAP_ADDRESS (fixed_heap_pointer);
  fixed_heap_pointer += as;
  return (z);
}

/*
\brief add char to line buffer
\param lbuf line buffer
\param lsiz size of line buffer
\param ch char to add
\param pos position to add ch
**/

static void add_linbuf (DATASET *dd, char ch, int pos)
{
  if (dd->linbuf == NULL || pos >= dd->linsiz - 1) {
    char *oldb = dd->linbuf;
    dd->linsiz += BUFFER_SIZE;
    dd->linbuf = (char *) edit_get_heap (dd, (size_t) (dd->linsiz));
    XABEND (dd->linbuf == NULL, "Insufficient memory", NULL);
    if (oldb == NULL) {
      (dd->linbuf)[0] = NULL_CHAR;
    } else {
      bufcpy (dd->linbuf, oldb, dd->linsiz);
    }
  }
  (dd->linbuf)[pos] = ch;
  (dd->linbuf)[pos + 1] = NULL_CHAR;
}

/* REGULAR EXPRESSION SUPPORT */

/*
\brief initialise regular expression
\param re regexp to initialise
**/

static void init_regexp (REGEXP *re)
{
  re->is_compiled = A68_FALSE;
  re->pattern[0] = NULL_CHAR;
  re->match = NULL;
  re->num_match = 0;
}

/*
\brief reset regular expression
\param re regexp to initialise
**/

static void reset_regexp (REGEXP *re)
{
  re->is_compiled = A68_FALSE;
  re->pattern[0] = NULL_CHAR;
  if (re->match != NULL) {
    free (re->match);
  }
  re->match = NULL;
  re->num_match = 0;
}

/*
\brief compile regular expression
\param dd current dataset
\param re regexp to compile
\param cmd command that calls this routine
\return return code
**/

static int compile_regexp (DATASET *dd, REGEXP *re, char *cmd)
{
  DISPLAY *scr = &(dd->display);
  int rc;
  char buffer[BUFFER_SIZE];
  re->is_compiled = A68_FALSE;
  rc = regcomp (&(re->compiled), re->pattern, REG_NEWLINE);
  if (rc != 0) {
    (void) regerror (rc, &(re->compiled), buffer, BUFFER_SIZE);
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s", cmd, buffer) >= 0);
    regfree (&(re->compiled));
    return (rc);
  } else {
    re->num_match = 1 + re->compiled.re_nsub;
    re->match = malloc ((size_t) (re->num_match * sizeof (regmatch_t)));
    if (re->match == NULL) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s", cmd, ERROR_OUT_OF_CORE) >= 0);
      regfree (&(re->compiled));
      return (-1);
    }
  }
  re->is_compiled = A68_TRUE;
  return (0);
}

/*
\brief match line to regular expression
\param dd current dataset
\param fragment whether matching the tail of a string after an earlier match
\param cmd command that calls this routine
\return whether match
**/

static BOOL_T match_regex (DATASET *dd, LINE *z, int eflags, char *cmd)
{
  DISPLAY *scr = &(dd->display);
  int rc1 = REG_NOMATCH, rc2 = REG_NOMATCH;
  char *str = TEXT (z);
/* Match first regex if present */
  if (dd->targ1.is_compiled) {
    rc1 = regexec (&(dd->targ1.compiled), str, dd->targ1.num_match, dd->targ1.match, eflags);
    if (rc1 != 0 && rc1 != REG_NOMATCH) {
      char buffer[BUFFER_SIZE];
      (void) regerror (rc1, &(dd->targ1.compiled), buffer, BUFFER_SIZE);
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s", cmd, buffer) >= 0);
      if (dd->targ1.match != NULL) {
        free (dd->targ1.match);
        dd->targ1.match = NULL;
      }
      return (A68_FALSE);
    }
  }
/* Match 2nd regex if present */
  if (dd->targ2.is_compiled) {
    rc2 = regexec (&(dd->targ2.compiled), str, dd->targ2.num_match, dd->targ2.match, eflags);
    if (rc2 != 0 && rc2 != REG_NOMATCH) {
      char buffer[BUFFER_SIZE];
      (void) regerror (rc2, &(dd->targ2.compiled), buffer, BUFFER_SIZE);
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s", cmd, buffer) >= 0);
      if (dd->targ2.match != NULL) {
        free (dd->targ2.match);
        dd->targ2.match = NULL;
      }
      return (A68_FALSE);
    }
  }
/* Form a result */
  if (dd->oper == NULL_CHAR) {
    return (rc1 != REG_NOMATCH);
  } else if (dd->oper == '|') {
    return ((rc1 != REG_NOMATCH) | (rc2 != REG_NOMATCH));
  } else if (dd->oper == '&') {
    return ((rc1 != REG_NOMATCH) & (rc2 != REG_NOMATCH));
  } else if (dd->oper == '^') {
    return ((rc1 != REG_NOMATCH) ^ (rc2 != REG_NOMATCH));
  } else {
    return (A68_FALSE);
  }
}

/*
\brief calculate positions to next tab stop
\param pos where are we now
\param tabs tab stop setting
\return number of blanks to next stop
**/

static int tab_reps (int pos, int tabs)
{
  int disp = pos % tabs;
  return (tabs - disp);
}

/*
\brief whether in a reserved row
\param dd current dataset
\return same
**/

static BOOL_T reserved_row (DATASET *dd, int row)
{
  DISPLAY *scr = &(dd->display);
  return (row == scr->cmd_row ||
          row == scr->scale_row ||
          row == scr->pf_row);
}

/*
\brief count reserved lines on screen
\param dd current dataset
\return same
**/

static int count_reserved (DATASET *dd)
{
  int k, n = 0;
  for (k = 0; k < LINES; k++) {
    if (reserved_row (dd, k)) {
      n++;
    }
  }
  return (n);
}

/*
\brief lines on screen for a line - account for tabs
\param dd current dataset
\param lin line to count
\return same
**/

static int lines_on_scr (DATASET *dd, LINE * lin)
{
  int k = 0, row = 1, col = MARGIN;
  char *txt = TEXT (lin);
  while (txt[k] != NULL_CHAR) {
    int reps, n;
    if (txt[k] == '\t') {
      reps = tab_reps (col - MARGIN, dd->tabs);
    } else {
      reps = 1;
    }
    for (n = 0; n < reps; n++) {
      if (col >= COLS) {
        row++;
        col = MARGIN;
      }
      col++;
    }
    k++;
  }
  if (col >= COLS) {
    row++;
    col = MARGIN;
  }
  return (row);
}

/*
\brief initialise curses
\param dd current dataset
**/

static void edit_init_curses (DATASET *dd)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  int res = count_reserved (dd);
  initscr ();
  XABEND (has_colors () == A68_FALSE, "edit requires colour display", NULL);
  start_color ();
/* Next works for instance on the Linux console
  if (can_change_color ()) {
    init_color (COLOR_WHITE, 1000, 1000, 1000);
    init_color (COLOR_GREEN, 600, 600, 600);
    init_color (COLOR_RED, 300, 300, 300);
  }
*/
  init_pair (TEXT_COLOUR, COLOR_GREEN, COLOR_BLACK);
  init_pair (HILIGHT_COLOUR, COLOR_WHITE, COLOR_BLACK);
  init_pair (SYSTEM_COLOUR, COLOR_RED, COLOR_BLACK);
  raw ();
  keypad (stdscr, TRUE);
  noecho ();
  nonl ();
  meta (stdscr, TRUE);
  mousemask (ALL_MOUSE_EVENTS, NULL);
  curs_set (1);
  scr->scale_row = res / 2 + (LINES - res) / 2;
  scr->cmd_row = LINES - 1;
  scr->pf_row = LINES - 2;
  curs->row = curs->col = -1;
  curs->sync = A68_FALSE;
  clear ();
  refresh ();
}

/*
\brief read a buffer from file
\param fd file to read from
\param buffer buffer to store line
\return same
**/

int get_buffer (FILE_T fd, char *buffer)
{
  int bytes;
  RESET_ERRNO;
  bytes = (int) io_read (fd, buffer, (size_t) BUFFER_SIZE);
  return (bytes);
}

/*
\brief generate a new line
\param dd current dataset
\return new line
**/

LINE *new_line (DATASET *dd)
{
  LINE *newl = (LINE *) edit_get_heap (dd, (size_t) sizeof (LINE));
  if (newl == NULL) {
    return (NULL);
  }
  newl->precmd[0] = NULL_CHAR;
  SELECT (newl) = A68_TRUE;
  NEXT (newl) = NULL;
  PREVIOUS (newl) = NULL;
  TEXT (newl) = NULL;
  NUMBER (newl) = 0;
  return (newl);
}

/*
\brief forward line, folded or not
\param z line to forward
**/

static void forward_line (LINE **z)
{
  if (*z == NULL) {
    return;
  }
  do {
    FORWARD (*z);
  } while (*z != NULL && ! (SELECT (*z) || NUMBER (*z) == 0));
}

/*
\brief backward line, folded or not
\param z line to "backward"
**/

static void backward_line (LINE **z)
{
  if (*z == NULL) {
    return;
  }
  do {
    BACKWARD (*z);
  } while (*z != NULL && ! (SELECT (*z) || NUMBER (*z) == 0));
}

/*
\brief align current line, cannot be TOF or EOF
\param dd current dataset
**/

static void align_current (DATASET *dd)
{
  if (IS_TOF (dd->curr)) {
    LINE *z = dd->curr;
    forward_line (&z);
    if (NOT_EOF (z)) {
      dd->curr = z;
    }
  } else if (IS_EOF (dd->curr)) {
    if (IS_TOF (PREVIOUS (dd->curr))) {
      dd->curr = dd->tof;
    } else {
      LINE *z = dd->curr;
      backward_line (&z);
      (dd)->curr = z;
    }
  }
}

/*
\brief generate a new string for a line
\param dd current dataset
\param l line to add string to
\param txt text to store in string
\param eat pointer to old lines, use string if fits
**/

static void new_edit_string (DATASET *dd, LINE *l, char *txt, LINE *eat)
{
  if (txt == NULL || strlen(txt) == 0) {
    l->reserved = 1;
    TEXT (l) = (char *) edit_get_heap (dd, (size_t) l->reserved);
    TEXT (l)[0] = NULL_CHAR;
    bufcpy (l->precmd, BLANK, (strlen (BLANK) + 1));
  } else {
    int res = 1 + (int) strlen (txt);
    if (res % BLOCK_SIZE > 0) {
      res += (BLOCK_SIZE - res % BLOCK_SIZE);
    }
    if (eat != NULL && eat->reserved >= res) {
      l->reserved = eat->reserved;
ABEND (l->reserved > 9999, "Strange length", NO_TEXT);
      TEXT (l) = TEXT (eat);
    } else {
      l->reserved = res;
      TEXT (l) = (char *) edit_get_heap (dd, (size_t) res);
    }
    if (TEXT (l) == NULL) {
      return;
    }
    bufcpy (TEXT (l), txt, res);
    bufcpy (l->precmd, BLANK, (strlen (BLANK) + 1));
  }
}

/*
\brief set prefix to line
\param l line to set
**/

static void set_prefix (LINE *l)
{
  bufcpy (l->precmd, BLANK, (strlen (BLANK) + 1));
}

/*
\brief reset prefixes to original state
\param dd current dataset
**/

static void edit_reset (DATASET *dd)
{
  int k = 0;
  LINE *z;
  for (z = dd->tof; z != NULL; FORWARD (z)) {
    if (NUMBER (z) != 0) {
      k++;
      NUMBER (z) = k;
    }
    set_prefix (z);
  }
  dd->size = k;
}

/*
\brief delete to end of line
\param dd current dataset
**/

static void cdelete (DATASET *dd)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  LINE *lin = curs->line;
  if (lin != NULL && curs->index < (int) strlen (TEXT (lin))) {
    TEXT (lin)[curs->index] = NULL_CHAR;
  }
}

/*
\brief split line at cursor position
\param dd current dataset
\param cmd command that calls this routine
**/

static void split_line (DATASET *dd, char *cmd)
{
/* We reset later so this routine can be repeated cheaply */
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  LINE *lin = curs->line, *newl;
  if (NEXT (lin) == NULL) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot split line", cmd) >= 0);
    return;
  }
  clear ();
  dd->bl_start = dd->bl_end = NULL;
  dd->alts++;
  dd->size++;
/* Insert a new line */
  newl = new_line (dd);
  if (newl == NULL) {
    return;
  }
  if ((curs->index < (int) strlen (TEXT (lin))) && IN_TEXT (lin)) {
    new_edit_string (dd, newl, &(TEXT (lin)[curs->index]), NULL);
    if (TEXT (newl) == NULL) {
      return;
    }
    TEXT (lin)[curs->index] = NULL_CHAR;
  } else {
    new_edit_string (dd, newl, "", NULL);
    if (TEXT (newl) == NULL) {
      return;
    }
  }
  PREVIOUS (newl) = lin;
  NEXT (newl) = NEXT (lin);
  NEXT (lin) = newl;
  PREVIOUS (NEXT (newl)) = newl;
  NUMBER (newl) = NUMBER (lin) + 1;
/* Position the cursor at the start of the new line */
  curs->sync_index = 0;
  curs->sync_line = newl;
  curs->sync = A68_TRUE;
  if (lin == scr->last_line) {
    forward_line (&dd->curr);
  }
}

/*
\brief join line with next one
\param dd current dataset
\param cmd command that calls this
**/

static void join_line (DATASET *dd, char *cmd)
{
/* We reset later so this routine can be repeated cheaply */
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  LINE *lin = curs->line, *prv, *u;
  int len, lcur, lprv;
  if (NUMBER (lin) == 0) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot join line", cmd) >= 0);
    return;
  }
  clear ();
  dd->bl_start = dd->bl_end = NULL;
  prv = PREVIOUS (lin);
  dd->alts++;
  dd->size--;
/* Join line with the previous one */
  if (prv == dd->tof) {
/* Express case */
    NEXT (dd->tof) = NEXT (lin);
    PREVIOUS (NEXT (prv)) = prv;
    curs->sync_index = 0;
    curs->sync_line = dd->tof;
    curs->sync = A68_TRUE;
    return;
  }
  lcur = (int) strlen (TEXT (lin));
  lprv = (int) strlen (TEXT (prv));
  len = lcur + lprv;
  if (prv->reserved <= len + 1) {
/* Not enough room */
    int res = len + 1;
    char *txt = TEXT (prv);
    if (res % BLOCK_SIZE > 0) {
      res += (BLOCK_SIZE - res % BLOCK_SIZE);
    }
    prv->reserved = res;
    TEXT (prv) = (char *) edit_get_heap (dd, (size_t) res);
    if (TEXT (prv) == NULL) {
      return;
    }
    bufcpy (TEXT (prv), txt, res);
  }
/* Delete the current line */
  bufcat (TEXT (prv), TEXT (lin), len + 1);
  NEXT (prv) = NEXT (lin);
  PREVIOUS (NEXT (prv)) = prv;
/* Position the cursor at the the new line on the screen */
  u = lin;
  backward_line (&u);
  if (u == NULL) {
    u = dd->tof;
  }
  if (dd->curr == lin) {
    dd->curr = u;
  }
  curs->sync_line = u;
  if (IN_TEXT (u)) {
    if (u == prv) {
      curs->sync_index = lprv;
    } else {
      curs->sync_index = (int) strlen (TEXT (u));
    }
  } else {
    curs->sync_index = 0;
  }
  curs->sync = A68_TRUE;
}

/*
\brief compose permission string
\param dd current dataset
\return same
**/

static char * perm_str (mode_t st_mode)
{
  static char perms[16];
  bufcpy (perms, "----------", 10);
  if (S_ISDIR (st_mode)) {perms[0] = 'd';};
#if defined S_ISLNK
  if (S_ISLNK (st_mode)) {perms[0] = 'l';};
#endif
  if (st_mode & S_IRUSR) {perms[1] = 'r';};
  if (st_mode & S_IWUSR) {perms[2] = 'w';};
  if (st_mode & S_IXUSR) {perms[3] = 'x';};
  if (st_mode & S_IRGRP) {perms[4] = 'r';};
  if (st_mode & S_IWGRP) {perms[5] = 'w';};
  if (st_mode & S_IXGRP) {perms[6] = 'x';};
  if (st_mode & S_IROTH) {perms[7] = 'r';};
  if (st_mode & S_IWOTH) {perms[8] = 'w';};
  if (st_mode & S_IXOTH) {perms[9] = 'x';};
  return (perms);
}

/*
\brief read a string from file
\param dd current dataset
\param cmd command that calls this routine
**/

static void edit_read_string (DATASET *dd, FILE_T fd)
{
  int bytes, posl;
  char ch;
/* Set up for reading */
  (dd->linbuf)[0] = NULL_CHAR;
  if (fd == -1) {
    return;
  }
  posl = 0;
  bytes = io_read (fd, &ch, 1);
  while (A68_TRUE) {
    if (bytes != 1) {
      return;
    } else if (ch == NEWLINE_CHAR) {
      return;
    } else {
      add_linbuf (dd, ch, posl);
      if (dd->linbuf == NULL) {
        return;
      }
      posl++;
    }
    bytes = io_read (fd, &ch, 1);
  }
}

/*
\brief read a file into a dataset
\param dd current dataset
\param cmd command that calls this routine
\param fname file name
\param eat old dataset lines to consume
**/

static void edit_read (DATASET * dd, char *cmd, char *fname, LINE *eat)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  FILE_T fd;
  int bytes, posl;
  char ch;
  LINE *curr = dd->curr;
/* Open the file */
  RESET_ERRNO;
  if ((int) strlen (fname) > 0) {
    fd = open (fname, A68_READ_ACCESS);
  } else {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot open file for reading", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  CHECK_ERRNO (cmd);
/* Set up for reading */
  posl = 0;
  bytes = io_read (fd, &ch, 1);
  while (A68_TRUE) {
    if (bytes != 1) {
      goto end;
    }
    if (ch == NEWLINE_CHAR) {
/* Link line */
      curs->line = curr;
      curs->index = (int) strlen (TEXT (curr));
      split_line (dd, cmd);
      FORWARD (curr);
      if (eat != NULL) {
        new_edit_string (dd, curr, dd->linbuf, eat);
        FORWARD (eat);
      } else {
        new_edit_string (dd, curr, dd->linbuf, curr);
      }
      if (TEXT (curr) == NULL) {
        return;
      }
/* Reinit line buffer */
      posl = 0;
      if (dd->linbuf != NULL) {
        (dd->linbuf)[0] = NULL_CHAR;
      }
    } else {
      add_linbuf (dd, ch, posl);
      if (dd->linbuf == NULL) {
        return;
      }
      posl++;
    }
    bytes = io_read (fd, &ch, 1);
  }
  end:
  edit_reset (dd);
  dd->bl_start = dd->bl_end = NULL;
  align_current (dd);
}

/*
\brief first read of a file
\param dd current dataset
\param cmd command that calls this routine
**/

static void edit_read_initial (DATASET * dd, char *cmd)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  FILE_T fd;
  LINE *curr;
  struct stat statbuf;
/* Initialisations */
  scr->cmd[0] = NULL_CHAR;
  init_regexp (&(dd->targ1));
  init_regexp (&(dd->targ2));
  init_regexp (&(dd->find));
  init_regexp (&(dd->repl));
  dd->subset = A68_FALSE;
  dd->alts = 0;
  curs->index = 0;
/* Add TOF */
  dd->tof = new_line (dd);
  if (dd->tof == NULL) {
    return;
  }
  new_edit_string (dd, dd->tof, TOPSTR, NULL);
  if (TEXT (dd->tof) == NULL) {
    return;
  }
  NUMBER (dd->tof) = 0;
  set_prefix (dd->tof);
/* Add EOF */
  curr = new_line (dd);
  if (curr == NULL) {
    return;
  }
  new_edit_string (dd, curr, BOTSTR, NULL);
  if (TEXT (curr) == NULL) {
    return;
  }
  NUMBER (curr) = 0;
  set_prefix (curr);
  PREVIOUS (curr) = dd->tof;
  NEXT (dd->tof) = curr;
  dd->curr = dd->tof;
/* Open the file */
  RESET_ERRNO;
  if ((int) strlen (dd->name) > 0) {
    fd = open (dd->name, A68_READ_ACCESS);
  } else {
    fd = -1;
  }
  if (fd == -1) {
    char datestr[BUFFER_SIZE];
    time_t rt;
    struct tm *tm;
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: creating new file", cmd) >= 0);
    dd->size = 0;
    dd->perms = A68_PROTECTION;
    dd->new_file = A68_TRUE;
    time (&rt);
    ASSERT ((tm = localtime (&rt)) != NULL);
    ASSERT ((strftime (datestr, BUFFER_SIZE, DATE_STRING, tm)) > 0);
    ASSERT (snprintf (dd->perm, SNPRINTF_SIZE, "%s", perm_str (dd->perms)) >= 0);
    ASSERT (snprintf (dd->date, SNPRINTF_SIZE, "%s", datestr) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  CHECK_ERRNO (cmd);
/* Collect file information; we display file date and permissions */
  dd->new_file = A68_FALSE;
  if (stat (dd->name, &statbuf) != -1) {
    struct tm *tm;
    char datestr[BUFFER_SIZE];
    dd->perms = statbuf.st_mode;
    ASSERT ((tm = localtime (&statbuf.st_mtime)) != NULL);
    ASSERT ((strftime (datestr, BUFFER_SIZE, DATE_STRING, tm)) > 0);
    ASSERT (snprintf (dd->perm, SNPRINTF_SIZE, "%s", perm_str (dd->perms)) >= 0);
    ASSERT (snprintf (dd->date, SNPRINTF_SIZE, "%s", datestr) >= 0);
  }
/* Set up for reading */
  edit_read (dd, cmd, dd->name, NULL);
  dd->alts = 0; /* Again, since edit_read inserts lines! */
  dd->curr = NEXT (dd->tof);
}

/*
\brief write dataset to file
\param dd current dataset
\param cmd command that calls this routine
\param fname file name
\param u first line to write
\param v write upto, but not including, this line
**/

static void edit_write (DATASET * dd, char *cmd, char *fname, LINE *u, LINE *v)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  FILE_T fd;
  LINE *curr;
/* Backwards range */
  if (NOT_EOF (v) && (NUMBER (v) < NUMBER (u))) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: backward range", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
/* Open the file */
  RESET_ERRNO;
  fd = open (fname, A68_WRITE_ACCESS, A68_PROTECTION);
  CHECK_ERRNO (cmd);
  if (fd == -1) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot open file for writing", cmd) >= 0);
    return;
  }
  for (curr = u; curr != v; FORWARD (curr)) {
    if (IN_TEXT (curr)) {
      if ((int) strlen (TEXT (curr)) > 0) {
        WRITE (fd, TEXT (curr));
      }
      if (NEXT (curr) != NULL) {
        WRITE (fd, "\n");
      }
    }
  }
  RESET_ERRNO;
  close (fd);
  CHECK_ERRNO (cmd);
}

/*
\brief write a file for recovery
\param dd current dataset
\param cmd command that calls this routine
**/

static void edit_write_undo_file (DATASET * dd, char *cmd)
{
  if ((dd->undo)[0] == NULL_CHAR) {
    return;
  }
  edit_write (dd, cmd, dd->undo, dd->tof, NULL);
  dd->undo_line = NUMBER (dd->curr);
}

/*
\brief read a file for recovery
\param dd current dataset
\param cmd command that calls this routine
**/

static void edit_read_undo_file (DATASET *dd, char *cmd)
{
  FILE_T fd;
  struct stat statbuf;
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  if ((dd->undo)[0] == NULL_CHAR) {
    return;
  }
  RESET_ERRNO;
  fd = open (dd->undo, A68_READ_ACCESS);
  if (fd == -1 || errno != 0) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot recover", cmd) >= 0);
    return;
  } else {
    LINE *eat = NULL, *curr;
    if (dd->tof != NULL) {
      eat = NEXT (dd->tof);
    }
    close (fd);
    dd->subset = A68_FALSE;
    curs->index = 0;
    dd->tof = new_line (dd);
    if (dd->tof == NULL) {
      return;
    }
    new_edit_string (dd, dd->tof, TOPSTR, NULL);
    if (TEXT (dd->tof) == NULL) {
      return;
    }
    NUMBER (dd->tof) = 0;
    set_prefix (dd->tof);
    curr = new_line (dd);
    if (curr == NULL) {
      return;
    }
    new_edit_string (dd, curr, BOTSTR, NULL);
    if (TEXT (curr) == NULL) {
      return;
    }
    NUMBER (curr) = 0;
    set_prefix (curr);
    PREVIOUS (curr) = dd->tof;
    NEXT (dd->tof) = curr;
    dd->curr = dd->tof;
    edit_read (dd, cmd, dd->undo, eat);
    if (stat (dd->undo, &statbuf) != -1) {
      struct tm *tm;
      char datestr[BUFFER_SIZE];
      dd->perms = statbuf.st_mode;
      ASSERT ((tm = localtime (&statbuf.st_mtime)) != NULL);
      ASSERT ((strftime (datestr, BUFFER_SIZE, DATE_STRING, tm)) > 0);
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: \"%s\" restored to state at %s", cmd, dd->name, datestr) >= 0);
    }
    if (remove (dd->undo) != 0) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s", cmd, ERROR_FILE_SCRATCH) >= 0);
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
    } else {
      char cmd2[BUFFER_SIZE];
      ASSERT (snprintf (cmd2, SNPRINTF_SIZE, ":%d", dd->undo_line) >= 0);
      set_current (dd, cmd, cmd2);
      align_current (dd);
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
    }
    return;
  }
}

/*
\brief garbage collector for the editor
\param dd current dataset
\param cmd current command under execution
**/

static void edit_garbage_collect (DATASET *dd, char *cmd)
{
  RESET_ERRNO;
  edit_write_undo_file (dd, cmd);
  if (errno != 0) {
    return;
  }
  fixed_heap_pointer = dd->heap_pointer;
  dd->tof = NULL;
  dd->linbuf = NULL;
  dd->linsiz = 0;
  edit_read_undo_file (dd, cmd);
  return;
}

/*
\brief put character on screen
\param row current row on screen
\param col current col on screen
\param ch character to put
\param dd current dataset
\param dd_line current text line
\param dd_index current index in text (of ch)
**/

static void putch (int row, int col, char ch, DATASET *dd, LINE *dd_line, int dd_index)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  BOOL_T forbidden = reserved_row (dd, row);
  BOOL_T text_area = (!forbidden) && col >= MARGIN;
  BOOL_T prefix_area = (!forbidden) && col < MARGIN;
  if (row < 0 || row >= LINES) {
    return;
  }
  if (IS_CNTRL (ch)) {
    ch = '~';
  }
  if (col < 0 || col >= COLS) {
    return;
  }
  if (row == scr->cmd_row && curs->row == row) {
    if (curs->col < MARGIN) {
      curs->in_forbidden = A68_TRUE;
      curs->in_text = A68_FALSE;
      curs->in_prefix = A68_FALSE;
      curs->in_cmd = A68_FALSE;
    } else {
      curs->in_cmd = A68_TRUE;
      curs->in_text = A68_FALSE;
      curs->in_prefix = A68_FALSE;
      if (curs->col == col) {
        curs->index = dd_index;
      }
    }
    curs->line = NULL;
  } else if (forbidden && curs->row == row) {
    curs->in_forbidden = A68_TRUE;
    curs->line = NULL;
  } else if (text_area && curs->sync && curs->sync_line == dd_line && curs->sync_index == dd_index) {
    curs->row = row;
    curs->col = col;
    curs->sync = A68_FALSE;
    curs->in_text = A68_TRUE;
    curs->in_prefix = A68_FALSE;
    curs->in_cmd = A68_FALSE;
    curs->index = dd_index;
    curs->line = dd_line;
  } else if (text_area && curs->row == row && curs->col == col) {
    curs->in_text = A68_TRUE;
    curs->in_prefix = A68_FALSE;
    curs->in_cmd = A68_FALSE;
    curs->index = dd_index;
    if (dd_line != NULL) {
      curs->line = dd_line;
    }
  } else if (prefix_area && curs->row == row && curs->col == col) {
    curs->in_text = A68_FALSE;
    curs->in_cmd = A68_FALSE;
    curs->in_prefix = A68_TRUE;
    curs->index = dd_index;
    if (dd_line != NULL) {
      curs->line = dd_line;
    }
  }
  mvaddch (row, col, (chtype) ch);
}

/*
\brief draw the screen
\param dd current dataset
**/

static void edit_draw (DATASET *dd)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  LINE *run, *tos;
  int row, k, virt_scal, lin_abo, lin_dif;
  char *prompt = PROMPT;
  LINE *z;
/* Initialisations */
  if (curs->line != NULL) {
    curs->last = curs->line;
  }
  curs->line = NULL;
  curs->in_forbidden = curs->in_prefix = curs->in_text = curs->in_cmd = A68_FALSE;
/* We locate the top-of-screen with respect to the current line */
  if (scr->scale_row > 0 && scr->scale_row < LINES) {
    virt_scal = scr->scale_row;
  } else {
    int res = count_reserved (dd);
    virt_scal = res / 2 + (LINES - res) / 2;
  }
/* How many lines above the scale ? */
  for (k = 0, lin_abo = 0; k < virt_scal; k ++) {
    if (reserved_row (dd, k)) {
      lin_abo++;
    }
  }
  for (z = dd->curr; z != NULL && lin_abo < virt_scal; ) {
    if (z == dd->curr) {
      lin_abo ++;
    } else {
      lin_abo += lines_on_scr (dd, z);
    }
    if (lin_abo < virt_scal) {
      backward_line (&z);
    }
  }
  if (z == NULL) {
    run = dd->tof;
  } else {
    run = z;
  }
  tos = run;
  lin_dif = virt_scal - lin_abo;
/* 
We now raster the screen  - first reserved rows 
*/
  for (row = 0; row < LINES; ) {
  /* COMMAND ROW - ====> Forward */
    if (row == scr->cmd_row) {
      int col = 0, ind = 0;
      attron (COLOR_PAIR (TEXT_COLOUR));
      for (ind = 0; ind < MARGIN; ind++) {
        putch (row, col, prompt[ind], dd, NULL, 0);
        col++;
      }
  /* Set initial cursor position at start up */
      if (curs->row == -1) {
        curs->row = row;
        curs->col = col;
      }
  /* Show command */
      for (ind = 0; ind < TEXT_WIDTH && IS_PRINT (scr->cmd[ind]); ind++) {
        putch (row, col, scr->cmd[ind], dd, NULL, ind);
        col++;
      }
      for (ind = col; ind < COLS; col++, ind++) {
        putch (row, col, BLANK_CHAR, dd, NULL, ind - MARGIN);
      }
      attron (COLOR_PAIR (TEXT_COLOUR));
      row++;
    } else if (row == scr->pf_row) {
  /* PF ROW - Program Function key help */
  /* ... but if there is important stuff, it is overridden! */
      if (strcmp (scr->dl0, "help") == 0) {
        int col = 0, ind = 0, pfk, space = (COLS - MARGIN - 4 * 8) / 8;
        char pft[BUFFER_SIZE];
        attron (COLOR_PAIR (SYSTEM_COLOUR));
        if (space < 4) {
          space = 4; /* Tant pis */
        }
        for (ind = 0; ind < MARGIN - 1; ind++) {
          putch (row, col, '-', dd, NULL, 0);
          col++;
        }
        putch (row, col, BLANK_CHAR, dd, NULL, ind);
        col++;
        for (pfk = 0; pfk < 8; pfk++) {
          ASSERT (snprintf (pft, SNPRINTF_SIZE, "F%1d=%-*s ", pfk + 1, space, pf_bind[pfk]) >= 0);
          for (k = 0; k < (int) strlen (pft); col++, k++) {
            putch (row, col, pft[k], dd, NULL, k - MARGIN);
          }
        }
        for (ind = col; ind < COLS; col++, ind++) {
          putch (row, col, BLANK_CHAR, dd, NULL, ind);
        }
        attron (COLOR_PAIR (TEXT_COLOUR));
        row++;
      } else {
        int col = 0, ind = 0;
        if (strlen (scr->dl0) == 0) {
          ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, 
            "%06d lines \"%s\" %s %s file %d (%d%%)", 
            dd->size, dd->name, dd->perm, dd->date, dd->num,
            (int) (100 * (double) fixed_heap_pointer / (double) temp_heap_pointer)) >= 0);
          attron (COLOR_PAIR (SYSTEM_COLOUR));
        } else {
          attron (COLOR_PAIR (SYSTEM_COLOUR));
          for (ind = 0; ind < MARGIN - 1; ind++) {
            putch (row, col, '-', dd, NULL, 0);
            col++;
          }
          putch (row, col, BLANK_CHAR, dd, NULL, ind);
          attron (COLOR_PAIR (HILIGHT_COLOUR));
          col++;
        }
        for (ind = 0; ind < COLS && IS_PRINT (scr->dl0[ind]); ind++) {
          putch (row, col, scr->dl0[ind], dd, NULL, 0);
          col++;
        }
        for (ind = col; ind < COLS; col++, ind++) {
          putch (row, col, BLANK_CHAR, dd, NULL, ind);
        }
        attron (COLOR_PAIR (TEXT_COLOUR));
        row++;
      }
    } else {
      row++;
    }
  }
/* 
Draw text lines 
*/
  for (row = 0; row < LINES; ) {
    if (reserved_row (dd, row)) {
      row++; 
    } else {
  /* RASTER A TEXT LINE */
      BOOL_T cont;
      scr->last_line = run;
      if (run == NULL) {
  /* Draw blank line to balance the screen */
        int col;
        for (col = 0; col < COLS; col++) {
          putch (row, col, BLANK_CHAR, dd, NULL, col);
        }
        row++;
      } else if (lin_dif > 0) {
  /* Draw blank line to balance the screen */
        int col;
        for (col = 0; col < COLS; col++) {
          putch (row, col, BLANK_CHAR, dd, NULL, col);
        }
        lin_dif--;
        row++;
      } else if (run->precmd == NULL) {
        row++;
      } else if (TEXT (run) == NULL) {
        row++;
      } else {
        int col = 0, ind = 0, conts = 0;
        char *txt = TEXT (run);
  /* Draw prefix */
        int pn = NUMBER (run) % 1000000;
        char *pdigits = "0123456789";
        char prefix[MARGIN + 1];
        prefix[MARGIN] = NULL_CHAR;
        prefix[MARGIN - 1] = BLANK_CHAR;
        if (pn == 0) {
          bufcpy (prefix, PREFIX, (strlen (PREFIX) + 1));
        } else {
          /* Next is a cheap print int */
          int pk;
          for (pk = MARGIN - 2; pk >= 0; pk--) {
            prefix[pk] = pdigits[pn % 10];
            pn /= 10;
          }
        }
        for (ind = 0; ind < MARGIN; ind++) {
          char chc = run->precmd[ind], chp = prefix[ind];
          if (chc == BLANK_CHAR) {
            attron (COLOR_PAIR (SYSTEM_COLOUR));
            putch (row, col, chp, dd, run, ind);
          } else {
            attron (COLOR_PAIR (HILIGHT_COLOUR));
            putch (row, col, chc, dd, run, ind);
          }
          col++;
        }
  /* Draw text */
        attron (COLOR_PAIR (TEXT_COLOUR));
        ind = 0;
        cont = A68_TRUE;
        while (cont) {
          int reps, n;
          char ch;
          if (txt[ind] == '\t') {
            ch = BLANK_CHAR;
            reps = tab_reps (col - MARGIN, dd->tabs);
          } else {
            cont = (txt[ind] != NULL_CHAR);
            ch = (char) TO_UCHAR (cont ? txt[ind] : (char) BLANK_CHAR);
            reps = 1;
          }
          for (n = 0; n < reps; n++) {
  /* Take new line if needed, if lines are left */
            if (col == COLS) {
              int num;
              char connum[MARGIN + 16];
              char *digits = "0123456789";
              k = row;
              if (lin_dif < 0) {
                lin_dif++;
              } else {
                do {
                  row++;
                } while (reserved_row (dd, row));
              }
              if (row >= LINES) {
                row = k;
                goto text_end;
              }
  /* Write a continuation number in the prefix area */
              conts++;
              connum[MARGIN - 1] = BLANK_CHAR;
              for (num = conts, k = MARGIN - 2; k >= 0; k--) {
                connum[k] = digits[num % 10];
                num = num / 10;
              }
              attron (COLOR_PAIR (SYSTEM_COLOUR));
              connum[0] = '+';
              col = 0;
              for (k = 0; k < MARGIN; k++) {
                putch (row, col, connum[k], dd, run, k);
                col++;
              }
              attron (COLOR_PAIR (TEXT_COLOUR));
            }
  /* Put the character */
            if (!IS_PRINT (ch)) {
              char nch = (char) TO_UCHAR ((int) 0x40 + (int) ch);
              attron (COLOR_PAIR (SYSTEM_COLOUR));
              if (IS_PRINT (nch)) {
                putch (row, col, nch, dd, run, ind); col++;
              } else {
                putch (row, col, '*', dd, run, ind); col++;
              }
            } else if (IS_TOF (run) || IS_EOF (run)) {
              attron (COLOR_PAIR (SYSTEM_COLOUR));
              putch (row, col, ch, dd, run, ind); col++;
            } else if (run == dd->curr) {
              attron (COLOR_PAIR (HILIGHT_COLOUR));
              putch (row, col, ch, dd, run, ind); col++;
            } else {
              attron (COLOR_PAIR (TEXT_COLOUR));
              putch (row, col, ch, dd, run, ind); col++;
            }
          }
          ind++;
        }
  /* Fill the line */
        text_end:
        for (k = col; k < COLS; k++, col++, ind++) {
          putch (row, col, BLANK_CHAR, dd, run, ind);
        }
        attron (COLOR_PAIR (TEXT_COLOUR));
        forward_line (&run);
        row++;
      }
    }
  }
/* Write the scale row now all data is complete */
  for (row = 0; row < LINES; ) {
    if (row == scr->scale_row) {
  /* SCALE ROW - ----+----1----+----2 */
      int col = 0, ind = 0;
      char pos[BUFFER_SIZE];
      attron (COLOR_PAIR (SYSTEM_COLOUR));
  /* Insert - or overwrite mode */
      putch (row, col, '-', dd, NULL, ind);
      col++;
      if (!curs->in_prefix && scr->ins_mode) {
        putch (row, col, 'I', dd, NULL, ind);
        col++;
      } else {
        putch (row, col, 'O', dd, NULL, ind);
        col++;
      }
  /* Altered or not */
      if (dd->alts > 0) {
        putch (row, col, 'A', dd, NULL, ind);
        col++;
      } else {
        putch (row, col, '-', dd, NULL, ind);
        col++;
      }
  /* Position in line */
      ASSERT (snprintf (pos, SNPRINTF_SIZE, "%3d", curs->index + 1) >= 0)
      for (ind = 0; ind < TEXT_WIDTH && IS_PRINT (pos[ind]); ind++) {
        if (pos[ind] == BLANK_CHAR) {
          putch (row, col, '-', dd, NULL, ind);
        } else {
          putch (row, col, pos[ind], dd, NULL, ind);
        }
        col++;
      }
      putch (row, col, BLANK_CHAR, dd, NULL, ind);
      col++;
  /* Scale */
      for (ind = 0; ind < TEXT_WIDTH; ind++) {
        k = ind + 1;
        if (k % 10 == 0) {
          char *digits = "0123456789";
          putch (row, col, digits[(k % 100) / 10], dd, NULL, 0);
        } else if (k % 5 == 0) {
          putch (row, col, '+', dd, NULL, 0);
        } else if (k == 1 || k == TEXT_WIDTH) {
          putch (row, col, '-', dd, NULL, 0);
        } else {
          putch (row, col, '-', dd, NULL, 0);
        }
        col++;
      }
      attron (COLOR_PAIR (TEXT_COLOUR));
    }
    row++;
  }
  move (curs->row, curs->col);
}

/* Routines to edit various parts of the screen */

/*
\brief edit prefix
\param dd current dataset
\param ch typed character
**/

static void edit_prefix (DATASET *dd, int ch)
{
/*
Prefix editing is very basic. You type in overwrite mode.
DEL erases the character under the cursor.
BACKSPACE erases the character left of the cursor.
*/
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  LINE *lin = curs->line;
  if (lin == NULL) {
    return;
  }
  if (ch <= UCHAR_MAX && IS_PRINT (ch) && curs->index < MARGIN - 1) {
    lin->precmd[curs->index] = (char) TO_UCHAR (ch);
    curs->col = (curs->col == MARGIN - 1 ? MARGIN - 1 : curs->col + 1);
  } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE) && curs->col > 0) {
    int i;
    curs->index = curs->col = (curs->col == 0 ? 0 : curs->col - 1);
    for (i = curs->index; i < MARGIN - 1; i++) {
      lin->precmd[i] = lin->precmd[i + 1];
    }
  } else if (ch == KEY_DC && curs->col < MARGIN - 1) {
    int i;
    for (i = curs->index; i < MARGIN - 1; i++) {
      lin->precmd[i] = lin->precmd[i + 1];
    }
  }
}

/*
\brief edit command
\param dd current dataset
\param ch typed character
**/

static void edit_cmd (DATASET *dd, int ch)
{
/*
Command editing is in insert or overwrite mode.
The command line is as wide as the screen minus the prompt.
If the cursor is outside the command string the latter is lengthened.
DEL erases the character under the cursor.
BACKSPACE erases the character to the left of the cursor.
*/
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  if (ch <= UCHAR_MAX && IS_PRINT (ch) && (int) strlen (scr->cmd) < (int) TEXT_WIDTH) {
    int j, k;
    while ((int) curs->index > (int) strlen (scr->cmd)) {
      k = (int) strlen (scr->cmd);
      scr->cmd[k] = BLANK_CHAR;
      scr->cmd[k + 1] = NULL_CHAR;
    }
    if (scr->ins_mode) {
      k = (int) strlen (scr->cmd);
      for (j = k + 1; j > curs->index; j--) {
        scr->cmd[j] = scr->cmd[j - 1];
      }
    }
    scr->cmd[curs->index] = (char) TO_UCHAR (ch);
    curs->col = (curs->col == COLS - 1 ? 0 : curs->col + 1);
  } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE) && curs->index > 0) {
    int k;
    if (curs->index == 0) {
      return;
    } else {
      curs->index--;
      curs->col--;
    }
    for (k = curs->index; k < (int) strlen (scr->cmd); k++) {
      scr->cmd[k] = scr->cmd[k + 1];
    }
  } else if (ch == KEY_DC && curs->col < COLS - 1) {
    int k;
    for (k = curs->index; k < (int) strlen (scr->cmd); k++) {
      scr->cmd[k] = scr->cmd[k + 1];
    }
  }
}

/*
\brief edit text
\param dd current dataset
\param ch typed character
**/

static void edit_text (DATASET *dd, int ch)
{
/*
Text editing is in insert or overwrite mode.
The string can be extended as long as memory lasts.
If the cursor is outside the string the latter is lengthened.
DEL erases the character under the cursor.
BACKSPACE erases the character to the left of the cursor.
*/
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  int llen = 0;
  LINE *lin = curs->line;
  if (lin == NULL) {
    return;
  }
  if (IS_TOF (lin) || IS_EOF (lin)) {
    return;
  }
  if (lin == scr->last_line) {
    llen = lines_on_scr (dd, lin);
  }
  dd->alts++;
  if (ch <= UCHAR_MAX && (IS_PRINT (ch) || ch == '\t')) {
    int j, k, len = (int) strlen (TEXT (lin));
    if (lin->reserved <= len + 2 || lin->reserved <= curs->index + 2) {
  /* Not enough room */
      char *txt = TEXT (lin);
      int l1 = (lin->reserved <= len + 2 ? len + 2 : 0);
      int l2 = (lin->reserved <= curs->index + 2 ? curs->index + 2 : 0);
      int res = (l1 > l2 ? l1 : l2) + BLOCK_SIZE;
      if (res % BLOCK_SIZE > 0) {
        res += (BLOCK_SIZE - res % BLOCK_SIZE);
      }
      lin->reserved = res;
      TEXT (lin) = (char *) edit_get_heap (dd, (size_t) res);
      if (TEXT (lin) == NULL) {
        return;
      }
      bufcpy (TEXT (lin), txt, res);
    }
  /* Pad with spaces to cursor position if needed */
    while (curs->index > (len = (int) strlen (TEXT (lin)))) {
      TEXT (lin)[len] = BLANK_CHAR;
      TEXT (lin)[len + 1] = NULL_CHAR;
    }
    if (scr->ins_mode) {
      k = (int) strlen (TEXT (lin));
      for (j = k + 1; j > curs->index; j--) {
        TEXT (lin)[j] = TEXT (lin)[j - 1];
      }
    }
    TEXT (lin)[curs->index] = (char) TO_UCHAR (ch);
    curs->sync_index = curs->index + 1;
    curs->sync_line = lin;
    curs->sync = A68_TRUE;
  } else if (ch == KEY_BACKSPACE || ch == BACKSPACE) {
    int k;
    char del;
    if (curs->index == 0) {
      join_line (dd, "edit");
      edit_reset (dd);
      dd->bl_start = dd->bl_end = NULL;
      return;
    } else {
      curs->index--;
      del = TEXT (lin)[curs->index];
    }
    for (k = curs->index; k < (int) strlen (TEXT (lin)); k++) {
      TEXT (lin)[k] = TEXT (lin)[k + 1];
    }
/* Song and dance to avoid problems with deleting tabs that span end-of-screen */
    curs->sync_index = curs->index;
    curs->sync_line = lin;
    curs->sync = A68_TRUE;
  } else if (ch == KEY_DC && curs->col < COLS) {
    int k;
    for (k = curs->index; k < (int) strlen (TEXT (lin)); k++) {
      TEXT (lin)[k] = TEXT (lin)[k + 1];
    }
    curs->sync_index = curs->index;
    curs->sync_line = lin;
    curs->sync = A68_TRUE;
  }
  if (lin == scr->last_line && lines_on_scr (dd, lin) > llen) {
    forward_line (&dd->curr);
  }
}

/*!
\brief whether x matches c; case insensitive
\param string string to test
\param string to match, caps in c are mandatory
\param rest pointer to string after the match
\return whether match
**/

BOOL_T match_cmd (char *x, char *c, char **args)
{
#define TERM(c) (c == NULL_CHAR || IS_DIGIT (c) || IS_SPACE (c) || IS_PUNCT (c))
  BOOL_T match = A68_TRUE; /* Until proven otherwise */
  if (x == NULL || c == NULL) {
    return (A68_FALSE);
  }
/* Single-symbol commands as '?' or '='. */
  if (IS_PUNCT (c[0])) {
    match = (BOOL_T) (x[0] == c[0]);
    if (args != NULL) {
      if (match && x[1] != NULL_CHAR) {
        (*args) = &x[1];
        SKIP_WHITE (*args);
      } else {
        (*args) = NULL;
      }
    }
    return ((BOOL_T) match);
  }
/* First the required letters */
  while (IS_UPPER (c[0]) && match) {
    match = (BOOL_T) (match & (TO_LOWER (x[0]) == TO_LOWER ((c++)[0])));
    if (! TERM (x[0])) {
      x++;
    }
  }
/* Then the facultative part */
  while (! TERM (x[0]) && c[0] != NULL_CHAR && match) {
    match = (BOOL_T) (match & (TO_LOWER ((x++)[0]) == TO_LOWER ((c++)[0])));
  }
/* Determine the args (arguments, counts) */
  if (args != NULL) {
    if (match && x[0] != NULL_CHAR) {
      (*args) = &x[0];
      SKIP_WHITE (*args);
    } else {
      (*args) = NULL;
    }
  }
  return ((BOOL_T) match);
#undef TERM
}

/*!
\brief translate integral argument
\param dd current dataset
\param cmd command that calls this routine
\return argument value: default is 1 if no value is present, or -1 if an error occurs
**/

static int int_arg (DATASET *dd, char *cmd, char *arg, char **rest, int def)
{
  DISPLAY *scr = &(dd->display);
  char *suffix;
  int k;
/* Fetch argument */
  SKIP_WHITE (arg);
  if (EMPTY_STRING (arg)) {
    return (1);
  };
/* Translate argument into integer */
  if (arg[0] == '*') {
    (*rest) = &arg[1];
    SKIP_WHITE (*rest);
    return (def);
  } else {
    RESET_ERRNO;
    k = strtol (arg, &suffix, 0);      /* Accept also octal and hex */
    if (errno != 0 || suffix == arg) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: invalid integral argument", cmd) >= 0);
      return (WRONG_TARGET);
    } else {
      (*rest) = suffix;
      SKIP_WHITE (*rest);
      return (k);
    }
  }
}

/*!
\brief get substitute strings 
\param dd current dataset
\param cmd command that calls this routine
\param arg point to arguments
\param rest will point to trialing text
**/

static BOOL_T get_subst (DATASET *dd, char *cmd, char *arg, char **rest)
{
/* Get the find and replacement string in a substitute command */
  DISPLAY *scr = &(dd->display);
  char delim, *q, *pat1, *pat2;
  int rc;
  if (EMPTY_STRING (arg)) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
    return (A68_FALSE);
  }
/* Initialise */
  reset_regexp (&(dd->find));
  reset_regexp (&(dd->repl));
  (*rest) = NULL;
  SKIP_WHITE (arg);
  q = arg;
  delim = *(q++);
/* Get find regexp */
  pat1 = dd->find.pattern;
  pat1[0] = NULL_CHAR;
  while (q[0] != delim && q[0] != NULL_CHAR) {
    if (q[0] == '\\') {
      *(pat1)++ = *q++;
      if (q[0] == NULL_CHAR) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: invalid regular expression", cmd) >= 0);
        *(pat1) = NULL_CHAR;
        return (A68_FALSE);
      }
      *(pat1)++ = *q++;
    } else {
      *(pat1)++ = *q++;
    }
  }
  *(pat1) = NULL_CHAR;
  if ((int) strlen (dd->find.pattern) == 0) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
    return (A68_FALSE);
  }
  rc = compile_regexp (dd, &(dd->find), cmd);
  if (rc != 0) {
    return (A68_FALSE);
  }
/* Get replacement string */
  if (q[0] != delim) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: unrecognised regular expression syntax", cmd) >= 0);
    return (A68_FALSE);
  }
  q = &q[1];
  pat2 = dd->repl.pattern;
  pat2[0] = NULL_CHAR;
  while (q[0] != delim && q[0] != NULL_CHAR) {
    if (q[0] == '\\') {
      *(pat2)++ = *q++;
      if (q[0] == NULL_CHAR) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: invalid regular expression", cmd) >= 0);
        *(pat2) = NULL_CHAR;
        return (A68_FALSE);
      }
      *(pat2)++ = *q++;
    } else {
      *(pat2)++ = *q++;
    }
  }
  *(pat2) = NULL_CHAR;
  if (q[0] == delim) {
    q++;
  }
  (*rest) = q;
  SKIP_WHITE (*rest);
  return (A68_TRUE);
}

/*!
\brief substitute target
\param dd current dataset
\param z line to substitute
\param rep number of substitutions ...
\param start ... starting at this occurence
\param cmd command that calls this routine
**/

static int substitute (DATASET *dd, LINE *z, int rep, int start, char *cmd)
{
  DISPLAY *scr = &(dd->display);
  int k, subs = 0, newt = 0, matcnt = 0;
/* Initialisation */
  for (k = 0; k < rep; k ++)
  {
    int i, lenn, lens, lent, nnwt, pos = 0;
    char *txt = &(TEXT (z)[newt]);
    int rc = regexec (&(dd->find.compiled), txt, dd->find.num_match, dd->find.match, (k == 0 ? 0 : REG_NOTBOL));
    if (rc == REG_NOMATCH) {
      goto subst_end;
    }
    matcnt++;
    if (matcnt < start) {
      newt += dd->find.match[0].rm_eo;
      continue;
    }
/* Part before match */
    (dd->linbuf)[0] = NULL_CHAR;
    lent = (int) strlen (TEXT (z));
    for (i = 0; i < newt + dd->find.match[0].rm_so; i ++) {
      add_linbuf (dd, TEXT (z)[i], pos);
      if (dd->linbuf == NULL) {
        return (SUBST_ERROR);
      }
      pos++;
    }
/* Insert substitution string */
    lens = (int) strlen (dd->repl.pattern);
    i = 0;
    while (i < lens && dd->repl.pattern[i] != NULL_CHAR) {
      if (dd->repl.pattern[i] == '\\' && IS_DIGIT (dd->repl.pattern[i + 1])) {
        int n = 0, j;
        switch (dd->repl.pattern[i + 1]) {
          case '0': {n = 0; break;}
          case '1': {n = 1; break;}
          case '2': {n = 2; break;}
          case '3': {n = 3; break;}
          case '4': {n = 4; break;}
          case '5': {n = 5; break;}
          case '6': {n = 6; break;}
          case '7': {n = 7; break;}
          case '8': {n = 8; break;}
          case '9': {n = 9; break;}
        }
        if (n >= (int) dd->find.num_match) {
          ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no group \\%d in regular expression", cmd, n) >= 0);
          return (SUBST_ERROR);
        }
        for (j = dd->find.match[n].rm_so; j < dd->find.match[n].rm_eo; j ++) {
          add_linbuf (dd, TEXT (z)[newt + j], pos);
          if (dd->linbuf == NULL) {
            return (SUBST_ERROR);
          }
          pos++;
        }
        i++; /* Skip digit in \n */
      } else {
        add_linbuf (dd, dd->repl.pattern[i], pos);
        if (dd->linbuf == NULL) {
          return (SUBST_ERROR);
        }
        pos++;
      }
      i++;
    }
    nnwt = pos;
/* Part after match */
    for (i = newt + dd->find.match[0].rm_eo; i < lent; i ++) {
      add_linbuf (dd, TEXT (z)[i], pos);
      if (dd->linbuf == NULL) {
        return (SUBST_ERROR);
      }
      pos++;
    }
    add_linbuf (dd, NULL_CHAR, pos);
    if (dd->linbuf == NULL) {
      return (SUBST_ERROR);
    }
/* Copy the new line */
    newt = nnwt;
    subs++;
    lenn = (int) strlen (dd->linbuf);
    if (z->reserved >= lenn + 1) {
      bufcpy (TEXT (z), dd->linbuf, z->reserved);
    } else {
      int res = lenn + 1;
      if (res % BLOCK_SIZE > 0) {
        res += (BLOCK_SIZE - res % BLOCK_SIZE);
      }
      z->reserved = res;
      TEXT (z) = (char *) edit_get_heap (dd, (size_t) res);
      if (TEXT (z) == NULL) {
        return (SUBST_ERROR);
      }
      bufcpy (TEXT (z), dd->linbuf, res);
    }
    if (TEXT (z)[newt] == NULL_CHAR) {
      goto subst_end;
    }
  }
  subst_end:
  return (subs);
}

/*!
\brief get target with respect to the current line
\param dd current dataset
\param cmd command that calls this routine
\param arg points to arguments
\param rest will point to text after arguments
\param compile must compile or has been compiled
**/

static LINE * get_regexp (DATASET *dd, char *cmd, char *arg, char **rest, BOOL_T compile)
{
/*
Target is of form
	[+|-]/regexp/ or
	[+|-]/regexp/&/regexp/ both must match
	[+|-]/regexp/|/regexp/ one or both must match
	[+|-]/regexp/^/regexp/ one must match, but not both
*/
  DISPLAY *scr = &(dd->display);
  char delim, *q, *pat1, *pat2;
  int rc;
  BOOL_T forward;
  if (compile == A68_FALSE) {
    if ((dd->targ1).is_compiled == A68_FALSE || dd->search == 0) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
      return (NULL);
    }
    if (dd->search == 1) {
      forward = A68_TRUE;
    } else {
      forward = A68_FALSE;
    }
  } else {
    if (EMPTY_STRING (arg)) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
      return (NULL);
    }
  /* Initialise */
    reset_regexp (&(dd->targ1));
    reset_regexp (&(dd->targ2));
    dd->oper = NULL_CHAR;
    (*rest) = NULL;
    SKIP_WHITE (arg);
    q = arg;
    if (q[0] == '+') {
      forward = A68_TRUE;
      q++;
    } else if (q[0] == '-') {
      forward = A68_FALSE;
      dd->search = -1;
      q++;
    } else {
      forward = A68_TRUE;
      dd->search = 1;
    }
    delim = *(q++);
  /* Get first regexp */
    pat1 = dd->targ1.pattern;
    pat1[0] = NULL_CHAR;
    while (q[0] != delim && q[0] != NULL_CHAR) {
      if (q[0] == '\\') {
        *(pat1)++ = *q++;
        *(pat1)++ = *q++;
      } else {
        *(pat1)++ = *q++;
      }
    }
    *(pat1) = NULL_CHAR;
    if ((int) strlen (dd->targ1.pattern) == 0) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
      return (NULL);
    }
    rc = compile_regexp (dd, &(dd->targ1), cmd);
    if (rc != 0) {
      return (NULL);
    }
  /* Get operator and 2nd regexp, if present */
    if (q[0] == delim && (q[1] == '&' || q[1] == '|' || q[1] == '^')) {
      dd->oper = q[1];
      if (q[2] != delim) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: unrecognised regular expression syntax", cmd) >= 0);
      }
      q = &q[3];
      pat2 = dd->targ2.pattern;
      pat2[0] = NULL_CHAR;
      while (q[0] != delim && q[0] != NULL_CHAR) {
        if (q[0] == '\\') {
          *(pat2)++ = *q++;
          *(pat2)++ = *q++;
        } else {
          *(pat2)++ = *q++;
        }
      }
      *(pat2) = NULL_CHAR;
      if ((int) strlen (dd->targ2.pattern) == 0) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
        return (NULL);
      }
      rc = compile_regexp (dd, &(dd->targ2), cmd);
      if (rc != 0) {
        return (0);
      }
    }
    if (q[0] == delim) {
      (*rest) = &(q[1]);
      SKIP_WHITE (*rest);
    } else {
      (*rest) = &(q[0]);
      SKIP_WHITE (*rest);
    }
  }
/* Find the first line matching the regex */
  if (forward) {
    LINE *u = dd->curr;
    forward_line (&u);
    if (NOT_EOF (u)) {
      LINE *z = u;
      for (z = u; NOT_EOF (z); forward_line (&z)) {
        if (match_regex (dd, z, 0, cmd)) {
          return (z);
        }
      }
    }
  } else {
    LINE *u = dd->curr;
    backward_line (&u);
    if (NOT_TOF (u)) {
      LINE *z = u;
      for (z = u; NOT_TOF (z); backward_line (&z)) {
        if (match_regex (dd, z, 0, cmd)) {
          return (z);
        }
      }
    }
  }
  ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: target not found", cmd) >= 0);
  return (NULL);
}

/*!
\brief get target with respect to the current line
\param dd current dataset
\param cmd command that calls this routine
\param arg points to arguments
\param rest will point to text after arguments
**/

LINE *get_target (DATASET *dd, char *cmd, char *args, char **rest, BOOL_T offset)
{
  DISPLAY *scr = &(dd->display);
  LINE *z = NULL;
  SKIP_WHITE (args);
  if (IS_DIGIT (args[0])) {
/* n	Relative displacement down */
    int n = int_arg (dd, cmd, args, rest, 1);
    if (n == WRONG_TARGET) {
      return (NULL);
    } else {
      int k;
      for (z = dd->curr, k = 0; z != NULL && k < n; forward_line (&z), k++) {;}
      if (z == NULL) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: target beyond end-of-data", cmd) >= 0);
      }
    }
  } else if (args[0] == '+' && IS_DIGIT (args[1])) {
/* +n	Relative displacement down */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NULL);
    } else {
      int k;
      for (z = dd->curr, k = 0; z != NULL && k < n; forward_line (&z), k++) {;}
      if (z == NULL) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: target beyond end-of-data", cmd) >= 0);
      }
    }
  } else if (args[0] == ':') {
/*:n	Absolute line number */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NULL);
    } else {
      for (z = dd->tof; z != NULL && NUMBER (z) != n; forward_line (&z)) {;}
      if (z == NULL) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: target outside selected lines", cmd) >= 0);
      }
    }
  } else if (args[0] == '-' && IS_DIGIT (args[1])) {
/* -n	Relative displacement up */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NULL);
    } else {
      int k;
      for (z = dd->curr, k = 0; z != NULL && k < n; backward_line (&z), k++) {;}
      if (z == NULL) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: target before top-of-data", cmd) >= 0);
      }
    }
  } else if (args[0] == '*' || args[0] == '$') {
/* *	end-of-data */
    for (z = dd->tof; NOT_EOF (z) ; forward_line (&z)) {;}
    (*rest) = (&args[1]);
    SKIP_WHITE (*rest);
  } else if (args[0] == '+' && args[1] == '*') {
/* *	end-of-data */
    for (z = dd->tof; NOT_EOF (z) ; forward_line (&z)) {;}
    (*rest) = (&args[2]);
    SKIP_WHITE (*rest);
  } else if (args[0] == '-' && args[1] == '*') {
/* *	top-of-data */
    for (z = dd->curr; NOT_TOF (z); backward_line (&z)) {;}
    (*rest) = (&args[2]);
    SKIP_WHITE (*rest);
  } else if (args[0] == '.') {
/* .IDF	Prefix identifier */
    LINE *u;
    char idf[8] = {'.', NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR};
    int k;
    for (k = 1; IS_ALNUM (args[k]) && k < MARGIN - 1; k++) {
      idf[k] = args[k];
    }
    (*rest) = (&args[k]);
    SKIP_WHITE (*rest);
/* Scan all file to check multiple definitions */
    for (u = dd->tof, z = NULL; u != NULL; forward_line (&u)) {
      char *v = u->precmd;
      SKIP_WHITE (v);
      if (strncmp (v, idf, (size_t) (k - 1)) == 0) {
        if (z != NULL) {
          ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: multiple targets %s", cmd, idf) >= 0);
          return (NULL);
        } else {
          z = u;
        }
      }
    }
    if (z == NULL) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no target %s", cmd, idf) >= 0);
    }
  } else if (args[0] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else if (args[0] == '-' && args[1] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else if (args[0] == '+' && args[1] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: unrecognised target syntax", cmd) >= 0);
    return (NULL);
  }
/* A target can have an offset +-m */
  if (!offset) {
    return (z);
  }
  args = *rest;
  if (args != NULL && args[0] == '+' && IS_DIGIT (args[1])) {
/* +n	Relative displacement down */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NULL);
    } else {
      int k;
      for (k = 0; z != NULL && k < n; forward_line (&z), k++) {;}
      if (z == NULL) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: target beyond end-of-data", cmd) >= 0);
      }
    }
  } else if (args != NULL && args[0] == '-' && IS_DIGIT (args[1])) {
/* -n	Relative displacement up */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NULL);
    } else {
      int k;
      for (k = 0; z != NULL && k < n; backward_line (&z), k++) {;}
      if (z == NULL) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: target before top-of-data", cmd) >= 0);
      }
    }
  }
  return (z);
}

/*
\brief dispatch lines to shell command and insert command output
\param dd current dataset
\param cmd edit cmd that calls this routine
\param u target line
\param argv shell command
**/

static void edit_filter (DATASET *dd, char *cmd, char *argv, LINE *u)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  char shell[BUFFER_SIZE];
  LINE *z;
/* Write selected lines ... */
  edit_write (dd, cmd, ".a68g.edit.out", dd->curr, u);
/* Delete the original lines */
  for (z = dd->curr; z != u && IN_TEXT (z); forward_line (&z)) {
    curs->line = z;
    curs->last = z;
    curs->index = 0;
    cdelete (dd);
    join_line (dd, cmd);
  }
  if (IN_TEXT (z)) {
    dd->curr = PREVIOUS (z);
  } else {
    dd->curr = dd->tof;
  }
  align_current (dd);
/* ... process the lines ... */
  RESET_ERRNO;
  ASSERT (snprintf (shell, SNPRINTF_SIZE, "%s < .a68g.edit.out > .a68g.edit.in", argv) >= 0);
  system (shell);
  CHECK_ERRNO (cmd);
/* ... and read lines */
  edit_read (dd, cmd, ".a68g.edit.in", NULL);
/* Done */
  bufcpy (scr->cmd, "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
  return;
}

/*
\brief move or copy lines (move is copy + delete)
\param dd current dataset
\param cmd command that calls this
\param args points to arguments
\param cmd_move move or copy 
**/

static void move_copy (DATASET *dd, char *cmd, char *args, BOOL_T cmd_move)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  LINE *u, *v, *w, *x, *z, *bl_start = NULL, *bl_end = NULL;
  char *cmdn, *rest = NULL;
  int j, n, count;
  if (cmd_move) {
    cmdn = "move";
  } else {
    cmdn = "copy";
  }
  if (dd->subset) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: fold disables %s", cmdn, cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  u = dd->curr;
  if (EMPTY_STRING (args)) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: insufficient arguments", cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  v = get_target (dd, cmd, args, &rest, A68_TRUE);
  args = rest;
  if (EMPTY_STRING (args)) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: insufficient arguments", cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  w = get_target (dd, cmd, args, &rest, A68_TRUE);
  if (!EMPTY_STRING (rest)) {
     args = rest;
     n = int_arg (dd, cmd, args, &rest, 1);
  } else {
     n = 1;
  }
  if (!EMPTY_STRING (rest)) {
    TRAILING (cmdn);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  /* Out of range */
  if (IS_EOF (w)) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot add after end-of-data", cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  /* Backwards range */
  if (NOT_EOF (v) && (NUMBER (v) < NUMBER (u))) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: backward range", cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  /* Copy to within range */
  if (NUMBER (u) <= NUMBER (w) && NUMBER (w) < NUMBER (v) - 1) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: target within selected range", cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  edit_write_undo_file (dd, cmdn);
  /* Count */
  for (count = 0, z = u; z != v; FORWARD (z)) {
    count++;
  }
  /* Copy */
  for (j = 0; j < n; j++) {
    /* Add lines */
    int k;
    for (k = 0, z = u; k < count && IN_TEXT (z); k++, FORWARD (z)) {
      curs->line = w;
      curs->index = (int) strlen (TEXT (w));
      split_line (dd, cmd);
    }
    /* Copy text */
    bl_start = NEXT (w);
    for (k = 0, x = NEXT (w), z = u; k < count && IN_TEXT (z); k++, FORWARD (x), FORWARD (z)) {
      char *txt = TEXT (z);
      int len = 1 + (int) strlen (txt);
      int res = len;
      if (res % BLOCK_SIZE > 0) {
        res += (BLOCK_SIZE - res % BLOCK_SIZE);
      }
      bl_end = x;
      x->reserved = res;
      TEXT (x) = (char *) edit_get_heap (dd, (size_t) res);
      if (TEXT (x) == NULL) {
        return;
      }
      bufcpy (TEXT (x), txt, res);
    }
  }
  /* Delete the original lines */
  if (cmd_move) {
    for (z = u; z != v && IN_TEXT (z); FORWARD (z)) {
      curs->line = z;
      curs->last = z;
      curs->index = 0;
      cdelete (dd);
      join_line (dd, cmd);
    }
  }
  /* Done */
  edit_reset (dd);
  if ((count * n) == 0) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s no lines", cmdn, (cmd_move ? "moved" : "copied")) >= 0);
  } else if ((count * n) == 1) {
    dd->bl_start = bl_start;
    dd->bl_end = bl_end;
    dd->alts++;
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s 1 line", cmdn, (cmd_move ? "moved" : "copied")) >= 0);
  } else if (n == 1) {
    dd->bl_start = bl_start;
    dd->bl_end = bl_end;
    dd->alts++;
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s %d lines", cmdn, (cmd_move ? "moved" : "copied"), count * n) >= 0);
  } else {
    dd->bl_start = bl_start;
    dd->bl_end = bl_end;
    dd->alts++;
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %s %d lines %d times", cmdn, (cmd_move ? "moved" : "copied"), count, n) >= 0);
  }
  bufcpy (scr->cmd, "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
  return;
}

/*
\brief indent lines to a column
\param dd current dataset
\param cmd command that calls this
\param args points to arguments
**/

static void indent (DATASET *dd, char *cmd, char *args)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  LINE *u, *v, *z;
  char *rest = NULL;
  int dir, k, n, m, count;
  if (dd->subset) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: folded dataset", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  u = dd->curr;
  if (EMPTY_STRING (args)) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: insufficient arguments", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  v = get_target (dd, cmd, args, &rest, A68_TRUE);
  args = rest;
  if (EMPTY_STRING (args)) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: insufficient arguments", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  dir = 0;
  if (!EMPTY_STRING (rest)) {
     args = rest;
     if (args[0] == '>') {
       args++;
       dir = 1;
     } else if (args[0] == '<') {
       args++;
       dir = -1;
     }
     n = int_arg (dd, cmd, args, &rest, 1);
  } else {
     n = 1;
  }
  if (!EMPTY_STRING (rest)) {
    TRAILING (cmd);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  /* Backwards range */
  if (NOT_EOF (v) && (NUMBER (v) < NUMBER (u))) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: backward range", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  /* Align */
  edit_write_undo_file (dd, cmd);
  k = -1;
  count = 0;
  for (z = u; z != v; FORWARD (z)) {
/* Find column of first printable character */
    if (k == -1 && NUMBER (z) != 0) {
      int j;
      char *t = TEXT (z);
      for (j = 0; k == -1 && t[j] != NULL_CHAR; j++) {
        if (!IS_SPACE (t[j])) {
          k = j;
        }
      }
    }
    if (dir == 1) {
      m = k + n;
    } else if (dir == 0) {
      m = n - 1;
    } else if (dir == -1) {
      m = k - n;
    }
/* Align the line, if we can */
    if (k >= 0 && NUMBER (z) != 0) {
      int delta = m - k, i, j;
      char *t = TEXT (z);
      (dd->linbuf)[0] = 0;
      i = 0;
      if (delta >= 0) {
        for (j = 0; j < delta; j ++) {
          add_linbuf (dd, BLANK_CHAR, i++);
          if (dd->linbuf == NULL) {
            CURSOR_TO_COMMAND (dd, curs);
            return;
          }
        }
        for (j = 0; t[j] != NULL_CHAR; j++) {
          add_linbuf (dd, t[j], i++);
          if (dd->linbuf == NULL) {
            CURSOR_TO_COMMAND (dd, curs);
            return;
          }
        }
      } else {
        for (j = 0; j < -delta && t[j] != NULL_CHAR && IS_SPACE (t[j]); j ++) {;}
        for (; t[j] != NULL_CHAR; j++) {
          add_linbuf (dd, t[j], i++);
          if (dd->linbuf == NULL) {
            CURSOR_TO_COMMAND (dd, curs);
            return;
          }
        }
      }
      new_edit_string (dd, z, dd->linbuf, NULL);
      if (TEXT (z) == NULL) {
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      count++;
    }
  }
  /* Done */
  edit_reset (dd);
  dd->bl_start = dd->bl_end = NULL;
  if (count == 0) {
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: indented no lines", cmd) >= 0);
  } else if (count == 1) {
    dd->alts++;
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: indented 1 line", cmd) >= 0);
  } else {
    dd->alts++;
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: indented %d lines", cmd, count) >= 0);
  }
  bufcpy (scr->cmd, "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
  return;
}

/*
\brief set current line
\param dd current dataset
\param cmd command that calls this
\param target to point current line to
**/

static void set_current (DATASET *dd, char *cmd, char *target)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  char *rest = NULL;
  LINE *z = get_target (dd, cmd, target, &rest, A68_TRUE);
  if (!EMPTY_STRING (rest)) {
    TRAILING (cmd);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  if (z != NULL) {
    dd->curr = z;
  }
  bufcpy (scr->cmd, "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
}

/*
\brief set current line and store target as command
\param dd current dataset
\param cmd command that calls this
\param target to point current line to
**/

static void set_current_store (DATASET *dd, char *cmd, char *target)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  char *rest = NULL;
  LINE *z;
  z = get_target (dd, cmd, target, &rest, A68_TRUE);
  if (!EMPTY_STRING (rest)) {
    TRAILING (cmd);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  if (z != NULL) {
    dd->curr = z;
  }
  bufcpy (scr->cmd, "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
}

/*
\brief give full command name
\param cmd command that calls this
\return same
**/

static char *full_cmd (char *cmd)
{
  if (match_cmd (cmd, "Add", NULL)) {
    return ("add");
  } else if (match_cmd (cmd, "AGain", NULL)) {
    return ("again");
  } else if (match_cmd (cmd, "Indent", NULL)) {
    return ("indent");
  } else if (match_cmd (cmd, "CAse", NULL)) {
    return ("case");
  } else if (match_cmd (cmd, "CDelete", NULL)) {
    return ("cdelete");
  } else if (match_cmd (cmd, "RUN", NULL)) {
    return ("run");
  } else if (match_cmd (cmd, "COpy", NULL)) {
    return ("copy");
  } else if (match_cmd (cmd, "C", NULL)) {
    return ("change");
  } else if (match_cmd (cmd, "DELete", NULL)) {
    return ("delete");
  } else if (match_cmd (cmd, "Edit", NULL)) {
    return ("edit");
  } else if (match_cmd (cmd, "FILE", NULL)) {
    return ("file");
  } else if (match_cmd (cmd, "FOld", NULL)) {
    return ("fold");
  } else if (match_cmd (cmd, "MEssage", NULL)) {
    return ("message");
  } else if (match_cmd (cmd, "MOve", NULL)) {
    return ("move");
  } else if (match_cmd (cmd, "Page", NULL)) {
    return ("page");
  } else if (match_cmd (cmd, "PF", NULL)) {
    return ("pf");
  } else if (match_cmd (cmd, "QQuit", NULL)) {
    return ("qquit");
  } else if (match_cmd (cmd, "Read", NULL)) {
    return ("read");
  } else if (match_cmd (cmd, "RESet", NULL)) {
    return ("reset");
  } else if (match_cmd (cmd, "SAve", NULL)) {
    return ("save");
  } else if (match_cmd (cmd, "SCale", NULL)) {
    return ("scale");
  } else if (match_cmd (cmd, "SHell", NULL)) {
    return ("shell");
  } else if (match_cmd (cmd, "Help", NULL)) {
    return ("help");
  } else if (match_cmd (cmd, "SYntax", NULL)) {
    return ("syntax");
  } else if (match_cmd (cmd, "S", NULL)) {
    return ("substitute");
  } else if (match_cmd (cmd, "TOGgle", NULL)) {
    return ("toggle");
  } else if (match_cmd (cmd, "Undo", NULL)) {
    return ("undo");
  } else if (match_cmd (cmd, "WQ", NULL)) {
    return ("wq");
  } else if (match_cmd (cmd, "Write", NULL)) {
    return ("write");
  } else if (match_cmd (cmd, "Xedit", NULL)) {
    return ("xedit");
  } else {
    return (cmd);
  }
}

/*
\brief execute editor command
\param dd current dataset
**/

static void edit_do_cmd (DATASET *dd)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  char *cmd = scr->cmd, *args = NULL, *rest = NULL;
  char *fcmd = full_cmd (cmd);
/* Initial white space is meaningless */
  SKIP_WHITE (cmd);
/* Empty command is meaningless */
  if ((int) strlen (cmd) == 0) {
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
/* Commands that are not stored */
  if (cmd[0] == '&') {
/* Execute the command but leave it in the buffer */
    char cp[BUFFER_SIZE];
    bufcpy (cp, cmd, BUFFER_SIZE);
    bufcpy (scr->cmd, &cp[1], BUFFER_SIZE);
    edit_do_cmd (dd);
    bufcpy (scr->cmd, cp, BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "?", &args)) {
/* Restore last command; do not execute */
    NO_ARGS ("?", args);
    bufcpy (scr->cmd, history[histix], BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "=", &args)) {
    NO_ARGS ("=", args);
/* Repeat last command on "=" */
    bufcpy (scr->cmd, history[histix], BUFFER_SIZE);
    edit_do_cmd (dd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } 
/* Commands that are stored */
  edit_add_history (cmd); 
/* Target commands that set the current line */
  if (IS_DIGIT (cmd[0])) {
    set_current (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == ':') {
    set_current (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '+' && IS_DIGIT (cmd[1])) {
    set_current (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '-' && IS_DIGIT (cmd[1])) {
    set_current (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '*' || cmd[0] == '$') {
    set_current (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '+' && cmd[1] == '*') {
    set_current (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '-' && cmd[1] == '*') {
    set_current (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '.') {
    set_current (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '/') {
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '-' && cmd[1] == '/') {
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '+' && cmd[1] == '/') {
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  }
  if (match_cmd (cmd, "AGain", &args)) {
/* AGAIN: repeat last search */
    LINE *z;
    NO_ARGS (fcmd, args);
    z = get_regexp (dd, cmd, args, &rest, A68_FALSE);
    if (z != NULL) {
      dd->curr = z;
    }
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "TOGgle", &args)) {
/* TOGGLE: switch between prefix/text and command line */
    NO_ARGS (fcmd, args);
    if (curs->in_cmd) {
      CURSOR_TO_CURRENT (dd, curs);
    } else if (! curs->in_forbidden) {
      CURSOR_TO_COMMAND (dd, curs);
    } else {
      CURSOR_TO_COMMAND (dd, curs);
    }
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  } else if (match_cmd (cmd, "Help", &args)) {
/* HELP: Give some rudimentary help */
    NO_ARGS (fcmd, args);
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "help") > 0);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  } else if (match_cmd (cmd, "CDelete", &args)) {
/* CDELETE: delete to end of line */
    NO_ARGS (fcmd, args);
    if (curs->in_text) {
      cdelete (dd);
      bufcpy (scr->cmd, "", BUFFER_SIZE);
    } else if (curs->in_cmd) {
      CURSOR_TO_COMMAND (dd, curs);
    }
    return;
  } else if (match_cmd (cmd, "RESet", &args)) {
    NO_ARGS (fcmd, args);
    edit_reset (dd);
    dd->bl_start = dd->bl_end = NULL;
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  } else if (match_cmd (cmd, "QQuit", &args)) {
/* QQUIT: quit immediately - live with it like a Klingon  */
    NO_ARGS (fcmd, args);
    if ((dd->undo[0]) != NULL_CHAR) {
      (void) remove (dd->undo);
    }
    longjmp (dd->edit_exit_label, 1);
  } else if (match_cmd (cmd, "Page", &args)) {
/* PAGE n: trace forward or backward by "drawing" the screen */
    int k, n = int_arg (dd, fcmd, args, &rest, 1);
    if (!EMPTY_STRING (rest)) {
      TRAILING (fcmd);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  /* Count */
    for (k = 0; k < abs (n); k++) {
      int lin = count_reserved (dd);
      LINE *z = dd->curr, *u = z;
      for (; IN_TEXT (z);) {
        lin += lines_on_scr (dd, z);
        if (lin > LINES) {
          break;
        }
        u = z;
        (n > 0 ? forward_line (&z) : backward_line (&z));
      }
      if (lin > LINES) {
        dd->curr = u;
      } else {
        dd->curr = z;
      }
      align_current (dd);
    }
    CURSOR_TO_COMMAND (dd, curs);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  } else if (match_cmd (cmd, "CAse", &args)) {
/* CASE: switch case of character under cursor */
    LINE *lin = curs->line;
    NO_ARGS (fcmd, args);
    if (lin != NULL && curs->index < (int) strlen (TEXT (lin))) {
      if (IS_UPPER (TEXT (lin)[curs->index])) {
        TEXT (lin)[curs->index] = TO_LOWER (TEXT (lin)[curs->index]);
        curs->index++;
        curs->sync_line = lin;
        curs->sync_index = curs->index;
        curs->sync = A68_TRUE;
        dd->alts++;
        bufcpy (scr->cmd, "", BUFFER_SIZE);
      } else if (IS_LOWER (TEXT (lin)[curs->index])) {
        TEXT (lin)[curs->index] = TO_UPPER (TEXT (lin)[curs->index]);
        curs->index++;
        curs->sync_line = lin;
        curs->sync_index = curs->index;
        curs->sync = A68_TRUE;
        dd->alts++;
        bufcpy (scr->cmd, "", BUFFER_SIZE);
      }
    }
    return;
  } else if (match_cmd (cmd, "Add", &args)) {
/* ADD [repetition]: add lines */
    LINE *z = dd->curr;
    int k, n = int_arg (dd, fcmd, args, &rest, 1);
    if (!EMPTY_STRING (rest)) {
      TRAILING (fcmd);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
    if (z != NULL && NOT_EOF (z)) {
      for (k = 0; k < n; k ++) {
        curs->line = z;
        curs->index = (int) strlen (TEXT (z));
        split_line (dd, fcmd);
      }
      edit_reset (dd);
      dd->bl_start = dd->bl_end = NULL;
  /* Cursor goes to next appended line, not the current line */
      curs->line = NEXT (z);
      SELECT (curs->line) = A68_TRUE;
      curs->index = 0;
      curs->sync_line = curs->line;
      curs->sync_index = 0;
      curs->sync = A68_TRUE;
      bufcpy (scr->cmd, "", BUFFER_SIZE);
    } else {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot add lines here", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
    }
    return;
  } else if (match_cmd (cmd, "DELete", &args)) {
/* DELETE [/target/]: delete lines */
    LINE *u, *z;
    int dels = 0;
    if (EMPTY_STRING (args)) {
      u = dd->curr;
      forward_line (&u);
    } else {
      u = get_target (dd, fcmd, args, &rest, A68_TRUE);
    }
    if (!EMPTY_STRING (rest)) {
      TRAILING (fcmd);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  /* Backwards range */
    if (NOT_EOF (u) && (NUMBER (u) < NUMBER (dd->curr))) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: backward range", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
    edit_write_undo_file (dd, fcmd);
    for (z = dd->curr; z != u && IN_TEXT (z); forward_line (&z)) {
      curs->line = z;
      curs->last = z;
      curs->index = 0;
      cdelete (dd);
      join_line (dd, fcmd);
      dels++;
    }
    dd->curr = z;
    align_current (dd);
    edit_reset (dd);
    dd->bl_start = dd->bl_end = NULL;
    if (dels == 0) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: deleted no lines", fcmd) >= 0);
    } else if (dels == 1) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: deleted 1 line", fcmd) >= 0);
    } else {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: deleted %d lines", fcmd, dels) >= 0);
    }
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "FILE", &args) || match_cmd (cmd, "WQ", &args)) {
/* FILE: save and quit */
    if (EMPTY_STRING (args)) {
      edit_write (dd, fcmd, dd->name, dd->tof, NULL);
      dd->alts = 0;
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
    } else {
      LINE *u = get_target (dd, fcmd, args, &rest, A68_TRUE);
      SKIP_WHITE (rest);
      if (EMPTY_STRING (rest)) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: missing filename", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      edit_write (dd, fcmd, rest, dd->curr, u);
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
    }
    if (errno == 0) {
      if ((dd->undo[0]) != NULL_CHAR) {
        (void) remove (dd->undo);
      }
      longjmp (dd->edit_exit_label, 1);
    } else {
      CURSOR_TO_COMMAND (dd, curs);
    }
    return;
  } else if (match_cmd (cmd, "Read", &args)) {
/* READ: read a dataset */
    if (EMPTY_STRING (args)) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: missing filename", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      edit_write_undo_file (dd, fcmd);
      edit_read (dd, fcmd, args, NULL);
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "PF", &args)) {
    int n = int_arg (dd, fcmd, args, &rest, 1);
    if (n < 1 || n > MAX_PF) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot set f%d", fcmd, n) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      ASSERT (snprintf (pf_bind[n - 1], SNPRINTF_SIZE, "%s", rest) >= 0);
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "SAve", &args) || match_cmd (cmd, "Write", &args)) {
/* Write: save the dataset */
    if (EMPTY_STRING (args)) {
      edit_write (dd, fcmd, dd->name, dd->tof, NULL);
      dd->alts = 0;
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      LINE *u = get_target (dd, fcmd, args, &rest, A68_TRUE);
      SKIP_WHITE (rest);
      if (EMPTY_STRING (rest)) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: missing filename", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      edit_write (dd, fcmd, rest, dd->curr, u);
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "RUN", &args)) {
    char ccmd[BUFFER_SIZE];
    int ret;
    LINE *cursav = dd->curr;
    if (dd->msgs != -1) {
      close (dd->msgs);
      dd->msgs = -1;
    }
    (void) remove (A68_DIAGNOSTICS_FILE);
    if (EMPTY_STRING (args)) {
      ASSERT (snprintf (ccmd, SNPRINTF_SIZE, "a68g --tui %s", A68_CHECK_FILE) >= 0);
    } else {
      ASSERT (snprintf (ccmd, SNPRINTF_SIZE, "a68g %s --tui %s", args, A68_CHECK_FILE) >= 0);
    }
    dd->curr = NEXT (dd->tof);
    ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "write * %s", A68_CHECK_FILE) >= 0);
    edit_do_cmd (dd);
    endwin ();
    ret = system (ccmd);
    edit_init_curses (dd);
    dd->curr = cursav;
    ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "message") >= 0);
    edit_do_cmd (dd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "SYntax", &args)) {
    ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "run --pedantic --norun") >= 0);
    edit_do_cmd (dd);
    return;
  } else if (match_cmd (cmd, "MEssage", &args)) {
    NO_ARGS (fcmd, args);
    if (dd->msgs == -1) {
      /* Open the file */
      RESET_ERRNO;
      dd->msgs = open (A68_DIAGNOSTICS_FILE, A68_READ_ACCESS);
      if (dd->msgs == -1 || errno != 0) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot open diagnostics file for reading", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
    }
    if (dd->msgs != -1) {
      int n, m;
      LINE *z;
      edit_read_string (dd, dd->msgs);
      if (strlen (dd->linbuf) == 0) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: no diagnostic to display", fcmd) >= 0);
        (void) close (dd->msgs);
        dd->msgs = -1;
        bufcpy (scr->cmd, "", BUFFER_SIZE);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }  
      n = int_arg (dd, cmd, &((dd->linbuf)[0]), &rest, 1);
      if (n == WRONG_TARGET) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: wrong target in file", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      } else {
        for (z = dd->tof; z != NULL && NUMBER (z) != n; forward_line (&z)) {;}
        if (z != NULL) {
          dd->curr = z;
          align_current (dd);
        }
      }
      edit_read_string (dd, dd->msgs);
      curs->sync_index = 0;
      if (n != 0 && n != WRONG_TARGET) {
        m = int_arg (dd, cmd, &((dd->linbuf)[0]), &rest, 1);
        if (m >= 0 && m < (int) strlen (TEXT (dd->curr))) {
          curs->sync_index = m;
        }
      }
      curs->sync_line = dd->curr;
      curs->sync = A68_TRUE;
      edit_read_string (dd, dd->msgs);
      if ((int) strlen (dd->linbuf) >= COLS) {
        (dd->linbuf)[COLS - 4] = NULL_CHAR;
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s ...", dd->linbuf) >= 0);
      } else {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s", dd->linbuf) >= 0);
      }
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      return;
    }
  } else if (match_cmd (cmd, "SCale", &args)) {
/* SCALE OFF|n: set scale row */
    if (match_cmd (args, "OFF", &rest)) {
      if (!EMPTY_STRING (rest)) {
        TRAILING (fcmd);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      scr->scale_row = A68_MAX_INT;
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      int res = count_reserved (dd);
      int n = int_arg (dd, fcmd, args, &rest, 1 + res / 2 + (LINES - res) / 2);
      if (!EMPTY_STRING (rest)) {
        TRAILING (fcmd);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      if ((reserved_row (dd, n - 1) && (n - 1) != scr->scale_row) || n < 2 || n > LINES) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot place scale at row %d", fcmd, n) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      scr->scale_row = n - 1;
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "MSG", &args)) {
/* MSG: display its argument in the message area */
    ARGS (fcmd, args);
    bufcpy (scr->dl0, args, BUFFER_SIZE);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "Undo", &args)) {
/* UNDO: restore to state last saved, if any */
    edit_read_undo_file (dd, fcmd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "DUmp", &args)) {
    FILE_T fd;
    char buff[BUFFER_SIZE];
    int j, k;
    NO_ARGS (fcmd, args);
    RESET_ERRNO;
    fd = open (".a68g.edit.dump", A68_WRITE_ACCESS, A68_PROTECTION);
    CHECK_ERRNO (fcmd);
    if (fd == -1) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: cannot open file for writing", fcmd) >= 0);
      return;
    }
    for (j = 0; j < LINES; j++) {
      for (k = 0; k < COLS; k++) {
        move (j, k);
        ASSERT (snprintf (buff, SNPRINTF_SIZE, "%c", (char) inch ()) >= 0);
        WRITE (fd, buff);
      }
      WRITE (fd, "\n");
    }
    RESET_ERRNO;
    close (fd);
    CHECK_ERRNO (fcmd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "Edit", &args) || match_cmd (cmd, "Xedit", &args)) {
/* EDIT filename: recursively edit another file */
    DATASET dataset;
    edit_write_undo_file (dd, fcmd);
    edit_dataset (&dataset, dd->num + 1, args, NULL);
    fixed_heap_pointer = dd->heap_pointer;
    dd->linbuf = NULL;
    dd->linsiz = 0;
    dd->tof = NULL;
    edit_read_undo_file (dd, fcmd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
/* Commands with targets */
  if (match_cmd (cmd, "FOld", &args)) {
/* FOLD [[TO] /target/]: select lines */
    LINE *z;
    if (!EMPTY_STRING (args) && match_cmd (args, "TO", &rest)) {
  /* Select all lines upto matching target */
      LINE *u;
      args = rest;
      u = get_target (dd, fcmd, args, &rest, A68_FALSE);
      if (!EMPTY_STRING (rest)) {
        TRAILING (fcmd);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      if (!IN_TEXT (u)) {
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
  /* Backwards range */
      if (NOT_EOF (u) && (NUMBER (u) < NUMBER (dd->curr))) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: backward range", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
  /* Empty range */
      if (u == dd->curr) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: empty range", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      for (z = dd->tof; z != NULL; FORWARD (z)) {
        SELECT (z) = A68_FALSE;
      }
      for (z = dd->curr; z != u; FORWARD (z)) {
        SELECT (z) = A68_TRUE;
      }
      dd->subset = A68_TRUE;
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
  /* FOLD [/target/]: select lines matching a target */
    /* Reset all lines */
      if (!EMPTY_STRING (args)) {
    /* Select all lines that match target */
        LINE *u = get_target (dd, fcmd, args, &rest, A68_FALSE);
        if (!EMPTY_STRING (rest)) {
          TRAILING (fcmd);
          CURSOR_TO_COMMAND (dd, curs);
          return;
        }
        if (!IN_TEXT (u)) {
          CURSOR_TO_COMMAND (dd, curs);
          return;
        }
        for (z = dd->tof; z != NULL; FORWARD (z)) {
          SELECT (z) = A68_FALSE;
        }
        SELECT (u) = A68_TRUE;
        for (z = NEXT (u); z != NULL; FORWARD (z)) {
          SELECT (z) = match_regex (dd, z, 0, fcmd);
        }
        dd->subset = A68_TRUE;
      } else {
  /* UNFOLD */
        for (z = dd->tof; z != NULL; FORWARD (z)) {
          SELECT (z) = A68_TRUE;
        }
        dd->subset = A68_FALSE;
      }
      dd->curr = dd->tof;
      forward_line (&(dd->curr));
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "Move", &args)) {
/* MOVE /target/ /target/ [n]: move lines */
    move_copy (dd, cmd, args, A68_TRUE);
    return;
  } else if (match_cmd (cmd, "COpy", &args)) {
/* COPY /target/ /target/ [n]: copy lines */
    move_copy (dd, cmd, args, A68_FALSE);
  } else if (match_cmd (cmd, "SHell", &args)) {
    if (EMPTY_STRING (args)) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: missing arguments", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      LINE *u = get_target (dd, fcmd, args, &rest, A68_TRUE);
      SKIP_WHITE (rest);
      if (EMPTY_STRING (rest)) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: missing shell command", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      edit_write_undo_file (dd, fcmd);
      edit_filter (dd, fcmd, rest, u);
      bufcpy (scr->cmd, "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "Indent", &args)) {
/* INDENT target column: indent lines to column */
    indent (dd, fcmd, args);
    return;
  } else if (match_cmd (cmd, "S", &args) || match_cmd (cmd, "C", &args)) {
/* SUBSTITUTE /find/replace/ [/target/] [repetition]: replace substrings */
    int reps, start, subs = 0;
    if (!get_subst (dd, fcmd, args, &rest)) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: unrecognised syntax", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
    if (EMPTY_STRING (rest)) {
      int m = substitute (dd, dd->curr, A68_MAX_INT, 1, fcmd);
      if (m == SUBST_ERROR) {
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      subs = m;
    } else {
      LINE *u, *z;
      int m;
      if (! EMPTY_STRING (rest)) {
        args = rest;
        u = get_target (dd, fcmd, args, &rest, A68_TRUE);
      } else {
        u = dd->curr;
        forward_line (&u);
      }
      if (! EMPTY_STRING (rest)) {
        args = rest;
        reps = int_arg (dd, fcmd, args, &rest, A68_MAX_INT);
      } else {
        reps = A68_MAX_INT; /* Default is global substitution */
      }
      if (! EMPTY_STRING (rest)) {
        args = rest;
        start = int_arg (dd, fcmd, args, &rest, 1);
      } else {
        start = 1;
      }
      if (! EMPTY_STRING (rest)) {
        TRAILING (fcmd);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
/* Backwards range */
      if (NOT_EOF (u) && (NUMBER (u) < NUMBER (dd->curr))) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: backward range", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      edit_write_undo_file (dd, fcmd);
      for (z = dd->curr; z != u && IN_TEXT (z); forward_line (&z)) {
        m = substitute (dd, z, reps, start, fcmd);
        if (m == SUBST_ERROR) {
          CURSOR_TO_COMMAND (dd, curs);
          return;
        }
        subs += m;
      }
      if (IN_TEXT (z)) {
        dd->curr = z;
      }
    }
    if (subs == 1) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %d occurences %sd", fcmd, subs, fcmd) >= 0);
    } else {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "%s: %d occurences %sd", fcmd, subs, fcmd) >= 0);
    }
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else {
/* Give error and clear the last command - sorry for the inconvenience */
    ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "edit: undefined command \"%s\"", cmd) >= 0);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
}

/*
\brief do a prefix command
\param dd current dataset
**/

static void edit_do_prefix (DATASET *dd)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  int as, cs, ccs, ds, dds, is, iis, js, xs, xxs, ps, qs, divs, total;
  LINE *z;
  char *arg;
/* Check prefix command */
  as = cs = ccs = ds = dds = is = iis = js = xs = xxs = ps = qs = divs = total = 0;
  for (z = dd->tof; z != NULL; FORWARD (z)) {
    char *p = z->precmd;
    SKIP_WHITE (p);
    if (match_cmd (p, "CC", &arg)) {
      if (NUMBER (z) == 0) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "copy: CC in invalid line") >= 0);
      } else {
        ccs++;
        total++;
      }
    } else if (match_cmd (p, "DD", &arg)) {
      if (NUMBER (z) == 0) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "delete: DD in invalid line") >= 0);
      } else {
        dds++;
        total++;
      }
    } else if (match_cmd (p, "II", &arg)) {
      if (NUMBER (z) == 0) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "indent: II in invalid line") >= 0);
      } else {
        iis++;
        total++;
      }
    } else if (match_cmd (p, "XX", &arg)) {
      if (NUMBER (z) == 0) {
        ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "move: XX in invalid line") >= 0);
      } else {
        xxs++;
        total++;
      }
    } else if (match_cmd (p, "A", &arg)) {
      as++;
      total++;
    } else if (match_cmd (p, "C", &arg)) {
      cs++;
      total++;
    } else if (match_cmd (p, "D", &arg)) {
      ds++;
      total++;
    } else if (match_cmd (p, "J", &arg)) {
      js++;
      total++;
    } else if (match_cmd (p, "I", &arg)) {
      is++;
      total++;
    } else if (match_cmd (p, "P", &arg)) {
      ps++;
      total++;
    } else if (match_cmd (p, "Q", &arg)) {
      qs++;
      total++;
    } else if (match_cmd (p, "X", &arg)) {
      xs++;
      total++;
    } else if (match_cmd (p, "/", &arg)) {
      divs++;
      total++;
    }
  }
/* Execute the command */
  if (as == 1 && total == 1) {
/* ADD */
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      LINE *cursavi = dd->curr;
      SKIP_WHITE (p);
      if (match_cmd (p, "A", &arg)) {
        dd->curr = z;
        ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "add %s", arg) >= 0);
        edit_do_cmd (dd);
        edit_reset (dd);
        dd->bl_start = dd->bl_end = NULL;
        dd->curr = cursavi;
        align_current (dd);
        bufcpy (scr->cmd, "", BUFFER_SIZE);
        return;
      }
    }
  } else if (is == 1 && total == 1) {
/* INDENT */
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      LINE *cursavi = dd->curr;
      SKIP_WHITE (p);
      if (match_cmd (p, "I", &arg)) {
        dd->curr = z;
        ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "indent 1 %s", arg) >= 0);
        edit_do_cmd (dd);
        edit_reset (dd);
        dd->curr = cursavi;
        align_current (dd);
        bufcpy (scr->cmd, "", BUFFER_SIZE);
        return;
      }
    }
  } else if (js == 1 && total == 1) {
/* JOIN */
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      LINE *cursavi = dd->curr;
      SKIP_WHITE (p);
      if (match_cmd (p, "J", &arg)) {
        LINE *x = NEXT (z);
        NO_ARGS ("J", arg);
        if (NUMBER (z) == 0 || x == NULL || NUMBER (x) == 0) {
          ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "join: cannot join") >= 0);
        } else {
          dd->curr = x;
          curs->line = x;
          curs->last = x;
          curs->index = 0;
          join_line (dd, "join");
        } 
        edit_reset (dd);
        dd->bl_start = dd->bl_end = NULL;
        dd->curr = cursavi;
        align_current (dd);
        bufcpy (scr->cmd, "", BUFFER_SIZE);
        return;
      }
    }
  } else if (ds == 1 && total == 1) {
/* DELETE */
    LINE *w;
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      LINE *cursavi = dd->curr;
      SKIP_WHITE (p);
      if (match_cmd (p, "D", &arg)) {
        w = dd->curr;
        dd->curr = z;
        if (EMPTY_STRING (arg)) {
          ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "delete") >= 0);
        } else {
          ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "delete %s", arg) >= 0);
        }
        edit_do_cmd (dd);
        if (w == z) {
          forward_line (&w);
        }
        dd->curr = w;
        edit_reset (dd);
        dd->bl_start = dd->bl_end = NULL;
        dd->curr = cursavi;
        align_current (dd);
        bufcpy (scr->cmd, "", BUFFER_SIZE);
        return;
      }
    }
  } else if (divs == 1 && total == 1) {
/* Set current line */
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      SKIP_WHITE (p);
      if (match_cmd (p, "/", &arg)) {
        NO_ARGS ("/", arg);
        dd->curr = z;
        edit_reset (dd);
        align_current (dd);
        bufcpy (scr->cmd, "", BUFFER_SIZE);
        return;
      }
    }
  } else if (dds == 2 && total == 2) {
/* DELETE block */
    LINE *u = NULL, *v = NULL, *w, *cursavi = dd->curr;
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      SKIP_WHITE (p);
      if (match_cmd (p, "DD", &arg)) {
        NO_ARGS ("DD", arg);
        if (u == NULL) {
          u = z;
        } else {
          v = z;
        }
      }
    }
    w = dd->curr;
    dd->curr = u;
    if (IS_EOF (v)) {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "delete *") >= 0);
    } else {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "delete :%d+1", NUMBER (v)) >= 0);
    }
    edit_do_cmd (dd);
    if (NUMBER (u) <= NUMBER (w) && NUMBER (w) <= NUMBER (v)) {
      dd->curr = PREVIOUS (w);
    } else {
      dd->curr = w;
    }
    dd->curr = cursavi;
    edit_reset (dd);
    dd->bl_start = dd->bl_end = NULL;
    align_current (dd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  } else if (iis == 2 && total == 2) {
/* INDENT block */
    LINE *u = NULL, *v = NULL, *cursavi = dd->curr;
    char upto[BUFFER_SIZE];
    char *rep = NULL;
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      SKIP_WHITE (p);
      if (match_cmd (p, "II", &arg)) {
        if (u == NULL) {
          rep = arg;
          u = z;
        } else {
          NO_ARGS ("indent", arg);
          v = z;
        }
      }
    }
    dd->curr = u;
    if (IS_EOF (v)) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, "*") >= 0);
    } else if (NOT_EOF (v)) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, ":%d+1", NUMBER (v)) >= 0);
    }
    if (EMPTY_STRING (rep)) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "indent: expected column number") >= 0);
    } else {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "indent %s %s", upto, rep) >= 0);
      edit_do_cmd (dd);
      edit_reset (dd);
    }
    dd->curr = cursavi;
    align_current (dd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  } else if ((ccs == 2 || xxs == 2) && (ps == 1 || qs == 1) && total == 3) {
/* COPY or MOVE block */
    LINE *u = NULL, *v = NULL, *w = NULL, *cursavi = dd->curr;
    char upto[BUFFER_SIZE], ins[BUFFER_SIZE];
    char *cmd = NULL, *delim = NULL, *rep = NULL;
    if (ccs == 2) {
      cmd = "copy";
      delim = "CC";
    } else if (xxs == 2) {
      cmd = "move";
      delim = "XX";
    }
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      SKIP_WHITE (p);
      if (match_cmd (p, delim, &arg)) {
        NO_ARGS (delim, arg);
        if (u == NULL) {
          u = z;
        } else {
          v = z;
        }
      } else if (match_cmd (p, "P", &arg)) {
        rep = arg;
        w = z;
      } else if (match_cmd (p, "Q", &arg)) {
        rep = arg;
        w = z;
      }
    }
    dd->curr = u;
    if (IS_EOF (v)) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, "*") >= 0);
    } else if (NOT_EOF (v)) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, ":%d+1", NUMBER (v)) >= 0);
    }
    if (IS_EOF (w) && ps == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, "*") >= 0);
    } else if (IS_EOF (w) && qs == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, "*-1") >= 0);
    } else if (NOT_EOF (w) && ps == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, ":%d", NUMBER (w)) >= 0);
    } else if (NOT_EOF (w) && qs == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, ":%d-1", NUMBER (w)) >= 0);
    }
    if (EMPTY_STRING (rep)) {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "%s %s %s", cmd, upto, ins) >= 0);
    } else {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "%s %s %s %s", cmd, upto, ins, rep) >= 0);
    }
    edit_do_cmd (dd);
    edit_reset (dd);
    dd->curr = cursavi;
    align_current (dd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  } else if ((cs == 1 || xs == 1) && (ps == 1 || qs == 1) && total == 2) {
/* COPY or MOVE line */
    LINE *u = NULL, *w = NULL, *cursavi = dd->curr;
    char ins[BUFFER_SIZE];
    char *cmd = NULL, *delim = NULL, *target = NULL, *rep = NULL;
    if (cs == 1) {
      cmd = "copy";
      delim = "C";
    } else if (xs == 1) {
      cmd = "move";
      delim = "X";
    }
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      SKIP_WHITE (p);
      if (match_cmd (p, delim, &arg)) {
        target = arg;
        u = z;
      } else if (match_cmd (p, "P", &arg)) {
        rep = arg;
        w = z;
      } else if (match_cmd (p, "Q", &arg)) {
        rep = arg;
        w = z;
      }
    }
    dd->curr = u;
    if (IS_EOF (w) && ps == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, "*") >= 0);
    } else if (IS_EOF (w) && qs == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, "*-1") >= 0);
    } else if (NOT_EOF (w) && ps == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, ":%d", NUMBER (w)) >= 0);
    } else if (NOT_EOF (w) && qs == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, ":%d-1", NUMBER (w)) >= 0);
    }
    if (EMPTY_STRING (target) && EMPTY_STRING (rep)) {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "%s 1 %s", cmd, ins) >= 0);
    } else if (EMPTY_STRING (target) && !EMPTY_STRING (rep)) {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "%s 1 %s %s", cmd, ins, rep) >= 0);
    } else if (!EMPTY_STRING (target) && EMPTY_STRING (rep)) {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "%s %s %s", cmd, target, ins) >= 0);
    } else if (!EMPTY_STRING (target) && !EMPTY_STRING (rep)) {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "%s %s %s %s", cmd, target, ins, rep) >= 0);
    }
    ASSERT (snprintf (scr->tmp_text, SNPRINTF_SIZE, "%s", scr->cmd) >= 0);
    edit_do_cmd (dd);
    edit_reset (dd);
    dd->curr = cursavi;
    align_current (dd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  } else if ((ps == 1 || qs == 1) && total == 1) {
/* COPY previous block */
    LINE *u = dd->bl_start, *v = dd->bl_end, *w = NULL, *cursavi = dd->curr;
    char upto[BUFFER_SIZE], ins[BUFFER_SIZE];
    char *cmd = "copy", *rep = NULL;
    if (u == NULL || v == NULL) {
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "copy: no previous block") >= 0);
      return;
    }
    for (z = dd->tof; z != NULL; FORWARD (z)) {
      char *p = z->precmd;
      SKIP_WHITE (p);
      if (match_cmd (p, "P", &arg)) {
        rep = arg;
        w = z;
      } else if (match_cmd (p, "Q", &arg)) {
        rep = arg;
        w = z;
      }
    }
    dd->curr = u;
    if (IS_EOF (v)) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, "*") >= 0);
    } else if (u == v) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, "1") >= 0);
    } else if (NOT_EOF (v)) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, ":%d+1", NUMBER (v)) >= 0);
    }
    if (IS_EOF (w) && ps == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, "*") >= 0);
    } else if (IS_EOF (w) && qs == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, "*-1") >= 0);
    } else if (NOT_EOF (w) && ps == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, ":%d", NUMBER (w)) >= 0);
    } else if (NOT_EOF (w) && qs == 1) {
      ASSERT (snprintf (ins, BUFFER_SIZE - 1, ":%d-1", NUMBER (w)) >= 0);
    }
    if (EMPTY_STRING (rep)) {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "%s %s %s", cmd, upto, ins) >= 0);
    } else {
      ASSERT (snprintf (scr->cmd, SNPRINTF_SIZE, "%s %s %s %s", cmd, upto, ins, rep) >= 0);
    }
    edit_do_cmd (dd);
    edit_reset (dd);
    dd->curr = cursavi;
    align_current (dd);
    bufcpy (scr->cmd, "", BUFFER_SIZE);
    return;
  }
  ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "edit: unrecognised prefix command") >= 0);
}

/*
\brief execute command from function key
\param dd current dataset
\param cmd command bound to key
**/

static void key_command (DATASET *dd, char *cmd)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  LINE *old = dd->curr;
  SAVE_CURSOR (dd, curs);
  bufcpy (scr->cmd, cmd, BUFFER_SIZE);
  edit_do_cmd (dd);
  CURSOR_TO_SAVE (dd, curs);
  if (old == dd->curr) {
    dd->refresh = A68_FALSE;
  } else {
    clear ();
  }
}

/*
\brief editor loop: get a key and do something with it
\param dd current dataset
**/

static void edit_loop (DATASET *dd)
{
  DISPLAY *scr = &(dd->display);
  CURSOR *curs = &(scr->curs);
  for (;;) {
    int ch, k;
    if (dd->refresh) {
      edit_draw (dd);
      refresh ();
    }
    dd->refresh = A68_TRUE;
    bufcpy (scr->dl0, "", BUFFER_SIZE);
    ch = wgetch (stdscr);
    if (ch == ESCAPE_CHAR) {
/*
Curses does not decode all keys on all terminals.
You may notice this when some function keys appear as escape sequences.
This is probably a bug of those terminals; we work around a number of them:
Get some CSI/SS2/SS3 sequences from different terminals.
*/
      char esc[8] = {NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR,
                     NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR};
      int j = 0, n = 0, m = 0;
      BOOL_T cont = A68_TRUE;
      while (cont && j < 6) {
        esc[j] = (char) ch;
        /* mvprintw (j, 0, "'%d'", ch); */
        n = 0;
        for (k = 0; dec_key[k].code >= 0; k++) {
          if (strncmp (dec_key[k].name, esc, (size_t) (j + 1)) == 0) {
            n++;
            m = k;
          }
        }
        if (n == 0) {
          ch = '~';
          ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "edit: undefined escape sequence %s", &esc[1]) >= 1);
          cont = A68_FALSE;
        } else if (n == 1) {
          for (j++ ; j != (int) strlen (dec_key[m].name); j++) {
            esc[j] = (char) wgetch (stdscr);
          }
          if (strcmp (dec_key[m].name, esc) == 0) {
            ch = dec_key[m].code;
          } else {
            ch = '~';
            ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "edit: undefined escape sequence %s", &esc[1]) >= 1);
          }
          cont = A68_FALSE;
        } else if (n > 1) {
          ch = wgetch (stdscr);
          j++;
        }
      }
    }
/* Substitute keys for uniform behaviour */
    for (k = 0; trans_tab[k].code >= 0; k++) {
      if (ch == trans_tab[k].code) {
        ch = trans_tab[k].trans;
      }
    }
/* Interprete the key */
    if (KEY_F0 < ch && ch <= KEY_F0 + 24) {
/* PF keys */
      for (k = 0; k < 24; k++) {
        if (ch == KEY_F0 + k + 1) {
          if ((int) strlen (pf_bind[k]) == 0) {
            ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "edit: PF%02d has no command", k + 1) >= 0);
          } else {
            SAVE_CURSOR (dd, curs);
            bufcpy (scr->cmd, pf_bind[k], BUFFER_SIZE);
            edit_do_cmd (dd);
            if (! match_cmd (pf_bind[k], "TOGGLE", NULL) && ! match_cmd (pf_bind[k], "CASE", NULL)) {
              CURSOR_TO_SAVE (dd, curs);
            }
          }
        }
      }
/* Prefix editing */
    } else if (ch <= UCHAR_MAX && IS_PRINT (ch) && curs->in_prefix) {
      edit_prefix (dd, ch);
    } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE || ch == KEY_DC) && curs->in_prefix) {
      edit_prefix (dd, ch);
    } else if (ch == NEWLINE_CHAR && curs->in_prefix) {
      SAVE_CURSOR (dd, curs);
      edit_do_prefix (dd);
      CURSOR_TO_SAVE (dd, curs);
      clear ();
/* Command line editing */
    } else if (ch <= UCHAR_MAX && IS_PRINT (ch) && curs->in_cmd) {
      edit_cmd (dd, ch);
    } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE || ch == KEY_DC) && curs->in_cmd) {
      edit_cmd (dd, ch);
    } else if (ch == NEWLINE_CHAR && curs->in_cmd) {
      edit_do_cmd (dd);
      clear ();
      curs->row = -1;
      curs->col = -1;
/* Text editing */
    } else if (ch <= UCHAR_MAX && (IS_PRINT (ch) || ch == '\t') && curs->in_text) {
      edit_text (dd, ch);
    } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE || ch == KEY_DC) &&  !curs->in_forbidden) {
      edit_text (dd, ch);
    } else if (ch == NEWLINE_CHAR && !curs->in_forbidden) {
      split_line (dd, "edit");
      edit_reset (dd);
      dd->bl_start = dd->bl_end = NULL;
      if (dd->size == 1) {
        dd->curr = NEXT (dd->tof);
      }
    } else if (ch == KEY_RESIZE) {
/* Resize event triggers reinitialisation */
      endwin ();
      edit_init_curses (dd);
    } else if (ch == KEY_MOUSE) {
/* Mouse control is basic: you can point with the cursor but little else */
      MEVENT event;
      if (getmouse (&event) == OK) {
        curs->bstate = event.bstate;
        if (event.bstate & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | 
                            BUTTON1_PRESSED | BUTTON1_RELEASED)) {
          if (reserved_row (dd, event.y) && event.y != scr->cmd_row) {
            PROTECTED ("edit");
          } else {
            curs->row = event.y;
            curs->col = event.x;
          }
        } else if (event.bstate & (BUTTON3_CLICKED | BUTTON3_DOUBLE_CLICKED | 
                                   BUTTON3_PRESSED | BUTTON3_RELEASED)) {
          if (reserved_row (dd, event.y) && event.y != scr->cmd_row) {
            PROTECTED ("edit");
          } else {
            curs->row = event.y;
            curs->col = event.x;
          }
        }
      }
/* Keyboard control keys */
    } else if (ch == KEY_UP) {
      int u = curs->row;
      do {
        u = (u == 0 ? LINES - 1 : u - 1);
      } while (reserved_row (dd, u) && u != scr->cmd_row);
      curs->row = u;
    } else if (ch == KEY_DOWN) {
      int u = curs->row;
      do {
        u = (u == LINES - 1 ? 0 : u + 1);
      } while (reserved_row (dd, u) && u != scr->cmd_row);
      curs->row = u;
    } else if (ch == KEY_SR || ch == KEY_CTRL('W')) {
      int h = histix;
      if (h == 0) {
        h = HISTORY - 1;
      } else {
        h --;
      }
      if ((int) strlen (history[h]) != 0) {
        bufcpy (scr->cmd, history[h], BUFFER_SIZE);
        histix = h;
      }
    } else if (ch == KEY_SF || ch == KEY_CTRL ('X')) {
      int h = histix;
      if (h == HISTORY - 1) {
        h = 0;
      } else {
        h ++;
      }
      if ((int) strlen (history[h]) != 0) {
        bufcpy (scr->cmd, history[h], BUFFER_SIZE);
        histix = h;
      }
    } else if (ch == KEY_RIGHT) {
      curs->col = (curs->col == COLS - 1 ? 0 : curs->col + 1);
    } else if (ch == KEY_LEFT) {
      curs->col = (curs->col == 0 ? COLS - 1 : curs->col - 1);
    } else if (ch == KEY_NPAGE || ch == KEY_C3) {
      key_command (dd, "page +1");
    } else if (ch == KEY_PPAGE || ch == KEY_A3) {
      key_command (dd, "page -1");
    } else if (ch == KEY_HOME || ch == KEY_A1) {
      key_command (dd, "-*");
    } else if (ch == KEY_END || ch == KEY_C1) {
      key_command (dd, "+*");
    } else if (ch == KEY_SLEFT || ch == KEY_CTRL('A')) {
      if (curs->in_text || curs->in_cmd) {
        curs->index = 0;
        curs->col = MARGIN;
      } else {
        PROTECTED ("edit");
      }
    } else if (ch == KEY_SRIGHT || ch == KEY_CTRL ('D')) {
      if (curs->in_text) {
        curs->index = (int) strlen (TEXT (curs->line));
        curs->sync_index = curs->index;
        curs->sync_line = curs->line;
        curs->sync = A68_TRUE;
      } else if (curs->in_cmd) {
        curs->index = (int) strlen (scr->cmd);
        curs->col = MARGIN + curs->index;
      } else {
        PROTECTED ("edit");
      }
    } else if (ch == KEY_B2) {
      if (curs->in_text) {
        if (curs->line != NULL && NUMBER (curs->line) > 0) {
          dd->curr = curs->line;
          curs->sync_index = 0;
          curs->sync_line = dd->curr;
          curs->sync = A68_TRUE;
        }
      } else {
        PROTECTED ("edit");
      }
/* Other keys */
    } else if (ch == KEY_IC) {
      scr->ins_mode = !scr->ins_mode;
/* Alas, unknown. File a complaint. */
    } else if (ch > 127) {
      if (curs->in_forbidden) {
        PROTECTED ("edit");
        goto end;
      }
      for (k = 0; key_tab[k].code >= 0; k++) {
        if (ch == key_tab[k].code) {
          ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "edit: undefined key %s", key_tab[k].name) >= 1);
          goto end;
        }
      }
      ASSERT (snprintf (scr->dl0, SNPRINTF_SIZE, "edit: undefined key %d", ch) >= 0);
    }
  end: continue;
  }
}

/*
\brief edit a dataset
\param dd current dataset
\param num number of dataset
\param filename file to edit
\param target optional target for initial current line
**/

static void edit_dataset (DATASET *dd, int num, char *filename, char *target)
{
/* Init ncurses */
  edit_init_curses (dd);
  bufcpy (dd->name, filename, BUFFER_SIZE);
  dd->tabs = TAB_STOP;
  dd->heap_pointer = fixed_heap_pointer;
/* Init edit */
  dd->display.dl0[0] = NULL_CHAR;
  dd->display.cmd[0] = NULL_CHAR;
  dd->linbuf = NULL;
  dd->linsiz = 0;
  dd->collect = A68_FALSE;
  edit_read_initial (dd, "edit");
  XABEND (heap_full (BUFFER_SIZE), "out of memory", NULL);
  dd->collect = A68_TRUE;
  dd->display.ins_mode = A68_TRUE;
  dd->refresh = A68_TRUE;
  dd->msgs = -1; /* File not open */
  dd->num = num;
  dd->undo_line = 0;
  dd->search = 0;
  dd->bl_start = dd->bl_end = NULL;
  if (target != NULL && (int) (strlen (target)) > 0) {
    char *rest;
    LINE *z = get_target (dd, "edit", target, &rest, A68_TRUE);
    if (z != NULL) {
      dd->curr = z;
    } else {
      ASSERT (snprintf (dd->display.dl0, SNPRINTF_SIZE, "edit: optional target not set") >= 0);
    }
  }
  if (!a68g_mkstemp (dd->undo, A68_WRITE_ACCESS, A68_PROTECTION)) {
    (dd->undo)[0] = NULL_CHAR;
    ASSERT (snprintf (dd->display.dl0, SNPRINTF_SIZE, "edit: cannot open temporary file for undo") >= 0);
  }
  remove (dd->undo);
  if (setjmp (dd->edit_exit_label) == 0) {
    edit_loop (dd);
  }
}

/*
\brief edit main routine
\param start_text not used
**/

void edit (char *start_text)
{
  DATASET dataset;
  int k;
  (void) start_text;
  genie_init_rng ();
  for (k = 0; k < HISTORY; k++) {
    bufcpy (history[k], "", BUFFER_SIZE);
  }
  histix = 0;
  for (k = 0; k < MAX_PF; k++) {
    pf_bind[k][0] = NULL_CHAR;
  }
  ASSERT (snprintf (pf_bind[0], SNPRINTF_SIZE, "toggle") >= 0);
  ASSERT (snprintf (pf_bind[1], SNPRINTF_SIZE, "-1") >= 0);
  ASSERT (snprintf (pf_bind[2], SNPRINTF_SIZE, "+1") >= 0);
  ASSERT (snprintf (pf_bind[3], SNPRINTF_SIZE, "again") >= 0);
  ASSERT (snprintf (pf_bind[4], SNPRINTF_SIZE, "case") >= 0);
  ASSERT (snprintf (pf_bind[5], SNPRINTF_SIZE, "cdelete") >= 0);
  ASSERT (snprintf (pf_bind[6], SNPRINTF_SIZE, "syntax") >= 0);
  ASSERT (snprintf (pf_bind[7], SNPRINTF_SIZE, "message") >= 0);
  ASSERT (snprintf (pf_bind[11], SNPRINTF_SIZE, "toggle") >= 0);
  if (program.files.initial_name == NULL) {
    errno = ENOTSUP;
    SCAN_ERROR (A68_TRUE, NULL, NULL, "edit: no filename");
  }
  edit_dataset (&dataset, 1, program.files.initial_name, program.options.target);
/* Exit edit */
  clear ();
  refresh ();
  endwin ();
  (void) remove (A68_DIAGNOSTICS_FILE);
  exit (EXIT_SUCCESS);
}

#endif /* defined HAVE_CURSES_H && defined HAVE_LIBNCURSES && defined HAVE_REGEX_H) */
