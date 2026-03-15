#include "template.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Context management ──────────────────────────────────────────────── */

TmplCtx *tmpl_ctx_new(void) {
    TmplCtx *ctx = calloc(1, sizeof(TmplCtx));
    if (!ctx) return NULL;
    ctx->capacity = 16;
    ctx->vars = calloc((size_t)ctx->capacity, sizeof(*ctx->vars));
    ctx->count = 0;
    return ctx;
}

void tmpl_ctx_free(TmplCtx *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->count; i++) {
        free(ctx->vars[i].key);
        free(ctx->vars[i].value);
        if (ctx->vars[i].list) {
            for (int j = 0; j < ctx->vars[i].list->count; j++)
                tmpl_ctx_free(ctx->vars[i].list->items[j]);
            free(ctx->vars[i].list->items);
            free(ctx->vars[i].list);
        }
    }
    free(ctx->vars);
    free(ctx);
}

void tmpl_ctx_set(TmplCtx *ctx, const char *key, const char *value) {
    if (!ctx || !key) return;

    /* update existing */
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->vars[i].key, key) == 0) {
            free(ctx->vars[i].value);
            ctx->vars[i].value = value ? strdup(value) : NULL;
            return;
        }
    }

    /* grow if needed */
    if (ctx->count >= ctx->capacity) {
        ctx->capacity *= 2;
        ctx->vars = realloc(ctx->vars, sizeof(*ctx->vars) * (size_t)ctx->capacity);
    }
    int idx = ctx->count++;
    ctx->vars[idx].key   = strdup(key);
    ctx->vars[idx].value = value ? strdup(value) : NULL;
    ctx->vars[idx].list  = NULL;
}

TmplList *tmpl_ctx_set_list(TmplCtx *ctx, const char *key) {
    if (!ctx || !key) return NULL;

    TmplList *list = calloc(1, sizeof(TmplList));
    list->capacity = 16;
    list->items    = calloc((size_t)list->capacity, sizeof(TmplCtx *));
    list->count    = 0;

    /* update existing */
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->vars[i].key, key) == 0) {
            /* free old list if any */
            if (ctx->vars[i].list) {
                for (int j = 0; j < ctx->vars[i].list->count; j++)
                    tmpl_ctx_free(ctx->vars[i].list->items[j]);
                free(ctx->vars[i].list->items);
                free(ctx->vars[i].list);
            }
            ctx->vars[i].list = list;
            return list;
        }
    }

    if (ctx->count >= ctx->capacity) {
        ctx->capacity *= 2;
        ctx->vars = realloc(ctx->vars, sizeof(*ctx->vars) * (size_t)ctx->capacity);
    }
    int idx = ctx->count++;
    ctx->vars[idx].key   = strdup(key);
    ctx->vars[idx].value = NULL;
    ctx->vars[idx].list  = list;
    return list;
}

TmplCtx *tmpl_list_add(TmplList *list) {
    if (!list) return NULL;
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->items = realloc(list->items, sizeof(TmplCtx *) * (size_t)list->capacity);
    }
    TmplCtx *ctx = tmpl_ctx_new();
    list->items[list->count++] = ctx;
    return ctx;
}

/* ── Lookup a variable in context ────────────────────────────────────── */

static const char *ctx_lookup_str(const TmplCtx *ctx, const char *key) {
    if (!ctx || !key) return NULL;
    /* support dotted access: item.field */
    const char *dot = strchr(key, '.');
    if (dot) {
        char parent[256];
        size_t plen = (size_t)(dot - key);
        if (plen >= sizeof(parent)) plen = sizeof(parent) - 1;
        memcpy(parent, key, plen);
        parent[plen] = '\0';
        /* look up parent - if it's a single item, we can't traverse here */
        (void)parent;
    }
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->vars[i].key, key) == 0)
            return ctx->vars[i].value;
    }
    return NULL;
}

static TmplList *ctx_lookup_list(const TmplCtx *ctx, const char *key) {
    if (!ctx || !key) return NULL;
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->vars[i].key, key) == 0)
            return ctx->vars[i].list;
    }
    return NULL;
}

/* ── Dynamic buffer ──────────────────────────────────────────────────── */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} RBuf;

static void rb_init(RBuf *b) {
    b->cap  = 1024;
    b->data = malloc(b->cap);
    b->len  = 0;
    b->data[0] = '\0';
}

