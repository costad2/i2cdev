#######################################
# The list of executables we are building seperated by spaces
# the 'bin_' indicates that these build products will be installed
# in the $(bindir) directory. For example /usr/bin
bin_PROGRAMS=lsi2c

#######################################
# Build information for each executable. The variable name is derived
# by use the name of the executable with each non alpha-numeric character is
# replaced by '_'. So a.out becomes a_out and the appropriate suffex added.
# '_SOURCES' for example.

# Sources for the a.out 
lsi2c_SOURCES= lsi2c.c

# Linker options for a.out
lsi2c_LDFLAGS = -fPIC $(top_srcdir)/libi2cdev/libi2cdev.la

# Compiler options for a.out
lsi2c_CFLAGS = -I$(top_srcdir)/include -std=c11
lsi2c_CPPFLAGS = -I$(top_srcdir)/include -std=c++11
