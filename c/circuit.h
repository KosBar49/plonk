/*
 * circuit.h -- circuit definition and preprocessing (setup).
 *
 * A circuit is given by its selector evaluations (q_L..q_C on each gate row)
 * and a permutation sigma over 3n positions. setup() converts these into
 * coefficient-form polynomials, exactly like plonk_fri/plonk.py:setup().
 */
#ifndef PLONK_CIRCUIT_H
#define PLONK_CIRCUIT_H

#include <stdint.h>
#include "field.h"
#include "config.h"

typedef struct {
    uint8_t log_n;
    uint32_t n;
    /* selector evaluations on H (length n) */
    felt qL[MAX_N], qR[MAX_N], qO[MAX_N], qM[MAX_N], qC[MAX_N];
    /* permutation over 3n positions */
    uint32_t sigma[3 * MAX_N];
} circuit_t;

typedef struct {
    uint8_t log_n;
    uint32_t n;
    felt omega;
    /* selectors in coefficient form (length n) */
    felt qL[MAX_N], qR[MAX_N], qO[MAX_N], qM[MAX_N], qC[MAX_N];
    /* sigma polynomials in coefficient form (length n) */
    felt S1[MAX_N], S2[MAX_N], S3[MAX_N];
} setup_t;

/* Fill `c` with the demo circuit: prove x^3 + x + 5 = 35. */
void circuit_demo(circuit_t *c);

/* Preprocess circuit -> setup (coefficient forms). */
void setup_circuit(const circuit_t *c, setup_t *pp);

#endif
