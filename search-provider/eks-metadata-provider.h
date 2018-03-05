/* Copyright 2018 Endless Mobile, Inc. */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define EKS_TYPE_METADATA_PROVIDER eks_metadata_provider_get_type ()
G_DECLARE_FINAL_TYPE (EksMetadataProvider, eks_metadata_provider, EKS, METADATA_PROVIDER, GObject)

G_END_DECLS
