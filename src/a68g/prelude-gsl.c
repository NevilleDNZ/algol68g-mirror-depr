//! @file prelude-gsl.c
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
//! Standard prelude definitions from GSL.

#include "a68g.h"
#include "a68g-optimiser.h"
#include "a68g-prelude.h"
#include "a68g-prelude-gsl.h"
#include "a68g-transput.h"
#include "a68g-mp.h"
#include "a68g-parser.h"
#include "a68g-physics.h"
#include "a68g-double.h"

#if defined (HAVE_GSL)

void stand_gsl_sf (void)
{
  a68_idf (A68_EXT, "airyai", A68_MCACHE (proc_real_real), genie_airy_ai_real);
  a68_idf (A68_EXT, "airyaiscaled", A68_MCACHE (proc_real_real), genie_airy_ai_scaled_real);
  a68_idf (A68_EXT, "airybi", A68_MCACHE (proc_real_real), genie_airy_bi_real);
  a68_idf (A68_EXT, "airybiscaled", A68_MCACHE (proc_real_real), genie_airy_bi_scaled_real);
  a68_idf (A68_EXT, "besselin0", A68_MCACHE (proc_real_real), genie_bessel_in0_real);
  a68_idf (A68_EXT, "besselin1", A68_MCACHE (proc_real_real), genie_bessel_in1_real);
  a68_idf (A68_EXT, "besselin0scaled", A68_MCACHE (proc_real_real), genie_bessel_in0_scaled_real);
  a68_idf (A68_EXT, "besselin1scaled", A68_MCACHE (proc_real_real), genie_bessel_in1_scaled_real);
  a68_idf (A68_EXT, "besseljn0", A68_MCACHE (proc_real_real), genie_bessel_jn0_real);
  a68_idf (A68_EXT, "besseljn1", A68_MCACHE (proc_real_real), genie_bessel_jn1_real);
  a68_idf (A68_EXT, "besselkn0", A68_MCACHE (proc_real_real), genie_bessel_kn0_real);
  a68_idf (A68_EXT, "besselkn1", A68_MCACHE (proc_real_real), genie_bessel_kn1_real);
  a68_idf (A68_EXT, "besselkn0scaled", A68_MCACHE (proc_real_real), genie_bessel_kn0_scaled_real);
  a68_idf (A68_EXT, "besselkn1scaled", A68_MCACHE (proc_real_real), genie_bessel_kn1_scaled_real);
  a68_idf (A68_EXT, "besselyn0", A68_MCACHE (proc_real_real), genie_bessel_yn0_real);
  a68_idf (A68_EXT, "besselyn1", A68_MCACHE (proc_real_real), genie_bessel_yn1_real);
  a68_idf (A68_EXT, "expinte1", A68_MCACHE (proc_real_real), genie_expint_e1_real);
  a68_idf (A68_EXT, "expintei", A68_MCACHE (proc_real_real), genie_expint_ei_real);
  a68_idf (A68_EXT, "dawson", A68_MCACHE (proc_real_real), genie_dawson_real);
  a68_idf (A68_EXT, "exprel", A68_MCACHE (proc_real_real), genie_exprel_real);
  a68_idf (A68_EXT, "betaincgsl", A68_MCACHE (proc_real_real_real_real), genie_beta_inc_real);
  a68_idf (A68_EXT, "poch", A68_MCACHE (proc_real_real_real), genie_poch_real);
  a68_idf (A68_EXT, "digamma", A68_MCACHE (proc_real_real), genie_digamma_real);
  a68_idf (A68_EXT, "airyaiderivative", A68_MCACHE (proc_real_real), genie_airy_ai_deriv_real);
  a68_idf (A68_EXT, "airyaideriv", A68_MCACHE (proc_real_real), genie_airy_ai_deriv_real);
  a68_idf (A68_EXT, "airyaiderivscaled", A68_MCACHE (proc_real_real), genie_airy_ai_deriv_scaled_real);
  a68_idf (A68_EXT, "airybiderivative", A68_MCACHE (proc_real_real), genie_airy_bi_deriv_real);
  a68_idf (A68_EXT, "airybideriv", A68_MCACHE (proc_real_real), genie_airy_bi_deriv_real);
  a68_idf (A68_EXT, "airybiderivscaled", A68_MCACHE (proc_real_real), genie_airy_bi_deriv_scaled_real);
  a68_idf (A68_EXT, "airyzeroaideriv", A68_MCACHE (proc_int_real), genie_airy_zero_ai_deriv_real);
  a68_idf (A68_EXT, "airyzeroai", A68_MCACHE (proc_int_real), genie_airy_zero_ai_real);
  a68_idf (A68_EXT, "airyzerobideriv", A68_MCACHE (proc_int_real), genie_airy_zero_bi_deriv_real);
  a68_idf (A68_EXT, "airyzerobi", A68_MCACHE (proc_int_real), genie_airy_zero_bi_real);
  a68_idf (A68_EXT, "anglerestrictpos", A68_MCACHE (proc_real_real), genie_angle_restrict_pos_real);
  a68_idf (A68_EXT, "anglerestrictsymm", A68_MCACHE (proc_real_real), genie_angle_restrict_symm_real);
  a68_idf (A68_EXT, "atanint", A68_MCACHE (proc_real_real), genie_atanint_real);
  a68_idf (A68_EXT, "besselil0scaled", A68_MCACHE (proc_real_real), genie_bessel_il0_scaled_real);
  a68_idf (A68_EXT, "besselil1scaled", A68_MCACHE (proc_real_real), genie_bessel_il1_scaled_real);
  a68_idf (A68_EXT, "besselil2scaled", A68_MCACHE (proc_real_real), genie_bessel_il2_scaled_real);
  a68_idf (A68_EXT, "besselilscaled", A68_MCACHE (proc_int_real_real), genie_bessel_il_scaled_real);
  a68_idf (A68_EXT, "besselilscaled", A68_MCACHE (proc_real_real), genie_bessel_il_scaled_real);
  a68_idf (A68_EXT, "besselin", A68_MCACHE (proc_int_real_real), genie_bessel_in_real);
  a68_idf (A68_EXT, "besselinscaled", A68_MCACHE (proc_int_real_real), genie_bessel_in_scaled_real);
  a68_idf (A68_EXT, "besselinscaled", A68_MCACHE (proc_real_real), genie_bessel_in_scaled_real);
  a68_idf (A68_EXT, "besselinu", A68_MCACHE (proc_real_real_real), genie_bessel_inu_real);
  a68_idf (A68_EXT, "besselinuscaled", A68_MCACHE (proc_real_real_real), genie_bessel_inu_scaled_real);
  a68_idf (A68_EXT, "besseljl0", A68_MCACHE (proc_real_real), genie_bessel_jl0_real);
  a68_idf (A68_EXT, "besseljl1", A68_MCACHE (proc_real_real), genie_bessel_jl1_real);
  a68_idf (A68_EXT, "besseljl2", A68_MCACHE (proc_real_real), genie_bessel_jl2_real);
  a68_idf (A68_EXT, "besseljl", A68_MCACHE (proc_int_real_real), genie_bessel_jl_real);
  a68_idf (A68_EXT, "besseljn", A68_MCACHE (proc_int_real_real), genie_bessel_jn_real);
  a68_idf (A68_EXT, "besselkl0scaled", A68_MCACHE (proc_real_real), genie_bessel_kl0_scaled_real);
  a68_idf (A68_EXT, "besselkl1scaled", A68_MCACHE (proc_real_real), genie_bessel_kl1_scaled_real);
  a68_idf (A68_EXT, "besselkl2scaled", A68_MCACHE (proc_real_real), genie_bessel_kl2_scaled_real);
  a68_idf (A68_EXT, "besselklscaled", A68_MCACHE (proc_int_real_real), genie_bessel_kl_scaled_real);
  a68_idf (A68_EXT, "besselklscaled", A68_MCACHE (proc_real_real), genie_bessel_kl_scaled_real);
  a68_idf (A68_EXT, "besselkn", A68_MCACHE (proc_int_real_real), genie_bessel_kn_real);
  a68_idf (A68_EXT, "besselknscaled", A68_MCACHE (proc_int_real_real), genie_bessel_kn_scaled_real);
  a68_idf (A68_EXT, "besselkn_scaled", A68_MCACHE (proc_real_real), genie_bessel_kn_scaled_real);
  a68_idf (A68_EXT, "besselknu", A68_MCACHE (proc_real_real_real), genie_bessel_knu_real);
  a68_idf (A68_EXT, "besselknuscaled", A68_MCACHE (proc_real_real), genie_bessel_knu_scaled_real);
  a68_idf (A68_EXT, "besselknuscaled", A68_MCACHE (proc_real_real_real), genie_bessel_knu_scaled_real);
  a68_idf (A68_EXT, "bessellnknu", A68_MCACHE (proc_real_real), genie_bessel_ln_knu_real);
  a68_idf (A68_EXT, "bessellnknu", A68_MCACHE (proc_real_real_real), genie_bessel_ln_knu_real);
  a68_idf (A68_EXT, "besselyl0", A68_MCACHE (proc_real_real), genie_bessel_yl0_real);
  a68_idf (A68_EXT, "besselyl1", A68_MCACHE (proc_real_real), genie_bessel_yl1_real);
  a68_idf (A68_EXT, "besselyl2", A68_MCACHE (proc_real_real), genie_bessel_yl2_real);
  a68_idf (A68_EXT, "besselyl", A68_MCACHE (proc_int_real_real), genie_bessel_yl_real);
  a68_idf (A68_EXT, "besselyn", A68_MCACHE (proc_int_real_real), genie_bessel_yn_real);
  a68_idf (A68_EXT, "besselynu", A68_MCACHE (proc_real_real_real), genie_bessel_ynu_real);
  a68_idf (A68_EXT, "besselzeroj0", A68_MCACHE (proc_int_real), genie_bessel_zero_jnu0_real);
  a68_idf (A68_EXT, "besselzeroj1", A68_MCACHE (proc_int_real), genie_bessel_zero_jnu1_real);
  a68_idf (A68_EXT, "besselzerojnu", A68_MCACHE (proc_int_real_real), genie_bessel_zero_jnu_real);
  a68_idf (A68_EXT, "chi", A68_MCACHE (proc_real_real), genie_chi_real);
  a68_idf (A68_EXT, "ci", A68_MCACHE (proc_real_real), genie_ci_real);
  a68_idf (A68_EXT, "clausen", A68_MCACHE (proc_real_real), genie_clausen_real);
  a68_idf (A68_EXT, "conicalp0", A68_MCACHE (proc_real_real_real), genie_conicalp_0_real);
  a68_idf (A68_EXT, "conicalp1", A68_MCACHE (proc_real_real_real), genie_conicalp_1_real);
  a68_idf (A68_EXT, "conicalpcylreg", A68_MCACHE (proc_int_real_real_real), genie_conicalp_cyl_reg_real);
  a68_idf (A68_EXT, "conicalphalf", A68_MCACHE (proc_real_real_real), genie_conicalp_half_real);
  a68_idf (A68_EXT, "conicalpmhalf", A68_MCACHE (proc_real_real_real), genie_conicalp_mhalf_real);
  a68_idf (A68_EXT, "conicalpsphreg", A68_MCACHE (proc_int_real_real_real), genie_conicalp_sph_reg_real);
  a68_idf (A68_EXT, "debye1", A68_MCACHE (proc_real_real), genie_debye_1_real);
  a68_idf (A68_EXT, "debye2", A68_MCACHE (proc_real_real), genie_debye_2_real);
  a68_idf (A68_EXT, "debye3", A68_MCACHE (proc_real_real), genie_debye_3_real);
  a68_idf (A68_EXT, "debye4", A68_MCACHE (proc_real_real), genie_debye_4_real);
  a68_idf (A68_EXT, "debye5", A68_MCACHE (proc_real_real), genie_debye_5_real);
  a68_idf (A68_EXT, "debye6", A68_MCACHE (proc_real_real), genie_debye_6_real);
  a68_idf (A68_EXT, "dilog", A68_MCACHE (proc_real_real), genie_dilog_real);
  a68_idf (A68_EXT, "doublefact", A68_MCACHE (proc_int_real), genie_doublefact_real);
  a68_idf (A68_EXT, "ellintd", A68_MCACHE (proc_real_real_real), genie_ellint_d_real);
  a68_idf (A68_EXT, "ellintecomp", A68_MCACHE (proc_real_real), genie_ellint_e_comp_real);
  a68_idf (A68_EXT, "ellinte", A68_MCACHE (proc_real_real_real), genie_ellint_e_real);
  a68_idf (A68_EXT, "ellintf", A68_MCACHE (proc_real_real_real), genie_ellint_f_real);
  a68_idf (A68_EXT, "ellintkcomp", A68_MCACHE (proc_real_real), genie_ellint_k_comp_real);
  a68_idf (A68_EXT, "ellintpcomp", A68_MCACHE (proc_real_real_real), genie_ellint_p_comp_real);
  a68_idf (A68_EXT, "ellintp", A68_MCACHE (proc_real_real_real_real), genie_ellint_p_real);
  a68_idf (A68_EXT, "ellintrc", A68_MCACHE (proc_real_real_real), genie_ellint_rc_real);
  a68_idf (A68_EXT, "ellintrd", A68_MCACHE (proc_real_real_real_real), genie_ellint_rd_real);
  a68_idf (A68_EXT, "ellintrf", A68_MCACHE (proc_real_real_real_real), genie_ellint_rf_real);
  a68_idf (A68_EXT, "ellintrj", A68_MCACHE (proc_real_real_real_real_real), genie_ellint_rj_real);
  a68_idf (A68_EXT, "etaint", A68_MCACHE (proc_int_real), genie_etaint_real);
  a68_idf (A68_EXT, "eta", A68_MCACHE (proc_real_real), genie_eta_real);
  a68_idf (A68_EXT, "expint3", A68_MCACHE (proc_real_real), genie_expint_3_real);
  a68_idf (A68_EXT, "expinte2", A68_MCACHE (proc_real_real), genie_expint_e2_real);
  a68_idf (A68_EXT, "expinten", A68_MCACHE (proc_int_real_real), genie_expint_en_real);
  a68_idf (A68_EXT, "expm1", A68_MCACHE (proc_real_real), genie_expm1_real);
  a68_idf (A68_EXT, "exprel2", A68_MCACHE (proc_real_real), genie_exprel_2_real);
  a68_idf (A68_EXT, "expreln", A68_MCACHE (proc_int_real_real), genie_exprel_n_real);
  a68_idf (A68_EXT, "fermidirac0", A68_MCACHE (proc_real_real), genie_fermi_dirac_0_real);
  a68_idf (A68_EXT, "fermidirac1", A68_MCACHE (proc_real_real), genie_fermi_dirac_1_real);
  a68_idf (A68_EXT, "fermidirac2", A68_MCACHE (proc_real_real), genie_fermi_dirac_2_real);
  a68_idf (A68_EXT, "fermidirac3half", A68_MCACHE (proc_real_real), genie_fermi_dirac_3half_real);
  a68_idf (A68_EXT, "fermidirachalf", A68_MCACHE (proc_real_real), genie_fermi_dirac_half_real);
  a68_idf (A68_EXT, "fermidiracinc0", A68_MCACHE (proc_real_real_real), genie_fermi_dirac_inc_0_real);
  a68_idf (A68_EXT, "fermidiracint", A68_MCACHE (proc_int_real_real), genie_fermi_dirac_int_real);
  a68_idf (A68_EXT, "fermidiracm1", A68_MCACHE (proc_real_real), genie_fermi_dirac_m1_real);
  a68_idf (A68_EXT, "fermidiracmhalf", A68_MCACHE (proc_real_real), genie_fermi_dirac_mhalf_real);
  a68_idf (A68_EXT, "gammaincgsl", A68_MCACHE (proc_real_real_real), genie_gamma_inc_real);
  a68_idf (A68_EXT, "gammaincp", A68_MCACHE (proc_real_real_real), genie_gamma_inc_p_real);
  a68_idf (A68_EXT, "gammaincq", A68_MCACHE (proc_real_real_real), genie_gamma_inc_q_real);
  a68_idf (A68_EXT, "gammainv", A68_MCACHE (proc_real_real), genie_gammainv_real);
  a68_idf (A68_EXT, "gammastar", A68_MCACHE (proc_real_real), genie_gammastar_real);
  a68_idf (A68_EXT, "gegenpoly1real", A68_MCACHE (proc_real_real_real), genie_gegenpoly_1_real);
  a68_idf (A68_EXT, "gegenpoly2real", A68_MCACHE (proc_real_real_real), genie_gegenpoly_2_real);
  a68_idf (A68_EXT, "gegenpoly3real", A68_MCACHE (proc_real_real_real), genie_gegenpoly_3_real);
  a68_idf (A68_EXT, "gegenpolynreal", A68_MCACHE (proc_real_real_real), genie_gegenpoly_n_real);
  a68_idf (A68_EXT, "hermitefunc", A68_MCACHE (proc_int_real_real), genie_hermite_func_real);
  a68_idf (A68_EXT, "hypot", A68_MCACHE (proc_real_real_real), genie_hypot_real);
  a68_idf (A68_EXT, "hzeta", A68_MCACHE (proc_real_real_real), genie_hzeta_real);
  a68_idf (A68_EXT, "laguerre1real", A68_MCACHE (proc_real_real_real), genie_laguerre_1_real);
  a68_idf (A68_EXT, "laguerre2real", A68_MCACHE (proc_real_real_real), genie_laguerre_2_real);
  a68_idf (A68_EXT, "laguerre3real", A68_MCACHE (proc_real_real_real), genie_laguerre_3_real);
  a68_idf (A68_EXT, "laguerrenreal", A68_MCACHE (proc_real_real_real), genie_laguerre_n_real);
  a68_idf (A68_EXT, "lambertw0", A68_MCACHE (proc_real_real), genie_lambert_w0_real);
  a68_idf (A68_EXT, "lambertwm1", A68_MCACHE (proc_real_real), genie_lambert_wm1_real);
  a68_idf (A68_EXT, "legendreh3d0", A68_MCACHE (proc_real_real_real), genie_legendre_h3d_0_real);
  a68_idf (A68_EXT, "legendreh3d1", A68_MCACHE (proc_real_real_real), genie_legendre_h3d_1_real);
  a68_idf (A68_EXT, "legendreh3d", A68_MCACHE (proc_int_real_real_real), genie_legendre_H3d_real);
  a68_idf (A68_EXT, "legendrep1", A68_MCACHE (proc_real_real), genie_legendre_p1_real);
  a68_idf (A68_EXT, "legendrep2", A68_MCACHE (proc_real_real), genie_legendre_p2_real);
  a68_idf (A68_EXT, "legendrep3", A68_MCACHE (proc_real_real), genie_legendre_p3_real);
  a68_idf (A68_EXT, "legendrepl", A68_MCACHE (proc_int_real_real), genie_legendre_pl_real);
  a68_idf (A68_EXT, "legendreq0", A68_MCACHE (proc_real_real), genie_legendre_q0_real);
  a68_idf (A68_EXT, "legendreq1", A68_MCACHE (proc_real_real), genie_legendre_q1_real);
  a68_idf (A68_EXT, "legendreql", A68_MCACHE (proc_int_real_real), genie_legendre_ql_real);
  a68_idf (A68_EXT, "lncosh", A68_MCACHE (proc_real_real), genie_lncosh_real);
  a68_idf (A68_EXT, "lndoublefact", A68_MCACHE (proc_int_real), genie_lndoublefact_real);
  a68_idf (A68_EXT, "lnpoch", A68_MCACHE (proc_real_real_real), genie_lnpoch_real);
  a68_idf (A68_EXT, "lnsinh", A68_MCACHE (proc_real_real), genie_lnsinh_real);
  a68_idf (A68_EXT, "ln1plusxmx", A68_MCACHE (proc_real_real), genie_log_1plusx_mx_real);
  a68_idf (A68_EXT, "ln1plusx", A68_MCACHE (proc_real_real), genie_log_1plusx_real);
  a68_idf (A68_EXT, "lnabs", A68_MCACHE (proc_real_real), genie_log_abs_real);
  a68_idf (A68_EXT, "pochrel", A68_MCACHE (proc_real_real_real), genie_pochrel_real);
  a68_idf (A68_EXT, "psi1int", A68_MCACHE (proc_int_real), genie_psi_1_int_real);
  a68_idf (A68_EXT, "psi1piy", A68_MCACHE (proc_real_real), genie_psi_1piy_real);
  a68_idf (A68_EXT, "psi1", A68_MCACHE (proc_real_real), genie_psi_1_real);
  a68_idf (A68_EXT, "psiint", A68_MCACHE (proc_int_real), genie_psi_int_real);
  a68_idf (A68_EXT, "psin", A68_MCACHE (proc_int_real_real), genie_psi_n_real);
  a68_idf (A68_EXT, "psi", A68_MCACHE (proc_real_real), genie_psi_real);
  a68_idf (A68_EXT, "shi", A68_MCACHE (proc_real_real), genie_shi_real);
  a68_idf (A68_EXT, "sinc", A68_MCACHE (proc_real_real), genie_sinc_real);
  a68_idf (A68_EXT, "si", A68_MCACHE (proc_real_real), genie_si_real);
  a68_idf (A68_EXT, "synchrotron1", A68_MCACHE (proc_real_real), genie_synchrotron_1_real);
  a68_idf (A68_EXT, "synchrotron2", A68_MCACHE (proc_real_real), genie_synchrotron_2_real);
  a68_idf (A68_EXT, "taylorcoeff", A68_MCACHE (proc_int_real_real), genie_taylorcoeff_real);
  a68_idf (A68_EXT, "transport2", A68_MCACHE (proc_real_real), genie_transport_2_real);
  a68_idf (A68_EXT, "transport3", A68_MCACHE (proc_real_real), genie_transport_3_real);
  a68_idf (A68_EXT, "transport4", A68_MCACHE (proc_real_real), genie_transport_4_real);
  a68_idf (A68_EXT, "transport5", A68_MCACHE (proc_real_real), genie_transport_5_real);
  a68_idf (A68_EXT, "zetaint", A68_MCACHE (proc_int_real), genie_zeta_int_real);
  a68_idf (A68_EXT, "zetam1int", A68_MCACHE (proc_int_real), genie_zetam1_int_real);
  a68_idf (A68_EXT, "zetam1", A68_MCACHE (proc_real_real), genie_zetam1_real);
  a68_idf (A68_EXT, "zeta", A68_MCACHE (proc_real_real), genie_zeta_real);
}

