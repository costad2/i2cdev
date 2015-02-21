/**
 * @file access.h
 * @copyright Violin Memory, Inc, 2014
 */

#ifndef LIB_SENSORS_ACCESS_H
#define LIB_SENSORS_ACCESS_H

#include "busses.h"

/* Check whether the chip name is an 'absolute' name, which can only match
   one chip, or whether it has wildcards. Returns 0 if it is absolute, 1
   if there are wildcards. */
int dev_chip_name_has_wildcards(const dev_chip *chip);

/* Compare two chips name descriptions, to see whether they could match.
 Return 0 if it does not match, return 1 if it does match. */
int dev_match_chip(const dev_chip *chip1, const dev_chip *chip2);
int dev_match_chip_config(const dev_chip *chip1, const dev_config_chip *chip2);
void dev_for_all_chips_match_config(dev_config_chip_head *head);
dev_chip *dev_match_all_adapter_configured_chips(dev_bus_adapter *adapter, dev_config_chip_head *head);
dev_config_chip *dev_config_chip_not_matched_chips(dev_config_chip_head *head);
#endif /* def LIB_SENSORS_ACCESS_H */
