#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
lib_LTLIBRARIES = libi2cdev.la

#######################################
# Build information for each library

# Sources for libi2cdev
libi2cdev_la_SOURCES = smbus.c access.c common.c data.c i2c-bus-parser.c i2c-dev-path.c i2c-error.c init.c smbus-dev.c sysfs.c 

# Linker options libi2cdev
libi2cdev_la_LDFLAGS = -fPIC -Wl,-rpath -Wl,LIBDIR

# Compiler options. Here we are adding the include directory
# to be searched for headers included in the source code.
libi2cdev_la_CFLAGS = -I$(top_srcdir)/include -std=c11
libi2cdev_la_CPPFLAGS = -I$(top_srcdir)/include -std=c++11
