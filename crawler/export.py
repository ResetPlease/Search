#!/usr/bin/env python3
import sys
import os
import csv
import yaml
import pymongo


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 export.py <config.yaml> <output_dir>")
        sys.exit(1)

    with open(sys.argv[1]) as f:
        cfg = yaml.safe_load(f)

    out_dir = sys.argv[2]
    os.makedirs(out_dir, exist_ok=True)

    client = pymongo.MongoClient(cfg["db"]["host"], cfg["db"]["port"])
    db = client[cfg["db"]["name"]]

    total_in_db = db.documents.estimated_document_count()
    print(f"Total docs in DB: {total_in_db}")

    meta_path = os.path.join(out_dir, "metadata.csv")
    meta_f = open(meta_path, "w", newline="")
    writer = csv.writer(meta_f)
    writer.writerow(["id", "url", "title", "source", "text_bytes"])

    total_text = 0
    exported = 0

    cursor = db.documents.find(
        {},
        {"url": 1, "title": 1, "text": 1, "source": 1},
        batch_size=500,
    )

    for doc in cursor:
        text = doc.get("text", "")
        if len(text) < 200:
            continue

        title = doc.get("title", "")
        url = doc.get("url", "")
        source = doc.get("source", "")

        file_path = os.path.join(out_dir, f"{exported}.txt")
        with open(file_path, "w", encoding="utf-8") as f:
            f.write(f"{title}\n{url}\n{text}\n")

        text_len = len(text.encode("utf-8"))
        total_text += text_len

        writer.writerow([exported, url, title, source, text_len])
        exported += 1

        if exported % 5000 == 0:
            print(f"Exported {exported} docs...")

    meta_f.close()
    cursor.close()

    stats_path = os.path.join(out_dir, "stats.txt")
    with open(stats_path, "w") as f:
        f.write(f"total_documents={exported}\n")
        f.write(f"total_text_bytes={total_text}\n")
        if exported > 0:
            f.write(f"avg_text_bytes={total_text // exported}\n")

    print(f"Export complete: {exported} docs, {total_text / 1024 / 1024:.1f} MB text")


if __name__ == "__main__":
    main()
