/**
 * @file i2cdiscov.h
 * @copyright Copyright, Violin Memory, Inc, 2014
 *
 * @brief Semi-public functions used in lsi2c
 * Mostly used for printing i2c bus structure and device relationships.
 */

#ifndef LIB_I2CDISCOV_H
#define LIB_I2CDISCOV_H

#include <stdbool.h>
#include <libi2cdev.h>
#include "busses.h"

/*** Options ***/

extern int i2c_dev_verbose;

/*** Exports ***/
extern const char *stdin_config_file_name;

extern int print_devbus_tree(void);
extern void print_i2cdev_tree(void);
extern int print_all_adapters_dev_chips(void);
extern int print_config_file_data(void);

extern int print_devbus(dev_bus_adapter *dev, bool print_children);
extern int print_adapters_devices(const dev_bus_adapter *dev);
extern int print_dev_bus(const dev_bus_adapter *dev);
extern int print_dev_chips(const dev_bus_adapter *adapter);
extern void print_dev_chip(const dev_chip *chip);
extern void print_config_chip_data(const dev_config_chip *chip);

/**
 * Parse an I2CBUS path and return the corresponding
 * dev_bus_adapter's id or -errno if the bus is invalid or could not be found.
 * @param path
 * @return i2c character device minor number
 */
extern int get_devbus_nr_from_path(const char *path);

/**
 * Parse an I2CBUS path and return the corresponding
 * dev_bus_adapter, or NULL if the bus is invalid or could not be found.
 * @return matching i2c adapter else NULL
 */
extern dev_bus_adapter *dev_i2c_lookup_i2c_bus(const char *i2cbus_arg);

/**
 * Instantiate an I2C device based on its board info
 * through the kernel's sysfs "new_device" interface.
 *
 * @param[in] info This describes the I2C device to be created.
 * @return negative errno on failure else zero on success
 */
extern int dev_new_sysfs_i2c_device(const struct dev_i2c_board_info *info);

/**
 * Remove an I2C device based on its board info
 * through the kernel's sysfs "delete_device" interface.
 * @param[in] info This describes the I2C device to be deleted.
 * @return negative errno on failure else zero on success
 */
extern int dev_remove_sysfs_i2c_device(const struct dev_i2c_board_info *info);

/**
 * Try to load i2c_dev kernel module. Do nothing, if module is already loaded.
 * @return 1 on success, 0 otherwise.
 */
extern int try_load_i2c_dev_mod(void);

/**
 * Initilize all chips defined within the config file,
 * through the kernel. (see "/doc/libi2cdev-api")
 *
 * @return negative errno on failure else zero on success
 */
extern int initialize_all_config_chips(void);

/**
 * Remove all chips defined within the config file,
 * through the kernel. (see "/doc/libi2cdev-api")
 *
 * @return negative errno on failure else zero on success
 */
extern int remove_all_config_chips(void);

/**
 * Remove kernel I2C devices from the specified adapter.
 *
 * @param [in] adapter the i2c adapter parent to remove its children
 * @return negative errno on failure else zero on success
 */
extern int remove_adapters_config_chips(dev_bus_adapter *adapter);

#endif /* LIB_I2CDISCOV_H */
