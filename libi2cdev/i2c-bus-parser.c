/**
 * @file i2c-bus-parser.c
 * @author Danielle Costantino <dcostantino@vmem.com>
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Gather and Print the installed i2c busses and devices
 * for most modern kernels (2.6+) through data collected from /sys.
 */

#define _GNU_SOURCE 1

#include <linux/limits.h> /* for PATH_MAX */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>	/* for NAME_MAX */
#include <sys/ioctl.h>
#include <sys/queue.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>	/* for strcasecmp() */
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stddef.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <alloca.h>
#include <search.h>

#include <busses.h>

#include "common.h"
#include "sysfs.h"
#include "data.h"

#include "i2c-error.h"
#include "i2c-bus-lists.h"
#include "i2c-dev-parser.h"
#include "i2c-dev-path.h"
#include "i2cdiscov.h"

int print_i2c_dev_list_count(dev_bus_adapter_head *list_head);
dev_bus_adapter *lookup_dev_bus_by_nr(int nr);

/**
 * Counting number of elements in the List
 * @param list
 * @return count of elements in list
 */
static int bus_list_length(dev_bus_adapter_head *list)
{
    const dev_bus_adapter *cur_ptr = NULL;
    int count = 0;

    if (!list) {
        return -EINVAL;
    }
    cur_ptr = LIST_FIRST(list);

    while (cur_ptr != NULL) {
        cur_ptr = LIST_NEXT(cur_ptr, node);
        count++;
    }
    return (count);
}

/**
 *
 * @param node
 * @return depth of the current node relative to root device
 */
static int bus_node_depth(const dev_bus_adapter *node)
{
    const dev_bus_adapter *parent = NULL;
    const dev_bus_adapter *child = NULL;
    int count = 0;

    if (node) {
        child = node;
        while (NULL != (parent = bus_get_parent(child))) {
            count++;
            child = parent;
        }
    }
    return (count);
}

/**
 *
 * @param list
 * @param channel
 * @return count of elements in list that match channel
 */
static int count_bus_match_channel_set_bus(dev_bus_adapter_head *list,
        int channel)
{
    dev_bus_adapter *entry = NULL;
    int count = 0;

    if (!list) {
        return 0;
    }

    LIST_FOREACH(entry, list, node) {
        if (entry->chan_id == channel) {
            entry->bus_id = count;
            count++;
        }
    }
    return (count);
}

/* ------------------------------------------------------------------------- */

/**
 *
 * @param root device root to iterate through
 * @param func function to run on iterator
 * @return number of iterations else -errno
 */
static int foreach_devbus_tree(dev_bus_adapter_head *root,
        int (*func)(dev_bus_adapter *))
{
    int err = 0;
    dev_bus_adapter *dev = NULL;
    dev_bus_adapter_head *children = NULL;

    if (!root) {
        return -EINVAL;
    }

    if (LIST_EMPTY(root)) {
        return 0;
    }

    LIST_FOREACH(dev, root, node) {
        children = &dev->children;

        err = func(dev);

        if (err < 0) {
            dev_parse_error_wfn(strerror(-err), __FILE__, __LINE__);
        }

        err = foreach_devbus_tree(children, func);

        if (err < 0) {
            dev_parse_error_wfn(strerror(-err), __FILE__, __LINE__);
        }
    }
    return err;
}

/**
 *
 * @param root device root to iterate through
 * @param func function to run on iterator.
 * If func returns a value less than zero then break.
 * else if func returns greater than or equal to 0
 * add that to the number of iterations
 * @param perr pointer to previous error
 * @return number of iterations else -errno
 */
static int const_foreach_devbus_tree(const dev_bus_adapter_head *root,
        int (*func)(const dev_bus_adapter *), int *perr)
{
    dev_bus_adapter *dev_var = NULL;
    int err = 0;
    int count = 0;

    if (!root) {
        return -ENODATA;
    }

    if (LIST_EMPTY(root)) {
        goto done;
    }

    LIST_FOREACH(dev_var, root, node) {
        int local_err = 0;
        int *p_local_err = perr;
        const dev_bus_adapter *dev = (const dev_bus_adapter *) dev_var;

        local_err = func(dev);
        if (local_err < 0) {
            if (p_local_err) {
                if (*p_local_err < 0) {
                    err = *p_local_err;
                    goto done;
                }
            }
        } else {
            count += local_err;
            err = const_foreach_devbus_tree(&dev->children, func, p_local_err);
            if (err >= 0) {
                count += err;
            }
        }
    }
done:
    return ((err < 0) ? err : count);
}
/* ------------------------------------------------------------------------- */

