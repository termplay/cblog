#include "builder.h"
#include "config.h"
#include "markdown.h"
#include "template.h"
#include "theme.h"
#include "rss.h"
#include "utils.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <strings.h>

/* ── Front matter parser ─────────────────────────────────────────────── */

static bool parse_front_matter(const char *content, FrontMatter *fm, const char **body) {
    memset(fm, 0, sizeof(FrontMatter));
    *body = content;

    if (strncmp(content, "---", 3) != 0)
        return false;

    const char *start = content + 3;
    while (*start == '\r' || *start == '\n') start++;

    const char *end = strstr(start, "\n---");
    if (!end) return false;

    /* Parse YAML-like key: value pairs */
    const char *p = start;
    while (p < end) {
        const char *eol = strchr(p, '\n');
        if (!eol || eol > end) eol = end;

        char line[MAX_LINE_LEN];
        size_t llen = (size_t)(eol - p);
        if (llen >= sizeof(line)) llen = sizeof(line) - 1;
        memcpy(line, p, llen);
        line[llen] = '\0';

        /* trim \r */
        if (llen > 0 && line[llen - 1] == '\r')
            line[--llen] = '\0';

        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = str_trim(line);
            char *val = str_trim(colon + 1);

            /* strip surrounding quotes */
            size_t vlen = strlen(val);
            if (vlen >= 2 && ((val[0] == '"' && val[vlen-1] == '"') ||
                              (val[0] == '\'' && val[vlen-1] == '\''))) {
                val[vlen - 1] = '\0';
                val++;
            }

            if (strcmp(key, "title") == 0) {
                snprintf(fm->title, sizeof(fm->title), "%s", val);
            } else if (strcmp(key, "date") == 0) {
                snprintf(fm->date, sizeof(fm->date), "%s", val);
            } else if (strcmp(key, "slug") == 0) {
                snprintf(fm->slug, sizeof(fm->slug), "%s", val);
            } else if (strcmp(key, "draft") == 0) {
                fm->draft = (strcmp(val, "true") == 0 || strcmp(val, "yes") == 0);
            } else if (strcmp(key, "tags") == 0) {
                /* Parse tags: [tag1, tag2] or tag1, tag2 */
                char *tv = val;
                if (*tv == '[') tv++;
                char *closing = strchr(tv, ']');
                if (closing) *closing = '\0';

                char *tok = strtok(tv, ",");
                while (tok && fm->tag_count < MAX_TAGS) {
                    char *trimmed = str_trim(tok);
                    /* strip quotes from individual tags */
                    size_t tlen = strlen(trimmed);
                    if (tlen >= 2 && ((trimmed[0] == '"' && trimmed[tlen-1] == '"') ||
                                      (trimmed[0] == '\'' && trimmed[tlen-1] == '\''))) {
                        trimmed[tlen - 1] = '\0';
                        trimmed++;
                    }
                    snprintf(fm->tags[fm->tag_count], sizeof(fm->tags[0]), "%s", trimmed);
                    fm->tag_count++;
                    tok = strtok(NULL, ",");
                }
            }
        }

        p = eol + 1;
    }

    /* Generate slug from title if not set */
    if (fm->slug[0] == '\0' && fm->title[0] != '\0') {
        char *s = slugify(fm->title);
        if (s) {
            snprintf(fm->slug, sizeof(fm->slug), "%s", s);
            free(s);
        }
    }

    /* Advance body past closing --- */
    *body = end + 4; /* \n--- */
    while (**body == '\r' || **body == '\n') (*body)++;

    return true;
}

/* ── Post loading ────────────────────────────────────────────────────── */

static int compare_posts_by_date(const void *a, const void *b) {
    const Post *pa = (const Post *)a;
    const Post *pb = (const Post *)b;
    return strcmp(pb->meta.date, pa->meta.date); /* newest first */
}

