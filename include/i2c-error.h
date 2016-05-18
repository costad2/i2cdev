/**
 * @file i2c-error.h
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief libi2cdev error codes and logging
 */

#ifndef LIB_I2CDEV_ERROR_H
#define LIB_I2CDEV_ERROR_H

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#define I2CDEV_ERR_EAGAIN     1 /* Arbitration lost */
#define I2CDEV_ERR_EBADMSG    2 /* Invalid Packet Error Code */
#define I2CDEV_ERR_EBUSY      3 /* SMBus adapter busy */
#define I2CDEV_ERR_EINVAL     4 /* Invalid argument */
#define I2CDEV_ERR_EIO        5 /* I/O error */
#define I2CDEV_ERR_ENODEV     6 /* No such device */
#define I2CDEV_ERR_ENXIO      7 /* Transfer didn't get an ACK */
#define I2CDEV_ERR_EOPNOTSUPP 8 /* Operation not supported */
#define I2CDEV_ERR_EPROTO     9 /* Slave does not conform to I2C/SMBus protocol */
#define I2CDEV_ERR_ETIMEDOUT 10 /* I2C Operation timed out */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Magic number definition must be in sync with enum below */
#define LIB_SMB_I2C_MAGIC   0x736D6232
#define LIB_SMB_I2C_POISON  0x6E736D62

/* This enum defines the library's internal state */
typedef enum i2csmbmagic_e {
    LIB_SMB_UNINIIALIZED = 0,
    LIB_SMB_BUSY = 1, /**< Indicates that the library is busy but no fault has occurred */
    LIB_SMB_READY = LIB_SMB_I2C_MAGIC, /**< This is the standard runtime state of the library when properly initialized */
    LIB_SMB_NOT_READY = LIB_SMB_I2C_POISON, /**<  If the library is in this state when an i2cdev internal call is made the library will call assert */
    LIB_SMB_UNKNOWN, /**< the status is unknown */
} libi2cdev_state_t;

extern enum i2csmbmagic_e *p_i2csmbmagic_state;

/* This function returns a pointer to a string which describes the error.
   errnum may be negative (the corresponding positive error is returned).
   You may not modify the result! */
const char *i2cdev_strerror(int errnum);

extern void libi2cdev_invalidate_cache(void);
extern void libi2cdev_clear_invalidate_flag(void);
extern bool libi2cdev_check_cache_is_valid(void);

extern enum i2csmbmagic_e get_libi2cdev_state(void);
extern int set_libi2cdev_state(enum i2csmbmagic_e state);

static inline bool check_libi2cdev_ready(void) {
    if (*p_i2csmbmagic_state == LIB_SMB_I2C_POISON) {
        fprintf(stderr, "libi2cdev internal call made before initialization of library!");
        assert(*p_i2csmbmagic_state != LIB_SMB_I2C_POISON);
        return false;
    } else {
        return (*p_i2csmbmagic_state == LIB_SMB_I2C_MAGIC)? true : false;
    }
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* def LIB_I2CDEV_ERROR_H */
