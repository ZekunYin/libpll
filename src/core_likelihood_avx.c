/*
    Copyright (C) 2015 Tomas Flouri

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact: Tomas Flouri <Tomas.Flouri@h-its.org>,
    Exelixis Lab, Heidelberg Instutute for Theoretical Studies
    Schloss-Wolfsbrunnenweg 35, D-69118 Heidelberg, Germany
*/

#include "pll.h"

PLL_EXPORT
double pll_core_edge_loglikelihood_ti_4x4_avx(unsigned int sites,
                                              unsigned int rate_cats,
                                              const double * parent_clv,
                                              const unsigned int * parent_scaler,
                                              const unsigned char * tipchars,
                                              const double * pmatrix,
                                              double ** frequencies,
                                              const double * rate_weights,
                                              const unsigned int * pattern_weights,
                                              const double * invar_proportion,
                                              const int * invar_indices,
                                              const unsigned int * freqs_indices,
                                              double * persite_lnl)
{
  unsigned int n,i,m = 0;
  double logl = 0;
  double prop_invar = 0;

  const double * clvp = parent_clv;
  const double * pmat;
  const double * freqs = NULL;

  double terma, terma_r;
  double site_lk, inv_site_lk;

  unsigned int scale_factors;
  unsigned int cstate;
  unsigned int states = 4;
  unsigned int states_padded = 4;

  __m256d xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;
  __m256i mask;

  for (n = 0; n < sites; ++n)
  {
    pmat = pmatrix;
    terma = 0;

    cstate = tipchars[n];

    mask = _mm256_set_epi64x(
              ((cstate >> 3) & 1) ? ~0 : 0,
              ((cstate >> 2) & 1) ? ~0 : 0,
              ((cstate >> 1) & 1) ? ~0 : 0,
              ((cstate >> 0) & 1) ? ~0 : 0);

    for (i = 0; i < rate_cats; ++i)
    {
      freqs = frequencies[freqs_indices[i]];

      /* load frequencies for current rate matrix */
      xmm0 = _mm256_load_pd(freqs);

      /* load pmatrix row 1 and multiply with clvc */
      xmm3 = _mm256_maskload_pd(pmat,mask);

      /* load pmatrix row 2 and multiply with clvc */
      pmat += states;
      xmm4 = _mm256_maskload_pd(pmat,mask);

      /* load pmatrix row 3 and multiply with clvc */
      pmat += states;
      xmm5 = _mm256_maskload_pd(pmat,mask);

      /* load pmatrix row 4 and multiply with clvc */
      pmat += states;
      xmm6 = _mm256_maskload_pd(pmat,mask);

      /* point to the pmatrix for the next rate category */
      pmat += states;

      /* create a vector containing the sums of xmm3, xmm4, xmm5, xmm6 */
      xmm1 = _mm256_unpackhi_pd(xmm3,xmm4);
      xmm2 = _mm256_unpacklo_pd(xmm3,xmm4);

      xmm3 = _mm256_unpackhi_pd(xmm5,xmm6);
      xmm4 = _mm256_unpacklo_pd(xmm5,xmm6);

      xmm5 = _mm256_add_pd(xmm1,xmm2);
      xmm6 = _mm256_add_pd(xmm3,xmm4);

      xmm1 = _mm256_permute2f128_pd(xmm5,xmm6, _MM_SHUFFLE(0,2,0,1));
      xmm2 = _mm256_blend_pd(xmm5,xmm6,12);
      xmm3 = _mm256_add_pd(xmm1,xmm2);

      /* multiply with frequencies */
      xmm1 = _mm256_mul_pd(xmm0,xmm3);

      /* multiply with clvp */
      xmm2 = _mm256_load_pd(clvp);
      xmm0 = _mm256_mul_pd(xmm1,xmm2);

      /* add up the elements of xmm0 */
      xmm1 = _mm256_hadd_pd(xmm0,xmm0);
      terma_r = ((double *)&xmm1)[0] + ((double *)&xmm1)[2];

      /* account for invariant sites */
      prop_invar = invar_proportion ? invar_proportion[freqs_indices[i]] : 0;
      if (prop_invar > 0)
      {
        inv_site_lk = (invar_indices[n] == -1) ?
                          0 : freqs[invar_indices[n]];
        terma += rate_weights[i] * (terma_r * (1 - prop_invar) +
                 inv_site_lk * prop_invar);
      }
      else
      {
        terma += terma_r * rate_weights[i];
      }

      clvp += states_padded;
    }

    /* count number of scaling factors to acount for */
    scale_factors = (parent_scaler) ? parent_scaler[n] : 0;

    /* compute site log-likelihood and scale if necessary */
    site_lk = log(terma) * pattern_weights[n];
    if (scale_factors)
      site_lk += scale_factors * log(PLL_SCALE_THRESHOLD);

    /* store per-site log-likelihood */
    if (persite_lnl)
      persite_lnl[m++] = site_lk;

    logl += site_lk;
  }
  return logl;
}

