/**
@file code.c
@author J. Marcel van der Veer
@brief Emit C code for Algol 68 constructs.
@section Copyright

This file is part of Algol68G - an Algol 68 compiler-interpreter.
Copyright (C) 2001-2013 J. Marcel van der Veer <algol68g@xs4all.nl>.

@section License

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.

@section Description

This file generates optimised C routines for many units in an Algol 68 source
program. A68G 1.x contained some general optimised routines. These are
decommissioned in A68G 2.x that dynamically generates routines depending
on the source code. The generated routines are compiled on the fly into a dynamic
library that is linked by the running interpreter.
To invoke this code generator specify option --optimise.
Currently the optimiser only considers units that operate on basic modes that are
contained in a single C struct, for instance primitive modes

  INT, REAL, BOOL, CHAR and BITS

and simple structures of these basic modes, such as

  COMPLEX

and also (single) references, rows and procedures

  REF MODE, [] MODE, PROC PARAMSETY MODE

The code generator employs a few simple optimisations like constant folding
and common subexpression elimination when DEREFERENCING or SLICING is
performed; for instance

  x[i + 1] := x[i + 1] + 1

translates into

  tmp = x[i + 1]; tmp := tmp + 1

There are no optimisations that are easily recognised by the back end compiler,
for instance symbolic simplification.

You will find here and there lines

  if (DEBUG_LEVEL >= ...) 

which I use to debug the compiler - MvdV.
1: denotations only
2: also basic unit compilation 
3: also better fetching of data from the stack
4: also compile enclosed clauses
Below definition switches everything on:
#define DEBUG_LEVEL 9
**/

#define DEBUG_LEVEL 9

#if defined HAVE_CONFIG_H
#include "a68g-config.h"
#endif

#include "a68g.h"

#define BASIC(p, n) (basic_unit (locate ((p), (n))))

#define CON "_const"
#define ELM "_elem"
#define TMP "_tmp"
#define ARG "_arg"
#define ARR "_array"
#define DEC "_declarer"
#define DRF "_deref"
#define DSP "_display"
#define FUN "_function"
#define PUP "_pop"
#define REF "_ref"
#define SEL "_field"
#define TUP "_tuple"

#define A68_MAKE_NOTHING 0
#define A68_MAKE_OTHERS 1
#define A68_MAKE_FUNCTION 2

#define OFFSET_OFF(s) (OFFSET (NODE_PACK (SUB (s))))
#define LONG_MODE(m) ((m) == MODE (LONG_INT) || (m) == MODE (LONG_REAL))
#define WIDEN_TO(p, a, b) (MOID (p) == MODE (b) && MOID (SUB (p)) == MODE (a))

#define GC_MODE(m) (m != NO_MOID && (IS (m, REF_SYMBOL) || IS (DEFLEX (m), ROW_SYMBOL)))
#define NEEDS_DNS(m) (m != NO_MOID && (IS (m, REF_SYMBOL) || IS (m, PROC_SYMBOL) || IS (m, UNION_SYMBOL) || IS (m, FORMAT_SYMBOL)))

#define CODE_EXECUTE(p) {\
  indentf (out, snprintf (line, SNPRINTF_SIZE, "EXECUTE_UNIT_TRACE (_N_ (%d));", NUMBER (p)));\
  }

#define NAME_SIZE 128

static BOOL_T long_mode_allowed = A68_TRUE;

static int indentation = 0;
static char line[BUFFER_SIZE];

static BOOL_T basic_unit (NODE_T *);
static char *compile_unit (NODE_T *, FILE_T, BOOL_T);
static void inline_unit (NODE_T *, FILE_T, int);
static void compile_units (NODE_T *, FILE_T);
static void indent (FILE_T, char *);
static void indentf (FILE_T, int);

/* The phases we go through */
enum {L_NONE = 0, L_DECLARE = 1, L_INITIALISE, L_EXECUTE, L_EXECUTE_2, L_YIELD, L_PUSH};

/*********************************************************/
/* TRANSLATION tabulates translations for genie actions. */
/* This tells what to call for an A68 action.            */
/*********************************************************/

typedef int LEVEL_T;
typedef struct {GPROC * procedure; char * code;} TRANSLATION;

static TRANSLATION monadics[] = {
  {genie_minus_int, "-"},
  {genie_minus_real, "-"},
  {genie_abs_int, "labs"},
  {genie_abs_real, "fabs"},
  {genie_sign_int, "SIGN"},
  {genie_sign_real, "SIGN"},
  {genie_entier_real, "a68g_entier"},
  {genie_round_real, "a68g_round"},
  {genie_not_bool, "!"},
  {genie_abs_bool, "(int) "},
  {genie_abs_bits, "(int) "},
  {genie_bin_int, "(unsigned) "},
  {genie_not_bits, "~"},
  {genie_abs_char, "TO_UCHAR"},
  {genie_repr_char, ""},
  {genie_re_complex, "a68g_re_complex"},
  {genie_im_complex, "a68g_im_complex"},
  {genie_minus_complex, "a68g_minus_complex"},
  {genie_abs_complex, "a68g_abs_complex"},
  {genie_arg_complex, "a68g_arg_complex"},
  {genie_conj_complex, "a68g_conj_complex"},
  {genie_round_long_mp, "(void) round_mp"},
  {genie_entier_long_mp, "(void) entier_mp"},
  {genie_minus_long_mp, "(void) minus_mp"},
  {genie_abs_long_mp, "(void) abs_mp"},
  {genie_idle, ""},
  {NO_GPROC, NO_TEXT}
};

static TRANSLATION dyadics[] = {
  {genie_add_int, "+"},
  {genie_sub_int, "-"},
  {genie_mul_int, "*"},
  {genie_over_int, "/"},
  {genie_mod_int, "a68g_mod_int"},
  {genie_div_int, "DIV_INT"},
  {genie_eq_int, "=="},
  {genie_ne_int, "!="},
  {genie_lt_int, "<"},
  {genie_gt_int, ">"},
  {genie_le_int, "<="},
  {genie_ge_int, ">="},
  {genie_plusab_int, "a68g_plusab_int"},
  {genie_minusab_int, "a68g_minusab_int"},
  {genie_timesab_int, "a68g_timesab_int"},
  {genie_overab_int, "a68g_overab_int"},
  {genie_add_real, "+"},
  {genie_sub_real, "-"},
  {genie_mul_real, "*"},
  {genie_div_real, "/"},
  {genie_pow_real, "a68g_pow_real"},
  {genie_pow_real_int, "a68g_pow_real_int"},
  {genie_eq_real, "=="},
  {genie_ne_real, "!="},
  {genie_lt_real, "<"},
  {genie_gt_real, ">"},
  {genie_le_real, "<="},
  {genie_ge_real, ">="},
  {genie_plusab_real, "a68g_plusab_real"},
  {genie_minusab_real, "a68g_minusab_real"},
  {genie_timesab_real, "a68g_timesab_real"},
  {genie_divab_real, "a68g_divab_real"},
  {genie_eq_char, "=="},
  {genie_ne_char, "!="},
  {genie_lt_char, "<"},
  {genie_gt_char, ">"},
  {genie_le_char, "<="},
  {genie_ge_char, ">="},
  {genie_eq_bool, "=="},
  {genie_ne_bool, "!="},
  {genie_and_bool, "&&"},
  {genie_or_bool, "||"},
  {genie_and_bits, "&"},
  {genie_or_bits, "|"},
  {genie_eq_bits, "=="},
  {genie_ne_bits, "!="},
  {genie_shl_bits, "<<"},
  {genie_shr_bits, ">>"},
  {genie_icomplex, "a68g_i_complex"},
  {genie_iint_complex, "a68g_i_complex"},
  {genie_abs_complex, "a68g_abs_complex"},
  {genie_arg_complex, "a68g_arg_complex"},
  {genie_add_complex, "a68g_add_complex"},
  {genie_sub_complex, "a68g_sub_complex"},
  {genie_mul_complex, "a68g_mul_complex"},
  {genie_div_complex, "a68g_div_complex"},
  {genie_eq_complex, "a68g_eq_complex"},
  {genie_ne_complex, "a68g_ne_complex"},
  {genie_add_long_int, "(void) add_mp"},
  {genie_add_long_mp, "(void) add_mp"},
  {genie_sub_long_int, "(void) sub_mp"},
  {genie_sub_long_mp, "(void) sub_mp"},
  {genie_mul_long_int, "(void) mul_mp"},
  {genie_mul_long_mp, "(void) mul_mp"},
  {genie_over_long_mp, "(void) over_mp"},
  {genie_div_long_mp, "(void) div_mp"},
  {genie_eq_long_mp, "eq_mp"},
  {genie_ne_long_mp, "ne_mp"},
  {genie_lt_long_mp, "lt_mp"},
  {genie_le_long_mp, "le_mp"},
  {genie_gt_long_mp, "gt_mp"},
  {genie_ge_long_mp, "ge_mp"},
  {NO_GPROC, NO_TEXT}
};

static TRANSLATION functions[] = {
  {genie_sqrt_real, "sqrt"},
  {genie_curt_real, "curt"},
  {genie_exp_real, "a68g_exp"},
  {genie_ln_real, "log"},
  {genie_log_real, "log10"},
  {genie_sin_real, "sin"},
  {genie_cos_real, "cos"},
  {genie_tan_real, "tan"},
  {genie_arcsin_real, "asin"},
  {genie_arccos_real, "acos"},
  {genie_arctan_real, "atan"},
  {genie_sinh_real, "sinh"},
  {genie_cosh_real, "cosh"},
  {genie_tanh_real, "tanh"},
  {genie_arcsinh_real, "a68g_asinh"},
  {genie_arccosh_real, "a68g_acosh"},
  {genie_arctanh_real, "a68g_atanh"},
  {genie_inverf_real, "inverf"},
  {genie_inverfc_real, "inverfc"},
  {genie_sqrt_complex, "a68g_sqrt_complex"},
  {genie_exp_complex, "a68g_exp_complex"},
  {genie_ln_complex, "a68g_ln_complex"},
  {genie_sin_complex, "a68g_sin_complex"},
  {genie_cos_complex, "a68g_cos_complex"},
  {genie_tan_complex, "a68g_tan_complex"},
  {genie_arcsin_complex, "a68g_arcsin_complex"},
  {genie_arccos_complex, "a68g_arccos_complex"},
  {genie_arctan_complex, "a68g_arctan_complex"},
  {genie_sqrt_long_mp, "(void) sqrt_mp"},
  {genie_exp_long_mp, "(void) exp_mp"},
  {genie_ln_long_mp, "(void) ln_mp"},
  {genie_log_long_mp, "(void) log_mp"},
  {genie_sin_long_mp, "(void) sin_mp"},
  {genie_cos_long_mp, "(void) cos_mp"},
  {genie_tan_long_mp, "(void) tan_mp"},
  {genie_asin_long_mp, "(void) asin_mp"},
  {genie_acos_long_mp, "(void) acos_mp"},
  {genie_atan_long_mp, "(void) atan_mp"},
  {genie_sinh_long_mp, "(void) sinh_mp"},
  {genie_cosh_long_mp, "(void) cosh_mp"},
  {genie_tanh_long_mp, "(void) tanh_mp"},
  {genie_arcsinh_long_mp, "(void) asinh_mp"},
  {genie_arccosh_long_mp, "(void) acosh_mp"},
  {genie_arctanh_long_mp, "(void) atanh_mp"},
  {NO_GPROC, NO_TEXT}
};

