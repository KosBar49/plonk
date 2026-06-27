/*
 * prover.c -- PLONK+FRI prover, static allocation, mirrors plonk.py:prove().
 *
 * All large working buffers are file-scope static arrays (no malloc). The
 * footprint is reported by prover_footprint().
 */
#include "prover.h"
#include "ntt.h"
#include "transcript.h"
#include <string.h>
#include <stdio.h>

/* ---- static working buffers (the prover's whole RAM cost) ---------------- */

/* witness coefficient forms */
static felt a_coef[MAX_N], b_coef[MAX_N], c_coef[MAX_N];
/* evaluations on the FRI domain (size NEXT) for the 7 committed polynomials */
static felt a_ev[MAX_NEXT], b_ev[MAX_NEXT], c_ev[MAX_NEXT];
static felt Z_ev[MAX_NEXT], tlo_ev[MAX_NEXT], tmid_ev[MAX_NEXT], thi_ev[MAX_NEXT];
/* permutation grand-product poly */
static felt Z_on_H[MAX_N], Z_coef[MAX_N];
static felt S1_onH[MAX_N], S2_onH[MAX_N], S3_onH[MAX_N];
/* selector/sigma evaluations on FRI domain */
static felt qL_e[MAX_NEXT], qR_e[MAX_NEXT], qO_e[MAX_NEXT], qM_e[MAX_NEXT], qC_e[MAX_NEXT];
static felt S1_e[MAX_NEXT], S2_e[MAX_NEXT], S3_e[MAX_NEXT];
/* domain helpers on FRI domain */
static felt X_e[MAX_NEXT], ZH_e[MAX_NEXT], L0_e[MAX_NEXT], Zsh_e[MAX_NEXT];
/* composition / quotient / DEEP buffers */
static felt C_e[MAX_NEXT], t_e[MAX_NEXT], Q_e[MAX_NEXT];
static felt t_coef[MAX_NEXT];           /* full inverse-coset result */
static felt tlo[MAX_N], tmid[MAX_N], thi[MAX_N];
/* scratch */
static felt scratch[MAX_NEXT];

/* ONE shared Merkle-tree buffer, reused for every commitment and rebuilt on
 * demand at query time from retained leaf data (poly-major query phase). This
 * trades a one-time tree rebuild per polynomial for ~7x less RAM. */
static uint8_t shared_tree_store[2 * MAX_NEXT * HASH_BYTES];
static merkle_tree_t shared_tree;
/* FRI layer evaluations packed into a flat pool (sum of shrinking layers < NEXT). */
static felt    fri_ev_pool[MAX_NEXT];
static uint32_t fri_ev_off[MAX_LOG_NEXT];
static uint32_t fri_ev_len[MAX_LOG_NEXT];

/* ---- helpers ------------------------------------------------------------- */

/* forward decl: rounds 3-5 live below prove() */
static int prove_rounds_3plus(const setup_t *pp, proof_t *proof,
                              transcript_t *tr, felt beta, felt gamma,
                              uint8_t log_n, uint32_t n, felt omega,
                              uint8_t log_NEXT, uint32_t NEXT, uint32_t blowup);

void witness_demo(witness_t *w, uint64_t x) {
    uint64_t x2 = x * x, x3 = x2 * x, s1 = x3 + x, s2 = s1 + 5;
    felt A[8] = { f_from_u64(x),  f_from_u64(x2), f_from_u64(x3), f_from_u64(s1),
                  f_from_u64(5),  f_from_u64(s2), 0, 0 };
    felt B[8] = { f_from_u64(x),  f_from_u64(x),  f_from_u64(x),  f_from_u64(5),
                  0, 0, 0, 0 };
    felt C[8] = { f_from_u64(x2), f_from_u64(x3), f_from_u64(s1), f_from_u64(s2),
                  0, 0, 0, 0 };
    memset(w, 0, sizeof(*w));
    memcpy(w->a, A, sizeof(A));
    memcpy(w->b, B, sizeof(B));
    memcpy(w->c, C, sizeof(C));
}

