#ifndef CBLOG_H
#define CBLOG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define CBLOG_VERSION "1.0.0"
#define MAX_PATH_LEN 1024
#define MAX_TITLE_LEN 256
#define MAX_SLUG_LEN 256
#define MAX_DATE_LEN 64
#define MAX_TAG_LEN 64
#define MAX_TAGS 32
#define MAX_URL_LEN 512
#define MAX_AUTHOR_LEN 128
#define MAX_POSTS 1024
#define MAX_LINE_LEN 4096
#define MAX_CONTENT_LEN (1024 * 1024)

/* Front matter for a blog post */
typedef struct {
    char title[MAX_TITLE_LEN];
    char date[MAX_DATE_LEN];
    char slug[MAX_SLUG_LEN];
    char tags[MAX_TAGS][MAX_TAG_LEN];
    int  tag_count;
    bool draft;
} FrontMatter;

/* A single blog post */
typedef struct {
    FrontMatter meta;
    char *content_md;   /* raw markdown */
    char *content_html; /* rendered html */
    char filepath[MAX_PATH_LEN];
} Post;

/* Site configuration from config.json */
typedef struct {
    char site_title[MAX_TITLE_LEN];
    char author[MAX_AUTHOR_LEN];
    char base_url[MAX_URL_LEN];
    char theme[MAX_TITLE_LEN];
    int  pagination_size;
    char output_dir[MAX_PATH_LEN];
    bool enable_rss;
    bool enable_seo;
    char site_description[MAX_LINE_LEN];
} Config;

/* Tag index entry */
typedef struct {
    char name[MAX_TAG_LEN];
    int  post_indices[MAX_POSTS];
    int  count;
} TagIndex;

#endif /* CBLOG_H */
