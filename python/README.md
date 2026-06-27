# PLONK + FRI for embedded ZKP

A pure-Python (MicroPython-compatible) reference implementation of PLONK with FRI
as the polynomial commitment scheme. Designed to be portable to ESP32/ESP8266 in C
with minimal structural changes.

No external dependencies; only `hashlib.sha256` from the standard library.

## Why this stack on ESP32

| Choice | Reason |
| --- | --- |
| **Goldilocks field** `p = 2^64 − 2^32 + 1` | Fits a 64-bit word; reduction is two subtractions in C. Two-adicity 2^32 covers any NTT domain you can fit in SRAM. |
| **FRI** instead of KZG | Hash-only, no pairings, no trusted setup. ESP32's hardware SHA-256 accelerator makes Merkle commitments cheap. |
| **SHA-256** Merkle trees | Hardware-accelerated on ESP32 (`mbedtls_sha256_hw`). ~50× faster than software. |
| **No lookups, no recursion** | Keeps memory bounded; everything fits in 520 KB SRAM for small circuits. |

## Files

```
field.py            Dispatcher; picks one of fields/*.py via $ZKP_FIELD
fields/
  goldilocks.py       p = 2^64 - 2^32 + 1   (Plonky2)
  babybear.py         p = 15 * 2^27 + 1     (Plonky3, SP1)
  koalabear.py        p = 2^31 - 2^24 + 1   (Plonky3)
poly.py             Dense polynomials in coefficient form
ntt.py              Radix-2 Cooley-Tukey NTT (+ coset variants)
merkle.py           SHA-256 Merkle tree
transcript.py       Fiat-Shamir transcript (SHA-256 sponge)
fri.py              Standalone FRI low-degree test
plonk.py            PLONK prover + verifier built on FRI
demo.py             Example: prove x^3 + x + 5 = 35 for secret x
benchmark.py        Cross-field comparison (subprocess per field)
test_fri.py         FRI unit tests
```

Switch fields via the environment variable:

```
ZKP_FIELD=goldilocks python3 demo.py    # default
ZKP_FIELD=babybear   python3 demo.py
ZKP_FIELD=koalabear  python3 demo.py
```

## Demo

```
$ python3 demo.py
Local gate check: OK
Setup done; n=8, log_n=3
Proving...
Proof generated.
Verifying...
Verification: ACCEPT

Tamper test 1: corrupted opening
  Verification of tampered proof: REJECT (good)

Tamper test 2: wrong witness (x=4 doesn't satisfy x^3+x+5=35)
  Prover aborted (expected): permutation argument doesn't telescope; ...

All tests passed.
```

## Field choice

The implementation supports three Plonkish+FRI-compatible fields, covering the
range of design decisions in the production STARK/Plonkish ecosystem:

| Field | Prime | Bits | Two-adicity | Origin |
| --- | --- | ---: | ---: | --- |
| Goldilocks | `2^64 - 2^32 + 1` | 64 | 32 | Plonky2 |
| BabyBear  | `15 * 2^27 + 1`   | 31 | 27 | Plonky3, SP1 |
| KoalaBear | `2^31 - 2^24 + 1` | 31 | 24 | Plonky3 |

The protocol code (PLONK rounds, FRI, transcript, Merkle) is field-agnostic. Only
the `fields/*.py` modules change. Drop-in via `ZKP_FIELD=babybear`.

### Why care about the field on an MCU

On a 64-bit host, Goldilocks is faster than the 31-bit primes because its
reduction (two subtractions of the high half) is essentially free on x86_64.
This is exactly the regime where Plonky2's original Goldilocks choice shines.

On a 32-bit MCU (Xtensa LX6/LX7 in ESP32, Cortex-M4/M7 in STM32), the calculus
flips:

- **Goldilocks multiply** of two 64-bit operands produces a 128-bit
  intermediate. The 32-bit core has to synthesize this from four 32-bit
  partial products + carries, and the reduction touches the 96-bit high half.
- **BabyBear/KoalaBear multiply** of two 31-bit operands produces a 62-bit
  intermediate — a single 32x32->64 instruction on every modern 32-bit ISA.
  Reduction is then a single conditional subtract or a few shifts.

In MicroPython specifically, `mpz` (heap-allocated bignum) kicks in around
`2^30`. Goldilocks values are always bignums; BabyBear/KoalaBear values stay
as inline `MP_OBJ_SMALL_INT` for nearly every arithmetic operation. This is
the gap that should materialize on real hardware.

### Cross-field benchmark (CPython, n=8 demo circuit)

```
$ python3 benchmark.py

field          bits  setup(ms)  prove(ms)  verify(ms) proof(bytes)       ok
----------------------------------------------------------------------------
goldilocks       64       0.33       8.47        6.73        92639     PASS
babybear         31       0.26       6.08        4.82        90294     PASS
koalabear        31       0.25       6.12        4.80        90297     PASS

Relative to Goldilocks (CPython, x86_64):
  babybear     prove  0.72x   verify  0.72x   proof -2345 bytes
  koalabear    prove  0.72x   verify  0.71x   proof -2342 bytes
```

