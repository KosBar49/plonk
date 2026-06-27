/*
 * selftest_main.c -- pure-C end-to-end: prove then verify in one process,
 * plus a tamper test. Build per field with -DFIELD_xxx.
 */
#include <stdio.h>
#include <time.h>
#include "field.h"
#include "circuit.h"
#include "prover.h"
#include "verifier.h"

int main(void) {
    static circuit_t c;
    static setup_t pp;
    static witness_t w;
    static proof_t pr;

    circuit_demo(&c);
    setup_circuit(&c, &pp);
    witness_demo(&w, 3);

    int rc = prove(&pp, &w, &pr);
    if (rc) { printf("field=%s prove FAILED rc=%d\n", FIELD_NAME, rc); return 1; }

    clock_t t0 = clock();
    int ok = verify(&pp, &pr);
    clock_t t1 = clock();
    double ms = 1000.0 * (double)(t1 - t0) / CLOCKS_PER_SEC;

    printf("field=%-10s  C-prove -> C-verify: %s   (verify %.2f ms)\n",
           FIELD_NAME, ok ? "ACCEPT" : "REJECT", ms);
    if (!ok) return 1;

    /* tamper: flip one opening, expect REJECT */
    proof_t bad = pr;
    bad.open_a = f_add(bad.open_a, 1);
    int bad_ok = verify(&pp, &bad);
    printf("field=%-10s  tampered opening:    %s\n",
           FIELD_NAME, bad_ok ? "ACCEPT (BUG!)" : "REJECT (good)");
    if (bad_ok) return 1;

    /* tamper: flip a query merkle leaf */
    proof_t bad2 = pr;
    bad2.queries[0].f_v_lo[0] = f_add(bad2.queries[0].f_v_lo[0], 1);
    int bad2_ok = verify(&pp, &bad2);
    printf("field=%-10s  tampered merkle leaf:%s\n",
           FIELD_NAME, bad2_ok ? "ACCEPT (BUG!)" : "REJECT (good)");
    if (bad2_ok) return 1;

    return 0;
}