void stand_gsl_linear_algebra (void)
{
  MOID_T *m;
// Vector and matrix pretty print.
  m = a68_proc (M_VOID, M_ROW_REAL, M_INT, NO_MOID);
  a68_idf (A68_EXT, "printvector", m, genie_print_vector);
  m = a68_proc (M_VOID, M_ROW_ROW_REAL, M_INT, NO_MOID);
  a68_idf (A68_EXT, "printmatrix", m, genie_print_matrix);
// Vector and matrix monadic.
  m = a68_proc (M_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_vector_minus);
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "CV", m, genie_vector_col);
  a68_op (A68_EXT, "RV", m, genie_vector_row);
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_matrix_minus);
  a68_op (A68_EXT, "T", m, genie_matrix_transpose);
  a68_op (A68_EXT, "INV", m, genie_matrix_inv);
  a68_op (A68_EXT, "PINV", m, genie_matrix_pinv);
  a68_op (A68_EXT, "MEAN", m, genie_matrix_column_mean);
  m = a68_proc (M_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "DET", m, genie_matrix_det);
  a68_op (A68_EXT, "TRACE", m, genie_matrix_trace);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_vector_complex_minus);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "+", m, genie_idle);
  a68_op (A68_EXT, "-", m, genie_matrix_complex_minus);
  a68_op (A68_EXT, "T", m, genie_matrix_complex_transpose);
  a68_op (A68_EXT, "INV", m, genie_matrix_complex_inv);
  m = a68_proc (M_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "DET", m, genie_matrix_complex_det);
  a68_op (A68_EXT, "TRACE", m, genie_matrix_complex_trace);
