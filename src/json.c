#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Internal parser state ───────────────────────────────────────────── */

typedef struct {
    const char *src;
    int         pos;
} Parser;

static void       skip_ws(Parser *p);
static JsonValue *parse_value(Parser *p);
static JsonValue *parse_string_val(Parser *p);
static JsonValue *parse_number_val(Parser *p);
static JsonValue *parse_object(Parser *p);
static JsonValue *parse_array(Parser *p);
static JsonValue *parse_literal(Parser *p);
static char      *parse_string_raw(Parser *p);

/* ── Helpers ─────────────────────────────────────────────────────────── */

static JsonValue *jv_alloc(JsonType t) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    if (v) v->type = t;
    return v;
}

static void skip_ws(Parser *p) {
    while (p->src[p->pos] && strchr(" \t\r\n", p->src[p->pos]))
        p->pos++;
}

static char next(Parser *p) {
    return p->src[p->pos];
}

static char consume(Parser *p) {
    return p->src[p->pos++];
}

/* ── String parsing (handles basic escapes) ──────────────────────────── */

static char *parse_string_raw(Parser *p) {
    if (next(p) != '"') return NULL;
    consume(p); /* opening quote */

    size_t cap = 128, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    while (next(p) && next(p) != '"') {
        char c = consume(p);
        if (c == '\\') {
            c = consume(p);
            switch (c) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'u':
                    /* skip 4 hex digits (simplified) */
                    for (int i = 0; i < 4 && next(p); i++) consume(p);
                    c = '?';
                    break;
                default: break;
            }
        }
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
        }
        buf[len++] = c;
    }
    if (next(p) == '"') consume(p); /* closing quote */
    buf[len] = '\0';
    return buf;
}

/* ── Value parsers ───────────────────────────────────────────────────── */

static JsonValue *parse_string_val(Parser *p) {
    char *s = parse_string_raw(p);
    if (!s) return NULL;
    JsonValue *v = jv_alloc(JSON_STRING);
    if (!v) { free(s); return NULL; }
    v->u.string = s;
    return v;
}

static JsonValue *parse_number_val(Parser *p) {
    const char *start = p->src + p->pos;
    char *end = NULL;
    double d = strtod(start, &end);
    if (end == start) return NULL;
    p->pos += (int)(end - start);
    JsonValue *v = jv_alloc(JSON_NUMBER);
    if (v) v->u.number = d;
    return v;
}

static JsonValue *parse_object(Parser *p) {
    consume(p); /* '{' */
    skip_ws(p);

    JsonValue *obj = jv_alloc(JSON_OBJECT);
    if (!obj) return NULL;

    int cap = 8;
    obj->u.object.pairs = malloc(sizeof(JsonPair) * (size_t)cap);
    obj->u.object.count = 0;

    if (next(p) == '}') { consume(p); return obj; }

    for (;;) {
        skip_ws(p);
        char *key = parse_string_raw(p);
        if (!key) break;

        skip_ws(p);
        if (next(p) == ':') consume(p);
        skip_ws(p);

        JsonValue *val = parse_value(p);

        if (obj->u.object.count >= cap) {
            cap *= 2;
            obj->u.object.pairs = realloc(obj->u.object.pairs,
                                           sizeof(JsonPair) * (size_t)cap);
        }
        int idx = obj->u.object.count++;
        obj->u.object.pairs[idx].key   = key;
        obj->u.object.pairs[idx].value = val;

        skip_ws(p);
        if (next(p) == ',') { consume(p); continue; }
        break;
    }
    skip_ws(p);
    if (next(p) == '}') consume(p);
    return obj;
}

