# PLONK + FRI for embedded zero-knowledge proofs

PhD research code: a PLONK proving system with FRI polynomial commitments, built
to study how the **field choice** (Goldilocks vs BabyBear vs KoalaBear) affects
prover and verifier cost on constrained microcontrollers (ESP32 / STM32 / nRF52 /
RP2040), and to provide a FRI-based prover and verifier that run on MCU-class
hardware.

## Repository layout

```
phd_code/
  python/        Reference implementation (prover + verifier), all 3 fields.
  c/             Portable C implementation (prover + verifier), all 3 fields.
  cpp/           C++ wrapper + Arduino/ESP32 sketch (thin layer over the C code).
  legacy/        Older BabyBear-only C verifier, kept for reference.
  files.zip      Original delivered bundle (superseded; safe to delete).
```

The three language tiers are deliberately separated so each can be read on its
own:

- **`python/`** is the readable reference. Pure Python (MicroPython-compatible,
  stdlib only). Use it to understand the protocol and as the cross-check oracle.
- **`c/`** is the real embedded implementation: field-generic, fully static
  allocation, one binary per field. This is what the dissertation measurements
  run on.
- **`cpp/`** is a thin C++ class plus an Arduino `.ino` sketch that wraps the C
  verifier for on-device deployment.
- **`legacy/`** is the first-generation BabyBear-only C verifier. It is
  superseded by `c/` but retained because the current Arduino sketch builds
  against it and because it documents the project's history.

## The research question

A hybrid deployment for IoT mesh authentication: a heavy **prover** runs off
device (gateway / PC), and a lightweight **verifier** runs in C on the sensor
node. The open question is how small a field, and how large a circuit, fit on
real MCU silicon, and whether 31-bit fields (BabyBear/KoalaBear) beat 64-bit
Goldilocks on a 32-bit MCU. They do, and by a clear margin.

## Quick start

Python reference (proves x^3 + x + 5 = 35):
```
cd python
set ZKP_FIELD=babybear&& python demo.py        # Windows cmd
ZKP_FIELD=babybear python3 demo.py             # Linux/macOS
```

C implementation (prover + verifier, all three fields):
```
cd c
mingw32-make test          # Windows (Strawberry GCC)
make test                  # Linux/macOS
```

C unit tests + cross-checks against Python:
```
cd c/tests && mingw32-make
```

## Cross-check guarantee

Two independent implementations agree byte-for-byte. A proof produced by the C
prover verifies under the Python verifier and vice versa, for all three fields.
This is the project's main correctness argument and it lives in
`c/tests/crosscheck/`.

| | Python verify | C verify |
| --- | --- | --- |
| Python prove | yes | yes |
| C prove | yes | yes |

## Key result

On a 32-bit MCU the field width decides whether a circuit fits. At a 512 KB SRAM
budget the 31-bit fields (BabyBear/KoalaBear) prove a 4x larger circuit than
64-bit Goldilocks. The full fit-boundary sweep is in `c/bench/`.

## Status and honest caveats

- Python prover + verifier: working for all three fields.
- C prover + verifier: working for all three fields; compiles clean with
  `-Wall -Wextra`; agrees with Python.
- NOT yet run on real ESP32/STM32 hardware. All footprint numbers so far are
  static host-side reservations, not silicon measurements. On-device cycle and
  RAM figures are the next step.
- The C verifier in `legacy/` is BabyBear-only by design.
- Goldilocks `f_mul` uses `__uint128_t` (fine on host and ESP32 GCC); the
  manual-reduction fallback for true 32-bit targets is present but unvalidated.

## Scope note

This is a Plonky2-family construction (Goldilocks + PLONK + FRI + DEEP) extended
with Plonky3's small fields, implemented for embedded characterisation. It is not
a port of Plonky3's Rust toolkit. The contribution is the measurement on 32-bit
MCU hardware.
# plonk