// Vector and matrix dyadic.
  m = a68_proc (M_BOOL, M_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "=", m, genie_vector_eq);
  a68_op (A68_EXT, "/=", m, genie_vector_ne);
  m = a68_proc (M_ROW_REAL, M_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "+", m, genie_vector_add);
  a68_op (A68_EXT, "-", m, genie_vector_sub);
  m = a68_proc (M_REF_ROW_REAL, M_REF_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "+:=", m, genie_vector_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_vector_plusab);
  a68_op (A68_EXT, "-:=", m, genie_vector_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_vector_minusab);
  m = a68_proc (M_BOOL, M_ROW_ROW_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "=", m, genie_matrix_eq);
  a68_op (A68_EXT, "/-", m, genie_matrix_ne);
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "+", m, genie_matrix_add);
  a68_op (A68_EXT, "-", m, genie_matrix_sub);
  a68_op (A68_EXT, "BEFORE", m, genie_matrix_hcat);
  a68_op (A68_EXT, "ABOVE", m, genie_matrix_vcat);
  a68_prio ("BEFORE", 3);
  a68_prio ("ABOVE", 3);
  m = a68_proc (M_REF_ROW_ROW_REAL, M_REF_ROW_ROW_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "+:=", m, genie_matrix_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_matrix_plusab);
  a68_op (A68_EXT, "-:=", m, genie_matrix_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_matrix_minusab);
  m = a68_proc (M_BOOL, M_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "=", m, genie_vector_complex_eq);
  a68_op (A68_EXT, "/=", m, genie_vector_complex_ne);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "+", m, genie_vector_complex_add);
  a68_op (A68_EXT, "-", m, genie_vector_complex_sub);
  m = a68_proc (M_REF_ROW_COMPLEX, M_REF_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "+:=", m, genie_vector_complex_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_vector_complex_plusab);
  a68_op (A68_EXT, "-:=", m, genie_vector_complex_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_vector_complex_minusab);
  m = a68_proc (M_BOOL, M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "=", m, genie_matrix_complex_eq);
  a68_op (A68_EXT, "/=", m, genie_matrix_complex_ne);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "+", m, genie_matrix_complex_add);
  a68_op (A68_EXT, "-", m, genie_matrix_complex_sub);
  m = a68_proc (M_REF_ROW_ROW_COMPLEX, M_REF_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "+:=", m, genie_matrix_complex_plusab);
  a68_op (A68_EXT, "PLUSAB", m, genie_matrix_complex_plusab);
  a68_op (A68_EXT, "-:=", m, genie_matrix_complex_minusab);
  a68_op (A68_EXT, "MINUSAB", m, genie_matrix_complex_minusab);
