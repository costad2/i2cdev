/**
 * @file sysfs.h
 * @copyright Violin Memory, Inc, 2014
 * @brief Functions used in reading sysfs parameters
 */

#ifndef LIB_SYSFS_H
#define LIB_SYSFS_H

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

extern const char *sysfs_mount;

/**
 * Initialize sysfs path and check that it is valid
 * @return 1 if sysfs filesystem was found else 0
 */
extern int init_sysfs(void);

/**
 * Read an attribute from sysfs
 * This function will read out the first line up to '\n' or '\0'
 * @param syspath path to read from.
 * @param attr attribute name to read from within 'syspath' path.
 * @return returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
extern char *sysfs_read_attr(const char *syspath, const char *attr);

/**
 * write up to size bytes from buffer to the file named filename.
 * The data in buffer is not necessarily a character string,
 * null character is output like any other character.
 *
 * @param filename sysfs file to write to
 * @param buffer content to write to file
 * @param size number of bytes to write
 * @return negative errno on failure else the number of bytes actually written.
 */
extern ssize_t sysfs_write_file(const char *filename, const char *buffer, size_t size);

/**
 * search for matching "key" from within the uevent file within syspath
 * @param syspath path to read from.
 * @param key attribute name to read from within 'syspath' path.
 * @return returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
extern char *sysfs_read_uevent_key_val(const char *syspath, const char *key);

/**
 * Read stat buffer from sysfs
 * @param syspath path to read from.
 * @param attr attribute name to read from within 'syspath' path.
 * @return returns a pointer to a freshly allocated stat buffer; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
extern struct stat *sysfs_read_stats(const char *syspath, const char *attr);

/**
 * Read a link from sysfs
 * @param syspath path to read from.
 * @param attr attribute name to read from within 'syspath' path.
 * @return returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
extern char *sysfs_read_link(const char *syspath, const char *attr);

/**
 * Read a device's module from sysfs
 * @param device path to read from.
 * @return returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
extern char *sysfs_read_device_module(const char *device);

/**
 * Read a device's driver from sysfs
 * @param device path to read from.
 * @return returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
extern char *sysfs_read_device_driver(const char *device);

/**
 * Read a device's subsystem from sysfs
 * @param device path to read from.
 * @return returns a pointer to a freshly allocated string; free it yourself.
 * If the file doesn't exist or can't be read, NULL is returned.
 */
extern char *sysfs_read_device_subsystem(const char *device);

#endif /* !LIB_SYSFS_H */
