//! @file single-mathlib.c
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

// This file contains bindings to the GNU R standalone mathematical library.

#if defined (HAVE_MATHLIB)

#include "a68g-genie.h"
#include "a68g-prelude.h"
#include <Rmath.h>

//! @brief PROC (REAL) REAL r digamma

void genie_R_digamma_real (NODE_T * p)
{
  C_FUNCTION (p, digamma);
}

//! @brief PROC (REAL) REAL r trigamma

void genie_R_trigamma_real (NODE_T * p)
{
  C_FUNCTION (p, trigamma);
}

//! @brief PROC (REAL) REAL r tetragamma

void genie_R_tetragamma_real (NODE_T * p)
{
  C_FUNCTION (p, tetragamma);
}

//! @brief PROC (REAL) REAL r pentagamma

void genie_R_pentagamma_real (NODE_T * p)
{
  C_FUNCTION (p, pentagamma);
}

//! @brief PROC (REAL, REAL) REAL r psigamma

void genie_R_psigamma_real (NODE_T * p)
{
  A68 (f_entry) = p;
  A68_REAL *x, *s;
  POP_OPERAND_ADDRESSES (p, x, s, A68_REAL);
  errno = 0;
  VALUE (s) = psigamma (VALUE (x), VALUE (s));
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

#define D_3(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_BOOL give_log;\
  A68_REAL a, b;\
  POP_OBJECT (p, &give_log, A68_BOOL);\
  POP_OBJECT (p, &b, A68_REAL);\
  POP_OBJECT (p, &a, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&a), VALUE (&b),\
                       (VALUE (&give_log) == A68_TRUE ? 1 : 0)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

#define D_4(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_BOOL give_log;\
  A68_REAL a, b, c;\
  POP_OBJECT (p, &give_log, A68_BOOL);\
  POP_OBJECT (p, &c, A68_REAL);\
  POP_OBJECT (p, &b, A68_REAL);\
  POP_OBJECT (p, &a, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&a), VALUE (&b), VALUE (&c),\
                       (VALUE (&give_log) == A68_TRUE ? 1 : 0)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

#define D_5(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_BOOL give_log;\
  A68_REAL a, b, c, d;\
  POP_OBJECT (p, &give_log, A68_BOOL);\
  POP_OBJECT (p, &d, A68_REAL);\
  POP_OBJECT (p, &c, A68_REAL);\
  POP_OBJECT (p, &b, A68_REAL);\
  POP_OBJECT (p, &a, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&a), VALUE (&b), VALUE (&c), VALUE (&d),\
                       (VALUE (&give_log) == A68_TRUE ? 1 : 0)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

#define PQ_4(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_BOOL lower_tail, log_p;\
  A68_REAL x, a;\
  POP_OBJECT (p, &log_p, A68_BOOL);\
  POP_OBJECT (p, &lower_tail, A68_BOOL);\
  POP_OBJECT (p, &a, A68_REAL);\
  POP_OBJECT (p, &x, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&x), VALUE (&a),\
              (VALUE (&lower_tail) == A68_TRUE ? 1 : 0),\
              (VALUE (&log_p) == A68_TRUE ? 1 : 0)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

#define PQ_5(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_BOOL lower_tail, log_p;\
  A68_REAL x, a, b;\
  POP_OBJECT (p, &log_p, A68_BOOL);\
  POP_OBJECT (p, &lower_tail, A68_BOOL);\
  POP_OBJECT (p, &b, A68_REAL);\
  POP_OBJECT (p, &a, A68_REAL);\
  POP_OBJECT (p, &x, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&x), VALUE (&a), VALUE (&b),\
              (VALUE (&lower_tail) == A68_TRUE ? 1 : 0),\
              (VALUE (&log_p) == A68_TRUE ? 1 : 0)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

#define PQ_6(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_BOOL lower_tail, log_p;\
  A68_REAL x, a, b, c;\
  POP_OBJECT (p, &log_p, A68_BOOL);\
  POP_OBJECT (p, &lower_tail, A68_BOOL);\
  POP_OBJECT (p, &c, A68_REAL);\
  POP_OBJECT (p, &b, A68_REAL);\
  POP_OBJECT (p, &a, A68_REAL);\
  POP_OBJECT (p, &x, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&x), VALUE (&a), VALUE (&b), VALUE (&c),\
              (VALUE (&lower_tail) == A68_TRUE ? 1 : 0),\
              (VALUE (&log_p) == A68_TRUE ? 1 : 0)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

#define R_1(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_REAL a;\
  POP_OBJECT (p, &a, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&a)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

#define R_2(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_REAL a, b;\
  POP_OBJECT (p, &b, A68_REAL);\
  POP_OBJECT (p, &a, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&a), VALUE (&b)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

#define R_3(a68_fun, R_fun)\
void a68_fun (NODE_T * p)\
{\
  A68 (f_entry) = p;\
  A68_REAL a, b, c;\
  POP_OBJECT (p, &c, A68_REAL);\
  POP_OBJECT (p, &b, A68_REAL);\
  POP_OBJECT (p, &a, A68_REAL);\
  errno = 0;\
  PUSH_VALUE (p, R_fun (VALUE (&a), VALUE (&b), VALUE (&c)), A68_REAL);\
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);\
}

