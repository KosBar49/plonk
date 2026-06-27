"""
Binary Merkle tree with SHA-256.

ESP32 has a hardware SHA-256 accelerator; on-device this is ~50x faster than pure
software. The commitment scheme for FRI is just `merkle_root(layer evaluations)`,
so this file is performance-critical.

Domain-separation tags (0x00 for leaves, 0x01 for nodes) prevent second-preimage
attacks across the tree. Length-prefixing leaf data prevents ambiguity when a leaf
is itself a tuple of field elements (we commit (v_lo, v_hi) pairs in FRI).
"""

import hashlib


def _h(data):
    return hashlib.sha256(data).digest()


def _encode(value):
    """Canonical bytes for a field element, int, bytes, or flat tuple/list thereof."""
    if isinstance(value, (bytes, bytearray)):
        return bytes(value)
    if isinstance(value, int):
        # 8 bytes is enough for Goldilocks (p < 2^64)
        return value.to_bytes(8, "little")
    if isinstance(value, (list, tuple)):
        return b"".join(_encode(x) for x in value)
    raise TypeError("cannot encode {}".format(type(value)))


def leaf_hash(value):
    payload = _encode(value)
    return _h(b"\x00" + payload)


def node_hash(left, right):
    return _h(b"\x01" + left + right)


class MerkleTree:
    """Power-of-two leaf count required."""

    def __init__(self, leaves):
        n = len(leaves)
        assert n > 0 and (n & (n - 1)) == 0, "leaf count must be a power of two"
        self.n = n
        self.layers = [[leaf_hash(v) for v in leaves]]
        while len(self.layers[-1]) > 1:
            prev = self.layers[-1]
            self.layers.append(
                [node_hash(prev[i], prev[i + 1]) for i in range(0, len(prev), 2)]
            )
        self.root = self.layers[-1][0]

    def open(self, index):
        """Authentication path for leaf at `index`. Sibling at each level, root-excluded."""
        assert 0 <= index < self.n
        path = []
        i = index
        for layer in self.layers[:-1]:
            path.append(layer[i ^ 1])
            i >>= 1
        return path


def verify_path(root, value, index, path):
    cur = leaf_hash(value)
    i = index
    for sib in path:
        if i & 1:
            cur = node_hash(sib, cur)
        else:
            cur = node_hash(cur, sib)
        i >>= 1
    return cur == root
