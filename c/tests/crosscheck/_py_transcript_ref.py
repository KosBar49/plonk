import sys, os
field = sys.argv[1] if len(sys.argv) > 1 else "babybear"
os.environ["ZKP_FIELD"] = field
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "python"))
import field as F
from transcript import Transcript

t = Transcript(b"plonk-fri")
root = bytes((i * 7 + 1) & 0xff for i in range(32))
t.absorb("commit_a", root)
beta = t.challenge_field("beta")
gamma = t.challenge_field("gamma")
t.absorb("openings", [11, 22, 33])
zeta = t.challenge_field("zeta")
q = t.challenge_int(64, "fri_query")

print("field=%s" % F.FIELD_NAME)
print("beta=%d" % beta)
print("gamma=%d" % gamma)
print("zeta=%d" % zeta)
print("q=%d" % q)
