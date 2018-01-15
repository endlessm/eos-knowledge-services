/* Copyright 2016 Endless Mobile, Inc. */

#include "eks-search-provider.h"

#include "eks-knowledge-app-dbus.h"
#include "eks-provider-iface.h"
#include "eks-search-provider-dbus.h"

#include <eos-knowledge-content.h>
#include <string.h>

#define RESULTS_LIMIT 5
#define MAX_DESCRIPTION_LENGTH 200

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
struct _EksSearchProvider
{
  GObject parent_instance;

  gchar *application_id;
  EksSearchProvider2 *skeleton;
  EksKnowledgeSearch *app_proxy;
  GCancellable *cancellable;
  // Hash table with ekn id string keys, EkncContentObjectModel values
  GHashTable *object_cache;
};

static void eks_search_provider_interface_init (EksProviderInterface *);

G_DEFINE_TYPE_WITH_CODE (EksSearchProvider,
                         eks_search_provider,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EKS_TYPE_PROVIDER,
                                                eks_search_provider_interface_init));

enum {
  PROP_0,
  PROP_APPLICATION_ID,
  NPROPS
};

static GParamSpec *eks_search_provider_props [NPROPS] = { NULL, };

static void
eks_search_provider_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EksSearchProvider *self = EKS_SEARCH_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_value_set_string (value, self->application_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eks_search_provider_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  EksSearchProvider *self = EKS_SEARCH_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_clear_pointer (&self->application_id, g_free);
      self->application_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eks_search_provider_finalize (GObject *object)
{
  EksSearchProvider *self = EKS_SEARCH_PROVIDER (object);

  g_clear_pointer (&self->application_id, g_free);
  g_clear_object (&self->skeleton);
  g_clear_object (&self->app_proxy);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->object_cache, g_hash_table_unref);

  G_OBJECT_CLASS (eks_search_provider_parent_class)->finalize (object);
}

static void
eks_search_provider_class_init (EksSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = eks_search_provider_get_property;
  object_class->set_property = eks_search_provider_set_property;
  object_class->finalize = eks_search_provider_finalize;

  eks_search_provider_props[PROP_APPLICATION_ID] =
    g_param_spec_string ("application-id", "Application Id", "Application Id",
      "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     NPROPS,
                                     eks_search_provider_props);
}

gchar *
object_path_from_app_id (const gchar *application_id)
{
  g_autoptr(GRegex) dot_regex = g_regex_new ("\\.", 0, 0, NULL);
  g_autofree gchar *replaced = g_regex_replace (dot_regex, application_id, -1, 0, "/", 0, NULL);
  return g_strconcat ("/", replaced, NULL);
}

static gboolean
ensure_app_proxy (EksSearchProvider *self)
{
  if (self->app_proxy != NULL)
    return TRUE;

  g_autofree gchar *object_path = object_path_from_app_id (self->application_id);
  GError *error = NULL;
  self->app_proxy = eks_knowledge_search_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                 G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                                                                   G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                 self->application_id,
                                                                 object_path,
                                                                 NULL,
                                                                 &error);
  if (error != NULL)
    {
      g_warning ("Error initializing dbus proxy: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }
  return TRUE;
}

typedef struct
{
  EksSearchProvider *self;
  GDBusMethodInvocation *invocation;
} SearchState;

static void
search_state_free (SearchState *state)
{
  g_object_unref (state->invocation);
  g_slice_free (SearchState, state);
}

static void
search_finished (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  SearchState *state = user_data;

  g_application_release (g_application_get_default ());

  GError *error = NULL;
  g_autoptr(EkncQueryResults) results = NULL;
  if (!(results = eknc_engine_query_finish (engine, result, &error)))
    {
      g_dbus_method_invocation_return_gerror (state->invocation, error);
      search_state_free (state);
      return;
    }

  GSList *models = eknc_query_results_get_models (results);

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
  for (GSList *l = models; l; l = l->next)
    {
      EkncContentObjectModel *model = l->data;
      g_autofree gchar *ekn_id = NULL;
      g_object_get (model, "ekn-id", &ekn_id, NULL);
      g_hash_table_insert (state->self->object_cache, g_strdup (ekn_id), g_object_ref (model));
      g_variant_builder_add (&builder, "s", ekn_id);
    }
  g_dbus_method_invocation_return_value (state->invocation, g_variant_new ("(as)", &builder));
  search_state_free (state);
}

static void
do_search (EksSearchProvider *self,
           GDBusMethodInvocation *invocation,
           gchar **terms)
{
  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_autofree char *search_terms = g_strjoinv (" ", terms);
  if (*search_terms == '\0')
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
      return;
    }

  g_application_hold (g_application_get_default ());

  const char *tags_match_any = { "EknArticleObject", NULL };

  self->cancellable = g_cancellable_new ();
  g_autoptr(EkncQueryObject) query_obj = g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                        "search-terms", search_terms,
                                                        "limit", RESULTS_LIMIT,
                                                        "app-id", self->application_id,
                                                        "tags-match-any", tags_match_any,
                                                        NULL);
  SearchState *state = g_slice_new0 (SearchState);
  state->self = self;
  state->invocation = g_object_ref (invocation);
  eknc_engine_query (eknc_engine_get_default (), query_obj, self->cancellable,
                     search_finished, state);
}

