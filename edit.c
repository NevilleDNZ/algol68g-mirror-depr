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
as the XEDIT/ISPF editors. 
It is meant for maintaining small to intermediate sized source code. The 
command set is small, and there is no huge file support: the text is in core
which makes for a *fast* editor.

It is still undocumented as it is not fully debugged and not feature-complete. 
I use it for daily editing work, but you *might* loose your work. 
Do not say I did not warn you!

The editor is modeless. If you are in text, what you type goes into the file.
What you type in the prefix, will be a prefix command. What you type on the
command line, is a command. No input-modes or command-mode or escapes.

The editor is also a very basic IDE for Algol 68 Genie, it can for instance 
take you to diagnostic positions in the code.

The editor supports prefix commands and text folding, like the
XEDIT/ISPF editors. Especially the folding is a nice feature.

Your screen looks like this:

+-----------------------------------------------------+
|filename date #1     n/n    col=n alt=n    ins       |<--Id/Message Line
|====>                                                |<--Command Line
|===== * * * Top of Data * * *                        |
|    1                                                |
|    2                                                |
|    3                                                |<--Current Line
|      |...+....1....+....2... ... ...6....+....7....||<--Scale
|    4                                                |
|    5                                                |
|    6                                                |
|===== * * * End of Data * * *                        |
+-----------------------------------------------------+
 |   | |                                             |
 +---+ +---------------------------------------------+
   |                         |
 Prefix Area              File Area


+-- Command overview ---
| target  A target as command, sets the current line
| ADD [n] Add n lines
| AGAIN   Repeat last search
| CASE    Switch case of character under cursor
| CDELETE Delete to end of line
| COPY    Copy up to 1st target to after 2nd target, [n] copies
| DELETE  Delete up to [target]
| INDENT  Indent text to a column
| FILE    Save file [to target filename] and quit
| FOLD    [[TO] target] Folds lines either up TO target, or matching target
| MESSAGE visualise a message from RUN or SYNTAX
| MOVE    target target [n] Up to 1st target to after 2nd target, n copies
| PAGE    [[+-]n|*] Forward or backward paging
| PFn cmd Binds function key n to cmd
| QQUIT   Categorically quit 
| READ    Insert filename after current line
| RESET   Reset prefixes
| RUN     Run with a68g
| SET     CMD TOP|BOTTOM|*|n Place the command line
| SET     IDF OFF|TOP|BOTTOM|*|n Place the file identification line
| SET     SCALE OFF|TOP|BOTTOM|*|n Place the scale
| SHELL   target cmd Filter lines using cmd
| S       Substitute command /find/replace/ [C][target [n|* [m|*]]]
| SYNTAX  Alias for a68g --pedantic --norun
| TOGGLE  Toggle between current line and command line (F1, F12 do the same)
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
| The editor uses POSIX ERE regular expression syntax.
| A prefix ~ to a regexp matches lines not matching regexp.
|
| [~]/regexp/             regexp must [not] match
| [~]/regexp/&[~]/regexp/ both lines must [not] match
| [~]/regexp/|[~]/regexp/ one or both must [not] match
| [~]/regexp/^[~]/regexp/ at most one regexp must [not] match
|
| In substitution you can specify matched subexpression with \1 .. \9
| syntax; furthermore you can specify \a .. \j or \A .. \J which will cast
| the subexpression to either lower - or upper case respectively.
|
| Prefix commands
| /    Make line the current line
| A[n] Add n new lines below this line
| C[n] Copy lines; use P (after) or Q (before) for destination
| CC   Copy block of lines marker; use P (after) or Q (before) for destination
| D[n] Delete lines
| DD   Delete block of lines marker
| I    Indent line relative to column or to absolute column [<|>][n]
| II   Indent block of lines
| J    Join with next line
| P[n] Add n copies after this line
| Q[n] Add n copies before this line
| U    Unselect line from editing
| X[n] Move lines; use P (after) or Q (before) for destination
| XX   Move block of lines marker; use P (after) or Q (before) for destination
+---
*/

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

/* Without CURSES or REGEX, we have no editor, so: */

#if defined HAVE_EDITOR

#define BACKSPACE 127
#define BLANK  "       "
#define BLOCK_SIZE 4
#define BOTSTR "* * * End of Data * * *"
#define DATE_STRING "%d-%m-%y %H:%M:%S"
#define EMPTY_STRING(s) ((s) == NO_TEXT || (s)[0] == NULL_CHAR)
#define FD_READ 0
#define FD_WRITE 1
#define HISTORY 64
#define IS_IN_TEXT(z) ((z) != NO_EDLIN && NUMBER (z) > 0)
#define IS_EOF(z) (!NOT_EOF(z))
#define IS_TOF(z) (!NOT_TOF(z))
#define MARGIN 7
#define MAX_PF 64
#define NOT_EOF(z) ((z) != NO_EDLIN && NEXT (z) != NO_EDLIN)
#define NOT_TOF(z) ((z) != NO_EDLIN && PREVIOUS (z) != NO_EDLIN)
#define PREFIX "====== "
#define PROMPT "=====> "
#define PROTECTED(s) ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cursor in protected area", (s)) >= 0)
#define SUBST_ERROR -1
#define TAB_STOP 8
#define TEXT_WIDTH (COLS - MARGIN)
#define TOPSTR "* * * Top of Data * * *"
#define TRAILING(s) ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: trailing text", (s)) >= 0)
#define WRONG_TARGET (-1)

#define EDIT_TEST(c) {\
  if (! (c)) {\
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: error detected at line %d", __FILE__, __LINE__) >= 0);\
  }}

#if defined HAVE_WIN32
#define true 1
#endif

#define REDRAW(s) {EDIT_TEST (clearok (s, true) != ERR);}

static char pf_bind[MAX_PF][BUFFER_SIZE];
static char history[HISTORY][BUFFER_SIZE];
static int histcurr = -1, histnext = -1, histprev = -1;
static int loop_cnt = 0;

#define KEY_CTRL(n) TO_UCHAR ((int) n - 0x40)

#define SAVE_CURSOR(dd, curs) {\
  ROW0 (curs) = ROW (curs);\
  COL0 (curs) = COL (curs);\
  }

#define CURSOR_TO_SAVE(dd, curs) {\
  ROW (curs) = ROW0 (curs);\
  COL (curs) = COL0 (curs);\
  }

#define CURSOR_TO_CURRENT(dd, curs) {\
  SYNC_LINE (curs) = CURR (dd);\
  SYNC_INDEX (curs) = 0;\
  SYNC (curs) = A68_TRUE;\
  }

#define CURSOR_TO_COMMAND(dd, curs) {\
  ROW (curs) = CMD_ROW (&DISPLAY (dd));\
  COL (curs) = MARGIN;\
  SYNC (curs) = A68_FALSE;\
  }

#define CHECK_ERRNO(cmd) {\
  if (errno != 0) {\
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s", cmd, ERROR_SPECIFICATION) >= 0);\
    bufcpy (DL0 (scr), ERROR_SPECIFICATION, BUFFER_SIZE);\
    return;\
  }}

#define SKIP_WHITE(w) {\
  while ((w) != NO_TEXT && (w)[0] != NULL_CHAR && IS_SPACE ((w)[0])) {\
    (w)++;\
  }}

