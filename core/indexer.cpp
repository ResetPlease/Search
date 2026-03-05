#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include "mystr.h"
#include "vec.h"
#include "map.h"
#include "porter.h"

struct DocInfo {
    char *title;
    char *url;
    char *source;
};

struct DocArr {
    DocInfo *items;
    int len;
    int cap;
};

static void docarr_init(DocArr *a) { a->items = 0; a->len = 0; a->cap = 0; }

static void docarr_push(DocArr *a, const char *title, const char *url, const char *src) {
    if (a->len >= a->cap) {
        int nc = a->cap == 0 ? 256 : a->cap * 2;
        DocInfo *nd = new DocInfo[nc];
        for (int i = 0; i < a->len; i++) nd[i] = a->items[i];
        delete[] a->items;
        a->items = nd;
        a->cap = nc;
    }
    a->items[a->len].title = str_dup(title);
    a->items[a->len].url = str_dup(url);
    a->items[a->len].source = str_dup(src);
    a->len++;
}

static void write_u16(FILE *f, unsigned short v) { fwrite(&v, 2, 1, f); }
static void write_u32(FILE *f, unsigned int v) { fwrite(&v, 4, 1, f); }
static void write_u64(FILE *f, unsigned long long v) { fwrite(&v, 8, 1, f); }
static void write_u8(FILE *f, unsigned char v) { fwrite(&v, 1, 1, f); }

static char *read_line(FILE *f) {
    char buf[65536];
    if (!fgets(buf, sizeof(buf), f)) return 0;
    int n = str_len(buf);
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return str_dup(buf);
}

static void tokenize_and_index(const char *text, Map *idx, int doc_id) {
    char token[256];
    int tlen = 0;
    const char *p = text;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        int digit = (c >= '0' && c <= '9');
        int apos = (c == '\'' && tlen > 0);
        if (letter || digit || apos) {
            if (tlen < 254) {
                token[tlen++] = to_lower(c);
            }
        } else {
            if (tlen >= 2) {
                while (tlen > 0 && token[tlen-1] == '\'') tlen--;
                if (tlen >= 2) {
                    token[tlen] = '\0';
                    int only_digits = 1;
                    for (int i = 0; i < tlen; i++) {
                        if (!is_digit(token[i])) { only_digits = 0; break; }
                    }
                    if (!only_digits) {
                        porter_stem(token);
                        tlen = str_len(token);
                        if (tlen >= 2) {
                            Vec *pl = map_get_or_create(idx, token);
                            if (pl->len == 0 || pl->items[pl->len - 1] != doc_id) {
                                vec_push(pl, doc_id);
                            }
                        }
                    }
                }
            }
            tlen = 0;
        }
        p++;
    }
    if (tlen >= 2) {
        while (tlen > 0 && token[tlen-1] == '\'') tlen--;
        if (tlen >= 2) {
            token[tlen] = '\0';
            int only_digits = 1;
            for (int i = 0; i < tlen; i++) {
                if (!is_digit(token[i])) { only_digits = 0; break; }
            }
            if (!only_digits) {
                porter_stem(token);
                tlen = str_len(token);
                if (tlen >= 2) {
                    Vec *pl = map_get_or_create(idx, token);
                    if (pl->len == 0 || pl->items[pl->len - 1] != doc_id) {
                        vec_push(pl, doc_id);
                    }
                }
            }
        }
    }
}

struct TermEntry {
    char *term;
    Vec *postings;
};

