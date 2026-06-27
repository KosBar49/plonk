# cpp/ -- C++ wrapper and Arduino/ESP32 sketch

A thin C++ layer over the C verifier, for running on-device.

```
PlonkVerifier.h     C++ class wrapping the C verifier (RAII, simple verify() API)
PlonkVerifier.ino   Arduino sketch: loads a proof, verifies it, prints timing + free heap
proof_data.h        A proof embedded as a C byte array (generated from the Python side)
```

## What this is

The heavy lifting stays in C. This folder only adds:
- a small C++ class so Arduino/ESP-IDF code can call the verifier idiomatically,
- a sketch that measures verify time and free-heap delta on the actual board --
  those are the on-device numbers for the paper.

## Assembling an Arduino sketch

The Arduino IDE compiles one folder as one sketch, so all C sources must sit next
to the `.ino`. To build for an ESP32:

1. Create a sketch folder named `PlonkVerifier/`.
2. Copy into it:
   - everything from this `cpp/` folder, and
   - the C verifier sources it builds against: from `../legacy/c_verifier/`
     copy `babybear.{c,h}`, `sha256.{c,h}`, `transcript.{c,h}`, `merkle.{c,h}`,
     `proof.{c,h}`, `plonk_verifier.{c,h}`, plus the generated
     `circuit_data.{c,h}`.
3. Open the folder in the Arduino IDE, select an ESP32 board, and upload.
4. Open Serial Monitor at 115200 baud to see the verdict, verify time, and
   free-heap delta.

On ESP32 the SHA-256 calls route to the hardware accelerator automatically
through the Arduino-ESP32 mbedtls build.

## Note on which C core this wraps

The sketch currently builds against the BabyBear-only verifier in
`../legacy/c_verifier/`. The newer field-generic implementation in `../c/` also
contains a verifier (`verifier.c`) supporting all three fields; wiring the sketch
to build against `../c/` instead is a natural next step and would let the same
on-device harness measure Goldilocks and KoalaBear as well.

## Regenerating the embedded proof

`proof_data.h` (and the legacy `proof.bin` / `circuit_data.*`) are produced from
the Python side:

```
cd ../python
set ZKP_FIELD=babybear&& python export_to_c.py
```

That writes the proof and circuit constants into `../legacy/c_verifier/`. See that
folder's README for converting `proof.bin` into the `proof_data.h` byte array.
