#define _GNU_SOURCE
#include "utils.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

int safe_write_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 0;
}

int read_text_file(const char *path, char *buf, size_t cap, size_t *out_len) {
    if (!path || !buf || cap < 2) return -1;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    size_t off = 0;
    while (off + 1 < cap) {
        ssize_t n = read(fd, buf + off, cap - off - 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return -1;
        }
        if (n == 0) break;
        off += (size_t)n;
    }
    buf[off] = '\0';
    close(fd);
    if (out_len) *out_len = off;
    return 0;
}

int read_text_file_alloc(const char *path, size_t max_bytes, char **out, size_t *out_len) {
    if (!path || !out || max_bytes == 0) return -1;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    size_t cap = 4096;
    if (cap > max_bytes + 1) cap = max_bytes + 1;
    char *buf = malloc(cap);
    if (!buf) { close(fd); return -1; }
    size_t len = 0;
    while (len < max_bytes) {
        if (len + 2048 + 1 > cap) {
            size_t ncap = cap * 2;
            if (ncap > max_bytes + 1) ncap = max_bytes + 1;
            if (ncap <= cap) break;
            char *tmp = realloc(buf, ncap);
            if (!tmp) { free(buf); close(fd); return -1; }
            buf = tmp; cap = ncap;
        }
        ssize_t n = read(fd, buf + len, cap - len - 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            free(buf); close(fd); return -1;
        }
        if (n == 0) break;
        len += (size_t)n;
    }
    buf[len] = '\0';
    close(fd);
    *out = buf;
    if (out_len) *out_len = len;
    return 0;
}

int appendf(char **buf, size_t *len, size_t *cap, const char *fmt, ...) {
    if (!buf || !len || !cap || !fmt) return -1;
    if (!*buf) {
        *cap = 4096;
        *buf = malloc(*cap);
        if (!*buf) return -1;
        (*buf)[0] = '\0';
        *len = 0;
    }
    for (;;) {
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(*buf + *len, *cap - *len, fmt, ap);
        va_end(ap);
        if (n < 0) return -1;
        if ((size_t)n < *cap - *len) {
            *len += (size_t)n;
            return 0;
        }
        size_t need = *len + (size_t)n + 1;
        size_t ncap = *cap * 2;
        while (ncap < need) ncap *= 2;
        if (ncap > 16 * 1024 * 1024) return -1;
        char *tmp = realloc(*buf, ncap);
        if (!tmp) return -1;
        *buf = tmp; *cap = ncap;
    }
}

void json_escape_append(char **buf, size_t *len, size_t *cap, const char *s) {
    if (!s) s = "";
    appendf(buf, len, cap, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        switch (*p) {
            case '\\': appendf(buf, len, cap, "\\\\"); break;
            case '"': appendf(buf, len, cap, "\\\""); break;
            case '\n': appendf(buf, len, cap, "\\n"); break;
            case '\r': appendf(buf, len, cap, "\\r"); break;
            case '\t': appendf(buf, len, cap, "\\t"); break;
            default:
                if (*p < 0x20) appendf(buf, len, cap, "\\u%04x", *p);
                else appendf(buf, len, cap, "%c", *p);
        }
    }
    appendf(buf, len, cap, "\"");
}

void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || isspace((unsigned char)s[n-1]))) s[--n] = '\0';
}

int is_digits(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; ++s) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

int parse_positive_int(const char *s, int minv, int maxv, int *out) {
    if (!is_digits(s)) return -1;
    errno = 0;
    long v = strtol(s, NULL, 10);
    if (errno || v < minv || v > maxv) return -1;
    if (out) *out = (int)v;
    return 0;
}

int path_readlink(const char *path, char *buf, size_t cap) {
    if (!path || !buf || cap < 2) return -1;
    ssize_t n = readlink(path, buf, cap - 1);
    if (n < 0) return -1;
    buf[n] = '\0';
    return 0;
}