CPython already shows a ~28% wall-clock advantage for the 31-bit fields,
driven mainly by the inverse operation (Fermat's `pow(a, P-2, P)`) which
performs ~31 squarings instead of ~64. The proof-size difference (~2.3 KB
out of ~93 KB) comes from field elements occupying 4 wire bytes instead of 8.

**This is not the embedded benchmark you'd publish.** CPython int overhead
dominates everything. The number to report is from a C port (or
MicroPython on actual silicon) where the 32-bit vs 64-bit multiply path
is the critical difference. The CPython results here only confirm:
(1) all three fields are protocol-equivalent, and (2) the relative ordering
favors small fields even before the embedded-specific advantage kicks in.

### Why not Mersenne31

`p = 2^31 - 1` has two-adicity of only 1 (`p - 1 = 2 * (2^30 - 1)`,
where `2^30 - 1` is odd). That means **no primitive 2^k-th root of unity
for k > 1**, so no radix-2 NTT, so no FRI commit on a coset of size `n *
blowup`. Mersenne31 is the natural ESP32 field on paper (single-instruction
reduction: `r = (x & 0x7FFFFFFF) + (x >> 31)`) but it requires the **Circle
STARK** machinery (StarkWare's `stwo`) which uses a different geometric
structure for the evaluation domain. That's a different protocol — not a
field swap — and is left as future work in this repo.

## Protocol summary

For an `n`-gate circuit (n a power of two), with witness columns a, b, c and
selectors q_L, q_R, q_O, q_M, q_C, plus a permutation σ encoding copy constraints:

1. **R1 commit witness.** IFFT `a, b, c` into coefficient form, evaluate on a
   coset of size N = n · 2^L (L = LOG_BLOWUP), Merkle-commit.
2. **R2 permutation.** β, γ ← transcript. Build the permutation polynomial Z by
   the telescoping product; commit. If telescoping doesn't land back at 1, the
   prover aborts (copy constraints violated).
3. **R3 quotient.** α ← transcript. On the FRI extended domain, pointwise build
       C(X) = gate(X) + α · perm(X) + α² · boundary(X)
   and divide by `Z_H(X) = X^n − 1` (well-defined on the coset because it doesn't
   vanish there). INTT, split into `t_lo, t_mid, t_hi` of degree < n, commit each.
4. **R4 evaluation.** ζ ← transcript. Send openings of
       a(ζ), b(ζ), c(ζ), Z(ζ), Z(ζω), t_lo(ζ), t_mid(ζ), t_hi(ζ).
   The verifier checks
       C(ζ) ≟ t(ζ) · Z_H(ζ),     where t(ζ) = t_lo(ζ) + ζⁿ·t_mid(ζ) + ζ²ⁿ·t_hi(ζ).
   The selectors and S_σ polynomials are evaluated locally by the verifier
   from their preprocessed coefficient forms.
5. **R5 batched opening (DEEP-FRI).** ρ ← transcript. The 8 openings collapse to
   a single low-degree test on
       Q(X) = Σ ρᵏ · (f_k(X) − f_k(ζ))/(X − ζ)
            + ρ_last · (Z(X) − Z(ζω))/(X − ζω).
   The prover does NOT commit to Q's layer 0 — the verifier reconstructs Q(x)
   at each FRI query position from the f_k openings, saving one Merkle tree.

## Security parameters

In `fri.py`:

| Constant | Value | Effect |
| --- | --- | --- |
| `LOG_BLOWUP` | 3 (blowup = 8) | FRI rate = 1/8 |
| `NUM_QUERIES` | 24 | ≈ 24 · log₂(8) = 72 bits of soundness (heuristic) |

For 100-bit soundness raise `NUM_QUERIES` to 34, or use `LOG_BLOWUP=4` (rate 1/16)
with 25 queries. For 128-bit you typically want grinding (proof-of-work on the
transcript) plus more queries.

## Known simplifications

These are deliberate to keep the implementation small. Each is the natural
next step:

1. **Public inputs are baked into q_C.** Real PLONK keeps a separate PI(X)
   polynomial so the same circuit can verify different statements without
   re-preprocessing. To add this: include `PI(ζ)` in the verifier's gate check
   and absorb the public-input vector into the transcript.
2. **No linearization polynomial.** Vanilla PLONK reduces the number of
   openings from 8 down to ~6 by constructing `r(X)`. Skipping it costs ~2
   extra Merkle paths per query but simplifies the prover by ~50 lines.
3. **No lookups (PlonKup/Plookup).** Range checks and bitwise ops are
   currently expensive (must be decomposed into gates). Adding plookup
   requires another permutation argument over the lookup table.
4. **One quotient split.** `t` is split into exactly 3 chunks of size n. For
   circuits with higher-degree custom gates the split must be larger.
5. **Soundness is heuristic-FRI, not conjectured-FRI.** For a tighter bound,
   apply the FRI soundness theorem of Ben-Sasson–Bentov–Horesh–Riabzev with
   the actual blowup and query count.

## Porting to ESP32

The Python here is structured so each module has a clear C analogue.

### Field (`field.py` → `field.c`)

Goldilocks reduction in C using xtensa ESP32 instructions:

```c
// Reduce 128-bit product to Goldilocks via two subtractions
// (technique from plonky2 and others)
static inline uint64_t goldilocks_reduce(uint128_t x) {
    uint64_t x_lo = (uint64_t)x;
    uint64_t x_hi = (uint64_t)(x >> 64);
    uint32_t x_hi_hi = (uint32_t)(x_hi >> 32);
    uint32_t x_hi_lo = (uint32_t)x_hi;

    uint64_t t0 = x_lo - x_hi_hi;
    if (x_lo < x_hi_hi) t0 -= ((1ULL << 32) - 1);  // wrap correction

    uint64_t t1 = (uint64_t)x_hi_lo << 32;
    t1 -= x_hi_lo;

    uint64_t r = t0 + t1;
    if (r < t0 || r >= GOLDILOCKS_P) r -= GOLDILOCKS_P;
    return r;
}
```

A Plantard-style Montgomery variant is ~30% faster and worth implementing if
the NTT dominates your profile.

### NTT (`ntt.py` → `ntt.c`)

In-place radix-2 Cooley-Tukey. Bit-reversal permutation on ESP32 should use
the ROM `__builtin_bswap32` plus a precomputed table of twiddle factors. Keep
twiddles in IRAM (32-bit aligned) for fast access.

For a domain of size N, the NTT does `(N/2)·log N` butterfly operations. On
ESP32-S3 with hand-tuned assembly, this is roughly:
- N = 64:   ~50 µs
- N = 512:  ~600 µs
- N = 4096: ~7 ms

### Merkle (`merkle.py` → `merkle.c`)

Use the hardware SHA accelerator:

```c
#include "mbedtls/sha256.h"
mbedtls_sha256_context ctx;
mbedtls_sha256_init(&ctx);
// On ESP32 mbedTLS auto-routes to hardware if config is set
mbedtls_sha256_starts(&ctx, 0);
mbedtls_sha256_update(&ctx, &TAG_LEAF, 1);
mbedtls_sha256_update(&ctx, value_bytes, 8);
mbedtls_sha256_finish(&ctx, out);
```

Streaming the tree level-by-level avoids holding all internal nodes in SRAM —
keep only the current layer plus the path being computed. For a 1024-leaf
tree this drops peak memory from 64 KB to 4 KB.

### Memory budget

For an n-gate circuit with blowup 2^L:

| Item | Size |
| --- | --- |
| 7 polynomial evaluations on FRI domain | 7 · n · 2^L · 8 bytes |
| Merkle trees (peak, streamed) | 7 · n · 2^L · 32 bytes (intermediate nodes, can be streamed) |
| FRI Q evaluations | n · 2^L · 8 bytes |
| Twiddle tables | n · 2^L · 8 bytes (precomputed in flash) |

For n=64, L=3 (N=512): roughly 7 · 512 · 8 = 28 KB for evaluations, plus
~16 KB for Merkle nodes if streamed → fits comfortably in ESP32's 520 KB SRAM
with room for the application.

For n=1024, L=3: ~57 KB evaluations + 32 KB Merkle → still fine.

For n=8192, L=3 (N=65536): ~3.6 MB just for evaluations. You'd need PSRAM and
careful streaming of the prover; the verifier remains small.

### Verifier-only on ESP8266

The verifier is much lighter than the prover: it doesn't do any NTT, just
~30 polynomial evaluations at one point (Horner over degree < n) and a few
hundred SHA-256 invocations for Merkle path checks. Estimated verify time on
ESP8266 (no hardware SHA): ~1 second for n=64, dominated by Merkle hashing.

### What does NOT port cleanly

- `int.from_bytes(..., 'little')` returning arbitrary-precision ints: in C use
  fixed `uint64_t`.
- The `% P` everywhere becomes the inlined reduction above.
- Python's `pow(a, P-2, P)` for inversion: in C either Fermat exponentiation
  with the addition-chain `P-2 = 0xFFFFFFFEFFFFFFFF` (66 squarings + 7
  multiplies) or extended-Euclid.

## Threat model and ZK

This implementation is **proof of knowledge but not zero-knowledge in its
current form** — the prover does not blind the witness polynomials before
committing. To make it ZK:

- Add a random multiple of `Z_H(X)` to each of `a, b, c, Z` before committing
  (PLONK's standard blinding).
- The verifier's checks pass because the blinding vanishes on H, but the
  Merkle commitments leak no information about the witness on the coset.

The structural impact is ~6 extra random field elements and a tweak to the
degree bound when splitting `t`. Documented in the PLONK paper, sec. 8.4.

## Citations

If you find this useful for the M&MS or MMAR papers, the underlying protocols
to credit are: PLONK (Gabizon, Williamson, Ciobotaru 2019), FRI (Ben-Sasson,
Bentov, Horesh, Riabzev 2018), DEEP-ALI (Ben-Sasson, Goldberg, Kopparty,
Saraf 2019), Goldilocks parameter choice (Polygon Zero / plonky2).
