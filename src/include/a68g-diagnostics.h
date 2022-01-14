//! @file a68g-diagnostics.h
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

#if !defined (A68G_DIAGNOSTICS_H)
#define A68G_DIAGNOSTICS_H

extern char *error_specification (void);
extern void diagnostic (STATUS_MASK_T, NODE_T *, char *, ...);
extern void diagnostics_to_terminal (LINE_T *, int);
extern void scan_error (LINE_T *, char *, char *);
extern void write_source_line (FILE_T, LINE_T *, NODE_T *, int);

// Error codes

#define A68_NO_DIAGNOSTICS    ((STATUS_MASK_T) 0x0)
#define A68_ERROR             ((STATUS_MASK_T) 0x1)
#define A68_SYNTAX_ERROR      ((STATUS_MASK_T) 0x2)
#define A68_MATH_ERROR        ((STATUS_MASK_T) 0x4)
#define A68_MATH_WARNING      ((STATUS_MASK_T) 0x8)
#define A68_WARNING           ((STATUS_MASK_T) 0x10)
#define A68_RUNTIME_ERROR     ((STATUS_MASK_T) 0x20)
#define A68_SUPPRESS_SEVERITY ((STATUS_MASK_T) 0x40)
#define A68_ALL_DIAGNOSTICS   ((STATUS_MASK_T) 0x80)
#define A68_RERUN             ((STATUS_MASK_T) 0x100)
#define A68_FORCE_DIAGNOSTICS ((STATUS_MASK_T) 0x200)
#define A68_FORCE_QUIT        ((STATUS_MASK_T) 0x400)
#define A68_NO_SYNTHESIS      ((STATUS_MASK_T) 0x800)

// Diagnostic texts

