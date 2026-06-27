"""
Number Theoretic Transform over Goldilocks.

In-place radix-2 Cooley-Tukey. The NTT lets us evaluate a degree-(n-1) polynomial
on the n-th roots of unity in O(n log n) instead of O(n^2) Horner evaluations.

Cost on ESP32 is the main bottleneck of any STARK-style prover: a 2^14 NTT in pure
MicroPython is dozens of seconds. In C with Plantard reduction it drops to ~100 ms.
"""

from field import P, inv, mul, add, sub, pow_, primitive_root_of_unity


def _bit_reverse_permute(a):
    """In-place bit-reversal permutation."""
    n = len(a)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            a[i], a[j] = a[j], a[i]


def ntt(a, inverse=False):
    """In-place radix-2 NTT. len(a) must be a power of two. Returns a new list."""
    n = len(a)
    if n == 1:
        return list(a)
    log_n = n.bit_length() - 1
    assert (1 << log_n) == n, "NTT requires power-of-two length"

    a = list(a)
    _bit_reverse_permute(a)

    omega = primitive_root_of_unity(log_n)
    if inverse:
        omega = inv(omega)

    length = 2
    while length <= n:
        # principal length-th root of unity = omega^(n/length)
        w_step = pow_(omega, n // length)
        half = length >> 1
        for i in range(0, n, length):
            w = 1
            for j in range(half):
                u = a[i + j]
                v = (a[i + j + half] * w) % P
                a[i + j] = (u + v) % P
                a[i + j + half] = (u - v) % P
                w = (w * w_step) % P
        length <<= 1

    if inverse:
        n_inv = inv(n)
        a = [(x * n_inv) % P for x in a]
    return a


def coset_ntt(coeffs, shift, target_log_n):
    """
    Evaluate poly `coeffs` on the coset {shift * omega^i : 0 <= i < 2^target_log_n}
    where omega is the primitive (2^target_log_n)-th root of unity.

    Equivalent to: pad coeffs to length 2^target_log_n, multiply c_i <- c_i * shift^i,
    then NTT.
    """
    n = 1 << target_log_n
    assert len(coeffs) <= n, "polynomial larger than target domain"
    padded = list(coeffs) + [0] * (n - len(coeffs))
    cur = 1
    for i in range(n):
        padded[i] = (padded[i] * cur) % P
        cur = (cur * shift) % P
    return ntt(padded, inverse=False)


def coset_intt(values, shift, source_log_n):
    """Inverse of coset_ntt: from evaluations on coset back to coefficients."""
    n = 1 << source_log_n
    assert len(values) == n
    b = ntt(values, inverse=True)
    shift_inv = inv(shift)
    cur = 1
    for i in range(n):
        b[i] = (b[i] * cur) % P
        cur = (cur * shift_inv) % P
    return b