#define NO_ARGS(c, z) {\
  if ((z) != NO_TEXT && (z)[0] != NULL_CHAR) {\
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: unexpected argument", c) >= 0);\
    ROW (curs) = CMD_ROW (scr);\
    COL (curs) = MARGIN;\
    return;\
  }}

#define ARGS(c, z) {\
  if ((z) == NO_TEXT || (z)[0] == NULL_CHAR) {\
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: missing argument", c) >= 0);\
    ROW (curs) = CMD_ROW (scr);\
    COL (curs) = MARGIN;\
    return;\
  }}

#define XABEND(p, reason, info) {\
  if (p) {\
    (void) endwin ();\
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
  {-1, -1, NO_TEXT}
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
  {-1, -1, NO_TEXT}
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
  {-1, -1, NO_TEXT}
};

KEY regexp_tab[] = {
  {'d', -1, "[+-]?[0-9]+"},
  {'f', -1, "[+-]?[0-9]*\\.?[0-9]+([eEdDqQ][+-]?[0-9]+)?"},
  {'w', -1, "[A-Za-z_][A-Za-z0-9_]*"},
  {-1, -1, NO_TEXT}
};

typedef struct EDLIN_T EDLIN_T;
struct EDLIN_T {
  int number, reserved;
  char precmd[MARGIN + 1];
  char *text;
  EDLIN_T *next, *previous;
  BOOL_T select, modified;
};

typedef struct CURSOR_T CURSOR_T;
struct CURSOR_T {
  int row, col, row0, col0, index;
  EDLIN_T *line, *last;
  BOOL_T in_forbidden, in_prefix, in_text, in_cmd;
  BOOL_T sync;
  int sync_index;
  EDLIN_T *sync_line;
  unsigned bstate;
};

typedef struct DISPLAY_T DISPLAY_T;
struct DISPLAY_T {
  int scale_row, cmd_row, idf_row;
  EDLIN_T *last_line;
  char status[BUFFER_SIZE];
  char tmp_text[BUFFER_SIZE];
  char cmd[BUFFER_SIZE];
  char dl0[BUFFER_SIZE];
  CURSOR_T curs;
  BOOL_T ins_mode;
};

typedef struct REGEXP_T REGEXP_T;
struct REGEXP_T {
  BOOL_T is_compiled, negate;
  char pattern[BUFFER_SIZE];
  regex_t compiled;
  regmatch_t *match;
  size_t num_match;
};

typedef struct DATASET_T DATASET_T;
struct DATASET_T {
  mode_t perms;
  char name[BUFFER_SIZE];
  char perm[BUFFER_SIZE];
  char date[BUFFER_SIZE];
  char undo[BUFFER_SIZE];
  int size, alts, tabs, num, undo_line, search, m_so, m_eo;
  EDLIN_T *tof; /* top-of-file */
  BOOL_T new_file;
  BOOL_T subset;
  BOOL_T collect;
  DISPLAY_T display;
  EDLIN_T *curr; /* Current line, above the scale */
  EDLIN_T *match; /* Last line to match a regular expression */
  EDLIN_T *bl_start, *bl_end; /* block at last copy or move */
  REGEXP_T targ1, targ2, find, repl;
  char oper; /* regexp operator: & or | */
  FILE_T msgs;
  ADDR_T heap_pointer;
  jmp_buf edit_exit_label;
  char *linbuf;
  int linsiz;
};

/* Forward routines */

static void edit_do_cmd (DATASET_T *);
static void edit_do_prefix (DATASET_T *);
static void edit_loop (DATASET_T *);
static void edit_dataset (DATASET_T *, int, char *, char *);
static void edit_garbage_collect (DATASET_T *, char *);
static void set_current (DATASET_T *, char *, char *);
static void backward_line (EDLIN_T **);
static void forward_line (EDLIN_T **);
static int int_arg (DATASET_T *, char *, char *, char **, int);

/*
\brief set pointers to track history
*/

static void edit_set_history (int ref)
{
  histprev = ref - 1;
  if (histprev < 0) {
    histprev = HISTORY - 1;
    while (histprev > 0 && strlen (history[histprev]) == 0) {
      histprev --;
    }
  }
  histnext = ref + 1;
  if (histprev >= HISTORY - 1 || strlen (history[histnext]) == 0) {
    histnext = 0;
  }
}

/*
\brief store command in a cyclic buffer
\param cmd command to store
*/

static void edit_add_history (char *cmd)
{
  histcurr ++;
  if (histcurr == HISTORY) {
    histcurr = 0;
  }
  bufcpy (history[histcurr], cmd, BUFFER_SIZE); 
  histprev = histnext = histcurr;
}

/*
\brief restore history
*/

void read_history (void)
{
  FILE *f = fopen (A68_HISTORY_FILE, "r");
  if (f != NO_FILE) {
    int k;
    for (k = 0; k < MAX_PF; k++) {
      if (fgets (input_line, BUFFER_SIZE, f) != NO_TEXT) {
        if (input_line[strlen (input_line) - 1] == NEWLINE_CHAR) {
          input_line[strlen (input_line) - 1] = NULL_CHAR;
        }
        if ((int) strlen (input_line) > 0) {
          bufcpy (pf_bind[k], input_line, BUFFER_SIZE);
        }
      }
    }
    histcurr = histnext = histprev = -1;
    while (!feof (f)) {
      if (fgets (input_line, BUFFER_SIZE, f) != NO_TEXT) {
        if (input_line[strlen (input_line) - 1] == NEWLINE_CHAR) {
          input_line[strlen (input_line) - 1] = NULL_CHAR;
        }
        edit_add_history (input_line);
      }
    }
    ASSERT (fclose (f) == 0);
  } else {
/* Laissez-passer */
    histcurr = histnext = histprev = -1;
    RESET_ERRNO;
  }
}

/*
\brief store history
*/

void write_history (void)
{
  FILE *f = fopen (A68_HISTORY_FILE, "w");
  if (f != NO_FILE) {
    int k;
    for (k = 0; k < MAX_PF; k++) {
      fprintf (f, "%s\n", pf_bind[k]);
    }
    for (k = 0; k <= histcurr; k ++) {
      fprintf (f, "%s\n", history[k]);
    }
    ASSERT (fclose (f) == 0);
  } else {
/* Laissez-passer */
    RESET_ERRNO;
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

static BYTE_T *edit_get_heap (DATASET_T *dd, size_t s)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  BYTE_T *z;
  int as = A68_ALIGN ((int) s);
  XABEND (heap_is_fluid == A68_FALSE, ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
/* If there is no space left, we collect garbage */
  if (heap_full (as) && COLLECT (dd)) {
    edit_garbage_collect (dd, "edit");
  }
  if (heap_full (as)) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: out of memory") >= 0);
    return (NO_BYTE);
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

static void add_linbuf (DATASET_T *dd, char ch, int pos)
{
  if (LINBUF (dd) == NO_TEXT || pos >= LINSIZ (dd) - 1) {
    char *oldb = LINBUF (dd);
    LINSIZ (dd) += BUFFER_SIZE;
    LINBUF (dd) = (char *) edit_get_heap (dd, (size_t) (LINSIZ (dd)));
    XABEND (LINBUF (dd) == NO_TEXT, "Insufficient memory", NO_TEXT);
    if (oldb == NO_TEXT) {
      (LINBUF (dd))[0] = NULL_CHAR;
    } else {
      bufcpy (LINBUF (dd), oldb, LINSIZ (dd));
    }
  }
  (LINBUF (dd))[pos] = ch;
  (LINBUF (dd))[pos + 1] = NULL_CHAR;
}

/* REGULAR EXPRESSION SUPPORT */

/*
\brief initialise regular expression
\param re regexp to initialise
**/

static void init_regexp (REGEXP_T *re)
{
  IS_COMPILED (re) = A68_FALSE;
  PATTERN (re)[0] = NULL_CHAR;
  MATCH (re) = NO_REGMATCH;
  NUM_MATCH (re) = 0;
}

/*
\brief reset regular expression
\param re regexp to initialise
**/

static void reset_regexp (REGEXP_T *re)
{
  IS_COMPILED (re) = A68_FALSE;
  PATTERN (re)[0] = NULL_CHAR;
  if (MATCH (re) != NO_REGMATCH) {
    free (MATCH (re));
  }
  MATCH (re) = NO_REGMATCH;
  NUM_MATCH (re) = 0;
}

/*
\brief compile regular expression
\param dd current dataset
\param re regexp to compile
\param cmd command that calls this routine
\return return code
**/

static int compile_regexp (DATASET_T *dd, REGEXP_T *re, char *cmd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  int rc;
  char buffer[BUFFER_SIZE];
  IS_COMPILED (re) = A68_FALSE;
  rc = regcomp (&(COMPILED (re)), PATTERN (re), REG_EXTENDED | REG_NEWLINE);
  if (rc != 0) {
    (void) regerror (rc, &(COMPILED (re)), buffer, BUFFER_SIZE);
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s", cmd, buffer) >= 0);
    regfree (&(COMPILED (re)));
    return (rc);
  } else {
    NUM_MATCH (re) = 1 + RE_NSUB (&COMPILED (re));
    MATCH (re) = malloc ((size_t) (NUM_MATCH (re) * sizeof (regmatch_t)));
    if (MATCH (re) == NO_REGMATCH) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s", cmd, ERROR_OUT_OF_CORE) >= 0);
      regfree (&(COMPILED (re)));
      return (-1);
    }
  }
  IS_COMPILED (re) = A68_TRUE;
  return (0);
}

/*
\brief match line to regular expression
\param dd current dataset
\param fragment whether matching the tail of a string after an earlier match
\param cmd command that calls this routine
\return whether match
**/

static BOOL_T match_regex (DATASET_T *dd, EDLIN_T *z, int eflags, char *cmd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  int rc1 = REG_NOMATCH, rc2 = REG_NOMATCH;
  BOOL_T m1 = A68_FALSE, m2 = A68_FALSE;
  char *str = TEXT (z);
/* Match first regex if present */
  if (IS_COMPILED (&TARG1 (dd))) {
    rc1 = regexec (&(COMPILED (&TARG1 (dd))), str, NUM_MATCH (&TARG1 (dd)), MATCH (&TARG1 (dd)), eflags);
    if (rc1 != 0 && rc1 != REG_NOMATCH) {
      char buffer[BUFFER_SIZE];
      (void) regerror (rc1, &(COMPILED (&TARG1 (dd))), buffer, BUFFER_SIZE);
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s", cmd, buffer) >= 0);
      if (MATCH (&TARG1 (dd)) != NO_REGMATCH) {
        free (MATCH (&TARG1 (dd)));
        MATCH (&TARG1 (dd)) = NO_REGMATCH;
      }
      return (A68_FALSE);
    }
    if (NEGATE (&TARG1 (dd))) {
      m1 = (rc1 == REG_NOMATCH);
    } else {
      m1 = (rc1 != REG_NOMATCH);
    }
  }
/* Match 2nd regex if present */
  if (IS_COMPILED (&TARG2 (dd))) {
    rc2 = regexec (&(COMPILED (&TARG2 (dd))), str, NUM_MATCH (&TARG2 (dd)), MATCH (&TARG2 (dd)), eflags);
    if (rc2 != 0 && rc2 != REG_NOMATCH) {
      char buffer[BUFFER_SIZE];
      (void) regerror (rc2, &(COMPILED (&TARG2 (dd))), buffer, BUFFER_SIZE);
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s", cmd, buffer) >= 0);
      if (MATCH (&TARG2 (dd)) != NO_REGMATCH) {
        free (MATCH (&TARG2 (dd)));
        MATCH (&TARG2 (dd)) = NO_REGMATCH;
      }
      return (A68_FALSE);
    }
    if (NEGATE (&TARG2 (dd))) {
      m2 = (rc2 == REG_NOMATCH);
    } else {
      m2 = (rc2 != REG_NOMATCH);
    }
  }
/* Form a result */
  M_MATCH (dd) = NO_EDLIN;
  M_SO (dd) = M_EO (dd) = -1;
  if (m1 && !NEGATE (&TARG1 (dd))) {
    M_MATCH (dd) = z;
    M_SO (dd) = (int) RM_SO (&(MATCH (&TARG1 (dd))[0]));      
    M_EO (dd) = (int) RM_EO (&(MATCH (&TARG1 (dd))[0]));      
  } else if (m2 && !NEGATE (&TARG2 (dd))) {
    M_MATCH (dd) = z;
    M_SO (dd) = (int) RM_SO (&(MATCH (&TARG2 (dd))[0]));      
    M_EO (dd) = (int) RM_EO (&(MATCH (&TARG2 (dd))[0]));      
  }
  if (OPER (dd) == NULL_CHAR) {
    return (m1);
  } else if (OPER (dd) == '|') {
    return (m1 | m2);
  } else if (OPER (dd) == '&') {
    return (m1 & m2);
  } else if (OPER (dd) == '^') {
    return (m1 ^ m2);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief get regular expression from string
\param re regular expression structure to use
\param str pointer in string to regular expression, set to end upon return
\param delim char to store regular expression delimiter
**/

static void copy_regexp (REGEXP_T *re, char **str, char *delim)
{
  char *pat = PATTERN (re), *q = *str;
  if (q[0] == '~') {
    NEGATE (re) = A68_TRUE;
    q++;
  } else {
    NEGATE (re) = A68_FALSE;
  }
  *delim = q[0];
  q++;
  while (q[0] != *delim && q[0] != NULL_CHAR) {
    if (q[0] == '\\') {
      int k;
      BOOL_T found = A68_FALSE;
      for (k = 0; !found && NAME (&regexp_tab[k]) != NO_TEXT; k ++) {
        if (q[1] == CODE (&regexp_tab[k])) {
          char *r = NAME (&regexp_tab[k]);
          while (*r != NULL_CHAR) {
            *(pat)++ = *(r)++;
          }
          found = A68_TRUE;
          q += 2;
        }
      }
      if (!found) {
        *(pat)++ = *q++;
        *(pat)++ = *q++;
      }
    } else {
      *(pat)++ = *q++;
    }
  }
  pat[0] = NULL_CHAR;
  *str = q;
}

/*!
\brief get regexp and find target with respect to the current line
\param dd current dataset
\param cmd command that calls this routine
\param arg points to arguments
\param rest will point to text after arguments
\param compile must compile or has been compiled
**/

static EDLIN_T * get_regexp (DATASET_T *dd, char *cmd, char *arg, char **rest, BOOL_T compile)
{
/*
Target is of form
	[+|-]/regexp/ or
	[+|-]/regexp/&/regexp/ both must match
	[+|-]/regexp/|/regexp/ one or both must match
	[+|-]/regexp/^/regexp/ one must match, but not both
*/
  DISPLAY_T *scr = &(DISPLAY (dd));
  char *q, delim;
  int rc;
  BOOL_T forward;
  if (compile == A68_FALSE) {
    if (IS_COMPILED (&(TARG1 (dd))) == A68_FALSE || SEARCH (dd) == 0) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
      return (NO_EDLIN);
    }
    if (SEARCH (dd) == 1) {
      forward = A68_TRUE;
    } else {
      forward = A68_FALSE;
    }
  } else {
    if (EMPTY_STRING (arg)) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
      return (NO_EDLIN);
    }
/* Initialise */
    reset_regexp (&(TARG1 (dd)));
    reset_regexp (&(TARG2 (dd)));
    OPER (dd) = NULL_CHAR;
    (*rest) = NO_TEXT;
    SKIP_WHITE (arg);
    q = arg;
    if (q[0] == '+') {
      forward = A68_TRUE;
      q++;
    } else if (q[0] == '-') {
      forward = A68_FALSE;
      SEARCH (dd) = -1;
      q++;
    } else {
      forward = A68_TRUE;
      SEARCH (dd) = 1;
    }
/* Get first regexp */
    copy_regexp (&TARG1 (dd), &q, &delim);
    if ((int) strlen (PATTERN (&TARG1 (dd))) == 0) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
      return (NO_EDLIN);
    }
    rc = compile_regexp (dd, &(TARG1 (dd)), cmd);
    if (rc != 0) {
      return (NO_EDLIN);
    }
/* Get operator and 2nd regexp, if present */
    if (q[0] == delim && (q[1] == '&' || q[1] == '|' || q[1] == '^')) {
      q++;
      OPER (dd) = q[0];
      q++;
      copy_regexp (&TARG2 (dd), &q, &delim);
      if ((int) strlen (PATTERN (&TARG2 (dd))) == 0) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
        return (NO_EDLIN);
      }
      rc = compile_regexp (dd, &(TARG2 (dd)), cmd);
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
    EDLIN_T *u = CURR (dd);
    forward_line (&u);
    if (NOT_EOF (u)) {
      EDLIN_T *z = u;
      for (z = u; NOT_EOF (z); forward_line (&z)) {
        if (match_regex (dd, z, 0, cmd)) {
          return (z);
        }
      }
    }
  } else {
    EDLIN_T *u = CURR (dd);
    backward_line (&u);
    if (NOT_TOF (u)) {
      EDLIN_T *z = u;
      for (z = u; NOT_TOF (z); backward_line (&z)) {
        if (match_regex (dd, z, 0, cmd)) {
          return (z);
        }
      }
    }
  }
  ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: not found", cmd) >= 0)
  return (NO_EDLIN);
}

/*!
\brief get target with respect to the current line
\param dd current dataset
\param cmd command that calls this routine
\param arg points to arguments
\param rest will point to text after arguments
**/

EDLIN_T *get_target (DATASET_T *dd, char *cmd, char *args, char **rest, BOOL_T offset)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  EDLIN_T *z = NO_EDLIN;
  SKIP_WHITE (args);
  if (IS_DIGIT (args[0])) {
/* n	Relative displacement down */
    int n = int_arg (dd, cmd, args, rest, 1);
    if (n == WRONG_TARGET) {
      return (NO_EDLIN);
    } else {
      int k;
      for (z = CURR (dd), k = 0; z != NO_EDLIN && k < n; forward_line (&z), k++) {;}
      if (z == NO_EDLIN) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: target beyond end-of-data", cmd) >= 0);
      }
    }
  } else if (args[0] == '+' && IS_DIGIT (args[1])) {
/* +n	Relative displacement down */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NO_EDLIN);
    } else {
      int k;
      for (z = CURR (dd), k = 0; z != NO_EDLIN && k < n; forward_line (&z), k++) {;}
      if (z == NO_EDLIN) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: target beyond end-of-data", cmd) >= 0);
      }
    }
  } else if (args[0] == ':') {
/*:n	Absolute line number */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NO_EDLIN);
    } else {
      for (z = TOF (dd); z != NO_EDLIN && NUMBER (z) != n; forward_line (&z)) {;}
      if (z == NO_EDLIN) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: target outside selected lines", cmd) >= 0);
      }
    }
  } else if (args[0] == '-' && IS_DIGIT (args[1])) {
/* -n	Relative displacement up */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NO_EDLIN);
    } else {
      int k;
      for (z = CURR (dd), k = 0; z != NO_EDLIN && k < n; backward_line (&z), k++) {;}
      if (z == NO_EDLIN) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: target before top-of-data", cmd) >= 0);
      }
    }
  } else if (args[0] == '*' || args[0] == '$') {
/* *	end-of-data */
    for (z = TOF (dd); NOT_EOF (z) ; forward_line (&z)) {;}
    (*rest) = (&args[1]);
    SKIP_WHITE (*rest);
  } else if (args[0] == '+' && args[1] == '*') {
/* *	end-of-data */
    for (z = TOF (dd); NOT_EOF (z) ; forward_line (&z)) {;}
    (*rest) = (&args[2]);
    SKIP_WHITE (*rest);
  } else if (args[0] == '-' && args[1] == '*') {
/* *	top-of-data */
    for (z = CURR (dd); NOT_TOF (z); backward_line (&z)) {;}
    (*rest) = (&args[2]);
    SKIP_WHITE (*rest);
  } else if (args[0] == '.') {
/* .IDF	Prefix identifier */
    EDLIN_T *u;
    char idf[8] = {'.', NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR, NULL_CHAR};
    int k;
    for (k = 1; IS_ALNUM (args[k]) && k < MARGIN - 1; k++) {
      idf[k] = args[k];
    }
    (*rest) = (&args[k]);
    SKIP_WHITE (*rest);
/* Scan all file to check multiple definitions */
    for (u = TOF (dd), z = NO_EDLIN; u != NO_EDLIN; forward_line (&u)) {
      char *v = PRECMD (u);
      SKIP_WHITE (v);
      if (strncmp (v, idf, (size_t) (k - 1)) == 0) {
        if (z != NO_EDLIN) {
          ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: multiple targets %s", cmd, idf) >= 0);
          return (NO_EDLIN);
        } else {
          z = u;
        }
      }
    }
    if (z == NO_EDLIN) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no target %s", cmd, idf) >= 0);
    }
  } else if (args[0] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else if (args[0] == '~' && args[1] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else if (args[0] == '-' && args[1] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else if (args[0] == '+' && args[1] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else if (args[0] == '-' && args[1] == '~' && args[2] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else if (args[0] == '+' && args[1] == '~' && args[2] == '/') {
    z = get_regexp (dd, cmd, args, rest, A68_TRUE);
  } else {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: unrecognised target syntax", cmd) >= 0);
    return (NO_EDLIN);
  }
/* A target can have an offset +-m */
  if (!offset) {
    return (z);
  }
  args = *rest;
  if (args != NO_TEXT && args[0] == '+' && IS_DIGIT (args[1])) {
/* +n	Relative displacement down */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NO_EDLIN);
    } else {
      int k;
      for (k = 0; z != NO_EDLIN && k < n; forward_line (&z), k++) {;}
      if (z == NO_EDLIN) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: target beyond end-of-data", cmd) >= 0);
      }
    }
  } else if (args != NO_TEXT && args[0] == '-' && IS_DIGIT (args[1])) {
/* -n	Relative displacement up */
    int n = int_arg (dd, cmd, &args[1], rest, 1);
    if (n == WRONG_TARGET) {
      return (NO_EDLIN);
    } else {
      int k;
      for (k = 0; z != NO_EDLIN && k < n; backward_line (&z), k++) {;}
      if (z == NO_EDLIN) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: target before top-of-data", cmd) >= 0);
      }
    }
  }
  return (z);
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

static BOOL_T reserved_row (DATASET_T *dd, int row)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  return (row == CMD_ROW (scr) || 
          row == SCALE_ROW (scr) ||
          row == IDF_ROW (scr));
}

/*
\brief count reserved lines on screen
\param dd current dataset
\return same
**/

static int count_reserved (DATASET_T *dd)
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

