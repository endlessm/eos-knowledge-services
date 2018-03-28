# Copyright 2016-2018 Endless Mobile, Inc.

if EOS_KNOWLEDGE_SERVICES_DBUS_SERVICE_ENABLED
## -----------
## Makefile.am
## -----------
## Please keep this file well-commented.

# # # DBUS SERVICES # # #
search-provider/eks-search-provider-dbus.h search-provider/eks-search-provider-dbus.c: search-provider/eks-search-provider-dbus.xml Makefile.am
	$(AM_V_GEN) $(MKDIR_P) $(@D) && \
	$(GDBUS_CODEGEN) \
	--interface-prefix=org.gnome.Shell \
	--c-namespace Eks \
	--generate-c-code search-provider/eks-search-provider-dbus \
	$<

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

EXTRA_DIST += \
	search-provider/eks-search-provider-dbus.xml \
	search-provider/eks-discovery-feed-provider-dbus.xml \
	search-provider/eks-knowledge-app-dbus.xml \
	$(NULL)

BUILT_SOURCES += \
	search-provider/eks-search-provider-dbus.h \
	search-provider/eks-search-provider-dbus.c \
	search-provider/eks-discovery-feed-provider-dbus.h \
	search-provider/eks-discovery-feed-provider-dbus.c \
	search-provider/eks-knowledge-app-dbus.h \
	search-provider/eks-knowledge-app-dbus.c \
	$(NULL)

eks_search_provider_v2_SOURCES = \
	search-provider/eks-discovery-feed-provider.c \
	search-provider/eks-discovery-feed-provider.h \
	search-provider/eks-discovery-feed-provider-dbus.c \
	search-provider/eks-discovery-feed-provider-dbus.h \
	search-provider/eks-knowledge-app-dbus.c \
	search-provider/eks-knowledge-app-dbus.h \
	search-provider/eks-provider-iface.h \
	search-provider/eks-provider-iface.c \
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

endif
