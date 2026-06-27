/* Print setup() output (selector + sigma coeffs) for cross-check vs Python. */
#include <stdio.h>
#include "field.h"
#include "circuit.h"

static void dump(const char *name, const felt *p, uint32_t n) {
    printf("%s=", name);
    for (uint32_t i = 0; i < n; ++i)
        printf("%llu%s", (unsigned long long)f_to_u64(p[i]), (i + 1 < n) ? "," : "");
    printf("\n");
}

int main(void) {
    circuit_t c; setup_t pp;
    circuit_demo(&c);
    setup_circuit(&c, &pp);
    printf("field=%s n=%u omega=%llu\n", FIELD_NAME, pp.n,
           (unsigned long long)f_to_u64(pp.omega));
    dump("qL", pp.qL, pp.n);
    dump("qC", pp.qC, pp.n);
    dump("S1", pp.S1, pp.n);
    dump("S2", pp.S2, pp.n);
    dump("S3", pp.S3, pp.n);
    return 0;
}
