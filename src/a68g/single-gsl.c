//! @file single-gsl.c
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
#include "a68g-prelude-gsl.h"
#include "a68g-double.h"
#include "a68g-numbers.h"

#if defined (HAVE_GSL)

#define PROC_RR_R(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_REAL *x;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (& (VALUE (x)));\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
}

#define PROC_R_R(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_REAL *x;\
  gsl_sf_result y;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), &y);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&y);\
}

#define PROC_R_R_DBL(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_REAL *x;\
  gsl_sf_result y;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), GSL_PREC_DOUBLE, &y);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&y);\
}

#define PROC_I_R(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_INT s;\
  gsl_sf_result y;\
  int status;\
  POP_OBJECT (p, &s, A68_INT);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (&s), &y);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  PUSH_VALUE (p, VAL (&y), A68_REAL);\
}

#define PROC_R_R_R(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y),  &r);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r);\
}

#define PROC_I_R_R(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_INT s;\
  A68_REAL x;\
  gsl_sf_result r;\
  int status;\
  POP_OBJECT (p, &x, A68_REAL);\
  POP_OBJECT (p, &s, A68_INT);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (&s), VALUE (&x), &r);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  PUSH_VALUE (p, VAL (&r), A68_REAL);\
}

#define PROC_I_R_R_REVERSED(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_INT s;\
  A68_REAL x;\
  gsl_sf_result r;\
  int status;\
  POP_OBJECT (p, &x, A68_REAL);\
  POP_OBJECT (p, &s, A68_INT);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (&x), VALUE (&s), &r);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  PUSH_VALUE (p, VAL (&r), A68_REAL);\
}

#define PROC_R_R_R_DBL(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), GSL_PREC_DOUBLE, &r);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r);\
}

#define PROC_R_R_R_R(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_REAL *x, *y, *z;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z),  &r);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r);\
}

#define PROC_I_R_R_R(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_INT s;\
  A68_REAL x, y;\
  gsl_sf_result r;\
  int status;\
  POP_OBJECT (p, &y, A68_REAL);\
  POP_OBJECT (p, &x, A68_REAL);\
  POP_OBJECT (p, &s, A68_INT);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (&s), VALUE (&x), VALUE (&y), &r);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  PUSH_VALUE (p, VAL (&r), A68_REAL);\
}

#define PROC_R_R_R_R_DBL(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_REAL *x, *y, *z;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z), GSL_PREC_DOUBLE, &r);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r);\
}

#define PROC_R_R_R_R_R_DBL(p, g, f)\
void g (NODE_T *p) {\
  A68 (f_entry) = p;\
  A68_REAL *x, *y, *z, *rho;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, rho, A68_REAL);\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z), VALUE (rho), GSL_PREC_DOUBLE, &r);\
  MATH_RTE (p, status != 0, M_REAL, (char *) gsl_strerror (status));\
  VALUE (x) = VAL (&r);\
}

//! @brief PROC airy ai = (REAL x) REAL

PROC_R_R_DBL (p, genie_airy_ai_real, gsl_sf_airy_Ai_e);

//! @brief PROC airy bi = (REAL x) REAL

PROC_R_R_DBL (p, genie_airy_bi_real, gsl_sf_airy_Bi_e);

//! @brief PROC airy ai scaled = (REAL x) REAL

PROC_R_R_DBL (p, genie_airy_ai_scaled_real, gsl_sf_airy_Ai_scaled_e);

//! @brief PROC airy bi scaled = (REAL x) REAL

PROC_R_R_DBL (p, genie_airy_bi_scaled_real, gsl_sf_airy_Bi_scaled_e);

//! @brief PROC airy ai deriv = (REAL x) REAL

PROC_R_R_DBL (p, genie_airy_ai_deriv_real, gsl_sf_airy_Ai_deriv_e);

//! @brief PROC airy bi deriv = (REAL x) REAL

PROC_R_R_DBL (p, genie_airy_bi_deriv_real, gsl_sf_airy_Bi_deriv_e);

//! @brief PROC airy ai deriv scaled = (REAL x) REAL

PROC_R_R_DBL (p, genie_airy_ai_deriv_scaled_real, gsl_sf_airy_Ai_deriv_scaled_e);

