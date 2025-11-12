#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"
mkdir -p "$BIN_DIR"

"$(command -v g++)" -std=c++20 -O2 "$ROOT_DIR/src/generator_main.cpp" -o "$BIN_DIR/task_generator"
"$BIN_DIR/task_generator" --output-dir "$ROOT_DIR/generated_inputs" --procs 4:4:4 --tasks 555:555:555
