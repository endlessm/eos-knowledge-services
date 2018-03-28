# Copyright (C) 2016-2018 Endless Mobile, Inc.
#
# Note that this file includes Makefile.am.dbus-service.inc for the DBus Service
# and Makefile.am.multi-services.inc for the multi-service dispatcher shim.
#
# We define all automake variables first and then include the parts of the
# library that we are building.

bin_PROGRAMS =
servicedir = $(datadir)/dbus-1/services
service_DATA =
BUILT_SOURCES =
CLEANFILES =
MAINTAINERCLEANFILES = $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL)
EXTRA_DIST =

do_subst = $(SED) -e 's|%bindir%|$(bindir)|g'
subst_files =

include $(top_srcdir)/Makefile.am.dbus-service.inc
include $(top_srcdir)/Makefile.am.multi-services.inc

-include $(top_srcdir)/git.mk

