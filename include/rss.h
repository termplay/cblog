#ifndef CBLOG_RSS_H
#define CBLOG_RSS_H

#include "cblog.h"

/* Generate an RSS feed XML string from the given posts. Caller frees result. */
char *rss_generate(const Config *cfg, Post *posts, int count);

#endif /* CBLOG_RSS_H */