static int lines_on_scr (DATASET_T *dd, EDLIN_T * lin)
{
  int k = 0, row = 1, col = MARGIN;
  char *txt = TEXT (lin);
  while (txt[k] != NULL_CHAR) {
    int reps, n;
    if (txt[k] == '\t') {
      reps = tab_reps (col - MARGIN, TABS (dd));
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

static void edit_init_curses (DATASET_T *dd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  (void) initscr ();
  (void) raw ();
  EDIT_TEST (keypad (stdscr, TRUE) != ERR);
  EDIT_TEST (noecho () != ERR);
  EDIT_TEST (nonl () != ERR);
  EDIT_TEST (meta (stdscr, TRUE) != ERR);
#if ! defined HAVE_WIN32
  (void) mousemask ((mmask_t) ALL_MOUSE_EVENTS, NULL);
#endif /* ! defined HAVE_WIN32 */
  EDIT_TEST (curs_set (1) != ERR);
  SCALE_ROW (scr) = LINES / 2;
  CMD_ROW (scr) = 1;
  IDF_ROW (scr) = 0;
  ROW (curs) = COL (curs) = -1;
  SYNC (curs) = A68_FALSE;
  REDRAW (stdscr);
  EDIT_TEST (wrefresh (stdscr) != ERR);
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

EDLIN_T *new_line (DATASET_T *dd)
{
  EDLIN_T *newl = (EDLIN_T *) edit_get_heap (dd, (size_t) sizeof (EDLIN_T));
  if (newl == NO_EDLIN) {
    return (NO_EDLIN);
  }
  PRECMD (newl)[0] = NULL_CHAR;
  SELECT (newl) = A68_TRUE;
  NEXT (newl) = NO_EDLIN;
  PREVIOUS (newl) = NO_EDLIN;
  TEXT (newl) = NO_TEXT;
  NUMBER (newl) = 0;
  MODIFIED (newl) = A68_FALSE;
  return (newl);
}

/*
\brief mark line as altered
\param dd current dataset
\return new line
**/

static void alt_line (DATASET_T *dd, EDLIN_T *z)
{
  if (!MODIFIED (z)) {
    MODIFIED (z) = A68_TRUE;
    ALTS (dd)++;
  }
  M_MATCH (dd) = NO_EDLIN;
  M_SO (dd) = M_EO (dd) = -1;
}

/*
\brief forward line, folded or not
\param z line to forward
**/

static void forward_line (EDLIN_T **z)
{
  if (*z == NO_EDLIN) {
    return;
  }
  do {
    FORWARD (*z);
  } while (*z != NO_EDLIN && ! (SELECT (*z) || NUMBER (*z) == 0));
}

/*
\brief backward line, folded or not
\param z line to "backward"
**/

static void backward_line (EDLIN_T **z)
{
  if (*z == NO_EDLIN) {
    return;
  }
  do {
    BACKWARD (*z);
  } while (*z != NO_EDLIN && ! (SELECT (*z) || NUMBER (*z) == 0));
}

/*
\brief align current line, cannot be TOF or EOF
\param dd current dataset
**/

static void align_current (DATASET_T *dd)
{
  if (IS_TOF (CURR (dd))) {
    EDLIN_T *z = CURR (dd);
    forward_line (&z);
    if (NOT_EOF (z)) {
      CURR (dd) = z;
    }
  } else if (IS_EOF (CURR (dd))) {
    if (IS_TOF (PREVIOUS (CURR (dd)))) {
      CURR (dd) = TOF (dd);
    } else {
      EDLIN_T *z = CURR (dd);
      backward_line (&z);
      CURR (dd) = z;
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

static void new_edit_string (DATASET_T *dd, EDLIN_T *l, char *txt, EDLIN_T *eat)
{
  if (txt == NO_TEXT || strlen(txt) == 0) {
    RESERVED (l) = 1;
    TEXT (l) = (char *) edit_get_heap (dd, (size_t) RESERVED (l));
    TEXT (l)[0] = NULL_CHAR;
    bufcpy (PRECMD (l), BLANK, (int) (strlen (BLANK) + 1));
  } else {
    int res = 1 + (int) strlen (txt);
    if (res % BLOCK_SIZE > 0) {
      res += (BLOCK_SIZE - res % BLOCK_SIZE);
    }
    if (eat != NO_EDLIN && RESERVED (eat) >= res) {
      RESERVED (l) = RESERVED (eat);
      TEXT (l) = TEXT (eat);
    } else {
      RESERVED (l) = res;
      TEXT (l) = (char *) edit_get_heap (dd, (size_t) res);
    }
    if (TEXT (l) == NO_TEXT) {
      return;
    }
    bufcpy (TEXT (l), txt, res);
    bufcpy (PRECMD (l), BLANK, (int) (strlen (BLANK) + 1));
  }
}

/*
\brief set prefix to line
\param l line to set
**/

static void set_prefix (EDLIN_T *l)
{
  bufcpy (PRECMD (l), BLANK, (int) (strlen (BLANK) + 1));
}

/*
\brief reset prefixes to original state
\param dd current dataset
**/

static void edit_reset (DATASET_T *dd)
{
  int k = 0;
  EDLIN_T *z;
  for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
    if (NUMBER (z) != 0) {
      k++;
      NUMBER (z) = k;
    }
    set_prefix (z);
  }
  SIZE (dd) = k;
}

/*
\brief delete to end of line
\param dd current dataset
**/

static void cdelete (DATASET_T *dd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  EDLIN_T *lin = LINE (curs);
  if (lin != NO_EDLIN && INDEX (curs) < (int) strlen (TEXT (lin))) {
    TEXT (lin)[INDEX (curs)] = NULL_CHAR;
  }
}

/*
\brief split line at cursor position
\param dd current dataset
\param cmd command that calls this routine
**/

static void split_line (DATASET_T *dd, char *cmd)
{
/* We reset later so this routine can be repeated cheaply */
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  EDLIN_T *lin = LINE (curs), *newl;
  if (NEXT (lin) == NO_EDLIN) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot split line", cmd) >= 0);
    return;
  }
  BL_START (dd) = BL_END (dd) = NO_EDLIN;
  ALTS (dd)++;
  SIZE (dd)++;
/* Insert a new line */
  newl = new_line (dd);
  if (newl == NO_EDLIN) {
    return;
  }
  if ((INDEX (curs) < (int) strlen (TEXT (lin))) && IS_IN_TEXT (lin)) {
    new_edit_string (dd, newl, &(TEXT (lin)[INDEX (curs)]), NO_EDLIN);
    if (TEXT (newl) == NO_TEXT) {
      return;
    }
    TEXT (lin)[INDEX (curs)] = NULL_CHAR;
  } else {
    new_edit_string (dd, newl, "", NO_EDLIN);
    if (TEXT (newl) == NO_TEXT) {
      return;
    }
  }
  PREVIOUS (newl) = lin;
  NEXT (newl) = NEXT (lin);
  NEXT (lin) = newl;
  PREVIOUS (NEXT (newl)) = newl;
  NUMBER (newl) = NUMBER (lin) + 1;
/* Position the cursor at the start of the new line */
  SYNC_INDEX (curs) = 0;
  SYNC_LINE (curs) = newl;
  SYNC (curs) = A68_TRUE;
  if (lin == LAST_LINE (scr)) {
    forward_line (&CURR (dd));
  }
}

/*
\brief join line with next one
\param dd current dataset
\param cmd command that calls this
**/

static void join_line (DATASET_T *dd, char *cmd)
{
/* We reset later so this routine can be repeated cheaply */
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  EDLIN_T *lin = LINE (curs), *prv, *u;
  int len, lcur, lprv;
  if (NUMBER (lin) == 0) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot join line", cmd) >= 0);
    return;
  }
  BL_START (dd) = BL_END (dd) = NO_EDLIN;
  prv = PREVIOUS (lin);
  ALTS (dd)++;
  SIZE (dd)--;
/* Join line with the previous one */
  if (prv == TOF (dd)) {
/* Express case */
    NEXT (TOF (dd)) = NEXT (lin);
    PREVIOUS (NEXT (prv)) = prv;
    SYNC_INDEX (curs) = 0;
    SYNC_LINE (curs) = TOF (dd);
    SYNC (curs) = A68_TRUE;
    return;
  }
  lcur = (int) strlen (TEXT (lin));
  lprv = (int) strlen (TEXT (prv));
  len = lcur + lprv;
  if (RESERVED (prv) <= len + 1) {
/* Not enough room */
    int res = len + 1;
    char *txt = TEXT (prv);
    if (res % BLOCK_SIZE > 0) {
      res += (BLOCK_SIZE - res % BLOCK_SIZE);
    }
    RESERVED (prv) = res;
    TEXT (prv) = (char *) edit_get_heap (dd, (size_t) res);
    if (TEXT (prv) == NO_TEXT) {
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
  if (u == NO_EDLIN) {
    u = TOF (dd);
  }
  if (CURR (dd) == lin) {
    CURR (dd) = u;
  }
  SYNC_LINE (curs) = u;
  if (IS_IN_TEXT (u)) {
    if (u == prv) {
      SYNC_INDEX (curs) = lprv;
    } else {
      SYNC_INDEX (curs) = (int) strlen (TEXT (u));
    }
  } else {
    SYNC_INDEX (curs) = 0;
  }
  SYNC (curs) = A68_TRUE;
}

/*
\brief read a string from file
\param dd current dataset
\param cmd command that calls this routine
**/

static void edit_read_string (DATASET_T *dd, FILE_T fd)
{
  int bytes, posl;
  char ch;
/* Set up for reading */
  (LINBUF (dd))[0] = NULL_CHAR;
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
      if (LINBUF (dd) == NO_TEXT) {
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

static void edit_read (DATASET_T * dd, char *cmd, char *fname, EDLIN_T *eat)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  FILE_T fd;
  int bytes, posl;
  char ch;
  EDLIN_T *curr = CURR (dd);
/* Open the file */
  RESET_ERRNO;
  if ((int) strlen (fname) > 0) {
    fd = open (fname, A68_READ_ACCESS);
  } else {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot open file for reading", cmd) >= 0);
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
      LINE (curs) = curr;
      INDEX (curs) = (int) strlen (TEXT (curr));
      split_line (dd, cmd);
      FORWARD (curr);
      if (eat != NO_EDLIN) {
        new_edit_string (dd, curr, LINBUF (dd), eat);
        FORWARD (eat);
      } else {
        new_edit_string (dd, curr, LINBUF (dd), curr);
      }
      if (TEXT (curr) == NO_TEXT) {
        ASSERT (close (fd) == 0);
        return;
      }
/* Reinit line buffer */
      posl = 0;
      if (LINBUF (dd) != NO_TEXT) {
        (LINBUF (dd))[0] = NULL_CHAR;
      }
    } else {
      add_linbuf (dd, ch, posl);
      if (LINBUF (dd) == NO_TEXT) {
        ASSERT (close (fd) == 0);
        return;
      }
      posl++;
    }
    bytes = io_read (fd, &ch, 1);
  }
  end:
  ASSERT (close (fd) == 0);
  edit_reset (dd);
  BL_START (dd) = BL_END (dd) = NO_EDLIN;
  align_current (dd);
}

/*
\brief first read of a file
\param dd current dataset
\param cmd command that calls this routine
**/

static void edit_read_initial (DATASET_T * dd, char *cmd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  FILE_T fd;
  EDLIN_T *curr;
  struct stat statbuf;
/* Initialisations */
  CMD (scr)[0] = NULL_CHAR;
  init_regexp (&(TARG1 (dd)));
  init_regexp (&(TARG2 (dd)));
  init_regexp (&(FIND (dd)));
  init_regexp (&(REPL (dd)));
  SUBSET (dd) = A68_FALSE;
  ALTS (dd) = 0;
  INDEX (curs) = 0;
/* Add TOF */
  TOF (dd) = new_line (dd);
  if (TOF (dd) == NO_EDLIN) {
    return;
  }
  new_edit_string (dd, TOF (dd), TOPSTR, NO_EDLIN);
  if (TEXT (TOF (dd)) == NO_TEXT) {
    return;
  }
  NUMBER (TOF (dd)) = 0;
  set_prefix (TOF (dd));
/* Add EOF */
  curr = new_line (dd);
  if (curr == NO_EDLIN) {
    return;
  }
  new_edit_string (dd, curr, BOTSTR, NO_EDLIN);
  if (TEXT (curr) == NO_TEXT) {
    return;
  }
  NUMBER (curr) = 0;
  set_prefix (curr);
  PREVIOUS (curr) = TOF (dd);
  NEXT (TOF (dd)) = curr;
  CURR (dd) = TOF (dd);
/* Open the file */
  RESET_ERRNO;
  if ((int) strlen (NAME (dd)) > 0) {
    fd = open (NAME (dd), A68_READ_ACCESS);
  } else {
    fd = -1;
  }
  if (fd == -1) {
    char datestr[BUFFER_SIZE];
    time_t rt;
    struct tm *tm;
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: creating new file", cmd) >= 0);
    SIZE (dd) = 0;
    PERMS (dd) = A68_PROTECTION;
    NEW_FILE (dd) = A68_TRUE;
    ASSERT (time (&rt) != (time_t) (-1));
    ASSERT ((tm = localtime (&rt)) != NULL);
    ASSERT ((strftime (datestr, BUFFER_SIZE, DATE_STRING, tm)) > 0);
    ASSERT (snprintf (DATE (dd), SNPRINTF_SIZE, "%s", datestr) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  CHECK_ERRNO (cmd);
/* Collect file information; we display file date and permissions */
  NEW_FILE (dd) = A68_FALSE;
  if (stat (NAME (dd), &statbuf) != -1) {
    struct tm *tm;
    char datestr[BUFFER_SIZE];
    PERMS (dd) = ST_MODE (&statbuf);
    ASSERT ((tm = localtime (&ST_MTIME (&statbuf))) != NULL);
    ASSERT ((strftime (datestr, BUFFER_SIZE, DATE_STRING, tm)) > 0);
    ASSERT (snprintf (DATE (dd), SNPRINTF_SIZE, "%s", datestr) >= 0);
  }
/* Set up for reading */
  edit_read (dd, cmd, NAME (dd), NO_EDLIN);
  ALTS (dd) = 0; /* Again, since edit_read inserts lines! */
  CURR (dd) = NEXT (TOF (dd));
}

/*
\brief write dataset to file
\param dd current dataset
\param cmd command that calls this routine
\param fname file name
\param u first line to write
\param v write upto, but not including, this line
**/

static void edit_write (DATASET_T * dd, char *cmd, char *fname, EDLIN_T *u, EDLIN_T *v)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  FILE_T fd;
  EDLIN_T *curr;
/* Backwards range */
  if (NOT_EOF (v) && (NUMBER (v) < NUMBER (u))) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: backward range", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
/* Open the file */
  RESET_ERRNO;
  fd = open (fname, A68_WRITE_ACCESS, A68_PROTECTION);
  CHECK_ERRNO (cmd);
  if (fd == -1) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot open file for writing", cmd) >= 0);
    return;
  }
  for (curr = u; curr != v; FORWARD (curr)) {
    if (IS_IN_TEXT (curr)) {
      if ((int) strlen (TEXT (curr)) > 0) {
        WRITE (fd, TEXT (curr));
      }
      if (NEXT (curr) != NO_EDLIN) {
        WRITE (fd, "\n");
      }
    }
  }
  RESET_ERRNO;
  ASSERT (close (fd) == 0);
  CHECK_ERRNO (cmd);
}

/*
\brief write a file for recovery
\param dd current dataset
\param cmd command that calls this routine
**/

static void edit_write_undo_file (DATASET_T * dd, char *cmd)
{
  if ((UNDO (dd))[0] == NULL_CHAR) {
    return;
  }
  edit_write (dd, cmd, UNDO (dd), TOF (dd), NO_EDLIN);
  UNDO_LINE (dd) = NUMBER (CURR (dd));
}

/*
\brief read a file for recovery
\param dd current dataset
\param cmd command that calls this routine
**/

static void edit_read_undo_file (DATASET_T *dd, char *cmd)
{
  FILE_T fd;
  struct stat statbuf;
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  if ((UNDO (dd))[0] == NULL_CHAR) {
    return;
  }
  RESET_ERRNO;
  fd = open (UNDO (dd), A68_READ_ACCESS);
  if (fd == -1 || errno != 0) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot recover", cmd) >= 0);
    return;
  } else {
    EDLIN_T *eat = NO_EDLIN, *curr;
    if (TOF (dd) != NO_EDLIN) {
      eat = NEXT (TOF (dd));
    }
    ASSERT (close (fd) == 0);
    SUBSET (dd) = A68_FALSE;
    INDEX (curs) = 0;
    TOF (dd) = new_line (dd);
    if (TOF (dd) == NO_EDLIN) {
      return;
    }
    new_edit_string (dd, TOF (dd), TOPSTR, NO_EDLIN);
    if (TEXT (TOF (dd)) == NO_TEXT) {
      return;
    }
    NUMBER (TOF (dd)) = 0;
    set_prefix (TOF (dd));
    curr = new_line (dd);
    if (curr == NO_EDLIN) {
      return;
    }
    new_edit_string (dd, curr, BOTSTR, NO_EDLIN);
    if (TEXT (curr) == NO_TEXT) {
      return;
    }
    NUMBER (curr) = 0;
    set_prefix (curr);
    PREVIOUS (curr) = TOF (dd);
    NEXT (TOF (dd)) = curr;
    CURR (dd) = TOF (dd);
    edit_read (dd, cmd, UNDO (dd), eat);
    if (stat (UNDO (dd), &statbuf) != -1) {
      struct tm *tm;
      char datestr[BUFFER_SIZE];
      PERMS (dd) = ST_MODE (&statbuf);
      ASSERT ((tm = localtime (&ST_MTIME (&statbuf))) != NULL);
      ASSERT ((strftime (datestr, BUFFER_SIZE, DATE_STRING, tm)) > 0);
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s restored to state at %s", cmd, NAME (dd), datestr) >= 0);
    }
    if (remove (UNDO (dd)) != 0) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s", cmd, ERROR_FILE_SCRATCH) >= 0);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
    } else {
      char cmd2[BUFFER_SIZE];
      ASSERT (snprintf (cmd2, SNPRINTF_SIZE, ":%d", UNDO_LINE (dd)) >= 0);
      set_current (dd, cmd, cmd2);
      align_current (dd);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
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

static void edit_garbage_collect (DATASET_T *dd, char *cmd)
{
  RESET_ERRNO;
  edit_write_undo_file (dd, cmd);
  if (errno != 0) {
    return;
  }
  fixed_heap_pointer = HEAP_POINTER (dd);
  TOF (dd) = NO_EDLIN;
  LINBUF (dd) = NO_TEXT;
  LINSIZ (dd) = 0;
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

static void edit_putch (int row, int col, char ch, DATASET_T *dd, EDLIN_T *dd_line, int dd_index)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  BOOL_T forbidden = reserved_row (dd, row);
  BOOL_T text_area = (!forbidden) && col >= MARGIN;
  BOOL_T prefix_area = (!forbidden) && col < MARGIN;
  int rc;
  if (row < 0 || row >= LINES) {
    return;
  }
  if (IS_CNTRL (ch)) {
    ch = '*';
  }
  if (col < 0 || col >= COLS) {
    return;
  }
  if (row == CMD_ROW (scr) && ROW (curs) == row) {
    if (COL (curs) < MARGIN) {
      IN_FORBIDDEN (curs) = A68_TRUE;
      IN_TEXT (curs) = A68_FALSE;
      IN_PREFIX (curs) = A68_FALSE;
      IN_CMD (curs) = A68_FALSE;
    } else {
      IN_CMD (curs) = A68_TRUE;
      IN_TEXT (curs) = A68_FALSE;
      IN_PREFIX (curs) = A68_FALSE;
      if (COL (curs) == col) {
        INDEX (curs) = dd_index;
      }
    }
    LINE (curs) = NO_EDLIN;
  } else if (forbidden && ROW (curs) == row) {
    IN_FORBIDDEN (curs) = A68_TRUE;
    LINE (curs) = NO_EDLIN;
  } else if (text_area && SYNC (curs) && SYNC_LINE (curs) == dd_line && SYNC_INDEX (curs) == dd_index) {
    ROW (curs) = row;
    COL (curs) = col;
    SYNC (curs) = A68_FALSE;
    IN_TEXT (curs) = A68_TRUE;
    IN_PREFIX (curs) = A68_FALSE;
    IN_CMD (curs) = A68_FALSE;
    INDEX (curs) = dd_index;
    LINE (curs) = dd_line;
  } else if (text_area && ROW (curs) == row && COL (curs) == col) {
    IN_TEXT (curs) = A68_TRUE;
    IN_PREFIX (curs) = A68_FALSE;
    IN_CMD (curs) = A68_FALSE;
    INDEX (curs) = dd_index;
    if (dd_line != NO_EDLIN) {
      LINE (curs) = dd_line;
    }
  } else if (prefix_area && ROW (curs) == row && COL (curs) == col) {
    IN_TEXT (curs) = A68_FALSE;
    IN_CMD (curs) = A68_FALSE;
    IN_PREFIX (curs) = A68_TRUE;
    INDEX (curs) = dd_index;
    if (dd_line != NO_EDLIN) {
      LINE (curs) = dd_line;
    }
  }
  EDIT_TEST (wmove (stdscr, row, col) != ERR);
  rc = waddch (stdscr, (chtype) ch);
  EDIT_TEST (rc != ERR || (row == (LINES - 1) && col == (COLS - 1)));
  EDIT_TEST (wmove (stdscr, row, col) != ERR);
}

/*
\brief draw the screen
\param dd current dataset
**/

static void edit_draw (DATASET_T *dd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  EDLIN_T *run, *tos, *lin = NO_EDLIN, *z;
  int row, k, virt_scal, lin_abo, lin_dif;
  char *prompt = PROMPT;
/* Initialisations */
  if (LINE (curs) != NO_EDLIN) {
    LAST (curs) = LINE (curs);
  }
  LINE (curs) = NO_EDLIN;
  IN_FORBIDDEN (curs) = IN_PREFIX (curs) = IN_TEXT (curs) = IN_CMD (curs) = A68_FALSE;
/* We locate the top-of-screen with respect to the current line */
  if (SCALE_ROW (scr) > 0 && SCALE_ROW (scr) < LINES) {
    virt_scal = SCALE_ROW (scr);
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
  for (z = CURR (dd); z != NO_EDLIN && lin_abo < virt_scal; ) {
    if (z == CURR (dd)) {
      lin_abo ++;
    } else {
      lin_abo += lines_on_scr (dd, z);
    }
    if (lin_abo < virt_scal) {
      backward_line (&z);
    }
  }
  if (z == NO_EDLIN) {
    run = TOF (dd);
  } else {
    run = z;
  }
  tos = run;
  lin_dif = virt_scal - lin_abo;
/* We now raster the screen  - first reserved rows */
  for (row = 0; row < LINES; ) {
/* COMMAND ROW - ====> Forward */
    if (row == CMD_ROW (scr)) {
      int col = 0, ind = 0;
      for (ind = 0; ind < MARGIN; ind++) {
        edit_putch (row, col, prompt[ind], dd, NO_EDLIN, 0);
        col++;
      }
/* Set initial cursor position at start up */
      if (ROW (curs) == -1) {
        ROW (curs) = row;
        COL (curs) = col;
      }
/* Show command */
      for (ind = 0; ind < TEXT_WIDTH && IS_PRINT (CMD (scr)[ind]); ind++) {
        edit_putch (row, col, CMD (scr)[ind], dd, NO_EDLIN, ind);
        col++;
      }
      for (ind = col; ind < COLS; col++, ind++) {
        edit_putch (row, col, BLANK_CHAR, dd, NO_EDLIN, ind - MARGIN);
      }
      row++;
    } else {
      row++;
    }
  }
/* Draw text lines */
  for (row = 0; row < LINES; ) {
    if (reserved_row (dd, row)) {
      row++; 
    } else {
/* Raster a text line */
      BOOL_T cont;
      LAST_LINE (scr) = run;
      if (run == NO_EDLIN) {
/* Draw blank line to balance the screen */
        int col;
        for (col = 0; col < COLS; col++) {
          edit_putch (row, col, BLANK_CHAR, dd, NO_EDLIN, col);
        }
        row++;
      } else if (lin_dif > 0) {
/* Draw blank line to balance the screen */
        int col;
        for (col = 0; col < COLS; col++) {
          edit_putch (row, col, BLANK_CHAR, dd, NO_EDLIN, col);
        }
        lin_dif--;
        row++;
      } else if (TEXT (run) == NO_TEXT) {
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
        if (ROW (curs) == row) {
          lin = run;
        }
        if (pn == 0) {
          bufcpy (prefix, PREFIX, (strlen (PREFIX) + 1));
        } else {
/* Next is a cheap print int */
          int pk;
          for (pk = MARGIN - 2; pk >= 0; pk--) {
            prefix[pk] = pdigits[pn % 10];
            pn /= 10;
          }
          for (pk = 0; pk < MARGIN - 2 && prefix[pk] == '0'; pk ++) {
            /* prefix[pk] = BLANK_CHAR; */
          }
        }
        for (ind = 0; ind < MARGIN; ind++) {
          char chc = PRECMD (run)[ind], chp = prefix[ind];
          if (chc == BLANK_CHAR) {
            edit_putch (row, col, chp, dd, run, ind);
          } else {
            edit_putch (row, col, chc, dd, run, ind);
          }
          col++;
        }
/* Draw text */
        ind = 0;
        cont = A68_TRUE;
        while (cont) {
          int reps, n;
          char ch;
          if (txt[ind] == '\t') {
            ch = BLANK_CHAR;
            reps = tab_reps (col - MARGIN, TABS (dd));
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
              if (ROW (curs) == row) {
                lin = run;
              }
/* Write a continuation number in the prefix area */
              conts++;
              connum[MARGIN - 1] = BLANK_CHAR;
              for (num = conts, k = MARGIN - 2; k >= 0; k--) {
                connum[k] = digits[num % 10];
                num = num / 10;
              }
              for (k = 0; k < MARGIN - 2 && connum[k] == '0'; k ++) {
                connum[k] = ' ';
              }
              connum[0] = '+';
              col = 0;
              for (k = 0; k < MARGIN; k++) {
                edit_putch (row, col, connum[k], dd, run, k);
                col++;
              }
            }
/* Put the character */
            if (!IS_PRINT (ch)) {
              char nch = (char) TO_UCHAR ((int) 0x40 + (int) ch);
              (void) wattron (stdscr, A_REVERSE);
              if (IS_PRINT (nch)) {
                edit_putch (row, col, nch, dd, run, ind); col++;
              } else {
                edit_putch (row, col, '*', dd, run, ind); col++;
              }
              (void) wattroff (stdscr, A_REVERSE);
            } else if (IS_TOF (run) || IS_EOF (run)) {
              edit_putch (row, col, ch, dd, run, ind); col++;
            } else if (run == CURR (dd)) {
              if (run == M_MATCH (dd)) {
                if (ind == M_SO (dd)) {
                  ROW (curs) = row;
                  COL (curs) = col;
                }
                if (ind > M_SO (dd) && ind < M_EO (dd)) {
                  (void) wattron (stdscr, A_REVERSE);
                  edit_putch (row, col, ch, dd, run, ind); col++;
                  (void) wattroff (stdscr, A_REVERSE);
                } else {
                  edit_putch (row, col, ch, dd, run, ind); col++;
                }
              } else {
                edit_putch (row, col, ch, dd, run, ind); col++;
              }
            } else {
              edit_putch (row, col, ch, dd, run, ind); col++;
            }
          }
          ind++;
        }
/* Fill the line */
        text_end:
        for (k = col; k < COLS; k++, col++, ind++) {
          edit_putch (row, col, BLANK_CHAR, dd, run, ind);
        }
        forward_line (&run);
        row++;
      }
    }
  }
/* Write the scale row now all data is complete */
  for (row = 0; row < LINES; ) {
    if (row == SCALE_ROW (scr)) {
/* SCALE ROW - ----+----1----+----2 */
      int col = 0, ind = 0;
      for (ind = 0; ind < MARGIN - 1; ind++) {
        edit_putch (row, col, BLANK_CHAR, dd, NO_EDLIN, 0);
        col++;
      }
      edit_putch (row, col, BLANK_CHAR, dd, NO_EDLIN, ind);
      col++;
/* Scale */
      for (ind = 0; ind < TEXT_WIDTH; ind++) {
        k = ind + 1;
        if (k % 10 == 0) {
          char *digits = "0123456789";
          edit_putch (row, col, digits[(k % 100) / 10], dd, NO_EDLIN, 0);
        } else if (k % 5 == 0) {
          edit_putch (row, col, '+', dd, NO_EDLIN, 0);
        } else {
          edit_putch (row, col, '-', dd, NO_EDLIN, 0);
        }
        col++;
      }
      row++;
    } else if (row == IDF_ROW (scr)) {
/* IDF ROW - Identification, unless there is important stuff to show */
      if (strcmp (DL0 (scr), "help") == 0) {
        int col = 0, ind = 0, pfk, space = (COLS - MARGIN - 4 * 8) / 8;
        char pft[BUFFER_SIZE];
        if (space < 4) {
          space = 4; /* Tant pis */
        }
        for (ind = 0; ind < MARGIN - 1; ind++) {
          edit_putch (row, col, '-', dd, NO_EDLIN, 0);
          col++;
        }
        edit_putch (row, col, BLANK_CHAR, dd, NO_EDLIN, ind);
        col++;
        for (pfk = 0; pfk < 8; pfk++) {
          ASSERT (snprintf (pft, SNPRINTF_SIZE, "f%1d=%-*s ", pfk + 1, space, pf_bind[pfk]) >= 0);
          for (k = 0; k < (int) strlen (pft); col++, k++) {
            edit_putch (row, col, pft[k], dd, NO_EDLIN, k - MARGIN);
          }
        }
        for (ind = col; ind < COLS; col++, ind++) {
          edit_putch (row, col, BLANK_CHAR, dd, NO_EDLIN, ind);
        }
        row++;
      } else {
        int col = 0, ind = 0;
        if (strlen (DL0 (scr)) == 0) {
/* Write file identification line */ 
          ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "\"%s\" %s #%d", 
            NAME (dd), DATE (dd), NUM (dd)) >= 0);
          for (ind = 0; ind < COLS && IS_PRINT (DL0 (scr)[ind]); ind++) {
            edit_putch (row, col, DL0 (scr)[ind], dd, NO_EDLIN, 0);
            col++;
          }
          if ((IN_TEXT (curs) || IN_PREFIX (curs)) && lin != NO_EDLIN && NUMBER (lin) > 0) {
            ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, " %6d/%-6d", 
              NUMBER (lin) % 1000000, SIZE (dd)) >= 0);
          } else {
            ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "              ") >= 0);
          }
          for (ind = 0; ind < COLS && IS_PRINT (DL0 (scr)[ind]); ind++) {
            edit_putch (row, col, DL0 (scr)[ind], dd, NO_EDLIN, 0);
            col++;
          }
          if (IN_CMD (curs) || lin != NO_EDLIN) {
            ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, " col=%-4d", 
              (INDEX (curs) + 1) % 10000) >= 0);
          } else {
            ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "         ") >= 0);
          }
          for (ind = 0; ind < COLS && IS_PRINT (DL0 (scr)[ind]); ind++) {
            edit_putch (row, col, DL0 (scr)[ind], dd, NO_EDLIN, 0);
            col++;
          }
          ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, " alt=%-4d", 
            ALTS (dd) % 10000) >= 0);
          for (ind = 0; ind < COLS && IS_PRINT (DL0 (scr)[ind]); ind++) {
            edit_putch (row, col, DL0 (scr)[ind], dd, NO_EDLIN, 0);
            col++;
          }
          if (INS_MODE (scr)) {
            ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, " ins") >= 0);
          } else {
            ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, " ovr") >= 0);
          }
          for (ind = 0; ind < COLS && IS_PRINT (DL0 (scr)[ind]); ind++) {
            edit_putch (row, col, DL0 (scr)[ind], dd, NO_EDLIN, 0);
            col++;
          }
        } else {
/* Write message in stead of identification */ 
          (void) wattron (stdscr, A_REVERSE);
          for (ind = 0; ind < COLS && IS_PRINT (DL0 (scr)[ind]); ind++) {
            edit_putch (row, col, DL0 (scr)[ind], dd, NO_EDLIN, 0);
            col++;
          }
          (void) wattroff (stdscr, A_REVERSE);
        }
        for (ind = col; ind < COLS; col++, ind++) {
          edit_putch (row, col, BLANK_CHAR, dd, NO_EDLIN, ind);
        }
        row++;
      }
    } else {
      row++;
    }
  }
  M_MATCH (dd) = NO_EDLIN;
  M_SO (dd) = M_EO (dd) = -1;
  EDIT_TEST (wrefresh (stdscr) != ERR);
}

