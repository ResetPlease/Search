#!/usr/bin/env python3
import sys
import time
import hashlib
import asyncio
import logging
import random
import xml.etree.ElementTree as ET

import yaml
import aiohttp
import pymongo
from bs4 import BeautifulSoup

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
log = logging.getLogger("crawl")

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                  "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36"
}


def extract_text(html, url):
    soup = BeautifulSoup(html, "lxml")
    title_tag = soup.find("title")
    title = title_tag.get_text(strip=True) if title_tag else ""
    for tag in soup(["script", "style", "nav", "header", "footer",
                     "aside", "figure", "figcaption", "noscript", "iframe"]):
        tag.decompose()
    article = soup.find("article")
    if article:
        paragraphs = article.find_all("p")
    else:
        main = soup.find("main")
        if main:
            paragraphs = main.find_all("p")
        else:
            paragraphs = soup.find_all("p")
    text = "\n".join(
        p.get_text(strip=True) for p in paragraphs
        if len(p.get_text(strip=True)) > 30
    )
    return title, text


async def fetch_bytes(session, url):
    try:
        async with session.get(url, timeout=aiohttp.ClientTimeout(total=15)) as resp:
            if resp.status == 200:
                return await resp.read()
    except Exception:
        pass
    return None


async def fetch_text(session, url):
    try:
        async with session.get(url, timeout=aiohttp.ClientTimeout(total=15)) as resp:
            if resp.status == 200:
                return await resp.text(errors="replace")
    except Exception:
        pass
    return None


async def collect_sitemap_urls(session, sitemap_url, domain, max_urls):
    urls = []
    ns = {"s": "http://www.sitemaps.org/schemas/sitemap/0.9"}
    log.info("Fetching sitemap: %s", sitemap_url)
    data = await fetch_bytes(session, sitemap_url)
    if not data:
        log.warning("Failed to fetch sitemap %s", sitemap_url)
        return urls

    try:
        root = ET.fromstring(data)
    except ET.ParseError:
        log.warning("XML parse error for %s", sitemap_url)
        return urls

    for loc in root.findall("s:url/s:loc", ns):
        u = loc.text.strip()
        if domain in u:
            urls.append(u)

    sub_sitemaps = [s.text.strip() for s in root.findall("s:sitemap/s:loc", ns)]
    log.info("Found %d direct URLs, %d sub-sitemaps for %s", len(urls), len(sub_sitemaps), domain)

    sem = asyncio.Semaphore(15)
    async def fetch_sub(sm_url):
        async with sem:
            return await fetch_bytes(session, sm_url)

    tasks = [fetch_sub(sm) for sm in sub_sitemaps[:80]]
    results = await asyncio.gather(*tasks)
    for sm_data in results:
        if sm_data:
            try:
                sm_root = ET.fromstring(sm_data)
                for loc in sm_root.findall("s:url/s:loc", ns):
                    u = loc.text.strip()
                    if domain in u:
                        urls.append(u)
            except ET.ParseError:
                pass
        if len(urls) >= max_urls * 3:
            break

    log.info("Total sitemap URLs for %s: %d", domain, len(urls))
    return urls


async def crawl_one(session, url, db, source_name, sem, stats):
    async with sem:
        html = await fetch_text(session, url)
        if not html:
            stats["err"] += 1
            return
        title, text = extract_text(html, url)
        if len(text) < 200:
            stats["err"] += 1
            return
        doc = {
            "url": url,
            "raw_html": html,
            "title": title,
            "text": text,
            "source": source_name,
            "crawl_timestamp": int(time.time()),
            "content_hash": hashlib.md5(html.encode("utf-8", errors="ignore")).hexdigest(),
        }
        try:
            db.documents.update_one({"url": url}, {"$set": doc}, upsert=True)
            stats["ok"] += 1
        except pymongo.errors.DuplicateKeyError:
            pass
        if stats["ok"] % 500 == 0 and stats["ok"] > 0:
            log.info("[%s] crawled=%d errors=%d", source_name, stats["ok"], stats["err"])


async def crawl_source(session, db, source_cfg, concurrency):
    name = source_cfg["name"]
    sitemap = source_cfg["sitemap"]
    domain = source_cfg["domain"]
    target = source_cfg.get("target", 15000)

    existing = set()
    for doc in db.documents.find({"source": name}, {"url": 1}):
        existing.add(doc["url"])
    log.info("[%s] Existing docs: %d", name, len(existing))

    all_urls = await collect_sitemap_urls(session, sitemap, domain, target)
    skip_ext = (".jpg", ".png", ".xml", ".gz", ".pdf", ".mp4", ".webp")
    new_urls = [
        u for u in all_urls
        if u not in existing and not u.endswith(skip_ext)
    ]
    random.shuffle(new_urls)
    new_urls = new_urls[:target]
    log.info("[%s] Will crawl %d new URLs", name, len(new_urls))

    sem = asyncio.Semaphore(concurrency)
    stats = {"ok": 0, "err": 0}
    batch_size = 800
    for i in range(0, len(new_urls), batch_size):
        batch = new_urls[i:i+batch_size]
        tasks = [crawl_one(session, u, db, name, sem, stats) for u in batch]
        await asyncio.gather(*tasks)
        log.info("[%s] Batch %d-%d: ok=%d err=%d", name, i, i+len(batch), stats["ok"], stats["err"])
        if stats["ok"] >= target:
            break

    log.info("[%s] DONE: %d new docs crawled", name, stats["ok"])
    return stats["ok"]


async def main():
    if len(sys.argv) < 2:
        print("Usage: python3 crawl.py <config.yaml> [source_name]")
        sys.exit(1)

    with open(sys.argv[1]) as f:
        cfg = yaml.safe_load(f)

    filter_source = sys.argv[2] if len(sys.argv) > 2 else None

    client = pymongo.MongoClient(cfg["db"]["host"], cfg["db"]["port"])
    db = client[cfg["db"]["name"]]
    db.documents.create_index("url", unique=True)

    concurrency = cfg["logic"].get("concurrency", 80)
    conn = aiohttp.TCPConnector(limit=concurrency, limit_per_host=concurrency, ttl_dns_cache=300)

    async with aiohttp.ClientSession(headers=HEADERS, connector=conn) as session:
        for src in cfg["sources"]:
            if filter_source and src["name"] != filter_source:
                continue
            await crawl_source(session, db, src, concurrency)

    total = db.documents.estimated_document_count()
    log.info("=== TOTAL DB DOCUMENTS: %d ===", total)


if __name__ == "__main__":
    asyncio.run(main())
