/*
 * verifier.h -- field-generic PLONK+FRI verifier over the static proof_t.
 */
#ifndef PLONK_VERIFIER_H
#define PLONK_VERIFIER_H

#include "field.h"
#include "circuit.h"
#include "prover.h"   /* proof_t, query_t */

/* Verify proof against preprocessed setup pp. Returns 1 accept, 0 reject. */
int verify(const setup_t *pp, const proof_t *proof);

#endif
