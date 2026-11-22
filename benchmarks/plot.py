"""Plot completed benchmark batches; partial/skipped rows are never extrapolated."""

import argparse
import csv
from collections import defaultdict
from pathlib import Path
from statistics import median

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="?", default="results/benchmark.csv")
    parser.add_argument("--output", default="results/benchmark.png")
    args = parser.parse_args()
    groups = defaultdict(list)
    skipped = 0
    with open(args.csv, newline="", encoding="utf-8") as source:
        for row in csv.DictReader(source):
            if row["status"] != "ok" or int(row["operations"]) != int(row["n"]):
                skipped += 1
                continue
            groups[(row["operation"], row["order"], int(row["n"]))].append(float(row["seconds"]))
    if not groups:
        parser.error("No completed measurements in CSV")

    fig, axes = plt.subplots(1, 3, figsize=(14, 4.8), layout="constrained")
    for ax, operation in zip(axes, ("insert", "find", "erase")):
        for order, color in (("random", "#2864a0"), ("sorted", "#c14c32")):
            sizes = sorted(n for op, seq, n in groups if op == operation and seq == order)
            if not sizes:
                continue
            times = [groups[(operation, order, n)] for n in sizes]
            middle = [median(values) for values in times]
            ax.errorbar(sizes, middle,
                        yerr=([m - min(v) for m, v in zip(middle, times)],
                              [max(v) - m for m, v in zip(middle, times)]),
                        marker="o", capsize=4, label=order, color=color)
        ax.set(xscale="log", yscale="log", xlabel="Elements", ylabel="Batch time, seconds", title=operation)
        ax.grid(True, which="both", alpha=0.2)
        ax.legend()
    fig.suptitle(f"Unbalanced BST: median and min–max across repeats\nExcluded partial/skipped rows: {skipped}")
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=180)
    plt.close(fig)
    print(output)


if __name__ == "__main__":
    main()
