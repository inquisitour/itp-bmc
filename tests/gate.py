import sys
src, dst = sys.argv[1], sys.argv[2]
L = open(src).read().split('\n')
M, I, Lc, O, A = map(int, L[0].split()[1:6])
inputs = L[1:1+I]
latches = L[1+I:1+I+Lc]
outs = L[1+I+Lc:1+I+Lc+O]
ands = L[1+I+Lc+O:1+I+Lc+O+A]
g = 2*(M+1)
gate = 2*(M+2)
out_lit = int(outs[0])
new = ["aag %d %d %d 1 %d" % (M+2, I, Lc+1, A+1)] + inputs + latches
new += ["%d %d" % (g, g)]
new += ["%d" % gate] + ands
new += ["%d %d %d" % (gate, g, out_lit)]
open(dst, 'w').write('\n'.join(new) + '\n')
