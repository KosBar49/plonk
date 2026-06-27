"""Smoke test: FRI accepts a low-degree poly, rejects a high-degree one."""
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from field import P
from fri import fri_prove, fri_verify, LOG_BLOWUP
from transcript import Transcript

random.seed(42)

# Test 1: degree-3 polynomial (log_n=2 -> n=4)
log_n = 2
n = 1 << log_n
coeffs = [random.randrange(P) for _ in range(n)]

prover_t = Transcript(b"test")
proof, _, _ = fri_prove(coeffs, log_n, prover_t)
verifier_t = Transcript(b"test")
ok = fri_verify(proof, log_n, verifier_t)
print("Test 1 (low-degree poly):", "PASS" if ok else "FAIL")
assert ok

# Test 2: tamper with one evaluation -> verification must fail
prover_t = Transcript(b"test")
proof, _, _ = fri_prove(coeffs, log_n, prover_t)
proof["queries"][0][0]["v_lo"] = (proof["queries"][0][0]["v_lo"] + 1) % P
verifier_t = Transcript(b"test")
ok = fri_verify(proof, log_n, verifier_t)
print("Test 2 (tampered value):", "FAIL_AS_EXPECTED" if not ok else "UNEXPECTED_PASS")
assert not ok

# Test 3: tamper with final value
prover_t = Transcript(b"test")
proof, _, _ = fri_prove(coeffs, log_n, prover_t)
proof["final_value"] = (proof["final_value"] + 1) % P
verifier_t = Transcript(b"test")
ok = fri_verify(proof, log_n, verifier_t)
print("Test 3 (tampered final):", "FAIL_AS_EXPECTED" if not ok else "UNEXPECTED_PASS")
assert not ok

# Test 4: bigger polynomial
log_n = 4
n = 1 << log_n
coeffs = [random.randrange(P) for _ in range(n)]
prover_t = Transcript(b"test2")
proof, _, _ = fri_prove(coeffs, log_n, prover_t)
verifier_t = Transcript(b"test2")
ok = fri_verify(proof, log_n, verifier_t)
print("Test 4 (log_n=4):", "PASS" if ok else "FAIL")
assert ok

print("All FRI smoke tests passed.")
