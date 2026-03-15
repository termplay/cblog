#ifndef CBLOG_CONFIG_H
#define CBLOG_CONFIG_H

#include "cblog.h"

/* Load config from config.json in project root. Returns true on success. */
bool config_load(Config *cfg, const char *project_dir);

/* Save config to config.json. Returns true on success. */
bool config_save(const Config *cfg, const char *project_dir);

/* Write a default config.json. */
bool config_write_default(const char *project_dir, const char *site_title);

/* Set a single config key (used by theme set, etc.) */
bool config_set_theme(const char *project_dir, const char *theme_name);

#endif /* CBLOG_CONFIG_H */
