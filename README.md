# itp-bmc: Interpolation-Based Model Checker

A safety model checker for AIGER circuits using Craig interpolation and fixpoint detection. Designed as a backend engine for [SymbiYosys](https://github.com/YosysHQ/sby).

## Build
```bash
make
```

Requires: g++ with C++17 support, gcc.

## Usage
```bash
./bmc <bound> <file.aig|file.aag> [skip]
```

**Arguments:**
- `bound` — maximum unrolling depth
- `file.aig` — AIGER circuit (binary or ASCII format)
- `skip` — (optional) number of initial timeframes to skip before checking bad states (default: 0)

**Environment:**
- `BMC_WORKDIR` — directory containing `minisatp/minisat` and temp files (default: directory of binary)

**Output:**
- `OK` — property holds (safe), either by fixpoint or bounded check
- `FAIL` — counterexample found

## Example
```bash
# Standalone
BMC_WORKDIR=. ./bmc 20 circuit.aig

# With skip (e.g. riscv-formal checks require reset cycles)
BMC_WORKDIR=. ./bmc 15 circuit.aig 10
```

## SymbiYosys Integration

Install as `itp-bmc` in PATH:
```bash
sudo cp bmc /usr/local/bin/itp-bmc
```

Then in your `.sby` file:
```
[engines]
itp <bound> <skip>
```

Requires [sby_engine_itp.py](https://github.com/YosysHQ/sby) to be installed in SymbiYosys.

## How It Works

1. Unrolls the AIGER transition relation up to bound `k`
2. Encodes as CNF using Tseitin transformation
3. Calls MiniSAT with proof logging
4. Extracts Craig interpolant from resolution proof
5. Checks fixpoint: if interpolant stabilizes, property is proved safe
6. Iterates until fixpoint or counterexample found

## Tested On

- [riscv-formal](https://github.com/YosysHQ/riscv-formal) NERV core: `insn_add`, `insn_sub`, `insn_and`, `insn_or`, `insn_xor`, `insn_lui` (all PASS)

## Dependencies

- [MiniSAT 1.14p](https://github.com/emina/minisatp) with proof logging (included in `minisatp/`)
- [AIGER library](http://fmv.jku.at/aiger/) (included in `aiger-1.9.9/`)

## Project Structure
```
src/
├── aiger_parser.cpp/.h    # AIGER parser (binary + ASCII, bad properties)
├── cnf_generator.cpp/.h   # BMC unrolling & Tseitin encoding
├── proof_parser.cpp/.h    # Resolution proof parser
├── interpolant.cpp/.h     # Craig interpolation from resolution proofs
├── model_checker.cpp/.h   # Main verification loop with fixpoint detection
└── main.cpp
aiger-1.9.9/               # AIGER library (aiger.c/h)
minisatp/                  # MiniSAT with proof logging
```

## Limitations

- No counterexample witness output (planned)
- ASCII AIGER output not supported (binary `.aig` input only via aiger library)
- Justice/liveness properties ignored (safety checking only)