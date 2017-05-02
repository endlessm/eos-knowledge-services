/* Copyright 2016 Endless Mobile, Inc. */

#include "eks-discovery-feed-provider.h"

#include "eks-knowledge-app-dbus.h"
#include "eks-discovery-feed-provider-dbus.h"

#include <eos-knowledge-content.h>
#include <eos-shard/eos-shard-shard-file.h>

#include <string.h>

struct _EksDiscoveryFeedDatabaseContentProvider
{
  GObject parent_instance;

  gchar *application_id;
  EksDiscoveryFeedContent *skeleton;
  EksDiscoveryFeedContent *app_proxy;
  GCancellable *cancellable;
};

G_DEFINE_TYPE (EksDiscoveryFeedDatabaseContentProvider,
               eks_discovery_feed_database_content_provider,
               G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_APPLICATION_ID,
  PROP_SKELETON,
  NPROPS
};

static GParamSpec *eks_discovery_feed_database_content_provider_props [NPROPS] = { NULL, };

static void
eks_discovery_feed_database_content_provider_get_property (GObject    *object,
                                                           guint       prop_id,
                                                           GValue     *value,
                                                           GParamSpec *pspec)
{
  EksDiscoveryFeedDatabaseContentProvider *self = EKS_DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_APPLICATION_ID:
      g_value_set_string (value, self->application_id);
      break;

    case PROP_SKELETON:
      g_value_set_object (value, self->skeleton);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
eks_discovery_feed_database_content_provider_set_property (GObject      *object,
                                                           guint         prop_id,
                                                           const GValue *value,
                                                           GParamSpec   *pspec)
{
  EksDiscoveryFeedDatabaseContentProvider *self = EKS_DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER (object);

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
eks_discovery_feed_database_content_provider_finalize (GObject *object)
{
  EksDiscoveryFeedDatabaseContentProvider *self = EKS_DISCOVERY_FEED_DATABASE_CONTENT_PROVIDER (object);

  g_clear_pointer (&self->application_id, g_free);
  g_clear_object (&self->skeleton);
  g_clear_object (&self->app_proxy);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (eks_discovery_feed_database_content_provider_parent_class)->finalize (object);
}

static void
eks_discovery_feed_database_content_provider_class_init (EksDiscoveryFeedDatabaseContentProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = eks_discovery_feed_database_content_provider_get_property;
  object_class->set_property = eks_discovery_feed_database_content_provider_set_property;
  object_class->finalize = eks_discovery_feed_database_content_provider_finalize;

  eks_discovery_feed_database_content_provider_props[PROP_APPLICATION_ID] =
    g_param_spec_string ("application-id", "Application Id", "Application Id",
      "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  eks_discovery_feed_database_content_provider_props[PROP_SKELETON] =
    g_param_spec_object ("skeleton", "Skeleton", "Skeleton",
      G_TYPE_DBUS_INTERFACE_SKELETON, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     NPROPS,
                                     eks_discovery_feed_database_content_provider_props);
}

static  gchar *
object_path_from_app_id (const gchar *application_id)
{
  g_autoptr(GRegex) dot_regex = g_regex_new ("\\.", 0, 0, NULL);
  g_autofree gchar *replaced = g_regex_replace (dot_regex, application_id, -1, 0, "/", 0, NULL);
  return g_strconcat ("/", replaced, NULL);
}

static gboolean
ensure_app_proxy (EksDiscoveryFeedDatabaseContentProvider *self)
{
  if (self->app_proxy != NULL)
    return TRUE;

  g_autofree gchar *object_path = object_path_from_app_id (self->application_id);
  GError *error = NULL;
  self->app_proxy = eks_discovery_feed_content_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION |
                                                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                                                       self->application_id,
                                                                       object_path,
                                                                       NULL,
                                                                       &error);
  if (error != NULL)
    {
      g_warning ("Error initializing dbus proxy: %s\n", error->message);
      g_clear_error (&error);
      return FALSE;
    }
  return TRUE;
}

typedef struct {
  GDBusMethodInvocation *invocation;
  EksDiscoveryFeedDatabaseContentProvider  *provider;
} DiscoveryFeedQueryState;

static DiscoveryFeedQueryState *
discovery_feed_query_state_new (GDBusMethodInvocation                  *invocation,
                                EksDiscoveryFeedDatabaseContentProvider *provider)
{
  DiscoveryFeedQueryState *state = g_slice_new0 (DiscoveryFeedQueryState);
  state->invocation = g_object_ref (invocation);
  state->provider = provider;

  return state;
}

static void
discovery_feed_query_state_free (DiscoveryFeedQueryState *state)
{
  g_object_unref (state->invocation);
  g_slice_free (DiscoveryFeedQueryState, state);
}

static inline gchar *
underscorify (const gchar *string)
{
  gchar *underscored = strdup (string);
  gchar *c = underscored;
  do
    {
      if (*c == '-')
        *c = '_';
    }
  while (*c++);

  return underscored;
}

static void
add_key_value_pair_to_variant (GVariantBuilder *builder,
                               const char      *key,
                               const char      *value)
{
  g_variant_builder_open (builder, G_VARIANT_TYPE ("{ss}"));
  g_variant_builder_add (builder, "s", key);
  g_variant_builder_add (builder, "s", value);
  g_variant_builder_close (builder);
}

static void
add_key_value_pair_from_model_to_variant (EkncContentObjectModel *model,
                                          GVariantBuilder        *builder,
                                          const char             *key)
{
  g_autofree gchar *value = NULL;
  g_autofree gchar *underscore_key = underscorify (key);
  g_object_get (model, key, &value, NULL);
  add_key_value_pair_to_variant (builder, underscore_key, value);
}

static gchar *
select_random_string_from_variant (GVariant *variant)
{
  gsize size = g_variant_n_children (variant);
  gint index = g_random_int_range (0, size);
  /* We need to unwrap the variant and then the inner string first */
  g_autoptr(GVariant) child_variant = g_variant_get_child_value (variant, index);
  g_autoptr(GVariant) child_value = g_variant_get_variant (child_variant);

  return g_variant_dup_string (child_value, NULL);
}

static const unsigned int DISCOVERY_FEED_SET_CUSTOM_TITLE = 1 << 0;

static void
article_card_descriptions_cb (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
  EkncEngine *engine = EKNC_ENGINE (source);
  DiscoveryFeedQueryState *state = user_data;

  g_application_release (g_application_get_default ());

  GError *error = NULL;
  EkncDomain *domain = eknc_engine_get_domain_for_app (engine, state->provider->application_id, &error);
  if (domain == NULL)
    {
      g_dbus_method_invocation_take_error (state->invocation, error);
      discovery_feed_query_state_free (state);
      return;
    }

  GSList *shards = eknc_domain_get_shards (domain);

  GVariantBuilder shard_builder;
  g_variant_builder_init (&shard_builder, G_VARIANT_TYPE_STRING_ARRAY);
  for (GSList *l = shards; l; l = l->next)
    {
      g_autofree gchar *shard_path = NULL;
      EosShardShardFile *shard = l->data;

      g_object_get (shard, "path", &shard_path, NULL);
      g_variant_builder_add (&shard_builder, "s", shard_path);
    }

  g_autoptr(EkncQueryResults) results = NULL;
  if (!(results = eknc_engine_query_finish (engine, result, &error)))
    {
      g_dbus_method_invocation_return_gerror (state->invocation, error);
      discovery_feed_query_state_free (state);
      return;
    }

  GSList *models = eknc_query_results_get_models (results);

  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{ss}"));
  for (GSList *l = models; l; l = l->next)
    {
      /* Start building up object */
      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));

      EkncContentObjectModel *model = l->data;
      guint flags = 0;

      /* Examine the discovery-feed-content object first and set flags
       * for things that we've overridden */
      g_autoptr(GVariant) discoveryFeedContentVariant;
      g_object_get (model,
                    "discovery-feed-content",
                    &discoveryFeedContentVariant,
                    NULL);
      GVariantIter discoveryFeedContentIter;
      g_variant_iter_init (&discoveryFeedContentIter,
                           discoveryFeedContentVariant);

      gchar *key;
      GVariant *value;

      while (g_variant_iter_loop (&discoveryFeedContentIter, "{sv}", &key, &value))
        {
          if (g_strcmp0(key, "blurbs") == 0)
            {
              g_autofree gchar *title = select_random_string_from_variant (value);

              if (title)
                {
                  add_key_value_pair_to_variant(&builder, "title", title);
                  add_key_value_pair_to_variant(&builder, "synopsis", "");
                  flags |= DISCOVERY_FEED_SET_CUSTOM_TITLE;
                }
            }
        }

      /* Add key-value pairs based on things we haven't addded yet */
      if (!(flags & DISCOVERY_FEED_SET_CUSTOM_TITLE))
        {
          add_key_value_pair_from_model_to_variant (model, &builder, "title");
          add_key_value_pair_from_model_to_variant (model, &builder, "synopsis");
        }

      add_key_value_pair_from_model_to_variant (model, &builder, "last-modified-date");
      add_key_value_pair_from_model_to_variant (model, &builder, "thumbnail-uri");
      add_key_value_pair_from_model_to_variant (model, &builder, "ekn-id");

      /* Stop building object */
      g_variant_builder_close (&builder);
    }
  g_dbus_method_invocation_return_value (state->invocation,
                                         g_variant_new ("(asaa{ss})", &shard_builder, &builder));
  discovery_feed_query_state_free (state);
}