PLL_EXPORT
double pll_core_edge_loglikelihood_ti_avx(unsigned int states,
                                          unsigned int sites,
                                          unsigned int rate_cats,
                                          const double * parent_clv,
                                          const unsigned int * parent_scaler,
                                          const unsigned char * tipchars,
                                          const unsigned int * tipmap,
                                          const double * pmatrix,
                                          double ** frequencies,
                                          const double * rate_weights,
                                          const unsigned int * pattern_weights,
                                          const double * invar_proportion,
                                          const int * invar_indices,
                                          const unsigned int * freqs_indices,
                                          double * persite_lnl)
{
  unsigned int n,i,j,k,m = 0;
  double logl = 0;
  double prop_invar = 0;

  const double * clvp = parent_clv;
  const double * pmat;
  const double * freqs = NULL;

  double terma, terma_r;
  double site_lk, inv_site_lk;

  unsigned int cstate;
  unsigned int scale_factors;
  unsigned int states_padded = (states+3) & 0xFFFFFFFC;

  __m256d xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
  __m256i mask;

  size_t displacement = (states_padded - states) * (states_padded);

  for (n = 0; n < sites; ++n)
  {
    pmat = pmatrix;
    terma = 0;

    cstate = tipmap[tipchars[n]];

    for (i = 0; i < rate_cats; ++i)
    {
      freqs = frequencies[freqs_indices[i]];
      terma_r = 0;

      /* iterate over quadruples of rows */
      for (j = 0; j < states_padded; j += 4)
      {
        xmm0 = _mm256_setzero_pd();
        xmm1 = _mm256_setzero_pd();
        xmm2 = _mm256_setzero_pd();
        xmm3 = _mm256_setzero_pd();

        /* point to the four rows */
        const double * row0 = pmat;
        const double * row1 = row0 + states_padded;
        const double * row2 = row1 + states_padded;
        const double * row3 = row2 + states_padded;

        /* set position of least significant bit in character state */
        register int lsb = 0;

        /* iterate quadruples of columns */
        for (k = 0; k < states_padded; k += 4)
        {
          
          /* set mask */
          mask = _mm256_set_epi64x(
                    ((cstate >> (lsb+3)) & 1) ? ~0 : 0,
                    ((cstate >> (lsb+2)) & 1) ? ~0 : 0,
                    ((cstate >> (lsb+1)) & 1) ? ~0 : 0,
                    ((cstate >> (lsb+0)) & 1) ? ~0 : 0);

          lsb += 4;

          /* row 0 */
          xmm4 = _mm256_maskload_pd(row0,mask);
          xmm0 = _mm256_add_pd(xmm0,xmm4);
          row0 += 4;

          /* row 1 */
          xmm4 = _mm256_maskload_pd(row1,mask);
          xmm1 = _mm256_add_pd(xmm1,xmm4);
          row1 += 4;

          /* row 2 */
          xmm4 = _mm256_maskload_pd(row2,mask);
          xmm2 = _mm256_add_pd(xmm2,xmm4);
          row2 += 4;

          /* row 3 */
          xmm4 = _mm256_maskload_pd(row3,mask);
          xmm3 = _mm256_add_pd(xmm3,xmm4);
          row3 += 4;

        }

        /* point pmatrix to the next four rows */ 
        pmat = row3;

        /* create a vector containing the sums of xmm0, xmm1, xmm2, xmm3 */
        xmm4 = _mm256_unpackhi_pd(xmm0,xmm1);
        xmm5 = _mm256_unpacklo_pd(xmm0,xmm1);

        xmm6 = _mm256_unpackhi_pd(xmm2,xmm3);
        xmm7 = _mm256_unpacklo_pd(xmm2,xmm3);

        xmm0 = _mm256_add_pd(xmm4,xmm5);
        xmm1 = _mm256_add_pd(xmm6,xmm7);

        xmm2 = _mm256_permute2f128_pd(xmm0,xmm1, _MM_SHUFFLE(0,2,0,1));
        xmm3 = _mm256_blend_pd(xmm0,xmm1,12);
        xmm0 = _mm256_add_pd(xmm2,xmm3);

        /* multiply with frequencies */
        xmm1 = _mm256_load_pd(freqs);
        xmm2 = _mm256_mul_pd(xmm0,xmm1);

        /* multiply with clvp */
        xmm0 = _mm256_load_pd(clvp);
        xmm1 = _mm256_mul_pd(xmm2,xmm0);

        /* add up the elements of xmm1 */
        xmm0 = _mm256_hadd_pd(xmm1,xmm1);
        terma_r += ((double *)&xmm0)[0] + ((double *)&xmm0)[2];

        freqs += 4;
        clvp += 4;
      }

      /* account for invariant sites */
      prop_invar = invar_proportion ? invar_proportion[freqs_indices[i]] : 0;
      if (prop_invar > 0)
      {
        freqs = frequencies[freqs_indices[i]];
        inv_site_lk = (invar_indices[n] == -1) ?
                          0 : freqs[invar_indices[n]];
        terma += rate_weights[i] * (terma_r * (1 - prop_invar) +
                 inv_site_lk * prop_invar);
      }
      else
      {
        terma += terma_r * rate_weights[i];
      }

      pmat -= displacement;
    }
    /* count number of scaling factors to acount for */
    scale_factors = (parent_scaler) ? parent_scaler[n] : 0;

    /* compute site log-likelihood and scale if necessary */
    site_lk = log(terma) * pattern_weights[n];
    if (scale_factors)
      site_lk += scale_factors * log(PLL_SCALE_THRESHOLD);

    /* store per-site log-likelihood */
    if (persite_lnl)
      persite_lnl[m++] = site_lk;

    logl += site_lk;
  }
  return logl;
}