/**
 * @param dev
 * @return 1 if read was successful else -error
 */
int print_dev_bus(const dev_bus_adapter *dev)
{
    int depth = 0;

    if (!dev) {
        return 0;
    }

    printf("bus: i2c-%d\t"
            "path: %-15s\t"
            "type: %s\t"
            "name: %-20s\t"
            , dev->nr, dev->bus.path,
            dev_sprint_bus_type(&dev->bus), dev->name);

    if (i2c_dev_verbose) {
        printf("\tparent: %-15s", dev->parent_name);
    }

    if (i2c_dev_verbose > 1) {
        printf("channel id: %d\t"
                "bus id: %d",
                dev->chan_id, dev->bus_id);
    }

    if (i2c_dev_verbose > 1) {
        depth = bus_node_depth(dev);
        printf("\tdepth: %d", depth);
        printf("\tsys_path: %-30s", dev->devpath);
    }
    printf("\n");
    return 1;
}

void print_dev_chip(const dev_chip *chip)
{
    if (!chip || !chip->bus_id) {
        return;
    }

    const char *bus_nr = dev_sprint_bus_nr(chip->bus_id);
    const char *bus_type = dev_sprint_bus_type(chip->bus_id);

    printf("bus=%d\t"
            "type=%s\t"
            "bus_path=%-15s\t"
            "address=0x%02hx\t"
            "name=%-15s\t"
            "driver=%-15s\t",
            chip->bus_id->nr, bus_type, bus_nr,
            chip->addr, chip->name, chip->driver);

    if (i2c_dev_verbose) {
        printf("module=%-15s\t",
                chip->module);
    }

    if (i2c_dev_verbose > 1) {
        printf("sys_path=%s", chip->devpath);
    }
    printf("\n");
}

/* returns the number of chips printed off an adapter */
int print_dev_chips(const dev_bus_adapter *adapter)
{
    int count = 0;
    const dev_chip *chip = NULL;
    const dev_chip_head *clients = NULL;
    if (adapter == NULL) {
        return 0;
    }
    clients = &adapter->clients;
    SLIST_FOREACH(chip, clients, node) {
        count++;
        print_dev_chip(chip);
    }
    return count;
}

void print_config_chip_data(const dev_config_chip *chip)
{
    int bus_nr = -1;

    if (!chip) {
        return;
    }

    if (chip->bus.nr >= 0 && chip->bus.path == BUS_PATH_ANY) {
        bus_nr = chip->bus.nr;
    } else {
        if (chip->bus.path != NULL) {
            bus_nr = strtoul(chip->bus.path, NULL, 0);
        }
    }

    printf("bus=%hd\t"
            "type=%s\t"
            "bus_path=%-15s\t"
            "address=0x%02hx\t"
            "name=%-15s",
            bus_nr,
            dev_sprint_bus_type(&chip->bus),
            chip->bus.path,
            chip->address,
            chip->prefix);

    printf("\t"
            "matched=%s\t"
            "has_adapter=%s",
            (chip->matched) ? "true" : "false",
            (chip->adapter_available) ? "true" : "false");

    if (i2c_dev_verbose) {
        printf("\tfile: %-25s\tline: %d", chip->line.filename,
                chip->line.lineno);
    }

    printf("\n");
}

int print_config_file_data(void)
{
    int count = 0;
    const dev_config_chip *p_config_chip = NULL;

    SLIST_FOREACH(p_config_chip, p_dev_config_list_head, node) {
        print_config_chip_data(p_config_chip);
        count++;
    }
    return count;
}

/**
 * @return number of devices read
 */
static int print_adapters_dev_chips(const dev_bus_adapter_head *list_head)
{
    return const_foreach_devbus_tree(list_head, print_dev_chips, NULL);
}

/**
 * @return number of devices read
 */
int print_adapters_devices(const dev_bus_adapter *dev)
{
    const dev_bus_adapter_head *list_head = NULL;
    if (!dev) {
        return 0;
    }
    list_head = &(dev->children);
    return const_foreach_devbus_tree(list_head, print_dev_chips, NULL);
}

