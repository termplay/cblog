#ifndef CBLOG_UTILS_H
#define CBLOG_UTILS_H

#include <stdbool.h>
#include <stddef.h>

/* File I/O */
char *read_file(const char *path);
bool  write_file(const char *path, const char *content);

/* Directory operations */
bool  mkdir_p(const char *path);
bool  copy_file(const char *src, const char *dst);
bool  copy_dir_recursive(const char *src, const char *dst);
bool  dir_exists(const char *path);
bool  file_exists(const char *path);
bool  rm_rf_dir(const char *path);
bool  remove_file(const char *path);

/* String utilities */
char *slugify(const char *input);
char *str_replace(const char *haystack, const char *needle, const char *replacement);
char *str_trim(char *s);
char *str_dup(const char *s);
void  str_to_lower(char *s);

/* Date/time */
void  get_current_date(char *buf, size_t len);       /* YYYY-MM-DD */
void  get_rfc822_date(const char *iso, char *buf, size_t len);

/* HTML escaping */
char *html_escape(const char *input);

/* Path manipulation */
void  path_join(char *out, size_t out_len, const char *a, const char *b);
const char *path_basename(const char *path);
const char *path_extension(const char *path);

#endif /* CBLOG_UTILS_H */