//
// Distribution functions
//

// Chi squared

//! @brief PROC dchisq = (REAL x, df, BOOL give log) REAL
//! @brief PROC pchisq = (REAL x, df, BOOL lower tail, log p) REAL
//! @brief PROC qchisq = (REAL p, df, BOOL lower tail, log p) REAL
//! @brief PROC rchisq = (REAL df) REAL

D_3 (genie_R_dchisq_real, dchisq);
PQ_4 (genie_R_pchisq_real, pchisq);
PQ_4 (genie_R_qchisq_real, qchisq);
R_1 (genie_R_rchisq_real, rchisq);

// Exponential

//! @brief PROC dexp = (REAL x, scale, BOOL give log) REAL
//! @brief PROC pexp = (REAL x, scale, BOOL lower tail, log p) REAL
//! @brief PROC qexp = (REAL p, scale, BOOL lower tail, log p) REAL
//! @brief PROC rexp = (REAL scale) REAL

D_3 (genie_R_dexp_real, dexp);
PQ_4 (genie_R_pexp_real, pexp);
PQ_4 (genie_R_qexp_real, qexp);
R_1 (genie_R_rexp_real, rexp);

// Geometric

//! @brief PROC dgeom = (REAL x, p, BOOL give log) REAL
//! @brief PROC pgeom = (REAL x, p, BOOL lower tail, log p) REAL
//! @brief PROC qgeom = (REAL p, p, BOOL lower tail, log p) REAL
//! @brief PROC rgeom = (REAL p) REAL

D_3 (genie_R_dgeom_real, dgeom);
PQ_4 (genie_R_pgeom_real, pgeom);
PQ_4 (genie_R_qgeom_real, qgeom);
R_1 (genie_R_rgeom_real, rgeom);

// Poisson 

//! @brief PROC dpois = (REAL x, lambda, BOOL give log) REAL
//! @brief PROC ppois = (REAL x, lambda, BOOL lower tail, log p) REAL
//! @brief PROC qpois = (REAL p, lambda, BOOL lower tail, log p) REAL
//! @brief PROC rpois = (REAL lambda) REAL

D_3 (genie_R_dpois_real, dpois);
PQ_4 (genie_R_ppois_real, ppois);
PQ_4 (genie_R_qpois_real, qpois);
R_1 (genie_R_rpois_real, rpois);

// Student

//! @brief PROC dt = (REAL x, n, BOOL give log) REAL
//! @brief PROC pt = (REAL x, n, BOOL lower tail, log p) REAL
//! @brief PROC qt = (REAL p, n, BOOL lower tail, log p) REAL
//! @brief PROC rt = (REAL n) REAL

D_3 (genie_R_dt_real, dt);
PQ_4 (genie_R_pt_real, pt);
PQ_4 (genie_R_qt_real, qt);
R_1 (genie_R_rt_real, rt);

// Beta

//! @brief PROC dbeta = (REAL x, a, b, BOOL give log) REAL
//! @brief PROC pbeta = (REAL x, a, b, BOOL lower tail, log p) REAL
//! @brief PROC qbeta = (REAL p, a, b, BOOL lower tail, log p) REAL
//! @brief PROC rbeta = (REAL p, a, b) REAL

D_4 (genie_R_dbeta_real, dbeta);
PQ_5 (genie_R_pbeta_real, pbeta);
PQ_5 (genie_R_qbeta_real, qbeta);
R_2 (genie_R_rbeta_real, rbeta);

// Binomial