// Vector and matrix scaling.
  m = a68_proc (M_ROW_REAL, M_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_real_scale_vector);
  m = a68_proc (M_ROW_REAL, M_ROW_REAL, M_REAL, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_scale_real);
  a68_op (A68_EXT, "/", m, genie_vector_div_real);
  m = a68_proc (M_ROW_ROW_REAL, M_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_real_scale_matrix);
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_REAL, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_scale_real);
  a68_op (A68_EXT, "/", m, genie_matrix_div_real);
  m = a68_proc (M_ROW_COMPLEX, M_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_complex_scale_vector_complex);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_complex_scale_complex);
  a68_op (A68_EXT, "/", m, genie_vector_complex_div_complex);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_complex_scale_matrix_complex);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_scale_complex);
  a68_op (A68_EXT, "/", m, genie_matrix_complex_div_complex);
  m = a68_proc (M_REF_ROW_REAL, M_REF_ROW_REAL, M_REAL, NO_MOID);
  a68_op (A68_EXT, "*:=", m, genie_vector_scale_real_ab);
  a68_op (A68_EXT, "/:=", m, genie_vector_div_real_ab);
  m = a68_proc (M_REF_ROW_ROW_REAL, M_REF_ROW_ROW_REAL, M_REAL, NO_MOID);
  a68_op (A68_EXT, "*:=", m, genie_matrix_scale_real_ab);
  a68_op (A68_EXT, "/:=", m, genie_matrix_div_real_ab);
  m = a68_proc (M_REF_ROW_COMPLEX, M_REF_ROW_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*:=", m, genie_vector_complex_scale_complex_ab);
  a68_op (A68_EXT, "/:=", m, genie_vector_complex_div_complex_ab);
  m = a68_proc (M_REF_ROW_ROW_COMPLEX, M_REF_ROW_ROW_COMPLEX, M_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*:=", m, genie_matrix_complex_scale_complex_ab);
  a68_op (A68_EXT, "/:=", m, genie_matrix_complex_div_complex_ab);
  m = a68_proc (M_ROW_REAL, M_ROW_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_times_matrix);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_complex_times_matrix);
// Matrix times vector or matrix.
  m = a68_proc (M_ROW_REAL, M_ROW_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_times_vector);
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_times_matrix);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_times_vector);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_matrix_complex_times_matrix);
// Vector and matrix miscellaneous.
  m = a68_proc (M_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "vectorecho", m, genie_vector_echo);
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "matrixecho", m, genie_matrix_echo);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_idf (A68_EXT, "complvectorecho", m, genie_vector_complex_echo);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, NO_MOID);
  a68_idf (A68_EXT, "complmatrixecho", m, genie_matrix_complex_echo);
  m = a68_proc (M_REAL, M_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_dot);
  m = a68_proc (M_COMPLEX, M_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "*", m, genie_vector_complex_dot);
  m = a68_proc (M_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "NORM", m, genie_vector_norm);
  m = a68_proc (M_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "NORM", m, genie_matrix_norm);
  m = a68_proc (M_REAL, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "NORM", m, genie_vector_complex_norm);
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_op (A68_EXT, "DYAD", m, genie_vector_dyad);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_op (A68_EXT, "DYAD", m, genie_vector_complex_dyad);
  a68_prio ("DYAD", 3);
