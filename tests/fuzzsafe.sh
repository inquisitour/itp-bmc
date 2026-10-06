#!/bin/bash
BMC=~/cav-project-sby/bmc
declare -A c
bad=""
for s in $(seq 1 200); do
  f=/tmp/fuzzsafe/c$s
  [ -f $f.aag ] || continue
  o=$(BMC_WORKDIR=/tmp/adiag timeout 120 $BMC 25 $f.aag 2>&1 | grep -E "Fixpoint reached|Counterexample found|Safe up to bound|ERROR" | tail -1)
  p=$(timeout 60 yosys-abc -c "read_aiger $f.aig; pdr" 2>&1 | grep -E "Property proved|was asserted" | tail -1)
  case "$o" in *Fixpoint*) a=F;; *Counterexample*) a=C;; *"Safe up to"*) a=B;; *ERROR*) a=E;; *) a=T;; esac
  case "$p" in *proved*) b=P;; *asserted*) b=A;; *) b=T;; esac
  c[$a$b]=$(( ${c[$a$b]:-0} + 1 ))
  if [ $a = C ] || [ $a = E ] || [ $b = A ]; then echo "seed $s: ours=$a pdr=$b"; bad="$bad $s"; fi
done
for k in "${!c[@]}"; do echo "  $k: ${c[$k]}"; done
echo "flagged:${bad:- none}"
