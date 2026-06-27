/*
 * merkle.h -- SHA-256 Merkle tree (construction + open + verify).
 *
 * Leaf/element encoding matches the Python reference EXACTLY:
 *   - a field element is hashed as 8-byte little-endian, regardless of field
 *     width (Goldilocks, BabyBear, KoalaBear all use 8 bytes here). This is
 *     what plonk_fri/merkle.py _encode() does.
 *   - leaf  = SHA256(0x00 || 8-byte-LE(value))
 *   - node  = SHA256(0x01 || left(32) || right(32))
 *
 * The prover builds a full tree; the verifier only checks a path.
 */
#ifndef PLONK_MERKLE_H
#define PLONK_MERKLE_H

#include <stddef.h>
#include <stdint.h>
#include "field.h"
#include "config.h"

#define HASH_BYTES 32

void merkle_leaf(felt value, uint8_t out[HASH_BYTES]);
void merkle_node(const uint8_t left[HASH_BYTES], const uint8_t right[HASH_BYTES],
                 uint8_t out[HASH_BYTES]);

/* A built tree. `nodes` holds all levels concatenated, level 0 = leaves.
 * Caller provides storage via merkle_tree_build into a scratch buffer sized
 * by merkle_tree_storage(n). */
typedef struct {
    uint32_t n;            /* number of leaves (power of two) */
    uint8_t  log_n;
    uint8_t *nodes;        /* (2n) * 32 bytes: level0 (n) + level1 (n/2) + ... + root(1), plus pad */
    uint32_t level_off[MAX_LOG_NEXT + 2];/* byte offset of each level within nodes */
    uint8_t  num_levels;
} merkle_tree_t;

/* Bytes needed to store a tree of n leaves. */
size_t merkle_tree_storage(uint32_t n);

/* Build a tree over `leaves` (length n) into the caller-provided `storage`
 * (>= merkle_tree_storage(n) bytes). */
void merkle_tree_build(merkle_tree_t *t, const felt *leaves, uint32_t n,
                       uint8_t *storage);

/* Copy the 32-byte root. */
void merkle_tree_root(const merkle_tree_t *t, uint8_t out[HASH_BYTES]);

/* Write the authentication path for `index` into `out_path`
 * (log_n * 32 bytes). */
void merkle_tree_open(const merkle_tree_t *t, uint32_t index, uint8_t *out_path);

/* Verify a path. Returns 1 on accept. */
int merkle_verify(const uint8_t root[HASH_BYTES], felt value, uint32_t index,
                  const uint8_t *siblings, size_t path_len);

#endif
