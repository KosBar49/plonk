"""
BabyBear field: p = 15 * 2^27 + 1 = 2013265921.

31-bit prime introduced by Plonky3 and used by SP1's zkVM. Designed for 32-bit
hosts: field elements fit in a 32-bit word and products fit in 64 bits (single
multiply with hardware support on most 32-bit MCUs including Xtensa and ARM
Cortex-M with DSP instructions).

Two-adicity 27 (p - 1 = 15 * 2^27) supports NTT domains up to 2^27, more than
enough for any IoT-scale circuit.

Reduction on a 32-bit MCU: a 64-bit product (a, b) < 2^62 reduces in roughly
  q = (high * 15) >> ...  // Plantard / Montgomery-style fast reduction
ymmv per platform. Pure Python `% P` here.
"""

FIELD_NAME = "babybear"
FIELD_BITS = 31

P = 15 * (1 << 27) + 1
assert P == 2013265921

# Multiplicative generator of F_p* (per Plonky3 source).
MULT_GEN = 31

# p - 1 = 15 * 2^27, so two-adicity is 27 and the odd cofactor is 15.
TWO_ADICITY = 27
# Compute TWO_ADIC_GEN as MULT_GEN raised to the odd cofactor — this gives an
# element of order exactly 2^27.
TWO_ADIC_GEN = pow(MULT_GEN, 15, P)

# Verify
assert pow(TWO_ADIC_GEN, 1 << TWO_ADICITY, P) == 1, "TWO_ADIC_GEN not 2^27-th root"
assert pow(TWO_ADIC_GEN, 1 << (TWO_ADICITY - 1), P) == P - 1, "TWO_ADIC_GEN not primitive"

# PLONK column coset shifts. K1 = MULT_GEN, K2 = MULT_GEN^2. We verify that
# neither lies in the small subgroups we'll use (asserts in `setup`).
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
    """Return omega such that omega has order exactly 2^log_n."""
    assert 0 <= log_n <= TWO_ADICITY, "log_n out of range for BabyBear (max 27)"
    g = TWO_ADIC_GEN
    for _ in range(TWO_ADICITY - log_n):
        g = (g * g) % P
    return g
