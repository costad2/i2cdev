/**
 * @file sysfs.c
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Functions used in reading sysfs parameters
 */

#define _GNU_SOURCE 1

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/queue.h>

#include <linux/limits.h> /* for PATH_MAX */
#include <linux/magic.h>

#include "common.h"
#include "sysfs.h"
#include "data.h"

#ifndef SYSFS_MAGIC
#define SYSFS_MAGIC 0x62656572
#endif // SYSFS_MAGIC

#define SYSFS_PATH  "/sys"

const char *sysfs_mount = NULL;

/* Override default sysfs path for testing only! */
/* Use this to override the default sysfs location and dir checks for testing. */
#ifdef  SYSFS_PATH_DEBUG
static const bool sysfs_path_debugging = true;
#else
static const bool sysfs_path_debugging = false;
#endif /* SYSFS_PATH_DEBUG */

#if defined(SYSFS_PATH_DEBUG) && defined(SYSFS_OVERRIDE_STRING)
#define SYSFS_PATH_STRING   __stringify(SYSFS_OVERRIDE_STRING)
#else
#define SYSFS_PATH_STRING   SYSFS_PATH
#endif /* SYSFS_PATH_DEBUG && SYSFS_OVERRIDE_STRING */

static const char *sysfs_mount_path = SYSFS_PATH_STRING;

/**
 * Initialize sysfs path and check that it is valid
 * @return 1 if sysfs filesystem was found, 0 otherwise
 */
int init_sysfs(void)
{
    struct statfs statfsbuf;
    struct stat statbuf;
    if (sysfs_mount == NULL) {
        /* This path is set for debugging purposes */
        if (sysfs_path_debugging == true) {
            fprintf(stderr,
                    "WARNING: This build has been compiled with sysfs path override enabled!\n");
            if (stat(sysfs_mount_path, &statbuf) < 0) {
                fprintf(stderr,
                        "ERROR: could not find valid sysfs path \"%s\" - %s\n",
                        sysfs_mount_path, strerror(errno));
            } else if (S_ISDIR(statbuf.st_mode)) {
                sysfs_mount = sysfs_mount_path;
                fprintf(stderr, "WARNING: sysfs path has been set to: \"%s\"\n",
                        sysfs_mount_path);
                return 1;
            } else {
                fprintf(stderr, "ERROR: invalid sysfs path \"%s\" - %s\n",
                        sysfs_mount_path, strerror(ENOTDIR));
            }
            sysfs_mount = NULL;
            return 0;
        } else {
            if (((statfs(sysfs_mount_path, &statfsbuf)) < 0)
                    || (statfsbuf.f_type != SYSFS_MAGIC)) {
                sysfs_mount = NULL;
                return 0;
            } else {
                sysfs_mount = sysfs_mount_path;
                return 1;
            }
        }
    }
    return 1;
}

/*
 * Read an attribute from sysfs
 * reads out the first (usually only) one line up to '\n' or '\0'
 * Returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
char *sysfs_read_attr(const char *syspath, const char *attr)
{
    char path[PATH_MAX];
    char buf[NAME_MAX];
    char *buf_end = NULL;
    char *line = NULL;
    FILE *fp = NULL;
    ssize_t slen = 0;

    slen = snprintf(path, sizeof(path), "%s/%s", syspath, attr);
    if (slen <= 0 || slen == (ssize_t) sizeof(path)) {
        return NULL;
    } else {
        path[slen] = '\0';
    }

    if ((fp = fopen(path, "r")) != NULL) {
        line = fgets(buf, sizeof(buf), fp);
        fclose(fp);
    }
    if (line == NULL){
        return NULL;
    }
    /* Last byte is a '\n'; chop that off */
    buf_end = strchrnul(line, '\n');
    *buf_end = '\0';
    if (*line == '\0') {
        return NULL;
    }
    return strdup(line);
}

#define MAX_SYSFS_WRITE_SIZE 4096

