#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "mystr.h"
#include "vec.h"
#include "porter.h"

struct IndexFile {
    FILE *fp;
    unsigned int num_terms;
    unsigned int num_docs;
    unsigned long long fwd_offset;
    unsigned long long dict_offset;
    unsigned long long post_offset;
    char **dict_terms;
    unsigned long long *dict_post_off;
    unsigned int *dict_post_cnt;
    char **doc_urls;
    char **doc_titles;
};

static void read_u16(FILE *f, unsigned short *v) { fread(v, 2, 1, f); }
static void read_u32(FILE *f, unsigned int *v) { fread(v, 4, 1, f); }
static void read_u64(FILE *f, unsigned long long *v) { fread(v, 8, 1, f); }

static int idx_open(IndexFile *idx, const char *path) {
    idx->fp = fopen(path, "rb");
    if (!idx->fp) return 0;

    char magic[4];
    fread(magic, 1, 4, idx->fp);
    if (magic[0]!='Y'||magic[1]!='I'||magic[2]!='D'||magic[3]!='X') {
        fclose(idx->fp);
        return 0;
    }

    unsigned int ver;
    read_u32(idx->fp, &ver);
    read_u32(idx->fp, &idx->num_terms);
    read_u32(idx->fp, &idx->num_docs);
    read_u64(idx->fp, &idx->fwd_offset);
    read_u64(idx->fp, &idx->dict_offset);
    read_u64(idx->fp, &idx->post_offset);

    idx->doc_urls = new char*[idx->num_docs];
    idx->doc_titles = new char*[idx->num_docs];
    fseek(idx->fp, (long)idx->fwd_offset, SEEK_SET);
    for (unsigned int i = 0; i < idx->num_docs; i++) {
        unsigned short ulen, tlen, slen;
        read_u16(idx->fp, &ulen);
        char *url = new char[ulen + 1];
        fread(url, 1, ulen, idx->fp);
        url[ulen] = '\0';
        idx->doc_urls[i] = url;

        read_u16(idx->fp, &tlen);
        char *title = new char[tlen + 1];
        fread(title, 1, tlen, idx->fp);
        title[tlen] = '\0';
        idx->doc_titles[i] = title;

        read_u16(idx->fp, &slen);
        fseek(idx->fp, slen, SEEK_CUR);
    }

    idx->dict_terms = new char*[idx->num_terms];
    idx->dict_post_off = new unsigned long long[idx->num_terms];
    idx->dict_post_cnt = new unsigned int[idx->num_terms];
    fseek(idx->fp, (long)idx->dict_offset, SEEK_SET);
    for (unsigned int i = 0; i < idx->num_terms; i++) {
        unsigned char tl;
        fread(&tl, 1, 1, idx->fp);
        char *term = new char[tl + 1];
        fread(term, 1, tl, idx->fp);
        term[tl] = '\0';
        idx->dict_terms[i] = term;
        read_u64(idx->fp, &idx->dict_post_off[i]);
        read_u32(idx->fp, &idx->dict_post_cnt[i]);
    }

    return 1;
}

