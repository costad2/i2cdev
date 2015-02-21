/**
 * @file common.h
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Common helper functions
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif /* _GNU_SOURCE */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

/* These are general purpose functions. They allow you to use variable-
 length arrays, which are extended automatically. A distinction is
 made between the current number of elements and the maximum number.
 You can only add elements at the end. Primitive, but very useful
 for internal use. */

#ifndef COUNT_OF
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#endif

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */
#ifndef __stringify
#define __stringify_1(x...)     #x
#define __stringify(x...)       __stringify_1(x)
#endif

#ifndef container_of
#define container_of(ptr, type, member)                         \
    ({const __typeof__( ((type *)0)->member ) *__mptr = (ptr);  \
    (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

/* Evaluate EXPRESSION, and repeat as long as it returns -1 with `errno'
 set to EINTR.  */
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)          \
({                                              \
    long int __result;                          \
    do __result = (long int) (expression);      \
    while (__result == -1L && errno == EINTR);  \
   __result;                                    \
})
#endif

static inline void p_safe_free(void **pointerp)
{
    /* check if pointer to pointer is null */
    if (pointerp != NULL && *pointerp != NULL) {
        free(*pointerp); /* actually deallocate memory */
        *pointerp = NULL; /* null terminate */
    }
}

static inline int close_fd(int fd)
{
    const int fsync_fail = (fsync(fd) != 0);
    const int close_fail = (close(fd) != 0);
    if (fsync_fail || close_fail) {
        return EOF;
    }
    return 0;
}

static inline FILE *concat_fopen(char *s1, char *s2, char *mode)
{
    char str[strlen(s1) + strlen(s2) + 1];
    strcpy(str, s1);
    strcat(str, s2);
    return fopen(str, mode);
}

static inline char *strim(char *string)
{
    char *ret;

    if (!string) {
        return NULL;
    }
    while (*string) {
        if (!isspace(*string)) {
            break;
        }
        string++;
    }
    ret = string;

    string = ret + strlen(ret) - 1;
    while (string > ret) {
        if (!isspace(*string)) {
            break;
        }
        string--;
    }
    string[1] = 0;

    return ret;
}

static inline int has_text(const char *text)
{
    if (!text) {
        return 0;
    }
    while (*text) {
        if (!isspace(*text)) {
            return 1;
        }
        text++;
    }
    return 0;
}

/**
 * sysfs_streq - return true if strings are equal, modulo trailing newline
 * @s1: one string
 * @s2: another string
 *
 * This routine returns true iff two strings are equal, treating both
 * NUL and newline-then-NUL as equivalent string terminations.  It's
 * geared for use with sysfs input strings, which generally terminate
 * with newlines but are compared against values without newlines.
 */
static inline bool sysfs_streq(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }

    if (*s1 == *s2) {
        return true;
    }else if (!*s1 && *s2 == '\n' && !s2[1]) {
        return true;
    } else if (*s1 == '\n' && !s1[1] && !*s2) {
        return true;
    }
    return false;
}

void dev_malloc_array(void *list, int *num_el, int *max_el,
        size_t el_size);
void dev_free_array(void *list, int *num_el, int *max_el);
void dev_add_array_el(const void *el, void *list, int *num_el,
        int *max_el, size_t el_size);
void dev_add_array_els(const void *els, int nr_els, void *list,
        int *num_el, int *max_el, size_t el_size);

#endif /* _COMMON_H_ */