static TRANSLATION constants[] = {
  {genie_int_lengths, "3"},
  {genie_int_shorths, "1"},
  {genie_real_lengths, "3"},
  {genie_real_shorths, "1"},
  {genie_complex_lengths, "3"},
  {genie_complex_shorths, "1"},
  {genie_bits_lengths, "3"},
  {genie_bits_shorths, "1"},
  {genie_bytes_lengths, "2"},
  {genie_bytes_shorths, "1"},
  {genie_int_width, "INT_WIDTH"},
  {genie_long_int_width, "LONG_INT_WIDTH"},
  {genie_longlong_int_width, "LONGLONG_INT_WIDTH"},
  {genie_real_width, "REAL_WIDTH"},
  {genie_long_real_width, "LONG_REAL_WIDTH"},
  {genie_longlong_real_width, "LONGLONG_REAL_WIDTH"},
  {genie_exp_width, "EXP_WIDTH"},
  {genie_long_exp_width, "LONG_EXP_WIDTH"},
  {genie_longlong_exp_width, "LONGLONG_EXP_WIDTH"},
  {genie_bits_width, "BITS_WIDTH"},
  {genie_bytes_width, "BYTES_WIDTH"},
  {genie_long_bytes_width, "LONG_BYTES_WIDTH"},
  {genie_max_abs_char, "UCHAR_MAX"},
  {genie_max_int, "A68_MAX_INT"},
  {genie_max_real, "DBL_MAX"},
  {genie_min_real, "DBL_MIN"},
  {genie_null_char, "NULL_CHAR"},
  {genie_small_real, "DBL_EPSILON"},
  {genie_pi, "A68_PI"},
  {genie_pi_long_mp, NO_TEXT},
  {genie_long_max_int, NO_TEXT},
  {genie_long_min_real, NO_TEXT},
  {genie_long_small_real, NO_TEXT},
  {genie_long_max_real, NO_TEXT},
  {genie_cgs_acre, "GSL_CONST_CGSM_ACRE"},
  {genie_cgs_angstrom, "GSL_CONST_CGSM_ANGSTROM"},
  {genie_cgs_astronomical_unit, "GSL_CONST_CGSM_ASTRONOMICAL_UNIT"},
  {genie_cgs_bar, "GSL_CONST_CGSM_BAR"},
  {genie_cgs_barn, "GSL_CONST_CGSM_BARN"},
  {genie_cgs_bohr_magneton, "GSL_CONST_CGSM_BOHR_MAGNETON"},
  {genie_cgs_bohr_radius, "GSL_CONST_CGSM_BOHR_RADIUS"},
  {genie_cgs_boltzmann, "GSL_CONST_CGSM_BOLTZMANN"},
  {genie_cgs_btu, "GSL_CONST_CGSM_BTU"},
  {genie_cgs_calorie, "GSL_CONST_CGSM_CALORIE"},
  {genie_cgs_canadian_gallon, "GSL_CONST_CGSM_CANADIAN_GALLON"},
  {genie_cgs_carat, "GSL_CONST_CGSM_CARAT"},
  {genie_cgs_cup, "GSL_CONST_CGSM_CUP"},
  {genie_cgs_curie, "GSL_CONST_CGSM_CURIE"},
  {genie_cgs_day, "GSL_CONST_CGSM_DAY"},
  {genie_cgs_dyne, "GSL_CONST_CGSM_DYNE"},
  {genie_cgs_electron_charge, "GSL_CONST_CGSM_ELECTRON_CHARGE"},
  {genie_cgs_electron_magnetic_moment, "GSL_CONST_CGSM_ELECTRON_MAGNETIC_MOMENT"},
  {genie_cgs_electron_volt, "GSL_CONST_CGSM_ELECTRON_VOLT"}, 
  {genie_cgs_erg, "GSL_CONST_CGSM_ERG"},
  {genie_cgs_faraday, "GSL_CONST_CGSM_FARADAY"}, 
  {genie_cgs_fathom, "GSL_CONST_CGSM_FATHOM"},
  {genie_cgs_fluid_ounce, "GSL_CONST_CGSM_FLUID_OUNCE"}, 
  {genie_cgs_foot, "GSL_CONST_CGSM_FOOT"},
  {genie_cgs_footcandle, "GSL_CONST_CGSM_FOOTCANDLE"}, 
  {genie_cgs_footlambert, "GSL_CONST_CGSM_FOOTLAMBERT"},
  {genie_cgs_gauss, "GSL_CONST_CGSM_GAUSS"}, 
  {genie_cgs_gram_force, "GSL_CONST_CGSM_GRAM_FORCE"},
  {genie_cgs_grav_accel, "GSL_CONST_CGSM_GRAV_ACCEL"},
  {genie_cgs_gravitational_constant, "GSL_CONST_CGSM_GRAVITATIONAL_CONSTANT"},
  {genie_cgs_hectare, "GSL_CONST_CGSM_HECTARE"}, 
  {genie_cgs_horsepower, "GSL_CONST_CGSM_HORSEPOWER"},
  {genie_cgs_hour, "GSL_CONST_CGSM_HOUR"}, 
  {genie_cgs_inch, "GSL_CONST_CGSM_INCH"},
  {genie_cgs_inch_of_mercury, "GSL_CONST_CGSM_INCH_OF_MERCURY"},
  {genie_cgs_inch_of_water, "GSL_CONST_CGSM_INCH_OF_WATER"}, 
  {genie_cgs_joule, "GSL_CONST_CGSM_JOULE"},
  {genie_cgs_kilometers_per_hour, "GSL_CONST_CGSM_KILOMETERS_PER_HOUR"},
  {genie_cgs_kilopound_force, "GSL_CONST_CGSM_KILOPOUND_FORCE"}, 
  {genie_cgs_knot, "GSL_CONST_CGSM_KNOT"},
  {genie_cgs_lambert, "GSL_CONST_CGSM_LAMBERT"}, 
  {genie_cgs_light_year, "GSL_CONST_CGSM_LIGHT_YEAR"},
  {genie_cgs_liter, "GSL_CONST_CGSM_LITER"}, 
  {genie_cgs_lumen, "GSL_CONST_CGSM_LUMEN"},
  {genie_cgs_lux, "GSL_CONST_CGSM_LUX"}, 
  {genie_cgs_mass_electron, "GSL_CONST_CGSM_MASS_ELECTRON"},
  {genie_cgs_mass_muon, "GSL_CONST_CGSM_MASS_MUON"}, 
  {genie_cgs_mass_neutron, "GSL_CONST_CGSM_MASS_NEUTRON"},
  {genie_cgs_mass_proton, "GSL_CONST_CGSM_MASS_PROTON"},
  {genie_cgs_meter_of_mercury, "GSL_CONST_CGSM_METER_OF_MERCURY"},
  {genie_cgs_metric_ton, "GSL_CONST_CGSM_METRIC_TON"}, 
  {genie_cgs_micron, "GSL_CONST_CGSM_MICRON"},
  {genie_cgs_mil, "GSL_CONST_CGSM_MIL"}, 
  {genie_cgs_mile, "GSL_CONST_CGSM_MILE"},
  {genie_cgs_miles_per_hour, "GSL_CONST_CGSM_MILES_PER_HOUR"}, 
  {genie_cgs_minute, "GSL_CONST_CGSM_MINUTE"},
  {genie_cgs_molar_gas, "GSL_CONST_CGSM_MOLAR_GAS"}, 
  {genie_cgs_nautical_mile, "GSL_CONST_CGSM_NAUTICAL_MILE"},
  {genie_cgs_newton, "GSL_CONST_CGSM_NEWTON"}, 
  {genie_cgs_nuclear_magneton, "GSL_CONST_CGSM_NUCLEAR_MAGNETON"},
  {genie_cgs_ounce_mass, "GSL_CONST_CGSM_OUNCE_MASS"}, 
  {genie_cgs_parsec, "GSL_CONST_CGSM_PARSEC"},
  {genie_cgs_phot, "GSL_CONST_CGSM_PHOT"}, 
  {genie_cgs_pint, "GSL_CONST_CGSM_PINT"},
  {genie_cgs_planck_constant_h, "6.6260693e-27"}, 
  {genie_cgs_planck_constant_hbar, "1.0545717e-27"},
  {genie_cgs_point, "GSL_CONST_CGSM_POINT"}, 
  {genie_cgs_poise, "GSL_CONST_CGSM_POISE"},
  {genie_cgs_pound_force, "GSL_CONST_CGSM_POUND_FORCE"}, 
  {genie_cgs_pound_mass, "GSL_CONST_CGSM_POUND_MASS"},
  {genie_cgs_poundal, "GSL_CONST_CGSM_POUNDAL"},
  {genie_cgs_proton_magnetic_moment, "GSL_CONST_CGSM_PROTON_MAGNETIC_MOMENT"},
  {genie_cgs_psi, "GSL_CONST_CGSM_PSI"}, 
  {genie_cgs_quart, "GSL_CONST_CGSM_QUART"},
  {genie_cgs_rad, "GSL_CONST_CGSM_RAD"}, 
  {genie_cgs_roentgen, "GSL_CONST_CGSM_ROENTGEN"},
  {genie_cgs_rydberg, "GSL_CONST_CGSM_RYDBERG"}, 
  {genie_cgs_solar_mass, "GSL_CONST_CGSM_SOLAR_MASS"},
  {genie_cgs_speed_of_light, "GSL_CONST_CGSM_SPEED_OF_LIGHT"},
  {genie_cgs_standard_gas_volume, "GSL_CONST_CGSM_STANDARD_GAS_VOLUME"},
  {genie_cgs_std_atmosphere, "GSL_CONST_CGSM_STD_ATMOSPHERE"}, 
  {genie_cgs_stilb, "GSL_CONST_CGSM_STILB"},
  {genie_cgs_stokes, "GSL_CONST_CGSM_STOKES"}, 
  {genie_cgs_tablespoon, "GSL_CONST_CGSM_TABLESPOON"},
  {genie_cgs_teaspoon, "GSL_CONST_CGSM_TEASPOON"}, 
  {genie_cgs_texpoint, "GSL_CONST_CGSM_TEXPOINT"},
  {genie_cgs_therm, "GSL_CONST_CGSM_THERM"}, 
  {genie_cgs_ton, "GSL_CONST_CGSM_TON"},
  {genie_cgs_torr, "GSL_CONST_CGSM_TORR"}, 
  {genie_cgs_troy_ounce, "GSL_CONST_CGSM_TROY_OUNCE"},
  {genie_cgs_uk_gallon, "GSL_CONST_CGSM_UK_GALLON"}, 
  {genie_cgs_uk_ton, "GSL_CONST_CGSM_UK_TON"},
  {genie_cgs_unified_atomic_mass, "GSL_CONST_CGSM_UNIFIED_ATOMIC_MASS"},
  {genie_cgs_us_gallon, "GSL_CONST_CGSM_US_GALLON"}, 
  {genie_cgs_week, "GSL_CONST_CGSM_WEEK"},
  {genie_cgs_yard, "GSL_CONST_CGSM_YARD"}, 
  {genie_mks_acre, "GSL_CONST_MKS_ACRE"},
  {genie_mks_angstrom, "GSL_CONST_MKS_ANGSTROM"},
  {genie_mks_astronomical_unit, "GSL_CONST_MKS_ASTRONOMICAL_UNIT"}, 
  {genie_mks_bar, "GSL_CONST_MKS_BAR"},
  {genie_mks_barn, "GSL_CONST_MKS_BARN"}, 
  {genie_mks_bohr_magneton, "GSL_CONST_MKS_BOHR_MAGNETON"},
  {genie_mks_bohr_radius, "GSL_CONST_MKS_BOHR_RADIUS"}, 
  {genie_mks_boltzmann, "GSL_CONST_MKS_BOLTZMANN"},
  {genie_mks_btu, "GSL_CONST_MKS_BTU"}, 
  {genie_mks_calorie, "GSL_CONST_MKS_CALORIE"},
  {genie_mks_canadian_gallon, "GSL_CONST_MKS_CANADIAN_GALLON"}, 
  {genie_mks_carat, "GSL_CONST_MKS_CARAT"},
  {genie_mks_cup, "GSL_CONST_MKS_CUP"}, 
  {genie_mks_curie, "GSL_CONST_MKS_CURIE"},
  {genie_mks_day, "GSL_CONST_MKS_DAY"}, 
  {genie_mks_dyne, "GSL_CONST_MKS_DYNE"},
  {genie_mks_electron_charge, "GSL_CONST_MKS_ELECTRON_CHARGE"},
  {genie_mks_electron_magnetic_moment, "GSL_CONST_MKS_ELECTRON_MAGNETIC_MOMENT"},
  {genie_mks_electron_volt, "GSL_CONST_MKS_ELECTRON_VOLT"}, 
  {genie_mks_erg, "GSL_CONST_MKS_ERG"},
  {genie_mks_faraday, "GSL_CONST_MKS_FARADAY"}, 
  {genie_mks_fathom, "GSL_CONST_MKS_FATHOM"},
  {genie_mks_fluid_ounce, "GSL_CONST_MKS_FLUID_OUNCE"}, 
  {genie_mks_foot, "GSL_CONST_MKS_FOOT"},
  {genie_mks_footcandle, "GSL_CONST_MKS_FOOTCANDLE"}, 
  {genie_mks_footlambert, "GSL_CONST_MKS_FOOTLAMBERT"},
  {genie_mks_gauss, "GSL_CONST_MKS_GAUSS"}, 
  {genie_mks_gram_force, "GSL_CONST_MKS_GRAM_FORCE"},
  {genie_mks_grav_accel, "GSL_CONST_MKS_GRAV_ACCEL"},
  {genie_mks_gravitational_constant, "GSL_CONST_MKS_GRAVITATIONAL_CONSTANT"},
  {genie_mks_hectare, "GSL_CONST_MKS_HECTARE"}, 
  {genie_mks_horsepower, "GSL_CONST_MKS_HORSEPOWER"},
  {genie_mks_hour, "GSL_CONST_MKS_HOUR"}, 
  {genie_mks_inch, "GSL_CONST_MKS_INCH"},
  {genie_mks_inch_of_mercury, "GSL_CONST_MKS_INCH_OF_MERCURY"},
  {genie_mks_inch_of_water, "GSL_CONST_MKS_INCH_OF_WATER"}, 
  {genie_mks_joule, "GSL_CONST_MKS_JOULE"},
  {genie_mks_kilometers_per_hour, "GSL_CONST_MKS_KILOMETERS_PER_HOUR"},
  {genie_mks_kilopound_force, "GSL_CONST_MKS_KILOPOUND_FORCE"}, 
  {genie_mks_knot, "GSL_CONST_MKS_KNOT"},
  {genie_mks_lambert, "GSL_CONST_MKS_LAMBERT"}, 
  {genie_mks_light_year, "GSL_CONST_MKS_LIGHT_YEAR"},
  {genie_mks_liter, "GSL_CONST_MKS_LITER"}, 
  {genie_mks_lumen, "GSL_CONST_MKS_LUMEN"},
  {genie_mks_lux, "GSL_CONST_MKS_LUX"}, 
  {genie_mks_mass_electron, "GSL_CONST_MKS_MASS_ELECTRON"},
  {genie_mks_mass_muon, "GSL_CONST_MKS_MASS_MUON"}, 
  {genie_mks_mass_neutron, "GSL_CONST_MKS_MASS_NEUTRON"},
  {genie_mks_mass_proton, "GSL_CONST_MKS_MASS_PROTON"},
  {genie_mks_meter_of_mercury, "GSL_CONST_MKS_METER_OF_MERCURY"},
  {genie_mks_metric_ton, "GSL_CONST_MKS_METRIC_TON"}, 
  {genie_mks_micron, "GSL_CONST_MKS_MICRON"},
  {genie_mks_mil, "GSL_CONST_MKS_MIL"}, 
  {genie_mks_mile, "GSL_CONST_MKS_MILE"},
  {genie_mks_miles_per_hour, "GSL_CONST_MKS_MILES_PER_HOUR"}, 
  {genie_mks_minute, "GSL_CONST_MKS_MINUTE"},
  {genie_mks_molar_gas, "GSL_CONST_MKS_MOLAR_GAS"}, 
  {genie_mks_nautical_mile, "GSL_CONST_MKS_NAUTICAL_MILE"},
  {genie_mks_newton, "GSL_CONST_MKS_NEWTON"}, 
  {genie_mks_nuclear_magneton, "GSL_CONST_MKS_NUCLEAR_MAGNETON"},
  {genie_mks_ounce_mass, "GSL_CONST_MKS_OUNCE_MASS"}, 
  {genie_mks_parsec, "GSL_CONST_MKS_PARSEC"},
  {genie_mks_phot, "GSL_CONST_MKS_PHOT"}, 
  {genie_mks_pint, "GSL_CONST_MKS_PINT"},
  {genie_mks_planck_constant_h, "6.6260693e-34"}, 
  {genie_mks_planck_constant_hbar, "1.0545717e-34"},
  {genie_mks_point, "GSL_CONST_MKS_POINT"}, 
  {genie_mks_poise, "GSL_CONST_MKS_POISE"},
  {genie_mks_pound_force, "GSL_CONST_MKS_POUND_FORCE"}, 
  {genie_mks_pound_mass, "GSL_CONST_MKS_POUND_MASS"},
  {genie_mks_poundal, "GSL_CONST_MKS_POUNDAL"},
  {genie_mks_proton_magnetic_moment, "GSL_CONST_MKS_PROTON_MAGNETIC_MOMENT"},
  {genie_mks_psi, "GSL_CONST_MKS_PSI"}, 
  {genie_mks_quart, "GSL_CONST_MKS_QUART"},
  {genie_mks_rad, "GSL_CONST_MKS_RAD"}, 
  {genie_mks_roentgen, "GSL_CONST_MKS_ROENTGEN"},
  {genie_mks_rydberg, "GSL_CONST_MKS_RYDBERG"}, 
  {genie_mks_solar_mass, "GSL_CONST_MKS_SOLAR_MASS"},
  {genie_mks_speed_of_light, "GSL_CONST_MKS_SPEED_OF_LIGHT"},
  {genie_mks_standard_gas_volume, "GSL_CONST_MKS_STANDARD_GAS_VOLUME"},
  {genie_mks_std_atmosphere, "GSL_CONST_MKS_STD_ATMOSPHERE"}, 
  {genie_mks_stilb, "GSL_CONST_MKS_STILB"},
  {genie_mks_stokes, "GSL_CONST_MKS_STOKES"}, 
  {genie_mks_tablespoon, "GSL_CONST_MKS_TABLESPOON"},
  {genie_mks_teaspoon, "GSL_CONST_MKS_TEASPOON"}, 
  {genie_mks_texpoint, "GSL_CONST_MKS_TEXPOINT"},
  {genie_mks_therm, "GSL_CONST_MKS_THERM"}, 
  {genie_mks_ton, "GSL_CONST_MKS_TON"},
  {genie_mks_torr, "GSL_CONST_MKS_TORR"}, 
  {genie_mks_troy_ounce, "GSL_CONST_MKS_TROY_OUNCE"},
  {genie_mks_uk_gallon, "GSL_CONST_MKS_UK_GALLON"}, 
  {genie_mks_uk_ton, "GSL_CONST_MKS_UK_TON"},
  {genie_mks_unified_atomic_mass, "GSL_CONST_MKS_UNIFIED_ATOMIC_MASS"},
  {genie_mks_us_gallon, "GSL_CONST_MKS_US_GALLON"},
  {genie_mks_vacuum_permeability, "GSL_CONST_MKS_VACUUM_PERMEABILITY"},
  {genie_mks_vacuum_permittivity, "GSL_CONST_MKS_VACUUM_PERMITTIVITY"}, 
  {genie_mks_week, "GSL_CONST_MKS_WEEK"},
  {genie_mks_yard, "GSL_CONST_MKS_YARD"}, 
  {genie_num_atto, "GSL_CONST_NUM_ATTO"},
  {genie_num_avogadro, "GSL_CONST_NUM_AVOGADRO"}, 
  {genie_num_exa, "GSL_CONST_NUM_EXA"},
  {genie_num_femto, "GSL_CONST_NUM_FEMTO"}, 
  {genie_num_fine_structure, "GSL_CONST_NUM_FINE_STRUCTURE"},
  {genie_num_giga, "GSL_CONST_NUM_GIGA"}, 
  {genie_num_kilo, "GSL_CONST_NUM_KILO"},
  {genie_num_mega, "GSL_CONST_NUM_MEGA"}, 
  {genie_num_micro, "GSL_CONST_NUM_MICRO"},
  {genie_num_milli, "GSL_CONST_NUM_MILLI"}, 
  {genie_num_nano, "GSL_CONST_NUM_NANO"},
  {genie_num_peta, "GSL_CONST_NUM_PETA"}, 
  {genie_num_pico, "GSL_CONST_NUM_PICO"},
  {genie_num_tera, "GSL_CONST_NUM_TERA"}, 
  {genie_num_yocto, "GSL_CONST_NUM_YOCTO"},
  {genie_num_yotta, "GSL_CONST_NUM_YOTTA"}, 
  {genie_num_zepto, "GSL_CONST_NUM_ZEPTO"},
  {genie_num_zetta, "GSL_CONST_NUM_ZETTA"},
  {NO_GPROC, NO_TEXT}
};

/**************************/
/* Pretty printing stuff. */
/**************************/

/**
@brief Write indented text.
@param out Output file descriptor.
@param str Text.
**/

static void indent (FILE_T out, char *str)
{
  int j = indentation;
  if (out == 0) {
    return;
  }
  while (j -- > 0) {
    WRITE (out, "  ");
  }
  WRITE (out, str);
}

/**
@brief Write unindented text.
@param out Output file descriptor.
@param str Text.
**/

static void undent (FILE_T out, char *str)
{
  if (out == 0) {
    return;
  }
  WRITE (out, str);
}

/**
@brief Write indent text.
@param out Output file descriptor.
@param ret Bytes written by snprintf.
**/

static void indentf (FILE_T out, int ret)
{
  if (out == 0) {
    return;
  }
  if (ret >= 0) {
    indent (out, line);
  } else {
    ABEND(A68_TRUE, "Return value failure", error_specification ());
  }
}

/**
@brief Write unindent text.
@param out Output file descriptor.
@param ret Bytes written by snprintf.
**/

static void undentf (FILE_T out, int ret)
{
  if (out == 0) {
    return;
  }
  if (ret >= 0) {
    WRITE (out, line);
  } else {
    ABEND(A68_TRUE, "Return value failure", error_specification ());
  }
}

/*************************************/
/* Administration of C declarations  */
/* Pretty printing of C declarations */
/*************************************/

/**
@brief Add declaration to a tree.
@param p Top token.
@param t Token text.
@return New entry.
**/

typedef struct DEC_T DEC_T;

struct DEC_T {
  char *text;
  int level;
  DEC_T *sub, *less, *more;
};

static DEC_T *root_idf = NO_DEC;

/**
@brief Add declaration to a tree.
@param p Top declaration.
@param level Pointer level (0, 1 = *, 2 = **, etcetera)
@param idf Identifier name.
@return New entry.
**/

DEC_T *add_identifier (DEC_T ** p, int level, char *idf)
{
  char *z = new_temp_string (idf);
  while (*p != NO_DEC) {
    int k = strcmp (z, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      ABEND(A68_TRUE, "duplicate declaration", z);
      return (*p);
    }
  }
  *p = (DEC_T *) get_temp_heap_space ((size_t) SIZE_AL (DEC_T));
  TEXT (*p) = z;
  LEVEL (*p) = level;
  SUB (*p) = LESS (*p) = MORE (*p) = NO_DEC;
  return (*p);
}

/**
@brief Add declaration to a tree.
@param p Top declaration.
@param mode Mode for identifier.
@param level Pointer level (0, 1 = *, 2 = **, etcetera)
@param idf Identifier name.
@return New entry.
**/

