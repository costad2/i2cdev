/**
 * @file i2c-dev-path.c
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Core I2C device path parsing functions
 */

#define _GNU_SOURCE 1

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include <linux/limits.h>

#include "data.h"
#include "common.h"
#include "busses.h"
#include "i2c-error.h"
#include "i2c-dev-parser.h"
#include "i2c-dev-path.h"

const char *i2cdev_disc_enum_name[] = {
    [I2CDEV_BUS] = "BUS",
    [I2CDEV_MUX] = "Mux",
    [I2CDEV_CHAN] = "Channel",
    [I2CDEV_ADDR] = "Address",
    [I2CDEV_END] = "END",
};

#define MAX_TOKEN_SIZE 8

/**
 *
 * @param[in] string The token string to parse
 * @param[in] delimiter
 * @param[out] end a pointer to the end of the string after the token
 * @return a newly allocated string making up the first part of the token else
 * NULL if not found and no string exists else duplicate the input string
 */
static char *tokenize_string(const char *string, int delimiter, const char **end)
{
    char *p_token = NULL;
    if (string == NULL || *string == '\0') {
        *end = string;
        return NULL;
    }
    p_token = strchrnul(string, delimiter);
    if (*p_token == '\0') {
        *end = p_token;
        return strdup(string);
    } else {
        char *tmp_str = strndup(string, (p_token - string));
        *end = (p_token + 1);
        return tmp_str;
    }
}

/**
 * Tokens are in the form of 'Mux.Bus' or 'Bus' where Bus and Mux are numbers
 *
 * @param [in]  token to parse
 * @param [out] descriptor pointer to store path information
 * @return negative errno on failure else the number of tokens on success
 */
static int parse_token(const char *token, dev_i2c_path_disc *descriptor)
{
    int err = 0;
    const char *end_subtoken = NULL;
    char *subtoken_start = NULL;
    char *endptr = NULL;

    assert(descriptor);

    if ((token == NULL) || (*token == '\0')) {
        descriptor->type = I2CDEV_END;
        return -1;
    }

    subtoken_start = tokenize_string(token, '.' , &end_subtoken);

    if (subtoken_start != NULL && *end_subtoken != '\0') {
        descriptor->type = I2CDEV_BUS;
        descriptor->value = BUS_NR_ANY;
        descriptor->id = strtoul(subtoken_start, &endptr, 0);
        if (*endptr != '\0') {
            /* Entire string is Not a number! */
            err = -EINVAL;
            goto free_tokens;
        }
        if (end_subtoken != NULL && *end_subtoken != '\0') {
            descriptor->value = strtoul(end_subtoken, &endptr, 0);
            if (*endptr != '\0') {
                /* Entire string is Not a number! */
                descriptor->value = -1;
                descriptor->type = I2CDEV_BUS;
            } else {
                descriptor->type = I2CDEV_MUX;
            }
        }
    } else {
        descriptor->type = I2CDEV_BUS;
        descriptor->value = BUS_NR_ANY;
        descriptor->id = strtoul(token, &endptr, 0);
        if (*endptr != '\0') {
            /* Entire string is Not a number! */
            err = -EINVAL;
            goto free_tokens;
        }
    }

free_tokens:
    free(subtoken_start);
    return err;
}

/**
 * path example: '0:0.2:0.0:1.5'
 * @param [in] path the i2c bus path to parse
 * @param [out] discp array used to store the parsed i2c bus path information
 * @return negative errno on failure else the number of tokens on success
 */
int parse_i2cdev_path(const char *path, dev_i2c_path_disc discp[])
{
    int err = 0;
    int token_cnt = 0;
    char *token = NULL;
    const char *end_string = NULL;

    end_string = path;

    /* walk through tokens */
    do {
        token = tokenize_string(end_string, ':', &end_string);

        err = parse_token(token, &discp[token_cnt]);

        if (token != NULL) {
            free(token);
            token = NULL;
        } else {
            break;
        }
        if (err == -1) {
            break;
        }
        token_cnt++;

    } while (end_string != NULL && *end_string != '\0');

    assert(token_cnt < MAX_BUS_DEPTH - 1);

    discp[token_cnt].type = I2CDEV_END;
    discp[token_cnt].value = 0;
    discp[token_cnt].id = 0;
    discp[token_cnt].depth = token_cnt;

    return token_cnt;
}