//! @brief PROC dbinom = (REAL x, n, p, BOOL give log) REAL
//! @brief PROC pbinom = (REAL x, n, p, BOOL lower tail, log p) REAL
//! @brief PROC qbinom = (REAL p, n, p, BOOL lower tail, log p) REAL
//! @brief PROC rbinom = (REAL p, n, p) REAL

D_4 (genie_R_dbinom_real, dbinom);
PQ_5 (genie_R_pbinom_real, pbinom);
PQ_5 (genie_R_qbinom_real, qbinom);
R_2 (genie_R_rbinom_real, rbinom);

// Chi squared, non central

//! @brief PROC dnchisq = (REAL x, df, ncp, BOOL give log) REAL
//! @brief PROC pnchisq = (REAL x, df, ncp, BOOL lower tail, log p) REAL
//! @brief PROC qnchisq = (REAL p, df, ncp, BOOL lower tail, log p) REAL
//! @brief PROC rnchisq = (REAL p, df, ncp) REAL

D_4 (genie_R_dnchisq_real, dnchisq);
PQ_5 (genie_R_pnchisq_real, pnchisq);
PQ_5 (genie_R_qnchisq_real, qnchisq);

// Cauchy

//! @brief PROC dcauchy = (REAL x, location, scale, BOOL give log) REAL
//! @brief PROC pcauchy = (REAL x, location, scale, BOOL lower tail, log p) REAL
//! @brief PROC qcauchy = (REAL p, location, scale, BOOL lower tail, log p) REAL
//! @brief PROC rcauchy = (REAL p, location, scale) REAL

D_4 (genie_R_dcauchy_real, dcauchy);
PQ_5 (genie_R_pcauchy_real, pcauchy);
PQ_5 (genie_R_qcauchy_real, qcauchy);
R_2 (genie_R_rcauchy_real, rcauchy);

// F

//! @brief PROC df = (REAL x, n1, n2, BOOL give log) REAL
//! @brief PROC pf = (REAL x, n1, n2, BOOL lower tail, log p) REAL
//! @brief PROC qf = (REAL p, n1, n2, BOOL lower tail, log p) REAL
//! @brief PROC rf = (REAL p, n1, n2) REAL

D_4 (genie_R_df_real, df);
PQ_5 (genie_R_pf_real, pf);
PQ_5 (genie_R_qf_real, qf);
R_2 (genie_R_rf_real, rf);

// Logistic

//! @brief PROC dlogis = (REAL x, location, scale, BOOL give log) REAL
//! @brief PROC plogis = (REAL x, location, scale, BOOL lower tail, log p) REAL
//! @brief PROC qlogis = (REAL p, location, scale, BOOL lower tail, log p) REAL
//! @brief PROC rlogis = (REAL p, location, scale) REAL

D_4 (genie_R_dlogis_real, dlogis);
PQ_5 (genie_R_plogis_real, plogis);
PQ_5 (genie_R_qlogis_real, qlogis);
R_2 (genie_R_rlogis_real, rlogis);

// Log-normal

//! @brief PROC dlnorm = (REAL x, logmu, logsd, BOOL give log) REAL
//! @brief PROC plnorm = (REAL x, logmu, logsd, BOOL lower tail, log p) REAL
//! @brief PROC qlnorm = (REAL p, logmu, logsd, BOOL lower tail, log p) REAL
//! @brief PROC rlnorm = (REAL p, logmu, logsd) REAL

D_4 (genie_R_dlnorm_real, dlnorm);
PQ_5 (genie_R_plnorm_real, plnorm);
PQ_5 (genie_R_qlnorm_real, qlnorm);
R_2 (genie_R_rlnorm_real, rlnorm);

// Negative binomial

//! @brief PROC dnbinom = (REAL x, size, prob, BOOL give log) REAL
//! @brief PROC pnbinom = (REAL x, size, prob, BOOL lower tail, log p) REAL
//! @brief PROC qnbinom = (REAL p, size, prob, BOOL lower tail, log p) REAL
//! @brief PROC rnbinom = (REAL p, size, prob) REAL

D_4 (genie_R_dnbinom_real, dnbinom);
PQ_5 (genie_R_pnbinom_real, pnbinom);
PQ_5 (genie_R_qnbinom_real, qnbinom);
R_2 (genie_R_rnbinom_real, rnbinom);

// t, non-central

//! @brief PROC dnt = (REAL x, df, delta, BOOL give log) REAL
//! @brief PROC pnt = (REAL x, df, delta, BOOL lower tail, log p) REAL
//! @brief PROC qnt = (REAL p, df, delta, BOOL lower tail, log p) REAL