// Principle Component Analysis.
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_REF_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "pcacv", m, genie_matrix_pca_cv);
  a68_idf (A68_EXT, "pcasvd", m, genie_matrix_pca_svd);
// Total Least Square regression.
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_REF_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "ols", m, genie_matrix_ols);
  a68_idf (A68_EXT, "tls", m, genie_matrix_tls);
// Partial Least Squares regression.
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_REF_ROW_REAL, M_INT, M_REAL, NO_MOID);
  a68_idf (A68_EXT, "pcr", m, genie_matrix_pcr);
  a68_idf (A68_EXT, "pls1", m, genie_matrix_pls1);
  a68_idf (A68_EXT, "pls2", m, genie_matrix_pls2);
// Routine left columns, a GSL alternative to trimming columns.
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_INT, NO_MOID);
  a68_idf (A68_EXT, "leftcolumns", m, genie_left_columns);
// Moore-Penrose pseudo inverse.
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_REAL, NO_MOID);
  a68_idf (A68_EXT, "pseudoinv", m, genie_matrix_pinv_lim);
// LU decomposition.
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_REF_ROW_INT, M_REF_INT, NO_MOID);
  a68_idf (A68_EXT, "ludecomp", m, genie_matrix_lu);
  m = a68_proc (M_REAL, M_ROW_ROW_REAL, M_INT, NO_MOID);
  a68_idf (A68_EXT, "ludet", m, genie_matrix_lu_det);
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_ROW_INT, NO_MOID);
  a68_idf (A68_EXT, "luinv", m, genie_matrix_lu_inv);
  m = a68_proc (M_ROW_REAL, M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_ROW_INT, M_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "lusolve", m, genie_matrix_lu_solve);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, M_REF_ROW_INT, M_REF_INT, NO_MOID);
  a68_idf (A68_EXT, "complexludecomp", m, genie_matrix_complex_lu);
  m = a68_proc (M_COMPLEX, M_ROW_ROW_COMPLEX, M_INT, NO_MOID);
  a68_idf (A68_EXT, "complexludet", m, genie_matrix_complex_lu_det);
  m = a68_proc (M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, M_ROW_INT, NO_MOID);
  a68_idf (A68_EXT, "complexluinv", m, genie_matrix_complex_lu_inv);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_ROW_COMPLEX, M_ROW_ROW_COMPLEX, M_ROW_INT, M_ROW_COMPLEX, NO_MOID);
  a68_idf (A68_EXT, "complexlusolve", m, genie_matrix_complex_lu_solve);
// SVD decomposition.
  m = a68_proc (M_VOID, M_ROW_ROW_REAL, M_REF_ROW_ROW_REAL, M_REF_ROW_REAL, M_REF_ROW_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "svddecomp", m, genie_matrix_svd);
  m = a68_proc (M_ROW_REAL, M_ROW_ROW_REAL, M_ROW_REAL, M_ROW_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "svdsolve", m, genie_matrix_svd_solve);
// QR decomposition.
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, M_REF_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "qrdecomp", m, genie_matrix_qr);
  m = a68_proc (M_ROW_REAL, M_ROW_ROW_REAL, M_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "qrsolve", m, genie_matrix_qr_solve);
  a68_idf (A68_EXT, "qrlssolve", m, genie_matrix_qr_ls_solve);
// Cholesky decomposition.
  m = a68_proc (M_ROW_ROW_REAL, M_ROW_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "choleskydecomp", m, genie_matrix_ch);
  m = a68_proc (M_ROW_REAL, M_ROW_ROW_REAL, M_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "choleskysolve", m, genie_matrix_ch_solve);
}

