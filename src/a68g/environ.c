//! @file environ.c
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
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-physics.h"
#include "a68g-numbers.h"
#include "a68g-optimiser.h"
#include "a68g-double.h"
#include "a68g-transput.h"

#define VECTOR_SIZE 512
#define FD_READ 0
#define FD_WRITE 1

//! @brief PROC (PROC VOID) VOID on gc event

void genie_on_gc_event (NODE_T * p)
{
  POP_PROCEDURE (p, &A68 (on_gc_event));
}

//! @brief Generic procedure for OP AND BECOMES (+:=, -:=, ...).

void genie_f_and_becomes (NODE_T * p, MOID_T * ref, GPROC * f)
{
  MOID_T *mode = SUB (ref);
  int size = SIZE (mode);
  BYTE_T *src = STACK_OFFSET (-size), *addr;
  A68_REF *dst = (A68_REF *) STACK_OFFSET (-size - A68_REF_SIZE);
  CHECK_REF (p, *dst, ref);
  addr = ADDRESS (dst);
  PUSH (p, addr, size);
  genie_check_initialisation (p, STACK_OFFSET (-size), mode);
  PUSH (p, src, size);
  (*f) (p);
  POP (p, addr, size);
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief INT system heap pointer

void genie_system_heap_pointer (NODE_T * p)
{
  PUSH_VALUE (p, (int) (A68_HP), A68_INT);
}

//! @brief INT system stack pointer

void genie_system_stack_pointer (NODE_T * p)
{
  BYTE_T stack_offset;
  PUSH_VALUE (p, (int) (A68 (system_stack_offset) - &stack_offset), A68_INT);
}
