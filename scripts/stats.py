#!/usr/bin/env python3
import sys
import os


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 stats.py <corpus_dir>")
        sys.exit(1)

    corpus_dir = sys.argv[1]
    doc_count = 0
    total_file_bytes = 0
    total_text_bytes = 0
    min_text = float("inf")
    max_text = 0

    for fname in os.listdir(corpus_dir):
        if not fname.endswith(".txt"):
            continue
        if fname in ("stats.txt", "metadata.csv"):
            continue
        fpath = os.path.join(corpus_dir, fname)
        fsize = os.path.getsize(fpath)
        total_file_bytes += fsize

        with open(fpath, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

        if len(lines) < 3:
            continue

        text = "".join(lines[2:])
        tb = len(text.encode("utf-8"))
        total_text_bytes += tb
        if tb < min_text:
            min_text = tb
        if tb > max_text:
            max_text = tb
        doc_count += 1

    if doc_count == 0:
        print("No documents found")
        return

    print(f"Documents: {doc_count}")
    print(f"Total file size: {total_file_bytes / 1024 / 1024:.1f} MB")
    print(f"Total text: {total_text_bytes / 1024 / 1024:.1f} MB")
    print(f"Avg file: {total_file_bytes // doc_count} bytes")
    print(f"Avg text: {total_text_bytes // doc_count} bytes")
    print(f"Min text: {min_text} bytes")
    print(f"Max text: {max_text} bytes")


if __name__ == "__main__":
    main()