static JsonValue *parse_array(Parser *p) {
    consume(p); /* '[' */
    skip_ws(p);

    JsonValue *arr = jv_alloc(JSON_ARRAY);
    if (!arr) return NULL;

    int cap = 8;
    arr->u.array.items = malloc(sizeof(JsonValue *) * (size_t)cap);
    arr->u.array.count = 0;

    if (next(p) == ']') { consume(p); return arr; }

    for (;;) {
        skip_ws(p);
        JsonValue *val = parse_value(p);
        if (!val) break;

        if (arr->u.array.count >= cap) {
            cap *= 2;
            arr->u.array.items = realloc(arr->u.array.items,
                                          sizeof(JsonValue *) * (size_t)cap);
        }
        arr->u.array.items[arr->u.array.count++] = val;

        skip_ws(p);
        if (next(p) == ',') { consume(p); continue; }
        break;
    }
    skip_ws(p);
    if (next(p) == ']') consume(p);
    return arr;
}

static JsonValue *parse_literal(Parser *p) {
    if (strncmp(p->src + p->pos, "true", 4) == 0) {
        p->pos += 4;
        JsonValue *v = jv_alloc(JSON_BOOL);
        if (v) v->u.boolean = true;
        return v;
    }
    if (strncmp(p->src + p->pos, "false", 5) == 0) {
        p->pos += 5;
        JsonValue *v = jv_alloc(JSON_BOOL);
        if (v) v->u.boolean = false;
        return v;
    }
    if (strncmp(p->src + p->pos, "null", 4) == 0) {
        p->pos += 4;
        return jv_alloc(JSON_NULL);
    }
    return NULL;
}

static JsonValue *parse_value(Parser *p) {
    skip_ws(p);
    char c = next(p);
    if (c == '"')                           return parse_string_val(p);
    if (c == '{')                           return parse_object(p);
    if (c == '[')                           return parse_array(p);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number_val(p);
    return parse_literal(p);
}

/* ── Public API ──────────────────────────────────────────────────────── */

JsonValue *json_parse(const char *input) {
    if (!input) return NULL;
    Parser p = { .src = input, .pos = 0 };
    return parse_value(&p);
}

void json_free(JsonValue *v) {
    if (!v) return;
    switch (v->type) {
        case JSON_STRING:
            free(v->u.string);
            break;
        case JSON_ARRAY:
            for (int i = 0; i < v->u.array.count; i++)
                json_free(v->u.array.items[i]);
            free(v->u.array.items);
            break;
        case JSON_OBJECT:
            for (int i = 0; i < v->u.object.count; i++) {
                free(v->u.object.pairs[i].key);
                json_free(v->u.object.pairs[i].value);
            }
            free(v->u.object.pairs);
            break;
        default:
            break;
    }
    free(v);
}

JsonValue *json_object_get(const JsonValue *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    for (int i = 0; i < obj->u.object.count; i++) {
        if (strcmp(obj->u.object.pairs[i].key, key) == 0)
            return obj->u.object.pairs[i].value;
    }
    return NULL;
}

const char *json_string_value(const JsonValue *v) {
    if (!v || v->type != JSON_STRING) return NULL;
    return v->u.string;
}

double json_number_value(const JsonValue *v) {
    if (!v || v->type != JSON_NUMBER) return 0.0;
    return v->u.number;
}

bool json_bool_value(const JsonValue *v) {
    if (!v || v->type != JSON_BOOL) return false;
    return v->u.boolean;
}

/* ── Builder helpers ─────────────────────────────────────────────────── */

JsonValue *json_new_string(const char *s) {
    JsonValue *v = jv_alloc(JSON_STRING);
    if (v) v->u.string = strdup(s ? s : "");
    return v;
}

JsonValue *json_new_bool(bool b) {
    JsonValue *v = jv_alloc(JSON_BOOL);
    if (v) v->u.boolean = b;
    return v;
}

JsonValue *json_new_number(double n) {
    JsonValue *v = jv_alloc(JSON_NUMBER);
    if (v) v->u.number = n;
    return v;
}

JsonValue *json_new_object(void) {
    JsonValue *v = jv_alloc(JSON_OBJECT);
    if (v) {
        v->u.object.pairs = malloc(sizeof(JsonPair) * 8);
        v->u.object.count = 0;
    }
    return v;
}

