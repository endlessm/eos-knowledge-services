/* Copyright 2016 Endless Mobile, Inc. */

#include "eks-search-app.h"

#include "eks-discovery-feed-provider-dbus.h"
#include "eks-discovery-feed-provider.h"
#include "eks-metadata-provider.h"
#include "eks-metadata-provider-dbus.h"
#include "eks-provider-iface.h"
#include "eks-search-provider.h"
#include "eks-search-provider-dbus.h"
#include "eks-subtree-dispatcher.h"

#include <string.h>

/**
 * EksSearchApp:
 *
 * Keeps track of all apps that use this version of EknServices and holds their
 * search provider and discovery feed DBus objects.
 */
struct _EksSearchApp
{
  GApplication parent_instance;

  EksSubtreeDispatcher *dispatcher;
  // Hash table with app id string keys, EksSearchProvider values
  GHashTable *app_search_providers;
  // Hash table with app id string keys, EksDiscoveryFeedProvider values
  GHashTable *discovery_feed_content_providers;
  // Hash table with app id string keys, EksMetadataProvider values
  GHashTable *metadata_providers;
};

G_DEFINE_TYPE (EksSearchApp,
               eks_search_app,
               G_TYPE_APPLICATION)

static void
eks_search_app_finalize (GObject *object)
{
  EksSearchApp *self = EKS_SEARCH_APP (object);

  g_clear_object (&self->dispatcher);
  g_clear_pointer (&self->app_search_providers, g_hash_table_unref);
  g_clear_pointer (&self->discovery_feed_content_providers, g_hash_table_unref);
  g_clear_pointer (&self->metadata_providers, g_hash_table_unref);

  G_OBJECT_CLASS (eks_search_app_parent_class)->finalize (object);
}

static gboolean
eks_search_app_register (GApplication    *application,
                         GDBusConnection *connection,
                         const gchar     *object_path,
                         GError          **error)
{
  EksSearchApp *self = EKS_SEARCH_APP (application);

  eks_subtree_dispatcher_register (self->dispatcher, connection, object_path, error);
  return TRUE;
}

static void
eks_search_app_unregister (GApplication    *application,
                           GDBusConnection *connection,
                           const gchar     *object_path)
{
  EksSearchApp *self = EKS_SEARCH_APP (application);

  eks_subtree_dispatcher_unregister (self->dispatcher);
}

static void
eks_search_app_class_init (EksSearchAppClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = eks_search_app_finalize;

  application_class->dbus_register = eks_search_app_register;
  application_class->dbus_unregister = eks_search_app_unregister;
}

// The following code is adapted from
// https://github.com/systemd/systemd/blob/master/src/basic/bus-label.c
static gint
unhexchar (gchar c)
{
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;

  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return -1;
}

static gchar *
bus_label_unescape (const gchar *f) {
  gchar *r, *t;
  gsize i;
  gsize l = f ? strlen (f) : 0;

  /* Special case for the empty string */
  if (l == 1 && *f == '_')
    return g_strdup ("");

  r = g_new (gchar, l + 1);
  if (!r)
    return NULL;

  for (i = 0, t = r; i < l; ++i)
    {
      if (f[i] == '_')
        {
          int a, b;

          if (l - i < 3 ||
              (a = unhexchar (f[i + 1])) < 0 ||
              (b = unhexchar (f[i + 2])) < 0)
            {
              /* Invalid escape code, let's take it literal then */
              *(t++) = '_';
            }
          else
            {
              *(t++) = (gchar) ((a << 4) | b);
              i += 2;
            }
        }
      else
        {
          *(t++) = f[i];
        }
    }

  *t = 0;

  return r;
}

typedef struct {
    GType create_type;
    GHashTable *cache;
} SubtreeObjectInfo;

static void
subtree_object_info_for_interface (EksSearchApp      *self,
                                   const gchar       *interface,
                                   SubtreeObjectInfo *info)
{
  if (g_strcmp0 (interface, "org.gnome.Shell.SearchProvider") == 0 ||
      g_strcmp0 (interface, "org.gnome.Shell.SearchProvider2") == 0)
    {
      info->create_type = EKS_TYPE_SEARCH_PROVIDER;
      info->cache = self->app_search_providers;
    }
  else if (g_strcmp0 (interface, "com.endlessm.DiscoveryFeedContent") == 0 ||
           g_strcmp0 (interface, "com.endlessm.DiscoveryFeedQuote") == 0 ||
           g_strcmp0 (interface, "com.endlessm.DiscoveryFeedWord") == 0 ||
           g_strcmp0 (interface, "com.endlessm.DiscoveryFeedNews") == 0 ||
           g_strcmp0 (interface, "com.endlessm.DiscoveryFeedVideo") == 0 ||
           g_strcmp0 (interface, "com.endlessm.DiscoveryFeedArtwork") == 0)
    {
      info->create_type = EKS_TYPE_DISCOVERY_FEED_PROVIDER;
      info->cache = self->discovery_feed_content_providers;
    }
  else if (g_strcmp0 (interface, "com.endlessm.ContentMetadata") == 0)
    {
      info->create_type = EKS_TYPE_METADATA_PROVIDER;
      info->cache = self->metadata_providers;
    }
  else
    g_assert_not_reached();
}

static GDBusInterfaceSkeleton *
dispatch_subtree (EksSubtreeDispatcher *dispatcher,
                  const gchar *subnode,
                  const gchar *interface,
                  EksSearchApp *self)
{
  SubtreeObjectInfo info;
  subtree_object_info_for_interface (self, interface, &info);

  EksProvider *provider = EKS_PROVIDER (g_hash_table_lookup (info.cache, subnode));
  if (provider == NULL)
    {
      g_autofree gchar *app_id = bus_label_unescape (subnode);
      provider = g_object_new (info.create_type,
                               "application-id", app_id,
                               NULL);
      g_hash_table_insert (info.cache, g_strdup (subnode), provider);
    }

  return eks_provider_skeleton_for_interface (provider, interface);
}

static GPtrArray *
eks_search_app_node_interface_infos ()
{
  GPtrArray *ptr_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_dbus_interface_info_unref);
  g_ptr_array_add (ptr_array, eks_search_provider2_interface_info ());
  g_ptr_array_add (ptr_array, eks_discovery_feed_content_interface_info ());
  g_ptr_array_add (ptr_array, eks_discovery_feed_quote_interface_info ());
  g_ptr_array_add (ptr_array, eks_discovery_feed_word_interface_info ());
  g_ptr_array_add (ptr_array, eks_discovery_feed_news_interface_info ());
  g_ptr_array_add (ptr_array, eks_discovery_feed_video_interface_info ());
  g_ptr_array_add (ptr_array, eks_discovery_feed_artwork_interface_info ());
  g_ptr_array_add (ptr_array, eks_content_metadata_interface_info ());
  return ptr_array;
}

static void
eks_search_app_init (EksSearchApp *self)
{
  self->dispatcher = g_object_new (EKS_TYPE_SUBTREE_DISPATCHER,
                                   "interface-infos", eks_search_app_node_interface_infos (),
                                   NULL);
  self->app_search_providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->discovery_feed_content_providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->metadata_providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  g_signal_connect (self->dispatcher, "dispatch-subtree",
                    G_CALLBACK (dispatch_subtree), self);
}
