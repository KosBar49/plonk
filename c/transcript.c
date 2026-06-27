#include "transcript.h"
#include <assert.h>
#include <string.h>

static void u32le(uint32_t v, uint8_t out[4]) {
    out[0] = (uint8_t)v; out[1] = (uint8_t)(v >> 8);
    out[2] = (uint8_t)(v >> 16); out[3] = (uint8_t)(v >> 24);
}

void transcript_init(transcript_t *t, const char *label, size_t label_len) {
    plonk_sha256_init(&t->h);
    plonk_sha256_update(&t->h, (const uint8_t *)"INIT", 4);
    uint8_t l2[2] = { (uint8_t)(label_len & 0xff), (uint8_t)((label_len >> 8) & 0xff) };
    plonk_sha256_update(&t->h, l2, 2);
    plonk_sha256_update(&t->h, (const uint8_t *)label, label_len);
}

/* internal: update with tag bytes + u32 length + data */
static void absorb_raw(transcript_t *t, const uint8_t *tag, size_t tag_len,
                       const uint8_t *data, size_t len) {
    plonk_sha256_update(&t->h, tag, tag_len);
    uint8_t dl[4]; u32le((uint32_t)len, dl);
    plonk_sha256_update(&t->h, dl, 4);
    if (len) plonk_sha256_update(&t->h, data, len);
}

void transcript_absorb_bytes(transcript_t *t, const char *label,
                             const uint8_t *data, size_t len) {
    /* Python appends ':' to the label for ordinary absorbs. */
    size_t ll = strlen(label);
    uint8_t tag[64];
    assert(ll + 1 <= sizeof(tag));
    memcpy(tag, label, ll); tag[ll] = ':';
    absorb_raw(t, tag, ll + 1, data, len);
}

void transcript_absorb_root(transcript_t *t, const char *label, const uint8_t root[32]) {
    transcript_absorb_bytes(t, label, root, 32);
}

void transcript_absorb_elem(transcript_t *t, const char *label, felt v) {
    uint8_t b[8]; felt_to_8le(v, b);
    transcript_absorb_bytes(t, label, b, 8);
}

void transcript_absorb_elems(transcript_t *t, const char *label,
                             const felt *vs, size_t count) {
    uint8_t buf[16 * 8];
    assert(count <= 16);
    for (size_t i = 0; i < count; ++i) felt_to_8le(vs[i], buf + i * 8);
    transcript_absorb_bytes(t, label, buf, count * 8);
}

static void squeeze(transcript_t *t, uint8_t out[32]) {
    plonk_sha256_ctx tmp = t->h;          /* copy to finalize without destroying */
    plonk_sha256_finish(&tmp, out);
    /* re-key: state = SHA("SQUEEZE" || digest) */
    plonk_sha256_init(&t->h);
    plonk_sha256_update(&t->h, (const uint8_t *)"SQUEEZE", 7);
    plonk_sha256_update(&t->h, out, 32);
}

felt transcript_challenge_field(transcript_t *t, const char *tag) {
    size_t tl = strlen(tag);
    uint8_t buf[64];
    assert(5 + tl <= sizeof(buf));
    memcpy(buf, "CHAL:", 5); memcpy(buf + 5, tag, tl);
    absorb_raw(t, buf, 5 + tl, NULL, 0);

    uint8_t d[32]; squeeze(t, d);
    /* int_from_le(d) % p, reducing from the most-significant byte down.
     * Use a 128-bit intermediate so (acc<<8)|byte cannot overflow even when
     * p is close to 2^64 (Goldilocks). On a 32-bit MCU without __int128,
     * GOLDILOCKS_NO_INT128 selects a manual path in the field header; here we
     * rely on the compiler's __uint128_t which is available on host and ESP32. */
    __uint128_t acc = 0;
    __uint128_t p = (__uint128_t)(uint64_t)F_P;
    for (int i = 31; i >= 0; --i) { acc = (acc << 8) | d[i]; acc %= p; }
    return f_from_u64((uint64_t)acc);
}

uint32_t transcript_challenge_int(transcript_t *t, const char *tag, uint32_t bound) {
    size_t tl = strlen(tag);
    uint8_t buf[64];
    assert(5 + tl <= sizeof(buf));
    memcpy(buf, "CHAL:", 5); memcpy(buf + 5, tag, tl);
    absorb_raw(t, buf, 5 + tl, NULL, 0);

    uint8_t d[32]; squeeze(t, d);
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | d[i];
    return (uint32_t)(v % bound);
}
