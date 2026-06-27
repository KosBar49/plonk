/*
 * verifier.c -- field-generic PLONK+FRI verifier. Mirrors plonk.py:verify()
 * and the original c_verifier/plonk_verifier.c, but works over any selected
 * field and the static proof_t structure.
 */
#include "verifier.h"
#include "transcript.h"
#include "merkle.h"
#include <string.h>

int verify(const setup_t *pp, const proof_t *proof) {
    const uint8_t log_n = pp->log_n;
    const uint32_t n = pp->n;
    const felt omega = pp->omega;
    const uint8_t log_NEXT = log_n + LOG_BLOWUP;
    const uint32_t NEXT = 1u << log_NEXT;

    if (proof->log_n != log_n) return 0;
    if (proof->num_queries != NUM_QUERIES) return 0;

    transcript_t tr;
    transcript_init(&tr, "plonk-fri", 9);

    /* replay R1..R4 */
    transcript_absorb_root(&tr, "commit_a", proof->commits[0]);
    transcript_absorb_root(&tr, "commit_b", proof->commits[1]);
    transcript_absorb_root(&tr, "commit_c", proof->commits[2]);
    felt beta  = transcript_challenge_field(&tr, "beta");
    felt gamma = transcript_challenge_field(&tr, "gamma");
    transcript_absorb_root(&tr, "commit_Z", proof->commits[3]);
    felt alpha = transcript_challenge_field(&tr, "alpha");
    transcript_absorb_root(&tr, "commit_t_lo",  proof->commits[4]);
    transcript_absorb_root(&tr, "commit_t_mid", proof->commits[5]);
    transcript_absorb_root(&tr, "commit_t_hi",  proof->commits[6]);
    felt zeta = transcript_challenge_field(&tr, "zeta");
    felt zeta_omega = f_mul(zeta, omega);
    felt opens[8] = { proof->open_a, proof->open_b, proof->open_c, proof->open_Z,
                      proof->open_Zw, proof->open_t_lo, proof->open_t_mid, proof->open_t_hi };
    transcript_absorb_elems(&tr, "openings", opens, 8);
    felt rho = transcript_challenge_field(&tr, "rho");

    /* ---- Step A: PLONK identity at zeta ---- */
    felt qL = poly_eval(pp->qL, n, zeta), qR = poly_eval(pp->qR, n, zeta);
    felt qO = poly_eval(pp->qO, n, zeta), qM = poly_eval(pp->qM, n, zeta);
    felt qC = poly_eval(pp->qC, n, zeta);
    felt S1 = poly_eval(pp->S1, n, zeta), S2 = poly_eval(pp->S2, n, zeta);
    felt S3 = poly_eval(pp->S3, n, zeta);

    felt a = proof->open_a, b = proof->open_b, c = proof->open_c;
    felt Z = proof->open_Z, Zw = proof->open_Zw;
    felt tl = proof->open_t_lo, tm = proof->open_t_mid, th = proof->open_t_hi;

    felt zeta_n = f_pow(zeta, (uint64_t)n);
    felt ZH = f_sub(zeta_n, 1);
    felt L0 = f_mul(ZH, f_inv(f_mul(f_from_u64((uint64_t)n), f_sub(zeta, 1))));

    felt gate = f_add(
        f_add(f_add(f_mul(qL, a), f_mul(qR, b)), f_mul(qO, c)),
        f_add(f_mul(f_mul(qM, a), b), qC));
    felt pn = f_mul(
        f_mul(f_add(f_add(a, f_mul(beta, zeta)), gamma),
              f_add(f_add(b, f_mul(beta, f_mul(F_K1, zeta))), gamma)),
        f_add(f_add(c, f_mul(beta, f_mul(F_K2, zeta))), gamma));
    felt pd = f_mul(
        f_mul(f_add(f_add(a, f_mul(beta, S1)), gamma),
              f_add(f_add(b, f_mul(beta, S2)), gamma)),
        f_add(f_add(c, f_mul(beta, S3)), gamma));
    felt perm = f_sub(f_mul(pn, Z), f_mul(pd, Zw));
    felt boundary = f_mul(f_sub(Z, 1), L0);
    felt Cz = f_add(f_add(gate, f_mul(alpha, perm)), f_mul(f_mul(alpha, alpha), boundary));

    felt zeta_2n = f_mul(zeta_n, zeta_n);
    felt tz = f_add(f_add(tl, f_mul(zeta_n, tm)), f_mul(zeta_2n, th));

    if (Cz != f_mul(tz, ZH)) return 0;

    /* ---- Step B: FRI ---- */
    uint8_t expected_layers = log_NEXT - 1;
    if (proof->q_layer_count != expected_layers) return 0;

    felt fri_alphas[MAX_LOG_NEXT];
    for (uint8_t i = 0; i < log_NEXT; ++i) {
        fri_alphas[i] = transcript_challenge_field(&tr, "fri_alpha");
        if (i < expected_layers)
            transcript_absorb_root(&tr, "fri_root", proof->q_layer_roots[i]);
    }
    transcript_absorb_elem(&tr, "fri_final", proof->fri_final_value);

    felt rho_pow[8];
    powers(rho, rho_pow, 8);
    felt omega_N = f_root_of_unity(log_NEXT);
    felt two_inv = f_inv(2);

    const uint8_t *f_roots[7] = {
        proof->commits[0], proof->commits[1], proof->commits[2], proof->commits[3],
        proof->commits[4], proof->commits[5], proof->commits[6] };
    felt f_vals[7] = { a, b, c, Z, tl, tm, th };
    const int Z_index = 3;

    for (int qi = 0; qi < NUM_QUERIES; ++qi) {
        const query_t *Q = &proof->queries[qi];
        uint32_t q0 = transcript_challenge_int(&tr, "fri_query", NEXT);
        if (Q->q0 != q0) return 0;

        uint32_t p_lo[MAX_LOG_NEXT], p_hi[MAX_LOG_NEXT];
        uint32_t cq = q0, cn = NEXT;
        for (uint8_t L = 0; L < log_NEXT; ++L) {
            uint32_t h = cn >> 1;
            p_lo[L] = cq % h; p_hi[L] = p_lo[L] + h; cq = p_lo[L]; cn = h;
        }
        uint32_t plo0 = p_lo[0], phi0 = p_hi[0];

        for (int k = 0; k < 7; ++k) {
            if (!merkle_verify(f_roots[k], Q->f_v_lo[k], plo0, Q->f_path_lo[k], log_NEXT)) return 0;
            if (!merkle_verify(f_roots[k], Q->f_v_hi[k], phi0, Q->f_path_hi[k], log_NEXT)) return 0;
        }

        felt x_lo0 = f_mul(F_MULT_GEN, f_pow(omega_N, (uint64_t)plo0));
        felt x_hi0 = f_neg(x_lo0); /* phi0 = plo0 + NEXT/2, omega_N^(NEXT/2) = -1 */

        felt inv_xl_z = f_inv(f_sub(x_lo0, zeta));
        felt inv_xh_z = f_inv(f_sub(x_hi0, zeta));
        felt Qlo = 0, Qhi = 0;
        for (int k = 0; k < 7; ++k) {
            felt v = f_vals[k];
            Qlo = f_add(Qlo, f_mul(rho_pow[k], f_mul(f_sub(Q->f_v_lo[k], v), inv_xl_z)));
            Qhi = f_add(Qhi, f_mul(rho_pow[k], f_mul(f_sub(Q->f_v_hi[k], v), inv_xh_z)));
        }
        felt inv_xl_zw = f_inv(f_sub(x_lo0, zeta_omega));
        felt inv_xh_zw = f_inv(f_sub(x_hi0, zeta_omega));
        Qlo = f_add(Qlo, f_mul(rho_pow[7], f_mul(f_sub(Q->f_v_lo[Z_index], Zw), inv_xl_zw)));
        Qhi = f_add(Qhi, f_mul(rho_pow[7], f_mul(f_sub(Q->f_v_hi[Z_index], Zw), inv_xh_zw)));

        /* fold layer 0 -> 1 */
        felt feven = f_mul(f_add(Qlo, Qhi), two_inv);
        felt fodd  = f_mul(f_sub(Qlo, Qhi), f_mul(two_inv, f_inv(x_lo0)));
        felt folded = f_add(feven, f_mul(fri_alphas[0], fodd));

        /* check against layer 1 opening */
        uint32_t next_half = (NEXT >> 1) >> 1;
        {
            const query_t *qq = Q;
            if (!merkle_verify(proof->q_layer_roots[0], qq->q_v_lo[0], p_lo[1],
                               qq->q_path_lo[0], log_NEXT - 1)) return 0;
            if (!merkle_verify(proof->q_layer_roots[0], qq->q_v_hi[0], p_hi[1],
                               qq->q_path_hi[0], log_NEXT - 1)) return 0;
            felt expected = (plo0 < next_half) ? qq->q_v_lo[0] : qq->q_v_hi[0];
            if (folded != expected) return 0;
        }

        /* continue folding through committed layers */
        felt cur_shift = f_mul(F_MULT_GEN, F_MULT_GEN);
        uint8_t cur_log = log_NEXT - 1;
        for (uint8_t li = 0; li < expected_layers; ++li) {
            uint32_t plo = p_lo[li + 1], phi = p_hi[li + 1];
            uint8_t path_len = log_NEXT - (li + 1);
            if (li > 0) {
                if (!merkle_verify(proof->q_layer_roots[li], Q->q_v_lo[li], plo,
                                   Q->q_path_lo[li], path_len)) return 0;
                if (!merkle_verify(proof->q_layer_roots[li], Q->q_v_hi[li], phi,
                                   Q->q_path_hi[li], path_len)) return 0;
            }
            felt om = f_root_of_unity(cur_log);
            felt x_lo_layer = f_mul(cur_shift, f_pow(om, (uint64_t)plo));
            felt fe = f_mul(f_add(Q->q_v_lo[li], Q->q_v_hi[li]), two_inv);
            felt fo = f_mul(f_sub(Q->q_v_lo[li], Q->q_v_hi[li]), f_mul(two_inv, f_inv(x_lo_layer)));
            folded = f_add(fe, f_mul(fri_alphas[li + 1], fo));

            if (li + 1 < expected_layers) {
                uint32_t cur_size_next = 1u << (cur_log - 1);
                uint32_t nh = cur_size_next >> 1;
                felt expected = (plo < nh) ? Q->q_v_lo[li + 1] : Q->q_v_hi[li + 1];
                if (folded != expected) return 0;
            } else {
                if (folded != proof->fri_final_value) return 0;
            }
            cur_shift = f_mul(cur_shift, cur_shift);
            cur_log--;
        }
    }
    return 1;
}
