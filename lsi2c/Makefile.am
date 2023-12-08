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

# Sources for the lsi2c 
lsi2c_SOURCES = lsi2c.c

# Linker options for lsi2c
lsi2c_LDADD = $(top_builddir)/libi2cdev/libi2cdev.a

# Compiler options for lsi2c
lsi2c_CFLAGS = -I$(top_srcdir)/include -std=c99 -fPIC
