#ifndef MYSTR_H
#define MYSTR_H

inline int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

inline char *str_dup(const char *s) {
    int n = str_len(s);
    char *r = new char[n + 1];
    for (int i = 0; i <= n; i++) r[i] = s[i];
    return r;
}

inline int str_cmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

inline void str_cpy(char *dst, const char *src) {
    while (*src) { *dst = *src; dst++; src++; }
    *dst = '\0';
}

inline char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

inline void to_lower_inplace(char *s) {
    while (*s) { *s = to_lower(*s); s++; }
}

inline char *to_lower_copy(const char *s) {
    char *r = str_dup(s);
    to_lower_inplace(r);
    return r;
}

inline int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline int is_digit(char c) {
    return c >= '0' && c <= '9';
}

inline int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

inline int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline int ends_with(const char *s, int slen, const char *suf, int suflen) {
    if (suflen > slen) return 0;
    for (int i = 0; i < suflen; i++) {
        if (s[slen - suflen + i] != suf[i]) return 0;
    }
    return 1;
}

#endif