//! @brief PROC airy bi deriv scaled = (REAL x) REAL

PROC_R_R_DBL (p, genie_airy_bi_deriv_scaled_real, gsl_sf_airy_Bi_deriv_scaled_e);

//! @brief PROC airy zero ai = (INT s) REAL

PROC_I_R (p, genie_airy_zero_ai_real, gsl_sf_airy_zero_Ai_e);

//! @brief PROC airy zero bi = (INT s) REAL

PROC_I_R (p, genie_airy_zero_bi_real, gsl_sf_airy_zero_Bi_e);

//! @brief PROC airy zero ai deriv = (INT s) REAL

PROC_I_R (p, genie_airy_zero_ai_deriv_real, gsl_sf_airy_zero_Ai_deriv_e);

//! @brief PROC airy zero bi deriv = (INT s) REAL

PROC_I_R (p, genie_airy_zero_bi_deriv_real, gsl_sf_airy_zero_Bi_deriv_e);

//! @brief PROC clausen = (REAL x) REAL

PROC_R_R (p, genie_clausen_real, gsl_sf_clausen_e);

//! @brief PROC bessel jn0 = (REAL x) REAL

PROC_R_R (p, genie_bessel_jn0_real, gsl_sf_bessel_J0_e);

//! @brief PROC bessel jn1 = (REAL x) REAL

PROC_R_R (p, genie_bessel_jn1_real, gsl_sf_bessel_J1_e);

//! @brief PROC bessel jn = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_bessel_jn_real, gsl_sf_bessel_Jn_e);

//! @brief PROC bessel yn0 = (REAL x) REAL

PROC_R_R (p, genie_bessel_yn0_real, gsl_sf_bessel_Y0_e);

//! @brief PROC bessel yn1 = (REAL x) REAL

PROC_R_R (p, genie_bessel_yn1_real, gsl_sf_bessel_Y1_e);

//! @brief PROC bessel yn = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_bessel_yn_real, gsl_sf_bessel_Yn_e);

//! @brief PROC bessel in0 = (REAL x) REAL

PROC_R_R (p, genie_bessel_in0_real, gsl_sf_bessel_I0_e);

//! @brief PROC bessel in1 = (REAL x) REAL

PROC_R_R (p, genie_bessel_in1_real, gsl_sf_bessel_I1_e);

//! @brief PROC bessel in = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_bessel_in_real, gsl_sf_bessel_In_e);

//! @brief PROC bessel in0 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_in0_scaled_real, gsl_sf_bessel_I0_scaled_e);

//! @brief PROC bessel in1 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_in1_scaled_real, gsl_sf_bessel_I1_scaled_e);

//! @brief PROC bessel in scaled = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_bessel_in_scaled_real, gsl_sf_bessel_In_scaled_e);

//! @brief PROC bessel kn0 = (REAL x) REAL

PROC_R_R (p, genie_bessel_kn0_real, gsl_sf_bessel_K0_e);

//! @brief PROC bessel kn1 = (REAL x) REAL

PROC_R_R (p, genie_bessel_kn1_real, gsl_sf_bessel_K1_e);

//! @brief PROC bessel kn = (INT n, REAL x) REAL 

PROC_I_R_R (p, genie_bessel_kn_real, gsl_sf_bessel_Kn_e);

//! @brief PROC bessel kn0 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_kn0_scaled_real, gsl_sf_bessel_K0_scaled_e);

//! @brief PROC bessel kn1 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_kn1_scaled_real, gsl_sf_bessel_K1_scaled_e);

//! @brief PROC bessel kn scaled = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_bessel_kn_scaled_real, gsl_sf_bessel_Kn_scaled_e);

//! @brief PROC bessel jl0 = (REAL x) REAL

PROC_R_R (p, genie_bessel_jl0_real, gsl_sf_bessel_j0_e);

//! @brief PROC bessel jl1 = (REAL x) REAL

PROC_R_R (p, genie_bessel_jl1_real, gsl_sf_bessel_j1_e);

//! @brief PROC bessel jl2 = (REAL x) REAL

PROC_R_R (p, genie_bessel_jl2_real, gsl_sf_bessel_j2_e);

//! @brief PROC bessel jl = (INT l, REAL x) REAL

PROC_I_R_R (p, genie_bessel_jl_real, gsl_sf_bessel_jl_e);

