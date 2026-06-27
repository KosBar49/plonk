/*
 * BabyBear field: p = 15 * 2^27 + 1 = 2013265921. 31-bit, felt = uint32_t.
 */
#ifndef BABYBEAR_F_H
#define BABYBEAR_F_H

#include <stdint.h>

typedef uint32_t felt;

#define FIELD_NAME        "babybear"
#define FIELD_ID          2
#define FIELD_ELEM_BYTES  4

#define F_P            2013265921u
#define F_MULT_GEN     31u
#define F_TWO_ADICITY  27
/* TWO_ADIC_GEN = MULT_GEN^15 mod p (element of order 2^27). Precomputed. */
#define F_TWO_ADIC_GEN 440564289u
#define F_K1           31u
#define F_K2           961u   /* 31^2 */

static inline felt f_add(felt a, felt b) {
    uint32_t s = a + b;
    return (s >= F_P) ? s - F_P : s;
}
static inline felt f_sub(felt a, felt b) {
    return (a >= b) ? a - b : a + F_P - b;
}
static inline felt f_neg(felt a) {
    return a ? F_P - a : 0u;
}
static inline felt f_mul(felt a, felt b) {
    return (uint32_t)(((uint64_t)a * (uint64_t)b) % F_P);
}
static inline felt f_pow(felt a, uint64_t e) {
    felt r = 1u, base = a;
    while (e) {
        if (e & 1u) r = f_mul(r, base);
        base = f_mul(base, base);
        e >>= 1;
    }
    return r;
}
static inline felt f_inv(felt a) { return f_pow(a, (uint64_t)F_P - 2u); }

static inline felt f_from_u64(uint64_t v) { return (felt)(v % F_P); }
static inline uint64_t f_to_u64(felt a)   { return (uint64_t)a; }

static inline felt f_root_of_unity(uint8_t log_n) {
    felt g = F_TWO_ADIC_GEN;
    for (uint8_t i = 0; i < (F_TWO_ADICITY - log_n); ++i) g = f_mul(g, g);
    return g;
}

#endif
