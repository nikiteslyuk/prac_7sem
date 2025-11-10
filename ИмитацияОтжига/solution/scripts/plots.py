import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def build_plot(csv_path: Path, output_path: Path) -> None:
    df = pd.read_csv(csv_path)
    if df.empty:
        raise SystemExit("Line CSV is empty")
    df = df.sort_values("workers")
    plt.figure(figsize=(7, 4))
    workers = df["workers"].tolist()
    plt.plot(workers, df["elapsed_seconds"], marker="o")
    plt.xlabel("Number of worker processes")
    plt.ylabel("Average execution time, s")
    plt.title("Execution time vs processes")
    plt.grid(True, alpha=0.3)
    plt.xticks(workers, workers)
    plt.tight_layout()
    plt.savefig(output_path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--line-csv", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    build_plot(Path(args.line_csv), Path(args.output))


if __name__ == "__main__":
    main()
