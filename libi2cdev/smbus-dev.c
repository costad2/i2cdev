/**
 * @file smbus-dev.c
 * @author Danielle Costantino <dcostantino@vmem.com>
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief libi2cdev SMBus access functions
 */

#define _GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/ioctl.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/types.h>

#include <busses.h>
#include <i2c-uapi.h>
#include <i2c-error.h>
#include <i2c/smbus.h>

#include "common.h"
#include "sysfs.h"
#include "smbus-dev.h"
#include "data.h"
#include "i2cdiscov.h"
#include "i2c-dev-parser.h"
#include "../version.h"

/* As of now the build system does not define O_CLOEXEC so it was necessary to define it here. */
#ifndef O_CLOEXEC
# define O_CLOEXEC  02000000 /* Set close_on_exec.  */
#endif

const char *libi2cdev_version = LIBI2CDEV_VERSION;

#define FORCE_I2C_DEV_ADDRESS_OVER 1

static int i2c_check_client_addr_validity(SMBusDevice *client);

/**
 * i2c_new_device - instantiate an i2c device
 * @adap: the adapter managing the device
 * @info: describes one I2C device; bus_num is ignored
 *
 * Create an i2c device.
 *
 * This returns the new i2c client, which may be saved for later use with
 * dev_i2c_delete(); or NULL to indicate an error.
 */
SMBusDevice *dev_i2c_new_device(struct dev_i2c_board_info const *info)
{
    SMBusDevice *client = NULL;
    int err = 0;

    if (!info) {
        err = -EINVAL;
        goto out_err;
    }

    if (!info->path) {
        err = -EINVAL;
        goto out_err;
    }

    client = calloc(1, sizeof(*client));
    if (!client) {
        err = -ENOMEM;
        goto out_err;
    }
    client->force = FORCE_I2C_DEV_ADDRESS_OVER;
    client->flags = info->flags;
    client->addr = info->addr;

    err = i2c_check_client_addr_validity(client);
    if (err < 0) {
        goto out_err;
    }

    strncpy(client->name, info->name, sizeof(client->name));
    strncpy(client->path, info->path, sizeof(client->path));

    client->adapter = NULL;

    devi2c_debug(NULL, "client [%s] registered at 0x%02x path: %s\n",
            client->name,
            client->addr, client->path);

    return client;

out_err:
    devi2c_warn(NULL, "Failed to register i2c client (%s)\n", strerror(-err));
    if (client) {
        free(client);
    }
    return NULL;
}

static const struct i2cdev_func {
    long value;
    const char* name;
} i2cdev_all_func[] = {
        { .value = I2C_FUNC_I2C,
                .name = "I2C" },
        { .value = I2C_FUNC_SMBUS_QUICK,
                .name = "SMBus Quick Command" },
        { .value = I2C_FUNC_SMBUS_WRITE_BYTE,
                .name = "SMBus Send Byte" },
        { .value = I2C_FUNC_SMBUS_READ_BYTE,
                .name = "SMBus Receive Byte" },
        { .value = I2C_FUNC_SMBUS_WRITE_BYTE_DATA,
                .name = "SMBus Write Byte" },
        { .value = I2C_FUNC_SMBUS_READ_BYTE_DATA,
                .name = "SMBus Read Byte" },
        { .value = I2C_FUNC_SMBUS_WRITE_WORD_DATA,
                .name = "SMBus Write Word" },
        { .value = I2C_FUNC_SMBUS_READ_WORD_DATA,
                .name = "SMBus Read Word" },
        { .value = I2C_FUNC_SMBUS_PROC_CALL,
                .name = "SMBus Process Call" },
        { .value = I2C_FUNC_SMBUS_WRITE_BLOCK_DATA,
                .name = "SMBus Block Write" },
        { .value = I2C_FUNC_SMBUS_READ_BLOCK_DATA,
                .name = "SMBus Block Read" },
        { .value = I2C_FUNC_SMBUS_BLOCK_PROC_CALL,
                .name = "SMBus Block Process Call" },
        { .value = I2C_FUNC_SMBUS_PEC,
                .name = "SMBus PEC" },
        { .value = I2C_FUNC_SMBUS_WRITE_I2C_BLOCK,
                .name = "I2C Block Write" },
        { .value = I2C_FUNC_SMBUS_READ_I2C_BLOCK,
                .name = "I2C Block Read" },
        { .value = 0, .name = "" }
};