/* Routines to edit various parts of the screen */

/*
\brief edit prefix
\param dd current dataset
\param ch typed character
**/

static void edit_prefix (DATASET_T *dd, int ch)
{
/*
Prefix editing is very basic. You type in overwrite mode.
DEL erases the character under the cursor.
BACKSPACE erases the character left of the cursor.
*/
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  EDLIN_T *lin = LINE (curs);
  if (lin == NO_EDLIN) {
    return;
  }
  if (ch <= UCHAR_MAX && IS_PRINT (ch) && INDEX (curs) < MARGIN - 1) {
    PRECMD (lin)[INDEX (curs)] = (char) TO_UCHAR (ch);
    COL (curs) = (COL (curs) == MARGIN - 1 ? MARGIN - 1 : COL (curs) + 1);
  } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE) && COL (curs) > 0) {
    int i;
    INDEX (curs) = COL (curs) = (COL (curs) == 0 ? 0 : COL (curs) - 1);
    for (i = INDEX (curs); i < MARGIN - 1; i++) {
      PRECMD (lin)[i] = PRECMD (lin)[i + 1];
    }
  } else if (ch == KEY_DC && COL (curs) < MARGIN - 1) {
    int i;
    for (i = INDEX (curs); i < MARGIN - 1; i++) {
      PRECMD (lin)[i] = PRECMD (lin)[i + 1];
    }
  }
}

/*
\brief edit command
\param dd current dataset
\param ch typed character
**/

static void edit_cmd (DATASET_T *dd, int ch)
{
/*
Command editing is in insert or overwrite mode.
The command line is as wide as the screen minus the prompt.
If the cursor is outside the command string the latter is lengthened.
DEL erases the character under the cursor.
BACKSPACE erases the character to the left of the cursor.
*/
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  if (ch <= UCHAR_MAX && IS_PRINT (ch) && (int) strlen (CMD (scr)) < (int) TEXT_WIDTH) {
    int j, k;
    while ((int) INDEX (curs) > (int) strlen (CMD (scr))) {
      k = (int) strlen (CMD (scr));
      CMD (scr)[k] = BLANK_CHAR;
      CMD (scr)[k + 1] = NULL_CHAR;
    }
    if (INS_MODE (scr)) {
      k = (int) strlen (CMD (scr));
      for (j = k + 1; j > INDEX (curs); j--) {
        CMD (scr)[j] = CMD (scr)[j - 1];
      }
    }
    CMD (scr)[INDEX (curs)] = (char) TO_UCHAR (ch);
    COL (curs) = (COL (curs) == COLS - 1 ? 0 : COL (curs) + 1);
  } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE) && INDEX (curs) > 0) {
    int k;
    if (INDEX (curs) == 0) {
      return;
    } else {
      INDEX (curs)--;
      COL (curs)--;
    }
    for (k = INDEX (curs); k < (int) strlen (CMD (scr)); k++) {
      CMD (scr)[k] = CMD (scr)[k + 1];
    }
  } else if (ch == KEY_DC && COL (curs) < COLS - 1) {
    int k;
    for (k = INDEX (curs); k < (int) strlen (CMD (scr)); k++) {
      CMD (scr)[k] = CMD (scr)[k + 1];
    }
  }
}

/*
\brief edit text
\param dd current dataset
\param ch typed character
**/

static void edit_text (DATASET_T *dd, int ch)
{
/*
Text editing is in insert or overwrite mode.
The string can be extended as long as memory lasts.
If the cursor is outside the string the latter is lengthened.
DEL erases the character under the cursor.
BACKSPACE erases the character to the left of the cursor.
*/
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  int llen = 0;
  EDLIN_T *lin = LINE (curs);
  if (lin == NO_EDLIN) {
    return;
  }
  if (IS_TOF (lin) || IS_EOF (lin)) {
    return;
  }
  if (lin == LAST_LINE (scr)) {
    llen = lines_on_scr (dd, lin);
  }
  alt_line (dd, lin);
  if (ch <= UCHAR_MAX && (IS_PRINT (ch) || ch == '\t')) {
    int j, k, len = (int) strlen (TEXT (lin));
    if (RESERVED (lin) <= len + 2 || RESERVED (lin) <= INDEX (curs) + 2) {
/* Not enough room */
      char *txt = TEXT (lin);
      int l1 = (RESERVED (lin) <= len + 2 ? len + 2 : 0);
      int l2 = (RESERVED (lin) <= INDEX (curs) + 2 ? INDEX (curs) + 2 : 0);
      int res = (l1 > l2 ? l1 : l2) + BLOCK_SIZE;
      if (res % BLOCK_SIZE > 0) {
        res += (BLOCK_SIZE - res % BLOCK_SIZE);
      }
      RESERVED (lin) = res;
      TEXT (lin) = (char *) edit_get_heap (dd, (size_t) res);
      if (TEXT (lin) == NO_TEXT) {
        return;
      }
      bufcpy (TEXT (lin), txt, res);
    }
/* Pad with spaces to cursor position if needed */
    while (INDEX (curs) > (len = (int) strlen (TEXT (lin)))) {
      TEXT (lin)[len] = BLANK_CHAR;
      TEXT (lin)[len + 1] = NULL_CHAR;
    }
    if (INS_MODE (scr)) {
      k = (int) strlen (TEXT (lin));
      for (j = k + 1; j > INDEX (curs); j--) {
        TEXT (lin)[j] = TEXT (lin)[j - 1];
      }
    }
    TEXT (lin)[INDEX (curs)] = (char) TO_UCHAR (ch);
    SYNC_INDEX (curs) = INDEX (curs) + 1;
    SYNC_LINE (curs) = lin;
    SYNC (curs) = A68_TRUE;
  } else if (ch == KEY_BACKSPACE || ch == BACKSPACE) {
    int k;
    char del;
    if (INDEX (curs) == 0) {
      join_line (dd, "edit");
      edit_reset (dd);
      BL_START (dd) = BL_END (dd) = NO_EDLIN;
      return;
    } else {
      INDEX (curs)--;
      del = TEXT (lin)[INDEX (curs)];
    }
    for (k = INDEX (curs); k < (int) strlen (TEXT (lin)); k++) {
      TEXT (lin)[k] = TEXT (lin)[k + 1];
    }
/* Song and dance to avoid problems deleting tabs spanning end-of-screen */
    SYNC_INDEX (curs) = INDEX (curs);
    SYNC_LINE (curs) = lin;
    SYNC (curs) = A68_TRUE;
  } else if (ch == KEY_DC && COL (curs) < COLS) {
    int k;
    for (k = INDEX (curs); k < (int) strlen (TEXT (lin)); k++) {
      TEXT (lin)[k] = TEXT (lin)[k + 1];
    }
    SYNC_INDEX (curs) = INDEX (curs);
    SYNC_LINE (curs) = lin;
    SYNC (curs) = A68_TRUE;
  }
  if (lin == LAST_LINE (scr) && lines_on_scr (dd, lin) > llen) {
    forward_line (&CURR (dd));
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
#define TRM(c) (c == NULL_CHAR || IS_DIGIT (c) || IS_SPACE (c) || IS_PUNCT (c))
  BOOL_T match = A68_TRUE; /* Until proven otherwise */
  if (x == NO_TEXT || c == NO_TEXT) {
    return (A68_FALSE);
  }
/* Single-symbol commands as '?' or '='. */
  if (IS_PUNCT (c[0])) {
    match = (BOOL_T) (x[0] == c[0]);
    if (args != NO_VAR) {
      if (match && x[1] != NULL_CHAR) {
        (*args) = &x[1];
        SKIP_WHITE (*args);
      } else {
        (*args) = NO_TEXT;
      }
    }
    return ((BOOL_T) match);
  }
/* First the required letters */
  while (IS_UPPER (c[0]) && match) {
    match = (BOOL_T) (match & (TO_LOWER (x[0]) == TO_LOWER ((c++)[0])));
    if (! TRM (x[0])) {
      x++;
    }
  }
/* Then the facultative part */
  while (! TRM (x[0]) && c[0] != NULL_CHAR && match) {
    match = (BOOL_T) (match & (TO_LOWER ((x++)[0]) == TO_LOWER ((c++)[0])));
  }
/* Determine the args (arguments, counts) */
  if (args != NO_VAR) {
    if (match && x[0] != NULL_CHAR) {
      (*args) = &x[0];
      SKIP_WHITE (*args);
    } else {
      (*args) = NO_TEXT;
    }
  }
  return ((BOOL_T) match);
#undef TRM
}

/*!
\brief translate integral argument
\param dd current dataset
\param cmd command that calls this routine
\return argument value: default is 1 if no value is present, or -1 if an error occurs
**/

static int int_arg (DATASET_T *dd, char *cmd, char *arg, char **rest, int def)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
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
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: invalid integral argument", cmd) >= 0);
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

static BOOL_T get_subst (DATASET_T *dd, char *cmd, char *arg, char **rest)
{
/* Get the find and replacement string in a substitute command */
  DISPLAY_T *scr = &(DISPLAY (dd));
  char delim, *q, *pat1, *pat2;
  int rc;
  if (EMPTY_STRING (arg)) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
    return (A68_FALSE);
  }
/* Initialise */
  reset_regexp (&(FIND (dd)));
  reset_regexp (&(REPL (dd)));
  (*rest) = NO_TEXT;
  SKIP_WHITE (arg);
  q = arg;
  delim = *(q++);
/* Get find regexp */
  pat1 = PATTERN (&FIND (dd));
  pat1[0] = NULL_CHAR;
  while (q[0] != delim && q[0] != NULL_CHAR) {
    if (q[0] == '\\') {
      *(pat1)++ = *q++;
      if (q[0] == NULL_CHAR) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: invalid regular expression", cmd) >= 0);
        *(pat1) = NULL_CHAR;
        return (A68_FALSE);
      }
      *(pat1)++ = *q++;
    } else {
      *(pat1)++ = *q++;
    }
  }
  *(pat1) = NULL_CHAR;
  if ((int) strlen (PATTERN (&FIND (dd))) == 0) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no regular expression", cmd) >= 0);
    return (A68_FALSE);
  }
  rc = compile_regexp (dd, &(FIND (dd)), cmd);
  if (rc != 0) {
    return (A68_FALSE);
  }
