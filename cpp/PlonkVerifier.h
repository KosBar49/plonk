/*
 * PlonkVerifier — Arduino-friendly C++ wrapper around the C verifier core.
 *
 * The heavy lifting stays in plain C (plonk_verifier.c et al.) so it remains
 * portable, fast, and directly comparable to embedded-crypto C baselines.
 * This class only provides ergonomic C++ access for Arduino sketches.
 *
 * Usage:
 *     #include "PlonkVerifier.h"
 *     PlonkVerifier verifier;
 *     bool ok = verifier.verify(proofBytes, proofLen);
 */

#ifndef PLONK_VERIFIER_HPP
#define PLONK_VERIFIER_HPP

#include <stddef.h>
#include <stdint.h>

extern "C" {
  #include "proof.h"
  #include "plonk_verifier.h"
}

class PlonkVerifier {
public:
    PlonkVerifier() : lastError_(nullptr) {}

    /* Verify a serialized proof. Returns true on accept, false on reject or
       malformed input. On failure, error() returns a human-readable reason. */
    bool verify(const uint8_t *proofData, size_t len) {
        lastError_ = nullptr;
        plonk_proof_t proof;
        if (!plonk_proof_load(&proof, proofData, len)) {
            lastError_ = "malformed proof (bad header or truncated)";
            return false;
        }
        if (plonk_verify(&proof) != 1) {
            lastError_ = "verification failed (invalid proof)";
            return false;
        }
        return true;
    }

    /* Parse the header without full verification — useful for sanity checks
       and for sizing buffers before streaming a proof in over Serial. */
    bool inspect(const uint8_t *proofData, size_t len, plonk_proof_t *outHeader) {
        return plonk_proof_load(outHeader, proofData, len) == 1;
    }

    const char *error() const { return lastError_; }

private:
    const char *lastError_;
};

#endif /* PLONK_VERIFIER_HPP */
