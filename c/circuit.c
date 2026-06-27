#include "circuit.h"
#include "ntt.h"
#include <string.h>

void circuit_demo(circuit_t *c) {
    c->log_n = 3;
    c->n = 8;
    uint32_t n = c->n;

    felt z = 0, one = 1, neg_one = f_neg(1);
    felt neg5  = f_neg(f_from_u64(5));
    felt neg35 = f_neg(f_from_u64(35));

    felt qL[8] = { z,       z,       one,     one,     one,  one,   z, z };
    felt qR[8] = { z,       z,       one,     one,     z,    z,     z, z };
    felt qO[8] = { neg_one, neg_one, neg_one, neg_one, z,    z,     z, z };
    felt qM[8] = { one,     one,     z,       z,       z,    z,     z, z };
    felt qC[8] = { z,       z,       z,       z,       neg5, neg35, z, z };

    memcpy(c->qL, qL, sizeof(qL));
    memcpy(c->qR, qR, sizeof(qR));
    memcpy(c->qO, qO, sizeof(qO));
    memcpy(c->qM, qM, sizeof(qM));
    memcpy(c->qC, qC, sizeof(qC));

    /* identity permutation, then apply copy-class cycles */
    for (uint32_t i = 0; i < 3 * n; ++i) c->sigma[i] = i;

    /* copy classes (positions: col*n + row; a=0..7,b=8..15,c=16..23) */
    uint32_t classes[6][4] = {
        {0, 8, 9, 10},   /* a0,b0,b1,b2 = x */
        {16, 1, 0, 0},   /* c0,a1 */
        {17, 2, 0, 0},   /* c1,a2 */
        {18, 3, 0, 0},   /* c2,a3 */
        {11, 4, 0, 0},   /* b3,a4 */
        {19, 5, 0, 0},   /* c3,a5 */
    };
    uint32_t class_len[6] = {4, 2, 2, 2, 2, 2};
    for (int ci = 0; ci < 6; ++ci) {
        uint32_t k = class_len[ci];
        for (uint32_t i = 0; i < k; ++i) {
            c->sigma[classes[ci][i]] = classes[ci][(i + 1) % k];
        }
    }
}

void setup_circuit(const circuit_t *c, setup_t *pp) {
    uint8_t log_n = c->log_n;
    uint32_t n = c->n;
    pp->log_n = log_n;
    pp->n = n;
    pp->omega = f_root_of_unity(log_n);

    /* selectors: coeff form = intt(evals) */
    felt tmp[MAX_N];
    #define INTT_COPY(dst, src) do { \
        memcpy(tmp, (src), n * sizeof(felt)); \
        ntt_inplace(tmp, log_n, 1); \
        memcpy((dst), tmp, n * sizeof(felt)); \
    } while (0)

    INTT_COPY(pp->qL, c->qL);
    INTT_COPY(pp->qR, c->qR);
    INTT_COPY(pp->qO, c->qO);
    INTT_COPY(pp->qM, c->qM);
    INTT_COPY(pp->qC, c->qC);

    /* sigma polynomials.
     * position p = col*n + i ; sigma maps p -> q = col'*n + i'.
     * S_{col}(omega^i) = k_factor[col'] * omega^(i').  Then intt. */
    felt k_factor[3] = { 1, F_K1, F_K2 };
    felt omega = pp->omega;
    felt omega_pow[MAX_N];
    omega_pow[0] = 1;
    for (uint32_t i = 1; i < n; ++i) omega_pow[i] = f_mul(omega_pow[i - 1], omega);

    felt s_vals[3][MAX_N];
    for (uint32_t col = 0; col < 3; ++col) {
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t p = col * n + i;
            uint32_t q = c->sigma[p];
            uint32_t col_prime = q / n;
            uint32_t i_prime = q % n;
            s_vals[col][i] = f_mul(k_factor[col_prime], omega_pow[i_prime]);
        }
    }
    INTT_COPY(pp->S1, s_vals[0]);
    INTT_COPY(pp->S2, s_vals[1]);
    INTT_COPY(pp->S3, s_vals[2]);
    #undef INTT_COPY
}
