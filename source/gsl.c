/*!
\file gsl.c
\brief calculus related routines from the C library and GNU Scientific Library
**/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2006 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "algol68g.h"
#include "genie.h"
#include "gsl.h"

/*
This file contains calculus related routines from the C library and GNU
scientific library. When GNU scientific library is not installed then the
routines in this file will give a runtime error when called. You can also choose
to not have them defined in "prelude.c"
*/

#ifdef HAVE_GSL
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_const.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_sf.h>
#endif

double inverf (double);
double inverfc (double);

#ifdef HAVE_GSL
A68_ENV_REAL (genie_cgs_acre, GSL_CONST_CGS_ACRE);
A68_ENV_REAL (genie_cgs_angstrom, GSL_CONST_CGS_ANGSTROM);
A68_ENV_REAL (genie_cgs_astronomical_unit, GSL_CONST_CGS_ASTRONOMICAL_UNIT);
A68_ENV_REAL (genie_cgs_bar, GSL_CONST_CGS_BAR);
A68_ENV_REAL (genie_cgs_barn, GSL_CONST_CGS_BARN);
A68_ENV_REAL (genie_cgs_bohr_magneton, GSL_CONST_CGS_BOHR_MAGNETON);
A68_ENV_REAL (genie_cgs_bohr_radius, GSL_CONST_CGS_BOHR_RADIUS);
A68_ENV_REAL (genie_cgs_boltzmann, GSL_CONST_CGS_BOLTZMANN);
A68_ENV_REAL (genie_cgs_btu, GSL_CONST_CGS_BTU);
A68_ENV_REAL (genie_cgs_calorie, GSL_CONST_CGS_CALORIE);
A68_ENV_REAL (genie_cgs_canadian_gallon, GSL_CONST_CGS_CANADIAN_GALLON);
A68_ENV_REAL (genie_cgs_carat, GSL_CONST_CGS_CARAT);
A68_ENV_REAL (genie_cgs_cup, GSL_CONST_CGS_CUP);
A68_ENV_REAL (genie_cgs_curie, GSL_CONST_CGS_CURIE);
A68_ENV_REAL (genie_cgs_day, GSL_CONST_CGS_DAY);
A68_ENV_REAL (genie_cgs_dyne, GSL_CONST_CGS_DYNE);
A68_ENV_REAL (genie_cgs_electron_charge, GSL_CONST_CGS_ELECTRON_CHARGE);
A68_ENV_REAL (genie_cgs_electron_magnetic_moment, GSL_CONST_CGS_ELECTRON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_cgs_electron_volt, GSL_CONST_CGS_ELECTRON_VOLT);
A68_ENV_REAL (genie_cgs_erg, GSL_CONST_CGS_ERG);
A68_ENV_REAL (genie_cgs_faraday, GSL_CONST_CGS_FARADAY);
A68_ENV_REAL (genie_cgs_fathom, GSL_CONST_CGS_FATHOM);
A68_ENV_REAL (genie_cgs_fluid_ounce, GSL_CONST_CGS_FLUID_OUNCE);
A68_ENV_REAL (genie_cgs_foot, GSL_CONST_CGS_FOOT);
A68_ENV_REAL (genie_cgs_footcandle, GSL_CONST_CGS_FOOTCANDLE);
A68_ENV_REAL (genie_cgs_footlambert, GSL_CONST_CGS_FOOTLAMBERT);
A68_ENV_REAL (genie_cgs_gauss, GSL_CONST_CGS_GAUSS);
A68_ENV_REAL (genie_cgs_gram_force, GSL_CONST_CGS_GRAM_FORCE);
A68_ENV_REAL (genie_cgs_grav_accel, GSL_CONST_CGS_GRAV_ACCEL);
A68_ENV_REAL (genie_cgs_gravitational_constant, GSL_CONST_CGS_GRAVITATIONAL_CONSTANT);
A68_ENV_REAL (genie_cgs_hectare, GSL_CONST_CGS_HECTARE);
A68_ENV_REAL (genie_cgs_horsepower, GSL_CONST_CGS_HORSEPOWER);
A68_ENV_REAL (genie_cgs_hour, GSL_CONST_CGS_HOUR);
A68_ENV_REAL (genie_cgs_inch, GSL_CONST_CGS_INCH);
A68_ENV_REAL (genie_cgs_inch_of_mercury, GSL_CONST_CGS_INCH_OF_MERCURY);
A68_ENV_REAL (genie_cgs_inch_of_water, GSL_CONST_CGS_INCH_OF_WATER);
A68_ENV_REAL (genie_cgs_joule, GSL_CONST_CGS_JOULE);
A68_ENV_REAL (genie_cgs_kilometers_per_hour, GSL_CONST_CGS_KILOMETERS_PER_HOUR);
A68_ENV_REAL (genie_cgs_kilopound_force, GSL_CONST_CGS_KILOPOUND_FORCE);
A68_ENV_REAL (genie_cgs_knot, GSL_CONST_CGS_KNOT);
A68_ENV_REAL (genie_cgs_lambert, GSL_CONST_CGS_LAMBERT);
A68_ENV_REAL (genie_cgs_light_year, GSL_CONST_CGS_LIGHT_YEAR);
A68_ENV_REAL (genie_cgs_liter, GSL_CONST_CGS_LITER);
A68_ENV_REAL (genie_cgs_lumen, GSL_CONST_CGS_LUMEN);
A68_ENV_REAL (genie_cgs_lux, GSL_CONST_CGS_LUX);
A68_ENV_REAL (genie_cgs_mass_electron, GSL_CONST_CGS_MASS_ELECTRON);
A68_ENV_REAL (genie_cgs_mass_muon, GSL_CONST_CGS_MASS_MUON);
A68_ENV_REAL (genie_cgs_mass_neutron, GSL_CONST_CGS_MASS_NEUTRON);
A68_ENV_REAL (genie_cgs_mass_proton, GSL_CONST_CGS_MASS_PROTON);
A68_ENV_REAL (genie_cgs_meter_of_mercury, GSL_CONST_CGS_METER_OF_MERCURY);
A68_ENV_REAL (genie_cgs_metric_ton, GSL_CONST_CGS_METRIC_TON);
A68_ENV_REAL (genie_cgs_micron, GSL_CONST_CGS_MICRON);
A68_ENV_REAL (genie_cgs_mil, GSL_CONST_CGS_MIL);
A68_ENV_REAL (genie_cgs_mile, GSL_CONST_CGS_MILE);
A68_ENV_REAL (genie_cgs_miles_per_hour, GSL_CONST_CGS_MILES_PER_HOUR);
A68_ENV_REAL (genie_cgs_minute, GSL_CONST_CGS_MINUTE);
A68_ENV_REAL (genie_cgs_molar_gas, GSL_CONST_CGS_MOLAR_GAS);
A68_ENV_REAL (genie_cgs_nautical_mile, GSL_CONST_CGS_NAUTICAL_MILE);
A68_ENV_REAL (genie_cgs_newton, GSL_CONST_CGS_NEWTON);
A68_ENV_REAL (genie_cgs_nuclear_magneton, GSL_CONST_CGS_NUCLEAR_MAGNETON);
A68_ENV_REAL (genie_cgs_ounce_mass, GSL_CONST_CGS_OUNCE_MASS);
A68_ENV_REAL (genie_cgs_parsec, GSL_CONST_CGS_PARSEC);
A68_ENV_REAL (genie_cgs_phot, GSL_CONST_CGS_PHOT);
A68_ENV_REAL (genie_cgs_pint, GSL_CONST_CGS_PINT);
A68_ENV_REAL (genie_cgs_planck_constant_h, 6.6260693e-27);
A68_ENV_REAL (genie_cgs_planck_constant_hbar, 6.6260693e-27 / (2 * A68G_PI));
A68_ENV_REAL (genie_cgs_point, GSL_CONST_CGS_POINT);
A68_ENV_REAL (genie_cgs_poise, GSL_CONST_CGS_POISE);
A68_ENV_REAL (genie_cgs_pound_force, GSL_CONST_CGS_POUND_FORCE);
A68_ENV_REAL (genie_cgs_pound_mass, GSL_CONST_CGS_POUND_MASS);
A68_ENV_REAL (genie_cgs_poundal, GSL_CONST_CGS_POUNDAL);
A68_ENV_REAL (genie_cgs_proton_magnetic_moment, GSL_CONST_CGS_PROTON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_cgs_psi, GSL_CONST_CGS_PSI);
A68_ENV_REAL (genie_cgs_quart, GSL_CONST_CGS_QUART);
A68_ENV_REAL (genie_cgs_rad, GSL_CONST_CGS_RAD);
A68_ENV_REAL (genie_cgs_roentgen, GSL_CONST_CGS_ROENTGEN);
A68_ENV_REAL (genie_cgs_rydberg, GSL_CONST_CGS_RYDBERG);
A68_ENV_REAL (genie_cgs_solar_mass, GSL_CONST_CGS_SOLAR_MASS);
A68_ENV_REAL (genie_cgs_speed_of_light, GSL_CONST_CGS_SPEED_OF_LIGHT);
A68_ENV_REAL (genie_cgs_standard_gas_volume, GSL_CONST_CGS_STANDARD_GAS_VOLUME);
A68_ENV_REAL (genie_cgs_std_atmosphere, GSL_CONST_CGS_STD_ATMOSPHERE);
A68_ENV_REAL (genie_cgs_stilb, GSL_CONST_CGS_STILB);
A68_ENV_REAL (genie_cgs_stokes, GSL_CONST_CGS_STOKES);
A68_ENV_REAL (genie_cgs_tablespoon, GSL_CONST_CGS_TABLESPOON);
A68_ENV_REAL (genie_cgs_teaspoon, GSL_CONST_CGS_TEASPOON);
A68_ENV_REAL (genie_cgs_texpoint, GSL_CONST_CGS_TEXPOINT);
A68_ENV_REAL (genie_cgs_therm, GSL_CONST_CGS_THERM);
A68_ENV_REAL (genie_cgs_ton, GSL_CONST_CGS_TON);
A68_ENV_REAL (genie_cgs_torr, GSL_CONST_CGS_TORR);
A68_ENV_REAL (genie_cgs_troy_ounce, GSL_CONST_CGS_TROY_OUNCE);
A68_ENV_REAL (genie_cgs_uk_gallon, GSL_CONST_CGS_UK_GALLON);
A68_ENV_REAL (genie_cgs_uk_ton, GSL_CONST_CGS_UK_TON);
A68_ENV_REAL (genie_cgs_unified_atomic_mass, GSL_CONST_CGS_UNIFIED_ATOMIC_MASS);
A68_ENV_REAL (genie_cgs_us_gallon, GSL_CONST_CGS_US_GALLON);
A68_ENV_REAL (genie_cgs_week, GSL_CONST_CGS_WEEK);
A68_ENV_REAL (genie_cgs_yard, GSL_CONST_CGS_YARD);
A68_ENV_REAL (genie_mks_acre, GSL_CONST_MKS_ACRE);
A68_ENV_REAL (genie_mks_angstrom, GSL_CONST_MKS_ANGSTROM);
A68_ENV_REAL (genie_mks_astronomical_unit, GSL_CONST_MKS_ASTRONOMICAL_UNIT);
A68_ENV_REAL (genie_mks_bar, GSL_CONST_MKS_BAR);
A68_ENV_REAL (genie_mks_barn, GSL_CONST_MKS_BARN);
A68_ENV_REAL (genie_mks_bohr_magneton, GSL_CONST_MKS_BOHR_MAGNETON);
A68_ENV_REAL (genie_mks_bohr_radius, GSL_CONST_MKS_BOHR_RADIUS);
A68_ENV_REAL (genie_mks_boltzmann, GSL_CONST_MKS_BOLTZMANN);
A68_ENV_REAL (genie_mks_btu, GSL_CONST_MKS_BTU);
A68_ENV_REAL (genie_mks_calorie, GSL_CONST_MKS_CALORIE);
A68_ENV_REAL (genie_mks_canadian_gallon, GSL_CONST_MKS_CANADIAN_GALLON);
A68_ENV_REAL (genie_mks_carat, GSL_CONST_MKS_CARAT);
A68_ENV_REAL (genie_mks_cup, GSL_CONST_MKS_CUP);
A68_ENV_REAL (genie_mks_curie, GSL_CONST_MKS_CURIE);
A68_ENV_REAL (genie_mks_day, GSL_CONST_MKS_DAY);
A68_ENV_REAL (genie_mks_dyne, GSL_CONST_MKS_DYNE);
A68_ENV_REAL (genie_mks_electron_charge, GSL_CONST_MKS_ELECTRON_CHARGE);
A68_ENV_REAL (genie_mks_electron_magnetic_moment, GSL_CONST_MKS_ELECTRON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_mks_electron_volt, GSL_CONST_MKS_ELECTRON_VOLT);
A68_ENV_REAL (genie_mks_erg, GSL_CONST_MKS_ERG);
A68_ENV_REAL (genie_mks_faraday, GSL_CONST_MKS_FARADAY);
A68_ENV_REAL (genie_mks_fathom, GSL_CONST_MKS_FATHOM);
A68_ENV_REAL (genie_mks_fluid_ounce, GSL_CONST_MKS_FLUID_OUNCE);
A68_ENV_REAL (genie_mks_foot, GSL_CONST_MKS_FOOT);
A68_ENV_REAL (genie_mks_footcandle, GSL_CONST_MKS_FOOTCANDLE);
A68_ENV_REAL (genie_mks_footlambert, GSL_CONST_MKS_FOOTLAMBERT);
A68_ENV_REAL (genie_mks_gauss, GSL_CONST_MKS_GAUSS);
A68_ENV_REAL (genie_mks_gram_force, GSL_CONST_MKS_GRAM_FORCE);
A68_ENV_REAL (genie_mks_grav_accel, GSL_CONST_MKS_GRAV_ACCEL);
A68_ENV_REAL (genie_mks_gravitational_constant, GSL_CONST_MKS_GRAVITATIONAL_CONSTANT);
A68_ENV_REAL (genie_mks_hectare, GSL_CONST_MKS_HECTARE);
A68_ENV_REAL (genie_mks_horsepower, GSL_CONST_MKS_HORSEPOWER);
A68_ENV_REAL (genie_mks_hour, GSL_CONST_MKS_HOUR);
A68_ENV_REAL (genie_mks_inch, GSL_CONST_MKS_INCH);
A68_ENV_REAL (genie_mks_inch_of_mercury, GSL_CONST_MKS_INCH_OF_MERCURY);
A68_ENV_REAL (genie_mks_inch_of_water, GSL_CONST_MKS_INCH_OF_WATER);
A68_ENV_REAL (genie_mks_joule, GSL_CONST_MKS_JOULE);
A68_ENV_REAL (genie_mks_kilometers_per_hour, GSL_CONST_MKS_KILOMETERS_PER_HOUR);
A68_ENV_REAL (genie_mks_kilopound_force, GSL_CONST_MKS_KILOPOUND_FORCE);
A68_ENV_REAL (genie_mks_knot, GSL_CONST_MKS_KNOT);
A68_ENV_REAL (genie_mks_lambert, GSL_CONST_MKS_LAMBERT);
A68_ENV_REAL (genie_mks_light_year, GSL_CONST_MKS_LIGHT_YEAR);
A68_ENV_REAL (genie_mks_liter, GSL_CONST_MKS_LITER);
A68_ENV_REAL (genie_mks_lumen, GSL_CONST_MKS_LUMEN);
A68_ENV_REAL (genie_mks_lux, GSL_CONST_MKS_LUX);
A68_ENV_REAL (genie_mks_mass_electron, GSL_CONST_MKS_MASS_ELECTRON);
A68_ENV_REAL (genie_mks_mass_muon, GSL_CONST_MKS_MASS_MUON);
A68_ENV_REAL (genie_mks_mass_neutron, GSL_CONST_MKS_MASS_NEUTRON);
A68_ENV_REAL (genie_mks_mass_proton, GSL_CONST_MKS_MASS_PROTON);
A68_ENV_REAL (genie_mks_meter_of_mercury, GSL_CONST_MKS_METER_OF_MERCURY);
A68_ENV_REAL (genie_mks_metric_ton, GSL_CONST_MKS_METRIC_TON);
A68_ENV_REAL (genie_mks_micron, GSL_CONST_MKS_MICRON);
A68_ENV_REAL (genie_mks_mil, GSL_CONST_MKS_MIL);
A68_ENV_REAL (genie_mks_mile, GSL_CONST_MKS_MILE);
A68_ENV_REAL (genie_mks_miles_per_hour, GSL_CONST_MKS_MILES_PER_HOUR);
A68_ENV_REAL (genie_mks_minute, GSL_CONST_MKS_MINUTE);
A68_ENV_REAL (genie_mks_molar_gas, GSL_CONST_MKS_MOLAR_GAS);
A68_ENV_REAL (genie_mks_nautical_mile, GSL_CONST_MKS_NAUTICAL_MILE);
A68_ENV_REAL (genie_mks_newton, GSL_CONST_MKS_NEWTON);
A68_ENV_REAL (genie_mks_nuclear_magneton, GSL_CONST_MKS_NUCLEAR_MAGNETON);
A68_ENV_REAL (genie_mks_ounce_mass, GSL_CONST_MKS_OUNCE_MASS);
A68_ENV_REAL (genie_mks_parsec, GSL_CONST_MKS_PARSEC);
A68_ENV_REAL (genie_mks_phot, GSL_CONST_MKS_PHOT);
A68_ENV_REAL (genie_mks_pint, GSL_CONST_MKS_PINT);
A68_ENV_REAL (genie_mks_planck_constant_h, 6.6260693e-34);
A68_ENV_REAL (genie_mks_planck_constant_hbar, 6.6260693e-34 / (2 * A68G_PI));
A68_ENV_REAL (genie_mks_point, GSL_CONST_MKS_POINT);
A68_ENV_REAL (genie_mks_poise, GSL_CONST_MKS_POISE);
A68_ENV_REAL (genie_mks_pound_force, GSL_CONST_MKS_POUND_FORCE);
A68_ENV_REAL (genie_mks_pound_mass, GSL_CONST_MKS_POUND_MASS);
A68_ENV_REAL (genie_mks_poundal, GSL_CONST_MKS_POUNDAL);
A68_ENV_REAL (genie_mks_proton_magnetic_moment, GSL_CONST_MKS_PROTON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_mks_psi, GSL_CONST_MKS_PSI);
A68_ENV_REAL (genie_mks_quart, GSL_CONST_MKS_QUART);
A68_ENV_REAL (genie_mks_rad, GSL_CONST_MKS_RAD);
A68_ENV_REAL (genie_mks_roentgen, GSL_CONST_MKS_ROENTGEN);
A68_ENV_REAL (genie_mks_rydberg, GSL_CONST_MKS_RYDBERG);
A68_ENV_REAL (genie_mks_solar_mass, GSL_CONST_MKS_SOLAR_MASS);
A68_ENV_REAL (genie_mks_speed_of_light, GSL_CONST_MKS_SPEED_OF_LIGHT);
A68_ENV_REAL (genie_mks_standard_gas_volume, GSL_CONST_MKS_STANDARD_GAS_VOLUME);
A68_ENV_REAL (genie_mks_std_atmosphere, GSL_CONST_MKS_STD_ATMOSPHERE);
A68_ENV_REAL (genie_mks_stilb, GSL_CONST_MKS_STILB);
A68_ENV_REAL (genie_mks_stokes, GSL_CONST_MKS_STOKES);
A68_ENV_REAL (genie_mks_tablespoon, GSL_CONST_MKS_TABLESPOON);
A68_ENV_REAL (genie_mks_teaspoon, GSL_CONST_MKS_TEASPOON);
A68_ENV_REAL (genie_mks_texpoint, GSL_CONST_MKS_TEXPOINT);
A68_ENV_REAL (genie_mks_therm, GSL_CONST_MKS_THERM);
A68_ENV_REAL (genie_mks_ton, GSL_CONST_MKS_TON);
A68_ENV_REAL (genie_mks_torr, GSL_CONST_MKS_TORR);
A68_ENV_REAL (genie_mks_troy_ounce, GSL_CONST_MKS_TROY_OUNCE);
A68_ENV_REAL (genie_mks_uk_gallon, GSL_CONST_MKS_UK_GALLON);
A68_ENV_REAL (genie_mks_uk_ton, GSL_CONST_MKS_UK_TON);
A68_ENV_REAL (genie_mks_unified_atomic_mass, GSL_CONST_MKS_UNIFIED_ATOMIC_MASS);
A68_ENV_REAL (genie_mks_us_gallon, GSL_CONST_MKS_US_GALLON);
A68_ENV_REAL (genie_mks_vacuum_permeability, GSL_CONST_MKS_VACUUM_PERMEABILITY);
A68_ENV_REAL (genie_mks_vacuum_permittivity, GSL_CONST_MKS_VACUUM_PERMITTIVITY);
A68_ENV_REAL (genie_mks_week, GSL_CONST_MKS_WEEK);
A68_ENV_REAL (genie_mks_yard, GSL_CONST_MKS_YARD);
A68_ENV_REAL (genie_num_atto, GSL_CONST_NUM_ATTO);
A68_ENV_REAL (genie_num_avogadro, GSL_CONST_NUM_AVOGADRO);
A68_ENV_REAL (genie_num_exa, GSL_CONST_NUM_EXA);
A68_ENV_REAL (genie_num_femto, GSL_CONST_NUM_FEMTO);
A68_ENV_REAL (genie_num_fine_structure, GSL_CONST_NUM_FINE_STRUCTURE);
A68_ENV_REAL (genie_num_giga, GSL_CONST_NUM_GIGA);
A68_ENV_REAL (genie_num_kilo, GSL_CONST_NUM_KILO);
A68_ENV_REAL (genie_num_mega, GSL_CONST_NUM_MEGA);
A68_ENV_REAL (genie_num_micro, GSL_CONST_NUM_MICRO);
A68_ENV_REAL (genie_num_milli, GSL_CONST_NUM_MILLI);
A68_ENV_REAL (genie_num_nano, GSL_CONST_NUM_NANO);
A68_ENV_REAL (genie_num_peta, GSL_CONST_NUM_PETA);
A68_ENV_REAL (genie_num_pico, GSL_CONST_NUM_PICO);
A68_ENV_REAL (genie_num_tera, GSL_CONST_NUM_TERA);
A68_ENV_REAL (genie_num_yocto, GSL_CONST_NUM_YOCTO);
A68_ENV_REAL (genie_num_yotta, GSL_CONST_NUM_YOTTA);
A68_ENV_REAL (genie_num_zepto, GSL_CONST_NUM_ZEPTO);
A68_ENV_REAL (genie_num_zetta, GSL_CONST_NUM_ZETTA);
#endif

