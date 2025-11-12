#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
DATA_DIR="$ROOT_DIR/generated_inputs"
CSV_PATH="$ROOT_DIR/artifacts/line_results.csv"
PLOT_TIME="$ROOT_DIR/artifacts/performance.png"
PLOT_ACC="$ROOT_DIR/artifacts/accuracy.png"

mkdir -p "$BUILD_DIR"
CXX="$(command -v g++)"
"$CXX" -std=c++20 -O2 "$ROOT_DIR/src/sa_classes.cpp" "$ROOT_DIR/src/annealing_main.cpp" -o "$BUILD_DIR/annealing_solver"

DATASET="dataset_P20_T2000"
DATA_PATH="$DATA_DIR/$DATASET.csv"
if [[ ! -f "$DATA_PATH" ]]; then
  "$CXX" -std=c++20 -O2 "$ROOT_DIR/src/generator_main.cpp" -o "$BUILD_DIR/task_generator"
  "$BUILD_DIR/task_generator" --output-dir "$DATA_DIR" >/dev/null
fi

rm -f "$CSV_PATH"
for workers in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
  echo "[run_line_plots] processes=$workers"
  "$BUILD_DIR/annealing_solver" \
    --input "$DATA_PATH" \
    --label "$DATASET" \
    --workers "$workers" \
    --runs 10 \
    --seed $((400000 + workers)) \
    --output "$CSV_PATH" \
    >/dev/null 2>&1
done

"$ROOT_DIR/.venv/bin/python3" "$ROOT_DIR/scripts/plots.py" --line-csv "$CSV_PATH" --output "$PLOT_TIME"
"$ROOT_DIR/.venv/bin/python3" "$ROOT_DIR/scripts/accuracy_plot.py" --csv "$CSV_PATH" --output "$PLOT_ACC"
echo "Performance plot -> $PLOT_TIME"
echo "Accuracy plot    -> $PLOT_ACC"
