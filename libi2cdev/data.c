/**
 * @file data.c
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Configuration file and chip parsing functions
 */

/* For strdup and snprintf */
#define _GNU_SOURCE 1

#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>	/* for NAME_MAX */
#include <sys/ioctl.h>
#include <sys/queue.h>

#include <string.h>
#include <strings.h>	/* for strcasecmp() */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <search.h>
#include <libgen.h>	/* for dirname() */

#include "sysfs.h"
#include "busses.h"
#include "common.h"

#include "i2c-dev-parser.h"
#include "i2c-error.h"

#include "data.h"

void dev_free_bus_id(dev_bus_id *bus)
{
    if (bus != NULL) {
        if (bus->path != NULL) {
            free(bus->path);
        }
        bus->path = NULL;
    }
}

void dev_free_chip_vals(dev_chip *chip)
{

    /* check if pointer to pointer is null */
    if (chip != NULL) {

        chip->bus_id = NULL;
        chip->adapter = NULL;

        if (chip->name != NULL) {
            free(chip->name);
        }
        chip->name = NULL;

        if (chip->driver != NULL) {
            free(chip->driver);
        }
        chip->driver = NULL;

        if (chip->subsystem != NULL) {
            free(chip->subsystem);
        }
        chip->subsystem = NULL;

        if (chip->module != NULL) {
            free(chip->module);
        }
        chip->module = NULL;

        if (chip->devpath != NULL) {
            free(chip->devpath);
        }
        chip->devpath = NULL;
    }
}

void dev_free_chip(dev_chip **chip)
{
    dev_chip *p_chip = NULL;
    /* check if pointer to pointer is null */
    if (chip != NULL && *chip != NULL) {
        p_chip = *chip;

        p_chip->bus_id = NULL;
        p_chip->adapter = NULL;

        if (p_chip->name != NULL) {
            free(p_chip->name);
        }
        p_chip->name = NULL;

        if (p_chip->driver != NULL) {
            free(p_chip->driver);
        }
        p_chip->driver = NULL;

        if (p_chip->subsystem != NULL) {
            free(p_chip->subsystem);
        }
        p_chip->subsystem = NULL;

        if (p_chip->module != NULL) {
            free(p_chip->module);
        }
        p_chip->module = NULL;

        if (p_chip->devpath != NULL) {
            free(p_chip->devpath);
        }
        p_chip->devpath = NULL;

        if (chip != NULL && *chip != NULL) {
            free(*chip); /* actually deallocate memory */
            *chip = NULL; /* null terminate */
        }
    }
}

/*
 Parse a chip name to the internal representation.

 The 'prefix' part in the result is freshly allocated. All old contents
 of res is overwritten. res itself is not allocated. In case of an error
 return (ie. != 0), res is undefined, but all allocations are undone.
 */

int dev_parse_chip_name(const char *name, dev_chip *res)
{
    char *dash = NULL;
    int err = 0;
    if (!res) {
        return -EINVAL;
    }
    res->name = CHIP_NAME_PREFIX_ANY;
    res->devpath = NULL;
    res->adapter = NULL;
    res->driver = NULL;
    res->subsystem = NULL;
    res->module = NULL;

    res->addr = CHIP_NAME_ADDR_ANY;
    res->bus_id->nr = BUS_NR_ANY;
    res->bus_id->type = DEV_BUS_TYPE_UNKNOWN;
    res->bus_id->path = BUS_PATH_ANY;
    /* First, the prefix. It's either "*" or a real chip name. */
    if (!strncmp(name, "*-", 2)) {
        res->name = CHIP_NAME_PREFIX_ANY;
        name += 2;
    } else {
        if ((dash = strchr(name, '-')) == NULL) {
            return -EINVAL;
        }
        res->name = strndup(name, dash - name);
        if (!res->name) {
            return -ENOMEM;
        }
        name = dash + 1;
    }

    /* Then we have either a sole "*" (all chips with this name) or a bus
     type and an address. */
    if (!strcmp(name, "*")) {
        res->bus_id->type = DEV_BUS_TYPE_ANY;
        res->bus_id->nr = BUS_NR_ANY;
        res->addr = CHIP_NAME_ADDR_ANY;
        res->bus_id->path = BUS_PATH_ANY;
        return 0;
    }

    if ((dash = strchr(name, '-')) == NULL) {
        err = -EINVAL;
        goto free_error;
    }
    if (!strncmp(name, "i2c", dash - name)) {
        res->bus_id->type = DEV_BUS_TYPE_I2C;
    } else if (!strncmp(name, "isa", dash - name)) {
        res->bus_id->type = DEV_BUS_TYPE_ISA;
    } else if (!strncmp(name, "pci", dash - name)) {
        res->bus_id->type = DEV_BUS_TYPE_PCI;
    } else if (!strncmp(name, "spi", dash - name)) {
        res->bus_id->type = DEV_BUS_TYPE_SPI;
    } else if (!strncmp(name, "virtual", dash - name)) {
        res->bus_id->type = DEV_BUS_TYPE_VIRTUAL;
    } else if (!strncmp(name, "acpi", dash - name)) {
        res->bus_id->type = DEV_BUS_TYPE_ACPI;
    } else if (!strncmp(name, "hid", dash - name)) {
        res->bus_id->type = DEV_BUS_TYPE_HID;
    } else {
        err = -EINVAL;
        goto free_error;
    }
    name = dash + 1;

    /* Some bus types have an additional bus number.
     For these, the next part is either a "*" (any bus of that type)
     or a decimal number. */
    switch (res->bus_id->type) {
    case DEV_BUS_TYPE_I2C:
    case DEV_BUS_TYPE_ACPI:
    case DEV_BUS_TYPE_ISA:
    case DEV_BUS_TYPE_OF:
    case DEV_BUS_TYPE_PCI:
    case DEV_BUS_TYPE_SPI:
    case DEV_BUS_TYPE_HID:
        if (!strncmp(name, "*-", 2)) {
            res->bus_id->nr = BUS_NR_ANY;
            res->bus_id->path = BUS_PATH_ANY;
            name += 2;
            break;
        } else {
            if ((dash = strchr(name, '-')) == NULL) {
                err = -EINVAL;
                goto free_error;
            }
            res->bus_id->path = strndup(name, dash - name);
            res->bus_id->nr = BUS_NR_PATH;
            name = dash + 1;
        }
        break;
    default:
        res->bus_id->nr = BUS_NR_ANY;
    }

    /* Last part is the chip address, or "*" for any address. */
    if (!strcmp(name, "*")) {
        res->addr = CHIP_NAME_ADDR_ANY;
    } else {
        res->addr = strtoul(name, &dash, 16);
        if (*name == '\0' || *dash != '\0' || res->addr < 0) {
            err = -EINVAL;
            goto free_error;
        }
    }

    return 0;

free_error:
    dev_free_chip(&res);
    return err;
}

