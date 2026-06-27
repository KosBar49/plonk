/* Merkle cross-check: build a tree over leaves 0..7, print root hex.
 * Compare against Python merkle.MerkleTree on the same leaves. */
#include <stdio.h>
#include <stdint.h>
#include "field.h"
#include "merkle.h"

int main(void) {
    uint32_t n = 8;
    felt leaves[8];
    for (uint32_t i = 0; i < n; ++i) leaves[i] = f_from_u64(100 + i);

    uint8_t storage[2 * 8 * HASH_BYTES];
    merkle_tree_t t;
    merkle_tree_build(&t, leaves, n, storage);

    uint8_t root[HASH_BYTES];
    merkle_tree_root(&t, root);
    printf("field=%s root=", FIELD_NAME);
    for (int i = 0; i < HASH_BYTES; ++i) printf("%02x", root[i]);
    printf("\n");

    /* open index 3 and verify */
    uint8_t path[8 * HASH_BYTES];
    merkle_tree_open(&t, 3, path);
    int ok = merkle_verify(root, leaves[3], 3, path, t.log_n);
    printf("open+verify idx3: %s\n", ok ? "OK" : "FAIL");

    /* wrong value must fail */
    int bad = merkle_verify(root, f_from_u64(999), 3, path, t.log_n);
    printf("verify wrong val: %s\n", bad ? "FAIL(accepted!)" : "OK(rejected)");
    return 0;
}
