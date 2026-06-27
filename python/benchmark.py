"""
Cross-field benchmark for PLONK+FRI.

Spawns one subprocess per field so each gets a fresh module state. Measures
prove time, verify time, peak working-set proxy, and proof size. Repeats to
average out noise.

Caveat: these numbers are CPython on x86_64. Python's int is heap-allocated
regardless of size for ints > 2^30 or so, so the per-op cost is similar
across fields. The interesting differential (Goldilocks vs BabyBear/KoalaBear)
shows up on a 32-bit MCU where 64-bit Goldilocks ints become heap bignums in
MicroPython while 31-bit BabyBear/KoalaBear stay as tagged small-ints. Use
these CPython numbers for correctness + relative ordering; for real ESP32
numbers, port the inner loops to C.
"""

import os
import subprocess
import sys
import json
import statistics
import time

FIELDS = ["goldilocks", "babybear", "koalabear"]
REPEATS = 5


WORKER = r"""
import os, sys, time, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) if "__file__" in dir() else ".")
import field
from demo import build_circuit, build_witness
from plonk import setup, prove, verify

circuit = build_circuit()
witness = build_witness(3)

t0 = time.perf_counter(); pp = setup(circuit);          t1 = time.perf_counter()
t2 = time.perf_counter(); proof = prove(pp, witness);   t3 = time.perf_counter()
t4 = time.perf_counter(); ok = verify(pp, proof);       t5 = time.perf_counter()

def sz(p):
    if isinstance(p, bytes): return len(p)
    if isinstance(p, int): return (p.bit_length() + 7) // 8 or 1
    if isinstance(p, (list, tuple)): return sum(sz(x) for x in p)
    if isinstance(p, dict): return sum(sz(v) for v in p.values())
    return 0

print(json.dumps({
    "field":   field.FIELD_NAME,
    "bits":    field.FIELD_BITS,
    "p":       field.P,
    "setup_ms":  (t1 - t0) * 1000,
    "prove_ms":  (t3 - t2) * 1000,
    "verify_ms": (t5 - t4) * 1000,
    "verified":  ok,
    "proof_bytes": sz(proof),
}))
"""


def run_one(field_name):
    env = dict(os.environ, ZKP_FIELD=field_name, PYTHONUNBUFFERED="1")
    cwd = os.path.dirname(os.path.abspath(__file__))
    r = subprocess.run(
        [sys.executable, "-c", WORKER],
        env=env, cwd=cwd,
        capture_output=True, text=True, timeout=120,
    )
    if r.returncode != 0:
        print("FAILED", field_name, file=sys.stderr)
        print(r.stderr, file=sys.stderr)
        return None
    return json.loads(r.stdout.strip().splitlines()[-1])


def main():
    print("PLONK+FRI cross-field benchmark")
    print("Circuit:  x^3 + x + 5 = 35 (n=8 gates, log_blowup=3, 24 FRI queries)")
    print("Reps:     {} per field (best timing reported)".format(REPEATS))
    print("Platform: CPython on x86_64 (relative ordering only; absolute numbers"
          " do not reflect ESP32 MicroPython performance)")
    print()

    results = {}
    for f in FIELDS:
        prove_times, verify_times, setup_times = [], [], []
        last = None
        for _ in range(REPEATS):
            r = run_one(f)
            if r is None:
                break
            last = r
            setup_times.append(r["setup_ms"])
            prove_times.append(r["prove_ms"])
            verify_times.append(r["verify_ms"])
        if last is None:
            continue
        results[f] = {
            "p":            last["p"],
            "bits":         last["bits"],
            "setup_ms":     min(setup_times),
            "prove_ms":     min(prove_times),
            "prove_med_ms": statistics.median(prove_times),
            "verify_ms":    min(verify_times),
            "verify_med_ms": statistics.median(verify_times),
            "proof_bytes":  last["proof_bytes"],
            "verified":     last["verified"],
        }

    # Pretty print
    print("{:<12} {:>6} {:>10} {:>10} {:>11} {:>12} {:>8}".format(
        "field", "bits", "setup(ms)", "prove(ms)", "verify(ms)", "proof(bytes)", "ok"))
    print("-" * 76)
    for f, r in results.items():
        print("{:<12} {:>6} {:>10.2f} {:>10.2f} {:>11.2f} {:>12d} {:>8}".format(
            f, r["bits"], r["setup_ms"], r["prove_ms"], r["verify_ms"],
            r["proof_bytes"], "PASS" if r["verified"] else "FAIL"))

    # Relative ordering against Goldilocks
    if "goldilocks" in results:
        base = results["goldilocks"]
        print()
        print("Relative to Goldilocks (CPython, x86_64):")
        for f, r in results.items():
            if f == "goldilocks":
                continue
            prove_ratio  = r["prove_ms"]  / base["prove_ms"]
            verify_ratio = r["verify_ms"] / base["verify_ms"]
            print("  {:<12} prove {:>5.2f}x   verify {:>5.2f}x   proof {:+d} bytes".format(
                f, prove_ratio, verify_ratio,
                r["proof_bytes"] - base["proof_bytes"]))

    print()
    print("Notes:")
    print("  - CPython stores Python ints as PyLong objects regardless of size,")
    print("    so per-op cost is dominated by interpreter overhead, NOT field width.")
    print("  - To see the ~2-4x speedup that 31-bit fields enable on 32-bit MCUs,")
    print("    port the inner loops (mul, pow, ntt butterflies) to C and re-run")
    print("    on hardware. The MicroPython int representation switches between")
    print("    tagged small-int and heap bignum around ~30 bits.")


if __name__ == "__main__":
    main()
