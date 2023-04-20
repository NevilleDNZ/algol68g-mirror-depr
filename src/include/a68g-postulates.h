//! @file a68g-postulates.h
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

#if !defined (__A68G_POSTULATES_H__)
#define __A68G_POSTULATES_H__

extern void init_postulates (void);
extern void free_postulate_list (POSTULATE_T *, POSTULATE_T *);
extern void make_postulate (POSTULATE_T **, MOID_T *, MOID_T *);
extern POSTULATE_T *is_postulated (POSTULATE_T *, MOID_T *);
extern POSTULATE_T *is_postulated_pair (POSTULATE_T *, MOID_T *, MOID_T *);

#endif
