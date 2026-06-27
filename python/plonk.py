"""
PLONK with FRI polynomial commitments.

Non-interactive protocol (Fiat-Shamir) for proving satisfiability of a PLONK
circuit:

  Gate equation per row i:
     q_L*a + q_R*b + q_O*c + q_M*a*b + q_C  ==  0    (evaluated at omega^i)

  Copy constraints captured by a permutation sigma on positions {0..3n-1}
     position p = column*n + i,  column in {0,1,2} = {a,b,c}.

Rounds (after circuit-and-public-input absorbed):

  R1  prover: commit Merkle(a_evals on FRI domain), Merkle(b_evals), Merkle(c_evals)
  R2  beta, gamma <- transcript;  prover commits Merkle(Z_evals)
  R3  alpha       <- transcript;  prover builds C = gate + alpha*perm + alpha^2*boundary,
                                  divides by Z_H = X^n - 1, splits quotient
                                  t = t_lo + X^n * t_mid + X^{2n} * t_hi, commits each
  R4  zeta        <- transcript;  prover sends openings
                                       a(z), b(z), c(z), Z(z), Z(z*omega),
                                       t_lo(z), t_mid(z), t_hi(z)
  R5  rho         <- transcript;  prover runs DEEP-FRI on
                                       Q(X) = sum rho^k * (f_k(X) - f_k(z))/(X - z)
                                              + rho^last * (Z(X) - Z(z*omega))/(X - z*omega)
                                  verifier reconstructs Q(x) at each FRI query from f_k openings.

Public inputs: in this minimal implementation we bake them into q_C at the circuit
construction stage. A production version would expose them as a separate PI(X)
polynomial.
"""

from field import (
    P, add, sub, mul, neg, inv, pow_,
    primitive_root_of_unity, MULT_GEN,
    K1, K2,
)
from poly import poly_eval
from ntt import ntt, coset_ntt
from merkle import MerkleTree, verify_path
from transcript import Transcript
from fri import LOG_BLOWUP, NUM_QUERIES


# -------------------------------------------------------------------- helpers

def _ntt(coeffs):    return ntt(coeffs, inverse=False)
def _intt(values):   return ntt(values, inverse=True)


def _commit(coeffs, log_n):
    """Evaluate poly on the FRI coset domain (size n * 2^LOG_BLOWUP) and Merkle-commit."""
    evals = coset_ntt(coeffs, MULT_GEN, log_n + LOG_BLOWUP)
    tree = MerkleTree(evals)
    return evals, tree


def _powers(base, count):
    out = [1] * count
    for i in range(1, count):
        out[i] = mul(out[i - 1], base)
    return out


# ---------------------------------------------------------------- preprocessing

def setup(circuit):
    """
    Preprocess: turn the circuit's selectors and permutation into polynomial
    coefficient forms. This is what a trusted setup would output in a real
    deployment; here it's deterministic from the circuit.
    """
    log_n = circuit["log_n"]
    n = 1 << log_n

    for key in ("q_L", "q_R", "q_O", "q_M", "q_C"):
        assert len(circuit[key]) == n, "{} length mismatch".format(key)
    assert len(circuit["permutation"]) == 3 * n

    # Selectors as polynomials of degree < n
    selectors = {key: _intt(circuit[key]) for key in ("q_L", "q_R", "q_O", "q_M", "q_C")}

    # Build S_sigma1, S_sigma2, S_sigma3.
    # Position p = col*n + i  ->  permutation maps p -> sigma(p) = col'*n + i'
    # Encoding:  S_sigma{col+1}(omega^i) = k_factor[col'] * omega^(i')
    omega = primitive_root_of_unity(log_n)
    k_factor = (1, K1, K2)
    omega_pow = _powers(omega, n)

    s_values = [[0] * n for _ in range(3)]
    sigma = circuit["permutation"]
    for col in range(3):
        for i in range(n):
            p = col * n + i
            q = sigma[p]
            assert 0 <= q < 3 * n, "permutation out of range"
            col_prime, i_prime = q // n, q % n
            s_values[col][i] = mul(k_factor[col_prime], omega_pow[i_prime])

    sigma_polys = [_intt(s_values[c]) for c in range(3)]

    return {
        "log_n": log_n,
        "n": n,
        "omega": omega,
        "selectors": selectors,
        "S1": sigma_polys[0],
        "S2": sigma_polys[1],
        "S3": sigma_polys[2],
    }


