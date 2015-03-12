/**
 * @file lsi2c.c
 * @author Danielle Costantino <dcostantino@vmem.com>
 * @copyright Copyright Violin Memory Inc. 2014
 * @brief Linux tool for I2C and SMBus device and adapter configuration.
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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif /* _GNU_SOURCE */

#include <unistd.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdbool.h>

#include "busses.h"
#include "i2c-dev-parser.h"
#include "smbus-dev.h"
#include "i2cdiscov.h"

#include "../version.h"

/* Options */
static int verbose_flag;
static int opt_tree; /* Show bus tree */

const char program_name[] = "lsi2c";

static void print_short_help(void)
{
    printf("Try `%s -h' for more information\n", program_name);
}

static void print_long_help(void)
{
    printf("Usage: %s [OPTION]...\n", program_name);
    puts(
            "  -c, --config-file     Specify a config file\n"
            "  -C, --print-config    Display i2c devices in configuration file\n"
            "  -a, --all             Print all i2c-devs in bus tree\n"
            "  -d, --print-devices   Display sysfs i2c devices\n"
            "  -t, --tree            Print i2c bus and children\n"
            "  -p, --path            Parse an i2c-dev path\n"
            "  -P, --probe           Probe an i2c-dev at addr on path\n"
            "  -F, --func            Print I2C bus functionality\n"
            "  -T, --timeout         Set adapter timeout in milliseconds\n"
            "  -S, --retry-count     Set adapter max retry count\n"
            "  -h, --help            Display this help text\n"
            "  -V, --version         Display the program version\n"
            "  -v, --verbose         Be verbose\n"
            "  -i, --initialize      Initialize devices in configuration file\n"
            "  -r, --remove          Remove devices in configuration file\n"
            "  -k, --kmod            Try to initialize i2c_dev kernel module\n"
            "\n"
            "Use `-' after `-c' to read the config file from stdin.\n");
}

static void print_version(void)
{
    printf("%s version, libi2cdev version %s, built %s %s\n", program_name,
            libi2cdev_version, __TIME__, __DATE__);
}

static int get_and_print_adapter_functionality(dev_bus_adapter *adapter)
{
    int err = 0;

    SMBusDevice dummy_client = {
        .addr = 0,
        .name = "dummy",
        .force = 1,
        .adapter = &(adapter->i2c_adapt),
    };

    SMBusDevice *client = &dummy_client;

    err = dev_i2c_open(client);

    if (err < 0) {
        return err;
    }

    err = dev_i2c_get_functionality(client->adapter);

    if (err < 0) {
        dev_i2c_close(client);
        return err;
    }

    dev_i2c_print_functionality(client->adapter->funcs);

    dev_i2c_close(client);

    return err;
}

static int set_adapter_timeout(dev_bus_adapter *adapter, unsigned long timeout)
{
    int err = 0;

    SMBusDevice dummy_client = {
        .addr = 0,
        .name = "dummy",
        .force = 1,
        .adapter = &(adapter->i2c_adapt),
    };

    SMBusDevice *client = &dummy_client;

    err = dev_i2c_open(client);

    if (err < 0) {
        return err;
    }

    err = dev_i2c_set_adapter_timeout(client->adapter, timeout);

    dev_i2c_close(client);

    return err;
}

static int set_adapter_retries(dev_bus_adapter *adapter, unsigned long retries)
{
    int err = 0;

    SMBusDevice dummy_client = {
        .addr = 0,
        .name = "dummy",
        .force = 1,
        .adapter = &(adapter->i2c_adapt),
    };

    SMBusDevice *client = &dummy_client;

    err = dev_i2c_open(client);

    if (err < 0) {
        return err;
    }

    err = dev_i2c_set_adapter_retries(client->adapter, retries);

    dev_i2c_close(client);

    return err;
}

