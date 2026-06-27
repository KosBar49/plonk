import sys, os
field = sys.argv[1] if len(sys.argv) > 1 else "babybear"
os.environ["ZKP_FIELD"] = field
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "python"))
import field as F
from demo import build_circuit
from plonk import setup

pp = setup(build_circuit())
def dump(name, p):
    print("%s=%s" % (name, ",".join(str(x) for x in p)))
print("field=%s n=%d omega=%d" % (F.FIELD_NAME, pp["n"], pp["omega"]))
dump("qL", pp["selectors"]["q_L"])
dump("qC", pp["selectors"]["q_C"])
dump("S1", pp["S1"])
dump("S2", pp["S2"])
dump("S3", pp["S3"])
