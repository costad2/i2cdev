/**
 * @file i2c-uapi.h
 * @author Danielle Costantino <dcostantino@vmem.com>
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief Public API for libi2cdev (a userspace i2c library).
 */

/*******************************************************************************
 * Copyright (C) 2015 Danielle Costantino <danielle.costantino@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 * *******************************************************************************/

#ifndef LIB_I2C_UAPI_H
#define LIB_I2C_UAPI_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/syslog.h>

extern const char *libi2cdev_version;

struct smbus_i2c_client;
struct smbus_i2c_adapter;
struct dev_i2c_board_info;
struct dev_client_list;

/* i2c */
#define I2C_NAME_SIZE   20
#define I2C_ADAPT_PATH_SIZE 48

/**
 * @struct dev_i2c_board_info - template for device creation
 * @param type chip type, to initialize i2c_client.name
 * @param flags to initialize i2c_client.flags
 * @param addr stored in i2c_client.addr
 * @param path path descriptor for acquiring an i2c bus adapter id
 *
 * dev_i2c_board_info is used to build tables of information listing I2C devices
 * that are present.  This information is used to grow the driver tree.
 * bus numbers identify adapters that aren't yet available.  For add-on boards,
 * i2c_new_device() does this dynamically with the adapter already known.
 */
struct dev_i2c_board_info {
    unsigned short addr;
    unsigned short flags;
    const char *name;
    const char *path;
    void *platform_data;
};

/**
 * @def DEV_I2C_BOARD_INFO_PATH(dev_name, dev_addr, dev_path)
 * This macro is used to define the parameters of an I2C device.
 * It lists an i2c device by its name, address, and path.
 * @param dev_name identifies the device name
 * @param dev_addr the device's address on the bus.
 * @param dev_path the device's path to acquire its bus.
 *
 * This macro initializes essential fields of a struct i2c_board_info,
 * declaring what has been provided on a particular board.  Optional
 * fields (such as associated irq, or device-specific platform_data)
 * are provided using conventional syntax.
 */
#define DEV_I2C_BOARD_INFO_PATH(dev_name, dev_addr, dev_path) \
    .name = dev_name, .flags = 0, .addr = (dev_addr),  .path = dev_path

/* flags for the client struct: */
#define I2C_CLIENT_PEC  0x04        /* Use Packet Error Checking */
#define I2C_CLIENT_TEN  0x10        /* we have a ten bit chip address */

/**
 * This represents an I2C slave device
 * @param flags I2C_CLIENT_TEN indicates the device uses a ten bit chip address;
 *  I2C_CLIENT_PEC indicates it uses SMBus Packet Error Checking
 * @param addr Address used on the I2C bus connected to the parent adapter.
 * @param name Indicates the type of the device, usually a chip name that's
 *  generic enough to hide second-sourcing and compatible revisions.
 * @param adapter manages the bus segment hosting this I2C device
 * @param dev Driver model device node for the slave.
 *
 * An smbus_i2c_client identifies a single device (i.e. chip) connected to an
 * i2c bus. The behavior exposed to Linux is defined by the driver
 * managing the device.
 *
 * @note applications should not directly manipulate the headers.
 */
typedef struct smbus_i2c_client {
    unsigned short flags;
    unsigned short addr; /**< chip address - 7bit */
    /*! @note addresses are stored in the _LOWER_ 7 bits */
    int force;
    char name[I2C_NAME_SIZE]; /**< the device name @note for informational purpose only, but must be set) */
    char path[I2C_ADAPT_PATH_SIZE]; /**< Path to the client's adapter */
    struct smbus_i2c_adapter *adapter; /**< the adapter we sit on */
    struct dev_client_list *client_node; /**< a pointer to allocated data pointing to itself */
    void *dev; /**< A void pointer that can be used to store device specific information */
} SMBusDevice;

#define to_devi2c_client(d) container_of(d, struct smbus_i2c_client, dev)

static inline void *devi2c_get_clientdata(const SMBusDevice *client)
{
    return client->dev;
}

static inline void devi2c_set_clientdata(SMBusDevice *client,
        void *data)
{
    client->dev = data;
}

/*---------------------------------------------------------------------------*/

/**
 * Library initialization function
 * Load the configuration file and the detected chips list. If this
 * returns a value unequal to zero, you are in trouble; you can not
 * assume anything will be initialized properly. If you want to
 * reload the configuration file, call i2cdev_cleanup() below before
 * calling i2cdev_init() again.
 *
 * @param input[in] configuration file to scan or NULL
 * @return negative errno on failure else zero on success
 */
extern int i2cdev_init(FILE *input);

/**
 * Used to rescan the i2c device tree and update internal data structures
 * @return negative errno on failure else zero on success
 */
extern int i2cdev_rescan(void);

/**
 * Clean-up function to free libraries resources
 * @note You can't access anything after
 * this, until the next i2cdev_init() call!
 */