# -------------------------------------------------------------------- prover

def prove(pp, witness, label=b"plonk-fri"):
    """
    Generate a proof for witness columns a, b, c with respect to preprocessed pp.
    """
    log_n = pp["log_n"]
    n = pp["n"]
    omega = pp["omega"]
    log_N = log_n + LOG_BLOWUP
    N = 1 << log_N
    blowup = 1 << LOG_BLOWUP

    a_vals = list(witness["a"])
    b_vals = list(witness["b"])
    c_vals = list(witness["c"])
    assert len(a_vals) == n and len(b_vals) == n and len(c_vals) == n

    transcript = Transcript(label)

    # ---- ROUND 1: commit witness polys
    a_coef = _intt(a_vals)
    b_coef = _intt(b_vals)
    c_coef = _intt(c_vals)
    a_evals, a_tree = _commit(a_coef, log_n)
    b_evals, b_tree = _commit(b_coef, log_n)
    c_evals, c_tree = _commit(c_coef, log_n)
    transcript.absorb("commit_a", a_tree.root)
    transcript.absorb("commit_b", b_tree.root)
    transcript.absorb("commit_c", c_tree.root)

    # ---- ROUND 2: permutation poly Z
    beta = transcript.challenge_field("beta")
    gamma = transcript.challenge_field("gamma")

    omega_pow = _powers(omega, n)
    # Evaluate S1, S2, S3 on H (these are the original column labels under sigma).
    S1_on_H = _ntt(pp["S1"])
    S2_on_H = _ntt(pp["S2"])
    S3_on_H = _ntt(pp["S3"])

    Z_on_H = [1] * n  # Z(omega^0) = 1
    for i in range(n - 1):
        wi = omega_pow[i]
        num = mul(
            mul(
                add(add(a_vals[i], mul(beta, wi)), gamma),
                add(add(b_vals[i], mul(beta, mul(K1, wi))), gamma),
            ),
            add(add(c_vals[i], mul(beta, mul(K2, wi))), gamma),
        )
        den = mul(
            mul(
                add(add(a_vals[i], mul(beta, S1_on_H[i])), gamma),
                add(add(b_vals[i], mul(beta, S2_on_H[i])), gamma),
            ),
            add(add(c_vals[i], mul(beta, S3_on_H[i])), gamma),
        )
        Z_on_H[i + 1] = mul(Z_on_H[i], mul(num, inv(den)))

    # Sanity check: telescope should land back at 1
    wi = omega_pow[n - 1]
    last_num = mul(
        mul(
            add(add(a_vals[n - 1], mul(beta, wi)), gamma),
            add(add(b_vals[n - 1], mul(beta, mul(K1, wi))), gamma),
        ),
        add(add(c_vals[n - 1], mul(beta, mul(K2, wi))), gamma),
    )
    last_den = mul(
        mul(
            add(add(a_vals[n - 1], mul(beta, S1_on_H[n - 1])), gamma),
            add(add(b_vals[n - 1], mul(beta, S2_on_H[n - 1])), gamma),
        ),
        add(add(c_vals[n - 1], mul(beta, S3_on_H[n - 1])), gamma),
    )
    telescope_check = mul(Z_on_H[n - 1], mul(last_num, inv(last_den)))
    assert telescope_check == 1, "permutation argument doesn't telescope; copy constraints violated"

    Z_coef = _intt(Z_on_H)
    Z_evals, Z_tree = _commit(Z_coef, log_n)
    transcript.absorb("commit_Z", Z_tree.root)

    # ---- ROUND 3: quotient polynomial t
    alpha = transcript.challenge_field("alpha")

    # Evaluate everything on the FRI extended domain.
    # Selectors and S_sigma's evals on FRI domain
    qL_ext = coset_ntt(pp["selectors"]["q_L"], MULT_GEN, log_N)
    qR_ext = coset_ntt(pp["selectors"]["q_R"], MULT_GEN, log_N)
    qO_ext = coset_ntt(pp["selectors"]["q_O"], MULT_GEN, log_N)
    qM_ext = coset_ntt(pp["selectors"]["q_M"], MULT_GEN, log_N)
    qC_ext = coset_ntt(pp["selectors"]["q_C"], MULT_GEN, log_N)
    S1_ext = coset_ntt(pp["S1"], MULT_GEN, log_N)
    S2_ext = coset_ntt(pp["S2"], MULT_GEN, log_N)
    S3_ext = coset_ntt(pp["S3"], MULT_GEN, log_N)

    # X-values on the FRI domain (we'll need these for perm constraint)
    omega_N = primitive_root_of_unity(log_N)
    X_ext = [0] * N
    cur = MULT_GEN
    for i in range(N):
        X_ext[i] = cur
        cur = mul(cur, omega_N)

    # Z(X*omega) on FRI domain: circular shift by blowup positions
    Z_shifted_ext = [Z_evals[(i + blowup) % N] for i in range(N)]

    # Z_H(X) = X^n - 1 on FRI domain
    one = 1
    Z_H_ext = [sub(pow_(x, n), one) for x in X_ext]

    # L_0(X) = (X^n - 1) / (n * (X - 1)) on FRI domain
    n_inv = inv(n)
    L0_ext = [
        mul(mul(Z_H_ext[i], n_inv), inv(sub(X_ext[i], one)))
        for i in range(N)
    ]

    alpha2 = mul(alpha, alpha)

    # Compute C on FRI domain pointwise
    C_ext = [0] * N
    for i in range(N):
        a, b, c = a_evals[i], b_evals[i], c_evals[i]
        Zi = Z_evals[i]
        Zwi = Z_shifted_ext[i]
        x = X_ext[i]
        # gate
        gate = add(
            add(
                add(mul(qL_ext[i], a), mul(qR_ext[i], b)),
                mul(qO_ext[i], c),
            ),
            add(mul(mul(qM_ext[i], a), b), qC_ext[i]),
        )
        # permutation
        perm_num = mul(
            mul(
                add(add(a, mul(beta, x)), gamma),
                add(add(b, mul(beta, mul(K1, x))), gamma),
            ),
            add(add(c, mul(beta, mul(K2, x))), gamma),
        )
        perm_den = mul(
            mul(
                add(add(a, mul(beta, S1_ext[i])), gamma),
                add(add(b, mul(beta, S2_ext[i])), gamma),
            ),
            add(add(c, mul(beta, S3_ext[i])), gamma),
        )
        perm = sub(mul(perm_num, Zi), mul(perm_den, Zwi))
        # boundary
        boundary = mul(sub(Zi, one), L0_ext[i])

        C_ext[i] = add(add(gate, mul(alpha, perm)), mul(alpha2, boundary))

    # t = C / Z_H pointwise on the coset (Z_H never vanishes on the coset because
    # MULT_GEN is not in H).
    t_ext = [mul(C_ext[i], inv(Z_H_ext[i])) for i in range(N)]

    # Convert t to coefficient form (it has degree < 3n by construction)
    # and split into t_lo, t_mid, t_hi.
    t_coef_full = [0] * N  # will use only first 3n coefficients
    # Inverse coset NTT
    from ntt import coset_intt
    t_coef_raw = coset_intt(t_ext, MULT_GEN, log_N)
    # Trim: anything beyond index 3n should be zero (sanity check)
    high = max((abs(0) for c in t_coef_raw[3 * n:]), default=0)
    for c in t_coef_raw[3 * n:]:
        assert c == 0, "quotient has degree >= 3n; constraint failed"
    t_lo = t_coef_raw[0:n]
    t_mid = t_coef_raw[n:2 * n]
    t_hi = t_coef_raw[2 * n:3 * n]

    t_lo_evals,  t_lo_tree  = _commit(t_lo,  log_n)
    t_mid_evals, t_mid_tree = _commit(t_mid, log_n)
    t_hi_evals,  t_hi_tree  = _commit(t_hi,  log_n)
    transcript.absorb("commit_t_lo",  t_lo_tree.root)
    transcript.absorb("commit_t_mid", t_mid_tree.root)
    transcript.absorb("commit_t_hi",  t_hi_tree.root)

    # ---- ROUND 4: evaluation challenge
    zeta = transcript.challenge_field("zeta")
    zeta_omega = mul(zeta, omega)

    open_a    = poly_eval(a_coef,  zeta)
    open_b    = poly_eval(b_coef,  zeta)
    open_c    = poly_eval(c_coef,  zeta)
    open_Z    = poly_eval(Z_coef,  zeta)
    open_Zw   = poly_eval(Z_coef,  zeta_omega)
    open_t_lo  = poly_eval(t_lo,  zeta)
    open_t_mid = poly_eval(t_mid, zeta)
    open_t_hi  = poly_eval(t_hi,  zeta)

    transcript.absorb("openings", [
        open_a, open_b, open_c, open_Z, open_Zw, open_t_lo, open_t_mid, open_t_hi,
    ])

    # ---- ROUND 5: batched DEEP quotient + FRI
    rho = transcript.challenge_field("rho")

    # The 7 polynomials opened at zeta, plus Z opened at zeta*omega.
    polys_at_zeta = [
        (a_evals,    open_a),
        (b_evals,    open_b),
        (c_evals,    open_c),
        (Z_evals,    open_Z),
        (t_lo_evals,  open_t_lo),
        (t_mid_evals, open_t_mid),
        (t_hi_evals,  open_t_hi),
    ]
    # Z at zeta*omega (extra)
    polys_at_zeta_omega = [
        (Z_evals,    open_Zw),
    ]

    # Q(x) = sum_k rho^k * (f_k(x) - v_k) / (x - zeta)
    #      + rho^k' * (Z(x) - z_omega) / (x - zeta*omega)
    # Build evaluations of Q on FRI domain.
    rho_pow = _powers(rho, len(polys_at_zeta) + len(polys_at_zeta_omega))

    Q_ext = [0] * N
    inv_x_minus_zeta       = [inv(sub(X_ext[i], zeta))       for i in range(N)]
    inv_x_minus_zeta_omega = [inv(sub(X_ext[i], zeta_omega)) for i in range(N)]

    for k, (evals, v) in enumerate(polys_at_zeta):
        rk = rho_pow[k]
        for i in range(N):
            term = mul(sub(evals[i], v), inv_x_minus_zeta[i])
            Q_ext[i] = add(Q_ext[i], mul(rk, term))

    offset = len(polys_at_zeta)
    for k, (evals, v) in enumerate(polys_at_zeta_omega):
        rk = rho_pow[offset + k]
        for i in range(N):
            term = mul(sub(evals[i], v), inv_x_minus_zeta_omega[i])
            Q_ext[i] = add(Q_ext[i], mul(rk, term))

    # FRI on Q. NOTE: we skip the layer-0 commitment for Q because the verifier
    # reconstructs Q(x) at any queried position from the openings of f_k below.
    # So we directly start folding.
    Q_layer_trees = []
    Q_layer_evals = []

    current = Q_ext
    current_shift = MULT_GEN
    current_log = log_N

    while current_log > 0:
        alpha_fri = transcript.challenge_field("fri_alpha")
        half = len(current) >> 1
        omega_layer = primitive_root_of_unity(current_log)
        two_inv = inv(2)
        new_layer = [0] * half
        x = current_shift
        for i in range(half):
            f_pos = current[i]
            f_neg = current[i + half]
            f_even = mul(add(f_pos, f_neg), two_inv)
            f_odd  = mul(sub(f_pos, f_neg), mul(two_inv, inv(x)))
            new_layer[i] = add(f_even, mul(alpha_fri, f_odd))
            x = mul(x, omega_layer)
        current = new_layer
        current_shift = mul(current_shift, current_shift)
        current_log -= 1

        if current_log > 0:
            tree = MerkleTree(current)
            Q_layer_trees.append(tree)
            Q_layer_evals.append(current)
            transcript.absorb("fri_root", tree.root)

    final_value = current[0]
    transcript.absorb("fri_final", final_value)

    # ---- QUERIES
    # For each query we also open f_k at the layer-0 positions p_lo, p_hi
    # so the verifier can reconstruct Q(x_lo), Q(x_hi).
    all_layer0_committed = [
        a_tree, b_tree, c_tree, Z_tree,
        t_lo_tree, t_mid_tree, t_hi_tree,
    ]
    all_layer0_evals = [
        a_evals, b_evals, c_evals, Z_evals,
        t_lo_evals, t_mid_evals, t_hi_evals,
    ]

    queries = []
    for _ in range(NUM_QUERIES):
        q0 = transcript.challenge_int(N, "fri_query")

        # Compute folding path positions
        # We need at layer 0 the pair (p_lo_0, p_hi_0), then descend.
        path_positions = []
        cur_q = q0
        cur_n = N
        for _layer in range(log_N):
            half = cur_n >> 1
            p_lo = cur_q % half
            p_hi = p_lo + half
            path_positions.append((p_lo, p_hi))
            cur_q = p_lo
            cur_n = half

        # Open each f_k at layer-0 positions p_lo_0 and p_hi_0
        p_lo_0, p_hi_0 = path_positions[0]
        f_openings = []
        for tree, evals in zip(all_layer0_committed, all_layer0_evals):
            f_openings.append({
                "v_lo": evals[p_lo_0],
                "v_hi": evals[p_hi_0],
                "path_lo": tree.open(p_lo_0),
                "path_hi": tree.open(p_hi_0),
            })

        # Open each Q layer at its (p_lo, p_hi). Layer 0 is implicit (no Merkle).
        Q_openings = []
        for layer_i in range(len(Q_layer_trees)):
            # Q_layer_trees[layer_i] corresponds to "Q after fold #(layer_i+1)",
            # which sits at position path_positions[layer_i + 1].
            p_lo, p_hi = path_positions[layer_i + 1]
            tree = Q_layer_trees[layer_i]
            evals = Q_layer_evals[layer_i]
            Q_openings.append({
                "v_lo": evals[p_lo],
                "v_hi": evals[p_hi],
                "path_lo": tree.open(p_lo),
                "path_hi": tree.open(p_hi),
            })

        queries.append({
            "q0": q0,
            "f_openings": f_openings,
            "Q_openings": Q_openings,
        })

    proof = {
        "commits": {
            "a": a_tree.root, "b": b_tree.root, "c": c_tree.root,
            "Z": Z_tree.root,
            "t_lo": t_lo_tree.root, "t_mid": t_mid_tree.root, "t_hi": t_hi_tree.root,
        },
        "openings": {
            "a": open_a, "b": open_b, "c": open_c,
            "Z": open_Z, "Zw": open_Zw,
            "t_lo": open_t_lo, "t_mid": open_t_mid, "t_hi": open_t_hi,
        },
        "fri": {
            "Q_layer_roots": [t.root for t in Q_layer_trees],
            "final_value": final_value,
            "queries": queries,
        },
    }
    return proof


