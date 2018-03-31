# Process this file with autoconf to produce configure

# Copyright 2014-2016 Endless Mobile, Inc.

# ------------
# configure.ac
# ------------
# Please keep this file well-commented. Autotools involve a lot of magical
# incantations, and it is easy to mess things up if you don't know what you
# are doing.

# Initialization
# --------------
# Initialize Autoconf: package name, version, bug report address, tarball name,
# website
AC_INIT([Endless OS Knowledge Services], [2],
    [], [eos-knowledge-services], [http://endlessm.com])
# Initialize Automake: enable all warnings and do not insist on GNU standards
# no-portability suppresses warnings about syntax specific to GNU make
AM_INIT_AUTOMAKE([-Wall -Wno-portability foreign 1.11 tar-ustar subdir-objects])
# Avoid spewing garbage over the terminal ('make V=1' to see the garbage)
AM_SILENT_RULES([yes])
# Keep Autotools macros local to this source tree
AC_CONFIG_MACRO_DIR([m4])

AC_CACHE_SAVE

# Required build tools
# --------------------
# Make sure we can create directory hierarchies
AC_PROG_MKDIR_P
# Needed for paths substitutions
AC_PROG_SED
# C compiler
AC_PROG_CC
AC_PROG_CC_C99
# Library configuration tool
PKG_PROG_PKG_CONFIG
# Needed for implementing dbus interfaces in C
AC_SUBST([GDBUS_CODEGEN], [`$PKG_CONFIG --variable gdbus_codegen gio-2.0`])

PKG_CHECK_MODULES([SEARCH_PROVIDER], [
    dmodel-0
    gio-2.0
    glib-2.0
    gobject-2.0
])

AC_CACHE_SAVE

# Output
# ------
# List files here that the configure script should output

AC_CONFIG_FILES([
    Makefile
])

# Do the output
AC_OUTPUT