void dev_i2c_print_functionality(unsigned long funcs)
{
    int i;
    for (i = 0; i2cdev_all_func[i].value; i++) {
        fprintf(stdout, "%-32s %s\n", i2cdev_all_func[i].name,
                (funcs & i2cdev_all_func[i].value) ? "yes" : "no");
    }
}

/* This is a permissive address validity check, I2C address map constraints
 * are purposely not enforced, except for the general call address. */
static int i2c_check_client_addr_validity(SMBusDevice *client)
{
    if (client->flags & I2C_CLIENT_TEN) {
        /* 10-bit address, all values are valid */
        if (client->addr > 0x3ff) {
            return -EINVAL;
        }
    } else {
        /* 7-bit address, reject the general call address */
        if (client->addr == 0x00 || client->addr > 0x7f) {
            return -EINVAL;
        }
    }
    return 0;
}

#define MAXPATH 16

int dev_i2c_open_i2c_dev(SMBusAdapter *adapter)
{
    int err = 0;
    char filename[MAXPATH];
    struct stat st;

    if (!adapter) {
        return -EINVAL;
    }
    if ((adapter->nr < 0) || (adapter->nr > 255)) {
        return -ECHRNG;
    }

    err = snprintf(filename, sizeof(filename), "/dev/i2c-%d", adapter->nr);
    if (err < 0) {
        return -errno;
    }

    if (stat(filename, &st) < 0) {
        return -errno;
    } else {
        if ((st.st_dev != adapter->char_dev) || (st.st_ino != adapter->char_dev_uid)) {
            if (adapter->ready) {
                adapter->ready = false;
                libi2cdev_invalidate_cache();
                devi2c_warn(NULL, "I2C adapter st_ino and st_dev do not match current i2c-dev \"%s\"", filename);
                return -EBADF;
            }
            adapter->char_dev = st.st_dev;
            adapter->char_dev_uid = st.st_ino;
        }
    }

    /* open is called here with the nonblocking flag to allow multiple
     * processes access to the same i2c-dev. This is needed because of
     * how the kernel i2c ioctl interfaces with the open file descriptor. */
    adapter->fd = open(filename, (O_RDWR | O_NONBLOCK | O_CLOEXEC));

    if (adapter->fd < 0) {
        err = -errno;
    } else {
        err = 0;
    }
    return (err);
}

int dev_i2c_get_functionality(SMBusAdapter *adapter)
{
    int ret = 0;
    if (!adapter) {
        return -EINVAL;
    }
    if (ioctl(adapter->fd, I2C_FUNCS, &adapter->funcs) < 0) {
        ret = -errno;
    }
    return ret;
}

int dev_i2c_set_slave_addr(SMBusAdapter *adapter, int address, int force)
{
    int ret = 0;
    if (!adapter) {
        return -EINVAL;
    }
    /* With force, let the user read from/write to the registers
     even when a driver is also running */
    if (ioctl(adapter->fd, force ? I2C_SLAVE_FORCE : I2C_SLAVE, address) < 0) {
        ret = -errno;
    }
    return ret;
}

/* set timeout is in units of 10 ms so multiply by 10 */
int dev_i2c_set_adapter_timeout(SMBusAdapter *adapter, int timeout_ms)
{
    int ret = 0;
    unsigned long timeout = DIV_ROUND_CLOSEST(timeout_ms, 10);

    if (timeout == 0) {
        return -EINVAL;
    }
    if (!adapter) {
        return -ENODEV;
    }
    if (ioctl(adapter->fd, I2C_TIMEOUT, timeout) < 0) {
        ret = -errno;
    }
    return ret;
}

/* number of times a device address should be polled when not acknowledging */
int dev_i2c_set_adapter_retries(SMBusAdapter *adapter, unsigned long retries)
{
    int ret = 0;

    if (!adapter) {
        return -ENODEV;
    }
    if (ioctl(adapter->fd, I2C_RETRIES, retries) < 0) {
        ret = -errno;
    }
    return ret;
}