//! @brief PROC bessel yl0 = (REAL x) REAL

PROC_R_R (p, genie_bessel_yl0_real, gsl_sf_bessel_y0_e);

//! @brief PROC bessel yl1 = (REAL x) REAL

PROC_R_R (p, genie_bessel_yl1_real, gsl_sf_bessel_y1_e);

//! @brief PROC bessel yl2 = (REAL x) REAL

PROC_R_R (p, genie_bessel_yl2_real, gsl_sf_bessel_y2_e);

//! @brief PROC bessel yl = (INT l, REAL x) REAL

PROC_I_R_R (p, genie_bessel_yl_real, gsl_sf_bessel_yl_e);

//! @brief PROC bessel il0 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_il0_scaled_real, gsl_sf_bessel_i0_scaled_e);

//! @brief PROC bessel il1 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_il1_scaled_real, gsl_sf_bessel_i1_scaled_e);

//! @brief PROC bessel il2 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_il2_scaled_real, gsl_sf_bessel_i2_scaled_e);

//! @brief PROC bessel il scaled = (INT l, REAL x) REAL

PROC_I_R_R (p, genie_bessel_il_scaled_real, gsl_sf_bessel_il_scaled_e);

//! @brief PROC bessel kl0 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_kl0_scaled_real, gsl_sf_bessel_k0_scaled_e);

//! @brief PROC bessel kl1 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_kl1_scaled_real, gsl_sf_bessel_k1_scaled_e);

//! @brief PROC bessel kl2 scaled = (REAL x) REAL

PROC_R_R (p, genie_bessel_kl2_scaled_real, gsl_sf_bessel_k2_scaled_e);

//! @brief PROC bessel kl scaled = (INT l, REAL x) REAL

PROC_I_R_R (p, genie_bessel_kl_scaled_real, gsl_sf_bessel_kl_scaled_e);

//! @brief PROC bessel jnu = (REAL nu, REAL x) REAL

PROC_R_R_R (p, genie_bessel_jnu_real, gsl_sf_bessel_Jnu_e);

//! @brief PROC bessel ynu = (REAL nu, x) REAL

PROC_R_R_R (p, genie_bessel_ynu_real, gsl_sf_bessel_Ynu_e);

//! @brief PROC bessel inu = (REAL nu, x) REAL

PROC_R_R_R (p, genie_bessel_inu_real, gsl_sf_bessel_Inu_e);

//! @brief PROC bessel inu scaled = (REAL nu, x) REAL

PROC_R_R_R (p, genie_bessel_inu_scaled_real, gsl_sf_bessel_Inu_scaled_e);

//! @brief PROC bessel knu = (REAL nu, x) REAL

PROC_R_R_R (p, genie_bessel_knu_real, gsl_sf_bessel_Knu_e);

//! @brief PROC bessel ln knu = (REAL nu, x) REAL

PROC_R_R_R (p, genie_bessel_ln_knu_real, gsl_sf_bessel_lnKnu_e);

//! @brief PROC bessel knu scaled = (REAL nu, x) REAL

PROC_R_R_R (p, genie_bessel_knu_scaled_real, gsl_sf_bessel_Knu_scaled_e);

//! @brief PROC bessel zero jnu0 = (INT s) REAL

PROC_I_R (p, genie_bessel_zero_jnu0_real, gsl_sf_bessel_zero_J0_e);

//! @brief PROC bessel zero jnu1 = (INT s) REAL

PROC_I_R (p, genie_bessel_zero_jnu1_real, gsl_sf_bessel_zero_J1_e);

//! @brief PROC bessel zero jnu = (INT s, REAL nu) REAL

PROC_I_R_R_REVERSED (p, genie_bessel_zero_jnu_real, gsl_sf_bessel_zero_Jnu_e);

//! @brief PROC dawson = (REAL x) REAL

PROC_R_R (p, genie_dawson_real, gsl_sf_dawson_e);

//! @brief PROC debye 1 = (REAL x) REAL

PROC_R_R (p, genie_debye_1_real, gsl_sf_debye_1_e);

//! @brief PROC debye 2 = (REAL x) REAL

PROC_R_R (p, genie_debye_2_real, gsl_sf_debye_2_e);