D_4 (genie_R_dnt_real, dnt);
PQ_5 (genie_R_pnt_real, pnt);
PQ_5 (genie_R_qnt_real, qnt);

// Normal

//! @brief PROC dnorm = (REAL x, mu, sigma, BOOL give log) REAL
//! @brief PROC pnorm = (REAL x, mu, sigma, BOOL lower tail, log p) REAL
//! @brief PROC qnorm = (REAL p, mu, sigma, BOOL lower tail, log p) REAL
//! @brief PROC rnorm = (REAL p, mu, sigma) REAL

D_4 (genie_R_dnorm_real, dnorm);
PQ_5 (genie_R_pnorm_real, pnorm);
PQ_5 (genie_R_qnorm_real, qnorm);
R_2 (genie_R_rnorm_real, rnorm);

// Uniform

//! @brief PROC dunif = (REAL x, a, b, BOOL give log) REAL
//! @brief PROC punif = (REAL x, a, b, BOOL lower tail, log p) REAL
//! @brief PROC qunif = (REAL p, a, b, BOOL lower tail, log p) REAL
//! @brief PROC runif = (REAL p, a, b) REAL

D_4 (genie_R_dunif_real, dunif);
PQ_5 (genie_R_punif_real, punif);
PQ_5 (genie_R_qunif_real, qunif);
R_2 (genie_R_runif_real, runif);

// Weibull

//! @brief PROC dweibull = (REAL x, shape, scale, BOOL give log) REAL
//! @brief PROC pweibull = (REAL x, shape, scale, BOOL lower tail, log p) REAL
//! @brief PROC qweibull = (REAL p, shape, scale, BOOL lower tail, log p) REAL
//! @brief PROC rweibull = (REAL p, shape, scale) REAL

D_4 (genie_R_dweibull_real, dweibull);
PQ_5 (genie_R_pweibull_real, pweibull);
PQ_5 (genie_R_qweibull_real, qweibull);
R_2 (genie_R_rweibull_real, rweibull);

// F, non central

//! @brief PROC dnf = (REAL x, n1, n2, ncp, BOOL give log) REAL
//! @brief PROC pnf = (REAL x, n1, n2, ncp, BOOL lower tail, log p) REAL
//! @brief PROC qnf = (REAL p, n1, n2, ncp, BOOL lower tail, log p) REAL

D_5 (genie_R_dnf_real, dnf);
PQ_6 (genie_R_pnf_real, pnf);
PQ_6 (genie_R_qnf_real, qnf);

// Hyper geometric

//! @brief PROC dhyper = (REAL x, nr, nb, n, BOOL give log) REAL
//! @brief PROC phyper = (REAL x, nr, nb, n, BOOL lower tail, log p) REAL
//! @brief PROC qhyper = (REAL p, nr, nb, n, BOOL lower tail, log p) REAL
//! @brief PROC rhyper = (REAL x, nr, nb, n, BOOL give log) REAL

D_5 (genie_R_dhyper_real, dhyper);
PQ_6 (genie_R_phyper_real, phyper);
PQ_6 (genie_R_qhyper_real, qhyper);
R_3 (genie_R_rhyper_real, rhyper);

// Tukey

//! @brief PROC ptukey = (REAL x, groups, df, treatments, BOOL lower tail, log p) REAL
//! @brief PROC qtukey = (REAL p, groups, df, treatments, BOOL lower tail, log p) REAL

PQ_6 (genie_R_ptukey_real, ptukey);
PQ_6 (genie_R_qtukey_real, qtukey);

// Wilcoxon

//! @brief PROC dwilcox = (REAL x, m, n, BOOL give log) REAL
//! @brief PROC pwilcox = (REAL x, m, n, BOOL lower tail, log p) REAL
//! @brief PROC qwilcox = (REAL p, m, n, BOOL lower tail, log p) REAL
//! @brief PROC rwilcox = (REAL p, m, n) REAL

