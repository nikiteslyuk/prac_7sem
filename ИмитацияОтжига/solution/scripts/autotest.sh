#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
DATA_DIR="$ROOT_DIR/generated_inputs"
ARTIFACTS_DIR="$ROOT_DIR/artifacts"
LINE_CSV="$ARTIFACTS_DIR/line_results.csv"
HEATMAP_CSV="$ARTIFACTS_DIR/heatmap.csv"
PLOTS_PNG="$ARTIFACTS_DIR/performance.png"
HEATMAP_PNG="$ARTIFACTS_DIR/heatmap.png"
PYTHON_BIN="$ROOT_DIR/.venv/bin/python3"
if [[ ! -x "$PYTHON_BIN" ]]; then
  PYTHON_BIN="$(command -v python3)"
fi

mkdir -p "$BUILD_DIR" "$DATA_DIR" "$ARTIFACTS_DIR"

echo "[1/6] Building generator and solver (g++)"
mkdir -p "$BUILD_DIR"
g++ -O2 -std=c++20 -pthread "$ROOT_DIR/src/sa_classes.cpp" "$ROOT_DIR/src/annealing_main.cpp" -o "$BUILD_DIR/annealing_solver"
g++ -O2 -std=c++20 "$ROOT_DIR/src/generator_main.cpp" -o "$BUILD_DIR/task_generator"

echo "[2/6] Generating datasets"
rm -f "$DATA_DIR"/*.csv 2>/dev/null || true
"$BUILD_DIR/task_generator" --output-dir "$DATA_DIR" >/dev/null

LINE_DATASET="dataset_P20_T2000"
LINE_PATH="$DATA_DIR/$LINE_DATASET.csv"
if [[ ! -f "$LINE_PATH" ]]; then
  echo "Dataset $LINE_DATASET not found" >&2
  exit 1
fi

echo "[3/6] Measuring scaling on $LINE_DATASET"
rm -f "$LINE_CSV"
for workers in 1 2 4 6 8 10; do
  echo "   -> running $LINE_DATASET with $workers processes"
  seed=$((100000 + workers))
  "$BUILD_DIR/annealing_solver" \
    --input "$LINE_PATH" \
    --label "$LINE_DATASET" \
    --workers "$workers" \
    --runs 10 \
    --seed "$seed" \
    --output "$LINE_CSV" \
    >/dev/null
done

echo "[4/6] Building line plot"
"$PYTHON_BIN" "$ROOT_DIR/scripts/plots.py" --line-csv "$LINE_CSV" --output "$PLOTS_PNG"

echo "[5/6] Collecting heatmap data (sequential)"
rm -f "$HEATMAP_CSV"
dataset_counter=0
ordered_datasets=()
while IFS= read -r entry; do
  ordered_datasets+=("$entry")
done < <(
  for dataset in "$DATA_DIR"/dataset_P*_T*.csv; do
    base="$(basename "$dataset" .csv)"
    proc_part="${base#dataset_P}"
    proc="${proc_part%%_T*}"
    tasks="${base##*_T}"
    printf "%03d %05d %s\n" "$proc" "$tasks" "$dataset"
  done | sort
)

for entry in "${ordered_datasets[@]}"; do
  read -r proc tasks dataset <<< "$entry"
  proc_num=$((10#$proc))
  task_num=$((10#$tasks))
  printf "   -> sequential run: %d processors, %d tasks\n" "$proc_num" "$task_num"
  label="$(basename "$dataset" .csv)"
  seed=$((200000 + dataset_counter))
  "$BUILD_DIR/annealing_solver" \
    --input "$dataset" \
    --label "$label" \
    --workers 1 \
    --runs 5 \
    --seed "$seed" \
    --output "$HEATMAP_CSV" \
    >/dev/null
  dataset_counter=$((dataset_counter + 1))
done

echo "[6/6] Building heatmap"
"$PYTHON_BIN" "$ROOT_DIR/scripts/heatmap.py" --csv "$HEATMAP_CSV" --output "$HEATMAP_PNG"

echo "Artifacts:"
echo "  line CSV  -> $LINE_CSV"
echo "  heatmap CSV -> $HEATMAP_CSV"
echo "  line plot -> $PLOTS_PNG"
echo "  heatmap   -> $HEATMAP_PNG"
