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
AC_INIT([Endless OS Knowledge Services], [1],
    [], [eos-knowledge-services], [http://endlessm.com])
# Initialize Automake: enable all warnings and do not insist on GNU standards
# no-portability suppresses warnings about syntax specific to GNU make
AM_INIT_AUTOMAKE([-Wall -Wno-portability foreign 1.11 tar-ustar])
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
# Library configuration tool
PKG_PROG_PKG_CONFIG
# Needed to compile GResources
AC_SUBST([GLIB_COMPILE_RESOURCES], [`$PKG_CONFIG --variable glib_compile_resources gio-2.0`])

# We need eos-knowledge-lib to build
PKG_CHECK_MODULES([KNOWLEDGE_LIB], [eos-knowledge-0])

# eos-knowledge-lib paths
AC_SUBST([EKL_PKGDATADIR], [`$PKG_CONFIG --variable pkgdatadir eos-knowledge-0`])
if test "$EKL_PKGDATADIR" == ""; then
   AC_MSG_ERROR([cannot locate eos-knowledge-0 pkgdatadir])
fi
AC_SUBST([EKL_PKGLIBDIR], [`$PKG_CONFIG --variable pkglibdir eos-knowledge-0`])
if test "$EKL_PKGLIBDIR" == ""; then
   AC_MSG_ERROR([cannot locate eos-knowledge-0 pkglibdir])
fi
AC_SUBST([EKL_TYPELIBDIR], [`$PKG_CONFIG --variable typelibdir eos-knowledge-0`])
if test "$EKL_TYPELIBDIR" == ""; then
   AC_MSG_ERROR([cannot locate eos-knowledge-0 typelibdir])
fi

AX_PROG_GJS
AX_CHECK_GIRS_GJS([Gio], [2.0])

AC_CACHE_SAVE

# Output
# ------
# List files here that the configure script should output

AC_CONFIG_FILES([
    Makefile
])

# Do the output
AC_OUTPUT
