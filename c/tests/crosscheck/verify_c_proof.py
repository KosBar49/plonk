"""
Load a binary proof (produced by the C prover, c_proof.bin) and verify it with
the Python verifier. This is the cross-check: C prover -> Python verifier.

Usage:  python verify_c_proof.py <field> [path]
"""
import sys, os, struct

field = sys.argv[1] if len(sys.argv) > 1 else "babybear"
path = sys.argv[2] if len(sys.argv) > 2 else "c_proof.bin"
os.environ["ZKP_FIELD"] = field
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "python"))

import field as F
from demo import build_circuit
from plonk import setup, verify
from fri import NUM_QUERIES, LOG_BLOWUP

ELEM = 8 if F.FIELD_NAME == "goldilocks" else 4

def main():
    data = open(path, "rb").read()
    off = 0
    assert data[:4] == b"PLNK", "bad magic"
    off = 4
    version, field_id = struct.unpack_from("<HH", data, off); off += 4
    log_n, log_blowup = data[off], data[off+1]; off += 2
    num_queries = struct.unpack_from("<H", data, off)[0]; off += 2
    off += 4  # reserved
    log_N = log_n + log_blowup

    def read_hash():
        nonlocal off
        h = data[off:off+32]; off += 32; return h
    def read_elem():
        nonlocal off
        v = int.from_bytes(data[off:off+ELEM], "little"); off += ELEM; return v

    commits = {k: read_hash() for k in ("a","b","c","Z","t_lo","t_mid","t_hi")}
    openings = {}
    for k in ("a","b","c","Z","Zw","t_lo","t_mid","t_hi"):
        openings[k] = read_elem()

    q_layer_roots = [read_hash() for _ in range(log_N - 1)]
    fri_final = read_elem()

    queries = []
    for _ in range(num_queries):
        q0 = struct.unpack_from("<I", data, off)[0]; off += 4
        f_v = [(0,0)] * 7
        vlo = [0]*7; vhi=[0]*7
        for k in range(7):
            vlo[k] = read_elem(); vhi[k] = read_elem()
        f_openings = []
        for k in range(7):
            plo = [read_hash() for _ in range(log_N)]
            phi = [read_hash() for _ in range(log_N)]
            f_openings.append({"v_lo": vlo[k], "v_hi": vhi[k],
                               "path_lo": plo, "path_hi": phi})
        Q_openings = []
        for li in range(1, log_N):
            qvlo = read_elem(); qvhi = read_elem()
            path_len = log_N - li
            plo = [read_hash() for _ in range(path_len)]
            phi = [read_hash() for _ in range(path_len)]
            Q_openings.append({"v_lo": qvlo, "v_hi": qvhi,
                               "path_lo": plo, "path_hi": phi})
        queries.append({"q0": q0, "f_openings": f_openings, "Q_openings": Q_openings})

    proof = {
        "commits": commits,
        "openings": openings,
        "fri": {"Q_layer_roots": q_layer_roots, "final_value": fri_final,
                "queries": queries},
    }

    assert off == len(data), "trailing bytes: parsed %d of %d" % (off, len(data))

    pp = setup(build_circuit())
    ok = verify(pp, proof)
    print("field=%s  C-proof -> Python-verify: %s" % (F.FIELD_NAME, "ACCEPT" if ok else "REJECT"))
    sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
