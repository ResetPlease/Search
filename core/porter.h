#ifndef PORTER_H
#define PORTER_H

#include "mystr.h"

static int is_consonant(const char *w, int i) {
    char c = w[i];
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') return 0;
    if (c == 'y') {
        if (i == 0) return 1;
        return !is_consonant(w, i - 1);
    }
    return 1;
}

static int measure(const char *w, int len) {
    int m = 0, i = 0;
    while (i < len && is_consonant(w, i)) i++;
    while (i < len) {
        while (i < len && !is_consonant(w, i)) i++;
        if (i >= len) break;
        m++;
        while (i < len && is_consonant(w, i)) i++;
    }
    return m;
}

static int has_vowel(const char *w, int len) {
    for (int i = 0; i < len; i++) {
        if (!is_consonant(w, i)) return 1;
    }
    return 0;
}

static int double_cons(const char *w, int len) {
    if (len < 2) return 0;
    if (w[len - 1] != w[len - 2]) return 0;
    return is_consonant(w, len - 1);
}

static int cvc(const char *w, int len) {
    if (len < 3) return 0;
    if (!is_consonant(w, len - 1)) return 0;
    if (is_consonant(w, len - 2)) return 0;
    if (!is_consonant(w, len - 3)) return 0;
    char c = w[len - 1];
    if (c == 'w' || c == 'x' || c == 'y') return 0;
    return 1;
}

static int suf_match(const char *w, int len, const char *suf) {
    int sl = str_len(suf);
    return ends_with(w, len, suf, sl);
}

static void suf_replace(char *w, int *len, const char *old_suf, const char *new_suf) {
    int ol = str_len(old_suf);
    int nl = str_len(new_suf);
    int base = *len - ol;
    for (int i = 0; i < nl; i++) w[base + i] = new_suf[i];
    w[base + nl] = '\0';
    *len = base + nl;
}

static void step1a(char *w, int *len) {
    if (suf_match(w, *len, "sses")) { suf_replace(w, len, "sses", "ss"); return; }
    if (suf_match(w, *len, "ies")) { suf_replace(w, len, "ies", "i"); return; }
    if (*len > 1 && !suf_match(w, *len, "ss") && w[*len - 1] == 's') {
        w[--(*len)] = '\0';
    }
}

static void step1b(char *w, int *len) {
    if (suf_match(w, *len, "eed")) {
        int base = *len - 3;
        if (measure(w, base) > 0) suf_replace(w, len, "eed", "ee");
        return;
    }
    int changed = 0;
    if (suf_match(w, *len, "ed")) {
        int base = *len - 2;
        if (has_vowel(w, base)) { w[base] = '\0'; *len = base; changed = 1; }
    } else if (suf_match(w, *len, "ing")) {
        int base = *len - 3;
        if (has_vowel(w, base)) { w[base] = '\0'; *len = base; changed = 1; }
    }
    if (changed) {
        if (suf_match(w, *len, "at")) { suf_replace(w, len, "at", "ate"); }
        else if (suf_match(w, *len, "bl")) { suf_replace(w, len, "bl", "ble"); }
        else if (suf_match(w, *len, "iz")) { suf_replace(w, len, "iz", "ize"); }
        else if (double_cons(w, *len) && w[*len-1] != 'l' && w[*len-1] != 's' && w[*len-1] != 'z') {
            w[--(*len)] = '\0';
        }
        else if (measure(w, *len) == 1 && cvc(w, *len)) {
            w[*len] = 'e'; w[++(*len)] = '\0';
        }
    }
}

static void step1c(char *w, int *len) {
    if (w[*len - 1] == 'y' && has_vowel(w, *len - 1)) {
        w[*len - 1] = 'i';
    }
}

static void step2(char *w, int *len) {
    struct { const char *from; const char *to; } rules[] = {
        {"ational", "ate"}, {"tional", "tion"}, {"enci", "ence"}, {"anci", "ance"},
        {"izer", "ize"}, {"abli", "able"}, {"alli", "al"}, {"entli", "ent"},
        {"eli", "e"}, {"ousli", "ous"}, {"ization", "ize"}, {"ation", "ate"},
        {"ator", "ate"}, {"alism", "al"}, {"iveness", "ive"}, {"fulness", "ful"},
        {"ousness", "ous"}, {"aliti", "al"}, {"iviti", "ive"}, {"biliti", "ble"},
    };
    int n = sizeof(rules) / sizeof(rules[0]);
    for (int i = 0; i < n; i++) {
        if (suf_match(w, *len, rules[i].from)) {
            int fl = str_len(rules[i].from);
            if (measure(w, *len - fl) > 0) {
                suf_replace(w, len, rules[i].from, rules[i].to);
            }
            return;
        }
    }
}

static void step3(char *w, int *len) {
    struct { const char *from; const char *to; } rules[] = {
        {"icate", "ic"}, {"ative", ""}, {"alize", "al"},
        {"iciti", "ic"}, {"ical", "ic"}, {"ful", ""}, {"ness", ""},
    };
    int n = sizeof(rules) / sizeof(rules[0]);
    for (int i = 0; i < n; i++) {
        if (suf_match(w, *len, rules[i].from)) {
            int fl = str_len(rules[i].from);
            if (measure(w, *len - fl) > 0) {
                suf_replace(w, len, rules[i].from, rules[i].to);
            }
            return;
        }
    }
}

static void step4(char *w, int *len) {
    const char *suffixes[] = {
        "al", "ance", "ence", "er", "ic", "able", "ible", "ant",
        "ement", "ment", "ent", "ion", "ou", "ism", "ate", "iti",
        "ous", "ive", "ize",
    };
    int n = sizeof(suffixes) / sizeof(suffixes[0]);
    for (int i = 0; i < n; i++) {
        if (suf_match(w, *len, suffixes[i])) {
            int sl = str_len(suffixes[i]);
            int base = *len - sl;
            if (str_cmp(suffixes[i], "ion") == 0) {
                if (base > 0 && (w[base - 1] == 's' || w[base - 1] == 't') && measure(w, base) > 1) {
                    w[base] = '\0';
                    *len = base;
                }
            } else {
                if (measure(w, base) > 1) {
                    w[base] = '\0';
                    *len = base;
                }
            }
            return;
        }
    }
}

static void step5a(char *w, int *len) {
    if (w[*len - 1] == 'e') {
        int base = *len - 1;
        if (measure(w, base) > 1) { w[--(*len)] = '\0'; return; }
        if (measure(w, base) == 1 && !cvc(w, base)) { w[--(*len)] = '\0'; }
    }
}

static void step5b(char *w, int *len) {
    if (measure(w, *len) > 1 && double_cons(w, *len) && w[*len - 1] == 'l') {
        w[--(*len)] = '\0';
    }
}

inline void porter_stem(char *w) {
    int len = str_len(w);
    if (len <= 2) return;
    step1a(w, &len);
    step1b(w, &len);
    step1c(w, &len);
    step2(w, &len);
    step3(w, &len);
    step4(w, &len);
    step5a(w, &len);
    step5b(w, &len);
}

#endif