DEC_T *add_declaration (DEC_T ** p, char *mode, int level, char *idf)
{
  char *z = new_temp_string (mode);
  while (*p != NO_DEC) {
    int k = strcmp (z, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else if (k > 0) {
      p = &MORE (*p);
    } else {
      (void) add_identifier (&SUB(*p), level, idf);
      return (*p);
    }
  }
  *p = (DEC_T *) get_temp_heap_space ((size_t) SIZE_AL (DEC_T));
  TEXT (*p) = z;
  LEVEL (*p) = -1;
  SUB (*p) = LESS (*p) = MORE (*p) = NO_DEC;
  (void) add_identifier(&SUB(*p), level, idf);
  return (*p);
}

static BOOL_T put_idf_comma = A68_TRUE;

/**
@brief Print identifiers (following mode).
@param out File to print to.
@param p Top token.
**/

void print_identifiers (FILE_T out, DEC_T *p)
{
  if (p != NO_DEC) {
    print_identifiers (out, LESS (p));
    if (put_idf_comma) {
      WRITE (out, ", ");
    } else {
      put_idf_comma = A68_TRUE;
    }
    if (LEVEL (p) > 0) {
      int k = LEVEL (p);
      while (k--) {
        WRITE (out, "*");
      }
      WRITE (out, " ");
    }
    WRITE (out, TEXT (p));
    print_identifiers (out, MORE (p));
  }
}

/**
@brief Print declarations.
@param out File to print to.
@param p Top token.
**/

void print_declarations (FILE_T out, DEC_T *p)
{
  if (p != NO_DEC) {
    print_declarations (out, LESS (p));
    indent (out, TEXT (p));
    WRITE (out, " ");
    put_idf_comma = A68_FALSE;
    print_identifiers (out, SUB (p));
    WRITELN (out, ";\n")
    print_declarations (out, MORE (p));
  }
}

/***************************************************************************/
/* Administration for common (sub) expression elimination.                 */
/* BOOK keeps track of already seen (temporary) variables and denotations. */
/***************************************************************************/

typedef struct {
  int action, phase;
  char * idf;
  void * info;
  int number;
  } BOOK_T;

enum {BOOK_NONE = 0, BOOK_DECL, BOOK_INIT, BOOK_DEREF, BOOK_ARRAY, BOOK_COMPILE};

#define MAX_BOOK 1024

BOOK_T temp_book[MAX_BOOK];
int temp_book_pointer;

/**
@brief Book identifier to keep track of it for CSE.
@param action Some identification as L_DECLARE or DEREFERENCING.
@param phase Phase in which booking is made.
@param idf Identifier name.
@param info Identifier information.
@param number Unique identifying number.
**/

static void sign_in (int action, int phase, char * idf, void * info, int number)
{
  if (temp_book_pointer < MAX_BOOK) {
    ACTION (&temp_book[temp_book_pointer]) = action;
    PHASE (&temp_book[temp_book_pointer]) = phase;
    IDF (&temp_book[temp_book_pointer]) = idf;
    INFO (&temp_book[temp_book_pointer]) = info;
    NUMBER (&temp_book[temp_book_pointer]) = number;
    temp_book_pointer ++;
  }
}

/**
@brief Whether identifier is signed_in.
@param action Some identification as L_DECLARE or DEREFERENCING.
@param phase Phase in which booking is made.
@param idf Identifier name.
@return Number given to it.
**/

static BOOK_T * signed_in (int action, int phase, char * idf)
{
  int k;
  for (k = 0; k < temp_book_pointer; k ++) {
    if (IDF (&temp_book[k]) == idf && ACTION (&temp_book[k]) == action && PHASE (&temp_book[k]) >= phase) {
      return (& (temp_book[k]));
    }
  }
  return (NO_BOOK);
}

/**
@brief Make name.
@param buf Output buffer.
@param name Appropriate name.
@param tag Optional tag to name.
@param n Unique identifying number.
@return Output buffer.
**/

static char * make_name (char * buf, char * name, char * tag, int n)
{
  if (strlen (tag) > 0) {
    ASSERT (snprintf (buf, NAME_SIZE, "%s_%s_%d", name, tag, n) >= 0);
  } else {
    ASSERT (snprintf (buf, NAME_SIZE, "%s_%d", name, n) >= 0);
  }
  ABEND (strlen (buf) >= NAME_SIZE, "make name error", NO_TEXT);
  return (buf);
}

/**
@brief Whether two sub-trees are the same Algol 68 construct.
@param l Left-hand tree.
@param r Right-hand tree.
@return See brief description.
**/

static BOOL_T same_tree (NODE_T * l, NODE_T *r)
{
  if (l == NO_NODE) {
    return ((BOOL_T) (r == NO_NODE));
  } else if (r == NO_NODE) {
    return ((BOOL_T) (l == NO_NODE));
  } else if (ATTRIBUTE (l) == ATTRIBUTE (r) && NSYMBOL (l) == NSYMBOL (r)) {
    return ((BOOL_T) (same_tree (SUB (l), SUB (r)) && same_tree (NEXT (l), NEXT (r))));
  } else {
    return (A68_FALSE);
  }
}


/********************/
/* Basic mode check */
/********************/

/**
@brief Whether primitive mode, with simple C equivalent.
@param m Mode to check.
@return See brief description.
**/

static BOOL_T primitive_mode (MOID_T * m)
{
  if (m == MODE (INT)) {
    return (A68_TRUE);
  } else if (m == MODE (REAL)) {
    return (A68_TRUE);
  } else if (m == MODE (BOOL)) {
    return (A68_TRUE);
  } else if (m == MODE (CHAR)) {
    return (A68_TRUE);
  } else if (m == MODE (BITS)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether mode for which denotations are compiled.
@param m Mode to check.
@return See brief description.
**/

static BOOL_T denotation_mode (MOID_T * m)
{
  if (primitive_mode (m)) {
    return (A68_TRUE);
  } else if (LONG_MODE (m) && long_mode_allowed) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether mode is handled by the constant folder.
@param m Mode to check.
@return See brief description.
**/

BOOL_T folder_mode (MOID_T * m)
{
  if (primitive_mode (m)) {
    return (A68_TRUE);
  } else if (m == MODE (COMPLEX)) {
    return (A68_TRUE);
  } else if (LONG_MODE (m)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether basic mode, for which units are compiled.
@param m Mode to check.
@return See brief description.
**/

static BOOL_T basic_mode (MOID_T * m)
{
  if (denotation_mode (m)) {
    return (A68_TRUE);
  } else if (IS (m, REF_SYMBOL)) {
    if (IS (SUB (m), REF_SYMBOL) || IS (SUB (m), PROC_SYMBOL)) {
      return (A68_FALSE);
    } else {
      return (basic_mode (SUB (m)));
    }
  } else if (IS (m, ROW_SYMBOL)) {
    if (primitive_mode (SUB (m))) {
      return (A68_TRUE);
    } else if (IS (SUB (m), STRUCT_SYMBOL)) {
      return (basic_mode (SUB (m)));
    } else {
      return (A68_FALSE);
    }
  } else if (IS (m, STRUCT_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      if (!primitive_mode (MOID (p))) {
        return (A68_FALSE);
      }
    }
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether basic mode, which is not a row.
@param m Mode to check.
@return See brief description.
**/

static BOOL_T basic_mode_non_row (MOID_T * m)
{
  if (denotation_mode (m)) {
    return (A68_TRUE);
  } else if (IS (m, REF_SYMBOL)) {
    if (IS (SUB (m), REF_SYMBOL) || IS (SUB (m), PROC_SYMBOL)) {
      return (A68_FALSE);
    } else {
      return (basic_mode_non_row (SUB (m)));
    }
  } else if (IS (m, STRUCT_SYMBOL)) {
    PACK_T *p = PACK (m);
    for (; p != NO_PACK; FORWARD (p)) {
      if (!primitive_mode (MOID (p))) {
        return (A68_FALSE);
      }
    }
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether stems from certain attribute.
@param p Node in syntax tree.
@param att Attribute to comply to.
@return See brief description.
**/

static NODE_T * locate (NODE_T * p, int att)
{
  if (IS (p, VOIDING)) {
    return (locate (SUB (p), att));
  } else if (IS (p, UNIT)) {
    return (locate (SUB (p), att));
  } else if (IS (p, TERTIARY)) {
    return (locate (SUB (p), att));
  } else if (IS (p, SECONDARY)) {
    return (locate (SUB (p), att));
  } else if (IS (p, PRIMARY)) {
    return (locate (SUB (p), att));
  } else if (IS (p, att)) {
    return (p);
  } else {
    return (NO_NODE);
  }
}

/**********************************************************/
/* Basic unit check                                       */
/* Whether a unit is sufficiently "basic" to be compiled. */
/**********************************************************/

/**
@brief Whether basic collateral clause.
@param p Node in syntax tree.
@return See brief description.
**/

static BOOL_T basic_collateral (NODE_T * p)
{
  if (p == NO_NODE) {
    return (A68_TRUE);
  } else if (IS (p, UNIT)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_unit (SUB (p)) && basic_collateral (NEXT (p))));
  } else {
    return ((BOOL_T) (basic_collateral (SUB (p)) && basic_collateral (NEXT (p))));
  }
}

/**
@brief Whether basic serial clause.
@param p Node in syntax tree.
@param total Total units.
@param good Basic units.
@return See brief description.
**/

static void count_basic_units (NODE_T * p, int * total, int * good)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      (* total) ++;
      if (basic_unit (p)) {
        (* good) ++;
      }
    } else if (IS (p, DECLARATION_LIST)) {
      (* total) ++;
    } else {
      count_basic_units (SUB (p), total, good);
    }
  }
}

/**
@brief Whether basic serial clause.
@param p Node in syntax tree.
@param want > 0 is how many units we allow, <= 0 is don't care
@return See brief description.
**/

static BOOL_T basic_serial (NODE_T * p, int want)
{
  int total = 0, good = 0;
  count_basic_units (p, &total, &good);
  if (want > 0) {
    return (total == want && total == good);
  } else {
    return (total == good);
  }
}

/**
@brief Whether basic indexer.
@param p Node in syntax tree.
@return See brief description.
**/

static BOOL_T basic_indexer (NODE_T * p)
{
  if (p == NO_NODE) {
    return (A68_TRUE);
  } else if (IS (p, TRIMMER)) {
    return (A68_FALSE);
  } else if (IS (p, UNIT)) {
    return (basic_unit (p));
  } else {
    return ((BOOL_T) (basic_indexer (SUB (p)) && basic_indexer (NEXT (p))));
  }
}

/**
@brief Whether basic slice.
@param p Starting node.
@return See brief description.
**/

static BOOL_T basic_slice (NODE_T * p)
{
  if (IS (p, SLICE)) {
    NODE_T * prim = SUB (p);
    NODE_T * idf = locate (prim, IDENTIFIER);
    if (idf != NO_NODE) {
      NODE_T * indx = NEXT (prim);
      return (basic_indexer (indx));
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether basic argument.
@param p Starting node.
@return See brief description.
**/

static BOOL_T basic_argument (NODE_T * p)
{
  if (p == NO_NODE) {
    return (A68_TRUE);
  } else if (IS (p, UNIT)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_unit (p) && basic_argument (NEXT (p))));
  } else {
    return ((BOOL_T) (basic_argument (SUB (p)) && basic_argument (NEXT (p))));
  }
}

/**
@brief Whether basic call.
@param p Starting node.
@return See brief description.
**/

static BOOL_T basic_call (NODE_T * p)
{
  if (IS (p, CALL)) {
    NODE_T * prim = SUB (p);
    NODE_T * idf = locate (prim, IDENTIFIER);
    if (idf == NO_NODE) {
      return (A68_FALSE);
    } else if (SUB_MOID (idf) == MOID (p)) { /* Prevent partial parametrisation */
      int k;
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          NODE_T * args = NEXT (prim);
          return (basic_argument (args));
        }
      }
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether basic monadic formula.
@param p Starting node.
@return See brief description.
**/

static BOOL_T basic_monadic_formula (NODE_T * p)
{
  if (IS (p, MONADIC_FORMULA)) {
    NODE_T * op = SUB (p);
    int k;
    for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k ++) {
      if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
        NODE_T * rhs = NEXT (op);
        return (basic_unit (rhs));
      }
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether basic dyadic formula.
@param p Starting node.
@return See brief description.
**/

static BOOL_T basic_formula (NODE_T * p)
{
  if (IS (p, FORMULA)) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NO_NODE) {
      return (basic_monadic_formula (lhs));
    } else {
      int k;
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          NODE_T * rhs = NEXT (op);
          return ((BOOL_T) (basic_unit (lhs) && basic_unit (rhs)));
        }
      }
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether basic conditional clause.
@param p Starting node.
@return See brief description.
**/

static BOOL_T basic_conditional (NODE_T * p)
{
  if (! (IS (p, IF_PART) || IS (p, OPEN_PART))) {
    return (A68_FALSE);
  }
  if (! basic_serial (NEXT_SUB (p), 1)) {
    return (A68_FALSE);
  }
  FORWARD (p);
  if (! (IS (p, THEN_PART) || IS (p, CHOICE))) {
    return (A68_FALSE);
  }
  if (! basic_serial (NEXT_SUB (p), 1)) {
    return (A68_FALSE);
  }
  FORWARD (p);
  if (IS (p, ELSE_PART) || IS (p, CHOICE)) {
    return (basic_serial (NEXT_SUB (p), 1));
  } else if (IS (p, FI_SYMBOL)) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Whether basic unit.
@param p Starting node.
@return See brief description.
**/

static BOOL_T basic_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return (A68_FALSE);
  } else if (IS (p, UNIT)) {
    return (basic_unit (SUB (p)));
  } else if (IS (p, TERTIARY)) {
    return (basic_unit (SUB (p)));
  } else if (IS (p, SECONDARY)) {
    return (basic_unit (SUB (p)));
  } else if (IS (p, PRIMARY)) {
    return (basic_unit (SUB (p)));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    return (basic_unit (SUB (p)));
  } else if (IS (p, CLOSED_CLAUSE)) {
    return (basic_serial (NEXT_SUB (p), 1));
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    return (basic_mode (MOID (p)) && basic_collateral (NEXT_SUB (p)));
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    return (basic_mode (MOID (p)) && basic_conditional (SUB (p)));
  } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), IDENTIFIER) != NO_NODE) {
    NODE_T * dst = SUB_SUB (p);
    NODE_T * src = NEXT_NEXT (dst);
    return ((BOOL_T) basic_unit (src) && basic_mode_non_row (MOID (src)));
  } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SLICE) != NO_NODE) {
    NODE_T * dst = SUB_SUB (p);
    NODE_T * src = NEXT_NEXT (dst);
    NODE_T * slice = locate (dst, SLICE);
    return ((BOOL_T) (IS (MOID (slice), REF_SYMBOL) && basic_slice (slice) && basic_unit (src) && basic_mode_non_row (MOID (src))));
  } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SELECTION) != NO_NODE) {
    NODE_T * dst = SUB_SUB (p);
    NODE_T * src = NEXT_NEXT (dst);
    return ((BOOL_T) (locate (NEXT_SUB (locate (dst, SELECTION)), IDENTIFIER) != NO_NODE && basic_unit (src) && basic_mode_non_row (MOID (dst))));
  } else if (IS (p, VOIDING)) {
    return (basic_unit (SUB (p)));
  } else if (IS (p, DEREFERENCING) && locate (SUB (p), IDENTIFIER)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && BASIC (SUB (p), IDENTIFIER)));
  } else if (IS (p, DEREFERENCING) && locate (SUB (p), SLICE)) {
    NODE_T * slice = locate (SUB (p), SLICE);
    return ((BOOL_T) (basic_mode (MOID (p)) && IS (MOID (SUB (slice)), REF_SYMBOL) && basic_slice (slice)));
  } else if (IS (p, DEREFERENCING) && locate (SUB (p), SELECTION)) {
    return ((BOOL_T) (primitive_mode (MOID (p)) && BASIC (SUB (p), SELECTION)));
  } else if (IS (p, WIDENING)) {
    if (WIDEN_TO (p, INT, REAL)) {
      return (basic_unit (SUB (p)));
    } else if (WIDEN_TO (p, INT, LONG_INT)) {
      return (basic_unit (SUB (p)));
    } else if (WIDEN_TO (p, REAL, COMPLEX)) {
      return (basic_unit (SUB (p)));
    } else if (WIDEN_TO (p, REAL, LONG_REAL)) {
      return (basic_unit (SUB (p)));
    } else if (WIDEN_TO (p, LONG_INT, LONG_REAL)) {
      return (basic_unit (SUB (p)));
    } else {
      return (A68_FALSE);
    }
  } else if (IS (p, IDENTIFIER)) {
    if (A68G_STANDENV_PROC (TAX (p))) {
      int k;
      for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
          return (A68_TRUE);
        }
      }
      return (A68_FALSE);
    } else {
      return (basic_mode (MOID (p)));
    }
  } else if (IS (p, DENOTATION)) {
    return (denotation_mode (MOID (p)));
  } else if (IS (p, MONADIC_FORMULA)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_monadic_formula (p)));
  } else if (IS (p, FORMULA)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_formula (p)));
  } else if (IS (p, CALL)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_call (p)));
  } else if (IS (p, CAST)) {
    return ((BOOL_T) (folder_mode (MOID (SUB (p))) && basic_unit (NEXT_SUB (p))));
  } else if (IS (p, SLICE)) {
    return ((BOOL_T) (basic_mode (MOID (p)) && basic_slice (p)));
  } else if (IS (p, SELECTION)) {
    NODE_T * sec = locate (NEXT_SUB (p), IDENTIFIER);
    if (sec == NO_NODE) {
      return (A68_FALSE);
    } else {
      return (basic_mode_non_row (MOID (sec)));
    }
  } else if (IS (p, IDENTITY_RELATION)) {
#define GOOD(p) (locate (p, IDENTIFIER) != NO_NODE && IS (MOID (locate ((p), IDENTIFIER)), REF_SYMBOL))
    NODE_T * lhs = SUB (p);
    NODE_T * rhs = NEXT_NEXT (lhs);
    if (GOOD (lhs) && GOOD (rhs)) {
      return (A68_TRUE);
    } else if (GOOD (lhs) && locate (rhs, NIHIL) != NO_NODE) {
      return (A68_TRUE);
    } else {
      return (A68_FALSE);
    }
#undef GOOD
  } else {
    return (A68_FALSE);
  }
}

/*******************************************************************/
/* Constant folder                                                 */ 
/* Uses interpreter routines to calculate compile-time expressions */
/*******************************************************************/

/***********************/
/* Constant unit check */
/***********************/

/**
@brief Whether constant collateral clause.
@param p Node in syntax tree.
@return See brief description.
**/

static BOOL_T constant_collateral (NODE_T * p)
{
  if (p == NO_NODE) {
    return (A68_TRUE);
  } else if (IS (p, UNIT)) {
    return ((BOOL_T) (folder_mode (MOID (p)) && constant_unit (SUB (p)) && constant_collateral (NEXT (p))));
  } else {
    return ((BOOL_T) (constant_collateral (SUB (p)) && constant_collateral (NEXT (p))));
  }
}

/**
@brief Whether constant serial clause.
@param p Node in syntax tree.
@param total Total units.
@param good Basic units.
@return See brief description.
**/

static void count_constant_units (NODE_T * p, int * total, int * good)
{
  if (p != NO_NODE) {
    if (IS (p, UNIT)) {
      (* total) ++;
      if (constant_unit (p)) {
        (* good) ++;
      }
      count_constant_units (NEXT (p), total, good);
    } else {
      count_constant_units (SUB (p), total, good);
      count_constant_units (NEXT (p), total, good);
    }
  }
}

/**
@brief Whether constant serial clause.
@param p Node in syntax tree.
@param want > 0 is how many units we allow, <= 0 is don't care
@return See brief description.
**/

static BOOL_T constant_serial (NODE_T * p, int want)
{
  int total = 0, good = 0;
  count_constant_units (p, &total, &good);
  if (want > 0) {
    return (total == want && total == good);
  } else {
    return (total == good);
  }
}

/**
@brief Whether constant argument.
@param p Starting node.
@return See brief description.
**/

static BOOL_T constant_argument (NODE_T * p)
{
  if (p == NO_NODE) {
    return (A68_TRUE);
  } else if (IS (p, UNIT)) {
    return ((BOOL_T) (folder_mode (MOID (p)) && constant_unit (p) && constant_argument (NEXT (p))));
  } else {
    return ((BOOL_T) (constant_argument (SUB (p)) && constant_argument (NEXT (p))));
  }
}

/**
@brief Whether constant call.
@param p Starting node.
@return See brief description.
**/

static BOOL_T constant_call (NODE_T * p)
{
  if (IS (p, CALL)) {
    NODE_T * prim = SUB (p);
    NODE_T * idf = locate (prim, IDENTIFIER);
    if (idf != NO_NODE) {
      int k;
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          NODE_T * args = NEXT (prim);
          return (constant_argument (args));
        }
      }
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether constant monadic formula.
@param p Starting node.
@return See brief description.
**/

static BOOL_T constant_monadic_formula (NODE_T * p)
{
  if (IS (p, MONADIC_FORMULA)) {
    NODE_T * op = SUB (p);
    int k;
    for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k ++) {
      if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
        NODE_T * rhs = NEXT (op);
        return (constant_unit (rhs));
      }
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether constant dyadic formula.
@param p Starting node.
@return See brief description.
**/

static BOOL_T constant_formula (NODE_T * p)
{
  if (IS (p, FORMULA)) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NO_NODE) {
      return (constant_monadic_formula (lhs));
    } else {
      int k;
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          NODE_T * rhs = NEXT (op);
          return ((BOOL_T) (constant_unit (lhs) && constant_unit (rhs)));
        }
      }
    }
  }
  return (A68_FALSE);
}

/**
@brief Whether constant unit.
@param p Starting node.
@return See brief description.
**/

BOOL_T constant_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return (A68_FALSE);
  } else if (IS (p, UNIT)) {
    return (constant_unit (SUB (p)));
  } else if (IS (p, TERTIARY)) {
    return (constant_unit (SUB (p)));
  } else if (IS (p, SECONDARY)) {
    return (constant_unit (SUB (p)));
  } else if (IS (p, PRIMARY)) {
    return (constant_unit (SUB (p)));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    return (constant_unit (SUB (p)));
  } else if (IS (p, CLOSED_CLAUSE)) {
    return (constant_serial (NEXT_SUB (p), 1));
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    return (folder_mode (MOID (p)) && constant_collateral (NEXT_SUB (p)));
  } else if (IS (p, WIDENING)) {
    if (WIDEN_TO (p, INT, REAL)) {
      return (constant_unit (SUB (p)));
    } else if (WIDEN_TO (p, INT, LONG_INT)) {
      return (constant_unit (SUB (p)));
    } else if (WIDEN_TO (p, REAL, COMPLEX)) {
      return (constant_unit (SUB (p)));
    } else if (WIDEN_TO (p, REAL, LONG_REAL)) {
      return (constant_unit (SUB (p)));
    } else if (WIDEN_TO (p, LONG_INT, LONG_REAL)) {
      return (constant_unit (SUB (p)));
    } else {
      return (A68_FALSE);
    }
  } else if (IS (p, IDENTIFIER)) {
    if (A68G_STANDENV_PROC (TAX (p))) {
      int k;
      for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
          return (A68_TRUE);
        }
      }
      return (A68_FALSE);
    } else {
/* Possible constant folding */
      NODE_T * def = NODE (TAX (p));
      BOOL_T ret = A68_FALSE;
      if (STATUS (p) & COOKIE_MASK) {
        diagnostic_node (A68_WARNING, p, WARNING_UNINITIALISED);
      } else {
        STATUS (p) |= COOKIE_MASK;
        if (folder_mode (MOID (p)) && def != NO_NODE && NEXT (def) != NO_NODE && IS (NEXT (def), EQUALS_SYMBOL)) {
          ret = constant_unit (NEXT_NEXT (def));
        }
      }
      STATUS (p) &= !(COOKIE_MASK);
      return (ret);
    }
  } else if (IS (p, DENOTATION)) {
    return (denotation_mode (MOID (p)));
  } else if (IS (p, MONADIC_FORMULA)) {
    return ((BOOL_T) (folder_mode (MOID (p)) && constant_monadic_formula (p)));
  } else if (IS (p, FORMULA)) {
    return ((BOOL_T) (folder_mode (MOID (p)) && constant_formula (p)));
  } else if (IS (p, CALL)) {
    return ((BOOL_T) (folder_mode (MOID (p)) && constant_call (p)));
  } else if (IS (p, CAST)) {
    return ((BOOL_T) (folder_mode (MOID (SUB (p))) && constant_unit (NEXT_SUB (p))));
  } else {
    return (A68_FALSE);
  }
}

/*****************************************************************/
/* Evaluate compile-time expressions using interpreter routines. */
/*****************************************************************/

/**
@brief Push denotation.
@param p Node in syntax tree.
**/

static void push_denotation (NODE_T * p)
{
#define PUSH_DENOTATION(mode, decl) {\
  decl z;\
  NODE_T *s = (IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p));\
  if (genie_string_to_value_internal (p, MODE (mode), NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {\
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (mode));\
  }\
  PUSH_PRIMITIVE (p, VALUE (&z), decl);}
/**/
#define PUSH_LONG_DENOTATION(mode, decl) {\
  decl z;\
  NODE_T *s = (IS (SUB (p), LONGETY) ? NEXT_SUB (p) : SUB (p));\
  if (genie_string_to_value_internal (p, MODE (mode), NSYMBOL (s), (BYTE_T *) z) == A68_FALSE) {\
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (mode));\
  }\
  PUSH (p, &z, SIZE (MODE (mode)));}
/**/
  if (MOID (p) == MODE (INT)) {
    PUSH_DENOTATION (INT, A68_INT);
  } else if (MOID (p) == MODE (REAL)) {
    PUSH_DENOTATION (REAL, A68_REAL);
  } else if (MOID (p) == MODE (BOOL)) {
    PUSH_DENOTATION (BOOL, A68_BOOL);
  } else if (MOID (p) == MODE (CHAR)) {
    if ((NSYMBOL (p))[0] == NULL_CHAR) {
      PUSH_PRIMITIVE (p, NULL_CHAR, A68_CHAR);
    } else {
      PUSH_PRIMITIVE (p, (NSYMBOL (p))[0], A68_CHAR);
    }
  } else if (MOID (p) == MODE (BITS)) {
    PUSH_DENOTATION (BITS, A68_BITS);
  } else if (MOID (p) == MODE (LONG_INT)) {
    PUSH_LONG_DENOTATION (LONG_INT, A68_LONG);
  } else if (MOID (p) == MODE (LONG_REAL)) {
    PUSH_LONG_DENOTATION (LONG_REAL, A68_LONG);
  }
#undef PUSH_DENOTATION
#undef PUSH_LONG_DENOTATION
}

/**
@brief Push widening.
@param p Starting node.
**/

static void push_widening (NODE_T * p)
{
  push_unit (SUB (p));
  if (WIDEN_TO (p, INT, REAL)) {
    A68_INT k;
    POP_OBJECT (p, &k, A68_INT);
    PUSH_PRIMITIVE (p, (double) VALUE (&k), A68_REAL);
  } else if (WIDEN_TO (p, REAL, COMPLEX)) {
    PUSH_PRIMITIVE (p, 0.0, A68_REAL);
  } else if (WIDEN_TO (p, INT, LONG_INT)) {
    genie_lengthen_int_to_long_mp (p);    
  } else if (WIDEN_TO (p, REAL, LONG_REAL)) {
    genie_lengthen_real_to_long_mp (p);    
  } else if (WIDEN_TO (p, LONG_INT, LONG_REAL)) {
    /* 1:1 mapping */;
  }
}

/**
@brief Code collateral units.
@param p Starting node.
**/

static void push_collateral_units (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    push_unit (p);
  } else {
    push_collateral_units (SUB (p));
    push_collateral_units (NEXT (p));
  }
}

/**
@brief Code argument.
@param p Starting node.
**/

static void push_argument (NODE_T * p)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT)) {
      push_unit (p);
    } else {
      push_argument (SUB (p));
    }
  }
}

/**
@brief Push unit.
@param p Starting node.
**/

void push_unit (NODE_T * p)
{
  if (p == NO_NODE) {
    return;
  }
  if (IS (p, UNIT)) {
    push_unit (SUB (p));
  } else if (IS (p, TERTIARY)) {
    push_unit (SUB (p));
  } else if (IS (p, SECONDARY)) {
    push_unit (SUB (p));
  } else if (IS (p, PRIMARY)) {
    push_unit (SUB (p));
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    push_unit (SUB (p));
  } else if (IS (p, CLOSED_CLAUSE)) {
    push_unit (SUB (NEXT_SUB (p)));
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    push_collateral_units (NEXT_SUB (p));
  } else if (IS (p, WIDENING)) {
    push_widening (p);
  } else if (IS (p, IDENTIFIER)) {
    if (A68G_STANDENV_PROC (TAX (p))) {
      (void) (*(PROCEDURE (TAX (p)))) (p);
    } else {
    /* Possible constant folding */
      NODE_T * def = NODE (TAX (p));
      push_unit (NEXT_NEXT (def));
    }
  } else if (IS (p, DENOTATION)) {
    push_denotation (p);
  } else if (IS (p, MONADIC_FORMULA)) {
    NODE_T * op = SUB (p);
    NODE_T * rhs = NEXT (op);
    push_unit (rhs);
    (*(PROCEDURE (TAX (op)))) (op);
  } else if (IS (p, FORMULA)) {
    NODE_T * lhs = SUB (p);
    NODE_T * op = NEXT (lhs);
    if (op == NO_NODE) {
      push_unit (lhs);
    } else {
      NODE_T * rhs = NEXT (op);
      push_unit (lhs);
      push_unit (rhs);
      (*(PROCEDURE (TAX (op)))) (op);
    }
  } else if (IS (p, CALL)) {
    NODE_T * prim = SUB (p);
    NODE_T * args = NEXT (prim);
    NODE_T * idf = locate (prim, IDENTIFIER);
    push_argument (args);
    (void) (*(PROCEDURE (TAX (idf)))) (p);
  } else if (IS (p, CAST)) {
    push_unit (NEXT_SUB (p));
  }
}

