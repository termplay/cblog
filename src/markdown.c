#include "markdown.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* ── Dynamic buffer ──────────────────────────────────────────────────── */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_init(Buf *b) {
    b->cap  = 1024;
    b->data = malloc(b->cap);
    b->len  = 0;
    b->data[0] = '\0';
}

static void buf_grow(Buf *b, size_t need) {
    while (b->len + need + 1 > b->cap) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
}

static void buf_append(Buf *b, const char *s, size_t n) {
    buf_grow(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void buf_appends(Buf *b, const char *s) {
    buf_append(b, s, strlen(s));
}

/* ── Inline parser ───────────────────────────────────────────────────── */

static void parse_inline(Buf *out, const char *text, size_t len);

static void html_escape_to(Buf *out, char c) {
    switch (c) {
        case '&': buf_appends(out, "&amp;");  break;
        case '<': buf_appends(out, "&lt;");   break;
        case '>': buf_appends(out, "&gt;");   break;
        case '"': buf_appends(out, "&quot;"); break;
        default:  buf_append(out, &c, 1);     break;
    }
}

static void parse_inline(Buf *out, const char *text, size_t len) {
    size_t i = 0;
    while (i < len) {
        /* inline code */
        if (text[i] == '`') {
            size_t j = i + 1;
            while (j < len && text[j] != '`') j++;
            if (j < len) {
                buf_appends(out, "<code>");
                for (size_t k = i + 1; k < j; k++)
                    html_escape_to(out, text[k]);
                buf_appends(out, "</code>");
                i = j + 1;
                continue;
            }
        }

        /* images: ![alt](url) */
        if (text[i] == '!' && i + 1 < len && text[i + 1] == '[') {
            size_t alt_start = i + 2;
            size_t alt_end   = alt_start;
            while (alt_end < len && text[alt_end] != ']') alt_end++;
            if (alt_end < len && alt_end + 1 < len && text[alt_end + 1] == '(') {
                size_t url_start = alt_end + 2;
                size_t url_end   = url_start;
                while (url_end < len && text[url_end] != ')') url_end++;
                if (url_end < len) {
                    buf_appends(out, "<img src=\"");
                    buf_append(out, text + url_start, url_end - url_start);
                    buf_appends(out, "\" alt=\"");
                    buf_append(out, text + alt_start, alt_end - alt_start);
                    buf_appends(out, "\">");
                    i = url_end + 1;
                    continue;
                }
            }
        }

        /* links: [text](url) */
        if (text[i] == '[') {
            size_t txt_start = i + 1;
            size_t txt_end   = txt_start;
            int depth = 1;
            while (txt_end < len && depth > 0) {
                if (text[txt_end] == '[') depth++;
                else if (text[txt_end] == ']') depth--;
                if (depth > 0) txt_end++;
            }
            if (txt_end < len && txt_end + 1 < len && text[txt_end + 1] == '(') {
                size_t url_start = txt_end + 2;
                size_t url_end   = url_start;
                while (url_end < len && text[url_end] != ')') url_end++;
                if (url_end < len) {
                    buf_appends(out, "<a href=\"");
                    buf_append(out, text + url_start, url_end - url_start);
                    buf_appends(out, "\">");
                    parse_inline(out, text + txt_start, txt_end - txt_start);
                    buf_appends(out, "</a>");
                    i = url_end + 1;
                    continue;
                }
            }
        }

        /* bold+italic: ***text*** or ___text___ */
        if (i + 2 < len &&
            ((text[i] == '*' && text[i+1] == '*' && text[i+2] == '*') ||
             (text[i] == '_' && text[i+1] == '_' && text[i+2] == '_'))) {
            char marker = text[i];
            size_t j = i + 3;
            while (j + 2 < len && !(text[j] == marker && text[j+1] == marker && text[j+2] == marker))
                j++;
            if (j + 2 < len) {
                buf_appends(out, "<strong><em>");
                parse_inline(out, text + i + 3, j - i - 3);
                buf_appends(out, "</em></strong>");
                i = j + 3;
                continue;
            }
        }

        /* bold: **text** or __text__ */
        if (i + 1 < len &&
            ((text[i] == '*' && text[i+1] == '*') ||
             (text[i] == '_' && text[i+1] == '_'))) {
            char marker = text[i];
            size_t j = i + 2;
            while (j + 1 < len && !(text[j] == marker && text[j+1] == marker))
                j++;
            if (j + 1 < len) {
                buf_appends(out, "<strong>");
                parse_inline(out, text + i + 2, j - i - 2);
                buf_appends(out, "</strong>");
                i = j + 2;
                continue;
            }
        }

        /* italic: *text* or _text_ */
        if (text[i] == '*' || text[i] == '_') {
            char marker = text[i];
            size_t j = i + 1;
            while (j < len && text[j] != marker) j++;
            if (j < len && j > i + 1) {
                buf_appends(out, "<em>");
                parse_inline(out, text + i + 1, j - i - 1);
                buf_appends(out, "</em>");
                i = j + 1;
                continue;
            }
        }

        /* inline HTML passthrough */
        if (text[i] == '<' && i + 1 < len && (isalpha((unsigned char)text[i+1]) || text[i+1] == '/')) {
            size_t j = i + 1;
            while (j < len && text[j] != '>') j++;
            if (j < len) {
                buf_append(out, text + i, j - i + 1);
                i = j + 1;
                continue;
            }
        }

        /* strikethrough: ~~text~~ */
        if (i + 1 < len && text[i] == '~' && text[i+1] == '~') {
            size_t j = i + 2;
            while (j + 1 < len && !(text[j] == '~' && text[j+1] == '~'))
                j++;
            if (j + 1 < len) {
                buf_appends(out, "<del>");
                parse_inline(out, text + i + 2, j - i - 2);
                buf_appends(out, "</del>");
                i = j + 2;
                continue;
            }
        }

        /* plain text */
        html_escape_to(out, text[i]);
        i++;
    }
}

/* ── Line utilities ──────────────────────────────────────────────────── */

typedef struct {
    const char *data;
    size_t      len;
} Line;

static int split_lines(const char *md, Line **out_lines) {
    int cap = 256, count = 0;
    Line *lines = malloc(sizeof(Line) * (size_t)cap);

    const char *p = md;
    while (*p) {
        const char *eol = strchr(p, '\n');
        if (!eol) eol = p + strlen(p);

        if (count >= cap) {
            cap *= 2;
            lines = realloc(lines, sizeof(Line) * (size_t)cap);
        }
        lines[count].data = p;
        lines[count].len  = (size_t)(eol - p);
        /* strip trailing \r */
        if (lines[count].len > 0 && lines[count].data[lines[count].len - 1] == '\r')
            lines[count].len--;
        count++;

        p = *eol ? eol + 1 : eol;
    }
    *out_lines = lines;
    return count;
}

static bool line_is_blank(const Line *l) {
    for (size_t i = 0; i < l->len; i++)
        if (!isspace((unsigned char)l->data[i])) return false;
    return true;
}

static int count_leading(const Line *l, char c) {
    int n = 0;
    while ((size_t)n < l->len && l->data[n] == c) n++;
    return n;
}

static bool is_hr(const Line *l) {
    if (l->len < 3) return false;
    int dashes = 0, stars = 0, underscores = 0;
    for (size_t i = 0; i < l->len; i++) {
        char c = l->data[i];
        if (c == '-') dashes++;
        else if (c == '*') stars++;
        else if (c == '_') underscores++;
        else if (!isspace((unsigned char)c)) return false;
    }
    return dashes >= 3 || stars >= 3 || underscores >= 3;
}

static bool starts_with_ul(const Line *l) {
    if (l->len < 2) return false;
    size_t i = 0;
    while (i < l->len && l->data[i] == ' ') i++;
    if (i >= l->len) return false;
    return (l->data[i] == '-' || l->data[i] == '*' || l->data[i] == '+')
           && i + 1 < l->len && l->data[i + 1] == ' ';
}

static bool starts_with_ol(const Line *l) {
    if (l->len < 3) return false;
    size_t i = 0;
    while (i < l->len && l->data[i] == ' ') i++;
    if (i >= l->len || !isdigit((unsigned char)l->data[i])) return false;
    while (i < l->len && isdigit((unsigned char)l->data[i])) i++;
    return i + 1 < l->len && l->data[i] == '.' && l->data[i + 1] == ' ';
}

static size_t list_content_offset(const Line *l, bool ordered) {
    size_t i = 0;
    while (i < l->len && l->data[i] == ' ') i++;
    if (ordered) {
        while (i < l->len && isdigit((unsigned char)l->data[i])) i++;
        i++; /* '.' */
    } else {
        i++; /* marker */
    }
    if (i < l->len && l->data[i] == ' ') i++;
    return i;
}

static bool is_table_separator(const Line *l) {
    bool has_dash = false;
    for (size_t i = 0; i < l->len; i++) {
        char c = l->data[i];
        if (c == '-') has_dash = true;
        else if (c != '|' && c != ':' && c != ' ') return false;
    }
    return has_dash;
}

static bool is_blockquote(const Line *l) {
    size_t i = 0;
    while (i < l->len && l->data[i] == ' ') i++;
    return i < l->len && l->data[i] == '>';
}

/* ── Block-level parser ──────────────────────────────────────────────── */

char *markdown_to_html(const char *md) {
    if (!md) return NULL;

    Buf out;
    buf_init(&out);

    Line *lines = NULL;
    int nlines = split_lines(md, &lines);
    int i = 0;

    while (i < nlines) {
        Line *l = &lines[i];

        /* blank line */
        if (line_is_blank(l)) {
            i++;
            continue;
        }

        /* fenced code block ``` */
        if (l->len >= 3 && l->data[0] == '`' && l->data[1] == '`' && l->data[2] == '`') {
            /* extract optional language */
            const char *lang = l->data + 3;
            size_t lang_len = l->len - 3;
            while (lang_len > 0 && isspace((unsigned char)*lang)) { lang++; lang_len--; }
            /* trim trailing space from lang */
            while (lang_len > 0 && isspace((unsigned char)lang[lang_len - 1])) lang_len--;

            if (lang_len > 0) {
                buf_appends(&out, "<pre><code class=\"language-");
                buf_append(&out, lang, lang_len);
                buf_appends(&out, "\">");
            } else {
                buf_appends(&out, "<pre><code>");
            }
            i++;
            while (i < nlines) {
                if (lines[i].len >= 3 && lines[i].data[0] == '`' &&
                    lines[i].data[1] == '`' && lines[i].data[2] == '`') {
                    i++;
                    break;
                }
                for (size_t k = 0; k < lines[i].len; k++)
                    html_escape_to(&out, lines[i].data[k]);
                buf_appends(&out, "\n");
                i++;
            }
            buf_appends(&out, "</code></pre>\n");
            continue;
        }

        /* heading */
        {
            int hashes = count_leading(l, '#');
            if (hashes >= 1 && hashes <= 6 && (size_t)hashes < l->len && l->data[hashes] == ' ') {
                char tag[8];
                snprintf(tag, sizeof(tag), "h%d", hashes);
                buf_appends(&out, "<");
                buf_appends(&out, tag);
                buf_appends(&out, ">");
                const char *content = l->data + hashes + 1;
                size_t content_len = l->len - (size_t)hashes - 1;
                /* trim trailing # */
                while (content_len > 0 && content[content_len - 1] == '#') content_len--;
                while (content_len > 0 && content[content_len - 1] == ' ') content_len--;
                parse_inline(&out, content, content_len);
                buf_appends(&out, "</");
                buf_appends(&out, tag);
                buf_appends(&out, ">\n");
                i++;
                continue;
            }
        }

        /* horizontal rule */
        if (is_hr(l)) {
            buf_appends(&out, "<hr>\n");
            i++;
            continue;
        }

        /* blockquote */
        if (is_blockquote(l)) {
            buf_appends(&out, "<blockquote>\n");
            Buf bq;
            buf_init(&bq);
            while (i < nlines && (is_blockquote(&lines[i]) || !line_is_blank(&lines[i]))) {
                const char *s = lines[i].data;
                size_t slen   = lines[i].len;
                size_t off = 0;
                while (off < slen && s[off] == ' ') off++;
                if (off < slen && s[off] == '>') {
                    off++;
                    if (off < slen && s[off] == ' ') off++;
                }
                buf_append(&bq, s + off, slen - off);
                buf_appends(&bq, "\n");
                i++;
                if (i < nlines && line_is_blank(&lines[i])) break;
            }
            char *inner = markdown_to_html(bq.data);
            if (inner) {
                buf_appends(&out, inner);
                free(inner);
            }
            free(bq.data);
            buf_appends(&out, "</blockquote>\n");
            continue;
        }

        /* table */
        if (l->len > 0 && memchr(l->data, '|', l->len) &&
            i + 1 < nlines && is_table_separator(&lines[i + 1])) {
            buf_appends(&out, "<table>\n<thead>\n<tr>\n");
            /* header row */
            const char *p = l->data;
            size_t plen = l->len;
            if (plen > 0 && p[0] == '|') { p++; plen--; }
            if (plen > 0 && p[plen - 1] == '|') plen--;

            /* parse cells */
            const char *cell = p;
            for (size_t k = 0; k <= plen; k++) {
                if (k == plen || p[k] == '|') {
                    size_t clen = (size_t)(&p[k] - cell);
                    while (clen > 0 && isspace((unsigned char)*cell)) { cell++; clen--; }
                    while (clen > 0 && isspace((unsigned char)cell[clen - 1])) clen--;
                    buf_appends(&out, "<th>");
                    parse_inline(&out, cell, clen);
                    buf_appends(&out, "</th>\n");
                    cell = &p[k + 1];
                }
            }
            buf_appends(&out, "</tr>\n</thead>\n<tbody>\n");
            i += 2; /* skip header + separator */

            while (i < nlines && !line_is_blank(&lines[i]) &&
                   memchr(lines[i].data, '|', lines[i].len)) {
                buf_appends(&out, "<tr>\n");
                p = lines[i].data;
                plen = lines[i].len;
                if (plen > 0 && p[0] == '|') { p++; plen--; }
                if (plen > 0 && p[plen - 1] == '|') plen--;

                cell = p;
                for (size_t k = 0; k <= plen; k++) {
                    if (k == plen || p[k] == '|') {
                        size_t clen = (size_t)(&p[k] - cell);
                        while (clen > 0 && isspace((unsigned char)*cell)) { cell++; clen--; }
                        while (clen > 0 && isspace((unsigned char)cell[clen - 1])) clen--;
                        buf_appends(&out, "<td>");
                        parse_inline(&out, cell, clen);
                        buf_appends(&out, "</td>\n");
                        cell = &p[k + 1];
                    }
                }
                buf_appends(&out, "</tr>\n");
                i++;
            }
            buf_appends(&out, "</tbody>\n</table>\n");
            continue;
        }

        /* unordered list */
        if (starts_with_ul(l)) {
            buf_appends(&out, "<ul>\n");
            while (i < nlines && !line_is_blank(&lines[i]) && starts_with_ul(&lines[i])) {
                size_t off = list_content_offset(&lines[i], false);
                buf_appends(&out, "<li>");
                parse_inline(&out, lines[i].data + off, lines[i].len - off);
                buf_appends(&out, "</li>\n");
                i++;
            }
            buf_appends(&out, "</ul>\n");
            continue;
        }

        /* ordered list */
        if (starts_with_ol(l)) {
            buf_appends(&out, "<ol>\n");
            while (i < nlines && !line_is_blank(&lines[i]) && starts_with_ol(&lines[i])) {
                size_t off = list_content_offset(&lines[i], true);
                buf_appends(&out, "<li>");
                parse_inline(&out, lines[i].data + off, lines[i].len - off);
                buf_appends(&out, "</li>\n");
                i++;
            }
            buf_appends(&out, "</ol>\n");
            continue;
        }

        /* inline HTML block (starts with < and an alpha or !) */
        if (l->len > 0 && l->data[0] == '<' &&
            (l->len > 1 && (isalpha((unsigned char)l->data[1]) || l->data[1] == '!'))) {
            /* pass through until blank line */
            while (i < nlines && !line_is_blank(&lines[i])) {
                buf_append(&out, lines[i].data, lines[i].len);
                buf_appends(&out, "\n");
                i++;
            }
            continue;
        }

        /* paragraph (default) */
        {
            buf_appends(&out, "<p>");
            bool first = true;
            while (i < nlines && !line_is_blank(&lines[i])) {
                /* break if next block element detected */
                if (count_leading(&lines[i], '#') >= 1 && !first) break;
                if (is_hr(&lines[i]) && !first) break;
                if (starts_with_ul(&lines[i]) && !first) break;
                if (starts_with_ol(&lines[i]) && !first) break;
                if (lines[i].len >= 3 && lines[i].data[0] == '`' &&
                    lines[i].data[1] == '`' && lines[i].data[2] == '`' && !first) break;
                if (is_blockquote(&lines[i]) && !first) break;

                if (!first) buf_appends(&out, "\n");
                parse_inline(&out, lines[i].data, lines[i].len);
                first = false;
                i++;
            }
            buf_appends(&out, "</p>\n");
        }
    }

    free(lines);
    return out.data;
}
