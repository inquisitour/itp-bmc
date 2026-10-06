#!/bin/bash
BMC=~/cav-project-sby/bmc
FUZZ=~/cav-project-sby/aiger-1.9.9/aigfuzz
A2B=~/cav-project-sby/aiger-1.9.9/aigtoaig
mkdir -p /tmp/fuzz /tmp/adiag
START=${1:-1}; END=${2:-50}
n=0; skipped=0; flagged=""
declare -A combo
for s in $(seq $START $END); do
  f=/tmp/fuzz/c$s
  $FUZZ -a -1 -S -m -s -o $f.aag $s 2>/dev/null || continue
  $A2B $f.aag $f.aig 2>/dev/null || continue
  ours=$(BMC_WORKDIR=/tmp/adiag timeout 60 $BMC 25 $f.aag 2>&1 | grep -E "Fixpoint reached|Counterexample found|Safe up to bound|ERROR" | tail -1)
  pdr=$(timeout 60 yosys-abc -c "read_aiger $f.aig; pdr" 2>&1 | grep -E "Property proved|was asserted in frame" | tail -1)
  o=T; p=T; ob=; ok=; pf=
  case "$ours" in
    *Fixpoint*)       o=F ;;
    *Counterexample*) o=C; ok=$(echo "$ours" | grep -oE '[0-9]+$') ;;
    *"Safe up to"*)   o=B; ob=$(echo "$ours" | grep -oE 'bound [0-9]+' | grep -oE '[0-9]+') ;;
    *ERROR*)          o=E ;;
  esac
  case "$pdr" in
    *proved*)   p=P ;;
    *asserted*) p=A; pf=$(echo "$pdr" | grep -oE 'frame [0-9]+' | grep -oE '[0-9]+') ;;
  esac
  combo[$o$p]=$(( ${combo[$o$p]:-0} + 1 ))
  if [ $o = T ] || [ $p = T ]; then skipped=$((skipped+1)); continue; fi
  n=$((n+1))
  bug=
  [ $o = F ] && [ $p = A ] && bug="WRONG-PROOF"
  [ $o = C ] && [ $p = P ] && bug="WRONG-CEX"
  [ $o = B ] && [ $p = A ] && [ "$pf" -le "$ob" ] && bug="MISSED-CEX(pdr frame $pf, ours safe to $ob)"
  [ $o = C ] && [ $p = A ] && [ "$ok" != "$pf" ] && bug="DEPTH-MISMATCH(ours $ok, pdr $pf)"
  [ $o = E ] && bug="ENGINE-ERROR"
  if [ -n "$bug" ]; then echo "seed $s: $bug"; flagged="$flagged $s"; fi
done
echo "compared=$n  skipped(timeout/none)=$skipped"
for k in "${!combo[@]}"; do echo "  $k: ${combo[$k]}"; done
echo "flagged seeds:${flagged:- none}"