/**
 * @param list_head
 * @return number of devices read
 */
static int print_devbus_list(const dev_bus_adapter_head *list_head)
{
    return const_foreach_devbus_tree(list_head, print_dev_bus, NULL);
}

/**
 * @param list_head
 * @return number of devices read
 */
int print_i2c_dev_list_count(dev_bus_adapter_head *list_head)
{
    int count = 0;
    if (!list_head) {
        return -EINVAL;
    }

    if (LIST_EMPTY(list_head)) {
        return -ENODATA;
    }

    count = bus_list_length(list_head);
    printf("Count: %d\n", count);
    return count;
}

/**
 * @param dev
 * @param print_children
 * @return number of devices read
 */
int print_devbus(dev_bus_adapter *dev, bool print_children)
{
    int count = 0;
    if (!dev) {
        return 0;
    }
    if (print_dev_bus(dev)) {
        count++;
    } else {
        return 0;
    }
    if (print_children == true) {
        count += print_devbus_list(&dev->children);
    }
    return count;
}
/**
 * @return number of devices read
 */
int print_devbus_tree(void)
{
    return print_devbus_list(dev_bus_list_headp);
}

/**
 * @return number of devices read
 */
int print_all_adapters_dev_chips(void)
{
    return print_adapters_dev_chips(dev_bus_list_headp);
}

/**
 * @param path I2C device path
 * @return matching i2c adapter else NULL
 */
static dev_bus_adapter *search_devbus_tree_fast_path(const char *path)
{
    dev_bus_adapter *dev_match = NULL;
    dev_bus_adapter_head *children = NULL;
    dev_i2c_path_disc pathdisc[MAX_BUS_DEPTH];
    dev_i2c_path_disc *p_pathdisc = NULL;
    int ret = 0;
    int count = 0;
    int i = 0;

    if (path == NULL || *path == '\0') {
        return NULL;
    }
    memset(pathdisc, 0, sizeof(pathdisc));
    ret = parse_i2cdev_path(path, pathdisc);
    if (ret < 0) {
        devi2c_err(NULL, "Failed to parse I2C bus string! \"%s\" - %s", path, strerror(-ret));
        return NULL;
    } else {
        count = ret;
    }
    p_pathdisc = &pathdisc[0];

    if (p_pathdisc->type == I2CDEV_END) {
        return NULL;
    }

    // first path argument is the base bus number/id (nr), so we can run a bsearch to find it
    dev_match = lookup_dev_bus_by_nr(p_pathdisc->id);
    if (!dev_match) {
        devi2c_warn(NULL, "Could not find matching I2C bus! \"%d\" - %s", p_pathdisc->id, strerror(ENODEV));
        return NULL;
    }

    children = &dev_match->children;
    p_pathdisc++;

    // Walk the tree looking for the device specified by p_pathd
    while((count >= i) && (p_pathdisc->type != I2CDEV_END)) {
        dev_bus_adapter *match_tmp = NULL;
        dev_bus_adapter *dev = NULL;
        if (!children) {
            return NULL;
        }
        switch (p_pathdisc->type) {
        case I2CDEV_BUS:
            if (LIST_EMPTY(children)) {
                return NULL;
            }
            LIST_FOREACH(dev, children, node) {
                if (dev->nr == p_pathdisc->id) {
                    children = &dev->children;
                    match_tmp = dev;
                    break;
                }
            }
            if (!match_tmp) {
                return NULL;
            }
            children = &match_tmp->children;
            break;
        case I2CDEV_MUX:
            if (LIST_EMPTY(children)) {
                return NULL;
            }
            LIST_FOREACH(dev, children, node) {
                if ((dev->bus_id == p_pathdisc->id) && (dev->chan_id == p_pathdisc->value)) {
                    match_tmp = dev;
                    break;
                }
            }
            if (!match_tmp) {
                return NULL;
            }
            children = &match_tmp->children;
            break;
        default:
            break;
        }
        p_pathdisc++ , i++;
        dev_match = match_tmp;
    };

    return dev_match;
}

/**
 * Parse an I2CBUS path and return the corresponding
 * dev_bus_adapter, or NULL if the bus is invalid or could not be found.
 * @return matching i2c adapter else NULL
 */
