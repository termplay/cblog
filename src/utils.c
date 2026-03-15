#include "utils.h"
#include "cblog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* ── File I/O ────────────────────────────────────────────────────────── */

char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

bool write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot write %s: %s\n", path, strerror(errno));
        return false;
    }
    size_t len = strlen(content);
    size_t wr  = fwrite(content, 1, len, f);
    fclose(f);
    return wr == len;
}

/* ── Directory operations ────────────────────────────────────────────── */

bool mkdir_p(const char *path) {
    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);

    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

bool dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool copy_file(const char *src, const char *dst) {
    FILE *in  = fopen(src, "rb");
    if (!in) return false;

    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in); fclose(out);
            return false;
        }
    }
    fclose(in);
    fclose(out);
    return true;
}

bool copy_dir_recursive(const char *src, const char *dst) {
    DIR *dir = opendir(src);
    if (!dir) return false;

    mkdir_p(dst);

    struct dirent *ent;
    bool ok = true;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char src_path[MAX_PATH_LEN], dst_path[MAX_PATH_LEN];
        path_join(src_path, sizeof(src_path), src, ent->d_name);
        path_join(dst_path, sizeof(dst_path), dst, ent->d_name);

        struct stat st;
        if (stat(src_path, &st) != 0) { ok = false; continue; }

        if (S_ISDIR(st.st_mode)) {
            if (!copy_dir_recursive(src_path, dst_path)) ok = false;
        } else {
            if (!copy_file(src_path, dst_path)) ok = false;
        }
    }
    closedir(dir);
    return ok;
}

bool rm_rf_dir(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return false;

    struct dirent *ent;
    bool ok = true;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char full[MAX_PATH_LEN];
        path_join(full, sizeof(full), path, ent->d_name);

        struct stat st;
        if (stat(full, &st) != 0) { ok = false; continue; }

        if (S_ISDIR(st.st_mode)) {
            if (!rm_rf_dir(full)) ok = false;
        } else {
            if (unlink(full) != 0) ok = false;
        }
    }
    closedir(dir);
    if (rmdir(path) != 0) ok = false;
    return ok;
}

bool remove_file(const char *path) {
    return unlink(path) == 0;
}

/* ── String utilities ────────────────────────────────────────────────── */

char *slugify(const char *input) {
    if (!input) return NULL;
    size_t len = strlen(input);
    char *slug = malloc(len + 1);
    if (!slug) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = input[i];
        if (isalnum((unsigned char)c)) {
            slug[j++] = (char)tolower((unsigned char)c);
        } else if (c == ' ' || c == '-' || c == '_') {
            if (j > 0 && slug[j - 1] != '-')
                slug[j++] = '-';
        }
    }
    /* trim trailing dash */
    while (j > 0 && slug[j - 1] == '-') j--;
    slug[j] = '\0';
    return slug;
}

char *str_replace(const char *haystack, const char *needle, const char *replacement) {
    if (!haystack || !needle || !replacement) return NULL;

    size_t needle_len = strlen(needle);
    size_t repl_len   = strlen(replacement);
    if (needle_len == 0) return strdup(haystack);

    /* count occurrences */
    int count = 0;
    const char *p = haystack;
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += needle_len;
    }
    if (count == 0) return strdup(haystack);

    size_t result_len = strlen(haystack) + (size_t)count * (repl_len - needle_len);
    char *result = malloc(result_len + 1);
    if (!result) return NULL;

    char *out = result;
    p = haystack;
    while (*p) {
        if (strncmp(p, needle, needle_len) == 0) {
            memcpy(out, replacement, repl_len);
            out += repl_len;
            p   += needle_len;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    return result;
}

char *str_trim(char *s) {
    if (!s) return NULL;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

char *str_dup(const char *s) {
    if (!s) return NULL;
    return strdup(s);
}

void str_to_lower(char *s) {
    if (!s) return;
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

/* ── Date/time ───────────────────────────────────────────────────────── */

void get_current_date(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%Y-%m-%d", t);
}

void get_rfc822_date(const char *iso, char *buf, size_t len) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    if (sscanf(iso, "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday) == 3) {
        t.tm_year -= 1900;
        t.tm_mon  -= 1;
        mktime(&t);
        strftime(buf, len, "%a, %d %b %Y 00:00:00 +0000", &t);
    } else {
        snprintf(buf, len, "%s", iso);
    }
}

/* ── HTML escaping ───────────────────────────────────────────────────── */

char *html_escape(const char *input) {
    if (!input) return NULL;
    size_t len = strlen(input);
    /* worst case: every char is & -> &amp; (5x) */
    char *out = malloc(len * 5 + 1);
    if (!out) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '&':  memcpy(out + j, "&amp;",  5); j += 5; break;
            case '<':  memcpy(out + j, "&lt;",   4); j += 4; break;
            case '>':  memcpy(out + j, "&gt;",   4); j += 4; break;
            case '"':  memcpy(out + j, "&quot;", 6); j += 6; break;
            case '\'': memcpy(out + j, "&#39;",  5); j += 5; break;
            default:   out[j++] = input[i]; break;
        }
    }
    out[j] = '\0';
    return out;
}

/* ── Path manipulation ───────────────────────────────────────────────── */

void path_join(char *out, size_t out_len, const char *a, const char *b) {
    size_t a_len = strlen(a);
    if (a_len > 0 && a[a_len - 1] == '/')
        snprintf(out, out_len, "%s%s", a, b);
    else
        snprintf(out, out_len, "%s/%s", a, b);
}

const char *path_basename(const char *path) {
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

const char *path_extension(const char *path) {
    const char *base = path_basename(path);
    const char *dot  = strrchr(base, '.');
    return dot ? dot : "";
}
