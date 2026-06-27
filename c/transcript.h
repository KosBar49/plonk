/*
 * transcript.h -- Fiat-Shamir SHA-256 sponge. Byte-for-byte match with
 * plonk_fri/transcript.py.
 *
 * Conventions (from the Python reference):
 *   init:    SHA(b"INIT" || u16_le(len(label)) || label)
 *   absorb(label,data): update(label || ':' || u32_le(len(data)) || data)
 *   challenge: update("CHAL:" || tag || u32_le(0));    [no trailing colon!]
 *              digest = state.digest();
 *              state  = SHA(b"SQUEEZE" || digest);
 *              field element  = int_from_le(digest[0:32]) % p
 *              int in [0,bnd) = int_from_le(digest[0:8]) % bnd
 *
 *   A single field element absorbed as data is 8-byte LE (Python int branch).
 *   A list of elements is the concatenation of 8-byte LE encodings.
 */
#ifndef PLONK_TRANSCRIPT_H
#define PLONK_TRANSCRIPT_H

#include <stddef.h>
#include <stdint.h>
#include "field.h"
#include "sha256.h"

typedef struct {
    plonk_sha256_ctx h;
} transcript_t;

void transcript_init(transcript_t *t, const char *label, size_t label_len);
void transcript_absorb_bytes(transcript_t *t, const char *label,
                             const uint8_t *data, size_t len);
void transcript_absorb_root(transcript_t *t, const char *label, const uint8_t root[32]);
void transcript_absorb_elem(transcript_t *t, const char *label, felt v);
/* Absorb a list of field elements under one label (8-byte LE each). */
void transcript_absorb_elems(transcript_t *t, const char *label,
                             const felt *vs, size_t count);

felt     transcript_challenge_field(transcript_t *t, const char *tag);
uint32_t transcript_challenge_int(transcript_t *t, const char *tag, uint32_t bound);

#endif