//! @brief PROC debye 3 = (REAL x) REAL

PROC_R_R (p, genie_debye_3_real, gsl_sf_debye_3_e);

//! @brief PROC debye 4 = (REAL x) REAL

PROC_R_R (p, genie_debye_4_real, gsl_sf_debye_4_e);

//! @brief PROC debye 5 = (REAL x) REAL

PROC_R_R (p, genie_debye_5_real, gsl_sf_debye_5_e);

//! @brief PROC debye 6 = (REAL x) REAL

PROC_R_R (p, genie_debye_6_real, gsl_sf_debye_6_e);

//! @brief PROC dilog = (REAL x) REAL

PROC_R_R (p, genie_dilog_real, gsl_sf_dilog_e);

//! @brief PROC ellint k comp = (REAL k) REAL

PROC_R_R_DBL (p, genie_ellint_k_comp_real, gsl_sf_ellint_Kcomp_e);

//! @brief PROC ellint e comp = (REAL k) REAL

PROC_R_R_DBL (p, genie_ellint_e_comp_real, gsl_sf_ellint_Ecomp_e);

//! @brief PROC ellint p comp = (REAL k, n) REAL

PROC_R_R_R_DBL (p, genie_ellint_p_comp_real, gsl_sf_ellint_Pcomp_e);

//! @brief PROC ellint d = (REAL phi, k) REAL

PROC_R_R_R_DBL (p, genie_ellint_d_real, gsl_sf_ellint_D_e);

//! @brief PROC ellint e = (REAL phi, k) REAL

PROC_R_R_R_DBL (p, genie_ellint_e_real, gsl_sf_ellint_E_e);

//! @brief PROC ellint f = (REAL phi, k) REAL

PROC_R_R_R_DBL (p, genie_ellint_f_real, gsl_sf_ellint_F_e);

//! @brief PROC ellint p = (REAL phi, k, n) REAL

PROC_R_R_R_R_DBL (p, genie_ellint_p_real, gsl_sf_ellint_P_e);

//! @brief PROC ellint rc = (REAL x, y) REAL

PROC_R_R_R_DBL (p, genie_ellint_rc_real, gsl_sf_ellint_RC_e);

//! @brief PROC ellint rf = (REAL x, y, z) REAL

PROC_R_R_R_R_DBL (p, genie_ellint_rf_real, gsl_sf_ellint_RF_e);

//! @brief PROC ellint rd = (REAL x, y, z) REAL

PROC_R_R_R_R_DBL (p, genie_ellint_rd_real, gsl_sf_ellint_RD_e);

//! @brief PROC ellint rj = (REAL x, y, z, p) REAL

PROC_R_R_R_R_R_DBL (p, genie_ellint_rj_real, gsl_sf_ellint_RJ_e);

//! @brief PROC expint e1 = (REAL x) REAL

PROC_R_R (p, genie_expint_e1_real, gsl_sf_expint_E1_e);

//! @brief PROC expint e2 = (REAL x) REAL

PROC_R_R (p, genie_expint_e2_real, gsl_sf_expint_E2_e);

//! @brief PROC expint en = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_expint_en_real, gsl_sf_expint_En_e);

//! @brief PROC expint ei = (REAL x) REAL

PROC_R_R (p, genie_expint_ei_real, gsl_sf_expint_Ei_e);

//! @brief PROC shi = (REAL x) REAL

PROC_R_R (p, genie_shi_real, gsl_sf_Shi_e);

//! @brief PROC chi = (REAL x) REAL

PROC_R_R (p, genie_chi_real, gsl_sf_Chi_e);

//! @brief PROC expint 3 = (REAL x) REAL

PROC_R_R (p, genie_expint_3_real, gsl_sf_expint_3_e);

//! @brief PROC si = (REAL x) REAL

PROC_R_R (p, genie_si_real, gsl_sf_Si_e);

//! @brief PROC ci = (REAL x) REAL

PROC_R_R (p, genie_ci_real, gsl_sf_Ci_e);

//! @brief PROC atanint = (REAL x) REAL

PROC_R_R (p, genie_atanint_real, gsl_sf_atanint_e);

//! @brief PROC fermi dirac m1 = (REAL x) REAL

