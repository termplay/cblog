#include "theme.h"
#include "json.h"
#include "utils.h"
#include "config.h"

#include <stdio.h>
#include <string.h>

bool theme_load(Theme *theme, const char *project_dir, const char *name) {
    if (!theme || !name) return false;
    memset(theme, 0, sizeof(Theme));
    snprintf(theme->name, sizeof(theme->name), "%s", name);

    /* Try project-local themes first */
    char theme_dir[MAX_PATH_LEN];
    snprintf(theme_dir, sizeof(theme_dir), "%s/themes/%s", project_dir, name);

    if (!dir_exists(theme_dir)) {
        /* Fallback to system-wide themes */
        snprintf(theme_dir, sizeof(theme_dir), "/usr/local/share/cblog/themes/%s", name);
        if (!dir_exists(theme_dir)) {
            fprintf(stderr, "Error: theme '%s' not found\n", name);
            return false;
        }
    }

    snprintf(theme->path, sizeof(theme->path), "%s", theme_dir);
    snprintf(theme->templates_dir, sizeof(theme->templates_dir), "%s/templates", theme_dir);
    snprintf(theme->assets_dir, sizeof(theme->assets_dir), "%s/assets", theme_dir);

    /* Load theme.json if present */
    char meta_path[MAX_PATH_LEN];
    snprintf(meta_path, sizeof(meta_path), "%s/theme.json", theme_dir);

    char *data = read_file(meta_path);
    if (data) {
        JsonValue *root = json_parse(data);
        free(data);
        if (root) {
            const char *v;
            if ((v = json_string_value(json_object_get(root, "version"))))
                snprintf(theme->version, sizeof(theme->version), "%s", v);
            if ((v = json_string_value(json_object_get(root, "author"))))
                snprintf(theme->author, sizeof(theme->author), "%s", v);
            if ((v = json_string_value(json_object_get(root, "description"))))
                snprintf(theme->description, sizeof(theme->description), "%s", v);
            json_free(root);
        }
    }

    return true;
}

char *theme_read_template(const Theme *theme, const char *filename) {
    if (!theme || !filename) return NULL;
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s", theme->templates_dir, filename);
    return read_file(path);
}

bool theme_copy_assets(const Theme *theme, const char *output_dir) {
    if (!theme || !output_dir) return false;
    if (!dir_exists(theme->assets_dir)) return true; /* no assets is OK */

    char dst[MAX_PATH_LEN];
    snprintf(dst, sizeof(dst), "%s/assets", output_dir);
    return copy_dir_recursive(theme->assets_dir, dst);
}

bool theme_set(const char *project_dir, const char *name) {
    if (!project_dir || !name) return false;

    /* Verify theme exists */
    Theme t;
    if (!theme_load(&t, project_dir, name)) {
        fprintf(stderr, "Error: theme '%s' does not exist\n", name);
        return false;
    }

    if (!config_set_theme(project_dir, name)) {
        fprintf(stderr, "Error: could not update config.json\n");
        return false;
    }

    printf("Theme set to '%s'\n", name);
    return true;
}