dev_bus_adapter *dev_i2c_lookup_i2c_bus(const char *i2cbus_arg)
{
    if (!i2cbus_arg) {
        return NULL;
    }
    if (check_libi2cdev_ready()) {
        return search_devbus_tree_fast_path(i2cbus_arg);
    } else {
        devi2c_err(NULL, "libi2cdev call made before library initialization!");
        return NULL;
    }
}

/**
 * Parse an I2CBUS path and return the corresponding
 * dev_bus_adapter's id or -errno if the bus is invalid or could not be found.
 * @param path
 * @return i2c character device minor number
 */
int get_devbus_nr_from_path(const char *path)
{
    dev_bus_adapter *found = NULL;

    found = dev_i2c_lookup_i2c_bus(path);
    if (found != NULL) {
        return found->nr;
    } else {
        return -ENODEV;
    }
}

static int compare_dev_bus_adapter_nr(const dev_bus_adapter *dev1,
        const dev_bus_adapter *dev2)
{
    if (!dev1) {
        return 1;
    } else if (!dev2) {
        return -1;
    }
    return ((dev1->nr) - (dev2->nr));
}

static int compare_dev_bus_adapter_id(const void *p1, const void *p2)
{
    const dev_bus_adapter * const dev1 = *(const dev_bus_adapter * const *) p1;
    const dev_bus_adapter * const dev2 = *(const dev_bus_adapter * const *) p2;

    if (!dev1) {
        return 1;
    } else if (!dev2) {
        return -1;
    }
    return ((dev1->nr) - (dev2->nr));
}

/**
 *
 * @param search
 * @param parent
 * @return negative errno on failure else zero on success
 */
static int bus_children_get(dev_bus_adapter_head *search,
        dev_bus_adapter *parent)
{
    int nr = -1;
    int err = 0;
    dev_bus_adapter *head = NULL;
    dev_bus_adapter *dtmp = NULL;
    dev_bus_adapter_head *children = NULL;

    if (!parent) {
        return -EINVAL;
    }

    head = LIST_FIRST(search);
    nr = parent->nr;
    children = &parent->children;

    while (NULL != (dtmp = bus_parent_nr_lookup(nr, head))) {
        int channel = -1;
        dev_bus_adapter *lookup = NULL;

        bus_list_del_init(dtmp);

        if (!children) {
            return -EINVAL;
        }

        dtmp->parent = parent;
        channel = dtmp->chan_id;

        if (LIST_EMPTY(children)) {
            bus_list_insert(dtmp, children);
        } else {
            int cmp = 0;
            lookup = LIST_FIRST(children);
            cmp = compare_bus_list_insert(dtmp, lookup,
                    compare_dev_bus_adapter_nr);
            if (cmp == 0) {
                return -EINVAL;
            }
        }

        count_bus_match_channel_set_bus(children, channel);

        if (LIST_EMPTY(search)) {
            break;
        } else {
            head = LIST_FIRST(search);
        }
    }

    children = &parent->children;

    if (LIST_EMPTY(children) || LIST_EMPTY(search)) {
        return 0;
    } else {
        dev_bus_adapter *tmp_child = NULL;
        LIST_FOREACH(tmp_child, children, node) {
            err = bus_children_get(search, tmp_child);
        }
    }
    return err;
}

/**
 *
 * @param search
 * @param root
 * @return negative errno on failure else zero on success
 */
static int adapter_tree_build(dev_bus_adapter_head *search,
        dev_bus_adapter_head *root)
{

    dev_bus_adapter *node = NULL;
    dev_bus_adapter *bus_tmp = NULL;
    dev_bus_adapter *node_root = NULL;
    dev_bus_adapter *dev = NULL;

    int err = 0;

    if ((search == NULL) || (root == NULL)) {
        return -EINVAL;
    }

    if (LIST_EMPTY(search)) {
        return -ENODATA;
    }

    node_root = LIST_FIRST(root);
    node = LIST_FIRST(search);

    while (NULL != (bus_tmp = bus_parent_nr_lookup(BUS_NR_ROOT, node))) {

        int cmp = 0;

        bus_list_del_init(bus_tmp);

        if (node_root == NULL) {
            bus_list_insert(bus_tmp, root);
        } else {
            cmp = compare_bus_list_insert(bus_tmp, node_root,
                    compare_dev_bus_adapter_nr);
            if (cmp == 0) {
                return -EINVAL;
            }
        }
        if (LIST_EMPTY(search)) {
            break;
        } else {
            node = LIST_FIRST(search);
        }
    }
    LIST_FOREACH(dev, root, node) {
        err = bus_children_get(search, dev);
    }
    return err;
}