/* commit: coeff -> coset eval on FRI domain -> Merkle tree. Writes evals to
 * `out_ev`, builds tree in shared buffer, copies root. */
static void commit(const felt *coef, uint8_t log_n, felt *out_ev,
                   uint8_t root_out[HASH_BYTES]) {
    uint8_t log_NEXT = log_n + LOG_BLOWUP;
    uint32_t NEXT = 1u << log_NEXT;
    coset_eval(coef, log_n, F_MULT_GEN, log_NEXT, out_ev);
    merkle_tree_build(&shared_tree, out_ev, NEXT, shared_tree_store);
    merkle_tree_root(&shared_tree, root_out);
}

/* ---- prover -------------------------------------------------------------- */

int prove(const setup_t *pp, const witness_t *w, proof_t *proof) {
    if (pp->log_n > MAX_LOG_N) return -1;
    const uint8_t log_n = pp->log_n;
    const uint32_t n = pp->n;
    const felt omega = pp->omega;
    const uint8_t log_NEXT = log_n + LOG_BLOWUP;
    const uint32_t NEXT = 1u << log_NEXT;
    const uint32_t blowup = 1u << LOG_BLOWUP;

    proof->log_n = log_n;
    proof->log_blowup = LOG_BLOWUP;
    proof->num_queries = NUM_QUERIES;

    transcript_t tr;
    transcript_init(&tr, "plonk-fri", 9);

    /* ===== ROUND 1: commit a, b, c ===== */
    memcpy(a_coef, w->a, n * sizeof(felt)); ntt_inplace(a_coef, log_n, 1);
    memcpy(b_coef, w->b, n * sizeof(felt)); ntt_inplace(b_coef, log_n, 1);
    memcpy(c_coef, w->c, n * sizeof(felt)); ntt_inplace(c_coef, log_n, 1);

    commit(a_coef, log_n, a_ev, proof->commits[0]);
    commit(b_coef, log_n, b_ev, proof->commits[1]);
    commit(c_coef, log_n, c_ev, proof->commits[2]);
    transcript_absorb_root(&tr, "commit_a", proof->commits[0]);
    transcript_absorb_root(&tr, "commit_b", proof->commits[1]);
    transcript_absorb_root(&tr, "commit_c", proof->commits[2]);

    /* ===== ROUND 2: permutation poly Z ===== */
    felt beta  = transcript_challenge_field(&tr, "beta");
    felt gamma = transcript_challenge_field(&tr, "gamma");

    felt omega_pow[MAX_N];
    powers(omega, omega_pow, n);

    /* S1,S2,S3 on H = ntt(coeff) */
    memcpy(S1_onH, pp->S1, n * sizeof(felt)); ntt_inplace(S1_onH, log_n, 0);
    memcpy(S2_onH, pp->S2, n * sizeof(felt)); ntt_inplace(S2_onH, log_n, 0);
    memcpy(S3_onH, pp->S3, n * sizeof(felt)); ntt_inplace(S3_onH, log_n, 0);

    Z_on_H[0] = 1;
    for (uint32_t i = 0; i + 1 < n; ++i) {
        felt wi = omega_pow[i];
        felt num = f_mul(
            f_mul(
                f_add(f_add(w->a[i], f_mul(beta, wi)), gamma),
                f_add(f_add(w->b[i], f_mul(beta, f_mul(F_K1, wi))), gamma)),
            f_add(f_add(w->c[i], f_mul(beta, f_mul(F_K2, wi))), gamma));
        felt den = f_mul(
            f_mul(
                f_add(f_add(w->a[i], f_mul(beta, S1_onH[i])), gamma),
                f_add(f_add(w->b[i], f_mul(beta, S2_onH[i])), gamma)),
            f_add(f_add(w->c[i], f_mul(beta, S3_onH[i])), gamma));
        Z_on_H[i + 1] = f_mul(Z_on_H[i], f_mul(num, f_inv(den)));
    }
    /* telescope check */
    {
        uint32_t i = n - 1;
        felt wi = omega_pow[i];
        felt num = f_mul(
            f_mul(
                f_add(f_add(w->a[i], f_mul(beta, wi)), gamma),
                f_add(f_add(w->b[i], f_mul(beta, f_mul(F_K1, wi))), gamma)),
            f_add(f_add(w->c[i], f_mul(beta, f_mul(F_K2, wi))), gamma));
        felt den = f_mul(
            f_mul(
                f_add(f_add(w->a[i], f_mul(beta, S1_onH[i])), gamma),
                f_add(f_add(w->b[i], f_mul(beta, S2_onH[i])), gamma)),
            f_add(f_add(w->c[i], f_mul(beta, S3_onH[i])), gamma));
        felt tele = f_mul(Z_on_H[i], f_mul(num, f_inv(den)));
        if (tele != 1) return 1;  /* copy constraints violated */
    }

    memcpy(Z_coef, Z_on_H, n * sizeof(felt));
    ntt_inplace(Z_coef, log_n, 1);
    commit(Z_coef, log_n, Z_ev, proof->commits[3]);
    transcript_absorb_root(&tr, "commit_Z", proof->commits[3]);

    /* round 3+ continue in prove_rounds_3plus (same file, below) */
    return prove_rounds_3plus(pp, proof, &tr, beta, gamma,
                              log_n, n, omega, log_NEXT, NEXT, blowup);
}

