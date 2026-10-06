import sys
n, kind, dst = int(sys.argv[1]), sys.argv[2], sys.argv[3]
L = ["aag %d 0 %d 1 1" % (n+1, n)] if kind == "safe" else ["aag %d 0 %d 1 0" % (n, n)]
L.append("2 %d 1" % (2*n))
for i in range(1, n):
    L.append("%d %d" % (2*(i+1), 2*i))
if kind == "safe":
    L.append("%d" % (2*(n+1)))
    L.append("%d 2 4" % (2*(n+1)))
else:
    L.append("%d" % (2*n))
open(dst, "w").write("\n".join(L) + "\n")
