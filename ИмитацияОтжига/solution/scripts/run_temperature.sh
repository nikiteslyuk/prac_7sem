#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
DATA_DIR="$ROOT_DIR/generated_inputs"
CSV_PATH="$ROOT_DIR/artifacts/heatmap.csv"
PYTHON_BIN="$ROOT_DIR/.venv/bin/python3"

mkdir -p "$BUILD_DIR"
CXX="$(command -v g++)"
"$CXX" -std=c++20 -O2 "$ROOT_DIR/src/sa_classes.cpp" "$ROOT_DIR/src/annealing_main.cpp" -o "$BUILD_DIR/annealing_solver"
"$CXX" -std=c++20 -O2 "$ROOT_DIR/src/generator_main.cpp" -o "$BUILD_DIR/task_generator"

rm -f "$DATA_DIR"/dataset_P*.csv
"$BUILD_DIR/task_generator" --output-dir "$DATA_DIR" >/dev/null

rm -f "$CSV_PATH"
counter=0
for dataset in "$DATA_DIR"/dataset_P*_T*.csv; do
  echo "Running dataset: $dataset"
  base="$(basename "$dataset" .csv)"
  seed=$((300000 + counter))
 "$BUILD_DIR/annealing_solver" \
    --input "$dataset" \
    --label "$base" \
    --workers 1 \
    --runs 5 \
    --seed "$seed" \
    --output "$CSV_PATH" \
    >/dev/null
  counter=$((counter + 1))
done

"$PYTHON_BIN" "$ROOT_DIR/scripts/heatmap.py" --csv "$CSV_PATH" --output "$ROOT_DIR/artifacts/heatmap.png"
echo "Heatmap ready: $ROOT_DIR/artifacts/heatmap.png"
