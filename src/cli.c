#include "cli.h"
#include "cblog.h"
#include "config.h"
#include "builder.h"
#include "server.h"
#include "theme.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Usage ───────────────────────────────────────────────────────────── */

static void print_usage(void) {
    printf("cblog %s — Static blog generator\n\n", CBLOG_VERSION);
    printf("Usage:\n");
    printf("  cblog init <project_name>        Create a new blog project\n");
    printf("  cblog new \"Post Title\"            Create a new blog post\n");
    printf("  cblog page \"Page Title\"          Create a new static page\n");
    printf("  cblog build                      Build the static site\n");
    printf("  cblog serve --port <port>        Serve locally (default: 8080)\n");
    printf("  cblog theme set <theme_name>     Set the active theme\n");
    printf("  cblog help                       Show this message\n");
    printf("  cblog version                    Show version\n");
}

/* ── Init command ────────────────────────────────────────────────────── */

static int cmd_init(const char *project_name) {
    printf("Initializing project '%s'...\n", project_name);

    if (dir_exists(project_name)) {
        fprintf(stderr, "Error: directory '%s' already exists\n", project_name);
        return 1;
    }

    /* Create directory structure */
    char path[MAX_PATH_LEN];

    mkdir_p(project_name);

    snprintf(path, sizeof(path), "%s/content", project_name);
    mkdir_p(path);

    snprintf(path, sizeof(path), "%s/themes", project_name);
    mkdir_p(path);

    snprintf(path, sizeof(path), "%s/public", project_name);
    mkdir_p(path);

    snprintf(path, sizeof(path), "%s/templates", project_name);
    mkdir_p(path);

    snprintf(path, sizeof(path), "%s/assets", project_name);
    mkdir_p(path);

    snprintf(path, sizeof(path), "%s/pages", project_name);
    mkdir_p(path);

    /* Copy default theme */
    {
        /* Try relative path first (if cblog repo/build dir is current) */
        const char *theme_sources[] = {
            "themes/default",
            "/usr/local/share/cblog/themes/default",
            NULL
        };

        char theme_dst[MAX_PATH_LEN];
        snprintf(theme_dst, sizeof(theme_dst), "%s/themes/default", project_name);

        bool copied = false;
        for (int i = 0; theme_sources[i]; i++) {
            if (dir_exists(theme_sources[i])) {
                copy_dir_recursive(theme_sources[i], theme_dst);
                copied = true;
                break;
            }
        }

        if (!copied) {
            /* Create a minimal default theme inline */
            char tpl_dir[MAX_PATH_LEN];
            snprintf(tpl_dir, sizeof(tpl_dir), "%s/templates", theme_dst);
            mkdir_p(tpl_dir);

            char css_dir[MAX_PATH_LEN];
            snprintf(css_dir, sizeof(css_dir), "%s/assets/css", theme_dst);
            mkdir_p(css_dir);

            /* theme.json */
            snprintf(path, sizeof(path), "%s/theme.json", theme_dst);
            write_file(path,
                "{\n"
                "    \"name\": \"default\",\n"
                "    \"version\": \"1.0.0\",\n"
                "    \"author\": \"cblog\",\n"
                "    \"description\": \"Default theme\"\n"
                "}\n");

            /* layout.html */
            snprintf(path, sizeof(path), "%s/templates/layout.html", theme_dst);
            write_file(path,
                "<!DOCTYPE html>\n"
                "<html lang=\"en\">\n"
                "<head>\n"
                "    <meta charset=\"UTF-8\">\n"
                "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                "    <title>{{ page_title }} — {{ site_title }}</title>\n"
                "    <link rel=\"stylesheet\" href=\"{{ base_url }}/assets/css/style.css\">\n"
                "</head>\n"
                "<body>\n"
                "    <header><nav><a href=\"{{ base_url }}/\">{{ site_title }}</a>"
                " | <a href=\"{{ base_url }}/archive.html\">Archive</a>"
                " | <a href=\"{{ base_url }}/tags.html\">Tags</a></nav></header>\n"
                "    <main>{{{ content }}}</main>\n"
                "    <footer><p>&copy; {{ year }} {{ author }}</p></footer>\n"
                "</body>\n"
                "</html>\n");

            /* index.html */
            snprintf(path, sizeof(path), "%s/templates/index.html", theme_dst);
            write_file(path,
                "<h1>{{ site_title }}</h1>\n"
                "{% for post in posts %}\n"
                "<article>\n"
                "    <h2><a href=\"{{ post.url }}\">{{ post.title }}</a></h2>\n"
                "    <time>{{ post.date }}</time>\n"
                "    <p>{{ post.excerpt }}</p>\n"
                "</article>\n"
                "{% endfor %}\n");

            /* post.html */
            snprintf(path, sizeof(path), "%s/templates/post.html", theme_dst);
            write_file(path,
                "<article>\n"
                "    <h1>{{ title }}</h1>\n"
                "    <time>{{ date }}</time>\n"
                "    <div class=\"content\">{{{ content }}}</div>\n"
                "    {% if tags %}\n"
                "    <div class=\"tags\">Tags: {{ tags }}</div>\n"
                "    {% endif %}\n"
                "</article>\n");

            /* archive.html */
            snprintf(path, sizeof(path), "%s/templates/archive.html", theme_dst);
            write_file(path,
                "<h1>Archive</h1>\n"
                "{% for post in posts %}\n"
                "<div class=\"archive-entry\">\n"
                "    <time>{{ post.date }}</time>\n"
                "    <a href=\"{{ post.url }}\">{{ post.title }}</a>\n"
                "</div>\n"
                "{% endfor %}\n");

            /* tag.html */
            snprintf(path, sizeof(path), "%s/templates/tag.html", theme_dst);
            write_file(path,
                "<h1>Tag: {{ tag_name }}</h1>\n"
                "{% for post in posts %}\n"
                "<div class=\"tag-entry\">\n"
                "    <a href=\"{{ post.url }}\">{{ post.title }}</a>\n"
                "    <time>{{ post.date }}</time>\n"
                "</div>\n"
                "{% endfor %}\n");

            /* style.css */
            snprintf(path, sizeof(path), "%s/assets/css/style.css", theme_dst);
            write_file(path,
                "*, *::before, *::after { box-sizing: border-box; }\n"
                "body { font-family: system-ui, -apple-system, sans-serif;\n"
                "  max-width: 48rem; margin: 0 auto; padding: 2rem;\n"
                "  line-height: 1.6; color: #1a1a1a; background: #fafafa; }\n"
                "a { color: #0066cc; text-decoration: none; }\n"
                "a:hover { text-decoration: underline; }\n"
                "header nav { padding: 1rem 0; border-bottom: 1px solid #ddd;\n"
                "  margin-bottom: 2rem; }\n"
                "header nav a { margin-right: 1rem; font-weight: 600; }\n"
                "article { margin-bottom: 2rem; }\n"
                "article h2 { margin-bottom: 0.25rem; }\n"
                "time { color: #666; font-size: 0.9rem; }\n"
                "pre { background: #f0f0f0; padding: 1rem; border-radius: 4px;\n"
                "  overflow-x: auto; }\n"
                "code { background: #f0f0f0; padding: 0.15rem 0.3rem;\n"
                "  border-radius: 3px; font-size: 0.9em; }\n"
                "pre code { background: none; padding: 0; }\n"
                "blockquote { border-left: 3px solid #ddd; margin-left: 0;\n"
                "  padding-left: 1rem; color: #555; }\n"
                "img { max-width: 100%; height: auto; }\n"
                "table { border-collapse: collapse; width: 100%; margin: 1rem 0; }\n"
                "th, td { border: 1px solid #ddd; padding: 0.5rem; text-align: left; }\n"
                "th { background: #f0f0f0; }\n"
                ".tags { margin-top: 1rem; color: #666; }\n"
                ".archive-entry, .tag-entry { display: flex; gap: 1rem;\n"
                "  margin-bottom: 0.5rem; }\n"
                "footer { margin-top: 3rem; padding-top: 1rem;\n"
                "  border-top: 1px solid #ddd; color: #666; font-size: 0.9rem; }\n"
            );
        }
    }

    /* Write default config */
    config_write_default(project_name, project_name);

    /* Create a welcome post */
    {
        char post_path[MAX_PATH_LEN];
        snprintf(post_path, sizeof(post_path), "%s/content/hello-world.md", project_name);

        char date[32];
        get_current_date(date, sizeof(date));

        char post_content[2048];
        snprintf(post_content, sizeof(post_content),
            "---\n"
            "title: \"Hello World\"\n"
            "date: %s\n"
            "slug: hello-world\n"
            "tags: [welcome, intro]\n"
            "draft: false\n"
            "---\n\n"
            "# Welcome to Your New Blog\n\n"
            "This is your first blog post. Edit or delete it, then build your site.\n\n"
            "## Getting Started\n\n"
            "1. Edit this post in `content/hello-world.md`\n"
            "2. Create new posts with `cblog new \"My Post\"`\n"
            "3. Build with `cblog build`\n"
            "4. Preview with `cblog serve --port 8080`\n\n"
            "Happy blogging!\n",
            date);
        write_file(post_path, post_content);
    }

    printf("Project '%s' created successfully!\n", project_name);
    printf("\nNext steps:\n");
    printf("  cd %s\n", project_name);
    printf("  cblog build\n");
    printf("  cblog serve --port 8080\n");
    return 0;
}