/* Get replacement string */
  if (q[0] != delim) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: unrecognised regular expression syntax", cmd) >= 0);
    return (A68_FALSE);
  }
  q = &q[1];
  pat2 = PATTERN (&REPL (dd));
  pat2[0] = NULL_CHAR;
  while (q[0] != delim && q[0] != NULL_CHAR) {
    if (q[0] == '\\') {
      *(pat2)++ = *q++;
      if (q[0] == NULL_CHAR) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: invalid regular expression", cmd) >= 0);
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
\brief substitute target in one line
\param dd current dataset
\param z line to substitute
\param rep number of substitutions ...
\param start ... starting at this occurence
\param confirm confirm each change
\param cmd command that calls this routine
**/

static int substitute (DATASET_T *dd, EDLIN_T *z, int rep, int start, BOOL_T *confirm, char *cmd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  int k, subs = 0, newt = 0, matcnt = 0;
/* Initialisation */
  for (k = 0; k < rep; k ++)
  {
    int i, lenn, lens, lent, nnwt, pos = 0;
    char *txt = &(TEXT (z)[newt]);
    int rc = regexec (&(COMPILED (&FIND (dd))), txt, NUM_MATCH (&FIND (dd)), MATCH (&FIND (dd)), (k == 0 ? 0 : REG_NOTBOL));
    if (rc == REG_NOMATCH) {
      goto subst_end;
    }
    matcnt++;
    if (matcnt < start) {
      newt += RM_EO (&(MATCH (&FIND (dd))[0]));
      continue;
    }
    if (*confirm) {
      char ch;
      BOOL_T loop;
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: [A]ll, [S]ubstitute, [N]ext or [Q]uit?", cmd) >= 0);
      CURR (dd) = z;
      edit_reset (dd);
      align_current (dd);
      M_MATCH (dd) = z;
      M_SO (dd) = newt + RM_SO (&(MATCH (&FIND (dd))[0]));      
      M_EO (dd) = newt + RM_EO (&(MATCH (&FIND (dd))[0]));      
      CURSOR_TO_COMMAND (dd, curs);
      edit_draw (dd);
      EDIT_TEST (wmove (stdscr, ROW (curs), COL (curs)) != ERR);
      EDIT_TEST (wrefresh (stdscr) != ERR);
      M_MATCH (dd) = NO_EDLIN;
      M_SO (dd) = -1;
      M_EO (dd) = -1;
      loop = A68_TRUE;
      while (loop) {
        ch = TO_LOWER (wgetch (stdscr));
        switch (ch) {
          case 'a': {loop = A68_FALSE; *confirm = A68_FALSE; break;}
          case 's': {loop = A68_FALSE; break;}
          case 'q': {
            bufcpy (DL0 (scr), "", BUFFER_SIZE);
            bufcpy (CMD (scr), "", BUFFER_SIZE);
            return (SUBST_ERROR);
          }
          case 'n': {
            newt += RM_EO (&(MATCH (&FIND (dd))[0]));
            goto do_nothing;
          }
        }
      }      
    }
/* Part before match */
    (LINBUF (dd))[0] = NULL_CHAR;
    lent = (int) strlen (TEXT (z));
    for (i = 0; i < newt + RM_SO (&(MATCH (&FIND (dd))[0])); i ++) {
      add_linbuf (dd, TEXT (z)[i], pos);
      if (LINBUF (dd) == NO_TEXT) {
        return (SUBST_ERROR);
      }
      pos++;
    }
/* Insert substitution string */
    lens = (int) strlen (PATTERN (&REPL (dd)));
    i = 0;
    while (i < lens && PATTERN (&REPL (dd))[i] != NULL_CHAR) {
      if (PATTERN (&REPL (dd))[i] == '\\' && (
            IS_DIGIT (PATTERN (&REPL (dd))[i + 1]) ||
            IS_UPPER (PATTERN (&REPL (dd))[i + 1]) ||
            IS_LOWER (PATTERN (&REPL (dd))[i + 1])
          )
        ) {
        int n = 0, strop = 0, j;
        switch (PATTERN (&REPL (dd))[i + 1]) {
          case '1': {n = 1; break;}
          case '2': {n = 2; break;}
          case '3': {n = 3; break;}
          case '4': {n = 4; break;}
          case '5': {n = 5; break;}
          case '6': {n = 6; break;}
          case '7': {n = 7; break;}
          case '8': {n = 8; break;}
          case '9': {n = 9; break;}
          case 'A': {n = 1; strop = 1; break;}
          case 'B': {n = 2; strop = 1; break;}
          case 'C': {n = 3; strop = 1; break;}
          case 'D': {n = 4; strop = 1; break;}
          case 'E': {n = 5; strop = 1; break;}
          case 'F': {n = 6; strop = 1; break;}
          case 'G': {n = 7; strop = 1; break;}
          case 'H': {n = 8; strop = 1; break;}
          case 'I': {n = 9; strop = 1; break;}
          case 'a': {n = 1; strop = -1; break;}
          case 'b': {n = 2; strop = -1; break;}
          case 'c': {n = 3; strop = -1; break;}
          case 'd': {n = 4; strop = -1; break;}
          case 'e': {n = 5; strop = -1; break;}
          case 'f': {n = 6; strop = -1; break;}
          case 'g': {n = 7; strop = -1; break;}
          case 'h': {n = 8; strop = -1; break;}
          case 'i': {n = 9; strop = -1; break;}
          default: {
            if (n >= (int) NUM_MATCH (&FIND (dd))) {
              ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: invalid group \\%d", cmd, PATTERN (&REPL (dd))[i + 1]) >= 0);
              return (SUBST_ERROR);
            }
          }
        }
        if (n >= (int) NUM_MATCH (&FIND (dd))) {
          ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no group \\%d in regular expression", cmd, n) >= 0);
          return (SUBST_ERROR);
        }
        if (RM_SO (&(MATCH (&FIND (dd))[n])) == -1 && RM_EO (&(MATCH (&FIND (dd))[n])) == -1) {
          ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: group \\%d in regular expression not set", cmd, n) >= 0);
          return (SUBST_ERROR);
        }
        for (j = RM_SO (&(MATCH (&FIND (dd))[n])); j < RM_EO (&(MATCH (&FIND (dd))[n])); j ++) {
          if (strop == -1) {
            add_linbuf (dd, TO_LOWER (TEXT (z)[newt + j]), pos);
          } else if (strop == 1) {
            add_linbuf (dd, TO_UPPER (TEXT (z)[newt + j]), pos);
          } else {
            add_linbuf (dd, TEXT (z)[newt + j], pos);
          }
          if (LINBUF (dd) == NO_TEXT) {
            return (SUBST_ERROR);
          }
          pos++;
        }
        i++; /* Skip digit in \n */
      } else {
        add_linbuf (dd, PATTERN (&REPL (dd))[i], pos);
        if (LINBUF (dd) == NO_TEXT) {
          return (SUBST_ERROR);
        }
        pos++;
      }
      i++;
    }
    nnwt = pos;
/* Part after match */
    for (i = newt + RM_EO (&(MATCH (&FIND (dd))[0])); i < lent; i ++) {
      add_linbuf (dd, TEXT (z)[i], pos);
      if (LINBUF (dd) == NO_TEXT) {
        return (SUBST_ERROR);
      }
      pos++;
    }
    add_linbuf (dd, NULL_CHAR, pos);
    if (LINBUF (dd) == NO_TEXT) {
      return (SUBST_ERROR);
    }
/* Copy the new line */
    newt = nnwt;
    subs++;
    lenn = (int) strlen (LINBUF (dd));
    if (RESERVED (z) >= lenn + 1) {
      bufcpy (TEXT (z), LINBUF (dd), RESERVED (z));
    } else {
      int res = lenn + 1;
      if (res % BLOCK_SIZE > 0) {
        res += (BLOCK_SIZE - res % BLOCK_SIZE);
      }
      RESERVED (z) = res;
      TEXT (z) = (char *) edit_get_heap (dd, (size_t) res);
      if (TEXT (z) == NO_TEXT) {
        return (SUBST_ERROR);
      }
      bufcpy (TEXT (z), LINBUF (dd), res);
    }
    ALTS (dd)++;
    if (TEXT (z)[newt] == NULL_CHAR) {
      goto subst_end;
    }
    do_nothing: /* skip */;
  }
  subst_end:
  return (subs);
}

/*
\brief dispatch lines to shell command and insert command output
\param dd current dataset
\param cmd edit cmd that calls this routine
\param u target line
\param argv shell command
**/

static void edit_filter (DATASET_T *dd, char *cmd, char *argv, EDLIN_T *u)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  char shell[BUFFER_SIZE];
  EDLIN_T *z;
/* Write selected lines ... */
  edit_write (dd, cmd, ".a68g.edit.out", CURR (dd), u);
/* Delete the original lines */
  for (z = CURR (dd); z != u && IS_IN_TEXT (z); forward_line (&z)) {
    LINE (curs) = z;
    LAST (curs) = z;
    INDEX (curs) = 0;
    cdelete (dd);
    join_line (dd, cmd);
  }
  if (IS_IN_TEXT (z)) {
    CURR (dd) = PREVIOUS (z);
  } else {
    CURR (dd) = TOF (dd);
  }
  align_current (dd);
/* ... process the lines ... */
  RESET_ERRNO;
  ASSERT (snprintf (shell, SNPRINTF_SIZE, "%s < .a68g.edit.out > .a68g.edit.in", argv) >= 0);
 EDIT_TEST (system (shell) != -1);
  CHECK_ERRNO (cmd);
/* ... and read lines */
  edit_read (dd, cmd, ".a68g.edit.in", NO_EDLIN);
/* Done */
  bufcpy (CMD (scr), "", BUFFER_SIZE);
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

static void move_copy (DATASET_T *dd, char *cmd, char *args, BOOL_T cmd_move)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  EDLIN_T *u, *v, *w, *x, *z, *bl_start = NO_EDLIN, *bl_end = NO_EDLIN;
  char *cmdn, *rest = NO_TEXT;
  int j, n, count;
  if (cmd_move) {
    cmdn = "move";
  } else {
    cmdn = "copy";
  }
  if (SUBSET (dd)) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: fold disables %s", cmdn, cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  u = CURR (dd);
  if (EMPTY_STRING (args)) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: insufficient arguments", cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  v = get_target (dd, cmd, args, &rest, A68_TRUE);
  args = rest;
  if (EMPTY_STRING (args)) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: insufficient arguments", cmdn) >= 0);
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
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot add after end-of-data", cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
/* Backwards range */
  if (NOT_EOF (v) && (NUMBER (v) < NUMBER (u))) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: backward range", cmdn) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
/* Copy to within range */
  if (NUMBER (u) <= NUMBER (w) && NUMBER (w) < NUMBER (v) - 1) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: target within selected range", cmdn) >= 0);
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
    for (k = 0, z = u; k < count && IS_IN_TEXT (z); k++, FORWARD (z)) {
      LINE (curs) = w;
      INDEX (curs) = (int) strlen (TEXT (w));
      split_line (dd, cmd);
    }
/* Copy text */
    bl_start = NEXT (w);
    for (k = 0, x = NEXT (w), z = u; k < count && IS_IN_TEXT (z); k++, FORWARD (x), FORWARD (z)) {
      char *txt = TEXT (z);
      int len = 1 + (int) strlen (txt);
      int res = len;
      if (res % BLOCK_SIZE > 0) {
        res += (BLOCK_SIZE - res % BLOCK_SIZE);
      }
      bl_end = x;
      RESERVED (x) = res;
      TEXT (x) = (char *) edit_get_heap (dd, (size_t) res);
      if (TEXT (x) == NO_TEXT) {
        return;
      }
      bufcpy (TEXT (x), txt, res);
    }
  }
/* Delete the original lines */
  if (cmd_move) {
    for (z = u; z != v && IS_IN_TEXT (z); FORWARD (z)) {
      LINE (curs) = z;
      LAST (curs) = z;
      INDEX (curs) = 0;
      cdelete (dd);
      join_line (dd, cmd);
    }
  }
/* Done */
  edit_reset (dd);
  if ((count * n) == 0) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s no lines", cmdn, (cmd_move ? "moved" : "copied")) >= 0);
  } else if ((count * n) == 1) {
    BL_START (dd) = bl_start;
    BL_END (dd) = bl_end;
    ALTS (dd)++;
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s 1 line", cmdn, (cmd_move ? "moved" : "copied")) >= 0);
  } else if (n == 1) {
    BL_START (dd) = bl_start;
    BL_END (dd) = bl_end;
    ALTS (dd)++;
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s %d lines", cmdn, (cmd_move ? "moved" : "copied"), count * n) >= 0);
  } else {
    BL_START (dd) = bl_start;
    BL_END (dd) = bl_end;
    ALTS (dd)++;
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %s %d lines %d times", cmdn, (cmd_move ? "moved" : "copied"), count, n) >= 0);
  }
  bufcpy (CMD (scr), "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
  return;
}

/*
\brief indent lines to a column
\param dd current dataset
\param cmd command that calls this
\param args points to arguments
**/

static void indent (DATASET_T *dd, char *cmd, char *args)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  EDLIN_T *u, *v, *z;
  char *rest = NO_TEXT;
  int dir, k, n, m, count;
  if (SUBSET (dd)) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: folded dataset", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  u = CURR (dd);
  if (EMPTY_STRING (args)) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: insufficient arguments", cmd) >= 0);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  v = get_target (dd, cmd, args, &rest, A68_TRUE);
  args = rest;
  if (EMPTY_STRING (args)) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: insufficient arguments", cmd) >= 0);
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
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: backward range", cmd) >= 0);
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
    } else /* if (dir == -1) */ {
      m = k - n;
    }
/* Align the line, if we can */
    if (k >= 0 && NUMBER (z) != 0) {
      int delta = m - k, i, j;
      char *t = TEXT (z);
      (LINBUF (dd))[0] = 0;
      i = 0;
      if (delta >= 0) {
        for (j = 0; j < delta; j ++) {
          add_linbuf (dd, BLANK_CHAR, i++);
          if (LINBUF (dd) == NO_TEXT) {
            CURSOR_TO_COMMAND (dd, curs);
            return;
          }
        }
        for (j = 0; t[j] != NULL_CHAR; j++) {
          add_linbuf (dd, t[j], i++);
          if (LINBUF (dd) == NO_TEXT) {
            CURSOR_TO_COMMAND (dd, curs);
            return;
          }
        }
      } else {
        for (j = 0; j < -delta && t[j] != NULL_CHAR && IS_SPACE (t[j]); j ++) {;}
        for (; t[j] != NULL_CHAR; j++) {
          add_linbuf (dd, t[j], i++);
          if (LINBUF (dd) == NO_TEXT) {
            CURSOR_TO_COMMAND (dd, curs);
            return;
          }
        }
      }
      new_edit_string (dd, z, LINBUF (dd), NO_EDLIN);
      alt_line (dd, z);
      if (TEXT (z) == NO_TEXT) {
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      count++;
    }
  }