ssize_t sysfs_write_file(const char *filename, const char *buffer, size_t size)
{
    int ret = 0, ret1 = 0;
    int fd = -1;
    ssize_t nbytes = 0;

    if (size > MAX_SYSFS_WRITE_SIZE) {
        return -EFBIG;
    }
    fd = open(filename, O_WRONLY);
    if (fd < 0) {
        ret = -errno;
    } else {
        nbytes = TEMP_FAILURE_RETRY(write(fd, buffer, size));
        if (nbytes < 0) {
            ret = -errno;
        }
        ret1 = TEMP_FAILURE_RETRY(close(fd));
    }
    return ((ret < 0) ? ret : ((ret1 < 0) ? ret1 : nbytes));
}

/*
 * Read and collect an array of uevent strings from sysfs
 * returns 0 on success else -errno.
 */
static int sysfs_read_uevents(char ***uevents, int *count, int *max,
        const char *syspath)
{
    char path[PATH_MAX];
    FILE *fp = NULL;
    char *line = NULL;
    size_t line_len = 0;
    ssize_t len = 0;

    len = snprintf(path, sizeof(path), "%s/%s", syspath, "uevent");
    if (len <= 0 || len == (ssize_t) sizeof(path)) {
        return -errno;
    } else {
        path[len] = '\0';
    }

    if ((fp = fopen(path, "r")) == NULL) {
        return -errno;
    }

    len = 0;
    while ((len = getline(&line, &line_len, fp)) != EOF) {
        char *uevent_val = NULL;
        char *p_line = NULL;
        if (line == NULL) {
            continue;
        }
        /* Last byte is a '\n'; chop that off */
        p_line = strchrnul(line, '\n');
        uevent_val = strndup(line, (p_line - line));

        int new_max_el;

        if (*count + 1 > *max) {
            new_max_el = *max *= 2;
            uevents = realloc(uevents, (size_t) new_max_el * sizeof(char *));
            if (!uevents)
                perror("realloc() Allocating new elements");
            *max = new_max_el;
        }
        memcpy(((char *) uevents) + *count * (int) sizeof(char *), uevent_val, (size_t) sizeof(char *));
        (*count)++;
        free(uevent_val);
    }
    free(line);
    fclose(fp);
    return (0);
}

char *sysfs_read_uevent_key_val(const char *syspath, const char *key)
{
    char *uevent = NULL;
    char *value = NULL;
    int pos = 0;
    int ret = 0;
    char **uevents = NULL;
    int uevent_count = 0;
    int uevent_max = 0;

    ret = sysfs_read_uevents(&uevents, &uevent_count, &uevent_max, syspath);
    if (ret < 0) {
        return NULL;
    }
    for (pos = 0; (uevents[pos] != NULL); ++pos) {
        uevent = uevents[pos];
        if ((strncmp(uevent, key, strlen(key))) == 0) {
            value = strdup(uevent);
            goto exit_free;
        }
    }

exit_free:
    if (uevents) {
        for (; (*uevents != NULL); uevents++) {
            if (*uevents) {
                free(*uevents);
            }
        }
    }
    if (ret < 0) {
        if (value) {
            free(value);
        }
        return NULL;
    }
    return value;
}

struct stat *sysfs_read_stats(const char *syspath, const char *attr)
{
    char path[PATH_MAX];
    struct stat statbuf;
    struct stat *sp = NULL;
    ssize_t len = 0;

    len = snprintf(path, sizeof(path), "%s/%s", syspath, attr);
    if (len <= 0 || len == (ssize_t) sizeof(path)) {
        return NULL;
    }
    path[len] = '\0';

    if (stat(path, &statbuf) < 0) {
        return NULL;
    } else {
        sp = malloc(sizeof(struct stat));
        if (sp == NULL) {
            return NULL;
        }
        memcpy(sp, &statbuf, sizeof(struct stat));
    }
    return (sp);
}