# -------------------------------------------------------------------- verifier

def verify(pp, proof, label=b"plonk-fri"):
    log_n = pp["log_n"]
    n = pp["n"]
    omega = pp["omega"]
    log_N = log_n + LOG_BLOWUP
    N = 1 << log_N
    blowup = 1 << LOG_BLOWUP

    transcript = Transcript(label)

    # Replay all challenges
    transcript.absorb("commit_a", proof["commits"]["a"])
    transcript.absorb("commit_b", proof["commits"]["b"])
    transcript.absorb("commit_c", proof["commits"]["c"])
    beta = transcript.challenge_field("beta")
    gamma = transcript.challenge_field("gamma")
    transcript.absorb("commit_Z", proof["commits"]["Z"])
    alpha = transcript.challenge_field("alpha")
    transcript.absorb("commit_t_lo",  proof["commits"]["t_lo"])
    transcript.absorb("commit_t_mid", proof["commits"]["t_mid"])
    transcript.absorb("commit_t_hi",  proof["commits"]["t_hi"])
    zeta = transcript.challenge_field("zeta")
    zeta_omega = mul(zeta, omega)
    o = proof["openings"]
    transcript.absorb("openings", [
        o["a"], o["b"], o["c"], o["Z"], o["Zw"], o["t_lo"], o["t_mid"], o["t_hi"],
    ])
    rho = transcript.challenge_field("rho")

    # ---- Step A: check the PLONK identity at zeta
    # Evaluate selectors and S_sigma at zeta locally
    qL_z = poly_eval(pp["selectors"]["q_L"], zeta)
    qR_z = poly_eval(pp["selectors"]["q_R"], zeta)
    qO_z = poly_eval(pp["selectors"]["q_O"], zeta)
    qM_z = poly_eval(pp["selectors"]["q_M"], zeta)
    qC_z = poly_eval(pp["selectors"]["q_C"], zeta)
    S1_z = poly_eval(pp["S1"], zeta)
    S2_z = poly_eval(pp["S2"], zeta)
    S3_z = poly_eval(pp["S3"], zeta)

    a_z, b_z, c_z = o["a"], o["b"], o["c"]
    Z_z, Zw_z    = o["Z"], o["Zw"]
    tl, tm, th   = o["t_lo"], o["t_mid"], o["t_hi"]

    zeta_n = pow_(zeta, n)
    Z_H_z  = sub(zeta_n, 1)
    # L_0(zeta) = (zeta^n - 1) / (n * (zeta - 1))
    L0_z   = mul(Z_H_z, inv(mul(n, sub(zeta, 1))))

    gate_z = add(
        add(
            add(mul(qL_z, a_z), mul(qR_z, b_z)),
            mul(qO_z, c_z),
        ),
        add(mul(mul(qM_z, a_z), b_z), qC_z),
    )
    perm_num_z = mul(
        mul(
            add(add(a_z, mul(beta, zeta)), gamma),
            add(add(b_z, mul(beta, mul(K1, zeta))), gamma),
        ),
        add(add(c_z, mul(beta, mul(K2, zeta))), gamma),
    )
    perm_den_z = mul(
        mul(
            add(add(a_z, mul(beta, S1_z)), gamma),
            add(add(b_z, mul(beta, S2_z)), gamma),
        ),
        add(add(c_z, mul(beta, S3_z)), gamma),
    )
    perm_z = sub(mul(perm_num_z, Z_z), mul(perm_den_z, Zw_z))
    boundary_z = mul(sub(Z_z, 1), L0_z)

    C_z = add(add(gate_z, mul(alpha, perm_z)), mul(mul(alpha, alpha), boundary_z))

    # t(zeta) = t_lo + zeta^n * t_mid + zeta^{2n} * t_hi
    t_z = add(add(tl, mul(zeta_n, tm)), mul(mul(zeta_n, zeta_n), th))

    if C_z != mul(t_z, Z_H_z):
        return False

    # ---- Step B: verify batched DEEP-FRI

    # Replay FRI fold challenges
    fri_alphas = []
    Q_roots = proof["fri"]["Q_layer_roots"]
    # log_N folds; the first fold's alpha precedes any Q-layer root absorption
    # because Q layer-0 is implicit.
    expected_layer_count = log_N - 1  # we commit Q layers 1..log_N-1 (size >=2)
    if len(Q_roots) != expected_layer_count:
        return False

    for i in range(log_N):
        fri_alphas.append(transcript.challenge_field("fri_alpha"))
        if i < expected_layer_count:
            transcript.absorb("fri_root", Q_roots[i])
    transcript.absorb("fri_final", proof["fri"]["final_value"])

    # rho powers for reconstructing Q
    rho_pow = _powers(rho, 8)  # 7 at zeta + 1 at zeta_omega = 8

    # Precompute omega_N and X(i) on demand
    omega_N = primitive_root_of_unity(log_N)

    two_inv = inv(2)

    if len(proof["fri"]["queries"]) != NUM_QUERIES:
        return False

    # Layer-0 polynomial commitment roots, in the order the prover used
    f_commit_roots = [
        proof["commits"]["a"], proof["commits"]["b"], proof["commits"]["c"],
        proof["commits"]["Z"],
        proof["commits"]["t_lo"], proof["commits"]["t_mid"], proof["commits"]["t_hi"],
    ]
    # The values claimed at zeta for each (the 7 polys opened at zeta)
    f_open_values = [
        o["a"], o["b"], o["c"], o["Z"], o["t_lo"], o["t_mid"], o["t_hi"],
    ]
    # For the (single) Z opening at zeta_omega we use index 3 in f_commit_roots / f_openings
    Z_index = 3

    for q_idx in range(NUM_QUERIES):
        q0 = transcript.challenge_int(N, "fri_query")
        query = proof["fri"]["queries"][q_idx]
        if query["q0"] != q0:
            return False

        # Recompute path positions
        path_positions = []
        cur_q = q0
        cur_n = N
        for _layer in range(log_N):
            half = cur_n >> 1
            p_lo = cur_q % half
            p_hi = p_lo + half
            path_positions.append((p_lo, p_hi))
            cur_q = p_lo
            cur_n = half

        # --- Layer 0: verify f_k openings, reconstruct Q(x_lo) and Q(x_hi)
        p_lo_0, p_hi_0 = path_positions[0]
        f_openings = query["f_openings"]
        if len(f_openings) != 7:
            return False

        for idx, root in enumerate(f_commit_roots):
            opn = f_openings[idx]
            if not verify_path(root, opn["v_lo"], p_lo_0, opn["path_lo"]):
                return False
            if not verify_path(root, opn["v_hi"], p_hi_0, opn["path_hi"]):
                return False

        # X values at the two layer-0 positions
        x_lo_0 = mul(MULT_GEN, pow_(omega_N, p_lo_0))
        x_hi_0 = mul(MULT_GEN, pow_(omega_N, p_hi_0))

        # Q(x_lo_0), Q(x_hi_0) reconstructed from openings:
        #   contributions at zeta from each of the 7 polys
        #   plus contribution at zeta_omega from Z (using same v as opening but value Zw)
        Q_lo = 0
        Q_hi = 0
        inv_xl_z = inv(sub(x_lo_0, zeta))
        inv_xh_z = inv(sub(x_hi_0, zeta))
        for k in range(7):
            v = f_open_values[k]
            rk = rho_pow[k]
            term_lo = mul(sub(f_openings[k]["v_lo"], v), inv_xl_z)
            term_hi = mul(sub(f_openings[k]["v_hi"], v), inv_xh_z)
            Q_lo = add(Q_lo, mul(rk, term_lo))
            Q_hi = add(Q_hi, mul(rk, term_hi))
        # Z at zeta_omega
        inv_xl_zw = inv(sub(x_lo_0, zeta_omega))
        inv_xh_zw = inv(sub(x_hi_0, zeta_omega))
        v_zw = o["Zw"]
        rk = rho_pow[7]
        Q_lo = add(Q_lo, mul(rk, mul(sub(f_openings[Z_index]["v_lo"], v_zw), inv_xl_zw)))
        Q_hi = add(Q_hi, mul(rk, mul(sub(f_openings[Z_index]["v_hi"], v_zw), inv_xh_zw)))

        # Folding from layer 0 to layer 1
        # alpha_0 was challenged BEFORE any Q-layer-root absorb
        # so fri_alphas[0] is the layer-0 -> layer-1 fold challenge
        x_lo = x_lo_0
        alpha0 = fri_alphas[0]
        f_even = mul(add(Q_lo, Q_hi), two_inv)
        f_odd  = mul(sub(Q_lo, Q_hi), mul(two_inv, inv(x_lo)))
        folded = add(f_even, mul(alpha0, f_odd))

        # Check against layer 1 opening (if any) or final value
        if expected_layer_count == 0:
            # No intermediate layers -> folded should equal final value
            if folded != proof["fri"]["final_value"]:
                return False
        else:
            # layer 1 is Q_openings[0]
            Q_layer = query["Q_openings"]
            if len(Q_layer) != expected_layer_count:
                return False
            # check which side of layer 1's pair matches our folded
            p_lo_1, p_hi_1 = path_positions[1]
            opn = Q_layer[0]
            if not verify_path(Q_roots[0], opn["v_lo"], p_lo_1, opn["path_lo"]):
                return False
            if not verify_path(Q_roots[0], opn["v_hi"], p_hi_1, opn["path_hi"]):
                return False
            # The folded value sits at position p_lo_0 within layer 1 of size N/2.
            # Layer 1's "lo" index in its (size N/2) layer is p_lo_0 % (N/4).
            # If p_lo_0 < N/4 then folded == v_lo of layer 1, else == v_hi.
            next_half = (N >> 1) >> 1  # N/4
            if p_lo_0 < next_half:
                if folded != opn["v_lo"]:
                    return False
            else:
                if folded != opn["v_hi"]:
                    return False

            # Continue folding through layers 1..expected_layer_count-1
            cur_shift = mul(MULT_GEN, MULT_GEN)  # after one squaring
            cur_log = log_N - 1
            cur_q_for_layer = p_lo_0
            for li in range(expected_layer_count):
                opn = Q_layer[li]
                p_lo, p_hi = path_positions[li + 1]
                if li > 0:
                    # already verified for li=0 above
                    if not verify_path(Q_roots[li], opn["v_lo"], p_lo, opn["path_lo"]):
                        return False
                    if not verify_path(Q_roots[li], opn["v_hi"], p_hi, opn["path_hi"]):
                        return False

                # Fold layer (li) -> layer (li+1)
                omega_layer = primitive_root_of_unity(cur_log)
                x_lo_layer = mul(cur_shift, pow_(omega_layer, p_lo))
                f_even = mul(add(opn["v_lo"], opn["v_hi"]), two_inv)
                f_odd  = mul(sub(opn["v_lo"], opn["v_hi"]), mul(two_inv, inv(x_lo_layer)))
                folded = add(f_even, mul(fri_alphas[li + 1], f_odd))

                if li + 1 < expected_layer_count:
                    # check against next layer's opening
                    nxt = Q_layer[li + 1]
                    cur_size_next = 1 << (cur_log - 1)
                    next_half = cur_size_next >> 1
                    if p_lo < next_half:
                        expected = nxt["v_lo"]
                    else:
                        expected = nxt["v_hi"]
                    if folded != expected:
                        return False
                else:
                    # last commit layer; next fold lands at final value
                    if folded != proof["fri"]["final_value"]:
                        return False

                cur_shift = mul(cur_shift, cur_shift)
                cur_log -= 1
                cur_q_for_layer = p_lo

    return True
