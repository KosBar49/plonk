/*
 * prover.h -- PLONK+FRI prover with fully static allocation.
 *
 * Produces a proof in the same in-memory structure the verifier consumes, and
 * the same wire format export_to_c.py / proof.c use, so a C-generated proof is
 * verifiable by the Python verifier and vice versa.
 */
#ifndef PLONK_PROVER_H
#define PLONK_PROVER_H

#include <stdint.h>
#include "field.h"
#include "config.h"
#include "circuit.h"
#include "merkle.h"

typedef struct {
    felt a[MAX_N], b[MAX_N], c[MAX_N];
} witness_t;

/* One FRI query's openings, in the layout the wire format expects. */
typedef struct {
    uint32_t q0;
    felt     f_v_lo[NUM_COMMITS], f_v_hi[NUM_COMMITS];
    /* paths stored flat: for each of the 7 polys, path_lo then path_hi,
       each log_NEXT hashes. */
    uint8_t  f_path_lo[NUM_COMMITS][MAX_LOG_NEXT * HASH_BYTES];
    uint8_t  f_path_hi[NUM_COMMITS][MAX_LOG_NEXT * HASH_BYTES];
    /* Q layer openings: layers 1..log_NEXT-1 */
    felt     q_v_lo[MAX_LOG_NEXT], q_v_hi[MAX_LOG_NEXT];
    uint8_t  q_path_lo[MAX_LOG_NEXT][MAX_LOG_NEXT * HASH_BYTES];
    uint8_t  q_path_hi[MAX_LOG_NEXT][MAX_LOG_NEXT * HASH_BYTES];
} query_t;

typedef struct {
    uint8_t  log_n, log_blowup;
    uint16_t num_queries;
    uint8_t  commits[NUM_COMMITS][HASH_BYTES];
    felt     open_a, open_b, open_c, open_Z, open_Zw, open_t_lo, open_t_mid, open_t_hi;
    uint8_t  q_layer_roots[MAX_LOG_NEXT][HASH_BYTES];
    uint8_t  q_layer_count;
    felt     fri_final_value;
    query_t  queries[NUM_QUERIES];
} proof_t;

/* Build the witness for the demo circuit with secret x. */
void witness_demo(witness_t *w, uint64_t x);

/* Prove. Returns 0 on success, nonzero on internal constraint failure. */
int prove(const setup_t *pp, const witness_t *w, proof_t *proof);

/* Print the static memory footprint of the prover buffers. */
void prover_footprint(void);

#endif
