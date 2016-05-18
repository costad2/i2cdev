/**
 * @file data.h
 * @copyright Violin Memory, Inc, 2014
 */

#ifndef DATA_H_
#define DATA_H_

#include <stddef.h>
#include "busses.h"

extern int i2cdev_rescan_count;

extern dev_config_chip *dev_config_chips;
extern int dev_config_chips_count;
extern int dev_config_chips_max;

extern char **dev_config_files;
extern int dev_config_files_count;
extern int dev_config_files_max;

extern size_t device_global_count;
extern dev_config_chip_head *p_dev_config_list_head;
extern dev_bus_adapter_head *dev_bus_list_headp;

extern dev_bus_adapter **adapter_global_array;
extern size_t adapter_global_count;

#define dev_add_config_files(el) dev_add_array_el( \
	(el), &dev_config_files, &dev_config_files_count, \
	&dev_config_files_max, sizeof(char *))

void dev_free_bus_id(dev_bus_id *bus);
void dev_free_chip(dev_chip **chip);
void dev_free_chip_vals(dev_chip *chip);

void free_adapter_val(dev_bus_adapter **adapter);
void free_adapter_list(dev_bus_adapter_head *list);
void free_dev_chip_list(dev_chip_head *list);

int dev_parse_chip_name(const char *name, dev_chip *res);
int dev_snprintf_chip_name(char *str, size_t size, const dev_chip *chip);
int dev_parse_bus_id(const char *name, dev_bus_id *bus);

const char *dev_sprint_bus_nr(const dev_bus_id *bus);
const char *dev_sprint_bus_type(const dev_bus_id *bus);


#endif /* DATA_H_ */
