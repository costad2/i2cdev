# i2cdev
I2C library and tools (lsi2c) for linux

I2C DEV TOOLS FOR LINUX
===================

This package contains an I2C dev library.


CONTENTS
--------

The various tools included in this package are grouped by category, each
category has its own sub-directory:

* include
  C/C++ header files for I2C and SMBus access over i2c-dev. Installed by
  default.

* lib
  The I2C library. Installed by
  default.
  
* lsi2c
  The i2c bus scanning utility

LICENSE
-------

Be aware that some files are released under GPL-2.0-or-later, everything else is
under LGPL-2.1-or-later.

Files with GPL-2.0-or-later:

* include/libi2cdev.h
* libi2cdev/smbus.c
* lsi2c/lsi2c.c


INSTALLATION
------------

./autogen.sh
./configure then simply run "make" to build the library and
tools, and "make install" to install them. You also can use "make uninstall"
to remove all the files you installed. By default, files are installed in
/usr/local but you can change the location by editing the Makefile file and
setting prefix to wherever you want. You may change the C compiler and the
compilation flags as well, and also decide whether to build the static
library or not.

Optionally, you can run "make strip" prior to "make install" if you want
smaller binaries. However, be aware that this will prevent any further
attempt to debug the library and tools.


DOCUMENTATION
-------------
See files in doc folder

QUESTIONS AND BUG REPORTS
-------------------------