/**
@brief Code constant folding.
@param p Node to start.
@param out Output file descriptor.
@param phase Phase of code generation.
**/

static void constant_folder (NODE_T * p, FILE_T out, int phase)
{
  if (phase == L_DECLARE) {
    if (MOID (p) == MODE (COMPLEX)) {
      char acc[NAME_SIZE];
      A68_REAL re, im;
      (void) make_name (acc, CON, "", NUMBER (p));
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &im, A68_REAL);
      POP_OBJECT (p, &re, A68_REAL);
      indentf (out, snprintf (line, SNPRINTF_SIZE, "A68_COMPLEX %s = {", acc));
      undentf (out, snprintf (line, SNPRINTF_SIZE, "{INIT_MASK, %.*g}", REAL_WIDTH, VALUE (&re)));
      undentf (out, snprintf (line, SNPRINTF_SIZE, ", {INIT_MASK, %.*g}", REAL_WIDTH, VALUE (&im)));
      undent (out, "};\n");
      ABEND (stack_pointer > 0, "stack not empty", NO_TEXT);
    } else if (LONG_MODE (MOID (p))) {
      char acc[NAME_SIZE];
      A68_LONG z;
      int k;
      (void) make_name (acc, CON, "", NUMBER (p));
      stack_pointer = 0;
      push_unit (p);
      POP (p, &z, SIZE (MOID (p)));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "A68_LONG %s = {INIT_MASK, %.0f", acc, z[1]));
      for (k = 1; k <= LONG_MP_DIGITS; k ++) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, ", %.0f", z[k + 1]));
      } 
      undent (out, "};\n");
      ABEND (stack_pointer > 0, "stack not empty", NO_TEXT);
    }
  } else if (phase == L_EXECUTE) {
    if (MOID (p) == MODE (COMPLEX)) {
      /* Done at declaration stage */
    } else if (LONG_MODE (MOID (p))) {
      /* Done at declaration stage */
    }
  } else if (phase == L_YIELD) {
    if (MOID (p) == MODE (INT)) {
      A68_INT k;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &k, A68_INT);
      ASSERT (snprintf (line, SNPRINTF_SIZE, "%d", VALUE (&k)) >= 0);
      undent (out, line);
      ABEND (stack_pointer > 0, "stack not empty", NO_TEXT);
    } else if (MOID (p) == MODE (REAL)) {
      A68_REAL x;
      double conv;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &x, A68_REAL);
/* Mind overflowing or underflowing values */
      if (VALUE (&x) == DBL_MAX) {
        undent (out, "DBL_MAX");
      } else if (VALUE (&x) == -DBL_MAX) {
        undent (out, "(-DBL_MAX)");
      } else {
        ASSERT (snprintf (line, SNPRINTF_SIZE, "%.*g", REAL_WIDTH, VALUE (&x)) >= 0);
        errno = 0;
        conv = strtod (line, NO_VAR);
        if (errno == ERANGE && conv == 0.0) {
          undent (out, "0.0");
        } else if (errno == ERANGE && conv == HUGE_VAL) {
          diagnostic_node (A68_WARNING, p, WARNING_OVERFLOW, MODE (REAL));
          undent (out, "DBL_MAX");
        } else if (errno == ERANGE && conv == -HUGE_VAL) {
          diagnostic_node (A68_WARNING, p, WARNING_OVERFLOW, MODE (REAL));
          undent (out, "(-DBL_MAX)");
        } else if (errno == ERANGE && conv >= 0) {
          diagnostic_node (A68_WARNING, p, WARNING_UNDERFLOW, MODE (REAL));
          undent (out, "DBL_MIN");
        } else if (errno == ERANGE && conv < 0) {
          diagnostic_node (A68_WARNING, p, WARNING_UNDERFLOW, MODE (REAL));
          undent (out, "(-DBL_MIN)");
        } else {
          if (strchr (line, '.') == NO_TEXT && strchr (line, 'e') == NO_TEXT && strchr (line, 'E') == NO_TEXT) {
            strncat (line, ".0", BUFFER_SIZE);
          }
          undent (out, line);
        }
      }
      ABEND (stack_pointer > 0, "stack not empty", NO_TEXT);
    } else if (MOID (p) == MODE (BOOL)) {
      A68_BOOL b;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &b, A68_BOOL);
      ASSERT (snprintf (line, SNPRINTF_SIZE, "%s", (VALUE (&b) ? "A68_TRUE" : "A68_FALSE")) >= 0);
      undent (out, line);
      ABEND (stack_pointer > 0, "stack not empty", NO_TEXT);
    } else if (MOID (p) == MODE (CHAR)) {
      A68_CHAR c;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &c, A68_CHAR);
      if (VALUE (&c) == '\'') {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "'\\\''"));
      } else if (VALUE (&c) == '\\') {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "'\\\\'"));
      } else if (VALUE (&c) == NULL_CHAR) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "NULL_CHAR"));
      } else if (IS_PRINT (VALUE (&c))) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "'%c'", VALUE (&c)));
      } else {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "(int) 0x%04x", (unsigned) VALUE (&c)));
      }
      ABEND (stack_pointer > 0, "stack not empty", NO_TEXT);
    } else if (MOID (p) == MODE (BITS)) {
      A68_BITS b;
      stack_pointer = 0;
      push_unit (p);
      POP_OBJECT (p, &b, A68_BITS);
      ASSERT (snprintf (line, SNPRINTF_SIZE, "0x%x", VALUE (&b)) >= 0);
      undent (out, line);
      ABEND (stack_pointer > 0, "stack not empty", NO_TEXT);
    } else if (MOID (p) == MODE (COMPLEX)) {
      char acc[NAME_SIZE];
      (void) make_name (acc, CON, "", NUMBER (p));
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(A68_REAL *) %s", acc));
    } else if (LONG_MODE (MOID (p))) {
      char acc[NAME_SIZE];
      (void) make_name (acc, CON, "", NUMBER (p));
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(MP_T *) %s", acc));
    }
  }
}

/********************************************/
/* Auxilliary routines for emitting C code. */
/********************************************/

/**
@brief Whether frame needs initialisation.
@param p Node in syntax tree.
**/

static BOOL_T need_initialise_frame (NODE_T * p)
{
  TAG_T *tag;
  int count;
  for (tag = ANONYMOUS (TABLE (p)); tag != NO_TAG; FORWARD (tag)) {
    if (PRIO (tag) == ROUTINE_TEXT) {
      return (A68_TRUE);
    } else if (PRIO (tag) == FORMAT_TEXT) {
      return (A68_TRUE);
    }
  }
  count = 0;
  genie_find_proc_op (p, &count);
  if (count > 0) {
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/**
@brief Comment source line.
@param p Node in syntax tree.
@param out Output file descriptor.
@param want_space Space required.
@param max_print Maximum items to print.
**/

static void comment_tree (NODE_T * p, FILE_T out, int *want_space, int *max_print)
{
/* Take care not to generate nested comments */
#define UNDENT(out, p) {\
  char * q;\
  for (q = p; q[0] != NULL_CHAR; q ++) {\
    if (q[0] == '*' && q[1] == '/') {\
      undent (out, "\\*\\/");\
      q ++;\
    } else if (q[0] == '/' && q[1] == '*') {\
      undent (out, "\\/\\*");\
      q ++;\
    } else {\
      char w[2];\
      w[0] = q[0];\
      w[1] = NULL_CHAR;\
      undent (out, w);\
    }\
  }}
/**/
  for (; p != NO_NODE && (*max_print) >= 0; FORWARD (p)) {
    if (IS (p, ROW_CHAR_DENOTATION)) {
      if (*want_space != 0) {
        UNDENT (out, " ");
      }
      UNDENT (out, "\"");
      UNDENT (out, NSYMBOL (p));
      UNDENT (out, "\"");
      *want_space = 2;
    } else if (SUB (p) != NO_NODE) {
      comment_tree (SUB (p), out, want_space, max_print);
    } else  if (NSYMBOL (p)[0] == '(' || NSYMBOL (p)[0] == '[' || NSYMBOL (p)[0] == '{') {
      if (*want_space == 2) {
        UNDENT (out, " ");
      }
      UNDENT (out, NSYMBOL (p));
      *want_space = 0;
    } else  if (NSYMBOL (p)[0] == ')' || NSYMBOL (p)[0] == ']' || NSYMBOL (p)[0] == '}') {
      UNDENT (out, NSYMBOL (p));
      *want_space = 1;
    } else  if (NSYMBOL (p)[0] == ';' || NSYMBOL (p)[0] == ',') {
      UNDENT (out, NSYMBOL (p));
      *want_space = 2;
    } else  if (strlen (NSYMBOL (p)) == 1 && (NSYMBOL (p)[0] == '.' || NSYMBOL (p)[0] == ':')) {
      UNDENT (out, NSYMBOL (p));
      *want_space = 2;
    } else {
      if (*want_space != 0) {
        UNDENT (out, " ");
      }
      if ((*max_print) > 0) {
        UNDENT (out, NSYMBOL (p));
      } else if ((*max_print) == 0) {
        if (*want_space == 0) {
          UNDENT (out, " ");
        }
        UNDENT (out, "...");
      }
      (*max_print)--;
      if (IS_UPPER (NSYMBOL (p)[0])) {
        *want_space = 2;
      } else if (! IS_ALNUM (NSYMBOL (p)[0])) {
        *want_space = 2;
      } else {
        *want_space = 1;
      }
    }
  }
#undef UNDENT
}

/**
@brief Comment source line.
@param p Node in syntax tree.
@param out Output file descriptor.
**/

static void comment_source (NODE_T * p, FILE_T out)
{
  int want_space = 0, max_print = 16;
  undentf (out, snprintf (line, SNPRINTF_SIZE, "/* %s: %d: ", FILENAME (LINE (INFO (p))), LINE_NUMBER (p)));
  comment_tree (p, out, &want_space, &max_print);
  undent (out, " */\n");
}

/**
@brief Inline comment source line.
@param p Node in syntax tree.
@param out Output file descriptor.
**/

static void inline_comment_source (NODE_T * p, FILE_T out)
{
  int want_space = 0, max_print = 8;
  undent (out, " /* ");
  comment_tree (p, out, &want_space, &max_print);
  undent (out, " */");
}

/**
@brief Write prelude.
@param out Output file descriptor.
**/

static void write_prelude (FILE_T out)
{
  indentf (out, snprintf (line, SNPRINTF_SIZE, "/* \"%s\" %s */\n\n", FILE_OBJECT_NAME (&program), PACKAGE_STRING));
  if (OPTION_LOCAL (&program)) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "#include \"a68g-config.h\"\n"));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "#include \"a68g.h\"\n\n"));
  } else {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "#include <%s/a68g-config.h>\n", PACKAGE));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "#include <%s/a68g.h>\n\n", PACKAGE));
  }
  indent (out, "#define _CODE_(n) PROP_T n (NODE_T * p) {\\\n");
  indent (out, "  PROP_T self;\n\n");
  indent (out, "#define _EDOC_(n, q) UNIT (&self) = n;\\\n");
  indent (out, "  SOURCE (&self) = q;\\\n");
  indent (out, "  (void) p;\\\n");
  indent (out, "  return (self);}\n\n");
  indent (out, "#define DIV_INT(i, j) ((double) (i) / (double) (j))\n");
  indent (out, "#define _N_(n) (node_register[n])\n");
  indent (out, "#define _S_(z) (STATUS (z))\n");
  indent (out, "#define _V_(z) (VALUE (z))\n\n");
}

/**
@brief Write initialisation of frame.
**/

static void init_static_frame (FILE_T out, NODE_T * p)
{
  if (AP_INCREMENT (TABLE (p)) > 0) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "FRAME_CLEAR (%d);\n", AP_INCREMENT (TABLE (p))));
  }
  if (LEX_LEVEL (p) == global_level) {
    indent (out, "global_pointer = frame_pointer;\n");
  }
  if (need_initialise_frame (p)) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "initialise_frame (_N_ (%d));\n", NUMBER (p)));
  }
}

/********************************/
/* COMPILATION OF PARTIAL UNITS */
/********************************/

/**
@brief Code getting objects from the stack.
@param p Node in syntax tree.
@param out Output file descriptor.
@param dst Where to store.
@param cast Mode to cast to.
**/

static void get_stack (NODE_T * p, FILE_T out, char * dst, char * cast) 
{
  if (DEBUG_LEVEL >= 4) {
    if (LEVEL (GINFO (p)) == global_level) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "GET_GLOBAL (%s, %s, %d);\n", dst, cast, OFFSET (TAX (p))));
    } else {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "GET_FRAME (%s, %s, %d, %d);\n", dst, cast, LEVEL (GINFO (p)), OFFSET (TAX (p))));
    }
  } else {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "GET_FRAME (%s, %s, %d, %d);\n", dst, cast, LEVEL (GINFO (p)), OFFSET (TAX (p))));
  } 
}

/**
@brief Code function prelude.
@param out Output file descriptor.
@param p Node in syntax tree.
@param fn Function name.
**/

static void write_fun_prelude (NODE_T * p, FILE_T out, char * fn)
{
  (void) p;
  indentf (out, snprintf (line, SNPRINTF_SIZE, "_CODE_ (%s)\n", fn));
  indentation ++;
  temp_book_pointer = 0;
}

/**
@brief Code function postlude.
@param out Output file descriptor.
@param p Node in syntax tree.
@param fn Function name.
**/

static void write_fun_postlude (NODE_T * p, FILE_T out, char * fn)
{
  (void) p;
  indentation --;
  indentf (out, snprintf (line, SNPRINTF_SIZE, "_EDOC_ (%s, _N_ (%d))\n\n", fn, NUMBER (p)));
  temp_book_pointer = 0;
}

/**
@brief Code internal a68g mode.
@param m Mode to check.
@return See brief description.
**/

static char *internal_mode (MOID_T * m)
{
  if (m == MODE (INT)) {
    return ("MODE (INT)");
  } else if (m == MODE (REAL)) {
    return ("MODE (REAL)");
  } else if (m == MODE (BOOL)) {
    return ("MODE (BOOL)");
  } else if (m == MODE (CHAR)) {
    return ("MODE (CHAR)");
  } else if (m == MODE (BITS)) {
    return ("MODE (BITS)");
  } else {
    return ("MODE (ERROR)");
  }
}

/**
@brief Code an A68 mode.
@param m Mode to code.
@return Internal identifier for mode.
**/

static char * inline_mode (MOID_T * m)
{
  if (m == MODE (INT)) {
    return ("A68_INT");
  } else if (m == MODE (REAL)) {
    return ("A68_REAL");
  } else if (LONG_MODE (m)) {
    return ("A68_LONG");
  } else if (m == MODE (BOOL)) {
    return ("A68_BOOL");
  } else if (m == MODE (CHAR)) {
    return ("A68_CHAR");
  } else if (m == MODE (BITS)) {
    return ("A68_BITS");
  } else if (m == MODE (COMPLEX)) {
    return ("A68_COMPLEX");
  } else if (IS (m, REF_SYMBOL)) {
    return ("A68_REF");
  } else if (IS (m, ROW_SYMBOL)) {
    return ("A68_ROW");
  } else if (IS (m, PROC_SYMBOL)) {
    return ("A68_PROCEDURE");
  } else if (IS (m, STRUCT_SYMBOL)) {
    return ("A68_STRUCT");
  } else {
    return ("A68_ERROR");
  }
}

/**
@brief Code denotation.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_denotation (NODE_T * p, FILE_T out, int phase)
{
  if (phase == L_DECLARE && LONG_MODE (MOID (p))) {
    char acc[NAME_SIZE];
    A68_LONG z;
    NODE_T *s = IS (SUB (p), LONGETY) ? NEXT_SUB (p) : SUB (p);
    int k;
    (void) make_name (acc, CON, "", NUMBER (p));
    if (genie_string_to_value_internal (p, MOID (p), NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (INT));
    }
    indentf (out, snprintf (line, SNPRINTF_SIZE, "A68_LONG %s = {INIT_MASK, %.0f", acc, z[1]));
    for (k = 1; k <= LONG_MP_DIGITS; k ++) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, ", %.0f", z[k + 1]));
    } 
    undent (out, "};\n");
  }
  if (phase == L_YIELD) {
    if (MOID (p) == MODE (INT)) {
      A68_INT z;
      NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
      char *den = NSYMBOL (s);
      if (genie_string_to_value_internal (p, MODE (INT), den, (BYTE_T *) & z) == A68_FALSE) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (INT));
      }
      undentf (out, snprintf (line, SNPRINTF_SIZE, "%d", VALUE (&z)));
    } else if (MOID (p) == MODE (REAL)) {
      A68_REAL z;
      NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
      char *den = NSYMBOL (s);
      if (genie_string_to_value_internal (p, MODE (REAL), den, (BYTE_T *) & z) == A68_FALSE) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (REAL));
      }
      if (strchr (den, '.') == NO_TEXT && strchr (den, 'e') == NO_TEXT && strchr (den, 'E') == NO_TEXT) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "(double) %s", den));
      } else {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", den));
      }
    } else if (LONG_MODE (MOID (p))) {
      char acc[NAME_SIZE];
      (void) make_name (acc, CON, "", NUMBER (p));
      undent (out, acc);
    } else if (MOID (p) == MODE (BOOL)) {
      undent (out, "(BOOL_T) A68_");
      undent (out, NSYMBOL (p));
    } else if (MOID (p) == MODE (CHAR)) {
      if (NSYMBOL (p)[0] == '\'') {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "'\\''"));
      } else if (NSYMBOL (p)[0] == NULL_CHAR) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "NULL_CHAR"));
      } else if (NSYMBOL (p)[0] == '\\') {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "'\\\\'"));
      } else {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "'%c'", (NSYMBOL (p))[0]));
      }
    } else if (MOID (p) == MODE (BITS)) {
      A68_BITS z;
      NODE_T *s = IS (SUB (p), SHORTETY) ? NEXT_SUB (p) : SUB (p);
      if (genie_string_to_value_internal (p, MODE (BITS), NSYMBOL (s), (BYTE_T *) & z) == A68_FALSE) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_IN_DENOTATION, MODE (BITS));
      }
      ASSERT (snprintf (line, SNPRINTF_SIZE, "(unsigned) 0x%x", VALUE (&z)) >= 0);
      undent (out, line);
    }
  }
}

/**
@brief Code widening.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_widening (NODE_T * p, FILE_T out, int phase)
{
  if (WIDEN_TO (p, INT, REAL)) {
    if (phase == L_DECLARE) {
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      undent (out, "(double) (");
      inline_unit (SUB (p), out, L_YIELD);
      undent (out, ")");
    }
  } else if (WIDEN_TO (p, REAL, COMPLEX)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MODE (COMPLEX)), 0, acc);
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
      indentf (out, snprintf (line, SNPRINTF_SIZE, "STATUS_RE (%s) = INIT_MASK;\n", acc));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "STATUS_IM (%s) = INIT_MASK;\n", acc));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "RE (%s) = (double) (", acc));
      inline_unit (SUB (p), out, L_YIELD);
      undent (out, ");\n");
      indentf (out, snprintf (line, SNPRINTF_SIZE, "IM (%s) = 0.0;\n", acc));
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(A68_REAL *) %s", acc));
    }
  } else if (WIDEN_TO (p, INT, LONG_INT)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MODE (LONG_INT)), 0, acc);
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
      indentf (out, snprintf (line, SNPRINTF_SIZE, "(void) int_to_mp (_N_ (%d), %s, ", NUMBER (p), acc));
      inline_unit (SUB (p), out, L_YIELD);
      undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", LONG_MP_DIGITS));
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(MP_T *) %s", acc));
    }
  } else if (WIDEN_TO (p, REAL, LONG_REAL)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MODE (LONG_REAL)), 0, acc);
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
      indentf (out, snprintf (line, SNPRINTF_SIZE, "(void) real_to_mp (_N_ (%d), %s, ", NUMBER (p), acc));
      inline_unit (SUB (p), out, L_YIELD);
      undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", LONG_MP_DIGITS));
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(MP_T *) %s", acc));
    }
  } else if (WIDEN_TO (p, LONG_INT, LONG_REAL)) {
    inline_unit (SUB (p), out, phase);
  }
}

/**
@brief Code dereferencing of identifier.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_dereference_identifier (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * q = locate (SUB (p), IDENTIFIER);
  ABEND (q == NO_NODE, "not dereferencing an identifier", NO_TEXT);
  if (phase == L_DECLARE) {
    if (signed_in (BOOK_DEREF, L_DECLARE, NSYMBOL (q)) != NO_BOOK) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (p));
      (void) add_declaration (&root_idf, inline_mode (MOID (p)), 1, idf);
      sign_in (BOOK_DEREF, L_DECLARE, NSYMBOL (p), NULL, NUMBER (p));
      inline_unit (SUB (p), out, L_DECLARE);
    }
  } else if (phase == L_EXECUTE) {
    if (signed_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q)) != NO_BOOK) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (p));
      inline_unit (SUB (p), out, L_EXECUTE);
      if (BODY (TAX (q)) != NO_TAG) {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) LOCAL_ADDRESS (", idf, inline_mode (MOID (p))));
        sign_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (p), NULL, NUMBER (p));
        inline_unit (SUB (p), out, L_YIELD);
        undent (out, ");\n");
      } else {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = DEREF (%s, ", idf, inline_mode (MOID (p))));
        sign_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (p), NULL, NUMBER (p));
        inline_unit (SUB (p), out, L_YIELD);
        undent (out, ");\n");
      }
    }
  } else if (phase == L_YIELD) {
    char idf[NAME_SIZE];
    if (signed_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q)) != NO_BOOK) {
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (signed_in (BOOK_DEREF, L_DECLARE, NSYMBOL (q))));
    } else {
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (p));
    }
    if (primitive_mode (MOID (p))) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s)", idf));
    } else if (MOID (p) == MODE (COMPLEX)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(A68_REAL *) (%s)", idf));
    } else if (LONG_MODE (MOID (p))) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(MP_T *) (%s)", idf));
    } else if (basic_mode (MOID (p))) {
      undent (out, idf);
    }
  }
}

/**
@brief Code identifier.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_identifier (NODE_T * p, FILE_T out, int phase)
{
/* Possible constant folding */
  NODE_T * def = NODE (TAX (p));
  if (primitive_mode (MOID (p)) && def != NO_NODE && NEXT (def) != NO_NODE && IS (NEXT (def), EQUALS_SYMBOL)) {
    NODE_T * src = locate (NEXT_NEXT (def), DENOTATION);
    if (src != NO_NODE) {
      inline_denotation (src, out, phase);
      return;
    }
  }
