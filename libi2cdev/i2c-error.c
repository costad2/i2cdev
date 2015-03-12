/**
 * @file i2c-error.c
 * @copyright Violin Memory, Inc, 2014
 *
 * @brief libi2cdev error codes and logging
 */

#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/syslog.h>

#include "common.h"
#include "i2c-error.h"

#include <libi2cdev.h>
#include <syslog.h>

static void devi2c_log_internal(SMBusDevice *devi2c, int priority, const char *format, ...) __attribute__((format(printf,3,4)));

static void i2cdev_default_parse_error_wfn(const char *err, const char *filename, int lineno);
static void i2cdev_default_fatal_error(const char *proc, const char *err);

devi2c_log_func_t devi2c_log __attribute__((format(printf,3,4))) = devi2c_log_internal;

void (*dev_parse_error_wfn) (const char *err, const char *filename, int lineno) = i2cdev_default_parse_error_wfn;
void (*dev_fatal_error) (const char *proc, const char *err) = i2cdev_default_fatal_error;

FILE *libi2cdev_stderr = NULL;

static unsigned long libi2csmbmagic = LIB_SMB_UNINIIALIZED;
enum i2csmbmagic_e *p_i2csmbmagic_state = (enum i2csmbmagic_e *)&libi2csmbmagic;

static bool i2cdev_rescan_required = false;

const char *i2cerrorlist[] = {
    /* Invalid error code    */ "Unknown error",
    /* I2CDEV_ERR_EAGAIN     */ "Arbitration lost",
    /* I2CDEV_ERR_EBADMSG    */ "Invalid Packet Error Code",
    /* I2CDEV_ERR_EBUSY      */ "SMBus adapter busy",
    /* I2CDEV_ERR_EINVAL     */ "Invalid argument",
    /* I2CDEV_ERR_EIO        */ "I/O error",
    /* I2CDEV_ERR_ENODEV     */ "No such device",
    /* I2CDEV_ERR_ENXIO      */ "Transfer didn't get an ACK",
    /* I2CDEV_ERR_EOPNOTSUPP */ "Operation not supported",
    /* I2CDEV_ERR_EPROTO     */ "Slave does not conform to I2C/SMBus protocol",
    /* I2CDEV_ERR_ETIMEDOUT  */ "I2C Operation timed out",
};

#define NUM_ERR \
  (sizeof(i2cerrorlist) / sizeof(i2cerrorlist[0]))
const int i2cdev_nerr_internal = NUM_ERR;

const char *i2cdev_strerror(int errnum)
{
    if (errnum < 0) {
        errnum = -errnum;
    }
    if (errnum >= i2cdev_nerr_internal) {
        errnum = 0;
    }
    return i2cerrorlist[errnum];
}

void i2cdev_default_parse_error_wfn(const char *err, const char *filename,
        int lineno)
{
    if (lineno) {
        devi2c_err(NULL, "Error: File %s, line %d: %s", filename, lineno, err);
    } else {
        devi2c_err(NULL, "Error: File %s: %s", filename, err);
    }
}

void i2cdev_default_fatal_error(const char *proc, const char *err)
{
    devi2c_err(NULL, "Fatal error in `%s': %s", proc, err);
    assert(false);
}

int set_libi2cdev_state(enum i2csmbmagic_e state)
{
    libi2csmbmagic = state;
    return 0;
}

bool libi2cdev_check_cache_is_valid(void)
{
    if ((i2cdev_rescan_required == true) && (get_libi2cdev_state() == LIB_SMB_READY)) {
        return false;
    } else {
        return true;
    }
}

void libi2cdev_invalidate_cache(void)
{
    i2cdev_rescan_required = true;
}

void libi2cdev_clear_invalidate_flag(void)
{
    i2cdev_rescan_required = false;
}

enum i2csmbmagic_e get_libi2cdev_state(void)
{
    switch (libi2csmbmagic) {
        case LIB_SMB_UNINIIALIZED:
            return LIB_SMB_UNINIIALIZED;
        break;
        case LIB_SMB_BUSY:
            return LIB_SMB_BUSY;
        break;
        case LIB_SMB_READY:
            return LIB_SMB_READY;
        break;
        case LIB_SMB_NOT_READY:
            return LIB_SMB_NOT_READY;
        break;
        default:
            return LIB_SMB_UNKNOWN;
        break;
    }
    return LIB_SMB_UNKNOWN;
}

#define MSG_PREFIX_LEN  512

static void vdevi2c_log(SMBusDevice *devi2c, int priority, const char *format, va_list ap)
{
    char message_prefix[MSG_PREFIX_LEN];
    char *msg_pre = NULL;

    if (!devi2c) {
        snprintf(message_prefix, sizeof(message_prefix), "[libi2cdev]: %s", format);
    } else {
        snprintf(message_prefix, sizeof(message_prefix), "[%s %s 0x%02x]: %s", (devi2c)->name, (devi2c)->path, (devi2c)->addr, format);
    }
    msg_pre = message_prefix;
    vsyslog(priority, msg_pre, ap);
}

static void vdevi2c_print(SMBusDevice *devi2c, int priority, const char *format, va_list ap)
{
    if (!devi2c) {
        fprintf(libi2cdev_stderr, "[libi2cdev] <%d>: ", priority);
    } else {
        fprintf(libi2cdev_stderr, "[%s %s 0x%02x] <%d>: ", devi2c->name, devi2c->path, devi2c->addr, priority);
    }
    vfprintf(libi2cdev_stderr, format, ap);
    if (!strrchr(format, '\n')) {
        fputc('\n', libi2cdev_stderr);
    }
}

void devi2c_set_logging_function(devi2c_log_func_t func)
{
    if (func == NULL) {
        devi2c_log = devi2c_log_internal;
    } else {
        devi2c_log = func;
    }
}

static void devi2c_log_internal(SMBusDevice *devi2c, int priority, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vdevi2c_log(devi2c, priority, format, ap);
    va_end(ap);
}

void devi2c_print(SMBusDevice *devi2c, int priority, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vdevi2c_print(devi2c, priority, format, ap);
    va_end(ap);
}

