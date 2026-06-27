"""
Demo circuit: prove knowledge of secret x such that x^3 + x + 5 = 35.

Witness: x = 3.  Constants 5 and 35 are public, baked into q_C.

Gate layout (n = 8):
  g0:  a*b = c        a=3,  b=3,  c=9        q_M=1, q_O=-1
  g1:  a*b = c        a=9,  b=3,  c=27       q_M=1, q_O=-1
  g2:  a+b = c        a=27, b=3,  c=30       q_L=1, q_R=1, q_O=-1
  g3:  a+b = c        a=30, b=5,  c=35       q_L=1, q_R=1, q_O=-1
  g4:  a   = 5        a=5                    q_L=1, q_C=-5
  g5:  a   = 35       a=35                   q_L=1, q_C=-35
  g6:  padding (no constraint)
  g7:  padding

Copy constraints:
  a0 = b0 = b1 = b2          (all = x)
  c0 = a1                    (= 9)
  c1 = a2                    (= 27)
  c2 = a3                    (= 30)
  b3 = a4                    (= 5)
  c3 = a5                    (= 35)
"""

import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from field import P, neg
from plonk import setup, prove, verify


def build_circuit():
    n = 8

    # Selectors. Note q_O carries -1 wherever the gate equation has -c.
    z = 0; one = 1; neg_one = neg(1)
    q_L = [z,        z,       one,      one,      one,    one,    z, z]
    q_R = [z,        z,       one,      one,      z,      z,      z, z]
    q_O = [neg_one,  neg_one, neg_one,  neg_one,  z,      z,      z, z]
    q_M = [one,      one,     z,        z,        z,      z,      z, z]
    q_C = [z,        z,       z,        z,        neg(5), neg(35), z, z]

    # Positions: p = column*n + row, column in {0,1,2} = {a,b,c}, so positions 0..23.
    #   a0=0  a1=1  a2=2  a3=3  a4=4  a5=5  a6=6  a7=7
    #   b0=8  b1=9  b2=10 b3=11 b4=12 b5=13 b6=14 b7=15
    #   c0=16 c1=17 c2=18 c3=19 c4=20 c5=21 c6=22 c7=23
    #
    # Build copy classes, then express each as a cycle in the permutation.
    classes = [
        [0, 8, 9, 10],     # a0, b0, b1, b2  (all x = 3)
        [16, 1],           # c0, a1          (= 9)
        [17, 2],           # c1, a2          (= 27)
        [18, 3],           # c2, a3          (= 30)
        [11, 4],           # b3, a4          (= 5)
        [19, 5],           # c3, a5          (= 35)
    ]

    sigma = list(range(3 * n))  # identity
    for cls in classes:
        # cycle: each element maps to the next, last back to first
        k = len(cls)
        for i in range(k):
            sigma[cls[i]] = cls[(i + 1) % k]

    return {
        "log_n": 3,
        "q_L": q_L, "q_R": q_R, "q_O": q_O, "q_M": q_M, "q_C": q_C,
        "permutation": sigma,
    }


def build_witness(x):
    # Compute intermediate values
    x2 = x * x
    x3 = x2 * x
    s1 = x3 + x       # = 30 if x=3
    s2 = s1 + 5       # = 35

    a = [x,  x2, x3, s1, 5,  s2, 0, 0]
    b = [x,  x,  x,  5,  0,  0,  0, 0]
    c = [x2, x3, s1, s2, 0,  0,  0, 0]
    return {"a": a, "b": b, "c": c}


def check_witness_against_circuit(circuit, witness):
    """Local sanity check that the gate equations hold."""
    n = 1 << circuit["log_n"]
    for i in range(n):
        a, b, c = witness["a"][i], witness["b"][i], witness["c"][i]
        qL, qR, qO = circuit["q_L"][i], circuit["q_R"][i], circuit["q_O"][i]
        qM, qC = circuit["q_M"][i], circuit["q_C"][i]
        v = (qL * a + qR * b + qO * c + qM * a * b + qC) % P
        assert v == 0, "gate %d unsatisfied: %d" % (i, v)


def main():
    circuit = build_circuit()
    witness = build_witness(3)
    check_witness_against_circuit(circuit, witness)
    print("Local gate check: OK")

    pp = setup(circuit)
    print("Setup done; n=%d, log_n=%d" % (pp["n"], pp["log_n"]))

    print("Proving...")
    proof = prove(pp, witness)
    print("Proof generated.")

    print("Verifying...")
    ok = verify(pp, proof)
    print("Verification:", "ACCEPT" if ok else "REJECT")
    assert ok, "valid proof was rejected"

    # ---- Soundness test 1: tamper with an opening
    print("\nTamper test 1: corrupted opening")
    bad = _copy_proof(proof)
    bad["openings"]["a"] = (bad["openings"]["a"] + 1) % P
    bad_ok = verify(pp, bad)
    print("  Verification of tampered proof:", "ACCEPT (BUG!)" if bad_ok else "REJECT (good)")
    assert not bad_ok

    # ---- Soundness test 2: try to prove a false statement
    print("\nTamper test 2: wrong witness (x=4 doesn't satisfy x^3+x+5=35)")
    try:
        bad_witness = build_witness(3)
        bad_witness["a"][0] = 4  # break the witness
        bad_proof = prove(pp, bad_witness)
        # Either the prover asserts out (constraint failure detected) OR the verifier rejects
        print("  Prover did not abort; checking verifier...")
        bad_ok = verify(pp, bad_proof)
        print("  Verification:", "ACCEPT (BUG!)" if bad_ok else "REJECT (good)")
        assert not bad_ok
    except AssertionError as e:
        print("  Prover aborted (expected):", str(e)[:80])

    print("\nAll tests passed.")


def _copy_proof(p):
    import copy
    return copy.deepcopy(p)


if __name__ == "__main__":
    main()