/* Macros. */

#define C_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  x->value = f (x->value);\
  math_rte (p, errno != 0, MODE (REAL), NULL);

#define OWN_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  x->value = f (p, x->value);\
  math_rte (p, errno != 0, MODE (REAL), NULL);

#define GSL_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  x->value = f (x->value);\
  math_rte (p, errno != 0, MODE (REAL), NULL);

#define GSL_COMPLEX_FUNCTION(f)\
  gsl_complex x, z;\
  A68_REAL *rex, *imx;\
  imx = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));\
  rex = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));\
  GSL_SET_COMPLEX (&x, rex->value, imx->value);\
  (void) gsl_set_error_handler_off ();\
  RESET_ERRNO;\
  z = f (x);\
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);\
  imx->value = GSL_IMAG(z);\
  rex->value = GSL_REAL(z)

#define GSL_1_FUNCTION(p, f)\
  A68_REAL *x;\
  gsl_sf_result y;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (x->value, &y);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  x->value = y.val

#define GSL_2_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (x->value, y->value, &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  x->value = r.val

#define GSL_2_INT_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f ((int) x->value, y->value, &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  x->value = r.val

#define GSL_3_FUNCTION(p, f)\
  A68_REAL *x, *y, *z;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (x->value, y->value, z->value,  &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  x->value = r.val

#define GSL_1D_FUNCTION(p, f)\
  A68_REAL *x;\
  gsl_sf_result y;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (x->value, GSL_PREC_DOUBLE, &y);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  x->value = y.val

#define GSL_2D_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (x->value, y->value, GSL_PREC_DOUBLE, &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  x->value = r.val

#define GSL_3D_FUNCTION(p, f)\
  A68_REAL *x, *y, *z;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (x->value, y->value, z->value, GSL_PREC_DOUBLE, &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  x->value = r.val

#define GSL_4D_FUNCTION(p, f)\
  A68_REAL *x, *y, *z, *rho;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, rho, A68_REAL);\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (x->value, y->value, z->value, rho->value, GSL_PREC_DOUBLE, &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  x->value = r.val

/*!
\brief the cube root of x
\param x
\return
**/

double curt (double x)
{
#define CBRT2 1.2599210498948731647672;
#define CBRT4 1.5874010519681994747517;
  int expo, sign;
  double z, x0;
  static double y[11] = {
    7.937005259840997e-01,
    8.193212706006459e-01,
    8.434326653017493e-01,
    8.662391053409029e-01,
    8.879040017426008e-01,
    9.085602964160699e-01,
    9.283177667225558e-01,
    9.472682371859097e-01,
    9.654893846056298e-01,
    9.830475724915586e-01,
    1.0
  };
  if (x == 0.0 || x == 1.0) {
    return (x);
  }
  if (x > 0.0) {
    sign = 1;
  } else {
    sign = -1;
    x = -x;
  }
  x = frexp (x, &expo);
/* Cube root in [0.5, 1] by Newton's method. */
  z = x;
  x = y[(int) (20 * x - 10)];
  x0 = 0;
  while (ABS (x - x0) > DBL_EPSILON) {
    x0 = x;
    x = (z / (x * x) + x + x) / 3;
  }
/* Get exponent. */
  if (expo >= 0) {
    int j = expo % 3;
    if (j == 1) {
      x *= CBRT2;
    } else if (j == 2) {
      x *= CBRT4;
    }
    expo /= 3;
  } else {
    int j = (-expo) % 3;
    if (j == 1) {
      x /= CBRT2;
    } else if (j == 2) {
      x /= CBRT4;
    }
    expo = -(-expo) / 3;
  }
  x = ldexp (x, expo);
  return (sign >= 0 ? x : -x);
}

/*!
\brief inverse complementary error function
\param y
\return
**/

double inverfc (double y)
{
  if (y < 0.0 || y > 2.0) {
    errno = EDOM;
    return (0.0);
  } else if (y == 0.0) {
    return (DBL_MAX);
  } else if (y == 1.0) {
    return (0.0);
  } else if (y == 2.0) {
    return (-DBL_MAX);
  } else {
/* Next is adapted code from a package that contains following statement:
   Copyright (c) 1996 Takuya Ooura.
   You may use, copy, modify this code for any purpose and without fee. */
    double s, t, u, v, x, z;
    if (y <= 1.0) {
      z = y;
    } else {
      z = 2.0 - y;
    }
    v = 0.916461398268964 - log (z);
    u = sqrt (v);
    s = (log (u) + 0.488826640273108) / v;
    t = 1.0 / (u + 0.231729200323405);
    x = u * (1.0 - s * (s * 0.124610454613712 + 0.5)) - ((((-0.0728846765585675 * t + 0.269999308670029) * t + 0.150689047360223) * t + 0.116065025341614) * t + 0.499999303439796) * t;
    t = 3.97886080735226 / (x + 3.97886080735226);
    u = t - 0.5;
    s = (((((((((0.00112648096188977922 * u + 1.05739299623423047e-4) * u - 0.00351287146129100025) * u - 7.71708358954120939e-4) * u + 0.00685649426074558612) * u + 0.00339721910367775861) * u - 0.011274916933250487) * u - 0.0118598117047771104) * u + 0.0142961988697898018) * u + 0.0346494207789099922) * u + 0.00220995927012179067;
    s = ((((((((((((s * u - 0.0743424357241784861) * u - 0.105872177941595488) * u + 0.0147297938331485121) * u + 0.316847638520135944) * u + 0.713657635868730364) * u + 1.05375024970847138) * u + 1.21448730779995237) * u + 1.16374581931560831) * u + 0.956464974744799006) * u + 0.686265948274097816) * u + 0.434397492331430115) * u + 0.244044510593190935) * t - z * exp (x * x - 0.120782237635245222);
    x += s * (x * s + 1.0);
    return (y <= 1.0 ? x : -x);
  }
}

/*!
\brief inverse error function
\param y
\return
**/

double inverf (double y)
{
  return (inverfc (1 - y));
}

/*!
\brief PROC sqrt = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_sqrt_real (NODE_T * p)
{
  C_FUNCTION (p, sqrt);
}

/*!
\brief PROC curt = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_curt_real (NODE_T * p)
{
  C_FUNCTION (p, curt);
}

/*!
\brief PROC exp = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_exp_real (NODE_T * p)
{
  C_FUNCTION (p, exp);
}

/*!
\brief PROC ln = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_ln_real (NODE_T * p)
{
  C_FUNCTION (p, log);
}

/*!
\brief PROC log = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_log_real (NODE_T * p)
{
  C_FUNCTION (p, log10);
}

/*!
\brief PROC sin = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_sin_real (NODE_T * p)
{
  C_FUNCTION (p, sin);
}

/*!
\brief PROC arcsin = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arcsin_real (NODE_T * p)
{
  C_FUNCTION (p, asin);
}

/*!
\brief PROC cos = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_cos_real (NODE_T * p)
{
  C_FUNCTION (p, cos);
}

/*!
\brief PROC arccos = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arccos_real (NODE_T * p)
{
  C_FUNCTION (p, acos);
}

/*!
\brief PROC tan = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_tan_real (NODE_T * p)
{
  C_FUNCTION (p, tan);
}

/*!
\brief PROC arctan = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arctan_real (NODE_T * p)
{
  C_FUNCTION (p, atan);
}

/*!
\brief PROC arctan2 = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_atan2_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  RESET_ERRNO;
  if (x->value == 0.0 && y->value == 0.0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (LONG_COMPLEX), NULL);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  x->value = atan2 (x->value, y->value);
  if (errno != 0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_MATH_EXCEPTION);
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief PROC sinh = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_sinh_real (NODE_T * p)
{
  C_FUNCTION (p, sinh);
}

/*!
\brief PROC cosh = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_cosh_real (NODE_T * p)
{
  C_FUNCTION (p, cosh);
}

/*!
\brief PROC tanh = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_tanh_real (NODE_T * p)
{
  C_FUNCTION (p, tanh);
}

/*!
\brief PROC arcsinh = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arcsinh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_asinh);
}

/*!
\brief PROC arccosh = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arccosh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_acosh);
}

/*!
\brief PROC arctanh = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arctanh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_atanh);
}

/*!
\brief PROC inverse erf = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_inverf_real (NODE_T * p)
{
  C_FUNCTION (p, inverf);
}

/*!
\brief PROC inverse erfc = (REAL) REAL 
\param p position in syntax tree, should not be NULL
**/

void genie_inverfc_real (NODE_T * p)
{
  C_FUNCTION (p, inverfc);
}

#ifdef HAVE_GSL

/*!
\brief PROC csinh = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_sinh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_sinh);
}

/*!
\brief PROC ccosh = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_cosh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_cosh);
}

/*!
\brief PROC ctanh = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_tanh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_tanh);
}

/*!
\brief PROC carcsinh = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_arcsinh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arcsinh);
}

/*!
\brief PROC carccosh = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_arccosh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arccosh);
}

/*!
\brief PROC carctanh = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_arctanh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arctanh);
}

/* "Special" functions - but what is so "special" about them? */

/*!
\brief PROC erf = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_erf_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_erf_e);
}

/*!
\brief PROC erfc = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_erfc_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_erfc_e);
}

/*!
\brief PROC gamma = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_gamma_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_gamma_e);
}

/*!
\brief PROC gamma incomplete = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_gamma_inc_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_gamma_inc_P_e);
}

/*!
\brief PROC lngamma = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_lngamma_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_lngamma_e);
}

/*!
\brief PROC factorial = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_factorial_real (NODE_T * p)
{
/* gsl_sf_fact reduces argument to int, hence we do gamma (x + 1) */
  A68_REAL *x = (A68_REAL *) STACK_OFFSET (-SIZE_OF (A68_REAL));
  x->value += 1.0;
  {
    GSL_1_FUNCTION (p, gsl_sf_gamma_e);
  }
}

/*!
\brief PROC beta = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_beta_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_beta_e);
}

/*!
\brief PROC beta incomplete = (REAL, REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_beta_inc_real (NODE_T * p)
{
  GSL_3_FUNCTION (p, gsl_sf_beta_inc_e);
}

/*!
\brief PROC airy ai = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_airy_ai_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Ai_e);
}

/*!
\brief PROC airy bi = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_airy_bi_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Bi_e);
}

/*!
\brief PROC airy ai derivative = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_airy_ai_deriv_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Ai_deriv_e);
}

/*!
\brief PROC airy bi derivative = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_airy_bi_deriv_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Bi_deriv_e);
}

/*!
\brief PROC bessel jn = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_jn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Jn_e);
}

/*!
\brief PROC bessel yn = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_yn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Yn_e);
}

/*!
\brief PROC bessel in = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_in_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_In_e);
}

/*!
\brief PROC bessel exp in = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
\return
**/

void genie_bessel_exp_in_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_In_scaled_e);
}

