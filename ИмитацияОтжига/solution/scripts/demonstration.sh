#!/usr/bin/env bash
set -euo pipefail

cmake -S .. -B ../build
cmake --build ../build --target annealing_solver
../build/annealing_solver --input ../generated_inputs/dataset_P12_T1000.csv --label demo --workers 4 --runs 1
