#ifndef MAP_H
#define MAP_H

#include "mystr.h"
#include "vec.h"

struct MapEntry {
    char *key;
    Vec postings;
    int occupied;
};

struct Map {
    MapEntry *slots;
    int capacity;
    int size;
};

static unsigned int fnv1a(const char *key) {
    unsigned int h = 2166136261u;
    while (*key) {
        h ^= (unsigned char)(*key);
        h *= 16777619u;
        key++;
    }
    return h;
}

inline void map_init(Map *m, int cap) {
    m->capacity = cap;
    m->size = 0;
    m->slots = new MapEntry[cap];
    for (int i = 0; i < cap; i++) {
        m->slots[i].key = 0;
        m->slots[i].occupied = 0;
        vec_init(&m->slots[i].postings);
    }
}

inline void map_free(Map *m) {
    for (int i = 0; i < m->capacity; i++) {
        if (m->slots[i].occupied) {
            delete[] m->slots[i].key;
            vec_free(&m->slots[i].postings);
        }
    }
    delete[] m->slots;
    m->slots = 0;
    m->capacity = 0;
    m->size = 0;
}

inline int map_find_slot(Map *m, const char *key) {
    unsigned int idx = fnv1a(key) % (unsigned int)m->capacity;
    while (m->slots[idx].occupied) {
        if (str_cmp(m->slots[idx].key, key) == 0) return (int)idx;
        idx = (idx + 1) % (unsigned int)m->capacity;
    }
    return (int)idx;
}

inline void map_resize(Map *m) {
    int old_cap = m->capacity;
    MapEntry *old = m->slots;
    int new_cap = old_cap * 2 + 1;
    m->slots = new MapEntry[new_cap];
    m->capacity = new_cap;
    m->size = 0;
    for (int i = 0; i < new_cap; i++) {
        m->slots[i].key = 0;
        m->slots[i].occupied = 0;
        vec_init(&m->slots[i].postings);
    }
    for (int i = 0; i < old_cap; i++) {
        if (old[i].occupied) {
            int s = map_find_slot(m, old[i].key);
            m->slots[s].key = old[i].key;
            m->slots[s].postings = old[i].postings;
            m->slots[s].occupied = 1;
            m->size++;
        }
    }
    delete[] old;
}

inline Vec *map_get_or_create(Map *m, const char *key) {
    if (m->size * 10 > m->capacity * 7) {
        map_resize(m);
    }
    int s = map_find_slot(m, key);
    if (!m->slots[s].occupied) {
        m->slots[s].key = str_dup(key);
        m->slots[s].occupied = 1;
        vec_init(&m->slots[s].postings);
        m->size++;
    }
    return &m->slots[s].postings;
}

inline Vec *map_get(Map *m, const char *key) {
    int s = map_find_slot(m, key);
    if (m->slots[s].occupied) return &m->slots[s].postings;
    return 0;
}

struct MapIter {
    Map *m;
    int pos;
};

inline void mapiter_init(MapIter *it, Map *m) {
    it->m = m;
    it->pos = -1;
}

inline int mapiter_next(MapIter *it) {
    while (++it->pos < it->m->capacity) {
        if (it->m->slots[it->pos].occupied) return 1;
    }
    return 0;
}

inline char *mapiter_key(MapIter *it) {
    return it->m->slots[it->pos].key;
}

inline Vec *mapiter_val(MapIter *it) {
    return &it->m->slots[it->pos].postings;
}

#endif
