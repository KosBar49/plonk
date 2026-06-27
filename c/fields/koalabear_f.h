/*
 * KoalaBear field: p = 2^31 - 2^24 + 1 = 2130706433. 31-bit, felt = uint32_t.
 */
#ifndef KOALABEAR_F_H
#define KOALABEAR_F_H

#include <stdint.h>

typedef uint32_t felt;

#define FIELD_NAME        "koalabear"
#define FIELD_ID          3
#define FIELD_ELEM_BYTES  4

#define F_P            2130706433u
#define F_MULT_GEN     3u
#define F_TWO_ADICITY  24
/* TWO_ADIC_GEN = MULT_GEN^127 mod p (element of order 2^24). Precomputed. */
#define F_TWO_ADIC_GEN 1791270792u
#define F_K1           3u
#define F_K2           9u    /* 3^2 */

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