/* No folding - consider identifier */
  if (phase == L_DECLARE) {
    if (signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (p)) != NO_BOOK) {
      return;
    } else if (A68G_STANDENV_PROC (TAX (p))) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      (void) add_declaration (&root_idf, inline_mode (MOID (p)), 1, idf);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (p), NULL, NUMBER (p));
    }
  } else if (phase == L_EXECUTE) {
    if (signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p)) != NO_BOOK) {
      return;
    } else if (A68G_STANDENV_PROC (TAX (p))) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      get_stack (p, out, idf, inline_mode (MOID (p)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), NULL, NUMBER (p));
    }
  } else if (phase == L_YIELD) {
    if (A68G_STANDENV_PROC (TAX (p))) {
      int k;
      for (k = 0; PROCEDURE (&constants[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (p)) == PROCEDURE (&constants[k])) {
          undent (out, CODE (&constants[k]));
          return;
        }
      }
    } else {
      char idf[NAME_SIZE];
      BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p));
      if (entry != NO_BOOK) {
        (void) make_name (idf, NSYMBOL (p), "", NUMBER (entry));
      } else {
        (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      }
      if (primitive_mode (MOID (p))) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s)", idf));
      } else if (MOID (p) == MODE (COMPLEX)) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "(A68_REAL *) (%s)", idf));
      } else if (LONG_MODE (MOID (p))) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "(MP_T *) (%s)", idf));
      } else if (basic_mode (MOID (p))) {
        undent (out, idf);
      }
    }
  }
}

/**
@brief Code indexer.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
@param k Counter.
@param tup Tuple pointer.
**/

static void inline_indexer (NODE_T * p, FILE_T out, int phase, int * k, char * tup)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    if (phase != L_YIELD) {
      inline_unit (p, out, phase);
    } else {
      if ((*k) == 0) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "(SPAN (&%s[%d]) * (", tup, (*k)));
      } else {
        undentf (out, snprintf (line, SNPRINTF_SIZE, " + (SPAN (&%s[%d]) * (", tup, (*k)));
      }
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (line, SNPRINTF_SIZE, ") - SHIFT (&%s[%d]))", tup, (*k)));
    }
    (*k) ++;
  } else {
    inline_indexer (SUB (p), out, phase, k, tup);
    inline_indexer (NEXT (p), out, phase, k, tup);
  }
}

/**
@brief Code dereferencing of slice.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_dereference_slice (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * indx = NEXT (prim);
  MOID_T * row_mode = DEFLEX (MOID (prim));
  MOID_T * mode = SUB_SUB (row_mode);
  char * symbol = NSYMBOL (SUB (prim));
  char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE];
  int k;
  if (phase == L_DECLARE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      (void) add_declaration (&root_idf, "A68_REF", 1, idf);
      (void) add_declaration (&root_idf, "A68_REF", 0, elm);
      (void) add_declaration (&root_idf, "A68_ARRAY", 1, arr);
      (void) add_declaration (&root_idf, "A68_TUPLE", 1, tup);
      (void) add_declaration (&root_idf, inline_mode (mode), 1, drf);
      sign_in (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      (void) add_declaration (&root_idf, "A68_REF", 0, elm);
      (void) add_declaration (&root_idf, inline_mode (mode), 1, drf);
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NO_TEXT);
  } else if (phase == L_EXECUTE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    NODE_T * pidf = locate (prim, IDENTIFIER);
    if (entry == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      get_stack (pidf, out, idf, "A68_REF");
      if (IS (row_mode, REF_SYMBOL) && IS (SUB (row_mode), ROW_SYMBOL)) {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, DEREF (A68_ROW, %s));\n", arr, tup, idf));
      } else {
        ABEND (A68_TRUE, "strange mode in dereference slice (execute)", NO_TEXT);
      }
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (arr, ARR, "", NUMBER (entry));
      (void) make_name (tup, TUP, "", NUMBER (entry));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = ARRAY (%s);\n", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NO_TEXT);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ");\n"));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = DEREF (%s, & %s);\n", drf, inline_mode (mode), elm));
  } else if (phase == L_YIELD) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NO_BOOK && same_tree (indx, (NODE_T *) (INFO (entry))) == A68_TRUE) {
      (void) make_name (drf, DRF, "", NUMBER (entry));
    } else {
      (void) make_name (drf, DRF, "", NUMBER (prim));
    }
    if (primitive_mode (mode)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s)", drf));
    } else if (mode == MODE (COMPLEX)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(A68_REAL *) (%s)", drf));
    } else if (LONG_MODE (mode)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(MP_T *) (%s)", drf));
    } else if (basic_mode (mode)) {
      undent (out, drf);
    } else {
      ABEND (A68_TRUE, "strange mode in dereference slice (yield)", NO_TEXT);
    }
  }
}

/**
@brief Code slice REF [] MODE -> REF MODE.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_slice_ref_to_ref (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * indx = NEXT (prim);
  MOID_T * mode = SUB_MOID (p);
  MOID_T * row_mode = DEFLEX (MOID (prim));
  char * symbol = NSYMBOL (SUB (prim));
  char idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE], drf[NAME_SIZE];
  int k;
  if (phase == L_DECLARE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      /*indentf (out, snprintf (line, SNPRINTF_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, inline_mode (mode), drf, arr, tup));*/
      (void) add_declaration (&root_idf, "A68_REF", 1, idf);
      (void) add_declaration (&root_idf, "A68_REF", 0, elm);
      (void) add_declaration (&root_idf, "A68_ARRAY", 1, arr);
      (void) add_declaration (&root_idf, "A68_TUPLE", 1, tup);
      (void) add_declaration (&root_idf, inline_mode (mode), 1, drf);
      sign_in (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      /*indentf (out, snprintf (line, SNPRINTF_SIZE, "A68_REF %s; %s * %s;\n", elm, inline_mode (mode), drf));*/
      (void) add_declaration (&root_idf, "A68_REF", 0, elm);
      (void) add_declaration (&root_idf, inline_mode (mode), 1, drf);
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NO_TEXT);
  } else if (phase == L_EXECUTE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry == NO_BOOK) {
      NODE_T * pidf = locate (prim, IDENTIFIER);
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      get_stack (pidf, out, idf, "A68_REF");
      if (IS (row_mode, REF_SYMBOL) && IS (SUB (row_mode), ROW_SYMBOL)) {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, DEREF (A68_ROW, %s));\n", arr, tup, idf));
      } else {
        ABEND (A68_TRUE, "strange mode in slice (execute)", NO_TEXT);
      }
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (arr, ARR, "", NUMBER (entry));
      (void) make_name (tup, TUP, "", NUMBER (entry));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = ARRAY (%s);\n", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NO_TEXT);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ");\n"));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = DEREF (%s, & %s);\n", drf, inline_mode (mode), elm));
  } else if (phase == L_YIELD) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NO_BOOK && same_tree (indx, (NODE_T *) (INFO (entry))) == A68_TRUE) {
      (void) make_name (elm, ELM, "", NUMBER (entry));
    } else {
      (void) make_name (elm, ELM, "", NUMBER (prim));
    }
    undentf (out, snprintf (line, SNPRINTF_SIZE, "(&%s)", elm));
  }
}

/**
@brief Code slice [] MODE -> MODE.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_slice (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * indx = NEXT (prim);
  MOID_T * mode = MOID (p);
  MOID_T * row_mode = DEFLEX (MOID (prim));
  char * symbol = NSYMBOL (SUB (prim));
  char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE];
  int k;
  if (phase == L_DECLARE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_DECLARE, symbol);
    if (entry == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "A68_REF * %s, %s; %s * %s; A68_ARRAY * %s; A68_TUPLE * %s;\n", idf, elm, inline_mode (mode), drf, arr, tup));
      sign_in (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "A68_REF %s; %s * %s;\n", elm, inline_mode (mode), drf));
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NO_TEXT);
  } else if (phase == L_EXECUTE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry == NO_BOOK) {
      NODE_T * pidf = locate (prim, IDENTIFIER);
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      get_stack (pidf, out, idf, "A68_REF");
      if (IS (row_mode, REF_SYMBOL)) {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, DEREF (A68_ROW, %s));\n", arr, tup, idf));
      } else {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, (A68_ROW *) %s);\n", arr, tup, idf));
      }
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), (void *) indx, NUMBER (prim));
    } else if (same_tree (indx, (NODE_T *) (INFO (entry))) == A68_FALSE) {
      (void) make_name (arr, ARR, "", NUMBER (entry));
      (void) make_name (tup, TUP, "", NUMBER (entry));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
    } else {
      return;
    }
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = ARRAY (%s);\n", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NO_TEXT);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ");\n"));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = DEREF (%s, & %s);\n", drf, inline_mode (mode), elm));
  } else if (phase == L_YIELD) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, symbol);
    if (entry != NO_BOOK && same_tree (indx, (NODE_T *) (INFO (entry))) == A68_TRUE) {
      (void) make_name (drf, DRF, "", NUMBER (entry));
    } else {
      (void) make_name (drf, DRF, "", NUMBER (prim));
    }
    if (primitive_mode (mode)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s)", drf));
    } else if (mode == MODE (COMPLEX)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(A68_REAL *) (%s)", drf));
    } else if (LONG_MODE (mode)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(MP_T *) (%s)", drf));
    } else if (basic_mode (mode)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", drf));
    } else {
      ABEND (A68_TRUE, "strange mode in slice (yield)", NO_TEXT);
    }
  }
}

/**
@brief Code monadic formula.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_monadic_formula (NODE_T * p, FILE_T out, int phase)
{
  NODE_T *op = SUB (p);
  NODE_T * rhs = NEXT (op);
  if (IS (p, MONADIC_FORMULA) && MOID (p) == MODE (COMPLEX)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MODE (COMPLEX)), 0, acc);
      inline_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_unit (rhs, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
          indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (%s, ", CODE (&monadics[k]), acc));
          inline_unit (rhs, out, L_YIELD);
          undentf (out, snprintf (line, SNPRINTF_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", acc));
    }
  } else if (IS (p, MONADIC_FORMULA) && LONG_MODE (MOID (rhs))) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MOID (p)), 0, acc);
      inline_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_unit (rhs, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
          if (LONG_MODE (MOID (p))) {
            indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (_N_ (%d), %s, ", CODE (&monadics[k]), NUMBER (op), acc));
          } else {
            indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (_N_ (%d), & %s, ", CODE (&monadics[k]), NUMBER (op), acc));
          }
          inline_unit (rhs, out, L_YIELD);
          undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", LONG_MP_DIGITS));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", acc));
    }
  } else if (IS (p, MONADIC_FORMULA) && basic_mode (MOID (p))) {
    if (phase != L_YIELD) {
      inline_unit (rhs, out, phase);
    } else {
      int k;
      for (k = 0; PROCEDURE (&monadics[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&monadics[k])) {
          if (IS_ALNUM ((CODE (&monadics[k]))[0])) {
            undent (out, CODE (&monadics[k]));
            undent (out, "(");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          } else {
            undent (out, CODE (&monadics[k]));
            undent (out, "(");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          }
        }
      }
    }
  }
}

/**
@brief Code dyadic formula.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_formula (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * lhs = SUB (p), *rhs;
  NODE_T * op = NEXT (lhs);
  if (IS (p, FORMULA) && op == NO_NODE) {
    inline_monadic_formula (lhs, out, phase);
    return;
  }
  rhs = NEXT (op);
  if (IS (p, FORMULA) && MOID (p) == MODE (COMPLEX)) {
    if (op == NO_NODE) {
      inline_monadic_formula (lhs, out, phase);
    } else if (phase == L_DECLARE) {
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      (void) add_declaration (&root_idf, inline_mode (MOID (p)), 0, acc);
      inline_unit (lhs, out, L_DECLARE);
      inline_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      char acc[NAME_SIZE];
      int k;
      (void) make_name (acc, TMP, "", NUMBER (p));
      inline_unit (lhs, out, L_EXECUTE);
      inline_unit (rhs, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          if (MOID (p) == MODE (COMPLEX)) {
            indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (%s, ", CODE (&dyadics[k]), acc));
          } else {
            indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (& %s, ", CODE (&dyadics[k]), acc));
          }
          inline_unit (lhs, out, L_YIELD);
          undentf (out, snprintf (line, SNPRINTF_SIZE, ", "));
          inline_unit (rhs, out, L_YIELD);
          undentf (out, snprintf (line, SNPRINTF_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      char acc[NAME_SIZE];
      (void) make_name (acc, TMP, "", NUMBER (p));
      if (MOID (p) == MODE (COMPLEX)) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", acc));
      } else {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (& %s)", acc));
      }
    }
  } else if (IS (p, FORMULA) && LONG_MODE (MOID (lhs)) && LONG_MODE (MOID (rhs))) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MOID (p)), 0, acc);
      inline_unit (lhs, out, L_DECLARE);
      inline_unit (rhs, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_unit (lhs, out, L_EXECUTE);
      inline_unit (rhs, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          if (LONG_MODE (MOID (p))) {
            indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (_N_ (%d), %s, ", CODE (&dyadics[k]), NUMBER (op), acc));
          } else {
            indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (_N_ (%d), & %s, ", CODE (&dyadics[k]), NUMBER (op), acc));
          }
          inline_unit (lhs, out, L_YIELD);
          undentf (out, snprintf (line, SNPRINTF_SIZE, ", "));
          inline_unit (rhs, out, L_YIELD);
          undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", LONG_MP_DIGITS));
        }
      }
    } else if (phase == L_YIELD) {
      if (LONG_MODE (MOID (p))) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", acc));
      } else {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (& %s)", acc));
      }
    }
  } else if (IS (p, FORMULA) && basic_mode (MOID (p))) {
    if (phase != L_YIELD) {
      inline_unit (lhs, out, phase);
      inline_unit (rhs, out, phase);
    } else {
      int k;
      for (k = 0; PROCEDURE (&dyadics[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (op)) == PROCEDURE (&dyadics[k])) {
          if (IS_ALNUM ((CODE (&dyadics[k]))[0])) {
            undent (out, CODE (&dyadics[k]));
            undent (out, "(");
            inline_unit (lhs, out, L_YIELD);
            undent (out, ", ");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          } else {
            undent (out, "(");
            inline_unit (lhs, out, L_YIELD);
            undent (out, " ");
            undent (out, CODE (&dyadics[k]));
            undent (out, " ");
            inline_unit (rhs, out, L_YIELD);
            undent (out, ")");
          }
        }
      }
    }
  }
}

/**
@brief Code argument.
@param p Starting node.
@param out Output file descriptor.
@param phase Phase of code generation.
@return See brief description.
**/

static void inline_single_argument (NODE_T * p, FILE_T out, int phase)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ARGUMENT_LIST) || IS (p, ARGUMENT)) {
      inline_single_argument (SUB (p), out, phase);
    } else if (IS (p, GENERIC_ARGUMENT_LIST) || IS (p, GENERIC_ARGUMENT)) {
      inline_single_argument (SUB (p), out, phase);
    } else if (IS (p, UNIT)) {
      inline_unit (p, out, phase);
    }
  }
}

/**
@brief Code call.
@param p Starting node.
@param out Output file descriptor.
@param phase Phase of code generation.
@return See brief description.
**/

static void inline_call (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * prim = SUB (p);
  NODE_T * args = NEXT (prim);
  NODE_T * idf = locate (prim, IDENTIFIER);
  if (MOID (p) == MODE (COMPLEX)) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MODE (COMPLEX)), 0, acc);
      inline_single_argument (args, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_single_argument (args, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (%s, ", CODE (&functions[k]), acc));
          inline_single_argument (args, out, L_YIELD);
          undentf (out, snprintf (line, SNPRINTF_SIZE, ");\n"));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", acc));
    }
  } else if (LONG_MODE (MOID (p))) {
    char acc[NAME_SIZE];
    (void) make_name (acc, TMP, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MOID (p)), 0, acc);
      inline_single_argument (args, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      int k;
      inline_single_argument (args, out, L_EXECUTE);
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          indentf (out, snprintf (line, SNPRINTF_SIZE, "%s (_N_ (%d), %s, ", CODE (&functions[k]), NUMBER (idf), acc));
          inline_single_argument (args, out, L_YIELD);
          undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", LONG_MP_DIGITS));
        }
      }
    } else if (phase == L_YIELD) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", acc));
    }
  } else if (basic_mode (MOID (p))) {
    if (phase != L_YIELD) {
      inline_single_argument (args, out, phase);
    } else {
      int k;
      for (k = 0; PROCEDURE (&functions[k]) != NO_GPROC; k ++) {
        if (PROCEDURE (TAX (idf)) == PROCEDURE (&functions[k])) {
          undent (out, CODE (&functions[k]));
          undent (out, " (");
          inline_single_argument (args, out, L_YIELD);
          undent (out, ")");
        }
      }
    }
  }
}

/**
@brief Code collateral units.
@param out Output file descriptor.
@param p Starting node.
@param phase Phase of compilation.
**/

static void inline_collateral_units (NODE_T * p, FILE_T out, int phase)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT)) {
    if (phase == L_DECLARE) {
      inline_unit (SUB (p), out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      inline_unit (SUB (p), out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "PUSH_PRIMITIVE (p, "));
      inline_unit (SUB (p), out, L_YIELD);
      undentf (out, snprintf (line, SNPRINTF_SIZE, ", %s);\n", inline_mode (MOID (p))));
    }
  } else {
    inline_collateral_units (SUB (p), out, phase);
    inline_collateral_units (NEXT (p), out, phase);
  }
}

/**
@brief Code collateral units.
@param out Output file descriptor.
@param p Starting node.
@param phase Compilation phase.
**/

static void inline_collateral (NODE_T * p, FILE_T out, int phase)
{
  char dsp[NAME_SIZE];
  (void) make_name (dsp, DSP, "", NUMBER (p));
  if (p == NO_NODE) {
    return;
  } else if (phase == L_DECLARE) {
    if (MOID (p) == MODE (COMPLEX)) {
      (void) add_declaration (&root_idf, inline_mode (MODE (REAL)), 1, dsp);
    } else {
      (void) add_declaration (&root_idf, inline_mode (MOID (p)), 1, dsp);
    }
    inline_collateral_units (NEXT_SUB (p), out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    if (MOID (p) == MODE (COMPLEX)) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) STACK_TOP;\n", dsp, inline_mode (MODE (REAL))));
    } else {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) STACK_TOP;\n", dsp, inline_mode (MOID (p))));
    }
    inline_collateral_units (NEXT_SUB (p), out, L_EXECUTE);
    inline_collateral_units (NEXT_SUB (p), out, L_YIELD);
  } else if (phase == L_YIELD) {
    undentf (out, snprintf (line, SNPRINTF_SIZE, "%s", dsp));
  }
}

/**
@brief Code basic closed clause.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_closed (NODE_T * p, FILE_T out, int phase)
{
  if (p == NO_NODE) {
    return;
  } else if (phase != L_YIELD) {
    inline_unit (SUB (NEXT_SUB (p)), out, phase);
  } else {
    undent (out, "(");
    inline_unit (SUB (NEXT_SUB (p)), out, L_YIELD);
    undent (out, ")");
  }
}

/**
@brief Code basic closed clause.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_conditional (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * if_part = NO_NODE, * then_part = NO_NODE, * else_part = NO_NODE;
  p = SUB (p);
  if (IS (p, IF_PART) || IS (p, OPEN_PART)) {
    if_part = p;
  } else {
    ABEND (A68_TRUE, "if-part expected", NO_TEXT);
  }
  FORWARD (p);
  if (IS (p, THEN_PART) || IS (p, CHOICE)) {
    then_part = p;
  } else {
    ABEND (A68_TRUE, "then-part expected", NO_TEXT);
  }
  FORWARD (p);
  if (IS (p, ELSE_PART) || IS (p, CHOICE)) {
    else_part = p;
  } else {
    else_part = NO_NODE;
  }
  if (phase == L_DECLARE) {
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_DECLARE);
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_DECLARE);
    inline_unit (SUB (NEXT_SUB (else_part)), out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_EXECUTE);
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_EXECUTE);
    inline_unit (SUB (NEXT_SUB (else_part)), out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    undent (out, "(");
    inline_unit (SUB (NEXT_SUB (if_part)), out, L_YIELD);
    undent (out, " ? ");
    inline_unit (SUB (NEXT_SUB (then_part)), out, L_YIELD);
    undent (out, " : ");
    if (else_part != NO_NODE) {
      inline_unit (SUB (NEXT_SUB (else_part)), out, L_YIELD);
    } else {
/*
This is not an ideal solution although RR permits it;
an omitted else-part means SKIP: yield some value of the
mode required.
*/
      inline_unit (SUB (NEXT_SUB (then_part)), out, L_YIELD);
    }
    undent (out, ")");
  }
}

