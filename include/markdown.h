#ifndef CBLOG_MARKDOWN_H
#define CBLOG_MARKDOWN_H

/* Convert a markdown string to HTML. Caller must free the returned string. */
char *markdown_to_html(const char *md);

#endif /* CBLOG_MARKDOWN_H */
