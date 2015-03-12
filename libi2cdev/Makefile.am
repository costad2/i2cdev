#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib

#lib_LTLIBRARIES = libi2cdev.la
lib_LIBRARIES = libi2cdev.a

#######################################
# Build information for each library

# Sources for libi2cdev
libi2cdev_a_SOURCES = \
	init.c smbus.c access.c common.c data.c sysfs.c \
	i2c-bus-parser.c i2c-dev-path.c i2c-error.c smbus-dev.c

# Compiler options. Here we are adding the include directory
# to be searched for headers included in the source code.
libi2cdev_a_CFLAGS = \
	-I$(top_srcdir)/include -std=c99 -fPIC -O2 -Wall