/*!
\brief PROC bessel kn = (REAL, REAL) REAL 
\param p position in syntax tree, should not be NULL
\return
**/

void genie_bessel_kn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Kn_e);
}

/*!
\brief PROC bessel exp kn = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_exp_kn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Kn_scaled_e);
}

/*!
\brief PROC bessel jl = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_jl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_jl_e);
}

/*!
\brief PROC bessel yl = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_yl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_yl_e);
}

/*!
\brief PROC bessel exp il = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_exp_il_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_il_scaled_e);
}

/*!
\brief PROC bessel exp kl = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_exp_kl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_kl_scaled_e);
}

/*!
\brief PROC bessel jnu = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_jnu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Jnu_e);
}

/*!
\brief PROC bessel ynu = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_ynu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Ynu_e);
}

/*!
\brief PROC bessel inu = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_inu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Inu_e);
}

/*!
\brief PROC bessel exp inu = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_exp_inu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Inu_scaled_e);
}

/*!
\brief PROC bessel knu = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_knu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Knu_e);
}

/*!
\brief PROC bessel exp knu = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_bessel_exp_knu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Knu_scaled_e);
}

/*!
\brief PROC elliptic integral k = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_elliptic_integral_k_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_ellint_Kcomp_e);
}

/*!
\brief PROC elliptic integral e = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_elliptic_integral_e_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_ellint_Ecomp_e);
}

/*!
\brief PROC elliptic integral rf = (REAL, REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_elliptic_integral_rf_real (NODE_T * p)
{
  GSL_3D_FUNCTION (p, gsl_sf_ellint_RF_e);
}

/*!
\brief PROC elliptic integral rd = (REAL, REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_elliptic_integral_rd_real (NODE_T * p)
{
  GSL_3D_FUNCTION (p, gsl_sf_ellint_RD_e);
}

/*!
\brief PROC elliptic integral rj = (REAL, REAL, REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_elliptic_integral_rj_real (NODE_T * p)
{
  GSL_4D_FUNCTION (p, gsl_sf_ellint_RJ_e);
}

/*!
\brief PROC elliptic integral rc = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_elliptic_integral_rc_real (NODE_T * p)
{
  GSL_2D_FUNCTION (p, gsl_sf_ellint_RC_e);
}

#endif

/*
Next part is a "stand-alone" version of GNU Scientific Library (GSL)
random number generator "taus113", based on GSL file "rng/taus113.c" that
has the notice:

Copyright (C) 2002 Atakan Gurkan
Based on the file taus.c which has the notice
Copyright (C) 1996, 1997, 1998, 1999, 2000 James Theiler, Brian Gough.

This is a maximally equidistributed combined, collision free
Tausworthe generator, with a period ~2^113 (~10^34).
The sequence is

x_n = (z1_n ^ z2_n ^ z3_n ^ z4_n)

b = (((z1_n <<  6) ^ z1_n) >> 13)
z1_{n+1} = (((z1_n & 4294967294) << 18) ^ b)
b = (((z2_n <<  2) ^ z2_n) >> 27)
z2_{n+1} = (((z2_n & 4294967288) <<  2) ^ b)
b = (((z3_n << 13) ^ z3_n) >> 21)
z3_{n+1} = (((z3_n & 4294967280) <<  7) ^ b)
b = (((z4_n <<  3)  ^ z4_n) >> 12)
z4_{n+1} = (((z4_n & 4294967168) << 13) ^ b)

computed modulo 2^32. In the formulas above '^' means exclusive-or
(C-notation), not exponentiation.
The algorithm is for 32-bit integers, hence a bitmask is used to clear
all but least significant 32 bits, after left shifts, to make the code
work on architectures where integers are 64-bit.

The generator is initialized with
zi = (69069 * z{i+1}) MOD 2^32 where z0 is the seed provided
During initialization a check is done to make sure that the initial seeds
have a required number of their most significant bits set.
After this, the state is passed through the RNG 10 times to ensure the
state satisfies a recurrence relation.

References:
P. L'Ecuyer, "Tables of Maximally-Equidistributed Combined LFSR Generators",
Mathematics of Computation, 68, 225 (1999), 261--269.
  http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme2.ps
P. L'Ecuyer, "Maximally Equidistributed Combined Tausworthe Generators",
Mathematics of Computation, 65, 213 (1996), 203--213.
  http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme.ps
the online version of the latter contains corrections to the print version.
*/