/* Done */
  edit_reset (dd);
  BL_START (dd) = BL_END (dd) = NO_EDLIN;
  if (count == 0) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: indented no lines", cmd) >= 0);
  } else if (count == 1) {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: indented 1 line", cmd) >= 0);
  } else {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: indented %d lines", cmd, count) >= 0);
  }
  bufcpy (CMD (scr), "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
  return;
}

/*
\brief set current line
\param dd current dataset
\param cmd command that calls this
\param target to point current line to
**/

static void set_current (DATASET_T *dd, char *cmd, char *target)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  char *rest = NO_TEXT;
  EDLIN_T *z = get_target (dd, cmd, target, &rest, A68_TRUE);
  if (!EMPTY_STRING (rest)) {
    TRAILING (cmd);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  if (z != NO_EDLIN) {
    CURR (dd) = z;
  }
  bufcpy (CMD (scr), "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
}

/*
\brief set current line and store target as command
\param dd current dataset
\param cmd command that calls this
\param target to point current line to
**/

static void set_current_store (DATASET_T *dd, char *cmd, char *target)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  char *rest = NO_TEXT;
  EDLIN_T *z;
  z = get_target (dd, cmd, target, &rest, A68_TRUE);
  if (!EMPTY_STRING (rest)) {
    TRAILING (cmd);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
  if (z != NO_EDLIN) {
    CURR (dd) = z;
  }
  bufcpy (CMD (scr), "", BUFFER_SIZE);
  CURSOR_TO_COMMAND (dd, curs);
}

/*
\brief give full command name
\param cmd command that calls this
\return same
**/

static char *full_cmd (char *cmd)
{
  if (match_cmd (cmd, "Add", NO_VAR)) {
    return ("add");
  } else if (match_cmd (cmd, "AGain", NO_VAR)) {
    return ("again");
  } else if (match_cmd (cmd, "Indent", NO_VAR)) {
    return ("indent");
  } else if (match_cmd (cmd, "CAse", NO_VAR)) {
    return ("case");
  } else if (match_cmd (cmd, "CDelete", NO_VAR)) {
    return ("cdelete");
  } else if (match_cmd (cmd, "RUN", NO_VAR)) {
    return ("run");
  } else if (match_cmd (cmd, "COpy", NO_VAR)) {
    return ("copy");
  } else if (match_cmd (cmd, "DELete", NO_VAR)) {
    return ("delete");
  } else if (match_cmd (cmd, "Edit", NO_VAR)) {
    return ("edit");
  } else if (match_cmd (cmd, "FILE", NO_VAR)) {
    return ("file");
  } else if (match_cmd (cmd, "FOld", NO_VAR)) {
    return ("fold");
  } else if (match_cmd (cmd, "MEssage", NO_VAR)) {
    return ("message");
  } else if (match_cmd (cmd, "MOve", NO_VAR)) {
    return ("move");
  } else if (match_cmd (cmd, "Page", NO_VAR)) {
    return ("page");
  } else if (match_cmd (cmd, "PF", NO_VAR)) {
    return ("pf");
  } else if (match_cmd (cmd, "QQuit", NO_VAR)) {
    return ("qquit");
  } else if (match_cmd (cmd, "Read", NO_VAR)) {
    return ("read");
  } else if (match_cmd (cmd, "RESet", NO_VAR)) {
    return ("reset");
  } else if (match_cmd (cmd, "SAve", NO_VAR)) {
    return ("save");
  } else if (match_cmd (cmd, "SET", NO_VAR)) {
    return ("set");
  } else if (match_cmd (cmd, "SHell", NO_VAR)) {
    return ("shell");
  } else if (match_cmd (cmd, "Help", NO_VAR)) {
    return ("help");
  } else if (match_cmd (cmd, "SYntax", NO_VAR)) {
    return ("syntax");
  } else if (match_cmd (cmd, "S", NO_VAR)) {
    return ("substitute");
  } else if (match_cmd (cmd, "S", NO_VAR)) {
    return ("sc");
  } else if (match_cmd (cmd, "TOGgle", NO_VAR)) {
    return ("toggle");
  } else if (match_cmd (cmd, "Undo", NO_VAR)) {
    return ("undo");
  } else if (match_cmd (cmd, "WQ", NO_VAR)) {
    return ("wq");
  } else if (match_cmd (cmd, "Write", NO_VAR)) {
    return ("write");
  } else if (match_cmd (cmd, "Xedit", NO_VAR)) {
    return ("xedit");
  } else {
    return (cmd);
  }
}

/*
\brief place reserved line
\param dd current dataset
\param line pointer to line
\param fcmd full name of SET command
\param args arguments
**/

static void edit_place (DATASET_T *dd, int *line, char *fcmd, char *args)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  char *rest = NO_TEXT;
  if (match_cmd (args, "TOP", &rest)) {
    if (!EMPTY_STRING (rest)) {
      TRAILING (fcmd);
      return;
    }
    if (reserved_row (dd, 0)) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot place at row %d", fcmd, 1) >= 0);
      return;
    }
    (*line) = 0;
    return;
  } else if (match_cmd (args, "BOTtom", &rest)) {
    if (!EMPTY_STRING (rest)) {
      TRAILING (fcmd);
      return;
    }
    if (reserved_row (dd, LINES - 1)) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot place at row %d", fcmd, LINES) >= 0);
      return;
    }
    (*line) = LINES - 1;
    return;
  } else {
    int n = int_arg (dd, fcmd, args, &rest, 1 + LINES / 2);
    if (!EMPTY_STRING (rest)) {
      TRAILING (fcmd);
      return;
    }
    if ((n < 0 || n > LINES) || reserved_row (dd, n - 1)) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot place at row %d", fcmd, n) >= 0);
      return;
    }
    (*line) = n - 1;
    return;
  }
}

/*
\brief execute set command
\param dd current dataset
\param fcmd full name of SET command
\param cmd command after SET
**/

static void edit_set_cmd (DATASET_T *dd, char *fcmd, char *cmd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  char *args = NO_TEXT, *rest = NO_TEXT;
  char gcmd[SNPRINTF_SIZE];
  ASSERT (snprintf (gcmd, SNPRINTF_SIZE, "%s %s", fcmd, cmd) >= 0);
  edit_add_history (CMD (scr));
  if (match_cmd (cmd, "SCALE", &args)) {
/* SCALE OFF|TOP|BOTTOM|*|n: set scale row */
    if (match_cmd (args, "OFF", &rest)) {
      if (!EMPTY_STRING (rest)) {
        TRAILING (gcmd);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      SCALE_ROW (scr) = A68_MAX_INT;
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      edit_place (dd, &SCALE_ROW (scr), gcmd, args);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "IDF", &args)) {
/* IDF OFF|TOP|BOTTOM|*|n: set scale row */
    if (match_cmd (args, "OFF", &rest)) {
      if (!EMPTY_STRING (rest)) {
        TRAILING (gcmd);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      IDF_ROW (scr) = A68_MAX_INT;
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      edit_place (dd, &IDF_ROW (scr), gcmd, args);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "CMD", &args)) {
/* CMD TOP|BOTTOM|*|n: set command row */
    edit_place (dd, &CMD_ROW (scr), gcmd, args);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else {
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: undefined command \"%s\"", gcmd) >= 0);
  }
}

/*
\brief execute editor command
\param dd current dataset
**/

static void edit_do_cmd (DATASET_T *dd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  char *cmd = CMD (scr), *args = NO_TEXT, *rest = NO_TEXT;
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
    bufcpy (CMD (scr), &cp[1], BUFFER_SIZE);
    edit_do_cmd (dd);
    bufcpy (CMD (scr), cp, BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "?", &args)) {
/* Restore last command; do not execute */
    NO_ARGS ("?", args);
    if (histcurr >= 0) {
      bufcpy (CMD (scr), history[histcurr], BUFFER_SIZE);
    }
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "=", &args)) {
    NO_ARGS ("=", args);
/* Repeat last command on "=" */
    if (histcurr >= 0) {
      bufcpy (CMD (scr), history[histcurr], BUFFER_SIZE);
      edit_do_cmd (dd);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
    }
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } 
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
    edit_add_history (cmd);
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '~' && cmd[1] == '/') {
    edit_add_history (cmd);
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '-' && cmd[1] == '/') {
    edit_add_history (cmd);
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '+' && cmd[1] == '/') {
    edit_add_history (cmd);
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '-' && cmd[1] == '~' && cmd[2] == '/') {
    edit_add_history (cmd);
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  } else if (cmd[0] == '+' && cmd[1] == '~' && cmd[2] == '/') {
    edit_add_history (cmd);
    set_current_store (dd, "edit", cmd);
    align_current (dd);
    return;
  }
  if (match_cmd (cmd, "AGain", &args)) {
/* AGAIN: repeat last search */
    EDLIN_T *z;
    NO_ARGS (fcmd, args);
    z = get_regexp (dd, cmd, args, &rest, A68_FALSE);
    if (z != NO_EDLIN) {
      CURR (dd) = z;
    }
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "TOGgle", &args)) {
/* TOGGLE: switch between prefix/text and command line */
    NO_ARGS (fcmd, args);
    if (IN_CMD (curs)) {
      CURSOR_TO_CURRENT (dd, curs);
    } else if (! IN_FORBIDDEN (curs)) {
      CURSOR_TO_COMMAND (dd, curs);
    } else {
      CURSOR_TO_COMMAND (dd, curs);
    }
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  } else if (match_cmd (cmd, "Help", &args)) {
/* HELP: Give some rudimentary help */
    NO_ARGS (fcmd, args);
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "help") > 0);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  } else if (match_cmd (cmd, "CDelete", &args)) {
/* CDELETE: delete to end of line */
    edit_add_history (cmd); 
    NO_ARGS (fcmd, args);
    if (IN_TEXT (curs)) {
      cdelete (dd);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
    } else if (IN_CMD (curs)) {
      CURSOR_TO_COMMAND (dd, curs);
    }
    return;
/* RESET: reset prefix commands */
  } else if (match_cmd (cmd, "RESet", &args)) {
    NO_ARGS (fcmd, args);
    edit_add_history (cmd); 
    edit_reset (dd);
    BL_START (dd) = BL_END (dd) = NO_EDLIN;
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  } else if (match_cmd (cmd, "QQuit", &args)) {
/* QQUIT: quit immediately - live with it like a Klingon */
    NO_ARGS (fcmd, args);
    if (ALTS (dd) > 0) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: file not saved", fcmd) >= 0);
      ALTS (dd) = 0;
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      if ((UNDO (dd)[0]) != NULL_CHAR) {
        EDIT_TEST (remove (UNDO (dd)) != -1);
      }
      longjmp (EDIT_EXIT_LABEL (dd), 1);
    }
  } else if (match_cmd (cmd, "Page", &args)) {
/* PAGE n: trace forward or backward by "drawing" the screen */
    int k, n = int_arg (dd, fcmd, args, &rest, 1);
    EDLIN_T *old = CURR (dd);
    BOOL_T at_bound = A68_FALSE;
    if (!EMPTY_STRING (rest)) {
      TRAILING (fcmd);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
/* Count */
    for (k = 0; k < abs (n); k++) {
      int lin = count_reserved (dd);
      EDLIN_T *z = CURR (dd), *u = z;
      BOOL_T cont = A68_TRUE;
      for (; IS_IN_TEXT (z) && cont;) {
        lin += lines_on_scr (dd, z);
        if (lin > LINES) {
          cont = A68_FALSE;
        } else {
          u = z;
          (n > 0 ? forward_line (&z) : backward_line (&z));
        }
      }
      if (lin > LINES) {
        CURR (dd) = u;
      } else {
        at_bound = A68_TRUE;
        CURR (dd) = z;
      }
      align_current (dd);
    }
    if (CURR (dd) == old) {
      if (at_bound) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: at file boundary", fcmd) >= 0);
      } else {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: line does not fit screen", fcmd) >= 0);
      }
    }
    CURSOR_TO_COMMAND (dd, curs);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  } else if (match_cmd (cmd, "CAse", &args)) {
/* CASE: switch case of character under cursor */
    EDLIN_T *lin = LINE (curs);
    NO_ARGS (fcmd, args);
    if (lin != NO_EDLIN && INDEX (curs) < (int) strlen (TEXT (lin))) {
      if (IS_UPPER (TEXT (lin)[INDEX (curs)])) {
        TEXT (lin)[INDEX (curs)] = TO_LOWER (TEXT (lin)[INDEX (curs)]);
        INDEX (curs)++;
        SYNC_LINE (curs) = lin;
        SYNC_INDEX (curs) = INDEX (curs);
        SYNC (curs) = A68_TRUE;
        alt_line (dd, lin);
        bufcpy (CMD (scr), "", BUFFER_SIZE);
      } else if (IS_LOWER (TEXT (lin)[INDEX (curs)])) {
        TEXT (lin)[INDEX (curs)] = TO_UPPER (TEXT (lin)[INDEX (curs)]);
        INDEX (curs)++;
        SYNC_LINE (curs) = lin;
        SYNC_INDEX (curs) = INDEX (curs);
        SYNC (curs) = A68_TRUE;
        alt_line (dd, lin);
        bufcpy (CMD (scr), "", BUFFER_SIZE);
      }
    }
    return;
  } else if (match_cmd (cmd, "Add", &args)) {
/* ADD [repetition]: add lines */
    EDLIN_T *z = CURR (dd);
    int k, n = int_arg (dd, fcmd, args, &rest, 1);
    edit_add_history (cmd); 
    if (!EMPTY_STRING (rest)) {
      TRAILING (fcmd);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
    if (z != NO_EDLIN && NOT_EOF (z)) {
      for (k = 0; k < n; k ++) {
        LINE (curs) = z;
        INDEX (curs) = (int) strlen (TEXT (z));
        split_line (dd, fcmd);
      }
      edit_reset (dd);
      BL_START (dd) = BL_END (dd) = NO_EDLIN;
/* Cursor goes to next appended line, not the current line */
      LINE (curs) = NEXT (z);
      SELECT (LINE (curs)) = A68_TRUE;
      INDEX (curs) = 0;
      SYNC_LINE (curs) = LINE (curs);
      SYNC_INDEX (curs) = 0;
      SYNC (curs) = A68_TRUE;
      bufcpy (CMD (scr), "", BUFFER_SIZE);
    } else {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot add lines here", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
    }
    return;
  } else if (match_cmd (cmd, "DELete", &args)) {
/* DELETE [/target/]: delete lines */
    EDLIN_T *u, *z;
    int dels = 0;
    edit_add_history (cmd); 
    if (EMPTY_STRING (args)) {
      u = CURR (dd);
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
    if (NOT_EOF (u) && (NUMBER (u) < NUMBER (CURR (dd)))) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: backward range", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
    edit_write_undo_file (dd, fcmd);
    for (z = CURR (dd); z != u && IS_IN_TEXT (z); forward_line (&z)) {
      LINE (curs) = z;
      LAST (curs) = z;
      INDEX (curs) = 0;
      cdelete (dd);
      join_line (dd, fcmd);
      dels++;
    }
    CURR (dd) = z;
    align_current (dd);
    edit_reset (dd);
    BL_START (dd) = BL_END (dd) = NO_EDLIN;
    if (dels == 0) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: deleted no lines", fcmd) >= 0);
    } else if (dels == 1) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: deleted 1 line", fcmd) >= 0);
    } else {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: deleted %d lines", fcmd, dels) >= 0);
    }
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "FILE", &args) || match_cmd (cmd, "WQ", &args)) {
/* FILE: save and quit */
    if (EMPTY_STRING (args)) {
      edit_write (dd, fcmd, NAME (dd), TOF (dd), NO_EDLIN);
      ALTS (dd) = 0;
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
    } else {
      EDLIN_T *u = get_target (dd, fcmd, args, &rest, A68_TRUE);
      SKIP_WHITE (rest);
      if (EMPTY_STRING (rest)) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: missing filename", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      edit_write (dd, fcmd, rest, CURR (dd), u);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
    }
    if (errno == 0) {
      if ((UNDO (dd)[0]) != NULL_CHAR) {
        EDIT_TEST (remove (UNDO (dd)) != -1);
      }
      longjmp (EDIT_EXIT_LABEL (dd), 1);
    } else {
      CURSOR_TO_COMMAND (dd, curs);
    }
    return;
  } else if (match_cmd (cmd, "Read", &args)) {
/* READ: read a dataset */
    edit_add_history (cmd); 
    if (EMPTY_STRING (args)) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: missing filename", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      edit_write_undo_file (dd, fcmd);
      edit_read (dd, fcmd, args, NO_EDLIN);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "PF", &args)) {
    int n = int_arg (dd, fcmd, args, &rest, 1);
    if (n < 1 || n > MAX_PF) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot set f%d", fcmd, n) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      ASSERT (snprintf (pf_bind[n - 1], SNPRINTF_SIZE, "%s", rest) >= 0);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "SAve", &args) || match_cmd (cmd, "Write", &args)) {
/* Write: save the dataset */
    edit_add_history (cmd); 
    if (EMPTY_STRING (args)) {
      struct stat statbuf;
      edit_write (dd, fcmd, NAME (dd), TOF (dd), NO_EDLIN);
      if (stat (NAME (dd), &statbuf) != -1) {
        struct tm *tm;
        char datestr[BUFFER_SIZE];
        PERMS (dd) = ST_MODE (&statbuf);
        ASSERT ((tm = localtime (&ST_MTIME (&statbuf))) != NULL);
        ASSERT ((strftime (datestr, BUFFER_SIZE, DATE_STRING, tm)) > 0);
        ASSERT (snprintf (DATE (dd), SNPRINTF_SIZE, "%s", datestr) >= 0);
      }
      ALTS (dd) = 0;
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      EDLIN_T *u = get_target (dd, fcmd, args, &rest, A68_TRUE);
      SKIP_WHITE (rest);
      if (EMPTY_STRING (rest)) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: missing filename", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      edit_write (dd, fcmd, rest, CURR (dd), u);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
/* RUN: run a program with a68g */
  } else if (match_cmd (cmd, "RUN", &args)) {
    char ccmd[BUFFER_SIZE];
    int ret;
    EDLIN_T *cursav = CURR (dd);
    edit_add_history (cmd); 
    if (MSGS (dd) != -1) {
      ASSERT (close (MSGS (dd)) == 0);
      MSGS (dd) = -1;
    }
    EDIT_TEST (remove (A68_DIAGNOSTICS_FILE) != -1);
    if (EMPTY_STRING (args)) {
      ASSERT (snprintf (ccmd, SNPRINTF_SIZE, "a68g --tui %s", A68_CHECK_FILE) >= 0);
    } else {
      ASSERT (snprintf (ccmd, SNPRINTF_SIZE, "a68g %s --tui %s", args, A68_CHECK_FILE) >= 0);
    }
    CURR (dd) = NEXT (TOF (dd));
    ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "write * %s", A68_CHECK_FILE) >= 0);
    edit_do_cmd (dd);
    EDIT_TEST (endwin () != ERR);
    ret = system (ccmd);
    edit_init_curses (dd);
    CURR (dd) = cursav;
    ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "message") >= 0);
    edit_do_cmd (dd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
/* SYNTAX: check syntax of an a68 program using a68g */  
  } else if (match_cmd (cmd, "SYntax", &args)) {
    edit_add_history (cmd); 
    ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "run --pedantic --norun") >= 0);
    edit_do_cmd (dd);
    return;