/* Return 0 on success, and an exit error code otherwise */
static int read_config_file(const char *config_file_name)
{
    FILE *config_file = NULL;
    int err = 0;

    if (config_file_name) {
        if (!strcmp(config_file_name, "-")) {
            config_file = stdin;
        } else {
            stdin_config_file_name = config_file_name;
        }
        config_file = fopen(config_file_name, "r");

        if (!config_file) {
            fprintf(stderr, "Could not open config file: %s\n",
                    config_file_name);
            return EXIT_FAILURE;
        }
    } else {
        /* Use libi2cdiscov default */
        config_file = NULL;
    }

    err = i2cdev_init(config_file);

    if (err != 0) {
        if (err < 0) {
            err = -err;
        }
        error(0, err, "Failed to initialize i2cdevices");
        if (config_file) {
            fclose(config_file);
        }
        return EXIT_FAILURE;
    }

    if (config_file && fclose(config_file) == EOF) {
        fprintf(stderr, "Error closing file: %s\n", strerror(errno));
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    int err = EXIT_SUCCESS;
    int c = 0, index_cnt = 0, i = 0;

    const char *config_file_name = NULL;
    const char *address_name = NULL;
    const char *bus_name_path = NULL;
    const char *rescan_arg = NULL;
    const char *retry_count_arg = NULL;
    const char *timeout_arg = NULL;

    bool do_bus_list_all = false;
    bool do_initialize_all_devs = false;
    bool do_initialize_i2c_dev_kmod = false;
    bool do_remove_all_devs = false;
    bool do_print_all_devs = false;
    bool do_print_all_config = false;
    bool do_print_funcs = false;
    bool do_probe_address = false;
    bool do_bus_rescan = false;
    bool do_set_retry_count = false;
    bool do_set_timeout = false;

    int dev_count = 0;
    int adapter_count = 0;
    int rescan_count = 0;
    int address = -1;
    int adapter_timeout_ms = 20;
    int adapter_retry_count = 1;

    dev_bus_adapter *found = NULL;

    struct option long_opts[] =  {
        { "help", no_argument, NULL, 'h' },
        { "version", no_argument, NULL, 'V'},
        { "verbose", no_argument, &verbose_flag, 'v'},
        { "tree", no_argument, &opt_tree, 't' },
        { "all", no_argument, NULL, 'a'},
        { "print-devices", no_argument, NULL, 'd'},
        { "print-config", no_argument, NULL, 'C'},
        { "initialize", no_argument, NULL, 'i'},
        { "remove", no_argument, NULL, 'r'},
        { "kmod", no_argument, NULL, 'k'},
        { "config-file", required_argument, NULL, 'c' },
        { "timeout", required_argument, NULL, 'T' },
        { "retry-count", required_argument, NULL, 'S' },
        { "func", no_argument, NULL, 'F' },
        { "path", required_argument, NULL, 'p' },
        { "probe", required_argument, NULL, 'P' },
        { "rescan", optional_argument, NULL, 'R' },
        { NULL, 0, NULL, 0 }
    };

    while (1) {
        c = getopt_long(argc, argv, "adhCVvtrikFc:p:P:R:S:T:", long_opts, &index_cnt);
        if (c == EOF) {
            break;
        }
        switch(c) {
        case ':':
        case '?':
            print_short_help();
            exit(EXIT_FAILURE);
        case 'h':
            print_long_help();
            exit(EXIT_SUCCESS);
        case 'V':
            print_version();
            exit(EXIT_SUCCESS);
        case 'v':
            i2c_dev_verbose++;
            break;
        case 'c':
            config_file_name = optarg;
            break;
        case 'p':
            bus_name_path = optarg;
            break;
        case 'P':
            address_name = optarg;
            address = strtol(address_name, NULL, 0);
            do_probe_address = true;
            do_initialize_i2c_dev_kmod = true;
            break;
        case 'R':
            rescan_arg = optarg;
            if (rescan_arg != NULL) {
                rescan_count = strtol(rescan_arg, NULL, 0);
            } else {
                rescan_count = 1;
            }
            do_bus_rescan = true;
            break;
        case 'F':
            do_print_funcs = true;
            do_initialize_i2c_dev_kmod = true;
            break;
        case 'T':
            timeout_arg = optarg;
            adapter_timeout_ms = strtol(timeout_arg, NULL, 0);
            do_set_timeout = true;
            do_initialize_i2c_dev_kmod = true;
            break;
        case 'S':
            retry_count_arg = optarg;
            adapter_retry_count = strtol(retry_count_arg, NULL, 0);
            do_set_retry_count = true;
            do_initialize_i2c_dev_kmod = true;
            break;
        case 'C':
            do_print_all_config = true;
            break;
        case 'd':
            do_print_all_devs = true;
            break;
        case 'r':
            do_remove_all_devs = true;
            break;
        case 'a':
            do_bus_list_all = true;
            break;
        case 'k':
            do_initialize_i2c_dev_kmod = true;
            break;
        case 't':
            opt_tree = 1;
            break;
        case 'i':
            do_initialize_all_devs = true;
            break;
        default:
            fprintf(stderr,
                "Internal error while parsing options!\n");
            exit(EXIT_FAILURE);
        }
    }

    if ((argc == 1) || ((argc == 2) && (i2c_dev_verbose > 0))) {
        do_bus_list_all = 1;
    }

    devi2c_set_logging_function(devi2c_print);

    if (i2c_dev_verbose) {
        printf("Searching for i2c devices\n");
    }

    err = read_config_file(config_file_name);
    if (err) {
        exit(err);
    }

    if (do_initialize_i2c_dev_kmod) {
        err = try_load_i2c_dev_mod();
        if (err == 0) {
            err = -EACCES;
            fprintf(stderr, "ERROR: kernel i2c_dev module failed to load: %s\n",strerror(-err));
            goto done;
        }
    }

    if (do_print_all_config) {
        print_config_file_data();
        goto done;
    }

    if (do_remove_all_devs) {
        err = remove_all_config_chips();
        goto done;
    } else if (do_initialize_all_devs) {
        err = initialize_all_config_chips();
        goto done;
    }

    if (bus_name_path != NULL) {
        found = dev_i2c_lookup_i2c_bus(bus_name_path);
    }

    if (do_set_retry_count) {
        if (!found) {
            err = -ENODEV;
            goto done;
        }
        if (i2c_dev_verbose) {
            print_dev_bus(found);
        }
        err = set_adapter_retries(found, adapter_retry_count);
        goto done;
    }

    if (do_set_timeout) {
        if (!found) {
            err = -ENODEV;
            goto done;
        }
        if (i2c_dev_verbose) {
            print_dev_bus(found);
        }
        err = set_adapter_timeout(found, adapter_timeout_ms);
        goto done;
    }

    if (do_probe_address) {
        printf("Probing Address: 0x%02hx , Path: %s\n", address, bus_name_path);
        err = dev_i2c_smbus_probe(address, bus_name_path, 0);
        printf("Result: %s\n",
                ((err < 0) ? ((err == -ENXIO) ? "NO-ACK" : strerror(-err)) : "DEVICE ACK"));
        err = 0;
        goto done;
    }

    if (do_print_funcs) {
        if (!found) {
            err = -ENODEV;
            goto done;
        }
        if (i2c_dev_verbose) {
            print_dev_bus(found);
        }
        err = get_and_print_adapter_functionality(found);
        goto done;
    }

    if (do_print_all_devs) {
        printf("I2C Devices:\n");
        if (found != NULL) {
            dev_count = print_adapters_devices(found);
        } else {
            dev_count = print_all_adapters_dev_chips();
        }
        printf("Count: %d\n", dev_count);
        goto done;
    }

    if ((found != NULL) && (!do_bus_list_all)) {
        printf("I2C Adapters:\n");
        adapter_count = print_devbus(found, opt_tree);
        printf("Count: %d\n", adapter_count);
        goto done;
    }

    if (do_bus_list_all) {
        printf("I2C Adapters:\n");
        adapter_count = print_devbus_tree();
        printf("Count: %d\n", adapter_count);
        goto done;
    }

done:

    if ((do_bus_rescan) && (!(err < 0))) {
        for (i = 0; i < rescan_count; ++i) {
            err = i2cdev_rescan();
            if (err < 0) {
                goto done;
            }
        }
    }

    i2cdev_cleanup();

    if (err < 0) {
        fprintf(stderr, "lsi2c exited with error: %s", strerror(-err));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