extern void i2cdev_cleanup(void);

/*---------------------------------------------------------------------------*/

/**
 * Type used in defining a logging facility
 * @param devi2c Handle to I2C slave device or NULL
 * @param priority syslog priority value
 * @param format Format string
 */
typedef void (*devi2c_log_func_t)(SMBusDevice *devi2c, int priority, const char *format, ...);

/**
 * Used to define a custom logging function
 * @param func Logging function pointer or NULL to reset to internal logging.
 */
extern void devi2c_set_logging_function(devi2c_log_func_t func);

extern devi2c_log_func_t devi2c_log;

extern void devi2c_print(SMBusDevice *devi2c, int priority, const char *format, ...) __attribute__((format(printf,3,4)));

/*
 * These macros can be used to log information about a specific client device
 * at a specific logging priority
 */
#define devi2c_err(devi2c, ...)         devi2c_log(devi2c, LOG_ERR, ##__VA_ARGS__)
#define devi2c_warn(devi2c, ...)        devi2c_log(devi2c, LOG_WARNING, ##__VA_ARGS__)
#define devi2c_notice(devi2c, ...)      devi2c_log(devi2c, LOG_NOTICE, ##__VA_ARGS__)
#define devi2c_info(devi2c, ...)        devi2c_log(devi2c, LOG_INFO, ##__VA_ARGS__)

#ifdef DEBUG
#define devi2c_debug(devi2c, ...)     devi2c_log(devi2c, LOG_DEBUG, ##__VA_ARGS__)
#else
static inline void devi2c_debug(SMBusDevice *devi2c, ...)   {}
#endif

/*---------------------------------------------------------------------------*/

/**
 * Instantiate an i2c device based on its board info.
 * This returns a pointer to the new I2C device.
 * An I2C client is needed to make any dev_i2c_* API calls.
 * When the client is no longer needed, use 'dev_i2c_delete();'
 * to free the device.
 *
 * @param[in] info This describes the I2C device to be created.
 * @return The new i2c client or NULL to indicate an error.
 */
extern SMBusDevice *dev_i2c_new_device(struct dev_i2c_board_info const *info);

/**
 * Deallocates and closes the client device passed into the function.
 *
 * @param[in] client device to close and deallocate
 */
extern void dev_i2c_delete(SMBusDevice *client);

/*---------------------------------------------------------------------------*/
/* usually are only used internally within each library call */
extern int dev_i2c_open(SMBusDevice *client);
extern int dev_i2c_close(SMBusDevice *client);
/*---------------------------------------------------------------------------*/

/**
 * This function can be used to check the presence of a device on an I2C bus.
 * This executes either the SMBus "receive byte" protocol or write byte.
 *
 * @param[in] addr address to check
 * @param[in] path path to check
 * @param[in] mode Probe mode ( auto = 0, read byte= 1, write byte = 2)
 * @return negative errno on failure else zero on success
 */
extern int32_t dev_i2c_smbus_probe(uint8_t addr, const char *path, int mode);

/*---------------------------------------------------------------------------*/

extern int32_t dev_i2c_smbus_write_quick(SMBusDevice *client, uint8_t value);

/**
 * dev_i2c_smbus_read_byte - SMBus "receive byte" protocol.
 *
 * @param[in] client Handle to slave device
 * @return negative errno on failure else the byte received from the device.
 */
extern int32_t dev_i2c_smbus_read_byte(SMBusDevice *client);

/**
 * dev_i2c_smbus_write_byte - SMBus "send byte" protocol
 *
 * @param[in] client Handle to slave device
 * @param[in] value Byte to be sent
 * @return negative errno on failure else zero on success
 */
extern int32_t dev_i2c_smbus_write_byte(SMBusDevice *client, uint8_t value);

/**
 * dev_i2c_smbus_read_byte_data - SMBus "read byte" protocol
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @return negative errno on failure else a data byte received from the device.
 */
extern int32_t dev_i2c_smbus_read_byte_data(SMBusDevice *client,
        uint8_t command);

/**
 * dev_i2c_smbus_write_byte_data - SMBus "write byte" protocol
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @param[in] value Byte being written
 * @return negative errno on failure else zero on success
 */
extern int32_t dev_i2c_smbus_write_byte_data(SMBusDevice *client,
        uint8_t command, uint8_t value);

/**
 * dev_i2c_smbus_read_word_data - SMBus "read word" protocol
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @return negative errno on failure else a 16-bit unsigned "word"
 *  received from the device.
 */
extern int32_t dev_i2c_smbus_read_word_data(SMBusDevice *client,
        uint8_t command);

#define i2c_swab16(_x) ((uint16_t)(                 \
    (((uint16_t)(_x) & (uint16_t)0x00ffU) << 8) |   \
    (((uint16_t)(_x) & (uint16_t)0xff00U) >> 8)))

