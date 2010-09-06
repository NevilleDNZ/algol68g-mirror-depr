/*!
\file gsl.h
\brief contains gsl and calculus related definitions
**/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program. If not, see <http://www.gnu.org/licenses/>.
*/

#if ! defined A68G_GSL_H
#define A68G_GSL_H

extern GENIE_PROCEDURE genie_exp_complex;
extern GENIE_PROCEDURE genie_cos_complex;
extern GENIE_PROCEDURE genie_cosh_complex;
extern GENIE_PROCEDURE genie_arccos_complex;
extern GENIE_PROCEDURE genie_arccosh_complex;
extern GENIE_PROCEDURE genie_arccosh_real;
extern GENIE_PROCEDURE genie_arcsin_complex;
extern GENIE_PROCEDURE genie_arcsinh_complex;
extern GENIE_PROCEDURE genie_arcsinh_real;
extern GENIE_PROCEDURE genie_arctan_complex;
extern GENIE_PROCEDURE genie_arctanh_complex;
extern GENIE_PROCEDURE genie_arctanh_real;
extern GENIE_PROCEDURE genie_inverf_real;
extern GENIE_PROCEDURE genie_inverfc_real;
extern GENIE_PROCEDURE genie_ln_complex;
extern GENIE_PROCEDURE genie_sin_complex;
extern GENIE_PROCEDURE genie_sinh_complex;
extern GENIE_PROCEDURE genie_sqrt_complex;
extern GENIE_PROCEDURE genie_tan_complex;
extern GENIE_PROCEDURE genie_tanh_complex;

#if defined ENABLE_NUMERICAL
extern GENIE_PROCEDURE genie_complex_scale_matrix_complex;
extern GENIE_PROCEDURE genie_complex_scale_vector_complex;
extern GENIE_PROCEDURE genie_laplace;
extern GENIE_PROCEDURE genie_matrix_add;
extern GENIE_PROCEDURE genie_matrix_ch;
extern GENIE_PROCEDURE genie_matrix_ch_solve;
extern GENIE_PROCEDURE genie_matrix_complex_add;
extern GENIE_PROCEDURE genie_matrix_complex_det;
extern GENIE_PROCEDURE genie_matrix_complex_div_complex;
extern GENIE_PROCEDURE genie_matrix_complex_div_complex_ab;
extern GENIE_PROCEDURE genie_matrix_complex_echo;
extern GENIE_PROCEDURE genie_matrix_complex_eq;
extern GENIE_PROCEDURE genie_matrix_complex_inv;
extern GENIE_PROCEDURE genie_matrix_complex_lu;
extern GENIE_PROCEDURE genie_matrix_complex_lu_det;
extern GENIE_PROCEDURE genie_matrix_complex_lu_inv;
extern GENIE_PROCEDURE genie_matrix_complex_lu_solve;
extern GENIE_PROCEDURE genie_matrix_complex_minus;
extern GENIE_PROCEDURE genie_matrix_complex_minusab;
extern GENIE_PROCEDURE genie_matrix_complex_ne;
extern GENIE_PROCEDURE genie_matrix_complex_plusab;
extern GENIE_PROCEDURE genie_matrix_complex_scale_complex;
extern GENIE_PROCEDURE genie_matrix_complex_scale_complex_ab;
extern GENIE_PROCEDURE genie_matrix_complex_sub;
extern GENIE_PROCEDURE genie_matrix_complex_times_matrix;
extern GENIE_PROCEDURE genie_matrix_complex_times_vector;
extern GENIE_PROCEDURE genie_matrix_complex_trace;
extern GENIE_PROCEDURE genie_matrix_complex_transpose;
extern GENIE_PROCEDURE genie_matrix_det;
extern GENIE_PROCEDURE genie_matrix_div_real;
extern GENIE_PROCEDURE genie_matrix_div_real_ab;
extern GENIE_PROCEDURE genie_matrix_echo;
extern GENIE_PROCEDURE genie_matrix_eq;
extern GENIE_PROCEDURE genie_matrix_inv;
extern GENIE_PROCEDURE genie_matrix_lu;
extern GENIE_PROCEDURE genie_matrix_lu_det;
extern GENIE_PROCEDURE genie_matrix_lu_inv;
extern GENIE_PROCEDURE genie_matrix_lu_solve;
extern GENIE_PROCEDURE genie_matrix_minus;
extern GENIE_PROCEDURE genie_matrix_minusab;
extern GENIE_PROCEDURE genie_matrix_ne;
extern GENIE_PROCEDURE genie_matrix_plusab;
extern GENIE_PROCEDURE genie_matrix_qr;
extern GENIE_PROCEDURE genie_matrix_qr_ls_solve;
extern GENIE_PROCEDURE genie_matrix_qr_solve;
extern GENIE_PROCEDURE genie_matrix_scale_real;
extern GENIE_PROCEDURE genie_matrix_scale_real_ab;
extern GENIE_PROCEDURE genie_matrix_sub;
extern GENIE_PROCEDURE genie_matrix_svd;
extern GENIE_PROCEDURE genie_matrix_svd_solve;
extern GENIE_PROCEDURE genie_matrix_times_matrix;
extern GENIE_PROCEDURE genie_matrix_times_vector;
extern GENIE_PROCEDURE genie_matrix_trace;
extern GENIE_PROCEDURE genie_matrix_transpose;
extern GENIE_PROCEDURE genie_real_scale_matrix;
extern GENIE_PROCEDURE genie_real_scale_vector;
extern GENIE_PROCEDURE genie_vector_add;
extern GENIE_PROCEDURE genie_vector_complex_add;
extern GENIE_PROCEDURE genie_vector_complex_div_complex;
extern GENIE_PROCEDURE genie_vector_complex_div_complex_ab;
extern GENIE_PROCEDURE genie_vector_complex_dot;
extern GENIE_PROCEDURE genie_vector_complex_dyad;
extern GENIE_PROCEDURE genie_vector_complex_echo;
extern GENIE_PROCEDURE genie_vector_complex_eq;
extern GENIE_PROCEDURE genie_vector_complex_minus;
extern GENIE_PROCEDURE genie_vector_complex_minusab;
extern GENIE_PROCEDURE genie_vector_complex_ne;
extern GENIE_PROCEDURE genie_vector_complex_norm;
extern GENIE_PROCEDURE genie_vector_complex_plusab;
extern GENIE_PROCEDURE genie_vector_complex_scale_complex;
extern GENIE_PROCEDURE genie_vector_complex_scale_complex_ab;
extern GENIE_PROCEDURE genie_vector_complex_sub;
extern GENIE_PROCEDURE genie_vector_complex_times_matrix;
extern GENIE_PROCEDURE genie_vector_div_real;
extern GENIE_PROCEDURE genie_vector_div_real_ab;
extern GENIE_PROCEDURE genie_vector_dot;
extern GENIE_PROCEDURE genie_vector_dyad;
extern GENIE_PROCEDURE genie_vector_echo;
extern GENIE_PROCEDURE genie_vector_eq;
extern GENIE_PROCEDURE genie_vector_minus;
extern GENIE_PROCEDURE genie_vector_minusab;
extern GENIE_PROCEDURE genie_vector_ne;
extern GENIE_PROCEDURE genie_vector_norm;
extern GENIE_PROCEDURE genie_vector_plusab;
extern GENIE_PROCEDURE genie_vector_scale_real;
extern GENIE_PROCEDURE genie_vector_scale_real_ab;
extern GENIE_PROCEDURE genie_vector_sub;
extern GENIE_PROCEDURE genie_vector_times_matrix;
#endif

