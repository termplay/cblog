#include "server.h"
#include "utils.h"
#include "cblog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <strings.h>

static volatile sig_atomic_t g_running = 1;
static int g_server_fd = -1;

static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
    if (g_server_fd >= 0) {
        shutdown(g_server_fd, SHUT_RDWR);
        close(g_server_fd);
        g_server_fd = -1;
    }
}

/* ── MIME type lookup ────────────────────────────────────────────────── */

static const char *get_mime_type(const char *path) {
    const char *ext = path_extension(path);
    if (!ext || !*ext) return "application/octet-stream";

    struct { const char *ext; const char *mime; } map[] = {
        { ".html", "text/html; charset=utf-8" },
        { ".htm",  "text/html; charset=utf-8" },
        { ".css",  "text/css; charset=utf-8" },
        { ".js",   "application/javascript; charset=utf-8" },
        { ".json", "application/json; charset=utf-8" },
        { ".xml",  "application/xml; charset=utf-8" },
        { ".png",  "image/png" },
        { ".jpg",  "image/jpeg" },
        { ".jpeg", "image/jpeg" },
        { ".gif",  "image/gif" },
        { ".svg",  "image/svg+xml" },
        { ".ico",  "image/x-icon" },
        { ".woff", "font/woff" },
        { ".woff2","font/woff2" },
        { ".ttf",  "font/ttf" },
        { ".txt",  "text/plain; charset=utf-8" },
        { ".md",   "text/plain; charset=utf-8" },
        { NULL, NULL }
    };

    for (int i = 0; map[i].ext; i++) {
        if (strcasecmp(ext, map[i].ext) == 0)
            return map[i].mime;
    }
    return "application/octet-stream";
}

/* ── Security: prevent directory traversal ───────────────────────────── */

static bool is_safe_path(const char *root, const char *requested) {
    char resolved_root[MAX_PATH_LEN];
    char resolved_path[MAX_PATH_LEN];
    char full_path[MAX_PATH_LEN];

    snprintf(full_path, sizeof(full_path), "%s/%s", root, requested);

    if (!realpath(root, resolved_root)) return false;
    if (!realpath(full_path, resolved_path)) return false;

    size_t root_len = strlen(resolved_root);
    return strncmp(resolved_path, resolved_root, root_len) == 0 &&
           (resolved_path[root_len] == '/' || resolved_path[root_len] == '\0');
}

/* ── Request handler ─────────────────────────────────────────────────── */

typedef struct {
    int         client_fd;
    const char *root_dir;
} ClientArg;

static void send_response(int fd, int status, const char *status_text,
                          const char *content_type, const char *body, size_t body_len) {
    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n",
        status, status_text, content_type, body_len);

    send(fd, header, (size_t)hlen, 0);
    if (body && body_len > 0)
        send(fd, body, body_len, 0);
}

static void send_error(int fd, int status, const char *text) {
    char body[256];
    int blen = snprintf(body, sizeof(body),
        "<html><body><h1>%d %s</h1></body></html>", status, text);
    send_response(fd, status, text, "text/html", body, (size_t)blen);
}

static void *handle_client(void *arg) {
    ClientArg *ca = (ClientArg *)arg;
    int fd = ca->client_fd;
    const char *root = ca->root_dir;
    free(ca);

    char request[4096];
    ssize_t n = recv(fd, request, sizeof(request) - 1, 0);
    if (n <= 0) {
        close(fd);
        return NULL;
    }
    request[n] = '\0';

    /* Parse method and path */
    char method[16], path[2048];
    if (sscanf(request, "%15s %2047s", method, path) != 2) {
        send_error(fd, 400, "Bad Request");
        close(fd);
        return NULL;
    }

    /* Only support GET and HEAD */
    bool is_head = (strcmp(method, "HEAD") == 0);
    if (strcmp(method, "GET") != 0 && !is_head) {
        send_error(fd, 405, "Method Not Allowed");
        close(fd);
        return NULL;
    }

    /* URL decode (basic: just handle %20 and +) */
    /* Strip query string */
    char *query = strchr(path, '?');
    if (query) *query = '\0';

    /* Default to index.html */
    char req_path[MAX_PATH_LEN];
    if (strcmp(path, "/") == 0)
        snprintf(req_path, sizeof(req_path), "index.html");
    else
        snprintf(req_path, sizeof(req_path), "%s", path + 1); /* skip leading / */

    /* Security check */
    if (strstr(req_path, "..") || !is_safe_path(root, req_path)) {
        send_error(fd, 403, "Forbidden");
        close(fd);
        return NULL;
    }

    /* Build full path */
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s/%s", root, req_path);

    /* If directory, try index.html */
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        size_t flen = strlen(full_path);
        if (flen > 0 && full_path[flen - 1] != '/')
            snprintf(full_path + flen, sizeof(full_path) - flen, "/index.html");
        else
            snprintf(full_path + flen, sizeof(full_path) - flen, "index.html");
    }

    /* Read file */
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        send_error(fd, 404, "Not Found");
        close(fd);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(f);
        send_error(fd, 500, "Internal Server Error");
        close(fd);
        return NULL;
    }

    char *body = malloc((size_t)file_size);
    if (!body) {
        fclose(f);
        send_error(fd, 500, "Internal Server Error");
        close(fd);
        return NULL;
    }

    size_t read_bytes = fread(body, 1, (size_t)file_size, f);
    fclose(f);

    const char *mime = get_mime_type(full_path);
    if (is_head)
        send_response(fd, 200, "OK", mime, NULL, read_bytes);
    else
        send_response(fd, 200, "OK", mime, body, read_bytes);
    free(body);
    close(fd);
    return NULL;
}

/* ── Public API ──────────────────────────────────────────────────────── */

int server_start(const char *root_dir, int port) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_server_fd);
        return 1;
    }

    if (listen(g_server_fd, 128) < 0) {
        perror("listen");
        close(g_server_fd);
        return 1;
    }

    printf("Serving %s on http://localhost:%d\n", root_dir, port);
    printf("Press Ctrl+C to stop.\n");

    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(g_server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!g_running) break;
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        ClientArg *arg = malloc(sizeof(ClientArg));
        if (!arg) {
            close(client_fd);
            continue;
        }
        arg->client_fd = client_fd;
        arg->root_dir  = root_dir;

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&tid, &attr, handle_client, arg) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(arg);
        }
        pthread_attr_destroy(&attr);
    }

    printf("\nShutting down server.\n");
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    return 0;
}
