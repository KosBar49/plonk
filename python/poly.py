"""
Dense polynomials in coefficient form over Goldilocks.

Layout: p[0] + p[1]*x + p[2]*x^2 + ... (little-endian by degree).
Empty list and [0] both mean the zero polynomial.

Operations are schoolbook; for big circuits use ntt.py for O(n log n) multiplication.
"""

from field import P, add, sub, mul, neg, inv, div, pow_


def trim(p):
    """Strip trailing zero coefficients."""
    i = len(p)
    while i > 0 and p[i - 1] == 0:
        i -= 1
    return p[:i]


def degree(p):
    p = trim(p)
    return len(p) - 1  # -1 for zero poly


def poly_add(a, b):
    n = max(len(a), len(b))
    out = [0] * n
    for i in range(n):
        x = a[i] if i < len(a) else 0
        y = b[i] if i < len(b) else 0
        out[i] = add(x, y)
    return out


def poly_sub(a, b):
    n = max(len(a), len(b))
    out = [0] * n
    for i in range(n):
        x = a[i] if i < len(a) else 0
        y = b[i] if i < len(b) else 0
        out[i] = sub(x, y)
    return out


def poly_mul(a, b):
    if not a or not b:
        return []
    out = [0] * (len(a) + len(b) - 1)
    for i, x in enumerate(a):
        if x == 0:
            continue
        for j, y in enumerate(b):
            out[i + j] = (out[i + j] + x * y) % P
    return out


def poly_scale(p, c):
    return [mul(x, c) for x in p]


def poly_eval(p, x):
    """Horner's method: O(deg p)."""
    r = 0
    for c in reversed(p):
        r = (r * x + c) % P
    return r


def poly_div(num, den):
    """Polynomial long division. Returns (quotient, remainder)."""
    num = trim(list(num))
    den = trim(list(den))
    if not den:
        raise ZeroDivisionError("divide by zero polynomial")
    if len(num) < len(den):
        return [0], num
    q = [0] * (len(num) - len(den) + 1)
    lead_inv = inv(den[-1])
    while len(num) >= len(den):
        coef = mul(num[-1], lead_inv)
        deg = len(num) - len(den)
        q[deg] = coef
        for i, d in enumerate(den):
            num[deg + i] = sub(num[deg + i], mul(coef, d))
        num = trim(num)
    return q, num


def poly_zerofier(roots):
    """Compute Z(x) = product_{r in roots} (x - r)."""
    out = [1]
    for r in roots:
        out = poly_mul(out, [neg(r), 1])
    return out


def lagrange_interpolate(xs, ys):
    """Interpolate the unique poly of degree < n through (xs[i], ys[i])."""
    n = len(xs)
    assert n == len(ys)
    result = [0] * n
    for i in range(n):
        # Build the i-th Lagrange basis: L_i(x) = prod_{j!=i} (x - xs[j]) / (xs[i] - xs[j])
        num = [1]
        den = 1
        for j in range(n):
            if i == j:
                continue
            num = poly_mul(num, [neg(xs[j]), 1])
            den = mul(den, sub(xs[i], xs[j]))
        scale = mul(ys[i], inv(den))
        for k in range(len(num)):
            result[k] = add(result[k], mul(num[k], scale))
    return result


def poly_shift_eval(p, ksi):
    """Return the coefficients of p(ksi * x) given coeffs of p(x)."""
    out = [0] * len(p)
    cur = 1
    for i, c in enumerate(p):
        out[i] = mul(c, cur)
        cur = mul(cur, ksi)
    return out
