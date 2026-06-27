/*
 * config.h -- compile-time dimensions for the static-allocation PLONK+FRI build.
 *
 * Everything is sized to MAX_LOG_N. There is NO dynamic allocation anywhere in
 * the prover or verifier; all working buffers are static arrays sized by the
 * macros below. This means the static footprint (printed by prover_footprint())
 * is a direct measurement of the RAM a given MAX_LOG_N needs -- which is exactly
 * the feasibility question for the ESP32.
 *
 * To change the maximum circuit size, override MAX_LOG_N at compile time:
 *     -DMAX_LOG_N=10
 *
 * FRI parameters (LOG_BLOWUP, NUM_QUERIES) MUST match plonk_fri/fri.py so that
 * C and Python proofs are mutually verifiable.
 */
#ifndef PLONK_CONFIG_H
#define PLONK_CONFIG_H

#ifndef MAX_LOG_N
#define MAX_LOG_N 8            /* max gates = 2^8 = 256 */
#endif

#define LOG_BLOWUP   3         /* blowup = 8  (matches fri.py) */
#define NUM_QUERIES  24        /* matches fri.py */

#define MAX_N        (1u << MAX_LOG_N)
#define MAX_LOG_NEXT (MAX_LOG_N + LOG_BLOWUP)
#define MAX_NEXT     (1u << MAX_LOG_NEXT)

/* Number of committed layer-0 polynomials: a,b,c,Z,t_lo,t_mid,t_hi */
#define NUM_COMMITS  7
/* Openings: the 7 above at zeta, plus Z at zeta*omega */
#define NUM_OPENINGS 8

#endif
