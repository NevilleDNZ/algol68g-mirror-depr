/*!
\file environ.h
\brief contains prelude definitions
**/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#if ! defined A68G_ENVIRON_H
#define A68G_ENVIRON_H

char bold_prelude_start[] = "\
BEGIN MODE DOUBLE = LONG REAL;!\
      start: commence:!\
      BEGIN!";

char bold_postlude[] = "\
      END;!\
      stop: abort: halt: SKIP!\
END!";

char quote_prelude_start[] = "\
'BEGIN' 'MODE' 'DOUBLE' = 'LONG' 'REAL',!\
               'QUAD' = 'LONG' 'LONG' 'REAL',!\
               'DEVICE' = 'FILE',!\
               'TEXT' = 'STRING';!\
        START: COMMENCE:!\
        'BEGIN'!";

char quote_postlude[] = "\
        'END';!\
        STOP: ABORT: HALT: 'SKIP'!\
'END'!";

#endif
