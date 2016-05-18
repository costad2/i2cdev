/**
 * @file init.c
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief libi2cdev initialization and cleanup routines
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <linux/limits.h> /* for PATH_MAX */

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <error.h>
#include <limits.h>
#include <ctype.h>
#include <error.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/stat.h>

#ifdef USE_LIBKMOD
#include <libkmod.h>
#endif

#include "i2cdiscov.h"
#include "i2c-bus-lists.h"
#include "i2c-error.h"
#include "i2c-dev-path.h"
#include "i2c-dev-parser.h"

#include "common.h"
#include "sysfs.h"
#include "access.h"
#include "data.h"
#include "busses.h"

#ifndef OVERRIDE_ETCDIR
#define ETCDIR "/etc"
#else
#ifndef ETCDIR_PATH
#define ETCDIR_PATH		/etc
#endif
#define ETCDIR		__stringify(ETCDIR_PATH)
#endif /* !OVERRIDE_ETCDIR */

#define DEFAULT_CONFIG_FILE	ETCDIR"/i2cdiscov.conf"
#define DEFAULT_CONFIG_DIR	ETCDIR"/i2cdiscov.d"

#define BUFLEN 1024
#define I2C_DEV_MOD_NAME "i2c_dev"

int i2c_dev_verbose = 0; /* Show detailed information */
int i2cdev_rescan_count = 0;

const char *stdin_config_file_name = NULL;

char **dev_config_files = NULL;
int dev_config_files_count = 0;
int dev_config_files_max = 0;

dev_config_chip *dev_config_chips = NULL;
int dev_config_chips_count = 0;
int dev_config_chips_max = 0;

static dev_config_chip_head dev_config_list_head;
dev_config_chip_head *p_dev_config_list_head = NULL;

static dev_bus_adapter_head dev_bus_list_head;
dev_bus_adapter_head *dev_bus_list_headp = NULL;

dev_bus_adapter **adapter_global_array = NULL;
size_t adapter_global_count = 0;
size_t device_global_count = 0;

static int parse_config_file(FILE *input, const char *name);

/**
 * This is in the form of "pca9541-i2c-1:0.1-0x73"
 * See doc/libi2cdev-api and doc/i2cinit.cfg for examples
 * and device configuration files.
 * @param input file to parse
 * @param name name of the file
 * @return 0 on success else -errno
 */
static int parse_config(FILE *input, const char *name)
{
    char *name_copy = NULL;
    int new_max_el;

    if (name != NULL) {
        /* Record configuration file name for error reporting */
        name_copy = strdup(name);
        if (name_copy == NULL) {
            return -ENOMEM;
        }

        if (dev_config_files_count + 1 > dev_config_files_max) {
            new_max_el = dev_config_files_max *= 2;
            dev_config_files = realloc(dev_config_files, (size_t) new_max_el * sizeof(char *));
            if (!dev_config_files)
                perror("realloc() Allocating new elements");
            dev_config_files_max = new_max_el;
        }
        memcpy(((char *) dev_config_files) + dev_config_files_count * (int) sizeof(char *), name_copy, (size_t) sizeof(char *));
        (dev_config_files_count)++;

    } else if (stdin_config_file_name != NULL) {
        /* Record configuration file name for error reporting */
        name_copy = strdup(stdin_config_file_name);
        if (name_copy == NULL) {
            return -ENOMEM;
        }
        if (dev_config_files_count + 1 > dev_config_files_max) {
            new_max_el = dev_config_files_max *= 2;
            dev_config_files = realloc(dev_config_files, (size_t) new_max_el * sizeof(char *));
            if (!dev_config_files)
                perror("realloc() Allocating new elements");
            dev_config_files_max = new_max_el;
        }
        memcpy(((char *) dev_config_files) + dev_config_files_count * (int) sizeof(char *), name_copy, (size_t) sizeof(char *));
        (dev_config_files_count)++;
    } else {
        name_copy = NULL;
    }

    return parse_config_file(input, name_copy);
}

/* if last byte is a '\n'; chop that off
 * also chop off anything after a comment '#'
 */
static char *clean_line(const char *line)
{
    char buffer[BUFLEN];
    char *begin = NULL;
    char *end = NULL;

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    begin = &buffer[0];

    /* Trim leading spaces */
    while ((isblank(*begin)) && (*begin != '\0')) {
        begin++;
    }

    if (*begin == '\0') {
        return NULL;
    }

    /* Find the end of the token.  */
    end = strpbrk(begin, "#\t \n");
    if (end != NULL) {
        *end = '\0';
    }
    if (*begin == '\0') {
        return NULL;
    }
    return strdup(begin);
}