#define ERROR_ACCESSING_NIL "attempt to access N"
#define ERROR_ACTION "action failed"
#define ERROR_ALIGNMENT "alignment error"
#define ERROR_ALLOCATION "allocation error"
#define ERROR_ARGUMENT_NUMBER "incorrect number of arguments for M"
#define ERROR_ASSERTION "assertion failure"
#define ERROR_CANNOT_OPEN_NAME "cannot open Z"
#define ERROR_CANNOT_WIDEN "cannot widen M to M"
#define ERROR_CANNOT_WRITE_LISTING "cannot write listing file"
#define ERROR_CHANNEL_DOES_NOT_ALLOW "channel does not allow Y"
#define ERROR_CLAUSE_WITHOUT_VALUE "clause does not yield a value"
#define ERROR_CLOSING_DEVICE "error while closing device"
#define ERROR_CLOSING_FILE "error while closing file"
#define ERROR_CODE "clause should be compiled"
#define ERROR_COMMA_MUST_SEPARATE "A and A must be separated by a comma-symbol"
#define ERROR_COMPONENT_NUMBER "M must have at least two components"
#define ERROR_COMPONENT_RELATED "M has firmly related components"
#define ERROR_CURSES "error in curses operation"
#define ERROR_CURSES_OFF_SCREEN "curses operation moves cursor off the screen"
#define ERROR_DEVICE_ALREADY_SET "device parameters already set"
#define ERROR_DEVICE_CANNOT_ALLOCATE "cannot allocate device parameters"
#define ERROR_DEVICE_CANNOT_OPEN "cannot open device"
#define ERROR_DEVICE_NOT_OPEN "device is not open"
#define ERROR_DEVICE_NOT_SET "device parameters not set"
#define ERROR_DIFFERENT_BOUNDS "rows have different bounds"
#define ERROR_DIVISION_BY_ZERO "attempt at M division by zero"
#define ERROR_DYADIC_PRIORITY "dyadic S has no priority declaration"
#define ERROR_EMPTY_ARGUMENT "empty argument"
#define ERROR_EMPTY_VALUE "attempt to use an uninitialised M value"
#define ERROR_EMPTY_VALUE_FROM (ERROR_EMPTY_VALUE)
#define ERROR_EXPECTED "Y expected"
#define ERROR_EXPECTED_NEAR "B expected in A, near Z L"
#define ERROR_EXPONENT_DIGIT "invalid exponent digit"
#define ERROR_EXPONENT_INVALID "invalid M exponent"
#define ERROR_FALSE_ASSERTION "false assertion"
#define ERROR_FFT "fourier transform error; Y; Y"
#define ERROR_FILE_ACCESS "file access error"
#define ERROR_FILE_ALREADY_OPEN "file is already open"
#define ERROR_FILE_CANNOT_OPEN_FOR "cannot open Z for Y"
#define ERROR_FILE_CANT_RESET "cannot reset file"
#define ERROR_FILE_CANT_SET "cannot set file"
#define ERROR_FILE_CLOSE "error while closing file"
#define ERROR_FILE_ENDED "end of file reached"
#define ERROR_FILE_INCLUDE_CTRL "control characters in include file"
#define ERROR_FILE_LOCK "error while locking file"
#define ERROR_FILE_NOT_OPEN "file is not open"
#define ERROR_FILE_NO_TEMP "cannot create unique temporary file name"
#define ERROR_FILE_READ "error while reading file"
#define ERROR_FILE_RESET "error while resetting file"
#define ERROR_FILE_SCRATCH "error while scratching file"
#define ERROR_FILE_SET "error while setting file"
#define ERROR_FILE_SOURCE_CTRL "control characters in source file"
#define ERROR_FILE_TRANSPUT "error transputting M value"
#define ERROR_FILE_TRANSPUT_SIGN "error transputting sign in M value"
#define ERROR_FILE_WRONG_MOOD "file is in Y mood"
#define ERROR_FORMAT_CANNOT_TRANSPUT "cannot transput M value with A"
#define ERROR_FORMAT_EXHAUSTED "patterns exhausted in format"
#define ERROR_FORMAT_INTS_REQUIRED "1 .. 3 M arguments required"
#define ERROR_FORMAT_INVALID_REPLICATOR "negative replicator"
#define ERROR_FORMAT_PICTURES "number of pictures does not match number of arguments"
#define ERROR_FORMAT_PICTURE_NUMBER "incorrect number of pictures for A"
#define ERROR_FORMAT_UNDEFINED "cannot use undefined format"
#define ERROR_INCORRECT_FILENAME "incorrect filename"
#define ERROR_INDEXER_NUMBER "incorrect number of indexers for M"
#define ERROR_INDEX_OUT_OF_BOUNDS "index out of bounds"
#define ERROR_INFINITE "infinite M value"
#define ERROR_INTERNAL_CONSISTENCY "internal consistency check failure"
#define ERROR_INVALID_ARGUMENT "invalid M argument"
#define ERROR_INVALID_DIMENSION "invalid dimension D"
#define ERROR_INVALID_OPERAND "M construct is an invalid operand"
#define ERROR_INVALID_OPERATOR_TAG "invalid operator tag"
#define ERROR_INVALID_PARAMETER "invalid parameter (U Z)"
#define ERROR_INVALID_PRIORITY "invalid priority declaration"
#define ERROR_INVALID_RADIX "invalid radix D"
#define ERROR_INVALID_SEQUENCE "Y is not a valid A"
#define ERROR_INVALID_SIZE "object of invalid size"
#define ERROR_INVALID_VALUE "invalid value"
#define ERROR_IN_DENOTATION "error in M denotation"
#define ERROR_KEYWORD "check for missing or unmatched keyword in clause starting at S"
#define ERROR_LABELED_UNIT_MUST_FOLLOW "S must be followed by a labeled unit"
#define ERROR_LABEL_BEFORE_DECLARATION "declaration cannot follow a labeled unit"
#define ERROR_LAPLACE "laplace transform error; Y; Y"
#define ERROR_LONG_STRING "string exceeds end of line"
#define ERROR_MATH "M math error"
#define ERROR_MATH_CHEBYSHEV "error while evaluating Chebyshev series"
#define ERROR_MATH_CONVERGENCE "no convergence"
#define ERROR_MATH_EXCEPTION "math exception"
#define ERROR_MATH_EXP "exp argument out of range"
#define ERROR_MODE_SPECIFICATION "M construct must yield a routine, row or structured value"
#define ERROR_MP_OUT_OF_BOUNDS "multiprecision value out of bounds"
#define ERROR_MULTIPLE_FIELD "multiple declaration of field S"
#define ERROR_MULTIPLE_TAG "multiple declaration of tag S"
#define ERROR_NOT_WELL_FORMED "M does not specify a well formed mode"
#define ERROR_NO_COMPONENT "M is neither component nor subset of M"
#define ERROR_NO_DYADIC "dyadic operator O S O has not been declared"
#define ERROR_NO_FIELD "M has no field Z"
#define ERROR_NO_FLEX_ARGUMENT "M value from A cannot be flexible"
#define ERROR_NO_MATRIX "M A does not yield a two-dimensional row"
#define ERROR_NO_MONADIC "monadic operator S O has not been declared"
#define ERROR_NO_NAME "M A does not yield a name"
#define ERROR_NO_NAME_REQUIRED "context does not require a name"
#define ERROR_NO_PARALLEL_CLAUSE "interpreter was compiled without support for the parallel-clause"
#define ERROR_NO_PRIORITY "S has no priority declaration"
#define ERROR_IS_DIRECTORY "source file cannot be a directory"
#define ERROR_NO_REGULAR_FILE "source is not a regular file"
#define ERROR_NO_ROW_OR_PROC "M A does not yield a row or procedure"
#define ERROR_NO_SOURCE_FILE "no source file specified"
#define ERROR_NO_SQUARE_MATRIX "M matrix is not square"
#define ERROR_NO_STRUCT "M A does not yield a structured value"
#define ERROR_NO_UNION "M is not a united mode"
#define ERROR_NO_UNIQUE_MODE "construct has no unique mode"
#define ERROR_NO_VECTOR "M A does not yield a one-dimensional row"
#define ERROR_DEPRECATED "M in this construct is deprecated in V"
#define ERROR_OPERAND_NUMBER "incorrect number of operands for S"
#define ERROR_OPERATOR_INVALID "monadic S cannot start with a character from Z"
#define ERROR_OPERATOR_INVALID_END "probably a missing symbol near invalid operator S"
#define ERROR_OPERATOR_RELATED "M Z is firmly related to M Z"
#define ERROR_OUT_OF_BOUNDS "M value out of bounds"
#define ERROR_OUT_OF_CORE "insufficient memory"
#define ERROR_PAGE_SIZE "error in page size"
#define ERROR_PARALLEL_JUMP "jump into different thread"
#define ERROR_PARALLEL_CANNOT_CREATE "cannot create thread"
#define ERROR_PARALLEL_OUTSIDE "invalid outside a parallel clause"
#define ERROR_PARALLEL_OVERFLOW "too many parallel units (Y)"
#define ERROR_PARENTHESIS "incorrect nesting, check for Y"
#define ERROR_PARENTHESIS_2 "encountered X L but expected X, check for Y"
#define ERROR_PRAGMENT "error in pragment"
#define ERROR_HIGH_PRECISION "unsupported precision"
#define ERROR_QUOTED_BOLD_TAG "error in quoted bold tag"
#define ERROR_REDEFINED_KEYWORD "attempt to redefine keyword Y in A"
#define ERROR_REFINEMENT_APPLIED "refinement is applied more than once"
#define ERROR_REFINEMENT_DEFINED "refinement already defined"
#define ERROR_REFINEMENT_EMPTY "empty refinement at end of source"
#define ERROR_REFINEMENT_INVALID "invalid refinement"
#define ERROR_REFINEMENT_NOT_APPLIED "refinement is not applied"
#define ERROR_RESOLVE "cannot resolve symbol"
#define ERROR_SCOPE_DYNAMIC_0 "value is exported out of its scope"
#define ERROR_SCOPE_DYNAMIC_1 "M value is exported out of its scope"
#define ERROR_SCOPE_DYNAMIC_2 "M value from %s is exported out of its scope"
#define ERROR_SHELL_SCRIPT "source is a shell script"
#define ERROR_SOUND_INTERNAL "error while processing M value (Y)"
#define ERROR_SOUND_INTERNAL_STRING "error while processing M value (Y \"Y\")"
#define ERROR_SOURCE_FILE_EMPTY "source file contains no program" 
#define ERROR_SOURCE_FILE_OPEN "error while opening source file"
#define ERROR_STACK_OVERFLOW "stack overflow"
#define ERROR_SUBSET_RELATED "M has firmly related subset M"
#define ERROR_SYNTAX "detected in A"
#define ERROR_SYNTAX_EXPECTED "expected A"
#define ERROR_SYNTAX_MIXED_DECLARATION "possibly mixed identity and variable declaration"
#define ERROR_SYNTAX_STRANGE_SEPARATOR "possibly a missing or erroneous separator nearby"
#define ERROR_SYNTAX_STRANGE_TOKENS "possibly a missing or erroneous symbol nearby"
#define ERROR_TIME_LIMIT_EXCEEDED "time limit exceeded"
#define ERROR_TOO_MANY_ARGUMENTS "too many arguments"
#define ERROR_TOO_MANY_OPEN_FILES "too many open files"
#define ERROR_TORRIX "linear algebra error; Y; Y"
#define ERROR_TRANSIENT_NAME "attempt at storing a transient name"
#define ERROR_UNBALANCED_KEYWORD "missing or unbalanced keyword in A, near Z L"
#define ERROR_UNDECLARED_TAG "tag S has not been declared properly"
#define ERROR_UNDECLARED_TAG_2 "tag Z has not been declared properly"
#define ERROR_UNDEFINED_TRANSPUT "transput of M value by this procedure is not defined"
#define ERROR_UNIMPLEMENTED "S is either not implemented or not compiled"
#define ERROR_UNSPECIFIED "unspecified error"
#define ERROR_UNTERMINATED_COMMENT "unterminated comment"
#define ERROR_UNTERMINATED_PRAGMAT "unterminated pragmat"
#define ERROR_UNTERMINATED_PRAGMENT "unterminated pragment"
#define ERROR_UNTERMINATED_STRING "unterminated string"
#define ERROR_UNWORTHY_CHARACTER "unworthy character"
#define ERROR_VACUUM "this vacuum cannot have row elements (use a Y generator)"
#define INFO_APPROPRIATE_DECLARER "appropriate declarer"
#define INFO_MISSING_KEYWORDS "missing or unmatched keyword"
#define WARNING_EXTENSION "@ is an extension"
#define WARNING_HIDES "declaration hides a declaration of S with larger reach"
#define WARNING_HIDES_PRELUDE "declaration hides prelude declaration of M S"
#define WARNING_HIP "@ should not be in C context"
#define WARNING_MATH_ACCURACY "accuracy loss due to choice of parameters"
#define WARNING_MATH_PRECISION "M A precision limited due to choice of parameters"
#define WARNING_OPTIMISATION "optimisation has no effect on this platform"
#define WARNING_OVERFLOW "M constant overflow"
#define WARNING_PRECISION "D digits precision impacts performance"
#define WARNING_SCOPE_STATIC "M A is a potential scope violation"
#define WARNING_SKIPPED_SUPERFLUOUS "skipped superfluous A"
#define WARNING_TAG_NOT_PORTABLE "tag S is not portable"
#define WARNING_TAG_UNUSED "tag S is not used"
#define WARNING_TRAILING "ignoring trailing character H in A"
#define WARNING_UNDERFLOW "M constant underflow"
#define WARNING_UNINITIALISED "identifier S might be used before being initialised"
#define WARNING_UNINTENDED "possibly unintended M A in M A"
#define WARNING_VOIDED "value of M @ will be voided"
#define WARNING_WIDENING_NOT_PORTABLE "implicit widening is not portable"

#endif