PLL_EXPORT
double pll_core_edge_loglikelihood_ii_avx(unsigned int states,
                                          unsigned int sites,
                                          unsigned int rate_cats,
                                          const double * parent_clv,
                                          const unsigned int * parent_scaler,
                                          const double * child_clv,
                                          const unsigned int * child_scaler,
                                          const double * pmatrix,
                                          double ** frequencies,
                                          const double * rate_weights,
                                          const unsigned int * pattern_weights,
                                          const double * invar_proportion,
                                          const int * invar_indices,
                                          const unsigned int * freqs_indices,
                                          double * persite_lnl)
{
  unsigned int n,i,j,k,m = 0;
  double logl = 0;
  double prop_invar = 0;

  const double * clvp = parent_clv;
  const double * clvc = child_clv;
  const double * pmat;
  const double * freqs = NULL;

  double terma, terma_r;
  double site_lk, inv_site_lk;

  unsigned int scale_factors;
  unsigned int states_padded = (states+3) & 0xFFFFFFFC;

  __m256d xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

  size_t displacement = (states_padded - states) * (states_padded);

  for (n = 0; n < sites; ++n)
  {
    pmat = pmatrix;
    terma = 0;
    for (i = 0; i < rate_cats; ++i)
    {
      freqs = frequencies[freqs_indices[i]];
      terma_r = 0;
      
      /* iterate over quadruples of rows */
      for (j = 0; j < states_padded; j += 4)
      {
        xmm0 = _mm256_setzero_pd();
        xmm1 = _mm256_setzero_pd();
        xmm2 = _mm256_setzero_pd();
        xmm3 = _mm256_setzero_pd();
        
        /* row 1 */
        for (k = 0; k < states_padded; k += 4)
        {
          xmm4 = _mm256_load_pd(pmat);
          xmm5 = _mm256_load_pd(clvc+k);
          xmm6 = _mm256_mul_pd(xmm4,xmm5);
          xmm0 = _mm256_add_pd(xmm0,xmm6);

          pmat += 4;
        }

        /* row 2 */
        for (k = 0; k < states_padded; k += 4)
        {
          xmm4 = _mm256_load_pd(pmat);
          xmm5 = _mm256_load_pd(clvc+k);
          xmm6 = _mm256_mul_pd(xmm4,xmm5);
          xmm1 = _mm256_add_pd(xmm1,xmm6);

          pmat += 4;
        }

        /* row 3 */
        for (k = 0; k < states_padded; k += 4)
        {
          xmm4 = _mm256_load_pd(pmat);
          xmm5 = _mm256_load_pd(clvc+k);
          xmm6 = _mm256_mul_pd(xmm4,xmm5);
          xmm2 = _mm256_add_pd(xmm2,xmm6);

          pmat += 4;
        }

        /* row 4 */
        for (k = 0; k < states_padded; k += 4)
        {
          xmm4 = _mm256_load_pd(pmat);
          xmm5 = _mm256_load_pd(clvc+k);
          xmm6 = _mm256_mul_pd(xmm4,xmm5);
          xmm3 = _mm256_add_pd(xmm3,xmm6);

          pmat += 4;
        }

        /* create a vector containing the sums of xmm0, xmm1, xmm2, xmm3 */
        xmm4 = _mm256_unpackhi_pd(xmm0,xmm1);
        xmm5 = _mm256_unpacklo_pd(xmm0,xmm1);

        xmm6 = _mm256_unpackhi_pd(xmm2,xmm3);
        xmm7 = _mm256_unpacklo_pd(xmm2,xmm3);

        xmm0 = _mm256_add_pd(xmm4,xmm5);
        xmm1 = _mm256_add_pd(xmm6,xmm7);

        xmm2 = _mm256_permute2f128_pd(xmm0,xmm1, _MM_SHUFFLE(0,2,0,1));
        xmm3 = _mm256_blend_pd(xmm0,xmm1,12);
        xmm0 = _mm256_add_pd(xmm2,xmm3);

        /* multiply with frequencies */
        xmm1 = _mm256_load_pd(freqs);
        xmm2 = _mm256_mul_pd(xmm0,xmm1);

        /* multiply with clvp */
        xmm0 = _mm256_load_pd(clvp);
        xmm1 = _mm256_mul_pd(xmm2,xmm0);

        /* add up the elements of xmm1 */
        xmm0 = _mm256_hadd_pd(xmm1,xmm1);
        terma_r += ((double *)&xmm0)[0] + ((double *)&xmm0)[2];

        freqs += 4;
        clvp += 4;
      }

      /* account for invariant sites */
      prop_invar = invar_proportion ? invar_proportion[freqs_indices[i]] : 0;
      if (prop_invar > 0)
      {
        freqs = frequencies[freqs_indices[i]];
        inv_site_lk = (invar_indices[n] == -1) ?
                          0 : freqs[invar_indices[n]];
        terma += rate_weights[i] * (terma_r * (1 - prop_invar) +
                 inv_site_lk * prop_invar);
      }
      else
      {
        terma += terma_r * rate_weights[i];
      }

      clvc += states_padded;
      pmat -= displacement;
    }
    /* count number of scaling factors to acount for */
    scale_factors = (parent_scaler) ? parent_scaler[n] : 0;
    scale_factors += (child_scaler) ? child_scaler[n] : 0;

    /* compute site log-likelihood and scale if necessary */
    site_lk = log(terma) * pattern_weights[n];
    if (scale_factors)
      site_lk += scale_factors * log(PLL_SCALE_THRESHOLD);

    /* store per-site log-likelihood */
    if (persite_lnl)
      persite_lnl[m++] = site_lk;

    logl += site_lk;
  }
  return logl;
}