/* MESSAGE: hop through diagnostics of a68g */
  } else if (match_cmd (cmd, "MEssage", &args)) {
    edit_add_history (cmd); 
    NO_ARGS (fcmd, args);
    if (MSGS (dd) == -1) {
    /* Open the file */
      RESET_ERRNO;
      MSGS (dd) = open (A68_DIAGNOSTICS_FILE, A68_READ_ACCESS);
      if (MSGS (dd) == -1 || errno != 0) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot open diagnostics file for reading", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
    }
    if (MSGS (dd) != -1) {
      int n, m;
      EDLIN_T *z;
      edit_read_string (dd, MSGS (dd));
      if (strlen (LINBUF (dd)) == 0) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: no diagnostic to display", fcmd) >= 0);
        ASSERT (close (MSGS (dd)) == 0);
        MSGS (dd) = -1;
        bufcpy (CMD (scr), "", BUFFER_SIZE);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }  
      n = int_arg (dd, cmd, &((LINBUF (dd))[0]), &rest, 1);
      if (n == WRONG_TARGET) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: wrong target in file", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      } else {
        for (z = TOF (dd); z != NO_EDLIN && NUMBER (z) != n; forward_line (&z)) {;}
        if (z != NO_EDLIN) {
          CURR (dd) = z;
          align_current (dd);
        }
      }
      edit_read_string (dd, MSGS (dd));
      SYNC_INDEX (curs) = 0;
      if (n != 0 && n != WRONG_TARGET) {
        m = int_arg (dd, cmd, &((LINBUF (dd))[0]), &rest, 1);
        if (m >= 0 && m < (int) strlen (TEXT (CURR (dd)))) {
          SYNC_INDEX (curs) = m;
        }
      }
      SYNC_LINE (curs) = CURR (dd);
      SYNC (curs) = A68_TRUE;
      edit_read_string (dd, MSGS (dd));
      if ((int) strlen (LINBUF (dd)) >= COLS) {
        (LINBUF (dd))[COLS - 4] = NULL_CHAR;
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s ...", LINBUF (dd)) >= 0);
      } else {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s", LINBUF (dd)) >= 0);
      }
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      return;
    }
  } else if (match_cmd (cmd, "SET", &args)) {
    edit_set_cmd (dd, fcmd, args);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "MSG", &args)) {
/* MSG: display its argument in the message area */
    ARGS (fcmd, args);
    bufcpy (DL0 (scr), args, BUFFER_SIZE);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "Undo", &args)) {
/* UNDO: restore to state last saved, if any */
    edit_read_undo_file (dd, fcmd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
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
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: cannot open file for writing", fcmd) >= 0);
      return;
    }
    for (j = 0; j < LINES; j++) {
      for (k = 0; k < COLS; k++) {
        EDIT_TEST (wmove (stdscr, j, k) != ERR);
        ASSERT (snprintf (buff, SNPRINTF_SIZE, "%c", (char) inch ()) >= 0);
        WRITE (fd, buff);
      }
      WRITE (fd, "\n");
    }
    RESET_ERRNO;
    ASSERT (close (fd) == 0);
    CHECK_ERRNO (fcmd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else if (match_cmd (cmd, "Edit", &args) || match_cmd (cmd, "Xedit", &args)) {
/* EDIT filename: recursively edit another file */
    DATASET_T dataset;
    edit_add_history (cmd); 
    edit_write_undo_file (dd, fcmd);
    edit_dataset (&dataset, NUM (dd) + 1, args, NO_TEXT);
    fixed_heap_pointer = HEAP_POINTER (dd);
    LINBUF (dd) = NO_TEXT;
    LINSIZ (dd) = 0;
    TOF (dd) = NO_EDLIN;
    edit_read_undo_file (dd, fcmd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
/* Commands with targets */
  if (match_cmd (cmd, "FOld", &args)) {
/* FOLD [[TO] /target/]: select lines */
    EDLIN_T *z;
    edit_add_history (cmd); 
    if (!EMPTY_STRING (args) && match_cmd (args, "TO", &rest)) {
/* Select all lines upto matching target */
      EDLIN_T *u;
      args = rest;
      u = get_target (dd, fcmd, args, &rest, A68_FALSE);
      if (!EMPTY_STRING (rest)) {
        TRAILING (fcmd);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      if (!IS_IN_TEXT (u)) {
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
/* Backwards range */
      if (NOT_EOF (u) && (NUMBER (u) < NUMBER (CURR (dd)))) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: backward range", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
/* Empty range */
      if (u == CURR (dd)) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: empty range", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
        SELECT (z) = A68_FALSE;
      }
      for (z = CURR (dd); z != u; FORWARD (z)) {
        SELECT (z) = A68_TRUE;
      }
      SUBSET (dd) = A68_TRUE;
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      M_MATCH (dd) = NO_EDLIN;
      M_SO (dd) = M_EO (dd) = -1; /* Show no match */
      return;
    } else {
/* FOLD [/target/]: select lines matching a target */
/* Reset all lines */
      if (!EMPTY_STRING (args)) {
/* Select all lines that match target */
        EDLIN_T *u = get_target (dd, fcmd, args, &rest, A68_FALSE);
        if (!EMPTY_STRING (rest)) {
          TRAILING (fcmd);
          CURSOR_TO_COMMAND (dd, curs);
          return;
        }
        if (!IS_IN_TEXT (u)) {
          CURSOR_TO_COMMAND (dd, curs);
          return;
        }
        for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
          SELECT (z) = A68_FALSE;
        }
        SELECT (u) = A68_TRUE;
        for (z = NEXT (u); z != NO_EDLIN; FORWARD (z)) {
          SELECT (z) = match_regex (dd, z, 0, fcmd);
        }
        SUBSET (dd) = A68_TRUE;
      } else {
/* UNFOLD */
        for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
          SELECT (z) = A68_TRUE;
        }
        SUBSET (dd) = A68_FALSE;
      }
      CURR (dd) = TOF (dd);
      forward_line (&(CURR (dd)));
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      M_MATCH (dd) = NO_EDLIN;
      M_SO (dd) = M_EO (dd) = -1; /* Show no match */
      return;
    }
  } else if (match_cmd (cmd, "Move", &args)) {
/* MOVE /target/ /target/ [n]: move lines */
    edit_add_history (cmd); 
    move_copy (dd, cmd, args, A68_TRUE);
    return;
  } else if (match_cmd (cmd, "COpy", &args)) {
/* COPY /target/ /target/ [n]: copy lines */
    edit_add_history (cmd); 
    move_copy (dd, cmd, args, A68_FALSE);
  } else if (match_cmd (cmd, "SHell", &args)) {
    edit_add_history (cmd); 
    if (EMPTY_STRING (args)) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: missing arguments", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    } else {
      EDLIN_T *u = get_target (dd, fcmd, args, &rest, A68_TRUE);
      SKIP_WHITE (rest);
      if (EMPTY_STRING (rest)) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: missing shell command", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      edit_write_undo_file (dd, fcmd);
      edit_filter (dd, fcmd, rest, u);
      bufcpy (CMD (scr), "", BUFFER_SIZE);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
  } else if (match_cmd (cmd, "Indent", &args)) {
/* INDENT target column: indent lines to column */
    edit_add_history (cmd); 
    indent (dd, fcmd, args);
    return;
  } else if (match_cmd (cmd, "S", &args)) {
/* SUBSTITUTE /find/replace/ [C] [/target/] [repetition]: replace substrings */
    int reps, start, subs = 0;
    BOOL_T confirm = A68_FALSE;
    edit_add_history (cmd); 
    if (!get_subst (dd, fcmd, args, &rest)) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: unrecognised syntax", fcmd) >= 0);
      CURSOR_TO_COMMAND (dd, curs);
      return;
    }
    if (EMPTY_STRING (rest)) {
      int m;
      confirm = A68_FALSE;
      m = substitute (dd, CURR (dd), A68_MAX_INT, 1, &confirm, fcmd);
      if (m == SUBST_ERROR) {
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      subs = m;
    } else {
      EDLIN_T *u, *z;
      int m;
      SKIP_WHITE (rest);
      if (TO_LOWER (rest[0]) == 'c') {
        confirm = A68_TRUE;
        rest++;
      }
      args = rest;
      u = get_target (dd, fcmd, args, &rest, A68_TRUE);
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
      if (NOT_EOF (u) && (NUMBER (u) < NUMBER (CURR (dd)))) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: backward range", fcmd) >= 0);
        CURSOR_TO_COMMAND (dd, curs);
        return;
      }
      edit_write_undo_file (dd, fcmd);
      for (z = CURR (dd); z != u && IS_IN_TEXT (z); forward_line (&z)) {
        m = substitute (dd, z, reps, start, &confirm, fcmd);
        if (m == SUBST_ERROR) {
          CURSOR_TO_COMMAND (dd, curs);
          return;
        }
        subs += m;
      }
      if (IS_IN_TEXT (z)) {
        CURR (dd) = z;
      }
    }
    if (subs == 1) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %d occurences %sd", fcmd, subs, fcmd) >= 0);
    } else {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "%s: %d occurences %sd", fcmd, subs, fcmd) >= 0);
    }
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  } else {
/* Give error and clear the last command - sorry for the inconvenience */
    edit_add_history (cmd);
    ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: undefined command \"%s\"", cmd) >= 0);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    CURSOR_TO_COMMAND (dd, curs);
    return;
  }
}

/*
\brief do a prefix command
\param dd current dataset
**/

static void edit_do_prefix (DATASET_T *dd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  int as, cs, ccs, ds, dds, is, iis, js, xs, xxs, ps, qs, divs, us, total;
  EDLIN_T *z;
  char *arg;
/* Check prefix command */
  as = cs = ccs = ds = dds = is = iis = js = xs = xxs = ps = qs = divs = us = total = 0;
  for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
    char *p = PRECMD (z);
    SKIP_WHITE (p);
    if (match_cmd (p, "CC", &arg)) {
      if (NUMBER (z) == 0) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "copy: CC in invalid line") >= 0);
      } else {
        ccs++;
        total++;
      }
    } else if (match_cmd (p, "DD", &arg)) {
      if (NUMBER (z) == 0) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "delete: DD in invalid line") >= 0);
      } else {
        dds++;
        total++;
      }
    } else if (match_cmd (p, "II", &arg)) {
      if (NUMBER (z) == 0) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "indent: II in invalid line") >= 0);
      } else {
        iis++;
        total++;
      }
    } else if (match_cmd (p, "XX", &arg)) {
      if (NUMBER (z) == 0) {
        ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "move: XX in invalid line") >= 0);
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
    } else if (match_cmd (p, "U", &arg)) {
      us++;
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
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      EDLIN_T *cursavi = CURR (dd);
      SKIP_WHITE (p);
      if (match_cmd (p, "A", &arg)) {
        CURR (dd) = z;
        ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "add %s", arg) >= 0);
        edit_do_cmd (dd);
        edit_reset (dd);
        BL_START (dd) = BL_END (dd) = NO_EDLIN;
        CURR (dd) = cursavi;
        align_current (dd);
        bufcpy (CMD (scr), "", BUFFER_SIZE);
        return;
      }
    }
  } else if (is == 1 && total == 1) {
/* INDENT */
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      EDLIN_T *cursavi = CURR (dd);
      SKIP_WHITE (p);
      if (match_cmd (p, "I", &arg)) {
        CURR (dd) = z;
        ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "indent 1 %s", arg) >= 0);
        edit_do_cmd (dd);
        edit_reset (dd);
        CURR (dd) = cursavi;
        align_current (dd);
        bufcpy (CMD (scr), "", BUFFER_SIZE);
        return;
      }
    }
  } else if (js == 1 && total == 1) {
/* JOIN */
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      EDLIN_T *cursavi = CURR (dd);
      SKIP_WHITE (p);
      if (match_cmd (p, "J", &arg)) {
        EDLIN_T *x = NEXT (z);
        NO_ARGS ("J", arg);
        if (NUMBER (z) == 0 || x == NO_EDLIN || NUMBER (x) == 0) {
          ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "join: cannot join") >= 0);
        } else {
          CURR (dd) = x;
          LINE (curs) = x;
          LAST (curs) = x;
          INDEX (curs) = 0;
          join_line (dd, "join");
        } 
        edit_reset (dd);
        BL_START (dd) = BL_END (dd) = NO_EDLIN;
        CURR (dd) = cursavi;
        align_current (dd);
        bufcpy (CMD (scr), "", BUFFER_SIZE);
        return;
      }
    }
  } else if (ds == 1 && total == 1) {
/* DELETE */
    EDLIN_T *w;
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      EDLIN_T *cursavi = CURR (dd);
      SKIP_WHITE (p);
      if (match_cmd (p, "D", &arg)) {
        w = CURR (dd);
        CURR (dd) = z;
        if (EMPTY_STRING (arg)) {
          ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "delete") >= 0);
        } else {
          ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "delete %s", arg) >= 0);
        }
        edit_do_cmd (dd);
        if (w == z) {
          forward_line (&w);
        }
        CURR (dd) = w;
        edit_reset (dd);
        BL_START (dd) = BL_END (dd) = NO_EDLIN;
        CURR (dd) = cursavi;
        align_current (dd);
        bufcpy (CMD (scr), "", BUFFER_SIZE);
        return;
      }
    }
  } else if (us == 1 && total == 1) {
/* UNSELECT */
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      EDLIN_T *cursavi = CURR (dd);
      SKIP_WHITE (p);
      if (match_cmd (p, "U", &arg)) {
        NO_ARGS ("U", arg);
        SELECT (z) = A68_FALSE;
        edit_reset (dd);
        BL_START (dd) = BL_END (dd) = NO_EDLIN;
        CURR (dd) = cursavi;
        align_current (dd);
        bufcpy (CMD (scr), "", BUFFER_SIZE);
        return;
      }
    }
  } else if (divs == 1 && total == 1) {
/* Set current line */
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      SKIP_WHITE (p);
      if (match_cmd (p, "/", &arg)) {
        NO_ARGS ("/", arg);
        CURR (dd) = z;
        edit_reset (dd);
        align_current (dd);
        bufcpy (CMD (scr), "", BUFFER_SIZE);
        return;
      }
    }
  } else if (dds == 2 && total == 2) {
/* DELETE block */
    EDLIN_T *u = NO_EDLIN, *v = NO_EDLIN, *w, *cursavi = CURR (dd);
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      SKIP_WHITE (p);
      if (match_cmd (p, "DD", &arg)) {
        NO_ARGS ("DD", arg);
        if (u == NO_EDLIN) {
          u = z;
        } else {
          v = z;
        }
      }
    }
    w = CURR (dd);
    CURR (dd) = u;
    if (IS_EOF (v)) {
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "delete *") >= 0);
    } else {
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "delete :%d+1", NUMBER (v)) >= 0);
    }
    edit_do_cmd (dd);
    if (NUMBER (u) <= NUMBER (w) && NUMBER (w) <= NUMBER (v)) {
      CURR (dd) = PREVIOUS (w);
    } else {
      CURR (dd) = w;
    }
    CURR (dd) = cursavi;
    edit_reset (dd);
    BL_START (dd) = BL_END (dd) = NO_EDLIN;
    align_current (dd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  } else if (iis == 2 && total == 2) {
/* INDENT block */
    EDLIN_T *u = NO_EDLIN, *v = NO_EDLIN, *cursavi = CURR (dd);
    char upto[BUFFER_SIZE];
    char *rep = NO_TEXT;
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      SKIP_WHITE (p);
      if (match_cmd (p, "II", &arg)) {
        if (u == NO_EDLIN) {
          rep = arg;
          u = z;
        } else {
          NO_ARGS ("indent", arg);
          v = z;
        }
      }
    }
    CURR (dd) = u;
    if (IS_EOF (v)) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, "*") >= 0);
    } else if (NOT_EOF (v)) {
      ASSERT (snprintf (upto, BUFFER_SIZE - 1, ":%d+1", NUMBER (v)) >= 0);
    }
    if (EMPTY_STRING (rep)) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "indent: expected column number") >= 0);
    } else {
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "indent %s %s", upto, rep) >= 0);
      edit_do_cmd (dd);
      edit_reset (dd);
    }
    CURR (dd) = cursavi;
    align_current (dd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  } else if ((ccs == 2 || xxs == 2) && (ps == 1 || qs == 1) && total == 3) {
/* COPY or MOVE block */
    EDLIN_T *u = NO_EDLIN, *v = NO_EDLIN, *w = NO_EDLIN, *cursavi = CURR (dd);
    char upto[BUFFER_SIZE], ins[BUFFER_SIZE];
    char *cmd = NO_TEXT, *delim = NO_TEXT, *rep = NO_TEXT;
    if (ccs == 2) {
      cmd = "copy";
      delim = "CC";
    } else if (xxs == 2) {
      cmd = "move";
      delim = "XX";
    }
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      SKIP_WHITE (p);
      if (match_cmd (p, delim, &arg)) {
        NO_ARGS (delim, arg);
        if (u == NO_EDLIN) {
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
    CURR (dd) = u;
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
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "%s %s %s", cmd, upto, ins) >= 0);
    } else {
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "%s %s %s %s", cmd, upto, ins, rep) >= 0);
    }
    edit_do_cmd (dd);
    edit_reset (dd);
    CURR (dd) = cursavi;
    align_current (dd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  } else if ((cs == 1 || xs == 1) && (ps == 1 || qs == 1) && total == 2) {
/* COPY or MOVE line */
    EDLIN_T *u = NO_EDLIN, *w = NO_EDLIN, *cursavi = CURR (dd);
    char ins[BUFFER_SIZE];
    char *cmd = NO_TEXT, *delim = NO_TEXT, *target = NO_TEXT, *rep = NO_TEXT;
    if (cs == 1) {
      cmd = "copy";
      delim = "C";
    } else if (xs == 1) {
      cmd = "move";
      delim = "X";
    }
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
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
    CURR (dd) = u;
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
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "%s 1 %s", cmd, ins) >= 0);
    } else if (EMPTY_STRING (target) && !EMPTY_STRING (rep)) {
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "%s 1 %s %s", cmd, ins, rep) >= 0);
    } else if (!EMPTY_STRING (target) && EMPTY_STRING (rep)) {
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "%s %s %s", cmd, target, ins) >= 0);
    } else if (!EMPTY_STRING (target) && !EMPTY_STRING (rep)) {
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "%s %s %s %s", cmd, target, ins, rep) >= 0);
    }
    ASSERT (snprintf (TMP_TEXT (scr), SNPRINTF_SIZE, "%s", CMD (scr)) >= 0);
    edit_do_cmd (dd);
    edit_reset (dd);
    CURR (dd) = cursavi;
    align_current (dd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  } else if ((ps == 1 || qs == 1) && total == 1) {
/* COPY previous block */
    EDLIN_T *u = BL_START (dd), *v = BL_END (dd), *w = NO_EDLIN, *cursavi = CURR (dd);
    char upto[BUFFER_SIZE], ins[BUFFER_SIZE];
    char *cmd = "copy", *rep = NO_TEXT;
    if (u == NO_EDLIN || v == NO_EDLIN) {
      ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "copy: no previous block") >= 0);
      return;
    }
    for (z = TOF (dd); z != NO_EDLIN; FORWARD (z)) {
      char *p = PRECMD (z);
      SKIP_WHITE (p);
      if (match_cmd (p, "P", &arg)) {
        rep = arg;
        w = z;
      } else if (match_cmd (p, "Q", &arg)) {
        rep = arg;
        w = z;
      }
    }
    CURR (dd) = u;
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
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "%s %s %s", cmd, upto, ins) >= 0);
    } else {
      ASSERT (snprintf (CMD (scr), SNPRINTF_SIZE, "%s %s %s %s", cmd, upto, ins, rep) >= 0);
    }
    edit_do_cmd (dd);
    edit_reset (dd);
    CURR (dd) = cursavi;
    align_current (dd);
    bufcpy (CMD (scr), "", BUFFER_SIZE);
    return;
  }
  ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: unrecognised prefix command") >= 0);
}

