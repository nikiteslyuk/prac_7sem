#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
DATA_DIR="$ROOT_DIR/generated_inputs"
CSV_PATH="$ROOT_DIR/artifacts/line_results.csv"
PLOT_PATH="$ROOT_DIR/artifacts/performance.png"

if [[ ! -x "$ROOT_DIR/.venv/bin/python3" ]]; then
  echo "Python virtualenv not found; please set up .venv first." >&2
  exit 1
fi

DATASET="dataset_P20_T2000"
DATA_PATH="$DATA_DIR/$DATASET.csv"

if [[ ! -f "$DATA_PATH" ]]; then
  echo "Dataset $DATASET not found. Run task_generator first." >&2
  exit 1
fi

rm -f "$CSV_PATH"
for workers in 1 2 4 6 8 10; do
  echo "Line plot run: $DATASET, workers=$workers"
  "$BUILD_DIR/annealing_solver" \
    --input "$DATA_PATH" \
    --label "$DATASET" \
    --workers "$workers" \
    --runs 1 \
    --seed $((100000 + workers)) \
    --output "$CSV_PATH" \
    >/dev/null
done

"$ROOT_DIR/.venv/bin/python3" "$ROOT_DIR/scripts/plots.py" \
  --line-csv "$CSV_PATH" \
  --output "$PLOT_PATH"

echo "Line plot ready: $PLOT_PATH"
