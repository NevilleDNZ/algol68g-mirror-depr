//! @file a68g-options.h
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

#if !defined (__A68G_OPTIONS_H__)
#define __A68G_OPTIONS_H__ 

extern BOOL_T set_options (OPTION_LIST_T *, BOOL_T);
extern char *optimisation_option (void);
extern void add_option_list (OPTION_LIST_T **, char *, LINE_T *);
extern void free_option_list (OPTION_LIST_T *);
extern void default_options (MODULE_T *);
extern void init_options (void);
extern void isolate_options (char *, LINE_T *);
extern void read_env_options (void);
extern void read_rc_options (void);

#endif