/* ===== ROUNDS 3-5 ===== */
static int prove_rounds_3plus(const setup_t *pp, proof_t *proof,
                              transcript_t *tr, felt beta, felt gamma,
                              uint8_t log_n, uint32_t n, felt omega,
                              uint8_t log_NEXT, uint32_t NEXT, uint32_t blowup) {
    /* ===== ROUND 3: quotient t ===== */
    felt alpha = transcript_challenge_field(tr, "alpha");
    felt alpha2 = f_mul(alpha, alpha);

    /* selectors + sigma on FRI domain */
    coset_eval(pp->qL, log_n, F_MULT_GEN, log_NEXT, qL_e);
    coset_eval(pp->qR, log_n, F_MULT_GEN, log_NEXT, qR_e);
    coset_eval(pp->qO, log_n, F_MULT_GEN, log_NEXT, qO_e);
    coset_eval(pp->qM, log_n, F_MULT_GEN, log_NEXT, qM_e);
    coset_eval(pp->qC, log_n, F_MULT_GEN, log_NEXT, qC_e);
    coset_eval(pp->S1, log_n, F_MULT_GEN, log_NEXT, S1_e);
    coset_eval(pp->S2, log_n, F_MULT_GEN, log_NEXT, S2_e);
    coset_eval(pp->S3, log_n, F_MULT_GEN, log_NEXT, S3_e);

    /* X on FRI domain, Z_H, L0, Z(X*omega) */
    felt omega_N = f_root_of_unity(log_NEXT);
    felt cur = F_MULT_GEN;
    for (uint32_t i = 0; i < NEXT; ++i) { X_e[i] = cur; cur = f_mul(cur, omega_N); }

    for (uint32_t i = 0; i < NEXT; ++i)
        Zsh_e[i] = Z_ev[(i + blowup) % NEXT];

    /* ZH_e[i] = X_e[i]^n - 1 is periodic with period blowup (omega_N^n has order blowup).
     * Compute blowup distinct values, precompute their inverses for t_e division. */
    felt ZH_base[1u << LOG_BLOWUP], inv_ZH[1u << LOG_BLOWUP];
    for (uint32_t j = 0; j < blowup; ++j) {
        ZH_base[j] = f_sub(f_pow(X_e[j], (uint64_t)n), 1);
        inv_ZH[j] = f_inv(ZH_base[j]);
    }
    for (uint32_t i = 0; i < NEXT; ++i)
        ZH_e[i] = ZH_base[i % blowup];

    felt n_inv = f_inv(f_from_u64((uint64_t)n));
    for (uint32_t i = 0; i < NEXT; ++i)
        L0_e[i] = f_mul(f_mul(ZH_e[i], n_inv), f_inv(f_sub(X_e[i], 1)));

    for (uint32_t i = 0; i < NEXT; ++i) {
        felt a = a_ev[i], b = b_ev[i], c = c_ev[i];
        felt Zi = Z_ev[i], Zwi = Zsh_e[i], x = X_e[i];
        felt gate = f_add(
            f_add(f_add(f_mul(qL_e[i], a), f_mul(qR_e[i], b)), f_mul(qO_e[i], c)),
            f_add(f_mul(f_mul(qM_e[i], a), b), qC_e[i]));
        felt pn = f_mul(
            f_mul(f_add(f_add(a, f_mul(beta, x)), gamma),
                  f_add(f_add(b, f_mul(beta, f_mul(F_K1, x))), gamma)),
            f_add(f_add(c, f_mul(beta, f_mul(F_K2, x))), gamma));
        felt pd = f_mul(
            f_mul(f_add(f_add(a, f_mul(beta, S1_e[i])), gamma),
                  f_add(f_add(b, f_mul(beta, S2_e[i])), gamma)),
            f_add(f_add(c, f_mul(beta, S3_e[i])), gamma));
        felt perm = f_sub(f_mul(pn, Zi), f_mul(pd, Zwi));
        felt boundary = f_mul(f_sub(Zi, 1), L0_e[i]);
        C_e[i] = f_add(f_add(gate, f_mul(alpha, perm)), f_mul(alpha2, boundary));
    }

    for (uint32_t i = 0; i < NEXT; ++i)
        t_e[i] = f_mul(C_e[i], inv_ZH[i % blowup]);

    coset_interp(t_e, F_MULT_GEN, log_NEXT, t_coef);
    /* high coeffs (>= 3n) must be zero */
    for (uint32_t i = 3 * n; i < NEXT; ++i)
        if (t_coef[i] != 0) return 2;  /* degree too high -> bug/constraint fail */

    for (uint32_t i = 0; i < n; ++i) {
        tlo[i]  = t_coef[i];
        tmid[i] = t_coef[n + i];
        thi[i]  = t_coef[2 * n + i];
    }
    commit(tlo, log_n, tlo_ev, proof->commits[4]);
    commit(tmid, log_n, tmid_ev, proof->commits[5]);
    commit(thi, log_n, thi_ev, proof->commits[6]);
    transcript_absorb_root(tr, "commit_t_lo",  proof->commits[4]);
    transcript_absorb_root(tr, "commit_t_mid", proof->commits[5]);
    transcript_absorb_root(tr, "commit_t_hi",  proof->commits[6]);

    /* ===== ROUND 4: openings at zeta ===== */
    felt zeta = transcript_challenge_field(tr, "zeta");
    felt zeta_omega = f_mul(zeta, omega);

    proof->open_a    = poly_eval(a_coef, n, zeta);
    proof->open_b    = poly_eval(b_coef, n, zeta);
    proof->open_c    = poly_eval(c_coef, n, zeta);
    proof->open_Z    = poly_eval(Z_coef, n, zeta);
    proof->open_Zw   = poly_eval(Z_coef, n, zeta_omega);
    proof->open_t_lo = poly_eval(tlo, n, zeta);
    proof->open_t_mid= poly_eval(tmid, n, zeta);
    proof->open_t_hi = poly_eval(thi, n, zeta);

    felt opens[8] = { proof->open_a, proof->open_b, proof->open_c, proof->open_Z,
                      proof->open_Zw, proof->open_t_lo, proof->open_t_mid, proof->open_t_hi };
    transcript_absorb_elems(tr, "openings", opens, 8);

    /* ===== ROUND 5: batched DEEP quotient + FRI ===== */
    felt rho = transcript_challenge_field(tr, "rho");
    felt rho_pow[8];
    powers(rho, rho_pow, 8);

    /* the 7 polys' evals and their zeta-openings */
    felt *ev7[7]  = { a_ev, b_ev, c_ev, Z_ev, tlo_ev, tmid_ev, thi_ev };
    felt  v7[7]   = { proof->open_a, proof->open_b, proof->open_c, proof->open_Z,
                      proof->open_t_lo, proof->open_t_mid, proof->open_t_hi };

    /* precompute 1/(X_e[i] - zeta) into scratch; reused across all 7 polys */
    for (uint32_t i = 0; i < NEXT; ++i)
        scratch[i] = f_inv(f_sub(X_e[i], zeta));

    memset(Q_e, 0, NEXT * sizeof(felt));
    for (int k = 0; k < 7; ++k) {
        felt rk = rho_pow[k], v = v7[k];
        const felt *ev = ev7[k];
        for (uint32_t i = 0; i < NEXT; ++i)
            Q_e[i] = f_add(Q_e[i], f_mul(rk, f_mul(f_sub(ev[i], v), scratch[i])));
    }
    /* Z at zeta*omega */
    {
        felt rk = rho_pow[7], v = proof->open_Zw;
        for (uint32_t i = 0; i < NEXT; ++i) {
            felt term = f_mul(f_sub(Z_ev[i], v), f_inv(f_sub(X_e[i], zeta_omega)));
            Q_e[i] = f_add(Q_e[i], f_mul(rk, term));
        }
    }

    /* FRI fold on Q. Layer 0 NOT committed (verifier reconstructs it). */
    uint8_t fri_count = 0;
    /* current layer in `scratch`, starting as Q_e */
    memcpy(scratch, Q_e, NEXT * sizeof(felt));
    felt cur_shift = F_MULT_GEN;
    uint8_t cur_log = log_NEXT;
    uint32_t cur_len = NEXT;
    felt two_inv = f_inv(2);

    while (cur_log > 0) {
        felt afri = transcript_challenge_field(tr, "fri_alpha");
        uint32_t half = cur_len >> 1;
        felt om = f_root_of_unity(cur_log);
        felt x = cur_shift;
        for (uint32_t i = 0; i < half; ++i) {
            felt fpos = scratch[i], fneg = scratch[i + half];
            felt feven = f_mul(f_add(fpos, fneg), two_inv);
            felt fodd  = f_mul(f_sub(fpos, fneg), f_mul(two_inv, f_inv(x)));
            scratch[i] = f_add(feven, f_mul(afri, fodd));
            x = f_mul(x, om);
        }
        cur_len = half;
        cur_shift = f_mul(cur_shift, cur_shift);
        cur_log--;

        if (cur_log > 0) {
            /* pack this layer's evals into the flat pool */
            uint32_t off = (fri_count == 0) ? 0 : (fri_ev_off[fri_count-1] + fri_ev_len[fri_count-1]);
            fri_ev_off[fri_count] = off;
            fri_ev_len[fri_count] = cur_len;
            memcpy(&fri_ev_pool[off], scratch, cur_len * sizeof(felt));
            merkle_tree_build(&shared_tree, &fri_ev_pool[off], cur_len, shared_tree_store);
            merkle_tree_root(&shared_tree, proof->q_layer_roots[fri_count]);
            transcript_absorb_root(tr, "fri_root", proof->q_layer_roots[fri_count]);
            fri_count++;
        }
    }
    proof->q_layer_count = fri_count;
    proof->fri_final_value = scratch[0];
    transcript_absorb_elem(tr, "fri_final", proof->fri_final_value);

    /* ===== QUERIES (poly-major to keep only ONE tree in RAM) ===== */
    /* First pass: derive every query's q0 and path positions. */
    static uint32_t pos_lo[NUM_QUERIES][MAX_LOG_NEXT];
    static uint32_t pos_hi[NUM_QUERIES][MAX_LOG_NEXT];
    for (int qi = 0; qi < NUM_QUERIES; ++qi) {
        uint32_t q0 = transcript_challenge_int(tr, "fri_query", NEXT);
        proof->queries[qi].q0 = q0;
        uint32_t cq = q0, cn = NEXT;
        for (uint8_t L = 0; L < log_NEXT; ++L) {
            uint32_t h = cn >> 1;
            pos_lo[qi][L] = cq % h; pos_hi[qi][L] = pos_lo[qi][L] + h;
            cq = pos_lo[qi][L]; cn = h;
        }
    }

    /* Layer-0 polys: rebuild each tree once, open all queries against it. */
    for (int k = 0; k < 7; ++k) {
        merkle_tree_build(&shared_tree, ev7[k], NEXT, shared_tree_store);
        for (int qi = 0; qi < NUM_QUERIES; ++qi) {
            query_t *Q = &proof->queries[qi];
            uint32_t plo = pos_lo[qi][0], phi = pos_hi[qi][0];
            Q->f_v_lo[k] = ev7[k][plo];
            Q->f_v_hi[k] = ev7[k][phi];
            merkle_tree_open(&shared_tree, plo, Q->f_path_lo[k]);
            merkle_tree_open(&shared_tree, phi, Q->f_path_hi[k]);
        }
    }

    /* FRI layers: rebuild each layer tree once from the flat pool, open all queries. */
    for (uint8_t li = 0; li < fri_count; ++li) {
        merkle_tree_build(&shared_tree, &fri_ev_pool[fri_ev_off[li]], fri_ev_len[li], shared_tree_store);
        for (int qi = 0; qi < NUM_QUERIES; ++qi) {
            query_t *Q = &proof->queries[qi];
            uint32_t plo = pos_lo[qi][li + 1], phi = pos_hi[qi][li + 1];
            Q->q_v_lo[li] = fri_ev_pool[fri_ev_off[li] + plo];
            Q->q_v_hi[li] = fri_ev_pool[fri_ev_off[li] + phi];
            merkle_tree_open(&shared_tree, plo, Q->q_path_lo[li]);
            merkle_tree_open(&shared_tree, phi, Q->q_path_hi[li]);
        }
    }

    return 0;
}

