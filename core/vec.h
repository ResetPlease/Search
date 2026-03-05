#ifndef VEC_H
#define VEC_H

#include "mystr.h"

struct Vec {
    int *items;
    int len;
    int cap;
};

inline void vec_init(Vec *v) {
    v->items = 0;
    v->len = 0;
    v->cap = 0;
}

inline void vec_free(Vec *v) {
    delete[] v->items;
    v->items = 0;
    v->len = 0;
    v->cap = 0;
}

inline void vec_push(Vec *v, int val) {
    if (v->len >= v->cap) {
        int nc = v->cap == 0 ? 32 : v->cap * 2;
        int *nd = new int[nc];
        for (int i = 0; i < v->len; i++) nd[i] = v->items[i];
        delete[] v->items;
        v->items = nd;
        v->cap = nc;
    }
    v->items[v->len++] = val;
}

static void merge_halves(int *arr, int *tmp, int lo, int mid, int hi) {
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) {
        if (arr[i] <= arr[j]) tmp[k++] = arr[i++];
        else tmp[k++] = arr[j++];
    }
    while (i < mid) tmp[k++] = arr[i++];
    while (j < hi) tmp[k++] = arr[j++];
    for (int x = lo; x < hi; x++) arr[x] = tmp[x];
}

static void merge_sort_range(int *arr, int *tmp, int lo, int hi) {
    if (hi - lo <= 1) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort_range(arr, tmp, lo, mid);
    merge_sort_range(arr, tmp, mid, hi);
    merge_halves(arr, tmp, lo, mid, hi);
}

inline void vec_sort(Vec *v) {
    if (v->len <= 1) return;
    int *tmp = new int[v->len];
    merge_sort_range(v->items, tmp, 0, v->len);
    delete[] tmp;
}

inline void vec_unique(Vec *v) {
    if (v->len <= 1) return;
    int w = 1;
    for (int i = 1; i < v->len; i++) {
        if (v->items[i] != v->items[w - 1]) {
            v->items[w++] = v->items[i];
        }
    }
    v->len = w;
}

inline Vec vec_intersect(Vec *a, Vec *b) {
    Vec r;
    vec_init(&r);
    int i = 0, j = 0;
    while (i < a->len && j < b->len) {
        if (a->items[i] == b->items[j]) {
            vec_push(&r, a->items[i]);
            i++; j++;
        } else if (a->items[i] < b->items[j]) {
            i++;
        } else {
            j++;
        }
    }
    return r;
}

inline Vec vec_unite(Vec *a, Vec *b) {
    Vec r;
    vec_init(&r);
    int i = 0, j = 0;
    while (i < a->len && j < b->len) {
        if (a->items[i] == b->items[j]) {
            vec_push(&r, a->items[i]);
            i++; j++;
        } else if (a->items[i] < b->items[j]) {
            vec_push(&r, a->items[i++]);
        } else {
            vec_push(&r, b->items[j++]);
        }
    }
    while (i < a->len) vec_push(&r, a->items[i++]);
    while (j < b->len) vec_push(&r, b->items[j++]);
    return r;
}

inline Vec vec_subtract(Vec *a, Vec *b) {
    Vec r;
    vec_init(&r);
    int i = 0, j = 0;
    while (i < a->len && j < b->len) {
        if (a->items[i] == b->items[j]) {
            i++; j++;
        } else if (a->items[i] < b->items[j]) {
            vec_push(&r, a->items[i++]);
        } else {
            j++;
        }
    }
    while (i < a->len) vec_push(&r, a->items[i++]);
    return r;
}

struct StrVec {
    char **items;
    int len;
    int cap;
};

inline void strvec_init(StrVec *v) {
    v->items = 0;
    v->len = 0;
    v->cap = 0;
}

inline void strvec_free(StrVec *v) {
    for (int i = 0; i < v->len; i++) delete[] v->items[i];
    delete[] v->items;
    v->items = 0;
    v->len = 0;
    v->cap = 0;
}

inline void strvec_push(StrVec *v, const char *s) {
    if (v->len >= v->cap) {
        int nc = v->cap == 0 ? 32 : v->cap * 2;
        char **nd = new char*[nc];
        for (int i = 0; i < v->len; i++) nd[i] = v->items[i];
        delete[] v->items;
        v->items = nd;
        v->cap = nc;
    }
    v->items[v->len++] = str_dup(s);
}

#endif