/**
 * dev_i2c_close - closes the adapter on the i2c device (called internally)
 * @param client
 * @return
 */
int dev_i2c_adapter_close(SMBusAdapter *adapter)
{
    if (!adapter) {
        return -EINVAL;
    }
    if (adapter->fd >= 0) {
        close(adapter->fd);
        adapter->fd = -1;
    }
    return 0;
}

static int register_client_node(dev_client_user_list *list, SMBusDevice *client)
{
    struct dev_client_list *client_node = NULL;
    if (!list || !client) {
        return -EINVAL;
    }
    if (client->client_node == NULL) {
        client_node = calloc(1, sizeof(*client_node));
        if (client_node == NULL) {
            return -ENOMEM;
        }
        client_node->client = client;
        // Add the client to the adapter's list of registered clients
        LIST_INSERT_HEAD(list, client_node, node);
    }
    // else the client was already added to it's adapter's client list so return 0
    return 0;
}

static void remove_client_node(SMBusDevice *client)
{
    if (!client) {
        return;
    }
    if (client->client_node != NULL) {
        struct dev_client_list *client_list = client->client_node;
        LIST_REMOVE(client_list, node);
        free(client_list);
        client_list = NULL;
    }
    client->client_node = NULL;
    // else the client was already removed from it's adapter's client list so return
}

/**
 * dev_i2c_new_adapter - Creates an adapter structure for the adapter id passed in param bus.
 * @return
 */
SMBusAdapter *dev_i2c_new_adapter(dev_bus_adapter *adapter, SMBusDevice *client)
{
    int err = 0;
    SMBusAdapter *adap = NULL;

    if (!adapter) {
        return NULL;
    }

    adap = &adapter->i2c_adapt;

    if (adap->fd >= 0) {
        devi2c_warn(client, "Adapter already has a file descriptor/open device!");
        goto error_exit;
    }

    if (client != NULL) {
        err = register_client_node(&adapter->user_clients, client);
        if (err < 0) {
            goto error_exit;
        }
    }

    adap->nr = adapter->nr;
    adap->name = adapter->name;
    adap->fd = -1;
    adap->prev_addr = -1;
    adap->funcs = 0;
    adap->ready = false;

    err = dev_i2c_open_i2c_dev(adap);
    if (err < 0) {
        goto exit_return;
    } else {
        dev_i2c_get_functionality(adap);
        dev_i2c_adapter_close(adap);
    }

    adap->ready = true;
    devi2c_debug(client, "Added new adapter to client list on i2c-%d adapter", adap->nr);

exit_return:
    return adap;

error_exit:
    return NULL;
}

/**
 * dev_i2c_close - closes the adapter on the i2c device (called internally)
 * @param client
 * @return
 */
int dev_i2c_close(SMBusDevice *client)
{
    SMBusAdapter *adap = NULL;

    if (!client) {
        return -EINVAL;
    }

    /** @note not having an adapter (the device was never opened with dev_i2c_open ) is not an error we should correct */
    adap = client->adapter;

    if (adap != NULL) {
        if (adap->fd >= 0) {
            close(adap->fd);
            adap->fd = -1;
        }
    }
    return 0;
}

/**
 * dev_i2c_delete - Deallocates and closes the client device passed into the function.
 * @param client
 */
void dev_i2c_delete(SMBusDevice *client)
{
    if (!client) {
        return;
    }
    dev_i2c_close(client);

    /* If there was an adapter found for this device de-register it from that adapter */
    if (client->adapter) {
        remove_client_node(client);
        client->adapter = NULL;
    }
    free(client);
    return;
}

/**
 *
 * @param client
 * @return
 */
