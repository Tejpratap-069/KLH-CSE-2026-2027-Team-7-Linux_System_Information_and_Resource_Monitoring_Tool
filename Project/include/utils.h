#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

uint64_t now_ms(void);
int read_text_file(const char *path, char *buf, size_t cap, size_t *out_len);
int read_text_file_alloc(const char *path, size_t max_bytes, char **out, size_t *out_len);
void json_escape_append(char **buf, size_t *len, size_t *cap, const char *s);
int appendf(char **buf, size_t *len, size_t *cap, const char *fmt, ...);
void trim_newline(char *s);
int is_digits(const char *s);
int parse_positive_int(const char *s, int minv, int maxv, int *out);
int safe_write_all(int fd, const void *buf, size_t len);
int path_readlink(const char *path, char *buf, size_t cap);

#endif
