//! @file a68g-masks.h
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2021 J. Marcel van der Veer <algol68g@xs4all.nl>.
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

#if !defined (__A68G_MASKS_H__)
#define __A68G_MASKS_H__

// Status Masks
#define NULL_MASK                 ((STATUS_MASK_T) 0x00000000)
#define IN_HEAP_MASK              ((STATUS_MASK_T) 0x00000001)
#define IN_FRAME_MASK             ((STATUS_MASK_T) 0x00000002)
#define IN_STACK_MASK             ((STATUS_MASK_T) 0x00000004)
#define IN_COMMON_MASK            ((STATUS_MASK_T) 0x00000008)
#define INIT_MASK                 ((STATUS_MASK_T) 0x00000010)
#define PLUS_INF_MASK             ((STATUS_MASK_T) 0x00000020)
#define MINUS_INF_MASK            ((STATUS_MASK_T) 0x00000040)
#define CONSTANT_MASK             ((STATUS_MASK_T) 0x00000080)
#define BLOCK_GC_MASK             ((STATUS_MASK_T) 0x00000100)
#define COOKIE_MASK               ((STATUS_MASK_T) 0x00000200)
#define SCOPE_ERROR_MASK          ((STATUS_MASK_T) 0x00000200)
#define ALLOCATED_MASK            ((STATUS_MASK_T) 0x00000400)
#define STANDENV_PROC_MASK        ((STATUS_MASK_T) 0x00000800)
#define COLOUR_MASK               ((STATUS_MASK_T) 0x00001000)
#define MODULAR_MASK              ((STATUS_MASK_T) 0x00002000)
#define OPTIMAL_MASK              ((STATUS_MASK_T) 0x00004000)
#define SERIAL_MASK               ((STATUS_MASK_T) 0x00008000)
#define CROSS_REFERENCE_MASK      ((STATUS_MASK_T) 0x00010000)
#define TREE_MASK                 ((STATUS_MASK_T) 0x00020000)
#define CODE_MASK                 ((STATUS_MASK_T) 0x00040000)
#define NOT_NEEDED_MASK           ((STATUS_MASK_T) 0x00080000)
#define SOURCE_MASK               ((STATUS_MASK_T) 0x00100000)
#define ASSERT_MASK               ((STATUS_MASK_T) 0x00200000)
#define NIL_MASK                  ((STATUS_MASK_T) 0x00400000)
#define SKIP_PROCEDURE_MASK       ((STATUS_MASK_T) 0x00800000)
#define SKIP_FORMAT_MASK          ((STATUS_MASK_T) 0x00800000)
#define SKIP_ROW_MASK	          ((STATUS_MASK_T) 0x00800000)
#define INTERRUPTIBLE_MASK        ((STATUS_MASK_T) 0x01000000)
#define BREAKPOINT_MASK           ((STATUS_MASK_T) 0x02000000)
#define BREAKPOINT_TEMPORARY_MASK ((STATUS_MASK_T) 0x04000000)
#define BREAKPOINT_INTERRUPT_MASK ((STATUS_MASK_T) 0x08000000)
#define BREAKPOINT_WATCH_MASK     ((STATUS_MASK_T) 0x10000000)
#define BREAKPOINT_TRACE_MASK     ((STATUS_MASK_T) 0x20000000)
#define SEQUENCE_MASK             ((STATUS_MASK_T) 0x40000000)
#define BREAKPOINT_ERROR_MASK     ((STATUS_MASK_T) 0xffffffff)

// CODEX masks

#define PROC_DECLARATION_MASK ((STATUS_MASK_T) 0x00000001)

#endif
