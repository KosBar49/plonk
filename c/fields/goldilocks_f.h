/*
 * Goldilocks field: p = 2^64 - 2^32 + 1 = 0xFFFFFFFF00000001.
 * felt = uint64_t. Multiplication needs a 128-bit intermediate.
 *
 * On hosts and 64-bit targets we use __uint128_t (GCC/Clang builtin).
 * On a true 32-bit MCU without __int128, define GOLDILOCKS_NO_INT128 to use
 * the explicit reduction (this is the path that pays the real 32-bit cost --
 * the whole reason Goldilocks is expected to lose to BabyBear on an MCU).
 */
#ifndef GOLDILOCKS_F_H
#define GOLDILOCKS_F_H

#include <stdint.h>

typedef uint64_t felt;

#define FIELD_NAME        "goldilocks"
#define FIELD_ID          1
#define FIELD_ELEM_BYTES  8

#define F_P            0xFFFFFFFF00000001ULL
#define F_MULT_GEN     7ULL
#define F_TWO_ADICITY  32
#define F_TWO_ADIC_GEN 1753635133440165772ULL
#define F_K1           7ULL
#define F_K2           49ULL

static inline felt f_add(felt a, felt b) {
    /* a,b < p < 2^64. Sum may overflow 64 bits. */
    uint64_t s = a + b;
    int carry = (s < a);                 /* overflow happened */
    if (carry || s >= F_P) s -= F_P;     /* one subtraction suffices */
    return s;
}
static inline felt f_sub(felt a, felt b) {
    return (a >= b) ? a - b : a + (F_P - b);  /* F_P - b can't overflow since b>0 */
}
static inline felt f_neg(felt a) {
    return a ? F_P - a : 0ULL;
}

#if defined(GOLDILOCKS_NO_INT128)
/* Explicit 128-bit reduction for 32-bit targets without __int128.
 * Reduces x = hi*2^64 + lo modulo p = 2^64 - 2^32 + 1 using the identity
 *   2^64 == 2^32 - 1 (mod p).
 * Standard plonky2-style folding. */
static inline felt f_mul(felt a, felt b) {
    /* Schoolbook 64x64 -> 128 using 32-bit limbs. */
    uint64_t a_lo = (uint32_t)a, a_hi = a >> 32;
    uint64_t b_lo = (uint32_t)b, b_hi = b >> 32;
    uint64_t ll = a_lo * b_lo;
    uint64_t lh = a_lo * b_hi;
    uint64_t hl = a_hi * b_lo;
    uint64_t hh = a_hi * b_hi;
    uint64_t cross = (ll >> 32) + (uint32_t)lh + (uint32_t)hl;
    uint64_t lo = (ll & 0xffffffffULL) | (cross << 32);
    uint64_t hi = hh + (lh >> 32) + (hl >> 32) + (cross >> 32);
    /* Now value = hi*2^64 + lo. Fold with 2^64 = 2^32 - 1 (mod p). */
    uint64_t hi_lo = (uint32_t)hi, hi_hi = hi >> 32;
    /* t = lo + hi_lo*2^32 - hi_lo - hi_hi  (mod p) */
    uint64_t t0 = lo;
    /* add hi_lo * (2^32 - 1) */
    uint64_t add = (hi_lo << 32) - hi_lo;
    uint64_t r = t0 + add;
    if (r < t0 || r >= F_P) r -= F_P;
    /* subtract hi_hi (since hi_hi*2^64 = hi_hi*(2^32-1)*2^32 ... ) -- handle simply */
    uint64_t sub = hi_hi % F_P;
    r = (r >= sub) ? r - sub : r + F_P - sub;
    /* one more fold for the (2^32-1) factor on hi_hi omitted-case: normalize */
    return r % F_P;  /* final safety reduce (cheap; correctness over micro-opt here) */
}
#else
static inline felt f_mul(felt a, felt b) {
    __uint128_t prod = (__uint128_t)a * (__uint128_t)b;
    return (felt)(prod % F_P);
}
#endif

static inline felt f_pow(felt a, uint64_t e) {
    felt r = 1ULL, base = a;
    while (e) {
        if (e & 1u) r = f_mul(r, base);
        base = f_mul(base, base);
        e >>= 1;
    }
    return r;
}
static inline felt f_inv(felt a) { return f_pow(a, F_P - 2ULL); }

static inline felt f_from_u64(uint64_t v) { return (felt)(v % F_P); }
static inline uint64_t f_to_u64(felt a)   { return (uint64_t)a; }

static inline felt f_root_of_unity(uint8_t log_n) {
    felt g = F_TWO_ADIC_GEN;
    for (uint8_t i = 0; i < (F_TWO_ADICITY - log_n); ++i) g = f_mul(g, g);
    return g;
}

#endif