void stand_gsl_constants (void)
{
// Constants ex GSL.
  a68_idf (A68_EXT, "cgsspeedoflight", M_REAL, genie_cgs_speed_of_light);
  a68_idf (A68_EXT, "cgsgravitationalconstant", M_REAL, genie_cgs_gravitational_constant);
  a68_idf (A68_EXT, "cgsplanckconstant", M_REAL, genie_cgs_planck_constant_h);
  a68_idf (A68_EXT, "cgsplanckconstantbar", M_REAL, genie_cgs_planck_constant_hbar);
  a68_idf (A68_EXT, "cgsastronomicalunit", M_REAL, genie_cgs_astronomical_unit);
  a68_idf (A68_EXT, "cgslightyear", M_REAL, genie_cgs_light_year);
  a68_idf (A68_EXT, "cgsparsec", M_REAL, genie_cgs_parsec);
  a68_idf (A68_EXT, "cgsgravaccel", M_REAL, genie_cgs_grav_accel);
  a68_idf (A68_EXT, "cgselectronvolt", M_REAL, genie_cgs_electron_volt);
  a68_idf (A68_EXT, "cgsmasselectron", M_REAL, genie_cgs_mass_electron);
  a68_idf (A68_EXT, "cgsmassmuon", M_REAL, genie_cgs_mass_muon);
  a68_idf (A68_EXT, "cgsmassproton", M_REAL, genie_cgs_mass_proton);
  a68_idf (A68_EXT, "cgsmassneutron", M_REAL, genie_cgs_mass_neutron);
  a68_idf (A68_EXT, "cgsrydberg", M_REAL, genie_cgs_rydberg);
  a68_idf (A68_EXT, "cgsboltzmann", M_REAL, genie_cgs_boltzmann);
  a68_idf (A68_EXT, "cgsbohrmagneton", M_REAL, genie_cgs_bohr_magneton);
  a68_idf (A68_EXT, "cgsnuclearmagneton", M_REAL, genie_cgs_nuclear_magneton);
  a68_idf (A68_EXT, "cgselectronmagneticmoment", M_REAL, genie_cgs_electron_magnetic_moment);
  a68_idf (A68_EXT, "cgsprotonmagneticmoment", M_REAL, genie_cgs_proton_magnetic_moment);
  a68_idf (A68_EXT, "cgsmolargas", M_REAL, genie_cgs_molar_gas);
  a68_idf (A68_EXT, "cgsstandardgasvolume", M_REAL, genie_cgs_standard_gas_volume);
  a68_idf (A68_EXT, "cgsminute", M_REAL, genie_cgs_minute);
  a68_idf (A68_EXT, "cgshour", M_REAL, genie_cgs_hour);
  a68_idf (A68_EXT, "cgsday", M_REAL, genie_cgs_day);
  a68_idf (A68_EXT, "cgsweek", M_REAL, genie_cgs_week);
  a68_idf (A68_EXT, "cgsinch", M_REAL, genie_cgs_inch);
  a68_idf (A68_EXT, "cgsfoot", M_REAL, genie_cgs_foot);
  a68_idf (A68_EXT, "cgsyard", M_REAL, genie_cgs_yard);
  a68_idf (A68_EXT, "cgsmile", M_REAL, genie_cgs_mile);
  a68_idf (A68_EXT, "cgsnauticalmile", M_REAL, genie_cgs_nautical_mile);
  a68_idf (A68_EXT, "cgsfathom", M_REAL, genie_cgs_fathom);
  a68_idf (A68_EXT, "cgsmil", M_REAL, genie_cgs_mil);
  a68_idf (A68_EXT, "cgspoint", M_REAL, genie_cgs_point);
  a68_idf (A68_EXT, "cgstexpoint", M_REAL, genie_cgs_texpoint);
  a68_idf (A68_EXT, "cgsmicron", M_REAL, genie_cgs_micron);
  a68_idf (A68_EXT, "cgsangstrom", M_REAL, genie_cgs_angstrom);
  a68_idf (A68_EXT, "cgshectare", M_REAL, genie_cgs_hectare);
  a68_idf (A68_EXT, "cgsacre", M_REAL, genie_cgs_acre);
  a68_idf (A68_EXT, "cgsbarn", M_REAL, genie_cgs_barn);
  a68_idf (A68_EXT, "cgsliter", M_REAL, genie_cgs_liter);
  a68_idf (A68_EXT, "cgsusgallon", M_REAL, genie_cgs_us_gallon);
  a68_idf (A68_EXT, "cgsquart", M_REAL, genie_cgs_quart);
  a68_idf (A68_EXT, "cgspint", M_REAL, genie_cgs_pint);
  a68_idf (A68_EXT, "cgscup", M_REAL, genie_cgs_cup);
  a68_idf (A68_EXT, "cgsfluidounce", M_REAL, genie_cgs_fluid_ounce);
  a68_idf (A68_EXT, "cgstablespoon", M_REAL, genie_cgs_tablespoon);
  a68_idf (A68_EXT, "cgsteaspoon", M_REAL, genie_cgs_teaspoon);
  a68_idf (A68_EXT, "cgscanadiangallon", M_REAL, genie_cgs_canadian_gallon);
  a68_idf (A68_EXT, "cgsukgallon", M_REAL, genie_cgs_uk_gallon);
  a68_idf (A68_EXT, "cgsmilesperhour", M_REAL, genie_cgs_miles_per_hour);
  a68_idf (A68_EXT, "cgskilometersperhour", M_REAL, genie_cgs_kilometers_per_hour);
  a68_idf (A68_EXT, "cgsknot", M_REAL, genie_cgs_knot);
  a68_idf (A68_EXT, "cgspoundmass", M_REAL, genie_cgs_pound_mass);
  a68_idf (A68_EXT, "cgsouncemass", M_REAL, genie_cgs_ounce_mass);
  a68_idf (A68_EXT, "cgston", M_REAL, genie_cgs_ton);
  a68_idf (A68_EXT, "cgsmetricton", M_REAL, genie_cgs_metric_ton);
  a68_idf (A68_EXT, "cgsukton", M_REAL, genie_cgs_uk_ton);
  a68_idf (A68_EXT, "cgstroyounce", M_REAL, genie_cgs_troy_ounce);
  a68_idf (A68_EXT, "cgscarat", M_REAL, genie_cgs_carat);
  a68_idf (A68_EXT, "cgsunifiedatomicmass", M_REAL, genie_cgs_unified_atomic_mass);
  a68_idf (A68_EXT, "cgsgramforce", M_REAL, genie_cgs_gram_force);
  a68_idf (A68_EXT, "cgspoundforce", M_REAL, genie_cgs_pound_force);
  a68_idf (A68_EXT, "cgskilopoundforce", M_REAL, genie_cgs_kilopound_force);
  a68_idf (A68_EXT, "cgspoundal", M_REAL, genie_cgs_poundal);
  a68_idf (A68_EXT, "cgscalorie", M_REAL, genie_cgs_calorie);
  a68_idf (A68_EXT, "cgsbtu", M_REAL, genie_cgs_btu);
  a68_idf (A68_EXT, "cgstherm", M_REAL, genie_cgs_therm);
  a68_idf (A68_EXT, "cgshorsepower", M_REAL, genie_cgs_horsepower);
  a68_idf (A68_EXT, "cgsbar", M_REAL, genie_cgs_bar);
  a68_idf (A68_EXT, "cgsstdatmosphere", M_REAL, genie_cgs_std_atmosphere);
  a68_idf (A68_EXT, "cgstorr", M_REAL, genie_cgs_torr);
  a68_idf (A68_EXT, "cgsmeterofmercury", M_REAL, genie_cgs_meter_of_mercury);
  a68_idf (A68_EXT, "cgsinchofmercury", M_REAL, genie_cgs_inch_of_mercury);
  a68_idf (A68_EXT, "cgsinchofwater", M_REAL, genie_cgs_inch_of_water);
  a68_idf (A68_EXT, "cgspsi", M_REAL, genie_cgs_psi);
  a68_idf (A68_EXT, "cgspoise", M_REAL, genie_cgs_poise);
  a68_idf (A68_EXT, "cgsstokes", M_REAL, genie_cgs_stokes);
  a68_idf (A68_EXT, "cgsfaraday", M_REAL, genie_cgs_faraday);
  a68_idf (A68_EXT, "cgselectroncharge", M_REAL, genie_cgs_electron_charge);
  a68_idf (A68_EXT, "cgsgauss", M_REAL, genie_cgs_gauss);
  a68_idf (A68_EXT, "cgsstilb", M_REAL, genie_cgs_stilb);
  a68_idf (A68_EXT, "cgslumen", M_REAL, genie_cgs_lumen);
  a68_idf (A68_EXT, "cgslux", M_REAL, genie_cgs_lux);
  a68_idf (A68_EXT, "cgsphot", M_REAL, genie_cgs_phot);
  a68_idf (A68_EXT, "cgsfootcandle", M_REAL, genie_cgs_footcandle);
  a68_idf (A68_EXT, "cgslambert", M_REAL, genie_cgs_lambert);
  a68_idf (A68_EXT, "cgsfootlambert", M_REAL, genie_cgs_footlambert);
  a68_idf (A68_EXT, "cgscurie", M_REAL, genie_cgs_curie);
  a68_idf (A68_EXT, "cgsroentgen", M_REAL, genie_cgs_roentgen);
  a68_idf (A68_EXT, "cgsrad", M_REAL, genie_cgs_rad);
  a68_idf (A68_EXT, "cgssolarmass", M_REAL, genie_cgs_solar_mass);
  a68_idf (A68_EXT, "cgsbohrradius", M_REAL, genie_cgs_bohr_radius);
  a68_idf (A68_EXT, "cgsnewton", M_REAL, genie_cgs_newton);
  a68_idf (A68_EXT, "cgsdyne", M_REAL, genie_cgs_dyne);
  a68_idf (A68_EXT, "cgsjoule", M_REAL, genie_cgs_joule);
  a68_idf (A68_EXT, "cgserg", M_REAL, genie_cgs_erg);
  a68_idf (A68_EXT, "mksaspeedoflight", M_REAL, genie_mks_speed_of_light);
  a68_idf (A68_EXT, "mksagravitationalconstant", M_REAL, genie_mks_gravitational_constant);
  a68_idf (A68_EXT, "mksaplanckconstant", M_REAL, genie_mks_planck_constant_h);
  a68_idf (A68_EXT, "mksaplanckconstantbar", M_REAL, genie_mks_planck_constant_hbar);
  a68_idf (A68_EXT, "mksavacuumpermeability", M_REAL, genie_mks_vacuum_permeability);
  a68_idf (A68_EXT, "mksaastronomicalunit", M_REAL, genie_mks_astronomical_unit);
  a68_idf (A68_EXT, "mksalightyear", M_REAL, genie_mks_light_year);
  a68_idf (A68_EXT, "mksaparsec", M_REAL, genie_mks_parsec);
  a68_idf (A68_EXT, "mksagravaccel", M_REAL, genie_mks_grav_accel);
  a68_idf (A68_EXT, "mksaelectronvolt", M_REAL, genie_mks_electron_volt);
  a68_idf (A68_EXT, "mksamasselectron", M_REAL, genie_mks_mass_electron);
  a68_idf (A68_EXT, "mksamassmuon", M_REAL, genie_mks_mass_muon);
  a68_idf (A68_EXT, "mksamassproton", M_REAL, genie_mks_mass_proton);
  a68_idf (A68_EXT, "mksamassneutron", M_REAL, genie_mks_mass_neutron);
  a68_idf (A68_EXT, "mksarydberg", M_REAL, genie_mks_rydberg);
  a68_idf (A68_EXT, "mksaboltzmann", M_REAL, genie_mks_boltzmann);
  a68_idf (A68_EXT, "mksabohrmagneton", M_REAL, genie_mks_bohr_magneton);
  a68_idf (A68_EXT, "mksanuclearmagneton", M_REAL, genie_mks_nuclear_magneton);
  a68_idf (A68_EXT, "mksaelectronmagneticmoment", M_REAL, genie_mks_electron_magnetic_moment);
  a68_idf (A68_EXT, "mksaprotonmagneticmoment", M_REAL, genie_mks_proton_magnetic_moment);
  a68_idf (A68_EXT, "mksamolargas", M_REAL, genie_mks_molar_gas);
  a68_idf (A68_EXT, "mksastandardgasvolume", M_REAL, genie_mks_standard_gas_volume);
  a68_idf (A68_EXT, "mksaminute", M_REAL, genie_mks_minute);
  a68_idf (A68_EXT, "mksahour", M_REAL, genie_mks_hour);
  a68_idf (A68_EXT, "mksaday", M_REAL, genie_mks_day);
  a68_idf (A68_EXT, "mksaweek", M_REAL, genie_mks_week);
  a68_idf (A68_EXT, "mksainch", M_REAL, genie_mks_inch);
  a68_idf (A68_EXT, "mksafoot", M_REAL, genie_mks_foot);
  a68_idf (A68_EXT, "mksayard", M_REAL, genie_mks_yard);
  a68_idf (A68_EXT, "mksamile", M_REAL, genie_mks_mile);
  a68_idf (A68_EXT, "mksanauticalmile", M_REAL, genie_mks_nautical_mile);
  a68_idf (A68_EXT, "mksafathom", M_REAL, genie_mks_fathom);
  a68_idf (A68_EXT, "mksamil", M_REAL, genie_mks_mil);
  a68_idf (A68_EXT, "mksapoint", M_REAL, genie_mks_point);
  a68_idf (A68_EXT, "mksatexpoint", M_REAL, genie_mks_texpoint);
  a68_idf (A68_EXT, "mksamicron", M_REAL, genie_mks_micron);
  a68_idf (A68_EXT, "mksaangstrom", M_REAL, genie_mks_angstrom);
  a68_idf (A68_EXT, "mksahectare", M_REAL, genie_mks_hectare);
  a68_idf (A68_EXT, "mksaacre", M_REAL, genie_mks_acre);
  a68_idf (A68_EXT, "mksabarn", M_REAL, genie_mks_barn);
  a68_idf (A68_EXT, "mksaliter", M_REAL, genie_mks_liter);
  a68_idf (A68_EXT, "mksausgallon", M_REAL, genie_mks_us_gallon);
  a68_idf (A68_EXT, "mksaquart", M_REAL, genie_mks_quart);
  a68_idf (A68_EXT, "mksapint", M_REAL, genie_mks_pint);
  a68_idf (A68_EXT, "mksacup", M_REAL, genie_mks_cup);
  a68_idf (A68_EXT, "mksafluidounce", M_REAL, genie_mks_fluid_ounce);
  a68_idf (A68_EXT, "mksatablespoon", M_REAL, genie_mks_tablespoon);
  a68_idf (A68_EXT, "mksateaspoon", M_REAL, genie_mks_teaspoon);
  a68_idf (A68_EXT, "mksacanadiangallon", M_REAL, genie_mks_canadian_gallon);
  a68_idf (A68_EXT, "mksaukgallon", M_REAL, genie_mks_uk_gallon);
  a68_idf (A68_EXT, "mksamilesperhour", M_REAL, genie_mks_miles_per_hour);
  a68_idf (A68_EXT, "mksakilometersperhour", M_REAL, genie_mks_kilometers_per_hour);
  a68_idf (A68_EXT, "mksaknot", M_REAL, genie_mks_knot);
  a68_idf (A68_EXT, "mksapoundmass", M_REAL, genie_mks_pound_mass);
  a68_idf (A68_EXT, "mksaouncemass", M_REAL, genie_mks_ounce_mass);
  a68_idf (A68_EXT, "mksaton", M_REAL, genie_mks_ton);
  a68_idf (A68_EXT, "mksametricton", M_REAL, genie_mks_metric_ton);
  a68_idf (A68_EXT, "mksaukton", M_REAL, genie_mks_uk_ton);
  a68_idf (A68_EXT, "mksatroyounce", M_REAL, genie_mks_troy_ounce);
  a68_idf (A68_EXT, "mksacarat", M_REAL, genie_mks_carat);
  a68_idf (A68_EXT, "mksaunifiedatomicmass", M_REAL, genie_mks_unified_atomic_mass);
  a68_idf (A68_EXT, "mksagramforce", M_REAL, genie_mks_gram_force);
  a68_idf (A68_EXT, "mksapoundforce", M_REAL, genie_mks_pound_force);
  a68_idf (A68_EXT, "mksakilopoundforce", M_REAL, genie_mks_kilopound_force);
  a68_idf (A68_EXT, "mksapoundal", M_REAL, genie_mks_poundal);
  a68_idf (A68_EXT, "mksacalorie", M_REAL, genie_mks_calorie);
  a68_idf (A68_EXT, "mksabtu", M_REAL, genie_mks_btu);
  a68_idf (A68_EXT, "mksatherm", M_REAL, genie_mks_therm);
  a68_idf (A68_EXT, "mksahorsepower", M_REAL, genie_mks_horsepower);
  a68_idf (A68_EXT, "mksabar", M_REAL, genie_mks_bar);
  a68_idf (A68_EXT, "mksastdatmosphere", M_REAL, genie_mks_std_atmosphere);
  a68_idf (A68_EXT, "mksatorr", M_REAL, genie_mks_torr);
  a68_idf (A68_EXT, "mksameterofmercury", M_REAL, genie_mks_meter_of_mercury);
  a68_idf (A68_EXT, "mksainchofmercury", M_REAL, genie_mks_inch_of_mercury);
  a68_idf (A68_EXT, "mksainchofwater", M_REAL, genie_mks_inch_of_water);
  a68_idf (A68_EXT, "mksapsi", M_REAL, genie_mks_psi);
  a68_idf (A68_EXT, "mksapoise", M_REAL, genie_mks_poise);
  a68_idf (A68_EXT, "mksastokes", M_REAL, genie_mks_stokes);
  a68_idf (A68_EXT, "mksafaraday", M_REAL, genie_mks_faraday);
  a68_idf (A68_EXT, "mksaelectroncharge", M_REAL, genie_mks_electron_charge);
  a68_idf (A68_EXT, "mksagauss", M_REAL, genie_mks_gauss);
  a68_idf (A68_EXT, "mksastilb", M_REAL, genie_mks_stilb);
  a68_idf (A68_EXT, "mksalumen", M_REAL, genie_mks_lumen);
  a68_idf (A68_EXT, "mksalux", M_REAL, genie_mks_lux);
  a68_idf (A68_EXT, "mksaphot", M_REAL, genie_mks_phot);
  a68_idf (A68_EXT, "mksafootcandle", M_REAL, genie_mks_footcandle);
  a68_idf (A68_EXT, "mksalambert", M_REAL, genie_mks_lambert);
  a68_idf (A68_EXT, "mksafootlambert", M_REAL, genie_mks_footlambert);
  a68_idf (A68_EXT, "mksacurie", M_REAL, genie_mks_curie);
  a68_idf (A68_EXT, "mksaroentgen", M_REAL, genie_mks_roentgen);
  a68_idf (A68_EXT, "mksarad", M_REAL, genie_mks_rad);
  a68_idf (A68_EXT, "mksasolarmass", M_REAL, genie_mks_solar_mass);
  a68_idf (A68_EXT, "mksabohrradius", M_REAL, genie_mks_bohr_radius);
  a68_idf (A68_EXT, "mksavacuumpermittivity", M_REAL, genie_mks_vacuum_permittivity);
  a68_idf (A68_EXT, "mksanewton", M_REAL, genie_mks_newton);
  a68_idf (A68_EXT, "mksadyne", M_REAL, genie_mks_dyne);
  a68_idf (A68_EXT, "mksajoule", M_REAL, genie_mks_joule);
  a68_idf (A68_EXT, "mksaerg", M_REAL, genie_mks_erg);
  a68_idf (A68_EXT, "numfinestructure", M_REAL, genie_num_fine_structure);
  a68_idf (A68_EXT, "numavogadro", M_REAL, genie_num_avogadro);
  a68_idf (A68_EXT, "numyotta", M_REAL, genie_num_yotta);
  a68_idf (A68_EXT, "numzetta", M_REAL, genie_num_zetta);
  a68_idf (A68_EXT, "numexa", M_REAL, genie_num_exa);
  a68_idf (A68_EXT, "numpeta", M_REAL, genie_num_peta);
  a68_idf (A68_EXT, "numtera", M_REAL, genie_num_tera);
  a68_idf (A68_EXT, "numgiga", M_REAL, genie_num_giga);
  a68_idf (A68_EXT, "nummega", M_REAL, genie_num_mega);
  a68_idf (A68_EXT, "numkilo", M_REAL, genie_num_kilo);
  a68_idf (A68_EXT, "nummilli", M_REAL, genie_num_milli);
  a68_idf (A68_EXT, "nummicro", M_REAL, genie_num_micro);
  a68_idf (A68_EXT, "numnano", M_REAL, genie_num_nano);
  a68_idf (A68_EXT, "numpico", M_REAL, genie_num_pico);
  a68_idf (A68_EXT, "numfemto", M_REAL, genie_num_femto);
  a68_idf (A68_EXT, "numatto", M_REAL, genie_num_atto);
  a68_idf (A68_EXT, "numzepto", M_REAL, genie_num_zepto);
  a68_idf (A68_EXT, "numyocto", M_REAL, genie_num_yocto);
}

