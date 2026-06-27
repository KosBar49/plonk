"""
Goldilocks field: p = 2^64 - 2^32 + 1.

64-bit prime. Plonky2's default. Cheap reduction in C (two subtractions). Two-adicity
2^32 means NTT domains up to 2^32 are available.

On a 32-bit MCU, multiplication of two field elements yields a 128-bit intermediate,
which is the slow path. Small-field alternatives (BabyBear, KoalaBear) avoid this.
"""

FIELD_NAME = "goldilocks"
FIELD_BITS = 64

P = (1 << 64) - (1 << 32) + 1
assert P == 0xFFFFFFFF00000001

# Multiplicative generator of F_p* (also generates the whole group of order p-1).
MULT_GEN = 7

# Two-adicity: p - 1 = 2^32 * (2^32 - 1).
TWO_ADICITY = 32
# Standard primitive 2^32-th root of unity (matches plonky2).
TWO_ADIC_GEN = 1753635133440165772

# Verify at import time
assert pow(TWO_ADIC_GEN, 1 << TWO_ADICITY, P) == 1, "TWO_ADIC_GEN not 2^32-th root"
assert pow(TWO_ADIC_GEN, 1 << (TWO_ADICITY - 1), P) == P - 1, "TWO_ADIC_GEN not primitive"

# PLONK column coset shifts: H, K1*H, K2*H must be disjoint cosets.
# Standard choice: K1 = MULT_GEN, K2 = MULT_GEN^2.
K1 = 7
K2 = 49


def add(a, b):  return (a + b) % P
def sub(a, b):  return (a - b) % P
def mul(a, b):  return (a * b) % P
def neg(a):     return (-a) % P
def pow_(a, n): return pow(a, n, P)
def inv(a):     return pow(a, P - 2, P)
def div(a, b):  return (a * pow(b, P - 2, P)) % P


def primitive_root_of_unity(log_n):
    """Return omega such that omega has order exactly 2^log_n."""
    assert 0 <= log_n <= TWO_ADICITY, "log_n out of range for Goldilocks"
    g = TWO_ADIC_GEN
    for _ in range(TWO_ADICITY - log_n):
        g = (g * g) % P
    return g