/**
 * Generate the i2c adapter path string
 * @param dev
 * @return negative errno on failure else zero on success
 */
static int match_set_path_test(dev_bus_adapter *dev)
{
    dev_bus_adapter *parent = NULL;
    dev_bus_adapter *child = NULL;
    char path[NAME_MAX];
    int err = 0;
    int path_off = 0;

    if (!dev) {
        return -ENODEV;
    }

    child = dev;

    if (bus_get_parent(child) == NULL) {
        char *tmp_path = NULL;
        if (asprintf(&tmp_path, "%u", dev->nr) < 0) {
            return -ENOMEM;
        }
        child->bus.path = tmp_path;
        return 0;
    } else {

        while ((parent = bus_get_parent(child)) != NULL) {

            if (parent != NULL) {
                if (parent->bus.path != NULL) {
                    path_off = strlen(parent->bus.path);
                    if (path_off >= (int)sizeof(path)) {
                        return -EINVAL;
                    }
                    strncpy(path, parent->bus.path, path_off + 1);
                }
            }
            if (child->bus.path == NULL) {
                if (child->chan_id >= 0) {
                    path_off = snprintf(path + path_off,
                            sizeof(path) - path_off,
                            ":%u.%u", child->bus_id, child->chan_id);
                } else {
                    path_off = snprintf(path + path_off,
                            sizeof(path) - path_off,
                            ":%u", child->nr);
                }
                if (path_off >= (int)sizeof(path)) {
                    return -EINVAL;
                }
                child->bus.path = strdup(path);
                if (child->bus.path == NULL) {
                    return -ENOMEM;
                }
            }
            child = parent;
        }
    }
    return err;
}

static int generate_bus_paths(dev_bus_adapter_head *root)
{
    return foreach_devbus_tree(root, match_set_path_test);
}

/**
 * Parse I2C mux device name
 * @param[in] name sysfs device name value
 * @param[out] parent id if parent is an i2c adapter
 * @param[out] channel channel id if adapter is a mux
 * @return negative errno on failure else zero on successful MUX match, 1 if the string
 * does not contain mux and return value of sscanf otherwise.
 */
static int parse_mux_name(const char *name, int *parent, int *channel)
{
    int ret = 0;
    if (name == NULL) {
        return -EINVAL;
    }
    if (strstr(name, "mux") != NULL) {
        ret = sscanf(name, "i2c-%d-mux (chan_id %d)", parent, channel);
        if (ret < 0) {
            return ret;
        } else if (ret == 2) {
            return 0;
        } else {
            devi2c_info(NULL, "Invalid i2c mux name: \"%s\"", name);
        }
    }
    return 1;
}

static int sysfs_i2c_device_selector(const struct dirent *ent)
{
    if (!ent) {
        return 0;
    }
    if (ent->d_name[0] == '.') { /* skip hidden entries */
        return 0;
    } else if (isdigit(ent->d_name[0])) {
        if (strchr(ent->d_name, '-') != NULL) {
            return 1;
        }
        return 0;
    } else {
        return 0;
    }
}

static int sysfs_i2c_bus_selector(const struct dirent *ent)
{
    if (!ent) {
        return 0;
    }
    if (ent->d_name[0] == '.') { /* skip hidden entries */
        return 0;
    } else if ((strncmp(ent->d_name, "i2c-", 4)) == 0) {
        return 1;
    } else {
        return 0;
    }
}

dev_bus_adapter *lookup_dev_bus_by_nr(int nr)
{
    dev_bus_adapter *match = NULL;
    dev_bus_adapter **p_match = NULL;
    dev_bus_adapter adapter_key_val = { .nr = nr, };
    dev_bus_adapter *adapter_key = &adapter_key_val;

    if (nr < 0) {
        return NULL;
    }

    assert(adapter_global_array);
    p_match = bsearch(&adapter_key, adapter_global_array, adapter_global_count,
            sizeof(*adapter_global_array), compare_dev_bus_adapter_id);

    if (p_match != NULL) {
        match = *p_match;
    }
    return (match);
}

