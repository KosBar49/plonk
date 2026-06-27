# c_core -- field-generic PLONK+FRI prover + verifier in C

A single C codebase implementing both the **prover** and **verifier** for
PLONK with FRI commitments, parameterised over three fields at compile time.
Fully static allocation (no malloc), so the memory footprint is a fixed,
measurable quantity -- which is the point for the embedded feasibility study.

This is the implementation used for the cross-field comparison on MCU-class
hardware. The Python reference lives in `..\plonk_fri`.

## Fields (compile-time selection)

Pick exactly one with a `-DFIELD_*` flag:

| Flag | Field | felt | Origin |
| --- | --- | --- | --- |
| `-DFIELD_GOLDILOCKS` | 2^64 - 2^32 + 1 | uint64_t | Plonky2 |
| `-DFIELD_BABYBEAR`   | 15*2^27 + 1     | uint32_t | Plonky3, SP1 |
| `-DFIELD_KOALABEAR`  | 2^31 - 2^24 + 1 | uint32_t | Plonky3 |

The whole protocol (NTT, Merkle, transcript, prover, verifier) is written
against the abstract interface in `field.h`. `felt` is 32-bit for the small
fields and 64-bit for Goldilocks -- so a benchmark on a 32-bit MCU actually
pays the real cost difference, which is the experiment.

## Build & run

```
mingw32-make test          # Windows (Strawberry GCC): build + run all 3 self-tests
make test                  # Linux/macOS
mingw32-make MAXLOGN=10    # change the max circuit size (default 8)
```

Self-test output (per field): C-prove -> C-verify ACCEPT, plus two tamper tests
that must REJECT.

## Cross-check against the Python reference

The strongest correctness evidence: a proof produced by the C prover is
verified by the *independent* Python verifier.

```
mingw32-make prove                       # builds prove_<field>.exe
prove_babybear.exe c_proof.bin           # C prover writes a proof
python verify_c_proof.py babybear c_proof.bin   # Python verifier checks it
```

All three fields pass C-prove -> Python-verify. Combined with Python-prove ->
Python-verify and C-prove -> C-verify, every combination agrees.

## Files

```
field.h               field dispatcher (selects fields/*_f.h)
fields/
  goldilocks_f.h        64-bit, __uint128_t mul (manual reduction path for MCU)
  babybear_f.h          31-bit
  koalabear_f.h         31-bit
config.h              compile-time dimensions (MAX_LOG_N, LOG_BLOWUP, NUM_QUERIES)
ntt.{c,h}             radix-2 NTT + coset eval/interp
merkle.{c,h}          SHA-256 Merkle: build, open, verify  (8-byte-LE leaf encoding)
transcript.{c,h}      Fiat-Shamir sponge (byte-identical to Python)
sha256.{c,h}          SHA-256 (mbedtls on ESP32, software fallback otherwise)
circuit.{c,h}         demo circuit + setup() preprocessing
prover.{c,h}          5-round prover + DEEP-FRI, all static buffers
verifier.{c,h}        field-generic verifier
prove_main.c          prover CLI -> writes <field>_proof.bin in Python wire format
selftest_main.c       prove+verify+tamper in one process
verify_c_proof.py     loads a C proof, verifies with the Python verifier

test_*.c              unit tests (field, ntt, merkle, setup, transcript)
_py_*_ref.py          Python reference outputs for the unit cross-checks
```

## Memory footprint (the embedded headline)

`prove_<field>.exe` prints the static footprint at startup. At the default
`MAX_LOG_N=8` the Merkle tree storage dominates:

```
Prover static footprint (MAX_LOG_N=8, field=babybear, felt=4B):
  felt buffers : ~291 KB
  merkle trees : ~2304 KB
  TOTAL        : ~2595 KB
```

That ~2.6 MB does NOT fit an ESP32 (520 KB SRAM). This is the feasibility
result the static-allocation design surfaces directly: the prover's tree
storage is the binding constraint, not the field arithmetic. Levers to bring
it down for on-device proving:

- Size the Merkle trees to the actual `n`, not `MAX_NEXT` (the demo uses
  `log_n=3`, so real usage is a few KB; the 2.6 MB is worst-case reservation).
- Lower `MAX_LOG_N`.
- Stream Merkle construction (keep only the current level + the path), which
  drops tree storage from O(N) to O(log N) per tree.
- Lower `LOG_BLOWUP` (trades soundness for memory).

The verifier, by contrast, needs only a few KB of stack regardless -- which is
why verifier-on-MCU is the comfortable case and prover-on-MCU is the hard,
publishable one.

## Notes

- `LOG_BLOWUP=3`, `NUM_QUERIES=24` are fixed to match `..\plonk_fri\fri.py`.
  Changing them requires changing both sides or the cross-check breaks.
- Goldilocks `f_mul` uses `__uint128_t` (available on host and ESP32 GCC). The
  `GOLDILOCKS_NO_INT128` path in `fields/goldilocks_f.h` is a placeholder for
  targets without 128-bit ints and should be validated before use on such a
  target.
- This is a Plonky2-family construction (Goldilocks+PLONK+FRI+DEEP) extended
  with Plonky3's small fields, for embedded characterisation -- not a port of
  Plonky3 itself.