int dev_i2c_open(SMBusDevice *client)
{
    int ret = 0;
    int scan_ret = -ENODATA;
    dev_bus_adapter *adapt = NULL;
    bool need_adapter = false;
    bool cache_is_valid = false;

    if (!client) {
        return -ENODEV;
    }
    if (!client->path) {
        devi2c_err(client, "ERROR: client has no path specified! - %s", strerror(-ret));
        return -EINVAL;
    }

    cache_is_valid = libi2cdev_check_cache_is_valid();
    if (cache_is_valid) {
        if (client->adapter) {
            ret = dev_i2c_open_i2c_dev(client->adapter);
            if (ret < 0) {
                /* The call to dev_i2c_open_i2c_dev can invalidate the cache
                 * this would require a rescan then looking up the device based on path.
                 */
                cache_is_valid = libi2cdev_check_cache_is_valid();
                if (!cache_is_valid) {
                    /** @note if i2cdev_rescan returns 0 then the cache is valid */
                    scan_ret = i2cdev_rescan();
                    if (scan_ret < 0) {
                        goto fatal_error;
                    } else {
                        need_adapter = true;
                        cache_is_valid = true;
                    }
                }  /* else you don't need a new adapter */
            }  /* else you don't need a new adapter */
        } else {
            need_adapter = true;
        }
    } else {
        goto fatal_error;
    }

    if (need_adapter) {
        adapt = dev_i2c_lookup_i2c_bus(client->path);
        if (!adapt) {
            ret = -ENODEV;
            devi2c_err(client, "Could not find i2c adapter - %s", strerror(ENODEV));
            goto exit_return;
        }
        client->adapter = dev_i2c_new_adapter(adapt, client);
        if (!client->adapter) {
            ret = -ENODEV;
            devi2c_err(client, "an adapter with client path [%s] could not be found!", client->path);
            goto exit_return;
        }
        ret = dev_i2c_open_i2c_dev(client->adapter);
    }

exit_return:

    return ret;

fatal_error:

    devi2c_err(client, "During device lookup libi2cdev failed to update cache - %s", strerror(-scan_ret));
    return scan_ret;
}

/**
 *
 * @param client
 * @return
 */
SMBusAdapter *dev_i2c_open_adapter(SMBusDevice *client)
{
    int ret = 0;
    ret = dev_i2c_open(client);

    if (ret < 0) {
        return NULL;
    } else {
        return client->adapter;
    }
}

static SMBusDevice dummy_client = {
    .addr = 0,
    .name = "dummy",
    .force = 1,
    .adapter = NULL,
};

#define MODE_AUTO   0
#define MODE_QUICK  1
#define MODE_READ   2

/**
 * dev_i2c_smbus_probe - SMBus Probe
 * @param addr: address to check
 * @param path: path to check
 * @param mode: mode to probe
 *
 * @brief This executes either the SMBus "receive byte" protocol or write byte, returning negative errno
 * else 0
 */
int32_t dev_i2c_smbus_probe(uint8_t addr, const char *path, int mode)
{
    int err = 0;
    __s32 ret = 0;
    int cmd = 0;
    dev_bus_adapter *adapter = NULL;
    SMBusDevice *client = &dummy_client;

    if (!path) {
        return -EINVAL;
    }

    adapter = dev_i2c_lookup_i2c_bus(path);

    if (adapter == NULL) {
        return -ENODEV;
    }

    dummy_client.adapter = &(adapter->i2c_adapt);
    dummy_client.addr = addr;

    err = i2c_check_client_addr_validity(client);
    if (err < 0) {
        return err;
    }

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }

    err = dev_i2c_get_functionality(client->adapter);
    if (err < 0) {
        goto error_exit;
    }

    switch (mode) {
    case MODE_AUTO:
        // Use a read function for eeproms to prevent data corruption.
        cmd = ((addr >= 0x30 && addr <= 0x37)
                || (addr >= 0x50 && addr <= 0x5F)) ? MODE_READ : MODE_QUICK;
        break;
    default:
        cmd = mode;
        break;
    }

    err = dev_i2c_set_slave_addr(client->adapter, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    /* Probe this address */
    switch (cmd) {
    case MODE_READ:
        /* This is known to lock SMBus on various write-only chips */
        ret = i2c_smbus_read_byte(client->adapter->fd);
        break;
    case MODE_QUICK:
    default:
        /* This is known to corrupt some EEPROMs */
        ret = i2c_smbus_write_quick(client->adapter->fd, I2C_SMBUS_WRITE);
        break;
    }

    dev_i2c_close(client);
    return ret;

error_exit:

    dev_i2c_close(client);
    return err;
}

