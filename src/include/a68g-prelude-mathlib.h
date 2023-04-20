//! @file a68g-prelude-mathlib.h
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

#if !defined (__A68G_PRELUDE_MATHLIB_H__)
#define __A68G_PRELUDE_MATHLIB_H__

#if defined (HAVE_MATHLIB)

extern void stand_mathlib (void);
extern void GetRNGstate (void);
extern void PutRNGstate (void);

extern GPROC genie_R_digamma_real;
extern GPROC genie_R_trigamma_real;
extern GPROC genie_R_tetragamma_real;
extern GPROC genie_R_pentagamma_real;
extern GPROC genie_R_psigamma_real;
extern GPROC genie_R_ptukey_real;
extern GPROC genie_R_qtukey_real;
extern GPROC genie_R_dnorm_real;
extern GPROC genie_R_pnorm_real;
extern GPROC genie_R_qnorm_real;
extern GPROC genie_R_rnorm_real;
extern GPROC genie_R_rnorm_real;
extern GPROC genie_R_dbeta_real;
extern GPROC genie_R_pbeta_real;
extern GPROC genie_R_qbeta_real;
extern GPROC genie_R_rbeta_real;
extern GPROC genie_R_dnbeta_real;
extern GPROC genie_R_pnbeta_real;
extern GPROC genie_R_qnbeta_real;
extern GPROC genie_R_rnbeta_real;
extern GPROC genie_R_dbinom_real;
extern GPROC genie_R_pbinom_real;
extern GPROC genie_R_qbinom_real;
extern GPROC genie_R_rbinom_real;
extern GPROC genie_R_dcauchy_real;
extern GPROC genie_R_pcauchy_real;
extern GPROC genie_R_qcauchy_real;
extern GPROC genie_R_rcauchy_real;
extern GPROC genie_R_dchisq_real;
extern GPROC genie_R_pchisq_real;
extern GPROC genie_R_qchisq_real;
extern GPROC genie_R_rchisq_real;
extern GPROC genie_R_dnchisq_real;
extern GPROC genie_R_pnchisq_real;
extern GPROC genie_R_qnchisq_real;
extern GPROC genie_R_rnchisq_real;
extern GPROC genie_R_dexp_real;
extern GPROC genie_R_pexp_real;
extern GPROC genie_R_qexp_real;
extern GPROC genie_R_rexp_real;
extern GPROC genie_R_df_real;
extern GPROC genie_R_pf_real;
extern GPROC genie_R_qf_real;
extern GPROC genie_R_rf_real;
extern GPROC genie_R_dnf_real;
extern GPROC genie_R_pnf_real;
extern GPROC genie_R_qnf_real;
extern GPROC genie_R_rnf_real;
extern GPROC genie_R_dgamma_real;
extern GPROC genie_R_pgamma_real;
extern GPROC genie_R_qgamma_real;
extern GPROC genie_R_rgamma_real;
extern GPROC genie_R_dgeom_real;
extern GPROC genie_R_pgeom_real;
extern GPROC genie_R_qgeom_real;
extern GPROC genie_R_rgeom_real;
extern GPROC genie_R_dhyper_real;
extern GPROC genie_R_phyper_real;
extern GPROC genie_R_qhyper_real;
extern GPROC genie_R_rhyper_real;
extern GPROC genie_R_dlogis_real;
extern GPROC genie_R_plogis_real;
extern GPROC genie_R_qlogis_real;
extern GPROC genie_R_rlogis_real;
extern GPROC genie_R_dlnorm_real;
extern GPROC genie_R_plnorm_real;
extern GPROC genie_R_qlnorm_real;
extern GPROC genie_R_rlnorm_real;
extern GPROC genie_R_dnbinom_real;
extern GPROC genie_R_pnbinom_real;
extern GPROC genie_R_qnbinom_real;
extern GPROC genie_R_rnbinom_real;
extern GPROC genie_R_dpois_real;
extern GPROC genie_R_ppois_real;
extern GPROC genie_R_qpois_real;
extern GPROC genie_R_rpois_real;
extern GPROC genie_R_dt_real;
extern GPROC genie_R_pt_real;
extern GPROC genie_R_qt_real;
extern GPROC genie_R_rt_real;
extern GPROC genie_R_dnt_real;
extern GPROC genie_R_pnt_real;
extern GPROC genie_R_qnt_real;
extern GPROC genie_R_rnt_real;
extern GPROC genie_R_dunif_real;
extern GPROC genie_R_punif_real;
extern GPROC genie_R_qunif_real;
extern GPROC genie_R_runif_real;
extern GPROC genie_R_dweibull_real;
extern GPROC genie_R_pweibull_real;
extern GPROC genie_R_qweibull_real;
extern GPROC genie_R_rweibull_real;
extern GPROC genie_R_dwilcox_real;
extern GPROC genie_R_pwilcox_real;
extern GPROC genie_R_qwilcox_real;
extern GPROC genie_R_rwilcox_real;
extern GPROC genie_R_dsignrank_real;
extern GPROC genie_R_psignrank_real;
extern GPROC genie_R_qsignrank_real;
extern GPROC genie_R_rsignrank_real;

#endif

#endif