static void rb_append(RBuf *b, const char *s, size_t n) {
    while (b->len + n + 1 > b->cap) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void rb_appends(RBuf *b, const char *s) {
    rb_append(b, s, strlen(s));
}

/* ── Template renderer ───────────────────────────────────────────────── */

/*
 * Syntax:
 *   {{ variable }}            – interpolation (raw, no escaping for HTML content)
 *   {% for item in list %}    – loop
 *   {% endfor %}              – end loop
 *   {% if variable %}         – conditional (truthy = non-empty, non-NULL)
 *   {% else %}                – else branch
 *   {% endif %}               – end conditional
 *   {% include "file.html" %} – include (not resolved here, just noted)
 *   {{{ variable }}}          – raw interpolation (same as {{ }}, kept for compat)
 */

static void render_impl(RBuf *out, const char *tpl, size_t tpl_len, const TmplCtx *ctx);

static void render_impl(RBuf *out, const char *tpl, size_t tpl_len, const TmplCtx *ctx) {
    size_t i = 0;

    while (i < tpl_len) {
        /* raw interpolation {{{ }}} */
        if (i + 2 < tpl_len && tpl[i] == '{' && tpl[i+1] == '{' && tpl[i+2] == '{') {
            size_t start = i + 3;
            const char *end = strstr(tpl + start, "}}}");
            if (end && (size_t)(end - tpl) <= tpl_len) {
                char varname[256];
                size_t vlen = (size_t)(end - (tpl + start));
                if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
                memcpy(varname, tpl + start, vlen);
                varname[vlen] = '\0';
                /* trim */
                char *vn = varname;
                while (*vn == ' ') vn++;
                char *ve = vn + strlen(vn) - 1;
                while (ve > vn && *ve == ' ') *ve-- = '\0';

                const char *val = ctx_lookup_str(ctx, vn);
                if (val) rb_appends(out, val);
                i = (size_t)(end - tpl) + 3;
                continue;
            }
        }

        /* block tags {% ... %} */
        if (i + 1 < tpl_len && tpl[i] == '{' && tpl[i+1] == '%') {
            const char *tag_end = strstr(tpl + i + 2, "%}");
            if (tag_end && (size_t)(tag_end - tpl) <= tpl_len) {
                /* extract tag content */
                const char *tag_start = tpl + i + 2;
                size_t tag_len = (size_t)(tag_end - tag_start);
                char tag[1024];
                if (tag_len >= sizeof(tag)) tag_len = sizeof(tag) - 1;
                memcpy(tag, tag_start, tag_len);
                tag[tag_len] = '\0';

                /* trim */
                char *t = tag;
                while (*t == ' ') t++;
                char *te = t + strlen(t) - 1;
                while (te > t && *te == ' ') *te-- = '\0';

                /* for item in list */
                char item_name[128], list_name[128];
                if (sscanf(t, "for %127s in %127s", item_name, list_name) == 2) {
                    /* find matching endfor */
                    size_t body_start = (size_t)(tag_end - tpl) + 2;
                    /* skip newline after tag */
                    if (body_start < tpl_len && tpl[body_start] == '\n') body_start++;

                    int depth = 1;
                    size_t pos = body_start;
                    size_t body_end = tpl_len;
                    while (pos < tpl_len && depth > 0) {
                        if (pos + 1 < tpl_len && tpl[pos] == '{' && tpl[pos+1] == '%') {
                            const char *inner_end = strstr(tpl + pos + 2, "%}");
                            if (inner_end) {
                                char inner[256];
                                size_t ilen = (size_t)(inner_end - (tpl + pos + 2));
                                if (ilen >= sizeof(inner)) ilen = sizeof(inner) - 1;
                                memcpy(inner, tpl + pos + 2, ilen);
                                inner[ilen] = '\0';
                                char *it = inner;
                                while (*it == ' ') it++;
                                if (strncmp(it, "for ", 4) == 0) depth++;
                                else if (strncmp(it, "endfor", 6) == 0) {
                                    depth--;
                                    if (depth == 0) {
                                        body_end = pos;
                                        /* skip optional preceding newline */
                                        if (body_end > body_start && tpl[body_end - 1] == '\n')
                                            body_end--;
                                        pos = (size_t)(inner_end - tpl) + 2;
                                        if (pos < tpl_len && tpl[pos] == '\n') pos++;
                                        break;
                                    }
                                }
                                pos = (size_t)(inner_end - tpl) + 2;
                                continue;
                            }
                        }
                        pos++;
                    }

                    TmplList *list = ctx_lookup_list(ctx, list_name);
                    if (list) {
                        for (int li = 0; li < list->count; li++) {
                            /* create merged context: parent + item vars */
                            TmplCtx *merged = tmpl_ctx_new();
                            /* copy parent vars */
                            for (int ci = 0; ci < ctx->count; ci++) {
                                if (ctx->vars[ci].value)
                                    tmpl_ctx_set(merged, ctx->vars[ci].key, ctx->vars[ci].value);
                                if (ctx->vars[ci].list) {
                                    /* shallow ref - just set pointer */
                                    /* For simplicity, copy string vars from list items */
                                }
                            }
                            /* set item vars with prefix */
                            TmplCtx *item_ctx = list->items[li];
                            for (int ci = 0; ci < item_ctx->count; ci++) {
                                char full_key[256];
                                snprintf(full_key, sizeof(full_key), "%s.%s",
                                         item_name, item_ctx->vars[ci].key);
                                if (item_ctx->vars[ci].value)
                                    tmpl_ctx_set(merged, full_key, item_ctx->vars[ci].value);
                                /* also set without prefix for convenience */
                                if (item_ctx->vars[ci].value)
                                    tmpl_ctx_set(merged, item_ctx->vars[ci].key, item_ctx->vars[ci].value);
                                if (item_ctx->vars[ci].list) {
                                    /* copy list references */
                                    if (merged->count >= merged->capacity) {
                                        merged->capacity *= 2;
                                        merged->vars = realloc(merged->vars,
                                            sizeof(*merged->vars) * (size_t)merged->capacity);
                                    }
                                    int midx = merged->count++;
                                    merged->vars[midx].key = strdup(full_key);
                                    merged->vars[midx].value = NULL;
                                    merged->vars[midx].list = item_ctx->vars[ci].list;

                                    /* also without prefix */
                                    if (merged->count >= merged->capacity) {
                                        merged->capacity *= 2;
                                        merged->vars = realloc(merged->vars,
                                            sizeof(*merged->vars) * (size_t)merged->capacity);
                                    }
                                    int midx2 = merged->count++;
                                    merged->vars[midx2].key = strdup(item_ctx->vars[ci].key);
                                    merged->vars[midx2].value = NULL;
                                    merged->vars[midx2].list = item_ctx->vars[ci].list;
                                }
                            }
                            render_impl(out, tpl + body_start, body_end - body_start, merged);
                            /* don't free shared lists */
                            for (int ci = 0; ci < merged->count; ci++) {
                                /* null out shared list ptrs so they aren't freed */
                                merged->vars[ci].list = NULL;
                            }
                            tmpl_ctx_free(merged);
                        }
                    }
                    i = pos;
                    continue;
                }

                /* if variable */
                char cond_var[256];
                if (sscanf(t, "if %255s", cond_var) == 1) {
                    size_t body_start = (size_t)(tag_end - tpl) + 2;
                    if (body_start < tpl_len && tpl[body_start] == '\n') body_start++;

                    int depth = 1;
                    size_t pos = body_start;
                    size_t body_end = tpl_len;
                    size_t else_pos = tpl_len; /* position of else body, if any */
                    bool has_else = false;

                    while (pos < tpl_len && depth > 0) {
                        if (pos + 1 < tpl_len && tpl[pos] == '{' && tpl[pos+1] == '%') {
                            const char *inner_end = strstr(tpl + pos + 2, "%}");
                            if (inner_end) {
                                char inner[256];
                                size_t ilen = (size_t)(inner_end - (tpl + pos + 2));
                                if (ilen >= sizeof(inner)) ilen = sizeof(inner) - 1;
                                memcpy(inner, tpl + pos + 2, ilen);
                                inner[ilen] = '\0';
                                char *it = inner;
                                while (*it == ' ') it++;
                                if (strncmp(it, "if ", 3) == 0) depth++;
                                else if (strncmp(it, "else", 4) == 0 && depth == 1) {
                                    has_else = true;
                                    body_end = pos;
                                    if (body_end > body_start && tpl[body_end - 1] == '\n')
                                        body_end--;
                                    else_pos = (size_t)(inner_end - tpl) + 2;
                                    if (else_pos < tpl_len && tpl[else_pos] == '\n') else_pos++;
                                }
                                else if (strncmp(it, "endif", 5) == 0) {
                                    depth--;
                                    if (depth == 0) {
                                        if (!has_else) {
                                            body_end = pos;
                                            if (body_end > body_start && tpl[body_end - 1] == '\n')
                                                body_end--;
                                        }
                                        pos = (size_t)(inner_end - tpl) + 2;
                                        if (pos < tpl_len && tpl[pos] == '\n') pos++;
                                        break;
                                    }
                                }
                                pos = (size_t)(inner_end - tpl) + 2;
                                continue;
                            }
                        }
                        pos++;
                    }

                    /* evaluate condition: truthy if non-NULL and non-empty */
                    const char *cval = ctx_lookup_str(ctx, cond_var);
                    TmplList *clist = ctx_lookup_list(ctx, cond_var);
                    bool truthy = (cval && cval[0] != '\0') || (clist && clist->count > 0);

                    if (truthy) {
                        render_impl(out, tpl + body_start, body_end - body_start, ctx);
                    } else if (has_else) {
                        /* else_pos is start of else body, pos is after endif */
                        size_t else_body_end = pos;
                        /* backtrack to find the {% endif %} start */
                        /* Simplified: else body is from else_pos to before {% endif %} */
                        /* Search backward from pos for {% endif %} */
                        size_t search = else_pos;
                        size_t endif_start = pos;
                        while (search < pos) {
                            if (search + 1 < pos && tpl[search] == '{' && tpl[search+1] == '%') {
                                const char *ie = strstr(tpl + search + 2, "%}");
                                if (ie) {
                                    char inner2[64];
                                    size_t i2len = (size_t)(ie - (tpl + search + 2));
                                    if (i2len >= sizeof(inner2)) i2len = sizeof(inner2) - 1;
                                    memcpy(inner2, tpl + search + 2, i2len);
                                    inner2[i2len] = '\0';
                                    char *it2 = inner2;
                                    while (*it2 == ' ') it2++;
                                    if (strncmp(it2, "endif", 5) == 0) {
                                        endif_start = search;
                                        if (endif_start > else_pos && tpl[endif_start - 1] == '\n')
                                            endif_start--;
                                        break;
                                    }
                                    search = (size_t)(ie - tpl) + 2;
                                    continue;
                                }
                            }
                            search++;
                        }
                        else_body_end = endif_start;
                        render_impl(out, tpl + else_pos, else_body_end - else_pos, ctx);
                    }
                    i = pos;
                    continue;
                }

                /* include "file" — not handled in render, caller should pre-process */
                /* skip unknown tags */
                i = (size_t)(tag_end - tpl) + 2;
                if (i < tpl_len && tpl[i] == '\n') i++;
                continue;
            }
        }

        /* variable interpolation {{ var }} */
        if (i + 1 < tpl_len && tpl[i] == '{' && tpl[i+1] == '{') {
            const char *end = strstr(tpl + i + 2, "}}");
            if (end && (size_t)(end - tpl) <= tpl_len) {
                char varname[256];
                size_t vlen = (size_t)(end - (tpl + i + 2));
                if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
                memcpy(varname, tpl + i + 2, vlen);
                varname[vlen] = '\0';
                /* trim */
                char *vn = varname;
                while (*vn == ' ') vn++;
                char *ve = vn + strlen(vn) - 1;
                while (ve > vn && *ve == ' ') *ve-- = '\0';

                const char *val = ctx_lookup_str(ctx, vn);
                if (val) rb_appends(out, val);
                i = (size_t)(end - tpl) + 2;
                continue;
            }
        }

        /* plain character */
        rb_append(out, &tpl[i], 1);
        i++;
    }
}

char *tmpl_render(const char *tpl, const TmplCtx *ctx) {
    if (!tpl) return NULL;
    RBuf out;
    rb_init(&out);
    render_impl(&out, tpl, strlen(tpl), ctx);
    return out.data;
}

char *tmpl_render_file(const char *path, const TmplCtx *ctx) {
    char *tpl = read_file(path);
    if (!tpl) {
        fprintf(stderr, "Error: cannot read template %s\n", path);
        return NULL;
    }
    char *result = tmpl_render(tpl, ctx);
    free(tpl);
    return result;
}
