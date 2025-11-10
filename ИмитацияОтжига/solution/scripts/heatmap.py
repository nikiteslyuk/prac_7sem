import argparse
from pathlib import Path

import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt


def build_heatmap(csv_path: Path, output_path: Path) -> None:
    df = pd.read_csv(csv_path)
    if df.empty:
        raise SystemExit("Heatmap CSV is empty; run autotests first.")

    pivot = (
        df.groupby(["processors", "tasks"])
          .agg(elapsed_seconds=("elapsed_seconds", "mean"))
          .reset_index()
          .pivot(index="processors", columns="tasks", values="elapsed_seconds")
    )
    pivot = pivot.sort_index(ascending=False)
    pivot = pivot[pivot.columns.sort_values()]
    plt.figure(figsize=(10, 6))
    sns.heatmap(
        pivot,
        cmap="magma",
        cbar_kws={"label": "Seconds"},
        xticklabels=pivot.columns,
        yticklabels=pivot.index,
    )
    plt.title("Algorithm execution time, s")
    plt.xlabel("Number of tasks")
    plt.ylabel("Number of processors")
    plt.tight_layout()
    plt.savefig(output_path)


def main() -> None:
    parser = argparse.ArgumentParser(description="Build heatmap from solver CSV output.")
    parser.add_argument("--csv", required=True, help="Heatmap CSV path.")
    parser.add_argument("--output", required=True, help="Destination PNG.")
    args = parser.parse_args()
    build_heatmap(Path(args.csv), Path(args.output))


if __name__ == "__main__":
    main()
