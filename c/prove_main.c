/*
 * prove_main.c -- run the C prover on the demo circuit, print timing and the
 * static footprint, and serialize the proof to c_proof.bin in the SAME wire
 * format as plonk_fri/export_to_c.py, so the Python verifier can check it.
 *
 * Build per field with -DFIELD_xxx.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "field.h"
#include "config.h"
#include "circuit.h"
#include "prover.h"

static void put_u16(FILE *f, uint16_t v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); }
static void put_u32(FILE *f, uint32_t v) {
    fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f);
    fputc((v >> 16) & 0xff, f); fputc((v >> 24) & 0xff, f);
}
static void put_elem(FILE *f, felt v) {
    uint64_t x = f_to_u64(v);
    for (int i = 0; i < FIELD_ELEM_BYTES; ++i) fputc((x >> (8 * i)) & 0xff, f);
}

static void serialize(const proof_t *pr, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror("fopen"); exit(2); }
    uint8_t log_NEXT = pr->log_n + pr->log_blowup;

    /* header */
    fputc('P', f); fputc('L', f); fputc('N', f); fputc('K', f);
    put_u16(f, 1);                 /* version */
    put_u16(f, FIELD_ID);
    fputc(pr->log_n, f); fputc(pr->log_blowup, f);
    put_u16(f, pr->num_queries);
    put_u32(f, 0);                 /* reserved */

    /* commits */
    for (int k = 0; k < NUM_COMMITS; ++k) fwrite(pr->commits[k], 1, 32, f);

    /* openings (8) */
    put_elem(f, pr->open_a);   put_elem(f, pr->open_b);   put_elem(f, pr->open_c);
    put_elem(f, pr->open_Z);   put_elem(f, pr->open_Zw);
    put_elem(f, pr->open_t_lo);put_elem(f, pr->open_t_mid);put_elem(f, pr->open_t_hi);

    /* fri layer roots + final */
    for (int i = 0; i < pr->q_layer_count; ++i) fwrite(pr->q_layer_roots[i], 1, 32, f);
    put_elem(f, pr->fri_final_value);

    /* queries */
    for (int qi = 0; qi < pr->num_queries; ++qi) {
        const query_t *Q = &pr->queries[qi];
        put_u32(f, Q->q0);
        for (int k = 0; k < NUM_COMMITS; ++k) {
            put_elem(f, Q->f_v_lo[k]); put_elem(f, Q->f_v_hi[k]);
        }
        for (int k = 0; k < NUM_COMMITS; ++k) {
            fwrite(Q->f_path_lo[k], 1, (size_t)log_NEXT * 32, f);
            fwrite(Q->f_path_hi[k], 1, (size_t)log_NEXT * 32, f);
        }
        for (int li = 0; li < pr->q_layer_count; ++li) {
            uint8_t path_len = log_NEXT - (li + 1);
            put_elem(f, Q->q_v_lo[li]); put_elem(f, Q->q_v_hi[li]);
            fwrite(Q->q_path_lo[li], 1, (size_t)path_len * 32, f);
            fwrite(Q->q_path_hi[li], 1, (size_t)path_len * 32, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "c_proof.bin";

    static circuit_t c;
    static setup_t pp;
    static witness_t w;
    static proof_t pr;

    circuit_demo(&c);
    setup_circuit(&c, &pp);
    witness_demo(&w, 3);

    prover_footprint();

    clock_t t0 = clock();
    int rc = prove(&pp, &w, &pr);
    clock_t t1 = clock();
    if (rc != 0) { fprintf(stderr, "prove failed rc=%d\n", rc); return 1; }

    double ms = 1000.0 * (double)(t1 - t0) / CLOCKS_PER_SEC;
    printf("field=%s prove_time=%.2f ms\n", FIELD_NAME, ms);

    serialize(&pr, out);
    printf("wrote %s\n", out);
    return 0;
}
