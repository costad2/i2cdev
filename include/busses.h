/**
 * @file busses.h
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Internal I2C bus structure definitions
 * most of these structures are containers for holding internal
 * i2c bus relationship data.
 */

#ifndef LIB_I2CDEV_BUSSES_H
#define LIB_I2CDEV_BUSSES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/types.h>

#define BUS_PATH_ANY            NULL
#define CHIP_NAME_PREFIX_ANY    NULL
#define CHIP_NAME_ADDR_ANY      (-1)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum devbus_num {
    BUS_NR_ANY = -1,
    BUS_NR_IGNORE = -2,
    BUS_NR_PATH = -3,
    BUS_NR_ROOT = -4,
    BUS_NR_INVALID = -5,
} devbus_num;

typedef enum devbus_type {
    DEV_BUS_TYPE_ANY = -1,
    DEV_BUS_TYPE_I2C = 0,
    DEV_BUS_TYPE_ISA = 1,
    DEV_BUS_TYPE_PCI = 2,
    DEV_BUS_TYPE_SPI = 3,
    DEV_BUS_TYPE_VIRTUAL = 4,
    DEV_BUS_TYPE_ACPI = 5,
    DEV_BUS_TYPE_HID = 6,
    DEV_BUS_TYPE_MUX = 7,
    DEV_BUS_TYPE_OF = 8,
    DEV_BUS_TYPE_UNKNOWN,
    DEV_BUS_TYPE_MAX,
} devbus_type;

typedef struct dev_bus_id {
    devbus_type type;
    int nr; /* This number corresponds to number in /dev/i2c-? */
    char *path;
} dev_bus_id;

/* Config file line reference */
typedef struct dev_config_line {
    const char *filename;
    int lineno;
} dev_config_line;

struct dev_config_chip;
struct dev_chip;
struct dev_bus_adapter;
struct smbus_i2c_client;

typedef struct dev_config_chip_list {
    struct dev_config_chip *slh_first; /* first element */
} dev_config_chip_head;

typedef struct {
    struct dev_config_chip *sle_next; /* next element */
} dev_config_chip_node;

/* Config file bus declaration: the bus type and number, combined with adapter
 name */
typedef struct dev_config_chip {
    char *prefix;
    int address;
    dev_bus_id bus;
    dev_config_line line;
    bool matched;
    bool adapter_available;
    const struct dev_bus_adapter *adapter;
    dev_config_chip_node node;
} dev_config_chip;

/*
 * i2c_adapter is the structure used to identify a physical i2c bus along
 * with the access algorithms necessary to access it.
 */
typedef struct smbus_i2c_adapter {
    int nr; /* This number corresponds to number in /dev/i2c-? */
    bool ready;
    char *name;

    int fd; /* open file descriptor: /dev/i2c-?, or -1 */

    char *char_dev_name;
    dev_t char_dev;
    ino_t char_dev_uid;

    int prev_addr; /* previous chip address */
    unsigned long funcs;
} SMBusAdapter;

typedef struct dev_chip_list {
    struct dev_chip *slh_first; /* first element */
} dev_chip_head;

typedef struct {
    struct dev_chip *sle_next; /* next element */
} dev_chip_node;

/* A chip name is encoded in this structure */
typedef struct dev_chip {
    int addr;

    dev_bus_id *bus_id;

    bool autoload;

    char *name;
    char *devpath;
    char *driver;
    char *module;
    char *subsystem;

    struct dev_bus_adapter *adapter; /* the adapter the device sits on */

    dev_chip_node node;
} dev_chip;

struct dev_client_list;

typedef struct dev_client_head {
    struct dev_client_list *lh_first; /* first element */
} dev_client_user_list;

typedef struct {
    struct dev_client_list *le_next; /* next element */
    struct dev_client_list **le_prev; /* address of previous next element */
} dev_client_user_node;

struct dev_client_list {
    struct smbus_i2c_client *client;
    dev_client_user_node node;
};

typedef struct dev_bus_list {
    struct dev_bus_adapter *lh_first; /* first element */
} dev_bus_adapter_head;

typedef struct {
    struct dev_bus_adapter *le_next; /* next element */
    struct dev_bus_adapter **le_prev; /* address of previous next element */
} dev_bus_adapter_node;

/*
 * dev_bus is the structure used to identify a physical i2c bus along
 * with the access algorithms necessary to access it.
 */
typedef struct dev_bus_adapter {
    struct dev_bus_adapter *parent;

    int nr; /* the bus nr, this number corresponds to number in /dev/i2c-? */

    dev_bus_id bus;
    int chan_id; /* the channel id */
    int bus_id; /* the bus id */
    int parent_id;
    bool parent_is_adapter;

    char *path;

    char *name;
    char *devpath;
    char *subsystem;
    char *parent_name;

    struct smbus_i2c_adapter i2c_adapt;

    dev_client_user_list user_clients;

    dev_chip_head clients;

    dev_bus_adapter_head children;
    dev_bus_adapter_node node;
} dev_bus_adapter;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LIB_I2CDEV_BUSSES_H */
