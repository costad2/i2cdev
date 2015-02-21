/**
 * @file common.c
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Common Functions
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "common.h"
#include "i2c-error.h"

#define A_BUNCH 16

void dev_malloc_array(void *list, int *num_el, int *max_el, size_t el_size)
{
    void **my_list = (void **) list;

    *my_list = malloc(el_size * A_BUNCH);
    if (!*my_list)
        dev_fatal_error(__func__, "Allocating new elements");
    *max_el = A_BUNCH;
    *num_el = 0;
}

void dev_free_array(void *list, int *num_el, int *max_el)
{
    void **my_list = (void **) list;
    p_safe_free(my_list);
    *num_el = 0;
    *max_el = 0;
}

void dev_add_array_el(const void *el, void *list, int *num_el, int *max_el,
        size_t el_size)
{
    int new_max_el;
    void **my_list = (void *) list;
    if (*num_el + 1 > *max_el) {
        new_max_el = *max_el + A_BUNCH;
        *my_list = realloc(*my_list, (size_t) new_max_el * el_size);
        if (!*my_list)
            dev_fatal_error(__func__, "Allocating new elements");
        *max_el = new_max_el;
    }
    memcpy(((char *) *my_list) + *num_el * (int) el_size, el, (size_t) el_size);
    (*num_el)++;
}

void dev_add_array_els(const void *els, int nr_els, void *list, int *num_el,
        int *max_el, size_t el_size)
{
    int new_max_el;
    void **my_list = (void *) list;
    if (*num_el + nr_els > *max_el) {
        new_max_el = (*max_el + nr_els + A_BUNCH);
        new_max_el -= new_max_el % A_BUNCH;
        *my_list = realloc(*my_list, (size_t) new_max_el * el_size);
        if (!*my_list)
            dev_fatal_error(__func__, "Allocating new elements");
        *max_el = new_max_el;
    }
    memcpy(((char *) *my_list) + *num_el * (int) el_size, els,
            (size_t) ((int) el_size * nr_els));
    *num_el += nr_els;
}

