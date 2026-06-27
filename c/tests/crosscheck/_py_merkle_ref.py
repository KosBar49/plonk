import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "python"))
from merkle import MerkleTree
leaves = [100 + i for i in range(8)]
t = MerkleTree(leaves)
print("python root =", t.root.hex())
