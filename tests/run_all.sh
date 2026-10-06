#!/bin/bash
# Full regression for itp-bmc. Every verdict is compared with ABC (pdr / bmc3).
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BMC="$ROOT/bmc"; A2B="$ROOT/aiger-1.9.9/aigtoaig"; FUZZ="$ROOT/aiger-1.9.9/aigfuzz"
W=/tmp/itp_suite
export BMC_WORKDIR=/tmp/adiag
mkdir -p $W/demo $W/fuzz $W/gated $W/ring /tmp/adiag
[ -d /tmp/adiag/minisatp ] || cp -r "$ROOT/minisatp" /tmp/adiag/
[ -x "$BMC" ] && [ -x "$A2B" ] && [ -x "$FUZZ" ] && command -v yosys-abc >/dev/null || { echo "missing bmc / aigtoaig / aigfuzz / yosys-abc"; exit 2; }
PASS=0; FAIL=0; WARN=0
declare -A tally

sec_start() { SEC="$1"; S_P=$PASS; S_F=$FAIL; S_W=$WARN; }
sec_end()   { echo "[$SEC] ok=$((PASS-S_P)) warn=$((WARN-S_W)) FAILED=$((FAIL-S_F))"; }

check() {   # name aag aig
  local name=$1 aag=$2 aig=$3 o p ours pdr od= pd=
  ours=$(timeout 120 "$BMC" 30 "$aag" 2>&1 | grep -E "Fixpoint reached|Counterexample found|Safe up to bound|ERROR" | tail -1)
  pdr=$(timeout 60 yosys-abc -c "read_aiger $aig; pdr" 2>&1 | grep -E "Property proved|was asserted" | tail -1)
  case "$ours" in
    *Fixpoint*)       o=F;;
    *Counterexample*) o=C; od=$(echo "$ours" | grep -oE '[0-9]+$');;
    *"Safe up to"*)   o=B; od=$(echo "$ours" | grep -oE 'bound [0-9]+' | grep -oE '[0-9]+' | head -1);;
    *ERROR*)          o=E;;
    *)                o=T;;
  esac
  case "$pdr" in
    *proved*)   p=P;;
    *asserted*) p=A;;
    *)          p=T;;
  esac
  if [ $p = A ]; then
    pd=$(timeout 60 yosys-abc -c "read_aiger $aig; bmc3 -F 80" 2>&1 | grep asserted | grep -oE "frame [0-9]+" | tail -1 | grep -oE '[0-9]+')
    [ -z "$pd" ] && pd=$(echo "$pdr" | grep -oE "frame [0-9]+" | grep -oE '[0-9]+')
    [ -z "$pd" ] && pd=-1
  fi
  local verdict=ok
  if   [ $o = E ]; then verdict="ENGINE ERROR"
  elif [ $o = T ] || [ $p = T ] || { [ $p = A ] && [ "$pd" = -1 ]; }; then verdict=timeout
  elif [ $o = F ] && [ $p = A ]; then verdict="WRONG PROOF (we say fixpoint, ABC finds counterexample at depth $pd)"
  elif [ $o = C ] && [ $p = P ]; then verdict="WRONG COUNTEREXAMPLE (ABC proves safe)"
  elif [ $o = C ] && [ $p = A ] && [ "$od" != "$pd" ]; then verdict="DEPTH MISMATCH ours=$od abc=$pd"
  elif [ $o = B ] && [ $p = A ] && [ "$pd" -le "$od" ]; then verdict="MISSED COUNTEREXAMPLE (ABC depth $pd, ours safe to $od)"
  fi
  tally[$o$p]=$(( ${tally[$o$p]:-0} + 1 ))
  case "$verdict" in
    ok)      PASS=$((PASS+1));;
    timeout) WARN=$((WARN+1)); echo "WARN $name: timeout";;
    *)       FAIL=$((FAIL+1)); echo "FAIL $name: $verdict";;
  esac
}

sec_start "toy interpolation tests (hand-computed answers)"
cat > /tmp/test_interp.cnf <<'EOF'
p cnf 4 6
-1 2 0
-1 3 0
-2 0
2 3 0
2 4 0
-4 0
EOF
cat > /tmp/test_interp2.cnf <<'EOF'
p cnf 4 5
-1 2 0
1 3 0
-2 4 0
-3 4 0
-4 0
EOF
rm -f /tmp/adiag/proof.txt
out=$("$BMC" --test-interp 2>/dev/null)
if echo "$out" | grep -q "Interpolant clauses: 1" && echo "$out" | grep -q "Clause: -2"; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); echo "FAIL toy-1: $out"; fi
rm -f /tmp/adiag/proof.txt
out=$("$BMC" --test-interp2 2>/dev/null)
if echo "$out" | grep -q "Interpolant clauses: 1" && echo "$out" | grep -q "Clause: 2 3"; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); echo "FAIL toy-2: $out"; fi
sec_end

sec_start "demo circuits"
for f in "$ROOT"/demo/*.aag; do
  b=$(basename "${f%.aag}")
  "$A2B" "$f" "$W/demo/$b.aig" 2>/dev/null || continue
  check "demo-$b" "$f" "$W/demo/$b.aig"
done
sec_end

sec_start "random circuits, seeds 1-300 (mostly unsafe)"
for s in $(seq 1 300); do
  f=$W/fuzz/c$s
  "$FUZZ" -a -1 -S -m -s -o $f.aag $s 2>/dev/null || continue
  "$A2B" $f.aag $f.aig 2>/dev/null || continue
  check "random-$s" $f.aag $f.aig
done
sec_end

sec_start "gated circuits, safe by construction"
for s in $(seq 1 200); do
  [ -f $W/fuzz/c$s.aag ] || continue
  python3 "$ROOT/tests/gate.py" $W/fuzz/c$s.aag $W/gated/c$s.aag 2>/dev/null || continue
  "$A2B" $W/gated/c$s.aag $W/gated/c$s.aig 2>/dev/null || continue
  check "gated-$s" $W/gated/c$s.aag $W/gated/c$s.aig
done
sec_end

sec_start "ring families (real invariant, reset=1 latch)"
for kind in safe unsafe; do
  for n in 3 4 5 6 8 10 12; do
    f=$W/ring/${kind}_$n
    python3 "$ROOT/tests/ring.py" $n $kind $f.aag
    "$A2B" $f.aag $f.aig 2>/dev/null || continue
    check "ring-$kind-$n" $f.aag $f.aig
  done
done
sec_end

echo
echo "Verdict pairs (ours,ABC): F=fixpoint C=counterexample B=bounded | P=proved A=counterexample"
for k in "${!tally[@]}"; do echo "  $k: ${tally[$k]}"; done | sort
echo "TOTAL ok=$PASS warn=$WARN FAILED=$FAIL"
if [ $FAIL -eq 0 ]; then echo "ALL GOOD"; else echo "FAILURES FOUND"; fi
[ $FAIL -eq 0 ]