int dev_snprintf_chip_name(char *str, size_t size, const dev_chip *chip)
{
    if (!chip) {
        return -EINVAL;
    }

    switch (chip->bus_id->type) {
    case DEV_BUS_TYPE_ISA:
        return snprintf(str, size, "%s-isa-%04x", chip->name,
                chip->addr);
    case DEV_BUS_TYPE_PCI:
        return snprintf(str, size, "%s-pci-%04x", chip->name,
                chip->addr);
    case DEV_BUS_TYPE_I2C:
        return snprintf(str, size, "%s-i2c-%hd-%02x", chip->name,
                chip->bus_id->nr, chip->addr);
    case DEV_BUS_TYPE_SPI:
        return snprintf(str, size, "%s-spi-%hd-%x", chip->name,
                chip->bus_id->nr, chip->addr);
    case DEV_BUS_TYPE_VIRTUAL:
        return snprintf(str, size, "%s-virtual-%x", chip->name,
                chip->addr);
    case DEV_BUS_TYPE_ACPI:
        return snprintf(str, size, "%s-acpi-%x", chip->name,
                chip->addr);
    case DEV_BUS_TYPE_HID:
        return snprintf(str, size, "%s-hid-%hd-%x", chip->name,
                chip->bus_id->nr, chip->addr);
    default:
        break;
    }

    return -EINVAL;
}

static const char *dev_bus_type_name[] = {
        [DEV_BUS_TYPE_I2C] = "i2c",
        [DEV_BUS_TYPE_ISA] = "isa",
        [DEV_BUS_TYPE_PCI] = "pci",
        [DEV_BUS_TYPE_SPI] = "spi",
        [DEV_BUS_TYPE_VIRTUAL] = "virtual",
        [DEV_BUS_TYPE_ACPI] = "acpi",
        [DEV_BUS_TYPE_HID] = "hid",
        [DEV_BUS_TYPE_MUX] = "mux",
        [DEV_BUS_TYPE_OF] = "of",
        [DEV_BUS_TYPE_UNKNOWN] = "",
};

const char *dev_sprint_bus_type(const dev_bus_id *bus)
{
    if (!bus || (bus->type >= DEV_BUS_TYPE_UNKNOWN)) {
        return NULL;
    }
    return dev_bus_type_name[bus->type];
}

const char *dev_sprint_bus_nr(const dev_bus_id *bus)
{
    if (!bus) {
        return NULL;
    }
    return bus->path;
}

int dev_parse_bus_id(const char *name, dev_bus_id *bus)
{
    char *endptr = NULL;
    if (!bus) {
        return -ENODATA;
    }
    if (strncmp(name, "i2c-", 4)) {
        return -EINVAL;
    }
    name += 4;
    bus->type = DEV_BUS_TYPE_I2C;

    if ((strchr(name, ':') != NULL) || (strchr(name, '.') != NULL)) {
        bus->path = strdup(name);
        bus->nr = BUS_NR_PATH;
        return 0;
    } else {
        bus->nr = strtoul(name, &endptr, 10);
        if (*name == '\0' || *endptr != '\0' || bus->nr < 0)
            return -EINVAL;
        bus->path = strdup(name);
        return 0;
    }
}

