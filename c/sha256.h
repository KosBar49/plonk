#ifndef PLONK_SHA256_H
#define PLONK_SHA256_H

#include <stddef.h>
#include <stdint.h>

/* On ESP32 with Arduino-ESP32 / ESP-IDF, use the hardware-accelerated mbedtls
   implementation. Otherwise fall back to the portable SHA-256 in sha256.c. */
#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32)
  #include "mbedtls/sha256.h"
  typedef mbedtls_sha256_context plonk_sha256_ctx;
  static inline void plonk_sha256_init(plonk_sha256_ctx *ctx) {
      mbedtls_sha256_init(ctx);
      mbedtls_sha256_starts(ctx, 0);  /* 0 = SHA-256, not SHA-224 */
  }
  static inline void plonk_sha256_update(plonk_sha256_ctx *ctx, const uint8_t *data, size_t len) {
      mbedtls_sha256_update(ctx, data, len);
  }
  static inline void plonk_sha256_finish(plonk_sha256_ctx *ctx, uint8_t out[32]) {
      mbedtls_sha256_finish(ctx, out);
      mbedtls_sha256_free(ctx);
  }
#else
  /* Portable fallback */
  typedef struct {
      uint8_t  data[64];
      uint32_t datalen;
      uint64_t bitlen;
      uint32_t state[8];
  } plonk_sha256_ctx;

  void plonk_sha256_init(plonk_sha256_ctx *ctx);
  void plonk_sha256_update(plonk_sha256_ctx *ctx, const uint8_t *data, size_t len);
  void plonk_sha256_finish(plonk_sha256_ctx *ctx, uint8_t out[32]);
#endif

/* One-shot helper, present on all platforms. */
static inline void plonk_sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
    plonk_sha256_ctx ctx;
    plonk_sha256_init(&ctx);
    plonk_sha256_update(&ctx, data, len);
    plonk_sha256_finish(&ctx, out);
}

#endif /* PLONK_SHA256_H */
