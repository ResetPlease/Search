#!/usr/bin/env python3
import sys
from collections import Counter
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 zipf.py <tokens_file> <output_png>")
        sys.exit(1)

    tokens_file = sys.argv[1]
    output_png = sys.argv[2]

    freq = Counter()
    with open(tokens_file, "r") as f:
        for line in f:
            w = line.strip()
            if w:
                freq[w] += 1

    counts = sorted(freq.values(), reverse=True)
    ranks = np.arange(1, len(counts) + 1)
    freqs = np.array(counts, dtype=float)

    C = freqs[0]
    zipf_theoretical = C / ranks

    plt.figure(figsize=(10, 6))
    plt.loglog(ranks, freqs, "b-", linewidth=0.8, label="Observed frequency")
    plt.loglog(ranks, zipf_theoretical, "r--", linewidth=1.0, label="Zipf law (f = C/r)")
    plt.xlabel("Rank (log scale)")
    plt.ylabel("Frequency (log scale)")
    plt.title("Term Frequency Distribution (Zipf's Law)")
    plt.legend()
    plt.grid(True, which="both", linestyle=":", alpha=0.4)

    info = (
        f"Unique terms: {len(counts):,}\n"
        f"Total tokens: {sum(counts):,}\n"
        f"Max freq: {counts[0]:,}\n"
        f"Min freq: {counts[-1]:,}"
    )
    plt.text(0.98, 0.97, info, transform=plt.gca().transAxes,
             fontsize=9, va="top", ha="right",
             bbox=dict(boxstyle="round,pad=0.4", facecolor="lightyellow", alpha=0.8))

    plt.tight_layout()
    plt.savefig(output_png, dpi=150)
    print(f"Zipf plot saved to {output_png}")
    print(f"Unique terms: {len(counts)}, Total tokens: {sum(counts)}")


if __name__ == "__main__":
    main()
