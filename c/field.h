/*
 * field.h -- compile-time field selection for the PLONK+FRI implementation.
 *
 * Select exactly one field at compile time:
 *     -DFIELD_GOLDILOCKS   p = 2^64 - 2^32 + 1   (felt = uint64_t)
 *     -DFIELD_BABYBEAR     p = 15*2^27 + 1       (felt = uint32_t)
 *     -DFIELD_KOALABEAR    p = 2^31 - 2^24 + 1   (felt = uint32_t)
 *
 * The rest of the codebase (ntt, merkle, transcript, prover, verifier) is
 * written against this abstract interface and never refers to a concrete
 * prime. The `felt` type is 32-bit for the small fields and 64-bit for
 * Goldilocks -- this is deliberate, so that a benchmark on a 32-bit MCU
 * actually pays the 64-bit cost for Goldilocks and the cheap 32-bit cost
 * for BabyBear/KoalaBear.
 *
 * Interface every field must provide:
 *     typedef ... felt;                  field element type
 *     FIELD_NAME (string), FIELD_ID (1/2/3), FIELD_ELEM_BYTES (8 or 4)
 *     F_P, F_MULT_GEN, F_TWO_ADIC_GEN, F_TWO_ADICITY, F_K1, F_K2
 *     felt f_add/f_sub/f_mul/f_neg(a[,b])
 *     felt f_pow(a, e)  (e is uint64_t)
 *     felt f_inv(a)
 *     felt f_root_of_unity(uint8_t log_n)
 *     felt f_from_u64(uint64_t)   / uint64_t f_to_u64(felt)
 */

#ifndef PLONK_FIELD_H
#define PLONK_FIELD_H

#include <stdint.h>

#if defined(FIELD_GOLDILOCKS)
  #include "fields/goldilocks_f.h"
#elif defined(FIELD_BABYBEAR)
  #include "fields/babybear_f.h"
#elif defined(FIELD_KOALABEAR)
  #include "fields/koalabear_f.h"
#else
  #error "Select a field: -DFIELD_GOLDILOCKS, -DFIELD_BABYBEAR, or -DFIELD_KOALABEAR"
#endif

/* Shared helpers that depend only on felt arithmetic. */

static inline void felt_to_8le(felt v, uint8_t out[8]) {
    uint64_t x = f_to_u64(v);
    for (int i = 0; i < 8; ++i) out[i] = (uint8_t)(x >> (8 * i));
}

static inline felt poly_eval(const felt *p, uint32_t n, felt x) {
    felt r = 0;
    for (uint32_t i = n; i > 0; --i) r = f_add(f_mul(r, x), p[i - 1]);
    return r;
}

static inline void powers(felt base, felt *out, uint32_t count) {
    out[0] = 1;
    for (uint32_t i = 1; i < count; ++i) out[i] = f_mul(out[i - 1], base);
}

#endif /* PLONK_FIELD_H */