/* ── New post command ────────────────────────────────────────────────── */

static int cmd_new(const char *title) {
    /* Check we're in a project */
    if (!file_exists("config.json")) {
        fprintf(stderr, "Error: config.json not found. Are you in a cblog project?\n");
        return 1;
    }

    char *slug = slugify(title);
    if (!slug) {
        fprintf(stderr, "Error: cannot generate slug\n");
        return 1;
    }

    char filename[MAX_PATH_LEN];
    snprintf(filename, sizeof(filename), "content/%s.md", slug);

    if (file_exists(filename)) {
        fprintf(stderr, "Error: %s already exists\n", filename);
        free(slug);
        return 1;
    }

    char date[32];
    get_current_date(date, sizeof(date));

    char content[2048];
    snprintf(content, sizeof(content),
        "---\n"
        "title: \"%s\"\n"
        "date: %s\n"
        "slug: %s\n"
        "tags: []\n"
        "draft: false\n"
        "---\n\n"
        "Write your content here.\n",
        title, date, slug);

    mkdir_p("content");
    if (!write_file(filename, content)) {
        fprintf(stderr, "Error: cannot create %s\n", filename);
        free(slug);
        return 1;
    }

    printf("Created: %s\n", filename);
    free(slug);
    return 0;
}

/* ── New page command ────────────────────────────────────────────────── */

