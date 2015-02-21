/**
 * @file i2c-bus-lists.h
 * @copyright Copyright, Violin Memory, Inc, 2014
 *
 * @brief I2C bus list maintenance functions
 * A bus list is used as a container for tracking I2c bus adapter relationships.
 * Each i2c bus adapter maintains bus-lists internally.
 * Lists may be used to keep track of peer nodes or parent-child relationships.
 * This module provides the capability to maintain ordering in a list
 * but does not itself enforce one.
 */

#ifndef I2C_BUS_H_
#define I2C_BUS_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "busses.h"

/* ------------------------------------------------------------------------- */

/**
 * Initialize a bus List
 * @param list
 */
static inline void init_bus_list(dev_bus_adapter_node *list)
{
    if (list != NULL) {
        list->le_next = NULL;
        list->le_prev = &list->le_next;
    }
}

/**
 * Initialize a dev List
 * @param node
 */
static inline void init_dev_list(dev_chip_node *node)
{
    if (node != NULL) {
        node->sle_next = NULL;
    }
}

/**
 * bus List Functions previous
 */
static inline dev_bus_adapter *bus_list_get_previous(dev_bus_adapter *entry)
{
    dev_bus_adapter *previous = NULL;
    if (entry != NULL) {
        if (entry->node.le_prev == &entry->node.le_next) {
            return NULL;
        } else {
            if (*entry->node.le_prev != NULL) {
                previous = *entry->node.le_prev;
                return previous;
            }
        }
    }
    return NULL;
}

/**
 * bus_list_remove - deletes entry from list.
 * @param entry: the element to delete from the list.
 * @brief list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void bus_list_remove(dev_bus_adapter *entry)
{
    if (entry != NULL) {
        LIST_REMOVE(entry, node);
    }
}

/**
 * bus_list_del_init - deletes entry from list and reinitialize it.
 * @param entry: the element to delete from the list.
 */
static inline void bus_list_del_init(dev_bus_adapter *entry)
{
    if (entry != NULL) {
        LIST_REMOVE(entry, node);
        init_bus_list(&entry->node);
    }
}

/**
 * bus_list_insert - add a new entry
 * @param new: new entry to be added
 * @param head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void bus_list_insert(dev_bus_adapter *new_entry,
        dev_bus_adapter_head *head)
{
    if ((new_entry != NULL) && (head != NULL)) {
        LIST_INSERT_HEAD(head, new_entry, node);
    }
}

/**
 * bus_list_insert_pos - add a new entry
 * @param new: new entry to be added
 * @param entry: list entry to add it before/after
 * @param pos: whether to add new before or after entry.
 *
 * Insert a new entry after or before the specified entry.
 */
static inline void bus_list_insert_pos(dev_bus_adapter *new_entry,
        dev_bus_adapter *entry, int pos)
{
    if ((new_entry != NULL) && (entry != NULL)) {
        if (pos > 0) {
            LIST_INSERT_AFTER(entry, new_entry, node);
        } else if (pos < 0) {
            LIST_INSERT_BEFORE(entry, new_entry, node);
        }
    }
}

/**
 * compare_bus_list_insert - add a new entry
 * @param new: new entry to be added
 * @param entry: list entry to add it before/after
 * @param compare: whether to add new before or after entry.
 *
 * Insert a new entry after or before the specified entry.
 */
static inline int compare_bus_list_insert(dev_bus_adapter *new_entry,
        dev_bus_adapter *entry,
        int (*compare)(const dev_bus_adapter *, const dev_bus_adapter *))
{
    int pos = compare(new_entry, entry);
    if (pos > 0) {
        LIST_INSERT_AFTER(entry, new_entry, node);
    } else if (pos < 0) {
        LIST_INSERT_BEFORE(entry, new_entry, node);
    }
    return pos;
}

/**
 * Get the Parent Device
 * @param child
 * @return parent
 */
static inline dev_bus_adapter *bus_get_parent(const dev_bus_adapter *child)
{
    if (!child) {
        return NULL;
    }
    return (child->parent);
}

/**
 * Used to lookup a device by parent id
 * @param parent_id
 * @param head
 * @return first matching adapter in list (head)
 */
static inline dev_bus_adapter *bus_parent_nr_lookup(int parent_id,
        dev_bus_adapter *head)
{
    while (head != NULL) {
        if (parent_id == head->parent_id) {
            return head;
        }
        if (((head)->node.le_next) == head) {
            break;
        }
        head = ((head)->node.le_next);
    }
    return NULL;
}

#endif /* I2C_BUS_H_ */
