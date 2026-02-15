# Interpolation-based Model Checker

## Build
```
make
```

## Usage
```
./bmc <bound> <file.aag>
```

## Output
- `OK` - model is safe up to bound
- `FAIL` - counterexample found

## Dependencies
- MiniSAT 1.14 with proof logging (included in minisatp/)
- AIGER tools (included in aiger-1.9.9/)