static void free_config_chip(dev_config_chip *chip)
{
    if (chip != NULL) {
        free(chip->prefix);
        free(chip);
    }
}

/**
 * Read a config file
 * @param input File pointer to the configuration file
 * @param name The name of the file @note the name passed here must persist through the life of the library
 * @return If the file doesn't exist or can't be read, -errno is returned else 0
 */
static int parse_config_file(FILE *input, const char *name)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read_path;
    int line_count = 0;
    int ret = 0;
    dev_config_chip *temp_chip = NULL;

    if (!input)
        return -ENOENT;

    while ((read_path = getline(&line, &len, input)) != -1) {
        char *line_entry = NULL;
        char *linep = NULL;
        char *dash = NULL;
        char *bus_path = NULL;
        char *bus_type = NULL;
        char *chip_type_path = NULL;
        dev_config_chip *chip = NULL;
        bool free_chip = true;
        line_count++;

        if (line == NULL) {
            continue;
        }

        line_entry = clean_line(line);

        if (line_entry == NULL) {
            continue;
        }

        chip = calloc(1, sizeof(*chip));
        if (chip == NULL) {
            goto while_free;
        }

        chip->line.filename = NULL;

        linep = line_entry;

        if (!(dash = strchr(linep, '-'))) {
            goto while_free;
        }

        chip->prefix = strndup(linep, dash - linep);
        if (chip->prefix == NULL) {
            goto while_free;
        }
        linep = dash + 1;

        if (!(dash = strchr(linep, '-'))) {
            goto while_free;
        }
        bus_type = strndup(linep, dash - linep);
        if (bus_type == NULL) {
            goto while_free;
        }
        linep = dash + 1;

        if (!(dash = strchr(linep, '-'))) {
            goto add_bus;
        }
        bus_path = strndup(linep, dash - linep);
        if (bus_path == NULL) {
            goto while_free;
        }
        linep = dash + 1;

        if (linep != NULL && *linep != '\0') {
            chip->address = strtoul(linep, &dash, 0);
        }

        if (asprintf(&chip_type_path, "%s-%s", bus_type, bus_path) < 0) {
            goto while_free;
        }

        dev_parse_bus_id(chip_type_path, &chip->bus);

add_bus:

        free_chip = false;
        chip->line.filename = name;
        chip->line.lineno = line_count;

        temp_chip = SLIST_FIRST(p_dev_config_list_head);
        if (temp_chip == NULL) {
            SLIST_INSERT_HEAD(p_dev_config_list_head, chip, node);
        } else {
            dev_config_chip *chip_tmp_p = NULL;
            while (((chip_tmp_p = SLIST_NEXT(temp_chip, node)) != NULL)
                    && (chip_tmp_p != temp_chip)) {
                temp_chip = chip_tmp_p;
            }
            SLIST_INSERT_AFTER(temp_chip, chip, node);
        }
while_free:
        if (free_chip == true) {
            free_config_chip(chip);
        }
        free(line_entry);
        free(bus_type);
        free(chip_type_path);
        free(bus_path);
        free(line);
        line = NULL;
    }
    free(line);
    return ret;
}

/**
 * Try to load the i2c_dev kernel module. Do nothing, if module is already loaded.
 * Returns 1 on success, 0 otherwise.
 */
