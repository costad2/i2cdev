/**
 * @file i2c-dev-path.h
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief definitions for I2C bus path parsing
 * For information regarding the definition of an I2C path
 * refer to the "Bus discovery and mapping detail" section of
 * the libi2cdev api document "/doc/libi2cdev-api"
 */

#ifndef I2C_DEV_PATH_H_
#define I2C_DEV_PATH_H_

#include <sys/queue.h>

#include "busses.h"

/**
 * I2C-DEV Discovery Path type and enum
 */
typedef enum {
	I2CDEV_BUS,
	I2CDEV_MUX,
	I2CDEV_CHAN,
	I2CDEV_ADDR,
	I2CDEV_END,
} I2cDevpType;

typedef struct i2c_path_array {
	int value;
	I2cDevpType type;
	struct i2c_path_array *child;
}i2c_path_disc;

typedef struct dev_i2c_path_array {
	int id;
	int value;
	int depth;
	I2cDevpType type;
} dev_i2c_path_disc;

#define MAX_BUS_DEPTH 20

/**
 * path example: '0:0.2:0.0:1.5'
 * @param [in] path the i2c bus path to parse
 * @param [out] discp array used to store the parsed i2c bus path information
 * @return negative errno on failure else the number of tokens on success
 */
extern int parse_i2cdev_path(const char *path, dev_i2c_path_disc discp[]);

#endif /* I2C_DEV_PATH_H_ */
