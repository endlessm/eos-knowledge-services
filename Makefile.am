## Process this file with automake to produce Makefile.in

# Copyright 2016 Endless Mobile, Inc.

## -----------
## Makefile.am
## -----------
## Please keep this file well-commented.

bin_PROGRAMS =
CLEANFILES =
MAINTAINERCLEANFILES = $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL)
EXTRA_DIST =

# # # SUBSTITUTED FILES # # #
# These files need to be filled in with make variables
do_subst = $(SED) -e 's|%bindir%|$(bindir)|g'
subst_files = \
	search-provider/com.endlessm.EknServices2.SearchProviderV2.service \
	$(NULL)

$(subst_files): %: %.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $(@D) && \
	$(do_subst) $< > $@

CLEANFILES += $(subst_files)
EXTRA_DIST += $(patsubst %,%.in,$(subst_files))

# # # DBUS SERVICES # # #
servicedir = $(datadir)/dbus-1/services
service_DATA = \
	search-provider/com.endlessm.EknServices2.SearchProviderV2.service \
	$(NULL)

search-provider/eks-discovery-feed-provider-dbus.h search-provider/eks-discovery-feed-provider-dbus.c: search-provider/eks-discovery-feed-provider-dbus.xml Makefile.am
	$(AM_V_GEN) $(MKDIR_P) $(@D) && \
	$(GDBUS_CODEGEN) \
	--interface-prefix=com.endlessm. \
	--c-namespace Eks \
	--generate-c-code search-provider/eks-discovery-feed-provider-dbus \
	$<

search-provider/eks-knowledge-app-dbus.h search-provider/eks-knowledge-app-dbus.c: search-provider/eks-knowledge-app-dbus.xml Makefile.am
	$(AM_V_GEN) $(MKDIR_P) $(@D) && \
	$(GDBUS_CODEGEN) \
	--interface-prefix=com.endlessm. \
	--c-namespace Eks \
	--generate-c-code search-provider/eks-knowledge-app-dbus \
	$<

search-provider/eks-metadata-provider-dbus.h search-provider/eks-metadata-provider-dbus.c: search-provider/eks-metadata-provider-dbus.xml Makefile.am
	$(AM_V_GEN) $(MKDIR_P) $(@D) && \
	$(GDBUS_CODEGEN) \
	--interface-prefix=com.endlessm. \
	--c-namespace Eks \
	--generate-c-code search-provider/eks-metadata-provider-dbus \
	$<

search-provider/eks-search-provider-dbus.h search-provider/eks-search-provider-dbus.c: search-provider/eks-search-provider-dbus.xml Makefile.am
	$(AM_V_GEN) $(MKDIR_P) $(@D) && \
	$(GDBUS_CODEGEN) \
	--interface-prefix=org.gnome.Shell \
	--c-namespace Eks \
	--generate-c-code search-provider/eks-search-provider-dbus \
	$<

EXTRA_DIST += \
	search-provider/eks-discovery-feed-provider-dbus.xml \
	search-provider/eks-knowledge-app-dbus.xml \
	search-provider/eks-metadata-provider-dbus.xml \
	search-provider/eks-search-provider-dbus.xml \
	$(NULL)

BUILT_SOURCES = \
	search-provider/eks-discovery-feed-provider-dbus.h \
	search-provider/eks-discovery-feed-provider-dbus.c \
	search-provider/eks-knowledge-app-dbus.h \
	search-provider/eks-knowledge-app-dbus.c \
	search-provider/eks-metadata-provider-dbus.h \
	search-provider/eks-metadata-provider-dbus.c \
	search-provider/eks-search-provider-dbus.h \
	search-provider/eks-search-provider-dbus.c \
	$(NULL)

eks_search_provider_v2_SOURCES = \
	search-provider/eks-discovery-feed-provider.c \
	search-provider/eks-discovery-feed-provider.h \
	search-provider/eks-discovery-feed-provider-dbus.c \
	search-provider/eks-discovery-feed-provider-dbus.h \
	search-provider/eks-errors.c \
	search-provider/eks-errors.h \
	search-provider/eks-knowledge-app-dbus.c \
	search-provider/eks-knowledge-app-dbus.h \
	search-provider/eks-metadata-provider.c \
	search-provider/eks-metadata-provider.h \
	search-provider/eks-metadata-provider-dbus.c \
	search-provider/eks-metadata-provider-dbus.h \
	search-provider/eks-provider-iface.h \
	search-provider/eks-provider-iface.c \
	search-provider/eks-query-util.c \
	search-provider/eks-query-util.h \
	search-provider/eks-search-app.c \
	search-provider/eks-search-app.h \
	search-provider/eks-search-main.c \
	search-provider/eks-search-provider-dbus.c \
	search-provider/eks-search-provider-dbus.h \
	search-provider/eks-search-provider.c \
	search-provider/eks-search-provider.h \
	search-provider/eks-subtree-dispatcher.c \
	search-provider/eks-subtree-dispatcher.h \
	$(NULL)
eks_search_provider_v2_CFLAGS = \
	@SEARCH_PROVIDER_CFLAGS@ \
	-I $(builddir)/search-provider \
	$(AM_CFLAGS) \
	$(NULL)
eks_search_provider_v2_LDADD = \
	@SEARCH_PROVIDER_LIBS@ \
	$(NULL)

# # # BINARIES # # #
bin_PROGRAMS += \
	eks-search-provider-v2 \
	$(NULL)

-include $(top_srcdir)/git.mk
