/* Copyright 2016 Endless Mobile, Inc. */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define EKS_TYPE_DISCOVERY_FEED_PROVIDER eks_discovery_feed_provider_get_type ()
G_DECLARE_FINAL_TYPE (EksDiscoveryFeedProvider, eks_discovery_feed_provider, EKS, DISCOVERY_FEED_PROVIDER, GObject)

G_END_DECLS
