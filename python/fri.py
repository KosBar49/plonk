"""
FRI — Fast Reed-Solomon Interactive Oracle Proof of Proximity.

Goal: convince a verifier that a function f : D -> F is close to a polynomial of
degree < n, given only Merkle-committed evaluations.

Protocol (made non-interactive via Fiat-Shamir):

  Commit phase
    Let D_0 = coset shift * <omega>, size N = n * blowup.
    Layer 0 = evaluations of f on D_0; Merkle-commit, absorb root.
    For each layer i with size > 1:
        verifier sends folding challenge alpha_i,
        prover folds:  f_{i+1}(x^2) = (f_i(x) + f_i(-x))/2 + alpha_i * (f_i(x) - f_i(-x))/(2x)
        new domain D_{i+1} = squared D_i, half the size,
        Merkle-commit f_{i+1}; absorb root.
    After log2(N) folds, f_final is a single constant; prover absorbs it.

  Query phase
    For each query:
        verifier picks position q in [0, N),
        prover opens at layer i the pair (q mod (N_i/2), q mod (N_i/2) + N_i/2)
            -- one of these IS q; the other is at -x (its negation in the multiplicative group),
        verifier checks Merkle paths and folding consistency between layers,
        at the last layer, folded value must equal f_final.

Security per query is ~log2(blowup) bits in the standard FRI heuristic. We default
to 20 queries with blowup 4 -> ~40 bits, fine for a demo. For 100-bit security
crank to blowup 8, 64 queries (see NUM_QUERIES / LOG_BLOWUP).
"""

from field import P, add, sub, mul, inv, pow_, neg, primitive_root_of_unity, MULT_GEN
from ntt import coset_ntt
from merkle import MerkleTree, verify_path


LOG_BLOWUP = 3       # blowup = 8
NUM_QUERIES = 24     # ~72 bits of soundness in the heuristic


def commit_polynomial_on_fri_domain(coeffs, log_n):
    """
    Evaluate a polynomial of degree < 2^log_n on the FRI extended domain
    (size 2^(log_n + LOG_BLOWUP)) and Merkle-commit the evaluations.

    Returns: (evals, MerkleTree).
    """
    log_N = log_n + LOG_BLOWUP
    evals = coset_ntt(coeffs, MULT_GEN, log_N)
    return evals, MerkleTree(evals)


def fri_prove(coeffs, log_n, transcript):
    """
    Prove that `coeffs` is a polynomial of degree < 2^log_n.

    The caller typically already has the layer-0 commitment (because the
    polynomial is some random linear combination of DEEP quotients constructed
    upstream). We recompute it here for simplicity of the API.
    """
    log_N = log_n + LOG_BLOWUP

    # ---- Layer 0
    evals, tree0 = commit_polynomial_on_fri_domain(coeffs, log_n)
    transcript.absorb("fri_root", tree0.root)

    layer_trees = [tree0]
    layer_evals = [evals]

    current = evals
    current_shift = MULT_GEN
    current_log = log_N

    # ---- Fold log_N times down to a single value
    while current_log > 0:
        alpha = transcript.challenge_field("fri_alpha")

        half = len(current) >> 1
        omega = primitive_root_of_unity(current_log)
        two_inv = inv(2)
        new_layer = [0] * half
        x = current_shift  # first domain point of this layer
        for i in range(half):
            f_pos = current[i]
            f_neg = current[i + half]
            f_even = mul(add(f_pos, f_neg), two_inv)
            f_odd = mul(sub(f_pos, f_neg), mul(two_inv, inv(x)))
            new_layer[i] = add(f_even, mul(alpha, f_odd))
            x = mul(x, omega)

        current = new_layer
        current_shift = mul(current_shift, current_shift)
        current_log -= 1

        if current_log > 0:
            tree = MerkleTree(current)
            layer_trees.append(tree)
            layer_evals.append(current)
            transcript.absorb("fri_root", tree.root)

    final_value = current[0]
    transcript.absorb("fri_final", final_value)

    # ---- Query phase
    queries = []
    for _ in range(NUM_QUERIES):
        q = transcript.challenge_int(1 << log_N, "fri_query")
        opens = []
        cur_q = q
        for tree, ev in zip(layer_trees, layer_evals):
            cur_n = len(ev)
            half = cur_n >> 1
            p_lo = cur_q % half
            p_hi = p_lo + half
            opens.append({
                "p_lo": p_lo,
                "v_lo": ev[p_lo],
                "v_hi": ev[p_hi],
                "path_lo": tree.open(p_lo),
                "path_hi": tree.open(p_hi),
            })
            cur_q = p_lo  # position in the next, half-sized layer
        queries.append(opens)

    proof = {
        "layer_roots": [t.root for t in layer_trees],
        "final_value": final_value,
        "queries": queries,
    }
    return proof, evals, tree0  # caller may want layer-0 artifacts


def fri_verify(proof, log_n, transcript):
    """Verify a FRI proof; returns True iff the committed function is close to deg < 2^log_n."""
    log_N = log_n + LOG_BLOWUP
    N = 1 << log_N
    roots = proof["layer_roots"]

    if len(roots) != log_N:
        return False
    if len(proof["queries"]) != NUM_QUERIES:
        return False

    # Replay the prover's transcript to recover alphas
    transcript.absorb("fri_root", roots[0])
    alphas = []
    for i in range(log_N):
        alphas.append(transcript.challenge_field("fri_alpha"))
        if i + 1 < log_N:
            transcript.absorb("fri_root", roots[i + 1])
    transcript.absorb("fri_final", proof["final_value"])

    two_inv = inv(2)
    shift0 = MULT_GEN

    for q_idx in range(NUM_QUERIES):
        q = transcript.challenge_int(N, "fri_query")
        opens = proof["queries"][q_idx]

        cur_q = q
        cur_shift = shift0
        cur_log = log_N

        for layer_i, opening in enumerate(opens):
            cur_n = 1 << cur_log
            half = cur_n >> 1
            p_lo = cur_q % half
            p_hi = p_lo + half

            # 1. Position must match what the verifier independently derived
            if opening["p_lo"] != p_lo:
                return False

            # 2. Merkle paths must check out
            root = roots[layer_i]
            if not verify_path(root, opening["v_lo"], p_lo, opening["path_lo"]):
                return False
            if not verify_path(root, opening["v_hi"], p_hi, opening["path_hi"]):
                return False

            # 3. Folding consistency
            omega = primitive_root_of_unity(cur_log)
            x_lo = mul(cur_shift, pow_(omega, p_lo))
            f_even = mul(add(opening["v_lo"], opening["v_hi"]), two_inv)
            f_odd = mul(sub(opening["v_lo"], opening["v_hi"]), mul(two_inv, inv(x_lo)))
            folded = add(f_even, mul(alphas[layer_i], f_odd))

            if layer_i + 1 < len(opens):
                # Folded value should match one of the openings at the next layer
                nxt = opens[layer_i + 1]
                next_half = half >> 1  # half of layer (i+1) which has size = half
                if p_lo < next_half:
                    expected = nxt["v_lo"]
                else:
                    expected = nxt["v_hi"]
                if folded != expected:
                    return False
            else:
                # Last layer: folded value must equal the final constant
                if folded != proof["final_value"]:
                    return False

            # Descend
            cur_q = p_lo
            cur_shift = mul(cur_shift, cur_shift)
            cur_log -= 1

    return True
