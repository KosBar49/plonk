#include "ntt.h"

static void bit_reverse_permute(felt *a, uint32_t n) {
    uint32_t j = 0;
    for (uint32_t i = 1; i < n; ++i) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j |= bit;
        if (i < j) { felt t = a[i]; a[i] = a[j]; a[j] = t; }
    }
}

void ntt_inplace(felt *a, uint8_t log_n, int inverse) {
    uint32_t n = 1u << log_n;
    if (n == 1) return;

    bit_reverse_permute(a, n);

    felt omega = f_root_of_unity(log_n);
    if (inverse) omega = f_inv(omega);

    for (uint32_t length = 2; length <= n; length <<= 1) {
        /* principal length-th root = omega^(n/length) */
        felt w_step = f_pow(omega, (uint64_t)(n / length));
        uint32_t half = length >> 1;
        for (uint32_t i = 0; i < n; i += length) {
            felt w = 1;
            for (uint32_t k = 0; k < half; ++k) {
                felt u = a[i + k];
                felt v = f_mul(a[i + k + half], w);
                a[i + k]        = f_add(u, v);
                a[i + k + half] = f_sub(u, v);
                w = f_mul(w, w_step);
            }
        }
    }

    if (inverse) {
        felt n_inv = f_inv(f_from_u64((uint64_t)n));
        for (uint32_t i = 0; i < n; ++i) a[i] = f_mul(a[i], n_inv);
    }
}

void coset_eval(const felt *coeffs, uint8_t log_n_src,
                felt shift, uint8_t log_N, felt *out) {
    uint32_t n = 1u << log_N;
    uint32_t src = 1u << log_n_src;
    /* copy + zero-pad */
    felt cur = 1;
    for (uint32_t i = 0; i < n; ++i) {
        felt c = (i < src) ? coeffs[i] : 0;
        out[i] = f_mul(c, cur);   /* c_i * shift^i */
        cur = f_mul(cur, shift);
    }
    ntt_inplace(out, log_N, 0);
}

void coset_interp(const felt *evals, felt shift, uint8_t log_N, felt *out) {
    uint32_t n = 1u << log_N;
    for (uint32_t i = 0; i < n; ++i) out[i] = evals[i];
    ntt_inplace(out, log_N, 1);
    felt shift_inv = f_inv(shift);
    felt cur = 1;
    for (uint32_t i = 0; i < n; ++i) {
        out[i] = f_mul(out[i], cur);
        cur = f_mul(cur, shift_inv);
    }
}
