//! @file parser-victal.c
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
//! Syntax check for formal, actual and virtual declarers.

#include "a68g.h"
#include "a68g-parser.h"

BOOL_T victal_check_declarer (NODE_T *, int);

//! @brief Check generator.

void victal_check_generator (NODE_T * p)
{
  if (!victal_check_declarer (NEXT (p), ACTUAL_DECLARER_MARK)) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
  }
}

//! @brief Check formal pack.

void victal_check_formal_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NO_NODE) {
    if (IS (p, FORMAL_DECLARERS)) {
      victal_check_formal_pack (SUB (p), x, z);
    } else if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_formal_pack (NEXT (p), x, z);
    } else if (IS (p, FORMAL_DECLARERS_LIST)) {
      victal_check_formal_pack (NEXT (p), x, z);
      victal_check_formal_pack (SUB (p), x, z);
    } else if (IS (p, DECLARER)) {
      victal_check_formal_pack (NEXT (p), x, z);
      (*z) &= victal_check_declarer (SUB (p), x);
    }
  }
}

//! @brief Check operator declaration.

void victal_check_operator_dec (NODE_T * p)
{
  if (IS (NEXT (p), FORMAL_DECLARERS)) {
    BOOL_T z = A68_TRUE;
    victal_check_formal_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarers");
    }
    FORWARD (p);
  }
  if (!victal_check_declarer (NEXT (p), FORMAL_DECLARER_MARK)) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
  }
}

//! @brief Check mode declaration.

void victal_check_mode_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
      victal_check_mode_dec (NEXT (p));
    } else if (is_one_of (p, MODE_SYMBOL, DEFINING_INDICANT, STOP)
               || is_one_of (p, EQUALS_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_mode_dec (NEXT (p));
    } else if (IS (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
    }
  }
}

//! @brief Check variable declaration.

void victal_check_variable_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
      victal_check_variable_dec (NEXT (p));
    } else if (is_one_of (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, STOP)
               || IS (p, COMMA_SYMBOL)) {
      victal_check_variable_dec (NEXT (p));
    } else if (IS (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (IS (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
      victal_check_variable_dec (NEXT (p));
    }
  }
}

//! @brief Check identity declaration.

void victal_check_identity_dec (NODE_T * p)
{
  if (p != NO_NODE) {
    if (IS (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
      victal_check_identity_dec (NEXT (p));
    } else if (is_one_of (p, DEFINING_IDENTIFIER, EQUALS_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_identity_dec (NEXT (p));
    } else if (IS (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (IS (p, DECLARER)) {
      if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      victal_check_identity_dec (NEXT (p));
    }
  }
}

//! @brief Check routine pack.

void victal_check_routine_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NO_NODE) {
    if (IS (p, PARAMETER_PACK)) {
      victal_check_routine_pack (SUB (p), x, z);
    } else if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_routine_pack (NEXT (p), x, z);
    } else if (is_one_of (p, PARAMETER_LIST, PARAMETER, STOP)) {
      victal_check_routine_pack (NEXT (p), x, z);
      victal_check_routine_pack (SUB (p), x, z);
    } else if (IS (p, DECLARER)) {
      *z &= victal_check_declarer (SUB (p), x);
    }
  }
}

//! @brief Check routine text.

void victal_check_routine_text (NODE_T * p)
{
  if (IS (p, PARAMETER_PACK)) {
    BOOL_T z = A68_TRUE;
    victal_check_routine_pack (p, FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarers");
    }
    FORWARD (p);
  }
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
  }
  victal_checker (NEXT (p));
}

//! @brief Check structure pack.

void victal_check_structure_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NO_NODE) {
    if (IS (p, STRUCTURE_PACK)) {
      victal_check_structure_pack (SUB (p), x, z);
    } else if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, STOP)) {
      victal_check_structure_pack (NEXT (p), x, z);
    } else if (is_one_of (p, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, STOP)) {
      victal_check_structure_pack (NEXT (p), x, z);
      victal_check_structure_pack (SUB (p), x, z);
    } else if (IS (p, DECLARER)) {
      (*z) &= victal_check_declarer (SUB (p), x);
    }
  }
}

//! @brief Check union pack.

void victal_check_union_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NO_NODE) {
    if (IS (p, UNION_PACK)) {
      victal_check_union_pack (SUB (p), x, z);
    } else if (is_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, VOID_SYMBOL, STOP)) {
      victal_check_union_pack (NEXT (p), x, z);
    } else if (IS (p, UNION_DECLARER_LIST)) {
      victal_check_union_pack (NEXT (p), x, z);
      victal_check_union_pack (SUB (p), x, z);
    } else if (IS (p, DECLARER)) {
      victal_check_union_pack (NEXT (p), x, z);
      (*z) &= victal_check_declarer (SUB (p), FORMAL_DECLARER_MARK);
    }
  }
}

//! @brief Check declarer.

BOOL_T victal_check_declarer (NODE_T * p, int x)
{
  if (p == NO_NODE) {
    return A68_FALSE;
  } else if (IS (p, DECLARER)) {
    return victal_check_declarer (SUB (p), x);
  } else if (is_one_of (p, LONGETY, SHORTETY, STOP)) {
    return A68_TRUE;
  } else if (is_one_of (p, VOID_SYMBOL, INDICANT, STANDARD, STOP)) {
    return A68_TRUE;
  } else if (IS_REF (p)) {
    return victal_check_declarer (NEXT (p), VIRTUAL_DECLARER_MARK);
  } else if (IS_FLEX (p)) {
    return victal_check_declarer (NEXT (p), x);
  } else if (IS (p, BOUNDS)) {
    victal_checker (SUB (p));
    if (x == FORMAL_DECLARER_MARK) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return A68_TRUE;
    } else if (x == VIRTUAL_DECLARER_MARK) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "virtual bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return A68_TRUE;
    } else {
      return victal_check_declarer (NEXT (p), x);
    }
  } else if (IS (p, FORMAL_BOUNDS)) {
    victal_checker (SUB (p));
    if (x == ACTUAL_DECLARER_MARK) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return A68_TRUE;
    } else {
      return victal_check_declarer (NEXT (p), x);
    }
  } else if (IS (p, STRUCT_SYMBOL)) {
    BOOL_T z = A68_TRUE;
    victal_check_structure_pack (NEXT (p), x, &z);
    return z;
  } else if (IS (p, UNION_SYMBOL)) {
    BOOL_T z = A68_TRUE;
    victal_check_union_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer pack");
    }
    return A68_TRUE;
  } else if (IS (p, PROC_SYMBOL)) {
    if (IS (NEXT (p), FORMAL_DECLARERS)) {
      BOOL_T z = A68_TRUE;
      victal_check_formal_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
      if (!z) {
        diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      FORWARD (p);
    }
    if (!victal_check_declarer (NEXT (p), FORMAL_DECLARER_MARK)) {
      diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    }
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief Check cast.

void victal_check_cast (NODE_T * p)
{
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    victal_checker (NEXT (p));
  }
}

//! @brief Driver for checking VICTALITY of declarers.

void victal_checker (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
    } else if (IS (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
    } else if (IS (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
    } else if (IS (p, GENERATOR)) {
      victal_check_generator (SUB (p));
    } else if (IS (p, ROUTINE_TEXT)) {
      victal_check_routine_text (SUB (p));
    } else if (IS (p, OPERATOR_PLAN)) {
      victal_check_operator_dec (SUB (p));
    } else if (IS (p, CAST)) {
      victal_check_cast (SUB (p));
    } else {
      victal_checker (SUB (p));
    }
  }
}
