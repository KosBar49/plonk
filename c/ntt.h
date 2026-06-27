/*
 * ntt.h -- radix-2 Cooley-Tukey NTT over the selected field, plus coset
 * evaluation and interpolation. Operates in place on felt arrays.
 *
 * All sizes are powers of two. The caller owns the buffers.
 */
#ifndef PLONK_NTT_H
#define PLONK_NTT_H

#include <stdint.h>
#include "field.h"

/* In-place forward (inverse=0) or inverse (inverse=1) NTT.
 * a has length n = 2^log_n. */
void ntt_inplace(felt *a, uint8_t log_n, int inverse);

/* Evaluate poly `coeffs` (length 2^log_n_src, zero-padded into a length-2^log_N
 * work buffer) on the coset shift*<omega_N>. Result written to `out`
 * (length 2^log_N). out and coeffs may not alias. */
void coset_eval(const felt *coeffs, uint8_t log_n_src,
                felt shift, uint8_t log_N, felt *out);

/* Inverse of coset_eval: from evaluations on coset shift*<omega_N> (length
 * 2^log_N) back to coefficients (length 2^log_N) in `out`. */
void coset_interp(const felt *evals, felt shift, uint8_t log_N, felt *out);

#endif
