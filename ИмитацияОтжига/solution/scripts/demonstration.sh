#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
mkdir -p "$BIN_DIR"

"$(command -v g++)" -std=c++20 -O2 "$ROOT_DIR/src/sa_classes.cpp" "$ROOT_DIR/src/annealing_main.cpp" -lpthread -o "$BIN_DIR/annealing_solver"

DATASET="$ROOT_DIR/generated_inputs/dataset_P12_T1000.csv"
if [[ ! -f "$DATASET" ]]; then
  "$(command -v g++)" -std=c++20 -O2 "$ROOT_DIR/src/generator_main.cpp" -o "$BIN_DIR/task_generator"
  "$BIN_DIR/task_generator" --output-dir "$ROOT_DIR/generated_inputs" >/dev/null
fi

"$BIN_DIR/annealing_solver" --input "$DATASET" --label demo --workers 4  --runs 10