PLL_EXPORT
double pll_core_edge_loglikelihood_ii_4x4_avx(unsigned int sites,
                                              unsigned int rate_cats,
                                              const double * parent_clv,
                                              const unsigned int * parent_scaler,
                                              const double * child_clv,
                                              const unsigned int * child_scaler,
                                              const double * pmatrix,
                                              double ** frequencies,
                                              const double * rate_weights,
                                              const unsigned int * pattern_weights,
                                              const double * invar_proportion,
                                              const int * invar_indices,
                                              const unsigned int * freqs_indices,
                                              double * persite_lnl)
{
  unsigned int n,i,m = 0;
  double logl = 0;
  double prop_invar = 0;

  const double * clvp = parent_clv;
  const double * clvc = child_clv;
  const double * pmat;
  const double * freqs = NULL;

  double terma, terma_r;
  double site_lk, inv_site_lk;

  unsigned int scale_factors;
  unsigned int states = 4;
  unsigned int states_padded = 4;

  __m256d xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;
  
  for (n = 0; n < sites; ++n)
  {
    pmat = pmatrix;
    terma = 0;
    for (i = 0; i < rate_cats; ++i)
    {
      freqs = frequencies[freqs_indices[i]];

      /* load frequencies for current rate matrix */
      xmm0 = _mm256_load_pd(freqs);

      /* load clvc */
      xmm1 = _mm256_load_pd(clvc);

      /* load pmatrix row 1 and multiply with clvc */
      xmm2 = _mm256_load_pd(pmat);
      xmm3 = _mm256_mul_pd(xmm1,xmm2);

      /* load pmatrix row 2 and multiply with clvc */
      pmat += states;
      xmm2 = _mm256_load_pd(pmat);
      xmm4 = _mm256_mul_pd(xmm1,xmm2);

      /* load pmatrix row 3 and multiply with clvc */
      pmat += states;
      xmm2 = _mm256_load_pd(pmat);
      xmm5 = _mm256_mul_pd(xmm1,xmm2);

      /* load pmatrix row 4 and multiply with clvc */
      pmat += states;
      xmm2 = _mm256_load_pd(pmat);
      xmm6 = _mm256_mul_pd(xmm1,xmm2);

      /* point to the pmatrix for the next rate category */
      pmat += states;

      /* create a vector containing the sums of xmm3, xmm4, xmm5, xmm6 */
      xmm1 = _mm256_unpackhi_pd(xmm3,xmm4);
      xmm2 = _mm256_unpacklo_pd(xmm3,xmm4);

      xmm3 = _mm256_unpackhi_pd(xmm5,xmm6);
      xmm4 = _mm256_unpacklo_pd(xmm5,xmm6);

      xmm5 = _mm256_add_pd(xmm1,xmm2);
      xmm6 = _mm256_add_pd(xmm3,xmm4);

      xmm1 = _mm256_permute2f128_pd(xmm5,xmm6, _MM_SHUFFLE(0,2,0,1));
      xmm2 = _mm256_blend_pd(xmm5,xmm6,12);
      xmm3 = _mm256_add_pd(xmm1,xmm2);

      /* multiply with frequencies */
      xmm1 = _mm256_mul_pd(xmm0,xmm3);

      /* multiply with clvp */
      xmm2 = _mm256_load_pd(clvp);
      xmm0 = _mm256_mul_pd(xmm1,xmm2);

      /* add up the elements of xmm0 */
      xmm1 = _mm256_hadd_pd(xmm0,xmm0);
      terma_r = ((double *)&xmm1)[0] + ((double *)&xmm1)[2];


      /* account for invariant sites */
      prop_invar = invar_proportion ? invar_proportion[freqs_indices[i]] : 0;
      if (prop_invar > 0)
      {
        inv_site_lk = (invar_indices[n] == -1) ?
                          0 : freqs[invar_indices[n]];
        terma += rate_weights[i] * (terma_r * (1 - prop_invar) +
                 inv_site_lk * prop_invar);
      }
      else
      {
        terma += terma_r * rate_weights[i];
      }

      clvp += states_padded;
      clvc += states_padded;
    }

    /* count number of scaling factors to acount for */
    scale_factors = (parent_scaler) ? parent_scaler[n] : 0;
    scale_factors += (child_scaler) ? child_scaler[n] : 0;

    /* compute site log-likelihood and scale if necessary */
    site_lk = log(terma) * pattern_weights[n];
    if (scale_factors)
      site_lk += scale_factors * log(PLL_SCALE_THRESHOLD);

    /* store per-site log-likelihood */
    if (persite_lnl)
      persite_lnl[m++] = site_lk;

    logl += site_lk;
  }
  return logl;
}
