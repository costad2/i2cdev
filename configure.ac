dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([libi2cdev],[1.0])
AC_PROG_CC
AC_CONFIG_MACRO_DIRS([m4])
AC_CANONICAL_TARGET

dnl Initialize automake
AM_INIT_AUTOMAKE

dnl this allows us specify individual liking flags for each target
AM_PROG_CC_C_O

dnl Initialize Libtool
LT_INIT

AC_CONFIG_FILES(Makefile
                libi2cdev/Makefile
                lsi2c/Makefile
                include/Makefile)
AC_OUTPUT
