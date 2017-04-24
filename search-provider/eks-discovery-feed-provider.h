/* Copyright 2016 Endless Mobile, Inc. */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define EKS_TYPE_DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER eks_discovery_feed_database_content_provider_get_type ()
G_DECLARE_FINAL_TYPE (EksDiscoveryFeedDatabaseContentProvider, eks_discovery_feed_database_content_provider, EKS, DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER, GObject)

G_END_DECLS
