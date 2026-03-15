#include "config.h"
#include "json.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

static void safe_copy(char *dst, size_t dst_sz, const char *src) {
    if (src)
        snprintf(dst, dst_sz, "%s", src);
    else
        dst[0] = '\0';
}

bool config_load(Config *cfg, const char *project_dir) {
    char path[MAX_PATH_LEN];
    path_join(path, sizeof(path), project_dir, "config.json");

    char *data = read_file(path);
    if (!data) {
        fprintf(stderr, "Error: cannot read %s\n", path);
        return false;
    }

    JsonValue *root = json_parse(data);
    free(data);
    if (!root) {
        fprintf(stderr, "Error: invalid JSON in %s\n", path);
        return false;
    }

    memset(cfg, 0, sizeof(Config));

    safe_copy(cfg->site_title, sizeof(cfg->site_title),
              json_string_value(json_object_get(root, "site_title")));
    safe_copy(cfg->author, sizeof(cfg->author),
              json_string_value(json_object_get(root, "author")));
    safe_copy(cfg->base_url, sizeof(cfg->base_url),
              json_string_value(json_object_get(root, "base_url")));
    safe_copy(cfg->theme, sizeof(cfg->theme),
              json_string_value(json_object_get(root, "theme")));
    safe_copy(cfg->output_dir, sizeof(cfg->output_dir),
              json_string_value(json_object_get(root, "output_dir")));

    JsonValue *pag = json_object_get(root, "pagination_size");
    cfg->pagination_size = pag ? (int)json_number_value(pag) : 10;

    JsonValue *rss = json_object_get(root, "enable_rss");
    cfg->enable_rss = rss ? json_bool_value(rss) : true;

    JsonValue *seo = json_object_get(root, "enable_seo");
    cfg->enable_seo = seo ? json_bool_value(seo) : false;

    JsonValue *sa = json_object_get(root, "enable_simple_analytics");
    cfg->enable_simple_analytics = sa ? json_bool_value(sa) : false;

    safe_copy(cfg->site_description, sizeof(cfg->site_description),
              json_string_value(json_object_get(root, "site_description")));

    /* defaults */
    if (cfg->theme[0] == '\0')
        snprintf(cfg->theme, sizeof(cfg->theme), "default");
    if (cfg->output_dir[0] == '\0')
        snprintf(cfg->output_dir, sizeof(cfg->output_dir), "public");
    if (cfg->pagination_size <= 0)
        cfg->pagination_size = 10;

    json_free(root);
    return true;
}

bool config_save(const Config *cfg, const char *project_dir) {
    JsonValue *root = json_new_object();
    json_object_set(root, "site_title",      json_new_string(cfg->site_title));
    json_object_set(root, "author",          json_new_string(cfg->author));
    json_object_set(root, "base_url",        json_new_string(cfg->base_url));
    json_object_set(root, "theme",           json_new_string(cfg->theme));
    json_object_set(root, "pagination_size", json_new_number(cfg->pagination_size));
    json_object_set(root, "output_dir",      json_new_string(cfg->output_dir));
    json_object_set(root, "enable_rss",              json_new_bool(cfg->enable_rss));
    json_object_set(root, "enable_seo",              json_new_bool(cfg->enable_seo));
    json_object_set(root, "enable_simple_analytics", json_new_bool(cfg->enable_simple_analytics));
    json_object_set(root, "site_description",        json_new_string(cfg->site_description));

    char *json_str = json_serialize(root);
    json_free(root);

    char path[MAX_PATH_LEN];
    path_join(path, sizeof(path), project_dir, "config.json");

    bool ok = write_file(path, json_str);
    free(json_str);
    return ok;
}

bool config_write_default(const char *project_dir, const char *site_title) {
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.site_title, sizeof(cfg.site_title), "%s", site_title);
    snprintf(cfg.author,     sizeof(cfg.author),     "Author");
    cfg.base_url[0] = '\0';
    snprintf(cfg.theme,      sizeof(cfg.theme),      "default");
    cfg.pagination_size = 10;
    snprintf(cfg.output_dir, sizeof(cfg.output_dir), "public");
    cfg.enable_rss = true;
    cfg.enable_seo = false;
    cfg.enable_simple_analytics = false;
    cfg.site_description[0] = '\0';
    return config_save(&cfg, project_dir);
}

bool config_set_theme(const char *project_dir, const char *theme_name) {
    Config cfg;
    if (!config_load(&cfg, project_dir)) return false;
    snprintf(cfg.theme, sizeof(cfg.theme), "%s", theme_name);
    return config_save(&cfg, project_dir);
}
