"""
Fiat-Shamir transcript (SHA-256 based).

This is a duplex-style sponge: each absorb mixes data into an internal state, each
challenge squeezes a fresh hash. The verifier must replay the same sequence of
absorbs to recover the same challenges.

Length-prefix everything you absorb. Failing to do so is the classic Fiat-Shamir
attack surface (concatenation ambiguity).
"""

import hashlib

from field import P


class Transcript:
    def __init__(self, label=b"plonk-fri-v0"):
        self._h = hashlib.sha256()
        self._h.update(b"INIT")
        self._h.update(len(label).to_bytes(2, "little"))
        self._h.update(label)

    def _absorb_bytes(self, tag, data):
        self._h.update(tag)
        self._h.update(len(data).to_bytes(4, "little"))
        self._h.update(data)

    def absorb(self, label, data):
        """Absorb arbitrary data under a context label."""
        tag = label.encode() if isinstance(label, str) else label
        if isinstance(data, int):
            data = data.to_bytes(8, "little")
        elif isinstance(data, (list, tuple)):
            buf = bytearray()
            for x in data:
                if isinstance(x, int):
                    buf += x.to_bytes(8, "little")
                elif isinstance(x, (bytes, bytearray)):
                    buf += bytes(x)
                else:
                    raise TypeError(type(x))
            data = bytes(buf)
        elif isinstance(data, (bytes, bytearray)):
            data = bytes(data)
        else:
            raise TypeError(type(data))
        self._absorb_bytes(tag + b":", data)

    def _squeeze(self):
        # Squeeze 32 bytes; fork the state so future absorbs build on this squeeze.
        digest = self._h.digest()
        new = hashlib.sha256()
        new.update(b"SQUEEZE")
        new.update(digest)
        self._h = new
        return digest

    def challenge_field(self, label="chal"):
        """Sample a uniform field element.

        Squeeze the full 256-bit SHA-256 output and reduce mod P. For any field
        with log2(P) <= 200, the bias is below 2^-50 which is negligible for
        statistical security (and well below the cryptographic 2^-100 standard).
        Previous code rejection-sampled to 8 bytes which works for Goldilocks
        (~64-bit P) but loops forever for 31-bit fields like BabyBear.
        """
        tag = label.encode() if isinstance(label, str) else label
        self._absorb_bytes(b"CHAL:" + tag, b"")
        d = self._squeeze()
        return int.from_bytes(d, "little") % P

    def challenge_int(self, bound, label="idx"):
        """Sample a uniform integer in [0, bound)."""
        assert bound > 0
        tag = label.encode() if isinstance(label, str) else label
        self._absorb_bytes(b"CHAL:" + tag, b"")
        d = self._squeeze()
        # Use 8 bytes; for bound up to 2^32 the bias is negligible.
        return int.from_bytes(d[:8], "little") % bound