#if defined ENABLE_NUMERICAL
extern GENIE_PROCEDURE genie_prime_factors;
extern GENIE_PROCEDURE genie_fft_complex_forward;
extern GENIE_PROCEDURE genie_fft_complex_backward;
extern GENIE_PROCEDURE genie_fft_complex_inverse;
extern GENIE_PROCEDURE genie_fft_forward;
extern GENIE_PROCEDURE genie_fft_backward;
extern GENIE_PROCEDURE genie_fft_inverse;
#endif

#if defined ENABLE_NUMERICAL
extern GENIE_PROCEDURE genie_cgs_speed_of_light;
extern GENIE_PROCEDURE genie_cgs_gravitational_constant;
extern GENIE_PROCEDURE genie_cgs_planck_constant_h;
extern GENIE_PROCEDURE genie_cgs_planck_constant_hbar;
extern GENIE_PROCEDURE genie_cgs_vacuum_permeability;
extern GENIE_PROCEDURE genie_cgs_astronomical_unit;
extern GENIE_PROCEDURE genie_cgs_light_year;
extern GENIE_PROCEDURE genie_cgs_parsec;
extern GENIE_PROCEDURE genie_cgs_grav_accel;
extern GENIE_PROCEDURE genie_cgs_electron_volt;
extern GENIE_PROCEDURE genie_cgs_mass_electron;
extern GENIE_PROCEDURE genie_cgs_mass_muon;
extern GENIE_PROCEDURE genie_cgs_mass_proton;
extern GENIE_PROCEDURE genie_cgs_mass_neutron;
extern GENIE_PROCEDURE genie_cgs_rydberg;
extern GENIE_PROCEDURE genie_cgs_boltzmann;
extern GENIE_PROCEDURE genie_cgs_bohr_magneton;
extern GENIE_PROCEDURE genie_cgs_nuclear_magneton;
extern GENIE_PROCEDURE genie_cgs_electron_magnetic_moment;
extern GENIE_PROCEDURE genie_cgs_proton_magnetic_moment;
extern GENIE_PROCEDURE genie_cgs_molar_gas;
extern GENIE_PROCEDURE genie_cgs_standard_gas_volume;
extern GENIE_PROCEDURE genie_cgs_minute;
extern GENIE_PROCEDURE genie_cgs_hour;
extern GENIE_PROCEDURE genie_cgs_day;
extern GENIE_PROCEDURE genie_cgs_week;
extern GENIE_PROCEDURE genie_cgs_inch;
extern GENIE_PROCEDURE genie_cgs_foot;
extern GENIE_PROCEDURE genie_cgs_yard;
extern GENIE_PROCEDURE genie_cgs_mile;
extern GENIE_PROCEDURE genie_cgs_nautical_mile;
extern GENIE_PROCEDURE genie_cgs_fathom;
extern GENIE_PROCEDURE genie_cgs_mil;
extern GENIE_PROCEDURE genie_cgs_point;
extern GENIE_PROCEDURE genie_cgs_texpoint;
extern GENIE_PROCEDURE genie_cgs_micron;
extern GENIE_PROCEDURE genie_cgs_angstrom;
extern GENIE_PROCEDURE genie_cgs_hectare;
extern GENIE_PROCEDURE genie_cgs_acre;
extern GENIE_PROCEDURE genie_cgs_barn;
extern GENIE_PROCEDURE genie_cgs_liter;
extern GENIE_PROCEDURE genie_cgs_us_gallon;
extern GENIE_PROCEDURE genie_cgs_quart;
extern GENIE_PROCEDURE genie_cgs_pint;
extern GENIE_PROCEDURE genie_cgs_cup;
extern GENIE_PROCEDURE genie_cgs_fluid_ounce;
extern GENIE_PROCEDURE genie_cgs_tablespoon;
extern GENIE_PROCEDURE genie_cgs_teaspoon;
extern GENIE_PROCEDURE genie_cgs_canadian_gallon;
extern GENIE_PROCEDURE genie_cgs_uk_gallon;
extern GENIE_PROCEDURE genie_cgs_miles_per_hour;
extern GENIE_PROCEDURE genie_cgs_kilometers_per_hour;
extern GENIE_PROCEDURE genie_cgs_knot;
extern GENIE_PROCEDURE genie_cgs_pound_mass;
extern GENIE_PROCEDURE genie_cgs_ounce_mass;
extern GENIE_PROCEDURE genie_cgs_ton;
extern GENIE_PROCEDURE genie_cgs_metric_ton;
extern GENIE_PROCEDURE genie_cgs_uk_ton;
extern GENIE_PROCEDURE genie_cgs_troy_ounce;
extern GENIE_PROCEDURE genie_cgs_carat;
extern GENIE_PROCEDURE genie_cgs_unified_atomic_mass;
extern GENIE_PROCEDURE genie_cgs_gram_force;
extern GENIE_PROCEDURE genie_cgs_pound_force;
extern GENIE_PROCEDURE genie_cgs_kilopound_force;
extern GENIE_PROCEDURE genie_cgs_poundal;
extern GENIE_PROCEDURE genie_cgs_calorie;
extern GENIE_PROCEDURE genie_cgs_btu;
extern GENIE_PROCEDURE genie_cgs_therm;
extern GENIE_PROCEDURE genie_cgs_horsepower;
extern GENIE_PROCEDURE genie_cgs_bar;
extern GENIE_PROCEDURE genie_cgs_std_atmosphere;
extern GENIE_PROCEDURE genie_cgs_torr;
extern GENIE_PROCEDURE genie_cgs_meter_of_mercury;
extern GENIE_PROCEDURE genie_cgs_inch_of_mercury;
extern GENIE_PROCEDURE genie_cgs_inch_of_water;
extern GENIE_PROCEDURE genie_cgs_psi;
extern GENIE_PROCEDURE genie_cgs_poise;
extern GENIE_PROCEDURE genie_cgs_stokes;
extern GENIE_PROCEDURE genie_cgs_faraday;
extern GENIE_PROCEDURE genie_cgs_electron_charge;
extern GENIE_PROCEDURE genie_cgs_gauss;
extern GENIE_PROCEDURE genie_cgs_stilb;
extern GENIE_PROCEDURE genie_cgs_lumen;
extern GENIE_PROCEDURE genie_cgs_lux;
extern GENIE_PROCEDURE genie_cgs_phot;
extern GENIE_PROCEDURE genie_cgs_footcandle;
extern GENIE_PROCEDURE genie_cgs_lambert;
extern GENIE_PROCEDURE genie_cgs_footlambert;
extern GENIE_PROCEDURE genie_cgs_curie;
extern GENIE_PROCEDURE genie_cgs_roentgen;
extern GENIE_PROCEDURE genie_cgs_rad;
extern GENIE_PROCEDURE genie_cgs_solar_mass;
extern GENIE_PROCEDURE genie_cgs_bohr_radius;
extern GENIE_PROCEDURE genie_cgs_vacuum_permittivity;
extern GENIE_PROCEDURE genie_cgs_newton;
extern GENIE_PROCEDURE genie_cgs_dyne;
extern GENIE_PROCEDURE genie_cgs_joule;
extern GENIE_PROCEDURE genie_cgs_erg;
extern GENIE_PROCEDURE genie_mks_speed_of_light;
extern GENIE_PROCEDURE genie_mks_gravitational_constant;
extern GENIE_PROCEDURE genie_mks_planck_constant_h;
extern GENIE_PROCEDURE genie_mks_planck_constant_hbar;
extern GENIE_PROCEDURE genie_mks_vacuum_permeability;
extern GENIE_PROCEDURE genie_mks_astronomical_unit;
extern GENIE_PROCEDURE genie_mks_light_year;
extern GENIE_PROCEDURE genie_mks_parsec;
extern GENIE_PROCEDURE genie_mks_grav_accel;
extern GENIE_PROCEDURE genie_mks_electron_volt;
extern GENIE_PROCEDURE genie_mks_mass_electron;
extern GENIE_PROCEDURE genie_mks_mass_muon;
extern GENIE_PROCEDURE genie_mks_mass_proton;
extern GENIE_PROCEDURE genie_mks_mass_neutron;
extern GENIE_PROCEDURE genie_mks_rydberg;
extern GENIE_PROCEDURE genie_mks_boltzmann;
extern GENIE_PROCEDURE genie_mks_bohr_magneton;
extern GENIE_PROCEDURE genie_mks_nuclear_magneton;
extern GENIE_PROCEDURE genie_mks_electron_magnetic_moment;
extern GENIE_PROCEDURE genie_mks_proton_magnetic_moment;
extern GENIE_PROCEDURE genie_mks_molar_gas;
extern GENIE_PROCEDURE genie_mks_standard_gas_volume;
extern GENIE_PROCEDURE genie_mks_minute;
extern GENIE_PROCEDURE genie_mks_hour;
extern GENIE_PROCEDURE genie_mks_day;
extern GENIE_PROCEDURE genie_mks_week;
extern GENIE_PROCEDURE genie_mks_inch;
extern GENIE_PROCEDURE genie_mks_foot;
extern GENIE_PROCEDURE genie_mks_yard;
extern GENIE_PROCEDURE genie_mks_mile;
extern GENIE_PROCEDURE genie_mks_nautical_mile;
extern GENIE_PROCEDURE genie_mks_fathom;
extern GENIE_PROCEDURE genie_mks_mil;
extern GENIE_PROCEDURE genie_mks_point;
extern GENIE_PROCEDURE genie_mks_texpoint;
extern GENIE_PROCEDURE genie_mks_micron;
extern GENIE_PROCEDURE genie_mks_angstrom;
extern GENIE_PROCEDURE genie_mks_hectare;
extern GENIE_PROCEDURE genie_mks_acre;
extern GENIE_PROCEDURE genie_mks_barn;
extern GENIE_PROCEDURE genie_mks_liter;
extern GENIE_PROCEDURE genie_mks_us_gallon;
extern GENIE_PROCEDURE genie_mks_quart;
extern GENIE_PROCEDURE genie_mks_pint;
extern GENIE_PROCEDURE genie_mks_cup;
extern GENIE_PROCEDURE genie_mks_fluid_ounce;
extern GENIE_PROCEDURE genie_mks_tablespoon;
extern GENIE_PROCEDURE genie_mks_teaspoon;
extern GENIE_PROCEDURE genie_mks_canadian_gallon;
extern GENIE_PROCEDURE genie_mks_uk_gallon;
extern GENIE_PROCEDURE genie_mks_miles_per_hour;
extern GENIE_PROCEDURE genie_mks_kilometers_per_hour;
extern GENIE_PROCEDURE genie_mks_knot;
extern GENIE_PROCEDURE genie_mks_pound_mass;
extern GENIE_PROCEDURE genie_mks_ounce_mass;
extern GENIE_PROCEDURE genie_mks_ton;
extern GENIE_PROCEDURE genie_mks_metric_ton;
extern GENIE_PROCEDURE genie_mks_uk_ton;
extern GENIE_PROCEDURE genie_mks_troy_ounce;
extern GENIE_PROCEDURE genie_mks_carat;
extern GENIE_PROCEDURE genie_mks_unified_atomic_mass;
extern GENIE_PROCEDURE genie_mks_gram_force;
extern GENIE_PROCEDURE genie_mks_pound_force;
extern GENIE_PROCEDURE genie_mks_kilopound_force;
extern GENIE_PROCEDURE genie_mks_poundal;
extern GENIE_PROCEDURE genie_mks_calorie;
extern GENIE_PROCEDURE genie_mks_btu;
extern GENIE_PROCEDURE genie_mks_therm;
extern GENIE_PROCEDURE genie_mks_horsepower;
extern GENIE_PROCEDURE genie_mks_bar;
extern GENIE_PROCEDURE genie_mks_std_atmosphere;
extern GENIE_PROCEDURE genie_mks_torr;
extern GENIE_PROCEDURE genie_mks_meter_of_mercury;
extern GENIE_PROCEDURE genie_mks_inch_of_mercury;
extern GENIE_PROCEDURE genie_mks_inch_of_water;
extern GENIE_PROCEDURE genie_mks_psi;
extern GENIE_PROCEDURE genie_mks_poise;
extern GENIE_PROCEDURE genie_mks_stokes;
extern GENIE_PROCEDURE genie_mks_faraday;
extern GENIE_PROCEDURE genie_mks_electron_charge;
extern GENIE_PROCEDURE genie_mks_gauss;
extern GENIE_PROCEDURE genie_mks_stilb;
extern GENIE_PROCEDURE genie_mks_lumen;
extern GENIE_PROCEDURE genie_mks_lux;
extern GENIE_PROCEDURE genie_mks_phot;
extern GENIE_PROCEDURE genie_mks_footcandle;
extern GENIE_PROCEDURE genie_mks_lambert;
extern GENIE_PROCEDURE genie_mks_footlambert;
extern GENIE_PROCEDURE genie_mks_curie;
extern GENIE_PROCEDURE genie_mks_roentgen;
extern GENIE_PROCEDURE genie_mks_rad;
extern GENIE_PROCEDURE genie_mks_solar_mass;
extern GENIE_PROCEDURE genie_mks_bohr_radius;
extern GENIE_PROCEDURE genie_mks_vacuum_permittivity;
extern GENIE_PROCEDURE genie_mks_newton;
extern GENIE_PROCEDURE genie_mks_dyne;
extern GENIE_PROCEDURE genie_mks_joule;
extern GENIE_PROCEDURE genie_mks_erg;
extern GENIE_PROCEDURE genie_num_fine_structure;
extern GENIE_PROCEDURE genie_num_avogadro;
extern GENIE_PROCEDURE genie_num_yotta;
extern GENIE_PROCEDURE genie_num_zetta;
extern GENIE_PROCEDURE genie_num_exa;
extern GENIE_PROCEDURE genie_num_peta;
extern GENIE_PROCEDURE genie_num_tera;
extern GENIE_PROCEDURE genie_num_giga;
extern GENIE_PROCEDURE genie_num_mega;
extern GENIE_PROCEDURE genie_num_kilo;
extern GENIE_PROCEDURE genie_num_milli;
extern GENIE_PROCEDURE genie_num_micro;
extern GENIE_PROCEDURE genie_num_nano;
extern GENIE_PROCEDURE genie_num_pico;
extern GENIE_PROCEDURE genie_num_femto;
extern GENIE_PROCEDURE genie_num_atto;
extern GENIE_PROCEDURE genie_num_zepto;
extern GENIE_PROCEDURE genie_num_yocto;
extern GENIE_PROCEDURE genie_airy_ai_deriv_real;
extern GENIE_PROCEDURE genie_airy_ai_real;
extern GENIE_PROCEDURE genie_airy_bi_deriv_real;
extern GENIE_PROCEDURE genie_airy_bi_real;
extern GENIE_PROCEDURE genie_bessel_exp_il_real;
extern GENIE_PROCEDURE genie_bessel_exp_in_real;
extern GENIE_PROCEDURE genie_bessel_exp_inu_real;
extern GENIE_PROCEDURE genie_bessel_exp_kl_real;
extern GENIE_PROCEDURE genie_bessel_exp_kn_real;
extern GENIE_PROCEDURE genie_bessel_exp_knu_real;
extern GENIE_PROCEDURE genie_bessel_in_real;
extern GENIE_PROCEDURE genie_bessel_inu_real;
extern GENIE_PROCEDURE genie_bessel_jl_real;
extern GENIE_PROCEDURE genie_bessel_jn_real;
extern GENIE_PROCEDURE genie_bessel_jnu_real;
extern GENIE_PROCEDURE genie_bessel_kn_real;
extern GENIE_PROCEDURE genie_bessel_knu_real;
extern GENIE_PROCEDURE genie_bessel_yl_real;
extern GENIE_PROCEDURE genie_bessel_yn_real;
extern GENIE_PROCEDURE genie_bessel_ynu_real;
extern GENIE_PROCEDURE genie_beta_inc_real;
extern GENIE_PROCEDURE genie_beta_real;
extern GENIE_PROCEDURE genie_elliptic_integral_e_real;
extern GENIE_PROCEDURE genie_elliptic_integral_k_real;
extern GENIE_PROCEDURE genie_elliptic_integral_rc_real;
extern GENIE_PROCEDURE genie_elliptic_integral_rd_real;
extern GENIE_PROCEDURE genie_elliptic_integral_rf_real;
extern GENIE_PROCEDURE genie_elliptic_integral_rj_real;
extern GENIE_PROCEDURE genie_factorial_real;
extern GENIE_PROCEDURE genie_gamma_inc_real;
extern GENIE_PROCEDURE genie_gamma_real;
extern GENIE_PROCEDURE genie_lngamma_real;
#endif