static int bin_search(IndexFile *idx, const char *term) {
    int lo = 0, hi = (int)idx->num_terms - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int c = str_cmp(idx->dict_terms[mid], term);
        if (c == 0) return mid;
        if (c < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static Vec idx_lookup(IndexFile *idx, const char *raw_term) {
    Vec result;
    vec_init(&result);

    char term[256];
    int tl = str_len(raw_term);
    if (tl > 254) tl = 254;
    for (int i = 0; i < tl; i++) term[i] = to_lower(raw_term[i]);
    term[tl] = '\0';
    porter_stem(term);

    int pos = bin_search(idx, term);
    if (pos < 0) return result;

    fseek(idx->fp, (long)idx->dict_post_off[pos], SEEK_SET);
    for (unsigned int i = 0; i < idx->dict_post_cnt[pos]; i++) {
        unsigned int doc_id;
        read_u32(idx->fp, &doc_id);
        vec_push(&result, (int)doc_id);
    }
    return result;
}

static Vec idx_all_docs(IndexFile *idx) {
    Vec r;
    vec_init(&r);
    for (unsigned int i = 0; i < idx->num_docs; i++) vec_push(&r, (int)i);
    return r;
}

enum OpType { OP_AND, OP_OR, OP_NOT, OP_LPAREN };

struct OpStack {
    OpType *items;
    int len;
    int cap;
};

static void opstack_init(OpStack *s) { s->items = 0; s->len = 0; s->cap = 0; }
static void opstack_free(OpStack *s) { delete[] s->items; }

static void opstack_push(OpStack *s, OpType op) {
    if (s->len >= s->cap) {
        int nc = s->cap == 0 ? 32 : s->cap * 2;
        OpType *nd = new OpType[nc];
        for (int i = 0; i < s->len; i++) nd[i] = s->items[i];
        delete[] s->items;
        s->items = nd;
        s->cap = nc;
    }
    s->items[s->len++] = op;
}

static OpType opstack_pop(OpStack *s) { return s->items[--s->len]; }
static OpType opstack_top(OpStack *s) { return s->items[s->len - 1]; }

struct VecStack {
    Vec *items;
    int len;
    int cap;
};

static void vecstack_init(VecStack *s) { s->items = 0; s->len = 0; s->cap = 0; }
static void vecstack_free(VecStack *s) { delete[] s->items; }

static void vecstack_push(VecStack *s, Vec v) {
    if (s->len >= s->cap) {
        int nc = s->cap == 0 ? 32 : s->cap * 2;
        Vec *nd = new Vec[nc];
        for (int i = 0; i < s->len; i++) nd[i] = s->items[i];
        delete[] s->items;
        s->items = nd;
        s->cap = nc;
    }
    s->items[s->len++] = v;
}

static Vec vecstack_pop(VecStack *s) { return s->items[--s->len]; }

static int precedence(OpType op) {
    if (op == OP_NOT) return 3;
    if (op == OP_AND) return 2;
    if (op == OP_OR) return 1;
    return 0;
}

static void apply_op(VecStack *vals, OpType op, IndexFile *idx) {
    if (op == OP_NOT) {
        Vec top = vecstack_pop(vals);
        Vec all = idx_all_docs(idx);
        Vec res = vec_subtract(&all, &top);
        vec_free(&top);
        vec_free(&all);
        vecstack_push(vals, res);
    } else if (op == OP_AND) {
        Vec b = vecstack_pop(vals);
        Vec a = vecstack_pop(vals);
        Vec res = vec_intersect(&a, &b);
        vec_free(&a);
        vec_free(&b);
        vecstack_push(vals, res);
    } else if (op == OP_OR) {
        Vec b = vecstack_pop(vals);
        Vec a = vecstack_pop(vals);
        Vec res = vec_unite(&a, &b);
        vec_free(&a);
        vec_free(&b);
        vecstack_push(vals, res);
    }
}

static Vec shunting_yard_eval(IndexFile *idx, const char *query) {
    OpStack ops;
    opstack_init(&ops);
    VecStack vals;
    vecstack_init(&vals);

    const char *p = query;
    int need_and = 0;

    while (*p) {
        while (*p && is_space(*p)) p++;
        if (!*p) break;

        if (*p == '(') {
            if (need_and) {
                while (ops.len > 0 && opstack_top(&ops) != OP_LPAREN && precedence(opstack_top(&ops)) >= precedence(OP_AND)) {
                    apply_op(&vals, opstack_pop(&ops), idx);
                }
                opstack_push(&ops, OP_AND);
            }
            opstack_push(&ops, OP_LPAREN);
            p++;
            need_and = 0;
        } else if (*p == ')') {
            while (ops.len > 0 && opstack_top(&ops) != OP_LPAREN) {
                apply_op(&vals, opstack_pop(&ops), idx);
            }
            if (ops.len > 0) opstack_pop(&ops);
            p++;
            need_and = 1;
        } else if (*p == '!' ) {
            if (need_and) {
                while (ops.len > 0 && opstack_top(&ops) != OP_LPAREN && precedence(opstack_top(&ops)) >= precedence(OP_AND)) {
                    apply_op(&vals, opstack_pop(&ops), idx);
                }
                opstack_push(&ops, OP_AND);
            }
            opstack_push(&ops, OP_NOT);
            p++;
            need_and = 0;
        } else if (*p == '&' && *(p+1) == '&') {
            while (ops.len > 0 && opstack_top(&ops) != OP_LPAREN && precedence(opstack_top(&ops)) >= precedence(OP_AND)) {
                apply_op(&vals, opstack_pop(&ops), idx);
            }
            opstack_push(&ops, OP_AND);
            p += 2;
            need_and = 0;
        } else if (*p == '|' && *(p+1) == '|') {
            while (ops.len > 0 && opstack_top(&ops) != OP_LPAREN && precedence(opstack_top(&ops)) >= precedence(OP_OR)) {
                apply_op(&vals, opstack_pop(&ops), idx);
            }
            opstack_push(&ops, OP_OR);
            p += 2;
            need_and = 0;
        } else {
            if (need_and) {
                while (ops.len > 0 && opstack_top(&ops) != OP_LPAREN && precedence(opstack_top(&ops)) >= precedence(OP_AND)) {
                    apply_op(&vals, opstack_pop(&ops), idx);
                }
                opstack_push(&ops, OP_AND);
            }
            char word[256];
            int wl = 0;
            while (*p && !is_space(*p) && *p != '(' && *p != ')' && *p != '!' && !(*p == '&' && *(p+1) == '&') && !(*p == '|' && *(p+1) == '|')) {
                if (wl < 254) word[wl++] = *p;
                p++;
            }
            word[wl] = '\0';
            Vec r = idx_lookup(idx, word);
            vecstack_push(&vals, r);
            need_and = 1;
        }
    }

    while (ops.len > 0) {
        apply_op(&vals, opstack_pop(&ops), idx);
    }

    Vec result;
    vec_init(&result);
    if (vals.len > 0) result = vecstack_pop(&vals);

    opstack_free(&ops);
    vecstack_free(&vals);
    return result;
}

int main(int argc, char **argv) {
    int json_mode = 0;
    int limit = 0;
    const char *index_path = 0;

    for (int i = 1; i < argc; i++) {
        if (str_cmp(argv[i], "--json") == 0) json_mode = 1;
        else if (str_cmp(argv[i], "--limit") == 0 && i + 1 < argc) { limit = atoi(argv[++i]); }
        else if (!index_path) index_path = argv[i];
    }

    if (!index_path) {
        fprintf(stderr, "Usage: search_cli <index.bin> [--json] [--limit N]\n");
        return 1;
    }

    IndexFile idx;
    if (!idx_open(&idx, index_path)) {
        fprintf(stderr, "Failed to open index: %s\n", index_path);
        return 1;
    }
    fprintf(stderr, "Index loaded: %u terms, %u docs\n", idx.num_terms, idx.num_docs);

    char query[4096];
    while (fgets(query, sizeof(query), stdin)) {
        int ql = str_len(query);
        while (ql > 0 && (query[ql-1] == '\n' || query[ql-1] == '\r')) query[--ql] = '\0';
        if (ql == 0) continue;

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        Vec results = shunting_yard_eval(&idx, query);

        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;

        int show = results.len;
        if (limit > 0 && show > limit) show = limit;

        if (json_mode) {
            printf("{\"query\":\"%s\",\"total\":%d,\"time_ms\":%.2f,\"results\":[", query, results.len, ms);
            for (int i = 0; i < show; i++) {
                int d = results.items[i];
                if (i > 0) printf(",");
                printf("{\"id\":%d,\"title\":\"", d);
                for (const char *c = idx.doc_titles[d]; *c; c++) {
                    if (*c == '"') printf("\\\"");
                    else if (*c == '\\') printf("\\\\");
                    else putchar(*c);
                }
                printf("\",\"url\":\"");
                for (const char *c = idx.doc_urls[d]; *c; c++) {
                    if (*c == '"') printf("\\\"");
                    else if (*c == '\\') printf("\\\\");
                    else putchar(*c);
                }
                printf("\"}");
            }
            printf("]}\n");
        } else {
            printf("Query: %s\nResults: %d (%.2f ms)\n", query, results.len, ms);
            for (int i = 0; i < show; i++) {
                int d = results.items[i];
                printf("  [%d] %s\n       %s\n", d, idx.doc_titles[d], idx.doc_urls[d]);
            }
            printf("\n");
        }
        fflush(stdout);
        vec_free(&results);
    }

    return 0;
}
