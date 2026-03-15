#ifndef CBLOG_THEME_H
#define CBLOG_THEME_H

#include "cblog.h"

typedef struct {
    char name[MAX_TITLE_LEN];
    char version[64];
    char author[MAX_AUTHOR_LEN];
    char description[512];
    char path[MAX_PATH_LEN];           /* absolute path to theme dir */
    char templates_dir[MAX_PATH_LEN];  /* path to templates/ */
    char assets_dir[MAX_PATH_LEN];     /* path to assets/ */
} Theme;

/* Load a theme by name from project_dir/themes/<name>.
 * Falls back to /usr/local/share/cblog/themes/<name>. */
bool theme_load(Theme *theme, const char *project_dir, const char *name);

/* Read a template file from the theme. Caller frees. */
char *theme_read_template(const Theme *theme, const char *filename);

/* Copy theme assets into the output directory. */
bool theme_copy_assets(const Theme *theme, const char *output_dir);

/* Set the active theme in config.json. */
bool theme_set(const char *project_dir, const char *name);

#endif /* CBLOG_THEME_H */
