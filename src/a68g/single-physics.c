//! @file single-physics.c
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
//! Physical constants from GSL.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-physics.h"
#include "a68g-numbers.h"

#if defined (HAVE_GSL)

A68_ENV_REAL (genie_cgs_acre, GSL_CONST_CGSM_ACRE);
A68_ENV_REAL (genie_cgs_angstrom, GSL_CONST_CGSM_ANGSTROM);
A68_ENV_REAL (genie_cgs_astronomical_unit, GSL_CONST_CGSM_ASTRONOMICAL_UNIT);
A68_ENV_REAL (genie_cgs_bar, GSL_CONST_CGSM_BAR);
A68_ENV_REAL (genie_cgs_barn, GSL_CONST_CGSM_BARN);
A68_ENV_REAL (genie_cgs_bohr_magneton, GSL_CONST_CGSM_BOHR_MAGNETON);
A68_ENV_REAL (genie_cgs_bohr_radius, GSL_CONST_CGSM_BOHR_RADIUS);
A68_ENV_REAL (genie_cgs_boltzmann, GSL_CONST_CGSM_BOLTZMANN);
A68_ENV_REAL (genie_cgs_btu, GSL_CONST_CGSM_BTU);
A68_ENV_REAL (genie_cgs_calorie, GSL_CONST_CGSM_CALORIE);
A68_ENV_REAL (genie_cgs_canadian_gallon, GSL_CONST_CGSM_CANADIAN_GALLON);
A68_ENV_REAL (genie_cgs_carat, GSL_CONST_CGSM_CARAT);
A68_ENV_REAL (genie_cgs_cup, GSL_CONST_CGSM_CUP);
A68_ENV_REAL (genie_cgs_curie, GSL_CONST_CGSM_CURIE);
A68_ENV_REAL (genie_cgs_day, GSL_CONST_CGSM_DAY);
A68_ENV_REAL (genie_cgs_dyne, GSL_CONST_CGSM_DYNE);
A68_ENV_REAL (genie_cgs_electron_charge, GSL_CONST_CGSM_ELECTRON_CHARGE);
A68_ENV_REAL (genie_cgs_electron_magnetic_moment, GSL_CONST_CGSM_ELECTRON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_cgs_electron_volt, GSL_CONST_CGSM_ELECTRON_VOLT);
A68_ENV_REAL (genie_cgs_erg, GSL_CONST_CGSM_ERG);
A68_ENV_REAL (genie_cgs_faraday, GSL_CONST_CGSM_FARADAY);
A68_ENV_REAL (genie_cgs_fathom, GSL_CONST_CGSM_FATHOM);
A68_ENV_REAL (genie_cgs_fluid_ounce, GSL_CONST_CGSM_FLUID_OUNCE);
A68_ENV_REAL (genie_cgs_foot, GSL_CONST_CGSM_FOOT);
A68_ENV_REAL (genie_cgs_footcandle, GSL_CONST_CGSM_FOOTCANDLE);
A68_ENV_REAL (genie_cgs_footlambert, GSL_CONST_CGSM_FOOTLAMBERT);
A68_ENV_REAL (genie_cgs_gauss, GSL_CONST_CGSM_GAUSS);
A68_ENV_REAL (genie_cgs_gram_force, GSL_CONST_CGSM_GRAM_FORCE);
A68_ENV_REAL (genie_cgs_grav_accel, GSL_CONST_CGSM_GRAV_ACCEL);
A68_ENV_REAL (genie_cgs_gravitational_constant, GSL_CONST_CGSM_GRAVITATIONAL_CONSTANT);
A68_ENV_REAL (genie_cgs_hectare, GSL_CONST_CGSM_HECTARE);
A68_ENV_REAL (genie_cgs_horsepower, GSL_CONST_CGSM_HORSEPOWER);
A68_ENV_REAL (genie_cgs_hour, GSL_CONST_CGSM_HOUR);
A68_ENV_REAL (genie_cgs_inch, GSL_CONST_CGSM_INCH);
A68_ENV_REAL (genie_cgs_inch_of_mercury, GSL_CONST_CGSM_INCH_OF_MERCURY);
A68_ENV_REAL (genie_cgs_inch_of_water, GSL_CONST_CGSM_INCH_OF_WATER);
A68_ENV_REAL (genie_cgs_joule, GSL_CONST_CGSM_JOULE);
A68_ENV_REAL (genie_cgs_kilometers_per_hour, GSL_CONST_CGSM_KILOMETERS_PER_HOUR);
A68_ENV_REAL (genie_cgs_kilopound_force, GSL_CONST_CGSM_KILOPOUND_FORCE);
A68_ENV_REAL (genie_cgs_knot, GSL_CONST_CGSM_KNOT);
A68_ENV_REAL (genie_cgs_lambert, GSL_CONST_CGSM_LAMBERT);
A68_ENV_REAL (genie_cgs_light_year, GSL_CONST_CGSM_LIGHT_YEAR);
A68_ENV_REAL (genie_cgs_liter, GSL_CONST_CGSM_LITER);
A68_ENV_REAL (genie_cgs_lumen, GSL_CONST_CGSM_LUMEN);
A68_ENV_REAL (genie_cgs_lux, GSL_CONST_CGSM_LUX);
A68_ENV_REAL (genie_cgs_mass_electron, GSL_CONST_CGSM_MASS_ELECTRON);
A68_ENV_REAL (genie_cgs_mass_muon, GSL_CONST_CGSM_MASS_MUON);
A68_ENV_REAL (genie_cgs_mass_neutron, GSL_CONST_CGSM_MASS_NEUTRON);
A68_ENV_REAL (genie_cgs_mass_proton, GSL_CONST_CGSM_MASS_PROTON);
A68_ENV_REAL (genie_cgs_meter_of_mercury, GSL_CONST_CGSM_METER_OF_MERCURY);
A68_ENV_REAL (genie_cgs_metric_ton, GSL_CONST_CGSM_METRIC_TON);
A68_ENV_REAL (genie_cgs_micron, GSL_CONST_CGSM_MICRON);
A68_ENV_REAL (genie_cgs_mil, GSL_CONST_CGSM_MIL);
A68_ENV_REAL (genie_cgs_mile, GSL_CONST_CGSM_MILE);
A68_ENV_REAL (genie_cgs_miles_per_hour, GSL_CONST_CGSM_MILES_PER_HOUR);
A68_ENV_REAL (genie_cgs_minute, GSL_CONST_CGSM_MINUTE);
A68_ENV_REAL (genie_cgs_molar_gas, GSL_CONST_CGSM_MOLAR_GAS);
A68_ENV_REAL (genie_cgs_nautical_mile, GSL_CONST_CGSM_NAUTICAL_MILE);
A68_ENV_REAL (genie_cgs_newton, GSL_CONST_CGSM_NEWTON);
A68_ENV_REAL (genie_cgs_nuclear_magneton, GSL_CONST_CGSM_NUCLEAR_MAGNETON);
A68_ENV_REAL (genie_cgs_ounce_mass, GSL_CONST_CGSM_OUNCE_MASS);
A68_ENV_REAL (genie_cgs_parsec, GSL_CONST_CGSM_PARSEC);
A68_ENV_REAL (genie_cgs_phot, GSL_CONST_CGSM_PHOT);
A68_ENV_REAL (genie_cgs_pint, GSL_CONST_CGSM_PINT);
A68_ENV_REAL (genie_cgs_planck_constant_h, 6.6260693e-27);
A68_ENV_REAL (genie_cgs_planck_constant_hbar, 6.6260693e-27 / (2 * CONST_PI));
A68_ENV_REAL (genie_cgs_point, GSL_CONST_CGSM_POINT);
A68_ENV_REAL (genie_cgs_poise, GSL_CONST_CGSM_POISE);
A68_ENV_REAL (genie_cgs_pound_force, GSL_CONST_CGSM_POUND_FORCE);
A68_ENV_REAL (genie_cgs_pound_mass, GSL_CONST_CGSM_POUND_MASS);
A68_ENV_REAL (genie_cgs_poundal, GSL_CONST_CGSM_POUNDAL);
A68_ENV_REAL (genie_cgs_proton_magnetic_moment, GSL_CONST_CGSM_PROTON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_cgs_psi, GSL_CONST_CGSM_PSI);
A68_ENV_REAL (genie_cgs_quart, GSL_CONST_CGSM_QUART);
A68_ENV_REAL (genie_cgs_rad, GSL_CONST_CGSM_RAD);
A68_ENV_REAL (genie_cgs_roentgen, GSL_CONST_CGSM_ROENTGEN);
A68_ENV_REAL (genie_cgs_rydberg, GSL_CONST_CGSM_RYDBERG);
A68_ENV_REAL (genie_cgs_solar_mass, GSL_CONST_CGSM_SOLAR_MASS);
A68_ENV_REAL (genie_cgs_speed_of_light, GSL_CONST_CGSM_SPEED_OF_LIGHT);
A68_ENV_REAL (genie_cgs_standard_gas_volume, GSL_CONST_CGSM_STANDARD_GAS_VOLUME);
A68_ENV_REAL (genie_cgs_std_atmosphere, GSL_CONST_CGSM_STD_ATMOSPHERE);
A68_ENV_REAL (genie_cgs_stilb, GSL_CONST_CGSM_STILB);
A68_ENV_REAL (genie_cgs_stokes, GSL_CONST_CGSM_STOKES);
A68_ENV_REAL (genie_cgs_tablespoon, GSL_CONST_CGSM_TABLESPOON);
A68_ENV_REAL (genie_cgs_teaspoon, GSL_CONST_CGSM_TEASPOON);
A68_ENV_REAL (genie_cgs_texpoint, GSL_CONST_CGSM_TEXPOINT);
A68_ENV_REAL (genie_cgs_therm, GSL_CONST_CGSM_THERM);
A68_ENV_REAL (genie_cgs_ton, GSL_CONST_CGSM_TON);
A68_ENV_REAL (genie_cgs_torr, GSL_CONST_CGSM_TORR);
A68_ENV_REAL (genie_cgs_troy_ounce, GSL_CONST_CGSM_TROY_OUNCE);
A68_ENV_REAL (genie_cgs_uk_gallon, GSL_CONST_CGSM_UK_GALLON);
A68_ENV_REAL (genie_cgs_uk_ton, GSL_CONST_CGSM_UK_TON);
A68_ENV_REAL (genie_cgs_unified_atomic_mass, GSL_CONST_CGSM_UNIFIED_ATOMIC_MASS);
A68_ENV_REAL (genie_cgs_us_gallon, GSL_CONST_CGSM_US_GALLON);
A68_ENV_REAL (genie_cgs_week, GSL_CONST_CGSM_WEEK);
A68_ENV_REAL (genie_cgs_yard, GSL_CONST_CGSM_YARD);
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
A68_ENV_REAL (genie_mks_planck_constant_hbar, 6.6260693e-34 / (2 * CONST_PI));
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