static char *readlink_internal(const char *filename)
{
    char buffer[PATH_MAX];
    char *p_link = NULL;
    ssize_t len = 0;

    len = readlink(filename, buffer, sizeof(buffer));
    if (len <= 0 || len == (ssize_t) sizeof(buffer)) {
        return NULL;
    }
    buffer[len] = '\0';
    p_link = strdup(buffer);
    if (p_link == NULL) {
        return NULL;
    } else {
        return p_link;
    }
}

/*
 * Read a link from sysfs
 * Returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
char *sysfs_read_link(const char *syspath, const char *attr)
{
    char path[PATH_MAX];

    if (!syspath) {
        return NULL;
    }
    if (!attr) {
        attr = "";
    }
    if (strlen(attr) == 0) {
        if (snprintf(path, sizeof(path), "%s", syspath) < 0) {
            return NULL;
        }
    } else {
        if (snprintf(path, sizeof(path), "%s/%s", syspath, attr) < 0) {
            return NULL;
        }
    }
    return readlink_internal(path);
}

/* From a sysfs device path, return the module name, or NULL */
char *sysfs_read_device_module(const char *syspath)
{
    char path[PATH_MAX];
    char path_target[PATH_MAX];
    char *module = NULL;
    char *pbname = NULL;
    ssize_t len = 0;
    struct stat st;

    if (snprintf(path, sizeof(path), "%s/driver/module", syspath) < 0)
        return NULL;
    if (lstat(path, &st) < 0) {
        return NULL;
    }
    len = readlink(path, path_target, sizeof(path_target));
    if (len <= 0 || len == (ssize_t) sizeof(path_target)) {
        return NULL;
    }
    path_target[len] = '\0';

    if (*path_target == '\0') {
        return NULL;
    }
    pbname = basename(path_target);
    if (pbname != NULL && *pbname != '\0') {
        module = strdup(pbname);
        if (module == NULL) {
            return NULL;
        } else {
            return module;
        }
    } else {
        return NULL;
    }
}

/* From a sysfs device path, return the driver name, or NULL */
char *sysfs_read_device_driver(const char *syspath)
{
    char path[PATH_MAX];
    char path_target[PATH_MAX];
    char *driver = NULL;
    char *pbname = NULL;
    ssize_t len = 0;
    struct stat st;

    if (snprintf(path, sizeof(path), "%s/driver", syspath) < 0) {
        return NULL;
    }
    if (lstat(path, &st) < 0) {
        return NULL;
    }
    len = readlink(path, path_target, sizeof(path_target));
    if (len <= 0 || len == (ssize_t) sizeof(path_target)) {
        return NULL;
    }
    path_target[len] = '\0';

    if (*path_target == '\0') {
        return NULL;
    }
    pbname = basename(path_target);
    if (pbname != NULL && *pbname != '\0') {
        driver = strdup(pbname);
        if (driver == NULL) {
            return NULL;
        } else {
            return driver;
        }
    } else {
        return NULL;
    }
}

/* From a sysfs device path, return the subsystem name, or NULL */
char *sysfs_read_device_subsystem(const char *syspath)
{
    char path[PATH_MAX];
    char path_target[PATH_MAX];
    char *subsystem = NULL;
    char *pbname = NULL;
    ssize_t len = 0;
    struct stat st;

    if (snprintf(path, sizeof(path), "%s/subsystem", syspath) < 0) {
        return NULL;
    }
    if (lstat(path, &st) < 0) {
        return NULL;
    }
    len = readlink(path, path_target, sizeof(path_target));
    if (len <= 0 || len == (ssize_t) sizeof(path_target)) {
        return NULL;
    }
    path_target[len] = '\0';
    if (*path_target == '\0') {
        return NULL;
    }
    pbname = basename(path_target);
    if (pbname != NULL && *pbname != '\0') {
        subsystem = strdup(pbname);
        if (subsystem == NULL) {
            return NULL;
        } else {
            return subsystem;
        }
    } else {
        return NULL;
    }
}

