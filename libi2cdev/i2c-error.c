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

devi2c_log_func_t devi2c_log __attribute__((format(printf,6,7))) = devi2c_syslog;

static unsigned int devi2c_log_level = LOG_NOTICE;
static unsigned int devi2c_log_level_error = LOG_WARNING;

typedef struct _code {
	const char *c_name;
	int c_val;
} CODE;

/* max string length of all the lookup priority names */
#define PRIORITY_NAME_MAX_LEN 7

static const char *get_log_priority_name(int priority)
{
	const char *pri_name = NULL;

	const CODE prioritynames[] = {
		{ "alert", LOG_ALERT },
		{ "crit", LOG_CRIT },
		{ "debug", LOG_DEBUG },
		{ "emerg", LOG_EMERG },
		{ "error", LOG_ERR },
		{ "info", LOG_INFO },
		{ "notice", LOG_NOTICE },
		{ "warning", LOG_WARNING },
		{ NULL, -1 }
	};

	for (int i = 0; prioritynames[i].c_name; ++i) {
		if (prioritynames[i].c_val == priority) {
			pri_name = prioritynames[i].c_name;
			break;
		}
	}
	return pri_name;
}

/* See devi2c.h for API */
void devi2c_log_set_level(unsigned int new_pri)
{
	devi2c_log_level = (new_pri > LOG_DEBUG) ? LOG_DEBUG : new_pri;
	setlogmask(LOG_UPTO(new_pri));
}

/* See devi2c.h for API */
unsigned int devi2c_get_log_level(void)
{
	return devi2c_log_level;
}

const char *devi2c_get_log_level_string(void)
{
	return get_log_priority_name(devi2c_log_level);
}

static void vplog_at_line(SMBusDevice *devi2c, int priority, const char *file,
		unsigned int line, const char *func, const char *format, va_list ap)
{
	char prefix[MSG_PREFIX_LEN];
	char *msg_pre = NULL;
	const char *pri_name = get_log_priority_name(priority);
	int len = strlen(pri_name);
	len = PRIORITY_NAME_MAX_LEN - len;
	if (!devi2c) {
		snprintf(prefix, sizeof(prefix),
				"[%s]%*s: %s - %s:%u: In function ‘%s’", pri_name, len, "",
				format, file, line, func);
	} else {
		snprintf(prefix, sizeof(prefix), "[%s][%s %s 0x%02x]: %s "
				"- %s:%u: In function ‘%s’", pri_name, (devi2c)->name, (devi2c)->path, (devi2c)->addr, format, file, line, func);

	}
	msg_pre = prefix;
	vsyslog(priority, msg_pre, ap);
}

static void vprint_at_line(SMBusDevice *devi2c, int priority, const char *file,
		unsigned int line, const char *func, const char *format, va_list ap)
{
	FILE *fp = NULL;
	if (priority <= devi2c_log_level) {
		const char *pri_name = get_log_priority_name(priority);
		int len = strlen(pri_name);
		len = PRIORITY_NAME_MAX_LEN - len;
		if (!devi2c) {
			fp = stderr;
			fprintf(fp, "[%s]%*s: ", pri_name, len, "");
		} else {
			fp = stderr;
			fprintf(fp, "[%s][%s %s 0x%02x]: ", pri_name, (devi2c)->name, (devi2c)->path, (devi2c)->addr);
		}
		char *fmt_tmp = strdupa(format);
		if (format[strlen(format) - 1] == '\n') {
			fmt_tmp[strlen(format) - 1] = '\0';
		}
		vfprintf(fp, fmt_tmp, ap);
		fprintf(fp, " : %s:%u: In function ‘%s’\n", file, line, func);
		if (fp == stderr) {
			fflush(fp);
		}
	}
}

static void vplog_local(SMBusDevice *devi2c, int priority, const char *format,
		va_list ap)
{
	char prefix[MSG_PREFIX_LEN];
	char *msg_pre = NULL;
	const char *pri_name = get_log_priority_name(priority);
	int len = strlen(pri_name);
	len = PRIORITY_NAME_MAX_LEN - len;
	if (!devi2c) {
		snprintf(prefix, sizeof(prefix), "[%s]%*s: %s", pri_name, len, "", format);
	} else {
		snprintf(prefix, sizeof(prefix), "[%s][%s %s 0x%02x]: %s", pri_name, (devi2c)->name, (devi2c)->path, (devi2c)->addr, format);
	}
	msg_pre = prefix;
	vsyslog(priority, msg_pre, ap);
}

static void vprint_local(SMBusDevice *devi2c, int priority, const char *format,
		va_list ap)
{
	FILE *fp = NULL;
	if (priority <= devi2c_log_level) {
		const char *pri_name = get_log_priority_name(priority);
		int len = strlen(pri_name);
		len = PRIORITY_NAME_MAX_LEN - len;
		fp = (priority <= devi2c_log_level_error) ? stderr : stdout;
		if (!devi2c) {
			fprintf(fp, "[%s]%*s: ", pri_name, len, "");
		} else {
			fprintf(fp, "[%s][%s %s 0x%02x]: ", pri_name, (devi2c)->name, (devi2c)->path, (devi2c)->addr);
		}
		char *fmt_tmp = strdupa(format);
		vfprintf(fp, fmt_tmp, ap);
		if (!strrchr(fmt_tmp, '\n')) {
			fputc('\n', fp);
		}
		if (fp == stderr) {
			fflush(fp);
		}
	}
}

static void devi2c_vplog(SMBusDevice *devi2c, int priority, const char *file,
		unsigned int line, const char *func, const char *format, va_list ap)
{
	if (priority <= devi2c_log_level_error) {
		vplog_at_line(devi2c, priority, file, line, func, format, ap);
	} else {
		vplog_local(devi2c, priority, format, ap);
	}
}

static void devi2c_vprint(SMBusDevice *devi2c, int priority, const char *file,
		unsigned int line, const char *func, const char *format, va_list ap)
{
	if (priority <= devi2c_log_level_error) {
		vprint_at_line(devi2c, priority, file, line, func, format, ap);
	} else {
		vprint_local(devi2c, priority, format, ap);
	}
}

/* See devi2c.h for API */
void devi2c_set_logging_function(devi2c_log_func_t func)
{
	if (func == NULL) {
		devi2c_log = devi2c_syslog;
	} else {
		devi2c_log = func;
	}
}

/* See devi2c.h for API */
void devi2c_logging_init(unsigned int pri)
{
	openlog(NULL,LOG_PID, pri);
	devi2c_set_logging_function(NULL);
	devi2c_log_set_level(pri);
}

void devi2c_syslog(SMBusDevice *devi2c, int priority, const char *file,
		unsigned int line, const char *func, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	devi2c_vplog(devi2c, priority, file, line, func, format, ap);
	va_end(ap);
}

void devi2c_print(SMBusDevice *devi2c, int priority, const char *file,
		unsigned int line, const char *func, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	devi2c_vprint(devi2c, priority, file, line, func, format, ap);
	va_end(ap);
}