#define LCG(n) ((69069UL * n) & 0xffffffffUL)
#define TAUSWORTHE_MASK 0xffffffffUL

typedef struct {
  unsigned long int z1, z2, z3, z4;
} taus113_state_t;

static taus113_state_t rng_state;

static unsigned long int taus113_get (taus113_state_t * state);
static void taus113_set (taus113_state_t * state, unsigned long int s);

/*!
\brief taus113_get
\param state
\return
**/

static unsigned long taus113_get (taus113_state_t * state)
{
  unsigned long b1, b2, b3, b4;
  b1 = ((((state->z1 << 6UL) & TAUSWORTHE_MASK) ^ state->z1) >> 13UL);
  state->z1 = ((((state->z1 & 4294967294UL) << 18UL) & TAUSWORTHE_MASK) ^ b1);
  b2 = ((((state->z2 << 2UL) & TAUSWORTHE_MASK) ^ state->z2) >> 27UL);
  state->z2 = ((((state->z2 & 4294967288UL) << 2UL) & TAUSWORTHE_MASK) ^ b2);
  b3 = ((((state->z3 << 13UL) & TAUSWORTHE_MASK) ^ state->z3) >> 21UL);
  state->z3 = ((((state->z3 & 4294967280UL) << 7UL) & TAUSWORTHE_MASK) ^ b3);
  b4 = ((((state->z4 << 3UL) & TAUSWORTHE_MASK) ^ state->z4) >> 12UL);
  state->z4 = ((((state->z4 & 4294967168UL) << 13UL) & TAUSWORTHE_MASK) ^ b4);
  return (state->z1 ^ state->z2 ^ state->z3 ^ state->z4);
}