static char *get_parent_dev_name(const char *device)
{
    char *bname = NULL;
    char *slash = NULL;
    if (device != NULL && *device != '\0') {
        slash = strrchr(device, '/');
        if (slash != NULL) {
            char *name = NULL;
            name = strndup(device, slash - device);
            if (name != NULL) {
                bname = basename(name);
                if (bname) {
                    bname = strdup(bname);
                }
                free(name);
                return bname;
            }
        }
    }
    return NULL;
}

/**
 *
 * @param[in] name The parent device name to parse
 * @param[out] bus The i2c adapter id/nr
 * @return negative errno on failure else zero on success
 */
static int dev_parse_parent_i2c_nr(const char *name, int *bus)
{
    char *endptr = NULL;

    if (name == NULL) {
        *bus = BUS_NR_INVALID;
        return -EINVAL;
    }

    if (strncmp(name, "i2c-", 4)) {
        *bus = BUS_NR_ROOT;
        return -ENODEV;
    }

    name += 4;

    *bus = strtol(name, &endptr, 10);

    if (*name == '\0' || *endptr != '\0' || *bus < 0) {
        *bus = BUS_NR_ROOT;
        return -ENODEV;
    } else {
        return 0;
    }
}

/**
 *	sysfs_read_i2c_dev_bus_adapter
 *
 * @param device
 * @param attr
 * @return negative errno on failure else zero on success
 */
static int sysfs_read_i2c_dev_bus_adapter(dev_bus_adapter *adapter,
        const char *device, const char *attr)
{
    char *name = NULL;
    char *link_path = NULL;
    const char *attrp = NULL;
    bool dev_is_mux = false;
    char *endptr = NULL;
    char char_dev_name[20];
    int err = 0;
    int ret = 0;
    int bus = -1;
    int parent_mux_bus = BUS_NR_INVALID;
    int parent_bus = BUS_NR_INVALID;
    int channel = -1;
    struct stat st;

    if ((!adapter) || (!device) || (!attr)) {
        return -EINVAL;
    }

    if ((!strncmp(attr, "i2c-", 4))) {
        attrp = attr + 4;
        bus = strtol(attrp, &endptr, 10);
        if (bus < 0) {
            return -errno;
        }
        if (*endptr != '\0') {
            return -EINVAL;
        }
    } else {
        return -EINVAL;
    }

    link_path = realpath(device, NULL);
    if (link_path == NULL) {
        err = -errno;
        goto exit_free;
    }
    name = sysfs_read_attr(link_path, "name");

    // TODO: add handling of new kernel mux device topology
    ret = parse_mux_name(name, &parent_mux_bus, &channel);
    if (ret < 0) {
        err = ret;
        goto exit_free;
    } else if (ret == 0) {
        dev_is_mux = true;
    }
    ret = 0;

    adapter->nr = bus;
    adapter->chan_id = channel;
    adapter->bus_id = -1;
    adapter->name = name;
    adapter->devpath = link_path;
    adapter->subsystem = sysfs_read_device_subsystem(link_path);
    adapter->parent_name = get_parent_dev_name(link_path);

    ret = dev_parse_parent_i2c_nr(adapter->parent_name, (&parent_bus));

    adapter->parent_is_adapter = (ret < 0) ? false : true;
    adapter->parent_id = parent_bus;
    adapter->path = BUS_PATH_ANY;

    if (dev_is_mux) {
        adapter->bus.type = DEV_BUS_TYPE_MUX;
    } else {
        adapter->bus.type = DEV_BUS_TYPE_I2C;
    }

    adapter->bus.nr = bus;
    adapter->bus.path = BUS_PATH_ANY;

    adapter->i2c_adapt.nr = bus;
    adapter->i2c_adapt.fd = -1;
    adapter->i2c_adapt.name = name;
    adapter->i2c_adapt.prev_addr = -1;

    err = snprintf(char_dev_name, sizeof(char_dev_name), "/dev/i2c-%d", adapter->nr);
    if (err < 0) {
        goto init;
    } else if (stat(char_dev_name, &st) < 0) {
        goto init;
    } else {
        adapter->i2c_adapt.char_dev_uid = st.st_ino;
        adapter->i2c_adapt.char_dev = st.st_dev;
    }

init:

    LIST_INIT(&adapter->children);
    LIST_INIT(&adapter->user_clients);
    SLIST_INIT(&adapter->clients);
    init_bus_list(&(adapter->node));

exit_free:

    if (err < 0) {
        free(name);
        name = NULL;
        free(link_path);
        link_path = NULL;
        dev_parse_error_wfn(strerror(-err), __FILE__, __LINE__);
    }
    return err;
}