/*
\brief execute command from function key
\param dd current dataset
\param cmd command bound to key
**/

static void key_command (DATASET_T *dd, char *cmd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  SAVE_CURSOR (dd, curs);
  bufcpy (CMD (scr), cmd, BUFFER_SIZE);
  edit_do_cmd (dd);
  CURSOR_TO_SAVE (dd, curs);
  REDRAW (stdscr);
}

/*
\brief editor loop: get a key and do something with it
\param dd current dataset
**/

static void edit_loop (DATASET_T *dd)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
  CURSOR_T *curs = &(CURS (scr));
  for (;;) {
    int ch, k;
    loop_cnt ++;
/* Redraw the screen ... */
    edit_draw (dd);
/* ... and set the cursor like this or else ncurses can misplace it :-( */
    EDIT_TEST (wmove (stdscr, ROW (curs), COL (curs)) != ERR);
    EDIT_TEST (wrefresh (stdscr) != ERR);
    bufcpy (DL0 (scr), "", BUFFER_SIZE);
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
        n = 0;
        for (k = 0; CODE (&dec_key[k]) >= 0; k++) {
          if (strncmp (NAME (&dec_key[k]), esc, (size_t) (j + 1)) == 0) {
            n++;
            m = k;
          }
        }
        if (n == 0) {
          ch = '~';
          ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: undefined escape sequence %s", &esc[1]) >= 1);
          cont = A68_FALSE;
        } else if (n == 1) {
          for (j++ ; j != (int) strlen (NAME (&dec_key[m])); j++) {
            esc[j] = (char) wgetch (stdscr);
          }
          if (strcmp (NAME (&dec_key[m]), esc) == 0) {
            ch = CODE (&dec_key[m]);
          } else {
            ch = '~';
            ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: undefined escape sequence %s", &esc[1]) >= 1);
          }
          cont = A68_FALSE;
        } else if (n > 1) {
          ch = wgetch (stdscr);
          j++;
        }
      }
    }
/* Substitute keys for uniform behaviour */
    for (k = 0; CODE (&trans_tab[k]) >= 0; k++) {
      if (ch == CODE (&trans_tab[k])) {
        ch = TRANS (&trans_tab[k]);
      }
    }
/* Interprete the key */
    if (KEY_F0 < ch && ch <= KEY_F0 + 24) {
/* PF keys */
      for (k = 0; k < 24; k++) {
        if (ch == KEY_F0 + k + 1) {
          if ((int) strlen (pf_bind[k]) == 0) {
            ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: PF%02d has no command", k + 1) >= 0);
          } else {
            SAVE_CURSOR (dd, curs);
            bufcpy (CMD (scr), pf_bind[k], BUFFER_SIZE);
            edit_do_cmd (dd);
            if (! match_cmd (pf_bind[k], "TOGGLE", NO_VAR) && ! match_cmd (pf_bind[k], "CASE", NO_VAR)) {
              CURSOR_TO_SAVE (dd, curs);
            }
          }
        }
      }
/* Prefix editing */
    } else if (ch <= UCHAR_MAX && IS_PRINT (ch) && IN_PREFIX (curs)) {
      edit_prefix (dd, ch);
    } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE || ch == KEY_DC) && IN_PREFIX (curs)) {
      edit_prefix (dd, ch);
    } else if (ch == NEWLINE_CHAR && IN_PREFIX (curs)) {
      SAVE_CURSOR (dd, curs);
      edit_do_prefix (dd);
      CURSOR_TO_SAVE (dd, curs);
      REDRAW (stdscr);
/* Command line editing */
    } else if (ch <= UCHAR_MAX && IS_PRINT (ch) && IN_CMD (curs)) {
      edit_cmd (dd, ch);
    } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE || ch == KEY_DC) && IN_CMD (curs)) {
      edit_cmd (dd, ch);
    } else if (ch == NEWLINE_CHAR && IN_CMD (curs)) {
      edit_do_cmd (dd);
      REDRAW (stdscr);
      ROW (curs) = -1;
      COL (curs) = -1;
/* Text editing */
    } else if (ch <= UCHAR_MAX && (IS_PRINT (ch) || ch == '\t') && IN_TEXT (curs)) {
      edit_text (dd, ch);
    } else if ((ch == KEY_BACKSPACE || ch == BACKSPACE || ch == KEY_DC) &&  !IN_FORBIDDEN (curs)) {
      edit_text (dd, ch);
    } else if (ch == NEWLINE_CHAR && !IN_FORBIDDEN (curs)) {
      split_line (dd, "edit");
      edit_reset (dd);
      BL_START (dd) = BL_END (dd) = NO_EDLIN;
      if (SIZE (dd) == 1) {
        CURR (dd) = NEXT (TOF (dd));
      }
    } else if (ch == KEY_RESIZE) {
/* Resize event triggers reinitialisation */
      EDIT_TEST (endwin () != ERR);
      edit_init_curses (dd);
    } else if (ch == KEY_MOUSE) {
/* Mouse control is basic: you can point with the cursor but little else */
#if ! defined HAVE_WIN32
      MEVENT event;
      if (getmouse (&event) != ERR) {
        BSTATE (curs) = BSTATE (&event);
        if (BSTATE (&event) & (BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | 
                            BUTTON1_PRESSED | BUTTON1_RELEASED)) {
          if (reserved_row (dd, Y (&event)) && Y (&event) != CMD_ROW (scr)) {
            PROTECTED ("edit");
          } else {
            ROW (curs) = Y (&event);
            COL (curs) = X (&event);
          }
        } else if (BSTATE (&event) & (BUTTON3_CLICKED | BUTTON3_DOUBLE_CLICKED | 
                                   BUTTON3_PRESSED | BUTTON3_RELEASED)) {
          if (reserved_row (dd, Y (&event)) && Y (&event) != CMD_ROW (scr)) {
            PROTECTED ("edit");
          } else {
            ROW (curs) = Y (&event);
            COL (curs) = X (&event);
          }
        }
      }
#endif /* ! defined HAVE_WIN32 */
/* Keyboard control keys */
    } else if (ch == KEY_UP) {
      int u = ROW (curs);
      do {
        u = (u == 0 ? LINES - 1 : u - 1);
      } while (reserved_row (dd, u) && u != CMD_ROW (scr));
      ROW (curs) = u;
    } else if (ch == KEY_DOWN) {
      int u = ROW (curs);
      do {
        u = (u == LINES - 1 ? 0 : u + 1);
      } while (reserved_row (dd, u) && u != CMD_ROW (scr));
      ROW (curs) = u;
    } else if (ch == KEY_CTRL('W') && histcurr >= 0) {
      if (histprev >= 0) {
        bufcpy (CMD (scr), history[histprev], BUFFER_SIZE);
        edit_set_history (histprev);
      }
    } else if (ch == KEY_CTRL('X') && histcurr >= 0) {
      if (histnext >= 0) {
        bufcpy (CMD (scr), history[histnext], BUFFER_SIZE);
        edit_set_history (histnext);
      }
    } else if (ch == KEY_RIGHT) {
      COL (curs) = (COL (curs) == COLS - 1 ? 0 : COL (curs) + 1);
    } else if (ch == KEY_LEFT) {
      COL (curs) = (COL (curs) == 0 ? COLS - 1 : COL (curs) - 1);
    } else if (ch == KEY_NPAGE || ch == KEY_C3) {
      key_command (dd, "page +1");
    } else if (ch == KEY_PPAGE || ch == KEY_A3) {
      key_command (dd, "page -1");
    } else if (ch == KEY_HOME || ch == KEY_A1) {
      key_command (dd, "-*");
    } else if (ch == KEY_END || ch == KEY_C1) {
      key_command (dd, "+*");
    } else if (ch == KEY_SLEFT || ch == KEY_CTRL('A')) {
      if (IN_TEXT (curs) || IN_CMD (curs)) {
        INDEX (curs) = 0;
        COL (curs) = MARGIN;
      } else {
        PROTECTED ("edit");
      }
    } else if (ch == KEY_SRIGHT || ch == KEY_CTRL ('D')) {
      if (IN_TEXT (curs)) {
        INDEX (curs) = (int) strlen (TEXT (LINE (curs)));
        SYNC_INDEX (curs) = INDEX (curs);
        SYNC_LINE (curs) = LINE (curs);
        SYNC (curs) = A68_TRUE;
      } else if (IN_CMD (curs)) {
        INDEX (curs) = (int) strlen (CMD (scr));
        COL (curs) = MARGIN + INDEX (curs);
      } else {
        PROTECTED ("edit");
      }
    } else if (ch == KEY_B2) {
      if (IN_TEXT (curs)) {
        if (LINE (curs) != NO_EDLIN && NUMBER (LINE (curs)) > 0) {
          CURR (dd) = LINE (curs);
          SYNC_INDEX (curs) = 0;
          SYNC_LINE (curs) = CURR (dd);
          SYNC (curs) = A68_TRUE;
        }
      } else {
        PROTECTED ("edit");
      }
/* Other keys */
    } else if (ch == KEY_IC) {
      INS_MODE (scr) = !INS_MODE (scr);
    } else if (ch > 127) {
      if (IN_FORBIDDEN (curs)) {
        PROTECTED ("edit");
        goto end;
      }
      for (k = 0; CODE (&key_tab[k]) >= 0; k++) {
        if (ch == CODE (&key_tab[k])) {
/* File a complaint. */
          ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: undefined key %s", NAME (&key_tab[k])) >= 1);
          goto end;
        }
      }
      /* ASSERT (snprintf (DL0 (scr), SNPRINTF_SIZE, "edit: undefined key %d", ch) >= 0); */
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

static void edit_dataset (DATASET_T *dd, int num, char *filename, char *target)
{
  DISPLAY_T *scr = &(DISPLAY (dd));
/* Init ncurses */
  edit_init_curses (dd);
  bufcpy (NAME (dd), filename, BUFFER_SIZE);
  TABS (dd) = TAB_STOP;
  HEAP_POINTER (dd) = fixed_heap_pointer;
/* Init edit */
  DL0 (&DISPLAY (dd))[0] = NULL_CHAR;
  CMD (&DISPLAY (dd))[0] = NULL_CHAR;
  LINBUF (dd) = NO_TEXT;
  LINSIZ (dd) = 0;
  COLLECT (dd) = A68_FALSE;
  edit_read_initial (dd, "edit");
  XABEND (heap_full (BUFFER_SIZE), "out of memory", NO_TEXT);
  COLLECT (dd) = A68_TRUE;
  INS_MODE (&DISPLAY (dd)) = A68_TRUE;
  MSGS (dd) = -1; /* File not open */
  NUM (dd) = num;
  UNDO_LINE (dd) = 0;
  SEARCH (dd) = 0;
  BL_START (dd) = BL_END (dd) = NO_EDLIN;
  M_MATCH (dd) = NO_EDLIN;
  M_SO (dd) = M_EO (dd) = -1; /* No match */
  if (target != NO_TEXT && (int) (strlen (target)) > 0) {
    char *rest;
    EDLIN_T *z = get_target (dd, "edit", target, &rest, A68_TRUE);
    if (z != NO_EDLIN) {
      CURR (dd) = z;
    } else {
      ASSERT (snprintf (DL0 (&DISPLAY (dd)), SNPRINTF_SIZE, "edit: optional target not set") >= 0);
    }
  }
  if (!a68g_mkstemp (UNDO (dd), A68_WRITE_ACCESS, A68_PROTECTION)) {
    (UNDO (dd))[0] = NULL_CHAR;
    ASSERT (snprintf (DL0 (&DISPLAY (dd)), SNPRINTF_SIZE, "edit: cannot open temporary file for undo") >= 0);
  }
  EDIT_TEST (remove (UNDO (dd)) != -1);
  loop_cnt = 0;
  if (setjmp (EDIT_EXIT_LABEL (dd)) == 0) {
    edit_loop (dd);
  }
}

/*
\brief edit main routine
\param start_text not used
**/

void edit (char *start_text)
{
  DATASET_T dataset;
  DISPLAY_T *scr = &(DISPLAY (&dataset));
  int k;
  (void) start_text;
  genie_init_rng ();
  for (k = 0; k < HISTORY; k++) {
    bufcpy (history[k], "", BUFFER_SIZE);
  }
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
  if (FILE_INITIAL_NAME (&program) == NO_TEXT) {
#if ! defined HAVE_WIN32
    errno = ENOTSUP;
#endif /* ! defined HAVE_WIN32 */
    SCAN_ERROR (A68_TRUE, NO_LINE, NO_TEXT, "edit: no filename");
  }
  read_history ();
  edit_dataset (&dataset, 1, FILE_INITIAL_NAME (&program), OPTION_TARGET (&program));
/* Exit edit */
  write_history ();
  EDIT_TEST (wclear (stdscr) != ERR);
  EDIT_TEST (wrefresh (stdscr) != ERR);
  EDIT_TEST (endwin () != ERR);
  EDIT_TEST (remove (A68_DIAGNOSTICS_FILE) != -1);
  exit (EXIT_SUCCESS);
}

#endif /* HAVE_EDITOR */