extern GENIE_PROCEDURE genie_erf_real;
extern GENIE_PROCEDURE genie_erfc_real;
extern GENIE_PROCEDURE genie_cosh_real;
extern GENIE_PROCEDURE genie_sinh_real;
extern GENIE_PROCEDURE genie_tanh_real;

extern void calc_rte (NODE_T *, BOOL_T, MOID_T *, const char *);
extern double curt (double);

#define GSL_CONST_NUM_FINE_STRUCTURE (7.297352533e-3) /* 1 */
#define GSL_CONST_NUM_AVOGADRO (6.02214199e23) /* 1 / mol */
#define GSL_CONST_NUM_YOTTA (1e24) /* 1 */
#define GSL_CONST_NUM_ZETTA (1e21) /* 1 */
#define GSL_CONST_NUM_EXA (1e18) /* 1 */
#define GSL_CONST_NUM_PETA (1e15) /* 1 */
#define GSL_CONST_NUM_TERA (1e12) /* 1 */
#define GSL_CONST_NUM_GIGA (1e9) /* 1 */
#define GSL_CONST_NUM_MEGA (1e6) /* 1 */
#define GSL_CONST_NUM_KILO (1e3) /* 1 */
#define GSL_CONST_NUM_MILLI (1e-3) /* 1 */
#define GSL_CONST_NUM_MICRO (1e-6) /* 1 */
#define GSL_CONST_NUM_NANO (1e-9) /* 1 */
#define GSL_CONST_NUM_PICO (1e-12) /* 1 */
#define GSL_CONST_NUM_FEMTO (1e-15) /* 1 */
#define GSL_CONST_NUM_ATTO (1e-18) /* 1 */
#define GSL_CONST_NUM_ZEPTO (1e-21) /* 1 */
#define GSL_CONST_NUM_YOCTO (1e-24) /* 1 */

