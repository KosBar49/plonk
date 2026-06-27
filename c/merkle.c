#include "merkle.h"
#include "sha256.h"
#include <string.h>

void merkle_leaf(felt value, uint8_t out[HASH_BYTES]) {
    uint8_t buf[9];
    buf[0] = 0x00;
    felt_to_8le(value, buf + 1);
    plonk_sha256(buf, sizeof(buf), out);
}

void merkle_node(const uint8_t left[HASH_BYTES], const uint8_t right[HASH_BYTES],
                 uint8_t out[HASH_BYTES]) {
    uint8_t buf[1 + 2 * HASH_BYTES];
    buf[0] = 0x01;
    memcpy(buf + 1, left, HASH_BYTES);
    memcpy(buf + 1 + HASH_BYTES, right, HASH_BYTES);
    plonk_sha256(buf, sizeof(buf), out);
}

size_t merkle_tree_storage(uint32_t n) {
    /* total nodes = n + n/2 + ... + 1 = 2n - 1; round to 2n for simplicity */
    return (size_t)2 * n * HASH_BYTES;
}

void merkle_tree_build(merkle_tree_t *t, const felt *leaves, uint32_t n,
                       uint8_t *storage) {
    t->n = n;
    t->nodes = storage;
    uint8_t log_n = 0;
    while ((1u << log_n) < n) ++log_n;
    t->log_n = log_n;

    /* level 0: leaves */
    uint32_t off = 0;
    t->level_off[0] = 0;
    for (uint32_t i = 0; i < n; ++i) {
        merkle_leaf(leaves[i], storage + (size_t)(off + i) * HASH_BYTES);
    }
    uint32_t level_count = n;
    uint8_t level = 0;
    off = n;
    while (level_count > 1) {
        t->level_off[level + 1] = off * HASH_BYTES;
        uint32_t parent_count = level_count >> 1;
        const uint8_t *prev = storage + t->level_off[level];
        uint8_t *cur = storage + t->level_off[level + 1];
        for (uint32_t i = 0; i < parent_count; ++i) {
            merkle_node(prev + (size_t)(2 * i) * HASH_BYTES,
                        prev + (size_t)(2 * i + 1) * HASH_BYTES,
                        cur + (size_t)i * HASH_BYTES);
        }
        off += parent_count;
        level_count = parent_count;
        ++level;
    }
    t->num_levels = level + 1;
}

void merkle_tree_root(const merkle_tree_t *t, uint8_t out[HASH_BYTES]) {
    /* root is the single node in the last level */
    const uint8_t *root = t->nodes + t->level_off[t->num_levels - 1];
    memcpy(out, root, HASH_BYTES);
}

void merkle_tree_open(const merkle_tree_t *t, uint32_t index, uint8_t *out_path) {
    uint32_t i = index;
    for (uint8_t level = 0; level + 1 < t->num_levels; ++level) {
        const uint8_t *lvl = t->nodes + t->level_off[level];
        uint32_t sib = i ^ 1u;
        memcpy(out_path + (size_t)level * HASH_BYTES,
               lvl + (size_t)sib * HASH_BYTES, HASH_BYTES);
        i >>= 1;
    }
}

int merkle_verify(const uint8_t root[HASH_BYTES], felt value, uint32_t index,
                  const uint8_t *siblings, size_t path_len) {
    uint8_t cur[HASH_BYTES];
    merkle_leaf(value, cur);
    uint32_t i = index;
    for (size_t k = 0; k < path_len; ++k) {
        const uint8_t *sib = siblings + k * HASH_BYTES;
        uint8_t next[HASH_BYTES];
        if (i & 1u) merkle_node(sib, cur, next);
        else        merkle_node(cur, sib, next);
        memcpy(cur, next, HASH_BYTES);
        i >>= 1;
    }
    return memcmp(cur, root, HASH_BYTES) == 0;
}