static gboolean
handle_get_initial_result_set (EksSearchProvider2 *skeleton,
                               GDBusMethodInvocation *invocation,
                               gchar **terms,
                               EksSearchProvider *self)
{
  do_search (self, invocation, terms);
  return TRUE;
}

static gboolean
handle_get_subsearch_result_set (EksSearchProvider2 *skeleton,
                                 GDBusMethodInvocation *invocation,
                                 gchar **previous_results,
                                 gchar **terms,
                                 EksSearchProvider *self)
{
  do_search (self, invocation, terms);
  return TRUE;
}


static gboolean
handle_get_result_metas (EksSearchProvider2 *skeleton,
                         GDBusMethodInvocation *invocation,
                         gchar **results,
                         EksSearchProvider *self)
{
  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));
  guint length = g_strv_length (results);
  for (guint i = 0; i < length; i++)
    {
      EkncContentObjectModel *model = g_hash_table_lookup (self->object_cache, results[i]);
      if (model == NULL)
        continue;

      GVariantBuilder meta_builder;
      g_variant_builder_init (&meta_builder, G_VARIANT_TYPE ("a{sv}"));
      g_autofree gchar *original_title = NULL;
      g_autofree gchar *title = NULL;
      g_autofree gchar *synopsis = NULL;
      g_object_get (model,
                    "original-title", &original_title,
                    "title", &title,
                    "synopsis", &synopsis,
                    NULL);
      gchar *visible_title = (original_title && *original_title) ? original_title : title;
      g_variant_builder_add (&meta_builder, "{sv}", "id", g_variant_new_string (results[i]));
      g_variant_builder_add (&meta_builder, "{sv}", "name", g_variant_new_string (visible_title));
      if (synopsis)
        {
          if (strlen (synopsis) > MAX_DESCRIPTION_LENGTH)
            {
              gchar *end = g_utf8_prev_char (synopsis + MAX_DESCRIPTION_LENGTH + 1);
              *end = '\0';
            }
          g_variant_builder_add (&meta_builder, "{sv}", "description", g_variant_new_string (synopsis));
        }
      g_variant_builder_add_value (&builder, g_variant_builder_end (&meta_builder));
    }

  g_dbus_method_invocation_return_value (invocation, g_variant_new ("(aa{sv})", &builder));
  return TRUE;
}

static gboolean
handle_activate_result (EksSearchProvider2 *skeleton,
                        GDBusMethodInvocation *invocation,
                        gchar *id,
                        gchar **terms,
                        guint32 timestamp,
                        EksSearchProvider *self)
{
  if (!ensure_app_proxy (self))
    return TRUE;

  g_autofree gchar *query = g_strjoinv (" ", terms);
  GError *error = NULL;
  eks_knowledge_search_call_load_item_sync (self->app_proxy,
                                            id,
                                            query,
                                            timestamp,
                                            NULL,
                                            &error);
  if (error != NULL)
    {
      g_warning ("Error activating result: %s", error->message);
      g_clear_error (&error);
    }
  return TRUE;
}

static gboolean
handle_launch_search (EksSearchProvider2 *skeleton,
                      GDBusMethodInvocation *invocation,
                      gchar **terms,
                      guint32 timestamp,
                      EksSearchProvider *self)
{
  if (!ensure_app_proxy (self))
    return TRUE;

  g_autofree gchar *query = g_strjoinv (" ", terms);
  GError *error = NULL;
  eks_knowledge_search_call_load_query_sync (self->app_proxy,
                                             query,
                                             timestamp,
                                             NULL,
                                             &error);
  if (error != NULL)
    {
      g_warning ("Error launching search: %s", error->message);
      g_clear_error (&error);
    }
  return TRUE;
}

static GDBusInterfaceSkeleton *
eks_search_provider_skeleton_for_interface (EksProvider *provider,
                                            const gchar *interface)
{
  EksSearchProvider *self = EKS_SEARCH_PROVIDER (provider);
  return G_DBUS_INTERFACE_SKELETON (self->skeleton);
}

static void
eks_search_provider_interface_init (EksProviderInterface *iface)
{
  iface->skeleton_for_interface = eks_search_provider_skeleton_for_interface;
}

static void
eks_search_provider_init (EksSearchProvider *self)
{
  self->skeleton = eks_search_provider2_skeleton_new ();
  g_signal_connect (self->skeleton, "handle-get-initial-result-set",
                    G_CALLBACK (handle_get_initial_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-subsearch-result-set",
                    G_CALLBACK (handle_get_subsearch_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-result-metas",
                    G_CALLBACK (handle_get_result_metas), self);
  g_signal_connect (self->skeleton, "handle-activate-result",
                    G_CALLBACK (handle_activate_result), self);
  g_signal_connect (self->skeleton, "handle-launch-search",
                    G_CALLBACK (handle_launch_search), self);

  self->object_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}