#define GSL_CONST_CGSM_GAUSS (1.0) /* cm / A s^2  */
#define GSL_CONST_CGSM_SPEED_OF_LIGHT (2.99792458e10) /* cm / s */
#define GSL_CONST_CGSM_GRAVITATIONAL_CONSTANT (6.673e-8) /* cm^3 / g s^2 */
#define GSL_CONST_CGSM_PLANCKS_CONSTANT_H (6.62606896e-27) /* g cm^2 / s */
#define GSL_CONST_CGSM_PLANCKS_CONSTANT_HBAR (1.05457162825e-27) /* g cm^2 / s */
#define GSL_CONST_CGSM_ASTRONOMICAL_UNIT (1.49597870691e13) /* cm */
#define GSL_CONST_CGSM_LIGHT_YEAR (9.46053620707e17) /* cm */
#define GSL_CONST_CGSM_PARSEC (3.08567758135e18) /* cm */
#define GSL_CONST_CGSM_GRAV_ACCEL (9.80665e2) /* cm / s^2 */
#define GSL_CONST_CGSM_ELECTRON_VOLT (1.602176487e-12) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_MASS_ELECTRON (9.10938188e-28) /* g */
#define GSL_CONST_CGSM_MASS_MUON (1.88353109e-25) /* g */
#define GSL_CONST_CGSM_MASS_PROTON (1.67262158e-24) /* g */
#define GSL_CONST_CGSM_MASS_NEUTRON (1.67492716e-24) /* g */
#define GSL_CONST_CGSM_RYDBERG (2.17987196968e-11) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_BOLTZMANN (1.3806504e-16) /* g cm^2 / K s^2 */
#define GSL_CONST_CGSM_MOLAR_GAS (8.314472e7) /* g cm^2 / K mol s^2 */
#define GSL_CONST_CGSM_STANDARD_GAS_VOLUME (2.2710981e4) /* cm^3 / mol */
#define GSL_CONST_CGSM_MINUTE (6e1) /* s */
#define GSL_CONST_CGSM_HOUR (3.6e3) /* s */
#define GSL_CONST_CGSM_DAY (8.64e4) /* s */
#define GSL_CONST_CGSM_WEEK (6.048e5) /* s */
#define GSL_CONST_CGSM_INCH (2.54e0) /* cm */
#define GSL_CONST_CGSM_FOOT (3.048e1) /* cm */
#define GSL_CONST_CGSM_YARD (9.144e1) /* cm */
#define GSL_CONST_CGSM_MILE (1.609344e5) /* cm */
#define GSL_CONST_CGSM_NAUTICAL_MILE (1.852e5) /* cm */
#define GSL_CONST_CGSM_FATHOM (1.8288e2) /* cm */
#define GSL_CONST_CGSM_MIL (2.54e-3) /* cm */
#define GSL_CONST_CGSM_POINT (3.52777777778e-2) /* cm */
#define GSL_CONST_CGSM_TEXPOINT (3.51459803515e-2) /* cm */
#define GSL_CONST_CGSM_MICRON (1e-4) /* cm */
#define GSL_CONST_CGSM_ANGSTROM (1e-8) /* cm */
#define GSL_CONST_CGSM_HECTARE (1e8) /* cm^2 */
#define GSL_CONST_CGSM_ACRE (4.04685642241e7) /* cm^2 */
#define GSL_CONST_CGSM_BARN (1e-24) /* cm^2 */
#define GSL_CONST_CGSM_LITER (1e3) /* cm^3 */
#define GSL_CONST_CGSM_US_GALLON (3.78541178402e3) /* cm^3 */
#define GSL_CONST_CGSM_QUART (9.46352946004e2) /* cm^3 */
#define GSL_CONST_CGSM_PINT (4.73176473002e2) /* cm^3 */
#define GSL_CONST_CGSM_CUP (2.36588236501e2) /* cm^3 */
#define GSL_CONST_CGSM_FLUID_OUNCE (2.95735295626e1) /* cm^3 */
#define GSL_CONST_CGSM_TABLESPOON (1.47867647813e1) /* cm^3 */
#define GSL_CONST_CGSM_TEASPOON (4.92892159375e0) /* cm^3 */
#define GSL_CONST_CGSM_CANADIAN_GALLON (4.54609e3) /* cm^3 */
#define GSL_CONST_CGSM_UK_GALLON (4.546092e3) /* cm^3 */
#define GSL_CONST_CGSM_MILES_PER_HOUR (4.4704e1) /* cm / s */
#define GSL_CONST_CGSM_KILOMETERS_PER_HOUR (2.77777777778e1) /* cm / s */
#define GSL_CONST_CGSM_KNOT (5.14444444444e1) /* cm / s */
#define GSL_CONST_CGSM_POUND_MASS (4.5359237e2) /* g */
#define GSL_CONST_CGSM_OUNCE_MASS (2.8349523125e1) /* g */
#define GSL_CONST_CGSM_TON (9.0718474e5) /* g */
#define GSL_CONST_CGSM_METRIC_TON (1e6) /* g */
#define GSL_CONST_CGSM_UK_TON (1.0160469088e6) /* g */
#define GSL_CONST_CGSM_TROY_OUNCE (3.1103475e1) /* g */
#define GSL_CONST_CGSM_CARAT (2e-1) /* g */
#define GSL_CONST_CGSM_UNIFIED_ATOMIC_MASS (1.660538782e-24) /* g */
#define GSL_CONST_CGSM_GRAM_FORCE (9.80665e2) /* cm g / s^2 */
#define GSL_CONST_CGSM_POUND_FORCE (4.44822161526e5) /* cm g / s^2 */
#define GSL_CONST_CGSM_KILOPOUND_FORCE (4.44822161526e8) /* cm g / s^2 */
#define GSL_CONST_CGSM_POUNDAL (1.38255e4) /* cm g / s^2 */
#define GSL_CONST_CGSM_CALORIE (4.1868e7) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_BTU (1.05505585262e10) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_THERM (1.05506e15) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_HORSEPOWER (7.457e9) /* g cm^2 / s^3 */
#define GSL_CONST_CGSM_BAR (1e6) /* g / cm s^2 */
#define GSL_CONST_CGSM_STD_ATMOSPHERE (1.01325e6) /* g / cm s^2 */
#define GSL_CONST_CGSM_TORR (1.33322368421e3) /* g / cm s^2 */
#define GSL_CONST_CGSM_METER_OF_MERCURY (1.33322368421e6) /* g / cm s^2 */
#define GSL_CONST_CGSM_INCH_OF_MERCURY (3.38638815789e4) /* g / cm s^2 */
#define GSL_CONST_CGSM_INCH_OF_WATER (2.490889e3) /* g / cm s^2 */
#define GSL_CONST_CGSM_PSI (6.89475729317e4) /* g / cm s^2 */
#define GSL_CONST_CGSM_POISE (1e0) /* g / cm s */
#define GSL_CONST_CGSM_STOKES (1e0) /* cm^2 / s */
#define GSL_CONST_CGSM_STILB (1e0) /* cd / cm^2 */
#define GSL_CONST_CGSM_LUMEN (1e0) /* cd sr */
#define GSL_CONST_CGSM_LUX (1e-4) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_PHOT (1e0) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_FOOTCANDLE (1.076e-3) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_LAMBERT (1e0) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_FOOTLAMBERT (1.07639104e-3) /* cd sr / cm^2 */
#define GSL_CONST_CGSM_CURIE (3.7e10) /* 1 / s */
#define GSL_CONST_CGSM_ROENTGEN (2.58e-8) /* abamp s / g */
#define GSL_CONST_CGSM_RAD (1e2) /* cm^2 / s^2 */
#define GSL_CONST_CGSM_SOLAR_MASS (1.98892e33) /* g */
#define GSL_CONST_CGSM_BOHR_RADIUS (5.291772083e-9) /* cm */
#define GSL_CONST_CGSM_NEWTON (1e5) /* cm g / s^2 */
#define GSL_CONST_CGSM_DYNE (1e0) /* cm g / s^2 */
#define GSL_CONST_CGSM_JOULE (1e7) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_ERG (1e0) /* g cm^2 / s^2 */
#define GSL_CONST_CGSM_STEFAN_BOLTZMANN_CONSTANT (5.67040047374e-5) /* g / K^4 s^3 */
#define GSL_CONST_CGSM_THOMSON_CROSS_SECTION (6.65245893699e-25) /* cm^2 */
#define GSL_CONST_CGSM_BOHR_MAGNETON (9.27400899e-21) /* abamp cm^2 */
#define GSL_CONST_CGSM_NUCLEAR_MAGNETON (5.05078317e-24) /* abamp cm^2 */
#define GSL_CONST_CGSM_ELECTRON_MAGNETIC_MOMENT (9.28476362e-21) /* abamp cm^2 */
#define GSL_CONST_CGSM_PROTON_MAGNETIC_MOMENT (1.410606633e-23) /* abamp cm^2 */
#define GSL_CONST_CGSM_FARADAY (9.64853429775e3) /* abamp s / mol */
#define GSL_CONST_CGSM_ELECTRON_CHARGE (1.602176487e-20) /* abamp s */

