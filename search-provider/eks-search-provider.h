/* Copyright 2016 Endless Mobile, Inc. */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define EKS_TYPE_SEARCH_PROVIDER eks_search_provider_get_type ()
G_DECLARE_FINAL_TYPE (EksSearchProvider, eks_search_provider, EKS, SEARCH_PROVIDER, GObject)

G_END_DECLS
