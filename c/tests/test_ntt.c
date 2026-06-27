/* NTT self-test: forward+inverse round-trips, coset eval+interp round-trip. */
#include <stdio.h>
#include <stdint.h>
#include "field.h"
#include "ntt.h"

static int arr_eq(const felt *x, const felt *y, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) if (x[i] != y[i]) return 0;
    return 1;
}

int main(void) {
    printf("field=%s\n", FIELD_NAME);
    uint8_t log_n = 4;
    uint32_t n = 1u << log_n;
    felt a[16], orig[16];
    for (uint32_t i = 0; i < n; ++i) { a[i] = f_from_u64(1000 + i*7); orig[i] = a[i]; }

    /* forward then inverse == identity */
    ntt_inplace(a, log_n, 0);
    ntt_inplace(a, log_n, 1);
    printf("ntt forward+inverse roundtrip: %s\n", arr_eq(a, orig, n) ? "OK" : "FAIL");

    /* coset eval then interp == identity (padded) */
    uint8_t log_N = 6;
    uint32_t N = 1u << log_N;
    felt ev[64], co[64];
    coset_eval(orig, log_n, F_MULT_GEN, log_N, ev);
    coset_interp(ev, F_MULT_GEN, log_N, co);
    /* first n coeffs should match orig, rest zero */
    int ok = 1;
    for (uint32_t i = 0; i < n; ++i) if (co[i] != orig[i]) ok = 0;
    for (uint32_t i = n; i < N; ++i) if (co[i] != 0) ok = 0;
    printf("coset eval+interp roundtrip: %s\n", ok ? "OK" : "FAIL");

    return 0;
}