static gboolean
handle_article_card_descriptions (EksDiscoveryFeedDatabaseContentProvider *skeleton,
                                  GDBusMethodInvocation                  *invocation,
                                  gpointer                                user_data)
{
    EksDiscoveryFeedDatabaseContentProvider *self = (EksDiscoveryFeedDatabaseContentProvider *) user_data;

    if (!ensure_app_proxy (self))
      return TRUE;

    EkncEngine *engine = eknc_engine_get_default ();

    /* Build up tags_match_any */
    GVariantBuilder tags_match_any_builder;
    g_variant_builder_init (&tags_match_any_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_any_builder, "s", "EknArticleObject");
    GVariant *tags_match_any = g_variant_builder_end (&tags_match_any_builder);

    GVariantBuilder tags_match_all_builder;
    g_variant_builder_init (&tags_match_all_builder, G_VARIANT_TYPE ("as"));
    g_variant_builder_add (&tags_match_all_builder, "s", "EknHasDiscoveryFeedTitle");
    GVariant *tags_match_all = g_variant_builder_end (&tags_match_all_builder);

    /* Create query and run it */
    g_autoptr(EkncQueryObject) query = g_object_new (EKNC_TYPE_QUERY_OBJECT,
                                                     "tags-match-any", tags_match_any,
                                                     "tags-match-all", tags_match_all,
                                                     "limit", 5,
                                                     "app-id", self->application_id,
                                                     NULL);

    /* Hold the application so that it doesn't go away whilst we're handling
     * the query */
    g_application_hold (g_application_get_default ());

    eknc_engine_query (engine,
                       query,
                       self->cancellable,
                       article_card_descriptions_cb,
                       discovery_feed_query_state_new (invocation, self));

    return TRUE;
}
                       
static void
eks_discovery_feed_database_content_provider_init (EksDiscoveryFeedDatabaseContentProvider *self)
{
  self->skeleton = eks_discovery_feed_content_skeleton_new ();
  g_signal_connect (self->skeleton, "handle-article-card-descriptions",
                    G_CALLBACK (handle_article_card_descriptions), self);
}
