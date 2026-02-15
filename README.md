# Interpolation-based Model Checker

A bounded model checker with interpolation-based verification for AIGER circuits.

## Build
```bash
make
```

## Usage
```bash
./bmc <bound> <file.aag>
```

**Arguments:**
- `bound` - maximum unrolling depth
- `file.aag` - AIGER ASCII format circuit

**Output:**
- `OK` - property holds (safe)
- `FAIL` - counterexample found

## Example
```bash
./bmc 10 circuit.aag
```

## Dependencies

- [MiniSAT 1.14](https://github.com/emina/minisatp) with proof logging
- [AIGER tools](http://fmv.jku.at/aiger/) for format conversion

## Project Structure
```
src/
├── aiger_parser.cpp/.h    # AIGER format parser
├── cnf_generator.cpp/.h   # BMC unrolling & Tseitin encoding
├── proof_parser.cpp/.h    # Resolution proof parser
├── interpolant.cpp/.h     # Craig interpolation
├── model_checker.cpp/.h   # Main verification loop
└── main.cpp
```