int try_load_i2c_dev_mod(void)
{
    int err = 0, loaded = 0;
    char errbuf[BUFLEN] = "";
#ifdef _LIBKMOD_
    int flags = 0;
    struct kmod_ctx *ctx;
    struct kmod_list *l, *list = NULL;

    ctx = kmod_new(NULL, NULL);
    if (!ctx) {
        snprintf(errbuf, BUFLEN, "kmod_new() failed!");
        goto done;
    }
    if (kmod_module_new_from_lookup(ctx, I2C_DEV_MOD_NAME, &list) < 0 || list == NULL) {
        snprintf(errbuf, BUFLEN, I2C_DEV_MOD_NAME " module lookup failed");
        goto ctx_unref;
    }

    flags |= KMOD_PROBE_APPLY_BLACKLIST_ALIAS_ONLY;
    kmod_list_foreach(l, list) {
        struct kmod_module *mod = kmod_module_get_module(l);
        err = kmod_module_probe_insert_module(mod, flags, NULL, NULL, NULL, NULL);
        if (err == -ENOENT) {
            snprintf(errbuf, BUFLEN,
                    "unknown symbol in module \"%s\", or unknown parameter (see dmesg)",
                    kmod_module_get_name(mod));
        } else if (err < 0) {
            snprintf(errbuf, BUFLEN, "(module %s): %s",
                    kmod_module_get_name(mod), strerror(-err));
        } else {
            kmod_module_unref(mod);
            ++loaded;
            break;
        }
        kmod_module_unref(mod);
    }

//list_unref:
    kmod_module_unref_list(list);
    ctx_unref:
    kmod_unref(ctx);
#else
    struct stat st;

    err = stat("/sys/class/i2c-dev", &st);
    if (err < 0) {
        if (errno != ENOENT) {
            err = -errno;
            goto done;
        } else {
            err = 0;
        }
    } else {
        return 1;
    }
    err = system("modprobe " I2C_DEV_MOD_NAME);
    if (err < 0) {
        snprintf(errbuf, BUFLEN, "failed to execute modprobe command");
    } else if (err > 0) {
        snprintf(errbuf, BUFLEN, "modprobe command exited with code %d",
                WEXITSTATUS(err));
    } else {
        ++loaded;
        goto done;
    }
#endif
    if (errbuf[0] != '\0')
        devi2c_err(NULL, "Failed to load required " I2C_DEV_MOD_NAME
        " kernel module: %s", errbuf);
done:
    return loaded;
}

static int config_file_filter(const struct dirent *entry)
{
    return entry->d_name[0] != '.'; /* Skip hidden files */
}

