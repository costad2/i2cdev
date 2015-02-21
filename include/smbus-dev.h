/**
 * @file smbus-dev.h
 * @author Danielle Costantino <dcostantino@vmem.com>
 * @copyright Copyright, Violin Memory, Inc, 2014
 *
 * @brief SMBus level access helper functions
 */

#ifndef SMBUS_DEV_H_
#define SMBUS_DEV_H_

extern SMBusAdapter *dev_i2c_new_adapter(dev_bus_adapter *adapter, SMBusDevice *client);

extern int dev_i2c_open_i2c_dev(SMBusAdapter *adapter);
extern int dev_i2c_adapter_close(SMBusAdapter *adapter);

extern SMBusAdapter *dev_i2c_open_adapter(SMBusDevice *client);

extern void dev_i2c_print_functionality(unsigned long functionality);

extern int dev_i2c_get_functionality(SMBusAdapter *adapter);
extern int dev_i2c_set_slave_addr(SMBusAdapter *adapter, int address, int force);
extern int dev_i2c_set_adapter_timeout(SMBusAdapter *adapter, int timeout_ms);
extern int dev_i2c_set_adapter_retries(SMBusAdapter *adapter, unsigned long retries);

/* Return the adapter number for a specific adapter */
static inline int i2c_adapter_id(SMBusAdapter *adap) {
    return adap->nr;
}

/* Return 1 if adapter supports everything we need, 0 if not. */
static inline int i2c_check_functionality(SMBusAdapter *adap, unsigned long func) {
    return ((func & adap->funcs) == func);
}

#endif /* SMBUS_DEV_H_ */
