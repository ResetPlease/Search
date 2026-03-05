#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>

static bool all_digits(const std::string &s) {
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}

static void tokenize_text(const char *text, FILE *out) {
    std::string tok;
    const char *p = text;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        bool is_letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        bool is_num = (c >= '0' && c <= '9');
        bool is_apos = (c == '\'');
        if (is_letter || is_num || (is_apos && !tok.empty())) {
            char lo = c;
            if (lo >= 'A' && lo <= 'Z') lo += 32;
            tok += lo;
        } else {
            if (tok.size() >= 2 && !all_digits(tok)) {
                while (!tok.empty() && tok.back() == '\'') tok.pop_back();
                if (tok.size() >= 2) {
                    fprintf(out, "%s\n", tok.c_str());
                }
            }
            tok.clear();
        }
        p++;
    }
    if (tok.size() >= 2 && !all_digits(tok)) {
        while (!tok.empty() && tok.back() == '\'') tok.pop_back();
        if (tok.size() >= 2) {
            fprintf(out, "%s\n", tok.c_str());
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: tokenizer <corpus_dir> <output_file>\n");
        return 1;
    }
    const char *corpus_dir = argv[1];
    const char *out_path = argv[2];

    FILE *out = fopen(out_path, "w");
    if (!out) { fprintf(stderr, "Cannot open %s\n", out_path); return 1; }

    DIR *dir = opendir(corpus_dir);
    if (!dir) { fprintf(stderr, "Cannot open dir %s\n", corpus_dir); fclose(out); return 1; }

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        int nlen = strlen(ent->d_name);
        if (nlen < 4) continue;
        if (strcmp(ent->d_name + nlen - 4, ".txt") != 0) continue;
        if (strcmp(ent->d_name, "stats.txt") == 0) continue;

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", corpus_dir, ent->d_name);

        std::ifstream ifs(path);
        if (!ifs.is_open()) continue;

        std::string line;
        std::getline(ifs, line);
        std::getline(ifs, line);

        std::string text;
        while (std::getline(ifs, line)) {
            text += line;
            text += ' ';
        }

        tokenize_text(text.c_str(), out);
        count++;
        if (count % 5000 == 0) fprintf(stderr, "tokenized %d files\n", count);
    }
    closedir(dir);
    fclose(out);
    fprintf(stderr, "Done: %d files tokenized\n", count);
    return 0;
}
