import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def build_accuracy_plot(csv_path: Path, output_path: Path) -> None:
    df = pd.read_csv(csv_path)
    if df.empty:
        raise SystemExit("Accuracy CSV is empty")
    df = df.sort_values("workers")
    workers = df["workers"].tolist()
    scores = df["best_score"]
    plt.figure(figsize=(7, 4))
    plt.plot(workers, scores, marker="o")
    plt.xlabel("Number of worker processes")
    plt.ylabel("Best score (lower is better)")
    plt.title("Solution quality vs processes")
    plt.grid(True, alpha=0.3)
    plt.xticks(workers, workers)
    plt.tight_layout()
    plt.savefig(output_path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    build_accuracy_plot(Path(args.csv), Path(args.output))


if __name__ == "__main__":
    main()
