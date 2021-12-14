//! @file prelude-mathlib.c
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

#include "a68g.h"
#include "a68g-optimiser.h"
#include "a68g-prelude.h"
#include "a68g-prelude-mathlib.h"
#include "a68g-transput.h"
#include "a68g-mp.h"
#include "a68g-parser.h"
#include "a68g-physics.h"
#include "a68g-double.h"

#if defined (HAVE_MATHLIB)

void stand_mathlib (void)
{
  MOID_T *m;
// R specific special functions
  a68_idf (A68_EXT, "rdigamma", A68_MCACHE (proc_real_real), genie_R_digamma_real);
  a68_idf (A68_EXT, "rtrigamma", A68_MCACHE (proc_real_real), genie_R_trigamma_real);
  a68_idf (A68_EXT, "rtetragamma", A68_MCACHE (proc_real_real), genie_R_tetragamma_real);
  a68_idf (A68_EXT, "rpentagamma", A68_MCACHE (proc_real_real), genie_R_pentagamma_real);
  a68_idf (A68_EXT, "rpsigamma", A68_MCACHE (proc_real_real_real), genie_R_psigamma_real);
// R distribution related functions
  m = A68_MCACHE (proc_real_real);
  a68_idf (A68_EXT, "rrt", m, genie_R_rt_real);
  a68_idf (A68_EXT, "rrchisq", m, genie_R_rchisq_real);
  a68_idf (A68_EXT, "rrexp", m, genie_R_rexp_real);
  a68_idf (A68_EXT, "rrgeom", m, genie_R_rgeom_real);
  a68_idf (A68_EXT, "rrpois", m, genie_R_rpois_real);
  a68_idf (A68_EXT, "rrsignrank", m, genie_R_rsignrank_real);
  m = A68_MCACHE (proc_real_real_real);
  a68_idf (A68_EXT, "rrbeta", m, genie_R_rbeta_real);
  a68_idf (A68_EXT, "rrbinom", m, genie_R_rbinom_real);
  a68_idf (A68_EXT, "rrcauchy", m, genie_R_rcauchy_real);
  a68_idf (A68_EXT, "rrf", m, genie_R_rf_real);
  a68_idf (A68_EXT, "rrlogis", m, genie_R_rlogis_real);
  a68_idf (A68_EXT, "rrlnorm", m, genie_R_rlnorm_real);
  a68_idf (A68_EXT, "rrnbinom", m, genie_R_rnbinom_real);
  a68_idf (A68_EXT, "rrnorm", m, genie_R_rnorm_real);
  a68_idf (A68_EXT, "rrunif", m, genie_R_runif_real);
  a68_idf (A68_EXT, "rrweibull", m, genie_R_rweibull_real);
  a68_idf (A68_EXT, "rrwilcox", m, genie_R_rwilcox_real);
  m = a68_proc (M_REAL, M_REAL, M_REAL, M_BOOL, NO_MOID);
  a68_idf (A68_EXT, "rdt", m, genie_R_dt_real);
  a68_idf (A68_EXT, "rdchisq", m, genie_R_dchisq_real);
  a68_idf (A68_EXT, "rdexp", m, genie_R_dexp_real);
  a68_idf (A68_EXT, "rdgeom", m, genie_R_dgeom_real);
  a68_idf (A68_EXT, "rdpois", m, genie_R_dpois_real);
  m = a68_proc (M_REAL, M_REAL, M_REAL, M_REAL, M_BOOL, NO_MOID);
  a68_idf (A68_EXT, "rdnorm", m, genie_R_dnorm_real);
  a68_idf (A68_EXT, "rdbeta", m, genie_R_dbeta_real);
  a68_idf (A68_EXT, "rdbinom", m, genie_R_dbinom_real);
  a68_idf (A68_EXT, "rdnchisq", m, genie_R_dnchisq_real);
  a68_idf (A68_EXT, "rdcauchy", m, genie_R_dcauchy_real);
  a68_idf (A68_EXT, "rdf", m, genie_R_df_real);
  a68_idf (A68_EXT, "rdlogis", m, genie_R_dlogis_real);
  a68_idf (A68_EXT, "rdlnorm", m, genie_R_dlnorm_real);
  a68_idf (A68_EXT, "rdnbinom", m, genie_R_dnbinom_real);
  a68_idf (A68_EXT, "rdnt", m, genie_R_dnt_real);
  a68_idf (A68_EXT, "rdunif", m, genie_R_dunif_real);
  a68_idf (A68_EXT, "rdweibull", m, genie_R_dweibull_real);
  m = a68_proc (M_REAL, M_REAL, M_REAL, M_REAL, M_REAL, M_BOOL, NO_MOID);
  a68_idf (A68_EXT, "rdnf", m, genie_R_dnf_real);
  a68_idf (A68_EXT, "rdhyper", m, genie_R_dhyper_real);
  m = a68_proc (M_REAL, M_REAL, M_REAL, M_BOOL, M_BOOL, NO_MOID);
  a68_idf (A68_EXT, "rpt", m, genie_R_pt_real);
  a68_idf (A68_EXT, "rqt", m, genie_R_qt_real);
  a68_idf (A68_EXT, "rpchisq", m, genie_R_pchisq_real);
  a68_idf (A68_EXT, "rqchisq", m, genie_R_qchisq_real);
  a68_idf (A68_EXT, "rpexp", m, genie_R_pexp_real);
  a68_idf (A68_EXT, "rqexp", m, genie_R_qexp_real);
  a68_idf (A68_EXT, "rpgeom", m, genie_R_pgeom_real);
  a68_idf (A68_EXT, "rqgeom", m, genie_R_qgeom_real);
  a68_idf (A68_EXT, "rppois", m, genie_R_ppois_real);
  a68_idf (A68_EXT, "rqpois", m, genie_R_qpois_real);
  m = a68_proc (M_REAL, M_REAL, M_REAL, M_REAL, M_BOOL, M_BOOL, NO_MOID);
  a68_idf (A68_EXT, "rpnorm", m, genie_R_pnorm_real);
  a68_idf (A68_EXT, "rqnorm", m, genie_R_qnorm_real);
  a68_idf (A68_EXT, "rpbeta", m, genie_R_pbeta_real);
  a68_idf (A68_EXT, "rqbeta", m, genie_R_qbeta_real);
  a68_idf (A68_EXT, "rpbinom", m, genie_R_pbinom_real);
  a68_idf (A68_EXT, "rqbinom", m, genie_R_qbinom_real);
  a68_idf (A68_EXT, "rpnchisq", m, genie_R_pnchisq_real);
  a68_idf (A68_EXT, "rqnchisq", m, genie_R_qnchisq_real);
  a68_idf (A68_EXT, "rpcauchy", m, genie_R_pcauchy_real);
  a68_idf (A68_EXT, "rqcauchy", m, genie_R_qcauchy_real);
  a68_idf (A68_EXT, "rpf", m, genie_R_pf_real);
  a68_idf (A68_EXT, "rqf", m, genie_R_qf_real);
  a68_idf (A68_EXT, "rplogis", m, genie_R_plogis_real);
  a68_idf (A68_EXT, "rqlogis", m, genie_R_qlogis_real);
  a68_idf (A68_EXT, "rplnorm", m, genie_R_plnorm_real);
  a68_idf (A68_EXT, "rqlnorm", m, genie_R_qlnorm_real);
  a68_idf (A68_EXT, "rpnbinom", m, genie_R_pnbinom_real);
  a68_idf (A68_EXT, "rqnbinom", m, genie_R_qnbinom_real);
  a68_idf (A68_EXT, "rpnt", m, genie_R_pnt_real);
  a68_idf (A68_EXT, "rqnt", m, genie_R_qnt_real);
  a68_idf (A68_EXT, "rpunif", m, genie_R_punif_real);
  a68_idf (A68_EXT, "rqunif", m, genie_R_qunif_real);
  a68_idf (A68_EXT, "rpweibull", m, genie_R_pweibull_real);
  a68_idf (A68_EXT, "rqweibull", m, genie_R_qweibull_real);
  m = a68_proc (M_REAL, M_REAL, M_REAL, M_REAL, M_REAL, M_BOOL, M_BOOL, NO_MOID);
  a68_idf (A68_EXT, "rptukey", m, genie_R_ptukey_real);
  a68_idf (A68_EXT, "rqtukey", m, genie_R_qtukey_real);
  a68_idf (A68_EXT, "rpnf", m, genie_R_pnf_real);
  a68_idf (A68_EXT, "rqnf", m, genie_R_qnf_real);
  a68_idf (A68_EXT, "rphyper", m, genie_R_phyper_real);
  a68_idf (A68_EXT, "rqhyper", m, genie_R_qhyper_real);
}

#endif