static int cmd_page(const char *title) {
    /* Check we're in a project */
    if (!file_exists("config.json")) {
        fprintf(stderr, "Error: config.json not found. Are you in a cblog project?\n");
        return 1;
    }

    char *slug = slugify(title);
    if (!slug) {
        fprintf(stderr, "Error: cannot generate slug\n");
        return 1;
    }

    char filename[MAX_PATH_LEN];
    snprintf(filename, sizeof(filename), "pages/%s.md", slug);

    if (file_exists(filename)) {
        fprintf(stderr, "Error: %s already exists\n", filename);
        free(slug);
        return 1;
    }

    char date[32];
    get_current_date(date, sizeof(date));

    char content[2048];
    snprintf(content, sizeof(content),
        "---\n"
        "title: \"%s\"\n"
        "date: %s\n"
        "slug: %s\n"
        "---\n\n"
        "Write your page content here.\n",
        title, date, slug);

    mkdir_p("pages");
    if (!write_file(filename, content)) {
        fprintf(stderr, "Error: cannot create %s\n", filename);
        free(slug);
        return 1;
    }

    printf("Created: %s\n", filename);
    free(slug);
    return 0;
}

/* ── Serve command ───────────────────────────────────────────────────── */

static int cmd_serve(int port) {
    Config cfg;
    if (!config_load(&cfg, ".")) {
        /* Try serving from public/ directly */
        if (dir_exists("public"))
            return server_start("public", port);
        fprintf(stderr, "Error: no public/ directory. Run 'cblog build' first.\n");
        return 1;
    }

    char output_dir[MAX_PATH_LEN];
    path_join(output_dir, sizeof(output_dir), ".", cfg.output_dir);
    if (!dir_exists(output_dir)) {
        fprintf(stderr, "Error: %s/ not found. Run 'cblog build' first.\n", cfg.output_dir);
        return 1;
    }

    return server_start(output_dir, port);
}

/* ── Theme command ───────────────────────────────────────────────────── */

static int cmd_theme_set(const char *name) {
    if (!file_exists("config.json")) {
        fprintf(stderr, "Error: config.json not found. Are you in a cblog project?\n");
        return 1;
    }
    return theme_set(".", name) ? 0 : 1;
}

/* ── Main dispatcher ─────────────────────────────────────────────────── */

int cli_run(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0) {
        printf("cblog %s\n", CBLOG_VERSION);
        return 0;
    }

    if (strcmp(cmd, "init") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cblog init <project_name>\n");
            return 1;
        }
        return cmd_init(argv[2]);
    }

    if (strcmp(cmd, "new") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cblog new \"Post Title\"\n");
            return 1;
        }
        return cmd_new(argv[2]);
    }

    if (strcmp(cmd, "page") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cblog page \"Page Title\"\n");
            return 1;
        }
        return cmd_page(argv[2]);
    }

    if (strcmp(cmd, "build") == 0) {
        return builder_build(".");
    }

    if (strcmp(cmd, "serve") == 0) {
        int port = 8080;
        for (int i = 2; i < argc - 1; i++) {
            if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) {
                port = atoi(argv[i + 1]);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Error: invalid port number\n");
                    return 1;
                }
            }
        }
        return cmd_serve(port);
    }

    if (strcmp(cmd, "theme") == 0) {
        if (argc >= 4 && strcmp(argv[2], "set") == 0) {
            return cmd_theme_set(argv[3]);
        }
        fprintf(stderr, "Usage: cblog theme set <theme_name>\n");
        return 1;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    print_usage();
    return 1;
}