static int term_cmp(const void *a, const void *b) {
    return str_cmp(((TermEntry*)a)->term, ((TermEntry*)b)->term);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: indexer <corpus_dir> <index_file>\n");
        return 1;
    }
    const char *corpus_dir = argv[1];
    const char *index_path = argv[2];

    Map idx;
    map_init(&idx, 200003);
    DocArr docs;
    docarr_init(&docs);

    DIR *d = opendir(corpus_dir);
    if (!d) { fprintf(stderr, "Cannot open %s\n", corpus_dir); return 1; }

    struct dirent *ent;
    while ((ent = readdir(d)) != 0) {
        int nlen = str_len(ent->d_name);
        if (nlen < 4) continue;
        if (!ends_with(ent->d_name, nlen, ".txt", 4)) continue;
        if (str_cmp(ent->d_name, "stats.txt") == 0) continue;
        if (str_cmp(ent->d_name, "metadata.csv") == 0) continue;

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", corpus_dir, ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char *title = read_line(f);
        char *url = read_line(f);
        if (!title || !url) { fclose(f); delete[] title; delete[] url; continue; }

        char text[1048576];
        int text_len = 0;
        char line[65536];
        while (fgets(line, sizeof(line), f)) {
            int ll = str_len(line);
            if (text_len + ll < (int)sizeof(text) - 1) {
                for (int i = 0; i < ll; i++) text[text_len++] = line[i];
            }
        }
        text[text_len] = '\0';
        fclose(f);

        int doc_id = docs.len;
        docarr_push(&docs, title, url, "");
        tokenize_and_index(text, &idx, doc_id);

        delete[] title;
        delete[] url;

        if (docs.len % 5000 == 0) fprintf(stderr, "indexed %d docs\n", docs.len);
    }
    closedir(d);

    fprintf(stderr, "Building index: %d docs, %d terms\n", docs.len, idx.size);

    TermEntry *terms = new TermEntry[idx.size];
    int tc = 0;
    MapIter it;
    mapiter_init(&it, &idx);
    while (mapiter_next(&it)) {
        terms[tc].term = mapiter_key(&it);
        terms[tc].postings = mapiter_val(&it);
        tc++;
    }
    qsort(terms, tc, sizeof(TermEntry), term_cmp);

    FILE *out = fopen(index_path, "wb");
    if (!out) { fprintf(stderr, "Cannot open %s for writing\n", index_path); return 1; }

    fwrite("YIDX", 1, 4, out);
    write_u32(out, 1);
    write_u32(out, (unsigned int)tc);
    write_u32(out, (unsigned int)docs.len);
    unsigned long long placeholder = 0;
    long fwd_off_pos = ftell(out);
    write_u64(out, placeholder);
    long dict_off_pos = ftell(out);
    write_u64(out, placeholder);
    long post_off_pos = ftell(out);
    write_u64(out, placeholder);

    unsigned long long fwd_offset = (unsigned long long)ftell(out);
    for (int i = 0; i < docs.len; i++) {
        int ulen = str_len(docs.items[i].url);
        int tlen = str_len(docs.items[i].title);
        int slen = str_len(docs.items[i].source);
        write_u16(out, (unsigned short)ulen);
        fwrite(docs.items[i].url, 1, ulen, out);
        write_u16(out, (unsigned short)tlen);
        fwrite(docs.items[i].title, 1, tlen, out);
        write_u16(out, (unsigned short)slen);
        fwrite(docs.items[i].source, 1, slen, out);
    }

    unsigned long long post_offset = (unsigned long long)ftell(out);
    unsigned long long *term_post_off = new unsigned long long[tc];
    for (int i = 0; i < tc; i++) {
        term_post_off[i] = (unsigned long long)ftell(out);
        Vec *pl = terms[i].postings;
        for (int j = 0; j < pl->len; j++) {
            write_u32(out, (unsigned int)pl->items[j]);
        }
    }

    unsigned long long dict_offset = (unsigned long long)ftell(out);
    for (int i = 0; i < tc; i++) {
        int tl = str_len(terms[i].term);
        write_u8(out, (unsigned char)tl);
        fwrite(terms[i].term, 1, tl, out);
        write_u64(out, term_post_off[i]);
        write_u32(out, (unsigned int)terms[i].postings->len);
    }

    fseek(out, fwd_off_pos, SEEK_SET);
    write_u64(out, fwd_offset);
    fseek(out, dict_off_pos, SEEK_SET);
    write_u64(out, dict_offset);
    fseek(out, post_off_pos, SEEK_SET);
    write_u64(out, post_offset);

    fclose(out);

    long long total_postings = 0;
    for (int i = 0; i < tc; i++) total_postings += terms[i].postings->len;
    fprintf(stderr, "Index written: %d terms, %d docs, %lld postings\n", tc, docs.len, total_postings);

    delete[] term_post_off;
    delete[] terms;
    return 0;
}