/**
 *
 * @param client
 * @param value
 * @return
 */
int32_t dev_i2c_smbus_write_quick(SMBusDevice *client, uint8_t value)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_write_quick(adap->fd, value);

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;
}

/**
 * dev_i2c_smbus_read_byte - SMBus "receive byte" protocol
 * @param client: Handle to slave device
 *
 * @brief This executes the SMBus "receive byte" protocol, returning negative errno
 * else the byte received from the device.
 */
int32_t dev_i2c_smbus_read_byte(SMBusDevice *client)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_read_byte(adap->fd);

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;
}

/**
 * dev_i2c_smbus_write_byte - SMBus "send byte" protocol
 * @param client: Handle to slave device
 * @param value: Byte to be sent
 *
 * @brief This executes the SMBus "send byte" protocol, returning negative errno
 * else zero on success.
 */
int32_t dev_i2c_smbus_write_byte(SMBusDevice *client, uint8_t value)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_write_byte(adap->fd, value);

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;
}

/**
 * dev_i2c_smbus_read_byte_data - SMBus "read byte" protocol
 * @param client: Handle to slave device
 * @param command: Byte interpreted by slave
 *
 * @brief This executes the SMBus "read byte" protocol, returning negative errno
 * else a data byte received from the device.
 */
int32_t dev_i2c_smbus_read_byte_data(SMBusDevice *client, uint8_t command)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_read_byte_data(adap->fd, command);

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;

}

/**
 * dev_i2c_smbus_write_byte_data - SMBus "write byte" protocol
 * @param client: Handle to slave device
 * @param command: Byte interpreted by slave
 * @param value: Byte being written
 *
 * @brief This executes the SMBus "write byte" protocol, returning negative errno
 * else zero on success.
 */
int32_t dev_i2c_smbus_write_byte_data(SMBusDevice *client, uint8_t command,
        uint8_t value)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_write_byte_data(adap->fd, command, value);

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;
}

/**
 * dev_i2c_smbus_read_word_data - SMBus "read word" protocol
 * @param client: Handle to slave device
 * @param command: Byte interpreted by slave
 *
 * This executes the SMBus "read word" protocol, returning negative errno
 * else a 16-bit unsigned "word" received from the device.
 */
int32_t dev_i2c_smbus_read_word_data(SMBusDevice *client, uint8_t command)
{
    int err = 0;
    __s32 ret = 0;

    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_read_word_data(adap->fd, command);

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;
}

/**
 * dev_i2c_smbus_write_word_data - SMBus "write word" protocol
 * @param client: Handle to slave device
 * @param command: Byte interpreted by slave
 * @param value: 16-bit "word" being written
 *
 * This executes the SMBus "write word" protocol, returning negative errno
 * else zero on success.
 */
int32_t dev_i2c_smbus_write_word_data(SMBusDevice *client, uint8_t command,
        uint16_t value)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_write_word_data(adap->fd, command, value);

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;
}

int32_t dev_i2c_smbus_process_call(SMBusDevice *client, uint8_t command,
        uint16_t value)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_process_call(adap->fd, command, value);
    if (ret < 0) {
        err = ret;
        goto error_exit;
    }

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;

}

/**
 * dev_i2c_smbus_read_block_data - SMBus "block read" protocol
 * @param client: Handle to slave device
 * @param command: Byte interpreted by slave
 * @param values: Byte array into which data will be read; big enough to hold
 *  the data returned by the slave.  SMBus allows at most 32 bytes.
 *
 * This executes the SMBus "block read" protocol, returning negative errno
 * else the number of data bytes in the slave's response.
 *
 * Note that using this function requires that the client's adapter support
 * the I2C_FUNC_SMBUS_READ_BLOCK_DATA functionality.  Not all adapter drivers
 * support this; its emulation through I2C messaging relies on a specific
 * mechanism (I2C_M_RECV_LEN) which may not be implemented.
 */