/**
 *
 * @param device
 * @param attr
 * @return negative errno on failure else zero on success
 */
static int sysfs_read_i2c_sub_device(dev_chip *chip, const char *path)
{
    const char *dummy_device_name = "dummy";

    if (!chip || !chip->adapter) {
        return -EINVAL;
    }

    if ((path == NULL) || (*path == '\0')) {
        return -EINVAL;
    }

    chip->devpath = realpath(path, NULL);
    if (chip->devpath == NULL) {
        return -errno;
    }

    chip->name = sysfs_read_attr(path, "name");
    if (chip->name == NULL) {
        free(chip->devpath);
        chip->devpath = NULL;
        return -ENOENT;
    }

    if (strncmp(chip->name, dummy_device_name, strlen(dummy_device_name)) != 0) {
        chip->driver = sysfs_read_device_driver(path);
        chip->module = sysfs_read_device_module(path);
    } else {
        chip->module = NULL;
        chip->driver = NULL;
    }

    chip->subsystem = sysfs_read_device_subsystem(path);

    init_dev_list(&chip->node);

    return 0;
}

/**
 * Gather All i2c devices whose parent "adapter"
 * @param device
 * @param attr
 * @return negative errno on failure else number of found devices on success
 */
static int gather_i2c_adapters_devices(dev_bus_adapter *adapter)
{
    int err = 0;
    int ret = 0;
    int count = 0;
    char path[PATH_MAX];
    char devname[NAME_MAX];
    int path_off = 0;
    int devname_off = 0;
    DIR *dir = NULL;
    struct dirent *ent = NULL;

    if (!adapter) {
        return -ENODEV;
    }

    path_off = strlen(adapter->devpath);
    if (path_off >= (int) sizeof(path)) {
        return -EINVAL;
    }
    strncpy(path, adapter->devpath, path_off);
    path[path_off] = '\0';

    devname_off = snprintf(devname, sizeof(devname), "%d-", adapter->nr);

    if ((dir = opendir(path)) == NULL) {
        return -errno;
    }

    while ((!ret) && (NULL != (ent = readdir(dir)))) {
        dev_chip *chip = NULL;
        int address = -1;
        char *endptr = NULL;
        const char *p_devname = NULL;
        if (ent->d_name[0] == '.') { /* skip hidden entries */
            continue;
        } else if (strncmp(ent->d_name, devname, devname_off) != 0) { /* skip entries not based on adapter name */
            continue;
        }
        p_devname = ent->d_name + devname_off;
        if (*p_devname == '\0') {
            continue;
        }
        address = strtol(p_devname, &endptr, 16);
        if (*endptr) {
            continue;
        }
        chip = calloc(1, sizeof(*chip));
        if (!chip) {
            err = -ENOMEM;
            goto exit_free;
        }
        chip->addr = address;
        chip->bus_id = &adapter->bus;
        chip->adapter = adapter;

        snprintf(path + path_off, sizeof(path) - path_off, "/%s", ent->d_name);

        ret = sysfs_read_i2c_sub_device(chip, path);
        if (ret < 0) {
            free(chip);
            continue;
        } else {
            count++;
            SLIST_INSERT_HEAD((&(adapter->clients)), chip, node);
            continue;
        }
    }
    closedir(dir);

    return count;

exit_free:

    if (dir) {
        closedir(dir);
    }

    if (err < 0) {
        free_dev_chip_list(&adapter->clients);
        dev_parse_error_wfn(strerror(-err), __FILE__, __LINE__);
    }

    return err;
}

/**
 * Gather All i2c adapters in /sys/bus/i2c/devices/
 * @return negative errno on failure else number of adapters found on success
 */