#define GSL_CONST_MKS_SPEED_OF_LIGHT (2.99792458e8) /* m / s */
#define GSL_CONST_MKS_GRAVITATIONAL_CONSTANT (6.673e-11) /* m^3 / kg s^2 */
#define GSL_CONST_MKS_PLANCKS_CONSTANT_H (6.62606896e-34) /* kg m^2 / s */
#define GSL_CONST_MKS_PLANCKS_CONSTANT_HBAR (1.05457162825e-34) /* kg m^2 / s */
#define GSL_CONST_MKS_ASTRONOMICAL_UNIT (1.49597870691e11) /* m */
#define GSL_CONST_MKS_LIGHT_YEAR (9.46053620707e15) /* m */
#define GSL_CONST_MKS_PARSEC (3.08567758135e16) /* m */
#define GSL_CONST_MKS_GRAV_ACCEL (9.80665e0) /* m / s^2 */
#define GSL_CONST_MKS_ELECTRON_VOLT (1.602176487e-19) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_MASS_ELECTRON (9.10938188e-31) /* kg */
#define GSL_CONST_MKS_MASS_MUON (1.88353109e-28) /* kg */
#define GSL_CONST_MKS_MASS_PROTON (1.67262158e-27) /* kg */
#define GSL_CONST_MKS_MASS_NEUTRON (1.67492716e-27) /* kg */
#define GSL_CONST_MKS_RYDBERG (2.17987196968e-18) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_BOLTZMANN (1.3806504e-23) /* kg m^2 / K s^2 */
#define GSL_CONST_MKS_MOLAR_GAS (8.314472e0) /* kg m^2 / K mol s^2 */
#define GSL_CONST_MKS_STANDARD_GAS_VOLUME (2.2710981e-2) /* m^3 / mol */
#define GSL_CONST_MKS_MINUTE (6e1) /* s */
#define GSL_CONST_MKS_HOUR (3.6e3) /* s */
#define GSL_CONST_MKS_DAY (8.64e4) /* s */
#define GSL_CONST_MKS_WEEK (6.048e5) /* s */
#define GSL_CONST_MKS_INCH (2.54e-2) /* m */
#define GSL_CONST_MKS_FOOT (3.048e-1) /* m */
#define GSL_CONST_MKS_YARD (9.144e-1) /* m */
#define GSL_CONST_MKS_MILE (1.609344e3) /* m */
#define GSL_CONST_MKS_NAUTICAL_MILE (1.852e3) /* m */
#define GSL_CONST_MKS_FATHOM (1.8288e0) /* m */
#define GSL_CONST_MKS_MIL (2.54e-5) /* m */
#define GSL_CONST_MKS_POINT (3.52777777778e-4) /* m */
#define GSL_CONST_MKS_TEXPOINT (3.51459803515e-4) /* m */
#define GSL_CONST_MKS_MICRON (1e-6) /* m */
#define GSL_CONST_MKS_ANGSTROM (1e-10) /* m */
#define GSL_CONST_MKS_HECTARE (1e4) /* m^2 */
#define GSL_CONST_MKS_ACRE (4.04685642241e3) /* m^2 */
#define GSL_CONST_MKS_BARN (1e-28) /* m^2 */
#define GSL_CONST_MKS_LITER (1e-3) /* m^3 */
#define GSL_CONST_MKS_US_GALLON (3.78541178402e-3) /* m^3 */
#define GSL_CONST_MKS_QUART (9.46352946004e-4) /* m^3 */
#define GSL_CONST_MKS_PINT (4.73176473002e-4) /* m^3 */
#define GSL_CONST_MKS_CUP (2.36588236501e-4) /* m^3 */
#define GSL_CONST_MKS_FLUID_OUNCE (2.95735295626e-5) /* m^3 */
#define GSL_CONST_MKS_TABLESPOON (1.47867647813e-5) /* m^3 */
#define GSL_CONST_MKS_TEASPOON (4.92892159375e-6) /* m^3 */
#define GSL_CONST_MKS_CANADIAN_GALLON (4.54609e-3) /* m^3 */
#define GSL_CONST_MKS_UK_GALLON (4.546092e-3) /* m^3 */
#define GSL_CONST_MKS_MILES_PER_HOUR (4.4704e-1) /* m / s */
#define GSL_CONST_MKS_KILOMETERS_PER_HOUR (2.77777777778e-1) /* m / s */
#define GSL_CONST_MKS_KNOT (5.14444444444e-1) /* m / s */
#define GSL_CONST_MKS_POUND_MASS (4.5359237e-1) /* kg */
#define GSL_CONST_MKS_OUNCE_MASS (2.8349523125e-2) /* kg */
#define GSL_CONST_MKS_TON (9.0718474e2) /* kg */
#define GSL_CONST_MKS_METRIC_TON (1e3) /* kg */
#define GSL_CONST_MKS_UK_TON (1.0160469088e3) /* kg */
#define GSL_CONST_MKS_TROY_OUNCE (3.1103475e-2) /* kg */
#define GSL_CONST_MKS_CARAT (2e-4) /* kg */
#define GSL_CONST_MKS_UNIFIED_ATOMIC_MASS (1.660538782e-27) /* kg */
#define GSL_CONST_MKS_GRAM_FORCE (9.80665e-3) /* kg m / s^2 */
#define GSL_CONST_MKS_POUND_FORCE (4.44822161526e0) /* kg m / s^2 */
#define GSL_CONST_MKS_KILOPOUND_FORCE (4.44822161526e3) /* kg m / s^2 */
#define GSL_CONST_MKS_POUNDAL (1.38255e-1) /* kg m / s^2 */
#define GSL_CONST_MKS_CALORIE (4.1868e0) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_BTU (1.05505585262e3) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_THERM (1.05506e8) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_HORSEPOWER (7.457e2) /* kg m^2 / s^3 */
#define GSL_CONST_MKS_BAR (1e5) /* kg / m s^2 */
#define GSL_CONST_MKS_STD_ATMOSPHERE (1.01325e5) /* kg / m s^2 */
#define GSL_CONST_MKS_TORR (1.33322368421e2) /* kg / m s^2 */
#define GSL_CONST_MKS_METER_OF_MERCURY (1.33322368421e5) /* kg / m s^2 */
#define GSL_CONST_MKS_INCH_OF_MERCURY (3.38638815789e3) /* kg / m s^2 */
#define GSL_CONST_MKS_INCH_OF_WATER (2.490889e2) /* kg / m s^2 */
#define GSL_CONST_MKS_PSI (6.89475729317e3) /* kg / m s^2 */
#define GSL_CONST_MKS_POISE (1e-1) /* kg m^-1 s^-1 */
#define GSL_CONST_MKS_STOKES (1e-4) /* m^2 / s */
#define GSL_CONST_MKS_STILB (1e4) /* cd / m^2 */
#define GSL_CONST_MKS_LUMEN (1e0) /* cd sr */
#define GSL_CONST_MKS_LUX (1e0) /* cd sr / m^2 */
#define GSL_CONST_MKS_PHOT (1e4) /* cd sr / m^2 */
#define GSL_CONST_MKS_FOOTCANDLE (1.076e1) /* cd sr / m^2 */
#define GSL_CONST_MKS_LAMBERT (1e4) /* cd sr / m^2 */
#define GSL_CONST_MKS_FOOTLAMBERT (1.07639104e1) /* cd sr / m^2 */
#define GSL_CONST_MKS_CURIE (3.7e10) /* 1 / s */
#define GSL_CONST_MKS_ROENTGEN (2.58e-4) /* A s / kg */
#define GSL_CONST_MKS_RAD (1e-2) /* m^2 / s^2 */
#define GSL_CONST_MKS_SOLAR_MASS (1.98892e30) /* kg */
#define GSL_CONST_MKS_BOHR_RADIUS (5.291772083e-11) /* m */
#define GSL_CONST_MKS_NEWTON (1e0) /* kg m / s^2 */
#define GSL_CONST_MKS_DYNE (1e-5) /* kg m / s^2 */
#define GSL_CONST_MKS_JOULE (1e0) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_ERG (1e-7) /* kg m^2 / s^2 */
#define GSL_CONST_MKS_STEFAN_BOLTZMANN_CONSTANT (5.67040047374e-8) /* kg / K^4 s^3 */
#define GSL_CONST_MKS_THOMSON_CROSS_SECTION (6.65245893699e-29) /* m^2 */
#define GSL_CONST_MKS_BOHR_MAGNETON (9.27400899e-24) /* A m^2 */
#define GSL_CONST_MKS_NUCLEAR_MAGNETON (5.05078317e-27) /* A m^2 */
#define GSL_CONST_MKS_ELECTRON_MAGNETIC_MOMENT (9.28476362e-24) /* A m^2 */
#define GSL_CONST_MKS_PROTON_MAGNETIC_MOMENT (1.410606633e-26) /* A m^2 */
#define GSL_CONST_MKS_FARADAY (9.64853429775e4) /* A s / mol */
#define GSL_CONST_MKS_ELECTRON_CHARGE (1.602176487e-19) /* A s */
#define GSL_CONST_MKS_VACUUM_PERMITTIVITY (8.854187817e-12) /* A^2 s^4 / kg m^3 */
#define GSL_CONST_MKS_VACUUM_PERMEABILITY (1.25663706144e-6) /* kg m / A^2 s^2 */
#define GSL_CONST_MKS_DEBYE (3.33564095198e-30) /* A s^2 / m^2 */
#define GSL_CONST_MKS_GAUSS (1e-4) /* kg / A s^2 */

#endif