void prover_footprint(void) {
    size_t felt_bytes =
        sizeof(a_coef) + sizeof(b_coef) + sizeof(c_coef) +
        sizeof(a_ev) + sizeof(b_ev) + sizeof(c_ev) +
        sizeof(Z_ev) + sizeof(tlo_ev) + sizeof(tmid_ev) + sizeof(thi_ev) +
        sizeof(Z_on_H) + sizeof(Z_coef) +
        sizeof(S1_onH) + sizeof(S2_onH) + sizeof(S3_onH) +
        sizeof(qL_e) + sizeof(qR_e) + sizeof(qO_e) + sizeof(qM_e) + sizeof(qC_e) +
        sizeof(S1_e) + sizeof(S2_e) + sizeof(S3_e) +
        sizeof(X_e) + sizeof(ZH_e) + sizeof(L0_e) + sizeof(Zsh_e) +
        sizeof(C_e) + sizeof(t_e) + sizeof(Q_e) +
        sizeof(t_coef) + sizeof(tlo) + sizeof(tmid) + sizeof(thi) +
        sizeof(scratch) + sizeof(fri_ev_pool);
    size_t tree_bytes = (size_t)2 * MAX_NEXT * HASH_BYTES;  /* ONE shared tree */
    printf("Prover static footprint (MAX_LOG_N=%d, field=%s, felt=%luB):\n",
           MAX_LOG_N, FIELD_NAME, (unsigned long)sizeof(felt));
    printf("  felt buffers : ~%lu KB\n", (unsigned long)(felt_bytes / 1024));
    printf("  merkle trees : ~%lu KB\n", (unsigned long)(tree_bytes / 1024));
    printf("  TOTAL        : ~%lu KB\n", (unsigned long)((felt_bytes + tree_bytes) / 1024));
}
