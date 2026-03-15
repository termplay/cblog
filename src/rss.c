#include "rss.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *rss_generate(const Config *cfg, Post *posts, int count) {
    if (!cfg || !posts) return NULL;

    size_t cap = 8192;
    char *xml = malloc(cap);
    if (!xml) return NULL;

    size_t len = 0;

#define RSSCAT(s) do { \
    size_t _n = strlen(s); \
    while (len + _n + 1 > cap) { cap *= 2; xml = realloc(xml, cap); } \
    memcpy(xml + len, s, _n); len += _n; xml[len] = '\0'; \
} while(0)

    RSSCAT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    RSSCAT("<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n");
    RSSCAT("<channel>\n");

    /* channel metadata */
    RSSCAT("  <title>");
    char *esc_title = html_escape(cfg->site_title);
    RSSCAT(esc_title);
    free(esc_title);
    RSSCAT("</title>\n");

    if (cfg->base_url[0]) {
        RSSCAT("  <link>");
        RSSCAT(cfg->base_url);
        RSSCAT("</link>\n");
        RSSCAT("  <atom:link href=\"");
        RSSCAT(cfg->base_url);
        RSSCAT("/rss.xml\" rel=\"self\" type=\"application/rss+xml\"/>\n");
    }

    RSSCAT("  <description>Blog by ");
    char *esc_author = html_escape(cfg->author);
    RSSCAT(esc_author);
    free(esc_author);
    RSSCAT("</description>\n");

    /* items */
    for (int i = 0; i < count; i++) {
        Post *p = &posts[i];
        if (p->meta.draft) continue;

        RSSCAT("  <item>\n");
        RSSCAT("    <title>");
        char *et = html_escape(p->meta.title);
        RSSCAT(et);
        free(et);
        RSSCAT("</title>\n");

        if (cfg->base_url[0]) {
            RSSCAT("    <link>");
            RSSCAT(cfg->base_url);
            RSSCAT("/posts/");
            RSSCAT(p->meta.slug);
            RSSCAT(".html</link>\n");

            RSSCAT("    <guid>");
            RSSCAT(cfg->base_url);
            RSSCAT("/posts/");
            RSSCAT(p->meta.slug);
            RSSCAT(".html</guid>\n");
        }

        char rfc_date[128];
        get_rfc822_date(p->meta.date, rfc_date, sizeof(rfc_date));
        RSSCAT("    <pubDate>");
        RSSCAT(rfc_date);
        RSSCAT("</pubDate>\n");

        if (p->content_html) {
            RSSCAT("    <description><![CDATA[");
            RSSCAT(p->content_html);
            RSSCAT("]]></description>\n");
        }

        RSSCAT("  </item>\n");
    }

    RSSCAT("</channel>\n");
    RSSCAT("</rss>\n");

#undef RSSCAT

    return xml;
}
