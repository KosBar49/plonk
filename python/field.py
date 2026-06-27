"""
Field dispatcher.

Selects the active field at import time based on the ZKP_FIELD environment variable.
Defaults to Goldilocks for backward compatibility.

Usage:
    ZKP_FIELD=babybear python3 demo.py
    ZKP_FIELD=koalabear python3 demo.py
    ZKP_FIELD=goldilocks python3 demo.py   # (default)
"""

import os

_field_name = os.environ.get("ZKP_FIELD", "goldilocks").strip().lower()

if _field_name == "goldilocks":
    from fields.goldilocks import *      # noqa: F401, F403
    from fields.goldilocks import (       # explicit for IDEs / static analysis
        P, MULT_GEN, TWO_ADICITY, TWO_ADIC_GEN, K1, K2,
        FIELD_NAME, FIELD_BITS,
        add, sub, mul, neg, pow_, inv, div, primitive_root_of_unity,
    )
elif _field_name == "babybear":
    from fields.babybear import *        # noqa: F401, F403
    from fields.babybear import (
        P, MULT_GEN, TWO_ADICITY, TWO_ADIC_GEN, K1, K2,
        FIELD_NAME, FIELD_BITS,
        add, sub, mul, neg, pow_, inv, div, primitive_root_of_unity,
    )
elif _field_name == "koalabear":
    from fields.koalabear import *       # noqa: F401, F403
    from fields.koalabear import (
        P, MULT_GEN, TWO_ADICITY, TWO_ADIC_GEN, K1, K2,
        FIELD_NAME, FIELD_BITS,
        add, sub, mul, neg, pow_, inv, div, primitive_root_of_unity,
    )
else:
    raise ValueError("Unknown ZKP_FIELD: {!r}. Use goldilocks, babybear, or koalabear.".format(_field_name))
