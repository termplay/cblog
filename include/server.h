#ifndef CBLOG_SERVER_H
#define CBLOG_SERVER_H

/* Start a local HTTP server serving files from root_dir on the given port.
 * Blocks until interrupted (SIGINT). Returns 0 on clean shutdown. */
int server_start(const char *root_dir, int port);

#endif /* CBLOG_SERVER_H */
