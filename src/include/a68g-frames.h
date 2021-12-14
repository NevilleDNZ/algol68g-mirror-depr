//! @file a68g-frames.h
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

#if !defined (__A68G_FRAMES_H__)
#define __A68G_FRAMES_H__

// Operations on stack frames

#define FRAME_ADDRESS(n) ((BYTE_T *) &(A68_STACK[n]))
#define FACT(n) ((ACTIVATION_RECORD *) FRAME_ADDRESS (n))
#define FRAME_CLEAR(m) FILL ((BYTE_T *) FRAME_OFFSET (FRAME_INFO_SIZE), 0, (m))
#define FRAME_BLOCKS(n) (BLOCKS (FACT (n)))
#define FRAME_DYNAMIC_LINK(n) (DYNAMIC_LINK (FACT (n)))
#define FRAME_DNS(n) (DYNAMIC_SCOPE (FACT (n)))
#define FRAME_INCREMENT(n) (AP_INCREMENT (TABLE (FRAME_TREE(n))))
#define FRAME_INFO_SIZE (A68_FRAME_ALIGN (sizeof (ACTIVATION_RECORD)))
#define FRAME_JUMP_STAT(n) (JUMP_STAT (FACT (n)))
#define FRAME_LEXICAL_LEVEL(n) (FRAME_LEVEL (FACT (n)))
#define FRAME_LOCAL(n, m) (FRAME_ADDRESS ((n) + FRAME_INFO_SIZE + (m)))
#define FRAME_NUMBER(n) (FRAME_NO (FACT (n)))
#define FRAME_OBJECT(n) (FRAME_OFFSET (FRAME_INFO_SIZE + (n)))
#define FRAME_OFFSET(n) (FRAME_ADDRESS (A68_FP + (n)))
#define FRAME_PARAMETER_LEVEL(n) (PARAMETER_LEVEL (FACT (n)))
#define FRAME_PARAMETERS(n) (PARAMETERS (FACT (n)))
#define FRAME_PROC_FRAME(n) (PROC_FRAME (FACT (n)))
#define FRAME_SIZE(fp) (FRAME_INFO_SIZE + FRAME_INCREMENT (fp))
#define FRAME_STATIC_LINK(n) (STATIC_LINK (FACT (n)))
#define FRAME_TREE(n) (NODE (FACT (n)))

#if defined (BUILD_PARALLEL_CLAUSE)
#define FRAME_THREAD_ID(n) (THREAD_ID (FACT (n)))
#endif

#define FOLLOW_SL(dest, l) {\
  (dest) = A68_FP;\
  if ((l) <= FRAME_PARAMETER_LEVEL ((dest))) {\
    (dest) = FRAME_PARAMETERS ((dest));\
  }\
  while ((l) != FRAME_LEXICAL_LEVEL ((dest))) {\
    (dest) = FRAME_STATIC_LINK ((dest));\
  }}

#define FOLLOW_STATIC_LINK(dest, l) {\
  if ((l) == A68 (global_level) && A68_GLOBALS > 0) {\
    (dest) = A68_GLOBALS;\
  } else {\
    FOLLOW_SL (dest, l)\
  }}

#define FRAME_GET(dest, cast, p) {\
  ADDR_T _m_z;\
  FOLLOW_STATIC_LINK (_m_z, LEVEL (GINFO (p)));\
  (dest) = (cast *) & (OFFSET (GINFO (p))[_m_z]);\
  }

#define GET_FRAME(dest, cast, level, offset) {\
  ADDR_T _m_z;\
  FOLLOW_SL (_m_z, (level));\
  (dest) = (cast *) & (A68_STACK [_m_z + FRAME_INFO_SIZE + (offset)]);\
  }

#define GET_GLOBAL(dest, cast, offset) {\
  (dest) = (cast *) & (A68_STACK [A68_GLOBALS + FRAME_INFO_SIZE + (offset)]);\
  }

// Opening of stack frames is in-line

// 
// STATIC_LINK_FOR_FRAME: determine static link for stack frame.
// new_lex_lvl: lexical level of new stack frame.
// returns: static link for stack frame at 'new_lex_lvl'. 

#define STATIC_LINK_FOR_FRAME(dest, new_lex_lvl) {\
  int _m_cur_lex_lvl = FRAME_LEXICAL_LEVEL (A68_FP);\
  if (_m_cur_lex_lvl == (new_lex_lvl)) {\
    (dest) = FRAME_STATIC_LINK (A68_FP);\
  } else if (_m_cur_lex_lvl > (new_lex_lvl)) {\
    ADDR_T _m_static_link = A68_FP;\
    while (FRAME_LEXICAL_LEVEL (_m_static_link) >= (new_lex_lvl)) {\
      _m_static_link = FRAME_STATIC_LINK (_m_static_link);\
    }\
    (dest) = _m_static_link;\
  } else {\
    (dest) = A68_FP;\
  }}

#define INIT_STATIC_FRAME(p) {\
  FRAME_CLEAR (AP_INCREMENT (TABLE (p)));\
  if (INITIALISE_FRAME (TABLE (p))) {\
    initialise_frame (p);\
  }}

