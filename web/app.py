#!/usr/bin/env python3
import os
import json
import subprocess
from flask import Flask, request, render_template

app = Flask(__name__)

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEARCH_CLI = os.path.join(BASE_DIR, "core", "search_cli")
INDEX_PATH = os.path.join(BASE_DIR, "index.bin")
PAGE_SIZE = 50


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/search")
def search():
    query = request.args.get("q", "").strip()
    page = int(request.args.get("page", 1))
    if not query:
        return render_template("index.html")

    big_limit = page * PAGE_SIZE + PAGE_SIZE
    proc = subprocess.run(
        [SEARCH_CLI, INDEX_PATH, "--json", "--limit", str(big_limit)],
        input=query + "\n",
        capture_output=True,
        text=True,
        timeout=10,
    )
    results = []
    total = 0
    time_ms = 0
    if proc.stdout.strip():
        try:
            data = json.loads(proc.stdout.strip())
            results = data.get("results", [])
            total = data.get("total", 0)
            time_ms = data.get("time_ms", 0)
        except json.JSONDecodeError:
            pass

    start = (page - 1) * PAGE_SIZE
    page_results = results[start:start + PAGE_SIZE]
    has_next = total > page * PAGE_SIZE

    return render_template(
        "results.html",
        query=query,
        results=page_results,
        total=total,
        time_ms=time_ms,
        page=page,
        has_next=has_next,
    )


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8090, debug=False)
