/* See LICENSE file for copyright and license details. */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

/* die() is taken from dwm's util.c */
void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
        fputc(' ', stderr);
        perror(NULL);
    } else {
        fputc('\n', stderr);
    }

    exit(1);
}

int is_number(const char *str, unsigned long buf_max_len) {
    unsigned long len = strnlen(str, buf_max_len);

    for (size_t i = 0; i < len; ++i)
        if (!isdigit(str[i])) return 0;

    return 1;
}

void reset_buffer(char *buf, size_t size) {
#ifdef __STDC_LIB_EXT1__
    memset_s(buf, size, 0, size);
#else
    memset(buf, 0, size);
#endif
}

void append_buffer(char *src, const char *dst, size_t size) {
#ifdef __STDC_LIB_EXT1__
    strncat_s(src, size, dst, size);
#else
    strncat(src, dst, size);
#endif
}

void copy_buffer(char *dst, const char *src, size_t size) {
#ifdef __STDC_LIB_EXT1__
    strncpy_s(dst, size, src, size);
#else
    strncpy(dst, src, size);
#endif
}

size_t remove_from_buffer(char *buf, char to_remove) {
    size_t new_size = 0;
    char *read = buf;
    char *write = buf;

    while (*read) {
        /* skip all char for removal */
        while (*read == to_remove) {
            read++;
        }
        /* set next valid char instead of 'to_remove' */
        if (*write != *read) *write = *read;

        read++;
        write++;
        new_size++;
    }
    return new_size;
}

/* simple popen2 implementation */

extern char **environ;

pid_t process_open(const char *program, int *pout) {
    /* no stdin as we don't need to write to the process */
    char *argp[] = {"sh", "-c", NULL, NULL};
    int pipe_fds[2];
    pid_t pid;

    if (pipe(pipe_fds) < 0) return -1;

    switch (pid = fork()) {
    case -1: /* error */
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return -1;
    case 0: /* child */
        (void)close(pipe_fds[0]);
        (void)dup2(pipe_fds[1], STDOUT_FILENO);

        argp[2] = (char *)program;
        execve("/bin/sh", argp, environ);
        _exit(127);
    }

    /* parent */
    (void)close(pipe_fds[1]);
    *pout = pipe_fds[0];
    return pid;
}