#define INIT_GLOBAL_POINTER(p) {\
  if (LEX_LEVEL (p) == A68 (global_level)) {\
    A68_GLOBALS = A68_FP;\
  }}

#if defined (BUILD_PARALLEL_CLAUSE)
#define OPEN_STATIC_FRAME(p) {\
  ADDR_T dynamic_link = A68_FP, static_link;\
  ACTIVATION_RECORD *act, *pre;\
  STATIC_LINK_FOR_FRAME (static_link, LEX_LEVEL (p));\
  pre = FACT (A68_FP);\
  A68_FP += FRAME_SIZE (dynamic_link);\
  act = FACT (A68_FP);\
  FRAME_NO (act) = FRAME_NO (pre) + 1;\
  FRAME_LEVEL (act) = LEX_LEVEL (p);\
  PARAMETER_LEVEL (act) = PARAMETER_LEVEL (pre);\
  PARAMETERS (act) = PARAMETERS (pre);\
  STATIC_LINK (act) = static_link;\
  DYNAMIC_LINK (act) = dynamic_link;\
  DYNAMIC_SCOPE (act) = A68_FP;\
  NODE (act) = p;\
  JUMP_STAT (act) = NO_JMP_BUF;\
  PROC_FRAME (act) = A68_FALSE;\
  THREAD_ID (act) = pthread_self ();\
  }
#else
#define OPEN_STATIC_FRAME(p) {\
  ADDR_T dynamic_link = A68_FP, static_link;\
  ACTIVATION_RECORD *act, *pre;\
  STATIC_LINK_FOR_FRAME (static_link, LEX_LEVEL (p));\
  pre = FACT (A68_FP);\
  A68_FP += FRAME_SIZE (dynamic_link);\
  act = FACT (A68_FP);\
  FRAME_NO (act) = FRAME_NO (pre) + 1;\
  FRAME_LEVEL (act) = LEX_LEVEL (p);\
  PARAMETER_LEVEL (act) = PARAMETER_LEVEL (pre);\
  PARAMETERS (act) = PARAMETERS (pre);\
  STATIC_LINK (act) = static_link;\
  DYNAMIC_LINK (act) = dynamic_link;\
  DYNAMIC_SCOPE (act) = A68_FP;\
  NODE (act) = p;\
  JUMP_STAT (act) = NO_JMP_BUF;\
  PROC_FRAME (act) = A68_FALSE;\
  }
#endif

/**
@def OPEN_PROC_FRAME
@brief Open a stack frame for a procedure.
**/

#if defined (BUILD_PARALLEL_CLAUSE)
#define OPEN_PROC_FRAME(p, environ) {\
  ADDR_T dynamic_link = A68_FP, static_link;\
  ACTIVATION_RECORD *act;\
  LOW_STACK_ALERT (p);\
  static_link = (environ > 0 ? environ : A68_FP);\
  if (A68_FP < static_link) {\
    diagnostic (A68_RUNTIME_ERROR, (p), ERROR_SCOPE_DYNAMIC_0);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  A68_FP += FRAME_SIZE (dynamic_link);\
  act = FACT (A68_FP);\
  FRAME_NO (act) = FRAME_NUMBER (dynamic_link) + 1;\
  FRAME_LEVEL (act) = LEX_LEVEL (p);\
  PARAMETER_LEVEL (act) = LEX_LEVEL (p);\
  PARAMETERS (act) = A68_FP;\
  STATIC_LINK (act) = static_link;\
  DYNAMIC_LINK (act) = dynamic_link;\
  DYNAMIC_SCOPE (act) = A68_FP;\
  NODE (act) = p;\
  JUMP_STAT (act) = NO_JMP_BUF;\
  PROC_FRAME (act) = A68_TRUE;\
  THREAD_ID (act) = pthread_self ();\
  }
#else
#define OPEN_PROC_FRAME(p, environ) {\
  ADDR_T dynamic_link = A68_FP, static_link;\
  ACTIVATION_RECORD *act;\
  LOW_STACK_ALERT (p);\
  static_link = (environ > 0 ? environ : A68_FP);\
  if (A68_FP < static_link) {\
    diagnostic (A68_RUNTIME_ERROR, (p), ERROR_SCOPE_DYNAMIC_0);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  A68_FP += FRAME_SIZE (dynamic_link);\
  act = FACT (A68_FP);\
  FRAME_NO (act) = FRAME_NUMBER (dynamic_link) + 1;\
  FRAME_LEVEL (act) = LEX_LEVEL (p);\
  PARAMETER_LEVEL (act) = LEX_LEVEL (p);\
  PARAMETERS (act) = A68_FP;\
  STATIC_LINK (act) = static_link;\
  DYNAMIC_LINK (act) = dynamic_link;\
  DYNAMIC_SCOPE (act) = A68_FP;\
  NODE (act) = p;\
  JUMP_STAT (act) = NO_JMP_BUF;\
  PROC_FRAME (act) = A68_TRUE;\
  }
#endif

#define CLOSE_FRAME {\
  ACTIVATION_RECORD *act = FACT (A68_FP);\
  A68_FP = DYNAMIC_LINK (act);\
  }

#endif