static int add_config_from_dir(const char *dir)
{
    int count = 0, res = 0, i = 0;
    struct dirent **namelist;

    count = scandir(dir, &namelist, config_file_filter, alphasort);
    if (count < 0) {
        res = -errno;
        /* Do not return an error if directory does not exist */
        if (res == -ENOENT) {
            return 0;
        }
        return res;
    }

    for (res = 0, i = 0; !res && i < count; i++) {
        int len = 0;
        char path[PATH_MAX];
        FILE *input = NULL;
        struct stat st;

        len = snprintf(path, sizeof(path), "%s/%s", dir, namelist[i]->d_name);
        if (len < 0 || len >= (int) sizeof(path)) {
            res = -errno;
            break;
        }

        /* Only accept regular files */
        if (stat(path, &st) < 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        input = fopen(path, "r");
        if (input) {
            res = parse_config(input, path);
            fclose(input);
        } else {
            res = -errno;
        }
    }

    /* Free memory allocated by scandir() */
    for (i = 0; i < count; i++) {
        free(namelist[i]);
    }
    free(namelist);

    return res;
}

void free_dev_chip_list(dev_chip_head *list)
{
    dev_chip *chip = NULL;
    if (list != NULL) {
        while ((chip = SLIST_FIRST(list)) != NULL) {
            SLIST_REMOVE_HEAD(list, node);
            dev_free_chip(&chip);
        }
    }
}

static void free_dev_client_list(dev_client_user_list *list)
{
    struct dev_client_list *client_list = NULL;
    while ((list != NULL) && (client_list = LIST_FIRST(list)) != NULL) {
        LIST_REMOVE(client_list, node);
        if (client_list->client) {
            client_list->client->adapter = NULL;
        }
        free(client_list);
    }
}

void free_adapter_val(dev_bus_adapter **adapter)
{
    /* check if pointer to pointer is null */
    if (adapter != NULL && *adapter != NULL) {

        if (LIST_EMPTY(&(*adapter)->children)) {

            free_dev_client_list(&((*adapter)->user_clients));

            if ((adapter) && (*adapter)) {

                if ((*adapter)->i2c_adapt.fd >= 0) {
                    close((*adapter)->i2c_adapt.fd);
                }

                free_dev_chip_list(&((*adapter)->clients));

                dev_free_bus_id(&(*adapter)->bus);
                if ((*adapter)->path != NULL) {
                    free((*adapter)->path);
                }
                (*adapter)->path = NULL;
                if ((*adapter)->name != NULL) {
                    free((*adapter)->name);
                }
                (*adapter)->name = NULL;
                if ((*adapter)->devpath != NULL) {
                    free((*adapter)->devpath);
                }
                (*adapter)->devpath = NULL;
                if ((*adapter)->subsystem != NULL) {
                    free((*adapter)->subsystem);
                }
                (*adapter)->subsystem = NULL;
                if ((*adapter)->parent_name != NULL) {
                    free((*adapter)->parent_name);
                }
                (*adapter)->parent_name = NULL;
            }

            if (adapter != NULL && *adapter != NULL) {
                free(*adapter); /* actually deallocate memory */
                *adapter = NULL; /* null terminate */
            }
        }
    }
}

void free_adapter_list(dev_bus_adapter_head *list)
{
    dev_bus_adapter *adapter = NULL;
    if (list != NULL) {
        while ((adapter = LIST_FIRST(list)) != NULL) {
            if (!LIST_EMPTY(&adapter->children)) {
                free_adapter_list(&adapter->children);
            }
            bus_list_remove(adapter);
            free_adapter_val(&adapter);
        }
    }
}

static void free_dev_config_chip(dev_config_chip *chip)
{
    if (chip != NULL) {
        if (chip->prefix != NULL) {
            free(chip->prefix);
        }
        chip->prefix = NULL;
        dev_free_bus_id(&chip->bus);
    }
}

static bool init_once = false;

/* Ideally, initialization and configuraton file loading should be exposed
 separately, to make it possible to load several configuration files. */
int i2cdev_init(FILE *input)
{
    int res = 0;
    if (get_libi2cdev_state() == LIB_SMB_READY) {
        return 0;
    }
    set_libi2cdev_state(LIB_SMB_BUSY);

    if (init_once == false) {

        LIST_INIT(&dev_bus_list_head);
        dev_bus_list_headp = &dev_bus_list_head;

        SLIST_INIT(&dev_config_list_head);
        p_dev_config_list_head = &dev_config_list_head;

        if (!init_sysfs()) {
            set_libi2cdev_state(LIB_SMB_NOT_READY);
            return -ENOENT;
        }

        if (input != NULL) {
            res = parse_config(input, NULL);
            if (res < 0) {
                goto exit_cleanup;
            }
        } else {
            const char* name = NULL;

            /* No configuration provided, use default */
            input = fopen(name = DEFAULT_CONFIG_FILE, "r");
            if (input != NULL) {
                res = parse_config(input, name);
                fclose(input);
                if (res < 0) {
                    goto exit_cleanup;
                }
            } else if (errno != ENOENT) {
                res = -errno;
                goto exit_cleanup;
            }
            /* Also check for files in default directory */
            res = add_config_from_dir(DEFAULT_CONFIG_DIR);
            if (res < 0) {
                goto exit_cleanup;
            }
        }
        init_once = true;
    }

    if ((res = gather_i2c_dev_busses()) < 0) {
        goto exit_cleanup;
    }
    set_libi2cdev_state(LIB_SMB_READY);

    // TODO: possibly move out of the init function (requires initialization aka libi2cdev_state = LIB_SMB_READY)
    dev_for_all_chips_match_config(p_dev_config_list_head);

    return 0;

exit_cleanup:
	i2cdev_cleanup();
    return res;
}

int i2cdev_rescan(void)
{
    int res = 0;

    if (get_libi2cdev_state() == LIB_SMB_UNINIIALIZED) {
        return i2cdev_init(NULL);
    } else if (get_libi2cdev_state() == LIB_SMB_READY) {
        devi2c_debug(NULL, "Rescanning I2C bus structure - total previous rescan count = %d", i2cdev_rescan_count);
        set_libi2cdev_state(LIB_SMB_BUSY);

        free_adapter_list(dev_bus_list_headp);

        if (adapter_global_array != NULL) {
            free(adapter_global_array);
            adapter_global_count = 0;
            adapter_global_array = NULL;
        }
        if ((res = gather_i2c_dev_busses()) < 0) {
            goto exit_cleanup;
        }
        i2cdev_rescan_count++;
        libi2cdev_clear_invalidate_flag();
        set_libi2cdev_state(LIB_SMB_READY);
        return 0;
    } else if (get_libi2cdev_state() == LIB_SMB_BUSY || get_libi2cdev_state() == LIB_SMB_UNKNOWN) {
        return -EBUSY;
    }

exit_cleanup:
    i2cdev_cleanup();
    return res;
}

int dev_remove_sysfs_i2c_device(const struct dev_i2c_board_info *info)
{
    char path[PATH_MAX];
    char check_path[PATH_MAX];
    char buffer[NAME_MAX];
    int ret = 0, count = 0;
    struct stat st;
    dev_bus_adapter *adapter = NULL;
    int nbytes = 0;

    if ((!info) || (info->path[0] == '\0')) {
        return -EINVAL;
    }
    adapter = dev_i2c_lookup_i2c_bus(info->path);

    if (!adapter) {
        return -ENODEV;
    }

    snprintf(check_path, sizeof(check_path), "%s/%hd-%04hx", adapter->devpath,
            adapter->nr, info->addr);

    devi2c_debug(NULL, "Checking if device exists: %s", check_path);

    if (stat(check_path, &st) < 0) {
        ret = -errno;
        goto exit_free;
    }

    if (snprintf(path, sizeof(path), "%s/%s", adapter->devpath, "delete_device") < 0) {
        ret = -errno;
        goto exit_free;
    }

    if ((count = snprintf(buffer, sizeof(buffer), "0x%02hx", info->addr)) < 0) {
        ret = -errno;
        goto exit_free;
    }
    nbytes = sysfs_write_file(path, buffer, count);
    if (nbytes < 0) {
        ret = nbytes;
        devi2c_warn(NULL, "Failed to write sysfs delete_device! - %s", strerror(-ret));
    }

exit_free:
    return (ret);
}

int dev_new_sysfs_i2c_device(const struct dev_i2c_board_info *info)
{
    char path[NAME_MAX];
    char check_path[NAME_MAX];
    char buffer[NAME_MAX];
    int ret = 0, count = 0;
    struct stat st;
    dev_bus_adapter *adapter = NULL;
    int nbytes = 0;

    if ((!info) || (info->path[0] == '\0')) {
        return -EINVAL;
    }
    adapter = dev_i2c_lookup_i2c_bus(info->path);

    if (!adapter) {
        return -ENODEV;
    }

    snprintf(check_path, sizeof(check_path), "%s/%hd-%04hx", adapter->devpath,
            adapter->nr, info->addr);

    devi2c_debug(NULL, "Checking if device exists: %s", check_path);

    ret = stat(check_path, &st);
    if (ret < 0) {
        if (errno != ENOENT) {
            ret = -errno;
            goto exit_free;
        } else {
            ret = 0;
        }
    } else {
        goto exit_free;
    }

    snprintf(path, sizeof(path), "%s/%s", adapter->devpath, "new_device");

    count = snprintf(buffer, sizeof(buffer), "%s 0x%02hx", info->name, info->addr);

    nbytes = sysfs_write_file(path, buffer, count);
    if (nbytes < 0) {
        ret = nbytes;
        devi2c_warn(NULL, "Failed to write sysfs new_device! - %s", strerror(-ret));
    }

exit_free:
    return (ret);
}

static int remove_sysfs_i2c_device(dev_chip *chip)
{
    FILE *f = NULL;
    char path[PATH_MAX];
    int res = 0, ret = 0;
    dev_bus_adapter *adapter = NULL;
    struct stat st;

    if (!chip) {
        return -ENODEV;
    }

    adapter = chip->adapter;

    if (!adapter) {
        return -ENODEV;
    }

    if (stat(chip->devpath, &st) < 0) {
        return -errno;
    }

    snprintf(path, sizeof(path), "%s/%s", adapter->devpath, "delete_device");

    if (stat(path, &st) < 0) {
        return -errno;
    } else {
        if (!(st.st_mode & S_IWUSR)) {
            return (-EACCES);
        }
    }

    /* Close on Exit for kernel sysfs write calls */
    if (NULL != (f = fopen(path, "w"))) {
        res = fprintf(f, "0x%02hx", chip->addr);
        if (res < 0) {
            ret = -errno;
        }
        res = fclose(f);
        if (ret < 0) {
            return (ret);
        } else {
            ret = res;
        }
    } else {
        ret = -errno;
    }
	return (ret);
}

int initialize_all_config_chips(void)
{
    int ret = 0;
    dev_config_chip *comp = NULL;

    dev_for_all_chips_match_config(p_dev_config_list_head);

    SLIST_FOREACH(comp, p_dev_config_list_head, node) {

        if ((comp->adapter_available == true) && (comp->matched == false)) {

            struct dev_i2c_board_info info = {
                .addr = comp->address,
                .flags = 0,
                .name = comp->prefix,
                .path = comp->bus.path,
            };

            struct dev_i2c_board_info *p_info = &info;

            if (i2c_dev_verbose) {
                devi2c_debug(NULL, "Found chip in configuration spec not initialized:");
                print_config_chip_data(comp);
            }

            ret = dev_new_sysfs_i2c_device(p_info);
            if (ret < 0) {
            	devi2c_warn(NULL, "Failed to add i2c device: \'%s\' - %s", p_info->name, strerror(-ret));
            } else {
                ret = i2cdev_rescan();
                if (ret < 0) {
                	devi2c_warn(NULL, "Failed to rescan i2c devices! - %s", strerror(-ret));
                    return ret;
                }
                dev_for_all_chips_match_config(p_dev_config_list_head);
            }
        }
    }

    return 0;
}

int remove_adapters_config_chips(dev_bus_adapter *adapter)
{
    int ret = 0;
    dev_chip *match = NULL;

    match = dev_match_all_adapter_configured_chips(adapter,
            p_dev_config_list_head);

    if (match != NULL) {
        if (i2c_dev_verbose) {
            devi2c_debug(NULL, "Found chip in configuration spec initialized:");
            print_dev_chip(match);
        }

        ret = remove_sysfs_i2c_device(match);
        if (ret < 0) {
        	devi2c_warn(NULL, "Failed to remove i2c device: \'%s\' - %s", match->name, strerror(-ret));
        } else {
            ret = i2cdev_rescan();
            if (ret < 0) {
            	devi2c_warn(NULL, "Failed to rescan i2c devices! - %s", strerror(-ret));
            }
            dev_for_all_chips_match_config(p_dev_config_list_head);
        }
    }

    return 0;
}

int remove_all_config_chips(void)
{
    int ret = 0;
    dev_config_chip *comp = NULL;

    dev_for_all_chips_match_config(p_dev_config_list_head);

    SLIST_FOREACH(comp, p_dev_config_list_head, node) {

        if ((comp->adapter_available == true) && (comp->matched == true)) {

            struct dev_i2c_board_info info = {
                .addr = comp->address,
                .flags = 0,
                .name = comp->prefix,
                .path = comp->bus.path,
            };

            struct dev_i2c_board_info *p_info = &info;

            if (i2c_dev_verbose) {
                devi2c_debug(NULL, "Found chip in configuration spec initialized:");
                print_config_chip_data(comp);
            }

            ret = dev_remove_sysfs_i2c_device(p_info);

            if (ret < 0) {
            	devi2c_warn(NULL, "Failed to remove i2c device: \'%s\' - %s", p_info->name, strerror(-ret));
            } else {
                ret = i2cdev_rescan();
                if (ret < 0) {
                	devi2c_warn(NULL, "Failed to rescan i2c devices! - %s", strerror(-ret));
                    return ret;
                }
                dev_for_all_chips_match_config(p_dev_config_list_head);
            }
        }
    }
    return ret;
}

void i2cdev_cleanup(void)
{
    int i = 0;
    dev_config_chip *chipptr = NULL;

    if (get_libi2cdev_state() == LIB_SMB_UNINIIALIZED) {
        return;
    }
    set_libi2cdev_state(LIB_SMB_NOT_READY);

    if (p_dev_config_list_head) {
        while (NULL != (chipptr = SLIST_FIRST(p_dev_config_list_head))) {
            SLIST_REMOVE_HEAD(p_dev_config_list_head, node);
            free_dev_config_chip(chipptr);
            if (chipptr != NULL) {
                free(chipptr);
            }
        }
    }

    for (i = 0; i < dev_config_files_count; i++) {
        if (dev_config_files[i] != NULL) {
            free(dev_config_files[i]);
        }
        dev_config_files[i] = NULL;
    }

    if (dev_config_files)
    	free(dev_config_files);
    dev_config_files = NULL;
	dev_config_files_count = 0;
	dev_config_files_max = 0;
    stdin_config_file_name = NULL;

    free_adapter_list(dev_bus_list_headp);

    if (adapter_global_array != NULL) {
        free(adapter_global_array);
    }

    i2cdev_rescan_count = 0;
    adapter_global_count = 0;
    adapter_global_array = NULL;
    init_once = false;

    set_libi2cdev_state(LIB_SMB_UNINIIALIZED);
}
