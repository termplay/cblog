#ifndef CBLOG_TEMPLATE_H
#define CBLOG_TEMPLATE_H

#include "cblog.h"

/*
 * Template context: a key-value store used for variable interpolation.
 * Values can be simple strings or lists of sub-contexts (for loops).
 */

typedef struct TmplCtx TmplCtx;
typedef struct TmplList TmplList;

struct TmplList {
    TmplCtx **items;
    int       count;
    int       capacity;
};

struct TmplCtx {
    struct {
        char *key;
        char *value;        /* simple string value, NULL if list */
        TmplList *list;     /* list value, NULL if simple string */
    } *vars;
    int count;
    int capacity;
};

/* Create / destroy */
TmplCtx  *tmpl_ctx_new(void);
void      tmpl_ctx_free(TmplCtx *ctx);

/* Set a string variable */
void      tmpl_ctx_set(TmplCtx *ctx, const char *key, const char *value);

/* Set a list variable (for loops) */
TmplList *tmpl_ctx_set_list(TmplCtx *ctx, const char *key);

/* Add an item to a list, returns the new sub-context */
TmplCtx  *tmpl_list_add(TmplList *list);

/* Render a template string with the given context. Caller frees result. */
char     *tmpl_render(const char *tpl, const TmplCtx *ctx);

/* Load a template file and render it. Caller frees result. */
char     *tmpl_render_file(const char *path, const TmplCtx *ctx);

#endif /* CBLOG_TEMPLATE_H */