void genie_R_dwilcox (NODE_T * p)
{
  A68 (f_entry) = p;
  A68_BOOL give_log;
  A68_REAL a, b, c;
  extern void wilcox_free (void);
  POP_OBJECT (p, &give_log, A68_BOOL);
  POP_OBJECT (p, &c, A68_REAL);
  POP_OBJECT (p, &b, A68_REAL);
  POP_OBJECT (p, &a, A68_REAL);
  errno = 0;
  PUSH_VALUE (p, dwilcox (VALUE (&a), VALUE (&b), VALUE (&c), (VALUE (&give_log) == A68_TRUE ? 1 : 0)), A68_REAL);
  wilcox_free ();
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

void genie_R_pwilcox (NODE_T * p)
{
  A68 (f_entry) = p;
  A68_BOOL lower_tail, log_p;
  A68_REAL x, a, b;
  extern void wilcox_free (void);
  POP_OBJECT (p, &log_p, A68_BOOL);
  POP_OBJECT (p, &lower_tail, A68_BOOL);
  POP_OBJECT (p, &b, A68_REAL);
  POP_OBJECT (p, &a, A68_REAL);
  POP_OBJECT (p, &x, A68_REAL);
  errno = 0;
  PUSH_VALUE (p, pwilcox (VALUE (&x), VALUE (&a), VALUE (&b), (VALUE (&lower_tail) == A68_TRUE ? 1 : 0), (VALUE (&log_p) == A68_TRUE ? 1 : 0)), A68_REAL);
  wilcox_free ();
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

void genie_R_qwilcox (NODE_T * p)
{
  A68_BOOL lower_tail, log_p;
  A68_REAL x, a, b;
  extern void wilcox_free (void);
  POP_OBJECT (p, &log_p, A68_BOOL);
  POP_OBJECT (p, &lower_tail, A68_BOOL);
  POP_OBJECT (p, &b, A68_REAL);
  POP_OBJECT (p, &a, A68_REAL);
  POP_OBJECT (p, &x, A68_REAL);
  errno = 0;
  PUSH_VALUE (p, qwilcox (VALUE (&x), VALUE (&a), VALUE (&b), (VALUE (&lower_tail) == A68_TRUE ? 1 : 0), (VALUE (&log_p) == A68_TRUE ? 1 : 0)), A68_REAL);
  wilcox_free ();
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

R_2 (genie_R_rwilcox_real, rwilcox);

// Wilcoxon sign rank

//! @brief PROC dsignrank = (REAL x, n, BOOL give log) REAL
//! @brief PROC psignrank = (REAL x, n, BOOL lower tail, log p) REAL
//! @brief PROC qsignrank = (REAL p, n, BOOL lower tail, log p) REAL
//! @brief PROC rsignrank = (REAL p, n) REAL

void genie_R_dsignrank (NODE_T * p)
{
  A68_BOOL give_log;
  A68_REAL a, b;
  extern void signrank_free (void);
  POP_OBJECT (p, &give_log, A68_BOOL);
  POP_OBJECT (p, &b, A68_REAL);
  POP_OBJECT (p, &a, A68_REAL);
  errno = 0;
  PUSH_VALUE (p, dsignrank (VALUE (&a), VALUE (&b), (VALUE (&give_log) == A68_TRUE ? 1 : 0)), A68_REAL);
  signrank_free ();
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

void genie_R_psignrank (NODE_T * p)
{
  A68_BOOL lower_tail, log_p;
  A68_REAL x, a;
  extern void signrank_free (void);
  POP_OBJECT (p, &log_p, A68_BOOL);
  POP_OBJECT (p, &lower_tail, A68_BOOL);
  POP_OBJECT (p, &a, A68_REAL);
  POP_OBJECT (p, &x, A68_REAL);
  errno = 0;
  PUSH_VALUE (p, psignrank (VALUE (&x), VALUE (&a), (VALUE (&lower_tail) == A68_TRUE ? 1 : 0), (VALUE (&log_p) == A68_TRUE ? 1 : 0)), A68_REAL);
  signrank_free ();
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

void genie_R_qsignrank (NODE_T * p)
{
  A68_BOOL lower_tail, log_p;
  A68_REAL x, a;
  extern void signrank_free (void);
  POP_OBJECT (p, &log_p, A68_BOOL);
  POP_OBJECT (p, &lower_tail, A68_BOOL);
  POP_OBJECT (p, &a, A68_REAL);
  POP_OBJECT (p, &x, A68_REAL);
  errno = 0;
  PUSH_VALUE (p, qsignrank (VALUE (&x), VALUE (&a), (VALUE (&lower_tail) == A68_TRUE ? 1 : 0), (VALUE (&log_p) == A68_TRUE ? 1 : 0)), A68_REAL);
  signrank_free ();
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

R_1 (genie_R_rsignrank_real, rsignrank);

#endif