/**
@brief Code dereferencing of selection.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_dereference_selection (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * field = SUB (p);
  NODE_T * sec = NEXT (field);
  NODE_T * idf = locate (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char * field_idf = NSYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) add_declaration (&root_idf, "A68_REF", 1, ref);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), NULL, NUMBER (field));
    }
    if (entry == NO_BOOK || field_idf != (char *) (INFO (entry))) {
      (void) make_name (sel, SEL, "", NUMBER (field));
      (void) add_declaration (&root_idf, inline_mode (SUB_MOID (field)), 1, sel);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      get_stack (idf, out, ref, "A68_REF");
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), NULL, NUMBER (field));
    }
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else if (field_idf != (char *) (INFO (entry))) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry != NO_BOOK && (char *) (INFO (entry)) == field_idf) {
      (void) make_name (sel, SEL, "", NUMBER (entry));
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (SUB_MOID (p))) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s)", sel));
    } else if (SUB_MOID (p) == MODE (COMPLEX)) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(A68_REAL *) (%s)", sel));
    } else if (LONG_MODE (SUB_MOID (p))) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(MP_T *) (%s)", sel));
    } else if (basic_mode (SUB_MOID (p))) {
      undent (out, sel);
    } else {
      ABEND (A68_TRUE, "strange mode in dereference selection (yield)", NO_TEXT);
    }
  }
}

/**
@brief Code selection.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_selection (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * field = SUB (p);
  NODE_T * sec = NEXT (field);
  NODE_T * idf = locate (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char * field_idf = NSYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) add_declaration (&root_idf, "A68_STRUCT", 0, ref);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), NULL, NUMBER (field));
    }
    if (entry == NO_BOOK || field_idf != (char *) (INFO (entry))) {
      (void) make_name (sel, SEL, "", NUMBER (field));
      (void) add_declaration (&root_idf, inline_mode (MOID (field)), 1, sel);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      get_stack (idf, out, ref, "BYTE_T");
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) & (%s[%d]);\n", sel, inline_mode (MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else if (field_idf != (char *) (INFO (entry))) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) & (%s[%d]);\n", sel, inline_mode (MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry != NO_BOOK && (char *) (INFO (entry)) == field_idf) {
      (void) make_name (sel, SEL, "", NUMBER (entry));
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (MOID (p))) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s)", sel));
    } else {
      ABEND (A68_TRUE, "strange mode in selection (yield)", NO_TEXT);
    }
  }
}

/**
@brief Code selection.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_selection_ref_to_ref (NODE_T * p, FILE_T out, int phase)
{
  NODE_T * field = SUB (p);
  NODE_T * sec = NEXT (field);
  NODE_T * idf = locate (sec, IDENTIFIER);
  char ref[NAME_SIZE], sel[NAME_SIZE];
  char * field_idf = NSYMBOL (SUB (field));
  if (phase == L_DECLARE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) add_declaration (&root_idf, "A68_REF", 1, ref);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), NULL, NUMBER (field));
    }
    if (entry == NO_BOOK || field_idf != (char *) (INFO (entry))) {
      (void) make_name (sel, SEL, "", NUMBER (field));
      (void) add_declaration (&root_idf, "A68_REF", 0, sel);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (sec, out, L_DECLARE);
  } else if (phase == L_EXECUTE) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE_2, NSYMBOL (idf));
    if (entry == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      get_stack (idf, out, ref, "A68_REF");
      (void) make_name (sel, SEL, "", NUMBER (field));
      sign_in (BOOK_DECL, L_EXECUTE_2, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else if (field_idf != (char *) (INFO (entry))) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (entry));
      (void) make_name (sel, SEL, "", NUMBER (field));
      sign_in (BOOK_DECL, L_EXECUTE_2, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = *%s;\n", sel, ref));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OFFSET (&%s) += %d;\n", sel, OFFSET_OFF (field)));
    inline_unit (sec, out, L_EXECUTE);
  } else if (phase == L_YIELD) {
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf));
    if (entry != NO_BOOK && (char *) (INFO (entry)) == field_idf) {
      (void) make_name (sel, SEL, "", NUMBER (entry));
    } else {
      (void) make_name (sel, SEL, "", NUMBER (field));
    }
    if (primitive_mode (SUB_MOID (p))) {
      undentf (out, snprintf (line, SNPRINTF_SIZE, "(&%s)", sel));
    } else {
      ABEND (A68_TRUE, "strange mode in selection (yield)", NO_TEXT);
    }
  }
}

/**
@brief Code identifier.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_ref_identifier (NODE_T * p, FILE_T out, int phase)
{
/* No folding - consider identifier */
  if (phase == L_DECLARE) {
    if (signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (p)) != NO_BOOK) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      (void) add_declaration (&root_idf, "A68_REF", 1, idf);
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (p), NULL, NUMBER (p));
    }
  } else if (phase == L_EXECUTE) {
    if (signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p)) != NO_BOOK) {
      return;
    } else {
      char idf[NAME_SIZE];
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
      get_stack (p, out, idf, "A68_REF");
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), NULL, NUMBER (p));
    }
  } else if (phase == L_YIELD) {
    char idf[NAME_SIZE];
    BOOK_T * entry = signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p));
    if (entry != NO_BOOK) {
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (entry));
    } else {
      (void) make_name (idf, NSYMBOL (p), "", NUMBER (p));
    }
    undent (out, idf);
  }
}

/**
@brief Code identity-relation.
@param p Starting node.
@param out Output file descriptor.
@param phase Phase of code generation.
**/

static void inline_identity_relation (NODE_T * p, FILE_T out, int phase)
{
#define GOOD(p) (locate (p, IDENTIFIER) != NO_NODE && IS (MOID (locate ((p), IDENTIFIER)), REF_SYMBOL))
  NODE_T * lhs = SUB (p);
  NODE_T * op = NEXT (lhs);
  NODE_T * rhs = NEXT (op);
  if (GOOD (lhs) && GOOD (rhs)) {
    if (phase == L_DECLARE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_DECLARE);
      inline_ref_identifier (ridf, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_EXECUTE);
      inline_ref_identifier (ridf, out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      NODE_T * ridf = locate (rhs, IDENTIFIER);
      if (IS (op, IS_SYMBOL)) {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "ADDRESS ("));
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf (out, snprintf (line, SNPRINTF_SIZE, ") == ADDRESS ("));
        inline_ref_identifier (ridf, out, L_YIELD);
        undentf (out, snprintf (line, SNPRINTF_SIZE, ")"));
      } else {
        undentf (out, snprintf (line, SNPRINTF_SIZE, "ADDRESS ("));
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf (out, snprintf (line, SNPRINTF_SIZE, ") != ADDRESS ("));
        inline_ref_identifier (ridf, out, L_YIELD);
        undentf (out, snprintf (line, SNPRINTF_SIZE, ")"));
      }
    }
  } else if (GOOD (lhs) && locate (rhs, NIHIL) != NO_NODE) {
    if (phase == L_DECLARE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_DECLARE);
    } else if (phase == L_EXECUTE) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      inline_ref_identifier (lidf, out, L_EXECUTE);
    } else if (phase == L_YIELD) {
      NODE_T * lidf = locate (lhs, IDENTIFIER);
      if (IS (op, IS_SYMBOL)) {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "IS_NIL (*"));
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf (out, snprintf (line, SNPRINTF_SIZE, ")"));
      } else {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "!IS_NIL (*"));
        inline_ref_identifier (lidf, out, L_YIELD);
        undentf (out, snprintf (line, SNPRINTF_SIZE, ")"));
      }
    }
  }
#undef GOOD
}

/**
@brief Code unit.
@param p Starting node.
@param out Object file.
@param phase Phase of code generation.
**/

static void inline_unit (NODE_T * p, FILE_T out, int phase)
{
  if (p == NO_NODE) {
    return;
  } else if (constant_unit (p) && locate (p, DENOTATION) == NO_NODE) {
    constant_folder (p, out, phase);
  } else if (IS (p, UNIT)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, TERTIARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, SECONDARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, PRIMARY)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, ENCLOSED_CLAUSE)) {
    inline_unit (SUB (p), out, phase);
  } else if (IS (p, CLOSED_CLAUSE)) {
    inline_closed (p, out, phase);
  } else if (IS (p, COLLATERAL_CLAUSE)) {
    inline_collateral (p, out, phase);
  } else if (IS (p, CONDITIONAL_CLAUSE)) {
    inline_conditional (p, out, phase);
  } else if (IS (p, WIDENING)) {
    inline_widening (p, out, phase);
  } else if (IS (p, IDENTIFIER)) {
    inline_identifier (p, out, phase);
  } else if (IS (p, DEREFERENCING) && locate (SUB (p), IDENTIFIER) != NO_NODE) {
    inline_dereference_identifier (p, out, phase);
  } else if (IS (p, SLICE)) {
    NODE_T * prim = SUB (p);
    MOID_T * mode = MOID (p);
    MOID_T * row_mode = DEFLEX (MOID (prim));
    if (mode == SUB (row_mode)) {
      inline_slice (p, out, phase);
    } else if (IS (mode, REF_SYMBOL) && IS (row_mode, REF_SYMBOL) && SUB (mode) == SUB_SUB (row_mode)) {
      inline_slice_ref_to_ref (p, out, phase);
    } else {
      ABEND (A68_TRUE, "strange mode for slice", NO_TEXT);
    }
  } else if (IS (p, DEREFERENCING) && locate (SUB (p), SLICE) != NO_NODE) {
    inline_dereference_slice (SUB (p), out, phase);
  } else if (IS (p, DEREFERENCING) && locate (SUB (p), SELECTION) != NO_NODE) {
    inline_dereference_selection (SUB (p), out, phase);
  } else if (IS (p, SELECTION)) {
    NODE_T * sec = NEXT_SUB (p);
    MOID_T * mode = MOID (p);
    MOID_T * struct_mode = MOID (sec);
    if (IS (struct_mode, REF_SYMBOL) && IS (mode, REF_SYMBOL)) {
      inline_selection_ref_to_ref (p, out, phase);
    } else if (IS (struct_mode, STRUCT_SYMBOL) && primitive_mode (mode)) {
      inline_selection (p, out, phase);
    } else {
      ABEND (A68_TRUE, "strange mode for selection", NO_TEXT);
    }
  } else if (IS (p, DENOTATION)) {
    inline_denotation (p, out, phase);
  } else if (IS (p, MONADIC_FORMULA)) {
    inline_monadic_formula (p, out, phase);
  } else if (IS (p, FORMULA)) {
    inline_formula (p, out, phase);
  } else if (IS (p, CALL)) {
    inline_call (p, out, phase);
  } else if (IS (p, CAST)) {
    inline_unit (NEXT_SUB (p), out, phase);
  } else if (IS (p, IDENTITY_RELATION)) {
    inline_identity_relation (p, out, phase);
  }
}

/*********************************/
/* COMPILATION OF COMPLETE UNITS */
/*********************************/

/**
@brief Compile code clause.
@param out Output file descriptor.
@param p Starting node.
@return Function name or NO_NODE.
**/

static void embed_code_clause (NODE_T * p, FILE_T out)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, ROW_CHAR_DENOTATION)) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s\n", NSYMBOL (p)));
    }
    embed_code_clause (SUB (p), out);
  }
}

/**
@brief Compile push.
@param p Starting node.
@param out Output file descriptor.
**/

static void compile_push (NODE_T * p, FILE_T out) {
  if (primitive_mode (MOID (p))) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "PUSH_PRIMITIVE (p, "));
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ", %s);\n", inline_mode (MOID (p))));
  } else if (basic_mode (MOID (p))) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "MOVE ((void *) STACK_TOP, (void *) "));
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", SIZE (MOID (p))));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer += %d;\n", SIZE (MOID (p))));
  } else {
    ABEND (A68_TRUE, "cannot push", moid_to_string (MOID (p), 80, NO_NODE));
  }
}

/**
@brief Compile assign (C source to C destination).
@param p Starting node.
@param out Output file descriptor.
@param dst String denoting destination.
**/

static void compile_assign (NODE_T * p, FILE_T out, char * dst) {
  if (primitive_mode (MOID (p))) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "_S_ (%s) = INIT_MASK;\n", dst));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s) = ", dst));
    inline_unit (p, out, L_YIELD);
    undent (out, ";\n");
  } else if (LONG_MODE (MOID (p))) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "MOVE_MP ((void *) %s, (void *) ", dst));
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", LONG_MP_DIGITS));
  } else if (basic_mode (MOID (p))) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "MOVE ((void *) %s, (void *) ", dst));
    inline_unit (p, out, L_YIELD);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", SIZE (MOID (p))));
  } else {
    ABEND (A68_TRUE, "cannot assign", moid_to_string (MOID (p), 80, NO_NODE));
  }
}

/**
@brief Compile denotation.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_denotation (NODE_T * p, FILE_T out, int compose_fun)
{
  if (denotation_mode (MOID (p))) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_denotation", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    if (primitive_mode (MOID (p))) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "PUSH_PRIMITIVE (p, "));
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (line, SNPRINTF_SIZE, ", %s);\n", inline_mode (MOID (p))));
    } else {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "PUSH (p, "));
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", SIZE (MOID (p))));
    }
    (void) make_name (fn, "_denotation", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile cast.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_cast (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_cast", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (NEXT_SUB (p), out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (NEXT_SUB (p), out, L_EXECUTE);
    compile_push (NEXT_SUB (p), out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_cast", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile identifier.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_identifier", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    (void) make_name (fn, "_identifier", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile dereference identifier.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_dereference_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_deref_identifier", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    (void) make_name (fn, "_deref_identifier", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile slice.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_slice", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_slice", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile slice.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_dereference_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_deref_slice", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_deref_slice", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile selection.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_selection", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_selection", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile selection.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_dereference_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_mode (MOID (p)) && basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_deref_selection", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_deref_selection", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile formula.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_formula (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_formula", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (p, out, L_EXECUTE);
    compile_push (p, out);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_formula", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile voiding formula.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_voiding_formula (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p)) {
    static char fn[NAME_SIZE];
    char pop[NAME_SIZE];
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, "_void_formula", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
    inline_unit (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
    inline_unit (p, out, L_EXECUTE);
    indent (out, "(void) (");
    inline_unit (p, out, L_YIELD);
    undent (out, ");\n");
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_void_formula", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile uniting.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_uniting (NODE_T * p, FILE_T out, int compose_fun)
{
  MOID_T *u = MOID (p), *v = MOID (SUB (p));
  NODE_T *q = SUB (p);
  if (basic_unit (q) && ATTRIBUTE (v) != UNION_SYMBOL && primitive_mode (v)) {
    static char fn[NAME_SIZE];
    char pop0[NAME_SIZE];
    (void) make_name (pop0, PUP, "0", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, "_unite", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop0);
    inline_unit (q, out, L_DECLARE);
    print_declarations (out, root_idf);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop0));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "PUSH_UNION (_N_ (%d), %s);\n", NUMBER (p), internal_mode (v)));
    inline_unit (q, out, L_EXECUTE);
    compile_push (q, out);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s + %d;\n", pop0, SIZE (u)));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile inline arguments.
@param p Starting node.
@param out Output file descriptor.
@param phase Compilation phase.
@param size Position in frame stack.
**/

static void inline_arguments (NODE_T * p, FILE_T out, int phase, int * size)
{
  if (p == NO_NODE) {
    return;
  } else if (IS (p, UNIT) && phase == L_PUSH) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "EXECUTE_UNIT_TRACE (_N_ (%d));\n", NUMBER (p)));
    inline_arguments (NEXT (p), out, L_PUSH, size);
  } else if (IS (p, UNIT)) {
    char arg[NAME_SIZE];
    (void) make_name (arg, ARG, "", NUMBER (p));
    if (phase == L_DECLARE) {
      (void) add_declaration (&root_idf, inline_mode (MOID (p)), 1, arg);
      inline_unit (p, out, L_DECLARE);
    } else if (phase == L_INITIALISE) {
      inline_unit (p, out, L_EXECUTE);
    } else if (phase == L_EXECUTE) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) FRAME_OBJECT (%d);\n", arg, inline_mode (MOID (p)), *size));
      (*size) += SIZE (MOID (p));
    } else if (phase == L_YIELD && primitive_mode (MOID (p))) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "_S_ (%s) = INIT_MASK;\n", arg));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s) = ", arg));
      inline_unit (p, out, L_YIELD);
      undent (out, ";\n");
    } else if (phase == L_YIELD && basic_mode (MOID (p))) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "MOVE ((void *) %s, (void *) ", arg));
      inline_unit (p, out, L_YIELD);
      undentf (out, snprintf (line, SNPRINTF_SIZE, ", %d);\n", SIZE (MOID (p))));
    }
  } else {
    inline_arguments (SUB (p), out, phase, size);
    inline_arguments (NEXT (p), out, phase, size);
  }
}

/**
@brief Compile deproceduring.
@param out Output file descriptor.
@param p Starting node.
@param compose_fun Whether to compose a function.
@return Function name or NO_NODE.
**/

static char * compile_deproceduring (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * idf = locate (SUB (p), IDENTIFIER);
  if (idf == NO_NODE) {
    return (NO_TEXT);
  } else if (! (SUB_MOID (idf) == MODE (VOID) || basic_mode (SUB_MOID (idf)))) {
    return (NO_TEXT);
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return (NO_TEXT);
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE];
    (void) make_name (fun, FUN, "", NUMBER (idf));
    comment_source (p, out);
    (void) make_name (fn, "_deproc", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
/* Declare */
    root_idf = NO_DEC;
    (void) add_declaration (&root_idf, "A68_PROCEDURE", 1, fun);
    (void) add_declaration (&root_idf, "NODE_T", 1, "body");
    print_declarations (out, root_idf);
/* Initialise */
    if (compose_fun != A68_MAKE_NOTHING) {
    }
    get_stack (idf, out, fun, "A68_PROCEDURE");
    indentf (out, snprintf (line, SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", fun));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
/* Execute procedure */
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, SNPRINTF_SIZE, "change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    if (GC_MODE (SUB_MOID (idf))) {
    }
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_deproc", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  }
}

/**
@brief Compile deproceduring.
@param out Output file descriptor.
@param p Starting node.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_voiding_deproceduring (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * idf = locate (SUB_SUB (p), IDENTIFIER);
  if (idf == NO_NODE) {
    return (NO_TEXT);
  } else if (! (SUB_MOID (idf) == MODE (VOID) || basic_mode (SUB_MOID (idf)))) {
    return (NO_TEXT);
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return (NO_TEXT);
  } else {
    static char fn[NAME_SIZE]; 
    char fun[NAME_SIZE], pop[NAME_SIZE];
    (void) make_name (fun, FUN, "", NUMBER (idf));
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, "_void_deproc", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
/* Declare */
    root_idf = NO_DEC;
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
    (void) add_declaration (&root_idf, "A68_PROCEDURE", 1, fun);
    (void) add_declaration (&root_idf, "NODE_T", 1, "body");
    print_declarations (out, root_idf);
/* Initialise */
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
    if (compose_fun != A68_MAKE_NOTHING) {
    }
    get_stack (idf, out, fun, "A68_PROCEDURE");
    indentf (out, snprintf (line, SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", fun));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
/* Execute procedure */
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, SNPRINTF_SIZE, "change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
    indent (out, "CLOSE_FRAME;\n");
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_void_deproc", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  }
}

/**
@brief Compile call.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_call (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * proc = SUB (p);
  NODE_T * args = NEXT (proc);
  NODE_T * idf = locate (proc, IDENTIFIER);
  if (idf == NO_NODE) {
    return (NO_TEXT);
  } else if (! (SUB_MOID (proc) == MODE (VOID) || basic_mode (SUB_MOID (proc)))) {
    return (NO_TEXT);
  } else if (DIM (MOID (proc)) == 0) {
    return (NO_TEXT);
  } else if (A68G_STANDENV_PROC (TAX (idf))) {
    if (basic_call (p)) {
      static char fn[NAME_SIZE];
      char fun[NAME_SIZE];
      (void) make_name (fun, FUN, "", NUMBER (proc));
      comment_source (p, out);
      (void) make_name (fn, "_call", "", NUMBER (p));
      if (compose_fun == A68_MAKE_FUNCTION) {
        write_fun_prelude (p, out, fn);
      }
      root_idf = NO_DEC;
      inline_unit (p, out, L_DECLARE);
      print_declarations (out, root_idf);
      inline_unit (p, out, L_EXECUTE);
      compile_push (p, out);
      if (compose_fun == A68_MAKE_FUNCTION) {
        (void) make_name (fn, "_call", "", NUMBER (p));
        write_fun_postlude (p, out, fn);
      }
      return (fn);
    } else {
      return (NO_TEXT);
    }
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return (NO_TEXT);
  } else if (DIM (PARTIAL_PROC (GINFO (proc))) != 0) {
    return (NO_TEXT);
  } else if (!basic_argument (args)) {
    return (NO_TEXT);
  } else {
    static char fn[NAME_SIZE]; 
    char fun[NAME_SIZE], pop[NAME_SIZE];
    int size;
/* Declare */
    (void) make_name (fun, FUN, "", NUMBER (proc));
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, "_call", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
/* Compute arguments */
    size = 0;
    root_idf = NO_DEC;
    inline_arguments (args, out, L_DECLARE, &size);
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
    (void) add_declaration (&root_idf, "A68_PROCEDURE", 1, fun);
    (void) add_declaration (&root_idf, "NODE_T", 1, "body");
    print_declarations (out, root_idf);
/* Initialise */
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
    if (compose_fun != A68_MAKE_NOTHING) {
    }
    inline_arguments (args, out, L_INITIALISE, &size);
    get_stack (idf, out, fun, "A68_PROCEDURE");
    indentf (out, snprintf (line, SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", fun));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
    size = 0;
    inline_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    inline_arguments (args, out, L_YIELD, &size);
/* Execute procedure */
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, SNPRINTF_SIZE, "change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    if (GC_MODE (SUB_MOID (proc))) {
    }
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_call", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  }
}

/**
@brief Compile call.
@param out Output file descriptor.
@param p Starting node.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_voiding_call (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * proc = SUB (locate (p, CALL));
  NODE_T * args = NEXT (proc);
  NODE_T * idf = locate (proc, IDENTIFIER);
  if (idf == NO_NODE) {
    return (NO_TEXT);
  } else if (! (SUB_MOID (proc) == MODE (VOID) || basic_mode (SUB_MOID (proc)))) {
    return (NO_TEXT);
  } else if (DIM (MOID (proc)) == 0) {
    return (NO_TEXT);
  } else if (A68G_STANDENV_PROC (TAX (idf))) {
    return (NO_TEXT);
  } else if (!(CODEX (TAX (idf)) & PROC_DECLARATION_MASK)) {
    return (NO_TEXT);
  } else if (DIM (PARTIAL_PROC (GINFO (proc))) != 0) {
    return (NO_TEXT);
  } else if (!basic_argument (args)) {
    return (NO_TEXT);
  } else {
    static char fn[NAME_SIZE];
    char fun[NAME_SIZE], pop[NAME_SIZE];
    int size;
/* Declare */
    (void) make_name (fun, FUN, "", NUMBER (proc));
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, "_void_call", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
/* Compute arguments */
    size = 0;
    root_idf = NO_DEC;
    inline_arguments (args, out, L_DECLARE, &size);
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
    (void) add_declaration (&root_idf, "A68_PROCEDURE", 1, fun);
    (void) add_declaration (&root_idf, "NODE_T", 1, "body");
    print_declarations (out, root_idf);
/* Initialise */
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
    if (compose_fun != A68_MAKE_NOTHING) {
    }
    inline_arguments (args, out, L_INITIALISE, &size);
    get_stack (idf, out, fun, "A68_PROCEDURE");
    indentf (out, snprintf (line, SNPRINTF_SIZE, "body = SUB (NODE (&BODY (%s)));\n", fun));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OPEN_PROC_FRAME (body, ENVIRON (%s));\n", fun));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "INIT_STATIC_FRAME (body);\n"));
    size = 0;
    inline_arguments (args, out, L_EXECUTE, &size);
    size = 0;
    inline_arguments (args, out, L_YIELD, &size);
/* Execute procedure */
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
    indent (out, "EXECUTE_UNIT_TRACE (NEXT_NEXT_NEXT (body));\n");
    indent (out, "if (frame_pointer == finish_frame_pointer) {\n");
    indentation ++;
    indentf (out, snprintf (line, SNPRINTF_SIZE, "change_masks (TOP_NODE (&program), BREAKPOINT_INTERRUPT_MASK, A68_TRUE);\n"));
    indentation --;
    indent (out, "}\n");
    indent (out, "CLOSE_FRAME;\n");
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_void_call", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  }
}

/**
@brief Compile voiding assignation.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_voiding_assignation_selection (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * dst = SUB (locate (p, ASSIGNATION));
  NODE_T * src = NEXT_NEXT (dst);
  if (BASIC (dst, SELECTION) && basic_unit (src) && basic_mode_non_row (MOID (dst))) {
    NODE_T * field = SUB (locate (dst, SELECTION));
    NODE_T * sec = NEXT (field);
    NODE_T * idf = locate (sec, IDENTIFIER);
    char sel[NAME_SIZE], ref[NAME_SIZE], pop[NAME_SIZE]; 
    char * field_idf = NSYMBOL (SUB (field));
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (pop, PUP, "", NUMBER (p));
    (void) make_name (fn, "_void_assign", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
/* Declare */
    root_idf = NO_DEC;
    if (signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf)) == NO_BOOK) {
      (void) make_name (ref, NSYMBOL (idf), "", NUMBER (field));
      (void) make_name (sel, SEL, "", NUMBER (field));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "A68_REF * %s; /* %s */\n", ref, NSYMBOL (idf)));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s * %s;\n", inline_mode (SUB_MOID (field)), sel));
      sign_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    } else {
      int n = NUMBER (signed_in (BOOK_DECL, L_DECLARE, NSYMBOL (idf)));
      (void) make_name (ref, NSYMBOL (idf), "", n);
      (void) make_name (sel, SEL, "", n);
    }
    inline_unit (src, out, L_DECLARE);
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
    print_declarations (out, root_idf);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