static ssize_t i2c_sysfs_gather_adapters(dev_bus_adapter ***list)
{

    char path[PATH_MAX];
    struct dirent **namelist = NULL;
    int err = 0;
    int path_off = 0;
    int num = 0;
    size_t count = 0;
    size_t found_count = 0;
    dev_bus_adapter **adapters = NULL;
    const char *bus_type = "i2c";
    int n = 0, i = 0;

    if (!list) {
        return -EINVAL;
    }
    assert(*list == NULL);

    if (!sysfs_mount) {
        return -ENOENT;
    }

    path_off = snprintf(path, sizeof(path), "%s/bus/%s/devices", sysfs_mount,
            bus_type);
    if (path_off >= (int) sizeof(path)) {
        return -EINVAL;
    }

    n = scandir(path, &namelist, sysfs_i2c_bus_selector, NULL);
    if (n < 0) {
        err = -errno;
        devi2c_err(NULL, "scandir failed!- %s", strerror(-err));
        return err;
    } else {

        count = (size_t) n;
        if (count == 0) {
            err = 0;
            goto exit_free;
        }

        adapters = calloc(count, sizeof(*adapters));
        if (!adapters) {
            err = -ENOMEM;
            goto exit_free;
        }
        i = 0;
        for (size_t h = 0; h < count; ++h) {

            adapters[i] = calloc(1, sizeof(*adapters[i]));
            if (adapters[i] == NULL) {
                err = -ENOMEM;
                num = i;
                goto exit_free;
            }

            snprintf(path + path_off, sizeof(path) - path_off, "/%s", namelist[h]->d_name);

            err = sysfs_read_i2c_dev_bus_adapter(adapters[i], path, namelist[h]->d_name);
            if (err < 0) {
                if (adapters[i] != NULL) {
                    free(adapters[i]);
                }
                adapters[i] = NULL;
                devi2c_notice(NULL, "invalid adapter! - %s", strerror(-err));
                continue;
            } else {
                i++;
            }
        }
        found_count = i;
        qsort(adapters, found_count, sizeof(*adapters), compare_dev_bus_adapter_id);
        *list = adapters;
        err = 0;
    }

exit_free:

    /* Free memory allocated by scandir() */
    for (size_t s = 0; s < count; ++s) {
        if (namelist[s] != NULL) {
            free(namelist[s]);
            namelist[s] = NULL;
        }
    }
    free(namelist);

    if (err < 0) {
        for (int r = 0; r < num; ++r) {
            if (adapters[r] != NULL) {
                free(adapters[r]);
            }
            adapters[r] = NULL;
        }
        if (adapters != NULL) {
            free(adapters);
        }
        adapters = NULL;
        return err;
    } else {
        return ((ssize_t) found_count);
    }
}

/* ------------------------------------------------------------------------- */

/**
 * Gather All i2c device bus information
 * @return negative errno on failure else zero on success
 */
int gather_i2c_dev_busses(void)
{

    int err = 0;
    int count = 0;
    dev_bus_adapter **adapters = NULL;
    dev_bus_adapter_head head_temp = { .lh_first = NULL };
    dev_bus_adapter_head *p_head_temp = &head_temp;

    if (!dev_bus_list_headp) {
        return -EFAULT;
    }

    /* look in sysfs */
    count = i2c_sysfs_gather_adapters(&adapters);

    if (count < 0) {
        err = count;
        devi2c_notice(NULL, "Error reading i2c adapters! - %s", strerror(-err));
        return err;
    } else if (count == 0) {
        err = 0;
        goto done;
    }

    for (int i = 0; i < count; ++i) {
        LIST_INSERT_HEAD(p_head_temp, adapters[i], node);
    }

    err = adapter_tree_build(p_head_temp, dev_bus_list_headp);

    if (err < 0) {
        devi2c_notice(NULL, "Failed to gather adapter roots - %s", strerror(-err));
        return err;
    }

    err = generate_bus_paths(dev_bus_list_headp);

    if (err < 0) {
        devi2c_notice(NULL, "Failed to generate adapter bus paths - %s", strerror(-err));
        return err;
    }

    for (int i = 0; i < count; ++i) {
        err = gather_i2c_adapters_devices(adapters[i]);
        if (err < 0) {
            devi2c_notice(NULL, "Error reading i2c devices! - %s", strerror(-err));
            goto done;
        } else {
            device_global_count += (size_t)err;
        }
    }

    err = 0;

done:

    adapter_global_array = adapters;
    adapter_global_count = (count >= 0) ? (size_t)count : 0 ;

    if (i2c_dev_verbose > 2) {
        devi2c_debug(NULL, "found %d i2c adapters", count);
    }
    return err;
}