static int load_posts(const char *content_dir, Post *posts, int max_posts) {
    DIR *dir = opendir(content_dir);
    if (!dir) {
        fprintf(stderr, "Error: cannot open %s\n", content_dir);
        return 0;
    }

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && count < max_posts) {
        const char *ext = path_extension(ent->d_name);
        if (strcmp(ext, ".md") != 0 && strcmp(ext, ".markdown") != 0)
            continue;

        char path[MAX_PATH_LEN];
        path_join(path, sizeof(path), content_dir, ent->d_name);

        char *raw = read_file(path);
        if (!raw) continue;

        Post *p = &posts[count];
        memset(p, 0, sizeof(Post));
        snprintf(p->filepath, sizeof(p->filepath), "%s", path);

        const char *body = raw;
        parse_front_matter(raw, &p->meta, &body);

        p->content_md = strdup(body);
        p->content_html = markdown_to_html(body);
        free(raw);

        if (p->meta.title[0] == '\0') {
            /* use filename as title */
            char name[256];
            snprintf(name, sizeof(name), "%s", ent->d_name);
            char *dot = strrchr(name, '.');
            if (dot) *dot = '\0';
            snprintf(p->meta.title, sizeof(p->meta.title), "%s", name);
        }

        if (p->meta.slug[0] == '\0') {
            char *s = slugify(p->meta.title);
            if (s) {
                snprintf(p->meta.slug, sizeof(p->meta.slug), "%s", s);
                free(s);
            }
        }

        count++;
    }
    closedir(dir);

    /* Sort by date descending */
    qsort(posts, (size_t)count, sizeof(Post), compare_posts_by_date);

    return count;
}

static void free_posts(Post *posts, int count) {
    for (int i = 0; i < count; i++) {
        free(posts[i].content_md);
        free(posts[i].content_html);
    }
}

/* ── Tag index builder ───────────────────────────────────────────────── */

static int build_tag_index(Post *posts, int post_count, TagIndex *tags, int max_tags) {
    int tag_count = 0;
    for (int i = 0; i < post_count; i++) {
        if (posts[i].meta.draft) continue;
        for (int t = 0; t < posts[i].meta.tag_count; t++) {
            const char *tag_name = posts[i].meta.tags[t];
            /* find or create tag */
            int found = -1;
            for (int j = 0; j < tag_count; j++) {
                if (strcasecmp(tags[j].name, tag_name) == 0) {
                    found = j;
                    break;
                }
            }
            if (found < 0 && tag_count < max_tags) {
                found = tag_count++;
                memset(&tags[found], 0, sizeof(TagIndex));
                snprintf(tags[found].name, sizeof(tags[found].name), "%s", tag_name);
            }
            if (found >= 0 && tags[found].count < MAX_POSTS) {
                tags[found].post_indices[tags[found].count++] = i;
            }
        }
    }
    return tag_count;
}

/* ── Wrap content in layout ──────────────────────────────────────────── */

static char *wrap_in_layout(const Theme *theme, const Config *cfg,
                            const char *title, const char *content,
                            const char *description, const char *page_url) {
    char *layout_tpl = theme_read_template(theme, "layout.html");
    if (!layout_tpl) {
        /* no layout, return content as-is */
        return strdup(content);
    }

    TmplCtx *ctx = tmpl_ctx_new();
    tmpl_ctx_set(ctx, "site_title", cfg->site_title);
    tmpl_ctx_set(ctx, "author", cfg->author);
    tmpl_ctx_set(ctx, "base_url", cfg->base_url);
    tmpl_ctx_set(ctx, "page_title", title);
    tmpl_ctx_set(ctx, "content", content);
    tmpl_ctx_set(ctx, "year", "2026");

    /* SEO meta tags */
    if (cfg->enable_seo) {
        tmpl_ctx_set(ctx, "enable_seo", "1");
        const char *desc = (description && description[0])
            ? description
            : (cfg->site_description[0] ? cfg->site_description : cfg->site_title);
        tmpl_ctx_set(ctx, "meta_description", desc);
        tmpl_ctx_set(ctx, "og_title", title);
        tmpl_ctx_set(ctx, "og_description", desc);
        tmpl_ctx_set(ctx, "og_type", "article");
        if (page_url && page_url[0])
            tmpl_ctx_set(ctx, "og_url", page_url);
        else
            tmpl_ctx_set(ctx, "og_url", cfg->base_url);
        tmpl_ctx_set(ctx, "twitter_card", "summary");
        tmpl_ctx_set(ctx, "twitter_title", title);
        tmpl_ctx_set(ctx, "twitter_description", desc);
    }

    char *result = tmpl_render(layout_tpl, ctx);
    tmpl_ctx_free(ctx);
    free(layout_tpl);
    return result;
}