void json_object_set(JsonValue *obj, const char *key, JsonValue *val) {
    if (!obj || obj->type != JSON_OBJECT || !key) return;

    /* update existing */
    for (int i = 0; i < obj->u.object.count; i++) {
        if (strcmp(obj->u.object.pairs[i].key, key) == 0) {
            json_free(obj->u.object.pairs[i].value);
            obj->u.object.pairs[i].value = val;
            return;
        }
    }

    /* Grow capacity if needed (initial alloc is 8, grow by doubling) */
    int count = obj->u.object.count;
    if (count > 0 && (count & (count - 1)) == 0 && count >= 8) {
        obj->u.object.pairs = realloc(obj->u.object.pairs,
                                       sizeof(JsonPair) * (size_t)(count * 2));
    }
    int idx = obj->u.object.count++;
    obj->u.object.pairs[idx].key   = strdup(key);
    obj->u.object.pairs[idx].value = val;
}

/* ── Serializer ──────────────────────────────────────────────────────── */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
    int    indent;
} SerCtx;

static void ser_grow(SerCtx *s, size_t need) {
    while (s->len + need >= s->cap) {
        s->cap *= 2;
        s->buf = realloc(s->buf, s->cap);
    }
}

static void ser_append(SerCtx *s, const char *str) {
    size_t n = strlen(str);
    ser_grow(s, n + 1);
    memcpy(s->buf + s->len, str, n);
    s->len += n;
    s->buf[s->len] = '\0';
}

static void ser_indent(SerCtx *s) {
    for (int i = 0; i < s->indent; i++)
        ser_append(s, "    ");
}

static void ser_string(SerCtx *s, const char *str) {
    ser_append(s, "\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  ser_append(s, "\\\""); break;
            case '\\': ser_append(s, "\\\\"); break;
            case '\n': ser_append(s, "\\n");  break;
            case '\r': ser_append(s, "\\r");  break;
            case '\t': ser_append(s, "\\t");  break;
            default: {
                char c[2] = { *p, '\0' };
                ser_append(s, c);
            }
        }
    }
    ser_append(s, "\"");
}

static void ser_value(SerCtx *s, const JsonValue *v) {
    if (!v) { ser_append(s, "null"); return; }

    switch (v->type) {
        case JSON_NULL:
            ser_append(s, "null");
            break;
        case JSON_BOOL:
            ser_append(s, v->u.boolean ? "true" : "false");
            break;
        case JSON_NUMBER: {
            char num[64];
            double d = v->u.number;
            if (d == (int)d)
                snprintf(num, sizeof(num), "%d", (int)d);
            else
                snprintf(num, sizeof(num), "%g", d);
            ser_append(s, num);
            break;
        }
        case JSON_STRING:
            ser_string(s, v->u.string);
            break;
        case JSON_ARRAY:
            ser_append(s, "[\n");
            s->indent++;
            for (int i = 0; i < v->u.array.count; i++) {
                ser_indent(s);
                ser_value(s, v->u.array.items[i]);
                if (i + 1 < v->u.array.count) ser_append(s, ",");
                ser_append(s, "\n");
            }
            s->indent--;
            ser_indent(s);
            ser_append(s, "]");
            break;
        case JSON_OBJECT:
            ser_append(s, "{\n");
            s->indent++;
            for (int i = 0; i < v->u.object.count; i++) {
                ser_indent(s);
                ser_string(s, v->u.object.pairs[i].key);
                ser_append(s, ": ");
                ser_value(s, v->u.object.pairs[i].value);
                if (i + 1 < v->u.object.count) ser_append(s, ",");
                ser_append(s, "\n");
            }
            s->indent--;
            ser_indent(s);
            ser_append(s, "}");
            break;
    }
}

char *json_serialize(const JsonValue *v) {
    SerCtx s = { .buf = malloc(256), .len = 0, .cap = 256, .indent = 0 };
    s.buf[0] = '\0';
    ser_value(&s, v);
    ser_append(&s, "\n");
    return s.buf;
}
