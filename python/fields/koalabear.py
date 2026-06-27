"""
KoalaBear field: p = 2^31 - 2^24 + 1 = 2130706433.

31-bit prime, slightly larger than BabyBear. p - 1 = 127 * 2^24, so two-adicity
is 24 (still plenty for IoT). Less smooth than BabyBear (odd cofactor 127 vs 15)
which slightly affects the Karatsuba/Montgomery reduction choice in C, but the
field element still fits in 32 bits.

Plonky3 ships this as a "balanced" alternative to BabyBear — slightly worse
2-adicity, slightly different reduction tricks.
"""

FIELD_NAME = "koalabear"
FIELD_BITS = 31

P = (1 << 31) - (1 << 24) + 1
assert P == 2130706433

# Multiplicative generator (per Plonky3 source).
MULT_GEN = 3

# p - 1 = 127 * 2^24
TWO_ADICITY = 24
TWO_ADIC_GEN = pow(MULT_GEN, 127, P)

assert pow(TWO_ADIC_GEN, 1 << TWO_ADICITY, P) == 1, "TWO_ADIC_GEN not 2^24-th root"
assert pow(TWO_ADIC_GEN, 1 << (TWO_ADICITY - 1), P) == P - 1, "TWO_ADIC_GEN not primitive"

# PLONK coset shifts. MULT_GEN = 3 → K1 = 3, K2 = 9.
K1 = MULT_GEN
K2 = (MULT_GEN * MULT_GEN) % P


def add(a, b):  return (a + b) % P
def sub(a, b):  return (a - b) % P
def mul(a, b):  return (a * b) % P
def neg(a):     return (-a) % P
def pow_(a, n): return pow(a, n, P)
def inv(a):     return pow(a, P - 2, P)
def div(a, b):  return (a * pow(b, P - 2, P)) % P


def primitive_root_of_unity(log_n):
    assert 0 <= log_n <= TWO_ADICITY, "log_n out of range for KoalaBear (max 24)"
    g = TWO_ADIC_GEN
    for _ in range(TWO_ADICITY - log_n):
        g = (g * g) % P
    return g