PROC_R_R (p, genie_fermi_dirac_m1_real, gsl_sf_fermi_dirac_m1_e);

//! @brief PROC fermi dirac 0 = (REAL x) REAL

PROC_R_R (p, genie_fermi_dirac_0_real, gsl_sf_fermi_dirac_0_e);

//! @brief PROC fermi dirac 1 = (REAL x) REAL

PROC_R_R (p, genie_fermi_dirac_1_real, gsl_sf_fermi_dirac_1_e);

//! @brief PROC fermi dirac 2 = (REAL x) REAL

PROC_R_R (p, genie_fermi_dirac_2_real, gsl_sf_fermi_dirac_2_e);

//! @brief PROC fermi dirac int = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_fermi_dirac_int_real, gsl_sf_fermi_dirac_int_e);

//! @brief PROC fermi dirac m half = (REAL x) REAL

PROC_R_R (p, genie_fermi_dirac_mhalf_real, gsl_sf_fermi_dirac_mhalf_e);

//! @brief PROC fermi dirac half = (REAL x) REAL

PROC_R_R (p, genie_fermi_dirac_half_real, gsl_sf_fermi_dirac_half_e);

//! @brief PROC fermi dirac 3 half = (REAL x) REAL

PROC_R_R (p, genie_fermi_dirac_3half_real, gsl_sf_fermi_dirac_3half_e);

//! @brief PROC fermi dirac inc0 = (REAL x, b) REAL

PROC_R_R_R (p, genie_fermi_dirac_inc_0_real, gsl_sf_fermi_dirac_inc_0_e);

//! @brief PROC digamma = (REAL x) REAL

PROC_R_R (p, genie_digamma_real, gsl_sf_psi_e);

//! @brief PROC gamma star = (REAL x) REAL

PROC_R_R (p, genie_gammastar_real, gsl_sf_gammastar_e);

//! @brief PROC gamma inv = (REAL x) REAL

PROC_R_R (p, genie_gammainv_real, gsl_sf_gammainv_e);

//! @brief PROC double fact = (INT n) REAL

PROC_I_R (p, genie_doublefact_real, gsl_sf_doublefact_e);

//! @brief PROC ln double fact = (INT n) REAL

PROC_I_R (p, genie_lndoublefact_real, gsl_sf_lndoublefact_e);

//! @brief PROC taylor coeff = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_taylorcoeff_real, gsl_sf_taylorcoeff_e);

//! @brief PROC poch = (REAL a, x) REAL

PROC_R_R_R (p, genie_poch_real, gsl_sf_poch_e);

//! @brief PROC lnpoch = (REAL a, x) REAL

PROC_R_R_R (p, genie_lnpoch_real, gsl_sf_lnpoch_e);

//! @brief PROC pochrel = (REAL a, x) REAL

PROC_R_R_R (p, genie_pochrel_real, gsl_sf_pochrel_e);

//! @brief PROC beta inc = (REAL a, b, x) REAL

PROC_R_R_R_R (p, genie_beta_inc_real, gsl_sf_beta_inc_e);

//! @brief PROC gamma inc = (REAL a, x) REAL

PROC_R_R_R (p, genie_gamma_inc_real, gsl_sf_gamma_inc_e);

//! @brief PROC gamma inc q = (REAL a, x) REAL

PROC_R_R_R (p, genie_gamma_inc_q_real, gsl_sf_gamma_inc_Q_e);

//! @brief PROC gamma inc p = (REAL a, x) REAL

PROC_R_R_R (p, genie_gamma_inc_p_real, gsl_sf_gamma_inc_P_e);

//! @brief PROC gegenpoly 1 = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_gegenpoly_1_real, gsl_sf_gegenpoly_1_e);

//! @brief PROC gegenpoly 2 = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_gegenpoly_2_real, gsl_sf_gegenpoly_2_e);

//! @brief PROC gegenpoly 3 = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_gegenpoly_3_real, gsl_sf_gegenpoly_3_e);

//! @brief PROC gegenpoly n = (INT n, REAL lambda, x) REAL

PROC_I_R_R_R (p, genie_gegenpoly_n_real, gsl_sf_gegenpoly_n_e);

//! @brief PROC laguerre 1 = (REAL a, x) REAL

PROC_R_R_R (p, genie_laguerre_1_real, gsl_sf_laguerre_1_e);

