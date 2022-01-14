//! @file a68g-nil.h
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

#if !defined (__A68G_NIL_H__)
#define __A68G_NIL_H__

// Various forms of NIL.

#define NO_A68_REF ((A68_REF *) NULL)
#define NO_ARRAY ((A68_ARRAY *) NULL)
#define NO_BOOK ((BOOK_T *) NULL)
#define NO_BOOL ((BOOL_T *) NULL)
#define NO_BYTE ((BYTE_T *) NULL)
#define NO_CONSTANT ((void *) NULL)
#define NO_DEC ((DEC_T *) NULL)
#define NO_DIAGNOSTIC ((DIAGNOSTIC_T *) NULL)
#define NO_EDLIN ((EDLIN_T *) NULL)
#define NO_FILE ((FILE *) NULL)
#define NO_FORMAT ((A68_FORMAT *) NULL)
#define NO_GINFO ((GINFO_T *) NULL)
#define NO_GPROC ((void (*) (NODE_T *)) NULL)
#define NO_HANDLE ((A68_HANDLE *) NULL)
#define NO_INT ((int *) NULL)
#define NO_JMP_BUF ((jmp_buf *) NULL)
#define NO_KEYWORD ((KEYWORD_T *) NULL)
#define NO_NINFO ((NODE_INFO_T *) NULL)
#define NO_NOTE ((void (*) (NODE_T *)) NULL)
#define NO_OPTION_LIST ((OPTION_LIST_T *) NULL)
#define NO_PACK ((PACK_T *) NULL)
#define NO_POSTULATE ((POSTULATE_T *) NULL)
#define NO_PPROC ((PROP_T (*) (NODE_T *)) NULL)
#define NO_PROCEDURE ((A68_PROCEDURE *) NULL)
#define NO_REAL ((REAL_T *) NULL)
#define NO_REFINEMENT ((REFINEMENT_T *) NULL)
#define NO_REGMATCH ((regmatch_t *) NULL)
#define NO_SCOPE ((SCOPE_T *) NULL)
#define NO_SOID ((SOID_T *) NULL)
#define NO_SOUND ((A68_SOUND *) NULL)
#define NO_STREAM NO_FILE
#define NO_TEXT ((char *) NULL)
#define NO_TICK ((BOOL_T *) NULL)
#define NO_TOKEN ((TOKEN_T *) NULL)
#define NO_TUPLE ((A68_TUPLE *) NULL)
#define NO_VAR (NULL)

static const A68_FORMAT nil_format = {
  INIT_MASK, NULL, 0
};

static const A68_HANDLE nil_handle = {
  INIT_MASK,
  NO_BYTE,
  0,
  NO_MOID,
  NO_HANDLE,
  NO_HANDLE
};

static const A68_REF nil_ref = {
  (STATUS_MASK_T) (INIT_MASK | NIL_MASK),
  0,
  0,
  NO_HANDLE
};

#endif