/* Initialise */
    if (signed_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf)) == NO_BOOK) {
      get_stack (idf, out, ref, "A68_REF");
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) & (ADDRESS (%s)[%d]);\n", sel, inline_mode (SUB_MOID (field)), ref, OFFSET_OFF (field)));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (idf), (void *) field_idf, NUMBER (field));
    }
    inline_unit (src, out, L_EXECUTE);
/* Generate */
    compile_assign (src, out, sel);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_void_assign", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile voiding assignation.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_voiding_assignation_slice (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * dst = SUB (locate (p, ASSIGNATION));
  NODE_T * src = NEXT_NEXT (dst);
  NODE_T * slice = locate (SUB (dst), SLICE);
  NODE_T * prim = SUB (slice);
  MOID_T * mode = SUB_MOID (dst);
  MOID_T * row_mode = DEFLEX (MOID (prim));
  if (IS (row_mode, REF_SYMBOL) && basic_slice (slice) && basic_unit (src) && basic_mode_non_row (MOID (src))) {
    NODE_T * indx = NEXT (prim);
    char * symbol = NSYMBOL (SUB (prim));
    char drf[NAME_SIZE], idf[NAME_SIZE], arr[NAME_SIZE], tup[NAME_SIZE], elm[NAME_SIZE], pop[NAME_SIZE];
    static char fn[NAME_SIZE];
    int k;
    comment_source (p, out);
    (void) make_name (pop, PUP, "", NUMBER (p));
    (void) make_name (fn, "_void_assign", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
/* Declare */
    root_idf = NO_DEC;
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
    if (signed_in (BOOK_DECL, L_DECLARE, symbol) == NO_BOOK) {
      (void) make_name (idf, symbol, "", NUMBER (prim));
      (void) make_name (arr, ARR, "", NUMBER (prim));
      (void) make_name (tup, TUP, "", NUMBER (prim));
      (void) make_name (elm, ELM, "", NUMBER (prim));
      (void) make_name (drf, DRF, "", NUMBER (prim));
      (void) add_declaration (&root_idf, "A68_REF", 1, idf);
      (void) add_declaration (&root_idf, "A68_REF", 0, elm);
      (void) add_declaration (&root_idf, "A68_ARRAY", 1, arr);
      (void) add_declaration (&root_idf, "A68_TUPLE", 1, tup);
      (void) add_declaration (&root_idf, inline_mode (mode), 1, drf);
      sign_in (BOOK_DECL, L_DECLARE, symbol, (void *) indx, NUMBER (prim));
    } else {
      int n = NUMBER (signed_in (BOOK_DECL, L_EXECUTE, symbol));
      (void) make_name (idf, symbol, "", n);
      (void) make_name (arr, ARR, "", n);
      (void) make_name (tup, TUP, "", n);
      (void) make_name (elm, ELM, "", n);
      (void) make_name (drf, DRF, "", n);
    }
    k = 0;
    inline_indexer (indx, out, L_DECLARE, &k, NO_TEXT);
    inline_unit (src, out, L_DECLARE);
    print_declarations (out, root_idf);
/* Initialise */
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
    if (signed_in (BOOK_DECL, L_EXECUTE, symbol) == NO_BOOK) {
      NODE_T * pidf = locate (prim, IDENTIFIER);
      get_stack (pidf, out, idf, "A68_REF");
      indentf (out, snprintf (line, SNPRINTF_SIZE, "GET_DESCRIPTOR (%s, %s, DEREF (A68_ROW, %s));\n", arr, tup, idf));
      indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = ARRAY (%s);\n", elm, arr));
      sign_in (BOOK_DECL, L_EXECUTE, NSYMBOL (p), (void *) indx, NUMBER (prim));
    }
    k = 0;
    inline_indexer (indx, out, L_EXECUTE, &k, NO_TEXT);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "OFFSET (& %s) += ROW_ELEMENT (%s, ", elm, arr));
    k = 0;
    inline_indexer (indx, out, L_YIELD, &k, tup);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ");\n"));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = DEREF (%s, & %s);\n", drf, inline_mode (mode), elm));
    inline_unit (src, out, L_EXECUTE);
/* Generate */
    compile_assign (src, out, drf);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_void_assign", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile voiding assignation.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_voiding_assignation_identifier (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T * dst = SUB (locate (p, ASSIGNATION));
  NODE_T * src = NEXT_NEXT (dst);
  if (BASIC (dst, IDENTIFIER) && basic_unit (src) && basic_mode_non_row (MOID (src))) {
    static char fn[NAME_SIZE];
    char idf[NAME_SIZE], pop[NAME_SIZE];
    NODE_T * q = locate (dst, IDENTIFIER);
/* Declare */
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, "_void_assign", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    if (signed_in (BOOK_DEREF, L_DECLARE, NSYMBOL (q)) == NO_BOOK) {
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (p));
      (void) add_declaration (&root_idf, inline_mode (SUB_MOID (dst)), 1, idf);
      sign_in (BOOK_DEREF, L_DECLARE, NSYMBOL (q), NULL, NUMBER (p));
    } else {
      (void) make_name (idf, NSYMBOL (q), "", NUMBER (signed_in (BOOK_DEREF, L_DECLARE, NSYMBOL (p))));
    }
    inline_unit (dst, out, L_DECLARE);
    inline_unit (src, out, L_DECLARE);
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
    print_declarations (out, root_idf);
/* Initialise */
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
    inline_unit (dst, out, L_EXECUTE);
    if (signed_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q)) == NO_BOOK) {
      if (BODY (TAX (q)) != NO_TAG) {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (%s *) LOCAL_ADDRESS (", idf, inline_mode (SUB_MOID (dst))));
        inline_unit (dst, out, L_YIELD);
        undent (out, ");\n");
        sign_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q), NULL, NUMBER (p));
      } else {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = DEREF (%s, ", idf, inline_mode (SUB_MOID (dst))));
        inline_unit (dst, out, L_YIELD);
        undent (out, ");\n");
        sign_in (BOOK_DEREF, L_EXECUTE, NSYMBOL (q), NULL, NUMBER (p));
      }
    }
    inline_unit (src, out, L_EXECUTE);
    compile_assign (src, out, idf);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_void_assign", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile identity-relation.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_identity_relation (NODE_T * p, FILE_T out, int compose_fun)
{
#define GOOD(p) (locate (p, IDENTIFIER) != NO_NODE && IS (MOID (locate ((p), IDENTIFIER)), REF_SYMBOL))
  NODE_T * lhs = SUB (p);
  NODE_T * op = NEXT (lhs);
  NODE_T * rhs = NEXT (op);
  if (GOOD (lhs) && GOOD (rhs)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_identity", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_identity_relation (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_identity_relation (p, out, L_EXECUTE);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "PUSH_PRIMITIVE (p, "));
    inline_identity_relation (p, out, L_YIELD);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ", A68_BOOL);\n"));
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_identity", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else if (GOOD (lhs) && locate (rhs, NIHIL) != NO_NODE) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_identity", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_identity_relation (p, out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_identity_relation (p, out, L_EXECUTE);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "PUSH_PRIMITIVE (p, "));
    inline_identity_relation (p, out, L_YIELD);
    undentf (out, snprintf (line, SNPRINTF_SIZE, ", A68_BOOL);\n"));
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_identity", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
#undef GOOD
}

/**
@brief Compile closed clause.
@param out Output file descriptor.
@param decs Number of declarations.
@param pop Value to restore stack pointer to. 
@param p Starting node.
**/

static void compile_declaration_list (NODE_T * p, FILE_T out, int * decs, char * pop)
{
  for (; p  != NO_NODE; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case MODE_DECLARATION:
    case PROCEDURE_DECLARATION:
    case BRIEF_OPERATOR_DECLARATION:
    case PRIORITY_DECLARATION:
      {
/* No action needed */
        (* decs) ++;
        return;
      }
    case OPERATOR_DECLARATION:
      {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "genie_operator_dec (_N_ (%d));", NUMBER (SUB (p))));
        inline_comment_source (p, out);
        undent (out, NEWLINE_STRING);
        (* decs) ++;
        break;
      }
    case IDENTITY_DECLARATION:
      {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "genie_identity_dec (_N_ (%d));", NUMBER (SUB (p))));
        inline_comment_source (p, out);
        undent (out, NEWLINE_STRING);
        (* decs) ++;
        break;
      }
    case VARIABLE_DECLARATION:
      {
        char declarer[NAME_SIZE];
        (void) make_name (declarer, DEC, "", NUMBER (SUB (p)));
        indent (out, "{");
        inline_comment_source (p, out);
        undent (out, NEWLINE_STRING);
        indentation ++;
        indentf (out, snprintf (line, SNPRINTF_SIZE, "NODE_T *%s = NO_NODE;\n", declarer));
        indentf (out, snprintf (line, SNPRINTF_SIZE, "genie_variable_dec (_N_ (%d), &%s, stack_pointer);\n", NUMBER (SUB (p)), declarer));
        indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
        indentation --;
        indent (out, "}\n");
        (* decs) ++;
        break;
      }
    case PROCEDURE_VARIABLE_DECLARATION:
      {
        indentf (out, snprintf (line, SNPRINTF_SIZE, "genie_proc_variable_dec (_N_ (%d));", NUMBER (SUB (p))));
        inline_comment_source (p, out);
        undent (out, NEWLINE_STRING);
        indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
        (* decs) ++;
        break;
      }
    default:
      {
        compile_declaration_list (SUB (p), out, decs, pop);
        break;
      }
    }
  }
}

/**
@brief Compile closed clause.
@param p Starting node.
@param out Output file descriptor.
@param last Last unit generated. 
@param units Number of units.
@param decs Number of declarations.
@param pop Value to restore stack pointer to. 
@param compose_fun Whether to compose a function.
**/

static void compile_serial_clause (NODE_T * p, FILE_T out, NODE_T ** last, int * units, int * decs, char * pop, int compose_fun)
{
  for (; p != NO_NODE; FORWARD (p)) {
    if (compose_fun == A68_MAKE_OTHERS) {
      if (IS (p, UNIT)) {
        (* units) ++;
      }
      if (IS (p, DECLARATION_LIST)) {
        (* decs) ++;
      }
      if (IS (p, UNIT) || IS (p, DECLARATION_LIST)) {
        if (compile_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
          if (IS (p, UNIT) && IS (SUB (p), TERTIARY)) {
            compile_units (SUB_SUB (p), out);
          } else {
            compile_units (SUB (p), out);
          }
        } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
          COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
          COMPILE_NAME (GINFO (p)) = COMPILE_NAME (GINFO (SUB (p)));
        }
        return;
      } else {
        compile_serial_clause (SUB (p), out, last, units, decs, pop, compose_fun);
      }
    } else switch (ATTRIBUTE (p)) {
      case UNIT:
        {
          (* last) = p;
          CODE_EXECUTE (p);
          inline_comment_source (p, out);
          undent (out, NEWLINE_STRING);
          (* units) ++;
          return;
        }
      case SEMI_SYMBOL:
        {
          if (IS (* last, UNIT) && MOID (* last) == MODE (VOID)) {
            break;
          } else if (IS (* last, DECLARATION_LIST)) {
            break;
          } else {
            indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
          }
          break;
        }
      case DECLARATION_LIST:
        {
          (* last) = p;
          compile_declaration_list (SUB (p), out, decs, pop);
          break;
        }
      default:
        {
          compile_serial_clause (SUB (p), out, last, units, decs, pop, compose_fun);
          break;
        }
    }
  }
}

/**
@brief Embed serial clause.
@param p Starting node.
@param out Output file descriptor.
@param pop Value to restore stack pointer to. 
*/

static void embed_serial_clause (NODE_T * p, FILE_T out, char * pop) 
{
  NODE_T * last = NO_NODE;
  int units = 0, decs = 0; 
  indentf (out, snprintf (line, SNPRINTF_SIZE, "OPEN_STATIC_FRAME (_N_ (%d));\n", NUMBER (p)));
  init_static_frame (out, p);
  compile_serial_clause (p, out, &last, &units, &decs, pop, A68_MAKE_FUNCTION);
  indent (out, "CLOSE_FRAME;\n");
}

/**
@brief Compile code clause.
@param out Output file descriptor.
@param p Starting node.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_code_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  comment_source (p, out);
  (void) make_name (fn, "_code", "", NUMBER (p));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  embed_code_clause (SUB (p), out);
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "_code", "", NUMBER (p));
    write_fun_postlude (p, out, fn);
  }
  return (fn);
}

/**
@brief Compile closed clause.
@param out Output file descriptor.
@param p Starting node.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_closed_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *sc = NEXT_SUB (p);
  if (MOID (p) == MODE (VOID) && LABELS (TABLE (sc)) == NO_TAG) {
    static char fn[NAME_SIZE];
    char pop[NAME_SIZE];
    int units = 0, decs = 0;
    NODE_T * last = NO_NODE;
    compile_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
    (void) make_name (pop, PUP, "", NUMBER (p));
    comment_source (p, out);
    (void) make_name (fn, "_closed", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
    print_declarations (out, root_idf);
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
    embed_serial_clause (sc, out, pop);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_closed", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile collateral clause.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_collateral_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  if (basic_unit (p) && IS (MOID (p), STRUCT_SYMBOL)) {
    static char fn[NAME_SIZE];
    comment_source (p, out);
    (void) make_name (fn, "_collateral", "", NUMBER (p));
    if (compose_fun == A68_MAKE_FUNCTION) {
      write_fun_prelude (p, out, fn);
    }
    root_idf = NO_DEC;
    inline_collateral_units (NEXT_SUB (p), out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_collateral_units (NEXT_SUB (p), out, L_EXECUTE);
    inline_collateral_units (NEXT_SUB (p), out, L_YIELD);
    if (compose_fun == A68_MAKE_FUNCTION) {
      (void) make_name (fn, "_collateral", "", NUMBER (p));
      write_fun_postlude (p, out, fn);
    }
    return (fn);
  } else {
    return (NO_TEXT);
  }
}

/**
@brief Compile conditional clause.
@param out Output file descriptor.
@param p Starting node.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_basic_conditional (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  NODE_T * q = SUB (p);
  if (! (basic_mode (MOID (p)) || MOID (p) == MODE (VOID))) {
    return (NO_TEXT);
  }
  p = q;
  if (!basic_conditional (p)) {
    return (NO_TEXT);
  }
  comment_source (p, out);
  (void) make_name (fn, "_conditional", "", NUMBER (q));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (q, out, fn);
  }
/* Collect declarations */
  if (IS (p, IF_PART) || IS (p, OPEN_PART)) {
    root_idf = NO_DEC;
    inline_unit (SUB (NEXT_SUB (p)), out, L_DECLARE);
    print_declarations (out, root_idf);
    inline_unit (SUB (NEXT_SUB (p)), out, L_EXECUTE);
    indent (out, "if (");
    inline_unit (SUB (NEXT_SUB (p)), out, L_YIELD);
    undent (out, ") {\n");
    indentation ++;
  } else {
    ABEND (A68_TRUE, "if-part expected", NO_TEXT);
  }
  FORWARD (p);
  if (IS (p, THEN_PART) || IS (p, CHOICE)) {
    int pop = temp_book_pointer;
    (void) compile_unit (SUB (NEXT_SUB (p)), out, A68_MAKE_NOTHING);
    indentation --;
    temp_book_pointer = pop;
  } else {
    ABEND (A68_TRUE, "then-part expected", NO_TEXT);
  }
  FORWARD (p);
  if (IS (p, ELSE_PART) || IS (p, CHOICE)) {
    int pop = temp_book_pointer;
    indent (out, "} else {\n");
    indentation ++;
    (void) compile_unit (SUB (NEXT_SUB (p)), out, A68_MAKE_NOTHING);
    indentation --;
    temp_book_pointer = pop;
  }
/* Done */
  indent (out, "}\n");
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "_conditional", "", NUMBER (q));
    write_fun_postlude (q, out, fn);
  }
  return (fn);
}

/**
@brief Compile conditional clause.
@param p Starting node.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_conditional_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  char pop[NAME_SIZE];
  int units = 0, decs = 0;
  NODE_T * q, * last;
/* We only compile IF basic unit or ELIF basic unit, so we save on opening frames */
/* Check worthiness of the clause */
  if (MOID (p) != MODE (VOID)) {
    return (NO_TEXT);
  }
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    if (! basic_serial (NEXT_SUB (q), 1)) {
      return (NO_TEXT);
    }
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      if (LABELS (TABLE (NEXT_SUB (q))) != NO_TAG) {
        return (NO_TEXT);
      }
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL)) {
      FORWARD (q);
    }
  }
/* Generate embedded units */
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      last = NO_NODE; units = decs = 0;
      compile_serial_clause (NEXT_SUB (q), out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL)) {
      FORWARD (q);
    }
  }
/* Prep and Dec */
  (void) make_name (fn, "_conditional", "", NUMBER (p));
  (void) make_name (pop, PUP, "", NUMBER (p));
  comment_source (p, out);
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  root_idf = NO_DEC;
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    inline_unit (SUB (NEXT_SUB (q)), out, L_DECLARE);
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL)) {
      FORWARD (q);
    }
  }
  (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
  print_declarations (out, root_idf);
/* Generate the function body */
  indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    inline_unit (SUB (NEXT_SUB (q)), out, L_EXECUTE);
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL)) {
      FORWARD (q);
    }
  }
  q = SUB (p);
  while (q != NO_NODE && is_one_of (q, IF_PART, OPEN_PART, ELIF_IF_PART, ELSE_OPEN_PART, STOP)) {
    BOOL_T else_part = A68_FALSE;
    if (is_one_of (q, IF_PART, OPEN_PART, STOP) ) {
      indent (out, "if (");
    } else {
      indent (out, "} else if (");
    }
    inline_unit (SUB (NEXT_SUB (q)), out, L_YIELD);
    undent (out, ") {\n");
    FORWARD (q);
    while (q != NO_NODE && (IS (q, THEN_PART) || IS (q, ELSE_PART) || IS (q, CHOICE))) {
      if (else_part) {
        indent (out, "} else {\n");
      }
      indentation ++;
      embed_serial_clause (NEXT_SUB (q), out, pop);
      indentation --;
      else_part = A68_TRUE;
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, ELIF_PART, BRIEF_ELIF_PART, STOP)) {
      q = SUB (q);
    } else if (q != NO_NODE && is_one_of (q, FI_SYMBOL, CLOSE_SYMBOL)) {
      FORWARD (q);
    }
  }
  indent (out, "}\n");
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "_conditional", "", NUMBER (p));
    write_fun_postlude (p, out, fn);
  }
  return (fn);
}