int32_t dev_i2c_smbus_read_block_data(SMBusDevice *client, uint8_t command,
        uint8_t *values)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_read_block_data(adap->fd, command, values);
    if (ret < 0) {
        err = ret;
        goto error_exit;
    }

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;

}

/**
 * dev_i2c_smbus_write_block_data - SMBus "block write" protocol
 * @param client: Handle to slave device
 * @param command: Byte interpreted by slave
 * @param length: Size of data block; SMBus allows at most 32 bytes
 * @param values: Byte array which will be written.
 *
 * This executes the SMBus "block write" protocol, returning negative errno
 * else zero on success.
 */
int32_t dev_i2c_smbus_write_block_data(SMBusDevice *client, uint8_t command,
        uint8_t length, const uint8_t *values)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_write_block_data(adap->fd, command, length, values);
    if (ret < 0) {
        err = ret;
        goto error_exit;
    }

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;

}

/* Returns the number of read bytes */
int32_t dev_i2c_smbus_read_i2c_block_data(SMBusDevice *client, uint8_t command,
        uint8_t length, uint8_t *values)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_read_i2c_block_data(adap->fd, command, length, values);
    if (ret < 0) {
        err = ret;
        goto error_exit;
    }

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;

}

int32_t dev_i2c_smbus_write_i2c_block_data(SMBusDevice *client, uint8_t command,
        uint8_t length, const uint8_t *values)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_write_i2c_block_data(adap->fd, command, length, values);
    if (ret < 0) {
        err = ret;
        goto error_exit;
    }

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;

}

/* Returns the number of read bytes */
int32_t dev_i2c_smbus_block_process_call(SMBusDevice *client, uint8_t command,
        uint8_t length, uint8_t *values)
{
    int err = 0;
    __s32 ret = 0;
    SMBusAdapter *adap = NULL;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }
    adap = client->adapter;

    err = dev_i2c_set_slave_addr(adap, client->addr, client->force);
    if (err < 0) {
        goto error_exit;
    }

    ret = i2c_smbus_block_process_call(adap->fd, command, length, values);
    if (ret < 0) {
        err = ret;
        goto error_exit;
    }

    err = dev_i2c_close(client);

    return (err < 0) ? err : ret;

error_exit:
    dev_i2c_close(client);
    return err;
}


static int i2c_transfer(SMBusAdapter *adap, struct i2c_msg *msgs, int num)
{
    int err = 0;

    assert(adap);

    struct i2c_rdwr_ioctl_data msgset = {
        .msgs = msgs,
        .nmsgs = num,
    };

    err = ioctl(adap->fd, I2C_RDWR, &msgset);
    if (err < 0) {
        err = -errno;
    }

    return err;
}

int dev_i2c_transfer_data(SMBusDevice *client,
        uint8_t write_length, uint8_t *write_data, uint8_t read_length,
        uint8_t *read_data)
{
    int err = 0;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }

    struct i2c_msg msgs[2] = {
        {
            .addr = client->addr,
            .flags = client->flags,
            .len = write_length,
            .buf = write_data,
        },
        {
            .addr = client->addr,
            .flags = (client->flags | I2C_M_RD),
            .len = read_length,
            .buf = read_data,
        },
    };

    err = i2c_transfer(client->adapter, msgs, 2);

    dev_i2c_close(client);
    return err;
}

int dev_i2c_write_data(SMBusDevice *client, uint8_t length,
        uint8_t *data)
{
    int err = 0;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }

    struct i2c_msg msgs = {
        .addr = client->addr,
        .flags = client->flags,
        .len = length,
        .buf = data,
    };

    err = i2c_transfer(client->adapter, &msgs, 1);

    dev_i2c_close(client);
    return err;
}

int dev_i2c_read_data(SMBusDevice *client, uint8_t length,
        uint8_t *data)
{
    int err = 0;

    err = dev_i2c_open(client);
    if (err < 0) {
        return err;
    }

    struct i2c_msg msgs = {
        .addr = client->addr,
        .flags = (client->flags | I2C_M_RD),
        .len = length,
        .buf = data,
    };

    err = i2c_transfer(client->adapter, &msgs, 1);

    dev_i2c_close(client);
    return err;
}
