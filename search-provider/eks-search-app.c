/* Copyright 2016 Endless Mobile, Inc. */

#include "eks-search-app.h"

#include "eks-search-provider.h"
#include "eks-search-provider-dbus.h"

#include <eos-knowledge-content.h>
#include <systemd/sd-bus.h>

/**
 * EksSearchProvider:
 *
 * A search provider for a single knowledge app, to be used through dbus by the
 * shell's global search. Requires the app id of the knowledge app it should run
 * searches for.
 *
 * This search provider will activate the actual knowledge app over dbus with a
 * result or search to display.
 */
struct _EksSearchApp
{
  GApplication parent_instance;

  EkncSubtreeDispatcher *dispatcher;
  // Hash table with app id string keys, EksSearchProvider values
  GHashTable *app_search_providers;
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

  G_OBJECT_CLASS (eks_search_app_parent_class)->finalize (object);
}

static gboolean
eks_search_app_register (GApplication *application,
                         GDBusConnection *connection,
                         const gchar *object_path,
                         GError **error)
{
  EksSearchApp *self = EKS_SEARCH_APP (application);

  eknc_subtree_dispatcher_register (self->dispatcher, connection, object_path);
  return TRUE;
}

static void
eks_search_app_unregister (GApplication *application,
                           GDBusConnection *connection,
                           const gchar *object_path)
{
  EksSearchApp *self = EKS_SEARCH_APP (application);

  eknc_subtree_dispatcher_unregister (self->dispatcher);
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

static GDBusInterfaceSkeleton *
dispatch_subtree (EkncSubtreeDispatcher *dispatcher,
                  const gchar *subnode,
                  EksSearchApp *self)
{
  EksSearchProvider *provider = g_hash_table_lookup (self->app_search_providers, subnode);
  if (provider == NULL)
    {
      g_autofree gchar *app_id = NULL;
      g_autofree gchar *prefixed = g_strconcat ("/", subnode, NULL);
      sd_bus_path_decode (prefixed, "/", &app_id);
      provider = g_object_new (EKS_TYPE_SEARCH_PROVIDER,
                               "application-id", app_id,
                               NULL);
      g_hash_table_insert (self->app_search_providers, g_strdup (subnode), provider);
    }

  GDBusInterfaceSkeleton *skeleton;
  g_object_get (provider, "skeleton", &skeleton, NULL);
  return skeleton;
}

static void
eks_search_app_init (EksSearchApp *self)
{
  self->dispatcher = g_object_new (EKNC_TYPE_SUBTREE_DISPATCHER,
                                   "interface-info", eks_search_provider2_interface_info (),
                                   NULL);
  self->app_search_providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  g_signal_connect (self->dispatcher, "dispatch-subtree",
                    G_CALLBACK (dispatch_subtree), self);
}
