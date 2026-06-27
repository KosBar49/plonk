/*
 * PlonkVerifier.ino — ESP32 demo sketch.
 *
 * Verifies an embedded PLONK+FRI proof (proof_data.h) for the circuit
 * x^3 + x + 5 = 35, and reports the result + timing over Serial.
 *
 * TARGET: ESP32 / ESP32-S3 (uses hardware-accelerated SHA-256 via mbedtls,
 *         which the Arduino-ESP32 core bundles). For other boards, the
 *         portable software SHA-256 in sha256.c is used automatically.
 *
 * BUILD (Arduino IDE):
 *   1. Put these files in the sketch folder (same directory as the .ino):
 *        PlonkVerifier.h    proof_data.h
 *        plonk_verifier.c   plonk_verifier.h
 *        proof.c            proof.h
 *        transcript.c       transcript.h
 *        merkle.c           merkle.h
 *        sha256.c           sha256.h
 *        babybear.c         babybear.h
 *        circuit_data.c     circuit_data.h
 *   2. Select your ESP32 board, compile, upload.
 *   3. Open Serial Monitor at 115200 baud.
 *
 * NOTE: the .c files compile as C automatically (Arduino compiles .c with the
 * C compiler and .ino/.cpp with C++), so the extern "C" wrapper in
 * PlonkVerifier.h links them correctly.
 */

#include "PlonkVerifier.h"
#include "proof_data.h"

PlonkVerifier verifier;

void setup() {
    Serial.begin(115200);
    while (!Serial) { ; }
    delay(500);

    Serial.println();
    Serial.println(F("PLONK+FRI verifier on-device demo"));
    Serial.println(F("Circuit: prove knowledge of x with x^3 + x + 5 = 35"));
    Serial.print(F("Proof size: "));
    Serial.print(proof_data_len);
    Serial.println(F(" bytes"));

    Serial.print(F("Free heap before: "));
    Serial.println(ESP.getFreeHeap());

    unsigned long t0 = micros();
    bool ok = verifier.verify(proof_data, proof_data_len);
    unsigned long t1 = micros();

    Serial.print(F("Free heap after:  "));
    Serial.println(ESP.getFreeHeap());

    Serial.println();
    if (ok) {
        Serial.println(F(">>> Verification: ACCEPT"));
    } else {
        Serial.print(F(">>> Verification: REJECT ("));
        Serial.print(verifier.error());
        Serial.println(F(")"));
    }
    Serial.print(F("Verify time: "));
    Serial.print((t1 - t0) / 1000.0, 3);
    Serial.println(F(" ms"));
}

void loop() {
    /* Re-run periodically so you can watch timing stability / thermal effects. */
    delay(5000);

    unsigned long t0 = micros();
    bool ok = verifier.verify(proof_data, proof_data_len);
    unsigned long t1 = micros();

    Serial.print(F("re-verify: "));
    Serial.print(ok ? F("ACCEPT") : F("REJECT"));
    Serial.print(F("  ("));
    Serial.print((t1 - t0) / 1000.0, 3);
    Serial.println(F(" ms)"));
}
