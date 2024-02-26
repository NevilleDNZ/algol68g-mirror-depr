//! @file rts-curses.c
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
//! Curses interface. 

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"

// Some routines that interface Algol68G and the curses library.

#if defined (HAVE_CURSES)

#define CHECK_CURSES_RETVAL(f) {\
  if (!(f)) {\
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_CURSES);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }}

//! @brief Clean_curses.

void clean_curses (void)
{
  if (A68 (curses_mode) == A68_TRUE) {
    (void) wattrset (stdscr, A_NORMAL);
    (void) endwin ();
    A68 (curses_mode) = A68_FALSE;
  }
}

//! @brief Init_curses.

void init_curses (void)
{
  (void) initscr ();
  (void) cbreak ();             // raw () would cut off ctrl-c
  (void) noecho ();
  (void) nonl ();
  (void) curs_set (0);
  if (has_colors ()) {
    (void) start_color ();
  }
}

//! @brief Watch stdin for input, do not wait very long.

int rgetchar (void)
{
#if defined (BUILD_WIN32)
  if (kbhit ()) {
    return getch ();
  } else {
    return NULL_CHAR;
  }
#else
  int retval;
  struct timeval tv;
  fd_set rfds;
  TV_SEC (&tv) = 0;
  TV_USEC (&tv) = 100;
  FD_ZERO (&rfds);
  FD_SET (0, &rfds);
  retval = select (1, &rfds, NULL, NULL, &tv);
  if (retval) {
// FD_ISSET(0, &rfds) will be true.  
    return getch ();
  } else {
    return NULL_CHAR;
  }
#endif
}

//! @brief PROC curses start = VOID

void genie_curses_start (NODE_T * p)
{
  (void) p;
  init_curses ();
  A68 (curses_mode) = A68_TRUE;
}

//! @brief PROC curses end = VOID

void genie_curses_end (NODE_T * p)
{
  (void) p;
  clean_curses ();
}

//! @brief PROC curses clear = VOID

void genie_curses_clear (NODE_T * p)
{
  if (A68 (curses_mode) == A68_FALSE) {
    genie_curses_start (p);
  }
  CHECK_CURSES_RETVAL (clear () != ERR);
}

//! @brief PROC curses refresh = VOID

void genie_curses_refresh (NODE_T * p)
{
  if (A68 (curses_mode) == A68_FALSE) {
    genie_curses_start (p);
  }
  CHECK_CURSES_RETVAL (refresh () != ERR);
}

//! @brief PROC curses lines = INT

void genie_curses_lines (NODE_T * p)
{
  if (A68 (curses_mode) == A68_FALSE) {
    genie_curses_start (p);
  }
  PUSH_VALUE (p, LINES, A68_INT);
}

//! @brief PROC curses columns = INT

void genie_curses_columns (NODE_T * p)
{
  if (A68 (curses_mode) == A68_FALSE) {
    genie_curses_start (p);
  }
  PUSH_VALUE (p, COLS, A68_INT);
}

//! @brief PROC curses getchar = CHAR

void genie_curses_getchar (NODE_T * p)
{
  if (A68 (curses_mode) == A68_FALSE) {
    genie_curses_start (p);
  }
  PUSH_VALUE (p, (char) rgetchar (), A68_CHAR);
}

//! @brief PROC curses colour = VOID

#define GENIE_COLOUR(f, n, fg, bg)\
void f (NODE_T *p) {\
  (void) p;\
  if ((n) < COLOR_PAIRS) {\
    (void) init_pair (n, (fg), (bg));\
    wattrset (stdscr, COLOR_PAIR ((n)) | A_BOLD);\
  }\
}\
void f##_inverse (NODE_T *p) {\
  (void) p;\
  if ((n + 8) < COLOR_PAIRS) {\
    (void) init_pair ((n) + 8, (bg), (fg));\
    wattrset (stdscr, COLOR_PAIR (((n) + 8)));\
  }\
}

GENIE_COLOUR (genie_curses_blue, 1, COLOR_BLUE, COLOR_BLACK)
  GENIE_COLOUR (genie_curses_cyan, 2, COLOR_CYAN, COLOR_BLACK)
  GENIE_COLOUR (genie_curses_green, 3, COLOR_GREEN, COLOR_BLACK)
  GENIE_COLOUR (genie_curses_magenta, 4, COLOR_MAGENTA, COLOR_BLACK)
  GENIE_COLOUR (genie_curses_red, 5, COLOR_RED, COLOR_BLACK)
  GENIE_COLOUR (genie_curses_white, 6, COLOR_WHITE, COLOR_BLACK)
  GENIE_COLOUR (genie_curses_yellow, 7, COLOR_YELLOW, COLOR_BLACK)
//! @brief PROC curses delchar = (CHAR) BOOL
     void genie_curses_del_char (NODE_T * p)
{
  A68_CHAR ch;
  int v;
  POP_OBJECT (p, &ch, A68_CHAR);
  v = (int) VALUE (&ch);
  PUSH_VALUE (p, (BOOL_T) (v == 8 || v == 127 || v == KEY_BACKSPACE), A68_BOOL);
}

//! @brief PROC curses putchar = (CHAR) VOID

void genie_curses_putchar (NODE_T * p)
{
  A68_CHAR ch;
  if (A68 (curses_mode) == A68_FALSE) {
    genie_curses_start (p);
  }
  POP_OBJECT (p, &ch, A68_CHAR);
  (void) (addch ((chtype) (VALUE (&ch))));
}

//! @brief PROC curses move = (INT, INT) VOID

void genie_curses_move (NODE_T * p)
{
  A68_INT i, j;
  if (A68 (curses_mode) == A68_FALSE) {
    genie_curses_start (p);
  }
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_INT);
  if (VALUE (&i) < 0 || VALUE (&i) >= LINES) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_CURSES_OFF_SCREEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (VALUE (&j) < 0 || VALUE (&j) >= COLS) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_CURSES_OFF_SCREEN);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  CHECK_CURSES_RETVAL (move (VALUE (&i), VALUE (&j)) != ERR);
}

#endif