/**
 * dev_i2c_smbus_write_word_data - SMBus "write word" protocol
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @param[in] value 16-bit "word" being written
 * @return negative errno on failure else zero on success
 */
extern int32_t dev_i2c_smbus_write_word_data(SMBusDevice *client,
        uint8_t command, uint16_t value);

static inline int32_t dev_i2c_smbus_read_word_swapped(SMBusDevice *client,
        uint8_t command)
{
    int32_t value = dev_i2c_smbus_read_word_data(client, command);
    return ((value < 0) ? value : i2c_swab16(value));
}

static inline int32_t dev_i2c_smbus_write_word_swapped(SMBusDevice *client,
        uint8_t command, uint16_t value)
{
    return (dev_i2c_smbus_write_word_data(client, command, i2c_swab16(value)));
}

/**
 * dev_i2c_smbus_read_block_data - SMBus "block read" protocol
 *
 * @note that using this function requires that the client's adapter support
 * the I2C_FUNC_SMBUS_READ_BLOCK_DATA functionality.  Not all adapter drivers
 * support this; its emulation through I2C messaging relies on a specific
 * mechanism (I2C_M_RECV_LEN) which may not be implemented.
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @param[out] values Byte array into which data will be read; big enough to hold
 *  the data returned by the slave.  SMBus allows at most 32 bytes.
 * @return negative errno on failure else the number of data bytes in the slave's response.
 */
extern int32_t dev_i2c_smbus_read_block_data(SMBusDevice *client,
        uint8_t command, uint8_t *values);

/**
 * dev_i2c_smbus_write_block_data - SMBus "block write" protocol
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @param[in] length Size of data block; SMBus allows at most 32 bytes
 * @param[in] values Byte array which will be written.
 * @return negative errno on failure else zero on success.
 */
extern int32_t dev_i2c_smbus_write_block_data(SMBusDevice *client,
        uint8_t command, uint8_t length, const uint8_t *values);

/**
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @param[in] length Size of data block to read; SMBus allows at most 32 bytes
 * @param[out] values Byte array into which data will be read; big enough to hold
 *  the data returned by the slave.  SMBus allows at most 32 bytes.
 * @return negative errno on failure else the number of data bytes read.
 */
extern int32_t dev_i2c_smbus_read_i2c_block_data(SMBusDevice *client,
        uint8_t command, uint8_t length, uint8_t *values);

/**
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @param[in] length Size of data block; SMBus allows at most 32 bytes
 * @param[in] values Byte array which will be written.
 * @return negative errno on failure else zero on success.
 */
extern int32_t dev_i2c_smbus_write_i2c_block_data(SMBusDevice *client,
        uint8_t command, uint8_t length, const uint8_t *values);

/**
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @param[in] value
 * @return negative errno on failure else <?>
 */
extern int32_t dev_i2c_smbus_process_call(SMBusDevice *client, uint8_t command,
        uint16_t value);

/**
 *
 * @param[in] client Handle to slave device
 * @param[in] command Byte interpreted by slave
 * @param[in] length Size of data block; SMBus allows at most 32 bytes
 * @param[in,out] values
 * @return negative errno on failure else the number of read bytes.
 */
extern int32_t dev_i2c_smbus_block_process_call(SMBusDevice *client,
        uint8_t command, uint8_t length, uint8_t *values);

/*---------------------------------------------------------------------------*/

/**
 *
 * @param[in] client Handle to slave device
 * @param[in] write_length Size of data block to read.
 * @param[in] write_data Byte array from which data will be written.
 * @param[in] read_length Size of read data array.
 * @param[out] read_data Byte array into which data will be read
 * @return negative errno on failure else 0.
 *
 * @note The use of pure I2C transactions is discouraged. When possible,
 * use an appropriate SMBus protocol call instead of this I2C accessor.
 */
extern int dev_i2c_transfer_data(SMBusDevice *client,
        uint8_t write_length, uint8_t *write_data, uint8_t read_length,
        uint8_t *read_data);

/**
 *
 * @param[in] client Handle to slave device
 * @param[in] length Size of data block to read.
 * @param[out] values Byte array into which data will be read.
 * @return negative errno on failure else 0.
 *
 * @note The use of pure I2C transactions is discouraged. When possible,
 * use an appropriate SMBus protocol call instead of this I2C accessor.
 */
extern int dev_i2c_read_data(SMBusDevice *client, uint8_t length,
        uint8_t *data);

/**
 *
 * @param[in] client Handle to slave device
 * @param[in] length Size of data block to write.
 * @param[in] values Byte array from which data will be written.
 * @return negative errno on failure else 0.
 *
 * @note The use of pure I2C transactions is discouraged. When possible,
 * use an appropriate SMBus protocol call instead of this I2C accessor.
 */
extern int dev_i2c_write_data(SMBusDevice *client, uint8_t length,
        uint8_t *data);

#endif /* LIB_I2C_UAPI_H */