//! @brief PROC laguerre 2 = (REAL a, x) REAL

PROC_R_R_R (p, genie_laguerre_2_real, gsl_sf_laguerre_2_e);

//! @brief PROC laguerre 3 = (REAL a, x) REAL

PROC_R_R_R (p, genie_laguerre_3_real, gsl_sf_laguerre_3_e);

//! @brief PROC laguerre n = (INT n, REAL a, x) REAL

PROC_I_R_R_R (p, genie_laguerre_n_real, gsl_sf_laguerre_n_e);

//! @brief PROC lambert w0 = (REAL x) REAL

PROC_R_R (p, genie_lambert_w0_real, gsl_sf_lambert_W0_e);

//! @brief PROC lambert wm1 = (REAL x) REAL

PROC_R_R (p, genie_lambert_wm1_real, gsl_sf_lambert_Wm1_e);

//! @brief PROC legendre p1 = (REAL x) REAL

PROC_R_R (p, genie_legendre_p1_real, gsl_sf_legendre_P1_e);

//! @brief PROC legendre p2 = (REAL x) REAL

PROC_R_R (p, genie_legendre_p2_real, gsl_sf_legendre_P2_e);

//! @brief PROC legendre p3 = (REAL x) REAL

PROC_R_R (p, genie_legendre_p3_real, gsl_sf_legendre_P3_e);

//! @brief PROC legendre pl = (INT l, REAL x) REAL

PROC_I_R_R (p, genie_legendre_pl_real, gsl_sf_legendre_Pl_e);

//! @brief PROC legendre q0 = (REAL x) REAL

PROC_R_R (p, genie_legendre_q0_real, gsl_sf_legendre_Q0_e);

//! @brief PROC legendre q1 = (REAL x) REAL

PROC_R_R (p, genie_legendre_q1_real, gsl_sf_legendre_Q1_e);

//! @brief PROC legendre ql = (INT l, REAL x) REAL

PROC_I_R_R (p, genie_legendre_ql_real, gsl_sf_legendre_Ql_e);

//! @brief PROC conicalp half = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_conicalp_half_real, gsl_sf_conicalP_half_e);

//! @brief PROC conicalp mhalf = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_conicalp_mhalf_real, gsl_sf_conicalP_mhalf_e);

//! @brief PROC conicalp 0 = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_conicalp_0_real, gsl_sf_conicalP_0_e);

//! @brief PROC conicalp 1 = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_conicalp_1_real, gsl_sf_conicalP_1_e);

//! @brief PROC conicalp sph reg = (INT n, REAL lambda, x) REAL

PROC_I_R_R_R (p, genie_conicalp_sph_reg_real, gsl_sf_conicalP_sph_reg_e);

//! @brief PROC conicalp cyl reg = (INT n, REAL lambda, x) REAL

PROC_I_R_R_R (p, genie_conicalp_cyl_reg_real, gsl_sf_conicalP_cyl_reg_e);

//! @brief PROC legendre h3d 0 = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_legendre_h3d_0_real, gsl_sf_legendre_H3d_0_e);

//! @brief PROC legendre h3d 1 = (REAL lambda, x) REAL

PROC_R_R_R (p, genie_legendre_h3d_1_real, gsl_sf_legendre_H3d_1_e);

//! @brief PROC legendre h3d = (INT l, REAL lambda, x) REAL

PROC_I_R_R_R (p, genie_legendre_H3d_real, gsl_sf_legendre_H3d_e);

//! @brief PROC psi int = (INT n) REAL

PROC_I_R (p, genie_psi_int_real, gsl_sf_psi_int_e);

//! @brief PROC psi = (INT n) REAL

PROC_R_R (p, genie_psi_real, gsl_sf_psi_e);

//! @brief PROC psi 1piy = (INT n) REAL

PROC_R_R (p, genie_psi_1piy_real, gsl_sf_psi_1piy_e);

//! @brief PROC psi 1 = (INT n) REAL

PROC_I_R (p, genie_psi_1_int_real, gsl_sf_psi_1_int_e);

//! @brief PROC psi 1 = (REAL x) REAL

PROC_R_R (p, genie_psi_1_real, gsl_sf_psi_1_e);