/**
@brief Compile unit from integral-case in-part.
@param p Node in syntax tree.
@param out Output file descriptor.
@param sym Node in syntax tree. 
@param k Value of enquiry clause.
@param count Unit counter.
@param compose_fun Whether to compose a function.
@return Whether a unit was compiled.
**/

BOOL_T compile_int_case_units (NODE_T * p, FILE_T out, NODE_T * sym, int k, int * count, int compose_fun)
{
  if (p == NO_NODE) {
    return (A68_FALSE);
  } else {
    if (IS (p, UNIT)) {
      if (k == * count) {
        if (compose_fun == A68_MAKE_FUNCTION) {
          indentf (out, snprintf (line, SNPRINTF_SIZE, "case %d: {\n", k));
          indentation ++;
          indentf (out, snprintf (line, SNPRINTF_SIZE, "OPEN_STATIC_FRAME (_N_ (%d));\n", NUMBER (sym)));
          CODE_EXECUTE (p);
          inline_comment_source (p, out);
          undent (out, NEWLINE_STRING);
          indent (out, "CLOSE_FRAME;\n");
          indent (out, "break;\n");
          indentation --;
          indent (out, "}\n");
        } else if (compose_fun == A68_MAKE_OTHERS) {
          if (compile_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
            if (IS (p, UNIT) && IS (SUB (p), TERTIARY)) {
              compile_units (SUB_SUB (p), out);
            } else {
              compile_units (SUB (p), out);
            }
          } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
            COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
            COMPILE_NAME (GINFO (p)) = COMPILE_NAME (GINFO (SUB (p)));
          }
        }
        return (A68_TRUE);
      } else {
        (* count)++;
        return (A68_FALSE);
      }
    } else {
      if (compile_int_case_units (SUB (p), out, sym, k, count, compose_fun)) {
        return (A68_TRUE);
      } else {
        return (compile_int_case_units (NEXT (p), out, sym, k, count, compose_fun));
      }
    }
  }
}

/**
@brief Compile integral-case-clause.
@param p Node in syntax tree.
@param out Output file descriptor.
@param compose_fun Whether to compose a function.
**/

static char * compile_int_case_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  static char fn[NAME_SIZE];
  char pop[NAME_SIZE];
  int units = 0, decs = 0, k = 0, count = 0;
  NODE_T * q, * last;
/* We only compile CASE basic unit */
/* Check worthiness of the clause */
  if (MOID (p) != MODE (VOID)) {
    return (NO_TEXT);
  }
  q = SUB (p);
  if (q != NO_NODE && is_one_of (q, CASE_PART, OPEN_PART, STOP)) {
    if (! basic_serial (NEXT_SUB (q), 1)) {
      return (NO_TEXT);
    }
    FORWARD (q);
  } else {
    return (NO_TEXT);
  }
  while (q != NO_NODE && is_one_of (q, CASE_IN_PART, OUT_PART, CHOICE, STOP)) {
    if (LABELS (TABLE (NEXT_SUB (q))) != NO_TAG) {
      return (NO_TEXT);
    }
    FORWARD (q);
  }
  if (q != NO_NODE && is_one_of (q, ESAC_SYMBOL, CLOSE_SYMBOL)) {
    FORWARD (q);
  } else {
    return (NO_TEXT);
  }
/* Generate embedded units */
  q = SUB (p);
  if (q != NO_NODE && is_one_of (q, CASE_PART, OPEN_PART, STOP)) {
    FORWARD (q);
    if (q != NO_NODE && is_one_of (q, CASE_IN_PART, CHOICE, STOP)) {
      last = NO_NODE; units = decs = 0;
      k = 0;
      do {
        count = 1;
        k ++;
      } while (compile_int_case_units (NEXT_SUB (q), out, NO_NODE, k, & count, A68_MAKE_OTHERS)); 
      FORWARD (q);
    }
    if (q != NO_NODE && is_one_of (q, OUT_PART, CHOICE, STOP)) {
      last = NO_NODE; units = decs = 0;
      compile_serial_clause (NEXT_SUB (q), out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
      FORWARD (q);
    }
  }
/* Prep and Dec */
  (void) make_name (pop, PUP, "", NUMBER (p));
  comment_source (p, out);
  (void) make_name (fn, "_case", "", NUMBER (p));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  root_idf = NO_DEC;
  q = SUB (p);
  inline_unit (SUB (NEXT_SUB (q)), out, L_DECLARE);
  (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
  print_declarations (out, root_idf);
/* Generate the function body */
  indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
  q = SUB (p);
  inline_unit (SUB (NEXT_SUB (q)), out, L_EXECUTE);
  indent (out, "switch (");
  inline_unit (SUB (NEXT_SUB (q)), out, L_YIELD);
  undent (out, ") {\n");
  indentation ++;
  FORWARD (q);
  k = 0;
  do {
    count = 1;
    k ++;
  } while (compile_int_case_units (NEXT_SUB (q), out, SUB (q), k, & count, A68_MAKE_FUNCTION)); 
  FORWARD (q);
  if (q != NO_NODE && is_one_of (q, OUT_PART, CHOICE, STOP)) {
    indent (out, "default: {\n");
    indentation ++;
    embed_serial_clause (NEXT_SUB (q), out, pop);
    indent (out, "break;\n");
    indentation --;
    indent (out, "}\n");
  }
  indentation --;
  indent (out, "}\n");
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "_case", "", NUMBER (p));
    write_fun_postlude (p, out, fn);
  }
  return (fn);
}

/**
@brief Compile loop clause.
@param out Output file descriptor.
@param p Starting node.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_loop_clause (NODE_T * p, FILE_T out, int compose_fun)
{
  NODE_T *for_part = NO_NODE, *from_part = NO_NODE, *by_part = NO_NODE, *to_part = NO_NODE, * downto_part = NO_NODE, * while_part = NO_NODE, * sc;
  static char fn[NAME_SIZE];
  char idf[NAME_SIZE], z[NAME_SIZE], pop[NAME_SIZE];
  NODE_T * q = SUB (p), * last = NO_NODE;
  int units, decs;
  BOOL_T gc, need_reinit;
/* FOR identifier */
  if (IS (q, FOR_PART)) {
    for_part = NEXT_SUB (q);
    FORWARD (q);
  }
/* FROM unit */
  if (IS (p, FROM_PART)) {
    from_part = NEXT_SUB (q);
    if (! basic_unit (from_part)) {
      return (NO_TEXT);
    }
    FORWARD (q);
  }
/* BY unit */
  if (IS (q, BY_PART)) {
    by_part = NEXT_SUB (q);
    if (! basic_unit (by_part)) {
      return (NO_TEXT);
    }
    FORWARD (q);
  }
/* TO unit, DOWNTO unit */
  if (IS (q, TO_PART)) {
    if (IS (SUB (q), TO_SYMBOL)) {
      to_part = NEXT_SUB (q);
      if (! basic_unit (to_part)) {
        return (NO_TEXT);
      }
    } else if (IS (SUB (q), DOWNTO_SYMBOL)) {
      downto_part = NEXT_SUB (q);
      if (! basic_unit (downto_part)) {
        return (NO_TEXT);
      }
    }
    FORWARD (q);
  }
/* WHILE DO OD is not yet supported */
  if (IS (q, WHILE_PART)) {
    return (NO_TEXT);
  }
/* DO UNTIL OD is not yet supported */
  if (IS (q, DO_PART) || IS (q, ALT_DO_PART)) {
    sc = q = NEXT_SUB (q);
    if (IS (q, SERIAL_CLAUSE)) {
      FORWARD (q);
    }
    if (q != NO_NODE && IS (q, UNTIL_PART)) {
      return (NO_TEXT);
    }
  } else {
    return (NO_TEXT);
  }
  if (LABELS (TABLE (sc)) != NO_TAG) {
    return (NO_TEXT);
  }
/* Loop clause is compiled */
  units = decs = 0;
  compile_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_OTHERS);
  gc = (decs > 0);
  comment_source (p, out);
  (void) make_name (fn, "_loop", "", NUMBER (p));
  if (compose_fun == A68_MAKE_FUNCTION) {
    write_fun_prelude (p, out, fn);
  }
  root_idf = NO_DEC;
    (void) make_name (idf, "k", "", NUMBER (p));
    (void) add_declaration (&root_idf, "int", 0, idf);
    if (for_part != NO_NODE) {
      (void) make_name (z, "z", "", NUMBER (p));
      (void) add_declaration (&root_idf, "A68_INT", 1, z);
    }
  if (from_part != NO_NODE) {
    inline_unit (from_part, out, L_DECLARE);
  }
  if (by_part != NO_NODE) {
    inline_unit (by_part, out, L_DECLARE);
  }
  if (to_part != NO_NODE) {
    inline_unit (to_part, out, L_DECLARE);
  }
  if (downto_part != NO_NODE) {
    inline_unit (downto_part, out, L_DECLARE);
  }
  if (while_part != NO_NODE) {
    inline_unit (SUB (NEXT_SUB (while_part)), out, L_DECLARE);
  }
  (void) make_name (pop, PUP, "", NUMBER (p));
  (void) add_declaration (&root_idf, "ADDR_T", 0, pop);
  print_declarations (out, root_idf);
  indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = stack_pointer;\n", pop));
  if (from_part != NO_NODE) {
    inline_unit (from_part, out, L_EXECUTE);
  }
  if (by_part != NO_NODE) {
    inline_unit (by_part, out, L_EXECUTE);
  }
  if (to_part != NO_NODE) {
    inline_unit (to_part, out, L_EXECUTE);
  }
  if (downto_part != NO_NODE) {
    inline_unit (downto_part, out, L_EXECUTE);
  }
  if (while_part != NO_NODE) {
    inline_unit (SUB (NEXT_SUB (while_part)), out, L_EXECUTE);
  }
  indentf (out, snprintf (line, SNPRINTF_SIZE, "OPEN_STATIC_FRAME (_N_ (%d));\n", NUMBER (sc)));
  init_static_frame (out, sc);
  if (for_part != NO_NODE) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "%s = (A68_INT *) (FRAME_OBJECT (OFFSET (TAX (_N_ (%d)))));\n", z, NUMBER (for_part)));
  }
/* The loop in C */
/* Initialisation */
    indentf (out, snprintf (line, SNPRINTF_SIZE, "for (%s = ", idf));
    if (from_part == NO_NODE) {
      undent (out, "1");
    } else {
      inline_unit (from_part, out, L_YIELD);
    }
    undent (out, "; ");
/* Condition */
    if (to_part == NO_NODE && downto_part == NO_NODE && while_part == NO_NODE) {
      undent (out, "A68_TRUE");
    } else {
      undent (out, idf);
      if (to_part != NO_NODE) {
        undent (out, " <= ");
      } else if (downto_part != NO_NODE) {
        undent (out, " >= ");
      }
      inline_unit (to_part, out, L_YIELD);
    }
    undent (out, "; ");
/* Increment */
    if (by_part == NO_NODE) {
      undent (out, idf);
      if (to_part != NO_NODE) {
        undent (out, " ++");
      } else if (downto_part != NO_NODE) {
        undent (out, " --");
      } else {
        undent (out, " ++");
      }
    } else {
      undent (out, idf);
      if (to_part != NO_NODE) {
        undent (out, " += ");
      } else if (downto_part != NO_NODE) {
        undent (out, " -= ");
      } else {
        undent (out, " += ");
      }
      inline_unit (by_part, out, L_YIELD);
    }
    undent (out, ") {\n");
  indentation++;
  if (gc) {
    indent (out, "/* PREEMPTIVE_GC; */\n");
  }
  if (for_part != NO_NODE) {
    indentf (out, snprintf (line, SNPRINTF_SIZE, "_S_ (%s) = INIT_MASK;\n", z));
    indentf (out, snprintf (line, SNPRINTF_SIZE, "_V_ (%s) = %s;\n", z, idf));
  }
  units = decs = 0;
  compile_serial_clause (sc, out, &last, &units, &decs, pop, A68_MAKE_FUNCTION);
/* Re-initialise if necessary */
  need_reinit = (BOOL_T) (AP_INCREMENT (TABLE (sc)) > 0 || need_initialise_frame (sc));
  if (need_reinit) {
    indent (out, "if (");
    if (to_part == NO_NODE && downto_part == NO_NODE) {
      undent (out, "A68_TRUE");
    } else {
      undent (out, idf);
      if (to_part != NO_NODE) {
        undent (out, " < ");
      } else if (downto_part != NO_NODE) {
        undent (out, " > ");
      }
      inline_unit (to_part, out, L_YIELD);
    }
    undent (out, ") {\n");
    indentation++;
    if (AP_INCREMENT (TABLE (sc)) > 0) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "FRAME_CLEAR (%d);\n", AP_INCREMENT (TABLE (sc))));
    }
    if (need_initialise_frame (sc)) {
      indentf (out, snprintf (line, SNPRINTF_SIZE, "initialise_frame (_N_ (%d));\n", NUMBER (sc)));
    }
    indentation--;
    indent (out, "}\n");
  }
/* End of loop */
  indentation--;
  indent (out, "}\n");
  indent (out, "CLOSE_FRAME;\n");
  indentf (out, snprintf (line, SNPRINTF_SIZE, "stack_pointer = %s;\n", pop));
  if (compose_fun == A68_MAKE_FUNCTION) {
    (void) make_name (fn, "_loop", "", NUMBER (p));
    write_fun_postlude (p, out, fn);
  }
  return (fn);
}

/**
@brief Compile serial units.
@param out Output file descriptor.
@param p Starting node.
@param compose_fun Whether to compose a function.
@return Function name.
**/

static char * compile_unit (NODE_T * p, FILE_T out, BOOL_T compose_fun)
{
/**/
#define COMPILE(p, out, fun, compose_fun) {\
  char * fn = (fun) (p, out, compose_fun);\
  if (compose_fun == A68_MAKE_FUNCTION && fn != NO_TEXT) {\
    ABEND (strlen (fn) > 32, ERROR_INTERNAL_CONSISTENCY, NO_TEXT);\
    COMPILE_NAME (GINFO (p)) = new_string (fn, NO_TEXT);\
    if (SUB (p) != NO_NODE && COMPILE_NODE (GINFO (SUB (p))) > 0) {\
      COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));\
    } else {\
      COMPILE_NODE (GINFO (p)) = NUMBER (p);\
    }\
    return (COMPILE_NAME (GINFO (p)));\
  } else {\
    COMPILE_NAME (GINFO (p)) = NO_TEXT;\
    COMPILE_NODE (GINFO (p)) = 0;\
    return (NO_TEXT);\
  }}
/**/
  LOW_SYSTEM_STACK_ALERT (p);
  if (p == NO_NODE) {
    return (NO_TEXT);
  } else if (is_one_of (p, UNIT, TERTIARY, SECONDARY, PRIMARY, ENCLOSED_CLAUSE, STOP)) {
    COMPILE (SUB (p), out, compile_unit, compose_fun);
  } 
  if (DEBUG_LEVEL >= 3) {
/* Control structure */
    if (IS (p, CLOSED_CLAUSE)) {
      COMPILE (p, out, compile_closed_clause, compose_fun);
    } else if (IS (p, COLLATERAL_CLAUSE)) {
      COMPILE (p, out, compile_collateral_clause, compose_fun);
    } else if (IS (p, CONDITIONAL_CLAUSE)) {
      char * fn2 = compile_basic_conditional (p, out, compose_fun);
      if (compose_fun == A68_MAKE_FUNCTION && fn2 != NO_TEXT) {
        ABEND (strlen (fn2) > 32, ERROR_INTERNAL_CONSISTENCY, NO_TEXT);
        COMPILE_NAME (GINFO (p)) = new_string (fn2, NO_TEXT);
        if (SUB (p) != NO_NODE && COMPILE_NODE (GINFO (SUB (p))) > 0) {
          COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
        } else {
          COMPILE_NODE (GINFO (p)) = NUMBER (p);
        }
        return (COMPILE_NAME (GINFO (p)));
      } else {
        COMPILE (p, out, compile_conditional_clause, compose_fun);
      }
    } else if (IS (p, CASE_CLAUSE)) {
      COMPILE (p, out, compile_int_case_clause, compose_fun);
    } else if (IS (p, LOOP_CLAUSE)) {
      COMPILE (p, out, compile_loop_clause, compose_fun);
    }
  }
  if (DEBUG_LEVEL >= 2) {
/* Simple constructions */
    if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), IDENTIFIER) != NO_NODE) {
      COMPILE (p, out, compile_voiding_assignation_identifier, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SLICE) != NO_NODE) {
      COMPILE (p, out, compile_voiding_assignation_slice, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), ASSIGNATION) && locate (SUB_SUB (p), SELECTION) != NO_NODE) {
      COMPILE (p, out, compile_voiding_assignation_selection, compose_fun);
    } else if (IS (p, SLICE)) {
      COMPILE (p, out, compile_slice, compose_fun);
    } else if (IS (p, DEREFERENCING) && locate (SUB (p), SLICE) != NO_NODE) {
      COMPILE (p, out, compile_dereference_slice, compose_fun);
    } else if (IS (p, SELECTION)) {
      COMPILE (p, out, compile_selection, compose_fun);
    } else if (IS (p, DEREFERENCING) && locate (SUB (p), SELECTION) != NO_NODE) {
      COMPILE (p, out, compile_dereference_selection, compose_fun);
    } else if (IS (p, CAST)) {
      COMPILE (p, out, compile_cast, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), FORMULA)) {
      COMPILE (SUB (p), out, compile_voiding_formula, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), MONADIC_FORMULA)) {
      COMPILE (SUB (p), out, compile_voiding_formula, compose_fun);
    } else if (IS (p, DEPROCEDURING)) {
      COMPILE (p, out, compile_deproceduring, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), DEPROCEDURING)) {
      COMPILE (p, out, compile_voiding_deproceduring, compose_fun);
    } else if (IS (p, CALL)) {
      COMPILE (p, out, compile_call, compose_fun);
    } else if (IS (p, VOIDING) && IS (SUB (p), CALL)) {
      COMPILE (p, out, compile_voiding_call, compose_fun);
    } else if (IS (p, IDENTITY_RELATION)) {
      COMPILE (p, out, compile_identity_relation, compose_fun);
    } else if (IS (p, UNITING)) {
      COMPILE (p, out, compile_uniting, compose_fun);
    }
  }
  if (DEBUG_LEVEL >= 1) {
    /* Debugging stuff, only basic */
    if (IS (p, DENOTATION)) {
      COMPILE (p, out, compile_denotation, compose_fun);
    } else if (IS (p, IDENTIFIER)) {
      COMPILE (p, out, compile_identifier, compose_fun);
    } else if (IS (p, DEREFERENCING) && locate (SUB (p), IDENTIFIER) != NO_NODE) {
      COMPILE (p, out, compile_dereference_identifier, compose_fun);
    } else if (IS (p, MONADIC_FORMULA)) {
      COMPILE (p, out, compile_formula, compose_fun);
    } else if (IS (p, FORMULA)) {
      COMPILE (p, out, compile_formula, compose_fun);
    }
  }
  if (IS (p, CODE_CLAUSE)) {
    COMPILE (p, out, compile_code_clause, compose_fun);
  }
  return (NO_TEXT);
#undef COMPILE
}

/**
@brief Compile units.
@param p Starting node.
@param out Output file descriptor.
**/

void compile_units (NODE_T * p, FILE_T out)
{
  ADDR_T pop_temp_heap_pointer = temp_heap_pointer; /* At the end we discard temporary declarations */
  for (; p != NO_NODE; FORWARD (p)) {
    if (IS (p, UNIT) || IS (p, CODE_CLAUSE)) {
      if (compile_unit (p, out, A68_MAKE_FUNCTION) == NO_TEXT) {
        compile_units (SUB (p), out);
      } else if (SUB (p) != NO_NODE && GINFO (SUB (p)) != NO_GINFO && COMPILE_NODE (GINFO (SUB (p))) > 0) {
        COMPILE_NODE (GINFO (p)) = COMPILE_NODE (GINFO (SUB (p)));
        COMPILE_NAME (GINFO (p)) = COMPILE_NAME (GINFO (SUB (p)));
      }
    } else {
      compile_units (SUB (p), out);
    }
  }
  temp_heap_pointer = pop_temp_heap_pointer;
}

/**
@brief Compiler driver.
@param out Output file descriptor.
**/

void compiler (FILE_T out)
{
  if (OPTION_OPTIMISE (&program) == A68_FALSE) {
    return;
  }
  indentation = 0;
  temp_book_pointer = 0;
  root_idf = NO_DEC;
  global_level = A68_MAX_INT;
  global_pointer = 0;
  get_global_level (SUB (TOP_NODE (&program)));
  max_lex_lvl = 0;
  genie_preprocess (TOP_NODE (&program), & max_lex_lvl, NULL);
  write_prelude (out);
  get_global_level (TOP_NODE (&program));
  stack_pointer = stack_start;
  expr_stack_limit = stack_end - storage_overhead;
  compile_units (TOP_NODE (&program), out);
  ABEND (indentation != 0, "indentation error", NO_TEXT);
}

