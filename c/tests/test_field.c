/* Field arithmetic self-test. Build per field with -DFIELD_xxx.
 * Verifies add/sub/mul/inv/pow and root-of-unity order against known answers. */
#include <stdio.h>
#include <stdint.h>
#include "field.h"

int main(void) {
    printf("field=%s id=%d elem_bytes=%d p=%llu\n",
           FIELD_NAME, FIELD_ID, FIELD_ELEM_BYTES, (unsigned long long)F_P);

    /* basic identities */
    felt a = f_from_u64(123456789ULL % F_P);
    felt b = f_from_u64(987654321ULL % F_P);
    felt ab = f_mul(a, b);
    felt inv_b = f_inv(b);
    felt back = f_mul(ab, inv_b);  /* should == a */
    printf("mul/inv roundtrip: %s\n", (back == a) ? "OK" : "FAIL");

    /* a * a^-1 == 1 */
    felt one = f_mul(a, f_inv(a));
    printf("a*inv(a)==1: %s\n", (one == 1) ? "OK" : "FAIL");

    /* (a+b)-b == a */
    felt c = f_sub(f_add(a, b), b);
    printf("(a+b)-b==a: %s\n", (c == a) ? "OK" : "FAIL");

    /* Fermat: a^(p-1) == 1 */
    felt fermat = f_pow(a, (uint64_t)F_P - 1ULL);
    printf("a^(p-1)==1: %s\n", (fermat == 1) ? "OK" : "FAIL");

    /* root of unity of order 2^log_n: w^(2^log_n)==1, w^(2^(log_n-1))==p-1 */
    uint8_t log_n = 3;
    felt w = f_root_of_unity(log_n);
    felt w_full = f_pow(w, 1ULL << log_n);
    felt w_half = f_pow(w, 1ULL << (log_n - 1));
    printf("rou order 2^%d: %s\n", log_n,
           (w_full == 1 && w_half == F_P - 1) ? "OK" : "FAIL");

    /* larger domain used by FRI: log_N = 6 */
    uint8_t log_N = 6;
    felt wN = f_root_of_unity(log_N);
    felt wN_full = f_pow(wN, 1ULL << log_N);
    felt wN_half = f_pow(wN, 1ULL << (log_N - 1));
    printf("rou order 2^%d: %s\n", log_N,
           (wN_full == 1 && wN_half == F_P - 1) ? "OK" : "FAIL");

    return 0;
}