/*!
\brief taus113_set
\param state
\param s
**/

static void taus113_set (taus113_state_t * state, unsigned long int s)
{
  if (!s) {
/* default seed is 1 */
    s = 1UL;
  }
  state->z1 = LCG (s);
  if (state->z1 < 2UL) {
    state->z1 += 2UL;
  }
  state->z2 = LCG (state->z1);
  if (state->z2 < 8UL) {
    state->z2 += 8UL;
  }
  state->z3 = LCG (state->z2);
  if (state->z3 < 16UL) {
    state->z3 += 16UL;
  }
  state->z4 = LCG (state->z3);
  if (state->z4 < 128UL) {
    state->z4 += 128UL;
  }
/* Calling RNG ten times to satify recurrence condition. */
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
}

/*!
\brief initialise rng
\param u
**/

void init_rng (unsigned long u)
{
  taus113_set (&rng_state, u);
}

/*!
\brief rng 53 bit
\param
\return
**/

double rng_53_bit (void)
{
  double a = (double) (taus113_get (&rng_state) >> 5);
  double b = (double) (taus113_get (&rng_state) >> 6);
  return (a * /* 2^26 */ 67108864.0 + b) / /* 2^53 */ 9007199254740992.0;
}

/*
Rules for analytic calculations using GNU Emacs Calc:
(used to find the values for the test program)

[ LCG(n) := n * 69069 mod (2^32) ]

[ b1(x) := rsh(xor(lsh(x, 6), x), 13),
q1(x) := xor(lsh(and(x, 4294967294), 18), b1(x)),
b2(x) := rsh(xor(lsh(x, 2), x), 27),
q2(x) := xor(lsh(and(x, 4294967288), 2), b2(x)),
b3(x) := rsh(xor(lsh(x, 13), x), 21),
q3(x) := xor(lsh(and(x, 4294967280), 7), b3(x)),
b4(x) := rsh(xor(lsh(x, 3), x), 12),
q4(x) := xor(lsh(and(x, 4294967168), 13), b4(x))
]

[ S([z1,z2,z3,z4]) := [q1(z1), q2(z2), q3(z3), q4(z4)] ]		 
*/