/* ── Build commands ──────────────────────────────────────────────────── */

int builder_build(const char *project_dir) {
    Config cfg;
    if (!config_load(&cfg, project_dir)) {
        fprintf(stderr, "Error: cannot load config.json. Are you in a cblog project?\n");
        return 1;
    }

    /* Load theme */
    Theme theme;
    if (!theme_load(&theme, project_dir, cfg.theme)) {
        fprintf(stderr, "Error: cannot load theme '%s'\n", cfg.theme);
        return 1;
    }

    /* Prepare output directory */
    char output_dir[MAX_PATH_LEN];
    path_join(output_dir, sizeof(output_dir), project_dir, cfg.output_dir);

    /* Smart-clean output dir: remove only build artifacts, preserve .git, CNAME, etc. */
    mkdir_p(output_dir);
    {
        /* Subdirectories managed by the build — remove and recreate */
        static const char *build_dirs[] = { "posts", "tags", "pages", "assets", NULL };
        for (int i = 0; build_dirs[i]; i++) {
            char sub[MAX_PATH_LEN];
            path_join(sub, sizeof(sub), output_dir, build_dirs[i]);
            if (dir_exists(sub))
                rm_rf_dir(sub);
        }

        /* Individual build files in the output root */
        static const char *build_files[] = {
            "index.html", "archive.html", "tags.html", "rss.xml", ".nojekyll", NULL
        };
        for (int i = 0; build_files[i]; i++) {
            char fp[MAX_PATH_LEN];
            path_join(fp, sizeof(fp), output_dir, build_files[i]);
            if (file_exists(fp))
                remove_file(fp);
        }
    }

    /* Create posts output dir */
    char posts_dir[MAX_PATH_LEN];
    path_join(posts_dir, sizeof(posts_dir), output_dir, "posts");
    mkdir_p(posts_dir);

    /* Create tags output dir */
    char tags_dir[MAX_PATH_LEN];
    path_join(tags_dir, sizeof(tags_dir), output_dir, "tags");
    mkdir_p(tags_dir);

    /* Create pages output dir */
    char pages_out_dir[MAX_PATH_LEN];
    path_join(pages_out_dir, sizeof(pages_out_dir), output_dir, "pages");
    mkdir_p(pages_out_dir);

    /* Load posts */
    char content_dir[MAX_PATH_LEN];
    path_join(content_dir, sizeof(content_dir), project_dir, "content");

    Post *posts = calloc(MAX_POSTS, sizeof(Post));
    if (!posts) {
        fprintf(stderr, "Error: out of memory\n");
        return 1;
    }
    int post_count = load_posts(content_dir, posts, MAX_POSTS);
    printf("Found %d post(s)\n", post_count);

    /* Build tag index */
    TagIndex *tags = calloc(256, sizeof(TagIndex));
    int tag_count = build_tag_index(posts, post_count, tags, 256);

    /* ── Generate individual post pages ─────────────────────────────── */
    char *post_tpl = theme_read_template(&theme, "post.html");
    if (!post_tpl) {
        fprintf(stderr, "Warning: no post.html template found\n");
        post_tpl = strdup("{{ content }}");
    }

    for (int i = 0; i < post_count; i++) {
        Post *p = &posts[i];
        if (p->meta.draft) {
            printf("  Skipping draft: %s\n", p->meta.title);
            continue;
        }

        TmplCtx *ctx = tmpl_ctx_new();
        tmpl_ctx_set(ctx, "title", p->meta.title);
        tmpl_ctx_set(ctx, "date", p->meta.date);
        tmpl_ctx_set(ctx, "slug", p->meta.slug);
        tmpl_ctx_set(ctx, "content", p->content_html);
        tmpl_ctx_set(ctx, "author", cfg.author);
        tmpl_ctx_set(ctx, "site_title", cfg.site_title);
        tmpl_ctx_set(ctx, "base_url", cfg.base_url);

        /* tags as comma-separated string */
        char tags_str[1024] = "";
        for (int t = 0; t < p->meta.tag_count; t++) {
            if (t > 0) strncat(tags_str, ", ", sizeof(tags_str) - strlen(tags_str) - 1);
            strncat(tags_str, p->meta.tags[t], sizeof(tags_str) - strlen(tags_str) - 1);
        }
        tmpl_ctx_set(ctx, "tags", tags_str);

        /* tags as list */
        TmplList *tag_list = tmpl_ctx_set_list(ctx, "tag_list");
        for (int t = 0; t < p->meta.tag_count; t++) {
            TmplCtx *tc = tmpl_list_add(tag_list);
            tmpl_ctx_set(tc, "name", p->meta.tags[t]);
            char tag_url[256];
            char *tag_slug = slugify(p->meta.tags[t]);
            snprintf(tag_url, sizeof(tag_url), "%s/tags/%s.html",
                     cfg.base_url, tag_slug);
            tmpl_ctx_set(tc, "url", tag_url);
            free(tag_slug);
        }

        char *inner = tmpl_render(post_tpl, ctx);

        /* Build excerpt for SEO description (strip tags + normalize whitespace) */
        char seo_desc[256] = "";
        if (p->content_html) {
            size_t sj = 0, sk = 0;
            bool seo_in_tag = false;
            bool prev_space = false;
            const char *shtml = p->content_html;
            while (shtml[sk] && sj < 160) {
                if (shtml[sk] == '<') { seo_in_tag = true; sk++; continue; }
                if (shtml[sk] == '>') { seo_in_tag = false; sk++; continue; }
                if (!seo_in_tag) {
                    char c = shtml[sk];
                    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                    if (c == ' ' && prev_space) { sk++; continue; }
                    prev_space = (c == ' ');
                    seo_desc[sj++] = c;
                }
                sk++;
            }
            seo_desc[sj] = '\0';
        }
        char post_url[MAX_URL_LEN];
        snprintf(post_url, sizeof(post_url), "%s/posts/%s.html",
                 cfg.base_url, p->meta.slug);
        char *full = wrap_in_layout(&theme, &cfg, p->meta.title, inner,
                                     seo_desc, post_url);

        char out_path[MAX_PATH_LEN];
        snprintf(out_path, sizeof(out_path), "%s/%s.html", posts_dir, p->meta.slug);
        write_file(out_path, full);
        printf("  Generated: posts/%s.html\n", p->meta.slug);

        free(inner);
        free(full);
        tmpl_ctx_free(ctx);
    }
    free(post_tpl);

    /* ── Generate static pages ─────────────────────────────────────── */
    {
        char pages_dir[MAX_PATH_LEN];
        path_join(pages_dir, sizeof(pages_dir), project_dir, "pages");

        if (dir_exists(pages_dir)) {
            /* Reuse post template for pages */
            char *page_tpl = theme_read_template(&theme, "page.html");
            if (!page_tpl)
                page_tpl = theme_read_template(&theme, "post.html");
            if (!page_tpl)
                page_tpl = strdup("{{{ content }}}");

            DIR *pdir = opendir(pages_dir);
            if (pdir) {
                int page_count = 0;
                struct dirent *ent;
                while ((ent = readdir(pdir)) != NULL) {
                    const char *ext = path_extension(ent->d_name);
                    if (strcmp(ext, ".md") != 0 && strcmp(ext, ".markdown") != 0)
                        continue;

                    char fpath[MAX_PATH_LEN];
                    path_join(fpath, sizeof(fpath), pages_dir, ent->d_name);

                    char *raw = read_file(fpath);
                    if (!raw) continue;

                    FrontMatter fm;
                    const char *body = raw;
                    parse_front_matter(raw, &fm, &body);

                    if (fm.draft) {
                        printf("  Skipping draft page: %s\n", fm.title);
                        free(raw);
                        continue;
                    }

                    char *html = markdown_to_html(body);

                    /* Derive slug from front matter or filename */
                    if (fm.slug[0] == '\0') {
                        char name[256];
                        snprintf(name, sizeof(name), "%s", ent->d_name);
                        char *dot = strrchr(name, '.');
                        if (dot) *dot = '\0';
                        char *s = slugify(name);
                        if (s) {
                            snprintf(fm.slug, sizeof(fm.slug), "%s", s);
                            free(s);
                        }
                    }
                    if (fm.title[0] == '\0') {
                        char name[256];
                        snprintf(name, sizeof(name), "%s", ent->d_name);
                        char *dot = strrchr(name, '.');
                        if (dot) *dot = '\0';
                        snprintf(fm.title, sizeof(fm.title), "%s", name);
                    }

                    TmplCtx *ctx = tmpl_ctx_new();
                    tmpl_ctx_set(ctx, "title", fm.title);
                    tmpl_ctx_set(ctx, "date", fm.date);
                    tmpl_ctx_set(ctx, "slug", fm.slug);
                    tmpl_ctx_set(ctx, "content", html);
                    tmpl_ctx_set(ctx, "author", cfg.author);
                    tmpl_ctx_set(ctx, "site_title", cfg.site_title);
                    tmpl_ctx_set(ctx, "base_url", cfg.base_url);
                    tmpl_ctx_set(ctx, "tags", "");

                    char *inner = tmpl_render(page_tpl, ctx);
                    char pg_url[MAX_URL_LEN];
                    snprintf(pg_url, sizeof(pg_url), "%s/pages/%s.html",
                             cfg.base_url, fm.slug);
                    char *full = wrap_in_layout(&theme, &cfg, fm.title, inner,
                                                 NULL, pg_url);

                    char out_path[MAX_PATH_LEN];
                    snprintf(out_path, sizeof(out_path), "%s/%s.html",
                             pages_out_dir, fm.slug);
                    write_file(out_path, full);
                    printf("  Generated page: pages/%s.html\n", fm.slug);

                    free(inner);
                    free(full);
                    free(html);
                    free(raw);
                    tmpl_ctx_free(ctx);
                    page_count++;
                }
                closedir(pdir);
                if (page_count > 0)
                    printf("Found %d page(s)\n", page_count);
            }
            free(page_tpl);
        }
    }

    /* ── Generate index page ────────────────────────────────────────── */
    {
        char *index_tpl = theme_read_template(&theme, "index.html");
        if (!index_tpl) index_tpl = strdup("<h1>{{ site_title }}</h1>\n{% for post in posts %}\n<a href=\"{{ post.url }}\">{{ post.title }}</a>\n{% endfor %}");

        TmplCtx *ctx = tmpl_ctx_new();
        tmpl_ctx_set(ctx, "site_title", cfg.site_title);
        tmpl_ctx_set(ctx, "author", cfg.author);
        tmpl_ctx_set(ctx, "base_url", cfg.base_url);

        TmplList *post_list = tmpl_ctx_set_list(ctx, "posts");
        int shown = 0;
        for (int i = 0; i < post_count && shown < cfg.pagination_size; i++) {
            if (posts[i].meta.draft) continue;
            TmplCtx *pc = tmpl_list_add(post_list);
            tmpl_ctx_set(pc, "title", posts[i].meta.title);
            tmpl_ctx_set(pc, "date", posts[i].meta.date);
            tmpl_ctx_set(pc, "slug", posts[i].meta.slug);
            char url[512];
            snprintf(url, sizeof(url), "%s/posts/%s.html", cfg.base_url, posts[i].meta.slug);
            tmpl_ctx_set(pc, "url", url);

            /* excerpt: first 200 chars of content */
            if (posts[i].content_html) {
                char excerpt[256];
                /* strip tags for excerpt */
                size_t j = 0, k = 0;
                bool in_tag = false;
                const char *html = posts[i].content_html;
                while (html[k] && j < 200) {
                    if (html[k] == '<') in_tag = true;
                    else if (html[k] == '>') in_tag = false;
                    else if (!in_tag) excerpt[j++] = html[k];
                    k++;
                }
                excerpt[j] = '\0';
                if (j >= 200) {
                    excerpt[197] = '.';
                    excerpt[198] = '.';
                    excerpt[199] = '.';
                    excerpt[200] = '\0';
                }
                tmpl_ctx_set(pc, "excerpt", excerpt);
            }
            shown++;
        }

        char *inner = tmpl_render(index_tpl, ctx);
        char *full = wrap_in_layout(&theme, &cfg, cfg.site_title, inner,
                                     NULL, NULL);

        char out_path[MAX_PATH_LEN];
        path_join(out_path, sizeof(out_path), output_dir, "index.html");
        write_file(out_path, full);
        printf("  Generated: index.html\n");

        free(inner);
        free(full);
        tmpl_ctx_free(ctx);
        free(index_tpl);
    }

    /* ── Generate archive page ──────────────────────────────────────── */
    {
        char *archive_tpl = theme_read_template(&theme, "archive.html");
        if (!archive_tpl)
            archive_tpl = strdup("<h1>Archive</h1>\n{% for post in posts %}\n<p><a href=\"{{ post.url }}\">{{ post.title }}</a> - {{ post.date }}</p>\n{% endfor %}");

        TmplCtx *ctx = tmpl_ctx_new();
        tmpl_ctx_set(ctx, "site_title", cfg.site_title);
        tmpl_ctx_set(ctx, "base_url", cfg.base_url);

        TmplList *post_list = tmpl_ctx_set_list(ctx, "posts");
        for (int i = 0; i < post_count; i++) {
            if (posts[i].meta.draft) continue;
            TmplCtx *pc = tmpl_list_add(post_list);
            tmpl_ctx_set(pc, "title", posts[i].meta.title);
            tmpl_ctx_set(pc, "date", posts[i].meta.date);
            char url[512];
            snprintf(url, sizeof(url), "%s/posts/%s.html", cfg.base_url, posts[i].meta.slug);
            tmpl_ctx_set(pc, "url", url);
        }

        char *inner = tmpl_render(archive_tpl, ctx);
        char *full = wrap_in_layout(&theme, &cfg, "Archive", inner,
                                     NULL, NULL);

        char out_path[MAX_PATH_LEN];
        path_join(out_path, sizeof(out_path), output_dir, "archive.html");
        write_file(out_path, full);
        printf("  Generated: archive.html\n");

        free(inner);
        free(full);
        tmpl_ctx_free(ctx);
        free(archive_tpl);
    }

    /* ── Generate tag pages ─────────────────────────────────────────── */
    {
        char *tag_tpl = theme_read_template(&theme, "tag.html");
        if (!tag_tpl)
            tag_tpl = strdup("<h1>Tag: {{ tag_name }}</h1>\n{% for post in posts %}\n<p><a href=\"{{ post.url }}\">{{ post.title }}</a></p>\n{% endfor %}");

        for (int ti = 0; ti < tag_count; ti++) {
            TmplCtx *ctx = tmpl_ctx_new();
            tmpl_ctx_set(ctx, "tag_name", tags[ti].name);
            tmpl_ctx_set(ctx, "site_title", cfg.site_title);
            tmpl_ctx_set(ctx, "base_url", cfg.base_url);

            TmplList *post_list = tmpl_ctx_set_list(ctx, "posts");
            for (int pi = 0; pi < tags[ti].count; pi++) {
                int idx = tags[ti].post_indices[pi];
                TmplCtx *pc = tmpl_list_add(post_list);
                tmpl_ctx_set(pc, "title", posts[idx].meta.title);
                tmpl_ctx_set(pc, "date", posts[idx].meta.date);
                char url[512];
                snprintf(url, sizeof(url), "%s/posts/%s.html",
                         cfg.base_url, posts[idx].meta.slug);
                tmpl_ctx_set(pc, "url", url);
            }

            char *inner = tmpl_render(tag_tpl, ctx);
            char page_title[256];
            snprintf(page_title, sizeof(page_title), "Tag: %s", tags[ti].name);
            char *full = wrap_in_layout(&theme, &cfg, page_title, inner,
                                         NULL, NULL);

            char *tag_slug = slugify(tags[ti].name);
            char out_path[MAX_PATH_LEN];
            snprintf(out_path, sizeof(out_path), "%s/%s.html", tags_dir, tag_slug);
            write_file(out_path, full);
            printf("  Generated: tags/%s.html\n", tag_slug);

            free(tag_slug);
            free(inner);
            free(full);
            tmpl_ctx_free(ctx);
        }
        free(tag_tpl);
    }

    /* ── Generate tags index page ──────────────────────────────────── */
    {
        char *tags_index_tpl = theme_read_template(&theme, "tags_index.html");
        if (!tags_index_tpl)
            tags_index_tpl = strdup(
                "<h1>Tags</h1>\n"
                "{% for tag in tags %}\n"
                "<section class=\"tag-group\">\n"
                "    <h2 id=\"{{ tag.slug }}\"><a href=\"{{ tag.url }}\">{{ tag.name }}</a>"
                " <span class=\"tag-count\">({{ tag.count }})</span></h2>\n"
                "    <ul>\n"
                "    {% for post in tag.posts %}\n"
                "        <li><time datetime=\"{{ post.date }}\">{{ post.date }}</time> "
                "&mdash; <a href=\"{{ post.url }}\">{{ post.title }}</a></li>\n"
                "    {% endfor %}\n"
                "    </ul>\n"
                "</section>\n"
                "{% endfor %}\n");

        TmplCtx *ctx = tmpl_ctx_new();
        tmpl_ctx_set(ctx, "site_title", cfg.site_title);
        tmpl_ctx_set(ctx, "base_url", cfg.base_url);

        TmplList *tag_list_all = tmpl_ctx_set_list(ctx, "tags");
        for (int ti = 0; ti < tag_count; ti++) {
            TmplCtx *tc = tmpl_list_add(tag_list_all);
            tmpl_ctx_set(tc, "name", tags[ti].name);
            char *ts = slugify(tags[ti].name);
            tmpl_ctx_set(tc, "slug", ts ? ts : tags[ti].name);
            char tag_url[256];
            snprintf(tag_url, sizeof(tag_url), "%s/tags/%s.html",
                     cfg.base_url, ts ? ts : tags[ti].name);
            tmpl_ctx_set(tc, "url", tag_url);
            char count_str[16];
            snprintf(count_str, sizeof(count_str), "%d", tags[ti].count);
            tmpl_ctx_set(tc, "count", count_str);
            free(ts);

            TmplList *tposts = tmpl_ctx_set_list(tc, "posts");
            for (int pi = 0; pi < tags[ti].count; pi++) {
                int idx = tags[ti].post_indices[pi];
                TmplCtx *pc = tmpl_list_add(tposts);
                tmpl_ctx_set(pc, "title", posts[idx].meta.title);
                tmpl_ctx_set(pc, "date", posts[idx].meta.date);
                char url[512];
                snprintf(url, sizeof(url), "%s/posts/%s.html",
                         cfg.base_url, posts[idx].meta.slug);
                tmpl_ctx_set(pc, "url", url);
            }
        }

        char *inner = tmpl_render(tags_index_tpl, ctx);
        char *full = wrap_in_layout(&theme, &cfg, "Tags", inner,
                                     NULL, NULL);

        char out_path[MAX_PATH_LEN];
        path_join(out_path, sizeof(out_path), output_dir, "tags.html");
        write_file(out_path, full);
        printf("  Generated: tags.html\n");

        free(inner);
        free(full);
        tmpl_ctx_free(ctx);
        free(tags_index_tpl);
    }

    /* ── Generate RSS feed ──────────────────────────────────────────── */
    if (cfg.enable_rss) {
        char *rss_xml = rss_generate(&cfg, posts, post_count);
        if (rss_xml) {
            char out_path[MAX_PATH_LEN];
            path_join(out_path, sizeof(out_path), output_dir, "rss.xml");
            write_file(out_path, rss_xml);
            printf("  Generated: rss.xml\n");
            free(rss_xml);
        }
    }

    /* ── Copy theme assets ──────────────────────────────────────────── */
    theme_copy_assets(&theme, output_dir);
    printf("  Copied theme assets\n");

    /* ── Copy project assets ────────────────────────────────────────── */
    {
        char proj_assets[MAX_PATH_LEN];
        path_join(proj_assets, sizeof(proj_assets), project_dir, "assets");
        if (dir_exists(proj_assets)) {
            char dst_assets[MAX_PATH_LEN];
            path_join(dst_assets, sizeof(dst_assets), output_dir, "assets");
            copy_dir_recursive(proj_assets, dst_assets);
            printf("  Copied project assets\n");
        }
    }

    /* ── Write .nojekyll for GitHub Pages ───────────────────────────── */
    {
        char nojekyll[MAX_PATH_LEN];
        path_join(nojekyll, sizeof(nojekyll), output_dir, ".nojekyll");
        write_file(nojekyll, "");
        printf("  Created .nojekyll\n");
    }

    printf("\nBuild complete! Output in %s/\n", cfg.output_dir);

    free_posts(posts, post_count);
    free(posts);
    free(tags);
    return 0;
}
