# Copyright 2016-2018 Endless Mobile, Inc.

if EOS_KNOWLEDGE_SERVICES_MULTI_SERVICES_ENABLED

bin_PROGRAMS += eks-multi-services-dispatcher

# # # SUBSTITUTED FILES # # #
# These files need to be filled in with make variables
subst_files += \
	search-provider/com.endlessm.EknServices.SearchProviderV1.service \
	search-provider/com.endlessm.EknServices2.SearchProviderV2.service \
	$(NULL)

$(subst_files): %: %.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $(@D) && \
	$(do_subst) $< > $@

CLEANFILES += $(subst_files)
EXTRA_DIST += $(patsubst %,%.in,$(subst_files))

eks_multi_services_dispatcher_SOURCES = \
	multi-services/eks-multi-services-dispatcher.c \
	$(NULL)
eks_multi_services_dispatcher_CFLAGS = \
	@MULTI_SERVICES_CFLAGS@ \
	-I $(builddir)/multi-services \
	$(AM_CFLAGS) \
	$(NULL)
eks_multi_services_dispatcher_LDADD = \
	@MULTI_SERVICES_LIBS@ \
	$(NULL)

service_DATA += \
	search-provider/com.endlessm.EknServices.SearchProviderV1.service \
	search-provider/com.endlessm.EknServices2.SearchProviderV2.service \
	$(NULL)

endif