void stand_gsl_fft_laplace (void)
{
  MOID_T *m;
// FFT.
  m = a68_proc (M_ROW_INT, M_INT, NO_MOID);
  a68_idf (A68_EXT, "primefactors", m, genie_prime_factors);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_COMPLEX, NO_MOID);
  a68_idf (A68_EXT, "fftcomplexforward", m, genie_fft_complex_forward);
  a68_idf (A68_EXT, "fftcomplexbackward", m, genie_fft_complex_backward);
  a68_idf (A68_EXT, "fftcomplexinverse", m, genie_fft_complex_inverse);
  m = a68_proc (M_ROW_COMPLEX, M_ROW_REAL, NO_MOID);
  a68_idf (A68_EXT, "fftforward", m, genie_fft_forward);
  m = a68_proc (M_ROW_REAL, M_ROW_COMPLEX, NO_MOID);
  a68_idf (A68_EXT, "fftbackward", m, genie_fft_backward);
  a68_idf (A68_EXT, "fftinverse", m, genie_fft_inverse);
// Laplace.
  m = a68_proc (M_REAL, A68_MCACHE (proc_real_real), M_REAL, M_REF_REAL, NO_MOID);
  a68_idf (A68_EXT, "laplace", m, genie_laplace);
}

void stand_gsl (void)
{
  stand_gsl_sf ();
  stand_gsl_linear_algebra ();
  stand_gsl_constants ();
  stand_gsl_fft_laplace ();
}

#endif
