/**
 * @file i2c-dev-parser.h
 * @copyright Violin Memory, Inc, 2014
 * @brief Functions used to gather and create the i2c
 * bus tree, used internally by the library.
 */

#ifndef I2C_DEV_PARSER_H_
#define I2C_DEV_PARSER_H_

#include <stdbool.h>
#include <busses.h>
#include <i2c-uapi.h>

/**
 * Gather All i2c device bus information
 * @return negative errno on failure else zero on success
 */
int gather_i2c_dev_busses(void);

#endif /* I2C_DEV_PARSER_H_ */
