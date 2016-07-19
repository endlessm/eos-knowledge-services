## Process this file with automake to produce Makefile.in

# Copyright 2016 Endless Mobile, Inc.

## -----------
## Makefile.am
## -----------
## Please keep this file well-commented.

bin_SCRIPTS =
CLEANFILES =
EXTRA_DIST =

# # # GRESOURCES # # #
search_provider_v1_gresource = search-provider/eks-search-provider-v1.gresource
search_provider_v1_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/search-provider --generate-dependencies $(srcdir)/$(search_provider_v1_gresource).xml)
search-provider/eks-search-provider-v1.gresource: $(search_provider_v1_gresource).xml $(search_provider_v1_files)
	$(AM_V_GEN) $(MKDIR_P) search-provider && \
	$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir)/search-provider $<

pkgdata_DATA = \
	search-provider/eks-search-provider-v1.gresource \
	$(NULL)

CLEANFILES += search-provider/eks-search-provider-v1.gresource

EXTRA_DIST += \
	$(search_provider_v1_gresource).xml \
	$(search_provider_v1_files) \
	$(NULL)

# # # SUBSTITUTED FILES # # #
# These files need to be filled in with make variables
do_subst = $(SED) \
	-e 's|%bindir%|$(bindir)|g' \
	-e 's|%pkgdatadir%|$(pkgdatadir)|g' \
	-e 's|%eklpkgdatadir%|$(EKL_PKGDATADIR)|g' \
	-e 's|%eklpkglibdir%|$(EKL_PKGLIBDIR)|g' \
	-e 's|%ekltypelibdir%|$(EKL_TYPELIBDIR)|g'

subst_files = \
	search-provider/eks-search-provider-v1 \
	search-provider/com.endlessm.EknServices.SearchProviderV1.service \
	$(NULL)

$(subst_files): %: %.in Makefile
	$(AM_V_GEN)$(MKDIR_P) $(@D) && \
	$(do_subst) $< > $@

CLEANFILES += $(subst_files)
EXTRA_DIST += $(patsubst %,%.in,$(subst_files))

# # # DBUS SERVICES # # #
servicedir = $(datadir)/dbus-1/services
service_DATA = \
	search-provider/com.endlessm.EknServices.SearchProviderV1.service \
	$(NULL)

# # # BINARIES # # #
bin_SCRIPTS += \
	search-provider/eks-search-provider-v1 \
	$(NULL)