//! @brief PROC psi n = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_psi_n_real, gsl_sf_psi_n_e);

//! @brief PROC synchrotron 1 = (REAL x) REAL

PROC_R_R (p, genie_synchrotron_1_real, gsl_sf_synchrotron_1_e);

//! @brief PROC synchrotron 2 = (REAL x) REAL

PROC_R_R (p, genie_synchrotron_2_real, gsl_sf_synchrotron_2_e);

//! @brief PROC transport 2 = (REAL x) REAL

PROC_R_R (p, genie_transport_2_real, gsl_sf_transport_2_e);

//! @brief PROC transport 3 = (REAL x) REAL

PROC_R_R (p, genie_transport_3_real, gsl_sf_transport_3_e);

//! @brief PROC transport 4 = (REAL x) REAL

PROC_R_R (p, genie_transport_4_real, gsl_sf_transport_4_e);

//! @brief PROC transport 5 = (REAL x) REAL

PROC_R_R (p, genie_transport_5_real, gsl_sf_transport_5_e);

//! @brief PROC hypot = (REAL x) REAL

PROC_R_R_R (p, genie_hypot_real, gsl_sf_hypot_e);

//! @brief PROC sinc = (REAL x) REAL

PROC_R_R (p, genie_sinc_real, gsl_sf_sinc_e);

//! @brief PROC lnsinh = (REAL x) REAL

PROC_R_R (p, genie_lnsinh_real, gsl_sf_lnsinh_e);

//! @brief PROC lncosh = (REAL x) REAL

PROC_R_R (p, genie_lncosh_real, gsl_sf_lncosh_e);

//! @brief PROC angle restrict symm = (REAL theta) REAL

PROC_RR_R (p, genie_angle_restrict_symm_real, gsl_sf_angle_restrict_symm_e);

//! @brief PROC angle restrict pos = (REAL theta) REAL

PROC_RR_R (p, genie_angle_restrict_pos_real, gsl_sf_angle_restrict_pos_e);

//! @brief PROC zeta int = (INT n) REAL

PROC_I_R (p, genie_zeta_int_real, gsl_sf_zeta_int_e);

//! @brief PROC zeta = (REAL s) REAL

PROC_R_R (p, genie_zeta_real, gsl_sf_zeta_e);

//! @brief PROC zetam1 int = (INT n) REAL

PROC_I_R (p, genie_zetam1_int_real, gsl_sf_zetam1_int_e);

//! @brief PROC zetam1 = (REAL s) REAL

PROC_R_R (p, genie_zetam1_real, gsl_sf_zetam1_e);

//! @brief PROC hzeta = (REAL s, q) REAL

PROC_R_R_R (p, genie_hzeta_real, gsl_sf_hzeta_e);

//! @brief PROC eta int = (INT n) REAL

PROC_I_R (p, genie_etaint_real, gsl_sf_eta_int_e);

//! @brief PROC eta = (REAL s) REAL

PROC_R_R (p, genie_eta_real, gsl_sf_eta_e);

//! @brief PROC expm1 = (REAL x) REAL

PROC_R_R (p, genie_expm1_real, gsl_sf_expm1_e);

//! @brief PROC exprel = (REAL x) REAL

PROC_R_R (p, genie_exprel_real, gsl_sf_exprel_e);

//! @brief PROC exprel2 = (REAL x) REAL

PROC_R_R (p, genie_exprel_2_real, gsl_sf_exprel_2_e);

//! @brief PROC exprel n = (INT l, REAL x) REAL

PROC_I_R_R (p, genie_exprel_n_real, gsl_sf_exprel_n_e);

//! @brief PROC logabs = (REAL x) REAL

PROC_R_R (p, genie_log_abs_real, gsl_sf_log_abs_e);

//! @brief PROC log1plusx = (REAL x) REAL

PROC_R_R (p, genie_log_1plusx_real, gsl_sf_log_1plusx_e);

//! @brief PROC log1plusxmx = (REAL x) REAL

PROC_R_R (p, genie_log_1plusx_mx_real, gsl_sf_log_1plusx_mx_e);

//! @brief PROC hermite func = (INT n, REAL x) REAL

PROC_I_R_R (p, genie_hermite_func_real, gsl_sf_hermite_func_e);

#endif
