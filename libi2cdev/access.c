/**
 * @file access.c
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Part of libi2cdev, a Linux library for reading i2c device data.
 * Specifically functions for configuration file parsing and chip matching.
 */

#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <strings.h>

#include "access.h"

#include "busses.h"
#include "data.h"
#include "error.h"
#include "sysfs.h"

#include "i2cdiscov.h"
#include "i2c-error.h"

/* We watch the recursion depth for variables only, as an easy way to
 detect cycles. */
#define DEPTH_MAX	8

extern dev_bus_adapter *lookup_dev_bus_by_nr(int nr);

/* Compare two chips name descriptions, to see whether they could match.
 Return 0 if it does not match, return 1 if it does match. */
int dev_match_chip(const dev_chip *chip1,
        const dev_chip *chip2)
{
    if (!chip1) {
        return 0;
    } else if (!chip2) {
        return 0;
    }
    if ((chip1->addr != chip2->addr) && (chip1->addr != CHIP_NAME_ADDR_ANY)
            && (chip2->addr != CHIP_NAME_ADDR_ANY)) {
        return 0;
    }

    if ((chip1->bus_id->nr != BUS_NR_ANY) && (chip2->bus_id->nr != BUS_NR_ANY)
            && (chip1->bus_id->nr != chip2->bus_id->nr)) {
        return 0;
    }

    if ((chip1->name != CHIP_NAME_PREFIX_ANY)
            && (chip2->name != CHIP_NAME_PREFIX_ANY)
            && strcasecmp(chip1->name, chip2->name)) {
        return 0;
    }

    if (strcasecmp(chip1->bus_id->path, chip2->bus_id->path)) {
        return 0;
    }

    return 1;
}

/* Compare two chips name descriptions, to see whether they could match.
 Return 0 if it does not match, return 1 if it does match. */
int dev_match_chip_config(const dev_chip *chip1,
        const dev_config_chip *chip2)
{
    if (!chip1) {
            return 0;
    } else if (!chip2) {
        return 0;
    }
    if ((chip1->name != CHIP_NAME_PREFIX_ANY)
            && (chip2->prefix != CHIP_NAME_PREFIX_ANY)
            && strcasecmp(chip1->name, chip2->prefix)) {
        return 0;
    }

    if ((chip1->addr != chip2->address) && (chip1->addr != CHIP_NAME_ADDR_ANY)
            && (chip2->address != CHIP_NAME_ADDR_ANY)) {
        return 0;
    }

    if (strcasecmp(chip1->bus_id->path, chip2->bus.path)) {
        return 0;
    }

    return 1;
}

/* Check whether the chip name is an 'absolute' name, which can only match
 one chip, or whether it has wildcards. Returns 0 if it is absolute, 1
 if there are wildcards. */
int dev_chip_name_has_wildcards(const dev_chip *chip)
{
    if ((chip->name == CHIP_NAME_PREFIX_ANY)
            || (chip->bus_id->type == DEV_BUS_TYPE_ANY)
            || (chip->bus_id->nr == BUS_NR_ANY)
            || (chip->addr == CHIP_NAME_ADDR_ANY)) {
        return 1;
    } else {
        return 0;
    }
}

/* Returns, one by one, a pointer to all sensor_chip structs of the
 config file which match with the given chip name. Last should be
 the value returned by the last call, or NULL if this is the first
 call. Returns NULL if no more matches are found. Do not modify
 the struct the return value points to!
 Note that this visits the list of chips from last to first. Usually,
 you want the match that was latest in the config file. */
dev_chip *dev_match_all_adapter_configured_chips(dev_bus_adapter *adapter,
        dev_config_chip_head *head)
{
    int ret = 0;
    dev_chip *chip = NULL;

    if (adapter == NULL) {
        return NULL;
    }

    if ((head == NULL) || (SLIST_FIRST(head) == NULL)) {
        return NULL;
    }

    SLIST_FOREACH(chip, &adapter->clients, node) {

        dev_config_chip *p_config_chip = NULL;

        SLIST_FOREACH(p_config_chip, head, node) {

            ret = dev_match_chip_config(chip, p_config_chip);

            if (ret == 1) {
                p_config_chip->matched = true;
                return chip;
            } else {
                continue;
            }
        }
    }
    return NULL;
}

/* Returns, one by one, a pointer to all sensor_chip structs of the
 config file which match with the given chip name. Last should be
 the value returned by the last call, or NULL if this is the first
 call. Returns NULL if no more matches are found. Do not modify
 the struct the return value points to!
 Note that this visits the list of chips from last to first. Usually,
 you want the match that was latest in the config file. */
void dev_for_all_chips_match_config(dev_config_chip_head *head)
{
    int ret = 0;
    dev_chip *chip = NULL;
    dev_config_chip *p_config_chip = NULL;
    const dev_bus_adapter *adapter = NULL;

    if (head == NULL || SLIST_FIRST(head) == NULL) {
        return;
    }

    SLIST_FOREACH(p_config_chip, head, node) {

        if (p_config_chip->bus.path != NULL) {
            adapter = dev_i2c_lookup_i2c_bus(p_config_chip->bus.path);
        } else {
            adapter = lookup_dev_bus_by_nr(p_config_chip->bus.nr);
        }

        if (adapter == NULL) {
            p_config_chip->adapter_available = false;
            p_config_chip->matched = false;
            p_config_chip->adapter = NULL;
            continue;
        } else {
            p_config_chip->adapter_available = true;
            p_config_chip->adapter = adapter;
        }

        SLIST_FOREACH(chip, &adapter->clients, node) {

            ret = dev_match_chip_config(chip, p_config_chip);

            if (ret == 1) {
                p_config_chip->matched = true;
                break;
            } else {
                p_config_chip->matched = false;
                continue;
            }
        }
    }
}

/* Returns, one by one, a pointer to all sensor_chip structs of the
 config file which match with the given chip name. Last should be
 the value returned by the last call, or NULL if this is the first
 call. Returns NULL if no more matches are found. Do not modify
 the struct the return value points to!
 Note that this visits the list of chips from last to first. Usually,
 you want the match that was latest in the config file. */
dev_config_chip *dev_config_chip_not_matched_chips(dev_config_chip_head *head)
{
    dev_config_chip *p_config_chip = NULL;
    dev_config_chip *unconfigured_chip = NULL;
    const dev_bus_adapter *adapter = NULL;

    if (head == NULL || SLIST_FIRST(head) == NULL) {
        return NULL;
    }

    SLIST_FOREACH(p_config_chip, head, node) {
        dev_chip *chip = NULL;
        if (p_config_chip->bus.path != NULL) {
            adapter = dev_i2c_lookup_i2c_bus(p_config_chip->bus.path);
        } else {
            adapter = lookup_dev_bus_by_nr(p_config_chip->bus.nr);
        }

        if (adapter == NULL) {
            p_config_chip->adapter_available = false;
            p_config_chip->matched = false;
            p_config_chip->adapter = NULL;
            continue;
        } else {
            p_config_chip->adapter_available = true;
            p_config_chip->adapter = adapter;
        }

        SLIST_FOREACH(chip, &adapter->clients, node) {
            if (dev_match_chip_config(chip, p_config_chip)) {
                p_config_chip->matched = true;
                break;
            } else {
                p_config_chip->matched = false;
                unconfigured_chip = p_config_chip;
                continue;
            }
        }
    }
    return unconfigured_chip;
}

