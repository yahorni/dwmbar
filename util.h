/* See LICENSE file for copyright and license details. */

#include <stdlib.h>

int is_number(const char *str, unsigned long buf_max_len);

/* buffer manipulation helpers */
void reset_buffer(char *buf, size_t size);
void append_buffer(char *src, const char *dst, size_t size);
void copy_buffer(char *dst, const char *src, size_t size);
size_t remove_from_buffer(char *buf, char toRemove);

/* simple popen2 implementation */
pid_t process_open(const char *command, int *pout);
